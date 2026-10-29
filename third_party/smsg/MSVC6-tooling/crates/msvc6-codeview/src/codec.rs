use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_base::le;
use msvc6_base::srcio;

pub const LF_PAD0: u8 = 0xf0;
pub const LF_NUMERIC: u16 = 0x8000;
pub const LF_INDEX: u16 = 0x1404;
pub const LF_FIELDLIST: u16 = 0x1203;
pub const LF_METHODLIST: u16 = 0x1206;
pub const LF_ONEMETHOD_ST: u16 = 0x140b;

pub const S_PROCREF_ST: u16 = 0x0400;
pub const S_DATAREF_ST: u16 = 0x0401;
pub const S_LPROCREF_ST: u16 = 0x0403;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Gen {
    St,
    Sz,
}

pub fn bytes(d: &[u8], at: usize, n: usize) -> Result<&[u8]> {
    le::bytes_at(d, at, n).map_err(|e| anyhow!("{e}"))
}

pub fn u8_at(d: &[u8], at: usize) -> Result<u8> {
    le::u8_at(d, at).map_err(|e| anyhow!("{e}"))
}

pub fn u16_at(d: &[u8], at: usize) -> Result<u16> {
    le::u16_at(d, at).map_err(|e| anyhow!("{e}"))
}

pub fn u32_at(d: &[u8], at: usize) -> Result<u32> {
    le::u32_at(d, at).map_err(|e| anyhow!("{e}"))
}

pub fn i32_at(d: &[u8], at: usize) -> Result<i32> {
    le::i32_at(d, at).map_err(|e| anyhow!("{e}"))
}

#[must_use]
pub fn us(n: u32) -> usize {
    usize::try_from(n).unwrap_or(usize::MAX)
}

pub fn u16_of(n: usize, what: &str) -> Result<u16> {
    u16::try_from(n).with_context(|| format!("{what} {n} does not fit 16 bits"))
}

pub fn u32_of(n: usize, what: &str) -> Result<u32> {
    u32::try_from(n).with_context(|| format!("{what} {n} does not fit 32 bits"))
}

pub fn name_at(d: &[u8], at: usize, g: Gen) -> Result<(&[u8], usize)> {
    match g {
        Gen::St => {
            let n = usize::from(*d.get(at).context("a name starts past its record")?);
            let start = at.saturating_add(1);
            let b = d.get(start..start.saturating_add(n)).with_context(|| {
                format!(
                    "a {n}-byte name at {at:#x} runs past its record of {} bytes",
                    d.len()
                )
            })?;
            Ok((b, start.saturating_add(n)))
        }
        Gen::Sz => {
            let rest = d.get(at..).context("a name starts past its record")?;
            let n = rest.iter().position(|b| *b == 0).with_context(|| {
                format!("the name at {at:#x} has no NUL before its record ends")
            })?;
            Ok((
                rest.get(..n).unwrap_or_default(),
                at.saturating_add(n).saturating_add(1),
            ))
        }
    }
}

pub fn name_text(d: &[u8], at: usize, g: Gen) -> Result<(String, usize)> {
    let (b, end) = name_at(d, at, g)?;
    Ok((srcio::decode(b), end))
}

pub fn push_name(out: &mut Vec<u8>, name: &[u8], g: Gen) -> Result<()> {
    match g {
        Gen::St => {
            out.push(u8::try_from(name.len()).with_context(|| {
                format!(
                    "the name {:?} is {} bytes; an ST name holds 255",
                    srcio::decode(name),
                    name.len()
                )
            })?);
            out.extend_from_slice(name);
        }
        Gen::Sz => {
            if name.contains(&0) {
                bail!(
                    "the name {:?} holds a NUL, which an SZ name cannot",
                    srcio::decode(name)
                );
            }
            out.extend_from_slice(name);
            out.push(0);
        }
    }
    Ok(())
}

