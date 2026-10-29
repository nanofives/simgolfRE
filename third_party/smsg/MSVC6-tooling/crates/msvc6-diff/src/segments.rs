use crate::comdat::{self, Comdat, REL_DIR32, REL_REL32, Reloc};
use crate::decode::{Dec, decode_keyed, decode_one};
use crate::env::{Env, RECORDED};
use crate::insndiff::align;
use crate::resolve::follow_thunk;
use anyhow::{Result, anyhow};
use iced_x86::{FlowControl, Mnemonic, OpKind, Register};
use msvc6_base::units::Rva;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const MIN_SEGMENT: usize = 50;
pub const MAX_SEGMENT: usize = 512;
const MIN_LITERAL: usize = 2;
const SOLID_RUN: usize = 3;
const RELAX_CELLS: usize = 1 << 22;
const LITERAL_PROBE: usize = 256;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Anchor {
    pub ix: usize,
    pub key: String,
    pub label: String,
}

#[derive(Debug, Clone, Default)]
pub struct Side {
    pub bytes: Vec<u8>,
    pub masked: Vec<bool>,
    pub dec: Vec<Dec>,
    pub reg_keys: Vec<String>,
    pub anchors: Vec<Anchor>,
    pub addrs: Vec<Option<Vec<u32>>>,
    pub wide: Vec<AlignRow>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, serde::Serialize)]
pub enum Verdict {
    Exact,
    RegOnly,
    Shape,
}

impl Verdict {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::Exact => "EXACT",
            Self::RegOnly => "REG-ONLY",
            Self::Shape => "SHAPE",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Seg {
    pub img: (usize, usize),
    pub ours: (usize, usize),
    pub verdict: Verdict,
    pub diff: usize,
    pub edit: usize,
    pub ia: (usize, usize),
    pub oa: (usize, usize),
    pub at: String,
    pub oversize: bool,
}

impl Seg {
    #[must_use]
    pub fn img_len(&self) -> usize {
        self.img.1.saturating_sub(self.img.0)
    }

