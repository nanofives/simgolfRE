use crate::lex::{Kind, Lexed, Token};
use serde::Serialize;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Def {
    pub name: String,
    pub base: String,
    pub scope: Vec<String>,
    pub head: usize,
    pub name_tok: usize,
    pub params: (usize, usize),
    pub open: usize,
    pub close: usize,
    pub first_line: usize,
    pub last_line: usize,
    pub start: usize,
    pub end: usize,
    pub extern_c: bool,
    pub is_static: bool,
    pub in_class: bool,
}

impl Def {
    #[must_use]
    pub fn qualified(&self) -> String {
        if self.name.contains("::") || self.scope.is_empty() {
            return self.name.clone();
        }
        format!("{}::{}", self.scope.join("::"), self.name)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Scope {
    File,
    Namespace(String),
    Linkage,
    Class(String),
    Enum,
    Block,
}

const SKIP_GROUPS: [&str; 5] = [
    "__declspec",
    "__attribute__",
    "throw",
    "__pragma",
    "alignas",
];

const CLASS_KEYS: [&str; 3] = ["class", "struct", "union"];

fn matching(toks: &[&Token], open: usize, o: &str, c: &str) -> Option<usize> {
    let mut depth = 0usize;
    for (k, t) in toks.iter().enumerate().skip(open) {
        if t.is_punct(o) {
            depth += 1;
        } else if t.is_punct(c) {
            depth = depth.saturating_sub(1);
            if depth == 0 {
                return Some(k);
            }
        }
    }
    None
}

fn skip_template(toks: &[&Token], head: usize, end: usize) -> usize {
    if !toks.get(head).is_some_and(|t| t.is("template")) {
        return head;
    }
    let mut depth = 0i32;
    for (k, t) in toks.iter().enumerate().take(end).skip(head + 1) {
        if t.is_punct("<") {
            depth += 1;
        } else if t.is_punct(">") {
            depth -= 1;
            if depth <= 0 {
                return k + 1;
            }
        } else if t.is_punct(">>") {
            depth -= 2;
            if depth <= 0 {
                return k + 1;
            }
        }
    }
    head
}

struct Head {
    name: String,
    base: String,
    name_tok: usize,
    params: (usize, usize),
}

fn function_head(toks: &[&Token], head: usize, open: usize) -> Option<Head> {
    let from = skip_template(toks, head, open);
    let mut k = from;
    let mut depth = 0i32;
    while k < open {
        let t = toks.get(k)?;
        if depth == 0 && t.is_punct("=") {
            return None;
        }
        if t.is_punct("(") || t.is_punct("[") {
            if depth == 0 && t.is_punct("(") {
                let prev = k.checked_sub(1).and_then(|p| toks.get(p));
                let skip = prev.is_some_and(|p| SKIP_GROUPS.contains(&p.text.as_str()));
                let operator_parens = prev.is_some_and(|p| p.is("operator"))
                    && toks.get(k + 1).is_some_and(|n| n.is_punct(")"));
                if operator_parens {
                    k += 2;
                    continue;
                }
                if !skip {
                    let close = matching(toks, k, "(", ")")?;
                    if close >= open {
                        return None;
                    }
                    let (name, base, name_tok) = name_before(toks, from, k)?;
                    let tail_ok = tail_is_function(toks, close + 1, open);
                    return tail_ok.then_some(Head {
                        name,
                        base,
                        name_tok,
                        params: (k, close),
                    });
                }
            }
            depth += 1;
        } else if t.is_punct(")") || t.is_punct("]") {
            depth -= 1;
        }
        k += 1;
    }
    None
}

fn tail_is_function(toks: &[&Token], from: usize, open: usize) -> bool {
    let mut k = from;
    while k < open {
        let Some(t) = toks.get(k) else {
            return false;
        };
        if t.is("const") || t.is("volatile") {
            k += 1;
        } else if t.is("throw") || t.is("__declspec") {
            let Some(c) = toks
                .get(k + 1)
                .filter(|n| n.is_punct("("))
                .and_then(|_| matching(toks, k + 1, "(", ")"))
            else {
                return false;
            };
            k = c + 1;
        } else {
            return t.is_punct(":");
        }
    }
    true
}

fn name_before(toks: &[&Token], from: usize, paren: usize) -> Option<(String, String, usize)> {
    let mut parts: Vec<String> = Vec::new();
    let mut k = paren.checked_sub(1)?;
    let last = toks.get(k)?;
    let name_tok;
    if let Some(op) = (from..=k)
        .rev()
        .find(|&j| toks.get(j).is_some_and(|t| t.is("operator")))
    {
        let mut spelt = String::new();
        let mut prev_word = true;
        for t in toks.get(op + 1..paren)? {
            let word = t.kind == Kind::Ident;
            if word && prev_word {
                spelt.push(' ');
            }
            spelt.push_str(&t.text);
            prev_word = word;
        }
        parts.push(format!("operator{spelt}"));
        name_tok = op;
        k = op;
    } else {
        if last.kind != Kind::Ident {
            return None;
        }
        let tilde = k
            .checked_sub(1)
            .and_then(|p| toks.get(p))
            .is_some_and(|t| t.is_punct("~"));
        parts.push(if tilde {
            format!("~{}", last.text)
        } else {
            last.text.clone()
        });
        name_tok = k;
        if tilde {
            k -= 1;
        }
    }
    while k >= from + 2 {
        let (Some(sep), Some(q)) = (toks.get(k - 1), toks.get(k - 2)) else {
            break;
        };
        if !sep.is_punct("::") {
            break;
        }
        if q.kind == Kind::Ident {
            parts.push(q.text.clone());
            k -= 2;
            continue;
        }
        if !q.is_punct(">") {
            break;
        }
        let mut depth = 0i32;
        let mut open = None;
        for j in (from..k - 1).rev() {
            let Some(x) = toks.get(j) else {
                break;
            };
            if x.is_punct(">") {
                depth += 1;
            } else if x.is_punct("<") {
                depth -= 1;
                if depth == 0 {
                    open = Some(j);
                    break;
                }
            }
        }
        let Some(o) = open.filter(|&o| o > from) else {
            break;
        };
        let Some(name) = toks.get(o - 1).filter(|t| t.kind == Kind::Ident) else {
            break;
        };
        let args: String = toks
            .get(o..k - 1)
            .unwrap_or_default()
            .iter()
            .map(|t| t.text.as_str())
            .collect();
        parts.push(format!("{}{args}", name.text));
        k = o - 1;
    }
    parts.reverse();
    let base = parts.last().cloned().unwrap_or_default();
    Some((parts.join("::"), base, name_tok))
}

fn class_name(toks: &[&Token], head: usize, open: usize) -> Option<(bool, String)> {
    let from = skip_template(toks, head, open);
    let slice = toks.get(from..open)?;
    let at = slice
        .iter()
        .position(|t| CLASS_KEYS.contains(&t.text.as_str()) || t.is("enum"))?;
    let is_enum = slice.get(at).is_some_and(|t| t.is("enum"));
    let name = slice
        .iter()
        .skip(at + 1)
        .take_while(|t| !t.is_punct(":"))
        .filter(|t| t.kind == Kind::Ident && !t.text.starts_with("__"))
        .last()
        .map(|t| t.text.clone())
        .unwrap_or_default();
    Some((is_enum, name))
}

#[must_use]
pub fn definitions(lx: &Lexed) -> Vec<Def> {
    let idx: Vec<usize> = lx.live_code();
    let toks: Vec<&Token> = idx.iter().filter_map(|&i| lx.tokens.get(i)).collect();
    let mut out = Vec::new();
    let mut stack: Vec<Scope> = vec![Scope::File];
    let mut head = 0usize;
    let mut tail = false;
    let mut k = 0usize;
    while let Some(t) = toks.get(k) {
        let top = stack.last().cloned().unwrap_or(Scope::File);
        if matches!(top, Scope::Block | Scope::Enum) {
            if t.is_punct("{") {
                stack.push(Scope::Block);
            } else if t.is_punct("}") {
                stack.pop();
                head = k + 1;
                tail = matches!(top, Scope::Enum);
            }
            k += 1;
            continue;
        }
        if t.is_punct(";") {
            head = k + 1;
            tail = false;
            k += 1;
            continue;
        }
        if t.is_punct("}") {
            let popped = stack.pop();
            if stack.is_empty() {
                stack.push(Scope::File);
            }
            head = k + 1;
            tail = matches!(popped, Some(Scope::Class(_)));
            k += 1;
            continue;
        }
        if matches!(top, Scope::Class(_))
            && t.is_punct(":")
            && k == head + 1
            && toks
                .get(head)
                .is_some_and(|h| matches!(h.text.as_str(), "public" | "private" | "protected"))
        {
            head = k + 1;
            k += 1;
            continue;
        }
        if !t.is_punct("{") {
            k += 1;
            continue;
        }
        if tail {
            stack.push(Scope::Block);
            k += 1;
            continue;
        }
        let first = toks.get(head);
        if first.is_some_and(|f| f.is("namespace")) {
            let name = toks
                .get(head + 1)
                .filter(|n| n.kind == Kind::Ident && head + 1 < k)
                .map(|n| n.text.clone())
                .unwrap_or_default();
            stack.push(Scope::Namespace(name));
            head = k + 1;
            k += 1;
            continue;
        }
        if first.is_some_and(|f| f.is("extern"))
            && toks.get(head + 1).is_some_and(|s| s.kind == Kind::Str)
            && head + 2 == k
        {
            stack.push(Scope::Linkage);
            head = k + 1;
            k += 1;
            continue;
        }
        if let Some(h) = function_head(&toks, head, k) {
            let Some(close) = matching(&toks, k, "{", "}") else {
                break;
            };
            let scope: Vec<String> = stack
                .iter()
                .filter_map(|s| match s {
                    Scope::Namespace(n) | Scope::Class(n) if !n.is_empty() => Some(n.clone()),
                    _ => None,
                })
                .collect();
            let head_toks = toks.get(head..k).unwrap_or_default();
            let extern_c = stack.contains(&Scope::Linkage)
                || (head_toks.first().is_some_and(|f| f.is("extern"))
                    && head_toks.get(1).is_some_and(|s| s.kind == Kind::Str));
            let is_static = head_toks.iter().any(|t| t.is("static"));
            let (Some(ht), Some(ct)) = (toks.get(head), toks.get(close)) else {
                break;
            };
            out.push(Def {
                name: h.name,
                base: h.base,
                scope,
                head: idx.get(head).copied().unwrap_or(0),
                name_tok: idx.get(h.name_tok).copied().unwrap_or(0),
                params: (
                    idx.get(h.params.0).copied().unwrap_or(0),
                    idx.get(h.params.1).copied().unwrap_or(0),
                ),
                open: idx.get(k).copied().unwrap_or(0),
                close: idx.get(close).copied().unwrap_or(0),
                first_line: ht.line,
                last_line: ct.end_line,
                start: ht.start,
                end: ct.end,
                extern_c,
                is_static,
                in_class: matches!(top, Scope::Class(_)),
            });
            k = close + 1;
            head = k;
            tail = false;
            continue;
        }
        match class_name(&toks, head, k) {
            Some((true, _)) => stack.push(Scope::Enum),
            Some((false, n)) => stack.push(Scope::Class(n)),
            None => stack.push(Scope::Block),
        }
        head = k + 1;
        k += 1;
    }
    out
}

#[must_use]
pub fn find<'a>(defs: &'a [Def], name: &str) -> Vec<&'a Def> {
    let (base, nth) = match name.rsplit_once('#') {
        Some((b, n)) => (b, n.parse::<usize>().ok()),
        None => (name, None),
    };
    let hits: Vec<&Def> = defs
        .iter()
        .filter(|d| {
            d.name == base || d.qualified() == base || (!base.contains("::") && d.base == base)
        })
        .collect();
    match nth {
        Some(n) => hits
            .into_iter()
            .nth(n.saturating_sub(1))
            .into_iter()
            .collect(),
        None => hits,
    }
}

