use crate::budget::Budget;
use anyhow::{Context as _, Result, anyhow};
use msvc6_base::units::Rva;
use msvc6_diff::comdat::{self, Comdat, REL_DIR32};
use msvc6_diff::env::Env;
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC};
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const ROUNDS: usize = 16;

pub const CONTENT_MIN: usize = 4;

pub const STEPS_PER_BYTE: u64 = 64;

pub const STEP_FLOOR: u64 = 1 << 22;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Bss {
    pub size: u32,
    pub align: u32,
    pub symbols: Vec<(u32, String)>,
}

pub fn bss_of(d: &[u8]) -> Result<Option<Bss>> {
    let o = comdat::parse(d)?;
    let mut out = None;
    for sec in &o.sections {
        if sec.name != ".bss" || sec.size == 0 {
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
            size: sec.size,
            align: sec.align(),
            symbols,
        });
    }
    Ok(out)
}

fn bss_index_of(d: &[u8]) -> Result<Option<u16>> {
    let o = comdat::parse(d)?;
    Ok(o.sections
        .iter()
        .find(|s| s.name == ".bss")
        .map(|s| s.number))
}

#[derive(Debug, Clone, PartialEq, Eq)]
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

#[derive(Debug, Clone, PartialEq, Eq)]
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
    pub steps: u64,
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

fn winner(v: &BTreeMap<u32, usize>) -> u32 {
    v.iter()
        .max_by(|a, b| a.1.cmp(b.1).then(b.0.cmp(a.0)))
        .map_or(0, |(k, _)| *k)
}

type Sections = BTreeMap<u16, (String, Comdat)>;

fn content_matches(img: &Image, s: &Comdat, base: u32, bud: &Budget) -> bool {
    let mut n = 0usize;
    for (k, (&ours, &masked)) in s.bytes.iter().zip(&s.relocated).enumerate() {
        if !bud.tick() {
            return false;
        }
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

fn content_seats(
    img: &Image,
    all: &Sections,
    seat: &BTreeMap<u16, u32>,
    bss_index: Option<u16>,
    bud: &Budget,
) -> Vec<(u16, u32)> {
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
    let mut found: BTreeMap<u16, BTreeSet<u32>> = BTreeMap::new();
    'outer: for &(cb, len) in extents.values() {
        for k in 0..len {
            if !bud.tick() {
                break 'outer;
            }
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
                    if !bud.tick() {
                        break 'outer;
                    }
                    let b = t.wrapping_sub(*adj);
                    if content_matches(img, s, b, bud) {
                        found.entry(*ti).or_default().insert(b);
                    }
                }
            }
        }
    }
    found
        .into_iter()
        .filter_map(|(ti, bases)| {
            let mut it = bases.into_iter();
            match (it.next(), it.next()) {
                (Some(b), None) => Some((ti, b)),
                _ => None,
            }
        })
        .collect()
}

#[allow(clippy::too_many_lines)]
pub fn seat_in(env: &Env, obj: &Path, known: &BTreeMap<String, u32>) -> Result<Report> {
    let d = comdat::read(obj)?;
    let img = &env.img;
    let base_va = img.opt.image_base.get();
    let bud = Budget::per_item(d.len(), STEPS_PER_BYTE, STEP_FLOOR);
    let texts = comdat::text_of(&d).with_context(|| obj.display().to_string())?;
    let members = msvc6_diff::seat::members_of(env, obj, &d, &texts)?;
    let mut all: Sections = BTreeMap::new();
    for c in texts {
        all.insert(c.index, (".text".to_string(), c));
    }
    for c in comdat::data_of(&d)? {
        all.insert(c.index, (c.section.clone(), c));
    }
    let bss = bss_of(&d)?;
    let bss_index = bss_index_of(&d)?;
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
    let stem = msvc6_diff::seat::scope_stem(obj);
    let mut named: Vec<(u16, u32, String, u32)> = Vec::new();
    for (idx, off, name) in comdat::defined_externals(&d)? {
        let data = Some(idx) == bss_index || all.get(&idx).is_some_and(|(n, _)| n != ".text");
        if !data {
            continue;
        }
        let rva = env
            .names
            .recorded(&name, &[stem.as_str()])
            .map(|(r, _)| r)
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
                if !bud.tick() {
                    break;
                }
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
                }
                if Some(ti) == bss_index && !r.symbol.starts_with('.') {
                    let e = bss_seen
                        .entry(r.symbol.clone())
                        .or_insert((r.target_offset, BTreeMap::new()));
                    *e.1.entry(implied.wrapping_add(r.target_offset))
                        .or_default() += 1;
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
            let found = content_seats(img, &all, &seat, bss_index, &bud);
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
        for (ti, b) in content_seats(img, &all, &seat, bss_index, &bud) {
            seat.insert(ti, b);
            by_content.insert(ti);
        }
    }
    if let Some(why) = bud.over() {
        return Err(anyhow!(
            "{}: data seating stopped at {why} ({} steps over {} bytes)",
            obj.display(),
            bud.spent(),
            d.len()
        ));
    }
    let mut out = Report {
        seeds,
        bss_sites,
        externs: externs
            .iter()
            .map(|(n, v)| (n.clone(), winner(v)))
            .collect(),
        steps: bud.spent(),
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
    if let (Some(b), Some(bi)) = (bss, bss_index) {
        let v = votes.get(&bi);
        let base = v.map(winner);
        out.bss_symbols = b.symbols.len();
        out.sections.push(Seated {
            index: bi,
            name: ".bss".to_string(),
            len: usize::try_from(b.size)?,
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

#[must_use]
pub fn seat_all(env: &Env, objs: &[PathBuf]) -> Vec<(PathBuf, Result<Report>)> {
    let empty = BTreeMap::new();
    let mut pool: BTreeMap<String, Option<u32>> = BTreeMap::new();
    for o in objs {
        if let Ok(r) = seat_in(env, o, &empty) {
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
        .map(|o| (o.clone(), seat_in(env, o, &known)))
        .collect()
}
