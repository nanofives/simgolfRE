use crate::attach::Shipped;
use anyhow::{Result, bail};
use serde::Deserialize;
use std::collections::BTreeSet;

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct PdbSpec {
    pub file: Option<String>,
    pub twin: Option<String>,
    pub name: Option<String>,
    #[serde(default)]
    pub shipped: Vec<Shipped>,
    pub paths: Option<PathsSpec>,
    pub relocate: Option<RelocateSpec>,
    pub author: Option<AuthorSpec>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(untagged)]
pub enum TableSpec {
    File(String),
    Columns { file: String, va: String },
}

impl TableSpec {
    #[must_use]
    pub fn file(&self) -> &str {
        match self {
            Self::File(f) | Self::Columns { file: f, .. } => f,
        }
    }

    #[must_use]
    pub fn va(&self) -> &str {
        match self {
            Self::File(_) => "va",
            Self::Columns { va, .. } => va,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AuthorSpec {
    pub root: String,
    pub rev: Option<String>,
    pub functions: String,
    pub data: TableSpec,
    pub globals: Option<String>,
    pub library: Option<String>,
    pub locals: Option<String>,
    pub index: String,
    pub types: Option<String>,
    #[serde(default)]
    pub headers: Vec<String>,
    #[serde(default)]
    pub optional_headers: Vec<String>,
    pub modules: Option<String>,
}

impl AuthorSpec {
    pub fn check(&self) -> Result<()> {
        let tables = [
            ("functions", Some(self.functions.as_str())),
            ("data", Some(self.data.file())),
            ("globals", self.globals.as_deref()),
            ("library", self.library.as_deref()),
            ("locals", self.locals.as_deref()),
            ("index", Some(self.index.as_str())),
            ("types", self.types.as_deref()),
            ("modules", self.modules.as_deref()),
        ];
        for (k, v) in tables {
            if let Some(v) = v
                && !product_rel(v)
            {
                bail!("author.{k} `{v}` must be a `/`-separated path inside author.root");
            }
        }
        for h in self.headers.iter().chain(&self.optional_headers) {
            if !product_rel(h) {
                bail!("author header `{h}` must be a `/`-separated path inside author.types");
            }
        }
        if self.data.va().is_empty() {
            bail!("author.data.va names no column");
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct RelocateSpec {
    pub report_key: Option<String>,
}

impl RelocateSpec {
    pub fn check(&self) -> Result<()> {
        if let Some(k) = &self.report_key
            && (k.is_empty() || k.chars().any(|c| c.is_whitespace() || c.is_control()))
        {
            bail!("relocate.report_key `{k}` must be a non-empty JSON key without white space");
        }
        Ok(())
    }

    #[must_use]
    pub fn key(spec: Option<&Self>) -> &str {
        spec.and_then(|s| s.report_key.as_deref())
            .unwrap_or(crate::report::KEY)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Lane {
    pub at: String,
    pub product: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AttestedDir {
    pub product: String,
    pub dir: String,
    #[serde(default)]
    pub output: bool,
    pub evidence: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum Record {
    Line,
    Build,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AttestedFiles {
    pub product: String,
    pub dir: String,
    pub names: Vec<String>,
    pub record: Option<Record>,
    pub evidence: String,
}

fn default_configs() -> Vec<String> {
    vec!["Release".into(), "Debug".into()]
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct PathsSpec {
    pub product: String,
    pub product_win: Option<String>,
    #[serde(default = "default_configs")]
    pub configs: Vec<String>,
    pub lanes: Vec<Lane>,
    #[serde(default)]
    pub attested: Vec<AttestedDir>,
    #[serde(default)]
    pub files: Vec<AttestedFiles>,
}

#[must_use]
pub fn is_md5(s: &str) -> bool {
    s.len() == 32 && s.bytes().all(|b| b.is_ascii_hexdigit())
}

fn windows_abs(p: &str) -> bool {
    let b = p.as_bytes();
    matches!(b, [d, b':', b'\\', ..] if d.is_ascii_alphabetic())
}

fn product_rel(p: &str) -> bool {
    !p.is_empty()
        && !p.starts_with('/')
        && !p.contains('\\')
        && p.split('/').all(|c| !c.is_empty() && c != "." && c != "..")
}

impl PdbSpec {
    pub fn check(&self) -> Result<()> {
        let mut seen = BTreeSet::new();
        for (i, s) in self.shipped.iter().enumerate() {
            if !is_md5(&s.md5) {
                bail!(
                    "shipped[{i}].md5 `{}` is not an md5: 32 hexadecimal digits",
                    s.md5
                );
            }
            if s.what.trim().is_empty() {
                bail!("shipped[{i}].what is empty: say which file the md5 is");
            }
            if !seen.insert(s.md5.to_ascii_lowercase()) {
                bail!("shipped[{i}].md5 {} is listed twice", s.md5);
            }
        }
        if let Some(n) = &self.name
            && (n.is_empty() || !n.is_ascii() || n.contains(['\\', '/', '\0']))
        {
            bail!("name `{n}` must be a plain ASCII file name");
        }
        if let Some(p) = &self.paths {
            p.check()?;
        }
        if let Some(r) = &self.relocate {
            r.check()?;
        }
        if let Some(a) = &self.author {
            a.check()?;
        }
        Ok(())
    }
}

impl PathsSpec {
    pub fn check(&self) -> Result<()> {
        if self.lanes.is_empty() {
            bail!("paths.lanes is empty: the image's own project is the first lane");
        }
        for (i, l) in self.lanes.iter().enumerate() {
            if !windows_abs(&l.at) {
                bail!(
                    "paths.lanes[{i}].at `{}` is not an absolute Windows path (`C:\\...`)",
                    l.at
                );
            }
            if !product_rel(&l.product) {
                bail!(
                    "paths.lanes[{i}].product `{}` must be a path inside the product, `/`-separated",
                    l.product
                );
            }
        }
        if let Some(w) = &self.product_win
            && !windows_abs(w)
        {
            bail!("paths.product_win `{w}` is not an absolute Windows path");
        }
        for (i, a) in self.attested.iter().enumerate() {
            if !product_rel(&a.product) || !windows_abs(&a.dir) || a.evidence.trim().is_empty() {
                bail!(
                    "paths.attested[{i}] needs a product path inside the product, an absolute Windows dir and its evidence"
                );
            }
        }
        for (i, f) in self.files.iter().enumerate() {
            if !product_rel(&f.product) || !windows_abs(&f.dir) || f.evidence.trim().is_empty() {
                bail!(
                    "paths.files[{i}] needs a product path inside the product, an absolute Windows dir and its evidence"
                );
            }
            if f.names.is_empty()
                || f.names
                    .iter()
                    .any(|n| n.is_empty() || n.contains(['\\', '/']))
            {
                bail!("paths.files[{i}].names must be bare file names");
            }
        }
        if self
            .configs
            .iter()
            .any(|c| c.is_empty() || c.contains(['\\', '/']))
        {
            bail!("paths.configs must be bare directory names (`Release`, `Debug`)");
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn spec(text: &str) -> Result<PdbSpec> {
        let v: PdbSpec = toml::from_str(text)?;
        v.check()?;
        Ok(v)
    }

    #[test]
    fn a_pdb_table_loads_and_its_mistakes_are_refused() {
        let ok = spec(
            "file = \"x.pdb\"\nshipped = [{ md5 = \"0123456789abcdef0123456789abcdef\", what = \"x\" }]\n\
             [paths]\nproduct = \"p\"\nlanes = [{ at = 'C:\\w', product = \"a/b\" }]\n",
        )
        .unwrap();
        assert_eq!(ok.paths.unwrap().configs, ["Release", "Debug"]);
        assert!(spec("shipped = [{ md5 = \"abc\", what = \"x\" }]").is_err());
        assert!(
            spec("shipped = [{ md5 = \"0123456789abcdef0123456789abcdef\", what = \"\" }]")
                .is_err()
        );
        assert!(spec("bogus = 1").is_err());
        assert!(spec("name = 'a\\b.pdb'").is_err());
        assert!(
            spec("[paths]\nproduct = \"p\"\nlanes = [{ at = 'w', product = \"a\" }]\n").is_err()
        );
        assert!(
            spec("[paths]\nproduct = \"p\"\nlanes = [{ at = 'C:\\w', product = \"../a\" }]\n")
                .is_err()
        );
        assert!(spec("[paths]\nproduct = \"p\"\nlanes = []\n").is_err());
        let two = "shipped = [{ md5 = \"0123456789abcdef0123456789abcdef\", what = \"x\" }, { md5 = \"0123456789ABCDEF0123456789abcdef\", what = \"y\" }]";
        assert!(spec(two).is_err());
    }
}
