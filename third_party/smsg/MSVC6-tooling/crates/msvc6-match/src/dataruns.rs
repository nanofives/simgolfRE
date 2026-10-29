use anyhow::{Context as _, Result};
use msvc6_base::units::Rva;
use msvc6_diff::comdat::REL_DIR32;
use msvc6_diff::env::Env;
use msvc6_diff::pairing::{Aligned, slack};
use msvc6_image::coff::{CLASS_EXTERNAL, Object, SCN_LNK_COMDAT, Symbol};
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use std::collections::{BTreeMap, BTreeSet};
use std::path::PathBuf;

pub const ROUNDS: usize = 6;
pub const CONTENT_BUDGET: u64 = 1 << 30;

fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    d.get(at..at.checked_add(4)?)
        .and_then(|b| b.try_into().ok())
        .map(u32::from_le_bytes)
}

#[must_use]
pub fn is_data(name: &str) -> bool {
    [".data", ".rdata", ".bss", ".CRT"]
        .iter()
        .any(|p| name == *p || name.starts_with(&format!("{p}$")))
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, serde::Serialize)]
pub struct Point {
    pub obj: usize,
    pub sec: u16,
    pub off: u32,
    pub rva: u32,
    pub via: String,
}

pub type Sizes = BTreeMap<(usize, u16), (String, bool, u32, String)>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Placed {
    pub obj: usize,
    pub sec: u16,
    pub name: String,
    pub comdat: bool,
    pub size: u32,
    pub bases: BTreeMap<u32, Vec<u32>>,
    pub label: String,
}

