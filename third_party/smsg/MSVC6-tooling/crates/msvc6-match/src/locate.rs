use anyhow::{Context as _, Result, bail};
use msvc6_image::archive::{Archive, MemberKind};
use msvc6_image::coff::Object;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::code::{self, Code, SectionSymbol};
use crate::place::{
    ALIGN_SLACK, Placement, Taken, Text, neighbour_bounds, order_assignments, place, target_at,
    witnessed_starts,
};

pub const PAIR_BUDGET: u64 = 1 << 24;

#[derive(Debug, Clone, Copy)]
pub struct Lib<'a> {
    pub name: &'a str,
    pub archive: &'a Archive<'a>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Recorded {
    pub rva: u32,
    pub len: u32,
    pub member: String,
    pub archive: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SectionAt {
    pub rva: u32,
    pub len: u32,
    pub index: u16,
    pub symbol: Option<String>,
    #[serde(skip)]
    pub symbols: Vec<SectionSymbol>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Refused {
    pub lo: u32,
    pub hi: u32,
    pub owners: Vec<String>,
}

#[derive(Debug, Clone, Default)]
pub struct Placed {
    pub index: usize,
    pub member: String,
    pub archive: String,
    pub comp_id: Option<u32>,
    pub code_bytes: u64,
    pub comdats: usize,
    pub unique: usize,
    pub ambiguous: usize,
    pub resolved_by_order: usize,
    pub by_witness: usize,
    pub absent: usize,
    pub unidentifiable: usize,
    pub exhausted: usize,
    pub imports: Vec<(String, u32)>,
    pub lo: Option<u32>,
    pub hi: Option<u32>,
    pub bytes: u32,
    pub contiguous: bool,
    pub tied_with: Vec<String>,
    pub witness: i64,
    pub recorded: bool,
    pub tie_break: Vec<String>,
    pub decided_with: Vec<String>,
    pub refs: Vec<(u32, String, u32, bool)>,
    pub outvoted: Vec<String>,
    pub refused: Vec<Refused>,
    pub symbols: Vec<String>,
    pub ranges: Vec<(u32, u32)>,
    pub leftover: Vec<Code>,
    pub unidentified: Vec<Code>,
    pub defined_at: Vec<(String, u32)>,
    pub statics_at: Vec<(String, u32)>,
    pub sections: Vec<SectionAt>,
    pub section_symbols: BTreeMap<u16, Vec<SectionSymbol>>,
    pub coverage: u32,
}

impl Placed {
    #[must_use]
    pub fn present(&self) -> bool {
        self.unique > 0
    }

    #[must_use]
    pub fn stem(&self) -> &str {
        self.member
            .rsplit(['\\', '/'])
            .next()
            .unwrap_or(&self.member)
    }

    #[must_use]
    pub fn undecided(&self) -> Vec<&str> {
        self.tied_with
            .iter()
            .filter(|t| !self.decided_with.contains(t))
            .map(String::as_str)
            .collect()
    }

    #[must_use]
    pub fn kept(&self) -> u32 {
        self.ranges
            .iter()
            .map(|(a, b)| b.saturating_sub(*a))
            .fold(0, u32::saturating_add)
    }

    fn add_range(&mut self, rva: u32, len: u32) {
        let end = rva.saturating_add(len);
        self.ranges.push((rva, end));
        self.bytes = self.bytes.saturating_add(len);
        self.lo = Some(self.lo.map_or(rva, |v| v.min(rva)));
        self.hi = Some(self.hi.map_or(end, |v| v.max(end)));
    }

    fn define(&mut self, c: &Code, rva: u32) {
        let syms = self
            .section_symbols
            .get(&c.index)
            .cloned()
            .unwrap_or_default();
        if let (Some(s), true) = (&c.symbol, c.external) {
            self.symbols.push(s.clone());
            if syms.is_empty() {
                self.defined_at.push((s.clone(), rva));
            }
        }
        for s in &syms {
            let at = rva.saturating_add(s.offset);
            if s.external {
                self.defined_at.push((s.name.clone(), at));
            } else {
                self.statics_at.push((s.name.clone(), at));
            }
        }
        self.sections.push(SectionAt {
            rva,
            len: c.len32(),
            index: c.index,
            symbol: c.symbol.clone(),
            symbols: syms,
        });
    }

    fn read_refs(&mut self, c: &Code, rva: u32, t: &Text<'_>) {
        for r in c.relocations.iter().filter(|r| !r.symbol.is_empty()) {
            if let Some(a) = target_at(t, rva, c, r) {
                self.refs.push((rva, r.symbol.clone(), a, r.external));
            }
        }
    }

    fn take(&mut self, c: &Code, rva: u32, t: &Text<'_>) {
        self.add_range(rva, c.len32());
        self.define(c, rva);
        self.read_refs(c, rva, t);
    }

    fn contiguous_ranges(&self) -> bool {
        self.ranges.len() > 1
            && self.ranges.windows(2).all(|w| match (w.first(), w.get(1)) {
                (Some((_, a_end)), Some((b, _))) => {
                    b >= a_end && b.saturating_sub(*a_end) < ALIGN_SLACK
                }
                _ => false,
            })
    }
}

#[derive(Debug, Clone, Default)]
pub struct Run {
    pub members: Vec<Placed>,
    pub considered: usize,
}

impl Run {
    #[must_use]
    pub fn present(&self) -> Vec<&Placed> {
        let mut v: Vec<&Placed> = self
            .members
            .iter()
            .filter(|p| !p.ranges.is_empty())
            .collect();
        v.sort_by(|a, b| {
            a.lo.unwrap_or(u32::MAX)
                .cmp(&b.lo.unwrap_or(u32::MAX))
                .then_with(|| a.member.cmp(&b.member))
        });
        v
    }

    #[must_use]
    pub fn unmatched(&self) -> Vec<&Placed> {
        self.members
            .iter()
            .filter(|p| p.ranges.is_empty())
            .collect()
    }

    #[must_use]
    pub fn covered(&self) -> u32 {
        let mut r: Vec<(u32, u32)> = self
            .members
            .iter()
            .filter(|p| p.present())
            .flat_map(|p| p.ranges.iter().copied())
            .collect();
        r.sort_unstable();
        let (mut bytes, mut end) = (0u32, 0u32);
        for (a, b) in r {
            let from = a.max(end);
            if b > from {
                bytes = bytes.saturating_add(b.saturating_sub(from));
            }
            end = end.max(b);
        }
        bytes
    }
}

fn place_member(name: &str, archive: &str, o: &Object<'_>, t: &Text<'_>) -> Result<Placed> {
    let codes = code::sections(o)?;
    let defined: BTreeSet<String> = codes.iter().filter_map(|c| c.symbol.clone()).collect();
    let mut p = Placed {
        member: name.to_string(),
        archive: archive.to_string(),
        comp_id: o.comp_id(),
        code_bytes: o.code_bytes(),
        comdats: codes.len(),
        section_symbols: code::section_symbols(o),
        ..Placed::default()
    };
    let mut hits: Vec<(u16, u32, u32)> = Vec::new();
    let mut ambiguous: Vec<Code> = Vec::new();
    for c in &codes {
        match place(c, t.bytes) {
            Placement::Unique(off) => {
                p.unique = p.unique.saturating_add(1);
                let rva = t.rva_of(off);
                hits.push((c.index, rva, c.len32()));
                p.take(c, rva, t);
                for r in &c.relocations {
                    if !r.external || r.symbol.is_empty() || defined.contains(&r.symbol) {
                        continue;
                    }
                    if let Some(a) = target_at(t, rva, c, r) {
                        p.imports.push((r.symbol.clone(), a));
                    }
                }
            }
            Placement::Ambiguous(_) => {
                p.ambiguous = p.ambiguous.saturating_add(1);
                ambiguous.push(c.clone());
            }
            Placement::Absent => p.absent = p.absent.saturating_add(1),
            Placement::Exhausted => {
                p.exhausted = p.exhausted.saturating_add(1);
                p.unidentifiable = p.unidentifiable.saturating_add(1);
                p.unidentified.push(c.clone());
            }
            Placement::Unidentifiable => {
                p.unidentifiable = p.unidentifiable.saturating_add(1);
                p.unidentified.push(c.clone());
            }
        }
    }
    if !ambiguous.is_empty() {
        let mut placed: Vec<(u32, u32)> = hits.iter().map(|(_, r, l)| (*r, *l)).collect();
        placed.sort_unstable();
        let monotonic = placed.windows(2).all(|w| match (w.first(), w.get(1)) {
            (Some((ar, al)), Some((br, _))) => *br >= ar.saturating_add(*al),
            _ => true,
        });
        let assigned = if monotonic && !placed.is_empty() {
            order_assignments(&placed, &ambiguous, 0, 0, t)
        } else {
            None
        };
        match assigned {
            None => p.leftover.extend(ambiguous.iter().cloned()),
            Some(assigned) => {
                let took: BTreeSet<usize> = assigned.iter().map(|(i, _, _)| *i).collect();
                for (i, c) in ambiguous.iter().enumerate() {
                    if !took.contains(&i) {
                        p.leftover.push(c.clone());
                    }
                }
                for (i, rva, len) in assigned {
                    let Some(c) = ambiguous.get(i) else { continue };
                    p.resolved_by_order = p.resolved_by_order.saturating_add(1);
                    p.ambiguous = p.ambiguous.saturating_sub(1);
                    p.unique = p.unique.saturating_add(1);
                    hits.push((c.index, rva, len));
                    p.take(c, rva, t);
                }
            }
        }
    }
    hits.sort_by_key(|(i, _, _)| *i);
    p.contiguous = hits.len() > 1
        && hits.windows(2).all(|w| {
            let (Some((_, ar, al)), Some((_, br, _))) = (w.first(), w.get(1)) else {
                return false;
            };
            let end = ar.saturating_add(*al);
            *br >= end && br.saturating_sub(end) < ALIGN_SLACK
        });
    Ok(p)
}

pub fn objects<'a>(a: &'a Archive<'a>) -> impl Iterator<Item = (usize, &'a str, &'a [u8])> {
    a.content()
        .filter(|(m, _)| m.kind == MemberKind::Object)
        .map(|(m, d)| (m.index, m.name.as_str(), d))
}

fn place_archive(lib: &Lib<'_>, t: &Text<'_>, jobs: usize) -> Result<Vec<Placed>> {
    let work: Vec<(usize, &str, &[u8])> = objects(lib.archive).collect();
    let one = |(index, name, d): (usize, &str, &[u8])| -> Result<Placed> {
        let o = Object::parse(d).with_context(|| format!("{}: member {name}", lib.name))?;
        let mut p = place_member(name, lib.name, &o, t)
            .with_context(|| format!("{}: member {name}", lib.name))?;
        p.index = index;
        Ok(p)
    };
    let jobs = jobs.clamp(1, 64);
    if jobs == 1 || work.len() < 2 {
        return work.into_iter().map(one).collect();
    }
    let chunk = work.len().div_ceil(jobs);
    let parts: Vec<Result<Vec<Placed>>> = std::thread::scope(|s| {
        let handles: Vec<_> = work
            .chunks(chunk)
            .map(|c| s.spawn(move || c.iter().map(|w| one(*w)).collect::<Result<Vec<_>>>()))
            .collect();
        handles
            .into_iter()
            .map(|h| {
                h.join()
                    .unwrap_or_else(|_| Err(anyhow::anyhow!("a placement thread panicked")))
            })
            .collect()
    });
    let mut out = Vec::with_capacity(work.len());
    for p in parts {
        out.extend(p?);
    }
    Ok(out)
}

fn neighbour_pass(all: &mut [Placed], t: &Text<'_>) {
    let mut idx: Vec<usize> = (0..all.len())
        .filter(|i| all.get(*i).is_some_and(|p| p.lo.is_some()))
        .collect();
    idx.sort_by_key(|i| all.get(*i).and_then(|p| p.lo));
    let spans: Vec<(u32, u32)> = idx
        .iter()
        .filter_map(|&j| all.get(j))
        .map(|q| (q.lo.unwrap_or(0), q.hi.unwrap_or(0)))
        .collect();
    let bounds: Vec<(usize, u32, u32)> = idx
        .iter()
        .enumerate()
        .filter_map(|(k, &i)| {
            let me = all.get(i)?;
            if me.leftover.is_empty() {
                return None;
            }
            let (floor, ceil) = neighbour_bounds(&spans, k);
            Some((i, floor, ceil))
        })
        .collect();
    for (i, floor, ceil) in bounds {
        let Some(me) = all.get(i) else { continue };
        let placed: Vec<(u32, u32)> = me
            .ranges
            .iter()
            .map(|(a, b)| (*a, b.saturating_sub(*a)))
            .collect();
        let left = me.leftover.clone();
        if placed.is_empty() || left.is_empty() {
            continue;
        }
        let Some(assigned) = order_assignments(&placed, &left, floor, ceil, t) else {
            continue;
        };
        let Some(me) = all.get_mut(i) else { continue };
        for (j, rva, _) in assigned {
            let Some(c) = left.get(j) else { continue };
            me.resolved_by_order = me.resolved_by_order.saturating_add(1);
            me.ambiguous = me.ambiguous.saturating_sub(1);
            me.unique = me.unique.saturating_add(1);
            me.take(c, rva, t);
        }
        me.ranges.sort_unstable();
        me.leftover.clear();
    }
}

pub type Attestations = BTreeMap<String, BTreeMap<u32, BTreeSet<usize>>>;

#[must_use]
pub fn credible(all: &[Placed]) -> Vec<bool> {
    let mut spans: Vec<(u32, u32, usize)> = all
        .iter()
        .enumerate()
        .flat_map(|(i, p)| p.ranges.iter().map(move |(a, b)| (*a, *b, i)))
        .collect();
    spans.sort_unstable();
    let n = spans.len();
    let mut next_diff: Vec<Option<usize>> = vec![None; n];
    for k in (0..n).rev() {
        let here = spans.get(k).map(|s| s.2);
        let after = spans.get(k.saturating_add(1)).map(|s| s.2);
        let v = match (here, after) {
            (Some(h), Some(a)) if h != a => Some(k.saturating_add(1)),
            (Some(_), Some(_)) => next_diff.get(k.saturating_add(1)).copied().flatten(),
            _ => None,
        };
        if let Some(slot) = next_diff.get_mut(k) {
            *slot = v;
        }
    }
    let mut out = vec![false; all.len()];
    let mut best: Option<(u32, usize)> = None;
    let mut second: Option<u32> = None;
    for (k, &(a, b, i)) in spans.iter().enumerate() {
        let before = match best {
            Some((e, m)) if m != i => e > a,
            Some(_) => second.is_some_and(|e| e > a),
            None => false,
        };
        let after = next_diff
            .get(k)
            .copied()
            .flatten()
            .and_then(|m| spans.get(m))
            .is_some_and(|s| s.0 < b);
        if !before
            && !after
            && let Some(o) = out.get_mut(i)
        {
            *o = true;
        }
        match best {
            None => best = Some((b, i)),
            Some((e, m)) if m == i => {
                if b > e {
                    best = Some((b, i));
                }
            }
            Some((e, _)) => {
                if b > e {
                    second = Some(e);
                    best = Some((b, i));
                } else if second.is_none_or(|s| b > s) {
                    second = Some(b);
                }
            }
        }
    }
    out
}

#[must_use]
pub fn attestations(all: &[Placed], credible: &[bool]) -> Attestations {
    let mut out = Attestations::new();
    for (i, p) in all.iter().enumerate() {
        if credible.get(i) != Some(&true) {
            continue;
        }
        let refs = p.refs.iter().filter(|r| r.3).map(|(_, n, a, _)| (n, a));
        for (n, a) in p
            .defined_at
            .iter()
            .chain(&p.imports)
            .map(|(n, a)| (n, a))
            .chain(refs)
        {
            out.entry(n.clone())
                .or_default()
                .entry(*a)
                .or_default()
                .insert(i);
        }
    }
    out
}

fn place_by_witness(all: &mut [Placed], t: &Text<'_>) {
    loop {
        let attest = attestations(all, &credible(all));
        let known: BTreeMap<String, u32> = attest
            .iter()
            .filter_map(|(n, at)| {
                let mut k = at.keys();
                match (k.next(), k.next()) {
                    (Some(a), None) => Some((n.clone(), *a)),
                    _ => None,
                }
            })
            .collect();
        let taken = Taken::of(all.iter().flat_map(|p| p.ranges.iter().copied()));
        let mut progress = false;
        for p in all.iter_mut() {
            let mut keep = Vec::new();
            for c in std::mem::take(&mut p.unidentified) {
                let starts = witnessed_starts(&c, t, &known, &taken);
                let [rva] = starts.as_slice() else {
                    keep.push(c);
                    continue;
                };
                let rva = *rva;
                p.unidentifiable = p.unidentifiable.saturating_sub(1);
                p.unique = p.unique.saturating_add(1);
                p.by_witness = p.by_witness.saturating_add(1);
                p.take(&c, rva, t);
                p.ranges.sort_unstable();
                progress = true;
                p.contiguous = p.contiguous_ranges();
            }
            p.unidentified = keep;
        }
        if !progress {
            return;
        }
    }
}

fn witness_score(all: &[Placed], i: usize, attest: &Attestations) -> i64 {
    let Some(p) = all.get(i) else { return 0 };
    let claims: BTreeSet<(&str, u32)> = p
        .defined_at
        .iter()
        .chain(&p.imports)
        .map(|(n, a)| (n.as_str(), *a))
        .collect();
    let by_other = |w: &BTreeSet<usize>| w.iter().any(|&j| j != i);
    let mut score = 0i64;
    for (n, a) in claims {
        let Some(at) = attest.get(n) else { continue };
        if at.get(&a).is_some_and(by_other) {
            score = score.saturating_add(1);
        } else if at.iter().any(|(x, w)| *x != a && by_other(w)) {
            score = score.saturating_sub(1);
        }
    }
    score
}

fn named_by_record(p: &Placed, record: &[Recorded]) -> bool {
    record.iter().any(|m| {
        m.member.eq_ignore_ascii_case(&p.member)
            && m.archive.eq_ignore_ascii_case(&p.archive)
            && p.ranges
                .iter()
                .any(|(a, b)| *a < m.rva.saturating_add(m.len) && m.rva < *b)
    })
}

type AttestedAt<'a> = BTreeMap<u32, Vec<(&'a str, &'a BTreeSet<usize>)>>;

fn attested_at(attest: &Attestations) -> AttestedAt<'_> {
    let mut out = AttestedAt::new();
    for (n, at) in attest {
        for (a, w) in at {
            out.entry(*a).or_default().push((n.as_str(), w));
        }
    }
    out
}

