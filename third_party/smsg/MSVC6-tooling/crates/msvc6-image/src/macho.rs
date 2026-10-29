use anyhow::{Context as _, Result, bail};
use serde::Serialize;
use std::collections::BTreeMap;

pub const MAGIC: u32 = 0xfeed_face;

pub const MAGIC_64: u32 = 0xfeed_facf;

pub const FAT_MAGIC: u32 = 0xcafe_babe;

pub const CPU_I386: u32 = 7;

pub const CPU_PPC: u32 = 18;

pub const HEADER: usize = 28;

pub const FAT_ARCH: usize = 20;

pub const SEGMENT_HEADER: usize = 56;

pub const SECTION_HEADER: usize = 68;

pub const NLIST: usize = 12;

pub const MAX_NAME: usize = 4096;

pub const LC_SEGMENT: u32 = 0x1;

pub const LC_SYMTAB: u32 = 0x2;

pub const LC_DYSYMTAB: u32 = 0xb;

pub const S_ZEROFILL: u32 = 0x1;

pub const S_NON_LAZY_SYMBOL_POINTERS: u32 = 0x6;

pub const S_LAZY_SYMBOL_POINTERS: u32 = 0x7;

pub const S_SYMBOL_STUBS: u32 = 0x8;

pub const S_ATTR_PURE_INSTRUCTIONS: u32 = 0x8000_0000;

pub const S_ATTR_SOME_INSTRUCTIONS: u32 = 0x0000_0400;

pub const INDIRECT_SYMBOL_LOCAL: u32 = 0x8000_0000;

pub const INDIRECT_SYMBOL_ABS: u32 = 0x4000_0000;

pub const N_STAB: u8 = 0xe0;

pub const N_TYPE: u8 = 0x0e;

pub const N_SECT: u8 = 0x0e;

pub const N_EXT: u8 = 0x01;

