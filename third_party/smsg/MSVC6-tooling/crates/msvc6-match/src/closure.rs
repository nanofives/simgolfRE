use iced_x86::{Decoder, DecoderOptions, FlowControl, OpKind, Register};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const HISTOGRAM_CAP: usize = 10;

#[derive(Debug, Clone, Copy)]
pub struct Text<'a> {
    pub bytes: &'a [u8],
    pub rva: u32,
    pub image_base: u32,
}

impl Text<'_> {
    fn at(&self, rva: u32, len: usize) -> Option<&[u8]> {
        let off = usize::try_from(rva.checked_sub(self.rva)?).ok()?;
        let rest = self.bytes.get(off..)?;
        rest.get(..len.min(rest.len()))
    }

    fn follow(&self, rva: u32) -> u32 {
        let Some(b) = self.at(rva, 5) else {
            return rva;
        };
        match (
            b.first(),
            b.get(1..5).and_then(|s| <[u8; 4]>::try_from(s).ok()),
        ) {
            (Some(0xe9), Some(d)) => rva.wrapping_add(5).wrapping_add(u32::from_le_bytes(d)),
            _ => rva,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub rva: u32,
    pub len: u32,
    pub blockers: BTreeSet<u32>,
    pub foreign: usize,
    pub indirect: usize,
}

fn owner(extents: &[(u32, u32)], target: u32) -> Option<u32> {
    let k = extents.partition_point(|(r, _)| *r <= target);
    let (r, l) = extents.get(k.checked_sub(1)?)?;
    (target < r.saturating_add(*l)).then_some(*r)
}

#[must_use]
pub fn closure(t: &Text<'_>, extents: &[(u32, u32)], defined: &BTreeSet<u32>) -> Vec<Row> {
    let mut sorted = extents.to_vec();
    sorted.sort_unstable();
    sorted.dedup_by_key(|e| e.0);
    let mut out = Vec::new();
    for (rva, len) in &sorted {
        if defined.contains(rva) {
            continue;
        }
        let Some(body) = t.at(*rva, usize::try_from(*len).unwrap_or(0)) else {
            continue;
        };
        let ip = u64::from(t.image_base).saturating_add(u64::from(*rva));
        let mut d = Decoder::with_ip(32, body, ip, DecoderOptions::NONE);
        let mut row = Row {
            rva: *rva,
            len: *len,
            blockers: BTreeSet::new(),
            foreign: 0,
            indirect: 0,
        };
        while d.can_decode() {
            let i = d.decode();
            if i.is_invalid() {
                break;
            }
            match i.flow_control() {
                FlowControl::Call if i.op0_kind() == OpKind::NearBranch32 => {
                    let target = u32::try_from(i.near_branch_target())
                        .ok()
                        .and_then(|va| va.checked_sub(t.image_base))
                        .map(|r| t.follow(r));
                    match target.and_then(|r| owner(&sorted, r)) {
                        Some(o) if !defined.contains(&o) && o != *rva => {
                            row.blockers.insert(o);
                        }
                        Some(_) => {}
                        None => row.foreign = row.foreign.saturating_add(1),
                    }
                }
                FlowControl::IndirectCall
                    if i.op0_kind() == OpKind::Memory && i.memory_base() != Register::None =>
                {
                    row.indirect = row.indirect.saturating_add(1);
                }
                _ => {}
            }
        }
        out.push(row);
    }
    out.sort_by_key(|c| (c.blockers.len(), std::cmp::Reverse(c.len), c.rva));
    out
}

#[must_use]
pub fn histogram(rows: &[Row]) -> BTreeMap<usize, usize> {
    let mut h = BTreeMap::new();
    for r in rows {
        let e = h
            .entry(r.blockers.len().min(HISTOGRAM_CAP))
            .or_insert(0usize);
        *e = e.saturating_add(1);
    }
    h
}
