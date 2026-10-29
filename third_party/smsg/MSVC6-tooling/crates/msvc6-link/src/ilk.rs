use anyhow::{Result, bail};
use serde::Serialize;
use std::collections::BTreeSet;

pub const MAGIC: &[u8] = b"Microsoft Linker Database\n";

pub const VERSION: u16 = 6;

pub const VERSION_AT: usize = 0x20;

pub const HDR_SELF_PTR: usize = 0x1d4;

pub const HDR_LEN: u32 = 0x2a4;

pub const MAX_ILK: u64 = 1 << 30;

pub const REC: usize = 60;

pub const RECORD_CAP: usize = 4096;

pub const VALID_RUN: usize = 5;

pub const SHOWN_RUNS: usize = 6;

pub const SHOWN_HITS: usize = 6;

#[must_use]
pub fn le32(b: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(
        b.get(at..at.checked_add(4)?)?.try_into().ok()?,
    ))
}

#[must_use]
pub fn name_at(b: &[u8], off: usize) -> Option<String> {
    let rest = b.get(off..off.saturating_add(16).min(b.len()))?;
    let end = rest.iter().position(|c| *c == 0)?;
    let s = rest.get(..end)?;
    (s.len() >= 2 && s.first() == Some(&b'.') && s.iter().all(u8::is_ascii_graphic))
        .then(|| s.iter().map(|c| char::from(*c)).collect())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Header {
    pub version: u16,
    pub base: u32,
    pub self_ptr: u32,
}

pub fn header(b: &[u8]) -> Result<Header> {
    if !b.starts_with(MAGIC) {
        bail!("it does not open with the `.ilk' signature, so it is not a linker database");
    }
    let Some(v) = b.get(VERSION_AT..VERSION_AT + 2) else {
        bail!("truncated header");
    };
    let version = u16::from_le_bytes([
        v.first().copied().unwrap_or(0),
        v.get(1).copied().unwrap_or(0),
    ]);
    if version != VERSION {
        bail!(
            "version {version} at +0x20, expected {VERSION}: LINK 6.00 refuses it with diagnostic 6020"
        );
    }
    let Some(self_ptr) = le32(b, HDR_SELF_PTR) else {
        bail!("no header pointer at +0x1d4");
    };
    let Some(base) = self_ptr.checked_sub(HDR_LEN) else {
        bail!("+0x1d4 = {self_ptr:#x} is below the header length, so the base cannot be derived");
    };
    Ok(Header {
        version,
        base,
        self_ptr,
    })
}

#[must_use]
pub fn find_dword(b: &[u8], needle: u32) -> Vec<usize> {
    b.as_chunks::<4>()
        .0
        .iter()
        .enumerate()
        .filter(|(_, w)| u32::from_le_bytes(**w) == needle)
        .map(|(i, _)| i * 4)
        .collect()
}

#[must_use]
pub fn walk_records(b: &[u8], known: &BTreeSet<u32>) -> Vec<(usize, usize)> {
    let hit = |at: usize| le32(b, at).is_some_and(|v| known.contains(&v));
    let mut runs: Vec<(usize, usize)> = Vec::new();
    let mut seen: BTreeSet<usize> = BTreeSet::new();
    let mut at = 0usize;
    while at.saturating_add(REC) <= b.len() {
        if hit(at) && !seen.contains(&at) {
            let mut lo = at;
            while lo >= REC && hit(lo - REC) && !seen.contains(&(lo - REC)) {
                lo -= REC;
            }
            let mut hi = lo;
            let mut n = 0usize;
            while hi.saturating_add(REC) <= b.len() && hit(hi) && seen.insert(hi) {
                hi += REC;
                n += 1;
            }
            if n >= 2 {
                runs.push((lo, n));
            }
        }
        at += 4;
    }
    runs.sort_by_key(|(_, n)| std::cmp::Reverse(*n));
    runs
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Record {
    pub at: usize,
    pub name: String,
    pub rva: u32,
    pub virt_size: u32,
    pub chars: u32,
    pub unknowns: [u32; 4],
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Records {
    pub found: usize,
    pub kept: Vec<Record>,
}

#[must_use]
pub fn section_records(b: &[u8], base: u32) -> Records {
    let mut r = Records::default();
    let mut at = 0usize;
    while at.saturating_add(0x24) <= b.len() {
        let rec = le32(b, at)
            .and_then(|p| p.checked_sub(base))
            .and_then(|off| usize::try_from(off).ok())
            .filter(|off| *off < b.len())
            .and_then(|off| name_at(b, off))
            .and_then(|name| {
                Some(Record {
                    at,
                    name,
                    rva: le32(b, at + 4)?,
                    virt_size: le32(b, at + 0x10)?,
                    chars: le32(b, at + 0x1c)?,
                    unknowns: [
                        le32(b, at + 8)?,
                        le32(b, at + 0xc)?,
                        le32(b, at + 0x14)?,
                        le32(b, at + 0x18)?,
                    ],
                })
            });
        if let Some(rec) = rec {
            r.found += 1;
            if r.kept.len() < RECORD_CAP {
                r.kept.push(rec);
            }
        }
        at += 4;
    }
    r
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Control {
    NotRun,
    Agrees(usize, usize),
    NoVerdict(usize),
}

#[must_use]
pub fn record_lines(rs: &Records, truth: &[(String, u32, u32)]) -> (Control, Vec<String>) {
    let mut out = vec![format!(
        "  {:<10} {:>9} {:>10} {:>10} {:>11}  vs the image",
        "name", "at", "rva", "VirtSize", "chars"
    )];
    let (mut agree, mut checked) = (0usize, 0usize);
    for r in &rs.kept {
        let verdict = match truth.iter().find(|(n, _, _)| *n == r.name) {
            Some((_, rva, vs)) => {
                checked += 1;
                if *rva == r.rva && *vs == r.virt_size {
                    agree += 1;
                    "AGREES (rva and VirtualSize)".to_string()
                } else {
                    format!("DIFFERS: image rva {rva:#x} VirtualSize {vs}")
                }
            }
            None => "no such section in the image".to_string(),
        };
        out.push(format!(
            "  {:<10} {:>#9x} {:>#10x} {:>10} {:>#11x}  {verdict}",
            r.name, r.at, r.rva, r.virt_size, r.chars
        ));
        out.push(format!(
            "  {:<10} {:>9} unknown +0x08 {:#x}  +0x0c {:#x}  +0x14 {:#x}  +0x18 {:#x}",
            "", "", r.unknowns[0], r.unknowns[1], r.unknowns[2], r.unknowns[3]
        ));
    }
    if rs.found > rs.kept.len() {
        out.push(format!(
            "  ... {} more record(s) of that shape past the first {RECORD_CAP}, not listed",
            rs.found - rs.kept.len()
        ));
    }
    let control = if truth.is_empty() {
        Control::NotRun
    } else if agree == 0 {
        Control::NoVerdict(checked)
    } else {
        Control::Agrees(agree, checked)
    };
    match control {
        Control::NotRun => out.push(
            "  NO CONTROL RUN: pass --image to check every record against that image's own section table; without it \
             these rows are a SHAPE MATCH and nothing more."
                .to_string(),
        ),
        Control::NoVerdict(n) => out.push(format!(
            "  NO VERDICT: {n} record(s) matched a section name and NONE agreed on rva and VirtualSize, so the record \
             layout is not what this reader assumes for this file."
        )),
        Control::Agrees(a, n) => out.push(format!(
            "  {a} of {n} record(s) AGREE with the image on BOTH rva and VirtualSize, so the layout holds here: +0x00 name \
             pointer, +0x04 rva, +0x10 VirtualSize, +0x1c characteristics."
        )),
    }
    (control, out)
}

#[must_use]
pub fn find_lines(b: &[u8], values: &[u32]) -> Vec<String> {
    let mut out = vec![
        "  DWORD SEARCH -- offsets holding each value, and the gaps between them:".to_string(),
    ];
    for v in values {
        let hits = find_dword(b, *v);
        if hits.is_empty() {
            out.push(format!("    {v:#010x}  NOT PRESENT anywhere 4-aligned"));
            continue;
        }
        let gaps: Vec<String> = hits
            .windows(2)
            .filter_map(|w| Some(format!("{}", w.get(1)? - w.first()?)))
            .take(8)
            .collect();
        out.push(format!(
            "    {v:#010x}  {} hit(s) at {}{}   gaps: {}",
            hits.len(),
            hits.iter()
                .take(SHOWN_HITS)
                .map(|h| format!("{h:#x}"))
                .collect::<Vec<_>>()
                .join(" "),
            if hits.len() > SHOWN_HITS { " ..." } else { "" },
            if gaps.is_empty() {
                "-- single hit, so no stride and NO structure is evidenced".to_string()
            } else {
                gaps.join(" ")
            }
        ));
    }
    out
}

#[must_use]
pub fn walk_lines(b: &[u8], known: &BTreeSet<u32>) -> (usize, Vec<String>) {
    let runs = walk_records(b, known);
    let total: usize = runs.iter().map(|(_, n)| n).sum();
    let mut out = vec![
        format!(
            "  VALIDATED RECORD WALK -- {} rva(s) taken from the map as ground truth",
            known.len()
        ),
        format!(
            "  {} run(s) of 2+ consecutive {REC}-byte slots whose +0x00 is a KNOWN rva; {total} slot(s) validated",
            runs.len()
        ),
    ];
    for (lo, n) in runs.iter().take(SHOWN_RUNS) {
        out.push(format!("    run at {lo:#08x}: {n} consecutive records"));
        for k in 0..(*n).min(4) {
            let o = lo + k * REC;
            out.push(format!(
                "      {o:#08x}  rva {:#08x}   +0x10 {:#x}   +0x18 {:#x}   +0x30 {:#x}   +0x38 {:#x}",
                le32(b, o).unwrap_or(0),
                le32(b, o + 0x10).unwrap_or(0),
                le32(b, o + 0x18).unwrap_or(0),
                le32(b, o + 0x30).unwrap_or(0),
                le32(b, o + 0x38).unwrap_or(0)
            ));
        }
    }
    let longest = runs.first().map_or(0, |(_, n)| *n);
    out.push(if longest < VALID_RUN {
        format!(
            "  NO VERDICT: the longest validated run is {longest} record(s), within chance for 32-bit values."
        )
    } else {
        format!(
            "  The longest run is {longest} consecutive records at a fixed {REC}-byte stride, each +0x00 an \
             independently known rva: the array is real and located; the fields past +0x00 remain unidentified."
        )
    });
    (longest, out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_section_shaped_names_are_accepted() {
        let mut b = vec![0u8; 64];
        b.splice(0..6, *b".text\0");
        assert_eq!(name_at(&b, 0).as_deref(), Some(".text"));
        b.splice(0..6, *b"text\0\0");
        assert_eq!(name_at(&b, 0), None);
        b.splice(0..2, *b".\0");
        assert_eq!(name_at(&b, 0), None);
        assert_eq!(name_at(&b, 999), None);
        assert_eq!(name_at(b".rdata", 0), None);
    }

    fn file(base: u32) -> Vec<u8> {
        let mut b = MAGIC.to_vec();
        b.resize(0x400, 0);
        b[VERSION_AT..VERSION_AT + 2].copy_from_slice(&VERSION.to_le_bytes());
        b[HDR_SELF_PTR..HDR_SELF_PTR + 4].copy_from_slice(&(base + HDR_LEN).to_le_bytes());
        b[0x300..0x306].copy_from_slice(b".text\0");
        let rec = 0x340;
        b[rec..rec + 4].copy_from_slice(&(base + 0x300).to_le_bytes());
        b[rec + 4..rec + 8].copy_from_slice(&0x1000u32.to_le_bytes());
        b[rec + 0x10..rec + 0x14].copy_from_slice(&0x5d1e0u32.to_le_bytes());
        b[rec + 0x1c..rec + 0x20].copy_from_slice(&0x6000_0020u32.to_le_bytes());
        b
    }

    #[test]
    fn the_base_comes_from_the_header_self_pointer_and_a_record_is_checked_against_the_image() {
        let b = file(0x3fff_0000);
        let h = header(&b).unwrap();
        assert_eq!(h.base, 0x3fff_0000);
        let rs = section_records(&b, h.base);
        assert_eq!(rs.found, 1);
        assert_eq!(rs.kept[0].virt_size, 0x5d1e0);
        let (c, _) = record_lines(&rs, &[(".text".to_string(), 0x1000, 0x5d1e0)]);
        assert_eq!(c, Control::Agrees(1, 1));
        let (c, _) = record_lines(&rs, &[(".text".to_string(), 0x1000, 1)]);
        assert_eq!(c, Control::NoVerdict(1));
        let mut bad = b.clone();
        bad[VERSION_AT] = 7;
        assert!(header(&bad).is_err());
        assert!(header(b"junk").is_err());
        let mut low = b;
        low[HDR_SELF_PTR..HDR_SELF_PTR + 4].copy_from_slice(&0x100u32.to_le_bytes());
        assert!(header(&low).is_err());
    }

    #[test]
    fn a_run_of_known_rvas_at_the_record_stride_is_found_once() {
        let mut b = vec![0u8; REC * 12];
        let known: BTreeSet<u32> = (0..8u32).map(|k| 0x1000 + k * 0x40).collect();
        for k in 0..8usize {
            let at = REC * (2 + k);
            b[at..at + 4]
                .copy_from_slice(&(0x1000 + u32::try_from(k).unwrap() * 0x40).to_le_bytes());
        }
        assert_eq!(walk_records(&b, &known), vec![(REC * 2, 8)]);
        assert_eq!(find_dword(&b, 0x1040), vec![REC * 3]);
        let (longest, _) = walk_lines(&b, &known);
        assert_eq!(longest, 8);
    }
}
