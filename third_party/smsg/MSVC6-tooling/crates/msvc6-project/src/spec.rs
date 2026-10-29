use crate::config::Project;
use crate::dsp::{Dsp, FileBlock, Settings, join_windows};
use crate::dsw::{Dsw, DswProject};
use crate::spans::{index, join};
use crate::switches::{self, Class};
use anyhow::{Result, anyhow};
use serde::Serialize;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Language {
    C,
    Cpp,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Origin {
    Dsp,
    Config,
    Vendored,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum PchMode {
    None,
    Auto,
    Create,
    Use,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Pch {
    pub mode: PchMode,
    pub header: Option<String>,
    pub file: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CompileSpec {
    pub image: String,
    pub project: String,
    pub config: Option<String>,
    pub origin: Origin,
    pub dsp: Option<String>,
    pub source: String,
    pub path: String,
    pub stem: String,
    pub language: Language,
    pub switches: Vec<String>,
    pub cflags: Vec<String>,
    pub includes: Vec<String>,
    pub include_paths: Vec<String>,
    pub defines: Vec<String>,
    pub undefines: Vec<String>,
    pub forced_includes: Vec<String>,
    pub pch: Pch,
    pub object: Option<String>,
    pub toolchain: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Refusal {
    pub image: String,
    pub project: String,
    pub source: Option<String>,
    pub why: String,
}

#[derive(Debug, Clone, Default)]
pub struct Specs {
    pub specs: Vec<CompileSpec>,
    pub refused: Vec<Refusal>,
    pub notes: Vec<String>,
}

fn language_of(name: &str, sw: &[String]) -> Language {
    if sw.iter().any(|s| s == "/TP") {
        return Language::Cpp;
    }
    if sw.iter().any(|s| s == "/TC") {
        return Language::C;
    }
    let ext = name
        .rsplit_once('.')
        .map(|(_, e)| e.to_ascii_lowercase())
        .unwrap_or_default();
    if ext == "c" {
        Language::C
    } else {
        Language::Cpp
    }
}

fn pch_of(sw: &[String]) -> Pch {
    let mut p = Pch {
        mode: PchMode::None,
        header: None,
        file: None,
    };
    for s in sw {
        let (mode, rest) = if let Some(r) = s.strip_prefix("/YX") {
            (Some(PchMode::Auto), r)
        } else if let Some(r) = s.strip_prefix("/Yc") {
            (Some(PchMode::Create), r)
        } else if let Some(r) = s.strip_prefix("/Yu") {
            (Some(PchMode::Use), r)
        } else if let Some(r) = s.strip_prefix("/Fp") {
            p.file = Some(r.to_string());
            (None, "")
        } else {
            (None, "")
        };
        if let Some(m) = mode {
            p.mode = m;
            p.header = (!rest.is_empty()).then(|| rest.to_string());
        }
    }
    p
}

#[derive(Debug)]
pub struct Build<'a> {
    pub image: &'a str,
    pub project: &'a str,
    pub config: Option<&'a str>,
    pub origin: Origin,
    pub dsp: Option<&'a Path>,
    pub base: &'a Path,
    pub source: &'a str,
    pub path: PathBuf,
    pub switches: Vec<String>,
    pub objdir: Option<PathBuf>,
    pub toolchain: Option<&'a str>,
}

#[must_use]
pub fn spec(b: Build<'_>) -> CompileSpec {
    let by = |c: Class| -> Vec<String> {
        b.switches
            .iter()
            .filter(|s| switches::class(s) == c)
            .cloned()
            .collect()
    };
    let strip = |v: Vec<String>, n: usize| -> Vec<String> {
        v.into_iter()
            .map(|s| s.get(n..).unwrap_or_default().to_string())
            .collect()
    };
    let includes = strip(by(Class::Include), 2);
    let include_paths = includes
        .iter()
        .map(|i| {
            let p = Path::new(i);
            if p.is_absolute() {
                p.to_path_buf()
            } else {
                join_windows(b.base, i)
            }
            .display()
            .to_string()
        })
        .collect();
    let cflags = b
        .switches
        .iter()
        .filter(|s| {
            !matches!(
                switches::class(s),
                Class::Include | Class::Output | Class::Build
            ) && !s.starts_with("/Fp")
        })
        .cloned()
        .collect();
    let name = b.source.rsplit(['\\', '/']).next().unwrap_or_default();
    let stem = name
        .rsplit_once('.')
        .map_or_else(|| name.to_string(), |(s, _)| s.to_string());
    CompileSpec {
        image: b.image.to_string(),
        project: b.project.to_string(),
        config: b.config.map(str::to_string),
        origin: b.origin,
        dsp: b.dsp.map(|d| d.display().to_string()),
        source: b.source.to_string(),
        path: b.path.display().to_string(),
        language: language_of(name, &b.switches),
        defines: strip(by(Class::Define), 2),
        undefines: strip(by(Class::Undefine), 2),
        forced_includes: strip(by(Class::ForcedInclude), 3),
        pch: pch_of(&b.switches),
        object: object_of(&b.switches, b.base, &stem)
            .or_else(|| b.objdir.map(|d| d.join(format!("{stem}.obj"))))
            .map(|p| p.display().to_string()),
        stem,
        includes,
        include_paths,
        cflags,
        switches: b.switches,
        toolchain: b.toolchain.map(str::to_string),
    }
}

#[must_use]
pub fn object_of(switches: &[String], base: &Path, stem: &str) -> Option<PathBuf> {
    let fo = switches
        .iter()
        .rev()
        .find_map(|s| s.strip_prefix("/Fo"))
        .filter(|v| !v.is_empty())?;
    let p = join_windows(base, fo);
    Some(if fo.ends_with(['\\', '/']) {
        p.join(format!("{stem}.obj"))
    } else if p.extension().is_none() {
        p.with_extension("obj")
    } else {
        p
    })
}

fn config_for(dsp: &Dsp, top_config: &str, top: &str) -> Result<String, String> {
    if dsp.name.eq_ignore_ascii_case(top) {
        return Ok(top_config.to_string());
    }
    let suffix = top_config.split_once(" - ").map_or(top_config, |(_, s)| s);
    let want = format!("{} - {suffix}", dsp.name);
    if dsp.configs.iter().any(|c| c == &want) {
        return Ok(want);
    }
    match dsp.configs.as_slice() {
        [one] => Ok(one.clone()),
        _ => Err(format!(
            "no configuration `{want}` to build with `{top_config}`, and {} to choose from ({})",
            dsp.configs.len(),
            dsp.configs.join(", ")
        )),
    }
}

fn refuse(image: &str, project: &str, source: Option<&str>, why: String) -> Refusal {
    Refusal {
        image: image.to_string(),
        project: project.to_string(),
        source: source.map(str::to_string),
        why,
    }
}

pub fn add_project(out: &mut Specs, image: &str, dsp: &Dsp, config: &str, toolchain: Option<&str>) {
    let s: Settings = dsp.settings(config);
    if !s.unmodelled.is_empty() {
        out.refused
            .push(refuse(image, &dsp.name, None, s.unmodelled.join("; ")));
        return;
    }
    let dir = dsp.dir();
    for f in s.files.iter().filter(|f: &&FileBlock| f.is_tu()) {
        if !f.unmodelled.is_empty() {
            out.refused.push(refuse(
                image,
                &dsp.name,
                Some(&f.source),
                f.unmodelled.join("; "),
            ));
            continue;
        }
        let objdir = f
            .intermediate_dir
            .as_ref()
            .or(s.intermediate_dir.as_ref())
            .map(|d| join_windows(&dir, d));
        out.specs.push(spec(Build {
            image,
            project: &dsp.name,
            config: Some(config),
            origin: Origin::Dsp,
            dsp: Some(&dsp.path),
            base: &dir,
            source: &f.source,
            path: join_windows(&dir, &f.source),
            switches: s.switches(f),
            objdir,
            toolchain,
        }));
    }
}

#[must_use]
pub fn top_of(config: &str) -> &str {
    config.split_once(" - ").map_or(config, |(n, _)| n)
}

fn add_member(
    out: &mut Specs,
    image: &str,
    dsw: &Dsw,
    proj: &DswProject,
    config: &str,
    toolchain: Option<&str>,
) {
    let top = top_of(config);
    let dp = dsw.dsp_path(proj);
    let dsp = match Dsp::read(&dp) {
        Ok(d) => d,
        Err(e) => {
            out.refused
                .push(refuse(image, &proj.name, None, format!("{e:#}")));
            return;
        }
    };
    match config_for(&dsp, config, top) {
        Ok(c) => {
            let suffix = config.split_once(" - ").map_or("", |(_, s)| s);
            if !c.ends_with(suffix) {
                out.notes.push(format!(
                    "{}: built in its one configuration `{c}`",
                    proj.name
                ));
            }
            add_project(out, image, &dsp, &c, toolchain);
        }
        Err(why) => out.refused.push(refuse(
            image,
            &proj.name,
            None,
            format!("{}: {why}", dp.display()),
        )),
    }
}

pub fn add_workspace(
    out: &mut Specs,
    image: &str,
    dsw: &Dsw,
    config: &str,
    toolchain: Option<&str>,
) -> Result<()> {
    for proj in dsw.build_order(top_of(config))? {
        add_member(out, image, dsw, proj, config, toolchain);
    }
    Ok(())
}

fn names_project(dsw: &Dsw, proj: &DswProject, name: &str) -> bool {
    proj.name.eq_ignore_ascii_case(name)
        || dsw
            .dsp_path(proj)
            .file_stem()
            .is_some_and(|s| s.to_string_lossy().eq_ignore_ascii_case(name))
}

pub fn add_named(
    out: &mut Specs,
    image: &str,
    dsw: &Dsw,
    names: &[String],
    config: &str,
    toolchain: Option<&str>,
) -> Result<()> {
    let mut read: Vec<String> = dsw
        .build_order(top_of(config))?
        .iter()
        .map(|q| q.name.clone())
        .collect();
    for name in names {
        let Some(proj) = dsw.projects.iter().find(|q| names_project(dsw, q, name)) else {
            continue;
        };
        if read.iter().any(|r| r == &proj.name) {
            continue;
        }
        read.push(proj.name.clone());
        add_member(out, image, dsw, proj, config, toolchain);
    }
    Ok(())
}

pub fn resolve(p: &Project, n: usize) -> Result<Specs> {
    resolve_with(p, n, &[])
}

pub fn resolve_with(p: &Project, n: usize, also: &[String]) -> Result<Specs> {
    let image = p.images.get(n).ok_or_else(|| anyhow!("no image[{n}]"))?;
    let key = p.image_key(n);
    let tc = image.compiler.as_deref();
    let mut out = Specs::default();
    if let Some(r) = &image.project {
        let pk = join(&key, "project");
        if let Some(w) = &r.dsw {
            let path = p.path_at(&join(&pk, "dsw"), w)?;
            let dsw = Dsw::read(&path).map_err(|e| p.error(&join(&pk, "dsw"), format!("{e:#}")))?;
            add_workspace(&mut out, &image.id, &dsw, &r.config, tc)
                .and_then(|()| add_named(&mut out, &image.id, &dsw, also, &r.config, tc))
                .map_err(|e| p.error(&join(&pk, "config"), format!("{e:#}")))?;
        } else if let Some(d) = &r.dsp {
            let path = p.path_at(&join(&pk, "dsp"), d)?;
            let dsp = Dsp::read(&path).map_err(|e| p.error(&join(&pk, "dsp"), format!("{e:#}")))?;
            add_project(&mut out, &image.id, &dsp, &r.config, tc);
        }
    }
    for (i, t) in image.tu.iter().enumerate() {
        let tk = index(&join(&key, "tu"), i);
        let path = p.path_at(&join(&tk, "source"), &t.source)?;
        let sw = switches::normalize(
            &t.cflags
                .iter()
                .flat_map(|c| switches::tokens(c))
                .collect::<Vec<_>>(),
        );
        let base = path
            .parent()
            .map_or_else(|| p.dir.clone(), Path::to_path_buf);
        out.specs.push(spec(Build {
            image: &image.id,
            project: t.project.as_deref().unwrap_or(&image.id),
            config: None,
            origin: Origin::Config,
            dsp: None,
            base: &base,
            source: &t.source,
            path: path.clone(),
            switches: sw,
            objdir: None,
            toolchain: tc,
        }));
    }
    for (i, v) in image.vendored.iter().enumerate() {
        let vk = index(&join(&key, "vendored"), i);
        let dir = p.path_at(&join(&vk, "dir"), &v.dir)?;
        let globs: Vec<String> = if v.sources.is_empty() {
            vec!["*.c".to_string(), "*.cpp".to_string()]
        } else {
            v.sources.clone()
        };
        let rd = std::fs::read_dir(&dir)
            .map_err(|e| p.error(&join(&vk, "dir"), format!("{}: {e}", dir.display())))?;
        let mut files: Vec<PathBuf> = rd
            .flatten()
            .map(|e| e.path())
            .filter(|f| f.is_file())
            .filter(|f| {
                let n = f
                    .file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_default();
                globs.iter().any(|g| {
                    msvc6_source::glob::matches(&g.to_ascii_lowercase(), &n.to_ascii_lowercase())
                })
            })
            .collect();
        files.sort();
        let sw = switches::normalize(
            &v.cflags
                .iter()
                .flat_map(|c| switches::tokens(c))
                .collect::<Vec<_>>(),
        );
        for f in files {
            let name = f
                .file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_default();
            out.specs.push(spec(Build {
                image: &image.id,
                project: &v.name,
                config: None,
                origin: Origin::Vendored,
                dsp: None,
                base: &dir,
                source: &name,
                path: f.clone(),
                switches: sw.clone(),
                objdir: None,
                toolchain: tc,
            }));
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_spec_splits_its_switches_by_what_they_do() {
        let sw = switches::normalize_line(
            "/nologo /W3 /MT /O2 /D \"WIN32\" /I \"include\" /U X /FIpre.h /Yc\"pch.h\" /Fp\"Release/a.pch\" /Fo\"Release/\" /FD /c /GX",
        );
        let s = spec(Build {
            image: "APP",
            project: "app",
            config: Some("app - Win32 Release"),
            origin: Origin::Dsp,
            dsp: None,
            base: Path::new("/p/app"),
            source: ".\\src\\a.cpp",
            path: PathBuf::from("/p/app/src/a.cpp"),
            switches: sw,
            objdir: Some(PathBuf::from("/p/app/Release")),
            toolchain: Some("vc6"),
        });
        assert_eq!(s.stem, "a");
        assert_eq!(s.language, Language::Cpp);
        assert_eq!(
            s.cflags,
            [
                "/MT", "/O2", "/DWIN32", "/UX", "/FIpre.h", "/Ycpch.h", "/GX"
            ]
        );
        assert_eq!(s.includes, ["include"]);
        assert_eq!(s.include_paths, ["/p/app/include"]);
        assert_eq!(s.defines, ["WIN32"]);
        assert_eq!(s.undefines, ["X"]);
        assert_eq!(s.forced_includes, ["pre.h"]);
        assert_eq!(s.pch.mode, PchMode::Create);
        assert_eq!(s.pch.header.as_deref(), Some("pch.h"));
        assert_eq!(s.pch.file.as_deref(), Some("Release/a.pch"));
        assert_eq!(s.object.as_deref(), Some("/p/app/Release/a.obj"));
        for (fo, want) in [
            ("/Fo\"Release/x.obj\"", "/p/app/Release/x.obj"),
            ("/Fo\"Other\\\\\"", "/p/app/Other/a.obj"),
            ("/FoRelease\\\\b", "/p/app/Release/b.obj"),
        ] {
            let mut sw = s.switches.clone();
            sw.extend(switches::normalize_line(fo));
            assert_eq!(
                object_of(&sw, Path::new("/p/app"), "a")
                    .map(|p| p.display().to_string())
                    .as_deref(),
                Some(want),
                "{fo}"
            );
        }
    }
}
