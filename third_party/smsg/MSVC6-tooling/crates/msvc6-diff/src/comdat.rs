use anyhow::{Context as _, Result, bail};
use msvc6_image::coff::{
    CLASS_EXTERNAL, CLASS_STATIC, CLASS_WEAK_EXTERNAL, Object, SCN_CNT_CODE, SectionHeader,
};
use std::collections::BTreeMap;
use std::path::Path;

pub const REL_DIR32: u16 = 6;
pub const REL_DIR32NB: u16 = 7;
pub const REL_REL32: u16 = 20;
pub const FIELD: usize = 4;
pub const MAX_OBJECT: usize = 256 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Reloc {
    pub at: u32,
    pub symbol: String,
    pub kind: u16,
    pub external: bool,
    pub global: bool,
    pub target_section: Option<String>,
    pub target_offset: u32,
    pub target_index: Option<u16>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Comdat {
    pub index: u16,
    pub section: String,
    pub symbol: Option<String>,
    pub external: bool,
    pub bytes: Vec<u8>,
    pub relocated: Vec<bool>,
    pub comdat: bool,
    pub align: u32,
    pub relocations: Vec<Reloc>,
}

impl Comdat {
    #[must_use]
    pub fn literal_len(&self) -> usize {
        self.relocated.iter().filter(|r| !**r).count()
    }

    #[must_use]
    pub fn name(&self) -> &str {
        self.symbol.as_deref().unwrap_or(self.section.as_str())
    }

    #[must_use]
    pub fn field(&self, at: u32) -> u32 {
        let at = at as usize;
        self.bytes
            .get(at..at.saturating_add(FIELD))
            .and_then(|b| <[u8; FIELD]>::try_from(b).ok())
            .map_or(0, u32::from_le_bytes)
    }

    #[must_use]
    pub fn relocs_in(&self, lo: u32, hi: u32) -> &[Reloc] {
        let a = self.relocations.partition_point(|r| r.at < lo);
        let b = self.relocations.partition_point(|r| r.at < hi).max(a);
        self.relocations.get(a..b).unwrap_or_default()
    }

    #[must_use]
    pub fn reloc_at(&self, at: u32) -> &[Reloc] {
        self.relocs_in(at, at.saturating_add(1))
    }
}

pub fn read(path: &Path) -> Result<Vec<u8>> {
    let len = std::fs::metadata(path)
        .with_context(|| format!("reading {}", path.display()))?
        .len();
    if usize::try_from(len).map_or(true, |l| l > MAX_OBJECT) {
        bail!(
            "{}: {len} bytes, more than the {MAX_OBJECT} an object may hold",
            path.display()
        );
    }
    std::fs::read(path).with_context(|| format!("reading {}", path.display()))
}

pub fn parse(d: &[u8]) -> Result<Object<'_>> {
    if d.len() > MAX_OBJECT {
        bail!(
            "{} bytes, more than the {MAX_OBJECT} an object may hold",
            d.len()
        );
    }
    let o = Object::parse(d)?;
    msvc6_image::coff::check_tables(&o)?;
    Ok(o)
}

fn owners(o: &Object<'_>) -> BTreeMap<u16, (String, bool)> {
    let mut owner: BTreeMap<u16, (String, bool)> = BTreeMap::new();
    for s in &o.symbols {
        let Some(section) = s.section_number() else {
            continue;
        };
        if s.name.is_empty() {
            continue;
        }
        let is_ext = s.class == CLASS_EXTERNAL;
        let is_section_name = s.name.starts_with('.');
        match owner.get(&section) {
            Some((_, true)) => {}
            _ if is_ext => {
                owner.insert(section, (s.name.clone(), true));
            }
            None if !is_section_name && s.class == CLASS_STATIC => {
                owner.insert(section, (s.name.clone(), false));
            }
            _ => {}
        }
    }
    owner
}

