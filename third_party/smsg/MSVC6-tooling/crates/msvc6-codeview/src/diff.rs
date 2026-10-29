use crate::decl::{Model, RClass, RKind, RMember, RMethod, base_name, is_header, method_sig};
use crate::record::{Access, MProp};
use crate::source::{
    Dcl, DeclTy, Loc, SClass, SDef, SKind, SMember, SMethod, Src, eval_const, leaf, lex,
};
use crate::ty::{Key, Ty, fundamental};
use crate::view::View;
use anyhow::{Result, bail};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};

pub const ORDER_UNITS: usize = 4096;

pub const SDK_CONSTANTS: &[(&str, i64)] = &[
    ("WM_USER", 0x400),
    ("WM_APP", 0x8000),
    ("MAX_PATH", 260),
    ("NULL", 0),
    ("FALSE", 0),
    ("TRUE", 1),
];

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub kind: &'static str,
    pub subject: String,
    pub ours: String,
    pub record: String,
    pub our_says: String,
    pub at: String,
    #[serde(skip)]
    pub fix: Option<Fix>,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Fix {
    pub file: PathBuf,
    pub from: usize,
    pub to: usize,
    pub old: String,
    pub new: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PlanMember {
    pub subject: String,
    pub ours: String,
    pub line: usize,
    pub end: usize,
    pub access: Access,
    pub want: Option<Access>,
    pub rec: Option<usize>,
    pub movable: bool,
    pub slots: bool,
    pub friend: bool,
    pub nested: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FieldRename {
    pub class: String,
    pub file: PathBuf,
    pub subject: String,
    pub old: String,
    pub new: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ClassPlan {
    pub class: String,
    pub file: PathBuf,
    pub line: usize,
    pub key: Key,
    pub members: Vec<PlanMember>,
}

#[derive(Debug)]
pub struct Resolver<'a> {
    pub src: &'a Src,
    pub rec: &'a Model,
    classes: HashMap<String, Key>,
    pub enums: BTreeMap<String, i64>,
}

fn sdk_constant(w: &str) -> Option<i64> {
    SDK_CONSTANTS.iter().find(|(k, _)| *k == w).map(|(_, v)| *v)
}

fn texts(s: &str) -> Vec<String> {
    lex(s, 1).into_iter().map(|t| t.text).collect()
}

impl<'a> Resolver<'a> {
    #[must_use]
    pub fn new(src: &'a Src, rec: &'a Model) -> Self {
        let mut classes = HashMap::new();
        for c in &src.classes {
            classes.insert(c.name.clone(), c.key);
        }
        for e in &src.enums {
            classes.insert(e.name.clone(), Key::Enum);
        }
        let mut r = Resolver {
            src,
            rec,
            classes,
            enums: BTreeMap::new(),
        };
        r.enums = r.enumerator_values();
        r
    }

    fn enumerator_values(&self) -> BTreeMap<String, i64> {
        let mut out: BTreeMap<String, i64> = BTreeMap::new();
        for e in &self.src.enums {
            let mut next = Some(0i64);
            for (n, v) in &e.values {
                let val = match v {
                    Some(x) => eval_const(&texts(x), &|w| {
                        out.get(w)
                            .copied()
                            .or_else(|| self.macro_value(w, &out, 0))
                            .or_else(|| sdk_constant(w))
                    }),
                    None => next,
                };
                if let Some(val) = val {
                    out.insert(n.clone(), val);
                }
                next = val.map(|x| x.wrapping_add(1));
            }
        }
        for e in &self.rec.enums {
            for (n, v) in &e.values {
                out.entry(n.clone()).or_insert(*v);
            }
        }
        out
    }

    fn macro_value(&self, w: &str, enums: &BTreeMap<String, i64>, depth: usize) -> Option<i64> {
        let body = self.src.macros.get(w)?;
        if depth > crate::source::MACRO_DEPTH {
            return None;
        }
        let toks = texts(body);
        if toks.is_empty() {
            return None;
        }
        eval_const(&toks, &|x| {
            enums
                .get(x)
                .copied()
                .or_else(|| self.macro_value(x, enums, depth.saturating_add(1)))
        })
    }

    #[must_use]
    pub fn constant(&self, toks: &[String]) -> Option<u64> {
        if toks.is_empty() {
            return None;
        }
        eval_const(toks, &|w| {
            self.enums
                .get(w)
                .copied()
                .or_else(|| self.macro_value(w, &self.enums, 0))
                .or_else(|| sdk_constant(w))
        })
        .and_then(|v| u64::try_from(v).ok())
    }

    fn lookup(&self, name: &str, scope: Option<&str>) -> Option<String> {
        let mut s = scope.map(str::to_string);
        loop {
            let q = match &s {
                Some(p) => format!("{p}::{name}"),
                None => name.to_string(),
            };
            if self.classes.contains_key(&q) || self.src.typedefs.contains_key(&q) {
                return Some(q);
            }
            let p = s.take()?;
            s = p.rsplit_once("::").map(|(a, _)| a.to_string());
        }
    }

    #[must_use]
    pub fn ty(&self, t: &DeclTy, scope: Option<&str>) -> Ty {
        self.ty_depth(t, scope, 0)
    }

    fn ty_depth(&self, t: &DeclTy, scope: Option<&str>, depth: usize) -> Ty {
        let base = self.base(&t.spec, scope, depth);
        self.apply(&t.dcl, base, scope, depth)
    }

    fn base(&self, spec: &[String], scope: Option<&str>, depth: usize) -> Ty {
        let konst = spec.iter().any(|w| w == "const");
        let volat = spec.iter().any(|w| w == "volatile");
        let words: Vec<&str> = spec
            .iter()
            .map(String::as_str)
            .filter(|w| *w != "const" && *w != "volatile")
            .collect();
        let t = if let Some(f) = fundamental(&words) {
            Ty::prim(f)
        } else if let [k, n] = words.as_slice() {
            let key = match *k {
                "class" => Some(Key::Class),
                "struct" => Some(Key::Struct),
                "union" => Some(Key::Union),
                "enum" => Some(Key::Enum),
                _ => None,
            };
            match key {
                Some(key) => {
                    let q = self.lookup(n, scope).unwrap_or_else(|| (*n).to_string());
                    Ty::udt(Some(key), &q)
                }
                None => Ty::Unknown(words.join(" ")),
            }
        } else if let [n] = words.as_slice() {
            self.named(n, scope, depth)
        } else {
            Ty::Unknown(words.join(" "))
        };
        if konst || volat {
            Ty::Cv {
                to: Box::new(t),
                konst,
                volat,
            }
        } else {
            t
        }
    }

    fn named(&self, n: &str, scope: Option<&str>, depth: usize) -> Ty {
        if depth > 16 {
            return Ty::Unknown(n.to_string());
        }
        if let Some(q) = self.lookup(n, scope) {
            if let Some(k) = self.classes.get(&q) {
                return Ty::udt(Some(*k), &q);
            }
            if let Some((td, _)) = self.src.typedefs.get(&q) {
                let s = q.rsplit_once("::").map(|(a, _)| a.to_string());
                return self.ty_depth(td, s.as_deref(), depth.saturating_add(1));
            }
        }
        if let Some(t) = self.rec.typedefs.get(n) {
            return t.clone();
        }
        if let Some(c) = self.rec.class(n) {
            return Ty::udt(Some(c.key), n);
        }
        if self.rec.enum_by_name.contains_key(n) {
            return Ty::udt(Some(Key::Enum), n);
        }
        Ty::Unknown(n.to_string())
    }

    fn apply(&self, d: &Dcl, base: Ty, scope: Option<&str>, depth: usize) -> Ty {
        match d {
            Dcl::Name(_) => base,
            Dcl::Ptr {
                inner,
                reference,
                konst,
                volat,
            } => self.apply(
                inner,
                Ty::Ptr {
                    to: Box::new(base),
                    reference: *reference,
                    konst: *konst,
                    volat: *volat,
                },
                scope,
                depth,
            ),
            Dcl::Array { inner, extent } => self.apply(
                inner,
                Ty::Array {
                    elem: Box::new(base),
                    count: self.constant(extent),
                },
                scope,
                depth,
            ),
            Dcl::Func {
                inner,
                params,
                varargs,
                cc,
                ..
            } => self.apply(
                inner,
                Ty::Func {
                    ret: Box::new(base),
                    cc: cc.clone(),
                    params: params
                        .iter()
                        .map(|p| self.ty_depth(&p.ty, scope, depth).param_decay())
                        .collect(),
                    varargs: *varargs,
                },
                scope,
                depth,
            ),
        }
    }
}

#[must_use]
pub fn same(rec: &Ty, ours: &Ty, uchar_bool: bool) -> bool {
    match (rec, ours) {
        (_, Ty::Unknown(_)) => true,
        (Ty::Prim(a), Ty::Prim(b)) => a == b || (uchar_bool && a == "unsigned char" && b == "bool"),
        (Ty::Udt { name: a, .. }, Ty::Udt { name: b, .. }) => a == b,
        (
            Ty::Ptr {
                to: a,
                reference: ra,
                konst: ka,
                volat: va,
            },
            Ty::Ptr {
                to: b,
                reference: rb,
                konst: kb,
                volat: vb,
            },
        ) => ra == rb && ka == kb && va == vb && same(a, b, uchar_bool),
        (
            Ty::Cv {
                to: a,
                konst: ka,
                volat: va,
            },
            Ty::Cv {
                to: b,
                konst: kb,
                volat: vb,
            },
        ) => ka == kb && va == vb && same(a, b, uchar_bool),
        (Ty::Array { elem: a, count: ca }, Ty::Array { elem: b, count: cb }) => {
            same(a, b, uchar_bool) && (ca.is_none() || cb.is_none() || ca == cb)
        }
        (
            Ty::Func {
                ret: ra,
                params: pa,
                varargs: va,
                ..
            },
            Ty::Func {
                ret: rb,
                params: pb,
                varargs: vb,
                ..
            },
        ) => {
            va == vb
                && pa.len() == pb.len()
                && same(ra, rb, uchar_bool)
                && pa.iter().zip(pb).all(|(x, y)| same(x, y, uchar_bool))
        }
        (Ty::Bits { ty: a, width: wa }, Ty::Bits { ty: b, width: wb }) => {
            wa == wb && same(a, b, uchar_bool)
        }
        _ => false,
    }
}

fn unresolved(t: &Ty) -> bool {
    match t {
        Ty::Unknown(_) => true,
        Ty::Ptr { to, .. } | Ty::Cv { to, .. } => unresolved(to),
        Ty::Array { elem, .. } => unresolved(elem),
        Ty::Bits { ty, .. } => unresolved(ty),
        Ty::Func { ret, params, .. } => unresolved(ret) || params.iter().any(unresolved),
        _ => false,
    }
}

fn core_class(t: &Ty) -> Option<&str> {
    match t {
        Ty::Udt { name, key } if *key != Some(Key::Enum) => Some(name),
        Ty::Ptr { to, .. } | Ty::Cv { to, .. } => core_class(to),
        Ty::Array { elem, .. } => core_class(elem),
        _ => None,
    }
}

fn rec_names(c: &RClass) -> BTreeSet<String> {
    c.members
        .iter()
        .filter(|m| match &m.kind {
            RKind::Method(x) => !x.implicit,
            RKind::Field { .. } | RKind::Static { .. } => true,
            _ => false,
        })
        .map(|m| m.name.clone())
        .collect()
}

fn our_names(c: &SClass) -> BTreeSet<String> {
    c.members
        .iter()
        .filter(|m| {
            matches!(
                m.kind,
                SKind::Field { .. } | SKind::Static { .. } | SKind::Method(_)
            )
        })
        .map(|m| m.name.clone())
        .collect()
}

#[must_use]
pub fn class_map(src: &Src, rec: &Model, ours: &[&SClass]) -> BTreeMap<String, String> {
    let rec_scope: Vec<&RClass> = rec
        .classes
        .iter()
        .filter(|c| rec.scope.contains(&c.name))
        .collect();
    let mut map: BTreeMap<String, String> = BTreeMap::new();
    let mut taken: BTreeSet<String> = BTreeSet::new();
    let rn: Vec<BTreeSet<String>> = rec_scope.iter().map(|c| rec_names(c)).collect();
    let mut pairs: Vec<(f64, usize, usize)> = Vec::new();
    for (i, o) in ours.iter().enumerate() {
        let on = our_names(o);
        for (j, (r, rset)) in rec_scope.iter().zip(&rn).enumerate() {
            if r.name.contains('<') {
                continue;
            }
            let inter = on.intersection(rset).count();
            let uni = on.union(rset).count();
            let same_name = o.name == r.name;
            if uni == 0 {
                if same_name {
                    pairs.push((0.5, i, j));
                }
                continue;
            }
            let jac = ratio(inter, uni);
            let score = jac + if same_name { 0.1 } else { 0.0 };
            if jac >= 0.3 || (same_name && (jac >= 0.1 || uni <= 3)) {
                pairs.push((score, i, j));
            }
        }
    }
    pairs.sort_by(|a, b| b.0.total_cmp(&a.0).then(a.1.cmp(&b.1)).then(a.2.cmp(&b.2)));
    for (_, i, j) in pairs {
        let (Some(o), Some(r)) = (ours.get(i), rec_scope.get(j)) else {
            continue;
        };
        if map.contains_key(&o.name) || taken.contains(&r.name) {
            continue;
        }
        map.insert(o.name.clone(), r.name.clone());
        taken.insert(r.name.clone());
    }
    let res = Resolver::new(src, rec);
    loop {
        let mut add: Vec<(String, String)> = Vec::new();
        for (o, r) in &map {
            let (Some(oc), Some(rc)) = (src.class(o), rec.class(r)) else {
                continue;
            };
            for (ob, rb) in oc.bases.iter().zip(&rc.bases) {
                add.push((ob.1.clone(), rb.name.clone()));
            }
            for om in &oc.members {
                if matches!(om.kind, SKind::Nested) {
                    add.push((format!("{o}::{}", om.name), format!("{r}::{}", om.name)));
                }
            }
            let ofs: Vec<(&str, &DeclTy)> = oc
                .members
                .iter()
                .filter_map(|m| match &m.kind {
                    SKind::Field { ty, .. } | SKind::Static { ty } => Some((m.name.as_str(), ty)),
                    _ => None,
                })
                .collect();
            let rfs: Vec<(&str, &Ty)> = rc
                .members
                .iter()
                .filter_map(|m| match &m.kind {
                    RKind::Field { ty, .. } | RKind::Static { ty } => Some((m.name.as_str(), ty)),
                    _ => None,
                })
                .collect();
            let by_pos = ofs.len() == rfs.len() && r.contains('<');
            for (k, (on, oty)) in ofs.iter().enumerate() {
                let rt = rfs
                    .iter()
                    .find(|(n, _)| n == on)
                    .map(|(_, t)| *t)
                    .or_else(|| by_pos.then(|| rfs.get(k).map(|x| x.1)).flatten());
                let Some(rt) = rt else { continue };
                let ot = res.ty(oty, Some(o));
                if let (Some(a), Some(b)) = (core_class(&ot), core_class(rt)) {
                    add.push((a.to_string(), b.to_string()));
                }
            }
        }
        let mut changed = false;
        for (a, b) in add {
            if map.contains_key(&a) || taken.contains(&b) {
                continue;
            }
            let (Some(oc), Some(rc)) = (ours.iter().find(|c| c.name == a), rec.class(&b)) else {
                continue;
            };
            let of = oc
                .members
                .iter()
                .filter(|m| matches!(m.kind, SKind::Field { .. }))
                .count();
            let rf = rc
                .members
                .iter()
                .filter(|m| matches!(m.kind, RKind::Field { .. }))
                .count();
            let (on, rn) = (our_names(oc), rec_names(rc));
            if of != rf
                && on.intersection(&rn).count().saturating_mul(5) < on.union(&rn).count()
                && !b.contains('<')
            {
                continue;
            }
            map.insert(a, b.clone());
            taken.insert(b);
            changed = true;
        }
        if !changed {
            break;
        }
    }
    for o in ours {
        if !map.contains_key(&o.name) && !taken.contains(&o.name) && rec.class(&o.name).is_some() {
            map.insert(o.name.clone(), o.name.clone());
            taken.insert(o.name.clone());
        }
    }
    map
}

fn ratio(a: usize, b: usize) -> f64 {
    let x = u32::try_from(a).map_or(f64::from(u32::MAX), f64::from);
    let y = u32::try_from(b).map_or(f64::from(u32::MAX), f64::from);
    x / y
}

fn norm_header(h: &str) -> String {
    base_name(h).replace(' ', "")
}

fn file_of(l: &Loc) -> String {
    l.file
        .file_name()
        .map(|f| f.to_string_lossy().to_ascii_lowercase())
        .unwrap_or_default()
}

#[derive(Debug, Clone)]
struct OurBody {
    loc: Loc,
    end: usize,
    in_class: bool,
    in_header: bool,
    params: Vec<Option<String>>,
}

#[derive(Debug)]
pub struct Diff<'a> {
    pub src: &'a Src,
    pub rec: &'a Model,
    pub res: Resolver<'a>,
    pub map: BTreeMap<String, String>,
    pub owner: bool,
    pub headers: PathBuf,
    pub rows: Vec<Row>,
    pub unresolved: BTreeSet<String>,
    pub plans: Vec<ClassPlan>,
    pub renames: Vec<FieldRename>,
}

fn word(b: bool, yes: &'static str, no: &'static str) -> &'static str {
    if b { yes } else { no }
}

