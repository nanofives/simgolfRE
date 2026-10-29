use anyhow::{Context as _, Result, bail};
use msvc6_base::le;
use msvc6_base::srcio;
use msvc6_base::units::{FileOffset, RawSize, Rva, Va, VirtSize, on_disk_len};
use serde::Serialize;
use std::path::Path;

use crate::us;

pub const DOS_MAGIC: &[u8; 2] = b"MZ";
pub const PE_MAGIC: &[u8; 4] = b"PE\0\0";
pub const PE32_MAGIC: u16 = 0x010b;
pub const PE32_PLUS_MAGIC: u16 = 0x020b;
pub const SECTION_HEADER: usize = 40;
pub const MAX_IMAGE: u64 = 1 << 30;
pub const DIRS_AT: usize = 96;
pub const IMAGE_SCN_CNT_CODE: u32 = 0x20;
pub const IMAGE_SCN_CNT_INITIALIZED_DATA: u32 = 0x40;
pub const IMAGE_SCN_CNT_UNINITIALIZED_DATA: u32 = 0x80;
pub const IMAGE_SCN_MEM_EXECUTE: u32 = 0x2000_0000;
pub const MAX_NAME: usize = 4096;
pub const NAME_BUDGET: usize = 4;
pub const NAME_ALLOWANCE: usize = 1024;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Section {
    pub index: usize,
    pub name: String,
    pub rva: Rva,
    pub virt_size: VirtSize,
    pub raw_off: FileOffset,
    pub raw_size: RawSize,
    pub flags: u32,
    pub header_off: usize,
}

impl Section {
    #[must_use]
    pub fn disk_len(&self) -> usize {
        on_disk_len(self.virt_size, self.raw_size)
    }

    #[must_use]
    pub fn mapped_len(&self) -> u32 {
        self.virt_size.get().max(self.raw_size.get())
    }

    #[must_use]
    pub fn end(&self) -> Rva {
        Rva(self.rva.get().saturating_add(self.mapped_len()))
    }

    #[must_use]
    pub fn contains(&self, rva: Rva) -> bool {
        rva.delta_from(self.rva)
            .is_some_and(|d| d < self.mapped_len())
    }

    #[must_use]
    pub fn is_code(&self) -> bool {
        self.flags & (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE) != 0
    }

    #[must_use]
    pub fn vsize_field(&self) -> usize {
        self.header_off.saturating_add(8)
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize)]
pub struct DataDir {
    pub rva: Rva,
    pub size: u32,
}

