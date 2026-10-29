use crate::jsonread::Json;
use crate::lex::{self, Tok, Token};
use crate::tomlread::{self, Entry, Value};
use std::collections::BTreeSet;
use std::path::{Component, Path, PathBuf};

pub const MAX_NESTING: usize = 64;

pub type Unit = Option<bool>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Root {
    pub path: PathBuf,
    pub kind: String,
    pub units: Vec<Unit>,
}

fn table_of(kind: &str) -> Option<&'static str> {
    match kind {
        "custom-build" => None,
        "bin" => Some("bin"),
        "test" => Some("test"),
        "bench" => Some("bench"),
        "example" => Some("example"),
        _ => Some("lib"),
    }
}

fn bool_field(fields: &[(String, &Value)], name: &str) -> Option<bool> {
    fields
        .iter()
        .find(|(k, _)| k == name)
        .and_then(|(_, v)| match v {
            Value::Bool(b) => Some(*b),
            _ => None,
        })
}

fn key_of(e: &Entry) -> Vec<&str> {
    e.key.iter().map(String::as_str).collect()
}

#[must_use]
pub fn harness(entries: &[Entry], kind: &str, name: &str) -> Option<bool> {
    let table = table_of(kind)?;
    if table == "lib" {
        return entries
            .iter()
            .find(|e| !e.header && key_of(e) == ["lib", "harness"])
            .and_then(|e| match e.value {
                Value::Bool(b) => Some(b),
                _ => None,
            });
    }
    let mut groups: Vec<Vec<(String, &Value)>> = Vec::new();
    let mut open = false;
    for e in entries {
        let k = key_of(e);
        if e.header {
            open = k == [table];
            if open {
                groups.push(Vec::new());
            }
            continue;
        }
        match k.as_slice() {
            [t, field] if open && *t == table => {
                if let Some(g) = groups.last_mut() {
                    g.push(((*field).to_string(), &e.value));
                }
            }
            [t] if *t == table => {
                if let Value::Array(items) = &e.value {
                    for item in items {
                        if let Value::Table(fields) = item {
                            groups.push(
                                fields
                                    .iter()
                                    .filter_map(|(k, v)| match k.as_slice() {
                                        [f] => Some((f.clone(), v)),
                                        _ => None,
                                    })
                                    .collect(),
                            );
                        }
                    }
                }
            }
            _ => {}
        }
    }
    groups
        .iter()
        .find(|g| {
            g.iter()
                .any(|(k, v)| k == "name" && matches!(v, Value::Str(s) if s == name))
        })
        .and_then(|g| bool_field(g, "harness"))
}

#[must_use]
pub fn units(kind: &str, test: bool, harness: Option<bool>) -> Vec<Unit> {
    let h = harness.unwrap_or(true);
    match kind {
        "custom-build" => vec![Some(false)],
        "test" => vec![Some(h)],
        "bench" => vec![None],
        _ if test => vec![Some(false), Some(h)],
        _ => vec![Some(false)],
    }
}

pub fn roots(metadata: &Json) -> Result<Vec<Root>, String> {
    let mut out = Vec::new();
    for p in metadata
        .get("packages")
        .map(Json::items)
        .unwrap_or_default()
    {
        let manifest = p
            .get("manifest_path")
            .and_then(Json::as_str)
            .ok_or("cargo metadata lists a package with no manifest_path")?;
        let text =
            crate::srcio::own_read(manifest).map_err(|e| format!("reading {manifest}: {e}"))?;
        let entries = tomlread::parse(&text).map_err(|e| format!("{manifest}:{e}"))?;
        for t in p.get("targets").map(Json::items).unwrap_or_default() {
            let kind = t
                .get("kind")
                .and_then(|k| k.items().first())
                .and_then(Json::as_str)
                .ok_or_else(|| format!("{manifest}: a target with no kind"))?;
            let name = t.get("name").and_then(Json::as_str).unwrap_or_default();
            let src = t
                .get("src_path")
                .and_then(Json::as_str)
                .ok_or_else(|| format!("{manifest}: target {name} has no src_path"))?;
            let test = t.get("test").and_then(Json::as_bool).unwrap_or(false);
            out.push(Root {
                path: normal(Path::new(src)),
                kind: kind.to_string(),
                units: units(kind, test, harness(&entries, kind, name)),
            });
        }
    }
    if out.is_empty() {
        return Err("cargo metadata lists no target".to_string());
    }
    Ok(out)
}

