#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Buf(Vec<u8>);

#[must_use]
pub fn n32(n: usize) -> u32 {
    u32::try_from(n).unwrap_or(u32::MAX)
}

#[must_use]
pub fn n16(n: usize) -> u16 {
    u16::try_from(n).unwrap_or(u16::MAX)
}

#[must_use]
pub fn us(n: u32) -> usize {
    usize::try_from(n).unwrap_or(usize::MAX)
}

#[must_use]
pub fn align_up(n: u32, a: u32) -> u32 {
    if a == 0 {
        n
    } else {
        n.div_ceil(a).saturating_mul(a)
    }
}

impl Buf {
    #[must_use]
    pub fn new() -> Self {
        Self(Vec::new())
    }

    pub fn u8(&mut self, v: u8) -> &mut Self {
        self.0.push(v);
        self
    }

    pub fn u16(&mut self, v: u16) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }

    pub fn u32(&mut self, v: u32) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }

    pub fn u32be(&mut self, v: u32) -> &mut Self {
        self.0.extend_from_slice(&v.to_be_bytes());
        self
    }

    pub fn bytes(&mut self, b: &[u8]) -> &mut Self {
        self.0.extend_from_slice(b);
        self
    }

    pub fn cstr(&mut self, s: &str) -> &mut Self {
        self.0.extend_from_slice(s.as_bytes());
        self.0.push(0);
        self
    }

    pub fn fill(&mut self, b: u8, n: usize) -> &mut Self {
        self.0.resize(self.0.len().saturating_add(n), b);
        self
    }

    pub fn zeros(&mut self, n: usize) -> &mut Self {
        self.fill(0, n)
    }

    pub fn align(&mut self, a: usize) -> &mut Self {
        if a > 1 {
            while !self.0.len().is_multiple_of(a) {
                self.0.push(0);
            }
        }
        self
    }

    pub fn pad_to(&mut self, len: usize) -> &mut Self {
        if self.0.len() < len {
            self.0.resize(len, 0);
        }
        self
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.0.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    #[must_use]
    pub fn pos(&self) -> u32 {
        n32(self.0.len())
    }

    pub fn put(&mut self, at: usize, b: &[u8]) {
        if let Some(s) = self.0.get_mut(at..at.saturating_add(b.len())) {
            s.copy_from_slice(b);
        }
    }

    pub fn put_u16(&mut self, at: usize, v: u16) {
        self.put(at, &v.to_le_bytes());
    }

    pub fn put_u32(&mut self, at: usize, v: u32) {
        self.put(at, &v.to_le_bytes());
    }

    #[must_use]
    pub fn as_slice(&self) -> &[u8] {
        &self.0
    }

    #[must_use]
    pub fn into_vec(self) -> Vec<u8> {
        self.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_writer_is_little_endian_and_patches_in_place() {
        let mut b = Buf::new();
        b.u16(0x5a4d).u32(0x0102_0304).u32be(0x0a0b_0c0d).cstr("ab");
        assert_eq!(
            b.as_slice(),
            [0x4d, 0x5a, 4, 3, 2, 1, 0xa, 0xb, 0xc, 0xd, b'a', b'b', 0]
        );
        b.put_u32(2, 0xffff_ffff);
        assert_eq!(&b.as_slice()[2..6], [0xff; 4]);
        b.put_u32(12, 7);
        assert_eq!(b.len(), 13);
        b.align(8);
        assert_eq!(b.len(), 16);
        assert_eq!(align_up(0x1001, 0x1000), 0x2000);
        assert_eq!(align_up(0, 0x200), 0);
        assert_eq!(align_up(5, 0), 5);
    }
}
