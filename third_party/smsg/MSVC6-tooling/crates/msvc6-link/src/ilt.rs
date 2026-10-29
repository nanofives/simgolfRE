use anyhow::{Result, bail};
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::seq::{inversions, lis_nondecreasing};

pub const JMP_REL32: u8 = 0xe9;
pub const INT3: u8 = 0xcc;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Ilt {
    pub start: u32,
    pub slots: Vec<(u32, u32)>,
}

#[must_use]
pub fn read_ilt(text: &[u8], text_rva: u32) -> Ilt {
    let lead = text.iter().take_while(|b| **b == INT3).count();
    let mut slots = Vec::new();
    let mut off = lead;
    while let Some(Ok([op, a, b, c, d])) = text.get(off..off + 5).map(<[u8; 5]>::try_from) {
        if op != JMP_REL32 {
            break;
        }
        let Ok(o32) = u32::try_from(off) else { break };
        let thunk = text_rva.wrapping_add(o32);
        let disp = i32::from_le_bytes([a, b, c, d]);
        let body = i64::from(thunk) + 5 + i64::from(disp);
        let Ok(body) = u32::try_from(body) else { break };
        slots.push((thunk, body));
        off += 5;
    }
    let start = u32::try_from(lead).map_or(text_rva, |l| text_rva.wrapping_add(l));
    Ilt { start, slots }
}

pub fn ilt_of(img: &Image, what: &str) -> Result<Ilt> {
    let Some(sec) = img.section(".text") else {
        bail!("{what}: no .text");
    };
    let ilt = read_ilt(img.section_bytes(sec), sec.rva.get());
    if ilt.slots.is_empty() {
        bail!(
            "{what}: .text does not start with an `e9' thunk run, so this is not an /INCREMENTAL link and has no ILT"
        );
    }
    Ok(ilt)
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Named {
    pub slot_of: BTreeMap<String, usize>,
    pub by_export: usize,
    pub by_symbol: usize,
    pub ambiguous: usize,
}

#[must_use]
pub fn name_slots(
    ilt: &Ilt,
    exports: &[(String, u32)],
    at: &BTreeMap<u32, Vec<String>>,
    single_only: bool,
) -> Named {
    let by_thunk: BTreeMap<u32, usize> = ilt
        .slots
        .iter()
        .enumerate()
        .map(|(i, (t, _))| (*t, i))
        .collect();
    let mut n = Named::default();
    for (name, rva) in exports {
        if let Some(i) = by_thunk.get(rva) {
            n.slot_of.insert(name.clone(), *i);
            n.by_export += 1;
        }
    }
    for (i, (thunk, _)) in ilt.slots.iter().enumerate() {
        for name in at.get(thunk).into_iter().flatten() {
            if !n.slot_of.contains_key(name) {
                n.slot_of.insert(name.clone(), i);
                n.by_symbol += 1;
            }
        }
    }
    for (i, (_, body)) in ilt.slots.iter().enumerate() {
        let Some(ns) = at.get(body) else { continue };
        if single_only && ns.len() > 1 {
            n.ambiguous += 1;
            continue;
        }
        for name in ns {
            if !n.slot_of.contains_key(name) {
                n.slot_of.insert(name.clone(), i);
                n.by_symbol += 1;
            }
        }
    }
    n
}

#[must_use]
pub fn exports_of(img: &Image) -> Vec<(String, u32)> {
    img.exports()
        .ok()
        .flatten()
        .map(|e| {
            e.entries
                .iter()
                .filter_map(|x| Some((x.name.clone()?, x.target.get())))
                .collect()
        })
        .unwrap_or_default()
}

#[derive(Clone, Debug, Default, Serialize)]
pub struct Summary {
    pub reference_slots: usize,
    pub candidate_slots: usize,
    pub reference_named: usize,
    pub candidate_named: usize,
    pub shared: usize,
    pub lis: usize,
    pub inversions: u64,
    pub misplaced: usize,
}

#[derive(Clone, Debug, Serialize)]
pub struct Misplaced {
    pub name: String,
    pub reference: usize,
    pub candidate: usize,
}

#[derive(Debug)]
pub struct Side<'a> {
    pub ilt: &'a Ilt,
    pub named: &'a Named,
    pub source: String,
}

#[derive(Debug, Default)]
pub struct Report {
    pub lines: Vec<String>,
    pub summary: Summary,
    pub rows: Vec<Misplaced>,
}

