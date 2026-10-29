use crate::le::{put, u32_at, u32_of, us};
use anyhow::{Context as _, Result, anyhow, bail};

pub const MAGIC: &[u8; 32] = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0";
pub const BLOCK: usize = 4096;
const NIL: u32 = u32::MAX;

#[must_use]
pub fn is_pdb70(head: &[u8]) -> bool {
    head.starts_with(MAGIC)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Stream {
    pub len: Option<usize>,
    pub blocks: Vec<usize>,
}

#[derive(Debug, Clone)]
pub struct Msf7 {
    data: Vec<u8>,
    pub block_size: usize,
    pub num_blocks: usize,
    pub fpm_block: usize,
    pub streams: Vec<Stream>,
    pub dir_blocks: Vec<usize>,
    pub map_block: usize,
}

fn gather(data: &[u8], bs: usize, blocks: &[usize], size: usize) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(size.min(blocks.len().saturating_mul(bs)));
    for b in blocks {
        let from = b
            .checked_mul(bs)
            .ok_or_else(|| anyhow!("block {b} is out of range"))?;
        let page = data
            .get(from..from.saturating_add(bs))
            .ok_or_else(|| anyhow!("block {b} runs past the file"))?;
        out.extend_from_slice(page);
    }
    if out.len() < size {
        bail!(
            "{} block(s) hold {} bytes, not the {size} the directory says",
            blocks.len(),
            out.len()
        );
    }
    out.truncate(size);
    Ok(out)
}

impl Msf7 {
    pub fn parse(data: Vec<u8>) -> Result<Self> {
        if !is_pdb70(&data) {
            bail!("no MSF 7.00 magic");
        }
        let bs = us(u32_at(&data, 32)?);
        if !matches!(bs, 512 | 1024 | 2048 | 4096) {
            bail!("block size {bs} is not one of 512, 1024, 2048, 4096");
        }
        let fpm_block = us(u32_at(&data, 36)?);
        if fpm_block != 1 && fpm_block != 2 {
            bail!("the free block map is at block {fpm_block}, not 1 or 2");
        }
        let num_blocks = us(u32_at(&data, 40)?);
        let dir_bytes = us(u32_at(&data, 44)?);
        let map_block = us(u32_at(&data, 52)?);
        if num_blocks.checked_mul(bs) != Some(data.len()) {
            bail!(
                "NumBlocks {num_blocks} x {bs} is not the file length {}",
                data.len()
            );
        }
        if !dir_bytes.is_multiple_of(4) {
            bail!("directory size {dir_bytes} is not a multiple of 4");
        }
        if map_block == 0 || map_block >= num_blocks {
            bail!("block map address {map_block} is outside 1..{num_blocks}");
        }
        let n_dir = dir_bytes.div_ceil(bs);
        if n_dir > bs / 4 {
            bail!("{n_dir} directory blocks do not fit one block map");
        }
        let mut dir_blocks = Vec::with_capacity(n_dir);
        for i in 0..n_dir {
            let b = us(u32_at(&data, map_block * bs + 4 * i)?);
            if b >= num_blocks {
                bail!("directory block {b} is past NumBlocks {num_blocks}");
            }
            dir_blocks.push(b);
        }
        let dir = gather(&data, bs, &dir_blocks, dir_bytes).context("the directory")?;
        let count = us(u32_at(&dir, 0).context("the directory's stream count")?);
        if count.saturating_mul(4).saturating_add(4) > dir.len() {
            bail!(
                "the directory counts {count} stream(s), more than its {} bytes hold",
                dir.len()
            );
        }
        let mut lens = Vec::with_capacity(count);
        for i in 0..count {
            let s = u32_at(&dir, 4 + 4 * i)?;
            lens.push((s != NIL).then_some(us(s)));
        }
        let mut at = 4 + 4 * count;
        let mut total = 0usize;
        let mut streams = Vec::with_capacity(count);
        for (i, len) in lens.into_iter().enumerate() {
            let n = len.unwrap_or(0).div_ceil(bs);
            total = total.saturating_add(n);
            if total > num_blocks || at.saturating_add(n.saturating_mul(4)) > dir.len() {
                bail!(
                    "stream {i} of {} bytes needs {n} block(s): past the directory or the file's {num_blocks} block(s)",
                    len.unwrap_or(0)
                );
            }
            let mut bl = Vec::with_capacity(n);
            for _ in 0..n {
                let b = us(u32_at(&dir, at)?);
                if b >= num_blocks {
                    bail!("stream {i} names block {b}, past NumBlocks {num_blocks}");
                }
                bl.push(b);
                at += 4;
            }
            streams.push(Stream { len, blocks: bl });
        }
        if at != dir_bytes {
            bail!("the directory is {dir_bytes} bytes and its contents end at {at}");
        }
        Ok(Self {
            data,
            block_size: bs,
            num_blocks,
            fpm_block,
            streams,
            dir_blocks,
            map_block,
        })
    }

