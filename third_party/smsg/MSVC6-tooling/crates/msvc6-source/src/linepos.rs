use crate::lex::{Cond, Kind, Lexed, Token};
use serde::Serialize;
use std::collections::BTreeSet;

pub const BUILTIN: [&str; 33] = [
    "assert",
    "_ASSERT",
    "_ASSERTE",
    "_ASSERT_EXPR",
    "_ASSERT_BASE",
    "_RPT0",
    "_RPT1",
    "_RPT2",
    "_RPT3",
    "_RPT4",
    "_RPT5",
    "_RPTF0",
    "_RPTF1",
    "_RPTF2",
    "_RPTF3",
    "_RPTF4",
    "_RPTF5",
    "_RPTW0",
    "_RPTW1",
    "_RPTW2",
    "_RPTW3",
    "_RPTW4",
    "_RPTW5",
    "_RPTFW0",
    "_RPTFW1",
    "_RPTFW2",
    "_RPTFW3",
    "_RPTFW4",
    "_RPTFW5",
    "ASSERT",
    "ASSERT_VALID",
    "ASSERT_KINDOF",
    "VERIFY",
];

pub const BUILTIN_OBJECT: [&str; 2] = ["DEBUG_NEW", "THIS_FILE"];

pub const BUILTIN_STRINGIZE: [&str; 4] = ["assert", "_ASSERTE", "_ASSERT_EXPR", "_ASSERT_BASE"];

pub const CRTDBG_ALLOC: [&str; 4] = ["malloc", "calloc", "realloc", "_expand"];

pub const LINE: &str = "__LINE__";

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct MacroSet {
    pub function: BTreeSet<String>,
    pub object: BTreeSet<String>,
    pub stringize: BTreeSet<String>,
    pub any_function: BTreeSet<String>,
}

struct Define {
    name: String,
    function: bool,
    body: Vec<Token>,
}

fn defines(lx: &Lexed) -> Vec<Define> {
    lx.directives
        .iter()
        .filter(|d| d.name == "define" && d.cond != Cond::Dead)
        .filter_map(|d| {
            let args = lx.args(d);
            let name = args.first().filter(|t| t.kind == Kind::Ident)?;
            let function = args
                .get(1)
                .is_some_and(|p| p.is_punct("(") && !p.space_before);
            let skip = if function {
                args.iter()
                    .position(|t| t.is_punct(")"))
                    .map_or(1, |c| c + 1)
            } else {
                1
            };
            Some(Define {
                name: name.text.clone(),
                function,
                body: args.iter().skip(skip).cloned().collect(),
            })
        })
        .collect()
}

impl MacroSet {
    #[must_use]
    pub fn builtin() -> Self {
        let set = |v: &[&str]| v.iter().map(|s| (*s).to_string()).collect::<BTreeSet<_>>();
        Self {
            function: set(&BUILTIN),
            object: set(&BUILTIN_OBJECT),
            stringize: set(&BUILTIN_STRINGIZE),
            any_function: set(&BUILTIN)
                .union(&set(&BUILTIN_STRINGIZE))
                .cloned()
                .collect(),
        }
    }

    pub fn add(&mut self, function: &[String], object: &[String], stringize: &[String]) {
        self.function.extend(function.iter().cloned());
        self.any_function.extend(function.iter().cloned());
        self.object.extend(object.iter().cloned());
        self.stringize.extend(stringize.iter().cloned());
        self.any_function.extend(stringize.iter().cloned());
    }

    fn scan(&self, toks: &[Token]) -> (bool, bool) {
        let (mut line, mut strz) = (false, false);
        for (k, t) in toks.iter().enumerate() {
            let next = toks.get(k + 1);
            if t.is_punct("#") && next.is_some_and(Token::is_ident) {
                strz = true;
            }
            if t.kind != Kind::Ident {
                continue;
            }
            let called = next.is_some_and(|n| n.is_punct("("));
            if t.text == LINE
                || self.object.contains(&t.text)
                || (called && self.function.contains(&t.text))
            {
                line = true;
            }
            if called && self.stringize.contains(&t.text) {
                strz = true;
            }
        }
        (line, strz)
    }

