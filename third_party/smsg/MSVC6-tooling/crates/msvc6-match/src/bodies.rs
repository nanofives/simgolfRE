use anyhow::{Result, anyhow, bail};
use msvc6_image::pe::Image;

pub const SEARCH_BUDGET: u64 = 1 << 30;

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Hit {
    pub rva: u32,
    pub shift: i64,
}

#[must_use]
pub fn masked_offsets(relocs: &[u32], lo: u32, hi: u32) -> Vec<usize> {
    let mut out = Vec::new();
    for &r in relocs {
        for b in r..r.saturating_add(4) {
            if b >= lo && b < hi {
                out.push((b - lo) as usize);
            }
        }
    }
    out.sort_unstable();
    out.dedup();
    out
}

#[must_use]
pub fn rel32_offsets(body: &[u8]) -> Vec<usize> {
    let mut out = Vec::new();
    let mut i = 0usize;
    while i.saturating_add(5) <= body.len() {
        if matches!(body.get(i), Some(0xe8 | 0xe9)) {
            out.extend(i + 1..i + 5);
            i += 5;
        } else {
            i += 1;
        }
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Found {
    pub starts: Vec<usize>,
    pub exhausted: bool,
}

#[must_use]
pub fn masked_find(hay: &[u8], needle: &[u8], mask: &[bool], budget: u64) -> Found {
    let mut out = Found {
        starts: Vec::new(),
        exhausted: false,
    };
    if needle.is_empty() || hay.len() < needle.len() {
        return out;
    }
    let mut left = budget;
    for start in 0..=(hay.len() - needle.len()) {
        let mut ok = true;
        for (i, n) in needle.iter().enumerate() {
            if left == 0 {
                out.exhausted = true;
                return out;
            }
            left -= 1;
            if mask.get(i) == Some(&false) && hay.get(start + i) != Some(n) {
                ok = false;
                break;
            }
        }
        if ok {
            out.starts.push(start);
        }
    }
    out
}

fn mask_of(reference: &Image, ref_rva: u32, needle: &[u8]) -> Option<(Vec<bool>, usize)> {
    let relocs: Vec<u32> = reference
        .relocations()
        .iter()
        .map(|(r, _)| r.get())
        .collect();
    let len = needle.len();
    let mut mask = vec![false; len];
    let end = ref_rva.checked_add(u32::try_from(len).ok()?)?;
    for m in masked_offsets(&relocs, ref_rva, end) {
        if let Some(slot) = mask.get_mut(m) {
            *slot = true;
        }
    }
    let from_relocs = mask.iter().filter(|m| **m).count();
    for m in rel32_offsets(needle) {
        if let Some(slot) = mask.get_mut(m) {
            *slot = true;
        }
    }
    Some((mask, from_relocs))
}

fn body(img: &Image, rva: u32, len: usize) -> Option<(&[u8], u32)> {
    let t = img.section(".text")?;
    let off = usize::try_from(rva.checked_sub(t.rva.get())?).ok()?;
    let b = img.section_bytes(t).get(off..off.checked_add(len)?)?;
    Some((b, t.rva.get()))
}

#[must_use]
pub fn find_unique(reference: &Image, cand: &Image, ref_rva: u32, len: usize) -> Option<u32> {
    let (needle, _) = body(reference, ref_rva, len)?;
    let ct = cand.section(".text")?;
    let (mask, _) = mask_of(reference, ref_rva, needle)?;
    let hits = masked_find(cand.section_bytes(ct), needle, &mask, SEARCH_BUDGET);
    match (hits.exhausted, hits.starts.as_slice()) {
        (false, [one]) => ct.rva.get().checked_add(u32::try_from(*one).ok()?),
        _ => None,
    }
}

#[must_use]
pub fn verified_pair(
    reference: &Image,
    candidate: &Image,
    ref_rva: u32,
    cand_rva: u32,
    len: usize,
) -> Option<(usize, usize)> {
    let (needle, _) = body(reference, ref_rva, len)?;
    let (hay, _) = body(candidate, cand_rva, len)?;
    let (mask, _) = mask_of(reference, ref_rva, needle)?;
    let mut differing = 0usize;
    let mut comparable = 0usize;
    for (i, (a, b)) in needle.iter().zip(hay).enumerate() {
        if mask.get(i) == Some(&true) {
            continue;
        }
        comparable += 1;
        if a != b {
            differing += 1;
        }
    }
    Some((differing, comparable))
}

#[derive(Debug, Clone)]
pub struct Located {
    pub rva: u32,
    pub len: usize,
    pub masked: usize,
    pub from_relocs: usize,
    pub hits: Vec<Hit>,
    pub exhausted: bool,
}

pub fn locate(reference: &Image, candidate: &Image, rva: u32, len: usize) -> Result<Located> {
    if len == 0 {
        bail!("the body at {rva:#x} records an empty span");
    }
    let (needle, _) = body(reference, rva, len)
        .ok_or_else(|| anyhow!("{rva:#x}+{len:#x} runs past the reference's .text"))?;
    let (mask, from_relocs) =
        mask_of(reference, rva, needle).ok_or_else(|| anyhow!("{rva:#x}+{len:#x} overflows"))?;
    let ot = candidate
        .section(".text")
        .ok_or_else(|| anyhow!("the candidate has no .text"))?;
    let found = masked_find(candidate.section_bytes(ot), needle, &mask, SEARCH_BUDGET);
    let hits = found
        .starts
        .into_iter()
        .filter_map(|p| {
            let r = ot.rva.get().checked_add(u32::try_from(p).ok()?)?;
            Some(Hit {
                rva: r,
                shift: i64::from(r) - i64::from(rva),
            })
        })
        .collect();
    Ok(Located {
        rva,
        len,
        masked: mask.iter().filter(|m| **m).count(),
        from_relocs,
        hits,
        exhausted: found.exhausted,
    })
}

#[must_use]
pub fn lines(label: &str, name: &str, thunk: bool, candidate: &str, l: &Located) -> Vec<String> {
    let pct = if l.len == 0 {
        0.0
    } else {
        100.0 * l.masked as f64 / l.len as f64
    };
    let mut out = vec![
        format!(
            "{label}: LOCATE {name}{}",
            if thunk { "  (a THUNK, not a body)" } else { "" }
        ),
        format!(
            "  reference  rva {:#08x}  body {} byte(s), {} MASKED ({pct:.0}%) -- {} from .reloc, {} more as `rel32` operands",
            l.rva,
            l.len,
            l.masked,
            l.from_relocs,
            l.masked.saturating_sub(l.from_relocs)
        ),
        format!("  candidate  {candidate}"),
    ];
    if l.masked.saturating_mul(3) > l.len {
        out.push(
            "  warning: over a third of this body is masked, so the query is WEAK".to_string(),
        );
    }
    if l.exhausted {
        out.push(format!(
            "  THE SEARCH RAN OUT of its {SEARCH_BUDGET} byte comparisons: no answer"
        ));
        return out;
    }
    match l.hits.as_slice() {
        [] => out.push(
            "  NOT FOUND UNDER THIS MASK: not `absent from the image'; an operand our link relocates and the reference does not stays literal".to_string(),
        ),
        [h] => out.push(format!(
            "  ONE MATCH at {:#08x}  shift {:+}",
            h.rva, h.shift
        )),
        many => {
            out.push(format!(
                "  {} MATCHES -- this body is NOT distinguishing under the mask:",
                many.len()
            ));
            for h in many.iter().take(8) {
                out.push(format!("      {:#08x}  shift {:+}", h.rva, h.shift));
            }
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_body_differing_only_in_relocated_operands_is_found() {
        let needle = vec![0x55, 0x8b, 0xec, 0xe8, 0x11, 0x22, 0x33, 0x44, 0xc3];
        let mut hay = vec![0x90; 32];
        let mut copy = needle.clone();
        if let Some(s) = copy.get_mut(4..8) {
            s.copy_from_slice(&[0xaa, 0xbb, 0xcc, 0xdd]);
        }
        if let Some(s) = hay.get_mut(10..10 + copy.len()) {
            s.copy_from_slice(&copy);
        }
        let mut mask = vec![false; needle.len()];
        for slot in mask.iter_mut().take(8).skip(4) {
            *slot = true;
        }
        assert_eq!(
            masked_find(&hay, &needle, &mask, SEARCH_BUDGET).starts,
            vec![10]
        );
        assert!(
            masked_find(&hay, &needle, &vec![false; needle.len()], SEARCH_BUDGET)
                .starts
                .is_empty()
        );
    }

    #[test]
    fn a_shared_pattern_reports_every_hit_and_masking_clamps() {
        let needle = vec![0x55, 0x8b, 0xec, 0x83, 0xec];
        let mut hay = vec![0u8; 40];
        if let Some(s) = hay.get_mut(4..9) {
            s.copy_from_slice(&needle);
        }
        if let Some(s) = hay.get_mut(20..25) {
            s.copy_from_slice(&needle);
        }
        assert_eq!(
            masked_find(&hay, &needle, &vec![false; needle.len()], SEARCH_BUDGET).starts,
            vec![4, 20]
        );
        assert_eq!(
            masked_offsets(&[0x1006, 0x2000], 0x1000, 0x1008),
            vec![6, 7]
        );
        assert!(masked_offsets(&[0x900], 0x1000, 0x1008).is_empty());
        assert!(
            masked_find(&[1, 2, 3], &[], &[], SEARCH_BUDGET)
                .starts
                .is_empty()
        );
    }

    #[test]
    fn a_search_that_matches_everywhere_runs_out_of_its_budget() {
        let hay = vec![0u8; 4096];
        let needle = vec![0u8; 64];
        let f = masked_find(&hay, &needle, &[false; 64], 10_000);
        assert!(f.exhausted);
        assert!(f.starts.len() < 200);
    }
}