    pub fn stream(&self, index: usize) -> Result<Vec<u8>> {
        let s = self
            .streams
            .get(index)
            .ok_or_else(|| anyhow!("no stream {index} (the file has {})", self.streams.len()))?;
        gather(&self.data, self.block_size, &s.blocks, s.len.unwrap_or(0))
            .with_context(|| format!("stream {index}"))
    }

    #[must_use]
    pub fn bytes(&self) -> &[u8] {
        &self.data
    }

    pub fn free_map(&self) -> Result<Vec<bool>> {
        let bs = self.block_size;
        let bits = 8 * bs;
        let mut out = Vec::with_capacity(self.num_blocks);
        let mut interval = 0usize;
        while out.len() < self.num_blocks {
            let at = interval
                .checked_mul(bs)
                .and_then(|b| b.checked_add(self.fpm_block))
                .and_then(|b| b.checked_mul(bs))
                .context("the free block map is out of range")?;
            let page = self
                .data
                .get(at..at.saturating_add(bs))
                .context("the free block map runs past the file")?;
            for i in 0..bits {
                if out.len() == self.num_blocks {
                    break;
                }
                let byte = page.get(i / 8).copied().unwrap_or(0xff);
                out.push(byte & (1 << (i % 8)) != 0);
            }
            interval += 8;
        }
        Ok(out)
    }

    pub fn used_but_free(&self) -> Result<(usize, usize)> {
        let free = self.free_map()?;
        let mut used: Vec<usize> = self
            .streams
            .iter()
            .flat_map(|s| s.blocks.iter().copied())
            .collect();
        used.extend_from_slice(&self.dir_blocks);
        used.push(self.map_block);
        used.push(0);
        used.push(self.fpm_block);
        let n = used.len();
        let bad = used
            .iter()
            .filter(|b| free.get(**b).copied().unwrap_or(true))
            .count();
        Ok((n, bad))
    }
}

fn is_fpm(b: usize, bs: usize) -> bool {
    matches!(b % bs, 1 | 2)
}

