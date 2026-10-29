use crate::linkmap::Map;
use crate::objkey::Keys;
use anyhow::{Result, bail};
use msvc6_image::archive::Archive;
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC, Object, SCN_LNK_COMDAT};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const SMALL_INLINE_LEN: u32 = 0x20;

pub const SHARED_DEFINERS: usize = 2;

pub const SEAT_RUNS_SHOWN: usize = 40;

pub const MIN_MEMBER: usize = 40;

pub const FIELD: usize = 4;

pub const AMBIGUOUS: &str = "AMBIGUOUS (address shared by several owners)";

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Placed {
    pub rva: u32,
    pub len: u32,
    pub member: String,
    pub archive: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TextSection {
    pub symbol: Option<String>,
    pub comdat: bool,
    pub bytes: Vec<u8>,
    pub mask: Vec<bool>,
}

#[must_use]
pub fn member_stem(member: &str) -> String {
    member
        .rsplit(['\\', '/'])
        .next()
        .unwrap_or(member)
        .to_ascii_lowercase()
}

pub fn text_sections(d: &[u8]) -> Result<Vec<TextSection>> {
    let o = Object::parse(d)?;
    let mut owner: BTreeMap<u16, (String, bool)> = BTreeMap::new();
    for s in &o.symbols {
        let Some(n) = s.section_number() else {
            continue;
        };
        if s.name.is_empty() {
            continue;
        }
        let ext = s.class == CLASS_EXTERNAL;
        match owner.get(&n) {
            Some((_, true)) => {}
            _ if ext => {
                owner.insert(n, (s.name.clone(), true));
            }
            None if !s.name.starts_with('.') && s.class == CLASS_STATIC => {
                owner.insert(n, (s.name.clone(), false));
            }
            _ => {}
        }
    }
    let budget = d.len().saturating_mul(4).saturating_add(1 << 16);
    let mut copied = 0usize;
    let mut out = Vec::new();
    for s in o.sections.iter().filter(|s| s.name == ".text") {
        let bytes = o.section_data(s).to_vec();
        copied = copied.saturating_add(bytes.len());
        if copied > budget {
            bail!(
                "its `.text` sections read more bytes than the {}-byte object holds",
                d.len()
            );
        }
        let mut mask = vec![false; bytes.len()];
        for r in o.relocations(s) {
            let at = r.va as usize;
            for slot in mask
                .iter_mut()
                .skip(at)
                .take(FIELD.min(bytes.len().saturating_sub(at)))
            {
                *slot = true;
            }
        }
        out.push(TextSection {
            symbol: owner.get(&s.number).map(|(n, _)| n.clone()),
            comdat: s.characteristics & SCN_LNK_COMDAT != 0,
            bytes,
            mask,
        });
    }
    Ok(out)
}

#[derive(Debug, Clone, Default)]
pub struct ArchiveText {
    pub stem: String,
    pub members: Vec<(String, Vec<TextSection>)>,
}

pub fn archive_text(stem: &str, d: &[u8]) -> Result<ArchiveText> {
    let a = Archive::parse(d)?;
    let mut members = Vec::new();
    for (m, body) in a.content() {
        if body.len() <= MIN_MEMBER || !msvc6_image::coff::looks_like_object(body) {
            continue;
        }
        let Ok(secs) = text_sections(body) else {
            continue;
        };
        members.push((m.name.clone(), secs));
    }
    Ok(ArchiveText {
        stem: stem.to_ascii_lowercase(),
        members,
    })
}

#[must_use]
pub fn member_sequence(rows: &[Placed], contested: impl Fn(&Placed) -> bool) -> Vec<String> {
    let mut key: BTreeMap<String, (Option<u32>, u32)> = BTreeMap::new();
    for m in rows.iter().filter(|m| m.len > 0) {
        let stem = member_stem(&m.member);
        if !stem.ends_with(".obj") {
            continue;
        }
        let firm = (!contested(m)).then_some(m.rva);
        let e = key.entry(stem).or_insert((firm, m.rva));
        e.1 = e.1.min(m.rva);
        e.0 = match (e.0, firm) {
            (Some(a), Some(b)) => Some(a.min(b)),
            (a, b) => a.or(b),
        };
    }
    let mut seq: Vec<(u32, String)> = key
        .into_iter()
        .map(|(stem, (firm, first))| (firm.unwrap_or(first), stem))
        .collect();
    seq.sort();
    seq.into_iter().map(|(_, s)| s).collect()
}

#[must_use]
pub fn shared_rows(
    rows: &[Placed],
    text: &[u8],
    text_rva: u32,
    archives: &[ArchiveText],
) -> BTreeSet<u32> {
    let mut sections: BTreeMap<(String, String), Vec<&TextSection>> = BTreeMap::new();
    let mut definers: BTreeMap<(String, String), BTreeSet<String>> = BTreeMap::new();
    for a in archives {
        for (member, secs) in &a.members {
            let stem = member_stem(member);
            for c in secs {
                if let (true, Some(s)) = (c.comdat, &c.symbol) {
                    definers
                        .entry((a.stem.clone(), s.clone()))
                        .or_default()
                        .insert(stem.clone());
                }
            }
            sections
                .entry((a.stem.clone(), stem))
                .or_default()
                .extend(secs.iter());
        }
    }
    let mut out = BTreeSet::new();
    for m in rows.iter().filter(|m| m.len > 0) {
        let arch = m.archive.to_ascii_lowercase();
        let Some(cs) = sections.get(&(arch.clone(), member_stem(&m.member))) else {
            continue;
        };
        let Some(off) = m
            .rva
            .checked_sub(text_rva)
            .and_then(|o| usize::try_from(o).ok())
        else {
            continue;
        };
        let Some(window) = usize::try_from(m.len)
            .ok()
            .and_then(|n| text.get(off..off.checked_add(n)?))
        else {
            continue;
        };
        let hit = cs.iter().any(|c| {
            c.bytes.len() == window.len()
                && c.bytes
                    .iter()
                    .zip(&c.mask)
                    .zip(window)
                    .all(|((b, masked), a)| *masked || a == b)
                && c.symbol.as_ref().is_some_and(|s| {
                    definers
                        .get(&(arch.clone(), s.clone()))
                        .is_some_and(|d| d.len() >= SHARED_DEFINERS)
                })
        });
        if hit {
            out.insert(m.rva);
        }
    }
    out
}

#[must_use]
pub fn reference_sequence(rows: &[Placed], shared: &BTreeSet<u32>) -> Vec<String> {
    member_sequence(rows, |m| {
        m.len < SMALL_INLINE_LEN || shared.contains(&m.rva)
    })
}

#[must_use]
pub fn reference_seats(rows: &[Placed]) -> BTreeMap<String, u32> {
    let mut out: BTreeMap<String, u32> = BTreeMap::new();
    for m in rows.iter().filter(|m| m.len > 0) {
        let stem = member_stem(&m.member);
        if stem.ends_with(".obj") {
            let e = out.entry(stem).or_insert(m.rva);
            *e = (*e).min(m.rva);
        }
    }
    out
}

fn archive_member<'a>(obj: &'a str, own: &Keys) -> Option<&'a str> {
    let (lib, member) = obj.split_once(':')?;
    let is_obj = member.to_ascii_lowercase().ends_with(".obj");
    (is_obj && !lib.contains('\\') && own.map_owner(obj) == obj).then_some(member)
}

