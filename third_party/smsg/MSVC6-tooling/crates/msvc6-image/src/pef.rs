use crate::be;
use crate::traceback::{self, Table};
use anyhow::{Context as _, Result, bail};
use serde::Serialize;
use std::collections::BTreeMap;

pub const MAGIC: &[u8; 8] = b"Joy!peff";

pub const HEADER: usize = 40;

pub const SECTION_HEADER: usize = 28;

pub const LOADER_HEADER: usize = 56;

pub const LIBRARY_ENTRY: usize = 24;

pub const RELOC_HEADER: usize = 12;

pub const EXPORT_ENTRY: usize = 10;

pub const MAX_SECTION: usize = 1 << 28;

pub const MAX_NAME: usize = 4096;

pub const REPEAT_NESTING: u32 = 4;

pub const STEP_FLOOR: u64 = 1 << 20;

pub const STEPS_PER_WORD: u64 = 16;

pub const RELOC_ALLOWANCE: usize = 1 << 16;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub enum Kind {
    Code,
    UnpackedData,
    PatternData,
    Constant,
    Loader,
    Debug,
    ExecutableData,
    Exception,
    Traceback,
    Other(u8),
}

impl Kind {
    #[must_use]
    pub fn of(b: u8) -> Self {
        match b {
            0 => Self::Code,
            1 => Self::UnpackedData,
            2 => Self::PatternData,
            3 => Self::Constant,
            4 => Self::Loader,
            5 => Self::Debug,
            6 => Self::ExecutableData,
            7 => Self::Exception,
            8 => Self::Traceback,
            n => Self::Other(n),
        }
    }

    #[must_use]
    pub fn label(self) -> String {
        match self {
            Self::Code => "code".into(),
            Self::UnpackedData => "data".into(),
            Self::PatternData => "pidata".into(),
            Self::Constant => "constant".into(),
            Self::Loader => "loader".into(),
            Self::Debug => "debug".into(),
            Self::ExecutableData => "execdata".into(),
            Self::Exception => "exception".into(),
            Self::Traceback => "traceback".into(),
            Self::Other(n) => format!("kind{n}"),
        }
    }

