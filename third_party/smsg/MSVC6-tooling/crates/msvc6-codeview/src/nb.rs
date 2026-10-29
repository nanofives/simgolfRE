use crate::codec::{bytes, u16_at, u32_at, u32_of, us};
use anyhow::{Context as _, Result, bail};
use msvc6_image::pe::Image;
use serde::Serialize;

pub const SST_MODULE: u16 = 0x120;
pub const SST_TYPES: u16 = 0x121;
pub const SST_PUBLIC: u16 = 0x122;
pub const SST_PUBLIC_SYM: u16 = 0x123;
pub const SST_SYMBOLS: u16 = 0x124;
pub const SST_ALIGN_SYM: u16 = 0x125;
pub const SST_SRC_LN_SEG: u16 = 0x126;
pub const SST_SRC_MODULE: u16 = 0x127;
pub const SST_LIBRARIES: u16 = 0x128;
pub const SST_GLOBAL_SYM: u16 = 0x129;
pub const SST_GLOBAL_PUB: u16 = 0x12a;
pub const SST_GLOBAL_TYPES: u16 = 0x12b;
pub const SST_MPC: u16 = 0x12c;
pub const SST_SEG_MAP: u16 = 0x12d;
pub const SST_SEG_NAME: u16 = 0x12e;
pub const SST_PRE_COMP: u16 = 0x12f;
pub const SST_PRE_COMP_MAP: u16 = 0x130;
pub const SST_OFF_MAP16: u16 = 0x131;
pub const SST_OFF_MAP32: u16 = 0x132;
pub const SST_FILE_INDEX: u16 = 0x133;
pub const SST_STATIC_SYM: u16 = 0x134;

pub const DIR_HEADER: u16 = 16;
pub const DIR_ENTRY: u16 = 12;

#[must_use]
pub fn sst_name(sst: u16) -> &'static str {
    match sst {
        SST_MODULE => "sstModule",
        SST_TYPES => "sstTypes",
        SST_PUBLIC => "sstPublic",
        SST_PUBLIC_SYM => "sstPublicSym",
        SST_SYMBOLS => "sstSymbols",
        SST_ALIGN_SYM => "sstAlignSym",
        SST_SRC_LN_SEG => "sstSrcLnSeg",
        SST_SRC_MODULE => "sstSrcModule",
        SST_LIBRARIES => "sstLibraries",
        SST_GLOBAL_SYM => "sstGlobalSym",
        SST_GLOBAL_PUB => "sstGlobalPub",
        SST_GLOBAL_TYPES => "sstGlobalTypes",
        SST_MPC => "sstMPC",
        SST_SEG_MAP => "sstSegMap",
        SST_SEG_NAME => "sstSegName",
        SST_PRE_COMP => "sstPreComp",
        SST_PRE_COMP_MAP => "sstPreCompMap",
        SST_OFF_MAP16 => "sstOffMap16",
        SST_OFF_MAP32 => "sstOffMap32",
        SST_FILE_INDEX => "sstFileIndex",
        SST_STATIC_SYM => "sstStaticSym",
        _ => "?",
    }
}

