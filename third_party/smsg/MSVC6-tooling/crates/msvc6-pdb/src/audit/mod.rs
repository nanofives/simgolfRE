mod file;
#[cfg(test)]
mod tests;

pub use file::{host_names, host_paths, path_needles, path_roots};

use crate::dbi::{DBG_FPO, SC_SIZE};
use crate::linedir::ByObject;
use crate::msf::Msf2;
use crate::paths::Policy;
use crate::reloc::{
    DbiView, FPO_SIZE, MapRow, SEGMAP_SIZE, image_span, record_end, records, shapes_agree, u16_at,
    u32_at,
};
use crate::report::Report;
use anyhow::{Context as _, Result, bail};
use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use std::collections::{BTreeMap, BTreeSet};
use std::io::Write;

pub const EXAMPLES: usize = 6;
pub const RESULT: &str = "RESULT";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Role {
    Public,
    Data,
    Proc,
    Thunk,
    Child,
    Other,
}

#[derive(Debug, Clone, Copy)]
struct Field {
    role: Role,
    at: usize,
    len: Option<(usize, usize)>,
    name: Option<usize>,
    scope: bool,
}

fn classify(kind: u16) -> Option<Option<Field>> {
    let f = |role, at, len, name, scope| {
        Some(Some(Field {
            role,
            at,
            len,
            name,
            scope,
        }))
    };
    match kind {
        0x1007 | 0x1008 | 0x100e | 0x100f => f(Role::Data, 4, None, Some(10), false),
        0x1009 => f(Role::Public, 4, None, Some(10), false),
        0x100a | 0x100b => f(Role::Proc, 28, Some((12, 4)), Some(35), true),
        0x0204 | 0x0205 => f(Role::Proc, 24, Some((12, 4)), Some(33), true),
        0x0206 => f(Role::Thunk, 12, Some((18, 2)), Some(21), true),
        0x0207 | 0x0208 => f(Role::Child, 12, Some((8, 4)), Some(18), true),
        0x0209 => f(Role::Child, 0, None, Some(7), false),
        0x0201 | 0x0202 | 0x020d | 0x020e => f(Role::Data, 0, None, Some(8), false),
        0x0203 => f(Role::Public, 0, None, Some(8), false),
        0x0005 | 0x020a | 0x1019 => f(Role::Other, 0, None, None, false),
        0x020b => f(Role::Other, 4, None, None, false),
        0x100c => f(Role::Other, 8, None, None, false),
        0x0001..=0x0004
        | 0x0006..=0x000e
        | 0x0200
        | 0x020c
        | 0x0400..=0x0403
        | 0x1001..=0x1006
        | 0x100d
        | 0x1012..=0x1014
        | 0x1017
        | 0x1018 => Some(None),
        _ => None,
    }
}

fn kind_name(kind: u16) -> &'static str {
    match kind {
        0x1007 => "S_LDATA32",
        0x1008 => "S_GDATA32",
        0x100e => "S_LTHREAD32",
        0x100f => "S_GTHREAD32",
        0x1009 => "S_PUB32",
        0x100a => "S_LPROC32",
        0x100b => "S_GPROC32",
        0x0204 => "S_LPROC32_16t",
        0x0205 => "S_GPROC32_16t",
        0x0206 => "S_THUNK32",
        0x0207 => "S_BLOCK32",
        0x0208 => "S_WITH32",
        0x0209 => "S_LABEL32",
        0x0201 => "S_LDATA32_16t",
        0x0202 => "S_GDATA32_16t",
        0x0203 => "S_PUB32_16t",
        0x020d => "S_LTHREAD32_16t",
        0x020e => "S_GTHREAD32_16t",
        0x0005 => "S_SSEARCH",
        0x020a => "S_CEXMODEL32",
        0x1019 => "S_ANNOTATION",
        0x020b => "S_VFTABLE32_16t",
        0x100c => "S_VFTABLE32",
        _ => "addressed record",
    }
}

fn st_name(b: &[u8], at: usize) -> String {
    let n = usize::from(b.get(at).copied().unwrap_or(0));
    b.get(at + 1..at + 1 + n)
        .map(|s| s.iter().map(|c| char::from(*c)).collect())
        .unwrap_or_default()
}

fn base_lower(path: &str) -> String {
    path.rsplit(['\\', '/'])
        .next()
        .unwrap_or(path)
        .to_ascii_lowercase()
}

#[must_use]
pub fn display_name(sym: &str) -> String {
    let mut head = sym;
    while let Some(at) = head.rfind(['+', '-']) {
        let (rest, off) = head.split_at(at);
        let hex = off
            .get(1..)
            .and_then(|o| o.strip_prefix("0x"))
            .is_some_and(|h| !h.is_empty() && h.chars().all(|c| c.is_ascii_hexdigit()));
        if !hex || rest.is_empty() {
            break;
        }
        head = rest;
    }
    let tail = sym.get(head.len()..).unwrap_or_default();
    format!("{}{tail}", base_name(head))
}

fn base_name(sym: &str) -> String {
    if sym.starts_with('?') {
        if let Ok(s) = msvc_demangler::demangle(sym, msvc_demangler::DemangleFlags::NAME_ONLY) {
            return s;
        }
        if let Some((head, _)) = sym.split_once("@@$4")
            && let Some((name, class)) = head.strip_prefix('?').and_then(|h| h.split_once('@'))
        {
            return format!("{class}::{name}`vtordisp'");
        }
        return sym.to_string();
    }
    let s = sym.strip_prefix('_').unwrap_or(sym);
    match s.rsplit_once('@') {
        Some((n, k)) if !n.is_empty() && k.chars().all(|c| c.is_ascii_digit()) && !k.is_empty() => {
            n.to_string()
        }
        _ => s.to_string(),
    }
}

#[must_use]
pub fn name_key(s: &str) -> String {
    let mut t = s.to_string();
    for w in ["class ", "struct ", "enum ", "union "] {
        t = t.replace(w, "");
    }
    let mut out = String::with_capacity(t.len());
    let mut depth = 0usize;
    for c in t.chars() {
        match c {
            '{' => depth += 1,
            '}' => depth = depth.saturating_sub(1),
            _ if depth > 0 => {}
            ' ' | '_' | '`' | '\'' => {}
            _ => out.push(c),
        }
    }
    out
}

#[must_use]
pub fn keys_of(name: &str) -> Vec<String> {
    let mut k = vec![name_key(name)];
    if name.starts_with('?') {
        let d = display_name(name);
        if d != name {
            k.push(name_key(&d));
        }
        if let Some(v) = vtordisp_target(name) {
            k.push(name_key(&v));
        }
    }
    let extra: Vec<String> = k
        .iter()
        .filter_map(|x| x.strip_suffix("vtordisp").map(str::to_string))
        .collect();
    k.extend(extra);
    k.retain(|x| !x.is_empty());
    k
}

fn vtordisp_target(sym: &str) -> Option<String> {
    let (head, _) = sym.split_once("@@$4")?;
    let rest = head.strip_prefix('?')?;
    let (special, rest) = if let Some(r) = rest.strip_prefix("?_E") {
        (Some("`vector deleting destructor'"), r)
    } else if let Some(r) = rest.strip_prefix("?_G") {
        (Some("`scalar deleting destructor'"), r)
    } else {
        (None, rest)
    };
    let mut parts: Vec<&str> = rest.split('@').filter(|p| !p.is_empty()).collect();
    let name = match special {
        Some(s) => s.to_string(),
        None => {
            if parts.is_empty() {
                return None;
            }
            parts.remove(0).to_string()
        }
    };
    parts.reverse();
    parts.push(&name);
    Some(parts.join("::"))
}

#[derive(Debug, Clone)]
struct ShipSec {
    name: String,
    rva: u32,
    vsize: u32,
    mapped: u32,
    exec: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Source {
    Report,
    Pairs,
    Expect,
}

#[derive(Debug, Clone)]
struct Entry {
    size: u32,
    keys: BTreeSet<String>,
    shown: String,
    obj: Option<String>,
    code: bool,
    our: Option<u32>,
    source: Source,
}

impl Entry {
    fn carries(&self, key: &str) -> bool {
        self.keys.contains(key)
    }
}

#[derive(Debug)]
struct Ours<'a> {
    img: &'a Image,
    by_key: BTreeMap<String, Vec<(u32, String)>>,
}

#[derive(Debug, Clone, Copy)]
pub struct Inputs<'a> {
    pub image: &'a Image,
    pub image_label: &'a str,
    pub report: Option<(&'a Report, &'a str)>,
    pub expect: Option<(&'a str, &'a str)>,
    pub pairs: Option<(&'a str, &'a str)>,
    pub retired: Option<&'a str>,
    pub ours: Option<(&'a Image, &'a [MapRow], &'a str)>,
    pub line_dirs: &'a ByObject,
    pub policy: Option<&'a Policy>,
    pub host: &'a [String],
    pub host_paths: &'a [String],
    pub examples: usize,
}

#[derive(Debug)]
struct Shipped<'a> {
    secs: Vec<ShipSec>,
    oracle: BTreeMap<u32, Vec<Entry>>,
    by_name: BTreeMap<(String, String), u32>,
    iat_names: BTreeMap<u32, String>,
    img: Option<&'a Image>,
    ours: Option<Ours<'a>>,
    may_fold: bool,
    exact: Vec<(u32, u32, u32, String)>,
    longest: u32,
    least: BTreeMap<u32, u32>,
    bounding: BTreeSet<u32>,
    retired: Option<BTreeMap<(u16, String, u32), usize>>,
    line_dirs: ByObject,
}

