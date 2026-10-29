use crate::be;
use serde::Serialize;

pub const MAX_CONTROLLED: usize = 64;

pub const MAX_NAME: usize = 1024;

pub const FIXED_PART: usize = 12;

pub const HAS_TB_OFFSET: u8 = 0x20;

pub const HAS_CONTROLLED: u8 = 0x08;

pub const INTERRUPT_HANDLER: u8 = 0x80;

pub const NAME_PRESENT: u8 = 0x40;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Table {
    pub at: u32,
    pub start: u32,
    pub end: u32,
    pub name: String,
    pub lang: u8,
    pub flags1: u8,
    pub flags2: u8,
    pub fixed_parms: u8,
    pub float_parms: u8,
    pub tb_offset: Option<u32>,
}

impl Table {
    #[must_use]
    pub fn function(&self) -> &str {
        self.name.strip_prefix('.').unwrap_or(&self.name)
    }

    #[must_use]
    pub fn len(&self) -> u32 {
        self.at.saturating_sub(self.start)
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

fn printable(name: &[u8]) -> bool {
    name.iter().all(|c| (0x20..0x7f).contains(c))
}

fn read_at(code: &[u8], o: usize, prev_end: u32) -> Option<(Table, usize)> {
    let byte = |k: usize| code.get(o.checked_add(k)?).copied();
    let (version, lang) = (byte(4)?, byte(5)?);
    let (flags1, flags2) = (byte(6)?, byte(7)?);
    let (fixed, float) = (byte(10)?, byte(11)? >> 1);
    if version != 0 || flags2 & NAME_PRESENT == 0 {
        return None;
    }
    let mut p = o.checked_add(FIXED_PART)?;
    if fixed != 0 || float != 0 {
        p = p.checked_add(4)?;
    }
    let mut tb_offset = None;
    if flags1 & HAS_TB_OFFSET != 0 {
        tb_offset = Some(be::u32_at(code, p)?);
        p = p.checked_add(4)?;
    }
    if flags2 & INTERRUPT_HANDLER != 0 {
        p = p.checked_add(4)?;
    }
    if flags1 & HAS_CONTROLLED != 0 {
        let n = usize::try_from(be::u32_at(code, p)?)
            .unwrap_or(usize::MAX)
            .min(MAX_CONTROLLED);
        p = p.checked_add(4)?.checked_add(n.checked_mul(4)?)?;
    }
    let len = usize::from(be::u16_at(code, p)?);
    let name = code.get(p.checked_add(2)?..p.checked_add(2)?.checked_add(len)?)?;
    if len == 0 || len >= MAX_NAME || !printable(name) {
        return None;
    }
    let at = u32::try_from(o).ok()?;
    let next = p.checked_add(2)?.checked_add(len)?.checked_add(3)? & !3;
    let end = u32::try_from(next).ok()?;
    let start = tb_offset.map_or(prev_end, |t| at.saturating_sub(t));
    Some((
        Table {
            at,
            start,
            end,
            name: msvc6_base::srcio::decode(name),
            lang,
            flags1,
            flags2,
            fixed_parms: fixed,
            float_parms: float,
            tb_offset,
        },
        next,
    ))
}

#[must_use]
pub fn scan(code: &[u8]) -> Vec<Table> {
    let mut out: Vec<Table> = Vec::new();
    let mut o = 4usize;
    while o.saturating_add(16) <= code.len() {
        if be::u32_at(code, o) != Some(0) {
            o = o.saturating_add(4);
            continue;
        }
        let prev_end = out.last().map_or(0, |t| t.end);
        match read_at(code, o, prev_end) {
            Some((t, next)) => {
                out.push(t);
                o = next.max(o.saturating_add(4));
            }
            None => o = o.saturating_add(4),
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn table(name: &[u8], tb_offset: Option<u32>, fixed: u8) -> Vec<u8> {
        let mut t = vec![0, 0, 0, 0, 0, 9];
        t.push(if tb_offset.is_some() {
            HAS_TB_OFFSET
        } else {
            0
        });
        t.push(NAME_PRESENT);
        t.extend_from_slice(&[0, 0, fixed, 0]);
        if fixed != 0 {
            t.extend_from_slice(&[0; 4]);
        }
        if let Some(off) = tb_offset {
            t.extend_from_slice(&off.to_be_bytes());
        }
        t.extend_from_slice(&u16::try_from(name.len()).unwrap_or(0).to_be_bytes());
        t.extend_from_slice(name);
        while !t.len().is_multiple_of(4) {
            t.push(0);
        }
        t
    }

    #[test]
    fn a_named_table_gives_the_function_its_extent() {
        let mut code = vec![0u8; 4];
        code.extend_from_slice(&0x3860_0000u32.to_be_bytes());
        code.extend_from_slice(&0x4e80_0020u32.to_be_bytes());
        code.extend(table(b".fill__6BufferFi", Some(8), 1));
        let after = code.len();
        code.extend_from_slice(&0x4e80_0020u32.to_be_bytes());
        code.extend(table(b".next", None, 0));
        let t = scan(&code);
        assert_eq!(t.len(), 2);
        assert_eq!((t[0].start, t[0].at), (4, 12));
        assert_eq!(t[0].function(), "fill__6BufferFi");
        assert_eq!(t[0].lang, 9);
        assert_eq!(t[1].start, u32::try_from(after).unwrap_or(0));
        assert_eq!(t[1].tb_offset, None);
    }

    #[test]
    fn a_zero_word_that_is_no_table_is_stepped_over() {
        let mut code = vec![0u8; 32];
        code.extend(table(b"\x01bad", Some(4), 0));
        code.extend(table(b"", Some(4), 0));
        assert!(scan(&code).is_empty());
        let mut cut = vec![0u8; 4];
        cut.extend(table(b".name_runs_out", Some(4), 0));
        cut.truncate(cut.len() - 4);
        assert!(scan(&cut).is_empty());
    }
}
