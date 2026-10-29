use crate::config::{AttestedDir, AttestedFiles, PathsSpec, Record};
use crate::dbi::{Dbi, ModRec, NO_STREAM, read_files};
use crate::le::{latin1, u16_at, u32_at, us};
use crate::msf::Msf2;
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::codec::Gen;
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Module {
    pub name: String,
    pub obj: String,
}

impl Module {
    #[must_use]
    pub fn of(m: &ModRec) -> Self {
        Self {
            name: m.name_text(),
            obj: m.obj_text(),
        }
    }
}

#[must_use]
pub fn object_key(module: &str) -> String {
    module
        .rsplit(['\\', '/'])
        .next()
        .unwrap_or(module)
        .to_ascii_lowercase()
}

fn is_absolute(p: &str) -> bool {
    let b = p.as_bytes();
    matches!(b, [d, b':', ..] if d.is_ascii_alphabetic()) || b.first() == Some(&b'\\')
}

fn joined(base: &[String], p: &str) -> Option<Vec<String>> {
    let mut out = base.to_vec();
    for c in p.split(['\\', '/']).filter(|c| !c.is_empty() && *c != ".") {
        if c == ".." {
            if out.len() <= 1 {
                return None;
            }
            out.pop();
        } else {
            out.push(c.to_string());
        }
    }
    Some(out)
}

fn starts_ci(long: &[String], prefix: &[String]) -> bool {
    long.len() >= prefix.len()
        && long
            .iter()
            .zip(prefix)
            .all(|(a, b)| a.eq_ignore_ascii_case(b))
}

fn entries(dir: &Path) -> Vec<String> {
    std::fs::read_dir(dir).map_or_else(
        |_| Vec::new(),
        |rd| {
            rd.filter_map(|e| e.ok().map(|e| e.file_name().to_string_lossy().into_owned()))
                .collect()
        },
    )
}

