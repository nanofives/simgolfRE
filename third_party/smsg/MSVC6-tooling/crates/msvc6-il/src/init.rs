use crate::cursor::{Cursor, Ids, Result, error, quote};
use std::fmt::Write as _;

pub const MAX_PAIRS: usize = 4096;

fn arr<const N: usize>(c: &mut Cursor<'_>) -> Result<[u8; N]> {
    let at = c.pos();
    <[u8; N]>::try_from(c.take(N)?).map_err(|_| error(at, "short read"))
}

fn scalar(c: &mut Cursor<'_>) -> Result<String> {
    let kind = i8::from_le_bytes([c.byte()?]);
    let size = i8::from_le_bytes([c.byte()?]);
    let v = match (size, kind) {
        (1, _) => format!("{}", c.byte()?),
        (2, _) => format!("{}", c.short()?),
        (4, 5) => format!("{}f", f32::from_le_bytes(arr(c)?)),
        (4, _) => {
            let l = c.long()?;
            if (-0x100..0x100).contains(&l) {
                format!("{l}")
            } else {
                format!("0x{:x}", u32::from_le_bytes(l.to_le_bytes()))
            }
        }
        (8, 5) => format!("{}", f64::from_le_bytes(arr(c)?)),
        (8, _) => format!("{}", c.quad()?),
        (10, _) => format!("{} (ext)", crate::ex::ext80(&arr(c)?)),
        _ => {
            return Err(error(
                c.pos(),
                format!("a scalar of size {size}: C2 crashes on it"),
            ));
        }
    };
    Ok(format!("scalar kind {kind} size {size} = {v}"))
}

fn items(c: &mut Cursor<'_>, id: &dyn Fn(u32) -> String, out: &mut String) -> Result<()> {
    loop {
        let off = c.pos();
        let k = c.byte()?;
        let text = match k {
            0x07 => {
                let _ = writeln!(out, "{off:04x}    07 end");
                return Ok(());
            }
            0x01 => format!("01 {}", scalar(c)?),
            0x02 => {
                let s = id(c.id()?);
                let o = c.long()?;
                let n = c.byte()?;
                format!("02 address {s} + {o} size {n}")
            }
            0x03 => {
                let b = c.lstr()?;
                format!("03 bytes [{}] {}", b.len(), quote(b))
            }
            0x04 => format!("04 byte 0x{:x}", c.byte()?),
            0x05 => {
                let n = c.long()?;
                let what = c.byte()?;
                match what {
                    1 => format!("05 repeat {n} x {}", scalar(c)?),
                    3 => {
                        let b = c.lstr()?;
                        format!("05 repeat {n} x bytes [{}] {}", b.len(), quote(b))
                    }
                    _ => {
                        return Err(error(off, format!("a repeat of item {what:#04x}: C2 ICEs")));
                    }
                }
            }
            0x06 => "06 group end".to_string(),
            0x08 => format!("08 zeros {}", c.long()?),
            0x0b => {
                let b = c.lstr()?;
                format!("0b counted bytes [{}] {}", b.len(), quote(b))
            }
            0x10 => {
                let mut v = Vec::new();
                loop {
                    let a = c.byte()?;
                    if a == 0x11 {
                        break;
                    }
                    if v.len() >= MAX_PAIRS {
                        return Err(error(off, format!("more than {MAX_PAIRS} pairs")));
                    }
                    let b = c.byte()?;
                    v.push(format!("({a},{b})"));
                }
                format!("10 pairs {} then {}", v.join(""), scalar(c)?)
            }
            _ => {
                return Err(error(
                    off,
                    format!("data item {k:#04x}: C2's reader ICEs on it"),
                ));
            }
        };
        let _ = writeln!(out, "{off:04x}    {text}");
    }
}

pub fn dump(data: &[u8], ids: Ids, id: &dyn Fn(u32) -> String, out: &mut String) -> Result<()> {
    let mut c = Cursor::new(data, 0).with_ids(ids);
    while !c.at_end() {
        let off = c.pos();
        let k = c.byte()?;
        match k {
            0x00 => {
                let s = id(c.id()?);
                let o = c.long()?;
                let _ = writeln!(out, "{off:04x}  00 data {s} at +{o}");
                items(&mut c, id, out)?;
            }
            0x0a => {
                let o = c.long()?;
                let _ = writeln!(out, "{off:04x}  0a data (no symbol) at +{o}");
                items(&mut c, id, out)?;
            }
            0x09 => {
                let s = id(c.id()?);
                let _ = writeln!(out, "{off:04x}  09 function {s}");
            }
            0x0c => {
                let _ = writeln!(out, "{off:04x}  0c byte 0x{:x}", c.byte()?);
            }
            0x0e => {
                let _ = writeln!(out, "{off:04x}  0e pch init {}", quote(c.str()?));
            }
            0x0f | 0xff => {
                let _ = writeln!(out, "{off:04x}  {k:02x} end");
                break;
            }
            _ => {
                return Err(error(
                    off,
                    format!("in record {k:#04x}: C2's reader ICEs on it"),
                ));
            }
        }
    }
    Ok(())
}

pub fn order(data: &[u8], ids: Ids) -> Result<Vec<(u8, u32)>> {
    let mut out = Vec::new();
    let mut c = Cursor::new(data, 0).with_ids(ids);
    let mut sink = String::new();
    let none = |i: u32| format!("#{i:x}");
    while !c.at_end() {
        let k = c.byte()?;
        match k {
            0x00 => {
                out.push((k, c.id()?));
                c.long()?;
                items(&mut c, &none, &mut sink)?;
            }
            0x0a => {
                c.long()?;
                items(&mut c, &none, &mut sink)?;
            }
            0x09 => out.push((k, c.id()?)),
            0x0c => {
                c.byte()?;
            }
            0x0e => {
                c.str()?;
            }
            0x0f | 0xff => break,
            _ => {
                return Err(error(
                    c.pos().saturating_sub(1),
                    format!("in record {k:#04x}: C2's reader ICEs on it"),
                ));
            }
        }
        sink.clear();
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_symbols_data_and_a_functions_place_read_in_order() {
        let b = [
            0x00, 0xd5, 0x00, 0x00, 0x01, 0x01, 0x04, 0x05, 0x07, 0x09, 0xd7, 0x00, 0x0f,
        ];
        let mut out = String::new();
        dump(&b, Ids::Plain, &|i| format!("#{i:x}"), &mut out).unwrap();
        assert_eq!(
            out,
            "0000  00 data #d5 at +0\n0004    01 scalar kind 1 size 4 = 5\n0008    07 end\n0009  09 function #d7\n000c  0f end\n"
        );
        assert_eq!(order(&b, Ids::Plain).unwrap(), vec![(0, 0xd5), (9, 0xd7)]);
        assert!(dump(&[0x42], Ids::Plain, &|i| format!("{i}"), &mut String::new()).is_err());
    }
}
