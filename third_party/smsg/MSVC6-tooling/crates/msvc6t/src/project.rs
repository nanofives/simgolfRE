use crate::Outcome;
use crate::cli;
use crate::image::json;
use anyhow::{Context as _, Result, bail};
use msvc6_project::check::{self, Status};
use msvc6_project::dsp::Dsp;
use msvc6_project::dsw::Dsw;
use msvc6_project::spec::{CompileSpec, Specs};
use serde::Serialize;
use std::path::Path;
use std::process::ExitCode;

pub use msvc6_project::config::Project as Config;
pub use msvc6_project::registry::{Resolved, read, resolve};

pub fn load(explicit: Option<&Path>) -> Result<Option<Config>> {
    Config::find(explicit)
}

fn parse_rva(s: &str) -> Result<u32> {
    let t = s
        .strip_prefix("0x")
        .or_else(|| s.strip_prefix("0X"))
        .with_context(|| format!("`{s}` is not a 0x-hexadecimal rva"))?;
    u32::from_str_radix(t, 16).with_context(|| format!("`{s}` is not a 0x-hexadecimal rva"))
}

pub fn init(a: &cli::InitArgs) -> Result<Outcome> {
    let o = msvc6_project::init::Options {
        images: a.images.clone(),
        ids: a.ids.clone(),
        dsw: a.dsw.clone(),
        dsp: a.dsp.clone(),
        config: a.project_config.clone(),
        vc6: a.vc6.clone(),
    };
    let text = msvc6_project::init::write(&a.out, &o, a.force)?;
    print!("{text}");
    eprintln!("wrote {}", a.out.display());
    Ok(Outcome::Ok)
}

pub fn check(config: Option<&Path>, a: &cli::CheckArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let only = a
        .images
        .iter()
        .map(|id| msvc6_project::registry::resolve_id(&p, id).map(|(n, _)| n))
        .collect::<Result<Vec<_>>>()?;
    let r = check::run(&p, &only);
    for row in &r.rows {
        println!(
            "{:<4} {:<22} {:<14} {}",
            match row.status {
                Status::Ok => "ok",
                Status::Fail => "FAIL",
                Status::Skip => "skip",
            },
            row.subject,
            row.check,
            row.detail
        );
    }
    let bad = r.failures();
    let skipped = r.skipped();
    println!(
        "check: {} of {} check(s) ok, {bad} failed{}",
        r.rows.len().saturating_sub(bad).saturating_sub(skipped),
        r.rows.len(),
        if skipped > 0 {
            format!(", {skipped} skipped (an optional input is unset)")
        } else {
            String::new()
        }
    );
    json(
        &a.json,
        "check",
        &p.file.display().to_string(),
        None::<&()>,
        &r.rows,
    )?;
    Ok(if bad > 0 {
        Outcome::Exit(ExitCode::from(1))
    } else {
        Outcome::Ok
    })
}

fn default_config(dsp: &Dsp, given: Option<&String>) -> Result<String> {
    match given {
        Some(c) => Ok(c.clone()),
        None => dsp.default_config.clone().with_context(|| {
            format!(
                "{}: no CFG= line; pass --project-config",
                dsp.path.display()
            )
        }),
    }
}

fn bare_specs(a: &cli::DspShowArgs) -> Result<(String, Specs)> {
    let image = "(none)".to_string();
    let mut out = Specs::default();
    if let Some(w) = &a.dsw {
        let dsw = Dsw::read(w)?;
        let first = dsw
            .projects
            .first()
            .with_context(|| format!("{}: lists no project", w.display()))?;
        let config = default_config(&Dsp::read(&dsw.dsp_path(first))?, a.project_config.as_ref())?;
        msvc6_project::spec::add_workspace(&mut out, &image, &dsw, &config, None)?;
    } else if let Some(d) = &a.dsp {
        let dsp = Dsp::read(d)?;
        let config = default_config(&dsp, a.project_config.as_ref())?;
        msvc6_project::spec::add_project(&mut out, &image, &dsp, &config, None);
    } else {
        bail!("name an IMAGE, or --dsw or --dsp");
    }
    Ok((image, out))
}

fn print_spec(s: &CompileSpec) {
    println!(
        "{} {} {}",
        s.project,
        s.source,
        s.config
            .as_deref()
            .map_or_else(String::new, |c| format!("[{c}]"))
    );
    println!("    switches {}", s.switches.join(" "));
    println!("    cflags   {}", s.cflags.join(" "));
    if !s.includes.is_empty() {
        println!(
            "    includes {}",
            s.includes
                .iter()
                .zip(s.include_paths.iter())
                .map(|(i, p)| format!("{i} ({p})"))
                .collect::<Vec<_>>()
                .join(" ")
        );
    }
    if let Some(o) = &s.object {
        println!("    object   {o}");
    }
}

