use crate::config::Placeholders as Config;
use crate::defs::{self, Def};
use crate::flow;
use crate::lex::{Cond, Kind, Lexed, Token};
use serde::Serialize;
use std::collections::BTreeMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Shape {
    Decompiler,
    Pad,
    RvaSuffix,
    Unknown,
    Spare,
    Unused,
    Probe,
    Slot,
    OffsetField,
    IndexedStem,
    Positional,
}

impl Shape {
    pub const ALL: [Self; 11] = [
        Self::Decompiler,
        Self::Pad,
        Self::RvaSuffix,
        Self::Unknown,
        Self::Spare,
        Self::Unused,
        Self::Probe,
        Self::Slot,
        Self::OffsetField,
        Self::IndexedStem,
        Self::Positional,
    ];

    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Decompiler => "DECOMPILER",
            Self::Pad => "PAD",
            Self::RvaSuffix => "RVA-SUFFIX",
            Self::Unknown => "UNKNOWN",
            Self::Spare => "SPARE",
            Self::Unused => "UNUSED",
            Self::Probe => "PROBE",
            Self::Slot => "SLOT",
            Self::OffsetField => "OFFSET-FIELD",
            Self::IndexedStem => "INDEXED",
            Self::Positional => "POSITIONAL",
        }
    }
}

pub const STEMS: [&str; 3] = ["sub_", "func_", "Stub"];

pub const VALUE_NUMBERS: [&str; 11] = [
    "11025", "22050", "44100", "48000", "16384", "32768", "65535", "65536", "100000", "115200",
    "1000000",
];

pub const DESCRIPTIVE_NUMBERS: [&str; 14] = [
    "16", "32", "64", "256", "555", "565", "888", "1555", "4444", "8888", "90", "180", "270", "360",
];

pub const UNUSED_VERBS: [&str; 18] = [
    "purge", "free", "release", "drop", "discard", "remove", "delete", "trim", "collect", "count",
    "find", "reclaim", "evict", "flush", "prune", "sweep", "clear", "skip",
];

const IDA_PREFIXES: [&str; 16] = [
    "dword_", "byte_", "word_", "qword_", "tbyte_", "off_", "stru_", "flt_", "dbl_", "asc_",
    "unk_", "loc_", "locret_", "sub_", "def_", "jpt_",
];

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Classifier {
    pub stems: Vec<String>,
    pub pad_suffixes: Vec<String>,
    pub pad_prefixes: Vec<String>,
    pub value_numbers: Vec<String>,
    pub descriptive_numbers: Vec<String>,
}

#[must_use]
pub fn word_parts(ident: &str) -> Vec<&str> {
    let mut out = Vec::new();
    for seg in ident.split('_').filter(|s| !s.is_empty()) {
        let b = seg.as_bytes();
        let mut start = 0usize;
        for i in 1..b.len() {
            let (Some(&p), Some(&c)) = (b.get(i - 1), b.get(i)) else {
                continue;
            };
            let next_lower = b.get(i + 1).is_some_and(u8::is_ascii_lowercase);
            let cut = (p.is_ascii_lowercase() && c.is_ascii_uppercase())
                || (p.is_ascii_uppercase() && c.is_ascii_uppercase() && next_lower)
                || (p.is_ascii_alphabetic() && c.is_ascii_digit())
                || (p.is_ascii_digit() && c.is_ascii_alphabetic());
            if cut {
                if let Some(w) = seg.get(start..i) {
                    out.push(w);
                }
                start = i;
            }
        }
        if let Some(w) = seg.get(start..) {
            out.push(w);
        }
    }
    out
}

fn hex(s: &str) -> bool {
    !s.is_empty() && s.bytes().all(|b| b.is_ascii_hexdigit())
}

fn digits(s: &str) -> bool {
    !s.is_empty() && s.bytes().all(|b| b.is_ascii_digit())
}

fn is_ida(ident: &str) -> bool {
    let core = ["g_", "s_", "m_"]
        .iter()
        .find_map(|p| ident.strip_prefix(p))
        .unwrap_or(ident);
    IDA_PREFIXES.iter().any(|p| {
        core.strip_prefix(p).is_some_and(|r| {
            (1..=8).contains(&r.len()) && hex(r) && r.bytes().any(|b| b.is_ascii_digit())
        })
    }) || core.strip_prefix("nullsub_").is_some_and(digits)
}

