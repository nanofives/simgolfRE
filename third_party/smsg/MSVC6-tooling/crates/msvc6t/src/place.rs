use crate::Outcome;
use crate::cli;
use crate::diff::env_of;
use crate::gate::objects;
use crate::image::json;
use anyhow::{Context as _, Result, anyhow};
use msvc6_base::srcio;

use msvc6_image::pe::Image;
use msvc6_match::{bodies, dataowner, dataruns, dataseat, partition, placement, seated};
use msvc6_project::config::Project;
use msvc6_project::mapfile::{Map, Owners};
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

const MAX_SOURCE: u64 = 64 << 20;

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

pub fn data(config: Option<&Path>, d: &cli::DataArgs) -> Result<Outcome> {
    match &d.verb {
        cli::DataVerb::Seat(a) => data_seat(config, a),
        cli::DataVerb::Runs(a) => data_runs(config, a),
        cli::DataVerb::Owner(a) => data_owner(config, a),
    }
}

pub fn place(config: Option<&Path>, p: &cli::PlaceArgs) -> Result<Outcome> {
    match &p.verb {
        cli::PlaceVerb::Map(a) => place_map(config, a),
        cli::PlaceVerb::Seated(a) => place_seated(config, a),
        cli::PlaceVerb::Partition(a) => place_partition(config, a),
        cli::PlaceVerb::Locate(a) => place_locate(config, a),
    }
}

#[derive(Debug, Serialize)]
struct SeatRow<'a> {
    object: String,
    #[serde(flatten)]
    section: &'a dataseat::Seated,
}

fn map_rvas(map: &Map) -> BTreeMap<String, u32> {
    let mut out = BTreeMap::new();
    for s in &map.symbols {
        out.entry(s.name.clone()).or_insert(s.rva);
    }
    out
}

fn data_seat(config: Option<&Path>, a: &cli::DataSeatArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let ours = a
        .map
        .as_deref()
        .map(Map::read)
        .transpose()?
        .map(|m| map_rvas(&m))
        .unwrap_or_default();
    let mut rows: Vec<(PathBuf, dataseat::Report)> = Vec::new();
    let mut refused = 0usize;
    for (o, r) in dataseat::seat_all(&env, &list, dataseat::CONTENT_BUDGET) {
        match r {
            Ok(r) => rows.push((o, r)),
            Err(e) => {
                eprintln!("data seat: REFUSED {}: {e:#}", o.display());
                refused += 1;
            }
        }
    }
    let json_rows: Vec<SeatRow<'_>> = rows
        .iter()
        .flat_map(|(o, r)| {
            r.sections.iter().map(move |s| SeatRow {
                object: msvc6_diff::seat::scope_stem(o),
                section: s,
            })
        })
        .collect();
    json(&a.json, "data seat", &env.label, None::<&()>, &json_rows)?;
    if let Some(name) = &a.layout {
        say(&dataseat::layout_lines(&rows, name, &ours, a.steps));
    } else if let [(o, r)] = rows.as_slice() {
        say(&dataseat::detail_lines(o, r));
    } else {
        for (o, r) in &rows {
            if a.detail
                .as_deref()
                .is_some_and(|d| msvc6_diff::seat::scope_stem(o) == d)
            {
                say(&dataseat::detail_lines(o, r));
            }
        }
        let mut rows = rows;
        say(&dataseat::ranking_lines(&mut rows));
    }
    Ok(if refused > 0 {
        Outcome::Exit(std::process::ExitCode::FAILURE)
    } else {
        Outcome::Ok
    })
}

