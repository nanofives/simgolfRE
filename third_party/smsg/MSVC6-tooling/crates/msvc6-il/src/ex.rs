use crate::cursor::{Cursor, Ids, Result, error};
use crate::grammar::Grammar;
use crate::ops;
use crate::ty;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Arg {
    Type(u16, Option<i32>),
    Id(u32),
    Byte(u8),
    Short(i16),
    Long(i32),
    Quad(i64),
    Real([u8; 12]),
    LStr(Vec<u8>),
    List(Vec<(i16, u8)>),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rec {
    pub off: usize,
    pub end: usize,
    pub op: u8,
    pub sub: Option<i16>,
    pub args: Vec<Arg>,
}

impl Rec {
    #[must_use]
    pub fn first_id(&self) -> Option<u32> {
        self.args.iter().find_map(|a| match a {
            Arg::Id(i) => Some(*i),
            _ => None,
        })
    }

    pub fn ids(&self) -> impl Iterator<Item = u32> + '_ {
        self.args.iter().filter_map(|a| match a {
            Arg::Id(i) => Some(*i),
            _ => None,
        })
    }

    #[must_use]
    pub fn ends_body(&self) -> bool {
        self.op == ops::ENDSCOPE && matches!(self.args.first(), Some(Arg::Long(0)))
    }

    #[must_use]
    pub fn long(&self) -> Option<i32> {
        self.args.iter().find_map(|a| match a {
            Arg::Long(l) => Some(*l),
            _ => None,
        })
    }
}

pub const MAX_WARNINGS: usize = 4096;

fn read_type(c: &mut Cursor<'_>) -> Result<Arg> {
    let t = c.ty()?;
    let x = ty::of(t);
    let extra = if x.kind == ty::KIND_AGGREGATE && x.size == 0 {
        Some(c.long()?)
    } else {
        None
    };
    Ok(Arg::Type(t, extra))
}

fn pragma_args(
    c: &mut Cursor<'_>,
    g: &Grammar,
    off: usize,
    sub: i16,
    a: &mut Vec<Arg>,
) -> Result<()> {
    let Some(p) = g.pragma(sub) else {
        return Err(error(
            off,
            format!("0x4f sub-op {sub} is outside C2's sub-op table"),
        ));
    };
    let Some(codes) = &p.codes else {
        return Ok(());
    };
    for &code in codes {
        match code {
            0x02 | 0x0e | 0x15 => a.push(Arg::Short(c.short()?)),
            0x0b => a.push(Arg::LStr(crate::sym::comment(c)?.into_bytes())),
            0x0c => a.push(Arg::LStr(c.lstr()?.to_vec())),
            0x14 => a.push(Arg::Long(c.long()?)),
            0x16 | 0x18 | b's' => a.push(Arg::Id(c.id()?)),
            0x17 => {
                let mut v = Vec::new();
                let mut n = c.short()?;
                while n != -1 {
                    if v.len() >= MAX_WARNINGS {
                        return Err(error(
                            off,
                            format!("more than {MAX_WARNINGS} warning states in one record"),
                        ));
                    }
                    let b = c.byte()?;
                    v.push((n, b));
                    n = c.short()?;
                }
                a.push(Arg::List(v));
            }
            b'l' => {
                if c.ids == Ids::Compact {
                    a.push(Arg::Long(c.long()?));
                } else {
                    a.push(Arg::Short(c.short()?));
                }
            }
            _ => {
                return Err(error(
                    off,
                    format!("0x4f sub-op {sub:#x} has code {code:#04x}, on which C2 ICEs"),
                ));
            }
        }
    }
    Ok(())
}

pub fn read(c: &mut Cursor<'_>, g: &Grammar, dlp: bool) -> Result<Rec> {
    let off = c.pos();
    let op = c.byte()?;
    let mut r = Rec {
        off,
        end: off,
        op,
        sub: None,
        args: Vec::new(),
    };
    let fmt = g
        .fmt(op)
        .ok_or_else(|| error(off, format!("op {op:#04x} is beyond C2's format table")))?;
    let a = &mut r.args;
    match fmt {
        0 | 0x10 => {}
        1 => {
            a.push(read_type(c)?);
            if dlp && op == 0x27 {
                a.push(Arg::Long(c.long()?));
            }
        }
        2 => {
            a.push(Arg::Id(c.id()?));
            if dlp && op == 0x28 {
                a.push(Arg::Long(c.long()?));
            }
        }
        3 => {
            a.push(Arg::Id(c.id()?));
            a.push(read_type(c)?);
            a.push(Arg::Byte(c.byte()?));
            a.push(Arg::Byte(c.byte()?));
        }
        4 => {
            a.push(Arg::Id(c.id()?));
            a.push(Arg::Byte(c.byte()?));
        }
        5 | 9 => {
            a.push(read_type(c)?);
            a.push(Arg::Byte(c.byte()?));
        }
        6 => {
            let t = read_type(c)?;
            let (raw, blk) = match &t {
                Arg::Type(raw, blk) => (*raw, *blk),
                _ => (0, None),
            };
            a.push(t);
            let c2 = ty::of(raw)
                .c2(blk)
                .ok_or_else(|| error(off, format!("constant type {raw:#x} ICEs C2")))?;
            if c2 & 0xf000 == 0x4000 {
                if c2 != 0x400a {
                    return Err(error(
                        off,
                        format!("a floating constant of C2 type {c2:#x} (C2 ICEs: not 0x400a)"),
                    ));
                }
                let b = c.take(12)?;
                let x = <[u8; 12]>::try_from(b).map_err(|_| error(off, "short real"))?;
                a.push(Arg::Real(x));
            } else if c2 & 0xfff == 8 {
                a.push(Arg::Quad(c.quad()?));
            } else {
                a.push(Arg::Long(c.long()?));
            }
        }
        7 | 0x12 => {
            a.push(read_type(c)?);
            a.push(Arg::Id(c.id()?));
        }
        8 => a.push(Arg::Id(c.id()?)),
        10 => a.push(Arg::Byte(c.byte()?)),
        0x0c => {
            let sub = c.short()?;
            r.sub = Some(sub);
            pragma_args(c, g, off, sub, &mut r.args)?;
        }
        0x0d | 0x11 => a.push(Arg::Long(c.long()?)),
        0x0e => {
            a.push(read_type(c)?);
            a.push(Arg::Id(c.id()?));
            a.push(Arg::Id(c.id()?));
        }
        0x0f => a.push(Arg::Short(c.short()?)),
        0x13 => {
            a.push(read_type(c)?);
            a.push(Arg::Long(c.long()?));
        }
        0x14 => {
            a.push(Arg::Long(c.long()?));
            a.push(Arg::Long(c.long()?));
        }
        0x15 => {
            a.push(Arg::Id(c.id()?));
            a.push(Arg::Id(c.id()?));
            a.push(Arg::Short(c.short()?));
        }
        _ => {
            return Err(error(
                off,
                format!("op {op:#04x} has format {fmt:#x}, on which C2 ICEs"),
            ));
        }
    }
    r.end = c.pos();
    Ok(r)
}

