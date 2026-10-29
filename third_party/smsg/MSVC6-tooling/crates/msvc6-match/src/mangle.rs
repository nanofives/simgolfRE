use serde::Serialize;

pub const MAX_DEPTH: usize = 64;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize)]
pub enum Kind {
    Int,
    Ptr,
    Float,
    Unknown,
}

#[derive(Debug, Clone, PartialEq, Eq, Default, Serialize)]
pub struct Sig {
    pub scope: Vec<String>,
    pub name: String,
    pub params: Vec<String>,
    pub params_known: bool,
    pub varargs: bool,
    pub konst: bool,
    pub is_static: Option<bool>,
}

impl Sig {
    #[must_use]
    pub fn named(name: &str) -> Self {
        Self {
            name: name.to_string(),
            ..Self::default()
        }
    }

    #[must_use]
    pub fn class(&self) -> Option<&str> {
        self.scope.last().map(String::as_str)
    }

    #[must_use]
    pub fn key(&self) -> String {
        let mut s = self.qual_name();
        if self.params_known {
            s.push('(');
            s.push_str(&self.params.join(","));
            if self.varargs {
                if !self.params.is_empty() {
                    s.push(',');
                }
                s.push_str("...");
            }
            s.push(')');
            if self.konst {
                s.push_str(" const");
            }
        }
        s
    }

    #[must_use]
    pub fn qual_name(&self) -> String {
        let mut s = String::new();
        for c in &self.scope {
            s.push_str(c);
            s.push_str("::");
        }
        s.push_str(&self.display_name());
        s
    }

    #[must_use]
    pub fn display_name(&self) -> String {
        let bare = |c: &str| c.split('<').next().unwrap_or(c).to_string();
        match self.name.as_str() {
            "{ctor}" => self.class().map_or_else(|| self.name.clone(), bare),
            "{dtor}" => format!("~{}", self.class().map_or_else(String::new, bare)),
            _ => self.name.clone(),
        }
    }

    #[must_use]
    pub fn has_this(&self) -> Option<bool> {
        if self.scope.is_empty() {
            return Some(false);
        }
        match self.name.as_str() {
            "{ctor}" | "{dtor}" => Some(true),
            "operator new" | "operator delete" | "operator new[]" | "operator delete[]" => {
                Some(false)
            }
            _ => self.is_static.map(|s| !s),
        }
    }

    #[must_use]
    pub fn kinds(&self) -> Vec<Kind> {
        self.params.iter().map(|p| kind_of(p)).collect()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default, Serialize)]
pub enum SigLevel {
    Mismatch,
    #[default]
    Unknown,
    Count,
    Kinds,
    Exact,
}

impl SigLevel {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::Mismatch => "mismatch",
            Self::Unknown => "unknown",
            Self::Count => "count",
            Self::Kinds => "kinds",
            Self::Exact => "exact",
        }
    }
}

#[must_use]
pub fn compare(theirs: &Sig, ours: &Sig, this_ours: Option<bool>) -> SigLevel {
    if !theirs.params_known || !ours.params_known {
        return SigLevel::Unknown;
    }
    let ot = this_ours.or_else(|| ours.has_this());
    if let (Some(a), Some(b)) = (theirs.has_this(), ot)
        && a != b
    {
        return SigLevel::Mismatch;
    }
    let (n, m) = (theirs.params.len(), ours.params.len());
    let count_ok = if theirs.varargs || ours.varargs {
        theirs.varargs == ours.varargs && n == m
    } else {
        n == m
    };
    if !count_ok {
        return SigLevel::Mismatch;
    }
    let kinds_ok = theirs
        .kinds()
        .iter()
        .zip(ours.kinds().iter())
        .all(|(a, b)| a == b || *a == Kind::Unknown || *b == Kind::Unknown);
    if !kinds_ok {
        return SigLevel::Count;
    }
    if theirs.params.iter().zip(&ours.params).all(|(a, b)| a == b) {
        SigLevel::Exact
    } else {
        SigLevel::Kinds
    }
}

const INT_TYPEDEFS: &[&str] = &[
    "DWORD", "WORD", "BYTE", "BOOL", "UINT", "ULONG", "LONG", "SHORT", "USHORT", "INT", "WPARAM",
    "LPARAM", "LRESULT", "COLORREF", "size_t", "time_t", "UCHAR", "CHAR", "MMRESULT", "HRESULT",
    "ATOM", "BOOLEAN", "uint", "ushort", "uchar", "ulong",
];