fn comps(p: &str) -> Vec<String> {
    joined(&[], p).unwrap_or_default()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Published {
    Stays(&'static str),
    Renamed { product: String, name: String },
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Resolved {
    File(Vec<String>),
    Dir(Vec<String>),
    Output {
        dir: Vec<String>,
        config: String,
        file: Option<String>,
    },
}

impl Resolved {
    #[must_use]
    pub fn product_path(&self) -> String {
        match self {
            Self::File(p) | Self::Dir(p) => p.join("/"),
            Self::Output { dir, config, file } => {
                let mut s = format!("{}/{config}", dir.join("/"));
                if let Some(f) = file {
                    s.push('/');
                    s.push_str(f);
                }
                s
            }
        }
    }
}

#[derive(Debug, Clone)]
pub struct Policy {
    product: PathBuf,
    lanes: Vec<(Vec<String>, Vec<String>)>,
    own: usize,
    attested: Vec<AttestedDir>,
    files: Vec<AttestedFiles>,
    configs: Vec<String>,
    line_names: BTreeMap<String, Vec<String>>,
}

impl Policy {
    pub fn new(
        spec: &PathsSpec,
        product: &Path,
        line_names: BTreeMap<String, Vec<String>>,
    ) -> Result<Self> {
        spec.check()?;
        let mut lanes: Vec<(Vec<String>, Vec<String>)> = spec
            .lanes
            .iter()
            .map(|l| (comps(&l.at), comps(&l.product)))
            .collect();
        let own = lanes.len();
        if let Some(w) = &spec.product_win {
            lanes.push((comps(w), Vec::new()));
        }
        let must = spec
            .lanes
            .iter()
            .map(|l| l.product.as_str())
            .chain(spec.attested.iter().map(|a| {
                if a.output {
                    a.product.rsplit_once('/').map_or("", |(d, _)| d)
                } else {
                    a.product.as_str()
                }
            }))
            .chain(spec.files.iter().map(|f| f.product.as_str()));
        for dir in must {
            if !product.join(dir).is_dir() {
                bail!(
                    "the product {} has no {dir}: the PDB's source names cannot be resolved against it",
                    product.display()
                );
            }
        }
        Ok(Self {
            product: product.to_path_buf(),
            lanes,
            own,
            attested: spec.attested.clone(),
            files: spec.files.clone(),
            configs: spec.configs.clone(),
            line_names,
        })
    }

    #[must_use]
    pub fn attested(&self) -> &[AttestedDir] {
        &self.attested
    }

    #[must_use]
    pub fn attested_files(&self) -> &[AttestedFiles] {
        &self.files
    }

    #[must_use]
    pub fn product(&self) -> &Path {
        &self.product
    }

    fn line_named(&self, name: &str, m: &Module) -> bool {
        self.line_names
            .get(&object_key(&m.name))
            .is_some_and(|ns| ns.iter().any(|n| n.eq_ignore_ascii_case(name)))
    }

    #[must_use]
    pub fn lanes_for_scan(&self) -> Vec<(String, Vec<String>, bool)> {
        self.lanes
            .iter()
            .map(|(l, p)| {
                let lane = l.join("\\");
                let rooted = self
                    .attested
                    .iter()
                    .any(|a| !a.output && a.dir.eq_ignore_ascii_case(&lane));
                let mut at = self.product.clone();
                for c in p {
                    at.push(c);
                }
                let subs = if rooted {
                    entries(&at)
                        .into_iter()
                        .filter(|e| at.join(e).is_dir())
                        .filter(|e| {
                            let d = format!("{lane}\\{e}");
                            !self.attested.iter().any(|a| a.dir.eq_ignore_ascii_case(&d))
                        })
                        .map(|e| e.to_ascii_lowercase())
                        .collect()
                } else {
                    Vec::new()
                };
                (lane, subs, rooted)
            })
            .collect()
    }

    fn module_dir(&self, m: &Module) -> Option<&[String]> {
        let main = &self.lanes.first()?.0;
        let base: &[String] = if is_absolute(&m.obj) { &[] } else { main };
        let mut dir = joined(base, &m.obj)?;
        dir.pop()?;
        if dir
            .last()
            .is_some_and(|c| self.configs.iter().any(|k| c.eq_ignore_ascii_case(k)))
        {
            dir.pop();
        }
        self.lanes
            .iter()
            .take(self.own)
            .find(|(l, _)| l.len() == dir.len() && starts_ci(&dir, l))
            .map(|(l, _)| l.as_slice())
    }

    fn lane_of(&self, abs: &[String]) -> Option<(String, Vec<String>)> {
        self.lanes
            .iter()
            .filter(|(l, _)| starts_ci(abs, l))
            .max_by_key(|(l, _)| l.len())
            .map(|(l, p)| {
                let mut rel = p.clone();
                rel.extend(abs.iter().skip(l.len()).cloned());
                (l.join("\\"), rel)
            })
    }

    fn entry(at: &Path, c: &str) -> Result<Option<String>> {
        let hits: Vec<String> = entries(at)
            .into_iter()
            .filter(|e| e.eq_ignore_ascii_case(c))
            .collect();
        match hits.as_slice() {
            [] => Ok(None),
            [one] => Ok(Some(one.clone())),
            _ => bail!(
                "{} holds {} entries spelled `{c}` in some case",
                at.display(),
                hits.len()
            ),
        }
    }

    fn product_file(&self, rel: &[String]) -> Result<Vec<String>> {
        let mut at = self.product.clone();
        let mut out = Vec::with_capacity(rel.len());
        for c in rel {
            let Some(one) = Self::entry(&at, c)? else {
                bail!("{} holds no `{c}`", at.display());
            };
            at.push(&one);
            out.push(one);
        }
        if !at.is_file() {
            bail!("{} is not a file", at.display());
        }
        Ok(out)
    }

    pub fn resolve(&self, rel: &[String]) -> Result<Option<Resolved>> {
        let mut at = self.product.clone();
        let mut exact: Vec<String> = Vec::with_capacity(rel.len());
        for c in rel {
            match Self::entry(&at, c)? {
                Some(one) if at.is_dir() => {
                    at.push(&one);
                    exact.push(one);
                }
                _ => break,
            }
        }
        if exact.len() == rel.len() {
            return Ok(if at.is_file() {
                Some(Resolved::File(exact))
            } else if at.is_dir() && !exact.is_empty() {
                Some(Resolved::Dir(exact))
            } else {
                None
            });
        }
        let rest = rel.get(exact.len()..).unwrap_or_default();
        if exact.is_empty() || !at.is_dir() {
            return Ok(None);
        }
        let is_config = |c: &str| self.configs.iter().any(|k| k.eq_ignore_ascii_case(c));
        Ok(match rest {
            [config] | [config, _] if is_config(config) => Some(Resolved::Output {
                dir: exact,
                config: config.clone(),
                file: rest.get(1).cloned(),
            }),
            [file] if exact.len() > 1 && exact.last().is_some_and(|c| is_config(c)) => {
                let config = exact.pop().unwrap_or_default();
                Some(Resolved::Output {
                    dir: exact,
                    config,
                    file: Some(file.clone()),
                })
            }
            _ => None,
        })
    }

    #[must_use]
    pub fn spell_file(&self, exact: &[String], record: Record) -> String {
        if let Some((base, dir)) = exact.split_last() {
            let d = dir.join("/");
            if let Some((f, n)) = self.files.iter().find_map(|f| {
                (f.product == d && f.record.is_none_or(|r| r == record))
                    .then(|| f.names.iter().find(|n| n.eq_ignore_ascii_case(base)))
                    .flatten()
                    .map(|n| (f, n))
            }) {
                return format!("{}\\{n}", f.dir);
            }
            if let Some(a) = self.row(&d) {
                return format!("{}\\{base}", a.dir);
            }
        }
        exact.join("\\")
    }

    fn row(&self, d: &str) -> Option<&AttestedDir> {
        self.attested.iter().find(|a| {
            if a.output {
                a.product.eq_ignore_ascii_case(d)
            } else {
                a.product == d
            }
        })
    }

    fn spell_dir(&self, exact: &[String]) -> String {
        self.row(&exact.join("/"))
            .map_or_else(|| exact.join("\\"), |a| a.dir.clone())
    }

    #[must_use]
    pub fn spell(&self, r: &Resolved) -> String {
        match r {
            Resolved::File(f) => self.spell_file(f, Record::Build),
            Resolved::Dir(d) => self.spell_dir(d),
            Resolved::Output { dir, config, file } => {
                let d = format!("{}/{}", dir.join("/"), config);
                let mut s = self.row(&d).map_or_else(
                    || format!("{}\\{config}", dir.join("\\")),
                    |a| a.dir.clone(),
                );
                if let Some(f) = file {
                    s.push('\\');
                    s.push_str(f);
                }
                s
            }
        }
    }

    pub fn publish(&self, name: &str, m: &Module) -> Result<Published> {
        if self.line_named(name, m) {
            return Ok(Published::Stays("its TU's own `#line` names it"));
        }
        let abs = if is_absolute(name) {
            joined(&[], name)
        } else {
            match self.module_dir(m) {
                Some(d) => joined(d, name),
                None => return Ok(Published::Stays("a library's own, relative to no lane")),
            }
        };
        let abs =
            abs.ok_or_else(|| anyhow!("`{name}` (module {}) climbs above its drive", m.name))?;
        let Some((lane, rel)) = self.lane_of(&abs) else {
            if !is_absolute(name) {
                bail!(
                    "`{name}` (module {}) is relative to the lane directory its module was compiled in, and \
                     resolves to `{}`, outside every lane",
                    m.name,
                    abs.join("\\")
                );
            }
            return Ok(Published::Stays("in no lane"));
        };
        let exact = self.product_file(&rel).with_context(|| {
            format!(
                "`{name}` (module {}, object {}) is under the lane directory `{lane}`, which stages the \
                 product's `{}`, but `{}` is no file of the product",
                m.name,
                m.obj,
                rel.first().map_or("", String::as_str),
                rel.join("/")
            )
        })?;
        Ok(Published::Renamed {
            product: exact.join("/"),
            name: self.spell_file(&exact, Record::Line),
        })
    }

    pub fn publish_path(&self, path: &str) -> Result<Option<(String, Option<String>)>> {
        if !is_absolute(path) {
            return Ok(None);
        }
        let abs = joined(&[], path).ok_or_else(|| anyhow!("`{path}` climbs above its drive"))?;
        let Some((lane, rel)) = self.lane_of(&abs) else {
            return Ok(None);
        };
        let r = self.resolve(&rel)?.ok_or_else(|| {
            anyhow!(
                "`{path}` is under the lane directory `{lane}`, but `{}` is no file or directory of the \
                 product, nor a configuration's output ({}) under one",
                rel.join("/"),
                self.configs.join(" / ")
            )
        })?;
        let product = match &r {
            Resolved::File(f) => Some(f.join("/")),
            _ => None,
        };
        Ok(Some((self.spell(&r), product)))
    }

    fn under_attested(&self, a: &AttestedDir, abs: &[String]) -> Result<Option<Resolved>> {
        let dir = comps(&a.dir);
        if !starts_ci(abs, &dir) {
            return Ok(None);
        }
        let mut rel = comps(&a.product);
        rel.extend(abs.iter().skip(dir.len()).cloned());
        self.resolve(&rel)
    }

    pub fn judge(&self, name: &str, m: &Module) -> std::result::Result<&'static str, String> {
        if self.line_named(name, m) {
            return Ok("named by its TU's own `#line`");
        }
        if !is_absolute(name) {
            if self.module_dir(m).is_none() {
                return Ok("a library's own, relative to no lane");
            }
            return match self.product_file(&comps(name)) {
                Ok(exact) => {
                    let want = self.spell_file(&exact, Record::Line);
                    if want == name {
                        Ok("product-relative")
                    } else {
                        Err(format!(
                            "`{name}` (module {}) is the product's `{}`, which the rule publishes as `{want}`",
                            m.name,
                            exact.join("/")
                        ))
                    }
                }
                Err(_) => Err(format!(
                    "`{name}` (module {}) is relative, and no file of the product {} is spelled so",
                    m.name,
                    self.product.display()
                )),
            };
        }
        let abs = comps(name);
        let mut wants: Vec<(String, &AttestedDir)> = Vec::new();
        let mut empty: Vec<&AttestedDir> = Vec::new();
        for a in self.attested.iter().filter(|a| !a.output) {
            if !starts_ci(&abs, &comps(&a.dir)) || abs.len() != comps(&a.dir).len() + 1 {
                continue;
            }
            let mut rel = comps(&a.product);
            rel.extend(abs.last().cloned());
            match self.product_file(&rel) {
                Ok(exact) => {
                    let want = self.spell_file(&exact, Record::Line);
                    if want == name {
                        return Ok("an attested directory");
                    }
                    wants.push((want, a));
                }
                Err(_) => empty.push(a),
            }
        }
        self.verdict(name, &abs, &format!(" (module {})", m.name), &wants, &empty)
    }

    pub fn judge_path(&self, path: &str, line: bool) -> std::result::Result<&'static str, String> {
        let p = path.trim_end_matches(['\\', '/']);
        let fits = |r: &Resolved| {
            self.spell(r) == p
                || (line && matches!(r, Resolved::File(f) if self.spell_file(f, Record::Line) == p))
        };
        if !is_absolute(p) {
            let Some(c) = joined(&[], p) else {
                return Ok("relative to its record's own directory");
            };
            return match self.resolve(&c) {
                Ok(Some(r)) if fits(&r) => Ok("product-relative"),
                Ok(Some(r)) => Err(format!(
                    "`{path}` is the product's `{}`, which the rule publishes as `{}`",
                    r.product_path(),
                    self.spell(&r)
                )),
                _ => Ok("relative to its record's own directory"),
            };
        }
        let abs = comps(p);
        let mut wants: Vec<(String, &AttestedDir)> = Vec::new();
        let mut empty: Vec<&AttestedDir> = Vec::new();
        for a in &self.attested {
            match self.under_attested(a, &abs) {
                Ok(Some(r)) if fits(&r) => return Ok("an attested directory"),
                Ok(Some(r)) => wants.push((self.spell(&r), a)),
                Ok(None) if starts_ci(&abs, &comps(&a.dir)) => empty.push(a),
                Ok(None) => {}
                Err(e) => return Err(format!("`{path}`: {e:#}")),
            }
        }
        self.verdict(p, &abs, "", &wants, &empty)
    }

    fn verdict(
        &self,
        name: &str,
        abs: &[String],
        who: &str,
        wants: &[(String, &AttestedDir)],
        empty: &[&AttestedDir],
    ) -> std::result::Result<&'static str, String> {
        if let Some((w, a)) = wants.iter().find(|(w, _)| w.eq_ignore_ascii_case(name)) {
            return Err(format!(
                "`{name}`{who} is in the attested `{}`, which the rule spells `{w}`: an attested spelling is held \
                 in its own case",
                a.dir
            ));
        }
        let rule = wants.first().map_or_else(String::new, |(w, _)| {
            format!(" (the rule publishes what it names as `{w}`)")
        });
        if let Some((lane, _)) = self.lane_of(abs) {
            return Err(format!(
                "`{name}`{who} names the build lane's directory `{lane}`, a path no original build and no \
                 reader's checkout has{rule}"
            ));
        }
        if let Some((w, a)) = wants.first() {
            return Err(format!(
                "`{name}`{who} is in the attested `{}`, and the rule publishes what it names as `{w}`",
                a.dir
            ));
        }
        if let Some(a) = empty.first() {
            return Err(format!(
                "`{name}`{who} is in the attested `{}`, but names no file of the product's `{}`",
                a.dir, a.product
            ));
        }
        Ok("in no lane")
    }
}

