use std::fmt;

pub const MAX_STR: usize = 0x81e;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    pub at: usize,
    pub what: String,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:#x}: {}", self.at, self.what)
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

#[must_use]
pub fn error(at: usize, what: impl Into<String>) -> Error {
    Error {
        at,
        what: what.into(),
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Ids {
    #[default]
    Plain,
    Compact,
}

#[derive(Debug, Clone)]
pub struct Cursor<'a> {
    data: &'a [u8],
    pos: usize,
    pub ids: Ids,
}

fn sbyte(b: u8) -> i8 {
    i8::from_le_bytes([b])
}

impl<'a> Cursor<'a> {
    #[must_use]
    pub fn new(data: &'a [u8], pos: usize) -> Self {
        Self {
            data,
            pos,
            ids: Ids::Plain,
        }
    }

    #[must_use]
    pub fn with_ids(mut self, ids: Ids) -> Self {
        self.ids = ids;
        self
    }

    #[must_use]
    pub fn pos(&self) -> usize {
        self.pos
    }

    pub fn seek(&mut self, pos: usize) {
        self.pos = pos;
    }

    #[must_use]
    pub fn data(&self) -> &'a [u8] {
        self.data
    }

    #[must_use]
    pub fn at_end(&self) -> bool {
        self.pos >= self.data.len()
    }

    #[must_use]
    pub fn left(&self) -> usize {
        self.data.len().saturating_sub(self.pos)
    }

    pub fn byte(&mut self) -> Result<u8> {
        let b = *self
            .data
            .get(self.pos)
            .ok_or_else(|| error(self.pos, "end of file"))?;
        self.pos = self.pos.saturating_add(1);
        Ok(b)
    }

    pub fn take(&mut self, n: usize) -> Result<&'a [u8]> {
        let end = self
            .pos
            .checked_add(n)
            .filter(|e| *e <= self.data.len())
            .ok_or_else(|| error(self.pos, format!("{n} byte(s) run past the end")))?;
        let s = self
            .data
            .get(self.pos..end)
            .ok_or_else(|| error(self.pos, format!("{n} byte(s) run past the end")))?;
        self.pos = end;
        Ok(s)
    }

    fn array<const N: usize>(&mut self) -> Result<[u8; N]> {
        let at = self.pos;
        let s = self.take(N)?;
        <[u8; N]>::try_from(s).map_err(|_| error(at, "short read"))
    }

    pub fn u16(&mut self) -> Result<u16> {
        Ok(u16::from_le_bytes(self.array()?))
    }

    pub fn u32(&mut self) -> Result<u32> {
        Ok(u32::from_le_bytes(self.array()?))
    }

    pub fn short(&mut self) -> Result<i16> {
        let b = sbyte(self.byte()?);
        if b != i8::MIN {
            return Ok(i16::from(b));
        }
        Ok(i16::from_le_bytes(self.array()?))
    }

    pub fn long(&mut self) -> Result<i32> {
        let b = sbyte(self.byte()?);
        if b != i8::MIN {
            return Ok(i32::from(b));
        }
        Ok(i32::from_le_bytes(self.array()?))
    }

    pub fn quad(&mut self) -> Result<i64> {
        let b = sbyte(self.byte()?);
        if b != i8::MIN {
            return Ok(i64::from(b));
        }
        Ok(i64::from_le_bytes(self.array()?))
    }

    pub fn id(&mut self) -> Result<u32> {
        if self.ids == Ids::Compact {
            let b0 = u32::from(self.byte()?);
            if b0 & 0x80 == 0 {
                return Ok(b0 + 0x003f_ff80);
            }
            let b1 = u32::from(self.byte()?);
            let mut v = (b0 & 0x3f) | (b1 << 6);
            if b0 & 0x40 != 0 {
                v |= u32::from(self.byte()?) << 14;
            }
            return Ok(v);
        }
        let lo = u32::from(self.u16()?);
        if lo & 0x8000 == 0 {
            return Ok(lo);
        }
        let hi = u32::from(self.u16()?);
        Ok((lo & 0x7fff) | (hi << 15))
    }

    pub fn ty(&mut self) -> Result<u16> {
        let b0 = self.byte()?;
        if b0 & 0x80 == 0 {
            return Ok(u16::from(b0));
        }
        let b1 = self.byte()?;
        Ok(u16::from_be_bytes([b0, b1]) & 0x7fff)
    }

    pub fn str(&mut self) -> Result<&'a [u8]> {
        let start = self.pos;
        let rest = self.data.get(start..).unwrap_or_default();
        let window = rest.get(..MAX_STR).unwrap_or(rest);
        let Some(n) = window.iter().position(|&b| b == 0) else {
            return Err(if window.len() == MAX_STR {
                error(
                    start,
                    format!("a string longer than C2's {MAX_STR:#x}-byte buffer"),
                )
            } else {
                error(start, "a string with no NUL before the end of file")
            });
        };
        let s = self.take(n)?;
        self.pos = self.pos.saturating_add(1);
        Ok(s)
    }

    pub fn lstr(&mut self) -> Result<&'a [u8]> {
        let n = self.short()?;
        match usize::try_from(n) {
            Ok(n) if n > 0 => self.take(n),
            _ => Ok(&[]),
        }
    }
}

