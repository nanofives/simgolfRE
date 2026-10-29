use msvc6_base::le;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::archive::Archive;
use crate::u32_of;

pub const DANS: u32 = 0x536e_6144;
pub const SEARCH: usize = 0x1000;
pub const MAX_OBJECTS: u32 = 50_000;

pub const PRODID_IMPORT: u32 = 0x01;
pub const C: [u32; 2] = [0x08, 0x0a];
pub const CPP: [u32; 1] = [0x0b];
pub const MASM: [u32; 3] = [0x0e, 0x0f, 0x12];

#[must_use]
pub fn prodid_name(id: u32) -> &'static str {
    match id {
        0x01 => "Import",
        0x02 => "Linker510",
        0x03 => "Cvtomf510",
        0x04 => "Linker600",
        0x05 => "Cvtomf600",
        0x06 => "Cvtres500",
        0x07 => "Utc11_Basic",
        0x08 => "Utc11_C",
        0x09 => "Utc12_Basic",
        0x0a => "Utc12_C",
        0x0b => "Utc12_CPP",
        0x0c => "AliasObj60",
        0x0d => "VisualBasic60",
        0x0e => "Masm613",
        0x0f => "Masm710",
        0x10 => "Linker511",
        0x11 => "Cvtomf511",
        0x12 => "Masm614",
        0x13 => "Linker512",
        _ => "?",
    }
}

