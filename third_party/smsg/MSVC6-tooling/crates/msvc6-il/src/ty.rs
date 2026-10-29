pub const VOLATILE: u16 = 0x1000;
pub const CONST: u16 = 0x2000;
pub const P4000: u16 = 0x4000;
pub const KIND_AGGREGATE: u8 = 6;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Ty {
    pub raw: u16,
    pub kind: u8,
    pub size: u8,
}

#[must_use]
pub fn of(raw: u16) -> Ty {
    Ty {
        raw,
        kind: u8::try_from(raw & 0xf).unwrap_or(0),
        size: u8::try_from((raw >> 4) & 0x1f).unwrap_or(0),
    }
}

impl Ty {
    #[must_use]
    pub fn kind_name(&self) -> &'static str {
        match self.kind {
            0 => "none",
            1 => "int",
            2 => "uint",
            3 => "ptr",
            4 => "ptr4",
            5 => "float",
            6 => "block",
            7 => "void",
            10 => "real",
            11 => "ptrb",
            _ => "kind?",
        }
    }

    #[must_use]
    pub fn high(&self) -> u16 {
        self.raw & 0x0e00
    }

    #[must_use]
    pub fn text(&self, block: Option<i32>) -> String {
        let bits = u32::from(self.size) * 8;
        let mut s = match self.kind {
            0 => "none".to_string(),
            1 => format!("i{bits}"),
            2 => format!("u{bits}"),
            3 => "p32".to_string(),
            4 => format!("p4.{}", self.size),
            5 => format!("f{bits}"),
            6 => match block {
                Some(n) => format!("b{n}"),
                None => format!("b{}", self.size),
            },
            7 => "void".to_string(),
            10 => format!("r{bits}"),
            11 => format!("pb.{}", self.size),
            _ => format!("ty{:x}", self.raw),
        };
        if self.raw & CONST != 0 {
            s.push_str(" const");
        }
        if self.raw & VOLATILE != 0 {
            s.push_str(" volatile");
        }
        if self.raw & P4000 != 0 {
            s.push_str(" p4000");
        }
        s
    }

    #[must_use]
    pub fn c2(&self, block: Option<i32>) -> Option<u16> {
        let size = match block {
            Some(n) => u32::try_from(n).ok()?,
            None => u32::from(self.size),
        };
        let low = u16::try_from(size & 0xfff).ok()?;
        Some(match self.kind {
            0 => 0,
            1 => 0x1000 | low,
            2 => 0x2000 | low,
            3 | 4 | 0xb => 0x3004,
            5 | 10 => {
                if size == 10 {
                    0x400a
                } else {
                    0x4000 | low
                }
            }
            6 => {
                if size > 0xfff {
                    0x5000
                } else {
                    0x5000 | low
                }
            }
            7 => 0x1004,
            _ => return None,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_type_word_reads_kind_size_and_qualifiers() {
        assert_eq!(of(0x41).text(None), "i32");
        assert_eq!(of(0x2041).text(None), "i32 const");
        assert_eq!(of(0x1041).text(None), "i32 volatile");
        assert_eq!(of(0x43).text(None), "p32");
        assert_eq!(of(0x6).text(Some(40)), "b40");
        assert_eq!(of(0xaa).text(None), "r80");
        assert_eq!(of(0x41).c2(None), Some(0x1004));
        assert_eq!(of(0xa5).c2(None), Some(0x400a));
        assert_eq!(of(0x6).c2(Some(0x2000)), Some(0x5000));
        assert_eq!(of(0x9).c2(None), None);
        assert_eq!(of(0x0641).high(), 0x0600);
        assert_eq!(of(0x0641).text(None), "i32");
    }
}
