use super::conv::clash;
use super::feat::{Callee, Feat, Side, width_clash};
use std::collections::{BTreeMap, BTreeSet, HashMap};

pub const MAX_FANOUT: usize = 3;

pub const MAX_IMPORT_FANOUT: usize = 2;

pub const MAX_GAP: usize = 6;

pub const MAX_SLOT_GAP: usize = 128;

pub const MAX_CALLEE_FANIN: usize = 40;

pub const MAX_SIZE_RATIO: f32 = 12.0;

pub const MAX_ROUNDS: usize = 40;

pub const MAX_EVIDENCE: usize = 4;

pub const MIN_SHAPE: usize = 30;

pub const SHAPE_DICE: f32 = 0.55;

pub const SHAPE_MARGIN: f32 = 0.08;

pub const SHAPE_KEEP: usize = 8;

pub const SHAPE_FLOOR: f32 = 0.45;

pub const SHAPE_POSTING: usize = 400;

pub const CTX_COMPAT: f32 = 0.34;

pub const CTX_GAP: usize = 64;

pub const LCS_CELLS: usize = 16_000_000;

pub const TIE: f32 = 1e-6;

pub type Props = BTreeMap<(usize, usize), (f32, Vec<String>)>;

#[derive(Debug, Default, Clone)]
pub struct Structure {
    pub ours: Vec<Option<usize>>,
    pub sib: Vec<Option<usize>>,
    pub layout: bool,
}

#[derive(Debug, Default, Clone)]
pub struct Pairs {
    pub o2s: BTreeMap<usize, usize>,
    pub s2o: BTreeMap<usize, usize>,
    pub why: BTreeMap<usize, (usize, String)>,
    pub refused_call: BTreeSet<(usize, usize)>,
    pub refused_caller: BTreeSet<(usize, usize)>,
    pub refused_tie: BTreeSet<(usize, usize)>,
    pub refused_slot: BTreeSet<(usize, usize)>,
    pub refused_layout: BTreeSet<(usize, usize)>,
}

impl Pairs {
    pub fn add(&mut self, a: usize, b: usize, round: usize, why: String) {
        self.o2s.insert(a, b);
        self.s2o.insert(b, a);
        self.why.insert(a, (round, why));
    }
}

#[must_use]
pub fn cut(s: &str, n: usize) -> String {
    s.chars()
        .take(n)
        .map(|c| if c.is_control() { ' ' } else { c })
        .collect()
}

fn real(n: usize) -> f32 {
    n as f32
}

fn ratio(n: usize, d: usize) -> f32 {
    real(n) / real(d.max(1))
}

fn index_of(side: &Side, key: &dyn Fn(&Feat) -> Vec<String>) -> BTreeMap<String, Vec<usize>> {
    let mut m: BTreeMap<String, Vec<usize>> = BTreeMap::new();
    for (i, f) in side.feats.iter().enumerate() {
        for k in key(f) {
            m.entry(k).or_default().push(i);
        }
    }
    m
}

fn add_vote(votes: &mut Props, a: usize, b: usize, w: f32, why: String) {
    let e = votes.entry((a, b)).or_insert((0.0, Vec::new()));
    e.0 += w;
    if e.1.len() < MAX_EVIDENCE {
        e.1.push(why);
    }
}

