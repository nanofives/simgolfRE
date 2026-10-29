use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_base::{bounded, srcio};
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_diff::env::Env;
use msvc6_model::{floatclass, hdrcombo, hdrprice, labelsafe, labelsweep, tubudget, tuflags};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

pub const MAX_SOURCE: u64 = 64 << 20;

#[derive(Debug, Args)]
pub struct HdrArgs {
    #[command(subcommand)]
    pub verb: HdrVerb,
}

#[derive(Debug, Args)]
pub struct Common {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id whose project the TUs come from."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "STEM",
        help = "Only the TUs whose stem is this (exact). Repeatable."
    )]
    pub tu: Vec<String>,
    #[arg(
        long,
        value_name = "N",
        help = "Compiles at once. Default: min(8, nproc/2)."
    )]
    pub jobs: Option<usize>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where the overlays and compile directories go. Default: a per-run directory under the temporary \
                directory, removed when the run ends."
    )]
    pub work: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct PriceArgs {
    #[command(flatten)]
    pub common: Common,
    #[arg(
        value_name = "HEADER",
        help = "The header as it stands in the project tree."
    )]
    pub header: PathBuf,
    #[arg(long, value_name = "FILE", help = "The edited header.")]
    pub with: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "This text in place of the TU of the same stem, for an edit that moves a declaration out of the \
                header into its owner."
    )]
    pub src: Option<PathBuf>,
    #[arg(
        long,
        help = "Measure only C1's counter shift per includer, from two C1-only runs (/Bk keeps the front end's IL \
                and /B2 names a C2 that does not exist), instead of two whole compiles."
    )]
    pub c1: bool,
}

#[derive(Debug, Args)]
pub struct SweepArgs {
    #[command(flatten)]
    pub common: Common,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 1,
        help = "Sweep every Nth pad of 0..64."
    )]
    pub step: usize,
    #[arg(
        long,
        value_name = "LINE",
        default_value_t = 1,
        help = "The 1-based source line the pad goes ahead of (1 = the top)."
    )]
    pub at: usize,
    #[arg(
        long,
        help = "Pad ahead of the line after each TU's last top-level `#include' instead of --at, which is where a \
                header edit's shift starts for everything the TU itself declares."
    )]
    pub after_includes: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Pad this header instead of the TU: every includer compiles against a copy of the header's \
                directory with the pad in it."
    )]
    pub header: Option<PathBuf>,
    #[arg(
        long,
        value_name = "N",
        help = "Only this pad, any count (0 is always added; 64 is the period check). Repeatable."
    )]
    pub pad: Vec<usize>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write every function's curve as TSV."
    )]
    pub curves: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct SafeArgs {
    #[command(flatten)]
    pub sweep: SweepArgs,
    #[arg(long, value_name = "FILE", help = "Write the safe-set table as TSV.")]
    pub out: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write each function's first `$L' at pad 0 as `tu symbol label', the table a --delta measured in a \
                tree carrying the edit is read against."
    )]
    pub labels: Option<PathBuf>,
    #[arg(
        long,
        value_name = "NAME=FILE",
        help = "One candidate edit's shift per TU, read from a saved `hdr price --c1' output, or per function from \
                a --labels table. Repeatable."
    )]
    pub delta: Vec<String>,
    #[arg(
        long,
        value_name = "N",
        help = "At most this many edits per combination."
    )]
    pub max: Option<usize>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 12,
        help = "Rows printed per ranking."
    )]
    pub top: usize,
}

#[derive(Debug, Args)]
pub struct ComboArgs {
    #[command(flatten)]
    pub common: Common,
    #[arg(
        value_name = "FILE",
        help = "The atoms: named patch hunks in unified-diff form."
    )]
    pub atoms: PathBuf,
    #[arg(
        long,
        value_name = "DIR",
        help = "The tree the atom paths are relative to. Default: the image project's directory."
    )]
    pub root: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "A directory of --root a combination copies (hard-linked) and patches. Repeatable; the default is \
                every directory the atoms' paths lead with."
    )]
    pub tree: Vec<String>,
    #[arg(
        long,
        value_name = "N",
        help = "At most this many free atoms per combination."
    )]
    pub max: Option<usize>,
    #[arg(
        long,
        value_name = "NAME",
        help = "An atom every combination carries. Repeatable."
    )]
    pub require: Vec<String>,
    #[arg(
        long,
        value_name = "NAME",
        help = "Write this combination into the tree and exit. Repeatable."
    )]
    pub apply: Vec<String>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 4,
        help = "Also print this many best combinations that have a riser."
    )]
    pub near: usize,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write every combination's row as TSV."
    )]
    pub out: Option<PathBuf>,
}

