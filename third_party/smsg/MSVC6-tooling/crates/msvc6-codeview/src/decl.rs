use crate::codec::{Gen, int_at, name_text, u16_at, u32_at};
use crate::record::{ATTR_COMPGEN, ATTR_PSEUDO, Access, Field, MProp, Proc, Record};
use crate::ty::{Key, Ty, prim_size};
use anyhow::{Result, bail};
use serde::Serialize;
use std::cell::Cell;
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::fmt::Write as _;

pub const TYPE_DEPTH: usize = 12;
pub const TYPE_NODES: usize = 1024;
pub const NODES_FLOOR: usize = 1 << 18;
pub const NODES_PER_BYTE: usize = 2;
pub const MEMBERS_FLOOR: usize = 1 << 16;
pub const BYTES_PER_MEMBER: usize = 4;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Body {
    pub file: String,
    pub line: u16,
    pub rva: u32,
    pub module: String,
    pub project: bool,
}

impl Body {
    #[must_use]
    pub fn base(&self) -> String {
        base_name(&self.file)
    }

    #[must_use]
    pub fn in_header(&self) -> bool {
        is_header(&self.file)
    }
}

#[must_use]
pub fn base_name(p: &str) -> String {
    p.rsplit(['\\', '/'])
        .next()
        .unwrap_or(p)
        .to_ascii_lowercase()
}

#[must_use]
pub fn is_header(p: &str) -> bool {
    let b = base_name(p);
    b.ends_with(".h") || b.ends_with(".hpp") || b.ends_with(".inl")
}

#[derive(Debug, Clone)]
pub struct RMethod {
    pub ret: Ty,
    pub params: Vec<Ty>,
    pub varargs: bool,
    pub cc: Option<String>,
    pub mprop: MProp,
    pub konst: bool,
    pub slot: Option<u32>,
    pub slot_measured: bool,
    pub implicit: bool,
    pub param_names: Vec<String>,
    pub body: Option<Body>,
    pub decorated: Option<String>,
    pub access: Option<Access>,
}

#[derive(Debug, Clone)]
pub enum RKind {
    Field { off: i64, ty: Ty },
    Static { ty: Ty },
    Method(Box<RMethod>),
    Nested { ty: Ty },
    VfPtr,
    Friend { ty: Ty },
}

#[derive(Debug, Clone)]
pub struct RMember {
    pub name: String,
    pub access: Option<Access>,
    pub kind: RKind,
}

#[derive(Debug, Clone)]
pub struct RBase {
    pub access: Option<Access>,
    pub name: String,
    pub virt: bool,
    pub off: i64,
}

#[derive(Debug, Clone)]
pub struct RClass {
    pub ti: u32,
    pub key: Key,
    pub name: String,
    pub size: u64,
    pub bases: Vec<RBase>,
    pub members: Vec<RMember>,
}

fn ranked(n: BTreeMap<String, usize>) -> Vec<(String, usize)> {
    let mut v: Vec<(String, usize)> = n.into_iter().collect();
    v.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
    v
}

impl RClass {
    pub fn bodies(&self) -> impl Iterator<Item = (&RMember, &Body)> {
        self.members.iter().filter_map(|m| match &m.kind {
            RKind::Method(x) => x.body.as_ref().map(|b| (m, b)),
            _ => None,
        })
    }

    #[must_use]
    pub fn headers(&self) -> Vec<(String, usize)> {
        let mut n: BTreeMap<String, usize> = BTreeMap::new();
        for (_, b) in self.bodies().filter(|(_, b)| b.in_header() && b.project) {
            *n.entry(b.base()).or_default() += 1;
        }
        ranked(n)
    }

    #[must_use]
    pub fn header(&self) -> Option<String> {
        self.headers().into_iter().next().map(|(h, _)| h)
    }

    #[must_use]
    pub fn sources(&self) -> Vec<(String, usize)> {
        let mut n: BTreeMap<String, usize> = BTreeMap::new();
        for (_, b) in self.bodies().filter(|(_, b)| !b.in_header() && b.project) {
            *n.entry(b.base()).or_default() += 1;
        }
        ranked(n)
    }
}

#[derive(Debug, Clone)]
pub struct REnum {
    pub name: String,
    pub utype: Ty,
    pub values: Vec<(String, i64)>,
}

#[derive(Debug, Clone, Default)]
pub struct Model {
    pub classes: Vec<RClass>,
    pub enums: Vec<REnum>,
    pub typedefs: BTreeMap<String, Ty>,
    pub by_name: HashMap<String, usize>,
    pub enum_by_name: HashMap<String, usize>,
    pub scope: BTreeSet<String>,
    pub statics: BTreeMap<String, u32>,
    pub project_dir: String,
    pub nodes: usize,
    pub node_cap: usize,
    pub members: usize,
    pub member_cap: usize,
}

struct ProcIndex<'a> {
    by_name: HashMap<&'a str, (usize, &'a Proc)>,
    by_name_ty: HashMap<(&'a str, u32), (Option<&'a Proc>, &'a Proc)>,
}

impl<'a> ProcIndex<'a> {
    fn new(rec: &'a Record, own: &BTreeSet<usize>) -> Self {
        let mut by_name: HashMap<&str, (usize, &Proc)> = HashMap::new();
        let mut by_name_ty: HashMap<(&str, u32), (Option<&Proc>, &Proc)> = HashMap::new();
        for p in rec.procs() {
            let e = by_name.entry(p.name.as_str()).or_insert((0, p));
            e.0 = e.0.saturating_add(1);
            let mine = own.contains(&p.module);
            let k = by_name_ty
                .entry((p.name.as_str(), p.ty))
                .or_insert((None, p));
            if mine && k.0.is_none() {
                k.0 = Some(p);
            }
        }
        Self {
            by_name,
            by_name_ty,
        }
    }