fn hex32(s: &str) -> Option<u32> {
    let t = s.trim();
    u32::from_str_radix(t.strip_prefix("0x").unwrap_or(t), 16).ok()
}

impl<'a> Shipped<'a> {
    fn sections_only(img: &'a Image) -> Result<Self> {
        let secs = img
            .sections
            .iter()
            .map(|s| ShipSec {
                name: s.name.clone(),
                rva: s.rva.get(),
                vsize: s.virt_size.get(),
                mapped: s.virt_size.get().max(s.raw_size.get()),
                exec: s.flags & IMAGE_SCN_MEM_EXECUTE != 0,
            })
            .collect();
        let iat_names = img
            .imports()?
            .iter()
            .flat_map(|d| d.entries.iter().map(|e| (e.iat_rva.get(), e.name.clone())))
            .collect();
        Ok(Self {
            secs,
            oracle: BTreeMap::new(),
            by_name: BTreeMap::new(),
            iat_names,
            img: Some(img),
            ours: None,
            may_fold: true,
            exact: Vec::new(),
            longest: 0,
            least: BTreeMap::new(),
            bounding: BTreeSet::new(),
            retired: None,
            line_dirs: BTreeMap::new(),
        })
    }

    fn inside_exact(&self, rva: u32) -> Option<Vec<(u32, &str, u32)>> {
        let longest = self.longest;
        let from = self
            .exact
            .partition_point(|e| e.0.saturating_add(longest) <= rva);
        let v: Vec<(u32, &str, u32)> = self
            .exact
            .get(from..)
            .unwrap_or_default()
            .iter()
            .take_while(|e| e.0 < rva)
            .filter(|e| rva - e.0 < e.1)
            .map(|e| (e.0, e.3.as_str(), e.2.saturating_add(rva - e.0)))
            .collect();
        (!v.is_empty()).then_some(v)
    }

    fn moved_into(&self, rva: u32, was: u32) -> Option<String> {
        let v = self.inside_exact(rva)?;
        if v.iter().any(|(_, _, o)| *o == was) {
            return None;
        }
        let (at, name, o) = v.first()?;
        Some(format!(
            "at {rva:#x} lies inside shipped `{name}` ({at:#x}, byte-identical to ours), which is our \
             {o:#x} there -- this record was our {was:#x}: moved into another function's body"
        ))
    }

    fn base(&self) -> u32 {
        self.img.map_or(0, |i| i.opt.image_base.get())
    }

    fn our_rva(&self, seg: u16, off: u32) -> Option<u32> {
        let s = self
            .ours
            .as_ref()?
            .img
            .sections
            .get(usize::from(seg).checked_sub(1)?)?;
        Some(s.rva.get().saturating_add(off))
    }

    fn exec_at(&self, rva: u32) -> bool {
        self.secs
            .iter()
            .any(|s| s.exec && rva >= s.rva && rva < s.rva.saturating_add(s.mapped))
    }

    fn add(&mut self, rva: u32, e: Entry) {
        if Self::bounds(&e) {
            self.bounding.insert(rva);
        }
        self.oracle.entry(rva).or_default().push(e);
    }

    fn index(&mut self) {
        self.longest = self.exact.iter().map(|e| e.1).max().unwrap_or(0);
        self.least = self
            .oracle
            .iter()
            .filter_map(|(at, es)| {
                es.iter()
                    .filter(|x| Self::bounds(x))
                    .map(|x| x.size)
                    .min()
                    .map(|m| (*at, m))
            })
            .collect();
    }

    fn load(i: &Inputs<'a>) -> Result<Self> {
        let mut me = Self::sections_only(i.image)?;
        match (i.report, i.expect) {
            (Some((r, _)), _) => {
                me.load_report(r);
                me.line_dirs = i.line_dirs.clone();
            }
            (None, Some((e, _))) => me.load_expect(e),
            (None, None) => {
                bail!("the audit needs a report (relocated) or an expect table (authored)")
            }
        }
        if let Some((p, _)) = i.pairs {
            me.load_pairs(p);
            let r = i.retired.context(
                "the derived pairings come with their retired list (`<pairs>.retired.tsv`), and none was given",
            )?;
            me.load_retired(r);
        }
        if let Some((img, rows, _)) = i.ours {
            me.load_ours(img, rows);
        }
        me.index();
        if me.oracle.is_empty() {
            bail!("the identity oracle is empty");
        }
        Ok(me)
    }

    fn load_report(&mut self, report: &Report) {
        let base = self.base();
        for r in &report.rows {
            let Some(rva) = r.rva(base) else {
                continue;
            };
            let size = r.size.and_then(|x| u32::try_from(x).ok()).unwrap_or(0);
            if size == 0 {
                continue;
            }
            let name = r.name.clone();
            let symbol = r.symbol.clone().unwrap_or_default();
            let obj = Some(r.obj.to_ascii_lowercase()).filter(|o| !o.is_empty());
            let mut keys: BTreeSet<String> = keys_of(&name).into_iter().collect();
            keys.extend(keys_of(&symbol));
            if let Some(o) = &obj {
                for k in &keys {
                    self.by_name.entry((k.clone(), o.clone())).or_insert(rva);
                }
            }
            let our = r.our_rva(base);
            if let (Some(o), Some(0)) = (our, r.edit) {
                self.exact.push((rva, size, o, name.clone()));
            }
            self.add(
                rva,
                Entry {
                    size,
                    keys,
                    shown: name,
                    obj,
                    code: true,
                    our,
                    source: Source::Report,
                },
            );
        }
        self.exact.sort_unstable_by_key(|e| e.0);
    }

    fn load_retired(&mut self, text: &str) {
        let mut m: BTreeMap<(u16, String, u32), usize> = BTreeMap::new();
        for line in text.lines().filter(|l| !l.starts_with('#')) {
            let t: Vec<&str> = line.split('\t').collect();
            let (Some(kind), Some(name), Some(rva)) = (
                t.first()
                    .and_then(|v| u16::from_str_radix(v.trim_start_matches("0x"), 16).ok()),
                t.get(1),
                t.get(3).and_then(|v| hex32(v)),
            ) else {
                continue;
            };
            *m.entry((kind, (*name).to_string(), rva)).or_default() += 1;
        }
        self.retired = Some(m);
    }

    fn load_pairs(&mut self, text: &str) {
        for line in text.lines() {
            if line.starts_with('#') || line.trim().is_empty() {
                continue;
            }
            let t: Vec<&str> = line.split('\t').collect();
            let (Some(our), Some(refr), Some(len)) = (
                t.first().and_then(|v| hex32(v)),
                t.get(1).and_then(|v| hex32(v)),
                t.get(2).and_then(|v| v.trim().parse::<u32>().ok()),
            ) else {
                continue;
            };
            let how = t.get(4).copied().unwrap_or_default();
            let names = t.get(7).copied().unwrap_or_default();
            let tok: Vec<&str> = names.split(' ').filter(|x| !x.is_empty()).collect();
            let mut keys = BTreeSet::new();
            if tok.len() <= MAX_NAME_TOKENS {
                for a in 0..tok.len() {
                    for b in a + 1..=tok.len() {
                        if let Some(span) = tok.get(a..b) {
                            keys.extend(keys_of(&span.join(" ")));
                        }
                    }
                }
            } else {
                for t in &tok {
                    keys.extend(keys_of(t));
                }
            }
            let code = self.exec_at(refr) && !matches!(how, "data" | "content");
            self.add(
                refr,
                Entry {
                    size: len.max(1),
                    keys,
                    shown: tok
                        .first()
                        .map_or_else(|| format!("({how})"), |n| (*n).to_string()),
                    obj: None,
                    code,
                    our: Some(our),
                    source: Source::Pairs,
                },
            );
        }
    }

    fn load_expect(&mut self, text: &str) {
        let base = self.base();
        let mut rows: Vec<(u32, bool, Vec<String>)> = Vec::new();
        for line in text.lines().skip(1) {
            let mut t = line.split('\t');
            let (Some(va), Some(kind), Some(names)) = (t.next(), t.next(), t.next()) else {
                continue;
            };
            let Some(rva) = hex32(va).and_then(|v| v.checked_sub(base)) else {
                continue;
            };
            let code = kind != "data" && self.exec_at(rva);
            rows.push((rva, code, names.split('|').map(str::to_string).collect()));
        }
        rows.sort_by_key(|r| r.0);
        let starts: Vec<u32> = rows.iter().map(|r| r.0).collect();
        for (rva, code, names) in rows {
            let next = starts.get(starts.partition_point(|s| *s <= rva)).copied();
            let sec_end = self
                .secs
                .iter()
                .find(|s| rva >= s.rva && rva < s.rva.saturating_add(s.mapped))
                .map(|s| s.rva.saturating_add(s.mapped));
            let end = next
                .into_iter()
                .chain(sec_end)
                .min()
                .unwrap_or(rva.saturating_add(1));
            let mut keys = BTreeSet::new();
            for n in &names {
                keys.extend(keys_of(n));
            }
            self.add(
                rva,
                Entry {
                    size: end.saturating_sub(rva).max(1),
                    keys,
                    shown: names.first().cloned().unwrap_or_default(),
                    obj: None,
                    code,
                    our: None,
                    source: Source::Expect,
                },
            );
        }
    }