#[must_use]
pub fn anchor_votes(o: &Side, s: &Side) -> Props {
    let mut votes = Props::new();
    let strs = |f: &Feat| f.strs.clone();
    let (os, ss) = (index_of(o, &strs), index_of(s, &strs));
    for (k, a) in &os {
        let Some(b) = ss.get(k) else { continue };
        if a.len() > MAX_FANOUT || b.len() > MAX_FANOUT {
            continue;
        }
        let w = (if k.len() >= 8 { 1.0 } else { 0.5 }) / real(a.len().saturating_mul(b.len()));
        for x in a {
            for y in b {
                add_vote(&mut votes, *x, *y, w, format!("str:{}", cut(k, 40)));
            }
        }
    }
    let imps = |f: &Feat| f.imports.iter().cloned().collect::<Vec<_>>();
    let (oi, si) = (index_of(o, &imps), index_of(s, &imps));
    for (k, a) in &oi {
        let Some(b) = si.get(k) else { continue };
        if a.len() > MAX_IMPORT_FANOUT || b.len() > MAX_IMPORT_FANOUT {
            continue;
        }
        let w = 0.5 / real(a.len().saturating_mul(b.len()));
        for x in a {
            for y in b {
                add_vote(&mut votes, *x, *y, w, format!("imp:{k}"));
            }
        }
    }
    for (tag, w, get) in [
        ("body", 2.0f32, (|f: &Feat| f.body) as fn(&Feat) -> u64),
        ("loose", 1.5, |f: &Feat| f.loose),
    ] {
        let key = |f: &Feat| {
            if f.body == 0 {
                Vec::new()
            } else {
                vec![format!("{:016x}", get(f))]
            }
        };
        let (ob, sb) = (index_of(o, &key), index_of(s, &key));
        for (k, a) in &ob {
            if let (Some([y]), [x]) = (sb.get(k).map(Vec::as_slice), a.as_slice()) {
                add_vote(&mut votes, *x, *y, w, tag.to_string());
            }
        }
    }
    votes
}

fn gram_set(m: &[u16]) -> BTreeSet<u64> {
    m.windows(4)
        .map(|w| {
            w.iter()
                .fold(0u64, |h, x| h.wrapping_mul(0x1_0000_01b3) ^ u64::from(*x))
        })
        .collect()
}

#[must_use]
pub fn shape_candidates(o: &Side, s: &Side) -> BTreeMap<usize, Vec<(f32, usize)>> {
    let sg: Vec<BTreeSet<u64>> = s
        .feats
        .iter()
        .map(|f| {
            if f.mnem.len() >= MIN_SHAPE {
                gram_set(&f.mnem)
            } else {
                BTreeSet::new()
            }
        })
        .collect();
    let mut post: HashMap<u64, Vec<usize>> = HashMap::new();
    for (j, g) in sg.iter().enumerate() {
        for x in g {
            post.entry(*x).or_default().push(j);
        }
    }
    let mut out = BTreeMap::new();
    for (i, f) in o.feats.iter().enumerate() {
        if f.mnem.len() < MIN_SHAPE {
            continue;
        }
        let g = gram_set(&f.mnem);
        let mut hits: BTreeMap<usize, usize> = BTreeMap::new();
        for x in &g {
            if let Some(p) = post.get(x).filter(|p| p.len() <= SHAPE_POSTING) {
                for j in p {
                    let h = hits.entry(*j).or_insert(0);
                    *h = h.saturating_add(1);
                }
            }
        }
        let mut best: Vec<(f32, usize)> = hits
            .iter()
            .map(|(j, c)| {
                let nb = sg.get(*j).map_or(0, BTreeSet::len);
                (2.0 * real(*c) / real(g.len().saturating_add(nb).max(1)), *j)
            })
            .filter(|(d, _)| *d >= SHAPE_FLOOR)
            .collect();
        best.sort_by(|a, b| b.0.total_cmp(&a.0).then(a.1.cmp(&b.1)));
        best.truncate(SHAPE_KEEP);
        if !best.is_empty() {
            out.insert(i, best);
        }
    }
    out
}

#[must_use]
pub fn shape_votes(cands: &BTreeMap<usize, Vec<(f32, usize)>>) -> Props {
    let mut out = Props::new();
    for (i, best) in cands {
        let (Some(top), second) = (best.first(), best.get(1).map_or(0.0, |x| x.0)) else {
            continue;
        };
        if top.0 >= SHAPE_DICE && top.0 - second >= SHAPE_MARGIN {
            out.insert(
                (*i, top.1),
                (
                    1.0 + 4.0 * (top.0 - SHAPE_DICE),
                    vec![format!("shape {:.2}", top.0)],
                ),
            );
        }
    }
    out
}

