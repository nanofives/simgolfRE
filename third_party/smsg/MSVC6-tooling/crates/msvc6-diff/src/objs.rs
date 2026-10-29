use anyhow::{Context as _, Result};
use msvc6_base::srcio;
use std::collections::HashMap;
use std::path::{Path, PathBuf};

pub const MAX_LOG: u64 = 64 << 20;

fn ext_is(p: &Path, e: &str) -> bool {
    p.extension().is_some_and(|x| x.eq_ignore_ascii_case(e))
}

fn entries(d: &Path) -> Vec<PathBuf> {
    let mut v: Vec<PathBuf> = std::fs::read_dir(d)
        .map(|rd| rd.filter_map(|e| e.ok().map(|e| e.path())).collect())
        .unwrap_or_default();
    v.sort();
    v
}

fn files_with(d: &Path, e: &str) -> Vec<PathBuf> {
    entries(d).into_iter().filter(|p| ext_is(p, e)).collect()
}

fn read_small(p: &Path) -> Option<String> {
    let len = std::fs::metadata(p).ok()?.len();
    if len > MAX_LOG {
        return None;
    }
    std::fs::read(p).ok().map(|b| srcio::tool_text(&b))
}

fn lower_name(p: &Path) -> Option<String> {
    p.file_name()
        .map(|n| n.to_string_lossy().to_ascii_lowercase())
}

pub fn objs_in(dir: &Path) -> Result<Vec<PathBuf>> {
    let mut v: Vec<PathBuf> = std::fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| ext_is(p, "obj"))
        .collect();
    v.sort();
    if let Some(order) = plg_link_order(dir) {
        v.sort_by_key(|p| {
            lower_name(p)
                .and_then(|n| order.get(&n).copied())
                .unwrap_or(usize::MAX)
        });
    }
    Ok(v)
}

pub fn plg_link_order(dir: &Path) -> Option<HashMap<String, usize>> {
    let up = dir.join("..");
    let mut logs = files_with(&up, "plg");
    if logs.is_empty() {
        let mut subs: Vec<PathBuf> = dir
            .parent()
            .map(workspace_dsps)
            .unwrap_or_default()
            .into_iter()
            .filter_map(|(_, p)| p.parent().map(Path::to_path_buf))
            .collect();
        subs.extend(entries(&up).into_iter().filter(|p| p.is_dir()));
        logs = subs.iter().flat_map(|s| files_with(s, "plg")).collect();
    }
    let text = read_small(logs.first()?)?;
    let mut order = HashMap::new();
    for line in text.lines() {
        let tok = line.trim().trim_matches('"');
        if tok.contains(char::is_whitespace) || !tok.to_ascii_lowercase().ends_with(".obj") {
            continue;
        }
        let name = tok
            .rsplit(['\\', '/'])
            .next()
            .unwrap_or(tok)
            .to_ascii_lowercase();
        let at = order.len();
        order.entry(name).or_insert(at);
    }
    (!order.is_empty()).then_some(order)
}

#[must_use]
pub fn workspace_projects(text: &str) -> Vec<(String, String)> {
    text.lines()
        .filter_map(|l| {
            let r = l.trim_end_matches('\r').strip_prefix("Project: \"")?;
            let (name, rest) = r.split_once("\"=")?;
            let path = rest.split(" - Package Owner").next()?.trim();
            Some((name.to_string(), path.to_string()))
        })
        .collect()
}

#[must_use]
pub fn join_windows(dir: &Path, rel: &str) -> PathBuf {
    rel.split('\\')
        .filter(|c| !c.is_empty() && *c != ".")
        .fold(dir.to_path_buf(), |mut p, c| {
            if c == ".." && p.file_name().is_some() {
                p.pop();
            } else {
                p.push(c);
            }
            p
        })
}

fn same_file(a: &Path, b: &Path) -> bool {
    match (a.canonicalize(), b.canonicalize()) {
        (Ok(x), Ok(y)) => x == y,
        _ => a == b,
    }
}