fn data_runs(config: Option<&Path>, a: &cli::DataRunsArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let mut list = objects(&a.objs)?;
    list.sort();
    let map = a.map.as_deref().map(Map::read).transpose()?;
    let placer = msvc6_diff::resolve::Placer::new(
        &env.names,
        |s| a.notes_only.iter().any(|n| Path::new(s).ends_with(n)),
        map.as_ref(),
        env.text(),
        env.text_rva,
    );
    let place = |stem: &str, f: &str| placer.place(stem, f);
    let rep = dataruns::measure(&env, &list, &place, dataruns::CONTENT_BUDGET)?;
    say(&dataruns::lines(&env, &rep, a.list));
    json(&a.json, "data runs", &env.label, None::<&()>, &rep.points)?;
    Ok(Outcome::Ok)
}

fn data_owner(config: Option<&Path>, a: &cli::DataOwnerArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let map = Map::read(&a.map)?;
    let sp = dataowner::spans(&map);
    if sp.is_empty() {
        return Err(anyhow!(
            "{} publishes no data symbol owned by an object: `0 interruptions` would read as the invariant holding when it was never tested",
            a.map.display()
        ));
    }
    println!("DATA SPANS PER OBJECT, from {}", a.map.display());
    say(&dataowner::span_lines(&sp));
    if !a.objs.objs.is_empty() || !a.objs.obj.is_empty() {
        let list = objects(&a.objs)?;
        println!("  TRUE CONTRIBUTION SIZES from the objects' COFF headers");
        for o in &list {
            let name = o
                .file_name()
                .map_or_else(String::new, |f| f.to_string_lossy().into_owned());
            match msvc6_diff::comdat::read(o).and_then(|d| msvc6_gate::coffx::sizes(&d)) {
                Ok(s) if s.data_sections.is_empty() => {
                    println!("    {name:22} no non-`.text' section at all");
                }
                Ok(s) => {
                    let parts: Vec<String> = s
                        .data_sections
                        .iter()
                        .map(|(n, v)| format!("{n} {v}"))
                        .collect();
                    println!("    {name:22} {}", parts.join("  "));
                }
                Err(e) => println!("    {name:22} unread: {e:#}"),
            }
        }
    }
    let ints = dataowner::interruptions(&sp, &map, dataowner::PAIR_BUDGET);
    say(&dataowner::interruption_lines(&ints));
    if let Some(c) = &a.candidate {
        let cand = Image::load(c).with_context(|| format!("loading {}", c.display()))?;
        let syms = dataowner::data_symbols(&map);
        if syms.is_empty() {
            return Err(anyhow!("{} publishes no data symbol row", a.map.display()));
        }
        let att = dataowner::attribute(&env.img, &cand, &syms);
        say(&dataowner::divergence_lines(syms.len(), &att));
    }
    Ok(Outcome::Ok)
}

fn tu_sources(p: &Project, n: usize) -> Result<Vec<PathBuf>> {
    let specs = msvc6_project::spec::resolve(p, n)?;
    let mut v: Vec<PathBuf> = specs.specs.iter().map(|s| PathBuf::from(&s.path)).collect();
    v.extend(
        specs
            .specs
            .iter()
            .filter_map(|s| s.dsp.as_ref().map(PathBuf::from)),
    );
    v.sort();
    v.dedup();
    Ok(v)
}

fn newest(files: &[PathBuf]) -> Option<(SystemTime, String)> {
    files
        .iter()
        .filter_map(|f| {
            let m = f.metadata().and_then(|m| m.modified()).ok()?;
            Some((m, f.display().to_string()))
        })
        .max_by_key(|(t, _)| *t)
}

fn digest(files: &[PathBuf]) -> Option<String> {
    let mut all = Vec::new();
    for f in files {
        if let Ok(b) = msvc6_gate::libs::read_bounded(f, MAX_SOURCE) {
            all.extend_from_slice(f.file_name().unwrap_or_default().as_encoded_bytes());
            all.extend_from_slice(&b);
        }
    }
    (!all.is_empty()).then(|| msvc6_base::digest::md5_hex(&all).chars().take(12).collect())
}

#[derive(Debug, Serialize)]
struct PlaceRow<'a> {
    symbol: &'a str,
    reference: u32,
    ours: u32,
}

