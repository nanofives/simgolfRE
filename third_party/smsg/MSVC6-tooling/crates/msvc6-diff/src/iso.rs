use crate::comdat::Comdat;
use anyhow::{Context as _, Result, bail};
use msvc6_base::digest::md5_hex;
use msvc6_source::defs::{self, Def};
use msvc6_source::lex::{self, Cond, Kind, Lexed};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const PAD_NAME: &str = "iso_counter_pad";
pub const PAD_TRIES: usize = 3;
pub const LABEL_MODULUS: u64 = 64;
pub const META: &str = "iso.json";
pub const META_FORMAT: u32 = 1;
const MAX_META: u64 = 1 << 20;
const INLINE_WORDS: [&str; 4] = ["inline", "__inline", "__forceinline", "template"];

const OPERATORS: [(&str, &str); 42] = [
    ("?_0", "operator/="),
    ("?_1", "operator%="),
    ("?_2", "operator>>="),
    ("?_3", "operator<<="),
    ("?_4", "operator&="),
    ("?_5", "operator|="),
    ("?_6", "operator^="),
    ("?_U", "operator new[]"),
    ("?_V", "operator delete[]"),
    ("?2", "operator new"),
    ("?3", "operator delete"),
    ("?4", "operator="),
    ("?5", "operator>>"),
    ("?6", "operator<<"),
    ("?7", "operator!"),
    ("?8", "operator=="),
    ("?9", "operator!="),
    ("?A", "operator[]"),
    ("?C", "operator->"),
    ("?D", "operator*"),
    ("?E", "operator++"),
    ("?F", "operator--"),
    ("?G", "operator-"),
    ("?H", "operator+"),
    ("?I", "operator&"),
    ("?J", "operator->*"),
    ("?K", "operator/"),
    ("?L", "operator%"),
    ("?M", "operator<"),
    ("?N", "operator<="),
    ("?O", "operator>"),
    ("?P", "operator>="),
    ("?Q", "operator,"),
    ("?R", "operator()"),
    ("?S", "operator~"),
    ("?T", "operator^"),
    ("?U", "operator|"),
    ("?V", "operator&&"),
    ("?W", "operator||"),
    ("?X", "operator*="),
    ("?Y", "operator+="),
    ("?Z", "operator-="),
];

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Target {
    pub base: String,
    pub scope: Option<String>,
}

fn c_name(sym: &str) -> Option<String> {
    let s = sym
        .strip_prefix('_')
        .or_else(|| sym.strip_prefix('@'))
        .unwrap_or(sym);
    let s = match s.rsplit_once('@') {
        Some((h, t)) if !t.is_empty() && t.bytes().all(|b| b.is_ascii_digit()) => h,
        _ => s,
    };
    let ok = !s.is_empty()
        && s.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_')
        && !s.as_bytes().first().is_some_and(u8::is_ascii_digit);
    ok.then(|| s.to_string())
}

impl Target {
    #[must_use]
    pub fn of_symbol(sym: &str) -> Option<Self> {
        let Some(rest) = sym.strip_prefix('?') else {
            return c_name(sym).map(|base| Self { base, scope: None });
        };
        if let Some(r) = rest.strip_prefix("?0").or_else(|| rest.strip_prefix("?1")) {
            let class = r.split('@').next().filter(|c| is_word(c))?;
            let base = if rest.starts_with("?1") {
                format!("~{class}")
            } else {
                class.to_string()
            };
            return Some(Self {
                base,
                scope: Some(class.to_string()),
            });
        }
        if let Some((code, spelt)) = OPERATORS.iter().find(|(c, _)| rest.starts_with(c)) {
            let after = rest.get(code.len()..)?;
            let scope = after
                .split('@')
                .next()
                .filter(|c| !c.is_empty())
                .map(str::to_string);
            if scope.as_deref().is_some_and(|s| !is_word(s)) {
                return None;
            }
            return Some(Self {
                base: (*spelt).to_string(),
                scope,
            });
        }
        let end = rest.find("@@")?;
        let mut parts = rest.get(..end)?.split('@');
        let base = parts.next().filter(|b| is_word(b))?.to_string();
        let scope = parts.next().map(str::to_string);
        if scope.as_deref().is_some_and(|s| !is_word(s)) {
            return None;
        }
        Some(Self { base, scope })
    }

    #[must_use]
    pub fn of_name(name: &str) -> Self {
        let name = name.trim();
        if name.starts_with('?')
            && let Some(t) = Self::of_symbol(name)
        {
            return t;
        }
        if !name.contains("::")
            && (name.starts_with('_') || name.starts_with('@'))
            && let Some(base) = c_name(name)
        {
            return Self { base, scope: None };
        }
        let mut parts = name.rsplit("::");
        let base = parts.next().unwrap_or(name).to_string();
        Self {
            base,
            scope: parts.next().map(str::to_string),
        }
    }

    #[must_use]
    pub fn names(&self, qualified: &str) -> bool {
        let mut parts = qualified.rsplit("::");
        if parts.next() != Some(self.base.as_str()) {
            return false;
        }
        match &self.scope {
            None => parts.next().is_none(),
            Some(s) => parts.next() == Some(s.as_str()),
        }
    }

    #[must_use]
    pub fn names_base(&self, qualified: &str) -> bool {
        qualified.rsplit("::").next() == Some(self.base.as_str())
    }

    #[must_use]
    pub fn text(&self) -> String {
        match &self.scope {
            Some(s) => format!("{s}::{}", self.base),
            None => self.base.clone(),
        }
    }
}

