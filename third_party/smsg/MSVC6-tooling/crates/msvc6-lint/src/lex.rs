use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    Line,
    Block,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Comment {
    pub kind: Kind,
    pub line: usize,
    pub col: usize,
    pub text: String,
}

impl Comment {
    #[must_use]
    pub fn describe(&self) -> &'static str {
        let t = self.text.as_str();
        if t.starts_with("///") && !t.starts_with("////") {
            "a doc comment"
        } else if t.starts_with("//!") || t.starts_with("/*!") {
            "an inner doc comment"
        } else if t.starts_with("/**") && !t.starts_with("/**/") && !t.starts_with("/***") {
            "a block doc comment"
        } else if self.kind == Kind::Block {
            "a block comment"
        } else {
            "a line comment"
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LexError {
    pub line: usize,
    pub col: usize,
    pub what: &'static str,
}

impl fmt::Display for LexError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}: {}", self.line, self.col, self.what)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Lexed {
    pub comments: Vec<Comment>,
    pub code: String,
}

const SHOWN: usize = 72;

fn at(cs: &[char], i: usize) -> Option<char> {
    cs.get(i).copied()
}

fn is_ident(c: char) -> bool {
    c == '_' || c.is_alphanumeric()
}

#[must_use]
pub fn position(cs: &[char], i: usize) -> (usize, usize) {
    let mut line = 1;
    let mut col = 1;
    for &c in cs.iter().take(i) {
        if c == '\n' {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
    }
    (line, col)
}

fn error(cs: &[char], i: usize, what: &'static str) -> LexError {
    let (line, col) = position(cs, i);
    LexError { line, col, what }
}

fn line_end(cs: &[char], i: usize) -> usize {
    let mut j = i;
    while at(cs, j).is_some_and(|c| c != '\n') {
        j += 1;
    }
    j
}

fn block_end(cs: &[char], i: usize) -> Result<usize, LexError> {
    let mut depth = 1usize;
    let mut j = i + 2;
    loop {
        match (at(cs, j), at(cs, j + 1)) {
            (None, _) => return Err(error(cs, i, "an unterminated block comment")),
            (Some('/'), Some('*')) => {
                depth += 1;
                j += 2;
            }
            (Some('*'), Some('/')) => {
                depth -= 1;
                j += 2;
                if depth == 0 {
                    return Ok(j);
                }
            }
            _ => j += 1,
        }
    }
}

fn string_end(cs: &[char], i: usize) -> Result<usize, LexError> {
    let mut j = i + 1;
    loop {
        match at(cs, j) {
            None => return Err(error(cs, i, "an unterminated string literal")),
            Some('\\') => j += 2,
            Some('"') => return Ok(j + 1),
            Some(_) => j += 1,
        }
    }
}

fn raw_end(cs: &[char], start: usize, body: usize, hashes: usize) -> Result<usize, LexError> {
    let mut j = body;
    loop {
        match at(cs, j) {
            None => return Err(error(cs, start, "an unterminated raw string literal")),
            Some('"') if (1..=hashes).all(|k| at(cs, j + k) == Some('#')) => {
                return Ok(j + 1 + hashes);
            }
            Some(_) => j += 1,
        }
    }
}

fn char_end(cs: &[char], i: usize) -> Result<Option<usize>, LexError> {
    match (at(cs, i + 1), at(cs, i + 2)) {
        (Some('\\'), _) => {
            let mut j = i + 3;
            loop {
                match at(cs, j) {
                    Some('\'') => return Ok(Some(j + 1)),
                    None | Some('\n') => {
                        return Err(error(cs, i, "an unterminated character literal"));
                    }
                    Some(_) => j += 1,
                }
            }
        }
        (Some(c), Some('\'')) if c != '\'' && c != '\n' => Ok(Some(i + 3)),
        _ => Ok(None),
    }
}

fn word_end(cs: &[char], i: usize) -> usize {
    let mut j = i;
    while at(cs, j).is_some_and(is_ident) {
        j += 1;
    }
    j
}

fn prefixed_literal_end(cs: &[char], start: usize, w: usize) -> Result<Option<usize>, LexError> {
    let word: String = cs.get(start..w).unwrap_or_default().iter().collect();
    match word.as_str() {
        "r" | "br" | "cr" => {
            let mut hashes = 0;
            while at(cs, w + hashes) == Some('#') {
                hashes += 1;
            }
            if at(cs, w + hashes) == Some('"') {
                raw_end(cs, start, w + hashes + 1, hashes).map(Some)
            } else {
                Ok(None)
            }
        }
        "b" if at(cs, w) == Some('\'') => char_end(cs, w),
        _ => Ok(None),
    }
}

fn comment(cs: &[char], kind: Kind, start: usize, end: usize) -> Comment {
    let (line, col) = position(cs, start);
    let text: String = cs
        .get(start..end)
        .unwrap_or_default()
        .iter()
        .take_while(|&&c| c != '\n')
        .take(SHOWN)
        .collect();
    Comment {
        kind,
        line,
        col,
        text: text.trim_end().to_string(),
    }
}

struct Scan {
    cs: Vec<char>,
    comments: Vec<Comment>,
    comment_spans: Vec<(usize, usize)>,
    literal_spans: Vec<(usize, usize)>,
}

fn scan(src: &str) -> Result<Scan, LexError> {
    let cs: Vec<char> = src.chars().collect();
    let mut comments = Vec::new();
    let mut comment_spans = Vec::new();
    let mut literal_spans = Vec::new();
    let mut i = 0usize;
    while let Some(c) = at(&cs, i) {
        let next = at(&cs, i + 1);
        let prev_ident = i
            .checked_sub(1)
            .and_then(|p| at(&cs, p))
            .is_some_and(is_ident);
        if c == '/' && (next == Some('/') || next == Some('*')) {
            let (kind, e) = if next == Some('/') {
                (Kind::Line, line_end(&cs, i))
            } else {
                (Kind::Block, block_end(&cs, i)?)
            };
            comments.push(comment(&cs, kind, i, e));
            comment_spans.push((i, e));
            i = e;
            continue;
        }
        let end = if c == '"' {
            string_end(&cs, i)?
        } else if c == '\'' {
            match char_end(&cs, i)? {
                Some(e) => e,
                None => {
                    i = word_end(&cs, i + 1);
                    continue;
                }
            }
        } else if is_ident(c) && !prev_ident {
            let w = word_end(&cs, i);
            match prefixed_literal_end(&cs, i, w)? {
                Some(e) => e,
                None => {
                    i = w;
                    continue;
                }
            }
        } else {
            i += 1;
            continue;
        };
        literal_spans.push((i, end));
        i = end;
    }
    Ok(Scan {
        cs,
        comments,
        comment_spans,
        literal_spans,
    })
}

pub fn lex(src: &str) -> Result<Lexed, LexError> {
    let s = scan(src)?;
    let mut masked = vec![false; s.cs.len()];
    for &(a, b) in s.comment_spans.iter().chain(&s.literal_spans) {
        for m in masked.iter_mut().take(b).skip(a) {
            *m = true;
        }
    }
    let code =
        s.cs.iter()
            .zip(&masked)
            .map(|(&c, &m)| if m && c != '\n' { ' ' } else { c })
            .collect();
    Ok(Lexed {
        comments: s.comments,
        code,
    })
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Tok {
    Ident(String),
    Lifetime(String),
    Lit(String),
    Punct(char),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Token {
    pub tok: Tok,
    pub line: usize,
    pub col: usize,
}

impl Token {
    #[must_use]
    pub fn is_ident(&self, name: &str) -> bool {
        matches!(&self.tok, Tok::Ident(n) if n == name)
    }

    #[must_use]
    pub fn is_punct(&self, c: char) -> bool {
        self.tok == Tok::Punct(c)
    }
}

fn is_ident_start(c: char) -> bool {
    c == '_' || c.is_alphabetic()
}

fn advance(cs: &[char], from: usize, to: usize, pos: &mut (usize, usize)) {
    for &c in cs.get(from..to).unwrap_or_default() {
        if c == '\n' {
            *pos = (pos.0 + 1, 1);
        } else {
            pos.1 += 1;
        }
    }
}

fn text_of(cs: &[char], a: usize, b: usize) -> String {
    cs.get(a..b).unwrap_or_default().iter().collect()
}

fn tokenize(cs: &[char], skips: &[(usize, usize)], literals: &[(usize, usize)]) -> Vec<Token> {
    let mut out = Vec::new();
    let (mut si, mut li) = (0usize, 0usize);
    let mut pos = (1usize, 1usize);
    let mut i = 0usize;
    while let Some(c) = at(cs, i) {
        if let Some(&(a, b)) = skips.get(si)
            && a <= i
        {
            si += 1;
            if a == i {
                advance(cs, i, b, &mut pos);
                i = b;
            }
            continue;
        }
        let mut push = |tok: Tok| {
            out.push(Token {
                tok,
                line: pos.0,
                col: pos.1,
            });
        };
        let lit = literals.get(li).copied();
        if let Some((a, b)) = lit
            && a <= i
        {
            li += 1;
            if a == i {
                push(Tok::Lit(text_of(cs, a, b)));
                advance(cs, i, b, &mut pos);
                i = b;
            }
            continue;
        }
        let end = if c.is_whitespace() {
            i + 1
        } else if is_ident(c) {
            let w = word_end(cs, i);
            let word = text_of(cs, i, w);
            if word == "r" && at(cs, w) == Some('#') && at(cs, w + 1).is_some_and(is_ident_start) {
                let w2 = word_end(cs, w + 1);
                push(Tok::Ident(text_of(cs, w + 1, w2)));
                w2
            } else if let Some((a, b)) = lit
                && a == w
                && (word == "b" || word == "c")
            {
                push(Tok::Lit(text_of(cs, i, b)));
                li += 1;
                b
            } else {
                push(Tok::Ident(word));
                w
            }
        } else if c == '\'' && at(cs, i + 1).is_some_and(is_ident_start) {
            let w = word_end(cs, i + 1);
            push(Tok::Lifetime(text_of(cs, i + 1, w)));
            w
        } else {
            push(Tok::Punct(c));
            i + 1
        };
        advance(cs, i, end, &mut pos);
        i = end;
    }
    out
}

pub fn tokens(src: &str) -> Result<Vec<Token>, LexError> {
    let s = scan(src)?;
    Ok(tokenize(&s.cs, &s.comment_spans, &s.literal_spans))
}

#[must_use]
pub fn code_tokens(code: &str) -> Vec<Token> {
    let cs: Vec<char> = code.chars().collect();
    tokenize(&cs, &[], &[])
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Attribute {
    pub line: usize,
    pub col: usize,
    pub inner: bool,
    pub body: Vec<Token>,
}

fn opens(t: &Token) -> bool {
    matches!(t.tok, Tok::Punct('(' | '[' | '{'))
}

fn closes(t: &Token) -> bool {
    matches!(t.tok, Tok::Punct(')' | ']' | '}'))
}

#[must_use]
pub fn group_end(tokens: &[Token], open: usize) -> usize {
    let mut depth = 0usize;
    for (k, t) in tokens.iter().enumerate().skip(open) {
        if opens(t) {
            depth += 1;
        } else if closes(t) {
            depth = depth.saturating_sub(1);
            if depth == 0 {
                return k;
            }
        }
    }
    tokens.len()
}

#[must_use]
pub fn attributes(tokens: &[Token]) -> Vec<Attribute> {
    let mut out = Vec::new();
    for (k, t) in tokens.iter().enumerate() {
        if !t.is_punct('#') {
            continue;
        }
        let inner = tokens.get(k + 1).is_some_and(|n| n.is_punct('!'));
        let open = if inner { k + 2 } else { k + 1 };
        if !tokens.get(open).is_some_and(|n| n.is_punct('[')) {
            continue;
        }
        let close = group_end(tokens, open);
        out.push(Attribute {
            line: t.line,
            col: t.col,
            inner,
            body: tokens.get(open + 1..close).unwrap_or_default().to_vec(),
        });
    }
    out
}

#[must_use]
pub fn keyed(body: &[Token], key: &str) -> Vec<usize> {
    body.iter()
        .enumerate()
        .filter(|(k, t)| t.is_ident(key) && body.get(k + 1).is_some_and(|n| n.is_punct('=')))
        .map(|(k, _)| k)
        .collect()
}

#[must_use]
pub fn shebang(src: &str) -> bool {
    let s = src.strip_prefix('\u{feff}').unwrap_or(src);
    let Some(rest) = s.strip_prefix("#!") else {
        return false;
    };
    lex(rest).map_or(true, |l| !l.code.trim_start().starts_with('['))
}

#[must_use]
pub fn plain_string(lit: &str) -> Option<String> {
    if let Some(body) = lit.strip_prefix('"') {
        let body = body.strip_suffix('"')?;
        return (!body.contains('\\')).then(|| body.to_string());
    }
    let rest = lit.strip_prefix('r')?;
    let fence = "#".repeat(rest.chars().take_while(|&c| c == '#').count());
    let body = rest
        .strip_prefix(fence.as_str())?
        .strip_prefix('"')?
        .strip_suffix(fence.as_str())?
        .strip_suffix('"')?;
    Some(body.to_string())
}

fn hex_char(digits: &str) -> Option<char> {
    char::from_u32(u32::from_str_radix(digits, 16).ok()?)
}

fn unescape(body: &str) -> Option<String> {
    let mut out = String::new();
    let mut it = body.chars().peekable();
    while let Some(c) = it.next() {
        if c != '\\' {
            out.push(c);
            continue;
        }
        match it.next()? {
            'n' => out.push('\n'),
            'r' => out.push('\r'),
            't' => out.push('\t'),
            '0' => out.push('\0'),
            e @ ('\\' | '\'' | '"') => out.push(e),
            'x' => {
                let h: String = [it.next()?, it.next()?].iter().collect();
                out.push(hex_char(&h)?);
            }
            'u' => {
                if it.next()? != '{' {
                    return None;
                }
                let mut h = String::new();
                loop {
                    match it.next()? {
                        '}' => break,
                        '_' => {}
                        d => h.push(d),
                    }
                }
                out.push(hex_char(&h)?);
            }
            '\n' | '\r' => {
                while it.peek().is_some_and(|c| c.is_whitespace()) {
                    it.next();
                }
            }
            _ => return None,
        }
    }
    Some(out)
}

#[must_use]
pub fn literal_text(lit: &str) -> Option<String> {
    let unprefixed = lit
        .strip_prefix('b')
        .or_else(|| lit.strip_prefix('c'))
        .unwrap_or(lit);
    if unprefixed.starts_with('r') {
        return plain_string(unprefixed);
    }
    let body = unprefixed
        .strip_prefix('"')
        .and_then(|b| b.strip_suffix('"'))
        .or_else(|| {
            unprefixed
                .strip_prefix('\'')
                .and_then(|b| b.strip_suffix('\''))
        })?;
    unescape(body)
}

#[must_use]
pub fn line_col(text: &str, byte: usize) -> (usize, usize) {
    let before = text.get(..byte).unwrap_or(text);
    let line = before.matches('\n').count() + 1;
    let col = before.rsplit('\n').next().unwrap_or("").chars().count() + 1;
    (line, col)
}

#[must_use]
pub fn doc_attributes(code: &str) -> Vec<(usize, usize)> {
    attributes(&code_tokens(code))
        .into_iter()
        .filter(|a| !keyed(&a.body, "doc").is_empty())
        .map(|a| (a.line, a.col))
        .collect()
}

#[must_use]
pub fn mentions(code: &str, needle: &str) -> Vec<(usize, usize)> {
    code.match_indices(needle)
        .map(|(b, _)| line_col(code, b))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn texts(src: &str) -> Vec<String> {
        lex(src)
            .unwrap()
            .comments
            .into_iter()
            .map(|c| c.text)
            .collect()
    }

    #[test]
    fn line_and_block_comments_are_found_with_their_positions() {
        let l = lex("fn a() {}\n  // one\nlet x = 1; /* two */\n").unwrap();
        assert_eq!(l.comments.len(), 2);
        assert_eq!((l.comments[0].line, l.comments[0].col), (2, 3));
        assert_eq!(l.comments[0].text, "// one");
        assert_eq!(l.comments[0].kind, Kind::Line);
        assert_eq!((l.comments[1].line, l.comments[1].col), (3, 12));
        assert_eq!(l.comments[1].kind, Kind::Block);
    }

    #[test]
    fn every_doc_comment_form_is_a_comment() {
        let src = "/// outer\n//! inner\n/** block */\n/*! inner block */\nfn f() {}\n";
        let l = lex(src).unwrap();
        let kinds: Vec<&str> = l.comments.iter().map(Comment::describe).collect();
        assert_eq!(
            kinds,
            [
                "a doc comment",
                "an inner doc comment",
                "a block doc comment",
                "an inner doc comment"
            ]
        );
    }

    #[test]
    fn nested_block_comments_end_at_their_own_close() {
        let l = lex("a /* x /* y */ z */ b // c\n").unwrap();
        assert_eq!(l.comments.len(), 2);
        assert!(l.comments[0].text.ends_with("z */"));
        assert!(l.code.contains(" b "));
    }

    #[test]
    fn strings_hold_no_comments() {
        assert!(texts("let u = \"http://x/*y*/\";\n").is_empty());
        assert!(texts("let e = \"a \\\" // b\";\n").is_empty());
        assert!(texts("let b = b\"//\";\nlet c = c\"/*\";\n").is_empty());
        assert_eq!(texts("let s = \"\\\\\"; // after\n"), ["// after"]);
    }

    #[test]
    fn raw_strings_end_at_their_own_hashes() {
        assert_eq!(texts("let r = r\"a\\\" // b\";\n").len(), 1);
        assert!(texts("let r = r#\"a \"// b\"#;\n").is_empty());
        assert!(texts("let r = r##\"a \"# // b\"##;\n").is_empty());
        assert!(texts("let r = br#\"/*\"#;\nlet s = cr\"//\";\n").is_empty());
        assert_eq!(texts("let r = r#\"x\"#; // tail\n"), ["// tail"]);
    }

    #[test]
    fn chars_and_lifetimes_are_told_apart() {
        assert!(texts("let c = '/'; let d = '\"'; let e = '\\'';\n").is_empty());
        assert!(texts("let q = b'\\''; let s = '\\\\';\n").is_empty());
        assert!(texts("let u = '\\u{2f}';\n").is_empty());
        assert_eq!(
            texts("fn f<'a>(x: &'a str) -> &'a str { x } // c\n"),
            ["// c"]
        );
        assert_eq!(texts("'outer: loop { break 'outer; } // d\n"), ["// d"]);
        assert_eq!(texts("let x: &'static str = \"\"; // e\n"), ["// e"]);
        assert!(texts("let r = 'a'..='z';\n").is_empty());
    }

    #[test]
    fn raw_identifiers_are_not_raw_strings() {
        assert_eq!(texts("let r#type = 1; // f\n"), ["// f"]);
        assert_eq!(texts("let for_r = 2; // g\n"), ["// g"]);
    }

    #[test]
    fn operators_are_not_comments() {
        assert!(texts("let a = 4 / 2; let mut b = 1; b /= 1; let c = a/ b;\n").is_empty());
    }

    #[test]
    fn the_masked_code_keeps_lines_and_blanks_literals() {
        let l = lex("let a = \"x\ny\";\nlet b = 1; // z\n").unwrap();
        assert_eq!(l.code.lines().count(), 3);
        assert!(!l.code.contains('x') && !l.code.contains('z'));
        assert!(l.code.contains("let b = 1;"));
    }

    #[test]
    fn an_unterminated_token_is_an_error_not_a_pass() {
        assert_eq!(
            lex("let a = \"open;\n").unwrap_err().what,
            "an unterminated string literal"
        );
        assert_eq!(
            lex("a /* b /* c */\n").unwrap_err().what,
            "an unterminated block comment"
        );
        assert_eq!(
            lex("let r = r#\"x\";\n").unwrap_err().what,
            "an unterminated raw string literal"
        );
    }

    #[test]
    fn doc_attributes_are_found_and_other_doc_attributes_are_not() {
        let l = lex("#[doc = \"text\"]\nfn a() {}\n#![doc=\"t\"]\n#[doc(hidden)]\nfn b() {}\n")
            .unwrap();
        assert_eq!(doc_attributes(&l.code), [(1, 1), (3, 1)]);
        let s = lex("let s = \"#[doc = 1]\";\n").unwrap();
        assert!(doc_attributes(&s.code).is_empty());
    }

    #[test]
    fn mentions_are_positioned_in_the_masked_code() {
        let l = lex("let a = \"needle\";\nuse needle;\n").unwrap();
        assert_eq!(mentions(&l.code, "needle"), [(2, 5)]);
    }

    #[test]
    fn a_doc_attribute_is_found_at_any_depth_and_by_its_raw_name() {
        let src = "#[cfg_attr(all(), doc = \"a\")]\nfn a() {}\n#[r#doc = \"b\"]\nfn b() {}\n\
                   #[cfg_attr(x, cfg_attr(y, inline, doc\n=\n\"c\"))]\nfn c() {}\n\
                   #[cfg(doc)]\n#[cfg_attr(doc, derive(Debug))]\nstruct D;\n";
        let l = lex(src).unwrap();
        assert_eq!(doc_attributes(&l.code), [(1, 1), (3, 1), (5, 1)]);
    }

    #[test]
    fn tokens_keep_literals_and_name_raw_identifiers() {
        let t = tokens("#[path = r#\"x.rs\"#] mod r#type; let b = b\"y\"; 'a: loop {}\n").unwrap();
        let toks: Vec<&Tok> = t.iter().map(|t| &t.tok).collect();
        assert!(toks.contains(&&Tok::Lit("r#\"x.rs\"#".to_string())));
        assert!(toks.contains(&&Tok::Ident("type".to_string())));
        assert!(toks.contains(&&Tok::Lit("b\"y\"".to_string())));
        assert!(toks.contains(&&Tok::Lifetime("a".to_string())));
        let a = attributes(&t);
        assert_eq!(a.len(), 1);
        assert_eq!(keyed(&a[0].body, "path"), [0]);
        let p = tokens("// c\n  #![x]\n").unwrap();
        assert_eq!((p[0].line, p[0].col), (2, 3));
        assert!(attributes(&p)[0].inner);
    }

    #[test]
    fn a_shebang_is_a_first_line_not_followed_by_an_attribute() {
        assert!(shebang("#!this line is dropped by rustc\nfn main() {}\n"));
        assert!(shebang("\u{feff}#!/usr/bin/env run-cargo-script\n"));
        assert!(shebang("#!"));
        assert!(!shebang("#![allow(dead_code)]\n"));
        assert!(!shebang("#!  \n /* x */ [allow(dead_code)]\n"));
        assert!(!shebang("fn main() {}\n#!x\n"));
    }

    #[test]
    fn literal_values_are_decoded_and_paths_must_be_plain() {
        assert_eq!(plain_string("\"a/b.rs\"").as_deref(), Some("a/b.rs"));
        assert_eq!(plain_string("r##\"a\"#b\"##").as_deref(), Some("a\"#b"));
        assert_eq!(plain_string("\"\\x2e\\x2e/x.rs\""), None);
        assert_eq!(plain_string("b\"x.rs\""), None);
        assert_eq!(
            literal_text("\"Acme\\x52ocket\"").as_deref(),
            Some("AcmeRocket")
        );
        assert_eq!(
            literal_text("\"a\\u{2f}b\\\n    c\"").as_deref(),
            Some("a/bc")
        );
        assert_eq!(literal_text("br#\"q\"#").as_deref(), Some("q"));
        assert_eq!(literal_text("b'x'").as_deref(), Some("x"));
        assert_eq!(literal_text("\"\\q\""), None);
    }
}
