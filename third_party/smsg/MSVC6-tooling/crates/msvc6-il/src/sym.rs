use crate::cursor::{Cursor, Ids, Result, error, quote};
use crate::ty;

pub const SCOPE_BUCKETS: u32 = 16;
pub const MAX_COMMENTS: i32 = 1 << 16;

pub const WRITER_ANCHORS: [(u8, usize); 2] = [(1, 12), (3, 7)];

pub const READER_LAYOUT: [(u8, &str); 18] = [
    (
        1,
        "level id b30 name type skip storage rclass size flags[+segment] cvtype",
    ),
    (
        2,
        "level id b30 name type skip storage rclass size flags[+segment] cvtype elem",
    ),
    (3, "level id role name refs lflags"),
    (
        4,
        "id b30 name type fclass size flags[+segment] cvtype[+b4a l46]",
    ),
    (5, "base id long b30 cvtype"),
    (6, "(end)"),
    (7, "level id name type skip long rclass long cvtype id long"),
    (8, "level id name cvtype type skip long value"),
    (9, "id name kind class"),
    (0x0a, "long pch count comments.."),
    (0x0b, "level id name skip cvtype"),
    (0x0d, "level"),
    (
        0x0e,
        "id b30 name type fclass size flags[+segment] cvtype[+b4a l46 l77] ex sy s6d fflags s6b",
    ),
    (
        0x10,
        "id b30 name type fclass size flags[+segment] cvtype[+b4a l46] target",
    ),
    (0x12, "id name"),
    (0x16, "str"),
    (0x17, "str"),
    (0x19, "long"),
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Value {
    Num(i64),
    Hex(u32),
    Id(u32),
    Ty(u16),
    Storage(u8),
    Rclass(u8),
    Fclass(u8),
    Text(Vec<u8>),
    Comments(Vec<String>),
    Note(&'static str),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Sym {
    pub off: usize,
    pub end: usize,
    pub class: u8,
    pub id: Option<u32>,
    pub name: Option<Vec<u8>>,
    pub level: Option<u8>,
    pub fields: Vec<(&'static str, Value)>,
    pub ex: Option<u32>,
    pub sy: Option<u32>,
    pub ends_list: bool,
}

pub const CLASS_VAR: u8 = 1;
pub const CLASS_GVAR: u8 = 2;
pub const CLASS_LABEL: u8 = 3;
pub const CLASS_FUNC: u8 = 4;
pub const CLASS_END: u8 = 6;
pub const CLASS_SEGMENT: u8 = 9;
pub const CLASS_GLEND: u8 = 0x0a;
pub const CLASS_BLOCK: u8 = 0x0d;
pub const CLASS_FDEF: u8 = 0x0e;
pub const CLASS_FREF: u8 = 0x10;
pub const CLASS_SRCFILE: u8 = 0x12;

#[must_use]
pub fn class_name(c: u8) -> &'static str {
    match c {
        1 => "var",
        2 => "gvar",
        3 => "label",
        4 => "func",
        5 => "alias5",
        6 => "end",
        7 => "rec7",
        8 => "rec8",
        9 => "segment",
        0x0a => "glend",
        0x0b => "typedef",
        0x0d => "block",
        0x0e => "fdef",
        0x10 => "fref10",
        0x12 => "srcfile",
        0x16 => "rec16",
        0x17 => "pchblob",
        0x19 => "rec19",
        _ => "?",
    }
}

impl Sym {
    #[must_use]
    pub fn get(&self, key: &str) -> Option<&Value> {
        self.fields.iter().find(|(k, _)| *k == key).map(|(_, v)| v)
    }

    #[must_use]
    pub fn num(&self, key: &str) -> Option<i64> {
        match self.get(key)? {
            Value::Num(n) => Some(*n),
            Value::Hex(h) | Value::Id(h) => Some(i64::from(*h)),
            Value::Ty(t) => Some(i64::from(*t)),
            Value::Storage(b) | Value::Rclass(b) | Value::Fclass(b) => Some(i64::from(*b)),
            _ => None,
        }
    }

    #[must_use]
    pub fn flags(&self) -> u32 {
        match self.get("flags") {
            Some(Value::Hex(h)) => *h,
            _ => 0,
        }
    }

    #[must_use]
    pub fn fflags(&self) -> u32 {
        match self.get("fflags") {
            Some(Value::Hex(h)) => *h,
            _ => 0,
        }
    }

    #[must_use]
    pub fn s6d(&self) -> Option<i16> {
        self.num("s6d").and_then(|n| i16::try_from(n).ok())
    }

    #[must_use]
    pub fn name_text(&self) -> String {
        self.name
            .as_deref()
            .map(crate::cursor::latin1)
            .unwrap_or_default()
    }

    #[must_use]
    pub fn is_function(&self) -> bool {
        matches!(self.class, CLASS_FUNC | CLASS_FDEF | CLASS_FREF)
    }
}

fn wide(c: &mut Cursor<'_>, wide: bool) -> Result<i64> {
    if wide {
        Ok(i64::from(c.long()?))
    } else {
        Ok(i64::from(c.short()?))
    }
}

fn byte(c: &mut Cursor<'_>) -> Result<i64> {
    Ok(i64::from(c.byte()?))
}

fn hex(c: &mut Cursor<'_>) -> Result<Value> {
    Ok(Value::Hex(u32::from(c.byte()?)))
}

pub fn comment(c: &mut Cursor<'_>) -> Result<String> {
    let kind = i8::from_le_bytes([c.byte()?]);
    if kind == 7 {
        let n = i8::from_le_bytes([c.byte()?]);
        let b = c.take(usize::try_from(n).unwrap_or(0))?;
        return Ok(format!("{kind}:{}", quote(b)));
    }
    let s = c.str()?;
    Ok(format!("{kind}:{}", quote(s)))
}

fn offset(v: i32) -> u32 {
    u32::from_le_bytes(v.to_le_bytes())
}

pub fn read(c: &mut Cursor<'_>, wide_sizes: bool) -> Result<Sym> {
    let off = c.pos();
    let class = c.byte()?;
    let mut s = Sym {
        off,
        end: off,
        class,
        id: None,
        name: None,
        level: None,
        fields: Vec::new(),
        ex: None,
        sy: None,
        ends_list: false,
    };
    let f = &mut s.fields;
    match class {
        CLASS_VAR | CLASS_GVAR => {
            s.level = Some(c.byte()?);
            s.id = Some(c.id()?);
            f.push(("b30", Value::Num(byte(c)?)));
            s.name = Some(c.str()?.to_vec());
            f.push(("type", Value::Ty(c.ty()?)));
            f.push(("skip", Value::Num(byte(c)?)));
            f.push(("storage", Value::Storage(c.byte()?)));
            f.push(("rclass", Value::Rclass(c.byte()?)));
            f.push(("size", Value::Num(i64::from(c.long()?))));
            let fl = c.id()?;
            f.push(("flags", Value::Hex(fl)));
            if fl & 0x200 != 0 {
                f.push(("segment", Value::Id(c.id()?)));
            }
            f.push(("cvtype", Value::Num(wide(c, wide_sizes)?)));
            if class == CLASS_GVAR {
                f.push(("elem", Value::Num(i64::from(c.long()?))));
            }
        }
        CLASS_LABEL => {
            s.level = Some(c.byte()?);
            s.id = Some(c.id()?);
            f.push(("role", Value::Num(byte(c)?)));
            s.name = Some(c.str()?.to_vec());
            f.push(("refs", Value::Num(i64::from(c.short()?))));
            f.push(("lflags", hex(c)?));
        }
        CLASS_FUNC | CLASS_FDEF | CLASS_FREF => {
            s.id = Some(c.id()?);
            f.push(("b30", Value::Num(byte(c)?)));
            s.name = Some(c.str()?.to_vec());
            f.push(("type", Value::Ty(c.ty()?)));
            f.push(("fclass", Value::Fclass(c.byte()?)));
            f.push(("size", Value::Num(i64::from(c.long()?))));
            let fl = c.id()?;
            f.push(("flags", Value::Hex(fl)));
            if fl & 0x200 != 0 {
                f.push(("segment", Value::Id(c.id()?)));
            }
            f.push(("cvtype", Value::Num(wide(c, wide_sizes)?)));
            let both = fl & 0xc == 0xc;
            if both {
                f.push(("b4a", Value::Num(byte(c)?)));
                f.push(("l46", Value::Num(i64::from(c.long()?))));
            }
            if class == CLASS_FDEF {
                if both {
                    f.push(("l77", Value::Num(i64::from(c.long()?))));
                }
                if c.ids == Ids::Plain {
                    let ex = offset(c.long()?);
                    let sy = offset(c.long()?);
                    s.ex = Some(ex);
                    s.sy = Some(sy);
                    f.push(("ex", Value::Hex(ex)));
                    f.push(("sy", Value::Hex(sy)));
                }
                f.push(("s6d", Value::Num(i64::from(c.short()?))));
                f.push(("fflags", Value::Hex(c.id()?)));
                f.push(("s6b", Value::Num(i64::from(c.short()?))));
                if c.ids == Ids::Compact {
                    let ex = c.long()?;
                    s.ex = Some(offset(ex));
                    f.push(("l4f", Value::Num(i64::from(ex))));
                    f.push(("s", Value::Num(i64::from(c.short()?))));
                }
            } else if class == CLASS_FREF {
                f.push(("target", Value::Id(c.id()?)));
            }
        }
        5 => {
            f.push(("base", Value::Id(c.id()?)));
            s.id = Some(c.id()?);
            f.push(("l10", Value::Num(i64::from(c.long()?))));
            f.push(("b30", Value::Num(byte(c)?)));
            f.push(("cvtype", Value::Num(wide(c, wide_sizes)?)));
        }
        CLASS_END => s.ends_list = true,
        7 => {
            s.level = Some(c.byte()?);
            s.id = Some(c.id()?);
            s.name = Some(c.str()?.to_vec());
            f.push(("type", Value::Ty(c.ty()?)));
            f.push(("skip", Value::Num(byte(c)?)));
            f.push(("l10", Value::Num(i64::from(c.long()?))));
            f.push(("rclass", Value::Rclass(c.byte()?)));
            f.push(("l0c", Value::Num(i64::from(c.long()?))));
            f.push(("cvtype", Value::Num(wide(c, wide_sizes)?)));
            f.push(("i3a", Value::Id(c.id()?)));
            f.push(("l40", Value::Num(i64::from(c.long()?))));
        }
        8 => {
            s.level = Some(c.byte()?);
            s.id = Some(c.id()?);
            s.name = Some(c.str()?.to_vec());
            f.push(("cvtype", Value::Num(wide(c, wide_sizes)?)));
            f.push(("type", Value::Ty(c.ty()?)));
            f.push(("skip", Value::Num(byte(c)?)));
            f.push(("l10", Value::Num(i64::from(c.long()?))));
            f.push(("value", Value::Text(c.lstr()?.to_vec())));
        }
        CLASS_SEGMENT => {
            s.id = Some(c.id()?);
            s.name = Some(c.str()?.to_vec());
            f.push(("kind", Value::Num(byte(c)?)));
            f.push(("class", Value::Text(c.str()?.to_vec())));
        }
        CLASS_GLEND => {
            f.push(("l", Value::Num(i64::from(c.long()?))));
            f.push(("pch", Value::Text(c.str()?.to_vec())));
            let at = c.pos();
            let n = c.long()?;
            if n > MAX_COMMENTS {
                return Err(error(
                    at,
                    format!(
                        "{n} object-file comments: more than the {MAX_COMMENTS} this reader takes"
                    ),
                ));
            }
            let mut v = Vec::new();
            for _ in 0..n.max(0) {
                v.push(comment(c)?);
            }
            f.push(("comments", Value::Comments(v)));
            s.ends_list = true;
        }
        0x0b => {
            s.level = Some(c.byte()?);
            s.id = Some(c.id()?);
            s.name = Some(c.str()?.to_vec());
            f.push(("skip", Value::Num(byte(c)?)));
            f.push(("cvtype", Value::Num(wide(c, wide_sizes)?)));
        }
        CLASS_BLOCK => s.level = Some(c.byte()?),
        CLASS_SRCFILE => {
            s.id = Some(c.id()?);
            s.name = Some(c.str()?.to_vec());
        }
        0x16 | 0x17 => s.name = Some(c.str()?.to_vec()),
        0x19 => {
            f.push(("l", Value::Num(i64::from(c.long()?))));
            f.push((
                "note",
                Value::Note("C2 returns 0x19 as a record pointer here: never seen"),
            ));
        }
        _ => {
            return Err(error(
                off,
                format!("symbol class {class:#04x} is none C2's symbol reader handles"),
            ));
        }
    }
    s.end = c.pos();
    Ok(s)
}

#[must_use]
pub fn hx(v: i64) -> String {
    if (-9..=9).contains(&v) {
        format!("{v}")
    } else if v < 0 {
        format!("-0x{:x}", v.unsigned_abs())
    } else {
        format!("0x{v:x}")
    }
}

fn value_text(v: &Value) -> String {
    match v {
        Value::Num(n) => hx(*n),
        Value::Hex(h) => format!("0x{h:x}"),
        Value::Id(i) => format!("#{i:x}"),
        Value::Ty(t) => format!("{} (0x{t:x})", ty::of(*t).kind_name()),
        Value::Storage(b) => format!("{} (+0x36 0x{:x})", b & 7, u32::from(b & 7) << 4),
        Value::Rclass(b) => format!("{} (+0x36 0x{:x})", b & 0xf, u32::from(b & 0xf) << 7),
        Value::Fclass(b) => format!("{} (+0x36 0x{:x})", b & 0xf, u32::from(b & 0xf) << 4),
        Value::Text(t) => quote(t),
        Value::Comments(c) => format!("[{}]", c.join(" ")),
        Value::Note(n) => (*n).to_string(),
    }
}

#[must_use]
pub fn name_hash(name: &[u8]) -> (u32, u32) {
    let mut h: u32 = 0;
    for &c in name {
        h = u32::from(c)
            .wrapping_add(h >> 4)
            .wrapping_add(h.wrapping_mul(4));
    }
    (h, (h >> 16) ^ (h & 0xffff))
}

#[must_use]
pub fn render(s: &Sym) -> String {
    let mut out = format!("{:02x} {:<8}", s.class, class_name(s.class));
    if let Some(id) = s.id {
        out.push_str(&format!(" #{id:x}"));
    }
    if let Some(n) = &s.name {
        out.push(' ');
        out.push_str(&quote(n));
    }
    if let Some(l) = s.level {
        out.push_str(&format!(" level {l}"));
        if l >= 2
            && matches!(s.class, CLASS_VAR | CLASS_GVAR)
            && let Some(n) = &s.name
        {
            out.push_str(&format!(" bucket {}", name_hash(n).1 % SCOPE_BUCKETS));
        }
    }
    for (k, v) in &s.fields {
        out.push_str(&format!(" {k}={}", value_text(v)));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fdef() -> Vec<u8> {
        let mut b = vec![CLASS_FDEF, 0xd7, 0x00, 0x00];
        b.extend(b"?two_locals@@YAHHH@Z\0");
        b.extend([
            0x01, 0x05, 0x04, 0x00, 0x00, 0x00, 0x80, 0xf4, 0x03, 0x00, 0x00,
        ]);
        b.extend([0x00, 0x22, 0x28, 0x00, 0x02]);
        b
    }

    #[test]
    fn a_definition_carries_its_offsets_size_and_flags() {
        let b = fdef();
        let s = read(&mut Cursor::new(&b, 0), true).unwrap();
        assert_eq!(s.end, b.len());
        assert_eq!(s.id, Some(0xd7));
        assert_eq!(s.ex, Some(0x3f4));
        assert_eq!(s.sy, Some(0));
        assert_eq!(s.s6d(), Some(0x22));
        assert_eq!(s.fflags(), 0x28);
        assert_eq!(
            render(&s),
            "0e fdef     #d7 \"?two_locals@@YAHHH@Z\" b30=0 type=int (0x1) fclass=5 (+0x36 0x50) size=4 flags=0x0 cvtype=0 ex=0x3f4 sy=0x0 s6d=0x22 fflags=0x28 s6b=2"
        );
    }

    #[test]
    fn a_local_shows_its_scope_bucket() {
        let mut b = vec![CLASS_VAR, 0x02, 0xda, 0x00, 0x00, b'x', 0];
        b.extend([0x01, 0x00, 0x01, 0x04, 0x04, 0x00, 0x01, 0x00]);
        let s = read(&mut Cursor::new(&b, 0), true).unwrap();
        assert_eq!(
            render(&s),
            "01 var      #da \"x\" level 2 bucket 8 b30=0 type=int (0x1) skip=0 storage=1 (+0x36 0x10) rclass=4 (+0x36 0x200) size=4 flags=0x100 cvtype=0"
        );
    }

    #[test]
    fn the_glend_record_ends_the_list_and_bounds_its_comments() {
        let b = [CLASS_GLEND, 0, 0, 1, 4, b'l', b'i', b'b', 0];
        let s = read(&mut Cursor::new(&b, 0), true).unwrap();
        assert!(s.ends_list);
        assert_eq!(render(&s), "0a glend    l=0 pch=\"\" comments=[4:\"lib\"]");
        let mut big = vec![CLASS_GLEND, 0, 0, 0x80];
        big.extend(i32::MAX.to_le_bytes());
        assert!(read(&mut Cursor::new(&big, 0), true).is_err());
        assert!(read(&mut Cursor::new(&[0x42], 0), true).is_err());
    }

    #[test]
    fn the_name_hash_buckets_locals() {
        assert_eq!(name_hash(b"x").1 % SCOPE_BUCKETS, 8);
        assert_eq!(name_hash(b"y").1 % SCOPE_BUCKETS, 9);
        assert_eq!(hx(-10), "-0xa");
        assert_eq!(hx(9), "9");
    }
}