#[must_use]
pub fn prodid_of(name: &str) -> Option<u32> {
    (0u32..0x14).find(|p| prodid_name(*p).eq_ignore_ascii_case(name))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct Stamp {
    pub prodid: u32,
    pub build: u32,
    pub objects: u32,
}

impl Stamp {
    #[must_use]
    pub fn comp_id(&self) -> u32 {
        (self.prodid << 16) | self.build
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Rich {
    pub key: u32,
    pub dans: usize,
    pub end: usize,
    pub stamps: Vec<Stamp>,
    pub checksum: u32,
}

impl Rich {
    #[must_use]
    pub fn key_is_checksum(&self) -> bool {
        self.key == self.checksum
    }

    #[must_use]
    pub fn objects(&self, prodid: u32) -> u32 {
        self.stamps
            .iter()
            .filter(|s| s.prodid == prodid)
            .map(|s| s.objects)
            .fold(0, u32::saturating_add)
    }

    #[must_use]
    pub fn family(&self, prodids: &[u32]) -> u32 {
        prodids
            .iter()
            .map(|p| self.objects(*p))
            .fold(0, u32::saturating_add)
    }

    #[must_use]
    pub fn families(&self) -> (u32, u32, u32) {
        (self.family(&C), self.family(&CPP), self.family(&MASM))
    }

    #[must_use]
    pub fn classes(&self) -> BTreeSet<u32> {
        self.stamps.iter().map(|s| s.prodid).collect()
    }

    #[must_use]
    pub fn builds(&self) -> BTreeSet<u32> {
        self.stamps.iter().map(|s| s.build).collect()
    }

    #[must_use]
    pub fn sorted(&self) -> Vec<Stamp> {
        let mut v = self.stamps.clone();
        v.sort_unstable();
        v
    }
}

#[must_use]
pub fn checksum(file: &[u8], dans: usize, stamps: &[Stamp]) -> u32 {
    let mut c = u32_of(dans);
    for (i, b) in file.iter().enumerate().take(dans) {
        if (0x3c..0x40).contains(&i) {
            continue;
        }
        c = c.wrapping_add(u32::from(*b).rotate_left(u32_of(i % 32)));
    }
    for s in stamps {
        c = c.wrapping_add(s.comp_id().rotate_left(s.objects % 32));
    }
    c
}

#[must_use]
pub fn decode(file: &[u8]) -> Option<Rich> {
    let head = file.get(..file.len().min(SEARCH))?;
    let rich = head
        .windows(4)
        .position(|w| w == b"Rich")
        .filter(|p| p.saturating_add(8) <= head.len())?;
    let key = le::u32_at(head, rich.saturating_add(4)).ok()?;
    let want = DANS ^ key;
    let dans = (0..rich)
        .find(|&o| le::u32_at(head, o).ok() == Some(want) && o.saturating_add(16) <= rich)?;
    let mut stamps = Vec::new();
    let mut at = dans.saturating_add(16);
    while at.saturating_add(8) <= rich {
        let cid = le::u32_at(head, at).ok()? ^ key;
        let cnt = le::u32_at(head, at.saturating_add(4)).ok()? ^ key;
        stamps.push(Stamp {
            prodid: cid >> 16,
            build: cid & 0xffff,
            objects: cnt,
        });
        at = at.saturating_add(8);
    }
    if stamps.is_empty()
        || stamps.iter().all(|s| s.prodid == 0)
        || stamps.iter().any(|s| s.objects > MAX_OBJECTS)
    {
        return None;
    }
    let sum = checksum(file, dans, &stamps);
    Some(Rich {
        key,
        dans,
        end: rich.saturating_add(8),
        stamps,
        checksum: sum,
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CensusRow {
    pub prodid: u32,
    pub product: &'static str,
    pub build: u32,
    pub reference: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub candidate: Option<u32>,
}

fn counts(r: Option<&Rich>) -> BTreeMap<(u32, u32), u32> {
    r.map(|r| {
        r.stamps
            .iter()
            .map(|s| ((s.prodid, s.build), s.objects))
            .collect()
    })
    .unwrap_or_default()
}

#[must_use]
pub fn census_rows(reference: Option<&Rich>, candidate: Option<Option<&Rich>>) -> Vec<CensusRow> {
    let r = counts(reference);
    let c = candidate.map(counts);
    let keys: BTreeSet<(u32, u32)> = r
        .keys()
        .chain(c.iter().flat_map(BTreeMap::keys))
        .copied()
        .collect();
    keys.into_iter()
        .map(|(p, b)| CensusRow {
            prodid: p,
            product: prodid_name(p),
            build: b,
            reference: r.get(&(p, b)).copied().unwrap_or(0),
            candidate: c.as_ref().map(|m| m.get(&(p, b)).copied().unwrap_or(0)),
        })
        .collect()
}

#[must_use]
pub fn archive_builds(a: &Archive<'_>) -> BTreeMap<u32, u32> {
    let mut out: BTreeMap<u32, u32> = BTreeMap::new();
    for (_, o) in a.objects() {
        for c in o.comp_ids() {
            let e = out.entry(c & 0xffff).or_insert(0);
            *e = e.saturating_add(1);
        }
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ArchiveWitness {
    pub archive: String,
    pub build: u32,
    pub members: u32,
    pub unique: bool,
    pub in_image: bool,
}

#[must_use]
pub fn witnesses(rich: &Rich, archives: &[(String, BTreeMap<u32, u32>)]) -> Vec<ArchiveWitness> {
    let mut seen: BTreeMap<u32, usize> = BTreeMap::new();
    for (_, b) in archives {
        for k in b.keys() {
            *seen.entry(*k).or_insert(0) += 1;
        }
    }
    let present = rich.builds();
    archives
        .iter()
        .flat_map(|(name, builds)| {
            builds.iter().map(|(b, n)| ArchiveWitness {
                archive: name.clone(),
                build: *b,
                members: *n,
                unique: archives.len() > 1 && seen.get(b).copied() == Some(1),
                in_image: present.contains(b),
            })
        })
        .collect()
}

#[must_use]
pub fn deciding_archives(w: &[ArchiveWitness]) -> Vec<String> {
    let mut out: Vec<String> = w
        .iter()
        .filter(|x| x.unique && x.in_image)
        .map(|x| x.archive.clone())
        .collect();
    out.dedup();
    out
}
