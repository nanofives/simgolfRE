use crate::source::{Src, parse_preprocessed, preprocess_with, strip_comments};
use anyhow::{Context as _, Result, bail};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Component, Path, PathBuf};

pub const INCLUDE_DEPTH: usize = 32;
pub const GLOB_DIRS: usize = 4096;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ViewSpec {
    pub name: String,
    pub projects: Vec<PathBuf>,
    pub owner: bool,
    pub defines: BTreeMap<String, String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tree {
    pub root: PathBuf,
    pub headers: PathBuf,
    pub search: Vec<String>,
    pub views: Vec<ViewSpec>,
}

#[derive(Debug, Clone)]
pub struct View {
    pub name: String,
    pub owner: bool,
    pub root: PathBuf,
    pub defines: BTreeMap<String, String>,
    pub search: Vec<PathBuf>,
    pub sources: Vec<PathBuf>,
    pub record_dir: PathBuf,
}

type Dsp = (BTreeMap<String, String>, Vec<String>, Vec<String>);

#[must_use]
pub fn read_dsp(text: &str) -> Dsp {
    let mut defs = BTreeMap::new();
    let mut incs = Vec::new();
    let mut srcs = Vec::new();
    let mut seen_cpp = false;
    for l in text.lines() {
        let l = l.trim();
        if let Some(rest) = l.strip_prefix("# ADD CPP ") {
            if seen_cpp {
                continue;
            }
            seen_cpp = true;
            let words = split_flags(rest);
            let mut it = words.iter();
            while let Some(w) = it.next() {
                if w == "/D" {
                    if let Some(v) = it.next() {
                        let (k, val) = v.split_once('=').unwrap_or((v.as_str(), "1"));
                        defs.insert(k.to_string(), val.to_string());
                    }
                } else if w == "/I"
                    && let Some(v) = it.next()
                {
                    incs.push(v.clone());
                }
            }
        } else if let Some(s) = l.strip_prefix("SOURCE=") {
            srcs.push(s.trim_matches('"').replace('\\', "/"));
        }
    }
    (defs, incs, srcs)
}

fn split_flags(s: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut q = false;
    for c in s.chars() {
        match c {
            '"' => q = !q,
            c if c.is_whitespace() && !q => {
                if !cur.is_empty() {
                    out.push(std::mem::take(&mut cur));
                }
            }
            c => cur.push(c),
        }
    }
    if !cur.is_empty() {
        out.push(cur);
    }
    out
}

pub fn glob_dirs(root: &Path, pattern: &str) -> Result<Vec<PathBuf>> {
    let mut level = vec![root.to_path_buf()];
    for part in pattern.split('/').filter(|p| !p.is_empty() && *p != ".") {
        if part == ".." {
            bail!("the search pattern `{pattern}` climbs out of the tree with `..`");
        }
        let mut next = Vec::new();
        for d in &level {
            if part == "*" {
                let Ok(rd) = std::fs::read_dir(d) else {
                    continue;
                };
                let mut names: Vec<PathBuf> = rd
                    .filter_map(|e| e.ok().map(|e| e.path()))
                    .filter(|p| p.is_dir())
                    .collect();
                names.sort();
                next.extend(names);
            } else {
                let p = d.join(part);
                if p.is_dir() {
                    next.push(p);
                }
            }
            if next.len() > GLOB_DIRS {
                bail!("the search pattern `{pattern}` matches over {GLOB_DIRS} directories");
            }
        }
        level = next;
    }
    Ok(level)
}

#[must_use]
pub fn lexical(p: &Path) -> PathBuf {
    let mut out = PathBuf::new();
    for c in p.components() {
        match c {
            Component::ParentDir => {
                if matches!(out.components().next_back(), Some(Component::Normal(_))) {
                    out.pop();
                } else {
                    out.push("..");
                }
            }
            Component::CurDir => {}
            other => out.push(other.as_os_str()),
        }
    }
    out
}
fn first_dsp(dir: &Path) -> Result<PathBuf> {
    let mut dsps: Vec<PathBuf> = std::fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|f| f.extension().is_some_and(|x| x == "dsp"))
        .collect();
    dsps.sort();
    dsps.into_iter()
        .next()
        .with_context(|| format!("no .dsp in {}", dir.display()))
}

impl Tree {
    #[must_use]
    pub fn spec(&self, name: &str) -> Option<&ViewSpec> {
        self.views.iter().find(|v| v.name == name)
    }

    #[must_use]
    pub fn names(&self) -> Vec<&str> {
        self.views.iter().map(|v| v.name.as_str()).collect()
    }