pub fn c11_names(b: &[u8]) -> Result<Vec<String>> {
    if b.len() < 4 {
        return Ok(Vec::new());
    }
    let c_file = usize::from(u16_at(b, 0).context("C11 header truncated")?);
    if c_file.saturating_mul(4).saturating_add(4) > b.len() {
        bail!(
            "the C11 region's {c_file} file record(s) do not fit its {} bytes",
            b.len()
        );
    }
    let mut out = Vec::with_capacity(c_file);
    for i in 0..c_file {
        let base = us(u32_at(b, 4 + 4 * i).context("C11 file table truncated")?);
        let cc = usize::from(u16_at(b, base).context("C11 file record truncated")?);
        let at = base.saturating_add(4).saturating_add(12 * cc);
        let n = usize::from(*b.get(at).context("C11 file name truncated")?);
        out.push(latin1(
            b.get(at + 1..at + 1 + n)
                .context("C11 file name truncated")?,
        ));
    }
    Ok(out)
}

pub fn st_bytes(name: &str) -> Result<Vec<u8>> {
    let b = name
        .chars()
        .map(|c| u8::try_from(u32::from(c)))
        .collect::<std::result::Result<Vec<u8>, _>>()
        .map_err(|_| anyhow!("`{name}` holds a char no PDB 2.0 name byte encodes"))?;
    if b.len() > 255 {
        bail!(
            "`{name}` is {} bytes: a PDB 2.0 name holds at most 255",
            b.len()
        );
    }
    Ok(b)
}

