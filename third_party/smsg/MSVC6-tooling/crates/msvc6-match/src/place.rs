use crate::code::{Code, REL_DIR32, REL_DIR32NB, REL_REL32, Reloc, u32_of};
use std::collections::BTreeMap;

pub const MIN_ANCHOR: usize = 4;
pub const MIN_WITNESS_LITERAL: usize = 4;
pub const ALIGN_SLACK: u32 = 16;
pub const CONTRIBUTION_ALIGN: u32 = 16;
pub const SEARCH_BUDGET: u64 = 1 << 24;

#[derive(Debug, Clone, Copy)]
pub struct Text<'a> {
    pub bytes: &'a [u8],
    pub rva: u32,
    pub image_base: u32,
}

impl Text<'_> {
    #[must_use]
    pub fn rva_of(&self, off: usize) -> u32 {
        self.rva.saturating_add(u32_of(off))
    }

    #[must_use]
    pub fn window(&self, rva: u32, len: usize) -> Option<&[u8]> {
        let off = usize::try_from(rva.checked_sub(self.rva)?).ok()?;
        self.bytes.get(off..off.checked_add(len)?)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Placement {
    Unique(usize),
    Ambiguous(usize),
    Absent,
    Unidentifiable,
    Exhausted,
}

#[must_use]
pub fn place(c: &Code, text: &[u8]) -> Placement {
    let Some((at, anchor)) = c.anchor() else {
        return Placement::Unidentifiable;
    };
    if anchor.len() < MIN_ANCHOR {
        return Placement::Unidentifiable;
    }
    let mut budget = SEARCH_BUDGET;
    let mut first = None;
    let mut hits = 0usize;
    for p in memchr::memmem::find_iter(text, anchor) {
        let Some(start) = p.checked_sub(at) else {
            continue;
        };
        let Some(window) = start
            .checked_add(c.bytes.len())
            .and_then(|e| text.get(start..e))
        else {
            continue;
        };
        match c.masked(window, &mut budget) {
            Some(true) => {
                hits = hits.saturating_add(1);
                first.get_or_insert(start);
            }
            Some(false) => {}
            None => return Placement::Exhausted,
        }
    }
    match (hits, first) {
        (1, Some(o)) => Placement::Unique(o),
        (0, _) | (_, None) => Placement::Absent,
        (n, Some(_)) => Placement::Ambiguous(n),
    }
}

#[must_use]
pub fn target_at(t: &Text<'_>, placed_at: u32, c: &Code, r: &Reloc) -> Option<u32> {
    let read4 = |b: &[u8]| -> Option<u32> { Some(u32::from_le_bytes(b.try_into().ok()?)) };
    let at = usize::try_from(r.at).ok()?;
    let addend = read4(c.bytes.get(at..at.checked_add(4)?)?)?;
    let field_rva = placed_at.checked_add(r.at)?;
    let raw = read4(t.window(field_rva, 4)?)?;
    match r.kind {
        REL_REL32 => Some(
            raw.wrapping_add(field_rva)
                .wrapping_add(4)
                .wrapping_sub(addend),
        ),
        REL_DIR32 => raw.wrapping_sub(addend).checked_sub(t.image_base),
        REL_DIR32NB => Some(raw.wrapping_sub(addend)),
        _ => None,
    }
}

#[derive(Debug, Clone, Default)]
pub struct Taken {
    starts: Vec<(u32, u32)>,
    max_end: Vec<u32>,
}

impl Taken {
    #[must_use]
    pub fn of(ranges: impl IntoIterator<Item = (u32, u32)>) -> Self {
        let mut starts: Vec<(u32, u32)> = ranges.into_iter().collect();
        starts.sort_unstable();
        let mut max_end = Vec::with_capacity(starts.len());
        let mut m = 0u32;
        for (_, e) in &starts {
            m = m.max(*e);
            max_end.push(m);
        }
        Self { starts, max_end }
    }

    #[must_use]
    pub fn overlaps(&self, lo: u32, hi: u32) -> bool {
        let n = self.starts.partition_point(|(s, _)| *s < hi);
        n.checked_sub(1)
            .and_then(|k| self.max_end.get(k))
            .is_some_and(|e| *e > lo)
    }
}

#[must_use]
pub fn witnessed_starts(
    c: &Code,
    t: &Text<'_>,
    known: &BTreeMap<String, u32>,
    taken: &Taken,
) -> Vec<u32> {
    let len = c.bytes.len();
    if c.literal_len() < MIN_WITNESS_LITERAL || len == 0 || len > t.bytes.len() {
        return Vec::new();
    }
    let Some((at, anchor)) = c.anchor() else {
        return Vec::new();
    };
    let checked: Vec<(&Reloc, u32)> = c
        .relocations
        .iter()
        .filter(|r| r.global && !r.symbol.is_empty())
        .filter_map(|r| known.get(&r.symbol).map(|a| (r, *a)))
        .collect();
    if checked.is_empty() {
        return Vec::new();
    }
    let len32 = u32_of(len);
    let mut budget = SEARCH_BUDGET;
    let mut out = Vec::new();
    for p in memchr::memmem::find_iter(t.bytes, anchor) {
        let Some(start) = p.checked_sub(at) else {
            continue;
        };
        let Some(window) = start.checked_add(len).and_then(|e| t.bytes.get(start..e)) else {
            continue;
        };
        match c.masked(window, &mut budget) {
            Some(true) => {}
            Some(false) => continue,
            None => return Vec::new(),
        }
        let rva = t.rva_of(start);
        if taken.overlaps(rva, rva.saturating_add(len32)) {
            continue;
        }
        if checked
            .iter()
            .all(|(r, a)| target_at(t, rva, c, r) == Some(*a))
        {
            out.push(rva);
        }
    }
    out
}

#[must_use]
pub fn neighbour_bounds(spans: &[(u32, u32)], k: usize) -> (u32, u32) {
    let floor = k
        .checked_sub(1)
        .and_then(|j| spans.get(j))
        .map_or(0, |(_, hi)| *hi);
    let ceil = k
        .checked_add(1)
        .and_then(|j| spans.get(j))
        .map_or(0, |(lo, _)| *lo);
    (floor, ceil)
}

#[must_use]
pub fn order_assignments(
    placed: &[(u32, u32)],
    unplaced: &[Code],
    obj_start: u32,
    obj_end: u32,
    t: &Text<'_>,
) -> Option<Vec<(usize, u32, u32)>> {
    let lo = placed.iter().map(|(a, _)| *a).min()?;
    let hi = placed.iter().map(|(a, n)| a.saturating_add(*n)).max()?;
    if unplaced.is_empty() {
        return Some(Vec::new());
    }
    let mut placed = placed.to_vec();
    placed.sort_unstable();
    let mut holes: Vec<(u32, u32)> = Vec::new();
    let mut cursor = if obj_start > 0 && obj_start < lo {
        obj_start
    } else {
        lo
    };
    for (a, n) in &placed {
        if *a > cursor {
            holes.push((cursor, a.saturating_sub(cursor)));
        }
        cursor = cursor.max(a.saturating_add(*n));
    }
    let end = obj_end.max(hi);
    if cursor < end {
        holes.push((cursor, end.saturating_sub(cursor)));
    }
    if holes.is_empty() {
        return Some(Vec::new());
    }
    let mut budget = SEARCH_BUDGET;
    let mut verify = |c: &Code, at: u32| -> Option<bool> {
        match t.window(at, c.bytes.len()) {
            Some(w) => c.masked(w, &mut budget),
            None => Some(false),
        }
    };
    let mut assigned: Vec<(usize, u32, u32)> = Vec::new();
    let mut next = 0usize;
    for (hs, hl) in &holes {
        let mut at = *hs;
        let stop = hs.saturating_add(*hl);
        while at < stop {
            let room = stop.saturating_sub(at);
            let mut chosen = None;
            for (i, c) in unplaced.iter().enumerate().skip(next) {
                let Ok(len) = u32::try_from(c.bytes.len()) else {
                    continue;
                };
                if len == 0 || len > room {
                    continue;
                }
                if verify(c, at)? {
                    chosen = Some((i, len));
                    break;
                }
            }
            let Some((i, len)) = chosen else {
                if next >= unplaced.len() {
                    break;
                }
                let aligned = at.checked_add(1).and_then(|n| {
                    n.div_ceil(CONTRIBUTION_ALIGN)
                        .checked_mul(CONTRIBUTION_ALIGN)
                });
                match aligned {
                    Some(a) if a > at && a < stop => {
                        at = a;
                        continue;
                    }
                    Some(a) if a >= stop => break,
                    _ => return None,
                }
            };
            assigned.push((i, at, len));
            next = i.saturating_add(1);
            at = at.saturating_add(len);
        }
    }
    Some(assigned)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fptrap() -> Code {
        Code {
            index: 1,
            symbol: Some("__fptrap".into()),
            external: true,
            bytes: vec![0x6a, 0x02, 0xe8, 0, 0, 0, 0, 0x59, 0xc3],
            relocated: vec![false, false, false, true, true, true, true, false, false],
            comdat: true,
            align: 16,
            relocations: vec![Reloc {
                at: 3,
                symbol: "__amsg_exit".into(),
                kind: REL_REL32,
                external: true,
                global: true,
            }],
        }
    }

    #[test]
    fn a_body_with_no_four_byte_run_places_by_its_relocation_witness() {
        let c = fptrap();
        assert_eq!(place(&c, &[0u8; 64]), Placement::Unidentifiable);
        let amsg = 0x1100u32;
        let mut text = vec![0xccu8; 0x200];
        let mut put = |start: usize, to: u32| {
            text[start..start + 9].copy_from_slice(&c.bytes);
            let field = 0x1000 + u32::try_from(start).unwrap() + 3;
            text[start + 3..start + 7].copy_from_slice(&to.wrapping_sub(field + 4).to_le_bytes());
        };
        put(0x10, amsg);
        put(0x40, 0x1180);
        let t = Text {
            bytes: &text,
            rva: 0x1000,
            image_base: 0x40_0000,
        };
        let known: BTreeMap<String, u32> =
            [("__amsg_exit".to_string(), amsg)].into_iter().collect();
        assert_eq!(
            witnessed_starts(&c, &t, &known, &Taken::default()),
            vec![0x1010]
        );
        assert!(witnessed_starts(&c, &t, &known, &Taken::of([(0x1000, 0x1020)])).is_empty());
        assert!(witnessed_starts(&c, &t, &BTreeMap::new(), &Taken::default()).is_empty());
        assert_eq!(target_at(&t, 0x1010, &c, &c.relocations[0]), Some(amsg));
    }

    #[test]
    fn neighbours_bound_both_sides_and_an_end_is_unbounded() {
        let spans = [(0x1000, 0x1100), (0x1200, 0x1300), (0x1400, 0x1500)];
        assert_eq!(neighbour_bounds(&spans, 1), (0x1100, 0x1400));
        assert_eq!(neighbour_bounds(&spans, 0), (0, 0x1200));
        assert_eq!(neighbour_bounds(&spans, 2), (0x1300, 0));
        assert_eq!(neighbour_bounds(&[(0x1000, 0x1100)], 0), (0, 0));
        assert_eq!(neighbour_bounds(&[(0x1000, 0x1100)], 9), (0, 0));
        let gap = [(0x7e7c0, 0x7e818), (0x7e820, 0x7e8a1), (0x7e8d0, 0x7e98a)];
        let (floor, ceil) = neighbour_bounds(&gap, 1);
        assert!(floor <= 0x7e8b0 && 0x7e8b0 + 0x1c <= ceil);
    }

    #[test]
    fn a_hole_is_filled_in_section_order_and_its_tail_is_padding() {
        let small = Code {
            bytes: vec![0x33, 0xc0, 0x40, 0xc3],
            relocated: vec![false; 4],
            ..Code::default()
        };
        let mut text = vec![0xccu8; 0x40];
        text[0x10..0x14].copy_from_slice(&small.bytes);
        let t = Text {
            bytes: &text,
            rva: 0x1000,
            image_base: 0,
        };
        let got = order_assignments(
            &[(0x1000, 0x10)],
            std::slice::from_ref(&small),
            0,
            0x1020,
            &t,
        );
        assert_eq!(got, Some(vec![(0, 0x1010, 4)]));
        let none = order_assignments(&[(0x1000, 0x10)], std::slice::from_ref(&small), 0, 0, &t);
        assert_eq!(none, Some(Vec::new()));
        let mut wrong = small.clone();
        wrong.bytes = vec![0x90; 8];
        wrong.relocated = vec![false; 8];
        assert_eq!(
            order_assignments(&[(0x1000, 0x10)], &[wrong], 0, 0x1040, &t),
            Some(Vec::new())
        );
    }

    #[test]
    fn taken_ranges_answer_overlap_as_a_scan_would() {
        let r = [(0x10, 0x20), (0x18, 0x30), (0x40, 0x48)];
        let t = Taken::of(r);
        for lo in 0..0x50u32 {
            for len in 1..0x10u32 {
                let hi = lo + len;
                let scan = r.iter().any(|(a, b)| lo < *b && *a < hi);
                assert_eq!(t.overlaps(lo, hi), scan, "{lo:#x}..{hi:#x}");
            }
        }
    }
}