#[must_use]
pub fn map_order(map: &Map, own: &Keys) -> Vec<String> {
    let code = map.code_sections();
    let mut seen = BTreeSet::new();
    let mut out = Vec::new();
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        if let Some(member) = archive_member(&r.obj, own) {
            let m = member.to_ascii_lowercase();
            if seen.insert(m.clone()) {
                out.push(m);
            }
        }
    }
    out
}

#[must_use]
pub fn map_seats(map: &Map, own: &Keys) -> BTreeMap<String, u32> {
    let code = map.code_sections();
    let mut out: BTreeMap<String, u32> = BTreeMap::new();
    if map.load == 0 {
        return out;
    }
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        let Some(member) = archive_member(&r.obj, own) else {
            continue;
        };
        let Some(rva) = r.va.checked_sub(map.load) else {
            continue;
        };
        let e = out.entry(member.to_ascii_lowercase()).or_insert(rva);
        *e = (*e).min(rva);
    }
    out
}

#[must_use]
pub fn ours_sizes(map: &Map) -> (BTreeMap<String, u64>, BTreeSet<String>) {
    let code = map.code_sections();
    let mut rows: Vec<(u32, String)> = Vec::new();
    let mut seen = BTreeSet::new();
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        let name = r
            .obj
            .rsplit(':')
            .next()
            .unwrap_or(&r.obj)
            .to_ascii_lowercase();
        if !name.ends_with(".obj") {
            continue;
        }
        seen.insert(name.clone());
        rows.push((r.va, name));
    }
    rows.sort_by_key(|(a, _)| *a);
    let mut out: BTreeMap<String, u64> = BTreeMap::new();
    let mut i = 0usize;
    while let Some((a, _)) = rows.get(i) {
        let mut j = i;
        while matches!(rows.get(j), Some((b, _)) if b == a) {
            j += 1;
        }
        let span = rows
            .get(j)
            .map_or(0, |(next, _)| u64::from(next.saturating_sub(*a)));
        if j - i == 1 {
            if let Some((_, name)) = rows.get(i) {
                *out.entry(name.clone()).or_default() += span;
            }
        } else {
            *out.entry(AMBIGUOUS.to_string()).or_default() += span;
        }
        i = j;
    }
    (out, seen)
}