#[must_use]
pub fn kind_of(t: &str) -> Kind {
    if t.contains('*') || t.contains('&') || t.contains('[') || t == "fnptr" {
        return Kind::Ptr;
    }
    let base = t.trim();
    match base {
        "float" | "double" | "long double" => return Kind::Float,
        "int" | "unsigned" | "char" | "unsigned char" | "signed char" | "short"
        | "unsigned short" | "long" | "unsigned long" | "bool" | "wchar_t" | "long long"
        | "unsigned long long" | "__int64" | "unsigned __int64" => return Kind::Int,
        _ => {}
    }
    if INT_TYPEDEFS.contains(&base) {
        return Kind::Int;
    }
    let caps = |extra: char| {
        base.chars()
            .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == extra)
    };
    if base.len() > 1 && base.starts_with('H') && caps('H') {
        return Kind::Ptr;
    }
    if (base.starts_with("LP") || base.starts_with("PC") || base.starts_with('P'))
        && base.len() > 2
        && caps('_')
    {
        return Kind::Ptr;
    }
    Kind::Unknown
}

#[must_use]
pub fn norm_type(t: &str) -> String {
    let t = t.trim();
    if t.contains("(*") || t.contains("( *") || t.contains("(__") {
        return "fnptr".to_string();
    }
    let mut toks: Vec<String> = Vec::new();
    let mut cur = String::new();
    let flush = |cur: &mut String, toks: &mut Vec<String>| {
        if !cur.is_empty() {
            toks.push(std::mem::take(cur));
        }
    };
    for c in t.chars() {
        match c {
            '*' | '&' | '[' | ']' => {
                flush(&mut cur, &mut toks);
                toks.push(c.to_string());
            }
            c if c.is_whitespace() => flush(&mut cur, &mut toks),
            c => cur.push(c),
        }
    }
    flush(&mut cur, &mut toks);
    toks.retain(|w| {
        !matches!(
            w.as_str(),
            "struct" | "class" | "enum" | "union" | "const" | "volatile" | "register"
        )
    });
    let mut words: Vec<&str> = Vec::new();
    let mut tail = String::new();
    for w in &toks {
        match w.as_str() {
            "*" | "&" | "[" | "]" => tail.push_str(w),
            _ if tail.is_empty() => words.push(w),
            _ => tail.push_str(w),
        }
    }
    match words.as_slice() {
        ["unsigned", "int"] => words = vec!["unsigned"],
        ["long" | "short", "int"] => words.truncate(1),
        ["unsigned", _, "int"] => words.truncate(2),
        ["signed"] => words = vec!["int"],
        _ => {}
    }
    let mut out = words.join(" ");
    out.push_str(&tail.replace("[]", "*"));
    out
}

const TYPE_WORDS: &[&str] = &[
    "int", "char", "short", "long", "unsigned", "signed", "float", "double", "bool", "void",
    "const", "volatile", "struct", "class", "enum", "union", "wchar_t", "__int64",
];

fn param_type(p: &str) -> String {
    let p = p.split('=').next().unwrap_or(p).trim();
    if p.contains("(*") || p.contains("( *") {
        return "fnptr".to_string();
    }
    let chars: Vec<char> = p.chars().collect();
    let mut end = chars.len();
    while end.checked_sub(1).and_then(|e| chars.get(e)) == Some(&']') {
        match chars
            .get(..end)
            .and_then(|b| b.iter().rposition(|c| *c == '['))
        {
            Some(o) => end = o,
            None => break,
        }
        while end
            .checked_sub(1)
            .and_then(|e| chars.get(e))
            .is_some_and(|c| *c == ' ')
        {
            end -= 1;
        }
    }
    let array = end < chars.len();
    let head: String = chars
        .get(..end)
        .map(|s| s.iter().collect())
        .unwrap_or_default();
    let mut start = head.len();
    for (i, c) in head.char_indices().rev() {
        if c.is_ascii_alphanumeric() || c == '_' {
            start = i;
        } else {
            break;
        }
    }
    let last = head.get(start..).unwrap_or("");
    let before = head.get(..start).unwrap_or("").trim_end();
    let typed = !last.is_empty()
        && !before.is_empty()
        && !TYPE_WORDS.contains(&last)
        && !before.ends_with("::")
        && before.chars().last().is_some_and(|c| {
            c == '*' || c == '&' || c.is_ascii_alphanumeric() || c == '_' || c == '>'
        });
    let mut t = norm_type(if typed { before } else { &head });
    if array {
        t.push('*');
    }
    t
}