pub fn one<'a>(defs: &'a [Def], name: &str) -> Result<&'a Def, String> {
    let hits = find(defs, name);
    match hits.as_slice() {
        [d] => Ok(d),
        [] => Err(format!("no function definition named `{name}`")),
        many => Err(format!(
            "`{name}` names {} definitions ({}): pick one with `{name}#N` or a qualified name",
            many.len(),
            many.iter()
                .map(|d| format!("{} at line {}", d.qualified(), d.first_line))
                .collect::<Vec<_>>()
                .join(", ")
        )),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lex::lex;

    fn names(src: &[u8]) -> Vec<String> {
        definitions(&lex(src)).iter().map(Def::qualified).collect()
    }

    #[test]
    fn definitions_are_found_however_their_heads_are_spelt() {
        let src = b"#include \"a.h\"\nint g_x = 3;\nint a[] = { 1, 2 };\nstatic int f(int x) { return x; }\nint Cls::Get() const\n{\n    return m_v;\n}\nvoid Cls::~Cls() {}\nCls::Cls(int v) : m_v(v), m_w(0) {\n}\nbool operator==(const A &a, const B &b) { return true; }\nint Cls::operator()(int i) { return i; }\nextern \"C\" int PackRGB(int r,\n                       int g) { return r | g; }\nextern \"C\" {\nint inside(void) { return 0; }\n}\nnamespace N { void nf() {} }\nstruct S { int m; int get() { return m; } } s;\nenum E { A = 1, B };\ntemplate <class T> T maxof(T a, T b) { return a > b ? a : b; }\nLRESULT CALLBACK WndProc(HWND h, UINT m)\n{\n    return 0;\n}\nint proto(int);\n#if 0\nint dead() { return 1; }\n#endif\n";
        assert_eq!(
            names(src),
            [
                "f",
                "Cls::Get",
                "Cls::~Cls",
                "Cls::Cls",
                "operator==",
                "Cls::operator()",
                "PackRGB",
                "inside",
                "N::nf",
                "S::get",
                "maxof",
                "WndProc"
            ]
        );
        let lx = lex(src);
        let defs = definitions(&lx);
        let pack = one(&defs, "PackRGB").unwrap();
        assert!(pack.extern_c);
        assert_eq!((pack.first_line, pack.last_line), (14, 15));
        assert!(one(&defs, "inside").unwrap().extern_c);
        assert!(one(&defs, "f").unwrap().is_static);
        assert!(one(&defs, "get").unwrap().in_class);
        let get = one(&defs, "Cls::Get").unwrap();
        assert_eq!(lx.tokens[get.name_tok].text, "Get");
        assert_eq!(lx.tokens[get.params.0].text, "(");
        assert_eq!(lx.tokens[get.open].text, "{");
        assert_eq!(lx.tokens[get.close].text, "}");
    }

    #[test]
    fn overloads_are_picked_by_occurrence() {
        let src = b"void A::A() {}\nvoid A::A(int) {}\n";
        let lx = lex(src);
        let defs = definitions(&lx);
        assert!(one(&defs, "A::A").is_err());
        assert_eq!(one(&defs, "A::A#2").unwrap().first_line, 2);
        assert!(one(&defs, "nothing").unwrap_err().contains("no function"));
    }
}
