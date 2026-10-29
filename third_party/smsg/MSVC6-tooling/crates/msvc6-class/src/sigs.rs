use crate::decl;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const MAX_CONTINUATION_LINES: usize = 8;

const DECL_ONLY_QUALIFIERS: [&str; 6] = [
    "virtual ",
    "static ",
    "inline ",
    "explicit ",
    "friend ",
    "extern ",
];

const TYPE_KEYWORDS: [&str; 14] = [
    "int", "void", "char", "long", "short", "unsigned", "signed", "float", "double", "const",
    "struct", "class", "enum", "bool",
];

const NAME_NOT: [&str; 12] = [
    "int", "void", "char", "long", "short", "float", "double", "unsigned", "signed", "uchar",
    "ushort", "ulong",
];

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct Site {
    pub declaration: bool,
    pub file: String,
    pub line: usize,
}

pub type Forms = BTreeMap<(String, String), BTreeMap<String, Vec<Site>>>;

#[must_use]
pub fn bases_of(line: &str) -> Option<(&str, Vec<String>)> {
    let t = line.trim_start();
    let rest = t
        .strip_prefix("class ")
        .or_else(|| t.strip_prefix("struct "))?;
    if !rest.contains('{') {
        return None;
    }
    let name = rest
        .split(|c: char| c == ':' || c == '{' || c.is_whitespace())
        .find(|s| !s.is_empty())?;
    if !name.chars().all(|c| c.is_alphanumeric() || c == '_') {
        return None;
    }
    let head = rest.split('{').next().unwrap_or(rest);
    let bases = match head.find(':') {
        None => Vec::new(),
        Some(i) => head
            .get(i + 1..)
            .unwrap_or_default()
            .split(',')
            .filter_map(|b| {
                let mut b = b.trim().trim_start_matches(':').trim();
                while let Some(next) = ["public ", "private ", "protected ", "virtual "]
                    .iter()
                    .find_map(|kw| b.strip_prefix(*kw))
                {
                    b = next.trim_start();
                }
                (!b.is_empty()).then(|| b.to_string())
            })
            .collect(),
    };
    Some((name, bases))
}

#[must_use]
pub fn declared_name(decl: &str) -> Option<String> {
    let ident = |s: &str| -> Option<String> {
        let t = s.trim().trim_start_matches('*').trim();
        (!t.is_empty()
            && t.chars().all(|c| c.is_alphanumeric() || c == '_')
            && !t.starts_with(|c: char| c.is_ascii_digit())
            && !TYPE_KEYWORDS.contains(&t))
        .then(|| t.to_string())
    };
    let head = decl.split('(').next()?;
    if let Some(name) = head
        .split(|c: char| c.is_whitespace() || c == '*' || c == '&')
        .rfind(|s| !s.is_empty())
        .and_then(ident)
    {
        return Some(name);
    }
    decl.split('(').nth(1)?.split(')').next().and_then(ident)
}

#[must_use]
pub fn normalise_signature(decl: &str) -> String {
    let decl = decl.split('{').next().unwrap_or(decl).trim();
    let Some(open) = decl.find('(') else {
        return decl.split_whitespace().collect::<Vec<_>>().join(" ");
    };
    let head: String = decl
        .get(..open)
        .unwrap_or_default()
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .replace(" *", "*")
        .replace("* ", "*")
        .replace(" &", "&")
        .replace("& ", "&");
    let args = decl
        .get(open + 1..)
        .unwrap_or_default()
        .rsplit_once(')')
        .map(|(a, _)| a)
        .unwrap_or_default();
    let params: Vec<String> = args
        .split(',')
        .map(|p| {
            let toks: Vec<&str> = p.split_whitespace().collect();
            if toks.len() < 2 {
                return toks.join(" ");
            }
            let last = toks.last().copied().unwrap_or_default();
            let ident = last.trim_start_matches(['*', '&']);
            let stars = last
                .get(..last.len().saturating_sub(ident.len()))
                .unwrap_or_default();
            let is_name = !ident.is_empty()
                && ident.chars().all(|c| c.is_alphanumeric() || c == '_')
                && !NAME_NOT.contains(&ident);
            if is_name {
                let mut head = toks.get(..toks.len() - 1).unwrap_or_default().join(" ");
                if !stars.is_empty() {
                    head.push(' ');
                    head.push_str(stars);
                }
                head
            } else {
                toks.join(" ")
            }
        })
        .map(|p| p.split_whitespace().collect::<Vec<_>>().join(" "))
        .filter(|p| !p.is_empty())
        .collect();
    format!("{head}({})", params.join(", "))
}

