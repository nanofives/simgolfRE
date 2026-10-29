use crate::budget::Budget;
use anyhow::{Result, bail};
use std::collections::{BTreeMap, BTreeSet};

pub const PROBE: &str = "c1probe_";

pub const STEPS_PER_TOKEN: u64 = 512;

pub const STEP_FLOOR: u64 = 1 << 20;

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Count {
    pub total: i64,
    pub by_kind: BTreeMap<&'static str, i64>,
    pub includes: Vec<String>,
    pub unmodelled: Vec<String>,
    pub marks: Vec<i64>,
    pub tokens: usize,
    pub steps: u64,
    pub depth: u32,
}

impl Count {
    fn add(&mut self, kind: &'static str, n: i64) {
        if n != 0 {
            self.total = self.total.saturating_add(n);
            let e = self.by_kind.entry(kind).or_insert(0);
            *e = e.saturating_add(n);
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Tok {
    Id(String),
    Num,
    Str(String),
    Chr,
    P(&'static str),
}

const PUNCT: &[&str] = &[
    "...", "<<=", ">>=", "->*", "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&",
    "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ".*", "{", "}", "(", ")", "[", "]", ";",
    ",", ":", "?", "=", "<", ">", "+", "-", "*", "/", "%", "&", "|", "^", "!", "~", ".", "#",
];

fn span(t: &[Tok], a: usize, b: usize) -> &[Tok] {
    t.get(a..b.min(t.len())).unwrap_or_default()
}

fn from(t: &[Tok], a: usize) -> &[Tok] {
    t.get(a..).unwrap_or_default()
}

fn is_p(t: &[Tok], i: usize, p: &str) -> bool {
    matches!(t.get(i), Some(Tok::P(q)) if *q == p)
}

fn is_id(t: Option<&Tok>) -> Option<&str> {
    match t {
        Some(Tok::Id(s)) => Some(s.as_str()),
        _ => None,
    }
}

#[must_use]
pub fn strip_comments(s: &str) -> String {
    let s = s.replace("\\\r\n", "").replace("\\\n", "");
    let b: Vec<char> = s.chars().collect();
    let ch = |k: usize| b.get(k).copied();
    let mut out = String::with_capacity(b.len());
    let mut i = 0;
    while let Some(c) = ch(i) {
        if c == '/' && ch(i + 1) == Some('/') {
            while ch(i).is_some_and(|x| x != '\n') {
                i += 1;
            }
        } else if c == '/' && ch(i + 1) == Some('*') {
            i += 2;
            while let Some(x) = ch(i) {
                if x == '*' && ch(i + 1) == Some('/') {
                    break;
                }
                if x == '\n' {
                    out.push('\n');
                }
                i += 1;
            }
            i += 2;
            out.push(' ');
        } else if c == '"' || c == '\'' {
            out.push(c);
            i += 1;
            while let Some(x) = ch(i) {
                if x == c || x == '\n' {
                    break;
                }
                if x == '\\' && i + 1 < b.len() {
                    out.push(x);
                    i += 1;
                }
                if let Some(y) = ch(i) {
                    out.push(y);
                }
                i += 1;
            }
            if let Some(x) = ch(i) {
                out.push(x);
                i += 1;
            }
        } else {
            out.push(c);
            i += 1;
        }
    }
    out
}

fn condition(rest: &str, defined: &BTreeSet<String>) -> bool {
    let r = rest.replace(' ', "");
    if r == "0" {
        return false;
    }
    if let Some(x) = r
        .strip_prefix("!defined(")
        .and_then(|x| x.strip_suffix(')'))
    {
        return !defined.contains(x);
    }
    if let Some(x) = r.strip_prefix("defined(").and_then(|x| x.strip_suffix(')')) {
        return defined.contains(x);
    }
    true
}

fn preprocess(s: &str, c: &mut Count) -> String {
    let mut defined: BTreeSet<String> = ["__cplusplus", "_MSC_VER", "_WIN32", "_M_IX86", "WIN32"]
        .iter()
        .map(|s| (*s).to_string())
        .collect();
    let mut stack: Vec<(bool, bool, bool)> = Vec::new();
    let live = |st: &[(bool, bool, bool)]| st.last().is_none_or(|t| t.0 && t.2);
    let mut out = String::new();
    let mut lines_seen: BTreeSet<String> = BTreeSet::new();
    let mut cur_file = String::new();
    let mut cur_seen = false;
    for line in s.lines() {
        let t = line.trim_start();
        let Some(d) = t.strip_prefix('#') else {
            if live(&stack) {
                if !t.is_empty() && !cur_file.is_empty() && !cur_seen {
                    lines_seen.insert(cur_file.clone());
                    cur_seen = true;
                    out.push_str("#\n");
                }
                out.push_str(line);
            }
            out.push('\n');
            continue;
        };
        out.push('\n');
        let d = d.trim_start();
        let word: String = d.chars().take_while(char::is_ascii_alphabetic).collect();
        let rest = d.get(word.len()..).unwrap_or_default().trim();
        let name = || -> String {
            rest.chars()
                .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
                .collect()
        };
        let parent = live(&stack);
        match word.as_str() {
            "ifdef" | "ifndef" | "if" => {
                let v = match word.as_str() {
                    "ifdef" => defined.contains(&name()),
                    "ifndef" => !defined.contains(&name()),
                    _ => condition(rest, &defined),
                };
                stack.push((v, v, parent));
            }
            "elif" => {
                if let Some(top) = stack.last_mut() {
                    let v = !top.1 && condition(rest, &defined);
                    top.0 = v;
                    top.1 |= v;
                }
            }
            "else" => {
                if let Some(top) = stack.last_mut() {
                    top.0 = !top.1;
                    top.1 = true;
                }
            }
            "endif" => {
                stack.pop();
            }
            "define" if parent => {
                defined.insert(name());
            }
            "undef" if parent => {
                defined.remove(&name());
            }
            "line" if parent => {
                if let Some(f) = rest.split('"').nth(1) {
                    cur_file = f.to_ascii_lowercase();
                    cur_seen = lines_seen.contains(&cur_file);
                }
            }
            "include" if parent => {
                let inc: String = rest
                    .trim_matches(|c| c == '"' || c == '<' || c == '>')
                    .split(['"', '>'])
                    .next()
                    .unwrap_or("")
                    .to_string();
                c.includes.push(inc);
            }
            _ => {}
        }
    }
    out
}

fn lex(s: &str, bud: &Budget) -> Vec<Tok> {
    let b: Vec<char> = s.chars().collect();
    let ch = |k: usize| b.get(k).copied();
    let mut v = Vec::new();
    let mut i = 0;
    'outer: while let Some(c) = ch(i) {
        if !bud.tick() {
            break;
        }
        if c.is_whitespace() {
            i += 1;
        } else if c.is_ascii_alphabetic() || c == '_' {
            let st = i;
            while ch(i).is_some_and(|x| x.is_ascii_alphanumeric() || x == '_') {
                i += 1;
            }
            let w: String = b.get(st..i).unwrap_or_default().iter().collect();
            if w == "L" && matches!(ch(i), Some('"' | '\'')) {
                continue;
            }
            v.push(Tok::Id(w));
        } else if c.is_ascii_digit() || (c == '.' && ch(i + 1).is_some_and(|d| d.is_ascii_digit()))
        {
            while ch(i).is_some_and(|x| x.is_ascii_alphanumeric() || x == '.' || x == '_') {
                i += 1;
            }
            v.push(Tok::Num);
        } else if c == '"' || c == '\'' {
            let st = i;
            i += 1;
            while ch(i).is_some_and(|x| x != c) {
                if ch(i) == Some('\\') {
                    i += 1;
                }
                i += 1;
            }
            i += 1;
            let end = i.saturating_sub(1).min(b.len());
            let body: String = b.get(st + 1..end).unwrap_or_default().iter().collect();
            if c == '"' {
                if let Some(Tok::Str(prev)) = v.last_mut() {
                    prev.push_str(&body);
                } else {
                    v.push(Tok::Str(body));
                }
            } else {
                v.push(Tok::Chr);
            }
        } else {
            for p in PUNCT {
                let n = p.len();
                if b.get(i..i + n)
                    .is_some_and(|w| w.iter().copied().eq(p.chars()))
                {
                    v.push(Tok::P(p));
                    i += n;
                    continue 'outer;
                }
            }
            i += 1;
        }
    }
    v
}

const BUILTIN: &[&str] = &[
    "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "bool",
    "__int8", "__int16", "__int32", "__int64", "wchar_t",
];
const QUAL: &[&str] = &[
    "const",
    "volatile",
    "static",
    "extern",
    "inline",
    "__inline",
    "__forceinline",
    "virtual",
    "register",
    "auto",
    "mutable",
    "explicit",
    "struct",
    "class",
    "union",
    "enum",
    "typename",
    "__cdecl",
    "__stdcall",
    "__fastcall",
    "__thiscall",
    "WINAPI",
    "CALLBACK",
    "APIENTRY",
    "__declspec",
];
const STMT_KW: &[&str] = &[
    "return", "if", "else", "while", "do", "for", "switch", "case", "default", "break", "continue",
    "goto", "delete", "new", "sizeof", "this", "throw", "try", "catch",
];

fn matching(t: &[Tok], i: usize, b: &Budget) -> usize {
    let (o, c) = match t.get(i) {
        Some(Tok::P("(")) => ("(", ")"),
        Some(Tok::P("[")) => ("[", "]"),
        _ => ("{", "}"),
    };
    let mut d = 0i64;
    let mut j = i;
    while let Some(x) = t.get(j) {
        if !b.tick() {
            return t.len();
        }
        match x {
            Tok::P(p) if *p == o => d += 1,
            Tok::P(p) if *p == c => {
                d -= 1;
                if d == 0 {
                    return j;
                }
            }
            _ => {}
        }
        j += 1;
    }
    t.len()
}

fn angle_close(t: &[Tok], i: usize, b: &Budget) -> usize {
    let mut d = 0i64;
    let mut j = i;
    while let Some(x) = t.get(j) {
        if !b.tick() {
            return t.len();
        }
        match x {
            Tok::P("<") => d += 1,
            Tok::P(">") => d -= 1,
            Tok::P(">>") => d -= 2,
            Tok::P("(") => j = matching(t, j, b),
            Tok::P(";" | "{") => return j.saturating_sub(1),
            _ => {}
        }
        if d <= 0 {
            return j;
        }
        j += 1;
    }
    t.len()
}

fn tok_text(t: &Tok) -> String {
    match t {
        Tok::Id(s) => format!("{s} "),
        Tok::Num => "0 ".to_string(),
        Tok::Str(s) => format!("\"{s}\" "),
        Tok::Chr => "'c' ".to_string(),
        Tok::P(p) => format!("{p} "),
    }
}

fn catch_type(p: &[Tok], b: &Budget) -> String {
    let mut v: Vec<&Tok> = b
        .it(p)
        .filter(|x| {
            !matches!(x, Tok::P("&")) && !matches!(is_id(Some(x)), Some("const" | "volatile"))
        })
        .collect();
    if v.len() >= 2 && matches!(v.last(), Some(Tok::Id(w)) if !BUILTIN.contains(&w.as_str())) {
        v.pop();
    }
    v.iter()
        .map(|x| tok_text(x))
        .collect::<String>()
        .trim_end()
        .to_string()
}

fn calls_ahead(t: &[Tok], b: &Budget) -> Vec<bool> {
    let n = t.len();
    let is_call = |k: usize| {
        matches!(t.get(k), Some(Tok::Id(w))
            if is_p(t, k + 1, "(")
                && !STMT_KW.contains(&w.as_str())
                && !BUILTIN.contains(&w.as_str()))
    };
    let mut before = Vec::with_capacity(n + 1);
    let mut c = 0usize;
    before.push(c);
    for k in 0..n {
        if !b.tick() {
            return vec![false; n + 1];
        }
        c += usize::from(is_call(k));
        before.push(c);
    }
    let any = |lo: usize, hi: usize| {
        before.get(hi).copied().unwrap_or(c) > before.get(lo).copied().unwrap_or(c)
    };
    let mut ans = vec![false; n + 1];
    let mut closes: Vec<usize> = Vec::new();
    for k in (0..n).rev() {
        let next = |at: usize| ans.get(at).copied().unwrap_or(false);
        let v = match t.get(k) {
            _ if is_call(k) => true,
            Some(Tok::P("}")) => {
                closes.push(k);
                false
            }
            Some(Tok::P("{")) => match closes.pop() {
                Some(m) => any(k + 1, m) || next(m + 1),
                None => any(k + 1, n),
            },
            _ => next(k + 1),
        };
        if let Some(x) = ans.get_mut(k) {
            *x = v;
        }
    }
    ans
}

fn split_commas<'a>(t: &'a [Tok], b: &Budget) -> Vec<&'a [Tok]> {
    let mut out = Vec::new();
    let (mut d, mut st) = (0i64, 0usize);
    for (j, x) in t.iter().enumerate() {
        if !b.tick() {
            break;
        }
        match x {
            Tok::P("(" | "[" | "{") => d += 1,
            Tok::P(")" | "]" | "}") => d -= 1,
            Tok::P(",") if d == 0 => {
                out.push(span(t, st, j));
                st = j + 1;
            }
            _ => {}
        }
    }
    out.push(from(t, st));
    out
}

fn has(b: &Budget, t: &[Tok], p: &str) -> bool {
    b.it(t).any(|x| matches!(x, Tok::P(q) if *q == p))
}

fn len_i64(n: usize) -> i64 {
    i64::try_from(n).unwrap_or(i64::MAX)
}

#[derive(Default, Clone)]
struct ClassInfo {
    polymorphic: bool,
    nontrivial_dtor: bool,
    ctor: bool,
    implicit_ctor: bool,
    virtual_dtor: bool,
    own_vfptr: bool,
    has_vbases: bool,
    virtuals: BTreeSet<String>,
}

impl ClassInfo {
    fn ctorish(&self) -> bool {
        self.ctor || self.implicit_ctor
    }
}

#[derive(Default, Clone)]
struct TemplateInfo {
    instance: i64,
    exotic: bool,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Region {
    Try,
    Catch,
    Local,
}

#[derive(Default)]
struct FnEh {
    states: bool,
    has_try: bool,
    has_throw: bool,
}

struct Ctx<'b> {
    b: &'b Budget,
    c: Count,
    strings: BTreeSet<String>,
    types: BTreeSet<String>,
    classes: BTreeMap<String, ClassInfo>,
    templates: BTreeMap<String, TemplateInfo>,
    overloads: BTreeMap<String, BTreeSet<String>>,
    specs: BTreeSet<String>,
    instances: BTreeSet<String>,
    eh_setup: bool,
    eh_descs: BTreeSet<String>,
    eh_thrown: BTreeSet<String>,
    eh_rethrow: bool,
    local_types: BTreeMap<String, String>,
    extern_c: bool,
    c_linkage: BTreeSet<String>,
    c_fn: bool,
    gx: bool,
    ptr_types: Vec<(usize, String, String)>,
    tok_base: usize,
    vci: bool,
    vdi: bool,
    vec_dd: BTreeSet<String>,
    scalar_dd: BTreeSet<String>,
    made_ctor: BTreeSet<String>,
}

impl Ctx<'_> {
    fn is_type(&self, s: &str) -> bool {
        BUILTIN.contains(&s)
            || self.types.contains(s)
            || self.classes.contains_key(s)
            || self.templates.contains_key(s)
    }