#[derive(Debug, Subcommand)]
pub enum HdrVerb {
    #[command(
        about = "Price a header edit over every translation unit that includes the header",
        long_about = "An inline respelled in a shared header is not a one-caller edit: C1's shared entity and label \
                      counter moves in every includer, and a C2 tie that keys on that counter mod 64 re-rolls. This \
                      compiles every includer twice -- as it stands, and with the header's directory replaced by a \
                      copy holding the edit -- and reports what moved: per COMDAT whose bytes or relocation targets \
                      changed, its kind (INLINE when its call-target counts changed, NUMBERING when they did not and \
                      the shift is not 0 mod 64, else CODEGEN) and, where the image seats it, the differing and \
                      aligned edit bytes before and after. The `$L' column is the one shift every later function \
                      moved by: 0 mod 64 leaves every label-keyed tie where it was. A failed compile prints FAILED \
                      and the run exits non-zero rather than a plausible +0 total. --c1 measures only the counter, \
                      from two C1-only runs. Exit 0, 1 when a compile failed."
    )]
    Price(Box<PriceArgs>),
    #[command(
        about = "Sweep C1's label counter: which functions a pure renumbering moves, and to what",
        long_about = "Compile each TU with 0..64 `extern int' declarations ahead of it (or ahead of a header, \
                      --header) and print every function whose aligned edit a pure renumbering lowers, with the pads \
                      that lower it. A pad k reads as `the original TU had k (or k-64) more counter numbers ahead of \
                      this function than ours'. The period is exactly 64, so pad 64 is the period check and must \
                      read as pad 0. The ADDEND rows are the label-keyed tie the edit count is blind to: two field \
                      loads swapped, every byte equal under the relocation mask. The BASE rows are the self-check -- \
                      every seated function of the pad-0 compile that is not byte-exact, with its figure to hold \
                      against a real build, and whether any pad changed its bytes at all. Each TU's pad directories \
                      are removed once its curves are read. Exit 0, 1 when a compile failed."
    )]
    Sweep(Box<SweepArgs>),
    #[command(
        about = "Every label-tied function's safe set of counter shifts, and which combination of edits fits",
        long_about = "A header edit can only land when the shift it gives each tied TU is one that TU tolerates. \
                      `hdr sweep' measures one TU's curve; this keeps every tied function's whole curve as a SET of \
                      residues and prices any sum of candidate edits against all of them at once, so a combination \
                      is found by arithmetic rather than by compiles. For a function any pad moves, residue r is \
                      SAFE when its differing, edit and addend at pad r are each no higher than at pad 0, and it \
                      FALLS when one is lower; a TU is safe at r when every one of its functions is. A --delta is \
                      one edit's shift per TU (a saved `hdr price --c1' output) or per function (a --labels table \
                      written in a tree carrying the edit), and every subset is priced: a RISER is a function whose \
                      figure rises, UNKNOWN a residue the sweep did not compile, UNSWEPT a TU an edit shifts that \
                      this run did not sweep. The model is exact only for ties keyed on the function's own start \
                      counter, so run --at 1 and --after-includes and trust a row only where they agree. Exit 0."
    )]
    Safe(Box<SafeArgs>),
    #[command(
        about = "Search combinations of header edits for one that moves bytes with no riser",
        long_about = "An owed header dedupe changes no instruction, but it moves C1's counter in every includer, so \
                      each atom alone may open a tie that three together close. Every combination of the atoms (up \
                      to --max of them, each with every --require atom) is applied to a hard-linked copy of the \
                      named trees, and each TU the patched tree makes reach a touched file is compiled against that \
                      copy. Every COMDAT whose bytes, relocation targets or addends differ from the unpatched \
                      compile is scored, so a TU a combination does not move costs nothing; two combinations that \
                      leave every file a TU opens alike compile it once. A RISER is any seated function whose edit \
                      rises. The command never edits the tree unless --apply names a combination, which it writes \
                      and exits. Restricting --tu to the counter-tie TUs is a screen, not a gate: an atom that \
                      changes a declaration a body uses moves it by codegen, in a TU no pad moves. Exit 0."
    )]
    Combo(Box<ComboArgs>),
}