#[must_use]
pub fn is_embedded(signature: &str) -> bool {
    matches!(signature, "NB09" | "NB11")
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Entry {
    pub sst: u16,
    pub imod: u16,
    pub lfo: u32,
    pub cb: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Nb<'a> {
    pub signature: [u8; 4],
    pub bytes: &'a [u8],
    pub dir_at: usize,
    pub dir_header: u16,
    pub entry_size: u16,
    pub lfo_next: u32,
    pub flags: u32,
    pub entries: Vec<Entry>,
    pub tail: &'a [u8],
}

impl<'a> Nb<'a> {
    pub fn parse(b: &'a [u8]) -> Result<Self> {
        let sig = bytes(b, 0, 4).context("a CodeView record shorter than its signature")?;
        let signature: [u8; 4] = sig.try_into().context("signature")?;
        if &signature != b"NB09" && &signature != b"NB11" {
            bail!(
                "not an NB09 or NB11 record (signature {:?})",
                msvc6_base::srcio::decode(&signature)
            );
        }
        let dir_at = us(u32_at(b, 4).context("the record's directory offset")?);
        let dir_header =
            u16_at(b, dir_at).with_context(|| format!("the directory header at {dir_at:#x}"))?;
        let entry_size = u16_at(b, dir_at.saturating_add(2))?;
        if dir_header < 8 {
            bail!("the directory header is {dir_header} byte(s), under the 8 that hold its count");
        }
        if entry_size < DIR_ENTRY {
            bail!("a directory entry is {entry_size} byte(s), under the {DIR_ENTRY} that hold one");
        }
        let count = us(u32_at(b, dir_at.saturating_add(4))?);
        let (lfo_next, flags) = if dir_header >= DIR_HEADER {
            (
                u32_at(b, dir_at.saturating_add(8))?,
                u32_at(b, dir_at.saturating_add(12))?,
            )
        } else {
            (0, 0)
        };
        let first = dir_at
            .checked_add(usize::from(dir_header))
            .context("directory offset overflows")?;
        let room = b.len().saturating_sub(first) / usize::from(entry_size);
        if count > room {
            bail!(
                "the directory claims {count} entries; the {} byte(s) after its header hold {room}",
                b.len().saturating_sub(first)
            );
        }
        let mut entries = Vec::with_capacity(count);
        for i in 0..count {
            let e = first.saturating_add(i.saturating_mul(usize::from(entry_size)));
            let entry = Entry {
                sst: u16_at(b, e)?,
                imod: u16_at(b, e.saturating_add(2))?,
                lfo: u32_at(b, e.saturating_add(4))?,
                cb: u32_at(b, e.saturating_add(8))?,
            };
            bytes(b, us(entry.lfo), us(entry.cb)).with_context(|| {
                format!(
                    "directory entry {i} ({} of module {}) at {:#x} + {:#x} runs past the record's {} bytes",
                    sst_name(entry.sst),
                    entry.imod,
                    entry.lfo,
                    entry.cb,
                    b.len()
                )
            })?;
            entries.push(entry);
        }
        let mut nb = Self {
            signature,
            bytes: b,
            dir_at,
            dir_header,
            entry_size,
            lfo_next,
            flags,
            tail: &[],
            entries,
        };
        nb.tail = b.get(nb.end()..).unwrap_or_default();
        Ok(nb)
    }

    #[must_use]
    pub fn data(&self, e: &Entry) -> &'a [u8] {
        bytes(self.bytes, us(e.lfo), us(e.cb)).unwrap_or_default()
    }

    #[must_use]
    pub fn signature_text(&self) -> String {
        msvc6_base::srcio::decode(&self.signature)
    }

    #[must_use]
    pub fn end(&self) -> usize {
        let dir_end = self
            .dir_at
            .saturating_add(usize::from(self.dir_header))
            .saturating_add(
                self.entries
                    .len()
                    .saturating_mul(usize::from(self.entry_size)),
            );
        self.entries
            .iter()
            .map(|e| us(e.lfo).saturating_add(us(e.cb)))
            .fold(dir_end.max(8), usize::max)
    }

    pub fn write(&self) -> Result<Vec<u8>> {
        let mut out = vec![0u8; self.end()];
        put(&mut out, 0, &self.signature)?;
        put(
            &mut out,
            4,
            &u32_of(self.dir_at, "directory offset")?.to_le_bytes(),
        )?;
        let mut head = Vec::with_capacity(usize::from(self.dir_header));
        head.extend_from_slice(&self.dir_header.to_le_bytes());
        head.extend_from_slice(&self.entry_size.to_le_bytes());
        head.extend_from_slice(&u32_of(self.entries.len(), "directory count")?.to_le_bytes());
        if self.dir_header >= DIR_HEADER {
            head.extend_from_slice(&self.lfo_next.to_le_bytes());
            head.extend_from_slice(&self.flags.to_le_bytes());
        }
        head.resize(usize::from(self.dir_header), 0);
        put(&mut out, self.dir_at, &head)?;
        let first = self.dir_at.saturating_add(usize::from(self.dir_header));
        for (i, e) in self.entries.iter().enumerate() {
            let mut d = Vec::with_capacity(usize::from(self.entry_size));
            d.extend_from_slice(&e.sst.to_le_bytes());
            d.extend_from_slice(&e.imod.to_le_bytes());
            d.extend_from_slice(&e.lfo.to_le_bytes());
            d.extend_from_slice(&e.cb.to_le_bytes());
            d.resize(usize::from(self.entry_size), 0);
            put(
                &mut out,
                first.saturating_add(i.saturating_mul(usize::from(self.entry_size))),
                &d,
            )?;
            put(&mut out, us(e.lfo), self.data(e))?;
        }
        out.extend_from_slice(self.tail);
        Ok(out)
    }
}

fn put(out: &mut [u8], at: usize, b: &[u8]) -> Result<()> {
    out.get_mut(at..at.saturating_add(b.len()))
        .with_context(|| format!("{} byte(s) at {at:#x} past the record", b.len()))?
        .copy_from_slice(b);
    Ok(())
}

