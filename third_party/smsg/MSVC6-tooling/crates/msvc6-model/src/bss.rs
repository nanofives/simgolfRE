use crate::budget::Budget;
use anyhow::{Result, anyhow, bail};
use std::collections::BTreeMap;

pub const MAX_VARS: usize = 4096;

pub const ZERO_INIT_STEPS_PER_BYTE: u64 = 64;

pub const ZERO_INIT_FLOOR: u64 = 1 << 20;

pub const ZERO_INIT_STEPS: u64 = 1 << 32;

pub const NB: u32 = 1024;

#[must_use]
pub fn ident_hash(name: &str) -> u32 {
    name.bytes().fold(0u32, |h, c| {
        h.wrapping_mul(4)
            .wrapping_add(h >> 4)
            .wrapping_add(u32::from(c))
    })
}

#[must_use]
pub fn ident_key(name: &str) -> u32 {
    let h = ident_hash(name);
    (h >> 16) ^ (h & 0xffff)
}

#[must_use]
pub fn bucket_of(s: &str) -> u32 {
    ident_key(s) & 0x3ff
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Scope {
    Global,
    Local,
    Namespace,
    Member,
}

impl Scope {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::Global => "global",
            Self::Local => "local",
            Self::Namespace => "namespace",
            Self::Member => "member",
        }
    }
}

