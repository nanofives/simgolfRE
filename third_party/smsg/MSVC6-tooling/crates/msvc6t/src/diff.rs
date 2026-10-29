use crate::Outcome;
use crate::cli;
use crate::image::json;
use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::decisions;
use msvc6_diff::env::Env;
use msvc6_diff::objs;
use msvc6_diff::seat;
use msvc6_diff::segasm;
use msvc6_diff::segments::{self, Verdict};
use msvc6_diff::segrank;
use msvc6_project::config::Project;
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub fn env_of(config: Option<&Path>, image: &str) -> Result<(Project, usize, Env)> {
    let p = Project::require(config)?;
    let (n, _) = msvc6_project::registry::resolve_id(&p, image)?;
    let env = Env::load(&p, n)?;
    Ok((p, n, env))
}

#[must_use]
pub fn jobs(j: Option<usize>) -> usize {
    j.unwrap_or_else(msvc6_diff::par::default_jobs)
        .clamp(1, msvc6_diff::par::MAX_JOBS)
}

pub fn object_list(dirs: &[PathBuf], objs: &[PathBuf], libs: bool) -> Result<Vec<PathBuf>> {
    let mut list = objs::build_list(dirs, libs)?;
    list.extend(objs.iter().cloned());
    if list.is_empty() {
        bail!("no objects: pass --objs DIR or --obj FILE");
    }
    Ok(list)
}

#[derive(Debug, Serialize)]
struct SegRow<'a> {
    seg: usize,
    img: (u64, u64),
    ours: (usize, usize),
    img_len: usize,
    ours_len: usize,
    verdict: &'a str,
    diff: usize,
    edit: usize,
    oversize: bool,
    at: &'a str,
    regmap: String,
}

pub fn segments(config: Option<&Path>, a: &cli::SegmentsArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let rva = env.rva_of(&a.func)?;
    let list = object_list(&a.objs, &a.obj, !a.no_libs)?;
    let (opath, ours) = seat::find(&env, &list, rva, a.symbol.as_deref())?;
    let an = segments::analyze_in(&env, rva, &opath, ours, a.max)?;
    let pick = a
        .asm
        .as_deref()
        .map(|spec| segasm::parse_pick(spec, an.segs.len()))
        .transpose()?;
    let va = |o: usize| an.start.saturating_add(o as u64);
    println!("{}", an.header);
    println!("seg\timg\tours\timg_len\tours_len\tverdict\tdiff\tedit\tcap\tat\tregmap");
    let mut rows = Vec::new();
    for (n, s) in an.segs.iter().enumerate() {
        let regmap = segasm::regmap_text(&an, s);
        if !a.shape || s.verdict == Verdict::Shape {
            println!(
                "{n}\t{:#x}..{:#x}\t+{:#x}..+{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{regmap}",
                va(s.img.0),
                va(s.img.1),
                s.ours.0,
                s.ours.1,
                s.img_len(),
                s.ours_len(),
                s.verdict.tag(),
                s.diff,
                s.edit,
                if s.oversize { "OVERSIZE" } else { "-" },
                s.at
            );
        }
        rows.push(SegRow {
            seg: n,
            img: (va(s.img.0), va(s.img.1)),
            ours: s.ours,
            img_len: s.img_len(),
            ours_len: s.ours_len(),
            verdict: s.verdict.tag(),
            diff: s.diff,
            edit: s.edit,
            oversize: s.oversize,
            at: &s.at,
            regmap,
        });
    }
    if let Some(pick) = &pick {
        let text = segasm::report(&env, &an, pick, &segasm::definers(&list));
        match &a.out {
            Some(p) => {
                srcio::own_write(p, &text).with_context(|| format!("writing {}", p.display()))?;
                println!(
                    "asm: {} line(s) written to {}",
                    text.lines().count(),
                    p.display()
                );
            }
            None => print!("{text}"),
        }
    }
    if a.batch {
        let units = segments::batch_units(&an.segs, a.max);
        println!("unit\timg\tsegs\tshape_bytes\tspan");
        for (n, u) in units.iter().enumerate() {
            println!(
                "{n}\t{:#x}..{:#x}\t{}\t{}\t{}",
                va(u.img.0),
                va(u.img.1),
                u.segs,
                u.shape_bytes,
                u.img.1.saturating_sub(u.img.0)
            );
        }
        println!(
            "batch: {} dispatch unit(s) of <= {} image bytes",
            units.len(),
            a.max
        );
    }
    let (mis, compared) = segments::addend_mismatches(&an.img, &an.ours, &an.segs);
    for m in &mis {
        let hexes = |v: &[u32]| {
            v.iter()
                .map(|r| format!("{r:#x}"))
                .collect::<Vec<_>>()
                .join(",")
        };
        let ip = |s: &segments::Side, i: usize| s.dec.get(i).map_or(0, |d| u64::from(d.ip));
        println!(
            "addend\t{:#x}\t+{:#x}\timage {}\tours {}",
            an.start.saturating_add(ip(&an.img, m.img_ix)),
            ip(&an.ours, m.ours_ix),
            hexes(&m.img),
            hexes(&m.ours)
        );
    }
    println!(
        "addends: {} of {compared} paired instruction(s) with data operands name another rva+addend than the image",
        mis.len()
    );
    println!("{}", segments::summary(&an.segs, a.max));
    json(&a.json, "segments", &env.label, None::<&()>, &rows)?;
    let (edit, _) = segments::edit_totals(&an.segs);
    Ok(
        if edit == 0 && an.segs.iter().all(|s| s.verdict == Verdict::Exact) {
            Outcome::Ok
        } else {
            Outcome::Differs
        },
    )
}

