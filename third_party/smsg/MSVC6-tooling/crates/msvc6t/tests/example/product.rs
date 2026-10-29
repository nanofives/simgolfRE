use msvc6_source::lex::{Cond, Kind, Token};
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::path::{Path, PathBuf};

pub const SOURCE_EXTENSIONS: [&str; 15] = [
    "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "inl", "inc", "asm", "def", "rc", "dsp",
    "dsw",
];

pub const HEADER_EXTENSIONS: [&str; 6] = ["h", "hh", "hpp", "hxx", "inl", "inc"];

pub const LEXED_EXTENSIONS: [&str; 11] = [
    "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "inl", "inc", "def",
];

pub const SPECIAL_WORDS: [&str; 77] = [
    "thunk",
    "adjustor",
    "public",
    "private",
    "protected",
    "static",
    "virtual",
    "const",
    "volatile",
    "class",
    "struct",
    "union",
    "enum",
    "void",
    "bool",
    "char",
    "signed",
    "unsigned",
    "short",
    "int",
    "long",
    "float",
    "double",
    "wchar_t",
    "__int8",
    "__int16",
    "__int32",
    "__int64",
    "__w64",
    "__ptr64",
    "__unaligned",
    "__restrict",
    "__cdecl",
    "__stdcall",
    "__thiscall",
    "__fastcall",
    "__pascal",
    "__clrcall",
    "__vectorcall",
    "__based",
    "operator",
    "new",
    "delete",
    "vftable",
    "vbtable",
    "vcall",
    "vtordisp",
    "vtordispex",
    "scalar",
    "vector",
    "deleting",
    "destructor",
    "constructor",
    "iterator",
    "vbase",
    "eh",
    "dynamic",
    "initializer",
    "atexit",
    "for",
    "local",
    "guard",
    "string",
    "copy",
    "closure",
    "default",
    "placement",
    "udt",
    "returning",
    "anonymous",
    "namespace",
    "RTTI",
    "Type",
    "Descriptor",
    "Hierarchy",
    "Complete",
    "Locator",
];

pub const RTTI_WORDS: [&str; 5] = ["Base", "Class", "Array", "Object", "at"];

pub const ADJUSTOR_MARKS: [(&str, &str); 4] = [
    ("@@$4", "vtordisp"),
    ("@@$5", "vtordisp"),
    ("@@$R", "vtordispex"),
    ("@@$B", "vcall"),
];

pub const HANDLE_TAG: &str = "__";

pub const HANDLE_MACRO: &str = "DECLARE_HANDLE";

pub const PRODUCT_QUOTE: &str = "product `";

pub const TYPE_KEYWORDS: [&str; 4] = ["class", "struct", "union", "enum"];

pub const C_LINKAGE: &str = "\"C\"";

#[derive(Debug, Clone, PartialEq, Eq)]
enum Block {
    Plain,
    Namespace(String),
    CLinkage,
}

pub const STD_SCOPE: &str = "std";

pub const JMP_REL32: u8 = 0xe9;

pub const JMP_INDIRECT: [u8; 2] = [0xff, 0x25];

pub const ILT_STRIDE: i64 = 5;

pub const STUB_STRIDE: i64 = 6;

fn extension(rel: &str) -> Option<String> {
    let name = rel.rsplit('/').next().unwrap_or(rel);
    match name.rsplit_once('.') {
        Some((stem, ext)) if !stem.is_empty() => Some(ext.to_ascii_lowercase()),
        _ => None,
    }
}

fn walk(root: &Path, rel: &str, depth: usize, out: &mut Vec<String>) -> Result<(), String> {
    if depth > 64 {
        return Err(format!("{rel}: more than 64 directories deep"));
    }
    let dir = if rel.is_empty() {
        root.to_path_buf()
    } else {
        root.join(rel)
    };
    let mut entries = std::fs::read_dir(&dir)
        .map_err(|e| format!("{}: {e}", dir.display()))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|e| format!("{}: {e}", dir.display()))?;
    entries.sort_by_key(std::fs::DirEntry::file_name);
    for e in entries {
        let name = e.file_name().to_string_lossy().into_owned();
        if name == ".git" {
            continue;
        }
        let path = if rel.is_empty() {
            name.clone()
        } else {
            format!("{rel}/{name}")
        };
        let ty = e
            .file_type()
            .map_err(|err| format!("{}: {err}", e.path().display()))?;
        if ty.is_dir() {
            walk(root, &path, depth + 1, out)?;
        } else if ty.is_file() {
            out.push(path);
        }
    }
    Ok(())
}

#[must_use]
pub fn words(s: &str) -> Vec<&str> {
    s.split(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
        .filter(|w| w.chars().next().is_some_and(|c| !c.is_ascii_digit()))
        .collect()
}

fn plain(s: &str) -> bool {
    !s.is_empty()
        && s.chars().all(|c| c.is_ascii_alphanumeric() || c == '_')
        && s.chars().next().is_some_and(|c| !c.is_ascii_digit())
}

#[must_use]
pub fn components(qualified: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut depth = 0usize;
    let mut cur = String::new();
    let mut chars = qualified.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '<' | '`' => depth += 1,
            '>' | '\'' => depth = depth.saturating_sub(1),
            ':' if depth == 0 && chars.peek() == Some(&':') => {
                chars.next();
                out.push(std::mem::take(&mut cur));
                continue;
            }
            _ => {}
        }
        cur.push(c);
    }
    out.push(cur);
    out
}

