use anyhow::{Result, anyhow, bail};
use iced_x86::{ConstantOffsets, Decoder, DecoderOptions, FlowControl, Instruction, OpKind};
use msvc6_base::units::Rva;
use msvc6_image::pe::{Dir, Image};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};

use crate::linkmap::{Map, Row};
use crate::objkey::obj_key;
use crate::verbose::Group;

#[derive(Debug)]
pub struct Link {
    pub img: Image,
    pub map: Map,
    pub groups: Vec<Group>,
}

#[derive(Debug, Default)]
pub struct Residue {
    pub rows: Vec<(usize, usize)>,
    pub reference: Vec<u8>,
}

impl Residue {
    fn written(&self, img: &Image) -> Vec<(usize, usize)> {
        let mut v: Vec<(usize, usize)> = self
            .rows
            .iter()
            .copied()
            .filter(|(o, l)| {
                let r = o.checked_add(*l).and_then(|e| self.reference.get(*o..e));
                let ours = o.checked_add(*l).and_then(|e| img.bytes().get(*o..e));
                r.is_some() && r == ours
            })
            .collect();
        v.sort_unstable();
        v
    }

    #[must_use]
    pub fn written_count(&self, img: &Image) -> usize {
        self.written(img).len()
    }
}

#[derive(Debug, Default, Clone)]
pub struct Renames {
    names: HashMap<String, String>,
    objects: HashMap<String, String>,
}

impl Renames {
    pub fn add(&mut self, old: &str, new: &str) -> Result<()> {
        if old.is_empty() || new.is_empty() {
            bail!("a rename names OLD=NEW, both non-empty");
        }
        let is_obj = |s: &str| s.to_ascii_lowercase().ends_with(".obj");
        if is_obj(old) && is_obj(new) {
            self.objects.insert(obj_key(old), obj_key(new));
        } else {
            self.names.insert(old.to_string(), new.to_string());
        }
        Ok(())
    }

    pub fn parse_arg(&mut self, s: &str) -> Result<()> {
        let (o, n) = s
            .split_once('=')
            .ok_or_else(|| anyhow!("--rename `{s}`: want OLD=NEW"))?;
        self.add(o.trim(), n.trim())
    }

    pub fn parse_file(&mut self, text: &str) -> Result<()> {
        for (k, l) in text.lines().enumerate() {
            let t = l.trim_end_matches('\r');
            if t.trim().is_empty() || t.trim_start().starts_with('#') {
                continue;
            }
            let (o, n) = t
                .split_once('\t')
                .ok_or_else(|| anyhow!("rename line {}: `{t}`: want OLD <tab> NEW", k + 1))?;
            self.add(o.trim(), n.trim())?;
        }
        Ok(())
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.names.is_empty() && self.objects.is_empty()
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.names.len() + self.objects.len()
    }

    fn name<'a>(&'a self, n: &'a str) -> &'a str {
        self.names.get(n).map_or(n, String::as_str)
    }