fn jobs(j: Option<usize>) -> usize {
    crate::diff::jobs(j)
}

struct Project {
    p: Config,
    n: usize,
    label: String,
    units: Vec<Unit>,
}

fn units_of(config: Option<&Path>, image: &str) -> Result<Project> {
    let p = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, image)?;
    let specs = msvc6_project::spec::resolve(&p, n)?;
    let default_tc = d.compiler.clone().unwrap_or_else(|| "vc6".to_string());
    let include_env = d
        .build
        .as_ref()
        .map(|b| msvc6_diff::compile::build_include(&p.toolchain_config(), b))
        .transpose()?
        .map(|(inc, _)| inc);
    let mut units: Vec<Unit> = Vec::new();
    for s in &specs.specs {
        if units.iter().any(|u| u.stem.eq_ignore_ascii_case(&s.stem)) {
            continue;
        }
        let mut u = Unit::of_spec(s, Path::new(&s.path), &default_tc)?;
        u.include_env.clone_from(&include_env);
        units.push(u);
    }
    units.sort_by(|a, b| a.stem.cmp(&b.stem));
    Ok(Project {
        label: d.id.clone(),
        p,
        n,
        units,
    })
}

fn chosen<'u>(units: &'u [Unit], want: &[String]) -> Result<Vec<&'u Unit>> {
    if want.is_empty() {
        return Ok(units.iter().collect());
    }
    want.iter()
        .map(|w| {
            units
                .iter()
                .find(|u| u.stem.eq_ignore_ascii_case(w))
                .with_context(|| format!("no TU named {w} in the image's project"))
        })
        .collect()
}

fn work_dir(given: Option<&Path>) -> PathBuf {
    given.map_or_else(
        || std::env::temp_dir().join("msvc6t-work"),
        Path::to_path_buf,
    )
}

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

fn warn(lines: &[String]) {
    for l in lines {
        eprintln!("{l}");
    }
}

pub fn run(config: Option<&Path>, a: &HdrArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    match &a.verb {
        HdrVerb::Price(x) => price(config, x, guard),
        HdrVerb::Sweep(x) => sweep(config, x, guard),
        HdrVerb::Safe(x) => safe(config, x, guard),
        HdrVerb::Combo(x) => combo(config, x, guard),
    }
}

fn price(config: Option<&Path>, a: &PriceArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    let pr = units_of(config, &a.common.image)?;
    let units = chosen(&pr.units, &a.common.tu)?;
    let owned: Vec<Unit> = units.into_iter().cloned().collect();
    guard()?;
    let c = Compiler::new(&pr.p)?;
    let env = if a.c1 {
        None
    } else {
        Some(Env::load(&pr.p, pr.n)?)
    };
    let work = work_dir(a.common.work.as_deref());
    let p = hdrprice::price(
        &c,
        env.as_ref(),
        &owned,
        &hdrprice::PriceOpts {
            label: &pr.label,
            header: &a.header,
            with: &a.with,
            src: a.src.as_deref(),
            c1: a.c1,
            jobs: jobs(a.common.jobs),
            work: &work,
        },
    )?;
    warn(&p.errors);
    say(&p.lines);
    say(&hdrprice::failed_lines(&p.failed));
    json(
        &a.common.json,
        "hdr price",
        &pr.label,
        None::<&()>,
        &p.lines,
    )?;
    match hdrprice::compiles_verdict(&p.failed, p.compiles) {
        Ok(()) => Ok(Outcome::Ok),
        Err(e) => {
            eprintln!("msvc6t: {e:#}");
            Ok(Outcome::Exit(ExitCode::from(1)))
        }
    }
}

fn sweep_opts<'a>(label: &'a str, a: &'a SweepArgs, work: &'a Path) -> labelsweep::SweepOpts<'a> {
    labelsweep::SweepOpts {
        label,
        step: a.step,
        at: a.at,
        header: a.header.as_deref(),
        pads: &a.pad,
        jobs: jobs(a.common.jobs),
        work,
    }
}

