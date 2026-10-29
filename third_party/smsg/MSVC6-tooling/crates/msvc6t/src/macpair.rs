use crate::Outcome;
use crate::project::Config;
use anyhow::{Context as _, Result};
use clap::Args;
use msvc6_image::pe::Image;
use msvc6_match::annotations;
use msvc6_match::input;
use msvc6_match::mac::names::{self, Context, MapRow};
use msvc6_match::mac::pair::Rules;
use msvc6_match::mac::side::{self, OurInput, Source};
use msvc6_match::mac::{self, Decisions, Inputs};
use msvc6_match::sibling::parse_oracles;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

#[derive(Debug, Args)]
pub struct PairArgs {
    #[arg(help = "Our image: an [[image]] id")]
    pub image: String,
    #[arg(
        long,
        help = "The Mac sources: `label<TAB>family<TAB>rank<TAB>kind<TAB>path[<TAB>twin]` rows (kind pef or \
                macho; a lower rank wins a conflict; twin a label of --twins, the PC build the source was \
                ported from; `#` lines skipped, `~/` is $HOME)"
    )]
    pub sources: PathBuf,
    #[arg(
        long,
        help = "The PC builds the sources' twins name, in `sibling pair --oracles` form"
    )]
    pub twins: Option<PathBuf>,
    #[arg(
        long,
        help = "The annotation table: reccmp markers kept out of the source, `image<TAB>va<TAB>kind<TAB>path<TAB>nth<TAB>anchor<TAB>name` \
                (an anchor is the definition's first code line or lines, whitespace runs as one space, `\\n` between \
                lines; nth its occurrence when it is not unique; a LIBRARY row names a library function)"
    )]
    pub annotations: PathBuf,
    #[arg(
        long,
        value_name = "DIR",
        help = "The tree the annotation paths are relative to"
    )]
    pub root: PathBuf,
    #[arg(
        long,
        value_name = "NAME",
        help = "The annotation table's image column for IMAGE (default: the reference's file name)"
    )]
    pub annotation_image: Option<String>,
    #[arg(
        long,
        value_name = "PREFIX",
        help = "Where two annotation rows name one address, the row whose path starts with this wins (repeatable)"
    )]
    pub prefer: Vec<String>,
    #[arg(
        long,
        required = true,
        help = "Our functions in scope: a source path holding one of these (repeatable)"
    )]
    pub scope: Vec<String>,
    #[arg(
        long,
        help = "The classes whose every Mac name gets a row (one class a line, `#` lines skipped)"
    )]
    pub classes: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FAMILY",
        help = "Only the sources of this family give those class rows (repeatable; default every family)"
    )]
    pub class_family: Vec<String>,
    #[arg(
        long,
        help = "Names our own binaries attest: `member<TAB>Class<TAB>name` or `function<TAB>name` rows; such a \
                name is kept and the Mac name recorded"
    )]
    pub attested: Option<PathBuf>,
    #[arg(
        long,
        help = "A map an earlier run wrote: a rename it applied reads as `evidence`, not `already`"
    )]
    pub prior: Option<PathBuf>,
    #[arg(
        long,
        help = "Measured refusals: `kind<TAB>key<TAB>reason<TAB>detail` rows (a method or function row's address, a \
                class row's old name); an APPLIED row they name becomes RECORDED with the reason"
    )]
    pub measured: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "A directory of system headers (the toolchain's include directory): a header renamed after its \
                class must not take one of their names (repeatable)"
    )]
    pub system_include: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "KIND",
        help = "Read IMAGE's function index from its providers of this kind only"
    )]
    pub provider: Option<String>,
    #[arg(
        long,
        help = "Pair two same-signature callees of one caller on their position alone, with no second link"
    )]
    pub single_link_twins: bool,
    #[arg(long, help = "The map to write")]
    pub map: PathBuf,
    #[arg(long, value_name = "DIR", help = "Where each source's pair tables go")]
    pub out: PathBuf,
}

