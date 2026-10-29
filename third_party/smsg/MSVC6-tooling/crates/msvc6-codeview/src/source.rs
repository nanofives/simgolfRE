use crate::record::Access;
use crate::ty::Key;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Loc {
    pub file: PathBuf,
    pub line: usize,
}

impl std::fmt::Display for Loc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.file.display(), self.line)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tok {
    pub text: String,
    pub line: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Dcl {
    Name(Option<String>),
    Ptr {
        inner: Box<Dcl>,
        reference: bool,
        konst: bool,
        volat: bool,
    },
    Array {
        inner: Box<Dcl>,
        extent: Vec<String>,
    },
    Func {
        inner: Box<Dcl>,
        params: Vec<Param>,
        varargs: bool,
        konst: bool,
        cc: Option<String>,
    },
}

impl Dcl {
    pub fn name(&self) -> Option<&str> {
        match self {
            Self::Name(n) => n.as_deref(),
            Self::Ptr { inner, .. } | Self::Array { inner, .. } | Self::Func { inner, .. } => {
                inner.name()
            }
        }
    }

    pub fn as_function(&self) -> Option<(&[Param], bool, bool, Option<&str>)> {
        match self {
            Self::Func {
                inner,
                params,
                varargs,
                konst,
                cc,
            } if matches!(**inner, Self::Name(_)) => {
                Some((params, *varargs, *konst, cc.as_deref()))
            }
            Self::Ptr { inner, .. } => inner.as_function(),
            _ => None,
        }
    }

