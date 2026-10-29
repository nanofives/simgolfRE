use crate::growth::{FILL, slot};
use crate::linkmap::Map;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const LIST_ROWS: usize = 12;

pub const END_WINDOW: u32 = 0x400;

#[must_use]
pub fn runs(bytes: &[u8], base: u32, lo: u32, hi: u32, min: usize) -> Vec<(u32, usize)> {
    let mut out = Vec::new();
    let l = usize::try_from(lo.saturating_sub(base)).unwrap_or(usize::MAX);
    let h = usize::try_from(hi.saturating_sub(base))
        .unwrap_or(usize::MAX)
        .min(bytes.len());
    let at = |i: usize| base.saturating_add(u32::try_from(i).unwrap_or(u32::MAX));
    let mut start: Option<usize> = None;
    for i in l..h {
        match (bytes.get(i) == Some(&FILL), start) {
            (true, None) => start = Some(i),
            (false, Some(s)) => {
                if i - s >= min {
                    out.push((at(s), i - s));
                }
                start = None;
            }
            _ => {}
        }
    }
    if let Some(s) = start
        && h - s >= min
    {
        out.push((at(s), h - s));
    }
    out
}

#[must_use]
pub fn own_end(map: &Map) -> Option<u32> {
    let code = map.code_sections();
    map.rows
        .iter()
        .filter(|r| code.contains(&r.sect) && r.obj.contains(':') && r.obj.ends_with(".obj"))
        .filter_map(|r| r.va.checked_sub(map.load))
        .min()
}

#[must_use]
pub fn our_layout(map: &Map) -> BTreeMap<String, (u32, String)> {
    let code = map.code_sections();
    let mut out = BTreeMap::new();
    for r in map
        .rows
        .iter()
        .filter(|r| code.contains(&r.sect) && r.function)
    {
        if !r.obj.ends_with(".obj") || r.obj.contains(':') {
            continue;
        }
        out.insert(r.name.clone(), (r.va.wrapping_sub(map.load), r.obj.clone()));
    }
    out
}

#[must_use]
pub fn slot_of(hole_rva: u32, hole_len: usize) -> Option<(u64, u64, u64)> {
    let aligned = u64::from(hole_rva).div_ceil(16) * 16;
    let end = u64::from(hole_rva).saturating_add(u64::try_from(hole_len).ok()?);
    let s = end.checked_sub(aligned)?;
    if s == 0 || !s.is_multiple_of(16) {
        return None;
    }
    let lo = (s.saturating_sub(15).saturating_mul(4)).div_ceil(5).max(1);
    let hi = s.saturating_mul(4).saturating_add(3) / 5;
    (lo <= hi && slot(lo) == s && slot(hi) == s).then_some((s, lo, hi))
}

#[derive(Debug, Clone, Default)]
pub struct Own {
    pub bodies: Vec<(String, u64)>,
    pub ends_at: BTreeMap<u32, String>,
}

impl Own {
    #[must_use]
    pub fn of(members: &[(String, u32, u64)]) -> Self {
        let mut o = Self::default();
        for (name, rva, size) in members {
            if let Ok(sz) = u32::try_from(*size) {
                o.ends_at
                    .entry(rva.saturating_add(sz))
                    .or_insert_with(|| name.clone());
            }
            o.bodies.push((name.clone(), *size));
        }
        o.bodies.sort_by(|a, b| a.1.cmp(&b.1).then(a.0.cmp(&b.0)));
        o
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Listing {
    pub list: bool,
    pub all: bool,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Counts {
    pub reference_runs: usize,
    pub candidate_runs: usize,
    pub exact: usize,
    pub at_rva: usize,
    pub missing: usize,
    pub out_of_range: usize,
    pub spurious: usize,
    pub spurious_bytes: usize,
    pub near_end: usize,
}

pub struct Region<'a> {
    pub bytes: &'a [u8],
    pub base: u32,
    pub end: u32,
}

impl std::fmt::Debug for Region<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Region")
            .field("base", &self.base)
            .field("end", &self.end)
            .finish_non_exhaustive()
    }
}

#[derive(Debug)]
pub struct Input<'a> {
    pub label: &'a str,
    pub candidate: &'a str,
    pub reference: Region<'a>,
    pub ours: Region<'a>,
    pub end_from_map: bool,
    pub min: usize,
    pub own: &'a Own,
    pub layout: &'a BTreeMap<String, (u32, String)>,
    pub show: Listing,
}