fn default_access(key: Key) -> Access {
    if key == Key::Class {
        Access::Private
    } else {
        Access::Public
    }
}

impl Diff<'_> {
    fn ours_ty(&self, t: &DeclTy, scope: &str) -> Ty {
        let t = self.res.ty(t, Some(scope));
        t.map_udt(&|k, n| Ty::udt(k, self.map.get(n).map_or(n, String::as_str)))
    }

    fn set_fix(&mut self, at: &Loc, to: usize, old: &str, new: &str) {
        if let Some(r) = self.rows.last_mut() {
            r.fix = Some(Fix {
                file: at.file.clone(),
                from: at.line,
                to: to.max(at.line),
                old: old.to_string(),
                new: new.to_string(),
            });
        }
    }

    fn push(
        &mut self,
        kind: &'static str,
        subject: String,
        ours: String,
        rs: String,
        os: String,
        at: String,
    ) {
        self.rows.push(Row {
            kind,
            subject,
            ours,
            record: rs,
            our_says: os,
            at,
            fix: None,
        });
    }

    pub fn run(&mut self, ours: &[&SClass]) {
        let mut our_types: BTreeSet<String> = BTreeSet::new();
        for c in ours {
            for m in &c.members {
                if let SKind::Field { ty, .. } | SKind::Static { ty } = &m.kind {
                    let mut v = Vec::new();
                    self.ours_ty(ty, &c.name).names(&mut v);
                    our_types.extend(v.into_iter().map(|(_, n)| n));
                }
            }
        }
        for c in ours {
            let Some(r) = self.map.get(&c.name).and_then(|r| self.rec.class(r)) else {
                if !c.name.contains("__anon") {
                    self.push(
                        "class-extra",
                        format!("(ours {})", c.name),
                        c.name.clone(),
                        "no record class".into(),
                        format!("{} {}", c.key.word(), c.name),
                        c.loc.to_string(),
                    );
                }
                continue;
            };
            self.class(c, r);
        }
        let mapped: BTreeSet<String> = self.map.values().cloned().collect();
        if self.owner {
            let rec = self.rec;
            let mut templates: BTreeMap<String, Vec<String>> = BTreeMap::new();
            for r in rec.classes.iter().filter(|c| rec.scope.contains(&c.name)) {
                if mapped.contains(&r.name)
                    || our_types.contains(&r.name)
                    || r.name.contains("__unnamed")
                    || !r.bodies().any(|(_, b)| b.project)
                {
                    continue;
                }
                let h = r.header();
                if let Some((t, _)) = r.name.split_once('<') {
                    templates
                        .entry(format!("{t}<T>\t{}", h.clone().unwrap_or_default()))
                        .or_default()
                        .push(r.name.clone());
                    continue;
                }
                self.push(
                    "class-missing",
                    r.name.clone(),
                    String::new(),
                    format!(
                        "{} {} ({})",
                        r.key.word(),
                        r.name,
                        h.clone().unwrap_or_else(|| "no header recorded".into())
                    ),
                    "not declared".into(),
                    h.unwrap_or_default(),
                );
            }
            for (k, v) in templates {
                let (t, h) = k.split_once('\t').unwrap_or((&k, ""));
                self.push(
                    "class-missing",
                    t.to_string(),
                    String::new(),
                    format!(
                        "template {t} ({h}): {} instantiation(s) ours lacks: {}",
                        v.len(),
                        v.join(", ")
                    ),
                    "not declared".into(),
                    h.to_string(),
                );
            }
        }
        self.enums();
    }

    fn class(&mut self, c: &SClass, r: &RClass) {
        let subj = r.name.clone();
        let at = c.loc.to_string();
        if let Some((t, _)) = r.name.split_once('<').filter(|_| !c.template) {
            let before = self.rows.len();
            self.members(c, r);
            let mut n: BTreeMap<&str, usize> = BTreeMap::new();
            for row in self.rows.drain(before..) {
                *n.entry(row.kind).or_default() += 1;
            }
            let counts: Vec<String> = n.iter().map(|(k, v)| format!("{v} {k}")).collect();
            self.push(
                "class-name",
                subj,
                c.name.clone(),
                format!(
                    "template {t}<T> ({}, {}): {}",
                    r.name,
                    r.header().unwrap_or_default(),
                    if counts.is_empty() {
                        "members agree".into()
                    } else {
                        counts.join(", ")
                    }
                ),
                format!("{} {}", c.key.word(), c.name),
                at,
            );
            return;
        }
        if c.name != r.name {
            self.push(
                "class-name",
                subj.clone(),
                c.name.clone(),
                r.name.clone(),
                c.name.clone(),
                at.clone(),
            );
        }
        if c.key != r.key {
            self.push(
                "class-key",
                subj.clone(),
                c.name.clone(),
                r.key.word().into(),
                c.key.word().into(),
                at.clone(),
            );
        }
        let base_word = |a: Option<Access>, key: Key| a.unwrap_or(default_access(key)).word();
        let ob: Vec<String> = c
            .bases
            .iter()
            .map(|(a, n, v)| {
                format!(
                    "{}{} {}",
                    base_word(*a, c.key),
                    if *v { " virtual" } else { "" },
                    self.map.get(n).unwrap_or(n)
                )
            })
            .collect();
        let rb: Vec<String> = r
            .bases
            .iter()
            .map(|b| {
                format!(
                    "{}{} {}",
                    base_word(b.access, r.key),
                    if b.virt { " virtual" } else { "" },
                    b.name
                )
            })
            .collect();
        if ob != rb {
            self.push(
                "base",
                subj.clone(),
                c.name.clone(),
                rb.join(", "),
                ob.join(", "),
                at.clone(),
            );
        }
        if let Some(h) = r.header() {
            let ours = file_of(&c.loc);
            if norm_header(&h) != ours {
                self.push(
                    "header-placement",
                    subj.clone(),
                    c.name.clone(),
                    h,
                    ours,
                    at,
                );
            }
        }
        self.members(c, r);
    }

    fn members(&mut self, c: &SClass, r: &RClass) {
        let rms: Vec<&RMember> = r
            .members
            .iter()
            .filter(|m| match &m.kind {
                RKind::Method(x) => !x.implicit,
                RKind::Field { .. } | RKind::Static { .. } => true,
                RKind::Nested { .. } => !m.name.contains("__unnamed"),
                _ => false,
            })
            .collect();
        let oms: Vec<&SMember> = c
            .members
            .iter()
            .filter(|m| match &m.kind {
                SKind::Nested => !m.name.starts_with("__anon"),
                SKind::Friend => false,
                _ => true,
            })
            .collect();
        let mut paired: Vec<(usize, usize)> = Vec::new();
        let mut used_r: BTreeSet<usize> = BTreeSet::new();
        let mut used_o: BTreeSet<usize> = BTreeSet::new();
        for exact in [true, false] {
            for (ri, rm) in rms.iter().enumerate() {
                if used_r.contains(&ri) {
                    continue;
                }
                let found = oms.iter().enumerate().find(|(oi, om)| {
                    !used_o.contains(oi)
                        && rec_side(&c.name, &r.name, &om.name) == rm.name
                        && self.kinds_match(rm, om, &c.name, exact)
                });
                if let Some((oi, _)) = found {
                    paired.push((ri, oi));
                    used_r.insert(ri);
                    used_o.insert(oi);
                }
            }
        }
        if !r.name.contains('<') {
            let plan = self.plan(c, r, &rms, &oms, &paired);
            self.plans.push(plan);
            self.field_renames(c, r, &rms, &oms, &paired);
        }
        let subj = |m: &str| format!("{}::{m}", r.name);
        for (ri, rm) in rms.iter().enumerate() {
            if used_r.contains(&ri) {
                continue;
            }
            let (what, rat) = match &rm.kind {
                RKind::Method(x) => (
                    method_sig(r, &rm.name, x),
                    x.body.as_ref().map(|b| format!("{}:{}", b.base(), b.line)),
                ),
                RKind::Field { off, ty } => (
                    format!("{} at +{off:#x}", ty.keyless().decl(&rm.name)),
                    None,
                ),
                RKind::Static { ty } => (format!("static {}", ty.keyless().decl(&rm.name)), None),
                RKind::Nested { ty } => (format!("nested {}", ty.keyless().spell()), None),
                _ => (rm.name.clone(), None),
            };
            let at = match rat {
                Some(a) => format!("{} (record body {a})", c.loc),
                None => c.loc.to_string(),
            };
            let access = rm.access.map_or("", Access::word);
            self.push(
                "member-missing",
                subj(&rm.name),
                c.name.clone(),
                format!("{access}: {what}"),
                "not declared".into(),
                at,
            );
        }
        for (oi, om) in oms.iter().enumerate() {
            if !used_o.contains(&oi) {
                self.push(
                    "member-extra",
                    format!("{}::{}", r.name, om.name),
                    format!("{}::{}", c.name, om.name),
                    "not in the record".into(),
                    om.name.clone(),
                    om.loc.to_string(),
                );
            }
        }
        for &(ri, oi) in &paired {
            if let (Some(rm), Some(om)) = (rms.get(ri), oms.get(oi)) {
                self.pair(c, r, rm, om);
            }
        }
        let name_of = |i: usize| rms.get(i).map_or("", |m| m.name.as_str());
        let mut method_names: HashMap<&str, usize> = HashMap::new();
        for m in &rms {
            if matches!(m.kind, RKind::Method(_)) {
                *method_names.entry(m.name.as_str()).or_default() += 1;
            }
        }
        let overloaded = |ri: usize| {
            rms.get(ri).is_some_and(|m| {
                matches!(m.kind, RKind::Method(_))
                    && method_names.get(m.name.as_str()).copied().unwrap_or(0) > 1
            })
        };
        let mut units: Vec<(usize, usize, (usize, usize))> = Vec::new();
        let mut groups: BTreeMap<&str, Vec<(usize, usize)>> = BTreeMap::new();
        for &(ri, oi) in &paired {
            if overloaded(ri) {
                groups.entry(name_of(ri)).or_default().push((ri, oi));
            } else {
                units.push((ri, oi, (ri, oi)));
            }
        }
        for g in groups.values() {
            let head = g.iter().map(|(r, _)| *r).min().unwrap_or(0);
            let first = g.iter().min_by_key(|(_, o)| *o).copied().unwrap_or((0, 0));
            units.push((head, first.1, first));
        }
        units.sort_by_key(|u| u.1);
        let seq: Vec<usize> = units.iter().map(|u| u.0).collect();
        let keep = lis(&seq);
        let mut rows: Vec<(usize, usize, String)> = Vec::new();
        for (k, (at, _, (ri, oi))) in units.iter().enumerate() {
            if keep.contains(&k) {
                continue;
            }
            let prev = (0..*at).rev().find(|p| used_r.contains(p));
            rows.push((
                *ri,
                *oi,
                match prev {
                    Some(p) => format!("after {}", name_of(p)),
                    None => "first".into(),
                },
            ));
        }
        for g in groups.values() {
            let mut by_o = g.clone();
            by_o.sort_by_key(|(_, o)| *o);
            let seq: Vec<usize> = by_o.iter().map(|(r, _)| *r).collect();
            let keep = lis(&seq);
            for (k, (ri, oi)) in by_o.iter().enumerate() {
                if keep.contains(&k) {
                    continue;
                }
                let sig = |i: usize| match rms.get(i).map(|m| &m.kind) {
                    Some(RKind::Method(x)) => method_sig(r, name_of(i), x),
                    _ => name_of(i).to_string(),
                };
                let prev = g.iter().map(|(p, _)| *p).filter(|p| p < ri).max();
                rows.push((
                    *ri,
                    *oi,
                    match prev {
                        Some(p) => format!("{} after {} in its overload group", sig(*ri), sig(p)),
                        None => format!("{} first in its overload group", sig(*ri)),
                    },
                ));
            }
        }
        rows.sort_by_key(|(_, o, _)| *o);
        for (ri, oi, says) in rows {
            let Some(om) = oms.get(oi) else { continue };
            self.push(
                "order",
                subj(name_of(ri)),
                format!("{}::{}", c.name, om.name),
                says,
                format!("member {} of ours", oi.saturating_add(1)),
                om.loc.to_string(),
            );
        }
    }

    fn field_renames(
        &mut self,
        c: &SClass,
        r: &RClass,
        rms: &[&RMember],
        oms: &[&SMember],
        paired: &[(usize, usize)],
    ) {
        let rf: Vec<(usize, &RMember)> = rms
            .iter()
            .enumerate()
            .filter(|(_, m)| matches!(m.kind, RKind::Field { .. }))
            .map(|(i, m)| (i, *m))
            .collect();
        let of: Vec<(usize, &SMember)> = oms
            .iter()
            .enumerate()
            .filter(|(_, m)| matches!(m.kind, SKind::Field { .. }))
            .map(|(i, m)| (i, *m))
            .collect();
        if rf.len() != of.len() || oms.iter().any(|m| m.anon.is_some()) {
            return;
        }
        let mut cand = Vec::new();
        for (&(ri, rm), &(oi, om)) in rf.iter().zip(&of) {
            if om.name == rm.name {
                continue;
            }
            let (RKind::Field { ty: rt, .. }, SKind::Field { ty: ot, bits }) = (&rm.kind, &om.kind)
            else {
                return;
            };
            let unpaired = !paired.iter().any(|(a, b)| *a == ri || *b == oi);
            if !unpaired
                || bits.is_some()
                || !same(&rt.keyless(), &self.ours_ty(ot, &c.name).keyless(), true)
            {
                return;
            }
            cand.push((om.name.clone(), rm.name.clone()));
        }
        for (old, new) in cand {
            self.renames.push(FieldRename {
                class: c.name.clone(),
                subject: format!("{}::{new}", r.name),
                file: c.loc.file.clone(),
                old,
                new,
            });
        }
    }

    fn plan(
        &self,
        c: &SClass,
        r: &RClass,
        rms: &[&RMember],
        oms: &[&SMember],
        paired: &[(usize, usize)],
    ) -> ClassPlan {
        let default = default_access(r.key);
        let members = c
            .members
            .iter()
            .map(|m| {
                let pair = oms
                    .iter()
                    .position(|o| std::ptr::eq(*o, m))
                    .and_then(|oi| paired.iter().find(|(_, o)| *o == oi))
                    .map(|(ri, _)| *ri);
                let nested = matches!(m.kind, SKind::Nested);
                let rec_virtual = pair.is_some_and(|ri| {
                    matches!(rms.get(ri).map(|x| &x.kind), Some(RKind::Method(x)) if x.mprop.is_virtual())
                });
                let anchored = |name: &str| {
                    c.members.iter().any(|o| {
                        o.name == name
                            && matches!(&o.kind, SKind::Method(x) if x.virt && !self.overrides(c, &o.name, x))
                    })
                };
                let (ours_virtual, is_override) = match &m.kind {
                    SKind::Method(x) => {
                        let ov = self.overrides(c, &m.name, x);
                        (Some(x.virt || ov), ov)
                    }
                    _ => (None, false),
                };
                let free_override =
                    ours_virtual == Some(true) && is_override && rec_virtual && !anchored(&m.name);
                PlanMember {
                    subject: format!("{}::{}", r.name, rec_side(&c.name, &r.name, &m.name)),
                    ours: format!("{}::{}", c.name, m.name),
                    line: m.loc.line,
                    end: m.end,
                    access: m.access,
                    want: pair
                        .filter(|_| !nested)
                        .and_then(|ri| rms.get(ri))
                        .map(|x| x.access.unwrap_or(default)),
                    rec: pair,
                    movable: pair.is_some()
                        && m.anon.is_none()
                        && ((ours_virtual == Some(false) && !rec_virtual) || free_override),
                    slots: ours_virtual == Some(true) && !free_override,
                    friend: matches!(m.kind, SKind::Friend),
                    nested,
                }
            })
            .collect();
        ClassPlan {
            class: c.name.clone(),
            file: c.loc.file.clone(),
            line: c.loc.line,
            key: c.key,
            members,
        }
    }

    fn kinds_match(&self, rm: &RMember, om: &SMember, scope: &str, exact: bool) -> bool {
        match (&rm.kind, &om.kind) {
            (RKind::Field { .. }, SKind::Field { .. })
            | (RKind::Static { .. }, SKind::Static { .. })
            | (RKind::Nested { .. }, SKind::Nested) => true,
            (RKind::Field { .. }, SKind::Static { .. })
            | (RKind::Static { .. }, SKind::Field { .. }) => !exact,
            (RKind::Method(x), SKind::Method(m)) => {
                if !exact {
                    return true;
                }
                x.params.len() == m.params.len()
                    && x.params.iter().zip(&m.params).all(|(a, b)| {
                        same(
                            &a.keyless(),
                            &self.ours_ty(&b.ty, scope).param_decay().keyless(),
                            x.decorated.is_none(),
                        )
                    })
            }
            _ => false,
        }
    }

    fn pair(&mut self, c: &SClass, r: &RClass, rm: &RMember, om: &SMember) {
        let subj = format!("{}::{}", r.name, rm.name);
        let ours = format!("{}::{}", c.name, om.name);
        let at = om.loc.to_string();
        let racc = rm.access.unwrap_or(default_access(r.key));
        if racc != om.access && !matches!(rm.kind, RKind::Nested { .. }) {
            self.push(
                "access",
                subj.clone(),
                ours.clone(),
                racc.word().into(),
                om.access.word().into(),
                at.clone(),
            );
        }
        match (&rm.kind, &om.kind) {
            (RKind::Field { ty: rt, .. }, SKind::Field { ty, bits }) => {
                let mut ot = self.ours_ty(ty, &c.name);
                if let Some(b) = bits {
                    let w = self
                        .res
                        .constant(&texts(b))
                        .and_then(|x| u8::try_from(x).ok());
                    ot = Ty::Bits {
                        ty: Box::new(ot),
                        width: w.unwrap_or(0),
                    };
                }
                self.cmp_ty(&subj, &ours, rt, &ot, true, &at);
            }
            (RKind::Static { ty: rt }, SKind::Static { ty }) => {
                let ot = self.ours_ty(ty, &c.name);
                self.cmp_ty(&subj, &ours, rt, &ot, true, &at);
            }
            (
                RKind::Field { .. } | RKind::Static { .. },
                SKind::Field { .. } | SKind::Static { .. },
            ) => {
                let rs = matches!(rm.kind, RKind::Static { .. });
                let os = matches!(om.kind, SKind::Static { .. });
                self.push(
                    "static-virtual",
                    subj,
                    ours,
                    word(rs, "static", "non-static").into(),
                    word(os, "static", "non-static").into(),
                    at,
                );
            }
            (RKind::Method(x), SKind::Method(m)) => self.method(c, r, rm, x, om, m),
            _ => {}
        }
    }

    fn cmp_ty(&mut self, subj: &str, ours: &str, rt: &Ty, ot: &Ty, uchar_bool: bool, at: &str) {
        if unresolved(ot) {
            self.unresolved.insert(ot.spell());
        }
        if !same(&rt.keyless(), &ot.keyless(), uchar_bool) {
            self.push(
                "type",
                subj.into(),
                ours.into(),
                rt.keyless().spell(),
                ot.keyless().spell(),
                at.into(),
            );
        }
    }

    fn method(
        &mut self,
        c: &SClass,
        r: &RClass,
        rm: &RMember,
        x: &RMethod,
        om: &SMember,
        m: &SMethod,
    ) {
        let subj = format!("{}::{}", r.name, rm.name);
        let ours = format!("{}::{}", c.name, om.name);
        let at = om.loc.to_string();
        let ub = x.decorated.is_none();
        if let Some(rt) = &m.ret {
            let ot = self.ours_ty(rt, &c.name);
            self.cmp_ty(&format!("{subj} (return)"), &ours, &x.ret, &ot, ub, &at);
        }
        if x.params.len() == m.params.len() {
            for (i, (a, b)) in x.params.iter().zip(&m.params).enumerate() {
                let ot = self.ours_ty(&b.ty, &c.name).param_decay();
                self.cmp_ty(
                    &format!("{subj} (parameter {})", i.saturating_add(1)),
                    &ours,
                    a,
                    &ot,
                    ub,
                    &at,
                );
            }
        } else {
            let rs: Vec<String> = x.params.iter().map(|t| t.keyless().spell()).collect();
            let os: Vec<String> = m
                .params
                .iter()
                .map(|p| self.ours_ty(&p.ty, &c.name).keyless().spell())
                .collect();
            self.push(
                "type",
                format!("{subj} (parameters)"),
                ours.clone(),
                format!("({})", rs.join(", ")),
                format!("({})", os.join(", ")),
                at.clone(),
            );
        }
        if x.varargs != m.varargs {
            self.push(
                "type",
                format!("{subj} (...)"),
                ours.clone(),
                word(x.varargs, "varargs", "no varargs").into(),
                word(m.varargs, "varargs", "no varargs").into(),
                at.clone(),
            );
        }
        let rv = x.mprop.is_virtual();
        let ov = m.virt || self.overrides(c, &om.name, m);
        let rs = x.mprop == MProp::Static;
        let mut q: Vec<(&'static str, &'static str)> = Vec::new();
        if rv != ov {
            q.push((
                word(rv, "virtual", "non-virtual"),
                word(ov, "virtual", "non-virtual"),
            ));
        }
        if rs != m.stat {
            q.push((
                word(rs, "static", "non-static"),
                word(m.stat, "static", "non-static"),
            ));
        }
        if x.mprop.is_pure() != m.pure {
            q.push((
                word(x.mprop.is_pure(), "pure", "not pure"),
                word(m.pure, "pure", "not pure"),
            ));
        }
        if x.konst != m.konst {
            q.push((
                word(x.konst, "const", "non-const"),
                word(m.konst, "const", "non-const"),
            ));
        }
        for (a, b) in q {
            self.push(
                "static-virtual",
                subj.clone(),
                ours.clone(),
                a.into(),
                b.into(),
                at.clone(),
            );
        }
        let body = self.our_body(c, om, m);
        for (i, rn) in x.param_names.iter().enumerate() {
            let formal = rn == "__formal";
            let dn = m.params.get(i).and_then(|p| p.name());
            let in_class = body.as_ref().is_some_and(|b| b.in_class);
            if !in_class && !formal && dn != Some(rn.as_str()) {
                self.push(
                    "param-name",
                    format!("{subj}#{}", i.saturating_add(1)),
                    ours.clone(),
                    rn.clone(),
                    dn.unwrap_or("(unnamed)").into(),
                    at.clone(),
                );
                if let Some(old) = dn {
                    self.set_fix(&om.loc, om.end, old, rn);
                }
            }
            if let Some(b) = &body {
                let bn = b.params.get(i).cloned().flatten();
                let want = (!formal).then(|| rn.clone());
                if bn != want {
                    self.push(
                        "param-name",
                        format!("{subj}#{} (definition)", i.saturating_add(1)),
                        ours.clone(),
                        want.clone().unwrap_or_else(|| "(unnamed)".into()),
                        bn.clone().unwrap_or_else(|| "(unnamed)".into()),
                        b.loc.to_string(),
                    );
                    if let (Some(old), Some(new)) = (bn.as_deref(), want.as_deref()) {
                        self.set_fix(&b.loc, b.end, old, new);
                    }
                }
            }
        }
        let Some(rb) = &x.body else { return };
        let rwhere = format!(
            "{} {}:{}",
            word(rb.in_header(), "inline", "out of line"),
            rb.base(),
            rb.line
        );
        match &body {
            None if !self.owner && !rb.in_header() => {}
            None => {
                self.push("inline-placement", subj, ours, rwhere, "no body".into(), at);
            }
            Some(b) if rb.in_header() != b.in_header => {
                self.push(
                    "inline-placement",
                    subj,
                    ours,
                    rwhere,
                    format!("{} {}", word(b.in_header, "inline", "out of line"), b.loc),
                    b.loc.to_string(),
                );
            }
            Some(b) if rb.in_header() && norm_header(&rb.file) != file_of(&b.loc) => {
                self.push(
                    "header-placement",
                    format!("{subj} (body)"),
                    ours,
                    format!("{}:{}", rb.base(), rb.line),
                    b.loc.to_string(),
                    b.loc.to_string(),
                );
            }
            Some(_) => {}
        }
    }

    fn overrides(&self, c: &SClass, name: &str, m: &SMethod) -> bool {
        self.overrides_depth(c, name, m, 0)
    }

    fn overrides_depth(&self, c: &SClass, name: &str, m: &SMethod, depth: usize) -> bool {
        if depth > crate::source::NEST {
            return false;
        }
        let mut seen = BTreeSet::new();
        let mut stack: Vec<String> = c.bases.iter().map(|b| b.1.clone()).collect();
        let ours: Vec<Ty> = m
            .params
            .iter()
            .map(|p| self.ours_ty(&p.ty, &c.name).param_decay().keyless())
            .collect();
        while let Some(b) = stack.pop() {
            if !seen.insert(b.clone()) {
                continue;
            }
            let Some(bc) = self.src.class(&b) else {
                continue;
            };
            if bc.members.iter().any(|bm| {
                bm.name == name
                    && matches!(&bm.kind, SKind::Method(x) if x.virt
                    && x.konst == m.konst
                    && x.params.len() == ours.len()
                    && x.params.iter().zip(&ours).all(|(p, o)| {
                        same(&self.ours_ty(&p.ty, &bc.name).param_decay().keyless(), o, false)
                    }))
            }) {
                return true;
            }
            stack.extend(bc.bases.iter().map(|x| x.1.clone()));
        }
        if name.starts_with('~') {
            return c.bases.iter().any(|b| {
                self.src.class(&b.1).is_some_and(|bc| {
                    bc.members.iter().any(|bm| {
                        bm.name.starts_with('~') && matches!(&bm.kind, SKind::Method(x) if x.virt)
                    }) || self.overrides_depth(bc, name, m, depth.saturating_add(1))
                })
            });
        }
        false
    }

    fn our_body(&self, c: &SClass, om: &SMember, m: &SMethod) -> Option<OurBody> {
        if m.body.is_some() {
            return Some(OurBody {
                loc: om.loc.clone(),
                end: om.end,
                in_class: true,
                in_header: is_header(&om.loc.file.to_string_lossy()),
                params: m
                    .params
                    .iter()
                    .map(|p| p.name().map(str::to_string))
                    .collect(),
            });
        }
        let full = format!("{}::{}", c.name, om.name);
        let cands: Vec<&SDef> = self
            .src
            .defs
            .iter()
            .filter(|d| d.name == full && d.params.len() == m.params.len())
            .collect();
        let pick = if cands.len() > 1 {
            cands
                .iter()
                .find(|d| {
                    d.params.iter().zip(&m.params).all(|(a, b)| {
                        same(
                            &self.ours_ty(&a.ty, &c.name).param_decay().keyless(),
                            &self.ours_ty(&b.ty, &c.name).param_decay().keyless(),
                            false,
                        )
                    })
                })
                .copied()
        } else {
            cands.first().copied()
        }?;
        Some(OurBody {
            loc: pick.loc.clone(),
            end: pick.end,
            in_class: false,
            in_header: is_header(&pick.loc.file.to_string_lossy()),
            params: pick
                .params
                .iter()
                .map(|p| p.name().map(str::to_string))
                .collect(),
        })
    }

    fn enums(&mut self) {
        let mut done: BTreeSet<String> = BTreeSet::new();
        let src = self.src;
        let rec = self.rec;
        let headers = self.headers.clone();
        for e in &src.enums {
            let ours: BTreeSet<&str> = e.values.iter().map(|(n, _)| n.as_str()).collect();
            if ours.is_empty() || !e.loc.file.starts_with(&headers) {
                continue;
            }
            let shared = |r: &crate::decl::REnum| {
                r.values
                    .iter()
                    .filter(|(n, _)| ours.contains(n.as_str()))
                    .count()
            };
            let by_name = rec.enumeration(&e.name);
            let r = by_name.or_else(|| {
                rec.enums
                    .iter()
                    .filter(|r| rec.scope.contains(&r.name))
                    .max_by_key(|r| shared(r))
                    .filter(|r| shared(r).saturating_mul(2) > ours.len())
            });
            let Some(r) = r else { continue };
            if !done.insert(r.name.clone()) {
                continue;
            }
            let at = e.loc.to_string();
            if r.name != e.name && !e.name.contains("__anon") && !r.name.contains("__unnamed") {
                self.push(
                    "class-name",
                    r.name.clone(),
                    e.name.clone(),
                    r.name.clone(),
                    e.name.clone(),
                    at.clone(),
                );
            }
            for (n, v) in &r.values {
                if !ours.contains(n.as_str()) {
                    self.push(
                        "enumerator",
                        format!("{}::{n}", r.name),
                        e.name.clone(),
                        format!("{n} = {v}"),
                        "missing".into(),
                        at.clone(),
                    );
                } else if let Some(o) = self.res.enums.get(n).copied().filter(|o| o != v) {
                    self.push(
                        "enumerator",
                        format!("{}::{n}", r.name),
                        e.name.clone(),
                        format!("{n} = {v}"),
                        format!("{n} = {o}"),
                        at.clone(),
                    );
                }
            }
            for (n, _) in &e.values {
                if !r.values.iter().any(|(m, _)| m == n) {
                    self.push(
                        "enumerator",
                        format!("{}::{n}", r.name),
                        e.name.clone(),
                        "not in the record".into(),
                        n.clone(),
                        at.clone(),
                    );
                }
            }
        }
    }
}