    fn load_ours(&mut self, img: &'a Image, rows: &[MapRow]) {
        let mut by_key: BTreeMap<String, Vec<(u32, String)>> = BTreeMap::new();
        for r in rows {
            for k in keys_of(&r.name) {
                by_key
                    .entry(k)
                    .or_default()
                    .push((r.rva, r.stem.to_ascii_lowercase()));
            }
        }
        self.ours = Some(Ours { img, by_key });
    }

    fn sec(&self, seg: u16) -> Option<&ShipSec> {
        self.secs.get(usize::from(seg).checked_sub(1)?)
    }

    fn bytes(&self, rva: u32, len: usize) -> Option<&[u8]> {
        self.img?.at_rva(Rva(rva), len)
    }

    fn bounds(e: &Entry) -> bool {
        e.code && e.source != Source::Pairs
    }

    fn code_at(&self, rva: u32) -> Option<(u32, &Entry)> {
        let bounding = self
            .bounding
            .range(..=rva)
            .next_back()
            .and_then(|start| Some((*start, self.oracle.get(start)?)))
            .and_then(|(start, es)| {
                let e = es
                    .iter()
                    .filter(|e| Self::bounds(e))
                    .max_by_key(|e| e.size)?;
                (rva - start < e.size).then_some((start, e))
            });
        bounding.or_else(|| {
            self.oracle
                .range(..=rva)
                .rev()
                .take(64)
                .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
                .filter(|(at, e)| e.code && rva - at < e.size)
                .max_by_key(|(_, e)| e.size)
        })
    }

    fn next_after(&self, start: u32, from: &Entry) -> Option<(u32, &Entry)> {
        self.bounding
            .range(start.saturating_add(1)..)
            .filter_map(|at| Some((*at, self.oracle.get(at)?)))
            .filter_map(|(at, es)| es.iter().find(|e| Self::bounds(e)).map(|e| (at, e)))
            .find(|(at, e)| {
                let piece = e.shown.rsplit_once("+0x").is_some_and(|(head, off)| {
                    u32::from_str_radix(off, 16).ok() == Some(*at - start)
                        && keys_of(head).iter().any(|k| from.carries(k))
                });
                !piece
            })
    }

    fn stays_in(&self, rva: u32, len: u32) -> std::result::Result<(), String> {
        let Some((start, e)) = self.code_at(rva) else {
            return Err(format!("at {rva:#x} lies in no shipped function"));
        };
        if let Some((next, g)) = self.next_after(start, e)
            && rva.saturating_add(len) > next
        {
            return Err(format!(
                "{rva:#x}+{len:#x} runs into shipped `{}` at {next:#x} (after `{}` at {start:#x})",
                g.shown, e.shown
            ));
        }
        Ok(())
    }

    fn proven(&self, e: &Entry, rva: u32, len: u32) -> bool {
        let (Some(o), Some(our)) = (&self.ours, e.our) else {
            return false;
        };
        let len = if len > 0 {
            len
        } else {
            self.least.get(&rva).copied().unwrap_or(e.size).min(e.size)
        };
        let n = usize::try_from(len).unwrap_or(0);
        let (Some(a), Some(b)) = (o.img.at_rva(Rva(our), n), self.bytes(rva, n)) else {
            return false;
        };
        let Some(img) = self.img else {
            return false;
        };
        shapes_agree(a, b, &image_span(o.img), &image_span(img))
    }

    fn import_stub(&self, rva: u32, len: u32) -> bool {
        let base = self.base();
        let slot = match self.bytes(rva, 6) {
            Some([0xff, 0x25, a, b, c, d]) => u32::from_le_bytes([*a, *b, *c, *d]),
            _ => return false,
        };
        len == 6
            && slot
                .checked_sub(base)
                .is_some_and(|s| self.iat_names.contains_key(&s))
    }

    fn stub_imports(&self, rva: u32, key: &str) -> bool {
        let base = self.base();
        let slot = match self.bytes(rva, 6) {
            Some([0xff, 0x25, a, b, c, d]) => u32::from_le_bytes([*a, *b, *c, *d]),
            _ => return false,
        };
        slot.checked_sub(base)
            .and_then(|s| self.iat_names.get(&s))
            .is_some_and(|n| keys_of(n).iter().any(|k| k == key))
    }
}

pub const MAX_NAME_TOKENS: usize = 16;

