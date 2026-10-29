use anyhow::{Context as _, Result, anyhow, bail};
use std::collections::{BTreeMap, BTreeSet};

use crate::iat::{Blocks, Pull};
use crate::qsort::{self, Crt};
use crate::verbose::Kind;

pub const SPACE_MAX: usize = 1 << 28;

pub const NODE_BUDGET: usize = 1 << 20;

pub const KEEP: usize = 12;

#[derive(Clone, Debug)]
pub struct Run {
    pub member: String,
    pub live: Vec<String>,
    pub nt: bool,
    pub start: usize,
}

#[must_use]
pub fn runs_of(pulls: &[Pull]) -> Vec<Run> {
    let mut out: Vec<Run> = Vec::new();
    for (i, p) in pulls.iter().enumerate() {
        let fresh = out.last().is_none_or(|r| r.member != p.member || r.nt);
        if fresh {
            out.push(Run {
                member: p.member.clone(),
                live: Vec::new(),
                nt: false,
                start: i,
            });
        }
        let Some(r) = out.last_mut() else { continue };
        match p.kind {
            Kind::NullThunk => r.nt = true,
            _ => r.live.push(p.symbol.clone()),
        }
    }
    out
}

pub fn ranks(keys: &[u8], crt: Crt) -> Result<Vec<usize>> {
    let mut idx: Vec<usize> = (0..keys.len()).collect();
    let key = |i: &usize| keys.get(*i).copied().unwrap_or(0);
    qsort::sort(crt, &mut idx, &|a: &usize, b: &usize| key(a).cmp(&key(b)))?;
    let mut seen = [0usize; 256];
    let mut r = vec![0usize; keys.len()];
    for i in idx {
        let k = usize::from(key(&i));
        if let (Some(slot), Some(c)) = (r.get_mut(i), seen.get_mut(k)) {
            *slot = *c;
            *c += 1;
        }
    }
    Ok(r)
}

struct Cand {
    pos: Vec<usize>,
    target: usize,
}

pub type Block = (Vec<usize>, Vec<usize>);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Placed<T> {
    Found(T),
    None,
    OutOfBudget,
}

pub fn place(blocks: &[Block], rank: &[usize]) -> Placed<Vec<usize>> {
    let (cands, fixed) = match place_pinned(blocks, rank, None) {
        Placed::Found(x) => x,
        Placed::None => return Placed::None,
        Placed::OutOfBudget => return Placed::OutOfBudget,
    };
    let held: BTreeSet<usize> = fixed
        .iter()
        .enumerate()
        .filter_map(|(a, d)| {
            cands
                .get(a)
                .and_then(|c| c.pos.get(d.trailing_zeros() as usize))
                .copied()
        })
        .collect();
    Placed::Found(
        blocks
            .iter()
            .flat_map(|(pos, _)| pos.iter().copied())
            .filter(|q| !held.contains(q))
            .collect(),
    )
}

pub fn place_slack(blocks: &[Block], rank: &[usize]) -> Placed<Vec<u64>> {
    let cands = match place_pinned(blocks, rank, None) {
        Placed::Found((c, _)) => c,
        Placed::None => return Placed::None,
        Placed::OutOfBudget => return Placed::OutOfBudget,
    };
    let mut out = Vec::with_capacity(cands.len());
    for a in 0..cands.len() {
        let w = cands.get(a).map_or(0, |c| c.pos.len()).min(64);
        let mut m = 0u64;
        for s in (0..w).filter_map(|s| u32::try_from(s).ok()) {
            match place_pinned(blocks, rank, Some((a, s))) {
                Placed::Found(_) => m |= 1u64 << s,
                Placed::None => {}
                Placed::OutOfBudget => return Placed::OutOfBudget,
            }
        }
        out.push(m);
    }
    Placed::Found(out)
}

fn bits(d: u64) -> impl Iterator<Item = u32> {
    (0..64u32).filter(move |s| d & (1u64 << s) != 0)
}

struct Solver<'a> {
    cands: &'a [Cand],
    run_next: &'a [Option<usize>],
    by_target: &'a [usize],
    rank: &'a [usize],
    nodes: usize,
}

