use crate::buf::{n16, n32, us};

pub const BLR: u32 = 0x4e80_0020;

pub const LANG_CPLUSPLUS: u8 = 9;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Traceback {
    pub name: String,
    pub tb_offset: Option<u32>,
    pub fixed_parms: u8,
    pub lang: u8,
}

impl Traceback {
    #[must_use]
    pub fn named(name: &str, tb_offset: Option<u32>) -> Self {
        Self {
            name: name.to_string(),
            tb_offset,
            fixed_parms: 0,
            lang: LANG_CPLUSPLUS,
        }
    }

    #[must_use]
    pub fn bytes(&self) -> Vec<u8> {
        let mut t = vec![0, 0, 0, 0, 0, self.lang];
        t.push(if self.tb_offset.is_some() { 0x20 } else { 0 });
        t.push(0x40);
        t.extend_from_slice(&[0, 0, self.fixed_parms, 0]);
        if self.fixed_parms != 0 {
            t.extend_from_slice(&[0; 4]);
        }
        if let Some(off) = self.tb_offset {
            t.extend_from_slice(&off.to_be_bytes());
        }
        t.extend_from_slice(&n16(self.name.len()).to_be_bytes());
        t.extend_from_slice(self.name.as_bytes());
        while !t.len().is_multiple_of(4) {
            t.push(0);
        }
        t
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Function {
    pub words: Vec<u32>,
    pub name: String,
}

#[must_use]
pub fn code_section(functions: &[Function]) -> (Vec<u8>, Vec<(u32, u32)>) {
    let mut code = vec![0u8; 4];
    let mut spans = Vec::new();
    for f in functions {
        let start = n32(code.len());
        for w in &f.words {
            code.extend_from_slice(&w.to_be_bytes());
        }
        let at = n32(code.len());
        code.extend(Traceback::named(&f.name, Some(at.saturating_sub(start))).bytes());
        spans.push((start, at));
    }
    (code, spans)
}

#[must_use]
pub fn pidata(raw: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    let emit = |op: u8, count: usize, out: &mut Vec<u8>| {
        if count > 0 && count < 32 {
            out.push((op << 5) | u8::try_from(count).unwrap_or(0));
        } else {
            out.push(op << 5);
            let mut groups = Vec::new();
            let mut v = count;
            loop {
                groups.push(u8::try_from(v & 0x7f).unwrap_or(0));
                v >>= 7;
                if v == 0 {
                    break;
                }
            }
            let last = groups.len().saturating_sub(1);
            for (k, g) in groups.iter().rev().enumerate() {
                out.push(if k < last { g | 0x80 } else { *g });
            }
        }
    };
    let mut i = 0usize;
    while i < raw.len() {
        let zeros = raw
            .get(i..)
            .unwrap_or_default()
            .iter()
            .take_while(|b| **b == 0)
            .count();
        if zeros >= 4 {
            emit(0, zeros, &mut out);
            i = i.saturating_add(zeros);
            continue;
        }
        let mut j = i;
        while j < raw.len()
            && raw
                .get(j..)
                .unwrap_or_default()
                .iter()
                .take_while(|b| **b == 0)
                .count()
                < 4
        {
            j = j.saturating_add(1);
        }
        emit(1, j.saturating_sub(i), &mut out);
        out.extend_from_slice(raw.get(i..j).unwrap_or_default());
        i = j;
    }
    out
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Library {
    pub name: String,
    pub imports: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Pef {
    pub functions: Vec<Function>,
    pub data: Vec<u8>,
    pub pattern: bool,
    pub libraries: Vec<Library>,
    pub relocations: Vec<u16>,
    pub main: u32,
}

fn be32(v: &mut Vec<u8>, x: u32) {
    v.extend_from_slice(&x.to_be_bytes());
}

fn be16(v: &mut Vec<u8>, x: u16) {
    v.extend_from_slice(&x.to_be_bytes());
}

impl Pef {
    #[must_use]
    pub fn loader(&self) -> Vec<u8> {
        let nlib = self.libraries.len();
        let nimp: usize = self.libraries.iter().map(|l| l.imports.len()).sum();
        let mut strings: Vec<u8> = Vec::new();
        let mut lib_names = Vec::new();
        let mut imp_names = Vec::new();
        for l in &self.libraries {
            lib_names.push(n32(strings.len()));
            strings.extend_from_slice(l.name.as_bytes());
            strings.push(0);
            for i in &l.imports {
                imp_names.push(n32(strings.len()));
                strings.extend_from_slice(i.as_bytes());
                strings.push(0);
            }
        }
        while !strings.len().is_multiple_of(4) {
            strings.push(0);
        }
        let header = 56usize;
        let reloc_hdr = header + nlib * 24 + nimp * 4;
        let rel_instr = reloc_hdr + 12;
        let strs = rel_instr + self.relocations.len() * 2 + (self.relocations.len() % 2) * 2;
        let hash = strs + strings.len();
        let mut l = Vec::new();
        for v in [1u32, self.main, u32::MAX, 0, u32::MAX, 0] {
            be32(&mut l, v);
        }
        for v in [
            n32(nlib),
            n32(nimp),
            1,
            n32(rel_instr),
            n32(strs),
            n32(hash),
            0,
            0,
        ] {
            be32(&mut l, v);
        }
        let mut first = 0usize;
        for (l_i, lib) in self.libraries.iter().enumerate() {
            be32(&mut l, lib_names.get(l_i).copied().unwrap_or(0));
            be32(&mut l, 0);
            be32(&mut l, 0);
            be32(&mut l, n32(lib.imports.len()));
            be32(&mut l, n32(first));
            be32(&mut l, 0);
            first = first.saturating_add(lib.imports.len());
        }
        for (k, off) in imp_names.iter().enumerate() {
            let class = if k == 0 { 0x02u32 } else { 0x00 };
            be32(&mut l, (class << 24) | off);
        }
        be16(&mut l, 1);
        be16(&mut l, 0);
        be32(&mut l, n32(self.relocations.len()));
        be32(&mut l, 0);
        for r in &self.relocations {
            be16(&mut l, *r);
        }
        while l.len() < strs {
            l.push(0);
        }
        l.extend_from_slice(&strings);
        be32(&mut l, 0);
        l
    }

    #[must_use]
    pub fn build(&self) -> Vec<u8> {
        let (code, _) = code_section(&self.functions);
        let packed = if self.pattern {
            pidata(&self.data)
        } else {
            self.data.clone()
        };
        let loader = self.loader();
        let nsec = 3usize;
        let mut at = 40 + nsec * 28;
        let mut place = |len: usize| {
            let a = at;
            at = at.saturating_add(len).next_multiple_of(16);
            a
        };
        let (code_at, data_at, loader_at) =
            (place(code.len()), place(packed.len()), place(loader.len()));
        let mut f = Vec::new();
        f.extend_from_slice(b"Joy!peffpwpc");
        for v in [1u32, 0x1234_5678, 0, 0, 0] {
            be32(&mut f, v);
        }
        be16(&mut f, n16(nsec));
        be16(&mut f, 2);
        be32(&mut f, 0);
        let kind_data = if self.pattern { 2u8 } else { 1 };
        for (len, total, packed_len, off, kind) in [
            (code.len(), code.len(), code.len(), code_at, 0u8),
            (
                self.data.len(),
                self.data.len(),
                packed.len(),
                data_at,
                kind_data,
            ),
            (0, 0, loader.len(), loader_at, 4),
        ] {
            be32(&mut f, u32::MAX);
            be32(&mut f, 0);
            be32(&mut f, n32(total));
            be32(&mut f, n32(len));
            be32(&mut f, n32(packed_len));
            be32(&mut f, n32(off));
            f.extend_from_slice(&[kind, 1, 4, 0]);
        }
        for (off, body) in [(code_at, &code), (data_at, &packed), (loader_at, &loader)] {
            if f.len() < off {
                f.resize(off, 0);
            }
            f.extend_from_slice(body);
        }
        f
    }
}

pub const CPU_I386: u32 = 7;

pub const CPU_PPC: u32 = 18;

pub const ATTR_PURE_INSTRUCTIONS: u32 = 0x8000_0000;

pub const ATTR_SOME_INSTRUCTIONS: u32 = 0x400;

pub const SYMBOL_STUBS: u32 = 0x8;

pub const NON_LAZY_POINTERS: u32 = 0x6;

pub const N_SECT: u8 = 0x0e;

pub const N_EXT: u8 = 0x01;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct MachSection {
    pub segment: String,
    pub name: String,
    pub addr: u32,
    pub data: Vec<u8>,
    pub flags: u32,
    pub reserved1: u32,
    pub reserved2: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct MachSymbol {
    pub name: String,
    pub ty: u8,
    pub sect: u8,
    pub value: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct MachO {
    pub cpu: u32,
    pub big_endian: bool,
    pub sections: Vec<MachSection>,
    pub symbols: Vec<MachSymbol>,
    pub indirect: Vec<u32>,
}

fn name16(v: &mut Vec<u8>, s: &str) {
    let mut b = s.as_bytes().to_vec();
    b.resize(16, 0);
    v.extend_from_slice(b.get(..16).unwrap_or_default());
}

impl MachO {
    fn w32(&self, v: &mut Vec<u8>, x: u32) {
        if self.big_endian {
            v.extend_from_slice(&x.to_be_bytes());
        } else {
            v.extend_from_slice(&x.to_le_bytes());
        }
    }

    #[must_use]
    pub fn build(&self) -> Vec<u8> {
        let nsect = self.sections.len();
        let seg = 56 + nsect * 68;
        let cmds = seg + 24 + 80;
        let mut strings: Vec<u8> = vec![0];
        let mut strx = Vec::new();
        for s in &self.symbols {
            strx.push(n32(strings.len()));
            strings.extend_from_slice(s.name.as_bytes());
            strings.push(0);
        }
        let mut at = 28 + cmds;
        let mut data_at = Vec::new();
        for s in &self.sections {
            at = at.next_multiple_of(16);
            data_at.push(at);
            at = at.saturating_add(s.data.len());
        }
        let symoff = at.next_multiple_of(4);
        let indoff = symoff + self.symbols.len() * 12;
        let stroff = indoff + self.indirect.len() * 4;
        let mut v = Vec::new();
        for x in [MAGIC_THIN, self.cpu, 3, 2, 3, n32(cmds), 0] {
            self.w32(&mut v, x);
        }
        self.w32(&mut v, 1);
        self.w32(&mut v, n32(seg));
        name16(&mut v, "");
        for _ in 0..6 {
            self.w32(&mut v, 0);
        }
        self.w32(&mut v, n32(nsect));
        self.w32(&mut v, 0);
        for (s, off) in self.sections.iter().zip(&data_at) {
            name16(&mut v, &s.name);
            name16(&mut v, &s.segment);
            for x in [
                s.addr,
                n32(s.data.len()),
                n32(*off),
                2,
                0,
                0,
                s.flags,
                s.reserved1,
                s.reserved2,
            ] {
                self.w32(&mut v, x);
            }
        }
        for x in [
            2u32,
            24,
            n32(symoff),
            n32(self.symbols.len()),
            n32(stroff),
            n32(strings.len()),
        ] {
            self.w32(&mut v, x);
        }
        self.w32(&mut v, 0xb);
        self.w32(&mut v, 80);
        for _ in 0..12 {
            self.w32(&mut v, 0);
        }
        self.w32(&mut v, n32(indoff));
        self.w32(&mut v, n32(self.indirect.len()));
        for _ in 0..4 {
            self.w32(&mut v, 0);
        }
        for (s, off) in self.sections.iter().zip(&data_at) {
            v.resize(*off, 0);
            v.extend_from_slice(&s.data);
        }
        v.resize(symoff, 0);
        for (s, x) in self.symbols.iter().zip(&strx) {
            self.w32(&mut v, *x);
            v.push(s.ty);
            v.push(s.sect);
            v.extend_from_slice(&[0, 0]);
            self.w32(&mut v, s.value);
        }
        for i in &self.indirect {
            self.w32(&mut v, *i);
        }
        v.extend_from_slice(&strings);
        v
    }
}

pub const MAGIC_THIN: u32 = 0xfeed_face;

#[must_use]
pub fn fat(slices: &[(u32, Vec<u8>)]) -> Vec<u8> {
    let mut v = Vec::new();
    be32(&mut v, 0xcafe_babe);
    be32(&mut v, n32(slices.len()));
    let mut at = (8 + slices.len() * 20).next_multiple_of(4096);
    let mut offs = Vec::new();
    for (cpu, b) in slices {
        be32(&mut v, *cpu);
        be32(&mut v, 3);
        be32(&mut v, n32(at));
        be32(&mut v, n32(b.len()));
        be32(&mut v, 12);
        offs.push(at);
        at = at.saturating_add(b.len()).next_multiple_of(4096);
    }
    for ((_, b), off) in slices.iter().zip(offs) {
        v.resize(off, 0);
        v.extend_from_slice(b);
    }
    v
}

#[must_use]
pub fn words(ws: &[u32]) -> Vec<u8> {
    ws.iter().flat_map(|w| w.to_be_bytes()).collect()
}

#[must_use]
pub fn word_at(code: &[u8], at: u32) -> Option<u32> {
    let a = us(at);
    let s = code.get(a..a.checked_add(4)?)?;
    Some(u32::from_be_bytes(<[u8; 4]>::try_from(s).ok()?))
}