fn lines_of(path: &Path) -> Result<Vec<String>> {
    Ok(input::text(path, input::MAX_TEXT)?
        .lines()
        .map(str::trim)
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .map(str::to_string)
        .collect())
}

fn attested(path: Option<&Path>) -> Result<BTreeSet<(String, String)>> {
    let mut out = BTreeSet::new();
    let Some(p) = path else { return Ok(out) };
    for l in lines_of(p)? {
        let c: Vec<&str> = l.split('\t').collect();
        match c.as_slice() {
            ["member", cl, n, ..] => {
                out.insert(("member".to_string(), format!("{cl}::{n}")));
                out.insert(("class".to_string(), (*cl).to_string()));
            }
            ["function", n, ..] => {
                out.insert(("function".to_string(), (*n).to_string()));
            }
            ["class", cl, ..] => {
                out.insert(("class".to_string(), (*cl).to_string()));
            }
            _ => {}
        }
    }
    Ok(out)
}

fn write(path: &Path, text: &str) -> Result<()> {
    msvc6_base::srcio::own_write(path, text).with_context(|| format!("writing {}", path.display()))
}

pub fn run(config: Option<&Path>, a: &PairArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let (n, _) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    let mut r = p.reference(n)?;
    let img =
        Image::parse(std::mem::take(&mut r.bytes)).with_context(|| r.path.display().to_string())?;
    let label = r
        .path
        .file_name()
        .map_or_else(|| a.image.clone(), |f| f.to_string_lossy().into_owned());
    let base = u64::from(img.opt.image_base.get());
    let ix = crate::sibling::index_for(&p, n, &img, a.provider.as_deref())?;
    let spans: Vec<(u64, u64)> = ix
        .iter()
        .filter(|f| !f.thunk)
        .filter_map(|f| {
            let end = f.end.filter(|e| *e > f.rva)?;
            Some((
                base.saturating_add(u64::from(f.rva)),
                base.saturating_add(u64::from(end)),
            ))
        })
        .collect();
    let rows = annotations::parse(&input::text(&a.annotations, input::MAX_TEXT)?)
        .with_context(|| a.annotations.display().to_string())?;
    let rows = side::choose_rows(
        rows,
        a.annotation_image.as_deref().unwrap_or(&label),
        &a.prefer,
    );
    let mut texts: BTreeMap<String, String> = BTreeMap::new();
    for row in rows.values().filter(|r| !r.is_library_entry()) {
        if texts.contains_key(&row.path) {
            continue;
        }
        let t = input::read(&a.root.join(&row.path), input::MAX_TEXT)
            .map(|b| msvc6_base::srcio::decode(&b))
            .unwrap_or_default();
        texts.insert(row.path.clone(), t);
    }
    let (ours, info) = side::our_side(&OurInput {
        label: &label,
        img: &img,
        spans,
        rows: &rows,
        texts: &texts,
        scope: &a.scope,
    });
    println!(
        "mac pair: {label} {} functions, {} in scope, API universe {}",
        ours.funs.len(),
        info.in_scope.iter().filter(|x| **x).count(),
        info.universe.len()
    );
    let sources: Vec<Source> = side::parse_sources(&input::text(&a.sources, input::MAX_TEXT)?)
        .with_context(|| a.sources.display().to_string())?;
    let mut sides = Vec::new();
    for s in &sources {
        let bytes = input::read(&s.path, input::MAX_IMAGE)?;
        let side = side::source_side(s, &bytes, &info.universe)
            .with_context(|| format!("{}: {}", s.label, s.path.display()))?;
        println!("mac pair: {} {} named functions", s.label, side.funs.len());
        sides.push(side);
    }
    let need: BTreeSet<String> = sources.iter().filter_map(|s| s.twin.clone()).collect();
    let mut twins = BTreeMap::new();
    if !need.is_empty() {
        let list = a
            .twins
            .as_ref()
            .context("a source names a PC twin: give --twins")?;
        let oracles = parse_oracles(&input::text(list, input::MAX_TEXT)?)
            .with_context(|| list.display().to_string())?;
        for t in &need {
            let or = oracles
                .iter()
                .find(|o| &o.label == t)
                .with_context(|| format!("twin `{t}` is not in {}", list.display()))?;
            let timg = Image::parse(input::read(&or.path, input::MAX_IMAGE)?)
                .with_context(|| or.path.display().to_string())?;
            twins.insert(t.clone(), side::pc_side(t, &timg, &info.universe));
        }
    }
    let rules = Rules {
        twins_need_two_links: !a.single_link_twins,
    };
    let out = mac::pair_all(Inputs {
        sources: &sources,
        sides,
        twins,
        ours: &ours,
        info: &info,
        rules,
    });
    for l in &out.log {
        println!("mac pair: {l}");
    }
    let classes: BTreeSet<String> = match &a.classes {
        Some(c) => lines_of(c)?.into_iter().collect(),
        None => BTreeSet::new(),
    };
    let attested = attested(a.attested.as_deref())?;
    let prior: Vec<MapRow> = match &a.prior {
        Some(m) => names::parse_map(&input::text(m, input::MAX_TEXT)?)?,
        None => Vec::new(),
    };
    let measured = match &a.measured {
        Some(m) => Some(input::text(m, input::MAX_TEXT)?),
        None => None,
    };
    let (rows, stale) = mac::decide(
        &out,
        &sources,
        &ours,
        &info,
        &Decisions {
            context: Context {
                image: &label,
                attested: &attested,
                classes: &classes,
                class_families: &a.class_family,
            },
            prior: &prior,
            measured: measured.as_deref(),
        },
    )?;
    for s in &stale {
        eprintln!("mac pair: measured row {s} names no APPLIED row (stale)");
    }
    let tracked = tracked_files(&a.root)?;
    let mut headers: BTreeSet<String> = BTreeSet::new();
    for d in &a.system_include {
        for e in std::fs::read_dir(d)
            .with_context(|| format!("reading {}", d.display()))?
            .flatten()
        {
            headers.insert(e.file_name().to_string_lossy().to_ascii_lowercase());
        }
    }
    let file_rows = msvc6_match::mac::files::file_rows(&msvc6_match::mac::files::FileInputs {
        rows: &rows,
        sources: &sources,
        sides: &out.sides,
        ours: &ours,
        info: &info,
        prior: &prior,
        tracked: &tracked,
        scope: &a.scope,
        system_headers: &headers,
        image: &label,
    });
    let mut rows = rows;
    rows.extend(file_rows);
    std::fs::create_dir_all(&a.out).with_context(|| format!("creating {}", a.out.display()))?;
    for (s, (side, dp)) in sources.iter().zip(out.sides.iter().zip(out.direct.iter())) {
        write(
            &a.out.join(format!("{}.direct.tsv", s.label)),
            &names::direct_table(side, &ours, dp),
        )?;
        if s.kind == side::SourceKind::Pef {
            write(
                &a.out.join(format!("{}.runs.tsv", s.label)),
                &names::runs_table(&rows, side, &ours),
            )?;
        }
    }
    write(
        &a.map,
        &names::render_map(&rows, &names::preamble(&sources, &label)),
    )?;
    for l in names::summary(&rows) {
        println!("MAC-NAMES {l}");
    }
    Ok(Outcome::Ok)
}