fn range_evidence(
    all: &[Placed],
    i: usize,
    (lo, hi): (u32, u32),
    attest: &Attestations,
    named_at: &AttestedAt<'_>,
) -> i64 {
    let Some(p) = all.get(i) else { return 0 };
    let by_other = |w: &BTreeSet<usize>| w.iter().any(|&j| j != i);
    let judge = |names: &[&str], a: u32, call: bool| -> i64 {
        let here = |n: &&str| {
            attest
                .get(*n)
                .and_then(|at| at.get(&a))
                .is_some_and(by_other)
        };
        let elsewhere = |n: &&str| {
            attest
                .get(*n)
                .is_some_and(|at| at.iter().any(|(x, w)| *x != a && by_other(w)))
        };
        let other_name = call
            && named_at
                .get(&a)
                .is_some_and(|v| v.iter().any(|(m, w)| !names.contains(m) && by_other(w)));
        if names.iter().any(here) {
            1
        } else if names.iter().any(elsewhere) || other_name {
            -1
        } else {
            0
        }
    };
    let inside = |a: u32| a >= lo && a < hi;
    let mut defs: BTreeMap<u32, Vec<&str>> = BTreeMap::new();
    for (n, a) in p.defined_at.iter().filter(|(_, a)| inside(*a)) {
        let v = defs.entry(*a).or_default();
        if !v.contains(&n.as_str()) {
            v.push(n.as_str());
        }
    }
    let mut score: i64 = defs
        .iter()
        .map(|(a, names)| judge(names, *a, false))
        .fold(0, i64::saturating_add);
    let refs: BTreeSet<(&str, u32, bool)> = p
        .refs
        .iter()
        .filter(|r| inside(r.0))
        .map(|(_, n, t, e)| (n.as_str(), *t, *e))
        .collect();
    for (n, t, external) in refs {
        if external {
            score = score.saturating_add(judge(&[n], t, true));
        } else {
            let own: Vec<u32> = p
                .statics_at
                .iter()
                .filter(|(s, _)| s == n)
                .map(|(_, a)| *a)
                .collect();
            if !own.is_empty() && !own.contains(&t) {
                score = score.saturating_sub(1);
            }
        }
    }
    score
}