fn pad4(n: usize) -> usize {
    n.next_multiple_of(4)
}

pub fn relayout(b: &[u8], names: &[Option<Vec<u8>>]) -> Result<Vec<u8>> {
    let w16 = |at: usize| -> Result<usize> {
        Ok(usize::from(u16_at(b, at).with_context(|| {
            format!("C11 region truncated at +{at:#x}")
        })?))
    };
    let w32 = |at: usize| -> Result<usize> {
        Ok(us(u32_at(b, at).with_context(|| {
            format!("C11 region truncated at +{at:#x}")
        })?))
    };
    let (c_file, c_seg) = (w16(0)?, w16(2)?);
    if names.len() != c_file {
        bail!(
            "{} new name(s) for a region of {c_file} file record(s)",
            names.len()
        );
    }
    let head = pad4(4 + 4 * c_file + 10 * c_seg);
    let mut pieces: Vec<(usize, usize, Option<usize>)> = Vec::new();
    let mut budget = b.len() / 4;
    for i in 0..c_file {
        let base = w32(4 + 4 * i)?;
        let cc = w16(base)?;
        budget = budget
            .checked_sub(cc + 1)
            .context("the C11 region's tables name more pieces than its bytes hold")?;
        let name_at = base.saturating_add(4).saturating_add(12 * cc);
        let n = usize::from(*b.get(name_at).context("C11 file name truncated")?);
        pieces.push((base, pad4(4 + 12 * cc + 1 + n), Some(i)));
        for j in 0..cc {
            let at = w32(base + 4 + 4 * j)?;
            pieces.push((at, pad4(4 + 6 * w16(at.saturating_add(2))?), None));
        }
    }
    pieces.sort_by_key(|p| p.0);
    let mut end = head;
    for (at, len, file) in &pieces {
        if *at != end {
            bail!(
                "the C11 region does not tile: {} at +{at:#x}, where the piece before it ends at +{end:#x}",
                file.map_or_else(|| "a block".to_string(), |i| format!("file record {i}"))
            );
        }
        end = at + len;
    }
    if end != b.len() {
        bail!(
            "the C11 region's pieces end at +{end:#x} and the region at +{:#x}",
            b.len()
        );
    }
    let mut out = b.get(..head).context("C11 header truncated")?.to_vec();
    let mut moved: BTreeMap<usize, usize> = BTreeMap::new();
    for (at, len, file) in &pieces {
        moved.insert(*at, out.len());
        let piece = b.get(*at..at + len).context("C11 piece truncated")?;
        match file.and_then(|i| names.get(i).cloned().flatten()) {
            Some(name) => {
                let cc = w16(*at)?;
                let fixed = 4 + 12 * cc;
                out.extend_from_slice(piece.get(..fixed).context("C11 file record truncated")?);
                out.push(u8::try_from(name.len()).context("a C11 name holds at most 255 bytes")?);
                out.extend_from_slice(&name);
                out.resize(pad4(out.len()), 0);
            }
            None => out.extend_from_slice(piece),
        }
    }
    let to = |old: usize| -> Result<u32> {
        Ok(u32::try_from(
            *moved
                .get(&old)
                .ok_or_else(|| anyhow!("no C11 piece at +{old:#x}"))?,
        )?)
    };
    for i in 0..c_file {
        let base = w32(4 + 4 * i)?;
        let nb = us(to(base)?);
        crate::le::set32(&mut out, 4 + 4 * i, u32::try_from(nb)?)?;
        for j in 0..w16(base)? {
            let blk = w32(base + 4 + 4 * j)?;
            crate::le::set32(&mut out, nb + 4 + 4 * j, to(blk)?)?;
        }
    }
    if out.len() < b.len() {
        out.resize(b.len(), 0);
    }
    Ok(out)
}

