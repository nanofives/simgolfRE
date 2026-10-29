use crate::Outcome;
use crate::cli;
use crate::diff::{env_of, jobs, object_list};
use crate::image::json;
use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::env::Env;
use msvc6_diff::objs;
use msvc6_diff::seat::{self, SeatMap};
use msvc6_gate::{addrsym, calltargets, relocid};
use msvc6_image::pe::Image;
use msvc6_project::config::Project;
use msvc6_project::mapfile::Map;
use msvc6_project::spans::{index, join};
use serde::Serialize;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

pub fn objects(o: &cli::BuildObjs) -> Result<Vec<PathBuf>> {
    if o.skip_lib.is_empty() {
        return object_list(&o.objs, &o.obj, !o.no_libs);
    }
    let mut out = Vec::new();
    for d in &o.objs {
        out.extend(objs::objs_in(d)?);
        if o.no_libs {
            continue;
        }
        for (lib, name) in objs::workspace_libs(d) {
            if o.skip_lib.iter().any(|s| s.eq_ignore_ascii_case(&name)) {
                continue;
            }
            out.extend(objs::lib_objs_in(&lib, d, &name)?);
        }
    }
    out.extend(o.obj.iter().cloned());
    if out.is_empty() {
        bail!("no objects: pass --objs DIR or --obj FILE");
    }
    Ok(out)
}

fn seated(env: &Env, list: &[PathBuf], jobs: usize, verb: &str) -> SeatMap {
    let map = seat::seat_map(env, list, jobs);
    for (o, e) in &map.refused {
        eprintln!("{verb}: REFUSED {}: {e}", o.display());
    }
    map
}

pub fn libraries(p: &Project, n: usize, given: &[PathBuf]) -> Result<Vec<PathBuf>> {
    if !given.is_empty() {
        return Ok(given.to_vec());
    }
    let d = p.images.get(n).with_context(|| format!("no image[{n}]"))?;
    let key = join(&p.image_key(n), "libraries");
    d.libraries
        .iter()
        .enumerate()
        .map(|(i, l)| p.path_at(&index(&key, i), l))
        .collect()
}

fn write(path: &Path, text: &str) -> Result<()> {
    srcio::own_write(path, text).with_context(|| format!("writing {}", path.display()))
}

fn emit(out: Option<&Path>, text: &str) -> Result<()> {
    match out {
        Some(p) => write(p, text),
        None => {
            print!("{text}");
            Ok(())
        }
    }
}

#[derive(Debug, Serialize)]
struct RelocidSummary {
    seated: usize,
    checked: usize,
    findings: usize,
    verdicts: std::collections::BTreeMap<&'static str, usize>,
    refused: Vec<String>,
}