struct Pairs {
    budget: u64,
}

impl Pairs {
    fn charge(&mut self, n: usize) -> Result<()> {
        let n = u64::try_from(n).unwrap_or(u64::MAX);
        match self.budget.checked_sub(n) {
            Some(b) => {
                self.budget = b;
                Ok(())
            }
            None => bail!(
                "the placed ranges overlap past {PAIR_BUDGET} pair checks; no MSVC6 archive does this"
            ),
        }
    }

    fn overlapping(&mut self, all: &[Placed]) -> Result<Vec<(usize, usize, usize, usize)>> {
        let mut spans: Vec<(u32, u32, usize, usize)> = all
            .iter()
            .enumerate()
            .flat_map(|(i, p)| {
                p.ranges
                    .iter()
                    .enumerate()
                    .map(move |(k, (a, b))| (*a, *b, i, k))
            })
            .collect();
        spans.sort_unstable();
        let mut out = Vec::new();
        let mut active: Vec<(u32, u32, usize, usize)> = Vec::new();
        for &(a, b, i, k) in &spans {
            active.retain(|s| s.1 > a);
            self.charge(active.len())?;
            for &(x, y, j, l) in &active {
                if j != i && x < b && a < y {
                    out.push((i, k, j, l));
                    out.push((j, l, i, k));
                }
            }
            active.push((a, b, i, k));
        }
        Ok(out)
    }
}

