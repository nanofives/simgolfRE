use msvc6_source::lex::{self, Cond, Kind, Token};
use std::collections::{BTreeMap, BTreeSet};

pub const DTOR: &str = "~";
pub const VPTR: &str = "<vptr>";
pub const MAX_DEPTH: usize = 8;
pub const MAX_LINEAGE: usize = 32;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Method {
    pub name: String,
    pub params: String,
    pub signature: String,
    pub virt: bool,
    pub pure: bool,
    pub stdcall: bool,
    pub inline_body: bool,
    pub line: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Legacy {
    pub virtuals: Vec<String>,
    pub pure: usize,
    pub members: Vec<(String, String)>,
    pub data: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ClassDecl {
    pub name: String,
    pub bases: Vec<String>,
    pub virtual_bases: Vec<String>,
    pub methods: Vec<Method>,
    pub data: Vec<String>,
    pub macro_used: Option<String>,
    pub legacy: Legacy,
    pub file: String,
    pub line: usize,
}

impl ClassDecl {
    #[must_use]
    pub fn virtuals(&self, legacy: bool) -> Vec<String> {
        if legacy {
            return self.legacy.virtuals.clone();
        }
        self.methods
            .iter()
            .filter(|m| m.virt)
            .map(|m| m.name.clone())
            .collect()
    }

    #[must_use]
    pub fn pure(&self, legacy: bool) -> usize {
        if legacy {
            return self.legacy.pure;
        }
        self.methods.iter().filter(|m| m.virt && m.pure).count()
    }

    #[must_use]
    pub fn members(&self, legacy: bool) -> Vec<(String, String)> {
        if legacy {
            return self.legacy.members.clone();
        }
        self.methods
            .iter()
            .map(|m| (m.name.clone(), m.signature.clone()))
            .collect()
    }

    #[must_use]
    pub fn data(&self, legacy: bool) -> &[String] {
        if legacy {
            &self.legacy.data
        } else {
            &self.data
        }
    }

    #[must_use]
    pub fn polymorphic(&self, legacy: bool) -> bool {
        if legacy {
            !self.legacy.virtuals.is_empty()
        } else {
            self.methods.iter().any(|m| m.virt)
        }
    }
}

#[must_use]
pub fn uncomment(src: &[u8]) -> String {
    let lx = lex::lex(src);
    let mut out: Vec<u8> = Vec::with_capacity(src.len());
    let mut at = 0usize;
    for c in &lx.comments {
        let (s, e) = (c.start.min(src.len()), c.end.min(src.len()));
        if s < at {
            continue;
        }
        out.extend_from_slice(src.get(at..s).unwrap_or_default());
        out.push(b' ');
        out.extend(
            src.get(s..e)
                .unwrap_or_default()
                .iter()
                .filter(|b| **b == b'\n'),
        );
        at = e.max(s);
    }
    out.extend_from_slice(src.get(at..).unwrap_or_default());
    msvc6_base::srcio::decode(&out)
}

#[must_use]
pub fn member_name(decl: &str) -> Option<String> {
    let head = decl.split('(').next()?.trim_end();
    let prefix = head.trim_end_matches(|c: char| c.is_ascii_alphanumeric() || c == '_');
    let name = head.get(prefix.len()..)?;
    if name.is_empty() {
        return None;
    }
    Some(if prefix.ends_with('~') {
        format!("~{name}")
    } else {
        name.to_string()
    })
}

fn parse_bases(clause: &str) -> Vec<String> {
    clause
        .split(',')
        .filter_map(|part| {
            part.split_whitespace()
                .rev()
                .find(|w| !matches!(*w, "public" | "private" | "protected" | "virtual"))
                .map(str::to_string)
        })
        .filter(|s| !s.is_empty())
        .collect()
}

fn line_starts(src: &str) -> impl Iterator<Item = (usize, &str)> {
    let mut at = 0usize;
    src.split_inclusive('\n').map(move |l| {
        let here = at;
        at = at.saturating_add(l.len());
        let line = l
            .strip_suffix('\n')
            .map_or(l, |t| t.strip_suffix('\r').unwrap_or(t));
        (here, line)
    })
}

fn closes(src: &str, opens: &BTreeSet<usize>) -> BTreeMap<usize, usize> {
    let mut out = BTreeMap::new();
    let mut depth = 0usize;
    let mut open: Vec<(usize, usize)> = Vec::new();
    for (i, b) in src.bytes().enumerate() {
        match b {
            b'{' => {
                if opens.contains(&i) {
                    open.push((i, depth));
                }
                depth = depth.saturating_add(1);
            }
            b'}' if depth > 0 => {
                depth -= 1;
                if open.last().is_some_and(|(_, d)| *d == depth)
                    && let Some((o, _)) = open.pop()
                {
                    out.insert(o, i);
                }
            }
            _ => {}
        }
    }
    out
}

#[must_use]
pub fn normalise_ws(s: &str) -> String {
    s.split_whitespace().collect::<Vec<_>>().join(" ")
}

fn types_are_void(parts: &[String]) -> bool {
    parts.len() == 1 && parts.first().map(|p| normalise_ws(p)) == Some("void".to_string())
}

#[must_use]
pub fn normalise_signature(decl: &str) -> String {
    let d = decl.trim().trim_start_matches("virtual ").trim();
    let Some(open) = d.find('(') else {
        return normalise_ws(d);
    };
    let mut depth = 0i32;
    let mut close = None;
    for (i, c) in d.char_indices().skip_while(|(i, _)| *i < open) {
        match c {
            '(' => depth = depth.saturating_add(1),
            ')' => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    close = Some(i);
                    break;
                }
            }
            _ => {}
        }
    }
    let Some(close) = close else {
        return normalise_ws(d);
    };
    let ret = d.get(..open).map(normalise_ws).unwrap_or_default();
    let params = d.get(open.saturating_add(1)..close).unwrap_or("");
    let mut parts: Vec<String> = Vec::new();
    let mut cur = String::new();
    let mut pd = 0i32;
    for c in params.chars() {
        match c {
            '(' => {
                pd = pd.saturating_add(1);
                cur.push(c);
            }
            ')' => {
                pd = pd.saturating_sub(1);
                cur.push(c);
            }
            ',' if pd == 0 => {
                parts.push(cur.clone());
                cur.clear();
            }
            _ => cur.push(c),
        }
    }
    if !cur.trim().is_empty() {
        parts.push(cur);
    }
    if types_are_void(&parts) {
        return format!("{ret}()");
    }
    let types: Vec<String> = parts
        .iter()
        .map(|p| {
            let t = normalise_ws(p);
            if t.contains('(') {
                return t;
            }
            let head = t.trim_end_matches(|c: char| c.is_ascii_alphanumeric() || c == '_');
            if head.trim().is_empty() {
                normalise_ws(&t)
            } else {
                normalise_ws(head)
            }
        })
        .collect();
    format!("{ret}({})", types.join(", "))
}