#[must_use]
pub fn placements(points: &[Point], sizes: &Sizes) -> Vec<Placed> {
    let mut by: BTreeMap<(usize, u16), BTreeMap<u32, Vec<u32>>> = BTreeMap::new();
    for p in points {
        by.entry((p.obj, p.sec))
            .or_default()
            .entry(p.rva.wrapping_sub(p.off))
            .or_default()
            .push(p.off);
    }
    by.into_iter()
        .map(|((obj, sec), mut bases)| {
            for v in bases.values_mut() {
                v.sort_unstable();
                v.dedup();
            }
            let (name, comdat, size, label) = sizes.get(&(obj, sec)).cloned().unwrap_or_default();
            Placed {
                obj,
                sec,
                name,
                comdat,
                size,
                bases,
                label,
            }
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Run {
    pub class: String,
    pub obj: usize,
    pub lo: u32,
    pub hi: u32,
    pub n: usize,
}

#[must_use]
pub fn runs(points: &[Point], sizes: &Sizes) -> Vec<Run> {
    let mut ext: Vec<(String, u32, usize)> = points
        .iter()
        .filter_map(|p| {
            let (name, comdat, _, _) = sizes.get(&(p.obj, p.sec))?;
            (!comdat).then(|| (name.clone(), p.rva, p.obj))
        })
        .collect();
    ext.sort_unstable();
    ext.dedup();
    let mut out: Vec<Run> = Vec::new();
    for (class, rva, obj) in ext {
        match out.last_mut() {
            Some(r) if r.obj == obj && r.class == class => {
                r.hi = rva;
                r.n += 1;
            }
            _ => out.push(Run {
                class,
                obj,
                lo: rva,
                hi: rva,
                n: 1,
            }),
        }
    }
    out
}

#[must_use]
pub fn interleaved(rs: &[Run]) -> BTreeSet<(usize, usize)> {
    let mut out = BTreeSet::new();
    for (i, r) in rs.iter().enumerate() {
        let Some(k) = rs
            .iter()
            .skip(i + 1)
            .position(|q| q.obj == r.obj && q.class == r.class)
        else {
            continue;
        };
        for q in rs.iter().skip(i + 1).take(k).filter(|q| q.class == r.class) {
            out.insert((r.obj.min(q.obj), r.obj.max(q.obj)));
        }
    }
    out
}

fn owner(o: &Object<'_>, sn: u16) -> Option<String> {
    let at0 = |class: u8| {
        o.symbols.iter().find(|s| {
            s.section_number() == Some(sn)
                && s.value == 0
                && s.class == class
                && !s.name.starts_with('.')
                && !s.name.starts_with("$L")
        })
    };
    at0(CLASS_EXTERNAL)
        .or_else(|| at0(msvc6_image::coff::CLASS_STATIC))
        .map(|s| s.name.clone())
}

#[derive(Debug, Default)]
pub struct Report {
    pub stems: Vec<String>,
    pub points: Vec<Point>,
    pub sizes: Sizes,
    pub place_of: BTreeMap<(usize, String), u32>,
    pub thunks: BTreeMap<u32, Vec<u32>>,
    pub labels: Vec<BTreeMap<(u16, u32), String>>,
    pub content_exhausted: bool,
}

impl Report {
    #[must_use]
    pub fn stem(&self, i: usize) -> &str {
        self.stems.get(i).map_or("?", String::as_str)
    }
}

fn ilt_thunks(env: &Env) -> BTreeMap<u32, Vec<u32>> {
    let mut thunks: BTreeMap<u32, Vec<u32>> = BTreeMap::new();
    let base = env.text_rva;
    let d = env.text();
    let start = d.iter().position(|b| *b == 0xe9).unwrap_or(0).min(16);
    let mut at = start;
    while d.get(at) == Some(&0xe9) {
        let rel = u32_at(d, at.saturating_add(1)).unwrap_or(0);
        let src = base.wrapping_add(u32::try_from(at).unwrap_or(0));
        thunks
            .entry(src.wrapping_add(5).wrapping_add(rel))
            .or_default()
            .push(src);
        at = at.saturating_add(5);
    }
    thunks
}

type Target = (usize, u16, u32);

#[allow(clippy::too_many_lines)]
pub fn measure(
    env: &Env,
    objs: &[PathBuf],
    place: &dyn Fn(&str, &str) -> Option<u32>,
    budget: u64,
) -> Result<Report> {
    let img = &env.img;
    let image_base = img.opt.image_base.get();
    let image = u64::from(image_base)
        ..u64::from(image_base).saturating_add(u64::from(img.opt.size_of_image));
    let bytes = |rva: u32, n: usize| img.at_rva(Rva(rva), n).map(<[u8]>::to_vec);
    let mut rep = Report {
        thunks: ilt_thunks(env),
        ..Report::default()
    };
    let datas: Vec<Vec<u8>> = objs
        .iter()
        .map(|p| msvc6_diff::comdat::read(p))
        .collect::<Result<_>>()?;
    let parsed: Vec<Object<'_>> = datas
        .iter()
        .zip(objs)
        .map(|(d, p)| msvc6_diff::comdat::parse(d).with_context(|| p.display().to_string()))
        .collect::<Result<_>>()?;
    rep.stems = objs
        .iter()
        .map(|p| msvc6_diff::seat::scope_stem(p))
        .collect();
    let mut defs: BTreeMap<String, Target> = BTreeMap::new();
    for (oi, o) in parsed.iter().enumerate() {
        let mut labels: BTreeMap<(u16, u32), String> = BTreeMap::new();
        for s in &o.sections {
            if is_data(&s.name) {
                let label = o
                    .symbols
                    .iter()
                    .filter(|y| {
                        y.section_number() == Some(s.number)
                            && !y.name.starts_with('.')
                            && !y.name.starts_with("$S")
                    })
                    .min_by_key(|y| y.value)
                    .map(|y| y.name.clone())
                    .unwrap_or_default();
                rep.sizes.insert(
                    (oi, s.number),
                    (
                        s.name.clone(),
                        s.characteristics & SCN_LNK_COMDAT != 0,
                        s.size,
                        label,
                    ),
                );
            }
        }
        for y in &o.symbols {
            let Some(sn) = y.section_number() else {
                continue;
            };
            let Some(s) = o.section(sn) else { continue };
            if is_data(&s.name) && !y.name.starts_with('.') {
                labels.insert((sn, y.value), y.name.clone());
            }
            if is_data(&s.name)
                && y.class == CLASS_EXTERNAL
                && s.characteristics & SCN_LNK_COMDAT == 0
            {
                defs.entry(y.name.clone()).or_insert((oi, sn, y.value));
            }
        }
        rep.labels.push(labels);
    }
    let target_of = |oi: usize, y: &Symbol, a: u32| -> Option<Target> {
        if let Some(sn) = y.section_number() {
            let s = parsed.get(oi)?.section(sn)?;
            return is_data(&s.name).then_some((oi, sn, y.value.wrapping_add(a)));
        }
        if y.section == 0 && y.value == 0 {
            let (o2, s2, v) = defs.get(&y.name)?;
            return Some((*o2, *s2, v.wrapping_add(a)));
        }
        None
    };
    for (oi, o) in parsed.iter().enumerate() {
        let stem = rep.stem(oi).to_string();
        for sec in &o.sections {
            if !sec.name.starts_with(".text") {
                continue;
            }
            let Some(f) = owner(o, sec.number) else {
                continue;
            };
            if f.starts_with("??_G") || f.starts_with("??_E") {
                continue;
            }
            let Some(base) = place(&stem, &f) else {
                continue;
            };
            rep.place_of.insert((oi, f.clone()), base);
            let body = o.section_data(sec);
            let relocs = o.relocations(sec);
            let sites: Vec<(u32, Target)> = relocs
                .iter()
                .filter(|r| r.typ == REL_DIR32)
                .filter_map(|r| {
                    let y = o.symbol(r.symbol)?;
                    let a = usize::try_from(r.va)
                        .ok()
                        .and_then(|x| u32_at(body, x))
                        .unwrap_or(0);
                    Some((r.va, target_of(oi, y, a)?))
                })
                .collect();
            if sites.is_empty() {
                continue;
            }
            let relocated: BTreeSet<usize> = relocs
                .iter()
                .filter_map(|r| usize::try_from(r.va).ok())
                .flat_map(|a| a..a.saturating_add(4))
                .collect();
            let theirs = bytes(base, slack(body.len()))
                .or_else(|| bytes(base, body.len()))
                .unwrap_or_default();
            let al = Aligned::new(body, &relocated, theirs, &image);
            for (at, (o2, s2, off)) in sites {
                let Some((i, foff)) = al.holding(at) else {
                    continue;
                };
                let Some(j) = al.ref_of.get(i).copied().flatten() else {
                    continue;
                };
                let Some(va) = al.field(j, foff) else {
                    continue;
                };
                if !image.contains(&u64::from(va)) {
                    continue;
                }
                rep.points.push(Point {
                    obj: o2,
                    sec: s2,
                    off,
                    rva: va.wrapping_sub(image_base),
                    via: f.clone(),
                });
            }
        }
    }
    let data_secs: Vec<(u32, &[u8])> = img
        .sections
        .iter()
        .filter(|s| s.flags & IMAGE_SCN_MEM_EXECUTE == 0)
        .filter_map(|s| Some((s.rva.get(), img.at_rva(s.rva, s.disk_len())?)))
        .collect();
    let mut budget = budget;
    for (oi, o) in parsed.iter().enumerate() {
        for sec in &o.sections {
            if !is_data(&sec.name) || sec.size < 4 || sec.raw_ptr == 0 {
                continue;
            }
            let raw = o.section_data(sec);
            let mut slots: BTreeMap<usize, Option<BTreeSet<u32>>> = BTreeMap::new();
            let mut anchored = 0usize;
            for r in o.relocations(sec) {
                let Ok(at) = usize::try_from(r.va) else {
                    continue;
                };
                if r.typ != REL_DIR32 {
                    slots.insert(at, None);
                    continue;
                }
                let y = o.symbol(r.symbol);
                let a = u32_at(raw, at).unwrap_or(0);
                let body = y.and_then(|y| {
                    rep.place_of
                        .get(&(oi, y.name.clone()))
                        .copied()
                        .or_else(|| {
                            (!y.name.starts_with('.'))
                                .then(|| place(rep.stem(oi), &y.name))
                                .flatten()
                        })
                });
                match body {
                    Some(b) if a == 0 => {
                        let mut set: BTreeSet<u32> = rep
                            .thunks
                            .get(&b)
                            .cloned()
                            .unwrap_or_default()
                            .into_iter()
                            .collect();
                        set.insert(b);
                        slots.insert(
                            at,
                            Some(
                                set.into_iter()
                                    .map(|r| r.wrapping_add(image_base))
                                    .collect(),
                            ),
                        );
                        anchored += 1;
                    }
                    _ => {
                        slots.insert(at, None);
                    }
                }
            }
            if anchored == 0 {
                continue;
            }
            let mut fits = |d: &[u8], at: usize| -> Option<bool> {
                let mut k = 0usize;
                while k < raw.len() {
                    budget = budget.checked_sub(1)?;
                    if let Some(s) = slots.get(&k) {
                        let Some(v) = u32_at(d, at.saturating_add(k)) else {
                            return Some(false);
                        };
                        if s.as_ref().is_some_and(|set| !set.contains(&v)) {
                            return Some(false);
                        }
                        k = k.saturating_add(4);
                        continue;
                    }
                    if d.get(at.saturating_add(k)) != raw.get(k) {
                        return Some(false);
                    }
                    k += 1;
                }
                Some(true)
            };
            let mut hits = Vec::new();
            'secs: for (srva, d) in &data_secs {
                for at in (0..d.len().saturating_sub(raw.len().saturating_sub(1))).step_by(4) {
                    match fits(d, at) {
                        Some(true) => {
                            hits.push(srva.wrapping_add(u32::try_from(at).unwrap_or(0)));
                        }
                        Some(false) => {}
                        None => {
                            rep.content_exhausted = true;
                            break 'secs;
                        }
                    }
                }
            }
            if let [rva] = hits.as_slice() {
                rep.points.push(Point {
                    obj: oi,
                    sec: sec.number,
                    off: 0,
                    rva: *rva,
                    via: "content".to_string(),
                });
            }
        }
    }
    for _round in 0..ROUNDS {
        let placed = placements(&rep.points, &rep.sizes);
        let mut known: BTreeSet<Target> =
            rep.points.iter().map(|p| (p.obj, p.sec, p.off)).collect();
        let before = rep.points.len();
        for p in placed.iter().filter(|p| p.bases.len() == 1) {
            let Some(base) = p.bases.keys().next().copied() else {
                continue;
            };
            let Some(o) = parsed.get(p.obj) else { continue };
            let Some(sec) = o.section(p.sec) else {
                continue;
            };
            let raw = o.section_data(sec);
            for r in o.relocations(sec) {
                if r.typ != REL_DIR32 {
                    continue;
                }
                let Some(y) = o.symbol(r.symbol) else {
                    continue;
                };
                let a = usize::try_from(r.va)
                    .ok()
                    .and_then(|x| u32_at(raw, x))
                    .unwrap_or(0);
                let Some(t) = target_of(p.obj, y, a) else {
                    continue;
                };
                if known.contains(&t) {
                    continue;
                }
                let Some(va) = img.u32_at_rva(Rva(base.wrapping_add(r.va))) else {
                    continue;
                };
                if !image.contains(&u64::from(va)) {
                    continue;
                }
                known.insert(t);
                rep.points.push(Point {
                    obj: t.0,
                    sec: t.1,
                    off: t.2,
                    rva: va.wrapping_sub(image_base),
                    via: "data".to_string(),
                });
            }
        }
        if rep.points.len() == before {
            break;
        }
    }
    Ok(rep)
}

#[allow(clippy::too_many_lines)]
#[must_use]
pub fn lines(env: &Env, rep: &Report, list: bool) -> Vec<String> {
    let placed = placements(&rep.points, &rep.sizes);
    let unplaced: Vec<_> = rep
        .sizes
        .iter()
        .filter(|(k, v)| v.2 > 0 && !placed.iter().any(|p| (p.obj, p.sec) == **k))
        .collect();
    let mut out = vec![
        format!(
            "{}: DATA-RUNS -- {} point(s) over {} object section(s); {} data section(s) with bytes NOT PLACED",
            env.label,
            rep.points.len(),
            placed.len(),
            unplaced.len()
        ),
        "PER SECTION (bases = rva - offset; one base means the section was laid down whole):"
            .to_string(),
    ];
    let sec_of = |rva: u32| {
        env.img
            .section_of(Rva(rva))
            .map_or("?", |s| s.name.as_str())
    };
    let mut order: Vec<&Placed> = placed.iter().collect();
    order.sort_by_key(|p| p.bases.keys().next().copied().unwrap_or(0));
    for p in &order {
        let b: Vec<String> = p
            .bases
            .iter()
            .map(|(b, o)| {
                format!(
                    "{b:#x}[{}]",
                    o.iter()
                        .map(|x| format!("{x:#x}"))
                        .collect::<Vec<_>>()
                        .join(",")
                )
            })
            .collect();
        out.push(format!(
            "  {:6} {:24} {:10} {} size {:5}  {:?}  {}",
            sec_of(p.bases.keys().next().copied().unwrap_or(0)),
            rep.stem(p.obj),
            p.name,
            if p.comdat { "COMDAT" } else { "      " },
            p.size,
            p.label,
            b.join(" ")
        ));
    }
    for ((o, _), (n, c, sz, l)) in &unplaced {
        out.push(format!(
            "  NOT PLACED {:24} {n:10} {} size {sz:5} {l:?}",
            rep.stem(*o),
            if *c { "COMDAT" } else { "" }
        ));
    }
    let rs = runs(&rep.points, &rep.sizes);
    out.push(
        "NON-COMDAT POINT RUNS IN REFERENCE ORDER, per section class (the original link order):"
            .to_string(),
    );
    for r in &rs {
        out.push(format!(
            "  {:10} {:6} {:#08x}..{:#08x} {:3} pt  {}",
            r.class,
            sec_of(r.lo),
            r.lo,
            r.hi,
            r.n,
            rep.stem(r.obj)
        ));
    }
    if list {
        out.push(
            "EVERY NON-COMDAT POINT (class, reference rva, object, symbol+offset, named by):"
                .to_string(),
        );
        let mut ps: Vec<&Point> = rep.points.iter().collect();
        ps.sort_by_key(|p| (rep.sizes.get(&(p.obj, p.sec)).map(|v| v.0.clone()), p.rva));
        let mut last = None;
        for p in ps {
            let Some((class, comdat, _, _)) = rep.sizes.get(&(p.obj, p.sec)) else {
                continue;
            };
            if *comdat || last == Some((p.rva, p.obj)) {
                continue;
            }
            last = Some((p.rva, p.obj));
            let sym = rep
                .labels
                .get(p.obj)
                .and_then(|l| l.range((p.sec, 0)..=(p.sec, p.off)).next_back())
                .map_or_else(
                    || format!("{class}+{:#x}", p.off),
                    |((_, v), n)| format!("{n}+{:#x}", p.off.wrapping_sub(*v)),
                );
            out.push(format!(
                "  {class:10} {:#08x} {:24} {sym:60} {}",
                p.rva,
                rep.stem(p.obj),
                p.via
            ));
        }
    }
    out.push(
        "INTERLEAVED (A .. B .. A: the reference holds them in ONE contribution):".to_string(),
    );
    let il = interleaved(&rs);
    for (a, b) in &il {
        out.push(format!("  {} + {}", rep.stem(*a), rep.stem(*b)));
    }
    if il.is_empty() {
        out.push("  none".to_string());
    }
    let mut definers: BTreeMap<u32, BTreeSet<usize>> = BTreeMap::new();
    for ((o, _), r) in &rep.place_of {
        definers.entry(*r).or_default().insert(*o);
    }
    let ilt_end = rep
        .thunks
        .values()
        .flatten()
        .max()
        .map_or(0, |m| m.saturating_add(5));
    let mut ft: Vec<(u32, usize)> = definers
        .iter()
        .filter(|(r, d)| d.len() == 1 && **r >= ilt_end)
        .filter_map(|(r, d)| Some((*r, *d.iter().next()?)))
        .collect();
    ft.sort_unstable();
    let mut trs: Vec<(usize, u32, u32, usize)> = Vec::new();
    for (r, o) in ft {
        match trs.last_mut() {
            Some(t) if t.0 == o => {
                t.2 = r;
                t.3 += 1;
            }
            _ => trs.push((o, r, r, 1)),
        }
    }
    let mut alt: BTreeMap<(usize, usize), usize> = BTreeMap::new();
    for w in trs.windows(3) {
        if let [a, b, c] = w
            && a.0 == c.0
            && a.0 != b.0
        {
            *alt.entry((a.0.min(b.0), a.0.max(b.0))).or_default() += 1;
        }
    }
    out.push(
        ".TEXT A-B-A SANDWICHES per object pair (one object's run strictly inside another's):"
            .to_string(),
    );
    let mut altv: Vec<_> = alt.iter().collect();
    altv.sort_by_key(|(_, n)| std::cmp::Reverse(**n));
    for ((a, b), n) in altv {
        out.push(format!("  {n:3}  {} / {}", rep.stem(*a), rep.stem(*b)));
    }
    out.push(".TEXT RUNS (placed functions in reference order):".to_string());
    for (o, lo, hi, n) in &trs {
        out.push(format!("  {lo:#08x}..{hi:#08x} {n:4} fn  {}", rep.stem(*o)));
    }
    let classes: BTreeSet<&str> = rs.iter().map(|r| r.class.as_str()).collect();
    for c in classes {
        let mut seen = BTreeSet::new();
        let o: Vec<&str> = rs
            .iter()
            .filter(|r| r.class == c && seen.insert(r.obj))
            .map(|r| rep.stem(r.obj))
            .collect();
        out.push(format!(
            "OBJECT ORDER in {c} (first point): {}",
            o.join(" ")
        ));
    }
    if rep.content_exhausted {
        out.push(String::from(
            "the content search ran out of its byte comparisons: a section it did not reach stays NOT PLACED"
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pt(obj: usize, sec: u16, off: u32, rva: u32) -> Point {
        Point {
            obj,
            sec,
            off,
            rva,
            via: String::new(),
        }
    }

    fn sizes(rows: &[((usize, u16), &str, bool, u32)]) -> Sizes {
        rows.iter()
            .map(|(k, n, c, s)| (*k, ((*n).to_string(), *c, *s, String::new())))
            .collect()
    }

    #[test]
    fn one_base_per_whole_section_and_interleave_is_found() {
        let s = sizes(&[((0, 1), ".data", false, 8), ((1, 1), ".data", false, 4)]);
        let pts = vec![pt(0, 1, 0, 0x100), pt(1, 1, 0, 0x104), pt(0, 1, 4, 0x108)];
        assert_eq!(placements(&pts, &s).first().map(|p| p.bases.len()), Some(2));
        let rs = runs(&pts, &s);
        assert_eq!(rs.iter().map(|r| r.obj).collect::<Vec<_>>(), vec![0, 1, 0]);
        assert_eq!(
            interleaved(&rs).into_iter().collect::<Vec<_>>(),
            vec![(0, 1)]
        );
        let pts = vec![pt(0, 1, 0, 0x100), pt(0, 1, 4, 0x104), pt(1, 1, 0, 0x108)];
        assert!(interleaved(&runs(&pts, &s)).is_empty());
    }

    #[test]
    fn data_and_bss_are_separate_groups_and_comdats_are_not_contributions() {
        let s = sizes(&[
            ((0, 1), ".data", false, 4),
            ((0, 2), ".bss", false, 4),
            ((1, 1), ".data", false, 4),
            ((0, 3), ".rdata", true, 8),
        ]);
        let pts = vec![pt(0, 1, 0, 0x100), pt(1, 1, 0, 0x104), pt(0, 2, 0, 0x200)];
        assert!(interleaved(&runs(&pts, &s)).is_empty());
        assert!(runs(&[pt(0, 3, 0, 0x200)], &s).is_empty());
        assert!(is_data(".CRT$XCU") && !is_data(".text"));
    }
}