fn is_decompiler(ident: &str) -> bool {
    if is_ida(ident) {
        return true;
    }
    let hex_after = |p: &str| {
        ident
            .strip_prefix(p)
            .is_some_and(|r| r.len() >= 4 && hex(r))
    };
    if [
        "FUN_",
        "DAT_",
        "LAB_",
        "PTR_",
        "SUB_",
        "switchD_",
        "caseD_",
        "thunk_FUN_",
    ]
    .iter()
    .any(|p| hex_after(p))
    {
        return true;
    }
    if ident.starts_with("extraout_") || ident.starts_with("unaff_") {
        return true;
    }
    if ident
        .strip_prefix("in_")
        .is_some_and(|r| r.len() >= 2 && r.bytes().all(|b| b.is_ascii_uppercase()))
    {
        return true;
    }
    if ident.strip_prefix("param_").is_some_and(digits)
        || ident == "undefined"
        || ident.strip_prefix("undefined").is_some_and(digits)
        || ident.strip_prefix("local_").is_some_and(hex)
    {
        return true;
    }
    if let Some((head, tail)) = ident.split_once("Var") {
        return (1..=4).contains(&head.len())
            && head.bytes().all(|c| b"bcdefhilpsuw".contains(&c))
            && digits(tail);
    }
    false
}

fn offset_like(h: &str) -> bool {
    (2..=4).contains(&h.len())
        && hex(h)
        && h.bytes().any(|b| b.is_ascii_digit())
        && (h.bytes().any(|b| b.is_ascii_alphabetic()) || h.starts_with('0'))
}

fn trailing_hex(ident: &str) -> Option<(&str, &str)> {
    let b = ident.as_bytes();
    let mut i = b.len();
    while i > 0 && b.get(i - 1).is_some_and(u8::is_ascii_hexdigit) {
        i -= 1;
    }
    while b.get(i).is_some_and(u8::is_ascii_alphabetic) && i < b.len() {
        i += 1;
    }
    let run = ident.get(i..)?;
    ((2..=4).contains(&run.len()) && run.starts_with(|c: char| c.is_ascii_digit()))
        .then(|| (ident.get(..i).unwrap_or_default(), run))
}

fn followed_by_number(lower: &[String], words: &[&str]) -> bool {
    lower.windows(2).any(|w| {
        w.first().is_some_and(|a| words.contains(&a.as_str()))
            && w.get(1).is_some_and(|d| digits(d))
    })
}

impl Classifier {
    #[must_use]
    pub fn new(cfg: &Config) -> Self {
        Self {
            stems: STEMS
                .iter()
                .map(|s| (*s).to_string())
                .chain(cfg.stems.iter().cloned())
                .collect(),
            pad_suffixes: cfg.pad_suffixes.clone(),
            pad_prefixes: cfg.pad_prefixes.clone(),
            value_numbers: VALUE_NUMBERS
                .iter()
                .map(|s| (*s).to_string())
                .chain(cfg.value_numbers.iter().cloned())
                .collect(),
            descriptive_numbers: DESCRIPTIVE_NUMBERS
                .iter()
                .map(|s| (*s).to_string())
                .chain(cfg.descriptive_numbers.iter().cloned())
                .collect(),
        }
    }

    fn descriptive(&self, n: &str) -> bool {
        self.descriptive_numbers.iter().any(|d| d == n)
    }

