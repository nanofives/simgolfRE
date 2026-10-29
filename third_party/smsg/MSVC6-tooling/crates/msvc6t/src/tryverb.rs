use crate::Outcome;
use crate::cli;
use crate::diff::{prepare, work_dir};
use crate::image::json;
use anyhow::{Context as _, Result, bail};
use msvc6_diff::env::Env;
use msvc6_diff::oracle::{AUTO, ByteOracle as _, Members, Try};
use msvc6_diff::score::{Score, Seat};
use serde::Serialize;
use std::path::{Path, PathBuf};

#[derive(Debug, Serialize)]
struct Row<'a> {
    rva: u32,
    seat: &'static str,
    symbol: Option<&'a str>,
    name: &'a str,
    len: usize,
    differing: usize,
    comparable: usize,
    masked: usize,
    relocs: usize,
}

#[derive(Debug, Serialize)]
struct Summary {
    verdict: &'static str,
    rva: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    anchor: Option<String>,
    members: usize,
    compared: usize,
    differing: usize,
    pad_skipped: usize,
    unseated: usize,
    misasserted: usize,
    superseded: usize,
    dropped: usize,
}

#[must_use]
pub fn isolated(stem: &str, tag: Option<&str>) -> String {
    match tag {
        Some(t) => {
            let clean: String = t
                .chars()
                .map(|c| if c.is_ascii_alphanumeric() { c } else { '_' })
                .collect();
            format!("{stem}_{clean}")
        }
        None => stem.to_string(),
    }
}

pub fn anchor_rva(env: &Env, given: &str, auto: bool) -> Result<u32> {
    if given.trim().eq_ignore_ascii_case("auto") {
        if !auto {
            bail!("the RVA `auto` derives the anchor from the object and needs --auto-members");
        }
        return Ok(AUTO);
    }
    env.rva_of(given)
}

fn tu_stem(p: &msvc6_project::config::Project, n: usize, id: &str, tu: &Path) -> Option<String> {
    let specs = msvc6_project::spec::resolve(p, n).ok()?;
    msvc6_diff::compile::spec_for(&specs, id, tu).map(|s| s.stem.clone())
}

fn rows(s: &Score) -> Vec<Row<'_>> {
    s.members
        .iter()
        .map(|m| Row {
            rva: m.at,
            seat: m.seat.tag(),
            symbol: m.symbol.as_deref(),
            name: &m.name,
            len: m.comdat.bytes.len(),
            differing: m.bad.len(),
            comparable: m.comparable,
            masked: m.masked,
            relocs: m.relocs,
        })
        .collect()
}

fn count(s: &Score, f: fn(Seat) -> bool) -> usize {
    s.members.iter().filter(|m| f(m.seat)).count()
}

fn lock(dir: &Path) -> Result<std::fs::File> {
    std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))?;
    let p = dir.join(".lock");
    let f = std::fs::File::options()
        .create(true)
        .truncate(false)
        .write(true)
        .open(&p)
        .with_context(|| format!("opening {}", p.display()))?;
    f.lock()
        .with_context(|| format!("locking {}", p.display()))?;
    Ok(f)
}

fn hint_text(rva: u32) -> String {
    if rva == AUTO {
        "auto".to_string()
    } else {
        format!("{rva:#x}")
    }
}

pub fn run(
    config: Option<&Path>,
    a: &cli::TryArgs,
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
        eprintln!("try: the build's include {u} lies under no mount; left out of INCLUDE");
    }
    let id = pr
        .project
        .images
        .get(pr.n)
        .map(|d| d.id.clone())
        .unwrap_or_default();
    let env = Env::load(&pr.project, pr.n)?;
    let rva = anchor_rva(&env, &a.rva, a.auto_members)?;
    if a.members.first() == Some(&rva) {
        bail!(
            "--members lists COMDATs 1..n and {rva:#x} is COMDAT[0], already named by the RVA; drop it, or every later member seats one slot too low"
        );
    }
    let members = if a.auto_members {
        let mut scopes = Vec::new();
        if let Some(s) = tu_stem(
            &pr.project,
            pr.n,
            &id,
            a.args_from.as_deref().unwrap_or(&a.source),
        ) {
            scopes.push(s);
        }
        if !scopes.contains(&pr.unit.stem) {
            scopes.push(pr.unit.stem.clone());
        }
        Members::Auto(scopes)
    } else {
        Members::Asserted(a.members.clone())
    };
    let dir: PathBuf = work_dir(None, &id, &isolated(&pr.unit.stem, a.isolate.as_deref()));
    let _held = lock(&dir)?;
    let c = msvc6_diff::compile::Compiler::new(&pr.project)?;
    let sess = c.runner.session()?;
    let out = c.compile(&sess, &pr.unit, &dir, false)?;
    for l in out.text.lines() {
        let l = l.trim_end();
        if !l.is_empty() && !l.eq_ignore_ascii_case(&pr.unit.file) {
            println!("  cl: {l}");
        }
    }
    let s = Try::new(&env, rva, members).score_file(&out.obj)?;
    if let Some(an) = &s.anchor {
        println!(
            "  ANCHOR: {} -> {:#x}, COMDAT[0] `{}'{}; the object is {}",
            if an.moved && rva != AUTO {
                format!("{rva:#x} (a hint: not COMDAT[0], so moved)")
            } else {
                hint_text(rva)
            },
            an.root,
            an.root_name,
            an.own.as_ref().map_or_else(
                || "; every COMDAT is SELECT_ANY".to_string(),
                |(n, r)| format!("; own first function `{n}' at {r:#x}")
            ),
            out.obj.display()
        );
    }
    if a.isolate.is_some() {
        println!("  ISOLATED: this compile is at {}", out.obj.display());
    }
    println!(
        "{id} {:#x}: {} function(s) in the object",
        s.rva,
        s.members.len()
    );
    for l in s.lines(true) {
        println!("{l}");
    }
    let views = msvc6_diff::blockmap::Views {
        block_map: a.block_map,
        window: a.window,
        align: a.align,
        step: a.align_step,
        keep_regs: a.keep_regs,
    };
    if views.any() {
        for m in &s.members {
            let text = msvc6_diff::blockmap::render_views(&env, m, &views);
            if !text.is_empty() {
                println!("  {:#08x} {}:", m.at, m.name);
                print!("{text}");
            }
        }
    }
    let summary = Summary {
        verdict: s.verdict(),
        rva: s.rva,
        anchor: s.anchor.as_ref().map(|an| an.root_name.clone()),
        members: s.members.len(),
        compared: s.compared,
        differing: s.differing,
        pad_skipped: s.pad_skipped,
        unseated: count(&s, |x| x == Seat::Unseated),
        misasserted: count(&s, |x| x == Seat::Misasserted),
        superseded: count(&s, |x| matches!(x, Seat::Superseded | Seat::Duplicate)),
        dropped: count(&s, |x| x == Seat::Dropped),
    };
    json(&a.json, "try", &id, Some(&summary), &rows(&s))?;
    Ok(if s.identical() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_isolation_tag_becomes_a_directory_name() {
        assert_eq!(isolated("Tile", None), "Tile");
        assert_eq!(isolated("Tile", Some("lane/a b")), "Tile_lane_a_b");
    }
}