fn outvote(
    all: &mut [Placed],
    attest: &Attestations,
    pairs: &mut Pairs,
) -> Result<Vec<(usize, usize, String)>> {
    let named_at = attested_at(attest);
    let ev: Vec<Vec<i64>> = all
        .iter()
        .enumerate()
        .map(|(i, p)| {
            p.ranges
                .iter()
                .map(|r| range_evidence(all, i, *r, attest, &named_at))
                .collect()
        })
        .collect();
    let at = |i: usize, k: usize| ev.get(i).and_then(|v| v.get(k)).copied().unwrap_or(0);
    let mut best: BTreeMap<(usize, usize), (i64, usize)> = BTreeMap::new();
    for (i, k, j, l) in pairs.overlapping(all)? {
        let cand = (at(j, l), j);
        let e = best.entry((i, k)).or_insert(cand);
        if cand > *e {
            *e = cand;
        }
    }
    type Loss = (usize, usize, (u32, u32), i64, i64);
    let mut lose: Vec<Loss> = Vec::new();
    for (i, p) in all.iter().enumerate() {
        for (k, &(a, b)) in p.ranges.iter().enumerate() {
            let mine = at(i, k);
            if let Some(&(theirs, j)) = best.get(&(i, k))
                && theirs > 0
                && mine < 0
            {
                lose.push((i, j, (a, b), theirs, mine));
            }
        }
    }
    let mut decided = Vec::new();
    for (i, j, (a, b), theirs, mine) in lose {
        let name = |k: usize| all.get(k).map_or_else(String::new, |q| q.member.clone());
        let (loser, winner) = (name(i), name(j));
        if let Some(p) = all.get_mut(i) {
            p.ranges.retain(|x| *x != (a, b));
            p.outvoted.push(format!(
                "lost {a:#x}..{b:#x} to {winner} ({theirs:+} against {mine:+})"
            ));
        }
        if let Some(q) = all.get_mut(j) {
            q.outvoted.push(format!(
                "took {a:#x}..{b:#x} from {loser} ({theirs:+} against {mine:+})"
            ));
        }
        decided.push((
            i,
            j,
            format!(
                "EVIDENCE {winner} took {a:#x}..{b:#x} from {loser} ({theirs:+} against {mine:+})"
            ),
        ));
    }
    Ok(decided)
}

