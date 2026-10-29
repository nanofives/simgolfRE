use crate::Outcome;
use crate::cli;
use crate::diff::{env_of, prepare, work_dir};
use crate::image::json;
use anyhow::{Context as _, Result, bail};
use msvc6_base::digest::md5_hex;
use msvc6_base::srcio;
use msvc6_diff::attrib::{self, Listing};
use msvc6_diff::comdat::{self, Comdat};
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_diff::env::Env;
use msvc6_diff::fastloop::{self, Measured, Oracle as _, Segments, Shape};
use msvc6_diff::iso::{self, Fate, Meta, Target, Verdict};
use msvc6_diff::seat;
use msvc6_diff::segments::{self, MAX_SEGMENT};
use msvc6_project::config::Project;
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::time::Instant;

pub const MAX_SOURCE: u64 = 64 << 20;
pub const LOOP_JOBS: usize = 4;

fn read_source(p: &Path) -> Result<Vec<u8>> {
    srcio::read_bounded(p, MAX_SOURCE).with_context(|| format!("reading {}", p.display()))
}

#[must_use]
pub fn iso_dir(given: Option<&Path>, image: &str, rva: u32) -> PathBuf {
    work_dir(given, image, &format!("iso-{rva:08x}"))
}

fn fresh(dir: &Path) -> Result<()> {
    if dir.exists() {
        std::fs::remove_dir_all(dir).with_context(|| format!("clearing {}", dir.display()))?;
    }
    std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))
}

fn with_tu_dir(mut u: Unit, tu: &Path) -> Unit {
    if let Some(d) = tu.parent().filter(|d| !d.as_os_str().is_empty()) {
        let canon = |p: &Path| p.canonicalize().ok();
        if !u.includes.iter().any(|i| canon(i) == canon(d)) {
            u.includes.push(d.to_path_buf());
        }
    }
    u
}

struct Built {
    obj: PathBuf,
    cod: Option<PathBuf>,
    outcome: String,
    secs: f64,
}

fn build(c: &Compiler, unit: &Unit, text: &[u8], dir: &Path) -> Result<Built> {
    fresh(dir)?;
    let staged = dir.join(&unit.file);
    srcio::write_bytes(&staged, text).with_context(|| format!("writing {}", staged.display()))?;
    let mut u = unit.clone();
    u.source = staged;
    let t0 = Instant::now();
    let sess = c.runner.session()?;
    let out = c.compile(&sess, &u, dir, true)?;
    Ok(Built {
        obj: out.obj,
        cod: out.cod,
        outcome: out.outcome.label().to_string(),
        secs: t0.elapsed().as_secs_f64(),
    })
}

struct Side {
    comdat: Comdat,
    label: Option<u64>,
}

fn named(obj: &Path, symbol: &str) -> Result<Comdat> {
    comdat::text(obj)?
        .into_iter()
        .find(|c| c.symbol.as_deref() == Some(symbol))
        .with_context(|| format!("{} defines no COMDAT `{symbol}`", obj.display()))
}

fn side_of(b: &Built, symbol: &str) -> Result<Side> {
    let listing = b.cod.as_deref().map(attrib::read).unwrap_or_default();
    Ok(Side {
        comdat: named(&b.obj, symbol)?,
        label: listing.first_label(symbol),
    })
}

fn find_tu(p: &Project, n: usize, id: &str, rva: u32, hint: Option<&str>) -> Result<PathBuf> {
    let Some(h) = hint else {
        bail!("{id}: the image's names give no name at {rva:#x}; pass --src TU (and --name NAME)");
    };
    let target = Target::of_name(h);
    let specs = msvc6_project::spec::resolve(p, n)?;
    let mut hits: Vec<PathBuf> = Vec::new();
    for s in specs.specs.iter().filter(|s| s.image == id) {
        let path = PathBuf::from(&s.path);
        let Ok(b) = read_source(&path) else {
            continue;
        };
        if !iso::defines(&b, &target).is_empty() && !hits.contains(&path) {
            hits.push(path);
        }
    }
    if hits.len() > 1 {
        bail!(
            "{} TUs of {id} define `{}` (the name at {rva:#x}): {}; pass --src",
            hits.len(),
            target.text(),
            hits.iter()
                .map(|p| p.display().to_string())
                .collect::<Vec<_>>()
                .join(", ")
        );
    }
    hits.into_iter().next().with_context(|| {
        format!(
            "no TU of {id} defines `{}` (the name at {rva:#x}); pass --src",
            target.text()
        )
    })
}

