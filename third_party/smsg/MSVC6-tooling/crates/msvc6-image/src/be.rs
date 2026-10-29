fn array_at<const N: usize>(d: &[u8], at: usize) -> Option<[u8; N]> {
    let s = d.get(at..at.checked_add(N)?)?;
    <[u8; N]>::try_from(s).ok()
}

#[must_use]
pub fn u16_at(d: &[u8], at: usize) -> Option<u16> {
    array_at(d, at).map(u16::from_be_bytes)
}

#[must_use]
pub fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    array_at(d, at).map(u32::from_be_bytes)
}

#[must_use]
pub fn i32_at(d: &[u8], at: usize) -> Option<i32> {
    array_at(d, at).map(i32::from_be_bytes)
}

#[must_use]
pub fn i16_at(d: &[u8], at: usize) -> Option<i16> {
    array_at(d, at).map(i16::from_be_bytes)
}

#[must_use]
pub fn cstr(d: &[u8], at: usize, max: usize) -> Option<&[u8]> {
    let s = d.get(at..)?;
    let s = s.get(..s.len().min(max))?;
    let end = s.iter().position(|c| *c == 0).unwrap_or(s.len());
    s.get(..end)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_readers_are_big_endian_and_bounded() {
        let d = [0x12, 0x34, 0x56, 0x78, 0xff];
        assert_eq!(u16_at(&d, 0), Some(0x1234));
        assert_eq!(u32_at(&d, 0), Some(0x1234_5678));
        assert_eq!(i16_at(&d, 3), Some(0x78ff));
        assert_eq!(i32_at(&d, 1), Some(0x3456_78ff));
        assert_eq!(u32_at(&d, 2), None);
        assert_eq!(u32_at(&d, usize::MAX), None);
        assert_eq!(cstr(b"ab\0cd", 0, 16), Some(&b"ab"[..]));
        assert_eq!(cstr(b"abcd", 1, 2), Some(&b"bc"[..]));
        assert_eq!(cstr(b"ab", 3, 2), None);
    }
}