fn member_pairs(all: &[Placed], pairs: &mut Pairs) -> Result<BTreeSet<(usize, usize)>> {
    Ok(pairs
        .overlapping(all)?
        .into_iter()
        .map(|(i, _, j, _)| (i.min(j), i.max(j)))
        .collect())
}

fn keep_one_owner(all: &mut [Placed], record: &[Recorded], pairs: &mut Pairs) -> Result<()> {
    for p in all.iter_mut() {
        p.coverage = p.kept();
    }
    let attest = attestations(all, &credible(all));
    let evidence = outvote(all, &attest, pairs)?;
    let scores: Vec<i64> = (0..all.len())
        .map(|i| witness_score(all, i, &attest))
        .collect();
    for (p, s) in all.iter_mut().zip(&scores) {
        p.witness = *s;
    }
    for p in all.iter_mut() {
        p.recorded = named_by_record(p, record);
    }
    let mut notes: Vec<(usize, usize, String)> = Vec::new();
    for (i, j) in member_pairs(all, pairs)? {
        let (Some(a), Some(b)) = (all.get(i), all.get(j)) else {
            continue;
        };
        if a.coverage != b.coverage {
            continue;
        }
        let why = if a.witness != b.witness {
            Some(format!(
                "WITNESS {} {:+} against {} {:+}",
                a.member, a.witness, b.member, b.witness
            ))
        } else if a.recorded != b.recorded {
            let (w, l) = if a.recorded { (a, b) } else { (b, a) };
            Some(format!("RECORD names {} over {}", w.member, l.member))
        } else {
            None
        };
        if let Some(w) = why {
            notes.push((i, j, w.clone()));
            notes.push((j, i, w));
        }
    }
    for (i, j, w) in evidence {
        let tied = match (all.get(i), all.get(j)) {
            (Some(a), Some(b)) => a.tied_with.contains(&b.member),
            _ => false,
        };
        if tied {
            notes.push((i, j, w.clone()));
            notes.push((j, i, w));
        }
    }
    let names: Vec<String> = all.iter().map(|p| p.member.clone()).collect();
    for (i, j, w) in notes {
        if let Some(p) = all.get_mut(i) {
            p.tie_break.push(w);
            if let Some(n) = names.get(j)
                && !p.decided_with.contains(n)
            {
                p.decided_with.push(n.clone());
            }
        }
    }
    all.sort_by(|a, b| {
        b.coverage
            .cmp(&a.coverage)
            .then(b.witness.cmp(&a.witness))
            .then(b.recorded.cmp(&a.recorded))
            .then_with(|| a.archive.cmp(&b.archive))
            .then_with(|| a.member.cmp(&b.member))
    });
    let mut taken: BTreeMap<u32, (u32, String)> = BTreeMap::new();
    for p in all.iter_mut() {
        let mut refused = Vec::new();
        let member = p.member.clone();
        p.ranges.retain(|&(a, b)| {
            let mut owners: Vec<String> = taken
                .range(..b.max(a))
                .rev()
                .take_while(|(_, (e, _))| *e > a)
                .map(|(_, (_, o))| o.clone())
                .collect();
            if owners.is_empty() {
                taken.insert(a, (b, member.clone()));
                return true;
            }
            owners.sort();
            owners.dedup();
            refused.push(Refused {
                lo: a,
                hi: b,
                owners,
            });
            false
        });
        p.refused = refused;
    }
    Ok(())
}

