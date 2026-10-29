use crate::le::{put, u16_at, u16_of, u32_at, u32_of, us};
use anyhow::{Context as _, Result, anyhow, bail};

pub const MAGIC: &[u8] = b"Microsoft C/C++ program database 2.00\r\n\x1aJG";
pub const HEADER_PAGES_AT: usize = 60;
pub const NIL: u32 = u32::MAX;
pub const MAX_PAGES: usize = 0x7fff;

#[must_use]
pub fn is_pdb20(head: &[u8]) -> bool {
    head.starts_with(MAGIC)
}

#[must_use]
pub fn fpm_span(page_size: usize) -> usize {
    match page_size {
        0x200 => 16,
        0x400 => 8,
        0x800 => 4,
        _ => 1,
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Stream {
    pub len: Option<usize>,
    pub pages: Vec<usize>,
}

impl Stream {
    #[must_use]
    pub fn size(&self) -> usize {
        self.len.unwrap_or(0)
    }
}

#[derive(Debug, Clone)]
pub struct Msf2 {
    data: Vec<u8>,
    pub page_size: usize,
    pub pages_used: usize,
    pub streams: Vec<Stream>,
    pub dir_trailing: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Unheld {
    Page(usize),
    StreamSlack(usize),
    DirectorySlack,
    HeaderTail,
    HeaderReserved,
    DirectoryReserved(usize),
}

impl std::fmt::Display for Unheld {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Page(p) => write!(f, "page {p}, which no stream uses"),
            Self::StreamSlack(i) => write!(f, "stream {i}'s slack past its length"),
            Self::DirectorySlack => f.write_str("the directory's slack past its length"),
            Self::HeaderTail => f.write_str("the header page past the directory page list"),
            Self::HeaderReserved => f.write_str("the MSF header's reserved word (offset 56)"),
            Self::DirectoryReserved(i) => write!(f, "directory entry {i}'s reserved word"),
        }
    }
}

fn gather(data: &[u8], page_size: usize, pages: &[usize], size: usize) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(size.min(pages.len().saturating_mul(page_size)));
    for p in pages {
        let from = p
            .checked_mul(page_size)
            .ok_or_else(|| anyhow!("page {p} is out of range"))?;
        let page = data
            .get(from..from.saturating_add(page_size))
            .ok_or_else(|| anyhow!("page {p} runs past the file"))?;
        out.extend_from_slice(page);
    }
    if out.len() < size {
        bail!(
            "{} page(s) of {page_size} hold {} bytes, not the {size} the directory says",
            pages.len(),
            out.len()
        );
    }
    out.truncate(size);
    Ok(out)
}