    fn rva_suffix(&self, ident: &str) -> bool {
        if let Some((head, tail)) = ident.rsplit_once('_')
            && !head.is_empty()
            && (5..=8).contains(&tail.len())
            && hex(tail)
            && tail.bytes().any(|b| b.is_ascii_digit())
        {
            let caps = head
                .bytes()
                .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == b'_');
            return !(caps && digits(tail)) && !self.value_numbers.iter().any(|v| v == tail);
        }
        let b = ident.as_bytes();
        let mut i = b.len();
        while i > 0 && b.get(i - 1).is_some_and(u8::is_ascii_hexdigit) {
            i -= 1;
        }
        while b.get(i).is_some_and(u8::is_ascii_alphabetic) {
            i += 1;
        }
        let run = ident.get(i..).unwrap_or_default();
        let after_letter = ident.get(..i).is_some_and(|h| {
            h.ends_with(|c: char| c.is_ascii_alphabetic())
                && !h.ends_with("0x")
                && !h.ends_with("0X")
        });
        after_letter
            && (5..=8).contains(&run.len())
            && run.starts_with(|c: char| c.is_ascii_digit())
            && (run.len() >= 6 || run.bytes().any(|c| c.is_ascii_alphabetic()))
            && !self.value_numbers.iter().any(|v| v == run)
    }

    fn pad_prefix(&self, ident: &str) -> bool {
        self.pad_prefixes.iter().any(|p| {
            ident
                .strip_prefix(p.as_str())
                .is_some_and(|r| p.ends_with('_') || r.starts_with(|c: char| c.is_ascii_digit()))
        })
    }

    fn pad(&self, ident: &str) -> bool {
        self.pad_suffixes.iter().any(|s| {
            let s = s.as_str();
            ident.strip_suffix(s).is_some_and(|h| !h.is_empty())
                || ident
                    .get(..ident.len().saturating_sub(1))
                    .and_then(|h| h.strip_suffix(s))
                    .is_some_and(|h| {
                        !h.is_empty() && ident.ends_with(|c: char| c.is_ascii_uppercase())
                    })
                || ident
                    .rsplit_once(&format!("{s}_"))
                    .is_some_and(|(h, n)| !h.is_empty() && digits(n))
        })
    }

    #[must_use]
    pub fn classify(&self, ident: &str) -> Option<Shape> {
        if is_decompiler(ident) {
            return Some(Shape::Decompiler);
        }
        if self.pad_prefix(ident) || self.pad(ident) {
            return Some(Shape::Pad);
        }
        if self.rva_suffix(ident) || ident.starts_with("_Fn_") || ident.starts_with("Fn_") {
            return Some(Shape::RvaSuffix);
        }
        let parts = word_parts(ident);
        let lower: Vec<String> = parts.iter().map(|p| p.to_ascii_lowercase()).collect();
        let has = |w: &str| lower.iter().any(|p| p == w);
        let unk_hex = lower.iter().any(|p| {
            p.strip_prefix("unk")
                .is_some_and(|r| r.bytes().all(|b| b.is_ascii_hexdigit()))
        });
        if has("unknown") || unk_hex {
            return Some(Shape::Unknown);
        }
        if lower.iter().any(|p| p.starts_with("spare")) {
            return Some(Shape::Spare);
        }
        if let Some(i) = lower.iter().position(|p| p.starts_with("unused")) {
            let verb = i
                .checked_sub(1)
                .and_then(|j| lower.get(j))
                .is_some_and(|v| UNUSED_VERBS.contains(&v.as_str()));
            if !verb {
                return Some(Shape::Unused);
            }
        }
        let core = ["m_", "g_", "s_"]
            .iter()
            .find_map(|p| ident.strip_prefix(p))
            .unwrap_or(ident)
            .to_ascii_lowercase();
        if has("dummy")
            || core
                .strip_prefix("probe")
                .is_some_and(|r| r.bytes().all(|b| b.is_ascii_digit()))
        {
            return Some(Shape::Probe);
        }
        let ends_digits = lower.last().is_some_and(|p| digits(p));
        if lower.windows(2).any(|w| {
            w.first().is_some_and(|a| a == "slot")
                && w.get(1)
                    .is_some_and(|d| d.starts_with(|c: char| c.is_ascii_digit()))
        }) || (has("slot") && ends_digits)
            || followed_by_number(&lower, &["vtable", "vftable", "vtbl", "hook"])
        {
            return Some(Shape::Slot);
        }
        let two_hex =
            |r: &str| r.len() == 2 && hex(r) && r.starts_with(|c: char| c.is_ascii_digit());
        if ident.strip_prefix('v').is_some_and(two_hex)
            || ident.strip_prefix('w').is_some_and(two_hex)
        {
            return Some(Shape::Slot);
        }
        let f_offset = ident.strip_prefix('f').is_some_and(|r| {
            (2..=5).contains(&r.len()) && hex(r) && r.bytes().any(|b| b.is_ascii_digit())
        });
        let field_stem = ["m_field", "m_reserved", "m_unused", "m_pad", "pad", "field"]
            .iter()
            .any(|stem| {
                ident.strip_prefix(stem).is_some_and(|rest| {
                    let rest = rest
                        .trim_start_matches("0x")
                        .trim_start_matches('_')
                        .trim_start_matches("0x");
                    (1..=4).contains(&rest.len()) && hex(rest)
                })
            });
        if field_stem || f_offset {
            return Some(Shape::OffsetField);
        }
        let flag_word = |p: &String| matches!(p.as_str(), "bit" | "bits" | "flag" | "flags");
        for (i, _) in lower.iter().enumerate().filter(|(_, p)| flag_word(p)) {
            let nums: Vec<&String> = lower.iter().skip(i + 1).take_while(|p| digits(p)).collect();
            let width = matches!(nums.as_slice(), [one] if ["8", "16", "24", "32", "64"].contains(&one.as_str()));
            if !nums.is_empty() && !width {
                return Some(Shape::OffsetField);
            }
        }
        let hex_literal = ident.match_indices("0x").any(|(i, _)| {
            let before_ok = i == 0 || ident.as_bytes().get(i - 1) == Some(&b'_');
            let rest = ident.get(i + 2..).unwrap_or_default();
            before_ok && rest.len() >= 2 && hex(rest)
        });
        if hex_literal {
            return Some(Shape::OffsetField);
        }
        if let Some(i) = ident.rfind('F') {
            let tail = ident.get(i + 1..).unwrap_or_default();
            let before = ident.get(..i).unwrap_or_default();
            if (1..=4).contains(&tail.len())
                && tail.starts_with(|c: char| c.is_ascii_digit())
                && hex(tail)
                && before.ends_with(|c: char| c.is_ascii_lowercase())
                && !before.to_ascii_lowercase().ends_with("key")
            {
                return Some(Shape::OffsetField);
            }
        }
        let pad_word = |w: &str| matches!(w, "pad" | "field" | "reserved" | "gap");
        if let Some(i) = lower.iter().position(|p| pad_word(p)) {
            let next = lower.get(i + 1);
            let numbered = next.is_some_and(|n| n.starts_with(|c: char| c.is_ascii_digit()));
            let first = i == 0
                || (i == 1 && matches!(lower.first().map(String::as_str), Some("m" | "g" | "s")));
            if numbered || (first && (next.is_none() || ident.contains('_'))) {
                return Some(Shape::OffsetField);
            }
        }
        let dimension = |h: &str| h.eq_ignore_ascii_case("2d") || h.eq_ignore_ascii_case("3d");
        if let Some((_, last)) = ident.rsplit_once('_')
            && hex(last)
            && last.bytes().any(|c| c.is_ascii_lowercase())
            && (2..=4).contains(&last.len())
            && last.bytes().any(|c| c.is_ascii_digit())
            && !dimension(last)
        {
            return Some(Shape::OffsetField);
        }
        if let Some(rest) = ident.strip_prefix("m_") {
            if hex(rest) && rest.bytes().any(|b| b.is_ascii_digit()) && rest.len() <= 4 {
                return Some(Shape::OffsetField);
            }
            if let Some((_, h)) = trailing_hex(ident)
                && ((offset_like(h) && !dimension(h)) || (digits(h) && !self.descriptive(h)))
            {
                return Some(Shape::OffsetField);
            }
        }
        if trailing_hex(ident).is_some_and(|(_, h)| (3..=4).contains(&h.len()) && offset_like(h)) {
            return Some(Shape::OffsetField);
        }
        if let Some(n) = lower
            .last()
            .filter(|p| digits(p) && (3..=4).contains(&p.len()))
        {
            let head = ident.get(..ident.len() - n.len()).unwrap_or_default();
            if head.ends_with(|c: char| c.is_ascii_lowercase()) && !self.descriptive(n) {
                return Some(Shape::OffsetField);
            }
        }
        if lower.iter().any(|p| p.starts_with("reserve")) {
            return Some(Shape::Spare);
        }
        for stem in &self.stems {
            if let Some(rest) = ident.strip_prefix(stem.as_str())
                && (digits(rest) || (rest.is_empty() && stem.ends_with('_')))
            {
                return Some(Shape::IndexedStem);
            }
        }
        let not_x = lower.windows(2).any(|w| {
            w.first().is_some_and(|a| a == "not")
                && w.get(1).is_some_and(|b| {
                    matches!(
                        b.as_str(),
                        "implemented" | "available" | "avail" | "written" | "supported"
                    )
                })
        });
        if has("stub") || has("unimplemented") || not_x {
            return Some(Shape::IndexedStem);
        }
        let indexed_noop = ident.split('_').collect::<Vec<_>>().windows(2).any(|w| {
            let (Some(a), Some(b)) = (w.first(), w.get(1)) else {
                return false;
            };
            let l = a.to_ascii_lowercase();
            matches!(
                l.trim_end_matches(|c: char| c.is_ascii_digit()),
                "noop" | "nop"
            ) && ((b.len() == 1 && b.bytes().all(|c| c.is_ascii_alphabetic())) || digits(b))
        }) || ident.split('_').any(|s| {
            let l = s.to_ascii_lowercase();
            let base = l.trim_end_matches(|c: char| c.is_ascii_digit());
            matches!(base, "noop" | "nop") && base.len() < l.len()
        });
        if followed_by_number(&lower, &["variant", "command", "cmd"]) || indexed_noop {
            return Some(Shape::IndexedStem);
        }
        if let Some((head, last)) = ident.rsplit_once('_') {
            let caps = !head.is_empty()
                && head
                    .bytes()
                    .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == b'_');
            let two = last.len() == 2
                && last.starts_with(|c: char| c.is_ascii_digit())
                && last
                    .bytes()
                    .all(|c| c.is_ascii_digit() || (b'A'..=b'F').contains(&c));
            if caps
                && two
                && (last.starts_with('0') || last.bytes().any(|c| c.is_ascii_alphabetic()))
                && !dimension(last)
            {
                return Some(Shape::IndexedStem);
            }
        }
        let matrix = ident.len() == 3
            && ident.starts_with('m')
            && ident
                .get(1..)
                .is_some_and(|r| r.bytes().all(|c| (b'0'..=b'3').contains(&c)));
        let letter_index = ident.len() >= 2
            && ident.starts_with(|c: char| c.is_ascii_lowercase())
            && ident.get(1..).is_some_and(digits);
        let word_index = ["arg", "param", "parm", "var", "tmp", "temp", "val", "loc"]
            .iter()
            .any(|w| ident.strip_prefix(w).is_some_and(digits));
        if (letter_index && !matrix)
            || word_index
            || followed_by_number(&lower, &["arg", "param", "parm"])
        {
            return Some(Shape::Positional);
        }
        None
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum DeclKind {
    Macro,
    Tag,
    Enumerator,
    Typedef,
    Function,
    Global,
    Member,
    Param,
    Local,
    Label,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Decl {
    pub kind: DeclKind,
    pub name: String,
    pub owner: String,
    pub line: usize,
}

fn declarator_names(toks: &[&Token]) -> Vec<(String, usize)> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut cur: Vec<&Token> = Vec::new();
    let flush = |cur: &mut Vec<&Token>, out: &mut Vec<(String, usize)>| {
        let stop = cur
            .iter()
            .position(|t| t.is_punct("=") || t.is_punct("[") || t.is_punct(":"))
            .unwrap_or(cur.len());
        let head = cur.get(..stop).unwrap_or_default();
        if let Some(t) = head.iter().rev().find(|t| t.kind == Kind::Ident)
            && head.len() >= 2
        {
            out.push((t.text.clone(), t.line));
        }
        cur.clear();
    };
    for t in toks {
        if t.is_punct("(") || t.is_punct("[") || t.is_punct("{") {
            depth += 1;
        } else if t.is_punct(")") || t.is_punct("]") || t.is_punct("}") {
            depth -= 1;
        }
        if t.is_punct(",") && depth == 0 {
            flush(&mut cur, &mut out);
            continue;
        }
        cur.push(t);
    }
    flush(&mut cur, &mut out);
    out
}

