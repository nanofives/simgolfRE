use crate::lex::{self, Comment, CommentKind};
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum Syntax {
    C,
    Def,
    Bat,
    HashLine,
    HashInline,
    Ini,
    Dsp,
    Dsw,
    Markdown,
    None,
}

impl Syntax {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::C => "c",
            Self::Def => "def",
            Self::Bat => "bat",
            Self::HashLine => "hash-line",
            Self::HashInline => "hash-inline",
            Self::Ini => "ini",
            Self::Dsp => "dsp",
            Self::Dsw => "dsw",
            Self::Markdown => "markdown",
            Self::None => "none",
        }
    }
}

pub const C_EXTS: [&str; 23] = [
    "c", "cc", "cp", "cpp", "cxx", "c++", "h", "hh", "hp", "hpp", "hxx", "h++", "inl", "inc",
    "ipp", "ixx", "tcc", "tpp", "txx", "i", "rc", "rc2", "idl",
];

pub const DSP_DIRECTIVES: [&str; 7] = [
    "PROP", "ADD", "SUBTRACT", "Begin", "End", "Name", "TARGTYPE",
];

pub const DSP_SIGNATURE: [&str; 2] = [
    "# Microsoft Developer Studio Project File - ",
    "# Microsoft Developer Studio Generated Build File, Format Version ",
];

#[must_use]
pub fn builtin(rel: &Path) -> Syntax {
    let name = rel
        .file_name()
        .map(|n| n.to_string_lossy().to_ascii_lowercase())
        .unwrap_or_default();
    match name.as_str() {
        ".clang-format" | "_clang-format" | ".clang-tidy" => return Syntax::HashInline,
        ".gitignore"
        | ".gitattributes"
        | ".clang-format-ignore"
        | "dockerfile"
        | ".dockerignore" => return Syntax::HashLine,
        ".editorconfig" | ".gitmodules" => return Syntax::Ini,
        _ => {}
    }
    let Some(ext) = rel
        .extension()
        .map(|e| e.to_string_lossy().to_ascii_lowercase())
    else {
        return Syntax::None;
    };
    match ext.as_str() {
        e if C_EXTS.contains(&e) => Syntax::C,
        "def" => Syntax::Def,
        "bat" | "cmd" => Syntax::Bat,
        "dsp" => Syntax::Dsp,
        "dsw" => Syntax::Dsw,
        "md" | "markdown" => Syntax::Markdown,
        "sh" | "bash" | "yml" | "yaml" | "toml" | "cfg" | "conf" => Syntax::HashInline,
        "ini" => Syntax::Ini,
        _ => Syntax::None,
    }
}

fn ws(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\r' | 0x0b | 0x0c)
}

fn indent(body: &[u8]) -> usize {
    body.iter().position(|b| !ws(*b)).unwrap_or(body.len())
}

fn line_comments(
    src: &[u8],
    kind: CommentKind,
    at: &dyn Fn(&[u8], usize) -> Option<usize>,
) -> Vec<Comment> {
    lex::physical_lines(src)
        .iter()
        .enumerate()
        .filter_map(|(n, l)| {
            let body = src.get(l.0..l.1).unwrap_or_default();
            at(body, n).map(|k| Comment {
                kind,
                start: l.0 + k,
                end: l.1,
                first: n + 1,
                last: n + 1,
                directive: false,
                whole_line: k == indent(body),
                closed: true,
            })
        })
        .collect()
}

fn shebang(body: &[u8], n: usize) -> bool {
    n == 0 && body.starts_with(b"#!")
}

#[must_use]
pub fn def_comments(src: &[u8]) -> Vec<Comment> {
    line_comments(src, CommentKind::Line, &|body, _| {
        let mut quoted = false;
        for (k, &b) in body.iter().enumerate() {
            match b {
                b'"' => quoted = !quoted,
                b';' if !quoted => return Some(k),
                _ => {}
            }
        }
        None
    })
}

