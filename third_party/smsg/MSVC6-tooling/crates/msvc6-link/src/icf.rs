use anyhow::{Result, anyhow, bail};
use std::cmp::Ordering;
use std::collections::{HashMap, HashSet};

use crate::comdat::{ObjComdat, ObjInfo, Target, is_candidate, is_short_import, object_comdats};
use crate::objkey::Keys;
use crate::qsort::{self, Crt};
use crate::verbose::{Group, Kind, Load};

pub const PAIR_BUDGET: u64 = 1 << 26;

pub const BYTE_BUDGET: u64 = 1 << 34;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Cand {
    pub name: String,
    pub obj: usize,
    pub external: bool,
    pub dead: bool,
    pub dup: bool,
    pub size: u32,
    pub nreloc: u16,
    pub chr: u32,
    pub sel: u8,
    pub bytes: Vec<u8>,
    pub relocs: Vec<(u32, u16, Target)>,
    pub cksum: u32,
    pub label: String,
}

#[derive(Clone, Debug, Default)]
pub struct Array {
    pub objects: Vec<String>,
    pub cands: Vec<Cand>,
    pub missing: Vec<String>,
    pub aliases: HashMap<String, String>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Model {
    pub externals_only: bool,
    pub dead_out: bool,
    pub dead_null: bool,
    pub dup_in: bool,
    pub sym_order: bool,
    pub thunks: bool,
    pub stable: bool,
    pub reverse: bool,
    pub crt: Option<Crt>,
}

impl Model {
    pub fn parse(s: &str) -> Result<Self> {
        let mut m = Self::default();
        for f in s.split(',').map(str::trim).filter(|f| !f.is_empty()) {
            match f {
                "externals-only" => m.externals_only = true,
                "dead-out" => m.dead_out = true,
                "dead-null" => m.dead_null = true,
                "dup-in" => m.dup_in = true,
                "sym-order" => m.sym_order = true,
                "thunks" => m.thunks = true,
                "stable" => m.stable = true,
                "reverse" => m.reverse = true,
                "wine" => m.crt = Some(Crt::Wine),
                _ => bail!("--model: unknown flag '{f}'"),
            }
        }
        Ok(m)
    }

