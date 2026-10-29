use crate::linkmap::Map;
use crate::symbols;
use crate::verbose::{self, Load};
use anyhow::{Context as _, Result, bail};
use msvc6_image::coff::{
    CLASS_EXTERNAL, CLASS_STATIC, Object, RELOC, SCN_CNT_CODE, SCN_LNK_COMDAT,
};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet, HashMap};

pub const SCN_LNK_INFO: u32 = 0x200;

pub const SCN_LNK_REMOVE: u32 = 0x800;

pub const HEAD: usize = 16;

pub const SPAN_BUDGET: u64 = 1 << 24;

pub const BYTES_BUDGET: usize = 4;

pub const CENSUSED: [&str; 3] = [".data", ".rdata", ".bss"];

#[derive(Debug, Clone, Default)]
struct Sec {
    name: String,
    size: u32,
    code: bool,
    comdat: bool,
    info: bool,
    data: Vec<u8>,
    relocs: Vec<(u32, u32)>,
}

impl Sec {
    fn censused(&self) -> bool {
        !self.comdat && !self.code && !self.info && CENSUSED.contains(&self.name.as_str())
    }

    fn addend(&self, at: u32) -> i64 {
        let i = at as usize;
        self.data
            .get(i..i.saturating_add(4))
            .and_then(|b| <[u8; 4]>::try_from(b).ok())
            .map_or(0, |b| i64::from(i32::from_le_bytes(b)))
    }
}

#[derive(Debug, Clone, Default)]
struct Sym {
    name: String,
    section: Option<u16>,
    value: u32,
    external: bool,
    named: bool,
    common: bool,
}

#[derive(Debug, Clone, Default)]
pub struct Obj {
    pub name: String,
    secs: BTreeMap<u16, Sec>,
    syms: BTreeMap<u32, Sym>,
    dead: BTreeSet<u16>,
    leader: BTreeMap<u16, String>,
    starts: BTreeMap<u16, Vec<u32>>,
    holders: BTreeMap<u16, Vec<(u32, bool, u32)>>,
}