pub fn dsp_show(config: Option<&Path>, a: &cli::DspShowArgs) -> Result<Outcome> {
    let (label, specs) = match &a.image {
        Some(id) => {
            let p = Config::require(config)?;
            let (n, d) = msvc6_project::registry::resolve_id(&p, id)?;
            if d.project.is_none() && d.tu.is_empty() && d.vendored.is_empty() {
                return Err(p.error(
                    &p.image_key(n),
                    format!(
                        "image `{}` declares no project, [[image.tu]] or [[image.vendored]]",
                        d.id
                    ),
                ));
            }
            (d.id.clone(), msvc6_project::spec::resolve(&p, n)?)
        }
        None => bare_specs(a)?,
    };
    let keep =
        |s: &CompileSpec| a.tus.is_empty() || a.tus.iter().any(|t| t.eq_ignore_ascii_case(&s.stem));
    let shown: Vec<&CompileSpec> = specs.specs.iter().filter(|s| keep(s)).collect();
    for s in &shown {
        print_spec(s);
    }
    let missing: Vec<&String> = a
        .tus
        .iter()
        .filter(|t| !specs.specs.iter().any(|s| s.stem.eq_ignore_ascii_case(t)))
        .collect();
    for t in &missing {
        println!("NO TU `{t}` in {label}'s projects");
    }
    for n in &specs.notes {
        println!("note: {n}");
    }
    for r in &specs.refused {
        println!(
            "REFUSED {} {}: {}",
            r.project,
            r.source.as_deref().unwrap_or("(every file)"),
            r.why
        );
    }
    let projects: std::collections::BTreeSet<&str> =
        shown.iter().map(|s| s.project.as_str()).collect();
    println!(
        "dsp show {label}: {} TU(s) in {} project(s), {} refused",
        shown.len(),
        projects.len(),
        specs.refused.len()
    );
    let rows: Vec<CompileSpec> = shown.into_iter().cloned().collect();
    json(&a.json, "dsp show", &label, None::<&()>, &rows)?;
    Ok(if specs.refused.is_empty() && missing.is_empty() {
        Outcome::Ok
    } else {
        Outcome::Exit(ExitCode::from(1))
    })
}

#[derive(Debug, Serialize)]
struct Summary<'a, T: Serialize> {
    total: usize,
    providers: &'a [T],
}

fn notes(n: &[String]) -> String {
    if n.is_empty() {
        String::new()
    } else {
        format!("; {}", n.join("; "))
    }
}

pub fn functions(config: Option<&Path>, a: &cli::FunctionsArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    if d.functions.is_empty() {
        return Err(p.error(
            &p.image_key(n),
            format!("image `{}` declares no `functions`", d.id),
        ));
    }
    let r = p.reference(n)?;
    let ix = msvc6_project::functions::index_of(&p, n, &r)?;
    let rows: Vec<&msvc6_project::functions::Function> = match &a.at {
        Some(s) => ix.containing(parse_rva(s)?).into_iter().collect(),
        None => ix.iter().collect(),
    };
    if !a.summary {
        for f in &rows {
            println!(
                "{:#010x} {:<10} {:<5} {:<12} {}{}",
                f.rva,
                f.end
                    .map_or_else(|| "?".to_string(), |e| format!("{e:#010x}")),
                if f.thunk { "thunk" } else { "" },
                f.provider,
                f.name.as_deref().unwrap_or("-"),
                f.object
                    .as_deref()
                    .map_or_else(String::new, |o| format!("  {o}"))
            );
        }
    }
    for s in &ix.stats {
        println!(
            "{}: {} {} record(s), {} new{}",
            s.provider,
            s.source,
            s.records,
            s.new,
            notes(&s.notes)
        );
    }
    println!("functions {}: {} function(s)", d.id, ix.len());
    let summary = Summary {
        total: ix.len(),
        providers: &ix.stats,
    };
    json(&a.json, "functions", &d.id, Some(&summary), &rows)?;
    if a.at.is_some() && rows.is_empty() {
        return Ok(Outcome::Exit(ExitCode::from(1)));
    }
    Ok(Outcome::Ok)
}

pub fn names(config: Option<&Path>, a: &cli::NamesArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    if d.names.is_empty() {
        return Err(p.error(
            &p.image_key(n),
            format!("image `{}` declares no `names`", d.id),
        ));
    }
    let r = p.reference(n)?;
    let o = msvc6_project::names::oracle_of(&p, n, &r)?;
    let at = a.at.as_deref().map(parse_rva).transpose()?;
    let rows: Vec<&msvc6_project::names::Name> = o
        .iter()
        .filter(|x| at.is_none_or(|r| x.rva == r))
        .filter(|x| a.name.as_ref().is_none_or(|m| &x.name == m))
        .collect();
    if !a.summary {
        for x in &rows {
            println!(
                "{:#010x} {:<9} {:<14} {}{}{}",
                x.rva,
                x.kind.key(),
                x.provider,
                x.name,
                x.size.map_or_else(String::new, |s| format!("  size {s}")),
                x.scope
                    .as_deref()
                    .map_or_else(String::new, |s| format!("  scope {s}"))
            );
        }
    }
    for s in &o.stats {
        println!(
            "{}: {} {} name(s){}",
            s.provider,
            s.source,
            s.names,
            notes(&s.notes)
        );
    }
    println!("names {}: {} name(s), {} shown", d.id, o.len(), rows.len());
    let summary = Summary {
        total: o.len(),
        providers: &o.stats,
    };
    json(&a.json, "names", &d.id, Some(&summary), &rows)?;
    if (at.is_some() || a.name.is_some()) && rows.is_empty() {
        return Ok(Outcome::Exit(ExitCode::from(1)));
    }
    Ok(Outcome::Ok)
}