fn swept(
    config: Option<&Path>,
    a: &SweepArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<(Project, labelsweep::Swept)> {
    let pr = units_of(config, &a.common.image)?;
    let units = chosen(&pr.units, &a.common.tu)?;
    if a.after_includes && a.header.is_some() {
        bail!("--after-includes pads the TU; --header pads the header: name one");
    }
    guard()?;
    let c = Compiler::new(&pr.p)?;
    let env = Env::load(&pr.p, pr.n)?;
    let work = work_dir(a.common.work.as_deref());
    let mut all = Vec::new();
    let mut s = labelsweep::Swept::default();
    for u in &units {
        let at = if a.after_includes {
            labelsafe::after_includes(&bounded::source(&u.source, MAX_SOURCE)?)
        } else {
            a.at
        };
        let mut o = sweep_opts(&pr.label, a, &work);
        o.at = at;
        let one = labelsweep::sweep(&c, &env, &pr.units, &[u], &o)?;
        all.push(one);
    }
    for one in all {
        s.lines.extend(one.lines);
        if s.tsv.is_empty() {
            s.tsv = one.tsv;
        } else {
            s.tsv.push_str(
                one.tsv
                    .lines()
                    .skip(1)
                    .collect::<Vec<_>>()
                    .join("\n")
                    .as_str(),
            );
            if !one.tsv.is_empty() {
                s.tsv.push('\n');
            }
        }
        s.labels.extend(one.labels);
        s.errors.extend(one.errors);
        s.failed.extend(one.failed);
        s.compiles += one.compiles;
    }
    Ok((pr, s))
}

fn sweep(config: Option<&Path>, a: &SweepArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    let (pr, s) = swept(config, a, guard)?;
    warn(&s.errors);
    say(&s.lines);
    say(&hdrprice::failed_lines(&s.failed));
    if let Some(p) = &a.curves {
        srcio::own_write(p, &s.tsv).with_context(|| format!("writing {}", p.display()))?;
        println!("  wrote {}", p.display());
    }
    json(
        &a.common.json,
        "hdr sweep",
        &pr.label,
        None::<&()>,
        &s.lines,
    )?;
    match hdrprice::compiles_verdict(&s.failed, s.compiles) {
        Ok(()) => Ok(Outcome::Ok),
        Err(e) => {
            eprintln!("msvc6t: {e:#}");
            Ok(Outcome::Exit(ExitCode::from(1)))
        }
    }
}

fn tied_of(tsv: &str, swept_pads: &[usize]) -> Vec<labelsafe::Tied> {
    let mut by: BTreeMap<(String, u32, String), (labelsweep::Fig, labelsweep::Curve)> =
        BTreeMap::new();
    for l in tsv.lines().skip(1) {
        let f: Vec<&str> = l.split('\t').collect();
        let [tu, rva, sym, pad, d, e, ad] = f.as_slice() else {
            continue;
        };
        let (Ok(rva), Ok(pad), Ok(d), Ok(e), Ok(ad)) = (
            u32::from_str_radix(rva.trim_start_matches("0x"), 16),
            pad.parse::<usize>(),
            d.parse::<usize>(),
            e.parse::<usize>(),
            ad.parse::<usize>(),
        ) else {
            continue;
        };
        let key = ((*tu).to_string(), rva, (*sym).to_string());
        let slot = by.entry(key).or_insert(((0, 0, 0), Vec::new()));
        if pad == 0 {
            slot.0 = (d, e, ad);
        } else {
            slot.1.push((pad, d, e, ad));
        }
    }
    by.into_iter()
        .map(|((tu, rva, sym), (base, curve))| {
            labelsafe::Tied::of(&tu, rva, &sym, base, &curve, swept_pads)
        })
        .collect()
}

fn safe(config: Option<&Path>, a: &SafeArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    let (pr, s) = swept(config, &a.sweep, guard)?;
    warn(&s.errors);
    let pads = labelsweep::pads_of(a.sweep.step, &a.sweep.pad);
    let tied = tied_of(&s.tsv, &pads);
    let flat: Vec<String> = {
        let moved: BTreeSet<&str> = tied.iter().map(|t| t.tu.as_str()).collect();
        chosen(&pr.units, &a.sweep.common.tu)?
            .iter()
            .map(|u| u.stem.clone())
            .filter(|t| !moved.contains(t.as_str()))
            .collect()
    };
    let at: BTreeMap<String, usize> = tied.iter().map(|t| (t.tu.clone(), a.sweep.at)).collect();
    let table = labelsafe::table(&tied, &flat, &at);
    srcio::own_write(&a.out, &table.tsv).with_context(|| format!("writing {}", a.out.display()))?;
    say(&table.lines);
    println!(
        "{} TU(s) swept at {} pad(s): {} tied function(s), {} flat; wrote {}",
        pr.units.len(),
        pads.len(),
        tied.len(),
        flat.len(),
        a.out.display()
    );
    if let Some(p) = &a.labels {
        srcio::own_write(p, labelsafe::labels_tsv(&s.labels))
            .with_context(|| format!("writing {}", p.display()))?;
        println!(
            "  wrote {} function label(s) at pad 0 to {}",
            s.labels.len(),
            p.display()
        );
    }
    if !a.delta.is_empty() {
        let deltas: Vec<labelsafe::Delta> = a
            .delta
            .iter()
            .map(|d| labelsafe::parse_delta(d, &s.labels))
            .collect::<Result<_>>()?;
        let swept_tus: BTreeSet<String> = tied.iter().map(|t| t.tu.clone()).collect();
        say(&labelsafe::search(
            &deltas, &tied, &swept_tus, a.max, a.top,
        )?);
    }
    json(
        &a.sweep.common.json,
        "hdr safe",
        &pr.label,
        None::<&()>,
        &table.lines,
    )?;
    Ok(Outcome::Ok)
}

fn atom_trees(atoms: &[hdrcombo::Atom], given: &[String]) -> Vec<String> {
    if !given.is_empty() {
        return given.to_vec();
    }
    let mut out: Vec<String> = Vec::new();
    for a in atoms {
        for f in &a.files {
            let lead = f
                .path
                .split(['/', '\\'])
                .next()
                .unwrap_or(&f.path)
                .to_string();
            if !lead.is_empty() && !out.contains(&lead) {
                out.push(lead);
            }
        }
    }
    out
}

fn project_root(pr: &Project) -> Result<PathBuf> {
    let d = pr.p.images.get(pr.n).context("no such image")?;
    let r = d
        .project
        .as_ref()
        .context("the image declares no project: name the tree with --root")?;
    let raw = r
        .dsw
        .as_ref()
        .or(r.dsp.as_ref())
        .context("the image's project names neither a .dsw nor a .dsp")?;
    let key = format!("{}.project", pr.p.image_key(pr.n));
    let path = pr.p.path_at(&key, raw)?;
    path.parent()
        .map(Path::to_path_buf)
        .with_context(|| format!("{}: no directory", path.display()))
}

fn combo(config: Option<&Path>, a: &ComboArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    let pr = units_of(config, &a.common.image)?;
    let root = match &a.root {
        Some(r) => r.clone(),
        None => project_root(&pr)?,
    };
    let roots: Vec<String> = vec![root.display().to_string()];
    let atoms = hdrcombo::read_atoms(&a.atoms, &roots)?;
    if !a.apply.is_empty() {
        say(&hdrcombo::apply(&root, &atoms, &a.apply)?);
        return Ok(Outcome::Ok);
    }
    let units = chosen(&pr.units, &a.common.tu)?;
    guard()?;
    let c = Compiler::new(&pr.p)?;
    let env = Env::load(&pr.p, pr.n)?;
    let work = work_dir(a.common.work.as_deref());
    let trees = atom_trees(&atoms, &a.tree);
    if trees.is_empty() {
        bail!("the atoms name no directory to copy: pass --tree DIR");
    }
    let s = hdrcombo::search(
        &c,
        &env,
        &units,
        &atoms,
        &hdrcombo::Opts {
            label: &pr.label,
            root: &root,
            trees: &trees,
            max: a.max,
            require: &a.require,
            near: a.near,
            jobs: jobs(a.common.jobs),
            work: &work,
        },
    )?;
    warn(&s.notes);
    say(&s.lines);
    if let Some(p) = &a.out {
        srcio::own_write(p, &s.tsv).with_context(|| format!("writing {}", p.display()))?;
    }
    json(
        &a.common.json,
        "hdr combo",
        &pr.label,
        None::<&()>,
        &s.lines,
    )?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Args)]