#[derive(Debug, Serialize)]
struct RankSummary {
    asked: usize,
    functions: usize,
    differing: usize,
    edit: usize,
    image: usize,
    raw: usize,
    unres: usize,
    refused: Vec<Refused>,
}

#[derive(Debug, Serialize)]
struct Refused {
    object: String,
    error: String,
}

pub fn segrank(config: Option<&Path>, a: &cli::SegrankArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let list = object_list(&a.objs, &[], !a.no_libs)?;
    let jobs = jobs(a.jobs);
    let map = seat::seat_map(&env, &list, jobs);
    for (o, e) in &map.refused {
        eprintln!("segrank: REFUSED {}: {e}", o.display());
    }
    let seats = &map.seats;
    let rvas: Vec<u32> = match &a.rvas {
        Some(f) => segrank::parse_rvas(
            &srcio::own_read(f).with_context(|| format!("reading {}", f.display()))?,
        ),
        None if a.all_differing => {
            msvc6_diff::memtable::differing(&msvc6_diff::memtable::score_objects(&env, &list, jobs))
        }
        None => seats.keys().copied().collect(),
    };
    let r = segrank::rank(&env, &list, seats, &rvas, jobs);
    for (rva, e) in &r.errors {
        eprintln!("segrank: {rva:#x}: {e}");
    }
    let tsv = segrank::tsv(&r.rows);
    match &a.out {
        Some(p) => {
            srcio::own_write(p, &tsv).with_context(|| format!("writing {}", p.display()))?;
            println!("wrote {} row(s) to {}", r.rows.len(), p.display());
        }
        None => print!("{tsv}"),
    }
    if let Some(p) = &a.decisions {
        let (mut all, errors) = segrank::decisions(&env, seats, &r.rows, jobs);
        for (rva, e) in &errors {
            eprintln!("decisions: {rva:#x}: {e}");
        }
        if let Some(u) = &a.upstream {
            let text = srcio::own_read(u).with_context(|| format!("reading {}", u.display()))?;
            decisions::apply_upstream(&mut all, &decisions::traced_upstream(&text));
        }
        let names: BTreeMap<u32, &str> = r.rows.iter().map(|x| (x.rva, x.name.as_str())).collect();
        let t = decisions::tsv(&all, &|rva| {
            names.get(&rva).copied().unwrap_or("?").to_string()
        });
        srcio::own_write(p, &t).with_context(|| format!("writing {}", p.display()))?;
        for l in decisions::summary_lines(&all, &p.display().to_string()) {
            println!("{l}");
        }
    }
    if let Some(p) = &a.prev {
        let prev = segrank::parse_prev(
            &srcio::own_read(p).with_context(|| format!("reading {}", p.display()))?,
        );
        for l in segrank::deltas(&r.rows, &prev) {
            println!("{l}");
        }
    }
    println!("{}", segrank::total_line(&r.rows, r.asked));
    println!("{}", segrank::raw_line(&r.rows));
    if !map.refused.is_empty() {
        println!("{}", segrank::refused_line(&map.refused, list.len()));
    }
    let sum = |f: fn(&segrank::Row) -> usize| r.rows.iter().map(f).sum::<usize>();
    let summary = RankSummary {
        asked: r.asked,
        functions: r.rows.len(),
        differing: sum(|x| x.differing),
        edit: sum(|x| x.edit),
        image: sum(|x| x.image),
        raw: sum(|x| x.raw),
        unres: sum(|x| x.unres),
        refused: map
            .refused
            .iter()
            .map(|(o, e)| Refused {
                object: o.display().to_string(),
                error: e.clone(),
            })
            .collect(),
    };
    json(&a.json, "segrank", &env.label, Some(&summary), &r.rows)?;
    if !map.refused.is_empty() {
        return Ok(Outcome::Exit(std::process::ExitCode::FAILURE));
    }
    Ok(
        if summary.edit == 0 && summary.raw == 0 && r.errors.is_empty() {
            Outcome::Ok
        } else {
            Outcome::Differs
        },
    )
}