#[must_use]
pub fn signature_key(sig: &str) -> String {
    sig.chars().filter(|c| !c.is_whitespace()).collect()
}

#[must_use]
pub fn arity(sig: &str) -> usize {
    let Some(open) = sig.find('(') else { return 0 };
    let inner = sig
        .get(open.saturating_add(1)..sig.len().saturating_sub(1))
        .unwrap_or("");
    if inner.trim().is_empty() {
        return 0;
    }
    let mut n = 1usize;
    let mut d = 0i32;
    for c in inner.chars() {
        match c {
            '(' => d = d.saturating_add(1),
            ')' => d = d.saturating_sub(1),
            ',' if d == 0 => n = n.saturating_add(1),
            _ => {}
        }
    }
    n
}

fn after_label(s: &str) -> String {
    match (
        s.rfind("public:"),
        s.rfind("private:"),
        s.rfind("protected:"),
    ) {
        (None, None, None) => s.to_string(),
        (a, b, c) => {
            let at = a.unwrap_or(0).max(b.unwrap_or(0)).max(c.unwrap_or(0));
            s.get(at..)
                .and_then(|t| t.find(':').and_then(|i| t.get(i.saturating_add(1)..)))
                .map_or(s, str::trim)
                .to_string()
        }
    }
}

#[must_use]
pub fn top_level_members(body: &str) -> Vec<(String, String)> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut stmt = String::new();
    for c in body.chars() {
        match c {
            '{' => {
                depth = depth.saturating_add(1);
                stmt.clear();
            }
            '}' => {
                depth = depth.saturating_sub(1);
                stmt.clear();
            }
            ';' => {
                if depth == 0 {
                    let d = after_label(&normalise_ws(&stmt));
                    if d.contains('(')
                        && !d.starts_with("typedef")
                        && let Some(n) = member_name(&d)
                    {
                        let key = if n.starts_with('~') {
                            DTOR.to_string()
                        } else {
                            n
                        };
                        out.push((key, normalise_signature(&d)));
                    }
                }
                stmt.clear();
            }
            _ => {
                if depth == 0 {
                    stmt.push(c);
                }
            }
        }
    }
    out
}

#[must_use]
pub fn top_level_data(body: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut stmt = String::new();
    let mut had_virtual = false;
    for c in body.chars() {
        match c {
            '{' => {
                depth = depth.saturating_add(1);
                stmt.push(c);
            }
            '}' => {
                depth = depth.saturating_sub(1);
                stmt.push(c);
            }
            ';' => {
                if depth == 0 {
                    let t = after_label(&normalise_ws(&stmt));
                    if t.starts_with("virtual") {
                        if !had_virtual {
                            had_virtual = true;
                            out.insert(0, VPTR.to_string());
                        }
                    } else if t.starts_with("union") || t.starts_with("struct") {
                        out.push("<union>".to_string());
                    } else if !t.is_empty() && !t.contains('(') && !t.starts_with("typedef") {
                        out.push(t);
                    }
                    stmt.clear();
                }
            }
            _ => stmt.push(c),
        }
    }
    out
}