    fn pick(&self, full: &str, mt: u32, nparams: usize) -> Option<&'a Proc> {
        if let Some((own, first)) = self.by_name_ty.get(&(full, mt)) {
            return Some(own.unwrap_or(first));
        }
        let (n, only) = self.by_name.get(full)?;
        (*n == 1 && only.params().len() == nparams).then_some(*only)
    }
}

impl Model {
    #[must_use]
    pub fn class(&self, name: &str) -> Option<&RClass> {
        self.by_name.get(name).and_then(|&i| self.classes.get(i))
    }

    #[must_use]
    pub fn enumeration(&self, name: &str) -> Option<&REnum> {
        self.enum_by_name.get(name).and_then(|&i| self.enums.get(i))
    }

    pub fn build(rec: &Record) -> Result<Self> {
        let own: BTreeSet<usize> = rec
            .modules
            .iter()
            .enumerate()
            .filter(|(_, m)| m.ilib == 0 && !m.name.to_ascii_lowercase().ends_with(".exp"))
            .map(|(i, _)| i)
            .collect();
        let procs = ProcIndex::new(rec, &own);
        let pubs: HashMap<u32, &str> = rec
            .publics
            .iter()
            .map(|p| (p.rva, p.name.as_str()))
            .collect();
        let mut dirs: BTreeMap<String, usize> = BTreeMap::new();
        for &i in &own {
            for f in rec
                .modules
                .get(i)
                .map(|m| m.files.as_slice())
                .unwrap_or_default()
            {
                let l = f.name.to_ascii_lowercase();
                if (l.ends_with(".cpp") || l.ends_with(".c"))
                    && let Some((d, _)) = l.rsplit_once('\\')
                {
                    *dirs.entry(format!("{d}\\")).or_default() += 1;
                }
            }
        }
        let project_dir = dirs
            .into_iter()
            .max_by_key(|(_, n)| *n)
            .map(|(d, _)| d)
            .unwrap_or_default();
        let mut m = Model {
            project_dir: project_dir.clone(),
            ..Model::default()
        };
        let conv = Conv::new(rec, project_dir);
        let mut members = 0usize;
        for (ti, r) in rec.types() {
            match r.leaf {
                0x1004..=0x1006 => {
                    let Some(h) = udt_head(r.leaf, r.data) else {
                        continue;
                    };
                    if h.prop & 0x80 != 0 || m.by_name.contains_key(&h.name) {
                        continue;
                    }
                    let c = conv.class(ti, &h, &procs, &pubs, &own);
                    members = members.saturating_add(c.members.len());
                    if members > conv.member_cap {
                        bail!(
                            "the record's classes declare over {} members, more than its {} bytes hold: a malformed record",
                            conv.member_cap,
                            rec.bytes
                        );
                    }
                    m.by_name.insert(c.name.clone(), m.classes.len());
                    m.classes.push(c);
                }
                0x1007 => {
                    let Some((prop, utype, field, name)) = enum_head(r.data) else {
                        continue;
                    };
                    if prop & 0x80 != 0 || m.enum_by_name.contains_key(&name) {
                        continue;
                    }
                    let values: Vec<(String, i64)> = rec
                        .fields(field)
                        .into_iter()
                        .filter_map(|f| match f {
                            Field::Enumerate { name, value } => Some((name, value)),
                            _ => None,
                        })
                        .collect();
                    members = members.saturating_add(values.len());
                    if members > conv.member_cap {
                        bail!(
                            "the record's types declare over {} members, more than its {} bytes hold: a malformed record",
                            conv.member_cap,
                            rec.bytes
                        );
                    }
                    m.enum_by_name.insert(name.clone(), m.enums.len());
                    m.enums.push(REnum {
                        name,
                        utype: conv.ty(utype),
                        values,
                    });
                }
                _ => {}
            }
            conv.check()?;
        }
        for c in &mut m.classes {
            let Some(slots) = vftable_name(&c.name).and_then(|n| rec.vftables.get(&n)) else {
                continue;
            };
            for mm in &mut c.members {
                let RKind::Method(x) = &mut mm.kind else {
                    continue;
                };
                let Some(rva) = x.body.as_ref().map(|b| b.rva) else {
                    continue;
                };
                if !x.mprop.is_virtual() {
                    continue;
                }
                if let Some(k) = slots.iter().position(|t| *t == rva) {
                    x.slot = u32::try_from(k.saturating_mul(4)).ok();
                    x.slot_measured = true;
                }
            }
        }
        for (name, ti) in &rec.udts {
            let t = conv.ty(*ti);
            let own_name = matches!(&t, Ty::Udt { name: n, .. } if n == name);
            if !own_name {
                m.typedefs.entry(name.clone()).or_insert(t);
            }
        }
        for g in rec
            .globals
            .iter()
            .chain(rec.modules.iter().flat_map(|x| x.data.iter()))
        {
            if g.name.contains("::") {
                m.statics.entry(g.name.clone()).or_insert(g.rva);
            }
        }
        let mut used: Vec<(Option<Key>, String)> = Vec::new();
        for (i, md) in rec.modules.iter().enumerate() {
            if !own.contains(&i) {
                continue;
            }
            for p in &md.procs {
                conv.ty(p.ty).names(&mut used);
                for l in &p.locals {
                    conv.ty(l.ty).names(&mut used);
                }
            }
            for d in &md.data {
                conv.ty(d.ty).names(&mut used);
            }
            conv.check()?;
        }
        conv.check()?;
        let used: BTreeSet<String> = used.into_iter().map(|(_, n)| n).collect();
        m.scope = m.scope_of(&own, rec, &used);
        m.nodes = conv.nodes.get();
        m.node_cap = conv.node_cap;
        m.members = members;
        m.member_cap = conv.member_cap;
        Ok(m)
    }