#[must_use]
pub fn normal(p: &Path) -> PathBuf {
    let mut out = PathBuf::new();
    for c in p.components() {
        match c {
            Component::CurDir => {}
            Component::ParentDir => match out.components().next_back() {
                Some(Component::Normal(_)) => {
                    out.pop();
                }
                Some(Component::RootDir | Component::Prefix(_)) => {}
                _ => out.push(".."),
            },
            c => out.push(c),
        }
    }
    out
}

fn parent(p: &Path) -> PathBuf {
    p.parent().map(Path::to_path_buf).unwrap_or_default()
}

fn opens(t: Option<&Token>) -> bool {
    t.is_some_and(|t| matches!(t.tok, Tok::Punct('(' | '[' | '{')))
}

fn args(toks: &[Token]) -> Vec<&[Token]> {
    let mut out = Vec::new();
    let mut depth = 0usize;
    let mut start = 0usize;
    for (k, t) in toks.iter().enumerate() {
        match t.tok {
            Tok::Punct('(' | '[' | '{') => depth += 1,
            Tok::Punct(')' | ']' | '}') => depth = depth.saturating_sub(1),
            Tok::Punct(',') if depth == 0 => {
                out.push(toks.get(start..k).unwrap_or_default());
                start = k + 1;
            }
            _ => {}
        }
    }
    out.push(toks.get(start..).unwrap_or_default());
    out.retain(|a| !a.is_empty());
    out
}

fn settled(vals: impl Iterator<Item = Option<bool>>, decides: bool) -> Option<bool> {
    let mut unknown = false;
    for v in vals {
        match v {
            Some(b) if b == decides => return Some(decides),
            Some(_) => {}
            None => unknown = true,
        }
    }
    (!unknown).then_some(!decides)
}

#[must_use]
pub fn cfg_holds(pred: &[Token], test: Unit) -> Option<bool> {
    match pred {
        [t] if t.is_ident("test") => test,
        [f, open, .., close] if open.is_punct('(') && close.is_punct(')') => {
            if lex::group_end(pred, 1) + 1 != pred.len() {
                return None;
            }
            let inner = args(pred.get(2..pred.len() - 1).unwrap_or_default());
            if f.is_ident("all") {
                settled(inner.iter().map(|p| cfg_holds(p, test)), false)
            } else if f.is_ident("any") {
                settled(inner.iter().map(|p| cfg_holds(p, test)), true)
            } else if f.is_ident("not") {
                match inner.as_slice() {
                    [p] => cfg_holds(p, test).map(|v| !v),
                    _ => None,
                }
            } else {
                None
            }
        }
        _ => None,
    }
}

fn path_value(attrs: &[&[Token]]) -> Option<Option<String>> {
    attrs
        .iter()
        .find(|a| {
            a.first().is_some_and(|t| t.is_ident("path"))
                && a.get(1).is_some_and(|t| t.is_punct('='))
        })
        .map(|a| match (a.get(2), a.len()) {
            (
                Some(Token {
                    tok: Tok::Lit(l), ..
                }),
                3,
            ) => lex::plain_string(l),
            _ => None,
        })
}

fn ends_at_semicolon(t: &Token, next: Option<&Token>) -> bool {
    let fn_follows = next.is_some_and(|n| {
        ["fn", "unsafe", "async", "extern"]
            .iter()
            .any(|k| n.is_ident(k))
    });
    ["use", "type", "static"].iter().any(|k| t.is_ident(k)) || (t.is_ident("const") && !fn_follows)
}

fn leading_inner(toks: &[Token]) -> (Vec<&[Token]>, usize) {
    let mut out = Vec::new();
    let mut k = 0usize;
    while toks.get(k).is_some_and(|t| t.is_punct('#'))
        && toks.get(k + 1).is_some_and(|t| t.is_punct('!'))
        && toks.get(k + 2).is_some_and(|t| t.is_punct('['))
    {
        let close = lex::group_end(toks, k + 2);
        out.push(toks.get(k + 3..close).unwrap_or_default());
        k = close + 1;
    }
    (out, k)
}

pub const CFG_ATTR_DOUBT: &str = "a `cfg_attr` that sets a `cfg` or a `path`, so the module walk cannot tell which file a target compiles";

