use crate::dbi::Dbi;
use crate::info::{guid_text, read_info};
use crate::le::{put, u32_at, us};
use crate::{msf, msf7};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_base::digest::md5_hex;
use msvc6_image::pe::Image;
use serde::{Deserialize, Serialize};

const DIR_DEBUG: usize = 6;
const CODEVIEW: u32 = 2;
const SCN_INITIALIZED_DATA: u32 = 0x40;
const SCN_EXECUTE: u32 = 0x2000_0000;
const SCN_WRITE: u32 = 0x8000_0000;
const ENTRY: usize = 28;

#[derive(Debug, Clone, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Shipped {
    pub md5: String,
    pub what: String,
    pub file: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PdbId {
    pub is70: bool,
    pub signature: u32,
    pub age: u32,
    pub guid: Option<[u8; 16]>,
    pub sections: Vec<u8>,
}

impl PdbId {
    #[must_use]
    pub fn ident(&self) -> String {
        match self.guid {
            Some(g) if self.is70 => {
                format!("PDB 7.0, RSDS GUID {} age {}", guid_text(&g), self.age)
            }
            _ => format!(
                "PDB 2.0, NB10 signature {:#010x} age {}",
                self.signature, self.age
            ),
        }
    }
}

pub fn pdb_id(file: &[u8]) -> Result<PdbId> {
    let is70 = msf7::is_pdb70(file);
    let (info, dbi, sections) = if is70 {
        let m = msf7::Msf7::parse(file.to_vec())?;
        let d = Dbi::parse(&m.stream(3)?)?;
        let sh = d
            .section_headers()
            .context("the PDB has no section-headers stream to check the image against")?;
        (m.stream(1)?, d, m.stream(usize::from(sh))?)
    } else {
        let m = msf::Msf2::parse(file.to_vec())?;
        let d = Dbi::parse(&m.stream(3)?)?;
        let sh = d
            .section_headers()
            .context("the PDB has no section-headers stream to check the image against")?;
        (m.stream(1)?, d, m.stream(usize::from(sh))?)
    };
    let _ = dbi;
    let info = read_info(&info)?;
    Ok(PdbId {
        is70,
        signature: info.signature,
        age: info.age,
        guid: info.guid,
        sections,
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Change {
    pub at: usize,
    pub old: Vec<u8>,
    pub new: Vec<u8>,
    pub what: String,
}

#[derive(Debug, Clone)]
struct Sec {
    at: usize,
    name: String,
    vsize: u32,
    va: u32,
    raw_size: u32,
    raw_ptr: u32,
    chars: u32,
}

#[derive(Debug, Clone)]
struct Pe {
    timestamp: u32,
    opt: usize,
    image_size: u32,
    secs: Vec<Sec>,
    sec_table: Vec<u8>,
}

fn parse_pe(d: &[u8]) -> Result<Pe> {
    let img = Image::parse(d.to_vec()).context("the copy is not a PE32 image")?;
    if img.opt.number_of_dirs <= u32::try_from(DIR_DEBUG).unwrap_or(0) {
        bail!("the optional header has no debug data directory");
    }
    let table = img.opt_off + usize::from(img.opt_size);
    let n = img.sections.len();
    let secs = img
        .sections
        .iter()
        .map(|s| Sec {
            at: s.header_off,
            name: s.name.clone(),
            vsize: s.virt_size.get(),
            va: s.rva.get(),
            raw_size: s.raw_size.get(),
            raw_ptr: u32::try_from(s.raw_off.get()).unwrap_or(u32::MAX),
            chars: s.flags,
        })
        .collect();
    Ok(Pe {
        timestamp: img.timestamp,
        opt: img.opt_off,
        image_size: img.opt.size_of_image,
        sec_table: d
            .get(table..table + 40 * n)
            .context("the section table is truncated")?
            .to_vec(),
        secs,
    })
}

#[must_use]
pub fn pe_checksum(d: &[u8], csum_at: usize) -> u32 {
    let mut sum: u64 = 0;
    let mut i = 0usize;
    while i < d.len() {
        let w = if i == csum_at || i == csum_at + 2 {
            0
        } else {
            u64::from(d.get(i).copied().unwrap_or(0))
                | (u64::from(d.get(i + 1).copied().unwrap_or(0)) << 8)
        };
        sum += w;
        sum = (sum & 0xffff) + (sum >> 16);
        i += 2;
    }
    sum = (sum & 0xffff) + (sum >> 16);
    let len = u64::try_from(d.len()).unwrap_or(0);
    u32::try_from((sum + len) & 0xffff_ffff).unwrap_or(0)
}

fn record(id: &PdbId, file: &str) -> Result<Vec<u8>> {
    let mut r = Vec::with_capacity(24 + file.len() + 1);
    if id.is70 {
        r.extend_from_slice(b"RSDS");
        r.extend_from_slice(&id.guid.context("a 7.0 PDB without a GUID")?);
    } else {
        r.extend_from_slice(b"NB10");
        r.extend_from_slice(&0u32.to_le_bytes());
        r.extend_from_slice(&id.signature.to_le_bytes());
    }
    r.extend_from_slice(&id.age.to_le_bytes());
    if file.as_bytes().contains(&0) || !file.is_ascii() {
        bail!("the PDB's file name {file:?} must be plain ASCII");
    }
    r.extend_from_slice(file.as_bytes());
    r.push(0);
    Ok(r)
}

fn debug_dir_at(d: &[u8], pe: &Pe) -> Result<Option<(usize, usize)>> {
    let dd = pe.opt + 96 + 8 * DIR_DEBUG;
    let rva = u32_at(d, dd)?;
    let size = us(u32_at(d, dd + 4)?);
    if rva == 0 {
        return Ok(None);
    }
    let sec = pe
        .secs
        .iter()
        .find(|s| rva >= s.va && rva < s.va.saturating_add(s.vsize.max(s.raw_size)))
        .ok_or_else(|| anyhow!("the debug directory at rva {rva:#x} is in no section"))?;
    let at = us(rva - sec.va)
        .checked_add(us(sec.raw_ptr))
        .context("the debug directory's file offset overflows")?;
    if size / ENTRY > d.len() / ENTRY {
        bail!("the debug directory's {size} bytes run past the file");
    }
    Ok(Some((at, size / ENTRY)))
}

fn existing(d: &[u8], pe: &Pe) -> Result<Option<(Vec<u8>, u32, u32)>> {
    let Some((at, n)) = debug_dir_at(d, pe)? else {
        return Ok(None);
    };
    for i in 0..n {
        let e = at + ENTRY * i;
        if u32_at(d, e + 12)? != CODEVIEW {
            continue;
        }
        let ptr = us(u32_at(d, e + 24)?);
        let fmt = crate::le::bytes(d, ptr, 4)
            .context("the CodeView record")?
            .to_vec();
        if fmt == b"NB10" {
            return Ok(Some((fmt, u32_at(d, ptr + 8)?, u32_at(d, ptr + 12)?)));
        }
        return Ok(Some((fmt, 0, u32_at(d, ptr + 20)?)));
    }
    Ok(None)
}

fn old_entries(d: &[u8], pe: &Pe) -> Result<Vec<(Vec<u8>, u32)>> {
    let Some((at, n)) = debug_dir_at(d, pe)? else {
        return Ok(Vec::new());
    };
    let mut out = Vec::with_capacity(n);
    for i in 0..n {
        let e = crate::le::bytes(d, at + ENTRY * i, ENTRY)
            .context("the debug directory runs past the file")?;
        out.push((e.to_vec(), u32_at(e, 12)?));
    }
    Ok(out)
}

fn change(d: &mut [u8], at: usize, new: &[u8], what: &str, log: &mut Vec<Change>) -> Result<()> {
    let old = crate::le::bytes(d, at, new.len())
        .with_context(|| format!("{what} at {at:#x} runs past the file"))?
        .to_vec();
    put(d, at, new)?;
    log.push(Change {
        at,
        old,
        new: new.to_vec(),
        what: what.to_string(),
    });
    Ok(())
}

fn plan(d: &mut [u8], pe: &Pe, id: &PdbId, file: &str) -> Result<Vec<Change>> {
    let rec = record(id, file)?;
    let ro =
        |s: &&Sec| s.chars & SCN_INITIALIZED_DATA != 0 && s.chars & (SCN_EXECUTE | SCN_WRITE) == 0;
    let sec = pe
        .secs
        .iter()
        .filter(ro)
        .find(|s| s.name == ".rdata")
        .or_else(|| pe.secs.iter().find(ro))
        .ok_or_else(|| anyhow!("the image has no read-only data section to hold the record"))?;
    let olds = old_entries(d, pe)?;
    let has_cv = olds.iter().any(|e| e.1 == CODEVIEW);
    let entries = olds.len().max(1) + usize::from(!olds.is_empty() && !has_cv);
    let dir_len = u32::try_from(ENTRY * entries).context("directory length")?;
    let dir_off = sec.vsize.next_multiple_of(4);
    let rec_off = dir_off.checked_add(dir_len).context("record offset")?;
    let end = rec_off
        .checked_add(u32::try_from(rec.len()).context("record length")?)
        .context("record end")?;
    if end > sec.raw_size {
        bail!(
            "{} has {} byte(s) of slack past its VirtualSize {:#x} and the record needs {}",
            sec.name,
            sec.raw_size.saturating_sub(sec.vsize),
            sec.vsize,
            end - sec.vsize
        );
    }
    let limit = pe
        .secs
        .iter()
        .filter(|s| s.va > sec.va)
        .map(|s| s.va)
        .min()
        .unwrap_or(pe.image_size);
    if sec.va.saturating_add(end) > limit {
        bail!("the record would run into the next section at rva {limit:#x}");
    }
    let file_at = us(sec.raw_ptr);
    let slack = d
        .get(file_at + us(sec.vsize)..file_at + us(end))
        .context("the slack runs past the file")?;
    if slack.iter().any(|b| *b != 0) {
        bail!(
            "{}'s bytes past its VirtualSize are not all zero: not padding, so not room",
            sec.name
        );
    }
    let mut log = Vec::new();
    let cv_entry = |head: &[u8]| -> Result<Vec<u8>> {
        let mut e = head.get(..12).context("debug entry")?.to_vec();
        e.extend_from_slice(&CODEVIEW.to_le_bytes());
        e.extend_from_slice(&u32::try_from(rec.len()).context("record")?.to_le_bytes());
        e.extend_from_slice(&(sec.va + rec_off).to_le_bytes());
        e.extend_from_slice(&(sec.raw_ptr + rec_off).to_le_bytes());
        Ok(e)
    };
    let mut fresh = vec![0u8; 12];
    put(&mut fresh, 4, &pe.timestamp.to_le_bytes())?;
    let mut dir = Vec::with_capacity(us(dir_len));
    for (bytes, kind) in &olds {
        if *kind == CODEVIEW {
            dir.extend_from_slice(&cv_entry(bytes)?);
        } else {
            dir.extend_from_slice(bytes);
        }
    }
    if !has_cv {
        dir.extend_from_slice(&cv_entry(&fresh)?);
    }
    change(
        d,
        file_at + us(dir_off),
        &dir,
        "IMAGE_DEBUG_DIRECTORY (in the slack)",
        &mut log,
    )?;
    change(
        d,
        file_at + us(rec_off),
        &rec,
        if id.is70 {
            "RSDS record (in the slack)"
        } else {
            "NB10 record (in the slack)"
        },
        &mut log,
    )?;
    change(
        d,
        sec.at + 8,
        &end.to_le_bytes(),
        &format!("{} VirtualSize", sec.name),
        &mut log,
    )?;
    let dd = pe.opt + 96 + 8 * DIR_DEBUG;
    let mut ent = (sec.va + dir_off).to_le_bytes().to_vec();
    ent.extend_from_slice(&dir_len.to_le_bytes());
    change(d, dd, &ent, "DataDirectory[6] (debug)", &mut log)?;
    let csum_at = pe.opt + 64;
    if u32_at(d, csum_at)? != 0 {
        let c = pe_checksum(d, csum_at);
        change(
            d,
            csum_at,
            &c.to_le_bytes(),
            "optional header CheckSum",
            &mut log,
        )?;
    }
    Ok(log)
}

#[derive(Debug, Clone)]
pub struct Attached {
    pub shipped: Shipped,
    pub md5_in: String,
    pub md5_out: String,
    pub file: String,
    pub id: PdbId,
    pub changes: Vec<Change>,
    pub bytes: Vec<u8>,
}

impl Attached {
    #[must_use]
    pub fn recipe(&self) -> String {
        recipe(
            &self.shipped.what,
            &self.md5_in,
            &self.file,
            &self.id.ident(),
            &self.changes,
            &self.md5_out,
        )
    }

    #[must_use]
    pub fn differing(&self) -> usize {
        self.changes
            .iter()
            .map(|c| c.old.iter().zip(&c.new).filter(|(a, b)| a != b).count())
            .sum()
    }
}

pub fn refusal(md5: &str, shipped: &[Shipped]) -> String {
    let list = if shipped.is_empty() {
        "  (none: the image's [image.pdb] lists no shipped md5)".to_string()
    } else {
        shipped
            .iter()
            .map(|s| format!("  {}  {}", s.md5, s.what))
            .collect::<Vec<_>>()
            .join("\n")
    };
    format!(
        "md5 {md5} is not a shipped image this recipe accepts -- start from an untouched copy; the accepted md5s are:\n{list}"
    )
}

pub fn attach(copy: &[u8], shipped: &[Shipped], pdb: &[u8], file: &str) -> Result<Attached> {
    let md5 = md5_hex(copy);
    let ship = shipped
        .iter()
        .find(|s| s.md5.eq_ignore_ascii_case(&md5))
        .ok_or_else(|| anyhow!("{}", refusal(&md5, shipped)))?
        .clone();
    let pe = parse_pe(copy)?;
    let id = pdb_id(pdb)?;
    if id.sections != pe.sec_table {
        bail!(
            "the PDB is not one for {}: its section-headers stream is not the image's section table",
            ship.what
        );
    }
    let mut d = copy.to_vec();
    match existing(&d, &pe)? {
        Some((fmt, sig, age)) if fmt == b"NB10" => {
            if (sig, age) != (id.signature, id.age) {
                bail!(
                    "{} carries NB10 signature {sig:#010x} age {age}, and the PDB is {:#010x} age {}: not this image's PDB",
                    ship.what,
                    id.signature,
                    id.age
                );
            }
            if !id.is70 {
                return Ok(Attached {
                    shipped: ship,
                    md5_out: md5.clone(),
                    md5_in: md5,
                    file: file.to_string(),
                    id,
                    changes: Vec::new(),
                    bytes: d,
                });
            }
        }
        Some((fmt, ..)) => bail!(
            "{} already carries a {} record; this recipe starts from the shipped file",
            ship.what,
            crate::le::latin1(&fmt)
        ),
        None => {}
    }
    let csum_at = pe.opt + 64;
    let stored = u32_at(&d, csum_at)?;
    if stored != 0 && pe_checksum(&d, csum_at) != stored {
        bail!(
            "{}'s CheckSum is {stored:#x} and this tool's PE checksum of it is {:#x}: refusing to rewrite a checksum it cannot reproduce",
            ship.what,
            pe_checksum(&d, csum_at)
        );
    }
    let changes = plan(&mut d, &pe, &id, file)?;
    Ok(Attached {
        shipped: ship,
        md5_out: md5_hex(&d),
        md5_in: md5,
        file: file.to_string(),
        id,
        changes,
        bytes: d,
    })
}

#[must_use]
pub fn recipe(
    what: &str,
    md5_in: &str,
    file: &str,
    ident: &str,
    log: &[Change],
    md5_out: &str,
) -> String {
    let hexs = |b: &[u8]| -> String {
        if !b.is_empty() && b.iter().all(|x| *x == 0) {
            return format!("{} zero bytes", b.len());
        }
        b.chunks(4)
            .map(|c| c.iter().map(|x| format!("{x:02x}")).collect::<String>())
            .collect::<Vec<_>>()
            .join(" ")
    };
    let mut s = format!(
        "### {what} -- `{file}`, {ident}\n\n1. Your copy must be that file: md5 `{md5_in}`.\n"
    );
    if log.is_empty() {
        s.push_str(
            "2. Nothing to change: the shipped file's own `NB10` record already names this PDB by \
             its signature and age. Put the PDB on the debugger's symbol path.\n\n",
        );
        return s;
    }
    s.push_str(
        "2. At each FILE offset below (hexadecimal; not an address), check that the bytes there \
         now are the first column, and write the second. Every byte not listed stays as it is.\n\n\
         | file offset | bytes there now | write | what it is |\n|---|---|---|---|\n",
    );
    let mut sorted: Vec<&Change> = log.iter().collect();
    sorted.sort_by_key(|c| c.at);
    for c in sorted {
        s.push_str(&format!(
            "| `{:#08x}` | `{}` | `{}` | {} |\n",
            c.at,
            hexs(&c.old),
            hexs(&c.new),
            c.what
        ));
    }
    s.push_str(&format!(
        "\n3. The copy's md5 is now `{md5_out}`. Keep the PDB as `{file}` beside the copy or on the \
         debugger's symbol path.\n\n"
    ));
    s
}

#[must_use]
pub fn short_hex(b: &[u8]) -> String {
    if b.len() > 12 {
        let head: String = b.iter().take(8).map(|x| format!("{x:02x}")).collect();
        return format!("{head}..({} B)", b.len());
    }
    b.iter().map(|x| format!("{x:02x}")).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_pe_checksum_matches_a_known_value() {
        assert_eq!(pe_checksum(&[0, 0, 0, 0], 100), 4);
        assert_eq!(pe_checksum(&[0xff, 0xff, 0, 0], 0), 4);
        assert_eq!(pe_checksum(&[1, 0, 2, 0], 100), 3 + 4);
    }

    #[test]
    fn a_recipe_lists_every_change_by_file_offset() {
        let log = vec![
            Change {
                at: 0x200,
                old: vec![0; 4],
                new: vec![1, 2, 3, 4],
                what: "b".into(),
            },
            Change {
                at: 0x100,
                old: vec![9],
                new: vec![8],
                what: "a".into(),
            },
        ];
        let r = recipe("X", "m1", "x.pdb", "PDB 2.0", &log, "m2");
        let a = r.find("| a |").unwrap();
        let b = r.find("| b |").unwrap();
        assert!(a < b);
        assert!(r.contains("`4 zero bytes` | `01020304`"));
        assert!(recipe("X", "m", "x.pdb", "PDB 2.0", &[], "m").contains("Nothing to change"));
        assert_eq!(short_hex(&[0xab; 20]), "abababababababab..(20 B)");
    }
}
