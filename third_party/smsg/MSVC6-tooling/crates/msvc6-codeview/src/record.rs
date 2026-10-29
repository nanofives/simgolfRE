use crate::codec::{
    self, Gen, LF_FIELDLIST, LF_INDEX, LF_METHODLIST, i32_at, int_at, name_text, numeric_len,
    u16_at, u32_at, us,
};
use crate::nb::{
    Nb, SST_ALIGN_SYM, SST_GLOBAL_PUB, SST_GLOBAL_SYM, SST_GLOBAL_TYPES, SST_MODULE, SST_SRC_MODULE,
};
use anyhow::{Context as _, Result, bail};
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::ops::Range;

pub const TI_MIN: u32 = 0x1000;
pub const VFTABLE_SLOTS: usize = 512;
pub const THUNK_HOPS: usize = 4;
pub const VFTABLE_SPAN: u32 = 0x800;
pub const VFTABLE_TOTAL: usize = 1 << 20;
pub const LINEAR_SECTIONS: usize = 96;
pub const FIELD_CAP: usize = 1 << 16;
pub const LINE_PAIR_BYTES: usize = 6;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TypeRef<'a> {
    pub leaf: u16,
    pub data: &'a [u8],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Local {
    pub name: String,
    pub ty: u32,
    pub bp: Option<i32>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Proc {
    pub name: String,
    pub rva: u32,
    pub len: u32,
    pub ty: u32,
    pub global: bool,
    pub locals: Vec<Local>,
    pub data: Vec<DataSym>,
    pub module: usize,
}