    fn scope_of(
        &self,
        own: &BTreeSet<usize>,
        rec: &Record,
        used: &BTreeSet<String>,
    ) -> BTreeSet<String> {
        let own_modules: BTreeSet<&str> = rec
            .modules
            .iter()
            .enumerate()
            .filter(|(i, _)| own.contains(i))
            .map(|(_, md)| md.name.as_str())
            .collect();
        let mut seed: Vec<String> = Vec::new();
        for c in &self.classes {
            let has = c.members.iter().any(|mm| match &mm.kind {
                RKind::Method(x) => x
                    .body
                    .as_ref()
                    .is_some_and(|b| b.project && own_modules.contains(b.module.as_str())),
                _ => false,
            });
            if has {
                seed.push(c.name.clone());
            }
        }
        let mut out: BTreeSet<String> = BTreeSet::new();
        let mut sig: BTreeSet<String> = BTreeSet::new();
        while let Some(n) = seed.pop() {
            if !out.insert(n.clone()) {
                continue;
            }
            let Some(c) = self.class(&n) else { continue };
            let mut names = Vec::new();
            for b in &c.bases {
                names.push((None, b.name.clone()));
            }
            for mm in &c.members {
                match &mm.kind {
                    RKind::Field { ty, .. } | RKind::Static { ty } | RKind::Nested { ty } => {
                        ty.names(&mut names);
                    }
                    RKind::Method(x) => {
                        let mut s = Vec::new();
                        x.ret.names(&mut s);
                        x.params.iter().for_each(|p| p.names(&mut s));
                        sig.extend(s.into_iter().map(|(_, n)| n));
                    }
                    _ => {}
                }
            }
            for (_, n) in names {
                if !out.contains(&n) {
                    seed.push(n);
                }
            }
        }
        for n in sig.iter().chain(used.iter()) {
            if self.enum_by_name.contains_key(n) {
                out.insert(n.clone());
            }
        }
        out
    }
}

#[must_use]
pub fn vftable_name(class: &str) -> Option<String> {
    if class.contains('<') {
        return None;
    }
    let mut rev: Vec<&str> = class.split("::").collect();
    rev.reverse();
    Some(format!("??_7{}@@6B@", rev.join("@")))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UdtHead {
    pub key: Key,
    pub prop: u16,
    pub field: u32,
    pub size: u64,
    pub name: String,
}

#[must_use]
pub fn udt_head(leaf: u16, d: &[u8]) -> Option<UdtHead> {
    let key = match leaf {
        0x1004 => Key::Class,
        0x1005 => Key::Struct,
        _ => Key::Union,
    };
    let at = if key == Key::Union { 8 } else { 16 };
    let (size, k) = int_at(d, at).ok()?;
    let (name, _) = name_text(d, at.saturating_add(k), Gen::St).ok()?;
    Some(UdtHead {
        key,
        prop: u16_at(d, 2).ok()?,
        field: u32_at(d, 4).ok()?,
        size: u64::try_from(size).unwrap_or(0),
        name,
    })
}

#[must_use]
pub fn enum_head(d: &[u8]) -> Option<(u16, u32, u32, String)> {
    Some((
        u16_at(d, 2).ok()?,
        u32_at(d, 4).ok()?,
        u32_at(d, 8).ok()?,
        name_text(d, 12, Gen::St).ok()?.0,
    ))
}

#[must_use]
pub fn cc_word(c: u8) -> Option<&'static str> {
    match c {
        0 | 1 => Some("__cdecl"),
        2 | 3 => Some("__pascal"),
        4 | 5 => Some("__fastcall"),
        7 | 8 => Some("__stdcall"),
        11 => Some("__thiscall"),
        _ => None,
    }
}

#[must_use]
pub fn prim(ti: u32) -> Ty {
    let base = match ti & 0xff {
        0x00 => "notype",
        0x03 => "void",
        0x08 => "HRESULT",
        0x10 => "signed char",
        0x11 => "short",
        0x12 => "long",
        0x13 => "__int64",
        0x20 => "unsigned char",
        0x21 => "unsigned short",
        0x22 => "unsigned long",
        0x23 => "unsigned __int64",
        0x30 => "bool",
        0x40 => "float",
        0x41 => "double",
        0x42 => "long double",
        0x68 => "char",
        0x69 => "unsigned char",
        0x70 => "char",
        0x71 => "wchar_t",
        0x72 => "short",
        0x73 => "unsigned short",
        0x74 => "int",
        0x75 => "unsigned int",
        0x76 => "__int64",
        0x77 => "unsigned __int64",
        x => return Ty::Unknown(format!("prim_{x:02x}")),
    };
    let t = Ty::prim(base);
    if (ti >> 8) & 7 != 0 { Ty::ptr(t) } else { t }
}

struct Conv<'a> {
    rec: &'a Record,
    project_dir: String,
    sizes: HashMap<String, u64>,
    nodes: Cell<usize>,
    node_cap: usize,
    member_cap: usize,
    call: Cell<usize>,
}

impl<'a> Conv<'a> {
    fn new(rec: &'a Record, project_dir: String) -> Self {
        let mut sizes = HashMap::new();
        for (_, t) in rec.types() {
            if !(0x1004..=0x1006).contains(&t.leaf) {
                continue;
            }
            if let Some(h) = udt_head(t.leaf, t.data)
                && h.prop & 0x80 == 0
            {
                sizes.entry(h.name).or_insert(h.size);
            }
        }
        Self {
            rec,
            project_dir,
            sizes,
            nodes: Cell::new(0),
            node_cap: rec.bytes.saturating_mul(NODES_PER_BYTE).max(NODES_FLOOR),
            member_cap: (rec.bytes / BYTES_PER_MEMBER).max(MEMBERS_FLOOR),
            call: Cell::new(0),
        }
    }

