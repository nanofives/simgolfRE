use msvc6_image::coff::{
    CLASS_EXTERNAL, CLASS_STATIC, CLASS_WEAK_EXTERNAL, Object, SCN_LNK_COMDAT, SELECT_ANY,
    SELECT_ASSOCIATIVE, Symbol,
};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Leader {
    pub section: u16,
    pub selection: u8,
    pub name: String,
    pub external: bool,
}

#[must_use]
pub fn is_section_symbol(s: &Symbol) -> bool {
    !s.aux.is_empty() && s.class == CLASS_STATIC && s.typ == 0
}

#[must_use]
pub fn leaders(o: &Object<'_>) -> Vec<Leader> {
    let mut out = Vec::new();
    let mut waiting: BTreeMap<u16, u8> = BTreeMap::new();
    let mut done: BTreeSet<u16> = BTreeSet::new();
    for s in &o.symbols {
        let Some(number) = s.section_number() else {
            continue;
        };
        if is_section_symbol(s)
            && let Some(def) = s.section_definition()
        {
            let comdat = o
                .section(number)
                .is_some_and(|h| h.characteristics & SCN_LNK_COMDAT != 0);
            if comdat
                && def.selection != 0
                && def.selection != SELECT_ASSOCIATIVE
                && !done.contains(&number)
            {
                waiting.insert(number, def.selection);
            }
            continue;
        }
        if let Some(selection) = waiting.remove(&number) {
            done.insert(number);
            out.push(Leader {
                section: number,
                selection,
                name: s.name.clone(),
                external: matches!(s.class, CLASS_EXTERNAL | CLASS_WEAK_EXTERNAL),
            });
        }
    }
    out
}

#[must_use]
pub fn associates(o: &Object<'_>) -> BTreeMap<u16, Vec<u16>> {
    let mut out: BTreeMap<u16, Vec<u16>> = BTreeMap::new();
    for s in o.symbols.iter().filter(|s| is_section_symbol(s)) {
        let (Some(def), Some(number)) = (s.section_definition(), s.section_number()) else {
            continue;
        };
        if def.selection == SELECT_ASSOCIATIVE {
            let v = out.entry(def.number).or_default();
            if !v.contains(&number) {
                v.push(number);
            }
        }
    }
    out
}

#[must_use]
pub fn select_any(o: &Object<'_>) -> BTreeSet<String> {
    leaders(o)
        .into_iter()
        .filter(|l| l.selection == SELECT_ANY)
        .map(|l| l.name)
        .collect()
}

#[must_use]
pub fn defined_externals(o: &Object<'_>) -> BTreeSet<String> {
    o.symbols
        .iter()
        .filter(|s| s.section_number().is_some() && s.class == CLASS_EXTERNAL && !s.name.is_empty())
        .map(|s| s.name.clone())
        .collect()
}

#[must_use]
pub fn undefined_ordered(o: &Object<'_>) -> Vec<String> {
    let defined: BTreeSet<&str> = o
        .symbols
        .iter()
        .filter(|s| s.section != 0 && !s.name.is_empty())
        .map(|s| s.name.as_str())
        .collect();
    let mut seen = BTreeSet::new();
    let mut out = Vec::new();
    for s in &o.symbols {
        if s.section != 0 || s.class != CLASS_EXTERNAL || s.value != 0 || s.name.is_empty() {
            continue;
        }
        if defined.contains(s.name.as_str()) || !seen.insert(s.name.as_str()) {
            continue;
        }
        out.push(s.name.clone());
    }
    out
}

#[must_use]
pub fn weak_defaults(o: &Object<'_>) -> Vec<(String, String)> {
    let mut out = Vec::new();
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
            out.push((s.name.clone(), def.name.clone()));
        }
    }
    out
}