    pub fn return_part(&self) -> Option<Dcl> {
        match self {
            Self::Func { inner, .. } if matches!(**inner, Self::Name(_)) => Some(Self::Name(None)),
            Self::Ptr {
                inner,
                reference,
                konst,
                volat,
            } => inner.return_part().map(|i| Self::Ptr {
                inner: Box::new(i),
                reference: *reference,
                konst: *konst,
                volat: *volat,
            }),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeclTy {
    pub spec: Vec<String>,
    pub dcl: Dcl,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Param {
    pub ty: DeclTy,
    pub has_default: bool,
}

impl Param {
    pub fn name(&self) -> Option<&str> {
        self.ty.dcl.name()
    }
}

#[derive(Debug, Clone)]
pub enum SKind {
    Field { ty: DeclTy, bits: Option<String> },
    Static { ty: DeclTy },
    Method(Box<SMethod>),
    Nested,
    Friend,
}

#[derive(Debug, Clone)]
pub struct SMethod {
    pub ret: Option<DeclTy>,
    pub params: Vec<Param>,
    pub varargs: bool,
    pub konst: bool,
    pub virt: bool,
    pub stat: bool,
    pub pure: bool,
    pub body: Option<Loc>,
}

#[derive(Debug, Clone)]
pub struct SMember {
    pub name: String,
    pub access: Access,
    pub kind: SKind,
    pub loc: Loc,
    pub end: usize,
    pub anon: Option<usize>,
}

#[derive(Debug, Clone)]
pub struct SClass {
    pub name: String,
    pub key: Key,
    pub bases: Vec<(Option<Access>, String, bool)>,
    pub members: Vec<SMember>,
    pub loc: Loc,
    pub template: bool,
}

#[derive(Debug, Clone)]
pub struct SEnum {
    pub name: String,
    pub values: Vec<(String, Option<String>)>,
    pub loc: Loc,
}

#[derive(Debug, Clone)]
pub struct SDef {
    pub name: String,
    pub params: Vec<Param>,
    pub loc: Loc,
    pub end: usize,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct ParseStats {
    pub tokens: usize,
    pub max_nest: usize,
    pub rework: usize,
}

#[derive(Debug, Clone, Default)]
pub struct Src {
    pub classes: Vec<SClass>,
    pub enums: Vec<SEnum>,
    pub typedefs: BTreeMap<String, (DeclTy, Loc)>,
    pub defs: Vec<SDef>,
    pub macros: BTreeMap<String, String>,
    pub skipped: Vec<(Loc, String)>,
    pub files: std::collections::BTreeSet<PathBuf>,
    pub stats: ParseStats,
}

impl Src {
    pub fn class(&self, name: &str) -> Option<&SClass> {
        self.classes.iter().find(|c| c.name == name)
    }
}

pub fn strip_comments(src: &[u8]) -> String {
    let lexed = msvc6_source::lex::lex(src);
    let mut spans: Vec<(usize, usize)> = lexed.comments.iter().map(|c| (c.start, c.end)).collect();
    spans.sort_unstable();
    let in_comment = |at: usize| {
        let k = spans.partition_point(|s| s.0 <= at);
        k.checked_sub(1)
            .and_then(|k| spans.get(k))
            .is_some_and(|s| at < s.1)
    };
    let mut out = Vec::with_capacity(src.len());
    let mut line = 1usize;
    for (b, start, physical) in msvc6_source::lex::logical(src) {
        if b == b'\n' {
            out.push(b'\n');
            line += 1;
            while line <= physical {
                out.push(b'\n');
                line += 1;
            }
        } else if in_comment(start) {
            out.push(b' ');
        } else {
            out.push(b);
        }
    }
    msvc6_base::srcio::decode(&out)
}

pub fn preprocess(text: &str, defs: &mut BTreeMap<String, String>) -> String {
    preprocess_with(text, defs, &mut |_, _| {})
}

pub fn preprocess_with(
    text: &str,
    defs: &mut BTreeMap<String, String>,
    include: &mut dyn FnMut(&str, &mut BTreeMap<String, String>),
) -> String {
    let lines: Vec<&str> = text.split('\n').collect();
    let mut out: Vec<String> = Vec::with_capacity(lines.len());
    let mut stack: Vec<(bool, bool, bool)> = Vec::new();
    let mut i = 0;
    while i < lines.len() {
        let mut line = lines.get(i).copied().unwrap_or_default().to_string();
        let mut n = 1;
        if line.trim_start().starts_with('#') {
            while line.trim_end().ends_with('\\') && i + n < lines.len() {
                let keep = line.trim_end().len().saturating_sub(1);
                line.truncate(keep);
                line.push(' ');
                line.push_str(lines.get(i + n).copied().unwrap_or_default());
                n += 1;
            }
            let d = line
                .trim_start()
                .strip_prefix('#')
                .unwrap_or_default()
                .trim_start()
                .to_string();
            let word: String = d
                .chars()
                .take_while(|c| c.is_alphanumeric() || *c == '_')
                .collect();
            let rest = d.get(word.len()..).unwrap_or_default().trim();
            let parent = stack.last().is_none_or(|f| f.0);
            match word.as_str() {
                "if" => {
                    let v = parent && eval_if(rest, defs) != 0;
                    stack.push((v, v, parent));
                }
                "ifdef" => {
                    let v = parent && defs.contains_key(first_word(rest));
                    stack.push((v, v, parent));
                }
                "ifndef" => {
                    let v = parent && !defs.contains_key(first_word(rest));
                    stack.push((v, v, parent));
                }
                "elif" => {
                    if let Some(f) = stack.last_mut() {
                        let v = f.2 && !f.1 && eval_if(rest, defs) != 0;
                        f.0 = v;
                        f.1 |= v;
                    }
                }
                "else" => {
                    if let Some(f) = stack.last_mut() {
                        f.0 = f.2 && !f.1;
                        f.1 = true;
                    }
                }
                "endif" => {
                    stack.pop();
                }
                "define" if parent => {
                    let name = first_word(rest);
                    let body = rest.strip_prefix(name).unwrap_or_default();
                    if !body.starts_with('(') {
                        defs.insert(name.to_string(), body.trim().to_string());
                    }
                }
                "undef" if parent => {
                    defs.remove(first_word(rest));
                }
                "include" if parent => {
                    if let Some(name) = rest.strip_prefix('"').and_then(|r| r.split('"').next()) {
                        include(name, defs);
                    }
                }
                _ => {}
            }
            for _ in 0..n {
                out.push(String::new());
            }
            i += n;
            continue;
        }
        out.push(if stack.last().is_none_or(|f| f.0) {
            line
        } else {
            String::new()
        });
        i += 1;
    }
    out.join("\n")
}

fn first_word(s: &str) -> &str {
    s.split(|c: char| !(c.is_alphanumeric() || c == '_'))
        .find(|w| !w.is_empty())
        .unwrap_or("")
}

pub fn eval_if(expr: &str, defs: &BTreeMap<String, String>) -> i64 {
    eval_if_stats(expr, defs).0
}

pub fn eval_if_stats(expr: &str, defs: &BTreeMap<String, String>) -> (i64, usize) {
    let toks = lex(expr, 1);
    let mut p = Expr {
        t: &toks,
        i: 0,
        defs,
        depth: 0,
        nest: 0,
        max_nest: 0,
    };
    let v = p.or();
    (v, p.max_nest)
}

pub fn eval_const(toks: &[String], names: &dyn Fn(&str) -> Option<i64>) -> Option<i64> {
    let text = toks.join(" ");
    let lexed = lex(&text, 1);
    let mut known = BTreeMap::new();
    for t in &lexed {
        if is_ident(&t.text) && t.text != "sizeof" {
            known.insert(t.text.clone(), names(&t.text)?.to_string());
        }
    }
    Some(eval_if(&text, &known))
}

pub const EXPR_NEST: usize = 256;
pub const NEST: usize = 64;
pub const REWORK: usize = 8;
pub const REWORK_FLOOR: usize = 1 << 16;
pub const MACRO_DEPTH: usize = 8;

struct Expr<'a> {
    t: &'a [Tok],
    i: usize,
    defs: &'a BTreeMap<String, String>,
    depth: usize,
    nest: usize,
    max_nest: usize,
}

impl Expr<'_> {
    fn peek(&self) -> &str {
        self.t.get(self.i).map_or("", |t| t.text.as_str())
    }

    fn eat(&mut self, s: &str) -> bool {
        if self.peek() == s {
            self.i += 1;
            true
        } else {
            false
        }
    }

    fn or(&mut self) -> i64 {
        let mut v = self.and();
        while self.eat("||") {
            let r = self.and();
            v = i64::from(v != 0 || r != 0);
        }
        v
    }

    fn and(&mut self) -> i64 {
        let mut v = self.bin(0);
        while self.eat("&&") {
            let r = self.bin(0);
            v = i64::from(v != 0 && r != 0);
        }
        v
    }

    fn bin(&mut self, level: usize) -> i64 {
        const LEVELS: &[&[&str]] = &[
            &["|"],
            &["^"],
            &["&"],
            &["==", "!="],
            &["<", ">", "<=", ">="],
            &["<<", ">>"],
            &["+", "-"],
            &["*", "/", "%"],
        ];
        let Some(ops) = LEVELS.get(level) else {
            return self.unary();
        };
        let mut v = self.bin(level + 1);
        loop {
            let op = self.peek().to_string();
            if !ops.contains(&op.as_str()) {
                return v;
            }
            self.i += 1;
            let r = self.bin(level + 1);
            v = match op.as_str() {
                "|" => v | r,
                "^" => v ^ r,
                "&" => v & r,
                "==" => i64::from(v == r),
                "!=" => i64::from(v != r),
                "<" => i64::from(v < r),
                ">" => i64::from(v > r),
                "<=" => i64::from(v <= r),
                ">=" => i64::from(v >= r),
                "<<" => v.wrapping_shl(u32::try_from(r).unwrap_or(0)),
                ">>" => v.wrapping_shr(u32::try_from(r).unwrap_or(0)),
                "+" => v.wrapping_add(r),
                "-" => v.wrapping_sub(r),
                "*" => v.wrapping_mul(r),
                "/" => v.checked_div(r).unwrap_or(0),
                _ => v.checked_rem(r).unwrap_or(0),
            };
        }
    }

    fn unary(&mut self) -> i64 {
        if self.nest > EXPR_NEST {
            self.i = self.t.len();
            return 0;
        }
        self.nest += 1;
        self.max_nest = self.max_nest.max(self.nest);
        let v = self.unary_inner();
        self.nest -= 1;
        v
    }

    fn unary_inner(&mut self) -> i64 {
        if self.eat("!") {
            return i64::from(self.unary() == 0);
        }
        if self.eat("~") {
            return !self.unary();
        }
        if self.eat("-") {
            return self.unary().wrapping_neg();
        }
        if self.eat("+") {
            return self.unary();
        }
        if self.eat("(") {
            let v = self.or();
            self.eat(")");
            return v;
        }
        let w = self.peek().to_string();
        self.i += 1;
        if w == "defined" {
            let paren = self.eat("(");
            let n = self.peek().to_string();
            self.i += 1;
            if paren {
                self.eat(")");
            }
            return i64::from(self.defs.contains_key(&n));
        }
        if let Some(v) = int_lit(&w) {
            return v;
        }
        if self.depth < MACRO_DEPTH
            && let Some(body) = self.defs.get(&w)
        {
            let toks = lex(body, 1);
            let mut sub = Expr {
                t: &toks,
                i: 0,
                defs: self.defs,
                depth: self.depth + 1,
                nest: self.nest,
                max_nest: self.max_nest,
            };
            let v = sub.or();
            self.max_nest = self.max_nest.max(sub.max_nest);
            return v;
        }
        0
    }
}

pub fn int_lit(w: &str) -> Option<i64> {
    let t = w.trim_end_matches(['u', 'U', 'l', 'L']);
    if let Some(h) = t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) {
        return i64::from_str_radix(h, 16).ok();
    }
    if t.len() > 1 && t.starts_with('0') && t.chars().all(|c| c.is_ascii_digit()) {
        return i64::from_str_radix(t.get(1..).unwrap_or_default(), 8).ok();
    }
    if !t.is_empty() && t.chars().all(|c| c.is_ascii_digit()) {
        return t.parse().ok();
    }
    if t.chars().count() == 3 && t.starts_with('\'') && t.ends_with('\'') {
        return t.chars().nth(1).map(|c| i64::from(u32::from(c)));
    }
    None
}

pub fn lex(text: &str, first: usize) -> Vec<Tok> {
    const PUNCT3: &[&str] = &["...", "<<=", ">>=", "->*"];
    const PUNCT2: &[&str] = &[
        "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=",
        "/=", "%=", "&=", "|=", "^=", ".*", "##",
    ];
    let b: Vec<char> = text.chars().collect();
    let at = |k: usize| b.get(k).copied();
    let mut out = Vec::new();
    let mut line = first;
    let mut i = 0;
    while let Some(c) = at(i) {
        if c == '\n' {
            line += 1;
            i += 1;
            continue;
        }
        if c.is_whitespace() {
            i += 1;
            continue;
        }
        let start = i;
        if c.is_alphabetic() || c == '_' {
            while at(i).is_some_and(|x| x.is_alphanumeric() || x == '_') {
                i += 1;
            }
        } else if c.is_ascii_digit() || (c == '.' && at(i + 1).is_some_and(|x| x.is_ascii_digit()))
        {
            while at(i).is_some_and(|x| x.is_alphanumeric() || x == '.' || x == '_') {
                i += 1;
            }
        } else if c == '"' || c == '\'' {
            i += 1;
            while at(i).is_some_and(|x| x != c && x != '\n') {
                if at(i) == Some('\\') {
                    i += 1;
                }
                i += 1;
            }
            i = (i + 1).min(b.len());
        } else {
            let rest: String = b
                .get(i..(i + 3).min(b.len()))
                .unwrap_or_default()
                .iter()
                .collect();
            let n = if PUNCT3.iter().any(|p| rest.starts_with(p)) {
                3
            } else if PUNCT2.iter().any(|p| rest.starts_with(p)) {
                2
            } else {
                1
            };
            i += n;
        }
        let text: String = b
            .get(start..i.min(b.len()))
            .unwrap_or_default()
            .iter()
            .collect();
        let nl = text.matches('\n').count();
        out.push(Tok { text, line });
        line += nl;
    }
    out
}

const STORAGE: &[&str] = &[
    "static",
    "virtual",
    "inline",
    "__inline",
    "__forceinline",
    "extern",
    "mutable",
    "explicit",
    "register",
    "auto",
    "friend",
    "typedef",
    "typename",
];
const CC: &[(&str, &str)] = &[
    ("__cdecl", "__cdecl"),
    ("_cdecl", "__cdecl"),
    ("cdecl", "__cdecl"),
    ("__stdcall", "__stdcall"),
    ("_stdcall", "__stdcall"),
    ("WINAPI", "__stdcall"),
    ("CALLBACK", "__stdcall"),
    ("APIENTRY", "__stdcall"),
    ("PASCAL", "__stdcall"),
    ("__fastcall", "__fastcall"),
    ("__thiscall", "__thiscall"),
];
const FUNDAMENTAL: &[&str] = &[
    "void", "bool", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
    "wchar_t", "__int8", "__int16", "__int32", "__int64",
];

fn cc_of(w: &str) -> Option<&'static str> {
    CC.iter().find(|(k, _)| *k == w).map(|(_, v)| *v)
}

pub fn is_ident(w: &str) -> bool {
    w.chars()
        .next()
        .is_some_and(|c| c.is_alphabetic() || c == '_')
}

struct P<'a> {
    t: &'a [Tok],
    i: usize,
    file: &'a Path,
    out: &'a mut Src,
    anon_types: usize,
    nest: usize,
    rework: usize,
    max_nest: usize,
}

#[derive(Default, Debug)]
struct Spec {
    words: Vec<String>,
    stat: bool,
    virt: bool,
    inline_kw: bool,
    typedef: bool,
    cc: Option<String>,
    defined: Option<String>,
}

impl P<'_> {
    fn peek(&self) -> &str {
        self.t.get(self.i).map_or("", |t| t.text.as_str())
    }
    fn over_rework(&self) -> bool {
        self.rework > self.t.len().saturating_mul(REWORK).max(REWORK_FLOOR)
    }

