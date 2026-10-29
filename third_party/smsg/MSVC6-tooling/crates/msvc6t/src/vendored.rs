use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_image::coff::Object;
use msvc6_image::pe::Image;
use msvc6_match::closure;
use msvc6_match::input;
use msvc6_match::vendored::{self, Located};
use msvc6_project::spec::{CompileSpec, Origin};
use serde::Serialize;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

#[derive(Debug, Args)]
pub struct VendoredArgs {
    #[command(subcommand)]
    pub verb: VendoredVerb,
}

#[derive(Debug, Subcommand)]
pub enum VendoredVerb {
    #[command(
        about = "Compile an image's vendored libraries from their own source",
        long_about = "Compile every source of each [[image.vendored]] library of IMAGE (its `dir`, its `sources` \
                      globs, default *.c and *.cpp, its `cflags`) with the image's compiler, through the runner and \
                      the compile cache, into OUT/<library>/<stem>.obj. A library that ships with its source is \
                      built, not reconstructed: its bytes are located in the image by `vendored locate`. Prints one \
                      line per source. Every compiler-running verb first checks this binary's provenance. JSON rows: \
                      one per source."
    )]
    Build(BuildArgs),
    #[command(
        about = "Locate the objects of a vendored library in an image by their code bytes",
        long_about = "Place each .text section of each object in OBJS (`vendored build`'s output for the \
                      library, or any directory of objects) in IMAGE's .text: the section's bytes with every field \
                      its own relocations overwrite masked must appear verbatim, found through its longest literal \
                      run (at least 4 bytes); a section found nowhere is retried with its trailing nop pad masked \
                      (an alignment fill CL laid differently), and sections found in several places are assigned \
                      in COMDAT order into the holes of their object's placed span. A section found nowhere is \
                      ABSENT: the original link's /OPT:REF dropped it, or the vendored source is not the source the \
                      image was built from. Prints each object (unique, by order, ambiguous, absent, \
                      unidentifiable, its span and bytes), each absent section, and the blocks the objects tile \
                      (16 bytes of slack between objects; `out of order` when the objects' order is not their \
                      addresses'). JSON rows: one per object."
    )]
    Locate(LocateArgs),
}

#[derive(Debug, Args)]
pub struct BuildArgs {
    pub image: String,
    #[arg(long, value_name = "DIR")]
    pub out: PathBuf,
    #[arg(long, help = "Only this library (repeatable)")]
    pub library: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LocateArgs {
    pub image: String,
    #[arg(long, value_name = "DIR", help = "The library's objects")]
    pub objs: PathBuf,
    #[arg(
        long,
        help = "The library's name in the report (default: the directory's name)"
    )]
    pub library: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ClosureArgs {
    pub image: String,
    #[arg(
        long,
        value_name = "DIR",
        help = "Our objects: every external .text COMDAT they define, resolved through the image's name oracle, \
                is a function the project supplies"
    )]
    pub objs: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "TSV",
        help = "Functions the project supplies: one hexadecimal rva or VA per line (the first column)"
    )]
    pub defined: Vec<PathBuf>,
    #[arg(long, default_value_t = 40, help = "How many rows to print")]
    pub limit: usize,
    #[command(flatten)]
    pub json: JsonArg,
}

fn vendored_specs(p: &Config, image: &str, only: &[String]) -> Result<(String, Vec<CompileSpec>)> {
    let (n, d) = msvc6_project::registry::resolve_id(p, image)?;
    let specs = msvc6_project::spec::resolve(p, n)?;
    let picked: Vec<CompileSpec> = specs
        .specs
        .into_iter()
        .filter(|s| s.origin == Origin::Vendored)
        .filter(|s| only.is_empty() || only.contains(&s.project))
        .collect();
    if picked.is_empty() {
        bail!(
            "image `{}` declares no [[image.vendored]] library{}",
            d.id,
            if only.is_empty() {
                ""
            } else {
                " of those named"
            }
        );
    }
    Ok((d.id.clone(), picked))
}

#[derive(Debug, Serialize)]
struct BuildRow {
    library: String,
    source: String,
    object: String,
    outcome: String,
}

