use crate::hash::{BUCKETS20, hash_v1, lhash};
use crate::le::{u32_at, u32_of, us};
use anyhow::{Context as _, Result, bail};
use msvc6_codeview::codec::{Gen, name_at, numeric_len, sym_name};
use std::collections::BTreeMap;

pub const HEADS20: usize = 4097;
pub const GSI70_VERSION: u32 = 0xeffe_0000 + 19_990_810;
pub const IPHR_HASH: u32 = 4096;
pub const BITMAP_WORDS: usize = (4096 + 1usize).div_ceil(32);
pub const RECORD_SIZE: usize = 12;

pub type Entry = (u32, u32, Vec<u8>);

pub fn read20(hash: &[u8]) -> Result<Vec<(u32, u32)>> {
    let body = hash
        .len()
        .checked_sub(4 * HEADS20)
        .context("a GSI hash shorter than its 4097 bucket heads")?;
    if !body.is_multiple_of(8) {
        bail!("a GSI hash whose records are {body} bytes, not a multiple of 8");
    }
    let mut out = Vec::with_capacity(body / 8);
    for i in 0..body / 8 {
        let off = u32_at(hash, 8 * i)?
            .checked_sub(1)
            .context("a hash record offset of 0")?;
        out.push((off, u32_at(hash, 8 * i + 4)?));
    }
    Ok(out)
}

pub fn write20(entries: &[(u32, &[u8])]) -> Result<Vec<u8>> {
    let mut keyed: Vec<(u32, usize, u32)> = entries
        .iter()
        .enumerate()
        .map(|(i, (off, name))| (lhash(name, BUCKETS20), i, *off))
        .collect();
    keyed.sort_unstable();
    let mut out = Vec::with_capacity(keyed.len() * 8 + 4 * HEADS20);
    let mut heads = vec![-1i32; HEADS20];
    for (i, (bucket, _, off)) in keyed.iter().enumerate() {
        if let Some(h) = heads.get_mut(us(*bucket))
            && *h < 0
        {
            *h = i32::try_from(i * RECORD_SIZE).context("hash record index")?;
        }
        out.extend_from_slice(&off.checked_add(1).context("offset")?.to_le_bytes());
        out.extend_from_slice(&1u32.to_le_bytes());
    }
    for h in heads {
        out.extend_from_slice(&h.to_le_bytes());
    }
    Ok(out)
}

pub fn heads20(hash: &[u8]) -> Result<Vec<i64>> {
    let body = hash
        .len()
        .checked_sub(4 * HEADS20)
        .context("a GSI hash shorter than its 4097 bucket heads")?;
    (0..HEADS20)
        .map(|b| {
            Ok(i64::from(i32::from_le_bytes(
                u32_at(hash, body + 4 * b)?.to_le_bytes(),
            )))
        })
        .collect()
}

#[must_use]
pub fn gsi_cmp(a: &[u8], b: &[u8]) -> std::cmp::Ordering {
    a.len().cmp(&b.len()).then_with(|| {
        if a.is_ascii() && b.is_ascii() {
            a.iter()
                .map(u8::to_ascii_lowercase)
                .cmp(b.iter().map(u8::to_ascii_lowercase))
        } else {
            a.cmp(b)
        }
    })
}

pub fn write70(entries: &[Entry]) -> Result<Vec<u8>> {
    if entries.is_empty() {
        let mut out = u32::MAX.to_le_bytes().to_vec();
        out.extend_from_slice(&GSI70_VERSION.to_le_bytes());
        out.extend_from_slice(&[0u8; 8]);
        return Ok(out);
    }
    let mut keyed: Vec<(u32, &Entry)> = entries
        .iter()
        .map(|e| (hash_v1(&e.2) % IPHR_HASH, e))
        .collect();
    keyed.sort_by(|(ba, a), (bb, b)| {
        ba.cmp(bb)
            .then_with(|| gsi_cmp(&a.2, &b.2))
            .then_with(|| a.0.cmp(&b.0))
    });
    let mut recs = Vec::with_capacity(8 * keyed.len());
    let mut starts: BTreeMap<u32, usize> = BTreeMap::new();
    for (i, (bucket, (off, cref, _))) in keyed.iter().enumerate() {
        starts.entry(*bucket).or_insert(i);
        recs.extend_from_slice(&(off.checked_add(1).context("offset")?).to_le_bytes());
        recs.extend_from_slice(&cref.to_le_bytes());
    }
    let mut bitmap = vec![0u32; BITMAP_WORDS];
    let mut heads = Vec::with_capacity(4 * starts.len());
    for (bucket, first) in &starts {
        let b = us(*bucket);
        if let Some(w) = bitmap.get_mut(b / 32) {
            *w |= 1 << (b % 32);
        }
        heads.extend_from_slice(&u32_of(first * RECORD_SIZE, "bucket start")?.to_le_bytes());
    }
    let mut out = Vec::with_capacity(16 + recs.len() + 4 * BITMAP_WORDS + heads.len());
    out.extend_from_slice(&u32::MAX.to_le_bytes());
    out.extend_from_slice(&GSI70_VERSION.to_le_bytes());
    out.extend_from_slice(&u32_of(recs.len(), "hash records")?.to_le_bytes());
    out.extend_from_slice(&u32_of(4 * BITMAP_WORDS + heads.len(), "hash buckets")?.to_le_bytes());
    out.extend_from_slice(&recs);
    for w in bitmap {
        out.extend_from_slice(&w.to_le_bytes());
    }
    out.extend_from_slice(&heads);
    Ok(out)
}