impl Solver<'_> {
    fn r(&self, a: usize, s: u32) -> usize {
        self.cands
            .get(a)
            .and_then(|c| c.pos.get(s as usize))
            .and_then(|&q| self.rank.get(q))
            .copied()
            .unwrap_or(usize::MAX)
    }

    fn prop(&self, dom: &mut [u64]) -> bool {
        loop {
            let mut changed = false;
            for a in 0..self.cands.len() {
                let Some(b) = self.run_next.get(a).copied().flatten() else {
                    continue;
                };
                let (da, db) = (
                    dom.get(a).copied().unwrap_or(0),
                    dom.get(b).copied().unwrap_or(0),
                );
                if da == 0 || db == 0 {
                    return false;
                }
                let lo = da.trailing_zeros();
                let hi = 63 - db.leading_zeros();
                let nb = db & !((1u64 << lo) - 1);
                let na = if hi >= 63 {
                    da
                } else {
                    da & ((1u64 << (hi + 1)) - 1)
                };
                if nb != db || na != da {
                    changed = true;
                    if let Some(x) = dom.get_mut(a) {
                        *x = na;
                    }
                    if let Some(x) = dom.get_mut(b) {
                        *x = nb;
                    }
                }
            }
            for w in self.by_target.windows(2) {
                let (Some(&x), Some(&y)) = (w.first(), w.get(1)) else {
                    continue;
                };
                let (dx, dy) = (
                    dom.get(x).copied().unwrap_or(0),
                    dom.get(y).copied().unwrap_or(0),
                );
                if dx == 0 || dy == 0 {
                    return false;
                }
                let min_x = bits(dx).map(|s| self.r(x, s)).min().unwrap_or(usize::MAX);
                let max_y = bits(dy).map(|s| self.r(y, s)).max().unwrap_or(0);
                let mut ny = dy;
                for s in bits(dy) {
                    if self.r(y, s) <= min_x {
                        ny &= !(1u64 << s);
                    }
                }
                let mut nx = dx;
                for s in bits(dx) {
                    if self.r(x, s) >= max_y {
                        nx &= !(1u64 << s);
                    }
                }
                if nx != dx || ny != dy {
                    changed = true;
                    if let Some(v) = dom.get_mut(x) {
                        *v = nx;
                    }
                    if let Some(v) = dom.get_mut(y) {
                        *v = ny;
                    }
                }
            }
            if dom.contains(&0) {
                return false;
            }
            if !changed {
                return true;
            }
        }
    }

    fn search(&mut self, mut dom: Vec<u64>) -> Placed<Vec<u64>> {
        self.nodes += 1;
        if self.nodes > NODE_BUDGET {
            return Placed::OutOfBudget;
        }
        if !self.prop(&mut dom) {
            return Placed::None;
        }
        let pick = dom
            .iter()
            .enumerate()
            .filter(|(_, d)| d.count_ones() > 1)
            .min_by_key(|(_, d)| d.count_ones())
            .map(|(a, _)| a);
        let Some(a) = pick else {
            return Placed::Found(dom);
        };
        let d = dom.get(a).copied().unwrap_or(0);
        for s in bits(d) {
            let mut next = dom.clone();
            if let Some(v) = next.get_mut(a) {
                *v = 1u64 << s;
            }
            match self.search(next) {
                Placed::None => {}
                other => return other,
            }
        }
        Placed::None
    }
}

fn place_pinned(
    blocks: &[Block],
    rank: &[usize],
    pin: Option<(usize, u32)>,
) -> Placed<(Vec<Cand>, Vec<u64>)> {
    let mut cands: Vec<Cand> = Vec::new();
    let mut run_next: Vec<Option<usize>> = Vec::new();
    for (pos, tgt) in blocks {
        let Some(d) = pos.len().checked_sub(tgt.len()) else {
            return Placed::None;
        };
        if d >= 64 {
            return Placed::None;
        }
        for (i, &t) in tgt.iter().enumerate() {
            if t == usize::MAX {
                return Placed::None;
            }
            let p: Vec<usize> = (0..=d).filter_map(|s| pos.get(i + s).copied()).collect();
            run_next.push((i + 1 < tgt.len()).then_some(cands.len() + 1));
            cands.push(Cand { pos: p, target: t });
        }
    }
    let n = cands.len();
    let mut by_target: Vec<usize> = (0..n).collect();
    by_target.sort_by_key(|&a| cands.get(a).map_or(usize::MAX, |c| c.target));
    let full = |a: usize| -> u64 {
        let w = cands.get(a).map_or(0, |c| c.pos.len());
        if w >= 64 { u64::MAX } else { (1u64 << w) - 1 }
    };
    let mut dom: Vec<u64> = (0..n).map(full).collect();
    if let Some((a, s)) = pin {
        let Some(d) = dom.get_mut(a) else {
            return Placed::None;
        };
        if s >= 64 || *d & (1u64 << s) == 0 {
            return Placed::None;
        }
        *d = 1u64 << s;
    }
    let mut solver = Solver {
        cands: &cands,
        run_next: &run_next,
        by_target: &by_target,
        rank,
        nodes: 0,
    };
    match solver.search(dom) {
        Placed::Found(fixed) => Placed::Found((cands, fixed)),
        Placed::None => Placed::None,
        Placed::OutOfBudget => Placed::OutOfBudget,
    }
}

