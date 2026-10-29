use anyhow::{Context as _, Result, bail};
use serde::Deserialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct ToolchainSpec {
    pub root: String,
    #[serde(default = "default_layout")]
    pub layout: String,
    #[serde(default)]
    pub expect: BTreeMap<String, String>,
    pub bin: Option<String>,
    pub common: Option<String>,
    pub lib: Option<String>,
    pub include: Option<String>,
}

fn default_layout() -> String {
    "vs6".to_string()
}

#[derive(Debug, Clone, Copy, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "lowercase")]
pub enum RunnerKind {
    #[default]
    Wine,
    Container,
    Native,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq, Default)]
#[serde(deny_unknown_fields)]
pub struct DisplaySpec {
    pub name: String,
    #[serde(default)]
    pub start: Vec<String>,
    pub sockets: Option<String>,
    pub locks: Option<String>,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(untagged)]
pub enum DisplayField {
    Name(String),
    Spec(DisplaySpec),
}

impl DisplayField {
    pub fn spec(&self) -> DisplaySpec {
        match self {
            Self::Name(n) => DisplaySpec {
                name: n.clone(),
                start: Vec::new(),
                sockets: None,
                locks: None,
            },
            Self::Spec(s) => s.clone(),
        }
    }
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq, Default)]
#[serde(deny_unknown_fields)]
pub struct RunnerSpec {
    #[serde(default)]
    pub kind: RunnerKind,
    #[serde(default)]
    pub command: Vec<String>,
    pub container: Option<String>,
    pub docker: Option<String>,
    #[serde(default)]
    pub winepath: Vec<String>,
    pub display: Option<DisplayField>,
    pub prefixes: Option<String>,
    pub template: Option<String>,
    pub native_msvcrt: Option<String>,
    pub msvcrt_md5: Option<String>,
    pub jobs: Option<usize>,
    pub job_memory: Option<String>,
    pub tokens: Option<String>,
    pub timeout: Option<String>,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq, Default)]