    fn check(&self) -> Result<()> {
        if self.nodes.get() > self.node_cap {
            bail!(
                "the record's types expand past {} nodes, more than its {} bytes can mean: a malformed record",
                self.node_cap,
                self.rec.bytes
            );
        }
        Ok(())
    }

    fn ty(&self, ti: u32) -> Ty {
        self.call.set(0);
        self.ty_depth(ti, 0)
    }

    fn spend(&self) -> bool {
        let c = self.call.get().saturating_add(1);
        self.call.set(c);
        self.nodes.set(self.nodes.get().saturating_add(1));
        c <= TYPE_NODES && self.nodes.get() <= self.node_cap
    }

    fn ty_depth(&self, ti: u32, depth: usize) -> Ty {
        if !self.spend() {
            return Ty::Unknown(format!("T{ti:x}"));
        }
        if ti < 0x1000 {
            return prim(ti);
        }
        if depth > TYPE_DEPTH {
            return Ty::Unknown(format!("T{ti:x}"));
        }
        let Some(r) = self.rec.ty(ti) else {
            return Ty::Unknown(format!("T{ti:x}"));
        };
        let d = r.data;
        let sub = |t: u32| self.ty_depth(t, depth.saturating_add(1));
        match r.leaf {
            0x1001 => {
                let a = u16_at(d, 4).unwrap_or(0);
                Ty::Cv {
                    to: Box::new(sub(u32_at(d, 0).unwrap_or(0))),
                    konst: a & 1 != 0,
                    volat: a & 2 != 0,
                }
            }
            0x1002 => {
                let a = u32_at(d, 4).unwrap_or(0);
                Ty::Ptr {
                    to: Box::new(sub(u32_at(d, 0).unwrap_or(0))),
                    reference: (a >> 5) & 7 == 1,
                    konst: a & 0x400 != 0,
                    volat: a & 0x200 != 0,
                }
            }
            0x1003 => {
                let elem = u32_at(d, 0).unwrap_or(0);
                let bytes = int_at(d, 8).map_or(0, |(v, _)| v);
                let es = self.size_of(elem);
                Ty::Array {
                    elem: Box::new(sub(elem)),
                    count: es
                        .filter(|s| *s > 0)
                        .map(|s| u64::try_from(bytes).unwrap_or(0) / s),
                }
            }
            0x1004..=0x1006 => match udt_head(r.leaf, d) {
                Some(h) => Ty::udt(Some(h.key), &h.name),
                None => Ty::Unknown(format!("T{ti:x}")),
            },
            0x1007 => match enum_head(d) {
                Some((_, _, _, n)) => Ty::udt(Some(Key::Enum), &n),
                None => Ty::Unknown(format!("T{ti:x}")),
            },
            0x1008 => {
                let (params, varargs) = self.args(u32_at(d, 8).unwrap_or(0), depth);
                Ty::Func {
                    ret: Box::new(sub(u32_at(d, 0).unwrap_or(0))),
                    cc: d.get(4).and_then(|c| cc_word(*c)).map(str::to_string),
                    params,
                    varargs,
                }
            }
            0x1009 => {
                let (params, varargs) = self.args(u32_at(d, 16).unwrap_or(0), depth);
                Ty::Func {
                    ret: Box::new(sub(u32_at(d, 0).unwrap_or(0))),
                    cc: d.get(12).and_then(|c| cc_word(*c)).map(str::to_string),
                    params,
                    varargs,
                }
            }
            0x1205 => Ty::Bits {
                ty: Box::new(sub(u32_at(d, 0).unwrap_or(0))),
                width: d.get(4).copied().unwrap_or(0),
            },
            other => Ty::Unknown(format!("T{ti:x}(leaf {other:#x})")),
        }
    }

    fn args(&self, al: u32, depth: usize) -> (Vec<Ty>, bool) {
        let Some(r) = self.rec.ty(al).filter(|r| r.leaf == 0x1201) else {
            return (Vec::new(), false);
        };
        let n = crate::codec::us(u32_at(r.data, 0).unwrap_or(0))
            .min(r.data.len().saturating_sub(4) / 4);
        let mut v = Vec::new();
        let mut varargs = false;
        for i in 0..n {
            if self.call.get() > TYPE_NODES {
                v.push(Ty::Unknown("...".into()));
                break;
            }
            let t = u32_at(r.data, 4usize.saturating_add(i.saturating_mul(4))).unwrap_or(0);
            if t == 0 && i.saturating_add(1) == n {
                varargs = true;
            } else {
                v.push(self.ty_depth(t, depth.saturating_add(1)));
            }
        }
        (v, varargs)
    }

    fn size_of(&self, ti: u32) -> Option<u64> {
        self.size_depth(ti, 0)
    }

    fn size_depth(&self, ti: u32, depth: usize) -> Option<u64> {
        if ti < 0x1000 {
            if (ti >> 8) & 7 != 0 {
                return Some(4);
            }
            return match prim(ti) {
                Ty::Prim(s) => prim_size(&s),
                _ => None,
            };
        }
        if depth > TYPE_DEPTH {
            return None;
        }
        let r = self.rec.ty(ti)?;
        match r.leaf {
            0x1001 => self.size_depth(u32_at(r.data, 0).ok()?, depth.saturating_add(1)),
            0x1002 => Some(4),
            0x1003 => int_at(r.data, 8)
                .ok()
                .and_then(|(v, _)| u64::try_from(v).ok()),
            0x1007 => Some(4),
            0x1004..=0x1006 => {
                let h = udt_head(r.leaf, r.data)?;
                if h.prop & 0x80 == 0 {
                    return Some(h.size);
                }
                self.sizes.get(&h.name).copied()
            }
            _ => None,
        }
    }