#[must_use]
pub fn data_types(decl: &str) -> String {
    let t = normalise_ws(decl);
    let (head, rest) = match t.split_once(',') {
        Some((h, r)) => (h.to_string(), Some(r.to_string())),
        None => (t.clone(), None),
    };
    let base = head.trim_end_matches(|c: char| c.is_ascii_alphanumeric() || c == '_');
    let ty = if base.trim().is_empty() {
        normalise_ws(&head)
    } else {
        normalise_ws(base)
    };
    let extra = rest.map_or(0usize, |r| r.split(',').count());
    let mut all = vec![ty.clone()];
    for _ in 0..extra {
        all.push(ty.clone());
    }
    all.join("; ")
}

#[must_use]
pub fn is_explicit_vptr(first: &str) -> bool {
    let t = normalise_ws(first).replace(' ', "");
    matches!(t.as_str(), "void*m_vptr" | "void*m_vfptr" | "void*vptr")
}

#[must_use]
pub fn top_level_virtuals(body: &str) -> (Vec<String>, usize) {
    let mut names = Vec::new();
    let mut pure = 0usize;
    let mut depth = 0i32;
    let mut stmt = String::new();
    for c in body.chars() {
        match c {
            '{' => {
                depth = depth.saturating_add(1);
                stmt.clear();
            }
            '}' => {
                depth = depth.saturating_sub(1);
                stmt.clear();
            }
            ';' => {
                if depth == 0 {
                    let s = normalise_ws(&stmt);
                    let d = s.rfind("virtual ").and_then(|i| s.get(i..)).unwrap_or("");
                    if !d.is_empty() && d.contains('(') {
                        if d.replace(' ', "").ends_with("=0") {
                            pure = pure.saturating_add(1);
                        }
                        let n = member_name(d).unwrap_or_else(|| "?operator".to_string());
                        names.push(if n.starts_with('~') {
                            DTOR.to_string()
                        } else {
                            n
                        });
                    }
                }
                stmt.clear();
            }
            _ => {
                if depth == 0 {
                    stmt.push(c);
                }
            }
        }
    }
    (names, pure)
}

#[must_use]
pub fn invokes_macro(body: &str, class: &str) -> Option<String> {
    let mut depth = 0i32;
    let mut scope = String::new();
    for c in body.chars() {
        match c {
            '{' => depth = depth.saturating_add(1),
            '}' => {
                depth = depth.saturating_sub(1);
                scope.push('\n');
            }
            _ if depth == 0 => scope.push(c),
            _ => {}
        }
    }
    for line in scope.lines() {
        let t = line.trim();
        if t.starts_with('#') || t.starts_with('*') || t.starts_with("//") || t.contains("virtual ")
        {
            continue;
        }
        let mut rest = t;
        while let Some(open) = rest.find('(') {
            let head = rest.get(..open).unwrap_or("").trim_end();
            let ident: String = head
                .chars()
                .rev()
                .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
                .collect::<Vec<_>>()
                .into_iter()
                .rev()
                .collect();
            let looks_macro = ident.len() >= 2
                && ident
                    .chars()
                    .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_')
                && ident.chars().next().is_some_and(|c| c.is_ascii_uppercase());
            if looks_macro && ident != class {
                return Some(ident);
            }
            rest = rest.get(open.saturating_add(1)..).unwrap_or("");
        }
    }
    None
}

const TYPE_WORDS: [&str; 20] = [
    "int", "void", "char", "long", "short", "unsigned", "signed", "float", "double", "bool",
    "const", "volatile", "struct", "class", "enum", "union", "__int8", "__int16", "__int32",
    "__int64",
];

const CONVENTIONS: [&str; 5] = [
    "__stdcall",
    "__cdecl",
    "__fastcall",
    "__thiscall",
    "_stdcall",
];

fn glue(tokens: &[&Token]) -> String {
    let mut out = String::new();
    for t in tokens {
        let tight = matches!(
            t.text.as_str(),
            "*" | "&" | "," | "(" | ")" | "[" | "]" | "::"
        ) || out.ends_with(['*', '&', ',', '(', '[', ':'])
            || out.is_empty();
        if !tight {
            out.push(' ');
        }
        out.push_str(&t.text);
    }
    out
}

fn split_top(tokens: &[&Token], sep: &str) -> Vec<Vec<usize>> {
    let mut out = vec![Vec::new()];
    let mut depth = 0i32;
    for (i, t) in tokens.iter().enumerate() {
        if t.kind == Kind::Punct {
            match t.text.as_str() {
                "(" | "[" | "<" | "{" => depth = depth.saturating_add(1),
                ")" | "]" | ">" | "}" => depth = depth.saturating_sub(1),
                s if s == sep && depth == 0 => {
                    out.push(Vec::new());
                    continue;
                }
                _ => {}
            }
        }
        if let Some(l) = out.last_mut() {
            l.push(i);
        }
    }
    out
}