#[must_use]
pub fn member_of(qualified: &str) -> Option<(String, String)> {
    let parts = components(qualified);
    let n = parts.len();
    if n < 2 {
        return None;
    }
    let member = parts.get(n - 1)?.trim();
    let member = member.split('`').next().unwrap_or(member);
    let member = member.strip_prefix('~').unwrap_or(member);
    let class = parts.get(n - 2)?.trim();
    let class = class.split('<').next().unwrap_or(class).trim();
    plain(class).then(|| (class.to_string(), member.to_string()))
}

#[must_use]
pub fn type_words(demangled: &str) -> BTreeSet<String> {
    let mut out = BTreeSet::new();
    let ws = words(demangled);
    for pair in ws.windows(2) {
        if let [k, t] = pair
            && TYPE_KEYWORDS.contains(k)
        {
            out.insert((*t).to_string());
        }
    }
    out
}

fn strip_arg_bytes(s: &str) -> &str {
    match s.rsplit_once('@') {
        Some((n, k)) if !n.is_empty() && !k.is_empty() && k.chars().all(|c| c.is_ascii_digit()) => {
            n
        }
        _ => s,
    }
}

#[must_use]
pub fn adjustor(decorated: &str) -> Option<String> {
    let (at, kind) = ADJUSTOR_MARKS
        .iter()
        .filter_map(|(m, kind)| decorated.find(m).map(|at| (at, *kind)))
        .min()?;
    let head = decorated.get(..at)?;
    let head = head
        .strip_prefix("??_E")
        .or_else(|| head.strip_prefix("??_G"))
        .map(|class| format!("{class}::`deleting destructor'"))
        .or_else(|| {
            let (member, class) = head.strip_prefix('?')?.split_once('@')?;
            (plain(member) && !class.is_empty()).then(|| {
                let scope: Vec<&str> = class.split('@').rev().collect();
                format!("{}::{member}", scope.join("::"))
            })
        })?;
    Some(format!("{head}`{kind}'"))
}

#[must_use]
pub fn readable(name: &str) -> String {
    if let Some(r) = name.strip_prefix("_?") {
        return readable(&format!("?{r}"));
    }
    if name.starts_with('?') {
        return msvc6_match::mac::types::msvc_qualified(name)
            .or_else(|| adjustor(name))
            .unwrap_or_else(|| name.to_string());
    }
    let s = name
        .strip_prefix('@')
        .or_else(|| name.strip_prefix('_'))
        .unwrap_or(name);
    strip_arg_bytes(s).to_string()
}

#[derive(Debug, Default)]
pub struct Product {
    files: Vec<String>,
    names: BTreeSet<String>,
}

impl Product {
    pub fn read(root: &Path) -> Result<Product, String> {
        let mut files = Vec::new();
        walk(root, "", 0, &mut files)?;
        let names = files
            .iter()
            .map(|f| f.rsplit('/').next().unwrap_or(f).to_ascii_lowercase())
            .collect();
        Ok(Product { files, names })
    }

    #[must_use]
    pub fn files(&self) -> &[String] {
        &self.files
    }

    #[must_use]
    pub fn has_file_named(&self, name: &str) -> bool {
        self.names.contains(&name.to_ascii_lowercase())
    }

    #[must_use]
    pub fn has_path(&self, rel: &str) -> bool {
        let rel = rel.trim_end_matches('/');
        rel.is_empty()
            || self
                .files
                .iter()
                .any(|f| f == rel || f.strip_prefix(rel).is_some_and(|r| r.starts_with('/')))
    }

    #[must_use]
    pub fn has_path_folded(&self, rel: &str) -> bool {
        self.files.iter().any(|f| f.eq_ignore_ascii_case(rel))
    }

    #[must_use]
    pub fn matches(&self, pattern: &str) -> bool {
        self.files
            .iter()
            .any(|f| msvc6_source::glob::whole(pattern, f))
    }

    #[must_use]
    pub fn absent_files(&self, text: &str) -> Vec<String> {
        let mut out = Vec::new();
        for w in text.split(|c: char| !(c.is_ascii_alphanumeric() || "_.-".contains(c))) {
            let w = w.trim_end_matches('.');
            let Some((stem, ext)) = w.rsplit_once('.') else {
                continue;
            };
            if stem.is_empty()
                || !SOURCE_EXTENSIONS.contains(&ext.to_ascii_lowercase().as_str())
                || self.has_file_named(w)
            {
                continue;
            }
            if !out.iter().any(|o| o == w) {
                out.push(w.to_string());
            }
        }
        out
    }
}

fn skip_group(code: &[&Token], open: usize, l: &str, r: &str) -> Option<usize> {
    let mut depth = 0usize;
    for (k, t) in code.iter().enumerate().skip(open) {
        if t.is_punct(l) {
            depth += 1;
        } else if t.is_punct(r) {
            depth = depth.checked_sub(1)?;
            if depth == 0 {
                return Some(k);
            }
        } else if l == "<" && (t.is_punct(";") || t.is_punct("{") || t.is_punct("}")) {
            return None;
        }
    }
    None
}

fn class_head(code: &[&Token], at: usize) -> Option<(String, usize)> {
    let mut name: Option<String> = None;
    let mut j = at + 1;
    loop {
        let t = code.get(j)?;
        if t.is("__declspec") {
            j = skip_group(code, j + 1, "(", ")")? + 1;
        } else if t.is_ident() {
            name = Some(t.text.clone());
            j += 1;
        } else if t.is_punct("::") {
            j += 1;
        } else if t.is_punct("<") && name.is_some() {
            j = skip_group(code, j, "<", ">")? + 1;
        } else if t.is_punct(":") {
            let open = code
                .iter()
                .enumerate()
                .skip(j)
                .find(|(_, t)| t.is_punct("{") || t.is_punct(";"))?;
            return (open.1.is_punct("{")).then_some((name?, open.0));
        } else if t.is_punct("{") {
            return Some((name?, j));
        } else {
            return None;
        }
    }
}

