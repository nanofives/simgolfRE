use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_image::pe::Image;
use msvc6_match::input;
use msvc6_match::sibling::code::Code;
use msvc6_match::sibling::diff;
use msvc6_match::sibling::discover::{atoms, discover};
use msvc6_match::sibling::feat::{Fun, Side, build};
use msvc6_match::sibling::pair::{Structure, pair_with};
use msvc6_match::sibling::report::{self, Report, Run};
use msvc6_match::sibling::{Oracle, home_path, parse_functions, parse_oracles, parse_seed};
use msvc6_match::vftables;
use msvc6_project::config::FunctionSource;
use msvc6_project::functions::{FunctionIndex, read_one};
use serde::Serialize;
use std::collections::{BTreeMap, HashMap};
use std::path::{Path, PathBuf};

#[derive(Debug, Args)]
pub struct SiblingArgs {
    #[command(subcommand)]
    pub verb: SiblingVerb,
}

#[derive(Debug, Subcommand)]
pub enum SiblingVerb {
    #[command(
        about = "Pair our functions with a sibling build's by what survives a compiler change, and read its names, files and objects off the pairs",
        long_about = "Pair the functions of our images (IMAGE, each an [[image]] id, paired as one population) with \
                      those of a SIBLING: another build of code we share, a PE that is only read, never run. The \
                      sibling's functions are found without symbols in its code sections (executable and flagged \
                      code, never a copy protector's `.cms_t`, `.cms_d` or `stxt*`): the entry point, exports, \
                      every call rel32 target, every /Od prologue after a return or pad, every 16-byte line after a \
                      pad, every pointer in data to a start; jmp thunks resolved away, each function cut at the \
                      next start with its trailing pad dropped. Ours come from each image's function index \
                      (--provider KIND: only its providers of that kind), named by --functions, else by the \
                      index, else as Ghidra names an unnamed one (FUN_<va>). The pairing, strongest first: \
                      ANCHORS (a string, normalised, or an import that at most three functions use on both sides; \
                      a masked body hash unique on both sides, exact and loose; a mnemonic 4-gram Dice of 0.55 \
                      clear of its runner-up by 0.08) vote per candidate pair, and a pair lands when it is the \
                      mutual best of both its functions, untied, at one full vote, of comparable size (an \
                      instruction-count ratio of at most 12, or both under 40). Then to a fixed point: CALL ORDER \
                      (a landed pair's callee lists aligned by LCS on imports and landed callees; a gap holding \
                      only unpaired callees of equal length pairs in order, after collapsing repeats if needed, \
                      and any gap is aligned by its callees' shared strings and imports; an unpaired callee of \
                      more than 40 callers holds no position), SOLE CALLERS, SHAPE TIES (near-equal bodies \
                      settled by their paired neighbours), and with --vftables VFTABLE SLOTS (every candidate \
                      .rdata run of code pointers on each side, ILT-followed, as one slot sequence aligned on \
                      landed pairs) and with --layout LAYOUT (both sides in address order: for a sibling linked \
                      from our image's own object order). Every proposal of the rounds after the anchors is \
                      gated: the two functions' calling conventions, read off the bodies (what every return \
                      pops, `ecx` read as a `this` at entry, the stack arguments read), must not clash, and \
                      neither may their pointer-store widths (only bytes against only words); a positional gap \
                      with one clash pairs nothing, unless it is two positions that fit crossed. Writes per \
                      sibling <label>.pairs.tsv (each pair, its atom, round, evidence, shape, the sibling's \
                      __FILE__, the name-like tokens of its strings ours lacks, both conventions and their \
                      clash), .names.tsv, .atomnames.tsv, .atoms.tsv (each run of packed unaligned functions, \
                      one object, whose pairs fall in two or more of our files), .order.tsv (each file's pairs in \
                      our order, inversions, reversed runs), .runs.tsv and .sib.tsv, and ours.runs.tsv and \
                      ours.names.tsv; with --oracles the union tables oracles.atoms.tsv and oracles.order.tsv."
    )]
    Pair(Box<PairArgs>),
    #[command(
        about = "The functions a PE's code holds, found without symbols",
        long_about = "Discover the functions of a PE image as `sibling pair` reads a sibling: entry, exports, call \
                      targets, /Od prologues and 16-byte lines after a return and pad, pointers from data; jmp \
                      thunks resolved away, each cut at the next start, its trailing pad dropped. One row per \
                      function: start, end (virtual addresses), instructions, and its packed-run atom. JSON rows: \
                      one per function."
    )]
    Functions(FunctionsArgs),
    #[command(
        about = "Compare two runs of `sibling pair`: the pairs each made and every other table",
        long_about = "Compare the tables two runs of `sibling pair` wrote (OLD and NEW: two output directories, or \
                      two pairs tables). A pairs table is compared by our function (image and address): a pair \
                      both hold with the same sibling function is SAME when every column agrees, its evidence's \
                      items compared as a set (an evidence list read in another order is counted, and two lists \
                      both holding the 4-item cap are taken as equal); MOVED when the sibling function differs; \
                      LOST or GAINED when one side alone holds it; CHANGED in a column otherwise. Every other \
                      table both directories hold is compared line for line, `#` lines skipped. Exit 0 when \
                      nothing differs, 3 when something does. JSON rows: one per table."
    )]
    Diff(DiffArgs),
}