#[derive(Clone, Debug)]
pub struct Hidden {
    pub member: String,
    pub pos: (usize, usize),
    pub dead: (usize, usize),
    pub tail: (usize, usize),
}

fn parse_range(s: &str) -> Result<(usize, usize)> {
    let (a, b) = s.split_once("..").unwrap_or((s, s));
    let a: usize = a.trim().parse().with_context(|| format!("range `{s}'"))?;
    let b: usize = b.trim().parse().with_context(|| format!("range `{s}'"))?;
    if a > b {
        bail!("range `{s}' is empty");
    }
    Ok((a, b))
}

pub type Ranges = Vec<(usize, usize)>;

pub fn parse_run_spec(spec: &str, runs: &[Run], n_pulls: usize) -> Result<(Ranges, Vec<Hidden>)> {
    let mut range: Ranges = runs
        .iter()
        .map(|r| if r.live.is_empty() { (1, 4) } else { (0, 4) })
        .collect();
    let mut hidden = Vec::new();
    for item in spec.split(',').map(str::trim).filter(|s| !s.is_empty()) {
        if let Some(h) = item.strip_prefix('+') {
            let (pos, rest) = h
                .split_once(':')
                .ok_or_else(|| anyhow!("--run-search `{item}': want +P[..Q]:DLL=A..B[/C..D]"))?;
            let (dll, ab) = rest
                .split_once('=')
                .ok_or_else(|| anyhow!("--run-search `{item}': want +P[..Q]:DLL=A..B[/C..D]"))?;
            let (ab, cd) = ab.split_once('/').unwrap_or((ab, "0..2"));
            let member = runs
                .iter()
                .find(|r| r.member.eq_ignore_ascii_case(dll))
                .map(|r| r.member.clone())
                .ok_or_else(|| {
                    anyhow!("--run-search `{item}': {dll} has no run, so it cannot hide one")
                })?;
            let pos = parse_range(pos)?;
            if pos.1 > n_pulls {
                bail!("--run-search `{item}': position past the population ({n_pulls})");
            }
            hidden.push(Hidden {
                member,
                pos,
                dead: parse_range(ab)?,
                tail: parse_range(cd)?,
            });
            continue;
        }
        let (i, ab) = item
            .split_once('=')
            .ok_or_else(|| anyhow!("--run-search `{item}': want RUN=A..B"))?;
        let i: usize = i.parse().with_context(|| format!("run index `{i}'"))?;
        let slot = range
            .get_mut(i)
            .ok_or_else(|| anyhow!("--run-search: no run {i} (there are {})", runs.len()))?;
        *slot = parse_range(ab)?;
    }
    Ok((range, hidden))
}

#[derive(Debug)]
pub struct Piece<'a> {
    pub member: &'a str,
    pub live: &'a [String],
    pub dead: usize,
    pub nt: bool,
    pub hidden: Option<usize>,
}