#[must_use]
pub fn split_top(s: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut cur = String::new();
    for c in s.chars() {
        match c {
            '<' | '(' | '[' => {
                depth = depth.saturating_add(1);
                cur.push(c);
            }
            '>' | ')' | ']' => {
                depth = depth.saturating_sub(1);
                cur.push(c);
            }
            ',' if depth == 0 => out.push(std::mem::take(&mut cur)),
            _ => cur.push(c),
        }
    }
    if !cur.trim().is_empty() {
        out.push(cur);
    }
    out
}

#[must_use]
pub fn split_scope(s: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut cur = String::new();
    let cs: Vec<char> = s.chars().collect();
    let mut i = 0;
    while let Some(&c) = cs.get(i) {
        match c {
            '<' => depth = depth.saturating_add(1),
            '>' => depth = depth.saturating_sub(1),
            _ => {}
        }
        if c == ':' && depth == 0 && cs.get(i.saturating_add(1)) == Some(&':') {
            out.push(std::mem::take(&mut cur));
            i = i.saturating_add(2);
            continue;
        }
        cur.push(c);
        i = i.saturating_add(1);
    }
    out.push(cur);
    out.into_iter().map(|x| x.trim().to_string()).collect()
}

fn last_word(s: &str) -> String {
    s.rsplit([' ', '*', '&']).next().unwrap_or(s).to_string()
}

fn data_symbol(s: &str) -> Option<Sig> {
    let parts = split_scope(s);
    let name = last_word(parts.last()?);
    let mut scope: Vec<String> = parts
        .get(..parts.len().saturating_sub(1))
        .unwrap_or_default()
        .to_vec();
    if let Some(first) = scope.first_mut() {
        *first = last_word(first);
    }
    Some(Sig {
        scope,
        name,
        params_known: false,
        ..Sig::default()
    })
}

fn strip_qualifiers(s: &str) -> (String, bool) {
    let mut s = s.to_string();
    let mut konst = false;
    loop {
        let trimmed = s.trim_end().to_string();
        if let Some(x) = trimmed.strip_suffix("const") {
            konst = true;
            s = x.to_string();
        } else if let Some(x) = trimmed.strip_suffix("= 0") {
            s = x.to_string();
        } else if let Some(x) = trimmed.strip_suffix("throw()") {
            s = x.to_string();
        } else {
            return (trimmed, konst);
        }
    }
}

fn matching_open(cs: &[char], close: usize) -> Option<usize> {
    let mut depth = 0i32;
    for i in (0..=close).rev() {
        match cs.get(i) {
            Some(')') => depth = depth.saturating_add(1),
            Some('(') => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    return Some(i);
                }
            }
            _ => {}
        }
    }
    None
}

#[must_use]
pub fn parse_text(text: &str, class_hint: Option<&str>) -> Option<Sig> {
    let mut t = text.trim().to_string();
    if let Some(p) = top_level_brace(&t) {
        t.truncate(p);
    }
    let t = t.trim().trim_end_matches(';').trim();
    let (mut s, konst) = strip_qualifiers(t);
    if let Some(p) = init_list_colon(&s) {
        s.truncate(p);
        s = s.trim_end().to_string();
    }
    let Some(close) = s.rfind(')').filter(|p| p.saturating_add(1) == s.len()) else {
        return data_symbol(&s);
    };
    let cs: Vec<char> = s.chars().collect();
    let close_ci = s.get(..close).map_or(0, |x| x.chars().count());
    let open = matching_open(&cs, close_ci)?;
    let head: String = cs.get(..open)?.iter().collect();
    let inner: String = cs.get(open.saturating_add(1)..close_ci)?.iter().collect();
    let head = head.trim_end();
    let qual = qualified_tail(head)?;
    let mut parts = split_scope(&qual);
    let raw_name = parts.pop()?;
    if parts.is_empty()
        && let Some(c) = class_hint
    {
        parts = split_scope(c);
    }
    let class_bare = parts
        .last()
        .map(|c| c.split('<').next().unwrap_or(c).trim().to_string());
    let name = if raw_name.starts_with('~') {
        "{dtor}".to_string()
    } else if class_bare.as_deref() == Some(raw_name.split('<').next().unwrap_or(&raw_name).trim())
    {
        "{ctor}".to_string()
    } else {
        raw_name.split_whitespace().collect::<Vec<_>>().join(" ")
    };
    let mut params = Vec::new();
    let mut varargs = false;
    let inner = inner.trim();
    if !(inner.is_empty() || inner == "void") {
        for p in split_top(inner) {
            let p = p.trim();
            if p == "..." {
                varargs = true;
            } else {
                params.push(param_type(p));
            }
        }
    }
    let is_static =
        (head.split_whitespace().any(|w| w == "static") && !parts.is_empty()).then_some(true);
    Some(Sig {
        scope: parts,
        name: canon_operator(&name),
        params,
        params_known: true,
        varargs,
        konst,
        is_static,
    })
}