#[must_use]
pub fn declarations(lx: &Lexed) -> Vec<Decl> {
    let mut out = Vec::new();
    for d in lx
        .directives
        .iter()
        .filter(|d| d.name == "define" && d.cond != Cond::Dead)
    {
        if let Some(t) = lx.args(d).first().filter(|t| t.kind == Kind::Ident) {
            out.push(Decl {
                kind: DeclKind::Macro,
                name: t.text.clone(),
                owner: "-".to_string(),
                line: t.line,
            });
        }
    }
    let ds: Vec<Def> = defs::definitions(lx);
    for d in &ds {
        out.push(Decl {
            kind: DeclKind::Function,
            name: d.base.clone(),
            owner: if d.in_class || d.name.contains("::") {
                d.qualified()
                    .rsplit_once("::")
                    .map_or("-", |x| x.0)
                    .to_string()
            } else {
                "-".to_string()
            },
            line: lx.tokens.get(d.name_tok).map_or(d.first_line, |t| t.line),
        });
        if let Ok(fl) = flow::analyse(lx, d, &flow::Opts::default()) {
            for dc in &fl.decls.declarators {
                out.push(Decl {
                    kind: if dc.param {
                        DeclKind::Param
                    } else {
                        DeclKind::Local
                    },
                    name: dc.name.clone(),
                    owner: d.qualified(),
                    line: lx.tokens.get(dc.tok).map_or(0, |t| t.line),
                });
            }
        }
        let body = flow::tokens(lx, d.open, d.close);
        for (k, t) in body.iter().enumerate() {
            let next = body.get(k + 1);
            let prev = k.checked_sub(1).and_then(|p| body.get(p));
            let at_start = prev.is_some_and(|p| {
                p.kind == Kind::Punct && matches!(p.text.as_str(), ";" | "{" | "}" | ":")
            }) || prev.is_some_and(|p| p.asm && p.line < t.line);
            if t.kind == Kind::Ident
                && next.is_some_and(|n| n.kind == Kind::Punct && n.text == ":")
                && !body
                    .get(k + 2)
                    .is_some_and(|n| n.kind == Kind::Punct && n.text == ":")
                && at_start
                && !matches!(
                    t.text.as_str(),
                    "default" | "case" | "public" | "private" | "protected"
                )
                && !prev.is_some_and(|p| p.text == "?")
            {
                out.push(Decl {
                    kind: DeclKind::Label,
                    name: t.text.clone(),
                    owner: d.qualified(),
                    line: t.line,
                });
            }
        }
    }
    let inside = |i: usize| ds.iter().any(|d| d.head <= i && i <= d.close);
    let idx: Vec<usize> = lx.live_code().into_iter().filter(|i| !inside(*i)).collect();
    let toks: Vec<&Token> = idx.iter().filter_map(|&i| lx.tokens.get(i)).collect();
    let mut scopes: Vec<(String, bool)> = Vec::new();
    let mut stmt: Vec<&Token> = Vec::new();
    let mut k = 0usize;
    let mut after_type = false;
    while let Some(t) = toks.get(k) {
        if t.is_punct("{") {
            let first = stmt.first();
            let is_enum = stmt.iter().any(|x| x.is("enum"));
            let class_kw = stmt
                .iter()
                .position(|x| x.is("class") || x.is("struct") || x.is("union") || x.is("enum"));
            let owner = match class_kw {
                Some(p) => {
                    let name = stmt
                        .iter()
                        .skip(p + 1)
                        .take_while(|x| !x.is_punct(":"))
                        .filter(|x| x.kind == Kind::Ident && !x.text.starts_with("__"))
                        .last()
                        .map(|x| (x.text.clone(), x.line));
                    if let Some((n, l)) = &name {
                        out.push(Decl {
                            kind: DeclKind::Tag,
                            name: n.clone(),
                            owner: scopes.last().map_or("-".to_string(), |s| s.0.clone()),
                            line: *l,
                        });
                    }
                    name.map(|x| x.0).unwrap_or_default()
                }
                None => String::new(),
            };
            let is_scope = class_kw.is_some()
                || first.is_some_and(|f| f.is("namespace"))
                || (first.is_some_and(|f| f.is("extern")) && stmt.len() == 2);
            if is_enum {
                let close = {
                    let mut depth = 0i32;
                    let mut c = k;
                    for (j, x) in toks.iter().enumerate().skip(k) {
                        if x.is_punct("{") {
                            depth += 1;
                        } else if x.is_punct("}") {
                            depth -= 1;
                            if depth == 0 {
                                c = j;
                                break;
                            }
                        }
                    }
                    c
                };
                let items: Vec<&Token> = toks.get(k + 1..close).unwrap_or_default().to_vec();
                let mut expect = true;
                let mut depth = 0i32;
                for x in &items {
                    if x.is_punct("(") {
                        depth += 1;
                    } else if x.is_punct(")") {
                        depth -= 1;
                    }
                    if expect && x.kind == Kind::Ident {
                        out.push(Decl {
                            kind: DeclKind::Enumerator,
                            name: x.text.clone(),
                            owner: if owner.is_empty() {
                                "-".to_string()
                            } else {
                                owner.clone()
                            },
                            line: x.line,
                        });
                        expect = false;
                    }
                    if x.is_punct(",") && depth == 0 {
                        expect = true;
                    }
                }
                k = close + 1;
                stmt.clear();
                after_type = false;
                continue;
            }
            if is_scope {
                let class = class_kw.is_some();
                let name = if class {
                    owner
                } else {
                    scopes.last().map_or(String::new(), |s| s.0.clone())
                };
                scopes.push((name, class));
                stmt.clear();
                k += 1;
                continue;
            }
            let mut depth = 0i32;
            for (j, x) in toks.iter().enumerate().skip(k) {
                if x.is_punct("{") {
                    depth += 1;
                } else if x.is_punct("}") {
                    depth -= 1;
                    if depth == 0 {
                        k = j;
                        break;
                    }
                }
            }
            stmt.push(t);
            k += 1;
            continue;
        }
        if t.is_punct("}") {
            let popped = scopes.pop();
            after_type = popped.is_some_and(|s| s.1);
            stmt.clear();
            k += 1;
            continue;
        }
        if t.is_punct(":")
            && stmt.len() == 1
            && stmt
                .first()
                .is_some_and(|f| matches!(f.text.as_str(), "public" | "private" | "protected"))
        {
            stmt.clear();
            k += 1;
            continue;
        }
        if t.is_punct(";") {
            let in_class = scopes.last().is_some_and(|s| s.1);
            let owner = if in_class {
                scopes.last().map_or("-".to_string(), |s| s.0.clone())
            } else {
                "-".to_string()
            };
            if std::mem::take(&mut after_type) {
                for piece in stmt.split(|x| x.is_punct(",")) {
                    let stop = piece
                        .iter()
                        .position(|x| x.is_punct("=") || x.is_punct("["))
                        .unwrap_or(piece.len());
                    if let Some(n) = piece
                        .get(..stop)
                        .unwrap_or_default()
                        .iter()
                        .rev()
                        .find(|x| x.kind == Kind::Ident)
                    {
                        out.push(Decl {
                            kind: if in_class {
                                DeclKind::Member
                            } else {
                                DeclKind::Global
                            },
                            name: n.text.clone(),
                            owner: owner.clone(),
                            line: n.line,
                        });
                    }
                }
            } else {
                classify_statement(&stmt, in_class, &owner, &mut out);
            }
            stmt.clear();
            k += 1;
            continue;
        }
        stmt.push(t);
        k += 1;
    }
    out
}