    fn deeper(&mut self) {
        self.nest += 1;
        self.max_nest = self.max_nest.max(self.nest);
    }

    fn peek_at(&self, k: usize) -> &str {
        self.t.get(self.i + k).map_or("", |t| t.text.as_str())
    }
    fn line(&self) -> usize {
        self.t
            .get(self.i)
            .or_else(|| self.t.last())
            .map_or(0, |t| t.line)
    }
    fn loc(&self) -> Loc {
        Loc {
            file: self.file.to_path_buf(),
            line: self.line(),
        }
    }
    fn prev_line(&self) -> usize {
        self.i
            .checked_sub(1)
            .and_then(|k| self.t.get(k))
            .map_or(0, |t| t.line)
    }

    fn done(&self) -> bool {
        self.i >= self.t.len()
    }
    fn eat(&mut self, s: &str) -> bool {
        if self.peek() == s {
            self.i += 1;
            true
        } else {
            false
        }
    }

    fn skip_group(&mut self) {
        let open = self.peek().to_string();
        let close = match open.as_str() {
            "(" => ")",
            "[" => "]",
            "{" => "}",
            "<" => ">",
            _ => return,
        };
        let mut depth = 0usize;
        while !self.done() {
            let w = self.peek();
            if w == open {
                depth += 1;
            } else if w == close {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    self.i += 1;
                    return;
                }
            } else if open == "<" && (w == ";" || w == "{") {
                return;
            }
            self.i += 1;
        }
    }

    fn skip_statement(&mut self) {
        let start = self.loc();
        let head: Vec<String> = self
            .t
            .get(self.i..(self.i + 6).min(self.t.len()))
            .unwrap_or_default()
            .iter()
            .map(|t| t.text.clone())
            .collect();
        self.skip_member();
        self.out.skipped.push((start, head.join(" ")));
    }

    fn skip_member(&mut self) {
        while !self.done() {
            match self.peek() {
                ";" => {
                    self.i += 1;
                    return;
                }
                "(" | "[" => self.skip_group(),
                "{" => {
                    self.skip_group();
                    self.eat(";");
                    return;
                }
                "}" => return,
                _ => self.i += 1,
            }
        }
    }

    fn scope(&mut self) {
        while !self.done() {
            if self.peek() == "}" {
                self.i += 1;
                continue;
            }
            let before = self.i;
            self.top_decl();
            if self.i == before {
                self.i += 1;
            }
        }
    }

    fn top_decl(&mut self) {
        match self.peek() {
            ";" => {
                self.i += 1;
            }
            "extern" if self.peek_at(1).starts_with('"') => {
                self.i += 2;
                if self.eat("{") {
                    while !self.done() && self.peek() != "}" {
                        let before = self.i;
                        self.top_decl();
                        if self.i == before {
                            self.i += 1;
                        }
                    }
                    self.eat("}");
                } else {
                    self.top_decl();
                }
            }
            "namespace" => {
                self.i += 1;
                if is_ident(self.peek()) {
                    self.i += 1;
                }
                if self.eat("{") {
                    while !self.done() && self.peek() != "}" {
                        let before = self.i;
                        self.top_decl();
                        if self.i == before {
                            self.i += 1;
                        }
                    }
                    self.eat("}");
                }
            }
            "template" => {
                self.i += 1;
                self.skip_group();
                let start = self.i;
                self.declaration(true);
                if self.i == start {
                    self.skip_statement();
                }
            }
            _ => {
                let start = self.i;
                self.declaration(false);
                if self.i == start {
                    self.skip_statement();
                }
            }
        }
    }

    fn specifiers(&mut self, class_name: Option<&str>, template: bool) -> Spec {
        let mut s = Spec::default();
        let mut have_type = false;
        loop {
            let w = self.peek().to_string();
            if w.is_empty() {
                return s;
            }
            if STORAGE.contains(&w.as_str()) {
                match w.as_str() {
                    "static" => s.stat = true,
                    "virtual" => s.virt = true,
                    "inline" | "__inline" | "__forceinline" => s.inline_kw = true,
                    "typedef" => s.typedef = true,
                    _ => {}
                }
                self.i += 1;
                continue;
            }
            if w == "__declspec" {
                self.i += 1;
                self.skip_group();
                continue;
            }
            if let Some(c) = cc_of(&w) {
                s.cc = Some(c.to_string());
                self.i += 1;
                continue;
            }
            if w == "const" || w == "volatile" {
                s.words.push(w);
                self.i += 1;
                continue;
            }
            if FUNDAMENTAL.contains(&w.as_str()) {
                s.words.push(w);
                have_type = true;
                self.i += 1;
                continue;
            }
            if matches!(w.as_str(), "class" | "struct" | "union" | "enum") {
                if have_type {
                    return s;
                }
                let key = w.clone();
                self.i += 1;
                let mut name = String::new();
                if is_ident(self.peek()) && cc_of(self.peek()).is_none() {
                    name = self.qualified();
                }
                if self.peek() == "{" || (self.peek() == ":" && key != "enum") {
                    let defined = self.class_or_enum(&key, &name, class_name, template);
                    s.words.push(key);
                    s.words.push(defined.clone());
                    s.defined = Some(defined);
                    have_type = true;
                    continue;
                }
                s.words.push(key);
                s.words.push(name);
                have_type = true;
                continue;
            }
            if w == "~" || w == "operator" {
                return s;
            }
            if is_ident(&w) {
                if have_type {
                    return s;
                }
                let save = self.i;
                let q = self.qualified();
                let next = self.peek().to_string();
                let is_ctor = class_name.is_some_and(|c| leaf(c) == q) && next == "(";
                if is_ctor
                    || (q.contains("::") && next == "(")
                    || q.ends_with("::")
                    || q.ends_with("::operator")
                {
                    self.i = save;
                    return s;
                }
                if next == "::" && self.peek_at(1) == "operator" {
                    self.i = save;
                    return s;
                }
                s.words.push(q);
                have_type = true;
                continue;
            }
            return s;
        }
    }

    fn qualified(&mut self) -> String {
        let mut q = String::new();
        loop {
            if !is_ident(self.peek()) || self.peek() == "operator" {
                break;
            }
            q.push_str(self.peek());
            self.i += 1;
            if self.peek() == "<" {
                let s = self.i;
                self.skip_group();
                let inner: Vec<&str> = self
                    .t
                    .get(s..self.i)
                    .unwrap_or_default()
                    .iter()
                    .map(|t| t.text.as_str())
                    .collect();
                q.push_str(&inner.join(" ").replace(" ,", ","));
                q = q.replace("< ", "<").replace(" >", ">");
            }
            if self.peek() == "::" {
                if self.peek_at(1) == "~" || self.peek_at(1) == "operator" {
                    q.push_str("::");
                    self.i += 1;
                    break;
                }
                q.push_str("::");
                self.i += 1;
                continue;
            }
            break;
        }
        q
    }

    fn declarator(&mut self, abstract_ok: bool) -> Option<Dcl> {
        self.declarator_cc(abstract_ok).map(|(d, _)| d)
    }

    fn declarator_cc(&mut self, abstract_ok: bool) -> Option<(Dcl, Option<String>)> {
        let mut ops: Vec<(bool, bool, bool)> = Vec::new();
        let mut pending_cc: Option<String> = None;
        loop {
            let w = self.peek().to_string();
            if w == "*" || w == "&" {
                self.i += 1;
                let (mut k, mut v) = (false, false);
                loop {
                    match self.peek() {
                        "const" => k = true,
                        "volatile" => v = true,
                        _ => break,
                    }
                    self.i += 1;
                }
                ops.push((w == "&", k, v));
                continue;
            }
            if let Some(c) = cc_of(&w) {
                pending_cc = Some(c.to_string());
                self.i += 1;
                continue;
            }
            if w == "__declspec" {
                self.i += 1;
                self.skip_group();
                continue;
            }
            break;
        }
        let (mut d, mut suffix_cc) = if self.peek() == "(" && self.nested_declarator_follows() {
            self.i += 1;
            if self.nest > NEST {
                return None;
            }
            self.deeper();
            let nested = self.declarator_cc(abstract_ok);
            self.nest -= 1;
            let (inner, dangling) = nested?;
            if !self.eat(")") {
                return None;
            }
            (inner, dangling)
        } else {
            let name = self.declarator_name();
            if name.is_none() && !abstract_ok {
                return None;
            }
            (Dcl::Name(name), pending_cc.take())
        };
        loop {
            if self.peek() == "[" {
                let s = self.i + 1;
                self.skip_group();
                let ext: Vec<String> = self
                    .t
                    .get(s..self.i.saturating_sub(1).max(s))
                    .unwrap_or_default()
                    .iter()
                    .map(|t| t.text.clone())
                    .collect();
                d = Dcl::Array {
                    inner: Box::new(d),
                    extent: ext,
                };
                continue;
            }
            if self.peek() == "(" {
                let (params, varargs) = self.params()?;
                let mut konst = false;
                loop {
                    match self.peek() {
                        "const" => {
                            konst = true;
                            self.i += 1;
                        }
                        "volatile" => self.i += 1,
                        "throw" => {
                            self.i += 1;
                            self.skip_group();
                        }
                        _ => break,
                    }
                }
                d = Dcl::Func {
                    inner: Box::new(d),
                    params,
                    varargs,
                    konst,
                    cc: suffix_cc.take(),
                };
                continue;
            }
            break;
        }
        for (r, k, v) in ops.into_iter().rev() {
            d = Dcl::Ptr {
                inner: Box::new(d),
                reference: r,
                konst: k,
                volat: v,
            };
        }
        Some((d, suffix_cc.or(pending_cc)))
    }

    fn nested_declarator_follows(&self) -> bool {
        let n = self.peek_at(1);
        n == "*" || n == "&" || cc_of(n).is_some() || n == "__declspec"
    }

    fn declarator_name(&mut self) -> Option<String> {
        let mut q = String::new();
        loop {
            if self.peek() == "~" {
                self.i += 1;
                q.push('~');
                if is_ident(self.peek()) {
                    q.push_str(self.peek());
                    self.i += 1;
                }
                return Some(q);
            }
            if self.peek() == "operator" {
                self.i += 1;
                q.push_str("operator");
                let w = self.peek().to_string();
                if w == "(" && self.peek_at(1) == ")" {
                    q.push_str("()");
                    self.i += 2;
                } else if w == "[" && self.peek_at(1) == "]" {
                    q.push_str("[]");
                    self.i += 2;
                } else if w == "new" || w == "delete" {
                    q.push(' ');
                    q.push_str(&w);
                    self.i += 1;
                    if self.peek() == "[" && self.peek_at(1) == "]" {
                        q.push_str("[]");
                        self.i += 2;
                    }
                } else if is_ident(&w) {
                    let mut parts = Vec::new();
                    while self.peek() != "(" && !self.done() {
                        parts.push(self.peek().to_string());
                        self.i += 1;
                    }
                    q.push(' ');
                    q.push_str(&parts.join(" "));
                } else {
                    q.push_str(&w);
                    self.i += 1;
                }
                return Some(q);
            }
            let w = self.peek();
            if !is_ident(w) || cc_of(w).is_some() || matches!(w, "const" | "volatile") {
                return (!q.is_empty()).then_some(q);
            }
            q.push_str(w);
            self.i += 1;
            if self.peek() == "<" && self.peek_at(2) == ">" {
                self.skip_group();
            }
            if self.peek() == "::" {
                q.push_str("::");
                self.i += 1;
                continue;
            }
            return Some(q);
        }
    }

    fn params(&mut self) -> Option<(Vec<Param>, bool)> {
        if self.nest > NEST {
            return None;
        }
        self.deeper();
        let p = self.params_inner();
        self.nest -= 1;
        p
    }

    fn params_inner(&mut self) -> Option<(Vec<Param>, bool)> {
        if !self.eat("(") {
            return None;
        }
        let mut out = Vec::new();
        let mut varargs = false;
        if self.peek() == "void" && self.peek_at(1) == ")" {
            self.i += 2;
            return Some((out, false));
        }
        while !self.done() && self.peek() != ")" {
            if self.eat("...") {
                varargs = true;
                continue;
            }
            let start = self.i;
            let spec = self.specifiers(None, false);
            let dcl = self.declarator(true)?;
            if self.i == start {
                return None;
            }
            let mut has_default = false;
            if self.eat("=") {
                has_default = true;
                while !self.done() && self.peek() != "," && self.peek() != ")" {
                    if matches!(self.peek(), "(" | "[" | "{") {
                        self.skip_group();
                    } else {
                        self.i += 1;
                    }
                }
            }
            out.push(Param {
                ty: DeclTy {
                    spec: spec.words,
                    dcl,
                },
                has_default,
            });
            if !self.eat(",") {
                break;
            }
        }
        if !self.eat(")") {
            return None;
        }
        Some((out, varargs))
    }

    fn class_or_enum(
        &mut self,
        key: &str,
        name: &str,
        outer: Option<&str>,
        template: bool,
    ) -> String {
        let loc = self.loc();
        let leafname = if name.is_empty() {
            self.anon_types += 1;
            format!("__anon{}_{}", self.anon_types, loc.line)
        } else {
            name.to_string()
        };
        let full = match outer {
            Some(o) => format!("{o}::{leafname}"),
            _ => leafname,
        };
        if self.nest > NEST {
            while !self.done() && self.peek() != "{" && self.peek() != ";" {
                self.i += 1;
            }
            self.skip_group();
            return full;
        }
        self.deeper();
        let full = self.class_or_enum_body(key, full, loc, template);
        self.nest -= 1;
        full
    }

    fn class_or_enum_body(&mut self, key: &str, full: String, loc: Loc, template: bool) -> String {
        if key == "enum" {
            let mut values = Vec::new();
            if self.eat("{") {
                while !self.done() && self.peek() != "}" {
                    let n = self.peek().to_string();
                    self.i += 1;
                    let mut v = None;
                    if self.eat("=") {
                        let mut e = Vec::new();
                        let mut depth = 0i32;
                        while !self.done()
                            && !(depth == 0 && (self.peek() == "," || self.peek() == "}"))
                        {
                            match self.peek() {
                                "(" => depth += 1,
                                ")" => depth -= 1,
                                _ => {}
                            }
                            e.push(self.peek().to_string());
                            self.i += 1;
                        }
                        v = Some(e.join(" "));
                    }
                    values.push((n, v));
                    self.eat(",");
                }
                self.eat("}");
            }
            self.out.enums.push(SEnum {
                name: full.clone(),
                values,
                loc,
            });
            return full;
        }
        let mut bases = Vec::new();
        if self.eat(":") {
            loop {
                let mut acc = None;
                let mut virt = false;
                loop {
                    match self.peek() {
                        "public" => acc = Some(Access::Public),
                        "protected" => acc = Some(Access::Protected),
                        "private" => acc = Some(Access::Private),
                        "virtual" => virt = true,
                        _ => break,
                    }
                    self.i += 1;
                }
                let b = self.qualified();
                bases.push((acc, b, virt));
                if !self.eat(",") {
                    break;
                }
            }
        }
        let k = match key {
            "class" => Key::Class,
            "struct" => Key::Struct,
            _ => Key::Union,
        };
        let idx = self.out.classes.len();
        self.out.classes.push(SClass {
            name: full.clone(),
            key: k,
            bases,
            members: Vec::new(),
            loc,
            template,
        });
        if self.eat("{") {
            let mut acc = if k == Key::Class {
                Access::Private
            } else {
                Access::Public
            };
            let mut anon = 0usize;
            let members = self.body(&full, &mut acc, &mut anon, None);
            if let Some(c) = self.out.classes.get_mut(idx) {
                c.members = members;
            }
            self.eat("}");
        }
        full
    }

    fn body(
        &mut self,
        class: &str,
        acc: &mut Access,
        anon: &mut usize,
        anon_of: Option<usize>,
    ) -> Vec<SMember> {
        let mut out = Vec::new();
        while !self.done() && self.peek() != "}" {
            let w = self.peek().to_string();
            if matches!(w.as_str(), "public" | "protected" | "private") && self.peek_at(1) == ":" {
                *acc = match w.as_str() {
                    "public" => Access::Public,
                    "protected" => Access::Protected,
                    _ => Access::Private,
                };
                self.i += 2;
                continue;
            }
            if w == ";" {
                self.i += 1;
                continue;
            }
            if (w == "union" || w == "struct") && self.peek_at(1) == "{" && !self.over_rework() {
                let save = self.i;
                self.i += 2;
                *anon += 1;
                let id = *anon;
                let mut inner_acc = *acc;
                self.deeper();
                let ms = if self.nest > NEST {
                    self.i = self.i.saturating_sub(1);
                    self.skip_group();
                    self.i = self.i.saturating_sub(1);
                    Vec::new()
                } else {
                    self.body(class, &mut inner_acc, anon, Some(id))
                };
                self.nest -= 1;
                self.eat("}");
                if self.eat(";") {
                    for mut m in ms {
                        m.anon = Some(m.anon.unwrap_or(id));
                        if anon_of.is_some() {
                            m.anon = anon_of;
                        }
                        out.push(m);
                    }
                    continue;
                }
                self.rework = self.rework.saturating_add(self.i.saturating_sub(save));
                self.i = save;
                if self.over_rework() {
                    self.skip_statement();
                    continue;
                }
            }
            if w == "friend" {
                let s = self.i;
                self.skip_member();
                out.push(SMember {
                    name: String::new(),
                    access: *acc,
                    kind: SKind::Friend,
                    loc: Loc {
                        file: self.file.to_path_buf(),
                        line: self.t.get(s).map_or(0, |t| t.line),
                    },
                    end: self.prev_line(),
                    anon: anon_of,
                });
                continue;
            }
            if w == "template" {
                self.i += 1;
                self.skip_group();
                self.skip_member();
                continue;
            }
            let start = self.i;
            let mut ms = self.member(class, *acc);
            if self.i == start {
                self.skip_statement();
                continue;
            }
            for m in &mut ms {
                m.anon = anon_of;
            }
            out.extend(ms);
        }
        out
    }

    fn member(&mut self, class: &str, acc: Access) -> Vec<SMember> {
        let loc0 = self.loc();
        let spec = self.specifiers(Some(class), false);
        let mut out = Vec::new();
        if let Some(d) = &spec.defined {
            out.push(SMember {
                name: leaf(d).to_string(),
                access: acc,
                kind: SKind::Nested,
                loc: loc0.clone(),
                end: self.prev_line(),
                anon: None,
            });
        }
        if self.eat(";") {
            return out;
        }
        if spec.typedef {
            while let Some(d) = self.declarator(false) {
                if let Some(n) = d.name() {
                    self.out.typedefs.insert(
                        format!("{class}::{n}"),
                        (
                            DeclTy {
                                spec: spec.words.clone(),
                                dcl: d.clone(),
                            },
                            loc0.clone(),
                        ),
                    );
                    out.push(SMember {
                        name: n.to_string(),
                        access: acc,
                        kind: SKind::Nested,
                        loc: loc0.clone(),
                        end: self.prev_line(),
                        anon: None,
                    });
                }
                if !self.eat(",") {
                    break;
                }
            }
            self.skip_member();
            return out;
        }
        loop {
            let loc = self.loc();
            let Some(d) = self.declarator(false) else {
                self.skip_member();
                return out;
            };
            let name = d.name().unwrap_or_default().to_string();
            if let Some((params, varargs, konst, _cc)) = d.as_function() {
                let leafname = leaf(class);
                let ctor = name == leafname || name == format!("~{leafname}");
                let ret = (!ctor).then(|| DeclTy {
                    spec: spec.words.clone(),
                    dcl: d.return_part().unwrap_or(Dcl::Name(None)),
                });
                let mut m = SMethod {
                    ret,
                    params: params.to_vec(),
                    varargs,
                    konst,
                    virt: spec.virt,
                    stat: spec.stat,
                    pure: false,
                    body: None,
                };
                if self.eat("=") {
                    m.pure = self.eat("0");
                }
                if self.peek() == ":" {
                    while !self.done() && self.peek() != "{" && self.peek() != ";" {
                        if self.peek() == "(" {
                            self.skip_group();
                        } else {
                            self.i += 1;
                        }
                    }
                }
                if self.peek() == "{" {
                    m.body = Some(self.loc());
                    self.skip_group();
                    self.eat(";");
                    out.push(SMember {
                        name,
                        access: acc,
                        kind: SKind::Method(Box::new(m)),
                        loc,
                        end: self.prev_line(),
                        anon: None,
                    });
                    return out;
                }
                out.push(SMember {
                    name,
                    access: acc,
                    kind: SKind::Method(Box::new(m)),
                    loc,
                    end: self.prev_line(),
                    anon: None,
                });
            } else {
                let mut bits = None;
                if self.eat(":") {
                    let mut e = Vec::new();
                    while !self.done() && self.peek() != ";" && self.peek() != "," {
                        e.push(self.peek().to_string());
                        self.i += 1;
                    }
                    bits = Some(e.join(" "));
                }
                if self.eat("=") {
                    while !self.done() && self.peek() != ";" && self.peek() != "," {
                        self.i += 1;
                    }
                }
                let ty = DeclTy {
                    spec: spec.words.clone(),
                    dcl: d,
                };
                out.push(SMember {
                    name,
                    access: acc,
                    kind: if spec.stat {
                        SKind::Static { ty }
                    } else {
                        SKind::Field { ty, bits }
                    },
                    loc,
                    end: self.prev_line(),
                    anon: None,
                });
            }
            if !self.eat(",") {
                break;
            }
        }
        self.skip_member();
        out
    }

    fn declaration(&mut self, template: bool) {
        let loc0 = self.loc();
        let spec = self.specifiers(None, template);
        if self.eat(";") {
            return;
        }
        if spec.typedef {
            let mut first = true;
            let anon = spec
                .defined
                .clone()
                .filter(|a| leaf(a).starts_with("__anon"));
            let mut renamed_to: Option<String> = None;
            while let Some(d) = self.declarator(false) {
                if let Some(n) = d.name() {
                    if let (true, Dcl::Name(_), Some(a)) = (first, &d, anon.as_deref()) {
                        self.rename_anon(a, n);
                        renamed_to = Some(n.to_string());
                    }
                    let mut words = spec.words.clone();
                    if let (Some(a), Some(r)) = (anon.as_deref(), renamed_to.as_deref()) {
                        for w in &mut words {
                            if w == a {
                                *w = r.to_string();
                            }
                        }
                    }
                    self.out.typedefs.insert(
                        n.to_string(),
                        (
                            DeclTy {
                                spec: words,
                                dcl: d.clone(),
                            },
                            loc0.clone(),
                        ),
                    );
                }
                first = false;
                if !self.eat(",") {
                    break;
                }
            }
            self.skip_member();
            return;
        }
        loop {
            let loc = self.loc();
            let Some(d) = self.declarator(false) else {
                self.skip_statement();
                return;
            };
            let name = d.name().unwrap_or_default().to_string();
            if let Some((params, _, _, _)) = d.as_function() {
                let cls = name.rsplit_once("::").map(|(c, _)| c.to_string());
                if self.peek() == ":" {
                    while !self.done() && self.peek() != "{" && self.peek() != ";" {
                        if self.peek() == "(" {
                            self.skip_group();
                        } else {
                            self.i += 1;
                        }
                    }
                }
                if self.peek() == "{" {
                    self.skip_group();
                    if cls.is_some() && !template {
                        self.out.defs.push(SDef {
                            name: name.clone(),
                            params: params.to_vec(),
                            loc,
                            end: self.prev_line(),
                        });
                    }

                    return;
                }
            }
            if self.eat("=") {
                while !self.done() && self.peek() != ";" && self.peek() != "," {
                    if matches!(self.peek(), "(" | "[" | "{") {
                        self.skip_group();
                    } else {
                        self.i += 1;
                    }
                }
            }
            if !self.eat(",") {
                break;
            }
        }
        if !self.eat(";") {
            self.skip_statement();
        }
    }

    fn rename_anon(&mut self, anon: &str, to: &str) {
        for c in &mut self.out.classes {
            if c.name == anon {
                c.name = to.to_string();
            } else if let Some(rest) = c.name.strip_prefix(&format!("{anon}::")) {
                c.name = format!("{to}::{rest}");
            }
        }
        for e in &mut self.out.enums {
            if e.name == anon {
                e.name = to.to_string();
            }
        }
    }
}

