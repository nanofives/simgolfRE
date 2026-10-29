use crate::dbi::{DBG_FPO, DBG_SECTION_HEADER, Dbi as RawDbi, SC_SIZE};
use crate::linedir::{ByObject, LineDirectives, object_key};
use crate::msf::Msf2;
use crate::pair::{self, Anchor, Derived};
use crate::paths::Policy;
use crate::report::{Report, Row};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::io::Write;
use std::path::Path;

pub const DBI_AGE_AT: usize = 8;
pub const FPO_SIZE: usize = 16;
pub const SEGMAP_SIZE: usize = 20;
pub const TS_SIG: usize = 8;
pub const TS_AGE: usize = 12;
pub const S_END: u16 = 0x0006;

#[derive(Debug, Clone)]
pub struct Sec {
    pub name: String,
    pub rva: u32,
    pub len: u32,
    pub vsize: u32,
}

#[must_use]
pub fn sections(img: &Image) -> Vec<Sec> {
    img.sections
        .iter()
        .map(|s| Sec {
            name: s.name.clone(),
            rva: s.rva.get(),
            len: s.virt_size.get().max(s.raw_size.get()),
            vsize: s.virt_size.get(),
        })
        .collect()
}

#[derive(Debug, Clone)]
struct Candidate {
    rva: u32,
    len: u32,
    obj: String,
}

impl Candidate {
    fn is_in(&self, module: &str) -> bool {
        let base = module.rsplit(['\\', '/']).next().unwrap_or(module);
        !self.obj.is_empty() && base.eq_ignore_ascii_case(&self.obj)
    }
}

#[derive(Debug, Default, Clone)]
pub struct Pairing {
    pub by_ours: BTreeMap<u32, (u32, u32)>,
    unplaced: BTreeMap<(String, String), (u32, u32)>,
    ambiguous: BTreeMap<u32, Vec<Candidate>>,
    pub library: BTreeSet<u32>,
}

impl Pairing {
    pub fn of_report(r: &Report, base: u32, what: &str) -> Result<Self> {
        let p = Self::from_rows(&r.rows, base);
        if p.by_ours.is_empty() && p.ambiguous.is_empty() {
            bail!("{what} paired no address at all");
        }
        Ok(p)
    }

    #[must_use]
    pub fn from_rows(rows: &[Row], base: u32) -> Self {
        let mut claims: BTreeMap<u32, Vec<Candidate>> = BTreeMap::new();
        let mut unplaced: BTreeMap<(String, String), (u32, u32)> = BTreeMap::new();
        let mut twice: BTreeSet<(String, String)> = BTreeSet::new();
        let mut library = BTreeSet::new();
        for r in rows {
            let (Some(rr), Some(size)) = (r.rva(base), r.size32()) else {
                continue;
            };
            let Some(ro) = r.our_rva(base) else {
                let key = (r.obj.to_ascii_lowercase(), r.name.clone());
                if !key.0.is_empty()
                    && !key.1.is_empty()
                    && !twice.contains(&key)
                    && unplaced
                        .insert(key.clone(), (rr, size))
                        .is_some_and(|had| had.0 != rr)
                {
                    unplaced.remove(&key);
                    twice.insert(key);
                }
                continue;
            };
            if r.library {
                library.insert(ro);
            }
            claims.entry(ro).or_default().push(Candidate {
                rva: rr,
                len: size,
                obj: r.obj.clone(),
            });
        }
        let mut p = Self {
            unplaced,
            library,
            ..Self::default()
        };
        for (ro, cands) in claims {
            let first = cands.first().map(|c| c.rva);
            if cands.iter().all(|c| Some(c.rva) == first) {
                if let Some(c) = cands.last() {
                    p.by_ours.insert(ro, (c.rva, c.len));
                }
            } else {
                p.ambiguous.insert(ro, cands);
            }
        }
        p
    }

    #[must_use]
    pub fn at(&self, ours: u32, module: Option<&str>) -> Option<u32> {
        self.run(ours, module).map(|(r, _)| r)
    }

    #[must_use]
    pub fn run(&self, ours: u32, module: Option<&str>) -> Option<(u32, u32)> {
        if let Some((r, len)) = self.by_ours.get(&ours) {
            return Some((*r, *len));
        }
        let near = self.by_ours.range(..=ours).next_back();
        let near_ambiguous = self.ambiguous.range(..=ours).next_back();
        let (start, rstart, len) = match (near, near_ambiguous) {
            (Some((s, _)), Some((a, cands))) if a > s => Self::settle(*a, cands, module)?,
            (None, Some((a, cands))) => Self::settle(*a, cands, module)?,
            (Some((s, (r, l))), _) => (*s, *r, *l),
            (None, None) => return None,
        };
        let delta = ours.checked_sub(start)?;
        if delta == 0 {
            Some((rstart, len))
        } else if delta < len {
            Some((rstart.checked_add(delta)?, len - delta))
        } else {
            None
        }
    }

    fn settle(start: u32, cands: &[Candidate], module: Option<&str>) -> Option<(u32, u32, u32)> {
        let m = module?;
        let mut hits = cands.iter().filter(|c| c.is_in(m));
        let c = hits.next()?;
        if hits.next().is_some() {
            return None;
        }
        Some((start, c.rva, c.len))
    }

    #[must_use]
    pub fn ambiguity(&self) -> (usize, usize) {
        (
            self.ambiguous.len(),
            self.ambiguous.values().map(Vec::len).sum(),
        )
    }

    pub fn widen(&mut self, extents: &[(u32, u32)]) -> usize {
        let mut n = 0;
        for (rva, len) in extents {
            if let Some((_, size)) = self.by_ours.get_mut(rva)
                && *len > *size
            {
                *size = *len;
                n += 1;
            }
        }
        n
    }

    pub fn graft(
        &mut self,
        records: &[(String, String, u32, u32)],
        verify: &dyn Fn(u32, u32, u32) -> bool,
    ) -> (usize, usize) {
        let (mut n, mut rejected) = (0usize, 0usize);
        for (obj, name, ours, len) in records {
            if self.by_ours.contains_key(ours) {
                continue;
            }
            let member = obj
                .rsplit(['\\', '/'])
                .next()
                .unwrap_or(obj)
                .to_ascii_lowercase();
            if let Some((r, size)) = self.unplaced.get(&(member, name.clone())).copied() {
                if *len == size && verify(*ours, r, *len) {
                    self.by_ours.insert(*ours, (r, *len));
                    n += 1;
                } else {
                    rejected += 1;
                }
            }
        }
        (n, rejected)
    }
}

#[must_use]
pub fn name_at(kind: u16) -> Option<usize> {
    match kind {
        0x1007 | 0x1008 | 0x1009 | 0x100e | 0x100f => Some(10),
        0x100a | 0x100b => Some(35),
        0x0206 | 0x1102 => Some(21),
        0x0204 | 0x0205 => Some(33),
        _ => None,
    }
}

#[must_use]
pub fn st_name(bytes: &[u8], at: usize) -> Option<String> {
    let n = usize::from(*bytes.get(at)?);
    let s = bytes.get(at + 1..at + 1 + n)?;
    Some(s.iter().map(|c| char::from(*c)).collect())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Role {
    Flat,
    Proc,
    Child,
}

type Field = (Role, usize, Option<(usize, usize)>);

fn field_of(kind: u16) -> Option<Field> {
    match kind {
        0x1007 | 0x1008 | 0x100e | 0x100f | 0x1009 | 0x020b => Some((Role::Flat, 4, None)),
        0x100c => Some((Role::Flat, 8, None)),
        0x0005 | 0x020a | 0x1019 | 0x0201 | 0x0202 | 0x0203 | 0x020d | 0x020e => {
            Some((Role::Flat, 0, None))
        }
        0x100a | 0x100b => Some((Role::Proc, 28, Some((12, 4)))),
        0x0204 | 0x0205 => Some((Role::Proc, 24, Some((12, 4)))),
        0x0206 => Some((Role::Proc, 12, Some((18, 2)))),
        0x0207 | 0x0208 => Some((Role::Child, 12, Some((8, 4)))),
        0x0209 => Some((Role::Child, 0, None)),
        _ => None,
    }
}

#[must_use]
pub fn address_at(kind: u16) -> Option<usize> {
    field_of(kind).map(|(_, at, _)| at)
}

type Scope = Option<(u16, u32, u32, u16, u32)>;

#[must_use]
pub fn opens_scope(kind: u16) -> bool {
    matches!(
        kind,
        0x100a | 0x100b | 0x0204 | 0x0205 | 0x0206 | 0x0207 | 0x0208
    )
}

#[must_use]
pub fn records(bytes: &[u8]) -> Vec<(usize, u16, usize, usize)> {
    let mut out = Vec::new();
    let mut at = 0usize;
    while at + 4 <= bytes.len() {
        let Some(len) = u16_at(bytes, at) else { break };
        let len = usize::from(len);
        if len < 2 || at + 2 + len > bytes.len() {
            break;
        }
        let Some(kind) = u16_at(bytes, at + 2) else {
            break;
        };
        out.push((at, kind, at + 4, len - 2));
        at = (record_end(bytes, at, kind, len) + 3) & !3usize;
    }
    out
}

#[must_use]
pub fn record_end(bytes: &[u8], at: usize, kind: u16, reclen: usize) -> usize {
    if matches!(kind, 0x0400 | 0x0401 | 0x0403) && reclen == 14 {
        at + 17 + usize::from(bytes.get(at + 16).copied().unwrap_or(0))
    } else {
        at + 2 + reclen
    }
}

#[must_use]
pub fn u16_at(d: &[u8], at: usize) -> Option<u16> {
    let b = d.get(at..at.checked_add(2)?)?;
    Some(u16::from_le_bytes([*b.first()?, *b.get(1)?]))
}

#[must_use]
pub fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    let b = d.get(at..at.checked_add(4)?)?;
    Some(u32::from_le_bytes([
        *b.first()?,
        *b.get(1)?,
        *b.get(2)?,
        *b.get(3)?,
    ]))
}

