use super::feat::{Callee, Feat, jaccard};
use crate::mangle::{self, Sig, SigLevel};
use crate::sibling::pair::{Tok, gap_pairs};
use std::collections::{BTreeMap, BTreeSet};

pub const MAX_FANOUT: usize = 3;

pub const MAX_CONST_FANOUT: usize = 2;

pub const MAX_SIZE_RATIO: f32 = 12.0;

pub const MAX_ROUNDS: usize = 40;

pub const MAX_ITEMS: usize = 6;

pub const MAX_PARENTS: usize = 3;

pub const TIE: f32 = 1e-6;

#[derive(Debug, Clone, Default)]
pub struct Fun {
    pub va: u64,
    pub end: u64,
    pub raw: String,
    pub sig: Option<Sig>,
    pub file: String,
}

#[derive(Debug, Default)]
pub struct Side {
    pub label: String,
    pub funs: Vec<Fun>,
    pub feats: Vec<Feat>,
    pub callers: Vec<BTreeSet<usize>>,
    pub msvc: bool,
}

impl Side {
    pub fn finish(&mut self) {
        self.callers = vec![BTreeSet::new(); self.funs.len()];
        for (i, f) in self.feats.iter().enumerate() {
            for c in &f.calls {
                if let Callee::Fn(t) = c
                    && *t != i
                    && let Some(s) = self.callers.get_mut(*t)
                {
                    s.insert(i);
                }
            }
        }
    }
}

#[derive(Debug, Default, Clone)]
pub struct Evidence {
    pub round: usize,
    pub how: String,
    pub votes: f32,
    pub items: Vec<String>,
    pub sig: SigLevel,
}

impl Evidence {
    #[must_use]
    pub fn kinds(&self) -> BTreeSet<&'static str> {
        let mut k = BTreeSet::new();
        for i in &self.items {
            for (p, tag) in [
                ("str:", "STR"),
                ("api:", "API"),
                ("flt:", "FLT"),
                ("int:", "INT"),
            ] {
                if i.starts_with(p) {
                    k.insert(tag);
                }
            }
            if i == "body" || i == "loose" {
                k.insert("BODY");
            }
        }
        match self.how.as_str() {
            "call" => {
                k.insert("CALL");
            }
            "caller" => {
                k.insert("CALLER");
            }
            "seed" => {
                k.insert("SEED");
            }
            _ => {}
        }
        k
    }
}

#[derive(Debug, Default, Clone)]
pub struct Pairs {
    pub a2b: BTreeMap<usize, usize>,
    pub b2a: BTreeMap<usize, usize>,
    pub why: BTreeMap<usize, Evidence>,
    pub held_twins: BTreeSet<(usize, usize)>,
}