fn classify_statement(stmt: &[&Token], in_class: bool, owner: &str, out: &mut Vec<Decl>) {
    let Some(first) = stmt.first() else {
        return;
    };
    if matches!(
        first.text.as_str(),
        "friend" | "using" | "template" | "namespace"
    ) || first.kind != Kind::Ident
    {
        return;
    }
    if stmt
        .iter()
        .any(|t| t.is("class") || t.is("struct") || t.is("union") || t.is("enum"))
        && stmt.len() <= 3
        && !stmt.iter().any(|t| t.is_punct("("))
    {
        if let Some(n) = stmt.iter().rev().find(|t| t.kind == Kind::Ident) {
            out.push(Decl {
                kind: DeclKind::Tag,
                name: n.text.clone(),
                owner: owner.to_string(),
                line: n.line,
            });
        }
        return;
    }
    let typedef = first.is("typedef");
    let paren = stmt.iter().position(|t| t.is_punct("("));
    if !typedef
        && let Some(p) = paren
        && p > 0
        && stmt
            .get(p - 1)
            .is_some_and(|t| t.kind == Kind::Ident && t.text != "__declspec")
        && !stmt
            .get(..p)
            .unwrap_or_default()
            .iter()
            .any(|t| t.is_punct("="))
    {
        if let Some(name) = stmt.get(p - 1) {
            out.push(Decl {
                kind: DeclKind::Function,
                name: name.text.clone(),
                owner: owner.to_string(),
                line: name.line,
            });
        }
        let mut depth = 0i32;
        let mut close = stmt.len();
        for (j, t) in stmt.iter().enumerate().skip(p) {
            if t.is_punct("(") {
                depth += 1;
            } else if t.is_punct(")") {
                depth -= 1;
                if depth == 0 {
                    close = j;
                    break;
                }
            }
        }
        let inner: Vec<&Token> = stmt.get(p + 1..close).unwrap_or_default().to_vec();
        for (n, l) in declarator_names(&inner) {
            if n != "void" {
                out.push(Decl {
                    kind: DeclKind::Param,
                    name: n,
                    owner: format!(
                        "{}{}",
                        if owner == "-" {
                            String::new()
                        } else {
                            format!("{owner}::")
                        },
                        stmt.get(p - 1).map_or("", |t| t.text.as_str())
                    ),
                    line: l,
                });
            }
        }
        return;
    }
    if typedef {
        if let Some(fp) = stmt.windows(3).find(|w| matches!(w, [a, b, c] if a.is_punct("(") && b.is_punct("*") && c.kind == Kind::Ident)) {
            if let Some(n) = fp.get(2) {
                out.push(Decl {
                    kind: DeclKind::Typedef,
                    name: n.text.clone(),
                    owner: owner.to_string(),
                    line: n.line,
                });
            }
            return;
        }
        for (n, l) in declarator_names(stmt.get(1..).unwrap_or_default()) {
            out.push(Decl {
                kind: DeclKind::Typedef,
                name: n,
                owner: owner.to_string(),
                line: l,
            });
        }
        return;
    }
    for (n, l) in declarator_names(stmt) {
        out.push(Decl {
            kind: if in_class {
                DeclKind::Member
            } else {
                DeclKind::Global
            },
            name: n,
            owner: owner.to_string(),
            line: l,
        });
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub path: String,
    pub line: usize,
    pub kind: DeclKind,
    pub owner: String,
    pub name: String,
    pub shape: Shape,
}

#[must_use]
pub fn census(path: &str, lx: &Lexed, c: &Classifier, allow: &[String]) -> Vec<Row> {
    let mut seen: BTreeMap<(DeclKind, String, String), ()> = BTreeMap::new();
    let mut rows = Vec::new();
    for d in declarations(lx) {
        if allow
            .iter()
            .any(|a| a == &d.name || crate::glob::matches(a, &d.name))
        {
            continue;
        }
        let Some(shape) = c.classify(&d.name) else {
            continue;
        };
        if seen
            .insert((d.kind, d.owner.clone(), d.name.clone()), ())
            .is_some()
        {
            continue;
        }
        rows.push(Row {
            path: path.to_string(),
            line: d.line,
            kind: d.kind,
            owner: d.owner,
            name: d.name,
            shape,
        });
    }
    rows
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lex::lex;

    fn c() -> Classifier {
        Classifier::new(&Config {
            pad_suffixes: vec!["Filler".into(), "Pad".into()],
            pad_prefixes: vec!["xx_".into(), "xx".into()],
            stems: vec!["Piece_".into()],
            ..Config::default()
        })
    }

    #[test]
    fn shapes_are_read_off_word_parts() {
        let c = c();
        let s = |n: &str| c.classify(n);
        assert_eq!(s("FUN_00401000"), Some(Shape::Decompiler));
        assert_eq!(s("iVar27"), Some(Shape::Decompiler));
        assert_eq!(s("dword_4a5b6c"), Some(Shape::Decompiler));
        assert_eq!(s("local_1c"), Some(Shape::Decompiler));
        assert_eq!(s("xx_net12"), Some(Shape::Pad));
        assert_eq!(s("xx12"), Some(Shape::Pad));
        assert_eq!(s("xxRay"), None);
        assert_eq!(s("PanelFiller"), Some(Shape::Pad));
        assert_eq!(s("FormPadA"), Some(Shape::Pad));
        assert_eq!(s("GetCount_00066A80"), Some(Shape::RvaSuffix));
        assert_eq!(s("g_data4a5b90"), Some(Shape::RvaSuffix));
        assert_eq!(s("CBR_115200"), None);
        assert_eq!(s("m_unk1f4"), Some(Shape::Unknown));
        assert_eq!(s("g_scoreSpare"), Some(Shape::Spare));
        assert_eq!(s("g_unusedIcon"), Some(Shape::Unused));
        assert_eq!(s("PurgeUnusedEntries"), None);
        assert_eq!(s("probe3"), Some(Shape::Probe));
        assert_eq!(s("slot12"), Some(Shape::Slot));
        assert_eq!(s("v0c"), Some(Shape::Slot));
        assert_eq!(s("m_field08"), Some(Shape::OffsetField));
        assert_eq!(s("pad13c"), Some(Shape::OffsetField));
        assert_eq!(s("m_block6c"), Some(Shape::OffsetField));
        assert_eq!(s("m_table565"), None);
        assert_eq!(s("Piece_90"), Some(Shape::IndexedStem));
        assert_eq!(s("ERR_0D"), Some(Shape::IndexedStem));
        assert_eq!(s("GL_TEXTURE_2D"), None);
        assert_eq!(s("a0"), Some(Shape::Positional));
        assert_eq!(s("arg4"), Some(Shape::Positional));
        assert_eq!(s("m00"), None);
        assert_eq!(s("GetTransparentColor"), None);
        assert_eq!(s("TrunkLine1"), None);
        assert_eq!(word_parts("HTMLParser"), ["HTML", "Parser"]);
        assert_eq!(word_parts("m_unk1f4"), ["m", "unk", "1", "f", "4"]);
    }

    #[test]
    fn the_census_finds_declarations_by_kind_and_owner() {
        let src = b"#define xx_pad1 1\nstruct Thing {\n    int m_field08;\n    int good;\n    void SetSlot3(int a, int arg1);\n};\nenum { ERR_0D = 13, OK };\nint g_unk04;\ntypedef int (*Fn_00401a20)(int);\nint f(int param_1)\n{\n    int iVar1 = param_1;\nLAB_00401020:\n    return iVar1;\n}\n";
        let lx = lex(src);
        let rows = census("t.cpp", &lx, &c(), &[]);
        let found: Vec<(DeclKind, &str, &str)> = rows
            .iter()
            .map(|r| (r.kind, r.owner.as_str(), r.name.as_str()))
            .collect();
        assert!(
            found.contains(&(DeclKind::Macro, "-", "xx_pad1")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Member, "Thing", "m_field08")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Function, "Thing", "SetSlot3")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Param, "Thing::SetSlot3", "arg1")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Enumerator, "-", "ERR_0D")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Global, "-", "g_unk04")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Typedef, "-", "Fn_00401a20")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Param, "f", "param_1")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Local, "f", "iVar1")),
            "{found:?}"
        );
        assert!(
            found.contains(&(DeclKind::Label, "f", "LAB_00401020")),
            "{found:?}"
        );
        assert!(!found.iter().any(|f| f.2 == "good" || f.2 == "OK"));
        let rows = census("t.cpp", &lx, &c(), &["iVar*".to_string()]);
        assert!(!rows.iter().any(|r| r.name == "iVar1"));
    }
}
