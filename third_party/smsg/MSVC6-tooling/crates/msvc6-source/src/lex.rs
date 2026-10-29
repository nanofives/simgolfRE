use crate::asmmacro::{Def, Path, Table};
use msvc6_base::srcio;
use std::collections::BTreeSet;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, serde::Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Kind {
    Ident,
    Number,
    Str,
    Chr,
    Header,
    Punct,
    Text,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, serde::Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Cond {
    Live,
    Unknown,
    Dead,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Token {
    pub kind: Kind,
    pub text: String,
    pub start: usize,
    pub end: usize,
    pub line: usize,
    pub end_line: usize,
    pub space_before: bool,
    pub first_on_line: bool,
    pub directive: Option<usize>,
    pub cond: Cond,
    pub asm: bool,
}

impl Token {
    #[must_use]
    pub fn is(&self, s: &str) -> bool {
        self.text == s && matches!(self.kind, Kind::Punct | Kind::Ident)
    }

    #[must_use]
    pub fn is_punct(&self, s: &str) -> bool {
        self.kind == Kind::Punct && self.text == s
    }

    #[must_use]
    pub fn is_ident(&self) -> bool {
        self.kind == Kind::Ident
    }

    #[must_use]
    pub fn in_code(&self) -> bool {
        self.directive.is_none() && self.cond != Cond::Dead
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, serde::Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum CommentKind {
    Line,
    Block,
    Asm,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Comment {
    pub kind: CommentKind,
    pub start: usize,
    pub end: usize,
    pub first: usize,
    pub last: usize,
    pub directive: bool,
    pub whole_line: bool,
    pub closed: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Directive {
    pub name: String,
    pub first: usize,
    pub last: usize,
    pub start: usize,
    pub end: usize,
    pub hash: usize,
    pub args: std::ops::Range<usize>,
    pub cond: Cond,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Problem {
    pub line: usize,
    pub what: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Lexed {
    pub tokens: Vec<Token>,
    pub comments: Vec<Comment>,
    pub directives: Vec<Directive>,
    pub problems: Vec<Problem>,
    pub trigraphs: Vec<usize>,
    pub splices: Vec<usize>,
    pub nuls: Vec<usize>,
    pub lines: usize,
}

impl Lexed {
    #[must_use]
    pub fn args(&self, d: &Directive) -> &[Token] {
        self.tokens.get(d.args.clone()).unwrap_or_default()
    }

    #[must_use]
    pub fn code(&self) -> Vec<&Token> {
        self.tokens.iter().filter(|t| t.in_code()).collect()
    }

    #[must_use]
    pub fn live_code(&self) -> Vec<usize> {
        self.tokens
            .iter()
            .enumerate()
            .filter(|(_, t)| t.in_code())
            .map(|(i, _)| i)
            .collect()
    }
}

#[derive(Debug, Clone, Copy)]
struct Ch {
    b: u8,
    start: usize,
    end: usize,
    line: usize,
}

#[must_use]
pub fn trigraph(c: u8) -> Option<u8> {
    Some(match c {
        b'=' => b'#',
        b'(' => b'[',
        b'/' => b'\\',
        b')' => b']',
        b'\'' => b'^',
        b'<' => b'{',
        b'!' => b'|',
        b'>' => b'}',
        b'-' => b'~',
        _ => return None,
    })
}

fn phases(
    src: &[u8],
    trigraphs: &mut Vec<usize>,
    splices: &mut Vec<usize>,
    nuls: &mut Vec<usize>,
) -> Vec<Ch> {
    let mut pos: Vec<usize> = Vec::with_capacity(src.len());
    for (i, &b) in src.iter().enumerate() {
        if b == 0 {
            nuls.push(i);
        } else {
            pos.push(i);
        }
    }
    let byte = |k: usize| pos.get(k).and_then(|&p| src.get(p)).copied();
    let raw_end = |k: usize| pos.get(k).map_or(src.len(), |&p| p + 1);
    let newline_after = |k: usize| match (byte(k), byte(k + 1)) {
        (Some(b'\n'), _) => Some(1),
        (Some(b'\r'), Some(b'\n')) => Some(2),
        _ => None,
    };
    let mut out = Vec::with_capacity(pos.len());
    let (mut k, mut line) = (0usize, 1usize);
    while let Some(raw) = byte(k) {
        let start = pos.get(k).copied().unwrap_or(0);
        let tri = (raw == b'?' && byte(k + 1) == Some(b'?'))
            .then(|| byte(k + 2).and_then(trigraph))
            .flatten();
        let (b, len) = tri.map_or((raw, 1), |t| (t, 3));
        if tri.is_some() {
            trigraphs.push(start);
        }
        if let Some(n) = (b == b'\\').then(|| newline_after(k + len)).flatten() {
            splices.push(start);
            k += len + n;
            line += 1;
            continue;
        }
        out.push(Ch {
            b,
            start,
            end: raw_end(k + len - 1),
            line,
        });
        if b == b'\n' {
            line += 1;
        }
        k += len;
    }
    out
}

fn raw_trigraph_splice(raw: &[u8]) -> bool {
    let kept: Vec<u8> = raw.iter().copied().filter(|&b| b != 0).collect();
    kept.windows(3).any(|w| w == b"??/")
}

const PUNCTS: [&[u8]; 27] = [
    b">>=", b"<<=", b"...", b"->*", b"->", b"++", b"--", b"<<", b">>", b"<=", b">=", b"==", b"!=",
    b"&&", b"||", b"+=", b"-=", b"*=", b"/=", b"%=", b"&=", b"|=", b"^=", b"::", b"##", b".*",
    b"#@",
];

pub const ASM_KEYWORDS: [&str; 2] = ["__asm", "_asm"];

#[must_use]
pub fn ident_start(b: u8) -> bool {
    b.is_ascii_alphabetic() || b == b'_' || b == b'$'
}

#[must_use]
pub fn ident_byte(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_' || b == b'$'
}

fn blank(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\r' | 0x0b | 0x0c)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Asm {
    Off,
    Pending,
    Line,
    Block,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Branch {
    Taken,
    Skipped,
    Unknown,
}

#[derive(Debug, Clone)]
struct Group {
    id: usize,
    branch: usize,
    now: Branch,
    any_taken: bool,
    any_unknown: bool,
    dead_parent: bool,
    saved: Asm,
    ends: Vec<(Branch, Asm)>,
    has_else: bool,
    line: usize,
}

#[derive(Debug, Clone)]
struct Call {
    name: String,
    line: usize,
    path: Path,
    start: Asm,
    open: bool,
    depth: usize,
    first: usize,
}

struct Lexer<'a> {
    v: Vec<Ch>,
    src: &'a [u8],
    out: Lexed,
    groups: Vec<Group>,
    dir: Option<usize>,
    asm: Asm,
    macros: Table,
    call: Option<Call>,
    reported: BTreeSet<String>,
    next_group: usize,
    dir_spills: bool,
}

fn constant(toks: &[Token]) -> Option<bool> {
    let inner: Vec<&Token> = toks
        .iter()
        .filter(|t| !t.is_punct("(") && !t.is_punct(")"))
        .collect();
    match inner.as_slice() {
        [t] if t.kind == Kind::Number => {
            let digits = t.text.trim_end_matches(['u', 'U', 'l', 'L']);
            let zero = digits.bytes().all(|b| b == b'0')
                || digits
                    .strip_prefix("0x")
                    .or_else(|| digits.strip_prefix("0X"))
                    .is_some_and(|h| !h.is_empty() && h.bytes().all(|b| b == b'0'));
            let number = digits.bytes().all(|b| b.is_ascii_digit())
                || digits
                    .strip_prefix("0x")
                    .or_else(|| digits.strip_prefix("0X"))
                    .is_some_and(|h| !h.is_empty() && h.bytes().all(|b| b.is_ascii_hexdigit()));
            number.then_some(!zero)
        }
        _ => None,
    }
}

impl Lexer<'_> {
    fn at(&self, k: usize) -> Option<u8> {
        self.v.get(k).map(|c| c.b)
    }

    fn cond(&self) -> Cond {
        if self
            .groups
            .iter()
            .any(|g| g.dead_parent || g.now == Branch::Skipped)
        {
            Cond::Dead
        } else if self.groups.iter().any(|g| g.now == Branch::Unknown) {
            Cond::Unknown
        } else {
            Cond::Live
        }
    }

    fn line_end(&self, k: usize) -> usize {
        let e = self.v.get(k).map_or(self.src.len(), |c| c.start);
        if e > 0 && self.src.get(e - 1) == Some(&b'\r') {
            e - 1
        } else {
            e
        }
    }

    fn finish_directive(&mut self, end: usize) {
        let Some(di) = self.dir.take() else {
            return;
        };
        let Some(d) = self.out.directives.get_mut(di) else {
            return;
        };
        d.end = end;
        d.args.end = self.out.tokens.len();
        if let Some(t) = self
            .out
            .tokens
            .get(d.args.start..d.args.end)
            .and_then(<[Token]>::last)
        {
            d.last = d.last.max(t.end_line);
        }
        let name = d.name.clone();
        let first = d.first;
        let d_cond = d.cond;
        let d_spills = std::mem::take(&mut self.dir_spills);
        let args: Vec<Token> = self
            .out
            .tokens
            .get(d.args.clone())
            .unwrap_or_default()
            .to_vec();
        let dead_now = self.cond() == Cond::Dead;
        match name.as_str() {
            "if" | "ifdef" | "ifndef" => {
                let now = if name == "if" {
                    match constant(&args) {
                        Some(true) => Branch::Taken,
                        Some(false) => Branch::Skipped,
                        None => Branch::Unknown,
                    }
                } else {
                    Branch::Unknown
                };
                let id = self.next_group;
                self.next_group += 1;
                self.groups.push(Group {
                    id,
                    branch: 0,
                    now,
                    any_taken: now == Branch::Taken,
                    any_unknown: now == Branch::Unknown,
                    dead_parent: dead_now,
                    saved: self.asm,
                    ends: Vec::new(),
                    has_else: false,
                    line: first,
                });
            }
            "elif" | "else" => {
                let asm = self.asm;
                if let Some(g) = self.groups.last_mut() {
                    g.ends.push((g.now, asm));
                    g.branch += 1;
                    let value = if name == "else" {
                        Some(true)
                    } else {
                        constant(&args)
                    };
                    g.has_else |= name == "else";
                    g.now = if g.any_taken {
                        Branch::Skipped
                    } else {
                        match (value, g.any_unknown) {
                            (Some(true), false) => Branch::Taken,
                            (Some(false), _) => Branch::Skipped,
                            _ => Branch::Unknown,
                        }
                    };
                    g.any_taken |= g.now == Branch::Taken;
                    g.any_unknown |= g.now == Branch::Unknown;
                    self.asm = g.saved;
                } else {
                    self.unmatched(&name, first);
                }
            }
            "endif" => {
                if let Some(mut g) = self.groups.pop() {
                    g.ends.push((g.now, self.asm));
                    if !g.has_else {
                        let implicit = if g.any_taken {
                            Branch::Skipped
                        } else if g.any_unknown {
                            Branch::Unknown
                        } else {
                            Branch::Taken
                        };
                        g.ends.push((implicit, g.saved));
                    }
                    self.asm = self.join_asm(&g, first);
                } else {
                    self.unmatched(&name, first);
                }
            }
            "define" if !dead_now => {
                if let Some((m, rest)) = args.split_first().filter(|(m, _)| m.is_ident()) {
                    let call = rest
                        .first()
                        .is_some_and(|t| t.is_punct("(") && !t.space_before);
                    let close = if call {
                        rest.iter()
                            .position(|t| t.is_punct(")"))
                            .map_or(rest.len(), |p| p + 1)
                    } else {
                        0
                    };
                    let params = call.then(|| {
                        rest.get(..close)
                            .unwrap_or_default()
                            .iter()
                            .filter(|t| t.is_ident())
                            .map(|t| t.text.clone())
                            .collect()
                    });
                    let body = rest
                        .get(close..)
                        .unwrap_or_default()
                        .iter()
                        .map(|t| t.text.clone())
                        .collect();
                    let path = self.path();
                    self.macros
                        .define(&m.text, Some(Def { params, body }), path);
                }
            }
            "undef" if !dead_now => {
                if let Some(m) = args.first() {
                    let path = self.path();
                    self.macros.define(&m.text, None, path);
                }
            }
            "error" | "warning" if d_cond == Cond::Unknown && d_spills => {
                self.out.problems.push(Problem {
                    line: first,
                    what: format!(
                        "the #{name} in a group the macros decide opens a block comment its line does not close: CL \
                         prints the text raw when the group is taken and reads a comment when it is not"
                    ),
                });
            }
            _ => {}
        }
    }

    fn closes_at(&self, j: usize) -> bool {
        match (self.v.get(j), self.v.get(j + 1)) {
            (Some(s), Some(e)) => {
                s.b == b'*'
                    && e.b == b'/'
                    && !raw_trigraph_splice(self.src.get(s.end..e.start).unwrap_or_default())
            }
            _ => false,
        }
    }

    fn spills(&self, from: usize, line_end: usize) -> bool {
        let mut j = from;
        while j < line_end {
            match (self.at(j), self.at(j + 1)) {
                (Some(b'/'), Some(b'/')) => return false,
                (Some(b'/'), Some(b'*')) => {
                    let mut c = j + 2;
                    while !self.closes_at(c) {
                        if c >= self.v.len() {
                            return true;
                        }
                        c += 1;
                    }
                    if c + 1 >= line_end {
                        return true;
                    }
                    j = c + 2;
                }
                (Some(q @ (b'"' | b'\'')), _) => j = self.literal_end(j, q).0,
                _ => j += 1,
            }
        }
        false
    }

    fn path(&self) -> Path {
        self.groups
            .iter()
            .filter(|g| g.now == Branch::Unknown)
            .map(|g| (g.id, g.branch))
            .collect()
    }

    fn in_call(&self) -> bool {
        self.call.as_ref().is_some_and(|c| c.open)
    }

    fn macro_problem(&mut self, name: &str, line: usize, why: &str) {
        if self.reported.insert(name.to_string()) {
            self.out.problems.push(Problem {
                line,
                what: why.to_string(),
            });
        }
    }

    fn use_macro(&mut self, name: &str, line: usize) {
        let path = self.path();
        if self.macros.takes_arguments(name, &path) {
            self.call = Some(Call {
                name: name.to_string(),
                line,
                path,
                start: self.asm,
                open: false,
                depth: 0,
                first: 0,
            });
            return;
        }
        match self.macros.run(&[name.to_string()], self.asm, &path) {
            Ok(s) => self.asm = s,
            Err(why) => self.macro_problem(name, line, &why),
        }
    }

    fn uncalled(&mut self) {
        if let Some(c) = self.call.take() {
            match self
                .macros
                .run(std::slice::from_ref(&c.name), c.start, &c.path)
            {
                Ok(s) => self.asm = s,
                Err(why) => self.macro_problem(&c.name, c.line, &why),
            }
        }
    }

    fn end_call(&mut self, c: &Call) {
        let lexed = self.asm;
        let mut toks = vec![c.name.clone(), "(".to_string()];
        toks.extend(
            self.out
                .tokens
                .get(c.first..)
                .unwrap_or_default()
                .iter()
                .map(|t| t.text.clone()),
        );
        toks.push(")".to_string());
        match self.macros.run(&toks, c.start, &c.path) {
            Ok(s) if lexed == c.start || lexed == s => self.asm = s,
            Ok(_) => self.macro_problem(
                &c.name,
                c.line,
                &format!(
                    "the arguments of `{}` hold `__asm`, which the lexer reads in place and the expansion places \
                     otherwise",
                    c.name
                ),
            ),
            Err(why) => self.macro_problem(&c.name, c.line, &why),
        }
    }

    fn unmatched(&mut self, name: &str, line: usize) {
        self.out.problems.push(Problem {
            line,
            what: format!("an #{name} with no open #if"),
        });
    }

    fn join_asm(&mut self, g: &Group, endif: usize) -> Asm {
        if g.dead_parent {
            return g.saved;
        }
        if let Some((_, a)) = g.ends.iter().find(|(b, _)| *b == Branch::Taken) {
            return *a;
        }
        let states: Vec<Asm> = g
            .ends
            .iter()
            .filter(|(b, _)| *b == Branch::Unknown)
            .map(|(_, a)| *a)
            .collect();
        match states.split_first() {
            None => g.saved,
            Some((first, rest)) if rest.iter().all(|a| a == first) => *first,
            Some(_) => {
                self.out.problems.push(Problem {
                    line: endif,
                    what: format!(
                        "the conditional group of lines {}-{endif} leaves `__asm` open in one branch and not in \
                         another: which text is assembler depends on the macros",
                        g.line
                    ),
                });
                g.saved
            }
        }
    }

    fn push(&mut self, kind: Kind, from: usize, to: usize, space: bool, bol: bool) {
        let text: Vec<u8> = self
            .v
            .get(from..to)
            .unwrap_or_default()
            .iter()
            .map(|c| c.b)
            .collect();
        let (start, line) = self.v.get(from).map_or((0, 0), |c| (c.start, c.line));
        let (end, end_line) = to
            .checked_sub(1)
            .and_then(|l| self.v.get(l))
            .map_or((start, line), |c| (c.end, c.line));
        let cond = match self.dir.and_then(|d| self.out.directives.get(d)) {
            Some(d) => d.cond,
            None => self.cond(),
        };
        self.out.tokens.push(Token {
            kind,
            text: srcio::decode(&text),
            start,
            end,
            line,
            end_line,
            space_before: space,
            first_on_line: bol,
            directive: self.dir,
            cond,
            asm: matches!(self.asm, Asm::Line | Asm::Block),
        });
    }

    fn literal_end(&self, k: usize, q: u8) -> (usize, bool) {
        let mut j = k + 1;
        while let Some(b) = self.at(j) {
            if b == b'\n' {
                return (j, false);
            }
            j += 1;
            if b == b'\\' {
                if self.at(j).is_some_and(|e| e != b'\n') {
                    j += 1;
                }
            } else if b == q {
                return (j, true);
            }
        }
        (j, false)
    }

    fn comment(&mut self, kind: CommentKind, k: usize, block: bool, bol: bool) -> usize {
        let (start, first) = self.v.get(k).map_or((0, 0), |c| (c.start, c.line));
        let mut last = first;
        let mut j = k + if kind == CommentKind::Asm { 1 } else { 2 };
        let mut closed = true;
        let end = if block {
            closed = false;
            let mut end = self.src.len();
            while let Some(c) = self.v.get(j).copied() {
                if self.closes_at(j) {
                    if let Some(e) = self.v.get(j + 1) {
                        end = e.end;
                        last = e.line;
                    }
                    j += 2;
                    closed = true;
                    break;
                }
                last = c.line;
                j += 1;
            }
            if !closed {
                last = self.v.last().map_or(first, |c| c.line);
                self.out.problems.push(Problem {
                    line: first,
                    what: "a block comment that never closes".to_string(),
                });
            }
            end
        } else {
            while let Some(c) = self.v.get(j).filter(|c| c.b != b'\n') {
                last = c.line;
                j += 1;
            }
            self.line_end(j)
        };
        if let Some(d) = self.dir.and_then(|d| self.out.directives.get_mut(d)) {
            d.last = d.last.max(last);
        }
        self.out.comments.push(Comment {
            kind,
            start,
            end,
            first,
            last,
            directive: self.dir.is_some(),
            whole_line: bol,
            closed,
        });
        j
    }

    fn run(&mut self) {
        let (mut k, mut bol, mut space) = (0usize, true, true);
        let mut header_next = false;
        while let Some(c) = self.v.get(k).copied() {
            match c.b {
                b'\n' => {
                    self.finish_directive(self.line_end(k));
                    if self.asm == Asm::Line {
                        self.asm = Asm::Off;
                    }
                    bol = true;
                    space = true;
                    header_next = false;
                    k += 1;
                    continue;
                }
                b if blank(b) => {
                    space = true;
                    k += 1;
                    continue;
                }
                b'/' if matches!(self.at(k + 1), Some(b'/' | b'*')) => {
                    let block = self.at(k + 1) == Some(b'*');
                    let kind = if block {
                        CommentKind::Block
                    } else {
                        CommentKind::Line
                    };
                    k = self.comment(kind, k, block, bol);
                    space = true;
                    continue;
                }
                b';' if self.dir.is_none()
                    && matches!(self.asm, Asm::Line | Asm::Block)
                    && self.cond() != Cond::Dead =>
                {
                    if let Some(name) = self
                        .call
                        .as_ref()
                        .filter(|c| c.open)
                        .map(|c| c.name.clone())
                    {
                        self.macro_problem(
                            &name,
                            c.line,
                            &format!(
                                "a `;` inside the arguments of `{name}` while `__asm` is open"
                            ),
                        );
                    }
                    k = self.comment(CommentKind::Asm, k, false, bol);
                    space = true;
                    continue;
                }
                _ => {}
            }
            let first = bol;
            bol = false;
            if self.dir.is_none() && self.call.as_ref().is_some_and(|c| !c.open) && c.b != b'(' {
                self.uncalled();
            }
            if c.b == b'#'
                && first
                && self.dir.is_none()
                && let Some(name) = self
                    .call
                    .as_ref()
                    .filter(|c| c.open)
                    .map(|c| c.name.clone())
            {
                self.macro_problem(
                    &name,
                    c.line,
                    &format!("a directive inside the arguments of `{name}`"),
                );
            }
            if c.b == b'#' && first && self.dir.is_none() {
                let cond = self.cond();
                let di = self.out.directives.len();
                self.out.directives.push(Directive {
                    name: String::new(),
                    first: c.line,
                    last: c.line,
                    start: c.start,
                    end: c.end,
                    hash: self.out.tokens.len(),
                    args: self.out.tokens.len() + 1..self.out.tokens.len() + 1,
                    cond,
                });
                self.dir = Some(di);
                self.push(Kind::Punct, k, k + 1, space, first);
                k += 1;
                let mut j = k;
                while self.at(j).is_some_and(blank) {
                    j += 1;
                }
                if self.at(j).is_some_and(ident_start) {
                    let s = j;
                    while self.at(j).is_some_and(ident_byte) {
                        j += 1;
                    }
                    let name = srcio::decode(
                        &self
                            .v
                            .get(s..j)
                            .unwrap_or_default()
                            .iter()
                            .map(|c| c.b)
                            .collect::<Vec<u8>>(),
                    );
                    header_next = matches!(name.as_str(), "include" | "import");
                    let raw_text =
                        matches!(name.as_str(), "error" | "warning") && cond != Cond::Dead;
                    let sp = j > k;
                    self.push(Kind::Ident, s, j, sp, false);
                    if let Some(d) = self.out.directives.get_mut(di) {
                        d.name = name;
                        d.args = self.out.tokens.len()..self.out.tokens.len();
                    }
                    k = j;
                    space = false;
                    if raw_text {
                        while self.at(k).is_some_and(blank) {
                            k += 1;
                            space = true;
                        }
                        let mut e = k;
                        while self.at(e).is_some_and(|b| b != b'\n') {
                            e += 1;
                        }
                        if cond == Cond::Unknown {
                            self.dir_spills = self.spills(k, e);
                        }
                        let mut t = e;
                        while t > k && self.at(t - 1).is_some_and(blank) {
                            t -= 1;
                        }
                        if t > k {
                            self.push(Kind::Text, k, t, space, false);
                        }
                        k = e;
                    }
                }
                continue;
            }
            if header_next && c.b == b'<' {
                let mut j = k + 1;
                while self.at(j).is_some_and(|b| b != b'>' && b != b'\n') {
                    j += 1;
                }
                if self.at(j) == Some(b'>') {
                    j += 1;
                }
                self.push(Kind::Header, k, j, space, first);
                header_next = false;
                k = j;
                space = false;
                continue;
            }
            header_next = false;
            if c.b == b'"' || c.b == b'\'' {
                let (j, closed) = self.literal_end(k, c.b);
                if !closed && self.dir.is_none() && self.cond() != Cond::Dead {
                    self.out.problems.push(Problem {
                        line: c.line,
                        what: format!(
                            "a {} literal that its line ends",
                            if c.b == b'"' { "string" } else { "character" }
                        ),
                    });
                }
                if self.asm == Asm::Pending && self.cond() != Cond::Dead {
                    self.asm = Asm::Line;
                }
                let kind = if c.b == b'"' { Kind::Str } else { Kind::Chr };
                self.push(kind, k, j, space, first);
                k = j;
                space = false;
                continue;
            }
            if ident_start(c.b) {
                let mut j = k;
                while self.at(j).is_some_and(ident_byte) {
                    j += 1;
                }
                let word: Vec<u8> = self
                    .v
                    .get(k..j)
                    .unwrap_or_default()
                    .iter()
                    .map(|c| c.b)
                    .collect();
                if word == b"L" && matches!(self.at(j), Some(b'"' | b'\'')) {
                    let q = self.at(j).unwrap_or(b'"');
                    let (e, _) = self.literal_end(j, q);
                    let kind = if q == b'"' { Kind::Str } else { Kind::Chr };
                    self.push(kind, k, e, space, first);
                    k = e;
                    space = false;
                    continue;
                }
                let code = self.dir.is_none() && self.cond() != Cond::Dead;
                let name = srcio::decode(&word);
                if code && !self.in_call() && self.macros.is_macro(&name) {
                    self.use_macro(&name, c.line);
                } else if code {
                    if self.asm != Asm::Block
                        && ASM_KEYWORDS.iter().any(|a| a.as_bytes() == word.as_slice())
                    {
                        self.asm = Asm::Pending;
                    } else if self.asm == Asm::Pending {
                        self.asm = Asm::Line;
                    }
                }
                self.push(Kind::Ident, k, j, space, first);
                k = j;
                space = false;
                continue;
            }
            if c.b.is_ascii_digit()
                || (c.b == b'.' && self.at(k + 1).is_some_and(|b| b.is_ascii_digit()))
            {
                let mut j = k + 1;
                while let Some(b) = self.at(j) {
                    if matches!(b, b'+' | b'-')
                        && self.at(j - 1).is_some_and(|p| matches!(p, b'e' | b'E'))
                    {
                        j += 1;
                        continue;
                    }
                    if ident_byte(b) || b == b'.' {
                        j += 1;
                        continue;
                    }
                    break;
                }
                if self.asm == Asm::Pending && self.cond() != Cond::Dead {
                    self.asm = Asm::Line;
                }
                self.push(Kind::Number, k, j, space, first);
                k = j;
                space = false;
                continue;
            }
            let len = PUNCTS
                .iter()
                .find(|p| {
                    p.iter()
                        .enumerate()
                        .all(|(o, &b)| self.at(k + o) == Some(b))
                })
                .map_or(1, |p| p.len());
            if self.dir.is_none() && self.cond() != Cond::Dead {
                match (self.asm, c.b) {
                    (Asm::Pending, b'{') => self.asm = Asm::Block,
                    (Asm::Pending, _) => self.asm = Asm::Line,
                    (Asm::Line | Asm::Block, b'}') => self.asm = Asm::Off,
                    _ => {}
                }
            }
            if self.dir.is_none()
                && let Some(mut call) = self.call.take()
            {
                match (call.open, c.b) {
                    (false, b'(') => {
                        call.open = true;
                        call.depth = 1;
                        call.first = self.out.tokens.len() + 1;
                        self.call = Some(call);
                    }
                    (true, b'(') => {
                        call.depth += 1;
                        self.call = Some(call);
                    }
                    (true, b')') if call.depth <= 1 => self.end_call(&call),
                    (true, b')') => {
                        call.depth -= 1;
                        self.call = Some(call);
                    }
                    _ => self.call = Some(call),
                }
            }
            self.push(Kind::Punct, k, k + len, space, first);
            k += len;
            space = false;
        }
        if let Some(name) = self
            .call
            .as_ref()
            .filter(|c| c.open)
            .map(|c| c.name.clone())
        {
            let line = self.v.last().map_or(1, |c| c.line);
            self.macro_problem(&name, line, &format!("the call of `{name}` never closes"));
        }
        self.uncalled();
        self.finish_directive(self.src.len());
        if !self.groups.is_empty() {
            self.out.problems.push(Problem {
                line: self.v.last().map_or(1, |c| c.line),
                what: format!("{} conditional group(s) never closed", self.groups.len()),
            });
        }
    }
}

#[must_use]
pub fn lex(src: &[u8]) -> Lexed {
    let mut trigraphs = Vec::new();
    let mut splices = Vec::new();
    let mut nuls = Vec::new();
    let v = phases(src, &mut trigraphs, &mut splices, &mut nuls);
    let lines = src.iter().filter(|&&b| b == b'\n').count()
        + usize::from(!src.is_empty() && src.last() != Some(&b'\n'));
    let mut lx = Lexer {
        v,
        src,
        out: Lexed {
            trigraphs,
            splices,
            nuls,
            lines,
            ..Lexed::default()
        },
        groups: Vec::new(),
        dir: None,
        asm: Asm::Off,
        macros: Table::default(),
        call: None,
        reported: BTreeSet::new(),
        next_group: 0,
        dir_spills: false,
    };
    lx.run();
    lx.out
}

#[must_use]
pub fn logical(src: &[u8]) -> Vec<(u8, usize, usize)> {
    let (mut t, mut s, mut n) = (Vec::new(), Vec::new(), Vec::new());
    phases(src, &mut t, &mut s, &mut n)
        .into_iter()
        .map(|c| (c.b, c.start, c.line))
        .collect()
}

#[must_use]
pub fn physical_lines(src: &[u8]) -> Vec<(usize, usize, usize)> {
    let mut out = Vec::new();
    let mut s = 0usize;
    for (i, &b) in src.iter().enumerate() {
        if b == b'\n' {
            let body = if i > s && src.get(i - 1) == Some(&b'\r') {
                i - 1
            } else {
                i
            };
            out.push((s, body, i + 1));
            s = i + 1;
        }
    }
    if s < src.len() {
        out.push((s, src.len(), src.len()));
    }
    out
}

#[must_use]
pub fn line_of(src: &[u8], off: usize) -> usize {
    src.get(..off.min(src.len()))
        .unwrap_or_default()
        .iter()
        .filter(|&&b| b == b'\n')
        .count()
        + 1
}

#[must_use]
pub fn number_value(text: &str) -> Option<i128> {
    let t = text.trim_end_matches(['u', 'U', 'l', 'L']);
    let t = t
        .strip_suffix("i64")
        .or_else(|| t.strip_suffix("I64"))
        .or_else(|| t.strip_suffix("i32"))
        .or_else(|| t.strip_suffix("i16"))
        .or_else(|| t.strip_suffix("i8"))
        .unwrap_or(t);
    let t = t.trim_end_matches(['u', 'U', 'l', 'L']);
    if let Some(h) = t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) {
        return i128::from_str_radix(h, 16).ok();
    }
    if t.len() > 1 && t.starts_with('0') && t.bytes().all(|b| b.is_ascii_digit()) {
        return i128::from_str_radix(t.get(1..)?, 8).ok();
    }
    t.parse::<i128>().ok()
}

#[must_use]
pub fn char_value(text: &str) -> Option<i128> {
    let body = text.strip_prefix('\'')?.strip_suffix('\'')?;
    let b = body.as_bytes();
    match b {
        [c] if *c != b'\\' => Some(i128::from(*c)),
        [b'\\', e] => Some(i128::from(match e {
            b'n' => b'\n',
            b't' => b'\t',
            b'r' => b'\r',
            b'0' => 0,
            b'\\' => b'\\',
            b'\'' => b'\'',
            b'"' => b'"',
            b'a' => 7,
            b'b' => 8,
            b'f' => 12,
            b'v' => 11,
            _ => return None,
        })),
        [b'\\', b'x', h @ ..] if !h.is_empty() && h.len() <= 8 => {
            h.iter().try_fold(0i128, |acc, &d| {
                let v = char::from(d).to_digit(16)?;
                Some(acc * 16 + i128::from(v))
            })
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn texts(lx: &Lexed) -> Vec<&str> {
        lx.tokens.iter().map(|t| t.text.as_str()).collect()
    }

    #[test]
    fn cp1252_bytes_round_trip_through_the_tokens_and_offsets() {
        let mut src = b"char s[] = \"".to_vec();
        src.extend(0x80u8..=0xff);
        src.extend_from_slice(b"\"; /* \xa7 \x93x\x94 */ int \xe9;\r\n");
        let lx = lex(&src);
        let s = lx.tokens.iter().find(|t| t.kind == Kind::Str).unwrap();
        assert_eq!(srcio::encode(&s.text).unwrap(), &src[s.start..s.end]);
        assert_eq!(s.text.chars().count(), 128 + 2);
        let c = &lx.comments[0];
        assert_eq!(&src[c.start..c.end], b"/* \xa7 \x93x\x94 */");
        let high = lx.tokens.iter().find(|t| t.text == "\u{e9}").unwrap();
        assert_eq!(high.kind, Kind::Punct);
        assert_eq!(&src[high.start..high.end], b"\xe9");
        assert!(lx.problems.is_empty());
    }

    #[test]
    fn trigraphs_are_replaced_before_anything_else_reads_the_source() {
        let src = b"??=define Q '??/''\nchar *s = \"a??/\"b\"; // c\nint x ??( 2 ??) = ??< 1 ??>;\nchar e = \"\\??=\"[0];\n";
        let lx = lex(src);
        let d = &lx.directives[0];
        assert_eq!(d.name, "define");
        assert_eq!(lx.args(d)[1].text, "'\\''");
        let s = lx
            .tokens
            .iter()
            .find(|t| t.kind == Kind::Str && t.line == 2)
            .unwrap();
        assert_eq!(s.text, "\"a\\\"b\"");
        assert_eq!(lx.comments.len(), 1);
        assert_eq!(lx.comments[0].first, 2);
        let line3: Vec<&str> = lx
            .tokens
            .iter()
            .filter(|t| t.line == 3)
            .map(|t| t.text.as_str())
            .collect();
        assert_eq!(line3, ["int", "x", "[", "2", "]", "=", "{", "1", "}", ";"]);
        let e = lx
            .tokens
            .iter()
            .find(|t| t.kind == Kind::Str && t.line == 4)
            .unwrap();
        assert_eq!(e.text, "\"\\#\"");
        assert_eq!(lx.trigraphs.len(), 8);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
    }

    #[test]
    fn a_trigraph_backslash_ending_a_literal_escapes_its_quote() {
        let src = b"char *p = \"x??/\"; // not a comment\n";
        let lx = lex(src);
        assert!(lx.comments.is_empty());
        assert_eq!(lx.problems.len(), 1);
        assert!(lx.problems[0].what.contains("string literal"));
        let src = b"char *p = \"x??/??/\"; // a comment\n";
        let lx = lex(src);
        assert_eq!(lx.comments.len(), 1);
        assert!(lx.problems.is_empty());
    }

    #[test]
    fn a_question_run_starts_its_trigraph_at_the_last_two() {
        let lx = lex(b"char *s = \"???=\";\n");
        let s = lx.tokens.iter().find(|t| t.kind == Kind::Str).unwrap();
        assert_eq!(s.text, "\"?#\"");
        let lx = lex(b"x = a ?? b;\n");
        assert_eq!(texts(&lx), ["x", "=", "a", "?", "?", "b", ";"]);
        assert!(lx.trigraphs.is_empty());
    }

    #[test]
    fn splices_join_lines_everywhere_and_keep_physical_lines() {
        let src = b"#define TWO a + \\\n   b\nint x = 1; // c \\\nstill the comment\nchar *s = \"ab\\\r\ncd\";\nint l\\\nong_name = 2;\nint after;\n";
        let lx = lex(src);
        let d = &lx.directives[0];
        assert_eq!(d.first, 1);
        assert_eq!(d.last, 2);
        assert_eq!(
            lx.args(d)
                .iter()
                .map(|t| t.text.as_str())
                .collect::<Vec<_>>(),
            ["TWO", "a", "+", "b"]
        );
        assert_eq!(lx.comments.len(), 1);
        assert_eq!((lx.comments[0].first, lx.comments[0].last), (3, 4));
        let s = lx.tokens.iter().find(|t| t.kind == Kind::Str).unwrap();
        assert_eq!(s.text, "\"abcd\"");
        assert_eq!((s.line, s.end_line), (5, 6));
        let long = lx.tokens.iter().find(|t| t.text == "long_name").unwrap();
        assert_eq!((long.line, long.end_line), (7, 8));
        let after = lx.tokens.iter().find(|t| t.text == "after").unwrap();
        assert_eq!(after.line, 9);
        assert_eq!(lx.splices.len(), 4);
        let src = b"int a; // trigraph splice ??/\nint b;\n";
        let lx = lex(src);
        assert!(lx.tokens.iter().all(|t| t.text != "b"));
        assert_eq!(lx.comments[0].last, 2);
    }

    #[test]
    fn a_lone_cr_is_no_line_break_and_crlf_is_one() {
        let lx = lex(b"int a;\r\nint b;\rint c;\nint d;");
        let line = |n: &str| lx.tokens.iter().find(|t| t.text == n).unwrap().line;
        assert_eq!((line("a"), line("b"), line("c"), line("d")), (1, 2, 2, 3));
        assert_eq!(lx.lines, 3);
        let lx = lex(b"int a; // x\r\nint b;\r\n");
        let c = &lx.comments[0];
        assert_eq!(c.end, 11);
    }

    #[test]
    fn if_zero_groups_are_dead_and_their_prose_does_not_derail_the_lexer() {
        let src = b"#if 0\nit's prose, don't \"lex\" me\n#define GONE 1\n#line 900\n#else\nint live;\n#endif\n#ifdef X\nint maybe;\n#if 0\nint deeper;\n#endif\n#elif 1\nint unknown_after;\n#endif\nint done; /* c */\n";
        let lx = lex(src);
        assert!(lx.problems.is_empty(), "{:?}", lx.problems);
        let cond = |n: &str| lx.tokens.iter().find(|t| t.text == n).unwrap().cond;
        assert_eq!(cond("me"), Cond::Dead);
        assert_eq!(cond("live"), Cond::Live);
        assert_eq!(cond("maybe"), Cond::Unknown);
        assert_eq!(cond("deeper"), Cond::Dead);
        assert_eq!(cond("unknown_after"), Cond::Unknown);
        assert_eq!(cond("done"), Cond::Live);
        let line = lx.directives.iter().find(|d| d.name == "line").unwrap();
        assert_eq!(line.cond, Cond::Dead);
        let def = lx.directives.iter().find(|d| d.name == "define").unwrap();
        assert_eq!(def.cond, Cond::Dead);
        assert_eq!(lx.comments.len(), 1);
        let lx = lex(b"#if 1\nint a;\n#else\nint b;\n#endif\n#if 0x0\nint c;\n#elif 0\nint d;\n#else\nint e;\n#endif\n");
        let cond = |n: &str| lx.tokens.iter().find(|t| t.text == n).unwrap().cond;
        assert_eq!(
            [cond("a"), cond("b"), cond("c"), cond("d"), cond("e")],
            [Cond::Live, Cond::Dead, Cond::Dead, Cond::Dead, Cond::Live]
        );
    }

    #[test]
    fn line_directives_and_their_numbers_are_tokens_of_the_directive() {
        let lx = lex(b"int a;\n#line 100 \"x.c\"\nint b = __LINE__;\n");
        let d = lx.directives.iter().find(|d| d.name == "line").unwrap();
        assert_eq!(d.first, 2);
        assert_eq!(
            lx.args(d)
                .iter()
                .map(|t| t.text.as_str())
                .collect::<Vec<_>>(),
            ["100", "\"x.c\""]
        );
        assert!(lx.args(d).iter().all(|t| t.directive.is_some()));
        let use_ = lx.tokens.iter().find(|t| t.text == "__LINE__").unwrap();
        assert_eq!((use_.line, use_.directive), (3, None));
    }

    #[test]
    fn inline_assembler_reads_a_semicolon_as_a_comment() {
        let src = b"void f() {\n  __asm {\n    mov eax, 1 ; it's a comment\n    add eax, 2\n  }\n  __asm mov ebx, 3 ; also one\n  int x = 1; int y;\n}\n";
        let lx = lex(src);
        assert_eq!(lx.comments.len(), 2);
        assert!(lx.comments.iter().all(|c| c.kind == CommentKind::Asm));
        assert!(lx.problems.is_empty());
        let x = lx.tokens.iter().find(|t| t.text == "x").unwrap();
        assert!(!x.asm);
        let mov = lx.tokens.iter().find(|t| t.text == "mov").unwrap();
        assert!(mov.asm);
        let y = lx.tokens.iter().find(|t| t.text == "y").unwrap();
        assert!(!y.asm);
    }

    #[test]
    fn include_names_wide_literals_and_numbers_lex_whole() {
        let lx = lex(b"#include <sys//types.h>\n#include \"a.h\"\nwchar_t *w = L\"x\"; double d = 1.5e-3; int h = 0x1Fu;\n");
        assert_eq!(lx.tokens[2].kind, Kind::Header);
        assert_eq!(lx.tokens[2].text, "<sys//types.h>");
        assert!(lx.comments.is_empty());
        assert!(
            lx.tokens
                .iter()
                .any(|t| t.kind == Kind::Str && t.text == "L\"x\"")
        );
        assert!(
            lx.tokens
                .iter()
                .any(|t| t.kind == Kind::Number && t.text == "1.5e-3")
        );
        assert_eq!(number_value("0x1Fu"), Some(31));
        assert_eq!(number_value("010"), Some(8));
        assert_eq!(number_value("12L"), Some(12));
        assert_eq!(char_value("'A'"), Some(65));
        assert_eq!(char_value("'\\n'"), Some(10));
    }

    #[test]
    fn an_unterminated_block_comment_is_a_problem() {
        let lx = lex(b"int a; /* never\nint b;\n");
        assert_eq!(lx.problems.len(), 1);
        assert!(!lx.comments[0].closed);
        assert_eq!(lx.comments[0].end, 23);
    }
}