fn tu_of(p: &Project, n: usize, id: &str, file: &Path) -> Result<PathBuf> {
    let specs = msvc6_project::spec::resolve(p, n)?;
    msvc6_diff::compile::spec_for(&specs, id, file)
        .map(|s| PathBuf::from(&s.path))
        .with_context(|| {
            format!(
                "{} names no translation unit of image `{id}` (by path, or by a file stem that names one TU); \
                 pass --args-from",
                file.display()
            )
        })
}

fn pch(u: &Unit) -> bool {
    u.cflags.iter().any(|f| {
        let l = f.to_ascii_lowercase();
        l.starts_with("/yx") || l.starts_with("/yu") || l.starts_with("-yx") || l.starts_with("-yu")
    })
}

fn names_of(env: &Env) -> impl Fn(u32) -> Option<String> + Sync + '_ {
    move |r: u32| env.names.by_rva().get(&r).cloned()
}

fn edit_of(env: &Env, rva: u32, obj: &Path, symbol: &str) -> Result<(usize, usize)> {
    let a = Segments {
        env,
        rva,
        max: MAX_SEGMENT,
    }
    .analyze(obj, symbol)?;
    Ok((segments::edit_totals(&a.segs).0, a.segs.len()))
}

#[derive(Debug, Serialize)]
struct IsoRow {
    rva: u32,
    symbol: String,
    tu: String,
    iso: String,
    verdict: Verdict,
    pad: usize,
    len: (usize, usize),
    bytes: usize,
    first_byte: Option<usize>,
    relocs: usize,
    first_reloc: Option<String>,
    renumbered: usize,
    md5: (String, String),
    labels: (Option<u64>, Option<u64>),
    lines: (usize, usize),
    target_line: usize,
    helpers: Vec<String>,
    kept: Vec<String>,
    dropped: usize,
    edit: (Option<usize>, Option<usize>),
    suspects: Vec<String>,
}

fn label_text(l: Option<u64>) -> String {
    l.map_or_else(|| "?".to_string(), |x| format!("$L{x}"))
}