fn callees(side: &Side, f: usize) -> BTreeSet<usize> {
    side.feats.get(f).map_or_else(BTreeSet::new, |ft| {
        ft.calls
            .iter()
            .filter_map(|c| match c {
                Callee::Fn(x) => Some(*x),
                _ => None,
            })
            .collect()
    })
}

#[must_use]
pub fn fits(o: &Side, s: &Side, p: usize, q: usize) -> bool {
    match (o.feats.get(p), s.feats.get(q)) {
        (Some(x), Some(y)) => clash(&x.conv, &y.conv).is_none() && !width_clash(x.widths, y.widths),
        _ => true,
    }
}

fn neighbor_proposals(
    o: &Side,
    s: &Side,
    pairs: &Pairs,
    cands: &BTreeMap<usize, Vec<(f32, usize)>>,
    refused: &mut BTreeSet<(usize, usize)>,
) -> Props {
    let mut out = Props::new();
    for (i, best) in cands {
        if pairs.o2s.contains_key(i) {
            continue;
        }
        let top = best.first().map_or(0.0, |x| x.0);
        if top < SHAPE_DICE {
            continue;
        }
        let mine_out: BTreeSet<usize> = callees(o, *i)
            .iter()
            .filter_map(|c| pairs.o2s.get(c).copied())
            .collect();
        let mine_in: BTreeSet<usize> = o
            .callers
            .get(*i)
            .into_iter()
            .flatten()
            .filter_map(|c| pairs.o2s.get(c).copied())
            .collect();
        let mut scored: Vec<(usize, usize)> = best
            .iter()
            .filter(|(d, j)| top - *d <= SHAPE_MARGIN && !pairs.s2o.contains_key(j))
            .map(|(_, j)| {
                let out_n = callees(s, *j).intersection(&mine_out).count();
                let in_n = s
                    .callers
                    .get(*j)
                    .map_or(0, |c| c.intersection(&mine_in).count());
                (out_n.saturating_add(in_n), *j)
            })
            .collect();
        scored.sort_by(|a, b| b.0.cmp(&a.0).then(a.1.cmp(&b.1)));
        if let Some((n, j)) = scored.first().copied()
            && n >= 1
            && scored.get(1).is_none_or(|x| x.0 < n)
        {
            if fits(o, s, *i, j) {
                out.insert((*i, j), (1.0, vec![format!("shape tie, {n} neighbours")]));
            } else {
                refused.insert((*i, j));
            }
        }
    }
    out
}

fn merged(mut a: Props, b: &Props) -> Props {
    for (k, (v, w)) in b {
        let e = a.entry(*k).or_insert((0.0, Vec::new()));
        e.0 += v;
        e.1.extend(w.iter().cloned());
    }
    a
}

#[derive(Debug, Clone, Copy)]
struct Best {
    votes: f32,
    other: usize,
    tied: bool,
}

fn note(m: &mut HashMap<usize, Best>, k: usize, other: usize, v: f32) {
    let e = m.entry(k).or_insert(Best {
        votes: f32::MIN,
        other: usize::MAX,
        tied: false,
    });
    if v > e.votes + TIE {
        *e = Best {
            votes: v,
            other,
            tied: false,
        };
    } else if (v - e.votes).abs() <= TIE && other != e.other {
        e.tied = true;
    }
}

fn comparable(na: usize, nb: usize) -> bool {
    let (na, nb) = (na.max(1), nb.max(1));
    real(na.max(nb)) / real(na.min(nb)) <= MAX_SIZE_RATIO || na.max(nb) <= 40
}

