use anyhow::{Context as _, Result, bail};
use msvc6_codeview::codec::type_record;
use std::collections::{BTreeMap, BTreeSet, HashMap};

use crate::write::st;

pub const TI_MIN: u32 = 0x1000;
pub const MAX_DEPTH: u32 = 64;

const LF_POINTER: u16 = 0x1002;
const LF_ARRAY_ST: u16 = 0x1003;
const LF_STRUCTURE_ST: u16 = 0x1005;
const LF_UNION_ST: u16 = 0x1006;
const LF_PROCEDURE: u16 = 0x1008;
const LF_MFUNCTION: u16 = 0x1009;
const LF_ARGLIST: u16 = 0x1201;
const LF_FIELDLIST: u16 = 0x1203;
const LF_MEMBER_ST: u16 = 0x1405;

pub const T_NOTYPE: u32 = 0;
pub const T_VOID: u32 = 0x0003;
pub const T_UCHAR: u32 = 0x0020;
const T_ULONG: u32 = 0x0022;
const PTR_ATTR: u32 = 0x0a | (4 << 13);
const PROP_FWDREF: u16 = 0x0080;
const MEMBER_PUBLIC: u16 = 3;

pub const CALL_CDECL: u8 = 0x00;
pub const CALL_FASTCALL: u8 = 0x04;
pub const CALL_STDCALL: u8 = 0x07;
pub const CALL_THISCALL: u8 = 0x0b;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Kind {
    Struct,
    Union,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum CType {
    Prim(u32, u32),
    Name(String),
    Tag(Kind, String),
    Ptr(Box<CType>),
    Array(Box<CType>, Option<u32>),
    Func(Box<Func>),
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Func {
    pub ret: CType,
    pub cc: u8,
    pub params: Vec<CType>,
    pub varargs: bool,
}

pub fn numeric(v: u32) -> Vec<u8> {
    match u16::try_from(v) {
        Ok(s) if s < 0x8000 => s.to_le_bytes().to_vec(),
        _ => {
            let mut out = 0x8004u16.to_le_bytes().to_vec();
            out.extend_from_slice(&v.to_le_bytes());
            out
        }
    }
}

fn primitive(words: &[&str]) -> Option<(u32, u32)> {
    let signed = words.contains(&"signed");
    let w: Vec<&str> = words
        .iter()
        .copied()
        .filter(|w| !matches!(*w, "const" | "volatile" | "signed"))
        .collect();
    let unsigned = w.contains(&"unsigned");
    let rest: Vec<&str> = w.iter().copied().filter(|x| *x != "unsigned").collect();
    let longs = rest.iter().filter(|x| **x == "long").count();
    let has_int = rest.contains(&"int");
    let core: Vec<&str> = rest
        .iter()
        .copied()
        .filter(|x| *x != "long" && *x != "int")
        .collect();
    Some(match (core.as_slice(), longs, unsigned) {
        ([], 0, false) if has_int || signed => (0x0074, 4),
        ([], 0, true) => (0x0075, 4),
        ([], 1, false) => (0x0012, 4),
        ([], 1, true) => (0x0022, 4),
        ([], 2, false) => (0x0013, 8),
        ([], 2, true) => (0x0023, 8),
        (["short"], 0, false) => (0x0011, 2),
        (["short"], 0, true) => (0x0021, 2),
        (["char"], 0, false) if signed => (0x0010, 1),
        (["char"], 0, false) => (0x0070, 1),
        (["char"], 0, true) => (0x0020, 1),
        (["__int64"], 0, false) => (0x0013, 8),
        (["__int64"], 0, true) => (0x0023, 8),
        (["__int32"], 0, false) => (0x0074, 4),
        (["__int32"], 0, true) => (0x0075, 4),
        (["__int16"], 0, false) => (0x0011, 2),
        (["__int16"], 0, true) => (0x0021, 2),
        (["__int8"], 0, false) => (0x0010, 1),
        (["__int8"], 0, true) => (0x0020, 1),
        (["double"], 1, false) => (0x0041, 8),
        ([one], 0, false) => match *one {
            "void" => (T_VOID, 0),
            "float" => (0x0040, 4),
            "double" => (0x0041, 8),
            "wchar_t" | "wchar16" => (0x0071, 2),
            "bool" => (0x0030, 1),
            "byte" | "uchar" | "undefined" | "undefined1" => (0x0020, 1),
            "sbyte" => (0x0010, 1),
            "ushort" | "word" | "undefined2" => (0x0021, 2),
            "uint" => (0x0075, 4),
            "ulong" | "dword" | "undefined4" => (0x0022, 4),
            "longlong" => (0x0013, 8),
            "ulonglong" | "qword" | "undefined8" => (0x0023, 8),
            "float10" => (0x0042, 10),
            _ => return None,
        },
        _ => return None,
    })
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Tok {
    Id(String),
    Num(u32),
    P(char),
    Ellipsis,
    Storage,
    PackPush(u32),
    PackPop,
    Include(String),
}

fn directive(line: &str, out: &mut Vec<Tok>) -> Result<()> {
    let t: String = line.chars().filter(|c| !c.is_whitespace()).collect();
    if let Some(rest) = t.strip_prefix("#pragmapack(push,") {
        let n = rest.trim_end_matches(')');
        let v = n
            .parse::<u32>()
            .with_context(|| format!("`{line}`: pack `{n}`"))?;
        if !matches!(v, 1 | 2 | 4 | 8 | 16) {
            bail!("`{line}`: pack {v} is not 1, 2, 4, 8 or 16");
        }
        out.push(Tok::PackPush(v));
    } else if t.starts_with("#pragmapack(pop") {
        out.push(Tok::PackPop);
    } else if let Some(rest) = t.strip_prefix("#include") {
        out.push(Tok::Include(
            rest.trim_matches(|c| c == '"' || c == '<' || c == '>')
                .to_string(),
        ));
    }
    Ok(())
}

fn lex(text: &str, out: &mut Vec<Tok>) -> Result<()> {
    let b: Vec<char> = text.chars().collect();
    let at = |k: usize| b.get(k).copied().unwrap_or('\0');
    let text_of =
        |s: usize, e: usize| -> String { b.get(s..e).unwrap_or_default().iter().collect() };
    let mut i = 0usize;
    let mut line_start = true;
    while i < b.len() {
        let c = at(i);
        if c == '\n' {
            line_start = true;
            i += 1;
        } else if c.is_whitespace() {
            i += 1;
        } else if c == '/' && at(i + 1) == '*' {
            i += 2;
            while i < b.len() && !(at(i) == '*' && at(i + 1) == '/') {
                i += 1;
            }
            i += 2;
        } else if c == '/' && at(i + 1) == '/' {
            while i < b.len() && at(i) != '\n' {
                i += 1;
            }
        } else if c == '#' && line_start {
            let start = i;
            while i < b.len() && at(i) != '\n' {
                i += 1;
            }
            directive(&text_of(start, i), out)?;
        } else {
            line_start = false;
            if c.is_ascii_alphabetic() || c == '_' {
                let start = i;
                while at(i).is_ascii_alphanumeric() || at(i) == '_' {
                    i += 1;
                }
                if at(i) == '@' && at(i + 1).is_ascii_digit() {
                    i += 1;
                    while at(i).is_ascii_digit() {
                        i += 1;
                    }
                }
                out.push(Tok::Id(text_of(start, i)));
            } else if c.is_ascii_digit() {
                let start = i;
                while at(i).is_ascii_alphanumeric() {
                    i += 1;
                }
                let s = text_of(start, i);
                let s = s.trim_end_matches(['u', 'U', 'l', 'L']);
                let v = match s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
                    Some(h) => u32::from_str_radix(h, 16),
                    None => s.parse::<u32>(),
                }
                .with_context(|| format!("number `{s}`"))?;
                out.push(Tok::Num(v));
            } else if c == '.' && at(i + 1) == '.' && at(i + 2) == '.' {
                out.push(Tok::Ellipsis);
                i += 3;
            } else if c == '@' {
                out.push(Tok::Storage);
                i += 1;
                while at(i).is_ascii_alphanumeric() {
                    i += 1;
                }
            } else {
                out.push(Tok::P(c));
                i += 1;
            }
        }
    }
    Ok(())
}

fn cc_of(s: &str) -> Option<u8> {
    match s {
        "__cdecl" => Some(CALL_CDECL),
        "__stdcall" => Some(CALL_STDCALL),
        "__fastcall" => Some(CALL_FASTCALL),
        "__thiscall" => Some(CALL_THISCALL),
        _ => None,
    }
}

const BASE_WORDS: &[&str] = &[
    "signed", "unsigned", "short", "long", "int", "char", "const", "volatile", "double", "float",
    "void", "__int64", "__int32", "__int16", "__int8",
];

#[derive(Debug, Clone)]
struct Def {
    kind: Kind,
    tag: String,
    fields: Vec<(String, CType)>,
    pack: u32,
}

struct Parser<'a> {
    t: &'a [Tok],
    i: usize,
    custom: bool,
    depth: u32,
}

impl<'a> Parser<'a> {
    fn new(t: &'a [Tok]) -> Self {
        Self {
            t,
            i: 0,
            custom: false,
            depth: 0,
        }
    }

    fn peek(&self) -> Option<&Tok> {
        self.t.get(self.i)
    }

    fn peek_at(&self, k: usize) -> Option<&Tok> {
        self.t.get(self.i.saturating_add(k))
    }

    fn is_p(&self, c: char) -> bool {
        self.peek() == Some(&Tok::P(c))
    }

    fn eat_p(&mut self, c: char) -> bool {
        let hit = self.is_p(c);
        if hit {
            self.i += 1;
        }
        hit
    }

    fn expect_p(&mut self, c: char) -> Result<()> {
        if !self.eat_p(c) {
            bail!("expected `{c}`, found {:?}", self.peek());
        }
        Ok(())
    }

    fn id(&self) -> Option<&str> {
        match self.peek() {
            Some(Tok::Id(s)) => Some(s.as_str()),
            _ => None,
        }
    }

    fn enter(&mut self) -> Result<()> {
        self.depth += 1;
        if self.depth > MAX_DEPTH {
            bail!("declarations nested deeper than {MAX_DEPTH}");
        }
        Ok(())
    }

    fn specifiers(&mut self, defs: &mut Vec<Def>, pack: u32) -> Result<CType> {
        self.enter()?;
        let r = self.specifiers_in(defs, pack);
        self.depth -= 1;
        r
    }

    fn specifiers_in(&mut self, defs: &mut Vec<Def>, pack: u32) -> Result<CType> {
        let mut words: Vec<String> = Vec::new();
        while let Some(w) = self.id().map(str::to_string) {
            match w.as_str() {
                "struct" | "union" => {
                    self.i += 1;
                    let kind = if w == "struct" {
                        Kind::Struct
                    } else {
                        Kind::Union
                    };
                    let tag = self.id().map(str::to_string);
                    if tag.is_some() {
                        self.i += 1;
                    }
                    if self.is_p('{') {
                        let tag = tag.context("an untagged struct body is outside the subset")?;
                        let fields = self.fields(defs, pack)?;
                        defs.push(Def {
                            kind,
                            tag: tag.clone(),
                            fields,
                            pack,
                        });
                        return Ok(CType::Tag(kind, tag));
                    }
                    return Ok(CType::Tag(kind, tag.context("`struct` with no tag")?));
                }
                "enum" => bail!("`enum` is outside the subset"),
                "__noreturn" | "extern" | "static" | "inline" | "__inline" => self.i += 1,
                x if BASE_WORDS.contains(&x) => {
                    words.push(w);
                    self.i += 1;
                }
                _ => {
                    if words
                        .iter()
                        .any(|x| !matches!(x.as_str(), "const" | "volatile"))
                    {
                        break;
                    }
                    self.i += 1;
                    while matches!(self.id(), Some("const" | "volatile")) {
                        self.i += 1;
                    }
                    return Ok(match primitive(&[w.as_str()]) {
                        Some((ti, size)) => CType::Prim(ti, size),
                        None => CType::Name(w),
                    });
                }
            }
        }
        let refs: Vec<&str> = words.iter().map(String::as_str).collect();
        if refs.iter().all(|w| matches!(*w, "const" | "volatile")) {
            bail!("no type in the specifiers at token {}", self.i);
        }
        let (ti, size) =
            primitive(&refs).with_context(|| format!("base type `{}`", refs.join(" ")))?;
        Ok(CType::Prim(ti, size))
    }

    fn fields(&mut self, defs: &mut Vec<Def>, pack: u32) -> Result<Vec<(String, CType)>> {
        self.expect_p('{')?;
        let mut out = Vec::new();
        while !self.eat_p('}') {
            if self.peek().is_none() {
                bail!("unterminated struct body");
            }
            let base = self.specifiers(defs, pack)?;
            loop {
                let (ty, name) = self.declarator(base.clone(), defs, pack)?;
                if self.is_p(':') {
                    bail!("a bit-field is outside the subset");
                }
                out.push((name.context("a field with no name")?, ty));
                if !self.eat_p(',') {
                    break;
                }
            }
            self.expect_p(';')?;
        }
        Ok(out)
    }

    fn declarator(
        &mut self,
        base: CType,
        defs: &mut Vec<Def>,
        pack: u32,
    ) -> Result<(CType, Option<String>)> {
        self.enter()?;
        let r = self.declarator_in(base, defs, pack);
        self.depth -= 1;
        r
    }

    fn declarator_in(
        &mut self,
        base: CType,
        defs: &mut Vec<Def>,
        pack: u32,
    ) -> Result<(CType, Option<String>)> {
        let mut ty = base;
        let mut levels = 0u32;
        loop {
            if self.eat_p('*') {
                levels += 1;
                if levels > MAX_DEPTH {
                    bail!("more than {MAX_DEPTH} levels of pointer");
                }
                ty = CType::Ptr(Box::new(ty));
            } else if matches!(self.id(), Some("const" | "volatile" | "__ptr32")) {
                self.i += 1;
            } else {
                break;
            }
        }
        if self.is_p('(')
            && (self.peek_at(1) == Some(&Tok::P('*'))
                || matches!(self.peek_at(1), Some(Tok::Id(s)) if cc_of(s).is_some()))
        {
            self.i += 1;
            let mut cc = CALL_CDECL;
            if let Some(c) = self.id().and_then(cc_of) {
                cc = c;
                self.i += 1;
            }
            self.expect_p('*')?;
            let name = self.id().map(str::to_string);
            if name.is_some() {
                self.i += 1;
            }
            self.expect_p(')')?;
            let (params, varargs) = self.params(defs, pack)?;
            let f = CType::Func(Box::new(Func {
                ret: ty,
                cc,
                params,
                varargs,
            }));
            return Ok((CType::Ptr(Box::new(f)), name));
        }
        let name = self.id().map(str::to_string);
        if name.is_some() {
            self.i += 1;
        }
        let mut dims = Vec::new();
        while self.eat_p('[') {
            let n = match self.peek() {
                Some(Tok::Num(n)) => {
                    let n = *n;
                    self.i += 1;
                    Some(n)
                }
                _ => None,
            };
            self.expect_p(']')?;
            dims.push(n);
            if levels.saturating_add(u32::try_from(dims.len()).unwrap_or(u32::MAX)) > MAX_DEPTH {
                bail!("more than {MAX_DEPTH} levels of pointer and array");
            }
        }
        for n in dims.into_iter().rev() {
            ty = CType::Array(Box::new(ty), n);
        }
        if self.peek() == Some(&Tok::Storage) {
            self.custom = true;
            self.i += 1;
        }
        Ok((ty, name))
    }

    fn params(&mut self, defs: &mut Vec<Def>, pack: u32) -> Result<(Vec<CType>, bool)> {
        self.expect_p('(')?;
        let mut params = Vec::new();
        let mut varargs = false;
        if self.eat_p(')') {
            return Ok((params, false));
        }
        loop {
            if self.peek() == Some(&Tok::Ellipsis) {
                self.i += 1;
                varargs = true;
            } else {
                let base = self.specifiers(defs, pack)?;
                let (ty, _) = self.declarator(base, defs, pack)?;
                if ty != CType::Prim(T_VOID, 0) {
                    params.push(ty);
                }
            }
            if !self.eat_p(',') {
                break;
            }
        }
        self.expect_p(')')?;
        if self.peek() == Some(&Tok::Storage) {
            self.custom = true;
            self.i += 1;
        }
        Ok((params, varargs))
    }
}

#[derive(Debug, Clone)]
pub struct Proto {
    pub name: String,
    pub func: Func,
    pub noreturn: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum NoType {
    CustomStorage,
    Unparsed(String),
}

pub fn parse_proto(s: &str) -> std::result::Result<Proto, NoType> {
    let mut t = Vec::new();
    lex(s, &mut t).map_err(|e| NoType::Unparsed(format!("{e:#}")))?;
    if t.contains(&Tok::Storage) {
        return Err(NoType::CustomStorage);
    }
    let open = t
        .iter()
        .position(|x| *x == Tok::P('('))
        .ok_or_else(|| NoType::Unparsed("no parameter list".into()))?;
    let name = match open.checked_sub(1).and_then(|k| t.get(k)) {
        Some(Tok::Id(n)) => n.clone(),
        _ => return Err(NoType::Unparsed("no name before `(`".into())),
    };
    let noreturn_tok = Tok::Id("__noreturn".into());
    let mut head: Vec<Tok> = t.get(..open.saturating_sub(1)).unwrap_or_default().to_vec();
    let noreturn = head.contains(&noreturn_tok);
    head.retain(|x| *x != noreturn_tok);
    let mut cc = CALL_CDECL;
    if let Some(c) = match head.last() {
        Some(Tok::Id(last)) => cc_of(last),
        _ => None,
    } {
        cc = c;
        head.pop();
    }
    let mut defs = Vec::new();
    let mut p = Parser::new(&head);
    let base = p
        .specifiers(&mut defs, 8)
        .map_err(|e| NoType::Unparsed(format!("return type: {e:#}")))?;
    let (ret, _) = p
        .declarator(base, &mut defs, 8)
        .map_err(|e| NoType::Unparsed(format!("return type: {e:#}")))?;
    if p.i != head.len() {
        return Err(NoType::Unparsed("tokens left in the return type".into()));
    }
    let tail: Vec<Tok> = t.get(open..).unwrap_or_default().to_vec();
    let mut q = Parser::new(&tail);
    let (params, varargs) = q
        .params(&mut defs, 8)
        .map_err(|e| NoType::Unparsed(format!("parameters: {e:#}")))?;
    if q.custom || p.custom {
        return Err(NoType::CustomStorage);
    }
    if q.i != tail.len() {
        return Err(NoType::Unparsed("tokens after the parameter list".into()));
    }
    Ok(Proto {
        name,
        func: Func {
            ret,
            cc,
            params,
            varargs,
        },
        noreturn,
    })
}

pub fn parse_type(s: &str) -> Result<CType> {
    let mut t = Vec::new();
    lex(s, &mut t)?;
    let mut defs = Vec::new();
    let mut p = Parser::new(&t);
    let base = p.specifiers(&mut defs, 8)?;
    let (ty, name) = p.declarator(base, &mut defs, 8)?;
    if name.is_some() || p.i != t.len() {
        bail!("`{s}` is not a bare type");
    }
    Ok(ty)
}

fn sdk_typedef(n: &str) -> Option<CType> {
    let p = CType::Prim;
    let ptr = |t: CType| CType::Ptr(Box::new(t));
    Some(match n {
        "size_t" | "UINT" | "UINT_PTR" => p(0x0075, 4),
        "INT" | "BOOL" | "INT_PTR" => p(0x0074, 4),
        "DWORD" | "ULONG" | "SIZE_T" | "ULONG_PTR" | "DWORD_PTR" => p(0x0022, 4),
        "LONG" | "HRESULT" | "LONG_PTR" => p(0x0012, 4),
        "WORD" | "USHORT" => p(0x0021, 2),
        "SHORT" => p(0x0011, 2),
        "BYTE" | "UCHAR" | "BOOLEAN" => p(0x0020, 1),
        "CHAR" => p(0x0070, 1),
        "WCHAR" => p(0x0071, 2),
        "LPVOID" | "PVOID" | "LPCVOID" | "HANDLE" | "HMODULE" | "HINSTANCE" | "HWND" => {
            ptr(p(T_VOID, 0))
        }
        "LPSTR" | "LPCSTR" | "PSTR" | "PCSTR" => ptr(p(0x0070, 1)),
        "LPWSTR" | "LPCWSTR" | "BSTR" => ptr(p(0x0071, 2)),
        "FARPROC" => ptr(CType::Func(Box::new(Func {
            ret: p(0x0074, 4),
            cc: CALL_STDCALL,
            params: Vec::new(),
            varargs: false,
        }))),
        _ => return None,
    })
}

fn align_up(v: u32, a: u32) -> u32 {
    if a <= 1 {
        v
    } else {
        v.div_ceil(a).saturating_mul(a)
    }
}

fn member(ti: u32, off: u32, name: &str) -> Result<Vec<u8>> {
    let mut m = Vec::new();
    m.extend_from_slice(&LF_MEMBER_ST.to_le_bytes());
    m.extend_from_slice(&MEMBER_PUBLIC.to_le_bytes());
    m.extend_from_slice(&ti.to_le_bytes());
    m.extend_from_slice(&numeric(off));
    m.extend_from_slice(&st(name)?);
    while !m.len().is_multiple_of(4) {
        let left = u8::try_from(4 - m.len() % 4).unwrap_or(0);
        m.push(0xf0 | left);
    }
    Ok(m)
}

fn tag_payload(
    kind: Kind,
    count: u16,
    prop: u16,
    fields: u32,
    size: u32,
    tag: &str,
) -> Result<(u16, Vec<u8>)> {
    let mut pl = Vec::new();
    pl.extend_from_slice(&count.to_le_bytes());
    pl.extend_from_slice(&prop.to_le_bytes());
    pl.extend_from_slice(&fields.to_le_bytes());
    let leaf = match kind {
        Kind::Struct => {
            pl.extend_from_slice(&0u32.to_le_bytes());
            pl.extend_from_slice(&0u32.to_le_bytes());
            LF_STRUCTURE_ST
        }
        Kind::Union => LF_UNION_ST,
    };
    pl.extend_from_slice(&numeric(size));
    pl.extend_from_slice(&st(tag)?);
    Ok((leaf, pl))
}

#[derive(Debug, Default)]
pub struct Types {
    pub records: Vec<Vec<u8>>,
    pub udts: Vec<(String, u32)>,
    typedefs: HashMap<String, CType>,
    defs: HashMap<String, Def>,
    order: Vec<String>,
    fwd: HashMap<(Kind, String), u32>,
    complete: HashMap<String, u32>,
    memo: HashMap<String, u32>,
    in_progress: BTreeSet<String>,
    pub placeholders: BTreeSet<String>,
    pub unlaid: BTreeMap<String, String>,
    pub skipped: Vec<String>,
}

impl Types {
    pub fn load_headers(
        &mut self,
        files: &[&str],
        read: &dyn Fn(&str) -> Result<String>,
    ) -> Result<()> {
        let mut seen = BTreeSet::new();
        let mut toks = Vec::new();
        for f in files {
            Self::lex_file(f, read, &mut seen, &mut toks, 0)?;
        }
        self.parse_items(&toks);
        Ok(())
    }

    fn lex_file(
        name: &str,
        read: &dyn Fn(&str) -> Result<String>,
        seen: &mut BTreeSet<String>,
        out: &mut Vec<Tok>,
        depth: u32,
    ) -> Result<()> {
        if depth > MAX_DEPTH {
            bail!("`#include` nested deeper than {MAX_DEPTH} at {name}");
        }
        if !seen.insert(name.to_string()) {
            return Ok(());
        }
        let text = read(name).with_context(|| format!("reading {name}"))?;
        let mut t = Vec::new();
        lex(&text, &mut t).with_context(|| format!("lexing {name}"))?;
        for tok in t {
            match tok {
                Tok::Include(inc) => Self::lex_file(&inc, read, seen, out, depth + 1)?,
                tok => out.push(tok),
            }
        }
        Ok(())
    }

    fn parse_items(&mut self, t: &[Tok]) {
        let mut pack: Vec<u32> = vec![8];
        let mut i = 0usize;
        while i < t.len() {
            match t.get(i) {
                Some(Tok::PackPush(n)) => {
                    pack.push(*n);
                    i += 1;
                    continue;
                }
                Some(Tok::PackPop) => {
                    if pack.len() > 1 {
                        pack.pop();
                    }
                    i += 1;
                    continue;
                }
                _ => {}
            }
            let cur = pack.last().copied().unwrap_or(8);
            let mut depth = 0i64;
            let mut end = i;
            while let Some(tok) = t.get(end) {
                match tok {
                    Tok::P('{') => depth += 1,
                    Tok::P('}') => depth -= 1,
                    Tok::P(';') if depth == 0 => break,
                    Tok::PackPush(_) | Tok::PackPop if depth == 0 => break,
                    _ => {}
                }
                end += 1;
            }
            let item = t.get(i..end).unwrap_or_default();
            if !item.is_empty()
                && let Err(e) = self.item(item, cur)
            {
                self.skipped.push(format!("{e:#}"));
            }
            i = if matches!(t.get(end), Some(Tok::P(';'))) {
                end + 1
            } else {
                end
            };
        }
    }

    fn item(&mut self, item: &[Tok], pack: u32) -> Result<()> {
        let mut defs = Vec::new();
        let mut p = Parser::new(item);
        let typedef = p.id() == Some("typedef");
        if typedef {
            p.i += 1;
        }
        let base = p.specifiers(&mut defs, pack)?;
        let mut names = Vec::new();
        while p.peek().is_some() {
            let (ty, name) = p.declarator(base.clone(), &mut defs, pack)?;
            if let Some(n) = name {
                names.push((n, ty));
            }
            if !p.eat_p(',') {
                break;
            }
        }
        if p.i != item.len() {
            bail!("tokens left over in a declaration ({:?})", item.get(p.i));
        }
        for d in defs {
            if !self.defs.contains_key(&d.tag) {
                self.order.push(d.tag.clone());
            }
            self.defs.insert(d.tag.clone(), d);
        }
        if typedef {
            for (n, ty) in names {
                if ty != CType::Name(n.clone()) {
                    self.typedefs.insert(n, ty);
                }
            }
        }
        Ok(())
    }

    fn resolve(&self, ty: &CType) -> CType {
        let mut cur = ty.clone();
        for _ in 0..MAX_DEPTH {
            let CType::Name(n) = &cur else {
                return cur;
            };
            match self.typedefs.get(n).cloned().or_else(|| sdk_typedef(n)) {
                Some(t) => cur = t,
                None => {
                    let kind = self.defs.get(n).map_or(Kind::Struct, |d| d.kind);
                    return CType::Tag(kind, n.clone());
                }
            }
        }
        cur
    }

    fn layout(&self, ty: &CType, depth: u32) -> std::result::Result<(u32, u32), String> {
        if depth > MAX_DEPTH {
            return Err(format!("nesting deeper than {MAX_DEPTH}"));
        }
        match self.resolve(ty) {
            CType::Prim(_, 0) => Err("void by value".into()),
            CType::Prim(_, s) => Ok((s, s.min(8))),
            CType::Ptr(_) => Ok((4, 4)),
            CType::Array(e, n) => {
                let (s, a) = self.layout(&e, depth + 1)?;
                Ok((s.saturating_mul(n.unwrap_or(0)), a))
            }
            CType::Tag(_, tag) => {
                let d = self
                    .defs
                    .get(&tag)
                    .ok_or_else(|| format!("`{tag}` by value, and no header defines it"))?;
                self.def_layout(d, depth + 1).map(|(s, a, _)| (s, a))
            }
            CType::Func(_) => Err("a function by value".into()),
            CType::Name(n) => Err(format!("unresolved `{n}`")),
        }
    }

    fn def_layout(&self, d: &Def, depth: u32) -> std::result::Result<(u32, u32, Vec<u32>), String> {
        let mut off = 0u32;
        let mut size = 0u32;
        let mut max_a = 1u32;
        let mut offs = Vec::with_capacity(d.fields.len());
        for (name, ty) in &d.fields {
            let (s, a) = self
                .layout(ty, depth)
                .map_err(|e| format!("field `{name}`: {e}"))?;
            let a = a.min(d.pack).max(1);
            max_a = max_a.max(a);
            match d.kind {
                Kind::Struct => {
                    off = align_up(off, a);
                    offs.push(off);
                    off = off.saturating_add(s);
                    size = off;
                }
                Kind::Union => {
                    offs.push(0);
                    size = size.max(s);
                }
            }
        }
        Ok((align_up(size, max_a), max_a, offs))
    }

    fn push(&mut self, leaf: u16, payload: &[u8]) -> Result<u32> {
        let n = u32::try_from(self.records.len()).context("type count")?;
        let ti = TI_MIN.checked_add(n).context("type index past u32")?;
        self.records.push(type_record(leaf, payload)?);
        Ok(ti)
    }

    fn memo_push(&mut self, key: String, leaf: u16, payload: &[u8]) -> Result<u32> {
        if let Some(t) = self.memo.get(&key) {
            return Ok(*t);
        }
        let t = self.push(leaf, payload)?;
        self.memo.insert(key, t);
        Ok(t)
    }

    fn forward(&mut self, kind: Kind, tag: &str) -> Result<u32> {
        if let Some(t) = self.fwd.get(&(kind, tag.to_string())) {
            return Ok(*t);
        }
        if !self.defs.contains_key(tag) {
            self.placeholders.insert(tag.to_string());
        }
        let (leaf, pl) = tag_payload(kind, 0, PROP_FWDREF, 0, 0, tag)?;
        let ti = self.push(leaf, &pl)?;
        self.fwd.insert((kind, tag.to_string()), ti);
        Ok(ti)
    }

    fn complete(&mut self, tag: &str, depth: u32) -> Result<Option<u32>> {
        if let Some(t) = self.complete.get(tag) {
            return Ok(Some(*t));
        }
        let Some(d) = self.defs.get(tag).cloned() else {
            return Ok(None);
        };
        if self.unlaid.contains_key(tag) || !self.in_progress.insert(tag.to_string()) {
            return Ok(None);
        }
        let r = self.complete_in(&d, depth);
        self.in_progress.remove(tag);
        r
    }

    fn complete_in(&mut self, d: &Def, depth: u32) -> Result<Option<u32>> {
        let (size, _, offs) = match self.def_layout(d, 0) {
            Ok(l) => l,
            Err(e) => {
                self.unlaid.insert(d.tag.clone(), e);
                return Ok(None);
            }
        };
        self.forward(d.kind, &d.tag)?;
        let mut fl = Vec::new();
        for ((name, ty), off) in d.fields.iter().zip(&offs) {
            let ti = self.ti_at(ty, depth + 1)?.unwrap_or(T_NOTYPE);
            fl.extend_from_slice(&member(ti, *off, name)?);
        }
        let field_ti = self.push(LF_FIELDLIST, &fl)?;
        let count = u16::try_from(d.fields.len()).context("field count")?;
        let (leaf, pl) = tag_payload(d.kind, count, 0, field_ti, size, &d.tag)?;
        let ti = self.push(leaf, &pl)?;
        self.complete.insert(d.tag.clone(), ti);
        Ok(Some(ti))
    }

    pub fn emit_headers(&mut self) -> Result<()> {
        let mut named: BTreeSet<String> = self.udts.iter().map(|(n, _)| n.clone()).collect();
        for tag in self.order.clone() {
            if let Some(ti) = self.complete(&tag, 0)?
                && named.insert(tag.clone())
            {
                self.udts.push((tag, ti));
            }
        }
        let mut names: Vec<String> = self.typedefs.keys().cloned().collect();
        names.sort();
        for n in names {
            if let Some(ti) = self.ti(&CType::Name(n.clone()))?
                && named.insert(n.clone())
            {
                self.udts.push((n, ti));
            }
        }
        Ok(())
    }

    pub fn ti(&mut self, ty: &CType) -> Result<Option<u32>> {
        self.ti_at(ty, 0)
    }

    fn ti_at(&mut self, ty: &CType, depth: u32) -> Result<Option<u32>> {
        if depth > MAX_DEPTH {
            bail!("a type nested deeper than {MAX_DEPTH}");
        }
        let ty = self.resolve(ty);
        Ok(match &ty {
            CType::Prim(p, _) => Some(*p),
            CType::Name(_) => None,
            CType::Tag(kind, tag) => match self.complete(tag, depth)? {
                Some(t) => Some(t),
                None => Some(self.forward(*kind, tag)?),
            },
            CType::Ptr(inner) => {
                let inner = self.resolve(inner);
                if let CType::Prim(p, _) = inner
                    && p < 0x100
                {
                    return Ok(Some(0x400 | p));
                }
                let target = match &inner {
                    CType::Tag(kind, tag) => Some(self.forward(*kind, tag)?),
                    other => self.ti_at(other, depth + 1)?,
                };
                let Some(target) = target else {
                    return Ok(None);
                };
                let mut pl = target.to_le_bytes().to_vec();
                pl.extend_from_slice(&PTR_ATTR.to_le_bytes());
                Some(self.memo_push(format!("ptr {target}"), LF_POINTER, &pl)?)
            }
            CType::Array(elem, n) => {
                let Some(e) = self.ti_at(elem, depth + 1)? else {
                    return Ok(None);
                };
                let Ok((esize, _)) = self.layout(elem, 0) else {
                    return Ok(None);
                };
                let bytes = esize.saturating_mul(n.unwrap_or(0));
                let mut pl = e.to_le_bytes().to_vec();
                pl.extend_from_slice(&T_ULONG.to_le_bytes());
                pl.extend_from_slice(&numeric(bytes));
                pl.extend_from_slice(&st("")?);
                Some(self.memo_push(format!("array {e} {bytes}"), LF_ARRAY_ST, &pl)?)
            }
            CType::Func(f) => self.procedure_at(f, depth + 1)?,
        })
    }

    pub fn procedure(&mut self, f: &Func) -> Result<Option<u32>> {
        self.procedure_at(f, 0)
    }

    fn this_of(&mut self, first: &CType, depth: u32) -> Result<Option<(u32, u32)>> {
        let CType::Ptr(inner) = self.resolve(first) else {
            return Ok(None);
        };
        let CType::Tag(kind, tag) = self.resolve(&inner) else {
            return Ok(None);
        };
        let class = self.forward(kind, &tag)?;
        Ok(self.ti_at(first, depth + 1)?.map(|this| (class, this)))
    }

    fn procedure_at(&mut self, f: &Func, depth: u32) -> Result<Option<u32>> {
        if depth > MAX_DEPTH {
            bail!("a type nested deeper than {MAX_DEPTH}");
        }
        let Some(ret) = self.ti_at(&f.ret, depth + 1)? else {
            return Ok(None);
        };
        let mut params: &[CType] = &f.params;
        let mut member = None;
        if f.cc == CALL_THISCALL
            && let Some((first, rest)) = f.params.split_first()
            && let Some(m) = self.this_of(first, depth)?
        {
            member = Some(m);
            params = rest;
        }
        let mut args = Vec::with_capacity(params.len() + 1);
        for p in params {
            let Some(t) = self.ti_at(p, depth + 1)? else {
                return Ok(None);
            };
            args.push(t);
        }
        if f.varargs {
            args.push(T_NOTYPE);
        }
        let n = u32::try_from(args.len()).context("argument count")?;
        let mut pl = n.to_le_bytes().to_vec();
        for a in &args {
            pl.extend_from_slice(&a.to_le_bytes());
        }
        let arglist = self.memo_push(format!("args {args:?}"), LF_ARGLIST, &pl)?;
        let count = u16::try_from(args.len()).context("parameter count")?;
        let mut pl = ret.to_le_bytes().to_vec();
        let leaf = match member {
            Some((class, this)) => {
                pl.extend_from_slice(&class.to_le_bytes());
                pl.extend_from_slice(&this.to_le_bytes());
                LF_MFUNCTION
            }
            None => LF_PROCEDURE,
        };
        pl.push(f.cc);
        pl.push(0);
        pl.extend_from_slice(&count.to_le_bytes());
        pl.extend_from_slice(&arglist.to_le_bytes());
        if member.is_some() {
            pl.extend_from_slice(&0i32.to_le_bytes());
        }
        Ok(Some(self.memo_push(
            format!("proc {leaf} {pl:?}"),
            leaf,
            &pl,
        )?))
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing)]
mod tests {
    use super::*;

    #[test]
    fn a_prototype_parses_with_its_convention() {
        let p = parse_proto("void * __thiscall arena_alloc(arena * a, int n)").unwrap();
        assert_eq!(p.name, "arena_alloc");
        assert_eq!(p.func.cc, CALL_THISCALL);
        assert_eq!(p.func.params.len(), 2);
        let q = parse_proto("__noreturn void __cdecl diag_fatal(int n, ...)").unwrap();
        assert!(q.noreturn && q.func.varargs);
        let r = parse_proto("void __stdcall _AbortPass@4(void)").unwrap();
        assert_eq!(r.name, "_AbortPass@4");
        assert!(r.func.params.is_empty());
        let s = parse_proto("int __cdecl run_list(int* passes, ctx* c, int (*fn)(ctx*))").unwrap();
        assert_eq!(s.func.params.len(), 3);
        assert!(matches!(&s.func.params[2], CType::Ptr(f) if matches!(**f, CType::Func(_))));
    }

    #[test]
    fn custom_storage_is_refused_not_guessed() {
        let e = parse_proto("sym * __thiscall sym_finish(sym *s @EDI, decl *d @ESI)");
        assert_eq!(e.err(), Some(NoType::CustomStorage));
    }

    #[test]
    fn a_header_struct_lays_out_sequentially_with_the_pack() {
        let h = "typedef unsigned char u8;\n#pragma pack(push, 1)\ntypedef struct a { u8 x; int y; } a;\n#pragma pack(pop)\nstruct b { u8 x; int y; struct b *next; char name[3]; };\n";
        let mut t = Types::default();
        t.load_headers(&["h.h"], &|_| Ok(h.to_string())).unwrap();
        let a = t.defs.get("a").cloned().unwrap();
        assert_eq!(t.def_layout(&a, 0).unwrap().0, 5, "packed 1");
        let b = t.defs.get("b").cloned().unwrap();
        let (size, _, offs) = t.def_layout(&b, 0).unwrap();
        assert_eq!(offs, vec![0, 4, 8, 12]);
        assert_eq!(size, 16);
        t.emit_headers().unwrap();
        assert!(t.complete.contains_key("b"));
        assert!(
            t.unlaid.is_empty() && t.skipped.is_empty(),
            "{:?} {:?}",
            t.unlaid,
            t.skipped
        );
        let names: Vec<&str> = t.udts.iter().map(|(n, _)| n.as_str()).collect();
        assert_eq!(names, ["a", "b", "u8"]);
    }

    #[test]
    fn data_spellings_parse() {
        assert!(matches!(
            parse_type("char[16]").unwrap(),
            CType::Array(_, Some(16))
        ));
        assert!(matches!(
            parse_type("wchar_t *[4]").unwrap(),
            CType::Array(_, Some(4))
        ));
        assert!(matches!(
            parse_type("unsigned int *").unwrap(),
            CType::Ptr(_)
        ));
        assert_eq!(parse_type("undefined4").unwrap(), CType::Prim(0x22, 4));
        assert!(matches!(
            parse_type("char *[]").unwrap(),
            CType::Array(_, None)
        ));
    }

    #[test]
    fn a_name_no_header_declares_is_a_forward_reference() {
        let mut t = Types::default();
        let ti = t.ti(&parse_type("FILE *").unwrap()).unwrap().unwrap();
        assert!(ti >= TI_MIN);
        assert!(t.placeholders.contains("FILE"));
        assert_eq!(t.ti(&parse_type("char *").unwrap()).unwrap(), Some(0x470));
        let f = parse_proto("HANDLE __stdcall open_it(LPCSTR name, DWORD flags)").unwrap();
        let p = t.procedure(&f.func).unwrap().unwrap();
        assert_eq!(t.procedure(&f.func).unwrap(), Some(p), "memoised");
    }

    #[test]
    fn a_member_function_takes_its_this_from_the_first_parameter() {
        let mut t = Types::default();
        t.load_headers(&["h.h"], &|_| Ok("struct arena { int n; };".to_string()))
            .unwrap();
        let f = parse_proto("void * __thiscall arena_alloc(arena * a, int n)").unwrap();
        let ti = t.procedure(&f.func).unwrap().unwrap();
        let rec = &t.records[usize::try_from(ti - TI_MIN).unwrap()];
        assert_eq!(u16::from_le_bytes([rec[2], rec[3]]), LF_MFUNCTION);
        assert_eq!(rec[16], CALL_THISCALL);
        assert_eq!(
            u16::from_le_bytes([rec[18], rec[19]]),
            1,
            "this is not an argument"
        );
    }

    #[test]
    fn outside_the_subset_is_skipped_and_counted() {
        let mut t = Types::default();
        let h = "enum e { A }; struct s { int a : 3; }; struct ok { int a; };";
        t.load_headers(&["h.h"], &|_| Ok(h.to_string())).unwrap();
        assert_eq!(t.skipped.len(), 2, "{:?}", t.skipped);
        t.emit_headers().unwrap();
        assert_eq!(t.udts.len(), 1);
        assert!(lex("#pragma pack(push, 3)", &mut Vec::new()).is_err());
        assert!(parse_type("int 99999999999").is_err());
    }

    #[test]
    fn deep_nesting_is_an_error_not_a_stack_overflow() {
        let deep = format!("int {}x", "*".repeat(100_000));
        assert!(parse_type(&deep).is_err());
        let fns = format!(
            "void f({}int{})",
            "int (*g)(".repeat(10_000),
            ")".repeat(10_000)
        );
        assert!(matches!(parse_proto(&fns), Err(NoType::Unparsed(_))));
        let bodies = format!(
            "struct s0 {{ {} int x; {} }};",
            (1..5000)
                .map(|i| format!("struct s{i} {{"))
                .collect::<String>(),
            "} y;".repeat(4999)
        );
        let mut t = Types::default();
        t.load_headers(&["h.h"], &|_| Ok(bodies.clone())).unwrap();
        assert_eq!(t.skipped.len(), 1);
        let chain: String = (1..20_000)
            .map(|i| format!("typedef t{} *t{i};\n", i - 1))
            .collect::<String>();
        let mut t = Types::default();
        t.load_headers(&["h.h"], &|_| {
            Ok(format!("typedef struct q {{ int a; }} t0;\n{chain}"))
        })
        .unwrap();
        assert!(t.emit_headers().is_err());
        let mut t = Types::default();
        let r = t.load_headers(&["0"], &|n: &str| {
            let k: u32 = n.parse().unwrap_or(0);
            Ok(format!("#include \"{}\"\n", k + 1))
        });
        assert!(format!("{:#}", r.unwrap_err()).contains("nested deeper"));
    }

    #[test]
    fn a_struct_by_value_chain_is_bounded() {
        let mut h = String::from("struct v0 { int a; };\n");
        for i in 1..5000 {
            h.push_str(&format!("struct v{i} {{ struct v{} inner; }};\n", i - 1));
        }
        let mut t = Types::default();
        t.load_headers(&["h.h"], &|_| Ok(h.clone())).unwrap();
        t.emit_headers().unwrap();
        assert!(
            t.unlaid.values().any(|e| e.contains("deeper")),
            "{:?}",
            t.unlaid.len()
        );
    }
}