#[allow(clippy::too_many_lines)]
fn place_map(config: Option<&Path>, a: &cli::PlaceMapArgs) -> Result<Outcome> {
    let (p, n, env) = env_of(config, &a.image)?;
    let map = Map::read(&a.map)?;
    if placement::map_symbols(&map).is_empty() {
        return Err(anyhow!(
            "{} publishes no `0001:` symbol: not a LINK 6.00 map",
            a.map.display()
        ));
    }
    let owners = Owners::new(&a.own);
    println!(
        "{}: WHERE OUR LINK PUT EACH OWN FUNCTION, against the reference",
        env.label
    );
    println!("  PROVENANCE  map {}", a.map.display());
    let sources = tu_sources(&p, n).unwrap_or_default();
    match (
        std::fs::metadata(&a.map).and_then(|m| m.modified()).ok(),
        newest(&sources),
    ) {
        (Some(mt), Some((st, who))) if st > mt => {
            let mins = st.duration_since(mt).map_or(0, |d| d.as_secs() / 60);
            println!(
                "  *** THE MAP IS STALE: `{who}' is {mins} minute(s) NEWER than this map; reported, not refused"
            );
        }
        (Some(_), Some((_, who))) => {
            println!("  PROVENANCE  map is newer than every source ({who} is the newest)")
        }
        _ => println!("  PROVENANCE  UNKNOWN -- no mtimes could be read"),
    }
    if a.by_object {
        let (rows, unresolved) = placement::rows_by_object(&env, &map, &owners, a.self_control);
        let flat: Vec<(u32, String)> = rows.iter().map(|(r, o, _)| (*r, o.clone())).collect();
        let runs = placement::object_runs(&flat);
        let objects: std::collections::BTreeSet<&str> =
            rows.iter().map(|(_, o, _)| o.as_str()).collect();
        println!(
            "  .text ORDER BY OBJECT{} -- {unresolved} map name(s) have no reference rva and are absent",
            if a.self_control {
                " (SELF-CONTROL)"
            } else {
                ""
            }
        );
        if a.collapse {
            println!("  {:>9}  {:>5}  object", "first rva", "syms");
            for (rva, o, k) in &runs {
                println!("  {rva:#9x}  {k:>5}  {o}");
            }
        } else if !a.interleave {
            println!("  {:>9}  {:<28}  symbol", "rva", "object");
            for (r, o, s) in &rows {
                println!("  {r:#9x}  {o:<28}  {s}");
            }
        }
        let split = placement::split_objects(&runs);
        if a.interleave {
            println!(
                "  {} object(s) INTERLEAVED -- in more than one run:",
                split.len()
            );
            for (o, starts) in &split {
                let s: Vec<String> = starts.iter().map(|x| format!("{x:#x}")).collect();
                println!("    {o:<28} {:>3} run(s): {}", starts.len(), s.join(" "));
            }
        }
        println!(
            "  {} symbol(s), {} run(s) over {} object(s); {} object(s) split into more than one run.{}",
            rows.len(),
            runs.len(),
            objects.len(),
            split.len(),
            if a.self_control {
                if runs.len() == objects.len() {
                    "  SELF-CONTROL PASS: one run per object."
                } else {
                    "  SELF-CONTROL FAIL: our own link is not one run per object."
                }
            } else {
                ""
            }
        );
        return Ok(Outcome::Ok);
    }
    match digest(&sources) {
        Some(d) => println!("  PROVENANCE  source digest {d}"),
        None => println!("  PROVENANCE  source digest UNAVAILABLE"),
    }
    let ours_path = a.ours.clone().unwrap_or_else(|| {
        let ext = p
            .reference(n)
            .ok()
            .and_then(|r| r.path.extension().map(std::ffi::OsStr::to_os_string))
            .unwrap_or_default();
        a.map.with_extension(ext)
    });
    let ours = Image::load(&ours_path).ok();
    match ours.as_ref().and_then(|o| placement::lattice(&env, o)) {
        Some(l) => say(&placement::lattice_lines(&l)),
        None => println!(
            "  ILT LATTICE: not compared -- no readable image at {}",
            ours_path.display()
        ),
    }
    let pl = placement::resolve(&env, &map);
    println!(
        "  map published {} symbol(s); {} resolve to a reference rva, {} do not",
        pl.published,
        pl.rows.len(),
        pl.unresolved
    );
    println!(
        "  AT THE REFERENCE'S ADDRESS: {} of {}",
        pl.exact(),
        pl.rows.len()
    );
    if !pl.rows.is_empty() {
        println!(
            "  {:>9}  {:>9}  {:>9}  symbol",
            "reference", "ours", "delta"
        );
        let shown: Vec<&(String, u32, u32)> = if a.all {
            pl.rows.iter().collect()
        } else {
            pl.rows.iter().filter(|(_, r, o)| r != o).take(20).collect()
        };
        for (s, r, o) in shown {
            println!(
                "  {r:#9x}  {o:#9x}  {:>+9}  {s}",
                i64::from(*o) - i64::from(*r)
            );
        }
        if !a.all {
            println!("  (misplaced only, first 20; pass --all for every resolved symbol)");
        }
    }
    let mut ref_addrs: Vec<u32> = pl.rows.iter().map(|(_, r, _)| *r).collect();
    ref_addrs.sort_unstable();
    let mut our_addrs: Vec<u32> = pl.rows.iter().map(|(_, _, o)| *o).collect();
    our_addrs.sort_unstable();
    let our_to_ref: BTreeMap<u32, u32> = pl.rows.iter().map(|(_, r, o)| (*o, *r)).collect();
    let rf = placement::law_fit(&ref_addrs, &|x| env.recorded(x));
    let of = placement::law_fit(&our_addrs, &|x| {
        our_to_ref.get(&x).and_then(|r| env.recorded(*r))
    });
    let rt = rf.fit + rf.deviations.len();
    let ot = of.fit + of.deviations.len();
    if rt > 0 && ot > 0 {
        println!("  ILINK RESERVATION LAW  slot = round_up_16(floor(body * 1.25))");
        println!(
            "    reference {} of {rt} slots fit, {} deviate, signed {:+} bytes",
            rf.fit,
            rf.deviations.len(),
            rf.signed
        );
        println!(
            "    ours      {} of {ot} slots fit, {} deviate, signed {:+} bytes",
            of.fit,
            of.deviations.len(),
            of.signed
        );
        for (x, body, slot, d) in rf.deviations.iter().take(4) {
            println!(
                "      reference {x:#08x}  body {body:>5}  slot {slot:>6}  law {:>6}  {d:>+6}  {}",
                placement::reserved_slot(*body),
                if *d > 0 {
                    "body SHRANK into a committed slot"
                } else {
                    "body GREW into its reservation"
                }
            );
        }
    }
    if a.content {
        match ours.as_ref().and_then(|o| placement::content(&env, o, &pl)) {
            Some((c, first)) => {
                println!(
                    "  CONTENT, body-vs-body at each image's own address ({} single-function rows, {} bytes)",
                    c.functions, c.bytes
                );
                println!("    differing at a `.reloc` site        {}", c.reloc);
                println!("    differing in an `e8`/`e9` rel32     {}", c.rel32);
                println!("    UNEXPLAINED                         {}", c.other);
                if !first.is_empty() {
                    println!("    first: {}", first.join(", "));
                }
            }
            None => println!("  CONTENT: not compared -- no readable image beside the map"),
        }
    }
    let rows: Vec<PlaceRow<'_>> = pl
        .rows
        .iter()
        .map(|(s, r, o)| PlaceRow {
            symbol: s,
            reference: *r,
            ours: *o,
        })
        .collect();
    json(&a.json, "place map", &env.label, None::<&()>, &rows)?;
    Ok(Outcome::Ok)
}

