use crate::linkmap::Map;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const ALIGN: u64 = 16;

pub const FILL: u8 = 0xcc;

pub const FILL_RUN: usize = 8;

pub const MIN_TAIL: usize = 64;

pub const SAMPLE_ROWS: usize = 8;

pub const RUN_ROWS: usize = 14;

pub const MIXED_ROWS: usize = 6;

pub const SPACING_ROWS: usize = 12;

pub const NAME_COLUMN: usize = 52;

#[must_use]
pub fn round16(x: u64) -> u64 {
    x.div_ceil(ALIGN).saturating_mul(ALIGN)
}

#[must_use]
pub fn slot(body: u64) -> u64 {
    round16(body.saturating_mul(5) / 4)
}

#[must_use]
pub fn slot32(body: u32) -> u32 {
    u32::try_from(slot(u64::from(body))).unwrap_or(u32::MAX)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Verdict {
    Reserved,
    Tight,
    Overlap,
    Ambiguous,
    Over,
    Under,
}

impl Verdict {
    pub const ALL: [Self; 6] = [
        Self::Reserved,
        Self::Tight,
        Self::Overlap,
        Self::Ambiguous,
        Self::Over,
        Self::Under,
    ];

    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Reserved => "RESERVED  round16(body*1.25)",
            Self::Tight => "TIGHT     round16(body)",
            Self::Overlap => "OVERLAP   extent crosses next entry",
            Self::Ambiguous => "AMBIGUOUS both rules agree",
            Self::Over => "OVER      gap > rule",
            Self::Under => "UNDER     gap < rule",
        }
    }

    #[must_use]
    pub fn tag(self) -> &'static str {
        self.label().split_whitespace().next().unwrap_or_default()
    }

    #[must_use]
    pub fn sided(self) -> Option<bool> {
        match self {
            Self::Tight => Some(true),
            Self::Reserved => Some(false),
            _ => None,
        }
    }

    #[must_use]
    pub fn matching(want: &str) -> Vec<Self> {
        let w = want.to_ascii_lowercase();
        Self::ALL
            .into_iter()
            .filter(|v| v.label().to_ascii_lowercase().starts_with(&w))
            .collect()
    }
}

#[must_use]
pub fn classify(body: u64, gap: u64) -> Verdict {
    if gap < round16(body) {
        return Verdict::Overlap;
    }
    if slot(body) == round16(body) && gap == round16(body) {
        return Verdict::Ambiguous;
    }
    if gap == slot(body) {
        return Verdict::Reserved;
    }
    if gap == round16(body) {
        return Verdict::Tight;
    }
    if gap > slot(body) {
        Verdict::Over
    } else {
        Verdict::Under
    }
}

#[must_use]
pub fn fill_body(sec: &[u8], off: usize, limit: usize) -> Option<usize> {
    let limit = limit.min(sec.len());
    let mut run = 0usize;
    let mut i = off;
    while i < limit.saturating_add(FILL_RUN - 1) && i < sec.len() {
        if sec.get(i) == Some(&FILL) {
            run += 1;
            if run == FILL_RUN {
                let start = i + 1 - FILL_RUN;
                return (start < limit).then_some(start - off);
            }
        } else {
            run = 0;
        }
        i += 1;
    }
    None
}