fn proc_extents(region: &[u8]) -> Vec<(u16, u32, u32, String)> {
    records(region)
        .into_iter()
        .filter(|(_, kind, _, len)| matches!(kind, 0x100a | 0x100b) && *len >= 36)
        .filter_map(|(_, _, p, _)| {
            let n = usize::from(*region.get(p + 35)?);
            Some((
                u16_at(region, p + 32)?,
                u32_at(region, p + 28)?,
                u32_at(region, p + 12)?,
                region
                    .get(p + 36..p + 36 + n)?
                    .iter()
                    .map(|b| char::from(*b))
                    .collect(),
            ))
        })
        .collect()
}

pub fn put_u16(d: &mut [u8], at: usize, v: u16) -> Result<()> {
    let s = d
        .get_mut(at..at.checked_add(2).context("overflow")?)
        .context("write past the stream")?;
    s.copy_from_slice(&v.to_le_bytes());
    Ok(())
}

pub fn put_u32(d: &mut [u8], at: usize, v: u32) -> Result<()> {
    let s = d
        .get_mut(at..at.checked_add(4).context("overflow")?)
        .context("write past the stream")?;
    s.copy_from_slice(&v.to_le_bytes());
    Ok(())
}

#[derive(Debug, Clone)]
pub struct Modi {
    pub stream: u16,
    pub sym_len: usize,
    pub line_len: usize,
    pub name: String,
    pub obj: String,
    pub at: usize,
    pub sc_at: usize,
}

#[derive(Debug, Clone)]
pub struct DbiView {
    pub version: u32,
    pub age: u32,
    pub globals: u16,
    pub publics: u16,
    pub symrecs: u16,
    pub segmap: (usize, usize),
    pub contribs: (usize, usize),
    pub file_info: (usize, usize),
    pub tsm: (usize, usize),
    pub ec: (usize, usize),
    pub dbg_streams: Vec<u16>,
    pub mods: Vec<Modi>,
}

impl DbiView {
    pub fn parse(s: &[u8]) -> Result<Self> {
        let d = RawDbi::parse(s)?;
        let mut mods = Vec::with_capacity(d.mods.len());
        for m in &d.mods {
            mods.push(Modi {
                stream: m.stream()?,
                sym_len: m.sym_len()?,
                line_len: m.c11_len()?,
                name: m.name_text(),
                obj: m.obj_text(),
                at: m.at,
                sc_at: m.at + 4,
            });
        }
        Ok(Self {
            version: d.version()?,
            age: d.age()?,
            globals: d.globals()?,
            publics: d.publics()?,
            symrecs: d.symrecs()?,
            segmap: (d.offsets.segmap, d.segmap.len()),
            contribs: (d.offsets.sc, d.sc.len()),
            file_info: (d.offsets.files, d.files.len()),
            tsm: (d.offsets.tsm, d.tsm.len()),
            ec: (d.offsets.ec, d.ec.len()),
            dbg_streams: d.dbg.clone(),
            mods,
        })
    }
}

pub fn type_servers(
    dbi: &[u8],
    (at, len): (usize, usize),
) -> Result<Vec<crate::buildname::TypeServer>> {
    let map = at
        .checked_add(len)
        .and_then(|end| dbi.get(at..end))
        .context("the type-server map runs past the DBI stream")?;
    let mut out = crate::buildname::type_servers(map)?;
    for s in &mut out {
        s.at += at;
    }
    Ok(out)
}

pub fn rename_type_servers(
    d: &mut [u8],
    tsm: (usize, usize),
    ours: u32,
    sig: u32,
    age: u32,
) -> Result<Vec<crate::buildname::TypeServer>> {
    let servers = type_servers(d, tsm)?;
    for s in &servers {
        let (ns, na) = if s.sig == ours { (sig, age) } else { (0, 0) };
        put_u32(d, s.at + TS_SIG, ns)?;
        put_u32(d, s.at + TS_AGE, na)?;
    }
    Ok(servers)
}

#[derive(Debug, Default, Clone)]
pub struct Score {
    pub patched: usize,
    pub records: usize,
    pub walked: usize,
    pub region_bytes: usize,
    pub retired: usize,
    pub kept: usize,
    pub kinds: BTreeMap<u16, usize>,
    pub retired_kinds: BTreeMap<u16, usize>,
    pub retired_in: BTreeMap<String, usize>,
    pub patched_in: BTreeMap<String, usize>,
    pub retired_list: Vec<(u16, String, String, u32)>,
    pub lines: usize,
    pub lines_left: usize,
    pub lines_physical: usize,
    pub lines_presumed: usize,
    pub ranges: usize,
    pub ranges_left: usize,
    pub blocks_dropped: usize,
    pub blocks_resorted: usize,
}

#[derive(Clone, Copy)]
pub struct Reloc<'a> {
    pub ours: &'a [Sec],
    pub reference: &'a [Sec],
    pub pair: &'a Pairing,
    pub module: Option<&'a str>,
    pub dirs: Option<&'a ByObject>,
}

impl std::fmt::Debug for Reloc<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Reloc")
            .field("module", &self.module)
            .finish_non_exhaustive()
    }
}

#[must_use]
pub fn c11_file_name(bytes: &[u8], at: usize) -> Option<String> {
    let n = usize::from(*bytes.get(at)?);
    Some(
        bytes
            .get(at + 1..at + 1 + n)?
            .iter()
            .map(|c| char::from(*c))
            .collect(),
    )
}