    #[must_use]
    pub fn ours_len(&self) -> usize {
        self.ours.1.saturating_sub(self.ours.0)
    }
}

#[must_use]
pub fn off(s: &Side, i: usize) -> usize {
    s.dec.get(i).map_or(s.bytes.len(), |d| d.ip as usize)
}

fn branch_disp(d: &Dec) -> Option<std::ops::Range<usize>> {
    if !matches!(
        d.insn.op0_kind(),
        OpKind::NearBranch32 | OpKind::NearBranch16
    ) {
        return None;
    }
    let end = d.end();
    let k = if d.insn.len() == 2 { 1 } else { 4 };
    Some(end.saturating_sub(k)..end)
}

fn outbound_branch(d: &Dec, lo: usize, hi: usize) -> Option<std::ops::Range<usize>> {
    let t = usize::try_from(d.insn.near_branch_target()).ok()?;
    if t >= lo && t < hi {
        return None;
    }
    branch_disp(d)
}

fn mask_over(s: &Side, a: usize, b: usize, every_branch: bool) -> Vec<bool> {
    let (lo, hi) = (off(s, a), off(s, b));
    let mut live: Vec<bool> = (lo..hi)
        .map(|i| !s.masked.get(i).copied().unwrap_or(false))
        .collect();
    for d in s.dec.get(a..b).unwrap_or(&[]) {
        let r = if every_branch {
            branch_disp(d)
        } else {
            outbound_branch(d, lo, hi)
        };
        for i in r.into_iter().flatten() {
            if let Some(x) = i.checked_sub(lo).and_then(|k| live.get_mut(k)) {
                *x = false;
            }
        }
    }
    live
}

fn live_mask(s: &Side, a: usize, b: usize) -> Vec<bool> {
    mask_over(s, a, b, false)
}

fn edit_mask(s: &Side, a: usize, b: usize) -> Vec<bool> {
    mask_over(s, a, b, true)
}

fn score(
    img: &Side,
    ours: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
    at: String,
    max: usize,
    edit: usize,
) -> Seg {
    let (il, ih) = (off(img, ia.0), off(img, ia.1));
    let (ol, oh) = (off(ours, oa.0), off(ours, oa.1));
    let ib = img.bytes.get(il..ih).unwrap_or(&[]);
    let ob = ours.bytes.get(ol..oh).unwrap_or(&[]);
    let im = live_mask(img, ia.0, ia.1);
    let om = live_mask(ours, oa.0, oa.1);
    let diff = ib
        .iter()
        .zip(ob)
        .zip(im.iter().zip(&om))
        .filter(|((x, y), (a, b))| **a && **b && x != y)
        .count()
        + ib.len().abs_diff(ob.len());
    let verdict = if diff == 0 && ib.len() == ob.len() {
        Verdict::Exact
    } else if ib.len() == ob.len() && img.reg_keys.get(ia.0..ia.1) == ours.reg_keys.get(oa.0..oa.1)
    {
        Verdict::RegOnly
    } else {
        Verdict::Shape
    };
    Seg {
        img: (il, ih),
        ours: (ol, oh),
        verdict,
        diff,
        edit,
        ia,
        oa,
        at,
        oversize: ih.saturating_sub(il) > max,
    }
}

fn masked_key(s: &Side, i: usize, lo: usize, live: &[bool]) -> Vec<u16> {
    let (a, b) = (off(s, i), off(s, i + 1));
    (a..b)
        .map(|k| {
            let byte = s.bytes.get(k).copied().unwrap_or(0);
            if k.checked_sub(lo)
                .and_then(|x| live.get(x))
                .copied()
                .unwrap_or(false)
            {
                u16::from(byte)
            } else {
                0x100
            }
        })
        .collect()
}

fn shape_key(s: &Side, i: usize) -> String {
    let Some(d) = s.dec.get(i) else {
        return String::new();
    };
    let mut k = format!("{:?}", d.insn.code());
    for op in 0..d.insn.op_count() {
        k.push_str(&format!(" {:?}", d.insn.op_kind(op)));
    }
    k
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub enum Pairing {
    Same,
    Form,
    Replaced,
    ImageOnly,
    OursOnly,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AlignRow {
    pub img: Option<usize>,
    pub ours: Option<usize>,
    pub pairing: Pairing,
    pub cost: usize,
}

#[must_use]
pub fn align_segment(
    img: &Side,
    ours: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
) -> Vec<AlignRow> {
    if !ours.wide.is_empty() {
        return ours
            .wide
            .iter()
            .copied()
            .filter(|r| match (r.img, r.ours) {
                (Some(i), _) => i >= ia.0 && i < ia.1,
                (None, Some(j)) => j >= oa.0 && j < oa.1,
                (None, None) => false,
            })
            .collect();
    }
    align_range(img, ours, ia, oa)
}

fn insn_len(s: &Side, i: usize) -> usize {
    off(s, i + 1).saturating_sub(off(s, i))
}

fn form_cost(a: &[u16], b: &[u16]) -> usize {
    a.iter()
        .zip(b)
        .filter(|(p, q)| **p != 0x100 && **q != 0x100 && p != q)
        .count()
}

#[must_use]
pub fn align_range(
    img: &Side,
    ours: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
) -> Vec<AlignRow> {
    let (il, ol) = (off(img, ia.0), off(ours, oa.0));
    let (im, om) = (edit_mask(img, ia.0, ia.1), edit_mask(ours, oa.0, oa.1));
    let ik: Vec<Vec<u16>> = (ia.0..ia.1).map(|i| masked_key(img, i, il, &im)).collect();
    let ok: Vec<Vec<u16>> = (oa.0..oa.1).map(|j| masked_key(ours, j, ol, &om)).collect();
    let mut out = Vec::new();
    let settle = |ri: &[usize], ro: &[usize], out: &mut Vec<AlignRow>| {
        let a: Vec<String> = ri.iter().map(|x| shape_key(img, ia.0 + x)).collect();
        let b: Vec<String> = ro.iter().map(|y| shape_key(ours, oa.0 + y)).collect();
        for (p, q) in align(&a, &b) {
            let (x, y) = (
                p.and_then(|k| ri.get(k)).copied(),
                q.and_then(|k| ro.get(k)).copied(),
            );
            let row = |pairing, cost| AlignRow {
                img: x.map(|x| ia.0 + x),
                ours: y.map(|y| oa.0 + y),
                pairing,
                cost,
            };
            let ic = x.map_or(0, |x| insn_len(img, ia.0 + x));
            let oc = y.map_or(0, |y| insn_len(ours, oa.0 + y));
            out.push(match (x, y) {
                (Some(x), Some(y))
                    if p.and_then(|k| a.get(k)) == q.and_then(|k| b.get(k))
                        && ik.get(x).map(Vec::len) == ok.get(y).map(Vec::len) =>
                {
                    let c = form_cost(
                        ik.get(x).map_or(&[][..], Vec::as_slice),
                        ok.get(y).map_or(&[][..], Vec::as_slice),
                    );
                    row(if c == 0 { Pairing::Same } else { Pairing::Form }, c)
                }
                (Some(_), Some(_)) => row(Pairing::Replaced, ic + oc),
                (Some(_), None) => row(Pairing::ImageOnly, ic),
                _ => row(Pairing::OursOnly, oc),
            });
        }
    };
    let (mut ri, mut ro) = (Vec::new(), Vec::new());
    for (a, b) in align(&ik, &ok) {
        match (a, b) {
            (Some(a), Some(b)) if ik.get(a) == ok.get(b) => {
                settle(&ri, &ro, &mut out);
                ri.clear();
                ro.clear();
                out.push(AlignRow {
                    img: Some(ia.0 + a),
                    ours: Some(oa.0 + b),
                    pairing: Pairing::Same,
                    cost: 0,
                });
            }
            _ => {
                ri.extend(a);
                ro.extend(b);
            }
        }
    }
    settle(&ri, &ro, &mut out);
    relax(img, ours, ia, oa, (&ik, &ok), out)
}

fn relax(
    img: &Side,
    ours: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
    keys: (&[Vec<u16>], &[Vec<u16>]),
    rows: Vec<AlignRow>,
) -> Vec<AlignRow> {
    let mut solid = vec![false; rows.len()];
    let mut k = 0;
    while k < rows.len() {
        let mut e = k;
        while rows.get(e).is_some_and(|r| r.pairing == Pairing::Same) {
            e += 1;
        }
        if e - k >= SOLID_RUN {
            solid.iter_mut().take(e).skip(k).for_each(|x| *x = true);
        }
        k = e.max(k + 1);
    }
    let mut out = Vec::with_capacity(rows.len());
    let mut k = 0;
    while k < rows.len() {
        if solid.get(k).copied().unwrap_or(true) {
            out.extend(rows.get(k).copied());
            k += 1;
            continue;
        }
        let mut e = k;
        while e < rows.len() && !solid.get(e).copied().unwrap_or(true) {
            e += 1;
        }
        let win = rows.get(k..e).unwrap_or(&[]);
        out.extend(solve(img, ours, ia, oa, keys, win).unwrap_or_else(|| win.to_vec()));
        k = e;
    }
    out
}

fn solve(
    img: &Side,
    ours: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
    (ik, ok): (&[Vec<u16>], &[Vec<u16>]),
    win: &[AlignRow],
) -> Option<Vec<AlignRow>> {
    let old: usize = win.iter().map(|r| r.cost).sum();
    if old == 0 {
        return None;
    }
    let i0 = win.iter().filter_map(|r| r.img).min()?;
    let i1 = win.iter().filter_map(|r| r.img).max()? + 1;
    let j0 = win.iter().filter_map(|r| r.ours).min()?;
    let j1 = win.iter().filter_map(|r| r.ours).max()? + 1;
    let (n, m) = (i1 - i0, j1 - j0);
    if n.saturating_mul(m) > RELAX_CELLS {
        return None;
    }
    let il: Vec<usize> = (i0..i1).map(|i| insn_len(img, i)).collect();
    let ol: Vec<usize> = (j0..j1).map(|j| insn_len(ours, j)).collect();
    let ish: Vec<String> = (i0..i1).map(|i| shape_key(img, i)).collect();
    let osh: Vec<String> = (j0..j1).map(|j| shape_key(ours, j)).collect();
    let key = |v: &[Vec<u16>], base: usize, x: usize| {
        x.checked_sub(base)
            .and_then(|k| v.get(k))
            .cloned()
            .unwrap_or_default()
    };
    let ikw: Vec<Vec<u16>> = (i0..i1).map(|i| key(ik, ia.0, i)).collect();
    let okw: Vec<Vec<u16>> = (j0..j1).map(|j| key(ok, oa.0, j)).collect();
    let pair = |x: usize, y: usize| -> Option<usize> {
        let (a, b) = (ikw.get(x)?, okw.get(y)?);
        if a == b {
            return Some(0);
        }
        (ish.get(x)? == osh.get(y)? && a.len() == b.len()).then(|| form_cost(a, b))
    };
    let w = m + 1;
    let mut dp = vec![0usize; (n + 1) * w];
    let mut how = vec![0u8; (n + 1) * w];
    let at = |dp: &[usize], x: usize, y: usize| dp.get(x * w + y).copied().unwrap_or(usize::MAX);
    let lx = |x: usize| il.get(x).copied().unwrap_or(0);
    let ly = |y: usize| ol.get(y).copied().unwrap_or(0);
    for x in (0..=n).rev() {
        for y in (0..=m).rev() {
            if x == n && y == m {
                continue;
            }
            let mut best = (usize::MAX, 0u8);
            if x < n
                && y < m
                && let Some(c) = pair(x, y)
            {
                best = (c.saturating_add(at(&dp, x + 1, y + 1)), 1);
            }
            if x < n {
                let c = lx(x).saturating_add(at(&dp, x + 1, y));
                if c < best.0 {
                    best = (c, 2);
                }
            }
            if y < m {
                let c = ly(y).saturating_add(at(&dp, x, y + 1));
                if c < best.0 {
                    best = (c, 3);
                }
            }
            if let Some(d) = dp.get_mut(x * w + y) {
                *d = best.0;
            }
            if let Some(h) = how.get_mut(x * w + y) {
                *h = best.1;
            }
        }
    }
    if at(&dp, 0, 0) >= old {
        return None;
    }
    let mut rows = Vec::with_capacity(n.max(m));
    let (mut x, mut y) = (0, 0);
    while x < n || y < m {
        match how.get(x * w + y).copied().unwrap_or(0) {
            1 => {
                let c = pair(x, y).unwrap_or(0);
                rows.push(AlignRow {
                    img: Some(i0 + x),
                    ours: Some(j0 + y),
                    pairing: if c == 0 { Pairing::Same } else { Pairing::Form },
                    cost: c,
                });
                x += 1;
                y += 1;
            }
            2 => {
                rows.push(AlignRow {
                    img: Some(i0 + x),
                    ours: None,
                    pairing: Pairing::ImageOnly,
                    cost: lx(x),
                });
                x += 1;
            }
            3 => {
                rows.push(AlignRow {
                    img: None,
                    ours: Some(j0 + y),
                    pairing: Pairing::OursOnly,
                    cost: ly(y),
                });
                y += 1;
            }
            _ => return None,
        }
    }
    Some(rows)
}

fn leaders(s: &Side) -> BTreeSet<usize> {
    let mut out = BTreeSet::new();
    for (i, d) in s.dec.iter().enumerate() {
        if !matches!(
            d.insn.flow_control(),
            FlowControl::ConditionalBranch
                | FlowControl::UnconditionalBranch
                | FlowControl::IndirectBranch
                | FlowControl::Return
        ) {
            continue;
        }
        out.insert(i + 1);
        if matches!(
            d.insn.op0_kind(),
            OpKind::NearBranch32 | OpKind::NearBranch16
        ) {
            let t = d.insn.near_branch_target();
            let k = s.dec.partition_point(|x| u64::from(x.ip) < t);
            if s.dec.get(k).is_some_and(|x| u64::from(x.ip) == t) {
                out.insert(k);
            }
        }
    }
    out
}

#[must_use]
pub fn is_call(s: &Side, i: usize) -> bool {
    s.dec.get(i).is_some_and(|d| {
        matches!(
            d.insn.flow_control(),
            FlowControl::Call | FlowControl::IndirectCall
        )
    })
}

fn secondary(
    img: &Side,
    ours: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
    keep: &dyn Fn(usize, usize) -> bool,
) -> Vec<(usize, usize)> {
    let ik: Vec<&str> = img
        .reg_keys
        .get(ia.0..ia.1)
        .unwrap_or(&[])
        .iter()
        .map(String::as_str)
        .collect();
    let ok: Vec<&str> = ours
        .reg_keys
        .get(oa.0..oa.1)
        .unwrap_or(&[])
        .iter()
        .map(String::as_str)
        .collect();
    align(&ik, &ok)
        .into_iter()
        .filter_map(|(a, b)| {
            let (a, b) = (a?, b?);
            (ik.get(a) == ok.get(b)).then_some((ia.0 + a, oa.0 + b))
        })
        .filter(|&(i, j)| i > ia.0 && j > oa.0 && keep(i, j))
        .collect()
}

fn greedy(
    img: &Side,
    ia: (usize, usize),
    oa: (usize, usize),
    cands: &[(usize, usize)],
    max: usize,
) -> Vec<(usize, usize)> {
    let mut cur = (ia.0, oa.0);
    let mut out = Vec::new();
    while off(img, ia.1).saturating_sub(off(img, cur.0)) > max {
        let past = |c: &&(usize, usize)| c.0 > cur.0 && c.1 > cur.1;
        let fits = cands
            .iter()
            .filter(past)
            .take_while(|c| off(img, c.0).saturating_sub(off(img, cur.0)) <= max)
            .last();
        let Some(&c) = fits.or_else(|| cands.iter().find(past)) else {
            break;
        };
        out.push(c);
        cur = c;
    }
    out
}

type Cut = (usize, usize, String);

type Candidates<'a> = dyn Fn((usize, usize), (usize, usize)) -> Vec<(usize, usize)> + 'a;

fn refine(
    img: &Side,
    ours: &Side,
    cuts: Vec<Cut>,
    max: usize,
    tag: &str,
    cands: &Candidates<'_>,
) -> Vec<Cut> {
    let end = (img.dec.len(), ours.dec.len());
    let mut out = Vec::with_capacity(cuts.len());
    for (k, c) in cuts.iter().enumerate() {
        let next = cuts.get(k + 1).map_or(end, |n| (n.0, n.1));
        out.push(c.clone());
        if off(img, next.0).saturating_sub(off(img, c.0)) <= max {
            continue;
        }
        let (ia, oa) = ((c.0, next.0), (c.1, next.1));
        for (i, j) in greedy(img, ia, oa, &cands(ia, oa), max) {
            out.push((i, j, tag.to_string()));
        }
    }
    out
}

fn corroborated(wide: &[AlignRow]) -> BTreeSet<(usize, usize)> {
    wide.iter()
        .filter(|r| matches!(r.pairing, Pairing::Same | Pairing::Form))
        .filter_map(|r| Some((r.img?, r.ours?)))
        .collect()
}

fn edits(wide: &[AlignRow], kept: &[Cut], end: (usize, usize)) -> Vec<usize> {
    let mut out = vec![0usize; kept.len()];
    let mut k = 0usize;
    for r in wide {
        while let Some(next) = kept.get(k + 1) {
            let past = r.img.is_some_and(|i| i >= next.0)
                || (r.img.is_none() && r.ours.is_some_and(|j| j >= next.1));
            if past && (next.0, next.1) <= end {
                k += 1;
            } else {
                break;
            }
        }
        if let Some(c) = out.get_mut(k) {
            *c += r.cost;
        }
    }
    out
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Matched {
    pub cut: usize,
    pub refused: usize,
}

#[must_use]
pub fn segment(img: &Side, ours: &Side, max: usize) -> (Vec<Seg>, Matched, Vec<AlignRow>) {
    let rows = if ours.wide.is_empty() {
        align_range(img, ours, (0, img.dec.len()), (0, ours.dec.len()))
    } else {
        ours.wide.clone()
    };
    let wide = corroborated(&rows);
    let ik: Vec<&str> = img.anchors.iter().map(|a| a.key.as_str()).collect();
    let ok: Vec<&str> = ours.anchors.iter().map(|a| a.key.as_str()).collect();
    let mut cuts: Vec<Cut> = vec![(0, 0, String::new())];
    let mut refused = 0;
    for (a, b) in align(&ik, &ok) {
        let (Some(a), Some(b)) = (a, b) else { continue };
        let (Some(x), Some(y)) = (img.anchors.get(a), ours.anchors.get(b)) else {
            continue;
        };
        if x.key != y.key {
            continue;
        }
        let last = cuts.last().map_or((0, 0), |c| (c.0, c.1));
        if x.ix > last.0 && y.ix > last.1 {
            if wide.contains(&(x.ix, y.ix)) {
                cuts.push((x.ix, y.ix, y.label.clone()));
            } else {
                refused += 1;
            }
        } else if x.ix == 0
            && y.ix == 0
            && let Some(c) = cuts.first_mut()
        {
            c.2.clone_from(&y.label);
        }
    }
    let matched = Matched {
        cut: cuts.len().saturating_sub(1),
        refused,
    };
    let end = (img.dec.len(), ours.dec.len());
    let mut kept: Vec<Cut> = Vec::new();
    for c in cuts {
        match kept.last() {
            Some(p) if off(img, c.0).saturating_sub(off(img, p.0)) < MIN_SEGMENT => {}
            _ => kept.push(c),
        }
    }
    if kept.len() > 1
        && kept
            .last()
            .is_some_and(|p| off(img, end.0).saturating_sub(off(img, p.0)) < MIN_SEGMENT)
    {
        kept.pop();
    }
    let (il, ol) = (leaders(img), leaders(ours));
    let kept = refine(img, ours, kept, max, "(bb)", &|ia, oa| {
        secondary(img, ours, ia, oa, &|i, j| {
            il.contains(&i) && ol.contains(&j) && wide.contains(&(i, j))
        })
    });
    let kept = refine(img, ours, kept, max, "(call)", &|ia, oa| {
        secondary(img, ours, ia, oa, &|i, j| {
            is_call(img, i) && is_call(ours, j) && wide.contains(&(i, j))
        })
    });
    let per = edits(&rows, &kept, end);
    let mut out = Vec::new();
    for (i, c) in kept.iter().enumerate() {
        let next = kept.get(i + 1).map_or(end, |n| (n.0, n.1));
        out.push(score(
            img,
            ours,
            (c.0, next.0),
            (c.1, next.1),
            c.2.clone(),
            max,
            per.get(i).copied().unwrap_or(0),
        ));
    }
    (out, matched, rows)
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

#[must_use]
pub fn literal(b: &[u8]) -> Option<String> {
    let end = b.iter().position(|x| *x == 0)?;
    let s = b.get(..end)?;
    (s.len() >= MIN_LITERAL
        && s.iter()
            .all(|c| (0x20..0x7f).contains(c) || matches!(c, b'\t' | b'\n' | b'\r')))
    .then(|| msvc6_base::srcio::decode(s))
}

fn operand_values(i: &iced_x86::Instruction) -> Vec<u64> {
    let mut vals = Vec::new();
    for op in 0..i.op_count() {
        match i.op_kind(op) {
            OpKind::Memory => vals.push(u64::from(i.memory_displacement32())),
            OpKind::Immediate32 | OpKind::Immediate8to32 => vals.push(i.immediate(op)),
            _ => {}
        }
    }
    vals
}

struct ImgCtx<'a> {
    env: &'a Env,
    own: std::ops::Range<u32>,
}

impl ImgCtx<'_> {
    fn rva_of(&self, v: u64) -> Option<u32> {
        (v >= self.env.base && v < self.env.top)
            .then(|| u32::try_from(v - self.env.base).ok())
            .flatten()
    }

    fn data_key(&self, d: &Dec, rva: u32) -> String {
        if let Some(n) = self.env.iat.get(&rva) {
            return format!("D:I:{n}");
        }
        if !self.env.named().contains(&rva) {
            let sz = d.insn.memory_size().size();
            let fpu = format!("{:?}", d.insn.mnemonic()).starts_with('F');
            if fpu
                && matches!(sz, 4 | 8 | 10)
                && let Some(b) = self.env.img.at_rva(Rva(rva), sz)
            {
                return format!("R:{}", hex(b));
            }
            let head = (0..=LITERAL_PROBE)
                .rev()
                .find_map(|n| self.env.img.at_rva(Rva(rva), n))
                .unwrap_or(&[]);
            if let Some(s) = literal(head) {
                return format!("S:{s}");
            }
        }
        format!("D:{rva:x}")
    }

    fn in_code(&self, r: u32) -> bool {
        self.own.contains(&r) || self.env.in_text(r)
    }

    fn anchors(&self, dec: &[Dec]) -> Vec<Anchor> {
        let mut out = Vec::new();
        let text = self.env.text();
        for (ix, d) in dec.iter().enumerate() {
            let i = &d.insn;
            let mut push = |key: String| {
                out.push(Anchor {
                    ix,
                    label: key.clone(),
                    key,
                });
            };
            match i.flow_control() {
                FlowControl::Return => {
                    push("RET".into());
                    continue;
                }
                FlowControl::Call if i.op0_kind() == OpKind::NearBranch32 => {
                    let r = self.own.start.wrapping_add(
                        u32::try_from(i.near_branch_target() & 0xffff_ffff).unwrap_or(0),
                    );
                    let t = follow_thunk(text, self.env.text_rva, r);
                    push(format!("C:{t:x}"));
                    continue;
                }
                FlowControl::IndirectCall
                    if i.op0_kind() == OpKind::Memory
                        && i.memory_base() == Register::None
                        && self.rva_of(u64::from(i.memory_displacement32())).is_some() =>
                {
                    if let Some(r) = self.rva_of(u64::from(i.memory_displacement32())) {
                        push(match self.env.iat.get(&r) {
                            Some(n) => format!("C:I:{n}"),
                            None => format!("C:[{r:x}]"),
                        });
                    }
                    continue;
                }
                FlowControl::IndirectBranch
                    if i.op0_kind() == OpKind::Memory
                        && i.memory_index() != Register::None
                        && self.rva_of(u64::from(i.memory_displacement32())).is_some() =>
                {
                    push("SW".into());
                    continue;
                }
                _ => {}
            }
            for v in operand_values(i) {
                let Some(r) = self.rva_of(v) else { continue };
                if self.in_code(r) {
                    continue;
                }
                push(self.data_key(d, r));
            }
        }
        out
    }

    fn addrs(&self, dec: &[Dec]) -> Vec<Option<Vec<u32>>> {
        dec.iter()
            .map(|d| {
                Some(
                    operand_values(&d.insn)
                        .into_iter()
                        .filter_map(|v| self.rva_of(v))
                        .filter(|r| !self.in_code(*r) && !self.env.iat.contains_key(r))
                        .collect(),
                )
            })
            .collect()
    }
}

struct OursCtx<'a> {
    env: &'a Env,
    data: BTreeMap<String, Vec<u8>>,
}

impl OursCtx<'_> {
    fn rva(&self, sym: &str) -> Option<u32> {
        let text = self.env.text();
        self.env
            .names
            .rva(sym, text, self.env.text_rva)
            .map(|r| follow_thunk(text, self.env.text_rva, r))
    }

    fn anchors(&self, c: &Comdat, dec: &[Dec]) -> Vec<Anchor> {
        let mut out = Vec::new();
        for (ix, d) in dec.iter().enumerate() {
            let i = &d.insn;
            let (lo, hi) = (
                d.ip,
                d.ip.saturating_add(u32::try_from(i.len()).unwrap_or(0)),
            );
            let mut push = |key: String, label: &str| {
                out.push(Anchor {
                    ix,
                    key,
                    label: label.to_string(),
                });
            };
            if i.flow_control() == FlowControl::Return {
                push("RET".into(), "RET");
                continue;
            }
            let call = matches!(
                i.flow_control(),
                FlowControl::Call | FlowControl::IndirectCall
            );
            for r in c.relocs_in(lo, hi) {
                let field = c.field(r.at);
                let sym = r.symbol.as_str();
                if let Some(n) = comdat::import_name(sym) {
                    push(format!("{}:I:{n}", if call { "C" } else { "D" }), sym);
                    continue;
                }
                if r.target_section
                    .as_deref()
                    .is_some_and(|t| t.starts_with(".text"))
                {
                    if i.flow_control() == FlowControl::IndirectBranch {
                        push("SW".into(), "SW");
                    }
                    continue;
                }
                if r.kind == REL_REL32 {
                    if let Some(t) = self.rva(sym) {
                        push(format!("C:{t:x}"), sym);
                    }
                    continue;
                }
                if let Some(t) = self.rva(sym) {
                    let t = t.wrapping_add(field);
                    push(
                        if call {
                            format!("C:[{t:x}]")
                        } else {
                            format!("D:{t:x}")
                        },
                        sym,
                    );
                    continue;
                }
                let Some(b) = self.data.get(sym) else {
                    continue;
                };
                if sym.starts_with("??_C@") {
                    if let Some(s) = b.get(field as usize..).and_then(literal) {
                        push(format!("S:{s}"), sym);
                    }
                } else if sym.starts_with("__real@") {
                    push(format!("R:{}", hex(b)), sym);
                }
            }
        }
        out
    }

    fn addrs(&self, c: &Comdat, dec: &[Dec]) -> Vec<Option<Vec<u32>>> {
        let text = self.env.text();
        dec.iter()
            .map(|d| {
                let (lo, hi) = (
                    d.ip,
                    d.ip.saturating_add(u32::try_from(d.insn.len()).unwrap_or(0)),
                );
                let mut out = Vec::new();
                for r in c.relocs_in(lo, hi) {
                    if r.kind == REL_REL32
                        || comdat::import_name(&r.symbol).is_some()
                        || r.target_section
                            .as_deref()
                            .is_some_and(|t| t.starts_with(".text"))
                    {
                        continue;
                    }
                    let t = self
                        .env
                        .names
                        .rva(&r.symbol, text, self.env.text_rva)?
                        .wrapping_add(c.field(r.at));
                    if !self.env.in_text(t) {
                        out.push(t);
                    }
                }
                Some(out)
            })
            .collect()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AddendMismatch {
    pub img_ix: usize,
    pub ours_ix: usize,
    pub img: Vec<u32>,
    pub ours: Vec<u32>,
}

#[must_use]
pub fn addend_mismatches(img: &Side, ours: &Side, segs: &[Seg]) -> (Vec<AddendMismatch>, usize) {
    let (mut out, mut compared) = (Vec::new(), 0);
    for s in segs {
        for r in align_segment(img, ours, s.ia, s.oa) {
            let (Some(i), Some(o)) = (r.img, r.ours) else {
                continue;
            };
            if !matches!(r.pairing, Pairing::Same | Pairing::Form) {
                continue;
            }
            let (Some(Some(x)), Some(Some(y))) = (img.addrs.get(i), ours.addrs.get(o)) else {
                continue;
            };
            if x.is_empty() || x.len() != y.len() {
                continue;
            }
            compared += 1;
            if x != y {
                out.push(AddendMismatch {
                    img_ix: i,
                    ours_ix: o,
                    img: x.clone(),
                    ours: y.clone(),
                });
            }
        }
    }
    (out, compared)
}

fn reloc_mask(rel: &[bool]) -> impl Fn(u32, u32, u64) -> bool + '_ {
    move |ip: u32, len: u32, _v: u64| {
        let lo = ip as usize;
        rel.get(lo..lo.saturating_add(len as usize))
            .is_some_and(|m| m.iter().any(|b| *b))
    }
}

fn our_tables(c: &Comdat) -> Vec<(usize, usize)> {
    let mask = reloc_mask(&c.relocated);
    let dec = decode_keyed(&c.bytes, &mask, false);
    let mut out = Vec::new();
    for d in dec
        .iter()
        .filter(|d| d.insn.memory_index() != Register::None)
    {
        let (lo, hi) = (
            d.ip,
            d.ip.saturating_add(u32::try_from(d.insn.len()).unwrap_or(0)),
        );
        for r in c
            .relocs_in(lo, hi)
            .iter()
            .filter(|r| r.kind == REL_DIR32 && r.target_section.as_deref() == Some(".text"))
        {
            out.push((
                d.ip as usize,
                r.target_offset.wrapping_add(c.field(r.at)) as usize,
            ));
        }
    }
    out
}

fn code_end(len: usize, tables: impl Iterator<Item = (usize, usize)>) -> usize {
    let mut cut = len;
    for (at, t) in tables {
        if at >= cut {
            break;
        }
        if t > 0 && t < cut {
            cut = t;
        }
    }
    cut
}

pub fn reachable_end(
    bytes: &[u8],
    relocated: &[bool],
    table: &dyn Fn(usize) -> Vec<usize>,
) -> usize {
    let mut seen = BTreeSet::new();
    let mut work = vec![0usize];
    let mut end = 0;
    let inside = |t: u64| usize::try_from(t).ok().filter(|t| *t < bytes.len());
    while let Some(o) = work.pop() {
        if o >= bytes.len() || !seen.insert(o) {
            continue;
        }
        let Some(rest) = bytes.get(o..) else { continue };
        let i = decode_one(rest, o as u64);
        let next = o.saturating_add(i.len());
        if i.is_invalid() || next > bytes.len() || i.mnemonic() == Mnemonic::Int3 {
            continue;
        }
        end = end.max(next);
        let branch = matches!(i.op0_kind(), OpKind::NearBranch32 | OpKind::NearBranch16);
        let reloc = relocated.get(o..next).is_some_and(|m| m.iter().any(|b| *b));
        let target = (branch && !reloc)
            .then(|| inside(i.near_branch_target()))
            .flatten();
        match i.flow_control() {
            FlowControl::Return => {}
            FlowControl::UnconditionalBranch => work.extend(target),
            FlowControl::ConditionalBranch => work.extend(target.into_iter().chain([next])),
            FlowControl::IndirectBranch => {
                work.extend(table(o).into_iter().filter(|t| *t < bytes.len()));
            }
            _ => work.push(next),
        }
    }
    end
}

#[must_use]
pub fn image_table_targets(
    code: &[u8],
    start: u64,
    text: &[u8],
    text_va: u64,
    o: usize,
) -> Vec<usize> {
    image_table_targets_from(code, start, text, text_va, o, &mut BTreeSet::new())
}

#[must_use]
pub fn image_table_targets_from(
    code: &[u8],
    start: u64,
    text: &[u8],
    text_va: u64,
    o: usize,
    walked: &mut BTreeSet<usize>,
) -> Vec<usize> {
    let Some(rest) = code.get(o..) else {
        return Vec::new();
    };
    let i = decode_one(rest, 0);
    if i.op0_kind() != OpKind::Memory || i.memory_index() == Register::None {
        return Vec::new();
    }
    let top = start.saturating_add(code.len() as u64);
    let v = u64::from(i.memory_displacement32());
    let Some(at) = v.checked_sub(text_va).and_then(|a| usize::try_from(a).ok()) else {
        return Vec::new();
    };
    let mut out = Vec::new();
    for (k, c) in text
        .get(at..)
        .unwrap_or(&[])
        .as_chunks::<4>()
        .0
        .iter()
        .enumerate()
    {
        let t = u64::from(u32::from_le_bytes(*c));
        if t < start || t >= top || !walked.insert(at.saturating_add(k.saturating_mul(4))) {
            break;
        }
        let Ok(x) = usize::try_from(t - start) else {
            break;
        };
        out.push(x);
    }
    out
}

fn self_table_reloc(c: &Comdat, p: usize) -> Option<&Reloc> {
    c.reloc_at(u32::try_from(p).ok()?)
        .iter()
        .find(|r| r.kind == REL_DIR32 && r.target_section.as_deref() == Some(".text"))
}

#[must_use]
pub fn our_table_targets(c: &Comdat, o: usize) -> Vec<usize> {
    our_table_targets_from(c, o, &mut BTreeSet::new())
}

#[must_use]
pub fn our_table_targets_from(c: &Comdat, o: usize, walked: &mut BTreeSet<usize>) -> Vec<usize> {
    let hi = c.bytes.get(o..).map_or(0, |r| decode_one(r, 0).len());
    let Some(disp) = (o..o.saturating_add(hi)).find_map(|p| self_table_reloc(c, p)) else {
        return Vec::new();
    };
    let mut t = disp.target_offset.wrapping_add(c.field(disp.at)) as usize;
    let mut out = Vec::new();
    while let Some(r) = self_table_reloc(c, t) {
        if !walked.insert(t) {
            break;
        }
        out.push(r.target_offset.wrapping_add(c.field(r.at)) as usize);
        t = t.saturating_add(4);
    }
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tail {
    NopTail { rec: u32 },
    Padded { pad: u32, len: u32 },
}

#[must_use]
pub fn nop_tail(env: &Env, addr: u32, bytes: &[u8]) -> Option<Tail> {
    let len = u32::try_from(bytes.len()).ok()?;
    let rec = env.recorded(addr)?;
    if rec == 0 || rec >= len || len - rec >= 16 || addr.checked_add(len)? % 16 != 0 {
        return None;
    }
    let tail = bytes.get(rec as usize..)?;
    if !tail.iter().all(|&b| b == 0x90) {
        return None;
    }
    let off = addr.checked_sub(env.text_rva)? as usize;
    let img = env
        .text()
        .get(off.saturating_add(rec as usize)..off.saturating_add(len as usize))?;
    (img == tail).then_some(Tail::NopTail { rec })
}

#[must_use]
pub fn padded(env: &Env, addr: u32, len: u32) -> Option<Tail> {
    let end = addr.checked_add(len)?;
    let seats = &env.seats;
    let rec = match seats.binary_search_by_key(&addr, |x| x.0) {
        Ok(k) => seats.get(k)?.1,
        Err(k) => {
            let prev_ok = k == 0
                || seats
                    .get(k.checked_sub(1)?)
                    .is_some_and(|p| p.0.saturating_add(p.1) <= addr);
            let next_ok = seats.get(k).is_none_or(|n| n.0 >= end);
            if !(prev_ok && next_ok) {
                return None;
            }
            0
        }
    };
    env.text_pad.iter().find_map(|&(pad, pad_len)| {
        let ok = pad.checked_add(pad_len)? == end && pad > addr && pad.checked_sub(addr)? >= rec;
        let ok = ok && (rec == 0 || addr.checked_add(rec)? == pad);
        ok.then_some(Tail::Padded { pad, len: pad_len })
    })
}

#[must_use]
pub fn pad_trimmed_len(env: &Env, addr: u32, bytes: &[u8]) -> Option<usize> {
    let len = u32::try_from(bytes.len()).ok()?;
    match nop_tail(env, addr, bytes).or_else(|| padded(env, addr, len))? {
        Tail::NopTail { rec } => Some(rec as usize),
        Tail::Padded { pad, .. } => Some(pad.checked_sub(addr)? as usize),
    }
}

fn side(bytes: &[u8], masked: Vec<bool>, mask: &dyn Fn(u32, u32, u64) -> bool) -> Side {
    let dec = decode_keyed(bytes, mask, false);
    let reg_keys = decode_keyed(bytes, mask, true)
        .into_iter()
        .map(|d| d.key)
        .collect();
    Side {
        bytes: bytes.to_vec(),
        masked,
        dec,
        reg_keys,
        anchors: Vec::new(),
        addrs: Vec::new(),
        wide: Vec::new(),
    }
}

#[must_use]
pub fn test_side(bytes: &[u8]) -> Side {
    side(bytes, vec![false; bytes.len()], &|_, _, _| false)
}

fn image_off_fields(enc: &[u8], base: u64, top: u64) -> Vec<(usize, usize)> {
    let mut d = iced_x86::Decoder::with_ip(32, enc, 0, iced_x86::DecoderOptions::NONE);
    let i = d.decode();
    let co = d.get_constant_offsets(&i);
    let hit = |v: u64| v >= base && v < top;
    let mut out = Vec::new();
    if co.has_displacement() && hit(u64::from(i.memory_displacement32())) {
        out.push((co.displacement_offset(), co.displacement_size()));
    }
    if co.has_immediate()
        && (0..i.op_count())
            .any(|k| matches!(i.op_kind(k), OpKind::Immediate32) && hit(i.immediate(k)))
    {
        out.push((co.immediate_offset(), co.immediate_size()));
    }
    out
}

#[derive(Debug, Clone)]
pub struct Analysis {
    pub header: String,
    pub segs: Vec<Seg>,
    pub start: u64,
    pub rva: u32,
    pub img: Side,
    pub ours: Side,
    pub comdat: Comdat,
    pub obj: PathBuf,
    pub want: usize,
    pub whence: &'static str,
}

#[must_use]
pub fn image_want(env: &Env, rva: u32, ours_len: usize) -> (usize, &'static str) {
    env.extent(rva, ours_len)
}

#[must_use]
pub fn raw_extent(want: usize, whence: &str, ours: usize) -> (usize, bool) {
    if whence == RECORDED {
        (want, true)
    } else {
        (ours, false)
    }
}

pub fn analyze_in(env: &Env, rva: u32, opath: &Path, ours: Comdat, max: usize) -> Result<Analysis> {
    let text = env.text();
    let (base, top) = (env.base, env.top);
    let (want, whence) = image_want(env, rva, ours.bytes.len());
    let reference = env
        .img
        .at_rva(Rva(rva), want)
        .ok_or_else(|| anyhow!("{rva:#x}+{want:#x} is not in {}'s file bytes", env.label))?;
    let own = rva..rva.saturating_add(u32::try_from(want).unwrap_or(u32::MAX));
    let image_mask = |_ip: u32, _len: u32, v: u64| v >= base && v < top;
    let first = decode_keyed(reference, &image_mask, false);
    let iend = code_end(
        reference.len(),
        first
            .iter()
            .filter(|d| {
                d.insn.mnemonic() != Mnemonic::Lea
                    && (d.insn.memory_base() != Register::None
                        || d.insn.memory_index() != Register::None)
            })
            .filter_map(|d| {
                let v = u64::from(d.insn.memory_displacement32());
                (v >= base && v < top)
                    .then(|| u32::try_from(v - base).ok())
                    .flatten()
                    .filter(|r| own.contains(r))
                    .map(|r| (d.ip as usize, (r - rva) as usize))
            }),
    );
    let start = base.saturating_add(u64::from(rva));
    let text_va = base.saturating_add(u64::from(env.text_rva));
    let walked = std::cell::RefCell::new(BTreeSet::new());
    let ireach = reachable_end(reference, &[], &|o| {
        image_table_targets_from(reference, start, text, text_va, o, &mut walked.borrow_mut())
    });
    let itail = (iend, if ireach > 0 { iend.min(ireach) } else { iend });
    let iend = itail.1;
    let ibytes = reference.get(..iend).unwrap_or(reference);
    let mut imasked = vec![false; ibytes.len()];
    for d in &first {
        let lo = d.ip as usize;
        let hi = lo.saturating_add(d.insn.len());
        if hi > ibytes.len() {
            break;
        }
        for (a, n) in image_off_fields(ibytes.get(lo..hi).unwrap_or(&[]), base, top) {
            for i in lo.saturating_add(a)..lo.saturating_add(a).saturating_add(n) {
                if let Some(m) = imasked.get_mut(i) {
                    *m = true;
                }
            }
        }
    }
    let mut is = side(ibytes, imasked, &image_mask);
    let ictx = ImgCtx { env, own };
    is.anchors = ictx.anchors(&is.dec);
    is.addrs = ictx.addrs(&is.dec);
    let oend = code_end(ours.bytes.len(), our_tables(&ours).into_iter())
        .min(pad_trimmed_len(env, rva, &ours.bytes).unwrap_or(ours.bytes.len()));
    let walked = std::cell::RefCell::new(BTreeSet::new());
    let oreach = reachable_end(&ours.bytes, &ours.relocated, &|o| {
        our_table_targets_from(&ours, o, &mut walked.borrow_mut())
    });
    let otail = (oend, if oreach > 0 { oend.min(oreach) } else { oend });
    let oend = otail.1;
    let obytes = ours.bytes.get(..oend).unwrap_or(&ours.bytes);
    let rel = ours
        .relocated
        .get(..oend)
        .unwrap_or(&ours.relocated)
        .to_vec();
    let ours_mask = reloc_mask(&rel);
    let mut os = side(obytes, rel.clone(), &ours_mask);
    let data: BTreeMap<String, Vec<u8>> = comdat::data(opath)?
        .into_iter()
        .filter_map(|c| Some((c.symbol?, c.bytes)))
        .collect();
    let octx = OursCtx { env, data };
    os.anchors = octx.anchors(&ours, &os.dec);
    os.addrs = octx.addrs(&ours, &os.dec);
    let (segs, matched, wide) = segment(&is, &os, max);
    os.wide = wide;
    let refused = match matched.refused {
        0 => String::new(),
        n => format!(" / {n} refused (not paired function-wide)"),
    };
    let header = format!(
        "{} {rva:#x}  `{}` in {}  image {:#x} code bytes of {want:#x} ({whence}), ours {:#x} of {:#x}; \
         anchors image {} / ours {} / matched {}{refused}; min segment {MIN_SEGMENT}, max {max}\n  anchor kinds: image {}; ours {}",
        env.label,
        ours.symbol.as_deref().unwrap_or("?"),
        opath.display(),
        ibytes.len(),
        obytes.len(),
        ours.bytes.len(),
        is.anchors.len(),
        os.anchors.len(),
        matched.cut,
        kinds(&is.anchors),
        kinds(&os.anchors)
    ) + &tail_lines(start, itail, otail);
    Ok(Analysis {
        header,
        segs,
        start,
        rva,
        img: is,
        ours: os,
        comdat: ours,
        obj: opath.to_path_buf(),
        want,
        whence,
    })
}

fn tail_lines(start: u64, img: (usize, usize), ours: (usize, usize)) -> String {
    let mut s = String::new();
    if img.0 > img.1 {
        s += &format!(
            "\n  TAIL {} bytes (not scored): image {:#x}..{:#x}, past the last reachable instruction",
            img.0 - img.1,
            start.saturating_add(img.1 as u64),
            start.saturating_add(img.0 as u64)
        );
    }
    if ours.0 > ours.1 {
        s += &format!(
            "\n  TAIL {} bytes (not scored): ours +{:#x}..+{:#x}, past the last reachable instruction",
            ours.0 - ours.1,
            ours.1,
            ours.0
        );
    }
    s
}

fn kinds(a: &[Anchor]) -> String {
    let mut m: BTreeMap<&str, usize> = BTreeMap::new();
    for x in a {
        let k = match x.key.get(..4) {
            Some("C:I:") => "C:I",
            Some("D:I:") => "D:I",
            _ => x.key.split(':').next().unwrap_or(""),
        };
        *m.entry(k).or_default() += 1;
    }
    m.iter()
        .map(|(k, n)| format!("{k}={n}"))
        .collect::<Vec<_>>()
        .join(" ")
}

#[derive(Debug, PartialEq, Eq)]
pub struct Unit {
    pub img: (usize, usize),
    pub segs: usize,
    pub shape_bytes: usize,
}

#[must_use]
pub fn batch_units(segs: &[Seg], max: usize) -> Vec<Unit> {
    let mut out: Vec<Unit> = Vec::new();
    let mut open = false;
    for s in segs.iter().filter(|s| s.verdict == Verdict::Shape) {
        match out.last_mut() {
            Some(u) if open && s.img.1.saturating_sub(u.img.0) <= max => {
                u.img.1 = s.img.1;
                u.segs += 1;
                u.shape_bytes += s.img_len();
            }
            _ => out.push(Unit {
                img: s.img,
                segs: 1,
                shape_bytes: s.img_len(),
            }),
        }
        open = !s.oversize;
    }
    out
}

#[must_use]
pub fn edit_totals(segs: &[Seg]) -> (usize, usize) {
    segs.iter()
        .fold((0, 0), |(e, i), s| (e + s.edit, i + s.img_len()))
}

#[must_use]
#[allow(clippy::cast_precision_loss)]
pub fn percent(part: usize, whole: usize) -> String {
    if whole == 0 {
        return "0.0%".to_string();
    }
    format!("{:.1}%", part as f64 * 100.0 / whole as f64)
}

#[must_use]
pub fn summary(segs: &[Seg], max: usize) -> String {
    let count = |v: Verdict| segs.iter().filter(|s| s.verdict == v).count();
    let shape = || segs.iter().filter(|s| s.verdict == Verdict::Shape);
    let shape_bytes: usize = shape().map(Seg::img_len).sum();
    let shape_diff: usize = shape().map(|s| s.diff).sum();
    let mut lens: Vec<usize> = segs.iter().map(Seg::img_len).collect();
    lens.sort_unstable();
    let median = lens.get(lens.len() / 2).copied().unwrap_or(0);
    let p90 = lens.get(lens.len() * 9 / 10).copied().unwrap_or(0);
    let longest = lens.last().copied().unwrap_or(0);
    let over = segs.iter().filter(|s| s.oversize).count();
    let (edit, image) = edit_totals(segs);
    format!(
        "summary: {} segment(s): EXACT {} / REG-ONLY {} / SHAPE {}; SHAPE bytes {shape_bytes} (image), \
         {shape_diff} differing; median segment {median} bytes, p90 {p90}, max {longest}; \
         OVERSIZE {over} (cap {max}); {edit} edit bytes of {image} image bytes ({} divergent)",
        segs.len(),
        count(Verdict::Exact),
        count(Verdict::RegOnly),
        count(Verdict::Shape),
        percent(edit, image),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    fn mk(bytes: &[u8], anchors: &[(usize, &str)]) -> Side {
        let mut s = test_side(bytes);
        s.anchors = anchors
            .iter()
            .map(|(ix, k)| Anchor {
                ix: *ix,
                key: (*k).into(),
                label: (*k).into(),
            })
            .collect();
        s
    }

    fn movs(n: usize, swapped: bool) -> Vec<u8> {
        [0x89, if swapped { 0xd9 } else { 0xd8 }].repeat(n)
    }

    #[test]
    fn identical_streams_are_one_exact_segment_with_no_edit() {
        let b = movs(40, false);
        let (segs, _, _) = segment(&mk(&b, &[]), &mk(&b, &[]), MAX_SEGMENT);
        assert_eq!(segs.len(), 1);
        assert_eq!(segs[0].verdict, Verdict::Exact);
        assert_eq!(edit_totals(&segs), (0, 80));
    }

    #[test]
    fn a_register_swap_is_reg_only_and_costs_its_differing_bytes() {
        let a = movs(40, false);
        let b = movs(40, true);
        let (segs, _, _) = segment(&mk(&a, &[]), &mk(&b, &[]), MAX_SEGMENT);
        assert_eq!(segs[0].verdict, Verdict::RegOnly);
        assert_eq!(edit_totals(&segs).0, 40);
    }

    #[test]
    fn an_inserted_instruction_costs_its_own_length_not_the_rest() {
        let a = movs(40, false);
        let mut b = vec![0x90];
        b.extend(movs(40, false));
        let (segs, _, _) = segment(&mk(&a, &[]), &mk(&b, &[]), MAX_SEGMENT);
        assert_eq!(edit_totals(&segs).0, 1);
        assert_eq!(segs[0].verdict, Verdict::Shape);
    }

    #[test]
    fn matched_anchors_cut_the_streams_into_independent_segments() {
        let mut a = movs(30, false);
        a.push(0xc3);
        a.extend(movs(30, false));
        let mut b = movs(30, true);
        b.push(0xc3);
        b.extend(movs(30, false));
        let (segs, m, _) = segment(
            &mk(&a, &[(30, "RET")]),
            &mk(&b, &[(30, "RET")]),
            MAX_SEGMENT,
        );
        assert_eq!(m.cut, 1);
        assert_eq!(segs.len(), 2);
        assert_eq!(segs[0].verdict, Verdict::RegOnly);
        assert_eq!(segs[1].verdict, Verdict::Exact);
    }

    #[test]
    fn a_literal_is_printable_text_of_two_bytes_or_more() {
        assert_eq!(literal(b"ab\0").as_deref(), Some("ab"));
        assert_eq!(literal(b"a\0"), None);
        assert_eq!(literal(b"\x01b\0"), None);
        assert_eq!(literal(b"abc"), None);
    }

    #[test]
    fn reachability_stops_at_the_last_reachable_instruction() {
        let b = [0x55, 0xc3, 0x90, 0x90, 0xcc];
        assert_eq!(reachable_end(&b, &[false; 5], &|_| Vec::new()), 2);
        let j = [0xeb, 0x01, 0xcc, 0xc3, 0x90];
        assert_eq!(reachable_end(&j, &[false; 5], &|_| Vec::new()), 4);
    }

    #[test]
    fn a_code_end_takes_the_first_table_an_earlier_instruction_names() {
        assert_eq!(code_end(100, [(0, 60), (10, 40), (70, 20)].into_iter()), 40);
        assert_eq!(code_end(100, std::iter::empty()), 100);
    }

    #[test]
    fn a_percentage_has_one_decimal() {
        assert_eq!(percent(1, 3), "33.3%");
        assert_eq!(percent(0, 0), "0.0%");
    }
}