pub fn leaf(q: &str) -> &str {
    q.rsplit("::").next().unwrap_or(q)
}
pub fn parse_bytes(src: &[u8], shown: &Path, defs: &mut BTreeMap<String, String>, out: &mut Src) {
    let pre = preprocess(&strip_comments(src), defs);
    parse_preprocessed(&pre, shown, out);
    for (k, v) in defs.iter() {
        out.macros.insert(k.clone(), v.clone());
    }
}

pub fn parse_preprocessed(pre: &str, shown: &Path, out: &mut Src) {
    let toks = lex(pre, 1);
    let mut p = P {
        t: &toks,
        i: 0,
        file: shown,
        out,
        anon_types: 0,
        nest: 0,
        rework: 0,
        max_nest: 0,
    };
    p.scope();
    let (max_nest, rework) = (p.max_nest, p.rework);
    out.stats.tokens = out.stats.tokens.saturating_add(toks.len());
    out.stats.max_nest = out.stats.max_nest.max(max_nest);
    out.stats.rework = out.stats.rework.saturating_add(rework);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(text: &str, defs: &[&str]) -> Src {
        let mut d: BTreeMap<String, String> = defs
            .iter()
            .map(|k| ((*k).to_string(), String::new()))
            .collect();
        let mut s = Src::default();
        parse_bytes(text.as_bytes(), Path::new("x.h"), &mut d, &mut s);
        s
    }

    #[test]
    fn a_splice_or_a_trigraph_in_a_declaration_reads_as_cl_reads_it() {
        let text = "class S {\npublic:\n    int m_a\\\nb;\n    char m_t??(4??);\n    int m_c; /* x\\\n y */\n    int m_d;\n};\n";
        let s = parse(text, &[]);
        let c = s.class("S").unwrap();
        let names: Vec<&str> = c.members.iter().map(|m| m.name.as_str()).collect();
        assert_eq!(names, ["m_ab", "m_t", "m_c", "m_d"], "{:#?}", c.members);
        let at = |n: &str| c.members.iter().find(|m| m.name == n).unwrap().loc.line;
        assert_eq!(
            (at("m_ab"), at("m_t"), at("m_c"), at("m_d")),
            (3, 5, 6, 8),
            "every line after a splice keeps its physical number"
        );
    }

    #[test]
    fn a_class_reads_member_for_member() {
        let text = "\
/* c */ class Dev : public Root {
public:
    Dev();
    virtual ~Dev();
    virtual SNDERR init(HWND game_hwnd, unsigned long flags) = 0;
    bool is_on() const { return m_flags.active; }
    static SNDERR __cdecl create_device(Dev **ppdev, unsigned long flags);
    Dev &operator=(const Dev &);
#ifdef _USRDLL
    void dll_only();
#else
    void game_only();
#endif
private:
    struct Flags_tag { int active : 1; int attached : 1; } m_flags;
    unsigned long m_volume, *m_pvol;
    char m_fname[0x104];
    void (__stdcall *m_pcallback)(unsigned int, unsigned long);
    union {
        Dev *m_pdll_device;
        void *m_raw;
    };
    static Dev *m_head;
    friend class Mgr;
};
inline Dev::Dev() : m_volume(0) {}
SNDERR Dev::init(HWND hwnd, unsigned long f) { return SNDERR_OK; }
Dev::~Dev() {}
char *Dev::name() const { return 0; }
typedef struct { int a; } PLAIN, *PPLAIN;
enum SNDERR { SNDERR_OK = 0, SNDERR_FAIL };
";
        let s = parse(text, &["_USRDLL"]);
        let c = s.class("Dev").unwrap();
        let names: Vec<&str> = c.members.iter().map(|m| m.name.as_str()).collect();
        assert_eq!(
            names,
            [
                "Dev",
                "~Dev",
                "init",
                "is_on",
                "create_device",
                "operator=",
                "dll_only",
                "Flags_tag",
                "m_flags",
                "m_volume",
                "m_pvol",
                "m_fname",
                "m_pcallback",
                "m_pdll_device",
                "m_raw",
                "m_head",
                ""
            ],
            "{:#?}",
            c.members
        );
        let m = |n: &str| c.members.iter().find(|m| m.name == n).unwrap();
        let SKind::Method(init) = &m("init").kind else {
            panic!()
        };
        assert!(init.virt && init.pure && init.params.len() == 2);
        assert_eq!(init.params[0].name(), Some("game_hwnd"));
        let SKind::Method(on) = &m("is_on").kind else {
            panic!()
        };
        assert!(on.konst && on.body.is_some());
        let SKind::Method(cd) = &m("create_device").kind else {
            panic!()
        };
        assert!(cd.stat);
        assert_eq!(m("m_volume").access, Access::Private);
        assert!(
            matches!(&m("m_pvol").kind, SKind::Field { ty, .. } if matches!(ty.dcl, Dcl::Ptr { .. }))
        );
        assert!(
            matches!(&m("m_fname").kind, SKind::Field { ty, .. } if matches!(&ty.dcl, Dcl::Array { extent, .. } if extent == &["0x104"]))
        );
        assert!(
            matches!(&m("m_pcallback").kind, SKind::Field { ty, .. } if matches!(&ty.dcl, Dcl::Func { inner, cc: Some(c), .. } if c == "__stdcall" && matches!(**inner, Dcl::Ptr { .. })))
        );
        assert!(m("m_pdll_device").anon.is_some() && m("m_pdll_device").anon == m("m_raw").anon);
        assert!(matches!(m("m_head").kind, SKind::Static { .. }));
        let fl = s.class("Dev::Flags_tag").unwrap();
        assert!(matches!(&fl.members[0].kind, SKind::Field { bits: Some(b), .. } if b == "1"));
        assert_eq!(c.bases, [(Some(Access::Public), "Root".to_string(), false)]);
        let defs: Vec<&str> = s.defs.iter().map(|d| d.name.as_str()).collect();
        assert_eq!(defs, ["Dev::Dev", "Dev::init", "Dev::~Dev", "Dev::name"]);
        assert_eq!(s.defs[1].params[0].name(), Some("hwnd"));
        assert!(s.typedefs.contains_key("PLAIN") && s.typedefs.contains_key("PPLAIN"));
        assert!(s.class("PLAIN").is_some(), "{:?}", s.classes);
        assert_eq!(s.enums.last().unwrap().values.len(), 2);
        assert!(s.skipped.is_empty(), "{:?}", s.skipped);
        let g = parse(text, &[]);
        assert!(
            g.class("Dev")
                .unwrap()
                .members
                .iter()
                .any(|m| m.name == "game_only")
        );
    }

    #[test]
    fn if_expressions_evaluate() {
        let mut d = BTreeMap::new();
        d.insert("A".to_string(), "2".to_string());
        assert_eq!(eval_if("defined(A) && A > 1", &d), 1);
        assert_eq!(eval_if("!defined B || (A == 3)", &d), 1);
        assert_eq!(eval_if("B", &d), 0);
        let names = |n: &str| (n == "N").then_some(16);
        assert_eq!(
            eval_const(&["N".into(), "*".into(), "2".into()], &names),
            Some(32)
        );
        assert_eq!(eval_const(&["M".into()], &names), None);
    }
}