impl Msf2 {
    pub fn parse(data: Vec<u8>) -> Result<Self> {
        if !is_pdb20(&data) {
            bail!("not a PDB 2.0 file: no `Microsoft C/C++ program database 2.00` magic");
        }
        let page_size = us(u32_at(&data, 44)?);
        if !matches!(page_size, 0x200 | 0x400 | 0x800 | 0x1000) {
            bail!("page size {page_size} is not one of 512, 1024, 2048 or 4096");
        }
        let pages_used = usize::from(u16_at(&data, 50)?);
        if pages_used.checked_mul(page_size) != Some(data.len()) {
            bail!(
                "the header counts {pages_used} page(s) of {page_size}, and the file is {} bytes",
                data.len()
            );
        }
        let fpm = usize::from(u16_at(&data, 48)?);
        if fpm == 0 || fpm >= pages_used {
            bail!("the free page map is at page {fpm}, outside the file's {pages_used} page(s)");
        }
        let dir_size = us(u32_at(&data, 52)?);
        let dir_pages = dir_size.div_ceil(page_size);
        if HEADER_PAGES_AT.saturating_add(dir_pages.saturating_mul(2)) > page_size {
            bail!(
                "a directory of {dir_size} bytes needs {dir_pages} page(s), more than the header page lists"
            );
        }
        let mut pages = Vec::with_capacity(dir_pages);
        for i in 0..dir_pages {
            let p = usize::from(u16_at(&data, HEADER_PAGES_AT + 2 * i)?);
            if p >= pages_used {
                bail!("directory page {p} is past the file's {pages_used} page(s)");
            }
            pages.push(p);
        }
        let dir = gather(&data, page_size, &pages, dir_size).context("the directory")?;
        let count = us(u32_at(&dir, 0).context("the directory's stream count")?);
        if count.saturating_mul(8).saturating_add(4) > dir.len() {
            bail!(
                "the directory counts {count} stream(s), more than its {} bytes hold",
                dir.len()
            );
        }
        let mut lens = Vec::with_capacity(count);
        for i in 0..count {
            let s = u32_at(&dir, 4 + 8 * i)?;
            lens.push((s != NIL).then_some(us(s)));
        }
        let mut at = 4 + 8 * count;
        let mut total = 0usize;
        let mut streams = Vec::with_capacity(count);
        for (i, len) in lens.into_iter().enumerate() {
            let n = len.unwrap_or(0).div_ceil(page_size);
            total = total.saturating_add(n);
            if total > pages_used {
                bail!(
                    "stream {i} of {} bytes needs {n} page(s), and the streams up to it {total}: more than the file's {pages_used} page(s)",
                    len.unwrap_or(0)
                );
            }
            if at.saturating_add(n.saturating_mul(2)) > dir.len() {
                bail!(
                    "stream {i} of {} bytes needs {n} page(s): past the directory's page lists",
                    len.unwrap_or(0)
                );
            }
            let mut pg = Vec::with_capacity(n);
            for _ in 0..n {
                let p = usize::from(u16_at(&dir, at)?);
                if p >= pages_used {
                    bail!("stream {i} names page {p}, past the file's {pages_used} page(s)");
                }
                pg.push(p);
                at += 2;
            }
            streams.push(Stream { len, pages: pg });
        }
        Ok(Self {
            data,
            page_size,
            pages_used,
            streams,
            dir_trailing: dir.len().saturating_sub(at),
        })
    }

    pub fn stream(&self, index: usize) -> Result<Vec<u8>> {
        let s = self.streams.get(index).ok_or_else(|| {
            anyhow!(
                "no stream {index}: the directory lists {}",
                self.streams.len()
            )
        })?;
        gather(&self.data, self.page_size, &s.pages, s.size())
            .with_context(|| format!("stream {index}"))
    }

    #[must_use]
    pub fn has(&self, index: usize) -> bool {
        index < self.streams.len()
    }

    #[must_use]
    pub fn bytes(&self) -> &[u8] {
        &self.data
    }

    #[must_use]
    pub fn into_bytes(self) -> Vec<u8> {
        self.data
    }

    #[must_use]
    pub fn file_len(&self) -> usize {
        self.data.len()
    }

    pub fn fpm(&self) -> Result<usize> {
        Ok(usize::from(u16_at(&self.data, 48)?))
    }

    pub fn put_stream(&mut self, index: usize, bytes: &[u8]) -> Result<()> {
        let s = self
            .streams
            .get(index)
            .ok_or_else(|| anyhow!("no stream {index}"))?;
        if bytes.len() != s.size() {
            bail!(
                "stream {index} is {} bytes and the replacement is {}: an edit in place keeps its length",
                s.size(),
                bytes.len()
            );
        }
        let ps = self.page_size;
        let pages = s.pages.clone();
        for (chunk, p) in bytes.chunks(ps).zip(&pages) {
            let at = p
                .checked_mul(ps)
                .ok_or_else(|| anyhow!("page {p} is out of range"))?;
            put(&mut self.data, at, chunk)?;
        }
        Ok(())
    }

    pub fn grow_stream(&mut self, index: usize, bytes: &[u8]) -> Result<()> {
        let s = self
            .streams
            .get(index)
            .cloned()
            .ok_or_else(|| anyhow!("no stream {index}"))?;
        if bytes.len() < s.size() {
            bail!(
                "stream {index} is {} bytes and the replacement {}: growing never shrinks a stream",
                s.size(),
                bytes.len()
            );
        }
        if bytes.len() == s.size() && s.len.is_some() {
            return self.put_stream(index, bytes);
        }
        let mut pages = s.pages;
        let need = bytes.len().div_ceil(self.page_size);
        while pages.len() < need {
            pages.push(self.append_page()?);
        }
        if let Some(e) = self.streams.get_mut(index) {
            *e = Stream {
                len: Some(bytes.len()),
                pages,
            };
        }
        self.write_directory()?;
        self.put_stream(index, bytes)
    }