impl DataDir {
    #[must_use]
    pub fn is_present(&self) -> bool {
        self.rva.get() != 0 || self.size != 0
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Dir {
    Export = 0,
    Import = 1,
    Resource = 2,
    Exception = 3,
    Security = 4,
    BaseReloc = 5,
    Debug = 6,
    Tls = 9,
    LoadConfig = 10,
    BoundImport = 11,
    Iat = 12,
    DelayImport = 13,
}

pub const DIR_NAMES: [&str; 16] = [
    "Export",
    "Import",
    "Resource",
    "Exception",
    "Security",
    "BaseReloc",
    "Debug",
    "Arch",
    "GlobalPtr",
    "TLS",
    "LoadConfig",
    "BoundImport",
    "IAT",
    "DelayImport",
    "CLR",
    "Reserved",
];

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct OptionalHeader {
    pub magic: u16,
    pub linker_major: u8,
    pub linker_minor: u8,
    pub size_of_code: u32,
    pub size_of_init_data: u32,
    pub size_of_uninit_data: u32,
    pub entry_point: Rva,
    pub base_of_code: Rva,
    pub base_of_data: Rva,
    pub image_base: Va,
    pub section_alignment: u32,
    pub file_alignment: u32,
    pub os_version: (u16, u16),
    pub image_version: (u16, u16),
    pub subsystem_version: (u16, u16),
    pub win32_version: u32,
    pub size_of_image: u32,
    pub size_of_headers: u32,
    pub checksum: u32,
    pub subsystem: u16,
    pub dll_characteristics: u16,
    pub stack_reserve: u32,
    pub stack_commit: u32,
    pub heap_reserve: u32,
    pub heap_commit: u32,
    pub loader_flags: u32,
    pub number_of_dirs: u32,
}

#[derive(Debug, Clone)]
pub struct Image {
    data: Vec<u8>,
    pub e_lfanew: usize,
    pub machine: u16,
    pub timestamp: u32,
    pub symbol_table: u32,
    pub symbols: u32,
    pub characteristics: u16,
    pub opt_size: u16,
    pub opt_off: usize,
    pub opt: OptionalHeader,
    pub dirs: Vec<DataDir>,
    pub sections: Vec<Section>,
}

fn field<T>(r: std::result::Result<T, le::Short>, what: &str) -> Result<T> {
    r.with_context(|| format!("truncated {what}"))
}

impl Image {
    pub fn load(path: impl AsRef<Path>) -> Result<Self> {
        let path = path.as_ref();
        let data = srcio::read_bounded(path, MAX_IMAGE)
            .with_context(|| format!("reading {}", path.display()))?;
        Self::parse(data).with_context(|| path.display().to_string())
    }

    pub fn parse(data: Vec<u8>) -> Result<Self> {
        if data.get(..2) != Some(&DOS_MAGIC[..]) {
            bail!("not an MZ image");
        }
        let e_lfanew = us(field(le::u32_at(&data, 0x3c), "DOS header")?);
        if le::bytes_at(&data, e_lfanew, 4).ok() != Some(&PE_MAGIC[..]) {
            bail!("no PE signature at e_lfanew {e_lfanew:#x}");
        }
        let coff = e_lfanew.checked_add(4).context("e_lfanew overflows")?;
        let at = |o: usize| coff.checked_add(o).context("COFF header offset overflows");
        let machine = field(le::u16_at(&data, at(0)?), "COFF header")?;
        let nsections = field(le::u16_at(&data, at(2)?), "COFF header")?;
        let timestamp = field(le::u32_at(&data, at(4)?), "COFF header")?;
        let symbol_table = field(le::u32_at(&data, at(8)?), "COFF header")?;
        let symbols = field(le::u32_at(&data, at(12)?), "COFF header")?;
        let opt_size = field(le::u16_at(&data, at(16)?), "COFF header")?;
        let characteristics = field(le::u16_at(&data, at(18)?), "COFF header")?;
        let opt_off = at(20)?;
        let opt = Self::optional(&data, opt_off, opt_size)?;
        let room = usize::from(opt_size).saturating_sub(DIRS_AT) / 8;
        let ndirs = us(opt.number_of_dirs).min(16).min(room);
        let mut dirs = Vec::with_capacity(ndirs);
        for i in 0..ndirs {
            let base = opt_off
                .checked_add(DIRS_AT)
                .and_then(|b| b.checked_add(i.checked_mul(8)?))
                .context("data directory offset overflows")?;
            dirs.push(DataDir {
                rva: Rva(field(le::u32_at(&data, base), "data directory")?),
                size: field(le::u32_at(&data, base.saturating_add(4)), "data directory")?,
            });
        }
        let table = opt_off
            .checked_add(usize::from(opt_size))
            .context("section table offset overflows")?;
        let mut sections = Vec::with_capacity(usize::from(nsections));
        for i in 0..usize::from(nsections) {
            let o = i
                .checked_mul(SECTION_HEADER)
                .and_then(|k| table.checked_add(k))
                .context("section header offset overflows")?;
            let h = le::bytes_at(&data, o, SECTION_HEADER)
                .with_context(|| format!("section header {i} runs past the file"))?;
            let name_bytes = h.get(..8).unwrap_or_default();
            let end = name_bytes.iter().position(|b| *b == 0).unwrap_or(8);
            let g = |k: usize| le::u32_at(h, k).unwrap_or(0);
            sections.push(Section {
                index: i,
                name: srcio::decode(name_bytes.get(..end).unwrap_or_default()),
                virt_size: VirtSize(g(8)),
                rva: Rva(g(12)),
                raw_size: RawSize(g(16)),
                raw_off: FileOffset(us(g(20))),
                flags: g(36),
                header_off: o,
            });
        }
        Ok(Self {
            data,
            e_lfanew,
            machine,
            timestamp,
            symbol_table,
            symbols,
            characteristics,
            opt_size,
            opt_off,
            opt,
            dirs,
            sections,
        })
    }

    fn optional(data: &[u8], at: usize, size: u16) -> Result<OptionalHeader> {
        if usize::from(size) < DIRS_AT {
            bail!("optional header is {size} byte(s), under the {DIRS_AT} a PE32 header holds");
        }
        let h = le::bytes_at(data, at, DIRS_AT).context("truncated optional header")?;
        let magic = le::u16_at(h, 0)?;
        if magic == PE32_PLUS_MAGIC {
            bail!("a PE32+ (64-bit) image; Visual C++ 6.0 builds PE32 only");
        }
        if magic != PE32_MAGIC {
            bail!("not PE32 (optional header magic {magic:#x})");
        }
        let d = |o: usize| le::u32_at(h, o).unwrap_or(0);
        let w = |o: usize| le::u16_at(h, o).unwrap_or(0);
        Ok(OptionalHeader {
            magic,
            linker_major: le::u8_at(h, 2).unwrap_or(0),
            linker_minor: le::u8_at(h, 3).unwrap_or(0),
            size_of_code: d(4),
            size_of_init_data: d(8),
            size_of_uninit_data: d(12),
            entry_point: Rva(d(16)),
            base_of_code: Rva(d(20)),
            base_of_data: Rva(d(24)),
            image_base: Va(d(28)),
            section_alignment: d(32),
            file_alignment: d(36),
            os_version: (w(40), w(42)),
            image_version: (w(44), w(46)),
            subsystem_version: (w(48), w(50)),
            win32_version: d(52),
            size_of_image: d(56),
            size_of_headers: d(60),
            checksum: d(64),
            subsystem: w(68),
            dll_characteristics: w(70),
            stack_reserve: d(72),
            stack_commit: d(76),
            heap_reserve: d(80),
            heap_commit: d(84),
            loader_flags: d(88),
            number_of_dirs: d(92),
        })
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
    pub fn len(&self) -> usize {
        self.data.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    #[must_use]
    pub fn is_dll(&self) -> bool {
        self.characteristics & 0x2000 != 0
    }

    #[must_use]
    pub fn section(&self, name: &str) -> Option<&Section> {
        self.sections.iter().find(|s| s.name == name)
    }

    #[must_use]
    pub fn section_of(&self, rva: Rva) -> Option<&Section> {
        self.sections.iter().find(|s| s.contains(rva))
    }

    #[must_use]
    pub fn section_at_file(&self, off: usize) -> Option<&Section> {
        self.sections.iter().find(|s| {
            let lo = s.raw_off.get();
            off >= lo && off < lo.saturating_add(us(s.raw_size.get()))
        })
    }

    #[must_use]
    pub fn dir(&self, which: Dir) -> Option<DataDir> {
        self.dirs.get(which as usize).copied()
    }

    #[must_use]
    pub fn present(&self, which: Dir) -> Option<DataDir> {
        self.dir(which).filter(DataDir::is_present)
    }

    #[must_use]
    pub fn rva_to_off(&self, rva: Rva) -> Option<FileOffset> {
        if rva.get() < self.opt.size_of_headers && !self.sections.iter().any(|s| s.contains(rva)) {
            return Some(FileOffset(us(rva.get())));
        }
        let s = self.section_of(rva)?;
        let delta = rva.delta_from(s.rva)?;
        if delta >= s.raw_size.get() {
            return None;
        }
        s.raw_off.checked_add(us(delta))
    }

    #[must_use]
    pub fn off_to_rva(&self, off: usize) -> Option<Rva> {
        let s = self.section_at_file(off)?;
        let d = u32::try_from(off.checked_sub(s.raw_off.get())?).ok()?;
        s.rva.checked_add(d)
    }

    #[must_use]
    pub fn at(&self, off: FileOffset, len: usize) -> Option<&[u8]> {
        le::bytes_at(&self.data, off.get(), len).ok()
    }

    #[must_use]
    pub fn at_rva(&self, rva: Rva, len: usize) -> Option<&[u8]> {
        self.at(self.rva_to_off(rva)?, len)
    }

    #[must_use]
    pub fn u16_at_rva(&self, rva: Rva) -> Option<u16> {
        le::u16_at(&self.data, self.rva_to_off(rva)?.get()).ok()
    }

    #[must_use]
    pub fn u32_at_rva(&self, rva: Rva) -> Option<u32> {
        le::u32_at(&self.data, self.rva_to_off(rva)?.get()).ok()
    }

    #[must_use]
    pub fn section_bytes(&self, s: &Section) -> &[u8] {
        le::bytes_at(&self.data, s.raw_off.get(), s.disk_len()).unwrap_or_default()
    }

    #[must_use]
    pub fn content_and_zero_tail(&self, s: &Section) -> (usize, usize) {
        let vs = us(s.virt_size.get());
        let disk = self.section_bytes(s);
        let on_disk = disk.len().min(vs);
        let last = disk
            .get(..on_disk)
            .unwrap_or_default()
            .iter()
            .rposition(|b| *b != 0)
            .map_or(0, |i| i.saturating_add(1));
        (last, vs.saturating_sub(last))
    }

    #[must_use]
    pub fn cstr_at_rva(&self, rva: Rva) -> Option<String> {
        self.cstr_scan(rva).map(|(s, _)| s)
    }

    #[must_use]
    pub fn cstr_scan(&self, rva: Rva) -> Option<(String, usize)> {
        let off = self.rva_to_off(rva)?.get();
        let rest = self.data.get(off..)?;
        let rest = rest.get(..MAX_NAME).unwrap_or(rest);
        let end = rest.iter().position(|b| *b == 0).unwrap_or(rest.len());
        let read = end.saturating_add(1).min(rest.len());
        Some((srcio::decode(rest.get(..end)?), read))
    }

    #[must_use]
    pub fn name_budget(&self) -> usize {
        self.len()
            .saturating_mul(NAME_BUDGET)
            .saturating_add(MAX_NAME.saturating_mul(NAME_ALLOWANCE))
    }

    #[must_use]
    pub fn header_structs_end(&self) -> usize {
        self.opt_off
            .saturating_add(usize::from(self.opt_size))
            .saturating_add(self.sections.len().saturating_mul(SECTION_HEADER))
    }

    #[must_use]
    pub fn raw_left(&self, rva: Rva) -> usize {
        let Some(off) = self.rva_to_off(rva) else {
            return 0;
        };
        let end = match self.section_of(rva) {
            Some(s) => s.raw_off.get().saturating_add(us(s.raw_size.get())),
            None => us(self.opt.size_of_headers),
        };
        end.min(self.data.len()).saturating_sub(off.get())
    }

    #[must_use]
    pub fn first_raw(&self) -> usize {
        self.sections
            .iter()
            .map(|s| s.raw_off.get())
            .filter(|&o| o > 0)
            .min()
            .unwrap_or(self.data.len())
    }

    #[must_use]
    pub fn overlay(&self) -> usize {
        let end = self
            .sections
            .iter()
            .map(|s| s.raw_off.get().saturating_add(us(s.raw_size.get())))
            .max()
            .unwrap_or(0)
            .max(us(self.opt.size_of_headers));
        self.data.len().saturating_sub(end)
    }

    #[must_use]
    pub fn relocations(&self) -> Vec<(Rva, u16)> {
        let mut out = Vec::new();
        let Some(d) = self.present(Dir::BaseReloc) else {
            return out;
        };
        let Some(start) = self.rva_to_off(d.rva) else {
            return out;
        };
        let end = start.get().saturating_add(us(d.size)).min(self.data.len());
        let mut off = start.get();
        while off.saturating_add(8) <= end {
            let (Ok(page), Ok(block)) = (
                le::u32_at(&self.data, off),
                le::u32_at(&self.data, off.saturating_add(4)),
            ) else {
                break;
            };
            if block < 8 {
                break;
            }
            let n = us(block).saturating_sub(8) / 2;
            for i in 0..n {
                let at = off.saturating_add(8).saturating_add(i.saturating_mul(2));
                let Ok(e) = le::u16_at(&self.data, at) else {
                    break;
                };
                let kind = e >> 12;
                if kind != 0
                    && let Some(r) = page.checked_add(u32::from(e & 0x0fff))
                {
                    out.push((Rva(r), kind));
                }
            }
            off = off.saturating_add(us(block));
        }
        out
    }
}