#[must_use]
pub fn bat_comments(src: &[u8]) -> Vec<Comment> {
    line_comments(src, CommentKind::Line, &|body, _| {
        let i = indent(body);
        let t = body.get(i..).unwrap_or_default();
        if t.starts_with(b"::") {
            return Some(i);
        }
        let w = t.strip_prefix(b"@").unwrap_or(t);
        let rem = w.get(..3).is_some_and(|r| r.eq_ignore_ascii_case(b"rem"))
            && w.get(3).is_none_or(|c| ws(*c));
        rem.then_some(i)
    })
}

#[must_use]
pub fn hash_line_comments(src: &[u8], ini: bool) -> Vec<Comment> {
    line_comments(src, CommentKind::Line, &|body, n| {
        if shebang(body, n) {
            return None;
        }
        let i = indent(body);
        body.get(i)
            .filter(|c| **c == b'#' || (ini && **c == b';'))
            .map(|_| i)
    })
}

#[must_use]
pub fn hash_inline_comments(src: &[u8]) -> Vec<Comment> {
    line_comments(src, CommentKind::Line, &|body, n| {
        if shebang(body, n) {
            return None;
        }
        let mut q: Option<u8> = None;
        let mut k = 0usize;
        while let Some(&c) = body.get(k) {
            match q {
                Some(b'"') if c == b'\\' => k += 1,
                Some(x) if c == x => q = None,
                Some(_) => {}
                None if c == b'\'' || c == b'"' => q = Some(c),
                None if c == b'#' && (k == 0 || body.get(k - 1).is_some_and(|p| ws(*p))) => {
                    return Some(k);
                }
                None => {}
            }
            k += 1;
        }
        None
    })
}

#[must_use]
pub fn dsp_comments(src: &[u8]) -> Vec<Comment> {
    line_comments(src, CommentKind::Line, &|body, n| {
        if DSP_SIGNATURE
            .get(n)
            .is_some_and(|s| body.starts_with(s.as_bytes()))
        {
            return None;
        }
        let rest = body.strip_prefix(b"#")?;
        let word: Vec<u8> = rest
            .iter()
            .skip_while(|b| ws(**b))
            .take_while(|b| !ws(**b))
            .copied()
            .collect();
        let directive = DSP_DIRECTIVES
            .iter()
            .any(|d| d.as_bytes() == word.as_slice());
        (!directive).then_some(0)
    })
}

#[must_use]
pub fn dsw_comments(src: &[u8]) -> Vec<Comment> {
    line_comments(src, CommentKind::Line, &|body, _| {
        body.starts_with(b"#").then_some(0)
    })
}

#[must_use]
pub fn html_comments(src: &[u8]) -> Vec<Comment> {
    let ls = lex::physical_lines(src);
    let mut out = Vec::new();
    let mut fenced = false;
    let mut n = 0usize;
    while let Some(l) = ls.get(n) {
        let body = src.get(l.0..l.1).unwrap_or_default();
        let t = body.get(indent(body)..).unwrap_or_default();
        if t.starts_with(b"```") || t.starts_with(b"~~~") {
            fenced = !fenced;
            n += 1;
            continue;
        }
        let open = (!fenced)
            .then(|| body.windows(4).position(|w| w == b"<!--"))
            .flatten();
        let Some(k) = open else {
            n += 1;
            continue;
        };
        let start = l.0 + k;
        let close = src
            .get(start + 4..)
            .and_then(|r| r.windows(3).position(|w| w == b"-->"))
            .map(|p| start + 4 + p + 3);
        let end = close.unwrap_or(src.len());
        let last = ls
            .iter()
            .position(|m| end <= m.2)
            .unwrap_or(ls.len().saturating_sub(1));
        out.push(Comment {
            kind: CommentKind::Block,
            start,
            end: ls.get(last).map_or(end, |m| end.min(m.1.max(start))),
            first: n + 1,
            last: last + 1,
            directive: false,
            whole_line: k == indent(body),
            closed: close.is_some(),
        });
        n = last + 1;
    }
    out
}

