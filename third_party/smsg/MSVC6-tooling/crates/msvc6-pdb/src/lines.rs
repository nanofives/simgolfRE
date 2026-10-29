use crate::le::{bytes, u16_at, u32_at, u32_of, us};
use crate::strtab::StrTab;
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::codec::{Gen, name_at};
use std::collections::BTreeMap;

pub const DEBUG_S_LINES: u32 = 0xf2;
pub const DEBUG_S_FILECHKSMS: u32 = 0xf4;
pub const LINE_STATEMENT: u32 = 0x8000_0000;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct C11File {
    pub name: Vec<u8>,
    pub blocks: Vec<C11Block>,
}

pub type C11Block = (u16, u32, u32, Vec<(u32, u16)>);
pub type LineRow = (Vec<u8>, u16, u32, u16);
pub type LineRange = (u16, u32, u32);
type Groups = BTreeMap<LineRange, Vec<(usize, Vec<(u32, u16)>)>>;

pub fn read_c11(b: &[u8]) -> Result<Vec<C11File>> {
    if b.len() < 4 {
        return Ok(Vec::new());
    }
    let c_file = usize::from(u16_at(b, 0)?);
    if c_file.saturating_mul(4).saturating_add(4) > b.len() {
        bail!(
            "the C11 region's {c_file} file record(s) do not fit its {} bytes",
            b.len()
        );
    }
    let mut blocks_left = b.len() / 12;
    let mut pairs_left = b.len() / 6;
    let mut out = Vec::with_capacity(c_file);
    for i in 0..c_file {
        let base = us(u32_at(b, 4 + 4 * i)?);
        let cc = usize::from(u16_at(b, base).with_context(|| format!("C11 file record {i}"))?);
        blocks_left = blocks_left.checked_sub(cc).ok_or_else(|| {
            anyhow!("the C11 region's file records name more blocks than its bytes hold")
        })?;
        let fse = base.saturating_add(4).saturating_add(4 * cc);
        let (name, _) = name_at(b, fse.saturating_add(8 * cc), Gen::St)
            .with_context(|| format!("C11 file record {i}'s name"))?;
        let mut blocks = Vec::with_capacity(cc);
        for j in 0..cc {
            let at = us(u32_at(b, base + 4 + 4 * j)?);
            let seg = u16_at(b, at)?;
            let n = usize::from(u16_at(b, at.saturating_add(2))?);
            pairs_left = pairs_left.checked_sub(n).ok_or_else(|| {
                anyhow!("the C11 region's blocks hold more line pairs than its bytes")
            })?;
            let mut pairs = Vec::with_capacity(n);
            for q in 0..n {
                pairs.push((
                    u32_at(b, at + 4 + 4 * q)?,
                    u16_at(b, at + 4 + 4 * n + 2 * q)?,
                ));
            }
            blocks.push((
                seg,
                u32_at(b, fse + 8 * j)?,
                u32_at(b, fse + 8 * j + 4)?,
                pairs,
            ));
        }
        out.push(C11File {
            name: name.to_vec(),
            blocks,
        });
    }
    Ok(out)
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct LineStats {
    pub rows: usize,
    pub in_procs: usize,
    pub orphans: usize,
    pub subsections: usize,
}

fn subsection(out: &mut Vec<u8>, kind: u32, data: &[u8]) -> Result<()> {
    out.extend_from_slice(&kind.to_le_bytes());
    out.extend_from_slice(&u32_of(data.len(), "subsection length")?.to_le_bytes());
    out.extend_from_slice(data);
    out.resize(out.len().next_multiple_of(4), 0);
    Ok(())
}

pub(crate) struct Holders {
    procs: Vec<(u16, u32, u32)>,
    tree: Vec<u64>,
    size: usize,
}

impl Holders {
    pub(crate) fn new(mut procs: Vec<(u16, u32, u32)>) -> Self {
        procs.retain(|p| p.0 != 0);
        procs.sort_unstable();
        let size = procs.len().next_power_of_two().max(1);
        let mut tree = vec![0u64; 2 * size];
        for (i, (_, o, l)) in procs.iter().enumerate() {
            if let Some(t) = tree.get_mut(size + i) {
                *t = u64::from(*o) + u64::from(*l);
            }
        }
        for i in (1..size).rev() {
            let v = tree
                .get(2 * i)
                .copied()
                .unwrap_or(0)
                .max(tree.get(2 * i + 1).copied().unwrap_or(0));
            if let Some(t) = tree.get_mut(i) {
                *t = v;
            }
        }
        Self { procs, tree, size }
    }

    fn rightmost(
        &self,
        node: usize,
        lo: usize,
        hi: usize,
        upto: usize,
        above: u64,
    ) -> Option<usize> {
        if lo > upto || self.tree.get(node).copied().unwrap_or(0) <= above {
            return None;
        }
        if hi - lo == 1 {
            return Some(lo);
        }
        let mid = lo + (hi - lo) / 2;
        self.rightmost(2 * node + 1, mid, hi, upto, above)
            .or_else(|| self.rightmost(2 * node, lo, mid, upto, above))
    }

    pub(crate) fn holder(&self, seg: u16, off: u32) -> Option<(u16, u32, u32)> {
        let seg_lo = self.procs.partition_point(|p| p.0 < seg);
        let p = self.procs.partition_point(|p| (p.0, p.1) <= (seg, off));
        if p == seg_lo {
            return None;
        }
        let i = self.rightmost(1, 0, self.size, p - 1, u64::from(off))?;
        (i >= seg_lo).then(|| self.procs.get(i).copied()).flatten()
    }
}

pub fn c13_lines(
    files: &[C11File],
    procs: &[(u16, u32, u32)],
    names: &mut StrTab,
    st: &mut LineStats,
) -> Result<Vec<u8>> {
    if files
        .iter()
        .all(|f| f.blocks.iter().all(|b| b.3.is_empty()))
    {
        return Ok(Vec::new());
    }
    let mut chk = Vec::with_capacity(8 * files.len());
    let mut entry_of = Vec::with_capacity(files.len());
    for f in files {
        entry_of.push(u32_of(chk.len(), "checksum entry")?);
        chk.extend_from_slice(&names.id(&f.name)?.to_le_bytes());
        chk.extend_from_slice(&[0, 0, 0, 0]);
    }
    let holders = Holders::new(procs.to_vec());
    let mut groups: Groups = BTreeMap::new();
    for (fi, f) in files.iter().enumerate() {
        for (seg, start, end, pairs) in &f.blocks {
            for (off, line) in pairs {
                st.rows += 1;
                let key = match holders.holder(*seg, *off) {
                    Some(p) => {
                        st.in_procs += 1;
                        p
                    }
                    None => {
                        st.orphans += 1;
                        (*seg, *start, end.saturating_sub(*start).saturating_add(1))
                    }
                };
                let g = groups.entry(key).or_default();
                match g.last_mut() {
                    Some((last, rows)) if *last == fi => rows.push((*off, *line)),
                    _ => g.push((fi, vec![(*off, *line)])),
                }
            }
        }
    }
    let mut out = Vec::new();
    subsection(&mut out, DEBUG_S_FILECHKSMS, &chk)?;
    for ((seg, start, size), blocks) in &groups {
        let mut d = Vec::new();
        d.extend_from_slice(&start.to_le_bytes());
        d.extend_from_slice(&seg.to_le_bytes());
        d.extend_from_slice(&0u16.to_le_bytes());
        d.extend_from_slice(&size.to_le_bytes());
        for (fi, rows) in blocks {
            d.extend_from_slice(
                &entry_of
                    .get(*fi)
                    .copied()
                    .context("file index")?
                    .to_le_bytes(),
            );
            d.extend_from_slice(&u32_of(rows.len(), "lines")?.to_le_bytes());
            d.extend_from_slice(&u32_of(12 + 8 * rows.len(), "line block")?.to_le_bytes());
            for (off, line) in rows {
                let rel = off.checked_sub(*start).ok_or_else(|| {
                    anyhow!("a line at {off:#x} before its subsection start {start:#x}")
                })?;
                d.extend_from_slice(&rel.to_le_bytes());
                d.extend_from_slice(&(u32::from(*line) | LINE_STATEMENT).to_le_bytes());
            }
        }
        subsection(&mut out, DEBUG_S_LINES, &d)?;
        st.subsections += 1;
    }
    Ok(out)
}

pub fn read_c13(
    c13: &[u8],
    names: &BTreeMap<u32, Vec<u8>>,
) -> Result<(Vec<LineRow>, Vec<LineRange>)> {
    let mut chk: BTreeMap<u32, u32> = BTreeMap::new();
    let mut lines = Vec::new();
    let mut at = 0usize;
    let mut subs = Vec::new();
    let mut ss = Vec::new();
    while at < c13.len() {
        let kind = u32_at(c13, at)?;
        let len = us(u32_at(c13, at + 4)?);
        ss.push((kind, bytes(c13, at + 8, len)?));
        at = (at + 8 + len).next_multiple_of(4);
    }
    for (kind, d) in &ss {
        if *kind == DEBUG_S_FILECHKSMS {
            let mut p = 0usize;
            while p < d.len() {
                chk.insert(u32_of(p, "entry")?, u32_at(d, p)?);
                let n = usize::from(*d.get(p + 4).context("checksum size")?);
                p = (p + 6 + n).next_multiple_of(4);
            }
        }
    }
    for (kind, d) in &ss {
        match *kind {
            DEBUG_S_FILECHKSMS => {}
            DEBUG_S_LINES => {
                let start = u32_at(d, 0)?;
                let seg = u16_at(d, 4)?;
                let flags = u16_at(d, 6)?;
                let size = u32_at(d, 8)?;
                subs.push((seg, start, size));
                let mut p = 12usize;
                while p < d.len() {
                    let file = u32_at(d, p)?;
                    let n = us(u32_at(d, p + 4)?);
                    let block = us(u32_at(d, p + 8)?);
                    let want = n
                        .checked_mul(if flags & 1 != 0 { 12 } else { 8 })
                        .and_then(|w| w.checked_add(12))
                        .context("a LINES block's row count overflows")?;
                    if block != want || p.saturating_add(block) > d.len() {
                        bail!("a LINES block of {n} rows claims {block} bytes");
                    }
                    let name_id = chk
                        .get(&file)
                        .ok_or_else(|| anyhow!("a LINES block names checksum entry {file:#x}"))?;
                    let name = names
                        .get(name_id)
                        .ok_or_else(|| anyhow!("a checksum entry names string {name_id:#x}"))?;
                    for q in 0..n {
                        let off = u32_at(d, p + 12 + 8 * q)?;
                        let fl = u32_at(d, p + 12 + 8 * q + 4)?;
                        lines.push((
                            name.clone(),
                            seg,
                            start.checked_add(off).context("a line offset overflows")?,
                            u16::try_from(fl & 0x00ff_ffff).context("a line past 65535")?,
                        ));
                    }
                    p += block;
                }
            }
            other => bail!("C13 subsection kind {other:#x} is not one this reader knows"),
        }
    }
    Ok((lines, subs))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn c11_rows_become_one_lines_subsection_per_procedure() {
        let files = vec![C11File {
            name: b"a.cpp".to_vec(),
            blocks: vec![(1, 0x100, 0x13f, vec![(0x100, 10), (0x108, 11), (0x120, 20)])],
        }];
        let procs = vec![(1u16, 0x100u32, 0x20u32), (1, 0x120, 0x20)];
        let mut names = StrTab::new();
        let mut st = LineStats::default();
        let c13 = c13_lines(&files, &procs, &mut names, &mut st).unwrap();
        assert_eq!(
            (st.rows, st.in_procs, st.orphans, st.subsections),
            (3, 3, 0, 2)
        );
        let tab = crate::strtab::read(&names.stream().unwrap()).unwrap();
        let (mut rows, subs) = read_c13(&c13, &tab).unwrap();
        assert_eq!(subs, vec![(1, 0x100, 0x20), (1, 0x120, 0x20)]);
        rows.sort();
        assert_eq!(
            rows,
            vec![
                (b"a.cpp".to_vec(), 1u16, 0x100u32, 10u16),
                (b"a.cpp".to_vec(), 1, 0x108, 11),
                (b"a.cpp".to_vec(), 1, 0x120, 20),
            ]
        );
    }

    #[test]
    fn the_holder_is_the_last_procedure_in_order_that_contains_the_row() {
        let h = Holders::new(vec![
            (1, 0x100, 0x100),
            (1, 0x110, 0x10),
            (1, 0x130, 0x8),
            (2, 0, 4),
        ]);
        assert_eq!(h.holder(1, 0x115), Some((1, 0x110, 0x10)));
        assert_eq!(h.holder(1, 0x125), Some((1, 0x100, 0x100)));
        assert_eq!(h.holder(1, 0x300), None);
        assert_eq!(h.holder(2, 2), Some((2, 0, 4)));
        assert_eq!(h.holder(2, 5), None);
        assert_eq!(h.holder(3, 0), None);
        assert_eq!(Holders::new(Vec::new()).holder(1, 0), None);
    }

    #[test]
    fn a_c11_region_whose_tables_alias_its_bytes_is_refused() {
        let mut b = vec![0u8; 32];
        b[0..2].copy_from_slice(&0xffffu16.to_le_bytes());
        assert!(read_c11(&b).is_err());
        let mut b = vec![0u8; 64];
        b[0..2].copy_from_slice(&1u16.to_le_bytes());
        b[4..8].copy_from_slice(&8u32.to_le_bytes());
        b[8..10].copy_from_slice(&0xffffu16.to_le_bytes());
        assert!(read_c11(&b).is_err());
    }
}