    fn object(&self, o: &str) -> String {
        let k = obj_key(o);
        self.objects.get(&k).cloned().unwrap_or(k)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum FieldKind {
    Rel32,
    Rel8,
    Disp,
    Imm,
    Table,
}

impl FieldKind {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Rel32 => "rel32 (a decoded call / jmp / jcc)",
            Self::Rel8 => "rel8 (a decoded short branch)",
            Self::Disp => "disp32 (an absolute memory operand)",
            Self::Imm => "imm32 (an absolute immediate)",
            Self::Table => "jump-table entry",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Field {
    pub size: usize,
    pub kind: FieldKind,
    pub value: u32,
}

pub type Fields = BTreeMap<usize, Field>;

#[derive(Clone, Debug)]
struct Body {
    start: u32,
    end: u32,
    rows: Vec<usize>,
    keys: Vec<String>,
    label: String,
    obj: String,
    renamed: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum How {
    Name,
    Renamed,
    FoldStatic,
    Position,
    Content,
}

pub type Tables = BTreeMap<u32, bool>;

#[derive(Debug)]
pub struct Text<'a> {
    pub bytes: &'a [u8],
    pub va: u32,
}

struct View<'a> {
    link: &'a Link,
    ib: u32,
    lo: u32,
    hi: u32,
    code: Vec<(u32, u32)>,
    bodies: Vec<Body>,
    residue: Vec<(usize, usize)>,
    tables: Tables,
}

#[must_use]
pub fn c1_numbered(name: &str) -> bool {
    let s = name.strip_prefix('_').unwrap_or(name);
    let Some(rest) = s.strip_prefix('$') else {
        return false;
    };
    let mut c = rest.chars();
    c.next().is_some_and(|k| k.is_ascii_uppercase())
        && !c.as_str().is_empty()
        && c.as_str().bytes().all(|b| b.is_ascii_digit())
}

fn row_key(r: &Row, folded: &HashSet<String>, renames: Option<&Renames>) -> Option<String> {
    let (name, obj) = match renames {
        Some(m) => (m.name(&r.name).to_string(), m.object(&r.obj)),
        None => (r.name.clone(), obj_key(&r.obj)),
    };
    if c1_numbered(&name) {
        None
    } else if folded.contains(&name) {
        Some(format!("N:{name}"))
    } else {
        Some(format!("O:{name}|{obj}"))
    }
}

impl<'a> View<'a> {
    fn new(
        link: &'a Link,
        folded: &HashSet<String>,
        res: &Residue,
        renames: Option<&Renames>,
    ) -> Result<Self> {
        let img = &link.img;
        let ib = img.opt.image_base.get();
        if link.map.load != 0 && link.map.load != ib {
            bail!(
                "the map's load address {:#x} is not the image base {ib:#x} -- the wrong map?",
                link.map.load
            );
        }
        let mut regions: Vec<(u32, String)> = Vec::new();
        let mut code: Vec<(u32, u32)> = Vec::new();
        for c in link.map.contribs.iter().filter(|c| c.class == "CODE") {
            let s = usize::from(c.sect)
                .checked_sub(1)
                .and_then(|i| img.sections.get(i))
                .ok_or_else(|| anyhow!("map contribution {} names no section", c.name))?;
            regions.push((s.rva.get().saturating_add(c.off), c.name.clone()));
            let sec = (
                s.rva.get(),
                s.rva
                    .get()
                    .saturating_add(u32::try_from(s.disk_len()).unwrap_or(u32::MAX)),
            );
            if !code.contains(&sec) {
                code.push(sec);
            }
        }
        if code.is_empty() {
            bail!("the map has no CODE contribution");
        }
        code.sort_unstable();
        let in_code = |rva: u32| code.iter().any(|(a, e)| rva >= *a && rva < *e);
        let mut starts: BTreeMap<u32, Vec<usize>> = BTreeMap::new();
        for (i, r) in link.map.rows.iter().enumerate() {
            let rva = r.va.wrapping_sub(ib);
            if r.sect != 0 && in_code(rva) {
                starts.entry(rva).or_default().push(i);
            }
        }
        let mut region_at: HashMap<u32, String> = HashMap::new();
        for (a, name) in &regions {
            starts.entry(*a).or_default();
            region_at.insert(*a, name.clone());
        }
        let list: Vec<(u32, Vec<usize>)> = starts.into_iter().collect();
        let mut bodies = Vec::with_capacity(list.len());
        for (i, (start, rows)) in list.iter().enumerate() {
            let sec_end = code
                .iter()
                .find(|(a, e)| start >= a && start < e)
                .map_or(*start, |(_, e)| *e);
            let end = list.get(i + 1).map_or(sec_end, |(n, _)| (*n).min(sec_end));
            let rs: Vec<&Row> = rows.iter().filter_map(|r| link.map.rows.get(*r)).collect();
            let mut keys: Vec<String> = rs
                .iter()
                .filter_map(|r| row_key(r, folded, renames))
                .collect();
            let renamed = renames.is_some_and(|m| {
                rs.iter().any(|r| {
                    m.names.contains_key(&r.name) || m.objects.contains_key(&obj_key(&r.obj))
                })
            });
            let region = region_at.get(start);
            if rows.is_empty()
                && let Some(name) = region
            {
                keys.push(format!("R:{name}"));
            }
            let first = rs.iter().min_by_key(|r| r.is_static);
            bodies.push(Body {
                start: *start,
                end,
                rows: rows.clone(),
                keys,
                label: first.map_or_else(
                    || region.cloned().unwrap_or_else(|| format!("@{start:#x}")),
                    |r| r.name.clone(),
                ),
                obj: first.map_or_else(String::new, |r| match renames {
                    Some(m) => m.object(&r.obj),
                    None => obj_key(&r.obj),
                }),
                renamed,
            });
        }
        let mut v = Self {
            link,
            ib,
            lo: ib,
            hi: ib.saturating_add(img.opt.size_of_image),
            code,
            bodies,
            residue: res.written(img),
            tables: Tables::new(),
        };
        let tables = {
            let texts: Vec<Text<'_>> = v.bodies.iter().filter_map(|b| v.text(b)).collect();
            find_tables(&texts, &|x| v.in_code_va(x))
        };
        v.tables = tables;
        Ok(v)
    }

    fn in_code_va(&self, va: u32) -> bool {
        va >= self.ib && self.is_code(va - self.ib)
    }

    fn text(&self, b: &Body) -> Option<Text<'_>> {
        let (bytes, file) = self.bytes(b)?;
        let content = self.content(bytes, file);
        Some(Text {
            bytes: bytes.get(..content)?,
            va: self.ib.wrapping_add(b.start),
        })
    }

    fn img(&self) -> &Image {
        &self.link.img
    }

    fn is_code(&self, rva: u32) -> bool {
        self.code.iter().any(|(a, e)| rva >= *a && rva < *e)
    }

    fn body_at(&self, rva: u32) -> Option<usize> {
        let i = self
            .bodies
            .partition_point(|b| b.start <= rva)
            .checked_sub(1)?;
        let b = self.bodies.get(i)?;
        (rva < b.end).then_some(i)
    }

    fn bytes(&self, b: &Body) -> Option<(&[u8], usize)> {
        let img = self.img();
        let s = img.section_of(Rva(b.start))?;
        let off = s
            .raw_off
            .get()
            .checked_add(usize::try_from(b.start - s.rva.get()).ok()?)?;
        let len = usize::try_from(b.end.checked_sub(b.start)?).ok()?;
        Some((img.bytes().get(off..off.checked_add(len)?)?, off))
    }

    fn content(&self, bytes: &[u8], file: usize) -> usize {
        let mut t = bytes.len();
        loop {
            while t > 0 && matches!(bytes.get(t - 1), Some(0x90 | 0xcc)) {
                t -= 1;
            }
            let at = file + t;
            let hit = self
                .residue
                .iter()
                .find(|(o, l)| *o >= file && *o < at && at <= o.saturating_add(*l));
            match hit {
                Some((o, _)) => t = o - file,
                None => break,
            }
        }
        t
    }

    fn masked(&self, b: &Body) -> Option<Vec<u8>> {
        let t = self.text(b)?;
        if t.bytes.len() < 4 {
            return None;
        }
        let fields = decode_fields(&t, self.lo, self.hi, &in_body(&self.tables, &t), &|x| {
            self.in_code_va(x)
        });
        let mut m = t.bytes.to_vec();
        for (at, f) in &fields {
            for k in *at..at.saturating_add(f.size) {
                if let Some(x) = m.get_mut(k) {
                    *x = 0;
                }
            }
        }
        Some(m)
    }
}

fn dword(b: &[u8], at: usize) -> Option<u32> {
    let s = b.get(at..at.checked_add(4)?)?;
    Some(u32::from_le_bytes([
        *s.first()?,
        *s.get(1)?,
        *s.get(2)?,
        *s.get(3)?,
    ]))
}

#[must_use]
pub fn find_tables(bodies: &[Text<'_>], in_code: &dyn Fn(u32) -> bool) -> Tables {
    let dword_at = |va: u32| -> Option<u32> {
        let i = bodies.partition_point(|t| t.va <= va).checked_sub(1)?;
        let t = bodies.get(i)?;
        dword(t.bytes, usize::try_from(va - t.va).ok()?)
    };
    let is_start = |va: u32| bodies.binary_search_by_key(&va, |t| t.va).is_ok();
    let mut tables: Tables = BTreeMap::new();
    for _ in 0..4 {
        let mut found: Tables = BTreeMap::new();
        for t in bodies {
            let local = in_body(&tables, t);
            walk(t, &local, in_code, |ins, co, _| {
                if !(co.has_displacement() && co.displacement_size() == 4) {
                    return;
                }
                let d = ins.memory_displacement32();
                if !in_code(d) {
                    return;
                }
                if ins.flow_control() == FlowControl::IndirectBranch
                    && ins.memory_index_scale() == 4
                {
                    if let Some(mut s) = (0..4u32)
                        .map(|k| d.wrapping_add(4 * k))
                        .find(|s| dword_at(*s).is_some_and(in_code))
                    {
                        while let Some(prev) = s.checked_sub(4).filter(|p| {
                            dword_at(*p).is_some_and(|v| is_start(v) && v.abs_diff(*p) <= 0x400)
                        }) {
                            s = prev;
                        }
                        found.insert(s, true);
                    }
                } else if matches!(ins.memory_size().size(), 1 | 2) {
                    found.entry(d).or_insert(false);
                }
            });
        }
        if found == tables {
            break;
        }
        tables = found;
    }
    tables
}

#[must_use]
pub fn in_body(tables: &Tables, t: &Text<'_>) -> BTreeMap<usize, bool> {
    let end = u32::try_from(t.bytes.len()).map_or(t.va, |n| t.va.saturating_add(n));
    tables
        .range(t.va..end)
        .filter_map(|(v, jt)| Some((usize::try_from(v - t.va).ok()?, *jt)))
        .collect()
}

#[must_use]
pub fn decode_fields(
    t: &Text<'_>,
    lo: u32,
    hi: u32,
    tables: &BTreeMap<usize, bool>,
    in_code: &dyn Fn(u32) -> bool,
) -> Fields {
    let mut out = Fields::new();
    let entries = walk(t, tables, in_code, |ins, co, off| {
        let len = ins.len();
        if ins.op0_kind() == OpKind::NearBranch32 {
            let short = ins.is_jmp_short()
                || ins.is_jcc_short()
                || ins.is_loop()
                || ins.is_loopcc()
                || ins.is_jcx_short();
            let (size, kind) = if short {
                (1, FieldKind::Rel8)
            } else {
                (4, FieldKind::Rel32)
            };
            if let (Some(at), Ok(value)) = (
                (off + len).checked_sub(size),
                u32::try_from(ins.near_branch_target()),
            ) {
                out.insert(at, Field { size, kind, value });
            }
        }
        if co.has_displacement() && co.displacement_size() == 4 {
            let v = ins.memory_displacement32();
            if v >= lo && v < hi {
                out.insert(
                    off + co.displacement_offset(),
                    Field {
                        size: 4,
                        kind: FieldKind::Disp,
                        value: v,
                    },
                );
            }
        }
        if co.has_immediate() && co.immediate_size() == 4 {
            let at = off + co.immediate_offset();
            if let Some(v) = dword(t.bytes, at).filter(|v| *v >= lo && *v < hi) {
                out.insert(
                    at,
                    Field {
                        size: 4,
                        kind: FieldKind::Imm,
                        value: v,
                    },
                );
            }
        }
    });
    for (at, f) in entries {
        out.insert(at, f);
    }
    out
}

fn walk(
    t: &Text<'_>,
    tables: &BTreeMap<usize, bool>,
    in_code: &dyn Fn(u32) -> bool,
    mut f: impl FnMut(&Instruction, &ConstantOffsets, usize),
) -> Vec<(usize, Field)> {
    let bytes = t.bytes;
    let mut entries = Vec::new();
    let mut pos = 0usize;
    while pos < bytes.len() {
        let next = tables.range(pos + 1..).next().map(|(k, _)| *k);
        if let Some(jt) = tables.get(&pos) {
            let mut p = pos;
            if *jt {
                while let Some(v) = dword(bytes, p).filter(|v| in_code(*v)) {
                    entries.push((
                        p,
                        Field {
                            size: 4,
                            kind: FieldKind::Table,
                            value: v,
                        },
                    ));
                    p += 4;
                    if next.is_some_and(|x| p >= x) {
                        break;
                    }
                }
            }
            pos = if p > pos {
                p
            } else {
                next.unwrap_or(bytes.len())
            };
            continue;
        }
        let stop = next.unwrap_or(bytes.len());
        let (Some(win), Ok(p32)) = (bytes.get(pos..stop), u32::try_from(pos)) else {
            break;
        };
        let mut d = Decoder::with_ip(
            32,
            win,
            u64::from(t.va.wrapping_add(p32)),
            DecoderOptions::NONE,
        );
        let mut ins = Instruction::default();
        while d.can_decode() {
            let at = pos + d.position();
            d.decode_out(&mut ins);
            if ins.is_invalid() {
                continue;
            }
            let co = d.get_constant_offsets(&ins);
            f(&ins, &co, at);
        }
        pos = stop;
    }
    entries
}

fn cover(f: &Fields, i: usize) -> Option<(usize, Field)> {
    let (p, x) = f.range(..=i).next_back()?;
    (i < p + x.size).then_some((*p, *x))
}

struct Side<'a> {
    bytes: &'a [u8],
    start: u32,
    content: usize,
    fields: Fields,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Cls {
    Same,
    Field(FieldKind),
    Pad,
    Unexplained,
}

#[derive(Debug, Default)]
struct Pairing {
    b2n: Vec<Option<(usize, How)>>,
    n2b: Vec<Option<usize>>,
    split: Vec<(usize, Vec<usize>)>,
    merged: Vec<(usize, Vec<usize>)>,
}

fn static_groups(groups: &[&Group]) -> Vec<BTreeSet<String>> {
    let mut out: Vec<BTreeSet<String>> = Vec::new();
    for g in groups {
        let objs: BTreeSet<String> = g
            .members()
            .filter(|m| m.name == ".text")
            .map(|m| obj_key(&m.obj))
            .collect();
        if objs.len() > 1 && !out.contains(&objs) {
            out.push(objs);
        }
    }
    out
}

fn set_pair(p: &mut Pairing, i: usize, j: usize, how: How) {
    if let Some(s) = p.b2n.get_mut(i) {
        *s = Some((j, how));
    }
    if let Some(s) = p.n2b.get_mut(j) {
        *s = Some(i);
    }
}

fn pair(b: &View<'_>, n: &View<'_>, static_sets: &[BTreeSet<String>], content: bool) -> Pairing {
    let mut p = Pairing {
        b2n: vec![None; b.bodies.len()],
        n2b: vec![None; n.bodies.len()],
        ..Pairing::default()
    };
    let mut index: HashMap<&str, Option<usize>> = HashMap::new();
    for (j, body) in n.bodies.iter().enumerate() {
        for k in &body.keys {
            index
                .entry(k.as_str())
                .and_modify(|e| {
                    if *e != Some(j) {
                        *e = None;
                    }
                })
                .or_insert(Some(j));
        }
    }
    let mut tentative: BTreeMap<usize, Vec<usize>> = BTreeMap::new();
    for (i, body) in b.bodies.iter().enumerate() {
        let to: BTreeSet<usize> = body
            .keys
            .iter()
            .filter_map(|k| index.get(k.as_str()).copied().flatten())
            .collect();
        match to.len() {
            0 => {}
            1 => {
                if let Some(j) = to.first() {
                    tentative.entry(*j).or_default().push(i);
                }
            }
            _ => p.split.push((i, to.into_iter().collect())),
        }
    }
    for (j, is) in tentative {
        if let [i] = is.as_slice() {
            let how = if b.bodies.get(*i).is_some_and(|x| x.renamed) {
                How::Renamed
            } else {
                How::Name
            };
            set_pair(&mut p, *i, j, how);
        } else {
            p.merged.push((j, is));
        }
    }
    for (i, body) in b.bodies.iter().enumerate() {
        if p.b2n.get(i).copied().flatten().is_some() {
            continue;
        }
        let found = body
            .rows
            .iter()
            .filter_map(|r| b.link.map.rows.get(*r))
            .filter(|r| r.is_static)
            .find_map(|r| {
                let mine = obj_key(&r.obj);
                static_sets
                    .iter()
                    .filter(|s| s.contains(&mine))
                    .flat_map(|s| s.iter())
                    .filter(|o| **o != mine)
                    .find_map(|o| {
                        let k = format!("O:{}|{o}", r.name);
                        index
                            .get(k.as_str())
                            .copied()
                            .flatten()
                            .filter(|j| p.n2b.get(*j).copied().flatten().is_none())
                    })
            });
        if let Some(j) = found {
            set_pair(&mut p, i, j, How::FoldStatic);
        }
    }
    let mut i = 0usize;
    while i < b.bodies.len() {
        if p.b2n.get(i).copied().flatten().is_some() {
            i += 1;
            continue;
        }
        let run_end = (i..b.bodies.len())
            .find(|x| p.b2n.get(*x).copied().flatten().is_some())
            .unwrap_or(b.bodies.len());
        let len = run_end - i;
        let after = match i.checked_sub(1) {
            None => Some(0),
            Some(prev) => p.b2n.get(prev).copied().flatten().map(|(j, _)| j + 1),
        };
        let before = if run_end == b.bodies.len() {
            n.bodies.len().checked_sub(len)
        } else {
            p.b2n
                .get(run_end)
                .copied()
                .flatten()
                .and_then(|(j, _)| j.checked_sub(len))
        };
        let paired_n = |y: usize| p.n2b.get(y).is_none_or(Option::is_some);
        let fits = |j0: usize| {
            (j0..j0 + len).all(|y| p.n2b.get(y).is_some_and(Option::is_none))
                && paired_n(j0 + len)
                && j0.checked_sub(1).is_none_or(paired_n)
                && (i..run_end).zip(j0..j0 + len).all(|(x, y)| {
                    match (b.bodies.get(x), n.bodies.get(y)) {
                        (Some(bx), Some(ny)) => bx.obj == ny.obj && !bx.obj.is_empty(),
                        _ => false,
                    }
                })
        };
        if let Some(j0) = [after, before].into_iter().flatten().find(|j0| fits(*j0)) {
            for (x, y) in (i..run_end).zip(j0..j0 + len) {
                set_pair(&mut p, x, y, How::Position);
            }
        }
        i = run_end;
    }
    if content {
        let mut by_content: HashMap<Vec<u8>, (Option<usize>, Option<usize>)> = HashMap::new();
        for (i, body) in b.bodies.iter().enumerate() {
            if p.b2n.get(i).copied().flatten().is_none()
                && let Some(m) = b.masked(body)
            {
                let e = by_content.entry(m).or_insert((None, None));
                e.0 = if e.0.is_none() {
                    Some(i)
                } else {
                    Some(usize::MAX)
                };
            }
        }
        for (j, body) in n.bodies.iter().enumerate() {
            if p.n2b.get(j).copied().flatten().is_none()
                && let Some(m) = n.masked(body)
                && let Some(e) = by_content.get_mut(&m)
            {
                e.1 = if e.1.is_none() {
                    Some(j)
                } else {
                    Some(usize::MAX)
                };
            }
        }
        let mut unique: Vec<(usize, usize)> = by_content
            .into_values()
            .filter_map(|(i, j)| Some((i?, j?)))
            .filter(|&(i, j)| i != usize::MAX && j != usize::MAX)
            .collect();
        unique.sort_unstable();
        for (i, j) in unique {
            set_pair(&mut p, i, j, How::Content);
        }
    }
    p
}

struct Xlate<'a> {
    b: &'a View<'a>,
    n: &'a View<'a>,
    p: &'a Pairing,
}

impl Xlate<'_> {
    fn va(&self, v: u32) -> Option<u32> {
        if v < self.b.lo || v >= self.b.hi {
            return None;
        }
        let rva = v - self.b.ib;
        if self.b.is_code(rva) {
            let i = self.b.body_at(rva)?;
            let (j, _) = self.p.b2n.get(i).copied().flatten()?;
            let (bb, nb) = (self.b.bodies.get(i)?, self.n.bodies.get(j)?);
            return Some(
                self.n
                    .ib
                    .wrapping_add(nb.start)
                    .wrapping_add(rva - bb.start),
            );
        }
        Some(rva.wrapping_add(self.n.ib))
    }

