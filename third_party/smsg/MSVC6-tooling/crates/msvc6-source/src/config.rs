use crate::strip::Layout;
use crate::syntax::Syntax;
use anyhow::{Context as _, Result, bail};
use serde::Deserialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Source {
    pub root: Option<String>,
    #[serde(default)]
    pub exclude: Vec<String>,
    #[serde(default)]
    pub syntax: BTreeMap<String, Syntax>,
    #[serde(default)]
    pub keep: Vec<Keep>,
    #[serde(default)]
    pub layout: BTreeMap<String, Layout>,
    #[serde(default)]
    pub macros: Macros,
    #[serde(default)]
    pub format: Option<Format>,
    #[serde(default)]
    pub prove: Prove,
    #[serde(default)]
    pub placeholders: Placeholders,
    #[serde(default)]
    pub guard: Guard,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Keep {
    pub path: String,
    pub line: usize,
    pub sha256: String,
    #[serde(default)]
    pub reason: String,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Macros {
    #[serde(default)]
    pub function: Vec<String>,
    #[serde(default)]
    pub object: Vec<String>,
    #[serde(default)]
    pub stringize: Vec<String>,
    #[serde(default)]
    pub crtdbg_map_alloc: bool,
    #[serde(default = "yes")]
    pub harvest: bool,
}

fn yes() -> bool {
    true
}

impl Default for Macros {
    fn default() -> Self {
        Self {
            function: Vec::new(),
            object: Vec::new(),
            stringize: Vec::new(),
            crtdbg_map_alloc: false,
            harvest: true,
        }
    }
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Format {
    pub command: Vec<String>,
    #[serde(default)]
    pub files: Vec<String>,
    #[serde(default)]
    pub ignore: Vec<String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Prove {
    #[serde(default)]
    pub compile: Vec<String>,
    #[serde(default)]
    pub env: BTreeMap<String, String>,
    pub jobs: Option<usize>,
    pub work: Option<String>,
    #[serde(default)]
    pub tu: Vec<TuSpec>,
    #[serde(default)]
    pub unbuilt: Vec<String>,
    #[serde(default)]
    pub include: Vec<String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TuSpec {
    #[serde(default)]
    pub source: String,
    #[serde(default)]
    pub sources: Vec<String>,
    pub cwd: Option<String>,
    #[serde(default)]
    pub cflags: Vec<String>,
    #[serde(default)]
    pub include: Vec<String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Placeholders {
    #[serde(default)]
    pub stems: Vec<String>,
    #[serde(default)]
    pub pad_suffixes: Vec<String>,
    #[serde(default)]
    pub pad_prefixes: Vec<String>,
    #[serde(default)]
    pub value_numbers: Vec<String>,
    #[serde(default)]
    pub descriptive_numbers: Vec<String>,
    #[serde(default)]
    pub allow: Vec<String>,
    #[serde(default)]
    pub files: Vec<String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Guard {
    #[serde(default)]
    pub image: Vec<GuardImage>,
    #[serde(default)]
    pub prototypes: bool,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct GuardImage {
    pub name: String,
    pub dirs: Vec<String>,
    #[serde(default)]
    pub exe: bool,
}

#[derive(Debug, Clone, Default)]
pub struct Loaded {
    pub source: Source,
    pub dir: PathBuf,
    pub path: Option<PathBuf>,
    pub roots: BTreeMap<String, String>,
}

pub fn expand(s: &str) -> Result<String> {
    expand_with(s, &BTreeMap::new())
}

pub fn expand_with(s: &str, roots: &BTreeMap<String, String>) -> Result<String> {
    let mut out = String::with_capacity(s.len());
    let mut rest = s;
    while let Some(i) = rest.find('$') {
        out.push_str(rest.get(..i).unwrap_or_default());
        let after = rest.get(i + 1..).unwrap_or_default();
        if let Some(r) = after.strip_prefix('$') {
            out.push('$');
            rest = r;
            continue;
        }
        let Some(body) = after.strip_prefix('{') else {
            let len = after
                .find(|c: char| !(c.is_ascii_alphanumeric() || c == '_' || c == '-'))
                .unwrap_or(after.len());
            let name = after.get(..len).unwrap_or_default();
            match roots.get(name) {
                Some(r) if !name.is_empty() => out.push_str(r),
                _ => bail!(
                    "`{s}` names ${name}, which is no toolchain of this config: spell a root $<id>, a variable \
                     ${{VAR}}, or $$ for a $"
                ),
            }
            rest = after.get(len..).unwrap_or_default();
            continue;
        };
        let Some(end) = body.find('}') else {
            bail!("`{s}`: a `${{` with no `}}`");
        };
        let inner = body.get(..end).unwrap_or_default();
        let (name, dflt) = match inner.split_once(":-") {
            Some((n, d)) => (n, Some(d)),
            None => (inner, None),
        };
        match (std::env::var(name).ok().filter(|v| !v.is_empty()), dflt) {
            (Some(v), _) => out.push_str(&v),
            (None, Some(d)) => out.push_str(&expand_with(d, roots)?),
            (None, None) => {
                bail!("`{s}` names ${{{name}}}, which is not set in the environment")
            }
        }
        rest = body.get(end + 1..).unwrap_or_default();
    }
    out.push_str(rest);
    let home = || std::env::var("HOME").context("`~` with no HOME");
    if out == "~" {
        return home();
    }
    if let Some(r) = out.strip_prefix("~/") {
        return Ok(format!("{}/{r}", home()?));
    }
    Ok(out)
}

impl Loaded {
    pub fn expand(&self, s: &str) -> Result<String> {
        expand_with(s, &self.roots)
    }

    pub fn path_of(&self, s: &str) -> Result<PathBuf> {
        let p = PathBuf::from(self.expand(s)?);
        Ok(if p.is_absolute() { p } else { self.dir.join(p) })
    }

    pub fn root(&self, given: Option<&Path>) -> Result<PathBuf> {
        let r = match (given, &self.source.root) {
            (Some(g), _) => g.to_path_buf(),
            (None, Some(r)) => self.path_of(r)?,
            (None, None) => std::env::current_dir()?,
        };
        std::fs::canonicalize(&r).with_context(|| format!("the source root {}", r.display()))
    }

    #[must_use]
    pub fn syntax_of(&self, rel: &str) -> Syntax {
        self.source
            .syntax
            .iter()
            .find(|(g, _)| crate::glob::matches(g, rel))
            .map_or_else(|| crate::syntax::builtin(Path::new(rel)), |(_, s)| *s)
    }

    #[must_use]
    pub fn excluded(&self, rel: &str) -> bool {
        self.source
            .exclude
            .iter()
            .any(|e| crate::glob::under(e, rel) || crate::glob::matches(e, rel))
    }

    #[must_use]
    pub fn keep_for(&self, rel: &str) -> Vec<crate::strip::KeepLine> {
        self.source
            .keep
            .iter()
            .filter(|k| k.path == rel)
            .map(|k| crate::strip::KeepLine {
                line: k.line,
                sha256: k.sha256.clone(),
            })
            .collect()
    }
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct SourceDoc {
    source: Option<Source>,
}

pub fn parse(text: &str, dir: &Path, path: Option<&Path>) -> Result<Loaded> {
    let doc: SourceDoc =
        toml::from_str(text).context("reading a [source] table (the only table this reads)")?;
    Ok(Loaded {
        source: doc.source.unwrap_or_default(),
        dir: dir.to_path_buf(),
        path: path.map(Path::to_path_buf),
        roots: BTreeMap::new(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_source_table_is_strict() {
        let ok = parse(
            "[source]\nexclude = [\"tools\"]\nsyntax = { \"*.ksy\" = \"hash-inline\" }\n[[source.keep]]\npath = \"a.sh\"\nline = 1\nsha256 = \"00\"\n",
            Path::new("/x"),
            None,
        )
        .unwrap();
        assert_eq!(ok.source.exclude, ["tools"]);
        assert_eq!(ok.syntax_of("spec/a.ksy"), Syntax::HashInline);
        assert_eq!(ok.syntax_of("src/a.cpp"), Syntax::C);
        assert!(ok.excluded("tools/x.rs"));
        assert!(!ok.excluded("toolsx/x.rs"));
        assert_eq!(ok.keep_for("a.sh").len(), 1);
        let bad = parse("[source]\nexclud = []\n", Path::new("/x"), None);
        assert!(format!("{:#}", bad.unwrap_err()).contains("exclud"));
        let top = parse("[sourse]\nexclude = []\n", Path::new("/x"), None);
        assert!(format!("{:#}", top.unwrap_err()).contains("sourse"));
    }

    #[test]
    fn variables_expand_and_an_unset_one_is_named() {
        let e = expand("${MSVC6T_SOURCE_TEST_UNSET_VAR}/x").unwrap_err();
        assert!(e.to_string().contains("MSVC6T_SOURCE_TEST_UNSET_VAR"));
        let home = std::env::var("HOME").unwrap();
        assert_eq!(expand("~/a").unwrap(), format!("{home}/a"));
        assert_eq!(expand("${HOME}/b").unwrap(), format!("{home}/b"));
        assert_eq!(
            expand("${MSVC6T_SOURCE_TEST_UNSET_VAR:-~/c}/$$d").unwrap(),
            format!("{home}/c/$d")
        );
        assert!(
            expand("$vc6/x")
                .unwrap_err()
                .to_string()
                .contains("no toolchain of this config")
        );
        let roots = BTreeMap::from([("vc6".to_string(), "/opt/vc".to_string())]);
        assert_eq!(
            expand_with("$vc6/VC98/x", &roots).unwrap(),
            "/opt/vc/VC98/x"
        );
        assert!(expand_with("$vc5/x", &roots).is_err());
    }
}
