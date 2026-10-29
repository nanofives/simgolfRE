use crate::spans::{Spans, index, join};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::config::CodeviewSpec;
use msvc6_image::clock::Field;
use msvc6_image::postlink::Row;
use msvc6_source::config::{Loaded, Source};
use msvc6_toolchain::config::{BuildSpec, CacheSpec, RunnerKind, RunnerSpec, ToolchainSpec};
use serde::Deserialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub const FILE: &str = "msvc6t.toml";
pub const ENV: &str = "MSVC6T_CONFIG";
pub const SCHEMA: u32 = 1;

pub const TABLES: [&str; 7] = [
    "schema",
    "toolchain",
    "runner",
    "cache",
    "image",
    "reccmp",
    "source",
];

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
struct Doc {
    schema: u32,
    #[serde(default)]
    optional: Vec<String>,
    #[serde(default)]
    toolchain: BTreeMap<String, ToolchainSpec>,
    #[serde(default)]
    runner: RunnerSpec,
    #[serde(default)]
    cache: CacheSpec,
    #[serde(default)]
    image: Vec<ImageDecl>,
    reccmp: Option<Reccmp>,
    source: Option<Source>,
}

struct Spec<T>(std::marker::PhantomData<T>);

impl<T: TryFrom<String, Error = String>> serde::de::Visitor<'_> for Spec<T> {
    type Value = T;

    fn expecting(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("a string")
    }

    fn visit_str<E: serde::de::Error>(self, v: &str) -> Result<T, E> {
        T::try_from(v.to_string()).map_err(E::custom)
    }
}

fn spec_str<'de, D: serde::Deserializer<'de>, T: TryFrom<String, Error = String>>(
    d: D,
) -> Result<T, D::Error> {
    d.deserialize_str(Spec(std::marker::PhantomData))
}

impl<'de> Deserialize<'de> for Sha256 {
    fn deserialize<D: serde::Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        spec_str(d)
    }
}

impl<'de> Deserialize<'de> for FunctionSource {
    fn deserialize<D: serde::Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        spec_str(d)
    }
}

impl<'de> Deserialize<'de> for NameSource {
    fn deserialize<D: serde::Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        spec_str(d)
    }
}

