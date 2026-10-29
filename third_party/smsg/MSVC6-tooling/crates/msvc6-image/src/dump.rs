use msvc6_base::le;
use msvc6_base::units::Rva;
use serde::Serialize;

use crate::pe::Image;
use crate::{u32_of, us};

pub const PER_LINE: usize = 16;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SectionRow {
    pub name: String,
    pub rva: u32,
    pub mapped_end: u32,
    pub virt_size: u32,
    pub raw_size: u32,
    pub raw_off: usize,
    pub flags: u32,
    pub tail_from: Option<u32>,
    pub tail: u32,
}

#[must_use]
pub fn sections(img: &Image) -> Vec<SectionRow> {
    img.sections
        .iter()
        .map(|s| {
            let mapped = s.mapped_len();
            let disk = u32_of(s.disk_len());
            let tail = s.virt_size.get().saturating_sub(s.raw_size.get());
            SectionRow {
                name: s.name.clone(),
                rva: s.rva.get(),
                mapped_end: s.rva.get().saturating_add(mapped),
                virt_size: s.virt_size.get(),
                raw_size: s.raw_size.get(),
                raw_off: s.raw_off.get(),
                flags: s.flags,
                tail_from: (tail > 0).then(|| s.rva.get().saturating_add(disk)),
                tail,
            }
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub enum Where {
    Headers {
        file_off: usize,
    },
    Section {
        name: String,
        offset: u32,
        file_off: Option<usize>,
        disk_end: u32,
    },
    Outside,
}

#[must_use]
pub fn locate(img: &Image, rva: Rva) -> Where {
    if let Some(s) = img.section_of(rva) {
        let offset = rva.get().saturating_sub(s.rva.get());
        let disk = u32_of(s.disk_len());
        return Where::Section {
            name: s.name.clone(),
            offset,
            file_off: (offset < disk).then(|| s.raw_off.get().saturating_add(us(offset))),
            disk_end: s.rva.get().saturating_add(disk),
        };
    }
    if rva.get() < img.opt.size_of_headers {
        return Where::Headers {
            file_off: us(rva.get()),
        };
    }
    Where::Outside
}

#[must_use]
pub fn hex_lines(at: u32, b: &[u8]) -> Vec<String> {
    b.chunks(PER_LINE)
        .enumerate()
        .map(|(i, chunk)| {
            let a = at.saturating_add(u32_of(i.saturating_mul(PER_LINE)));
            let hex: Vec<String> = chunk.iter().map(|c| format!("{c:02x}")).collect();
            let ascii: String = chunk
                .iter()
                .map(|c| {
                    if c.is_ascii_graphic() || *c == b' ' {
                        char::from(*c)
                    } else {
                        '.'
                    }
                })
                .collect();
            format!("  {a:#010x}  {:<47}  |{ascii}|", hex.join(" "))
        })
        .collect()
}

#[must_use]
pub fn dword_target(img: &Image, b: &[u8]) -> Option<(u32, Option<(u32, String)>)> {
    let d = le::u32_at(b, 0).ok()?;
    let as_rva = d.checked_sub(img.opt.image_base.get());
    let sec = as_rva.and_then(|r| img.section_of(Rva(r)).map(|s| (r, s.name.clone())));
    Some((d, sec))
}

#[must_use]
pub fn final_zero_run(bytes: &[u8]) -> usize {
    bytes.iter().rev().take_while(|b| **b == 0).count()
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct MapTextbss {
    pub begin: u32,
    pub end: u32,
    pub row_len: Option<u32>,
}

fn sec_offset(tok: &str, section: &str) -> Option<u32> {
    let (s, off) = tok.split_once(':')?;
    if s != section {
        return None;
    }
    u32::from_str_radix(off, 16).ok()
}

#[must_use]
pub fn map_textbss(map: &str, section: &str) -> Option<MapTextbss> {
    let (mut begin, mut end, mut row_len) = (None, None, None);
    for l in map.lines() {
        let t: Vec<&str> = l.split_whitespace().collect();
        match (t.first().copied(), t.get(1).copied(), t.get(2).copied()) {
            (Some(at), Some("__enc$textbss$begin"), _) => begin = sec_offset(at, section),
            (Some(at), Some("__enc$textbss$end"), _) => end = sec_offset(at, section),
            (Some(at), Some(len), Some(".textbss")) if sec_offset(at, section).is_some() => {
                row_len = len
                    .strip_suffix('H')
                    .and_then(|x| u32::from_str_radix(x, 16).ok());
            }
            _ => {}
        }
    }
    Some(MapTextbss {
        begin: begin?,
        end: end?,
        row_len,
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Textbss {
    pub text_rva: u32,
    pub text_end: u32,
    pub run_from: u32,
    pub run: u32,
}

#[must_use]
pub fn textbss(img: &Image, text: &str) -> Option<Textbss> {
    let s = img.section(text)?;
    let vs = s.virt_size.get();
    let disk = img.section_bytes(s);
    let on_disk = disk.get(..disk.len().min(us(vs))).unwrap_or_default();
    let virtual_tail = vs.saturating_sub(u32_of(on_disk.len()));
    let run = u32_of(final_zero_run(on_disk)).saturating_add(virtual_tail);
    let text_end = s.rva.get().saturating_add(vs);
    Some(Textbss {
        text_rva: s.rva.get(),
        text_end,
        run_from: text_end.saturating_sub(run),
        run,
    })
}
