use crate::functions::Function;
use iced_x86::{Decoder, DecoderOptions, Mnemonic, OpKind};
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};

pub const IMAGE_SCN_CNT_CODE: u32 = 0x20;
pub const IMAGE_SCN_MEM_EXECUTE: u32 = 0x2000_0000;

#[must_use]
pub fn is_protector_section(name: &str) -> bool {
    let n = name.trim_end_matches('\0').to_ascii_lowercase();
    n == ".cms_t" || n == ".cms_d" || n.starts_with("stxt") || n.starts_with(".stxt")
}

#[must_use]
pub fn is_code_section(name: &str, flags: u32) -> bool {
    flags & IMAGE_SCN_MEM_EXECUTE != 0
        && flags & IMAGE_SCN_CNT_CODE != 0
        && !is_protector_section(name)
}

struct Code<'a> {
    base: u32,
    exec: Vec<(u32, &'a [u8])>,
}

impl Code<'_> {
    fn section(&self, rva: u32) -> Option<(u32, &[u8])> {
        self.exec
            .iter()
            .find(|(lo, b)| {
                rva >= *lo
                    && u64::from(rva)
                        < u64::from(*lo).saturating_add(u64::try_from(b.len()).unwrap_or(u64::MAX))
            })
            .map(|(lo, b)| (*lo, *b))
    }

    fn is_exec(&self, rva: u32) -> bool {
        self.section(rva).is_some()
    }

    fn at(&self, rva: u32, len: usize) -> Option<&[u8]> {
        let (lo, b) = self.section(rva)?;
        let off = usize::try_from(rva.checked_sub(lo)?).ok()?;
        b.get(off..off.checked_add(len)?)
    }

    fn jump_target(&self, rva: u32) -> Option<u32> {
        let b = self.at(rva, 5)?;
        if b.first() != Some(&0xe9) {
            return None;
        }
        let rel = i32::from_le_bytes([*b.get(1)?, *b.get(2)?, *b.get(3)?, *b.get(4)?]);
        let next = u32::try_from(i64::from(rva) + 5 + i64::from(rel)).ok()?;
        self.is_exec(next).then_some(next)
    }

    fn import_jump(&self, rva: u32) -> bool {
        self.at(rva, 2) == Some(&[0xff, 0x25][..])
    }
}

fn after_boundary(sec: &[u8], off: usize) -> bool {
    let prev = off.checked_sub(1).and_then(|p| sec.get(p)).copied();
    let ret_n = off
        .checked_sub(3)
        .and_then(|p| sec.get(p))
        .is_some_and(|b| *b == 0xc2)
        && prev == Some(0);
    matches!(prev, None | Some(0xc3 | 0xcc | 0x90)) || ret_n
}

#[must_use]
pub fn prologue_at(sec: &[u8], off: usize) -> bool {
    sec.get(off..off.saturating_add(3)) == Some(&[0x55, 0x8b, 0xec][..]) && after_boundary(sec, off)
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
    let ret = k.checked_sub(1).and_then(|p| sec.get(p)) == Some(&0xc3);
    let ret_n = k.checked_sub(3).and_then(|p| sec.get(p)) == Some(&0xc2)
        && k.checked_sub(1).and_then(|p| sec.get(p)) == Some(&0);
    ret || ret_n
}

fn trim_pad(code: &Code<'_>, rva: u32, next: u32) -> u32 {
    let len = usize::try_from(next.saturating_sub(rva)).unwrap_or(0);
    let Some(b) = code.at(rva, len) else {
        return next;
    };
    let mut end = b.len();
    while end > 1
        && b.get(end - 1)
            .is_some_and(|c| matches!(c, 0xcc | 0x90 | 0x00))
    {
        end -= 1;
    }
    rva.saturating_add(u32::try_from(end).unwrap_or(0))
}