pub fn accept(
    props: &Props,
    pairs: &mut Pairs,
    sizes: (&[usize], &[usize]),
    round: usize,
    kind: &str,
) -> usize {
    let mut best_o: HashMap<usize, Best> = HashMap::new();
    let mut best_s: HashMap<usize, Best> = HashMap::new();
    for ((a, b), (v, _)) in props {
        if pairs.o2s.contains_key(a) || pairs.s2o.contains_key(b) {
            continue;
        }
        note(&mut best_o, *a, *b, *v);
        note(&mut best_s, *b, *a, *v);
    }
    let mut landed = 0usize;
    for (&(a, b), (v, why)) in props {
        if *v < 1.0 - TIE {
            continue;
        }
        let (Some(bo), Some(bs)) = (best_o.get(&a), best_s.get(&b)) else {
            continue;
        };
        if bo.other != b || bs.other != a || bo.tied || bs.tied {
            continue;
        }
        let na = sizes.0.get(a).copied().unwrap_or(1);
        let nb = sizes.1.get(b).copied().unwrap_or(1);
        if !comparable(na, nb) || pairs.o2s.contains_key(&a) || pairs.s2o.contains_key(&b) {
            continue;
        }
        pairs.add(a, b, round, format!("{kind}:{v:.1}:{}", why.join(",")));
        landed = landed.saturating_add(1);
    }
    landed
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Tok {
    I(String),
    P(usize),
    U(usize),
    X,
}

impl Tok {
    fn anchors(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::I(a), Self::I(b)) => a == b,
            (Self::P(a), Self::P(b)) => a == b,
            _ => false,
        }
    }
}

#[must_use]
pub fn lcs(a: &[Tok], b: &[Tok]) -> Vec<(usize, usize)> {
    let (n, m) = (a.len(), b.len());
    if n == 0 || m == 0 || n.saturating_mul(m) > LCS_CELLS {
        return Vec::new();
    }
    let w = m.saturating_add(1);
    let mut t = vec![0u32; n.saturating_add(1).saturating_mul(w)];
    let at = |t: &[u32], i: usize, j: usize| {
        t.get(i.saturating_mul(w).saturating_add(j))
            .copied()
            .unwrap_or(0)
    };
    for i in (0..n).rev() {
        for j in (0..m).rev() {
            let v = if a.get(i).zip(b.get(j)).is_some_and(|(x, y)| x.anchors(y)) {
                at(&t, i + 1, j + 1).saturating_add(1)
            } else {
                at(&t, i + 1, j).max(at(&t, i, j + 1))
            };
            if let Some(c) = t.get_mut(i.saturating_mul(w).saturating_add(j)) {
                *c = v;
            }
        }
    }
    let (mut i, mut j, mut out) = (0, 0, Vec::new());
    while i < n && j < m {
        if a.get(i).zip(b.get(j)).is_some_and(|(x, y)| x.anchors(y)) {
            out.push((i, j));
            i += 1;
            j += 1;
        } else if at(&t, i + 1, j) >= at(&t, i, j + 1) {
            i += 1;
        } else {
            j += 1;
        }
    }
    out
}

fn collapse(v: &[Tok]) -> Vec<Tok> {
    let mut out: Vec<Tok> = Vec::new();
    for t in v {
        if out.last() != Some(t) {
            out.push(t.clone());
        }
    }
    out
}

fn gaps(a: &[Tok], b: &[Tok]) -> Vec<(usize, usize, usize, usize)> {
    let mut bounds = Vec::new();
    let (mut pi, mut pj) = (0usize, 0usize);
    for (i, j) in lcs(a, b) {
        bounds.push((pi, i, pj, j));
        pi = i.saturating_add(1);
        pj = j.saturating_add(1);
    }
    bounds.push((pi, a.len(), pj, b.len()));
    bounds
}

fn single(s: &BTreeSet<usize>) -> Option<usize> {
    if s.len() == 1 {
        s.iter().next().copied()
    } else {
        None
    }
}

