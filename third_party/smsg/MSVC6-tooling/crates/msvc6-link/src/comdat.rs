use anyhow::{Context as _, Result, bail};
use msvc6_image::coff::{CLASS_STATIC, CLASS_WEAK_EXTERNAL, Object, SELECT_ASSOCIATIVE, Symbol};
use std::collections::{BTreeMap, HashMap};

pub const BYTES_BUDGET: usize = 4;

pub const SCN_MEM_EXECUTE: u32 = 0x2000_0000;
pub const SCN_MEM_WRITE: u32 = 0x8000_0000;
pub const SCN_CNT_CODE: u32 = 0x20;

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Target {
    Ext(String),
    Local(usize, usize),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ObjComdat {
    pub name: String,
    pub external: bool,
    pub size: u32,
    pub nreloc: u16,
    pub chr: u32,
    pub sel: u8,
    pub bytes: Vec<u8>,
    pub relocs: Vec<(u32, u16, Target)>,
    pub sec: usize,
    pub sec_name: String,
    pub parent: Option<usize>,
    pub sym: usize,
    pub cksum: u32,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ObjInfo {
    pub comdats: Vec<ObjComdat>,
    pub plain_defs: Vec<String>,
    pub aliases: Vec<(String, String)>,
}

fn is_global(s: &Symbol) -> bool {
    matches!(
        s.class,
        msvc6_image::coff::CLASS_EXTERNAL | CLASS_WEAK_EXTERNAL
    )
}

fn has_aux_section(s: &Symbol) -> bool {
    !s.aux.is_empty() && s.class == CLASS_STATIC && s.typ == 0
}

fn aux_u32(a: &[u8; 18], at: usize) -> u32 {
    msvc6_base::le::u32_at(a, at).unwrap_or(0)
}

fn aux_u16(a: &[u8; 18], at: usize) -> u16 {
    msvc6_base::le::u16_at(a, at).unwrap_or(0)
}

#[must_use]
pub fn is_candidate(c: &ObjComdat) -> bool {
    c.size > 0
        && c.chr & SCN_MEM_EXECUTE != 0
        && !c.name.starts_with("_$E")
        && (c.chr & SCN_MEM_WRITE == 0 || c.chr & SCN_CNT_CODE != 0)
}

pub fn object_comdats(d: &[u8], obj: usize) -> Result<ObjInfo> {
    let f = Object::parse(d).context("not a COFF object")?;
    type SecInfo = (String, bool, u8, Option<usize>, usize);
    let mut by_sec: BTreeMap<usize, SecInfo> = BTreeMap::new();
    let syms = &f.symbols;
    for (k, s) in syms.iter().enumerate() {
        if !has_aux_section(s) {
            continue;
        }
        let Some(number) = s.section_number() else {
            continue;
        };
        let Some(a) = s.aux.first() else { continue };
        let selection = a.get(14).copied().unwrap_or(0);
        if selection == 0 || selection == SELECT_ASSOCIATIVE {
            continue;
        }
        let Some(c) = syms.iter().skip(k + 1).find(|x| x.section == s.section) else {
            continue;
        };
        let global = is_global(c);
        by_sec.entry(usize::from(number)).or_insert_with(|| {
            (
                c.name.clone(),
                global,
                if global { selection } else { 0 },
                None,
                usize::try_from(c.index).unwrap_or(usize::MAX),
            )
        });
    }
    let mut names: HashMap<usize, (String, bool)> = HashMap::new();
    let mut info = ObjInfo::default();
    let mut weak: Vec<(String, usize)> = Vec::new();
    let mut cksums: HashMap<usize, u32> = HashMap::new();
    for y in syms {
        let idx = usize::try_from(y.index).unwrap_or(usize::MAX);
        names.insert(idx, (y.name.clone(), is_global(y)));
        if has_aux_section(y)
            && let (Some(s), Some(a)) = (y.section_number(), y.aux.first())
        {
            let s = usize::from(s);
            cksums.entry(s).or_insert(aux_u32(a, 8));
            if a.get(14).copied() == Some(SELECT_ASSOCIATIVE) {
                let parent = usize::from(aux_u16(a, 12));
                by_sec.entry(s).or_insert((
                    y.name.clone(),
                    false,
                    SELECT_ASSOCIATIVE,
                    Some(parent),
                    idx,
                ));
            }
        }
        if y.class == CLASS_WEAK_EXTERNAL
            && let Some(a) = y.aux.first()
        {
            let t = usize::try_from(aux_u32(a, 0)).unwrap_or(usize::MAX);
            weak.push((y.name.clone(), t));
        }
        if let Some(i) = y.section_number().map(usize::from)
            && is_global(y)
            && !by_sec.contains_key(&i)
            && !y.name.is_empty()
        {
            info.plain_defs.push(y.name.clone());
        }
    }
    for (alias, t) in weak {
        if let Some((n, _)) = names.get(&t) {
            info.aliases.push((alias, n.clone()));
        }
    }
    let budget = d.len().saturating_mul(BYTES_BUDGET).saturating_add(1 << 16);
    let mut copied = 0usize;
    for s in &f.sections {
        let si = usize::from(s.number);
        let Some((name, external, sel, parent, sym)) = by_sec.get(&si).cloned() else {
            continue;
        };
        let bytes = f.section_data(s).to_vec();
        let relocs: Vec<(u32, u16, Target)> = f
            .relocations(s)
            .into_iter()
            .map(|r| {
                let si = usize::try_from(r.symbol).unwrap_or(usize::MAX);
                let t = match names.get(&si) {
                    Some((n, true)) => Target::Ext(n.clone()),
                    _ => Target::Local(obj, si),
                };
                (r.va, r.typ, t)
            })
            .collect();
        copied = copied
            .saturating_add(bytes.len())
            .saturating_add(relocs.len().saturating_mul(10));
        if copied > budget {
            bail!(
                "not a COFF object LINK wrote: its COMDAT sections read {copied} bytes of a {}-byte file (sections share their data)",
                d.len()
            );
        }
        info.comdats.push(ObjComdat {
            name,
            external,
            size: s.size,
            nreloc: s.relocs,
            chr: s.characteristics,
            sel,
            bytes,
            relocs,
            sec: si,
            sec_name: s.name.clone(),
            parent,
            sym,
            cksum: cksums.get(&si).copied().unwrap_or(0),
        });
    }
    Ok(info)
}

#[must_use]
pub fn is_short_import(d: &[u8]) -> bool {
    d.get(0..4) == Some(&[0, 0, 0xff, 0xff][..])
}