pub struct FloatsArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id whose reference the shapes are read from."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "DIR",
        help = "Our objects, joined to the reference's uses per function: a disagreeing spelling is named. \
                Repeatable."
    )]
    pub objs: Vec<PathBuf>,
    #[arg(
        long,
        help = "Print every datum, not only the conflicts and the cross-TU reads."
    )]
    pub all: bool,
    #[arg(
        long,
        help = "Read every body as /Od code: C2 keeps source operand order and never folds a division there, so \
                only the move, negate and store rows discriminate. Default: the image's own flags."
    )]
    pub od: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

fn float_functions<'a>(
    records: impl Iterator<Item = (u32, Option<&'a str>, bool)>,
) -> Vec<(u32, String)> {
    records
        .filter(|(_, _, thunk)| !thunk)
        .map(|(rva, name, _)| {
            (
                rva,
                name.map_or_else(|| format!("{rva:#08x}"), str::to_string),
            )
        })
        .collect()
}

pub fn floats(config: Option<&Path>, a: &FloatsArgs) -> Result<Outcome> {
    let (p, n, env) = crate::diff::env_of(config, &a.image)?;
    let d = p.images.get(n).context("no such image")?;
    let od = a.od
        || d.tu.iter().any(|t| t.cflags.iter().any(|f| f == "/Od"))
        || msvc6_project::spec::resolve(&p, n)?
            .specs
            .iter()
            .any(|s| s.cflags.iter().any(|f| f == "/Od"));
    let r = p.reference(n)?;
    let index = msvc6_project::functions::index_of(&p, n, &r)?;
    let functions = float_functions(index.iter().map(|f| (f.rva, f.name.as_deref(), f.thunk)));
    if functions.is_empty() {
        bail!("{}: the function index holds no function", env.label);
    }
    let uses = floatclass::reference_uses(&floatclass::Reference {
        label: &env.label,
        img: &env.img,
        functions: &functions,
        od,
    })?;
    let ours = if a.objs.is_empty() {
        None
    } else {
        let mut objs = Vec::new();
        for path in msvc6_diff::objs::build_list(&a.objs, true)? {
            let Ok(cs) = msvc6_diff::comdat::text(&path) else {
                continue;
            };
            objs.push((msvc6_diff::seat::scope_stem(&path), cs));
        }
        Some(floatclass::our_uses(&objs))
    };
    let rep = floatclass::report(&env.label, &env.img, &uses, ours.as_ref(), a.all);
    say(&rep.lines);
    json(&a.json, "floats", &env.label, None::<&()>, &uses)?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Args)]
