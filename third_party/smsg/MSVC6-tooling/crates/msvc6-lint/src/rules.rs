use crate::lex::{self, Tok, Token};
use std::fmt;
use std::path::Path;

pub const SRCIO_FILE: &str = "srcio.rs";

pub const SRCIO_LINT: &str = "disallowed_methods";

pub const SRCIO_LINT_STEM: &str = "disallowed_method";

pub const SILENCING_GROUPS: [&str; 3] = ["all", "style", "restriction"];

pub const SILENCING_LINTS: [&str; 2] = ["warnings", "renamed_and_removed_lints"];

pub const INCLUDE_MACROS: [&str; 3] = ["include", "include_str", "include_bytes"];

pub const CHECKED_MACROS: [&str; 2] = ["include", "include_str"];

pub const DATA_MACROS: [&str; 2] = ["include_str", "include_bytes"];

pub const PRELUDE_ROOTS: [&str; 3] = ["std", "core", "alloc"];

fn imported_includes(toks: &[Token]) -> Vec<(usize, usize, String)> {
    let mut out = Vec::new();
    for (k, t) in toks.iter().enumerate() {
        if !t.is_ident("use") {
            continue;
        }
        let tree: Vec<&Token> = toks
            .get(k + 1..)
            .unwrap_or_default()
            .iter()
            .take_while(|n| !n.is_punct(';'))
            .collect();
        let root = tree.iter().find(|n| !n.is_punct(':'));
        if !root.is_some_and(|r| PRELUDE_ROOTS.iter().any(|p| r.is_ident(p))) {
            continue;
        }
        for n in &tree {
            if let Some(m) = INCLUDE_MACROS.iter().find(|m| n.is_ident(m)) {
                out.push((n.line, n.col, (*m).to_string()));
            }
        }
    }
    out
}

fn shadowed_includes(toks: &[Token]) -> Vec<(usize, usize, &'static str, &'static str)> {
    let mut out = Vec::new();
    for (k, t) in toks.iter().enumerate() {
        let (how, at) =
            if t.is_ident("macro_rules") && toks.get(k + 1).is_some_and(|n| n.is_punct('!')) {
                ("macro_rules!", k + 2)
            } else if t.is_ident("as") {
                ("use ... as", k + 1)
            } else {
                continue;
            };
        if let Some(n) = toks.get(at)
            && let Some(m) = INCLUDE_MACROS.iter().find(|m| n.is_ident(m))
        {
            out.push((n.line, n.col, how, *m));
        }
    }
    out
}

fn named_by_a_macro_argument(body: &[Token]) -> bool {
    body.iter().enumerate().any(|(k, t)| {
        t.is_punct('$')
            && (k == 0
                || body
                    .get(k - 1)
                    .is_some_and(|p| p.is_punct('(') || p.is_punct(',')))
    })
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Rule {
    Comment,
    Unreadable,
    Include,
    Srcio,
    Config,
    Cache,
}

impl Rule {
    #[must_use]
    pub fn header(self) -> &'static str {
        match self {
            Self::Comment => {
                "SOURCE RULE: {whose} does not build with a comment in its Rust source"
            }
            Self::Unreadable => {
                "SOURCE RULE: {whose} does not build with a Rust source the rules cannot read"
            }
            Self::Include => {
                "SOURCE RULE: {whose} does not build with a module or include that reaches past its checked .rs files"
            }
            Self::Srcio => {
                "SOURCE RULE: {whose} does not build with clippy::disallowed_methods silenced outside a srcio.rs"
            }
            Self::Config => "CONFIG RULE: {whose} does not build with these config files",
            Self::Cache => {
                "TREE RULE: {whose} does not build with a directory hidden from its checks"
            }
        }
    }

    #[must_use]
    pub fn footer(self) -> &'static str {
        match self {
            Self::Comment => {
                "The source is comment-free: help text belongs in clap attributes, rationale in the code's names."
            }
            Self::Unreadable => "A token the lexer cannot close is a refusal, not a pass.",
            Self::Include => {
                "#[path] and include! name a .rs file inside the crate, so the comment and format checks read \
                 every line it compiles. include_bytes! takes a plain literal naming no .rs file, which dep-info \
                 would list as compiled, and include_str! is refused: of a .rs file for that reason, of any other \
                 as text no check reads. No macro_rules! or `as` names one of the three, which would shadow the \
                 builtin."
            }
            Self::Srcio => {
                "Read and write files through srcio: only a srcio.rs may allow the strict and lossy reads \
                 that clippy.toml denies."
            }
            Self::Config => {
                "Configs are comment-free; rustfmt and clippy read only the root rustfmt.toml and clippy.toml; \
                 lint levels come from [workspace.lints]."
            }
            Self::Cache => {
                "Only target/ and the lint's own build directory are skipped, and only when cargo's signature \
                 heads their CACHEDIR.TAG."
            }
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Refusal {
    pub file: String,
    pub line: usize,
    pub col: usize,
    pub what: String,
    pub rule: Rule,
}

impl fmt::Display for Refusal {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}:{}: {}", self.file, self.line, self.col, self.what)
    }
}