fn body_idents(code: &[&Token], open: usize) -> BTreeSet<String> {
    let mut out = BTreeSet::new();
    let mut depth = 0usize;
    let mut paren = 0usize;
    for t in code.iter().skip(open) {
        if t.is_punct("{") {
            depth += 1;
        } else if t.is_punct("}") {
            depth = depth.saturating_sub(1);
            if depth == 0 {
                break;
            }
        } else if depth == 1 && t.is_punct("(") {
            paren += 1;
        } else if depth == 1 && t.is_punct(")") {
            paren = paren.saturating_sub(1);
        } else if depth == 1 && paren == 0 && t.is_ident() {
            out.insert(t.text.clone());
        }
    }
    out
}

fn is_definition(code: &[&Token], open: usize) -> bool {
    if !code.get(open).is_some_and(|t| t.is_punct("(")) {
        return false;
    }
    let Some(close) = skip_group(code, open, "(", ")") else {
        return false;
    };
    code.iter()
        .skip(close + 1)
        .find(|t| t.is_punct("{") || t.is_punct(";") || t.is_punct("=") || t.is_punct(","))
        .is_some_and(|t| t.is_punct("{"))
}

#[derive(Debug, Default)]
pub struct Sources {
    files: Vec<String>,
    tokens: HashMap<String, Vec<usize>>,
    classes: HashMap<String, BTreeSet<String>>,
    qualified: HashSet<(String, String)>,
    defs: HashMap<String, BTreeSet<usize>>,
    macros: HashMap<String, BTreeSet<String>>,
    tags: BTreeSet<String>,
    functions: HashSet<String>,
    c_linkage: HashSet<String>,
    c_units: HashSet<String>,
    cpp_units: HashSet<String>,
    namespaces: HashSet<String>,
}

impl Sources {
    pub fn read(root: &Path, dirs: &[String], extensionless: bool) -> Result<Sources, String> {
        let mut s = Sources::default();
        s.add(root, dirs, &LEXED_EXTENSIONS, extensionless)?;
        Ok(s)
    }

    pub fn add(
        &mut self,
        root: &Path,
        dirs: &[String],
        exts: &[&str],
        extensionless: bool,
    ) -> Result<(), String> {
        for d in dirs {
            let mut files = Vec::new();
            walk(root, d.trim_end_matches('/'), 0, &mut files)?;
            for rel in files {
                let lexed = match extension(&rel) {
                    Some(e) => exts.contains(&e.as_str()),
                    None => extensionless,
                };
                if lexed && !self.files.contains(&rel) {
                    let at = root.join(&rel);
                    let bytes = std::fs::read(&at).map_err(|e| format!("{}: {e}", at.display()))?;
                    self.scan(&rel, &bytes);
                }
            }
        }
        Ok(())
    }

    pub fn scan(&mut self, rel: &str, bytes: &[u8]) {
        let lx = msvc6_source::lex::lex(bytes);
        let k = self.files.len();
        self.files.push(rel.to_string());
        let mut seen: BTreeSet<&str> = BTreeSet::new();
        for t in &lx.tokens {
            if t.kind != Kind::Ident || t.cond == Cond::Dead {
                continue;
            }
            let included = t
                .directive
                .and_then(|d| lx.directives.get(d))
                .is_some_and(|d| d.name == "include" || d.name == "import");
            if !included {
                seen.insert(t.text.as_str());
            }
        }
        for w in seen {
            self.tokens.entry(w.to_string()).or_default().push(k);
        }
        for d in &lx.directives {
            if d.name == "define" && d.cond != Cond::Dead {
                let mut it = lx.args(d).iter().filter(|t| t.is_ident());
                if let Some(name) = it.next() {
                    self.macros
                        .entry(name.text.clone())
                        .or_default()
                        .extend(it.map(|t| t.text.clone()));
                }
            }
        }
        let code: Vec<&Token> = lx.tokens.iter().filter(|t| t.in_code()).collect();
        let unit = match extension(rel).as_deref() {
            Some("c") => Some(true),
            Some("cpp" | "cc" | "cxx") => Some(false),
            _ => None,
        };
        self.structure(&code, k, unit);
    }