pub const CFG_DOUBT: &str = "a `cfg` the module walk cannot judge (it reads `test`, `all`, `any` and `not`), so \
                             it cannot tell whether a target compiles what it guards";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Verdict {
    Live(Vec<Unit>),
    Doubt(&'static str),
}

#[must_use]
pub fn verdict(attrs: &[&[Token]], units: &[Unit]) -> Verdict {
    let mut preds: Vec<&[Token]> = Vec::new();
    for a in attrs {
        let Some(first) = a.first() else { continue };
        if first.is_ident("cfg") && a.get(1).is_some_and(|t| t.is_punct('(')) {
            let close = lex::group_end(a, 1);
            preds.push(a.get(2..close).unwrap_or_default());
        } else if first.is_ident("cfg_attr")
            && a.iter()
                .skip(1)
                .any(|t| t.is_ident("cfg") || t.is_ident("cfg_attr") || t.is_ident("path"))
        {
            return Verdict::Doubt(CFG_ATTR_DOUBT);
        }
    }
    let mut live = Vec::new();
    let mut unknown = false;
    for &u in units {
        match settled(preds.iter().map(|p| cfg_holds(p, u)), false) {
            Some(true) => live.push(u),
            Some(false) => {}
            None => unknown = true,
        }
    }
    if unknown {
        Verdict::Doubt(CFG_DOUBT)
    } else {
        Verdict::Live(live)
    }
}

#[derive(Debug, Clone)]
struct Ctx {
    file: PathBuf,
    dir: PathBuf,
    relative: Option<String>,
    units: Vec<Unit>,
    doubt: Option<&'static str>,
    depth: usize,
}

#[derive(Debug, Default)]
pub struct Walk {
    pub modules: BTreeSet<PathBuf>,
    pub unseen: Vec<String>,
    root: PathBuf,
    real_root: Option<PathBuf>,
    seen: BTreeSet<(PathBuf, PathBuf, Option<String>, Vec<Unit>)>,
}

impl Walk {
    #[must_use]
    pub fn shown(&self, p: &Path) -> String {
        [Some(&self.root), self.real_root.as_ref()]
            .into_iter()
            .flatten()
            .find_map(|r| p.strip_prefix(r).ok())
            .unwrap_or(p)
            .to_string_lossy()
            .replace('\\', "/")
    }

    #[must_use]
    pub fn module_names(&self) -> BTreeSet<String> {
        self.modules.iter().map(|p| self.shown(p)).collect()
    }

    fn report(&mut self, file: &Path, at: (usize, usize), what: &str) {
        let line = format!("{}:{}:{}: {what}", self.shown(file), at.0, at.1);
        self.unseen.push(line);
    }

    fn judge(&mut self, attrs: &[&[Token]], ctx: &Ctx, at: (usize, usize)) -> Option<Vec<Unit>> {
        if let Some(d) = ctx.doubt {
            let what = format!("{d}, on a module that holds it");
            self.report(&ctx.file, at, &what);
            return None;
        }
        match verdict(attrs, &ctx.units) {
            Verdict::Live(u) => (!u.is_empty()).then_some(u),
            Verdict::Doubt(d) => {
                self.report(&ctx.file, at, d);
                None
            }
        }
    }

    fn file(
        &mut self,
        path: &Path,
        dir: PathBuf,
        relative: Option<String>,
        units: Vec<Unit>,
        from: (&Path, (usize, usize)),
        depth: usize,
    ) {
        let key = (
            path.to_path_buf(),
            dir.clone(),
            relative.clone(),
            units.clone(),
        );
        if !self.seen.insert(key) {
            return;
        }
        if depth > MAX_NESTING {
            self.report(
                from.0,
                from.1,
                "modules nested deeper than the module walk follows",
            );
            return;
        }
        let toks = match crate::srcio::own_read(path) {
            Ok(src) => match lex::tokens(&src) {
                Ok(t) => t,
                Err(e) => {
                    self.report(
                        path,
                        (e.line, e.col),
                        &format!("cannot be read as Rust by the module walk: {}", e.what),
                    );
                    return;
                }
            },
            Err(e) => {
                let what = format!(
                    "names {}, which the module walk cannot read ({e})",
                    self.shown(path)
                );
                self.report(from.0, from.1, &what);
                return;
            }
        };
        let (inner, k) = leading_inner(&toks);
        let mut ctx = Ctx {
            file: path.to_path_buf(),
            dir,
            relative,
            units,
            doubt: None,
            depth,
        };
        let Some(units) = self.judge(&inner, &ctx, (1, 1)) else {
            return;
        };
        self.modules.insert(path.to_path_buf());
        ctx.units = units;
        self.items(toks.get(k..).unwrap_or_default(), &ctx);
    }

    fn items(&mut self, toks: &[Token], ctx: &Ctx) {
        let mut attrs: Vec<&[Token]> = Vec::new();
        let mut head = true;
        let mut semi_only = false;
        let mut k = 0usize;
        while let Some(t) = toks.get(k) {
            let next = toks.get(k + 1);
            if t.is_punct('#') {
                let inner = next.is_some_and(|n| n.is_punct('!'));
                let open = if inner { k + 2 } else { k + 1 };
                if toks.get(open).is_some_and(|n| n.is_punct('[')) {
                    let close = lex::group_end(toks, open);
                    if !inner && head {
                        attrs.push(toks.get(open + 1..close).unwrap_or_default());
                    }
                    k = close + 1;
                    continue;
                }
            }
            if t.is_punct(';') {
                attrs.clear();
                head = true;
                semi_only = false;
                k += 1;
                continue;
            }
            if head && t.is_ident("pub") {
                k = if next.is_some_and(|n| n.is_punct('(')) {
                    lex::group_end(toks, k + 1) + 1
                } else {
                    k + 1
                };
                continue;
            }
            let path_segment = matches!(t.tok, Tok::Ident(_))
                && next.is_some_and(|n| n.is_punct(':'))
                && toks.get(k + 2).is_some_and(|n| n.is_punct(':'));
            if head && (t.is_punct(':') || path_segment) {
                k += 1;
                continue;
            }
            let at = (t.line, t.col);
            if head
                && t.is_ident("mod")
                && let Some(Token {
                    tok: Tok::Ident(name),
                    ..
                }) = next
            {
                if toks.get(k + 2).is_some_and(|n| n.is_punct(';')) {
                    self.outline(ctx, name, &attrs, at);
                    attrs.clear();
                    k += 3;
                    continue;
                }
                if toks.get(k + 2).is_some_and(|n| n.is_punct('{')) {
                    let close = lex::group_end(toks, k + 2);
                    self.inline(
                        ctx,
                        name,
                        &attrs,
                        toks.get(k + 3..close).unwrap_or_default(),
                        at,
                    );
                    attrs.clear();
                    k = close + 1;
                    continue;
                }
            }
            if t.is_ident("include")
                && next.is_some_and(|n| n.is_punct('!'))
                && opens(toks.get(k + 2))
            {
                let close = lex::group_end(toks, k + 2);
                if head {
                    self.include(ctx, toks.get(k + 3..close).unwrap_or_default(), &attrs, at);
                } else {
                    self.report(
                        &ctx.file,
                        at,
                        "an `include!` inside an item, which the module walk cannot place",
                    );
                }
                head = false;
                if toks.get(k + 2).is_some_and(|n| n.is_punct('{')) && !semi_only {
                    attrs.clear();
                    head = true;
                }
                k = close + 1;
                continue;
            }
            if opens(Some(t)) {
                let close = lex::group_end(toks, k);
                self.foreign(ctx, toks.get(k + 1..close).unwrap_or_default());
                head = false;
                if t.is_punct('{') && !semi_only {
                    attrs.clear();
                    head = true;
                }
                k = close + 1;
                continue;
            }
            if head {
                semi_only = ends_at_semicolon(t, next);
                head = false;
            }
            k += 1;
        }
    }

    fn foreign(&mut self, ctx: &Ctx, toks: &[Token]) {
        for (k, t) in toks.iter().enumerate() {
            let next = toks.get(k + 1);
            let what = if t.is_ident("mod") && next.is_some_and(|n| n.is_punct('$')) {
                "a `mod` a macro names, which the module walk cannot see"
            } else if t.is_ident("mod")
                && next.is_some_and(|n| matches!(n.tok, Tok::Ident(_)))
                && toks.get(k + 2).is_some_and(|n| n.is_punct(';'))
            {
                "an out-of-line `mod` inside a block or a macro, which the module walk cannot place"
            } else if t.is_ident("include")
                && next.is_some_and(|n| n.is_punct('!'))
                && opens(toks.get(k + 2))
            {
                "an `include!` inside a block or a macro, which the module walk cannot place"
            } else {
                continue;
            };
            self.report(&ctx.file, (t.line, t.col), what);
        }
    }

    fn outline(&mut self, ctx: &Ctx, name: &str, attrs: &[&[Token]], at: (usize, usize)) {
        let Some(units) = self.judge(attrs, ctx, at) else {
            return;
        };
        let from = (ctx.file.as_path(), at);
        match path_value(attrs) {
            Some(Some(v)) => {
                let file = normal(&ctx.dir.join(v));
                let dir = parent(&file);
                self.file(&file, dir, None, units, from, ctx.depth + 1);
            }
            Some(None) => {
                self.report(&ctx.file, at, "a `#[path]` whose value is not a plain string literal, which the module walk cannot follow");
            }
            None => {
                let base = match &ctx.relative {
                    Some(r) => ctx.dir.join(r),
                    None => ctx.dir.clone(),
                };
                let flat = base.join(format!("{name}.rs"));
                let nested = base.join(name).join("mod.rs");
                match (flat.is_file(), nested.is_file()) {
                    (true, false) => self.file(
                        &flat,
                        base,
                        Some(name.to_string()),
                        units,
                        from,
                        ctx.depth + 1,
                    ),
                    (false, true) => {
                        self.file(&nested, base.join(name), None, units, from, ctx.depth + 1)
                    }
                    (found, _) => {
                        let what = format!(
                            "`mod {name};` names {} {} and {}",
                            if found { "both" } else { "neither" },
                            self.shown(&flat),
                            self.shown(&nested)
                        );
                        self.report(&ctx.file, at, &what);
                    }
                }
            }
        }
    }

    fn inline(
        &mut self,
        ctx: &Ctx,
        name: &str,
        attrs: &[&[Token]],
        body: &[Token],
        at: (usize, usize),
    ) {
        let enter = |v: Verdict, units: &[Unit], doubt: Option<&'static str>| match v {
            Verdict::Live(u) => (!u.is_empty()).then_some((u, doubt)),
            Verdict::Doubt(d) => Some((units.to_vec(), Some(d))),
        };
        let Some((units, held)) = enter(verdict(attrs, &ctx.units), &ctx.units, ctx.doubt) else {
            return;
        };
        let dir = match path_value(attrs) {
            Some(Some(v)) => normal(&ctx.dir.join(v)),
            Some(None) => {
                self.report(&ctx.file, at, "a `#[path]` whose value is not a plain string literal, which the module walk cannot follow");
                return;
            }
            None => {
                let mut d = ctx.dir.clone();
                if let Some(r) = &ctx.relative {
                    d.push(r);
                }
                d.push(name);
                d
            }
        };
        let (inner, k) = leading_inner(body);
        let Some((units, held)) = enter(verdict(&inner, &units), &units, held) else {
            return;
        };
        let child = Ctx {
            file: ctx.file.clone(),
            dir,
            relative: None,
            units,
            doubt: held,
            depth: ctx.depth + 1,
        };
        self.items(body.get(k..).unwrap_or_default(), &child);
    }

    fn include(&mut self, ctx: &Ctx, args: &[Token], attrs: &[&[Token]], at: (usize, usize)) {
        let Some(units) = self.judge(attrs, ctx, at) else {
            return;
        };
        let value = match args {
            [
                Token {
                    tok: Tok::Lit(l), ..
                },
            ] => lex::plain_string(l),
            [
                Token {
                    tok: Tok::Lit(l), ..
                },
                c,
            ] if c.is_punct(',') => lex::plain_string(l),
            _ => None,
        };
        let Some(v) = value else {
            self.report(&ctx.file, at, "an `include!` whose value is not a plain string literal, which the module walk cannot follow");
            return;
        };
        let file = normal(&parent(&ctx.file).join(v));
        let dir = parent(&file);
        self.file(
            &file,
            dir,
            None,
            units,
            (ctx.file.as_path(), at),
            ctx.depth + 1,
        );
    }
}

