use crate::buf::{Buf, n16, n32};

pub const MACHINE_I386: u16 = 0x14c;

pub const CLASS_EXTERNAL: u8 = 2;
pub const CLASS_STATIC: u8 = 3;
pub const CLASS_LABEL: u8 = 6;
pub const CLASS_FUNCTION: u8 = 101;
pub const CLASS_FILE: u8 = 103;
pub const CLASS_WEAK_EXTERNAL: u8 = 105;

pub const REL_DIR32: u16 = 6;
pub const REL_DIR32NB: u16 = 7;
pub const REL_REL32: u16 = 20;

pub const SCN_TEXT: u32 = 0x6050_0020;
pub const SCN_DATA: u32 = 0xc030_0040;
pub const SCN_RDATA: u32 = 0x4030_0040;
pub const SCN_BSS: u32 = 0xc030_0080;
pub const SCN_DRECTVE: u32 = 0x0010_0a00;
pub const SCN_DEBUG: u32 = 0x4210_0040;
pub const SCN_COMDAT: u32 = 0x1000;

pub const SELECT_NODUPLICATES: u8 = 1;
pub const SELECT_ANY: u8 = 2;
pub const SELECT_ASSOCIATIVE: u8 = 5;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Reloc {
    pub va: u32,
    pub symbol: u32,
    pub typ: u16,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Section {
    pub name: String,
    pub characteristics: u32,
    pub data: Vec<u8>,
    pub uninit: u32,
    pub relocs: Vec<Reloc>,
    pub lines: Vec<(u32, u16)>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Symbol {
    pub name: String,
    pub value: u32,
    pub section: i16,
    pub typ: u16,
    pub class: u8,
    pub aux: Vec<[u8; 18]>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Object {
    pub machine: u16,
    pub timestamp: u32,
    pub characteristics: u16,
    pub sections: Vec<Section>,
    pub symbols: Vec<Symbol>,
}

impl Default for Object {
    fn default() -> Self {
        Self::new()
    }
}

#[must_use]
pub fn section_aux(
    len: u32,
    nrelocs: u16,
    nlines: u16,
    checksum: u32,
    number: u16,
    selection: u8,
) -> [u8; 18] {
    let mut a = [0u8; 18];
    let mut b = Buf::new();
    b.u32(len)
        .u16(nrelocs)
        .u16(nlines)
        .u32(checksum)
        .u16(number)
        .u8(selection);
    for (slot, v) in a.iter_mut().zip(b.as_slice()) {
        *slot = *v;
    }
    a
}

#[must_use]
pub fn aux_with(at: usize, field: &[u8]) -> [u8; 18] {
    let mut a = [0u8; 18];
    if let Some(s) = a.get_mut(at..at.saturating_add(field.len())) {
        s.copy_from_slice(field);
    }
    a
}

impl Object {
    #[must_use]
    pub fn new() -> Self {
        Self {
            machine: MACHINE_I386,
            timestamp: 0x3c00_0000,
            characteristics: 0,
            sections: Vec::new(),
            symbols: Vec::new(),
        }
    }

    pub fn section(&mut self, s: Section) -> i16 {
        self.sections.push(s);
        i16::try_from(self.sections.len()).unwrap_or(i16::MAX)
    }

    pub fn code(&mut self, name: &str, data: &[u8]) -> i16 {
        self.section(Section {
            name: name.to_string(),
            characteristics: SCN_TEXT,
            data: data.to_vec(),
            ..Section::default()
        })
    }

    #[must_use]
    pub fn next_index(&self) -> u32 {
        self.symbols
            .iter()
            .map(|s| 1usize.saturating_add(s.aux.len()))
            .fold(0u32, |a, n| a.saturating_add(n32(n)))
    }

    pub fn symbol(&mut self, s: Symbol) -> u32 {
        let at = self.next_index();
        self.symbols.push(s);
        at
    }

    pub fn plain(&mut self, name: &str, value: u32, section: i16, typ: u16, class: u8) -> u32 {
        self.symbol(Symbol {
            name: name.to_string(),
            value,
            section,
            typ,
            class,
            aux: Vec::new(),
        })
    }

    pub fn external(&mut self, name: &str, value: u32, section: i16) -> u32 {
        self.plain(name, value, section, 0x20, CLASS_EXTERNAL)
    }

    pub fn undefined(&mut self, name: &str) -> u32 {
        self.plain(name, 0, 0, 0x20, CLASS_EXTERNAL)
    }

    pub fn comp_id(&mut self, prodid: u16, build: u16) -> u32 {
        self.plain(
            "@comp.id",
            (u32::from(prodid) << 16) | u32::from(build),
            -1,
            0,
            CLASS_STATIC,
        )
    }

    pub fn section_symbol(&mut self, number: i16, selection: u8, associated: u16) -> u32 {
        let k = usize::try_from(number.saturating_sub(1)).unwrap_or(usize::MAX);
        let (name, len, nrel, nlines) = self.sections.get(k).map_or_else(
            || (String::new(), 0, 0, 0),
            |s| {
                (
                    s.name.clone(),
                    n32(s.data.len()).max(s.uninit),
                    n16(s.relocs.len()),
                    n16(s.lines.len()),
                )
            },
        );
        self.symbol(Symbol {
            name,
            value: 0,
            section: number,
            typ: 0,
            class: CLASS_STATIC,
            aux: vec![section_aux(len, nrel, nlines, 0, associated, selection)],
        })
    }

    #[must_use]
    pub fn build(&self) -> Vec<u8> {
        let mut strings = Buf::new();
        let mut long = |name: &str| -> u32 {
            let at = n32(4usize.saturating_add(strings.len()));
            strings.cstr(name);
            at
        };
        let mut sec_names = Vec::new();
        for s in &self.sections {
            let mut n = [0u8; 8];
            if s.name.len() > 8 {
                let text = format!("/{}", long(&s.name));
                for (slot, c) in n.iter_mut().zip(text.bytes()) {
                    *slot = c;
                }
            } else {
                for (slot, c) in n.iter_mut().zip(s.name.bytes()) {
                    *slot = c;
                }
            }
            sec_names.push(n);
        }
        let mut sym_names = Vec::new();
        for s in &self.symbols {
            let mut n = [0u8; 8];
            if s.name.len() > 8 {
                let at = long(&s.name);
                if let Some(t) = n.get_mut(4..8) {
                    t.copy_from_slice(&at.to_le_bytes());
                }
            } else {
                for (slot, c) in n.iter_mut().zip(s.name.bytes()) {
                    *slot = c;
                }
            }
            sym_names.push(n);
        }
        let headers = 20usize.saturating_add(self.sections.len().saturating_mul(40));
        let mut at = headers;
        let mut places = Vec::new();
        for s in &self.sections {
            let data = if s.data.is_empty() { 0 } else { at };
            at = at.saturating_add(s.data.len());
            let relocs = if s.relocs.is_empty() { 0 } else { at };
            at = at.saturating_add(s.relocs.len().saturating_mul(10));
            let lines = if s.lines.is_empty() { 0 } else { at };
            at = at.saturating_add(s.lines.len().saturating_mul(6));
            places.push((data, relocs, lines));
        }
        let symptr = at;
        let nsyms = self.next_index();
        let mut b = Buf::new();
        b.u16(self.machine)
            .u16(n16(self.sections.len()))
            .u32(self.timestamp)
            .u32(if nsyms == 0 && strings.is_empty() {
                0
            } else {
                n32(symptr)
            })
            .u32(nsyms)
            .u16(0)
            .u16(self.characteristics);
        for ((s, n), (data, relocs, lines)) in self.sections.iter().zip(&sec_names).zip(&places) {
            let size = if s.data.is_empty() {
                s.uninit
            } else {
                n32(s.data.len())
            };
            b.bytes(n)
                .u32(0)
                .u32(0)
                .u32(size)
                .u32(n32(*data))
                .u32(n32(*relocs))
                .u32(n32(*lines))
                .u16(n16(s.relocs.len()))
                .u16(n16(s.lines.len()))
                .u32(s.characteristics);
        }
        for s in &self.sections {
            b.bytes(&s.data);
            for r in &s.relocs {
                b.u32(r.va).u32(r.symbol).u16(r.typ);
            }
            for (addr, line) in &s.lines {
                b.u32(*addr).u16(*line);
            }
        }
        for (s, n) in self.symbols.iter().zip(&sym_names) {
            b.bytes(n)
                .u32(s.value)
                .u16(s.section.cast_unsigned())
                .u16(s.typ)
                .u8(s.class)
                .u8(u8::try_from(s.aux.len()).unwrap_or(u8::MAX));
            for a in &s.aux {
                b.bytes(a);
            }
        }
        b.u32(n32(4usize.saturating_add(strings.len())));
        b.bytes(strings.as_slice());
        b.into_vec()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn long_names_go_to_the_string_table() {
        let mut o = Object::new();
        let t = o.code(".text$mn_long", &[0xc3]);
        o.section_symbol(t, SELECT_ANY, 0);
        o.external("?LongFunctionName@@YAXXZ", 0, t);
        o.comp_id(0x0b, 8168);
        let b = o.build();
        assert_eq!(&b[..2], &MACHINE_I386.to_le_bytes());
        assert_eq!(&b[20..23], b"/4\0");
        assert_eq!(u32::from_le_bytes(b[12..16].try_into().unwrap()), 4);
        let symptr = usize::try_from(u32::from_le_bytes(b[8..12].try_into().unwrap())).unwrap();
        assert_eq!(symptr, 20 + 40 + 1);
        assert_eq!(&b[symptr..symptr + 4], &[0; 4]);
        assert_eq!(&b[symptr + 4..symptr + 8], &18u32.to_le_bytes());
        let strtab = symptr + 18 * 4;
        assert_eq!(&b[strtab + 4..strtab + 18], b".text$mn_long\0");
    }
}