fn place_seated(config: Option<&Path>, a: &cli::PlaceSeatedArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let ours = msvc6_gate::libs::read_bounded(&a.ours, msvc6_gate::libs::MAX_ARCHIVE)?;
    let map = a.map.as_deref().map(Map::read).transpose()?;
    let rep = seated::measure(&env, &ours, &list, map.as_ref())?;
    for (o, e) in &rep.refused {
        eprintln!("place seated: REFUSED {}: {e}", o.display());
    }
    say(&seated::lines(
        &env.label,
        &a.ours.display().to_string(),
        &rep,
        a.list_placement,
    ));
    json(
        &a.json,
        "place seated",
        &env.label,
        None::<&()>,
        &rep.placement,
    )?;
    Ok(if rep.refused.is_empty() {
        Outcome::Ok
    } else {
        Outcome::Exit(std::process::ExitCode::FAILURE)
    })
}

fn place_partition(config: Option<&Path>, a: &cli::PlacePartitionArgs) -> Result<Outcome> {
    let (p, n, env) = env_of(config, &a.image)?;
    let map = Map::read(&a.map)?;
    if partition::map_objects(&map).is_empty() {
        return Err(anyhow!(
            "{} names no own objects on its `0001:` lines",
            a.map.display()
        ));
    }
    let mut cpp = String::new();
    let mut sources = 0usize;
    for s in tu_sources(&p, n)?
        .iter()
        .filter(|s| !s.extension().is_some_and(|e| e.eq_ignore_ascii_case("dsp")))
    {
        if let Ok(t) = msvc6_gate::libs::read_bounded(s, MAX_SOURCE).map(|b| srcio::decode(&b)) {
            cpp.push_str(&t);
            cpp.push('\n');
            sources += 1;
        }
    }
    if sources == 0 {
        return Err(anyhow!(
            "no TU source of image `{}` could be read: ownership cannot be decided",
            env.label
        ));
    }
    let part = partition::measure(&env, &map, &cpp);
    say(&partition::lines(&env.label, &part, a.moves, a.list));
    #[derive(Debug, Serialize)]
    struct RunRow<'a> {
        object: &'a str,
        functions: usize,
        first: u32,
    }
    let rows: Vec<RunRow<'_>> = part
        .runs
        .iter()
        .map(|(o, v)| RunRow {
            object: o,
            functions: v.len(),
            first: v.first().map_or(0, |x| x.0),
        })
        .collect();
    json(&a.json, "place partition", &env.label, None::<&()>, &rows)?;
    Ok(Outcome::Ok)
}

fn parse_rva(s: &str) -> Result<u32> {
    let t = s.trim();
    let h = t
        .strip_prefix("0x")
        .or_else(|| t.strip_prefix("0X"))
        .unwrap_or(t);
    u32::from_str_radix(h, 16).with_context(|| format!("`{s}` is no hexadecimal rva"))
}

fn place_locate(config: Option<&Path>, a: &cli::PlaceLocateArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let rva = parse_rva(&a.rva)?;
    let len = env.recorded(rva).with_context(|| {
        format!(
            "no recorded function starts at {rva:#x}: locate takes the REFERENCE's rva with a recorded extent, not our linked address"
        )
    })?;
    let cand =
        Image::load(&a.candidate).with_context(|| format!("loading {}", a.candidate.display()))?;
    let name = env
        .function_names
        .get(&rva)
        .cloned()
        .unwrap_or_else(|| format!("{rva:#x}"));
    let l = bodies::locate(&env.img, &cand, rva, len as usize)?;
    say(&bodies::lines(
        &env.label,
        &name,
        false,
        &a.candidate.display().to_string(),
        &l,
    ));
    json(&a.json, "place locate", &env.label, None::<&()>, &l.hits)?;
    Ok(Outcome::Ok)
}