    fn overload(&mut self, key: &str, params: &[Tok]) -> i64 {
        if self.extern_c || self.c_linkage.contains(key) {
            self.c_linkage.insert(key.to_string());
            return 0;
        }
        let sig: Vec<String> = split_commas(params, self.b)
            .iter()
            .map(|p| catch_type(p, self.b))
            .collect();
        let sig = sig.join(",");
        let sig = if sig == "void" { String::new() } else { sig };
        let set = self.overloads.entry(key.to_string()).or_default();
        i64::from(set.insert(sig) && set.len() > 1)
    }

    fn unmodelled(&mut self, what: &str) {
        if !self.c.unmodelled.iter().any(|u| u == what) {
            self.c.unmodelled.push(what.to_string());
        }
    }

    fn template_uses(&mut self, t: &[Tok], complete_ok: bool) {
        let mut j = 0;
        let mut paren = 0i64;
        while let Some(x) = t.get(j) {
            if !self.b.tick() {
                return;
            }
            match x {
                Tok::P("(") => paren += 1,
                Tok::P(")") => paren -= 1,
                Tok::Id(n) if self.templates.contains_key(n) && is_p(t, j + 1, "<") => {
                    let e = angle_close(t, j + 1, self.b);
                    let key: String = self.b.it(span(t, j, e + 1)).map(tok_text).collect();
                    if self.specs.insert(key.clone()) {
                        self.c.add("template-id", 1);
                    }
                    let complete = complete_ok
                        && paren == 0
                        && !matches!(t.get(e + 1), Some(Tok::P("*" | "&" | "::")));
                    if complete && self.instances.insert(key) {
                        let info = self.templates.get(n).cloned().unwrap_or_default();
                        self.c.add("template instance", info.instance);
                        if info.exotic {
                            self.unmodelled("template instance with a ctor/dtor/virtual");
                        }
                    }
                    j = e;
                }
                _ => {}
            }
            j += 1;
        }
    }