fn one_to_one(
    fw: &BTreeMap<usize, BTreeSet<usize>>,
    bw: &BTreeMap<usize, BTreeSet<usize>>,
) -> Vec<(usize, usize)> {
    fw.iter()
        .filter_map(|(p, qs)| {
            let q = single(qs)?;
            (single(bw.get(&q)?)? == *p).then_some((*p, q))
        })
        .collect()
}

pub fn gap_pairs(
    a: &[Tok],
    b: &[Tok],
    fits: &dyn Fn(usize, usize) -> bool,
    refused: &mut BTreeSet<(usize, usize)>,
) -> Vec<(usize, usize)> {
    gap_pairs_upto(a, b, fits, refused, MAX_GAP)
}

pub fn gap_pairs_upto(
    a: &[Tok],
    b: &[Tok],
    fits: &dyn Fn(usize, usize) -> bool,
    refused: &mut BTreeSet<(usize, usize)>,
    max: usize,
) -> Vec<(usize, usize)> {
    let mut fw: BTreeMap<usize, BTreeSet<usize>> = BTreeMap::new();
    let mut bw: BTreeMap<usize, BTreeSet<usize>> = BTreeMap::new();
    for (i0, i1, j0, j1) in gaps(a, b) {
        let (Some(ga), Some(gb)) = (a.get(i0..i1), b.get(j0..j1)) else {
            continue;
        };
        if ga.is_empty() || gb.is_empty() {
            continue;
        }
        let only_open = |g: &[Tok]| g.iter().all(|t| matches!(t, Tok::U(_) | Tok::X));
        if !only_open(ga) || !only_open(gb) {
            continue;
        }
        let (ca, cb) = if ga.len() == gb.len() {
            (ga.to_vec(), gb.to_vec())
        } else {
            (collapse(ga), collapse(gb))
        };
        if ca.len() != cb.len() || ca.len() > max {
            continue;
        }
        let open: Vec<(usize, usize)> = ca
            .iter()
            .zip(cb.iter())
            .filter_map(|(x, y)| match (x, y) {
                (Tok::U(p), Tok::U(q)) => Some((*p, *q)),
                _ => None,
            })
            .collect();
        let open = if open.iter().any(|(p, q)| !fits(*p, *q)) {
            match open.as_slice() {
                [(p0, q0), (p1, q1)] if fits(*p0, *q1) && fits(*p1, *q0) => {
                    vec![(*p0, *q1), (*p1, *q0)]
                }
                _ => {
                    refused.extend(open);
                    continue;
                }
            }
        } else {
            open
        };
        for (p, q) in open {
            fw.entry(p).or_default().insert(q);
            bw.entry(q).or_default().insert(p);
        }
    }
    one_to_one(&fw, &bw)
}

#[must_use]
pub fn context_compat(a: &Feat, b: &Feat) -> f32 {
    let keys = |f: &Feat| -> BTreeSet<String> {
        f.strs
            .iter()
            .cloned()
            .chain(f.imports.iter().cloned())
            .collect()
    };
    let (ka, kb) = (keys(a), keys(b));
    let union = ka.union(&kb).count();
    if ka.is_empty() || kb.is_empty() || union == 0 {
        return 0.0;
    }
    ratio(ka.intersection(&kb).count(), union)
}

