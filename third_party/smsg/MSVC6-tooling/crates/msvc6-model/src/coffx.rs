use anyhow::Result;
use msvc6_diff::comdat::{self, Comdat};
use msvc6_image::coff::{SELECT_ANY, SELECT_ASSOCIATIVE};
use std::collections::{BTreeMap, BTreeSet};

pub fn comdat_selections_of(d: &[u8]) -> Result<BTreeMap<String, bool>> {
    let o = comdat::parse(d)?;
    let mut out = BTreeMap::new();
    for c in o.comdats() {
        if c.selection == SELECT_ASSOCIATIVE {
            continue;
        }
        let Some(name) = c.symbol.filter(|n| !n.is_empty()) else {
            continue;
        };
        out.insert(name, c.selection == SELECT_ANY);
    }
    Ok(out)
}

pub fn associated_sections_of(d: &[u8]) -> Result<BTreeMap<u16, String>> {
    let o = comdat::parse(d)?;
    let all = o.comdats();
    let mut out = BTreeMap::new();
    for c in all.iter().filter(|c| c.selection != SELECT_ASSOCIATIVE) {
        let Some(name) = c.symbol.as_ref().filter(|n| !n.is_empty()) else {
            continue;
        };
        out.entry(c.section).or_insert_with(|| name.clone());
        for a in all
            .iter()
            .filter(|a| a.selection == SELECT_ASSOCIATIVE && a.associated == Some(c.section))
        {
            out.entry(a.section).or_insert_with(|| name.clone());
        }
    }
    Ok(out)
}

pub fn code_comdats_named_of(d: &[u8]) -> Result<Vec<Comdat>> {
    comdat::comdats_of(d, &[".text", ".text$x"])
}

pub fn noncomdat_data_targets_of(d: &[u8]) -> Result<Vec<String>> {
    let o = comdat::parse(d)?;
    let names: BTreeSet<String> = o
        .sections
        .iter()
        .map(|s| s.name.clone())
        .filter(|n| {
            !n.starts_with(".text")
                && !n.starts_with(".debug")
                && !n.starts_with(".xdata")
                && n != ".drectve"
        })
        .collect();
    let want: Vec<&str> = names.iter().map(String::as_str).collect();
    Ok(comdat::comdats_of(d, &want)?
        .into_iter()
        .filter(|c| !c.comdat)
        .flat_map(|c| c.relocations.into_iter().map(|r| r.symbol))
        .collect())
}

pub fn data_comdats_of(d: &[u8]) -> Result<Vec<Comdat>> {
    comdat::data_of(d)
}
