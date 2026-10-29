use anyhow::{Result, bail};

pub use msvc6_codeview::codec::{bytes, u16_at, u16_of, u32_at, u32_of};

#[must_use]
pub fn us(v: u32) -> usize {
    usize::try_from(v).unwrap_or(usize::MAX)
}

pub fn put(d: &mut [u8], at: usize, b: &[u8]) -> Result<()> {
    let len = d.len();
    match d.get_mut(at..at.saturating_add(b.len())) {
        Some(s) => {
            s.copy_from_slice(b);
            Ok(())
        }
        None => bail!(
            "a write of {} bytes at {at:#x} runs past {len} bytes",
            b.len()
        ),
    }
}

pub fn set16(d: &mut [u8], at: usize, v: u16) -> Result<()> {
    put(d, at, &v.to_le_bytes())
}

pub fn set32(d: &mut [u8], at: usize, v: u32) -> Result<()> {
    put(d, at, &v.to_le_bytes())
}

pub fn push16(out: &mut Vec<u8>, v: u16) {
    out.extend_from_slice(&v.to_le_bytes());
}

pub fn push32(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

#[must_use]
pub fn latin1(b: &[u8]) -> String {
    b.iter().map(|c| char::from(*c)).collect()
}

pub fn free_slots(cap: usize) -> FreeSlots {
    FreeSlots {
        next: (0..=cap).collect(),
        cap,
        steps: 0,
    }
}

#[derive(Debug, Clone)]
pub struct FreeSlots {
    next: Vec<usize>,
    cap: usize,
    steps: usize,
}

impl FreeSlots {
    #[must_use]
    pub fn steps(&self) -> usize {
        self.steps
    }

    fn find(&mut self, i: usize) -> usize {
        let mut r = i;
        while let Some(n) = self.next.get(r).copied() {
            if n == r {
                break;
            }
            self.steps = self.steps.saturating_add(1);
            r = n;
        }
        let mut c = i;
        while let Some(n) = self.next.get(c).copied() {
            if n == c {
                break;
            }
            self.steps = self.steps.saturating_add(1);
            if let Some(s) = self.next.get_mut(c) {
                *s = r;
            }
            c = n;
        }
        r
    }

    pub fn take_from(&mut self, home: usize) -> Option<usize> {
        if self.cap == 0 {
            return None;
        }
        let mut s = self.find(home % self.cap);
        if s == self.cap {
            s = self.find(0);
            if s == self.cap {
                return None;
            }
        }
        if let Some(n) = self.next.get_mut(s) {
            *n = s.saturating_add(1);
        }
        Some(s)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_free_slot_is_the_first_one_from_home_wrapping_around() {
        let mut f = free_slots(4);
        assert_eq!(f.take_from(2), Some(2));
        assert_eq!(f.take_from(2), Some(3));
        assert_eq!(f.take_from(2), Some(0));
        assert_eq!(f.take_from(7), Some(1));
        assert_eq!(f.take_from(0), None);
        assert_eq!(free_slots(0).take_from(0), None);
    }

    #[test]
    fn slots_taken_from_one_home_cost_a_constant_each() {
        let n = 20_000usize;
        let mut f = free_slots(n);
        for k in 0..n {
            assert_eq!(f.take_from(7), Some((7 + k) % n));
        }
        assert_eq!(f.take_from(7), None);
        assert!(f.steps() <= 5 * n, "{} steps for {n} slots", f.steps());
    }

    #[test]
    fn writes_past_the_end_are_errors() {
        let mut d = [0u8; 4];
        assert!(set32(&mut d, 0, 7).is_ok());
        assert!(set32(&mut d, 1, 7).is_err());
        assert!(set16(&mut d, usize::MAX, 7).is_err());
        assert!(put(&mut d, 3, &[1, 2]).is_err());
        assert_eq!(latin1(&[0x41, 0xe9]), "A\u{e9}");
    }
}