    fn structure(&mut self, code: &[&Token], k: usize, unit: Option<bool>) {
        let mut open: Vec<Block> = Vec::new();
        let mut next_block = Block::Plain;
        let mut c_single = false;
        for (i, t) in code.iter().enumerate() {
            if t.is_punct("{") {
                if next_block == Block::Plain {
                    c_single = false;
                }
                open.push(std::mem::replace(&mut next_block, Block::Plain));
                continue;
            }
            if t.is_punct("}") {
                open.pop();
                continue;
            }
            let wrappers = open.iter().filter(|b| **b != Block::Plain).count();
            let c_blocks = open.iter().filter(|b| **b == Block::CLinkage).count();
            let depth = open.len();
            if t.is_punct(";") {
                c_single = false;
                continue;
            }
            if !t.is_ident() {
                continue;
            }
            let text = t.text.as_str();
            if text == "extern"
                && code
                    .get(i + 1)
                    .is_some_and(|s| s.kind == Kind::Str && s.text == C_LINKAGE)
            {
                if code.get(i + 2).is_some_and(|b| b.is_punct("{")) {
                    next_block = Block::CLinkage;
                } else {
                    c_single = true;
                }
                continue;
            }
            if text == "namespace"
                && code.get(i + 2).is_some_and(|b| b.is_punct("{"))
                && let Some(n) = code.get(i + 1).filter(|n| n.is_ident())
            {
                self.namespaces.insert(n.text.clone());
                next_block = Block::Namespace(n.text.clone());
                continue;
            }
            let scope_depth = depth.saturating_sub(wrappers);
            if TYPE_KEYWORDS.contains(&text) {
                let mut j = i + 1;
                if code.get(j).is_some_and(|n| n.is("__declspec")) {
                    j = skip_group(code, j + 1, "(", ")").map_or(j, |c| c + 1);
                }
                if let Some(n) = code.get(j).filter(|n| n.is_ident()) {
                    self.tags.insert(n.text.clone());
                }
                if text != "enum"
                    && let Some((name, open_at)) = class_head(code, i)
                {
                    let body = body_idents(code, open_at);
                    self.classes.entry(name).or_default().extend(body);
                }
                continue;
            }
            if text == HANDLE_MACRO
                && let (Some(p), Some(n), Some(c)) =
                    (code.get(i + 1), code.get(i + 2), code.get(i + 3))
                && p.is_punct("(")
                && n.is_ident()
                && c.is_punct(")")
            {
                self.tags.insert(format!("{}{HANDLE_TAG}", n.text));
            }
            let mut j = i + 1;
            if code.get(j).is_some_and(|n| n.is_punct("<")) {
                j = skip_group(code, j, "<", ">").map_or(j, |c| c + 1);
            }
            if code.get(j).is_some_and(|n| n.is_punct("::")) {
                let (member, after) = match (code.get(j + 1), code.get(j + 2)) {
                    (Some(m), _) if m.is_ident() => (Some(m.text.clone()), j + 2),
                    (Some(tilde), Some(m)) if tilde.is_punct("~") && m.is_ident() => {
                        (Some(m.text.clone()), j + 3)
                    }
                    _ => (None, j),
                };
                if let Some(m) = member {
                    if is_definition(code, after) {
                        self.defs
                            .entry(format!("{text}::{m}"))
                            .or_default()
                            .insert(k);
                    }
                    self.qualified.insert((text.to_string(), m));
                }
                continue;
            }
            let scoped = i > 0 && code.get(i - 1).is_some_and(|p| p.is_punct("::"));
            if scope_depth == 0 && !scoped && code.get(i + 1).is_some_and(|n| n.is_punct("(")) {
                let typed = i > 0
                    && code
                        .get(i - 1)
                        .is_some_and(|p| p.is_ident() || p.is_punct("*") || p.is_punct("&"));
                if typed {
                    let scope: Vec<&str> = open
                        .iter()
                        .filter_map(|b| match b {
                            Block::Namespace(n) => Some(n.as_str()),
                            _ => None,
                        })
                        .collect();
                    let name = if scope.is_empty() {
                        text.to_string()
                    } else {
                        format!("{}::{text}", scope.join("::"))
                    };
                    if c_blocks > 0 || c_single {
                        self.c_linkage.insert(name.clone());
                    }
                    match unit {
                        Some(true) => {
                            self.c_units.insert(name.clone());
                        }
                        Some(false) => {
                            self.cpp_units.insert(name.clone());
                        }
                        None => {}
                    }
                    self.functions.insert(name);
                }
                if is_definition(code, i + 1) {
                    self.defs.entry(text.to_string()).or_default().insert(k);
                }
            }
        }
    }

    #[must_use]
    pub fn files(&self) -> &[String] {
        &self.files
    }

    fn found(&self, t: &str) -> Option<&Vec<usize>> {
        self.tokens.get(t).or_else(|| {
            t.strip_suffix(HANDLE_TAG)
                .filter(|r| plain(r))
                .and_then(|r| self.tokens.get(r))
        })
    }

    #[must_use]
    pub fn has_token(&self, t: &str) -> bool {
        self.found(t).is_some()
    }

    #[must_use]
    pub fn class_defined(&self, c: &str) -> bool {
        self.classes.contains_key(c)
    }

    #[must_use]
    pub fn has_tag(&self, t: &str) -> bool {
        self.tags.contains(t) || self.classes.contains_key(t)
    }

    #[must_use]
    pub fn c_function(&self, f: &str) -> bool {
        self.c_linkage.contains(f) || (self.c_units.contains(f) && !self.cpp_units.contains(f))
    }

    #[must_use]
    pub fn cpp_function(&self, f: &str) -> bool {
        self.functions.contains(f) && !self.c_function(f)
    }

    #[must_use]
    pub fn is_namespace(&self, n: &str) -> bool {
        self.namespaces.contains(n)
    }

    #[must_use]
    pub fn has_member(&self, c: &str, m: &str) -> bool {
        if self.qualified.contains(&(c.to_string(), m.to_string())) {
            return true;
        }
        self.classes.get(c).is_some_and(|body| {
            body.contains(m)
                || body
                    .iter()
                    .filter_map(|b| self.macros.get(b))
                    .any(|expansion| expansion.contains(m))
        })
    }

    #[must_use]
    pub fn has_stem(&self, stem: &str) -> bool {
        self.files.iter().any(|f| {
            let n = f.rsplit('/').next().unwrap_or(f);
            n.rsplit_once('.')
                .is_some_and(|(st, _)| st.eq_ignore_ascii_case(stem))
        })
    }

