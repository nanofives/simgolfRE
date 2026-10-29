use anyhow::{Context as _, Result, bail};
use msvc6_base::le;
use msvc6_base::srcio;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::us;

pub const HEADER: usize = 20;
pub const SECTION_HEADER: usize = 40;
pub const SYMBOL: usize = 18;
pub const RELOC: usize = 10;
pub const LINE: usize = 6;
pub const MAX_NAME: usize = 4096;

pub const CLASS_EXTERNAL: u8 = 2;
pub const CLASS_STATIC: u8 = 3;
pub const CLASS_LABEL: u8 = 6;
pub const CLASS_FUNCTION: u8 = 101;
pub const CLASS_FILE: u8 = 103;
pub const CLASS_SECTION: u8 = 104;
pub const CLASS_WEAK_EXTERNAL: u8 = 105;

pub const SCN_CNT_CODE: u32 = 0x20;
pub const SCN_CNT_UNINITIALIZED_DATA: u32 = 0x80;
pub const SCN_LNK_COMDAT: u32 = 0x1000;

pub const SELECT_NODUPLICATES: u8 = 1;
pub const SELECT_ANY: u8 = 2;
pub const SELECT_SAME_SIZE: u8 = 3;
pub const SELECT_EXACT_MATCH: u8 = 4;
pub const SELECT_ASSOCIATIVE: u8 = 5;
pub const SELECT_LARGEST: u8 = 6;

pub const MACHINES: [u16; 6] = [0, 0x14c, 0x8664, 0x1c0, 0x1c2, 0x200];

pub const COMP_ID: &str = "@comp.id";

pub const NAME_ALLOWANCE: usize = 1024;
pub const NAME_BUDGET: usize = 4;

#[must_use]
pub fn selection_name(s: u8) -> &'static str {
    match s {
        SELECT_NODUPLICATES => "nodup",
        SELECT_ANY => "any",
        SELECT_SAME_SIZE => "samesize",
        SELECT_EXACT_MATCH => "exact",
        SELECT_ASSOCIATIVE => "assoc",
        SELECT_LARGEST => "largest",
        _ => "?",
    }
}