#[must_use]
pub fn comments(syntax: Syntax, src: &[u8]) -> Vec<Comment> {
    match syntax {
        Syntax::C => lex::lex(src).comments,
        Syntax::Def => def_comments(src),
        Syntax::Bat => bat_comments(src),
        Syntax::HashLine => hash_line_comments(src, false),
        Syntax::Ini => hash_line_comments(src, true),
        Syntax::HashInline => hash_inline_comments(src),
        Syntax::Dsp => dsp_comments(src),
        Syntax::Dsw => dsw_comments(src),
        Syntax::Markdown => html_comments(src),
        Syntax::None => Vec::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn spans(src: &[u8], cs: &[Comment]) -> Vec<String> {
        cs.iter()
            .map(|c| msvc6_base::srcio::decode(&src[c.start..c.end]))
            .collect()
    }

    #[test]
    fn file_names_and_extensions_choose_a_syntax() {
        let s = |p: &str| builtin(&PathBuf::from(p));
        assert_eq!(s("src/a.CPP"), Syntax::C);
        assert_eq!(s("res/app.rc"), Syntax::C);
        assert_eq!(s("x/app.def"), Syntax::Def);
        assert_eq!(s("build.bat"), Syntax::Bat);
        assert_eq!(s("docker/Dockerfile"), Syntax::HashLine);
        assert_eq!(s(".editorconfig"), Syntax::Ini);
        assert_eq!(s("p/p.dsp"), Syntax::Dsp);
        assert_eq!(s("w.dsw"), Syntax::Dsw);
        assert_eq!(s("README.md"), Syntax::Markdown);
        assert_eq!(s("a/.clang-format"), Syntax::HashInline);
        assert_eq!(s("LICENSE"), Syntax::None);
        assert_eq!(s("data.json"), Syntax::None);
    }

    #[test]
    fn line_syntaxes_find_their_comments_and_leave_what_their_consumer_reads() {
        let def = b"LIBRARY \"a;b\"\nEXPORTS\n  F @1 ; note\n; whole\n";
        assert_eq!(spans(def, &def_comments(def)), ["; note", "; whole"]);
        let bat = b"@echo off\r\nrem one\r\n  @REM two\r\n:: three\r\nremark x\r\nrem\r\n";
        assert_eq!(
            spans(bat, &bat_comments(bat)),
            ["rem one", "@REM two", ":: three", "rem"]
        );
        let sh = b"#!/usr/bin/env bash\n# a\nx=\"#no\" # b\ny='#no'#no\n";
        assert_eq!(spans(sh, &hash_inline_comments(sh)), ["# a", "# b"]);
        let ini = b"; i\n[core]\n  # j\nk = v ; not\n";
        assert_eq!(spans(ini, &hash_line_comments(ini, true)), ["; i", "# j"]);
    }

    #[test]
    fn a_dsp_keeps_its_signature_and_directives() {
        let dsp = b"# Microsoft Developer Studio Project File - Name=\"x\" - Package Owner=<4>\r\n# Microsoft Developer Studio Generated Build File, Format Version 6.00\r\n# ** DO NOT EDIT **\r\n\r\n# TARGTYPE \"Win32 (x86) Static Library\" 0x0104\r\n# PROP BASE Use_MFC 0\r\n# ADD CPP /nologo\r\n# Begin Source File\r\n# a note\r\n# Microsoft Developer Studio Generated Build File, Format Version 6.00\r\n";
        let cs = dsp_comments(dsp);
        assert_eq!(
            spans(dsp, &cs),
            [
                "# ** DO NOT EDIT **",
                "# a note",
                "# Microsoft Developer Studio Generated Build File, Format Version 6.00"
            ]
        );
        assert_eq!(cs[2].first, 10);
    }

    #[test]
    fn markdown_comments_skip_fenced_code() {
        let md =
            b"# Title\n<!-- one -->\n```\n<!-- code -->\n```\ntext <!-- two\nlines --> after\n";
        let cs = html_comments(md);
        assert_eq!(spans(md, &cs), ["<!-- one -->", "<!-- two\nlines -->"]);
        assert_eq!((cs[1].first, cs[1].last), (6, 7));
    }
}