    fn sides(&self, i: usize, j: usize) -> Option<(Side<'_>, Side<'_>)> {
        let (bb, nb) = (self.b.bodies.get(i)?, self.n.bodies.get(j)?);
        let side = |v: &View<'_>, body: &Body| -> Option<(usize, Fields)> {
            let t = v.text(body)?;
            let fields = decode_fields(&t, v.lo, v.hi, &in_body(&v.tables, &t), &|x| {
                v.in_code_va(x)
            });
            Some((t.bytes.len(), fields))
        };
        let (cb, fb) = side(self.b, bb)?;
        let (cn, fnw) = side(self.n, nb)?;
        Some((
            Side {
                bytes: self.b.bytes(bb)?.0,
                start: bb.start,
                content: cb,
                fields: fb,
            },
            Side {
                bytes: self.n.bytes(nb)?.0,
                start: nb.start,
                content: cn,
                fields: fnw,
            },
        ))
    }

    fn classify(&self, s: &(Side<'_>, Side<'_>), i: usize) -> Cls {
        let (b, n) = s;
        let (x, y) = (b.bytes.get(i), n.bytes.get(i));
        match (i < b.content, i < n.content) {
            (true, true) => {
                if x == y {
                    return Cls::Same;
                }
                if let (Some((pb, fb)), Some((pn, fnw))) =
                    (cover(&b.fields, i), cover(&n.fields, i))
                    && pb == pn
                    && fb.size == fnw.size
                    && fb.kind == fnw.kind
                    && self.va(fb.value) == Some(fnw.value)
                {
                    return Cls::Field(fnw.kind);
                }
                Cls::Unexplained
            }
            (false, false) => {
                if x == y {
                    Cls::Same
                } else {
                    Cls::Pad
                }
            }
            _ => Cls::Unexplained,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum Owner {
    MovedBody,
    CodeField(FieldKind),
    DataPointer,
    Export,
    Pad,
    SectionTail,
    Header(&'static str),
    Unclassified,
}

impl Owner {
    #[must_use]
    pub fn label(self) -> String {
        match self {
            Self::MovedBody => "MOVED-BODY (the body's own byte, carried along)".into(),
            Self::CodeField(k) => format!("ADDRESS-FIELD code {}", k.label()),
            Self::DataPointer => "ADDRESS-FIELD data (a dword naming moved code)".into(),
            Self::Export => "ADDRESS-FIELD export (an export rva)".into(),
            Self::Pad => "PAD (fill / declared text-pad residue past a body)".into(),
            Self::SectionTail => {
                "SECTION-TAIL (BASE's last bytes, moved down past NEW's shorter end)".into()
            }
            Self::Header(f) => format!("HEADER {f}"),
            Self::Unclassified => "UNCLASSIFIED".into(),
        }
    }
}

#[derive(Clone, Debug, Serialize)]
pub struct Unclassified {
    pub off: usize,
    pub rva: Option<u32>,
    pub at: String,
    pub base: u8,
    pub new: u8,
}

#[derive(Clone, Debug, Serialize)]
pub struct Changed {
    pub label: String,
    pub base: u32,
    pub new: u32,
    pub bytes: usize,
    pub first: usize,
    pub content: (usize, usize),
    pub base_bytes: String,
    pub new_bytes: String,
}

#[derive(Clone, Debug, Serialize)]
pub struct Misrouted {
    pub body: String,
    pub rel: usize,
    pub kind: FieldKind,
    pub base: u32,
    pub want: Option<u32>,
    pub new: u32,
}

#[derive(Clone, Debug, Serialize)]
pub struct SurvivorMove {
    pub group: usize,
    pub members: usize,
    pub base: (String, String, Option<u32>),
    pub new: (String, String, Option<u32>),
}

#[derive(Debug, Default, Serialize)]
pub struct Report {
    pub differing: usize,
    pub runs: usize,
    pub longest: usize,
    pub bytes_by: BTreeMap<Owner, (usize, BTreeSet<u64>)>,
    pub unclassified: Vec<Unclassified>,
    pub groups: (usize, usize),
    pub dead_groups: usize,
    pub survivors: Vec<SurvivorMove>,
    pub unmatched_groups: Vec<String>,
    pub bodies: (usize, usize),
    pub paired: BTreeMap<How, usize>,
    pub renamed: Vec<(String, String, u32, How)>,
    pub gone: Vec<(String, u32)>,
    pub only_new: Vec<(String, u32)>,
    pub split: Vec<String>,
    pub merged: Vec<String>,
    pub moved: BTreeMap<i64, usize>,
    pub moved_list: Vec<(String, u32, u32)>,
    pub out_of_order: Vec<(String, String, u32, u32)>,
    pub fields_followed: BTreeMap<FieldKind, usize>,
    pub misrouted: Vec<Misrouted>,
    pub changed: Vec<Changed>,
    pub data_followed: usize,
    pub stale: Vec<(u32, u32, u32, String)>,
    pub data_moved: Vec<(String, u32, u32)>,
}

impl Report {
    #[must_use]
    pub fn findings(&self) -> usize {
        self.unclassified.len()
            + self.changed.len()
            + self.misrouted.len()
            + self.gone.len()
            + self.only_new.len()
            + self.split.len()
            + self.merged.len()
            + self.out_of_order.len()
            + self.stale.len()
            + self.data_moved.len()
            + self.unmatched_groups.len()
    }
}

fn hexs(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn data_label(v: &View<'_>, rva: u32) -> String {
    let va = rva.wrapping_add(v.ib);
    let sec = v
        .img()
        .section_of(Rva(rva))
        .map_or_else(|| "?".to_string(), |s| s.name.clone());
    let row = v
        .link
        .map
        .rows
        .iter()
        .filter(|r| r.va <= va && r.sect != 0)
        .max_by_key(|r| r.va);
    match row {
        Some(r) if va - r.va < 0x1_0000 => format!("{sec} {}+{:#x}", r.name, va - r.va),
        _ => sec,
    }
}

fn header_field(img: &Image, off: usize) -> Option<&'static str> {
    let coff = img.e_lfanew.saturating_add(4);
    let opt = img.opt_off;
    let within = |a: usize, n: usize| off >= a && off < a.saturating_add(n);
    if within(coff + 4, 4) {
        return Some("COFF TimeDateStamp");
    }
    for (o, name) in [
        (4, "SizeOfCode"),
        (8, "SizeOfInitializedData"),
        (12, "SizeOfUninitializedData"),
        (16, "AddressOfEntryPoint"),
        (56, "SizeOfImage"),
        (64, "CheckSum"),
    ] {
        if within(opt + o, 4) {
            return Some(name);
        }
    }
    let sec0 = opt.saturating_add(usize::from(img.opt_size));
    for i in 0..img.sections.len() {
        let s = sec0.saturating_add(i.saturating_mul(40));
        if within(s + 8, 4) {
            return Some("section VirtualSize");
        }
        if within(s + 16, 4) {
            return Some("section SizeOfRawData");
        }
    }
    None
}

fn exports(img: &Image) -> (Option<(u32, u32)>, Option<u32>) {
    let Some(d) = img.present(Dir::Export) else {
        return (None, None);
    };
    (crate::iltdiff::eat_of(img), d.rva.get().checked_add(4))
}

fn file_to_rva(img: &Image, off: usize) -> Option<(u32, bool)> {
    img.sections.iter().find_map(|s| {
        let o = s.raw_off.get();
        let raw = usize::try_from(s.raw_size.get()).ok()?;
        if off < o || off >= o.saturating_add(raw) {
            return None;
        }
        let d = u32::try_from(off - o).ok()?;
        Some((s.rva.get().saturating_add(d), off - o < s.disk_len()))
    })
}

fn shrunk_tail(base: &Image, new: &Image, off: usize) -> Option<usize> {
    let sn = new.sections.iter().find(|s| {
        let o = s.raw_off.get();
        let raw = usize::try_from(s.raw_size.get()).unwrap_or(0);
        off >= o && off < o.saturating_add(raw)
    })?;
    let sb = base
        .sections
        .iter()
        .find(|s| s.raw_off.get() == sn.raw_off.get() && s.rva.get() == sn.rva.get())?;
    let (vb, vn) = (sb.disk_len(), sn.disk_len());
    let rel = off.checked_sub(sn.raw_off.get())?;
    if vn >= vb || rel < vn || rel >= vb {
        return None;
    }
    (*new.bytes().get(off)? == 0).then_some(vb - vn)
}

fn carried_down(
    x: &Xlate<'_>,
    sides: &HashMap<usize, (Side<'_>, Side<'_>)>,
    rva: u32,
    delta: usize,
) -> bool {
    let Some(i) = x.b.body_at(rva) else {
        return false;
    };
    let Some((j, _)) = x.p.b2n.get(i).copied().flatten() else {
        return false;
    };
    let (Some(bb), Some(nb), Some(s)) = (x.b.bodies.get(i), x.n.bodies.get(j), sides.get(&j))
    else {
        return false;
    };
    let moved = bb
        .start
        .checked_sub(nb.start)
        .and_then(|d| usize::try_from(d).ok());
    let rel = usize::try_from(rva - bb.start).unwrap_or(usize::MAX);
    moved == Some(delta) && x.classify(s, rel) != Cls::Unexplained
}

fn match_groups<'g>(
    bg: &'g [Group],
    ng: &'g [Group],
) -> (Vec<(&'g Group, &'g Group)>, Vec<&'g Group>) {
    let key = |g: &Group| -> BTreeSet<(String, String)> {
        g.members()
            .map(|m| (m.name.clone(), obj_key(&m.obj)))
            .collect()
    };
    let nkeys: Vec<BTreeSet<(String, String)>> = ng.iter().map(key).collect();
    let mut used = vec![false; ng.len()];
    let mut out = Vec::new();
    let mut lone = Vec::new();
    for g in bg {
        let k = key(g);
        let hit = nkeys
            .iter()
            .enumerate()
            .find(|(j, nk)| !used.get(*j).copied().unwrap_or(true) && !nk.is_disjoint(&k))
            .map(|(j, _)| j);
        match hit.and_then(|j| ng.get(j).map(|x| (j, x))) {
            Some((j, x)) => {
                if let Some(u) = used.get_mut(j) {
                    *u = true;
                }
                out.push((g, x));
            }
            None => lone.push(g),
        }
    }
    for (j, g) in ng.iter().enumerate() {
        if !used.get(j).copied().unwrap_or(true) {
            lone.push(g);
        }
    }
    (out, lone)
}

#[derive(Debug)]
pub struct Options<'a> {
    pub renames: Option<&'a Renames>,
    pub content: bool,
}

#[allow(clippy::too_many_lines)]
pub fn compare(bl: &Link, nl: &Link, res: &Residue, opt: &Options<'_>) -> Result<Report> {
    let (bi, ni) = (&bl.img, &nl.img);
    if bi.len() != ni.len() {
        bail!(
            "the two images differ in length ({} vs {} bytes): not a placement-only change",
            bi.len(),
            ni.len()
        );
    }
    if bi.sections.len() != ni.sections.len()
        || bi
            .sections
            .iter()
            .zip(&ni.sections)
            .any(|(a, b)| a.rva != b.rva || a.raw_off != b.raw_off || a.name != b.name)
    {
        bail!("the two images' section tables differ in placement: not a placement-only change");
    }
    let all_groups: Vec<&Group> = bl.groups.iter().chain(&nl.groups).collect();
    let mut folded: HashSet<String> = all_groups
        .iter()
        .flat_map(|g| g.members())
        .filter(|m| m.name != ".text")
        .map(|m| m.name.clone())
        .collect();
    if let Some(r) = opt.renames {
        let more: Vec<String> = folded.iter().map(|n| r.name(n).to_string()).collect();
        folded.extend(more);
        folded.extend(r.names.values().cloned());
    }
    let b = View::new(bl, &folded, res, opt.renames)?;
    let n = View::new(nl, &folded, res, None)?;
    let statics = static_groups(&all_groups);
    let p = pair(&b, &n, &statics, opt.content);
    let x = Xlate {
        b: &b,
        n: &n,
        p: &p,
    };
    let mut rep = Report {
        groups: (bl.groups.len(), nl.groups.len()),
        bodies: (b.bodies.len(), n.bodies.len()),
        ..Report::default()
    };
    let mut sides: HashMap<usize, (Side<'_>, Side<'_>)> = HashMap::new();
    for (i, e) in p.b2n.iter().enumerate() {
        let Some((j, how)) = e else {
            if let Some(body) = b.bodies.get(i) {
                rep.gone.push((body.label.clone(), body.start));
            }
            continue;
        };
        *rep.paired.entry(*how).or_insert(0) += 1;
        let (Some(bb), Some(nb)) = (b.bodies.get(i), n.bodies.get(*j)) else {
            continue;
        };
        if matches!(how, How::Position | How::Content | How::Renamed) && bb.label != nb.label {
            rep.renamed
                .push((bb.label.clone(), nb.label.clone(), nb.start, *how));
        }
        if bb.start != nb.start {
            *rep.moved
                .entry(i64::from(nb.start) - i64::from(bb.start))
                .or_insert(0) += 1;
            rep.moved_list.push((nb.label.clone(), bb.start, nb.start));
        }
        if let Some(s) = x.sides(i, *j) {
            sides.insert(*j, s);
        }
    }
    for (j, e) in p.n2b.iter().enumerate() {
        if e.is_none()
            && let Some(body) = n.bodies.get(j)
        {
            rep.only_new.push((body.label.clone(), body.start));
        }
    }
    let label = |v: &View<'_>, i: usize| {
        v.bodies
            .get(i)
            .map_or_else(String::new, |x| format!("{}@{:#x}", x.label, x.start))
    };
    rep.split = p
        .split
        .iter()
        .map(|(i, js)| {
            format!(
                "{} -> {}",
                label(&b, *i),
                js.iter()
                    .map(|j| label(&n, *j))
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        })
        .collect();
    rep.merged = p
        .merged
        .iter()
        .map(|(j, is)| {
            format!(
                "{} <- {}",
                label(&n, *j),
                is.iter()
                    .map(|i| label(&b, *i))
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        })
        .collect();
    for j in 0..n.bodies.len() {
        let Some(s) = sides.get(&j) else { continue };
        let Some(nb) = n.bodies.get(j) else { continue };
        let len = s.0.bytes.len().max(s.1.bytes.len());
        let bad: Vec<usize> = (0..len)
            .filter(|i| x.classify(s, *i) == Cls::Unexplained)
            .collect();
        if let Some(first) = bad.first().copied() {
            let lo = first.saturating_sub(2);
            let snip = |b: &[u8]| hexs(b.get(lo..(lo + 12).min(b.len())).unwrap_or(&[]));
            rep.changed.push(Changed {
                label: nb.label.clone(),
                base: s.0.start,
                new: s.1.start,
                bytes: bad.len(),
                first,
                content: (s.0.content, s.1.content),
                base_bytes: snip(s.0.bytes),
                new_bytes: snip(s.1.bytes),
            });
            continue;
        }
        for (at, fb) in &s.0.fields {
            let Some(fnw) = s.1.fields.get(at) else {
                continue;
            };
            if fnw.size != fb.size || fnw.kind != fb.kind {
                continue;
            }
            *rep.fields_followed.entry(fb.kind).or_insert(0) += 1;
            let want = x.va(fb.value);
            if want != Some(fnw.value) {
                rep.misrouted.push(Misrouted {
                    body: nb.label.clone(),
                    rel: *at,
                    kind: fb.kind,
                    base: fb.value,
                    want,
                    new: fnw.value,
                });
            }
        }
    }
    let (pairs, lone) = match_groups(&bl.groups, &nl.groups);
    let fold_static: Vec<(u32, String, u32, String)> = p
        .b2n
        .iter()
        .enumerate()
        .filter_map(|(i, e)| match e {
            Some((j, How::FoldStatic)) => {
                let (bb, nb) = (b.bodies.get(i)?, n.bodies.get(*j)?);
                Some((bb.start, bb.obj.clone(), nb.start, nb.obj.clone()))
            }
            _ => None,
        })
        .collect();
    let addr = |v: &View<'_>, name: &str| -> Option<u32> {
        v.link
            .map
            .rows
            .iter()
            .find(|r| r.name == name)
            .map(|r| r.va.wrapping_sub(v.ib))
    };
    let static_addr = |ob: &str, on: &str| -> (Option<u32>, Option<u32>) {
        let (ob, on) = (obj_key(ob), obj_key(on));
        fold_static
            .iter()
            .find(|(_, bo, _, no)| *bo == ob && *no == on)
            .map_or((None, None), |(a, _, z, _)| (Some(*a), Some(*z)))
    };
    let code_objs: HashSet<String> = [&b, &n]
        .iter()
        .flat_map(|v| v.bodies.iter().map(|x| x.obj.clone()))
        .collect();
    let in_map = |g: &Group| {
        g.members().any(|m| {
            if m.name == ".text" {
                code_objs.contains(&obj_key(&m.obj))
            } else {
                bl.map
                    .rows
                    .iter()
                    .chain(&nl.map.rows)
                    .any(|r| r.name == m.name)
            }
        })
    };
    for (gb, gn) in &pairs {
        if !in_map(gb) && !in_map(gn) {
            rep.dead_groups += 1;
            continue;
        }
        let (sb, sn) = (&gb.survivor, &gn.survivor);
        if sb.name != sn.name || obj_key(&sb.obj) != obj_key(&sn.obj) {
            let (ab, an) = if sb.name == ".text" || sn.name == ".text" {
                static_addr(&sb.obj, &sn.obj)
            } else {
                (addr(&b, &sb.name), addr(&n, &sn.name))
            };
            rep.survivors.push(SurvivorMove {
                group: bl
                    .groups
                    .iter()
                    .position(|g| std::ptr::eq(g, *gb))
                    .map_or(0, |i| i + 1),
                members: gn.replaced.len() + 1,
                base: (sb.name.clone(), sb.obj.clone(), ab),
                new: (sn.name.clone(), sn.obj.clone(), an),
            });
        }
    }
    for g in lone {
        if in_map(g) {
            rep.unmatched_groups
                .push(format!("{} {}", g.survivor.name, g.survivor.obj));
        } else {
            rep.dead_groups += 1;
        }
    }
    let survivor_names: HashSet<String> = pairs
        .iter()
        .filter(|(gb, gn)| {
            gb.survivor.name != gn.survivor.name
                || obj_key(&gb.survivor.obj) != obj_key(&gn.survivor.obj)
        })
        .flat_map(|(gb, gn)| gb.members().chain(gn.members()))
        .map(|m| m.name.clone())
        .collect();
    let survivor_objs: HashSet<String> = pairs
        .iter()
        .filter(|(gb, gn)| obj_key(&gb.survivor.obj) != obj_key(&gn.survivor.obj))
        .flat_map(|(gb, gn)| gb.members().chain(gn.members()))
        .filter(|m| m.name == ".text")
        .map(|m| obj_key(&m.obj))
        .collect();
    let is_survivor = |i: usize, how: How| -> bool {
        let Some(body) = b.bodies.get(i) else {
            return false;
        };
        body.rows
            .iter()
            .filter_map(|r| b.link.map.rows.get(*r))
            .any(|r| {
                survivor_names.contains(&r.name)
                    || (how == How::FoldStatic && survivor_objs.contains(&obj_key(&r.obj)))
            })
    };
    let seq: Vec<(usize, usize)> = p
        .b2n
        .iter()
        .enumerate()
        .filter_map(|(i, e)| e.map(|(j, how)| (i, j, how)))
        .filter(|(i, _, how)| !is_survivor(*i, *how))
        .map(|(i, j, _)| (i, j))
        .collect();
    let js: Vec<usize> = seq.iter().map(|(_, j)| *j).collect();
    let keep = crate::seq::lis_strict(&js);
    for (k, (i, j)) in seq.iter().enumerate() {
        if !keep.contains(&k)
            && let (Some(bb), Some(nb)) = (b.bodies.get(*i), n.bodies.get(*j))
        {
            rep.out_of_order
                .push((nb.label.clone(), nb.obj.clone(), bb.start, nb.start));
        }
    }
    let data_rows = |v: &View<'_>, renames: Option<&Renames>| -> HashMap<String, u32> {
        v.link
            .map
            .rows
            .iter()
            .filter(|r| r.sect != 0 && !v.is_code(r.va.wrapping_sub(v.ib)))
            .map(|r| {
                let (name, obj) = match renames {
                    Some(m) => (m.name(&r.name).to_string(), m.object(&r.obj)),
                    None => (r.name.clone(), obj_key(&r.obj)),
                };
                (format!("{name}|{obj}"), r.va)
            })
            .collect()
    };
    let (db, dn) = (data_rows(&b, opt.renames), data_rows(&n, None));
    for (k, va) in &db {
        if let Some(nv) = dn.get(k).filter(|nv| *nv != va) {
            rep.data_moved.push((k.clone(), *va, *nv));
        }
    }
    rep.data_moved.sort();
    let starts: HashSet<u32> = b
        .bodies
        .iter()
        .enumerate()
        .filter(|(i, body)| {
            p.b2n
                .get(*i)
                .copied()
                .flatten()
                .and_then(|(j, _)| n.bodies.get(j))
                .is_some_and(|nb| nb.start != body.start)
        })
        .map(|(_, body)| body.start.wrapping_add(b.ib))
        .collect();
    for s in &bi.sections {
        if b.is_code(s.rva.get()) {
            continue;
        }
        let mut r = s.rva.get();
        let end = s.rva.get().saturating_add(u32::try_from(s.disk_len())?);
        while r.saturating_add(4) <= end {
            if let Some(vb) = bi.u32_at_rva(Rva(r))
                && starts.contains(&vb)
            {
                rep.data_followed += 1;
                let vn = ni.u32_at_rva(Rva(r));
                let want = x.va(vb);
                if vn != want {
                    rep.stale.push((r, vb, vn.unwrap_or(0), data_label(&n, r)));
                }
            }
            r = r.saturating_add(4);
        }
    }
    let headers_end = bi
        .sections
        .iter()
        .map(|s| s.raw_off.get())
        .filter(|o| *o > 0)
        .min()
        .unwrap_or(0);
    let (eat_b, stamp_b) = exports(bi);
    let mut run = 0usize;
    for (off, (xb, yn)) in bi.bytes().iter().zip(ni.bytes()).enumerate() {
        if xb == yn {
            run = 0;
            continue;
        }
        rep.differing += 1;
        if run == 0 {
            rep.runs += 1;
        }
        run += 1;
        rep.longest = rep.longest.max(run);
        let mut owner = Owner::Unclassified;
        let mut unit: u64 = 0;
        let mut at: String;
        let mut rva_out = None;
        if off < headers_end {
            at = "headers".into();
            if let Some(f) = header_field(ni, off) {
                owner = Owner::Header(f);
                if f == "AddressOfEntryPoint" {
                    let ok = x.va(bi.opt.entry_point.get().wrapping_add(b.ib))
                        == Some(ni.opt.entry_point.get().wrapping_add(n.ib));
                    if !ok {
                        owner = Owner::Unclassified;
                        at = "headers AddressOfEntryPoint (does not translate)".into();
                    }
                }
                unit = u64::try_from(off).unwrap_or(0);
            }
        } else if let Some((rva, on_disk)) = file_to_rva(ni, off) {
            rva_out = Some(rva);
            if !on_disk {
                at = "section file slack".into();
                if let Some(delta) =
                    shrunk_tail(bi, ni, off).filter(|d| carried_down(&x, &sides, rva, *d))
                {
                    owner = Owner::SectionTail;
                    unit = u64::try_from(delta).unwrap_or(0);
                }
            } else if n.is_code(rva) {
                match n.body_at(rva) {
                    None => at = "code outside every body".into(),
                    Some(j) => {
                        let nb = n.bodies.get(j);
                        let rel = nb.map_or(0, |nb| rva - nb.start);
                        at = format!(
                            "{}+{rel:#x}",
                            nb.map_or_else(String::new, |nb| nb.label.clone())
                        );
                        match sides.get(&j) {
                            None => at.push_str(" (a body only in NEW)"),
                            Some(s) => {
                                let rel_u = usize::try_from(rel).unwrap_or(usize::MAX);
                                match x.classify(s, rel_u) {
                                    Cls::Same => {
                                        owner = Owner::MovedBody;
                                        unit = u64::try_from(j).unwrap_or(0);
                                    }
                                    Cls::Field(k) => {
                                        owner = Owner::CodeField(k);
                                        let start =
                                            cover(&s.1.fields, rel_u).map_or(rel_u, |(p, _)| p);
                                        unit = u64::from(nb.map_or(0, |nb| nb.start))
                                            + u64::try_from(start).unwrap_or(0);
                                    }
                                    Cls::Pad => {
                                        owner = Owner::Pad;
                                        unit = u64::try_from(j).unwrap_or(0);
                                    }
                                    Cls::Unexplained => {}
                                }
                            }
                        }
                    }
                }
            } else {
                at = data_label(&n, rva);
                if stamp_b.is_some_and(|t| rva >= t && rva < t.saturating_add(4)) {
                    owner = Owner::Header("export TimeDateStamp");
                    unit = u64::from(rva);
                } else {
                    let a = rva & !3;
                    for p in
                        std::iter::once(a).chain((rva.saturating_sub(3)..=rva).filter(|q| *q != a))
                    {
                        let (Some(vb), Some(vn)) = (bi.u32_at_rva(Rva(p)), ni.u32_at_rva(Rva(p)))
                        else {
                            continue;
                        };
                        if vb == vn {
                            continue;
                        }
                        let in_eat = eat_b.is_some_and(|(s, e)| p >= s && p.saturating_add(4) <= e);
                        if in_eat
                            && b.is_code(vb)
                            && x.va(vb.wrapping_add(b.ib)) == Some(vn.wrapping_add(n.ib))
                        {
                            owner = Owner::Export;
                            unit = u64::from(p);
                            break;
                        }
                        if b.is_code(vb.wrapping_sub(b.ib)) && x.va(vb) == Some(vn) {
                            owner = Owner::DataPointer;
                            unit = u64::from(p);
                            break;
                        }
                    }
                }
            }
        } else {
            at = "outside every section".into();
        }
        if owner == Owner::Unclassified {
            rep.unclassified.push(Unclassified {
                off,
                rva: rva_out,
                at,
                base: *xb,
                new: *yn,
            });
        }
        let e = rep.bytes_by.entry(owner).or_default();
        e.0 += 1;
        if owner != Owner::Unclassified {
            e.1.insert(unit);
        }
    }
    Ok(rep)
}