pub fn build(
    config: Option<&Path>,
    a: &BuildArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    guard()?;
    let p = Config::require(config)?;
    let (id, specs) = vendored_specs(&p, &a.image, &a.library)?;
    let d = p
        .images
        .iter()
        .find(|d| d.id == id)
        .context("the image vanished from the config")?;
    let tc = d.compiler.clone().unwrap_or_else(|| "vc6".to_string());
    let c = msvc6_diff::compile::Compiler::new(&p)?;
    let sess = c.runner.session()?;
    let mut rows = Vec::new();
    let mut failed = 0usize;
    for s in &specs {
        let unit = msvc6_diff::compile::Unit::of_spec(s, Path::new(&s.path), &tc)?;
        let dir = a.out.join(&s.project).join(&s.stem);
        let out = c.compile(&sess, &unit, &dir, false);
        let (object, outcome) = match out {
            Ok(o) => {
                let lib = a.out.join(&s.project);
                let dest = lib.join(format!("{}.obj", s.stem));
                std::fs::copy(&o.obj, &dest).with_context(|| {
                    format!("copying {} to {}", o.obj.display(), dest.display())
                })?;
                (dest.display().to_string(), o.outcome.label().to_string())
            }
            Err(e) => {
                failed = failed.saturating_add(1);
                (String::new(), format!("FAILED: {e:#}"))
            }
        };
        println!("{}\t{}\t{outcome}\t{object}", s.project, s.path);
        rows.push(BuildRow {
            library: s.project.clone(),
            source: s.path.clone(),
            object,
            outcome,
        });
    }
    println!(
        "vendored build {id}: {} source(s), {failed} failed",
        rows.len()
    );
    json(&a.json, "vendored build", &id, None::<&()>, &rows)?;
    Ok(if failed > 0 {
        Outcome::Exit(std::process::ExitCode::from(1))
    } else {
        Outcome::Ok
    })
}

fn image_of(p: &Config, id: &str) -> Result<(String, Image)> {
    let (n, _) = msvc6_project::registry::resolve_id(p, id)?;
    let mut r = p.reference(n)?;
    let img =
        Image::parse(std::mem::take(&mut r.bytes)).with_context(|| r.path.display().to_string())?;
    Ok((r.label, img))
}

fn objects_in(dir: &Path) -> Result<Vec<PathBuf>> {
    let mut v: Vec<PathBuf> = std::fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x.eq_ignore_ascii_case("obj")))
        .collect();
    v.sort();
    Ok(v)
}

pub fn locate(config: Option<&Path>, a: &LocateArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let (id, img) = image_of(&p, &a.image)?;
    let t = msvc6_match::text_of(&img)?;
    let library = a.library.clone().unwrap_or_else(|| {
        a.objs.file_name().map_or_else(
            || "vendored".to_string(),
            |n| n.to_string_lossy().into_owned(),
        )
    });
    let mut located: Vec<Located> = Vec::new();
    for o in objects_in(&a.objs)? {
        let name = o
            .file_name()
            .map_or_else(String::new, |n| n.to_string_lossy().into_owned());
        let bytes = input::read(&o, input::MAX_IMAGE)?;
        located.push(vendored::locate_object(&name, &bytes, &t)?);
    }
    if located.is_empty() {
        bail!("{}: no .obj file", a.objs.display());
    }
    let mut absent = 0usize;
    for l in &located {
        println!(
            "{:<24} unique {:>3} by-order {:>3} ambiguous {:>3} absent {:>3} unidentifiable {:>3}  {}  {} byte(s)",
            l.object,
            l.unique,
            l.by_order,
            l.ambiguous,
            l.absent,
            l.unidentifiable,
            match (l.lo, l.hi) {
                (Some(lo), Some(hi)) => format!("{lo:#x}..{hi:#x}"),
                _ => "unplaced".to_string(),
            },
            l.bytes
        );
        for s in l.sections.iter().filter(|s| s.verdict == "absent") {
            absent = absent.saturating_add(1);
            println!(
                "    ABSENT {} ({} byte(s))",
                s.symbol.as_deref().unwrap_or("?"),
                s.len
            );
        }
    }
    let blocks = vendored::blocks(&located, msvc6_match::place::CONTRIBUTION_ALIGN);
    for b in &blocks {
        println!(
            "block {:#x}..{:#x} ({} byte(s)): {} object(s){}",
            b.lo,
            b.hi,
            b.hi.saturating_sub(b.lo),
            b.objects.len(),
            if b.in_order { "" } else { ", out of order" }
        );
    }
    let bytes: u32 = located.iter().map(|l| l.bytes).sum();
    println!(
        "vendored locate {id} {library}: {} object(s), {bytes} byte(s) placed in {} block(s), {absent} section(s) absent",
        located.len(),
        blocks.len()
    );
    json(&a.json, "vendored locate", &id, Some(&blocks), &located)?;
    Ok(Outcome::Ok)
}