impl Reloc<'_> {
    fn translate(&self, seg: u16, off: u32) -> Option<(u16, u32)> {
        let idx = usize::from(seg).checked_sub(1)?;
        let s = self.ours.get(idx)?;
        let ours = s.rva.checked_add(off)?;
        let r = self.pair.at(ours, self.module)?;
        for (i, sec) in self.reference.iter().enumerate() {
            if r >= sec.rva && r < sec.rva.checked_add(sec.len)? {
                let n = u16::try_from(i.checked_add(1)?).ok()?;
                return Some((n, r.checked_sub(sec.rva)?));
            }
        }
        None
    }

    fn span(&self, seg: u16, off: u32, len: u32) -> Option<(u16, u32, u32)> {
        let s = self.ours.get(usize::from(seg).checked_sub(1)?)?;
        let (r, run) = self.pair.run(s.rva.checked_add(off)?, self.module)?;
        for (i, sec) in self.reference.iter().enumerate() {
            if r >= sec.rva && r < sec.rva.checked_add(sec.len)? {
                let n = u16::try_from(i.checked_add(1)?).ok()?;
                let room = sec.rva.checked_add(sec.len)?.checked_sub(r)?;
                return Some((n, r.checked_sub(sec.rva)?, len.min(run).min(room)));
            }
        }
        None
    }

    pub fn region(&self, bytes: &mut [u8], score: &mut Score) -> Result<()> {
        let all = records(bytes);
        score.records += all.len();
        score.walked += all.last().map_or(0, |(at, kind, _, len)| {
            record_end(bytes, *at, *kind, len + 2)
        });
        score.region_bytes += bytes.len();
        let mut stack: Vec<Scope> = Vec::new();
        for (_, kind, payload, len) in all {
            if kind == S_END {
                stack.pop();
                continue;
            }
            let field = field_of(kind).filter(|(_, at, _)| at + 6 <= len);
            let Some((role, field_at, len_field)) = field else {
                if opens_scope(kind) {
                    stack.push(None);
                }
                continue;
            };
            let at = payload + field_at;
            let span = match len_field {
                Some((p, 4)) => u32_at(bytes, payload + p).unwrap_or(0),
                Some((p, _)) => u32::from(u16_at(bytes, payload + p).unwrap_or(0)),
                None => 0,
            };
            let name = name_at(kind)
                .or(match kind {
                    0x0209 => Some(7),
                    0x0207 => Some(18),
                    _ => None,
                })
                .and_then(|n| st_name(bytes, payload + n))
                .unwrap_or_default();
            let off = u32_at(bytes, at).context("record truncated")?;
            let seg = u16_at(bytes, at + 4).context("record truncated")?;
            if seg == 0 {
                score.kept += 1;
                if opens_scope(kind) {
                    stack.push(None);
                }
                continue;
            }
            if usize::from(seg) == self.ours.len() + 1 {
                let abs = u16::try_from(self.reference.len() + 1).context("too many sections")?;
                put_u16(bytes, at + 4, abs)?;
                score.kept += 1;
                if opens_scope(kind) {
                    stack.push(None);
                }
                continue;
            }
            let sec = self
                .ours
                .get(usize::from(seg).saturating_sub(1))
                .map_or_else(|| format!("segment {seg}"), |s| s.name.clone());
            let moved = match role {
                Role::Child => match stack.last().copied().flatten() {
                    Some((ps, po, pl, ns, no)) if ps == seg && off >= po && off - po <= pl => {
                        Some((ns, no + (off - po)))
                    }
                    _ => None,
                },
                Role::Flat | Role::Proc => self.translate(seg, off),
            };
            match moved {
                Some((ns, no)) => {
                    put_u32(bytes, at, no)?;
                    put_u16(bytes, at + 4, ns)?;
                    score.patched += 1;
                    *score.kinds.entry(kind).or_default() += 1;
                    *score.patched_in.entry(sec).or_default() += 1;
                    if opens_scope(kind) {
                        stack.push(Some((seg, off, span, ns, no)));
                    }
                }
                None => {
                    put_u16(bytes, at + 4, 0)?;
                    put_u32(bytes, at, 0)?;
                    score.retired += 1;
                    *score.retired_kinds.entry(kind).or_default() += 1;
                    let rva = self
                        .ours
                        .get(usize::from(seg).saturating_sub(1))
                        .map_or(off, |s| s.rva.wrapping_add(off));
                    score.retired_list.push((kind, name, sec.clone(), rva));
                    *score.retired_in.entry(sec).or_default() += 1;
                    if opens_scope(kind) {
                        stack.push(None);
                    }
                }
            }
        }
        Ok(())
    }

    fn starts(&self, bytes: &[u8], out: &mut BTreeMap<u32, Anchor>) {
        for (_, kind, payload, len) in records(bytes) {
            let (Some(at), Some(nat)) = (address_at(kind), name_at(kind)) else {
                continue;
            };
            if at + 6 > len {
                continue;
            }
            let (Some(off), Some(seg)) =
                (u32_at(bytes, payload + at), u16_at(bytes, payload + at + 4))
            else {
                continue;
            };
            let Some(s) = usize::from(seg)
                .checked_sub(1)
                .and_then(|i| self.ours.get(i))
            else {
                continue;
            };
            let Some(rva) = s.rva.checked_add(off) else {
                continue;
            };
            let e = out.entry(rva).or_default();
            e.data |= matches!(kind, 0x1007 | 0x1008 | 0x100e | 0x100f);
            if let Some(n) = st_name(bytes, payload + nat)
                && !e.names.contains(&n)
            {
                e.names.push(n);
            }
        }
    }

    pub fn c11(
        &self,
        bytes: &mut [u8],
        procs: &[(u16, u32, u32)],
        score: &mut Score,
    ) -> Result<()> {
        if bytes.len() < 4 {
            return Ok(());
        }
        let c_file = usize::from(u16_at(bytes, 0).context("C11 header truncated")?);
        let c_seg = usize::from(u16_at(bytes, 2).context("C11 header truncated")?);
        if c_file.saturating_mul(4) > bytes.len() {
            bail!(
                "C11 file table of {c_file} entries runs past its {} bytes",
                bytes.len()
            );
        }
        let mut bases = Vec::with_capacity(c_file);
        for i in 0..c_file {
            bases.push(usize::try_from(
                u32_at(bytes, 4 + 4 * i).context("C11 file table truncated")?,
            )?);
        }
        let se_at = 4 + 4 * c_file;
        let seg_at = se_at + 8 * c_seg;
        let mut cover: BTreeMap<u16, (u16, u32, u32)> = BTreeMap::new();
        let dirs = self
            .dirs
            .zip(self.module)
            .and_then(|(d, m)| d.get(&object_key(m)));
        for base in bases {
            let cc = usize::from(u16_at(bytes, base).context("C11 file record truncated")?);
            if cc.saturating_mul(12) > bytes.len() {
                bail!("C11 file record at {base:#x} names {cc} blocks, more than its region holds");
            }
            let mut blocks = Vec::with_capacity(cc);
            for j in 0..cc {
                blocks.push(usize::try_from(
                    u32_at(bytes, base + 4 + 4 * j).context("C11 line table truncated")?,
                )?);
            }
            let fse = base + 4 + 4 * cc;
            let name = c11_file_name(bytes, fse + 8 * cc).context("C11 file name truncated")?;
            let alias = dirs.filter(|d| d.names(&name)).map(|d| (d, name.as_str()));
            for (j, at) in blocks.iter().enumerate() {
                let range_at = fse + 8 * j;
                match self.c11_block(bytes, *at, range_at, procs, alias, score)? {
                    Some((seg, ns, start, end)) => {
                        let c = cover.entry(seg).or_insert((ns, start, end));
                        c.1 = c.1.min(start);
                        c.2 = c.2.max(end);
                        score.ranges += 1;
                    }
                    None => score.ranges_left += 1,
                }
            }
        }
        for i in 0..c_seg {
            let seg = u16_at(bytes, seg_at + 2 * i).context("C11 segment table truncated")?;
            let (ns, start, end) = cover.get(&seg).copied().unwrap_or((seg, 0, 0));
            put_u32(bytes, se_at + 8 * i, start)?;
            put_u32(bytes, se_at + 8 * i + 4, end)?;
            put_u16(bytes, seg_at + 2 * i, ns)?;
            if cover.contains_key(&seg) {
                score.ranges += 1;
            } else {
                score.ranges_left += 1;
            }
        }
        Ok(())
    }

    fn c11_block(
        &self,
        bytes: &mut [u8],
        at: usize,
        range_at: usize,
        procs: &[(u16, u32, u32)],
        alias: Option<(&LineDirectives, &str)>,
        score: &mut Score,
    ) -> Result<Option<(u16, u16, u32, u32)>> {
        let seg = u16_at(bytes, at).context("C11 block truncated")?;
        let pairs = usize::from(u16_at(bytes, at + 2).context("C11 block truncated")?);
        let old_end = u32_at(bytes, range_at + 4).context("C11 range truncated")?;
        if pairs.saturating_mul(6) > bytes.len() {
            bail!("C11 block at {at:#x} holds {pairs} pairs, more than its region does");
        }
        let mut kept: Vec<(u32, u16, u32)> = Vec::with_capacity(pairs);
        let mut newseg: Option<u16> = None;
        let mut old_max = 0u32;
        for k in 0..pairs {
            let off = u32_at(bytes, at + 4 + 4 * k).context("C11 pair truncated")?;
            let line = u16_at(bytes, at + 4 + 4 * pairs + 2 * k).context("C11 line truncated")?;
            old_max = old_max.max(off);
            match self.translate(seg, off) {
                Some((ns, no)) if newseg.is_none_or(|s| s == ns) => {
                    newseg = Some(ns);
                    kept.push((no, line, off));
                }
                _ => score.lines_left += 1,
            }
        }
        if !kept
            .windows(2)
            .all(|w| w.first().map(|a| a.0) <= w.last().map(|b| b.0))
        {
            score.blocks_resorted += 1;
        }
        kept.sort_by_key(|p| p.0);
        let n = kept.len();
        put_u16(bytes, at + 2, u16::try_from(n)?)?;
        for (i, (no, _, _)) in kept.iter().enumerate() {
            put_u32(bytes, at + 4 + 4 * i, *no)?;
        }
        for (i, (_, line, _)) in kept.iter().enumerate() {
            let line = match alias {
                Some((d, name)) => {
                    if let Some(p) = d.physical(name, *line) {
                        score.lines_physical += 1;
                        p
                    } else {
                        score.lines_presumed += 1;
                        *line
                    }
                }
                None => *line,
            };
            put_u16(bytes, at + 4 + 4 * n + 2 * i, line)?;
        }
        bytes
            .get_mut(at + 4 + 6 * n..at + 4 + 6 * pairs)
            .context("C11 block truncated")?
            .fill(0);
        score.lines += n;
        let (Some(ns), Some(first), Some(last)) = (newseg, kept.first(), kept.last()) else {
            if pairs > 0 {
                score.blocks_dropped += 1;
            }
            put_u32(bytes, range_at, 0)?;
            put_u32(bytes, range_at + 4, 0)?;
            return Ok(None);
        };
        put_u16(bytes, at, ns)?;
        let tail = procs
            .iter()
            .find(|(s, o, l)| *s == seg && last.2 >= *o && last.2 - *o < *l)
            .map_or_else(
                || old_end.saturating_sub(old_max),
                |(_, o, l)| {
                    o.saturating_add(*l)
                        .saturating_sub(1)
                        .saturating_sub(last.2)
                },
            );
        let end = last.0.saturating_add(tail);
        put_u32(bytes, range_at, first.0)?;
        put_u32(bytes, range_at + 4, end)?;
        Ok(Some((seg, ns, first.0, end)))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MapRow {
    pub rva: u32,
    pub name: String,
    pub stem: String,
}

#[derive(Debug, Clone, Copy)]
pub struct Inputs<'a> {
    pub name: &'a str,
    pub reference: &'a Image,
    pub ours: &'a Image,
    pub report: &'a Report,
    pub report_label: &'a str,
    pub map: Option<(&'a [MapRow], &'a str)>,
    pub names: &'a BTreeMap<String, u32>,
    pub scoped: &'a BTreeMap<String, BTreeMap<String, u32>>,
    pub dropped: &'a BTreeMap<String, String>,
    pub dirs: &'a ByObject,
    pub policy: Option<&'a Policy>,
    pub pairs: Option<&'a Path>,
    pub no_derive: bool,
}

#[derive(Debug)]
pub struct Done {
    pub score: Score,
    pub derived: Option<Derived>,
    pub anchors: BTreeMap<u32, Anchor>,
}

fn contributions(dbi: &[u8], d: &DbiView, ours: &[Sec]) -> Vec<(u32, u32)> {
    let mut out = Vec::new();
    let (at, len) = d.contribs;
    if len <= 4 {
        return out;
    }
    for i in 0..(len - 4) / SC_SIZE {
        let s = at + 4 + SC_SIZE * i;
        let (Some(isect), Some(off), Some(size)) =
            (u16_at(dbi, s), u32_at(dbi, s + 4), u32_at(dbi, s + 8))
        else {
            continue;
        };
        let Some(sec) = usize::from(isect).checked_sub(1).and_then(|k| ours.get(k)) else {
            continue;
        };
        if let Some(rva) = sec.rva.checked_add(off) {
            out.push((rva, size));
        }
    }
    out
}