pub struct TuFlagsArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id whose carve rows the candidates are checked against."
    )]
    pub image: String,
    #[arg(
        value_name = "DIR",
        help = "The directory of candidate translation units, each stating its members as per-COMDAT rows."
    )]
    pub dir: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "The rvas a recovered body is recorded at, one per line (0x-hexadecimal, a tab and anything after it \
                ignored, so a names TSV serves). A candidate rva this does not hold is UNKNOWN and refutes nothing."
    )]
    pub bodies: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "The rvas the image ATTESTS the switch at, one per line: the shipped exception-handling funclet and \
                unwind record of a /GX body. Its absence proves nothing, so this list only ever adds a CERTAIN."
    )]
    pub attested: Option<PathBuf>,
    #[arg(
        long,
        value_name = "SWITCH",
        default_value = "/GX",
        help = "The per-TU switch whose transition refutes a grouping."
    )]
    pub switch: String,
    #[arg(
        long,
        value_name = "FLAG",
        help = "The compile flags every recorded body is judged by: --bodies lists rvas only, so no body carries flags of its own. Repeatable."
    )]
    pub default: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

fn rva_list(p: &Path, base: u32) -> Result<BTreeSet<u32>> {
    let text = bounded::tool(p, MAX_SOURCE).with_context(|| p.display().to_string())?;
    let out: BTreeSet<u32> = text
        .lines()
        .filter_map(|l| {
            let first = l.split(['\t', ',']).next()?.trim();
            let hex = first
                .strip_prefix("0x")
                .or_else(|| first.strip_prefix("0X"))?;
            let v = u32::from_str_radix(hex, 16).ok()?;
            Some(if base > 0 && v >= base { v - base } else { v })
        })
        .collect();
    if out.is_empty() {
        bail!(
            "{}: no 0x-hexadecimal address in its first column",
            p.display()
        );
    }
    Ok(out)
}

fn carve_rows(
    bodies: &Path,
    attested: Option<&Path>,
    base: u32,
) -> Result<BTreeMap<u32, tuflags::Carve>> {
    let eh = attested
        .map(|p| rva_list(p, base))
        .transpose()?
        .unwrap_or_default();
    Ok(rva_list(bodies, base)?
        .into_iter()
        .map(|rva| {
            (
                rva,
                tuflags::Carve {
                    eh_data: eh.contains(&rva),
                    cflags: None,
                },
            )
        })
        .collect())
}

