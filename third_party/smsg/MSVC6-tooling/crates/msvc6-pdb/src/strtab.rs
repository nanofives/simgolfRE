use crate::hash::hash_v1;
use crate::le::{bytes, free_slots, u32_at, u32_of, us};
use anyhow::{Context as _, Result, bail};
use msvc6_codeview::codec::{Gen, name_at, push_name};
use std::collections::BTreeMap;

pub const MAGIC: u32 = 0xeffe_effe;

#[derive(Debug, Clone, Default)]
pub struct StrTab {
    buf: Vec<u8>,
    ids: BTreeMap<Vec<u8>, u32>,
    order: Vec<u32>,
}

#[must_use]
pub fn bucket_count(n: usize) -> usize {
    let mut nb = 1usize;
    for count in 1..=n {
        if nb * 3 / 4 < count {
            nb = nb * 3 / 2 + 1;
        }
    }
    nb
}

impl StrTab {
    #[must_use]
    pub fn new() -> Self {
        Self {
            buf: vec![0],
            ids: BTreeMap::new(),
            order: Vec::new(),
        }
    }

    pub fn seeded(s: &[u8]) -> Result<Self> {
        if u32_at(s, 0)? != MAGIC || u32_at(s, 4)? != 1 {
            bail!("the string table is not a version-1 0xeffeeffe table");
        }
        let cb = us(u32_at(s, 8)?);
        let buf = bytes(s, 12, cb)?.to_vec();
        if buf.first() != Some(&0) {
            bail!("the string table does not start with the empty string");
        }
        let nb = us(u32_at(s, 12 + cb)?);
        if nb.saturating_mul(4).saturating_add(20 + cb) != s.len() {
            bail!(
                "the string table's {nb} bucket(s) and {cb} string bytes do not fill its {} bytes",
                s.len()
            );
        }
        let mut me = Self {
            buf,
            ids: BTreeMap::new(),
            order: Vec::new(),
        };
        for i in 0..nb {
            let id = u32_at(s, 16 + cb + 4 * i)?;
            if id == 0 {
                continue;
            }
            let (n, _) = name_at(&me.buf, us(id), Gen::Sz)?;
            if me.ids.insert(n.to_vec(), id).is_none() {
                me.order.push(id);
            }
        }
        let count = us(u32_at(s, 16 + cb + 4 * nb)?);
        if me.order.len() != count {
            bail!(
                "the string table counts {count} strings and its buckets hold {}",
                me.order.len()
            );
        }
        Ok(me)
    }

    pub fn id(&mut self, s: &[u8]) -> Result<u32> {
        if let Some(i) = self.ids.get(s) {
            return Ok(*i);
        }
        let id = u32_of(self.buf.len(), "string table offset")?;
        push_name(&mut self.buf, s, Gen::Sz)?;
        self.ids.insert(s.to_vec(), id);
        self.order.push(id);
        Ok(id)
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.order.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.order.is_empty()
    }

    pub fn stream(&self) -> Result<Vec<u8>> {
        self.stream_and_steps().map(|(s, _)| s)
    }

    pub fn stream_and_steps(&self) -> Result<(Vec<u8>, usize)> {
        let nb = bucket_count(self.order.len());
        let mut buckets = vec![0u32; nb];
        let mut free = free_slots(nb);
        for (s, id) in &self.ids {
            let slot = free
                .take_from(us(hash_v1(s)) % nb)
                .context("string table full")?;
            if let Some(b) = buckets.get_mut(slot) {
                *b = *id;
            }
        }
        let mut out = Vec::with_capacity(20 + self.buf.len() + 4 * nb);
        out.extend_from_slice(&MAGIC.to_le_bytes());
        out.extend_from_slice(&1u32.to_le_bytes());
        out.extend_from_slice(&u32_of(self.buf.len(), "string bytes")?.to_le_bytes());
        out.extend_from_slice(&self.buf);
        out.extend_from_slice(&u32_of(nb, "buckets")?.to_le_bytes());
        for b in buckets {
            out.extend_from_slice(&b.to_le_bytes());
        }
        out.extend_from_slice(&u32_of(self.order.len(), "string count")?.to_le_bytes());
        Ok((out, free.steps()))
    }
}

pub fn walk(s: &[u8]) -> Result<BTreeMap<u32, Vec<u8>>> {
    let cb = us(u32_at(s, 8)?);
    let buf = bytes(s, 12, cb)?;
    let mut out = BTreeMap::new();
    let mut at = 0usize;
    while at < buf.len() {
        let (n, e) = name_at(buf, at, Gen::Sz)?;
        out.insert(u32_of(at, "string offset")?, n.to_vec());
        at = e;
    }
    Ok(out)
}

