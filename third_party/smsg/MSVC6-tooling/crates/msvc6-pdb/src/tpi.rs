use crate::hash::{BUCKETS20, TPI70_BUCKETS, type_hash20, type_hash70};
use crate::le::{bytes, u16_at, u32_at, u32_of, us};
use anyhow::{Context as _, Result, bail};

pub const V50: u32 = 19_961_031;
pub const V80: u32 = 20_040_203;
pub const HEADER: usize = 56;
pub const TI_MIN: u32 = 0x1000;
const SEEK_EVERY: usize = 8192;

#[derive(Debug, Clone)]
pub struct Tpi<'a> {
    pub version: u32,
    pub ti_min: u32,
    pub ti_max: u32,
    pub hash_stream: u16,
    pub hash_aux: u16,
    pub key_size: u32,
    pub buckets: u32,
    pub vals: (u32, u32),
    pub offs: (u32, u32),
    pub adj: (u32, u32),
    pub records: Vec<&'a [u8]>,
}

pub fn read(s: &[u8]) -> Result<Tpi<'_>> {
    let hdr = us(u32_at(s, 4).context("the TPI header")?);
    if hdr != HEADER {
        bail!("TPI header is {hdr} bytes, not {HEADER}");
    }
    let gprec = us(u32_at(s, 16)?);
    let end = hdr.saturating_add(gprec);
    if end > s.len() {
        bail!("the TPI's {gprec} record bytes run past the stream");
    }
    let mut records = Vec::new();
    let mut at = hdr;
    while at < end {
        let len = usize::from(u16_at(s, at)?) + 2;
        if len < 4 {
            bail!("the type record at {at:#x} has length {}", len - 2);
        }
        records.push(bytes(s, at, len)?);
        at += len;
    }
    let me = Tpi {
        version: u32_at(s, 0)?,
        ti_min: u32_at(s, 8)?,
        ti_max: u32_at(s, 12)?,
        hash_stream: u16_at(s, 20)?,
        hash_aux: u16_at(s, 22)?,
        key_size: u32_at(s, 24)?,
        buckets: u32_at(s, 28)?,
        vals: (u32_at(s, 32)?, u32_at(s, 36)?),
        offs: (u32_at(s, 40)?, u32_at(s, 44)?),
        adj: (u32_at(s, 48)?, u32_at(s, 52)?),
        records,
    };
    if u32_of(me.records.len(), "type count")? != me.ti_max.saturating_sub(me.ti_min) {
        bail!(
            "the TPI holds {} records for type indices {:#x}..{:#x}",
            me.records.len(),
            me.ti_min,
            me.ti_max
        );
    }
    if at != end || end != s.len() {
        bail!(
            "the TPI's records end at {at:#x}, its header says {end:#x}, the stream is {:#x}",
            s.len()
        );
    }
    Ok(me)
}

fn seek_pairs(records: &[Vec<u8>], ti_min: u32) -> Result<Vec<u8>> {
    let mut pairs = Vec::new();
    let mut at = 0usize;
    let mut last: Option<usize> = None;
    for (i, r) in records.iter().enumerate() {
        if last.is_none_or(|l| at >= l + SEEK_EVERY) {
            pairs.extend_from_slice(&(ti_min + u32_of(i, "type index")?).to_le_bytes());
            pairs.extend_from_slice(&u32_of(at, "type offset")?.to_le_bytes());
            last = Some(at);
        }
        at += r.len();
    }
    Ok(pairs)
}

fn header(
    version: u32,
    records: &[Vec<u8>],
    ti_min: u32,
    hash_stream: u16,
    (key, buckets): (u32, u32),
    (vals_len, offs_len): (usize, usize),
) -> Result<Vec<u8>> {
    let gprec: usize = records.iter().map(Vec::len).sum();
    let mut s = Vec::with_capacity(HEADER + gprec);
    for v in [
        version,
        u32_of(HEADER, "TPI header")?,
        ti_min,
        ti_min + u32_of(records.len(), "type count")?,
        u32_of(gprec, "type bytes")?,
    ] {
        s.extend_from_slice(&v.to_le_bytes());
    }
    s.extend_from_slice(&hash_stream.to_le_bytes());
    s.extend_from_slice(&0xffffu16.to_le_bytes());
    s.extend_from_slice(&key.to_le_bytes());
    s.extend_from_slice(&buckets.to_le_bytes());
    for (o, l) in [
        (0, vals_len),
        (vals_len, offs_len),
        (vals_len + offs_len, 0),
    ] {
        s.extend_from_slice(&u32_of(o, "hash offset")?.to_le_bytes());
        s.extend_from_slice(&u32_of(l, "hash length")?.to_le_bytes());
    }
    for r in records {
        s.extend_from_slice(r);
    }
    Ok(s)
}

