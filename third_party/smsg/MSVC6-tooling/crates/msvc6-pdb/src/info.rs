use crate::hash::hash_v1;
use crate::le::{bytes, free_slots, u32_at, u32_of, us};
use anyhow::{Context as _, Result, bail};
use md5::Digest as _;
use msvc6_codeview::codec::{Gen, name_at, push_name};
use std::collections::BTreeMap;

pub const VC60: u32 = 19_970_604;
pub const VC70: u32 = 20_000_404;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Info {
    pub version: u32,
    pub signature: u32,
    pub age: u32,
    pub guid: Option<[u8; 16]>,
    pub names: BTreeMap<Vec<u8>, u32>,
    pub tail: Vec<u8>,
}

fn read_bits(s: &[u8], at: &mut usize) -> Result<Vec<u32>> {
    let n = us(u32_at(s, *at)?);
    let start = at.saturating_add(4);
    if n.saturating_mul(4) > s.len().saturating_sub(start) {
        bail!(
            "a bit vector of {n} word(s) at {:#x} runs past stream 1",
            *at
        );
    }
    let mut w = Vec::with_capacity(n);
    for i in 0..n {
        w.push(u32_at(s, start + 4 * i)?);
    }
    *at = start + 4 * n;
    Ok(w)
}

pub fn read_info(s: &[u8]) -> Result<Info> {
    let version = u32_at(s, 0).context("stream 1's version")?;
    let signature = u32_at(s, 4)?;
    let age = u32_at(s, 8)?;
    let (guid, mut at) = if version >= VC70 {
        let mut g = [0u8; 16];
        g.copy_from_slice(bytes(s, 12, 16)?);
        (Some(g), 28)
    } else {
        (None, 12)
    };
    let cb = us(u32_at(s, at)?);
    let buf = bytes(s, at + 4, cb)
        .context("the named-stream map's names")?
        .to_vec();
    at += 4 + cb;
    let size = us(u32_at(s, at)?);
    at += 8;
    let present = read_bits(s, &mut at)?;
    let _deleted = read_bits(s, &mut at)?;
    let mut names = BTreeMap::new();
    let mut seen = 0usize;
    for (w, word) in present.iter().enumerate() {
        for bit in 0..32 {
            if word & (1 << bit) != 0 {
                seen += 1;
                if seen > size {
                    bail!(
                        "the named-stream map says {size} entries and its present bits hold more"
                    );
                }
                let key = us(u32_at(s, at)?);
                let val = u32_at(s, at + 4)?;
                at += 8;
                let (n, _) = name_at(&buf, key, Gen::Sz)
                    .with_context(|| format!("named stream {} of the map", 32 * w + bit))?;
                names.insert(n.to_vec(), val);
            }
        }
    }
    if names.len() != size {
        bail!(
            "the named-stream map says {size} entries and holds {}",
            names.len()
        );
    }
    Ok(Info {
        version,
        signature,
        age,
        guid,
        names,
        tail: s.get(at..).unwrap_or_default().to_vec(),
    })
}

fn named_map(out: &mut Vec<u8>, names: &BTreeMap<Vec<u8>, u32>) -> Result<()> {
    let mut buf = Vec::new();
    let mut keys = Vec::new();
    for (n, v) in names {
        keys.push((u32_of(buf.len(), "name offset")?, n.as_slice(), *v));
        push_name(&mut buf, n, Gen::Sz)?;
    }
    out.extend_from_slice(&u32_of(buf.len(), "names")?.to_le_bytes());
    out.extend_from_slice(&buf);
    let cap = (keys.len() * 3 / 2 + 1).max(1);
    let mut slots: Vec<Option<(u32, u32)>> = vec![None; cap];
    let mut free = free_slots(cap);
    for (key, n, v) in &keys {
        let h = us(hash_v1(n) & 0xffff) % cap;
        let j = free.take_from(h).context("the named-stream map is full")?;
        if let Some(s) = slots.get_mut(j) {
            *s = Some((*key, *v));
        }
    }
    out.extend_from_slice(&u32_of(keys.len(), "map size")?.to_le_bytes());
    out.extend_from_slice(&u32_of(cap, "map capacity")?.to_le_bytes());
    let words = cap.div_ceil(32);
    out.extend_from_slice(&u32_of(words, "words")?.to_le_bytes());
    for w in 0..words {
        let mut word = 0u32;
        for b in 0..32 {
            if slots.get(32 * w + b).is_some_and(Option::is_some) {
                word |= 1 << b;
            }
        }
        out.extend_from_slice(&word.to_le_bytes());
    }
    out.extend_from_slice(&0u32.to_le_bytes());
    for (key, v) in slots.iter().flatten() {
        out.extend_from_slice(&key.to_le_bytes());
        out.extend_from_slice(&v.to_le_bytes());
    }
    Ok(())
}