#[must_use]
pub fn compare(label: &str, reference: &Side<'_>, candidate: &Side<'_>, show: usize) -> Report {
    let mut rep = Report::default();
    let (rilt, rn) = (reference.ilt, reference.named);
    let (cilt, cn) = (candidate.ilt, candidate.named);
    let out = &mut rep.lines;
    out.push(format!(
        "{label}: ILT ORDER -- our link's thunk order against the reference's, keyed by DECORATED NAME. A reader: \
         LINK decides the order (no hash family fits it), this only compares two links."
    ));
    out.push(format!(
        "  reference  {} slot(s) from {:#x}; named {} ({} by export, {} by a name row at the thunk or alone at the body{})",
        rilt.slots.len(),
        rilt.start,
        rn.slot_of.len(),
        rn.by_export,
        rn.by_symbol,
        if rn.ambiguous > 0 {
            format!(", {} body(ies) skipped as carrying several names", rn.ambiguous)
        } else {
            String::new()
        }
    ));
    out.push(format!(
        "  candidate  {} slot(s) from {:#x}; named {} ({} by export, {} by MAP public at the body{})",
        cilt.slots.len(),
        cilt.start,
        cn.slot_of.len(),
        cn.by_export,
        cn.by_symbol,
        candidate.source
    ));
    let mut shared: Vec<(&String, usize, usize)> = rn
        .slot_of
        .iter()
        .filter_map(|(n, r)| cn.slot_of.get(n).map(|c| (n, *r, *c)))
        .collect();
    shared.sort_by_key(|(n, r, _)| (*r, (*n).clone()));
    let only_ref = rn.slot_of.len() - shared.len();
    let only_cand = cn.slot_of.len() - shared.len();
    let seq: Vec<usize> = shared.iter().map(|(_, _, c)| *c).collect();
    let keep = lis_nondecreasing(&seq);
    let inv = inversions(&seq);
    out.push(format!(
        "  SHARED {} name(s) ({only_ref} named in the reference only, {only_cand} in the candidate only -- not compared)",
        shared.len()
    ));
    out.push(format!(
        "  LIS {}/{}   inversions {inv}   misplaced {}",
        keep.len(),
        shared.len(),
        shared.len() - keep.len()
    ));
    rep.summary = Summary {
        reference_slots: rilt.slots.len(),
        candidate_slots: cilt.slots.len(),
        reference_named: rn.slot_of.len(),
        candidate_named: cn.slot_of.len(),
        shared: shared.len(),
        lis: keep.len(),
        inversions: inv,
        misplaced: shared.len() - keep.len(),
    };
    let in_lis: BTreeSet<usize> = keep.iter().copied().collect();
    let misplaced: Vec<usize> = (0..shared.len()).filter(|i| !in_lis.contains(i)).collect();
    for i in &misplaced {
        if let Some((n, r, c)) = shared.get(*i) {
            rep.rows.push(Misplaced {
                name: (*n).clone(),
                reference: *r,
                candidate: *c,
            });
        }
    }
    if misplaced.is_empty() {
        return rep;
    }
    let out = &mut rep.lines;
    out.push(format!(
        "  first {} MISPLACED, in reference order: ref slot -> our slot, with the reference neighbours and where we put them",
        show.min(misplaced.len())
    ));
    let fmt = |i: Option<usize>| -> String {
        i.and_then(|i| shared.get(i))
            .map_or_else(|| "-".to_string(), |(n, r, c)| format!("{n} ({r}->{c})"))
    };
    for i in misplaced.iter().take(show) {
        let Some((n, r, c)) = shared.get(*i) else {
            continue;
        };
        out.push(format!("    {r:>5} -> {c:<5} {n}"));
        out.push(format!("          after  {}", fmt(i.checked_sub(1))));
        out.push(format!("          before {}", fmt(Some(i + 1))));
    }
    rep
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ilt_is_the_e9_run_after_the_int3_pad() {
        let text = [
            0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xe9, 0x10, 0, 0, 0, 0xe9, 0x20, 0, 0, 0, 0x55,
        ];
        let ilt = read_ilt(&text, 0x1000);
        assert_eq!(ilt.start, 0x1005);
        assert_eq!(ilt.slots, vec![(0x1005, 0x101a), (0x100a, 0x102f)]);
    }

    #[test]
    fn slots_are_named_by_export_then_at_the_thunk_then_alone_at_the_body() {
        let ilt = Ilt {
            start: 0x1005,
            slots: vec![(0x1005, 0x2000), (0x100a, 0x2100), (0x100f, 0x2200)],
        };
        let mut at: BTreeMap<u32, Vec<String>> = BTreeMap::new();
        at.insert(0x100a, vec!["_j_b".into()]);
        at.insert(0x2100, vec!["_b".into()]);
        at.insert(0x2200, vec!["_c".into(), "_c2".into()]);
        let n = name_slots(&ilt, &[("_a".into(), 0x1005)], &at, true);
        assert_eq!(n.by_export, 1);
        assert_eq!(n.slot_of.get("_j_b"), Some(&1));
        assert_eq!(n.slot_of.get("_b"), Some(&1));
        assert_eq!(n.ambiguous, 1);
        assert!(!n.slot_of.contains_key("_c"));
        let all = name_slots(&ilt, &[], &at, false);
        assert_eq!(all.slot_of.get("_c2"), Some(&2));
    }

    #[test]
    fn a_reordered_candidate_reports_its_misplaced_names() {
        let ilt = Ilt {
            start: 0,
            slots: vec![(0, 0); 4],
        };
        let r = Named {
            slot_of: [("a", 0), ("b", 1), ("c", 2), ("d", 3)]
                .iter()
                .map(|(n, s)| ((*n).to_string(), *s))
                .collect(),
            ..Named::default()
        };
        let c = Named {
            slot_of: [("a", 0), ("b", 3), ("c", 1), ("d", 2)]
                .iter()
                .map(|(n, s)| ((*n).to_string(), *s))
                .collect(),
            ..Named::default()
        };
        let side = |n| Side {
            ilt: &ilt,
            named: n,
            source: String::new(),
        };
        let rep = compare("X", &side(&r), &side(&c), 10);
        assert_eq!(rep.summary.lis, 3);
        assert_eq!(rep.summary.inversions, 2);
        assert_eq!(rep.rows.len(), 1);
        assert_eq!(rep.rows[0].name, "b");
    }
}