    fn template(&mut self, t: &[Tok], i: usize) -> usize {
        if !is_p(t, i + 1, "<") {
            let e = self.skip_decl(t, i);
            self.template_uses(span(t, i + 1, e), true);
            return e;
        }
        let close = angle_close(t, i + 1, self.b);
        let tparams = span(t, i + 2, close);
        if tparams.is_empty() {
            self.unmodelled("explicit template specialisation");
            return self.skip_decl(t, close + 1);
        }
        let nontype = split_commas(tparams, self.b)
            .iter()
            .filter(|p| !p.is_empty() && !matches!(is_id(p.first()), Some("class" | "typename")))
            .count();
        let nontype = len_i64(nontype);
        let k = close + 1;
        let is_class = matches!(is_id(t.get(k)), Some("struct" | "class" | "union"));
        if is_class {
            let name = is_id(t.get(k + 1)).unwrap_or("").to_string();
            let mut b = k + 1;
            while t.get(b).is_some() && !is_p(t, b, "{") && !is_p(t, b, ";") {
                if !self.b.tick() {
                    return t.len();
                }
                b += 1;
            }
            if !is_p(t, b, "{") {
                self.unmodelled("template forward declaration");
                return b + 1;
            }
            let e = matching(t, b, self.b);
            let (def, info) = self.template_members(span(t, b + 1, e), &name);
            self.c.add("template", 2 + def + nontype);
            self.templates.insert(name, info);
            let mut after = e + 1;
            if is_p(t, after, ";") {
                after += 1;
            }
            return after;
        }
        let e = self.skip_decl(t, k);
        let mut j = k;
        while j < e.min(t.len()) {
            if !self.b.tick() {
                return t.len();
            }
            if is_p(t, j, "<") {
                j = angle_close(t, j, self.b);
            } else if is_p(t, j, "(") {
                let pe = matching(t, j, self.b);
                let p = self.params(span(t, j + 1, pe));
                self.c.add("template", 1 + p + nontype);
                break;
            }
            j += 1;
        }
        e
    }

    fn template_members(&mut self, t: &[Tok], name: &str) -> (i64, TemplateInfo) {
        let mut def = 0;
        let mut info = TemplateInfo {
            instance: 8,
            exotic: false,
        };
        let mut i = 0;
        while let Some(x) = t.get(i) {
            if !self.b.tick() {
                break;
            }
            if matches!(x, Tok::Id(w) if w == "public" || w == "private" || w == "protected")
                && is_p(t, i + 1, ":")
            {
                i += 2;
                continue;
            }
            if is_p(t, i, ";") {
                i += 1;
                continue;
            }
            let st = i;
            let mut j = i;
            let mut paren = None;
            while let Some(y) = t.get(j) {
                if !self.b.tick() {
                    break;
                }
                match y {
                    Tok::P("(") => {
                        let e = matching(t, j, self.b);
                        if paren.is_none() {
                            paren = Some((j, e));
                        }
                        j = e;
                    }
                    Tok::P(";") => break,
                    Tok::P("{") => {
                        j = matching(t, j, self.b);
                        break;
                    }
                    _ => {}
                }
                j += 1;
            }
            let m = span(t, st, j);
            let is_static = self.b.it(m).any(|x| is_id(Some(x)) == Some("static"));
            match paren {
                Some((p, e)) if !has(self.b, span(m, 0, p.saturating_sub(st)), "=") => {
                    let n = 1 + self.params(span(t, p + 1, e));
                    def += n;
                    info.instance += n;
                    let fname = p.checked_sub(1).and_then(|q| is_id(t.get(q)));
                    if fname.is_some_and(|f| f != name && f != "operator") {
                        let o = self.overload(
                            &format!("template {name}::{}", fname.unwrap_or("")),
                            span(t, p + 1, e),
                        );
                        def += o;
                        info.instance += o;
                    }
                    if has(self.b, m, "~")
                        || fname == Some(name)
                        || self.b.it(m).any(|x| is_id(Some(x)) == Some("virtual"))
                    {
                        info.exotic = true;
                    }
                }
                _ => {
                    let n = len_i64(
                        split_commas(m, self.b)
                            .iter()
                            .filter(|d| !d.is_empty())
                            .count(),
                    );
                    def += n;
                    info.instance += if is_static { 2 * n } else { n };
                }
            }
            i = j + 1;
        }
        (def, info)
    }

    fn params(&self, t: &[Tok]) -> i64 {
        if t.is_empty() || !self.b.enter() {
            return 0;
        }
        let mut n = 0;
        for p in split_commas(t, self.b) {
            if p.is_empty() || p == [Tok::P("...")] {
                continue;
            }
            n += 1;
            let end = self
                .b
                .it(p)
                .position(|x| *x == Tok::P("="))
                .unwrap_or(p.len());
            n += self.nested_params(span(p, 0, end));
        }
        self.b.leave();
        n
    }

    fn nested_params(&self, p: &[Tok]) -> i64 {
        let mut n = 0;
        let mut j = 0;
        while p.get(j).is_some() {
            if !self.b.tick() {
                break;
            }
            if is_p(p, j, "(") {
                let e = matching(p, j, self.b);
                let inner = span(p, j + 1, e);
                let is_decl_group = inner.first() == Some(&Tok::P("*"))
                    || inner.first() == Some(&Tok::P("&"))
                    || self.b.it(inner).any(|x| {
                        matches!(x, Tok::Id(s) if s == "__cdecl" || s == "__stdcall" || s == "WINAPI" || s == "CALLBACK")
                    });
                if !is_decl_group && j > 0 {
                    n += self.params(inner);
                }
                j = e + 1;
            } else {
                j += 1;
            }
        }
        n
    }

    fn string(&mut self, s: &str) {
        if self.strings.insert(s.to_string()) {
            self.c.add("string", 1);
        }
    }

    fn scope(&mut self, t: &[Tok]) {
        if !self.b.enter() {
            return;
        }
        let mut i = 0;
        while i < t.len() && self.b.tick() {
            i = self.decl(t, i, None);
        }
        self.b.leave();
    }