fn relocations_of(
    o: &Object<'_>,
    sec: &SectionHeader,
    bytes_len: usize,
) -> Result<(Vec<Reloc>, Vec<bool>)> {
    let mut relocated = vec![false; bytes_len];
    let mut out = Vec::new();
    for (k, r) in o.relocations(sec).into_iter().enumerate() {
        let at = r.va as usize;
        if at.checked_add(FIELD).is_none_or(|e| e > bytes_len) {
            bail!(
                "section {} `{}`: relocation {k}'s field at {at:#x} runs past the section's {bytes_len} byte(s)",
                sec.number,
                sec.name
            );
        }
        for b in 0..FIELD {
            if let Some(slot) = relocated.get_mut(at.saturating_add(b)) {
                *slot = true;
            }
        }
        let (symbol, external, global, target_section, target_offset, target_index) =
            match o.symbol(r.symbol) {
                Some(s) => {
                    let idx = s.section_number().filter(|i| o.section(*i).is_some());
                    (
                        s.name.clone(),
                        s.section == 0,
                        s.class == CLASS_EXTERNAL,
                        idx.and_then(|i| o.section(i)).map(|x| x.name.clone()),
                        s.value,
                        idx,
                    )
                }
                None => (String::new(), false, false, None, 0, None),
            };
        out.push(Reloc {
            at: r.va,
            symbol,
            kind: r.typ,
            external,
            global,
            target_section,
            target_offset,
            target_index,
        });
    }
    out.sort_by_key(|r| r.at);
    Ok((out, relocated))
}

pub fn comdats_of(d: &[u8], want: &[&str]) -> Result<Vec<Comdat>> {
    let o = parse(d)?;
    let owner = owners(&o);
    let mut out = Vec::new();
    for sec in &o.sections {
        if !want.contains(&sec.name.as_str()) {
            continue;
        }
        let bytes = o.section_data(sec).to_vec();
        let (relocations, relocated) = relocations_of(&o, sec, bytes.len())?;
        out.push(Comdat {
            index: sec.number,
            section: sec.name.clone(),
            symbol: owner.get(&sec.number).map(|(n, _)| n.clone()),
            external: owner.get(&sec.number).is_some_and(|(_, e)| *e),
            bytes,
            relocated,
            comdat: sec.is_comdat(),
            align: sec.align(),
            relocations,
        });
    }
    Ok(out)
}

pub fn text_of(d: &[u8]) -> Result<Vec<Comdat>> {
    comdats_of(d, &[".text"])
}

pub fn data_of(d: &[u8]) -> Result<Vec<Comdat>> {
    comdats_of(d, &[".rdata", ".data"])
}

pub fn text(path: &Path) -> Result<Vec<Comdat>> {
    text_of(&read(path)?).with_context(|| path.display().to_string())
}

pub fn data(path: &Path) -> Result<Vec<Comdat>> {
    data_of(&read(path)?).with_context(|| path.display().to_string())
}

pub fn has_code(d: &[u8]) -> Result<bool> {
    let o = parse(d)?;
    Ok(o.sections
        .iter()
        .any(|s| s.size > 0 && s.characteristics & SCN_CNT_CODE != 0))
}

pub fn defined_externals(d: &[u8]) -> Result<Vec<(u16, u32, String)>> {
    let o = parse(d)?;
    Ok(o.symbols
        .iter()
        .filter(|s| s.class == CLASS_EXTERNAL && !s.name.is_empty() && !s.name.starts_with('.'))
        .filter_map(|s| Some((s.section_number()?, s.value, s.name.clone())))
        .collect())
}

pub fn weak_defaults_of(d: &[u8]) -> Result<BTreeMap<String, String>> {
    let o = parse(d)?;
    let mut out = BTreeMap::new();
    for s in o.symbols.iter().filter(|s| s.class == CLASS_WEAK_EXTERNAL) {
        let Some(tag) = s
            .aux
            .first()
            .and_then(|a| msvc6_base::le::u32_at(a, 0).ok())
        else {
            continue;
        };
        if let Some(def) = o.symbol(tag)
            && !s.name.is_empty()
        {
            out.insert(s.name.clone(), def.name.clone());
        }
    }
    Ok(out)
}

pub fn import_name(sym: &str) -> Option<String> {
    let s = sym.strip_prefix("__imp_")?;
    if s.starts_with('?') {
        return Some(s.to_string());
    }
    let s = s.strip_prefix('_').unwrap_or(s);
    Some(match s.rsplit_once('@') {
        Some((h, t)) if !t.is_empty() && t.chars().all(|c| c.is_ascii_digit()) => h.to_string(),
        _ => s.to_string(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_import_symbol_names_its_import() {
        assert_eq!(
            import_name("__imp__GetTickCount@0").as_deref(),
            Some("GetTickCount")
        );
        assert_eq!(import_name("__imp_?f@@YAXXZ").as_deref(), Some("?f@@YAXXZ"));
        assert_eq!(import_name("_GetTickCount@0"), None);
        assert_eq!(import_name("__imp__strlen").as_deref(), Some("strlen"));
    }
}
