use crate::cursor::{Cursor, Result as IlResult, error};
use crate::ex::{self, Rec};
use crate::grammar::Grammar;
use crate::header::{self, Header};
use crate::ops;
use crate::sym::{self, Sym};
use crate::tree::Names;
use anyhow::{Context as _, Result};
use std::collections::{BTreeMap, HashMap};
use std::ffi::OsString;
use std::path::{Path, PathBuf};

pub const MAX_STREAM: u64 = 1 << 30;
pub const FFLAG_PCH: u32 = 0x1;
pub const FFLAG_DEFINED: u32 = 0x8;
pub const FFLAG_COMPILE: u32 = 0x20;
pub const FFLAG_CANDIDATE: u32 = 0x40;
pub const FFLAG_INLINE: u32 = 0x80;
pub const FFLAG_DTOR_OBJECTS: u32 = 0x100;
pub const FFLAG_TRY: u32 = 0x200;

#[derive(Debug, Clone, Default)]
pub struct Files {
    pub base: String,
    pub gl: Vec<u8>,
    pub sy: Vec<u8>,
    pub ex: Vec<u8>,
    pub init: Vec<u8>,
    pub pch: Option<Vec<u8>>,
    pub db: bool,
}

#[must_use]
pub fn member(base: &Path, ext: &str) -> PathBuf {
    let mut s: OsString = base.as_os_str().to_os_string();
    s.push(ext);
    PathBuf::from(s)
}

