use anyhow::{Context as _, Result, bail};
use msvc6_image::pe::image::Image;
use std::collections::VecDeque;
use std::path::Path;

pub const MAX_IMAGE: u64 = 64 << 20;
pub const MAX_CODES: usize = 16;
pub const MAX_SYM_CODES: usize = 64;
pub const MAX_PRAGMAS: usize = 256;
pub const MAX_OPS: usize = 0x100;
pub const SYM_ENTRIES: usize = 0x20;
pub const SYM_TABLE_MIN: usize = 0x13;

pub const OP_SYM: usize = 0x26;
pub const OP_LABEL: usize = 0x29;
pub const OP_CONST: usize = 0x33;
pub const OP_CALL: usize = 0x3e;
pub const OP_PRAGMA: usize = 0x4f;
pub const OP_ENDSCOPE: usize = 0x54;

pub const FMT_NONE: u8 = 0;
pub const FMT_TYPE: u8 = 1;
pub const FMT_ID: u8 = 2;
pub const FMT_TYPE_BYTE: u8 = 5;
pub const FMT_TYPE_BYTE9: u8 = 9;
pub const FMT_CONST: u8 = 6;
pub const FMT_ID8: u8 = 8;
pub const FMT_PRAGMA: u8 = 0x0c;
pub const FMT_LONG: u8 = 0x0d;
pub const FMT_LONG11: u8 = 0x11;
pub const FMT_MAX: u8 = 0x1f;
pub const FMT_WINDOW: u32 = 0x400;

