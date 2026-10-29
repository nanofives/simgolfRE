use msvc6_project::names::{Kind, NameOracle};
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};

pub const SKIPPED_PROVIDERS: [&str; 1] = ["imports"];

#[derive(Debug, Clone, Default)]
struct Tier {
    flat: HashMap<String, u32>,
    scoped: HashMap<String, HashMap<String, u32>>,
    source: String,
    rows: Vec<(String, u32, Option<String>)>,
}

#[derive(Debug, Clone, Copy)]
pub struct TierView<'a> {
    pub source: &'a str,
    pub flat: &'a HashMap<String, u32>,
    pub scoped: &'a HashMap<String, HashMap<String, u32>>,
    pub rows: &'a [(String, u32, Option<String>)],
}

#[derive(Debug, Clone, Default)]
pub struct Names {
    exports: HashMap<String, u32>,
    tiers: Vec<Tier>,
    absolute: HashMap<String, u32>,
    named: BTreeSet<u32>,
    by_rva: BTreeMap<u32, String>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Resolved {
    pub rva: u32,
    pub via_thunk: Option<u32>,
    pub exported: bool,
}

fn keep_low(m: &mut HashMap<String, u32>, name: &str, rva: u32) {
    m.entry(name.to_string())
        .and_modify(|r| *r = (*r).min(rva))
        .or_insert(rva);
}

impl Names {
    #[must_use]
    pub fn from_oracle(o: &NameOracle) -> Self {
        let mut n = Self::default();
        let bodies = o.named_bodies();
        let mut all = o.iter();
        for s in &o.stats {
            let rows: Vec<&msvc6_project::names::Name> = all.by_ref().take(s.names).collect();
            if s.provider == "exports" {
                for x in rows {
                    n.exports.entry(x.name.clone()).or_insert(x.rva);
                    n.named.insert(x.rva);
                    n.by_rva.entry(x.rva).or_insert_with(|| x.name.clone());
                }
                continue;
            }
            if !SKIPPED_PROVIDERS.contains(&s.provider.as_str()) {
                let mut t = tier(&mut n.absolute, &mut n.named, &rows, &bodies);
                t.source.clone_from(&s.source);
                n.tiers.push(t);
                for x in rows.iter().filter(|x| x.scope.is_none()) {
                    n.by_rva.entry(x.rva).or_insert_with(|| x.name.clone());
                }
            }
        }
        n
    }
}

fn tier(
    absolute: &mut HashMap<String, u32>,
    named: &mut BTreeSet<u32>,
    rows: &[&msvc6_project::names::Name],
    bodies: &HashSet<&str>,
) -> Tier {
    let mut t = Tier::default();
    for x in rows {
        if x.kind == Kind::Thunk && bodies.contains(x.name.as_str()) {
            named.insert(x.rva);
            continue;
        }
        if x.kind == Kind::Absolute {
            keep_low(absolute, &x.name, x.rva);
        } else {
            t.rows.push((x.name.clone(), x.rva, x.scope.clone()));
        }
        match &x.scope {
            Some(s) => {
                t.scoped
                    .entry(s.clone())
                    .or_default()
                    .entry(x.name.clone())
                    .or_insert(x.rva);
            }
            None => {
                keep_low(&mut t.flat, &x.name, x.rva);
                named.insert(x.rva);
            }
        }
    }
    t
}

impl Names {
    #[must_use]
    pub fn exports(&self) -> &HashMap<String, u32> {
        &self.exports
    }

    #[must_use]
    pub fn named(&self) -> &BTreeSet<u32> {
        &self.named
    }

    pub fn tiers(&self) -> impl Iterator<Item = TierView<'_>> {
        self.tiers.iter().map(|t| TierView {
            source: &t.source,
            flat: &t.flat,
            scoped: &t.scoped,
            rows: &t.rows,
        })
    }

    #[must_use]
    pub fn scope_names(&self) -> BTreeSet<&str> {
        self.tiers
            .iter()
            .flat_map(|t| t.scoped.keys().map(String::as_str))
            .collect()
    }