#[derive(Debug)]
enum Identity {
    Named,
    Folded(String),
    Linked(&'static str, String),
    Fail(String),
}

fn weak_default(key: &str) -> Option<String> {
    key.strip_prefix("??E").map(|rest| format!("??G{rest}"))
}

impl Shipped<'_> {
    fn identify(&self, rva: u32, key: &str, obj: Option<&str>, code: bool, len: u32) -> Identity {
        let here = self.oracle.get(&rva).map(Vec::as_slice).unwrap_or_default();
        if key.is_empty() {
            return if here
                .iter()
                .any(|e| e.keys.is_empty() && e.source == Source::Pairs)
            {
                Identity::Named
            } else {
                Identity::Fail(format!(
                    "an unnamed record at {rva:#x}, where the shipped side proves no unnamed datum"
                ))
            };
        }
        if let Some(e) = here.iter().find(|e| e.carries(key)) {
            if e.source == Source::Pairs
                && e.code
                && self.ours.is_some()
                && !self.proven(e, rva, len)
            {
                return Identity::Fail(format!(
                    "at {rva:#x}: the pairing that names it here does not hold byte for byte"
                ));
            }
            if let (Some(fo), Some(m)) = (&e.obj, obj)
                && fo != m
                && let Some(own) = self.by_name.get(&(key.to_string(), m.to_string()))
                && *own != rva
            {
                return Identity::Fail(format!(
                    "of {m} lands on {rva:#x}, shipped `{}` of {fo}, while its own is at \
                     {own:#x} -- WRONG TU",
                    e.shown
                ));
            }
            return Identity::Named;
        }
        if let Some(o) = &self.ours {
            let containing = self
                .oracle
                .range(..=rva)
                .rev()
                .take(64)
                .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
                .filter(|(at, e)| e.code == code && rva - at < e.size.max(1))
                .collect::<Vec<_>>();
            for (at, e) in containing {
                let Some(our) = e.our else { continue };
                let want = our.saturating_add(rva - at);
                let hit = o.by_key.get(key).is_some_and(|v| {
                    v.iter().any(|(r, stem)| {
                        *r == want
                            && obj.is_none_or(|m| m.strip_suffix(".obj").unwrap_or(m) == stem)
                    })
                });
                if !(hit && self.proven(e, at, if rva == at { len } else { 0 })) {
                    continue;
                }
                if rva > at {
                    return Identity::Linked(
                        "INSIDE",
                        format!(
                            "at {rva:#x} = shipped `{}` {at:#x}+{:#x}, as in our link",
                            e.shown,
                            rva - at
                        ),
                    );
                }
                if weak_default(key).is_some_and(|g| e.carries(&g)) {
                    return Identity::Linked(
                        "ALIAS",
                        format!(
                            "at {rva:#x} = shipped `{}`, the weak external's default, as in our link",
                            e.shown
                        ),
                    );
                }
                if !self.may_fold {
                    return Identity::Fail(format!(
                        "at {rva:#x}: our link puts it on shipped `{}`'s body, but that link is \
                         INCREMENTAL and folds nothing",
                        e.shown
                    ));
                }
                return Identity::Folded(format!(
                    "at {rva:#x} = shipped `{}`, byte-identical, as our link folded it",
                    e.shown
                ));
            }
        }
        let what = here
            .first()
            .map_or_else(|| "nothing".to_string(), |e| format!("`{}`", e.shown));
        Identity::Fail(format!(
            "at {rva:#x}: the shipped side has {what} there, not this name"
        ))
    }
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Row {
    pub ok: usize,
    pub retired: usize,
    pub folded: usize,
    pub linked: usize,
    pub failed: usize,
}

#[derive(Debug, Clone)]
pub struct Tally {
    pub rows: BTreeMap<String, Row>,
    pub examples: BTreeMap<String, Vec<String>>,
    pub proofs: BTreeMap<String, Vec<String>>,
    shown: usize,
}

impl Default for Tally {
    fn default() -> Self {
        Self::new(EXAMPLES)
    }
}

impl Tally {
    #[must_use]
    pub fn new(shown: usize) -> Self {
        Self {
            rows: BTreeMap::new(),
            examples: BTreeMap::new(),
            proofs: BTreeMap::new(),
            shown,
        }
    }
    fn ok(&mut self, class: &str) {
        self.ok_n(class, 1);
    }
    fn ok_n(&mut self, class: &str, n: usize) {
        self.rows.entry(class.to_string()).or_default().ok += n;
    }
    fn retired(&mut self, class: &str) {
        self.rows.entry(class.to_string()).or_default().retired += 1;
    }
    fn folded(&mut self, class: &str, why: String) {
        self.rows.entry(class.to_string()).or_default().folded += 1;
        self.proof(class, "FOLD", why);
    }
    fn linked(&mut self, class: &str, tag: &str, why: String) {
        self.rows.entry(class.to_string()).or_default().linked += 1;
        self.proof(class, tag, why);
    }
    fn proof(&mut self, class: &str, what: &str, why: String) {
        let ex = self.proofs.entry(format!("{what} {class}")).or_default();
        if ex.len() < self.shown {
            ex.push(why);
        }
    }
    fn fail(&mut self, class: &str, why: String) {
        self.rows.entry(class.to_string()).or_default().failed += 1;
        let ex = self.examples.entry(class.to_string()).or_default();
        if ex.len() < self.shown {
            ex.push(why);
        }
    }
    fn identity(&mut self, class: &str, id: Identity, why: &dyn Fn(&str) -> String) {
        match id {
            Identity::Named => self.ok(class),
            Identity::Folded(e) => self.folded(class, why(&e)),
            Identity::Linked(tag, e) => self.linked(class, tag, why(&e)),
            Identity::Fail(e) => self.fail(class, why(&e)),
        }
    }
    #[must_use]
    pub fn failures(&self) -> usize {
        self.rows.values().map(|r| r.failed).sum()
    }
    #[must_use]
    pub fn row(&self, class: &str) -> Row {
        self.rows.get(class).copied().unwrap_or_default()
    }
    pub fn print(&self, w: &mut dyn Write) -> std::io::Result<()> {
        writeln!(
            w,
            "  {:<38} {:>7} {:>8} {:>7} {:>7} {:>7}",
            "field", "live ok", "retired", "folded", "by link", "FAILED"
        )?;
        for (class, r) in &self.rows {
            writeln!(
                w,
                "  {class:<38} {:>7} {:>8} {:>7} {:>7} {:>7}",
                r.ok, r.retired, r.folded, r.linked, r.failed
            )?;
        }
        for (class, ex) in &self.proofs {
            for e in ex {
                writeln!(w, "    {class}: {e}")?;
            }
        }
        for (class, ex) in &self.examples {
            for e in ex {
                writeln!(w, "    FAIL {class}: {e}")?;
            }
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Copy)]
struct Scope {
    live: Option<(u16, u32, u32)>,
    was: Option<(u16, u32)>,
}

#[derive(Debug, Clone)]
struct ModRef {
    index: usize,
    obj: String,
}

#[derive(Debug)]
struct Auditor<'s, 'a> {
    ship: &'s Shipped<'a>,
    tally: Tally,
    claims: BTreeMap<(u16, u32), Vec<(usize, String)>>,
    own_code: BTreeMap<usize, Vec<(u16, u32, u32)>>,
    procs: BTreeMap<(u16, u32), u32>,
    proc_ends: BTreeMap<u32, usize>,
    moved: BTreeMap<usize, Vec<Move>>,
    before: Option<Before>,
    retired_left: Option<BTreeMap<(u16, String, u32), usize>>,
    types: Option<TypeRange>,
    our_procs: BTreeMap<usize, Vec<(u16, u32, u32)>>,
}

type Move = (u16, u32, u32, u16, u32);

#[derive(Debug, Clone, Copy)]
struct TypeRange {
    min: u32,
    mac: u32,
}

impl TypeRange {
    fn resolves(self, ti: u32) -> bool {
        ti < 0x1000 || (self.min..self.mac).contains(&ti)
    }
}

fn typind_at(kind: u16) -> Option<usize> {
    match kind {
        0x1001 | 0x1002 | 0x1003 | 0x1007 | 0x1008 => Some(0),
        0x1006 => Some(4),
        0x100a | 0x100b => Some(24),
        _ => None,
    }
}

#[derive(Debug, Default)]
struct Before {
    contribs: BTreeMap<(usize, u32), Vec<u32>>,
    modrec: BTreeMap<usize, (u32, u32)>,
    fpo: BTreeMap<u32, Vec<(u32, Vec<u8>)>>,
}

impl Before {
    fn read(original: &Msf2, ours: &Image) -> Result<Self> {
        let bytes = original.stream(3)?;
        let dbi = DbiView::parse(&bytes)?;
        let our_rva = |isect: u16, off: u32| {
            ours.sections
                .get(usize::from(isect).checked_sub(1)?)
                .map(|s| s.rva.get().saturating_add(off))
        };
        let mut b = Self::default();
        let (at, len) = dbi.contribs;
        if len >= 4 {
            for k in 0..(len - 4) / SC_SIZE {
                let p = at + 4 + SC_SIZE * k;
                let isect = u16_at(&bytes, p).unwrap_or(0);
                let off = u32_at(&bytes, p + 4).unwrap_or(0);
                let size = u32_at(&bytes, p + 8).unwrap_or(0);
                let imod = usize::from(u16_at(&bytes, p + 16).unwrap_or(0));
                if let Some(r) = our_rva(isect, off) {
                    b.contribs.entry((imod, r)).or_default().push(size);
                }
            }
        }
        for (index, m) in dbi.mods.iter().enumerate() {
            let isect = u16_at(&bytes, m.sc_at).unwrap_or(0);
            let off = u32_at(&bytes, m.sc_at + 4).unwrap_or(0);
            let size = u32_at(&bytes, m.sc_at + 8).unwrap_or(0);
            if let Some(r) = our_rva(isect, off) {
                b.modrec.insert(index, (r, size));
            }
        }
        if let Some(n) = dbi
            .dbg_streams
            .get(DBG_FPO)
            .copied()
            .filter(|n| *n != 0xffff)
        {
            let s = original.stream(usize::from(n))?;
            for rec in s.as_chunks::<FPO_SIZE>().0 {
                if let (Some(start), Some(size), Some(rest)) =
                    (u32_at(rec, 0), u32_at(rec, 4), rec.get(8..))
                {
                    b.fpo.entry(start).or_default().push((size, rest.to_vec()));
                }
            }
        }
        Ok(b)
    }
}

fn joined(v: &[u32]) -> String {
    v.iter()
        .map(|o| format!("{o:#x}"))
        .collect::<Vec<_>>()
        .join(" / ")
}

impl<'s, 'a> Auditor<'s, 'a> {
    fn new(ship: &'s Shipped<'a>, shown: usize) -> Self {
        Self {
            ship,
            tally: Tally::new(shown),
            claims: BTreeMap::new(),
            own_code: BTreeMap::new(),
            procs: BTreeMap::new(),
            proc_ends: BTreeMap::new(),
            moved: BTreeMap::new(),
            before: None,
            retired_left: ship.retired.clone(),
            types: None,
            our_procs: BTreeMap::new(),
        }
    }

    fn ours_of(&self, rva: u32) -> Vec<u32> {
        let all: Vec<u32> = match self.ship.inside_exact(rva) {
            Some(v) => v.into_iter().map(|(_, _, o)| o).collect(),
            None => self
                .ship
                .oracle
                .range(..=rva)
                .rev()
                .take(64)
                .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
                .filter(|(at, e)| rva - at < e.size.max(1))
                .filter_map(|(at, e)| e.our.map(|o| o.saturating_add(rva - at)))
                .collect(),
        };
        let mut seen = BTreeSet::new();
        all.into_iter().filter(|o| seen.insert(*o)).collect()
    }

    #[cfg(test)]
    fn clamp_ok(&self, rva: u32, len: u32, w: u32) -> bool {
        self.clamps(rva, len, &[w]).first().is_some_and(|c| c.1)
    }

    fn add_proc(&mut self, seg: u16, off: u32, len: u32) {
        let end = |l: u32| {
            self.ship
                .sec(seg)
                .map(|s| s.rva.saturating_add(off).saturating_add(l))
        };
        if let Some(old) = self.procs.insert((seg, off), len)
            && let Some(e) = end(old)
            && let Some(n) = self.proc_ends.get_mut(&e)
        {
            *n = n.saturating_sub(1);
            if *n == 0 {
                self.proc_ends.remove(&e);
            }
        }
        if let Some(e) = end(len) {
            *self.proc_ends.entry(e).or_default() += 1;
        }
    }

    fn clamps(&self, rva: u32, len: u32, cands: &[u32]) -> Vec<(u32, bool)> {
        let end = rva.saturating_add(len);
        let sec_end = self
            .ship
            .secs
            .iter()
            .find(|s| rva >= s.rva && rva < s.rva.saturating_add(s.mapped))
            .is_some_and(|s| {
                end == s.rva.saturating_add(s.vsize) || end == s.rva.saturating_add(s.mapped)
            });
        let any = sec_end || self.proc_ends.contains_key(&end);
        let body_ends: BTreeSet<u32> = if any {
            BTreeSet::new()
        } else {
            self.ship
                .oracle
                .range(..=rva)
                .rev()
                .take(64)
                .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
                .filter(|(at, e)| rva - at < e.size.max(1) && at.saturating_add(e.size) == end)
                .filter_map(|(at, e)| e.our.map(|o| o.saturating_add(rva - at)))
                .collect()
        };
        cands
            .iter()
            .map(|o| (*o, any || body_ends.contains(o)))
            .collect()
    }

    fn place(
        &self,
        seg: u16,
        off: u32,
        len: u32,
    ) -> std::result::Result<(u32, &'s ShipSec), String> {
        let s = self
            .ship
            .sec(seg)
            .ok_or_else(|| format!("segment {seg} is not a shipped section"))?;
        let end = off
            .checked_add(len)
            .ok_or_else(|| format!("{seg}:{off:#x}+{len:#x} overflows"))?;
        if off >= s.mapped.max(1) || end > s.mapped {
            return Err(format!(
                "{seg}:{off:#x}+{len:#x} runs past shipped {} ({:#x} bytes)",
                s.name, s.mapped
            ));
        }
        Ok((s.rva + off, s))
    }

    #[cfg(test)]
    fn region(&mut self, bytes: &[u8], where_: &str, module: Option<&ModRef>) {
        self.region_from(bytes, where_, module, None);
    }