pub fn numeric_len(d: &[u8], at: usize) -> Result<usize> {
    let v = u16_at(d, at)?;
    if v < LF_NUMERIC {
        return Ok(2);
    }
    let n = match v {
        0x8000 => 1,
        0x8001 | 0x8002 => 2,
        0x8003..=0x8005 => 4,
        0x800b => 6,
        0x8006 | 0x8009 | 0x800a | 0x800c | 0x801a => 8,
        0x8007 => 10,
        0x8008 | 0x800d | 0x8017..=0x8019 => 16,
        0x800e => 20,
        0x800f => 32,
        0x8010 => 2 + usize::from(u16_at(d, at.saturating_add(2))?),
        _ => bail!("numeric leaf {v:#06x} at {at:#x} is not one CodeView defines"),
    };
    let total = 2 + n;
    bytes(d, at, total).with_context(|| format!("the numeric leaf {v:#06x} at {at:#x}"))?;
    Ok(total)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Numeric {
    Int(i64),
    UInt(u64),
    Other(u16),
}

impl Numeric {
    #[must_use]
    pub fn int(self) -> i64 {
        match self {
            Self::Int(v) => v,
            Self::UInt(v) => i64::try_from(v).unwrap_or(i64::MAX),
            Self::Other(_) => 0,
        }
    }
}

pub fn numeric(d: &[u8], at: usize) -> Result<(Numeric, usize)> {
    let len = numeric_len(d, at)?;
    let v = u16_at(d, at)?;
    let p = at.saturating_add(2);
    let val = match v {
        0..0x8000 => Numeric::Int(i64::from(v)),
        0x8000 => Numeric::Int(i64::from(i8::from_le_bytes([u8_at(d, p)?]))),
        0x8001 => Numeric::Int(i64::from(i16::from_le_bytes(u16_at(d, p)?.to_le_bytes()))),
        0x8002 => Numeric::Int(i64::from(u16_at(d, p)?)),
        0x8003 => Numeric::Int(i64::from(i32_at(d, p)?)),
        0x8004 => Numeric::Int(i64::from(u32_at(d, p)?)),
        0x8009 | 0x800a => {
            let lo = u64::from(u32_at(d, p)?);
            let hi = u64::from(u32_at(d, p.saturating_add(4))?);
            let w = lo | (hi << 32);
            if v == 0x8009 {
                Numeric::Int(i64::from_le_bytes(w.to_le_bytes()))
            } else {
                Numeric::UInt(w)
            }
        }
        other => Numeric::Other(other),
    };
    Ok((val, len))
}

pub fn int_at(d: &[u8], at: usize) -> Result<(i64, usize)> {
    numeric(d, at).map(|(v, n)| (v.int(), n))
}

pub fn push_numeric(out: &mut Vec<u8>, v: i64) {
    if let Ok(s) = u16::try_from(v)
        && s < LF_NUMERIC
    {
        out.extend_from_slice(&s.to_le_bytes());
    } else if let Ok(c) = i8::try_from(v) {
        out.extend_from_slice(&0x8000u16.to_le_bytes());
        out.extend_from_slice(&c.to_le_bytes());
    } else if let Ok(s) = i16::try_from(v) {
        out.extend_from_slice(&0x8001u16.to_le_bytes());
        out.extend_from_slice(&s.to_le_bytes());
    } else if let Ok(s) = u16::try_from(v) {
        out.extend_from_slice(&0x8002u16.to_le_bytes());
        out.extend_from_slice(&s.to_le_bytes());
    } else if let Ok(s) = i32::try_from(v) {
        out.extend_from_slice(&0x8003u16.to_le_bytes());
        out.extend_from_slice(&s.to_le_bytes());
    } else if let Ok(s) = u32::try_from(v) {
        out.extend_from_slice(&0x8004u16.to_le_bytes());
        out.extend_from_slice(&s.to_le_bytes());
    } else {
        out.extend_from_slice(&0x8009u16.to_le_bytes());
        out.extend_from_slice(&v.to_le_bytes());
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Leaf {
    pub st: u16,
    pub sz: u16,
    pub fixed: usize,
    pub numerics: usize,
    pub named: bool,
    pub name: &'static str,
}

const fn leaf(
    st: u16,
    sz: u16,
    fixed: usize,
    numerics: usize,
    named: bool,
    name: &'static str,
) -> Leaf {
    Leaf {
        st,
        sz,
        fixed,
        numerics,
        named,
        name,
    }
}

pub const TYPE_LEAVES: &[Leaf] = &[
    leaf(0x000a, 0x000a, 0, 0, false, "LF_VTSHAPE"),
    leaf(0x000e, 0x000e, 0, 0, false, "LF_LABEL"),
    leaf(0x000f, 0x000f, 0, 0, false, "LF_NULL"),
    leaf(0x0010, 0x0010, 0, 0, false, "LF_NOTTRAN"),
    leaf(0x0014, 0x0014, 0, 0, false, "LF_ENDPRECOMP"),
    leaf(0x1001, 0x1001, 0, 0, false, "LF_MODIFIER"),
    leaf(0x1002, 0x1002, 0, 0, false, "LF_POINTER"),
    leaf(0x1003, 0x1503, 8, 1, true, "LF_ARRAY"),
    leaf(0x1004, 0x1504, 16, 1, true, "LF_CLASS"),
    leaf(0x1005, 0x1505, 16, 1, true, "LF_STRUCTURE"),
    leaf(0x1006, 0x1506, 8, 1, true, "LF_UNION"),
    leaf(0x1007, 0x1507, 12, 0, true, "LF_ENUM"),
    leaf(0x1008, 0x1008, 0, 0, false, "LF_PROCEDURE"),
    leaf(0x1009, 0x1009, 0, 0, false, "LF_MFUNCTION"),
    leaf(0x100a, 0x100a, 0, 0, false, "LF_COBOL0"),
    leaf(0x100c, 0x1508, 8, 0, true, "LF_DIMARRAY"),
    leaf(0x100d, 0x100d, 0, 0, false, "LF_VFTPATH"),
    leaf(0x1010, 0x150a, 4, 0, true, "LF_ALIAS"),
    leaf(0x1201, 0x1201, 0, 0, false, "LF_ARGLIST"),
    leaf(0x1202, 0x1202, 0, 0, false, "LF_DEFARG"),
    leaf(0x1204, 0x1204, 0, 0, false, "LF_DERIVED"),
    leaf(0x1205, 0x1205, 0, 0, false, "LF_BITFIELD"),
    leaf(0x1206, 0x1206, 0, 0, false, "LF_METHODLIST"),
    leaf(0x1207, 0x1207, 0, 0, false, "LF_DIMCONU"),
    leaf(0x1208, 0x1208, 0, 0, false, "LF_DIMCONLU"),
    leaf(0x1209, 0x1209, 0, 0, false, "LF_DIMVARU"),
    leaf(0x120a, 0x120a, 0, 0, false, "LF_DIMVARLU"),
    leaf(0x120b, 0x120b, 0, 0, false, "LF_REFSYM"),
];

pub const MEMBERS: &[Leaf] = &[
    leaf(0x1400, 0x1400, 6, 1, false, "LF_BCLASS"),
    leaf(0x1401, 0x1401, 10, 2, false, "LF_VBCLASS"),
    leaf(0x1402, 0x1402, 10, 2, false, "LF_IVBCLASS"),
    leaf(0x1403, 0x150c, 6, 0, true, "LF_FRIENDFCN"),
    leaf(0x1404, 0x1404, 6, 0, false, "LF_INDEX"),
    leaf(0x1405, 0x150d, 6, 1, true, "LF_MEMBER"),
    leaf(0x1406, 0x150e, 6, 0, true, "LF_STMEMBER"),
    leaf(0x1407, 0x150f, 6, 0, true, "LF_METHOD"),
    leaf(0x1408, 0x1510, 6, 0, true, "LF_NESTTYPE"),
    leaf(0x1409, 0x1409, 6, 0, false, "LF_VFUNCTAB"),
    leaf(0x140a, 0x140a, 6, 0, false, "LF_FRIENDCLS"),
    leaf(0x140b, 0x1511, 6, 0, true, "LF_ONEMETHOD"),
    leaf(0x140c, 0x140c, 10, 0, false, "LF_VFUNCOFF"),
    leaf(0x140d, 0x1512, 6, 0, true, "LF_NESTTYPEEX"),
    leaf(0x140e, 0x1513, 6, 0, true, "LF_MEMBERMODIFY"),
    leaf(0x0403, 0x1502, 2, 1, true, "LF_ENUMERATE"),
];

#[must_use]
pub fn find_leaf(table: &'static [Leaf], kind: u16, g: Gen) -> Option<&'static Leaf> {
    table.iter().find(|l| match g {
        Gen::St => l.st == kind,
        Gen::Sz => l.sz == kind,
    })
}

#[must_use]
pub fn leaf_name(kind: u16) -> &'static str {
    if kind == LF_FIELDLIST {
        return "LF_FIELDLIST";
    }
    [Gen::St, Gen::Sz]
        .iter()
        .find_map(|g| find_leaf(TYPE_LEAVES, kind, *g).or_else(|| find_leaf(MEMBERS, kind, *g)))
        .map_or("?", |l| l.name)
}

#[must_use]
pub fn is_intro(attr: u16) -> bool {
    matches!((attr >> 2) & 7, 4 | 6)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Part<'a> {
    Fixed(&'a [u8]),
    Numeric(&'a [u8]),
    Name(&'a [u8]),
    Rest(&'a [u8]),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Parsed<'a> {
    pub kind: u16,
    pub leaf: Option<&'static Leaf>,
    pub parts: Vec<Part<'a>>,
}

fn layout<'a>(
    l: &'static Leaf,
    kind: u16,
    d: &'a [u8],
    at: usize,
    g: Gen,
    member: bool,
) -> Result<(Parsed<'a>, usize)> {
    let mut p = at;
    let mut fixed = l.fixed;
    if member && l.st == LF_ONEMETHOD_ST && is_intro(u16_at(d, p)?) {
        fixed += 4;
    }
    let mut parts = vec![Part::Fixed(
        bytes(d, p, fixed).with_context(|| format!("{}: its fixed fields", l.name))?,
    )];
    p = p.saturating_add(fixed);
    for _ in 0..l.numerics {
        let n = numeric_len(d, p).with_context(|| format!("{}: its numeric field", l.name))?;
        parts.push(Part::Numeric(bytes(d, p, n)?));
        p = p.saturating_add(n);
    }
    if l.named {
        let (name, e) = name_at(d, p, g).with_context(|| format!("{}: its name", l.name))?;
        parts.push(Part::Name(name));
        p = e;
    }
    Ok((
        Parsed {
            kind,
            leaf: Some(l),
            parts,
        },
        p,
    ))
}

#[must_use]
pub fn skip_pad(d: &[u8], mut at: usize) -> usize {
    while let Some(b) = d.get(at) {
        if *b < LF_PAD0 {
            break;
        }
        at = at.saturating_add(usize::from((*b & 0x0f).max(1)));
    }
    at
}

fn all_pad(b: &[u8]) -> bool {
    b.iter().all(|x| *x == 0 || *x >= LF_PAD0)
}

pub fn parse_type(kind: u16, payload: &[u8], g: Gen) -> Result<Vec<Parsed<'_>>> {
    if kind == LF_FIELDLIST {
        let mut out = Vec::new();
        let mut p = 0usize;
        while p < payload.len() {
            p = skip_pad(payload, p);
            if p >= payload.len() {
                break;
            }
            let mk = u16_at(payload, p)?;
            let m = find_leaf(MEMBERS, mk, g).ok_or_else(|| {
                anyhow!("field-list member {mk:#06x} at {p:#x} is not one this codec knows")
            })?;
            let (parsed, e) = layout(m, mk, payload, p.saturating_add(2), g, true)?;
            out.push(parsed);
            p = e;
        }
        return Ok(out);
    }
    let Some(l) = find_leaf(TYPE_LEAVES, kind, g) else {
        return Ok(vec![Parsed {
            kind,
            leaf: None,
            parts: vec![Part::Rest(payload)],
        }]);
    };
    if !l.named && l.numerics == 0 {
        return Ok(vec![Parsed {
            kind,
            leaf: Some(l),
            parts: vec![Part::Rest(payload)],
        }]);
    }
    let (parsed, e) = layout(l, kind, payload, 0, g, false)?;
    let rest = payload.get(e..).unwrap_or_default();
    if !all_pad(rest) {
        bail!(
            "{}: {} byte(s) after its fields are not padding",
            l.name,
            rest.len()
        );
    }
    Ok(vec![parsed])
}

fn lf_pad(out: &mut Vec<u8>) {
    while !out.len().is_multiple_of(4) {
        let left = 4 - out.len() % 4;
        out.push(LF_PAD0 | u8::try_from(left).unwrap_or(0));
    }
}

fn kind_in(l: Option<&'static Leaf>, kind: u16, from: Gen, to: Gen) -> Result<u16> {
    if from == to {
        return Ok(kind);
    }
    let l = l.ok_or_else(|| {
        anyhow!(
            "leaf {kind:#06x} has no known layout, so it cannot be respelled from {from:?} to {to:?}"
        )
    })?;
    Ok(match to {
        Gen::St => l.st,
        Gen::Sz => l.sz,
    })
}

pub fn write_type(kind: u16, parsed: &[Parsed<'_>], from: Gen, to: Gen) -> Result<Vec<u8>> {
    let mut out = vec![0u8; 4];
    let k = if kind == LF_FIELDLIST {
        for m in parsed {
            out.extend_from_slice(&kind_in(m.leaf, m.kind, from, to)?.to_le_bytes());
            for part in &m.parts {
                write_part(&mut out, part, to)?;
            }
            lf_pad(&mut out);
        }
        LF_FIELDLIST
    } else {
        let one = parsed.first().context("a type record parses to one leaf")?;
        for part in &one.parts {
            write_part(&mut out, part, to)?;
        }
        lf_pad(&mut out);
        kind_in(one.leaf, kind, from, to)?
    };
    let reclen = u16_of(out.len().saturating_sub(2), "type record length")?;
    out.get_mut(..2)
        .context("record header")?
        .copy_from_slice(&reclen.to_le_bytes());
    out.get_mut(2..4)
        .context("record header")?
        .copy_from_slice(&k.to_le_bytes());
    Ok(out)
}

fn write_part(out: &mut Vec<u8>, part: &Part<'_>, to: Gen) -> Result<()> {
    match part {
        Part::Fixed(b) | Part::Numeric(b) | Part::Rest(b) => out.extend_from_slice(b),
        Part::Name(n) => push_name(out, n, to)?,
    }
    Ok(())
}

pub fn type_record(kind: u16, payload: &[u8]) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(payload.len().saturating_add(8));
    out.extend_from_slice(&[0, 0]);
    out.extend_from_slice(&kind.to_le_bytes());
    out.extend_from_slice(payload);
    lf_pad(&mut out);
    let reclen = u16_of(out.len().saturating_sub(2), "type record length")?;
    out.get_mut(..2)
        .context("record header")?
        .copy_from_slice(&reclen.to_le_bytes());
    Ok(out)
}

pub fn respell_type(rec: &[u8], from: Gen, to: Gen) -> Result<Vec<u8>> {
    let len = usize::from(u16_at(rec, 0)?);
    let kind = u16_at(rec, 2)?;
    let payload = bytes(rec, 4, len.saturating_sub(2))
        .with_context(|| format!("type record {kind:#06x} of length {len}"))?;
    let parsed = parse_type(kind, payload, from)?;
    write_type(kind, &parsed, from, to)
}

pub fn type_record_at(s: &[u8], at: usize) -> Result<(u16, &[u8], usize)> {
    let len = usize::from(u16_at(s, at).context("a type record's length")?);
    if len < 2 {
        bail!("the type record at {at:#x} has length {len}");
    }
    let kind = u16_at(s, at.saturating_add(2))?;
    let payload = bytes(s, at.saturating_add(4), len - 2).with_context(|| {
        format!("the type record at {at:#x} (leaf {kind:#06x}, length {len}) runs past its stream")
    })?;
    Ok((kind, payload, at.saturating_add(2).saturating_add(len)))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Shape {
    Verbatim,
    Named,
    Thunk,
    Compile2,
    Ref,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SymKind {
    pub st: u16,
    pub sz: u16,
    pub fixed_st: usize,
    pub fixed_sz: usize,
    pub numeric: bool,
    pub shape: Shape,
    pub name: &'static str,
}

const fn k(
    st: u16,
    sz: u16,
    fixed: usize,
    numeric: bool,
    shape: Shape,
    name: &'static str,
) -> SymKind {
    SymKind {
        st,
        sz,
        fixed_st: fixed,
        fixed_sz: fixed,
        numeric,
        shape,
        name,
    }
}

const fn reference(st: u16, sz: u16, name: &'static str) -> SymKind {
    SymKind {
        st,
        sz,
        fixed_st: 12,
        fixed_sz: 10,
        numeric: false,
        shape: Shape::Ref,
        name,
    }
}

pub const SYM_KINDS: &[SymKind] = &[
    k(0x0001, 0x0001, 0, false, Shape::Verbatim, "S_COMPILE"),
    k(0x0005, 0x0005, 0, false, Shape::Verbatim, "S_SSEARCH"),
    k(0x0006, 0x0006, 0, false, Shape::Verbatim, "S_END"),
    k(0x0007, 0x0007, 0, false, Shape::Verbatim, "S_SKIP"),
    k(0x0009, 0x1101, 4, false, Shape::Named, "S_OBJNAME"),
    k(0x000a, 0x000a, 0, false, Shape::Verbatim, "S_ENDARG"),
    k(0x000d, 0x000d, 0, false, Shape::Verbatim, "S_RETURN"),
    k(0x000e, 0x000e, 0, false, Shape::Verbatim, "S_ENTRYTHIS"),
    k(0x0206, 0x1102, 21, false, Shape::Thunk, "S_THUNK32"),
    k(0x0207, 0x1103, 18, false, Shape::Named, "S_BLOCK32"),
    k(0x0208, 0x1104, 18, false, Shape::Named, "S_WITH32"),
    k(0x0209, 0x1105, 7, false, Shape::Named, "S_LABEL32"),
    k(0x020a, 0x020a, 0, false, Shape::Verbatim, "S_CEXMODEL32"),
    k(0x0402, 0x0402, 0, false, Shape::Verbatim, "S_ALIGN"),
    k(0x1001, 0x1106, 6, false, Shape::Named, "S_REGISTER"),
    k(0x1002, 0x1107, 4, true, Shape::Named, "S_CONSTANT"),
    k(0x1003, 0x1108, 4, false, Shape::Named, "S_UDT"),
    k(0x1004, 0x1109, 4, false, Shape::Named, "S_COBOLUDT"),
    k(0x1005, 0x110a, 6, false, Shape::Named, "S_MANYREG"),
    k(0x1006, 0x110b, 8, false, Shape::Named, "S_BPREL32"),
    k(0x1007, 0x110c, 10, false, Shape::Named, "S_LDATA32"),
    k(0x1008, 0x110d, 10, false, Shape::Named, "S_GDATA32"),
    k(0x1009, 0x110e, 10, false, Shape::Named, "S_PUB32"),
    k(0x100a, 0x110f, 35, false, Shape::Named, "S_LPROC32"),
    k(0x100b, 0x1110, 35, false, Shape::Named, "S_GPROC32"),
    k(0x100c, 0x100c, 0, false, Shape::Verbatim, "S_VFTABLE32"),
    k(0x100d, 0x1111, 10, false, Shape::Named, "S_REGREL32"),
    k(0x100e, 0x1112, 10, false, Shape::Named, "S_LTHREAD32"),
    k(0x100f, 0x1113, 10, false, Shape::Named, "S_GTHREAD32"),
    k(0x1012, 0x1012, 0, false, Shape::Verbatim, "S_FRAMEPROC"),
    k(0x1013, 0x1116, 18, false, Shape::Compile2, "S_COMPILE2"),
    reference(S_PROCREF_ST, 0x1125, "S_PROCREF"),
    reference(S_DATAREF_ST, 0x1126, "S_DATAREF"),
    reference(S_LPROCREF_ST, 0x1127, "S_LPROCREF"),
];

#[must_use]
pub fn sym_kind(kind: u16, g: Gen) -> Option<&'static SymKind> {
    SYM_KINDS.iter().find(|s| match g {
        Gen::St => s.st == kind,
        Gen::Sz => s.sz == kind,
    })
}

#[must_use]
pub fn sym_name(kind: u16) -> &'static str {
    sym_kind(kind, Gen::St)
        .or_else(|| sym_kind(kind, Gen::Sz))
        .map_or("?", |s| s.name)
}

#[must_use]
pub fn pointer_fields(kind: u16, g: Gen) -> &'static [usize] {
    match sym_kind(kind, g).map(|s| s.st) {
        Some(0x100a | 0x100b | 0x0206) => &[0, 4, 8],
        Some(0x0207 | 0x0208) => &[0, 4],
        _ => &[],
    }
}

#[must_use]
pub fn short_ref(region: &[u8], at: usize, kind: u16, reclen: usize) -> Option<usize> {
    if matches!(kind, S_PROCREF_ST | S_DATAREF_ST | S_LPROCREF_ST) && reclen == 14 {
        let n = usize::from(*region.get(at.saturating_add(16))?);
        Some(at.saturating_add(17).saturating_add(n))
    } else {
        None
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SymRecord<'a> {
    pub at: usize,
    pub kind: u16,
    pub payload: &'a [u8],
    pub end: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Walk {
    pub aligned: bool,
    pub short_refs: bool,
}

pub const NB_SYMBOLS: Walk = Walk {
    aligned: false,
    short_refs: false,
};

pub const PDB20_SYMBOLS: Walk = Walk {
    aligned: true,
    short_refs: true,
};

pub fn walk(region: &[u8], how: Walk) -> Result<Vec<SymRecord<'_>>> {
    let mut out = Vec::new();
    let mut at = 0usize;
    while at < region.len() {
        let rest = region.get(at..).unwrap_or_default();
        if rest.len() < 4 || u16_at(region, at)? == 0 {
            if rest.iter().all(|b| *b == 0) {
                break;
            }
            bail!("the symbol record at {at:#x} has no length and what follows is not zero");
        }
        let len = usize::from(u16_at(region, at)?);
        let kind = u16_at(region, at.saturating_add(2))?;
        if len < 2 {
            bail!("the symbol record at {at:#x} has length {len}");
        }
        let end = if how.short_refs {
            short_ref(region, at, kind, len)
        } else {
            None
        }
        .unwrap_or(at.saturating_add(2).saturating_add(len));
        if end > region.len() {
            bail!(
                "the symbol record at {at:#x} ({}, length {len}) runs past its {} byte(s)",
                sym_name(kind),
                region.len()
            );
        }
        out.push(SymRecord {
            at,
            kind,
            payload: region.get(at.saturating_add(4)..end).unwrap_or_default(),
            end,
        });
        let next = if how.aligned {
            end.next_multiple_of(4).min(region.len())
        } else {
            end
        };
        if !region
            .get(end..next)
            .unwrap_or_default()
            .iter()
            .all(|b| *b == 0)
        {
            bail!(
                "the bytes between the symbol record ending at {end:#x} and the next are not zero"
            );
        }
        at = next;
    }
    Ok(out)
}

pub fn sym_record(kind: u16, payload: &[u8]) -> Result<Vec<u8>> {
    let total = payload.len().saturating_add(4).next_multiple_of(4);
    let mut out = Vec::with_capacity(total);
    out.extend_from_slice(&u16_of(total - 2, "symbol record length")?.to_le_bytes());
    out.extend_from_slice(&kind.to_le_bytes());
    out.extend_from_slice(payload);
    out.resize(total, 0);
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SymParts<'a> {
    pub kind: &'static SymKind,
    pub generation: Gen,
    pub fixed: &'a [u8],
    pub numeric: Option<&'a [u8]>,
    pub name: Option<&'a [u8]>,
    pub tail: Vec<SymTail<'a>>,
    pub pad: &'a [u8],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SymTail<'a> {
    Bytes(&'a [u8]),
    Name(&'a [u8]),
    Strings(Vec<&'a [u8]>),
}

pub fn parse_sym(kind: u16, payload: &[u8], g: Gen) -> Result<SymParts<'_>> {
    let sk = sym_kind(kind, g).ok_or_else(|| {
        anyhow!(
            "symbol kind {kind:#06x} ({} bytes) is not one this codec knows",
            payload.len()
        )
    })?;
    if sk.shape == Shape::Verbatim {
        return Ok(SymParts {
            kind: sk,
            generation: g,
            fixed: payload,
            numeric: None,
            name: None,
            tail: Vec::new(),
            pad: &[],
        });
    }
    let fixed_len = match g {
        Gen::St => sk.fixed_st,
        Gen::Sz => sk.fixed_sz,
    };
    let fixed = bytes(payload, 0, fixed_len).with_context(|| format!("{}: its fields", sk.name))?;
    let mut at = fixed_len;
    let numeric = if sk.numeric {
        let n = numeric_len(payload, at)?;
        let b = bytes(payload, at, n)?;
        at = at.saturating_add(n);
        Some(b)
    } else {
        None
    };
    let nameless =
        sk.shape == Shape::Ref && payload.get(at..).is_none_or(|r| r.iter().all(|b| *b == 0));
    let (name, end) = if nameless {
        (None, payload.len())
    } else {
        let (n, e) = name_at(payload, at, g).with_context(|| format!("{}: its name", sk.name))?;
        (Some(n), e)
    };
    at = end;
    let mut tail = Vec::new();
    match sk.shape {
        Shape::Thunk => match fixed.get(20).copied().unwrap_or(0) {
            0 | 4 => {}
            1 => {
                tail.push(SymTail::Bytes(bytes(payload, at, 2)?));
                let (t, e) = name_at(payload, at.saturating_add(2), g)?;
                tail.push(SymTail::Name(t));
                at = e;
            }
            2 => {
                tail.push(SymTail::Bytes(bytes(payload, at, 2)?));
                at = at.saturating_add(2);
            }
            ord => bail!("S_THUNK32 ordinal {ord} is not one this codec knows"),
        },
        Shape::Compile2 => {
            let mut strings = Vec::new();
            while at < payload.len() {
                let (s, e) = name_at(payload, at, Gen::Sz)?;
                at = e;
                if s.is_empty() {
                    break;
                }
                strings.push(s);
            }
            tail.push(SymTail::Strings(strings));
            return Ok(SymParts {
                kind: sk,
                generation: g,
                fixed,
                numeric,
                name,
                tail,
                pad: payload.get(at..).unwrap_or_default(),
            });
        }
        _ => {}
    }
    let pad = payload.get(at..).unwrap_or_default();
    if !pad.iter().all(|b| *b == 0) {
        bail!(
            "{} {:?}: {} byte(s) after its fields are not padding",
            sk.name,
            srcio::decode(name.unwrap_or_default()),
            pad.len()
        );
    }
    Ok(SymParts {
        kind: sk,
        generation: g,
        fixed,
        numeric,
        name,
        tail,
        pad,
    })
}

pub fn write_sym(p: &SymParts<'_>, to: Gen, keep_pad: bool) -> Result<(u16, Vec<u8>)> {
    let kind = match to {
        Gen::St => p.kind.st,
        Gen::Sz => p.kind.sz,
    };
    let mut out = Vec::with_capacity(p.fixed.len().saturating_add(32));
    if p.kind.shape == Shape::Ref && p.generation != to {
        let head = bytes(p.fixed, 0, 10)?;
        out.extend_from_slice(head);
        if to == Gen::St {
            out.extend_from_slice(&[0, 0]);
        }
    } else {
        out.extend_from_slice(p.fixed);
    }
    if let Some(n) = p.numeric {
        out.extend_from_slice(n);
    }
    match p.name {
        Some(n) => push_name(&mut out, n, to)?,
        None if p.kind.shape == Shape::Ref && to == Gen::Sz => out.push(0),
        None => {}
    }
    for t in &p.tail {
        match t {
            SymTail::Bytes(b) => out.extend_from_slice(b),
            SymTail::Name(n) => push_name(&mut out, n, to)?,
            SymTail::Strings(ss) => {
                for s in ss {
                    push_name(&mut out, s, Gen::Sz)?;
                }
                out.push(0);
            }
        }
    }
    if keep_pad {
        out.extend_from_slice(p.pad);
    }
    Ok((kind, out))
}

pub fn respell_sym(kind: u16, payload: &[u8], from: Gen, to: Gen) -> Result<(u16, Vec<u8>)> {
    let p = parse_sym(kind, payload, from)?;
    if p.kind.shape == Shape::Ref && from == Gen::St && u16_at(p.fixed, 10)? != 0 {
        bail!(
            "{} record with a nonzero fill word {:#06x}: VC6 writes 0 there",
            p.kind.name,
            u16_at(p.fixed, 10)?
        );
    }
    write_sym(&p, to, false)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_read_and_write_in_both_generations() {
        let mut st = Vec::new();
        push_name(&mut st, b"Dev", Gen::St).unwrap();
        assert_eq!(st, b"\x03Dev");
        assert_eq!(name_at(&st, 0, Gen::St).unwrap(), (&b"Dev"[..], 4));
        let mut sz = Vec::new();
        push_name(&mut sz, b"Dev", Gen::Sz).unwrap();
        assert_eq!(sz, b"Dev\0");
        assert_eq!(name_at(&sz, 0, Gen::Sz).unwrap(), (&b"Dev"[..], 4));
        assert!(name_at(b"\x09ab", 0, Gen::St).is_err());
        assert!(name_at(b"ab", 0, Gen::Sz).is_err());
        assert!(push_name(&mut Vec::new(), &[b'a'; 256], Gen::St).is_err());
        assert!(push_name(&mut Vec::new(), b"a\0b", Gen::Sz).is_err());
    }

    #[test]
    fn numerics_decode_and_encode_the_smallest_leaf() {
        for v in [
            0i64,
            0x7fff,
            -1,
            -129,
            0x8000,
            0xffff,
            -40000,
            0x1_0000,
            0xffff_ffff,
            -1 << 40,
        ] {
            let mut b = Vec::new();
            push_numeric(&mut b, v);
            let (got, n) = int_at(&b, 0).unwrap();
            assert_eq!((got, n), (v, b.len()), "{v}");
        }
        assert_eq!(
            numeric_len(&[0x10, 0x80, 3, 0, b'a', b'b', b'c'], 0).unwrap(),
            7
        );
        assert!(numeric_len(&[0x10, 0x80, 9, 0, b'a'], 0).is_err());
        assert!(numeric_len(&[0x99, 0x80], 0).is_err());
        assert_eq!(
            numeric(&[0x05, 0x80, 0, 0, 0x80, 0x3f], 0).unwrap().0,
            Numeric::Other(0x8005)
        );
    }

    #[test]
    fn a_class_record_respells_to_sz_and_back() {
        let mut p = Vec::new();
        p.extend_from_slice(&2u16.to_le_bytes());
        p.extend_from_slice(&0u16.to_le_bytes());
        p.extend_from_slice(&0x1003u32.to_le_bytes());
        p.extend_from_slice(&[0; 8]);
        push_numeric(&mut p, 8);
        push_name(&mut p, b"Dev", Gen::St).unwrap();
        let st = type_record(0x1004, &p).unwrap();
        assert!(st.len().is_multiple_of(4));
        let sz = respell_type(&st, Gen::St, Gen::Sz).unwrap();
        assert_eq!(u16_at(&sz, 2).unwrap(), 0x1504);
        assert_eq!(respell_type(&sz, Gen::Sz, Gen::St).unwrap(), st);
        assert_eq!(respell_type(&st, Gen::St, Gen::St).unwrap(), st);
    }

    #[test]
    fn a_field_list_respells_member_by_member() {
        let mut p = Vec::new();
        p.extend_from_slice(&0x1405u16.to_le_bytes());
        p.extend_from_slice(&1u16.to_le_bytes());
        p.extend_from_slice(&0x22u32.to_le_bytes());
        push_numeric(&mut p, 4);
        push_name(&mut p, b"m_flags", Gen::St).unwrap();
        lf_pad(&mut p);
        p.extend_from_slice(&0x140bu16.to_le_bytes());
        p.extend_from_slice(&(3u16 | (4 << 2)).to_le_bytes());
        p.extend_from_slice(&0x1004u32.to_le_bytes());
        p.extend_from_slice(&0u32.to_le_bytes());
        push_name(&mut p, b"set", Gen::St).unwrap();
        lf_pad(&mut p);
        let st = type_record(LF_FIELDLIST, &p).unwrap();
        let parsed = parse_type(LF_FIELDLIST, &p, Gen::St).unwrap();
        assert_eq!(parsed.len(), 2);
        assert_eq!(
            write_type(LF_FIELDLIST, &parsed, Gen::St, Gen::St).unwrap(),
            st
        );
        let sz = respell_type(&st, Gen::St, Gen::Sz).unwrap();
        assert_eq!(respell_type(&sz, Gen::Sz, Gen::St).unwrap(), st);
    }

    #[test]
    fn symbols_walk_respell_and_honour_the_short_reference_length() {
        let mut pub32 = Vec::new();
        pub32.extend_from_slice(&0u32.to_le_bytes());
        pub32.extend_from_slice(&0x10u32.to_le_bytes());
        pub32.extend_from_slice(&1u16.to_le_bytes());
        push_name(&mut pub32, b"_main", Gen::St).unwrap();
        let rec = sym_record(0x1009, &pub32).unwrap();
        let mut region = rec.clone();
        let mut r = Vec::new();
        r.extend_from_slice(&14u16.to_le_bytes());
        r.extend_from_slice(&S_PROCREF_ST.to_le_bytes());
        r.extend_from_slice(&[0; 12]);
        push_name(&mut r, b"_f", Gen::St).unwrap();
        region.extend_from_slice(&r);
        region.resize(region.len().next_multiple_of(4), 0);
        region.extend_from_slice(&rec);
        let recs = walk(&region, PDB20_SYMBOLS).unwrap();
        assert_eq!(
            recs.iter().map(|r| r.kind).collect::<Vec<_>>(),
            [0x1009, S_PROCREF_ST, 0x1009]
        );
        let (k70, p70) = respell_sym(0x1009, recs[0].payload, Gen::St, Gen::Sz).unwrap();
        assert_eq!(k70, 0x110e);
        let (k6, p6) = respell_sym(k70, &p70, Gen::Sz, Gen::St).unwrap();
        assert_eq!(sym_record(k6, &p6).unwrap(), rec);
        let (rk, rp) = respell_sym(S_PROCREF_ST, recs[1].payload, Gen::St, Gen::Sz).unwrap();
        assert_eq!((rk, rp.len()), (0x1125, 13));
        assert!(walk(&[8, 0, 9, 0x10, 0], NB_SYMBOLS).is_err());
        assert!(walk(&[1, 0, 9, 0x10], NB_SYMBOLS).is_err());
    }
}