#[derive(Debug, Args)]
pub struct TypesArgs {
    #[arg(
        help = "The name map: `rva<TAB>kind<TAB>old<TAB>new<TAB>state<TAB>mangled<TAB>evidence[<TAB>mac<TAB>types]` rows"
    )]
    pub map: PathBuf,
    #[arg(
        long,
        help = "Our image: its name oracle's decorated names at each row's rva give our parameter types"
    )]
    pub image: Option<String>,
    #[arg(
        long,
        value_name = "DUMP",
        help = "First write the map's `mac` column from a `demangled<TAB>mangled` dump of the Mac image's text \
                symbols, and append one `mac-only` row per Mac symbol of a mapped class no row takes"
    )]
    pub fill: Option<PathBuf>,
    #[arg(
        long,
        help = "The verdicts on kinds of difference: `FROM=>TO` (or ARITY, CONST) then `APPLIED` or `RECORDED: why` \
                (default: `<map stem>-types.tsv` beside the map, when it exists)"
    )]
    pub verdicts: Option<PathBuf>,
    #[arg(long, help = "Write the per-symbol table here")]
    pub table: Option<PathBuf>,
}

fn names_by_rva(p: &Config, image: &str) -> Result<BTreeMap<u32, Vec<String>>> {
    let (n, _) = msvc6_project::registry::resolve_id(p, image)?;
    let r = p.reference(n)?;
    let oracle = msvc6_project::names::oracle_of(p, n, &r)?;
    let mut by: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    for nm in oracle.iter().filter(|nm| nm.name.starts_with('?')) {
        by.entry(nm.rva).or_default().push(nm.name.clone());
    }
    Ok(by)
}

