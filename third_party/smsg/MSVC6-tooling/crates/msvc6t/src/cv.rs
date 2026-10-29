use anyhow::{Context as _, Result, bail};
use msvc6_base::digest::sha256_hex;
use msvc6_base::{rowjson, srcio};
use msvc6_codeview::config::Setup;
use msvc6_codeview::decl::{self, Model};
use msvc6_codeview::diff::{self, ClassPlan, FieldRename, Print};
use msvc6_codeview::record::Record;
use msvc6_codeview::source::Src;
use msvc6_codeview::{apply, debuginfo, names};
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

use crate::Outcome;
use crate::cli::{CvApplyArgs, CvCheckArgs, CvDeclArgs, CvDiffArgs, CvNamesArgs, DebuginfoArgs};
use crate::project::{self, Config};

struct Target {
    label: String,
    img: Image,
    setup: Option<Setup>,
}

fn read_checked(path: &Path, sha256: Option<&str>, what: &str) -> Result<Vec<u8>> {
    let bytes =
        std::fs::read(path).with_context(|| format!("{what}: reading {}", path.display()))?;
    if let Some(want) = sha256 {
        let have = sha256_hex(&bytes);
        if !have.eq_ignore_ascii_case(want) {
            bail!(
                "{what}: {} has sha256 {have}, not the {want} the config declares",
                path.display()
            );
        }
    }
    Ok(bytes)
}

fn target(config: Option<&Config>, arg: &str, with_views: bool) -> Result<Target> {
    if let Some(c) = config
        && let Some((n, d)) = c.image(arg)
        && let Some(spec) = &d.codeview
    {
        let key = format!("{}.codeview", c.image_key(n));
        let setup = spec
            .resolve(&c.dir, &|raw| c.expand(raw), with_views)
            .map_err(|e| c.error(&key, format!("{e:#}")))?;
        let what = format!("image `{}` [image.codeview] record", d.id);
        let bytes = read_checked(&setup.record, setup.sha256.as_deref(), &what)
            .map_err(|e| c.error(&key, format!("{e:#}")))?;
        let img = Image::parse(bytes).with_context(|| setup.record.display().to_string())?;
        return Ok(Target {
            label: d.id.clone(),
            img,
            setup: Some(setup),
        });
    }
    let r = project::resolve(config, arg)?;
    let img = Image::parse(r.bytes).with_context(|| r.path.display().to_string())?;
    Ok(Target {
        label: r.label,
        img,
        setup: None,
    })
}

fn model(t: &Target) -> Result<Model> {
    let rec = Record::from_image(&t.img).with_context(|| t.label.clone())?;
    Model::build(&rec).with_context(|| t.label.clone())
}

#[derive(Debug, Serialize)]
struct DeclSummary {
    classes: usize,
    enums: usize,
    in_scope: usize,
    project_dir: String,
}

pub fn decl(config: Option<&Config>, a: &CvDeclArgs) -> Result<Outcome> {
    let t = target(config, &a.image, false)?;
    let m = model(&t)?;
    print!("{}", decl::render(&m, a.all));
    if let Some(p) = &a.json.json {
        let rows = decl::rows(&m, a.all);
        let summary = DeclSummary {
            classes: m.classes.len(),
            enums: m.enums.len(),
            in_scope: m.scope.len(),
            project_dir: m.project_dir.clone(),
        };
        rowjson::write_with(p, "cv decl", &t.label, Some(&summary), &rows)?;
    }
    Ok(Outcome::Ok)
}

struct Views<'a> {
    setup: &'a Setup,
    tree: &'a msvc6_codeview::view::Tree,
}

fn views<'a>(t: &'a Target, arg: &str) -> Result<Views<'a>> {
    let setup = t.setup.as_ref().with_context(|| {
        format!("`{arg}` has no [image.codeview] table: a view needs the config's sources")
    })?;
    let tree = setup.tree.as_ref().with_context(|| {
        format!("image `{arg}`: [image.codeview] declares no [[image.codeview.view]]")
    })?;
    Ok(Views { setup, tree })
}

fn explained(given: Option<&Path>, setup: &Setup) -> Result<Vec<msvc6_codeview::diff::Explained>> {
    let Some(p) = given
        .map(Path::to_path_buf)
        .or_else(|| setup.explained.clone())
    else {
        return Ok(Vec::new());
    };
    let text = srcio::own_read(&p).with_context(|| format!("reading {}", p.display()))?;
    diff::read_explained(&text, &p.display().to_string(), &setup.reasons)
}

