use crate::le::{bytes, set32, u16_at, u16_of, u32_at, u32_of, us};
use anyhow::{Context as _, Result, bail};
use msvc6_codeview::codec::{Gen, name_at, push_name};
use std::collections::BTreeMap;

pub const V60: u32 = 19_970_606;
pub const V70: u32 = 19_990_903;
pub const HEADER: usize = 64;
pub const MODULE_HEADER: usize = 64;
pub const NO_STREAM: u16 = 0xffff;
pub const DBG_FPO: usize = 0;
pub const DBG_SECTION_HEADER: usize = 5;
pub const SC_SIZE: usize = 28;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ModRec {
    pub at: usize,
    pub head: Vec<u8>,
    pub name: Vec<u8>,
    pub obj: Vec<u8>,
}

impl ModRec {
    pub fn stream(&self) -> Result<u16> {
        u16_at(&self.head, 34)
    }
    pub fn sym_len(&self) -> Result<usize> {
        Ok(us(u32_at(&self.head, 36)?))
    }
    pub fn c11_len(&self) -> Result<usize> {
        Ok(us(u32_at(&self.head, 40)?))
    }
    pub fn c13_len(&self) -> Result<usize> {
        Ok(us(u32_at(&self.head, 44)?))
    }
    pub fn ni_src(&self) -> Result<u32> {
        u32_at(&self.head, 56)
    }
    #[must_use]
    pub fn name_text(&self) -> String {
        crate::le::latin1(&self.name)
    }
    #[must_use]
    pub fn obj_text(&self) -> String {
        crate::le::latin1(&self.obj)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dbi {
    pub header: Vec<u8>,
    pub mods: Vec<ModRec>,
    pub sc: Vec<u8>,
    pub segmap: Vec<u8>,
    pub files: Vec<u8>,
    pub tsm: Vec<u8>,
    pub ec: Vec<u8>,
    pub dbg: Vec<u16>,
    pub offsets: Offsets,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Offsets {
    pub sc: usize,
    pub segmap: usize,
    pub files: usize,
    pub tsm: usize,
    pub ec: usize,
    pub dbg: usize,
}

const LENS: [usize; 7] = [24, 28, 32, 36, 40, 48, 52];

impl Dbi {
    pub fn parse(s: &[u8]) -> Result<Self> {
        if u32_at(s, 0).context("the DBI stream is too short for its header")? != u32::MAX {
            bail!("the DBI stream does not open with 0xffffffff (the header VC6's LINK writes)");
        }
        let header = bytes(s, 0, HEADER)?.to_vec();
        let mut lens = [0usize; 7];
        for (l, o) in lens.iter_mut().zip(LENS) {
            *l = us(u32_at(s, o)?);
        }
        let [modi, sc, segmap, files, tsm, dbg, ec] = lens;
        let total = lens.iter().fold(HEADER, |a, n| a.saturating_add(*n));
        if total != s.len() {
            bail!(
                "the DBI's substreams sum to {total} bytes and the stream is {}",
                s.len()
            );
        }
        let mut at = HEADER;
        let end = HEADER + modi;
        let mut mods = Vec::new();
        while at < end {
            let head = bytes(s, at, MODULE_HEADER)
                .with_context(|| format!("the module record at {at:#x}"))?
                .to_vec();
            if at + MODULE_HEADER > end {
                bail!("the module record at {at:#x} runs past the module list");
            }
            let region = s.get(..end).unwrap_or_default();
            let (name, e) = name_at(region, at + MODULE_HEADER, Gen::Sz)
                .with_context(|| format!("the module record at {at:#x}: its name"))?;
            let (obj, e2) = name_at(region, e, Gen::Sz)
                .with_context(|| format!("the module record at {at:#x}: its object name"))?;
            mods.push(ModRec {
                at,
                head,
                name: name.to_vec(),
                obj: obj.to_vec(),
            });
            at = e2.next_multiple_of(4);
        }
        if at != end {
            bail!("the module records end at {at:#x}, the header says {end:#x}");
        }
        let mut take = |n: usize| -> Result<(usize, Vec<u8>)> {
            let b = bytes(s, at, n)?.to_vec();
            let was = at;
            at += n;
            Ok((was, b))
        };
        let (sc_at, sc) = take(sc)?;
        let (segmap_at, segmap) = take(segmap)?;
        let (files_at, files) = take(files)?;
        let (tsm_at, tsm) = take(tsm)?;
        let (ec_at, ec) = take(ec)?;
        let (dbg_at, dbgb) = take(dbg)?;
        let dbg = (0..dbgb.len() / 2)
            .map(|i| u16_at(&dbgb, 2 * i))
            .collect::<Result<_>>()?;
        Ok(Self {
            header,
            mods,
            sc,
            segmap,
            files,
            tsm,
            ec,
            dbg,
            offsets: Offsets {
                sc: sc_at,
                segmap: segmap_at,
                files: files_at,
                tsm: tsm_at,
                ec: ec_at,
                dbg: dbg_at,
            },
        })
    }

    pub fn version(&self) -> Result<u32> {
        u32_at(&self.header, 4)
    }
    pub fn age(&self) -> Result<u32> {
        u32_at(&self.header, 8)
    }
    pub fn globals(&self) -> Result<u16> {
        u16_at(&self.header, 12)
    }
    pub fn publics(&self) -> Result<u16> {
        u16_at(&self.header, 16)
    }
    pub fn symrecs(&self) -> Result<u16> {
        u16_at(&self.header, 20)
    }

    #[must_use]
    pub fn section_headers(&self) -> Option<u16> {
        self.dbg
            .get(DBG_SECTION_HEADER)
            .copied()
            .filter(|s| *s != NO_STREAM)
    }

    pub fn bytes(&self) -> Result<Vec<u8>> {
        let mut modi = Vec::new();
        for m in &self.mods {
            modi.extend_from_slice(&m.head);
            push_name(&mut modi, &m.name, Gen::Sz)?;
            push_name(&mut modi, &m.obj, Gen::Sz)?;
            modi.resize(modi.len().next_multiple_of(4), 0);
        }
        let mut h = self.header.clone();
        for (o, n) in [
            (24, modi.len()),
            (28, self.sc.len()),
            (32, self.segmap.len()),
            (36, self.files.len()),
            (40, self.tsm.len()),
            (48, 2 * self.dbg.len()),
            (52, self.ec.len()),
        ] {
            set32(&mut h, o, u32_of(n, "DBI substream")?)?;
        }
        let mut out = h;
        out.extend_from_slice(&modi);
        out.extend_from_slice(&self.sc);
        out.extend_from_slice(&self.segmap);
        out.extend_from_slice(&self.files);
        out.extend_from_slice(&self.tsm);
        out.extend_from_slice(&self.ec);
        for d in &self.dbg {
            out.extend_from_slice(&d.to_le_bytes());
        }
        Ok(out)
    }
}

pub fn read_files(f: &[u8], g: Gen) -> Result<Vec<Vec<Vec<u8>>>> {
    if f.is_empty() {
        return Ok(Vec::new());
    }
    let nm = usize::from(u16_at(f, 0)?);
    let mut counts = Vec::with_capacity(nm);
    for i in 0..nm {
        counts.push(usize::from(u16_at(f, 4 + 2 * nm + 2 * i)?));
    }
    let total: usize = counts.iter().sum();
    let offs_at = 4 + 4 * nm;
    let names_at = offs_at + 4 * total;
    if names_at > f.len() {
        bail!(
            "the file info's {total} name offsets run past its {} bytes",
            f.len()
        );
    }
    let mut out = Vec::with_capacity(nm);
    let mut k = 0usize;
    for c in counts {
        let mut v = Vec::with_capacity(c);
        for _ in 0..c {
            let o = us(u32_at(f, offs_at + 4 * k)?);
            v.push(name_at(f, names_at.saturating_add(o), g)?.0.to_vec());
            k += 1;
        }
        out.push(v);
    }
    Ok(out)
}

pub fn write_files(mods: &[Vec<Vec<u8>>], g: Gen) -> Result<Vec<u8>> {
    let nm = mods.len();
    let total: usize = mods.iter().map(Vec::len).sum();
    let mut out = Vec::new();
    out.extend_from_slice(&u16_of(nm, "modules")?.to_le_bytes());
    out.extend_from_slice(&u16::try_from(total).unwrap_or(u16::MAX).to_le_bytes());
    let mut first = 0usize;
    for m in mods {
        out.extend_from_slice(&u16::try_from(first).unwrap_or(u16::MAX).to_le_bytes());
        first += m.len();
    }
    for m in mods {
        out.extend_from_slice(&u16_of(m.len(), "files of a module")?.to_le_bytes());
    }
    let mut buf = Vec::new();
    let mut at: BTreeMap<&[u8], u32> = BTreeMap::new();
    let mut offs = Vec::with_capacity(total);
    for m in mods {
        for n in m {
            let o = match at.get(n.as_slice()) {
                Some(o) => *o,
                None => {
                    let o = u32_of(buf.len(), "file name offset")?;
                    push_name(&mut buf, n, g)?;
                    at.insert(n.as_slice(), o);
                    o
                }
            };
            offs.push(o);
        }
    }
    for o in offs {
        out.extend_from_slice(&o.to_le_bytes());
    }
    out.extend_from_slice(&buf);
    out.resize(out.len().next_multiple_of(4), 0);
    Ok(out)
}

pub fn section_rvas(headers: &[u8]) -> Result<Vec<u32>> {
    headers
        .as_chunks::<40>()
        .0
        .iter()
        .map(|h| u32_at(h, 12))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn module(name: &str, stream: u16) -> ModRec {
        let mut head = vec![0u8; MODULE_HEADER];
        head[34..36].copy_from_slice(&stream.to_le_bytes());
        ModRec {
            at: 0,
            head,
            name: name.as_bytes().to_vec(),
            obj: name.as_bytes().to_vec(),
        }
    }

    fn dbi() -> Dbi {
        let mut header = vec![0u8; HEADER];
        header[..4].copy_from_slice(&u32::MAX.to_le_bytes());
        header[4..8].copy_from_slice(&V60.to_le_bytes());
        Dbi {
            header,
            mods: vec![module("a.obj", 9), module("long\\b.obj", NO_STREAM)],
            sc: vec![1; 32],
            segmap: vec![2; 24],
            files: write_files(&[vec![b"a.c".to_vec()], vec![]], Gen::St).unwrap(),
            tsm: Vec::new(),
            ec: vec![3; 8],
            dbg: vec![NO_STREAM, 1, 2, 3, 4, 8],
            offsets: Offsets::default(),
        }
    }

    #[test]
    fn a_dbi_writes_and_reads_back_substream_for_substream() {
        let d = dbi();
        let b = d.bytes().unwrap();
        let back = Dbi::parse(&b).unwrap();
        assert_eq!(back.mods.len(), 2);
        assert_eq!(back.mods[1].name, b"long\\b.obj");
        assert_eq!(back.mods[0].stream().unwrap(), 9);
        assert_eq!(back.mods[1].at, HEADER + 64 + 12);
        assert_eq!(
            (back.sc.clone(), back.segmap.clone(), back.ec.clone()),
            (d.sc.clone(), d.segmap.clone(), d.ec.clone())
        );
        assert_eq!(back.section_headers(), Some(8));
        assert_eq!(back.offsets.sc, 64 + (64 + 12) + (64 + 24));
        assert_eq!(back.bytes().unwrap(), b);
        let f = read_files(&back.files, Gen::St).unwrap();
        assert_eq!(f, vec![vec![b"a.c".to_vec()], vec![]]);
    }

    #[test]
    fn a_dbi_whose_substreams_do_not_fill_it_is_refused() {
        let mut b = dbi().bytes().unwrap();
        b.push(0);
        assert!(Dbi::parse(&b).is_err());
        let mut b = dbi().bytes().unwrap();
        b[24..28].copy_from_slice(&0xffff_fff0u32.to_le_bytes());
        assert!(Dbi::parse(&b).is_err());
        let mut b = dbi().bytes().unwrap();
        b[24..28].copy_from_slice(&40u32.to_le_bytes());
        b[28..32].copy_from_slice(&(32u32 + 64 + 12 + 64 + 24 - 40).to_le_bytes());
        assert!(Dbi::parse(&b).is_err());
        assert!(Dbi::parse(&[0xff; 8]).is_err());
    }

    #[test]
    fn file_info_offsets_past_their_names_are_refused() {
        let mut f = write_files(&[vec![b"a.c".to_vec(), b"b.h".to_vec()]], Gen::St).unwrap();
        f[8..12].copy_from_slice(&0x7fff_ffffu32.to_le_bytes());
        assert!(read_files(&f, Gen::St).is_err());
        let mut f = write_files(&[vec![b"a.c".to_vec()]], Gen::St).unwrap();
        f[6..8].copy_from_slice(&0xffffu16.to_le_bytes());
        assert!(read_files(&f, Gen::St).is_err());
    }
}