#[derive(Debug, Args)]
pub struct PairArgs {
    #[arg(
        required = true,
        help = "Our images: [[image]] ids, paired as one population"
    )]
    pub images: Vec<String>,
    #[arg(long, help = "The sibling PE (or --oracles)")]
    pub sibling: Option<PathBuf>,
    #[arg(
        long,
        help = "The sibling's tag in the output file names (with --sibling)"
    )]
    pub label: Option<String>,
    #[arg(
        long,
        conflicts_with = "sibling",
        help = "A `label<TAB>path[<TAB>OURVA=SIBVA,...[<TAB>branch]]` list (`#` lines skipped; `~/` is $HOME): \
                every sibling in it against our side, built once"
    )]
    pub oracles: Option<PathBuf>,
    #[arg(long, value_name = "DIR", help = "Output directory")]
    pub out: PathBuf,
    #[arg(
        long,
        help = "Report only our functions whose source file holds one of these (repeatable)"
    )]
    pub only: Vec<String>,
    #[arg(
        long,
        value_name = "IMAGE=TSV",
        help = "Our functions' files and names from a `rva<TAB>file<TAB>name` table (an rva or a VA, \
                hexadecimal; `#` lines skipped), instead of the function index's names (repeatable)"
    )]
    pub functions: Vec<String>,
    #[arg(
        long,
        value_name = "KIND",
        help = "Read each image's function index from its providers of this kind only (ghidra-json, map, \
                reccmp-csv, pdb, discover)"
    )]
    pub provider: Option<String>,
    #[arg(
        long,
        value_name = "OURVA=SIBVA",
        help = "A fixed pair to start from (hexadecimal, repeatable; with --sibling)"
    )]
    pub seed: Vec<String>,
    #[arg(long, help = "Also pair vftable slots")]
    pub vftables: bool,
    #[arg(
        long,
        help = "Also pair along the two layouts (a sibling linked from our image's object order)"
    )]
    pub layout: bool,
}

#[derive(Debug, Args)]
pub struct FunctionsArgs {
    pub image: PathBuf,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DiffArgs {
    pub old: PathBuf,
    pub new: PathBuf,
    #[arg(
        long,
        help = "Print every differing pair, not only the first 20 of each kind"
    )]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

fn kind_of(s: &FunctionSource) -> &'static str {
    match s {
        FunctionSource::GhidraJson(_) => "ghidra-json",
        FunctionSource::Map(_) => "map",
        FunctionSource::ReccmpCsv(_) => "reccmp-csv",
        FunctionSource::Pdb(_) => "pdb",
        FunctionSource::Discover => "discover",
    }
}

pub fn index_for(
    p: &Config,
    n: usize,
    img: &Image,
    provider: Option<&str>,
) -> Result<FunctionIndex> {
    let d = p.images.get(n).context("no such image")?;
    let mut ix = FunctionIndex::default();
    let mut used = 0usize;
    for (i, s) in d.functions.iter().enumerate() {
        if provider.is_some_and(|k| k != kind_of(s)) {
            continue;
        }
        let (kind, src, fs, notes) = read_one(p, n, i, s, img)?;
        ix.add(&kind, &src, fs, notes);
        used = used.saturating_add(1);
    }
    if used == 0 {
        bail!(
            "image `{}` has no function provider{}",
            d.id,
            provider.map_or_else(String::new, |k| format!(" of kind {k}"))
        );
    }
    Ok(ix)
}

pub struct Ours {
    pub images: Vec<Image>,
    pub labels: Vec<report::Image>,
    pub side: Side,
}