    fn region_from(
        &mut self,
        bytes: &[u8],
        where_: &str,
        module: Option<&ModRef>,
        original: Option<&[u8]>,
    ) {
        let all = records(bytes);
        let end = all.last().map_or(0, |(at, kind, _, len)| {
            record_end(bytes, *at, *kind, len + 2)
        });
        if bytes.len().saturating_sub(end) >= 4 {
            self.tally.fail(
                "record walk",
                format!(
                    "{where_}: the walk stops at byte {end} of {} -- a mis-parse",
                    bytes.len()
                ),
            );
        } else {
            self.tally.ok("record walk");
        }
        let mut stack: Vec<Scope> = Vec::new();
        for (at, kind, payload, len) in all {
            if let Some(o) = original {
                let end = record_end(bytes, at, kind, len + 2);
                let mask = classify(kind)
                    .flatten()
                    .map(|f| (payload + f.at, payload + f.at + 6));
                let first = (at..end).find(|i| {
                    !mask.is_some_and(|(a, b)| *i >= a && *i < b) && bytes.get(*i) != o.get(*i)
                });
                match first {
                    None => self.tally.ok("record bytes = --original"),
                    Some(i) => self.tally.fail(
                        "record bytes = --original",
                        format!(
                            "{where_}: {} at {at} differs from --original at byte +{} ({:#04x} for {:#04x}) -- outside its address, which is all a relocation writes",
                            kind_name(kind),
                            i - at,
                            bytes.get(i).copied().unwrap_or(0),
                            o.get(i).copied().unwrap_or(0)
                        ),
                    ),
                }
            }
            if let (Some(t), Some(p)) = (self.types, typind_at(kind))
                && let Some(ti) = u32_at(bytes, payload + p)
            {
                if t.resolves(ti) {
                    self.tally.ok("type index resolves");
                } else {
                    self.tally.fail(
                        "type index resolves",
                        format!(
                            "{where_}: {} at {at} carries type index {ti:#x}, which names no type of this file (TPI {:#x}..{:#x}){}",
                            kind_name(kind),
                            t.min,
                            t.mac,
                            if ti >= 0x0100_0000 {
                                " -- an index into a type server, which does not ship"
                            } else {
                                ""
                            }
                        ),
                    );
                }
            }
            if kind == 0x0006 {
                stack.pop();
                continue;
            }
            let Some(class) = classify(kind) else {
                self.tally.fail(
                    "unclassified record kind",
                    format!(
                        "{where_}: kind {kind:#06x} at {at} -- no rule says whether it holds an address"
                    ),
                );
                continue;
            };
            let Some(f) = class else { continue };
            let label = format!(
                "{} {}",
                if module.is_some() {
                    "module"
                } else {
                    "symrecs"
                },
                kind_name(kind)
            );
            let rec = bytes.get(payload..payload + len).unwrap_or_default();
            let (Some(off), Some(seg)) = (u32_at(rec, f.at), u16_at(rec, f.at + 4)) else {
                self.tally.fail(
                    &label,
                    format!("{where_}: record at {at} too short for its address"),
                );
                if f.scope {
                    stack.push(Scope {
                        live: None,
                        was: None,
                    });
                }
                continue;
            };
            let was = original.and_then(|o| {
                let r = o.get(payload..payload + len)?;
                Some((u16_at(r, f.at + 4)?, u32_at(r, f.at)?))
            });
            let flen = match f.len {
                Some((p, 4)) => u32_at(rec, p).unwrap_or(0),
                Some((p, _)) => u32::from(u16_at(rec, p).unwrap_or(0)),
                None => 0,
            };
            let name = f.name.map(|n| st_name(rec, n)).unwrap_or_default();
            if let (Role::Proc, Some(m), Some((ws, wo))) = (f.role, module, was) {
                self.our_procs
                    .entry(m.index)
                    .or_default()
                    .push((ws, wo, flen));
            }
            let parent = stack.last().copied();
            let live = if seg == 0 {
                if off == 0 {
                    self.retirement(&label, kind, &name, was, where_);
                } else {
                    self.tally.fail(
                        &label,
                        format!(
                            "{where_}: `{name}` has segment 0 but offset {off:#x} (half-retired)"
                        ),
                    );
                }
                None
            } else if usize::from(seg) == self.ship.secs.len() + 1 && f.role == Role::Public {
                self.tally.ok("symrecs S_PUB32 (absolute)");
                None
            } else {
                self.live(
                    &label, f.role, seg, off, flen, &name, parent, was, module, where_,
                )
            };
            if f.scope {
                stack.push(Scope { live, was });
            }
        }
    }