#[must_use]
pub fn fill_in(sec: &[u8], sec_rva: u32, from: u32, to: u32) -> Option<usize> {
    let a = usize::try_from(from.checked_sub(sec_rva)?).ok()?;
    let b = usize::try_from(to.checked_sub(sec_rva)?).ok()?;
    Some(sec.get(a..b)?.iter().filter(|x| **x == FILL).count())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    pub body: u64,
    pub name: String,
    pub thunk: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Pair {
    pub rva: u32,
    pub body: u64,
    pub gap: u64,
    pub verdict: Verdict,
    pub name: String,
}

impl Pair {
    #[must_use]
    pub fn excess(&self) -> u64 {
        slot(self.body).saturating_sub(self.gap)
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Census {
    pub entries: BTreeMap<u32, Entry>,
    pub absorbed: usize,
    pub thunks: usize,
    pub pairs: Vec<Pair>,
    pub agree: usize,
    pub disagree: usize,
    pub nofill: usize,
    pub untrusted: Vec<(u32, u64, usize)>,
}

pub const THUNK_LABEL: &str = "thunk (excluded)";

#[must_use]
pub fn census(
    mut entries: BTreeMap<u32, Entry>,
    text: &[u8],
    text_rva: u32,
    funclet: &dyn Fn(&str) -> bool,
) -> Census {
    let keys: Vec<u32> = entries.keys().copied().collect();
    let mut absorbed = 0usize;
    let mut parent: Option<u32> = None;
    let mut grown: Vec<(u32, u64)> = Vec::new();
    for k in &keys {
        let Some(e) = entries.get(k) else { continue };
        if !funclet(&e.name) {
            parent = Some(*k);
            continue;
        }
        absorbed += 1;
        if let Some(p) = parent {
            grown.push((p, u64::from(k - p).saturating_add(e.body)));
        }
    }
    for (p, body) in grown {
        if let Some(e) = entries.get_mut(&p) {
            e.body = body;
        }
    }
    let real: Vec<u32> = keys
        .into_iter()
        .filter(|k| entries.get(k).is_some_and(|e| !funclet(&e.name)))
        .collect();
    let mut c = Census {
        absorbed,
        ..Census::default()
    };
    for w in real.windows(2) {
        let (Some(&a), Some(&n)) = (w.first(), w.get(1)) else {
            continue;
        };
        let Some(e) = entries.get(&a) else { continue };
        if e.thunk {
            c.thunks += 1;
            continue;
        }
        let gap = u64::from(n - a);
        c.pairs.push(Pair {
            rva: a,
            body: e.body,
            gap,
            verdict: classify(e.body, gap),
            name: e.name.clone(),
        });
        let start = usize::try_from(a.saturating_sub(text_rva)).unwrap_or(usize::MAX);
        let stop = usize::try_from(n.saturating_sub(text_rva)).unwrap_or(usize::MAX);
        match fill_body(text, start, stop) {
            None => c.nofill += 1,
            Some(f) if round16(u64::try_from(f).unwrap_or(u64::MAX)) == round16(e.body) => {
                c.agree += 1;
            }
            Some(f) => {
                c.disagree += 1;
                c.untrusted.push((a, e.body, f));
            }
        }
    }
    c.entries = entries;
    c
}

fn pct(n: usize, of: usize) -> f64 {
    if of == 0 {
        return 0.0;
    }
    f64::from(u32::try_from(n).unwrap_or(u32::MAX)) * 100.0
        / f64::from(u32::try_from(of).unwrap_or(u32::MAX))
}

fn short(name: &str, n: usize) -> String {
    name.chars().take(n).collect()
}

fn va(rva: u32, base: u32) -> u64 {
    u64::from(rva) + u64::from(base)
}

impl Census {
    #[must_use]
    pub fn counts(&self) -> BTreeMap<&'static str, usize> {
        let mut out: BTreeMap<&'static str, usize> = BTreeMap::new();
        for p in &self.pairs {
            *out.entry(p.verdict.label()).or_default() += 1;
        }
        if self.thunks > 0 {
            out.insert(THUNK_LABEL, self.thunks);
        }
        out
    }

    #[must_use]
    pub fn count_lines(&self) -> Vec<String> {
        let total = self.pairs.len();
        let mut out = vec![
            String::new(),
            format!("  {total} consecutive non-thunk pair(s)"),
        ];
        for (k, v) in self.counts() {
            out.push(format!("    {k:38} {v:5}   {:5.1}%", pct(v, total)));
        }
        out
    }

    pub fn list_lines(&self, want: &str, base: u32) -> Result<Vec<String>, String> {
        let kinds = Verdict::matching(want);
        let sel: Vec<&Pair> = self
            .pairs
            .iter()
            .filter(|p| kinds.contains(&p.verdict))
            .collect();
        if sel.is_empty() {
            return Err(format!(
                "--list '{want}' matched no pair. One of: reserved, tight, overlap, ambiguous, over, under."
            ));
        }
        let mut out = vec![
            String::new(),
            format!("  ALL {} pair(s) matching `{want}':", sel.len()),
        ];
        for p in sel {
            out.push(format!(
                "    {:#010x}  {:9}  body {:6}  gap {:6}  rule {:6}  excess {:6}   {}",
                va(p.rva, base),
                p.verdict.tag(),
                p.body,
                p.gap,
                slot(p.body),
                p.excess(),
                short(&p.name, NAME_COLUMN)
            ));
        }
        Ok(out)
    }

    #[must_use]
    pub fn cross_lines(&self, base: u32) -> Vec<String> {
        let mut out = vec![
            String::new(),
            "  BODY LENGTH CROSS-CHECK -- each extent against a 0xcc fill scan, same 16-byte bucket:"
                .to_string(),
            format!(
                "    AGREE {}   DISAGREE {}   NO FILL BEFORE THE NEXT BODY {}",
                self.agree, self.disagree, self.nofill
            ),
        ];
        for (a, ext, f) in self.untrusted.iter().take(SAMPLE_ROWS) {
            out.push(format!(
                "    UNTRUSTED {:#010x}  extent {ext:6}  fill {f:6}  -> buckets {} vs {}",
                va(*a, base),
                round16(*ext),
                round16(u64::try_from(*f).unwrap_or(u64::MAX))
            ));
        }
        out
    }

    #[must_use]
    pub fn first_lines(&self, base: u32) -> Vec<String> {
        let mut out = Vec::new();
        for want in [Verdict::Tight, Verdict::Reserved] {
            let some: Vec<&Pair> = self
                .pairs
                .iter()
                .filter(|p| p.verdict == want)
                .take(SAMPLE_ROWS)
                .collect();
            if some.is_empty() {
                continue;
            }
            out.push(String::new());
            out.push(format!("  first {} rows:", want.tag()));
            for p in some {
                out.push(format!(
                    "    {:#010x}  body {:6}  gap {:6}  rule {:6}   {}",
                    va(p.rva, base),
                    p.body,
                    p.gap,
                    slot(p.body),
                    short(&p.name, 46)
                ));
            }
        }
        out
    }

    #[must_use]
    pub fn first_disagreeing(&self, ours: &BTreeSet<String>) -> Option<&Pair> {
        self.pairs
            .iter()
            .filter(|p| ours.contains(&p.name) && p.verdict != Verdict::Reserved)
            .min_by_key(|p| p.rva)
    }

    #[must_use]
    pub fn split_lines(
        &self,
        map_label: &str,
        ours: &BTreeSet<String>,
        theirs: &BTreeSet<String>,
        base: u32,
    ) -> Vec<String> {
        let mut split: BTreeMap<(&'static str, &'static str), usize> = BTreeMap::new();
        for p in &self.pairs {
            let side = if ours.contains(&p.name) {
                "OURS"
            } else if theirs.contains(&p.name) {
                "LIBRARY"
            } else {
                "UNKNOWN"
            };
            *split.entry((side, p.verdict.label())).or_default() += 1;
        }
        let mut out = vec![
            String::new(),
            format!(
                "  SPLIT BY WHO DEFINES THE SYMBOL, from {map_label} ({} explicit-object and {} archive-member spellings):",
                ours.len(),
                theirs.len()
            ),
        ];
        for side in ["OURS", "LIBRARY", "UNKNOWN"] {
            let tot: usize = split
                .iter()
                .filter(|((s, _), _)| *s == side)
                .map(|(_, v)| *v)
                .sum();
            out.push(format!("    {side:8}  {tot} pair(s)"));
            for ((s, label), v) in &split {
                if *s == side {
                    out.push(format!("      {label:38} {v:5}   {:5.1}%", pct(*v, tot)));
                }
            }
        }
        if let Some(p) = self.first_disagreeing(ours) {
            let downstream = self
                .pairs
                .iter()
                .filter(|x| x.rva >= p.rva && ours.contains(&x.name))
                .count();
            let own = self.pairs.iter().filter(|x| ours.contains(&x.name)).count();
            out.push(String::new());
            out.push(format!(
                "  FIRST DISAGREEING OWN BOUNDARY: {:#010x}  body {}  gap {}  rule {}  {}",
                va(p.rva, base),
                p.body,
                p.gap,
                slot(p.body),
                short(&p.name, 44)
            ));
            out.push(format!("    verdict {}", p.verdict.label()));
            out.push(format!(
                "    {downstream} of {own} own boundaries lie at or after it: an address is the sum of every body and gap \
                 before it, so a change that only moves bytes at or after this boundary cannot reach the reference \
                 while this one disagrees"
            ));
        }
        out
    }

    #[must_use]
    pub fn band_lines(&self, cut_va: u32, base: u32) -> Vec<String> {
        let cut = cut_va.saturating_sub(base);
        let mut split: BTreeMap<(&'static str, &'static str), usize> = BTreeMap::new();
        for p in &self.pairs {
            let side = if p.rva < cut { "BELOW" } else { "AT/ABOVE" };
            *split.entry((side, p.verdict.label())).or_default() += 1;
        }
        let mut out = vec![String::new(), format!("  SPLIT AT {cut_va:#010x}:")];
        for side in ["BELOW", "AT/ABOVE"] {
            let tot: usize = split
                .iter()
                .filter(|((s, _), _)| *s == side)
                .map(|(_, v)| *v)
                .sum();
            out.push(format!("    {side} {cut_va:#x}  --  {tot} pair(s)"));
            for ((s, label), v) in &split {
                if *s == side {
                    out.push(format!("      {label:38} {v:5}   {:5.1}%", pct(*v, tot)));
                }
            }
        }
        out
    }

    #[must_use]
    pub fn runs(&self) -> Vec<(&'static str, usize, u32)> {
        let mut runs: Vec<(&'static str, usize, u32)> = Vec::new();
        for p in &self.pairs {
            let side = match p.verdict.sided() {
                Some(true) => "TIGHT",
                Some(false) => "RESERVED",
                None => "-",
            };
            match runs.last_mut() {
                Some((s, n, _)) if *s == side => *n += 1,
                _ => runs.push((side, 1, p.rva)),
            }
        }
        runs.retain(|(s, _, _)| *s != "-");
        runs
    }

    #[must_use]
    pub fn run_lines(&self, base: u32) -> Vec<String> {
        let sided = self.runs();
        let longest = sided.iter().map(|(_, n, _)| *n).max().unwrap_or(0);
        let singles = sided.iter().filter(|(_, n, _)| *n == 1).count();
        let total: usize = sided.iter().map(|(_, n, _)| *n).sum();
        let mean = if sided.is_empty() {
            0.0
        } else {
            f64::from(u32::try_from(total).unwrap_or(u32::MAX))
                / f64::from(u32::try_from(sided.len()).unwrap_or(u32::MAX))
        };
        let mut out = vec![
            String::new(),
            format!(
                "  DOES `TIGHT' CLUSTER? {} run(s) of one side or the other, mean length {mean:.1}, longest {longest}, \
                 {singles} of length 1.",
                sided.len()
            ),
        ];
        for (s, n, at) in sided.iter().take(RUN_ROWS) {
            out.push(format!(
                "    {s:8} x{n:<4} starting {:#010x}",
                va(*at, base)
            ));
        }
        out
    }
}

#[must_use]
pub fn member_at(rows: &[(u32, String)], rva: u32) -> Option<&str> {
    let i = rows.partition_point(|(r, _)| *r < rva);
    match rows.get(i) {
        Some((r, m)) if *r == rva => Some(m.as_str()),
        _ => None,
    }
}

#[must_use]
pub fn expected_pure(sizes: &[usize], p_tight: f64) -> f64 {
    let q = 1.0 - p_tight;
    sizes
        .iter()
        .map(|n| {
            let n = i32::try_from(*n).unwrap_or(i32::MAX);
            p_tight.powi(n) + q.powi(n)
        })
        .sum()
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Purity {
    NoSample,
    OneSided,
    PerObject,
    NotPerObject,
}

#[derive(Debug, Clone, PartialEq)]
pub struct ByMember {
    pub placed: usize,
    pub attributed: usize,
    pub unattributed: usize,
    pub multi: usize,
    pub pure: usize,
    pub expected: f64,
    pub tight: usize,
    pub reserved: usize,
    pub verdict: Purity,
    pub mixed: Vec<(String, usize, usize)>,
}

#[must_use]
pub fn by_member(pairs: &[Pair], placed: &[(u32, String)]) -> ByMember {
    let mut sorted = placed.to_vec();
    sorted.sort_by_key(|(r, _)| *r);
    let mut per: BTreeMap<&str, (usize, usize)> = BTreeMap::new();
    let mut unattributed = 0usize;
    for p in pairs {
        let Some(tight) = p.verdict.sided() else {
            continue;
        };
        match member_at(&sorted, p.rva) {
            Some(m) => {
                let e = per.entry(m).or_default();
                if tight {
                    e.0 += 1;
                } else {
                    e.1 += 1;
                }
            }
            None => unattributed += 1,
        }
    }
    let multi: Vec<(&&str, &(usize, usize))> =
        per.iter().filter(|(_, (t, r))| t + r >= 2).collect();
    let pure = multi
        .iter()
        .filter(|(_, (t, r))| *t == 0 || *r == 0)
        .count();
    let tight: usize = multi.iter().map(|(_, (t, _))| *t).sum();
    let reserved: usize = multi.iter().map(|(_, (_, r))| *r).sum();
    let p = if tight + reserved == 0 {
        0.0
    } else {
        f64::from(u32::try_from(tight).unwrap_or(u32::MAX))
            / f64::from(u32::try_from(tight + reserved).unwrap_or(u32::MAX))
    };
    let sizes: Vec<usize> = multi.iter().map(|(_, (t, r))| t + r).collect();
    let expected = expected_pure(&sizes, p);
    let observed = f64::from(u32::try_from(pure).unwrap_or(u32::MAX));
    let verdict = if multi.is_empty() {
        Purity::NoSample
    } else if tight.min(reserved) * 20 < tight + reserved {
        Purity::OneSided
    } else if observed > expected + (expected * 0.15).max(3.0) {
        Purity::PerObject
    } else {
        Purity::NotPerObject
    };
    let mut mixed: Vec<(String, usize, usize)> = per
        .iter()
        .filter(|(_, (t, r))| *t > 0 && *r > 0)
        .map(|(m, (t, r))| ((*m).to_string(), *t, *r))
        .collect();
    mixed.sort_by_key(|(_, t, r)| std::cmp::Reverse(t + r));
    ByMember {
        placed: placed.len(),
        attributed: tight + reserved,
        unattributed,
        multi: multi.len(),
        pure,
        expected,
        tight,
        reserved,
        verdict,
        mixed,
    }
}

impl ByMember {
    #[must_use]
    pub fn lines(&self) -> Vec<String> {
        let total = self.tight + self.reserved;
        let rate = pct(self.tight, total);
        let mut out = vec![
            String::new(),
            format!(
                "  BY MEMBER -- does the verdict partition by archive member? {} placed member row(s); {} sided pair(s) \
                 attributed by exact entry address, {} not named by the table",
                self.placed, self.attributed, self.unattributed
            ),
            format!("    members with >= 2 sided rows   {:>5}", self.multi),
            format!("    of those, PURE (all one side)  {:>5}", self.pure),
            format!(
                "    EXPECTED pure if independent   {:>7.1}   at the observed TIGHT rate {rate:.1}%",
                self.expected
            ),
            format!(
                "    global split among them        {} TIGHT / {} RESERVED",
                self.tight, self.reserved
            ),
        ];
        out.push(match self.verdict {
            Purity::NoSample => {
                "    NO MEMBER CONTRIBUTES TWO SIDED ROWS: no verdict.".to_string()
            }
            Purity::OneSided => format!(
                "    ONE-SIDED SAMPLE ({} on the minority side of {total}): purity is near 1 by construction, so no \
                 clustering verdict is drawn",
                self.tight.min(self.reserved)
            ),
            Purity::PerObject => format!(
                "    READS PER-OBJECT: purity exceeds the independent null by {:.1}",
                f64::from(u32::try_from(self.pure).unwrap_or(u32::MAX)) - self.expected
            ),
            Purity::NotPerObject => format!(
                "    DOES NOT READ PER-OBJECT: purity is at or below what independent draws give ({:.1})",
                self.expected
            ),
        });
        if !self.mixed.is_empty() {
            out.push("    largest MIXED member(s):".to_string());
            for (m, t, r) in self.mixed.iter().take(MIXED_ROWS) {
                out.push(format!("      {t:>3} TIGHT {r:>3} RESERVED  {m}"));
            }
        }
        out
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Tail {
    Unreadable,
    Occupied,
    Reversed,
    BothEmpty,
    Mixed,
}

impl Tail {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Unreadable => "tail under 64B -- body alignment dominates, NO signal",
            Self::Occupied => "OCCUPIED in ref, FILL in ours",
            Self::Reversed => "REVERSED -- ref leaves it EMPTY and we place code",
            Self::BothEmpty => "empty in BOTH",
            Self::Mixed => "mixed",
        }
    }
}

#[must_use]
pub fn tail_of(tail: usize, reference_fill: usize, ours_fill: usize) -> Tail {
    if tail < MIN_TAIL {
        Tail::Unreadable
    } else if reference_fill * 4 < tail && ours_fill * 4 > tail * 3 {
        Tail::Occupied
    } else if ours_fill * 4 < tail && reference_fill * 4 > tail * 3 {
        Tail::Reversed
    } else if reference_fill * 4 > tail * 3 && ours_fill * 4 > tail * 3 {
        Tail::BothEmpty
    } else {
        Tail::Mixed
    }
}

#[derive(Debug)]
pub struct Sections<'a> {
    pub reference: &'a [u8],
    pub reference_rva: u32,
    pub ours: &'a [u8],
    pub ours_rva: u32,
}

#[must_use]
pub fn occupancy_lines(c: &Census, s: &Sections<'_>, base: u32, label: &str) -> Vec<String> {
    let mut out = vec![
        String::new(),
        format!(
            "  RESERVE-TAIL OCCUPANCY for every TIGHT pair -- what is in [entry+body, entry+rule): `ent' counts the \
             recorded entries strictly inside it; candidate {label}"
        ),
        format!(
            "    {:<12} {:>6} {:>6} {:>4} {:>10} {:>10}   verdict / function",
            "entry", "body", "tail", "ent", "ref fill", "our fill"
        ),
    ];
    let mut n: BTreeMap<Tail, usize> = BTreeMap::new();
    for p in c.pairs.iter().filter(|p| p.verdict == Verdict::Tight) {
        let end = p
            .rva
            .saturating_add(u32::try_from(p.body).unwrap_or(u32::MAX));
        let stop = p
            .rva
            .saturating_add(u32::try_from(slot(p.body)).unwrap_or(u32::MAX));
        if stop <= end {
            continue;
        }
        let tail = usize::try_from(stop - end).unwrap_or(usize::MAX);
        let ents = c.entries.range(end.saturating_add(1)..stop).count();
        let (Some(rf), Some(of)) = (
            fill_in(s.reference, s.reference_rva, end, stop),
            fill_in(s.ours, s.ours_rva, end, stop),
        ) else {
            continue;
        };
        let t = tail_of(tail, rf, of);
        *n.entry(t).or_default() += 1;
        out.push(format!(
            "    {:#010x} {:>6} {tail:>6} {ents:>4} {rf:>10} {of:>10}   {}  {}",
            va(p.rva, base),
            p.body,
            t.label(),
            short(&p.name, 34)
        ));
    }
    let get = |t: Tail| n.get(&t).copied().unwrap_or(0);
    out.push(format!(
        "    {} OCCUPIED-in-reference, {} REVERSED, {} empty in both, {} with a tail under 64B that carries no signal.",
        get(Tail::Occupied),
        get(Tail::Reversed),
        get(Tail::BothEmpty),
        get(Tail::Unreadable)
    ));
    out
}

#[must_use]
pub fn spellings(mangled: &str) -> Vec<String> {
    if !mangled.starts_with('?') {
        let bare = mangled.trim_start_matches('_').to_string();
        if bare == mangled {
            return vec![bare];
        }
        return vec![mangled.to_string(), bare];
    }
    if mangled.len() > 3 && mangled.get(..2) == Some("??") {
        let rest = mangled.get(3..).unwrap_or("");
        let cls = rest.split('@').next().unwrap_or("");
        if !cls.is_empty() {
            let tilde = if mangled.get(2..3) == Some("1") {
                "~"
            } else {
                ""
            };
            return vec![format!("{cls}::{tilde}{cls}"), cls.to_string()];
        }
    }
    let head = mangled
        .trim_start_matches('?')
        .split("@@")
        .next()
        .unwrap_or("");
    let parts: Vec<&str> = head.split('@').filter(|p| !p.is_empty()).collect();
    match parts.as_slice() {
        [] => Vec::new(),
        [one] => vec![(*one).to_string()],
        _ => {
            let mut rev = parts.clone();
            rev.reverse();
            let mut out = vec![rev.join("::")];
            if let Some(p) = parts.first() {
                out.push((*p).to_string());
            }
            out
        }
    }
}

#[must_use]
pub fn ours_and_theirs(map: &Map) -> (BTreeSet<String>, BTreeSet<String>) {
    let code = map.code_sections();
    let mut ours = BTreeSet::new();
    let mut theirs = BTreeSet::new();
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        if !r.obj.ends_with(".obj") {
            continue;
        }
        let set = if r.obj.contains(':') {
            &mut theirs
        } else {
            &mut ours
        };
        set.extend(spellings(&r.name));
    }
    (ours, theirs)
}

#[must_use]
pub fn rdata_total(sections: &[(String, u64)]) -> u64 {
    sections
        .iter()
        .filter(|(n, _)| n == ".rdata")
        .map(|(_, sz)| *sz)
        .sum()
}

#[must_use]
pub fn rdata_starts(map: &Map) -> BTreeMap<String, u32> {
    let rdata: BTreeSet<u16> = map
        .contribs
        .iter()
        .filter(|c| c.name == ".rdata")
        .map(|c| c.sect)
        .collect();
    let mut first: BTreeMap<String, u32> = BTreeMap::new();
    for r in &map.rows {
        if !rdata.contains(&r.sect) || !r.obj.ends_with(".obj") || r.obj.contains(':') {
            continue;
        }
        let e = first.entry(r.obj.clone()).or_insert(r.va);
        *e = (*e).min(r.va);
    }
    first
}

#[must_use]
pub fn rdata_lines(starts: &BTreeMap<String, u32>, raw: &dyn Fn(&str) -> u64) -> Vec<String> {
    let mut rows: Vec<(&String, u32, u64)> = starts.iter().map(|(o, s)| (o, *s, raw(o))).collect();
    rows.sort_by_key(|r| r.1);
    let mut out = vec![
        String::new(),
        "  INTER-OBJECT `.rdata' GAPS IN OUR LINK, from the map and each object's own `.rdata' size. A gap is \
         growth room or content not yet written; only the reference's bytes at the boundary tell which."
            .to_string(),
    ];
    let mut total: i64 = 0;
    for (i, (obj, start, rawsz)) in rows.iter().enumerate() {
        let Some(next) = rows.get(i + 1).map(|r| r.1) else {
            out.push(format!(
                "    {obj:20} first {start:#010x}  raw {rawsz:5}   (last -- no following object to bound it)"
            ));
            continue;
        };
        let gap = i64::from(next.saturating_sub(*start));
        let r = i64::try_from(*rawsz).unwrap_or(i64::MAX);
        total = total.saturating_add(gap.saturating_sub(r));
        out.push(format!(
            "    {obj:20} first {start:#010x}  raw {rawsz:5}  gap {gap:5}  UNEXPLAINED {:+5}",
            gap.saturating_sub(r)
        ));
    }
    out.push(format!(
        "    total unexplained: {total} byte(s) -- reserve and missing content, not separated."
    ));
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Spacing {
    Match,
    Tighter(u32),
    Looser(u32),
}

#[must_use]
pub fn spacing_of(ours: u32, reference: u32) -> Spacing {
    match ours.cmp(&reference) {
        std::cmp::Ordering::Equal => Spacing::Match,
        std::cmp::Ordering::Less => Spacing::Tighter(reference - ours),
        std::cmp::Ordering::Greater => Spacing::Looser(ours - reference),
    }
}

#[must_use]
pub fn publics_by_object(map: &Map, image_base: u32) -> Vec<(String, u32, String)> {
    map.rows
        .iter()
        .filter(|r| r.obj.ends_with(".obj"))
        .filter_map(|r| Some((r.name.clone(), r.va.checked_sub(image_base)?, r.obj.clone())))
        .collect()
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct SpacingReport {
    pub matched: usize,
    pub tighter: usize,
    pub looser: usize,
    pub no_ref: usize,
    pub not_adjacent: usize,
    pub rows: Vec<(String, String, u32, u32)>,
}

#[must_use]
pub fn spacing(ours: &[(String, u32, String)], refs: &BTreeMap<String, u32>) -> SpacingReport {
    let rank: BTreeMap<u32, usize> = refs
        .values()
        .copied()
        .collect::<BTreeSet<u32>>()
        .into_iter()
        .enumerate()
        .map(|(i, r)| (r, i))
        .collect();
    let index: BTreeMap<&str, usize> = refs
        .iter()
        .filter_map(|(n, r)| Some((n.as_str(), *rank.get(r)?)))
        .collect();
    let mut by_obj: BTreeMap<&str, Vec<(u32, &str)>> = BTreeMap::new();
    for (name, rva, obj) in ours {
        by_obj
            .entry(obj.as_str())
            .or_default()
            .push((*rva, name.as_str()));
    }
    let mut s = SpacingReport::default();
    for syms in by_obj.values_mut() {
        syms.sort_unstable();
        for pair in syms.windows(2) {
            let [(a_rva, a), (b_rva, b)] = pair else {
                continue;
            };
            let (Some(ra), Some(rb), Some(ia), Some(ib)) = (
                refs.get(*a),
                refs.get(*b),
                index.get(a).copied(),
                index.get(b).copied(),
            ) else {
                s.no_ref += 1;
                continue;
            };
            if ib != ia + 1 {
                s.not_adjacent += 1;
                continue;
            }
            let (Some(og), Some(rg)) = (b_rva.checked_sub(*a_rva), rb.checked_sub(*ra)) else {
                s.not_adjacent += 1;
                continue;
            };
            match spacing_of(og, rg) {
                Spacing::Match => s.matched += 1,
                Spacing::Tighter(_) => {
                    s.tighter += 1;
                    s.rows.push(((*a).to_string(), (*b).to_string(), og, rg));
                }
                Spacing::Looser(_) => {
                    s.looser += 1;
                    s.rows.push(((*a).to_string(), (*b).to_string(), og, rg));
                }
            }
        }
    }
    s
}

impl SpacingReport {
    #[must_use]
    pub fn scored(&self) -> usize {
        self.matched + self.tighter + self.looser
    }

    #[must_use]
    pub fn lines(&self, label: &str, map_label: &str) -> Vec<String> {
        let scored = self.scored();
        let mut out = vec![
            String::new(),
            format!(
                "{label}: does our link space consecutive same-object functions as the reference does?"
            ),
            format!("  MAP: {map_label}"),
            format!(
                "  POPULATION: {scored} pair(s) adjacent in BOTH our map and the reference's own address order, out of {} \
                 same-object pair(s) in the map. EXCLUDED: {} with no reference address, {} not adjacent in the reference.",
                scored + self.no_ref + self.not_adjacent,
                self.no_ref,
                self.not_adjacent
            ),
        ];
        if scored == 0 {
            out.push("  -> NOTHING SCORED: no verdict.".to_string());
            return out;
        }
        out.push(format!("    MATCH   {:>4} of {scored}", self.matched));
        out.push(format!(
            "    TIGHTER {:>4}  -- we packed where the reference left room",
            self.tighter
        ));
        out.push(format!("    LOOSER  {:>4}", self.looser));
        for (a, b, og, rg) in self.rows.iter().take(SPACING_ROWS) {
            out.push(format!(
                "      ours {og:>6}  reference {rg:>6}   {a} -> {b}"
            ));
        }
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_rule_is_round16_of_five_quarters_truncated_before_rounding() {
        assert_eq!(slot(480), 608);
        assert_eq!(slot(34), 48);
        assert_ne!(slot(34), round16(round16(34) * 5 / 4));
        assert_eq!(486 * 5 / 4, 607);
        assert_eq!(slot(486), 608);
        assert_eq!(slot(472), 592);
        assert_eq!(slot32(40), 64);
        assert_eq!(classify(480, 608), Verdict::Reserved);
        assert_eq!(classify(480, 480), Verdict::Tight);
        assert_eq!(classify(600, 480), Verdict::Overlap);
        assert_eq!(classify(100, 4096), Verdict::Over);
    }

    #[test]
    fn a_gap_both_rules_predict_is_ambiguous_and_every_other_body_discriminates() {
        let ambiguous: Vec<u64> = (1..4096).filter(|b| slot(*b) == round16(*b)).collect();
        for &b in &ambiguous {
            assert_eq!(classify(b, round16(b)), Verdict::Ambiguous, "body {b}");
        }
        for b in (1..4096u64).filter(|b| !ambiguous.contains(b)) {
            assert_eq!(classify(b, round16(b)), Verdict::Tight, "body {b}");
            assert_eq!(classify(b, slot(b)), Verdict::Reserved, "body {b}");
        }
        assert_eq!(ambiguous.iter().max().copied(), Some(51));
        assert_eq!(ambiguous.len(), 13 + 10 + 7 + 3);
    }

    #[test]
    fn a_fill_window_outside_its_section_counts_nothing() {
        let sec = vec![FILL; 32];
        assert_eq!(fill_in(&sec, 0x1000, 0x1000, 0x1010), Some(16));
        assert_eq!(fill_in(&sec, 0x1000, 0x1000, 0x1020), Some(32));
        assert_eq!(fill_in(&sec, 0x1000, 0x1010, 0x1040), None);
        assert_eq!(fill_in(&sec, 0x1000, 0x0f00, 0x1010), None);
        assert_eq!(fill_in(&sec, 0x1000, 0x1010, 0x1008), None);
        assert_eq!(fill_in(&[0x55, 0x8b, 0xec, FILL], 0, 0, 4), Some(1));
    }

    #[test]
    fn a_fill_scan_stops_at_the_first_eight_byte_run_before_the_limit() {
        let mut sec = vec![0x90u8; 64];
        for b in sec.iter_mut().skip(20).take(8) {
            *b = FILL;
        }
        assert_eq!(fill_body(&sec, 4, 40), Some(16));
        assert_eq!(fill_body(&sec, 4, 20), None);
        assert_eq!(fill_body(&sec, 4, 21), Some(16));
        assert_eq!(fill_body(&sec, 200, 300), None);
    }

    fn e(body: u64, name: &str) -> Entry {
        Entry {
            body,
            name: name.to_string(),
            thunk: false,
        }
    }

    #[test]
    fn a_funclet_is_absorbed_into_its_parent_and_a_thunk_counted_apart() {
        let mut fns = BTreeMap::new();
        fns.insert(0x1000, e(0x40, "parent"));
        fns.insert(0x1040, e(0x10, "parent_unwind"));
        fns.insert(0x1070, e(10, "next"));
        fns.insert(
            0x1080,
            Entry {
                thunk: true,
                ..e(5, "thunk")
            },
        );
        fns.insert(0x1090, e(4, "last"));
        let text = vec![0x90u8; 0x100];
        let c = census(fns, &text, 0x1000, &|n: &str| n.contains("_unwind"));
        assert_eq!(c.absorbed, 1);
        assert_eq!(c.thunks, 1);
        assert_eq!(c.pairs.len(), 2);
        assert_eq!(c.pairs[0].body, 0x50);
        assert_eq!(c.pairs[0].gap, 0x70);
        assert_eq!(c.pairs[0].verdict, Verdict::Reserved);
        assert_eq!(c.counts().get(THUNK_LABEL), Some(&1));
        assert_eq!(c.nofill, 2);
    }

    #[test]
    fn exact_entry_attribution_refuses_a_near_miss() {
        let rows = vec![
            (0x1000u32, "a.obj".to_string()),
            (0x2000, "b.obj".to_string()),
        ];
        assert_eq!(member_at(&rows, 0x1000), Some("a.obj"));
        assert_eq!(member_at(&rows, 0x2000), Some("b.obj"));
        assert_eq!(member_at(&rows, 0x1abc), None);
        assert_eq!(member_at(&rows, 0x0fff), None);
    }

    #[test]
    fn the_null_expects_purity_from_a_one_sided_sample() {
        let sizes = vec![2usize; 20];
        assert!(expected_pure(&sizes, 0.998) > 19.8);
        assert!((expected_pure(&sizes, 0.5) - 10.0).abs() < 0.01);
    }

    fn pair(rva: u32, verdict: Verdict, name: &str) -> Pair {
        Pair {
            rva,
            body: 10,
            gap: 16,
            verdict,
            name: name.to_string(),
        }
    }

    #[test]
    fn the_earliest_disagreeing_own_boundary_is_named_not_the_first_seen() {
        let ours: BTreeSet<String> = ["late", "early", "fine"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        let c = Census {
            pairs: vec![
                pair(0x9000, Verdict::Tight, "late"),
                pair(0x1000, Verdict::Reserved, "fine"),
                pair(0x2000, Verdict::Tight, "early"),
                pair(0x0800, Verdict::Tight, "theirs"),
            ],
            ..Census::default()
        };
        assert_eq!(c.first_disagreeing(&ours).map(|p| p.rva), Some(0x2000));
        let clean = Census {
            pairs: vec![pair(0x1000, Verdict::Reserved, "fine")],
            ..Census::default()
        };
        assert!(clean.first_disagreeing(&ours).is_none());
    }

    #[test]
    fn a_one_sided_member_sample_draws_no_verdict_and_a_mixed_member_is_listed() {
        let pairs: Vec<Pair> = (0..40u32)
            .map(|i| pair(0x1000 + i * 16, Verdict::Tight, "x"))
            .chain([pair(0x9000, Verdict::Reserved, "y")])
            .collect();
        let placed: Vec<(u32, String)> = pairs
            .iter()
            .map(|p| (p.rva, format!("m{}.obj", p.rva / 0x100)))
            .collect();
        let b = by_member(&pairs, &placed);
        assert_eq!(b.verdict, Purity::OneSided);
        assert_eq!(b.unattributed, 0);
        let mixed = vec![
            pair(0x100, Verdict::Tight, "a"),
            pair(0x200, Verdict::Reserved, "b"),
        ];
        let placed = vec![
            (0x100, "one.obj".to_string()),
            (0x200, "one.obj".to_string()),
        ];
        let b = by_member(&mixed, &placed);
        assert_eq!(b.mixed, vec![("one.obj".to_string(), 1, 1)]);
    }

    #[test]
    fn spellings_cover_c_names_ctors_dtors_and_methods() {
        assert_eq!(spellings("_malloc"), vec!["_malloc", "malloc"]);
        assert_eq!(spellings("helper"), vec!["helper"]);
        assert_eq!(
            spellings("??0Widget@@QAE@XZ"),
            vec!["Widget::Widget", "Widget"]
        );
        assert_eq!(
            spellings("??1Widget@@QAE@XZ"),
            vec!["Widget::~Widget", "Widget"]
        );
        assert_eq!(
            spellings("?Draw@Widget@@QAEXXZ"),
            vec!["Widget::Draw", "Draw"]
        );
        assert_eq!(spellings("?Run@@YAXXZ"), vec!["Run"]);
    }

    #[test]
    fn every_rdata_section_of_an_object_counts() {
        let secs = vec![
            (".drectve".to_string(), 58),
            (".rdata".to_string(), 4),
            (".text".to_string(), 83),
            (".rdata".to_string(), 4),
            (".rdata".to_string(), 12),
        ];
        assert_eq!(rdata_total(&secs), 20);
        assert_eq!(rdata_total(&[]), 0);
    }

    #[test]
    fn spacing_names_the_direction_and_scores_only_pairs_adjacent_in_both() {
        assert_eq!(spacing_of(144, 176), Spacing::Tighter(32));
        assert_eq!(spacing_of(176, 176), Spacing::Match);
        assert_eq!(spacing_of(208, 176), Spacing::Looser(32));
        let ours = vec![
            ("a".to_string(), 0x100, "x.obj".to_string()),
            ("b".to_string(), 0x190, "x.obj".to_string()),
            ("c".to_string(), 0x300, "x.obj".to_string()),
            ("d".to_string(), 0x400, "y.obj".to_string()),
        ];
        let refs: BTreeMap<String, u32> =
            [("a", 0x1000), ("b", 0x10b0), ("z", 0x1100), ("c", 0x1200)]
                .iter()
                .map(|(n, r)| ((*n).to_string(), *r))
                .collect();
        let s = spacing(&ours, &refs);
        assert_eq!((s.matched, s.tighter, s.looser), (0, 1, 0));
        assert_eq!((s.no_ref, s.not_adjacent), (0, 1));
        let mut aliased = refs.clone();
        aliased.insert("?b@@YAXXZ".to_string(), 0x10b0);
        aliased.insert("?a@@YAXXZ".to_string(), 0x1000);
        let s = spacing(&ours, &aliased);
        assert_eq!((s.matched, s.tighter, s.looser), (0, 1, 0));
        assert_eq!((s.no_ref, s.not_adjacent), (0, 1));
    }

    #[test]
    fn a_tail_verdict_needs_sixty_four_bytes() {
        assert_eq!(tail_of(20, 0, 20), Tail::Unreadable);
        assert_eq!(tail_of(128, 0, 128), Tail::Occupied);
        assert_eq!(tail_of(128, 128, 0), Tail::Reversed);
        assert_eq!(tail_of(128, 128, 128), Tail::BothEmpty);
        assert_eq!(tail_of(128, 64, 64), Tail::Mixed);
    }
}