fn missing_label(i: &Input<'_>, rva: u32, len: usize) -> String {
    slot_of(rva, len).map_or_else(
        || "  not a COMDAT slot (no body size reserves it)".to_string(),
        |(s, lo, hi)| {
            let pool = i.own.bodies.iter().filter(|(_, sz)| *sz > s).count();
            let prev = i.own.ends_at.get(&rva).map_or_else(
                || "  PRECEDED BY nothing our objects define".to_string(),
                |nm| format!("  PRECEDED BY {nm}"),
            );
            let names = if i.own.bodies.is_empty() {
                "  (pass --objs to name candidates)".to_string()
            } else if pool == 0 {
                "  NO own body outgrows this slot -- so no eviction explains it".to_string()
            } else {
                format!("  {pool} outgrow it")
            };
            format!("  SLOT {s:5} B  <- gen-1 body {lo}..{hi} B{prev}{names}")
        },
    )
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn report(i: &Input<'_>) -> (Counts, Vec<String>) {
    let rr = runs(
        i.reference.bytes,
        i.reference.base,
        i.reference.base,
        i.reference.end,
        i.min,
    );
    let cr = runs(i.ours.bytes, i.ours.base, i.ours.base, i.ours.end, i.min);
    let sum = |v: &[(u32, usize)]| v.iter().map(|(_, l)| *l).sum::<usize>();
    let mut out = vec![
        format!(
            "{}: `0xcc' FILL RUNS in the OWN-CODE region, runs of {}+ bytes",
            i.label, i.min
        ),
        format!("  candidate {}", i.candidate),
        format!(
            "  reference own code {:#07x}..{:#07x}   candidate {:#07x}..{:#07x}{}",
            i.reference.base,
            i.reference.end,
            i.ours.base,
            i.ours.end,
            if i.end_from_map {
                " (candidate end from its /MAP)"
            } else {
                " (NO --map: candidate end assumed the reference's, so its count is NOT trustworthy)"
            }
        ),
        String::new(),
        format!(
            "  {:<12} {:>10} {:>8} {:>10} {:>9}",
            "", "region B", "runs", "fill B", "longest"
        ),
    ];
    let row = |tag: &str, r: &Region<'_>, v: &[(u32, usize)]| {
        format!(
            "  {tag:<12} {:>10} {:>8} {:>10} {:>9}",
            r.end.saturating_sub(r.base),
            v.len(),
            sum(v),
            v.iter().map(|(_, l)| *l).max().unwrap_or(0)
        )
    };
    out.push(row("reference", &i.reference, &rr));
    out.push(row("candidate", &i.ours, &cr));
    let cset: BTreeMap<u32, usize> = cr.iter().copied().collect();
    let mut c = Counts {
        reference_runs: rr.len(),
        candidate_runs: cr.len(),
        ..Counts::default()
    };
    let mut misses: Vec<String> = Vec::new();
    let keep = |misses: &Vec<String>| i.show.all || misses.len() < LIST_ROWS;
    for (rva, len) in &rr {
        if *rva >= i.ours.end {
            c.out_of_range += 1;
            continue;
        }
        match cset.get(rva) {
            Some(l) if l == len => c.exact += 1,
            Some(l) => {
                c.at_rva += 1;
                if keep(&misses) {
                    misses.push(format!(
                        "    {rva:#08x}  reference {len:5} B   ours {l:5} B  LENGTH DIFFERS"
                    ));
                }
            }
            None => {
                c.missing += 1;
                if keep(&misses) {
                    misses.push(format!(
                        "    {rva:#08x}  reference {len:5} B   ours none  {}",
                        missing_label(i, *rva, *len)
                    ));
                }
            }
        }
    }
    out.push(String::new());
    out.push(format!(
        "  HOLES REPRODUCED: {} of {} at the same rva AND length; {} at the same rva with a different length; {} \
         where we have no hole at all; {} beyond our own-code end and so NOT counted as misses.",
        c.exact,
        rr.len(),
        c.at_rva,
        c.missing,
        c.out_of_range
    ));
    if i.show.list {
        out.extend(misses.iter().cloned());
        if !i.show.all && misses.len() == LIST_ROWS {
            out.push(format!(
                "    ... first {LIST_ROWS} only; pass `--all' for every row, or a larger `--min'"
            ));
        }
    } else if !misses.is_empty() {
        out.push("  pass `--list' for the first differing holes".to_string());
    }
    let rset: BTreeSet<u32> = rr.iter().map(|(r, _)| *r).collect();
    let spurious: Vec<(u32, usize)> = cr
        .iter()
        .copied()
        .filter(|(r, _)| !rset.contains(r))
        .collect();
    c.spurious = spurious.len();
    c.spurious_bytes = sum(&spurious);
    let mut last_of: BTreeMap<&str, u32> = BTreeMap::new();
    for (rva, obj) in i.layout.values() {
        let e = last_of.entry(obj.as_str()).or_insert(*rva);
        *e = (*e).max(*rva);
    }
    let ends: BTreeSet<u32> = last_of.values().copied().collect();
    let near = |rva: u32| ends.iter().any(|e| *e < rva && rva - *e < END_WINDOW);
    c.near_end = spurious.iter().filter(|(r, _)| near(*r)).count();
    out.push(String::new());
    out.push(format!(
        "  SPURIOUS: {} run(s), {} byte(s) -- fill WE emit at an rva the reference has no hole at. {} of our {} runs \
         are at an rva the reference also holes.",
        c.spurious,
        c.spurious_bytes,
        cr.len() - c.spurious,
        cr.len()
    ));
    out.push(format!(
        "  OF THOSE, {} start within {END_WINDOW:#x} of an object's LAST member ({} object(s) seen), and {} do not: \
         a proximity count, not an attribution.",
        c.near_end,
        ends.len(),
        c.spurious - c.near_end
    ));
    if i.show.list {
        let cap = if i.show.all { usize::MAX } else { LIST_ROWS };
        for (rva, len) in spurious.iter().take(cap) {
            let prev = i.own.ends_at.get(rva).map_or_else(String::new, |nm| {
                format!("  PRECEDED BY {}", nm.chars().take(44).collect::<String>())
            });
            out.push(format!(
                "    {rva:#08x}  ours {len:5} B   reference none{prev}{}",
                if near(*rva) {
                    "  near a contribution END"
                } else {
                    ""
                }
            ));
        }
        if !i.show.all && spurious.len() > LIST_ROWS {
            out.push(format!("    ... first {LIST_ROWS} of {}", spurious.len()));
        }
    }
    (c, out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_threshold_is_inclusive_and_a_run_at_the_end_is_kept() {
        let b = [0u8, FILL, FILL, 0, FILL, FILL, FILL, 0];
        assert_eq!(runs(&b, 0, 0, 8, 3), vec![(4, 3)]);
        assert_eq!(runs(&b, 0, 0, 8, 2).len(), 2);
        assert!(runs(&b, 0, 0, 8, 4).is_empty());
        assert_eq!(runs(&[0u8, FILL, FILL, FILL], 0, 0, 4, 3), vec![(1, 3)]);
        assert_eq!(
            runs(&[0u8, FILL, FILL, FILL, 0], 0x1000, 0x1000, 0x1005, 3),
            vec![(0x1001, 3)]
        );
        let all = [FILL; 16];
        assert!(runs(&all, 0x1000, 0, 0x10, 4).is_empty());
        assert_eq!(runs(&all, 0x1000, 0x1000, 0x9999, 4), vec![(0x1000, 16)]);
        assert!(runs(&[1u8, 2, 3, 4], 0, 0, 4, 1).is_empty());
    }

    #[test]
    fn a_vacated_slot_is_derived_from_the_run_and_a_sliver_is_no_slot() {
        assert_eq!(slot_of(0x83d8, 1544), Some((1536, 1217, 1229)));
        assert!(slot_of(0x1001, 4).is_none());
        for start in [0x1000u32, 0x1003, 0x100f] {
            for len in 1..3000usize {
                let brute = {
                    let aligned = u64::from(start).div_ceil(16) * 16;
                    let end = u64::from(start) + len as u64;
                    end.checked_sub(aligned).filter(|s| *s > 0).and_then(|s| {
                        let w: Vec<u64> = (1..=s * 2).filter(|b| slot(*b) == s).collect();
                        Some((s, *w.first()?, *w.last()?))
                    })
                };
                assert_eq!(slot_of(start, len), brute, "{start:#x} + {len}");
            }
        }
    }

    #[test]
    fn our_own_code_ends_at_the_first_archive_member() {
        let m = crate::linkmap::parse(
            " x\r\n Preferred load address is 10000000\r\n\r\n Start Length Name Class\r\n 0001:00000000 00001000H \
             .text CODE\r\n\r\n Address Publics by Value Rva+Base Lib:Object\r\n\r\n 0001:00000000 _a 10001000 f a.obj\r\n \
             0001:00000500 _b 10001500 f LIBX:b.obj\r\n 0001:00000400 _c 10001400 f LIBX:c.obj\r\n",
        );
        assert_eq!(own_end(&m), Some(0x1400));
        assert_eq!(
            our_layout(&m).get("_a"),
            Some(&(0x1000, "a.obj".to_string()))
        );
        assert_eq!(our_layout(&m).len(), 1);
    }
}