pub fn gap_context_pairs(
    a: &[Tok],
    b: &[Tok],
    compat: &dyn Fn(usize, usize) -> f32,
) -> Vec<(usize, usize)> {
    let mut fw: BTreeMap<usize, BTreeSet<usize>> = BTreeMap::new();
    let mut bw: BTreeMap<usize, BTreeSet<usize>> = BTreeMap::new();
    for (i0, i1, j0, j1) in gaps(a, b) {
        let (Some(ga), Some(gb)) = (a.get(i0..i1), b.get(j0..j1)) else {
            continue;
        };
        if ga.is_empty() || gb.is_empty() || ga.len() > CTX_GAP || gb.len() > CTX_GAP {
            continue;
        }
        let (n, w) = (ga.len(), gb.len());
        let c = |i: usize, j: usize| -> f32 {
            match (ga.get(i), gb.get(j)) {
                (Some(Tok::U(p)), Some(Tok::U(q))) => {
                    let v = compat(*p, *q);
                    if v >= CTX_COMPAT { v } else { 0.0 }
                }
                _ => 0.0,
            }
        };
        let width = w + 1;
        let mut t = vec![0f32; (n + 1) * width];
        let at = |t: &[f32], i: usize, j: usize| t.get(i * width + j).copied().unwrap_or(0.0);
        for i in 1..=n {
            for j in 1..=w {
                let diag = at(&t, i - 1, j - 1) + c(i - 1, j - 1);
                let up = at(&t, i - 1, j);
                let left = at(&t, i, j - 1);
                if let Some(x) = t.get_mut(i * width + j) {
                    *x = diag.max(up).max(left);
                }
            }
        }
        let (mut i, mut j) = (n, w);
        while i > 0 && j > 0 {
            let here = at(&t, i, j);
            let cv = c(i - 1, j - 1);
            let diag = at(&t, i - 1, j - 1);
            if cv > 0.0 && (here - (diag + cv)).abs() < TIE {
                if let (Some(Tok::U(p)), Some(Tok::U(q))) = (ga.get(i - 1), gb.get(j - 1)) {
                    fw.entry(*p).or_default().insert(*q);
                    bw.entry(*q).or_default().insert(*p);
                }
                i -= 1;
                j -= 1;
            } else if (here - at(&t, i - 1, j)).abs() < TIE {
                i -= 1;
            } else {
                j -= 1;
            }
        }
    }
    one_to_one(&fw, &bw)
}

#[must_use]
pub fn tokens(side: &Side, f: usize, paired: &BTreeMap<usize, usize>, ours: bool) -> Vec<Tok> {
    let busy = |x: usize| side.callers.get(x).map_or(0, BTreeSet::len) > MAX_CALLEE_FANIN;
    side.feats.get(f).map_or_else(Vec::new, |ft| {
        ft.calls
            .iter()
            .filter_map(|c| match c {
                Callee::Import(n) => Some(Tok::I(n.clone())),
                Callee::Indirect => Some(Tok::X),
                Callee::Fn(x) => {
                    let p = if ours {
                        paired.contains_key(x).then_some(Tok::P(*x))
                    } else {
                        paired.get(x).map(|o| Tok::P(*o))
                    };
                    match p {
                        Some(t) => Some(t),
                        None if busy(*x) => None,
                        None => Some(Tok::U(*x)),
                    }
                }
            })
            .collect()
    })
}

fn name_of(side: &Side, i: usize) -> &str {
    side.funs.get(i).map_or("?", |f| f.name.as_str())
}

fn call_proposals(
    o: &Side,
    s: &Side,
    pairs: &Pairs,
    refused: &mut BTreeSet<(usize, usize)>,
) -> Props {
    let mut props = Props::new();
    let fit = |p: usize, q: usize| fits(o, s, p, q);
    for (a, b) in &pairs.o2s {
        let ta = tokens(o, *a, &pairs.o2s, true);
        let tb = tokens(s, *b, &pairs.s2o, false);
        let compat = |p: usize, q: usize| match (o.feats.get(p), s.feats.get(q)) {
            (Some(x), Some(y)) => context_compat(x, y),
            _ => 0.0,
        };
        let positional = gap_pairs(&ta, &tb, &fit, refused);
        let mut context = Vec::new();
        for x in gap_context_pairs(&ta, &tb, &compat) {
            if positional.contains(&x) {
                continue;
            }
            if fit(x.0, x.1) {
                context.push(x);
            } else {
                refused.insert(x);
            }
        }
        for (p, q) in positional.into_iter().chain(context) {
            if p == *a || q == *b {
                continue;
            }
            let e = props.entry((p, q)).or_insert((0.0, Vec::new()));
            e.0 += 1.0;
            if e.1.len() < 2 {
                e.1.push(format!("in {}", name_of(o, *a)));
            }
        }
    }
    props
}