#[must_use]
pub fn rec_side(our_class: &str, rec_class: &str, name: &str) -> String {
    let (o, r) = (leaf(our_class), leaf(rec_class));
    if name == o {
        r.to_string()
    } else if name.strip_prefix('~') == Some(o) {
        format!("~{r}")
    } else {
        name.to_string()
    }
}

#[must_use]
pub fn lis(seq: &[usize]) -> BTreeSet<usize> {
    if seq.len() > ORDER_UNITS {
        return (0..seq.len()).collect();
    }
    let n = seq.len();
    let mut len = vec![1usize; n];
    let mut prev = vec![usize::MAX; n];
    for (i, si) in seq.iter().enumerate() {
        for (j, sj) in seq.iter().enumerate().take(i) {
            let lj = len.get(j).copied().unwrap_or(1);
            if sj < si && lj.saturating_add(1) > len.get(i).copied().unwrap_or(1) {
                if let Some(l) = len.get_mut(i) {
                    *l = lj.saturating_add(1);
                }
                if let Some(p) = prev.get_mut(i) {
                    *p = j;
                }
            }
        }
    }
    let mut out = BTreeSet::new();
    let Some(mut k) =
        (0..n).max_by_key(|&i| (len.get(i).copied().unwrap_or(0), std::cmp::Reverse(i)))
    else {
        return out;
    };
    loop {
        if !out.insert(k) {
            break;
        }
        match prev.get(k).copied() {
            Some(p) if p != usize::MAX => k = p,
            _ => break,
        }
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Explained {
    pub view: String,
    pub kind: String,
    pub subject: String,
    pub reason: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Reasons {
    pub categories: Vec<String>,
    pub cite: BTreeMap<String, String>,
}

pub fn read_explained(text: &str, shown: &str, rules: &Reasons) -> Result<Vec<Explained>> {
    let mut out = Vec::new();
    for (i, l) in text.lines().enumerate() {
        if l.trim().is_empty() || l.starts_with('#') {
            continue;
        }
        let f: Vec<&str> = l.split('\t').collect();
        let [view, kind, subject, reason] = f.as_slice() else {
            bail!(
                "{shown}:{}: want 4 tab-separated fields, got {}",
                i.saturating_add(1),
                f.len()
            );
        };
        let cat = reason.split(':').next().unwrap_or("");
        if !rules.categories.is_empty() && !rules.categories.iter().any(|c| c == cat) {
            bail!(
                "{shown}:{}: a reason opens {}, not {cat:?}",
                i.saturating_add(1),
                rules
                    .categories
                    .iter()
                    .map(|c| format!("{c}:"))
                    .collect::<Vec<_>>()
                    .join(" / ")
            );
        }
        if let Some(needs) = rules.cite.get(cat)
            && !reason.contains(needs.as_str())
        {
            bail!(
                "{shown}:{}: a {cat}: reason names its {needs} row",
                i.saturating_add(1)
            );
        }
        out.push(Explained {
            view: (*view).to_string(),
            kind: (*kind).to_string(),
            subject: (*subject).to_string(),
            reason: (*reason).to_string(),
        });
    }
    Ok(out)
}

#[must_use]
pub fn explains<'e>(ex: &'e [Explained], view: &str, r: &Row) -> Option<&'e Explained> {
    ex.iter().find(|e| {
        (e.view == "*" || e.view == view)
            && e.kind == r.kind
            && (e.subject == r.subject
                || e.subject
                    .strip_suffix('*')
                    .is_some_and(|p| r.subject.starts_with(p)))
    })
}

#[derive(Debug, Clone, Default)]
pub struct Compared {
    pub rows: Vec<Row>,
    pub unresolved: BTreeSet<String>,
    pub plans: Vec<ClassPlan>,
    pub renames: Vec<FieldRename>,
}

#[must_use]
pub fn compare(rec: &Model, src: &Src, view: &View) -> Compared {
    compare_in(rec, src, &view.headers_rel(), view.owner)
}

#[must_use]
pub fn compare_in(rec: &Model, src: &Src, headers: &Path, owner: bool) -> Compared {
    let ours: Vec<&SClass> = src
        .classes
        .iter()
        .filter(|c| c.loc.file.starts_with(headers) && !c.template)
        .collect();
    let map = class_map(src, rec, &ours);
    let mut d = Diff {
        src,
        rec,
        res: Resolver::new(src, rec),
        map,
        owner,
        headers: headers.to_path_buf(),
        rows: Vec::new(),
        unresolved: BTreeSet::new(),
        plans: Vec::new(),
        renames: Vec::new(),
    };
    d.run(&ours);
    Compared {
        rows: d.rows,
        unresolved: d.unresolved,
        plans: d.plans,
        renames: d.renames,
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Shown {
    #[serde(flatten)]
    pub row: Row,
    pub explained: Option<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Summary {
    pub view: String,
    pub mismatches: usize,
    pub explained: usize,
    pub unexplained: usize,
    pub categories: BTreeMap<String, usize>,
    pub unresolved: Vec<String>,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Print {
    pub all: bool,
    pub counts: bool,
}

#[must_use]
pub fn filter_class(rows: Vec<Row>, class: Option<&str>) -> Vec<Row> {
    rows.into_iter()
        .filter(|r| {
            class.is_none_or(|c| {
                r.subject.split("::").next() == Some(c) || r.ours.split("::").next() == Some(c)
            })
        })
        .collect()
}

#[must_use]
pub fn report(
    view: &str,
    rows: &[Row],
    unresolved: &BTreeSet<String>,
    ex: &[Explained],
    print: Print,
) -> (String, Vec<Shown>, Summary) {
    let mut per: BTreeMap<String, (usize, usize)> = BTreeMap::new();
    let mut by_kind: BTreeMap<&str, (usize, usize)> = BTreeMap::new();
    let mut cats: BTreeMap<String, usize> = BTreeMap::new();
    let mut out = String::new();
    let mut shown = Vec::with_capacity(rows.len());
    let mut unexplained = 0usize;
    for r in rows {
        let e = explains(ex, view, r);
        let cls = r.subject.split("::").next().unwrap_or("").to_string();
        let ent = per.entry(cls).or_default();
        ent.0 += 1;
        let k = by_kind.entry(r.kind).or_default();
        k.0 += 1;
        if let Some(e) = e {
            ent.1 += 1;
            k.1 += 1;
            *cats
                .entry(e.reason.split(':').next().unwrap_or("").to_string())
                .or_default() += 1;
        } else {
            unexplained += 1;
        }
        shown.push(Shown {
            row: r.clone(),
            explained: e.map(|e| e.reason.clone()),
        });
        if print.counts || (e.is_some() && !print.all) {
            continue;
        }
        let _ = writeln!(
            out,
            "{}{:<16} {}{}  record: {}  ours: {}  {}{}",
            if e.is_some() { "EXPLAINED " } else { "" },
            r.kind,
            r.subject,
            if r.ours.is_empty() || r.subject.ends_with(&r.ours) {
                String::new()
            } else {
                format!(" (ours {})", r.ours)
            },
            r.record,
            r.our_says,
            r.at,
            e.map_or(String::new(), |e| format!("  -- {}", e.reason))
        );
    }
    for (c, (n, e)) in &per {
        let _ = writeln!(out, "CLASS {c}: {n} mismatch(es), {e} explained");
    }
    for (k, (n, e)) in &by_kind {
        let _ = writeln!(out, "KIND {k}: {n} mismatch(es), {e} explained");
    }
    if !unresolved.is_empty() {
        let _ = writeln!(
            out,
            "UNRESOLVED (not compared): {} type(s): {}",
            unresolved.len(),
            unresolved
                .iter()
                .take(30)
                .cloned()
                .collect::<Vec<_>>()
                .join(" | ")
        );
    }
    let cat: Vec<String> = cats.iter().map(|(k, v)| format!("{k} {v}")).collect();
    let _ = writeln!(
        out,
        "CV-DIFF {view}: {} mismatch(es), {} explained ({}), {unexplained} UNEXPLAINED",
        rows.len(),
        rows.len().saturating_sub(unexplained),
        if cat.is_empty() {
            "none".into()
        } else {
            cat.join(", ")
        }
    );
    let summary = Summary {
        view: view.to_string(),
        mismatches: rows.len(),
        explained: rows.len().saturating_sub(unexplained),
        unexplained,
        categories: cats,
        unresolved: unresolved.iter().cloned().collect(),
    };
    (out, shown, summary)
}