    #[allow(clippy::too_many_lines)]
    fn decl(&mut self, t: &[Tok], i: usize, class: Option<&str>) -> usize {
        let Some(first) = t.get(i) else {
            return t.len();
        };
        match first {
            Tok::P(";") => return i + 1,
            Tok::P("#") => {
                self.c.add("file", 1);
                return i + 1;
            }
            Tok::P("}" | "{") => return i + 1,
            Tok::Id(w)
                if class.is_some() && (w == "public" || w == "private" || w == "protected") =>
            {
                return if is_p(t, i + 1, ":") { i + 2 } else { i + 1 };
            }
            Tok::Id(w) if w == "namespace" => {
                let mut j = i + 1;
                while t.get(j).is_some() && !is_p(t, j, "{") {
                    if !self.b.tick() {
                        return t.len();
                    }
                    j += 1;
                }
                let e = matching(t, j, self.b);
                self.scope(span(t, j + 1, e));
                return e + 1;
            }
            Tok::Id(w) if w == "template" => {
                return self.template(t, i);
            }
            Tok::Id(w) if w == "extern" && matches!(t.get(i + 1), Some(Tok::Str(_))) => {
                let c = matches!(t.get(i + 1), Some(Tok::Str(s)) if s == "C");
                let was = std::mem::replace(&mut self.extern_c, c);
                let r = if is_p(t, i + 2, "{") {
                    let e = matching(t, i + 2, self.b);
                    self.scope(span(t, i + 3, e));
                    e + 1
                } else if self.b.enter() {
                    let r = self.decl(t, i + 2, class);
                    self.b.leave();
                    r
                } else {
                    t.len()
                };
                self.extern_c = was;
                return r;
            }
            _ => {}
        }
        let mut j = i;
        let mut is_typedef = false;
        let mut is_static = false;
        let mut is_virtual = false;
        let mut is_friend = false;
        let mut head_end = None;
        let mut tag_kind: Option<String> = None;
        let mut tag_name: Option<String> = None;
        while let Some(x) = t.get(j) {
            if !self.b.tick() {
                return t.len();
            }
            match x {
                Tok::Id(w) if w == "typedef" => is_typedef = true,
                Tok::Id(w) if w == "static" => is_static = true,
                Tok::Id(w) if w == "virtual" => is_virtual = true,
                Tok::Id(w) if w == "friend" => is_friend = true,
                Tok::Id(w) if w == "__declspec" => {
                    if is_p(t, j + 1, "(") {
                        j = matching(t, j + 1, self.b);
                    }
                }
                Tok::Id(w)
                    if (w == "struct" || w == "class" || w == "union" || w == "enum")
                        && tag_kind.is_none() =>
                {
                    tag_kind = Some(w.clone());
                    if let Some(n) = is_id(t.get(j + 1)) {
                        tag_name = Some(n.to_string());
                    }
                    let mut k = j + 1;
                    let mut in_bases = false;
                    while let Some(y) = t.get(k) {
                        if !self.b.tick() {
                            return t.len();
                        }
                        match y {
                            Tok::P("{") => {
                                head_end = Some(k);
                                break;
                            }
                            Tok::P(":") => {
                                in_bases = true;
                                k += 1;
                            }
                            Tok::P(",") if in_bases => k += 1,
                            Tok::P(";" | "(" | "=" | ")" | ",") => break,
                            _ => k += 1,
                        }
                    }
                }
                _ => {}
            }
            if head_end.is_some() {
                break;
            }
            match t.get(j) {
                Some(Tok::P(";" | "{" | "(" | "=" | "," | ":" | "[")) => break,
                _ => j += 1,
            }
        }
        if let Some(b) = head_end {
            let e = matching(t, b, self.b);
            let kind = tag_kind.clone().unwrap_or_default();
            let name = tag_name.clone().filter(|n| {
                self.b
                    .it(span(t, i + 1, b))
                    .any(|x| is_id(Some(x)) == Some(n.as_str()))
            });
            if kind == "enum" {
                self.c.add("enum", 1);
                let body = span(t, b + 1, e);
                let n = len_i64(
                    split_commas(body, self.b)
                        .iter()
                        .filter(|x| !x.is_empty())
                        .count(),
                );
                self.c.add("enumerator", n);
                if let Some(n) = &name {
                    self.types.insert(n.clone());
                }
            } else {
                let head = span(t, i, b);
                let mut bases: Vec<String> = Vec::new();
                let mut vbases: Vec<String> = Vec::new();
                if let Some(p) = self.b.it(head).position(|x| *x == Tok::P(":")) {
                    let mut seg_virtual = false;
                    let mut angle = 0usize;
                    for x in self.b.it(from(head, p + 1)) {
                        match x {
                            Tok::P("<") => angle += 1,
                            Tok::P(">") => angle = angle.saturating_sub(1),
                            Tok::P(",") if angle == 0 => seg_virtual = false,
                            Tok::Id(w) if w == "virtual" => seg_virtual = true,
                            Tok::Id(w)
                                if ["public", "private", "protected"].contains(&w.as_str()) => {}
                            Tok::Id(w) => {
                                bases.push(w.clone());
                                if seg_virtual {
                                    vbases.push(w.clone());
                                }
                            }
                            _ => {}
                        }
                    }
                }
                if !self.b.enter() {
                    return t.len();
                }
                let info = self.class_body(span(t, b + 1, e), name.as_deref(), &bases, &vbases);
                self.b.leave();
                if let Some(n) = &name {
                    self.classes.insert(n.clone(), info);
                }
            }
            let after = e + 1;
            let mut k = after;
            let mut n = 0;
            let mut any = false;
            let mut depth = 0i64;
            while let Some(x) = t.get(k) {
                if !self.b.tick() {
                    return t.len();
                }
                if depth == 0 && *x == Tok::P(";") {
                    break;
                }
                match x {
                    Tok::P("(" | "[" | "{") => depth += 1,
                    Tok::P(")" | "]" | "}") => depth -= 1,
                    Tok::P(",") if depth == 0 => {
                        n += 1;
                        any = false;
                    }
                    Tok::Id(w) if depth == 0 && !QUAL.contains(&w.as_str()) => {
                        if is_typedef {
                            self.types.insert(w.clone());
                        }
                        any = true;
                    }
                    _ => {}
                }
                k += 1;
            }
            if any {
                n += 1;
            }
            self.c
                .add(if is_typedef { "typedef" } else { "declarator" }, n);
            if class.is_none() && !is_typedef && n == 0 && kind == "union" && tag_name.is_none() {
                self.c.add("anonymous union object", 3);
            }
            return k + 1;
        }
        if tag_kind.is_some() && is_p(t, j, ";") && j == i + 2 {
            let known = tag_name
                .as_ref()
                .is_some_and(|n| self.classes.contains_key(n));
            if let Some(n) = &tag_name {
                self.classes.entry(n.clone()).or_default();
            }
            if !known {
                self.c.add("forward", 1);
            }
            return j + 1;
        }
        let _ = is_virtual;
        self.simple(t, i, class, is_typedef, is_static, is_friend)
    }

    fn skip_decl(&self, t: &[Tok], i: usize) -> usize {
        let mut j = i;
        while let Some(x) = t.get(j) {
            if !self.b.tick() {
                return t.len();
            }
            match x {
                Tok::P(";") => return j + 1,
                Tok::P("{") => {
                    let e = matching(t, j, self.b);
                    if is_p(t, e + 1, ";") {
                        return e + 2;
                    }
                    return e + 1;
                }
                _ => j += 1,
            }
        }
        t.len()
    }