    pub fn view(&self, name: &str) -> Result<View> {
        let spec = self.spec(name).with_context(|| {
            format!(
                "no view `{name}`; the config's views are {}",
                self.names().join(", ")
            )
        })?;
        let root = self
            .root
            .canonicalize()
            .with_context(|| format!("no source tree {}", self.root.display()))?;
        let record_dir = root.join(&self.headers);
        if !record_dir.is_dir() {
            bail!(
                "{} holds no {}: the record headers the config names",
                root.display(),
                self.headers.display()
            );
        }
        let mut v = View {
            name: spec.name.clone(),
            owner: spec.owner,
            root: root.clone(),
            defines: BTreeMap::new(),
            search: Vec::new(),
            sources: Vec::new(),
            record_dir,
        };
        for (k, rel) in spec.projects.iter().enumerate() {
            let p = root.join(rel);
            let p = p
                .canonicalize()
                .with_context(|| format!("view `{name}`: no project directory {}", p.display()))?;
            if !p.starts_with(&root) {
                bail!(
                    "view `{name}`: the project {} lies outside the tree {}",
                    p.display(),
                    root.display()
                );
            }
            let dsp = first_dsp(&p)?;
            let text = msvc6_base::srcio::read(&dsp)
                .with_context(|| format!("reading {}", dsp.display()))?;
            let (defs, incs, srcs) = read_dsp(&text);
            if k == 0 {
                v.defines = defs;
            }
            for i in incs {
                v.search.push(lexical(&p.join(i.replace('\\', "/"))));
            }
            for s in srcs {
                let f = p.join(&s);
                if f.extension().is_some_and(|x| x == "cpp" || x == "c") {
                    v.sources.push(f);
                }
            }
        }
        for (k, val) in &spec.defines {
            v.defines.insert(k.clone(), val.clone());
        }
        for pat in &self.search {
            v.search.extend(glob_dirs(&root, pat)?);
        }
        Ok(v)
    }
}

impl View {
    #[must_use]
    pub fn headers_rel(&self) -> PathBuf {
        self.record_dir
            .strip_prefix(&self.root)
            .unwrap_or(&self.record_dir)
            .to_path_buf()
    }

    pub fn read(&self) -> Result<Src> {
        let mut src = Src::default();
        let mut defs = self.defines.clone();
        let mut seen = BTreeSet::new();
        let mut errs = Vec::new();
        for s in &self.sources {
            self.read_tu(s, &mut defs, &mut seen, &mut src, &mut errs, 0);
        }
        if let Some(e) = errs.first() {
            bail!("{e} ({} file(s) unreadable)", errs.len());
        }
        Ok(src)
    }

    fn read_tu(
        &self,
        path: &Path,
        defs: &mut BTreeMap<String, String>,
        seen: &mut BTreeSet<PathBuf>,
        src: &mut Src,
        errs: &mut Vec<String>,
        depth: usize,
    ) {
        let key = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
        if depth > INCLUDE_DEPTH || !seen.insert(key) {
            return;
        }
        let bytes = match std::fs::read(path) {
            Ok(t) => t,
            Err(e) => {
                errs.push(format!("reading {}: {e}", path.display()));
                return;
            }
        };
        let stripped = strip_comments(&bytes);
        let pre = preprocess_with(&stripped, defs, &mut |name, d| {
            if let Some(f) = self.resolve(path, name) {
                self.read_tu(&f, d, seen, src, errs, depth.saturating_add(1));
            }
        });
        let shown = path.strip_prefix(&self.root).unwrap_or(path);
        parse_preprocessed(&pre, shown, src);
        src.files.insert(shown.to_path_buf());
        for (k, v) in defs.iter() {
            src.macros.entry(k.clone()).or_insert_with(|| v.clone());
        }
    }

    fn resolve(&self, from: &Path, name: &str) -> Option<PathBuf> {
        let name = name.replace('\\', "/");
        let mut dirs: Vec<PathBuf> = from.parent().map(Path::to_path_buf).into_iter().collect();
        dirs.extend(self.search.iter().cloned());
        dirs.into_iter()
            .map(|d| d.join(&name))
            .find(|f| f.is_file() && f.starts_with(&self.root))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_dsp_gives_defines_includes_and_sources() {
        let dsp = "# ADD BASE CPP /D \"X\"\n# ADD CPP /nologo /O2 /I \"include\" /D \"WIN32\" /D \"_USRDLL\" /D N=2 /c\nSOURCE=.\\src\\a.cpp\n# ADD CPP /GX\n";
        let (d, i, s) = read_dsp(dsp);
        assert_eq!(d.get("_USRDLL").map(String::as_str), Some("1"));
        assert_eq!(d.get("N").map(String::as_str), Some("2"));
        assert!(!d.contains_key("X"));
        assert_eq!(i, ["include"]);
        assert_eq!(s, ["./src/a.cpp"]);
    }

    #[test]
    fn a_search_pattern_lists_directories_sorted_and_never_climbs() {
        let d = std::env::temp_dir().join(format!("msvc6-codeview-glob-{}", std::process::id()));
        for p in ["a/z/include", "a/b/include", "a/c/other"] {
            std::fs::create_dir_all(d.join(p)).unwrap();
        }
        let got = glob_dirs(&d, "a/*/include").unwrap();
        assert_eq!(got, [d.join("a/b/include"), d.join("a/z/include")]);
        assert!(glob_dirs(&d, "../x").is_err());
        std::fs::remove_dir_all(&d).unwrap();
    }
}