fn is_srcio(file: &str) -> bool {
    Path::new(file).file_name().is_some_and(|n| n == SRCIO_FILE)
}

fn parent_of(file: &str) -> &str {
    file.rsplit_once('/').map_or("", |(d, _)| d)
}

#[must_use]
pub fn resolve(base: &str, rel: &str) -> Option<Vec<String>> {
    let mut parts: Vec<String> = base
        .split('/')
        .filter(|s| !s.is_empty() && *s != ".")
        .map(str::to_string)
        .collect();
    for seg in rel.split('/') {
        match seg {
            "" | "." => {}
            ".." => {
                parts.pop()?;
            }
            s => parts.push(s.to_string()),
        }
    }
    Some(parts)
}

const NOT_PLAIN: &str = "whose value is not a plain string literal: write the path plainly";

#[must_use]
pub fn target_problem(file: &str, crate_dir: &str, value: Option<&str>) -> Option<String> {
    let Some(v) = value else {
        return Some(NOT_PLAIN.to_string());
    };
    if !v.ends_with(".rs") {
        return Some(format!(
            "of `{v}`, which is not a .rs file: every file the crate compiles is a checked .rs file"
        ));
    }
    let bound: Vec<&str> = crate_dir.split('/').filter(|s| !s.is_empty()).collect();
    let inside = !v.starts_with('/')
        && !v.contains('\\')
        && !v.contains(':')
        && resolve(parent_of(file), v)
            .is_some_and(|t| t.len() > bound.len() && t.iter().zip(&bound).all(|(a, b)| a == b));
    (!inside).then(|| {
        format!(
            "of `{v}`, which leaves {}: a crate compiles only the checked files inside it",
            if crate_dir.is_empty() {
                "the tree"
            } else {
                crate_dir
            }
        )
    })
}

#[must_use]
pub fn data_problem(value: Option<&str>) -> Option<String> {
    match value {
        None => Some(NOT_PLAIN.to_string()),
        Some(v) if v.ends_with(".rs") => Some(format!(
            "of `{v}`, which reads a .rs file as data: rustc's dep-info lists it as a compiled source \
             though no crate compiles it"
        )),
        Some(_) => None,
    }
}

fn include_problem(file: &str, crate_dir: &str, name: &str, value: Option<&str>) -> Option<String> {
    if DATA_MACROS.contains(&name)
        && let Some(p) = data_problem(value)
    {
        return Some(p);
    }
    if CHECKED_MACROS.contains(&name) {
        target_problem(file, crate_dir, value)
    } else {
        None
    }
}

fn opens(t: Option<&Token>) -> bool {
    t.is_some_and(|t| matches!(t.tok, Tok::Punct('(' | '[' | '{')))
}

fn lit_value(t: Option<&Token>) -> Option<String> {
    match &t?.tok {
        Tok::Lit(l) => lex::plain_string(l),
        _ => None,
    }
}