pub const ENDS: u16 = 0x1000;
pub const ENDING_OPS: [usize; 5] = [0x38, 0x39, 0x3a, 0x41, 0x4b];
pub const FLOWING_OPS: [usize; 4] = [0x02, 0x26, 0x30, 0x33];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Pragma {
    pub codes: Option<Vec<u8>>,
    pub flag: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Found {
    pub attr: u32,
    pub fmt: u32,
    pub pragma: u32,
}

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Grammar {
    pub fmt: Vec<u8>,
    pub attr: Vec<u16>,
    pub pragma: Vec<Pragma>,
    pub found: Option<Found>,
    pub source: String,
}

pub type WriterOp = (u16, Option<Vec<u8>>);

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Writer {
    pub ops: Vec<WriterOp>,
    pub syms: Vec<Vec<u8>>,
    pub ops_at: u32,
    pub syms_at: Option<u32>,
    pub sym_reads: usize,
}

impl Grammar {
    #[must_use]
    pub fn from_parts(fmt: Vec<u8>, attr: Vec<u16>, pragma: Vec<Pragma>) -> Self {
        Self {
            fmt,
            attr,
            pragma,
            found: None,
            source: "built in code".to_string(),
        }
    }

    #[must_use]
    pub fn fmt(&self, op: u8) -> Option<u8> {
        self.fmt.get(usize::from(op)).copied()
    }

    #[must_use]
    pub fn attr(&self, op: u8) -> u16 {
        self.attr.get(usize::from(op)).copied().unwrap_or(0)
    }

    #[must_use]
    pub fn ends_statement(&self, op: u8) -> bool {
        op == 0x4b || self.attr(op) & ENDS != 0
    }

    #[must_use]
    pub fn pragma(&self, sub: i16) -> Option<&Pragma> {
        let [low, _] = sub.to_le_bytes();
        let idx = usize::try_from(i8::from_le_bytes([low])).ok()?;
        self.pragma.get(idx)
    }

    pub fn load(path: &Path) -> Result<Self> {
        let image = read_image(path)?;
        let mut g = locate(&image).with_context(|| path.display().to_string())?;
        g.source = path.display().to_string();
        Ok(g)
    }
}

pub fn read_image(path: &Path) -> Result<Image> {
    let bytes = crate::file::read(path, MAX_IMAGE)?;
    Image::parse(bytes).with_context(|| path.display().to_string())
}

struct Space<'a> {
    base: u32,
    segs: Vec<(u32, &'a [u8])>,
}

impl<'a> Space<'a> {
    fn of(img: &'a Image) -> Self {
        let segs = img
            .sections
            .iter()
            .filter(|s| !s.is_code() && s.raw_size.get() > 0)
            .map(|s| (s.rva.get(), img.section_bytes(s)))
            .collect();
        Self {
            base: img.opt.image_base.get(),
            segs,
        }
    }

    fn at(&self, va: u32, len: usize) -> Option<&'a [u8]> {
        let rva = va.checked_sub(self.base)?;
        self.segs.iter().find_map(|(start, bytes)| {
            let d = usize::try_from(rva.checked_sub(*start)?).ok()?;
            bytes.get(d..d.checked_add(len)?)
        })
    }

    fn before(&self, end: u32, max: u32) -> Option<(u32, &'a [u8])> {
        let rva = end.checked_sub(self.base)?;
        self.segs.iter().find_map(|(start, bytes)| {
            let d = rva.checked_sub(*start)?;
            let len = u32::try_from(bytes.len()).ok()?;
            if d == 0 || d > len {
                return None;
            }
            let from = d.saturating_sub(max);
            let s = bytes.get(usize::try_from(from).ok()?..usize::try_from(d).ok()?)?;
            Some((self.base.checked_add(*start)?.checked_add(from)?, s))
        })
    }

    fn rest(&self, va: u32) -> Option<&'a [u8]> {
        let rva = va.checked_sub(self.base)?;
        self.segs.iter().find_map(|(start, bytes)| {
            let d = usize::try_from(rva.checked_sub(*start)?).ok()?;
            bytes.get(d..)
        })
    }

    fn u16(&self, va: u32) -> Option<u16> {
        let b = self.at(va, 2)?;
        Some(u16::from_le_bytes(<[u8; 2]>::try_from(b).ok()?))
    }

    fn u32(&self, va: u32) -> Option<u32> {
        let b = self.at(va, 4)?;
        Some(u32::from_le_bytes(<[u8; 4]>::try_from(b).ok()?))
    }

    fn codes(&self, va: u32) -> Option<Vec<u8>> {
        self.codes_upto(va, MAX_CODES)
    }

    fn codes_upto(&self, va: u32, max: usize) -> Option<Vec<u8>> {
        let rest = self.rest(va)?;
        let n = rest.iter().take(max).position(|&b| b == 0)?;
        let s = rest.get(..n)?;
        (s.iter().all(|&b| (0x01..=0x7f).contains(&b))).then(|| s.to_vec())
    }

    fn vas(&self) -> impl Iterator<Item = (u32, usize)> + '_ {
        self.segs.iter().flat_map(move |(start, bytes)| {
            let va = self.base.saturating_add(*start);
            (0..bytes.len().saturating_sub(7))
                .step_by(4)
                .filter_map(move |d| Some((va.checked_add(u32::try_from(d).ok()?)?, d)))
        })
    }
}

fn pragma_entry(sp: &Space<'_>, va: u32) -> Option<Pragma> {
    let ptr = sp.u32(va)?;
    let flag = sp.u32(va.checked_add(4)?)?;
    if flag > 1 {
        return None;
    }
    if ptr == 0 {
        return Some(Pragma { codes: None, flag });
    }
    Some(Pragma {
        codes: Some(sp.codes(ptr)?),
        flag,
    })
}

fn pragma_table(sp: &Space<'_>) -> Option<(u32, Vec<Pragma>)> {
    for (va, _) in sp.vas() {
        let one = pragma_entry(sp, va.checked_add(8)?);
        let two = pragma_entry(sp, va.checked_add(16)?);
        let (Some(one), Some(two)) = (one, two) else {
            continue;
        };
        if one.codes.as_deref() != Some(b"l") || two.codes.as_deref() != Some(b"s") {
            continue;
        }
        let mut v = Vec::new();
        while v.len() < MAX_PRAGMAS {
            let at = va.checked_add(u32::try_from(v.len().checked_mul(8)?).ok()?)?;
            match pragma_entry(sp, at) {
                Some(p) => v.push(p),
                None => break,
            }
        }
        return Some((va, v));
    }
    None
}