    #[allow(clippy::too_many_lines)]
    fn simple(
        &mut self,
        t: &[Tok],
        i: usize,
        class: Option<&str>,
        is_typedef: bool,
        is_static: bool,
        is_friend: bool,
    ) -> usize {
        if self
            .b
            .it(from(t, i))
            .take_while(|x| **x != Tok::P("(") && **x != Tok::P(";"))
            .any(|x| is_id(Some(x)).is_some_and(|w| w.starts_with(PROBE)))
        {
            self.c.marks.push(self.c.total);
        }
        let mut j = i;
        let mut depth = 0i64;
        let mut body = None;
        let mut paren_seen = false;
        let mut eq_scan = i;
        let mut eq_found = false;
        while let Some(x) = t.get(j) {
            if !self.b.tick() {
                return t.len();
            }
            match x {
                Tok::P("(" | "[") => depth += 1,
                Tok::P(")" | "]") => {
                    depth -= 1;
                    if depth == 0 {
                        paren_seen = true;
                    }
                }
                Tok::P("{") if depth == 0 => {
                    while eq_scan < j && !eq_found {
                        eq_found = is_p(t, eq_scan, "=");
                        eq_scan += 1;
                    }
                    if paren_seen && !eq_found {
                        body = Some(j);
                        break;
                    }
                    j = matching(t, j, self.b);
                }
                Tok::P(";") if depth == 0 => break,
                _ => {}
            }
            j += 1;
        }
        let decl_end = body.unwrap_or(j);
        let head = span(t, i, decl_end);
        let mut this_fn = false;
        let mut c_fn = self.extern_c;
        let mut n_params = 0;
        for d in split_commas(head, self.b) {
            if d.is_empty() || !self.b.tick() {
                continue;
            }
            let upto = (0..d.len())
                .take_while(|_| self.b.tick())
                .find(|&k| {
                    (is_p(d, k, "=") || is_p(d, k, ":"))
                        && !(k > 0 && is_id(d.get(k - 1)) == Some("operator"))
                })
                .unwrap_or(d.len());
            let d = span(d, 0, upto);
            let ids: Vec<&str> = self.b.it(d).filter_map(|x| is_id(Some(x))).collect();
            if ids.is_empty() && !has(self.b, d, "~") {
                continue;
            }
            let mut pl = None;
            let mut k = 0;
            while d.get(k).is_some() {
                if !self.b.tick() {
                    break;
                }
                if is_p(d, k, "(") {
                    let e = matching(d, k, self.b);
                    let inner = span(d, k + 1, e);
                    let group = inner.first() == Some(&Tok::P("*"))
                        || inner.first() == Some(&Tok::P("&"))
                        || inner.first().is_some_and(|x| {
                            matches!(x, Tok::Id(s) if s == "__cdecl" || s == "__stdcall" || s == "WINAPI" || s == "CALLBACK" || s == "APIENTRY")
                        });
                    if group {
                        let rest = from(d, e + 1);
                        if rest.first() == Some(&Tok::P("(")) {
                            let e2 = matching(rest, 0, self.b);
                            n_params += self.params(span(rest, 1, e2));
                        }
                        break;
                    }
                    if k > 0 {
                        pl = Some((k, e));
                    }
                    break;
                }
                k += 1;
            }
            let dtor = has(self.b, d, "~");
            let name = if let Some((k, _)) = pl {
                is_id(self.b.rit(span(d, 0, k)).find(|x| matches!(x, Tok::Id(_)))).unwrap_or("")
            } else {
                ids.last().copied().unwrap_or("")
            };
            if is_typedef {
                self.types.insert(name.to_string());
            }
            let qualified = pl.is_some_and(|(k, _)| has(self.b, span(d, 0, k), "::"));
            if let Some((k, e)) = pl {
                let p = self.params(span(d, k + 1, e));
                n_params += p;
                if dtor {
                    if class.is_some() {
                    } else if body.is_none() {
                        self.c.add("declarator", 1);
                    }
                    this_fn = true;
                } else {
                    if !(class.is_some() && Some(name) == class) {
                        self.c.add(
                            if is_friend {
                                "friend"
                            } else if is_typedef {
                                "typedef"
                            } else {
                                "declarator"
                            },
                            1,
                        );
                    }
                    this_fn = (class.is_some() && !is_static) || qualified;
                    if !is_typedef && !is_friend && !(class.is_some() && Some(name) == class) {
                        let scope = if qualified {
                            is_id(
                                self.b
                                    .rit(span(d, 0, k))
                                    .skip(2)
                                    .find(|x| matches!(x, Tok::Id(_))),
                            )
                            .unwrap_or("")
                            .to_string()
                        } else {
                            class.unwrap_or("").to_string()
                        };
                        let fname = if name == "operator" {
                            let head = span(d, 0, k);
                            let at = head
                                .iter()
                                .rposition(|x| is_id(Some(x)) == Some("operator"));
                            at.map(|a| self.b.it(span(d, a, k)).map(tok_text).collect())
                                .unwrap_or_default()
                        } else {
                            name.to_string()
                        };
                        let key = format!("{scope}::{fname}");
                        let o = self.overload(&key, span(d, k + 1, e));
                        self.c.add("overload", o);
                        c_fn = self.c_linkage.contains(&key);
                    }
                }
            } else if is_typedef {
                self.c.add("typedef", 1);
            } else if class.is_some() && !is_friend {
                self.c.add(
                    if is_static { "static member" } else { "member" },
                    if is_static { 2 } else { 1 },
                );
            } else {
                self.c.add("declarator", 1);
            }
        }
        self.c.add("param", n_params);
        self.template_uses(head, !is_typedef);
        self.expr_items(head);
        match body {
            Some(b) => {
                let e = matching(t, b, self.b);
                self.c.add("exit label", 1);
                if this_fn {
                    self.c.add("this", 1);
                }
                if self.b.enter() {
                    let was = std::mem::replace(&mut self.c_fn, c_fn);
                    self.body(span(t, b + 1, e));
                    self.c_fn = was;
                    self.b.leave();
                }
                e + 1
            }
            None => j + 1,
        }
    }

    #[allow(clippy::too_many_lines)]
    fn class_body(
        &mut self,
        t: &[Tok],
        name: Option<&str>,
        bases: &[String],
        vbases: &[String],
    ) -> ClassInfo {
        let mut info = ClassInfo::default();
        self.c.add("class", 8);
        let base_info: Vec<(ClassInfo, bool)> = bases
            .iter()
            .filter_map(|b| {
                self.classes
                    .get(b)
                    .cloned()
                    .map(|c| (c, vbases.contains(b)))
            })
            .collect();
        if let Some(n) = name {
            self.classes.entry(n.to_string()).or_default();
        }
        info.virtuals = base_info
            .iter()
            .flat_map(|(b, _)| b.virtuals.iter().cloned())
            .collect();
        let mut new_virtual = false;
        let mut has_virtual = false;
        let mut has_dtor = false;
        let mut ctors = 0;
        let base_vdtor = bases
            .iter()
            .any(|b| self.classes.get(b).is_some_and(|c| c.virtual_dtor));
        let mut i = 0;
        let mut member_classes: Vec<String> = Vec::new();
        while let Some(x) = t.get(i) {
            if !self.b.tick() {
                break;
            }
            if matches!(x, Tok::Id(w) if w == "public" || w == "private" || w == "protected")
                && is_p(t, i + 1, ":")
            {
                i += 2;
                continue;
            }
            let st = i;
            let rest = from(t, st);
            let is_virtual = self
                .b
                .it(rest)
                .take_while(|x| **x != Tok::P(";") && **x != Tok::P("{") && **x != Tok::P("("))
                .any(|x| is_id(Some(x)) == Some("virtual"));
            let dtor_at = self
                .b
                .it(rest)
                .take_while(|x| **x != Tok::P(";") && **x != Tok::P("{"))
                .position(|x| *x == Tok::P("~"));
            let ctor_at = name.and_then(|n| {
                let lim = self
                    .b
                    .it(rest)
                    .position(|x| *x == Tok::P(";") || *x == Tok::P("{"))
                    .unwrap_or(rest.len());
                (0..lim.saturating_sub(1))
                    .take_while(|_| self.b.tick())
                    .find(|&k| {
                        is_id(rest.get(k)) == Some(n)
                            && is_p(rest, k + 1, "(")
                            && (k == 0 || !is_p(rest, k - 1, "~"))
                            && (k == 0
                                || !matches!(rest.get(k - 1), Some(Tok::Id(_)))
                                || is_id(rest.get(k - 1)).is_some_and(|w| QUAL.contains(&w)))
                    })
            });
            if let Some(w) = is_id(t.get(st))
                && self.classes.contains_key(w)
                && is_id(t.get(st + 1)).is_some()
            {
                member_classes.push(w.to_string());
            }
            if is_virtual {
                has_virtual = true;
                if dtor_at.is_some() {
                    new_virtual |= !base_vdtor;
                } else {
                    let fname = self
                        .b
                        .it(rest)
                        .take_while(|x| **x != Tok::P(";") && **x != Tok::P("{"))
                        .position(|x| *x == Tok::P("("))
                        .and_then(|k| k.checked_sub(1))
                        .and_then(|k| is_id(rest.get(k)))
                        .map(str::to_string);
                    match fname {
                        Some(f) => {
                            if info.virtuals.insert(f) {
                                new_virtual = true;
                            }
                        }
                        None => new_virtual = true,
                    }
                }
            }
            if let Some(k) = dtor_at {
                has_dtor = true;
                self.c.add("destructor", 4);
                let is_virtual = is_virtual || base_vdtor;
                if is_virtual {
                    self.c.add("virtual dtor", 1);
                    info.virtual_dtor = true;
                    let inline_body = self
                        .b
                        .it(from(rest, k))
                        .take_while(|x| **x != Tok::P(";"))
                        .any(|x| *x == Tok::P("{"));
                    if inline_body {
                        self.c.add("inline virtual dtor", 4);
                    }
                }
                if let Some(n) = name {
                    let e = self.classes.entry(n.to_string()).or_default();
                    e.nontrivial_dtor = true;
                    e.virtual_dtor |= is_virtual;
                }
            }
            if let Some(k) = ctor_at {
                ctors += 1;
                let s = from(rest, k + 1);
                let e = matching(s, 0, self.b);
                let p = span(s, 1, e);
                let copy = name.is_some_and(|n| {
                    self.b.it(p).any(|x| is_id(Some(x)) == Some(n))
                        && has(self.b, p, "&")
                        && split_commas(p, self.b).len() == 1
                });
                self.c.add("constructor", 1);
                info.ctor = true;
                if let Some(n) = name {
                    self.classes.entry(n.to_string()).or_default().ctor = true;
                }
                if ctors == 1 {
                    self.c.add("implicit ctor", -1);
                } else {
                    self.c.add("ctor overload", 1);
                }
                if copy {
                    self.c.add("implicit copy ctor", -3);
                }
            }
            if let Some(n) = name {
                let lim = self
                    .b
                    .it(rest)
                    .position(|x| *x == Tok::P(";") || *x == Tok::P("{"))
                    .unwrap_or(rest.len());
                if let Some(k) = (0..lim)
                    .take_while(|_| self.b.tick())
                    .find(|&k| is_id(rest.get(k)) == Some("operator") && is_p(rest, k + 1, "="))
                    && is_p(rest, k + 2, "(")
                {
                    let e = matching(rest, k + 2, self.b);
                    if self
                        .b
                        .it(span(rest, k + 3, e))
                        .any(|x| is_id(Some(x)) == Some(n))
                    {
                        self.c.add("implicit operator=", -2);
                    }
                }
            }
            i = self.decl(t, st, name.or(Some("")));
            if i <= st {
                i = st + 1;
            }
        }
        let base_poly = base_info.iter().any(|(b, _)| b.polymorphic);
        let sub_dtor = base_info.iter().any(|(b, _)| b.nontrivial_dtor)
            || member_classes
                .iter()
                .any(|m| self.classes.get(m).is_some_and(|c| c.nontrivial_dtor));
        let nv_vfptr = base_info.iter().any(|(b, v)| !v && b.own_vfptr);
        let vbased = !vbases.is_empty() || base_info.iter().any(|(b, _)| b.has_vbases);
        if !vbased {
            if has_virtual {
                self.c.add("vftable", 3);
            } else if base_poly {
                self.c.add("derived vftable", 2);
            }
        } else {
            if new_virtual && !nv_vfptr {
                self.c.add("vftable", 3);
            }
            let poly_bases = base_info.iter().filter(|(b, _)| b.polymorphic).count();
            self.c.add("derived vftable", 2 * len_i64(poly_bases));
        }
        if base_info.iter().any(|(_, v)| *v) {
            self.c.add("virtual base", 3);
            if base_info.iter().any(|(b, v)| *v && b.nontrivial_dtor) {
                self.c.add("virtual base dtor", 1);
            }
        }
        let nv_vbased = base_info.iter().filter(|(b, v)| !v && b.has_vbases).count();
        self.c.add("inherited virtual base", 2 * len_i64(nv_vbased));
        if !has_dtor && sub_dtor {
            self.c.add("implicit dtor", 4);
            if base_vdtor {
                self.c.add("virtual dtor", 1);
            }
        }
        info.polymorphic = has_virtual || base_poly;
        info.own_vfptr = new_virtual || nv_vfptr;
        info.has_vbases = !vbases.is_empty() || base_info.iter().any(|(b, _)| b.has_vbases);
        info.nontrivial_dtor = has_dtor || sub_dtor;
        info.virtual_dtor |= base_info.iter().any(|(b, _)| b.virtual_dtor);
        let sub_ctor = base_info.iter().map(|(b, _)| b).any(ClassInfo::ctorish)
            || member_classes
                .iter()
                .any(|m| self.classes.get(m).is_some_and(ClassInfo::ctorish));
        info.implicit_ctor = !info.ctor && (info.polymorphic || sub_ctor);
        info
    }