    pub fn harvest<'a>(&mut self, lexed: impl IntoIterator<Item = &'a Lexed>, map_alloc: bool) {
        let mut defs: Vec<Define> = Vec::new();
        let mut alloc = map_alloc;
        for lx in lexed {
            for d in defines(lx) {
                if d.name == "_CRTDBG_MAP_ALLOC" {
                    alloc = true;
                }
                if d.function {
                    self.any_function.insert(d.name.clone());
                }
                defs.push(d);
            }
        }
        if alloc {
            self.function
                .extend(CRTDBG_ALLOC.iter().map(|s| (*s).to_string()));
        }
        loop {
            let mut grew = false;
            for d in &defs {
                let (line, strz) = self.scan(&d.body);
                if line && !self.function.contains(&d.name) && !self.object.contains(&d.name) {
                    if d.function {
                        self.function.insert(d.name.clone());
                    } else {
                        self.object.insert(d.name.clone());
                    }
                    grew = true;
                }
                if strz && d.function && self.stringize.insert(d.name.clone()) {
                    grew = true;
                }
                if let ([one], false) = (d.body.as_slice(), d.function) {
                    if self.function.contains(&one.text) && self.function.insert(d.name.clone()) {
                        grew = true;
                    }
                    if self.stringize.contains(&one.text) && self.stringize.insert(d.name.clone()) {
                        grew = true;
                    }
                    if self.any_function.contains(&one.text)
                        && self.any_function.insert(d.name.clone())
                    {
                        grew = true;
                    }
                }
            }
            if !grew {
                break;
            }
        }
    }

    #[must_use]
    pub fn line_names(&self) -> BTreeSet<String> {
        let mut s: BTreeSet<String> = self.function.union(&self.object).cloned().collect();
        s.insert(LINE.to_string());
        s
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Use {
    pub name: String,
    pub token: usize,
    pub first: usize,
    pub last: usize,
    pub own_last: usize,
    pub line_pos: bool,
    pub brace: Option<usize>,
    pub spelling: String,
    pub cond: Cond,
    pub in_directive: bool,
}

fn closing(toks: &[&Token], open: usize) -> Option<usize> {
    let mut depth = 0usize;
    for (k, t) in toks.iter().enumerate().skip(open) {
        if t.is_punct("(") {
            depth += 1;
        } else if t.is_punct(")") {
            depth = depth.saturating_sub(1);
            if depth == 0 {
                return Some(k);
            }
        }
    }
    None
}

#[must_use]
pub fn function_braces(toks: &[&Token]) -> Vec<Option<usize>> {
    let mut depth = 0usize;
    let mut func: Option<(usize, usize)> = None;
    let mut out = Vec::with_capacity(toks.len());
    let mut paren_close_before = false;
    for (k, t) in toks.iter().enumerate() {
        if t.is_punct("{") {
            let prev = k.checked_sub(1).and_then(|p| toks.get(p));
            let opens = func.is_none()
                && prev.is_some_and(|p| {
                    p.is_punct(")") || p.is("const") || p.is("volatile") || paren_close_before
                });
            if opens {
                func = Some((depth, t.line));
            }
            depth += 1;
            out.push(func.map(|f| f.1));
            paren_close_before = false;
            continue;
        }
        if t.is_punct("}") {
            out.push(func.map(|f| f.1));
            depth = depth.saturating_sub(1);
            if func.is_some_and(|(d, _)| depth <= d) {
                func = None;
            }
            continue;
        }
        if func.is_none() {
            if t.is_punct(";") {
                paren_close_before = false;
            } else if t.is_punct(":")
                && k.checked_sub(1)
                    .and_then(|p| toks.get(p))
                    .is_some_and(|p| p.is_punct(")"))
            {
                paren_close_before = true;
            }
        }
        out.push(func.map(|f| f.1));
    }
    out
}

#[must_use]
pub fn uses(lx: &Lexed, macros: &MacroSet) -> Vec<Use> {
    let local: BTreeSet<String> = defines(lx)
        .into_iter()
        .filter(|d| d.function)
        .map(|d| d.name)
        .collect();
    let code: Vec<usize> = lx.live_code();
    let toks: Vec<&Token> = code.iter().filter_map(|&i| lx.tokens.get(i)).collect();
    let braces = function_braces(&toks);
    let groups: Vec<(usize, usize)> = toks
        .iter()
        .enumerate()
        .filter(|(k, t)| {
            t.is_punct("(")
                && k.checked_sub(1)
                    .and_then(|p| toks.get(p))
                    .is_some_and(|p| p.kind == Kind::Ident)
        })
        .filter_map(|(k, _)| closing(&toks, k).map(|c| (k, c)))
        .collect();
    let spell = |a: usize, b: usize| {
        let mut s = String::new();
        for (j, t) in toks.iter().enumerate().take(b + 1).skip(a) {
            if j > a && t.space_before {
                s.push(' ');
            }
            s.push_str(&t.text);
        }
        s
    };
    let mut out = Vec::new();
    for (k, t) in toks.iter().enumerate() {
        if t.kind != Kind::Ident {
            continue;
        }
        let next_paren = toks.get(k + 1).is_some_and(|n| n.is_punct("("));
        let line_pos = t.text == LINE
            || macros.object.contains(&t.text)
            || (next_paren && macros.function.contains(&t.text));
        let strz = next_paren && macros.stringize.contains(&t.text);
        if !line_pos && !strz {
            continue;
        }
        let own = if next_paren {
            closing(&toks, k + 1).unwrap_or(k)
        } else {
            k
        };
        let macro_close = groups
            .iter()
            .filter(|(o, c)| *o < k && k <= *c)
            .filter(|(o, _)| {
                o.checked_sub(1).and_then(|p| toks.get(p)).is_some_and(|n| {
                    macros.function.contains(&n.text)
                        || macros.any_function.contains(&n.text)
                        || local.contains(&n.text)
                })
            })
            .map(|(_, c)| *c)
            .max();
        let numbered = macro_close.map_or(own, |c| c.max(own));
        let outer = groups
            .iter()
            .filter(|(o, c)| *o < k && k <= *c)
            .map(|(_, c)| *c)
            .max();
        let end = outer.map_or(own, |o| o.max(own));
        out.push(Use {
            name: t.text.clone(),
            token: code.get(k).copied().unwrap_or(0),
            first: t.line,
            last: toks.get(end).map_or(t.line, |e| e.end_line),
            own_last: toks.get(numbered).map_or(t.line, |e| e.end_line),
            line_pos,
            brace: braces.get(k).copied().flatten(),
            spelling: spell(k, end),
            cond: t.cond,
            in_directive: false,
        });
    }
    let names = macros.line_names();
    for d in &lx.directives {
        if d.cond == Cond::Dead || matches!(d.name.as_str(), "define" | "undef") {
            continue;
        }
        if let Some(t) = lx.args(d).iter().find(|t| names.contains(&t.text)) {
            out.push(Use {
                name: t.text.clone(),
                token: d.hash,
                first: t.line,
                last: d.last,
                own_last: t.end_line,
                line_pos: true,
                brace: None,
                spelling: format!("#{}", d.name),
                cond: d.cond,
                in_directive: true,
            });
        }
    }
    out.sort_by_key(|u| u.token);
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Pin {
    pub line: usize,
    pub number: usize,
    pub cond: Cond,
}

#[must_use]
pub fn pins(lx: &Lexed) -> Vec<Pin> {
    lx.directives
        .iter()
        .filter(|d| d.name == "line" && d.cond != Cond::Dead)
        .filter_map(|d| {
            let n: usize = lx.args(d).first()?.text.parse().ok()?;
            Some(Pin {
                line: d.last,
                number: n,
                cond: d.cond,
            })
        })
        .collect()
}

#[must_use]
pub fn compiler_line(pins: &[Pin], line: usize) -> Option<usize> {
    let last = pins
        .iter()
        .filter(|p| p.cond == Cond::Live && p.line < line)
        .max_by_key(|p| p.line);
    let after = last.map_or(0, |p| p.line);
    if pins
        .iter()
        .any(|p| p.cond == Cond::Unknown && p.line > after && p.line < line)
    {
        return None;
    }
    Some(match last {
        Some(p) => p.number + (line - p.line - 1),
        None => line,
    })
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Anchors {
    pub lines: BTreeSet<usize>,
    pub line_directives: BTreeSet<usize>,
}

impl Anchors {
    #[must_use]
    pub fn holds(&self, first: usize) -> bool {
        let end = self
            .line_directives
            .range(first + 1..)
            .next()
            .copied()
            .unwrap_or(usize::MAX);
        self.lines.range(first..end).next().is_some()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.lines.is_empty()
    }
}

#[must_use]
pub fn anchors(lx: &Lexed, macros: &MacroSet) -> Anchors {
    let mut a = Anchors::default();
    for u in uses(lx, macros).iter().filter(|u| u.line_pos) {
        a.lines.insert(u.first);
        a.lines.insert(u.own_last);
        if let Some(b) = u.brace {
            a.lines.insert(b);
        }
    }
    for d in lx
        .directives
        .iter()
        .filter(|d| d.name == "line" && d.cond == Cond::Live)
    {
        a.line_directives.insert(d.last);
    }
    a
}

#[must_use]
pub fn proof(before: &Lexed, after: &Lexed, macros: &MacroSet) -> Vec<String> {
    let (ub, ua) = (uses(before, macros), uses(after, macros));
    let (pb, pa) = (pins(before), pins(after));
    if ub.iter().map(|u| &u.name).ne(ua.iter().map(|u| &u.name)) {
        return vec![format!(
            "the line positions and stringizing uses themselves differ ({} before, {} after)",
            ub.len(),
            ua.len()
        )];
    }
    let mut moved = Vec::new();
    for (b, a) in ub.iter().zip(&ua) {
        let at = format!("`{}` at line {}", b.name, b.first);
        if b.spelling != a.spelling {
            moved.push(format!(
                "{at}: its stringized spelling changes from `{}` to `{}`",
                b.spelling, a.spelling
            ));
            continue;
        }
        if !b.line_pos || b.cond == Cond::Dead {
            continue;
        }
        let ends = |p: &[Pin], u: &Use| [compiler_line(p, u.first), compiler_line(p, u.own_last)];
        let (eb, ea) = (ends(&pb, b), ends(&pa, a));
        if eb.iter().chain(&ea).any(Option::is_none) {
            moved.push(format!(
                "{at}: a conditional `#line` may renumber it, so its number cannot be proven"
            ));
            continue;
        }
        if eb != ea {
            moved.push(format!(
                "{at}: numbered {:?} before and {:?} after",
                eb.iter().flatten().collect::<Vec<_>>(),
                ea.iter().flatten().collect::<Vec<_>>()
            ));
            continue;
        }
        let brace = |p: &[Pin], u: &Use| u.brace.map(|l| compiler_line(p, l));
        if brace(&pb, b) != brace(&pa, a) {
            moved.push(format!(
                "{at}: its function's `{{` moves from line {:?} to {:?}, which /ZI records",
                b.brace, a.brace
            ));
        }
    }
    moved
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lex::lex;

    #[test]
    fn a_function_like_use_is_numbered_by_its_closing_parenthesis() {
        let lx =
            lex(b"void f(int n)\n{\n    assert(n !=\n           0);\n    int l = __LINE__;\n}\n");
        let u = uses(&lx, &MacroSet::builtin());
        assert_eq!(u.len(), 2);
        assert_eq!((u[0].first, u[0].own_last), (3, 4));
        assert_eq!(u[0].spelling, "assert(n != 0)");
        assert_eq!(u[0].brace, Some(2));
        assert_eq!((u[1].name.as_str(), u[1].first), ("__LINE__", 5));
    }

    #[test]
    fn a_use_inside_a_macro_call_is_numbered_by_that_call() {
        let lx = lex(
            b"#define PAIR(a, b) a\nint x = PAIR(__LINE__,\n   1);\nint y = g(__LINE__,\n   1);\n",
        );
        let u = uses(&lx, &MacroSet::builtin());
        assert_eq!(u[0].own_last, 3);
        assert_eq!(u[1].own_last, 4);
    }

    #[test]
    fn the_harvest_follows_definitions_to_a_fixed_point() {
        let hdr = lex(b"#define MY_TRACE(x) _RPT0(0, x)\n#define CHECK assert\n#define WHERE __LINE__\n#define TWICE(x) MY_TRACE(x)\n#define NAME(x) #x\n");
        let mut m = MacroSet::builtin();
        m.harvest([&hdr], false);
        assert!(m.function.contains("MY_TRACE"));
        assert!(m.function.contains("TWICE"));
        assert!(m.function.contains("CHECK"));
        assert!(m.stringize.contains("CHECK"));
        assert!(m.object.contains("WHERE"));
        assert!(m.stringize.contains("NAME"));
        let src = lex(b"void f() {\n CHECK(a&&b);\n}\n");
        let u = uses(&src, &m);
        assert_eq!(u[0].spelling, "CHECK(a&&b)");
    }

    #[test]
    fn line_directives_renumber_and_a_conditional_one_makes_numbers_unknown() {
        let lx = lex(b"int a;\n#line 100\nint b;\n#ifdef X\n#line 500\n#endif\nint c;\n#if 0\n#line 900\n#endif\nint d;\n");
        let p = pins(&lx);
        assert_eq!(compiler_line(&p, 1), Some(1));
        assert_eq!(compiler_line(&p, 3), Some(100));
        assert_eq!(compiler_line(&p, 7), None);
        let lx = lex(b"#line 10\nint a;\n#if 0\n#line 900\n#endif\nint d;\n");
        assert_eq!(compiler_line(&pins(&lx), 6), Some(14));
    }

    #[test]
    fn the_proof_refuses_a_moved_use_and_passes_a_respaced_one() {
        let m = MacroSet::builtin();
        let a = lex(b"void f(int n)\n{\n    assert(n);\n}\n");
        let same = lex(b"void f(int n)\n{\n    assert(n) ;\n}\n");
        assert!(proof(&a, &same, &m).is_empty());
        let moved = lex(b"void f(int n)\n{\n\n    assert(n);\n}\n");
        let why = proof(&a, &moved, &m);
        assert_eq!(why.len(), 1);
        assert!(why[0].contains("numbered"), "{why:?}");
        let respelt = lex(b"void f(int n)\n{\n    assert( n);\n}\n");
        assert!(proof(&a, &respelt, &m)[0].contains("stringized"));
        let brace = lex(b"void f(int n) {\n\n    assert(n);\n}\n");
        assert!(proof(&a, &brace, &m)[0].contains("`{`"));
        let dead = lex(b"#if 0\nassert(x);\n#endif\n");
        assert!(uses(&dead, &m).is_empty());
    }

    #[test]
    fn a_directive_reading_the_line_is_a_use() {
        let lx = lex(b"#if __LINE__ > 3\nint a;\n#endif\n");
        let u = uses(&lx, &MacroSet::builtin());
        assert_eq!(u.len(), 1);
        assert!(u[0].in_directive);
        let a = anchors(&lx, &MacroSet::builtin());
        assert!(a.holds(1));
        assert!(!a.holds(2));
    }

    #[test]
    fn anchors_hold_lines_up_to_the_next_line_directive() {
        let lx = lex(b"int a;\nint b;\n#line 50\nint c = __LINE__;\n");
        let a = anchors(&lx, &MacroSet::builtin());
        assert!(!a.holds(1));
        assert!(a.holds(4));
        let lx = lex(b"int a;\nint b;\nint c = __LINE__;\n");
        assert!(anchors(&lx, &MacroSet::builtin()).holds(1));
    }
}