pub fn relocate(i: &Inputs<'_>, db: &mut Msf2, w: &mut dyn Write) -> Result<Done> {
    let reference = i.reference;
    let ours_img = i.ours;
    let base = reference.opt.image_base.get();
    if ours_img.opt.image_base.get() != base {
        bail!(
            "our link is based at {:#x} and the reference at {base:#x}",
            ours_img.opt.image_base.get()
        );
    }
    let ref_secs = sections(reference);
    let our_secs = sections(ours_img);
    let mut pair = Pairing::of_report(i.report, base, i.report_label)?;
    writeln!(
        w,
        "{}: {} address pairing(s) from {}",
        i.name,
        pair.by_ours.len(),
        i.report_label
    )?;
    let (amb, amb_rows) = pair.ambiguity();
    if amb > 0 {
        writeln!(
            w,
            "  {amb} of our address(es) paired with more than one reference address ({amb_rows} \
             row(s)): a module record takes the candidate its own object compiled, anything \
             else does not translate"
        )?;
    }
    for obj in i.dirs.keys() {
        writeln!(
            w,
            "  {obj}: its TU has a `#line` -- the file record it names gets the TU's physical lines"
        )?;
    }
    match i.policy {
        Some(p) => {
            for l in crate::paths::rename(db, p)?.lines(p) {
                writeln!(w, "  {l}")?;
            }
        }
        None => writeln!(
            w,
            "  source names: no paths policy, so every name stays as our link wrote it"
        )?,
    }
    let info = crate::info::read_info(&db.stream(1)?)?;
    if info.version != crate::info::VC60 {
        bail!(
            "the PDB has interface version {} -- not the VC6 PDB a VC6 link writes",
            info.version
        );
    }
    let dbi_stream = db.stream(3)?;
    if dbi_stream.is_empty() {
        bail!("the PDB has an EMPTY DBI stream: a library PDB, which carries no addresses");
    }
    let dbi = DbiView::parse(&dbi_stream)?;
    if dbi.version != crate::dbi::V60 {
        writeln!(
            w,
            "  NOTE: DBI version {}, not VC6's {}",
            dbi.version,
            crate::dbi::V60
        )?;
    }
    writeln!(
        w,
        "  {} module(s); globals stream {}, publics stream {}, symbol records stream {}",
        dbi.mods.len(),
        dbi.globals,
        dbi.publics,
        dbi.symrecs
    )?;
    let (derived, anchors) = if i.no_derive {
        writeln!(w, "  --no-derive: the report's pairing alone")?;
        (None, BTreeMap::new())
    } else {
        let (d, anchors) = derive(i, db, &dbi, &dbi_stream, &pair, w)?;
        for (a, p) in &d.pairs {
            pair.by_ours.insert(*a, (p.refr, p.len));
        }
        (Some(d), anchors)
    };
    let mut extents = Vec::new();
    let mut named = Vec::new();
    for m in &dbi.mods {
        if m.stream == 0xffff || m.sym_len <= 4 {
            continue;
        }
        let s = db.stream(usize::from(m.stream))?;
        let region = s
            .get(4..m.sym_len.min(s.len()))
            .ok_or_else(|| anyhow!("module {} symbol region out of range", m.name))?;
        for (seg, off, len, name) in proc_extents(region) {
            let Some(rva) = usize::from(seg)
                .checked_sub(1)
                .and_then(|k| our_secs.get(k))
                .and_then(|sec| sec.rva.checked_add(off))
            else {
                continue;
            };
            extents.push((rva, len));
            named.push((m.name.clone(), name, rva, len));
        }
    }
    let widened = pair.widen(&extents);
    let (grafted, refused) = pair.graft(&named, &|o, r, len| {
        same_shape(ours_img, reference, o, r, len)
    });
    writeln!(
        w,
        "  {widened} pairing(s) widened to their procedure record's length, {grafted} grafted by \
         (object, name) for a row the /MAP does not place and proved byte-identical at the shipped \
         address, {refused} refused by that proof"
    )?;
    let reloc = Reloc {
        ours: &our_secs,
        reference: &ref_secs,
        pair: &pair,
        dirs: Some(i.dirs),
        module: None,
    };
    let mut score = Score::default();
    let mut recs = db.stream(usize::from(dbi.symrecs))?;
    reloc.region(&mut recs, &mut score)?;
    writeln!(
        w,
        "  symbol records stream: {} record(s) over {} of {} byte(s)",
        score.records, score.walked, score.region_bytes
    )?;
    let addrs = address_map(&recs)?;
    db.put_stream(usize::from(dbi.symrecs), &recs)?;
    let mut mod_syms = 0usize;
    for m in &dbi.mods {
        if m.stream == 0xffff || m.sym_len <= 4 {
            continue;
        }
        let mut s = db.stream(usize::from(m.stream))?;
        let end = m.sym_len.min(s.len());
        let region = s
            .get_mut(4..end)
            .ok_or_else(|| anyhow!("module {} symbol region out of range", m.name))?;
        let procs: Vec<(u16, u32, u32)> = proc_extents(region)
            .into_iter()
            .map(|(s, o, l, _)| (s, o, l))
            .collect();
        let before = score.patched;
        let mreloc = Reloc {
            module: Some(m.name.as_str()),
            ..reloc
        };
        mreloc.region(region, &mut score)?;
        mod_syms += score.patched - before;
        if m.line_len > 0 {
            let from = m.sym_len;
            let to = from.saturating_add(m.line_len).min(s.len());
            let lines = s
                .get_mut(from..to)
                .ok_or_else(|| anyhow!("module {} line region out of range", m.name))?;
            mreloc.c11(lines, &procs, &mut score)?;
        }
        db.put_stream(usize::from(m.stream), &s)?;
    }
    writeln!(
        w,
        "  module symbol record(s) patched: {mod_syms}\n  C11 line entries: {} translated, {} dropped \
         (untranslatable, each alone); {} block(s) left with none, {} re-sorted; {} range(s) set, {} emptied\n  \
         C11 line numbers under a TU's own `#line`: {} written as the TU's physical line, {} left presumed (no \
         region maps them)",
        score.lines,
        score.lines_left,
        score.blocks_dropped,
        score.blocks_resorted,
        score.ranges,
        score.ranges_left,
        score.lines_physical,
        score.lines_presumed
    )?;
    let sorted = resort_publics(db, dbi.publics, &addrs)?;
    let (th_done, th_left, n_sects) = patch_publics_tables(db, dbi.publics, reference, &ref_secs)?;
    writeln!(
        w,
        "  publics address map re-sorted: {sorted} entr(ies)\n  \
         publics thunk map: {th_done} entr(ies) set to the reference's own thunk targets, \
         {th_left} retired; section map: {n_sects} entr(ies)"
    )?;
    let mut dbi_bytes = dbi_stream.clone();
    let segs = patch_segmap(&mut dbi_bytes, dbi.segmap, &ref_secs)?;
    let (sc_done, sc_left) = patch_contribs(&mut dbi_bytes, &dbi, &reloc)?;
    db.put_stream(3, &dbi_bytes)?;
    writeln!(
        w,
        "  segment map: {segs} segment(s) set to the reference's section lengths\n  \
         section contributions: {sc_done} translated (length clamped to the contiguous run), \
         {sc_left} RETIRED (no pairing for the first byte of that contribution)"
    )?;
    match dbi.dbg_streams.get(DBG_FPO).copied() {
        Some(n) if n != 0xffff => {
            let mut s = db.stream(usize::from(n))?;
            let (f_done, f_left) = patch_fpo(&mut s, &reloc)?;
            db.put_stream(usize::from(n), &s)?;
            writeln!(
                w,
                "  FPO stream {n}: {f_done} entr(ies) relocated, {f_left} RETIRED, re-sorted by \
                 start address"
            )?;
        }
        _ => writeln!(w, "  FPO stream: none in the optional debug header")?,
    }
    match dbi.dbg_streams.get(DBG_SECTION_HEADER).copied() {
        Some(n) if n != 0xffff => {
            let hdrs = section_header_bytes(reference)?;
            let mut s = db.stream(usize::from(n))?;
            let take = hdrs.len().min(s.len());
            let dst = s
                .get_mut(..take)
                .ok_or_else(|| anyhow!("section header stream {n} too short"))?;
            let src = hdrs
                .get(..take)
                .ok_or_else(|| anyhow!("section header table truncated"))?;
            dst.copy_from_slice(src);
            db.put_stream(usize::from(n), &s)?;
            writeln!(
                w,
                "  section headers stream {n}: {} byte(s) replaced with the reference's own \
                 section table ({} section(s))",
                take,
                reference.sections.len()
            )?;
        }
        _ => writeln!(
            w,
            "  section headers stream: none in the optional debug header"
        )?,
    }
    let cv = reference.codeview()?;
    let mut s1 = db.stream(1)?;
    match &cv {
        Some(c) => {
            put_u32(&mut s1, 4, c.signature)?;
            put_u32(&mut s1, 8, c.age)?;
            writeln!(
                w,
                "  identity: signature {:#010x} age {} -- MATCHES the shipped {} record ({})",
                c.signature, c.age, c.format, c.path
            )?;
        }
        None => {
            put_u32(&mut s1, 4, reference.timestamp)?;
            put_u32(&mut s1, 8, 1)?;
            writeln!(
                w,
                "  identity: {} has no debug directory, so no record to match -- the PDB is named \
                 by the image's TimeDateStamp {:#010x}, age 1 (our link's {:#010x} age {} was a \
                 clock reading), and must be loaded explicitly",
                i.name, reference.timestamp, info.signature, info.age
            )?;
        }
    }
    db.put_stream(1, &s1)?;
    let (new_sig, age) = cv
        .as_ref()
        .map_or((reference.timestamp, 1), |c| (c.signature, c.age));
    if age != dbi.age {
        let mut d = db.stream(3)?;
        put_u32(&mut d, DBI_AGE_AT, age)?;
        db.put_stream(3, &d)?;
        writeln!(
            w,
            "  DBI header age: {} -> {age}, equal to stream 1's",
            dbi.age
        )?;
    }
    if dbi.tsm.1 > 0 {
        let mut d = db.stream(3)?;
        let servers = rename_type_servers(&mut d, dbi.tsm, info.signature, new_sig, age)?;
        db.put_stream(3, &d)?;
        writeln!(
            w,
            "  type-server map: {} entr(ies) ({}), this file's named {new_sig:#010x} age {age}, any other server's signature zeroed",
            servers.len(),
            servers
                .iter()
                .map(|s| s.pdb.as_str())
                .collect::<Vec<_>>()
                .join(", ")
        )?;
    }
    let zeroed = db.zero_unused()?;
    writeln!(
        w,
        "  {zeroed} byte(s) no stream holds zeroed (stale pages, stream slack, stale pointers): the file carries nothing of the process that wrote it"
    )?;
    Ok(Done {
        score,
        derived,
        anchors,
    })
}

impl Done {
    #[must_use]
    pub fn retired_tsv(&self) -> String {
        let mut t = String::from("# kind\tname\tour section\tour rva\n");
        for (k, n, s, r) in &self.score.retired_list {
            t.push_str(&format!("{k:#06x}\t{n}\t{s}\t{r:#x}\n"));
        }
        t
    }