#[must_use]
pub fn walk(root: &Path, roots: &[Root]) -> Walk {
    let mut w = Walk {
        root: root.to_path_buf(),
        real_root: std::fs::canonicalize(root).ok(),
        ..Walk::default()
    };
    for r in roots {
        let dir = parent(&r.path);
        w.file(
            &r.path,
            dir,
            None,
            r.units.clone(),
            (r.path.as_path(), (1, 1)),
            0,
        );
    }
    w
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Tree(PathBuf);

    impl Tree {
        fn new(tag: &str, files: &[(&str, &str)]) -> Self {
            let d = std::env::temp_dir()
                .join(format!("msvc6-lint-modtree-{tag}-{}", std::process::id()));
            if d.exists() {
                std::fs::remove_dir_all(&d).unwrap();
            }
            for (p, body) in files {
                let at = d.join(p);
                std::fs::create_dir_all(at.parent().unwrap()).unwrap();
                crate::srcio::own_write(&at, body).unwrap();
            }
            Self(d)
        }

        fn root(&self, p: &str, units: &[Unit]) -> Root {
            Root {
                path: self.0.join(p),
                kind: "lib".to_string(),
                units: units.to_vec(),
            }
        }
    }

    impl Drop for Tree {
        fn drop(&mut self) {
            std::fs::remove_dir_all(&self.0).ok();
        }
    }

    fn names(w: &Walk) -> Vec<String> {
        w.module_names().into_iter().collect()
    }

    const BOTH: [Unit; 2] = [Some(false), Some(true)];

    #[test]
    fn a_path_is_normalised_by_its_components_alone() {
        for (p, want) in [
            ("a/./b/../c.rs", "a/c.rs"),
            ("../../x.rs", "../../x.rs"),
            ("a/../../x.rs", "../x.rs"),
            ("/../x.rs", "/x.rs"),
            ("/t/./src/sub/../lib.rs", "/t/src/lib.rs"),
        ] {
            assert_eq!(normal(Path::new(p)), Path::new(want), "{p}");
        }
    }

    #[test]
    fn module_files_resolve_as_rustc_finds_them() {
        let t = Tree::new(
            "paths",
            &[
                (
                    "src/lib.rs",
                    "pub mod a;\npub mod b;\n#[path = \"p/q.rs\"]\npub mod q;\npub mod inl {\n    pub mod deep;\n    \
                     #[path = \"x.rs\"]\n    pub mod px;\n}\n#[path = \"pathdir\"]\npub mod pin {\n    pub mod z;\n}\n\
                     pub(crate) mod r#type;\nmod holder {\n    include!(\"../inc/code.rs\");\n}\n",
                ),
                (
                    "src/a.rs",
                    "pub mod child;\n#[path = \"sib.rs\"]\npub mod sib;\npub mod ia {\n    pub mod leaf;\n}\n",
                ),
                ("src/a/child.rs", ""),
                ("src/sib.rs", ""),
                ("src/a/ia/leaf.rs", ""),
                ("src/b/mod.rs", "pub mod c;\n"),
                ("src/b/c.rs", ""),
                ("src/p/q.rs", "pub mod r;\n"),
                ("src/p/r.rs", ""),
                ("src/inl/deep.rs", ""),
                ("src/inl/x.rs", ""),
                ("src/pathdir/z.rs", ""),
                ("src/type.rs", ""),
                ("inc/code.rs", "pub mod m;\n"),
                ("inc/m.rs", ""),
                ("src/notes.rs", "pub fn n() {}\n"),
                ("tests/t.rs", "mod common;\n"),
                ("tests/common/mod.rs", "pub mod toys;\n"),
                ("tests/common/toys.rs", ""),
                (
                    "build.rs",
                    "#[path = \"src/sib.rs\"]\nmod sib;\nfn main() {}\n",
                ),
            ],
        );
        let w = walk(
            &t.0,
            &[
                t.root("src/lib.rs", &BOTH),
                t.root("tests/t.rs", &[Some(true)]),
                t.root("build.rs", &[Some(false)]),
            ],
        );
        assert!(w.unseen.is_empty(), "{:?}", w.unseen);
        assert_eq!(
            names(&w),
            [
                "build.rs",
                "inc/code.rs",
                "inc/m.rs",
                "src/a.rs",
                "src/a/child.rs",
                "src/a/ia/leaf.rs",
                "src/b/c.rs",
                "src/b/mod.rs",
                "src/inl/deep.rs",
                "src/inl/x.rs",
                "src/lib.rs",
                "src/p/q.rs",
                "src/p/r.rs",
                "src/pathdir/z.rs",
                "src/sib.rs",
                "src/type.rs",
                "tests/common/mod.rs",
                "tests/common/toys.rs",
                "tests/t.rs",
            ]
        );
    }

    #[test]
    fn a_cfg_admits_a_module_only_in_a_unit_it_holds_in_and_one_the_walk_cannot_judge_is_reported()
    {
        let t = Tree::new(
            "cfg",
            &[
                (
                    "src/lib.rs",
                    "#[cfg(test)]\nmod t;\n#[cfg(any())]\nmod dead;\n#[cfg(all(test, not(any())))]\nmod both;\n\
                     #[cfg(unix)]\nmod u;\n#[cfg_attr(test, path = \"x.rs\")]\nmod ca;\nmod inner_dead;\n\
                     #[cfg(test)]\nmod tests_inline {\n    mod nested;\n}\nmod plain;\n",
                ),
                ("src/t.rs", ""),
                ("src/dead.rs", ""),
                ("src/both.rs", ""),
                ("src/u.rs", ""),
                ("src/ca.rs", ""),
                ("src/x.rs", ""),
                ("src/inner_dead.rs", "#![cfg(any())]\npub fn f() {}\n"),
                ("src/tests_inline/nested.rs", ""),
                ("src/plain.rs", ""),
            ],
        );
        let w = walk(&t.0, &[t.root("src/lib.rs", &BOTH)]);
        assert_eq!(
            names(&w),
            [
                "src/both.rs",
                "src/lib.rs",
                "src/plain.rs",
                "src/t.rs",
                "src/tests_inline/nested.rs"
            ]
        );
        assert_eq!(w.unseen.len(), 2, "{:?}", w.unseen);
        assert!(
            w.unseen[0].starts_with("src/lib.rs:8:1: a `cfg` the module walk cannot judge"),
            "{:?}",
            w.unseen
        );
        assert!(
            w.unseen[1].starts_with("src/lib.rs:10:1: a `cfg_attr` that sets a `cfg` or a `path`"),
            "{:?}",
            w.unseen
        );
        let build = walk(&t.0, &[t.root("src/lib.rs", &[Some(false)])]);
        assert_eq!(names(&build), ["src/lib.rs", "src/plain.rs"]);
        let bench = walk(&t.0, &[t.root("src/lib.rs", &[None])]);
        assert_eq!(names(&bench), ["src/lib.rs", "src/plain.rs"]);
        assert_eq!(bench.unseen.len(), 5, "{:?}", bench.unseen);
    }

    #[test]
    fn a_cfg_some_units_cannot_judge_is_reported_at_its_own_module() {
        let t = Tree::new(
            "partial",
            &[
                (
                    "src/lib.rs",
                    "#[cfg(any(test, unix))]\nmod p4m;\n#[cfg(any(test, unix))]\nmod q {\n    mod qn;\n}\nmod plain;\n",
                ),
                ("src/p4m.rs", "#[cfg(not(test))]\nmod p4n;\n"),
                ("src/p4m/p4n.rs", ""),
                ("src/q/qn.rs", ""),
                ("src/plain.rs", ""),
            ],
        );
        let w = walk(&t.0, &[t.root("src/lib.rs", &BOTH)]);
        assert_eq!(names(&w), ["src/lib.rs", "src/plain.rs"]);
        assert_eq!(
            w.unseen,
            [
                format!("src/lib.rs:2:1: {CFG_DOUBT}"),
                format!("src/lib.rs:5:5: {CFG_DOUBT}, on a module that holds it"),
            ]
        );
    }

    #[test]
    fn an_include_inside_an_item_is_reported_and_one_at_an_item_head_takes_that_item_s_cfg() {
        let t = Tree::new(
            "itemhead",
            &[
                (
                    "src/lib.rs",
                    "#[cfg(any())]\npub const P5: u32 = include!(\"p5v.rs\");\nconst A: u8 = 1;\ninclude!(\"ok1.rs\");\n\
                     #[cfg(any())]\nfn f2() {}\ninclude!(\"ok2.rs\");\n#[cfg(any())]\nconst B: u8 = { 1 };\n\
                     include!(\"ok3.rs\");\n#[cfg(any())]\ninclude!(\"dead.rs\");\n",
                ),
                ("src/p5v.rs", "7\n"),
                ("src/ok1.rs", ""),
                ("src/ok2.rs", ""),
                ("src/ok3.rs", ""),
                ("src/dead.rs", ""),
            ],
        );
        let w = walk(&t.0, &[t.root("src/lib.rs", &BOTH)]);
        assert_eq!(
            names(&w),
            ["src/lib.rs", "src/ok1.rs", "src/ok2.rs", "src/ok3.rs"]
        );
        assert_eq!(
            w.unseen,
            ["src/lib.rs:2:21: an `include!` inside an item, which the module walk cannot place"]
        );
    }

    #[test]
    fn a_module_or_include_a_macro_or_a_block_holds_is_reported_and_counts_nothing() {
        let t = Tree::new(
            "macro",
            &[
                (
                    "src/lib.rs",
                    "macro_rules! m {\n    ($n:ident) => {\n        pub mod $n;\n    };\n}\nm!(generated);\nfn f() {\n    \
                     #[path = \"blk.rs\"]\n    mod blk;\n    mod fine {}\n}\nmacro_rules! code {\n    () => {\n        \
                     include!(\"y.rs\");\n    };\n}\nconst _: () = {\n    include!(\"z.rs\")\n};\n#[cfg(any())]\n\
                     std::include!(\"w.rs\");\n",
                ),
                ("src/generated.rs", ""),
                ("src/blk.rs", ""),
                ("src/y.rs", ""),
                ("src/z.rs", ""),
                ("src/w.rs", ""),
            ],
        );
        let w = walk(&t.0, &[t.root("src/lib.rs", &BOTH)]);
        assert_eq!(names(&w), ["src/lib.rs"]);
        let at: Vec<&str> = w
            .unseen
            .iter()
            .map(|u| u.split(": ").next().unwrap())
            .collect();
        assert_eq!(
            at,
            [
                "src/lib.rs:3:13",
                "src/lib.rs:9:5",
                "src/lib.rs:14:9",
                "src/lib.rs:18:5"
            ],
            "{:?}",
            w.unseen
        );
        assert!(
            w.unseen[0].contains("a `mod` a macro names"),
            "{:?}",
            w.unseen
        );
        assert!(w.unseen[1].contains("an out-of-line `mod` inside a block or a macro"));
        assert!(w.unseen[2].contains("an `include!` inside a block or a macro"));
    }

    #[test]
    fn a_target_s_units_follow_its_kind_test_flag_and_harness() {
        let entries = tomlread::parse(
            "[package]\nname = \"x\"\n\n[lib]\nharness = false\n\n[[test]]\nname = \"plain\"\nharness = false\n\n\
             [[test]]\nname = \"other\"\n\n[[bin]]\nname = \"x\"\n",
        )
        .unwrap();
        assert_eq!(harness(&entries, "lib", "x"), Some(false));
        assert_eq!(harness(&entries, "test", "plain"), Some(false));
        assert_eq!(harness(&entries, "test", "other"), None);
        assert_eq!(harness(&entries, "test", "auto"), None);
        assert_eq!(harness(&entries, "bin", "x"), None);
        let inline = tomlread::parse(
            "test = [{ name = \"inl\", harness = false }]\n\n[package]\nname = \"y\"\n",
        )
        .unwrap();
        assert_eq!(harness(&inline, "test", "inl"), Some(false));
        assert_eq!(units("lib", true, Some(false)), [Some(false), Some(false)]);
        assert_eq!(units("lib", true, None), BOTH);
        assert_eq!(units("lib", false, None), [Some(false)]);
        assert_eq!(units("test", true, Some(false)), [Some(false)]);
        assert_eq!(units("test", true, None), [Some(true)]);
        assert_eq!(units("custom-build", false, None), [Some(false)]);
        assert_eq!(units("bench", false, None), [None]);
    }

    #[test]
    fn cargo_metadata_names_every_target_root_with_its_units() {
        let t = Tree::new(
            "meta",
            &[(
                "Cargo.toml",
                "[package]\nname = \"x\"\n\n[[test]]\nname = \"plain\"\nharness = false\n",
            )],
        );
        let manifest = t.0.join("Cargo.toml").display().to_string();
        let src = t.0.join("src/lib.rs").display().to_string();
        let json = format!(
            "{{\"packages\":[{{\"name\":\"x\",\"manifest_path\":{m:?},\"targets\":[\
             {{\"kind\":[\"lib\"],\"name\":\"x\",\"src_path\":{s:?},\"test\":true}},\
             {{\"kind\":[\"test\"],\"name\":\"plain\",\"src_path\":\"/t/tests/plain.rs\",\"test\":true}},\
             {{\"kind\":[\"custom-build\"],\"name\":\"build-script-build\",\"src_path\":\"/t/./build.rs\",\
             \"test\":false}}]}}]}}",
            m = manifest,
            s = src
        );
        let r = roots(&crate::jsonread::parse(&json).unwrap()).unwrap();
        let got: Vec<(String, Vec<Unit>)> = r
            .iter()
            .map(|r| (r.path.display().to_string(), r.units.clone()))
            .collect();
        assert_eq!(
            got,
            [
                (src, BOTH.to_vec()),
                ("/t/tests/plain.rs".to_string(), vec![Some(false)]),
                ("/t/build.rs".to_string(), vec![Some(false)]),
            ]
        );
        assert!(roots(&crate::jsonread::parse("{\"packages\":[]}").unwrap()).is_err());
    }

    #[test]
    fn this_tree_s_walk_reaches_every_crate_source_and_reports_no_module() {
        let root = normal(&Path::new(env!("CARGO_MANIFEST_DIR")).join("../.."));
        let o = std::process::Command::new(env!("CARGO"))
            .args(["metadata", "--format-version", "1", "--no-deps", "--locked"])
            .current_dir(&root)
            .output()
            .unwrap();
        assert!(o.status.success(), "{}", crate::srcio::tool_text(&o.stderr));
        let meta = crate::jsonread::parse(&crate::srcio::tool_text(&o.stdout)).unwrap();
        let w = walk(&root, &roots(&meta).unwrap());
        assert!(w.unseen.is_empty(), "{:?}", w.unseen);
        let reached = w.module_names();
        let listed = crate::tree::listed_files(&root);
        let missing: Vec<&String> = crate::compiled::crate_sources(&listed)
            .into_iter()
            .filter(|f| !reached.contains(*f))
            .collect();
        assert!(missing.is_empty(), "{missing:?}");
    }
}