    fn expr_items(&mut self, t: &[Tok]) {
        let mut j = 0;
        let mut cast_close = usize::MAX;
        while let Some(x) = t.get(j) {
            if !self.b.tick() {
                return;
            }
            match x {
                Tok::Str(s) => {
                    let s = s.clone();
                    self.string(&s);
                }
                Tok::Id(w) if w == "new" && !(j > 0 && is_id(t.get(j - 1)) == Some("operator")) => {
                    self.c.add("new", 2);
                    self.new_expr(from(t, j + 1));
                }
                Tok::Id(w)
                    if w == "delete" && !(j > 0 && is_id(t.get(j - 1)) == Some("operator")) =>
                {
                    self.c.add("delete", 1);
                    self.delete_expr(from(t, j + 1));
                }
                Tok::P("(") => {
                    let e = matching(t, j, self.b);
                    let inner = span(t, j + 1, e);
                    let prev = j.checked_sub(1).and_then(|p| t.get(p));
                    let prev_is_sizeof = is_id(prev) == Some("sizeof");
                    let prev_callable = matches!(prev, Some(Tok::Id(w)) if !STMT_KW.contains(&w.as_str()) || w == "sizeof")
                        && !prev_is_sizeof;
                    let prev_callable = prev_callable
                        || (matches!(prev, Some(Tok::P(")" | "]")))
                            && j.checked_sub(1) != Some(cast_close));
                    if !prev_callable && self.is_type_id(inner) {
                        let operand = matches!(
                            t.get(e + 1),
                            Some(
                                Tok::Id(_)
                                    | Tok::Num
                                    | Tok::Str(_)
                                    | Tok::Chr
                                    | Tok::P("(" | "*" | "&" | "-" | "~" | "!")
                            )
                        );
                        if prev_is_sizeof || operand {
                            self.c.add("type-id", 1);
                            cast_close = e;
                        }
                    }
                }
                _ => {}
            }
            j += 1;
        }
    }

    fn construct(&mut self, n: &str) {
        if self.classes.get(n).is_some_and(|c| c.implicit_ctor)
            && self.made_ctor.insert(n.to_string())
        {
            self.c.add("implicit ctor made", 2);
        }
    }

    fn vec_ddtor(&mut self, n: &str) {
        if self.vec_dd.insert(n.to_string()) {
            self.c.add("??_E", 7);
            if !self.gx && !self.vdi {
                self.vdi = true;
                self.c.add("vector dtor iterator", 12);
            }
        }
    }

    fn new_expr(&mut self, t: &[Tok]) {
        let mut k = 0;
        if t.first() == Some(&Tok::P("(")) {
            k = matching(t, 0, self.b) + 1;
            self.c.add("placement new", 1);
        }
        while let (Some(_), Some(Tok::P("::"))) = (is_id(t.get(k)), t.get(k + 1)) {
            if !self.b.tick() {
                return;
            }
            k += 2;
        }
        let Some(n) = is_id(t.get(k)).map(str::to_string) else {
            return;
        };
        if is_p(t, k + 1, "<") {
            if self.templates.contains_key(&n) {
                self.unmodelled("new of a class template instance");
            }
            return;
        }
        let Some(ci) = self.classes.get(&n).cloned() else {
            return;
        };
        if is_p(t, k + 1, "*") {
            return;
        }
        self.construct(&n);
        if !is_p(t, k + 1, "[") {
            if self.gx && ci.ctorish() {
                self.c.add("new ctor eh", 2);
            }
            return;
        }
        if ci.implicit_ctor && ci.virtual_dtor {
            self.unmodelled("new T[n] of a class with an implicit ctor and a virtual dtor");
        }
        let e = matching(t, k + 1, self.b);
        let literal = self
            .b
            .it(span(t, k + 2, e))
            .all(|x| matches!(x, Tok::Num | Tok::P(_)));
        let dtor = ci.nontrivial_dtor;
        if ci.ctorish() {
            if !(self.gx && dtor) && !self.vci {
                self.vci = true;
                self.c.add("vector ctor iterator", 11);
            }
            let site = match (self.gx, dtor) {
                (true, true) => 4,
                (true, false) => 3,
                (false, _) => 1,
            };
            self.c.add("new[] site", site + i64::from(!literal));
        }
        if dtor {
            self.vec_ddtor(&n);
        }
    }

    fn delete_expr(&mut self, t: &[Tok]) {
        let arr = t.first() == Some(&Tok::P("["));
        let s = if arr { from(t, 2) } else { t };
        let mut end = 0;
        let mut d = 0i64;
        while let Some(x) = s.get(end) {
            if !self.b.tick() {
                return;
            }
            match x {
                Tok::P("(" | "[") => d += 1,
                Tok::P(")" | "]") if d == 0 => break,
                Tok::P(")" | "]") => d -= 1,
                Tok::P(";" | ",") if d == 0 => break,
                _ => {}
            }
            end += 1;
        }
        let mut op = span(s, 0, end);
        let ty = if op.first() == Some(&Tok::P("("))
            && self.is_type_id(span(op, 1, matching(op, 0, self.b)))
        {
            is_id(op.get(1)).map(str::to_string)
        } else {
            while op.last() == Some(&Tok::P("]")) {
                if !self.b.tick() {
                    return;
                }
                let open = op.iter().rposition(|x| *x == Tok::P("[")).unwrap_or(0);
                op = span(op, 0, open);
            }
            let pos =
                (op.as_ptr() as usize).saturating_sub(self.tok_base) / std::mem::size_of::<Tok>();
            is_id(op.last()).and_then(|nm| {
                self.b
                    .rit(&self.ptr_types)
                    .find(|(at, n, _)| *at < pos && n == nm)
                    .or_else(|| self.b.it(&self.ptr_types).find(|(_, n, _)| n == nm))
                    .map(|x| x.2.clone())
            })
        };
        let Some(ty) = ty else { return };
        let Some(ci) = self.classes.get(&ty).cloned() else {
            return;
        };
        if !ci.nontrivial_dtor {
            return;
        }
        self.c.add("delete dtor site", 1);
        if arr {
            if !ci.virtual_dtor {
                self.vec_ddtor(&ty);
            }
        } else if !ci.virtual_dtor && self.scalar_dd.insert(ty.clone()) {
            self.c.add("scalar delete body", 4);
        }
    }