fn file_label(path: &Path, fallback: &str) -> String {
    path.file_name().map_or_else(
        || fallback.to_string(),
        |n| n.to_string_lossy().into_owned(),
    )
}

fn tables_by_image(specs: &[String]) -> Result<HashMap<String, PathBuf>> {
    let mut out = HashMap::new();
    for s in specs {
        let (k, v) = s
            .split_once('=')
            .with_context(|| format!("--functions `{s}` is not IMAGE=TSV"))?;
        out.insert(k.to_string(), PathBuf::from(v));
    }
    Ok(out)
}

pub fn our_side(
    p: &Config,
    ids: &[String],
    tables: &[String],
    provider: Option<&str>,
) -> Result<Ours> {
    let tables = tables_by_image(tables)?;
    let mut ours = Ours {
        images: Vec::new(),
        labels: Vec::new(),
        side: Side::default(),
    };
    for id in ids {
        let (n, _) = msvc6_project::registry::resolve_id(p, id)?;
        let mut r = p.reference(n)?;
        let img = Image::parse(std::mem::take(&mut r.bytes))
            .with_context(|| r.path.display().to_string())?;
        let label = file_label(&r.path, id);
        let base = img.opt.image_base.get();
        let names = match tables.get(id.as_str()).or_else(|| tables.get(&label)) {
            Some(t) => parse_functions(&input::text(t, input::MAX_TEXT)?, base),
            None => BTreeMap::new(),
        };
        let ix = index_for(p, n, &img, provider)?;
        let mut funs: Vec<Fun> = Vec::new();
        for f in ix.iter().filter(|f| !f.thunk) {
            let Some(end) = f.end.filter(|e| *e > f.rva) else {
                continue;
            };
            let (file, name) = names.get(&f.rva).cloned().unwrap_or_else(|| {
                (
                    String::new(),
                    f.name.clone().unwrap_or_else(|| {
                        format!(
                            "FUN_{:08x}",
                            u64::from(base).saturating_add(u64::from(f.rva))
                        )
                    }),
                )
            });
            funs.push(Fun {
                image: label.clone(),
                rva: f.rva,
                end,
                name,
                file,
            });
        }
        build(&Code::new(&img), funs, &mut ours.side);
        ours.labels.push(report::Image { label, base });
        ours.images.push(img);
    }
    ours.side.finish();
    Ok(ours)
}

fn slot_tables(img: &Image, side: &Side, image: Option<&str>) -> Vec<Option<usize>> {
    let Ok(runs) = vftables::detect(img, ".text", ".rdata") else {
        return Vec::new();
    };
    let Some(tsec) = img.section(".text") else {
        return Vec::new();
    };
    let text = img.section_bytes(tsec);
    let trva = tsec.rva.get();
    let at: HashMap<u32, usize> = side
        .funs
        .iter()
        .enumerate()
        .filter(|(_, f)| image.is_none_or(|i| f.image == i))
        .map(|(k, f)| (f.rva, k))
        .collect();
    runs.iter()
        .flat_map(|r| r.slots.iter())
        .map(|r| at.get(&vftables::follow(text, trva, *r)).copied())
        .collect()
}

pub fn sibling_side(path: &Path, label: &str) -> Result<(Image, Side)> {
    let bytes = input::read(path, input::MAX_IMAGE)?;
    let img = Image::parse(bytes).with_context(|| path.display().to_string())?;
    let mut side = Side::default();
    {
        let code = Code::new(&img);
        let funs = discover(&code, label);
        build(&code, funs, &mut side);
    }
    side.finish();
    let spans: Vec<(u32, u32)> = side.funs.iter().map(|f| (f.rva, f.end)).collect();
    side.atom = atoms(&spans);
    Ok((img, side))
}

fn seeds(ours: &Ours, sib: &Side, simg: &Image, or: &Oracle) -> Result<Vec<(usize, usize)>> {
    let base = |label: &str| {
        ours.labels
            .iter()
            .find(|l| l.label == label)
            .map_or(0, |l| l.base)
    };
    let sbase = u64::from(simg.opt.image_base.get());
    let mut out = Vec::new();
    for (a, b) in &or.seeds {
        let i = ours
            .side
            .funs
            .iter()
            .position(|f| u64::from(base(&f.image)).saturating_add(u64::from(f.rva)) == *a);
        let j = sib
            .funs
            .iter()
            .position(|g| sbase.saturating_add(u64::from(g.rva)) == *b);
        match (i, j) {
            (Some(i), Some(j)) => out.push((i, j)),
            (None, _) => bail!("seed {a:#x}={b:#x}: no function of ours starts at {a:#x}"),
            (_, None) => bail!("seed {a:#x}={b:#x}: no sibling function starts at {b:#x}"),
        }
    }
    Ok(out)
}

