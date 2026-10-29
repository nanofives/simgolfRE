use crate::le::u16_at;
use msvc6_codeview::codec::{Gen, name_at, numeric_len};

pub const BUCKETS20: u32 = 4096;
pub const TPI70_BUCKETS: u32 = 0x3ffff;

#[must_use]
pub fn lhash(bytes: &[u8], modulo: u32) -> u32 {
    let mut h = 0u32;
    let whole = bytes.len() & !3;
    for w in bytes.get(..whole).unwrap_or_default().as_chunks::<4>().0 {
        h ^= u32::from_le_bytes(*w);
    }
    let rest = bytes.get(whole..).unwrap_or_default();
    let mut it = rest.iter();
    if rest.len() & 2 != 0 {
        let lo = it.next().copied().unwrap_or(0);
        let hi = it.next().copied().unwrap_or(0);
        h ^= u32::from(u16::from_le_bytes([lo, hi]));
    }
    if rest.len() & 1 != 0 {
        h ^= u32::from(it.next().copied().unwrap_or(0));
    }
    h |= 0x2020_2020;
    h ^= h >> 11;
    h ^= h >> 16;
    if modulo == 0 { h } else { h % modulo }
}

#[must_use]
pub fn hash_v1(b: &[u8]) -> u32 {
    lhash(b, 0)
}

#[must_use]
pub fn udt_name20(rec: &[u8]) -> Option<(&[u8], u16)> {
    let leaf = u16_at(rec, 2).ok()?;
    let prop = u16_at(rec, 6).ok()?;
    let at = match leaf {
        0x1004 | 0x1005 => 20,
        0x1006 => 12,
        0x1007 => 16,
        _ => return None,
    };
    let at = if leaf == 0x1007 {
        at
    } else {
        let v = u16_at(rec, at).ok()?;
        at + match v {
            0..0x8000 => 2,
            0x8000 => 3,
            0x8001..=0x8002 => 4,
            0x8003..=0x8005 => 6,
            0x8006 | 0x8009 | 0x800a => 10,
            0x8007 => 12,
            0x8008 => 18,
            _ => return None,
        }
    };
    let n = usize::from(*rec.get(at)?);
    Some((rec.get(at + 1..at + 1 + n)?, prop))
}

fn anonymous20(name: &[u8]) -> bool {
    name.ends_with(b"__unnamed") || name.ends_with(b"<unnamed-tag>")
}

#[must_use]
pub fn type_hash20(rec: &[u8], modulo: u32) -> u32 {
    match udt_name20(rec) {
        Some((name, prop)) if prop & 0x180 == 0 && !anonymous20(name) => {
            let mut b = Vec::with_capacity(name.len() + 1);
            b.push(u8::try_from(name.len()).unwrap_or(u8::MAX));
            b.extend_from_slice(name);
            lhash(&b, modulo)
        }
        _ => lhash(rec, modulo),
    }
}

#[must_use]
pub fn jam_crc(b: &[u8]) -> u32 {
    let mut crc = 0u32;
    for byte in b {
        crc ^= u32::from(*byte);
        for _ in 0..8 {
            crc = if crc & 1 != 0 {
                (crc >> 1) ^ 0xedb8_8320
            } else {
                crc >> 1
            };
        }
    }
    crc
}

type Udt70<'a> = (&'a [u8], Option<&'a [u8]>, u16);

fn udt70(rec: &[u8]) -> anyhow::Result<Option<Udt70<'_>>> {
    let kind = u16_at(rec, 2)?;
    let (at, prop) = match kind {
        0x1504 | 0x1505 => (4 + 16, u16_at(rec, 6)?),
        0x1506 => (4 + 8, u16_at(rec, 6)?),
        0x1507 => (4 + 12, u16_at(rec, 6)?),
        _ => return Ok(None),
    };
    let at = if kind == 0x1507 {
        at
    } else {
        at + numeric_len(rec, at)?
    };
    let (name, e) = name_at(rec, at, Gen::Sz)?;
    let unique = if prop & 0x200 != 0 {
        Some(name_at(rec, e, Gen::Sz)?.0)
    } else {
        None
    };
    Ok(Some((name, unique, prop)))
}

fn anonymous70(name: &[u8]) -> bool {
    name == b"<unnamed-tag>"
        || name == b"__unnamed"
        || name.ends_with(b"::<unnamed-tag>")
        || name.ends_with(b"::__unnamed")
}