struct UnRun<'a> {
    first: &'a Unclassified,
    base: Vec<u8>,
    new: Vec<u8>,
}

impl UnRun<'_> {
    fn place(&self) -> &str {
        self.first.at.split('+').next().unwrap_or("")
    }
}

fn unclassified_runs(us: &[Unclassified]) -> Vec<UnRun<'_>> {
    let mut runs: Vec<UnRun<'_>> = Vec::new();
    for u in us {
        match runs.last_mut() {
            Some(r)
                if r.first.off + r.base.len() == u.off
                    && u.at.split('+').next() == Some(r.place()) =>
            {
                r.base.push(u.base);
                r.new.push(u.new);
            }
            _ => runs.push(UnRun {
                first: u,
                base: vec![u.base],
                new: vec![u.new],
            }),
        }
    }
    runs
}

#[derive(Debug)]
pub struct Names<'a> {
    pub base: &'a str,
    pub new: &'a str,
    pub maps: (&'a str, &'a str),
    pub logs: (&'a str, &'a str),
}

#[allow(clippy::too_many_lines)]
#[must_use]
pub fn lines(
    rep: &Report,
    bl: &Link,
    nl: &Link,
    res: &Residue,
    names: &Names<'_>,
    all: bool,
    limit: usize,
) -> Vec<String> {
    let cap = if all { usize::MAX } else { limit };
    let md5 = |i: &Image| msvc6_base::digest::md5_hex(i.bytes());
    let mut out = vec![
        format!("ICF-DIFF base {} ({})", names.base, md5(&bl.img)),
        format!("         new  {} ({})", names.new, md5(&nl.img)),
        format!("  maps {} | {}", names.maps.0, names.maps.1),
        format!("  logs {} | {}", names.logs.0, names.logs.1),
        format!(
            "  text-pad residue rows: {} declared, {} written in base, {} in new",
            res.rows.len(),
            res.written(&bl.img).len(),
            res.written(&nl.img).len()
        ),
        format!(
            "  {} byte(s) differ in {} run(s), the longest {}",
            rep.differing, rep.runs, rep.longest
        ),
    ];
    for (k, (nbytes, units)) in &rep.bytes_by {
        if *k == Owner::Unclassified {
            continue;
        }
        let what = match k {
            Owner::MovedBody | Owner::Pad => "body(ies)",
            Owner::SectionTail => "section(s)",
            _ => "field(s)",
        };
        out.push(format!(
            "    {:<58} {nbytes:>7} byte(s) in {} {what}",
            k.label(),
            units.len()
        ));
    }
    out.push(format!(
        "    {:<58} {:>7} byte(s)",
        "UNCLASSIFIED",
        rep.unclassified.len()
    ));
    let runs = unclassified_runs(&rep.unclassified);
    let mut by_place: BTreeMap<&str, (usize, usize)> = BTreeMap::new();
    for r in &runs {
        let e = by_place.entry(r.place()).or_default();
        e.0 += r.base.len();
        e.1 += 1;
    }
    out.push(format!(
        "      {} run(s) in {} place(s):",
        runs.len(),
        by_place.len()
    ));
    for (place, (nbytes, nruns)) in by_place.iter().take(cap) {
        out.push(format!(
            "        {place}: {nbytes} byte(s) in {nruns} run(s)"
        ));
    }
    for r in runs.iter().take(cap) {
        let show = |b: &[u8]| {
            let h = hexs(b.get(..b.len().min(16)).unwrap_or(&[]));
            if b.len() > 16 { format!("{h}..") } else { h }
        };
        out.push(format!(
            "      file {:#x} rva {} {}: {} byte(s), base {} new {}",
            r.first.off,
            r.first
                .rva
                .map_or_else(|| "-".into(), |v| format!("{v:#x}")),
            r.first.at,
            r.base.len(),
            show(&r.base),
            show(&r.new)
        ));
    }
    out.push(format!(
        "  FOLD GROUPS: {} in base, {} in new ({} dead: no member in either map); {} with another survivor{}",
        rep.groups.0,
        rep.groups.1,
        rep.dead_groups,
        rep.survivors.len(),
        if rep.unmatched_groups.is_empty() {
            String::new()
        } else {
            format!(", {} in one log only", rep.unmatched_groups.len())
        }
    ));
    let at = |a: Option<u32>| a.map_or_else(|| "?".into(), |r| format!("{r:#x}"));
    for s in &rep.survivors {
        out.push(format!(
            "    ICF-SURVIVOR G{} ({} names): base {} {} @{} -> new {} {} @{}",
            s.group,
            s.members,
            s.base.0,
            s.base.1,
            at(s.base.2),
            s.new.0,
            s.new.1,
            at(s.new.2)
        ));
    }
    for g in rep.unmatched_groups.iter().take(cap) {
        out.push(format!("    group in one log only: {g}"));
    }
    let paired: Vec<String> = rep
        .paired
        .iter()
        .map(|(h, c)| format!("{h:?} {c}"))
        .collect();
    out.push(format!(
        "  BODIES: {} in base, {} in new; paired {} ({}); {} only in base, {} only in new; {} split, {} merged",
        rep.bodies.0,
        rep.bodies.1,
        rep.paired.values().sum::<usize>(),
        paired.join(", "),
        rep.gone.len(),
        rep.only_new.len(),
        rep.split.len(),
        rep.merged.len()
    ));
    for (want, by) in [
        (How::Renamed, "rename"),
        (How::Content, "content"),
        (How::Position, "position"),
    ] {
        for (bname, nname, a, _) in rep.renamed.iter().filter(|r| r.3 == want).take(cap) {
            out.push(format!(
                "    paired by {by} (renamed): {bname} -> {nname} @{a:#x}"
            ));
        }
    }
    for (l, a) in rep.gone.iter().take(cap) {
        out.push(format!("    only in base: {l} @{a:#x}"));
    }
    for (l, a) in rep.only_new.iter().take(cap) {
        out.push(format!("    only in new: {l} @{a:#x}"));
    }
    for s in rep.split.iter().take(cap) {
        out.push(format!("    split: {s}"));
    }
    for s in rep.merged.iter().take(cap) {
        out.push(format!("    merged: {s}"));
    }
    let hist: Vec<String> = rep
        .moved
        .iter()
        .map(|(d, c)| format!("{d:+}x{c}"))
        .collect();
    out.push(format!(
        "  MOVED: {} body(ies), by delta: {}",
        rep.moved_list.len(),
        hist.join(" ")
    ));
    if all {
        for (l, a, z) in &rep.moved_list {
            out.push(format!("    {l} {a:#x} -> {z:#x}"));
        }
    }
    out.push(format!(
        "  ORDER: {} body(ies) off the longest run that keeps base order (moved survivors excluded)",
        rep.out_of_order.len()
    ));
    for (l, o, a, z) in rep.out_of_order.iter().take(cap) {
        out.push(format!("    out of order: {l} ({o}) {a:#x} -> {z:#x}"));
    }
    let per: Vec<String> = rep
        .fields_followed
        .iter()
        .map(|(k, c)| format!("{k:?} {c}"))
        .collect();
    out.push(format!(
        "  FIELDS: {} code field(s) of the unchanged pairs followed ({}), {} misrouted",
        rep.fields_followed.values().sum::<usize>(),
        per.join(", "),
        rep.misrouted.len()
    ));
    for m in rep.misrouted.iter().take(cap) {
        out.push(format!(
            "    misrouted: {}+{:#x} {:?} base {:#x} wants {} new {:#x}",
            m.body,
            m.rel,
            m.kind,
            m.base,
            at(m.want),
            m.new
        ));
    }
    out.push(format!(
        "  DATA: {} aligned dword(s) naming a moved body's start followed, {} stale; {} data symbol(s) moved",
        rep.data_followed,
        rep.stale.len(),
        rep.data_moved.len()
    ));
    for (r, vb, vn, l) in rep.stale.iter().take(cap) {
        out.push(format!(
            "    stale: rva {r:#x} {l}: base {vb:#x} new {vn:#x}"
        ));
    }
    for (k, a, z) in rep.data_moved.iter().take(cap) {
        out.push(format!("    data moved: {k} {a:#x} -> {z:#x}"));
    }
    out.push(format!(
        "  CHANGED: {} body(ies) differ from themselves beyond their address fields (not ICF)",
        rep.changed.len()
    ));
    for c in rep.changed.iter().take(cap) {
        out.push(format!(
            "    {} base {:#x} new {:#x}: {} byte(s), content {} -> {}, first at +{:#x}: base {} new {}",
            c.label, c.base, c.new, c.bytes, c.content.0, c.content.1, c.first, c.base_bytes, c.new_bytes
        ));
    }
    out.push(format!(
        "  VERDICT: {}",
        if rep.findings() == 0 {
            "ICF PLACEMENT ONLY -- every differing byte is a moved body's own byte, an address field that \
             follows its target, pad, a section tail carried down, or a header field, and every field and \
             order follows"
        } else {
            "NOT ICF PLACEMENT ONLY -- see the findings above"
        }
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::linkmap::parse as parse_map;
    use crate::verbose::groups as parse_groups;

    const IB: u32 = 0x40_0000;
    const TEXT: u32 = 0x1000;
    const RDATA: u32 = 0x2000;

    fn put32(d: &mut [u8], off: usize, v: u32) {
        d[off..off + 4].copy_from_slice(&v.to_le_bytes());
    }

    fn file(rva: u32) -> usize {
        if rva >= RDATA {
            0x600 + (rva - RDATA) as usize
        } else {
            0x400 + (rva - TEXT) as usize
        }
    }

    fn pe(fill: impl FnOnce(&mut Vec<u8>)) -> Image {
        let mut d = vec![0u8; 0x800];
        d[0..2].copy_from_slice(b"MZ");
        put32(&mut d, 0x3c, 0x80);
        d[0x80..0x84].copy_from_slice(b"PE\0\0");
        d[0x84..0x86].copy_from_slice(&0x14cu16.to_le_bytes());
        d[0x86..0x88].copy_from_slice(&2u16.to_le_bytes());
        d[0x94..0x96].copy_from_slice(&0xe0u16.to_le_bytes());
        let opt = 0x98;
        d[opt..opt + 2].copy_from_slice(&0x10bu16.to_le_bytes());
        put32(&mut d, opt + 16, TEXT);
        put32(&mut d, opt + 28, IB);
        put32(&mut d, opt + 32, 0x1000);
        put32(&mut d, opt + 36, 0x200);
        put32(&mut d, opt + 56, 0x3000);
        put32(&mut d, opt + 60, 0x400);
        put32(&mut d, opt + 92, 16);
        for (i, (name, va, ptr)) in [
            (b".text\0\0\0", TEXT, 0x400u32),
            (b".rdata\0\0", RDATA, 0x600),
        ]
        .iter()
        .enumerate()
        {
            let a = opt + 0xe0 + 40 * i;
            d[a..a + 8].copy_from_slice(*name);
            put32(&mut d, a + 8, 0x200);
            put32(&mut d, a + 12, *va);
            put32(&mut d, a + 16, 0x200);
            put32(&mut d, a + 20, *ptr);
        }
        for b in &mut d[0x400..0x600] {
            *b = 0xcc;
        }
        fill(&mut d);
        Image::parse(d).unwrap()
    }

    fn map(publics: &[(u32, &str, &str)], statics: &[(u32, &str, &str)]) -> String {
        let row = |(rva, name, obj): &(u32, &str, &str)| {
            let (sect, base) = if *rva >= RDATA { (2, RDATA) } else { (1, TEXT) };
            let flag = if sect == 1 { "f  " } else { "   " };
            format!(
                " {sect:04x}:{:08x}       {name:<26} {:08x} {flag} {obj}\r\n",
                rva - base,
                IB + rva
            )
        };
        let mut s = String::from(
            " fixture\r\n\r\n Timestamp is 6acbc485 (Sun Oct 11 17:16:53 2026)\r\n\r\n Preferred load address is \
             00400000\r\n\r\n Start         Length     Name                   Class\r\n 0001:00000000 00000200H \
             .text                   CODE\r\n 0002:00000000 00000200H .rdata                  DATA\r\n\r\n  \
             Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n",
        );
        for p in publics {
            s.push_str(&row(p));
        }
        s.push_str("\r\n entry point at        0001:00000000\r\n\r\n Static symbols\r\n\r\n");
        for p in statics {
            s.push_str(&row(p));
        }
        s
    }

    fn fold_log(survivor: (&str, &str), replaced: (&str, &str)) -> String {
        format!(
            "    Discarded _Dead from a.obj\n    Selected symbol:\n        {} from {}\n    Replaced \
             symbol(s):\n        {} from {}\n",
            survivor.0, survivor.1, replaced.0, replaced.1
        )
    }

    #[derive(Clone, Copy)]
    struct Layout {
        a: u32,
        b: u32,
        j: u32,
        cd: u32,
        b_ret: u32,
        b_slot: Option<u32>,
    }

    const BASE: Layout = Layout {
        a: 0x1000,
        b: 0x1010,
        j: 0x1020,
        cd: 0x1050,
        b_ret: 1,
        b_slot: None,
    };
    const MOVED: Layout = Layout {
        a: 0x1000,
        cd: 0x1010,
        b: 0x1020,
        j: 0x1030,
        b_ret: 1,
        b_slot: None,
    };
    const RESIDUE: [u8; 6] = [0xff, 0x15, 0x00, 0x20, 0x40, 0x00];

    fn image(l: Layout, residue_at_j_pad: bool) -> Image {
        pe(|d| {
            let a = file(l.a);
            d[a] = 0xe8;
            put32(d, a + 1, (l.cd).wrapping_sub(l.a + 5));
            d[a + 5] = 0xc3;
            let b = file(l.b);
            d[b] = 0xb8;
            put32(d, b + 1, l.b_ret);
            d[b + 5] = 0xc3;
            let j = file(l.j);
            d[j..j + 4].copy_from_slice(&[0x8b, 0x44, 0x24, 0x04]);
            d[j + 4..j + 7].copy_from_slice(&[0xff, 0x24, 0x85]);
            put32(d, j + 7, IB + l.j + 0x14);
            d[j + 0xb..j + 0xe].copy_from_slice(&[0x33, 0xc0, 0xc3]);
            d[j + 0xe] = 0xb8;
            put32(d, j + 0xf, 1);
            d[j + 0x13] = 0xc3;
            put32(d, j + 0x14, IB + l.j + 0xb);
            put32(d, j + 0x18, IB + l.j + 0xe);
            if residue_at_j_pad {
                d[j + 0x1c..j + 0x22].copy_from_slice(&RESIDUE);
            }
            let c = file(l.cd);
            d[c..c + 3].copy_from_slice(&[0x33, 0xc0, 0xc3]);
            put32(d, file(RDATA), IB + l.b_slot.unwrap_or(l.b));
            put32(d, file(RDATA) + 4, IB + l.cd);
        })
    }

    fn link_with(
        l: Layout,
        rows: &[(u32, &str, &str)],
        survivor: (&str, &str),
        replaced: (&str, &str),
        residue: bool,
    ) -> Link {
        Link {
            img: image(l, residue),
            map: parse_map(&map(rows, &[])),
            groups: parse_groups(&fold_log(survivor, replaced)),
        }
    }

    fn link(l: Layout, survivor: (&str, &str), replaced: (&str, &str), residue: bool) -> Link {
        let obj = survivor.1;
        link_with(
            l,
            &[
                (l.a, "_A", "a.obj"),
                (l.b, "_B", "b.obj"),
                (l.j, "_J", "b.obj"),
                (l.cd, "_C", obj),
                (l.cd, "_D", obj),
                (RDATA, "??_7V@@6B@", "a.obj"),
            ],
            survivor,
            replaced,
            residue,
        )
    }

    fn base() -> Link {
        link(BASE, ("_C", "c.obj"), ("_D", "d.obj"), false)
    }

    fn moved(l: Layout) -> Link {
        link(l, ("_D", "d.obj"), ("_C", "c.obj"), false)
    }

    fn moved_renamed(l: Layout) -> Link {
        link_with(
            l,
            &[
                (l.a, "_A", "a.obj"),
                (l.b, "_B2", "b2.obj"),
                (l.j, "_J", "b.obj"),
                (l.cd, "_C", "d.obj"),
                (l.cd, "_D", "d.obj"),
                (RDATA, "??_7V@@6B@", "a.obj"),
            ],
            ("_D", "d.obj"),
            ("_C", "c.obj"),
            false,
        )
    }

    fn owned(rep: &Report, o: Owner) -> usize {
        rep.bytes_by.get(&o).map_or(0, |e| e.0)
    }

    fn plain() -> Options<'static> {
        Options {
            renames: None,
            content: true,
        }
    }

    #[test]
    fn a_moved_survivor_and_everything_that_follows_it_is_classified() {
        let rep = compare(&base(), &moved(MOVED), &Residue::default(), &plain()).unwrap();
        assert!(rep.differing > 0);
        assert!(rep.unclassified.is_empty(), "{:?}", rep.unclassified);
        assert_eq!(rep.findings(), 0, "{rep:?}");
        assert_eq!(rep.survivors.len(), 1);
        let s = &rep.survivors[0];
        assert_eq!((s.base.0.as_str(), s.base.2), ("_C", Some(BASE.cd)));
        assert_eq!((s.new.0.as_str(), s.new.2), ("_D", Some(MOVED.cd)));
        assert!(owned(&rep, Owner::CodeField(FieldKind::Rel32)) > 0);
        assert!(owned(&rep, Owner::CodeField(FieldKind::Disp)) > 0);
        assert!(owned(&rep, Owner::CodeField(FieldKind::Table)) > 0);
        assert_eq!(
            rep.bytes_by.get(&Owner::DataPointer).map(|e| e.1.len()),
            Some(2)
        );
        assert!(owned(&rep, Owner::MovedBody) > 0);
        assert_eq!(rep.moved_list.len(), 3);
        assert!(rep.out_of_order.is_empty());
    }

    #[test]
    fn a_renamed_and_moved_body_pairs_by_its_masked_content() {
        let rep = compare(
            &base(),
            &moved_renamed(MOVED),
            &Residue::default(),
            &plain(),
        )
        .unwrap();
        assert!(rep.unclassified.is_empty(), "{:?}", rep.unclassified);
        assert_eq!(rep.findings(), 0, "{rep:?}");
        assert_eq!(rep.paired.get(&How::Content), Some(&1));
        assert_eq!(
            rep.renamed
                .iter()
                .map(|r| (r.0.as_str(), r.1.as_str(), r.3))
                .collect::<Vec<_>>(),
            vec![("_B", "_B2", How::Content)]
        );
        let changed = moved_renamed(Layout { b_ret: 2, ..MOVED });
        let rep = compare(&base(), &changed, &Residue::default(), &plain()).unwrap();
        assert_eq!(rep.paired.get(&How::Content), None);
        assert!(rep.findings() > 0);
        let off = Options {
            renames: None,
            content: false,
        };
        let rep = compare(&base(), &moved_renamed(MOVED), &Residue::default(), &off).unwrap();
        assert!(
            rep.findings() > 0,
            "without content pairing the renamed body is unpaired"
        );
    }

    #[test]
    fn a_rename_map_pairs_a_renamed_body_that_changed_too_and_reports_the_change() {
        let mut r = Renames::default();
        r.parse_arg("_B=_B2").unwrap();
        let opt = Options {
            renames: Some(&r),
            content: false,
        };
        let rep = compare(&base(), &moved_renamed(MOVED), &Residue::default(), &opt).unwrap();
        assert_eq!(rep.findings(), 0, "{rep:?}");
        assert_eq!(rep.paired.get(&How::Renamed), Some(&1));
        let changed = moved_renamed(Layout { b_ret: 2, ..MOVED });
        let rep = compare(&base(), &changed, &Residue::default(), &opt).unwrap();
        assert_eq!(rep.paired.get(&How::Renamed), Some(&1));
        assert_eq!(rep.changed.len(), 1, "{rep:?}");
        assert_eq!(rep.changed[0].label, "_B2");
        assert!(r.parse_arg("nothing").is_err());
        let mut f = Renames::default();
        f.parse_file("# a note\n_B\t_B2\n\nb.obj\tb2.obj\n")
            .unwrap();
        assert_eq!(f.len(), 2);
        assert!(f.parse_file("_B _B2\n").is_err());
    }

    #[test]
    fn a_changed_body_byte_is_unclassified_and_names_its_body() {
        let new = moved(Layout { b_ret: 2, ..MOVED });
        let rep = compare(&base(), &new, &Residue::default(), &plain()).unwrap();
        assert_eq!(rep.unclassified.len(), 1, "{:?}", rep.unclassified);
        assert!(rep.unclassified[0].at.starts_with("_B+0x1"));
        assert_eq!(rep.changed.len(), 1);
        assert_eq!(rep.changed[0].label, "_B");
        assert!(rep.findings() > 0);
    }

    #[test]
    fn a_vftable_slot_that_does_not_follow_its_body_is_stale() {
        let new = moved(Layout {
            b_slot: Some(BASE.b),
            ..MOVED
        });
        let rep = compare(&base(), &new, &Residue::default(), &plain()).unwrap();
        assert!(rep.unclassified.is_empty());
        assert_eq!(rep.stale.len(), 1, "{rep:?}");
        assert_eq!(rep.stale[0].0, RDATA);
        assert!(rep.findings() > 0);
    }

    #[test]
    fn a_body_that_moves_without_a_survivor_is_out_of_order() {
        let new = link(MOVED, ("_C", "c.obj"), ("_D", "d.obj"), false);
        let rep = compare(&base(), &new, &Residue::default(), &plain()).unwrap();
        assert!(rep.survivors.is_empty());
        assert!(rep.unclassified.is_empty());
        assert_eq!(rep.out_of_order.len(), 1, "{rep:?}");
        assert!(rep.findings() > 0);
    }

    #[test]
    fn declared_residue_left_behind_in_a_pad_is_pad() {
        let b = link(BASE, ("_C", "c.obj"), ("_D", "d.obj"), true);
        let row = file(BASE.j + 0x1c);
        let mut reference = vec![0u8; 0x800];
        reference[row..row + 6].copy_from_slice(&RESIDUE);
        let res = Residue {
            rows: vec![(row, 6)],
            reference,
        };
        let rep = compare(&b, &moved(MOVED), &res, &plain()).unwrap();
        assert_eq!(rep.findings(), 0, "{rep:?}");
        assert!(owned(&rep, Owner::Pad) > 0);
        let rep = compare(&b, &moved(MOVED), &Residue::default(), &plain()).unwrap();
        assert_eq!(rep.changed.len(), 1);
        assert_eq!(rep.changed[0].label, "_J");
    }

    const WIDE: Layout = Layout {
        a: 0x1000,
        b: 0x1020,
        j: 0x1030,
        cd: 0x1060,
        b_ret: 1,
        b_slot: None,
    };

    fn ending(l: Layout, vsize: u32, edit: Option<(u32, u8)>) -> Link {
        let mut lk = base();
        let mut d = image(l, false).bytes().to_vec();
        put32(&mut d, 0x180, vsize);
        for b in &mut d[0x400 + vsize as usize..0x600] {
            *b = 0;
        }
        if let Some((rva, v)) = edit {
            d[file(rva)] = v;
        }
        let text = map(
            &[
                (l.a, "_A", "a.obj"),
                (l.b, "_B", "b.obj"),
                (l.j, "_J", "b.obj"),
                (l.cd, "_C", "c.obj"),
                (l.cd, "_D", "c.obj"),
                (RDATA, "??_7V@@6B@", "a.obj"),
            ],
            &[],
        );
        lk.map = parse_map(&text);
        lk.img = Image::parse(d).unwrap();
        lk
    }

    #[test]
    fn a_shrunk_sections_tail_carried_down_is_section_tail() {
        let rep = compare(
            &ending(WIDE, 0x63, None),
            &ending(BASE, 0x53, None),
            &Residue::default(),
            &plain(),
        )
        .unwrap();
        assert!(rep.unclassified.is_empty(), "{:?}", rep.unclassified);
        assert_eq!(owned(&rep, Owner::SectionTail), 0x10);
        assert_eq!(rep.findings(), 0, "{rep:?}");
    }

    #[test]
    fn a_tail_byte_whose_body_changed_stays_unclassified() {
        let rep = compare(
            &ending(WIDE, 0x63, None),
            &ending(BASE, 0x53, Some((BASE.cd + 1, 0xc9))),
            &Residue::default(),
            &plain(),
        )
        .unwrap();
        assert!(
            rep.unclassified
                .iter()
                .any(|u| u.off == file(WIDE.cd + 1) && u.at == "section file slack"),
            "{:?}",
            rep.unclassified
        );
        assert_eq!(owned(&rep, Owner::SectionTail), 0xf);
    }

    #[test]
    fn slack_past_a_section_that_did_not_shrink_is_not_a_tail() {
        let base = ending(BASE, 0x53, None);
        let wide = ending(WIDE, 0x63, None);
        assert_eq!(shrunk_tail(&wide.img, &base.img, file(0x1058)), Some(0x10));
        assert_eq!(shrunk_tail(&base.img, &wide.img, file(0x1058)), None);
        assert_eq!(shrunk_tail(&wide.img, &base.img, file(0x1063)), None);
        assert_eq!(shrunk_tail(&wide.img, &base.img, file(0x1052)), None);
    }

    #[test]
    fn identical_links_have_no_findings() {
        let rep = compare(&base(), &base(), &Residue::default(), &plain()).unwrap();
        assert_eq!(rep.differing, 0);
        assert_eq!(rep.findings(), 0);
        assert_eq!(rep.paired.get(&How::Name), Some(&4));
    }

    #[test]
    fn c1_numbered_names_are_recognised() {
        assert!(c1_numbered("_$E12"));
        assert!(c1_numbered("$S7"));
        assert!(!c1_numbered("_$E"));
        assert!(!c1_numbered("_$Ex1"));
        assert!(!c1_numbered("?OnInit@@YAXXZ"));
        assert!(!c1_numbered("__$ReturnUdt"));
    }

    #[test]
    fn a_static_fold_group_and_two_logs_pair_by_a_shared_member() {
        let b = parse_groups(
            "    Selected symbol:\n        \".text\" from libc.lib(ehvecdtr.obj)\n    Replaced symbol(s):\n        \
             \".text\" from libc.lib(frame.obj)\n    Selected symbol:\n        _v_free_small from \
             vendor.lib(MEM.OBJ)\n    Replaced symbol(s):\n        _v_free_large from vendor.lib(MEM.OBJ)\n",
        );
        let n = parse_groups(
            "    Selected symbol:\n        _v_free_large from vendor.lib(MEM.OBJ)\n    Replaced symbol(s):\n        \
             _v_free_small from vendor.lib(MEM.OBJ)\n    Selected symbol:\n        \".text\" from \
             libc.lib(frame.obj)\n    Replaced symbol(s):\n        \".text\" from libc.lib(ehvecdtr.obj)\n",
        );
        assert_eq!(b.len(), 2);
        let sets = static_groups(&b.iter().collect::<Vec<_>>());
        assert_eq!(sets.len(), 1);
        assert!(sets[0].contains("libc:frame.obj") && sets[0].contains("libc:ehvecdtr.obj"));
        let (pairs, lone) = match_groups(&b, &n);
        assert!(lone.is_empty());
        assert_eq!(pairs.len(), 2);
        assert_eq!(pairs[0].1.survivor.obj, "libc.lib(frame.obj)");
        assert_eq!(pairs[1].1.survivor.name, "_v_free_large");
    }

    #[test]
    fn a_biased_jump_table_operand_finds_its_table() {
        let va = IB + TEXT;
        let mut bytes = vec![0xff, 0x24, 0x85];
        bytes.extend_from_slice(&(va + 8 - 4).to_le_bytes());
        bytes.push(0x90);
        bytes.extend_from_slice(&va.to_le_bytes());
        bytes.extend_from_slice(&(va + 7).to_le_bytes());
        let t = Text { bytes: &bytes, va };
        let in_code = |v: u32| (va..va + 0x10).contains(&v);
        let tables = find_tables(std::slice::from_ref(&t), &in_code);
        assert_eq!(tables.get(&(va + 8)), Some(&true));
        let f = decode_fields(&t, IB, IB + 0x3000, &in_body(&tables, &t), &in_code);
        assert_eq!(f.get(&3).map(|x| x.kind), Some(FieldKind::Disp));
        assert_eq!(
            f.get(&8).map(|x| (x.kind, x.value)),
            Some((FieldKind::Table, va))
        );
        assert_eq!(f.get(&12).map(|x| x.value), Some(va + 7));
    }

    #[test]
    fn adjacent_unclassified_bytes_in_one_body_are_one_run() {
        let u = |off: usize, at: &str| Unclassified {
            off,
            rva: None,
            at: at.into(),
            base: 1,
            new: 2,
        };
        let us = [
            u(10, "_F+0x1"),
            u(11, "_F+0x2"),
            u(12, "_G+0x0"),
            u(20, "_G+0x8"),
        ];
        let runs = unclassified_runs(&us);
        assert_eq!(runs.len(), 3);
        assert_eq!((runs[0].place(), runs[0].base.len()), ("_F", 2));
    }
}