    #[must_use]
    pub fn is_data(self) -> bool {
        matches!(self, Self::UnpackedData | Self::PatternData)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Section {
    pub index: usize,
    pub name_offset: Option<u32>,
    pub default_address: u32,
    pub total: u32,
    pub unpacked: u32,
    pub packed: u32,
    pub offset: u32,
    pub kind: Kind,
    pub share: u8,
    pub alignment: u8,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Header {
    pub architecture: String,
    pub format_version: u32,
    pub timestamp: u32,
    pub old_def_version: u32,
    pub old_imp_version: u32,
    pub current_version: u32,
    pub instantiated: u16,
    pub sections: Vec<Section>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub enum Target {
    Code(u32),
    Data(u32),
    Import(usize),
    Other,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Library {
    pub name: String,
    pub old_imp_version: u32,
    pub current_version: u32,
    pub first: usize,
    pub count: usize,
    pub options: u8,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Import {
    pub name: String,
    pub class: u8,
    pub weak: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Export {
    pub name: String,
    pub class: u8,
    pub section: i16,
    pub value: u32,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Loader {
    pub main: Option<(usize, u32)>,
    pub init: Option<(usize, u32)>,
    pub term: Option<(usize, u32)>,
    pub libraries: Vec<Library>,
    pub imports: Vec<Import>,
    pub exports: Vec<Export>,
}

#[derive(Debug, Clone)]
pub struct Pef {
    pub header: Header,
    pub loader: Loader,
    pub code_section: usize,
    pub data_section: usize,
    pub code: Vec<u8>,
    pub data: Vec<u8>,
    pub toc: u32,
    pub relocs: BTreeMap<u32, Target>,
    pub tables: Vec<Table>,
}

fn word(f: &[u8], at: usize, what: &str) -> Result<u32> {
    be::u32_at(f, at)
        .with_context(|| format!("{what} at {at:#x} runs past the {} byte(s)", f.len()))
}

fn half(f: &[u8], at: usize, what: &str) -> Result<u16> {
    be::u16_at(f, at)
        .with_context(|| format!("{what} at {at:#x} runs past the {} byte(s)", f.len()))
}

fn offset(base: usize, n: usize, size: usize) -> Result<usize> {
    n.checked_mul(size)
        .and_then(|k| base.checked_add(k))
        .context("a table offset overflows")
}

fn us(v: u32) -> usize {
    usize::try_from(v).unwrap_or(usize::MAX)
}

fn ascii(b: &[u8]) -> String {
    msvc6_base::srcio::decode(b)
}

fn entry_point(section: u32, at: u32) -> Option<(usize, u32)> {
    (section != u32::MAX).then(|| (us(section), at))
}

pub fn header(f: &[u8]) -> Result<Header> {
    if f.get(..8) != Some(&MAGIC[..]) {
        bail!("not a PEF container (no `Joy!peff` tag)");
    }
    let arch = f.get(8..12).context("PEF header runs past the file")?;
    let n = usize::from(half(f, 32, "section count")?);
    let room = f.len().saturating_sub(HEADER) / SECTION_HEADER;
    if n > room {
        bail!("{n} section header(s), but the file holds room for {room}");
    }
    let mut sections = Vec::with_capacity(n);
    for i in 0..n {
        let h = offset(HEADER, i, SECTION_HEADER)?;
        let name_off = be::i32_at(f, h).context("section header")?;
        let kind = *f.get(h.saturating_add(24)).context("section header")?;
        sections.push(Section {
            index: i,
            name_offset: u32::try_from(name_off).ok(),
            default_address: word(f, h + 4, "section header")?,
            total: word(f, h + 8, "section header")?,
            unpacked: word(f, h + 12, "section header")?,
            packed: word(f, h + 16, "section header")?,
            offset: word(f, h + 20, "section header")?,
            kind: Kind::of(kind),
            share: f.get(h + 25).copied().unwrap_or(0),
            alignment: f.get(h + 26).copied().unwrap_or(0),
        });
    }
    Ok(Header {
        architecture: ascii(arch),
        format_version: word(f, 12, "PEF header")?,
        timestamp: word(f, 16, "PEF header")?,
        old_def_version: word(f, 20, "PEF header")?,
        old_imp_version: word(f, 24, "PEF header")?,
        current_version: word(f, 28, "PEF header")?,
        instantiated: half(f, 34, "PEF header")?,
        sections,
    })
}

pub fn raw<'a>(f: &'a [u8], s: &Section) -> Result<&'a [u8]> {
    let at = us(s.offset);
    let len = us(s.packed);
    f.get(at..at.checked_add(len).context("section extent overflows")?)
        .with_context(|| {
            format!(
                "section {} ({}): {len} byte(s) at {at:#x} run past the {}-byte file",
                s.index,
                s.kind.label(),
                f.len()
            )
        })
}

fn varint(b: &[u8], p: &mut usize) -> Result<usize> {
    let mut v = 0usize;
    for _ in 0..5 {
        let x = *b.get(*p).context("a pattern count runs past the section")?;
        *p = p.saturating_add(1);
        v = v.checked_shl(7).context("a pattern count overflows")? | usize::from(x & 0x7f);
        if x & 0x80 == 0 {
            return Ok(v);
        }
    }
    bail!("a pattern count longer than five bytes")
}

struct Out {
    bytes: Vec<u8>,
    cap: usize,
}

impl Out {
    fn room(&self, n: usize) -> Result<()> {
        if self.bytes.len().saturating_add(n) > self.cap {
            bail!(
                "pattern-initialised data unpacks past the {} byte(s) its section declares",
                self.cap
            );
        }
        Ok(())
    }

    fn zeros(&mut self, n: usize) -> Result<()> {
        self.room(n)?;
        self.bytes.resize(self.bytes.len().saturating_add(n), 0);
        Ok(())
    }

    fn put(&mut self, b: &[u8]) -> Result<()> {
        self.room(b.len())?;
        self.bytes.extend_from_slice(b);
        Ok(())
    }
}

fn take<'a>(raw: &'a [u8], p: &mut usize, n: usize) -> Result<&'a [u8]> {
    let s = raw
        .get(*p..p.checked_add(n).context("a pattern run overflows")?)
        .context("a pattern run reads past the section")?;
    *p = p.saturating_add(n);
    Ok(s)
}

pub fn unpack_pidata(raw: &[u8], cap: usize) -> Result<Vec<u8>> {
    let mut o = Out {
        bytes: Vec::new(),
        cap: cap.min(MAX_SECTION),
    };
    let mut p = 0usize;
    while let Some(&b) = raw.get(p) {
        p = p.saturating_add(1);
        let op = b >> 5;
        let mut count = usize::from(b & 0x1f);
        if count == 0 {
            count = varint(raw, &mut p)?;
        }
        match op {
            0 => o.zeros(count)?,
            1 => {
                let s = take(raw, &mut p, count)?;
                o.put(s)?;
            }
            2 => {
                let repeat = varint(raw, &mut p)?.saturating_add(1);
                let block = take(raw, &mut p, count)?;
                if block.is_empty() {
                    continue;
                }
                o.room(block.len().saturating_mul(repeat))?;
                for _ in 0..repeat {
                    o.put(block)?;
                }
            }
            3 => {
                let custom = varint(raw, &mut p)?;
                let repeat = varint(raw, &mut p)?;
                let common = take(raw, &mut p, count)?;
                for _ in 0..repeat {
                    o.put(common)?;
                    let c = take(raw, &mut p, custom)?;
                    o.put(c)?;
                }
                o.put(common)?;
            }
            4 => {
                let custom = varint(raw, &mut p)?;
                let repeat = varint(raw, &mut p)?;
                for _ in 0..repeat {
                    o.zeros(count)?;
                    let c = take(raw, &mut p, custom)?;
                    o.put(c)?;
                }
                o.zeros(count)?;
            }
            other => bail!("pattern opcode {other} at byte {}", p.saturating_sub(1)),
        }
    }
    Ok(o.bytes)
}

struct Relocator<'a> {
    instrs: &'a [u16],
    data: &'a [u8],
    code_index: usize,
    data_index: usize,
    out: BTreeMap<u32, Target>,
    addr: u32,
    import_index: usize,
    sect_c: usize,
    sect_d: usize,
    steps: u64,
    budget: u64,
    cap: usize,
}

impl Relocator<'_> {
    fn tick(&mut self) -> Result<()> {
        self.steps = self.steps.saturating_add(1);
        if self.steps > self.budget {
            bail!(
                "the relocation instructions run past their budget of {} steps",
                self.budget
            );
        }
        Ok(())
    }

    fn put(&mut self, t: Target) -> Result<()> {
        if self.out.len() >= self.cap && !self.out.contains_key(&self.addr) {
            bail!(
                "the relocation instructions relocate more than {} words, a word for every 4 bytes of the file \
                 and {RELOC_ALLOWANCE} more",
                self.cap
            );
        }
        self.out.insert(self.addr, t);
        Ok(())
    }

    fn inside(&self) -> Result<()> {
        if us(self.addr).saturating_add(4) > self.data.len() {
            bail!(
                "a relocation of the word at {:#x}, past the {}-byte data section",
                self.addr,
                self.data.len()
            );
        }
        Ok(())
    }

    fn by_section(&mut self, sect: usize) -> Result<()> {
        self.tick()?;
        self.inside()?;
        let v = be::u32_at(self.data, us(self.addr)).unwrap_or(0);
        let t = if sect == self.code_index {
            Target::Code(v)
        } else if sect == self.data_index {
            Target::Data(v)
        } else {
            Target::Other
        };
        self.put(t)?;
        self.addr = self.addr.wrapping_add(4);
        Ok(())
    }

    fn by_import(&mut self, index: usize) -> Result<()> {
        self.tick()?;
        self.inside()?;
        self.put(Target::Import(index))?;
        self.addr = self.addr.wrapping_add(4);
        Ok(())
    }

    fn group(&mut self, sub: u32, run: u32) -> Result<()> {
        for _ in 0..run {
            match sub {
                0 => self.by_section(self.sect_c)?,
                1 => self.by_section(self.sect_d)?,
                2 => {
                    self.by_section(self.sect_c)?;
                    self.by_section(self.sect_d)?;
                    self.addr = self.addr.wrapping_add(4);
                }
                3 => {
                    self.by_section(self.sect_c)?;
                    self.by_section(self.sect_d)?;
                }
                4 => {
                    self.by_section(self.sect_d)?;
                    self.addr = self.addr.wrapping_add(4);
                }
                5 => {
                    self.by_import(self.import_index)?;
                    self.import_index = self.import_index.saturating_add(1);
                }
                other => bail!("relocation group subopcode {other}"),
            }
        }
        Ok(())
    }

    fn repeat(&mut self, pc: usize, blocks: usize, times: u64, depth: u32) -> Result<()> {
        if depth >= REPEAT_NESTING {
            bail!("relocation repeats nest deeper than {REPEAT_NESTING}");
        }
        let from = pc
            .checked_sub(blocks)
            .context("a relocation repeat reaches before the first instruction")?;
        for _ in 0..times {
            self.tick()?;
            self.run(from, pc, depth.saturating_add(1))?;
        }
        Ok(())
    }

    fn run(&mut self, from: usize, to: usize, depth: u32) -> Result<()> {
        let mut pc = from;
        while pc < to {
            self.tick()?;
            let h = *self
                .instrs
                .get(pc)
                .context("a relocation instruction past the table")?;
            let next = self
                .instrs
                .get(pc.saturating_add(1))
                .copied()
                .map_or(0, u32::from);
            let w = u32::from(h);
            if h >> 14 == 0 {
                self.addr = self.addr.wrapping_add(((w >> 6) & 0xff).wrapping_mul(4));
                for _ in 0..(w & 0x3f) {
                    self.by_section(self.sect_d)?;
                }
                pc = pc.saturating_add(1);
            } else if h >> 13 == 0b010 {
                self.group((w >> 9) & 0xf, (w & 0x1ff).saturating_add(1))?;
                pc = pc.saturating_add(1);
            } else if h >> 13 == 0b011 {
                let index = us(w & 0x1ff);
                match (w >> 9) & 0xf {
                    0 => {
                        self.by_import(index)?;
                        self.import_index = index.saturating_add(1);
                    }
                    1 => self.sect_c = index,
                    2 => self.sect_d = index,
                    3 => self.by_section(index)?,
                    other => bail!("relocation index subopcode {other}"),
                }
                pc = pc.saturating_add(1);
            } else if h >> 12 == 0b1000 {
                self.addr = self.addr.wrapping_add((w & 0xfff).saturating_add(1));
                pc = pc.saturating_add(1);
            } else if h >> 12 == 0b1001 {
                let blocks = us(((w >> 8) & 0xf).saturating_add(1));
                let times = u64::from((w & 0xff).saturating_add(1));
                self.repeat(pc, blocks, times, depth)?;
                pc = pc.saturating_add(1);
            } else if h >> 10 == 0b10_1000 {
                self.addr = ((w & 0x3ff) << 16) | next;
                pc = pc.saturating_add(2);
            } else if h >> 10 == 0b10_1001 {
                let index = us(((w & 0x3ff) << 16) | next);
                self.by_import(index)?;
                self.import_index = index.saturating_add(1);
                pc = pc.saturating_add(2);
            } else if h >> 10 == 0b10_1100 {
                let blocks = us(((w >> 6) & 0xf).saturating_add(1));
                let times = u64::from(((w & 0x3f) << 16) | next);
                self.repeat(pc, blocks, times, depth)?;
                pc = pc.saturating_add(2);
            } else if h >> 10 == 0b10_1101 {
                let index = us(((w & 0x3f) << 16) | next);
                match (w >> 6) & 0xf {
                    0 => self.by_section(index)?,
                    1 => self.sect_c = index,
                    2 => self.sect_d = index,
                    other => bail!("relocation set-or-by-section subopcode {other}"),
                }
                pc = pc.saturating_add(2);
            } else {
                bail!("relocation opcode {h:#06x} is not defined");
            }
        }
        Ok(())
    }
}

fn cname(l: &[u8], strs: usize, off: u32) -> String {
    us(off)
        .checked_add(strs)
        .and_then(|at| be::cstr(l, at, MAX_NAME))
        .map(ascii)
        .unwrap_or_default()
}

fn exports(l: &[u8], strs: usize, hash: usize, power: u32, n: usize) -> Result<Vec<Export>> {
    if power > 24 {
        bail!("export hash table power {power}");
    }
    let keys = offset(hash, 1usize << power, 4)?;
    let syms = offset(keys, n, 4)?;
    if offset(syms, n, EXPORT_ENTRY)? > l.len() {
        bail!("{n} export(s) run past the {}-byte loader section", l.len());
    }
    let mut out = Vec::with_capacity(n);
    for k in 0..n {
        let key = word(l, offset(keys, k, 4)?, "export key")?;
        let e = offset(syms, k, EXPORT_ENTRY)?;
        let cn = word(l, e, "export symbol")?;
        let len = us(key >> 16);
        let at = strs
            .checked_add(us(cn & 0x00ff_ffff))
            .context("an export name offset overflows")?;
        let name = l
            .get(at..at.saturating_add(len))
            .map(ascii)
            .unwrap_or_default();
        out.push(Export {
            name,
            class: u8::try_from(cn >> 24).unwrap_or(0),
            section: be::i16_at(l, e.saturating_add(8)).context("export symbol")?,
            value: word(l, e.saturating_add(4), "export symbol")?,
        });
    }
    Ok(out)
}

struct LoaderTables {
    loader: Loader,
    rel_instr: usize,
    rel_headers: Vec<(usize, usize, usize)>,
}

fn loader(l: &[u8]) -> Result<LoaderTables> {
    let g = |o: usize| word(l, o, "loader header");
    let nlib = us(g(24)?);
    let nimp = us(g(28)?);
    let nrel = us(g(32)?);
    let rel_instr = us(g(36)?);
    let strs = us(g(40)?);
    let lib_tab = LOADER_HEADER;
    let imp_tab = offset(lib_tab, nlib, LIBRARY_ENTRY)?;
    let rel_tab = offset(imp_tab, nimp, 4)?;
    if offset(rel_tab, nrel, RELOC_HEADER)? > l.len() {
        bail!(
            "{nlib} librar(ies), {nimp} import(s) and {nrel} relocation header(s) run past the {}-byte loader section",
            l.len()
        );
    }
    let mut libraries = Vec::with_capacity(nlib);
    for k in 0..nlib {
        let e = offset(lib_tab, k, LIBRARY_ENTRY)?;
        libraries.push(Library {
            name: cname(l, strs, g(e)?),
            old_imp_version: g(e + 4)?,
            current_version: g(e + 8)?,
            count: us(g(e + 12)?),
            first: us(g(e + 16)?),
            options: l.get(e + 20).copied().unwrap_or(0),
        });
    }
    let mut imports = Vec::with_capacity(nimp);
    for j in 0..nimp {
        let w = g(offset(imp_tab, j, 4)?)?;
        let class = u8::try_from(w >> 24).unwrap_or(0);
        imports.push(Import {
            name: cname(l, strs, w & 0x00ff_ffff),
            class: class & 0x0f,
            weak: class & 0x80 != 0,
        });
    }
    let mut rel_headers = Vec::with_capacity(nrel);
    for k in 0..nrel {
        let h = offset(rel_tab, k, RELOC_HEADER)?;
        rel_headers.push((
            usize::from(half(l, h, "relocation header")?),
            us(g(h + 4)?),
            us(g(h + 8)?),
        ));
    }
    let nexp = us(g(52)?);
    let exports = exports(l, strs, us(g(44)?), g(48)?, nexp)?;
    Ok(LoaderTables {
        loader: Loader {
            main: entry_point(g(0)?, g(4)?),
            init: entry_point(g(8)?, g(12)?),
            term: entry_point(g(16)?, g(20)?),
            libraries,
            imports,
            exports,
        },
        rel_instr,
        rel_headers,
    })
}

fn data_of(f: &[u8], s: &Section) -> Result<Vec<u8>> {
    let packed = raw(f, s)?;
    let total = us(s.total).max(us(s.unpacked));
    if total > MAX_SECTION {
        bail!(
            "section {}: {total} byte(s), more than the {MAX_SECTION} this reader unpacks",
            s.index
        );
    }
    let mut d = if s.kind == Kind::PatternData {
        unpack_pidata(packed, total).with_context(|| format!("section {} (pidata)", s.index))?
    } else {
        packed
            .get(..packed.len().min(total))
            .unwrap_or(packed)
            .to_vec()
    };
    if d.len() < total {
        d.resize(total, 0);
    }
    Ok(d)
}

pub fn load(f: &[u8]) -> Result<Pef> {
    let header = header(f)?;
    let code_s = header
        .sections
        .iter()
        .find(|s| s.kind == Kind::Code)
        .context("PEF without a code section")?;
    let data_s = header
        .sections
        .iter()
        .find(|s| s.kind.is_data())
        .context("PEF without a data section")?;
    let mut loaders = header.sections.iter().filter(|s| s.kind == Kind::Loader);
    let loader_s = loaders.next().context("PEF without a loader section")?;
    if loaders.next().is_some() {
        bail!("PEF with two loader sections");
    }
    let code = raw(f, code_s)?.to_vec();
    let data = data_of(f, data_s)?;
    let l = raw(f, loader_s)?;
    let lt = loader(l)?;
    let (code_index, data_index) = (code_s.index, data_s.index);
    let mut relocs = BTreeMap::new();
    let words = u64::try_from(data.len() / 4).unwrap_or(u64::MAX);
    for (sect, count, first) in &lt.rel_headers {
        if *sect != data_index {
            continue;
        }
        let base = lt
            .rel_instr
            .checked_add(*first)
            .context("relocation instructions' offset overflows")?;
        let end = offset(base, *count, 2)?;
        let table = l.get(base..end).with_context(|| {
            format!(
                "{count} relocation instruction(s) at {base:#x} run past the {}-byte loader section",
                l.len()
            )
        })?;
        let instrs: Vec<u16> = table
            .as_chunks::<2>()
            .0
            .iter()
            .map(|c| u16::from_be_bytes(*c))
            .collect();

        let n = u64::try_from(instrs.len()).unwrap_or(u64::MAX);
        let mut r = Relocator {
            instrs: &instrs,
            data: &data,
            code_index,
            data_index,
            out: BTreeMap::new(),
            addr: 0,
            import_index: 0,
            sect_c: 0,
            sect_d: 1,
            steps: 0,
            budget: STEPS_PER_WORD
                .saturating_mul(words.saturating_add(n))
                .max(STEP_FLOOR),
            cap: (f.len() / 4).saturating_add(RELOC_ALLOWANCE),
        };
        r.run(0, instrs.len(), 0)
            .with_context(|| format!("relocations of section {sect}"))?;
        relocs.extend(r.out);
    }
    let (main_sec, main_off) = lt
        .loader
        .main
        .context("PEF without a main symbol: no TOC to read code through")?;
    if main_sec != data_index {
        bail!("the PEF main symbol is not a data-section transition vector");
    }
    let tv = main_off
        .checked_add(4)
        .context("main symbol offset overflows")?;
    let toc = match relocs.get(&tv) {
        Some(Target::Data(t)) => *t,
        _ => be::u32_at(&data, us(tv)).unwrap_or(0),
    };
    let tables = traceback::scan(&code);
    Ok(Pef {
        header,
        loader: lt.loader,
        code_section: code_index,
        data_section: data_index,
        code,
        data,
        toc,
        relocs,
        tables,
    })
}

impl Pef {
    #[must_use]
    pub fn target(&self, off: u32) -> Option<Target> {
        self.relocs.get(&off).copied()
    }

    #[must_use]
    pub fn code_word(&self, off: u32) -> Option<u32> {
        be::u32_at(&self.code, us(off))
    }

    #[must_use]
    pub fn data_bytes(&self, off: u32, n: usize) -> Option<&[u8]> {
        let at = us(off);
        self.data.get(at..at.checked_add(n)?)
    }

    #[must_use]
    pub fn import_name(&self, i: usize) -> Option<&str> {
        self.loader.imports.get(i).map(|m| m.name.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pattern_data_unpacks_each_opcode_and_stops_at_its_size() {
        let raw = [0x03, 0x22, b'a', b'b', 0x41, 0x02, b'x'];
        assert_eq!(unpack_pidata(&raw, 64).unwrap(), b"\0\0\0abxxx".to_vec());
        let interleave = [0x61, 0x01, 0x02, b'-', b'a', b'b'];
        assert_eq!(unpack_pidata(&interleave, 64).unwrap(), b"-a-b-".to_vec());
        let zeros = [0x81, 0x01, 0x02, b'a', b'b'];
        assert_eq!(unpack_pidata(&zeros, 64).unwrap(), b"\0a\0b\0".to_vec());
        assert!(unpack_pidata(&raw, 7).is_err());
        let huge = [0x41, 0xff, 0xff, 0xff, 0x7f, b'x'];
        assert!(unpack_pidata(&huge, 1 << 20).is_err());
        assert!(unpack_pidata(&[0xe1], 64).is_err());
        assert!(unpack_pidata(&[0x23, b'a'], 64).is_err());
    }

    fn relocate(instrs: &[u16], data: &[u8]) -> Result<BTreeMap<u32, Target>> {
        let mut r = Relocator {
            instrs,
            data,
            code_index: 0,
            data_index: 1,
            out: BTreeMap::new(),
            addr: 0,
            import_index: 0,
            sect_c: 0,
            sect_d: 1,
            steps: 0,
            budget: STEP_FLOOR,
            cap: data.len() / 4 + RELOC_ALLOWANCE,
        };
        r.run(0, instrs.len(), 0)?;
        Ok(r.out)
    }

    #[test]
    fn relocations_by_section_import_and_repeat() {
        let data = [0, 0, 0, 0x10, 0, 0, 0, 0x20, 0, 0, 0, 0, 0, 0, 0, 0];
        let out = relocate(&[0x4600, 0x6007], &data).unwrap();
        assert_eq!(out.get(&0), Some(&Target::Code(0x10)));
        assert_eq!(out.get(&4), Some(&Target::Data(0x20)));
        assert_eq!(out.get(&8), Some(&Target::Import(7)));
        let rep = relocate(&[0x4200, 0x9001], &data).unwrap();
        assert_eq!(rep.len(), 3);
        assert!(relocate(&[0x9000], &data).is_err());
        assert!(relocate(&[0xc000], &data).is_err());
        let flood = relocate(&[0x4200, 0xb03f, 0xffff], &data);
        assert!(flood.is_err());
    }
}