fn code_strings_start(sp: &Space<'_>, table: u32, n: usize) -> Option<u32> {
    let near = table.saturating_sub(u32::try_from(MAX_OPS).ok()?);
    (0..n)
        .filter_map(|i| sp.u32(table.checked_add(u32::try_from(i.checked_mul(8)?).ok()?)?))
        .filter(|p| *p != 0 && *p >= near && *p < table)
        .min()
}

fn fmt_fits(f: &[u8]) -> bool {
    let at = |i: usize| f.get(i).copied();
    f.iter().all(|&b| b <= FMT_MAX)
        && at(OP_PRAGMA) == Some(FMT_PRAGMA)
        && at(OP_CONST) == Some(FMT_CONST)
        && matches!(at(OP_SYM), Some(FMT_ID | FMT_ID8))
        && matches!(at(OP_LABEL), Some(FMT_ID | FMT_ID8))
        && matches!(at(OP_CALL), Some(FMT_TYPE_BYTE | FMT_TYPE_BYTE9))
        && matches!(at(OP_ENDSCOPE), Some(FMT_LONG | FMT_LONG11))
}

fn fmt_table(sp: &Space<'_>, end: u32) -> Option<(u32, Vec<u8>)> {
    let (lo, window) = sp.before(end, FMT_WINDOW)?;
    let mut fits = Vec::new();
    for (i, &b) in window.iter().enumerate() {
        if b != FMT_PRAGMA {
            continue;
        }
        let Some(f) = i.checked_sub(OP_PRAGMA).and_then(|s| window.get(s..)) else {
            continue;
        };
        let run = f.iter().take_while(|&&x| x <= FMT_MAX).count();
        let Some(table) = f.get(..run).filter(|t| t.len() > OP_ENDSCOPE) else {
            continue;
        };
        if fmt_fits(table) {
            let at = lo.checked_add(u32::try_from(i.checked_sub(OP_PRAGMA)?).ok()?)?;
            fits.push((at, table.to_vec()));
        }
    }
    fits.into_iter().max_by_key(|(at, _)| *at)
}

fn attr_fits(a: &[u16]) -> bool {
    let at = |i: usize| a.get(i).copied();
    ENDING_OPS
        .iter()
        .all(|&o| at(o).is_some_and(|w| w & ENDS != 0))
        && FLOWING_OPS
            .iter()
            .all(|&o| at(o).is_some_and(|w| w & ENDS == 0))
}

fn attr_table(sp: &Space<'_>, fmt_at: u32, ops: usize) -> Option<(u32, Vec<u16>)> {
    let mut fits = Vec::new();
    for n in ops.saturating_sub(8)..=ops.saturating_add(16) {
        let bytes = u32::try_from(n.checked_mul(2)?).ok()?;
        let Some(start) = fmt_at.checked_sub(bytes) else {
            continue;
        };
        let Some(raw) = sp.at(start, n.checked_mul(2)?) else {
            continue;
        };
        let a: Vec<u16> = raw
            .as_chunks::<2>()
            .0
            .iter()
            .map(|c| u16::from_le_bytes(*c))
            .collect();
        if attr_fits(&a) {
            fits.push((start, a));
        }
    }
    let target = ops.saturating_add(2);
    fits.into_iter()
        .min_by_key(|(_, a)| a.len().abs_diff(target))
}

