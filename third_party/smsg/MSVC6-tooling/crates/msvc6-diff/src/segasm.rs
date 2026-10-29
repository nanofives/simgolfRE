use crate::comdat::{self, Comdat, REL_DIR32, REL_REL32, Reloc};
use crate::env::Env;
use crate::resolve::{follow_thunk, ilt_slot};
use crate::seat::scope_stem;
use crate::segments::{
    AlignRow, Analysis, Pairing, Seg, Side, Verdict, align_segment, literal, off,
};
use anyhow::{Result, anyhow, bail};
use iced_x86::{
    Decoder, DecoderOptions, Formatter as _, Instruction, MasmFormatter, OpKind, Register,
};
use msvc6_base::units::Rva;
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC};
use std::collections::{BTreeMap, HashMap};
use std::ops::Range;
use std::path::{Path, PathBuf};

pub const REGMAP_SHOWN: usize = 6;
pub const IMG_TEXT: usize = 34;
pub const LISTED: usize = 20;
const QUOTED: usize = 32;
const LITERAL_RUN: usize = 0x1_0000;

pub fn parse_pick(spec: &str, count: usize) -> Result<Vec<usize>> {
    let s = spec.trim();
    if s.eq_ignore_ascii_case("all") {
        return Ok((0..count).collect());
    }
    let num = |x: &str| {
        x.trim()
            .parse::<usize>()
            .map_err(|_| anyhow!("--asm: `{x}' is not a segment number (N, N-M or all)"))
    };
    let (lo, hi) = match s.split_once('-') {
        Some((a, b)) => (num(a)?, num(b)?),
        None => {
            let n = num(s)?;
            (n, n)
        }
    };
    if lo > hi || hi >= count {
        bail!(
            "--asm {spec}: the function has segments 0..{}",
            count.saturating_sub(1)
        );
    }
    Ok((lo..=hi).collect())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rename {
    pub ours: String,
    pub image: String,
    pub first: u64,
    pub count: usize,
    pub of: usize,
}

fn reg(r: Register) -> String {
    format!("{r:?}").to_lowercase()
}

fn op_keys(i: &Instruction, op: u32) -> (Vec<String>, Option<String>) {
    let wide = |r: Register| (r != Register::None && r.is_gpr()).then(|| reg(r.full_register32()));
    match i.op_kind(op) {
        OpKind::Register => (wide(i.op_register(op)).into_iter().collect(), None),
        OpKind::Memory => {
            let b = i.memory_base();
            let regs = [wide(b), wide(i.memory_index())]
                .into_iter()
                .flatten()
                .collect();
            let slot = (matches!(b, Register::EBP | Register::ESP)
                && i.memory_index() == Register::None)
                .then(|| {
                    let d = i.memory_displacement32().cast_signed();
                    let sign = if d < 0 { '-' } else { '+' };
                    format!("[{}{sign}{:#x}]", reg(b), d.unsigned_abs())
                });
            (regs, slot)
        }
        _ => (Vec::new(), None),
    }
}

#[must_use]
pub fn renames(img: &Side, ours: &Side, rows: &[AlignRow], start: u64) -> Vec<Rename> {
    let mut pairs: BTreeMap<(String, String), (usize, u64)> = BTreeMap::new();
    let mut total: HashMap<String, usize> = HashMap::new();
    for r in rows {
        if !matches!(r.pairing, Pairing::Same | Pairing::Form) {
            continue;
        }
        let (Some(di), Some(dj)) = (
            r.img.and_then(|i| img.dec.get(i)),
            r.ours.and_then(|j| ours.dec.get(j)),
        ) else {
            continue;
        };
        let (a, b) = (&di.insn, &dj.insn);
        if a.op_count() != b.op_count() {
            continue;
        }
        let va = start.saturating_add(u64::from(di.ip));
        for op in 0..a.op_count() {
            if a.op_kind(op) != b.op_kind(op) {
                continue;
            }
            let ((ra, sa), (rb, sb)) = (op_keys(a, op), op_keys(b, op));
            let mut zipped: Vec<(String, String)> = Vec::new();
            if ra.len() == rb.len() {
                zipped.extend(ra.into_iter().zip(rb));
            }
            if let (Some(x), Some(y)) = (sa, sb) {
                zipped.push((x, y));
            }
            for (x, y) in zipped {
                *total.entry(y.clone()).or_default() += 1;
                pairs.entry((y, x)).or_insert((0, va)).0 += 1;
            }
        }
    }
    let mut out: Vec<Rename> = pairs
        .into_iter()
        .filter(|((y, x), _)| y != x)
        .map(|((y, x), (count, first))| Rename {
            of: total.get(&y).copied().unwrap_or(count),
            ours: y,
            image: x,
            first,
            count,
        })
        .collect();
    out.sort_by(|a, b| (a.first, &a.ours).cmp(&(b.first, &b.ours)));
    out
}

#[must_use]
pub fn regmap_text(a: &Analysis, s: &Seg) -> String {
    if s.verdict == Verdict::Exact {
        return "-".into();
    }
    let rows = align_segment(&a.img, &a.ours, s.ia, s.oa);
    let r = renames(&a.img, &a.ours, &rows, a.start);
    if r.is_empty() {
        return "-".into();
    }
    let mut cell: Vec<String> = r
        .iter()
        .take(REGMAP_SHOWN)
        .map(|r| {
            format!(
                "{}->{}@{:#x} {}/{}",
                r.ours, r.image, r.first, r.count, r.of
            )
        })
        .collect();
    if r.len() > REGMAP_SHOWN {
        cell.push(format!("+{} more", r.len() - REGMAP_SHOWN));
    }
    cell.join(", ")
}

#[derive(Debug, Clone, Default)]
pub struct ImageNames {
    by_rva: BTreeMap<u32, String>,
}

impl ImageNames {
    #[must_use]
    pub fn load(env: &Env) -> Self {
        let mut by_rva: BTreeMap<u32, String> = env
            .names
            .by_rva()
            .iter()
            .filter(|(_, n)| env.names.absolute(n).is_none())
            .map(|(r, n)| (*r, n.clone()))
            .collect();
        for (r, n) in &env.function_names {
            by_rva.entry(*r).or_insert_with(|| n.clone());
        }
        Self { by_rva }
    }

    #[must_use]
    pub fn get(&self, rva: u32) -> Option<&str> {
        self.by_rva.get(&rva).map(String::as_str)
    }
}

#[must_use]
pub fn image_name(
    env: &Env,
    names: &ImageNames,
    own: &Range<u32>,
    v: u64,
    code: bool,
) -> Option<String> {
    if v < env.base || v >= env.top {
        return None;
    }
    let r = u32::try_from(v - env.base).ok()?;
    if own.contains(&r) {
        return None;
    }
    if let Some(n) = env.iat.get(&r) {
        return Some(format!("__imp_{n}"));
    }
    let (text, text_rva) = (env.text(), env.text_rva);
    let body = if code {
        follow_thunk(text, text_rva, r)
    } else {
        ilt_slot(text, text_rva, r).unwrap_or(r)
    };
    if let Some(n) = names.get(body).or_else(|| names.get(r)) {
        if n.starts_with("??_C@")
            && let Some(l) = image_literal(env, body)
        {
            return Some(l);
        }
        return Some(n.to_string());
    }
    if code || env.in_text(body) {
        return Some(format!("{body:#x}"));
    }
    image_literal(env, body)
}

fn quote(s: &str) -> String {
    let t: String = s.chars().take(QUOTED).collect();
    let more = if s.chars().count() > QUOTED {
        "..."
    } else {
        ""
    };
    format!("{t:?}{more}")
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Operand {
    Branch,
    Memory,
    Immediate,
}

fn image_names(ours: &OursNames<'_>, i: &Instruction, row: Option<&[OursField]>) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    for op in 0..i.op_count() {
        let n = match i.op_kind(op) {
            OpKind::NearBranch32 => ours.image(i.near_branch_target(), Operand::Branch, row),
            OpKind::Memory
                if i.memory_base() == Register::None || i.memory_displacement32() > 0xffff =>
            {
                ours.image(u64::from(i.memory_displacement32()), Operand::Memory, row)
            }
            OpKind::Immediate32 | OpKind::Immediate8to32 => {
                ours.image(i.immediate(op) & 0xffff_ffff, Operand::Immediate, row)
            }
            _ => None,
        };
        out.extend(n);
    }
    out.dedup();
    out
}

fn image_comment(ours: &OursNames<'_>, i: &Instruction, row: Option<&[OursField]>) -> String {
    image_names(ours, i, row).join(", ")
}

fn signed_offset(field: u32) -> String {
    let s = field.cast_signed();
    if s < 0 {
        format!("-{:#x}", s.unsigned_abs())
    } else {
        format!("+{field:#x}")
    }
}

#[must_use]
pub fn own_name(symbol: &str, field: u32, addend: bool, literal: Option<&str>) -> String {
    if let Some(n) = comdat::import_name(symbol) {
        return format!("__imp_{n}");
    }
    let head = literal.unwrap_or(symbol);
    if addend && field != 0 {
        format!("{head}{}", signed_offset(field))
    } else {
        head.to_string()
    }
}

#[must_use]
pub fn by_row(key: u64, kind: Operand, row: Option<&[OursField]>) -> Option<Option<String>> {
    let row = row?;
    if let Some(f) = row.iter().find(|f| f.va == Some(key)) {
        return Some(Some(f.name.clone()));
    }
    (kind == Operand::Immediate).then_some(None)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OursField {
    pub at: usize,
    pub rel32: bool,
    pub field: u32,
    pub symbol: String,
    pub va: Option<u64>,
    pub name: String,
}

#[derive(Debug)]
pub struct OursNames<'a> {
    env: &'a Env,
    names: &'a ImageNames,
    own: Range<u32>,
    stem: String,
    definer: &'a HashMap<String, String>,
    comdat: &'a Comdat,
    literals: HashMap<String, String>,
    paired: HashMap<String, u32>,
}

#[must_use]
pub fn definers(objs: &[PathBuf]) -> HashMap<String, String> {
    let mut out = HashMap::new();
    for o in objs {
        let Ok(d) = comdat::read(o) else { continue };
        for (_, _, name) in comdat::defined_externals(&d).unwrap_or_default() {
            out.entry(name).or_insert_with(|| scope_stem(o));
        }
    }
    out
}

#[must_use]
pub fn literals_of(obj: &Path) -> HashMap<String, String> {
    let mut out: HashMap<String, String> = HashMap::new();
    let Ok(d) = comdat::read(obj) else {
        return out;
    };
    let Ok(o) = comdat::parse(&d) else {
        return out;
    };
    for s in &o.symbols {
        if !matches!(s.class, CLASS_EXTERNAL | CLASS_STATIC)
            || s.name.is_empty()
            || s.name.starts_with('.')
            || !(s.name.starts_with("??_C@") || s.name.starts_with("$SG"))
        {
            continue;
        }
        let Some(sec) = s.section_number().and_then(|n| o.section(n)) else {
            continue;
        };
        if !(sec.name.starts_with(".data") || sec.name.starts_with(".rdata")) {
            continue;
        }
        let Some(lit) = o
            .section_data(sec)
            .get(s.value as usize..)
            .map(|b| b.get(..LITERAL_RUN).unwrap_or(b))
            .and_then(literal)
        else {
            continue;
        };
        out.entry(s.name.clone()).or_insert_with(|| quote(&lit));
    }
    out
}

impl<'a> OursNames<'a> {
    #[must_use]
    pub fn load(
        env: &'a Env,
        names: &'a ImageNames,
        a: &'a Analysis,
        definer: &'a HashMap<String, String>,
    ) -> Self {
        let mut me = Self {
            env,
            names,
            own: a.rva
                ..a.rva
                    .saturating_add(u32::try_from(a.img.bytes.len()).unwrap_or(0)),
            stem: scope_stem(&a.obj),
            definer,
            comdat: &a.comdat,
            literals: literals_of(&a.obj),
            paired: HashMap::new(),
        };
        me.paired = paired_targets(&me, a);
        me
    }

    fn target(&self, r: &Reloc) -> Option<u32> {
        if comdat::import_name(&r.symbol).is_some() {
            return None;
        }
        let env = self.env;
        let mut scopes: Vec<&str> = vec![self.stem.as_str()];
        if let Some(d) = self.definer.get(&r.symbol).filter(|d| **d != self.stem) {
            scopes.push(d.as_str());
        }
        match env
            .names
            .rva_in(&r.symbol, &scopes, env.text(), env.text_rva)
        {
            Some(u) if r.kind == REL_REL32 => Some(follow_thunk(env.text(), env.text_rva, u)),
            Some(u) => Some(u),
            None => self.paired.get(&r.symbol).copied(),
        }
    }

    #[must_use]
    pub fn image(&self, v: u64, kind: Operand, row: Option<&[OursField]>) -> Option<String> {
        let env = self.env;
        let code = kind == Operand::Branch;
        let key = match u32::try_from(v.wrapping_sub(env.base)) {
            Ok(r) if code => {
                env.base
                    .saturating_add(u64::from(follow_thunk(env.text(), env.text_rva, r)))
            }
            _ => v,
        };
        if let Some(n) = by_row(key, kind, row) {
            return n;
        }
        image_name(env, self.names, &self.own, v, code)
    }

    #[must_use]
    pub fn fields(&self, ip: usize, len: usize) -> Vec<OursField> {
        let (Ok(lo), Ok(hi)) = (u32::try_from(ip), u32::try_from(ip.saturating_add(len))) else {
            return Vec::new();
        };
        self.comdat
            .relocs_in(lo, hi)
            .iter()
            .map(|r| {
                let field = self.comdat.field(r.at);
                let name = own_name(
                    &r.symbol,
                    field,
                    r.kind == REL_DIR32 || r.symbol.starts_with(".text"),
                    self.literals.get(&r.symbol).map(String::as_str),
                );
                OursField {
                    at: (r.at as usize).saturating_sub(ip),
                    rel32: r.kind == REL_REL32,
                    field,
                    symbol: r.symbol.clone(),
                    va: self.target(r).map(|u| {
                        self.env
                            .base
                            .saturating_add(u64::from(u.wrapping_add(field)))
                    }),
                    name,
                }
            })
            .collect()
    }

    fn insn_fields(&self, a: &Analysis, j: usize) -> Vec<OursField> {
        let o = off(&a.ours, j);
        self.fields(o, off(&a.ours, j + 1).saturating_sub(o))
    }
}

fn fields_comment(fields: &[OursField]) -> String {
    fields
        .iter()
        .map(|f| f.name.as_str())
        .collect::<Vec<_>>()
        .join(", ")
}

fn read4(b: &[u8], at: usize) -> Option<u32> {
    b.get(at..at.checked_add(4)?)
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
        .map(u32::from_le_bytes)
}

fn paired_targets(ours: &OursNames<'_>, a: &Analysis) -> HashMap<String, u32> {
    let env = ours.env;
    let rows = align_segment(&a.img, &a.ours, (0, a.img.dec.len()), (0, a.ours.dec.len()));
    let mut seen: HashMap<String, Option<u32>> = HashMap::new();
    for row in rows.iter().filter(|r| r.pairing == Pairing::Same) {
        let (Some(i), Some(j)) = (row.img, row.ours) else {
            continue;
        };
        let (o, p) = (off(&a.img, i), off(&a.ours, j));
        let len = off(&a.ours, j + 1).saturating_sub(p);
        let (Ok(lo), Ok(hi)) = (u32::try_from(p), u32::try_from(p.saturating_add(len))) else {
            continue;
        };
        for r in ours.comdat.relocs_in(lo, hi) {
            if comdat::import_name(&r.symbol).is_some() || ours.target(r).is_some() {
                continue;
            }
            let k = o.saturating_add((r.at as usize).saturating_sub(p));
            let Some(v) = read4(&a.img.bytes, k) else {
                continue;
            };
            let field = ours.comdat.field(r.at);
            let rel32 = r.kind == REL_REL32;
            let va = if rel32 {
                a.start
                    .wrapping_add(k as u64)
                    .wrapping_add(4)
                    .wrapping_add(u64::from(v))
                    & 0xffff_ffff
            } else {
                u64::from(v)
            };
            let Some(rva) = va
                .checked_sub(env.base)
                .filter(|_| va < env.top)
                .and_then(|x| u32::try_from(x).ok())
            else {
                continue;
            };
            let rva = if rel32 {
                follow_thunk(env.text(), env.text_rva, rva)
            } else {
                rva
            };
            let u = rva.wrapping_sub(field);
            if let Some(text) = ours.literals.get(&r.symbol)
                && (rel32 || image_literal(env, u).as_ref() != Some(text))
            {
                continue;
            }
            seen.entry(r.symbol.clone())
                .and_modify(|e| {
                    if *e != Some(u) {
                        *e = None;
                    }
                })
                .or_insert(Some(u));
        }
    }
    seen.into_iter()
        .filter_map(|(s, u)| Some((s, u?)))
        .collect()
}

fn image_literal(env: &Env, rva: u32) -> Option<String> {
    let n = longest_run(&|n| env.img.at_rva(Rva(rva), n).is_some(), LITERAL_RUN);
    literal(env.img.at_rva(Rva(rva), n)?).map(|s| quote(&s))
}

#[must_use]
pub fn longest_run(readable: &dyn Fn(usize) -> bool, max: usize) -> usize {
    let (mut lo, mut hi) = (0usize, max);
    while lo < hi {
        let mid = lo.saturating_add(hi).div_ceil(2);
        if readable(mid) {
            lo = mid;
        } else {
            hi = mid.saturating_sub(1);
        }
    }
    lo
}

fn image_values(env: &Env, i: &Instruction) -> Vec<u64> {
    let mut out = Vec::new();
    for op in 0..i.op_count() {
        match i.op_kind(op) {
            OpKind::NearBranch32 => {
                let r = u32::try_from(i.near_branch_target().wrapping_sub(env.base)).unwrap_or(0);
                out.push(env.base.saturating_add(u64::from(follow_thunk(
                    env.text(),
                    env.text_rva,
                    r,
                ))));
            }
            OpKind::Memory
                if i.memory_base() == Register::None || i.memory_displacement32() > 0xffff =>
            {
                out.push(u64::from(i.memory_displacement32()));
            }
            OpKind::Immediate32 | OpKind::Immediate8to32 => {
                out.push(i.immediate(op) & 0xffff_ffff);
            }
            _ => {}
        }
    }
    out.sort_unstable();
    out.dedup();
    out
}

type PairedRow<'r> = (&'r AlignRow, u64, Instruction, Vec<OursField>);