#[derive(Debug, Default, Clone, Serialize)]
pub struct Score {
    pub records: BTreeMap<(String, String), usize>,
    pub file_info: BTreeMap<(String, String), usize>,
    pub stays: BTreeMap<&'static str, usize>,
    pub grown: Vec<(String, usize, usize)>,
    pub file_info_len: Option<(usize, usize)>,
    pub build: BTreeMap<(&'static str, String, String), usize>,
    pub symbols: Vec<(String, usize, usize)>,
    pub regions: usize,
    pub refs: usize,
    pub dbi_len: Option<(usize, usize)>,
    pub link_info: Option<(usize, usize)>,
    pub pages: (usize, usize),
    pub owners: BTreeMap<String, String>,
}

#[must_use]
pub fn dir_of(name: &str) -> String {
    name.rfind(['\\', '/']).map_or_else(String::new, |i| {
        name.get(..=i).unwrap_or_default().to_string()
    })
}

impl Score {
    pub fn claim(&mut self, name: &str, product: &str) -> Result<()> {
        let was = self
            .owners
            .entry(name.to_ascii_lowercase())
            .or_insert_with(|| product.to_string());
        if was != product {
            bail!("`{name}` would name two product files, {was} and {product}");
        }
        Ok(())
    }

    fn count_record(&mut self, old: &str, new: &str) {
        *self.records.entry((dir_of(old), dir_of(new))).or_insert(0) += 1;
    }

    fn count_file_info(&mut self, old: &str, new: &str) {
        *self
            .file_info
            .entry((dir_of(old), dir_of(new)))
            .or_insert(0) += 1;
    }