    pub fn tail(&self, out: &str, size: usize, w: &mut dyn Write) -> Result<()> {
        let score = &self.score;
        writeln!(
            w,
            "  patched {} record(s), retired {} unmapped, left {} absolute\n  wrote {out} ({size} bytes)",
            score.patched, score.retired, score.kept
        )?;
        for (k, n) in &score.kinds {
            writeln!(w, "    kind {k:#06x}  {n} patched")?;
        }
        for (k, n) in &score.retired_kinds {
            writeln!(
                w,
                "    kind {k:#06x}  {n} RETIRED (no reference pairing for its address)"
            )?;
        }
        for (s, n) in &score.retired_in {
            writeln!(w, "    retired in OUR {s}: {n}")?;
        }
        let secs: BTreeSet<&String> = score
            .patched_in
            .keys()
            .chain(score.retired_in.keys())
            .collect();
        writeln!(
            w,
            "  BY OUR SECTION (address-bearing records):  patched / retired"
        )?;
        for s in secs {
            writeln!(
                w,
                "    {s:10} {:6} / {:6}",
                score.patched_in.get(s).copied().unwrap_or(0),
                score.retired_in.get(s).copied().unwrap_or(0)
            )?;
        }
        Ok(())
    }
}

#[must_use]
pub fn retired_path(pairs: &Path) -> std::path::PathBuf {
    pairs.with_extension("retired.tsv")
}

pub fn same_shape(ours: &Image, reference: &Image, our_rva: u32, ref_rva: u32, len: u32) -> bool {
    let n = usize::try_from(len).unwrap_or(0);
    if n == 0 {
        return false;
    }
    let (Some(a), Some(b)) = (
        ours.at_rva(Rva(our_rva), n),
        reference.at_rva(Rva(ref_rva), n),
    ) else {
        return false;
    };
    shapes_agree(a, b, &image_span(ours), &image_span(reference))
}

#[must_use]
pub fn image_span(img: &Image) -> std::ops::Range<u32> {
    let base = img.opt.image_base.get();
    let end = img
        .sections
        .iter()
        .map(|s| {
            s.rva
                .get()
                .saturating_add(s.virt_size.get().max(s.raw_size.get()))
        })
        .max()
        .unwrap_or(img.opt.size_of_image)
        .max(img.opt.size_of_image);
    base..base.saturating_add(end)
}

#[must_use]
pub fn shapes_agree(
    a: &[u8],
    b: &[u8],
    a_addrs: &std::ops::Range<u32>,
    b_addrs: &std::ops::Range<u32>,
) -> bool {
    let n = a.len().min(b.len());
    let mut masked = vec![false; n];
    for i in 0..n.saturating_sub(3) {
        let (Some(x), Some(y)) = (u32_at(a, i), u32_at(b, i)) else {
            continue;
        };
        if x != y && a_addrs.contains(&x) && b_addrs.contains(&y) {
            for m in masked.iter_mut().skip(i).take(4) {
                *m = true;
            }
        }
    }
    for i in 0..n.saturating_sub(4) {
        if matches!(a.get(i), Some(0xe8 | 0xe9)) && a.get(i) == b.get(i) {
            for m in masked.iter_mut().skip(i + 1).take(4) {
                *m = true;
            }
        }
    }
    a.len() == b.len()
        && (0..n).all(|i| masked.get(i).copied().unwrap_or(false) || a.get(i) == b.get(i))
}

type Rows = BTreeMap<u32, (u32, u32)>;