pub fn write(streams: &[Vec<u8>]) -> Result<Vec<u8>> {
    let bs = BLOCK;
    let mut next = 3usize;
    let mut alloc = || {
        while is_fpm(next, bs) {
            next += 1;
        }
        let b = next;
        next += 1;
        b
    };
    let mut lists: Vec<Vec<usize>> = Vec::with_capacity(streams.len());
    for s in streams {
        lists.push((0..s.len().div_ceil(bs)).map(|_| alloc()).collect());
    }
    let mut dir = u32_of(streams.len(), "stream count")?
        .to_le_bytes()
        .to_vec();
    for s in streams {
        dir.extend_from_slice(&u32_of(s.len(), "stream length")?.to_le_bytes());
    }
    for l in &lists {
        for b in l {
            dir.extend_from_slice(&u32_of(*b, "block number")?.to_le_bytes());
        }
    }
    let dir_list: Vec<usize> = (0..dir.len().div_ceil(bs)).map(|_| alloc()).collect();
    if dir_list.len() > bs / 4 {
        bail!(
            "the directory needs {} blocks and one block map holds {}",
            dir_list.len(),
            bs / 4
        );
    }
    let map_block = alloc();
    let mut num_blocks = next;
    if matches!(num_blocks % bs, 1 | 2) {
        num_blocks = num_blocks - num_blocks % bs + 3;
    }
    let mut out = vec![0u8; num_blocks * bs];
    for (s, l) in streams.iter().zip(&lists) {
        for (chunk, b) in s.chunks(bs).zip(l) {
            put(&mut out, *b * bs, chunk)?;
        }
    }
    for (chunk, b) in dir.chunks(bs).zip(&dir_list) {
        put(&mut out, *b * bs, chunk)?;
    }
    let mut map = Vec::with_capacity(4 * dir_list.len());
    for b in &dir_list {
        map.extend_from_slice(&u32_of(*b, "block number")?.to_le_bytes());
    }
    put(&mut out, map_block * bs, &map)?;
    let bits = 8 * bs;
    let mut interval = 0usize;
    while interval * bs < num_blocks {
        let mut page = vec![0xffu8; bs];
        if interval.is_multiple_of(8) {
            let first = (interval / 8) * bits;
            for (i, byte) in page.iter_mut().enumerate() {
                let mut v = 0u8;
                for bit in 0..8 {
                    if first + 8 * i + bit >= num_blocks {
                        v |= 1 << bit;
                    }
                }
                *byte = v;
            }
        }
        put(&mut out, (interval * bs + 1) * bs, &page)?;
        put(&mut out, (interval * bs + 2) * bs, &page)?;
        interval += 1;
    }
    let mut sb = MAGIC.to_vec();
    for v in [
        u32_of(bs, "block size")?,
        1,
        u32_of(num_blocks, "block count")?,
        u32_of(dir.len(), "directory size")?,
        0,
        u32_of(map_block, "block map")?,
    ] {
        sb.extend_from_slice(&v.to_le_bytes());
    }
    put(&mut out, 0, &sb)?;
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_written_container_reads_back_stream_for_stream() {
        let streams = vec![
            Vec::new(),
            b"info".to_vec(),
            vec![7u8; 5000],
            Vec::new(),
            (0..=255u8).cycle().take(3 * BLOCK + 17).collect(),
        ];
        let bytes = write(&streams).unwrap();
        assert_eq!(bytes.len() % BLOCK, 0);
        let m = Msf7::parse(bytes).unwrap();
        assert_eq!(m.streams.len(), streams.len());
        for (i, s) in streams.iter().enumerate() {
            assert_eq!(&m.stream(i).unwrap(), s, "stream {i}");
        }
        assert!(
            m.streams
                .iter()
                .all(|s| !s.blocks.contains(&1) && !s.blocks.contains(&2))
        );
        assert!(m.free_map().unwrap().iter().all(|f| !f));
        assert_eq!(m.used_but_free().unwrap().1, 0);
    }

    #[test]
    fn a_container_past_one_interval_skips_the_next_intervals_map_blocks() {
        let streams = vec![Vec::new(), vec![1u8; (BLOCK + 1) * BLOCK]];
        let m = Msf7::parse(write(&streams).unwrap()).unwrap();
        let blocks = &m.streams[1].blocks;
        assert!(!blocks.contains(&(BLOCK + 1)) && !blocks.contains(&(BLOCK + 2)));
        assert_eq!(m.stream(1).unwrap().len(), (BLOCK + 1) * BLOCK);
        assert_eq!(m.used_but_free().unwrap().1, 0);
    }

    #[test]
    fn a_damaged_superblock_or_directory_is_refused() {
        let good = write(&[Vec::new(), b"x".to_vec()]).unwrap();
        let mut b = good.clone();
        b[36] = 3;
        assert!(Msf7::parse(b).is_err());
        let mut b = good.clone();
        b.truncate(b.len() - 1);
        assert!(Msf7::parse(b).is_err());
        let mut b = good.clone();
        b[44..48].copy_from_slice(&0x7fff_fffcu32.to_le_bytes());
        assert!(Msf7::parse(b).is_err());
        let m = Msf7::parse(good.clone()).unwrap();
        let d = m.dir_blocks[0] * BLOCK;
        let mut b = good.clone();
        b[d..d + 4].copy_from_slice(&0x0fff_ffffu32.to_le_bytes());
        assert!(Msf7::parse(b).is_err());
        let mut b = good;
        b[d + 8..d + 12].copy_from_slice(&0x7fff_0000u32.to_le_bytes());
        assert!(Msf7::parse(b).is_err());
    }
}