    pub fn set_stream(&mut self, index: usize, bytes: &[u8]) -> Result<()> {
        let s = self
            .streams
            .get(index)
            .cloned()
            .ok_or_else(|| anyhow!("no stream {index}"))?;
        if bytes.len() >= s.size() {
            return self.grow_stream(index, bytes);
        }
        let ps = self.page_size;
        let mut pages = s.pages;
        let freed = pages.split_off(bytes.len().div_ceil(ps));
        if let Some(e) = self.streams.get_mut(index) {
            *e = Stream {
                len: Some(bytes.len()),
                pages: pages.clone(),
            };
        }
        self.write_directory()?;
        self.put_stream(index, bytes)?;
        if let Some(last) = pages.last() {
            let used = bytes.len() - (pages.len() - 1) * ps;
            self.zero(last * ps + used, (last + 1) * ps)?;
        }
        for p in freed {
            self.release_page(p)?;
        }
        Ok(())
    }

    fn zero(&mut self, from: usize, to: usize) -> Result<()> {
        self.data
            .get_mut(from..to)
            .ok_or_else(|| anyhow!("bytes {from:#x}..{to:#x} run past the file"))?
            .fill(0);
        Ok(())
    }

    fn fpm_bit(&mut self, p: usize) -> Result<&mut u8> {
        let ps = self.page_size;
        if p / 8 >= fpm_span(ps) * ps {
            bail!(
                "page {p} lies past the free page map's {} bits",
                fpm_span(ps) * ps * 8
            );
        }
        let fpm = self.fpm()?;
        self.data
            .get_mut(fpm * ps + p / 8)
            .context("the free page map runs past the file")
    }

    fn release_page(&mut self, p: usize) -> Result<()> {
        let ps = self.page_size;
        self.zero(p * ps, (p + 1) * ps)?;
        *self.fpm_bit(p)? |= 1u8 << (p % 8);
        Ok(())
    }

    fn append_page(&mut self) -> Result<usize> {
        let ps = self.page_size;
        let p = self.pages_used;
        if p + 1 >= 0x8000 {
            bail!(
                "a page past {p} overflows the PDB 2.0 page count, which readers take as signed 16-bit"
            );
        }
        if p / 8 >= fpm_span(ps) * ps {
            bail!(
                "page {p} lies past the free page map's {} bits",
                fpm_span(ps) * ps * 8
            );
        }
        self.data.resize((p + 1) * ps, 0);
        self.pages_used = p + 1;
        put(
            &mut self.data,
            50,
            &u16_of(p + 1, "page count")?.to_le_bytes(),
        )?;
        *self.fpm_bit(p)? &= !(1u8 << (p % 8));
        Ok(p)
    }

    pub fn dir_pages(&self) -> Result<Vec<usize>> {
        let n = us(u32_at(&self.data, 52)?).div_ceil(self.page_size);
        (0..n)
            .map(|i| Ok(usize::from(u16_at(&self.data, HEADER_PAGES_AT + 2 * i)?)))
            .collect()
    }

    fn write_directory(&mut self) -> Result<()> {
        let ps = self.page_size;
        let mut dir_pages = self.dir_pages()?;
        let old_size = us(u32_at(&self.data, 52)?);
        let old = gather(&self.data, ps, &dir_pages, old_size)?;
        let mut dir = u32_of(self.streams.len(), "stream count")?
            .to_le_bytes()
            .to_vec();
        for (i, s) in self.streams.iter().enumerate() {
            let len = match s.len {
                Some(l) => u32_of(l, "stream length")?,
                None => NIL,
            };
            dir.extend_from_slice(&len.to_le_bytes());
            dir.extend_from_slice(&u32_at(&old, 8 + 8 * i).unwrap_or(0).to_le_bytes());
        }
        for s in &self.streams {
            for p in &s.pages {
                dir.extend_from_slice(&u16_of(*p, "page number")?.to_le_bytes());
            }
        }
        while dir_pages.len() < dir.len().div_ceil(ps) {
            dir_pages.push(self.append_page()?);
        }
        for p in dir_pages.split_off(dir.len().div_ceil(ps).max(1)) {
            self.release_page(p)?;
        }
        if HEADER_PAGES_AT + 2 * dir_pages.len() > ps {
            bail!(
                "a directory of {} page(s) does not fit the header's page list",
                dir_pages.len()
            );
        }
        for (chunk, p) in dir.chunks(ps).zip(&dir_pages) {
            put(&mut self.data, p * ps, chunk)?;
        }
        let mut head = u32_of(dir.len(), "directory size")?.to_le_bytes().to_vec();
        head.extend_from_slice(
            self.data
                .get(56..60)
                .context("the MSF header is truncated")?,
        );
        for p in &dir_pages {
            head.extend_from_slice(&u16_of(*p, "page number")?.to_le_bytes());
        }
        put(&mut self.data, 52, &head)?;
        self.dir_trailing = 0;
        Ok(())
    }