fn canon_operator(n: &str) -> String {
    if let Some(rest) = n.strip_prefix("operator") {
        let r = rest.trim();
        if r.chars().next().is_some_and(|c| c.is_ascii_alphabetic()) {
            return format!(
                "operator {}",
                r.split_whitespace().collect::<Vec<_>>().join(" ")
            );
        }
        return format!("operator{}", r.replace(' ', ""));
    }
    n.to_string()
}

fn top_level_brace(s: &str) -> Option<usize> {
    let mut depth = 0i32;
    for (i, c) in s.char_indices() {
        match c {
            '(' => depth = depth.saturating_add(1),
            ')' => depth = depth.saturating_sub(1),
            '{' if depth == 0 => return Some(i),
            _ => {}
        }
    }
    None
}

fn init_list_colon(s: &str) -> Option<usize> {
    let mut depth = 0i32;
    let mut seen_close = false;
    let b: Vec<(usize, char)> = s.char_indices().collect();
    for (k, (i, c)) in b.iter().enumerate() {
        match c {
            '(' => depth = depth.saturating_add(1),
            ')' => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    seen_close = true;
                }
            }
            ':' if depth == 0 && seen_close => {
                let prev = k.checked_sub(1).and_then(|j| b.get(j)).map(|x| x.1);
                let next = b.get(k.saturating_add(1)).map(|x| x.1);
                if prev != Some(':') && next != Some(':') {
                    return Some(*i);
                }
            }
            _ => {}
        }
    }
    None
}

fn qualified_tail(head: &str) -> Option<String> {
    let h = head.trim_end();
    if let Some(p) = h.rfind("operator") {
        let after = h.get(p.saturating_add(8)..).unwrap_or("");
        let boundary = p == 0
            || h.get(..p)
                .and_then(|x| x.chars().last())
                .is_some_and(|c| !c.is_ascii_alphanumeric() && c != '_');
        if boundary
            && (after.is_empty()
                || !after.starts_with(|c: char| c.is_ascii_alphanumeric() || c == '_')
                || after.starts_with(' '))
        {
            let start = scope_prefix_start(h.get(..p).unwrap_or("")).unwrap_or(p);
            return h.get(start..).map(|x| x.trim().to_string());
        }
    }
    let start = scope_prefix_start(h).unwrap_or(0);
    let q = h.get(start..)?.trim();
    (!q.is_empty()).then(|| q.to_string())
}

fn scope_prefix_start(s: &str) -> Option<usize> {
    let cs: Vec<(usize, char)> = s.char_indices().collect();
    let mut i = cs.len();
    let mut depth = 0i32;
    let mut start = None;
    while i > 0 {
        let Some(&(pos, c)) = cs.get(i - 1) else {
            break;
        };
        if depth > 0 {
            match c {
                '>' => depth = depth.saturating_add(1),
                '<' => depth = depth.saturating_sub(1),
                _ => {}
            }
        } else if c == '>' {
            depth = 1;
        } else if !(c.is_ascii_alphanumeric() || c == '_' || c == ':' || c == '~') {
            break;
        }
        start = Some(pos);
        i -= 1;
    }
    start
}

fn cw_operator(code: &str) -> Option<&'static str> {
    Some(match code {
        "ct" => "{ctor}",
        "dt" => "{dtor}",
        "nw" => "operator new",
        "dl" => "operator delete",
        "nwa" => "operator new[]",
        "dla" => "operator delete[]",
        "as" => "operator=",
        "eq" => "operator==",
        "ne" => "operator!=",
        "lt" => "operator<",
        "gt" => "operator>",
        "le" => "operator<=",
        "ge" => "operator>=",
        "pl" => "operator+",
        "mi" => "operator-",
        "ml" => "operator*",
        "dv" => "operator/",
        "md" => "operator%",
        "vc" => "operator[]",
        "cl" => "operator()",
        "rf" => "operator->",
        "pp" => "operator++",
        "mm" => "operator--",
        "apl" => "operator+=",
        "ami" => "operator-=",
        "amu" => "operator*=",
        "adv" => "operator/=",
        "aad" => "operator&=",
        "aor" => "operator|=",
        "aer" => "operator^=",
        "als" => "operator<<=",
        "ars" => "operator>>=",
        "ls" => "operator<<",
        "rs" => "operator>>",
        "aa" => "operator&&",
        "oo" => "operator||",
        "nt" => "operator!",
        "co" => "operator~",
        "ad" => "operator&",
        "or" => "operator|",
        "er" => "operator^",
        _ => return None,
    })
}