    #[must_use]
    pub fn crt(self) -> Crt {
        self.crt.unwrap_or(Crt::Vc6)
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Entry {
    Object(String),
    Member {
        lib: String,
        member: String,
    },
    Thunk {
        code: String,
        lib: String,
        member: String,
    },
}

impl Entry {
    #[must_use]
    pub fn spelled(&self) -> String {
        match self {
            Self::Object(o) => o.clone(),
            Self::Member { lib, member } => format!("{lib}({member})"),
            Self::Thunk { code, lib, member } => format!("thunk:{code}|{lib}({member})"),
        }
    }

    #[must_use]
    pub fn member_spelled(&self) -> String {
        match self {
            Self::Object(o) => o.clone(),
            Self::Member { lib, member } | Self::Thunk { lib, member, .. } => {
                format!("{lib}({member})")
            }
        }
    }
}

#[must_use]
pub fn loaded_entries(loads: &[Load]) -> Vec<Entry> {
    let mut out = Vec::new();
    for l in loads {
        if !l.is_import() {
            out.push(Entry::Member {
                lib: l.lib.clone(),
                member: l.member.clone(),
            });
        } else if l.kind() == Kind::Import {
            let code = l.symbol.strip_prefix("__imp_").unwrap_or(&l.symbol);
            out.push(Entry::Thunk {
                code: code.to_string(),
                lib: l.lib.clone(),
                member: l.member.clone(),
            });
        }
    }
    out
}

pub trait Inputs {
    fn object(&mut self, name: &str) -> Option<Vec<u8>>;
    fn member(&mut self, lib: &str, member: &str) -> Option<Vec<u8>>;
    fn member_defining(&mut self, lib: &str, names: &[String]) -> Option<Vec<u8>>;
}

#[derive(Clone, Debug)]
pub enum Unit {
    Thunk(String, String),
    Obj(String, ObjInfo),
    Missing(String),
}

#[derive(Clone, Debug, Default)]
pub struct Parsed {
    pub units: Vec<Unit>,
    pub refused: Vec<(String, String)>,
}

pub fn parse(order: &[Entry], src: &mut dyn Inputs) -> Parsed {
    let mut out = Parsed::default();
    for e in order {
        let at = out.units.len();
        let read = |out: &mut Parsed, name: String, bytes: &[u8]| -> ObjInfo {
            match object_comdats(bytes, at) {
                Ok(i) => i,
                Err(err) => {
                    if !is_short_import(bytes) {
                        out.refused.push((name, format!("{err:#}")));
                    }
                    ObjInfo::default()
                }
            }
        };
        match e {
            Entry::Thunk { code, lib, .. } => {
                let long = src
                    .member_defining(lib, &[format!("__imp_{code}"), code.clone()])
                    .filter(|b| !is_short_import(b));
                match long {
                    Some(bytes) => {
                        let spelled = e.member_spelled();
                        let info = read(&mut out, spelled.clone(), &bytes);
                        out.units.push(Unit::Obj(spelled, info));
                    }
                    None => out
                        .units
                        .push(Unit::Thunk(code.clone(), e.member_spelled())),
                }
            }
            Entry::Member { lib, member } => match src.member(lib, member) {
                Some(bytes) => {
                    let s = e.spelled();
                    let info = read(&mut out, s.clone(), &bytes);
                    out.units.push(Unit::Obj(s, info));
                }
                None => out.units.push(Unit::Missing(e.spelled())),
            },
            Entry::Object(o) => match src.object(o) {
                Some(bytes) => {
                    let info = read(&mut out, o.clone(), &bytes);
                    out.units.push(Unit::Obj(o.clone(), info));
                }
                None => out.units.push(Unit::Missing(o.clone())),
            },
        }
    }
    out
}

#[must_use]
pub fn glob(pat: &str, s: &str) -> bool {
    let parts: Vec<&str> = pat.split('*').collect();
    if parts.len() == 1 {
        return pat == s;
    }
    let (Some(first), Some(last)) = (parts.first(), parts.last()) else {
        return false;
    };
    if !s.starts_with(first) || s.len() < first.len() + last.len() || !s.ends_with(last) {
        return false;
    }
    let mut rest = s.get(first.len()..s.len() - last.len()).unwrap_or("");
    for p in parts.iter().skip(1).take(parts.len().saturating_sub(2)) {
        match rest.find(p) {
            Some(i) => rest = rest.get(i + p.len()..).unwrap_or(""),
            None => return false,
        }
    }
    true
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Definer {
    Drop {
        obj: String,
        pat: String,
    },
    Define {
        obj: String,
        pat: String,
        before: Option<String>,
    },
}

impl Definer {
    pub fn parse(s: &str, define: bool) -> Result<Self> {
        let (obj, rest) = s
            .split_once(':')
            .ok_or_else(|| anyhow!("'{s}': want OBJ:GLOB"))?;
        let obj = obj.to_string();
        if !define {
            return Ok(Self::Drop {
                obj,
                pat: rest.to_string(),
            });
        }
        let (pat, before) = match rest.split_once(',') {
            Some((p, b)) => (p.to_string(), (!b.is_empty()).then(|| b.to_string())),
            None => (rest.to_string(), None),
        };
        Ok(Self::Define { obj, pat, before })
    }

    fn obj(&self) -> &str {
        match self {
            Self::Drop { obj, .. } | Self::Define { obj, .. } => obj,
        }
    }
}

fn apply_definers(p: &Parsed, defs: &[Definer]) -> Result<Vec<Unit>> {
    let mut units = p.units.clone();
    let unit_of = |units: &[Unit], obj: &str| -> Option<usize> {
        units.iter().position(|u| match u {
            Unit::Obj(n, _) => n.eq_ignore_ascii_case(obj),
            _ => false,
        })
    };
    for d in defs {
        if let Definer::Drop { obj, pat } = d {
            let lo = obj.to_ascii_lowercase();
            let mut hit = false;
            for u in &mut units {
                let Unit::Obj(n, info) = u else { continue };
                if !glob(&lo, &n.to_ascii_lowercase()) {
                    continue;
                }
                hit = true;
                let gone: HashSet<usize> = info
                    .comdats
                    .iter()
                    .filter(|c| c.parent.is_none() && c.external && glob(pat, &c.name))
                    .map(|c| c.sec)
                    .collect();
                info.comdats
                    .retain(|c| !gone.contains(&c.parent.unwrap_or(c.sec)));
            }
            if !hit {
                bail!("{obj}: not in the link");
            }
            continue;
        }
        let ui = unit_of(&units, d.obj()).ok_or_else(|| anyhow!("{}: not in the link", d.obj()))?;
        if let Definer::Define { pat, before, .. } = d {
            let have: HashSet<String> = match units.get(ui) {
                Some(Unit::Obj(_, info)) => info
                    .comdats
                    .iter()
                    .filter(|c| c.external)
                    .map(|c| c.name.clone())
                    .collect(),
                _ => HashSet::new(),
            };
            let mut copies: Vec<ObjComdat> = Vec::new();
            let mut seen: HashSet<String> = HashSet::new();
            for u in &p.units {
                let Unit::Obj(_, info) = u else { continue };
                for c in info.comdats.iter().filter(|c| {
                    c.parent.is_none()
                        && c.external
                        && glob(pat, &c.name)
                        && !have.contains(&c.name)
                }) {
                    if seen.insert(c.name.clone()) {
                        copies.push(c.clone());
                        copies.extend(
                            info.comdats
                                .iter()
                                .filter(|x| x.parent == Some(c.sec))
                                .cloned(),
                        );
                    }
                }
            }
            if let Some(Unit::Obj(_, info)) = units.get_mut(ui) {
                let mut next = info.comdats.iter().map(|c| c.sec).max().unwrap_or(0) + 1;
                let mut renum: HashMap<usize, usize> = HashMap::new();
                for c in &mut copies {
                    if c.parent.is_none() {
                        renum.insert(c.sec, next);
                        c.sec = next;
                        next += 1;
                    }
                }
                for c in &mut copies {
                    if let Some(par) = c.parent {
                        c.parent = renum.get(&par).copied();
                        c.sec = next;
                        next += 1;
                    }
                }
                let at = before
                    .as_ref()
                    .and_then(|b| info.comdats.iter().position(|c| &c.name == b))
                    .unwrap_or(info.comdats.len());
                info.comdats.splice(at..at, copies);
            }
        }
    }
    Ok(units)
}

pub fn assemble(
    p: &Parsed,
    dead: &HashSet<(String, String)>,
    model: Model,
    defs: &[Definer],
) -> Result<Array> {
    let units = apply_definers(p, defs)?;
    let dead_ext: HashSet<&str> = dead.iter().map(|(n, _)| n.as_str()).collect();
    let mut arr = Array::default();
    let mut defined: HashSet<String> = HashSet::new();
    let mut dead_secs: HashSet<(usize, usize)> = HashSet::new();
    for u in units {
        let (obj, mut info) = match u {
            Unit::Missing(m) => {
                arr.missing.push(m);
                continue;
            }
            Unit::Thunk(code, member) => {
                if model.thunks {
                    let oi = arr.objects.len();
                    arr.cands.push(Cand {
                        dead: dead.contains(&(code.clone(), member.to_ascii_lowercase())),
                        dup: false,
                        name: code.clone(),
                        obj: oi,
                        external: true,
                        size: 6,
                        nreloc: 1,
                        chr: 0x6000_0020,
                        sel: 1,
                        bytes: vec![0xff, 0x25, 0, 0, 0, 0],
                        relocs: vec![(2, 6, Target::Ext(format!("__imp_{code}")))],
                        cksum: 0,
                        label: code,
                    });
                    arr.objects.push(member);
                }
                continue;
            }
            Unit::Obj(o, i) => (o, i),
        };
        let oi = arr.objects.len();
        arr.objects.push(obj.clone());
        for (a, t) in info.aliases.drain(..) {
            arr.aliases.entry(a).or_insert(t);
        }
        if model.sym_order {
            info.comdats.sort_by_key(|c| c.sym);
        }
        let objl = obj.to_ascii_lowercase();
        let mut accepted: HashMap<usize, bool> = HashMap::new();
        for c in info.comdats.iter().filter(|c| c.parent.is_none()) {
            let ok = !(c.external && defined.contains(&c.name));
            if c.external && ok {
                defined.insert(c.name.clone());
            }
            accepted.insert(c.sec, ok);
            let is_dead = if c.external {
                dead_ext.contains(c.name.as_str())
            } else {
                dead.contains(&(c.name.clone(), objl.clone()))
            };
            if is_dead {
                dead_secs.insert((oi, c.sec));
            }
        }
        for c in info.comdats {
            let ok = match c.parent {
                Some(p) => accepted.get(&p).copied().unwrap_or(true),
                None => accepted.get(&c.sec).copied().unwrap_or(false),
            };
            if !(ok || model.dup_in && c.parent.is_none()) || !is_candidate(&c) {
                continue;
            }
            let is_dead = dead_secs.contains(&(oi, c.parent.unwrap_or(c.sec)));
            if is_dead && model.dead_out {
                continue;
            }
            let label = if c.external {
                c.name.clone()
            } else {
                c.sec_name.clone()
            };
            arr.cands.push(Cand {
                dead: is_dead,
                dup: !ok,
                name: c.name,
                obj: oi,
                external: c.external,
                size: c.size,
                nreloc: c.nreloc,
                chr: c.chr,
                sel: c.sel,
                bytes: c.bytes,
                label,
                relocs: c.relocs,
                cksum: c.cksum,
            });
        }
        defined.extend(info.plain_defs);
    }
    Ok(arr)
}

#[must_use]
pub fn key(c: &Cand) -> (u32, u16) {
    (c.size, c.nreloc)
}

pub fn sorted(arr: &Array, seq: &[usize], model: Model) -> Result<Vec<usize>> {
    let mut idx: Vec<usize> = seq
        .iter()
        .copied()
        .filter(|&i| !model.externals_only || arr.cands.get(i).is_some_and(|c| c.external))
        .collect();
    let null = |c: &Cand| model.dead_null && c.dead;
    let comp = |a: &usize, b: &usize| -> Ordering {
        match (arr.cands.get(*a), arr.cands.get(*b)) {
            (Some(x), Some(y)) => match (null(x), null(y)) {
                (true, true) => Ordering::Equal,
                (true, false) => Ordering::Greater,
                (false, true) => Ordering::Less,
                (false, false) => key(x).cmp(&key(y)),
            },
            _ => Ordering::Equal,
        }
    };
    if model.reverse {
        idx.reverse();
    }
    if model.stable {
        idx.sort_by(comp);
    } else {
        qsort::sort(model.crt(), &mut idx, &comp)?;
    }
    Ok(idx)
}

fn find_root(root: &[usize], mut i: usize) -> usize {
    let mut hops = 0usize;
    while let Some(&r) = root.get(i) {
        if r == i || hops > root.len() {
            break;
        }
        i = r;
        hops += 1;
    }
    i
}

pub fn fold(arr: &Array, order: &[usize]) -> Result<Vec<usize>> {
    let n = arr.cands.len();
    let mut root: Vec<usize> = (0..n).collect();
    let ext_index: HashMap<&str, usize> = arr
        .cands
        .iter()
        .enumerate()
        .filter(|(_, c)| c.external)
        .map(|(i, c)| (c.name.as_str(), i))
        .collect();
    let (mut pairs, mut bytes) = (0u64, 0u64);
    loop {
        let mut changed = false;
        let mut i = 0usize;
        while i < order.len() {
            let Some(k) = order.get(i).and_then(|&c| arr.cands.get(c)).map(key) else {
                break;
            };
            let mut j = i + 1;
            while order.get(j).and_then(|&c| arr.cands.get(c)).map(key) == Some(k) {
                j += 1;
            }
            for a in i..j {
                let Some(&ca) = order.get(a) else { continue };
                let Some(x) = arr.cands.get(ca) else { continue };
                if x.dead || x.dup || root.get(ca) != Some(&ca) {
                    continue;
                }
                for b in a + 1..j {
                    let Some(&cb) = order.get(b) else { continue };
                    let Some(y) = arr.cands.get(cb) else { continue };
                    if y.dead || y.dup || root.get(cb) != Some(&cb) {
                        continue;
                    }
                    pairs += 1;
                    bytes += u64::try_from(x.bytes.len().min(y.bytes.len())).unwrap_or(u64::MAX);
                    if pairs > PAIR_BUDGET || bytes > BYTE_BUDGET {
                        bail!(
                            "the fold walk compared {pairs} pairs and {bytes} bytes, past its budget: an array built to be quadratic"
                        );
                    }
                    let same_target = |s: &Target, t: &Target| -> bool {
                        if s == t {
                            return true;
                        }
                        match (s, t) {
                            (Target::Ext(p), Target::Ext(q)) => {
                                match (ext_index.get(p.as_str()), ext_index.get(q.as_str())) {
                                    (Some(&pi), Some(&qi)) => {
                                        find_root(&root, pi) == find_root(&root, qi)
                                    }
                                    _ => false,
                                }
                            }
                            _ => false,
                        }
                    };
                    let equal = x.chr == y.chr
                        && x.sel == y.sel
                        && x.cksum == y.cksum
                        && x.bytes == y.bytes
                        && x.relocs.len() == y.relocs.len()
                        && x.relocs
                            .iter()
                            .zip(&y.relocs)
                            .all(|(r, s)| r.0 == s.0 && r.1 == s.1 && same_target(&r.2, &s.2));
                    if equal {
                        if let Some(slot) = root.get_mut(cb) {
                            *slot = ca;
                        }
                        changed = true;
                    }
                }
            }
            i = j;
        }
        if !changed {
            break;
        }
    }
    Ok((0..n).map(|i| find_root(&root, i)).collect())
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Move {
    pub name: String,
    pub before: Option<String>,
}

impl Move {
    pub fn parse(s: &str) -> Result<Self> {
        let (name, before) = s.split_once(',').unwrap_or((s, ""));
        if name.is_empty() {
            bail!("--move `{s}`: want NAME,BEFORE or NAME,");
        }
        Ok(Self {
            name: name.to_string(),
            before: (!before.is_empty()).then(|| before.to_string()),
        })
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct LoadAt {
    pub obj: String,
    pub before: Option<String>,
}

impl LoadAt {
    pub fn parse(s: &str) -> Result<Self> {
        let (obj, before) = s
            .split_once(',')
            .ok_or_else(|| anyhow!("--load `{s}`: want OBJ,BEFORE or OBJ,"))?;
        if obj.is_empty() {
            bail!("--load `{s}`: no OBJ");
        }
        Ok(Self {
            obj: obj.to_string(),
            before: (!before.is_empty()).then(|| before.to_string()),
        })
    }
}

pub fn apply_loads(order: &[Entry], loads: &[LoadAt], keys: &Keys) -> Result<Vec<Entry>> {
    let mut out = order.to_vec();
    let find = |o: &[Entry], n: &str| {
        o.iter().position(|x| {
            let s = x.spelled();
            s.eq_ignore_ascii_case(n) || keys.object_key(&s).eq_ignore_ascii_case(n)
        })
    };
    for l in loads {
        let from =
            find(&out, &l.obj).ok_or_else(|| anyhow!("--load {}: not in the link", l.obj))?;
        let x = out.remove(from);
        let at = match &l.before {
            Some(b) => {
                find(&out, b).ok_or_else(|| anyhow!("--load {}: {b} not in the link", l.obj))?
            }
            None => out.len(),
        };
        out.insert(at, x);
    }
    Ok(out)
}

#[must_use]
pub fn apply_move(arr: &Array, seq: &[usize], m: &Move) -> Option<Vec<usize>> {
    let name_at = |i: &usize| arr.cands.get(*i).map(|c| c.name.as_str());
    let from = seq
        .iter()
        .position(|i| name_at(i) == Some(m.name.as_str()))?;
    let mut out = seq.to_vec();
    let c = out.remove(from);
    let obj = arr.cands.get(c)?.obj;
    let at = match &m.before {
        Some(b) => out.iter().position(|i| name_at(i) == Some(b.as_str()))?,
        None => out
            .iter()
            .rposition(|i| arr.cands.get(*i).is_some_and(|x| x.obj == obj))
            .map_or(out.len(), |p| p + 1),
    };
    out.insert(at, c);
    Some(out)
}

pub type ByLabel = HashMap<(String, String), Vec<usize>>;

#[must_use]
pub fn by_label(arr: &Array) -> ByLabel {
    let mut out: ByLabel = HashMap::new();
    for (i, c) in arr.cands.iter().enumerate() {
        if let (Some(o), false) = (arr.objects.get(c.obj), c.dup) {
            out.entry((c.label.clone(), o.to_ascii_lowercase()))
                .or_default()
                .push(i);
        }
    }
    out
}

#[must_use]
pub fn resolve_group(arr: &Array, by_label: &ByLabel, g: &Group) -> Option<Vec<usize>> {
    let alias_hit = |name: &str| -> Option<Vec<usize>> {
        let d = arr.aliases.get(name)?;
        let c = arr
            .cands
            .iter()
            .position(|c| c.external && !c.dup && &c.name == d)?;
        Some(vec![c])
    };
    let ext_hit = |name: &str| -> Option<Vec<usize>> {
        let c = arr
            .cands
            .iter()
            .position(|c| c.external && !c.dup && c.name == name)?;
        Some(vec![c])
    };
    let hits: Vec<Vec<usize>> = g
        .members()
        .map(|s| {
            by_label
                .get(&(s.name.clone(), s.obj.to_ascii_lowercase()))
                .cloned()
                .or_else(|| alias_hit(&s.name))
                .or_else(|| ext_hit(&s.name))
        })
        .collect::<Option<Vec<_>>>()?;
    let sig: Option<&Vec<u8>> = hits
        .iter()
        .find(|h| h.len() == 1)
        .and_then(|h| h.first())
        .and_then(|&c| arr.cands.get(c))
        .map(|c| &c.bytes);
    let mut out = Vec::new();
    let mut used: HashSet<usize> = HashSet::new();
    for h in &hits {
        let pick = if h.len() == 1 {
            h.first().copied()
        } else {
            let bytes_of = |c: &usize| arr.cands.get(*c).map(|x| &x.bytes);
            match sig {
                Some(s) => h
                    .iter()
                    .find(|c| bytes_of(c) == Some(s) && !used.contains(c)),
                None => h.iter().find(|c| {
                    !used.contains(c)
                        && hits
                            .iter()
                            .all(|o| o.iter().any(|d| bytes_of(d) == bytes_of(c)))
                }),
            }
            .copied()
        };
        let p = pick?;
        used.insert(p);
        out.push(p);
    }
    Some(out)
}

#[must_use]
pub fn log_pairs(
    arr: &Array,
    resolved: &[Option<Vec<usize>>],
    pos: &dyn Fn(usize) -> usize,
) -> (usize, usize) {
    let (mut agree, mut pairs) = (0usize, 0usize);
    let mut last: Option<((u32, u16), usize)> = None;
    for r in resolved.iter().flatten() {
        for w in r.windows(2) {
            if let [a, b] = w {
                pairs += 1;
                agree += usize::from(pos(*a) < pos(*b));
            }
        }
        let Some((k, s)) = r
            .get(1)
            .and_then(|&s| arr.cands.get(s).map(|c| (key(c), s)))
        else {
            continue;
        };
        if let Some((lk, ls)) = last
            && lk == k
        {
            pairs += 1;
            agree += usize::from(pos(ls) < pos(s));
        }
        last = Some((k, s));
    }
    (agree, pairs)
}

pub fn first_of(
    arr: &Array,
    seq: &[usize],
    model: Model,
    members: &[usize],
) -> Result<Option<usize>> {
    let set: HashSet<usize> = members.iter().copied().collect();
    Ok(sorted(arr, seq, model)?
        .into_iter()
        .find(|c| set.contains(c)))
}

#[must_use]
pub fn show(arr: &Array, c: usize) -> String {
    arr.cands.get(c).map_or_else(
        || "?".into(),
        |x| {
            let o = arr.objects.get(x.obj).map_or("?", String::as_str);
            if x.external {
                format!("{} {o}", x.name)
            } else {
                format!("{} ({}) {o}", x.label, x.name)
            }
        },
    )
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Dump {
    Key(u32, u16),
    Obj(String),
}

impl Dump {
    pub fn parse(s: &str) -> Result<Self> {
        if s.to_ascii_lowercase().contains(".obj") {
            return Ok(Self::Obj(s.to_string()));
        }
        let (a, r) = s.split_once(':').unwrap_or((s, "0"));
        Ok(Self::Key(a.parse::<u32>()?, r.parse::<u16>()?))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    pub fn cand(name: &str, obj: usize, size: u32, bytes: &[u8]) -> Cand {
        Cand {
            name: name.into(),
            obj,
            external: true,
            dead: false,
            dup: false,
            size,
            nreloc: 0,
            chr: 0x6050_1020,
            sel: 2,
            bytes: bytes.to_vec(),
            relocs: Vec::new(),
            cksum: 0,
            label: name.into(),
        }
    }

    #[test]
    fn the_first_in_sorted_order_survives_and_dead_ones_never_fold() {
        let mut arr = Array {
            objects: vec!["A.obj".into(), "B.obj".into()],
            cands: vec![
                cand("a", 0, 16, &[0xc3]),
                cand("b", 1, 16, &[0xc3]),
                cand("c", 1, 16, &[0xc2, 4, 0]),
                cand("d", 0, 32, &[0xc3]),
            ],
            ..Array::default()
        };
        let s = sorted(&arr, &[0, 1, 2, 3], Model::default()).unwrap();
        let r = fold(&arr, &s).unwrap();
        let ra = r.first().copied();
        assert_eq!(r.get(1).copied(), ra);
        assert_ne!(r.get(2).copied(), ra);
        assert_eq!(r.get(3).copied(), Some(3));
        if let Some(c) = arr.cands.get_mut(0) {
            c.dead = true;
        }
        let r = fold(&arr, &s).unwrap();
        assert_eq!(r.get(1).copied(), Some(1));
    }

    #[test]
    fn glob_matches_runs_and_anchors_both_ends() {
        assert!(glob("*@WidgetBase@@*", "?OnMouseUp@WidgetBase@@UAEXHH@Z"));
        assert!(glob("a*b*c", "axxbyc"));
        assert!(glob("ab", "ab"));
        assert!(glob("a*", "a"));
        assert!(!glob("a*b", "ba"));
        assert!(!glob("*@Screen@@*", "?OnMouseUp@WidgetBase@@UAEXHH@Z"));
    }

    fn comdat(name: &str, sec: usize) -> ObjComdat {
        ObjComdat {
            name: name.into(),
            external: true,
            size: 16,
            nreloc: 0,
            chr: 0x6050_1020,
            sel: 2,
            bytes: vec![0xc2, 8, 0],
            relocs: Vec::new(),
            sec,
            sec_name: ".text".into(),
            parent: None,
            sym: sec,
            cksum: 0,
        }
    }

    fn unit(obj: &str, names: &[&str]) -> Unit {
        Unit::Obj(
            obj.into(),
            ObjInfo {
                comdats: names
                    .iter()
                    .enumerate()
                    .map(|(i, n)| comdat(n, i + 1))
                    .collect(),
                ..ObjInfo::default()
            },
        )
    }

    fn definer_of(arr: &Array, name: &str) -> Option<String> {
        arr.cands
            .iter()
            .find(|c| c.name == name && !c.dup)
            .and_then(|c| arr.objects.get(c.obj).cloned())
    }

    #[test]
    fn a_dropped_definer_hands_the_name_to_the_next_and_a_defined_one_takes_it() -> Result<()> {
        let p = Parsed {
            units: vec![
                unit("A.obj", &["x", "y"]),
                unit("B.obj", &["x"]),
                unit("C.obj", &[]),
            ],
            refused: Vec::new(),
        };
        let none = HashSet::new();
        let m = Model::default();
        let base = assemble(&p, &none, m, &[])?;
        assert_eq!(definer_of(&base, "x").as_deref(), Some("A.obj"));
        let dropped = assemble(&p, &none, m, &[Definer::parse("A.obj:x*", false)?])?;
        assert_eq!(definer_of(&dropped, "x").as_deref(), Some("B.obj"));
        assert_eq!(definer_of(&dropped, "y").as_deref(), Some("A.obj"));
        let late = assemble(&p, &none, m, &[Definer::parse("C.obj:y", true)?])?;
        assert_eq!(definer_of(&late, "y").as_deref(), Some("A.obj"));
        let moved = assemble(
            &p,
            &none,
            m,
            &[
                Definer::parse("A.obj:y", false)?,
                Definer::parse("C.obj:y", true)?,
            ],
        )?;
        assert_eq!(definer_of(&moved, "y").as_deref(), Some("C.obj"));
        Ok(())
    }

    #[test]
    fn a_dead_external_stays_dead_wherever_it_is_taken_from() -> Result<()> {
        let p = Parsed {
            units: vec![unit("A.obj", &["x"]), unit("B.obj", &["x"])],
            refused: Vec::new(),
        };
        let dead: HashSet<(String, String)> = [("x".to_string(), "a.obj".to_string())].into();
        let arr = assemble(
            &p,
            &dead,
            Model::default(),
            &[Definer::parse("A.obj:x", false)?],
        )?;
        assert!(
            arr.cands
                .iter()
                .any(|c| c.name == "x" && c.obj == 1 && c.dead)
        );
        Ok(())
    }

    #[test]
    fn a_load_what_if_moves_one_element_of_the_link_order() -> Result<()> {
        let keys = Keys::new(["engine"]);
        let m = |lib: &str, member: &str| Entry::Member {
            lib: lib.into(),
            member: member.into(),
        };
        let order = vec![
            Entry::Object("Plot.obj".into()),
            m("engine.lib", "Arena.obj"),
            m("engine.lib", "Dialog.obj"),
            m("libc.lib", "a.obj"),
        ];
        let l = |o: &str, b: Option<&str>| LoadAt {
            obj: o.into(),
            before: b.map(Into::into),
        };
        let got = apply_loads(&order, &[l("Dialog.obj", Some("Arena.obj"))], &keys)?;
        let spelled: Vec<String> = got.iter().map(Entry::spelled).collect();
        assert_eq!(
            spelled,
            [
                "Plot.obj",
                "engine.lib(Dialog.obj)",
                "engine.lib(Arena.obj)",
                "libc.lib(a.obj)"
            ]
        );
        let got = apply_loads(&order, &[l("Plot.obj", None)], &keys)?;
        assert_eq!(got.last().map(Entry::spelled).as_deref(), Some("Plot.obj"));
        assert!(apply_loads(&order, &[l("Nope.obj", None)], &keys).is_err());
        Ok(())
    }
}
