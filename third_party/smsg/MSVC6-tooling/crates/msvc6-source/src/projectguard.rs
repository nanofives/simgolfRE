use crate::defs::{self, Def};
use crate::lex::{self, Cond, Kind, Lexed, Token};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const TU_EXTS: [&str; 5] = ["c", "cc", "cpp", "cxx", "c++"];

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Violation {
    pub rule: String,
    pub file: String,
    pub line: usize,
    pub what: String,
    pub refuses: bool,
}

#[must_use]
pub fn is_tu(rel: &str) -> bool {
    rel.rsplit_once('.')
        .is_some_and(|(_, e)| TU_EXTS.contains(&e.to_ascii_lowercase().as_str()))
}

fn file_scope_statements(lx: &Lexed, ds: &[Def]) -> Vec<Vec<usize>> {
    let inside = |i: usize| ds.iter().any(|d| d.head <= i && i <= d.close);
    let idx: Vec<usize> = lx.live_code().into_iter().filter(|i| !inside(*i)).collect();
    let mut out = Vec::new();
    let mut cur: Vec<usize> = Vec::new();
    let mut depth = 0i32;
    let mut scope_depth: Vec<i32> = Vec::new();
    for i in idx {
        let Some(t) = lx.tokens.get(i) else {
            continue;
        };
        if t.is_punct("{") {
            let opener: Vec<&Token> = cur.iter().filter_map(|&j| lx.tokens.get(j)).collect();
            let linkage = opener.first().is_some_and(|f| f.is("extern"))
                && opener.get(1).is_some_and(|s| s.kind == Kind::Str)
                && opener.len() == 2;
            let namespace = opener.first().is_some_and(|f| f.is("namespace"));
            if (linkage || namespace) && depth == 0 {
                scope_depth.push(depth);
                cur.clear();
                continue;
            }
            depth += 1;
            cur.push(i);
            continue;
        }
        if t.is_punct("}") {
            if depth == 0 && scope_depth.pop().is_some() {
                cur.clear();
                continue;
            }
            depth -= 1;
            cur.push(i);
            continue;
        }
        if t.is_punct(";") && depth == 0 {
            out.push(std::mem::take(&mut cur));
            continue;
        }
        cur.push(i);
    }
    out
}

fn declared_name(lx: &Lexed, stmt: &[usize]) -> Option<(String, usize)> {
    let toks: Vec<&Token> = stmt.iter().filter_map(|&i| lx.tokens.get(i)).collect();
    let paren = toks.iter().position(|t| t.is_punct("("));
    let upto = paren.unwrap_or(toks.len());
    let head = toks.get(..upto).unwrap_or_default();
    let stop = head
        .iter()
        .position(|t| t.is_punct("=") || t.is_punct("["))
        .unwrap_or(head.len());
    head.get(..stop)
        .unwrap_or_default()
        .iter()
        .rev()
        .find(|t| t.kind == Kind::Ident)
        .map(|t| (t.text.clone(), t.line))
}

fn top_level_eq(toks: &[&Token]) -> bool {
    let mut depth = 0i32;
    for t in toks {
        if t.is_punct("(") || t.is_punct("[") || t.is_punct("{") {
            depth += 1;
        } else if t.is_punct(")") || t.is_punct("]") || t.is_punct("}") {
            depth -= 1;
        } else if depth == 0 && t.is_punct("=") {
            return true;
        }
    }
    false
}

const CALLING: [&str; 6] = [
    "__cdecl",
    "__stdcall",
    "__fastcall",
    "WINAPI",
    "CALLBACK",
    "APIENTRY",
];