    #[must_use]
    pub fn definers(&self, key: &str) -> Vec<&str> {
        self.defs
            .get(key)
            .map(|ks| {
                ks.iter()
                    .filter_map(|k| self.files.get(*k).map(String::as_str))
                    .collect()
            })
            .unwrap_or_default()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Row {
    pub rva: u32,
    pub name: String,
    pub rest: String,
}

#[derive(Debug, Default)]
pub struct Verdict {
    pub kept: Vec<Row>,
    pub left_out: Vec<(Row, String)>,
    pub renamed: Vec<(Row, String)>,
}

#[derive(Debug)]
pub struct Names {
    pub own: Sources,
    pub library: std::rc::Rc<Sources>,
    pub archived: HashSet<String>,
}

impl Names {
    fn known(&self, w: &str, types: &BTreeSet<String>, library: bool) -> bool {
        self.own.has_token(w)
            || (types.contains(w) && (self.own.has_tag(w) || self.library.has_tag(w)))
            || (library && self.library.has_token(w))
    }

    #[must_use]
    pub fn name_problems(&self, name: &str) -> Vec<String> {
        if self.archived.contains(name) {
            return Vec::new();
        }
        if let Some(r) = name.strip_prefix("_?") {
            return self.name_problems(&format!("?{r}"));
        }
        if name.starts_with('?') {
            return self.decorated_problems(name);
        }
        let s = name.strip_prefix('@').unwrap_or(name);
        let s = strip_arg_bytes(s);
        if let Some((head, _)) = s.split_once('$') {
            let stem = head.trim_start_matches('_');
            return if stem.is_empty() || self.own.has_token(stem) || self.own.has_stem(stem) {
                Vec::new()
            } else {
                vec![format!(
                    "{stem}, no token or source file stem of the image's own"
                )]
            };
        }
        let candidates = [
            s,
            s.strip_prefix('_').unwrap_or(s),
            s.trim_start_matches('_'),
        ];
        if candidates.iter().any(|c| self.own.has_token(c)) {
            Vec::new()
        } else {
            vec![format!("{s}, in none of the image's own sources")]
        }
    }

    fn decorated_problems(&self, s: &str) -> Vec<String> {
        let full = msvc6_diff::score::demangle(Some(s));
        let readable = msvc6_match::mac::types::msvc_qualified(s).filter(|_| full != s);
        let Some(q) = readable.or_else(|| adjustor(s)) else {
            return vec![format!("{s}, which the demangler cannot read")];
        };
        let full = if full == s { q.clone() } else { full };
        let library = components(&q).first().is_some_and(|c| c == STD_SCOPE)
            || member_of(&q).is_some_and(|(class, _)| {
                !self.own.class_defined(&class) && self.library.class_defined(&class)
            });
        let rtti = full.contains("`RTTI ");
        let types = type_words(&full);
        let mut out: Vec<String> = Vec::new();
        for w in words(&full).into_iter().chain(words(&q)) {
            if SPECIAL_WORDS.contains(&w)
                || (rtti && RTTI_WORDS.contains(&w))
                || w == STD_SCOPE
                || self.known(w, &types, library)
            {
                continue;
            }
            let p = format!("{w}, in none of the image's own sources");
            if !out.contains(&p) {
                out.push(p);
            }
        }
        if !out.is_empty() {
            return out;
        }
        if let Some((class, member)) = member_of(&q) {
            if class == STD_SCOPE {
                return out;
            }
            let reg: &Sources = if library { &self.library } else { &self.own };
            if !reg.class_defined(&class) && reg.is_namespace(&class) {
                if full.contains('(') && reg.c_function(&q) {
                    out.push(format!(
                        "{q}, a function the image's sources give C linkage"
                    ));
                }
            } else if !reg.class_defined(&class) {
                out.push(format!(
                    "{class}, a class the image's sources do not define"
                ));
            } else if plain(&member) && member != class && !reg.has_member(&class, &member) {
                out.push(format!(
                    "{class}::{member}, a member {class} does not declare"
                ));
            }
        } else if full.contains('(') && self.own.c_function(&q) {
            out.push(format!(
                "{q}, a function the image's sources give C linkage"
            ));
        }
        out
    }

    #[must_use]
    pub fn import_spelling(&self, name: &str) -> Option<String> {
        let bare = name.strip_prefix("__imp_")?;
        if !bare.starts_with('?') {
            return None;
        }
        let problems = self.name_problems(bare);
        if !problems.is_empty() {
            return Some(problems.join("; "));
        }
        let full = msvc6_diff::score::demangle(Some(bare));
        let q = msvc6_match::mac::types::msvc_qualified(bare)?;
        let free = member_of(&q)
            .is_none_or(|(c, _)| !self.own.class_defined(&c) && self.own.is_namespace(&c));
        (full.contains('(') && free && !self.own.cpp_function(&q)).then(|| {
            format!(
                "a C++ spelling of {q}, which the image's sources do not declare with C++ linkage"
            )
        })
    }

    #[must_use]
    pub fn absent_quoted(&self, text: &str) -> Vec<String> {
        text.split(PRODUCT_QUOTE)
            .skip(1)
            .filter_map(|q| q.split('`').next())
            .filter(|i| plain(i) && !self.own.has_token(i))
            .map(str::to_string)
            .collect()
    }
}

impl Names {
    #[must_use]
    pub fn new(own: Sources, library: std::rc::Rc<Sources>, archives: &[Vec<u8>]) -> Names {
        let mut archived = HashSet::new();
        for a in archives {
            if let Ok(a) = msvc6_image::archive::Archive::parse(a) {
                archived.extend(a.symbol_index().into_iter().map(|(s, _)| s));
            }
        }
        Names {
            own,
            library,
            archived,
        }
    }
}

#[derive(Debug)]
pub struct View {
    pub base: u32,
    pub names: Names,
    pub globals: BTreeMap<u32, Vec<String>>,
    pub image: msvc6_image::pe::Image,
    pub imports: BTreeMap<u32, String>,
    pub import_libs: HashSet<String>,
}

impl View {
    pub fn new(
        image: msvc6_image::pe::Image,
        names: Names,
        globals_csv: Option<&str>,
    ) -> Result<View, String> {
        let base = image.opt.image_base.0;
        let mut globals: BTreeMap<u32, Vec<String>> = BTreeMap::new();
        if let Some(text) = globals_csv {
            let rows = msvc6_project::reccmp::parse_csv(Path::new("globals"), text)
                .map_err(|e| format!("{e:#}"))?;
            for r in rows {
                if let (Some(rva), Some(n)) = (r.va.checked_sub(base), r.name) {
                    globals.entry(rva).or_default().push(n);
                }
            }
        }
        let mut imports = BTreeMap::new();
        for dll in image.imports().map_err(|e| format!("{e:#}"))? {
            for e in dll.entries {
                imports.insert(e.iat_rva.0, e.name);
            }
        }
        Ok(View {
            base,
            names,
            globals,
            image,
            imports,
            import_libs: HashSet::new(),
        })
    }

    fn jmp_at(&self, rva: u32) -> Option<u32> {
        let b = self.image.at_rva(msvc6_base::units::Rva(rva), 5)?;
        let (op, rel) = b.split_first()?;
        if *op != JMP_REL32 {
            return None;
        }
        let rel = i32::from_le_bytes(rel.try_into().ok()?);
        rva.checked_add(5)?.checked_add_signed(rel)
    }

    #[must_use]
    pub fn ilt_target(&self, rva: u32) -> Option<u32> {
        let t = self.jmp_at(rva)?;
        let at = |d: i64| {
            i64::from(rva)
                .checked_add(d)
                .and_then(|r| u32::try_from(r).ok())
                .is_some_and(|r| self.jmp_at(r).is_some())
        };
        let s = ILT_STRIDE;
        ((at(s) && at(2 * s)) || (at(-s) && at(s)) || (at(-s) && at(-2 * s))).then_some(t)
    }

    #[must_use]
    pub fn with_import_libs(mut self, archives: &[Vec<u8>]) -> View {
        for a in archives {
            if let Ok(a) = msvc6_image::archive::Archive::parse(a) {
                self.import_libs
                    .extend(a.symbol_index().into_iter().map(|(s, _)| s));
            }
        }
        self
    }

    fn stub_slot(&self, rva: u32) -> Option<&String> {
        let b = self.image.at_rva(msvc6_base::units::Rva(rva), 6)?;
        let (op, abs) = b.split_at_checked(2)?;
        if op != JMP_INDIRECT {
            return None;
        }
        let abs = u32::from_le_bytes(abs.try_into().ok()?);
        self.imports.get(&abs.checked_sub(self.base)?)
    }

    #[must_use]
    pub fn import_stub(&self, rva: u32) -> Option<&String> {
        let i = self.stub_slot(rva)?;
        let near = |d: i64| {
            i64::from(rva)
                .checked_add(d)
                .and_then(|r| u32::try_from(r).ok())
                .is_some_and(|r| self.stub_slot(r).is_some())
        };
        (near(STUB_STRIDE) || near(-STUB_STRIDE)).then_some(i)
    }

    #[must_use]
    pub fn import_problem(&self, rva: u32, name: &str) -> Option<String> {
        let slot = self.imports.get(&rva);
        let Some(bare) = name.strip_prefix("__imp_") else {
            return slot.map(|i| format!("the reference's IAT slot here imports {i}"));
        };
        let Some(i) = slot else {
            return Some("no IAT slot of the reference is here".to_string());
        };
        if decorated_import(i) {
            return (bare != i).then(|| {
                format!("the reference's IAT slot here imports {i}, which it does not spell")
            });
        }
        if !same_import(bare, i) {
            return Some(format!("the reference's IAT slot here imports {i}"));
        }
        (!self.import_libs.contains(name))
            .then(|| format!("{name}, a symbol of no import library the image's build links"))
    }

    #[must_use]
    pub fn stub_problem(&self, rva: u32, name: &str) -> Option<String> {
        let i = self.import_stub(rva)?;
        if decorated_import(i) {
            if name != i {
                return Some(format!(
                    "the import stub here jumps through the IAT slot of {i}, which it does not spell"
                ));
            }
            if !name.starts_with('?') {
                return None;
            }
            let p = self.names.name_problems(name);
            if !p.is_empty() {
                return Some(p.join("; "));
            }
            return self.names.import_spelling(&format!("__imp_{name}"));
        }
        (!(same_import(name, i) && self.import_libs.contains(name))).then(|| {
            format!(
                "the import stub here jumps through the IAT slot of {i}; {name} is no stub an import library the image's build links holds"
            )
        })
    }

    #[must_use]
    pub fn reconcile(&self, addresses: Vec<Row>, symbols: Vec<Row>) -> (Verdict, Verdict) {
        let mut a = Verdict::default();
        let mut ilt_a = Vec::new();
        for r in addresses {
            if self.ilt_target(r.rva).is_some() {
                ilt_a.push(r);
            } else {
                self.judge(r, None, &mut a);
            }
        }
        let mut s = Verdict::default();
        let mut ilt_s = Vec::new();
        let named_a = names_at(&a.kept);
        for r in symbols {
            if self.ilt_target(r.rva).is_some() {
                ilt_s.push(r);
            } else {
                self.judge(r, Some(&named_a), &mut s);
            }
        }
        let mut bodies = names_at(&a.kept);
        for r in &s.kept {
            bodies.entry(r.rva).or_default().push(r.name.clone());
        }
        for r in ilt_a {
            self.thunk(r, &bodies, &mut a);
        }
        for r in ilt_s {
            self.thunk(r, &bodies, &mut s);
        }
        (a, s)
    }

    fn judge(&self, r: Row, addresses: Option<&BTreeMap<u32, Vec<String>>>, v: &mut Verdict) {
        let why = if r.name.starts_with("__imp_") || self.imports.contains_key(&r.rva) {
            self.import_problem(r.rva, &r.name)
                .or_else(|| self.names.import_spelling(&r.name))
        } else if self.import_stub(r.rva).is_some() {
            self.stub_problem(r.rva, &r.name)
        } else if let Some(g) = self.globals.get(&r.rva).filter(|g| !g.contains(&r.name)) {
            Some(format!("the product's globals name it {}", g.join(", ")))
        } else if let Some(a) = addresses
            .and_then(|m| m.get(&r.rva))
            .filter(|a| !a.contains(&r.name))
        {
            Some(format!("the addresses table names it {}", a.join(", ")))
        } else {
            let p = self.names.name_problems(&r.name);
            (!p.is_empty()).then(|| p.join("; "))
        };
        match why {
            Some(w) => v.left_out.push((r, w)),
            None => v.kept.push(r),
        }
    }

    fn thunk(&self, r: Row, bodies: &BTreeMap<u32, Vec<String>>, v: &mut Verdict) {
        let Some(t) = self.ilt_target(r.rva) else {
            v.kept.push(r);
            return;
        };
        let mut targets: Vec<String> = bodies.get(&t).cloned().unwrap_or_default();
        if targets.is_empty()
            && let Some(i) = self.import_stub(t).filter(|i| i.starts_with(['_', '?']))
        {
            targets.push(i.clone());
        }
        if targets.contains(&r.name) {
            v.kept.push(r);
            return;
        }
        match targets.first() {
            Some(n) => {
                let mut renamed = r.clone();
                renamed.name.clone_from(n);
                v.renamed.push((r, n.clone()));
                v.kept.push(renamed);
            }
            None => v.left_out.push((
                r,
                format!("an ILT thunk to 0x{t:x}, which the tables do not name"),
            )),
        }
    }
}

#[must_use]
pub fn names_at(rows: &[Row]) -> BTreeMap<u32, Vec<String>> {
    let mut out: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    for r in rows {
        out.entry(r.rva).or_default().push(r.name.clone());
    }
    out
}

#[must_use]
pub fn rows_of(tsv: &str) -> Vec<Row> {
    tsv.lines()
        .skip(1)
        .filter_map(|l| {
            let (rva, rest) = l.split_once('\t')?;
            let (name, rest) = rest.split_once('\t').unwrap_or((rest, ""));
            let rva = u32::from_str_radix(rva.strip_prefix("0x")?, 16).ok()?;
            Some(Row {
                rva,
                name: name.to_string(),
                rest: rest.to_string(),
            })
        })
        .collect()
}

#[must_use]
pub fn note_names(note: &str) -> Vec<(String, u32)> {
    let mut out = Vec::new();
    let mut rest = note;
    while let Some(at) = rest.find("@0x") {
        let before = rest.get(..at).unwrap_or_default();
        let after = rest.get(at + 3..).unwrap_or_default();
        let digits = after
            .find(|c: char| !c.is_ascii_hexdigit())
            .unwrap_or(after.len());
        let from = [" in ", "(after "]
            .iter()
            .filter_map(|m| before.rfind(m).map(|k| k + m.len()))
            .max()
            .unwrap_or(0);
        if let Ok(va) = u32::from_str_radix(after.get(..digits).unwrap_or_default(), 16) {
            out.push((
                before.get(from..).unwrap_or_default().trim().to_string(),
                va,
            ));
        }
        rest = after.get(digits..).unwrap_or_default();
    }
    out
}

#[must_use]
pub fn named_at(addresses: &[Row], symbols: &[Row], rva: u32) -> Option<String> {
    addresses
        .iter()
        .chain(symbols)
        .find(|r| r.rva == rva)
        .map(|r| r.name.clone())
}

fn under(root: &Path, p: &Path) -> Option<String> {
    p.strip_prefix(root)
        .ok()
        .map(|r| r.to_string_lossy().replace('\\', "/"))
}

fn project_dirs(
    project: &msvc6_project::config::Project,
    decl: &msvc6_project::config::ImageDecl,
    root: &Path,
) -> Result<Vec<String>, String> {
    let mut dirs = Vec::new();
    if let Some(dsw) = decl.project.as_ref().and_then(|p| p.dsw.as_ref()) {
        let path = project.path(dsw).map_err(|e| format!("{e:#}"))?;
        let w = msvc6_project::dsw::Dsw::read(&path).map_err(|e| format!("{e:#}"))?;
        for p in &w.projects {
            if let Some(d) = w.dsp_path(p).parent().and_then(|d| under(root, d)) {
                dirs.push(d);
            }
        }
    }
    Ok(dirs)
}

fn windows_key(p: &str) -> String {
    let mut out = String::new();
    for c in p.chars() {
        let c = if c == '/' { '\\' } else { c };
        if !(c == '\\' && out.ends_with('\\')) {
            out.push(c.to_ascii_lowercase());
        }
    }
    out.trim_end_matches('\\').to_string()
}

fn resolve_folded(base: &Path, tail: &[&str]) -> PathBuf {
    let mut at = base.to_path_buf();
    for c in tail {
        let exact = at.join(c);
        if exact.exists() {
            at = exact;
            continue;
        }
        let folded = std::fs::read_dir(&at).ok().and_then(|d| {
            d.flatten()
                .map(|e| e.path())
                .find(|p| p.file_name().is_some_and(|n| n.eq_ignore_ascii_case(c)))
        });
        at = folded.unwrap_or(exact);
    }
    at
}

fn mounted(
    project: &msvc6_project::config::Project,
    build: &msvc6_toolchain::config::BuildSpec,
    windows: &str,
) -> Option<PathBuf> {
    let key = windows_key(windows);
    build.mount.iter().find_map(|m| {
        let at = windows_key(&m.at);
        let rest = key.strip_prefix(&at)?;
        if !(rest.is_empty() || rest.starts_with('\\')) {
            return None;
        }
        let from = project.path(&m.from).ok()?;
        let original = windows.replace('\\', "/");
        let tail: Vec<&str> = original.split('/').filter(|s| !s.is_empty()).collect();
        let depth = rest.split('\\').filter(|s| !s.is_empty()).count();
        let tail = tail.get(tail.len().saturating_sub(depth)..)?;
        Some(resolve_folded(&from, tail))
    })
}

fn include_dirs(
    project: &msvc6_project::config::Project,
    decl: &msvc6_project::config::ImageDecl,
    root: &Path,
) -> Vec<String> {
    let Some(build) = &decl.build else {
        return Vec::new();
    };
    build
        .include
        .iter()
        .filter_map(|inc| mounted(project, build, inc))
        .filter_map(|p| under(root, &p))
        .collect()
}

pub fn import_libraries(
    project: &msvc6_project::config::Project,
    decl: &msvc6_project::config::ImageDecl,
) -> Result<Vec<Vec<u8>>, String> {
    let Some(build) = &decl.build else {
        return Ok(Vec::new());
    };
    let mut out = Vec::new();
    for dir in build.lib.iter().filter_map(|l| mounted(project, build, l)) {
        let Ok(entries) = std::fs::read_dir(&dir) else {
            continue;
        };
        let mut libs: Vec<PathBuf> = entries
            .flatten()
            .map(|e| e.path())
            .filter(|p| p.extension().is_some_and(|e| e.eq_ignore_ascii_case("lib")) && p.is_file())
            .collect();
        libs.sort();
        for l in libs {
            out.push(std::fs::read(&l).map_err(|e| format!("{}: {e}", l.display()))?);
        }
    }
    Ok(out)
}

fn within(d: &str, p: &str) -> bool {
    d == p || d.strip_prefix(p).is_some_and(|r| r.starts_with('/'))
}

pub fn own_dirs(
    project: &msvc6_project::config::Project,
    decl: &msvc6_project::config::ImageDecl,
    root: &Path,
) -> Result<(Vec<String>, Vec<String>), String> {
    let mut code = project_dirs(project, decl, root)?;
    code.sort();
    code.dedup();
    let mut every = Vec::new();
    for d in &project.images {
        every.extend(project_dirs(project, d, root)?);
    }
    let mut headers: Vec<String> = include_dirs(project, decl, root)
        .into_iter()
        .filter(|d| every.iter().any(|p| within(d, p)))
        .collect();
    if let Some(c) = &decl.classes {
        for h in &c.headers {
            let p = project.path(h).map_err(|e| format!("{e:#}"))?;
            headers.extend(under(root, &p));
        }
    }
    headers.retain(|d| !code.iter().any(|p| within(d, p)));
    headers.sort();
    headers.dedup();
    Ok((code, headers))
}

pub fn image_view(
    project: &msvc6_project::config::Project,
    id: &str,
    root: &Path,
    libraries: &mut BTreeMap<std::path::PathBuf, std::rc::Rc<Sources>>,
) -> Result<View, String> {
    let (_, decl) = project
        .image(id)
        .ok_or_else(|| format!("the config has no image {id}"))?;
    let (code, headers) = own_dirs(project, decl, root)?;
    let mut own = Sources::read(root, &code, false)?;
    own.add(root, &headers, &HEADER_EXTENSIONS, false)?;
    let include = match decl
        .compiler
        .as_ref()
        .and_then(|c| project.toolchains.get(c))
    {
        Some(t) => Some(match &t.include {
            Some(i) => project.path(i).map_err(|e| format!("{e:#}"))?,
            None => project
                .path(&t.root)
                .map_err(|e| format!("{e:#}"))?
                .join("VC98")
                .join("Include"),
        }),
        None => None,
    };
    let library = match include {
        Some(inc) => {
            if !libraries.contains_key(&inc) {
                let read = Sources::read(&inc, &[String::new()], true)?;
                libraries.insert(inc.clone(), std::rc::Rc::new(read));
            }
            libraries.get(&inc).cloned().unwrap_or_default()
        }
        None => std::rc::Rc::default(),
    };
    let mut archives = Vec::new();
    for l in &decl.libraries {
        let p = project.path(l).map_err(|e| format!("{e:#}"))?;
        archives.push(std::fs::read(&p).map_err(|e| format!("{}: {e}", p.display()))?);
    }
    let mut globals = None;
    for n in &decl.names {
        if let msvc6_project::config::NameSource::ReccmpCsv(f) = n {
            let p = project.path(f).map_err(|e| format!("{e:#}"))?;
            if p.starts_with(root) {
                globals =
                    Some(msvc6_base::srcio::read(&p).map_err(|e| format!("{}: {e}", p.display()))?);
            }
        }
    }
    let reference = project
        .path(&decl.reference)
        .map_err(|e| format!("{e:#}"))?;
    let image = msvc6_image::pe::Image::load(&reference)
        .map_err(|e| format!("{}: {e:#}", reference.display()))?;
    let libs = import_libraries(project, decl)?;
    Ok(View::new(
        image,
        Names::new(own, library, &archives),
        globals.as_deref(),
    )?
    .with_import_libs(&libs))
}

#[must_use]
pub fn same_import(name: &str, import: &str) -> bool {
    let bare = |s: &str| {
        if s.starts_with('?') {
            readable(s)
        } else {
            readable(&format!("_{}", s.trim_start_matches('_')))
        }
    };
    name == import || bare(name) == bare(import)
}

#[must_use]
pub fn decorated_import(import: &str) -> bool {
    import.starts_with(['?', '_', '@']) || import.contains('@')
}
