use crate::decode::{Dec, decode_keyed};
use crate::insndiff::align;
use std::collections::BTreeSet;
use std::ops::Range;

pub const REF_SLACK_DIV: usize = 8;
pub const REF_SLACK_MIN: usize = 64;
pub const MAX_BODY: usize = 1 << 20;

#[must_use]
pub fn slack(body: usize) -> usize {
    body.saturating_add((body / REF_SLACK_DIV).max(REF_SLACK_MIN))
}

fn idx(v: u32) -> Option<usize> {
    usize::try_from(v).ok()
}

fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    d.get(at..at.checked_add(4)?)
        .and_then(|b| b.try_into().ok())
        .map(u32::from_le_bytes)
}

#[derive(Debug, Clone)]
pub struct Aligned {
    pub ours: Vec<Dec>,
    pub theirs: Vec<Dec>,
    bytes: Vec<u8>,
    pub ref_of: Vec<Option<usize>>,
    pub ours_of: Vec<Option<usize>>,
}

impl Aligned {
    #[must_use]
    pub fn new(
        body: &[u8],
        relocated: &BTreeSet<usize>,
        theirs: Vec<u8>,
        image: &Range<u64>,
    ) -> Self {
        if body.len() > MAX_BODY || theirs.len() > slack(MAX_BODY) {
            return Self {
                ours: Vec::new(),
                theirs: Vec::new(),
                bytes: theirs,
                ref_of: Vec::new(),
                ours_of: Vec::new(),
            };
        }
        let ours = decode_keyed(
            body,
            &|ip, len, _| {
                (ip..ip.saturating_add(len)).any(|i| idx(i).is_some_and(|i| relocated.contains(&i)))
            },
            false,
        );
        let tdec = decode_keyed(&theirs, &|_, _, v| image.contains(&v), false);
        let ok: Vec<&str> = ours.iter().map(|d| d.key.as_str()).collect();
        let tk: Vec<&str> = tdec.iter().map(|d| d.key.as_str()).collect();
        let mut ref_of = vec![None; ours.len()];
        let mut ours_of = vec![None; tdec.len()];
        let pairs = align(&ok, &tk);
        for (a, b) in pairs {
            let (Some(a), Some(b)) = (a, b) else { continue };
            if ok.get(a) == tk.get(b) {
                if let Some(x) = ref_of.get_mut(a) {
                    *x = Some(b);
                }
                if let Some(x) = ours_of.get_mut(b) {
                    *x = Some(a);
                }
            }
        }
        Self {
            ours,
            theirs: tdec,
            bytes: theirs,
            ref_of,
            ours_of,
        }
    }

    #[must_use]
    pub fn holding(&self, at: u32) -> Option<(usize, u32)> {
        let i = self.ours.partition_point(|d| d.ip <= at).checked_sub(1)?;
        let d = self.ours.get(i)?;
        let len = u32::try_from(d.insn.len()).ok()?;
        (at.checked_add(4)? <= d.ip.checked_add(len)?).then_some((i, at - d.ip))
    }

    #[must_use]
    pub fn field(&self, k: usize, foff: u32) -> Option<u32> {
        let d = self.theirs.get(k)?;
        u32_at(&self.bytes, idx(d.ip.checked_add(foff)?)?)
    }

    #[must_use]
    pub fn gap_free(&self, i: usize, j: usize, q: usize) -> bool {
        let delta = i.wrapping_sub(j);
        (j.min(q)..=j.max(q)).all(|k| {
            self.ours_of
                .get(k)
                .copied()
                .flatten()
                .is_some_and(|a| a.wrapping_sub(k) == delta)
        })
    }

    #[must_use]
    pub fn gap_free_ours(&self, i: usize, j: usize, p: usize) -> bool {
        let delta = j.wrapping_sub(i);
        (i.min(p)..=i.max(p)).all(|k| {
            self.ref_of
                .get(k)
                .copied()
                .flatten()
                .is_some_and(|b| b.wrapping_sub(k) == delta)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_body_shifted_by_a_byte_pairs_by_instruction() {
        let ours = [0xb8, 0, 0, 0, 0, 0xc3];
        let theirs = vec![0x90, 0xb8, 0x00, 0x20, 0x40, 0x00, 0xc3];
        let reloc: BTreeSet<usize> = (1..5).collect();
        let a = Aligned::new(&ours, &reloc, theirs, &(0x40_0000..0x41_0000));
        assert_eq!(a.holding(1), Some((0, 1)));
        assert_eq!(a.ref_of.first().copied().flatten(), Some(1));
        assert_eq!(a.field(1, 1), Some(0x40_2000));
        assert_eq!(a.holding(5), None);
        assert!(a.gap_free(0, 1, 1));
        assert_eq!(slack(8), 72);
    }

    #[test]
    fn a_body_past_the_cap_pairs_nothing() {
        let ours = vec![0x90; MAX_BODY + 1];
        let a = Aligned::new(&ours, &BTreeSet::new(), ours.clone(), &(0..1));
        assert!(a.ref_of.iter().all(Option::is_none));
        assert!(a.holding(0).is_none());
    }
}