fn param_key(tokens: &[&Token]) -> String {
    let groups = split_top(tokens, ",");
    let mut params: Vec<String> = Vec::new();
    for g in &groups {
        let mut p: Vec<&Token> = g.iter().filter_map(|i| tokens.get(*i).copied()).collect();
        if let Some(eq) = p.iter().position(|t| t.is_punct("=")) {
            p.truncate(eq);
        }
        let mut stars = 0usize;
        while p.last().is_some_and(|t| t.is_punct("]")) {
            let Some(open) = p.iter().rposition(|t| t.is_punct("[")) else {
                break;
            };
            p.truncate(open);
            stars += 1;
        }
        let named = p.len() >= 2
            && p.last()
                .is_some_and(|t| t.kind == Kind::Ident && !TYPE_WORDS.contains(&t.text.as_str()))
            && p.get(p.len() - 2).is_some_and(|t| {
                t.kind == Kind::Ident || t.is_punct("*") || t.is_punct("&") || t.is_punct(">")
            });
        if named {
            p.pop();
        }
        let mut s = glue(&p);
        for _ in 0..stars {
            s.push('*');
        }
        if !s.is_empty() {
            params.push(s);
        }
    }
    if params.len() == 1 && params.first().map(String::as_str) == Some("void") {
        params.clear();
    }
    params.join(",")
}

fn method_of(stmt: &[&Token], class: &str, inline_body: bool) -> Option<Method> {
    let first = stmt.first()?;
    let lead = |w: &str| {
        stmt.iter()
            .take_while(|t| !t.is_punct("("))
            .any(|t| t.is(w))
    };
    if lead("typedef") || lead("friend") || first.is("using") || first.is("template") {
        return None;
    }
    let open = stmt.iter().position(|t| t.is_punct("("))?;
    let head = stmt.get(..open)?;
    let after = stmt.get(open.saturating_add(1)..)?;
    if after
        .first()
        .is_some_and(|t| t.is_punct("*") || t.is_punct("&"))
    {
        let mut d = 1i32;
        let close = after.iter().position(|t| {
            if t.is_punct("(") {
                d += 1;
            } else if t.is_punct(")") {
                d -= 1;
            }
            d == 0
        })?;
        if after
            .get(close.saturating_add(1))
            .is_some_and(|t| t.is_punct("("))
        {
            return None;
        }
    }
    let mut depth = 1i32;
    let close = after.iter().position(|t| {
        if t.is_punct("(") {
            depth += 1;
        } else if t.is_punct(")") {
            depth -= 1;
        }
        depth == 0
    })?;
    let params = after.get(..close)?;
    let tail = after.get(close.saturating_add(1)..).unwrap_or_default();
    let (name, name_at) = if let Some(op) = head.iter().position(|t| t.is("operator")) {
        let text: String = head
            .get(op..)
            .unwrap_or_default()
            .iter()
            .map(|t| t.text.as_str())
            .collect();
        (text, op)
    } else {
        let at = head.iter().rposition(|t| t.is_ident())?;
        let tilde = at > 0 && head.get(at - 1).is_some_and(|t| t.is_punct("~"));
        let n = head.get(at)?.text.clone();
        if tilde {
            (DTOR.to_string(), at - 1)
        } else {
            (n, at)
        }
    };
    let cv: Vec<&str> = tail
        .iter()
        .take_while(|t| t.is("const") || t.is("volatile"))
        .map(|t| t.text.as_str())
        .collect();
    let pure = tail.windows(2).any(|w| {
        w.first().is_some_and(|a| a.is_punct("=")) && w.get(1).is_some_and(|b| b.text == "0")
    });
    let quals = head.get(..name_at).unwrap_or_default();
    let virt = quals.iter().any(|t| t.is("virtual"));
    let stdcall = quals.iter().any(|t| {
        t.is("__stdcall") || t.is("_stdcall") || t.is("WINAPI") || t.is("STDMETHODCALLTYPE")
    });
    let ret: Vec<&Token> = quals
        .iter()
        .copied()
        .filter(|t| {
            !matches!(
                t.text.as_str(),
                "virtual" | "inline" | "static" | "explicit" | "__inline" | "__forceinline"
            ) && !CONVENTIONS.contains(&t.text.as_str())
        })
        .collect();
    let mut params_key = format!("({})", param_key(params));
    for q in &cv {
        params_key.push_str(q);
    }
    let shown = if name == DTOR {
        format!("~{class}")
    } else {
        name.clone()
    };
    let signature = format!(
        "{}{}{shown}({})",
        glue(&ret),
        if ret.is_empty() { "" } else { " " },
        param_key(params)
    );
    Some(Method {
        line: first.line,
        name,
        params: params_key,
        signature,
        virt,
        pure,
        stdcall,
        inline_body,
    })
}