    fn class(
        &self,
        ti: u32,
        h: &UdtHead,
        procs: &ProcIndex<'_>,
        pubs: &HashMap<u32, &str>,
        own: &BTreeSet<usize>,
    ) -> RClass {
        let mut c = RClass {
            ti,
            key: h.key,
            name: h.name.clone(),
            size: h.size,
            bases: Vec::new(),
            members: Vec::new(),
        };
        for f in self.rec.fields(h.field) {
            match f {
                Field::Base { attr, ty, off } => c.bases.push(RBase {
                    access: Access::of(attr),
                    name: udt_name(&self.ty(ty)),
                    virt: false,
                    off,
                }),
                Field::VBase { attr, ty, .. } => c.bases.push(RBase {
                    access: Access::of(attr),
                    name: udt_name(&self.ty(ty)),
                    virt: true,
                    off: 0,
                }),
                Field::Member {
                    attr,
                    ty,
                    off,
                    name,
                } => c.members.push(RMember {
                    name,
                    access: Access::of(attr),
                    kind: RKind::Field {
                        off,
                        ty: self.ty(ty),
                    },
                }),
                Field::Static { attr, ty, name } => c.members.push(RMember {
                    name,
                    access: Access::of(attr),
                    kind: RKind::Static { ty: self.ty(ty) },
                }),
                Field::Nested { ty, name } => c.members.push(RMember {
                    name,
                    access: None,
                    kind: RKind::Nested { ty: self.ty(ty) },
                }),
                Field::VfPtr => c.members.push(RMember {
                    name: String::new(),
                    access: None,
                    kind: RKind::VfPtr,
                }),
                Field::FriendClass { ty } => c.members.push(RMember {
                    name: String::new(),
                    access: None,
                    kind: RKind::Friend { ty: self.ty(ty) },
                }),
                Field::FriendFn { ty, name } => c.members.push(RMember {
                    name,
                    access: None,
                    kind: RKind::Friend { ty: self.ty(ty) },
                }),
                Field::Method { name, overloads } => {
                    for (k, (attr, mt, vb)) in overloads.into_iter().rev().enumerate() {
                        let mut m = self.method(&c.name, &name, attr, mt, vb, procs, pubs, own);
                        let back = u32::try_from(k).unwrap_or(0).saturating_mul(4);
                        m.slot = vb.and_then(|v| v.checked_sub(back));
                        c.members.push(RMember {
                            name: name.clone(),
                            access: m.access.or(Access::of(attr)),
                            kind: RKind::Method(Box::new(m)),
                        });
                    }
                }
                Field::OneMethod {
                    attr,
                    ty,
                    vbaseoff,
                    name,
                } => {
                    let m = self.method(&c.name, &name, attr, ty, vbaseoff, procs, pubs, own);
                    c.members.push(RMember {
                        name,
                        access: m.access.or(Access::of(attr)),
                        kind: RKind::Method(Box::new(m)),
                    });
                }
                Field::Enumerate { .. } | Field::Unknown(_) => {}
            }
            if c.members.len() > self.member_cap {
                break;
            }
        }
        c
    }

    #[allow(clippy::too_many_arguments)]
    fn method(
        &self,
        class: &str,
        name: &str,
        attr: u16,
        mt: u32,
        vtoff: Option<u32>,
        procs: &ProcIndex<'_>,
        pubs: &HashMap<u32, &str>,
        own: &BTreeSet<usize>,
    ) -> RMethod {
        let _ = own;
        let (mut ret, mut params, mut varargs, mut cc, mut konst) =
            (Ty::prim("void"), Vec::new(), false, None, false);
        let mut nparams = 0usize;
        let mut has_this = false;
        if let Some(r) = self.rec.ty(mt).filter(|r| r.leaf == 0x1009) {
            if let Ty::Func {
                ret: r0,
                cc: c0,
                params: p0,
                varargs: v0,
            } = self.ty(mt)
            {
                ret = *r0;
                cc = c0;
                nparams = p0.len();
                params = p0;
                varargs = v0;
            }
            let this = u32_at(r.data, 8).unwrap_or(0);
            has_this = this != 0;
            if let Ty::Ptr { to, .. } = self.ty(this) {
                konst = matches!(*to, Ty::Cv { konst: true, .. });
            }
        }
        let full = format!("{class}::{name}");
        let pick = procs.pick(&full, mt, nparams);
        let body = pick.map(|p| {
            let (file, line) = self
                .rec
                .line_of(p.module, p.rva)
                .map_or((String::new(), 0), |(f, l)| (f.to_string(), l));
            let project = file.to_ascii_lowercase().starts_with(&self.project_dir);
            Body {
                file,
                line,
                rva: p.rva,
                module: self
                    .rec
                    .modules
                    .get(p.module)
                    .map(|m| m.name.clone())
                    .unwrap_or_default(),
                project,
            }
        });
        let param_names = pick
            .map(|p| p.params().iter().map(|l| l.name.clone()).collect())
            .unwrap_or_default();
        let decorated = pick
            .and_then(|p| pubs.get(&p.rva))
            .map(|s| (*s).to_string());
        let mut mprop = MProp::of(attr);
        let mut access = None;
        if has_this && mprop == MProp::Static {
            mprop = MProp::Vanilla;
        }
        if let Some((acc, virt, stat, k)) = decorated.as_deref().and_then(decorated_class) {
            access = Some(acc);
            konst = k;
            if stat {
                mprop = MProp::Static;
            } else if virt && !mprop.is_virtual() {
                mprop = MProp::Virtual;
            } else if !virt && mprop.is_virtual() {
                mprop = MProp::Vanilla;
            }
        }
        if let Some(d) = &decorated
            && let Some((r2, p2)) = decorated_types(d)
            && p2.len() == params.len()
        {
            ret = r2;
            params = p2;
        }
        let implicit = attr & (ATTR_PSEUDO | ATTR_COMPGEN) != 0
            || name.starts_with("__")
            || (body.is_none() && decorated.is_none() && is_copy_member(class, name, &params));
        RMethod {
            ret,
            params,
            varargs,
            cc,
            mprop,
            konst,
            slot: vtoff,
            slot_measured: false,
            implicit,
            param_names,
            body,
            decorated,
            access,
        }
    }
}

