use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Short {
    pub at: usize,
    pub want: usize,
    pub have: usize,
}

impl fmt::Display for Short {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} byte(s) at {:#x} run past the {} byte(s) there are",
            self.want, self.at, self.have
        )
    }
}

impl std::error::Error for Short {}

pub fn bytes_at(d: &[u8], at: usize, n: usize) -> Result<&[u8], Short> {
    let short = Short {
        at,
        want: n,
        have: d.len(),
    };
    d.get(at..at.checked_add(n).ok_or(short)?).ok_or(short)
}

fn array_at<const N: usize>(d: &[u8], at: usize) -> Result<[u8; N], Short> {
    let mut a = [0u8; N];
    a.copy_from_slice(bytes_at(d, at, N)?);
    Ok(a)
}

pub fn u8_at(d: &[u8], at: usize) -> Result<u8, Short> {
    array_at::<1>(d, at).map(|[b]| b)
}

pub fn u16_at(d: &[u8], at: usize) -> Result<u16, Short> {
    array_at(d, at).map(u16::from_le_bytes)
}

pub fn u32_at(d: &[u8], at: usize) -> Result<u32, Short> {
    array_at(d, at).map(u32::from_le_bytes)
}

pub fn i32_at(d: &[u8], at: usize) -> Result<i32, Short> {
    array_at(d, at).map(i32::from_le_bytes)
}

#[cfg(test)]
mod tests {
    use super::*;

    const D: [u8; 6] = [0x4d, 0x5a, 0x90, 0x00, 0xff, 0xff];

    #[test]
    fn the_readers_are_little_endian() {
        assert_eq!(u8_at(&D, 2), Ok(0x90));
        assert_eq!(u16_at(&D, 0), Ok(0x5a4d));
        assert_eq!(u32_at(&D, 0), Ok(0x0090_5a4d));
        assert_eq!(i32_at(&D, 2), Ok(-0x0000_ff70));
        assert_eq!(bytes_at(&D, 4, 2), Ok(&[0xff, 0xff][..]));
        assert_eq!(bytes_at(&D, 6, 0), Ok(&[][..]));
    }

    #[test]
    fn a_read_past_the_end_says_where_and_by_how_much() {
        let e = u32_at(&D, 4).unwrap_err();
        assert_eq!(
            e,
            Short {
                at: 4,
                want: 4,
                have: 6
            }
        );
        assert_eq!(
            e.to_string(),
            "4 byte(s) at 0x4 run past the 6 byte(s) there are"
        );
        assert!(bytes_at(&D, usize::MAX, 2).is_err());
        assert!(u8_at(&[], 0).is_err());
    }
}