pub fn locate(img: &Image) -> Result<Grammar> {
    let sp = Space::of(img);
    let Some((pragma_at, pragma)) = pragma_table(&sp) else {
        bail!(
            "no IL grammar here: no table of 0x4f sub-op codes whose entries 1 and 2 are \"l\" and \"s\" (is this C2.DLL?)"
        );
    };
    let Some(strings) = code_strings_start(&sp, pragma_at, pragma.len()) else {
        bail!("the 0x4f sub-op table at {pragma_at:#x} points at no code string");
    };
    let Some((fmt_at, fmt)) = fmt_table(&sp, strings) else {
        bail!(
            "no operand format table ends below {strings:#x} (the sub-op codes): none fits ops 0x26 sym, 0x29 label, 0x33 const, 0x3e call, 0x4f pragma and 0x54 endscope"
        );
    };
    let Some((attr_at, attr)) = attr_table(&sp, fmt_at, fmt.len()) else {
        bail!(
            "no op attribute table ends at {fmt_at:#x}: none whose ops 0x38, 0x39, 0x3a, 0x41 and 0x4b end a statement and 0x02, 0x26, 0x30 and 0x33 do not"
        );
    };
    Ok(Grammar {
        fmt,
        attr,
        pragma,
        found: Some(Found {
            attr: attr_at,
            fmt: fmt_at,
            pragma: pragma_at,
        }),
        source: String::new(),
    })
}

fn writer_entry(sp: &Space<'_>, va: u32) -> Option<(u16, Option<Vec<u8>>)> {
    let attr = sp.u16(va)?;
    let pad = sp.u16(va.checked_add(2)?)?;
    let ptr = sp.u32(va.checked_add(4)?)?;
    if pad != 0 {
        return None;
    }
    if ptr == 0 {
        return Some((attr, None));
    }
    Some((attr, Some(sp.codes(ptr)?)))
}

fn writer_ops(sp: &Space<'_>, c2: &Grammar) -> Option<(u32, Vec<WriterOp>)> {
    let probe = |va: u32, k: usize| -> Option<bool> {
        let e = writer_entry(sp, va.checked_add(u32::try_from(k.checked_mul(8)?).ok()?)?)?;
        Some(e.0 == c2.attr.get(k)? & 0x7fff)
    };
    for (va, _) in sp.vas() {
        if !(0..8).all(|k| probe(va, k) == Some(true)) {
            continue;
        }
        let mut v = Vec::new();
        while v.len() < MAX_OPS {
            let at = va.checked_add(u32::try_from(v.len().checked_mul(8)?).ok()?)?;
            match writer_entry(sp, at) {
                Some(e) => v.push(e),
                None => break,
            }
        }
        return Some((va, v));
    }
    None
}

fn sym_entry(sp: &Space<'_>, va: u32) -> Option<Option<Vec<u8>>> {
    let ptr = sp.u32(va)?;
    let flag = sp.u32(va.checked_add(4)?)?;
    if flag > 2 {
        return None;
    }
    if ptr == 0 {
        return Some(None);
    }
    sp.codes_upto(ptr, MAX_SYM_CODES).map(Some)
}

type SymFound = Option<(u32, Vec<Vec<u8>>)>;

fn writer_syms(sp: &Space<'_>, fields: &[(u8, usize)]) -> (SymFound, usize) {
    let len_at = |va: u32| sym_entry(sp, va).map(|e| e.map_or(0, |c| c.len()));
    let mut windows: [VecDeque<(u32, Option<usize>)>; 2] = [VecDeque::new(), VecDeque::new()];
    let mut reads = 0usize;
    for (va, _) in sp.vas() {
        let Some(w) = windows.get_mut(usize::from(va & 4 != 0)) else {
            continue;
        };
        while w.front().is_some_and(|(a, _)| *a < va) {
            w.pop_front();
        }
        if w.front().is_some_and(|(a, _)| *a != va) {
            w.clear();
        }
        while w.len() < SYM_ENTRIES {
            let next = match w.back() {
                None => Some(va),
                Some((_, None)) => None,
                Some((a, Some(_))) => a.checked_add(8),
            };
            let Some(at) = next else {
                break;
            };
            reads = reads.saturating_add(1);
            w.push_back((at, len_at(at)));
        }
        let run = w.iter().take_while(|(_, l)| l.is_some()).count();
        let agrees = fields.iter().all(|(class, n)| {
            usize::from(*class) < run
                && w.get(usize::from(*class))
                    .is_some_and(|(_, l)| *l == Some(*n))
        });
        if run >= SYM_TABLE_MIN && agrees {
            let v = w
                .iter()
                .take(run)
                .map(|(a, _)| sym_entry(sp, *a).flatten().unwrap_or_default())
                .collect();
            return (Some((va, v)), reads);
        }
    }
    (None, reads)
}