#[must_use]
pub fn section_align(characteristics: u32) -> u32 {
    match (characteristics >> 20) & 0xf {
        0 => 1,
        f => 1u32 << (f - 1).min(13),
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct FileHeader {
    pub machine: u16,
    pub sections: u16,
    pub timestamp: u32,
    pub symbol_table: u32,
    pub symbols: u32,
    pub opt_size: u16,
    pub characteristics: u16,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SectionHeader {
    pub number: u16,
    pub name: String,
    pub virt_size: u32,
    pub va: u32,
    pub size: u32,
    pub raw_ptr: u32,
    pub reloc_ptr: u32,
    pub line_ptr: u32,
    pub relocs: u16,
    pub lines: u16,
    pub characteristics: u32,
}

impl SectionHeader {
    #[must_use]
    pub fn is_comdat(&self) -> bool {
        self.characteristics & SCN_LNK_COMDAT != 0
    }

    #[must_use]
    pub fn is_uninit(&self) -> bool {
        self.characteristics & SCN_CNT_UNINITIALIZED_DATA != 0 || self.raw_ptr == 0
    }

    #[must_use]
    pub fn align(&self) -> u32 {
        section_align(self.characteristics)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Symbol {
    pub index: u32,
    pub name: String,
    pub value: u32,
    pub section: i16,
    pub typ: u16,
    pub class: u8,
    pub aux: Vec<[u8; SYMBOL]>,
}

impl Symbol {
    #[must_use]
    pub fn defined(&self) -> bool {
        self.section > 0
    }

    #[must_use]
    pub fn is_external(&self) -> bool {
        self.class == CLASS_EXTERNAL
    }

    #[must_use]
    pub fn is_function(&self) -> bool {
        self.typ & 0x30 == 0x20
    }

    #[must_use]
    pub fn section_number(&self) -> Option<u16> {
        u16::try_from(self.section).ok().filter(|n| *n > 0)
    }

    #[must_use]
    pub fn section_definition(&self) -> Option<SectionDef> {
        if self.class != CLASS_STATIC || self.section_number().is_none() {
            return None;
        }
        let a = self.aux.first()?;
        Some(SectionDef {
            len: le::u32_at(a, 0).ok()?,
            relocs: le::u16_at(a, 4).ok()?,
            lines: le::u16_at(a, 6).ok()?,
            checksum: le::u32_at(a, 8).ok()?,
            number: le::u16_at(a, 12).ok()?,
            selection: le::u8_at(a, 14).ok()?,
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct SectionDef {
    pub len: u32,
    pub relocs: u16,
    pub lines: u16,
    pub checksum: u32,
    pub number: u16,
    pub selection: u8,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Reloc {
    pub va: u32,
    pub symbol: u32,
    pub typ: u16,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Comdat {
    pub section: u16,
    pub selection: u8,
    pub associated: Option<u16>,
    pub symbol: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ImportObject {
    pub machine: u16,
    pub timestamp: u32,
    pub hint: u16,
    pub kind: u16,
    pub name_type: u16,
    pub symbol: String,
    pub dll: String,
}

impl ImportObject {
    pub fn parse(d: &[u8]) -> Option<Self> {
        if le::u16_at(d, 0).ok()? != 0 || le::u16_at(d, 2).ok()? != 0xffff {
            return None;
        }
        let ty = le::u16_at(d, 18).ok()?;
        let rest = d.get(20..)?;
        let mut parts = rest.split(|b| *b == 0);
        let symbol = srcio::decode(parts.next()?);
        let dll = srcio::decode(parts.next()?);
        Some(Self {
            machine: le::u16_at(d, 6).ok()?,
            timestamp: le::u32_at(d, 8).ok()?,
            hint: le::u16_at(d, 16).ok()?,
            kind: ty & 3,
            name_type: (ty >> 2) & 7,
            symbol,
            dll,
        })
    }

    #[must_use]
    pub fn name_type_name(&self) -> &'static str {
        match self.name_type {
            0 => "ordinal",
            1 => "name",
            2 => "noprefix",
            3 => "undecorate",
            _ => "?",
        }
    }
}

#[must_use]
pub fn looks_like_object(d: &[u8]) -> bool {
    le::u16_at(d, 0).is_ok_and(|m| MACHINES.contains(&m)) && le::u16_at(d, 16).is_ok_and(|o| o == 0)
}

#[derive(Debug, Clone)]
pub struct Object<'a> {
    data: &'a [u8],
    pub header: FileHeader,
    pub sections: Vec<SectionHeader>,
    pub symbols: Vec<Symbol>,
    strtab: usize,
}

fn name8(raw: &[u8]) -> String {
    let end = raw.iter().position(|b| *b == 0).unwrap_or(raw.len());
    srcio::decode(raw.get(..end).unwrap_or_default())
}

impl<'a> Object<'a> {
    pub fn parse(data: &'a [u8]) -> Result<Self> {
        let h = le::bytes_at(data, 0, HEADER).context("not a COFF object: truncated header")?;
        let g = |k: usize| le::u32_at(h, k).unwrap_or(0);
        let w = |k: usize| le::u16_at(h, k).unwrap_or(0);
        let header = FileHeader {
            machine: w(0),
            sections: w(2),
            timestamp: g(4),
            symbol_table: g(8),
            symbols: g(12),
            opt_size: w(16),
            characteristics: w(18),
        };
        if header.machine == 0 && header.sections == 0xffff {
            bail!("an import object, not a COFF object");
        }
        let strtab = us(header.symbol_table)
            .checked_add(
                us(header.symbols)
                    .checked_mul(SYMBOL)
                    .context("symbol table overflows")?,
            )
            .context("symbol table overflows")?;
        if header.symbols != 0 && strtab > data.len() {
            bail!(
                "not a COFF object: {} symbol(s) at {:#x} run past the {} byte(s) there are",
                header.symbols,
                header.symbol_table,
                data.len()
            );
        }
        let mut o = Self {
            data,
            header,
            sections: Vec::new(),
            symbols: Vec::new(),
            strtab,
        };
        let budget = data
            .len()
            .saturating_mul(NAME_BUDGET)
            .saturating_add(MAX_NAME.saturating_mul(NAME_ALLOWANCE));
        let mut scanned = 0usize;
        let mut name_at = |o: &Self, off: usize| -> Result<Option<String>> {
            let got = o.scan(off);
            scanned = scanned.saturating_add(got.as_ref().map_or(0, |(_, n)| *n));
            if scanned > budget {
                bail!(
                    "not a COFF object: its names read {scanned} bytes of a {}-byte string table; names overlap",
                    o.string_table_end().saturating_sub(o.strtab)
                );
            }
            Ok(got.map(|(s, _)| s))
        };
        let table = HEADER.saturating_add(usize::from(header.opt_size));
        for i in 0..usize::from(header.sections) {
            let at = table.saturating_add(i.saturating_mul(SECTION_HEADER));
            let s = le::bytes_at(data, at, SECTION_HEADER)
                .with_context(|| format!("not a COFF object: section header {i} is truncated"))?;
            let g = |k: usize| le::u32_at(s, k).unwrap_or(0);
            let w = |k: usize| le::u16_at(s, k).unwrap_or(0);
            let raw = s.get(..8).unwrap_or_default();
            let name = match raw
                .strip_prefix(b"/")
                .and_then(|num| name8(num).parse::<usize>().ok())
            {
                Some(off) => name_at(&o, off)?.unwrap_or_else(|| name8(raw)),
                None => name8(raw),
            };
            o.sections.push(SectionHeader {
                number: u16::try_from(i.saturating_add(1)).unwrap_or(u16::MAX),
                name,
                virt_size: g(8),
                va: g(12),
                size: g(16),
                raw_ptr: g(20),
                reloc_ptr: g(24),
                line_ptr: g(28),
                relocs: w(32),
                lines: w(34),
                characteristics: g(36),
            });
        }
        let mut k = 0u32;
        while k < header.symbols {
            let at = us(header.symbol_table).saturating_add(us(k).saturating_mul(SYMBOL));
            let raw = le::bytes_at(data, at, SYMBOL)
                .with_context(|| format!("symbol {k} is truncated"))?;
            let naux = le::u8_at(raw, 17).unwrap_or(0);
            let name = if raw.get(..4) == Some(&[0, 0, 0, 0][..]) {
                name_at(&o, us(le::u32_at(raw, 4).unwrap_or(0)))?.unwrap_or_default()
            } else {
                name8(raw.get(..8).unwrap_or_default())
            };
            let mut aux = Vec::with_capacity(usize::from(naux));
            for a in 1..=u32::from(naux) {
                let ao = us(header.symbol_table)
                    .saturating_add(us(k.saturating_add(a)).saturating_mul(SYMBOL));
                let mut rec = [0u8; SYMBOL];
                if let Ok(b) = le::bytes_at(data, ao, SYMBOL) {
                    rec.copy_from_slice(b);
                }
                aux.push(rec);
            }
            o.symbols.push(Symbol {
                index: k,
                name,
                value: le::u32_at(raw, 8).unwrap_or(0),
                section: le::u16_at(raw, 12).unwrap_or(0).cast_signed(),
                typ: le::u16_at(raw, 14).unwrap_or(0),
                class: le::u8_at(raw, 16).unwrap_or(0),
                aux,
            });
            k = k.saturating_add(1).saturating_add(u32::from(naux));
        }
        Ok(o)
    }

    #[must_use]
    pub fn data(&self) -> &'a [u8] {
        self.data
    }

    #[must_use]
    pub fn string_table_end(&self) -> usize {
        let size = le::u32_at(self.data, self.strtab).map_or(0, us);
        self.strtab.saturating_add(size).min(self.data.len())
    }

    #[must_use]
    pub fn string(&self, off: usize) -> Option<String> {
        self.scan(off).map(|(s, _)| s)
    }

    fn scan(&self, off: usize) -> Option<(String, usize)> {
        let start = self.strtab.checked_add(off)?;
        let s = self.data.get(start..self.string_table_end())?;
        let s = s.get(..MAX_NAME).unwrap_or(s);
        let end = std::ffi::CStr::from_bytes_until_nul(s).map_or(s.len(), |c| c.to_bytes().len());
        let read = end.saturating_add(1).min(s.len());
        Some((srcio::decode(s.get(..end)?), read))
    }

    #[must_use]
    pub fn section(&self, number: u16) -> Option<&SectionHeader> {
        self.sections.get(usize::from(number).checked_sub(1)?)
    }

    #[must_use]
    pub fn section_data(&self, s: &SectionHeader) -> &'a [u8] {
        if s.raw_ptr == 0 {
            return &[];
        }
        le::bytes_at(self.data, us(s.raw_ptr), us(s.size)).unwrap_or_default()
    }

    #[must_use]
    pub fn relocations(&self, s: &SectionHeader) -> Vec<Reloc> {
        (0..usize::from(s.relocs))
            .filter_map(|i| {
                let at = us(s.reloc_ptr).checked_add(i.checked_mul(RELOC)?)?;
                let r = le::bytes_at(self.data, at, RELOC).ok()?;
                Some(Reloc {
                    va: le::u32_at(r, 0).ok()?,
                    symbol: le::u32_at(r, 4).ok()?,
                    typ: le::u16_at(r, 8).ok()?,
                })
            })
            .collect()
    }

    #[must_use]
    pub fn symbol(&self, index: u32) -> Option<&Symbol> {
        self.symbols
            .binary_search_by_key(&index, |s| s.index)
            .ok()
            .and_then(|i| self.symbols.get(i))
    }

    #[must_use]
    pub fn comp_ids(&self) -> Vec<u32> {
        self.symbols
            .iter()
            .filter(|s| s.name == COMP_ID)
            .map(|s| s.value)
            .collect()
    }

    #[must_use]
    pub fn comp_id(&self) -> Option<u32> {
        self.comp_ids().first().copied()
    }

    #[must_use]
    pub fn comdats(&self) -> Vec<Comdat> {
        let mut named: BTreeMap<i16, Vec<(u32, &str)>> = BTreeMap::new();
        for x in &self.symbols {
            if x.section_definition().is_none() {
                named
                    .entry(x.section)
                    .or_default()
                    .push((x.index, x.name.as_str()));
            }
        }
        let mut seen: BTreeSet<u16> = BTreeSet::new();
        let mut out = Vec::new();
        for s in &self.symbols {
            let Some(def) = s.section_definition() else {
                continue;
            };
            let Some(number) = s.section_number() else {
                continue;
            };
            if def.selection == 0
                || !self.section(number).is_some_and(SectionHeader::is_comdat)
                || !seen.insert(number)
            {
                continue;
            }
            let symbol = (def.selection != SELECT_ASSOCIATIVE)
                .then(|| {
                    let list = named.get(&s.section)?;
                    let at = list.partition_point(|(i, _)| *i <= s.index);
                    list.get(at).map(|(_, n)| (*n).to_string())
                })
                .flatten();
            out.push(Comdat {
                section: number,
                selection: def.selection,
                associated: (def.selection == SELECT_ASSOCIATIVE).then_some(def.number),
                symbol,
            });
        }
        out
    }

    #[must_use]
    pub fn defined_externals(&self) -> BTreeSet<String> {
        self.symbols
            .iter()
            .filter(|s| s.defined() && s.is_external() && !s.name.is_empty())
            .map(|s| s.name.clone())
            .collect()
    }

    #[must_use]
    pub fn undefined_externals(&self) -> (BTreeSet<String>, BTreeSet<String>) {
        let defined: BTreeSet<&str> = self
            .symbols
            .iter()
            .filter(|s| s.section != 0)
            .map(|s| s.name.as_str())
            .collect();
        let mut undefined = BTreeSet::new();
        let mut common = BTreeSet::new();
        for s in &self.symbols {
            if s.section != 0 || !s.is_external() || s.name.is_empty() {
                continue;
            }
            if defined.contains(s.name.as_str()) {
                continue;
            }
            if s.value == 0 {
                undefined.insert(s.name.clone());
            } else {
                common.insert(s.name.clone());
            }
        }
        (undefined, common)
    }

    #[must_use]
    pub fn code_bytes(&self) -> u64 {
        self.sections
            .iter()
            .filter(|s| s.name.starts_with(".text"))
            .map(|s| u64::from(s.size))
            .sum()
    }

    #[must_use]
    pub fn section_symbols(&self) -> BTreeMap<u16, Vec<(u32, String, bool)>> {
        let mut out: BTreeMap<u16, Vec<(u32, String, bool)>> = BTreeMap::new();
        for s in &self.symbols {
            let Some(n) = s.section_number() else {
                continue;
            };
            if !(s.class == CLASS_EXTERNAL || s.class == CLASS_STATIC)
                || s.name.is_empty()
                || s.name.starts_with('.')
                || s.name.starts_with('$')
            {
                continue;
            }
            out.entry(n)
                .or_default()
                .push((s.value, s.name.clone(), s.is_external()));
        }
        for v in out.values_mut() {
            v.sort_by(|a, b| a.0.cmp(&b.0).then(b.2.cmp(&a.2)).then(a.1.cmp(&b.1)));
            v.dedup();
        }
        out
    }
}

pub fn check_tables(o: &Object<'_>) -> Result<()> {
    let len = o.data().len();
    let mut tables: Vec<(usize, usize, u16)> = Vec::new();
    for s in &o.sections {
        let data = s.size as usize;
        if s.raw_ptr != 0
            && (s.raw_ptr as usize)
                .checked_add(data)
                .is_none_or(|e| e > len)
        {
            bail!(
                "section {} `{}`: {data} byte(s) at {:#x} run past the {len} there are",
                s.number,
                s.name,
                s.raw_ptr
            );
        }
        if s.relocs == 0 {
            continue;
        }
        let at = s.reloc_ptr as usize;
        let n = usize::from(s.relocs)
            .checked_mul(RELOC)
            .context("a relocation table overflows")?;
        let end = at.checked_add(n).context("a relocation table overflows")?;
        if end > len {
            bail!(
                "section {} `{}`: {} relocation(s) at {at:#x} run past the {len} byte(s) there are",
                s.number,
                s.name,
                s.relocs
            );
        }
        tables.push((at, end, s.number));
    }
    tables.sort_unstable();
    for w in tables.windows(2) {
        if let [a, b] = w
            && b.0 < a.1
        {
            bail!(
                "sections {} and {} share relocation bytes at {:#x}; a compiler writes one table per section",
                a.2,
                b.2,
                b.0
            );
        }
    }
    Ok(())
}