pub fn pack(signature: &[u8; 4], subsections: &[(u16, u16, Vec<u8>)]) -> Result<Vec<u8>> {
    let mut nb = signature.to_vec();
    nb.extend_from_slice(&[0u8; 4]);
    let mut placed = Vec::with_capacity(subsections.len());
    for (sst, imod, body) in subsections {
        let at = nb.len();
        nb.extend_from_slice(body);
        nb.resize(nb.len().next_multiple_of(4), 0);
        placed.push((*sst, *imod, at, body.len()));
    }
    let dir = u32_of(nb.len(), "directory offset")?;
    put(&mut nb, 4, &dir.to_le_bytes())?;
    nb.extend_from_slice(&DIR_HEADER.to_le_bytes());
    nb.extend_from_slice(&DIR_ENTRY.to_le_bytes());
    nb.extend_from_slice(&u32_of(placed.len(), "directory count")?.to_le_bytes());
    nb.extend_from_slice(&[0u8; 8]);
    for (sst, imod, at, cb) in placed {
        nb.extend_from_slice(&sst.to_le_bytes());
        nb.extend_from_slice(&imod.to_le_bytes());
        nb.extend_from_slice(&u32_of(at, "subsection offset")?.to_le_bytes());
        nb.extend_from_slice(&u32_of(cb, "subsection size")?.to_le_bytes());
    }
    Ok(nb)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Found {
    DebugDirectory,
    Trailer,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Located {
    pub signature: String,
    pub offset: usize,
    pub len: usize,
    pub found: Found,
}

pub fn trailer(file: &[u8]) -> Option<Located> {
    let n = file.len();
    let tail = file.get(n.checked_sub(8)?..)?;
    let sig = tail.get(..4)?;
    if !sig.starts_with(b"NB") {
        return None;
    }
    let back = us(u32_at(tail, 4).ok()?);
    let start = n.checked_sub(back)?;
    if file.get(start..start.checked_add(4)?)? != sig {
        return None;
    }
    Some(Located {
        signature: msvc6_base::srcio::decode(sig),
        offset: start,
        len: back,
        found: Found::Trailer,
    })
}

pub fn locate(img: &Image) -> Result<Option<Located>> {
    if let Some(cv) = img.codeview()? {
        let start = us(cv.file_offset);
        if is_embedded(&cv.format) {
            bytes(img.bytes(), start, us(cv.len)).with_context(|| {
                format!(
                    "the {} record at {start:#x} + {:#x} runs past the file",
                    cv.format, cv.len
                )
            })?;
            return Ok(Some(Located {
                signature: cv.format,
                offset: start,
                len: us(cv.len),
                found: Found::DebugDirectory,
            }));
        }
    }
    Ok(trailer(img.bytes()).filter(|t| is_embedded(&t.signature)))
}

pub fn record_of(img: &Image) -> Result<&[u8]> {
    let l = locate(img)?.context(
        "no NB09 or NB11 CodeView record: neither in the debug directory nor at the file's end",
    )?;
    bytes(img.bytes(), l.offset, l.len)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_packed_record_parses_and_writes_back_byte_for_byte() {
        let nb = pack(
            b"NB11",
            &[
                (SST_MODULE, 1, vec![1, 2, 3]),
                (SST_GLOBAL_TYPES, 0xffff, vec![9; 8]),
            ],
        )
        .unwrap();
        let p = Nb::parse(&nb).unwrap();
        assert_eq!(p.entries.len(), 2);
        assert_eq!(p.data(&p.entries[0]), [1, 2, 3]);
        assert_eq!(p.write().unwrap(), nb);
    }

    #[test]
    fn a_directory_that_cannot_fit_is_refused() {
        let mut nb = pack(b"NB11", &[(SST_MODULE, 1, vec![1, 2, 3])]).unwrap();
        let dir = us(u32_at(&nb, 4).unwrap());
        nb[dir + 4..dir + 8].copy_from_slice(&0xffff_ffffu32.to_le_bytes());
        assert!(Nb::parse(&nb).unwrap_err().to_string().contains("claims"));
        let mut nb = pack(b"NB11", &[(SST_MODULE, 1, vec![1, 2, 3])]).unwrap();
        nb[dir + 2..dir + 4].copy_from_slice(&0u16.to_le_bytes());
        assert!(Nb::parse(&nb).is_err());
        let mut nb = pack(b"NB11", &[(SST_MODULE, 1, vec![1, 2, 3])]).unwrap();
        nb[dir + 16 + 8..dir + 16 + 12].copy_from_slice(&0x7fff_ffffu32.to_le_bytes());
        assert!(
            Nb::parse(&nb)
                .unwrap_err()
                .to_string()
                .contains("runs past")
        );
        assert!(Nb::parse(b"NB10\0\0\0\0").is_err());
        assert!(Nb::parse(b"NB1").is_err());
    }

    #[test]
    fn a_trailer_points_back_at_its_signature() {
        let mut file = vec![0u8; 16];
        file.extend_from_slice(b"NB09");
        file.extend_from_slice(&[0; 8]);
        file.extend_from_slice(b"NB09");
        file.extend_from_slice(&20u32.to_le_bytes());
        let t = trailer(&file).unwrap();
        assert_eq!((t.offset, t.len, t.signature.as_str()), (16, 20, "NB09"));
        let mut bad = file.clone();
        let n = bad.len();
        bad[n - 4..].copy_from_slice(&0xffffu32.to_le_bytes());
        assert!(trailer(&bad).is_none());
    }
}