fn is_copy_member(class: &str, name: &str, params: &[Ty]) -> bool {
    let leaf = class.rsplit("::").next().unwrap_or(class);
    if name != leaf && name != "operator=" {
        return false;
    }
    matches!(params, [Ty::Ptr { to, reference: true, .. }]
        if matches!(&**to, Ty::Cv { to: inner, konst: true, .. }
            if matches!(&**inner, Ty::Udt { name: n, .. } if n == class)))
}

#[must_use]
pub fn udt_name(t: &Ty) -> String {
    match t {
        Ty::Udt { name, .. } => name.clone(),
        other => other.spell(),
    }
}

#[must_use]
pub fn decorated_types(d: &str) -> Option<(Ty, Vec<Ty>)> {
    let r = msvc_demangler::parse(d).ok()?;
    match &r.symbol_type {
        msvc_demangler::Type::MemberFunction(_, _, params, _, ret)
        | msvc_demangler::Type::NonMemberFunction(_, params, _, ret) => {
            let ps: Vec<Ty> = params
                .types
                .iter()
                .filter(|t| !matches!(t, msvc_demangler::Type::Void(_)))
                .map(|t| dem_ty(t, 0))
                .collect();
            Some((dem_ty(ret, 0), ps))
        }
        _ => None,
    }
}

#[must_use]
pub fn decorated_class(d: &str) -> Option<(Access, bool, bool, bool)> {
    use msvc_demangler::{FuncClass, StorageClass, Type};
    let r = msvc_demangler::parse(d).ok()?;
    let Type::MemberFunction(fc, _, _, sc, _) = &r.symbol_type else {
        return None;
    };
    let access = if fc.contains(FuncClass::PRIVATE) {
        Access::Private
    } else if fc.contains(FuncClass::PROTECTED) {
        Access::Protected
    } else {
        Access::Public
    };
    Some((
        access,
        fc.contains(FuncClass::VIRTUAL),
        fc.contains(FuncClass::STATIC),
        sc.contains(StorageClass::CONST),
    ))
}

fn dem_ty(t: &msvc_demangler::Type<'_>, depth: usize) -> Ty {
    use msvc_demangler::{StorageClass, Type};
    if depth > TYPE_DEPTH.saturating_mul(4) {
        return Ty::Unknown("...".into());
    }
    let d = depth.saturating_add(1);
    let cv = |inner: Ty, sc: &StorageClass| {
        if sc.intersects(StorageClass::CONST | StorageClass::VOLATILE) {
            Ty::Cv {
                to: Box::new(inner),
                konst: sc.contains(StorageClass::CONST),
                volat: sc.contains(StorageClass::VOLATILE),
            }
        } else {
            inner
        }
    };
    let sym = |s: &msvc_demangler::Symbol<'_>| {
        let mut parts: Vec<String> = s.scope.names.iter().map(|n| dem_name(n, d)).collect();
        parts.reverse();
        parts.push(dem_name(&s.name, d));
        parts.join("::")
    };
    match t {
        Type::Void(sc) => cv(Ty::prim("void"), sc),
        Type::Bool(sc) => cv(Ty::prim("bool"), sc),
        Type::Char(sc) => cv(Ty::prim("char"), sc),
        Type::Schar(sc) => cv(Ty::prim("signed char"), sc),
        Type::Uchar(sc) => cv(Ty::prim("unsigned char"), sc),
        Type::Short(sc) => cv(Ty::prim("short"), sc),
        Type::Ushort(sc) => cv(Ty::prim("unsigned short"), sc),
        Type::Int(sc) => cv(Ty::prim("int"), sc),
        Type::Uint(sc) => cv(Ty::prim("unsigned int"), sc),
        Type::Long(sc) => cv(Ty::prim("long"), sc),
        Type::Ulong(sc) => cv(Ty::prim("unsigned long"), sc),
        Type::Int64(sc) => cv(Ty::prim("__int64"), sc),
        Type::Uint64(sc) => cv(Ty::prim("unsigned __int64"), sc),
        Type::Wchar(sc) => cv(Ty::prim("wchar_t"), sc),
        Type::Float(sc) => cv(Ty::prim("float"), sc),
        Type::Double(sc) => cv(Ty::prim("double"), sc),
        Type::Ldouble(sc) => cv(Ty::prim("long double"), sc),
        Type::Struct(s, sc) => cv(Ty::udt(Some(Key::Struct), &sym(s)), sc),
        Type::Class(s, sc) => cv(Ty::udt(Some(Key::Class), &sym(s)), sc),
        Type::Union(s, sc) => cv(Ty::udt(Some(Key::Union), &sym(s)), sc),
        Type::Enum(s, sc) => cv(Ty::udt(Some(Key::Enum), &sym(s)), sc),
        Type::Ptr(inner, sc) | Type::Ref(inner, sc) => Ty::Ptr {
            to: Box::new(dem_ty(inner, d)),
            reference: matches!(t, Type::Ref(..)),
            konst: sc.contains(StorageClass::CONST),
            volat: sc.contains(StorageClass::VOLATILE),
        },
        Type::Array(n, inner, _) => Ty::Array {
            elem: Box::new(dem_ty(inner, d)),
            count: u64::try_from(*n).ok(),
        },
        Type::NonMemberFunction(cc, params, _, ret) => Ty::Func {
            ret: Box::new(dem_ty(ret, d)),
            cc: Some(dem_cc(*cc).to_string()),
            params: params
                .types
                .iter()
                .filter(|p| !matches!(p, Type::Void(_) | Type::VarArgs))
                .map(|p| dem_ty(p, d))
                .collect(),
            varargs: params.types.iter().any(|p| matches!(p, Type::VarArgs)),
        },
        other => Ty::Unknown(format!("{other:?}").chars().take(40).collect()),
    }
}