pub struct Prepared {
    pub project: Project,
    pub n: usize,
    pub unit: msvc6_diff::compile::Unit,
    pub unmapped: Vec<String>,
}

pub fn prepare(
    config: Option<&Path>,
    image: &str,
    source: &Path,
    args_from: Option<&Path>,
    cflags: Option<&str>,
    includes: &[PathBuf],
) -> Result<Prepared> {
    let p = Project::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, image)?;
    let specs = msvc6_project::spec::resolve(&p, n)?;
    let tu = args_from.unwrap_or(source);
    let spec = msvc6_diff::compile::spec_for(&specs, &d.id, tu).with_context(|| {
        format!(
            "{} names no translation unit of image `{}` (by path, or by a file stem that names one TU); pass --args-from",
            tu.display(),
            d.id
        )
    })?;
    let default_tc = d.compiler.clone().unwrap_or_else(|| "vc6".to_string());
    let mut unit = msvc6_diff::compile::Unit::of_spec(spec, source, &default_tc)?;
    if let Some(c) = cflags {
        unit.cflags = msvc6_project::switches::normalize_line(c);
    }
    unit.includes.extend(includes.iter().cloned());
    let mut unmapped = Vec::new();
    if let Some(b) = &d.build {
        let (inc, left) = msvc6_diff::compile::build_include(&p.toolchain_config(), b)?;
        unit.include_env = Some(inc);
        unmapped = left;
    }
    Ok(Prepared {
        project: p,
        n,
        unit,
        unmapped,
    })
}

#[must_use]
pub fn work_dir(given: Option<&Path>, image: &str, stem: &str) -> PathBuf {
    given.map_or_else(
        || {
            std::env::temp_dir()
                .join("msvc6t-work")
                .join(image)
                .join(stem)
        },
        Path::to_path_buf,
    )
}

#[derive(Debug, Serialize)]
struct CompileRow {
    source: String,
    object: String,
    listing: Option<String>,
    toolchain: String,
    outcome: String,
    args: Vec<String>,
}

pub fn compile(
    config: Option<&Path>,
    a: &cli::CompileArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    guard()?;
    let pr = prepare(
        config,
        &a.image,
        &a.source,
        a.args_from.as_deref(),
        a.cflags.as_deref(),
        &a.include,
    )?;
    for u in &pr.unmapped {
        eprintln!("compile: the build's include {u} lies under no mount; left out of INCLUDE");
    }
    let id = pr
        .project
        .images
        .get(pr.n)
        .map(|d| d.id.clone())
        .unwrap_or_default();
    let dir = work_dir(a.out.as_deref(), &id, &pr.unit.stem);
    let c = msvc6_diff::compile::Compiler::new(&pr.project)?;
    let sess = c.runner.session()?;
    let out = c.compile(&sess, &pr.unit, &dir, a.listing)?;
    print!("{}", out.text);
    println!(
        "compile: {} -> {} ({}, toolchain {})",
        pr.unit.source.display(),
        out.obj.display(),
        out.outcome.label(),
        pr.unit.toolchain
    );
    if let Some(cod) = &out.cod {
        println!("compile: listing {}", cod.display());
    }
    let row = CompileRow {
        source: pr.unit.source.display().to_string(),
        object: out.obj.display().to_string(),
        listing: out.cod.as_ref().map(|c| c.display().to_string()),
        toolchain: pr.unit.toolchain.clone(),
        outcome: out.outcome.label().to_string(),
        args: out.args.clone(),
    };
    json(&a.json, "compile", &id, None::<&()>, &[row])?;
    Ok(Outcome::Ok)
}