fn includes(toks: &[Token]) -> Vec<(usize, usize, &'static str, Option<String>)> {
    let mut out = Vec::new();
    for (k, t) in toks.iter().enumerate() {
        let Some(name) = INCLUDE_MACROS.iter().find(|m| t.is_ident(m)) else {
            continue;
        };
        if !toks.get(k + 1).is_some_and(|n| n.is_punct('!')) || !opens(toks.get(k + 2)) {
            continue;
        }
        let close = lex::group_end(toks, k + 2);
        let args = toks.get(k + 3..close).unwrap_or_default();
        let single = match args {
            [a] => Some(a),
            [a, c] if c.is_punct(',') => Some(a),
            _ => None,
        };
        out.push((t.line, t.col, *name, lit_value(single)));
    }
    out
}

fn ident_of(t: &Token) -> &str {
    match &t.tok {
        Tok::Ident(n) => n,
        _ => "",
    }
}

fn silencers(body: &[Token]) -> Vec<(usize, usize, String)> {
    let mut out = Vec::new();
    for (k, t) in body.iter().enumerate() {
        let lint_attr = t.is_ident("allow") || t.is_ident("expect");
        if !lint_attr || !body.get(k + 1).is_some_and(|n| n.is_punct('(')) {
            continue;
        }
        let close = lex::group_end(body, k + 1);
        let group = body.get(k + 2..close).unwrap_or_default();
        for (j, g) in group.iter().enumerate() {
            let back = |n: usize| j.checked_sub(n).and_then(|p| group.get(p));
            let after_path = back(1).is_some_and(|p| p.is_punct(':'));
            let clippy_group = after_path
                && back(2).is_some_and(|c| c.is_punct(':'))
                && back(3).is_some_and(|c| c.is_ident("clippy"))
                && SILENCING_GROUPS.iter().any(|s| g.is_ident(s));
            if clippy_group {
                out.push((g.line, g.col, format!("clippy::{}", ident_of(g))));
            } else if !after_path && SILENCING_LINTS.iter().any(|s| g.is_ident(s)) {
                out.push((g.line, g.col, ident_of(g).to_string()));
            }
        }
    }
    out
}

fn blank_first_line(src: &str) -> String {
    let end = src.find('\n').unwrap_or(src.len());
    let (first, rest) = src.split_at(end);
    let mut out: String = first.chars().map(|_| ' ').collect();
    out.push_str(rest);
    out
}

#[must_use]
pub fn check_source(file: &str, src: &str) -> Vec<Refusal> {
    check_source_in(file, src, parent_of(file))
}

#[must_use]
pub fn check_source_in(file: &str, src: &str, crate_dir: &str) -> Vec<Refusal> {
    let refusal = |(line, col): (usize, usize), rule: Rule, what: String| Refusal {
        file: file.to_string(),
        line,
        col,
        what,
        rule,
    };
    let mut out = Vec::new();
    let unshebanged;
    let src = if lex::shebang(src) {
        out.push(refusal(
            (1, 1),
            Rule::Comment,
            "a shebang line, which is a comment by another spelling".to_string(),
        ));
        unshebanged = blank_first_line(src);
        unshebanged.as_str()
    } else {
        src
    };
    let (lexed, toks) = match lex::lex(src).and_then(|l| lex::tokens(src).map(|t| (l, t))) {
        Ok(x) => x,
        Err(e) => {
            out.push(refusal(
                (e.line, e.col),
                Rule::Unreadable,
                format!("cannot be read as Rust: {}", e.what),
            ));
            return out;
        }
    };
    out.extend(lexed.comments.iter().map(|c| {
        refusal(
            (c.line, c.col),
            Rule::Comment,
            format!("{} `{}`", c.describe(), c.text),
        )
    }));
    for at in lex::doc_attributes(&lexed.code) {
        out.push(refusal(
            at,
            Rule::Comment,
            "a `#[doc = ...]` attribute, which is a comment by another spelling".to_string(),
        ));
    }
    let srcio = is_srcio(file);
    for a in lex::attributes(&toks) {
        if named_by_a_macro_argument(&a.body) {
            out.push(refusal(
                (a.line, a.col),
                Rule::Include,
                "an attribute a macro argument names (`$...`), which could spell `doc`, `path` or a lint \
                 level the rules would not read"
                    .to_string(),
            ));
        }
        for k in lex::keyed(&a.body, "path") {
            let value = lit_value(a.body.get(k + 2));
            if let Some(p) = target_problem(file, crate_dir, value.as_deref()) {
                out.push(refusal(
                    (a.line, a.col),
                    Rule::Include,
                    format!("a `#[path]` {p}"),
                ));
            }
        }
        if !srcio {
            for (line, col, what) in silencers(&a.body) {
                out.push(refusal(
                    (line, col),
                    Rule::Srcio,
                    format!(
                        "allows {what} outside a {SRCIO_FILE}, which silences clippy::{SRCIO_LINT}"
                    ),
                ));
            }
        }
    }
    for (line, col, name, value) in includes(&toks) {
        if let Some(p) = include_problem(file, crate_dir, name, value.as_deref()) {
            out.push(refusal(
                (line, col),
                Rule::Include,
                format!("an `{name}!` {p}"),
            ));
        }
    }
    for (line, col, name) in imported_includes(&toks) {
        out.push(refusal(
            (line, col),
            Rule::Include,
            format!("a `use` of {name}, whose calls by another name the rules would not read"),
        ));
    }
    for (line, col, how, name) in shadowed_includes(&toks) {
        out.push(refusal(
            (line, col),
            Rule::Include,
            format!("a `{how}` named `{name}`, which shadows the builtin `{name}!` the rules read"),
        ));
    }
    if !srcio {
        for at in lex::mentions(&lexed.code.replace('-', "_"), SRCIO_LINT_STEM) {
            out.push(refusal(
                at,
                Rule::Srcio,
                format!(
                    "names clippy::{SRCIO_LINT} (or its old name {SRCIO_LINT_STEM}) outside a {SRCIO_FILE}: \
                     read and write files through srcio"
                ),
            ));
        }
    }
    out.sort_by_key(|r| (r.line, r.col));
    out
}