pub fn write20(records: &[Vec<u8>], hash_stream: u16) -> Result<(Vec<u8>, Vec<u8>)> {
    let mut hash = Vec::with_capacity(2 * records.len() + 64);
    for t in records {
        let h = u16::try_from(type_hash20(t, BUCKETS20)).context("type hash")?;
        hash.extend_from_slice(&h.to_le_bytes());
    }
    let vals_len = hash.len();
    let pairs = seek_pairs(records, TI_MIN)?;
    let offs_len = pairs.len();
    hash.extend_from_slice(&pairs);
    let s = header(
        V50,
        records,
        TI_MIN,
        hash_stream,
        (2, BUCKETS20),
        (vals_len, offs_len),
    )?;
    Ok((s, hash))
}

pub fn write70(records: &[Vec<u8>], ti_min: u32, hash_stream: u16) -> Result<(Vec<u8>, Vec<u8>)> {
    let mut hash = Vec::with_capacity(4 * records.len() + 64);
    for r in records {
        hash.extend_from_slice(&(type_hash70(r)? % TPI70_BUCKETS).to_le_bytes());
    }
    let vals_len = hash.len();
    let pairs = seek_pairs(records, ti_min)?;
    let offs_len = pairs.len();
    hash.extend_from_slice(&pairs);
    let s = header(
        V80,
        records,
        ti_min,
        hash_stream,
        (4, TPI70_BUCKETS),
        (vals_len, offs_len),
    )?;
    Ok((s, hash))
}

pub fn stored_hash(t: &Tpi<'_>, hash: &[u8], i: usize) -> Option<u32> {
    let base = us(t.vals.0);
    if t.key_size == 2 {
        u16_at(hash, base.checked_add(2 * i)?).ok().map(u32::from)
    } else {
        u32_at(hash, base.checked_add(4 * i)?).ok()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn recs() -> Vec<Vec<u8>> {
        vec![
            vec![0x0a, 0x00, 0x02, 0x10, 0x20, 0, 0, 0, 0x0a, 0, 0, 0],
            vec![6, 0, 0x01, 0x12, 0, 0, 0, 0],
        ]
    }

    #[test]
    fn a_tpi_writes_and_reads_back_in_both_generations() {
        let (s, h) = write20(&recs(), 4).unwrap();
        let t = read(&s).unwrap();
        assert_eq!(
            (t.version, t.ti_min, t.ti_max, t.key_size),
            (V50, 0x1000, 0x1002, 2)
        );
        assert_eq!(t.records.len(), 2);
        assert_eq!(stored_hash(&t, &h, 0), Some(0x440));
        let (s7, h7) = write70(&recs(), 0x1000, 5).unwrap();
        let t7 = read(&s7).unwrap();
        assert_eq!(
            (t7.version, t7.key_size, t7.buckets),
            (V80, 4, TPI70_BUCKETS)
        );
        assert_eq!(h7.len(), 4 * 2 + 8);
    }

    #[test]
    fn a_tpi_that_does_not_tile_or_count_is_refused() {
        let (s, _) = write20(&recs(), 4).unwrap();
        let mut b = s.clone();
        b[12..16].copy_from_slice(&0x1003u32.to_le_bytes());
        assert!(read(&b).is_err());
        let mut b = s.clone();
        b[16..20].copy_from_slice(&0xffff_ff00u32.to_le_bytes());
        assert!(read(&b).is_err());
        let mut b = s.clone();
        b[56..58].copy_from_slice(&0u16.to_le_bytes());
        assert!(read(&b).is_err());
        let mut b = s;
        b.push(0);
        assert!(read(&b).is_err());
    }
}