pub fn tu_flags(config: Option<&Path>, a: &TuFlagsArgs) -> Result<Outcome> {
    let r = crate::project::resolve(crate::project::load(config)?.as_ref(), &a.image)?;
    let label = r.label.clone();
    let base = msvc6_image::pe::Image::parse(r.bytes)
        .with_context(|| r.path.display().to_string())?
        .opt
        .image_base
        .get();
    let carves = carve_rows(&a.bodies, a.attested.as_deref(), base)?;
    let mut files: Vec<PathBuf> = std::fs::read_dir(&a.dir)
        .with_context(|| format!("reading {}", a.dir.display()))?
        .flatten()
        .map(|e| e.path())
        .filter(|p| {
            p.extension()
                .and_then(|x| x.to_str())
                .is_some_and(|e| matches!(e, "c" | "cpp"))
        })
        .collect();
    files.sort();
    if files.is_empty() {
        bail!("{}: no candidate `.c' or `.cpp' to screen", a.dir.display());
    }
    let sources: Vec<(String, String)> = files
        .iter()
        .filter_map(|p| {
            let name = p.file_name()?.to_string_lossy().into_owned();
            Some((name, bounded::source(p, MAX_SOURCE).ok()?))
        })
        .collect();
    let cands: Vec<tuflags::Candidate<'_>> = sources
        .iter()
        .map(|(name, text)| tuflags::Candidate {
            name,
            rvas: tuflags::member_rvas(text),
        })
        .collect();
    let (v, lines) = tuflags::report(&label, &a.switch, &cands, &carves, &a.default);
    say(&lines);
    json(&a.json, "tu-flags", &label, Some(&v), &lines)?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Args)]
pub struct TuBudgetArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id whose Rich header is the reference count."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "FILE",
        help = "The library table whose placed members are the floor on the library term (lib locate --write)."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "An archive whose whole bucket is the ceiling on the library term. Repeatable; the default is the \
                image's configured `libraries'."
    )]
    pub archive: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "IMG",
        help = "Our linked image: its own Rich census against the reference's, which is a different question from \
                the source band and can only be read against another Rich count."
    )]
    pub ours: Option<PathBuf>,
    #[arg(
        long,
        value_name = "IMAGE",
        help = "A second [[image]] whose own object counts are known without this screen, printed as the control; \
                its known counts are --known."
    )]
    pub control: Option<String>,
    #[arg(
        long,
        value_name = "N",
        help = "The control's known own object count for C, C++ and MASM, in that order. Repeatable."
    )]
    pub known: Vec<u32>,
    #[command(flatten)]
    pub json: JsonArg,
}

struct Terms {
    located: BTreeMap<u32, (u32, Vec<(String, u32)>)>,
    ceiling: BTreeMap<u32, u32>,
    vendored: BTreeMap<u32, u32>,
}

fn terms_of(p: &Config, n: usize, table: Option<&Path>, archives: &[PathBuf]) -> Result<Terms> {
    let mut t = Terms {
        located: BTreeMap::new(),
        ceiling: BTreeMap::new(),
        vendored: BTreeMap::new(),
    };
    let stem = |m: &str| msvc6_link::memberorder::member_stem(m);
    let signals: Vec<(&str, BTreeSet<String>)> = match table {
        Some(path) => {
            let t = msvc6_match::table::Table::read(path)?;
            vec![
                ("member", t.member.iter().map(|m| stem(&m.member)).collect()),
                ("byname", t.byname.iter().map(|m| stem(&m.member)).collect()),
                ("import", t.import.iter().map(|m| stem(&m.member)).collect()),
            ]
        }
        None => Vec::new(),
    };
    let mut seen: BTreeMap<u32, BTreeSet<String>> = BTreeMap::new();
    for path in archives {
        let d = bounded::read(path, crate::growth::MAX_ARCHIVE)
            .with_context(|| path.display().to_string())?;
        let a = msvc6_image::archive::Archive::parse(&d)?;
        for row in msvc6_image::census::members(&a)? {
            let Some(prodid) = row.prodid else { continue };
            *t.ceiling.entry(prodid).or_insert(0) += 1;
            let name = stem(&row.member);
            let e = t.located.entry(prodid).or_insert((0, Vec::new()));
            if e.1.is_empty() {
                e.1 = signals.iter().map(|(k, _)| ((*k).to_string(), 0)).collect();
            }
            let mut hit = false;
            for ((_, n), (_, set)) in e.1.iter_mut().zip(&signals) {
                if set.contains(&name) {
                    *n += 1;
                    hit = true;
                }
            }
            if hit && seen.entry(prodid).or_default().insert(name) {
                e.0 += 1;
            }
        }
    }
    let d = p.images.get(n).context("no such image")?;
    for v in &d.vendored {
        for s in &v.sources {
            if let Some(b) = tubudget::bucket_of_source(s) {
                *t.vendored.entry(b).or_insert(0) += 1;
            }
        }
    }
    Ok(t)
}

