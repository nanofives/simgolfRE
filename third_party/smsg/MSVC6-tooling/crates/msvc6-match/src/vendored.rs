use crate::code::{self, Code};
use crate::place::{Placement, Text, order_assignments, place};
use anyhow::{Context as _, Result};
use msvc6_image::coff::Object;
use serde::Serialize;

pub const MAX_PAD: usize = 15;

pub const NOP: u8 = 0x90;

#[must_use]
pub fn trailing_padding(c: &Code) -> usize {
    let n = c
        .bytes
        .iter()
        .zip(&c.relocated)
        .rev()
        .take_while(|(b, r)| **b == NOP && !**r)
        .count()
        .min(MAX_PAD);
    if n >= c.bytes.len() { 0 } else { n }
}

#[must_use]
pub fn place_code(c: &Code, text: &[u8]) -> (Placement, usize) {
    let strict = place(c, text);
    let pad = trailing_padding(c);
    if strict != Placement::Absent || pad == 0 {
        return (strict, 0);
    }
    let mut trimmed = c.clone();
    let keep = trimmed.bytes.len().saturating_sub(pad);
    for m in trimmed.relocated.iter_mut().skip(keep) {
        *m = true;
    }
    match place(&trimmed, text) {
        Placement::Unique(o) => (Placement::Unique(o), pad),
        _ => (strict, 0),
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Section {
    pub index: u16,
    pub symbol: Option<String>,
    pub len: u32,
    pub verdict: String,
    pub rva: Option<u32>,
    pub pad_masked: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Located {
    pub object: String,
    pub sections: Vec<Section>,
    pub unique: usize,
    pub by_order: usize,
    pub ambiguous: usize,
    pub absent: usize,
    pub unidentifiable: usize,
    pub lo: Option<u32>,
    pub hi: Option<u32>,
    pub bytes: u32,
}

impl Located {
    fn note(&mut self, rva: u32, len: u32) {
        self.lo = Some(self.lo.map_or(rva, |l| l.min(rva)));
        let end = rva.saturating_add(len);
        self.hi = Some(self.hi.map_or(end, |h| h.max(end)));
        self.bytes = self.bytes.saturating_add(len);
    }
}

pub fn locate_object(name: &str, bytes: &[u8], t: &Text<'_>) -> Result<Located> {
    let o = Object::parse(bytes).with_context(|| name.to_string())?;
    let codes = code::sections(&o).with_context(|| name.to_string())?;
    let mut out = Located {
        object: name.to_string(),
        ..Located::default()
    };
    let mut placed: Vec<(u32, u32)> = Vec::new();
    let mut ambiguous: Vec<(usize, Code)> = Vec::new();
    for c in &codes {
        let (p, pad) = place_code(c, t.bytes);
        let (verdict, rva) = match p {
            Placement::Unique(off) => {
                let rva = t.rva_of(off);
                out.unique = out.unique.saturating_add(1);
                placed.push((rva, c.len32()));
                out.note(rva, c.len32());
                ("unique", Some(rva))
            }
            Placement::Ambiguous(_) => {
                ambiguous.push((out.sections.len(), c.clone()));
                ("ambiguous", None)
            }
            Placement::Absent => {
                out.absent = out.absent.saturating_add(1);
                ("absent", None)
            }
            Placement::Unidentifiable | Placement::Exhausted => {
                out.unidentifiable = out.unidentifiable.saturating_add(1);
                ("unidentifiable", None)
            }
        };
        out.sections.push(Section {
            index: c.index,
            symbol: c.symbol.clone(),
            len: c.len32(),
            verdict: verdict.to_string(),
            rva,
            pad_masked: pad,
        });
    }
    let unplaced: Vec<Code> = ambiguous.iter().map(|(_, c)| c.clone()).collect();
    let assigned = if placed.is_empty() {
        None
    } else {
        order_assignments(&placed, &unplaced, 0, 0, t)
    };
    for (k, rva, len) in assigned.unwrap_or_default() {
        let Some((slot, _)) = ambiguous.get(k) else {
            continue;
        };
        if let Some(s) = out.sections.get_mut(*slot) {
            s.verdict = "by-order".to_string();
            s.rva = Some(rva);
        }
        out.by_order = out.by_order.saturating_add(1);
        out.note(rva, len);
    }
    out.ambiguous = out
        .sections
        .iter()
        .filter(|s| s.verdict == "ambiguous")
        .count();
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Block {
    pub lo: u32,
    pub hi: u32,
    pub objects: Vec<String>,
    pub in_order: bool,
}

#[must_use]
pub fn blocks(located: &[Located], slack: u32) -> Vec<Block> {
    let mut spans: Vec<(u32, u32, usize)> = located
        .iter()
        .enumerate()
        .filter_map(|(i, l)| Some((l.lo?, l.hi?, i)))
        .collect();
    spans.sort_unstable();
    let mut out: Vec<Block> = Vec::new();
    let mut order: Vec<Vec<usize>> = Vec::new();
    for (lo, hi, i) in spans {
        let name = located
            .get(i)
            .map_or_else(String::new, |l| l.object.clone());
        match out.last_mut() {
            Some(b) if lo <= b.hi.saturating_add(slack) => {
                b.hi = b.hi.max(hi);
                b.objects.push(name);
                if let Some(o) = order.last_mut() {
                    o.push(i);
                }
            }
            _ => {
                out.push(Block {
                    lo,
                    hi,
                    objects: vec![name],
                    in_order: true,
                });
                order.push(vec![i]);
            }
        }
    }
    for (b, o) in out.iter_mut().zip(order) {
        b.in_order = o.windows(2).all(|w| matches!(w, [x, y] if x < y));
    }
    out
}