impl Proc {
    #[must_use]
    pub fn params(&self) -> Vec<&Local> {
        let mut p: Vec<&Local> = self
            .locals
            .iter()
            .filter(|l| l.bp.is_some_and(|o| o >= 8) && l.name != "this")
            .collect();
        p.sort_by_key(|l| l.bp);
        p.dedup_by_key(|l| l.bp);
        p
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DataSym {
    pub name: String,
    pub ty: u32,
    pub rva: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SrcFile {
    pub name: String,
    pub lines: Vec<(u32, u16)>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Module {
    pub index: u16,
    pub ilib: u16,
    pub name: String,
    pub objname: String,
    pub procs: Vec<Proc>,
    pub data: Vec<DataSym>,
    pub udts: Vec<(String, u32)>,
    pub files: Vec<SrcFile>,
    line_index: Vec<(u32, usize, usize, u16)>,
}

#[derive(Debug, Clone, Default)]
pub struct Record {
    pub signature: String,
    pub bytes: usize,
    blob: Vec<u8>,
    types: Vec<(u16, Range<usize>)>,
    pub modules: Vec<Module>,
    pub publics: Vec<DataSym>,
    pub globals: Vec<DataSym>,
    pub udts: Vec<(String, u32)>,
    pub constants: Vec<(String, u32, i64)>,
    pub vftables: BTreeMap<String, Vec<u32>>,
    pub problems: Vec<String>,
}

fn segment_rva(secs: &[u32], seg: u16, off: u32) -> u32 {
    usize::from(seg)
        .checked_sub(1)
        .and_then(|i| secs.get(i))
        .map_or(u32::MAX, |v| v.wrapping_add(off))
}

#[must_use]
pub fn vftables(img: &Image, publics: &[DataSym]) -> BTreeMap<String, Vec<u32>> {
    let mut out = BTreeMap::new();
    let base = img.opt.image_base.get();
    let data = img.bytes();
    let mut raw: Vec<(u32, u32, usize)> = img
        .sections
        .iter()
        .map(|s| (s.rva.get(), s.raw_size.get(), s.raw_off.get()))
        .collect();
    let mut code: Vec<(u32, u32)> = img
        .sections
        .iter()
        .filter(|s| s.flags & 0x2000_0020 != 0)
        .map(|s| (s.rva.get(), s.virt_size.get()))
        .collect();
    let few = img.sections.len() <= LINEAR_SECTIONS;
    if !few {
        raw.sort_by_key(|s| s.0);
        code.sort_by_key(|s| s.0);
    }
    let file_of = |rva: u32| {
        let hit = |s: &(u32, u32, usize)| {
            let d = rva.checked_sub(s.0)?;
            (d < s.1).then(|| s.2.saturating_add(us(d)))
        };
        if few {
            raw.iter().find_map(hit)
        } else {
            raw.get(raw.partition_point(|s| s.0 <= rva).checked_sub(1)?)
                .and_then(hit)
        }
    };
    let is_code = |rva: u32| {
        let hit = |s: &(u32, u32)| rva.checked_sub(s.0).is_some_and(|d| d < s.1);
        if few {
            code.iter().any(hit)
        } else {
            code.partition_point(|s| s.0 <= rva)
                .checked_sub(1)
                .and_then(|k| code.get(k))
                .is_some_and(hit)
        }
    };
    let follow = |mut rva: u32| {
        for _ in 0..THUNK_HOPS {
            let Some(f) = file_of(rva) else { break };
            if data.get(f) != Some(&0xe9) {
                break;
            }
            let Ok(rel) = u32_at(data, f.saturating_add(1)) else {
                break;
            };
            rva = rva.wrapping_add(5).wrapping_add(rel);
        }
        rva
    };
    let mut starts: Vec<u32> = publics.iter().map(|p| p.rva).collect();
    starts.sort_unstable();
    starts.dedup();
    let mut total = 0usize;
    for p in publics.iter().filter(|p| p.name.starts_with("??_7")) {
        let next = starts.partition_point(|s| *s <= p.rva);
        let end = starts
            .get(next)
            .copied()
            .unwrap_or_else(|| p.rva.saturating_add(VFTABLE_SPAN));
        let mut slots = Vec::new();
        let mut at = p.rva;
        while at.saturating_add(4) <= end && slots.len() < VFTABLE_SLOTS && total < VFTABLE_TOTAL {
            let Some(va) = file_of(at).and_then(|f| u32_at(data, f).ok()) else {
                break;
            };
            let rva = va.wrapping_sub(base);
            if !is_code(rva) {
                break;
            }
            slots.push(follow(rva));
            total = total.saturating_add(1);
            at = at.saturating_add(4);
        }
        out.insert(p.name.clone(), slots);
    }
    out
}

impl Record {
    pub fn from_image(img: &Image) -> Result<Self> {
        let nb = crate::nb::record_of(img)?;
        let secs: Vec<u32> = img.sections.iter().map(|s| s.rva.get()).collect();
        let mut rec = Self::parse(nb, &secs)?;
        rec.vftables = vftables(img, &rec.publics);
        Ok(rec)
    }

    pub fn parse(nb: &[u8], secs: &[u32]) -> Result<Self> {
        let nb = Nb::parse(nb)?;
        let rva = |seg: u16, off: u32| segment_rva(secs, seg, off);
        let mut rec = Record {
            signature: nb.signature_text(),
            bytes: nb.bytes.len(),
            ..Record::default()
        };
        let mut mods: BTreeMap<u16, Module> = BTreeMap::new();
        let mut seen_types = false;
        for e in &nb.entries {
            let s = nb.data(e);
            let what = || format!("{} of module {}", crate::nb::sst_name(e.sst), e.imod);
            match e.sst {
                SST_GLOBAL_TYPES => {
                    if seen_types {
                        bail!("the record holds two sstGlobalTypes subsections");
                    }
                    seen_types = true;
                    let (blob, types) = global_types(s).with_context(what)?;
                    rec.blob = blob;
                    rec.types = types;
                }
                SST_MODULE => {
                    let cseg = usize::from(u16_at(s, 4).with_context(what)?);
                    let (name, _) =
                        name_text(s, 8usize.saturating_add(cseg.saturating_mul(12)), Gen::St)
                            .with_context(what)?;
                    let m = mods.entry(e.imod).or_default();
                    m.index = e.imod;
                    m.ilib = u16_at(s, 2).with_context(what)?;
                    m.name = name;
                }
                _ => {}
            }
        }
        for e in &nb.entries {
            let s = nb.data(e);
            let what = || format!("{} of module {}", crate::nb::sst_name(e.sst), e.imod);
            match e.sst {
                SST_ALIGN_SYM => {
                    let m = mods.entry(e.imod).or_default();
                    m.index = e.imod;
                    let syms = symbols(s.get(4..).unwrap_or_default(), &rva).with_context(what)?;
                    absorb_module(m, syms);
                }
                SST_SRC_MODULE => {
                    let m = mods.entry(e.imod).or_default();
                    m.index = e.imod;
                    m.files = src_module(s, &rva).with_context(what)?;
                }
                SST_GLOBAL_SYM | SST_GLOBAL_PUB => {
                    let cbs = us(u32_at(s, 4).with_context(what)?);
                    let body = codec::bytes(s, 16, cbs).with_context(|| {
                        format!(
                            "{}: its {cbs} symbol byte(s) run past the subsection",
                            what()
                        )
                    })?;
                    for sym in symbols(body, &rva).with_context(what)? {
                        match sym {
                            Sym::Data(d) if e.sst == SST_GLOBAL_PUB => rec.publics.push(d),
                            Sym::Data(d) => rec.globals.push(d),
                            Sym::Udt(n, t) => rec.udts.push((n, t)),
                            Sym::Constant(n, t, v) => rec.constants.push((n, t, v)),
                            _ => {}
                        }
                    }
                }
                _ => {}
            }
        }
        rec.modules = mods.into_values().collect();
        for (i, m) in rec.modules.iter_mut().enumerate() {
            for p in &mut m.procs {
                p.module = i;
            }
            let mut idx: Vec<(u32, usize, usize, u16)> = Vec::new();
            let mut order = 0usize;
            for (fi, f) in m.files.iter().enumerate() {
                for &(a, l) in &f.lines {
                    idx.push((a, order, fi, l));
                    order = order.saturating_add(1);
                }
            }
            idx.sort_unstable_by(|x, y| x.0.cmp(&y.0).then(y.1.cmp(&x.1)));
            m.line_index = idx;
        }
        Ok(rec)
    }

    #[must_use]
    pub fn items(&self) -> usize {
        let per_module = self.modules.iter().map(|m| {
            m.procs
                .iter()
                .map(|p| {
                    1usize
                        .saturating_add(p.locals.len())
                        .saturating_add(p.data.len())
                })
                .fold(0, usize::saturating_add)
                .saturating_add(m.data.len())
                .saturating_add(m.udts.len())
                .saturating_add(
                    m.files
                        .iter()
                        .map(|f| f.lines.len())
                        .fold(0, usize::saturating_add),
                )
        });
        per_module
            .fold(self.types.len(), usize::saturating_add)
            .saturating_add(self.publics.len())
            .saturating_add(self.globals.len())
            .saturating_add(self.udts.len())
            .saturating_add(self.constants.len())
    }

    #[must_use]
    pub fn type_count(&self) -> usize {
        self.types.len()
    }

    #[must_use]
    pub fn ty(&self, ti: u32) -> Option<TypeRef<'_>> {
        let i = us(ti.checked_sub(TI_MIN)?);
        let (leaf, r) = self.types.get(i)?;
        Some(TypeRef {
            leaf: *leaf,
            data: self.blob.get(r.clone()).unwrap_or_default(),
        })
    }

    pub fn types(&self) -> impl Iterator<Item = (u32, TypeRef<'_>)> {
        self.types.iter().enumerate().map(|(i, (leaf, r))| {
            (
                TI_MIN.saturating_add(u32::try_from(i).unwrap_or(u32::MAX)),
                TypeRef {
                    leaf: *leaf,
                    data: self.blob.get(r.clone()).unwrap_or_default(),
                },
            )
        })
    }

    pub fn procs(&self) -> impl Iterator<Item = &Proc> {
        self.modules.iter().flat_map(|m| m.procs.iter())
    }

    #[must_use]
    pub fn line_of(&self, module: usize, rva: u32) -> Option<(&str, u16)> {
        let m = self.modules.get(module)?;
        let k = m.line_index.partition_point(|e| e.0 <= rva);
        let &(_, _, fi, l) = m.line_index.get(k.checked_sub(1)?)?;
        Some((m.files.get(fi)?.name.as_str(), l))
    }
}

#[derive(Debug, Clone)]
enum Sym {
    ObjName(String),
    ProcStart(Proc),
    Scope,
    End,
    Local(Local),
    Data(DataSym),
    Udt(String, u32),
    Constant(String, u32, i64),
    Other,
}

fn symbols(b: &[u8], rva: &dyn Fn(u16, u32) -> u32) -> Result<Vec<Sym>> {
    let mut out = Vec::new();
    let mut p = 0usize;
    while p.saturating_add(4) <= b.len() {
        let len = usize::from(u16_at(b, p)?);
        if len < 2 {
            break;
        }
        let typ = u16_at(b, p.saturating_add(2))?;
        let end = p.saturating_add(2).saturating_add(len);
        let d = b.get(p.saturating_add(4)..end).with_context(|| {
            format!(
                "the symbol record at {p:#x} ({}, length {len}) runs past its {} byte(s)",
                codec::sym_name(typ),
                b.len()
            )
        })?;
        let at = || format!("the {} record at {p:#x}", codec::sym_name(typ));
        let sym = match typ {
            0x0009 => Sym::ObjName(name_text(d, 4, Gen::St).with_context(at)?.0),
            0x0006 => Sym::End,
            0x0206..=0x0208 => Sym::Scope,
            0x1001 => Sym::Local(Local {
                ty: u32_at(d, 0).with_context(at)?,
                name: name_text(d, 6, Gen::St).with_context(at)?.0,
                bp: None,
            }),
            0x1006 => Sym::Local(Local {
                bp: Some(i32_at(d, 0).with_context(at)?),
                ty: u32_at(d, 4).with_context(at)?,
                name: name_text(d, 8, Gen::St).with_context(at)?.0,
            }),
            0x1002 => {
                let ty = u32_at(d, 0).with_context(at)?;
                let (v, k) = int_at(d, 4).with_context(at)?;
                Sym::Constant(
                    name_text(d, 4usize.saturating_add(k), Gen::St)
                        .with_context(at)?
                        .0,
                    ty,
                    v,
                )
            }
            0x1003 => Sym::Udt(
                name_text(d, 4, Gen::St).with_context(at)?.0,
                u32_at(d, 0).with_context(at)?,
            ),
            0x1007..=0x1009 => Sym::Data(DataSym {
                ty: u32_at(d, 0).with_context(at)?,
                rva: rva(
                    u16_at(d, 8).with_context(at)?,
                    u32_at(d, 4).with_context(at)?,
                ),
                name: name_text(d, 10, Gen::St).with_context(at)?.0,
            }),
            0x100a | 0x100b => Sym::ProcStart(Proc {
                len: u32_at(d, 12).with_context(at)?,
                ty: u32_at(d, 24).with_context(at)?,
                rva: rva(
                    u16_at(d, 32).with_context(at)?,
                    u32_at(d, 28).with_context(at)?,
                ),
                name: name_text(d, 35, Gen::St).with_context(at)?.0,
                global: typ == 0x100b,
                locals: Vec::new(),
                data: Vec::new(),
                module: 0,
            }),
            _ => Sym::Other,
        };
        out.push(sym);
        p = end;
    }
    Ok(out)
}

fn absorb_module(m: &mut Module, syms: Vec<Sym>) {
    let mut depth = 0usize;
    let mut cur: Option<Proc> = None;
    for s in syms {
        match s {
            Sym::ObjName(n) => m.objname = n,
            Sym::ProcStart(p) => {
                if depth == 0 {
                    cur = Some(p);
                }
                depth = depth.saturating_add(1);
            }
            Sym::Scope => depth = depth.saturating_add(1),
            Sym::End => {
                depth = depth.saturating_sub(1);
                if depth == 0
                    && let Some(p) = cur.take()
                {
                    m.procs.push(p);
                }
            }
            Sym::Local(l) => {
                if let Some(p) = cur.as_mut() {
                    p.locals.push(l);
                }
            }
            Sym::Data(d) => match cur.as_mut() {
                Some(p) => p.data.push(d),
                None => m.data.push(d),
            },
            Sym::Udt(n, t) if cur.is_none() => m.udts.push((n, t)),
            _ => {}
        }
    }
    if let Some(p) = cur.take() {
        m.procs.push(p);
    }
}

type Types = (Vec<u8>, Vec<(u16, Range<usize>)>);

fn global_types(s: &[u8]) -> Result<Types> {
    let n = us(u32_at(s, 4).context("the type count")?);
    let room = s.len().saturating_sub(8) / 4;
    if n > room {
        bail!(
            "{n} types claimed; the subsection's {} byte(s) hold at most {room} offsets",
            s.len()
        );
    }
    let data = 8usize.saturating_add(n.saturating_mul(4));
    let mut out = Vec::with_capacity(n);
    for i in 0..n {
        let off = us(u32_at(s, 8usize.saturating_add(i.saturating_mul(4)))?);
        let at = data.saturating_add(off);
        let ti = TI_MIN.saturating_add(u32::try_from(i).unwrap_or(u32::MAX));
        let (leaf, payload, end) =
            codec::type_record_at(s, at).with_context(|| format!("type {ti:#x}"))?;
        out.push((leaf, end.saturating_sub(payload.len())..end));
    }
    Ok((s.to_vec(), out))
}

fn src_module(s: &[u8], rva: &dyn Fn(u16, u32) -> u32) -> Result<Vec<SrcFile>> {
    let cf = usize::from(u16_at(s, 0)?);
    let mut out = Vec::with_capacity(cf);
    let cap = s.len() / LINE_PAIR_BYTES;
    let mut pairs = 0usize;
    for k in 0..cf {
        let fo = us(u32_at(s, 4usize.saturating_add(k.saturating_mul(4)))?);
        let cs = usize::from(u16_at(s, fo).with_context(|| format!("source file {k}"))?);
        let (name, _) = name_text(
            s,
            fo.saturating_add(4).saturating_add(cs.saturating_mul(12)),
            Gen::St,
        )
        .with_context(|| format!("source file {k}'s name"))?;
        let mut lines = Vec::new();
        for j in 0..cs {
            let lo = us(u32_at(
                s,
                fo.saturating_add(4).saturating_add(j.saturating_mul(4)),
            )?);
            let seg = u16_at(s, lo).with_context(|| format!("{name}: line block {j}"))?;
            let cp = usize::from(u16_at(s, lo.saturating_add(2))?);
            let block = cp.saturating_mul(6);
            codec::bytes(s, lo.saturating_add(4), block).with_context(|| {
                format!("{name}: line block {j} of {cp} pair(s) runs past the subsection")
            })?;
            pairs = pairs.saturating_add(cp);
            if pairs > cap {
                bail!(
                    "{name}: the line blocks name over {cap} pairs, more than the subsection's {} byte(s) hold: they overlap",
                    s.len()
                );
            }
            lines.reserve(cp);
            for q in 0..cp {
                let off = u32_at(s, lo.saturating_add(4).saturating_add(q.saturating_mul(4)))?;
                let line = u16_at(
                    s,
                    lo.saturating_add(4)
                        .saturating_add(cp.saturating_mul(4))
                        .saturating_add(q.saturating_mul(2)),
                )?;
                lines.push((rva(seg, off), line));
            }
        }
        out.push(SrcFile { name, lines });
    }
    Ok(out)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, serde::Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Access {
    Private,
    Protected,
    Public,
}

impl Access {
    #[must_use]
    pub fn of(attr: u16) -> Option<Self> {
        match attr & 3 {
            1 => Some(Self::Private),
            2 => Some(Self::Protected),
            3 => Some(Self::Public),
            _ => None,
        }
    }

    #[must_use]
    pub fn word(self) -> &'static str {
        match self {
            Self::Private => "private",
            Self::Protected => "protected",
            Self::Public => "public",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MProp {
    Vanilla,
    Virtual,
    Static,
    Friend,
    Intro,
    PureVirtual,
    PureIntro,
}

impl MProp {
    #[must_use]
    pub fn of(attr: u16) -> Self {
        match (attr >> 2) & 7 {
            1 => Self::Virtual,
            2 => Self::Static,
            3 => Self::Friend,
            4 => Self::Intro,
            5 => Self::PureVirtual,
            6 => Self::PureIntro,
            _ => Self::Vanilla,
        }
    }

    #[must_use]
    pub fn is_virtual(self) -> bool {
        matches!(
            self,
            Self::Virtual | Self::Intro | Self::PureVirtual | Self::PureIntro
        )
    }

    #[must_use]
    pub fn is_pure(self) -> bool {
        matches!(self, Self::PureVirtual | Self::PureIntro)
    }

    #[must_use]
    pub fn introduces(self) -> bool {
        matches!(self, Self::Intro | Self::PureIntro)
    }
}

pub const ATTR_PSEUDO: u16 = 0x20;
pub const ATTR_COMPGEN: u16 = 0x100;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Field {
    Base {
        attr: u16,
        ty: u32,
        off: i64,
    },
    VBase {
        attr: u16,
        ty: u32,
    },
    Enumerate {
        name: String,
        value: i64,
    },
    Member {
        attr: u16,
        ty: u32,
        off: i64,
        name: String,
    },
    Static {
        attr: u16,
        ty: u32,
        name: String,
    },
    Method {
        name: String,
        overloads: Vec<(u16, u32, Option<u32>)>,
    },
    OneMethod {
        attr: u16,
        ty: u32,
        vbaseoff: Option<u32>,
        name: String,
    },
    Nested {
        ty: u32,
        name: String,
    },
    VfPtr,
    FriendClass {
        ty: u32,
    },
    FriendFn {
        ty: u32,
        name: String,
    },
    Unknown(u16),
}

impl Record {
    #[must_use]
    pub fn fields(&self, fl: u32) -> Vec<Field> {
        let mut out = Vec::new();
        let mut seen = BTreeSet::new();
        let mut next = Some(fl);
        let mut spent = 0usize;
        while let Some(f) = next.take() {
            if spent > FIELD_CAP {
                break;
            }
            if !seen.insert(f) {
                break;
            }
            let Some(r) = self.ty(f).filter(|r| r.leaf == LF_FIELDLIST) else {
                break;
            };
            let d = r.data;
            let mut p = 0usize;
            while p.saturating_add(2) <= d.len() {
                if d.get(p).is_some_and(|b| *b >= codec::LF_PAD0) {
                    p = p.saturating_add(usize::from(d.get(p).map_or(1, |b| b & 0x0f).max(1)));
                    continue;
                }
                match self.field_at(d, p) {
                    Ok((Field::Unknown(LF_INDEX), _, Some(cont))) => {
                        next = Some(cont);
                        break;
                    }
                    Ok((fld, np, _)) => {
                        let stop = matches!(fld, Field::Unknown(_));
                        spent = spent.saturating_add(match &fld {
                            Field::Method { overloads, .. } => overloads.len().max(1),
                            _ => 1,
                        });
                        out.push(fld);
                        if stop || spent > FIELD_CAP {
                            break;
                        }
                        p = np;
                    }
                    Err(_) => break,
                }
            }
        }
        out
    }

    fn field_at(&self, d: &[u8], p: usize) -> Result<(Field, usize, Option<u32>)> {
        let lf = u16_at(d, p)?;
        let a = |k: usize| p.saturating_add(k);
        Ok(match lf {
            0x1400 => {
                let (off, k) = int_at(d, a(8))?;
                (
                    Field::Base {
                        attr: u16_at(d, a(2))?,
                        ty: u32_at(d, a(4))?,
                        off,
                    },
                    a(8).saturating_add(k),
                    None,
                )
            }
            0x1401 | 0x1402 => {
                let k1 = numeric_len(d, a(12))?;
                let k2 = numeric_len(d, a(12).saturating_add(k1))?;
                (
                    Field::VBase {
                        attr: u16_at(d, a(2))?,
                        ty: u32_at(d, a(4))?,
                    },
                    a(12).saturating_add(k1).saturating_add(k2),
                    None,
                )
            }
            0x0403 => {
                let (value, k) = int_at(d, a(4))?;
                let (name, np) = name_text(d, a(4).saturating_add(k), Gen::St)?;
                (Field::Enumerate { name, value }, np, None)
            }
            LF_INDEX => (Field::Unknown(LF_INDEX), a(8), Some(u32_at(d, a(4))?)),
            0x1405 => {
                let (off, k) = int_at(d, a(8))?;
                let (name, np) = name_text(d, a(8).saturating_add(k), Gen::St)?;
                (
                    Field::Member {
                        attr: u16_at(d, a(2))?,
                        ty: u32_at(d, a(4))?,
                        off,
                        name,
                    },
                    np,
                    None,
                )
            }
            0x1406 => {
                let (name, np) = name_text(d, a(8), Gen::St)?;
                (
                    Field::Static {
                        attr: u16_at(d, a(2))?,
                        ty: u32_at(d, a(4))?,
                        name,
                    },
                    np,
                    None,
                )
            }
            0x1407 => {
                let ml = u32_at(d, a(4))?;
                let (name, np) = name_text(d, a(8), Gen::St)?;
                (
                    Field::Method {
                        name,
                        overloads: self.method_list(ml),
                    },
                    np,
                    None,
                )
            }
            0x1408 => {
                let (name, np) = name_text(d, a(8), Gen::St)?;
                (
                    Field::Nested {
                        ty: u32_at(d, a(4))?,
                        name,
                    },
                    np,
                    None,
                )
            }
            0x1409 => (Field::VfPtr, a(8), None),
            0x140a => (
                Field::FriendClass {
                    ty: u32_at(d, a(4))?,
                },
                a(8),
                None,
            ),
            0x1403 => {
                let (name, np) = name_text(d, a(8), Gen::St)?;
                (
                    Field::FriendFn {
                        ty: u32_at(d, a(4))?,
                        name,
                    },
                    np,
                    None,
                )
            }
            0x140b => {
                let attr = u16_at(d, a(2))?;
                let ty = u32_at(d, a(4))?;
                let mut n = a(8);
                let vbaseoff = if MProp::of(attr).introduces() {
                    n = n.saturating_add(4);
                    Some(u32_at(d, a(8))?)
                } else {
                    None
                };
                let (name, np) = name_text(d, n, Gen::St)?;
                (
                    Field::OneMethod {
                        attr,
                        ty,
                        vbaseoff,
                        name,
                    },
                    np,
                    None,
                )
            }
            other => (Field::Unknown(other), d.len(), None),
        })
    }

    #[must_use]
    pub fn method_list(&self, ml: u32) -> Vec<(u16, u32, Option<u32>)> {
        let Some(r) = self.ty(ml).filter(|r| r.leaf == LF_METHODLIST) else {
            return Vec::new();
        };
        let d = r.data;
        let mut out = Vec::new();
        let mut q = 0usize;
        while q.saturating_add(8) <= d.len() {
            let (Ok(attr), Ok(ix)) = (u16_at(d, q), u32_at(d, q.saturating_add(4))) else {
                break;
            };
            q = q.saturating_add(8);
            let vb = if MProp::of(attr).introduces() {
                let v = u32_at(d, q).ok();
                q = q.saturating_add(4);
                v
            } else {
                None
            };
            out.push((attr, ix, vb));
        }
        out
    }
}