    fn is_type_id(&self, t: &[Tok]) -> bool {
        if t.is_empty() {
            return false;
        }
        let mut k = 0;
        let mut saw_type = false;
        while let Some(x) = t.get(k) {
            if !self.b.tick() {
                return false;
            }
            match x {
                Tok::Id(w) if QUAL.contains(&w.as_str()) => {}
                Tok::Id(w) if self.is_type(w) => {
                    if saw_type && !BUILTIN.contains(&w.as_str()) {
                        return false;
                    }
                    saw_type = true;
                }
                Tok::Id(_) if !saw_type => {
                    saw_type = self.b.it(from(t, k + 1)).all(|x| {
                        *x == Tok::P("*") || *x == Tok::P("&") || is_id(Some(x)) == Some("const")
                    }) && k + 1 < t.len();
                    if !saw_type {
                        return false;
                    }
                }
                Tok::P("*" | "&") if saw_type => {}
                Tok::P("::") => {}
                _ => return false,
            }
            k += 1;
        }
        saw_type
    }

    #[allow(clippy::too_many_lines)]
    fn body(&mut self, t: &[Tok]) {
        let mut j = 0;
        let mut stmt_start = true;
        let mut last_label = false;
        let mut do_depth: Vec<i64> = Vec::new();
        let mut depth = 0i64;
        let mut regions: Vec<(Region, i64)> = Vec::new();
        let mut pending: Option<Region> = None;
        let mut fe = FnEh::default();
        let mut ahead: Option<Vec<bool>> = None;
        while let Some(tok) = t.get(j) {
            if !self.b.tick() {
                return;
            }
            let at_start = stmt_start;
            stmt_start = false;
            match tok {
                Tok::P("{") => {
                    depth += 1;
                    if let Some(k) = pending.take() {
                        regions.push((k, depth));
                    }
                    stmt_start = true;
                    j += 1;
                    continue;
                }
                Tok::P("}") => {
                    regions.retain(|r| r.1 < depth);
                    depth -= 1;
                    stmt_start = true;
                    j += 1;
                    continue;
                }
                Tok::P(";") => {
                    stmt_start = true;
                    last_label = false;
                    j += 1;
                    continue;
                }
                _ => {}
            }
            if let Tok::Id(w) = tok {
                match w.as_str() {
                    "if" => {
                        self.c.add("if", 1);
                        j = self.cond(t, j + 1);
                        stmt_start = true;
                        last_label = false;
                        continue;
                    }
                    "else" => {
                        self.c.add("else", 1);
                        stmt_start = true;
                        last_label = false;
                        j += 1;
                        continue;
                    }
                    "while" => {
                        if do_depth.last() == Some(&depth) {
                            do_depth.pop();
                        } else {
                            self.c.add("loop", 3);
                        }
                        j = self.cond(t, j + 1);
                        stmt_start = true;
                        last_label = false;
                        continue;
                    }
                    "do" => {
                        self.c.add("loop", 3);
                        do_depth.push(depth);
                        stmt_start = true;
                        j += 1;
                        continue;
                    }
                    "for" => {
                        self.c.add("loop", 3);
                        if is_p(t, j + 1, "(") {
                            let e = matching(t, j + 1, self.b);
                            let inner = span(t, j + 2, e);
                            let semi = self
                                .b
                                .it(inner)
                                .position(|x| *x == Tok::P(";"))
                                .unwrap_or(inner.len());
                            if self.looks_like_decl(span(inner, 0, semi)) {
                                self.local_decl(span(inner, 0, semi));
                                self.expr_items(from(inner, semi));
                            } else {
                                self.expr_items(inner);
                            }
                            j = e + 1;
                        } else {
                            j += 1;
                        }
                        stmt_start = true;
                        last_label = false;
                        continue;
                    }
                    "switch" => {
                        self.c.add("switch", 4);
                        j = self.cond(t, j + 1);
                        stmt_start = true;
                        continue;
                    }
                    "struct" | "class" | "union" | "enum" if at_start => {
                        let j2 = if self.b.enter() {
                            let r = self.decl(t, j, None);
                            self.b.leave();
                            r
                        } else {
                            t.len()
                        };
                        j = j2.max(j + 1);
                        stmt_start = true;
                        continue;
                    }
                    "try" => {
                        fe.states = true;
                        fe.has_try = true;
                        let nest = if regions.iter().any(|r| r.0 == Region::Catch) {
                            2
                        } else if regions.is_empty() {
                            0
                        } else {
                            1
                        };
                        self.c.add("try", 1 + nest);
                        pending = Some(Region::Try);
                        j += 1;
                        stmt_start = true;
                        last_label = false;
                        continue;
                    }
                    "catch" => {
                        if is_p(t, j + 1, "(") {
                            let e = matching(t, j + 1, self.b);
                            let inner = span(t, j + 2, e);
                            if inner != [Tok::P("...")] {
                                self.c.add("catch", 1);
                                let key = catch_type(inner, self.b);
                                self.eh_desc(&key);
                            }
                            j = e + 1;
                        } else {
                            j += 1;
                        }
                        pending = Some(Region::Catch);
                        stmt_start = true;
                        last_label = false;
                        continue;
                    }
                    "throw" => {
                        if is_p(t, j + 1, ";") {
                            if !self.eh_rethrow {
                                self.eh_rethrow = true;
                                self.c.add("rethrow", 1);
                            }
                        } else {
                            fe.has_throw = true;
                            self.c.add("throw", 2);
                            let key = self.thrown_type(from(t, j + 1));
                            self.eh_desc(&key);
                            if self.eh_thrown.insert(key) {
                                let first = i64::from(self.eh_thrown.len() == 1);
                                self.c.add("eh throwinfo", 6 + first);
                            }
                        }
                        j += 1;
                        last_label = false;
                        continue;
                    }
                    "return"
                        if regions.iter().any(|r| r.0 == Region::Catch) && !is_p(t, j + 1, ";") =>
                    {
                        self.c.add("catch return", 2);
                    }
                    "case" | "default" => {
                        if !last_label {
                            self.c.add("case", 1);
                        }
                        while t.get(j).is_some() && !is_p(t, j, ":") {
                            if !self.b.tick() {
                                return;
                            }
                            j += 1;
                        }
                        j += 1;
                        last_label = true;
                        stmt_start = true;
                        continue;
                    }
                    _ => {}
                }
                if at_start && is_p(t, j + 1, ":") && !STMT_KW.contains(&w.as_str()) {
                    self.c.add("goto label", 2);
                    j += 2;
                    stmt_start = true;
                    continue;
                }
            }
            last_label = false;
            let st = j;
            let mut d = 0i64;
            while let Some(x) = t.get(j) {
                if !self.b.tick() {
                    return;
                }
                match x {
                    Tok::P("(" | "[") => d += 1,
                    Tok::P(")" | "]") => d -= 1,
                    Tok::P("{") if d == 0 => {
                        if j > st && is_p(t, j - 1, "=") {
                            j = matching(t, j, self.b);
                        } else {
                            break;
                        }
                    }
                    Tok::P(";" | "}") if d == 0 => break,
                    _ => {}
                }
                j += 1;
            }
            let s = span(t, st, j);
            if at_start && self.looks_like_decl(s) {
                let b = self.b;
                if self.local_decl(s) > 0
                    && ahead
                        .get_or_insert_with(|| calls_ahead(t, b))
                        .get(j)
                        .copied()
                        .unwrap_or(false)
                {
                    fe.states = true;
                    regions.push((Region::Local, depth));
                }
            } else {
                self.expr_items(s);
            }
        }
        self.eh_function(&fe);
    }