struct Cur<'a> {
    b: &'a [u8],
    i: usize,
}

impl Cur<'_> {
    fn peek(&self) -> Option<u8> {
        self.b.get(self.i).copied()
    }

    fn bump(&mut self) -> Option<u8> {
        let c = self.peek()?;
        self.i = self.i.saturating_add(1);
        Some(c)
    }

    fn num(&mut self) -> Option<usize> {
        let s = self.i;
        let mut v: usize = 0;
        while let Some(c) = self.peek().filter(u8::is_ascii_digit) {
            v = v.checked_mul(10)?.checked_add(usize::from(c - b'0'))?;
            self.i = self.i.saturating_add(1);
        }
        (self.i > s).then_some(v)
    }

    fn take(&mut self, n: usize) -> Option<String> {
        let s = self.b.get(self.i..self.i.checked_add(n)?)?;
        self.i = self.i.saturating_add(n);
        Some(msvc6_base::srcio::decode(s))
    }

    fn at_end(&self) -> bool {
        self.i >= self.b.len()
    }
}

fn cw_class(c: &mut Cur<'_>, depth: usize) -> Option<Vec<String>> {
    match c.peek()? {
        b'Q' => {
            c.bump();
            let n = if c.peek() == Some(b'_') {
                c.bump();
                let n = c.num()?;
                if c.bump()? != b'_' {
                    return None;
                }
                n
            } else {
                usize::from(c.bump()?.checked_sub(b'0')?)
            };
            let mut out = Vec::new();
            for _ in 0..n {
                let len = c.num()?;
                out.push(cw_template_text(&c.take(len)?, depth)?);
            }
            Some(out)
        }
        b'0'..=b'9' => {
            let len = c.num()?;
            Some(vec![cw_template_text(&c.take(len)?, depth)?])
        }
        _ => None,
    }
}

fn cw_template_text(s: &str, depth: usize) -> Option<String> {
    let Some(open) = s.find('<') else {
        return Some(s.to_string());
    };
    let Some(inner) = s.get(open.saturating_add(1)..s.len().saturating_sub(1)) else {
        return Some(s.to_string());
    };
    let mut c = Cur {
        b: inner.as_bytes(),
        i: 0,
    };
    let mut args = Vec::new();
    while !c.at_end() {
        if c.peek() == Some(b',') {
            c.bump();
            continue;
        }
        match cw_type(&mut c, &[], depth.checked_add(1)?) {
            Some(t) => args.push(t),
            None => return Some(s.to_string()),
        }
    }
    Some(format!(
        "{}<{}>",
        s.get(..open).unwrap_or(""),
        args.join(",")
    ))
}

fn cw_type(c: &mut Cur<'_>, prev: &[String], depth: usize) -> Option<String> {
    if depth > MAX_DEPTH {
        return None;
    }
    let mut suffix = String::new();
    let (mut unsigned, mut signed) = (false, false);
    loop {
        match c.peek()? {
            b'C' | b'V' => {
                c.bump();
            }
            b'U' => {
                c.bump();
                unsigned = true;
            }
            b'S' => {
                c.bump();
                signed = true;
            }
            b'P' => {
                c.bump();
                suffix.insert(0, '*');
            }
            b'R' => {
                c.bump();
                suffix.insert(0, '&');
            }
            b'A' => {
                c.bump();
                c.num()?;
                if c.bump()? != b'_' {
                    return None;
                }
                suffix.insert(0, '*');
            }
            b'M' => {
                c.bump();
                cw_class(c, depth.saturating_add(1))?;
                cw_type(c, prev, depth.saturating_add(1))?;
                return Some("fnptr".to_string());
            }
            b'F' => {
                c.bump();
                while c.peek().is_some_and(|x| x != b'_') {
                    cw_type(c, prev, depth.saturating_add(1))?;
                }
                c.bump();
                cw_type(c, prev, depth.saturating_add(1))?;
                return Some("fnptr".to_string());
            }
            b'T' => {
                c.bump();
                let n = c.num()?;
                return prev.get(n.checked_sub(1)?).cloned();
            }
            _ => break,
        }
    }
    let base = match c.peek()? {
        b'v' => "void",
        b'c' if unsigned => "unsigned char",
        b'c' if signed => "signed char",
        b'c' => "char",
        b's' if unsigned => "unsigned short",
        b's' => "short",
        b'i' if unsigned => "unsigned",
        b'i' => "int",
        b'l' if unsigned => "unsigned long",
        b'l' => "long",
        b'x' if unsigned => "unsigned long long",
        b'x' => "long long",
        b'f' => "float",
        b'd' => "double",
        b'r' => "long double",
        b'b' => "bool",
        b'w' => "wchar_t",
        b'e' => "...",
        b'0'..=b'9' | b'Q' => {
            let parts = cw_class(c, depth.saturating_add(1))?;
            return Some(format!("{}{suffix}", parts.join("::")));
        }
        _ => return None,
    };
    c.bump();
    Some(format!("{base}{suffix}"))
}