#[must_use]
pub fn seat_shifts(
    r: &[String],
    refseat: &BTreeMap<String, u32>,
    ourseat: &BTreeMap<String, u32>,
) -> Vec<(String, i64)> {
    r.iter()
        .filter_map(|m| {
            let a = i64::from(*refseat.get(m)?);
            let b = i64::from(*ourseat.get(m)?);
            Some((m.clone(), b - a))
        })
        .collect()
}

#[must_use]
pub fn modal_shift(shifts: &[(String, i64)]) -> i64 {
    let mut n: BTreeMap<i64, usize> = BTreeMap::new();
    for (_, d) in shifts {
        *n.entry(*d).or_default() += 1;
    }
    n.into_iter()
        .max_by(|(da, ca), (db, cb)| ca.cmp(cb).then(db.abs().cmp(&da.abs())))
        .map_or(0, |(d, _)| d)
}

#[must_use]
pub fn seat_lines(shifts: &[(String, i64)]) -> Vec<String> {
    let mode = modal_shift(shifts);
    let off = |d: i64| (d - mode).unsigned_abs();
    let count = |f: &dyn Fn(i64) -> bool| shifts.iter().filter(|(_, d)| f(*d)).count();
    let total: u64 = shifts.iter().map(|(_, d)| off(*d)).sum();
    let mut out = vec![
        String::new(),
        format!(
            "  SEATS -- each common member's first `.text' rva, ours minus the reference's ({} member(s) seated on both \
             sides); the MODE is the shift every member before it owes.",
            shifts.len()
        ),
        format!(
            "    at the reference rva (shift 0)      {:>4}",
            count(&|d| d == 0)
        ),
        format!(
            "    at the modal shift {mode:>+8}         {:>4}",
            count(&|d| d == mode)
        ),
    ];
    for (lab, lo, hi) in [
        ("off the mode by 1..0x10", 0u64, 0x10u64),
        ("off the mode by 0x11..0x100", 0x10, 0x100),
        ("off the mode by 0x101..0x1000", 0x100, 0x1000),
    ] {
        out.push(format!(
            "    {lab:34}  {:>4}",
            count(&|d| off(d) > lo && off(d) <= hi)
        ));
    }
    out.push(format!(
        "    off the mode by more than 0x1000    {:>4}",
        count(&|d| off(d) > 0x1000)
    ));
    out.push(format!(
        "    sum of |shift - mode|           {total:>8} byte(s)"
    ));
    let mut runs: Vec<(usize, usize, i64)> = Vec::new();
    for (i, (_, d)) in shifts.iter().enumerate() {
        match runs.last_mut() {
            Some((_, hi, rd)) if *rd == *d => *hi = i,
            _ => runs.push((i, i, *d)),
        }
    }
    out.push(format!(
        "    {} run(s) of one shift in reference order (first {} shown):",
        runs.len(),
        runs.len().min(SEAT_RUNS_SHOWN)
    ));
    for (lo, hi, d) in runs.iter().take(SEAT_RUNS_SHOWN) {
        let name = shifts.get(*lo).map_or("?", |(m, _)| m.as_str());
        out.push(format!(
            "      {:>4}..{:<4} {:>3} member(s)  shift {d:>+8}  from {name}",
            lo + 1,
            hi + 1,
            hi - lo + 1
        ));
    }
    out
}