fn dsps_of(base: &Path, dsws: &[PathBuf]) -> Vec<(String, PathBuf)> {
    let mut out: Vec<(String, PathBuf)> = Vec::new();
    for w in dsws {
        let Some(text) = read_small(w) else {
            continue;
        };
        for (name, rel) in workspace_projects(&text) {
            let p = join_windows(base, &rel);
            if !out.iter().any(|(_, q)| same_file(q, &p)) {
                out.push((name, p));
            }
        }
    }
    out.sort();
    out
}

#[must_use]
pub fn workspace_dsps(dir: &Path) -> Vec<(String, PathBuf)> {
    let mut out = dsps_of(dir, &files_with(dir, "dsw"));
    if out.is_empty()
        && let Some((up, comp)) = dir.parent().and_then(|u| Some((u, u.file_name()?)))
    {
        let w = up.join(format!("{}.dsw", comp.to_string_lossy()));
        if w.is_file() {
            let v = dsps_of(up, &[w]);
            if v.iter()
                .any(|(_, p)| p.parent().is_some_and(|d| same_file(d, dir)))
            {
                out = v;
            }
        }
    }
    out
}

fn one_dsp(dir: &Path) -> Option<PathBuf> {
    let hits = files_with(dir, "dsp");
    match hits.as_slice() {
        [one] => Some(one.clone()),
        _ => None,
    }
}

fn is_static_library(text: &str) -> bool {
    text.lines()
        .any(|l| l.starts_with("# TARGTYPE ") && l.contains("Static Library"))
}

pub fn workspace_libs(dir: &Path) -> Vec<(PathBuf, String)> {
    let (Some(leaf), Some(up)) = (dir.file_name(), dir.parent()) else {
        return Vec::new();
    };
    let listed = workspace_dsps(up);
    let mut subs: Vec<(PathBuf, String)> = if listed.is_empty() {
        let me = up.canonicalize().ok();
        let mut cands = entries(up);
        cands.extend(
            up.parent()
                .map(entries)
                .unwrap_or_default()
                .into_iter()
                .filter(|p| p.canonicalize().ok() != me),
        );
        cands
            .into_iter()
            .filter(|p| p.is_dir())
            .filter_map(|p| {
                let dsp = one_dsp(&p)?;
                if !is_static_library(&read_small(&dsp)?) {
                    return None;
                }
                let n = dsp.file_stem()?.to_str()?.to_string();
                p.join(leaf).is_dir().then(|| (p.join(leaf), n))
            })
            .collect()
    } else {
        let me = one_dsp(up).and_then(|p| p.canonicalize().ok());
        listed
            .into_iter()
            .filter(|(_, p)| p.canonicalize().ok() != me)
            .filter_map(|(_, p)| {
                read_small(&p)?;
                let name = p.file_stem()?.to_str()?.to_string();
                let out = p.parent()?.join(leaf);
                out.is_dir().then_some((out, name))
            })
            .collect()
    };
    subs.sort_by(|a, b| a.1.cmp(&b.1));
    subs
}

fn map_in(out: &Path) -> Option<String> {
    files_with(out, "map").first().and_then(|m| read_small(m))
}

pub fn lib_objs_in(lib: &Path, out: &Path, project: &str) -> Result<Vec<PathBuf>> {
    let mut v = objs_in(lib)?;
    let Some(text) = map_in(out) else {
        return Ok(v);
    };
    let name = project.to_ascii_lowercase();
    let mut first: HashMap<String, u32> = HashMap::new();
    for l in text.lines().filter(|l| l.trim_start().starts_with("0001:")) {
        let t: Vec<&str> = l.split_whitespace().collect();
        let Some((lib_name, member)) = t.last().and_then(|o| o.split_once(':')) else {
            continue;
        };
        if !lib_name.eq_ignore_ascii_case(&name) {
            continue;
        }
        let Some(va) = t
            .iter()
            .skip(2)
            .find(|x| x.len() == 8)
            .and_then(|x| u32::from_str_radix(x, 16).ok())
        else {
            continue;
        };
        let e = first.entry(member.to_ascii_lowercase()).or_insert(va);
        *e = (*e).min(va);
    }
    if first.is_empty() {
        return Ok(v);
    }
    v.sort_by_key(|p| {
        lower_name(p)
            .and_then(|n| first.get(&n).copied())
            .unwrap_or(u32::MAX)
    });
    Ok(v)
}