#[serde(deny_unknown_fields)]
pub struct CacheSpec {
    pub dir: Option<String>,
    pub max: Option<String>,
    pub enabled: Option<bool>,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct MountSpec {
    pub at: String,
    pub from: String,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct StageSpec {
    pub at: String,
    pub from: String,
    #[serde(default)]
    pub files: Vec<String>,
    #[serde(default)]
    pub crlf: Vec<String>,
    #[serde(default)]
    pub rename: BTreeMap<String, String>,
    #[serde(default)]
    pub edit: Vec<EditSpec>,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct EditSpec {
    #[serde(default)]
    pub file: String,
    pub find: String,
    pub replace: String,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct ImplibSpec {
    pub def: String,
    pub out: String,
    pub toolchain: Option<String>,
    #[serde(default)]
    pub link50compat: bool,
    #[serde(default)]
    pub verbatim_symbols: bool,
    #[serde(default)]
    pub import_name: bool,
    pub hints_from: Option<String>,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct MsdevStep {
    pub dir: String,
    pub open: String,
    pub config: String,
    pub toolchain: Option<String>,
    #[serde(default)]
    pub include: Vec<String>,
    #[serde(default)]
    pub lib: Vec<String>,
    #[serde(default)]
    pub path: Vec<String>,
    #[serde(default = "yes")]
    pub rebuild: bool,
    #[serde(default)]
    pub log: Option<String>,
    #[serde(default)]
    pub requires: Vec<String>,
}

fn yes() -> bool {
    true
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct PostSpec {
    pub run: Vec<String>,
    pub cwd: Option<String>,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct BuildSpec {
    pub toolchain: Option<String>,
    #[serde(default)]
    pub mount: Vec<MountSpec>,
    #[serde(default)]
    pub stage: Vec<StageSpec>,
    #[serde(default)]
    pub implib: Vec<ImplibSpec>,
    #[serde(default)]
    pub include: Vec<String>,
    #[serde(default)]
    pub lib: Vec<String>,
    #[serde(default)]
    pub path: Vec<String>,
    pub msdev: Vec<MsdevStep>,
    pub output: String,
    #[serde(default)]
    pub require_crt: Vec<String>,
    #[serde(default)]
    pub post: Vec<PostSpec>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImageSpec {
    pub id: String,
    pub reference: Option<String>,
    pub sha256: Option<String>,
    pub compiler: Option<String>,
    pub linker: Option<String>,
    pub clock: Vec<msvc6_image::clock::Field>,
    pub build: Option<BuildSpec>,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub file: Option<PathBuf>,
    pub dir: PathBuf,
    pub toolchains: BTreeMap<String, ToolchainSpec>,
    pub runner: RunnerSpec,
    pub cache: CacheSpec,
    pub images: Vec<ImageSpec>,
}

impl Config {
    #[must_use]
    pub fn empty(dir: &Path) -> Self {
        Self {
            file: None,
            dir: dir.to_path_buf(),
            toolchains: BTreeMap::new(),
            runner: RunnerSpec::default(),
            cache: CacheSpec::default(),
            images: Vec::new(),
        }
    }

    pub fn image(&self, id: &str) -> Result<&ImageSpec> {
        self.images
            .iter()
            .find(|i| i.id.eq_ignore_ascii_case(id))
            .with_context(|| {
                format!(
                    "no [[image]] with id {id}; the config has {}",
                    self.images
                        .iter()
                        .map(|i| i.id.as_str())
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            })
    }

    pub fn expand(&self, raw: &str) -> Result<String> {
        expand(raw, &|name| {
            if let Some(t) = self.toolchains.get(name) {
                return Some(self.expand_root(t));
            }
            None
        })
    }

    fn expand_root(&self, t: &ToolchainSpec) -> String {
        expand(&t.root, &|_| None).unwrap_or_else(|_| t.root.clone())
    }

    pub fn path(&self, raw: &str) -> Result<PathBuf> {
        let s = self.expand(raw)?;
        let p = PathBuf::from(&s);
        Ok(if p.is_absolute() { p } else { self.dir.join(p) })
    }
}

#[cfg(test)]
#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct TestImage {
    id: String,
    reference: Option<String>,
    compiler: Option<String>,
    #[serde(default)]
    clock: Vec<msvc6_image::clock::Field>,
    build: Option<BuildSpec>,
}

#[cfg(test)]
#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct TestDoc {
    #[serde(default)]
    toolchain: BTreeMap<String, ToolchainSpec>,
    #[serde(default)]
    runner: RunnerSpec,
    #[serde(default)]
    cache: CacheSpec,
    #[serde(default)]
    image: Vec<TestImage>,
}

#[cfg(test)]
pub(crate) fn test_config(text: &str, file: Option<&Path>) -> Config {
    let d: TestDoc = toml::from_str(text).unwrap_or_else(|e| panic!("{e}"));
    Config {
        file: file.map(Path::to_path_buf),
        dir: file
            .and_then(Path::parent)
            .map_or_else(|| PathBuf::from("."), Path::to_path_buf),
        toolchains: d.toolchain,
        runner: d.runner,
        cache: d.cache,
        images: d
            .image
            .into_iter()
            .map(|i| ImageSpec {
                id: i.id,
                reference: i.reference,
                sha256: None,
                compiler: i.compiler,
                linker: None,
                clock: i.clock,
                build: i.build,
            })
            .collect(),
    }
}

pub fn expand(raw: &str, named: &dyn Fn(&str) -> Option<String>) -> Result<String> {
    let mut out = String::with_capacity(raw.len());
    let mut rest = raw;
    while let Some(i) = rest.find('$') {
        out.push_str(rest.get(..i).unwrap_or_default());
        let after = rest.get(i + 1..).unwrap_or_default();
        if let Some(r) = after.strip_prefix('$') {
            out.push('$');
            rest = r;
        } else if let Some(body) = after.strip_prefix('{') {
            let end = body
                .find('}')
                .with_context(|| format!("`{raw}`: an unclosed ${{"))?;
            let inner = body.get(..end).unwrap_or_default();
            let (name, dflt) = match inner.split_once(":-") {
                Some((n, d)) => (n, Some(d)),
                None => (inner, None),
            };
            match (std::env::var(name).ok().filter(|v| !v.is_empty()), dflt) {
                (Some(v), _) => out.push_str(&v),
                (None, Some(d)) => out.push_str(&expand(d, named)?),
                (None, None) => {
                    bail!("`{raw}` names ${{{name}}}, which is not set in the environment")
                }
            }
            rest = body.get(end + 1..).unwrap_or_default();
        } else {
            let len = after
                .find(|c: char| !(c.is_ascii_alphanumeric() || c == '_' || c == '-'))
                .unwrap_or(after.len());
            let name = after.get(..len).unwrap_or_default();
            match named(name) {
                Some(v) if !name.is_empty() => out.push_str(&v),
                _ => bail!("`{raw}` names ${name}, which is no toolchain of this config"),
            }
            rest = after.get(len..).unwrap_or_default();
        }
    }
    out.push_str(rest);
    let home = || std::env::var("HOME").unwrap_or_default();
    if out == "~" {
        return Ok(home());
    }
    if let Some(r) = out.strip_prefix("~/") {
        return Ok(format!("{}/{r}", home()));
    }
    Ok(out)
}

pub fn parse_size(s: &str) -> Result<u64> {
    let t = s.trim();
    let split = t
        .find(|c: char| !(c.is_ascii_digit() || c == '.'))
        .unwrap_or(t.len());
    let num: f64 = t
        .get(..split)
        .unwrap_or_default()
        .parse()
        .with_context(|| format!("`{s}` is not a size such as \"20 GiB\""))?;
    let unit = t
        .get(split..)
        .unwrap_or_default()
        .trim()
        .to_ascii_lowercase();
    let mul: u64 = match unit.as_str() {
        "" | "b" => 1,
        "k" | "kb" | "kib" => 1 << 10,
        "m" | "mb" | "mib" => 1 << 20,
        "g" | "gb" | "gib" => 1 << 30,
        "t" | "tb" | "tib" => 1 << 40,
        _ => bail!("`{s}`: unknown size unit `{unit}`"),
    };
    let whole = num.trunc();
    let frac = num - whole;
    let w = format!("{whole:.0}")
        .parse::<u64>()
        .with_context(|| format!("`{s}` is out of range"))?;
    let f = format!("{:.0}", frac * 1024.0)
        .parse::<u64>()
        .unwrap_or(0)
        .saturating_mul(mul)
        / 1024;
    Ok(w.saturating_mul(mul).saturating_add(f))
}

pub fn parse_duration(s: &str) -> Result<std::time::Duration> {
    let t = s.trim();
    let split = t.find(|c: char| !c.is_ascii_digit()).unwrap_or(t.len());
    let n: u64 = t
        .get(..split)
        .unwrap_or_default()
        .parse()
        .with_context(|| format!("`{s}` is not a duration such as \"30 min\""))?;
    let unit = t
        .get(split..)
        .unwrap_or_default()
        .trim()
        .to_ascii_lowercase();
    let secs = match unit.as_str() {
        "" | "s" | "sec" | "secs" => n,
        "m" | "min" | "mins" => n.saturating_mul(60),
        "h" | "hour" | "hours" => n.saturating_mul(3600),
        _ => bail!("`{s}`: unknown duration unit `{unit}`"),
    };
    Ok(std::time::Duration::from_secs(secs))
}

#[cfg(test)]
mod tests {
    use super::*;

    const CFG: &str = r#"
[toolchain.vc6]
root = "/opt/vs6"
expect = { cl = "12.00.8168" }
[runner]
kind = "container"
container = "w"
winepath = ["$vc6/X"]
[cache]
max = "2 GiB"
[[image]]
id = "APP"
reference = "ref/app.exe"
[image.build]
msdev = [{ dir = 'C:\p', open = "a.dsp", config = "a - Win32 Release" }]
output = 'C:\p\Release\a.exe'
"#;

    #[test]
    fn the_sections_expand_their_paths_and_default_their_fields() {
        let c = test_config(CFG, Some(Path::new("/w/msvc6t.toml")));
        assert_eq!(c.runner.kind, RunnerKind::Container);
        assert_eq!(c.toolchains["vc6"].layout, "vs6");
        assert_eq!(c.expand(&c.runner.winepath[0]).unwrap(), "/opt/vs6/X");
        assert_eq!(
            c.path("ref/app.exe").unwrap(),
            PathBuf::from("/w/ref/app.exe")
        );
        let b = c.image("app").unwrap().build.as_ref().unwrap();
        assert_eq!(b.msdev[0].config, "a - Win32 Release");
        assert!(b.msdev[0].rebuild);
        assert_eq!(
            parse_size(c.cache.max.as_deref().unwrap()).unwrap(),
            2 << 30
        );
        assert!(c.image("nope").is_err());
        assert!(Config::empty(Path::new("/w")).images.is_empty());
    }

    #[test]
    fn an_unknown_field_in_a_section_type_is_refused() {
        let bad = CFG.replace("kind = \"container\"", "kind = \"container\"\nwat = 1");
        let e = toml::from_str::<TestDoc>(&bad).unwrap_err().to_string();
        assert!(e.contains("wat"), "{e}");
        let bad = CFG.replace("output =", "outptu = 1\noutput =");
        let e = toml::from_str::<TestDoc>(&bad).unwrap_err().to_string();
        assert!(e.contains("outptu"), "{e}");
    }

    #[test]
    fn expansion_names_what_is_missing() {
        let e = expand("${MSVC6T_SURELY_UNSET_VAR}/x", &|_| None).unwrap_err();
        assert!(e.to_string().contains("MSVC6T_SURELY_UNSET_VAR"), "{e}");
        let e = expand("$nope/x", &|_| None).unwrap_err();
        assert!(e.to_string().contains("no toolchain"), "{e}");
        assert_eq!(
            expand("a$t/b", &|n| (n == "t").then(|| "T".into())).unwrap(),
            "aT/b"
        );
        assert_eq!(
            expand("${MSVC6T_SURELY_UNSET_VAR:-~/c}/$$x", &|_| None).unwrap(),
            format!("{}/c/$x", std::env::var("HOME").unwrap())
        );
    }

    #[test]
    fn sizes_and_durations_parse() {
        assert_eq!(parse_size("20 GiB").unwrap(), 20 << 30);
        assert_eq!(parse_size("512").unwrap(), 512);
        assert_eq!(parse_size("1.5 KiB").unwrap(), 1536);
        assert!(parse_size("3 parsecs").is_err());
        assert_eq!(parse_duration("30 min").unwrap().as_secs(), 1800);
        assert_eq!(parse_duration("90").unwrap().as_secs(), 90);
    }
}