pub fn relocid(config: Option<&Path>, a: &cli::RelocidArgs) -> Result<Outcome> {
    let (p, n, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let jobs = jobs(a.objs.jobs);
    let map = seated(&env, &list, jobs, "relocid");
    let only = a.func.as_deref().map(|f| env.rva_of(f)).transpose()?;
    let archives = libraries(&p, n, &a.libs)?;
    let rep = relocid::measure(&relocid::Inputs {
        env: &env,
        objs: &list,
        seats: &map.seats,
        archives: &archives,
        notes_only: &a.notes_only,
        only,
        jobs,
    });
    if only.is_some() && rep.checked == 0 {
        println!(
            "relocid {}: the function is not seated or not byte-identical under try's mask: not checked",
            env.label
        );
    }
    let mut tsv = String::from(relocid::TSV_HEAD);
    for r in rep
        .rows
        .iter()
        .filter(|r| a.all || only.is_some() || r.verdict.finding() || r.verdict.lead())
    {
        tsv.push_str(&relocid::tsv_line(&env.label, r));
    }
    emit(a.out.as_deref(), &tsv)?;
    if let Some(s) = &a.seats_out {
        let text: String = rep
            .seats
            .iter()
            .map(|(r, c)| format!("{r:#08x}\t{}\n", if *c { "checked" } else { "not" }))
            .collect();
        write(s, &text)?;
    }
    for r in rep.rows.iter().filter(|r| r.verdict.finding()) {
        println!("{}", relocid::finding_line(&env.label, r));
    }
    let mut refused: Vec<String> = map
        .refused
        .iter()
        .map(|(o, e)| format!("{}: {e}", o.display()))
        .collect();
    refused.extend(
        rep.refused
            .iter()
            .map(|(o, e)| format!("{}: {e}", o.display())),
    );
    for l in relocid::summary(&env.label, &rep, refused.len()) {
        println!("{l}");
    }
    for r in &rep.refused {
        eprintln!("relocid: REFUSED {}: {}", r.0.display(), r.1);
    }
    let summary = RelocidSummary {
        seated: rep.seated,
        checked: rep.checked,
        findings: rep.findings(),
        verdicts: relocid::tally(&rep)
            .into_iter()
            .map(|(v, n)| (v.as_str(), n))
            .collect(),
        refused: refused.clone(),
    };
    let rows: Vec<&relocid::Row> = rep
        .rows
        .iter()
        .filter(|r| a.all || only.is_some() || r.verdict.finding() || r.verdict.lead())
        .collect();
    json(&a.json, "relocid", &env.label, Some(&summary), &rows)?;
    if !refused.is_empty() {
        return Ok(Outcome::Exit(ExitCode::FAILURE));
    }
    Ok(if rep.findings() == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn map_data_names(map: Option<&Path>) -> Result<BTreeSet<String>> {
    let Some(m) = map else {
        return Ok(BTreeSet::new());
    };
    let m = Map::read(m)?;
    Ok(m.symbols
        .iter()
        .filter(|s| s.section != 1 && s.lib.is_none() && s.object.ends_with(".obj"))
        .map(|s| s.name.clone())
        .collect())
}

#[derive(Debug, Serialize)]
struct CallSummary {
    objects: usize,
    seated_names: usize,
    checked: usize,
    agreed: usize,
    disagreed: usize,
    through_thunk: usize,
    uncovered_sites: usize,
    uncovered_names: usize,
    unseated: usize,
    superseded: usize,
    duplicate: usize,
    refused: Vec<String>,
}

#[derive(Debug, Serialize)]
struct WrongRow<'a> {
    object: &'a str,
    from: &'a str,
    from_rva: u32,
    site: u32,
    symbol: &'a str,
    symbol_at: u32,
    image_target: u32,
}

pub fn calltargets(config: Option<&Path>, a: &cli::CalltargetsArgs) -> Result<Outcome> {
    let (_, _, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let own = map_data_names(a.map.as_deref())?;
    let rep = calltargets::measure(&calltargets::Inputs {
        env: &env,
        objs: &list,
        census: a.causes.is_some(),
        map_data: &own,
    })?;
    for (o, e) in &rep.refused {
        eprintln!("calltargets: REFUSED {}: {e}", o.display());
    }
    for l in calltargets::report_lines(&env, &rep, a.list_uncovered) {
        println!("{l}");
    }
    if let Some(path) = &a.causes {
        let c = calltargets::causes(&env, &rep);
        let bodies = path.with_extension("bodies.tsv");
        write(path, &c.rows)?;
        write(&bodies, &c.bodies)?;
        for l in calltargets::causes_lines(&c, rep.wrong.len(), path, &bodies) {
            println!("{l}");
        }
    }
    let refused: Vec<String> = rep
        .refused
        .iter()
        .map(|(o, e)| format!("{}: {e}", o.display()))
        .collect();
    let summary = CallSummary {
        objects: rep.objects,
        seated_names: rep.seated_names,
        checked: rep.checked,
        agreed: rep.agreed,
        disagreed: rep.wrong.len(),
        through_thunk: rep.through_thunk,
        uncovered_sites: rep.uncovered.values().sum(),
        uncovered_names: rep.uncovered.len(),
        unseated: rep.unseated,
        superseded: rep.dropped.len(),
        duplicate: rep.duplicate.len(),
        refused: refused.clone(),
    };
    let rows: Vec<WrongRow<'_>> = rep
        .wrong
        .iter()
        .map(|w| WrongRow {
            object: &w.object,
            from: &w.from,
            from_rva: w.from_rva,
            site: w.site,
            symbol: &w.symbol,
            symbol_at: w.symbol_is_at,
            image_target: w.reference_targets,
        })
        .collect();
    json(&a.json, "calltargets", &env.label, Some(&summary), &rows)?;
    if !refused.is_empty() {
        return Ok(Outcome::Exit(ExitCode::FAILURE));
    }
    Ok(if rep.wrong.is_empty() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[derive(Debug, Serialize)]
struct AddrSummary {
    functions: usize,
    verdicts: std::collections::BTreeMap<String, usize>,
    failed: Vec<String>,
    splits: Vec<String>,
}

pub fn addrsym(config: Option<&Path>, a: &cli::AddrsymArgs) -> Result<Outcome> {
    let (p, n, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let jobs = jobs(a.objs.jobs);
    let exe = match &a.exe {
        Some(e) => e.clone(),
        None => {
            let d = p.images.get(n).context("no such image")?;
            let r = p.reference(n)?;
            let name = r.path.file_name().map(PathBuf::from).with_context(|| {
                format!("image `{}`'s reference has no file name: pass --exe", d.id)
            })?;
            a.objs
                .objs
                .first()
                .map(|o| o.join(&name))
                .context("pass --exe, or an --objs directory holding the linked image")?
        }
    };
    let map_path = a.map.clone().unwrap_or_else(|| exe.with_extension("map"));
    let ours = Image::load(&exe).with_context(|| format!("loading {}", exe.display()))?;
    let map = Map::read(&map_path)?;
    if map.symbols.is_empty() {
        bail!("{}: no symbol rows", map_path.display());
    }
    let linked = addrsym::Linked::new(&ours, &map);
    let seats = seated(&env, &list, jobs, "addrsym");
    let targets: Vec<u32> = if a.funcs.is_empty() {
        seats.seats.keys().copied().collect()
    } else {
        a.funcs
            .iter()
            .map(|f| env.rva_of(f))
            .collect::<Result<_>>()?
    };
    let rep = addrsym::run(
        &env,
        &seats.seats,
        &linked,
        &targets,
        !a.funcs.is_empty(),
        a.all,
        jobs,
    )?;
    emit(a.out.as_deref(), &addrsym::tsv(&rep.rows))?;
    for l in &rep.lines {
        println!("{l}");
    }
    println!("{}", addrsym::total_line(&rep));
    for s in &rep.splits {
        println!("{s}");
    }
    let summary = AddrSummary {
        functions: rep.functions,
        verdicts: rep.total.clone(),
        failed: rep.failed.clone(),
        splits: rep.splits.clone(),
    };
    json(&a.json, "addrsym", &env.label, Some(&summary), &rep.rows)?;
    if !seats.refused.is_empty() {
        return Ok(Outcome::Exit(ExitCode::FAILURE));
    }
    Ok(if rep.mismatches() == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[derive(Debug, Serialize)]
struct AccountSummary {
    symbols: usize,
    totals: msvc6_gate::account::Split,
    file_total: usize,
    unseen: usize,
    content_defects: usize,
    displaced: Vec<String>,
    met: bool,
}

#[derive(Debug, Serialize)]
struct AccountRow<'a> {
    section: &'a str,
    #[serde(flatten)]
    split: &'a msvc6_gate::account::Split,
}

pub fn account(config: Option<&Path>, a: &cli::AccountArgs) -> Result<Outcome> {
    use msvc6_gate::account;
    let (_, _, env) = env_of(config, &a.image)?;
    let ours = Image::load(&a.ours).with_context(|| format!("loading {}", a.ours.display()))?;
    let map = Map::read(&a.map)?;
    let owners = msvc6_project::mapfile::Owners::new(&a.own);
    let acc = account::measure(&env.img, &ours, &map, &owners)?;
    for l in account::lines(
        &acc,
        &a.ours.display().to_string(),
        &a.map.display().to_string(),
    ) {
        println!("{l}");
    }
    if a.objs.objs.is_empty() && a.objs.obj.is_empty() {
        println!(
            "  CROSS-CHECK SKIPPED: pass --objs for our objects' COMDAT extents; without them .text's OURS is a hull, an upper bound"
        );
    } else {
        let list = objects(&a.objs)?;
        match account::own_text_bytes(&env, &ours, &list) {
            Ok((differ, total)) => println!(
                "  CROSS-CHECK, .text own bytes from our objects' COMDAT extents: {differ} of {total} own COMDAT byte(s) differ, against {} attributed OURS in .text above",
                acc.per.get(".text").map_or(0, |s| s.ours)
            ),
            Err(e) => println!("  CROSS-CHECK SKIPPED: {e:#}"),
        }
    }
    let t = acc.totals();
    let unseen = acc.unseen();
    if unseen > 0 {
        println!(
            "ACCOUNT {}: NOT MET -- {unseen} differing byte(s) are UNSEEN: outside every shared section's extent and every named range",
            env.label
        );
    }
    if acc.content_defects() > 0 {
        println!(
            "ACCOUNT {}: NOT MET -- {} distinct own value(s) occur nowhere in the reference's copy of their section: a CONTENT difference",
            env.label,
            acc.content_defects()
        );
    }
    if t.unclassified > 0 {
        println!(
            "ACCOUNT {}: NOT MET -- {} differing byte(s) are UNCLASSIFIED, below the first /MAP symbol of a section that has symbols:",
            env.label, t.unclassified
        );
        for r in account::unclassified_runs(&acc) {
            println!("{r}");
        }
        if acc.unclassified.len() >= account::UNCLASSIFIED_CAP {
            println!(
                "    (collection capped at {} addresses)",
                account::UNCLASSIFIED_CAP
            );
        }
    }
    if acc.met() {
        println!(
            "ACCOUNT {}: MET -- {} differing byte(s), 0 UNCLASSIFIED",
            env.label, t.differing
        );
    }
    println!("{}", account::engineer_line(&acc));
    let summary = AccountSummary {
        symbols: acc.symbols,
        totals: t,
        file_total: acc.file_total,
        unseen,
        content_defects: acc.content_defects(),
        displaced: acc.displaced.clone(),
        met: acc.met(),
    };
    let rows: Vec<AccountRow<'_>> = acc
        .per
        .iter()
        .filter(|(_, s)| s.differing > 0)
        .map(|(n, s)| AccountRow {
            section: n,
            split: s,
        })
        .collect();
    json(&a.json, "account", &env.label, Some(&summary), &rows)?;
    Ok(if acc.met() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[derive(Debug, Serialize)]
struct LiteralsSummary {
    objects: usize,
    sites: usize,
    mismatched: usize,
    literals_defined: usize,
    misaligned: usize,
    unplaced: usize,
    dtor_bodies: usize,
    dtor_sites: usize,
    data_sites: usize,
    foreign_sites: usize,
}

pub fn literals(config: Option<&Path>, a: &cli::LiteralsArgs) -> Result<Outcome> {
    use msvc6_gate::literals;
    let (_, _, env) = env_of(config, &a.image)?;
    let mut list = objects(&a.objs)?;
    list.sort_by(|x, y| x.file_name().cmp(&y.file_name()));
    let map = a.map.as_deref().map(Map::read).transpose()?;
    let placer = msvc6_diff::resolve::Placer::new(
        &env.names,
        |s| a.notes_only.iter().any(|n| Path::new(s).ends_with(n)),
        map.as_ref(),
        env.text(),
        env.text_rva,
    );
    let base = u32::try_from(env.base).context("an image base past 4 GiB")?;
    let size = env.img.opt.size_of_image;
    let bytes = |rva: u32, n: usize| {
        env.img
            .at_rva(msvc6_base::units::Rva(rva), n)
            .map(<[u8]>::to_vec)
    };
    let mut total = literals::Report::default();
    let mut rows: Vec<literals::Mismatch> = Vec::new();
    let mut refused = 0usize;
    for p in &list {
        let stem = seat::scope_stem(p);
        let place = |f: &str| placer.place(&stem, f);
        let r = match msvc6_diff::comdat::read(p)
            .and_then(|d| literals::screen(&d, base, size, &place, &bytes))
        {
            Ok(r) => r,
            Err(e) => {
                eprintln!("literals: REFUSED {}: {e:#}", p.display());
                refused += 1;
                continue;
            }
        };
        for m in &r.mismatched {
            println!("{}", literals::mismatch_line(&stem, m));
        }
        println!("{}", literals::object_line(&stem, &r));
        for u in r.unplaced.iter().take(4) {
            println!("    unplaced: {u}");
        }
        for (f, at) in r.misaligned.iter().take(4) {
            println!("    layout differs at the site, NOT CHECKED: {f} +{at:#x}");
        }
        rows.extend(r.mismatched.iter().cloned());
        total.add(r);
    }
    for l in literals::total_lines(list.len(), &total) {
        println!("{l}");
    }
    let bad = total.mismatched.len();
    if bad > 0 {
        println!(
            "LITERALS {}: FAIL -- {bad} literal site(s) name a different string than the reference's instruction does",
            env.label
        );
    } else {
        println!(
            "LITERALS {}: PASS -- every checked site names the reference's string",
            env.label
        );
    }
    let summary = LiteralsSummary {
        objects: list.len(),
        sites: total.sites,
        mismatched: bad,
        literals_defined: total.literals_defined,
        misaligned: total.misaligned.len(),
        unplaced: total.unplaced.len(),
        dtor_bodies: total.dtor_bodies,
        dtor_sites: total.dtor_sites,
        data_sites: total.data_sites,
        foreign_sites: total.foreign_sites,
    };
    json(&a.json, "literals", &env.label, Some(&summary), &rows)?;
    if refused > 0 {
        return Ok(Outcome::Exit(ExitCode::FAILURE));
    }
    Ok(if bad == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

pub fn jumps(config: Option<&Path>, a: &cli::JumpsArgs) -> Result<Outcome> {
    use msvc6_gate::jumps;
    let (_, _, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let jobs = jobs(a.objs.jobs);
    let excuses = a
        .excuses
        .as_deref()
        .map(jumps::read_excuses)
        .transpose()?
        .unwrap_or_default();
    let map = seated(&env, &list, jobs, "jumps");
    if let Some(p) = &a.seats_out {
        let text: String = map.seats.keys().map(|r| format!("{r:#08x}\n")).collect();
        write(p, &text)?;
    }
    let ctx = msvc6_diff::rawdiff::Ctx::build(&env, &list, &map.seats);
    if let Some(f) = &a.func {
        let rva = env.rva_of(f)?;
        let (o, c) = seat::find(&env, &list, rva, a.symbol.as_deref())?;
        let an =
            msvc6_diff::segments::analyze_in(&env, rva, &o, c, msvc6_diff::segments::MAX_SEGMENT)?;
        let ch = jumps::check_linked(&env, &ctx, &an);
        println!(
            "jumps {} {f}: image {:#x} ({} instructions, {} near branches) vs ours {} ({} instructions, {} near branches, {} relocated)",
            env.label,
            an.start,
            an.img.dec.len(),
            ch.img_t.iter().flatten().count(),
            an.obj.display(),
            an.ours.dec.len(),
            ch.ours_t.iter().flatten().count(),
            ch.relocated
        );
        let fs = jumps::findings(&an, &ch);
        for x in &fs {
            println!(
                "{}{}",
                x.line,
                jumps::tag(&excuses.named, &env.label, an.rva, x)
            );
        }
        let (n, m, t, k) = jumps::summary(&ch.verdicts);
        println!(
            "jumps: {n} paired, {m} mismatched, {t} target-unaligned, {k} unpaired; {} leave the function, each compared by destination ({} relocated, {} of them into this function)",
            ch.outside, ch.relocated, ch.own
        );
        let (fm, ft, ok) = jumps::failing(&fs, &excuses.named, &env.label, an.rva);
        println!("{}", jumps::landing_line(fm, ft, ok));
        json(&a.json, "jumps", &env.label, None::<&()>, &fs)?;
        return Ok(if fm + ft == 0 {
            Outcome::Ok
        } else {
            Outcome::Differs
        });
    }
    let all = jumps::run_all(&env, &list, &map.seats, &excuses, jobs);
    println!(
        "jumps {} --all: {} object(s), {} function(s) seated",
        env.label,
        list.len(),
        all.done.len()
    );
    let mut rows: Vec<&jumps::Finding> = Vec::new();
    let mut errs: Vec<String> = Vec::new();
    for (rva, name, r) in &all.done {
        match r {
            Ok(d) => {
                for f in d
                    .findings
                    .iter()
                    .filter(|f| f.kind != jumps::Kind::Unpaired)
                {
                    println!(
                        "FINDING {rva:#08x} {name}: {}{}",
                        f.line,
                        jumps::tag(&excuses.named, &env.label, *rva, f)
                    );
                    rows.push(f);
                }
            }
            Err(e) => errs.push(format!("{rva:#08x} {name}: {e}")),
        }
    }
    for b in all.dups.ok.iter().chain(&all.dups.bad) {
        println!("FINDING {b}");
    }
    let mut show_failed: Vec<String> = Vec::new();
    for s in &a.show {
        let rva = env.rva_of(s)?;
        match all.done.iter().find(|(r, _, _)| *r == rva) {
            Some((rva, name, Ok(d))) => println!(
                "jumps {rva:#08x} {name}: {} paired, {} mismatched, {} target-unaligned, {} unpaired; {} leave the function, {} relocated",
                d.summary.0, d.summary.1, d.summary.2, d.summary.3, d.relocated.2, d.relocated.0
            ),
            Some((rva, name, Err(e))) => {
                println!("jumps {rva:#08x} {name}: NOT CHECKED -- {e}");
                show_failed.push(format!("{rva:#x}"));
            }
            None => {
                println!("jumps {rva:#08x}: NOT CHECKED -- no object of the build seats it");
                show_failed.push(format!("{rva:#x}"));
            }
        }
    }
    for e in errs.iter().take(10) {
        println!("  not checked: {e}");
    }
    for s in jumps::stale_named(&excuses.named, &env.label, &all) {
        println!(
            "  STALE named row: {} {:#x} ours +{:#x} matches no TARGET-UNALIGNED row",
            s.image, s.rva, s.ours
        );
    }
    for (o, e) in &all.dups.refused {
        eprintln!("jumps: REFUSED {}: {e}", o.display());
    }
    let total = jumps::total_line(&all);
    json(&a.json, "jumps", &env.label, Some(&all.totals), &rows)?;
    if !map.refused.is_empty() || !all.dups.refused.is_empty() {
        println!(
            "JUMPS {}: FAIL -- {total}; an object could not be read",
            env.label
        );
        return Ok(Outcome::Exit(ExitCode::FAILURE));
    }
    if !jumps::passes(&all) || !show_failed.is_empty() {
        println!(
            "JUMPS {}: FAIL -- {total}; {} function(s) with a MISMATCH or an unnamed TARGET-UNALIGNED row{}",
            env.label,
            all.totals.bad_functions,
            if show_failed.is_empty() {
                String::new()
            } else {
                format!(", unchecked: {}", show_failed.join(" "))
            }
        );
        return Ok(Outcome::Differs);
    }
    println!("JUMPS {}: PASS -- {total}", env.label);
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct OperandRow {
    cert: f64,
    class: &'static str,
    line: String,
}

pub fn operands(config: Option<&Path>, a: &cli::OperandsArgs) -> Result<Outcome> {
    use msvc6_gate::operands;
    let (_, _, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let jobs = jobs(a.objs.jobs);
    let mut seats = seated(&env, &list, jobs, "operands").seats;
    let only = a.func.as_deref().map(|f| env.rva_of(f)).transpose()?;
    if let (Some(rva), Some(sym)) = (only, &a.symbol) {
        let found = seat::find(&env, &list, rva, Some(sym))?;
        seats.insert(rva, found);
    }
    let rvas: Vec<u32> = match only {
        Some(r) => vec![r],
        None => seats.keys().copied().collect(),
    };
    let text = env.text().to_vec();
    let text_rva = env.text_rva;
    let pops: std::sync::Mutex<std::collections::HashMap<u32, Option<u32>>> = Default::default();
    let pop_at = |r: u32| {
        if let Some(v) = pops.lock().ok().and_then(|m| m.get(&r).copied()) {
            return v;
        }
        let v = operands::callee_pop(&text, text_rva, r);
        if let Ok(mut m) = pops.lock() {
            m.insert(r, v);
        }
        v
    };
    type One = (u32, Result<(Vec<OperandRow>, operands::Tally, Vec<String>)>);
    let done: Vec<One> = msvc6_diff::par::map(&rvas, jobs, |&rva| {
        let r = (|| {
            let seat = seats
                .get(&rva)
                .with_context(|| format!("no COMDAT seats at {rva:#x}"))?;
            let an = msvc6_diff::segments::analyze_in(
                &env,
                rva,
                &seat.0,
                seat.1.clone(),
                msvc6_diff::segments::MAX_SEGMENT,
            )?;
            let name = seat.1.symbol.clone().unwrap_or_else(|| "?".into());
            let (hits, tally) = operands::find(&an, &pop_at);
            let depth = if a.depths {
                operands::depth_lines(&an, &pop_at)
            } else {
                Vec::new()
            };
            Ok((
                hits.iter()
                    .filter(|h| h.cert >= a.min_cert)
                    .map(|h| OperandRow {
                        cert: h.cert,
                        class: h.class,
                        line: operands::line(&an, &name, h),
                    })
                    .collect(),
                tally,
                depth,
            ))
        })();
        (rva, r)
    });
    let mut rows: Vec<OperandRow> = Vec::new();
    let mut by_class: std::collections::BTreeMap<&str, usize> = std::collections::BTreeMap::new();
    let (mut pairs, mut reorders, mut fns) = (0usize, 0usize, 0usize);
    for (rva, r) in done {
        match r {
            Ok((ls, t, depth)) => {
                fns += 1;
                pairs += t.pairs;
                reorders += t.reorders;
                for d in depth {
                    eprintln!("{d}");
                }
                for l in ls {
                    *by_class.entry(l.class).or_default() += 1;
                    rows.push(l);
                }
            }
            Err(e) => eprintln!("operands: {rva:#x}: {e:#}"),
        }
    }
    rows.sort_by(|x, y| y.cert.total_cmp(&x.cert).then(x.line.cmp(&y.line)));
    let mut tsv = format!("{}\n", operands::HEADER);
    for r in &rows {
        tsv.push_str(&r.line);
        tsv.push('\n');
    }
    match &a.out {
        Some(p) => {
            write(p, &tsv)?;
            println!("wrote {} row(s) to {}", rows.len(), p.display());
        }
        None => print!("{tsv}"),
    }
    let classes: Vec<String> = by_class.iter().map(|(k, n)| format!("{k} {n}")).collect();
    println!(
        "operands: {} hit(s) at cert >= {} over {fns} function(s), {pairs} paired instruction(s); {reorders} reorder(s) filtered; {}",
        rows.len(),
        a.min_cert,
        classes.join(", ")
    );
    json(&a.json, "operands", &env.label, None::<&()>, &rows)?;
    Ok(Outcome::Ok)
}

pub fn globals(config: Option<&Path>, a: &cli::GlobalsArgs) -> Result<Outcome> {
    use msvc6_gate::globals;
    let (_, _, env) = env_of(config, &a.image)?;
    let ours = Image::load(&a.ours).with_context(|| format!("loading {}", a.ours.display()))?;
    let map = Map::read(&a.map)?;
    let rep = globals::measure(&env, &ours, &map)?;
    for l in globals::lines(&env.label, &rep, a.all) {
        println!("{l}");
    }
    json(&a.json, "globals", &env.label, None::<&()>, &rep.pairs)?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct CallRow {
    caller: Option<u32>,
    caller_name: String,
    callee: Option<u32>,
    callee_name: String,
    image: usize,
    ours: usize,
}

pub fn calls(config: Option<&Path>, a: &cli::CallsArgs) -> Result<Outcome> {
    use msvc6_gate::calls::{self, Func};
    let (_, _, env) = env_of(config, &a.image)?;
    let list = objects(&a.objs)?;
    let t = calls::tables(&env, &list, jobs(a.objs.jobs));
    for (o, e) in &t.ours.refused {
        eprintln!("calls: REFUSED {}: {e}", o.display());
    }
    let o = calls::Opts {
        caller: a.caller.as_deref(),
        callee: a.callee.as_deref(),
        diff: a.diff,
        by_callee: a.by_callee,
    };
    let dirs: Vec<String> = a
        .objs
        .objs
        .iter()
        .map(|d| d.display().to_string())
        .collect();
    for l in calls::lines(&t, &o, &dirs.join(", ")) {
        println!("{l}");
    }
    let at = |f: &Func| match f {
        Func::At(r) => Some(*r),
        Func::Unresolved(_) => None,
    };
    let mut keys: BTreeSet<&(Func, Func)> = t.img.keys().collect();
    keys.extend(t.ours.pairs.keys());
    let rows: Vec<CallRow> = keys
        .into_iter()
        .map(|k| CallRow {
            caller: at(&k.0),
            caller_name: t.names.of(&k.0),
            callee: at(&k.1),
            callee_name: t.names.of(&k.1),
            image: t.img.get(k).copied().unwrap_or(0),
            ours: t.ours.pairs.get(k).copied().unwrap_or(0),
        })
        .collect();
    json(&a.json, "calls", &env.label, None::<&()>, &rows)?;
    if !t.ours.refused.is_empty() {
        return Ok(Outcome::Exit(ExitCode::FAILURE));
    }
    Ok(Outcome::Ok)
}
