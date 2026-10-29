use anyhow::{Context as _, Result, bail};
use msvc6_image::archive::{Archive, MemberKind};
use msvc6_image::coff::{ImportObject, Object};
use serde::Serialize;
use std::collections::BTreeMap;

use crate::iat::{self, Blocks, Control, Pull, block_order, compare, invert, sorted_population};
use crate::qsort::Crt;
use crate::verbose::Kind;

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ImportLib {
    pub dll: String,
    pub by_name: BTreeMap<String, String>,
    pub by_ordinal: BTreeMap<u16, String>,
}

pub fn read_import_lib(data: &[u8]) -> Result<Vec<ImportLib>> {
    let ar = Archive::parse(data)?;
    let mut by_dll: BTreeMap<String, ImportLib> = BTreeMap::new();
    for (m, d) in ar.content() {
        match m.kind {
            MemberKind::Import => {
                let Some(io) = ImportObject::parse(d) else {
                    continue;
                };
                let e = by_dll
                    .entry(io.dll.to_ascii_lowercase())
                    .or_insert_with(|| ImportLib {
                        dll: io.dll.clone(),
                        ..ImportLib::default()
                    });
                let imp = format!("__imp_{}", io.symbol);
                if io.name_type == 0 {
                    e.by_ordinal.insert(io.hint, imp);
                } else {
                    e.by_name
                        .insert(msvc6_image::archive::undecorate_import(&imp), imp.clone());
                    let bare = io
                        .symbol
                        .strip_prefix('_')
                        .unwrap_or(&io.symbol)
                        .to_string();
                    e.by_name.entry(bare).or_insert(imp.clone());
                    e.by_name.entry(io.symbol.clone()).or_insert(imp);
                }
            }
            MemberKind::Object => {
                let Ok(o) = Object::parse(d) else { continue };
                let dll = m.name.clone();
                if !crate::verbose::is_import_member(&dll) {
                    continue;
                }
                let e = by_dll
                    .entry(dll.to_ascii_lowercase())
                    .or_insert_with(|| ImportLib {
                        dll: dll.clone(),
                        ..ImportLib::default()
                    });
                for s in o.defined_externals() {
                    if let Some(code) = s.strip_prefix("__imp_") {
                        e.by_name
                            .entry(msvc6_image::archive::undecorate_import(&s))
                            .or_insert(s.clone());
                        let bare = code.strip_prefix('_').unwrap_or(code).to_string();
                        e.by_name.entry(bare).or_insert(s.clone());
                        e.by_name.entry(code.to_string()).or_insert(s.clone());
                    }
                }
            }
            _ => {}
        }
    }
    Ok(by_dll.into_values().collect())
}

