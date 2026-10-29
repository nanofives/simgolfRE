use anyhow::{Context as _, Result, bail};
use msvc6_image::archive::Archive;
use msvc6_image::census;
use msvc6_image::libdiff;
use msvc6_image::objcmp::{self, Cmp, Masks};
use serde::Serialize;
use std::path::PathBuf;

use crate::Outcome;
use crate::cli::{LibCensusArgs, LibDiffArgs, ObjDiffArgs};
use crate::image::json;
use crate::project;

#[derive(Debug, Serialize)]
struct PairRow {
    object: String,
    status: &'static str,
    differing: usize,
    first: Option<usize>,
    lengths: Option<(usize, usize)>,
    lines: Vec<(Option<String>, Option<String>)>,
}

pub fn obj_diff(a: &ObjDiffArgs) -> Result<Outcome> {
    let m = Masks {
        labels: a.normalize_labels,
        lines: a.mask_line_numbers,
        pdb_sig: a.mask_pdb_signature,
        debug: a.ignore_debug,
    };
    let pair = a.a.is_file() && a.b.is_file();
    let objs = if pair {
        vec![PathBuf::new()]
    } else {
        objcmp::objects(&a.a)
    };
    if objs.is_empty() {
        bail!("no .obj under {}", a.a.display());
    }
    let mut rows = Vec::new();
    for rel in &objs {
        let (pa, pb) = if pair {
            (a.a.clone(), a.b.clone())
        } else {
            (a.a.join(rel), a.b.join(rel))
        };
        let shown = if pair {
            pa.display().to_string()
        } else {
            rel.display().to_string()
        };
        let mut row = PairRow {
            object: shown.clone(),
            status: "identical",
            differing: 0,
            first: None,
            lengths: None,
            lines: Vec::new(),
        };
        let Ok(bb) = std::fs::read(&pb) else {
            row.status = "missing";
            println!("MISSING  {shown}");
            rows.push(row);
            continue;
        };
        let ba = project::read(&pa)?;
        if m.canonical() {
            if let Some(d) =
                objcmp::compare_canonical(&ba, &bb, m, a.show).with_context(|| shown.clone())?
            {
                row.status = "differs";
                row.differing = d.count;
                println!("DIFFERS  {shown}  {} canonical line(s) differ", d.count);
                for (x, y) in &d.shown {
                    println!("  - {}", x.as_deref().unwrap_or("<none>"));
                    println!("  + {}", y.as_deref().unwrap_or("<none>"));
                }
                row.lines = d.shown;
            }
        } else {
            match objcmp::compare(&ba, &bb, m.pdb_sig) {
                Cmp::Same => {}
                Cmp::Length(x, y) => {
                    row.status = "length";
                    row.lengths = Some((x, y));
                    println!("LENGTH   {shown}  {x} vs {y} bytes");
                }
                Cmp::Bytes(f, n) => {
                    row.status = "bytes";
                    row.first = Some(f);
                    row.differing = n;
                    println!(
                        "BYTES    {shown}  {n} byte(s) differ, first at {f:#x} (of {})",
                        ba.len()
                    );
                }
            }
        }
        rows.push(row);
    }
    let same = rows.iter().filter(|r| r.status == "identical").count();
    let missing = rows.iter().filter(|r| r.status == "missing").count();
    let differ = rows.len().saturating_sub(same).saturating_sub(missing);
    json::<(), _>(&a.json, "obj diff", &a.a.display().to_string(), None, &rows)?;
    let mut masked = vec!["the COFF TimeDateStamp (bytes 4..8)"];
    if m.pdb_sig {
        masked.push("each LF_TYPESERVER signature");
    }
    if m.canonical() {
        masked.push("file pointers, the string table, the .file name and non-COMDAT checksums");
    }
    if m.labels {
        masked.push("$L/$T and author label names");
    }
    if m.lines {
        masked.push("source line numbers");
    }
    if m.debug {
        masked.push("the .debug$ sections");
    }
    println!(
        "{} object(s): {same} identical but for {}; {differ} differ; {missing} missing in {}",
        rows.len(),
        masked.join(", "),
        a.b.display()
    );
    Ok(if differ.saturating_add(missing) == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn first(d: &libdiff::FirstDiff) -> String {
    format!(
        "row {} of {}/{}: ours `{}` / reference `{}`",
        d.row,
        d.ours_rows,
        d.reference_rows,
        d.ours.as_deref().unwrap_or("(none)"),
        d.reference.as_deref().unwrap_or("(none)")
    )
}

pub fn lib_diff(a: &LibDiffArgs) -> Result<Outcome> {
    let (da, db) = (project::read(&a.ours)?, project::read(&a.reference)?);
    let ours = Archive::parse(&da).with_context(|| a.ours.display().to_string())?;
    let reference = Archive::parse(&db).with_context(|| a.reference.display().to_string())?;
    let d = libdiff::diff(&ours, &reference)?;
    json(
        &a.json,
        "lib diff",
        &a.ours.display().to_string(),
        Some(&d.order),
        &d.rows,
    )?;
    println!(
        "{} ({} member(s)) against {} ({} member(s))",
        a.ours.display(),
        d.ours_members,
        a.reference.display(),
        d.reference_members
    );
    match &d.order {
        None => println!(
            "  member order: IDENTICAL, {} names in the same order",
            d.ours_members
        ),
        Some(o) => println!("  member order: DIFFERS at {}", first(o)),
    }
    let mut same = 0usize;
    for r in &d.rows {
        match r.status {
            "identical" => same = same.saturating_add(1),
            "only-ours" => println!("  only ours: {}", r.member),
            "only-reference" => println!("  only the reference's: {}", r.member),
            _ => {
                println!(
                    "  {}: @comp.id ours {} / reference {}",
                    r.member,
                    r.ours_comp_id
                        .map_or_else(|| "none".to_string(), |c| format!("{c:#x}")),
                    r.reference_comp_id
                        .map_or_else(|| "none".to_string(), |c| format!("{c:#x}"))
                );
                for (k, x) in &r.differences {
                    println!("    {k}: {}", first(x));
                }
            }
        }
    }
    println!(
        "  {same} member(s) identical in sections, relocations, COMDATs and symbols (dates, COFF timestamps \
         and .file records excluded)"
    );
    println!(
        "VERDICT: {}",
        if d.identical() {
            "IDENTICAL MEMBERS IN THE REFERENCE'S ORDER"
        } else {
            "DIFFERENT"
        }
    );
    Ok(if d.identical() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

pub fn lib_census(a: &LibCensusArgs) -> Result<Outcome> {
    let d = project::read(&a.archive)?;
    let ar = Archive::parse(&d).with_context(|| a.archive.display().to_string())?;
    let rows = census::members(&ar)?;
    let keep = |r: &census::MemberRow| {
        a.only
            .as_deref()
            .is_none_or(|f| r.member.contains(f) || r.defines.iter().any(|s| s.contains(f)))
    };
    let label = a.archive.display().to_string();
    if a.members {
        let shown: Vec<&census::MemberRow> = rows.iter().filter(|r| keep(r)).collect();
        json::<(), _>(&a.json, "lib census", &label, None, &shown)?;
        println!("{label}: every member in archive order: stamp, sections, defined externals");
        for r in &shown {
            println!("  #{:<4} {:<24} {}", r.index, r.member, r.stamp());
            if let Some(i) = &r.import {
                println!("         {i}");
            } else if !r.sections.is_empty() {
                println!(
                    "         {}",
                    r.sections
                        .iter()
                        .map(|(n, s)| format!("{n}:{s:#x}"))
                        .collect::<Vec<_>>()
                        .join(" ")
                );
            }
            if r.import.is_none() && !r.defines.is_empty() {
                println!("         defines: {}", r.defines.join(" "));
            }
        }
        println!("  {} member(s) shown", shown.len());
        return Ok(Outcome::Ok);
    }
    let kept: Vec<census::MemberRow> = rows.iter().filter(|r| keep(r)).cloned().collect();
    let buckets = census::buckets(&kept);
    json::<(), _>(&a.json, "lib census", &label, None, &buckets)?;
    println!(
        "{label}: {} member(s), bucketed by the @comp.id prodid a Rich header counts them under",
        kept.len()
    );
    for b in &buckets {
        println!("\n  {}: {} member(s)", b.label, b.members.len());
        for chunk in b.members.chunks(8) {
            println!("    {}", chunk.join(" "));
        }
    }
    let codeless: Vec<&census::BucketRow> =
        buckets.iter().filter(|b| !b.codeless.is_empty()).collect();
    println!("\n  members with no .text* section, which a placement by code bytes cannot see:");
    if codeless.is_empty() {
        println!("    none");
    }
    for b in codeless {
        println!("    {}: {} member(s)", b.label, b.codeless.len());
        for chunk in b.codeless.chunks(8) {
            println!("      {}", chunk.join(" "));
        }
    }
    Ok(Outcome::Ok)
}