#[must_use]
pub fn cpu_name(cpu: u32) -> String {
    match cpu {
        CPU_I386 => "i386".into(),
        CPU_PPC => "ppc".into(),
        n => format!("cpu{n}"),
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Slice {
    pub cpu: u32,
    pub offset: usize,
    pub size: usize,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Section {
    pub index: usize,
    pub segment: String,
    pub name: String,
    pub addr: u64,
    pub size: u64,
    pub offset: u32,
    pub flags: u32,
    pub reserved1: u32,
    pub reserved2: u32,
}

impl Section {
    #[must_use]
    pub fn kind(&self) -> u32 {
        self.flags & 0xff
    }

    #[must_use]
    pub fn is_code(&self) -> bool {
        self.flags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS) != 0
    }

    #[must_use]
    pub fn label(&self) -> String {
        format!("{},{}", self.segment, self.name)
    }

    #[must_use]
    pub fn end(&self) -> u64 {
        self.addr.saturating_add(self.size)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Symbol {
    pub va: u64,
    pub name: String,
    pub section: usize,
    pub code: bool,
    pub external: bool,
}

#[derive(Debug, Clone)]
pub struct MachO {
    pub cpu: u32,
    pub big_endian: bool,
    pub sections: Vec<Section>,
    pub symbols: Vec<Symbol>,
    pub stubs: BTreeMap<u64, String>,
    pub slots: BTreeMap<u64, String>,
    bytes: Vec<u8>,
}

#[derive(Clone, Copy)]
struct Reader<'a> {
    b: &'a [u8],
    big: bool,
}

impl Reader<'_> {
    fn u32(&self, at: usize) -> Option<u32> {
        let s = self.b.get(at..at.checked_add(4)?)?;
        let a = <[u8; 4]>::try_from(s).ok()?;
        Some(if self.big {
            u32::from_be_bytes(a)
        } else {
            u32::from_le_bytes(a)
        })
    }

    fn need(&self, at: usize, what: &str) -> Result<u32> {
        self.u32(at).with_context(|| {
            format!(
                "{what} at {at:#x} runs past the {}-byte slice",
                self.b.len()
            )
        })
    }
}

fn us(v: u32) -> usize {
    usize::try_from(v).unwrap_or(usize::MAX)
}

fn cname(b: &[u8], at: usize, max: usize) -> String {
    let s = b.get(at..).unwrap_or_default();
    let s = s.get(..s.len().min(max)).unwrap_or_default();
    let end = s.iter().position(|c| *c == 0).unwrap_or(s.len());
    msvc6_base::srcio::decode(s.get(..end).unwrap_or_default())
}

fn unprefixed(raw: String) -> String {
    match raw.strip_prefix('_') {
        Some(s) => s.to_string(),
        None => raw,
    }
}

pub fn slices(bytes: &[u8]) -> Result<Vec<Slice>> {
    let be = Reader {
        b: bytes,
        big: true,
    };
    let le = Reader {
        b: bytes,
        big: false,
    };
    match (be.u32(0), le.u32(0)) {
        (Some(FAT_MAGIC), _) => {
            let n = us(be.need(4, "fat header")?);
            let room = bytes.len().saturating_sub(8) / FAT_ARCH;
            if n > room {
                bail!("{n} fat arch(es), but the file holds room for {room}");
            }
            let mut out = Vec::with_capacity(n);
            for k in 0..n {
                let e = k
                    .checked_mul(FAT_ARCH)
                    .and_then(|o| o.checked_add(8))
                    .context("fat arch offset overflows")?;
                let cpu = be.need(e, "fat arch")?;
                let offset = us(be.need(e + 8, "fat arch")?);
                let size = us(be.need(e + 12, "fat arch")?);
                if offset.checked_add(size).is_none_or(|end| end > bytes.len()) {
                    bail!(
                        "fat arch {} ({}): {size} byte(s) at {offset:#x} run past the {}-byte file",
                        k,
                        cpu_name(cpu),
                        bytes.len()
                    );
                }
                out.push(Slice { cpu, offset, size });
            }
            Ok(out)
        }
        (Some(MAGIC), _) => Ok(vec![Slice {
            cpu: be.need(4, "header")?,
            offset: 0,
            size: bytes.len(),
        }]),
        (_, Some(MAGIC)) => Ok(vec![Slice {
            cpu: le.need(4, "header")?,
            offset: 0,
            size: bytes.len(),
        }]),
        (Some(MAGIC_64), _) | (_, Some(MAGIC_64)) => {
            bail!("a 64-bit Mach-O; this reader takes 32-bit slices")
        }
        _ => bail!("not a Mach-O image (thin or fat)"),
    }
}

pub fn slice_of(bytes: &[u8], cpu: u32) -> Result<&[u8]> {
    let all = slices(bytes)?;
    let s = all.iter().find(|s| s.cpu == cpu).with_context(|| {
        format!(
            "no {} slice (the image holds {})",
            cpu_name(cpu),
            all.iter()
                .map(|s| cpu_name(s.cpu))
                .collect::<Vec<_>>()
                .join(", ")
        )
    })?;
    bytes
        .get(s.offset..s.offset.saturating_add(s.size))
        .context("slice past the file")
}

struct Commands {
    sections: Vec<Section>,
    symtab: Option<(usize, usize, usize, usize)>,
    indirect: Option<(usize, usize)>,
}

fn segment(r: Reader<'_>, off: usize, size: usize, sections: &mut Vec<Section>) -> Result<()> {
    let nsects = us(r.need(off + 48, "segment")?);
    let room = size.saturating_sub(SEGMENT_HEADER) / SECTION_HEADER;
    if nsects > room {
        bail!("a segment declares {nsects} section(s) in room for {room}");
    }
    for k in 0..nsects {
        let s = k
            .checked_mul(SECTION_HEADER)
            .and_then(|o| o.checked_add(off.checked_add(SEGMENT_HEADER)?))
            .context("section header offset overflows")?;
        sections.push(Section {
            index: sections.len(),
            name: cname(r.b, s, 16),
            segment: cname(r.b, s + 16, 16),
            addr: u64::from(r.need(s + 32, "section")?),
            size: u64::from(r.need(s + 36, "section")?),
            offset: r.need(s + 40, "section")?,
            flags: r.need(s + 56, "section")?,
            reserved1: r.need(s + 60, "section")?,
            reserved2: r.need(s + 64, "section")?,
        });
    }
    Ok(())
}

fn commands(r: Reader<'_>) -> Result<Commands> {
    let ncmds = us(r.need(16, "header")?);
    let sizeofcmds = us(r.need(20, "header")?);
    let end = HEADER
        .checked_add(sizeofcmds)
        .filter(|e| *e <= r.b.len())
        .with_context(|| {
            format!(
                "{sizeofcmds} byte(s) of load commands run past the {}-byte slice",
                r.b.len()
            )
        })?;
    if ncmds > sizeofcmds / 8 {
        bail!("{ncmds} load command(s) in {sizeofcmds} byte(s)");
    }
    let mut out = Commands {
        sections: Vec::new(),
        symtab: None,
        indirect: None,
    };
    let mut off = HEADER;
    for i in 0..ncmds {
        let cmd = r.need(off, "load command")?;
        let size = us(r.need(off + 4, "load command")?);
        if size < 8 || !size.is_multiple_of(4) || off.saturating_add(size) > end {
            bail!("load command {i} at {off:#x}: size {size}");
        }
        match cmd {
            LC_SEGMENT => segment(r, off, size, &mut out.sections)?,
            LC_SYMTAB => {
                out.symtab = Some((
                    us(r.need(off + 8, "symtab")?),
                    us(r.need(off + 12, "symtab")?),
                    us(r.need(off + 16, "symtab")?),
                    us(r.need(off + 20, "symtab")?),
                ));
            }
            LC_DYSYMTAB => {
                out.indirect = Some((
                    us(r.need(off + 56, "dysymtab")?),
                    us(r.need(off + 60, "dysymtab")?),
                ));
            }
            _ => {}
        }
        off = off.saturating_add(size);
    }
    Ok(out)
}

fn indirect_names(
    r: Reader<'_>,
    sections: &[Section],
    names: &[String],
    table: Option<(usize, usize)>,
) -> Result<(BTreeMap<u64, String>, BTreeMap<u64, String>)> {
    let mut stubs = BTreeMap::new();
    let mut slots = BTreeMap::new();
    let Some((ioff, n)) = table else {
        return Ok((stubs, slots));
    };
    if n.checked_mul(4)
        .and_then(|k| k.checked_add(ioff))
        .is_none_or(|e| e > r.b.len())
    {
        bail!(
            "{n} indirect symbol(s) at {ioff:#x} run past the {}-byte slice",
            r.b.len()
        );
    }
    let isym = |i: usize| -> Option<String> {
        if i >= n {
            return None;
        }
        let v = r.u32(ioff.checked_add(i.checked_mul(4)?)?)?;
        if v & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS) != 0 {
            return None;
        }
        names.get(us(v)).cloned()
    };
    for s in sections {
        let (step, into) = match s.kind() {
            S_SYMBOL_STUBS if s.reserved2 > 0 => (u64::from(s.reserved2), &mut stubs),
            S_NON_LAZY_SYMBOL_POINTERS | S_LAZY_SYMBOL_POINTERS => (4, &mut slots),
            _ => continue,
        };
        let first = us(s.reserved1);
        let count = usize::try_from(s.size / step)
            .unwrap_or(usize::MAX)
            .min(n.saturating_sub(first));
        for j in 0..count {
            let Some(name) = first.checked_add(j).and_then(&isym) else {
                continue;
            };
            let at = u64::try_from(j)
                .ok()
                .and_then(|j| j.checked_mul(step))
                .and_then(|o| s.addr.checked_add(o));
            if let Some(at) = at {
                into.insert(at, name);
            }
        }
    }
    Ok((stubs, slots))
}

pub fn parse(slice: &[u8]) -> Result<MachO> {
    let big = match (be_word(slice), le_word(slice)) {
        (Some(MAGIC), _) => true,
        (_, Some(MAGIC)) => false,
        _ => bail!("not a 32-bit Mach-O slice"),
    };
    let r = Reader { b: slice, big };
    let cpu = r.need(4, "header")?;
    let c = commands(r)?;
    let (symoff, nsyms, stroff, strsize) = c
        .symtab
        .context("no LC_SYMTAB: a stripped image names nothing")?;
    if nsyms
        .checked_mul(NLIST)
        .and_then(|k| k.checked_add(symoff))
        .is_none_or(|e| e > slice.len())
    {
        bail!(
            "{nsyms} symbol(s) at {symoff:#x} run past the {}-byte slice",
            slice.len()
        );
    }
    let strs = slice
        .get(stroff..stroff.saturating_add(strsize))
        .with_context(|| {
            format!(
                "a {strsize}-byte string table at {stroff:#x} runs past the {}-byte slice",
                slice.len()
            )
        })?;
    let mut names: Vec<String> = Vec::with_capacity(nsyms);
    let mut symbols = Vec::new();
    for k in 0..nsyms {
        let e = k
            .checked_mul(NLIST)
            .and_then(|o| o.checked_add(symoff))
            .context("symbol offset overflows")?;
        let strx = us(r.need(e, "nlist")?);
        let ty = *slice.get(e + 4).context("nlist")?;
        let sect = *slice.get(e + 5).context("nlist")?;
        let value = u64::from(r.need(e + 8, "nlist")?);
        let name = unprefixed(cname(strs, strx, MAX_NAME));
        names.push(name.clone());
        if ty & N_STAB != 0 || ty & N_TYPE != N_SECT || sect == 0 {
            continue;
        }
        let si = usize::from(sect) - 1;
        symbols.push(Symbol {
            va: value,
            name,
            section: si,
            code: c.sections.get(si).is_some_and(Section::is_code),
            external: ty & N_EXT != 0,
        });
    }
    symbols.sort_by(|a, b| a.va.cmp(&b.va).then_with(|| a.name.cmp(&b.name)));
    let (stubs, slots) = indirect_names(r, &c.sections, &names, c.indirect)?;
    Ok(MachO {
        cpu,
        big_endian: big,
        sections: c.sections,
        symbols,
        stubs,
        slots,
        bytes: slice.to_vec(),
    })
}

fn be_word(b: &[u8]) -> Option<u32> {
    crate::be::u32_at(b, 0)
}

fn le_word(b: &[u8]) -> Option<u32> {
    msvc6_base::le::u32_at(b, 0).ok()
}

pub fn load(bytes: &[u8], cpu: u32) -> Result<MachO> {
    parse(slice_of(bytes, cpu)?)
}

impl MachO {
    #[must_use]
    pub fn section_bytes(&self, s: &Section) -> Option<&[u8]> {
        if s.kind() == S_ZEROFILL || s.size == 0 {
            return None;
        }
        let at = us(s.offset);
        let len = usize::try_from(s.size).ok()?;
        self.bytes.get(at..at.checked_add(len)?)
    }

    #[must_use]
    pub fn section(&self, segment: &str, name: &str) -> Option<&Section> {
        self.sections
            .iter()
            .find(|s| s.segment == segment && s.name == name)
    }

    #[must_use]
    pub fn functions(&self) -> Vec<(u64, u64, String)> {
        let mut out: Vec<(u64, u64, String)> = Vec::new();
        let code: Vec<&Symbol> = self.symbols.iter().filter(|s| s.code).collect();
        let mut later: Vec<Option<u64>> = vec![None; code.len()];
        let mut above: Option<u64> = None;
        for k in (0..code.len()).rev() {
            let va = code.get(k).map_or(0, |s| s.va);
            let next_va = code.get(k.saturating_add(1)).map(|s| s.va);
            if next_va.is_some_and(|n| n > va) {
                above = next_va;
            }
            if let Some(x) = later.get_mut(k) {
                *x = above.filter(|a| *a > va);
            }
        }
        for (k, s) in code.iter().enumerate() {
            let sect_end = self.sections.get(s.section).map_or(s.va, Section::end);
            let next = later
                .get(k)
                .copied()
                .flatten()
                .map_or(sect_end, |n| n.min(sect_end));
            if out.last().is_some_and(|(va, _, _)| *va == s.va) {
                continue;
            }
            if next > s.va {
                out.push((s.va, next, s.name.clone()));
            }
        }
        out
    }
}
