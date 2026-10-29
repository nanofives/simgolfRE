use anyhow::{Context as _, Result, bail};
use msvc6_match::copies::{Group, Offered};
use msvc6_match::locate::Placed;
use msvc6_match::table::{ArchiveRow, Comparison, Table, compare};
use msvc6_project::spans::{index, join};
use serde::Serialize;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

use crate::Outcome;
use crate::cli::LibLocateArgs;
use crate::image::json;
use crate::project::{self, Config};

#[derive(Debug, Serialize)]
struct Row<'a> {
    member: &'a str,
    archive: &'a str,
    index: usize,
    lo: Option<u32>,
    hi: Option<u32>,
    bytes: u32,
    kept: u32,
    comdats: usize,
    placed: usize,
    by_order: usize,
    by_witness: usize,
    ambiguous: usize,
    absent: usize,
    unidentifiable: usize,
    contiguous: bool,
    ranges: &'a [(u32, u32)],
    comp_id: Option<u32>,
    tied_with: &'a [String],
    tie_break: &'a [String],
    outvoted: &'a [String],
    refused: &'a [msvc6_match::locate::Refused],
}

impl<'a> Row<'a> {
    fn of(p: &'a Placed) -> Self {
        Self {
            member: &p.member,
            archive: &p.archive,
            index: p.index,
            lo: p.lo,
            hi: p.hi,
            bytes: p.bytes,
            kept: p.kept(),
            comdats: p.comdats,
            placed: p.unique,
            by_order: p.resolved_by_order,
            by_witness: p.by_witness,
            ambiguous: p.ambiguous,
            absent: p.absent,
            unidentifiable: p.unidentifiable,
            contiguous: p.contiguous,
            ranges: &p.ranges,
            comp_id: p.comp_id,
            tied_with: &p.tied_with,
            tie_break: &p.tie_break,
            outvoted: &p.outvoted,
            refused: &p.refused,
        }
    }
}

#[derive(Debug, Serialize)]
struct Summary<'a> {
    text_rva: u32,
    text_len: usize,
    considered: usize,
    present: usize,
    covered: u32,
    contested: u32,
    by_order: usize,
    by_witness: usize,
    libraries: &'a [Group],
    record: Option<&'a Comparison>,
    census: &'a [msvc6_match::census::Bucket],
    imports: Option<&'a msvc6_match::imports::Join>,
    unmatched_with_code: Vec<(String, usize)>,
}

fn offered(
    config: Option<&Config>,
    r: &project::Resolved,
    a: &LibLocateArgs,
) -> Result<Vec<Offered>> {
    let mut entries: Vec<(String, PathBuf)> = Vec::new();
    if a.libs.is_empty() {
        let (Some(c), Some(d), Some(key)) = (config, &r.decl, &r.key) else {
            bail!(
                "{} is no [[image]] of a config, so there are no `libraries`: pass --lib",
                r.path.display()
            );
        };
        if d.libraries.is_empty() {
            return Err(c.error(
                &join(key, "libraries"),
                format!("image `{}` declares no `libraries`; pass --lib", d.id),
            ));
        }
        for (i, l) in d.libraries.iter().enumerate() {
            let k = index(&join(key, "libraries"), i);
            entries.push((l.clone(), c.path_at(&k, l)?));
        }
    } else {
        for p in &a.libs {
            entries.push((p.display().to_string(), p.clone()));
        }
    }
    entries
        .into_iter()
        .map(|(label, path)| {
            let bytes = project::read(&path)?;
            Ok(Offered { label, path, bytes })
        })
        .collect()
}

fn jobs(a: &LibLocateArgs) -> usize {
    a.jobs.unwrap_or_else(|| {
        std::thread::available_parallelism()
            .map_or(1, |n| n.get() / 2)
            .clamp(1, 8)
    })
}