fn cw_split(m: &str) -> Option<(String, &str)> {
    if let Some(after) = m.strip_prefix("__") {
        let end = after.find("__")?;
        let code = after.get(..end)?;
        let name = cw_operator(code).map_or_else(|| format!("__{code}"), str::to_string);
        return Some((name, after.get(end.saturating_add(2)..)?));
    }
    let b = m.as_bytes();
    let mut i = 1usize;
    while i.saturating_add(2usize) < b.len() {
        if b.get(i) == Some(&b'_') && b.get(i.saturating_add(1)) == Some(&b'_') {
            let n = b.get(i.saturating_add(2)).copied();
            if n.is_some_and(|x| x.is_ascii_digit() || x == b'Q' || x == b'F') {
                return Some((m.get(..i)?.to_string(), m.get(i.saturating_add(2)..)?));
            }
        }
        i = i.saturating_add(1);
    }
    None
}

#[must_use]
pub fn cw(mangled: &str) -> Option<Sig> {
    let m = mangled.strip_prefix('.').unwrap_or(mangled);
    if m.is_empty() {
        return None;
    }
    let Some((name, rest)) = cw_split(m) else {
        if m.starts_with("__") {
            return None;
        }
        return Some(Sig::named(m));
    };
    let mut c = Cur {
        b: rest.as_bytes(),
        i: 0,
    };
    let scope = if c.peek().is_some_and(|x| x.is_ascii_digit() || x == b'Q') {
        cw_class(&mut c, 0)?
    } else {
        Vec::new()
    };
    let mut konst = false;
    let mut is_static = None;
    loop {
        match c.peek() {
            Some(b'C') => {
                konst = true;
                c.bump();
            }
            Some(b'V') => {
                c.bump();
            }
            Some(b'S') => {
                is_static = Some(true);
                c.bump();
            }
            _ => break,
        }
    }
    if c.at_end() {
        return Some(Sig {
            scope,
            name,
            ..Sig::default()
        });
    }
    if c.bump()? != b'F' {
        return None;
    }
    if !scope.is_empty() && is_static.is_none() {
        is_static = Some(false);
    }
    let mut params: Vec<String> = Vec::new();
    let mut varargs = false;
    while !c.at_end() {
        if c.peek() == Some(b'N') {
            c.bump();
            let n = usize::from(c.bump()?.checked_sub(b'0')?);
            let k = usize::from(c.bump()?.checked_sub(b'0')?);
            let t = params.get(k.checked_sub(1)?).cloned()?;
            for _ in 0..n {
                params.push(t.clone());
            }
            continue;
        }
        let t = cw_type(&mut c, &params, 0)?;
        if t == "..." {
            varargs = true;
        } else if t != "void" || !params.is_empty() || !c.at_end() {
            params.push(norm_type(&t));
        }
    }
    Some(Sig {
        scope,
        name,
        params,
        params_known: true,
        varargs,
        konst,
        is_static,
    })
}

#[must_use]
pub fn itanium(mangled: &str) -> Option<Sig> {
    if !mangled.starts_with("_Z") {
        return Some(Sig::named(mangled));
    }
    let sym = cpp_demangle::Symbol::new(mangled).ok()?;
    let text = sym.demangle().ok()?;
    let mut s = parse_text(&text, None)?;
    s.is_static = None;
    Some(s)
}

#[must_use]
pub fn itanium_text(mangled: &str) -> Option<String> {
    let sym = cpp_demangle::Symbol::new(mangled).ok()?;
    sym.demangle().ok()
}