    pub fn fpm_pages(&self) -> Result<Vec<usize>> {
        let span = fpm_span(self.page_size);
        let fpm = self.fpm()?;
        let mut v: Vec<usize> = (1..1 + 2 * span).chain(fpm..fpm + span).collect();
        v.sort_unstable();
        v.dedup();
        Ok(v)
    }

    pub fn used_pages(&self) -> Result<Vec<bool>> {
        let mut used = vec![false; self.pages_used];
        let mut mark = |p: usize| {
            if let Some(u) = used.get_mut(p) {
                *u = true;
            }
        };
        mark(0);
        for p in self.fpm_pages()? {
            mark(p);
        }
        for p in self.dir_pages()? {
            mark(p);
        }
        for s in &self.streams {
            for p in &s.pages {
                mark(*p);
            }
        }
        Ok(used)
    }

    pub fn unheld(&self) -> Result<Vec<(Unheld, usize, usize)>> {
        let ps = self.page_size;
        let mut out = Vec::new();
        for (p, u) in self.used_pages()?.iter().enumerate() {
            if !u {
                out.push((Unheld::Page(p), p * ps, (p + 1) * ps));
            }
        }
        let slack = |pages: &[usize], size: usize| {
            pages.last().map(|last| {
                let used_in_last = size.saturating_sub((pages.len() - 1) * ps);
                (last * ps + used_in_last, (last + 1) * ps)
            })
        };
        for (i, s) in self.streams.iter().enumerate() {
            if let Some((a, b)) = slack(&s.pages, s.size()) {
                out.push((Unheld::StreamSlack(i), a, b));
            }
        }
        let dir = self.dir_pages()?;
        let dir_size = us(u32_at(&self.data, 52)?);
        if let Some((a, b)) = slack(&dir, dir_size) {
            out.push((Unheld::DirectorySlack, a, b));
        }
        out.push((Unheld::HeaderTail, HEADER_PAGES_AT + 2 * dir.len(), ps));
        out.push((Unheld::HeaderReserved, 56, 60));
        for i in 0..self.streams.len() {
            let off = 8 + 8 * i;
            if let Some(p) = dir.get(off / ps) {
                let at = p * ps + off % ps;
                out.push((Unheld::DirectoryReserved(i), at, at + 4));
            }
        }
        Ok(out)
    }