#[must_use]
pub fn pieces<'a>(runs: &'a [Run], hidden: &'a [Hidden], c: &[usize]) -> Option<Vec<Piece<'a>>> {
    let nr = runs.len();
    let at = |i: usize| c.get(i).copied().unwrap_or(0);
    let mut hv: Vec<(usize, usize, usize, usize)> = (0..hidden.len())
        .map(|j| (at(nr + 3 * j), j, at(nr + 3 * j + 1), at(nr + 3 * j + 2)))
        .collect();
    hv.sort_unstable();
    let mut out = Vec::new();
    let mut k = 0usize;
    let push_hidden = |out: &mut Vec<Piece<'a>>, j: usize, dead: usize| {
        let member = hidden.get(j).map_or("", |h| h.member.as_str());
        out.push(Piece {
            member,
            live: &[],
            dead,
            nt: false,
            hidden: Some(j),
        });
    };
    let tail_min = |j: usize| hidden.get(j).map_or(0, |h| h.tail.0);
    for (ri, r) in runs.iter().enumerate() {
        let end = r.start + r.live.len() + usize::from(r.nt);
        while let Some(&(p, j, d, t)) = hv.get(k) {
            if p > r.start {
                break;
            }
            if t != tail_min(j) {
                return None;
            }
            push_hidden(&mut out, j, d);
            k += 1;
        }
        let mut from = 0usize;
        let mut dead = at(ri);
        while let Some(&(p, j, d, t)) = hv.get(k) {
            if p >= end {
                break;
            }
            let cut = (p - r.start).min(r.live.len());
            out.push(Piece {
                member: &r.member,
                live: r.live.get(from..cut).unwrap_or(&[]),
                dead,
                nt: false,
                hidden: None,
            });
            dead = t;
            push_hidden(&mut out, j, d);
            k += 1;
            while let Some(&(p2, j2, d2, t2)) = hv.get(k) {
                if p2 != p {
                    break;
                }
                if t2 != tail_min(j2) {
                    return None;
                }
                push_hidden(&mut out, j2, d2);
                k += 1;
            }
            from = cut;
        }
        out.push(Piece {
            member: &r.member,
            live: r.live.get(from..).unwrap_or(&[]),
            dead,
            nt: r.nt,
            hidden: None,
        });
    }
    while let Some(&(_, j, d, t)) = hv.get(k) {
        if t != tail_min(j) {
            return None;
        }
        push_hidden(&mut out, j, d);
        k += 1;
    }
    Some(out)
}

#[derive(Default)]
struct Top {
    best: Vec<(usize, u32, Vec<usize>)>,
    full: usize,
    tested: usize,
    spent: bool,
}

impl Top {
    fn push(&mut self, x: (usize, u32, Vec<usize>), n_live: usize) {
        self.tested += 1;
        if x.0 == n_live {
            self.full += 1;
        }
        self.best.push(x);
        if self.best.len() > 4 * KEEP {
            self.trim();
        }
    }

    fn trim(&mut self) {
        self.best
            .sort_by(|a, b| b.0.cmp(&a.0).then_with(|| a.2.cmp(&b.2)));
        self.best.truncate(KEEP);
    }

    fn merge(&mut self, o: Self) {
        self.full += o.full;
        self.tested += o.tested;
        self.spent |= o.spent;
        self.best.extend(o.best);
        self.trim();
    }
}

type Fit = (usize, u32, Vec<usize>, bool);

type Layout<'p> = (Vec<usize>, Vec<(&'p str, Vec<usize>, Vec<usize>)>);