pub fn parse_window(w: &str) -> Result<(usize, usize)> {
    let Some((a, b)) = w.split_once("..") else {
        bail!("--window {w}: expected LO..HI, e.g. 150..167");
    };
    let lo: usize = a
        .trim()
        .parse()
        .map_err(|e| anyhow::anyhow!("--window {w}: {e}"))?;
    let hi: usize = b
        .trim()
        .parse()
        .map_err(|e| anyhow::anyhow!("--window {w}: {e}"))?;
    if lo == 0 || hi < lo {
        bail!("--window {w}: positions are 1-based and LO <= HI");
    }
    Ok((lo - 1, hi))
}

pub type WindowRow = (usize, String, usize, String, usize);

#[must_use]
pub fn window_rows(r: &[String], o: &[String], lo: usize, hi: usize) -> Vec<WindowRow> {
    let index = |s: &[String]| -> BTreeMap<String, usize> {
        let mut m = BTreeMap::new();
        for (i, x) in s.iter().enumerate() {
            m.entry(x.clone()).or_insert(i + 1);
        }
        m
    };
    let (ri, oi) = (index(r), index(o));
    (lo..hi.min(r.len().max(o.len())))
        .map(|i| {
            let rm = r.get(i).cloned().unwrap_or_default();
            let om = o.get(i).cloned().unwrap_or_default();
            let rin_o = oi.get(&rm).copied().unwrap_or(0);
            let oin_r = ri.get(&om).copied().unwrap_or(0);
            (i + 1, rm, rin_o, om, oin_r)
        })
        .collect()
}

#[must_use]
pub fn window_lines(rows: &[WindowRow], left: &str, right: &str) -> Vec<String> {
    let bracket = format!("[{right}]");
    let w = bracket.len().max(6);
    let mut out = vec![format!(
        "    {:>4}  {:22} {bracket:>w$}   {:22} {:>6}",
        "pos",
        left,
        right,
        format!("[{}]", left.get(..3).unwrap_or(left))
    )];
    for (p, rm, rin_o, om, oin_r) in rows {
        let mark = if rm == om { ' ' } else { '*' };
        out.push(format!(
            "   {mark}{p:>4}  {rm:22} {rin_o:>w$}   {om:22} {oin_r:>6}"
        ));
    }
    out
}

#[must_use]
pub fn common_pair(a: &[String], b: &[String]) -> (Vec<String>, Vec<String>) {
    let sa: BTreeSet<&String> = a.iter().collect();
    let sb: BTreeSet<&String> = b.iter().collect();
    (
        a.iter().filter(|m| sb.contains(m)).cloned().collect(),
        b.iter().filter(|m| sa.contains(m)).cloned().collect(),
    )
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Agreement {
    pub reference: usize,
    pub ours: usize,
    pub common: usize,
    pub lcs: usize,
}

#[must_use]
pub fn agreement(refseq: &[String], ours: &[String]) -> Agreement {
    let (r, o) = common_pair(refseq, ours);
    Agreement {
        reference: refseq.len(),
        ours: ours.len(),
        common: r.len(),
        lcs: crate::seq::lcs_len(&r, &o),
    }
}

pub struct Inputs<'a> {
    pub label: &'a str,
    pub table: &'a str,
    pub map: &'a str,
    pub refseq: &'a [String],
    pub ours: &'a [String],
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Views<'a> {
    pub misplaced: bool,
    pub sizes: bool,
    pub seats: bool,
    pub window: Option<(usize, usize)>,
    pub rows: &'a [Placed],
}

impl std::fmt::Debug for Inputs<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Inputs")
            .field("label", &self.label)
            .field("refseq", &self.refseq.len())
            .field("ours", &self.ours.len())
            .finish_non_exhaustive()
    }
}