pub fn iso(
    config: Option<&Path>,
    a: &cli::IsoArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    guard()?;
    let started = Instant::now();
    let (p, n, env) = env_of(config, &a.image)?;
    let rva = env.rva_of(&a.func)?;
    let id = env.label.clone();
    let hint = a
        .name
        .clone()
        .or_else(|| env.names.by_rva().get(&rva).cloned());
    let tu = match &a.src {
        Some(s) if s.is_file() => s.clone(),
        Some(s) => tu_of(&p, n, &id, s)?,
        None => find_tu(&p, n, &id, rva, hint.as_deref())?,
    };
    let pr = prepare(config, &a.image, &tu, a.args_from.as_deref(), None, &[])?;
    for u in &pr.unmapped {
        eprintln!("iso: the build's include {u} lies under no mount; left out of INCLUDE");
    }
    let unit = with_tu_dir(pr.unit, &tu);
    let c = Compiler::new(&pr.project)?;
    let dir = iso_dir(a.out.as_deref(), &id, rva);
    std::fs::create_dir_all(&dir).with_context(|| format!("creating {}", dir.display()))?;
    let text = read_source(&tu)?;
    let full = build(&c, &unit, &text, &dir.join("tu"))?;
    let (_, seated) = seat::find(&env, std::slice::from_ref(&full.obj), rva, None)
        .with_context(|| format!("seating {rva:#x} in {}", full.obj.display()))?;
    let symbol = seated
        .symbol
        .clone()
        .with_context(|| format!("the COMDAT seated at {rva:#x} has no symbol"))?;
    let target = Target::of_symbol(&symbol)
        .or_else(|| hint.as_deref().map(Target::of_name))
        .with_context(|| format!("cannot read a source name out of `{symbol}`; pass --name"))?;
    let tu_side = side_of(&full, &symbol)?;
    let iso_path = dir.join(&unit.file);
    let reduce = |pad: usize| -> Result<iso::Reduced> {
        let r = iso::reduce(&text, &target, pad)
            .with_context(|| format!("{} (target `{symbol}`)", tu.display()))?;
        srcio::write_bytes(&iso_path, &r.text)
            .with_context(|| format!("writing {}", iso_path.display()))?;
        Ok(r)
    };
    let mut r = reduce(0)?;
    println!(
        "iso {id} {rva:#x} `{symbol}`: {} -> {} ({} of {} lines{}; target at TU line {}; {} helper(s) kept, {} \
         definition(s) dropped)",
        tu.display(),
        iso_path.display(),
        r.lines().saturating_sub(r.tail),
        iso::count_lines(&text),
        if r.tail == 0 {
            String::new()
        } else {
            format!(
                ", then {} blank line(s) to its last code line's TU number, for CL's listing",
                r.tail
            )
        },
        r.target_line,
        r.count(Fate::Helper),
        r.count(Fate::Dropped)
    );
    let helpers = r.names(Fate::Helper);
    if !helpers.is_empty() {
        println!("  helpers kept: {}", helpers.join(", "));
    }
    let kept = r.names(Fate::Kept);
    if !kept.is_empty() {
        println!(
            "  kept whole (a conditional group crosses them): {}",
            kept.join(", ")
        );
    }
    let iso_compile = |text: &[u8]| {
        build(&c, &unit, text, &dir.join("iso")).with_context(|| {
            format!(
                "the iso {} does not compile: measure this function with `msvc6t loop --whole`",
                iso_path.display()
            )
        })
    };
    let mut built = iso_compile(&r.text)?;
    let mut mine = side_of(&built, &symbol)?;
    println!(
        "  compile: TU {:.1} s ({}), iso {:.1} s ({})",
        full.secs, full.outcome, built.secs, built.outcome
    );
    let mut pad = 0usize;
    for _ in 0..iso::PAD_TRIES {
        let (Some(x), Some(y)) = (tu_side.label, mine.label) else {
            break;
        };
        let Some(next) = iso::pad_step(x, y, pad) else {
            break;
        };
        if let Some(why) = &r.pad_refused {
            println!("  counter pad refused: {why}");
            break;
        }
        let before = iso::prove(&tu_side.comdat, &mine.comdat).bytes;
        pad = next;
        r = reduce(pad)?;
        if let Some(why) = &r.pad_refused {
            println!("  counter pad refused: {why}");
            break;
        }
        built = iso_compile(&r.text)?;
        mine = side_of(&built, &symbol)?;
        println!(
            "  counter pad {pad}: TU $L{x}, iso $L{y} -> {} ({before} -> {} differing byte(s))",
            label_text(mine.label),
            iso::prove(&tu_side.comdat, &mine.comdat).bytes
        );
    }
    let proof = iso::prove(&tu_side.comdat, &mine.comdat);
    let labels = (tu_side.label, mine.label);
    match labels {
        (Some(x), Some(y)) => println!(
            "  C1 label counter at the body: TU $L{x} (mod {m} = {}), iso $L{y} (mod {m} = {}){}",
            x % iso::LABEL_MODULUS,
            y % iso::LABEL_MODULUS,
            if x % iso::LABEL_MODULUS == y % iso::LABEL_MODULUS {
                ""
            } else {
                " -- MOVED"
            },
            m = iso::LABEL_MODULUS
        ),
        _ => println!("  C1 label counter: no `$L` label in a listing"),
    }
    let faithful = proof.faithful();
    let suspects = if faithful {
        Vec::new()
    } else {
        iso::suspects(&proof, labels, pch(&unit))
    };
    if faithful {
        println!(
            "  PROOF: FAITHFUL {:#x} bytes md5 {}{}",
            proof.len.0,
            proof.md5.0,
            if proof.renumbered.is_empty() {
                String::new()
            } else {
                format!(
                    ", {} relocation target(s) renumbered only",
                    proof.renumbered.len()
                )
            }
        );
    } else {
        println!(
            "  PROOF: DIFFERS: length {:#x} vs {:#x}, {} byte(s) differ{}, {} relocation(s) differ{}",
            proof.len.0,
            proof.len.1,
            proof.bytes,
            proof
                .first_byte
                .map_or_else(String::new, |b| format!(" (first at +{b:#x})")),
            proof.relocs,
            proof
                .first_reloc
                .as_ref()
                .map_or_else(String::new, |f| format!(" (first: {f})"))
        );
        for s in &suspects {
            println!("  suspect: {s}");
        }
    }
    let scores = (
        edit_of(&env, rva, &full.obj, &symbol),
        edit_of(&env, rva, &built.obj, &symbol),
    );
    match &scores {
        (Ok((ea, na)), Ok((eb, nb))) => println!(
            "  SCORE: TU edit {ea} over {na} segment(s), iso edit {eb} over {nb}{}",
            if ea == eb {
                ""
            } else {
                " -- THE EDIT FIGURES DIFFER: loop measures the iso, not the TU"
            }
        ),
        (x, y) => println!(
            "  SCORE: not comparable ({})",
            x.as_ref()
                .err()
                .or(y.as_ref().err())
                .map(|e| format!("{e:#}"))
                .unwrap_or_default()
        ),
    }
    let verdict = if faithful {
        Verdict::Faithful
    } else {
        Verdict::Differs
    };
    let canon = |p: &Path| p.canonicalize().ok();
    let args = a.args_from.clone().filter(|f| canon(f) != canon(&tu));
    let meta = Meta {
        format: iso::META_FORMAT,
        image: id.clone(),
        rva,
        tu: tu.canonicalize().unwrap_or_else(|_| tu.clone()),
        args: args.map(|f| f.canonicalize().unwrap_or(f)),
        iso: iso_path.clone(),
        symbol: symbol.clone(),
        verdict,
        pad,
    };
    let saved = meta.save(&dir)?;
    println!(
        "  objects: {} {}; proof {} ({:.1} s)",
        full.obj.display(),
        built.obj.display(),
        saved.display(),
        started.elapsed().as_secs_f64()
    );
    if faithful {
        println!(
            "  next: msvc6t loop {id} {rva:#x} {} [VARIANT...]",
            iso_path.display()
        );
    } else {
        println!(
            "  the iso is NOT faithful to {}: measure this function with `msvc6t loop --whole`",
            tu.display()
        );
    }
    let row = IsoRow {
        rva,
        symbol,
        tu: tu.display().to_string(),
        iso: iso_path.display().to_string(),
        verdict,
        pad,
        len: proof.len,
        bytes: proof.bytes,
        first_byte: proof.first_byte,
        relocs: proof.relocs,
        first_reloc: proof.first_reloc.clone(),
        renumbered: proof.renumbered.len(),
        md5: proof.md5.clone(),
        labels,
        lines: (r.lines(), iso::count_lines(&text)),
        target_line: r.target_line,
        helpers,
        kept,
        dropped: r.count(Fate::Dropped),
        edit: (
            scores.0.as_ref().ok().map(|s| s.0),
            scores.1.as_ref().ok().map(|s| s.0),
        ),
        suspects,
    };
    json(&a.json, "iso", &id, None::<&()>, &[row])?;
    Ok(if faithful {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[derive(Debug, Serialize)]
struct LoopRow {
    index: usize,
    file: String,
    md5: String,
    edit: Option<usize>,
    vs_base: Option<i64>,
    vs_last_run: Option<i64>,
    differing: String,
    moved: String,
    secs: f64,
    error: Option<String>,
    best: bool,
}

#[derive(Debug, Serialize)]
struct LoopSummary {
    symbol: String,
    mode: String,
    files: usize,
    best: usize,
    best_edit: Option<usize>,
    last_run: Option<usize>,
    runs: usize,
}

struct Proven {
    tu: PathBuf,
    symbol: String,
    verdict: Verdict,
    pad: usize,
    args: Option<PathBuf>,
}

fn locate(
    config: Option<&Path>,
    a: &cli::LoopArgs,
    env: &Env,
    rva: u32,
    proof_dir: &Path,
) -> Result<Proven> {
    let id = env.label.as_str();
    if let Some(m) = Meta::load(proof_dir).filter(|m| m.image == id && m.rva == rva) {
        return Ok(Proven {
            args: a.args_from.clone().or_else(|| m.args.clone()),
            tu: m.tu,
            symbol: m.symbol,
            verdict: m.verdict,
            pad: m.pad,
        });
    }
    let p = Project::require(config)?;
    let (n, _) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    let tu = tu_of(&p, n, id, a.args_from.as_deref().unwrap_or(&a.file))?;
    let pr = prepare(config, &a.image, &tu, a.args_from.as_deref(), None, &[])?;
    let unit = with_tu_dir(pr.unit, &tu);
    let c = Compiler::new(&pr.project)?;
    let dir = work_dir(None, id, "loop").join(format!("locate-{rva:08x}"));
    let b = build(&c, &unit, &read_source(&tu)?, &dir)?;
    let (_, seated) = seat::find(env, std::slice::from_ref(&b.obj), rva, None)
        .with_context(|| format!("seating {rva:#x} in {}", b.obj.display()))?;
    let symbol = seated
        .symbol
        .with_context(|| format!("the COMDAT seated at {rva:#x} has no symbol"))?;
    eprintln!(
        "note: no `msvc6t iso {id} {rva:#x}` proof in {}; the one-function reduction is UNPROVEN for this function \
         (run iso once: it compiles the TU beside the iso and compares the COMDAT)",
        proof_dir.display()
    );
    Ok(Proven {
        tu,
        symbol,
        verdict: Verdict::Unproven,
        pad: 0,
        args: a.args_from.clone(),
    })
}

fn now() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |d| d.as_secs())
}

struct Staged {
    index: usize,
    file: PathBuf,
    text: std::result::Result<Vec<u8>, String>,
}

struct Bench<'a> {
    c: &'a Compiler,
    unit: &'a Unit,
    work: &'a Path,
    last: &'a Path,
    oracle: &'a Segments<'a>,
    symbol: &'a str,
    namer: &'a fastloop::Namer<'a>,
}