fn mark_ties(all: &mut [Placed], pairs: &mut Pairs) -> Result<()> {
    let names: Vec<String> = all.iter().map(|p| p.member.clone()).collect();
    let cov: Vec<u32> = all.iter().map(Placed::kept).collect();
    for (i, j) in member_pairs(all, pairs)? {
        if cov.get(i) != cov.get(j) {
            continue;
        }
        let (Some(ni), Some(nj)) = (names.get(i), names.get(j)) else {
            continue;
        };
        if let Some(p) = all.get_mut(i) {
            p.tied_with.push(nj.clone());
        }
        if let Some(p) = all.get_mut(j) {
            p.tied_with.push(ni.clone());
        }
    }
    Ok(())
}

pub fn locate(t: &Text<'_>, libs: &[Lib<'_>], record: &[Recorded], jobs: usize) -> Result<Run> {
    let mut all: Vec<Placed> = Vec::new();
    for lib in libs {
        all.extend(place_archive(lib, t, jobs)?);
    }
    let considered = all.len();
    neighbour_pass(&mut all, t);
    place_by_witness(&mut all, t);
    let mut pairs = Pairs {
        budget: PAIR_BUDGET,
    };
    mark_ties(&mut all, &mut pairs)?;
    keep_one_owner(&mut all, record, &mut pairs)?;
    Ok(Run {
        members: all,
        considered,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn at(member: &str, ranges: &[(u32, u32)]) -> Placed {
        Placed {
            member: member.into(),
            archive: "libc".into(),
            ranges: ranges.to_vec(),
            unique: ranges.len(),
            ..Placed::default()
        }
    }

    fn naive(all: &[Placed]) -> Vec<bool> {
        let mut spans: Vec<(u32, u32, usize)> = all
            .iter()
            .enumerate()
            .flat_map(|(i, p)| p.ranges.iter().map(move |(a, b)| (*a, *b, i)))
            .collect();
        spans.sort_unstable();
        let mut out = vec![false; all.len()];
        for (k, &(a, b, i)) in spans.iter().enumerate() {
            let before = spans.iter().take(k).any(|&(_, e, j)| j != i && e > a);
            let after = spans
                .iter()
                .skip(k + 1)
                .take_while(|s| s.0 < b)
                .any(|s| s.2 != i);
            if !before && !after {
                out[i] = true;
            }
        }
        out
    }

    #[test]
    fn credibility_needs_one_range_no_other_member_overlaps() {
        let all = [
            at("a.obj", &[(0x100, 0x180), (0x200, 0x220)]),
            at("b.obj", &[(0x100, 0x180)]),
            at("c.obj", &[(0x300, 0x32b)]),
            at("d.obj", &[(0x400, 0x410)]),
            at("e.obj", &[(0x40f, 0x420)]),
            at("f.obj", &[(0x500, 0x600), (0x510, 0x520)]),
            at("g.obj", &[(0x5f0, 0x5f8), (0x700, 0x710)]),
        ];
        assert_eq!(
            credible(&all),
            vec![true, false, true, false, false, true, true]
        );
        assert_eq!(credible(&all), naive(&all));
    }

    #[test]
    fn credibility_agrees_with_the_pairwise_reading_on_many_layouts() {
        let mut seed = 0x2545_f491_u32;
        let mut next = || {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            seed
        };
        for _ in 0..200 {
            let members: Vec<Placed> = (0..6)
                .map(|m| {
                    let ranges: Vec<(u32, u32)> = (0..(next() % 4))
                        .map(|_| {
                            let a = next() % 64;
                            (a, a + 1 + next() % 8)
                        })
                        .collect();
                    at(&format!("m{m}.obj"), &ranges)
                })
                .collect();
            assert_eq!(credible(&members), naive(&members));
        }
    }

    #[test]
    fn overlapping_ranges_are_the_pairwise_reading_on_many_layouts() {
        let mut seed = 0x1234_5678_u32;
        let mut next = || {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            seed
        };
        for _ in 0..200 {
            let members: Vec<Placed> = (0..6)
                .map(|m| {
                    let ranges: Vec<(u32, u32)> = (0..(next() % 4))
                        .map(|_| {
                            let a = next() % 64;
                            (a, a + next() % 9)
                        })
                        .collect();
                    at(&format!("m{m}.obj"), &ranges)
                })
                .collect();
            let mut want = BTreeSet::new();
            for (i, p) in members.iter().enumerate() {
                for (k, &(a, b)) in p.ranges.iter().enumerate() {
                    for (j, q) in members.iter().enumerate() {
                        for (l, &(x, y)) in q.ranges.iter().enumerate() {
                            if i != j && x < b && a < y {
                                want.insert((i, k, j, l));
                            }
                        }
                    }
                }
            }
            let mut pairs = Pairs {
                budget: PAIR_BUDGET,
            };
            let got: BTreeSet<_> = pairs.overlapping(&members).unwrap().into_iter().collect();
            assert_eq!(got, want);
        }
    }

    #[test]
    fn one_long_range_over_many_short_ones_costs_one_check_each() {
        let n = 100_000u32;
        let mut all: Vec<Placed> = (0..n)
            .map(|k| at(&format!("m{k}.obj"), &[(16 * k, 16 * k + 8)]))
            .collect();
        all.push(at("long.obj", &[(0, 16 * n)]));
        let mut pairs = Pairs {
            budget: PAIR_BUDGET,
        };
        let got = pairs.overlapping(&all).unwrap();
        assert_eq!(got.len(), 2 * usize::try_from(n).unwrap());
        assert!(PAIR_BUDGET - pairs.budget <= 2 * u64::from(n));
    }

    #[test]
    fn a_range_another_member_holds_is_refused_with_its_owners() {
        let mut all = vec![
            at("a.obj", &[(0x1000, 0x1020)]),
            at("c.obj", &[(0x1020, 0x1040)]),
            at("s.obj", &[(0x1018, 0x1028)]),
        ];
        let mut pairs = Pairs {
            budget: PAIR_BUDGET,
        };
        keep_one_owner(&mut all, &[], &mut pairs).unwrap();
        let s = all.iter().find(|p| p.member == "s.obj").unwrap();
        assert!(s.ranges.is_empty());
        assert_eq!(
            s.refused,
            vec![Refused {
                lo: 0x1018,
                hi: 0x1028,
                owners: vec!["a.obj".into(), "c.obj".into()]
            }]
        );
    }

    fn placed_at(
        member: &str,
        ranges: &[(u32, u32)],
        defined: &[(&str, u32)],
        imports: &[(&str, u32)],
    ) -> Placed {
        Placed {
            member: format!("obj\\{member}"),
            archive: "libc".into(),
            ranges: ranges.to_vec(),
            unique: ranges.len(),
            defined_at: defined.iter().map(|(n, a)| ((*n).into(), *a)).collect(),
            imports: imports.iter().map(|(n, a)| ((*n).into(), *a)).collect(),
            ..Placed::default()
        }
    }

    fn owner(all: &[Placed], a: u32) -> Option<String> {
        all.iter()
            .find(|p| p.ranges.iter().any(|r| r.0 == a))
            .map(|p| p.stem().to_string())
    }

    fn settle(all: &mut [Placed], record: &[Recorded]) {
        let mut pairs = Pairs {
            budget: PAIR_BUDGET,
        };
        mark_ties(all, &mut pairs).unwrap();
        keep_one_owner(all, record, &mut pairs).unwrap();
    }

    #[test]
    fn a_tie_goes_to_the_witnessed_member_then_the_recorded_one() {
        let at = 0x6_bca0;
        let mut all = vec![
            placed_at(
                "mbscat.obj",
                &[(at, at + 0xf0)],
                &[("__mbscpy", at), ("__mbscat", at + 0x10)],
                &[],
            ),
            placed_at(
                "strcat.obj",
                &[(at, at + 0xf0)],
                &[("_strcpy", at), ("_strcat", at + 0x10)],
                &[],
            ),
            placed_at("cvt.obj", &[(0x6_b000, 0x6_b080)], &[], &[("_strcpy", at)]),
            placed_at(
                "ismbbyte.obj",
                &[(0x6_6057, 0x6_6068)],
                &[("__ismbbkalnum", 0x6_6057)],
                &[],
            ),
            placed_at(
                "tell.obj",
                &[(0x6_6057, 0x6_6068)],
                &[("__tell", 0x6_6057)],
                &[],
            ),
            placed_at(
                "abs.obj",
                &[(0x6_7000, 0x6_7010)],
                &[("_abs", 0x6_7000)],
                &[],
            ),
            placed_at(
                "labs.obj",
                &[(0x6_7000, 0x6_7010)],
                &[("_labs", 0x6_7000)],
                &[],
            ),
        ];
        let record = [Recorded {
            rva: 0x6_6057,
            len: 0x11,
            member: "obj\\tell.obj".into(),
            archive: "libc".into(),
        }];
        settle(&mut all, &record);
        assert_eq!(owner(&all, at).as_deref(), Some("strcat.obj"));
        assert_eq!(owner(&all, 0x6_6057).as_deref(), Some("tell.obj"));
        assert_eq!(owner(&all, 0x6_7000).as_deref(), Some("abs.obj"));
        let by = |m: &str| all.iter().find(|p| p.stem() == m).unwrap();
        assert!(by("strcat.obj").tie_break[0].starts_with("WITNESS"));
        assert!(by("tell.obj").tie_break[0].starts_with("RECORD"));
        assert!(by("abs.obj").tie_break.is_empty());
        assert_eq!(by("abs.obj").tied_with, vec!["obj\\labs.obj"]);
    }

    #[test]
    fn a_contested_range_goes_to_the_stronger_evidence_and_a_tie_says_so() {
        let mut ismbbyte = placed_at(
            "ismbbyte.obj",
            &[(0x6_6057, 0x6_6068), (0x6_c000, 0x6_c042)],
            &[("__ismbbkalnum", 0x6_6057)],
            &[],
        );
        ismbbyte.statics_at = vec![("_x_ismbbtype".into(), 0x6_c000)];
        ismbbyte.refs = vec![(0x6_6057, "_x_ismbbtype".into(), 0x6_95bb, false)];
        let mut tell = placed_at(
            "tell.obj",
            &[(0x6_6057, 0x6_6068)],
            &[("__tell", 0x6_6057)],
            &[("__lseek", 0x6_95bb)],
        );
        tell.refs = vec![(0x6_6057, "__lseek".into(), 0x6_95bb, true)];
        let lseek = placed_at(
            "lseek.obj",
            &[(0x6_95bb, 0x6_9620)],
            &[("__lseek", 0x6_95bb)],
            &[],
        );
        let mut all = vec![ismbbyte.clone(), tell.clone(), lseek.clone()];
        settle(&mut all, &[]);
        assert_eq!(owner(&all, 0x6_6057).as_deref(), Some("tell.obj"));
        assert_eq!(owner(&all, 0x6_c000).as_deref(), Some("ismbbyte.obj"));
        let by = |all: &[Placed], m: &str| all.iter().find(|p| p.stem() == m).unwrap().clone();
        assert_eq!(
            by(&all, "ismbbyte.obj").outvoted,
            vec!["lost 0x66057..0x66068 to obj\\tell.obj (+1 against -1)".to_string()]
        );
        assert!(by(&all, "tell.obj").tie_break.is_empty());
        ismbbyte.ranges = vec![(0x6_6057, 0x6_6068)];
        let mut all = vec![ismbbyte, tell, lseek];
        settle(&mut all, &[]);
        let t = by(&all, "tell.obj");
        assert_eq!(t.tied_with, vec!["obj\\ismbbyte.obj"]);
        assert_eq!(
            t.tie_break,
            vec![
                "EVIDENCE obj\\tell.obj took 0x66057..0x66068 from obj\\ismbbyte.obj (+1 against -1)"
            ]
        );
    }

    #[test]
    fn a_tie_is_decided_only_when_every_tied_partner_has_a_note() {
        let (r1, r2) = ((0x6_6057, 0x6_6068), (0x6_7000, 0x6_7010));
        let mut ismbbyte = placed_at(
            "ismbbyte.obj",
            &[r1, r2],
            &[("__ismbbkalnum", 0x6_6057)],
            &[],
        );
        ismbbyte.statics_at = vec![("_x_ismbbtype".into(), 0x6_c000)];
        ismbbyte.refs = vec![(0x6_6057, "_x_ismbbtype".into(), 0x6_95bb, false)];
        let mut tell = placed_at(
            "tell.obj",
            &[r1, r2],
            &[("__tell", 0x6_6057)],
            &[("__lseek", 0x6_95bb)],
        );
        tell.refs = vec![(0x6_6057, "__lseek".into(), 0x6_95bb, true)];
        let dup = placed_at("dup.obj", &[r1, r2], &[], &[("__lseek", 0x6_95bb)]);
        let lseek = placed_at(
            "lseek.obj",
            &[(0x6_95bb, 0x6_9620)],
            &[("__lseek", 0x6_95bb)],
            &[],
        );
        let mut all = vec![ismbbyte, tell, dup, lseek];
        settle(&mut all, &[]);
        let t = all.iter().find(|p| p.stem() == "tell.obj").unwrap();
        assert_eq!(t.tied_with.len(), 2, "{:?}", t.tied_with);
        assert_eq!(t.tie_break.len(), 2, "{:?}", t.tie_break);
        assert_eq!(t.decided_with, vec!["obj\\ismbbyte.obj"]);
        assert_eq!(t.undecided(), vec!["obj\\dup.obj"]);
    }

    #[test]
    fn one_ghost_does_not_silence_a_name() {
        let body = [0x6au8, 0x19, 0xe8, 0, 0, 0, 0, 0x59, 0xc3];
        let c = Code {
            index: 1,
            symbol: Some("__purecall".into()),
            external: true,
            bytes: body.to_vec(),
            relocated: vec![false, false, false, true, true, true, true, false, false],
            comdat: true,
            align: 16,
            relocations: vec![crate::code::Reloc {
                at: 3,
                symbol: "__amsg_exit".into(),
                kind: crate::code::REL_REL32,
                external: true,
                global: true,
            }],
        };
        let amsg = 0x1100u32;
        let mut text = vec![0xccu8; 0x200];
        text[0x10..0x19].copy_from_slice(&body);
        let rel = amsg.wrapping_sub(0x1013 + 4);
        text[0x13..0x17].copy_from_slice(&rel.to_le_bytes());
        let t = Text {
            bytes: &text,
            rva: 0x1000,
            image_base: 0x1000_0000,
        };
        let pv = Placed {
            member: "obj\\purevirt.obj".into(),
            unidentified: vec![c],
            unidentifiable: 1,
            ..Placed::default()
        };
        let mut all = vec![
            placed_at(
                "crt0msg.obj",
                &[(0x1100, 0x1120)],
                &[("__amsg_exit", amsg)],
                &[],
            ),
            placed_at(
                "lseek.obj",
                &[(0x1140, 0x1160), (0x1160, 0x1170)],
                &[],
                &[("__amsg_exit", amsg)],
            ),
            placed_at(
                "wlseek.obj",
                &[(0x1140, 0x1160)],
                &[],
                &[("__amsg_exit", 0x1180)],
            ),
            pv,
        ];
        place_by_witness(&mut all, &t);
        assert_eq!(all[3].ranges, vec![(0x1010, 0x1019)]);
        assert_eq!(all[3].defined_at, vec![("__purecall".to_string(), 0x1010)]);
        assert_eq!(all[3].by_witness, 1);
    }
}
