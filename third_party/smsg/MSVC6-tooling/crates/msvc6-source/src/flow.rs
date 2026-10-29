use crate::defs::Def;
use crate::lex::{Kind, Lexed};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tok {
    pub kind: Kind,
    pub text: String,
    pub line: usize,
    pub idx: usize,
    pub asm: bool,
}

impl Tok {
    fn is(&self, s: &str) -> bool {
        self.text == s && matches!(self.kind, Kind::Punct | Kind::Ident)
    }

    fn punct(&self, s: &str) -> bool {
        self.kind == Kind::Punct && self.text == s
    }

    fn ident(&self) -> bool {
        self.kind == Kind::Ident
    }
}

fn is(t: Option<&Tok>, s: &str) -> bool {
    t.is_some_and(|t| t.is(s))
}

fn punct(t: Option<&Tok>, s: &str) -> bool {
    t.is_some_and(|t| t.punct(s))
}

#[must_use]
pub fn tokens(lx: &Lexed, from: usize, to: usize) -> Vec<Tok> {
    lx.tokens
        .iter()
        .enumerate()
        .take(to + 1)
        .skip(from)
        .filter(|(_, t)| t.in_code())
        .map(|(i, t)| Tok {
            kind: t.kind,
            text: t.text.clone(),
            line: t.line,
            idx: i,
            asm: t.asm,
        })
        .collect()
}

pub const COMPOUND: [&str; 10] = ["+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="];

pub const EXPR_KEYWORDS: [&str; 9] = [
    "return", "sizeof", "delete", "new", "throw", "case", "goto", "else", "do",
];

pub const STATEMENT_KEYWORDS: [&str; 12] = [
    "if", "while", "for", "do", "switch", "return", "break", "continue", "goto", "case", "default",
    "else",
];