#[must_use]
pub fn discover(img: &Image) -> Vec<Function> {
    let base = img.opt.image_base.get();
    let code = Code {
        base,
        exec: img
            .sections
            .iter()
            .filter(|s| is_code_section(&s.name, s.flags))
            .map(|s| (s.rva.get(), img.section_bytes(s)))
            .collect(),
    };
    let mut starts: BTreeSet<u32> = BTreeSet::new();
    let entry = img.opt.entry_point.get();
    if code.is_exec(entry) {
        starts.insert(entry);
    }
    if let Ok(Some(e)) = img.exports() {
        for x in e.entries.iter().filter(|x| x.forwarder.is_none()) {
            if code.is_exec(x.target.get()) {
                starts.insert(x.target.get());
            }
        }
    }
    for (lo, sec) in &code.exec {
        for off in 0..sec.len() {
            if (prologue_at(sec, off) || aligned_after_pad(sec, off))
                && let Ok(o) = u32::try_from(off)
            {
                starts.insert(lo.saturating_add(o));
            }
        }
        let mut d = Decoder::with_ip(
            32,
            sec,
            u64::from(code.base) + u64::from(*lo),
            DecoderOptions::NONE,
        );
        while d.can_decode() {
            let i = d.decode();
            if i.is_invalid() {
                continue;
            }
            if i.mnemonic() == Mnemonic::Call
                && i.op0_kind() == OpKind::NearBranch32
                && let Some(t) = u32::try_from(i.near_branch_target())
                    .ok()
                    .and_then(|t| t.checked_sub(code.base))
                && code.is_exec(t)
            {
                starts.insert(t);
            }
        }
    }
    for s in img
        .sections
        .iter()
        .filter(|s| s.flags & IMAGE_SCN_MEM_EXECUTE == 0 && !is_protector_section(&s.name))
    {
        let b = img.section_bytes(s);
        for w in b.as_chunks::<4>().0 {
            let v = u32::from_le_bytes(*w);
            let Some(t) = v.checked_sub(base) else {
                continue;
            };
            let Some((lo, sec)) = code.section(t) else {
                continue;
            };
            let Ok(off) = usize::try_from(t - lo) else {
                continue;
            };
            if prologue_at(sec, off) || (after_boundary(sec, off) && sec.get(off) != Some(&0xcc)) {
                starts.insert(t);
            }
        }
    }
    let mut thunks: BTreeMap<u32, Option<u32>> = BTreeMap::new();
    let mut extra = Vec::new();
    for &s in &starts {
        if let Some(t) = code.jump_target(s) {
            thunks.insert(s, Some(t));
            extra.push(t);
        } else if code.import_jump(s) {
            thunks.insert(s, None);
        }
    }
    starts.extend(extra);
    let all: Vec<u32> = starts.into_iter().collect();
    let mut out = Vec::with_capacity(all.len());
    for (k, &s) in all.iter().enumerate() {
        let Some((lo, sec)) = code.section(s) else {
            continue;
        };
        let sec_end = lo.saturating_add(u32::try_from(sec.len()).unwrap_or(u32::MAX));
        let end = match thunks.get(&s) {
            Some(Some(_)) => s.saturating_add(5),
            Some(None) => s.saturating_add(6),
            None => {
                let next = all.get(k + 1).copied().unwrap_or(sec_end).min(sec_end);
                trim_pad(&code, s, next)
            }
        };
        out.push(Function {
            rva: s,
            end: Some(end),
            name: None,
            thunk: thunks.contains_key(&s),
            provider: "discover".to_string(),
            object: None,
        });
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn prologues_follow_a_return_or_pad() {
        let code = [
            0xc3, 0x55, 0x8b, 0xec, 0xc3, 0x55, 0x8b, 0xec, 0x00, 0x55, 0x8b, 0xec,
        ];
        assert!(prologue_at(&code, 1));
        assert!(prologue_at(&code, 5));
        assert!(!prologue_at(&code, 9));
        let mut pad = vec![0x90u8; 32];
        pad[10] = 0xc3;
        pad[16] = 0x8b;
        assert!(aligned_after_pad(&pad, 16));
        assert!(!aligned_after_pad(&pad, 8));
        assert!(is_protector_section("stxt774"));
        assert!(!is_code_section(".data", 0xc000_0040));
        assert!(is_code_section(".text", 0x6000_0020));
    }
}