fn is_prototype(toks: &[&Token]) -> bool {
    let Some(p) = toks.iter().position(|t| t.is_punct("(")) else {
        return false;
    };
    let Some(prev) = p.checked_sub(1).and_then(|q| toks.get(q)) else {
        return false;
    };
    if prev.kind != Kind::Ident || CALLING.contains(&prev.text.as_str()) {
        return false;
    }
    let after = toks.get(p + 1);
    if after
        .is_some_and(|a| a.is_punct("*") || a.is_punct("&") || CALLING.contains(&a.text.as_str()))
    {
        return false;
    }
    if toks
        .iter()
        .any(|t| t.is("class") || t.is("struct") || t.is("enum") || t.is("union"))
    {
        return false;
    }
    let mut depth = 0i32;
    for t in toks.iter().skip(p) {
        if t.is_punct("(") {
            depth += 1;
        } else if t.is_punct(")") {
            depth -= 1;
            if depth == 0 {
                break;
            }
        } else if depth == 1 && matches!(t.kind, Kind::Str | Kind::Chr | Kind::Number) {
            return false;
        }
    }
    p >= 1
}

fn operator_name(toks: &[&Token]) -> Option<(String, usize)> {
    let op = toks.iter().position(|t| t.is("operator"))?;
    let rest: Vec<&str> = toks
        .iter()
        .skip(op + 1)
        .take_while(|t| !t.is_punct("("))
        .map(|t| t.text.as_str())
        .collect();
    let line = toks.get(op).map_or(0, |t| t.line);
    Some((
        format!("operator {}", rest.join(" "))
            .trim_end()
            .to_string(),
        line,
    ))
}

#[must_use]
pub fn hand_declarations(lx: &Lexed, ds: &[Def], prototypes: bool) -> Vec<(usize, String, bool)> {
    let defined: BTreeSet<&str> = ds.iter().map(|d| d.base.as_str()).collect();
    let mut out = Vec::new();
    for stmt in file_scope_statements(lx, ds) {
        let toks: Vec<&Token> = stmt.iter().filter_map(|&i| lx.tokens.get(i)).collect();
        let Some(first) = toks.first() else {
            continue;
        };
        if matches!(
            first.text.as_str(),
            "typedef" | "template" | "using" | "friend" | "static"
        ) {
            continue;
        }
        let has_body = toks.iter().any(|t| t.is_punct("{"));
        if has_body || top_level_eq(&toks) {
            continue;
        }
        let is_extern = first.is("extern");
        let prototype = !is_extern && prototypes && is_prototype(&toks);
        if !(is_extern || prototype) {
            continue;
        }
        let Some((name, line)) = operator_name(&toks).or_else(|| declared_name(lx, &stmt)) else {
            continue;
        };
        if defined.contains(name.as_str()) {
            continue;
        }
        out.push((line, name, is_extern));
    }
    out
}

#[must_use]
pub fn type_definitions(lx: &Lexed, ds: &[Def]) -> Vec<(usize, String, Cond)> {
    let inside = |i: usize| ds.iter().any(|d| d.open <= i && i <= d.close);
    let toks: Vec<&Token> = lx
        .tokens
        .iter()
        .enumerate()
        .filter(|(i, t)| t.directive.is_none() && t.cond != Cond::Dead && !inside(*i))
        .map(|(_, t)| t)
        .collect();
    let mut out = Vec::new();
    let mut scopes: Vec<(i32, String)> = Vec::new();
    let mut depth = 0i32;
    let mut pending: Option<String> = None;
    for (k, t) in toks.iter().enumerate() {
        if t.is_punct("{") {
            depth += 1;
            if let Some(n) = pending.take() {
                scopes.push((depth, n));
            }
            continue;
        }
        if t.is_punct("}") {
            if scopes.last().is_some_and(|s| s.0 == depth) {
                scopes.pop();
            }
            depth -= 1;
            continue;
        }
        if t.is_punct(";") {
            pending = None;
            continue;
        }
        if !(t.is("class") || t.is("struct") || t.is("union") || t.is("enum")) {
            continue;
        }
        if k > 0
            && toks.get(k - 1).is_some_and(|p| {
                p.is("friend") || p.is_punct("<") || p.is_punct(",") || p.is_punct("(")
            })
        {
            continue;
        }
        let mut j = k + 1;
        let mut name: Option<&Token> = None;
        while let Some(x) = toks.get(j) {
            if x.kind == Kind::Ident && !x.text.starts_with("__") {
                name = Some(x);
                j += 1;
                continue;
            }
            break;
        }
        let mut m = j;
        if toks.get(m).is_some_and(|x| x.is_punct(":")) {
            while toks
                .get(m)
                .is_some_and(|x| !x.is_punct("{") && !x.is_punct(";"))
            {
                m += 1;
            }
        }
        if !toks.get(m).is_some_and(|x| x.is_punct("{")) {
            continue;
        }
        let outer: Vec<&str> = scopes.iter().map(|s| s.1.as_str()).collect();
        let qualify = |n: &str| {
            if outer.is_empty() {
                n.to_string()
            } else {
                format!("{}::{n}", outer.join("::"))
            }
        };
        let typedef = k > 0 && toks.get(k - 1).is_some_and(|p| p.is("typedef"));
        let alias = if typedef {
            let mut d = 0i32;
            let mut close = None;
            for (q, x) in toks.iter().enumerate().skip(m) {
                if x.is_punct("{") {
                    d += 1;
                } else if x.is_punct("}") {
                    d -= 1;
                    if d == 0 {
                        close = Some(q);
                        break;
                    }
                }
            }
            close
                .and_then(|c| toks.get(c + 1))
                .filter(|x| x.kind == Kind::Ident)
        } else {
            None
        };
        if let Some(n) = name {
            out.push((n.line, qualify(&n.text), n.cond));
        }
        if let Some(a) = alias.filter(|a| name.is_none_or(|n| n.text != a.text)) {
            out.push((a.line, qualify(&a.text), a.cond));
        }
        pending = Some(
            name.or(alias.copied())
                .map_or_else(String::new, |n| n.text.clone()),
        );
    }
    out
}