    #[must_use]
    pub fn by_rva(&self) -> &BTreeMap<u32, String> {
        &self.by_rva
    }

    #[must_use]
    pub fn absolute(&self, name: &str) -> Option<u32> {
        self.absolute.get(name).copied()
    }

    #[must_use]
    pub fn has_scope(&self, scope: &str) -> bool {
        self.tiers.iter().any(|t| t.scoped.contains_key(scope))
    }

    #[must_use]
    pub fn scoped(&self, scope: &str, name: &str) -> Option<u32> {
        self.tiers
            .iter()
            .find_map(|t| t.scoped.get(scope).and_then(|s| s.get(name)).copied())
    }

    #[must_use]
    pub fn recorded(&self, name: &str, scopes: &[&str]) -> Option<(u32, bool)> {
        if let Some(r) = self.exports.get(name) {
            return Some((*r, true));
        }
        self.tiers.iter().find_map(|t| {
            scopes
                .iter()
                .find_map(|s| t.scoped.get(*s).and_then(|m| m.get(name)))
                .or_else(|| t.flat.get(name))
                .map(|r| (*r, false))
        })
    }

    #[must_use]
    pub fn resolve(
        &self,
        name: &str,
        scopes: &[&str],
        text: &[u8],
        text_rva: u32,
    ) -> Option<Resolved> {
        let (rva, exported) = self.recorded(name, scopes)?;
        let off = rva.checked_sub(text_rva)? as usize;
        if let Some(tgt) = jump_slot(text, text_rva, rva, off) {
            let lands_on_thunk =
                tgt.checked_sub(text_rva).and_then(|o| text.get(o as usize)) == Some(&0xe9);
            if !lands_on_thunk {
                return Some(Resolved {
                    rva: tgt,
                    via_thunk: Some(rva),
                    exported,
                });
            }
        }
        Some(Resolved {
            rva,
            via_thunk: None,
            exported,
        })
    }

    #[must_use]
    pub fn rva(&self, name: &str, text: &[u8], text_rva: u32) -> Option<u32> {
        self.resolve(name, &[], text, text_rva).map(|r| r.rva)
    }

    #[must_use]
    pub fn rva_in(&self, name: &str, scopes: &[&str], text: &[u8], text_rva: u32) -> Option<u32> {
        self.resolve(name, scopes, text, text_rva).map(|r| r.rva)
    }
}

fn jump_slot(text: &[u8], _text_rva: u32, rva: u32, off: usize) -> Option<u32> {
    if text.get(off) != Some(&0xe9)
        || !matches!(text.get(off.saturating_add(5)), Some(&0xe9 | &0xcc))
    {
        return None;
    }
    let rel: [u8; 4] = text
        .get(off.saturating_add(1)..off.saturating_add(5))?
        .try_into()
        .ok()?;
    Some(
        rva.wrapping_add(5)
            .wrapping_add(i32::from_le_bytes(rel).cast_unsigned()),
    )
}

#[must_use]
pub fn ilt_slot(text: &[u8], text_rva: u32, rva: u32) -> Option<u32> {
    let off = usize::try_from(rva.checked_sub(text_rva)?).ok()?;
    jump_slot(text, text_rva, rva, off)
}

#[derive(Debug)]
pub struct Placer<'a> {
    claims: Vec<TierView<'a>>,
    notes: Vec<TierView<'a>>,
    map: BTreeMap<String, u32>,
    text: &'a [u8],
    text_rva: u32,
}

impl<'a> Placer<'a> {
    #[must_use]
    pub fn new(
        names: &'a Names,
        is_note: impl Fn(&str) -> bool,
        map: Option<&msvc6_project::mapfile::Map>,
        text: &'a [u8],
        text_rva: u32,
    ) -> Self {
        let (notes, claims) = names.tiers().partition(|t| is_note(t.source));
        let mut first = BTreeMap::new();
        for s in map.map(|m| m.symbols.as_slice()).unwrap_or_default() {
            first.entry(s.name.clone()).or_insert(s.rva);
        }
        Self {
            claims,
            notes,
            map: first,
            text,
            text_rva,
        }
    }