fn derive(
    i: &Inputs<'_>,
    db: &Msf2,
    dbi: &DbiView,
    dbi_stream: &[u8],
    pair: &Pairing,
    w: &mut dyn Write,
) -> Result<(Derived, BTreeMap<u32, Anchor>)> {
    let ref_secs = sections(i.reference);
    let our_secs = sections(i.ours);
    let pre = Reloc {
        ours: &our_secs,
        reference: &ref_secs,
        pair,
        module: None,
        dirs: None,
    };
    let mut anchors: BTreeMap<u32, Anchor> = BTreeMap::new();
    pre.starts(&db.stream(usize::from(dbi.symrecs))?, &mut anchors);
    for m in &dbi.mods {
        if m.stream == 0xffff || m.sym_len <= 4 {
            continue;
        }
        let s = db.stream(usize::from(m.stream))?;
        if let Some(r) = s.get(4..m.sym_len.min(s.len())) {
            pre.starts(r, &mut anchors);
        }
    }
    let from_pdb = anchors.len();
    let mut map_rows = 0usize;
    if let Some((rows, _)) = i.map {
        for r in rows {
            if i.ours.section_of(Rva(r.rva)).is_none() {
                continue;
            }
            map_rows += 1;
            let e = anchors.entry(r.rva).or_default();
            if !e.names.contains(&r.name) {
                e.names.push(r.name.clone());
            }
            if e.stem.is_empty() {
                e.stem.clone_from(&r.stem);
            }
        }
    }
    for a in pair.by_ours.keys() {
        anchors.entry(*a).or_default();
    }
    let contribs = contributions(dbi_stream, dbi, &our_secs);
    writeln!(
        w,
        "  derivation inputs: {} symbol start(s) ({from_pdb} from the PDB, {map_rows} /MAP row(s){}), \
         {} contribution(s), {} reference name(s), {} scoped table(s)",
        anchors.len(),
        i.map.map_or(String::new(), |(_, p)| format!(" from {p}")),
        contribs.len(),
        i.names.len(),
        i.scoped.len()
    )?;
    let (fallback, given): (Rows, Rows) = pair
        .by_ours
        .iter()
        .map(|(a, p)| (*a, *p))
        .partition(|(a, _)| pair.library.contains(a));
    writeln!(
        w,
        "  the report: {} own row(s) seed the walk, {} library row(s) are candidates only",
        given.len(),
        fallback.len()
    )?;
    let d = pair::derive(&pair::Inputs {
        reference: i.reference,
        ours: i.ours,
        given: &given,
        fallback: &fallback,
        anchors: &anchors,
        contribs: &contribs,
        names: i.names,
        scoped: i.scoped,
        dropped: i.dropped,
        budget: pair::Budget::default(),
    })?;
    let sec = |rva: u32| {
        i.ours
            .section_of(Rva(rva))
            .map_or_else(|| "?".to_string(), |s| s.name.clone())
    };
    let mut by: BTreeMap<(String, &'static str, String), usize> = BTreeMap::new();
    for (a, p) in &d.pairs {
        *by.entry((sec(*a), p.how.label(), p.proof.kind().to_string()))
            .or_default() += 1;
    }
    writeln!(
        w,
        "  derived {} pairing(s) in {} round(s); operands agreeing with a pairing {}, CONTRADICTING {} (+ {} naming an identical copy, ICF), \
         in no symbol {}; refused {}; dropped by the audit {}",
        d.pairs.len(),
        d.rounds,
        d.agree,
        d.contradict.len(),
        d.icf_twins,
        d.stray,
        d.refused.len(),
        d.audited_out.len()
    )?;
    if d.content_spent {
        writeln!(
            w,
            "  NOTE: the content search spent its budget of {} byte comparisons: the anchors left were not searched for",
            d.budget.content
        )?;
    }
    if d.rounds_capped {
        writeln!(
            w,
            "  NOTE: the derivation stopped after {} rounds, each pairing at least one anchor",
            d.budget.rounds
        )?;
    }
    for ((s, how, proof), n) in &by {
        writeln!(w, "    {s:8} {how:13} {proof:14} {n}")?;
    }
    writeln!(
        w,
        "  the report's library rows: {} confirmed at the row's own address, {} CORRECTED (the evidence \
         pairs them elsewhere), {} not verified (kept as the row gives them)",
        d.confirmed,
        d.corrected.len(),
        d.unverified.len()
    )?;
    let mut classes: BTreeMap<&str, usize> = BTreeMap::new();
    for (_, c, _) in &d.account {
        *classes.entry(c).or_default() += 1;
    }
    if !d.account.is_empty() {
        writeln!(
            w,
            "  {} anchor(s) UNPAIRED, by searching the reference for their bytes: {}",
            d.account.len(),
            classes
                .iter()
                .map(|(c, n)| format!("{c} {n}"))
                .collect::<Vec<_>>()
                .join(", ")
        )?;
    }
    for (o2, t, r, from) in d.contradict.iter().take(12) {
        writeln!(
            w,
            "    CONTRADICTION: our {o2:#x} is paired to {t:#x}, an operand of {from:#x} says {r:#x}"
        )?;
    }
    if let Some(p) = i.pairs {
        msvc6_base::srcio::own_write(p, pairs_tsv(&d, &anchors, &sec))
            .with_context(|| format!("writing {}", p.display()))?;
        writeln!(w, "  wrote {}", p.display())?;
    }
    Ok((d, anchors))
}

fn pairs_tsv(d: &Derived, anchors: &BTreeMap<u32, Anchor>, sec: &dyn Fn(u32) -> String) -> String {
    let names = |a: &u32| {
        anchors
            .get(a)
            .map(|x| x.names.join(" "))
            .unwrap_or_default()
    };
    let mut t =
        String::from("# our rva\treference rva\tlength\tour section\thow\tproof\tvotes\tnames\n");
    for (a, q) in &d.pairs {
        t.push_str(&format!(
            "{a:#x}\t{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\n",
            q.refr,
            q.len,
            sec(*a),
            q.how.label(),
            q.proof.label(),
            q.votes,
            names(a)
        ));
    }
    for (a, c, why) in &d.refused {
        t.push_str(&format!("# REFUSED {a:#x}\t{c:#x}\t{why}\t{}\n", names(a)));
    }
    for (a, c) in &d.audited_out {
        t.push_str(&format!("# AUDITED OUT {a:#x}\t{c:#x}\n"));
    }
    for (a, r, c) in &d.corrected {
        t.push_str(&format!(
            "# CORRECTED LIBRARY ROW {a:#x}\trow {r:#x}\tderived {c:#x}\t{}\n",
            names(a)
        ));
    }
    for (a, r, why) in &d.unverified {
        t.push_str(&format!(
            "# UNVERIFIED LIBRARY ROW {a:#x}\trow {r:#x}\t{why}\t{}\n",
            names(a)
        ));
    }
    for (a, c, why) in &d.account {
        t.push_str(&format!("# UNPAIRED {a:#x}\t{c}\t{why}\t{}\n", names(a)));
    }
    for (o2, tt, r, from) in &d.contradict {
        t.push_str(&format!(
            "# CONTRADICTION {o2:#x}\tpaired {tt:#x}\toperand {r:#x}\tof {from:#x}\n"
        ));
    }
    t
}

fn address_map(recs: &[u8]) -> Result<BTreeMap<u32, (u16, u32)>> {
    let mut out = BTreeMap::new();
    for (pos, kind, payload, len) in records(recs) {
        let Some(at) = address_at(kind) else { continue };
        if at + 6 > len {
            continue;
        }
        let off = u32_at(recs, payload + at).context("record truncated")?;
        let seg = u16_at(recs, payload + at + 4).context("record truncated")?;
        out.insert(u32::try_from(pos)?, (seg, off));
    }
    Ok(out)
}

fn resort_publics(db: &mut Msf2, publics: u16, addrs: &BTreeMap<u32, (u16, u32)>) -> Result<usize> {
    if publics == 0xffff {
        return Ok(0);
    }
    let mut s = db.stream(usize::from(publics))?;
    let hash_len = usize::try_from(u32_at(&s, 0).context("publics header truncated")?)?;
    let map_len = usize::try_from(u32_at(&s, 4).context("publics header truncated")?)?;
    let at = 28usize
        .checked_add(hash_len)
        .context("publics header overflow")?;
    let end = at.checked_add(map_len).context("publics header overflow")?;
    if end > s.len() || map_len % 4 != 0 {
        bail!(
            "publics address map ({at}..{end}) does not fit stream {publics} of {} bytes",
            s.len()
        );
    }
    let n = map_len / 4;
    let mut entries: Vec<u32> = Vec::with_capacity(n);
    for k in 0..n {
        entries.push(u32_at(&s, at + 4 * k).context("address map truncated")?);
    }
    let key = |off: &u32| -> (u16, u32, u32) {
        match addrs.get(off) {
            None => (0, 0, *off),
            Some((seg, o)) => (*seg, *o, *off),
        }
    };
    entries.sort_by_key(key);
    for (k, e) in entries.iter().enumerate() {
        put_u32(&mut s, at + 4 * k, *e)?;
    }
    db.put_stream(usize::from(publics), &s)?;
    Ok(n)
}

pub fn section_header_bytes(img: &Image) -> Result<Vec<u8>> {
    let at = img
        .opt_off
        .checked_add(usize::from(img.opt_size))
        .context("section table offset overflow")?;
    let len = img
        .sections
        .len()
        .checked_mul(40)
        .context("section table length overflow")?;
    let b = img
        .bytes()
        .get(at..at.checked_add(len).context("overflow")?)
        .context("section table runs past the image")?;
    Ok(b.to_vec())
}

fn patch_contribs(dbi: &mut [u8], d: &DbiView, reloc: &Reloc<'_>) -> Result<(usize, usize)> {
    let mut done = 0usize;
    let mut left = 0usize;
    let mut sites: Vec<(usize, Option<&str>)> = d
        .mods
        .iter()
        .map(|m| (m.sc_at, Some(m.name.as_str())))
        .collect();
    let (at, len) = d.contribs;
    let n = if len > 4 { (len - 4) / SC_SIZE } else { 0 };
    for k in 0..n {
        let s = at + 4 + SC_SIZE * k;
        let imod = u16_at(dbi, s + 16).context("contribution truncated")?;
        let module = d.mods.get(usize::from(imod)).map(|m| m.name.as_str());
        sites.push((s, module));
    }
    for (s, module) in sites {
        let isect = u16_at(dbi, s).context("contribution truncated")?;
        let off = u32_at(dbi, s + 4).context("contribution truncated")?;
        let size = u32_at(dbi, s + 8).context("contribution truncated")?;
        if isect == 0 || isect == 0xffff {
            continue;
        }
        let r = Reloc { module, ..*reloc };
        if let Some((ns, no, nz)) = r.span(isect, off, size) {
            put_u16(dbi, s, ns)?;
            put_u32(dbi, s + 4, no)?;
            put_u32(dbi, s + 8, nz)?;
            done += 1;
        } else {
            put_u16(dbi, s, 0)?;
            put_u32(dbi, s + 4, 0)?;
            put_u32(dbi, s + 8, 0)?;
            left += 1;
        }
    }
    let mut rows: Vec<Vec<u8>> = Vec::with_capacity(n);
    for k in 0..n {
        let s = at + 4 + SC_SIZE * k;
        rows.push(
            dbi.get(s..s + SC_SIZE)
                .context("contribution truncated")?
                .to_vec(),
        );
    }
    rows.sort_by_key(|r| {
        (
            u16_at(r, 0).unwrap_or(0),
            u32_at(r, 4).unwrap_or(0),
            u32_at(r, 8).unwrap_or(0),
        )
    });
    for (k, r) in rows.iter().enumerate() {
        let s = at + 4 + SC_SIZE * k;
        dbi.get_mut(s..s + SC_SIZE)
            .context("contribution truncated")?
            .copy_from_slice(r);
    }
    Ok((done, left))
}

fn patch_fpo(s: &mut [u8], reloc: &Reloc<'_>) -> Result<(usize, usize)> {
    let (mut done, mut left) = (0usize, 0usize);
    let n = s.len() / FPO_SIZE;
    for k in 0..n {
        let at = FPO_SIZE * k;
        let start = u32_at(s, at).context("FPO entry truncated")?;
        let size = u32_at(s, at + 4).context("FPO entry truncated")?;
        if let Some((r, run)) = reloc.pair.run(start, None) {
            put_u32(s, at, r)?;
            put_u32(s, at + 4, size.min(run))?;
            done += 1;
        } else {
            put_u32(s, at, 0)?;
            put_u32(s, at + 4, 0)?;
            left += 1;
        }
    }
    let mut rows: Vec<Vec<u8>> = Vec::with_capacity(n);
    for k in 0..n {
        let at = FPO_SIZE * k;
        rows.push(s.get(at..at + FPO_SIZE).context("FPO truncated")?.to_vec());
    }
    rows.sort_by_key(|r| (u32_at(r, 0).unwrap_or(0), u32_at(r, 4).unwrap_or(0)));
    for (k, r) in rows.iter().enumerate() {
        let at = FPO_SIZE * k;
        s.get_mut(at..at + FPO_SIZE)
            .context("FPO truncated")?
            .copy_from_slice(r);
    }
    Ok((done, left))
}

fn patch_publics_tables(
    db: &mut Msf2,
    publics: u16,
    reference: &Image,
    ref_secs: &[Sec],
) -> Result<(usize, usize, usize)> {
    if publics == 0xffff {
        return Ok((0, 0, 0));
    }
    let mut s = db.stream(usize::from(publics))?;
    let g = |o: usize| u32_at(&s, o).context("publics header truncated");
    let hash = usize::try_from(g(0)?)?;
    let map = usize::try_from(g(4)?)?;
    let n_thunks = usize::try_from(g(8)?)?;
    let n_sects = usize::try_from(g(24)?)?;
    let thunks_at = 28usize
        .checked_add(hash)
        .and_then(|v| v.checked_add(map))
        .context("publics header overflow")?;
    let sects_at = n_thunks
        .checked_mul(4)
        .and_then(|v| v.checked_add(thunks_at))
        .context("publics header overflow")?;
    if n_sects
        .checked_mul(8)
        .and_then(|v| v.checked_add(sects_at))
        .is_none_or(|e| e > s.len())
    {
        bail!(
            "the publics stream's trailing tables ({n_thunks} thunk(s), {n_sects} section(s)) do \
             not fit its {} bytes",
            s.len()
        );
    }
    let (mut done, mut left) = (0usize, 0usize);
    if n_thunks > 0 {
        let isect = u16_at(&s, 16).context("publics header truncated")?;
        let stride = g(12)?.max(1);
        let sec = ref_secs
            .get(usize::from(isect).saturating_sub(1))
            .ok_or_else(|| {
                anyhow!("the thunk table names section {isect}, which the reference does not have")
            })?;
        let lattice = (0..16u32)
            .find(|d| {
                reference
                    .at_rva(Rva(sec.rva + d), 1)
                    .and_then(|b| b.first().copied())
                    == Some(0xe9)
            })
            .map(|d| sec.rva + d);
        for k in 0..n_thunks {
            let target = lattice
                .and_then(|l| {
                    let slot = l.checked_add(stride.checked_mul(u32::try_from(k).ok()?)?)?;
                    let b = reference.at_rva(Rva(slot), 5)?;
                    if b.first() != Some(&0xe9) {
                        return None;
                    }
                    let rel = i32::from_le_bytes([*b.get(1)?, *b.get(2)?, *b.get(3)?, *b.get(4)?]);
                    u32::try_from(i64::from(slot) + 5 + i64::from(rel)).ok()
                })
                .unwrap_or(0);
            put_u32(&mut s, thunks_at + 4 * k, target)?;
            if target == 0 {
                left += 1;
            } else {
                done += 1;
            }
        }
        if let Some(l) = lattice {
            put_u32(&mut s, 20, l.saturating_sub(sec.rva))?;
        }
    }
    let mut sects = 0usize;
    for j in 0..n_sects {
        let isect = u16_at(&s, sects_at + 8 * j + 4).context("section map truncated")?;
        let Some(sec) = ref_secs.get(usize::from(isect).saturating_sub(1)) else {
            continue;
        };
        put_u32(&mut s, sects_at + 8 * j, sec.rva)?;
        sects += 1;
    }
    db.put_stream(usize::from(publics), &s)?;
    Ok((done, left, sects))
}

fn patch_segmap(dbi: &mut [u8], at_len: (usize, usize), ref_secs: &[Sec]) -> Result<usize> {
    let (at, len) = at_len;
    if len < 4 {
        return Ok(0);
    }
    let count = usize::from(u16_at(dbi, at).context("segment map truncated")?);
    let mut done = 0usize;
    for k in 0..count {
        let rec = at + 4 + SEGMAP_SIZE * k;
        if rec + SEGMAP_SIZE > at + len {
            break;
        }
        let flags = u16_at(dbi, rec).context("segment map truncated")?;
        if flags & 0x0200 != 0 {
            continue;
        }
        let frame = u16_at(dbi, rec + 6).context("segment map truncated")?;
        let Some(sec) = ref_secs.get(usize::from(frame).saturating_sub(1)) else {
            continue;
        };
        put_u32(dbi, rec + 12, 0)?;
        put_u32(dbi, rec + 16, sec.vsize)?;
        done += 1;
    }
    Ok(done)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sec(name: &str, rva: u32, len: u32) -> Sec {
        Sec {
            name: name.into(),
            rva,
            len,
            vsize: len,
        }
    }

    fn row(address: &str, recomp: &str, size: u64, obj: &str) -> Row {
        Row {
            address: crate::report::hex(address),
            recomp: crate::report::hex(recomp),
            has_recomp: true,
            obj: obj.into(),
            size: Some(size),
            ..Row::default()
        }
    }

    fn pairing(map: &[(u32, u32, u32)]) -> Pairing {
        Pairing {
            by_ours: map.iter().map(|(o, r, l)| (*o, (*r, *l))).collect(),
            ..Pairing::default()
        }
    }

    fn proc_record(seg: u16, off: u32, len: u32) -> Vec<u8> {
        let mut p = vec![0u8; 12];
        p.extend_from_slice(&len.to_le_bytes());
        p.extend_from_slice(&[0u8; 12]);
        p.extend_from_slice(&off.to_le_bytes());
        p.extend_from_slice(&seg.to_le_bytes());
        p.push(0);
        p.push(0);
        let mut b = u16::try_from(p.len() + 2).unwrap().to_le_bytes().to_vec();
        b.extend_from_slice(&0x100bu16.to_le_bytes());
        b.extend_from_slice(&p);
        b
    }

    fn label_record(off: u32) -> Vec<u8> {
        let mut r = 10u16.to_le_bytes().to_vec();
        r.extend_from_slice(&0x0209u16.to_le_bytes());
        r.extend_from_slice(&off.to_le_bytes());
        r.extend_from_slice(&1u16.to_le_bytes());
        r.extend_from_slice(&[0, 0]);
        r
    }

    #[test]
    fn a_record_walk_reads_length_kind_and_payload() {
        let mut b = Vec::new();
        b.extend_from_slice(&14u16.to_le_bytes());
        b.extend_from_slice(&0x1009u16.to_le_bytes());
        b.extend_from_slice(&0u32.to_le_bytes());
        b.extend_from_slice(&0x6a0u32.to_le_bytes());
        b.extend_from_slice(&1u16.to_le_bytes());
        b.extend_from_slice(&[0, 0]);
        let r = records(&b);
        assert_eq!(r.len(), 1);
        assert_eq!(r[0].1, 0x1009);
        assert_eq!(
            u32_at(&b, r[0].2 + address_at(0x1009).unwrap()),
            Some(0x6a0)
        );
        let mut short = b.clone();
        short.truncate(10);
        assert!(records(&short).is_empty());
    }

    #[test]
    fn a_reference_record_is_walked_past_its_name() {
        let mut b = 14u16.to_le_bytes().to_vec();
        b.extend_from_slice(&0x0400u16.to_le_bytes());
        b.extend_from_slice(&[0u8; 12]);
        b.push(3);
        b.extend_from_slice(b"abc");
        while !b.len().is_multiple_of(4) {
            b.push(0);
        }
        let at = b.len();
        b.extend_from_slice(&label_record(0x10));
        let r = records(&b);
        assert_eq!(r.len(), 2);
        assert_eq!(r[1].0, at);
    }

    #[test]
    fn a_child_record_moves_with_its_procedure_and_an_outside_one_retires() {
        let ours = vec![sec(".text", 0x1000, 0x8000)];
        let reference = vec![sec(".text", 0x1000, 0x8000)];
        let pair = pairing(&[(0x2000, 0x5000, 0x40)]);
        let reloc = Reloc {
            ours: &ours,
            reference: &reference,
            pair: &pair,
            module: None,
            dirs: None,
        };
        let mut b = proc_record(1, 0x1000, 0x40);
        let inside = b.len() + 4;
        b.extend_from_slice(&label_record(0x1012));
        let outside = b.len() + 4;
        b.extend_from_slice(&label_record(0x3000));
        let mut score = Score::default();
        reloc.region(&mut b, &mut score).unwrap();
        assert_eq!(u32_at(&b, 4 + 28), Some(0x4000), "the procedure moved");
        assert_eq!(u32_at(&b, inside), Some(0x4012), "the label moved with it");
        assert_eq!(u16_at(&b, inside + 4), Some(1));
        assert_eq!(u16_at(&b, outside + 4), Some(0), "outside: retired");
        assert_eq!(u32_at(&b, outside), Some(0));
        assert_eq!(score.retired_kinds.get(&0x0209), Some(&1));
        assert_eq!(score.kinds.get(&0x0209), Some(&1));
        assert_eq!(score.retired_list.len(), 1);
    }

    #[test]
    fn a_line_directives_record_gets_the_tus_physical_lines() {
        use crate::linedir::LineDirectives;
        let ours = vec![sec(".text", 0x1000, 0x8000)];
        let reference = vec![sec(".text", 0x1000, 0x8000)];
        let pair = pairing(&[(0x2000, 0x5000, 0x40)]);
        let mut src: Vec<String> = (1..=931).map(|i| format!("x{i};")).collect();
        src.push("#line 1458 \"C:\\\\A\\\\T.cpp\"".into());
        src.extend((933..=940).map(|i| format!("y{i};")));
        let d = LineDirectives::parse(&src.join("\n"), "T.cpp")
            .unwrap()
            .unwrap();
        let dirs: ByObject = [("t.obj".to_string(), d)].into_iter().collect();
        let region = || {
            c11_region(
                1,
                &[
                    ("c:\\a\\src\\t.cpp", &[(0x1000, 10), (0x1008, 11)]),
                    ("C:\\A\\T.cpp", &[(0x1010, 1459), (0x1018, 1460)]),
                ],
                (0x1000, 0x103f),
            )
        };
        let run = |module: &str, dirs: Option<&ByObject>| {
            let reloc = Reloc {
                ours: &ours,
                reference: &reference,
                pair: &pair,
                module: Some(module),
                dirs,
            };
            let mut b = region();
            let mut score = Score::default();
            reloc.c11(&mut b, &[(1, 0x1000, 0x40)], &mut score).unwrap();
            (c11_pairs(&b), score.lines_physical, score.lines_presumed)
        };
        let named = |a: [(u32, u16); 2], b: [(u32, u16); 2]| {
            vec![
                ("c:\\a\\src\\t.cpp".to_string(), a.to_vec()),
                ("C:\\A\\T.cpp".to_string(), b.to_vec()),
            ]
        };
        assert_eq!(
            run(".\\Debug\\T.obj", Some(&dirs)),
            (
                named([(0x4000, 10), (0x4008, 11)], [(0x4010, 934), (0x4018, 935)]),
                2,
                0
            )
        );
        let presumed = named(
            [(0x4000, 10), (0x4008, 11)],
            [(0x4010, 1459), (0x4018, 1460)],
        );
        assert_eq!(
            run(".\\Debug\\U.obj", Some(&dirs)),
            (presumed.clone(), 0, 0)
        );
        assert_eq!(run(".\\Debug\\T.obj", None), (presumed, 0, 0));
    }

    #[test]
    fn a_line_pair_that_does_not_translate_is_dropped_alone() {
        let ours = vec![sec(".text", 0x1000, 0x8000)];
        let reference = vec![sec(".text", 0x1000, 0x8000)];
        let pair = pairing(&[(0x2000, 0x5000, 0x10), (0x2010, 0x4000, 0x10)]);
        let reloc = Reloc {
            ours: &ours,
            reference: &reference,
            pair: &pair,
            module: None,
            dirs: None,
        };
        let mut b = c11_region(
            1,
            &[("a.c", &[(0x1000, 1), (0x1010, 2), (0x1030, 3)])],
            (0x1000, 0x103f),
        );
        let mut score = Score::default();
        reloc.c11(&mut b, &[(1, 0x1000, 0x20)], &mut score).unwrap();
        assert_eq!(
            c11_pairs(&b),
            vec![("a.c".to_string(), vec![(0x3000, 2), (0x4000, 1)])]
        );
        assert_eq!(
            (score.lines, score.lines_left, score.blocks_resorted),
            (2, 1, 1)
        );
    }

    #[test]
    fn two_rows_claiming_one_counterpart_are_never_overwritten() {
        let mut a = row("0x4044b0", "0x405c40", 16, "Alpha.obj");
        a.name = "$E42".into();
        let mut b = row("0x44aee0", "0x405c40", 16, "Beta.obj");
        b.name = "$E42".into();
        let p = Pairing::from_rows(&[a, b], 0x40_0000);
        assert!(p.by_ours.is_empty(), "neither row may win by being last");
        assert_eq!(p.ambiguity(), (1, 2));
        assert_eq!(p.at(0x5c40, Some(r"C:\w\Release\Alpha.obj")), Some(0x44b0));
        assert_eq!(p.at(0x5c40, Some(r"C:\w\Release\Beta.obj")), Some(0x4aee0));
        assert_eq!(p.at(0x5c40, None), None, "a public belongs to no module");
        assert!(Pairing::of_report(&Report::default(), 0, "r").is_err());
    }

    #[test]
    fn a_row_with_no_counterpart_is_kept_as_an_unplaced_candidate_once() {
        let mut placed = row("0x10001000", "0x10002000", 32, "A.obj");
        placed.name = "f".into();
        let mut unplaced = row("0x100090c0", "", 34, "Sound.obj");
        unplaced.name = "$E6".into();
        unplaced.recomp = None;
        let mut again = unplaced.clone();
        again.address = Some(0x1000_a000);
        let mut third = unplaced.clone();
        third.address = Some(0x1000_b000);
        let p = Pairing::from_rows(&[placed, unplaced.clone()], 0x1000_0000);
        assert_eq!(p.by_ours.get(&0x2000), Some(&(0x1000, 32)));
        assert_eq!(
            p.unplaced
                .get(&("sound.obj".to_string(), "$E6".to_string())),
            Some(&(0x90c0, 34))
        );
        let q = Pairing::from_rows(&[unplaced, again, third], 0x1000_0000);
        assert!(
            q.unplaced.is_empty(),
            "a name twice in one object stays refused"
        );
    }

    #[test]
    fn a_shape_proof_masks_moved_addresses_and_nothing_else() {
        let a_addrs = 0x40_0000..0x50_0000;
        let b_addrs = 0x40_0000..0x50_0000;
        let mk = |rel: i32, abs: u32, last: u8| {
            let mut v = vec![0x55, 0x8b, 0xec, 0xe8];
            v.extend_from_slice(&rel.to_le_bytes());
            v.push(0xa1);
            v.extend_from_slice(&abs.to_le_bytes());
            v.push(last);
            v
        };
        let a = mk(0x100, 0x44_1000, 0xc3);
        assert!(shapes_agree(
            &a,
            &mk(0x2200, 0x45_2000, 0xc3),
            &a_addrs,
            &b_addrs
        ));
        assert!(!shapes_agree(
            &a,
            &mk(0x2200, 0x45_2000, 0x90),
            &a_addrs,
            &b_addrs
        ));
        assert!(!shapes_agree(
            &a,
            &mk(0x2200, 0x10_0000, 0xc3),
            &a_addrs,
            &b_addrs
        ));
        assert!(!shapes_agree(
            &a,
            a.get(..a.len() - 1).unwrap(),
            &a_addrs,
            &b_addrs
        ));
    }

    #[test]
    fn an_interior_address_translates_through_its_function() {
        let p = pairing(&[(0x2be0, 0x1880, 40)]);
        assert_eq!(p.at(0x2be0, None), Some(0x1880));
        assert_eq!(p.at(0x2be8, None), Some(0x1888));
        assert_eq!(p.at(0x2c08, None), None, "past the function's length");
        assert_eq!(p.at(0x100, None), None, "below every pairing");
        assert_eq!(p.run(0x2be8, None), Some((0x1888, 32)));
    }

    #[test]
    fn one_reference_address_named_twice_is_not_ambiguous() {
        let p = Pairing::from_rows(
            &[
                row("0x10001880", "0x10002be0", 40, "T.obj"),
                row("0x10001880", "0x10002be0", 40, "T.obj"),
            ],
            0x1000_0000,
        );
        assert_eq!(p.ambiguity(), (0, 0));
        assert_eq!(p.at(0x2be0, None), Some(0x1880));
    }

    #[test]
    fn widen_takes_the_record_length_and_graft_pairs_by_object_and_name() {
        let mut p = pairing(&[(0x2be0, 0x1880, 40)]);
        p.unplaced
            .insert(("keys.obj".to_string(), "$E6".to_string()), (0x1b410, 34));
        p.unplaced
            .insert(("other.obj".to_string(), "$E6".to_string()), (0x1b500, 34));
        assert_eq!(p.widen(&[(0x2be0, 64), (0x3000, 99)]), 1);
        assert_eq!(p.widen(&[(0x2be0, 8)]), 0, "never shortens");
        assert_eq!(p.at(0x2c08, None), Some(0x18a8));
        let named = vec![
            (
                ".\\Release\\keys.obj".to_string(),
                "$E6".to_string(),
                0x2be0,
                34,
            ),
            (
                ".\\Release\\Keys.obj".to_string(),
                "$E6".to_string(),
                0x9100,
                34,
            ),
            (
                ".\\Release\\Keys.obj".to_string(),
                "$E9".to_string(),
                0x9200,
                34,
            ),
        ];
        assert_eq!(p.graft(&named, &|_, _, _| true), (1, 0));
        assert_eq!(p.at(0x9100, None), Some(0x1b410));
        assert_eq!(p.at(0x2be0, None), Some(0x1880));
        assert_eq!(p.at(0x9200, None), None);
        let mut q = pairing(&[(0x2be0, 0x1880, 40)]);
        q.unplaced = p.unplaced.clone();
        assert_eq!(q.graft(&named, &|_, _, _| false), (0, 1));
        let mut r = Pairing {
            unplaced: p.unplaced.clone(),
            ..Pairing::default()
        };
        let short = vec![(
            ".\\Release\\Keys.obj".to_string(),
            "$E6".to_string(),
            0x9100,
            33,
        )];
        assert_eq!(r.graft(&short, &|_, _, _| true), (0, 1));
    }

    fn server(sig: u32, age: u32, pdb: &str, via: &str) -> Vec<u8> {
        let mut b = vec![0u8; 8];
        b.extend_from_slice(&sig.to_le_bytes());
        b.extend_from_slice(&age.to_le_bytes());
        b.extend_from_slice(pdb.as_bytes());
        b.push(0);
        b.extend_from_slice(via.as_bytes());
        b.push(0);
        while !b.len().is_multiple_of(4) {
            b.push(0);
        }
        b
    }

    #[test]
    fn a_type_server_map_is_aligned_entries_and_only_their_identities_change() {
        let tsm = [
            server(
                0x6ac8_c868,
                1,
                "C:\\W\\Debug\\app.pdb",
                "C:\\W\\Debug\\app.pdb",
            ),
            server(0x6ac8_c866, 1, "c:\\w\\debug\\vc60.pdb", ".\\Debug\\a.obj"),
            server(
                0x3667_1b00,
                0x112,
                "e:\\crt\\libcmtd.pdb",
                "C:\\vc\\lib\\libcmtd.lib",
            ),
        ]
        .concat();
        let mut d = vec![0xeeu8; 64];
        d.extend_from_slice(&tsm);
        d.extend_from_slice(&[0xfe, 0xef, 0xfe, 0xef]);
        let range = (64, tsm.len());
        let servers = type_servers(&d, range).unwrap();
        assert_eq!(servers.len(), 3);
        assert!(servers.iter().all(|s| (s.at - 64) % 4 == 0));
        let before = d.clone();
        rename_type_servers(&mut d, range, 0x6ac8_c868, 0x3c18_cb6a, 23).unwrap();
        let after = type_servers(&d, range).unwrap();
        assert_eq!(
            after.iter().map(|s| (s.sig, s.age)).collect::<Vec<_>>(),
            [(0x3c18_cb6a, 23), (0, 0), (0, 0)]
        );
        let words: Vec<usize> = servers
            .iter()
            .flat_map(|s| (s.at + TS_SIG..s.at + TS_AGE + 4).collect::<Vec<_>>())
            .collect();
        for (i, (x, y)) in before.iter().zip(&d).enumerate() {
            if !words.contains(&i) {
                assert_eq!(x, y, "byte {i} changed outside an identity word");
            }
        }
        assert!(type_servers(&d, (64, tsm.len() - 4)).is_err());
        assert!(type_servers(&d, (64, 12)).is_err());
        assert!(type_servers(&d, (64, 0)).unwrap().is_empty());
    }

    fn c11_region(seg: u16, files: &[(&str, &[(u32, u16)])], range: (u32, u32)) -> Vec<u8> {
        let u32_of = |v: usize| u32::try_from(v).unwrap();
        let pad4 = |b: &mut Vec<u8>| {
            while !b.len().is_multiple_of(4) {
                b.push(0);
            }
        };
        let mut b = Vec::new();
        b.extend_from_slice(&u16::try_from(files.len()).unwrap().to_le_bytes());
        b.extend_from_slice(&1u16.to_le_bytes());
        let table = b.len();
        b.resize(table + 4 * files.len(), 0);
        b.extend_from_slice(&range.0.to_le_bytes());
        b.extend_from_slice(&range.1.to_le_bytes());
        b.extend_from_slice(&seg.to_le_bytes());
        pad4(&mut b);
        for (i, (name, pairs)) in files.iter().enumerate() {
            let base = b.len();
            b[table + 4 * i..table + 4 * i + 4].copy_from_slice(&u32_of(base).to_le_bytes());
            b.extend_from_slice(&1u16.to_le_bytes());
            b.extend_from_slice(&0u16.to_le_bytes());
            let ln_at = b.len();
            b.extend_from_slice(&0u32.to_le_bytes());
            let start = pairs.first().map_or(range.0, |p| p.0);
            b.extend_from_slice(&start.to_le_bytes());
            b.extend_from_slice(&range.1.to_le_bytes());
            b.push(u8::try_from(name.len()).unwrap());
            b.extend_from_slice(name.as_bytes());
            pad4(&mut b);
            let at = b.len();
            b[ln_at..ln_at + 4].copy_from_slice(&u32_of(at).to_le_bytes());
            b.extend_from_slice(&seg.to_le_bytes());
            b.extend_from_slice(&u16::try_from(pairs.len()).unwrap().to_le_bytes());
            for (o, _) in *pairs {
                b.extend_from_slice(&o.to_le_bytes());
            }
            for (_, l) in *pairs {
                b.extend_from_slice(&l.to_le_bytes());
            }
            pad4(&mut b);
        }
        b
    }

    fn c11_pairs(b: &[u8]) -> Vec<(String, Vec<(u32, u16)>)> {
        let w16 = |at: usize| usize::from(u16::from_le_bytes([b[at], b[at + 1]]));
        let w32 = |at: usize| u32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]]);
        let mut out = Vec::new();
        for i in 0..w16(0) {
            let base = usize::try_from(w32(4 + 4 * i)).unwrap();
            let cc = w16(base);
            let name = c11_file_name(b, base + 4 + 12 * cc).unwrap();
            let mut pairs = Vec::new();
            for j in 0..cc {
                let at = usize::try_from(w32(base + 4 + 4 * j)).unwrap();
                let n = w16(at + 2);
                for k in 0..n {
                    let line = u16::try_from(w16(at + 4 + 4 * n + 2 * k)).unwrap();
                    pairs.push((w32(at + 4 + 4 * k), line));
                }
            }
            out.push((name, pairs));
        }
        out
    }
}