fn caller_proposals(
    o: &Side,
    s: &Side,
    pairs: &Pairs,
    refused: &mut BTreeSet<(usize, usize)>,
) -> Props {
    let mut props = Props::new();
    for (a, b) in &pairs.o2s {
        let (Some(ca), Some(cb)) = (o.callers.get(*a), s.callers.get(*b)) else {
            continue;
        };
        let (pa, ua): (Vec<usize>, Vec<usize>) = ca.iter().partition(|x| pairs.o2s.contains_key(x));
        let (pb, ub): (Vec<usize>, Vec<usize>) = cb.iter().partition(|x| pairs.s2o.contains_key(x));
        let mapped: BTreeSet<usize> = pa
            .iter()
            .filter_map(|x| pairs.o2s.get(x).copied())
            .collect();
        if mapped != pb.iter().copied().collect::<BTreeSet<_>>() {
            continue;
        }
        if let ([p], [q]) = (ua.as_slice(), ub.as_slice()) {
            if !fits(o, s, *p, *q) {
                refused.insert((*p, *q));
                continue;
            }
            let e = props.entry((*p, *q)).or_insert((0.0, Vec::new()));
            e.0 += 1.0;
            if e.1.len() < 2 {
                e.1.push(format!("of {}", name_of(o, *a)));
            }
        }
    }
    props
}

fn vftable_proposals(
    o: &Side,
    s: &Side,
    pairs: &Pairs,
    st: &Structure,
    refused: &mut BTreeSet<(usize, usize)>,
) -> Props {
    let ta: Vec<Tok> = st
        .ours
        .iter()
        .map(|x| match x {
            Some(i) if pairs.o2s.contains_key(i) => Tok::P(*i),
            Some(i) => Tok::U(*i),
            None => Tok::X,
        })
        .collect();
    let tb: Vec<Tok> = st
        .sib
        .iter()
        .map(|y| match y {
            Some(j) => pairs.s2o.get(j).map_or(Tok::U(*j), |i| Tok::P(*i)),
            None => Tok::X,
        })
        .collect();
    let fit = |p: usize, q: usize| fits(o, s, p, q);
    let mut props = Props::new();
    for (p, q) in gap_pairs_upto(&ta, &tb, &fit, refused, MAX_SLOT_GAP) {
        let e = props.entry((p, q)).or_insert((0.0, Vec::new()));
        e.0 += 1.0;
        if e.1.is_empty() {
            e.1.push("slot gap".to_string());
        }
    }
    props
}

fn layout_proposals(
    o: &Side,
    s: &Side,
    pairs: &Pairs,
    refused: &mut BTreeSet<(usize, usize)>,
) -> Props {
    let mut oi: Vec<usize> = (0..o.funs.len()).collect();
    oi.sort_by_key(|i| o.funs.get(*i).map(|f| (f.image.clone(), f.rva)));
    let mut si: Vec<usize> = (0..s.funs.len()).collect();
    si.sort_by_key(|j| s.funs.get(*j).map_or(0, |f| f.rva));
    let ta: Vec<Tok> = oi
        .iter()
        .map(|i| {
            if pairs.o2s.contains_key(i) {
                Tok::P(*i)
            } else {
                Tok::U(*i)
            }
        })
        .collect();
    let tb: Vec<Tok> = si
        .iter()
        .map(|j| pairs.s2o.get(j).map_or(Tok::U(*j), |i| Tok::P(*i)))
        .collect();
    let fit = |p: usize, q: usize| fits(o, s, p, q);
    let mut props = Props::new();
    for (p, q) in gap_pairs(&ta, &tb, &fit, refused) {
        props.entry((p, q)).or_insert((0.0, Vec::new())).0 += 1.0;
    }
    let pos: HashMap<usize, usize> = oi.iter().enumerate().map(|(k, i)| (*i, k)).collect();
    for (k, v) in &mut props {
        let after = pos
            .get(&k.0)
            .and_then(|at| oi.get(..*at))
            .and_then(|prev| prev.iter().rev().find(|i| pairs.o2s.contains_key(i)))
            .and_then(|i| o.funs.get(*i))
            .map_or("the start", |f| f.name.as_str());
        v.1.push(format!("gap after {after}"));
    }
    props
}