impl<'de> Deserialize<'de> for DroppedSource {
    fn deserialize<D: serde::Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        spec_str(d)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Sha256(String);

impl Sha256 {
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl TryFrom<String> for Sha256 {
    type Error = String;

    fn try_from(s: String) -> Result<Self, String> {
        if s.len() == 64 && s.bytes().all(|b| b.is_ascii_hexdigit()) {
            Ok(Self(s.to_ascii_lowercase()))
        } else {
            Err(format!(
                "`{s}` is not a sha256: 64 hexadecimal digits, {} given",
                s.len()
            ))
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FunctionSource {
    GhidraJson(String),
    Map(Option<String>),
    ReccmpCsv(String),
    Pdb(Option<String>),
    Discover,
}

pub const FUNCTION_KINDS: [&str; 5] = [
    "ghidra-json:<file>",
    "map[:<file>]",
    "reccmp-csv:<file>",
    "pdb[:<file>]",
    "discover",
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NameSource {
    Tsv(String),
    Map(Option<String>),
    Exports,
    Imports,
    ReccmpMarkers(String),
    ReccmpCsv(String),
    Pdb(Option<String>),
}

pub const NAME_KINDS: [&str; 7] = [
    "tsv:<file>",
    "map[:<file>]",
    "exports",
    "imports",
    "reccmp-markers:<dir>",
    "reccmp-csv:<file>",
    "pdb[:<file>]",
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DroppedSource {
    Tsv(String),
}

pub const DROPPED_KINDS: [&str; 1] = ["tsv:<file>"];

#[must_use]
pub fn absent_note(var: &str) -> String {
    format!("absent: ${{{var}}} is not set, and the config lists it as optional")
}

impl FunctionSource {
    #[must_use]
    pub fn input(&self) -> Option<&str> {
        match self {
            Self::GhidraJson(f) | Self::ReccmpCsv(f) => Some(f),
            Self::Map(f) | Self::Pdb(f) => f.as_deref(),
            Self::Discover => None,
        }
    }

    #[must_use]
    pub fn kind(&self) -> &'static str {
        match self {
            Self::GhidraJson(_) => "ghidra-json",
            Self::Map(_) => "map",
            Self::ReccmpCsv(_) => "reccmp-csv",
            Self::Pdb(_) => "pdb",
            Self::Discover => "discover",
        }
    }
}

impl NameSource {
    #[must_use]
    pub fn input(&self) -> Option<&str> {
        match self {
            Self::Tsv(f) | Self::ReccmpMarkers(f) | Self::ReccmpCsv(f) => Some(f),
            Self::Map(f) | Self::Pdb(f) => f.as_deref(),
            Self::Exports | Self::Imports => None,
        }
    }

    #[must_use]
    pub fn kind(&self) -> &'static str {
        match self {
            Self::Tsv(_) => "tsv",
            Self::Map(_) => "map",
            Self::Exports => "exports",
            Self::Imports => "imports",
            Self::ReccmpMarkers(_) => "reccmp-markers",
            Self::ReccmpCsv(_) => "reccmp-csv",
            Self::Pdb(_) => "pdb",
        }
    }
}

impl DroppedSource {
    #[must_use]
    pub fn input(&self) -> Option<&str> {
        match self {
            Self::Tsv(f) => Some(f),
        }
    }
}

fn split_spec(s: &str) -> (&str, Option<&str>) {
    match s.split_once(':') {
        Some((k, a)) => (k, Some(a)),
        None => (s, None),
    }
}

fn arg_of(kind: &str, a: Option<&str>) -> Result<String, String> {
    match a {
        Some(a) if !a.is_empty() => Ok(a.to_string()),
        _ => Err(format!("`{kind}` needs a path: `{kind}:<path>`")),
    }
}

fn no_arg(kind: &str, a: Option<&str>) -> Result<(), String> {
    match a {
        None => Ok(()),
        Some(a) => Err(format!("`{kind}` takes no argument, `{a}` given")),
    }
}

impl TryFrom<String> for FunctionSource {
    type Error = String;

    fn try_from(s: String) -> Result<Self, String> {
        let (k, a) = split_spec(&s);
        Ok(match k {
            "ghidra-json" => Self::GhidraJson(arg_of(k, a)?),
            "map" => Self::Map(a.filter(|a| !a.is_empty()).map(str::to_string)),
            "reccmp-csv" => Self::ReccmpCsv(arg_of(k, a)?),
            "discover" => {
                no_arg(k, a)?;
                Self::Discover
            }
            "pdb" => Self::Pdb(a.filter(|a| !a.is_empty()).map(str::to_string)),
            _ => {
                return Err(format!(
                    "`{s}` names no function index; the providers are {}",
                    FUNCTION_KINDS.join(", ")
                ));
            }
        })
    }
}

impl TryFrom<String> for NameSource {
    type Error = String;

    fn try_from(s: String) -> Result<Self, String> {
        let (k, a) = split_spec(&s);
        Ok(match k {
            "tsv" => Self::Tsv(arg_of(k, a)?),
            "map" => Self::Map(a.filter(|a| !a.is_empty()).map(str::to_string)),
            "exports" => {
                no_arg(k, a)?;
                Self::Exports
            }
            "imports" => {
                no_arg(k, a)?;
                Self::Imports
            }
            "reccmp-markers" => Self::ReccmpMarkers(arg_of(k, a)?),
            "reccmp-csv" => Self::ReccmpCsv(arg_of(k, a)?),
            "pdb" => Self::Pdb(a.filter(|a| !a.is_empty()).map(str::to_string)),
            _ => {
                return Err(format!(
                    "`{s}` names no name oracle; the providers are {}",
                    NAME_KINDS.join(", ")
                ));
            }
        })
    }
}

impl TryFrom<String> for DroppedSource {
    type Error = String;

    fn try_from(s: String) -> Result<Self, String> {
        let (k, a) = split_spec(&s);
        match k {
            "tsv" => Ok(Self::Tsv(arg_of(k, a)?)),
            _ => Err(format!(
                "`{s}` names no dropped-name list; the providers are {}",
                DROPPED_KINDS.join(", ")
            )),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ProjectRef {
    pub dsw: Option<String>,
    pub dsp: Option<String>,
    pub config: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TuDecl {
    pub source: String,
    #[serde(default)]
    pub cflags: Vec<String>,
    pub project: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VendoredDecl {
    pub name: String,
    pub dir: String,
    #[serde(default)]
    pub cflags: Vec<String>,
    #[serde(default)]
    pub sources: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Reccmp {
    pub project: String,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ImageDecl {
    pub id: String,
    pub reference: String,
    pub sha256: Option<Sha256>,
    pub compiler: Option<String>,
    pub linker: Option<String>,
    pub project: Option<ProjectRef>,
    pub objdir: Option<String>,
    pub map: Option<String>,
    #[serde(default)]
    pub functions: Vec<FunctionSource>,
    #[serde(default)]
    pub names: Vec<NameSource>,
    #[serde(default)]
    pub dropped: Vec<DroppedSource>,
    #[serde(default)]
    pub libraries: Vec<String>,
    #[serde(default)]
    pub clock: Vec<Field>,
    pub build: Option<BuildSpec>,
    #[serde(default)]
    pub tu: Vec<TuDecl>,
    #[serde(default)]
    pub vendored: Vec<VendoredDecl>,
    #[serde(default)]
    pub residue: Vec<Row>,
    pub codeview: Option<CodeviewSpec>,
    pub pdb: Option<msvc6_pdb::config::PdbSpec>,
    pub classes: Option<crate::classes::ClassesSpec>,
    pub eh: Option<crate::classes::EhSpec>,
}

#[derive(Debug, Clone)]
pub struct Project {
    pub file: PathBuf,
    pub dir: PathBuf,
    text: String,
    spans: Spans,
    pub schema: u32,
    pub optional: Vec<String>,
    pub toolchains: BTreeMap<String, ToolchainSpec>,
    pub runner: RunnerSpec,
    pub cache: CacheSpec,
    pub images: Vec<ImageDecl>,
    pub reccmp: Option<Reccmp>,
    pub source: Source,
}

pub const MAX_KEY_DEPTH: usize = 32;

fn located(file: &Path, text: &str, spans: &Spans, key: &str, msg: &str) -> String {
    let pos = spans.nearest(key).map_or(0, |(_, p)| p);
    format!(
        "{}:{}: {key}: {msg}",
        file.display(),
        spans.location(text, pos)
    )
}

fn control_in(prefix: &str, v: &toml::Value) -> Option<(String, char)> {
    match v {
        toml::Value::String(s) => s
            .chars()
            .find(|c| c.is_control())
            .map(|c| (prefix.to_string(), c)),
        toml::Value::Array(a) => a
            .iter()
            .enumerate()
            .find_map(|(n, x)| control_in(&index(prefix, n), x)),
        toml::Value::Table(t) => t.iter().find_map(|(k, x)| control_in(&join(prefix, k), x)),
        _ => None,
    }
}

fn unquoted(s: &str) -> String {
    let mut quote: Option<char> = None;
    s.chars()
        .filter(|&c| match quote {
            Some(q) => {
                if c == q {
                    quote = None;
                }
                false
            }
            None if c == '"' || c == '\'' => {
                quote = Some(c);
                false
            }
            None => true,
        })
        .collect()
}

fn key_depth(line: &str) -> usize {
    let u = unquoted(line.trim_start());
    let u = u.split('#').next().unwrap_or_default();
    if let Some(h) = u.strip_prefix('[') {
        let h = h
            .trim_start_matches('[')
            .split(']')
            .next()
            .unwrap_or_default();
        return h.matches('.').count().saturating_add(1);
    }
    let (k, v) = u.split_once('=').unwrap_or((u, ""));
    let (mut depth, mut deepest) = (0usize, 0usize);
    for c in v.chars() {
        match c {
            '{' | '[' => {
                depth = depth.saturating_add(1);
                deepest = deepest.max(depth);
            }
            '}' | ']' => depth = depth.saturating_sub(1),
            _ => {}
        }
    }
    k.matches('.')
        .count()
        .saturating_add(1)
        .saturating_add(deepest)
}

fn too_deep(file: &Path, text: &str) -> Option<String> {
    text.lines().enumerate().find_map(|(n, l)| {
        let d = key_depth(l);
        (d > MAX_KEY_DEPTH).then(|| {
            format!(
                "{}:{}:1: a key or value nested {d} levels deep: a config nests at most {MAX_KEY_DEPTH}",
                file.display(),
                n.saturating_add(1)
            )
        })
    })
}

fn pre_checks(file: &Path, text: &str, spans: &Spans) -> Result<()> {
    let Ok(t) = toml::from_str::<toml::Table>(text) else {
        return Ok(());
    };
    if let Some(v) = t.get("schema")
        && v.as_integer() != Some(i64::from(SCHEMA))
    {
        bail!(
            "{}",
            located(
                file,
                text,
                spans,
                "schema",
                &format!("schema {v} is not one this build reads (it reads {SCHEMA})")
            )
        );
    }
    if let Some((key, c)) = t.iter().find_map(|(k, v)| control_in(&join("", k), v)) {
        bail!(
            "{}",
            located(
                file,
                text,
                spans,
                &key,
                &format!(
                    "a control character (U+{:04X}) in a string: a basic string's escapes (\\t, \\n, \\u0000) \
                     are read as written, so write a Windows path in single quotes or with doubled backslashes",
                    u32::from(c)
                )
            )
        );
    }
    Ok(())
}

fn located_message(file: &Path, text: &str, spans: &Spans, e: &toml::de::Error) -> String {
    let msg = e.message().trim().replace('\n', " ");
    let Some(span) = e.span() else {
        return format!("{}:1:1: (document): {msg}", file.display());
    };
    let loc = spans.location(text, span.start);
    let whole = span.start == 0 && (span.end == 0 || span.end >= text.trim_end().len());
    let at = if whole {
        None
    } else {
        spans.path_at(span.start)
    };
    let missing = msg
        .strip_prefix("missing field `")
        .and_then(|r| r.split('`').next());
    let key = match (missing, at) {
        (Some(name), Some(k)) => join(&k, name),
        (Some(name), None) => name.to_string(),
        (None, Some(k)) => k,
        (None, None) => spans.guess(text, span.start),
    };
    format!("{}:{loc}: {key}: {msg}", file.display())
}

impl Project {
    pub fn parse(text: &str, file: &Path) -> Result<Self> {
        let spans = Spans::of(text);
        pre_checks(file, text, &spans)?;
        let doc: Doc =
            toml::from_str(text).map_err(|e| match (e.span(), too_deep(file, text)) {
                (None, Some(m)) => anyhow!("{m}"),
                _ => anyhow!("{}", located_message(file, text, &spans, &e)),
            })?;
        let dir = file
            .parent()
            .filter(|p| !p.as_os_str().is_empty())
            .map_or_else(|| PathBuf::from("."), Path::to_path_buf);
        let p = Self {
            file: file.to_path_buf(),
            dir,
            text: text.to_string(),
            spans,
            schema: doc.schema,
            optional: doc.optional,
            toolchains: doc.toolchain,
            runner: doc.runner,
            cache: doc.cache,
            images: doc.image,
            reccmp: doc.reccmp,
            source: doc.source.unwrap_or_default(),
        };
        p.validate()?;
        Ok(p)
    }

    pub fn load(file: &Path) -> Result<Self> {
        let text = msvc6_base::srcio::own_read(file)
            .with_context(|| format!("reading {}", file.display()))?;
        let mut p = Self::parse(&text, file)?;
        if let Ok(abs) = std::fs::canonicalize(file)
            && let Some(d) = abs.parent()
        {
            d.clone_into(&mut p.dir);
        }
        Ok(p)
    }

    pub fn locate(given: Option<&Path>) -> Result<Option<PathBuf>> {
        if let Some(p) = given {
            if !p.is_file() {
                anyhow::bail!("--config {}: no such file", p.display());
            }
            return Ok(Some(p.to_path_buf()));
        }
        let here = PathBuf::from(FILE);
        if here.is_file() {
            return Ok(Some(here));
        }
        match std::env::var_os(ENV) {
            Some(p) if !p.is_empty() => {
                let p = PathBuf::from(p);
                if !p.is_file() {
                    anyhow::bail!("${ENV} names {}, which is no file", p.display());
                }
                Ok(Some(p))
            }
            _ => Ok(None),
        }
    }

    pub fn find(given: Option<&Path>) -> Result<Option<Self>> {
        Self::locate(given)?.map(|f| Self::load(&f)).transpose()
    }

    pub fn require(given: Option<&Path>) -> Result<Self> {
        let file = Self::locate(given)?.with_context(|| {
            format!("no {FILE} here and ${ENV} is not set; pass --config or run `msvc6t init`")
        })?;
        Self::load(&file)
    }

    #[must_use]
    pub fn text(&self) -> &str {
        &self.text
    }

    #[must_use]
    pub fn canonical_file(&self) -> PathBuf {
        std::fs::canonicalize(&self.file).unwrap_or_else(|_| self.file.clone())
    }

    #[must_use]
    pub fn at(&self, key: &str) -> String {
        match self.spans.nearest(key) {
            Some((_, pos)) => format!(
                "{}:{}: {key}",
                self.file.display(),
                self.spans.location(&self.text, pos)
            ),
            None => format!("{}:1:1: {key}", self.file.display()),
        }
    }

    #[must_use]
    pub fn error(&self, key: &str, msg: impl std::fmt::Display) -> anyhow::Error {
        anyhow!("{}: {msg}", self.at(key))
    }

    #[must_use]
    pub fn image_key(&self, n: usize) -> String {
        index("image", n)
    }

    #[must_use]
    pub fn image(&self, id: &str) -> Option<(usize, &ImageDecl)> {
        self.images
            .iter()
            .enumerate()
            .find(|(_, d)| d.id.eq_ignore_ascii_case(id))
    }

    #[must_use]
    pub fn ids(&self) -> Vec<&str> {
        self.images.iter().map(|d| d.id.as_str()).collect()
    }

    #[must_use]
    pub fn absent(&self, raw: &str) -> Option<String> {
        let mut rest = raw;
        while let Some(i) = rest.find('$') {
            let after = rest.get(i + 1..).unwrap_or_default();
            if let Some(r) = after.strip_prefix('$') {
                rest = r;
                continue;
            }
            let Some(body) = after.strip_prefix('{') else {
                rest = after;
                continue;
            };
            let end = body.find('}')?;
            let name = body.get(..end).unwrap_or_default();
            rest = body.get(end + 1..).unwrap_or_default();
            if self.optional.iter().any(|o| o == name)
                && !std::env::var(name).is_ok_and(|v| !v.is_empty())
            {
                return Some(name.to_string());
            }
        }
        None
    }

    pub fn expand(&self, raw: &str) -> Result<String> {
        msvc6_toolchain::config::expand(raw, &|name| {
            self.toolchains.get(name).map(|t| {
                msvc6_toolchain::config::expand(&t.root, &|_| None)
                    .unwrap_or_else(|_| t.root.clone())
            })
        })
    }

    pub fn path(&self, raw: &str) -> Result<PathBuf> {
        let p = PathBuf::from(self.expand(raw)?);
        Ok(if p.is_absolute() { p } else { self.dir.join(p) })
    }

    pub fn path_at(&self, key: &str, raw: &str) -> Result<PathBuf> {
        self.path(raw)
            .map_err(|e| self.error(key, format!("{e:#}")))
    }

    fn known_toolchain(&self, key: &str, id: Option<&String>) -> Result<()> {
        match id {
            Some(t) if !self.toolchains.contains_key(t) => Err(self.error(
                key,
                format!(
                    "`{t}` is no [toolchain.*] of this config; it has {}",
                    if self.toolchains.is_empty() {
                        "none".to_string()
                    } else {
                        self.toolchains
                            .keys()
                            .cloned()
                            .collect::<Vec<_>>()
                            .join(", ")
                    }
                ),
            )),
            _ => Ok(()),
        }
    }

    fn validate(&self) -> Result<()> {
        if self.schema != SCHEMA {
            return Err(self.error(
                "schema",
                format!(
                    "schema {} is not one this build reads (it reads {SCHEMA})",
                    self.schema
                ),
            ));
        }
        for (i, v) in self.optional.iter().enumerate() {
            let ok = v.starts_with(|c: char| c.is_ascii_alphabetic() || c == '_')
                && v.chars().all(|c| c.is_ascii_alphanumeric() || c == '_');
            if !ok || self.optional.iter().take(i).any(|w| w == v) {
                return Err(self.error(
                    &index("optional", i),
                    format!("`{v}` is not a variable name listed once"),
                ));
            }
        }
        for (id, t) in &self.toolchains {
            let key = join("toolchain", id);
            if t.layout != "vs6" {
                return Err(self.error(
                    &join(&key, "layout"),
                    format!(
                        "layout `{}` is unknown (only \"vs6\": VC98/BIN, COMMON/MSDEV98/BIN, VC98/LIB, VC98/INCLUDE)",
                        t.layout
                    ),
                ));
            }
            for role in t.expect.keys() {
                let roles: Vec<&str> = msvc6_toolchain::layout::ROLES.iter().map(|r| r.0).collect();
                if !roles.contains(&role.as_str()) {
                    return Err(self.error(
                        &join(&join(&key, "expect"), role),
                        format!(
                            "`{role}` names no tool role; the roles are {}",
                            roles.join(", ")
                        ),
                    ));
                }
            }
        }
        self.validate_runner()?;
        let mut seen: BTreeMap<String, usize> = BTreeMap::new();
        for (n, d) in self.images.iter().enumerate() {
            let key = self.image_key(n);
            let idk = join(&key, "id");
            if d.id.is_empty()
                || !d
                    .id
                    .chars()
                    .all(|c| c.is_ascii_alphanumeric() || matches!(c, '_' | '-' | '.'))
            {
                return Err(self.error(
                    &idk,
                    format!(
                        "`{}` is not an image id: letters, digits, `_`, `-` and `.` only",
                        d.id
                    ),
                ));
            }
            if let Some(first) = seen.insert(d.id.to_ascii_uppercase(), n) {
                return Err(self.error(
                    &idk,
                    format!(
                        "`{}` is already the id of image[{first}] (ids compare case-blind)",
                        d.id
                    ),
                ));
            }
            self.known_toolchain(&join(&key, "compiler"), d.compiler.as_ref())?;
            self.known_toolchain(&join(&key, "linker"), d.linker.as_ref())?;
            msvc6_image::clock::check_request(&d.clock)
                .map_err(|e| self.error(&join(&key, "clock"), format!("{e:#}")))?;
            if let Some(p) = &d.project {
                let pk = join(&key, "project");
                match (&p.dsw, &p.dsp) {
                    (Some(_), Some(_)) => {
                        return Err(self.error(
                            &pk,
                            "names both `dsw` and `dsp`; a project is one or the other",
                        ));
                    }
                    (None, None) => {
                        return Err(self.error(&pk, "names neither `dsw` nor `dsp`"));
                    }
                    _ => {}
                }
                if p.config.trim().is_empty() {
                    return Err(self.error(&join(&pk, "config"), "an empty configuration name"));
                }
            }
            if let Some(b) = &d.build {
                let bk = join(&key, "build");
                self.known_toolchain(&join(&bk, "toolchain"), b.toolchain.as_ref())?;
                for (i, s) in b.msdev.iter().enumerate() {
                    self.known_toolchain(
                        &join(&index(&join(&bk, "msdev"), i), "toolchain"),
                        s.toolchain.as_ref(),
                    )?;
                }
                for (i, s) in b.implib.iter().enumerate() {
                    self.known_toolchain(
                        &join(&index(&join(&bk, "implib"), i), "toolchain"),
                        s.toolchain.as_ref(),
                    )?;
                }
                if b.msdev.is_empty() {
                    return Err(
                        self.error(&join(&bk, "msdev"), "an [image.build] with no msdev step")
                    );
                }
            }
            if let Some(cv) = &d.codeview {
                cv.check()
                    .map_err(|e| self.error(&join(&key, "codeview"), format!("{e:#}")))?;
            }
            if let Some(pdb) = &d.pdb {
                pdb.check()
                    .map_err(|e| self.error(&join(&key, "pdb"), format!("{e:#}")))?;
            }
            if let Some(c) = &d.classes {
                c.check()
                    .map_err(|e| self.error(&join(&key, "classes"), format!("{e:#}")))?;
            }
            if let Some(eh) = &d.eh {
                eh.check()
                    .map_err(|e| self.error(&join(&key, "eh"), format!("{e:#}")))?;
            }
            for (i, t) in d.tu.iter().enumerate() {
                if t.source.trim().is_empty() {
                    return Err(self.error(
                        &join(&index(&join(&key, "tu"), i), "source"),
                        "an empty source",
                    ));
                }
            }
        }
        if let Some(r) = &self.reccmp
            && r.project.trim().is_empty()
        {
            return Err(self.error("reccmp.project", "an empty path"));
        }
        Ok(())
    }

    fn validate_runner(&self) -> Result<()> {
        let r = &self.runner;
        if r.kind == RunnerKind::Container && r.container.is_none() {
            return Err(self.error(
                "runner.kind",
                "kind = \"container\" needs `container`, the container's name",
            ));
        }
        if r.jobs == Some(0) {
            return Err(self.error("runner.jobs", "jobs = 0 runs nothing; at least 1"));
        }
        if let Some(m) = &r.job_memory {
            msvc6_toolchain::config::parse_size(m)
                .map_err(|e| self.error("runner.job_memory", format!("{e:#}")))?;
        }
        if let Some(t) = &r.timeout {
            msvc6_toolchain::config::parse_duration(t)
                .map_err(|e| self.error("runner.timeout", format!("{e:#}")))?;
        }
        if let Some(m) = &self.cache.max {
            msvc6_toolchain::config::parse_size(m)
                .map_err(|e| self.error("cache.max", format!("{e:#}")))?;
        }
        Ok(())
    }

    #[must_use]
    pub fn toolchain_config(&self) -> msvc6_toolchain::config::Config {
        msvc6_toolchain::config::Config {
            file: Some(self.canonical_file()),
            dir: self.dir.clone(),
            toolchains: self.toolchains.clone(),
            runner: self.runner.clone(),
            cache: self.cache.clone(),
            images: self
                .images
                .iter()
                .map(|d| msvc6_toolchain::config::ImageSpec {
                    id: d.id.clone(),
                    reference: Some(d.reference.clone()),
                    sha256: d.sha256.as_ref().map(|s| s.as_str().to_string()),
                    compiler: d.compiler.clone(),
                    linker: d.linker.clone(),
                    clock: d.clock.clone(),
                    build: d.build.clone(),
                })
                .collect(),
        }
    }

    #[must_use]
    pub fn source_config(&self) -> Loaded {
        Loaded {
            source: self.source.clone(),
            dir: self.dir.clone(),
            path: Some(self.canonical_file()),
            roots: self
                .toolchains
                .iter()
                .map(|(id, t)| {
                    let root = msvc6_toolchain::config::expand(&t.root, &|_| None)
                        .unwrap_or_else(|_| t.root.clone());
                    (id.clone(), root)
                })
                .collect(),
        }
    }
}

pub fn source_config(given: Option<&Path>) -> Result<Loaded> {
    match Project::find(given)? {
        Some(p) => Ok(p.source_config()),
        None => Ok(Loaded {
            dir: std::env::current_dir()?,
            ..Loaded::default()
        }),
    }
}

pub fn toolchain_config(given: Option<&Path>) -> Result<msvc6_toolchain::config::Config> {
    Ok(Project::require(given)?.toolchain_config())
}