#[must_use]
pub fn latin1(b: &[u8]) -> String {
    b.iter().map(|&c| char::from(c)).collect()
}

#[must_use]
pub fn quote(b: &[u8]) -> String {
    let mut s = String::from("\"");
    for &c in b {
        match c {
            b'"' => s.push_str("\\\""),
            b'\\' => s.push_str("\\\\"),
            0x20..=0x7e => s.push(char::from(c)),
            b'\n' => s.push_str("\\n"),
            b'\t' => s.push_str("\\t"),
            _ => s.push_str(&format!("\\x{c:02x}")),
        }
    }
    s.push('"');
    s
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_short_long_and_quad_escape_on_0x80() {
        let b = [0x05, 0xfe, 0x80, 0x34, 0x12, 0x80, 0x78, 0x56, 0x34, 0x12];
        let mut c = Cursor::new(&b, 0);
        assert_eq!(c.short().unwrap(), 5);
        assert_eq!(c.long().unwrap(), -2);
        assert_eq!(c.short().unwrap(), 0x1234);
        assert_eq!(c.long().unwrap(), 0x1234_5678);
        assert!(c.at_end());
        let q = [0x80, 1, 0, 0, 0, 0, 0, 0, 0x80];
        assert_eq!(Cursor::new(&q, 0).quad().unwrap(), i64::MIN + 1);
    }

    #[test]
    fn an_id_takes_a_second_word_on_bit_15() {
        let b = [0xd7, 0x00, 0x01, 0x80, 0x02, 0x00];
        let mut c = Cursor::new(&b, 0);
        assert_eq!(c.id().unwrap(), 0xd7);
        assert_eq!(c.id().unwrap(), 0x0001 | (2 << 15));
        let k = [0x05, 0x81, 0x02, 0xc1, 0x02, 0x03];
        let mut c = Cursor::new(&k, 0).with_ids(Ids::Compact);
        assert_eq!(c.id().unwrap(), 5 + 0x003f_ff80);
        assert_eq!(c.id().unwrap(), 1 | (2 << 6));
        assert_eq!(c.id().unwrap(), 1 | (2 << 6) | (3 << 14));
    }

    #[test]
    fn a_type_is_one_byte_or_two_big_endian() {
        let b = [0x41, 0xa0, 0x41, 0x86, 0x01];
        let mut c = Cursor::new(&b, 0);
        assert_eq!(c.ty().unwrap(), 0x41);
        assert_eq!(c.ty().unwrap(), 0x2041);
        assert_eq!(c.ty().unwrap(), 0x0601);
    }

    #[test]
    fn a_string_ends_at_its_nul_within_the_buffer() {
        let b = b"two.cpp\0x";
        let mut c = Cursor::new(b, 0);
        assert_eq!(c.str().unwrap(), b"two.cpp");
        assert_eq!(c.pos(), 8);
        assert!(Cursor::new(b"abc", 0).str().is_err());
        let long = vec![b'a'; MAX_STR + 4];
        assert!(Cursor::new(&long, 0).str().is_err());
        assert_eq!(Cursor::new(&[0x02, b'h', b'i'], 0).lstr().unwrap(), b"hi");
        assert!(Cursor::new(&[0x05, b'h'], 0).lstr().is_err());
    }

    #[test]
    fn a_read_past_the_end_is_an_error_at_its_offset() {
        let mut c = Cursor::new(&[0x80, 0x01], 0);
        let e = c.long().unwrap_err();
        assert_eq!(e.at, 1);
        assert!(Cursor::new(&[], 0).byte().is_err());
        assert_eq!(quote(b"a\"b\\\x01"), "\"a\\\"b\\\\\\x01\"");
    }
}