pub fn read70(hash: &[u8]) -> Result<Vec<(u32, u32, u32)>> {
    if u32_at(hash, 0)? != u32::MAX || u32_at(hash, 4)? != GSI70_VERSION {
        bail!("not a 7.0 GSI hash header");
    }
    let hr = us(u32_at(hash, 8)?);
    let nb = us(u32_at(hash, 12)?);
    if !hr.is_multiple_of(8) || 16usize.saturating_add(hr).saturating_add(nb) != hash.len() {
        bail!(
            "GSI hash: {hr} record bytes + {nb} bucket bytes do not fill {} bytes",
            hash.len()
        );
    }
    let n = hr / 8;
    if n == 0 && nb == 0 {
        return Ok(Vec::new());
    }
    if n == 0 {
        bail!("GSI hash: no records and {nb} bucket bytes: an empty 7.0 GSI is its header alone");
    }
    let mut buckets = Vec::new();
    for w in 0..BITMAP_WORDS {
        let word = u32_at(hash, 16 + hr + 4 * w)?;
        for bit in 0..32 {
            if word & (1 << bit) != 0 {
                buckets.push(u32_of(32 * w + bit, "bucket")?);
            }
        }
    }
    let heads_at = 16 + hr + 4 * BITMAP_WORDS;
    if heads_at + 4 * buckets.len() != hash.len() {
        bail!(
            "GSI hash: {} set bucket bits, {} bucket bytes",
            buckets.len(),
            hash.len().saturating_sub(heads_at)
        );
    }
    let mut starts = Vec::with_capacity(buckets.len());
    for i in 0..buckets.len() {
        let v = us(u32_at(hash, heads_at + 4 * i)?);
        if !v.is_multiple_of(RECORD_SIZE) {
            bail!("a bucket start of {v}, not a multiple of {RECORD_SIZE}");
        }
        starts.push(v / RECORD_SIZE);
    }
    if starts.windows(2).any(|w| w.first() >= w.last()) || starts.last().is_some_and(|s| *s >= n) {
        bail!("GSI bucket starts are not ascending inside the records");
    }
    let mut out = Vec::with_capacity(n);
    for i in 0..n {
        let k = starts.partition_point(|s| *s <= i);
        let bucket = k
            .checked_sub(1)
            .and_then(|k| buckets.get(k))
            .copied()
            .context("a hash record before the first bucket")?;
        let off = u32_at(hash, 16 + 8 * i)?
            .checked_sub(1)
            .context("a hash record offset of 0")?;
        out.push((off, u32_at(hash, 16 + 8 * i + 4)?, bucket));
    }
    Ok(out)
}

pub fn hashed_name70(kind: u16, payload: &[u8]) -> Result<Vec<u8>> {
    let at = match kind {
        0x110e | 0x110d | 0x110c | 0x1112 | 0x1113 => 10,
        0x1108 => 4,
        0x1107 => 4 + numeric_len(payload, 4)?,
        0x1125..=0x1127 => 10,
        _ => bail!(
            "a hashed record of kind {kind:#06x} ({}) has no name rule",
            sym_name(kind)
        ),
    };
    Ok(name_at(payload, at, Gen::Sz)?.0.to_vec())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_7_0_gsi_round_trips_and_buckets_by_name() {
        let entries = vec![
            (0u32, 1u32, b"main".to_vec()),
            (16, 1, b"Main".to_vec()),
            (32, 1, b"_foo".to_vec()),
        ];
        let back = read70(&write70(&entries).unwrap()).unwrap();
        assert_eq!(back.len(), 3);
        for (off, cref, bucket) in &back {
            let e = entries.iter().find(|e| e.0 == *off).unwrap();
            assert_eq!(*cref, 1);
            assert_eq!(*bucket, hash_v1(&e.2) % IPHR_HASH);
        }
        assert_eq!(back[0].2, back[1].2);
    }

    #[test]
    fn the_2_0_gsi_round_trips_with_its_heads() {
        let e: Vec<(u32, &[u8])> = vec![(0, b"__lc_codepage"), (20, b"iPrimaryLen"), (40, b"x")];
        let h = write20(&e).unwrap();
        let back = read20(&h).unwrap();
        assert_eq!(back.len(), 3);
        assert_eq!(back[0], (0, 1));
        let heads = heads20(&h).unwrap();
        assert_eq!(heads[3], 0);
        assert_eq!(heads[14], 12);
        assert!(read20(&h[..10]).is_err());
    }

    #[test]
    fn a_malformed_7_0_gsi_is_refused() {
        let good = write70(&[(0, 1, b"a".to_vec())]).unwrap();
        let mut b = good.clone();
        b[8..12].copy_from_slice(&0xffff_fff8u32.to_le_bytes());
        assert!(read70(&b).is_err());
        let mut b = good.clone();
        let last = b.len() - 4;
        b[last..].copy_from_slice(&5u32.to_le_bytes());
        assert!(read70(&b).is_err());
        assert!(read70(&good[..4]).is_err());
        let empty = write70(&[]).unwrap();
        assert_eq!(read70(&empty).unwrap(), []);
        for bits in [0u32, 1] {
            let mut b = empty.clone();
            b[12..16].copy_from_slice(&u32_of(4 * BITMAP_WORDS, "bitmap").unwrap().to_le_bytes());
            b.extend_from_slice(&bits.to_le_bytes());
            b.resize(16 + 4 * BITMAP_WORDS, 0);
            let e = read70(&b).unwrap_err();
            assert!(format!("{e:#}").contains("no records"), "{e:#}");
        }
    }
}