#[must_use]
pub fn ext80(b: &[u8; 12]) -> f64 {
    let [m0, m1, m2, m3, m4, m5, m6, m7, e0, e1, _, _] = *b;
    let mant = u64::from_le_bytes([m0, m1, m2, m3, m4, m5, m6, m7]);
    let se = u16::from_le_bytes([e0, e1]);
    let sign = if se & 0x8000 != 0 { -1.0 } else { 1.0 };
    let exp = i32::from(se & 0x7fff);
    if exp == 0 && mant == 0 {
        return 0.0 * sign;
    }
    let hi = f64::from(u32::try_from(mant >> 32).unwrap_or(u32::MAX));
    let lo = f64::from(u32::try_from(mant & 0xffff_ffff).unwrap_or(u32::MAX));
    sign * (hi * 4_294_967_296.0 + lo) * 2f64.powi(exp - 16383 - 63)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::grammar::Pragma;

    pub fn toy() -> Grammar {
        let mut fmt = vec![0u8; 0xbc];
        let mut set = |op: usize, f: u8| fmt[op] = f;
        set(0x26, 2);
        set(0x29, 2);
        set(0x2d, 2);
        set(0x30, 1);
        set(0x32, 1);
        set(0x33, 6);
        set(0x3a, 2);
        set(0x3e, 5);
        set(0x41, 1);
        set(0x4f, 0x0c);
        set(0x54, 0x0d);
        set(0x5b, 0x0d);
        let mut pragma = vec![
            Pragma {
                codes: None,
                flag: 0
            };
            0x30
        ];
        pragma[1].codes = Some(b"l".to_vec());
        pragma[2].codes = Some(b"s".to_vec());
        pragma[0x1f].codes = Some(vec![0x14]);
        pragma[0x23].codes = Some(vec![0x17]);
        let mut attr = vec![0u16; 0xbe];
        for o in [0x38, 0x39, 0x3a, 0x41, 0x4b] {
            attr[o] = 0x1000;
        }
        Grammar::from_parts(fmt, attr, pragma)
    }

    #[test]
    fn records_read_their_operands_by_the_format_table() {
        let g = toy();
        let b = [
            0x4f, 0x1f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x26, 0xd7, 0x00, 0x33, 0xa0, 0x41, 0x01,
            0x4f, 0x23, 0x10, 0x0e, 0x80, 0xff, 0xff, 0x54, 0x00,
        ];
        let mut c = Cursor::new(&b, 0);
        let r = read(&mut c, &g, false).unwrap();
        assert_eq!(r.sub, Some(0x1f));
        assert_eq!(r.args, vec![Arg::Long(0x0080_0000)]);
        assert_eq!(read(&mut c, &g, false).unwrap().first_id(), Some(0xd7));
        let k = read(&mut c, &g, false).unwrap();
        assert_eq!(k.args, vec![Arg::Type(0x2041, None), Arg::Long(1)]);
        let w = read(&mut c, &g, false).unwrap();
        assert_eq!(w.args, vec![Arg::List(vec![(0x10, 0x0e)])]);
        assert!(read(&mut c, &g, false).unwrap().ends_body());
        assert!(c.at_end());
    }

    #[test]
    fn an_op_or_sub_op_beyond_the_tables_is_an_error() {
        let g = toy();
        assert!(read(&mut Cursor::new(&[0xf0], 0), &g, false).is_err());
        assert!(read(&mut Cursor::new(&[0x4f, 0x60], 0), &g, false).is_err());
        assert!(read(&mut Cursor::new(&[0x4f, 0x80, 0xff, 0x7f], 0), &g, false).is_err());
    }

    #[test]
    fn an_x87_real_reads_as_its_value() {
        let one = [0, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0x3f, 0, 0];
        assert!((ext80(&one) - 1.0).abs() < 1e-12);
        let half = [0, 0, 0, 0, 0, 0, 0, 0x80, 0xfe, 0xbf, 0, 0];
        assert!((ext80(&half) + 0.5).abs() < 1e-12);
    }
}