    #[must_use]
    pub fn place(&self, stem: &str, f: &str) -> Option<u32> {
        self.claims
            .iter()
            .find_map(|t| {
                t.scoped
                    .get(stem)
                    .and_then(|s| s.get(f))
                    .or_else(|| t.flat.get(f))
            })
            .or_else(|| self.map.get(f))
            .or_else(|| self.notes.iter().find_map(|t| t.flat.get(f)))
            .copied()
            .map(|r| ilt_slot(self.text, self.text_rva, r).unwrap_or(r))
    }
}

#[must_use]
pub fn follow_thunk(text: &[u8], text_rva: u32, rva: u32) -> u32 {
    let Ok(off) = usize::try_from(rva.wrapping_sub(text_rva)) else {
        return rva;
    };
    if text.get(off) != Some(&0xe9) {
        return rva;
    }
    let Some(arr) = text
        .get(off.saturating_add(1)..off.saturating_add(5))
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
    else {
        return rva;
    };
    let rel = i32::from_le_bytes(arr);
    let Ok(d) = u32::try_from(i64::from(rva) + 5 + i64::from(rel)) else {
        return rva;
    };
    match usize::try_from(d.wrapping_sub(text_rva)) {
        Ok(o) if o < text.len() && text.get(o) != Some(&0xe9) => d,
        _ => rva,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_project::names::Name;

    fn name(rva: u32, n: &str, scope: Option<&str>, provider: &str) -> Name {
        Name {
            rva,
            name: n.to_string(),
            kind: Kind::Label,
            size: None,
            scope: scope.map(str::to_string),
            provider: provider.to_string(),
        }
    }

    #[test]
    fn exports_then_each_provider_in_order_a_scope_beating_its_own_flat_row() {
        let mut o = NameOracle::default();
        o.add(
            "exports",
            "img",
            vec![name(0x1000, "_e", None, "exports")],
            Vec::new(),
        );
        o.add(
            "tsv",
            "symbols.tsv",
            vec![
                name(0x2000, "_s", None, "tsv"),
                name(0x2100, "_both", None, "tsv"),
            ],
            Vec::new(),
        );
        o.add(
            "tsv",
            "addresses.tsv",
            vec![
                name(0x3000, "_both", Some("Obj"), "tsv"),
                name(0x3100, "_x", None, "tsv"),
                name(0x3200, "_x", Some("Obj"), "tsv"),
            ],
            Vec::new(),
        );
        let n = Names::from_oracle(&o);
        let text = vec![0u8; 0x4000];
        assert_eq!(n.rva("_e", &text, 0), Some(0x1000));
        assert_eq!(n.rva_in("_both", &["Obj"], &text, 0), Some(0x2100));
        assert_eq!(n.rva_in("_x", &["Obj"], &text, 0), Some(0x3200));
        assert_eq!(n.rva_in("_x", &["Other"], &text, 0), Some(0x3100));
        assert_eq!(n.rva("_x", &text, 0), Some(0x3100));
        assert!(n.named().contains(&0x3100) && !n.named().contains(&0x3200));
    }

    #[test]
    fn a_jump_slot_resolves_to_its_body_unless_the_body_is_a_slot() {
        let mut text = vec![0xccu8; 0x40];
        text[0] = 0xe9;
        text[1..5].copy_from_slice(&0x1bu32.to_le_bytes());
        text[5] = 0xe9;
        text[6..10].copy_from_slice(&0x16u32.to_le_bytes());
        text[0x20] = 0x55;
        assert_eq!(ilt_slot(&text, 0x1000, 0x1000), Some(0x1020));
        assert_eq!(follow_thunk(&text, 0x1000, 0x1000), 0x1020);
        assert_eq!(follow_thunk(&text, 0x1000, 0x1020), 0x1020);
    }

    #[test]
    fn a_name_the_tables_give_an_ilt_thunk_and_its_body_records_the_body() {
        let thunk = |rva: u32, n: &str| Name {
            kind: Kind::Thunk,
            ..name(rva, n, None, "tsv")
        };
        let mut o = NameOracle::default();
        o.add(
            "tsv",
            "symbols.tsv",
            vec![thunk(0x1000, "_f"), thunk(0x1005, "_t")],
            Vec::new(),
        );
        o.add(
            "tsv",
            "addresses.tsv",
            vec![
                name(0x1020, "_f", None, "tsv"),
                name(0x1030, "_t", Some("obj"), "tsv"),
            ],
            Vec::new(),
        );
        let n = Names::from_oracle(&o);
        let mut text = vec![0xccu8; 0x40];
        text[0] = 0xe9;
        text[1..5].copy_from_slice(&0x1bu32.to_le_bytes());
        text[5] = 0xe9;
        text[6..10].copy_from_slice(&0x26u32.to_le_bytes());
        text[0x20] = 0x55;
        text[0x30] = 0x55;
        assert_eq!(n.recorded("_f", &[]), Some((0x1020, false)));
        assert_eq!(n.rva("_f", &text, 0x1000), Some(0x1020));
        assert_eq!(n.recorded("_t", &[]), Some((0x1005, false)));
        assert_eq!(n.rva("_t", &text, 0x1000), Some(0x1030));
        assert_eq!(n.rva_in("_t", &["obj"], &text, 0x1000), Some(0x1030));
        assert_eq!(n.rva_in("_t", &["other"], &text, 0x1000), Some(0x1030));
        assert!(n.named().contains(&0x1000) && n.named().contains(&0x1020));
        assert_eq!(n.by_rva().get(&0x1000).map(String::as_str), Some("_f"));
    }

    #[test]
    fn a_placer_takes_claims_scoped_then_flat_then_the_map_then_notes_and_follows_an_ilt_slot() {
        let mut o = NameOracle::default();
        o.add(
            "tsv",
            "tables/a.symbols.tsv",
            vec![
                name(0x1000, "_f", None, "tsv"),
                name(0x1030, "_g", Some("obj"), "tsv"),
                name(0x1034, "_g", None, "tsv"),
            ],
            Vec::new(),
        );
        o.add(
            "tsv",
            "tables/a.notes.tsv",
            vec![
                name(0x1010, "_f", None, "tsv"),
                name(0x1038, "_n", None, "tsv"),
                name(0x103c, "_k", None, "tsv"),
            ],
            Vec::new(),
        );
        let n = Names::from_oracle(&o);
        let map = msvc6_project::mapfile::Map::parse(
            std::path::Path::new("t.map"),
            " t\r\n\r\n Preferred load address is 10000000\r\n\r\n\
             \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
             \x200001:00000040  _m  10001040 f   obj.obj\r\n\
             \x200001:00000050  _n  10001050 f   obj.obj\r\n",
        )
        .unwrap_or_else(|e| panic!("{e:#}"));
        let mut text = vec![0xccu8; 0x60];
        text[0] = 0xe9;
        text[1..5].copy_from_slice(&0x1bu32.to_le_bytes());
        text[0x20] = 0x55;
        let p = Placer::new(&n, |s| s.ends_with("notes.tsv"), Some(&map), &text, 0x1000);
        assert_eq!(p.place("obj", "_f"), Some(0x1020), "the slot's body");
        assert_eq!(p.place("obj", "_g"), Some(0x1030));
        assert_eq!(p.place("other", "_g"), Some(0x1034));
        assert_eq!(p.place("obj", "_m"), Some(0x1040));
        assert_eq!(p.place("obj", "_n"), Some(0x1050), "the map before a note");
        assert_eq!(p.place("obj", "_k"), Some(0x103c));
        assert_eq!(p.place("obj", "_none"), None);
        let bare = Placer::new(&n, |_| false, None, &text, 0x1000);
        assert_eq!(bare.place("obj", "_n"), Some(0x1038));
    }
}