#[must_use]
pub fn crate_dir_of(root: &Path, path: &Path) -> String {
    let mut d = path.parent();
    while let Some(dir) = d {
        if dir == root || !dir.starts_with(root) {
            break;
        }
        if dir.join("Cargo.toml").is_file() {
            return crate::tree::relative(root, dir);
        }
        d = dir.parent();
    }
    path.parent()
        .map(|p| crate::tree::relative(root, p))
        .unwrap_or_default()
}

#[must_use]
pub fn check_file(root: &Path, path: &Path) -> Vec<Refusal> {
    let shown = crate::tree::relative(root, path);
    match crate::srcio::own_read(path) {
        Ok(text) => check_source_in(&shown, &text, &crate_dir_of(root, path)),
        Err(e) => vec![Refusal {
            file: shown,
            line: 1,
            col: 1,
            what: format!("cannot be read as UTF-8 Rust: {e}"),
            rule: Rule::Unreadable,
        }],
    }
}

#[must_use]
pub fn check_files(root: &Path, files: &[std::path::PathBuf]) -> Vec<Refusal> {
    files.iter().flat_map(|p| check_file(root, p)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn shown(r: &[Refusal]) -> Vec<String> {
        r.iter().map(ToString::to_string).collect()
    }

    #[test]
    fn a_comment_of_any_form_is_refused_with_its_position() {
        let r = check_source("src/a.rs", "fn a() {}\n/// doc\nfn b() {} // tail\n");
        assert_eq!(
            shown(&r),
            [
                "src/a.rs:2:1: a doc comment `/// doc`",
                "src/a.rs:3:11: a line comment `// tail`"
            ]
        );
        assert!(r.iter().all(|x| x.rule == Rule::Comment));
    }

    #[test]
    fn a_doc_attribute_is_refused() {
        let r = check_source("b.rs", "#[doc = \"x\"]\npub fn f() {}\n");
        assert_eq!(r.len(), 1);
        assert!(r[0].what.contains("#[doc = ...]"), "{r:?}");
    }

    #[test]
    fn a_doc_attribute_in_cfg_attr_or_by_raw_name_is_refused() {
        for src in [
            "#[cfg_attr(all(), doc = \"a third spelling\")]\npub fn f() {}\n",
            "#[cfg_attr(any(), cfg_attr(all(), doc = \"deeper\"))]\npub fn f() {}\n",
            "#[r#doc = \"raw-identifier doc attribute\"]\npub fn f() {}\n",
            "#![cfg_attr(all(), r#doc = \"inner\")]\n",
        ] {
            let r = check_source("le.rs", src);
            assert_eq!(r.len(), 1, "{src}: {r:?}");
            assert_eq!(r[0].rule, Rule::Comment, "{src}");
        }
        let hidden = "#[doc(hidden)]\n#[cfg_attr(doc, doc(hidden))]\npub fn f() {}\n";
        assert!(check_source("le.rs", hidden).is_empty());
    }

    #[test]
    fn a_shebang_line_is_refused_and_an_inner_attribute_is_not() {
        let r = check_source(
            "le.rs",
            "#!this whole line is a comment rustc drops as a shebang\npub fn f() {}\n",
        );
        assert_eq!(
            shown(&r),
            ["le.rs:1:1: a shebang line, which is a comment by another spelling"]
        );
        let r = check_source("le.rs", "#!it's got \"quotes\nfn f() {}\n");
        assert_eq!(r.len(), 1, "{r:?}");
        assert!(check_source("le.rs", "#![allow(dead_code)]\npub fn f() {}\n").is_empty());
    }

    #[test]
    fn the_srcio_allowance_is_refused_outside_srcio() {
        let allow = "#![allow(clippy::disallowed_methods)]\npub fn f() {}\n";
        let r = check_source("crates/x/src/io.rs", allow);
        assert_eq!(r.len(), 1);
        assert_eq!(r[0].rule, Rule::Srcio);
        assert!(check_source("crates/x/src/srcio.rs", allow).is_empty());
        let quoted = "pub const S: &str = \"clippy::disallowed_methods\";\n";
        assert!(check_source("crates/x/src/io.rs", quoted).is_empty());
    }

    #[test]
    fn a_group_allow_that_silences_the_srcio_lint_is_refused_outside_srcio() {
        for src in [
            "#[allow(clippy::style)]\npub fn f() {}\n",
            "#[expect(clippy::all)]\npub fn f() {}\n",
            "#![allow(clippy :: restriction)]\n",
            "#[cfg_attr(all(), allow(unused, clippy::style))]\npub fn f() {}\n",
            "#[allow(warnings)]\npub fn f() {}\n",
            "#[r#allow(clippy::r#all)]\npub fn f() {}\n",
        ] {
            let r = check_source("crates/x/src/le.rs", src);
            assert_eq!(r.len(), 1, "{src}: {r:?}");
            assert_eq!(r[0].rule, Rule::Srcio, "{src}");
            assert!(
                check_source("crates/x/src/srcio.rs", src).is_empty(),
                "{src}"
            );
        }
        for src in [
            "#[allow(clippy::allow_attributes)]\npub fn f() {}\n",
            "#[deny(clippy::all)]\npub fn f() {}\n",
            "#[allow(unused, clippy::cast_possible_truncation)]\npub fn f() {}\n",
            "#[allow(rustdoc::all)]\npub fn f() {}\n",
        ] {
            assert!(check_source("crates/x/src/le.rs", src).is_empty(), "{src}");
        }
    }

    #[test]
    fn the_srcio_lint_under_its_old_or_hyphenated_name_is_refused_outside_srcio() {
        let pair =
            "#[allow(renamed_and_removed_lints, clippy::disallowed_method)]\npub fn f() {}\n";
        let r = check_source("crates/x/src/le.rs", pair);
        assert_eq!(r.len(), 2, "{r:?}");
        assert!(r.iter().all(|x| x.rule == Rule::Srcio), "{r:?}");
        assert!(
            r[0].what.contains("allows renamed_and_removed_lints"),
            "{r:?}"
        );
        assert!(r[1].what.contains("names clippy::"), "{r:?}");
        assert!(check_source("crates/x/src/srcio.rs", pair).is_empty());
        for src in [
            "#[expect(renamed_and_removed_lints)]\npub fn f() {}\n",
            "#![allow(clippy::disallowed_method)]\n",
            "#![allow(clippy::disallowed-methods)]\n",
        ] {
            let r = check_source("crates/x/src/le.rs", src);
            assert_eq!(r.len(), 1, "{src}: {r:?}");
            assert_eq!(r[0].rule, Rule::Srcio, "{src}");
        }
        let quoted = "pub const S: &str = \"clippy::disallowed-method\";\n";
        assert!(check_source("crates/x/src/le.rs", quoted).is_empty());
    }

    #[test]
    fn a_module_or_include_must_name_a_rs_file_inside_the_crate() {
        for src in [
            "#[path = \"deep/hidden.rs\"]\nmod hidden;\n",
            "#[path = \"../tests/common.rs\"]\nmod common;\n",
            "include!(r\"gen.rs\");\n",
        ] {
            let r = check_source_in("crates/x/src/lib.rs", src, "crates/x");
            assert!(r.is_empty(), "{src}: {r:?}");
        }
        for (src, why) in [
            (
                "#[path = \"hidden.inc\"]\npub mod hidden;\n",
                "not a .rs file",
            ),
            (
                "#[path = \"../../../outside/hidden.rs\"]\nmod h;\n",
                "leaves crates/x",
            ),
            (
                "#[path = \"../../../../vfy-target/outside/hidden.rs\"]\nmod h;\n",
                "leaves crates/x",
            ),
            (
                "#[path = \"../../../design/planted_mod.rs\"]\nmod h;\n",
                "leaves crates/x",
            ),
            ("#[path = \"/tmp/x.rs\"]\nmod h;\n", "leaves crates/x"),
            (
                "#[path = \"\\x2e\\x2e/\\x2e\\x2e/y.rs\"]\nmod h;\n",
                "not a plain string",
            ),
            (
                "#[cfg_attr(all(), path = \"x.inc\")]\nmod h;\n",
                "not a .rs file",
            ),
            ("include!(\"planted_inc.txt\");\n", "not a .rs file"),
            (
                "const S: &str = include_str!(\"../../../README.md\");\n",
                "not a .rs file",
            ),
            (
                "include!(concat!(env!(\"OUT_DIR\"), \"/x.rs\"));\n",
                "not a plain string",
            ),
            (
                "const S: &str = std::include_str!(\"../../x.rs\");\n",
                "reads a .rs file as data",
            ),
        ] {
            let r = check_source_in("crates/x/src/lib.rs", src, "crates/x");
            assert_eq!(r.len(), 1, "{src}: {r:?}");
            assert_eq!(r[0].rule, Rule::Include, "{src}");
            assert!(r[0].what.contains(why), "{src}: {r:?}");
        }
        let r = check_source_in(
            "crates/x/build.rs",
            "#[path = \"src/a.rs\"]\nmod a;\n",
            "crates/x",
        );
        assert!(r.is_empty(), "{r:?}");
    }

    #[test]
    fn an_include_imported_by_another_name_or_an_attribute_a_macro_argument_names_is_refused() {
        for (src, what) in [
            (
                "use core::include as inc;\ninc!(\"x.txt\");\n",
                "a `use` of include",
            ),
            (
                "use ::std::{include_str as s, fmt};\n",
                "a `use` of include_str",
            ),
            (
                "use core::include_bytes as b;\nconst B: &[u8] = b!(\"y.rs\");\n",
                "a `use` of include_bytes",
            ),
            (
                "macro_rules! m { ($n:ident) => { #[$n = \"x.rs\"] mod y; }; }\n",
                "an attribute a macro argument names",
            ),
            (
                "macro_rules! m { ($l:path) => { #[cfg_attr(all(), $l)] pub fn f() {} }; }\n",
                "an attribute a macro argument names",
            ),
        ] {
            let r = shown(&check_source("crates/a/src/lib.rs", src));
            assert!(r.iter().any(|l| l.contains(what)), "{src}: {r:?}");
        }
        for ok in [
            "use crate::include;\nuse msvc6_link::{include, joint};\n",
            "use crate::include_bytes as b;\n",
            "macro_rules! m { ($x:expr) => { #[inline] pub fn f() -> u8 { $x } }; }\n",
        ] {
            let r = shown(&check_source("crates/a/src/lib.rs", ok));
            assert!(r.is_empty(), "{ok}: {r:?}");
        }
    }

    #[test]
    fn a_macro_or_an_alias_that_shadows_an_include_builtin_is_refused() {
        for (src, what) in [
            (
                "macro_rules! include {\n    ($p:literal) => {\n        0\n    };\n}\n",
                "a `macro_rules!` named `include`",
            ),
            (
                "macro_rules! include_str {\n    () => {\n        \"\"\n    };\n}\n",
                "a `macro_rules!` named `include_str`",
            ),
            (
                "macro_rules! r#include_bytes {\n    () => {};\n}\n",
                "a `macro_rules!` named `include_bytes`",
            ),
            (
                "use g::include_bytes as include_str;\n",
                "a `use ... as` named `include_str`",
            ),
            (
                "use x::{y as include_bytes, z};\n",
                "a `use ... as` named `include_bytes`",
            ),
            (
                "pub use self::m as include;\n",
                "a `use ... as` named `include`",
            ),
        ] {
            let r = check_source("crates/a/src/lib.rs", src);
            assert!(
                r.iter()
                    .any(|x| x.rule == Rule::Include && x.what.starts_with(what)),
                "{src}: {r:?}"
            );
        }
        for ok in [
            "macro_rules! included {\n    () => {};\n}\n",
            "use x::include as inc2;\n",
            "pub fn f(v: &[u8]) -> u64 {\n    v.len() as u64\n}\n",
        ] {
            let r = shown(&check_source("crates/a/src/lib.rs", ok));
            assert!(r.is_empty(), "{ok}: {r:?}");
        }
    }

    #[test]
    fn an_include_str_or_include_bytes_of_a_rs_file_is_refused_as_a_source_no_crate_compiles() {
        for src in [
            "const S: &str = include_str!(\"y.rs\");\n",
            "const S: &str = include_str!(r\"y.rs\");\n",
            "const S: &str = core::include_str!(\"../tests/y.rs\");\n",
            "const B: &[u8] = include_bytes!(\"y.rs\");\n",
            "const B: &[u8] = ::std::include_bytes!(\"sub/y.rs\",);\n",
            "const B: &[u8] = include_bytes!(\"../../../fixtures/y.rs\");\n",
        ] {
            let r = check_source_in("crates/x/src/lib.rs", src, "crates/x");
            assert_eq!(r.len(), 1, "{src}: {r:?}");
            assert_eq!(r[0].rule, Rule::Include, "{src}");
            assert!(
                r[0].what.contains("which reads a .rs file as data"),
                "{src}: {r:?}"
            );
        }
        for src in [
            "const B: &[u8] = include_bytes!(concat!(\"y\", \".rs\"));\n",
            "macro_rules! m { ($p:literal) => { include_bytes!($p) }; }\n",
        ] {
            let r = check_source_in("crates/x/src/lib.rs", src, "crates/x");
            assert_eq!(r.len(), 1, "{src}: {r:?}");
            assert!(r[0].what.contains("not a plain string"), "{src}: {r:?}");
        }
        let r = check_source_in(
            "crates/x/src/lib.rs",
            "const B: &[u8] = include_bytes!(\"table.bin\");\n",
            "crates/x",
        );
        assert!(r.is_empty(), "{r:?}");
    }

    #[test]
    fn an_unlexable_file_is_refused_not_passed() {
        let r = check_source("c.rs", "fn f() { let s = \"open; }\n");
        assert_eq!(r.len(), 1);
        assert!(r[0].what.starts_with("cannot be read as Rust"), "{r:?}");
        assert_eq!(r[0].rule, Rule::Unreadable);
    }

    #[test]
    fn a_clean_file_passes() {
        assert!(check_source("d.rs", "pub fn f(x: &str) -> &str {\n    x\n}\n").is_empty());
        let literals = "pub const RAW: &str = r#\"a // b /* c */ \"quoted\" d\"#;\n\
                        pub const U: &str = \"http://example.invalid/*x*/ // y\";\n\
                        pub const Q: char = '\"';\npub const B: &[u8] = br##\"a \"# // b\"##;\n";
        assert!(check_source("d.rs", literals).is_empty());
    }
}
