use anyhow::Result;
use msvc6_base::units::Rva;
use msvc6_diff::comdat::{self, Comdat, REL_DIR32};
use msvc6_diff::env::Env;
use msvc6_diff::seat::{self, scope_stem};
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC};
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const ROUNDS: usize = 16;
pub const CONTENT_MIN: usize = 4;
pub const CONTENT_BUDGET: usize = 1 << 26;

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Seated {
    pub index: u16,
    pub name: String,
    pub len: usize,
    pub base: Option<u32>,
    pub by_content: bool,
    pub votes: usize,
    pub agree: usize,
    pub compared: usize,
    pub differ: usize,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct BssMismatch {
    pub symbol: String,
    pub offset: u32,
    pub image_rva: u32,
    pub seated_rva: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Report {
    pub sections: Vec<Seated>,
    pub bss: Vec<BssMismatch>,
    pub bss_symbols: usize,
    pub bss_sites: Vec<(u32, u32)>,
    pub seeds: usize,
    pub externs: Vec<(String, u32)>,
    pub content_exhausted: bool,
}

impl Report {
    #[must_use]
    pub fn data_differ(&self) -> usize {
        self.sections
            .iter()
            .filter(|s| s.name != ".bss")
            .map(|s| s.differ)
            .sum()
    }

    #[must_use]
    pub fn data_compared(&self) -> usize {
        self.sections
            .iter()
            .filter(|s| s.name != ".bss")
            .map(|s| s.compared)
            .sum()
    }

    #[must_use]
    pub fn content(&self) -> usize {
        self.sections.iter().filter(|s| s.by_content).count()
    }

    #[must_use]
    pub fn unseated(&self) -> usize {
        self.sections.iter().filter(|s| s.base.is_none()).count()
    }
}

fn image_byte(img: &Image, rva: u32) -> Option<u8> {
    img.section_of(Rva(rva))?;
    Some(
        img.at_rva(Rva(rva), 1)
            .and_then(|b| b.first().copied())
            .unwrap_or(0),
    )
}

fn image_section(img: &Image, rva: u32) -> Option<&str> {
    img.section_of(Rva(rva)).map(|s| s.name.as_str())
}

fn kind_fits(img: &Image, kind: &str, rva: u32) -> bool {
    let raw = img.at_rva(Rva(rva), 1).is_some();
    match (kind, image_section(img, rva)) {
        (".data", Some(".data")) | (".rdata", Some(".rdata")) => raw,
        (".bss", Some(".data" | ".bss")) => true,
        _ => false,
    }
}

fn context_agrees(img: &Image, c: &Comdat, at: u32, site: u32) -> bool {
    (1..=2u32).all(|k| {
        let Some(o) = at.checked_sub(k) else {
            return true;
        };
        let i = o as usize;
        if c.relocated.get(i).copied().unwrap_or(true) {
            return true;
        }
        c.bytes.get(i).copied() == image_byte(img, site.wrapping_sub(k))
    })
}

fn field(bytes: &[u8], at: u32) -> Option<u32> {
    let a = usize::try_from(at).ok()?;
    let b: [u8; 4] = bytes.get(a..a.checked_add(4)?)?.try_into().ok()?;
    Some(u32::from_le_bytes(b))
}

#[must_use]
pub fn winner(v: &BTreeMap<u32, usize>) -> u32 {
    v.iter()
        .max_by(|a, b| a.1.cmp(b.1).then(b.0.cmp(a.0)))
        .map_or(0, |(k, _)| *k)
}

#[derive(Debug)]
struct Bss {
    index: u16,
    size: u32,
    symbols: Vec<(u32, String)>,
}

fn bss_of(d: &[u8]) -> Result<Option<Bss>> {
    let o = comdat::parse(d)?;
    let mut out = None;
    for sec in o.sections.iter().filter(|s| s.name == ".bss") {
        let size = sec.size.max(sec.virt_size);
        if size == 0 {
            continue;
        }
        let mut symbols: Vec<(u32, String)> = o
            .symbols
            .iter()
            .filter(|s| s.section_number() == Some(sec.number))
            .filter(|s| s.class == CLASS_EXTERNAL || s.class == CLASS_STATIC)
            .filter(|s| !s.name.is_empty() && !s.name.starts_with('.'))
            .map(|s| (s.value, s.name.clone()))
            .collect();
        symbols.sort();
        out = Some(Bss {
            index: sec.number,
            size,
            symbols,
        });
    }
    Ok(out)
}

#[allow(clippy::too_many_lines)]
pub fn seat_in(
    env: &Env,
    obj: &Path,
    known: &BTreeMap<String, u32>,
    budget: usize,
) -> Result<Report> {
    let img = &env.img;
    let d = comdat::read(obj)?;
    let base_va = img.opt.image_base.get();
    let text = env.text();
    let (texts, members) = seat::derive(env, obj)?;
    let mut all: BTreeMap<u16, (String, Comdat)> = BTreeMap::new();
    for c in texts {
        all.insert(c.index, (".text".to_string(), c));
    }
    for c in comdat::data_of(&d)? {
        all.insert(c.index, (c.section.clone(), c));
    }
    let bss = bss_of(&d)?;
    let bss_index = bss.as_ref().map(|b| b.index);
    let mut seat: BTreeMap<u16, u32> = BTreeMap::new();
    let mut seeds = 0usize;
    let text_idx: Vec<u16> = all
        .iter()
        .filter(|(_, (n, _))| n == ".text")
        .map(|(i, _)| *i)
        .collect();
    for (idx, m) in text_idx.iter().zip(&members) {
        if let Some(r) = m.rva {
            seat.insert(*idx, r);
            seeds += 1;
        }
    }
    let stem = scope_stem(obj);
    let mut named: Vec<(u16, u32, String, u32)> = Vec::new();
    for (idx, off, name) in comdat::defined_externals(&d)? {
        let data = Some(idx) == bss_index || all.get(&idx).is_some_and(|(n, _)| n != ".text");
        if !data {
            continue;
        }
        let rva = env
            .names
            .rva_in(&name, &[stem.as_str()], text, env.text_rva)
            .or_else(|| known.get(&name).copied());
        if let Some(r) = rva {
            named.push((idx, off, name, r));
        }
    }
    let mut votes: BTreeMap<u16, BTreeMap<u32, usize>> = BTreeMap::new();
    let mut bss_seen: BTreeMap<String, (u32, BTreeMap<u32, usize>)> = BTreeMap::new();
    let mut externs: BTreeMap<String, BTreeMap<u32, usize>> = BTreeMap::new();
    let mut by_content: BTreeSet<u16> = BTreeSet::new();
    let mut bss_sites: Vec<(u32, u32)> = Vec::new();
    let mut exhausted = false;
    for _ in 0..ROUNDS {
        votes.clear();
        bss_seen.clear();
        externs.clear();
        bss_sites.clear();
        for (idx, off, name, r) in &named {
            *votes
                .entry(*idx)
                .or_default()
                .entry(r.wrapping_sub(*off))
                .or_default() += 1;
            if Some(*idx) == bss_index {
                bss_sites.push((*off, *r));
                *bss_seen
                    .entry(name.clone())
                    .or_insert((*off, BTreeMap::new()))
                    .1
                    .entry(*r)
                    .or_default() += 1;
            }
        }
        for (idx, (_, c)) in &all {
            let Some(&at_base) = seat.get(idx) else {
                continue;
            };
            for r in &c.relocations {
                if r.kind != REL_DIR32 {
                    continue;
                }
                let Some(addend) = field(&c.bytes, r.at) else {
                    continue;
                };
                let site = at_base.wrapping_add(r.at);
                if !context_agrees(img, c, r.at, site) {
                    continue;
                }
                let Some(va) = img.u32_at_rva(Rva(site)) else {
                    continue;
                };
                let Some(ti) = r.target_index else {
                    let lands = image_section(img, va.wrapping_sub(base_va));
                    if r.external && addend == 0 && matches!(lands, Some(".data" | ".rdata")) {
                        *externs
                            .entry(r.symbol.clone())
                            .or_default()
                            .entry(va.wrapping_sub(base_va))
                            .or_default() += 1;
                    }
                    continue;
                };
                let implied = va
                    .wrapping_sub(base_va)
                    .wrapping_sub(addend)
                    .wrapping_sub(r.target_offset);
                let kind = if Some(ti) == bss_index {
                    ".bss"
                } else {
                    all.get(&ti).map_or("", |(n, _)| n.as_str())
                };
                if !kind_fits(img, kind, va.wrapping_sub(base_va)) {
                    continue;
                }
                *votes.entry(ti).or_default().entry(implied).or_default() += 1;
                if Some(ti) == bss_index {
                    bss_sites.push((
                        addend.wrapping_add(r.target_offset),
                        va.wrapping_sub(base_va),
                    ));
                    if !r.symbol.starts_with('.') {
                        let e = bss_seen
                            .entry(r.symbol.clone())
                            .or_insert((r.target_offset, BTreeMap::new()));
                        *e.1.entry(implied.wrapping_add(r.target_offset))
                            .or_default() += 1;
                    }
                }
            }
        }
        let mut grew = false;
        for (ti, v) in &votes {
            if all.get(ti).is_some_and(|(n, _)| n == ".text") {
                continue;
            }
            let win = winner(v);
            if seat.get(ti) != Some(&win) {
                seat.insert(*ti, win);
                grew = true;
            }
        }
        if !grew {
            let (found, ex) = content_seats(img, &all, &seat, bss_index, budget);
            exhausted |= ex;
            if found.is_empty() {
                break;
            }
            for (ti, b) in found {
                seat.insert(ti, b);
                by_content.insert(ti);
            }
        }
    }
    let lone: Vec<u16> = seat
        .iter()
        .filter(|(ti, b)| {
            !by_content.contains(*ti)
                && all.get(*ti).is_some_and(|(n, c)| {
                    (n == ".data" || n == ".rdata")
                        && votes.get(*ti).map_or(0, |v| v.values().sum::<usize>()) == 1
                        && c.bytes
                            .iter()
                            .zip(&c.relocated)
                            .enumerate()
                            .any(|(k, (&o, &m))| {
                                !m && u32::try_from(k)
                                    .ok()
                                    .and_then(|k| image_byte(img, b.wrapping_add(k)))
                                    != Some(o)
                            })
                })
        })
        .map(|(ti, _)| *ti)
        .collect();
    if !lone.is_empty() {
        for ti in &lone {
            seat.remove(ti);
        }
        let (found, ex) = content_seats(img, &all, &seat, bss_index, budget);
        exhausted |= ex;
        for (ti, b) in found {
            seat.insert(ti, b);
            by_content.insert(ti);
        }
    }
    let mut out = Report {
        seeds,
        bss_sites,
        externs: externs
            .iter()
            .map(|(n, v)| (n.clone(), winner(v)))
            .collect(),
        content_exhausted: exhausted,
        ..Report::default()
    };
    for (idx, (n, c)) in &all {
        if n == ".text" {
            continue;
        }
        let v = votes.get(idx);
        let base = seat.get(idx).copied();
        let (mut compared, mut differ) = (0, 0);
        if let Some(b) = base {
            for (k, (&ours, &masked)) in c.bytes.iter().zip(&c.relocated).enumerate() {
                if masked {
                    continue;
                }
                compared += 1;
                let theirs = u32::try_from(k)
                    .ok()
                    .and_then(|k| image_byte(img, b.wrapping_add(k)));
                if theirs != Some(ours) {
                    differ += 1;
                }
            }
        }
        out.sections.push(Seated {
            index: *idx,
            name: n.clone(),
            len: c.bytes.len(),
            base,
            by_content: by_content.contains(idx),
            votes: v.map_or(0, |v| v.values().sum()),
            agree: match (v, base) {
                (Some(v), Some(b)) => v.get(&b).copied().unwrap_or(0),
                _ => 0,
            },
            compared,
            differ,
        });
    }
    if let Some(b) = bss {
        let v = votes.get(&b.index);
        let base = v.map(winner);
        out.bss_symbols = b.symbols.len();
        out.sections.push(Seated {
            index: b.index,
            name: ".bss".to_string(),
            len: b.size as usize,
            base,
            by_content: false,
            votes: v.map_or(0, |v| v.values().sum()),
            agree: match (v, base) {
                (Some(v), Some(w)) => v.get(&w).copied().unwrap_or(0),
                _ => 0,
            },
            compared: 0,
            differ: 0,
        });
        if let Some(w) = base {
            for (sym, (off, rv)) in &bss_seen {
                let image_rva = winner(rv);
                let seated_rva = w.wrapping_add(*off);
                if image_rva != seated_rva {
                    out.bss.push(BssMismatch {
                        symbol: sym.clone(),
                        offset: *off,
                        image_rva,
                        seated_rva,
                    });
                }
            }
        }
    }
    out.sections.sort_by_key(|s| s.index);
    Ok(out)
}

type Found = (Vec<(u16, u32)>, bool);

fn content_seats(
    img: &Image,
    all: &BTreeMap<u16, (String, Comdat)>,
    seat: &BTreeMap<u16, u32>,
    bss_index: Option<u16>,
    budget: usize,
) -> Found {
    let base_va = img.opt.image_base.get();
    let mut want: BTreeMap<u16, (&str, Vec<u32>)> = BTreeMap::new();
    let mut extents: BTreeMap<u16, (u32, u32)> = BTreeMap::new();
    for (idx, (_, c)) in all {
        let Some(&cb) = seat.get(idx) else {
            continue;
        };
        for r in &c.relocations {
            let Some(ti) = r.target_index else {
                continue;
            };
            if r.kind != REL_DIR32 || seat.contains_key(&ti) || Some(ti) == bss_index {
                continue;
            }
            let Some((kind, _)) = all.get(&ti) else {
                continue;
            };
            if kind != ".data" && kind != ".rdata" {
                continue;
            }
            let Some(addend) = field(&c.bytes, r.at) else {
                continue;
            };
            let e = want.entry(ti).or_insert((kind.as_str(), Vec::new()));
            e.1.push(addend.wrapping_add(r.target_offset));
            let len = u32::try_from(c.bytes.len()).unwrap_or(u32::MAX);
            extents.insert(*idx, (cb, len.saturating_add(len / 4)));
        }
    }
    let mut budget = budget;
    let mut exhausted = false;
    let mut found: BTreeMap<u16, BTreeSet<u32>> = BTreeMap::new();
    'scan: for &(cb, len) in extents.values() {
        for k in 0..len {
            let Some(va) = img.u32_at_rva(Rva(cb.wrapping_add(k))) else {
                continue;
            };
            let t = va.wrapping_sub(base_va);
            for (ti, (kind, adjs)) in &want {
                if !kind_fits(img, kind, t) {
                    continue;
                }
                let Some((_, s)) = all.get(ti) else {
                    continue;
                };
                for adj in adjs {
                    let cost = s.bytes.len().max(1);
                    if budget < cost {
                        exhausted = true;
                        break 'scan;
                    }
                    budget -= cost;
                    let b = t.wrapping_sub(*adj);
                    if content_matches(img, s, b) {
                        found.entry(*ti).or_default().insert(b);
                    }
                }
            }
        }
    }
    let out = found
        .into_iter()
        .filter_map(|(ti, bases)| {
            let mut it = bases.into_iter();
            match (it.next(), it.next()) {
                (Some(b), None) => Some((ti, b)),
                _ => None,
            }
        })
        .collect();
    (out, exhausted)
}

fn content_matches(img: &Image, s: &Comdat, base: u32) -> bool {
    let mut n = 0usize;
    for (k, (&ours, &masked)) in s.bytes.iter().zip(&s.relocated).enumerate() {
        if masked {
            continue;
        }
        let Ok(k) = u32::try_from(k) else {
            return false;
        };
        if img
            .at_rva(Rva(base.wrapping_add(k)), 1)
            .and_then(|b| b.first().copied())
            != Some(ours)
        {
            return false;
        }
        n += 1;
    }
    n >= CONTENT_MIN
}

#[must_use]
pub fn seat_all(env: &Env, objs: &[PathBuf], budget: usize) -> Vec<(PathBuf, Result<Report>)> {
    let empty = BTreeMap::new();
    let mut pool: BTreeMap<String, Option<u32>> = BTreeMap::new();
    for o in objs {
        if let Ok(r) = seat_in(env, o, &empty, budget) {
            for (n, v) in r.externs {
                pool.entry(n)
                    .and_modify(|e| {
                        if *e != Some(v) {
                            *e = None;
                        }
                    })
                    .or_insert(Some(v));
            }
        }
    }
    let known: BTreeMap<String, u32> = pool
        .into_iter()
        .filter_map(|(n, v)| v.map(|v| (n, v)))
        .collect();
    objs.iter()
        .map(|o| (o.clone(), seat_in(env, o, &known, budget)))
        .collect()
}

#[must_use]
pub fn detail_lines(obj: &Path, r: &Report) -> Vec<String> {
    let mut out = vec![
        format!(
            "{}: {} code COMDAT(s) seated as seeds",
            obj.display(),
            r.seeds
        ),
        "  idx  section  len     base        votes  agree  compared  differ   (* seated by content)"
            .to_string(),
    ];
    for s in &r.sections {
        out.push(format!(
            "  {:>3}  {:<7}  {:<6}  {:<10}  {:>5}  {:>5}  {:>8}  {:>6}",
            s.index,
            s.name,
            s.len,
            s.base.map_or_else(
                || "UNSEATED".to_string(),
                |b| format!("{b:#x}{}", if s.by_content { "*" } else { "" })
            ),
            s.votes,
            s.agree,
            s.compared,
            s.differ
        ));
    }
    for m in &r.bss {
        out.push(format!(
            "  BSS-ORDER {} at +{:#x}: image {:#x}, seated {:#x}",
            m.symbol, m.offset, m.image_rva, m.seated_rva
        ));
    }
    out.push(format!(
        "TOTAL .data/.rdata differ {} of {} compared, {} section(s) unseated, .bss {} symbol(s) off the winning base (of {} defined)",
        r.data_differ(),
        r.data_compared(),
        r.unseated(),
        r.bss.len(),
        r.bss_symbols
    ));
    if r.content_exhausted {
        out.push(String::from(
            "  the content search ran out of its byte comparisons: a section it did not reach stays unseated"
        ));
    }
    out
}

#[derive(Debug)]
struct LayoutRow {
    base: u32,
    len: usize,
    obj: String,
    by_content: bool,
    votes: usize,
    ours: Option<u32>,
}

fn our_bases(obj: &Path, ours: &BTreeMap<String, u32>) -> BTreeMap<u16, u32> {
    let mut out = BTreeMap::new();
    let Ok(d) = comdat::read(obj) else {
        return out;
    };
    for (index, offset, name) in comdat::defined_externals(&d).unwrap_or_default() {
        if let Some(rva) = ours.get(&name) {
            out.entry(index).or_insert_with(|| rva.wrapping_sub(offset));
        }
    }
    out
}

#[must_use]
pub fn layout_lines(
    rows: &[(PathBuf, Report)],
    name: &str,
    ours: &BTreeMap<String, u32>,
    steps: bool,
) -> Vec<String> {
    let mut seated: Vec<LayoutRow> = Vec::new();
    let mut unseated: Vec<(String, usize)> = Vec::new();
    for (o, r) in rows {
        let bases = if ours.is_empty() {
            BTreeMap::new()
        } else {
            our_bases(o, ours)
        };
        for s in r.sections.iter().filter(|s| s.name == name) {
            match s.base {
                Some(base) => seated.push(LayoutRow {
                    base,
                    len: s.len,
                    obj: scope_stem(o),
                    by_content: s.by_content,
                    votes: s.votes,
                    ours: bases.get(&s.index).copied(),
                }),
                None => unseated.push((scope_stem(o), s.len)),
            }
        }
    }
    seated.sort_by(|a, b| a.base.cmp(&b.base).then_with(|| a.obj.cmp(&b.obj)));
    let mut out = vec![
        format!(
            "{name} LAYOUT IN THE REFERENCE -- {} seated section(s), in address order (* seated by content)",
            seated.len()
        ),
        "  base        end         len       gap      votes  ours        shift     step  object"
            .to_string(),
    ];
    let mut prev_end: Option<u32> = None;
    let (mut gaps, mut overlaps) = (0i64, 0i64);
    let mut last_shift: Option<i64> = None;
    for r in &seated {
        let end = r
            .base
            .saturating_add(u32::try_from(r.len).unwrap_or(u32::MAX));
        let gap = prev_end.map(|p| i64::from(r.base) - i64::from(p));
        prev_end = Some(end);
        match gap {
            Some(g) if g > 0 => gaps += g,
            Some(g) if g < 0 => overlaps += g,
            _ => {}
        }
        let shift = r.ours.map(|o| i64::from(r.base) - i64::from(o));
        let step = shift.zip(last_shift).map(|(s, l)| s - l);
        let first = shift.is_some() && last_shift.is_none();
        if shift.is_some() {
            last_shift = shift;
        }
        if steps && !first && step.is_none_or(|d| d == 0) {
            continue;
        }
        let show = |v: Option<i64>| v.map_or_else(|| "-".to_string(), |g| g.to_string());
        out.push(format!(
            "  {:#010x}  {end:#010x}  {:>8}  {:>8}  {:>5}  {:<10}  {:>8}  {:>6}  {}{}",
            r.base,
            r.len,
            show(gap),
            r.votes,
            r.ours
                .map_or_else(|| "-".to_string(), |o| format!("{o:#010x}")),
            show(shift),
            show(step),
            r.obj,
            if r.by_content { "*" } else { "" }
        ));
    }
    unseated.sort();
    for (obj, len) in &unseated {
        out.push(format!(
            "  UNSEATED    -           {len:>8}         -      0  -                  -  {obj}"
        ));
    }
    let unseated_len: usize = unseated.iter().map(|(_, l)| l).sum();
    out.push(format!(
        "TOTAL gaps +{gaps} byte(s), overlaps {overlaps} byte(s), {} unseated section(s) of {unseated_len} byte(s)",
        unseated.len()
    ));
    out
}

#[must_use]
pub fn ranking_lines(rows: &mut [(PathBuf, Report)]) -> Vec<String> {
    rows.sort_by(|a, b| {
        b.1.data_differ()
            .cmp(&a.1.data_differ())
            .then_with(|| a.0.cmp(&b.0))
    });
    let mut out = vec!["differ  compared  unseated  content  bss-off  seeds  object".to_string()];
    for (o, r) in rows.iter() {
        out.push(format!(
            "{:>6}  {:>8}  {:>8}  {:>7}  {:>7}  {:>5}  {}",
            r.data_differ(),
            r.data_compared(),
            r.unseated(),
            r.content(),
            r.bss.len(),
            r.seeds,
            scope_stem(o)
        ));
    }
    let total: usize = rows.iter().map(|(_, r)| r.data_differ()).sum();
    out.push(format!(
        "TOTAL {} object(s), {total} differing .data/.rdata byte(s)",
        rows.len()
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn most_votes_win_and_a_tie_goes_to_the_lower_base() {
        let v: BTreeMap<u32, usize> = [(0x30, 2), (0x10, 2), (0x20, 1)].into_iter().collect();
        assert_eq!(winner(&v), 0x10);
        assert_eq!(winner(&BTreeMap::new()), 0);
    }
}
