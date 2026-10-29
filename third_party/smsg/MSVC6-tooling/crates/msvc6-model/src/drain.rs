use crate::coffx;
use anyhow::Result;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GlFn {
    pub name: String,
    pub flags: u8,
    pub off: u32,
    pub exported: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Stream {
    Ex,
    Pch,
    Both,
    Unknown,
}

impl Stream {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Stream::Ex => "ex",
            Stream::Pch => "pch",
            Stream::Both => "ex|pch",
            Stream::Unknown => "?",
        }
    }
}

pub const FN_OPEN: [u8; 4] = [0x4f, 0x1f, 0x80, 0x00];

pub fn gl_counter(gl: &[u8]) -> Result<(u32, Option<u16>)> {
    let h = msvc6_il::header::read(&mut msvc6_il::cursor::Cursor::new(gl, 0))
        .map_err(|e| anyhow::anyhow!("not a front end's .gl IL stream: {e}"))?;
    Ok((h.next_id, h.build()))
}

fn gl_name(s: &msvc6_il::sym::Sym) -> String {
    let text = msvc6_base::srcio::tool_text(s.name.as_deref().unwrap_or_default());
    if text.starts_with("$E") {
        format!("_{text}")
    } else {
        text
    }
}

#[must_use]
pub fn gl_functions(gl: &[u8]) -> (Vec<GlFn>, usize) {
    functions_of(&msvc6_il::unit::globals(gl))
}

pub const FLAG_THUNK: u32 = 0x400;

pub const FCLASS_EXPORT: i64 = 9;

#[must_use]
pub fn scheduled(s: &msvc6_il::sym::Sym) -> bool {
    s.class == msvc6_il::sym::CLASS_FDEF && s.flags() & FLAG_THUNK == 0
}

fn functions_of(g: &msvc6_il::unit::Globals) -> (Vec<GlFn>, usize) {
    let mut out = Vec::new();
    let mut odd = usize::from(g.error.is_some());
    for s in g.records.iter().filter(|s| scheduled(s)) {
        match s.ex {
            Some(off) => out.push(GlFn {
                name: gl_name(s),
                flags: u8::try_from(s.flags() & 0xff).unwrap_or(0),
                exported: s.num("fclass") == Some(FCLASS_EXPORT),
                off,
            }),
            None => odd = odd.saturating_add(1),
        }
    }
    (out, odd)
}

fn opens_at(b: &[u8], off: u32) -> bool {
    let o = off as usize;
    b.get(o..o.saturating_add(4)) == Some(&FN_OPEN[..])
}