impl Pairs {
    fn add(&mut self, a: usize, b: usize, ev: Evidence) {
        self.a2b.insert(a, b);
        self.b2a.insert(b, a);
        self.why.insert(a, ev);
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Rules {
    pub twins_need_two_links: bool,
}

impl Default for Rules {
    fn default() -> Self {
        Self {
            twins_need_two_links: true,
        }
    }
}

#[derive(Debug, Clone, Default)]
struct Prop {
    votes: f32,
    items: Vec<String>,
    parents: BTreeSet<usize>,
    twin: bool,
}

type Props = BTreeMap<(usize, usize), Prop>;

type AnchorKind<'a> = (&'a str, &'a dyn Fn(&Feat) -> Vec<String>, usize, f32);

fn stack_size(t: &str) -> u16 {
    if matches!(t, "double" | "long long" | "unsigned long long" | "__int64") {
        8
    } else {
        4
    }
}

fn param_bytes(s: &Sig) -> u16 {
    s.params
        .iter()
        .map(|p| stack_size(p))
        .fold(0u16, u16::saturating_add)
}

#[must_use]
pub fn code_check(s: &Sig, f: &Feat) -> SigLevel {
    if !s.params_known {
        return SigLevel::Unknown;
    }
    let has_this = s.has_this();
    if let (Some(a), Some(b)) = (has_this, f.this_ecx)
        && !a
        && b
    {
        return SigLevel::Mismatch;
    }
    if has_this == Some(true)
        && !s.varargs
        && let Some(pop) = f.ret_pop
    {
        return if pop == param_bytes(s) {
            SigLevel::Count
        } else {
            SigLevel::Mismatch
        };
    }
    if has_this == Some(false)
        && let Some(pop) = f.ret_pop
        && pop != 0
        && pop != param_bytes(s)
    {
        return SigLevel::Mismatch;
    }
    SigLevel::Unknown
}

#[must_use]
pub fn gate(a: &Side, ai: usize, b: &Side, bi: usize) -> SigLevel {
    let (Some(fa), Some(fb)) = (a.funs.get(ai), b.funs.get(bi)) else {
        return SigLevel::Mismatch;
    };
    let (Some(xa), Some(xb)) = (a.feats.get(ai), b.feats.get(bi)) else {
        return SigLevel::Mismatch;
    };
    match (&fa.sig, &fb.sig) {
        (Some(sa), Some(sb)) => {
            let this_b = if b.msvc {
                xb.this_ecx.filter(|t| *t)
            } else {
                None
            };
            let lvl = mangle::compare(sa, sb, this_b);
            if lvl == SigLevel::Mismatch || (b.msvc && code_check(sa, xb) == SigLevel::Mismatch) {
                return SigLevel::Mismatch;
            }
            lvl
        }
        (Some(sa), None) if b.msvc => code_check(sa, xb),
        (None, Some(sb)) if a.msvc => code_check(sb, xa),
        (None, None) if a.msvc && b.msvc => {
            if xa.this_ecx.is_some() && xb.this_ecx.is_some() && xa.this_ecx != xb.this_ecx {
                return SigLevel::Mismatch;
            }
            if let (Some(p), Some(q)) = (xa.ret_pop, xb.ret_pop)
                && p != q
            {
                return SigLevel::Mismatch;
            }
            SigLevel::Unknown
        }
        _ => SigLevel::Unknown,
    }
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

fn vote(votes: &mut Props, x: usize, y: usize, w: f32, why: String) {
    let e = votes.entry((x, y)).or_default();
    e.votes += w;
    if e.items.len() < MAX_ITEMS {
        e.items.push(why);
    }
}

fn apis(f: &Feat) -> Vec<String> {
    f.calls
        .iter()
        .filter_map(|c| match c {
            Callee::Api(n) => Some(n.clone()),
            _ => None,
        })
        .collect::<BTreeSet<_>>()
        .into_iter()
        .collect()
}

fn consts(f: &Feat, prefix: &str) -> Vec<String> {
    f.consts
        .iter()
        .filter(|k| k.starts_with(prefix))
        .cloned()
        .collect()
}

fn anchor_votes(a: &Side, b: &Side, use_bodies: bool) -> Props {
    let mut votes = Props::new();
    let strs = |f: &Feat| f.strs.clone();
    let flts = |f: &Feat| consts(f, "f:");
    let ints = |f: &Feat| consts(f, "i:");
    let kinds: [AnchorKind<'_>; 4] = [
        ("str", &strs, MAX_FANOUT, 1.0),
        ("api", &apis, MAX_CONST_FANOUT, 0.5),
        ("flt", &flts, MAX_CONST_FANOUT, 0.5),
        ("int", &ints, MAX_CONST_FANOUT, 0.34),
    ];
    for (tag, key, fan, w0) in kinds {
        let (ia, ib) = (index_of(a, key), index_of(b, key));
        for (k, xs) in &ia {
            let Some(ys) = ib.get(k) else { continue };
            if xs.len() > fan || ys.len() > fan {
                continue;
            }
            let w = if tag == "str" {
                (if k.len() >= 8 { 1.0 } else { 0.5 }) * w0
            } else {
                w0
            };
            let w = w / xs.len().saturating_mul(ys.len()) as f32;
            for x in xs {
                for y in ys {
                    vote(
                        &mut votes,
                        *x,
                        *y,
                        w,
                        format!("{tag}:{}", k.chars().take(40).collect::<String>()),
                    );
                }
            }
        }
    }
    if use_bodies {
        for (tag, w, get) in [
            ("body", 2.0f32, (|f: &Feat| f.body) as fn(&Feat) -> u64),
            ("loose", 1.5, |f: &Feat| f.loose),
        ] {
            let key = |f: &Feat| {
                if get(f) == 0 {
                    Vec::new()
                } else {
                    vec![format!("{:016x}", get(f))]
                }
            };
            let (ia, ib) = (index_of(a, &key), index_of(b, &key));
            for (k, xs) in &ia {
                if let (Some([y]), [x]) = (ib.get(k).map(Vec::as_slice), xs.as_slice()) {
                    vote(&mut votes, *x, *y, w, tag.to_string());
                }
            }
        }
    }
    votes
}

#[derive(Debug, Clone, Copy)]
struct Best {
    votes: f32,
    other: usize,
    tied: bool,
}

fn note(m: &mut BTreeMap<usize, Best>, k: usize, other: usize, v: f32) {
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

fn comparable(a: &Side, x: usize, b: &Side, y: usize) -> bool {
    let na = a.feats.get(x).map_or(1, |f| f.size.max(1));
    let nb = b.feats.get(y).map_or(1, |f| f.size.max(1));
    na.max(nb) as f32 / na.min(nb) as f32 <= MAX_SIZE_RATIO || na.max(nb) <= 40
}

fn accept(
    props: &Props,
    pairs: &mut Pairs,
    sides: (&Side, &Side),
    round: usize,
    how: &str,
    rules: Rules,
) -> usize {
    let (a, b) = sides;
    let mut best_a = BTreeMap::new();
    let mut best_b = BTreeMap::new();
    let mut gates: BTreeMap<(usize, usize), SigLevel> = BTreeMap::new();
    for (&(x, y), p) in props {
        if pairs.a2b.contains_key(&x) || pairs.b2a.contains_key(&y) {
            continue;
        }
        let g = gate(a, x, b, y);
        gates.insert((x, y), g);
        if g == SigLevel::Mismatch {
            continue;
        }
        note(&mut best_a, x, y, p.votes);
        note(&mut best_b, y, x, p.votes);
    }
    let mut landed = 0usize;
    for (&(x, y), p) in props {
        if p.votes < 1.0 - TIE {
            continue;
        }
        let g = gates.get(&(x, y)).copied().unwrap_or(SigLevel::Mismatch);
        if g == SigLevel::Mismatch {
            continue;
        }
        let (Some(bx), Some(by)) = (best_a.get(&x), best_b.get(&y)) else {
            continue;
        };
        if bx.other != y || by.other != x || bx.tied || by.tied {
            continue;
        }
        if !comparable(a, x, b, y) || pairs.a2b.contains_key(&x) || pairs.b2a.contains_key(&y) {
            continue;
        }
        if rules.twins_need_two_links && p.twin && p.parents.len() < 2 {
            pairs.held_twins.insert((x, y));
            continue;
        }
        pairs.add(
            x,
            y,
            Evidence {
                round,
                how: how.to_string(),
                votes: p.votes,
                items: p.items.clone(),
                sig: g,
            },
        );
        landed = landed.saturating_add(1);
    }
    landed
}

fn tokens(side: &Side, f: usize, paired: &BTreeMap<usize, usize>, named_side: bool) -> Vec<Tok> {
    side.feats.get(f).map_or_else(Vec::new, |ft| {
        ft.calls
            .iter()
            .map(|c| match c {
                Callee::Api(n) => Tok::I(n.clone()),
                Callee::Indirect => Tok::X,
                Callee::Fn(x) => {
                    if named_side {
                        if paired.contains_key(x) {
                            Tok::P(*x)
                        } else {
                            Tok::U(*x)
                        }
                    } else {
                        paired.get(x).map_or(Tok::U(*x), |o| Tok::P(*o))
                    }
                }
            })
            .collect()
    })
}

fn name_of(side: &Side, i: usize) -> String {
    side.funs.get(i).map_or_else(String::new, |f| {
        f.sig
            .as_ref()
            .map_or_else(|| format!("{:#x}", f.va), Sig::qual_name)
    })
}

#[must_use]
pub fn shape_key(s: &Sig) -> Option<String> {
    s.params_known.then(|| {
        let special = if s.name.starts_with('{') {
            s.name.as_str()
        } else {
            ""
        };
        format!(
            "{}|{:?}|{}|{}|{special}",
            s.params.join(","),
            s.has_this(),
            s.varargs,
            s.konst
        )
    })
}

#[must_use]
pub fn twins(a: &Side, b: &Side, proposed: &[(usize, usize)]) -> BTreeSet<(usize, usize)> {
    let key = |side: &Side, i: usize| {
        side.funs
            .get(i)
            .and_then(|f| f.sig.as_ref())
            .and_then(shape_key)
    };
    let mut out = BTreeSet::new();
    for (side, pick) in [
        (
            a,
            (|p: &(usize, usize)| p.0) as fn(&(usize, usize)) -> usize,
        ),
        (b, |p: &(usize, usize)| p.1),
    ] {
        let mut by: BTreeMap<String, Vec<(usize, usize)>> = BTreeMap::new();
        for p in proposed {
            if let Some(k) = key(side, pick(p)) {
                by.entry(k).or_default().push(*p);
            }
        }
        out.extend(by.into_values().filter(|v| v.len() > 1).flatten());
    }
    out
}

fn call_proposals(a: &Side, b: &Side, pairs: &Pairs) -> Props {
    let mut props = Props::new();
    for (x, y) in &pairs.a2b {
        let ta = tokens(a, *x, &pairs.a2b, true);
        let tb = tokens(b, *y, &pairs.b2a, false);
        let fits = |p: usize, q: usize| gate(a, p, b, q) != SigLevel::Mismatch;
        let mut refused = BTreeSet::new();
        let proposed: Vec<(usize, usize)> = gap_pairs(&ta, &tb, &fits, &mut refused)
            .into_iter()
            .filter(|(p, q)| p != x && q != y)
            .collect();
        let ambiguous = twins(a, b, &proposed);
        for (p, q) in proposed {
            let e = props.entry((p, q)).or_default();
            e.votes += 1.0;
            e.parents.insert(*x);
            e.twin |= ambiguous.contains(&(p, q));
            if e.items.len() < MAX_PARENTS {
                e.items.push(format!("in {}", name_of(a, *x)));
            }
        }
    }
    props
}

fn caller_proposals(a: &Side, b: &Side, pairs: &Pairs) -> Props {
    let mut props = Props::new();
    for (x, y) in &pairs.a2b {
        let (Some(ca), Some(cb)) = (a.callers.get(*x), b.callers.get(*y)) else {
            continue;
        };
        let (pa, ua): (Vec<usize>, Vec<usize>) = ca.iter().partition(|c| pairs.a2b.contains_key(c));
        let (pb, ub): (Vec<usize>, Vec<usize>) = cb.iter().partition(|c| pairs.b2a.contains_key(c));
        let mapped: BTreeSet<usize> = pa
            .iter()
            .filter_map(|c| pairs.a2b.get(c).copied())
            .collect();
        if mapped != pb.iter().copied().collect::<BTreeSet<_>>() {
            continue;
        }
        if let ([p], [q]) = (ua.as_slice(), ub.as_slice()) {
            let e = props.entry((*p, *q)).or_default();
            e.votes += 1.0;
            e.parents.insert(*x);
            if e.items.len() < MAX_PARENTS {
                e.items.push(format!("of {}", name_of(a, *x)));
            }
        }
    }
    props
}

#[must_use]
pub fn pair(a: &Side, b: &Side, seeds: &[(usize, usize)], propagate: bool) -> Pairs {
    pair_with(a, b, seeds, propagate, Rules::default())
}

#[must_use]
pub fn pair_with(
    a: &Side,
    b: &Side,
    seeds: &[(usize, usize)],
    propagate: bool,
    rules: Rules,
) -> Pairs {
    let mut pairs = Pairs::default();
    for (x, y) in seeds {
        pairs.add(
            *x,
            *y,
            Evidence {
                how: "seed".to_string(),
                votes: 1.0,
                sig: gate(a, *x, b, *y),
                ..Evidence::default()
            },
        );
    }
    let sides = (a, b);
    let base = anchor_votes(a, b, a.msvc && b.msvc);
    accept(&base, &mut pairs, sides, 0, "anchor", rules);
    for round in 1..=MAX_ROUNDS {
        let n = if propagate {
            let n1 = accept(
                &call_proposals(a, b, &pairs),
                &mut pairs,
                sides,
                round,
                "call",
                rules,
            );
            let n2 = accept(
                &caller_proposals(a, b, &pairs),
                &mut pairs,
                sides,
                round,
                "caller",
                rules,
            );
            n1.saturating_add(n2)
        } else {
            0
        };
        let n3 = accept(&base, &mut pairs, sides, round, "anchor", rules);
        if n.saturating_add(n3) == 0 {
            break;
        }
    }
    pairs
}

#[must_use]
pub fn konst(a: &Feat, b: &Feat) -> (f32, f32) {
    (jaccard(&a.disps, &b.disps), jaccard(&a.consts, &b.consts))
}