fn measured(
    p: &Config,
    n: usize,
    table: Option<&Path>,
    archives: &[PathBuf],
) -> Result<tubudget::Measured> {
    let r = p.reference(n)?;
    let img = msvc6_image::pe::Image::parse(r.bytes.clone())?;
    let rich = msvc6_image::rich::decode(img.bytes())
        .with_context(|| format!("{}: no Rich header, so it has no census to read", r.label))?;
    let t = terms_of(p, n, table, archives)?;
    Ok(tubudget::measure(&tubudget::Counts {
        rich: &rich,
        located: &|b| t.located.get(&b).cloned().unwrap_or((0, Vec::new())),
        ceiling: &|b| t.ceiling.get(&b).copied().unwrap_or(0),
        vendored: &|b| t.vendored.get(&b).copied().unwrap_or(0),
    }))
}

pub fn tu_budget(config: Option<&Path>, a: &TuBudgetArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    let key = format!("{}.libraries", p.image_key(n));
    let archives: Vec<PathBuf> = if a.archive.is_empty() {
        d.libraries
            .iter()
            .filter_map(|l| p.path_at(&key, l).ok())
            .collect()
    } else {
        a.archive.clone()
    };
    if archives.is_empty() {
        bail!("name an archive: --archive FILE, or the image's `libraries' in the config");
    }
    let m = measured(&p, n, a.library.as_deref(), &archives)?;
    let specs = msvc6_project::spec::resolve(&p, n)?;
    let sources: Vec<String> = specs.specs.iter().map(|s| s.source.clone()).collect();
    let linked = match &a.ours {
        Some(path) => {
            let img = msvc6_image::pe::Image::parse(
                bounded::read(path, crate::growth::MAX_IMAGE)
                    .with_context(|| path.display().to_string())?,
            )?;
            let rich = msvc6_image::rich::decode(img.bytes())
                .with_context(|| format!("{}: no Rich header", path.display()))?;
            Some((path.display().to_string(), tubudget::linked_counts(&rich)))
        }
        None => None,
    };
    let ours = tubudget::Ours {
        sources: tubudget::source_counts(&sources),
        linked,
    };
    let control = match &a.control {
        Some(id) => {
            let (cn, cd) = msvc6_project::registry::resolve_id(&p, id)?;
            let ckey = format!("{}.libraries", p.image_key(cn));
            let carch: Vec<PathBuf> = cd
                .libraries
                .iter()
                .filter_map(|l| p.path_at(&ckey, l).ok())
                .collect();
            if a.known.len() != tubudget::BUCKETS.len() {
                bail!(
                    "--control needs --known once per bucket ({}): C, C++ and MASM",
                    tubudget::BUCKETS.len()
                );
            }
            Some(tubudget::Control {
                label: cd.id.clone(),
                known: a.known.clone(),
                rows: measured(&p, cn, a.library.as_deref(), &carch)?.rows,
            })
        }
        None => None,
    };
    let lines = tubudget::report(&d.id, &m, &ours, control.as_ref());
    say(&lines);
    json(&a.json, "tu-budget", &d.id, None::<&()>, &m.rows)?;
    Ok(Outcome::Ok)
}

#[cfg(test)]
mod tests {
    use super::float_functions;

    #[test]
    fn a_function_the_index_holds_without_a_name_still_ends_the_body_before_it() {
        let got = float_functions(
            [
                (0x10, Some("a"), false),
                (0x20, None, false),
                (0x30, Some("t"), true),
            ]
            .into_iter(),
        );
        assert_eq!(
            got,
            [(0x10, "a".to_string()), (0x20, "0x000020".to_string())]
        );
    }
}