    fn retirement(
        &mut self,
        label: &str,
        kind: u16,
        name: &str,
        was: Option<(u16, u32)>,
        where_: &str,
    ) {
        let w = was
            .filter(|(s, _)| *s != 0)
            .and_then(|(s, o)| self.ship.our_rva(s, o));
        match w {
            None => self.tally.retired(label),
            Some(w) => match self.retired_left.as_mut() {
                Some(left) => {
                    let k = (kind, name.to_string(), w);
                    match left.get_mut(&k) {
                        Some(n) if *n > 0 => {
                            *n -= 1;
                            self.tally.retired(label);
                        }
                        _ => self.tally.fail(
                            label,
                            format!(
                                "{where_}: `{name}` (our {w:#x}) is RETIRED, but the derivation's retired list does not name it -- a live record our link had"
                            ),
                        ),
                    }
                }
                None => self.tally.fail(
                    label,
                    format!(
                        "{where_}: `{name}` (our {w:#x}) is RETIRED with no retired list to justify it (the pairings name `<pairs>.retired.tsv`)"
                    ),
                ),
            },
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn live(
        &mut self,
        label: &str,
        role: Role,
        seg: u16,
        off: u32,
        len: u32,
        name: &str,
        parent: Option<Scope>,
        was: Option<(u16, u32)>,
        module: Option<&ModRef>,
        where_: &str,
    ) -> Option<(u16, u32, u32)> {
        let why = |e: &str| format!("{where_}: `{name}` {e}");
        let span = if matches!(role, Role::Proc | Role::Thunk) {
            len
        } else {
            0
        };
        let (rva, sec) = match self.place(seg, off, span) {
            Ok(p) => p,
            Err(e) => {
                self.tally.fail(label, why(&e));
                return Some((seg, off, len));
            }
        };
        let at_parent_end = role == Role::Child
            && matches!(parent, Some(Scope { live: Some((ps, po, pl)), .. }) if ps == seg && off == po.saturating_add(pl));
        let mut moved_with_its_body = false;
        if let Some(w) = was
            .filter(|(s, _)| *s != 0 && !at_parent_end)
            .and_then(|(s, o)| self.ship.our_rva(s, o))
        {
            let back = self.ours_of(rva);
            moved_with_its_body = self
                .ship
                .inside_exact(rva)
                .is_some_and(|v| v.iter().any(|(_, _, o)| *o == w));
            if !back.contains(&w) {
                let e = self.ship.moved_into(rva, w).unwrap_or_else(|| {
                    format!(
                        "at {rva:#x} is our {} -- this record was our {w:#x}: not where the oracle's pairing puts it",
                        if back.is_empty() {
                            "nothing (no paired body contains it)".to_string()
                        } else {
                            joined(&back)
                        }
                    )
                });
                self.tally.fail(label, why(&e));
                return Some((seg, off, len));
            }
        }
        if role == Role::Data
            && sec.exec
            && let Some(Scope {
                live: Some((ps, po, pl)),
                ..
            }) = parent
            && (ps != seg || off < po || off >= po.saturating_add(pl))
        {
            self.tally.fail(
                label,
                why(&format!(
                    "at {seg}:{off:#x} is code-section data outside its procedure {ps}:{po:#x}+{pl:#x}"
                )),
            );
            return Some((seg, off, len));
        }
        let key = name_key(name);
        let obj = module.map(|m| m.obj.as_str());
        match role {
            Role::Other => self.tally.ok(label),
            Role::Data if key.is_empty() && moved_with_its_body => self.tally.ok(label),
            Role::Public | Role::Data => {
                let id = self
                    .ship
                    .identify(rva, &key, obj, sec.exec && role == Role::Public, 0);
                self.tally.identity(label, id, &why);
            }
            Role::Thunk => {
                if self.ship.import_stub(rva, len) && self.ship.stub_imports(rva, &key) {
                    self.tally.ok(label);
                } else {
                    let id = self.ship.identify(rva, &key, obj, true, len);
                    self.tally.identity(label, id, &why);
                }
            }
            Role::Proc => {
                if !sec.exec {
                    self.tally.fail(
                        label,
                        why(&format!("at {rva:#x} is in non-code section {}", sec.name)),
                    );
                    return Some((seg, off, len));
                }
                if let Err(e) = self.ship.stays_in(rva, len) {
                    self.tally.fail(label, why(&e));
                    return Some((seg, off, len));
                }
                let id = self.ship.identify(rva, &key, obj, true, len);
                let good = !matches!(id, Identity::Fail(_));
                self.tally.identity(label, id, &why);
                if good {
                    if let Some(m) = module {
                        self.claims
                            .entry((seg, off))
                            .or_default()
                            .push((m.index, key));
                        self.own_code
                            .entry(m.index)
                            .or_default()
                            .push((seg, off, len));
                        if let Some((ws, wo)) = was {
                            self.moved
                                .entry(m.index)
                                .or_default()
                                .push((ws, wo, len, seg, off));
                        }
                    }
                    self.add_proc(seg, off, len);
                }
            }
            Role::Child => match parent {
                None => {
                    let id = self.ship.identify(rva, &key, obj, sec.exec, 0);
                    self.tally.identity(label, id, &why);
                }
                Some(Scope { live: None, .. }) => self.tally.fail(
                    label,
                    why(&format!(
                        "at {seg}:{off:#x} is LIVE under a retired procedure"
                    )),
                ),
                Some(Scope {
                    live: Some((ps, po, pl)),
                    was: pwas,
                }) => {
                    let delta = off.checked_sub(po);
                    let kept = match (was, pwas) {
                        (Some((ws, wo)), Some((ps0, po0))) => {
                            ws == ps0 && wo.checked_sub(po0) == delta
                        }
                        _ => true,
                    };
                    let label_ok = match (was, pwas) {
                        (Some(_), Some(_)) => delta.is_some_and(|d| d <= pl),
                        _ => delta.is_some_and(|d| d < pl),
                    };
                    let inside = ps == seg
                        && kept
                        && if len == 0 {
                            label_ok
                        } else {
                            off.checked_add(len)
                                .is_some_and(|e| e <= po.saturating_add(pl))
                                && delta.is_some()
                        };
                    if inside {
                        self.tally.ok(label);
                    } else {
                        self.tally.fail(
                            label,
                            why(&format!(
                                "at {seg}:{off:#x}+{len:#x} lies outside its procedure {ps}:{po:#x}+{pl:#x}"
                            )),
                        );
                    }
                }
            },
        }
        Some((seg, off, len))
    }

    fn cross_module(&mut self) {
        let claims = std::mem::take(&mut self.claims);
        for who in claims.values() {
            let first = who.first().map(|w| w.0);
            if who.iter().all(|w| Some(w.0) == first) {
                self.tally.ok("procedure claimed by two modules");
            } else {
                let mods: Vec<String> = who.iter().map(|w| w.0.to_string()).collect();
                let why = format!("modules {} at one shipped function", mods.join(", "));
                if self.ship.may_fold {
                    self.tally.folded("procedure claimed by two modules", why);
                } else {
                    self.tally.fail(
                        "procedure claimed by two modules",
                        format!("{why}, on an INCREMENTAL link that folds nothing"),
                    );
                }
            }
        }
    }

    #[cfg(test)]
    fn c11(&mut self, b: &[u8], where_: &str, module: &ModRef) {
        self.c11_from(b, where_, module, None);
    }

    fn want_line(&self, module: &ModRef, name: Option<&str>, line: u16) -> u16 {
        name.and_then(|n| {
            self.ship
                .line_dirs
                .get(&module.obj)
                .filter(|d| d.names(n))
                .and_then(|d| d.physical(n, line))
        })
        .unwrap_or(line)
    }

    fn c11_from(&mut self, b: &[u8], where_: &str, module: &ModRef, original: Option<&[u8]>) {
        let (Some(c_file), Some(c_seg)) = (u16_at(b, 0), u16_at(b, 2)) else {
            return;
        };
        let (c_file, c_seg) = (usize::from(c_file), usize::from(c_seg));
        if c_file
            .saturating_mul(4)
            .saturating_add(c_seg.saturating_mul(10))
            > b.len()
        {
            self.tally.fail(
                "C11 file",
                format!(
                    "{where_}: {c_file} file(s) and {c_seg} segment(s) do not fit {} bytes",
                    b.len()
                ),
            );
            return;
        }
        let se_at = 4 + 4 * c_file;
        let seg_at = se_at + 8 * c_seg;
        for i in 0..c_seg {
            let seg = u16_at(b, seg_at + 2 * i).unwrap_or(0);
            self.range(b, se_at + 8 * i, seg, None, where_);
        }
        let own = self
            .own_code
            .get(&module.index)
            .cloned()
            .unwrap_or_default();
        let moved = self.moved.get(&module.index).cloned().unwrap_or_default();
        for i in 0..c_file {
            let Some(base) = u32_at(b, 4 + 4 * i).and_then(|v| usize::try_from(v).ok()) else {
                self.tally
                    .fail("C11 file", format!("{where_}: file table truncated"));
                continue;
            };
            let cc = usize::from(u16_at(b, base).unwrap_or(0));
            if cc.saturating_mul(12) > b.len() {
                self.tally.fail(
                    "C11 file",
                    format!("{where_}: a file record at {base:#x} names {cc} blocks"),
                );
                continue;
            }
            let fse = base + 4 + 4 * cc;
            let name = crate::reloc::c11_file_name(b, fse + 8 * cc);
            for j in 0..cc {
                let Some(at) = u32_at(b, base + 4 + 4 * j).and_then(|v| usize::try_from(v).ok())
                else {
                    self.tally
                        .fail("C11 block", format!("{where_}: block table truncated"));
                    continue;
                };
                let seg = u16_at(b, at).unwrap_or(0);
                let pairs = usize::from(u16_at(b, at + 2).unwrap_or(0));
                let mut offs = Vec::with_capacity(pairs.min(b.len() / 4));
                for k in 0..pairs {
                    if let Some(o) = u32_at(b, at + 4 + 4 * k) {
                        offs.push(o);
                    }
                }
                if pairs == 0 {
                    self.tally.retired("C11 block");
                } else {
                    let stray = self.block_home(
                        b,
                        at,
                        seg,
                        pairs,
                        &offs,
                        original,
                        &moved,
                        &own,
                        module,
                        name.as_deref(),
                        where_,
                    );
                    match stray {
                        None => self.tally.ok("C11 block"),
                        Some(e) => self.tally.fail("C11 block", e),
                    }
                }
                for o in &offs {
                    match self.place(seg, *o, 0) {
                        Ok(_) => self.tally.ok("C11 line"),
                        Err(e) => self.tally.fail("C11 line", format!("{where_}: {e}")),
                    }
                }
                self.range(b, fse + 8 * j, seg, Some(&offs), where_);
            }
        }
        if let Some(o) = original {
            self.c11_lines(b, o, where_, module);
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn block_home(
        &self,
        b: &[u8],
        at: usize,
        seg: u16,
        pairs: usize,
        offs: &[u32],
        original: Option<&[u8]>,
        moved: &[Move],
        own: &[(u16, u32, u32)],
        module: &ModRef,
        name: Option<&str>,
        where_: &str,
    ) -> Option<String> {
        let was_seg = original.and_then(|o| u16_at(o, at));
        let mut stray: Option<String> = None;
        for (k, x) in offs.iter().enumerate() {
            let was = original.and_then(|o| u32_at(o, at + 4 + 4 * k));
            let homes: Vec<(u16, u32)> = match (was_seg, was) {
                (Some(ws), Some(w)) => moved
                    .iter()
                    .filter(|(ps, po, pl, _, _)| *ps == ws && w >= *po && w - po < (*pl).max(1))
                    .map(|(_, po, _, ns, no)| (*ns, no.saturating_add(w - po)))
                    .collect(),
                _ => Vec::new(),
            };
            let home = homes.first().copied();
            let ok = if homes.is_empty() {
                own.iter()
                    .any(|(s, o, l)| *s == seg && *x >= *o && *x - o < (*l).max(1))
            } else {
                homes.iter().any(|(ns, want)| *ns == seg && want == x)
            };
            if !ok && stray.is_none() {
                stray = Some(match home {
                    Some((ns, want)) => format!(
                        "{where_}: a line at {seg}:{x:#x} should be at {ns}:{want:#x}, the same offset of its own procedure"
                    ),
                    None => format!(
                        "{where_}: a line at {seg}:{x:#x} lies in none of {}'s own procedures or contributions",
                        module.obj
                    ),
                });
            }
        }
        if stray.is_some() && original.is_some() {
            let line_no = |bytes: &[u8], k: usize| u16_at(bytes, at + 4 + 4 * pairs + 2 * k);
            let mut want: Vec<(u32, Option<u16>)> = Vec::with_capacity(pairs);
            let mut have: Vec<(u32, Option<u16>)> = Vec::with_capacity(pairs);
            let mut complete = true;
            for (k, x) in offs.iter().enumerate() {
                let w = original.and_then(|o| u32_at(o, at + 4 + 4 * k));
                let home = match (was_seg, w) {
                    (Some(ws), Some(w)) => moved
                        .iter()
                        .find(|(ps, po, pl, _, _)| *ps == ws && w >= *po && w - po < (*pl).max(1))
                        .map(|(_, po, _, _, no)| no.saturating_add(w - po)),
                    _ => None,
                };
                match home {
                    Some(h) => want.push((
                        h,
                        original
                            .and_then(|o| line_no(o, k))
                            .map(|l| self.want_line(module, name, l)),
                    )),
                    None => complete = false,
                }
                have.push((*x, line_no(b, k)));
            }
            want.sort_unstable();
            have.sort_unstable();
            if complete && want == have {
                stray = None;
            }
        }
        stray
    }

    fn c11_lines(&mut self, b: &[u8], o: &[u8], where_: &str, module: &ModRef) {
        let (Some(cf), Some(cs)) = (u16_at(b, 0), u16_at(b, 2)) else {
            return;
        };
        let (cf, cs) = (usize::from(cf), usize::from(cs));
        let moved = self.moved.get(&module.index).cloned().unwrap_or_default();
        let own = self
            .own_code
            .get(&module.index)
            .cloned()
            .unwrap_or_default();
        let ours = self
            .our_procs
            .get(&module.index)
            .cloned()
            .unwrap_or_default();
        let homes = |ws: u16, w: u32| -> Vec<(u16, u32)> {
            moved
                .iter()
                .filter(|(ps, po, pl, _, _)| *ps == ws && w >= *po && w - po < (*pl).max(1))
                .map(|(_, po, _, ns, no)| (*ns, no.saturating_add(w - po)))
                .collect()
        };
        let mut cover: BTreeMap<u16, (u16, u32, u32)> = BTreeMap::new();
        for i in 0..cf {
            let Some(base) = u32_at(b, 4 + 4 * i).and_then(|v| usize::try_from(v).ok()) else {
                continue;
            };
            let cc = usize::from(u16_at(b, base).unwrap_or(0));
            if cc.saturating_mul(12) > b.len() {
                continue;
            }
            let fse = base + 4 + 4 * cc;
            let name = crate::reloc::c11_file_name(b, fse + 8 * cc);
            for j in 0..cc {
                let Some(at) = u32_at(b, base + 4 + 4 * j).and_then(|v| usize::try_from(v).ok())
                else {
                    continue;
                };
                let (seg_n, n) = (
                    u16_at(b, at).unwrap_or(0),
                    usize::from(u16_at(b, at + 2).unwrap_or(0)),
                );
                let (seg_o, n_o) = (
                    u16_at(o, at).unwrap_or(0),
                    usize::from(u16_at(o, at + 2).unwrap_or(0)),
                );
                let mut orig: Vec<Option<(u32, u16)>> = (0..n_o.min(o.len() / 4))
                    .map(|k| {
                        Some((
                            u32_at(o, at + 4 + 4 * k)?,
                            u16_at(o, at + 4 + 4 * n_o + 2 * k)?,
                        ))
                    })
                    .collect();
                let old_max = orig.iter().flatten().map(|p| p.0).max().unwrap_or(0);
                let mut stray: Option<String> = None;
                let mut first: Option<u32> = None;
                let mut last: Option<(u32, u32)> = None;
                if n > n_o {
                    stray = Some(format!(
                        "{where_}: the block at {at} holds {n} line pairs where our link's held {n_o}"
                    ));
                }
                for k in 0..n.min(n_o) {
                    if stray.is_some() {
                        break;
                    }
                    let (Some(x), Some(line)) =
                        (u32_at(b, at + 4 + 4 * k), u16_at(b, at + 4 + 4 * n + 2 * k))
                    else {
                        continue;
                    };
                    let q = (0..orig.len()).find(|q| {
                        orig.get(*q).copied().flatten().is_some_and(|(w, l)| {
                            self.want_line(module, name.as_deref(), l) == line && {
                                let h = homes(seg_o, w);
                                if h.is_empty() {
                                    own.iter().any(|(s, of, le)| {
                                        *s == seg_n && x >= *of && x - of < (*le).max(1)
                                    })
                                } else {
                                    h.iter().any(|(ns, nx)| *ns == seg_n && *nx == x)
                                }
                            }
                        })
                    });
                    match q.and_then(|q| orig.get_mut(q).and_then(Option::take)) {
                        Some((w, _)) => {
                            first = Some(first.map_or(x, |f| f.min(x)));
                            if last.is_none_or(|(lx, _)| x >= lx) {
                                last = Some((x, w));
                            }
                        }
                        None => {
                            stray = Some(format!(
                                "{where_}: line {line} at {seg_n}:{x:#x} is no line of our link moved with its procedure -- an offset and its line number travel together"
                            ));
                        }
                    }
                }
                if stray.is_none()
                    && n < n_o
                    && b.get(at + 4 + 6 * n..at + 4 + 6 * n_o)
                        .is_some_and(|z| z.iter().any(|c| *c != 0))
                {
                    stray = Some(format!(
                        "{where_}: past its {n} kept pairs the block at {at} is not zero-filled"
                    ));
                }
                let range_at = fse + 8 * j;
                let have = (
                    u32_at(b, range_at).unwrap_or(0),
                    u32_at(b, range_at + 4).unwrap_or(0),
                );
                let want = match (first, last) {
                    (Some(f), Some((lx, lw))) => {
                        let old_end = u32_at(o, range_at + 4).unwrap_or(0);
                        let tail = ours
                            .iter()
                            .find(|(s, po, pl)| *s == seg_o && lw >= *po && lw - po < *pl)
                            .map_or_else(
                                || old_end.saturating_sub(old_max),
                                |(_, po, pl)| {
                                    po.saturating_add(*pl).saturating_sub(1).saturating_sub(lw)
                                },
                            );
                        (f, lx.saturating_add(tail))
                    }
                    _ => (0, 0),
                };
                if stray.is_none() && have != want {
                    stray = Some(format!(
                        "{where_}: the block at {at} has range {:#x}..{:#x}, its kept lines make {:#x}..{:#x}",
                        have.0, have.1, want.0, want.1
                    ));
                }
                if n > 0 {
                    let c = cover.entry(seg_o).or_insert((seg_n, want.0, want.1));
                    c.1 = c.1.min(want.0);
                    c.2 = c.2.max(want.1);
                }
                match stray {
                    None => self.tally.ok("C11 lines = --original, moved"),
                    Some(e) => self.tally.fail("C11 lines = --original, moved", e),
                }
            }
        }
        let se_at = 4 + 4 * cf;
        let seg_at = se_at + 8 * cs;
        for i in 0..cs.min(b.len() / 8) {
            let seg_o = u16_at(o, seg_at + 2 * i).unwrap_or(0);
            let want = cover.get(&seg_o).copied().unwrap_or((seg_o, 0, 0));
            let have = (
                u16_at(b, seg_at + 2 * i).unwrap_or(0),
                u32_at(b, se_at + 8 * i).unwrap_or(0),
                u32_at(b, se_at + 8 * i + 4).unwrap_or(0),
            );
            if have == want {
                self.tally.ok("C11 module segment range");
            } else {
                self.tally.fail(
                    "C11 module segment range",
                    format!(
                        "{where_}: segment entry {i} says {}:{:#x}..{:#x}; its blocks' kept lines bound {}:{:#x}..{:#x}",
                        have.0, have.1, have.2, want.0, want.1, want.2
                    ),
                );
            }
        }
    }

    fn range(&mut self, b: &[u8], at: usize, seg: u16, lines: Option<&[u32]>, where_: &str) {
        let (Some(start), Some(end)) = (u32_at(b, at), u32_at(b, at + 4)) else {
            self.tally
                .fail("C11 range", format!("{where_}: range truncated"));
            return;
        };
        if start == 0 && end == 0 {
            self.tally.retired("C11 range");
            return;
        }
        if end < start {
            self.tally.fail(
                "C11 range",
                format!("{where_}: range {start:#x}..{end:#x} runs backwards"),
            );
            return;
        }
        if let Err(e) = self.place(seg, start, end - start) {
            self.tally.fail("C11 range", format!("{where_}: {e}"));
            return;
        }
        if let Some(ls) = lines
            && let Some(o) = ls.iter().find(|o| **o < start || **o > end)
        {
            self.tally.fail(
                "C11 range",
                format!(
                    "{where_}: line at {seg}:{o:#x} outside its block's range {start:#x}..{end:#x}"
                ),
            );
            return;
        }
        self.tally.ok("C11 range");
    }

    fn contrib(
        &mut self,
        class: &str,
        isect: u16,
        off: u32,
        size: u32,
        module: Option<&ModRef>,
    ) -> Option<(u16, u32, u32)> {
        if isect == 0 {
            if off == 0 && size == 0 {
                self.tally.retired(class);
            } else {
                self.tally.fail(
                    class,
                    format!("section 0 with offset {off:#x} size {size:#x} (half-retired)"),
                );
            }
            return None;
        }
        let (rva, sec) = match self.place(isect, off, size) {
            Ok(p) => p,
            Err(e) => {
                self.tally.fail(class, e);
                return None;
            }
        };
        let here = format!("{isect}:{off:#x}+{size:#x}");
        if let (true, Some(m)) = (self.before.is_some(), module) {
            let back = self.ours_of(rva);
            let clamps = self.clamps(rva, size, &back);
            let modrec = class.starts_with("module record");
            let matched = self.before.as_mut().and_then(|b| {
                if modrec {
                    let (w, z) = b.modrec.get(&m.index).copied()?;
                    clamps
                        .iter()
                        .find(|(o, c)| *o == w && (z == size || (z > size && *c)))
                        .map(|_| (w, z))
                } else {
                    clamps.iter().find_map(|(o, c)| {
                        let v = b.contribs.get_mut(&(m.index, *o))?;
                        let k = v.iter().position(|z| *z == size || (*z > size && *c))?;
                        Some((*o, v.swap_remove(k)))
                    })
                }
            });
            if matched.is_none() {
                self.tally.fail(
                    class,
                    format!(
                        "{} at {here}: shipped {rva:#x} is our {}, where no contribution of this module remains in our link of this length or clamped from a longer one at a body end",
                        m.obj,
                        if back.is_empty() {
                            "nothing (no paired body contains it)".to_string()
                        } else {
                            joined(&back)
                        }
                    ),
                );
                return None;
            }
        }
        if sec.exec {
            let m = module.map(|m| m.index);
            let own = m.and_then(|i| self.own_code.get(&i)).and_then(|v| {
                v.iter()
                    .find(|(s, o, l)| *s == isect && *o == off && size <= (*l).max(1))
                    .copied()
            });
            if own.is_some() {
                self.tally.ok(class);
                return Some((isect, off, size));
            }
            let here_entries = self
                .ship
                .oracle
                .get(&rva)
                .map(Vec::as_slice)
                .unwrap_or_default();
            let at_its_function = here_entries.iter().any(|e| {
                e.code
                    && match (&e.obj, module) {
                        (Some(fo), Some(mm)) => *fo == mm.obj,
                        (None, _) => true,
                        (Some(_), None) => false,
                    }
            });
            let in_a_pairing = self
                .ship
                .oracle
                .range(..=rva)
                .rev()
                .take(64)
                .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
                .any(|(at, e)| {
                    e.source == Source::Pairs && rva - at < e.size && size <= e.size - (rva - at)
                });
            if !(at_its_function || in_a_pairing) {
                self.tally.fail(
                    class,
                    format!(
                        "{} at {here} starts at no procedure of its own and no shipped function of its object",
                        module.map_or("?", |m| m.obj.as_str())
                    ),
                );
                return None;
            }
            if !in_a_pairing && let Err(e) = self.ship.stays_in(rva, size) {
                self.tally.fail(class, format!("{here}: {e}"));
                return None;
            }
            self.tally.ok(class);
            if let Some(k) = m {
                self.own_code.entry(k).or_default().push((isect, off, size));
            }
            return Some((isect, off, size));
        }
        let inside = self
            .ship
            .oracle
            .range(..=rva)
            .rev()
            .take(64)
            .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
            .any(|(at, e)| rva - at < e.size && size <= e.size - (rva - at));
        if inside {
            self.tally.ok(class);
            Some((isect, off, size))
        } else {
            self.tally.fail(
                class,
                format!("{here} lies inside no datum the shipped side knows"),
            );
            None
        }
    }
}

fn ilt_slots(db: &Msf2) -> Result<u32> {
    let dbi = DbiView::parse(&db.stream(3)?)?;
    if dbi.publics == 0xffff {
        return Ok(0);
    }
    let s = db.stream(usize::from(dbi.publics))?;
    Ok(u32_at(&s, 8).unwrap_or(0))
}

pub fn run(
    i: &Inputs<'_>,
    pdb_label: &str,
    db: &Msf2,
    mut original: Option<Msf2>,
    w: &mut dyn Write,
) -> Result<Tally> {
    let mut ship = Shipped::load(i)?;
    let slots = ilt_slots(db)?;
    ship.may_fold = slots == 0;
    let entries: usize = ship.oracle.values().map(Vec::len).sum();
    writeln!(
        w,
        "audit {pdb_label} against the SHIPPED {} ({} section(s); identity oracle: {entries} name(s) at {} \
         address(es) from {}{}{})",
        i.image_label,
        ship.secs.len(),
        ship.oracle.len(),
        i.report
            .map(|(_, l)| l)
            .or(i.expect.map(|(_, l)| l))
            .unwrap_or_default(),
        i.pairs.map_or_else(String::new, |(_, l)| format!(" + {l}")),
        i.ours.map_or_else(
            || "; NO --ours, so no fold can be proved".to_string(),
            |(_, _, l)| format!("; our link {l} proves folds")
        ),
    )?;
    if slots > 0 {
        writeln!(
            w,
            "  link: INCREMENTAL ({slots} ILT slot(s) in the publics thunk map) -- it folds nothing, \
             so no name may pass as a fold"
        )?;
    } else {
        writeln!(
            w,
            "  link: full (no ILT) -- an /OPT:ICF fold passes only where our link proves it"
        )?;
    }
    let policy = if i.report.is_some() { i.policy } else { None };
    if let (Some(o), Some(p)) = (original.as_mut(), policy) {
        let s = crate::paths::rename(o, p)?;
        writeln!(
            w,
            "  --original renamed as the relocation renames it: {} C11 file record(s), {} file-info name(s), \
             {} line region(s) grown, {} -> {} page(s)",
            s.records.values().sum::<usize>(),
            s.file_info.values().sum::<usize>(),
            s.grown.len(),
            s.pages.0,
            s.pages.1
        )?;
    }
    if let Some(o) = &original
        && o.file_len() != db.file_len()
    {
        bail!(
            "--original is {} bytes (its source names published) and the audited PDB {}: a relocation is \
             in place on the renamed file, so this is not the PDB it started from",
            o.file_len(),
            db.file_len()
        );
    }
    file::audit(db, &ship, original.as_ref(), i, policy, w)
}

fn fpo(a: &mut Auditor<'_, '_>, s: &[u8]) {
    if !s.len().is_multiple_of(FPO_SIZE) {
        a.tally.fail(
            "FPO",
            format!("stream of {} bytes is not whole FPO_DATA records", s.len()),
        );
    }
    let mut prev: Option<u32> = None;
    for k in 0..s.len() / FPO_SIZE {
        let start = u32_at(s, FPO_SIZE * k).unwrap_or(0);
        let size = u32_at(s, FPO_SIZE * k + 4).unwrap_or(0);
        if let Some(p) = prev.filter(|p| *p > start) {
            a.tally.fail(
                "FPO sorted by start",
                format!("entry {k} at {start:#x} follows {p:#x}"),
            );
        } else {
            a.tally.ok("FPO sorted by start");
        }
        prev = Some(start);
        if start == 0 && size == 0 {
            a.tally.retired("FPO");
            continue;
        }
        let seg_off = a.ship.secs.iter().enumerate().find_map(|(j, x)| {
            (x.exec
                && start >= x.rva
                && start.saturating_add(size) <= x.rva.saturating_add(x.mapped))
            .then(|| (u16::try_from(j + 1).unwrap_or(0), start - x.rva))
        });
        let Some((seg, off)) = seg_off else {
            a.tally.fail(
                "FPO",
                format!("entry {k} {start:#x}+{size:#x} is not inside a shipped code section"),
            );
            continue;
        };
        if a.before.is_some() {
            let rest = s
                .get(FPO_SIZE * k + 8..FPO_SIZE * (k + 1))
                .unwrap_or_default();
            let back = a.ours_of(start);
            let clamps = a.clamps(start, size, &back);
            let matched = a.before.as_mut().and_then(|b| {
                clamps.iter().find_map(|(o, c)| {
                    let v = b.fpo.get_mut(o)?;
                    let j = v
                        .iter()
                        .position(|(z, r)| r == rest && (*z == size || (*z > size && *c)))?;
                    Some(v.swap_remove(j))
                })
            });
            if matched.is_none() {
                a.tally.fail(
                    "FPO",
                    format!(
                        "entry {k} at {start:#x}+{size:#x}: no FPO record of our link remains with these fields and this length (or clamped from a longer one at a body end) at {}",
                        if back.is_empty() {
                            "any paired body".to_string()
                        } else {
                            joined(&back)
                        }
                    ),
                );
                continue;
            }
        }
        if let Some(len) = a.procs.get(&(seg, off)).copied() {
            if len == size {
                a.tally.ok("FPO");
            } else {
                a.tally.fail(
                    "FPO",
                    format!(
                        "entry {k} at {start:#x} covers {size:#x} bytes, and the procedure that starts there is {len:#x} -- not its procedure"
                    ),
                );
            }
            continue;
        }
        let at_start = a
            .ship
            .oracle
            .get(&start)
            .and_then(|es| es.iter().filter(|e| e.code).max_by_key(|e| e.size));
        let whole = a
            .ship
            .oracle
            .range(..=start)
            .rev()
            .take(64)
            .flat_map(|(at, es)| es.iter().map(move |e| (*at, e)))
            .any(|(at, e)| {
                (e.code || e.source == Source::Pairs)
                    && start - at < e.size
                    && size <= e.size - (start - at)
            });
        if whole || (at_start.is_some() && a.ship.stays_in(start, size).is_ok()) {
            a.tally.ok("FPO");
        } else {
            a.tally.fail(
                "FPO",
                match at_start {
                    Some(e) => format!(
                        "entry {k} at {start:#x}+{size:#x} overruns shipped `{}` and lies in no body the shipped side knows",
                        e.shown
                    ),
                    None => format!(
                        "entry {k} at {start:#x}+{size:#x} starts no procedure and lies in no body the shipped side knows"
                    ),
                },
            );
        }
    }
}

fn segmap(a: &mut Auditor<'_, '_>, dbi: &[u8], (at, len): (usize, usize)) {
    if len < 4 {
        return;
    }
    let count = usize::from(u16_at(dbi, at).unwrap_or(0));
    for k in 0..count {
        let rec = at + 4 + SEGMAP_SIZE * k;
        if rec + SEGMAP_SIZE > at + len {
            a.tally
                .fail("segment map", format!("descriptor {k} truncated"));
            break;
        }
        let flags = u16_at(dbi, rec).unwrap_or(0);
        let frame = u16_at(dbi, rec + 6).unwrap_or(0);
        let off = u32_at(dbi, rec + 12).unwrap_or(0);
        let size = u32_at(dbi, rec + 16).unwrap_or(0);
        if flags & 0x0200 != 0 {
            a.tally.ok("segment map");
            continue;
        }
        match a.ship.sec(frame) {
            Some(sec) if off == 0 && size == sec.vsize => a.tally.ok("segment map"),
            Some(sec) => a.tally.fail(
                "segment map",
                format!(
                    "descriptor {k} (frame {frame}) says offset {off:#x} length {size:#x}; LINK writes 0 / {:#x} for shipped {}",
                    sec.vsize, sec.name
                ),
            ),
            None => a.tally.fail(
                "segment map",
                format!("descriptor {k} names frame {frame}, not a shipped section"),
            ),
        }
    }
}