fn data_of(stmt: &[&Token]) -> Option<String> {
    let first = stmt.first()?;
    if ["typedef", "friend", "using", "enum", "static", "template"]
        .iter()
        .any(|w| first.is(w))
    {
        return None;
    }
    if first.is("union") || first.is("struct") || first.is("class") {
        return Some("<union>".to_string());
    }
    let text = glue(stmt);
    (!text.is_empty()).then_some(text)
}

#[must_use]
pub fn scan_body(body: &str, class: &str) -> (Vec<Method>, Vec<String>) {
    let lx = lex::lex(body.as_bytes());
    let toks: Vec<&Token> = lx
        .tokens
        .iter()
        .filter(|t| t.directive.is_none() && t.cond != Cond::Dead)
        .collect();
    let mut methods = Vec::new();
    let mut data: Vec<String> = Vec::new();
    let mut stmt: Vec<&Token> = Vec::new();
    let mut i = 0usize;
    let finish = |stmt: &mut Vec<&Token>,
                  inline: bool,
                  methods: &mut Vec<Method>,
                  data: &mut Vec<String>| {
        while stmt.len() >= 2
            && stmt.get(1).is_some_and(|t| t.is_punct(":"))
            && stmt
                .first()
                .is_some_and(|t| t.is("public") || t.is("private") || t.is("protected"))
        {
            stmt.drain(..2);
        }
        if stmt.is_empty() {
            return;
        }
        if let Some(m) = method_of(stmt, class, inline) {
            methods.push(m);
        } else if !inline
            && !stmt.iter().any(|t| t.is_punct("("))
            && let Some(d) = data_of(stmt)
        {
            data.push(d);
        }
        stmt.clear();
    };
    while let Some(t) = toks.get(i).copied() {
        if t.is_punct(";") {
            finish(&mut stmt, false, &mut methods, &mut data);
            stmt.clear();
            i += 1;
            continue;
        }
        if t.is_punct("{") {
            let mut d = 0i32;
            let mut j = i;
            while let Some(u) = toks.get(j) {
                if u.is_punct("{") {
                    d += 1;
                } else if u.is_punct("}") {
                    d -= 1;
                    if d == 0 {
                        break;
                    }
                }
                j += 1;
            }
            let function = stmt.iter().any(|x| x.is_punct("("))
                && !stmt.first().is_some_and(|x| {
                    x.is("struct") || x.is("class") || x.is("union") || x.is("enum")
                });
            if function {
                finish(&mut stmt, true, &mut methods, &mut data);
                stmt.clear();
            } else {
                stmt.push(t);
                if let Some(u) = toks.get(j) {
                    stmt.push(u);
                }
            }
            i = j.saturating_add(1);
            continue;
        }
        if t.is_punct(":") && stmt.is_empty() {
            i += 1;
            continue;
        }
        if t.is_punct(":")
            && stmt.len() == 1
            && stmt
                .first()
                .is_some_and(|x| x.is("public") || x.is("private") || x.is("protected"))
        {
            stmt.clear();
            i += 1;
            continue;
        }
        stmt.push(t);
        i += 1;
    }
    finish(&mut stmt, false, &mut methods, &mut data);
    if methods.iter().any(|m| m.virt) {
        data.insert(0, VPTR.to_string());
    }
    (methods, data)
}

pub const MAX_HEAD: usize = 1 << 16;
pub const MAX_BASES: usize = 1 << 8;
pub const SCAN_FACTOR: usize = 4;

struct Head<'a> {
    line: usize,
    name: String,
    clause: &'a str,
    brace: usize,
}