#[must_use]
pub fn run_starts(buckets: &[u32]) -> Vec<Option<usize>> {
    let nb = buckets.len();
    let mut out = vec![None; nb];
    let Some(zero) = buckets.iter().position(|b| *b == 0) else {
        return out;
    };
    let mut start = None;
    for k in 1..=nb {
        let j = (zero + k) % nb;
        if buckets.get(j).copied().unwrap_or(0) == 0 {
            start = None;
            continue;
        }
        let s = *start.get_or_insert(j);
        if let Some(o) = out.get_mut(j) {
            *o = Some(s);
        }
    }
    out
}

#[must_use]
pub fn reachable(home: usize, j: usize, run_start: Option<usize>, nb: usize) -> bool {
    match run_start {
        None => true,
        Some(r) => {
            let d = |a: usize, b: usize| (b + nb - a) % nb;
            d(r, home) <= d(r, j)
        }
    }
}

pub fn read(s: &[u8]) -> Result<BTreeMap<u32, Vec<u8>>> {
    if u32_at(s, 0)? != MAGIC {
        bail!("/names has no 0xeffeeffe signature");
    }
    let cb = us(u32_at(s, 8)?);
    let buf = bytes(s, 12, cb)?;
    let nb = us(u32_at(s, 12 + cb)?);
    if nb.saturating_mul(4).saturating_add(20 + cb) != s.len() {
        bail!("/names has bytes past its string count, or too few for its buckets");
    }
    let mut ids = Vec::with_capacity(nb);
    for i in 0..nb {
        ids.push(u32_at(s, 16 + cb + 4 * i)?);
    }
    let count = us(u32_at(s, 16 + cb + 4 * nb)?);
    let mut out = BTreeMap::new();
    for id in ids.iter().filter(|i| **i != 0) {
        let (n, _) = name_at(buf, us(*id), Gen::Sz)?;
        out.insert(*id, n.to_vec());
    }
    if out.len() != count {
        bail!(
            "/names counts {count} strings and its buckets hold {}",
            out.len()
        );
    }
    let runs = run_starts(&ids);
    let mut at: BTreeMap<u32, Vec<usize>> = BTreeMap::new();
    for (j, id) in ids.iter().enumerate().filter(|(_, i)| **i != 0) {
        at.entry(*id).or_default().push(j);
    }
    for (id, n) in &out {
        let home = us(hash_v1(n)) % nb.max(1);
        let ok = at.get(id).is_some_and(|ps| {
            ps.iter()
                .any(|j| reachable(home, *j, runs.get(*j).copied().flatten(), nb))
        });
        if !ok {
            bail!(
                "/names: {:?} is not reachable from its hash bucket",
                msvc6_base::srcio::decode(n)
            );
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_string_table_finds_every_name_from_its_hash() {
        let mut t = StrTab::new();
        let a = t.id(b"c:\\src\\a.cpp").unwrap();
        let b = t.id(b"c:\\src\\b.h").unwrap();
        assert_eq!(t.id(b"c:\\src\\a.cpp").unwrap(), a);
        assert_eq!(a, 1);
        let s = t.stream().unwrap();
        let m = read(&s).unwrap();
        assert_eq!(m.get(&b).unwrap(), b"c:\\src\\b.h");
        assert_eq!(m.len(), 2);
        let again = StrTab::seeded(&s).unwrap();
        assert_eq!(again.stream().unwrap(), s);
        assert_eq!(walk(&s).unwrap().len(), 3);
    }

    #[test]
    fn a_name_moved_past_an_empty_bucket_is_unreachable() {
        let mut t = StrTab::new();
        t.id(b"x.c").unwrap();
        let s = t.stream().unwrap();
        let cb = us(u32_at(&s, 8).unwrap());
        let nb = us(u32_at(&s, 12 + cb).unwrap());
        assert!(nb >= 2);
        let at = |i: usize| 16 + cb + 4 * i;
        let home = (0..nb).find(|i| u32_at(&s, at(*i)).unwrap() != 0).unwrap();
        let mut b = s.clone();
        let id = u32_at(&s, at(home)).unwrap();
        b[at(home)..at(home) + 4].copy_from_slice(&0u32.to_le_bytes());
        let other = (home + 1) % nb;
        b[at(other)..at(other) + 4].copy_from_slice(&id.to_le_bytes());
        assert!(read(&b).is_err());
    }

    #[test]
    fn run_starts_follow_the_ring() {
        assert_eq!(
            run_starts(&[1, 0, 2, 3]),
            vec![Some(2), None, Some(2), Some(2)]
        );
        assert_eq!(run_starts(&[1, 1]), vec![None, None]);
        assert!(reachable(2, 0, Some(2), 4));
        assert!(!reachable(0, 3, Some(2), 4));
    }

    #[test]
    fn many_names_in_one_bucket_stay_linear() {
        let mut t = StrTab::new();
        for i in 0..20_000u32 {
            t.id(format!("n{i}").as_bytes()).unwrap();
        }
        let (s, steps) = t.stream_and_steps().unwrap();
        assert_eq!(read(&s).unwrap().len(), 20_000);
        assert!(steps <= 5 * 20_000, "{steps} slot steps for 20000 names");
    }
}