impl ImportLib {
    #[must_use]
    pub fn symbol_for(&self, name: &str) -> Option<&String> {
        if let Some(n) = name
            .strip_prefix("Ordinal(")
            .and_then(|r| r.strip_suffix(')'))
            .and_then(|n| n.parse::<u16>().ok())
        {
            return self.by_ordinal.get(&n);
        }
        self.by_name.get(name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Include {
    pub dll: String,
    pub name: String,
    pub symbol: String,
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Plan {
    pub libraries: Vec<String>,
    pub includes: Vec<Include>,
    pub same: usize,
    pub slots: usize,
}

pub fn plan(blocks: &Blocks, dlls: &[String], libs: &[ImportLib], crt: Crt) -> Result<Plan> {
    let lib_of = |dll: &str| libs.iter().find(|l| l.dll.eq_ignore_ascii_case(dll));
    let mut pulls: Vec<Pull> = Vec::new();
    let mut names: BTreeMap<(String, String), String> = BTreeMap::new();
    for dll in dlls {
        let Some(block) = blocks.get(&dll.to_ascii_lowercase()) else {
            bail!("{dll}: the image imports nothing from it");
        };
        let lib = lib_of(dll).with_context(|| format!("{dll}: no import library holds it"))?;
        for (_, name) in block {
            let sym = lib.symbol_for(name).with_context(|| {
                format!(
                    "{dll}!{name}: the import library {} has no symbol for it",
                    lib.dll
                )
            })?;
            names.insert((dll.to_ascii_lowercase(), sym.clone()), name.clone());
            pulls.push(Pull {
                member: lib.dll.clone(),
                symbol: sym.clone(),
                kind: Kind::Import,
                first_ref: None,
            });
        }
        pulls.push(Pull {
            member: lib.dll.clone(),
            symbol: format!(
                "\u{7f}{}_NULL_THUNK_DATA",
                lib.dll
                    .rsplit_once('.')
                    .map_or(lib.dll.as_str(), |(s, _)| s)
            ),
            kind: Kind::NullThunk,
            first_ref: None,
        });
    }
    if blocks.len() != dlls.len() {
        let missing: Vec<&String> = blocks
            .keys()
            .filter(|k| !dlls.iter().any(|d| d.eq_ignore_ascii_case(k)))
            .collect();
        bail!(
            "the library order names {} of the image's {} DLLs (missing {missing:?})",
            dlls.len(),
            blocks.len()
        );
    }
    let sorted = sorted_population(&pulls, crt, Control::Model)?;
    let inv = invert(&pulls, &sorted, blocks);
    let order = iat::apply_inversion(&pulls, &inv)?;
    let check = sorted_population(&order, crt, Control::Model)?;
    let c = compare(&order, &check, blocks, &block_order(&order, &check));
    if c.same != c.slots {
        bail!(
            "the inverted order sorts into {} of {} slots: the model cannot reach this IAT",
            c.same,
            c.slots
        );
    }
    let includes = order
        .iter()
        .filter(|p| p.kind == Kind::Import)
        .map(|p| Include {
            dll: p.member.clone(),
            name: names
                .get(&(p.member.to_ascii_lowercase(), p.symbol.clone()))
                .cloned()
                .unwrap_or_default(),
            symbol: p.symbol.clone(),
        })
        .collect();
    Ok(Plan {
        libraries: dlls.to_vec(),
        includes,
        same: c.same,
        slots: c.slots,
    })
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Check {
    pub dll: String,
    pub same: usize,
    pub slots: usize,
    pub first: Option<(usize, String, String)>,
}

#[must_use]
pub fn check(want: &Blocks, got: &Blocks) -> Vec<Check> {
    want.iter()
        .map(|(dll, w)| {
            let empty = Vec::new();
            let g = got.get(dll).unwrap_or(&empty);
            let same = w.iter().zip(g).filter(|(a, b)| a.1 == b.1).count();
            let first = w
                .iter()
                .zip(g)
                .position(|(a, b)| a.1 != b.1)
                .or_else(|| (w.len() != g.len()).then_some(w.len().min(g.len())))
                .map(|i| {
                    (
                        i,
                        w.get(i).map_or_else(|| "-".to_string(), |x| x.1.clone()),
                        g.get(i).map_or_else(|| "-".to_string(), |x| x.1.clone()),
                    )
                });
            Check {
                dll: dll.clone(),
                same,
                slots: w.len(),
                first,
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn lib(dll: &str, names: &[&str]) -> ImportLib {
        ImportLib {
            dll: dll.into(),
            by_name: names
                .iter()
                .map(|n| ((*n).to_string(), format!("__imp__{n}@0")))
                .collect(),
            by_ordinal: BTreeMap::new(),
        }
    }

    #[test]
    fn the_include_order_sorts_into_the_wanted_iat() -> Result<()> {
        let k: Vec<String> = (0..12).rev().map(|i| format!("K{i}")).collect();
        let u: Vec<String> = ["U3", "U0", "U4", "U1", "U2"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        let mut want: Blocks = BTreeMap::new();
        for (dll, names) in [("kernel32.dll", &k), ("user32.dll", &u)] {
            want.insert(
                dll.to_string(),
                names
                    .iter()
                    .enumerate()
                    .map(|(j, n)| (u32::try_from(j * 4).unwrap_or(0), n.clone()))
                    .collect(),
            );
        }
        let kn: Vec<&str> = k.iter().map(String::as_str).collect();
        let un: Vec<&str> = u.iter().map(String::as_str).collect();
        let libs = vec![lib("KERNEL32.dll", &kn), lib("USER32.dll", &un)];
        let p = plan(
            &want,
            &["KERNEL32.dll".into(), "USER32.dll".into()],
            &libs,
            Crt::Vc6,
        )?;
        assert_eq!((p.same, p.slots), (17, 17));
        assert_eq!(p.includes.len(), 17);
        assert!(
            p.includes
                .iter()
                .all(|i| i.symbol == format!("__imp__{}@0", i.name))
        );
        assert!(plan(&want, &["KERNEL32.dll".into()], &libs, Crt::Vc6).is_err());
        Ok(())
    }
}