pub fn build_list(dirs: &[PathBuf], libs: bool) -> Result<Vec<PathBuf>> {
    let mut out = Vec::new();
    for d in dirs {
        out.extend(objs_in(d)?);
        if libs {
            for (lib, name) in workspace_libs(d) {
                out.extend(lib_objs_in(&lib, d, &name)?);
            }
        }
    }
    Ok(out)
}

#[must_use]
pub fn library_projects(dirs: &[PathBuf], libs: bool) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    if !libs {
        return out;
    }
    for (_, name) in dirs.iter().flat_map(|d| workspace_libs(d)) {
        if !out.iter().any(|n| n.eq_ignore_ascii_case(&name)) {
            out.push(name);
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-diff-objs-{tag}-{}", std::process::id()));
        std::fs::remove_dir_all(&d).ok();
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn objects_follow_the_link_order_of_the_build_log() {
        let root = scratch("plg");
        let rel = root.join("Release");
        std::fs::create_dir_all(&rel).unwrap();
        for n in ["Reader.obj", "Track.obj", "Zeta.obj", "Alpha.obj"] {
            srcio::own_write(rel.join(n), b"").unwrap();
        }
        let names = |v: Vec<PathBuf>| -> Vec<String> {
            v.iter()
                .map(|p| p.file_name().unwrap().to_string_lossy().into_owned())
                .collect()
        };
        assert_eq!(
            names(objs_in(&rel).unwrap()),
            ["Alpha.obj", "Reader.obj", "Track.obj", "Zeta.obj"]
        );
        srcio::own_write(
            root.join("app.plg"),
            "/nologo /O2 /c \r\n\"C:\\w\\src\\Track.cpp\"\r\n[\r\n/dll /out:\"Release/app.dll\" \r\n\
             .\\Release\\Zeta.obj\r\n.\\Release\\Track.obj\r\n.\\Release\\Reader.obj\r\n]\r\n",
        )
        .unwrap();
        assert_eq!(
            names(objs_in(&rel).unwrap()),
            ["Zeta.obj", "Track.obj", "Reader.obj", "Alpha.obj"]
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn a_workspace_library_beside_the_project_is_listed_by_archive_name() {
        let root = scratch("ws");
        let app = root.join("app");
        let lib = root.join("engine.lib");
        std::fs::create_dir_all(app.join("Release")).unwrap();
        std::fs::create_dir_all(lib.join("Release")).unwrap();
        srcio::own_write(
            app.join("app.dsw"),
            "Project: \"app\"=.\\app.dsp - Package Owner=<4>\r\nProject: \"engine\"=..\\engine.lib\\engine.dsp - Package Owner=<4>\r\n",
        )
        .unwrap();
        srcio::own_write(
            app.join("app.dsp"),
            "# TARGTYPE \"Win32 (x86) Application\" 0x0101\r\n",
        )
        .unwrap();
        srcio::own_write(
            lib.join("engine.dsp"),
            "# TARGTYPE \"Win32 (x86) Static Library\" 0x0104\r\n",
        )
        .unwrap();
        let libs = workspace_libs(&app.join("Release"));
        assert_eq!(libs.len(), 1);
        assert_eq!(libs[0].1, "engine");
        assert!(libs[0].0.ends_with("engine.lib/Release"));
        let dirs = [app.join("Release"), app.join("Release")];
        assert_eq!(library_projects(&dirs, true), ["engine"]);
        assert!(library_projects(&dirs, false).is_empty());
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn a_windows_path_joins_lexically() {
        assert_eq!(
            join_windows(Path::new("/a/b/c"), "..\\lib\\x.dsp"),
            PathBuf::from("/a/b/lib/x.dsp")
        );
        assert_eq!(
            join_windows(Path::new("/a"), ".\\x.dsp"),
            PathBuf::from("/a/x.dsp")
        );
    }
}
