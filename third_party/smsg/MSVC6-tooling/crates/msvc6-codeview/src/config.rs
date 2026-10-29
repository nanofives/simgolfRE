use crate::diff::Reasons;
use crate::view::{Tree, ViewSpec};
use anyhow::{Context as _, Result, bail};
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Component, Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ViewEntry {
    pub name: String,
    pub projects: Vec<String>,
    #[serde(default)]
    pub owner: bool,
    #[serde(default)]
    pub defines: BTreeMap<String, String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CodeviewSpec {
    pub record: String,
    pub sha256: Option<String>,
    pub root: Option<String>,
    pub headers: Option<String>,
    #[serde(default)]
    pub search: Vec<String>,
    pub explained: Option<String>,
    #[serde(default)]
    pub reasons: Vec<String>,
    #[serde(default)]
    pub cite: BTreeMap<String, String>,
    pub modules: Option<String>,
    #[serde(default)]
    pub view: Vec<ViewEntry>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Setup {
    pub record: PathBuf,
    pub sha256: Option<String>,
    pub tree: Option<Tree>,
    pub explained: Option<PathBuf>,
    pub reasons: Reasons,
    pub modules: Option<String>,
}

fn relative(what: &str, p: &str) -> Result<PathBuf> {
    let path = PathBuf::from(p.replace('\\', "/"));
    if path.is_absolute()
        || path
            .components()
            .any(|c| matches!(c, Component::ParentDir | Component::Prefix(_)))
    {
        bail!("{what} `{p}` must be a path inside the tree, relative and without `..`");
    }
    Ok(path)
}

fn path_of(base: &Path, expanded: &str) -> PathBuf {
    let p = PathBuf::from(expanded);
    if p.is_relative() { base.join(p) } else { p }
}

impl CodeviewSpec {
    pub fn from_value(v: &toml::Value) -> Result<Self> {
        let s = v
            .clone()
            .try_into::<Self>()
            .context("the [image.codeview] table")?;
        s.views()?;
        Ok(s)
    }

    pub fn check(&self) -> Result<()> {
        self.views().map(|_| ())
    }

    fn views(&self) -> Result<Option<(&str, PathBuf, Vec<ViewSpec>)>> {
        if let Some(s) = &self.sha256
            && !(s.len() == 64 && s.bytes().all(|b| b.is_ascii_hexdigit()))
        {
            bail!("[image.codeview]: sha256 `{s}` is not 64 hexadecimal digits");
        }
        let mut names = BTreeSet::new();
        for v in &self.view {
            if !names.insert(v.name.as_str()) {
                bail!("[image.codeview]: two views are named `{}`", v.name);
            }
            if v.projects.is_empty() {
                bail!("[image.codeview]: view `{}` names no project", v.name);
            }
        }
        for s in &self.search {
            relative("search pattern", s)?;
        }
        if self.view.is_empty() {
            return Ok(None);
        }
        let root = self
            .root
            .as_deref()
            .context("[image.codeview]: views need a `root`, the source tree")?;
        let headers = self
            .headers
            .as_deref()
            .context("[image.codeview]: views need `headers`, the record headers' directory")?;
        let mut views = Vec::with_capacity(self.view.len());
        for v in &self.view {
            views.push(ViewSpec {
                name: v.name.clone(),
                projects: v
                    .projects
                    .iter()
                    .map(|p| relative(&format!("view `{}`: project", v.name), p))
                    .collect::<Result<_>>()?,
                owner: v.owner,
                defines: v.defines.clone(),
            });
        }
        Ok(Some((root, relative("headers", headers)?, views)))
    }

    pub fn resolve(
        &self,
        base: &Path,
        expand: &dyn Fn(&str) -> Result<String>,
        with_views: bool,
    ) -> Result<Setup> {
        let record = path_of(base, &expand(&self.record)?);
        let views = self.views()?;
        let (tree, explained) = match views {
            Some((root, headers, views)) if with_views => (
                Some(Tree {
                    root: path_of(base, &expand(root)?),
                    headers,
                    search: self.search.clone(),
                    views,
                }),
                self.explained
                    .as_deref()
                    .map(|e| expand(e).map(|x| path_of(base, &x)))
                    .transpose()?,
            ),
            _ => (None, None),
        };
        Ok(Setup {
            record,
            sha256: self.sha256.clone(),
            tree,
            explained,
            reasons: Reasons {
                categories: self.reasons.clone(),
                cite: self.cite.clone(),
            },
            modules: self.modules.clone(),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn spec(text: &str) -> Result<CodeviewSpec> {
        let v: toml::Value = toml::from_str(text)?;
        CodeviewSpec::from_value(&v)
    }

    #[test]
    fn a_codeview_table_is_strict_and_resolves_its_paths() {
        let s = spec(
            "record = \"${R}/a.dll\"\nroot = \"tree\"\nheaders = \"lib/include\"\nsearch = [\"lib/*/include\"]\n[[view]]\nname = \"a.dll\"\nprojects = [\"lib/a\"]\nowner = true\n",
        )
        .unwrap();
        let x = |t: &str| Ok(t.replace("${R}", "/r"));
        let set = s.resolve(Path::new("/cfg"), &x, true).unwrap();
        assert_eq!(set.record, PathBuf::from("/r/a.dll"));
        let tree = set.tree.unwrap();
        assert_eq!(tree.root, PathBuf::from("/cfg/tree"));
        assert_eq!(tree.views[0].projects, [PathBuf::from("lib/a")]);
        assert!(spec("record = \"a\"\nrecrod = \"b\"\n").is_err());
        assert!(spec("record = \"a\"\nsha256 = \"abc\"\n").is_err());
        assert!(
            spec("record = \"a\"\n[[view]]\nname = \"v\"\nprojects = [\"p\"]\nownr = true\n")
                .is_err()
        );
        assert!(spec("record = \"a\"\nroot = \"t\"\nheaders = \"../h\"\n[[view]]\nname = \"v\"\nprojects = [\"p\"]\n").is_err());
        assert!(
            spec("record = \"a\"\nheaders = \"h\"\n[[view]]\nname = \"v\"\nprojects = [\"p\"]\n")
                .is_err()
        );
        assert!(spec("record = \"a\"\nroot = \"t\"\nheaders = \"h\"\n[[view]]\nname = \"v\"\nprojects = [\"p\"]\n[[view]]\nname = \"v\"\nprojects = [\"q\"]\n").is_err());
        let lazy = spec("record = \"${R}/a\"\nroot = \"${UNSET}\"\nheaders = \"h\"\n[[view]]\nname = \"v\"\nprojects = [\"p\"]\n").unwrap();
        let strict = |t: &str| {
            if t.contains("UNSET") {
                anyhow::bail!("unset")
            } else {
                Ok(t.replace("${R}", "/r"))
            }
        };
        assert!(lazy.resolve(Path::new("/c"), &strict, false).is_ok());
        assert!(lazy.resolve(Path::new("/c"), &strict, true).is_err());
    }
}