#[must_use]
pub fn logical_lines(src: &str) -> Vec<(usize, String)> {
    let stripped = decl::uncomment(src.as_bytes());
    let mut out = Vec::new();
    let mut buf = String::new();
    let mut start = 0usize;
    let mut held = 0usize;
    for (i, line) in stripped.lines().enumerate() {
        let t = line.trim();
        if buf.is_empty() {
            start = i + 1;
            buf.push_str(t);
            held = 1;
        } else {
            buf.push(' ');
            buf.push_str(t);
            held += 1;
        }
        let depth = buf.chars().fold(0i32, |d, c| match c {
            '(' => d + 1,
            ')' => d - 1,
            _ => d,
        });
        if depth <= 0 || held >= MAX_CONTINUATION_LINES {
            let whole = std::mem::take(&mut buf);
            let mut any = false;
            for part in whole.split_inclusive(';') {
                let t = part.trim();
                if !t.is_empty() {
                    out.push((start, t.to_string()));
                    any = true;
                }
            }
            if !any {
                out.push((start, whole));
            }
        }
    }
    if !buf.is_empty() {
        out.push((start, buf));
    }
    out
}

#[must_use]
pub fn canonical_sig(sig: &str) -> String {
    let mut s = sig.replace("(void)", "()");
    s = s.split_whitespace().collect::<Vec<_>>().join(" ");
    for p in ['*', '&', ',', '(', ')'] {
        s = s.replace(&format!(" {p}"), &p.to_string());
        s = s.replace(&format!("{p} "), &p.to_string());
    }
    s
}

#[must_use]
pub fn decay_arrays(text: &str) -> String {
    let mut s = text.to_string();
    let mut from = 0usize;
    while let Some(open) = s.get(from..).and_then(|t| t.find('[')).map(|i| from + i) {
        let Some(close) = s.get(open..).and_then(|t| t.find(']')).map(|i| open + i) else {
            break;
        };
        let head = s.get(..open).unwrap_or_default();
        let trimmed = head.trim_end();
        let ident_start = trimmed
            .rfind(|c: char| !(c.is_alphanumeric() || c == '_'))
            .map_or(0, |i| i + 1);
        let has_ident = ident_start < trimmed.len();
        s.replace_range(open..=close, "");
        if has_ident {
            s.insert(ident_start, '*');
            from = ident_start + 1;
        } else {
            s.insert(open, '*');
            from = open + 1;
        }
    }
    s
}

fn arity(sig: &str) -> usize {
    let (Some(open), Some(close)) = (sig.find('('), sig.rfind(')')) else {
        return 0;
    };
    let inner = sig.get(open + 1..close).unwrap_or_default().trim();
    if inner.is_empty() || inner == "void" {
        return 0;
    }
    let mut depth = 0i32;
    let mut n = 1usize;
    for c in inner.chars() {
        match c {
            '(' | '<' => depth += 1,
            ')' | '>' => depth -= 1,
            ',' if depth == 0 => n += 1,
            _ => {}
        }
    }
    n
}

#[must_use]
pub fn strip_decl_only(text: &str) -> String {
    let mut s = text.trim().to_string();
    loop {
        let before = s.clone();
        for q in DECL_ONLY_QUALIFIERS {
            if let Some(rest) = s.strip_prefix(q) {
                s = rest.trim_start().to_string();
            }
        }
        if s == before {
            break;
        }
    }
    let mut out = String::with_capacity(s.len());
    let mut skipping = false;
    let mut depth = 0i32;
    for c in s.chars() {
        match c {
            '(' => depth += 1,
            ')' => depth -= 1,
            _ => {}
        }
        if skipping {
            if c == ',' || (c == ')' && depth == 0) {
                skipping = false;
            } else {
                continue;
            }
        } else if c == '=' && depth > 0 {
            skipping = true;
            continue;
        }
        out.push(c);
    }
    out
}