fn measure(b: &Bench<'_>, st: &Staged) -> Measured {
    let Bench {
        c,
        unit,
        work,
        last,
        oracle,
        symbol,
        namer,
    } = *b;
    let t0 = Instant::now();
    let file = st.file.display().to_string();
    let text = match &st.text {
        Ok(t) => t,
        Err(e) => {
            return Measured {
                file,
                md5: String::new(),
                secs: 0.0,
                scored: Err(e.clone()),
            };
        }
    };
    let scored = (|| -> Result<fastloop::Scored> {
        let b = build(c, unit, text, &work.join(format!("v{}", st.index)))?;
        let keep = last.join(format!("v{}", st.index));
        std::fs::copy(&b.obj, keep.with_extension("obj"))
            .with_context(|| format!("keeping {}", b.obj.display()))?;
        let mut listing = match &b.cod {
            Some(cod) => {
                std::fs::copy(cod, keep.with_extension("cod"))
                    .with_context(|| format!("keeping {}", cod.display()))?;
                attrib::read(cod)
            }
            None => Listing::default(),
        };
        listing.retext(&iso::line_texts(text));
        let a = oracle.analyze(&b.obj, symbol)?;
        Ok(fastloop::score(&a, &listing, symbol, namer))
    })();
    Measured {
        file,
        md5: md5_hex(text),
        secs: t0.elapsed().as_secs_f64(),
        scored: scored.map_err(|e| format!("{e:#}")),
    }
}