    pub fn zero_unused(&mut self) -> Result<usize> {
        let mut n = 0usize;
        for (_, a, b) in self.unheld()? {
            if let Some(r) = self.data.get_mut(a..b) {
                n += r.iter().filter(|c| **c != 0).count();
                r.fill(0);
            }
        }
        Ok(n)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Built {
    pub bytes: Vec<u8>,
    pub page_size: usize,
    pub pages: usize,
}

pub fn layout(streams: &[Vec<u8>]) -> Result<Built> {
    for (page, span) in [(0x400usize, 8usize), (0x1000, 1)] {
        let data: usize = streams.iter().map(|s| s.len().div_ceil(page)).sum();
        let dir_len = 4 + 8 * streams.len() + 2 * data;
        let dir_pages = dir_len.div_ceil(page);
        let total = 1 + 2 * span + data + dir_pages;
        if total > MAX_PAGES || HEADER_PAGES_AT + 2 * dir_pages > page {
            continue;
        }
        let mut out = vec![0u8; total * page];
        let mut next = 1 + 2 * span;
        let mut dir = u32_of(streams.len(), "stream count")?
            .to_le_bytes()
            .to_vec();
        for s in streams {
            dir.extend_from_slice(&u32_of(s.len(), "stream length")?.to_le_bytes());
            dir.extend_from_slice(&0u32.to_le_bytes());
        }
        for s in streams {
            for chunk in s.chunks(page) {
                put(&mut out, next * page, chunk)?;
                dir.extend_from_slice(&u16_of(next, "page number")?.to_le_bytes());
                next += 1;
            }
        }
        let mut dir_list = Vec::with_capacity(dir_pages);
        for chunk in dir.chunks(page) {
            put(&mut out, next * page, chunk)?;
            dir_list.push(u16_of(next, "page number")?);
            next += 1;
        }
        if next != total {
            bail!("laid out {next} pages, planned {total}");
        }
        for p in total..span * page * 8 {
            if let Some(b) = out.get_mut(page + p / 8) {
                *b |= 1 << (p % 8);
            }
        }
        put(&mut out, 0, MAGIC)?;
        let mut head = u32_of(page, "page size")?.to_le_bytes().to_vec();
        head.extend_from_slice(&1u16.to_le_bytes());
        head.extend_from_slice(&u16_of(total, "page count")?.to_le_bytes());
        head.extend_from_slice(&u32_of(dir.len(), "directory size")?.to_le_bytes());
        head.extend_from_slice(&0u32.to_le_bytes());
        for p in dir_list {
            head.extend_from_slice(&p.to_le_bytes());
        }
        put(&mut out, 44, &head)?;
        return Ok(Built {
            bytes: out,
            page_size: page,
            pages: total,
        });
    }
    bail!("the streams do not fit a PDB 2.0 container (16-bit page numbers)")
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> Vec<Vec<u8>> {
        vec![
            Vec::new(),
            vec![1u8; 10],
            vec![2u8; 3000],
            Vec::new(),
            (0..=255u8).cycle().take(1024).collect(),
        ]
    }

    #[test]
    fn a_laid_out_container_reads_back_stream_for_stream() {
        let streams = sample();
        let b = layout(&streams).unwrap();
        assert_eq!(b.page_size, 1024);
        assert_eq!(b.bytes.len(), b.page_size * b.pages);
        let m = Msf2::parse(b.bytes).unwrap();
        for (i, s) in streams.iter().enumerate() {
            assert_eq!(&m.stream(i).unwrap(), s, "stream {i}");
        }
        assert!(m.streams.iter().flat_map(|s| &s.pages).all(|p| *p >= 17));
        assert_eq!(m.dir_trailing, 0);
    }

    #[test]
    fn what_no_stream_holds_is_zeroed_and_the_streams_are_kept() {
        let mut m = Msf2::parse(layout(&sample()).unwrap().bytes).unwrap();
        let before: Vec<Vec<u8>> = (0..m.streams.len()).map(|i| m.stream(i).unwrap()).collect();
        let ranges = m.unheld().unwrap();
        for (_, a, _) in &ranges {
            if let Some(b) = m.data.get_mut(*a) {
                *b = 0x5a;
            }
        }
        let fpm = m.fpm().unwrap() * m.page_size;
        let map = m.data[fpm..fpm + 8 * 1024].to_vec();
        assert!(m.zero_unused().unwrap() > 0);
        assert_eq!(m.data[fpm..fpm + 8 * 1024], map[..]);
        for (u, a, b) in &ranges {
            assert!(m.data[*a..*b].iter().all(|c| *c == 0), "{u}");
        }
        let after: Vec<Vec<u8>> = (0..m.streams.len()).map(|i| m.stream(i).unwrap()).collect();
        assert_eq!(before, after);
        for want in [
            Unheld::StreamSlack(1),
            Unheld::DirectorySlack,
            Unheld::HeaderTail,
            Unheld::HeaderReserved,
            Unheld::DirectoryReserved(0),
        ] {
            assert!(
                ranges
                    .iter()
                    .any(|(u, ..)| std::mem::discriminant(u) == std::mem::discriminant(&want)),
                "{want}"
            );
        }
    }

    #[test]
    fn a_stream_grows_onto_appended_pages_and_shrinks_releasing_them() {
        let mut m = Msf2::parse(layout(&sample()).unwrap().bytes).unwrap();
        let ps = m.page_size;
        let n = m.streams.len();
        let before: Vec<Vec<u8>> = (0..n).map(|i| m.stream(i).unwrap()).collect();
        let k = 2;
        let mut s = before[k].clone();
        s.extend((0..700 * ps).map(|i| u8::try_from(i % 251).unwrap()));
        m.set_stream(k, &s).unwrap();
        let (held, dirs) = (m.streams[k].pages.clone(), m.dir_pages().unwrap());
        assert!(dirs.len() > 1, "the directory took a second page");
        assert!(m.grow_stream(k, &s[..s.len() - 1]).is_err());
        let short: Vec<u8> = (1..=10).collect();
        m.set_stream(k, &short).unwrap();
        assert_eq!(m.streams[k].pages, held[..1]);
        assert_eq!(m.dir_pages().unwrap().len(), 1);
        let fpm = m.fpm().unwrap();
        for p in held[1..].iter().chain(&dirs[1..]) {
            assert_eq!(m.data[fpm * ps + p / 8] >> (p % 8) & 1, 1, "page {p} free");
            assert!(m.data[p * ps..(p + 1) * ps].iter().all(|c| *c == 0));
        }
        let back = Msf2::parse(m.bytes().to_vec()).unwrap();
        for (i, b) in before.iter().enumerate() {
            let now = back.stream(i).unwrap();
            if i == k {
                assert_eq!(now, short);
            } else {
                assert_eq!(&now, b, "stream {i}");
            }
        }
        let mut again = Msf2::parse(layout(&sample()).unwrap().bytes).unwrap();
        again.set_stream(k, &s).unwrap();
        again.set_stream(k, &short).unwrap();
        assert!(
            again.bytes() == m.bytes(),
            "set the same way twice, one file"
        );
    }

    #[test]
    fn a_damaged_header_or_directory_is_refused() {
        let good = layout(&sample()).unwrap().bytes;
        let mut b = good.clone();
        b[44] = 3;
        assert!(Msf2::parse(b).is_err());
        let mut b = good.clone();
        b.truncate(b.len() - 1);
        assert!(Msf2::parse(b).is_err());
        let mut b = good.clone();
        b[52..56].copy_from_slice(&0xffff_fff0u32.to_le_bytes());
        assert!(Msf2::parse(b).is_err());
        let d = usize::from(u16::from_le_bytes([good[60], good[61]])) * 1024;
        let mut b = good.clone();
        b[d..d + 4].copy_from_slice(&0x0fff_ffffu32.to_le_bytes());
        assert!(Msf2::parse(b).is_err());
        let mut b = good.clone();
        b[d + 4..d + 8].copy_from_slice(&0x7fff_0000u32.to_le_bytes());
        assert!(Msf2::parse(b).is_err());
        let mut b = good;
        b[d + 4..d + 8].copy_from_slice(&NIL.to_le_bytes());
        let m = Msf2::parse(b).unwrap();
        assert_eq!(m.streams[0].len, None);
        assert!(m.stream(0).unwrap().is_empty());
        assert!(m.dir_trailing == 0);
    }

    #[test]
    fn streams_that_need_more_pages_than_the_file_holds_are_refused_within_the_directory() {
        let good = layout(&sample()).unwrap().bytes;
        let pages_used = usize::from(u16::from_le_bytes([good[50], good[51]]));
        let d = usize::from(u16::from_le_bytes([good[60], good[61]])) * 1024;
        let mut b = good;
        b[52..56].copy_from_slice(&1024u32.to_le_bytes());
        let want = u32::try_from((pages_used + 1) * 1024).unwrap();
        b[d + 4..d + 8].copy_from_slice(&want.to_le_bytes());
        assert!(4 + 8 * 5 + 2 * (pages_used + 1) <= 1024);
        let e = Msf2::parse(b).unwrap_err().to_string();
        assert!(
            e.contains(&format!(
                "stream 0 of {want} bytes needs {} page(s), and the streams up to it {}: more than the file's {pages_used} page(s)",
                pages_used + 1,
                pages_used + 1
            )),
            "{e}"
        );
    }
}