#[derive(Debug, Clone, Default)]
pub struct Linkage {
    words: BTreeSet<String>,
}

impl Linkage {
    #[must_use]
    pub fn of(files: &[(String, String)]) -> Self {
        let mut words = BTreeSet::new();
        for (_, src) in files {
            for line in src.lines() {
                let Some(rest) = line.trim_start().strip_prefix('#') else {
                    continue;
                };
                let Some(rest) = rest.trim_start().strip_prefix("define") else {
                    continue;
                };
                let mut it = rest.split_whitespace();
                let Some(name) = it.next() else { continue };
                if name.contains('(')
                    || !name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_')
                {
                    continue;
                }
                let body: Vec<&str> = it.collect();
                if body.is_empty() || body.first().is_some_and(|b| b.starts_with("__declspec")) {
                    words.insert(name.to_string());
                }
            }
        }
        Self { words }
    }

    #[must_use]
    pub fn strip(&self, text: &str) -> String {
        let mut s = text.to_string();
        while let Some(at) = s.find("__declspec") {
            let Some(close) = s.get(at..).and_then(|t| t.find(')')).map(|i| at + i) else {
                break;
            };
            s.replace_range(at..=close, " ");
        }
        if self.words.is_empty() {
            return s;
        }
        s.split_inclusive(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
            .map(|piece| {
                let word =
                    piece.trim_end_matches(|c: char| !(c.is_ascii_alphanumeric() || c == '_'));
                if self.words.contains(word) {
                    piece.get(word.len()..).unwrap_or_default().to_string()
                } else {
                    piece.to_string()
                }
            })
            .collect()
    }
}

fn declaration(text: &str, link: &Linkage) -> Option<(String, String)> {
    let t = strip_decl_only(&decay_arrays(&link.strip(text)));
    let t = t.trim_end();
    if !t.ends_with(';') || !t.contains('(') || t.contains("(*") {
        return None;
    }
    let body = t.trim_end_matches([';', ' ']);
    let name = declared_name(body)?;
    Some((name, canonical_sig(&normalise_signature(body))))
}

fn definition(text: &str, link: &Linkage) -> Option<(String, String, String)> {
    let t = strip_decl_only(&decay_arrays(&link.strip(text)));
    let head = t.split('(').next()?;
    let (before, member) = head.rsplit_once("::")?;
    let member = member.trim();
    let class = before
        .rsplit(|c: char| c.is_whitespace() || c == '*' || c == '&')
        .find(|s| !s.is_empty())?
        .trim();
    let ok = |s: &str| {
        !s.is_empty()
            && s.chars()
                .all(|c| c.is_alphanumeric() || c == '_' || c == '~')
    };
    if !ok(class) || !ok(member) {
        return None;
    }
    let unqualified = t.replacen(&format!("{class}::"), "", 1);
    let body = unqualified.trim_end_matches([';', '{', ' ']);
    Some((
        class.to_string(),
        member.to_string(),
        canonical_sig(&normalise_signature(body)),
    ))
}

#[must_use]
pub fn disagrees(f: &BTreeMap<String, Vec<Site>>) -> bool {
    let decls: BTreeSet<&String> = f
        .iter()
        .filter(|(_, s)| s.iter().any(|x| x.declaration))
        .map(|(sig, _)| sig)
        .collect();
    let defns: BTreeSet<&String> = f
        .iter()
        .filter(|(_, s)| s.iter().any(|x| !x.declaration))
        .map(|(sig, _)| sig)
        .collect();
    if decls.is_empty() || defns.is_empty() {
        return false;
    }
    let lone: Vec<&&String> = decls.difference(&defns).collect();
    defns
        .difference(&decls)
        .any(|d| lone.iter().any(|c| arity(d) == arity(c)))
}

pub fn collect(name: &str, src: &str, link: &Linkage, forms: &mut Forms) {
    let mut stack: Vec<(String, i32)> = Vec::new();
    let mut depth = 0i32;
    for (line, text) in logical_lines(src) {
        let opened = bases_of(&text).map(|(n, _)| n.to_string());
        let inside = stack.last().is_some_and(|(_, d)| depth == *d + 1);
        if let Some((class, _)) = stack.last() {
            if inside && let Some((member, sig)) = declaration(&text, link) {
                forms
                    .entry((class.clone(), member))
                    .or_default()
                    .entry(sig)
                    .or_default()
                    .push(Site {
                        declaration: true,
                        file: name.to_string(),
                        line,
                    });
            }
        } else if depth == 0
            && let Some((class, member, sig)) = definition(&text, link)
        {
            forms
                .entry((class, member))
                .or_default()
                .entry(sig)
                .or_default()
                .push(Site {
                    declaration: false,
                    file: name.to_string(),
                    line,
                });
        }
        if let Some(c) = opened {
            stack.push((c, depth));
        }
        for ch in text.chars() {
            match ch {
                '{' => depth += 1,
                '}' => {
                    depth -= 1;
                    if stack.last().is_some_and(|(_, open)| depth <= *open) {
                        stack.pop();
                    }
                }
                _ => {}
            }
        }
    }
}

#[derive(Debug, Clone, Default, Serialize)]
pub struct Report {
    pub files: usize,
    pub pairs: usize,
    pub comparable: usize,
    pub both: usize,
    pub disagreeing: Vec<Row>,
    pub strong: usize,
    pub weak: usize,
}

#[derive(Debug, Clone, Serialize)]
pub struct Row {
    pub class: String,
    pub member: String,
    pub forms: Vec<(String, Vec<Site>)>,
}

#[must_use]
pub fn screen(files: &[(String, String)], keep_linkage: bool) -> Report {
    let link = if keep_linkage {
        Linkage::default()
    } else {
        Linkage::of(files)
    };
    let mut forms: Forms = BTreeMap::new();
    for (name, src) in files {
        let file = name.rsplit('/').next().unwrap_or(name);
        collect(file, src, &link, &mut forms);
    }
    let comparable: Vec<_> = forms
        .iter()
        .filter(|(_, f)| f.values().map(Vec::len).sum::<usize>() >= 2)
        .collect();
    let both = comparable
        .iter()
        .filter(|(_, f)| {
            f.values().flatten().any(|s| s.declaration)
                && f.values().flatten().any(|s| !s.declaration)
        })
        .count();
    let mut r = Report {
        files: files.len(),
        pairs: forms.len(),
        comparable: comparable.len(),
        both,
        ..Report::default()
    };
    for ((class, member), f) in comparable.iter().filter(|(_, f)| disagrees(f)) {
        let has_defn = f.values().flatten().any(|s| !s.declaration);
        let has_decl = f.values().flatten().any(|s| s.declaration);
        if has_defn && has_decl {
            r.strong += 1;
        } else {
            r.weak += 1;
        }
        r.disagreeing.push(Row {
            class: class.clone(),
            member: member.clone(),
            forms: f
                .iter()
                .map(|(sig, sites)| {
                    let mut s = sites.clone();
                    s.sort_by_key(|x| !x.declaration);
                    (sig.clone(), s)
                })
                .collect(),
        });
    }
    r
}

#[must_use]
pub fn report(r: &Report, label: &str) -> Vec<String> {
    let mut out = vec![
        format!("{label}: DOES EACH CLASS MEMBER'S DECLARATION AGREE WITH ITS DEFINITION?"),
        format!(
            "  POPULATION: {} file(s) parsed, {} distinct (class, member) pair(s), of which {} were written down at least TWICE and are therefore comparable, and {} have BOTH a declaration and a definition.",
            r.files, r.pairs, r.comparable, r.both
        ),
    ];
    for row in &r.disagreeing {
        let kind = if row
            .forms
            .iter()
            .flat_map(|(_, s)| s)
            .any(|s| !s.declaration)
        {
            "DECL vs DEFN"
        } else {
            "DECL vs DECL"
        };
        out.push(format!(
            "  {kind}  {}::{} -- {} forms",
            row.class,
            row.member,
            row.forms.len()
        ));
        for (sig, sites) in &row.forms {
            let at: Vec<String> = sites
                .iter()
                .map(|s| {
                    format!(
                        "{}:{}{}",
                        s.file,
                        s.line,
                        if s.declaration { "" } else { " [DEFN]" }
                    )
                })
                .collect();
            out.push(format!("      {sig}   [{}]", at.join(" ")));
        }
    }
    let classes: BTreeSet<&String> = r.disagreeing.iter().map(|x| &x.class).collect();
    out.push(format!(
        "  -> {} pair(s) disagree over {} class(es): {} with a DEFINITION on one side (the strong case -- the definition was scored against the image, so suspect the declaration) and {} between declarations only (at least one is wrong; this screen cannot say which).{}",
        r.disagreeing.len(),
        classes.len(),
        r.strong,
        r.weak,
        if r.disagreeing.is_empty() {
            " No pair disagrees, so no member is declared one way and defined another."
        } else {
            ""
        }
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn forms(src: &str, link: &Linkage) -> Forms {
        let mut f = Forms::new();
        collect("t.cpp", src, link, &mut f);
        f
    }

    #[test]
    fn a_char_against_int_at_equal_arity_disagrees_and_overloads_do_not() {
        let none = Linkage::default();
        let src = "class Sprite {\npublic:\n    int Blend(Surface *dst, int x, int y, char alpha);\n};\nint Sprite::Blend(Surface *dst, int x, int y, int alpha)\n{\n    return 0;\n}\n";
        let f = forms(src, &none);
        assert!(
            f.get(&("Sprite".into(), "Blend".into()))
                .is_some_and(disagrees)
        );
        let src = "class M {\npublic:\n    M(const Q&);\n    M(const V *);\n};\nM::M(const Q&)\n{\n}\nM::M(const V *)\n{\n}\n";
        let f = forms(src, &none);
        assert!(
            f.get(&("M".into(), "M".into()))
                .is_some_and(|e| !disagrees(e))
        );
        let lone = forms("class S {\npublic:\n    int Only(int);\n};\n", &none);
        assert!(lone.values().all(|e| !disagrees(e)));
    }

    #[test]
    fn qualifiers_defaults_void_spacing_and_arrays_are_not_differences() {
        let none = Linkage::default();
        let d = |t| declaration(t, &none).map(|(_, s)| s);
        let f = |t| definition(t, &none).map(|(_, _, s)| s);
        assert_eq!(
            d("virtual int Draw(Surface *dst, int x);"),
            f("int Sprite::Draw(Surface *dst, int x)")
        );
        assert_eq!(
            d("int Fill(int color = 0);"),
            f("int Surface::Fill(int color)")
        );
        assert_eq!(d("void f(int p[5]);"), f("void C::f(int *p)"));
        assert_eq!(canonical_sig("void*f(void)"), canonical_sig("void*f()"));
        assert_eq!(
            canonical_sig("M*A(const M *)"),
            canonical_sig("M*A(const M*)")
        );
        assert!(declaration("int m_width;", &none).is_none());
        assert!(declaration("int (*m_hook)(int);", &none).is_none());
        assert_eq!(
            definition("Sprite::~Sprite()", &none).map(|(c, m, _)| (c, m)),
            Some(("Sprite".to_string(), "~Sprite".to_string()))
        );
        assert_eq!(arity("int f(int(*)(int,int),char)"), 2);
        assert!(logical_lines("    virtual void p00(); virtual void p04();\n").len() >= 2);
        assert!(
            logical_lines("int C::f(Surface *dst, int x,\n        int y, int a)\n")[0]
                .1
                .contains("int a")
        );
    }

    #[test]
    fn an_export_macro_is_linkage_and_not_part_of_the_signature() {
        let header = "#ifdef BUILDING\n#define LIB_API __declspec(dllexport)\n#else\n#define LIB_API __declspec(dllimport)\n#endif\nclass Lib {\npublic:\n    LIB_API void calc(Tile *t);\n};\n";
        let source = "void Lib::calc(Tile *t)\n{\n}\n";
        let files = vec![
            ("include/lib.h".to_string(), header.to_string()),
            ("src/lib.cpp".to_string(), source.to_string()),
        ];
        assert!(screen(&files, false).disagreeing.is_empty());
        let legacy = screen(&files, true);
        assert_eq!(legacy.disagreeing.len(), 1);
        let lines = report(&legacy, "IMG");
        assert!(
            lines
                .iter()
                .any(|l| l == "      LIB_API void calc(Tile*)   [lib.h:8]"),
            "{lines:#?}"
        );
    }
}