#[must_use]
pub fn object_key(spelled: &str) -> String {
    let lower = spelled.trim().to_ascii_lowercase();
    match lower.split_once('(') {
        Some((lib, member)) => {
            let lib = lib.rsplit(['\\', '/']).next().unwrap_or(lib);
            format!("{lib}({member}")
        }
        None => lower
            .rsplit(['\\', '/'])
            .next()
            .unwrap_or(&lower)
            .to_string(),
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Log {
    pub loaded: BTreeSet<String>,
    pub discarded: BTreeSet<(String, String)>,
}

#[must_use]
pub fn parse_log(text: &str) -> Log {
    let loaded = verbose::loads(text)
        .iter()
        .map(Load::spelled)
        .map(|s| object_key(&s))
        .collect();
    let discarded = verbose::discarded(text)
        .into_iter()
        .map(|(sym, obj)| (object_key(&obj), sym))
        .collect();
    Log { loaded, discarded }
}

#[must_use]
pub fn is_verbose_log(text: &str) -> bool {
    text.contains("Searching Libraries") && text.contains("Loaded ")
}

#[must_use]
pub fn last_configuration(log: &str) -> Option<String> {
    log.lines()
        .filter_map(|l| {
            l.trim()
                .trim_matches('-')
                .strip_prefix("Configuration: ")
                .map(|c| c.trim().to_string())
        })
        .next_back()
}

#[must_use]
pub fn link_line(dsp: &str, cfg: &str) -> Option<String> {
    let want = format!("\"$(CFG)\" == \"{cfg}\"");
    let mut inside = false;
    for l in dsp.lines() {
        let t = l.trim();
        if t.starts_with("!IF") || t.starts_with("!ELSEIF") {
            inside = t.contains(&want);
        } else if t.starts_with("!ENDIF") {
            inside = false;
        } else if inside && let Some(rest) = t.strip_prefix("# ADD LINK32") {
            return Some(rest.trim().to_string());
        }
    }
    None
}

#[must_use]
pub fn opt_ref(switches: &str) -> bool {
    let s = switches.to_ascii_lowercase();
    let words: Vec<&str> = s.split_whitespace().collect();
    let opt = |o: &str| {
        words
            .iter()
            .any(|w| w.starts_with("/opt:") && w.split([':', ',']).any(|x| x == o))
    };
    if opt("ref") {
        return true;
    }
    if opt("noref") {
        return false;
    }
    let has = |t: &str| {
        words
            .iter()
            .any(|w| *w == t || w.strip_prefix(t).is_some_and(|r| r.starts_with(',')))
    };
    !(has("/debug") || has("/incremental:yes"))
}

pub fn read_object(name: &str, d: &[u8], log: &Log) -> Result<Obj> {
    let f = Object::parse(d).with_context(|| format!("{name}: not a COFF object"))?;
    let budget = d.len() / RELOC + 1;
    let bytes_budget = d.len().saturating_mul(BYTES_BUDGET);
    let (mut read, mut copied) = (0usize, 0usize);
    let mut secs = BTreeMap::new();
    for s in &f.sections {
        read = read.saturating_add(usize::from(s.relocs));
        copied = copied.saturating_add(f.section_data(s).len());
        if read > budget || copied > bytes_budget {
            bail!(
                "{name}: its sections name more relocations or bytes than the {} bytes of the object hold",
                d.len()
            );
        }
        let chars = s.characteristics;
        secs.insert(
            s.number,
            Sec {
                info: chars & (SCN_LNK_REMOVE | SCN_LNK_INFO) != 0 || s.name.starts_with(".debug"),
                name: s.name.clone(),
                size: s.size,
                code: chars & SCN_CNT_CODE != 0,
                comdat: chars & SCN_LNK_COMDAT != 0,
                data: f.section_data(s).to_vec(),
                relocs: f.relocations(s).iter().map(|r| (r.va, r.symbol)).collect(),
            },
        );
    }
    let mut syms = BTreeMap::new();
    for s in &f.symbols {
        let section = s.section_number();
        let external = s.class == CLASS_EXTERNAL;
        syms.insert(
            s.index,
            Sym {
                common: external && s.section == 0 && s.value > 0,
                named: (external || s.class == CLASS_STATIC)
                    && !s.name.is_empty()
                    && !s.name.starts_with('.'),
                name: s.name.clone(),
                section,
                value: s.value,
                external,
            },
        );
    }
    let assoc = symbols::associates(&f);
    let groups: Vec<(String, Vec<u16>)> = symbols::leaders(&f)
        .into_iter()
        .map(|l| {
            let mut v = vec![l.section];
            v.extend(assoc.get(&l.section).into_iter().flatten().copied());
            (l.name, v)
        })
        .collect();
    Ok(Obj::new(name, secs, syms, &groups, &log.discarded))
}

impl Obj {
    fn new(
        name: &str,
        secs: BTreeMap<u16, Sec>,
        syms: BTreeMap<u32, Sym>,
        groups: &[(String, Vec<u16>)],
        discarded: &BTreeSet<(String, String)>,
    ) -> Self {
        let me = object_key(name);
        let mut dead = BTreeSet::new();
        let mut leader = BTreeMap::new();
        for (sym, sections) in groups {
            let gone = discarded.contains(&(me.clone(), sym.clone()));
            for &s in sections {
                leader.insert(s, sym.clone());
                if gone {
                    dead.insert(s);
                }
            }
        }
        let mut starts: BTreeMap<u16, Vec<u32>> = BTreeMap::new();
        let mut holders: BTreeMap<u16, Vec<(u32, bool, u32)>> = BTreeMap::new();
        for (index, s) in &syms {
            let Some(n) = s.section else { continue };
            if !s.named {
                continue;
            }
            holders
                .entry(n)
                .or_default()
                .push((s.value, s.external, *index));
            if secs.get(&n).is_some_and(Sec::censused) {
                starts.entry(n).or_default().push(s.value);
            }
        }
        for v in starts.values_mut() {
            v.sort_unstable();
            v.dedup();
        }
        for v in holders.values_mut() {
            v.sort_unstable();
        }
        Self {
            name: name.to_string(),
            secs,
            syms,
            dead,
            leader,
            starts,
            holders,
        }
    }

    fn hull(&self, n: u16, at: i64) -> Option<u32> {
        let size = self.secs.get(&n)?.size;
        if at < 0 || at >= i64::from(size.max(1)) {
            return None;
        }
        let at = u32::try_from(at).ok()?;
        let v = self.starts.get(&n)?;
        let i = v.partition_point(|x| *x <= at);
        v.get(i.checked_sub(1)?).copied()
    }

    fn hull_len(&self, n: u16, start: u32) -> u32 {
        let size = self.secs.get(&n).map_or(0, |s| s.size);
        let next = self.starts.get(&n).and_then(|v| {
            let i = v.partition_point(|x| *x <= start);
            v.get(i).copied()
        });
        next.unwrap_or(size).saturating_sub(start)
    }

    fn holder(&self, n: u16, at: u32) -> String {
        if let Some(l) = self.leader.get(&n) {
            return l.clone();
        }
        let named = self.holders.get(&n).and_then(|v| {
            let i = v.partition_point(|(value, _, _)| *value <= at);
            v.get(i.checked_sub(1)?)
        });
        match named.and_then(|(_, _, ix)| self.syms.get(ix)) {
            Some(s) => s.name.clone(),
            None => self
                .secs
                .get(&n)
                .map_or_else(String::new, |s| s.name.clone()),
        }
    }

    #[must_use]
    pub fn discarded_sections(&self) -> usize {
        self.dead.len()
    }

    #[must_use]
    pub fn is_member(&self) -> bool {
        self.name.contains('(')
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum Key {
    Hull(usize, u16, u32),
    Common(String),
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum Source {
    Code { name: String, init: bool },
    Data(Key),
    Dropped(String),
}

#[derive(Debug, Clone)]
struct Edge {
    from: Source,
    to: Key,
    via: Option<String>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize)]
pub enum Class {
    #[serde(rename = "CL-GUARD")]
    ClGuard,
    #[serde(rename = "DATA-ONLY")]
    DataOnly,
    #[serde(rename = "DROPPED")]
    Dropped,
    #[serde(rename = "HDR-COPY")]
    HdrCopy,
    #[serde(rename = "INIT-ONLY")]
    InitOnly,
    #[serde(rename = "NEIGHBOUR")]
    Neighbour,
    #[serde(rename = "NONE")]
    None,
    #[serde(rename = "REACHED")]
    Reached,
}

impl Class {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::ClGuard => "CL-GUARD",
            Self::DataOnly => "DATA-ONLY",
            Self::Dropped => "DROPPED",
            Self::HdrCopy => "HDR-COPY",
            Self::InitOnly => "INIT-ONLY",
            Self::Neighbour => "NEIGHBOUR",
            Self::None => "NONE",
            Self::Reached => "REACHED",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub class: Class,
    pub object: String,
    pub section: String,
    pub size: u32,
    pub names: Vec<String>,
    pub by: Vec<String>,
    #[serde(skip)]
    pub head: Vec<u8>,
    #[serde(skip)]
    key: Key,
}

#[derive(Debug, Clone, Default)]
pub struct Census {
    pub rows: Vec<Row>,
    pub defined: usize,
    pub reached: usize,
}

fn signed_hex(a: i64) -> String {
    if a < 0 {
        format!("-{:#x}", a.unsigned_abs())
    } else {
        format!("+{a:#x}")
    }
}

fn is_init(holder: &str) -> bool {
    holder.starts_with("_$E") || holder.starts_with("$E")
}

fn edges_of(objs: &[Obj], by_name: &BTreeMap<String, Key>) -> Vec<Edge> {
    let mut edges = Vec::new();
    for (oi, o) in objs.iter().enumerate() {
        for (&n, sec) in &o.secs {
            if sec.info {
                continue;
            }
            for &(at, si) in &sec.relocs {
                let Some(sym) = o.syms.get(&si) else { continue };
                let a = sec.addend(at);
                let mut direct: Option<Key> = None;
                let mut neighbour: Option<Key> = None;
                if sym.common {
                    direct = Some(Key::Common(sym.name.clone()));
                } else if let Some(tn) = sym
                    .section
                    .filter(|tn| o.secs.get(tn).is_some_and(Sec::censused))
                {
                    let lands = o.hull(tn, i64::from(sym.value) + a);
                    if sym.named {
                        let own = o.hull(tn, i64::from(sym.value));
                        direct = own.map(|h| Key::Hull(oi, tn, h));
                        if lands != own {
                            neighbour = lands.map(|h| Key::Hull(oi, tn, h));
                        }
                    } else {
                        direct = lands.map(|h| Key::Hull(oi, tn, h));
                    }
                } else if sym.external
                    && let Some(k) = by_name.get(&sym.name)
                {
                    direct = Some(k.clone());
                    if let Key::Hull(doi, dn, dv) = k {
                        let lands = objs.get(*doi).and_then(|d| d.hull(*dn, i64::from(*dv) + a));
                        if lands != Some(*dv) {
                            neighbour = lands.map(|h| Key::Hull(*doi, *dn, h));
                        }
                    }
                }
                if direct.is_none() && neighbour.is_none() {
                    continue;
                }
                let from = if o.dead.contains(&n) {
                    Source::Dropped(format!("{}: {}", o.name, o.holder(n, at)))
                } else if sec.censused() {
                    match o.hull(n, i64::from(at)) {
                        Some(h) => Source::Data(Key::Hull(oi, n, h)),
                        None => Source::Code {
                            name: format!("{}: {}", o.name, sec.name),
                            init: false,
                        },
                    }
                } else {
                    let h = o.holder(n, at);
                    Source::Code {
                        init: is_init(&h),
                        name: format!("{}: {h}", o.name),
                    }
                };
                if let Some(t) = direct {
                    edges.push(Edge {
                        from: from.clone(),
                        to: t,
                        via: None,
                    });
                }
                if let Some(t) = neighbour {
                    edges.push(Edge {
                        from,
                        to: t,
                        via: Some(format!("{}{}", sym.name, signed_hex(a))),
                    });
                }
            }
        }
    }
    edges
}

fn is_guard(name: &str) -> bool {
    name.strip_prefix("_$S")
        .is_some_and(|d| !d.is_empty() && d.bytes().all(|b| b.is_ascii_digit()))
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn census(objs: &[Obj], all: bool) -> Census {
    let mut by_name: BTreeMap<String, Key> = BTreeMap::new();
    let mut names: BTreeMap<Key, BTreeSet<String>> = BTreeMap::new();
    let mut owner: BTreeMap<Key, usize> = BTreeMap::new();
    let mut statics: BTreeMap<String, (usize, BTreeSet<Vec<u8>>)> = BTreeMap::new();
    for (oi, o) in objs.iter().enumerate() {
        for s in o.syms.values() {
            if s.common {
                let k = Key::Common(s.name.clone());
                by_name.entry(s.name.clone()).or_insert_with(|| k.clone());
                names.entry(k.clone()).or_default().insert(s.name.clone());
                owner.entry(k).or_insert(oi);
                continue;
            }
            let Some(n) = s.section else { continue };
            if !s.named || !o.secs.get(&n).is_some_and(Sec::censused) {
                continue;
            }
            let k = Key::Hull(oi, n, s.value);
            if s.external {
                by_name.insert(s.name.clone(), k.clone());
            } else if let Some(sec) = o.secs.get(&n).filter(|sec| sec.name != ".bss") {
                let len = o.hull_len(n, s.value) as usize;
                let at = s.value as usize;
                let bytes = sec
                    .data
                    .get(at..at.saturating_add(len))
                    .unwrap_or_default()
                    .to_vec();
                let e = statics.entry(s.name.clone()).or_default();
                e.0 += 1;
                e.1.insert(bytes);
            }
            names.entry(k.clone()).or_default().insert(s.name.clone());
            owner.insert(k, oi);
        }
    }
    let edges = edges_of(objs, &by_name);
    let mut reached: BTreeSet<Key> = BTreeSet::new();
    for e in &edges {
        if e.via.is_none() && matches!(e.from, Source::Code { init: false, .. }) {
            reached.insert(e.to.clone());
        }
    }
    let mut data_edges: BTreeMap<&Key, Vec<&Key>> = BTreeMap::new();
    for e in &edges {
        if let (Source::Data(k), None) = (&e.from, &e.via)
            && *k != e.to
        {
            data_edges.entry(k).or_default().push(&e.to);
        }
    }
    let mut work: Vec<Key> = reached.iter().cloned().collect();
    while let Some(k) = work.pop() {
        for t in data_edges.get(&k).into_iter().flatten() {
            if reached.insert((*t).clone()) {
                work.push((*t).clone());
            }
        }
    }
    let mut into: BTreeMap<&Key, Vec<&Edge>> = BTreeMap::new();
    for e in &edges {
        into.entry(&e.to).or_default().push(e);
    }
    let first_name = |k: &Key| names.get(k).and_then(|s| s.iter().next().cloned());
    let mut rows: Vec<Row> = Vec::new();
    for (k, ns) in &names {
        let reached_here = reached.contains(k);
        if reached_here && !all {
            continue;
        }
        let mut neighbour = BTreeSet::new();
        let mut init = BTreeSet::new();
        let mut dropped = BTreeSet::new();
        let mut data = BTreeSet::new();
        for e in into.get(k).into_iter().flatten() {
            match (&e.from, &e.via) {
                (Source::Code { name, init: false }, Some(v)) => {
                    neighbour.insert(format!("{name} via {v}"));
                }
                (Source::Data(fk), Some(v)) if reached.contains(fk) => {
                    neighbour.insert(format!("{} via {v}", first_name(fk).unwrap_or_default()));
                }
                (Source::Code { name, init: true }, _) => {
                    init.insert(name.clone());
                }
                (Source::Dropped(n), _) => {
                    dropped.insert(n.clone());
                }
                (Source::Data(fk), None) if fk != k => {
                    data.insert(fk.clone());
                }
                _ => {}
            }
        }
        let copies = ns
            .iter()
            .filter_map(|n| statics.get(n))
            .filter(|(_, b)| b.len() == 1)
            .map(|(c, _)| *c)
            .max()
            .unwrap_or(0);
        let (class, by): (Class, Vec<String>) = if reached_here {
            (Class::Reached, Vec::new())
        } else if ns.iter().any(|n| is_guard(n)) {
            (Class::ClGuard, Vec::new())
        } else if copies > 1 {
            (Class::HdrCopy, vec![format!("{copies} objects define it")])
        } else if !neighbour.is_empty() {
            (
                Class::Neighbour,
                neighbour
                    .into_iter()
                    .chain(init.iter().map(|i| format!("{i} (init)")))
                    .chain(dropped.iter().map(|d| format!("{d} (dropped)")))
                    .collect(),
            )
        } else if !init.is_empty() {
            (Class::InitOnly, init.into_iter().chain(dropped).collect())
        } else if !dropped.is_empty() {
            (Class::Dropped, dropped.into_iter().collect())
        } else if !data.is_empty() {
            (
                Class::DataOnly,
                data.iter().filter_map(first_name).collect(),
            )
        } else {
            (Class::None, Vec::new())
        };
        let oi = owner.get(k).copied().unwrap_or(0);
        let Some(o) = objs.get(oi) else { continue };
        let (section, size, head) = match k {
            Key::Hull(_, n, v) => {
                let Some(s) = o.secs.get(n) else { continue };
                let len = o.hull_len(*n, *v);
                let at = *v as usize;
                let head = s
                    .data
                    .get(
                        at..at
                            .saturating_add((len as usize).min(HEAD))
                            .min(s.data.len()),
                    )
                    .unwrap_or_default()
                    .to_vec();
                (format!("{}+{v:#x}", s.name), len, head)
            }
            Key::Common(nm) => {
                let size = o
                    .syms
                    .values()
                    .find(|s| s.common && &s.name == nm)
                    .map_or(0, |s| s.value);
                ("COMMON".to_string(), size, Vec::new())
            }
        };
        rows.push(Row {
            class,
            object: o.name.clone(),
            section,
            size,
            names: ns.iter().cloned().collect(),
            by,
            head,
            key: k.clone(),
        });
    }
    rows.sort_by(|a, b| (a.class, &a.object, &a.key).cmp(&(b.class, &b.object, &b.key)));
    Census {
        defined: names.len(),
        reached: names.keys().filter(|k| reached.contains(k)).count(),
        rows,
    }
}

#[must_use]
pub fn map_vas(map: &Map) -> BTreeMap<String, u32> {
    let mut out = BTreeMap::new();
    for r in &map.rows {
        out.entry(r.name.clone()).or_insert(r.va);
    }
    out
}

#[must_use]
pub fn section_bases(objs: &[Obj], vas: &BTreeMap<String, u32>) -> BTreeMap<(usize, u16), u32> {
    let mut bases = BTreeMap::new();
    for (oi, o) in objs.iter().enumerate() {
        for s in o.syms.values() {
            let Some(n) = s.section else { continue };
            if !s.external || !o.secs.get(&n).is_some_and(Sec::censused) {
                continue;
            }
            if let Some(va) = vas.get(&s.name) {
                bases.entry((oi, n)).or_insert(va.wrapping_sub(s.value));
            }
        }
    }
    bases
}

#[must_use]
pub fn row_va(
    r: &Row,
    vas: &BTreeMap<String, u32>,
    bases: &BTreeMap<(usize, u16), u32>,
) -> Option<u32> {
    r.names
        .iter()
        .find_map(|n| vas.get(n).copied())
        .or(match r.key {
            Key::Hull(oi, n, v) => bases.get(&(oi, n)).map(|b| b.wrapping_add(v)),
            Key::Common(_) => None,
        })
}

#[must_use]
pub fn reference_hits(spans: &[Option<(u32, u32)>], file: &[u8]) -> Vec<Option<u64>> {
    let mut budget = SPAN_BUDGET;
    let mut wanted: Vec<Option<(u32, u32)>> = Vec::with_capacity(spans.len());
    for s in spans {
        match s {
            Some((va, size)) => {
                let n = u64::from((*size).max(1));
                if n > budget {
                    wanted.push(None);
                } else {
                    budget -= n;
                    wanted.push(Some((*va, (*size).max(1))));
                }
            }
            None => wanted.push(None),
        }
    }
    let mut union: Vec<(u64, u64)> = wanted
        .iter()
        .flatten()
        .map(|(va, n)| (u64::from(*va), u64::from(*va) + u64::from(*n)))
        .collect();
    union.sort_unstable();
    let mut merged: Vec<(u64, u64)> = Vec::new();
    for (a, b) in union {
        match merged.last_mut() {
            Some((_, e)) if a <= *e => *e = (*e).max(b),
            _ => merged.push((a, b)),
        }
    }
    let inside = |v: u64| {
        let i = merged.partition_point(|(a, _)| *a <= v);
        i.checked_sub(1)
            .and_then(|i| merged.get(i))
            .is_some_and(|(_, e)| v < *e)
    };
    let mut counts: HashMap<u32, u64> = HashMap::new();
    for w in file.windows(4) {
        if let Ok(b) = <[u8; 4]>::try_from(w) {
            let v = u32::from_le_bytes(b);
            if inside(u64::from(v)) {
                *counts.entry(v).or_insert(0) += 1;
            }
        }
    }
    let mut values: Vec<(u32, u64)> = counts.into_iter().collect();
    values.sort_unstable();
    let mut prefix: Vec<u64> = Vec::with_capacity(values.len() + 1);
    prefix.push(0);
    for (_, c) in &values {
        let last = prefix.last().copied().unwrap_or(0);
        prefix.push(last.saturating_add(*c));
    }
    wanted
        .iter()
        .map(|w| {
            let (va, n) = (*w)?;
            let lo = values.partition_point(|(v, _)| *v < va);
            let end = u64::from(va) + u64::from(n);
            let hi = values.partition_point(|(v, _)| u64::from(*v) < end);
            Some(
                prefix
                    .get(hi)
                    .copied()
                    .unwrap_or(0)
                    .saturating_sub(prefix.get(lo).copied().unwrap_or(0)),
            )
        })
        .collect()
}

#[must_use]
pub fn show(b: &[u8]) -> String {
    let hex: Vec<String> = b.iter().map(|x| format!("{x:02x}")).collect();
    let txt: String = b
        .iter()
        .map(|&c| {
            if (0x20..0x7f).contains(&c) {
                char::from(c)
            } else {
                '.'
            }
        })
        .collect();
    format!("{} |{txt}|", hex.join(" "))
}

#[derive(Debug)]
pub struct Report<'a> {
    pub label: &'a str,
    pub config: &'a str,
    pub log: &'a str,
    pub map: Option<&'a str>,
    pub objs: &'a [Obj],
    pub census: &'a Census,
    pub hits: &'a [Option<u64>],
}

impl Report<'_> {
    #[must_use]
    pub fn tally(&self) -> BTreeMap<Class, (usize, u64)> {
        let mut t: BTreeMap<Class, (usize, u64)> = BTreeMap::new();
        for r in &self.census.rows {
            let e = t.entry(r.class).or_default();
            e.0 += 1;
            e.1 += u64::from(r.size);
        }
        t
    }

    #[must_use]
    pub fn lines(&self) -> Vec<String> {
        let members = self.objs.iter().filter(|o| o.is_member()).count();
        let mut out = vec![
            format!(
                "{}: ORPHAN DATA -- non-COMDAT data no kept section reaches ({} object(s): {} command line, {members} \
                 library member(s); {}, /OPT:REF; log {})",
                self.label,
                self.objs.len(),
                self.objs.len() - members,
                self.config,
                self.log
            ),
            format!(
                "  data defined {}   reached {}   discarded sections {}",
                self.census.defined,
                self.census.reached,
                self.objs.iter().map(Obj::discarded_sections).sum::<usize>()
            ),
        ];
        for (cl, (n, b)) in self.tally() {
            out.push(format!("  {:<10} {n:>5} datum(s) {b:>7} byte(s)", cl.tag()));
        }
        out.push(format!(
            "  ref = dwords anywhere in the reference file encoding an address inside the datum (VAs from {}); 0: no \
             operand or stored pointer encodes one",
            self.map.unwrap_or("no map: ?")
        ));
        out.push(
            "  class      object                         section+off     size  ref  name(s)  <- named by  | first bytes"
                .to_string(),
        );
        for (i, r) in self.census.rows.iter().enumerate() {
            let hits = self
                .hits
                .get(i)
                .copied()
                .flatten()
                .map_or_else(|| "?".to_string(), |h| h.to_string());
            out.push(format!(
                "  {:<10} {:<30} {:<15} {:>5} {:>4}  {}  <- {}  | {}",
                r.class.tag(),
                r.object,
                r.section,
                r.size,
                hits,
                r.names.join(" = "),
                if r.by.is_empty() {
                    "-".to_string()
                } else {
                    r.by.join(", ")
                },
                show(&r.head)
            ));
        }
        out
    }
}

#[must_use]
pub fn loaded_members(log: &Log, lib_file: &str, member_file: &str) -> bool {
    log.loaded
        .contains(&object_key(&format!("{lib_file}({member_file})")))
}

#[must_use]
pub fn member_of(key: &str) -> Option<&str> {
    key.split_once('(')?.1.strip_suffix(')')
}

#[must_use]
pub fn loaded_as<'a>(log: &'a Log, file: &str) -> Option<&'a String> {
    let key = object_key(file);
    log.loaded
        .iter()
        .find(|l| member_of(l) == Some(key.as_str()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sym(name: &str, section: Option<u16>, value: u32, external: bool) -> Sym {
        Sym {
            name: name.to_string(),
            section,
            value,
            external,
            named: true,
            common: false,
        }
    }

    fn data(size: u32, relocs: &[(u32, u32, i32)]) -> Sec {
        let mut d = vec![0u8; size as usize];
        for &(at, _, a) in relocs {
            if let Some(f) = d.get_mut(at as usize..at as usize + 4) {
                f.copy_from_slice(&a.to_le_bytes());
            }
        }
        Sec {
            name: ".data".to_string(),
            size,
            data: d,
            relocs: relocs.iter().map(|&(at, s, _)| (at, s)).collect(),
            ..Sec::default()
        }
    }

    fn code(relocs: &[(u32, u32, i32)]) -> Sec {
        let mut s = data(8 * u32::try_from(relocs.len().max(1)).unwrap_or(1), relocs);
        s.name = ".text".to_string();
        s.code = true;
        s.comdat = true;
        s
    }

    fn row<'a>(c: &'a Census, name: &str) -> Option<&'a Row> {
        c.rows.iter().find(|r| r.names.iter().any(|n| n == name))
    }

    fn discarded(pairs: &[(&str, &str)]) -> BTreeSet<(String, String)> {
        pairs
            .iter()
            .map(|(o, s)| ((*o).to_string(), (*s).to_string()))
            .collect()
    }

    fn sample() -> Obj {
        let mut secs = BTreeMap::new();
        secs.insert(1, data(28, &[]));
        secs.insert(2, code(&[(0, 10, 0), (4, 10, 8), (8, 12, -4)]));
        secs.insert(3, code(&[(0, 13, 0)]));
        let mut syms = BTreeMap::new();
        syms.insert(10, sym("_A", Some(1), 0, true));
        syms.insert(11, sym("_B", Some(1), 8, true));
        syms.insert(12, sym("_C", Some(1), 16, true));
        syms.insert(13, sym("_D", Some(1), 20, true));
        syms.insert(14, sym("_E", Some(1), 24, true));
        syms.insert(20, sym("?f@@YAXXZ", Some(2), 0, true));
        syms.insert(21, sym("?g@@YAXXZ", Some(3), 0, true));
        let groups = vec![
            ("?f@@YAXXZ".to_string(), vec![2]),
            ("?g@@YAXXZ".to_string(), vec![3]),
        ];
        Obj::new(
            "X.obj",
            secs,
            syms,
            &groups,
            &discarded(&[("x.obj", "?g@@YAXXZ")]),
        )
    }

    #[test]
    fn an_end_pointer_or_a_biased_index_is_a_neighbour_not_a_reach() {
        let c = census(&[sample()], true);
        assert_eq!(row(&c, "_A").map(|r| r.class), Some(Class::Reached));
        let b = row(&c, "_B");
        assert_eq!(b.map(|r| r.class), Some(Class::Neighbour));
        assert!(b.is_some_and(|r| {
            r.by.iter()
                .any(|s| s.contains("?f@@YAXXZ") && s.contains("_A+0x8"))
        }));
        assert_eq!(row(&c, "_C").map(|r| r.class), Some(Class::Reached));
        assert_eq!(row(&c, "_D").map(|r| r.class), Some(Class::Dropped));
        assert_eq!(row(&c, "_E").map(|r| r.class), Some(Class::None));
        assert_eq!((c.defined, c.reached), (5, 2));
    }

    #[test]
    fn an_addend_inside_the_hull_reaches_and_a_section_symbol_reaches_its_offset() {
        let mut secs = BTreeMap::new();
        secs.insert(1, data(16, &[]));
        secs.insert(2, code(&[(0, 10, 4), (4, 1, 8)]));
        let mut syms = BTreeMap::new();
        syms.insert(
            1,
            Sym {
                name: ".data".to_string(),
                section: Some(1),
                ..Sym::default()
            },
        );
        syms.insert(10, sym("_A", Some(1), 0, true));
        syms.insert(11, sym("_B", Some(1), 8, false));
        syms.insert(20, sym("?f@@YAXXZ", Some(2), 0, true));
        let o = Obj::new(
            "X.obj",
            secs,
            syms,
            &[("?f@@YAXXZ".to_string(), vec![2])],
            &BTreeSet::new(),
        );
        let c = census(&[o], false);
        assert!(c.rows.is_empty(), "{:?}", c.rows);
        assert_eq!(c.reached, 2);
    }

    #[test]
    fn a_reached_table_keeps_what_it_points_at_and_an_unreached_one_does_not() {
        let mut secs = BTreeMap::new();
        secs.insert(1, data(16, &[(0, 12, 0), (4, 13, 0)]));
        secs.insert(2, code(&[(0, 10, 0)]));
        let mut syms = BTreeMap::new();
        syms.insert(10, sym("_T", Some(1), 0, true));
        syms.insert(11, sym("_U", Some(1), 4, true));
        syms.insert(12, sym("_P", Some(1), 8, true));
        syms.insert(13, sym("_Q", Some(1), 12, true));
        syms.insert(20, sym("?f@@YAXXZ", Some(2), 0, true));
        let o = Obj::new(
            "X.obj",
            secs,
            syms,
            &[("?f@@YAXXZ".to_string(), vec![2])],
            &BTreeSet::new(),
        );
        let c = census(&[o], true);
        assert_eq!(row(&c, "_P").map(|r| r.class), Some(Class::Reached));
        assert_eq!(row(&c, "_U").map(|r| r.class), Some(Class::None));
        assert_eq!(row(&c, "_Q").map(|r| r.class), Some(Class::DataOnly));
    }

    #[test]
    fn compiler_classes_print_apart() {
        let mk = |name: &str| {
            let mut secs = BTreeMap::new();
            let mut d = data(8, &[]);
            d.data = vec![0x35, 0xfa, 0x8e, 0x3c, 0, 0, 0, 0];
            secs.insert(1, d);
            secs.insert(2, code(&[(0, 11, 0), (8, 12, 0)]));
            secs.insert(3, data(4, &[]));
            let mut syms = BTreeMap::new();
            syms.insert(10, sym("_g_fScale", Some(1), 0, false));
            syms.insert(11, sym(&format!("?G{name}@@3HA"), Some(1), 4, true));
            syms.insert(12, sym("_$S7", Some(3), 0, false));
            syms.insert(20, sym("_$E3", Some(2), 0, false));
            Obj::new(name, secs, syms, &[], &BTreeSet::new())
        };
        let c = census(&[mk("A.obj"), mk("B.obj")], false);
        assert!(c.rows.iter().all(|r| r.class != Class::None));
        assert_eq!(
            c.rows.iter().filter(|r| r.class == Class::ClGuard).count(),
            2
        );
        assert_eq!(
            c.rows.iter().filter(|r| r.class == Class::HdrCopy).count(),
            2
        );
        assert_eq!(
            row(&c, "?GA.obj@@3HA").map(|r| r.class),
            Some(Class::InitOnly)
        );
    }

    #[test]
    fn a_discarded_comdat_kills_its_associates_and_only_its_own_object() {
        let groups = vec![
            ("?f@@YAXXZ".to_string(), vec![3, 4, 5]),
            ("?g@@YAXXZ".to_string(), vec![6]),
        ];
        let gone = discarded(&[("lib.lib(x.obj)", "?f@@YAXXZ"), ("other.obj", "?g@@YAXXZ")]);
        let o = Obj::new(
            "lib.lib(X.obj)",
            BTreeMap::new(),
            BTreeMap::new(),
            &groups,
            &gone,
        );
        assert_eq!(o.dead, [3, 4, 5].into());
        assert_eq!(o.leader.get(&6).map(String::as_str), Some("?g@@YAXXZ"));
    }

    #[test]
    fn the_link_switches_decide_opt_ref() {
        assert!(opt_ref(
            "/nologo /subsystem:windows /machine:I386 /out:\"Release/app.exe\""
        ));
        assert!(!opt_ref(
            "/nologo /dll /machine:I386 /incremental:yes /def:\"app.def\""
        ));
        assert!(!opt_ref("/nologo /dll /debug /machine:I386"));
        assert!(opt_ref("/nologo /debug /opt:ref"));
        assert!(!opt_ref("/nologo /OPT:NOREF"));
        assert!(opt_ref("/nologo /opt:ref,noicf /debug"));
        let dsp = "!IF  \"$(CFG)\" == \"g - Win32 Release\"\n# ADD LINK32 /nologo\n!ELSEIF  \"$(CFG)\" == \"g - \
                   Win32 Debug\"\n# ADD LINK32 /nologo /debug\n!ENDIF\n";
        assert_eq!(
            link_line(dsp, "g - Win32 Debug").as_deref(),
            Some("/nologo /debug")
        );
        assert_eq!(link_line(dsp, "g - Win32 Other"), None);
        let log = "--------------------Configuration: lib - Win32 Release--------------------\n\
                   --------------------Configuration: g - Win32 Release--------------------\n";
        assert_eq!(
            last_configuration(log).as_deref(),
            Some("g - Win32 Release")
        );
    }

    #[test]
    fn the_log_reads_loaded_members_and_discarded_pairs_without_a_path() {
        let log = "        Loaded own.lib(Arena.obj)\r\n    Discarded ___no_lead_zeros\r\n    Discarded \
                   ?f@@YAXXZ from Plot.obj\r\n    Discarded _g from C:\\b\\own.lib(Net.obj)\r\n    Discarded \
                   \"void __cdecl CopyLog(void)\" (?CopyLog@@YAXXZ) from own.lib(Log.obj)\r\n";
        let l = parse_log(log);
        assert!(l.loaded.contains("own.lib(arena.obj)"));
        assert!(loaded_members(&l, "OWN.LIB", "Arena.obj"));
        assert!(
            l.discarded
                .contains(&("plot.obj".to_string(), "?f@@YAXXZ".to_string()))
        );
        assert!(
            l.discarded
                .contains(&("own.lib(net.obj)".to_string(), "_g".to_string()))
        );
        assert!(l.discarded.contains(&(
            "own.lib(log.obj)".to_string(),
            "?CopyLog@@YAXXZ".to_string()
        )));
        assert_eq!(l.discarded.len(), 3);
    }

    #[test]
    fn a_field_address_inside_the_datum_counts_and_a_huge_datum_runs_out_of_budget() {
        let file = [0xa1, 0x08, 0x10, 0x40, 0x00, 0x00, 0x10, 0x40, 0x00];
        let got = reference_hits(
            &[
                Some((0x0040_1000, 4)),
                Some((0x0040_1000, 12)),
                Some((0x0040_2000, 16)),
                None,
                Some((0, u32::MAX)),
            ],
            &file,
        );
        assert_eq!(got, vec![Some(1), Some(2), Some(0), None, None]);
    }

    #[test]
    fn holders_and_hulls_are_found_by_search() {
        let o = sample();
        assert_eq!(o.hull(1, 9), Some(8));
        assert_eq!(o.hull(1, 28), None);
        assert_eq!(o.hull(1, -1), None);
        assert_eq!(o.hull_len(1, 24), 4);
        assert_eq!(o.holder(2, 4), "?f@@YAXXZ");
        assert_eq!(o.holder(1, 9), "_B");
    }
}