pub fn run_search(
    pulls: &[Pull],
    crt: Crt,
    blocks: &Blocks,
    spec: &str,
    total: Option<usize>,
    names: bool,
    jobs: usize,
) -> Result<Vec<String>> {
    let mut lines = Vec::new();
    let runs = runs_of(pulls);
    let mut members: Vec<String> = runs.iter().map(|r| r.member.clone()).collect();
    members.sort();
    members.dedup();
    if members.len() > 255 {
        bail!("--run-search: {} DLLs, more than 255 keys", members.len());
    }
    let kid =
        |m: &str| u8::try_from(members.iter().position(|x| x == m).unwrap_or(0)).unwrap_or(u8::MAX);
    let (range, hidden) = parse_run_spec(spec, &runs, pulls.len())?;
    lines.push(String::new());
    lines.push(format!(
        "  RUN-LENGTH SEARCH ({}): the key sequence is one run per library sweep",
        crt.label()
    ));
    for (i, (r, (a, b))) in runs.iter().zip(range.iter()).enumerate() {
        let end = r.start + r.live.len() + usize::from(r.nt);
        lines.push(format!(
            "    run {i:2} {:13} {:3} live{}  discarded {a}..={b}  (population {}..{end})",
            r.member,
            r.live.len(),
            if r.nt { " + null thunk" } else { "" },
            r.start
        ));
        if names {
            for (k, n) in r.live.iter().enumerate() {
                lines.push(format!("           {:3} {k:2} {n}", r.start + k));
            }
        }
    }
    for (j, h) in hidden.iter().enumerate() {
        lines.push(format!(
            "    hidden {j} {:13} before population {}..={}  discarded {}..={}  split tail {}..={}",
            h.member, h.pos.0, h.pos.1, h.dead.0, h.dead.1, h.tail.0, h.tail.1
        ));
    }
    let sets: BTreeMap<String, BTreeMap<String, usize>> = blocks
        .iter()
        .map(|(k, v)| {
            (
                k.clone(),
                v.iter()
                    .enumerate()
                    .map(|(j, (_, n))| (n.clone(), j))
                    .collect(),
            )
        })
        .collect();
    let mut dims: Vec<(usize, usize)> = range.clone();
    for h in &hidden {
        dims.extend([h.pos, h.dead, h.tail]);
    }
    let space = dims
        .iter()
        .try_fold(1usize, |acc, (a, b)| acc.checked_mul(b - a + 1))
        .ok_or_else(|| anyhow!("--run-search: the space overflows"))?;
    if space > SPACE_MAX {
        bail!(
            "--run-search: {space} points, more than the {SPACE_MAX} this search walks; narrow the ranges"
        );
    }
    lines.push(format!("    {space} point(s) in the space"));
    let n_live: usize = runs.iter().map(|r| r.live.len()).sum();
    let combo = |mut ix: usize| -> Vec<usize> {
        dims.iter()
            .map(|(a, b)| {
                let w = b - a + 1;
                let v = a + ix % w;
                ix /= w;
                v
            })
            .collect()
    };
    let nr = runs.len();
    let dead_sum = |ps: &[Piece]| -> usize { ps.iter().map(|p| p.dead).sum() };
    let layout = |c: &[usize]| -> Result<Option<Layout<'_>>> {
        let Some(ps) = pieces(&runs, &hidden, c) else {
            return Ok(None);
        };
        if let Some(t) = total
            && dead_sum(&ps) != t
        {
            return Ok(None);
        }
        let mut keys: Vec<u8> = Vec::new();
        let mut pos_of: Vec<(&str, Vec<usize>, Vec<usize>)> = Vec::new();
        for p in &ps {
            let k = kid(p.member);
            let start = keys.len();
            let n = p.live.len() + p.dead;
            keys.resize(keys.len() + n, k);
            if p.nt {
                keys.push(k);
            }
            let tg = sets.get(&p.member.to_ascii_lowercase());
            let targets: Vec<usize> = p
                .live
                .iter()
                .map(|n| tg.and_then(|m| m.get(n)).copied().unwrap_or(usize::MAX))
                .collect();
            pos_of.push((p.member, (start..start + n).collect(), targets));
        }
        Ok(Some((ranks(&keys, crt)?, pos_of)))
    };
    let blocks_of = |pos_of: &[(&str, Vec<usize>, Vec<usize>)], m: &str| -> Vec<Block> {
        pos_of
            .iter()
            .filter(|(mm, _, _)| *mm == m)
            .map(|(_, p, t)| (p.clone(), t.clone()))
            .collect()
    };
    let fit = |c: &[usize]| -> Result<Option<Fit>> {
        let Some((rank, pos_of)) = layout(c)? else {
            return Ok(None);
        };
        let mut score = 0usize;
        let mut mask = 0u32;
        let mut dead_all = Vec::new();
        let mut spent = false;
        for (mi, m) in members.iter().enumerate() {
            let bl = blocks_of(&pos_of, m);
            let live: usize = bl.iter().map(|(_, t)| t.len()).sum();
            match place(&bl, &rank) {
                Placed::Found(d) => {
                    score += live;
                    mask |= 1u32 << mi.min(31);
                    dead_all.extend(d);
                }
                Placed::None => {}
                Placed::OutOfBudget => spent = true,
            }
        }
        Ok(Some((score, mask, dead_all, spent)))
    };
    let jobs = jobs.clamp(1, space.max(1));
    let chunk = space.div_ceil(jobs);
    let mut top = Top::default();
    let mut first_err: Option<anyhow::Error> = None;
    std::thread::scope(|s| {
        let hs: Vec<_> = (0..jobs)
            .map(|j| {
                let (fit, combo) = (&fit, &combo);
                s.spawn(move || -> Result<Top> {
                    let mut t = Top::default();
                    for ix in (j * chunk)..((j + 1) * chunk).min(space) {
                        let c = combo(ix);
                        if let Some((sc, m, _, spent)) = fit(&c)? {
                            t.spent |= spent;
                            t.push((sc, m, c), n_live);
                        }
                    }
                    t.trim();
                    Ok(t)
                })
            })
            .collect();
        for h in hs {
            match h.join() {
                Ok(Ok(t)) => top.merge(t),
                Ok(Err(e)) => {
                    first_err.get_or_insert(e);
                }
                Err(_) => {
                    first_err.get_or_insert(anyhow!("a run-search worker panicked"));
                }
            }
        }
    });
    if let Some(e) = first_err {
        return Err(e);
    }
    lines.push(format!(
        "    {} assignment(s) tested; {} explain all {n_live} live slots; best {KEEP}:",
        top.tested, top.full
    ));
    if top.spent {
        lines.push(format!(
            "    NOTE: some placements ran out of the {NODE_BUDGET}-node budget and were scored as not fitting"
        ));
    }
    let show = |c: &[usize]| -> String {
        let runs_s: Vec<String> = c.iter().take(nr).map(ToString::to_string).collect();
        let mut s = format!("[{}]", runs_s.join(" "));
        for (j, h) in hidden.iter().enumerate() {
            let v = |o: usize| c.get(nr + 3 * j + o).copied().unwrap_or(0);
            s.push_str(&format!("  +{}:{}={}/{}", v(0), h.member, v(1), v(2)));
        }
        s
    };
    for (s, mask, c) in &top.best {
        let fits: Vec<&str> = members
            .iter()
            .enumerate()
            .filter(|(mi, _)| mask & (1u32 << (*mi).min(31)) != 0)
            .map(|(_, m)| m.as_str())
            .collect();
        let t = pieces(&runs, &hidden, c).map_or(0, |ps| dead_sum(&ps));
        lines.push(format!(
            "      {s:3}/{n_live}  discarded {}  total {t}  fits {}",
            show(c),
            fits.join(" ")
        ));
    }
    if let Some((_, _, c)) = top.best.first()
        && let (Some(ps), Some((s, _, dead, _))) = (pieces(&runs, &hidden, c), fit(c)?)
    {
        lines.push(format!(
            "    the best's key sequence ({s}/{n_live}), piece by piece, with its discarded imports' offsets (fitted DLLs only):"
        ));
        let mut at = 0usize;
        for p in &ps {
            let len = p.live.len() + p.dead;
            let mine: Vec<String> = dead
                .iter()
                .filter(|&&q| q >= at && q < at + len)
                .map(|q| (q - at).to_string())
                .collect();
            lines.push(format!(
                "      {:13} {:3} live {:2} discarded{}{}{}",
                p.member,
                p.live.len(),
                p.dead,
                if p.nt { " + null thunk" } else { "" },
                p.hidden
                    .map_or(String::new(), |j| format!("  (hidden {j})")),
                if mine.is_empty() {
                    String::new()
                } else {
                    format!("  at offset(s) {}", mine.join(" "))
                }
            ));
            at += len + usize::from(p.nt);
        }
    }
    if names {
        let set = |m: u64| -> String {
            let v: Vec<u32> = bits(m).collect();
            match (v.first(), v.last()) {
                (Some(a), Some(b)) if v.len() == (b - a + 1) as usize && a != b => {
                    format!("{a}-{b}")
                }
                _ => v.iter().map(u32::to_string).collect::<Vec<_>>().join(","),
            }
        };
        for (s, _, c) in top.best.iter().filter(|(s, _, _)| *s == n_live) {
            let (Some(ps), Some((rank, pos_of))) = (pieces(&runs, &hidden, c), layout(c)?) else {
                continue;
            };
            lines.push(format!(
                "    SLACK of {} ({s}/{n_live}): each live import, the discarded imports of its run that may load before it",
                show(c)
            ));
            for m in &members {
                let Placed::Found(sl) = place_slack(&blocks_of(&pos_of, m), &rank) else {
                    continue;
                };
                let mut k = 0usize;
                for (pi, p) in ps
                    .iter()
                    .enumerate()
                    .filter(|(_, p)| p.member == m.as_str())
                {
                    lines.push(format!("      {m} (piece {pi}, {} discarded):", p.dead));
                    for n in p.live {
                        lines.push(format!(
                            "        {:>6}  {n}",
                            sl.get(k).map_or_else(|| "?".to_string(), |&b| set(b))
                        ));
                        k += 1;
                    }
                }
            }
        }
    }
    Ok(lines)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pull(member: &str, symbol: &str, kind: Kind) -> Pull {
        Pull {
            member: member.into(),
            symbol: symbol.into(),
            kind,
            first_ref: None,
        }
    }

    #[test]
    fn runs_split_at_null_thunks_and_place_finds_the_identity() {
        let p = vec![
            pull("K.dll", "a", Kind::Import),
            pull("K.dll", "b", Kind::Import),
            pull("K.dll", "K_NULL_THUNK_DATA", Kind::NullThunk),
            pull("U.dll", "c", Kind::Import),
            pull("U.dll", "U_NULL_THUNK_DATA", Kind::NullThunk),
            pull("K.dll", "d", Kind::Import),
        ];
        let r = runs_of(&p);
        assert_eq!(r.len(), 3);
        assert_eq!(r[0].live, vec!["a", "b"]);
        assert!(r[0].nt && !r[2].nt);
        let keys = [0u8, 0, 0, 1, 1, 0];
        let rank = ranks(&keys, Crt::Vc6).unwrap();
        let mut live = [0usize, 1, 5];
        live.sort_by_key(|&q| rank[q]);
        let tgt = |q: usize| live.iter().position(|&x| x == q).unwrap();
        let blocks = vec![(vec![0, 1], vec![tgt(0), tgt(1)]), (vec![5], vec![tgt(5)])];
        assert_eq!(place(&blocks, &rank), Placed::Found(Vec::new()));
    }

    #[test]
    fn a_hidden_sweep_splits_a_run_and_a_boundary_one_does_not() {
        let p = vec![
            pull("K.dll", "a", Kind::Import),
            pull("K.dll", "K_NULL_THUNK_DATA", Kind::NullThunk),
            pull("U.dll", "c", Kind::Import),
            pull("U.dll", "U_NULL_THUNK_DATA", Kind::NullThunk),
            pull("K.dll", "d", Kind::Import),
            pull("K.dll", "e", Kind::Import),
        ];
        let runs = runs_of(&p);
        assert_eq!(runs[2].start, 4);
        let (range, hidden) = parse_run_spec("2=1..1,+5:u.dll=2..2/0..3", &runs, p.len()).unwrap();
        assert_eq!(range[2], (1, 1));
        assert_eq!(hidden[0].member, "U.dll");
        let ps = pieces(&runs, &hidden, &[0, 0, 1, 5, 2, 3]).unwrap();
        let shape: Vec<(&str, usize, usize, bool)> = ps
            .iter()
            .map(|q| (q.member, q.live.len(), q.dead, q.nt))
            .collect();
        assert_eq!(
            shape,
            vec![
                ("K.dll", 1, 0, true),
                ("U.dll", 1, 0, true),
                ("K.dll", 1, 1, false),
                ("U.dll", 0, 2, false),
                ("K.dll", 1, 3, false)
            ]
        );
        assert!(pieces(&runs, &hidden, &[0, 0, 1, 4, 2, 0]).is_some());
        assert!(pieces(&runs, &hidden, &[0, 0, 1, 4, 2, 3]).is_none());
        assert!(parse_run_spec("+1:X.dll=1..1", &runs, p.len()).is_err());
    }

    #[test]
    fn an_oversized_space_is_refused_before_it_is_walked() {
        let p: Vec<Pull> = (0..13)
            .flat_map(|i| {
                let m = if i % 2 == 0 { "K.dll" } else { "U.dll" };
                [
                    pull(m, &format!("x{i}"), Kind::Import),
                    pull(m, "NT", Kind::NullThunk),
                ]
            })
            .collect();
        let e = run_search(&p, Crt::Vc6, &Blocks::new(), "", None, false, 2).unwrap_err();
        assert!(format!("{e:#}").contains("points, more than"), "{e:#}");
    }
}