fn explain(
    img: &msvc6_image::pe::Image,
    offered: &[Offered],
    needle: &str,
    a: &LibLocateArgs,
    label: &str,
) -> Result<Outcome> {
    let rows = msvc6_match::explain(img, offered, needle)?;
    json::<(), _>(&a.json, "lib locate --explain", label, None, &rows)?;
    let mut last: Option<(String, String)> = None;
    for v in &rows {
        let here = (v.archive.clone(), v.member.clone());
        if last.as_ref() != Some(&here) {
            println!("{}  {}", v.archive, v.member);
            last = Some(here);
        }
        println!(
            "  #{:<3} {:<28} {:>4}B literal {:>4} anchor {:>3}  {}\n        {}",
            v.index,
            v.symbol.as_deref().unwrap_or("(static)"),
            v.len,
            v.literal,
            v.anchor,
            v.verdict,
            v.hex
        );
    }
    if rows.is_empty() {
        println!("no member's name contains `{needle}`");
    }
    Ok(Outcome::Ok)
}

fn flag(p: &Placed) -> &'static str {
    if p.unique < 2 {
        "single"
    } else if p.contiguous {
        "contiguous"
    } else {
        "SPLIT"
    }
}

fn libraries(groups: &[Group], base: u32) {
    for g in groups {
        let distinct = g.copies.iter().filter(|c| c.same_as.is_none()).count();
        let Some(chosen) = g.chosen_copy() else {
            continue;
        };
        if distinct < 2 {
            println!(
                "  {}: {}{}",
                g.file,
                chosen.label,
                if g.copies.len() > 1 {
                    format!(" ({} identical copies)", g.copies.len())
                } else {
                    String::new()
                }
            );
            continue;
        }
        println!(
            "  {}: {distinct} distinct copies, chosen by the bytes they place{}: {}",
            g.file,
            if g.tie {
                " (TIE: the first listed)"
            } else {
                ""
            },
            chosen.label
        );
        for c in &g.copies {
            match &c.same_as {
                Some(s) => println!("    {}  the same file as {s}", c.label),
                None => println!(
                    "    {}  {} of {} members present, {} bytes",
                    c.label, c.present, c.considered, c.covered
                ),
            }
        }
        for w in g.witnesses.iter().take(8) {
            let others: Vec<String> = w
                .others
                .iter()
                .map(|o| match (o.len, o.placed) {
                    (None, _) => format!("{} has no such section", o.label),
                    (Some(l), None) => format!("{} {l} B absent", o.label),
                    (Some(l), Some(r)) => format!("{} {l} B at {r:#x}", o.label),
                })
                .collect();
            println!(
                "    witness {} {} {} B at {:#x} (va {:#x}): {}",
                w.member,
                w.symbol,
                w.len,
                w.rva,
                w.rva.saturating_add(base),
                others.join("; ")
            );
        }
        if g.witnesses.len() > 8 {
            println!(
                "    ... and {} more witness section(s)",
                g.witnesses.len() - 8
            );
        }
    }
}

fn pulled(map: &Path) -> Result<BTreeSet<String>> {
    let m = msvc6_project::mapfile::Map::read(map)?;
    let set: BTreeSet<String> = m
        .symbols
        .iter()
        .filter(|s| s.lib.is_some())
        .map(|s| s.object.to_ascii_lowercase())
        .collect();
    if set.is_empty() {
        bail!(
            "{} names no archive member (no `lib:member.obj` in its public symbols)",
            map.display()
        );
    }
    Ok(set)
}