    #[must_use]
    pub fn lines(&self, policy: &Policy) -> Vec<String> {
        let n: usize = self.records.values().sum();
        let f: usize = self.file_info.values().sum();
        let b: usize = self.build.values().sum();
        let mut out = vec![format!(
            "source names: {n} C11 file record(s), {f} file-info name(s) and {b} build-record path(s) published, resolved in {}",
            policy.product.display()
        )];
        for ((from, to), k) in &self.records {
            out.push(format!("  {k:5} record(s)  {from}  ->  {to}"));
        }
        for (why, k) in &self.stays {
            out.push(format!("  {k:5} record(s)  stay: {why}"));
        }
        for ((field, from, to), k) in &self.build {
            out.push(format!("  {k:5} {field}  {from}  ->  {to}"));
        }
        for a in policy.attested() {
            out.push(format!(
                "  attested {} = {}: {}",
                a.product, a.dir, a.evidence
            ));
        }
        for a in policy.attested_files() {
            out.push(format!(
                "  attested {}/{{{}}} = {}\\..., in {}: {}",
                a.product,
                a.names.join(","),
                a.dir,
                a.record.map_or("every record", |r| match r {
                    Record::Line => "the line records",
                    Record::Build => "the build records",
                }),
                a.evidence
            ));
        }
        if let Some((was, now)) = self.file_info_len {
            out.push(format!("  file info: {was} -> {now} byte(s)"));
        }
        if let Some((was, now)) = self.dbi_len {
            out.push(format!(
                "  DBI (module list, EC names, type-server map re-emitted): {was} -> {now} byte(s)"
            ));
        }
        if let Some((was, now)) = self.link_info {
            out.push(format!("  /LinkInfo: {was} -> {now} byte(s)"));
        }
        let delta: i64 = self
            .symbols
            .iter()
            .map(|(_, a, b)| i64::try_from(*b).unwrap_or(0) - i64::try_from(*a).unwrap_or(0))
            .sum();
        out.push(format!(
            "  {} module line region(s) grew ({} byte(s)); {} symbol region(s) re-emitted, {} at another length ({delta:+} byte(s)), {} reference(s) re-pointed; the file {} -> {} page(s)",
            self.grown.len(),
            self.grown.iter().map(|(_, a, b)| b.saturating_sub(*a)).sum::<usize>(),
            self.regions,
            self.symbols.len(),
            self.refs,
            self.pages.0,
            self.pages.1
        ));
        out
    }
}

pub fn rename_file_info(
    fi: &[u8],
    mods: &[ModRec],
    policy: &Policy,
    score: &mut Score,
) -> Result<Option<Vec<u8>>> {
    if fi.is_empty() {
        return Ok(None);
    }
    let w16 = |at: usize| -> Result<usize> {
        Ok(usize::from(u16_at(fi, at).context("file info truncated")?))
    };
    let nm = w16(0)?;
    if nm != mods.len() {
        bail!(
            "the file info lists {nm} module(s) and the DBI {}",
            mods.len()
        );
    }
    let counts: Vec<usize> = (0..nm)
        .map(|i| w16(4 + 2 * nm + 2 * i))
        .collect::<Result<_>>()?;
    let total: usize = counts.iter().sum();
    let offs_at = 4 + 4 * nm;
    let names_at = offs_at + 4 * total;
    if names_at > fi.len() {
        bail!(
            "the file info's {total} name offsets run past its {} bytes",
            fi.len()
        );
    }
    let mut names: Vec<Vec<u8>> = Vec::with_capacity(total);
    let mut changed = false;
    let mut k = 0usize;
    for (m, c) in mods.iter().zip(&counts) {
        let module = Module::of(m);
        for _ in 0..*c {
            let o = us(u32_at(fi, offs_at + 4 * k).context("file info truncated")?);
            let at = names_at.saturating_add(o);
            let n = usize::from(*fi.get(at).context("file info name truncated")?);
            let old = fi
                .get(at + 1..at + 1 + n)
                .context("file info name truncated")?;
            let text = latin1(old);
            let new = match policy.publish(&text, &module)? {
                Published::Renamed { name, product } => {
                    score.claim(&name, &product)?;
                    score.count_file_info(&text, &name);
                    st_bytes(&name)?
                }
                Published::Stays(_) => old.to_vec(),
            };
            changed |= new != old;
            names.push(new);
            k += 1;
        }
    }
    if !changed {
        return Ok(None);
    }
    let mut out = fi.get(..offs_at).context("file info truncated")?.to_vec();
    let mut buf: Vec<u8> = Vec::new();
    let mut at: BTreeMap<&[u8], u32> = BTreeMap::new();
    for n in &names {
        let o = match at.get(n.as_slice()) {
            Some(o) => *o,
            None => {
                let o = u32::try_from(buf.len())?;
                buf.push(u8::try_from(n.len())?);
                buf.extend_from_slice(n);
                at.insert(n.as_slice(), o);
                o
            }
        };
        out.extend_from_slice(&o.to_le_bytes());
    }
    out.extend_from_slice(&buf);
    out.resize(pad4(out.len()), 0);
    Ok(Some(out))
}

pub fn rename(db: &mut Msf2, policy: &Policy) -> Result<Score> {
    let mut score = Score {
        pages: (db.pages_used, db.pages_used),
        ..Score::default()
    };
    let mut dbi_bytes = db.stream(3)?;
    let dbi = Dbi::parse(&dbi_bytes)?;
    let mut maps: BTreeMap<usize, crate::buildname::RecordMap> = BTreeMap::new();
    let mut seen = std::collections::BTreeSet::new();
    for (imod, m) in dbi.mods.iter().enumerate() {
        let stream = m.stream()?;
        if stream == NO_STREAM {
            continue;
        }
        if !seen.insert(stream) {
            bail!(
                "module {}'s stream {stream} is another module's too",
                m.name_text()
            );
        }
        let module = Module::of(m);
        let (sym_len, line_len) = (m.sym_len()?, m.c11_len()?);
        let s = db.stream(usize::from(stream))?;
        let (from, to) = (sym_len, sym_len.saturating_add(line_len));
        let region = s
            .get(from..to)
            .ok_or_else(|| anyhow!("module {}'s line region runs past its stream", module.name))?;
        let mut lines: Option<Vec<u8>> = None;
        if line_len > 0 {
            let mut new = Vec::new();
            let mut any = false;
            for name in c11_names(region).with_context(|| format!("module {}", module.name))? {
                match policy.publish(&name, &module)? {
                    Published::Renamed { name: n, product } => {
                        score.claim(&n, &product)?;
                        score.count_record(&name, &n);
                        let b = st_bytes(&n)?;
                        any |= b != name.as_bytes();
                        new.push(Some(b));
                    }
                    Published::Stays(why) => {
                        *score.stays.entry(why).or_insert(0) += 1;
                        new.push(None);
                    }
                }
            }
            if any {
                lines = Some(
                    relayout(region, &new).with_context(|| format!("module {}", module.name))?,
                );
            }
        }
        let syms = if sym_len > 4 {
            crate::buildname::rename_symbols(
                s.get(4..sym_len).ok_or_else(|| {
                    anyhow!("module {}'s symbols run past its stream", module.name)
                })?,
                policy,
                &mut score,
            )
            .with_context(|| format!("module {}'s symbols", module.name))?
        } else {
            None
        };
        if lines.is_none() && syms.is_none() {
            continue;
        }
        let mut ns = s.get(..4.min(sym_len)).unwrap_or_default().to_vec();
        match &syms {
            Some((r, map)) => {
                ns.extend_from_slice(r);
                maps.insert(imod + 1, map.clone());
                score.regions += 1;
            }
            None => ns.extend_from_slice(s.get(4.min(sym_len)..sym_len).unwrap_or_default()),
        }
        let new_sym = ns.len();
        ns.extend_from_slice(lines.as_deref().unwrap_or(region));
        let new_lines = ns.len() - new_sym;
        ns.extend_from_slice(s.get(to..).unwrap_or_default());
        for (len, was, at, what) in [
            (new_sym, sym_len, 36, "symbols"),
            (new_lines, line_len, 40, "lines"),
        ] {
            if len == was {
                continue;
            }
            if what == "lines" {
                score.grown.push((module.name.clone(), was, len));
            } else {
                score.symbols.push((module.name.clone(), was, len));
            }
            crate::le::set32(&mut dbi_bytes, m.at + at, u32::try_from(len)?)?;
        }
        db.set_stream(usize::from(stream), &ns)?;
    }
    if !maps.is_empty() {
        let i = usize::from(dbi.symrecs()?);
        let mut sr = db.stream(i)?;
        score.refs = crate::buildname::repoint_refs(&mut sr, &maps)?;
        db.set_stream(i, &sr)?;
    }
    let (fi_at, fi_len) = (dbi.offsets.files, dbi.files.len());
    if let Some(mut nf) = rename_file_info(&dbi.files, &dbi.mods, policy, &mut score)? {
        let now = nf.len().max(fi_len);
        score.file_info_len = Some((fi_len, now));
        nf.resize(now, 0);
        let mut d = dbi_bytes.get(..fi_at).unwrap_or_default().to_vec();
        d.extend_from_slice(&nf);
        d.extend_from_slice(dbi_bytes.get(fi_at + fi_len..).unwrap_or_default());
        crate::le::set32(&mut d, 36, u32::try_from(now)?)?;
        dbi_bytes = d;
    }
    if let Some(d) = crate::buildname::rename_dbi(&dbi_bytes, policy, &mut score)? {
        score.dbi_len = Some((dbi_bytes.len(), d.len()));
        dbi_bytes = d;
    }
    db.set_stream(3, &dbi_bytes)?;
    if let Some(li) = crate::buildname::link_info_stream(db)? {
        let s = db.stream(li)?;
        if let Some(n) = crate::buildname::rename_link_info(&s, policy, &mut score)? {
            score.link_info = Some((s.len(), n.len()));
            db.set_stream(li, &n)?;
        }
    }
    score.pages.1 = db.pages_used;
    Ok(score)
}

pub const SOURCES: &str = "source names are published ones";
pub const BUILD: &str = "build-record paths are published ones";
pub const LANES: &str = "no stream holds a build lane's directory";

#[derive(Debug, Default, Clone, Serialize)]
pub struct Judged {
    pub rows: BTreeMap<&'static str, (usize, Vec<String>)>,
}

impl Judged {
    fn ok(&mut self, class: &'static str) {
        self.rows.entry(class).or_default().0 += 1;
    }
    fn fail(&mut self, class: &'static str, why: String) {
        self.rows.entry(class).or_default().1.push(why);
    }
    #[must_use]
    pub fn failures(&self) -> usize {
        self.rows.values().map(|(_, f)| f.len()).sum()
    }
}

pub fn check(file: &[u8], policy: &Policy) -> Result<Judged> {
    let db = Msf2::parse(file.to_vec())?;
    let dbi_bytes = db.stream(3)?;
    let dbi = Dbi::parse(&dbi_bytes)?;
    let mut j = Judged::default();
    for m in &dbi.mods {
        let (stream, sym, lines) = (m.stream()?, m.sym_len()?, m.c11_len()?);
        if stream == NO_STREAM || lines == 0 {
            continue;
        }
        let module = Module::of(m);
        let s = db.stream(usize::from(stream))?;
        let Some(region) = s.get(sym..sym.saturating_add(lines)) else {
            j.fail(
                SOURCES,
                format!(
                    "module {}: its line region runs past its stream",
                    module.name
                ),
            );
            continue;
        };
        match c11_names(region) {
            Ok(names) => {
                for (i, n) in names.iter().enumerate() {
                    match policy.judge(n, &module) {
                        Ok(_) => j.ok(SOURCES),
                        Err(e) => j.fail(
                            SOURCES,
                            format!("module {} C11 file record {i}: {e}", module.name),
                        ),
                    }
                }
            }
            Err(e) => j.fail(SOURCES, format!("module {}: {e:#}", module.name)),
        }
    }
    match read_files(&dbi.files, Gen::St) {
        Ok(files) => {
            for (m, names) in dbi.mods.iter().zip(&files) {
                let module = Module::of(m);
                for n in names {
                    match policy.judge(&latin1(n), &module) {
                        Ok(_) => j.ok(SOURCES),
                        Err(e) => j.fail(SOURCES, format!("module {} file info: {e}", module.name)),
                    }
                }
            }
        }
        Err(e) => j.fail(SOURCES, format!("the DBI file info does not parse: {e:#}")),
    }
    let judge_path = |j: &mut Judged, path: &str, at: &str| match policy.judge_path(path, false) {
        Ok(_) => j.ok(BUILD),
        Err(e) => j.fail(BUILD, format!("{at}: {e}")),
    };
    for m in &dbi.mods {
        let (stream, sym) = (m.stream()?, m.sym_len()?);
        if stream == NO_STREAM || sym <= 4 {
            continue;
        }
        let s = db.stream(usize::from(stream))?;
        match s.get(4..sym).map(crate::buildname::symbol_paths) {
            Some(Ok(paths)) => {
                for (field, p) in paths {
                    judge_path(&mut j, &p, &format!("module {} {field}", m.name_text()));
                }
            }
            Some(Err(e)) => j.fail(BUILD, format!("module {}: {e:#}", m.name_text())),
            None => j.fail(
                BUILD,
                format!("module {}: its symbols run past its stream", m.name_text()),
            ),
        }
    }
    match crate::buildname::dbi_paths(&dbi_bytes) {
        Ok(paths) => {
            for (at, p) in paths {
                judge_path(&mut j, &p, &at);
            }
        }
        Err(e) => j.fail(BUILD, format!("the DBI: {e:#}")),
    }
    match crate::buildname::link_info_stream(&db)
        .and_then(|i| i.map(|i| db.stream(i)).transpose())
        .and_then(|s| s.map_or_else(|| Ok(Vec::new()), |s| crate::buildname::link_info_paths(&s)))
    {
        Ok(paths) => {
            for (at, p) in paths {
                judge_path(&mut j, &p, at);
            }
        }
        Err(e) => j.fail(BUILD, format!("/LinkInfo: {e:#}")),
    }
    match crate::buildname::lane_text(&db, policy) {
        Ok(hits) if hits.is_empty() => j.ok(LANES),
        Ok(hits) => {
            for (i, at, text) in hits {
                j.fail(LANES, format!("stream {i} +{at:#x}: `{text}`"));
            }
        }
        Err(e) => j.fail(LANES, format!("{e:#}")),
    }
    Ok(j)
}

pub fn line_directive(l: &str) -> Option<String> {
    let rest = l
        .trim_start()
        .strip_prefix('#')?
        .trim_start()
        .strip_prefix("line")?;
    if !rest.starts_with(|c: char| c.is_ascii_whitespace()) {
        return None;
    }
    let rest = rest.trim_start();
    let digits: String = rest.chars().take_while(char::is_ascii_digit).collect();
    digits.parse::<u32>().ok()?;
    let tail = rest.get(digits.len()..)?.trim();
    let mut out = String::new();
    let mut it = tail.strip_prefix('"')?.chars();
    while let Some(c) = it.next() {
        match c {
            '"' => return Some(out),
            '\\' => out.push(it.next()?),
            c => out.push(c),
        }
    }
    None
}