#[must_use]
pub fn classify(sym: &str) -> (String, String, Scope) {
    let s = sym
        .strip_prefix('_')
        .filter(|r| r.starts_with('?'))
        .unwrap_or(sym);
    let Some(body) = s.strip_prefix('?') else {
        let id = s.strip_prefix('_').unwrap_or(s).to_string();
        return (id.clone(), id, Scope::Global);
    };
    let ident = body.split('@').next().unwrap_or("").to_string();
    let rest = body.get(ident.len()..).unwrap_or_default();
    if rest.starts_with("@?") {
        return (ident, s.to_string(), Scope::Local);
    }
    if rest.starts_with("@@") {
        return (ident.clone(), ident, Scope::Global);
    }
    let scoped = rest.trim_start_matches('@');
    let (scopes, storage) = scoped.split_once("@@").unwrap_or((scoped, ""));
    if storage.starts_with('2') {
        return (ident, s.to_string(), Scope::Member);
    }
    let inner = scopes.split('@').next().unwrap_or("").to_string();
    (ident, inner, Scope::Namespace)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Var {
    pub symbol: String,
    pub scope: Scope,
    pub bucket: u32,
    pub offset: u32,
    pub size: u32,
    pub image: Option<u32>,
    pub foreign: bool,
    pub zero_init: Option<usize>,
}

fn at(b: &[u32], i: usize) -> u32 {
    b.get(i).copied().unwrap_or(0)
}

#[must_use]
pub fn longest_ordered(b: &[u32]) -> Vec<usize> {
    let n = b.len().min(MAX_VARS);
    let mut len = vec![1usize; n];
    let mut prev = vec![usize::MAX; n];
    for i in 0..n {
        let bi = at(b, i);
        for j in 0..i {
            let lj = len.get(j).copied().unwrap_or(0);
            if at(b, j) <= bi && lj + 1 > len.get(i).copied().unwrap_or(0) {
                if let Some(x) = len.get_mut(i) {
                    *x = lj + 1;
                }
                if let Some(x) = prev.get_mut(i) {
                    *x = j;
                }
            }
        }
    }
    let Some(mut cur) = (0..n).max_by_key(|&i| (len.get(i).copied(), std::cmp::Reverse(i))) else {
        return Vec::new();
    };
    let mut out = vec![cur];
    while let Some(&p) = prev.get(cur) {
        if p == usize::MAX {
            break;
        }
        cur = p;
        out.push(cur);
    }
    out.reverse();
    out
}

#[must_use]
pub fn intervals(b: &[u32], keep: &[usize]) -> Vec<(usize, u32, u32)> {
    let mut out = Vec::new();
    for i in 0..b.len() {
        if keep.contains(&i) {
            continue;
        }
        let lo = keep
            .iter()
            .filter(|&&k| k < i)
            .map(|&k| at(b, k))
            .max()
            .unwrap_or(0);
        let hi = keep
            .iter()
            .filter(|&&k| k > i)
            .map(|&k| at(b, k))
            .min()
            .unwrap_or(0x3ff);
        out.push((i, lo, hi));
    }
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Slot {
    Keep,
    Rename { lo: u32, hi: u32 },
}

#[must_use]
pub fn plan(b: &[u32], k: u32, pinned: &[bool]) -> Option<Vec<Slot>> {
    let n = b.len();
    if n > MAX_VARS || pinned.len() != n {
        return None;
    }
    let bucket = |t: usize| -> u32 {
        if t == 0 {
            0
        } else if t == n + 1 {
            0x3ff
        } else {
            at(b, t - 1)
        }
    };
    let pins_before: Vec<usize> = std::iter::once(0)
        .chain(pinned.iter().scan(0usize, |c, &p| {
            *c += usize::from(p);
            Some(*c)
        }))
        .collect();
    let pb = |i: usize| pins_before.get(i).copied().unwrap_or(0);
    let pins_between = |s: usize, t: usize| pb(t - 1).saturating_sub(pb(s));
    let width = |s: usize, t: usize| -> Option<u32> {
        let m = u32::try_from(t - s - 1).ok()?;
        if m == 0 {
            return Some(u32::MAX);
        }
        let (lo, hi) = (bucket(s), bucket(t));
        (lo <= hi).then(|| (hi - lo + 1) / m)
    };
    let mut best: Vec<Option<(usize, u32)>> = vec![None; n + 2];
    let mut prev = vec![0usize; n + 2];
    if let Some(x) = best.get_mut(0) {
        *x = Some((0, u32::MAX));
    }
    for t in 1..=n + 1 {
        for s in 0..t {
            let Some((kept, narrow)) = best.get(s).copied().flatten() else {
                continue;
            };
            if bucket(s) > bucket(t) || pins_between(s, t) > 0 {
                continue;
            }
            let Some(w) = width(s, t) else { continue };
            if w < k {
                continue;
            }
            let cand = (kept + usize::from(t <= n), narrow.min(w));
            if best.get(t).copied().flatten().is_none_or(|b| cand > b) {
                if let Some(x) = best.get_mut(t) {
                    *x = Some(cand);
                }
                if let Some(x) = prev.get_mut(t) {
                    *x = s;
                }
            }
        }
    }
    best.get(n + 1).copied().flatten()?;
    let mut anchors = vec![n + 1];
    let mut cur = n + 1;
    while cur != 0 {
        cur = prev.get(cur).copied().unwrap_or(0);
        anchors.push(cur);
    }
    anchors.reverse();
    let mut out = vec![Slot::Keep; n];
    for w in anchors.windows(2) {
        let [s, t] = *w else { continue };
        let m = t - s - 1;
        if m == 0 {
            continue;
        }
        let (lo, span) = (bucket(s), bucket(t) - bucket(s) + 1);
        let m32 = u32::try_from(m).ok()?;
        for j in 0..m32 {
            if let Some(x) = out.get_mut(s + j as usize) {
                *x = Slot::Rename {
                    lo: lo + j * span / m32,
                    hi: lo + (j + 1) * span / m32 - 1,
                };
            }
        }
    }
    Some(out)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Pick {
    Keep,
    Rename(usize),
    Unplaced,
}

pub const RENAME_COST: u64 = 1_000;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Pair {
    pub row: usize,
    pub opts: Vec<(Pick, Pick, u32)>,
}

#[derive(Clone, Copy)]
enum Step {
    One(Pick),
    Two(Pick, Pick),
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn assign(
    cur: &[u32],
    cands: &[Vec<(u32, u32)>],
    pinned: &[bool],
    rename: u64,
    pairs: &[Pair],
) -> Vec<Pick> {
    const N: usize = NB as usize;
    const UNPLACED: u64 = 1_000_000;
    const INF: u64 = u64::MAX / 4;
    let rows = cur.len().min(MAX_VARS);
    let pin = |i: usize| pinned.get(i).copied().unwrap_or(false);
    let cand_row = |i: usize| cands.get(i).map_or(&[][..], Vec::as_slice);
    let mut first: Vec<Option<&Pair>> = vec![None; rows];
    for p in pairs {
        if p.row + 1 < rows
            && let Some(x) = first.get_mut(p.row)
        {
            *x = Some(p);
        }
    }
    let bucket = |i: usize, p: Pick| match p {
        Pick::Keep => cur.get(i).map(|&c| c as usize).filter(|&b| b < N),
        Pick::Rename(k) => cand_row(i).get(k).map(|c| c.0 as usize).filter(|&b| b < N),
        Pick::Unplaced => None,
    };
    let get = |v: &[u64], s: usize| v.get(s).copied().unwrap_or(INF);
    let mut dp = vec![INF; 2 * N];
    if let Some(x) = dp.get_mut(1) {
        *x = 0;
    }
    let mut back: Vec<Vec<(usize, Step)>> = Vec::with_capacity(rows);
    let mut i = 0;
    while i < rows {
        let mut nx = vec![INF; 2 * N];
        let mut par = vec![(0usize, Step::One(Pick::Unplaced)); 2 * N];
        let mut below = vec![(INF, 0usize); N + 1];
        for x in 0..N {
            let (c0, c1) = (get(&dp, 2 * x), get(&dp, 2 * x + 1));
            let mut b = below.get(x).copied().unwrap_or((INF, 0));
            if c0 < b.0 {
                b = (c0, 2 * x);
            }
            if c1 < b.0 {
                b = (c1, 2 * x + 1);
            }
            if let Some(y) = below.get_mut(x + 1) {
                *y = b;
            }
        }
        let entry = |x: usize, kept: bool| {
            let (mut c, mut from) = below.get(x).copied().unwrap_or((INF, 0));
            if kept && get(&dp, 2 * x + 1) < c {
                (c, from) = (get(&dp, 2 * x + 1), 2 * x + 1);
            }
            (c, from)
        };
        let set = |nx: &mut Vec<u64>,
                   par: &mut Vec<(usize, Step)>,
                   to: usize,
                   c: u64,
                   p: (usize, Step)| {
            if c < get(nx, to) {
                if let Some(x) = nx.get_mut(to) {
                    *x = c;
                }
                if let Some(x) = par.get_mut(to) {
                    *x = p;
                }
            }
        };
        if let Some(p) = first.get(i).copied().flatten() {
            if !pin(i) && !pin(i + 1) {
                for s in 0..2 * N {
                    let d = get(&dp, s);
                    if d < INF {
                        set(
                            &mut nx,
                            &mut par,
                            s,
                            d + 2 * UNPLACED,
                            (s, Step::Two(Pick::Unplaced, Pick::Unplaced)),
                        );
                    }
                }
            }
            for &(px, py, cost) in &p.opts {
                let (Some(bx), Some(by)) = (bucket(i, px), bucket(i + 1, py)) else {
                    continue;
                };
                let (kx, ky) = (px == Pick::Keep, py == Pick::Keep);
                if (pin(i) && !kx) || (pin(i + 1) && !ky) || !(by > bx || (by == bx && kx && ky)) {
                    continue;
                }
                let (c, from) = entry(bx, kx);
                let renamed = u64::from(!kx) + u64::from(!ky);
                let c = c.saturating_add(renamed * rename + u64::from(cost));
                let to = 2 * by + usize::from(ky);
                set(&mut nx, &mut par, to, c, (from, Step::Two(px, py)));
            }
            dp = nx;
            back.push(vec![(0usize, Step::One(Pick::Unplaced)); 2 * N]);
            back.push(par);
            i += 2;
            continue;
        }
        if !pin(i) {
            for s in 0..2 * N {
                let d = get(&dp, s);
                if d < INF {
                    set(
                        &mut nx,
                        &mut par,
                        s,
                        d + UNPLACED,
                        (s, Step::One(Pick::Unplaced)),
                    );
                }
            }
        }
        if let Some(x) = cur.get(i).map(|&c| c as usize).filter(|&x| x < N) {
            let (c, from) = entry(x, true);
            if c < INF {
                set(
                    &mut nx,
                    &mut par,
                    2 * x + 1,
                    c,
                    (from, Step::One(Pick::Keep)),
                );
            }
        }
        if !pin(i) {
            for (j, &(xb, cost)) in cand_row(i).iter().enumerate() {
                let xb = xb as usize;
                if xb >= N {
                    continue;
                }
                let (c, from) = below.get(xb).copied().unwrap_or((INF, 0));
                let c = c.saturating_add(rename + u64::from(cost));
                set(
                    &mut nx,
                    &mut par,
                    2 * xb,
                    c,
                    (from, Step::One(Pick::Rename(j))),
                );
            }
        }
        dp = nx;
        back.push(par);
        i += 1;
    }
    let mut cur_s = (0..2 * N).min_by_key(|&s| get(&dp, s)).unwrap_or(1);
    let mut out = vec![Pick::Unplaced; cur.len()];
    let mut i = rows;
    while i > 0 {
        i -= 1;
        let Some(&(from, step)) = back.get(i).and_then(|b| b.get(cur_s)) else {
            break;
        };
        match step {
            Step::One(p) => {
                if let Some(x) = out.get_mut(i) {
                    *x = p;
                }
            }
            Step::Two(px, py) => {
                if let Some(x) = out.get_mut(i) {
                    *x = py;
                }
                if let Some(x) = i.checked_sub(1).and_then(|k| out.get_mut(k)) {
                    *x = px;
                }
                i = i.saturating_sub(1);
            }
        }
        cur_s = from;
    }
    out
}

#[must_use]
pub fn room(cur: &[u32], cands: &[Vec<(u32, u32)>], pinned: &[bool]) -> Vec<Option<(u32, u32)>> {
    let n = cur.len();
    let options = |i: usize| -> Vec<(u32, bool)> {
        let mut o: Vec<(u32, bool)> = Vec::new();
        if let Some(&c) = cur.get(i).filter(|&&c| c < NB) {
            o.push((c, true));
        }
        if !pinned.get(i).copied().unwrap_or(false) {
            o.extend(cands.get(i).into_iter().flatten().map(|c| (c.0, false)));
        }
        o
    };
    let follows = |a: (u32, bool), b: (u32, bool)| b.0 > a.0 || (b.0 == a.0 && a.1 && b.1);
    let mut low: Vec<Option<(u32, bool)>> = Vec::with_capacity(n);
    let mut atv = Some((0, true));
    for i in 0..n {
        atv = atv.and_then(|a| {
            options(i)
                .into_iter()
                .filter(|&o| follows(a, o))
                .min_by_key(|&(b, k)| (b, !k))
        });
        low.push(atv);
    }
    let mut high: Vec<Option<(u32, bool)>> = vec![None; n];
    let mut atn: Option<Option<(u32, bool)>> = Some(None);
    for i in (0..n).rev() {
        atn = atn.and_then(|next| {
            options(i)
                .into_iter()
                .filter(|&o| next.is_none_or(|nx| follows(o, nx)))
                .max_by_key(|&(b, k)| (b, k))
                .map(Some)
        });
        if let Some(x) = high.get_mut(i) {
            *x = atn.flatten();
        }
    }
    (0..n)
        .map(|j| {
            let lo = if j == 0 {
                Some(1)
            } else {
                low.get(j - 1).copied().flatten().map(|l| l.0 + 1)
            }?;
            let hi = if j + 1 == n {
                Some(NB - 1)
            } else {
                high.get(j + 1)
                    .copied()
                    .flatten()
                    .map(|h| h.0.saturating_sub(1))
            }?;
            (lo <= hi).then_some((lo, hi))
        })
        .collect()
}

pub fn vars_of(obj_bytes: &[u8], report: Option<&crate::dataseat::Report>) -> Result<Vec<Var>> {
    let Some(bss) = crate::dataseat::bss_of(obj_bytes)? else {
        return Ok(Vec::new());
    };
    if bss.symbols.len() > MAX_VARS {
        bail!(
            ".bss holds {} symbols, more than the {MAX_VARS} the order model reads",
            bss.symbols.len()
        );
    }
    let syms = &bss.symbols;
    let mut sites: Vec<(u32, u32)> = report.map(|r| r.bss_sites.clone()).unwrap_or_default();
    sites.sort_unstable();
    let mut out = Vec::new();
    for (i, (off, name)) in syms.iter().enumerate() {
        let end = syms.get(i + 1).map_or(bss.size, |n| n.0);
        let (_, hashed, scope) = classify(name);
        let mut votes: BTreeMap<u32, usize> = BTreeMap::new();
        let hi = end.max(off.saturating_add(1));
        let from = sites.partition_point(|s| s.0 < *off);
        let to = sites.partition_point(|s| s.0 < hi);
        for &(o, rva) in sites.get(from..to).unwrap_or_default() {
            *votes.entry(rva.wrapping_sub(o - off)).or_default() += 1;
        }
        let image = votes
            .iter()
            .max_by(|a, b| a.1.cmp(b.1).then(b.0.cmp(a.0)))
            .map(|(k, _)| *k);
        out.push(Var {
            symbol: name.clone(),
            scope,
            bucket: bucket_of(&hashed),
            offset: *off,
            size: end.wrapping_sub(*off),
            image,
            foreign: false,
            zero_init: None,
        });
    }
    let placed: Vec<u32> = out.iter().filter_map(|v| v.image).collect();
    let inside = |s: u32, r: u32| r >= s && r - s < bss.size;
    if let Some(start) = placed.iter().copied().max_by_key(|&s| {
        (
            placed.iter().filter(|&&r| inside(s, r)).count(),
            std::cmp::Reverse(s),
        )
    }) {
        for v in &mut out {
            v.foreign = v.image.is_some_and(|r| !inside(start, r));
        }
    }
    Ok(out)
}

#[must_use]
pub fn strip_comments(t: &str) -> String {
    let b = t.as_bytes();
    let mut out = Vec::with_capacity(b.len());
    let mut i = 0;
    while let Some(&c) = b.get(i) {
        if c == b'/' && b.get(i + 1) == Some(&b'*') {
            let end = t
                .get(i + 2..)
                .and_then(|r| r.find("*/"))
                .map_or(b.len(), |e| i + 2 + e + 2);
            out.extend(std::iter::repeat_n(b' ', end - i));
            i = end;
        } else if c == b'/' && b.get(i + 1) == Some(&b'/') {
            let end = t
                .get(i..)
                .and_then(|r| r.find('\n'))
                .map_or(b.len(), |e| i + e);
            out.extend(std::iter::repeat_n(b' ', end - i));
            i = end;
        } else {
            out.push(c);
            i += 1;
        }
    }
    msvc6_base::srcio::tool_text(&out)
}

#[must_use]
pub fn is_zero_init(init: &str) -> bool {
    let t: String = init.chars().filter(|c| !c.is_whitespace()).collect();
    let t = t.trim_start_matches('{').trim_end_matches('}');
    t.split(',').all(|x| {
        let x = x.trim_matches(|c| c == '{' || c == '}');
        x.is_empty()
            || matches!(
                x,
                "0" | "0L"
                    | "0l"
                    | "0u"
                    | "0U"
                    | "0UL"
                    | "0x0"
                    | "0x00"
                    | "0x00000000"
                    | "0.0"
                    | "0.0f"
                    | "0.f"
                    | "0.0F"
                    | "NULL"
                    | "false"
                    | "FALSE"
                    | "'\\0'"
            )
    })
}

#[must_use]
pub fn is_brace_init(init: &str) -> bool {
    init.trim_start().starts_with('{')
}

const CALLING_CONVENTIONS: [&str; 9] = [
    "__cdecl",
    "_cdecl",
    "__stdcall",
    "_stdcall",
    "__fastcall",
    "_fastcall",
    "__pascal",
    "WINAPI",
    "CALLBACK",
];

#[must_use]
pub fn initialised_at(src: &str, ident: &str) -> Option<usize> {
    let bud = Budget::per_item(src.len(), ZERO_INIT_STEPS_PER_BYTE, ZERO_INIT_FLOOR);
    initialised_within(src, ident, &bud).filter(|_| bud.over().is_none())
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn initialised_within(src: &str, ident: &str, bud: &Budget) -> Option<usize> {
    const NOT_TYPES: [&str; 9] = [
        "else", "return", "do", "case", "goto", "throw", "delete", "new", "sizeof",
    ];
    if ident.is_empty() {
        return None;
    }
    let b = src.as_bytes();
    let byte = |i: usize| b.get(i).copied().unwrap_or(0);
    let word = |c: u8| c.is_ascii_alphanumeric() || c == b'_';
    let text = |a: usize, z: usize| src.get(a..z).unwrap_or_default();
    let mut from = 0;
    while let Some(k) = src.get(from..).and_then(|r| r.find(ident)) {
        if !bud.spend(k.saturating_add(ident.len())) {
            return None;
        }
        let at = from + k;
        from = at + ident.len();
        if at > 0 && word(byte(at - 1)) || b.get(from).is_some_and(|&c| word(c)) {
            continue;
        }
        let mut start = at;
        while bud.tick() && start >= 2 && byte(start - 1) == b':' && byte(start - 2) == b':' {
            let mut w = start - 2;
            while bud.tick() && w > 0 && word(byte(w - 1)) {
                w -= 1;
            }
            if w == start - 2 {
                break;
            }
            start = w;
        }
        let mut p = start;
        while bud.tick() && p > 0 && (byte(p - 1) == b' ' || byte(p - 1) == b'\t') {
            p -= 1;
        }
        if p == start && !(p > 0 && byte(p - 1) == b'*') || p == 0 {
            continue;
        }
        let mut starred = false;
        while bud.tick() && p > 0 && matches!(byte(p - 1), b'*' | b'&' | b' ' | b'\t') {
            starred |= byte(p - 1) == b'*';
            p -= 1;
        }
        if starred && p > 0 && word(byte(p - 1)) {
            let mut w = p;
            while bud.tick() && w > 0 && word(byte(w - 1)) {
                w -= 1;
            }
            let mut q = w;
            while bud.tick() && q > 0 && (byte(q - 1) == b' ' || byte(q - 1) == b'\t') {
                q -= 1;
            }
            if CALLING_CONVENTIONS.contains(&text(w, p)) && q > 0 && byte(q - 1) == b'(' {
                p = q;
            }
        }
        let fnptr = starred && p > 0 && byte(p - 1) == b'(';
        if fnptr {
            p -= 1;
            while bud.tick() && p > 0 && (byte(p - 1) == b' ' || byte(p - 1) == b'\t') {
                p -= 1;
            }
        }
        if p == 0 {
            continue;
        }
        let prev = byte(p - 1);
        if !word(prev) {
            continue;
        }
        let mut w = p;
        while bud.tick() && w > 0 && word(byte(w - 1)) {
            w -= 1;
        }
        if NOT_TYPES.contains(&text(w, p)) {
            continue;
        }
        let mut after = from;
        if fnptr {
            let mut q = after;
            while bud.tick() && q < b.len() && byte(q).is_ascii_whitespace() {
                q += 1;
            }
            if b.get(q) != Some(&b')') {
                continue;
            }
            q += 1;
            while bud.tick() && q < b.len() && byte(q).is_ascii_whitespace() {
                q += 1;
            }
            if b.get(q) == Some(&b'(') {
                let mut depth = 0i64;
                while bud.tick() && q < b.len() {
                    match byte(q) {
                        b'(' => depth += 1,
                        b')' => depth -= 1,
                        _ => {}
                    }
                    q += 1;
                    if depth == 0 {
                        break;
                    }
                }
            }
            after = q;
        }
        let mut q = after;
        loop {
            if !bud.tick() {
                return None;
            }
            while bud.tick() && q < b.len() && byte(q).is_ascii_whitespace() {
                q += 1;
            }
            if b.get(q) == Some(&b'[') {
                q = src
                    .get(q..)
                    .and_then(|r| r.find(']'))
                    .map_or(b.len(), |e| q + e + 1);
                continue;
            }
            break;
        }
        if b.get(q) != Some(&b'=') || b.get(q + 1) == Some(&b'=') {
            continue;
        }
        let mut depth = 0i64;
        let mut e = q + 1;
        while bud.tick() && e < b.len() {
            match byte(e) {
                b'{' | b'(' => depth += 1,
                b'}' | b')' => depth -= 1,
                b';' => break,
                b',' if depth == 0 => break,
                _ => {}
            }
            e += 1;
        }
        let init = text(q + 1, e);
        if is_zero_init(init) || is_brace_init(init) {
            return Some(at);
        }
    }
    None
}

pub fn mark_zero_inits(text: &str, vars: &mut [Var]) -> Result<()> {
    let text = strip_comments(text);
    let n = u64::try_from(text.len().saturating_mul(vars.len().max(1))).unwrap_or(u64::MAX);
    let steps = n
        .saturating_mul(ZERO_INIT_STEPS_PER_BYTE)
        .saturating_add(ZERO_INIT_FLOOR)
        .min(ZERO_INIT_STEPS);
    let bud = Budget::new(steps, crate::budget::NESTING);
    for v in vars.iter_mut() {
        let ident = classify(&v.symbol).0;
        v.zero_init = initialised_within(&text, &ident, &bud);
    }
    if let Some(why) = bud.over() {
        bail!(
            "the `= 0` scan stopped at {why} ({} steps over {} byte(s) and {} variable(s))",
            bud.spent(),
            text.len(),
            vars.len()
        );
    }
    Ok(())
}

fn var(vars: &[Var], i: usize) -> Option<&Var> {
    vars.get(i)
}

fn bucket_at(vars: &[Var], i: usize) -> u32 {
    var(vars, i).map_or(0, |v| v.bucket)
}

fn walked_of(vars: &[Var], i: usize) -> bool {
    var(vars, i).is_some_and(|v| v.zero_init.is_none())
}

#[must_use]
pub fn predicted(vars: &[Var], given: &[usize]) -> Vec<usize> {
    let mut walked: Vec<usize> = given
        .iter()
        .copied()
        .filter(|&i| walked_of(vars, i))
        .collect();
    walked.sort_by_key(|&i| bucket_at(vars, i));
    let mut zero: Vec<usize> = given
        .iter()
        .copied()
        .filter(|&i| var(vars, i).is_some_and(|v| v.zero_init.is_some()))
        .collect();
    zero.sort_by_key(|&i| var(vars, i).and_then(|v| v.zero_init));
    walked.extend(zero);
    walked
}

pub const GUARD_MAX: u32 = 9999;

#[must_use]
pub fn guard_number(sym: &str) -> Option<u32> {
    classify(sym).0.strip_prefix("$S")?.parse().ok()
}

#[must_use]
pub fn guard_numbers_in(lo: u32, hi: u32, max: u32) -> Vec<u32> {
    (1..=max)
        .filter(|n| (lo..=hi).contains(&bucket_of(&format!("$S{n}"))))
        .collect()
}

#[must_use]
pub fn compact<T: Copy + Into<i64>>(ns: &[T], limit: usize) -> String {
    let mut runs: Vec<(i64, i64)> = Vec::new();
    for n in ns.iter().map(|&n| n.into()) {
        match runs.last_mut() {
            Some(r) if r.1 + 1 == n => r.1 = n,
            _ => runs.push((n, n)),
        }
    }
    let mut out: Vec<String> = runs
        .iter()
        .take(limit)
        .map(|&(a, b)| {
            if a == b {
                a.to_string()
            } else {
                format!("{a}..={b}")
            }
        })
        .collect();
    if runs.len() > limit {
        out.push(format!("... ({} runs)", runs.len()));
    }
    out.join(" ")
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GuardFit {
    pub var: usize,
    pub n: u32,
    pub lo: u32,
    pub hi: u32,
    pub fits: Vec<u32>,
}

impl GuardFit {
    #[must_use]
    pub fn nearest_shift(&self) -> Option<i64> {
        self.fits
            .iter()
            .map(|&f| i64::from(f) - i64::from(self.n))
            .min_by_key(|&d| (d.unsigned_abs(), d < 0))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Verdict {
    pub stem: String,
    pub vars: Vec<Var>,
    pub model_exact: bool,
    pub image_order: Vec<usize>,
    pub image_exact: bool,
    pub misplaced: Vec<(usize, u32, u32)>,
    pub appended: Option<(usize, usize)>,
    pub inverted: Vec<(usize, usize)>,
    pub out_of_order: usize,
}

impl Verdict {
    fn walked(&self) -> Vec<usize> {
        self.image_order
            .iter()
            .copied()
            .filter(|&i| walked_of(&self.vars, i))
            .collect()
    }

    fn v(&self, i: usize) -> Option<&Var> {
        self.vars.get(i)
    }
}

#[must_use]
pub fn guard_fits(v: &Verdict) -> Vec<GuardFit> {
    let walked = v.walked();
    let mut out = Vec::new();
    for (k, &i) in walked.iter().enumerate() {
        let Some(n) = v.v(i).and_then(|x| guard_number(&x.symbol)) else {
            continue;
        };
        let others: Vec<usize> = (0..walked.len()).filter(|&j| j != k).collect();
        let wb = |j: usize| walked.get(j).map_or(0, |&w| bucket_at(&v.vars, w));
        let ob: Vec<u32> = others.iter().map(|&j| wb(j)).collect();
        let keep: Vec<usize> = longest_ordered(&ob)
            .into_iter()
            .filter_map(|x| others.get(x).copied())
            .collect();
        let lo = keep
            .iter()
            .filter(|&&j| j < k)
            .map(|&j| wb(j))
            .max()
            .unwrap_or(0);
        let hi = keep
            .iter()
            .filter(|&&j| j > k)
            .map(|&j| wb(j))
            .min()
            .unwrap_or(0x3ff);
        out.push(GuardFit {
            var: i,
            n,
            lo,
            hi,
            fits: guard_numbers_in(lo, hi, GUARD_MAX),
        });
    }
    out
}

#[must_use]
pub fn misplaced_with_free_guards(v: &Verdict) -> usize {
    let nb: Vec<u32> = v
        .image_order
        .iter()
        .filter(|&&i| {
            v.v(i)
                .is_some_and(|x| x.zero_init.is_none() && guard_number(&x.symbol).is_none())
        })
        .map(|&i| bucket_at(&v.vars, i))
        .collect();
    let unseated = guard_fits(v).iter().filter(|g| g.fits.is_empty()).count();
    nb.len() - longest_ordered(&nb).len() + unseated
}

#[must_use]
pub fn guard_reading(g: &GuardFit) -> String {
    if g.fits.binary_search(&g.n).is_ok() {
        return "our number fits".to_string();
    }
    match g.nearest_shift() {
        None => format!("NO number up to {GUARD_MAX} fits: the neighbours' names are what differ"),
        Some(d) if d > 0 => format!(
            "needs >= {d} MORE numbers ahead of it: {d} in-class static data member declaration(s) in its \
             headers, or {} file-scope dynamic-init object(s), or {d} function(s) with a local static, or {d} \
             `new T[n]` of a constructed T / anonymous union object(s), or a mix",
            (d + 4) / 5
        ),
        Some(d) => format!(
            "needs {} FEWER numbers before it: the original declared less ahead of it",
            d.unsigned_abs()
        ),
    }
}

#[must_use]
pub fn common_run(a: &[usize], b: &[usize]) -> usize {
    let mut row = vec![0usize; b.len() + 1];
    for &x in a {
        let mut diag = 0;
        for (j, &y) in b.iter().enumerate() {
            let up = row.get(j + 1).copied().unwrap_or(0);
            let left = row.get(j).copied().unwrap_or(0);
            if let Some(r) = row.get_mut(j + 1) {
                *r = if x == y { diag + 1 } else { up.max(left) };
            }
            diag = up;
        }
    }
    row.last().copied().unwrap_or(0)
}

#[must_use]
pub fn same_bucket_inversions(vars: &[Var], order: &[usize]) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    for (k, &a) in order.iter().enumerate() {
        for &b in order.get(k + 1..).unwrap_or_default() {
            let (Some(x), Some(y)) = (var(vars, a), var(vars, b)) else {
                continue;
            };
            if x.bucket == y.bucket && y.offset < x.offset {
                out.push((a, b));
            }
        }
    }
    out
}

pub fn judge(stem: &str, vars: Vec<Var>) -> Result<Verdict> {
    if vars.len() > MAX_VARS {
        return Err(anyhow!(
            "{stem}: {} .bss variables, more than the {MAX_VARS} the order model reads",
            vars.len()
        ));
    }
    let all: Vec<usize> = (0..vars.len()).collect();
    let model_exact = predicted(&vars, &all) == all;
    let mut image_order: Vec<usize> = (0..vars.len())
        .filter(|&i| var(&vars, i).is_some_and(|v| v.image.is_some() && !v.foreign))
        .collect();
    image_order.sort_by_key(|&i| var(&vars, i).and_then(|v| v.image));
    let image_exact = predicted(&vars, &image_order) == image_order;
    let walked: Vec<usize> = image_order
        .iter()
        .copied()
        .filter(|&i| walked_of(&vars, i))
        .collect();
    let ib: Vec<u32> = walked.iter().map(|&i| bucket_at(&vars, i)).collect();
    let keep = longest_ordered(&ib);
    let misplaced: Vec<(usize, u32, u32)> = intervals(&ib, &keep)
        .into_iter()
        .filter_map(|(k, lo, hi)| Some((*walked.get(k)?, lo, hi)))
        .collect();
    let n = image_order.len();
    let zero = |i: usize| var(&vars, i).is_some_and(|v| v.zero_init.is_some());
    let appended = (1..n)
        .map(|k| {
            let head_ix = image_order.get(..n - k).unwrap_or_default();
            let tail_ix = image_order.get(n - k..).unwrap_or_default();
            let head: Vec<u32> = head_ix
                .iter()
                .filter(|&&i| !zero(i))
                .map(|&i| bucket_at(&vars, i))
                .collect();
            let renames = head.len() - longest_ordered(&head).len();
            let wrongly_zero = head_ix.iter().filter(|&&i| zero(i)).count();
            let claims = tail_ix.iter().filter(|&&i| !zero(i)).count();
            (k, renames + wrongly_zero + claims, renames)
        })
        .filter(|&(_, c, r)| {
            !misplaced.is_empty() && (c < misplaced.len() || c == misplaced.len() && r == 0)
        })
        .min_by_key(|&(k, c, _)| (c, k))
        .map(|(k, c, _)| (k, c));
    let inverted = same_bucket_inversions(&vars, &walked);
    let mut ours = image_order.clone();
    ours.sort_by_key(|&i| var(&vars, i).map_or(0, |v| v.offset));
    let out_of_order = image_order.len() - common_run(&image_order, &ours);
    Ok(Verdict {
        stem: stem.to_string(),
        vars,
        model_exact,
        image_order,
        image_exact,
        misplaced,
        appended,
        inverted,
        out_of_order,
    })
}

#[must_use]
pub fn names_table(names: &str) -> Vec<String> {
    let mut rows: Vec<(u32, usize, String, String, Scope)> = names
        .split([',', ' '])
        .filter(|n| !n.is_empty())
        .enumerate()
        .map(|(i, n)| {
            let (_, hashed, scope) = classify(n);
            (bucket_of(&hashed), i, n.to_string(), hashed, scope)
        })
        .collect();
    rows.sort_by(|a, b| a.0.cmp(&b.0).then(b.1.cmp(&a.1)));
    let mut out = vec![
        "bucket  key16   hashed-as         name   (predicted .bss order; a tie is later-declared first)"
            .to_string(),
    ];
    for (b, _, n, h, s) in rows {
        out.push(format!("{b:>6}  {:>5}  {:<9}  {n}", ident_key(&h), s.tag()));
    }
    out
}

fn rva_text(r: Option<u32>) -> String {
    r.map_or_else(|| "?".to_string(), |r| format!("{r:#x}"))
}

#[must_use]
pub fn inversion_reading(v: &Verdict, a: usize, b: usize) -> String {
    let (Some(x), Some(y)) = (v.v(a), v.v(b)) else {
        return String::new();
    };
    format!(
        "SAME-BUCKET ORDER, bucket {}: the reference puts {} ({}) before {} ({}), ours the other way; \
         a chain walks latest-declared first, so the original declared {} AFTER {}",
        x.bucket,
        x.symbol,
        rva_text(x.image),
        y.symbol,
        rva_text(y.image),
        classify(&x.symbol).0,
        classify(&y.symbol).0
    )
}

#[must_use]
pub fn at_image_offset(v: &Verdict) -> (Option<u32>, Vec<bool>) {
    let mut votes: BTreeMap<u32, usize> = BTreeMap::new();
    for &i in &v.image_order {
        if let Some(x) = v.v(i)
            && let Some(r) = x.image
        {
            *votes.entry(r.wrapping_sub(x.offset)).or_default() += 1;
        }
    }
    let base = votes
        .iter()
        .max_by_key(|(b, n)| (**n, std::cmp::Reverse(**b)))
        .map(|(b, _)| *b);
    let at = v
        .vars
        .iter()
        .map(|x| base.is_some_and(|b| x.image.is_some_and(|r| r.wrapping_sub(x.offset) == b)))
        .collect();
    (base, at)
}

#[must_use]
pub fn exact_spans(v: &Verdict, at: &[bool]) -> Vec<(usize, usize)> {
    let mut out: Vec<(usize, usize)> = Vec::new();
    for (k, &i) in v.image_order.iter().enumerate() {
        if !at.get(i).copied().unwrap_or(false) {
            continue;
        }
        match out.last_mut() {
            Some(last) if last.1 + 1 == k => last.1 = k,
            _ => out.push((k, k)),
        }
    }
    out
}

#[must_use]
pub fn verdict_lines(v: &Verdict) -> Vec<String> {
    let mut out = Vec::new();
    out.push(format!(
        "{}: {} var(s), model {}, reference {}",
        v.stem,
        v.vars.len(),
        if v.model_exact { "EXACT" } else { "DIFFERS" },
        if v.image_order.len() < 2 {
            "n/a".to_string()
        } else if v.image_exact {
            "EXACT".to_string()
        } else {
            format!(
                "DIFFERS ({} misplaced; {} of {} out of image order)",
                v.misplaced.len(),
                v.out_of_order,
                v.image_order.len()
            )
        }
    ));
    if let Some((k, c)) = v.appended {
        out.push(format!(
            "  OR: the image's last {k} variable(s) were `= 0` in the original and appended in definition order \
             ({c} claim(s) against {} rename(s))",
            v.misplaced.len()
        ));
    }
    let walked = v.walked();
    let breaks: Vec<String> = walked
        .windows(2)
        .filter_map(|w| match *w {
            [a, b] if bucket_at(&v.vars, b) < bucket_at(&v.vars, a) => {
                v.v(b).and_then(|x| x.image).map(|r| format!("{r:#x}"))
            }
            _ => None,
        })
        .collect();
    if !breaks.is_empty() {
        out.push(format!(
            "  {} descent(s) in image order, at {}: each is a rename OR a TU boundary the image has and we do not",
            breaks.len(),
            breaks.join(" ")
        ));
    }
    for &(a, b) in &v.inverted {
        out.push(format!("  {}", inversion_reading(v, a, b)));
    }
    for g in guard_fits(v) {
        out.push(format!(
            "  GUARD {}: ours $S{}, its place needs buckets {}..={}: $S n in {{{}}}; {}",
            v.v(g.var).map_or("", |x| x.symbol.as_str()),
            g.n,
            g.lo,
            g.hi,
            compact(&g.fits, 8),
            guard_reading(&g)
        ));
    }
    let (base, at) = at_image_offset(v);
    if let Some(base) = base {
        let spans = exact_spans(v, &at);
        out.push(format!(
            "  AT IMAGE OFFSET: {}/{} placed variable(s) sit where the reference has them (base {base:#x}, the \
             plurality of image - ours); {} exact span(s): {}",
            at.iter().filter(|&&a| a).count(),
            v.image_order.len(),
            spans.len(),
            spans
                .iter()
                .map(|(a, b)| format!("{}..={}", a + 1, b + 1))
                .collect::<Vec<_>>()
                .join(" ")
        ));
    }
    out.push(
        "  row image-rva  ours    size   bucket     shift  scope      symbol   (`=`: at the image offset; shift: \
         ours minus the image's offset)"
            .to_string(),
    );
    let mut shown: Vec<usize> = v.image_order.clone();
    shown.extend((0..v.vars.len()).filter(|i| !v.image_order.contains(i)));
    for i in shown {
        let Some(x) = v.v(i) else { continue };
        let foreign = if x.foreign {
            "   (FOREIGN: outside this TU's extent in the image)"
        } else {
            ""
        };
        let mark = v
            .misplaced
            .iter()
            .find(|m| m.0 == i)
            .map_or(String::new(), |(_, lo, hi)| {
                format!("   <- MISPLACED: name must hash into buckets {lo}..={hi}")
            });
        let zero = if x.zero_init.is_some() {
            "   (= 0: appended in definition order)"
        } else {
            ""
        };
        let row = v
            .image_order
            .iter()
            .position(|&k| k == i)
            .map_or_else(|| "   ".to_string(), |k| format!("{:>3}", k + 1));
        let shift = match (base, x.image) {
            (Some(b), Some(r)) => {
                let d = i64::from(x.offset) - i64::from(r.wrapping_sub(b));
                if d == 0 {
                    "0".to_string()
                } else {
                    format!("{}{:#x}", if d < 0 { "-" } else { "+" }, d.unsigned_abs())
                }
            }
            _ => "-".to_string(),
        };
        out.push(format!(
            "{row}{} {:<9}  +{:<5x}  {:>5}  {:>6}  {shift:>8}  {:<9}  {}{mark}{foreign}{zero}",
            if at.get(i).copied().unwrap_or(false) {
                "="
            } else {
                " "
            },
            x.image
                .map_or_else(|| "unseated".to_string(), |r| format!("{r:#x}")),
            x.offset,
            x.size,
            x.bucket,
            x.scope.tag(),
            x.symbol
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bucket_ordered(b: &[u32]) -> bool {
        b.windows(2).all(|w| matches!(*w, [x, y] if x <= y))
    }

    #[test]
    fn the_hash_matches_the_compiler_on_measured_toys() {
        let b: Vec<u32> = ["aaa", "bbb", "ccc", "AB", "BA"]
            .iter()
            .map(|n| bucket_of(n))
            .collect();
        assert_eq!(b, vec![43, 65, 86, 330, 333]);
        assert!(bucket_ordered(&b));
        for n in ["aiy", "aju", "akq", "alm", "ami", "ane"] {
            assert_eq!(ident_key(n), 2148);
        }
        assert!(bucket_of("eNN") < bucket_of("C"));
    }

    #[test]
    fn the_hashed_string_follows_the_scope() {
        assert_eq!(classify("?g_x@@3HA").2, Scope::Global);
        assert_eq!(classify("_s_x").1, "s_x");
        let (id, h, s) = classify("_?cnt@?1??f@@YAHXZ@4HA");
        assert_eq!(
            (id.as_str(), h.as_str(), s),
            ("cnt", "?cnt@?1??f@@YAHXZ@4HA", Scope::Local)
        );
        let (_, h, s) = classify("?m@Cls@@2HA");
        assert_eq!((h.as_str(), s), ("?m@Cls@@2HA", Scope::Member));
        let (_, h, s) = classify("?v@NS@@3HA");
        assert_eq!((h.as_str(), s), ("NS", Scope::Namespace));
    }

    #[test]
    fn a_misplaced_name_gets_the_interval_of_its_kept_neighbours() {
        let b = [10, 500, 20, 30, 40];
        let keep = longest_ordered(&b);
        assert_eq!(keep, vec![0, 2, 3, 4]);
        assert_eq!(intervals(&b, &keep), vec![(1, 10, 20)]);
    }

    #[test]
    fn out_of_order_counts_what_the_longest_common_run_leaves() {
        assert_eq!(common_run(&[0, 1, 2, 3], &[0, 1, 2, 3]), 4);
        assert_eq!(common_run(&[0, 1, 2, 3], &[3, 0, 1, 2]), 3);
        assert_eq!(common_run(&[0, 1, 2, 3], &[3, 2, 1, 0]), 1);
        assert_eq!(common_run(&[], &[]), 0);
    }

    #[test]
    fn a_dynamic_aggregate_is_appended_and_a_dynamic_scalar_is_walked() {
        let src = "int arr[4] = { f(1), 168, f(2), 40 };\nint scal = f(3);\nint g_x = 0;\n\
                   P g_w = { f(4), 5 };\nP g_v = mkP();\nC g_y[3];\nint g_z[2] = { 0, f(3) };\n";
        for appended in ["arr", "g_x", "g_w", "g_z"] {
            assert!(initialised_at(src, appended).is_some(), "{appended}");
        }
        for walked in ["scal", "g_v", "g_y"] {
            assert_eq!(initialised_at(src, walked), None, "{walked}");
        }
    }

    #[test]
    fn a_zero_initialiser_is_found_on_qualified_and_function_pointer_declarators() {
        let src = "Frame *Button::s_defaultFrames[3] = {0};\n\
                   void (*g_beforeHook)() = 0;\n\
                   int g_plain[4] = {0};\n\
                   int *p; void f() { *p = 0; x.y = 0; }\n";
        assert!(initialised_at(src, "s_defaultFrames").is_some());
        assert!(initialised_at(src, "g_beforeHook").is_some());
        assert!(initialised_at(src, "g_plain").is_some());
        assert_eq!(initialised_at(src, "p"), None);
        assert_eq!(initialised_at(src, "y"), None);
        assert_eq!(initialised_at("void f() { Cls::s_n = 0; }", "s_n"), None);
        let cc = "void (__stdcall *g_afterHook)() = 0;\n\
                  void (__cdecl *g_keyHook)(int key) = 0;\n\
                  void (__cdecl *g_downHook)(int, int) = 0;\n\
                  void (__cdecl *g_walkedHook)(int, int);\n";
        assert!(initialised_at(cc, "g_afterHook").is_some());
        assert!(initialised_at(cc, "g_keyHook").is_some());
        assert!(initialised_at(cc, "g_downHook").is_some());
        assert_eq!(initialised_at(cc, "g_walkedHook"), None);
        assert_eq!(initialised_at("void f() { (*g_hook)(); }", "g_hook"), None);
    }

    #[test]
    fn a_plan_drops_an_anchor_to_widen_the_gap_it_closes() {
        let b = [10, 500, 20, 30, 40];
        let none = [false; 5];
        assert_eq!(
            plan(&b, 1, &none),
            Some(vec![
                Slot::Keep,
                Slot::Rename { lo: 10, hi: 20 },
                Slot::Keep,
                Slot::Keep,
                Slot::Keep
            ])
        );
        let p = plan(&b, 12, &none).unwrap_or_default();
        assert_eq!(
            p,
            vec![
                Slot::Keep,
                Slot::Keep,
                Slot::Rename { lo: 500, hi: 673 },
                Slot::Rename { lo: 674, hi: 848 },
                Slot::Rename { lo: 849, hi: 1023 }
            ]
        );
        let mut pin = none;
        if let Some(x) = pin.get_mut(1) {
            *x = true;
        }
        assert_eq!(
            plan(&b, 1, &pin).map(|p| p.get(1).copied()),
            Some(Some(Slot::Keep))
        );
        assert_eq!(plan(&[50, 40], 1, &[true, true]), None);
    }

    #[test]
    fn an_assignment_renames_the_fewest_and_leaves_the_unfittable_unplaced() {
        let cur = [10, 500, 20, 30, 40];
        let mut cands = vec![Vec::new(); 5];
        if let Some(c) = cands.get_mut(1) {
            *c = vec![(700, 0), (15, 1)];
        }
        let free = [false; 5];
        assert_eq!(
            assign(&cur, &cands, &free, RENAME_COST, &[]),
            vec![
                Pick::Keep,
                Pick::Rename(1),
                Pick::Keep,
                Pick::Keep,
                Pick::Keep
            ]
        );
        if let Some(c) = cands.get_mut(1) {
            *c = vec![(700, 0)];
        }
        assert_eq!(
            assign(&cur, &cands, &free, RENAME_COST, &[]).get(1),
            Some(&Pick::Unplaced)
        );
        let cur = [10, 10, 5];
        let cands = vec![Vec::new(), Vec::new(), vec![(10, 0), (11, 0)]];
        assert_eq!(
            assign(&cur, &cands, &[false; 3], RENAME_COST, &[]),
            vec![Pick::Keep, Pick::Keep, Pick::Rename(1)]
        );
        let picks = assign(
            &[10, 500, 20],
            &[vec![], vec![(15, 0)], vec![]],
            &[false, true, false],
            RENAME_COST,
            &[],
        );
        assert_eq!(picks.get(1), Some(&Pick::Keep));
        assert_eq!(picks.get(2), Some(&Pick::Unplaced));
        let picks = assign(
            &[10, u32::MAX, 30],
            &[vec![], vec![(20, 5)], vec![]],
            &[false; 3],
            RENAME_COST,
            &[],
        );
        assert_eq!(picks, vec![Pick::Keep, Pick::Rename(0), Pick::Keep]);
        let (cur, cands) = ([30, u32::MAX], [vec![(10, 0)], vec![(20, 0), (40, 9)]]);
        assert_eq!(
            assign(&cur, &cands, &[false; 2], RENAME_COST, &[]),
            vec![Pick::Keep, Pick::Rename(1)]
        );
        assert_eq!(
            assign(&cur, &cands, &[false; 2], 1, &[]),
            vec![Pick::Rename(0), Pick::Rename(0)]
        );
    }

    #[test]
    fn a_pair_takes_one_joint_option_with_the_rest() {
        let cur = [u32::MAX, u32::MAX, 30];
        let cands = [vec![(10, 0), (20, 0)], vec![(12, 0), (25, 0)], vec![]];
        let pairs = [Pair {
            row: 0,
            opts: vec![
                (Pick::Rename(1), Pick::Rename(0), 0),
                (Pick::Rename(0), Pick::Rename(1), 4),
            ],
        }];
        assert_eq!(
            assign(&cur, &cands, &[false; 3], 1, &pairs),
            vec![Pick::Rename(0), Pick::Rename(1), Pick::Keep]
        );
        let pairs = [Pair {
            row: 0,
            opts: vec![(Pick::Rename(1), Pick::Rename(0), 0)],
        }];
        assert_eq!(
            assign(&cur, &cands, &[false; 3], 1, &pairs).get(..2),
            Some(&[Pick::Unplaced, Pick::Unplaced][..])
        );
    }

    #[test]
    fn room_is_the_window_a_new_name_alone_may_take() {
        let cur = [10, u32::MAX, 40];
        let cands = [vec![(20, 0)], vec![(15, 0)], vec![(35, 0)]];
        assert_eq!(
            room(&cur, &cands, &[false; 3]).get(1),
            Some(&Some((11, 39)))
        );
        assert_eq!(
            room(&[50, 45, 40], &[vec![], vec![], vec![]], &[true; 3]).get(1),
            Some(&None)
        );
    }

    fn var_at(symbol: &str, image: u32) -> Var {
        let (_, hashed, scope) = classify(symbol);
        Var {
            symbol: symbol.to_string(),
            scope,
            bucket: bucket_of(&hashed),
            offset: 0,
            size: 4,
            image: Some(image),
            foreign: false,
            zero_init: None,
        }
    }

    fn same_bucket_pair() -> (String, String) {
        let names: Vec<String> = (0..4000).map(|k| format!("g_sheet{k}")).collect();
        for (i, a) in names.iter().enumerate() {
            for b in names.iter().skip(i + 1) {
                if bucket_of(a) == bucket_of(b) {
                    return (a.clone(), b.clone());
                }
            }
        }
        (String::new(), String::new())
    }

    #[test]
    fn a_same_bucket_pair_in_the_other_order_is_an_inversion_not_a_misplacement() -> Result<()> {
        let (a, b) = same_bucket_pair();
        let mut sheet = var_at(&format!("?{a}@@3PAUObj@@A"), 0x59ca58);
        let mut mix = var_at(&format!("?{b}@@3PAUObj@@A"), 0x59dea0);
        assert_eq!(sheet.bucket, mix.bucket);
        (sheet.offset, mix.offset) = (0x2000, 0x1000);
        let v = judge("T", vec![mix.clone(), sheet.clone()])?;
        assert!(v.image_exact && v.misplaced.is_empty());
        assert_eq!(v.inverted, vec![(1, 0)]);
        assert!(inversion_reading(&v, 1, 0).contains(&format!("declared {a} AFTER {b}")));
        (sheet.offset, mix.offset) = (0x1000, 0x2000);
        assert!(judge("T", vec![sheet, mix])?.inverted.is_empty());
        Ok(())
    }

    #[test]
    fn a_guard_number_is_read_from_either_spelling() {
        assert_eq!(guard_number("_$S17"), Some(17));
        assert_eq!(guard_number("_?$S6@?1??g@@YAHXZ@4EA"), Some(6));
        assert_eq!(guard_number("?g_x@@3HA"), None);
        assert_eq!(guard_number("_$E4"), None);
    }

    #[test]
    fn guards_below_100_hash_high_and_from_100_hash_low() {
        assert!((1..100).all(|n| (953..=994).contains(&bucket_of(&format!("$S{n}")))));
        assert_eq!(bucket_of("$S100"), 15);
        assert_eq!(
            guard_numbers_in(0, 24, 199),
            (100..=115).chain(120..=121).collect::<Vec<_>>()
        );
        assert_eq!(compact(&[1u32, 2, 3, 7, 9, 10], 8), "1..=3 7 9..=10");
        assert_eq!(compact(&[1u32, 3, 5], 2), "1 3 ... (3 runs)");
    }

    fn member_in(prefix: &str, lo: u32, hi: u32) -> String {
        (0..1_000_000u32)
            .map(|k| format!("?{prefix}{k}@Ctl@@2UObj@@A"))
            .find(|n| (lo..=hi).contains(&bucket_of(n)))
            .unwrap_or_default()
    }

    #[test]
    fn a_guard_before_a_low_bucket_name_needs_a_three_digit_number() -> Result<()> {
        let low = member_in("s_low", 24, 24);
        let high = member_in("s_high", 961, 1023);
        let vars = vec![
            var_at("_$S17", 0x4408c0),
            var_at(&low, 0x4408c8),
            var_at(&high, 0x4408f8),
        ];
        assert_eq!(vars.get(1).map(|v| v.bucket), Some(24));
        let v = judge("Ctl", vars)?;
        assert!(!v.image_exact);
        let g = guard_fits(&v);
        assert_eq!(g.len(), 1);
        let g0 = g.first().cloned().ok_or_else(|| anyhow!("no guard fit"))?;
        assert_eq!((g0.n, g0.lo, g0.hi), (17, 0, 24));
        assert_eq!(g0.fits.first(), Some(&100));
        assert_eq!(g0.nearest_shift(), Some(83));
        assert!(guard_reading(&g0).contains(">= 83 MORE"));
        assert_eq!((v.misplaced.len(), misplaced_with_free_guards(&v)), (1, 0));
        let ok = judge("t", vec![var_at("_$S100", 1), var_at("?g_z@@3HA", 2)])?;
        let g = guard_fits(&ok);
        assert_eq!(
            g.first().map(guard_reading).as_deref(),
            Some("our number fits")
        );
        Ok(())
    }
}