#[must_use]
pub fn function_keys(lx: &Lexed, ds: &[Def]) -> Vec<(usize, String, Cond)> {
    ds.iter()
        .filter(|d| !d.is_static)
        .map(|d| {
            let params: Vec<String> = crate::flow::params(lx, d)
                .into_iter()
                .map(|(_, t)| t)
                .collect();
            let cond = lx.tokens.get(d.name_tok).map_or(Cond::Live, |t| t.cond);
            (
                d.first_line,
                format!("{}({})", d.qualified(), params.join(",")),
                cond,
            )
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Image {
    pub name: String,
    pub exe: bool,
    pub files: Vec<(String, Vec<u8>)>,
}

pub fn guard(images: &[Image], prototypes: bool) -> Vec<Violation> {
    let mut out = Vec::new();
    let mut types_by_image: BTreeMap<String, BTreeMap<String, Vec<(String, usize)>>> =
        BTreeMap::new();
    for img in images {
        let mut types: BTreeMap<String, Vec<(String, usize, Cond)>> = BTreeMap::new();
        let mut funcs: BTreeMap<String, Vec<(String, usize, Cond)>> = BTreeMap::new();
        for (rel, bytes) in &img.files {
            let lx = lex::lex(bytes);
            let ds = defs::definitions(&lx);
            if is_tu(rel) {
                for (line, name, ext) in hand_declarations(&lx, &ds, prototypes) {
                    out.push(Violation {
                        rule: "hand-declaration".to_string(),
                        file: rel.clone(),
                        line,
                        what: format!(
                            "{} `{name}` in a translation unit that does not define it: it belongs in a header \
                             the unit includes",
                            if ext { "an extern declaration of" } else { "a prototype of" }
                        ),
                        refuses: true,
                    });
                }
            }
            for (line, name, cond) in type_definitions(&lx, &ds) {
                types
                    .entry(name)
                    .or_default()
                    .push((rel.clone(), line, cond));
            }
            for (line, key, cond) in function_keys(&lx, &ds) {
                funcs
                    .entry(key)
                    .or_default()
                    .push((rel.clone(), line, cond));
            }
        }
        let mut dup = |rule: &str, what: &str, m: &BTreeMap<String, Vec<(String, usize, Cond)>>| {
            for (name, places) in m {
                if places.len() < 2 {
                    continue;
                }
                let conditional = places.iter().any(|p| p.2 == Cond::Unknown);
                let at: Vec<String> = places.iter().map(|p| format!("{}:{}", p.0, p.1)).collect();
                if let Some(first) = places.first() {
                    out.push(Violation {
                        rule: rule.to_string(),
                        file: first.0.clone(),
                        line: first.1,
                        what: format!(
                            "{what} `{name}` is defined {} times in {}: {}{}",
                            places.len(),
                            img.name,
                            at.join(", "),
                            if conditional {
                                " (one under a condition this guard does not evaluate: reported, not refused)"
                            } else {
                                ""
                            }
                        ),
                        refuses: !conditional,
                    });
                }
            }
        };
        dup("type-twice", "the type", &types);
        dup("function-twice", "the function", &funcs);
        types_by_image.insert(
            img.name.clone(),
            types
                .into_iter()
                .map(|(k, v)| (k, v.into_iter().map(|(f, l, _)| (f, l)).collect()))
                .collect(),
        );
    }
    let names: Vec<&String> = types_by_image.keys().collect();
    for (i, a) in names.iter().enumerate() {
        for b in names.iter().skip(i + 1) {
            let (Some(ta), Some(tb)) = (types_by_image.get(*a), types_by_image.get(*b)) else {
                continue;
            };
            let exe = images
                .iter()
                .any(|im| (&im.name == *a || &im.name == *b) && im.exe);
            for (name, pa) in ta {
                let Some(pb) = tb.get(name) else {
                    continue;
                };
                let (Some(x), Some(y)) = (pa.first(), pb.first()) else {
                    continue;
                };
                if x.0 == y.0 {
                    continue;
                }
                out.push(Violation {
                    rule: "cross-image-type".to_string(),
                    file: x.0.clone(),
                    line: x.1,
                    what: format!(
                        "the type `{name}` is defined in {a} ({}:{}) and in {b} ({}:{}): one image should include \
                         the other's header",
                        x.0, x.1, y.0, y.1
                    ),
                    refuses: exe,
                });
            }
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn img(name: &str, exe: bool, files: &[(&str, &str)]) -> Image {
        Image {
            name: name.into(),
            exe,
            files: files
                .iter()
                .map(|(p, s)| ((*p).to_string(), s.as_bytes().to_vec()))
                .collect(),
        }
    }

    #[test]
    fn a_hand_declaration_in_a_unit_is_refused_and_a_forward_one_is_not() {
        let a = img(
            "app.exe",
            true,
            &[(
                "src/a.cpp",
                "#include \"a.h\"\nextern int g_count;\nint helper(int);\nstatic int local(int);\nextern \"C\" int exported(void) { return 0; }\nint later(int);\nint use() { return later(1) + local(2); }\nint later(int x) { return x; }\nstatic int local(int x) { return x; }\nextern \"C\" {\nint c_proto(void);\n}\n",
            )],
        );
        let v = guard(&[a], true);
        let names: Vec<&str> = v.iter().map(|x| x.what.as_str()).collect();
        assert_eq!(v.len(), 3, "{names:?}");
        assert!(names.iter().any(|w| w.contains("`g_count`")));
        assert!(names.iter().any(|w| w.contains("`helper`")));
        assert!(names.iter().any(|w| w.contains("`c_proto`")));
    }

    #[test]
    fn a_type_or_function_defined_twice_is_refused_across_files_and_images() {
        let a = img(
            "app.exe",
            true,
            &[
                ("include/s.h", "struct Shared { int a; };\nstruct Fwd;\n"),
                (
                    "src/x.cpp",
                    "struct Shared { int a; };\nint f(int a) { return a; }\nint f(long a) { return 1; }\n",
                ),
                ("src/y.cpp", "int f(int b) { return b; }\n"),
            ],
        );
        let b = img(
            "lib.dll",
            false,
            &[(
                "include/t.h",
                "struct Shared { int a; };\nstruct Own { int b; };\n",
            )],
        );
        let v = guard(&[a, b], false);
        let rules: Vec<&str> = v.iter().map(|x| x.rule.as_str()).collect();
        assert_eq!(
            rules.iter().filter(|r| **r == "type-twice").count(),
            1,
            "{v:#?}"
        );
        assert_eq!(rules.iter().filter(|r| **r == "function-twice").count(), 1);
        let cross = v.iter().find(|x| x.rule == "cross-image-type").unwrap();
        assert!(cross.refuses);
        assert!(cross.what.contains("`Shared`"));
    }
}
