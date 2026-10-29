use crate::lex::{self, Comment};
use crate::linepos::{self, Anchors, MacroSet};
use crate::syntax::{self, Syntax};
use msvc6_base::digest::sha256_hex;
use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum Layout {
    Default,
    Anchored,
    LineKeep,
}

impl Layout {
    pub const ALL: [Self; 3] = [Self::Default, Self::Anchored, Self::LineKeep];

    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Default => "default",
            Self::Anchored => "anchored",
            Self::LineKeep => "line-keep",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct KeepLine {
    pub line: usize,
    pub sha256: String,
}

#[must_use]
pub fn keep_set(src: &[u8], cs: &[Comment], keep: &[KeepLine]) -> Vec<bool> {
    cs.iter()
        .map(|c| {
            let text = src.get(c.start..c.end).unwrap_or_default();
            keep.iter()
                .any(|k| k.line == c.first && k.sha256 == sha256_hex(text))
        })
        .collect()
}

fn ws(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\r' | 0x0b | 0x0c)
}

fn all_ws(s: &[u8]) -> bool {
    s.iter().all(|b| ws(*b))
}

fn trim_end(mut v: Vec<u8>) -> Vec<u8> {
    while v.last().is_some_and(|b| ws(*b)) {
        v.pop();
    }
    v
}

fn join_pieces(pieces: &[&[u8]]) -> Vec<u8> {
    let mut out: Vec<u8> = Vec::new();
    for (i, p) in pieces.iter().enumerate() {
        if i == 0 {
            out.extend_from_slice(p);
            continue;
        }
        if out.last().is_none_or(|b| ws(*b)) {
            let t = p.iter().position(|b| !ws(*b)).unwrap_or(p.len());
            out.extend_from_slice(p.get(t..).unwrap_or_default());
        } else {
            if p.first().is_some_and(|b| !ws(*b)) {
                out.push(b' ');
            }
            out.extend_from_slice(p);
        }
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Unit {
    Line { body: Vec<u8>, eol: Vec<u8> },
    Gone,
}

fn normalize_blank_runs(units: Vec<Unit>) -> Vec<Unit> {
    let blank = |u: &Unit| matches!(u, Unit::Line { body, .. } if all_ws(body));
    let mut out: Vec<Unit> = Vec::with_capacity(units.len());
    let mut i = 0usize;
    while let Some(u) = units.get(i) {
        if !blank(u) && *u != Unit::Gone {
            out.push(u.clone());
            i += 1;
            continue;
        }
        let mut j = i;
        while units.get(j).is_some_and(|u| blank(u) || *u == Unit::Gone) {
            j += 1;
        }
        let run = units.get(i..j).unwrap_or_default();
        if run.contains(&Unit::Gone) {
            let edge = i == 0 || j >= units.len();
            let longest = run
                .split(|u| *u == Unit::Gone)
                .map(<[Unit]>::len)
                .max()
                .unwrap_or(0);
            let keep = if edge { 0 } else { longest };
            out.extend(run.iter().filter(|u| **u != Unit::Gone).take(keep).cloned());
        } else {
            out.extend(run.iter().cloned());
        }
        i = j;
    }
    out
}

#[must_use]
pub fn strip_with(
    src: &[u8],
    cs: &[Comment],
    keep: &[bool],
    line_kept: &dyn Fn(&Comment) -> bool,
) -> Vec<u8> {
    let gone: Vec<&Comment> = cs
        .iter()
        .zip(keep.iter().chain(std::iter::repeat(&false)))
        .filter(|(_, k)| !**k)
        .map(|(c, _)| c)
        .collect();
    if gone.is_empty() {
        return src.to_vec();
    }
    let mut lk: BTreeSet<usize> = BTreeSet::new();
    for c in gone.iter().filter(|c| line_kept(c)) {
        lk.extend(c.first.saturating_sub(1)..c.last);
    }
    let mut mask = vec![false; src.len()];
    for c in &gone {
        for m in mask.get_mut(c.start..c.end).into_iter().flatten() {
            *m = true;
        }
    }
    let masked = |i: usize| mask.get(i).copied().unwrap_or(false);
    let ls = lex::physical_lines(src);
    let pieces = |from: usize, to: usize| -> (Vec<&[u8]>, bool) {
        let mut out = Vec::new();
        let mut removed = false;
        let (mut s, mut i) = (from, from);
        while i < to {
            if masked(i) {
                out.push(src.get(s..i).unwrap_or_default());
                removed = true;
                while i < to && masked(i) {
                    i += 1;
                }
                s = i;
            } else {
                i += 1;
            }
        }
        out.push(src.get(s..to).unwrap_or_default());
        (out, removed)
    };
    let open_ended = |l: &(usize, usize, usize)| l.1 < l.2 && masked(l.1);
    let holder = |at: usize| gone.iter().find(|c| c.start <= at && at < c.end);
    let mut units: Vec<Unit> = Vec::with_capacity(ls.len());
    let mut i = 0usize;
    while let Some(l) = ls.get(i) {
        let eol = src.get(l.1..l.2).unwrap_or_default().to_vec();
        if !lk.contains(&i) && open_ended(l) {
            let mut all: Vec<&[u8]> = Vec::new();
            let mut j = i;
            let mut end_eol = eol;
            while let Some(m) = ls.get(j) {
                if j > i && lk.contains(&j) {
                    break;
                }
                let (p, _) = pieces(m.0, m.1);
                if j > i {
                    all.push(&[]);
                }
                all.extend(p);
                end_eol = src.get(m.1..m.2).unwrap_or_default().to_vec();
                j += 1;
                if !open_ended(m) {
                    break;
                }
            }
            let body = trim_end(join_pieces(&all));
            let n = j - i;
            if all_ws(&body) {
                units.extend(std::iter::repeat_n(Unit::Gone, n));
            } else {
                units.push(Unit::Line { body, eol: end_eol });
                units.extend(std::iter::repeat_n(Unit::Gone, n.saturating_sub(1)));
            }
            i = j;
            continue;
        }
        let (p, removed) = pieces(l.0, l.1);
        let ends_inside = open_ended(l);
        if !removed && !ends_inside {
            units.push(Unit::Line {
                body: src.get(l.0..l.1).unwrap_or_default().to_vec(),
                eol,
            });
            i += 1;
            continue;
        }
        let mut body = trim_end(join_pieces(&p));
        if !lk.contains(&i) && all_ws(&body) {
            units.push(Unit::Gone);
            i += 1;
            continue;
        }
        if all_ws(&body) {
            body.clear();
        }
        if let Some(c) = ends_inside.then(|| holder(l.1)).flatten() {
            let code_after = ls
                .get(c.last.saturating_sub(1))
                .is_some_and(|e| src.get(c.end..e.1).is_some_and(|t| !all_ws(t)));
            if c.directive && code_after && line_kept(c) {
                if !body.is_empty() {
                    body.push(b' ');
                }
                body.push(b'\\');
            }
        }
        units.push(Unit::Line { body, eol });
        i += 1;
    }
    let mut out = Vec::with_capacity(src.len());
    for u in normalize_blank_runs(units) {
        if let Unit::Line { body, eol } = u {
            out.extend(body);
            out.extend(eol);
        }
    }
    out
}

#[must_use]
pub fn strip(src: &[u8], cs: &[Comment], keep: &[bool], layout: Layout, a: &Anchors) -> Vec<u8> {
    match layout {
        Layout::Default => strip_with(src, cs, keep, &|_| false),
        Layout::LineKeep => strip_with(src, cs, keep, &|_| true),
        Layout::Anchored => strip_with(src, cs, keep, &|c| a.holds(c.first)),
    }
}

fn word_tokens(syntax: Syntax, src: &[u8]) -> Vec<(Vec<u8>, usize, bool)> {
    let mut mask = vec![false; src.len()];
    for c in syntax::comments(syntax, src) {
        for m in mask.get_mut(c.start..c.end).into_iter().flatten() {
            *m = true;
        }
    }
    let mut out = Vec::new();
    for (n, l) in lex::physical_lines(src).iter().enumerate() {
        let mut w: Vec<u8> = Vec::new();
        for i in l.0..l.1 {
            let b = src.get(i).copied().unwrap_or(b' ');
            if mask.get(i).copied().unwrap_or(false) || ws(b) {
                if !w.is_empty() {
                    out.push((std::mem::take(&mut w), n + 1, false));
                }
            } else {
                w.push(b);
            }
        }
        if !w.is_empty() {
            out.push((w, n + 1, false));
        }
    }
    out
}

fn code_tokens(syntax: Syntax, src: &[u8]) -> Vec<(Vec<u8>, usize, bool)> {
    if syntax != Syntax::C {
        return word_tokens(syntax, src);
    }
    let lx = lex::lex(src);
    let mut out: Vec<(Vec<u8>, usize, bool)> = Vec::with_capacity(lx.tokens.len());
    let mut last_dir: Option<usize> = None;
    for t in &lx.tokens {
        if last_dir.is_some() && t.directive != last_dir {
            out.push((b"\n".to_vec(), 0, true));
        }
        last_dir = t.directive;
        let bytes = msvc6_base::srcio::encode(&t.text).unwrap_or_default();
        out.push((bytes, t.line, t.directive.is_some()));
    }
    if last_dir.is_some() {
        out.push((b"\n".to_vec(), 0, true));
    }
    out
}

#[must_use]
pub fn token_difference(syntax: Syntax, a: &[u8], b: &[u8], lined: bool) -> Option<String> {
    let (x, y) = (code_tokens(syntax, a), code_tokens(syntax, b));
    (0..x.len().max(y.len()))
        .find(|&i| match (x.get(i), y.get(i)) {
            (Some(p), Some(q)) => p.0 != q.0 || p.2 != q.2 || (lined && p.1 != q.1),
            _ => true,
        })
        .map(|i| {
            let say = |t: Option<&(Vec<u8>, usize, bool)>| {
                t.map_or("the end of the file".to_string(), |(b, l, _)| {
                    format!("`{}` (line {l})", msvc6_base::srcio::decode(b))
                })
            };
            format!(
                "code token #{} was {} and is {}",
                i + 1,
                say(x.get(i)),
                say(y.get(i))
            )
        })
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Plan {
    pub syntax: Syntax,
    pub comments: Vec<Comment>,
    pub keep: Vec<bool>,
    pub anchors: Anchors,
    pub problems: Vec<String>,
}

impl Plan {
    #[must_use]
    pub fn new(syntax: Syntax, src: &[u8], keep: &[KeepLine], macros: &MacroSet) -> Self {
        let (comments, anchors, problems) = if syntax == Syntax::C {
            let lx = lex::lex(src);
            let a = linepos::anchors(&lx, macros);
            let p = lx
                .problems
                .iter()
                .map(|p| format!("line {}: {}", p.line, p.what))
                .collect();
            (lx.comments, a, p)
        } else {
            (
                syntax::comments(syntax, src),
                Anchors::default(),
                Vec::new(),
            )
        };
        let keep = keep_set(src, &comments, keep);
        Self {
            syntax,
            comments,
            keep,
            anchors,
            problems,
        }
    }

    #[must_use]
    pub fn stripped(&self) -> usize {
        self.keep.iter().filter(|k| !**k).count()
    }

    #[must_use]
    pub fn left(&self) -> Vec<&Comment> {
        self.comments
            .iter()
            .zip(&self.keep)
            .filter(|(_, k)| !**k)
            .map(|(c, _)| c)
            .collect()
    }

    #[must_use]
    pub fn apply(&self, src: &[u8], layout: Layout) -> Vec<u8> {
        strip(src, &self.comments, &self.keep, layout, &self.anchors)
    }

    pub fn check(
        &self,
        src: &[u8],
        out: &[u8],
        layout: Layout,
        macros: &MacroSet,
    ) -> Result<(), String> {
        if let Some(p) = self.problems.first() {
            return Err(format!(
                "the lexer cannot read it as the compiler does: {p}"
            ));
        }
        if let Some(d) = token_difference(self.syntax, src, out, layout == Layout::LineKeep) {
            return Err(format!("the strip changes the code: {d}"));
        }
        let want: Vec<&[u8]> = self
            .comments
            .iter()
            .zip(&self.keep)
            .filter(|(_, k)| **k)
            .map(|(c, _)| src.get(c.start..c.end).unwrap_or_default())
            .collect();
        let after = syntax::comments(self.syntax, out);
        let got: Vec<&[u8]> = after
            .iter()
            .map(|c| out.get(c.start..c.end).unwrap_or_default())
            .collect();
        if got != want {
            return Err(format!(
                "the comments left are not the ones kept: {} kept, {} left",
                want.len(),
                got.len()
            ));
        }
        if layout == Layout::LineKeep {
            let (a, b) = (
                lex::physical_lines(src).len(),
                lex::physical_lines(out).len(),
            );
            if a != b {
                return Err(format!(
                    "{a} lines in and {b} out: not a line-keeping layout"
                ));
            }
        }
        if self.syntax == Syntax::C {
            let moved = linepos::proof(&lex::lex(src), &lex::lex(out), macros);
            if let Some(m) = moved.first() {
                return Err(format!("a line position moves: {m}"));
            }
        }
        Ok(())
    }

    #[must_use]
    pub fn best(
        &self,
        src: &[u8],
        macros: &MacroSet,
        want: Option<Layout>,
        fmt: Option<&Formatter<'_>>,
    ) -> Outcome {
        let order: Vec<Layout> = match want {
            Some(l) => vec![l],
            None if self.syntax == Syntax::C && !self.anchors.is_empty() => {
                vec![Layout::Anchored, Layout::LineKeep]
            }
            None => vec![Layout::Default, Layout::LineKeep],
        };
        let mut refusals = Vec::new();
        for l in order {
            let raw = self.apply(src, l);
            let out = match fmt {
                None => Ok(raw),
                Some(f) => f(&raw).and_then(|once| {
                    let twice = f(&once)?;
                    if twice == once {
                        Ok(once)
                    } else {
                        Err("the formatter does not reach a fixed point on it".to_string())
                    }
                }),
            };
            let out = match out {
                Ok(o) => o,
                Err(e) => {
                    refusals.push((l, format!("formatting the strip: {e}")));
                    continue;
                }
            };
            match self.check(src, &out, l, macros) {
                Ok(()) => {
                    return Outcome {
                        layout: Some(l),
                        bytes: out,
                        refusals,
                    };
                }
                Err(e) => refusals.push((l, e)),
            }
        }
        Outcome {
            layout: None,
            bytes: src.to_vec(),
            refusals,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Outcome {
    pub layout: Option<Layout>,
    pub bytes: Vec<u8>,
    pub refusals: Vec<(Layout, String)>,
}

pub type Formatter<'a> = dyn Fn(&[u8]) -> Result<Vec<u8>, String> + 'a;

pub fn run_formatter(
    command: &[String],
    path: &std::path::Path,
    bytes: &[u8],
) -> Result<Vec<u8>, String> {
    use std::io::Write as _;
    let args: Vec<String> = command
        .iter()
        .map(|a| a.replace("{path}", &path.display().to_string()))
        .collect();
    let (exe, rest) = args
        .split_first()
        .ok_or_else(|| "[source.format] command is empty".to_string())?;
    let mut child = std::process::Command::new(exe)
        .args(rest)
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .spawn()
        .map_err(|e| format!("running {exe}: {e}"))?;
    let mut stdin = child.stdin.take().ok_or("the formatter has no stdin")?;
    let input = bytes.to_vec();
    let writer = std::thread::spawn(move || stdin.write_all(&input));
    let out = child
        .wait_with_output()
        .map_err(|e| format!("waiting for {exe}: {e}"))?;
    writer
        .join()
        .map_err(|_| "the formatter's writer panicked".to_string())?
        .map_err(|e| format!("writing to {exe}: {e}"))?;
    if !out.status.success() {
        return Err(format!(
            "{exe} exited {}: {}",
            out.status,
            msvc6_base::srcio::tool_text(&out.stderr).trim()
        ));
    }
    Ok(out.stdout)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Left {
    pub path: String,
    pub line: usize,
    pub syntax: Syntax,
    pub text: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct FileRow {
    pub path: String,
    pub syntax: Syntax,
    pub comments: usize,
    pub kept: usize,
    pub problems: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Scan {
    pub files: Vec<FileRow>,
    pub left: Vec<Left>,
    pub without_syntax: Vec<String>,
}

#[must_use]
pub fn excerpt(src: &[u8], c: &Comment) -> String {
    let text = src.get(c.start..c.end).unwrap_or_default();
    let first = text.split(|&b| b == b'\n').next().unwrap_or_default();
    let first = first.strip_suffix(b"\r").unwrap_or(first);
    let cut = first.get(..first.len().min(72)).unwrap_or_default();
    let more = if cut.len() < text.len() { " ..." } else { "" };
    format!("{}{more}", msvc6_base::srcio::decode(cut))
}

pub fn macro_set(
    root: &std::path::Path,
    cfg: &crate::config::Loaded,
    files: &[std::path::PathBuf],
) -> MacroSet {
    let mut m = MacroSet::builtin();
    let c = &cfg.source.macros;
    m.add(&c.function, &c.object, &c.stringize);
    if c.harvest {
        let lexed: Vec<lex::Lexed> = files
            .iter()
            .filter(|p| cfg.syntax_of(&crate::tree::rel_str(p)) == Syntax::C)
            .filter_map(|p| std::fs::read(root.join(p)).ok())
            .map(|b| lex::lex(&b))
            .collect();
        m.harvest(lexed.iter(), c.crtdbg_map_alloc);
    }
    m
}

pub fn scan(
    root: &std::path::Path,
    cfg: &crate::config::Loaded,
    files: &[std::path::PathBuf],
    macros: &MacroSet,
) -> anyhow::Result<Scan> {
    let mut s = Scan::default();
    for p in files {
        let rel = crate::tree::rel_str(p);
        let syntax = cfg.syntax_of(&rel);
        if syntax == Syntax::None {
            s.without_syntax.push(rel);
            continue;
        }
        let src = std::fs::read(root.join(p))
            .map_err(|e| anyhow::anyhow!("reading {}: {e}", root.join(p).display()))?;
        let plan = Plan::new(syntax, &src, &cfg.keep_for(&rel), macros);
        for c in plan.left() {
            s.left.push(Left {
                path: rel.clone(),
                line: c.first,
                syntax,
                text: excerpt(&src, c),
            });
        }
        s.files.push(FileRow {
            path: rel,
            syntax,
            comments: plan.comments.len(),
            kept: plan.keep.iter().filter(|k| **k).count(),
            problems: plan.problems.clone(),
        });
    }
    Ok(s)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(src: &[u8], layout: Layout) -> Vec<u8> {
        let m = MacroSet::builtin();
        let p = Plan::new(Syntax::C, src, &[], &m);
        let out = p.apply(src, layout);
        p.check(src, &out, layout, &m).unwrap();
        let again = Plan::new(Syntax::C, &out, &[], &m);
        assert_eq!(again.stripped(), 0);
        out
    }

    #[test]
    fn the_default_layout_drops_comment_lines_and_joins_code() {
        let src = b"/* head */\n\nint a; // tail\nint/* mid */b;\n\n\n/* block\n   over lines */\nint c;\n";
        assert_eq!(
            run(src, Layout::Default),
            b"int a;\nint b;\n\n\nint c;\n".to_vec()
        );
    }

    #[test]
    fn the_line_keep_layout_keeps_every_line() {
        let src = b"/* head */\nint a; // tail\n/* block\n   over lines */\nint c;\n";
        let out = run(src, Layout::LineKeep);
        assert_eq!(out, b"\nint a;\n\n\nint c;\n".to_vec());
        assert_eq!(lex::physical_lines(&out).len(), 5);
    }

    #[test]
    fn the_anchored_layout_keeps_lines_above_a_line_position_only() {
        let src = b"/* free */\nint a;\n#line 10\n/* pinned */\nint b = __LINE__;\n";
        let out = run(src, Layout::Anchored);
        assert_eq!(out, b"int a;\n#line 10\n\nint b = __LINE__;\n".to_vec());
        let m = MacroSet::builtin();
        let p = Plan::new(Syntax::C, src, &[], &m);
        let bad = p.apply(src, Layout::Default);
        let e = p.check(src, &bad, Layout::Anchored, &m).unwrap_err();
        assert!(e.contains("line position"), "{e}");
    }

    #[test]
    fn a_comment_in_a_directive_keeps_the_directive_whole() {
        let src = b"#define A 1 /* one\n two */ + 2\nint x = A;\n";
        let out = run(src, Layout::Default);
        assert_eq!(out, b"#define A 1 + 2\nint x = A;\n".to_vec());
        let out = run(src, Layout::LineKeep);
        assert_eq!(out, b"#define A 1 \\\n+ 2\nint x = A;\n".to_vec());
    }

    #[test]
    fn a_line_comment_ending_in_a_splice_takes_the_next_line() {
        let src = b"int a; // note \\\nint gone;\nint b;\n";
        assert_eq!(run(src, Layout::Default), b"int a;\nint b;\n".to_vec());
    }

    #[test]
    fn strings_holding_comment_text_and_trigraphs_are_untouched() {
        let src = b"char *s = \"/* not */ // no\";\nchar *t = \"a??/\"b\"; /* yes */\nchar c = '??/''; // gone\n";
        let out = run(src, Layout::Default);
        assert_eq!(
            out,
            b"char *s = \"/* not */ // no\";\nchar *t = \"a??/\"b\";\nchar c = '??/'';\n".to_vec()
        );
    }

    #[test]
    fn cp1252_and_crlf_survive_byte_for_byte() {
        let src = b"char *s = \"\xa7\x93\";\r\n/* \xe9t\xe9 */\r\nint a; // \xff\r\n";
        assert_eq!(
            run(src, Layout::Default),
            b"char *s = \"\xa7\x93\";\r\nint a;\r\n".to_vec()
        );
    }

    #[test]
    fn a_kept_line_stays_where_its_consumer_reads_it() {
        let src = b"#!/bin/sh\n# drop\necho hi # drop too\n";
        let p = Plan::new(Syntax::HashInline, src, &[], &MacroSet::builtin());
        assert_eq!(p.stripped(), 2);
        assert_eq!(
            p.apply(src, Layout::Default),
            b"#!/bin/sh\necho hi\n".to_vec()
        );
        let toml_src = b"# required\nkey = 1\n";
        let keep = [KeepLine {
            line: 1,
            sha256: sha256_hex(b"# required"),
        }];
        let p = Plan::new(Syntax::HashInline, toml_src, &keep, &MacroSet::builtin());
        assert_eq!(p.stripped(), 0);
        let moved = [KeepLine {
            line: 2,
            sha256: sha256_hex(b"# required"),
        }];
        let p = Plan::new(Syntax::HashInline, toml_src, &moved, &MacroSet::builtin());
        assert_eq!(p.stripped(), 1);
    }

    #[test]
    fn the_best_layout_falls_back_and_says_why() {
        let src = b"int a;\n/* c */\nint b = __LINE__;\n";
        let m = MacroSet::builtin();
        let p = Plan::new(Syntax::C, src, &[], &m);
        let o = p.best(src, &m, None, None);
        assert_eq!(o.layout, Some(Layout::Anchored));
        assert_eq!(o.bytes, b"int a;\n\nint b = __LINE__;\n".to_vec());
        let o = p.best(src, &m, Some(Layout::Default), None);
        assert_eq!(o.layout, None);
        assert!(
            o.refusals[0].1.contains("line position"),
            "{:?}",
            o.refusals
        );
    }

    #[test]
    fn blank_runs_keep_their_longer_side() {
        let src = b"int a;\n\n\n// c\n\nint b;\n";
        assert_eq!(run(src, Layout::Default), b"int a;\n\n\nint b;\n".to_vec());
    }
}