fn paired_rows<'r>(ours: &OursNames<'_>, a: &Analysis, rows: &'r [AlignRow]) -> Vec<PairedRow<'r>> {
    rows.iter()
        .filter_map(|r| {
            let (i, j) = (r.img?, r.ours?);
            let o = off(&a.img, i);
            let va = a.start.saturating_add(o as u64);
            let (insn, _) = one(&a.img.bytes, o, va)?;
            Some((r, va, insn, ours.insn_fields(a, j)))
        })
        .collect()
}

#[must_use]
pub fn two_names(ours: &OursNames<'_>, a: &Analysis) -> Vec<(u64, String, String)> {
    let env = ours.env;
    let rows = align_segment(&a.img, &a.ours, (0, a.img.dec.len()), (0, a.ours.dec.len()));
    let mut out = Vec::new();
    for (r, va, insn, fields) in paired_rows(ours, a, &rows) {
        let (left, right) = (
            image_comment(ours, &insn, Some(&fields)),
            fields_comment(&fields),
        );
        if left.is_empty() || right.is_empty() || left == right {
            continue;
        }
        let mut theirs: Vec<u64> = fields.iter().filter_map(|f| f.va).collect();
        theirs.sort_unstable();
        theirs.dedup();
        let alike = if fields.iter().all(|f| f.va.is_some()) {
            !theirs.is_empty() && theirs == image_values(env, &insn)
        } else {
            r.pairing == Pairing::Same
        };
        if alike {
            out.push((va, left, right));
        }
    }
    out
}