pub fn type_hash70(rec: &[u8]) -> anyhow::Result<u32> {
    if let Some((name, unique, prop)) = udt70(rec)? {
        let fwd = prop & 0x80 != 0;
        let scoped = prop & 0x100 != 0;
        let anon = unique.is_some() && anonymous70(name);
        if !fwd && !scoped && !anon {
            return Ok(hash_v1(name));
        }
        if let (false, Some(u), false) = (fwd, unique, anon) {
            return Ok(hash_v1(u));
        }
    }
    Ok(jam_crc(rec))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_name_hash_reproduces_vc6s_own_buckets() {
        assert_eq!(lhash(b"__lc_codepage", 4096), 3);
        assert_eq!(lhash(b"iPrimaryLen", 4096), 14);
        assert_eq!(lhash(b"wchar_t", 4096), 18);
        assert_eq!(lhash(b"LPBYTE", 4096), 19);
    }

    #[test]
    fn the_type_hash_covers_the_whole_record() {
        let r0 = [0x0a, 0x00, 0x02, 0x10, 0x20, 0, 0, 0, 0x0a, 0, 0, 0];
        assert_eq!(lhash(&r0, 4096), 0x440);
        let r1 = [
            0x12, 0x00, 0x01, 0x12, 3, 0, 0, 0, 0, 0x10, 0, 0, 0, 0x10, 0, 0, 0x22, 0, 0, 0,
        ];
        assert_eq!(lhash(&r1, 4096), 0x630);
        assert_eq!(type_hash20(&r0, 4096), 0x440);
    }

    #[test]
    fn a_named_complete_udt_hashes_its_length_prefixed_name() {
        let mut s = vec![
            0x2a, 0x00, 0x05, 0x10, 0x06, 0x00, 0x08, 0x00, 0x35, 0x10, 0, 0,
        ];
        s.extend_from_slice(&[0; 8]);
        s.extend_from_slice(&[0x18, 0x00, 0x15]);
        s.extend_from_slice(b"_RTL_CRITICAL_SECTION");
        assert_eq!(
            udt_name20(&s).map(|(n, _)| n),
            Some(&b"_RTL_CRITICAL_SECTION"[..])
        );
        assert_eq!(type_hash20(&s, 4096), 1121);
        let mut e = vec![
            0x1e, 0x00, 0x07, 0x10, 0x02, 0x00, 0x00, 0x00, 0x74, 0, 0, 0,
        ];
        e.extend_from_slice(&[0x66, 0x10, 0, 0, 0x0c]);
        e.extend_from_slice(b"LATENCY_TIME");
        e.extend_from_slice(&[0xf3, 0xf2, 0xf1]);
        assert_eq!(type_hash20(&e, 4096), 2938);
        for (lo, hi) in [(0x80u8, 0x00u8), (0x00, 0x01)] {
            let mut f = s.clone();
            f[6] = lo;
            f[7] = hi;
            assert_eq!(type_hash20(&f, 4096), lhash(&f, 4096));
        }
        let mut a = vec![
            0x1e, 0x00, 0x05, 0x10, 0x02, 0x00, 0x08, 0x00, 0xd2, 0x12, 0, 0,
        ];
        a.extend_from_slice(&[0; 8]);
        a.extend_from_slice(&[0x10, 0x00, 0x09]);
        a.extend_from_slice(b"__unnamed");
        assert_eq!(type_hash20(&a, 4096), lhash(&a, 4096));
    }

    fn jam_crc_init(b: &[u8], init: u32) -> u32 {
        let mut crc = init;
        for byte in b {
            crc ^= u32::from(*byte);
            for _ in 0..8 {
                crc = if crc & 1 != 0 {
                    (crc >> 1) ^ 0xedb8_8320
                } else {
                    crc >> 1
                };
            }
        }
        crc
    }

    #[test]
    fn jam_crc_is_crc32_from_zero_without_the_final_inversion() {
        assert_eq!(jam_crc(b""), 0);
        assert_ne!(jam_crc(b"a"), jam_crc(b"b"));
        assert_eq!(!jam_crc_init(b"123456789", u32::MAX), 0xcbf4_3926);
        assert_eq!(jam_crc(b"123456789"), jam_crc_init(b"123456789", 0));
    }

    #[test]
    fn a_complete_7_0_struct_hashes_its_name_and_a_forward_reference_its_bytes() {
        let mut s = vec![0, 0, 0x05, 0x15, 1, 0, 0, 0];
        s.extend_from_slice(&[0; 12]);
        s.extend_from_slice(&[4, 0]);
        s.extend_from_slice(b"Dev\0");
        assert_eq!(type_hash70(&s).unwrap(), hash_v1(b"Dev"));
        s[6] = 0x80;
        assert_eq!(type_hash70(&s).unwrap(), jam_crc(&s));
        assert_eq!(
            type_hash70(&[0, 0, 1, 0x12]).unwrap(),
            jam_crc(&[0, 0, 1, 0x12])
        );
    }
}