fn dem_cc(c: msvc_demangler::CallingConv) -> &'static str {
    use msvc_demangler::CallingConv;
    match c {
        CallingConv::Cdecl => "__cdecl",
        CallingConv::Pascal => "__pascal",
        CallingConv::Thiscall => "__thiscall",
        CallingConv::Stdcall => "__stdcall",
        CallingConv::Fastcall => "__fastcall",
        CallingConv::_Regcall => "__regcall",
    }
}

fn dem_name(n: &msvc_demangler::Name<'_>, depth: usize) -> String {
    use msvc_demangler::Name;
    match n {
        Name::NonTemplate(b) | Name::AsInterface(b) => msvc6_base::srcio::decode(b),
        Name::Template(inner, params) => {
            let ps: Vec<String> = params
                .types
                .iter()
                .map(|t| dem_ty(t, depth).spell())
                .collect();
            format!("{}<{}>", dem_name(inner, depth), ps.join(","))
        }
        other => format!("{other:?}"),
    }
}

#[must_use]
pub fn render(m: &Model, all: bool) -> String {
    let mut out = String::new();
    let in_scope = |n: &str| all || m.scope.contains(n);
    let mut by_header: BTreeMap<String, Vec<&RClass>> = BTreeMap::new();
    let mut unplaced: Vec<&RClass> = Vec::new();
    for c in m.classes.iter().filter(|c| in_scope(&c.name)) {
        match c.header() {
            Some(h) => by_header.entry(h).or_default().push(c),
            None => unplaced.push(c),
        }
    }
    let _ = writeln!(
        out,
        "// The record's declarations: {} class(es) in {} header(s), {} with no header recorded, {} enum(s).",
        by_header.values().map(Vec::len).sum::<usize>(),
        by_header.len(),
        unplaced.len(),
        m.enums.iter().filter(|e| in_scope(&e.name)).count()
    );
    for (h, cs) in &mut by_header {
        let first = |c: &RClass| {
            c.bodies()
                .filter(|(_, b)| b.base() == *h)
                .map(|(_, b)| b.line)
                .min()
                .unwrap_or(u16::MAX)
        };
        cs.sort_by_key(|c| first(c));
        let _ = writeln!(out, "\n// ===== {h}");
        for c in cs.iter() {
            out.push_str(&render_class(m, c));
        }
    }
    let _ = writeln!(out, "\n// ===== no header recorded (no inline body)");
    unplaced.sort_by_key(|c| c.ti);
    for c in unplaced {
        out.push_str(&render_class(m, c));
    }
    let _ = writeln!(out, "\n// ===== enums");
    for e in m.enums.iter().filter(|e| in_scope(&e.name)) {
        let u = match &e.utype {
            Ty::Prim(p) if p == "int" => String::new(),
            t => format!("  // underlying {}", t.spell()),
        };
        let _ = writeln!(out, "\nenum {} {{{u}", e.name);
        for (n, v) in &e.values {
            let _ = writeln!(out, "    {n} = {v},");
        }
        let _ = writeln!(out, "}};");
    }
    out
}

#[must_use]
pub fn render_class(m: &Model, c: &RClass) -> String {
    let mut s = String::new();
    let heads = c.headers();
    let srcs = c.sources();
    let list = |v: &[(String, usize)]| {
        v.iter()
            .map(|(f, n)| format!("{f} x{n}"))
            .collect::<Vec<_>>()
            .join(", ")
    };
    let _ = writeln!(
        s,
        "\n// T{:x}, size {:#x}; inline bodies: {}; out-of-line bodies: {}",
        c.ti,
        c.size,
        if heads.is_empty() {
            "none".into()
        } else {
            list(&heads)
        },
        if srcs.is_empty() {
            "none".into()
        } else {
            list(&srcs)
        }
    );
    let bases: Vec<String> = c
        .bases
        .iter()
        .map(|b| {
            format!(
                "{}{}{}{}",
                b.access.map_or(String::new(), |a| format!("{} ", a.word())),
                if b.virt { "virtual " } else { "" },
                b.name,
                if b.off == 0 {
                    String::new()
                } else {
                    format!(" /* at +{:#x} */", b.off)
                }
            )
        })
        .collect();
    let _ = writeln!(
        s,
        "{} {}{} {{",
        c.key.word(),
        c.name,
        if bases.is_empty() {
            String::new()
        } else {
            format!(" : {}", bases.join(", "))
        }
    );
    let default = if c.key == Key::Class {
        Access::Private
    } else {
        Access::Public
    };
    let mut cur = default;
    for mm in &c.members {
        let acc = mm.access.unwrap_or(cur);
        if mm.access.is_some() && acc != cur && !matches!(mm.kind, RKind::VfPtr) {
            let _ = writeln!(s, "{}:", acc.word());
            cur = acc;
        }
        let _ = writeln!(s, "    {}", render_member(m, c, mm));
    }
    let _ = writeln!(s, "}};");
    s
}

#[must_use]
pub fn member_note(x: &RMethod) -> Vec<String> {
    let mut note = Vec::new();
    if let Some(v) = x.slot {
        note.push(format!("vtbl +{v:#x}"));
    }
    match &x.body {
        Some(b) => note.push(format!(
            "{} {}:{}",
            if b.in_header() { "inline" } else { "body" },
            b.base(),
            b.line
        )),
        None if x.mprop.is_pure() => {}
        None => note.push("no body in the record".into()),
    }
    if x.decorated.is_none() && (mentions_uchar(&x.ret) || x.params.iter().any(mentions_uchar)) {
        note.push("unsigned char: or bool, no decorated name decides".into());
    }
    note
}