#[must_use]
pub fn stream_of(ex: &[u8], pch: &[u8], off: u32) -> Stream {
    match (opens_at(ex, off), opens_at(pch, off)) {
        (true, false) => Stream::Ex,
        (false, true) => Stream::Pch,
        (true, true) => Stream::Both,
        (false, false) => Stream::Unknown,
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct IlFn {
    pub name: String,
    pub off: u32,
    pub stream: Stream,
    pub header: bool,
    pub exported: bool,
    pub size: u32,
}

impl IlFn {
    #[must_use]
    pub fn deferred(&self) -> bool {
        self.stream != Stream::Ex || (self.header && !self.exported) || self.name.starts_with("_$E")
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct TuIl {
    pub fns: BTreeMap<String, IlFn>,
    pub odd: usize,
    pub ex_len: usize,
    pub pch_len: usize,
    pub build: Option<u16>,
    pub error: Option<String>,
}

#[must_use]
pub fn read_tu(gl: &[u8], ex: &[u8], pch: &[u8]) -> TuIl {
    read_tu_with(gl, ex, pch, None)
}

#[must_use]
pub fn read_tu_used(gl: &[u8], ex: &[u8], pch: &[u8], creator: &BTreeMap<String, u32>) -> TuIl {
    read_tu_with(gl, ex, pch, Some(creator))
}

fn read_tu_with(gl: &[u8], ex: &[u8], pch: &[u8], creator: Option<&BTreeMap<String, u32>>) -> TuIl {
    let g = msvc6_il::unit::globals(gl);
    let (recs, odd) = functions_of(&g);
    let mut fns: BTreeMap<String, IlFn> = BTreeMap::new();
    for r in recs {
        let header = r.flags & 0x20 != 0;
        let stream = match (stream_of(ex, pch, r.off), creator) {
            (Stream::Both, Some(c)) if c.get(&r.name) == Some(&r.off) => Stream::Pch,
            (Stream::Both, Some(_)) => Stream::Ex,
            (Stream::Both | Stream::Unknown, _) if header => Stream::Pch,
            (Stream::Both | Stream::Unknown, _) => Stream::Ex,
            (s, _) => s,
        };
        fns.entry(r.name.clone()).or_insert(IlFn {
            name: r.name,
            off: r.off,
            stream,
            header,
            exported: r.exported,
            size: 0,
        });
    }
    let mut by: BTreeMap<Stream, Vec<u32>> = BTreeMap::new();
    for f in fns.values() {
        by.entry(f.stream).or_default().push(f.off);
    }
    for v in by.values_mut() {
        v.sort_unstable();
        v.dedup();
    }
    for f in fns.values_mut() {
        let end = match f.stream {
            Stream::Ex => u32::try_from(ex.len()).unwrap_or(u32::MAX),
            _ => f.off,
        };
        let next = by
            .get(&f.stream)
            .and_then(|v| {
                let p = v.partition_point(|&o| o <= f.off);
                v.get(p).copied()
            })
            .unwrap_or(end);
        f.size = next.saturating_sub(f.off);
    }
    TuIl {
        fns,
        odd,
        ex_len: ex.len(),
        pch_len: pch.len(),
        build: g.header.as_ref().and_then(msvc6_il::header::Header::build),
        error: g.error.clone(),
    }
}

#[must_use]
pub fn predicted(names: &[String], il: &TuIl) -> Vec<String> {
    let mut keyed: Vec<(u32, u8, usize, &String)> = Vec::new();
    let mut last = (0u32, 0u8);
    for (i, n) in names.iter().enumerate() {
        if let Some(f) = il.fns.get(n) {
            last = (f.off, u8::from(f.stream != Stream::Ex));
        }
        keyed.push((last.0, last.1, i, n));
    }
    keyed.sort();
    keyed.into_iter().map(|(_, _, _, n)| n.clone()).collect()
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Refs {
    pub text: BTreeMap<String, Vec<String>>,
    pub data: BTreeMap<String, Vec<String>>,
    pub roots: Vec<String>,
}

pub fn refs_of_bytes(d: &[u8]) -> Result<Refs> {
    let mut r = Refs::default();
    let parent = coffx::associated_sections_of(d)?;
    let mut funclets: Vec<(String, Vec<String>)> = Vec::new();
    for c in coffx::code_comdats_named_of(d)? {
        let to: Vec<String> = c.relocations.into_iter().map(|x| x.symbol).collect();
        if c.section == ".text$x" {
            if let Some(p) = parent.get(&c.index) {
                funclets.push((p.clone(), to));
            }
        } else if let Some(s) = c.symbol {
            r.text.insert(s, to);
        }
    }
    for (p, to) in funclets {
        r.text.entry(p).or_default().extend(to);
    }
    for c in coffx::data_comdats_of(d)? {
        if let Some(s) = c.symbol {
            r.data
                .insert(s, c.relocations.into_iter().map(|x| x.symbol).collect());
        }
    }
    r.roots = coffx::noncomdat_data_targets_of(d)?;
    Ok(r)
}

#[must_use]
pub fn predicted_demand(names: &[String], il: &TuIl, refs: &Refs) -> Vec<String> {
    predicted_shifted(names, il, refs, u32::MAX, 0, true)
}

struct St<'a> {
    refs: &'a Refs,
    wanted: BTreeSet<String>,
    waiting: Vec<String>,
    out: Vec<String>,
    done: BTreeSet<String>,
}

fn want(st: &mut St<'_>, s: &str) {
    let mut stack = vec![s.to_string()];
    while let Some(s) = stack.pop() {
        if !st.wanted.insert(s.clone()) {
            continue;
        }
        let mut next: Vec<String> = Vec::new();
        if let Some(d) = st.refs.data.get(&s) {
            next.extend(d.iter().cloned());
        }
        if let Some(rest) = s.strip_prefix("??_E") {
            next.push(format!("??_G{rest}"));
        }
        stack.extend(next.into_iter().rev());
    }
}

#[must_use]
pub fn predicted_shifted(
    names: &[String],
    il: &TuIl,
    refs: &Refs,
    from: u32,
    delta: i64,
    demand: bool,
) -> Vec<String> {
    let shift = |f: &IlFn| {
        let pch = f.stream != Stream::Ex;
        i64::from(f.off) + if pch && f.off >= from { delta } else { 0 }
    };
    let mut items: Vec<(i64, u8, usize, &str)> = Vec::new();
    let mut last = (0i64, 0u8);
    for (i, n) in names.iter().enumerate() {
        if let Some(f) = il.fns.get(n) {
            last = (shift(f), u8::from(f.deferred()));
        }
        items.push((last.0, last.1, i, n));
    }
    items.sort();
    let mut st = St {
        refs,
        wanted: BTreeSet::new(),
        waiting: Vec::new(),
        out: Vec::new(),
        done: BTreeSet::new(),
    };
    for r in &refs.roots {
        want(&mut st, r);
    }
    for (_, pch, _, n) in items {
        if st.done.contains(n) {
            continue;
        }
        if pch == 0 || !demand || st.wanted.contains(n) {
            emit_recursive(&mut st, n);
        } else {
            st.waiting.push(n.to_string());
        }
    }
    let rest = std::mem::take(&mut st.waiting);
    st.out.extend(rest);
    st.out
}

fn emit_recursive(st: &mut St<'_>, f: &str) {
    let mut stack: Vec<(String, bool)> = vec![(f.to_string(), false)];
    while let Some((f, entered)) = stack.pop() {
        if !entered {
            st.out.push(f.clone());
            st.done.insert(f.clone());
            let rs = st.refs.text.get(&f).cloned().unwrap_or_default();
            for r in &rs {
                want(st, r);
            }
        }
        if let Some(p) = st.waiting.iter().position(|w| st.wanted.contains(w)) {
            let w = st.waiting.remove(p);
            stack.push((f, true));
            stack.push((w, false));
        }
    }
}

#[must_use]
pub fn displaced(want: &[String], got: &[String]) -> usize {
    let at: BTreeMap<&String, usize> = want.iter().enumerate().map(|(i, n)| (n, i)).collect();
    let seq: Vec<usize> = got.iter().filter_map(|n| at.get(n).copied()).collect();
    let mut tails: Vec<usize> = Vec::new();
    for x in seq {
        match tails.binary_search(&x) {
            Ok(_) => {}
            Err(p) if p == tails.len() => tails.push(x),
            Err(p) => {
                if let Some(t) = tails.get_mut(p) {
                    *t = x;
                }
            }
        }
    }
    got.len().saturating_sub(tails.len())
}

#[must_use]
pub fn against(want: &[String], pred: &[String]) -> usize {
    let w: BTreeSet<&String> = want.iter().collect();
    let p: Vec<String> = pred.iter().filter(|n| w.contains(n)).cloned().collect();
    let pw: BTreeSet<&String> = p.iter().collect();
    let w2: Vec<String> = want.iter().filter(|n| pw.contains(n)).cloned().collect();
    displaced(&w2, &p)
}

#[must_use]
pub fn exact(known: &[String], pred: &[String]) -> bool {
    !known.is_empty() && against(known, pred) == 0
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct BestShift {
    pub best: usize,
    pub lo: i64,
    pub hi: i64,
}

pub const SHIFT_LO: i64 = -0x4000;

pub const SHIFT_HI: i64 = 0x8000;

pub const SHIFT_STEP: i64 = 4;

#[must_use]
pub fn shift_points() -> u64 {
    SHIFT_HI.abs_diff(SHIFT_LO) + 1
}

#[must_use]
pub fn best_shift(
    known: &[String],
    il: &TuIl,
    refs: &Refs,
    want_order: &[String],
    from: u32,
    demand: bool,
    jobs: usize,
) -> BestShift {
    let ks: Vec<i64> = (SHIFT_LO..=SHIFT_HI).map(|k| k * SHIFT_STEP).collect();
    let score: Vec<(i64, usize)> = msvc6_diff::par::map(&ks, jobs, |&d| {
        (
            d,
            against(
                want_order,
                &predicted_shifted(known, il, refs, from, d, demand),
            ),
        )
    });
    let best = score.iter().map(|x| x.1).min().unwrap_or(0);
    let near = score
        .iter()
        .enumerate()
        .filter(|(_, x)| x.1 == best)
        .min_by_key(|(_, x)| x.0.abs())
        .map_or(0, |(i, _)| i);
    let (mut a, mut b) = (near, near);
    let at = |k: usize| score.get(k).map(|x| x.1);
    while a > 0 && at(a - 1) == Some(best) {
        a -= 1;
    }
    while at(b + 1) == Some(best) {
        b += 1;
    }
    BestShift {
        best,
        lo: score.get(a).map_or(0, |x| x.0),
        hi: score.get(b).map_or(0, |x| x.0),
    }
}

#[must_use]
pub fn predicted_hybrid(names: &[String], ob0: &[String], il: &TuIl, refs0: &Refs) -> Vec<String> {
    let known: Vec<String> = ob0
        .iter()
        .filter(|n| il.fns.contains_key(*n))
        .cloned()
        .collect();
    let keep: BTreeSet<&String> = names.iter().collect();
    predicted_demand(&known, il, refs0)
        .into_iter()
        .filter(|n| keep.contains(n))
        .collect()
}

#[must_use]
pub fn ob0_score(names: &[String], ob0: &[String], il: &TuIl) -> (usize, usize) {
    let in0: BTreeSet<&String> = ob0.iter().collect();
    let held: Vec<String> = names
        .iter()
        .filter(|n| in0.contains(n) && il.fns.contains_key(*n))
        .cloned()
        .collect();
    let hs: BTreeSet<&String> = held.iter().collect();
    let sub: Vec<String> = ob0.iter().filter(|n| hs.contains(n)).cloned().collect();
    (displaced(&held, &sub), held.len())
}

fn before(v: &[String], k: usize) -> String {
    k.checked_sub(1)
        .and_then(|p| v.get(p))
        .map_or_else(|| "(start)".to_string(), String::clone)
}

pub fn report_ob0(
    names: &[String],
    ob0: &[String],
    il: &TuIl,
    out: &mut Vec<String>,
) -> Option<usize> {
    let in0: BTreeSet<&String> = ob0.iter().collect();
    let (d, n) = ob0_score(names, ob0, il);
    let held: Vec<String> = names
        .iter()
        .filter(|n| in0.contains(n) && il.fns.contains_key(*n))
        .cloned()
        .collect();
    let hs: BTreeSet<&String> = held.iter().collect();
    let sub: Vec<String> = ob0.iter().filter(|n| hs.contains(n)).cloned().collect();
    out.push(format!(
        "      /Ob0: {} COMDAT(s); {n} of the object's {} with a .gl record in it, {d} displaced against that order{}",
        ob0.len(),
        names.len(),
        if d == 0 && n > 0 { " (SUBSEQUENCE)" } else { "" }
    ));
    for x in names
        .iter()
        .filter(|x| !in0.contains(x) && il.fns.contains_key(*x))
        .take(4)
    {
        out.push(format!("      /Ob0 lacks {x}"));
    }
    let at: BTreeMap<&String, usize> = held.iter().enumerate().map(|(i, n)| (n, i)).collect();
    let mut shown = 0;
    for (i, n) in sub.iter().enumerate() {
        let j = at.get(n).copied().unwrap_or(i);
        if before(&held, j) != before(&sub, i) && shown < 6 {
            out.push(format!(
                "      /Ob0 DIFF {n}: object after {} | /Ob0 after {}",
                before(&held, j),
                before(&sub, i)
            ));
            shown += 1;
        }
    }
    (n > 0).then_some(d)
}

#[must_use]
pub fn batch_order(order: &[String], flags: &BTreeMap<String, String>) -> Vec<String> {
    let set = |s: &String| flags.get(s).map_or("", String::as_str);
    let mut sets: Vec<&str> = Vec::new();
    for s in order {
        if !sets.contains(&set(s)) {
            sets.push(set(s));
        }
    }
    sets.iter()
        .flat_map(|f| order.iter().filter(move |s| set(s) == *f))
        .cloned()
        .collect()
}

pub type Predicted = (TuIl, (bool, bool), Refs);

pub fn report_compiled(
    stem: &str,
    names: &[String],
    il: TuIl,
    own: &[String],
    refs: Refs,
    out: &mut Vec<String>,
) -> Predicted {
    if own != names {
        out.push(format!(
            "    PREDICT {stem}: NOTE the /Bk compile's own order differs from the given object's ({} displaced): \
             the object was not compiled pch-create, or not from this source",
            displaced(names, own)
        ));
    }
    let known: Vec<String> = names
        .iter()
        .filter(|n| il.fns.contains_key(*n))
        .cloned()
        .collect();
    let missing = names.len() - known.len();
    let pure = predicted(&known, &il);
    let demand = predicted_demand(&known, &il, &refs);
    let (dp, dd) = (displaced(&known, &pure), displaced(&known, &demand));
    let exact = (
        !known.is_empty() && pure == known,
        !known.is_empty() && demand == known,
    );
    out.push(format!(
        "    PREDICT {stem}: {} COMDAT(s), {} with a .gl offset ({missing} without: thunks, initialisers); \
         merge-by-offset {dp} displaced, with demand {dd}{}; .ex {} B, pch {} B{}",
        names.len(),
        known.len(),
        match exact {
            (true, true) => " (both EXACT)",
            (true, false) => " (offset EXACT)",
            (false, true) => " (demand EXACT)",
            (false, false) => "",
        },
        il.ex_len,
        il.pch_len,
        if il.odd > 0 {
            format!(", {} record(s) with an unread offset form", il.odd)
        } else {
            String::new()
        }
    ));
    let o = |n: &String| {
        il.fns.get(n).map_or("-".to_string(), |f| {
            format!("{}@{:#x}", f.stream.tag(), f.off)
        })
    };
    let at: BTreeMap<&String, usize> = known.iter().enumerate().map(|(i, n)| (n, i)).collect();
    for (tag, pred, ok) in [("offset", &pure, exact.0), ("demand", &demand, exact.1)] {
        if ok {
            continue;
        }
        let mut shown = 0;
        for (i, n) in pred.iter().enumerate() {
            let j = at.get(n).copied().unwrap_or(i);
            if before(&known, j) != before(pred, i) && shown < 6 {
                out.push(format!(
                    "      {tag} DIFF {n} ({}): object after {} | predicted after {}",
                    o(n),
                    before(&known, j),
                    before(pred, i)
                ));
                shown += 1;
            }
        }
    }
    (il, exact, refs)
}