fn hex_cell(l: &str) -> Option<u32> {
    let c = l.split(['\t', ' ']).next()?.trim();
    let c = c
        .strip_prefix("0x")
        .or_else(|| c.strip_prefix("0X"))
        .unwrap_or(c);
    u32::from_str_radix(c, 16).ok()
}

pub fn closure_verb(config: Option<&Path>, a: &ClosureArgs) -> Result<Outcome> {
    let p = Config::require(config)?;
    let (n, _) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    let r = p.reference(n)?;
    let img = Image::parse(r.bytes.clone()).with_context(|| r.path.display().to_string())?;
    let base = img.opt.image_base.get();
    let ix = msvc6_project::functions::index_of(&p, n, &r)?;
    let extents: Vec<(u32, u32)> = ix
        .iter()
        .filter(|f| !f.thunk)
        .filter_map(|f| Some((f.rva, f.end?.checked_sub(f.rva)?)))
        .filter(|(_, l)| *l > 0)
        .collect();
    let mut defined: BTreeSet<u32> = BTreeSet::new();
    for t in &a.defined {
        for l in input::text(t, input::MAX_TEXT)?
            .lines()
            .filter(|l| !l.starts_with('#'))
        {
            if let Some(v) = hex_cell(l) {
                defined.insert(if v >= base { v - base } else { v });
            }
        }
    }
    if !a.objs.is_empty() {
        let oracle = msvc6_project::names::oracle_of(&p, n, &r)?;
        for dir in &a.objs {
            for o in objects_in(dir)? {
                let bytes = input::read(&o, input::MAX_IMAGE)?;
                let obj = Object::parse(&bytes).with_context(|| o.display().to_string())?;
                for c in
                    msvc6_match::code::sections(&obj).with_context(|| o.display().to_string())?
                {
                    let Some(s) = c.symbol.as_deref().filter(|_| c.external) else {
                        continue;
                    };
                    for nm in oracle.bodies(s) {
                        defined.insert(nm.rva);
                    }
                }
            }
        }
    }
    let t = msvc6_match::text_of(&img)?;
    let rows = closure::closure(
        &closure::Text {
            bytes: t.bytes,
            rva: t.rva,
            image_base: base,
        },
        &extents,
        &defined,
    );
    let ready: Vec<&closure::Row> = rows.iter().filter(|r| r.blockers.is_empty()).collect();
    println!(
        "{}: {} function(s) with an extent, {} supplied by the project, {} ranked",
        a.image,
        extents.len(),
        extents.len().saturating_sub(rows.len()),
        rows.len()
    );
    println!(
        "ready: {} function(s), {} byte(s): every direct callee is supplied or lies outside the index (a zero says nothing of virtual calls, the `indirect` column)",
        ready.len(),
        ready.iter().map(|r| r.len).sum::<u32>()
    );
    println!("      rva        len   blockers  foreign  indirect  blocking rvas");
    for c in rows.iter().take(a.limit) {
        let list: Vec<String> = c
            .blockers
            .iter()
            .take(6)
            .map(|b| format!("{b:#x}"))
            .collect();
        println!(
            "  {:#010x}  {:5}  {:8}  {:7}  {:8}  {}{}",
            c.rva,
            c.len,
            c.blockers.len(),
            c.foreign,
            c.indirect,
            list.join(" "),
            if c.blockers.len() > 6 {
                format!(" +{}", c.blockers.len() - 6)
            } else {
                String::new()
            }
        );
    }
    if rows.len() > a.limit {
        println!("  ... {} more; --limit shows them", rows.len() - a.limit);
    }
    println!(
        "closure histogram ({} means that many or more):",
        closure::HISTOGRAM_CAP
    );
    for (k, v) in closure::histogram(&rows) {
        println!("  {k:2} blocker(s): {v}");
    }
    json(&a.json, "closure", &a.image, None::<&()>, &rows)?;
    Ok(Outcome::Ok)
}