fn heads(src: &str) -> (Vec<Head<'_>>, usize) {
    let bytes = src.as_bytes();
    let mut out = Vec::new();
    let mut long = 0usize;
    let mut next: Option<Option<usize>> = None;
    for (n, (line_start, line)) in line_starts(src).enumerate() {
        let Some(rest) = line
            .strip_prefix("class ")
            .or_else(|| line.strip_prefix("struct "))
        else {
            continue;
        };
        let Some(word) = rest.split_whitespace().next() else {
            continue;
        };
        let name = word.trim_end_matches([':', '{', ';']).to_string();
        if name.is_empty() {
            continue;
        }
        let stop = match next {
            Some(Some(p)) if p >= line_start => Some(p),
            Some(None) => None,
            _ => {
                let found = bytes
                    .get(line_start..)
                    .and_then(|t| t.iter().position(|b| matches!(b, b'{' | b';')))
                    .map(|o| line_start.saturating_add(o));
                next = Some(found);
                found
            }
        };
        let Some(brace) = stop.filter(|s| bytes.get(*s) == Some(&b'{')) else {
            continue;
        };
        if brace.saturating_sub(line_start) > MAX_HEAD {
            long += 1;
            continue;
        }
        let clause = src
            .get(line_start..brace)
            .and_then(|h| h.split_once(':').map(|(_, c)| c))
            .unwrap_or_default();
        out.push(Head {
            line: n,
            name,
            clause,
            brace,
        });
    }
    (out, long)
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Scan {
    pub decls: Vec<ClassDecl>,
    pub unread: usize,
}

#[must_use]
pub fn class_decls(src: &str, file: &str) -> Vec<ClassDecl> {
    class_scan(src, file).decls
}

#[must_use]
pub fn class_scan(src: &str, file: &str) -> Scan {
    let (heads, long) = heads(src);
    let total = heads.len();
    let mut unread = long;
    let ends = closes(src, &heads.iter().map(|h| h.brace).collect());
    let mut scanned = 0usize;
    let budget = src.len().saturating_mul(SCAN_FACTOR);
    let mut out = Vec::new();
    for (
        k,
        Head {
            line: n,
            name,
            clause,
            brace,
        },
    ) in heads.into_iter().enumerate()
    {
        scanned = scanned.saturating_add(clause.len());
        if scanned > budget {
            unread = unread.saturating_add(total.saturating_sub(k));
            break;
        }
        let bases = parse_bases(clause);
        if bases.len() > MAX_BASES {
            unread = unread.saturating_add(1);
            continue;
        }
        let virtual_bases = clause
            .split(',')
            .filter(|p| p.split_whitespace().any(|w| w == "virtual"))
            .flat_map(parse_bases)
            .collect();
        let Some(body) = ends
            .get(&brace)
            .and_then(|e| src.get(brace.saturating_add(1)..*e))
        else {
            continue;
        };
        scanned = scanned.saturating_add(body.len());
        if scanned > budget {
            unread = unread.saturating_add(total.saturating_sub(k));
            break;
        }
        let (virtuals, pure) = top_level_virtuals(body);
        let (methods, data) = scan_body(body, &name);
        let line_no = n.saturating_add(1);
        let methods = methods
            .into_iter()
            .map(|mut m| {
                m.line = m.line.saturating_add(line_no).saturating_sub(1);
                m
            })
            .collect();
        out.push(ClassDecl {
            macro_used: invokes_macro(body, &name),
            legacy: Legacy {
                virtuals,
                pure,
                members: top_level_members(body),
                data: top_level_data(body),
            },
            methods,
            data,
            name,
            bases,
            virtual_bases,
            file: file.to_string(),
            line: line_no,
        });
    }
    Scan { decls: out, unread }
}

#[must_use]
pub fn virtual_macros(files: &[(String, String)]) -> BTreeMap<String, (String, String)> {
    let mut out = BTreeMap::new();
    for (_, src) in files {
        let joined = src.replace("\\\n", " ");
        for line in joined.lines() {
            let Some(rest) = line.trim_start().strip_prefix("#define ") else {
                continue;
            };
            let Some((name, tail)) = rest.split_once('(') else {
                continue;
            };
            let Some((param, body)) = tail.split_once(')') else {
                continue;
            };
            let param = param.trim();
            let ident =
                |t: &str| !t.is_empty() && t.chars().all(|c| c.is_ascii_alphanumeric() || c == '_');
            if ident(name) && ident(param) && body.contains("virtual ") {
                out.insert(
                    name.to_string(),
                    (param.to_string(), body.trim().to_string()),
                );
            }
        }
    }
    out
}

pub const MAX_EXPANSIONS: usize = 1 << 16;
pub const MAX_EXPAND_WORK: usize = 1 << 28;

#[must_use]
pub fn expand(src: &str, macros: &BTreeMap<String, (String, String)>) -> String {
    expand_within(src, macros).0
}

#[must_use]
pub fn expand_within(src: &str, macros: &BTreeMap<String, (String, String)>) -> (String, bool) {
    let mut out = String::with_capacity(src.len());
    let mut budget = MAX_EXPANSIONS;
    let mut work = 0usize;
    let mut spent = false;
    for line in src.lines() {
        if line.trim_start().starts_with("#define ") {
            out.push('\n');
            continue;
        }
        let mut l = line.to_string();
        for (name, (param, body)) in macros {
            work = work.saturating_add(l.len().saturating_add(1));
            if work > MAX_EXPAND_WORK {
                spent = true;
                break;
            }
            let open = format!("{name}(");
            let mut from_at = 0usize;
            while let Some(i) = l
                .get(from_at..)
                .and_then(|t| t.find(&open))
                .map(|i| i + from_at)
            {
                work = work.saturating_add(l.len().saturating_add(body.len().saturating_mul(2)));
                if budget == 0 || work > MAX_EXPAND_WORK {
                    spent = true;
                    break;
                }
                budget -= 1;
                let from = i + open.len();
                let Some(j) = l.get(from..).and_then(|t| t.find(')')) else {
                    break;
                };
                let arg = l.get(from..from + j).unwrap_or("").trim().to_string();
                let text = body
                    .replace(&format!("{param}##"), &arg)
                    .replace(&format!("##{param}"), &arg);
                l.replace_range(i..=from + j, &text);
                from_at = i.saturating_add(text.len());
            }
        }
        out.push_str(&l);
        out.push('\n');
    }
    (out, spent)
}

#[must_use]
pub fn declaration_rvas(src: &str) -> Vec<(String, u32)> {
    let mut out = Vec::new();
    for line in src.lines() {
        let Some((decl, comment)) = line.split_once("/*") else {
            continue;
        };
        let Some(v) = decl.find("virtual ") else {
            continue;
        };
        let Some(name) = decl.get(v..).and_then(member_name) else {
            continue;
        };
        let mut rvas = Vec::new();
        let b = comment.as_bytes();
        let mut i = 0;
        while let Some(off) = comment.get(i..).and_then(|t| t.find("0x")) {
            let at = i + off;
            let digits: String = comment
                .get(at + 2..)
                .unwrap_or("")
                .chars()
                .take_while(char::is_ascii_hexdigit)
                .collect();
            let offset = at > 0 && b.get(at - 1) == Some(&b'+');
            if digits.len() >= 4
                && !offset
                && let Ok(r) = u32::from_str_radix(&digits, 16)
            {
                rvas.push(r);
            }
            i = at + 2 + digits.len();
        }
        if let [r] = rvas.as_slice() {
            let n = if name.starts_with('~') {
                DTOR.to_string()
            } else {
                name
            };
            out.push((n, *r));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn decls(src: &str) -> Vec<ClassDecl> {
        class_decls(&uncomment(src.as_bytes()), "t.h")
    }

    #[test]
    fn a_comment_is_whitespace_and_cannot_declare_a_slot() {
        let d = decls("class A {\n/* virtual int hidden(); */\nvirtual int real();\n};\n");
        assert_eq!(d.len(), 1);
        assert_eq!(d[0].virtuals(true), vec!["real"]);
        assert_eq!(d[0].virtuals(false), vec!["real"]);
        assert_eq!(uncomment(b"int/*x*/a;"), "int a;");
        assert_eq!(uncomment(b"a; // b\nc;"), "a;  \nc;");
        assert_eq!(uncomment(b"x = \"/*\";"), "x = \"/*\";");
    }

    #[test]
    fn a_forward_declaration_is_not_a_definition() {
        let d = decls("class Fwd;\nclass Real {\nvirtual int a();\n};");
        assert_eq!(d.len(), 1);
        assert_eq!(d[0].name, "Real");
        assert_eq!(d[0].line, 2);
    }

    #[test]
    fn a_nested_class_keeps_its_own_virtuals() {
        let d = decls(
            "class Outer {\nvirtual int a();\nclass Inner { virtual int b(); virtual int c(); };\nvirtual int d();\n};",
        );
        let outer = d.iter().find(|c| c.name == "Outer").unwrap();
        assert_eq!(outer.virtuals(true), vec!["a", "d"]);
        assert_eq!(outer.virtuals(false), vec!["a", "d"]);
    }

    #[test]
    fn an_access_label_does_not_swallow_the_declaration_after_it() {
        let (v, pure) = top_level_virtuals(
            "public:\nvirtual ~G();\nvirtual int a();\nprotected:\nvirtual int b() = 0;",
        );
        assert_eq!(v, vec![DTOR, "a", "b"]);
        assert_eq!(pure, 1);
        let (m, _) = scan_body(
            "public:\nvirtual ~G();\nvirtual int a();\nprotected:\nvirtual int b() = 0;",
            "G",
        );
        let got: Vec<(&str, bool, bool)> = m
            .iter()
            .map(|m| (m.name.as_str(), m.virt, m.pure))
            .collect();
        assert_eq!(
            got,
            vec![("~", true, false), ("a", true, false), ("b", true, true)]
        );
    }

    #[test]
    fn a_body_defined_in_the_class_is_a_member_too() {
        let body = "public:\n    virtual ~T() { Reset(); }\n    virtual int Draw(int *p, int n) { return 0; }\n    int m_x;\n    void Set(int v) { m_x = v; } int m_y;\n";
        let (legacy, _) = top_level_virtuals(body);
        assert!(legacy.is_empty());
        let (m, data) = scan_body(body, "T");
        let got: Vec<(&str, &str, bool)> = m
            .iter()
            .map(|m| (m.name.as_str(), m.params.as_str(), m.inline_body))
            .collect();
        assert_eq!(
            got,
            vec![
                ("~", "()", true),
                ("Draw", "(int*,int)", true),
                ("Set", "(int)", true)
            ]
        );
        assert_eq!(data, vec![VPTR, "int m_x", "int m_y"]);
    }

    #[test]
    fn parameter_names_defaults_and_arrays_leave_the_types() {
        let (m, _) = scan_body(
            "virtual void a(const char *name, int n = 4);\nvirtual void b(void);\nvirtual void c(int p[], DWORD);\nvirtual int d() const;\nvirtual void __stdcall e(int);\nint (*m_hook)(int);\nvoid f(unsigned int, Rect &r);\n",
            "C",
        );
        let got: Vec<(&str, &str)> = m
            .iter()
            .map(|m| (m.name.as_str(), m.params.as_str()))
            .collect();
        assert_eq!(
            got,
            vec![
                ("a", "(const char*,int)"),
                ("b", "()"),
                ("c", "(int*,DWORD)"),
                ("d", "()const"),
                ("e", "(int)"),
                ("f", "(unsigned int,Rect&)")
            ]
        );
        assert!(m.iter().find(|m| m.name == "e").unwrap().stdcall);
        assert!(!m.iter().find(|m| m.name == "a").unwrap().stdcall);
    }

    #[test]
    fn a_macro_at_class_scope_is_named_and_a_constructor_is_not_one() {
        assert_eq!(
            invokes_macro("\n    M65();\n    virtual ~M65();\n", "M65"),
            None
        );
        assert_eq!(
            invokes_macro(
                "\n    SURFACE_V8(a) SURFACE_V8(b)\n    virtual void v56();\n",
                "P"
            )
            .as_deref(),
            Some("SURFACE_V8")
        );
        assert_eq!(
            invokes_macro("\n    int Size() { return SIZE_OF(4); }\n", "C"),
            None
        );
        assert_eq!(
            invokes_macro(
                "\n#define V8(n) virtual void n##0();\n    virtual void v0();\n",
                "C"
            ),
            None
        );
    }

    #[test]
    fn a_one_parameter_virtual_macro_expands_to_positioned_slots() {
        let src = "#define X_V8(n) virtual void n##0(); virtual void n##1(); \\\n virtual void n##2();\nstruct A {\n    X_V8(a) virtual void z();\n};\n";
        let files = vec![("a.h".to_string(), src.to_string())];
        let m = virtual_macros(&files);
        let d = class_decls(&expand(src, &m), "a.h");
        assert_eq!(d[0].virtuals(true), vec!["a0", "a1", "a2", "z"]);
        assert_eq!(d[0].virtuals(false), vec!["a0", "a1", "a2", "z"]);
        let selfish: BTreeMap<String, (String, String)> = [(
            "R".to_string(),
            ("n".to_string(), "R(n) virtual void n();".to_string()),
        )]
        .into_iter()
        .collect();
        let text = expand("struct B { R(x) };\n", &selfish);
        assert!(text.len() < 4096, "{}", text.len());
    }

    #[test]
    fn a_comment_pin_skips_offsets_and_two_address_lines() {
        let src = "    virtual int  R(int a);  /* slot  3, vtbl+0x0c -- 0x7b4e0 */\n    virtual void P(); /* slot 7, +0x1c -- default 0x4890, body 0x7e5e0 */\n    virtual ~W(); /* 0x805a0 */\n";
        assert_eq!(
            declaration_rvas(src),
            vec![("R".to_string(), 0x7b4e0), ("~".to_string(), 0x805a0)]
        );
    }

    #[test]
    fn signatures_normalise_names_void_and_spacing() {
        assert_eq!(
            signature_key(&normalise_signature("void Reset(void)")),
            signature_key(&normalise_signature("void Reset()"))
        );
        assert_eq!(
            normalise_signature("int f(Surface *dst, int x)"),
            "int f(Surface *, int)"
        );
        assert_eq!(arity("void f(int, char *)"), 2);
        assert_eq!(data_types("ushort a, b"), "ushort; ushort");
        assert!(is_explicit_vptr("void *m_vptr"));
        assert!(!is_explicit_vptr("int m_vptr"));
    }

    #[test]
    fn a_crlf_source_reads_as_its_lf_twin_and_braces_pair_from_their_own_opening() {
        let lf = "struct S { int a; };\nclass A : public B {\npublic:\n    virtual void f();\n    struct In { int b; };\n};\nclass C;\nclass D { virtual int g(); };\n";
        let crlf = lf.replace('\n', "\r\n");
        let names = |s: &str| -> Vec<(String, Vec<String>, usize, usize)> {
            class_decls(s, "t.h")
                .into_iter()
                .map(|c| (c.name, c.bases, c.methods.len(), c.line))
                .collect()
        };
        assert_eq!(names(lf), names(&crlf));
        assert_eq!(
            names(lf),
            vec![
                ("S".to_string(), vec![], 0, 1),
                ("A".to_string(), vec!["B".to_string()], 1, 2),
                ("D".to_string(), vec![], 1, 8)
            ]
        );
        assert!(class_decls("class E {\n{\n};\n", "t.h").is_empty());
        assert_eq!(class_decls("}\n}\nclass F { int x; };\n", "t.h").len(), 1);
    }
}