#[must_use]
pub fn is_generic(name: &str) -> bool {
    let Some((prefix, digits)) = name.split_once("Var") else {
        return false;
    };
    !prefix.is_empty()
        && prefix.chars().all(|c| c.is_ascii_lowercase())
        && !digits.is_empty()
        && digits.chars().all(|c| c.is_ascii_digit())
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub enum Track {
    #[default]
    Locals,
    Generic,
    Names(BTreeSet<String>),
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Opts {
    pub track: Track,
    pub out_param: Vec<String>,
    pub write_only: Vec<String>,
    pub short_circuit: bool,
    pub decl_defs: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Declarator {
    pub tok: usize,
    pub name: String,
    pub ty: String,
    pub initialised: bool,
    pub param: bool,
    pub depth: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Decls {
    pub declarators: Vec<Declarator>,
    pub toks: BTreeSet<usize>,
}

impl Decls {
    #[must_use]
    pub fn names(&self) -> BTreeSet<String> {
        self.declarators.iter().map(|d| d.name.clone()).collect()
    }
}

#[derive(Debug, Clone, Default)]
struct Node {
    line: usize,
    var: Option<usize>,
    def: bool,
    maybe: bool,
    tok: usize,
    decl: bool,
    succ: Vec<usize>,
}

#[derive(Debug, Clone, Copy)]
struct Event {
    pos: usize,
    line: usize,
    var: usize,
    def: bool,
    maybe: bool,
    tok: usize,
    decl: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Problem(pub String);

struct Builder<'a> {
    toks: &'a [Tok],
    i: usize,
    nodes: Vec<Node>,
    cur: Vec<usize>,
    breaks: Vec<Vec<usize>>,
    conts: Vec<Vec<usize>>,
    switches: Vec<(usize, bool)>,
    labels: BTreeMap<String, usize>,
    gotos: Vec<(usize, String)>,
    vars: BTreeMap<String, usize>,
    names: Vec<String>,
    opts: &'a Opts,
    decls: Decls,
    locals: BTreeSet<String>,
    depth: usize,
    escaped: BTreeSet<usize>,
}

fn assignment_end(toks: &[Tok], from: usize) -> usize {
    let mut depth = 0i32;
    let mut ternary = 0i32;
    for (k, t) in toks.iter().enumerate().skip(from) {
        if t.kind != Kind::Punct {
            continue;
        }
        match t.text.as_str() {
            "(" | "[" | "{" => depth += 1,
            ")" | "]" | "}" => {
                depth -= 1;
                if depth < 0 {
                    return k;
                }
            }
            "?" if depth == 0 => ternary += 1,
            ":" if depth == 0 => {
                if ternary == 0 {
                    return k;
                }
                ternary -= 1;
            }
            "," | ";" if depth == 0 => return k,
            _ => {}
        }
    }
    toks.len()
}

fn split_top(toks: &[Tok], sep: &str) -> Vec<Vec<Tok>> {
    let mut out = vec![Vec::new()];
    let mut depth = 0i32;
    for t in toks {
        if t.kind == Kind::Punct {
            match t.text.as_str() {
                "(" | "[" | "{" => depth += 1,
                ")" | "]" | "}" => depth -= 1,
                s if s == sep && depth == 0 => {
                    out.push(Vec::new());
                    continue;
                }
                _ => {}
            }
        }
        if let Some(last) = out.last_mut() {
            last.push(t.clone());
        }
    }
    out
}

fn unary_at(toks: &[Tok], k: usize) -> bool {
    match k.checked_sub(1).and_then(|p| toks.get(p)) {
        None => true,
        Some(p) => {
            (p.kind == Kind::Punct && !matches!(p.text.as_str(), ")" | "]"))
                || (p.ident() && EXPR_KEYWORDS.contains(&p.text.as_str()))
        }
    }
}

fn strip_parens(toks: &[Tok]) -> &[Tok] {
    let mut s = toks;
    loop {
        let (Some(f), Some(l)) = (s.first(), s.last()) else {
            return s;
        };
        if !(f.punct("(") && l.punct(")")) {
            return s;
        }
        let mut depth = 0i32;
        let mut whole = true;
        for (k, t) in s.iter().enumerate() {
            if t.punct("(") {
                depth += 1;
            } else if t.punct(")") {
                depth -= 1;
                if depth == 0 && k + 1 < s.len() {
                    whole = false;
                    break;
                }
            }
        }
        if !whole {
            return s;
        }
        s = s.get(1..s.len() - 1).unwrap_or_default();
    }
}

fn has_top(toks: &[Tok], ops: &[&str]) -> Option<usize> {
    let mut depth = 0i32;
    let mut found = None;
    for (k, t) in toks.iter().enumerate() {
        if t.kind != Kind::Punct {
            continue;
        }
        match t.text.as_str() {
            "(" | "[" | "{" => depth += 1,
            ")" | "]" | "}" => depth -= 1,
            s if depth == 0 && ops.contains(&s) => {
                if found.is_none() {
                    found = Some(k);
                }
            }
            "?" | "=" if depth == 0 => return None,
            s if depth == 0 && COMPOUND.contains(&s) => return None,
            _ => {}
        }
    }
    found
}

impl<'a> Builder<'a> {
    fn new(toks: &'a [Tok], opts: &'a Opts, locals: BTreeSet<String>) -> Self {
        let mut b = Self {
            toks,
            i: 0,
            nodes: Vec::new(),
            cur: Vec::new(),
            breaks: Vec::new(),
            conts: Vec::new(),
            switches: Vec::new(),
            labels: BTreeMap::new(),
            gotos: Vec::new(),
            vars: BTreeMap::new(),
            names: Vec::new(),
            opts,
            decls: Decls::default(),
            locals,
            depth: 0,
            escaped: BTreeSet::new(),
        };
        b.nodes.push(Node::default());
        b.cur = vec![0];
        b
    }

    fn tracked(&self, name: &str) -> bool {
        match &self.opts.track {
            Track::Locals => self.locals.contains(name),
            Track::Generic => is_generic(name),
            Track::Names(n) => n.contains(name),
        }
    }

    fn var(&mut self, name: &str) -> usize {
        if let Some(v) = self.vars.get(name) {
            return *v;
        }
        let v = self.names.len();
        self.vars.insert(name.to_string(), v);
        self.names.push(name.to_string());
        v
    }

    fn edge(&mut self, from: usize, to: usize) {
        if let Some(n) = self.nodes.get_mut(from)
            && !n.succ.contains(&to)
        {
            n.succ.push(to);
        }
    }

    fn add(&mut self, n: Node) -> usize {
        let id = self.nodes.len();
        self.nodes.push(n);
        for p in std::mem::take(&mut self.cur) {
            self.edge(p, id);
        }
        self.cur = vec![id];
        id
    }

    fn empty(&mut self, line: usize) -> usize {
        self.add(Node {
            line,
            tok: usize::MAX,
            ..Node::default()
        })
    }

    fn peek(&self) -> Option<&'a Tok> {
        self.toks.get(self.i)
    }

    fn line(&self) -> usize {
        self.peek()
            .or_else(|| self.toks.last())
            .map_or(0, |t| t.line)
    }

    fn expect(&mut self, s: &str) -> Result<(), Problem> {
        if is(self.peek(), s) {
            self.i += 1;
            Ok(())
        } else {
            Err(Problem(format!(
                "line {}: expected `{s}`, found `{}`",
                self.line(),
                self.peek()
                    .map_or("the end of the body", |t| t.text.as_str())
            )))
        }
    }

    fn until_semicolon(&mut self) -> Result<Vec<Tok>, Problem> {
        let mut out = Vec::new();
        let mut depth = 0i32;
        let start = self.line();
        while let Some(t) = self.peek() {
            if t.kind == Kind::Punct {
                match t.text.as_str() {
                    "(" | "[" | "{" => depth += 1,
                    ")" | "]" | "}" => {
                        depth -= 1;
                        if depth < 0 && t.text == "}" {
                            return Ok(out);
                        }
                    }
                    ";" if depth == 0 => {
                        self.i += 1;
                        return Ok(out);
                    }
                    _ => {}
                }
            }
            self.i += 1;
            out.push(t.clone());
        }
        Err(Problem(format!(
            "line {start}: a statement runs to the end of the body"
        )))
    }

    fn paren(&mut self) -> Result<Vec<Tok>, Problem> {
        self.expect("(")?;
        let mut out = Vec::new();
        let mut depth = 1i32;
        while let Some(t) = self.peek() {
            self.i += 1;
            if t.punct("(") {
                depth += 1;
            } else if t.punct(")") {
                depth -= 1;
                if depth == 0 {
                    return Ok(out);
                }
            }
            out.push(t.clone());
        }
        Err(Problem(format!("line {}: an unbalanced `(`", self.line())))
    }

    fn callee_of(toks: &[Tok], k: usize) -> Option<String> {
        let mut depth = 0i32;
        for j in (0..k).rev() {
            let x = toks.get(j)?;
            if x.kind != Kind::Punct {
                continue;
            }
            match x.text.as_str() {
                ")" | "]" => depth += 1,
                "(" | "[" if depth > 0 => depth -= 1,
                "(" => {
                    return j
                        .checked_sub(1)
                        .and_then(|p| toks.get(p))
                        .filter(|p| p.ident())
                        .map(|p| p.text.clone());
                }
                _ => {}
            }
        }
        None
    }

    fn scan(&mut self, toks: &[Tok], offset: usize) -> Vec<Event> {
        let mut out = Vec::new();
        for (k, t) in toks.iter().enumerate() {
            if !t.ident() || !self.tracked(&t.text) {
                continue;
            }
            let prev = k.checked_sub(1).and_then(|p| toks.get(p));
            if prev.is_some_and(|p| p.punct(".") || p.punct("->") || p.punct("::")) {
                continue;
            }
            let v = self.var(&t.text);
            let next = toks.get(k + 1);
            let shorted = toks.get(..k).is_some_and(|b| {
                b.iter()
                    .any(|x| x.punct("?") || x.punct("&&") || x.punct("||"))
            });
            let at = |pos: usize, def: bool, maybe: bool| Event {
                pos: offset + pos,
                line: t.line,
                var: v,
                def,
                maybe: def && (maybe || shorted),
                tok: t.idx,
                decl: false,
            };
            if t.asm {
                out.push(at(k, false, false));
                out.push(at(k, true, true));
                continue;
            }
            let prefix = |s: &str| punct(prev, s) && unary_at(toks, k - 1);
            if k > 0 && prefix("&") {
                self.escaped.insert(v);
                let callee = Self::callee_of(toks, k - 1);
                let always = callee
                    .as_ref()
                    .is_some_and(|c| self.opts.out_param.contains(c));
                let no_read = always
                    || callee
                        .as_ref()
                        .is_some_and(|c| self.opts.write_only.contains(c));
                if !no_read {
                    out.push(at(k, false, false));
                }
                out.push(at(k, true, !always));
            } else if k > 0 && prefix("*") {
                out.push(at(k, false, false));
            } else if punct(next, "=") {
                out.push(at(assignment_end(toks, k + 2), true, false));
            } else if next
                .is_some_and(|n| n.kind == Kind::Punct && COMPOUND.contains(&n.text.as_str()))
            {
                out.push(at(k, false, false));
                out.push(at(assignment_end(toks, k + 2), true, false));
            } else if punct(next, "++")
                || punct(next, "--")
                || punct(prev, "++")
                || punct(prev, "--")
            {
                out.push(at(k, false, false));
                out.push(at(k, true, false));
            } else {
                out.push(at(k, false, false));
            }
        }
        out
    }

    fn emit(&mut self, mut events: Vec<Event>, line: usize) -> (usize, usize) {
        events.sort_by_key(|e| (e.pos, e.def));
        let mut first = None;
        let mut last = None;
        let mut open = false;
        if events.first().is_some_and(|e| e.maybe) {
            let n = self.empty(line);
            first = Some(n);
            last = Some(n);
        }
        for e in events {
            let bypass = self.cur.clone();
            let n = self.add(Node {
                line: e.line,
                var: Some(e.var),
                def: e.def,
                maybe: e.maybe,
                tok: e.tok,
                decl: e.decl,
                succ: Vec::new(),
            });
            open = e.maybe;
            if open {
                self.cur.extend(bypass);
            }
            first.get_or_insert(n);
            last = Some(n);
        }
        if open {
            last = Some(self.empty(line));
        }
        match (first, last) {
            (Some(f), Some(l)) => (f, l),
            _ => {
                let n = self.empty(line);
                (n, n)
            }
        }
    }

    fn expr(&mut self, toks: &[Tok], line: usize) -> (usize, usize) {
        let ev = self.scan(toks, 0);
        self.emit(ev, line)
    }

    fn is_declaration(&self, toks: &[Tok]) -> bool {
        let Some(first) = toks.first() else {
            return false;
        };
        if !first.ident()
            || EXPR_KEYWORDS.contains(&first.text.as_str())
            || STATEMENT_KEYWORDS.contains(&first.text.as_str())
            || self.locals_declared(&first.text)
        {
            return false;
        }
        let mut prev: Option<&Tok> = None;
        for t in toks {
            match t.kind {
                Kind::Ident => {
                    if let Some(p) = prev
                        && (p.ident() || p.punct("*") || p.punct("&") || p.punct(">"))
                    {
                        return true;
                    }
                }
                Kind::Punct if t.punct("::") || t.punct("*") || t.punct("&") => {}
                Kind::Punct if t.punct("<") || t.punct(">") || t.punct(",") => {}
                _ => return false,
            }
            prev = Some(t);
        }
        false
    }

    fn locals_declared(&self, name: &str) -> bool {
        self.decls.declarators.iter().any(|d| d.name == name)
    }

    fn declaration(&mut self, toks: &[Tok], line: usize, semicolon: Option<usize>) {
        let mut events = Vec::new();
        let mut type_text = String::new();
        let mut offset = 0usize;
        let storage = toks
            .first()
            .is_some_and(|t| matches!(t.text.as_str(), "static" | "extern" | "typedef"));
        for (n, d) in split_top(toks, ",").iter().enumerate() {
            let base = offset;
            offset += d.len() + 1;
            let stop = d
                .iter()
                .position(|t| t.punct("=") || t.punct("(") || t.punct("["))
                .unwrap_or(d.len());
            let head = d.get(..stop).unwrap_or_default();
            let Some(name_at) = head.iter().rposition(Tok::ident) else {
                continue;
            };
            if n == 0 {
                type_text = head
                    .get(..name_at)
                    .unwrap_or_default()
                    .iter()
                    .map(|t| t.text.as_str())
                    .collect::<Vec<_>>()
                    .join(" ")
                    .replace(" *", "*");
            }
            let Some(name) = head.get(name_at) else {
                continue;
            };
            let rest = d.get(stop..).unwrap_or_default();
            let initialised = rest.first().is_some_and(|t| t.punct("=") || t.punct("("))
                || rest.iter().any(|t| t.punct("="));
            if !storage {
                let stars = head
                    .get(..name_at)
                    .unwrap_or_default()
                    .iter()
                    .rev()
                    .take_while(|t| t.punct("*"))
                    .count();
                let ty = format!("{}{}", type_text.trim_end_matches('*'), "*".repeat(stars));
                self.decls.declarators.push(Declarator {
                    tok: name.idx,
                    name: name.text.clone(),
                    ty,
                    initialised,
                    param: false,
                    depth: self.depth,
                });
                self.locals.insert(name.text.clone());
                self.decls.toks.extend(head.iter().map(|t| t.idx));
                if rest.first().is_some_and(|t| t.punct("[")) {
                    let close = rest.iter().position(|t| t.punct("]")).unwrap_or(0);
                    self.decls
                        .toks
                        .extend(rest.get(..=close).unwrap_or_default().iter().map(|t| t.idx));
                }
            }
            events.extend(self.scan(rest, base + stop));
            if self.tracked(&name.text) && !storage {
                let v = self.var(&name.text);
                if initialised || self.opts.decl_defs {
                    events.push(Event {
                        pos: base + d.len(),
                        line: name.line,
                        var: v,
                        def: true,
                        maybe: false,
                        tok: name.idx,
                        decl: !initialised,
                    });
                }
            }
        }
        if !storage {
            let mut depth = 0i32;
            for t in toks {
                if t.kind != Kind::Punct {
                    continue;
                }
                match t.text.as_str() {
                    "(" | "[" | "{" => depth += 1,
                    ")" | "]" | "}" => depth -= 1,
                    "," if depth == 0 => {
                        self.decls.toks.insert(t.idx);
                    }
                    _ => {}
                }
            }
            if let Some(s) = semicolon {
                self.decls.toks.insert(s);
            }
        }
        self.emit(events, line);
    }

    fn cond(&mut self, toks: &[Tok], line: usize) -> (Vec<usize>, Vec<usize>) {
        if self.opts.short_circuit {
            let s = strip_parens(toks);
            if let Some((bang, rest)) = s.split_first()
                && bang.punct("!")
                && strip_parens(rest).len() + 2 == rest.len()
                && !rest.is_empty()
            {
                let (t, f) = self.cond(rest, line);
                return (f, t);
            }
            if let Some(at) = has_top(s, &["||"]) {
                let (l, r) = (
                    s.get(..at).unwrap_or_default(),
                    s.get(at + 1..).unwrap_or_default(),
                );
                let (lt, lf) = self.cond(l, line);
                self.cur = lf;
                let (rt, rf) = self.cond(r, line);
                return ([lt, rt].concat(), rf);
            }
            if let Some(at) = has_top(s, &["&&"]) {
                let (l, r) = (
                    s.get(..at).unwrap_or_default(),
                    s.get(at + 1..).unwrap_or_default(),
                );
                let (lt, lf) = self.cond(l, line);
                self.cur = lt;
                let (rt, rf) = self.cond(r, line);
                return (rt, [lf, rf].concat());
            }
        }
        let (_, last) = self.expr(toks, line);
        (vec![last], vec![last])
    }

    fn block_or_stmt(&mut self) -> Result<(), Problem> {
        self.stmt()
    }

    fn stmt(&mut self) -> Result<(), Problem> {
        let Some(t) = self.peek() else {
            return Err(Problem(
                "a statement expected at the end of the body".into(),
            ));
        };
        let line = t.line;
        if t.asm {
            let mut toks = Vec::new();
            while let Some(a) = self.peek().filter(|a| a.asm) {
                self.i += 1;
                toks.push(a.clone());
            }
            self.expr(&toks, line);
            return Ok(());
        }
        if t.is("__asm") || t.is("_asm") {
            self.i += 1;
            let mut toks = Vec::new();
            if punct(self.peek(), "{") {
                self.i += 1;
                let mut depth = 1i32;
                while let Some(a) = self.peek() {
                    self.i += 1;
                    if a.punct("{") {
                        depth += 1;
                    } else if a.punct("}") {
                        depth -= 1;
                        if depth == 0 {
                            break;
                        }
                    }
                    toks.push(Tok {
                        asm: true,
                        ..a.clone()
                    });
                }
            } else {
                while let Some(a) = self.peek().filter(|a| a.asm && a.line == line) {
                    self.i += 1;
                    toks.push(a.clone());
                }
            }
            let toks: Vec<Tok> = toks.into_iter().map(|t| Tok { asm: true, ..t }).collect();
            self.expr(&toks, line);
            return Ok(());
        }
        if t.punct("{") {
            self.i += 1;
            self.depth += 1;
            while !punct(self.peek(), "}") {
                if self.peek().is_none() {
                    return Err(Problem("an unclosed block".into()));
                }
                self.stmt()?;
            }
            self.i += 1;
            self.depth = self.depth.saturating_sub(1);
            return Ok(());
        }
        if t.punct(";") {
            self.i += 1;
            return Ok(());
        }
        match t.text.as_str() {
            "if" if t.ident() => {
                self.i += 1;
                let c = self.paren()?;
                let (tr, fa) = self.cond(&c, line);
                self.cur = tr;
                self.block_or_stmt()?;
                let after_then = std::mem::take(&mut self.cur);
                if is(self.peek(), "else") {
                    self.i += 1;
                    self.cur = fa;
                    self.block_or_stmt()?;
                    self.cur.extend(after_then);
                } else {
                    self.cur = [after_then, fa].concat();
                }
                return Ok(());
            }
            "while" if t.ident() => {
                self.i += 1;
                let head = self.empty(line);
                let c = self.paren()?;
                let (tr, fa) = self.cond(&c, line);
                self.cur = tr;
                self.loop_body(head, fa)?;
                return Ok(());
            }
            "do" if t.ident() => {
                self.i += 1;
                let head = self.empty(line);
                self.breaks.push(Vec::new());
                self.conts.push(Vec::new());
                self.block_or_stmt()?;
                let conts = self.conts.pop().unwrap_or_default();
                self.cur.extend(conts);
                self.expect("while")?;
                let c = self.paren()?;
                let (tr, fa) = self.cond(&c, line);
                for p in tr {
                    self.edge(p, head);
                }
                self.expect(";")?;
                let breaks = self.breaks.pop().unwrap_or_default();
                self.cur = [fa, breaks].concat();
                return Ok(());
            }
            "for" if t.ident() => {
                self.i += 1;
                let inner = self.paren()?;
                let parts = split_top(&inner, ";");
                let (init, cond, step) = (
                    parts.first().cloned().unwrap_or_default(),
                    parts.get(1).cloned().unwrap_or_default(),
                    parts.get(2).cloned().unwrap_or_default(),
                );
                self.depth += 1;
                if self.is_declaration(&init) {
                    self.declaration(&init, line, None);
                } else if !init.is_empty() {
                    self.expr(&init, line);
                }
                let head = self.empty(line);
                let (tr, fa) = if cond.is_empty() {
                    (vec![head], Vec::new())
                } else {
                    self.cond(&cond, line)
                };
                self.cur = tr;
                self.breaks.push(Vec::new());
                self.conts.push(Vec::new());
                self.block_or_stmt()?;
                let conts = self.conts.pop().unwrap_or_default();
                self.cur.extend(conts);
                if !step.is_empty() {
                    self.expr(&step, line);
                }
                for p in std::mem::take(&mut self.cur) {
                    self.edge(p, head);
                }
                let breaks = self.breaks.pop().unwrap_or_default();
                self.cur = [fa, breaks].concat();
                self.depth = self.depth.saturating_sub(1);
                return Ok(());
            }
            "switch" if t.ident() => {
                self.i += 1;
                let c = self.paren()?;
                let (_, head) = self.expr(&c, line);
                self.cur = Vec::new();
                self.switches.push((head, false));
                self.breaks.push(Vec::new());
                self.block_or_stmt()?;
                let breaks = self.breaks.pop().unwrap_or_default();
                let (head, has_default) = self.switches.pop().unwrap_or((head, false));
                self.cur.extend(breaks);
                if !has_default {
                    self.cur.push(head);
                }
                return Ok(());
            }
            "case" | "default" if t.ident() => {
                self.i += 1;
                let mut depth = 0i32;
                let mut ternary = 0i32;
                while let Some(x) = self.peek() {
                    self.i += 1;
                    if x.punct("(") {
                        depth += 1;
                    } else if x.punct(")") {
                        depth -= 1;
                    } else if x.punct("?") {
                        ternary += 1;
                    } else if x.punct(":") && depth == 0 {
                        if ternary == 0 {
                            break;
                        }
                        ternary -= 1;
                    }
                }
                let n = self.empty(line);
                if let Some(sw) = self.switches.last_mut() {
                    if t.text == "default" {
                        sw.1 = true;
                    }
                    let h = sw.0;
                    self.edge(h, n);
                }
                return Ok(());
            }
            "break" if t.ident() => {
                self.i += 1;
                self.expect(";")?;
                let cur = std::mem::take(&mut self.cur);
                if let Some(b) = self.breaks.last_mut() {
                    b.extend(cur);
                }
                return Ok(());
            }
            "continue" if t.ident() => {
                self.i += 1;
                self.expect(";")?;
                let cur = std::mem::take(&mut self.cur);
                if let Some(c) = self.conts.last_mut() {
                    c.extend(cur);
                }
                return Ok(());
            }
            "return" if t.ident() => {
                self.i += 1;
                let e = self.until_semicolon()?;
                self.expr(&e, line);
                self.cur = Vec::new();
                return Ok(());
            }
            "goto" if t.ident() => {
                self.i += 1;
                let e = self.until_semicolon()?;
                let label = e.first().map(|l| l.text.clone()).unwrap_or_default();
                let n = self.empty(line);
                self.gotos.push((n, label));
                self.cur = Vec::new();
                return Ok(());
            }
            "try" | "__try" if t.ident() => {
                self.i += 1;
                let entry = self.empty(line);
                let first_node = self.nodes.len();
                self.block_or_stmt()?;
                let inside: Vec<usize> = (first_node..self.nodes.len()).collect();
                let mut ends = std::mem::take(&mut self.cur);
                while is(self.peek(), "catch")
                    || is(self.peek(), "__except")
                    || is(self.peek(), "__finally")
                {
                    let kw = self.peek().map(|k| k.text.clone()).unwrap_or_default();
                    self.i += 1;
                    self.cur = [vec![entry], inside.clone(), ends.clone()].concat();
                    if kw != "__finally" {
                        let p = self.paren()?;
                        if kw == "__except" {
                            self.expr(&p, line);
                        }
                    }
                    self.block_or_stmt()?;
                    ends.extend(std::mem::take(&mut self.cur));
                }
                self.cur = ends;
                return Ok(());
            }
            _ => {}
        }
        if t.ident()
            && punct(self.toks.get(self.i + 1), ":")
            && !punct(self.toks.get(self.i + 2), ":")
        {
            self.i += 2;
            let n = self.empty(line);
            self.labels.insert(t.text.clone(), n);
            return Ok(());
        }
        let start = self.i;
        let toks = self.until_semicolon()?;
        let semicolon = self
            .toks
            .get(self.i.wrapping_sub(1))
            .filter(|s| s.punct(";"))
            .map(|s| s.idx);
        if self.is_declaration(&toks) {
            self.declaration(&toks, line, semicolon);
        } else {
            let _ = start;
            self.expr(&toks, line);
        }
        Ok(())
    }

    fn loop_body(&mut self, head: usize, exits: Vec<usize>) -> Result<(), Problem> {
        self.breaks.push(Vec::new());
        self.conts.push(Vec::new());
        self.block_or_stmt()?;
        let conts = self.conts.pop().unwrap_or_default();
        for p in [std::mem::take(&mut self.cur), conts].concat() {
            self.edge(p, head);
        }
        let breaks = self.breaks.pop().unwrap_or_default();
        self.cur = [exits, breaks].concat();
        Ok(())
    }

    fn finish(&mut self) -> Result<(), Problem> {
        for (from, label) in std::mem::take(&mut self.gotos) {
            let Some(&to) = self.labels.get(&label) else {
                return Err(Problem(format!(
                    "`goto {label}` names no label in the body"
                )));
            };
            self.edge(from, to);
        }
        let exit = self.empty(self.line());
        let _ = exit;
        Ok(())
    }
}

#[must_use]
pub fn params(lx: &Lexed, def: &Def) -> Vec<(Tok, String)> {
    let toks = tokens(lx, def.params.0 + 1, def.params.1.saturating_sub(1));
    let mut out = Vec::new();
    for p in split_top(&toks, ",") {
        let stop = p
            .iter()
            .position(|t| t.punct("=") || t.punct("["))
            .unwrap_or(p.len());
        let head = p.get(..stop).unwrap_or_default();
        if head.len() < 2 {
            continue;
        }
        let Some(at) = head.iter().rposition(Tok::ident) else {
            continue;
        };
        let Some(name) = head.get(at) else {
            continue;
        };
        if at == 0 || name.text == "void" {
            continue;
        }
        let ty = head
            .get(..at)
            .unwrap_or_default()
            .iter()
            .map(|t| t.text.as_str())
            .collect::<Vec<_>>()
            .join(" ")
            .replace(" *", "*")
            .replace(" &", "&");
        out.push((name.clone(), ty));
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Use {
    pub var: String,
    pub tok: usize,
    pub line: usize,
    pub reached: BTreeSet<Option<usize>>,
    pub real: BTreeSet<Option<usize>>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Write {
    pub var: String,
    pub tok: usize,
    pub line: usize,
    pub maybe: bool,
    pub decl: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Web {
    pub var: String,
    pub defs: Vec<(usize, usize)>,
    pub uses: Vec<(usize, usize)>,
    pub from_entry: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Flow {
    pub uses: Vec<Use>,
    pub writes: Vec<Write>,
    pub webs: Vec<Web>,
    pub decls: Decls,
    pub params: Vec<(Tok, String)>,
    pub escaped: BTreeSet<String>,
    pub nodes: usize,
}

fn solve(nodes: &[Node], nvars: usize) -> Vec<BTreeMap<usize, BTreeSet<usize>>> {
    let mut preds: Vec<Vec<usize>> = vec![Vec::new(); nodes.len()];
    for (i, n) in nodes.iter().enumerate() {
        for &s in &n.succ {
            if let Some(p) = preds.get_mut(s) {
                p.push(i);
            }
        }
    }
    let mut result: Vec<BTreeMap<usize, BTreeSet<usize>>> = vec![BTreeMap::new(); nodes.len()];
    for v in 0..nvars {
        let defs: Vec<usize> = nodes
            .iter()
            .enumerate()
            .filter(|(_, n)| n.def && n.var == Some(v))
            .map(|(i, _)| i)
            .collect();
        let mut slot: BTreeMap<usize, usize> = BTreeMap::new();
        slot.insert(0, 0);
        for (k, d) in defs.iter().enumerate() {
            slot.insert(*d, k + 1);
        }
        let width = defs.len() + 1;
        let words = width.div_ceil(64);
        let mut out: Vec<Vec<u64>> = vec![vec![0; words]; nodes.len()];
        let set = |b: &mut Vec<u64>, bit: usize| {
            if let Some(w) = b.get_mut(bit / 64) {
                *w |= 1u64 << (bit % 64);
            }
        };
        let transfer = |i: usize, inp: &[u64]| -> Vec<u64> {
            let mut o = inp.to_vec();
            let Some(n) = nodes.get(i) else {
                return o;
            };
            if i == 0 {
                o = vec![0; words];
                set(&mut o, 0);
                return o;
            }
            if n.def && n.var == Some(v) {
                if !n.maybe {
                    o = vec![0; words];
                }
                if let Some(&b) = slot.get(&i) {
                    set(&mut o, b);
                }
            }
            o
        };
        let mut work: Vec<usize> = (0..nodes.len()).rev().collect();
        let mut queued = vec![true; nodes.len()];
        while let Some(i) = work.pop() {
            if let Some(q) = queued.get_mut(i) {
                *q = false;
            }
            let mut inp = vec![0u64; words];
            for &p in preds.get(i).into_iter().flatten() {
                for (a, b) in inp.iter_mut().zip(out.get(p).into_iter().flatten()) {
                    *a |= *b;
                }
            }
            let o = transfer(i, &inp);
            let changed = out.get(i).is_none_or(|old| *old != o);
            if changed {
                if let Some(slot_out) = out.get_mut(i) {
                    *slot_out = o;
                }
                for &s in nodes.get(i).map(|n| n.succ.as_slice()).unwrap_or_default() {
                    if queued.get(s) == Some(&false) {
                        if let Some(q) = queued.get_mut(s) {
                            *q = true;
                        }
                        work.push(s);
                    }
                }
            }
        }
        for (i, n) in nodes.iter().enumerate() {
            if n.def || n.var != Some(v) {
                continue;
            }
            let mut inp = vec![0u64; words];
            for &p in preds.get(i).into_iter().flatten() {
                for (a, b) in inp.iter_mut().zip(out.get(p).into_iter().flatten()) {
                    *a |= *b;
                }
            }
            let mut reached = BTreeSet::new();
            for (d, &b) in &slot {
                if inp.get(b / 64).is_some_and(|w| w & (1u64 << (b % 64)) != 0) {
                    reached.insert(*d);
                }
            }
            if let Some(r) = result.get_mut(i) {
                r.insert(v, reached);
            }
        }
    }
    result
}

fn find(parent: &mut [usize], x: usize) -> usize {
    let mut r = x;
    while let Some(&p) = parent.get(r) {
        if p == r {
            break;
        }
        r = p;
    }
    let mut c = x;
    while let Some(&p) = parent.get(c) {
        if p == r {
            break;
        }
        if let Some(slot) = parent.get_mut(c) {
            *slot = r;
        }
        c = p;
    }
    r
}

pub fn analyse(lx: &Lexed, def: &Def, opts: &Opts) -> Result<Flow, Problem> {
    let ps = params(lx, def);
    let toks = tokens(lx, def.open, def.close);
    let locals: BTreeSet<String> = ps.iter().map(|(t, _)| t.text.clone()).collect();
    let mut pre = Decls::default();
    for (t, ty) in &ps {
        pre.declarators.push(Declarator {
            tok: t.idx,
            name: t.text.clone(),
            ty: ty.clone(),
            initialised: true,
            param: true,
            depth: 0,
        });
    }
    let mut b = Builder::new(&toks, opts, locals);
    b.decls.declarators.extend(pre.declarators);
    b.stmt()?;
    if b.i < toks.len() {
        return Err(Problem(format!(
            "line {}: the body parses short of its closing brace",
            b.line()
        )));
    }
    b.finish()?;
    let nvars = b.names.len();
    let mut entry_defs: BTreeMap<usize, usize> = BTreeMap::new();
    for (t, _) in &ps {
        if b.tracked(&t.text) {
            let v = b.var(&t.text);
            entry_defs.insert(v, t.idx);
        }
    }
    let nvars = nvars.max(b.names.len());
    let reach = solve(&b.nodes, nvars);
    let tok_of = |node: usize, v: usize| -> Option<usize> {
        if node == 0 {
            return entry_defs.get(&v).copied();
        }
        b.nodes.get(node).map(|n| n.tok)
    };
    let mut uses = Vec::new();
    let mut parent: Vec<usize> = (0..b.nodes.len()).collect();
    for (i, n) in b.nodes.iter().enumerate() {
        let Some(v) = n.var.filter(|_| !n.def) else {
            continue;
        };
        let reached = reach
            .get(i)
            .and_then(|r| r.get(&v))
            .cloned()
            .unwrap_or_default();
        for &d in &reached {
            let (a, c) = (find(&mut parent, i), find(&mut parent, d));
            if let Some(slot) = parent.get_mut(a) {
                *slot = c;
            }
        }
        let real: BTreeSet<Option<usize>> = reached
            .iter()
            .filter(|&&d| d == 0 || !b.nodes.get(d).is_some_and(|x| x.decl))
            .map(|&d| if d == 0 { tok_of(0, v) } else { tok_of(d, v) })
            .collect();
        uses.push(Use {
            var: b.names.get(v).cloned().unwrap_or_default(),
            tok: n.tok,
            line: n.line,
            reached: reached
                .iter()
                .map(|&d| if d == 0 { tok_of(0, v) } else { tok_of(d, v) })
                .collect(),
            real,
        });
    }
    let writes: Vec<Write> = b
        .nodes
        .iter()
        .filter(|n| n.def)
        .filter_map(|n| {
            Some(Write {
                var: b.names.get(n.var?)?.clone(),
                tok: n.tok,
                line: n.line,
                maybe: n.maybe,
                decl: n.decl,
            })
        })
        .collect();
    let mut groups: BTreeMap<(usize, usize), Web> = BTreeMap::new();
    for (i, n) in b.nodes.iter().enumerate() {
        let Some(v) = n.var else {
            continue;
        };
        let root = find(&mut parent, i);
        let w = groups.entry((v, root)).or_insert_with(|| Web {
            var: b.names.get(v).cloned().unwrap_or_default(),
            defs: Vec::new(),
            uses: Vec::new(),
            from_entry: false,
        });
        if n.def {
            w.defs.push((n.line, n.tok));
        } else {
            w.uses.push((n.line, n.tok));
            if reach
                .get(i)
                .and_then(|r| r.get(&v))
                .is_some_and(|s| s.contains(&0))
            {
                w.from_entry = true;
            }
        }
    }
    let mut webs: Vec<Web> = groups.into_values().collect();
    webs.sort_by(|a, b| {
        a.var.cmp(&b.var).then(
            a.defs
                .first()
                .or(a.uses.first())
                .cmp(&b.defs.first().or(b.uses.first())),
        )
    });
    Ok(Flow {
        uses,
        writes,
        webs,
        decls: b.decls.clone(),
        params: ps,
        escaped: b
            .escaped
            .iter()
            .filter_map(|&v| b.names.get(v).cloned())
            .collect(),
        nodes: b.nodes.len(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::defs::{definitions, one};
    use crate::lex::lex;

    fn webs(src: &str, f: &str, var: &str) -> Vec<Web> {
        let lx = lex(src.as_bytes());
        let defs = definitions(&lx);
        let d = one(&defs, f).unwrap();
        let fl = analyse(
            &lx,
            d,
            &Opts {
                track: Track::Generic,
                short_circuit: true,
                ..Opts::default()
            },
        )
        .unwrap();
        fl.webs.into_iter().filter(|w| w.var == var).collect()
    }

    #[test]
    fn unrelated_values_in_one_generic_local_are_separate_webs() {
        let src = "int f(int n)\n{\n    int iVar1;\n    iVar1 = g();\n    h(iVar1);\n    for (iVar1 = 0; iVar1 < n; iVar1++) {\n        k(iVar1);\n    }\n    iVar1 = m();\n    return iVar1;\n}\n";
        let w = webs(src, "f", "iVar1");
        assert_eq!(w.len(), 3, "{w:?}");
        assert!(w.iter().all(|w| !w.from_entry));
    }

    #[test]
    fn an_if_else_join_is_one_web() {
        let src = "int f(int n)\n{\n    int iVar1;\n    if (n) iVar1 = 1; else iVar1 = 2;\n    return iVar1;\n}\n";
        assert_eq!(webs(src, "f", "iVar1").len(), 1);
    }

    #[test]
    fn a_read_before_any_write_comes_from_the_entry() {
        let src = "int f()\n{\n    int iVar1;\n    return iVar1;\n}\n";
        let w = webs(src, "f", "iVar1");
        assert!(w[0].from_entry);
    }

    #[test]
    fn gotos_switches_and_loops_carry_values() {
        let src = "int f(int n)\n{\n    int iVar1 = 0;\nagain:\n    switch (n) {\n    case 1:\n        iVar1 = 5;\n        break;\n    default:\n        n--;\n        if (n) goto again;\n    }\n    do { iVar1++; } while (iVar1 < 3);\n    return iVar1;\n}\n";
        let w = webs(src, "f", "iVar1");
        assert_eq!(w.len(), 1, "{w:?}");
    }

    #[test]
    fn a_write_after_a_short_circuit_is_exact_and_an_address_merges() {
        let src = "int f(int a)\n{\n    int iVar1 = 0;\n    if (a && (iVar1 = g()) != 0) {\n        h(iVar1);\n    }\n    iVar1 = 7;\n    p(&iVar1);\n    return iVar1;\n}\n";
        let lx = lex(src.as_bytes());
        let defs = definitions(&lx);
        let fl = analyse(
            &lx,
            one(&defs, "f").unwrap(),
            &Opts {
                track: Track::Generic,
                short_circuit: true,
                ..Opts::default()
            },
        )
        .unwrap();
        let h = fl.uses.iter().find(|u| u.line == 5).unwrap();
        assert_eq!(h.reached.len(), 1);
        assert!(fl.escaped.contains("iVar1"));
        let last = fl.uses.iter().find(|u| u.line == 9).unwrap();
        assert_eq!(last.reached.len(), 2);
    }

    #[test]
    fn locals_and_parameters_are_tracked_with_their_types() {
        let src = "unsigned short Cls::Get(int n, char *p)\n{\n    unsigned short v, *pv = 0;\n    int arr[4];\n    v = n;\n    return v + *p;\n}\n";
        let lx = lex(src.as_bytes());
        let defs = definitions(&lx);
        let fl = analyse(&lx, one(&defs, "Cls::Get").unwrap(), &Opts::default()).unwrap();
        let ty = |n: &str| {
            fl.decls
                .declarators
                .iter()
                .find(|d| d.name == n)
                .map(|d| d.ty.clone())
                .unwrap()
        };
        assert_eq!(ty("v"), "unsigned short");
        assert_eq!(ty("pv"), "unsigned short*");
        assert_eq!(ty("arr"), "int");
        assert_eq!(ty("p"), "char*");
        assert!(
            fl.decls
                .declarators
                .iter()
                .any(|d| d.name == "n" && d.param)
        );
        let n_use = fl.uses.iter().find(|u| u.var == "n").unwrap();
        assert_eq!(n_use.reached.len(), 1);
        assert!(n_use.reached.iter().all(Option::is_some));
    }

    #[test]
    fn inline_assembler_reads_and_may_write_what_it_names() {
        let src = "int f(int a)\n{\n    int x = a;\n    __asm {\n        mov eax, x\n        mov x, eax\n    }\n    return x;\n}\n";
        let lx = lex(src.as_bytes());
        let defs = definitions(&lx);
        let fl = analyse(&lx, one(&defs, "f").unwrap(), &Opts::default()).unwrap();
        let last = fl.uses.iter().rfind(|u| u.var == "x").unwrap();
        assert_eq!(last.line, 8);
        assert!(last.reached.len() >= 2);
    }
}