pub fn writer(img: &Image, c2: &Grammar, fields: &[(u8, usize)]) -> Result<Writer> {
    let sp = Space::of(img);
    let Some((ops_at, ops)) = writer_ops(&sp, c2) else {
        bail!(
            "no op writer table whose first eight attribute words are C2's (is this C1XX.DLL or C1.DLL?)"
        );
    };
    let (syms, sym_reads) = writer_syms(&sp, fields);
    Ok(Writer {
        ops,
        syms_at: syms.as_ref().map(|(a, _)| *a),
        syms: syms.map(|(_, s)| s).unwrap_or_default(),
        sym_reads,
        ops_at,
    })
}

#[must_use]
pub fn fmt_desc(f: u8) -> &'static str {
    match f {
        0 | 0x10 => "-",
        1 => "type",
        2 => "id",
        3 => "id type byte byte",
        4 => "id byte",
        5 => "type byte",
        6 => "type value (long; quad if size 8; 12-byte real if type 0x400a)",
        7 => "type id",
        8 => "id",
        9 => "type byte",
        10 => "byte",
        0xb => "(C2 ICEs)",
        0xc => "short sub-op, then the sub-op's codes",
        0xd | 0x11 => "long",
        0xe => "type id id",
        0xf => "short",
        0x12 => "type id(raw)",
        0x13 => "type long",
        0x14 => "long long",
        0x15 => "id id short",
        _ => "(C2 fatal-errors)",
    }
}

#[must_use]
pub fn fmt_tokens(f: u8) -> &'static str {
    match f {
        0 | 0x10 => "",
        1 => "T",
        2 | 8 => "I",
        3 => "ITBB",
        4 => "IB",
        5 | 9 => "TB",
        6 => "TV",
        7 | 0x12 => "TI",
        10 => "B",
        0x0c => "P",
        0x0d | 0x11 => "L",
        0x0e => "TII",
        0x0f => "S",
        0x13 => "TL",
        0x14 => "LL",
        0x15 => "IIS",
        _ => "!",
    }
}

#[must_use]
pub fn writer_tokens(codes: &[u8]) -> String {
    let mut s = String::new();
    for &c in codes {
        s.push_str(match c {
            0x06 => "T",
            0x08 | 0x11 => "I",
            0x01 | 0x03 | 0x07 | 0x0d | 0x0e => "B",
            0x02 | 0x14 | 0x16 => "L",
            0x0b => "TV",
            0x15 => "IIS",
            _ => "",
        });
    }
    s
}

#[must_use]
pub fn pragma_code_desc(c: u8) -> &'static str {
    match c {
        0x02 => "short (+ the current line base)",
        0x0b => "an object-file comment: kind byte, then a string (kind 7: a counted blob)",
        0x0c => "lstr",
        0x0e => "short",
        0x14 => "long",
        0x15 => "short",
        0x16 => "id (resets C2's next-id counter)",
        0x17 => "(short warning, byte state) pairs up to the short -1",
        0x18 => "id",
        b'l' => "line (short; long in compact-id mode)",
        b's' => "id of a source file",
        _ => "(C2 ICEs)",
    }
}

#[must_use]
pub fn writer_code_desc(c: u8) -> &'static str {
    match c {
        0x01 => "byte: operand is a bit-field",
        0x02 => "long",
        0x03 => "byte: a call's convention class",
        0x06 => "type",
        0x07 | 0x0d | 0x0e => "byte 0",
        0x08 => "id of the operand's symbol",
        0x0b => "type and constant value",
        0x11 => "long or id",
        0x14 => "short-valued long",
        0x15 => "id id short",
        0x16 => "a writer-computed long",
        0x1b => "compact-id mode only",
        _ => "(no bytes: the writer ignores it)",
    }
}