pub fn diff(config: Option<&Config>, a: &CvDiffArgs) -> Result<Outcome> {
    let t = target(config, &a.image, true)?;
    let v = views(&t, &a.image)?;
    let m = model(&t)?;
    let view = v.tree.view(&a.view)?;
    let src = view.read()?;
    let ex = explained(a.explained.as_deref(), v.setup)?;
    let c = diff::compare(&m, &src, &view);
    let rows = diff::filter_class(c.rows, a.class.as_deref());
    let (text, shown, summary) = diff::report(
        &view.name,
        &rows,
        &c.unresolved,
        &ex,
        Print {
            all: a.all,
            counts: a.counts,
        },
    );
    print!("{text}");
    if let Some(p) = &a.json.json {
        rowjson::write_with(p, "cv diff", &t.label, Some(&summary), &shown)?;
    }
    Ok(if summary.unexplained > 0 {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

fn class_of(subject: &str) -> &str {
    subject.split("::").next().unwrap_or("")
}

pub fn apply(config: Option<&Config>, a: &CvApplyArgs) -> Result<Outcome> {
    let t = target(config, &a.image, true)?;
    let v = views(&t, &a.image)?;
    let m = model(&t)?;
    let view = v.tree.view(&a.view)?;
    let src = view.read()?;
    let c = diff::compare(&m, &src, &view);
    let mut others: Vec<Src> = Vec::new();
    for spec in v.tree.views.iter().filter(|s| s.name != view.name) {
        others.push(v.tree.view(&spec.name)?.read()?);
    }
    let protected: BTreeSet<PathBuf> = if a.shared {
        BTreeSet::new()
    } else {
        others
            .iter()
            .flat_map(|s| s.files.iter().cloned())
            .collect()
    };
    let except: BTreeSet<String> = match &a.except {
        Some(p) => {
            apply::read_except(&srcio::read(p).with_context(|| format!("reading {}", p.display()))?)
        }
        None => BTreeSet::new(),
    };
    let only = |c: &str| a.class.as_deref().is_none_or(|x| x == c);
    let done = match a.kind.as_str() {
        "access" | "order" => {
            let plans: Vec<ClassPlan> = c
                .plans
                .into_iter()
                .filter(|p| {
                    only(&p.class)
                        || p.members
                            .first()
                            .is_some_and(|m| only(class_of(&m.subject)))
                })
                .collect();
            apply::apply_plans(
                &view.root,
                &plans,
                a.kind == "access",
                a.kind == "order",
                &protected,
                &except,
            )?
        }
        "field-name" => {
            let renames: Vec<FieldRename> = c
                .renames
                .into_iter()
                .filter(|f| only(&f.class) || only(class_of(&f.subject)))
                .filter(|f| !except.contains(&f.subject))
                .collect();
            let mut srcs: Vec<&Src> = vec![&src];
            srcs.extend(others.iter());
            apply::apply_renames(&view.root, &renames, &srcs, &protected)?
        }
        _ => {
            let rows = diff::filter_class(c.rows, a.class.as_deref());
            apply::apply_fixes(&view.root, &rows, &a.kind, &protected)?
        }
    };
    for l in &done.changed {
        println!("CHANGED {l}");
    }
    for r in &done.refused {
        println!("REFUSED {r}");
    }
    println!(
        "APPLIED {}: {} fix(es) to {}, {} refused",
        a.kind,
        done.count,
        view.name,
        done.refused.len()
    );
    Ok(Outcome::Ok)
}

pub fn names(config: Option<&Config>, a: &CvNamesArgs) -> Result<Outcome> {
    let t = target(config, &a.image, false)?;
    let rec = Record::from_image(&t.img).with_context(|| t.label.clone())?;
    let modules = a
        .modules
        .clone()
        .or_else(|| t.setup.as_ref().and_then(|s| s.modules.clone()));
    let rows = names::attested(&rec, modules.as_deref()).with_context(|| t.label.clone())?;
    for r in &rows {
        println!("{}", r.line());
    }
    if let Some(p) = &a.json.json {
        rowjson::write(p, "cv names", &t.label, &rows)?;
    }
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct InfoSummary {
    files: usize,
    images: usize,
    embedded: usize,
    pdb_references: usize,
    no_record: usize,
    pdb_files: usize,
    skipped_links: Vec<String>,
    unreadable: Vec<String>,
    duplicates: Vec<String>,
}

pub fn debuginfo(a: &DebuginfoArgs) -> Result<Outcome> {
    let scan = debuginfo::scan(&a.paths)?;
    let images = scan.entries.iter().filter(|e| e.kind == "pe").count();
    let embedded = scan.entries.iter().filter(|e| e.embedded).count();
    let pdb_refs = scan
        .entries
        .iter()
        .filter(|e| e.kind == "pe" && !e.embedded && e.format != "none")
        .count();
    let none = scan
        .entries
        .iter()
        .filter(|e| e.kind == "pe" && e.format == "none")
        .count();
    let pdbs = scan
        .entries
        .iter()
        .filter(|e| e.kind == "pdb" || e.kind == "dbg")
        .count();
    let shown: Vec<&debuginfo::Entry> = scan
        .entries
        .iter()
        .filter(|e| a.all || e.kind == "big" || (e.kind != "mz" && e.format != "none"))
        .collect();
    println!(
        "debuginfo: {} file(s) read, {images} image(s): {embedded} with an embedded NB09 / NB11 record, \
         {pdb_refs} naming a PDB, {none} with no CodeView record; {pdbs} PDB or DBG file(s)",
        scan.files
    );
    for e in &shown {
        println!("{}", debuginfo::line(e));
    }
    for l in &scan.skipped_links {
        println!("  skipped a symlinked directory: {l}");
    }
    for d in &scan.duplicates {
        println!("  duplicate: {d}");
    }
    for u in &scan.unreadable {
        println!("  unreadable: {u}");
    }
    if let Some(p) = &a.json.json {
        let summary = InfoSummary {
            files: scan.files,
            images,
            embedded,
            pdb_references: pdb_refs,
            no_record: none,
            pdb_files: pdbs,
            skipped_links: scan.skipped_links.clone(),
            unreadable: scan.unreadable.clone(),
            duplicates: scan.duplicates.clone(),
        };
        rowjson::write_with(p, "debuginfo", "-", Some(&summary), &shown)?;
    }
    Ok(Outcome::Ok)
}

pub fn check(config: Option<&Config>, a: &CvCheckArgs) -> Result<Outcome> {
    let t = target(config, &a.image, false)?;
    let rec = msvc6_codeview::nb::record_of(&t.img).with_context(|| t.label.clone())?;
    let r = msvc6_codeview::check::check(rec).with_context(|| t.label.clone())?;
    for l in msvc6_codeview::check::lines(&r) {
        println!("{l}");
    }
    if let Some(p) = &a.json.json {
        rowjson::write_with(p, "cv check", &t.label, Some(&r), &r.problems)?;
    }
    Ok(if r.identical() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}