fn is_word(s: &str) -> bool {
    !s.is_empty()
        && s.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_')
        && !s.as_bytes().first().is_some_and(u8::is_ascii_digit)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Fate {
    Target,
    Helper,
    Kept,
    Dropped,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Item {
    pub name: String,
    pub first_line: usize,
    pub last_line: usize,
    pub fate: Fate,
    pub why: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Reduced {
    pub text: Vec<u8>,
    pub items: Vec<Item>,
    pub target_line: usize,
    pub pad: usize,
    pub pad_refused: Option<String>,
    pub tail: usize,
}

impl Reduced {
    #[must_use]
    pub fn names(&self, f: Fate) -> Vec<String> {
        self.items
            .iter()
            .filter(|i| i.fate == f)
            .map(|i| i.name.clone())
            .collect()
    }

    #[must_use]
    pub fn count(&self, f: Fate) -> usize {
        self.items.iter().filter(|i| i.fate == f).count()
    }

    #[must_use]
    pub fn lines(&self) -> usize {
        count_lines(&self.text)
    }
}

#[must_use]
pub fn count_lines(b: &[u8]) -> usize {
    b.iter().filter(|&&c| c == b'\n').count()
        + usize::from(!b.is_empty() && b.last() != Some(&b'\n'))
}

fn blank(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\r' | 0x0b | 0x0c)
}

fn only_ws(b: &[u8]) -> bool {
    b.iter().all(|&c| blank(c))
}

struct Text<'a> {
    src: &'a [u8],
    lx: Lexed,
    lines: Vec<(usize, usize, usize)>,
    logical: Vec<usize>,
    busy: Vec<bool>,
}

fn mark(v: &mut [bool], from: usize, to: usize) {
    for l in from..=to {
        if let Some(b) = v.get_mut(l) {
            *b = true;
        }
    }
}

#[must_use]
pub fn logical_lines(lx: &Lexed, physical: usize) -> Vec<usize> {
    let pins: BTreeMap<usize, usize> = lx
        .directives
        .iter()
        .filter(|d| d.name == "line" && d.cond != Cond::Dead)
        .filter_map(|d| {
            let n = lx.args(d).first()?.text.parse::<usize>().ok()?;
            (n > 0).then_some((d.last, n))
        })
        .collect();
    let mut out = vec![0usize; physical + 2];
    let mut next = 1usize;
    for (p, slot) in out.iter_mut().enumerate().skip(1) {
        *slot = next;
        next = pins
            .get(&p)
            .copied()
            .unwrap_or_else(|| next.saturating_add(1));
    }
    out
}

impl<'a> Text<'a> {
    fn new(src: &'a [u8]) -> Self {
        let lx = lex::lex(src);
        let lines = lex::physical_lines(src);
        let mut busy = vec![false; lines.len() + 2];
        for t in &lx.tokens {
            mark(&mut busy, t.line, t.end_line);
        }
        for d in &lx.directives {
            mark(&mut busy, d.first, d.last);
        }
        let logical = logical_lines(&lx, lines.len());
        Self {
            src,
            lx,
            lines,
            logical,
            busy,
        }
    }

    fn line_of(&self, off: usize) -> usize {
        self.lines.partition_point(|l| l.2 <= off) + 1
    }

    fn start_of(&self, line: usize) -> usize {
        line.checked_sub(1)
            .and_then(|k| self.lines.get(k))
            .map_or(self.src.len(), |l| l.0)
    }

    fn bol(&self, off: usize) -> bool {
        off == 0 || self.src.get(off - 1) == Some(&b'\n')
    }

    fn logical_of_line(&self, line: usize) -> usize {
        self.logical.get(line).copied().unwrap_or(line)
    }

    fn logical_at(&self, off: usize) -> usize {
        self.logical_of_line(self.line_of(off))
    }

    fn logical_before(&self, off: usize) -> usize {
        if self.bol(off) {
            self.logical_at(off)
        } else {
            self.logical_at(off.saturating_sub(1))
        }
    }

    fn straddles(&self, at: usize) -> Option<usize> {
        self.lx
            .comments
            .iter()
            .find(|c| c.start < at && at < c.end)
            .map(|c| c.last)
    }

    fn head_start(&self, line: usize, start: usize) -> Option<usize> {
        let s = self.start_of(line);
        only_ws(self.src.get(s..start).unwrap_or_default()).then_some(s)
    }

    fn walk_back(&self, head: usize, fallback: usize) -> usize {
        let mut first = head;
        while first > 1 && !self.busy.get(first - 1).copied().unwrap_or(true) {
            first -= 1;
        }
        while first < head {
            match self.straddles(self.start_of(first)) {
                Some(last) => first = last + 1,
                None => return self.start_of(first),
            }
        }
        let s = self.start_of(head);
        if self.straddles(s).is_some() {
            fallback
        } else {
            s
        }
    }

    fn tail_end(&self, line: usize, end: usize) -> usize {
        let Some(&(_, body, next)) = line.checked_sub(1).and_then(|k| self.lines.get(k)) else {
            return end;
        };
        let mut p = end;
        loop {
            while p < body && self.src.get(p).is_some_and(|&b| blank(b)) {
                p += 1;
            }
            if p >= body {
                return next;
            }
            match self.lx.comments.iter().find(|c| c.start == p) {
                Some(c) if c.last == line && c.end > p => p = c.end,
                _ => return end,
            }
        }
    }

    fn region(&self, d: &Def) -> (usize, usize) {
        let start = match self.head_start(d.first_line, d.start) {
            Some(_) => self.walk_back(d.first_line, d.start),
            None => d.start,
        };
        (start, self.tail_end(d.last_line, d.end))
    }

    fn balanced(&self, a: usize, b: usize) -> bool {
        let mut depth = 0usize;
        for d in self
            .lx
            .directives
            .iter()
            .filter(|d| d.start >= a && d.start < b)
        {
            match d.name.as_str() {
                "if" | "ifdef" | "ifndef" => depth += 1,
                "elif" | "else" if depth == 0 => return false,
                "endif" => match depth.checked_sub(1) {
                    Some(n) => depth = n,
                    None => return false,
                },
                _ => {}
            }
        }
        depth == 0
    }

    fn token_range(&self, a: usize, b: usize) -> std::ops::Range<usize> {
        let lo = self.lx.tokens.partition_point(|t| t.start < a);
        let hi = self.lx.tokens.partition_point(|t| t.start < b);
        lo..hi.max(lo)
    }

    fn ids(&self, a: usize, b: usize) -> BTreeSet<String> {
        self.lx
            .tokens
            .get(self.token_range(a, b))
            .unwrap_or_default()
            .iter()
            .filter(|t| t.kind == Kind::Ident && t.cond != Cond::Dead)
            .map(|t| t.text.clone())
            .collect()
    }

    fn def_ids(&self, d: &Def) -> BTreeSet<String> {
        let toks = &self.lx.tokens;
        let mut q = d.name_tok;
        if q > d.head && toks.get(q - 1).is_some_and(|t| t.is_punct("~")) {
            q -= 1;
        }
        while q >= d.head + 2
            && toks.get(q - 1).is_some_and(|t| t.is_punct("::"))
            && toks.get(q - 2).is_some_and(|t| t.is_ident())
        {
            q -= 2;
        }
        let end = self.token_range(d.start, d.end).end;
        let pick = |r: std::ops::Range<usize>| {
            toks.get(r)
                .unwrap_or_default()
                .iter()
                .filter(|t| t.kind == Kind::Ident && t.cond != Cond::Dead)
                .map(|t| t.text.clone())
                .collect::<Vec<_>>()
        };
        pick(d.head..q.max(d.head))
            .into_iter()
            .chain(pick(d.params.0.max(q)..end))
            .collect()
    }

    fn inline(&self, d: &Def) -> bool {
        self.lx
            .tokens
            .get(d.head..d.name_tok.max(d.head))
            .unwrap_or_default()
            .iter()
            .any(|t| t.in_code() && INLINE_WORDS.contains(&t.text.as_str()))
    }

    fn pad_regions(&self, inside: &[bool]) -> Vec<(usize, usize)> {
        let t = &self.lx.tokens;
        let mut out = Vec::new();
        for (i, x) in t.iter().enumerate() {
            let starts = x.is("extern")
                && x.in_code()
                && !inside.get(i).copied().unwrap_or(false)
                && t.get(i + 1).is_some_and(|n| n.is("int"))
                && t.get(i + 2)
                    .is_some_and(|n| n.is_ident() && n.text.starts_with(PAD_NAME));
            if !starts {
                continue;
            }
            let Some(semi) = t
                .iter()
                .skip(i + 3)
                .find(|n| n.in_code() && (n.is_punct(";") || n.is_punct("{") || n.is_punct("}")))
                .filter(|n| n.is_punct(";"))
            else {
                continue;
            };
            let start = self.head_start(x.line, x.start).unwrap_or(x.start);
            let mut end = self.tail_end(semi.end_line, semi.end);
            if self.bol(end) {
                let next = self.line_of(end);
                if let Some(d) = self
                    .lx
                    .directives
                    .iter()
                    .find(|d| d.name == "line" && d.first == next && d.cond != Cond::Dead)
                {
                    end = self.start_of(d.last + 1);
                }
            }
            out.push((start, end));
        }
        out
    }
}

struct Cand<'d> {
    def: &'d Def,
    region: (usize, usize),
    ids: BTreeSet<String>,
    internal: bool,
    inline: bool,
    fate: Option<Fate>,
    why: String,
}

fn key_of(base: &str) -> &str {
    base.trim_start_matches('~')
}

struct Out<'t, 'a> {
    t: &'t Text<'a>,
    buf: Vec<u8>,
    cur: usize,
    bol: bool,
    padded: bool,
}

impl Out<'_, '_> {
    fn newline(&mut self) {
        self.buf.push(b'\n');
        self.cur = self.cur.saturating_add(1);
        self.bol = true;
    }

    fn line(&mut self, n: usize) {
        if !self.bol {
            self.newline();
        }
        self.buf
            .extend_from_slice(format!("#line {n}\n").as_bytes());
        self.cur = n;
        self.bol = true;
    }

    fn kept(&mut self, a: usize, b: usize) {
        if a >= b {
            return;
        }
        let want = self.t.logical_at(a);
        if !self.bol && (self.t.bol(a) || self.cur != want) {
            self.newline();
        }
        if self.cur != want {
            self.line(want);
        }
        self.buf
            .extend_from_slice(self.t.src.get(a..b).unwrap_or_default());
        self.cur = self.t.logical_before(b);
        self.bol = self.t.bol(b);
    }

    fn pad(&mut self, k: usize, at: usize) {
        if !self.bol {
            self.newline();
        }
        self.buf.extend_from_slice(pad_line(k).as_bytes());
        self.bol = true;
        self.padded = true;
        self.line(self.t.logical_at(at));
    }
}

#[must_use]
pub fn pad_line(k: usize) -> String {
    let names: Vec<String> = (0..k).map(|i| format!("{PAD_NAME}{i}")).collect();
    format!("extern int {};\n", names.join(", "))
}

#[must_use]
pub fn line_texts(src: &[u8]) -> BTreeMap<usize, String> {
    let t = Text::new(src);
    let directive_lines: BTreeSet<usize> =
        t.lx.directives
            .iter()
            .filter(|d| d.name == "line")
            .flat_map(|d| d.first..=d.last)
            .collect();
    let mut out = BTreeMap::new();
    for (k, &(a, body, _)) in t.lines.iter().enumerate() {
        let p = k + 1;
        let text = msvc6_base::srcio::decode(src.get(a..body).unwrap_or_default());
        if directive_lines.contains(&p)
            || text
                .trim_start()
                .starts_with(&format!("extern int {PAD_NAME}"))
        {
            continue;
        }
        out.entry(t.logical_of_line(p))
            .or_insert_with(|| text.trim().to_string());
    }
    out
}

#[must_use]
pub fn defines(src: &[u8], target: &Target) -> Vec<String> {
    defs::definitions(&lex::lex(src))
        .iter()
        .map(Def::qualified)
        .filter(|q| target.names(q) || (target.scope.is_none() && target.names_base(q)))
        .collect()
}

pub fn reduce(src: &[u8], target: &Target, pad: usize) -> Result<Reduced> {
    let t = Text::new(src);
    let all = defs::definitions(&t.lx);
    let targets: Vec<&Def> = all
        .iter()
        .filter(|d| target.names(&d.qualified()))
        .collect();
    let Some(first) = targets.iter().min_by_key(|d| d.start).copied() else {
        bail!("no definition of `{}` in this file", target.text());
    };
    let last_start = targets.iter().map(|d| d.start).max().unwrap_or(first.start);
    let mut inside = vec![false; t.lx.tokens.len()];
    let mut cands: Vec<Cand<'_>> = Vec::new();
    for d in all.iter().filter(|d| !d.in_class) {
        let region = t.region(d);
        for slot in inside
            .get_mut(t.token_range(d.start, d.end))
            .unwrap_or_default()
        {
            *slot = true;
        }
        let is_target = target.names(&d.qualified());
        let balanced = t.balanced(region.0, region.1);
        let (fate, why) = if is_target {
            (Some(Fate::Target), "the target".to_string())
        } else if balanced {
            (None, String::new())
        } else {
            (
                Some(Fate::Kept),
                "a conditional group crosses its bounds".to_string(),
            )
        };
        cands.push(Cand {
            def: d,
            region,
            ids: t.def_ids(d),
            internal: d.is_static,
            inline: t.inline(d),
            fate,
            why,
        });
    }
    let pads = t.pad_regions(&inside);
    let in_pad = |off: usize| pads.iter().any(|&(a, b)| off >= a && off < b);
    let mut refs: BTreeSet<String> = BTreeSet::new();
    let mut called: BTreeSet<String> = BTreeSet::new();
    for (i, tok) in t.lx.tokens.iter().enumerate() {
        if inside.get(i).copied().unwrap_or(false)
            || tok.kind != Kind::Ident
            || tok.cond == Cond::Dead
            || in_pad(tok.start)
        {
            continue;
        }
        if tok.directive.is_some() {
            called.insert(tok.text.clone());
        }
        refs.insert(tok.text.clone());
    }
    for d in targets.iter().filter(|d| d.in_class) {
        called.extend(t.ids(d.start, d.end));
    }
    for c in cands.iter().filter(|c| c.fate.is_some()) {
        refs.extend(c.ids.iter().cloned());
        called.extend(c.ids.iter().cloned());
    }
    loop {
        let mut grew = false;
        for c in cands.iter_mut().filter(|c| c.fate.is_none()) {
            let key = key_of(&c.def.base);
            let why = if (c.internal || c.inline) && refs.contains(key) {
                "static or inline, named by a kept item"
            } else if c.def.start < last_start && called.contains(key) {
                "defined ahead of the target, named by a kept body"
            } else {
                continue;
            };
            c.fate = Some(Fate::Helper);
            c.why = why.to_string();
            refs.extend(c.ids.iter().cloned());
            called.extend(c.ids.iter().cloned());
            grew = true;
        }
        if !grew {
            break;
        }
    }
    let mut drops: Vec<(usize, usize)> = pads.clone();
    for c in &mut cands {
        if c.fate.is_none() {
            c.fate = Some(Fate::Dropped);
            c.why = "not named by a kept item".to_string();
        }
        if c.fate == Some(Fate::Dropped) {
            drops.push(c.region);
        }
    }
    drops.sort_unstable();
    let pad_refused = (pad > 0 && first.in_class).then(|| {
        format!(
            "`{}` is defined inside its class: no counter pad can precede it",
            first.qualified()
        )
    });
    let anchor = (pad > 0 && pad_refused.is_none()).then(|| {
        t.head_start(first.first_line, first.start)
            .unwrap_or(first.start)
    });
    let mut out = Out {
        t: &t,
        buf: Vec::with_capacity(src.len()),
        cur: 1,
        padded: false,
        bol: true,
    };
    let mut at = 0usize;
    let mut spans: Vec<(usize, usize)> = Vec::new();
    for &(a, b) in &drops {
        if a > at {
            spans.push((at, a));
        }
        at = at.max(b);
    }
    if at < src.len() {
        spans.push((at, src.len()));
    }
    let mut last_code = 0usize;
    for (a, b) in spans {
        if let Some(p) = src
            .get(a..b)
            .and_then(|s| s.iter().rposition(|c| !c.is_ascii_whitespace()))
        {
            last_code = last_code.max(t.logical_at(a + p));
        }
        match anchor.filter(|&x| x >= a && x < b) {
            Some(x) => {
                out.kept(a, x);
                out.pad(pad, x);
                out.kept(x, b);
            }
            None => out.kept(a, b),
        }
    }
    let physical = count_lines(&out.buf);
    let mut tail = 0usize;
    if physical < last_code {
        if !out.bol {
            out.newline();
        }
        out.buf
            .extend(std::iter::repeat_n(b'\n', last_code - physical));
        tail = last_code - physical;
    }
    let mut items: Vec<Item> = cands
        .iter()
        .map(|c| Item {
            name: c.def.qualified(),
            first_line: t.logical_of_line(c.def.first_line),
            last_line: t.logical_of_line(c.def.last_line),
            fate: c.fate.unwrap_or(Fate::Dropped),
            why: c.why.clone(),
        })
        .collect();
    for d in targets.iter().filter(|d| d.in_class) {
        items.push(Item {
            name: d.qualified(),
            first_line: t.logical_of_line(d.first_line),
            last_line: t.logical_of_line(d.last_line),
            fate: Fate::Target,
            why: "the target, defined in its class".to_string(),
        });
    }
    items.sort_by_key(|i| i.first_line);
    Ok(Reduced {
        text: out.buf,
        items,
        target_line: t.logical_of_line(first.first_line),
        pad: if out.padded { pad } else { 0 },
        pad_refused,
        tail,
    })
}

pub const NUMBERED: [&str; 3] = ["$SG", "$L", "_$E"];

pub const GUARD: &str = "?$S";

fn digit_run(s: &str) -> usize {
    s.bytes().take_while(u8::is_ascii_digit).count()
}

#[must_use]
pub fn unnumbered(name: &str) -> String {
    for p in NUMBERED {
        if let Some(rest) = name.strip_prefix(p)
            && !rest.is_empty()
            && digit_run(rest) == rest.len()
        {
            return p.to_string();
        }
    }
    if let Some(rest) = name.strip_prefix(GUARD) {
        let n = digit_run(rest);
        if let Some(tail) = rest.get(n..)
            && n > 0
            && tail.starts_with('@')
        {
            return format!("{GUARD}{tail}");
        }
    }
    name.to_string()
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Proof {
    pub len: (usize, usize),
    pub bytes: usize,
    pub first_byte: Option<usize>,
    pub relocs: usize,
    pub first_reloc: Option<String>,
    pub renumbered: Vec<(String, String)>,
    pub md5: (String, String),
}

impl Proof {
    #[must_use]
    pub fn faithful(&self) -> bool {
        self.len.0 == self.len.1 && self.bytes == 0 && self.relocs == 0
    }
}

fn reloc_text(r: &crate::comdat::Reloc) -> String {
    format!("+{:#x} type {} -> {}", r.at, r.kind, r.symbol)
}

#[must_use]
pub fn prove(tu: &Comdat, iso: &Comdat) -> Proof {
    let pairs = || tu.bytes.iter().zip(&iso.bytes);
    let mut p = Proof {
        len: (tu.bytes.len(), iso.bytes.len()),
        bytes: pairs().filter(|(x, y)| x != y).count(),
        first_byte: pairs().position(|(x, y)| x != y),
        relocs: tu.relocations.len().abs_diff(iso.relocations.len()),
        md5: (md5_hex(&tu.bytes), md5_hex(&iso.bytes)),
        ..Proof::default()
    };
    for (x, y) in tu.relocations.iter().zip(&iso.relocations) {
        if (x.at, x.kind) != (y.at, y.kind) || unnumbered(&x.symbol) != unnumbered(&y.symbol) {
            p.relocs += 1;
            if p.first_reloc.is_none() {
                p.first_reloc = Some(format!("{} vs {}", reloc_text(x), reloc_text(y)));
            }
        } else if x.symbol != y.symbol {
            p.renumbered.push((x.symbol.clone(), y.symbol.clone()));
        }
    }
    if p.first_reloc.is_none() && tu.relocations.len() != iso.relocations.len() {
        p.first_reloc = Some(format!(
            "{} relocation(s) vs {}",
            tu.relocations.len(),
            iso.relocations.len()
        ));
    }
    p
}

#[must_use]
pub fn pad_step(tu: u64, iso: u64, pad: usize) -> Option<usize> {
    let m = LABEL_MODULUS;
    let d = usize::try_from((tu % m + m - iso % m) % m).ok()?;
    let modulus = usize::try_from(m).ok()?;
    (d != 0).then(|| (pad + d) % modulus)
}

#[must_use]
pub fn suspects(p: &Proof, labels: (Option<u64>, Option<u64>), pch: bool) -> Vec<String> {
    let mut out = Vec::new();
    if let (Some(x), Some(y)) = labels
        && x % LABEL_MODULUS != y % LABEL_MODULUS
    {
        out.push(format!(
            "C1's label counter mod {LABEL_MODULUS} moved (TU $L{x} = {}, iso $L{y} = {}): a label-keyed C2 tie",
            x % LABEL_MODULUS,
            y % LABEL_MODULUS
        ));
    }
    if let Some((a, b)) = p.renumbered.iter().find(|(a, _)| a.contains("_$E")) {
        out.push(format!(
            "an `_$E` number moved ({a} vs {b}): a file-scope object the dropped text declared"
        ));
    }
    if pch {
        out.push(
            "a pch boundary: the TU compiles with a precompiled header, whose prefix the iso may end elsewhere"
                .to_string(),
        );
    }
    out.push(
        "an inline closure or a shared literal: a body or string the dropped definitions instantiated"
            .to_string(),
    );
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum Verdict {
    Faithful,
    Differs,
    Unproven,
}

impl Verdict {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::Faithful => "FAITHFUL",
            Self::Differs => "DIFFERS",
            Self::Unproven => "UNPROVEN",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Meta {
    pub format: u32,
    pub image: String,
    pub rva: u32,
    pub tu: PathBuf,
    pub args: Option<PathBuf>,
    pub iso: PathBuf,
    pub symbol: String,
    pub verdict: Verdict,
    pub pad: usize,
}

impl Meta {
    #[must_use]
    pub fn load(dir: &Path) -> Option<Self> {
        let p = dir.join(META);
        if std::fs::metadata(&p).ok()?.len() > MAX_META {
            return None;
        }
        let text = msvc6_base::srcio::own_read(&p).ok()?;
        serde_json::from_str::<Self>(&text)
            .ok()
            .filter(|m| m.format == META_FORMAT && !m.symbol.is_empty())
    }

    pub fn save(&self, dir: &Path) -> Result<PathBuf> {
        let p = dir.join(META);
        let mut text = serde_json::to_string_pretty(self)?;
        text.push('\n');
        msvc6_base::srcio::own_write(&p, text)
            .with_context(|| format!("writing {}", p.display()))?;
        Ok(p)
    }

    #[must_use]
    pub fn args_tu(&self) -> &Path {
        self.args.as_deref().unwrap_or(&self.tu)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::comdat::Reloc;

    const TU: &str = "/* banner */\n#include \"a.h\"\n#define K 3\nint g_x;\nstatic int s_tab[] = { 1, 2 };\n\n\
/* the helper */\nstatic int Helper(int a)\n{\n    return a + K;\n}\n\nstatic int Unused(int a) { return a; }\n\n\
static int Deep(int a)\n{\n    return a * 3;\n}\n\nstatic int Mid(int a)\n{\n    return Deep(a) + 1;\n}\n\n\
/* a plain function the target calls */\nint Plain(int a, int b)\n{\n    if (a) { return b; }\n    return 0;\n}\n\n\
void Other(void)\n{\n    const char *s = \"} { not code\";\n    char c = '}';\n    g_x = 1;\n}\n\n\
struct S { int f(int); int g() { return 7; } };\nint S::f(int a) { return a; }\n\n\
inline int Twice(int a) { return a + a; }\n\ntemplate <class T> T Biggest(T a, T b) { return a > b ? a : b; }\n\n\
#define BODY(x) ((x) + 1)\nint Later(int a);\nint Target(int gi)\n{\n    const char *s = \"}\";\n    /* } */\n    \
return Helper(gi) + Plain(gi, 2) + Later(gi) + BODY(s_tab[0]) + Mid(gi) + Twice(gi) + Biggest(gi, 1);\n}\n\n\
int Target(double d)\n{\n    return (int)d;\n}\n\nint Later(int a) { return a * 2; }\n";

    fn reduced(src: &str, pad: usize) -> (Reduced, String) {
        let r = reduce(src.as_bytes(), &Target::of_name("Target"), pad).unwrap();
        let text = msvc6_base::srcio::decode(&r.text);
        (r, text)
    }

    fn logical_line_of(text: &str, needle: &str) -> usize {
        let lx = lex::lex(text.as_bytes());
        let phys = text.lines().position(|l| l == needle).unwrap() + 1;
        logical_lines(&lx, text.lines().count())[phys]
    }

    #[test]
    fn every_overload_of_the_target_and_its_closure_are_kept() {
        let (r, text) = reduced(TU, 0);
        assert_eq!(r.names(Fate::Target), vec!["Target", "Target"]);
        assert_eq!(
            r.names(Fate::Helper),
            vec!["Helper", "Deep", "Mid", "Plain", "Twice", "Biggest"]
        );
        assert_eq!(
            r.names(Fate::Dropped),
            vec!["Unused", "Other", "S::f", "Later"]
        );
        assert!(text.contains("    if (a) { return b; }"), "{text}");
        assert!(text.contains("return Deep(a) + 1;"), "{text}");
        assert!(text.contains("int Later(int a);\n"), "{text}");
        assert!(!text.contains("return a * 2"), "{text}");
        assert!(!text.contains("not code"), "{text}");
        assert!(!text.contains("g_x = 1"), "{text}");
        assert!(text.contains("struct S { int f(int); int g() { return 7; } };"));
        assert!(text.contains("#define BODY(x)"));
        assert!(text.contains("return (int)d;"));
        assert!(!text.contains("a plain function") || text.contains("int Plain"));
    }

    #[test]
    fn kept_text_keeps_its_tu_line_numbers() {
        let (r, text) = reduced(TU, 0);
        for needle in [
            "int Target(int gi)",
            "int Target(double d)",
            "#define BODY(x) ((x) + 1)",
            "int Plain(int a, int b)",
            "inline int Twice(int a) { return a + a; }",
        ] {
            let tu = TU.lines().position(|l| l == needle).unwrap() + 1;
            assert_eq!(logical_line_of(&text, needle), tu, "{needle}\n{text}");
        }
        let tu = TU.lines().position(|l| l == "int Target(int gi)").unwrap() + 1;
        assert_eq!(r.target_line, tu);
        assert!(text.contains("#line "), "{text}");
    }

    #[test]
    fn the_iso_reaches_its_last_code_line_and_reads_back_by_tu_number() {
        let (r, _) = reduced(TU, 0);
        let last = TU.lines().position(|l| l == "    return (int)d;").unwrap() + 2;
        assert_eq!(r.lines(), last);
        let texts = line_texts(&r.text);
        for (k, l) in TU.lines().enumerate() {
            if [
                "int Target(int gi)",
                "int Plain(int a, int b)",
                "#define K 3",
            ]
            .contains(&l)
            {
                assert_eq!(texts.get(&(k + 1)).map(String::as_str), Some(l));
            }
        }
        let (p, _) = reduced(TU, 2);
        let texts = line_texts(&p.text);
        assert!(texts.values().all(|t| !t.contains(PAD_NAME)));
        assert_eq!(
            texts.get(&p.target_line).map(String::as_str),
            Some("int Target(int gi)")
        );
    }

    #[test]
    fn reducing_a_reduction_changes_nothing() {
        let (r, _) = reduced(TU, 0);
        let again = reduce(&r.text, &Target::of_name("Target"), 0).unwrap();
        assert_eq!(again.text, r.text);
        assert_eq!(again.count(Fate::Dropped), 0);
        assert_eq!(again.target_line, r.target_line);
    }

    #[test]
    fn the_counter_pad_precedes_the_target_and_is_replaced_not_stacked() {
        let (r, text) = reduced(TU, 3);
        assert_eq!(r.pad, 3);
        let lines: Vec<&str> = text.lines().collect();
        let at = lines
            .iter()
            .position(|l| *l == "int Target(int gi)")
            .unwrap();
        assert!(
            lines[..at]
                .contains(&"extern int iso_counter_pad0, iso_counter_pad1, iso_counter_pad2;")
        );
        assert_eq!(text.matches("extern int iso_counter_pad").count(), 1);
        assert_eq!(logical_line_of(&text, "int Target(int gi)"), r.target_line);
        let same = reduce(&r.text, &Target::of_name("Target"), 3).unwrap();
        assert_eq!(same.text, r.text);
        let again = reduce(&r.text, &Target::of_name("Target"), 5).unwrap();
        let t2 = msvc6_base::srcio::decode(&again.text);
        assert_eq!(t2.matches("extern int iso_counter_pad").count(), 1);
        assert!(t2.contains("iso_counter_pad4;"));
        assert_eq!(logical_line_of(&t2, "int Target(int gi)"), r.target_line);
        let none = reduce(&r.text, &Target::of_name("Target"), 0).unwrap();
        let t3 = msvc6_base::srcio::decode(&none.text);
        assert!(!t3.contains(PAD_NAME));
        assert_eq!(logical_line_of(&t3, "int Target(int gi)"), r.target_line);
    }

    #[test]
    fn braces_in_comments_strings_and_dead_code_do_not_split_items() {
        let src = "#if 0\nvoid Ghost() {\n#endif\n/* { */\nconst char *g = \"{\";\nchar q = '{';\n\
static void Gone() { const char *x = \"}}}\"; }\n// }\nint Target(int a)\n{\n    return a;\n}\n";
        let (r, text) = reduced(src, 0);
        assert_eq!(r.names(Fate::Dropped), vec!["Gone"]);
        assert!(
            text.starts_with("#if 0\nvoid Ghost() {\n#endif\n/* { */\n"),
            "{text}"
        );
        assert!(
            text.contains("const char *g = \"{\";\nchar q = '{';\n"),
            "{text}"
        );
        assert_eq!(
            logical_line_of(&text, "int Target(int a)"),
            src.lines().position(|l| l == "int Target(int a)").unwrap() + 1
        );
    }

    #[test]
    fn a_definition_whose_conditional_crosses_its_bounds_is_kept() {
        let src = "#if A\nvoid Odd() {\n    return;\n#endif\n}\n\nvoid Fine() { }\n\nint Target() { return 1; }\n";
        let r = reduce(src.as_bytes(), &Target::of_name("Target"), 0).unwrap();
        assert_eq!(r.names(Fate::Dropped), vec!["Fine"]);
        assert_eq!(r.names(Fate::Kept), vec!["Odd"]);
        let text = msvc6_base::srcio::decode(&r.text);
        assert!(
            text.starts_with("#if A\nvoid Odd() {\n    return;\n#endif\n}\n"),
            "{text}"
        );
    }

    #[test]
    fn a_target_absent_from_the_file_is_an_error() {
        let e = reduce(b"int f() { return 0; }\n", &Target::of_name("Target"), 0).unwrap_err();
        assert!(e.to_string().contains("no definition of `Target`"));
        assert_eq!(defines(TU.as_bytes(), &Target::of_name("Target")).len(), 2);
        assert!(defines(TU.as_bytes(), &Target::of_name("S::f")).len() == 1);
        assert_eq!(defines(TU.as_bytes(), &Target::of_name("f")), vec!["S::f"]);
        assert!(defines(TU.as_bytes(), &Target::of_name("T::f")).is_empty());
    }

    #[test]
    fn a_member_target_keeps_its_class_and_drops_its_siblings() {
        let src = "class C {\npublic:\n    int a;\n    int Get() const;\n    void Set(int v);\n    C();\n    ~C();\n};\n\n\
C::C() : a(0)\n{\n}\n\nC::~C()\n{\n}\n\nvoid C::Set(int v)\n{\n    a = v;\n}\n\nint C::Get() const\n{\n    C tmp;\n    return a;\n}\n";
        let sym = Target::of_symbol("?Get@C@@QBEHXZ").unwrap();
        let r = reduce(src.as_bytes(), &sym, 0).unwrap();
        assert_eq!(r.names(Fate::Target), vec!["C::Get"]);
        assert_eq!(r.names(Fate::Helper), vec!["C::C", "C::~C"]);
        assert_eq!(r.names(Fate::Dropped), vec!["C::Set"]);
    }

    #[test]
    fn decorated_names_split_into_identifier_and_scope() {
        let t = |s: &str| Target::of_symbol(s).map(|t| (t.base, t.scope));
        assert_eq!(
            t("?ComputeScore@@YAHH@Z"),
            Some(("ComputeScore".into(), None))
        );
        assert_eq!(
            t("?Update@Actor@@QAEXXZ"),
            Some(("Update".into(), Some("Actor".into())))
        );
        assert_eq!(
            t("??0Actor@@QAE@XZ"),
            Some(("Actor".into(), Some("Actor".into())))
        );
        assert_eq!(
            t("??1Actor@@QAE@XZ"),
            Some(("~Actor".into(), Some("Actor".into())))
        );
        assert_eq!(
            t("??4Foo@@QAEAAV0@ABV0@@Z"),
            Some(("operator=".into(), Some("Foo".into())))
        );
        assert_eq!(t("??2@YAPAXI@Z"), Some(("operator new".into(), None)));
        assert_eq!(t("_PackRGB@12"), Some(("PackRGB".into(), None)));
        assert_eq!(t("_strlen"), Some(("strlen".into(), None)));
        assert_eq!(t("??$Max@H@@YAHHH@Z"), None);
        assert_eq!(t("??_GFoo@@UAEPAXI@Z"), None);
        let up = Target::of_name("Actor::Update");
        assert!(up.names("Actor::Update"));
        assert!(up.names("NS::Actor::Update"));
        assert!(!up.names("Update"));
        assert!(!up.names("Other::Update"));
        assert!(Target::of_name("RunFrame").names("RunFrame"));
        assert!(!Target::of_name("RunFrame").names("X::RunFrame"));
    }

    #[test]
    fn numbered_targets_compare_unnumbered() {
        assert_eq!(unnumbered("$SG12345"), "$SG");
        assert_eq!(unnumbered("_$E17"), "_$E");
        assert_eq!(unnumbered("$L45"), "$L");
        assert_eq!(unnumbered("?g_x@@3HA"), "?g_x@@3HA");
        assert_eq!(
            unnumbered("??_C@_05ABCD@hello?$AA@"),
            "??_C@_05ABCD@hello?$AA@"
        );
        assert_eq!(
            unnumbered("__real@8@3ff00000000000000000"),
            "__real@8@3ff00000000000000000"
        );
        assert_eq!(
            unnumbered("?$S1@?1??Get@Pool@@SAAAV2@XZ@4IA"),
            "?$S@?1??Get@Pool@@SAAAV2@XZ@4IA"
        );
        for exact in [
            "??$f@$00@@YAXXZ",
            "??$f@$01@@YAXXZ",
            "??$Grid@$0BA@$07@@QAEXXZ",
            "__unwindfunclet$?f@@YAXXZ$0",
            "__ehhandler$?f@@YAXXZ",
            "$SG",
            "$SG12x",
            "$L45$1",
            "_$E7@4",
            "?$S@?1??f@@YAXXZ@4IA",
            "?$S1",
            "?x$SG12@@3HA",
        ] {
            assert_eq!(unnumbered(exact), exact);
        }
    }

    #[test]
    fn two_value_template_instances_are_two_targets() {
        let one = comdat(&[0xe8, 0, 0, 0, 0, 0xc3], &[(1, "??$f@$00@@YAXXZ")]);
        let two = comdat(&[0xe8, 0, 0, 0, 0, 0xc3], &[(1, "??$f@$01@@YAXXZ")]);
        let p = prove(&one, &two);
        assert!(!p.faithful());
        assert_eq!(p.relocs, 1);
        assert!(p.renumbered.is_empty());
        assert!(
            p.first_reloc
                .as_deref()
                .is_some_and(|r| r.contains("??$f@$00@@YAXXZ") && r.contains("??$f@$01@@YAXXZ")),
            "{:?}",
            p.first_reloc
        );
        assert!(prove(&one, &one.clone()).faithful());
        let funclet = |n: u8| {
            comdat(
                &[0x68, 0, 0, 0, 0, 0xc3],
                &[(1, &format!("__unwindfunclet$?f@@YAXXZ${n}"))],
            )
        };
        assert!(!prove(&funclet(0), &funclet(1)).faithful());
        let guard = |n: u8| {
            comdat(
                &[0xa1, 0, 0, 0, 0, 0xc3],
                &[(1, &format!("?$S{n}@?1??Get@Pool@@SAAAV2@XZ@4IA"))],
            )
        };
        let p = prove(&guard(1), &guard(3));
        assert!(p.faithful(), "{p:?}");
        assert_eq!(p.renumbered.len(), 1);
        let numbered = comdat(
            &[0x68, 0, 0, 0, 0, 0x68, 0, 0, 0, 0, 0xc3],
            &[(1, "$SG2051"), (6, "_$E12")],
        );
        let renumbered = comdat(
            &[0x68, 0, 0, 0, 0, 0x68, 0, 0, 0, 0, 0xc3],
            &[(1, "$SG77"), (6, "_$E3")],
        );
        let p = prove(&numbered, &renumbered);
        assert!(p.faithful(), "{p:?}");
        assert_eq!(p.renumbered.len(), 2);
    }

    fn comdat(bytes: &[u8], relocs: &[(u32, &str)]) -> Comdat {
        Comdat {
            bytes: bytes.to_vec(),
            relocated: vec![false; bytes.len()],
            relocations: relocs
                .iter()
                .map(|(at, s)| Reloc {
                    at: *at,
                    symbol: (*s).to_string(),
                    kind: 6,
                    external: false,
                    global: false,
                    target_section: None,
                    target_offset: 0,
                    target_index: None,
                })
                .collect(),
            ..Comdat::default()
        }
    }

    #[test]
    fn a_proof_tolerates_renumbering_and_nothing_else() {
        let a = comdat(&[0x68, 0, 0, 0, 0, 0xc3], &[(1, "$SG100")]);
        let b = comdat(&[0x68, 0, 0, 0, 0, 0xc3], &[(1, "$SG7")]);
        let p = prove(&a, &b);
        assert!(p.faithful());
        assert_eq!(p.renumbered, vec![("$SG100".into(), "$SG7".into())]);
        let c = comdat(&[0x68, 0, 0, 0, 0, 0xc3], &[(1, "?g@@3HA")]);
        let p = prove(&a, &c);
        assert!(!p.faithful());
        assert_eq!(p.relocs, 1);
        assert!(p.first_reloc.unwrap().contains("?g@@3HA"));
        let d = comdat(&[0x68, 0, 0, 0, 1, 0xc3], &[(1, "$SG100")]);
        let p = prove(&a, &d);
        assert_eq!((p.bytes, p.first_byte), (1, Some(4)));
        let e = comdat(&[0x68, 0, 0, 0, 0, 0xc3, 0x90], &[(1, "$SG100")]);
        assert!(!prove(&a, &e).faithful());
    }

    #[test]
    fn the_pad_moves_the_counter_by_the_difference_mod_64() {
        assert_eq!(pad_step(130, 128, 0), Some(2));
        assert_eq!(pad_step(128, 130, 0), Some(62));
        assert_eq!(pad_step(192, 128, 5), None);
        assert_eq!(pad_step(1, 0, 63), Some(0));
        let p = Proof::default();
        let s = suspects(&p, (Some(130), Some(128)), true);
        assert!(s[0].contains("moved"));
        assert!(s.iter().any(|x| x.contains("pch")));
    }

    #[test]
    fn line_directives_in_the_input_renumber_its_lines() {
        let src = "a\nb\n#line 279\nc\nd\n#line 1602\ne\n";
        let lx = lex::lex(src.as_bytes());
        let m = logical_lines(&lx, 7);
        assert_eq!((m[1], m[2], m[4], m[5], m[7]), (1, 2, 279, 280, 1602));
    }

    #[test]
    fn a_meta_record_round_trips() {
        let d = std::env::temp_dir().join(format!("msvc6-diff-iso-meta-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        let m = Meta {
            format: META_FORMAT,
            image: "IMG".into(),
            rva: 0x1234,
            tu: PathBuf::from("/src/a.cpp"),
            args: None,
            iso: d.join("a.cpp"),
            symbol: "?f@@YAXXZ".into(),
            verdict: Verdict::Faithful,
            pad: 2,
        };
        m.save(&d).unwrap();
        assert_eq!(Meta::load(&d), Some(m));
        msvc6_base::srcio::own_write(d.join(META), "{\"format\": 1}").unwrap();
        assert_eq!(Meta::load(&d), None);
        std::fs::remove_dir_all(&d).unwrap();
    }
}