impl Files {
    pub fn open(base: &Path, pch: Option<&Path>) -> Result<Self> {
        let need = |ext: &str| crate::file::read(&member(base, ext), MAX_STREAM);
        Ok(Self {
            base: base.display().to_string(),
            gl: need("gl")?,
            sy: need("sy")?,
            ex: need("ex")?,
            init: crate::file::read_if(&member(base, "in"), MAX_STREAM)?.unwrap_or_default(),
            pch: pch.map(|p| crate::file::read(p, MAX_STREAM)).transpose()?,
            db: member(base, "db").exists(),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SymInfo {
    pub class: u8,
    pub name: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Stream {
    Ex,
    Pch,
}

#[derive(Debug, Clone)]
pub struct Unit {
    pub header: Header,
    pub globals: Vec<Sym>,
    pub gl_tail: usize,
    pub lists: BTreeMap<usize, Vec<Sym>>,
    pub syms: HashMap<u32, SymInfo>,
}

pub fn read_list(c: &mut Cursor<'_>, wide: bool) -> IlResult<Vec<Sym>> {
    let mut v = Vec::new();
    loop {
        let s = sym::read(c, wide)?;
        let end = s.ends_list;
        v.push(s);
        if end || c.at_end() {
            return Ok(v);
        }
    }
}

impl Unit {
    pub fn read(files: &Files) -> Result<Self> {
        let mut c = Cursor::new(&files.gl, 0);
        let h = header::read(&mut c).with_context(|| format!("{}gl", files.base))?;
        let mut c = c.with_ids(h.ids);
        let mut globals = Vec::new();
        while !c.at_end() {
            let s = sym::read(&mut c, h.wide).with_context(|| format!("{}gl", files.base))?;
            let end = s.ends_list;
            globals.push(s);
            if end {
                break;
            }
        }
        let gl_tail = c.pos();
        let mut lists = BTreeMap::new();
        let mut c = Cursor::new(&files.sy, 0).with_ids(h.ids);
        while !c.at_end() {
            let start = c.pos();
            let v = read_list(&mut c, h.wide).with_context(|| format!("{}sy", files.base))?;
            lists.insert(start, v);
        }
        let mut u = Self {
            header: h,
            globals,
            gl_tail,
            lists,
            syms: HashMap::new(),
        };
        let mut syms = HashMap::new();
        for s in u.globals.iter().chain(u.lists.values().flatten()) {
            if let Some(id) = s.id {
                syms.insert(
                    id,
                    SymInfo {
                        class: s.class,
                        name: s.name_text(),
                    },
                );
            }
        }
        u.syms = syms;
        Ok(u)
    }

    #[must_use]
    pub fn fdefs(&self) -> Vec<&Sym> {
        let mut f: Vec<&Sym> = self
            .globals
            .iter()
            .chain(self.lists.values().flatten())
            .filter(|s| s.ex.is_some())
            .collect();
        f.sort_by_key(|s| (stream_of(s), s.ex));
        f
    }

    #[must_use]
    pub fn by_id(&self) -> HashMap<u32, &Sym> {
        let mut m = HashMap::new();
        for s in self.globals.iter().chain(self.lists.values().flatten()) {
            if let Some(id) = s.id
                && (s.ex.is_some() || !m.contains_key(&id))
            {
                m.insert(id, s);
            }
        }
        m
    }

    #[must_use]
    pub fn short(&self, id: u32) -> String {
        match self.syms.get(&id) {
            Some(s) if !s.name.is_empty() => s.name.clone(),
            Some(s) if s.class == sym::CLASS_LABEL => format!("L{id:x}"),
            _ => format!("#{id:x}"),
        }
    }

    #[must_use]
    pub fn full(&self, id: u32) -> String {
        match self.syms.get(&id) {
            Some(s) if !s.name.is_empty() => format!("#{id:x}:{}", s.name),
            Some(s) if s.class == sym::CLASS_LABEL => format!("#{id:x}:L"),
            _ => format!("#{id:x}"),
        }
    }

    #[must_use]
    pub fn dlp(&self) -> bool {
        self.header.dlp()
    }

    #[must_use]
    pub fn body_start(&self, ex: &[u8], g: &Grammar) -> Option<usize> {
        if ex.first() != Some(&ops::EXHEAD) {
            return Some(0);
        }
        let mut c = Cursor::new(ex, 0).with_ids(self.header.ids);
        let head = ex::read(&mut c, g, self.dlp()).ok()?;
        head.long().and_then(|n| usize::try_from(n).ok())
    }

    pub fn body(&self, files: &Files, g: &Grammar, f: &Sym) -> IlResult<Vec<Rec>> {
        let at =
            f.ex.and_then(|o| usize::try_from(o).ok())
                .ok_or_else(|| error(f.off, "a definition with no ex offset"))?;
        let data: &[u8] = match stream_of(f) {
            Stream::Ex => &files.ex,
            Stream::Pch => files.pch.as_deref().ok_or_else(|| {
                error(
                    f.off,
                    format!(
                        "{}: its body is in the pch (fflags bit 0), and no pch was given",
                        f.name_text()
                    ),
                )
            })?,
        };
        if at >= data.len() {
            return Err(error(
                at,
                format!("{}: ex offset past the end of its stream", f.name_text()),
            ));
        }
        let mut c = Cursor::new(data, at).with_ids(self.header.ids);
        let mut v = Vec::new();
        loop {
            let r = ex::read(&mut c, g, self.dlp())?;
            let done = r.ends_body();
            let eof = r.op == ops::EOF;
            v.push(r);
            if done {
                return Ok(v);
            }
            if eof || c.at_end() {
                return Err(error(
                    at,
                    format!("{}: the body has no end (54 0)", f.name_text()),
                ));
            }
        }
    }
}

#[must_use]
pub fn stream_of(s: &Sym) -> Stream {
    if s.fflags() & FFLAG_PCH != 0 {
        Stream::Pch
    } else {
        Stream::Ex
    }
}

impl Names for Unit {
    fn short(&self, id: u32) -> String {
        Unit::short(self, id)
    }

    fn full(&self, id: u32) -> String {
        Unit::full(self, id)
    }
}

#[derive(Debug, Clone, Default)]
pub struct Globals {
    pub header: Option<Header>,
    pub records: Vec<Sym>,
    pub error: Option<String>,
}

#[must_use]
pub fn globals(gl: &[u8]) -> Globals {
    let mut out = Globals::default();
    let mut c = Cursor::new(gl, 0);
    let h = match header::read(&mut c) {
        Ok(h) => h,
        Err(e) => {
            out.error = Some(e.to_string());
            return out;
        }
    };
    let mut c = c.with_ids(h.ids);
    while !c.at_end() {
        match sym::read(&mut c, h.wide) {
            Ok(s) => {
                let end = s.ends_list;
                out.records.push(s);
                if end {
                    break;
                }
            }
            Err(e) => {
                out.error = Some(e.to_string());
                break;
            }
        }
    }
    out.header = Some(h);
    out
}
