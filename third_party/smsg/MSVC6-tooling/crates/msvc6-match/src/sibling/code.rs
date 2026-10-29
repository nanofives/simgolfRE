use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use msvc6_image::pe::image::{IMAGE_SCN_CNT_CODE, IMAGE_SCN_MEM_EXECUTE};
use std::collections::HashMap;

pub const MIN_STR: usize = 4;

pub const MAX_STR: usize = 512;

pub const THUNK_HOPS: usize = 4;

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

#[must_use]
pub fn str_key(s: &str) -> String {
    s.trim()
        .trim_end_matches(['!', '.', ':', ' ', '\t', '\r', '\n'])
        .trim()
        .to_string()
}

#[must_use]
pub fn plausible_string(s: &[u8], min: usize) -> bool {
    s.len() >= min
        && s.iter()
            .all(|c| (0x20..0x7f).contains(c) || matches!(c, b'\t' | b'\r' | b'\n') || *c >= 0xa0)
        && s.iter().any(u8::is_ascii_alphabetic)
}

#[derive(Debug)]
pub struct Code<'a> {
    pub img: &'a Image,
    pub base: u32,
    pub exec: Vec<(u32, u32)>,
    pub iat: HashMap<u32, String>,
}

pub(crate) fn le4(b: Option<&[u8]>) -> [u8; 4] {
    b.and_then(|s| <[u8; 4]>::try_from(s).ok())
        .unwrap_or([0; 4])
}

impl<'a> Code<'a> {
    #[must_use]
    pub fn new(img: &'a Image) -> Self {
        let base = img.opt.image_base.get();
        let exec = img
            .sections
            .iter()
            .filter(|s| is_code_section(&s.name, s.flags))
            .map(|s| {
                let lo = s.rva.get();
                let len = u32::try_from(s.disk_len()).unwrap_or(0);
                (lo, lo.saturating_add(len))
            })
            .collect();
        let mut iat = HashMap::new();
        for d in img.imports().unwrap_or_default() {
            let dll = d.dll.to_ascii_lowercase();
            for e in d.entries {
                iat.insert(e.iat_rva.get(), format!("{dll}!{}", e.name));
            }
        }
        Self {
            img,
            base,
            exec,
            iat,
        }
    }

    #[must_use]
    pub fn is_exec(&self, rva: u32) -> bool {
        self.exec.iter().any(|(lo, hi)| rva >= *lo && rva < *hi)
    }

    #[must_use]
    pub fn rva_of(&self, va: u64) -> Option<u32> {
        u32::try_from(va).ok()?.checked_sub(self.base)
    }

    #[must_use]
    pub fn va(&self, rva: u32) -> u64 {
        u64::from(self.base).saturating_add(u64::from(rva))
    }

    #[must_use]
    pub fn bytes(&self, rva: u32, len: usize) -> Option<&[u8]> {
        self.img.at_rva(Rva(rva), len)
    }

    #[must_use]
    pub fn code_bounds(&self) -> (u64, u64) {
        let lo = self.exec.iter().map(|e| e.0).min().unwrap_or(0);
        let hi = self.exec.iter().map(|e| e.1).max().unwrap_or(0);
        (self.va(lo), self.va(hi))
    }

    pub fn resolve(&self, mut rva: u32) -> Result<u32, String> {
        for _ in 0..THUNK_HOPS {
            let Some(b) = self.bytes(rva, 6) else {
                return Ok(rva);
            };
            match (b.first(), b.get(1)) {
                (Some(0xe9), _) => {
                    let rel = i32::from_le_bytes(le4(b.get(1..5)));
                    let Ok(next) = u32::try_from(i64::from(rva) + 5 + i64::from(rel)) else {
                        return Ok(rva);
                    };
                    if !self.is_exec(next) {
                        return Ok(rva);
                    }
                    rva = next;
                }
                (Some(0xff), Some(0x25)) => {
                    let abs = u32::from_le_bytes(le4(b.get(2..6)));
                    return match abs.checked_sub(self.base).and_then(|r| self.iat.get(&r)) {
                        Some(n) => Err(n.clone()),
                        None => Ok(rva),
                    };
                }
                _ => return Ok(rva),
            }
        }
        Ok(rva)
    }

    #[must_use]
    pub fn string_at(&self, rva: u32) -> Option<String> {
        if self.is_exec(rva) {
            return None;
        }
        let sec = self
            .img
            .section_of(Rva(rva))
            .filter(|s| !is_protector_section(&s.name))?;
        let avail = usize::try_from(
            sec.rva
                .get()
                .saturating_add(u32::try_from(sec.disk_len()).ok()?)
                .saturating_sub(rva),
        )
        .ok()?;
        let raw = self.bytes(rva, avail.min(MAX_STR))?;
        let end = raw.iter().position(|c| *c == 0)?;
        let s = raw.get(..end)?;
        plausible_string(s, MIN_STR).then(|| msvc6_base::srcio::decode(s))
    }
}