fn render_member(m: &Model, c: &RClass, mm: &RMember) -> String {
    match &mm.kind {
        RKind::VfPtr => "// (vfptr)".into(),
        RKind::Field { off, ty } => {
            let uc = mentions_uchar(ty);
            format!(
                "{};  // +{off:#x}{}",
                ty.keyless().decl(&mm.name),
                if uc { " (unsigned char: or bool)" } else { "" }
            )
        }
        RKind::Static { ty } => {
            let at = m
                .statics
                .get(&format!("{}::{}", c.name, mm.name))
                .map_or(String::new(), |r| format!("  // defined at {r:#x}"));
            format!("static {};{at}", ty.keyless().decl(&mm.name))
        }
        RKind::Nested { ty } => format!("// nested: {} {}", ty.spell(), mm.name),
        RKind::Friend { ty } => format!("friend {};", ty.spell()),
        RKind::Method(x) => {
            let sig = method_sig(c, &mm.name, x);
            if x.implicit {
                return format!("// implicit: {sig}");
            }
            format!("{sig}  // {}", member_note(x).join("; "))
        }
    }
}

#[must_use]
pub fn mentions_uchar(t: &Ty) -> bool {
    match t {
        Ty::Prim(p) => p == "unsigned char",
        Ty::Ptr { to, .. } | Ty::Cv { to, .. } => mentions_uchar(to),
        Ty::Array { elem, .. } => mentions_uchar(elem),
        Ty::Bits { ty, .. } => mentions_uchar(ty),
        _ => false,
    }
}

#[must_use]
pub fn method_sig(c: &RClass, name: &str, x: &RMethod) -> String {
    let leaf = c.name.rsplit("::").next().unwrap_or(&c.name);
    let ctor = name == leaf || name == format!("~{leaf}");
    let mut ps: Vec<String> = x
        .params
        .iter()
        .enumerate()
        .map(|(i, t)| {
            let n = x.param_names.get(i).map_or("", String::as_str);
            t.keyless().decl(if n == "__formal" { "" } else { n })
        })
        .collect();
    if x.varargs {
        ps.push("...".into());
    }
    let pre = match x.mprop {
        MProp::Static => "static ",
        p if p.is_virtual() => "virtual ",
        _ => "",
    };
    let cc = match x.cc.as_deref() {
        Some("__thiscall") | None => String::new(),
        Some(c) if x.mprop == MProp::Static && c == "__cdecl" => String::new(),
        Some(c) => format!("{c} "),
    };
    let ret = if ctor {
        String::new()
    } else {
        format!("{} ", x.ret.keyless().spell())
    };
    format!(
        "{pre}{ret}{cc}{name}({}){}{};",
        ps.join(", "),
        if x.konst { " const" } else { "" },
        if x.mprop.is_pure() { " = 0" } else { "" }
    )
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct DeclRow {
    pub class: String,
    pub ti: u32,
    pub key: &'static str,
    pub size: u64,
    pub header: Option<String>,
    pub member: String,
    pub kind: &'static str,
    pub access: Option<Access>,
    pub declaration: String,
    pub offset: Option<i64>,
    pub slot: Option<u32>,
    pub slot_measured: bool,
    pub implicit: bool,
    pub body: Option<Body>,
    pub decorated: Option<String>,
}

#[must_use]
pub fn rows(m: &Model, all: bool) -> Vec<DeclRow> {
    let mut out = Vec::new();
    for c in m
        .classes
        .iter()
        .filter(|c| all || m.scope.contains(&c.name))
    {
        let header = c.header();
        let default = if c.key == Key::Class {
            Access::Private
        } else {
            Access::Public
        };
        let mut cur = default;
        for mm in &c.members {
            let acc = mm.access.unwrap_or(cur);
            if mm.access.is_some() && !matches!(mm.kind, RKind::VfPtr) {
                cur = acc;
            }
            let base = DeclRow {
                class: c.name.clone(),
                ti: c.ti,
                key: c.key.word(),
                size: c.size,
                header: header.clone(),
                member: mm.name.clone(),
                kind: "",
                access: (!matches!(mm.kind, RKind::VfPtr | RKind::Nested { .. })).then_some(acc),
                declaration: String::new(),
                offset: None,
                slot: None,
                slot_measured: false,
                implicit: false,
                body: None,
                decorated: None,
            };
            out.push(match &mm.kind {
                RKind::VfPtr => DeclRow {
                    kind: "vfptr",
                    ..base
                },
                RKind::Field { off, ty } => DeclRow {
                    kind: "field",
                    declaration: format!("{};", ty.keyless().decl(&mm.name)),
                    offset: Some(*off),
                    ..base
                },
                RKind::Static { ty } => DeclRow {
                    kind: "static",
                    declaration: format!("static {};", ty.keyless().decl(&mm.name)),
                    ..base
                },
                RKind::Nested { ty } => DeclRow {
                    kind: "nested",
                    declaration: ty.spell(),
                    ..base
                },
                RKind::Friend { ty } => DeclRow {
                    kind: "friend",
                    declaration: format!("friend {};", ty.spell()),
                    ..base
                },
                RKind::Method(x) => DeclRow {
                    kind: "method",
                    declaration: method_sig(c, &mm.name, x),
                    slot: x.slot,
                    slot_measured: x.slot_measured,
                    implicit: x.implicit,
                    body: x.body.clone(),
                    decorated: x.decorated.clone(),
                    ..base
                },
            });
        }
    }
    for e in m.enums.iter().filter(|e| all || m.scope.contains(&e.name)) {
        for (n, v) in &e.values {
            out.push(DeclRow {
                class: e.name.clone(),
                ti: 0,
                key: "enum",
                size: 4,
                header: None,
                member: n.clone(),
                kind: "enumerator",
                access: None,
                declaration: format!("{n} = {v},"),
                offset: Some(*v),
                slot: None,
                slot_measured: false,
                implicit: false,
                body: None,
                decorated: None,
            });
        }
    }
    out
}