#[must_use]
pub fn pair(o: &Side, s: &Side, seeds: &[(usize, usize)]) -> Pairs {
    pair_with(o, s, seeds, &Structure::default())
}

#[must_use]
pub fn pair_with(o: &Side, s: &Side, seeds: &[(usize, usize)], st: &Structure) -> Pairs {
    let sizes_o: Vec<usize> = o.feats.iter().map(|f| f.mnem.len()).collect();
    let sizes_s: Vec<usize> = s.feats.iter().map(|f| f.mnem.len()).collect();
    let sizes = (sizes_o.as_slice(), sizes_s.as_slice());
    let mut pairs = Pairs::default();
    for (a, b) in seeds {
        pairs.add(*a, *b, 0, "seed".to_string());
    }
    let cands = shape_candidates(o, s);
    let base = merged(anchor_votes(o, s), &shape_votes(&cands));
    accept(&base, &mut pairs, sizes, 0, "anchor");
    let mut refused = [
        BTreeSet::new(),
        BTreeSet::new(),
        BTreeSet::new(),
        BTreeSet::new(),
        BTreeSet::new(),
    ];
    for round in 1..=MAX_ROUNDS {
        let [call, caller, tie, slot, lay] = &mut refused;
        let a = accept(
            &call_proposals(o, s, &pairs, call),
            &mut pairs,
            sizes,
            round,
            "call",
        );
        let b = accept(
            &caller_proposals(o, s, &pairs, caller),
            &mut pairs,
            sizes,
            round,
            "caller",
        );
        let c = accept(&base, &mut pairs, sizes, round, "anchor");
        let d = accept(
            &neighbor_proposals(o, s, &pairs, &cands, tie),
            &mut pairs,
            sizes,
            round,
            "shape-tie",
        );
        let e = accept(
            &vftable_proposals(o, s, &pairs, st, slot),
            &mut pairs,
            sizes,
            round,
            "vftable",
        );
        let f = if st.layout {
            accept(
                &layout_proposals(o, s, &pairs, lay),
                &mut pairs,
                sizes,
                round,
                "layout",
            )
        } else {
            0
        };
        if a + b + c + d + e + f == 0 {
            break;
        }
    }
    let [call, caller, tie, slot, lay] = refused;
    pairs.refused_call = call;
    pairs.refused_caller = caller;
    pairs.refused_tie = tie;
    pairs.refused_slot = slot;
    pairs.refused_layout = lay;
    pairs
}

#[must_use]
pub fn shape(a: &[u16], b: &[u16]) -> f32 {
    let grams = |v: &[u16]| {
        let mut m: HashMap<(u16, u16, u16), usize> = HashMap::new();
        for w in v.windows(3) {
            if let [x, y, z] = w {
                let c = m.entry((*x, *y, *z)).or_insert(0);
                *c = c.saturating_add(1);
            }
        }
        m
    };
    let (ga, gb) = (grams(a), grams(b));
    let (na, nb): (usize, usize) = (ga.values().sum(), gb.values().sum());
    if na.saturating_add(nb) == 0 {
        return 0.0;
    }
    let common: usize = ga
        .iter()
        .map(|(k, v)| gb.get(k).map_or(0, |w| (*v).min(*w)))
        .sum();
    2.0 * real(common) / real(na.saturating_add(nb))
}