pub fn lib_locate(config: Option<&Path>, a: &LibLocateArgs) -> Result<Outcome> {
    let c = project::load(config)?;
    let mut r = project::resolve(c.as_ref(), &a.image)?;
    let label = r
        .path
        .file_name()
        .map_or_else(|| r.label.clone(), |n| n.to_string_lossy().into_owned());
    let img = msvc6_image::pe::Image::parse(std::mem::take(&mut r.bytes))
        .with_context(|| r.path.display().to_string())?;
    let offered = offered(c.as_ref(), &r, a)?;
    if let Some(needle) = &a.explain {
        return explain(&img, &offered, needle, a, &label);
    }
    let replaced = a
        .write
        .as_deref()
        .filter(|w| a.record.is_none() && w.is_file());
    let record = a
        .record
        .as_deref()
        .or(replaced)
        .map(Table::read)
        .transpose()?;
    let recorded = record.as_ref().map(Table::recorded).unwrap_or_default();
    let pulled = a.map.as_deref().map(pulled).transpose()?;
    let rep = msvc6_match::run(&img, &offered, &recorded, jobs(a))?;
    let present = rep.run.present();
    println!(
        "{label}: placing {} of {} archive member(s)",
        present.len(),
        rep.run.considered
    );
    if let Some(w) = replaced {
        println!(
            "  an exact tie is decided by the rows of {}, the table this run replaces",
            w.display()
        );
    }
    libraries(&rep.groups, rep.image_base);
    println!("\n  present objects, in image order:");
    for p in &present {
        let span = match (p.lo, p.hi) {
            (Some(x), Some(y)) => format!("{x:#08x}..{y:#08x}"),
            _ => "-".to_string(),
        };
        println!(
            "    {span}  {:>6}B  {:>3}/{:<3} comdats placed  {:11} {}",
            p.bytes,
            p.unique,
            p.comdats,
            flag(p),
            p.member
        );
        for o in &p.outvoted {
            println!("        evidence: {o}");
        }
        if !p.tied_with.is_empty() {
            if p.undecided().is_empty() {
                println!(
                    "        tie with {} decided: {}",
                    p.tied_with.join(", "),
                    p.tie_break.join("; ")
                );
            } else {
                println!(
                    "        UNDECIDED: ties exactly on covered bytes with {}; the archive and member name chose",
                    p.tied_with.join(", ")
                );
            }
        }
        for x in &p.refused {
            println!(
                "        refused {:#x}..{:#x}: held by {}",
                x.lo,
                x.hi,
                x.owners.join(", ")
            );
        }
    }
    let lost: Vec<&Placed> = rep
        .run
        .unmatched()
        .into_iter()
        .filter(|p| !p.refused.is_empty() || !p.outvoted.is_empty())
        .collect();
    if !lost.is_empty() {
        println!("\n  members that matched and kept no range:");
    }
    for p in &lost {
        println!("    {}", p.member);
        for o in &p.outvoted {
            println!("        evidence: {o}");
        }
        for x in &p.refused {
            println!(
                "        refused {:#x}..{:#x}: held by {}",
                x.lo,
                x.hi,
                x.owners.join(", ")
            );
        }
    }
    let cmp = a.record.as_ref().map(|_| compare(&recorded, &present));
    if let Some(k) = &cmp {
        for d in &k.differences {
            println!("{}", d.line());
        }
        println!(
            "  {} recorded member row(s) this placement does not re-derive as recorded",
            k.differences.len()
        );
        for u in &k.unrecorded {
            println!(
                "    UNRECORDED {:#08x} len {:#x}: {} ({})",
                u.rva, u.len, u.member, u.archive
            );
        }
        println!(
            "  {} of {} recorded member row(s) re-derived; {} kept range(s) the record does not hold",
            k.rederived,
            k.rows,
            k.unrecorded.len()
        );
    }
    let covered = rep.run.covered();
    let claimed = present
        .iter()
        .filter(|p| p.present())
        .map(|p| p.bytes)
        .fold(0u32, u32::saturating_add);
    let contested = claimed.saturating_sub(covered);
    let total = u32::try_from(rep.text_len).unwrap_or(u32::MAX).max(1);
    println!(
        "\n  {} of {} objects present, covering {covered} bytes = {:.1}% of .text",
        present.iter().filter(|p| p.present()).count(),
        present.len(),
        f64::from(covered) * 100.0 / f64::from(total)
    );
    let by_order: usize = present.iter().map(|p| p.resolved_by_order).sum();
    let by_witness: usize = present.iter().map(|p| p.by_witness).sum();
    println!(
        "  {by_order} COMDAT(s) placed by section order and {by_witness} by relocation witness, each verified at its address"
    );
    let exhausted: usize = rep.run.members.iter().map(|p| p.exhausted).sum();
    if exhausted > 0 {
        println!(
            "  {exhausted} COMDAT(s) ran out of the search budget ({} byte compares) and were read as unidentifiable",
            msvc6_match::place::SEARCH_BUDGET
        );
    }
    if contested > 0 {
        println!(
            "  {contested} of the {claimed} claimed bytes are contested: one body several members hold"
        );
    }
    let multi = present.iter().filter(|p| p.unique > 1).count();
    let contiguous = present
        .iter()
        .filter(|p| p.unique > 1 && p.contiguous)
        .count();
    println!(
        "  of the {multi} with more than one placed COMDAT, {contiguous} occupy one contiguous run"
    );
    let unmatched = rep.run.unmatched();
    let mut with_code: Vec<(String, usize)> = unmatched
        .iter()
        .filter(|p| p.comdats > 0)
        .map(|p| (p.stem().to_ascii_lowercase(), p.comdats))
        .collect();
    with_code.sort_by(|x, y| y.1.cmp(&x.1).then_with(|| x.0.cmp(&y.0)));
    println!(
        "  {} of the {} members matched nothing: {} with code, {} codeless",
        unmatched.len(),
        rep.run.considered,
        with_code.len(),
        unmatched.len().saturating_sub(with_code.len())
    );
    match &pulled {
        Some(set) => {
            let missed: Vec<&(String, usize)> =
                with_code.iter().filter(|(n, _)| set.contains(n)).collect();
            println!(
                "    our link pulls {} of those with code (candidate misses); {} it does not pull",
                missed.len(),
                with_code.len().saturating_sub(missed.len())
            );
            for (n, k) in missed.iter().take(12) {
                println!("      MISS {k:>3} COMDAT(s)  {n}");
            }
        }
        None => {
            for (n, k) in with_code.iter().take(8) {
                println!("      {k:>3} COMDAT(s)  {n}");
            }
        }
    }
    println!(
        "\n  census by @comp.id, per library: members linked (a kept range, a supplied import, its descriptors) / members (codeless); then the Rich header count"
    );
    for b in &rep.census {
        let per: Vec<String> = b
            .libraries
            .iter()
            .map(|l| format!("{} {}/{} ({})", l.library, l.linked, l.members, l.codeless))
            .collect();
        println!(
            "    {:<22} {}; Rich {}",
            b.label(),
            per.join(", "),
            b.rich.map_or_else(|| "-".to_string(), |n| n.to_string())
        );
    }
    if let Some(j) = &rep.imports {
        println!(
            "\n  imports: {} of {} named import(s) supplied by the import members read ({} short, {} objects); {} by ordinal",
            j.supplied, j.named, j.short, j.objects, j.by_ordinal
        );
        for m in &j.multi {
            println!(
                "    {} supplies {}: {}",
                m.member,
                m.names.len(),
                m.names.join(", ")
            );
        }
        if !j.unsupplied.is_empty() {
            println!(
                "    {} unsupplied: {}",
                j.unsupplied.len(),
                j.unsupplied
                    .iter()
                    .take(8)
                    .cloned()
                    .collect::<Vec<_>>()
                    .join(", ")
            );
        }
    }
    if let Some(w) = &a.write {
        let mut t = Table::of_run(&rep.run);
        t.image = Some(label.clone());
        t.image_sha256 = Some(msvc6_base::digest::sha256_hex(img.bytes()));
        t.archive = rep
            .groups
            .iter()
            .filter_map(|g| {
                g.chosen_copy().map(|c| ArchiveRow {
                    name: g.library.clone(),
                    path: c.path.clone(),
                    sha256: c.sha256.clone(),
                    copies: g.copies.len(),
                })
            })
            .collect();
        let carried = if w.is_file() {
            t.carry(&Table::read(w)?)
        } else {
            0
        };
        msvc6_base::srcio::own_write(w, t.render())
            .with_context(|| format!("writing {}", w.display()))?;
        eprintln!(
            "wrote {} member row(s) and {} import row(s) to {}{}",
            t.member.len(),
            t.import.len(),
            w.display(),
            if carried > 0 {
                format!(", {carried} carried forward")
            } else {
                String::new()
            }
        );
    }
    let rows: Vec<Row<'_>> = present.iter().map(|p| Row::of(p)).collect();
    let summary = Summary {
        text_rva: rep.text_rva,
        text_len: rep.text_len,
        considered: rep.run.considered,
        present: present.len(),
        covered,
        contested,
        by_order,
        by_witness,
        libraries: &rep.groups,
        record: cmp.as_ref(),
        census: &rep.census,
        imports: rep.imports.as_ref(),
        unmatched_with_code: with_code,
    };
    json(&a.json, "lib locate", &label, Some(&summary), &rows)?;
    Ok(match &cmp {
        Some(k) if !k.differences.is_empty() || !k.unrecorded.is_empty() => Outcome::Differs,
        _ => Outcome::Ok,
    })
}