fn one(a: &PairArgs, ours: &Ours, or: &Oracle) -> Result<Report> {
    let (simg, sib) = sibling_side(&or.path, &or.label)?;
    let seeds = seeds(ours, &sib, &simg, or)?;
    let mut st = Structure {
        layout: a.layout,
        ..Structure::default()
    };
    if a.vftables {
        for (img, l) in ours.images.iter().zip(&ours.labels) {
            st.ours.extend(slot_tables(img, &ours.side, Some(&l.label)));
        }
        st.sib = slot_tables(&simg, &sib, None);
    }
    let pairs = pair_with(&ours.side, &sib, &seeds, &st);
    let code = Code::new(&simg);
    let run = Run {
        ours: &ours.side,
        images: &ours.labels,
        sib: &sib,
        sib_code: &code,
        pairs: &pairs,
        label: &or.label,
        only: &a.only,
    };
    Ok(report::report(&run, &a.out.display().to_string()))
}

fn write(dir: &Path, name: &str, text: &str) -> Result<()> {
    let p = dir.join(name);
    msvc6_base::srcio::own_write(&p, text).with_context(|| format!("writing {}", p.display()))
}

pub fn pair(config: Option<&Path>, a: &PairArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let list = match (&a.oracles, &a.sibling) {
        (Some(o), _) => parse_oracles(&input::text(o, input::MAX_TEXT)?)
            .with_context(|| o.display().to_string())?,
        (None, Some(s)) => vec![Oracle {
            label: a.label.clone().unwrap_or_else(|| {
                s.file_stem().map_or_else(
                    || "sibling".to_string(),
                    |n| n.to_string_lossy().into_owned(),
                )
            }),
            path: home_path(&s.display().to_string()),
            seeds: a
                .seed
                .iter()
                .map(|s| parse_seed(s))
                .collect::<Result<_>>()?,
            branch: String::new(),
        }],
        (None, None) => bail!("give --sibling or --oracles"),
    };
    let ours = our_side(&p, &a.images, &a.functions, a.provider.as_deref())?;
    std::fs::create_dir_all(&a.out).with_context(|| format!("creating {}", a.out.display()))?;
    let mut rows = Vec::new();
    for or in &list {
        let r = one(a, &ours, or).with_context(|| format!("sibling {}", or.label))?;
        for (name, text) in &r.files {
            write(&a.out, name, text)?;
        }
        print!("{}", r.summary);
        rows.push((or.label.clone(), or.branch.clone(), r.atom_rows, r.inverted));
    }
    if let Some(o) = &a.oracles {
        let (atoms, order, na, no) = report::union_tables(&o.display().to_string(), &rows);
        write(&a.out, "oracles.atoms.tsv", &atoms)?;
        write(&a.out, "oracles.order.tsv", &order)?;
        println!(
            "wrote {}/oracles.{{atoms,order}}.tsv ({na} atom row(s), {no} inverted file row(s))",
            a.out.display()
        );
    }
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct FunctionRow {
    start: String,
    end: String,
    instructions: usize,
    atom: usize,
}

pub fn functions(a: &FunctionsArgs) -> Result<Outcome> {
    let (img, side) = sibling_side(&a.image, "")?;
    let code = Code::new(&img);
    let mut rows = Vec::new();
    for (k, f) in side.funs.iter().enumerate() {
        let row = FunctionRow {
            start: format!("{:#x}", code.va(f.rva)),
            end: format!("{:#x}", code.va(f.end)),
            instructions: side.feats.get(k).map_or(0, |x| x.mnem.len()),
            atom: side.atom.get(k).copied().unwrap_or(0),
        };
        println!(
            "{}\t{}\t{}\t{}",
            row.start, row.end, row.instructions, row.atom
        );
        rows.push(row);
    }
    eprintln!("{}: {} function(s)", a.image.display(), rows.len());
    json(
        &a.json,
        "sibling functions",
        &a.image.display().to_string(),
        None::<&()>,
        &rows,
    )?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct TableRow {
    table: String,
    status: String,
    detail: String,
}

fn tables_in(dir: &Path) -> Result<Vec<String>> {
    let mut out: Vec<String> = std::fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .flatten()
        .filter(|e| e.file_type().is_ok_and(|t| t.is_file()))
        .map(|e| e.file_name().to_string_lossy().into_owned())
        .filter(|n| n.ends_with(".tsv"))
        .collect();
    out.sort();
    Ok(out)
}

fn show<T: std::fmt::Display>(what: &str, items: &[T], all: bool) {
    for x in items.iter().take(if all { usize::MAX } else { 20 }) {
        println!("  {what} {x}");
    }
    if !all && items.len() > 20 {
        println!("  {what} ... {} more", items.len() - 20);
    }
}

fn pairs_row(name: &str, old: &str, new: &str, all: bool) -> Result<TableRow> {
    let (head, a) = diff::parse_pairs(old).with_context(|| format!("OLD {name}"))?;
    let (_, b) = diff::parse_pairs(new).with_context(|| format!("NEW {name}"))?;
    let d = diff::pairs(&head, &a, &b);
    let detail = format!(
        "{} on both sides, {} same ({} with their evidence in another order, {} at the evidence cap); {} moved, {} lost, {} gained, {} changed",
        d.both,
        d.same,
        d.reordered,
        d.capped,
        d.moved.len(),
        d.lost.len(),
        d.gained.len(),
        d.changed.len()
    );
    println!("pairs {name}: {detail}");
    let cell =
        |c: &diff::Changed| format!("{} {} {}: {} -> {}", c.image, c.va, c.column, c.old, c.new);
    show("MOVED", &d.moved.iter().map(cell).collect::<Vec<_>>(), all);
    show(
        "CHANGED",
        &d.changed.iter().map(cell).collect::<Vec<_>>(),
        all,
    );
    let key = |k: &(String, String)| format!("{} {}", k.0, k.1);
    show("LOST", &d.lost.iter().map(key).collect::<Vec<_>>(), all);
    show("GAINED", &d.gained.iter().map(key).collect::<Vec<_>>(), all);
    Ok(TableRow {
        table: name.to_string(),
        status: if d.equal() { "same" } else { "DIFFERS" }.to_string(),
        detail,
    })
}

fn table_row(name: &str, old: &str, new: &str) -> TableRow {
    let lines = old.lines().filter(|l| !l.starts_with('#')).count();
    match diff::first_difference(old, new) {
        None => {
            println!("table {name}: same ({lines} line(s))");
            TableRow {
                table: name.to_string(),
                status: "same".into(),
                detail: format!("{lines} line(s)"),
            }
        }
        Some((n, a, b)) => {
            println!("table {name}: DIFFERS at line {n}\n  OLD {a}\n  NEW {b}");
            TableRow {
                table: name.to_string(),
                status: "DIFFERS".into(),
                detail: format!("line {n}"),
            }
        }
    }
}

pub fn diff_verb(a: &DiffArgs) -> Result<Outcome> {
    let pairs_of: Vec<(String, PathBuf, PathBuf)> = if a.old.is_dir() && a.new.is_dir() {
        let (x, y) = (tables_in(&a.old)?, tables_in(&a.new)?);
        for n in x.iter().filter(|n| !y.contains(n)) {
            println!("only in OLD: {n}");
        }
        for n in y.iter().filter(|n| !x.contains(n)) {
            println!("only in NEW: {n}");
        }
        x.iter()
            .filter(|n| y.contains(n))
            .map(|n| (n.clone(), a.old.join(n), a.new.join(n)))
            .collect()
    } else {
        vec![(file_label(&a.new, "pairs"), a.old.clone(), a.new.clone())]
    };
    if pairs_of.is_empty() {
        bail!("no table both sides hold");
    }
    let mut rows = Vec::new();
    for (name, o, n) in &pairs_of {
        let old = input::text(o, input::MAX_TEXT)?;
        let new = input::text(n, input::MAX_TEXT)?;
        let row = if old.starts_with(report::PAIRS_HEAD) && new.starts_with(report::PAIRS_HEAD) {
            pairs_row(name, &old, &new, a.all)?
        } else {
            table_row(name, &old, &new)
        };
        rows.push(row);
    }
    let bad = rows.iter().filter(|r| r.status != "same").count();
    println!("diff: {} table(s), {bad} differ", rows.len());
    json(
        &a.json,
        "sibling diff",
        &a.new.display().to_string(),
        None::<&()>,
        &rows,
    )?;
    Ok(if bad > 0 {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}