    fn eh_function(&mut self, fe: &FnEh) {
        if !self.gx {
            return;
        }
        let states = fe.states && (fe.has_try || !self.c_fn);
        if states {
            self.c.add("eh function", 1);
        }
        if (states || fe.has_throw) && !self.eh_setup {
            self.eh_setup = true;
            let extra = if fe.has_try {
                1
            } else if fe.has_throw {
                2
            } else {
                0
            };
            self.c.add("eh setup", 5 + extra);
        }
    }

    fn eh_desc(&mut self, key: &str) {
        if self.eh_descs.insert(key.to_string()) {
            let first = i64::from(self.eh_descs.len() == 1);
            self.c.add("eh type", 2 + first);
        }
    }

    fn thrown_type(&mut self, t: &[Tok]) -> String {
        let end = self
            .b
            .it(t)
            .position(|x| *x == Tok::P(";"))
            .unwrap_or(t.len());
        let e = span(t, 0, end);
        let key = match e {
            [Tok::Num] | [Tok::P("-"), Tok::Num] => Some("int".to_string()),
            [Tok::Chr] => Some("char".to_string()),
            [Tok::Str(_)] => Some("char *".to_string()),
            [Tok::P("("), ..] => {
                let c = matching(e, 0, self.b);
                let inner = span(e, 1, c);
                self.is_type_id(inner).then(|| catch_type(inner, self.b))
            }
            [Tok::Id(w)] => self.local_types.get(w).cloned(),
            [Tok::Id(w), Tok::Id(n), ..] if w == "new" => Some(format!("{n} *")),
            [Tok::Id(n), Tok::P("("), ..] if self.classes.contains_key(n) => Some(n.clone()),
            _ => None,
        };
        key.unwrap_or_else(|| {
            self.unmodelled("throw of an unresolved type");
            self.b.it(e).map(tok_text).collect()
        })
    }

    fn cond(&mut self, t: &[Tok], j: usize) -> usize {
        if !is_p(t, j, "(") {
            return j;
        }
        let e = matching(t, j, self.b);
        let inner = span(t, j + 1, e);
        if self.looks_like_decl(inner) {
            self.local_decl(inner);
        } else {
            self.expr_items(inner);
        }
        e + 1
    }

    fn looks_like_decl(&self, s: &[Tok]) -> bool {
        let Some(w) = is_id(s.first()) else {
            return false;
        };
        if STMT_KW.contains(&w) {
            return false;
        }
        if BUILTIN.contains(&w) || QUAL.contains(&w) {
            return true;
        }
        let mut k = 1;
        if self.templates.contains_key(w) && is_p(s, 1, "<") {
            k = angle_close(s, 1, self.b) + 1;
        }
        let k0 = k;
        while is_p(s, k, "::") {
            if !self.b.tick() {
                return false;
            }
            k += 2;
        }
        while is_p(s, k, "*") || is_p(s, k, "&") {
            if !self.b.tick() {
                return false;
            }
            k += 1;
        }
        let ptr = k > k0 && !is_p(s, k0, "::");
        match (is_id(s.get(k)), s.get(k + 1)) {
            (Some(_), None) => true,
            (Some(_), Some(Tok::P(";" | "," | "[" | "("))) => !ptr || self.is_type(w),
            (Some(_), Some(Tok::P("="))) => true,
            _ => false,
        }
    }

    fn local_decl(&mut self, s: &[Tok]) -> usize {
        let is_static = self.b.it(s).any(|x| is_id(Some(x)) == Some("static"));
        self.template_uses(s, true);
        let ty = self
            .b
            .it(s)
            .filter_map(|x| is_id(Some(x)))
            .find(|w| !QUAL.contains(w))
            .unwrap_or("")
            .to_string();
        let dtor_class = self.classes.get(&ty).is_some_and(|c| c.nontrivial_dtor);
        let mut unwind = 0;
        for (n, d) in split_commas(s, self.b).into_iter().enumerate() {
            if d.is_empty() || !self.b.tick() {
                continue;
            }
            let lim = self
                .b
                .it(d)
                .position(|x| matches!(x, Tok::P("=" | "(" | "[")))
                .unwrap_or(d.len());
            let head = span(d, 0, lim);
            let ptr = self.b.it(head).any(|x| matches!(x, Tok::P("*" | "&")));
            if let Some(nm) = self.b.rit(head).find_map(|x| is_id(Some(x))) {
                let full = if ptr { format!("{ty} *") } else { ty.clone() };
                self.local_types.insert(nm.to_string(), full);
            }
            if dtor_class && !ptr && !is_static && (n > 0 || lim > 1) {
                unwind += 1;
            }
            if !ptr && (n > 0 || lim > 1) {
                self.construct(&ty);
            }
            self.c.add(
                if is_static { "static local" } else { "local" },
                if is_static { 2 } else { 1 },
            );
            let eq = self
                .b
                .it(d)
                .position(|x| *x == Tok::P("="))
                .unwrap_or(d.len());
            self.expr_items(from(d, eq));
            if eq == d.len()
                && let Some(p) = self.b.it(d).position(|x| *x == Tok::P("("))
            {
                self.expr_items(from(d, p));
            }
        }
        unwind
    }
}

fn pointer_names(toks: &[Tok], b: &Budget) -> Vec<(usize, String, String)> {
    let mut m = Vec::new();
    let mut j = 0;
    while j + 2 < toks.len() {
        if !b.tick() {
            break;
        }
        if let (Some(ty), true) = (is_id(toks.get(j)), is_p(toks, j + 1, "*")) {
            let mut k = j + 1;
            while (is_p(toks, k, "*") || is_id(toks.get(k)) == Some("const")) && b.tick() {
                k += 1;
            }
            if let Some(n) = is_id(toks.get(k))
                && matches!(toks.get(k + 1), Some(Tok::P(";" | "," | ")" | "[" | "=")))
                && !STMT_KW.contains(&ty)
            {
                m.push((j, n.to_string(), ty.to_string()));
            }
        }
        j += 1;
    }
    m
}

#[must_use]
pub fn live_words(text: &str) -> BTreeSet<String> {
    let mut c = Count::default();
    let live = preprocess(&strip_comments(text), &mut c);
    let mut out = BTreeSet::new();
    let mut w = String::new();
    for ch in live.chars().chain(std::iter::once(' ')) {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            w.push(ch);
        } else if !w.is_empty() {
            if !w.starts_with(|c: char| c.is_ascii_digit()) {
                out.insert(std::mem::take(&mut w));
            }
            w.clear();
        }
    }
    out
}

pub fn count(text: &str) -> Result<Count> {
    count_with(text, true)
}

pub fn count_with(text: &str, gx: bool) -> Result<Count> {
    let mut c = Count::default();
    let live = preprocess(&strip_comments(text), &mut c);
    let chars = live.chars().count();
    let lexb = Budget::per_item(chars, 4, STEP_FLOOR);
    let toks = lex(&live, &lexb);
    if let Some(why) = lexb.over() {
        bail!(
            "the C1 counter model stopped lexing {chars} character(s) at {why} ({} steps)",
            lexb.spent()
        );
    }
    let b = Budget::per_item(toks.len(), STEPS_PER_TOKEN, STEP_FLOOR);
    let ptr_types = pointer_names(&toks, &b);
    c.tokens = toks.len();
    let mut cx = Ctx {
        b: &b,
        c,
        strings: BTreeSet::new(),
        types: BTreeSet::new(),
        classes: BTreeMap::new(),
        templates: BTreeMap::new(),
        overloads: BTreeMap::new(),
        specs: BTreeSet::new(),
        instances: BTreeSet::new(),
        eh_setup: false,
        eh_descs: BTreeSet::new(),
        eh_thrown: BTreeSet::new(),
        eh_rethrow: false,
        local_types: BTreeMap::new(),
        gx,
        extern_c: false,
        c_linkage: BTreeSet::new(),
        c_fn: false,
        ptr_types,
        tok_base: toks.as_ptr() as usize,
        vci: false,
        vdi: false,
        vec_dd: BTreeSet::new(),
        scalar_dd: BTreeSet::new(),
        made_ctor: BTreeSet::new(),
    };
    cx.scope(&toks);
    let mut out = cx.c;
    out.steps = b.spent();
    out.depth = b.deepest();
    if let Some(why) = b.over() {
        bail!(
            "the C1 counter model stopped at {why}: {} token(s), {} step(s), nesting {} of {}",
            toks.len(),
            b.spent(),
            b.deepest(),
            b.max_depth()
        );
    }
    Ok(out)
}