pub fn types(config: Option<&Path>, a: &TypesArgs) -> Result<Outcome> {
    use msvc6_match::mac::types;
    let mut lines = types::parse(&input::text(&a.map, input::MAX_TEXT)?);
    let by_rva = match &a.image {
        Some(id) => names_by_rva(&Config::require(config)?, id)?,
        None => BTreeMap::new(),
    };
    if let Some(f) = &a.fill {
        let r = types::fill(&mut lines, &input::text(f, input::MAX_TEXT)?, &by_rva);
        write(&a.map, &types::render(&lines))?;
        println!(
            "mac types --fill: {} row(s) given a Mac signature, {} already had one, {} name no Mac function, {} ambiguous among overloads, {} mac-only row(s) appended",
            r.set,
            r.kept,
            r.none,
            r.ambiguous.len(),
            r.added
        );
        for x in &r.ambiguous {
            println!("  AMBIGUOUS {x}");
        }
    }
    let vpath = a.verdicts.clone().unwrap_or_else(|| {
        a.map.with_file_name(format!(
            "{}-types.tsv",
            a.map
                .file_stem()
                .map_or_else(|| "map".into(), |s| s.to_string_lossy().into_owned())
        ))
    });
    let verdicts = if vpath.is_file() {
        types::parse_verdicts(&input::text(&vpath, input::MAX_TEXT)?)
    } else {
        BTreeMap::new()
    };
    let t = types::table(&lines, &by_rva, &verdicts);
    if let Some(o) = &a.table {
        write(o, &t.text)?;
    }
    print!("{}", types::summary(&t));
    for b in &t.bad {
        println!("  {b}");
    }
    Ok(match types::check(&t) {
        Ok(()) => Outcome::Ok,
        Err(e) => {
            println!("mac types: {e}");
            Outcome::Differs
        }
    })
}

pub const MAX_TRACKED: usize = 1 << 20;

fn walk(root: &Path, dir: &Path, out: &mut Vec<String>) -> Result<()> {
    for e in std::fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .flatten()
    {
        let p = e.path();
        let Ok(t) = e.file_type() else { continue };
        if t.is_dir() && e.file_name() != ".git" {
            walk(root, &p, out)?;
        } else if t.is_file() {
            if out.len() >= MAX_TRACKED {
                anyhow::bail!("{}: more than {MAX_TRACKED} files", root.display());
            }
            if let Ok(rel) = p.strip_prefix(root) {
                out.push(msvc6_source::tree::rel_str(rel));
            }
        }
    }
    Ok(())
}

fn tracked_files(root: &Path) -> Result<Vec<String>> {
    if msvc6_source::git::is_repo(root) {
        return Ok(msvc6_source::git::listed(root)?
            .iter()
            .map(|p| msvc6_source::tree::rel_str(p))
            .collect());
    }
    let mut out = Vec::new();
    walk(root, root, &mut out)?;
    out.sort();
    Ok(out)
}