fn names(v: &[String], from: usize, n: usize) -> String {
    v.iter()
        .skip(from)
        .take(n)
        .cloned()
        .collect::<Vec<_>>()
        .join(" ")
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn report(i: &Inputs<'_>, map: &Map, v: &Views<'_>) -> (Agreement, Vec<String>) {
    let rs: BTreeSet<&String> = i.refseq.iter().collect();
    let os: BTreeSet<&String> = i.ours.iter().collect();
    let (r, o) = common_pair(i.refseq, i.ours);
    let a = agreement(i.refseq, i.ours);
    let only: Vec<&str> = rs.difference(&os).map(|s| s.as_str()).collect();
    let mut out = vec![
        format!(
            "{}: archive member SELECTION and ORDER against the reference",
            i.label
        ),
        String::new(),
        format!(
            "  reference places      {:>4} member(s)  ({}, image order)",
            a.reference, i.table
        ),
        format!(
            "  our link pulls        {:>4} member(s)  ({})",
            a.ours, i.map
        ),
        format!("  common to both        {:>4}", a.common),
        format!(
            "  reference-only        {:>4}  {}",
            only.len(),
            only.iter().take(8).copied().collect::<Vec<_>>().join(" ")
        ),
        String::new(),
        format!(
            "  ORDER AGREEMENT (LCS)  {} of {}  ({:.0}%)",
            a.lcs,
            r.len(),
            if r.is_empty() {
                0.0
            } else {
                f64::from(u32::try_from(a.lcs).unwrap_or(u32::MAX)) * 100.0
                    / f64::from(u32::try_from(r.len()).unwrap_or(u32::MAX))
            }
        ),
        String::new(),
    ];
    match r.iter().zip(o.iter()).position(|(x, y)| x != y) {
        Some(at) => {
            out.push(format!("  MATCHING PREFIX: {at} member(s)"));
            out.push(format!(
                "  first divergence at position {}: reference wants `{}', ours has `{}'",
                at + 1,
                r.get(at).map_or("?", String::as_str),
                o.get(at).map_or("?", String::as_str)
            ));
            let lo = at.saturating_sub(2);
            out.push(format!("    reference {}..: {}", lo + 1, names(&r, lo, 8)));
            out.push(format!("    ours      {}..: {}", lo + 1, names(&o, lo, 8)));
        }
        None => out.push(format!(
            "  MATCHING PREFIX: all {} shared member(s) -- the sequences agree",
            r.len()
        )),
    }
    if v.sizes {
        out.extend(size_lines(v.rows, map));
    }
    if v.seats {
        out.extend(seat_lines(&seat_shifts(
            &r,
            &reference_seats(v.rows),
            &map_seats(map, &Keys::default()),
        )));
    }
    if v.misplaced {
        let bad = crate::seq::misplaced(&r, &o);
        out.push(String::new());
        out.push(format!(
            "  MISPLACED -- {} member(s) off one longest common subsequence (the count is fixed, the names are one \
             witness among several):",
            bad.len()
        ));
        for (name, rp, op) in &bad {
            out.push(format!(
                "    {name:20} reference position {rp:>4}   ours {op:>4}"
            ));
        }
    }
    if let Some((lo, hi)) = v.window {
        out.push(String::new());
        out.push(format!(
            "  WINDOW {}..{hi} of the common-member sequences (the other side's position in brackets):",
            lo + 1
        ));
        out.extend(window_lines(
            &window_rows(&r, &o, lo, hi),
            "reference",
            "ours",
        ));
    }
    (a, out)
}

#[must_use]
pub fn size_lines(rows: &[Placed], map: &Map) -> Vec<String> {
    let mut refsz: BTreeMap<String, u64> = BTreeMap::new();
    for m in rows.iter().filter(|m| m.len > 0) {
        *refsz.entry(member_stem(&m.member)).or_default() += u64::from(m.len);
    }
    let (oursz, ourseen) = ours_sizes(map);
    let rtot: u64 = refsz.values().sum();
    let otot: u64 = refsz.keys().filter_map(|k| oursz.get(k)).sum();
    let mut out = vec![
        String::new(),
        "  PER-MEMBER `.text' BYTES, reference against ours -- each symbol's own extent summed, never a difference \
         of two addresses:"
            .to_string(),
        format!(
            "    reference, members with a recorded extent   {:>4} member(s)  {rtot:>8} byte(s)",
            refsz.len()
        ),
        format!(
            "    ours, the same members                      {:>4} member(s)  {otot:>8} byte(s)",
            refsz.keys().filter(|k| oursz.contains_key(*k)).count()
        ),
        format!(
            "    difference                                                    {:>8}",
            i64::try_from(otot).unwrap_or(i64::MAX) - i64::try_from(rtot).unwrap_or(i64::MAX)
        ),
    ];
    let mut diffs: Vec<(i64, &String, u64, u64)> = refsz
        .iter()
        .map(|(k, rv)| {
            let ov = oursz.get(k).copied().unwrap_or(0);
            (
                i64::try_from(ov).unwrap_or(i64::MAX) - i64::try_from(*rv).unwrap_or(i64::MAX),
                k,
                *rv,
                ov,
            )
        })
        .collect();
    diffs.sort_by_key(|(d, _, _, _)| *d);
    let label = |k: &str, ov: u64| -> String {
        if ov > 0 {
            format!("{ov:>7}")
        } else if ourseen.contains(k) {
            "  AMBIG".to_string()
        } else {
            " ABSENT".to_string()
        }
    };
    out.push("    largest members where OURS IS SMALLER than the reference's:".to_string());
    for (d, k, rv, ov) in diffs.iter().take(8) {
        out.push(format!(
            "      {k:22} reference {rv:>7}  ours {}  {d:>+8}",
            label(k, *ov)
        ));
    }
    out.push("    largest members where OURS IS BIGGER:".to_string());
    for (d, k, rv, ov) in diffs.iter().rev().take(6) {
        out.push(format!(
            "      {k:22} reference {rv:>7}  ours {}  {d:>+8}",
            label(k, *ov)
        ));
    }
    let ambig = oursz.get(AMBIGUOUS).copied().unwrap_or(0);
    let absent: Vec<&str> = refsz
        .keys()
        .filter(|k| !ourseen.contains(*k))
        .map(String::as_str)
        .collect();
    out.push(format!(
        "    AMBIGUOUS spans (one address, several owners)  {ambig} byte(s) -- excluded from every row above"
    ));
    out.push(format!(
        "    members the reference has that our map never names: {} {}",
        absent.len(),
        absent.join(" ")
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn v(s: &[&str]) -> Vec<String> {
        s.iter().map(|x| (*x).to_string()).collect()
    }

    fn row(rva: u32, len: u32, member: &str) -> Placed {
        Placed {
            rva,
            len,
            member: format!("build\\obj\\{member}"),
            archive: "libx".to_string(),
        }
    }

    #[test]
    fn a_contested_row_does_not_position_its_member() {
        let rows = vec![
            row(0x33900, 0x16, "a.obj"),
            row(0x33920, 0x11, "b.obj"),
            row(0x33940, 0x80, "b.obj"),
            row(0x33a00, 0x10, "tiny.obj"),
            row(0x33b00, 0x60, "a.obj"),
            row(0x33c00, 0, "early.obj"),
            row(0x33000, 0x40, "notes.txt"),
        ];
        assert_eq!(
            member_sequence(&rows, |_| false),
            v(&["a.obj", "b.obj", "tiny.obj"])
        );
        let shared: BTreeSet<u32> = [0x33900].into_iter().collect();
        assert_eq!(
            member_sequence(&rows, |m| shared.contains(&m.rva)),
            v(&["b.obj", "tiny.obj", "a.obj"])
        );
        assert_eq!(
            member_sequence(&rows, |m| m.len < SMALL_INLINE_LEN),
            v(&["b.obj", "tiny.obj", "a.obj"])
        );
        assert_eq!(
            reference_sequence(&rows, &BTreeSet::new()),
            v(&["b.obj", "tiny.obj", "a.obj"])
        );
    }

    #[test]
    fn the_window_is_one_based_inclusive_and_cross_indexed() {
        assert_eq!(parse_window("2..3").ok(), Some((1, 3)));
        assert!(parse_window("0..3").is_err());
        assert!(parse_window("5..3").is_err());
        assert!(parse_window("5").is_err());
        let r = v(&["a", "b", "c", "d"]);
        let o = v(&["a", "c", "b", "d"]);
        assert_eq!(
            window_rows(&r, &o, 1, 3),
            vec![
                (2, "b".to_string(), 3, "c".to_string(), 3),
                (3, "c".to_string(), 2, "b".to_string(), 2),
            ]
        );
        assert_eq!(window_rows(&r, &o, 3, 99).len(), 1);
    }

    const MAP: &str = " app\r\n\r\n Preferred load address is 00400000\r\n\r\n Start Length Name Class\r\n \
                       0001:00000000 00001000H .text CODE\r\n 0002:00000000 00000100H .rdata DATA\r\n\r\n  Address \
                       Publics by Value Rva+Base Lib:Object\r\n\r\n 0001:00000000 _a 00401000 f LIBX:one.obj\r\n \
                       0001:00000000 _b 00401000 f LIBX:two.obj\r\n 0001:00000100 _c 00401100 f LIBX:three.obj\r\n \
                       0001:00000200 _d 00401200 f LIBX:three.obj\r\n 0001:00000300 _e 00401300 f own:mine.obj\r\n \
                       0001:00000400 _f 00401400 f main.obj\r\n 0002:00000000 _g 00402000 LIBX:data.obj\r\n";

    #[test]
    fn two_owners_at_one_address_are_ambiguous_and_an_own_library_is_not_an_archive_member() {
        let m = crate::linkmap::parse(MAP);
        let (sizes, seen) = ours_sizes(&m);
        assert!(seen.contains("one.obj") && seen.contains("two.obj"));
        assert_eq!(sizes.get("one.obj"), None);
        assert_eq!(sizes.get(AMBIGUOUS), Some(&0x100));
        assert_eq!(sizes.get("three.obj"), Some(&0x200));
        let own = Keys::new(["own"]);
        assert_eq!(map_order(&m, &own), v(&["one.obj", "two.obj", "three.obj"]));
        assert_eq!(
            map_order(&m, &Keys::default()),
            v(&["one.obj", "two.obj", "three.obj", "mine.obj"])
        );
        let seats = map_seats(&m, &own);
        assert_eq!(seats.get("three.obj"), Some(&0x1100));
        assert!(!seats.contains_key("data.obj"));
    }

    #[test]
    fn the_mode_prefers_the_commoner_shift_then_the_smaller() {
        let s =
            |v: &[i64]| -> Vec<(String, i64)> { v.iter().map(|d| (String::new(), *d)).collect() };
        assert_eq!(modal_shift(&s(&[-0x30, -0x30, 4, 0])), -0x30);
        assert_eq!(modal_shift(&s(&[-8, 8, 4, 4, -8])), 4);
        assert_eq!(modal_shift(&s(&[])), 0);
    }

    #[test]
    fn a_shared_inline_row_is_found_by_masked_bytes_and_its_definer_count() {
        let body = TextSection {
            symbol: Some("?rdbuf@ios@@QBEPAVstreambuf@@XZ".to_string()),
            comdat: true,
            bytes: vec![0x8b, 0x41, 0, 0, 0, 0, 0xc3],
            mask: vec![false, false, true, true, true, true, false],
        };
        let a = ArchiveText {
            stem: "libx".to_string(),
            members: vec![
                ("x\\a.obj".to_string(), vec![body.clone()]),
                ("x\\b.obj".to_string(), vec![body]),
            ],
        };
        let text = [0x90, 0x8b, 0x41, 1, 2, 3, 4, 0xc3, 0x90];
        let rows = vec![row(0x1001, 7, "a.obj"), row(0x1000, 7, "b.obj")];
        assert_eq!(shared_rows(&rows, &text, 0x1000, &[a]), [0x1001].into());
    }

    #[test]
    fn agreement_counts_common_members_and_their_lcs() {
        let a = agreement(&v(&["a", "b", "c", "x"]), &v(&["c", "a", "b", "y"]));
        assert_eq!(
            a,
            Agreement {
                reference: 4,
                ours: 4,
                common: 3,
                lcs: 2
            }
        );
    }
}
