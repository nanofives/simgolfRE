use super::code::{Code, is_protector_section};
use super::feat::{Fun, decode};
use iced_x86::{Decoder, DecoderOptions, FlowControl, OpKind};
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use std::collections::BTreeSet;

#[must_use]
pub fn prologue_at(sec: &[u8], off: usize) -> bool {
    off.checked_add(3).and_then(|e| sec.get(off..e)) == Some(&[0x55, 0x8b, 0xec][..])
        && after_boundary(sec, off)
}

#[must_use]
pub fn after_boundary(sec: &[u8], off: usize) -> bool {
    let prev = off.checked_sub(1).and_then(|p| sec.get(p)).copied();
    let ret_n = off
        .checked_sub(3)
        .and_then(|p| sec.get(p))
        .is_some_and(|b| *b == 0xc2)
        && prev == Some(0);
    matches!(prev, None | Some(0xc3 | 0xcc | 0x90)) || ret_n
}

#[must_use]
pub fn aligned_after_pad(sec: &[u8], off: usize) -> bool {
    if !off.is_multiple_of(16) || matches!(sec.get(off), None | Some(0xcc | 0x90 | 0x00)) {
        return false;
    }
    let mut k = off;
    while k > 0 && off - k < 16 && matches!(sec.get(k - 1), Some(0xcc | 0x90)) {
        k -= 1;
    }
    if k == off || off - k >= 16 {
        return false;
    }
    let before = |n: usize| k.checked_sub(n).and_then(|p| sec.get(p)).copied();
    before(1) == Some(0xc3) || (before(3) == Some(0xc2) && before(1) == Some(0))
}

fn code_starts(code: &Code<'_>, lo: u32, sec: &[u8], starts: &mut BTreeSet<u32>) {
    for off in 0..sec.len() {
        if (prologue_at(sec, off) || aligned_after_pad(sec, off))
            && let Ok(o) = u32::try_from(off)
        {
            starts.insert(lo.saturating_add(o));
        }
    }
    let mut d = Decoder::with_ip(32, sec, code.va(lo), DecoderOptions::NONE);
    while d.can_decode() {
        let i = d.decode();
        if i.is_invalid() {
            continue;
        }
        if i.flow_control() == FlowControl::Call
            && i.op0_kind() == OpKind::NearBranch32
            && let Some(t) = code.rva_of(i.near_branch_target())
            && code.is_exec(t)
        {
            starts.insert(t);
        }
    }
}

fn data_pointers(code: &Code<'_>, starts: &mut BTreeSet<u32>) {
    for s in code
        .img
        .sections
        .iter()
        .filter(|s| s.flags & IMAGE_SCN_MEM_EXECUTE == 0 && !is_protector_section(&s.name))
    {
        for w in code.img.section_bytes(s).as_chunks::<4>().0 {
            let Some(t) = u32::from_le_bytes(*w).checked_sub(code.base) else {
                continue;
            };
            let Some((lo, hi)) = code.exec.iter().find(|(lo, hi)| t >= *lo && t < *hi) else {
                continue;
            };
            let (Ok(off), Some(sec)) = (
                usize::try_from(t - lo),
                code.bytes(*lo, usize::try_from(hi - lo).unwrap_or(0)),
            ) else {
                continue;
            };
            if prologue_at(sec, off) || (after_boundary(sec, off) && sec.get(off) != Some(&0xcc)) {
                starts.insert(t);
            }
        }
    }
}

#[must_use]
pub fn trim_pad(code: &Code<'_>, rva: u32, next: u32) -> u32 {
    let len = usize::try_from(next.saturating_sub(rva)).unwrap_or(0);
    let Some(b) = code.bytes(rva, len) else {
        return next;
    };
    let mut end = b.len();
    while end > 0
        && b.get(end - 1)
            .is_some_and(|c| matches!(c, 0xcc | 0x90 | 0x00))
    {
        end -= 1;
    }
    let mut edge = 0u32;
    for i in &decode(code, rva, next) {
        let e = u32::try_from(i.next_ip().saturating_sub(code.va(rva))).unwrap_or(0);
        edge = e;
        if usize::try_from(e).unwrap_or(usize::MAX) >= end {
            break;
        }
    }
    rva.saturating_add(edge).min(next)
}

#[must_use]
pub fn discover(code: &Code<'_>, label: &str) -> Vec<Fun> {
    let mut starts: BTreeSet<u32> = BTreeSet::new();
    starts.insert(code.img.opt.entry_point.get());
    if let Ok(Some(e)) = code.img.exports() {
        for x in e.entries {
            starts.insert(x.target.get());
        }
    }
    for (lo, hi) in &code.exec {
        let len = usize::try_from(hi - lo).unwrap_or(0);
        if let Some(sec) = code.bytes(*lo, len) {
            code_starts(code, *lo, sec, &mut starts);
        }
    }
    data_pointers(code, &mut starts);
    let resolved: BTreeSet<u32> = starts
        .iter()
        .filter_map(|s| match code.resolve(*s) {
            Ok(r) if r == *s => Some(r),
            Ok(r) => code.is_exec(r).then_some(r),
            Err(_) => None,
        })
        .collect();
    let thunks: BTreeSet<u32> = starts
        .iter()
        .copied()
        .filter(|s| code.resolve(*s) != Ok(*s))
        .collect();
    let all: Vec<u32> = resolved.union(&thunks).copied().collect();
    let mut out = Vec::new();
    for (k, s) in all.iter().enumerate() {
        if thunks.contains(s) {
            continue;
        }
        let sec_end = code
            .exec
            .iter()
            .find(|(lo, hi)| s >= lo && s < hi)
            .map_or(*s, |e| e.1);
        let next = all
            .get(k.saturating_add(1))
            .copied()
            .unwrap_or(sec_end)
            .min(sec_end);
        out.push(Fun {
            image: label.to_string(),
            rva: *s,
            end: trim_pad(code, *s, next),
            name: format!("sub_{:x}", code.va(*s)),
            file: String::new(),
        });
    }
    out
}

#[must_use]
pub fn functions(code: &Code<'_>) -> Vec<(u32, u32)> {
    discover(code, "")
        .into_iter()
        .map(|f| (f.rva, f.end))
        .collect()
}

#[must_use]
pub fn atoms(funs: &[(u32, u32)]) -> Vec<usize> {
    let mut out = Vec::with_capacity(funs.len());
    let mut id = 0usize;
    let mut prev_end: Option<u32> = None;
    for (rva, end) in funs {
        let joins = prev_end == Some(*rva) && !rva.is_multiple_of(16);
        if !joins && prev_end.is_some() {
            id = id.saturating_add(1);
        }
        out.push(id);
        prev_end = Some(*end);
    }
    out
}