pub fn write_info70(info: &Info, guid: &[u8; 16]) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    for v in [VC70, info.signature, info.age] {
        out.extend_from_slice(&v.to_le_bytes());
    }
    out.extend_from_slice(guid);
    named_map(&mut out, &info.names)?;
    match info.tail.as_slice() {
        [] => {}
        t @ [_, _, _, _] => {
            let v = u32_at(t, 0)?;
            if matches!(v, 20_091_201 | 20_140_508 | 0x4d54_4f4e | 0x494e_494d) {
                bail!("niMac {v} would read as a 7.0 feature code");
            }
            out.extend_from_slice(t);
        }
        t => bail!(
            "{} byte(s) after the named-stream map; VC6 writes the 4-byte niMac",
            t.len()
        ),
    }
    Ok(out)
}

#[must_use]
pub fn write_info20_empty(signature: u32, age: u32) -> Vec<u8> {
    let mut info = VC60.to_le_bytes().to_vec();
    info.extend_from_slice(&signature.to_le_bytes());
    info.extend_from_slice(&age.to_le_bytes());
    for w in [0u32, 0, 1, 1, 0, 0, 0] {
        info.extend_from_slice(&w.to_le_bytes());
    }
    info
}

pub fn write_info20(signature: u32, age: u32, names: &BTreeMap<Vec<u8>, u32>) -> Result<Vec<u8>> {
    let mut out = VC60.to_le_bytes().to_vec();
    out.extend_from_slice(&signature.to_le_bytes());
    out.extend_from_slice(&age.to_le_bytes());
    named_map(&mut out, names)?;
    out.extend_from_slice(&0u32.to_le_bytes());
    Ok(out)
}

#[must_use]
pub fn guid_of(pdb20: &[u8]) -> [u8; 16] {
    let d = md5::Md5::digest(pdb20);
    let mut u = [0u8; 16];
    u.copy_from_slice(&d);
    if let Some(b) = u.get_mut(6) {
        *b = (*b & 0x0f) | 0x30;
    }
    if let Some(b) = u.get_mut(8) {
        *b = (*b & 0x3f) | 0x80;
    }
    let mut g = u;
    g.get_mut(0..4).map(<[u8]>::reverse);
    g.get_mut(4..6).map(<[u8]>::reverse);
    g.get_mut(6..8).map(<[u8]>::reverse);
    g
}

#[must_use]
pub fn guid_text(g: &[u8; 16]) -> String {
    let le = |r: std::ops::Range<usize>| -> String {
        g.get(r)
            .unwrap_or_default()
            .iter()
            .rev()
            .map(|b| format!("{b:02X}"))
            .collect()
    };
    let be = |r: std::ops::Range<usize>| -> String {
        g.get(r)
            .unwrap_or_default()
            .iter()
            .map(|b| format!("{b:02X}"))
            .collect()
    };
    format!(
        "{{{}-{}-{}-{}-{}}}",
        le(0..4),
        le(4..6),
        le(6..8),
        be(8..10),
        be(10..16)
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_identity_stream_round_trips_with_its_names_and_nimac() {
        let mut names = BTreeMap::new();
        names.insert(b"/LinkInfo".to_vec(), 5);
        names.insert(b"/names".to_vec(), 9);
        let info = Info {
            version: VC60,
            signature: 0x3c20_f5ab,
            age: 3,
            guid: None,
            names,
            tail: 7u32.to_le_bytes().to_vec(),
        };
        let g = guid_of(b"x");
        let back = read_info(&write_info70(&info, &g).unwrap()).unwrap();
        assert_eq!(back.version, VC70);
        assert_eq!(
            (back.signature, back.age, back.guid),
            (0x3c20_f5ab, 3, Some(g))
        );
        assert_eq!(back.names, info.names);
        assert_eq!(back.tail, info.tail);
        let two = read_info(&write_info20(1, 2, &info.names).unwrap()).unwrap();
        assert_eq!((two.version, two.guid, two.names), (VC60, None, info.names));
        let empty = read_info(&write_info20_empty(1, 2)).unwrap();
        assert!(empty.names.is_empty());
        assert_eq!(empty.tail, 0u32.to_le_bytes());
    }

    #[test]
    fn the_guid_is_the_md5_with_version_3_bits() {
        let t = guid_text(&guid_of(b"x"));
        assert_eq!(t.len(), 38);
        assert_eq!(&t[15..16], "3");
        assert!(matches!(&t[20..21], "8" | "9" | "A" | "B"));
        assert_eq!(guid_of(b"x"), guid_of(b"x"));
    }

    #[test]
    fn a_map_that_claims_more_than_it_holds_is_refused() {
        let mut s = write_info20_empty(1, 1);
        s[16..20].copy_from_slice(&5u32.to_le_bytes());
        assert!(read_info(&s).is_err());
        let mut s = write_info20_empty(1, 1);
        s[24..28].copy_from_slice(&0x4000_0000u32.to_le_bytes());
        assert!(read_info(&s).is_err());
    }
}