#[must_use]
pub fn own_mismatches(ours: &OursNames<'_>, a: &Analysis) -> Vec<(u64, String, String)> {
    let env = ours.env;
    let rows = align_segment(&a.img, &a.ours, (0, a.img.dec.len()), (0, a.ours.dec.len()));
    let own = |shown: &str, f: &OursField| {
        shown.starts_with("__imp_")
            || shown.starts_with('"')
            || shown
                .strip_prefix(f.symbol.as_str())
                .is_some_and(|rest| rest.is_empty() || rest.starts_with(['+', '-']))
    };
    let mut out = Vec::new();
    for (_, va, insn, fields) in paired_rows(ours, a, &rows) {
        let values = image_values(env, &insn);
        for f in &fields {
            if !own(&f.name, f) {
                out.push((va, f.name.clone(), f.symbol.clone()));
            }
            let Some(v) = f.va.filter(|v| values.contains(v)) else {
                continue;
            };
            let kind = if f.rel32 {
                Operand::Branch
            } else {
                Operand::Memory
            };
            if let Some(n) = ours.image(v, kind, Some(&fields)).filter(|n| !own(n, f)) {
                out.push((va, n, f.symbol.clone()));
            }
        }
    }
    out
}

fn hexb(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn one(bytes: &[u8], at: usize, ip: u64) -> Option<(Instruction, String)> {
    let b = bytes.get(at..)?;
    let mut d = Decoder::with_ip(32, b, ip, DecoderOptions::NONE);
    let i = d.decode();
    if i.is_invalid() {
        return None;
    }
    let mut s = String::new();
    MasmFormatter::new().format(&i, &mut s);
    Some((i, s))
}

#[must_use]
pub fn mark(p: Pairing) -> char {
    match p {
        Pairing::Same => ' ',
        Pairing::Form => '~',
        Pairing::Replaced => '*',
        Pairing::ImageOnly => '<',
        Pairing::OursOnly => '>',
    }
}

fn with_comment(t: &str, c: &str, width: usize) -> String {
    if c.is_empty() {
        t.to_string()
    } else {
        format!("{t:<width$} ; {c}")
    }
}

#[must_use]
pub fn listing(ours: &OursNames<'_>, a: &Analysis, n: usize, s: &Seg) -> String {
    let va = |o: usize| a.start.saturating_add(o as u64);
    let rows = align_segment(&a.img, &a.ours, s.ia, s.oa);
    let mut out = format!(
        "== seg {n}  image {:#x}..{:#x}  ours +{:#x}..+{:#x}  {}  diff {}  edit {}  at {}\n   regmap (ours->image): {}\n",
        va(s.img.0),
        va(s.img.1),
        s.ours.0,
        s.ours.1,
        s.verdict.tag(),
        s.diff,
        s.edit,
        if s.at.is_empty() { "(head)" } else { &s.at },
        regmap_text(a, s)
    );
    for r in &rows {
        let row = r.ours.map(|j| ours.insn_fields(a, j));
        let left = r
            .img
            .and_then(|i| {
                let o = off(&a.img, i);
                let len = off(&a.img, i + 1).saturating_sub(o);
                let (insn, t) = one(&a.img.bytes, o, va(o))?;
                let c = image_comment(ours, &insn, row.as_deref());
                let raw = hexb(a.img.bytes.get(o..o.saturating_add(len)).unwrap_or(&[]));
                Some(format!(
                    "{:#08x} {raw:<14} {}",
                    va(o),
                    with_comment(&t, &c, IMG_TEXT)
                ))
            })
            .unwrap_or_default();
        let right = r
            .ours
            .and_then(|j| {
                let o = off(&a.ours, j);
                let len = off(&a.ours, j + 1).saturating_sub(o);
                let (_, t) = one(&a.ours.bytes, o, o as u64)?;
                let c = row.as_deref().map(fields_comment).unwrap_or_default();
                let raw = hexb(a.ours.bytes.get(o..o.saturating_add(len)).unwrap_or(&[]));
                Some(format!("+{o:04x} {raw:<14} {}", with_comment(&t, &c, 0)))
            })
            .unwrap_or_default();
        let line = format!(
            "{} {left:<width$} | {right}",
            mark(r.pairing),
            width = 9 + 15 + IMG_TEXT + 26
        );
        out.push_str(line.trim_end());
        out.push('\n');
    }
    out
}

#[must_use]
pub fn report(
    env: &Env,
    a: &Analysis,
    pick: &[usize],
    definer: &HashMap<String, String>,
) -> String {
    let names = ImageNames::load(env);
    let ours = OursNames::load(env, &names, a, definer);
    let mut text = String::new();
    for &n in pick {
        if let Some(s) = a.segs.get(n) {
            text.push_str(&listing(&ours, a, n, s));
        }
    }
    let two = two_names(&ours, a);
    text.push_str(&format!(
        "names: {} paired row(s) of the function address alike and read two ways\n",
        two.len()
    ));
    for (va, l, r) in two.iter().take(LISTED) {
        text.push_str(&format!("names:   {va:#x}  {l}  |  {r}\n"));
    }
    let bad = own_mismatches(&ours, a);
    text.push_str(&format!(
        "names: {} name(s) shown for a relocation of ours that are not its own symbol\n",
        bad.len()
    ));
    for (va, shown, sym) in bad.iter().take(LISTED) {
        text.push_str(&format!("names:   {va:#x}  {shown}  for  {sym}\n"));
    }
    text
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::segments::test_side;

    #[test]
    fn asm_picks_one_a_range_or_all() {
        assert_eq!(parse_pick("3", 5).unwrap(), [3]);
        assert_eq!(parse_pick("1-3", 5).unwrap(), [1, 2, 3]);
        assert_eq!(parse_pick("all", 3).unwrap(), [0, 1, 2]);
        assert!(parse_pick("5", 5).is_err());
        assert!(parse_pick("3-1", 5).is_err());
        assert!(parse_pick("x", 5).is_err());
    }

    #[test]
    fn a_register_swap_reads_as_two_consistent_renames() {
        let img = test_side(&[0x8b, 0x71, 0x04, 0x56]);
        let ours = test_side(&[0x8b, 0x4e, 0x04, 0x51]);
        let rows = align_segment(&img, &ours, (0, 2), (0, 2));
        assert!(rows.iter().all(|r| r.pairing == Pairing::Form));
        let r = renames(&img, &ours, &rows, 0x40_1000);
        let got: Vec<(&str, &str, u64, usize, usize)> = r
            .iter()
            .map(|r| (r.ours.as_str(), r.image.as_str(), r.first, r.count, r.of))
            .collect();
        assert_eq!(
            got,
            [
                ("ecx", "esi", 0x40_1000, 2, 2),
                ("esi", "ecx", 0x40_1000, 1, 1),
            ]
        );
    }

    #[test]
    fn a_frame_slot_permutation_reads_as_slot_renames() {
        let img = test_side(&[0x89, 0x7d, 0xf0, 0x8b, 0x45, 0xf4]);
        let ours = test_side(&[0x89, 0x7d, 0xf4, 0x8b, 0x45, 0xf8]);
        let rows = align_segment(&img, &ours, (0, 2), (0, 2));
        let r = renames(&img, &ours, &rows, 0);
        let got: Vec<(&str, &str, u64)> = r
            .iter()
            .map(|r| (r.ours.as_str(), r.image.as_str(), r.first))
            .collect();
        assert_eq!(
            got,
            [
                ("[ebp-0xc]", "[ebp-0x10]", 0),
                ("[ebp-0x8]", "[ebp-0xc]", 3)
            ]
        );
    }

    #[test]
    fn a_relocation_reads_by_its_own_symbol_and_signed_addend() {
        assert_eq!(
            own_name("?t@@3PAFA", 0xffff_ff9a, true, None),
            "?t@@3PAFA-0x66"
        );
        assert_eq!(
            own_name("?b@@3PAUT@@A", 0x3880, true, None),
            "?b@@3PAUT@@A+0x3880"
        );
        assert_eq!(own_name("?n@@3CA", 0, true, None), "?n@@3CA");
        assert_eq!(
            own_name("??_C@_05A@Lake?5?$AA@", 5, true, Some("\"Lake \"")),
            "\"Lake \"+0x5"
        );
        assert_eq!(
            own_name("__imp__timeGetTime@0", 0, true, None),
            "__imp_timeGetTime"
        );
        assert_eq!(
            own_name("?Free@@YAXPAX@Z", 0xffff_fffc, false, None),
            "?Free@@YAXPAX@Z"
        );
        assert_eq!(signed_offset(0xffff_ffff), "-0x1");
        assert_eq!(signed_offset(0x21), "+0x21");
    }

    #[test]
    fn an_image_operand_is_named_by_its_rows_own_relocation() {
        let field = |symbol: &str, va: u64, name: &str| OursField {
            at: 2,
            rel32: false,
            field: 0,
            symbol: symbol.into(),
            va: Some(va),
            name: name.into(),
        };
        let end = [field("?b@@3PAUT@@A", 0x5a_6d3c, "?b@@3PAUT@@A+0x3880")];
        let theme = [field("?n@@3CA", 0x5a_6d3c, "?n@@3CA")];
        assert_eq!(
            by_row(0x5a_6d3c, Operand::Memory, Some(&end)),
            Some(Some("?b@@3PAUT@@A+0x3880".into()))
        );
        assert_eq!(
            by_row(0x5a_6d3c, Operand::Memory, Some(&theme)),
            Some(Some("?n@@3CA".into()))
        );
        assert_eq!(by_row(0x80_0000, Operand::Immediate, Some(&[])), Some(None));
        assert_eq!(by_row(0x5a_0000, Operand::Memory, Some(&theme)), None);
        assert_eq!(by_row(0x80_0000, Operand::Immediate, None), None);
    }

    #[test]
    fn a_literal_run_is_found_whatever_its_length() {
        let sec = [b'x'; 99];
        assert_eq!(longest_run(&|n| n <= sec.len(), LITERAL_RUN), 99);
        assert_eq!(longest_run(&|n| n <= 64, LITERAL_RUN), 64);
        assert_eq!(longest_run(&|_| false, LITERAL_RUN), 0);
        assert_eq!(quote(&"y".repeat(40)), format!("{:?}...", "y".repeat(32)));
    }

    #[test]
    fn the_marks_name_each_pairing() {
        let m: String = [
            Pairing::Same,
            Pairing::Form,
            Pairing::Replaced,
            Pairing::ImageOnly,
            Pairing::OursOnly,
        ]
        .into_iter()
        .map(mark)
        .collect();
        assert_eq!(m, " ~*<>");
    }
}