pub fn run_loop(
    config: Option<&Path>,
    a: &cli::LoopArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    guard()?;
    let started = Instant::now();
    let (_, _, env) = env_of(config, &a.image)?;
    let rva = env.rva_of(&a.func)?;
    let id = env.label.clone();
    let proof_dir = iso_dir(a.iso.as_deref(), &id, rva);
    let pv = locate(config, a, &env, rva, &proof_dir)?;
    if pv.verdict == Verdict::Differs && !a.whole {
        bail!(
            "the iso of {id} {rva:#x} DIFFERS from its TU ({}): pass --whole to compile the files as whole TUs",
            proof_dir.join(iso::META).display()
        );
    }
    let target = Target::of_symbol(&pv.symbol);
    if target.is_none() && !a.whole {
        bail!(
            "cannot read a source name out of `{}` to reduce the files; pass --whole",
            pv.symbol
        );
    }
    let pr = prepare(config, &a.image, &pv.tu, pv.args.as_deref(), None, &[])?;
    let unit = with_tu_dir(pr.unit, &pv.tu);
    let c = Compiler::new(&pr.project)?;
    let base = work_dir(None, &id, "loop");
    let work = a
        .work
        .clone()
        .unwrap_or_else(|| base.join(format!("{rva:08x}")));
    let last = work.join("last");
    fresh(&last)?;
    let files: Vec<PathBuf> = std::iter::once(a.file.clone())
        .chain(a.variants.iter().cloned())
        .collect();
    let staged: Vec<Staged> = files
        .iter()
        .enumerate()
        .map(|(index, f)| {
            let text = read_source(f).and_then(|t| match (&target, a.whole) {
                (Some(tg), false) => iso::reduce(&t, tg, pv.pad)
                    .map(|r| r.text)
                    .with_context(|| format!("{} (target `{}`)", f.display(), pv.symbol)),
                _ => Ok(t),
            });
            Staged {
                index,
                file: f.clone(),
                text: text.map_err(|e| format!("{e:#}")),
            }
        })
        .collect();
    let jobs = a
        .jobs
        .unwrap_or(LOOP_JOBS)
        .clamp(1, msvc6_diff::par::MAX_JOBS)
        .min(files.len().max(1));
    let oracle = Segments {
        env: &env,
        rva,
        max: MAX_SEGMENT,
    };
    let namer = names_of(&env);
    let bench = Bench {
        c: &c,
        unit: &unit,
        work: &work,
        last: &last,
        oracle: &oracle,
        symbol: &pv.symbol,
        namer: &namer,
    };
    let history = match &a.history {
        Some(d) => d.join(format!("{id}-{rva:08x}.tsv")),
        None => base.join(format!("{rva:08x}.tsv")),
    };
    let old = read_history(&history)?;
    let measured: Vec<Measured> = msvc6_diff::par::map(&staged, jobs, |st| measure(&bench, st));
    let wall = started.elapsed().as_secs_f64();
    let prev = fastloop::previous(&fastloop::parse_history(&old));
    let ts = now();
    let run = format!("{}-{ts}", std::process::id());
    let mut add = old;
    for (k, m) in measured.iter().enumerate() {
        if let Ok(s) = &m.scored {
            add.push_str(
                &fastloop::Row {
                    time: ts,
                    run: run.clone(),
                    index: k,
                    file: m.file.clone(),
                    md5: m.md5.clone(),
                    edit: s.edit,
                    differing: fastloop::differing(&s.segs),
                }
                .line(),
            );
        }
    }
    if let Some(d) = history.parent() {
        std::fs::create_dir_all(d).with_context(|| format!("creating {}", d.display()))?;
    }
    srcio::own_write(&history, add).with_context(|| format!("writing {}", history.display()))?;
    let mode = if a.whole {
        "whole TU".to_string()
    } else {
        format!("iso ({})", pv.verdict.tag())
    };
    let slowest = measured.iter().map(|m| m.secs).fold(0.0f64, f64::max);
    println!(
        "loop {id} {rva:#x} `{}` {mode}: {} file(s), wall {wall:.1} s (slowest compile+score {slowest:.1} s, jobs {jobs})",
        pv.symbol,
        files.len()
    );
    let asm_path = last.join("asm.txt");
    let frontier_path = last.join("frontier.txt");
    let shape = Shape {
        frontier: a.frontier,
        asm_lines: a.asm_lines,
    };
    let (lines, best) = fastloop::report(
        &measured,
        &prev,
        ts,
        shape,
        (asm_path.as_path(), frontier_path.as_path()),
    );
    for l in &lines {
        println!("{l}");
    }
    let shown = measured
        .get(best)
        .and_then(|m| m.scored.as_ref().ok())
        .or_else(|| measured.first().and_then(|m| m.scored.as_ref().ok()));
    if let Some(s) = shown {
        srcio::own_write(&asm_path, &s.full)
            .with_context(|| format!("writing {}", asm_path.display()))?;
        srcio::own_write(&frontier_path, &s.frontier)
            .with_context(|| format!("writing {}", frontier_path.display()))?;
    }
    println!(
        "  history {}; objects {}",
        history.display(),
        last.display()
    );
    let base_scored = measured.first().and_then(|m| m.scored.as_ref().ok());
    let rows: Vec<LoopRow> = measured
        .iter()
        .enumerate()
        .map(|(k, m)| {
            let s = m.scored.as_ref().ok();
            let signed = |x: usize| i64::try_from(x).unwrap_or(i64::MAX);
            LoopRow {
                index: k,
                file: m.file.clone(),
                md5: m.md5.clone(),
                edit: s.map(|s| s.edit),
                vs_base: s
                    .zip(base_scored)
                    .map(|(s, b)| signed(s.edit).saturating_sub(signed(b.edit))),
                vs_last_run: s.and_then(|s| fastloop::delta_of(&prev, s.edit)),
                differing: s.map(|s| fastloop::differing(&s.segs)).unwrap_or_default(),
                moved: s
                    .zip(base_scored)
                    .map(|(s, b)| fastloop::moved(&b.segs, &s.segs))
                    .unwrap_or_default(),
                secs: m.secs,
                error: m.scored.as_ref().err().cloned(),
                best: k == best,
            }
        })
        .collect();
    let best_edit = measured.get(best).and_then(Measured::edit);
    let summary = LoopSummary {
        symbol: pv.symbol.clone(),
        mode,
        files: files.len(),
        best,
        best_edit,
        last_run: prev.base.map(|b| b.1),
        runs: prev.runs,
    };
    json(&a.json, "loop", &id, Some(&summary), &rows)?;
    if base_scored.is_none() {
        let e = measured
            .first()
            .and_then(|m| m.scored.as_ref().err())
            .cloned()
            .unwrap_or_default();
        bail!("{}: {e}", a.file.display());
    }
    Ok(if best_edit == Some(0) {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn read_history(history: &Path) -> Result<String> {
    match std::fs::symlink_metadata(history) {
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(String::new()),
        _ => srcio::own_read_bounded(history, fastloop::MAX_HISTORY).with_context(|| {
            format!(
                "reading the loop history {}: it is kept as it is; move it aside to start another",
                history.display()
            )
        }),
    }
}

#[cfg(test)]
mod history_tests {
    use super::*;

    #[test]
    fn a_history_past_its_cap_or_unreadable_is_refused_not_overwritten() {
        let d = std::env::temp_dir().join(format!("msvc6t-loop-history-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        let missing = d.join("none.tsv");
        assert_eq!(read_history(&missing).unwrap(), "");
        let ok = d.join("ok.tsv");
        srcio::own_write(&ok, "row\n").unwrap();
        assert_eq!(read_history(&ok).unwrap(), "row\n");
        let big = d.join("big.tsv");
        std::fs::File::create(&big)
            .unwrap()
            .set_len(fastloop::MAX_HISTORY + 1)
            .unwrap();
        let e = format!("{:#}", read_history(&big).unwrap_err());
        assert!(
            e.contains("move it aside") && e.contains("more than the"),
            "{e}"
        );
        let bad = d.join("bad.tsv");
        srcio::write_bytes(&bad, &[0xff, 0xfe, b'\n']).unwrap();
        assert!(read_history(&bad).is_err());
        std::fs::remove_dir_all(&d).unwrap();
    }
}
