use anyhow::Result;
use msvc6_base::digest::md5_hex;
use serde::Serialize;

use crate::archive::Archive;
use crate::coff::{CLASS_FILE, ImportObject, Object, check_tables, selection_name};

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct MemberDigest {
    pub sections: Vec<String>,
    pub relocations: Vec<String>,
    pub comdats: Vec<String>,
    pub symbols: Vec<String>,
    pub comp_id: Option<u32>,
}

pub fn digest(d: &[u8]) -> Result<MemberDigest> {
    if let Some(i) = ImportObject::parse(d) {
        return Ok(MemberDigest {
            symbols: vec![format!(
                "import {} from {} hint {} kind {} name type {}",
                i.symbol,
                i.dll,
                i.hint,
                i.kind,
                i.name_type_name()
            )],
            ..MemberDigest::default()
        });
    }
    let o = Object::parse(d)?;
    check_tables(&o)?;
    let mut g = MemberDigest {
        comp_id: o.comp_id(),
        ..MemberDigest::default()
    };
    for s in &o.sections {
        g.sections.push(format!(
            "{} chars {:#010x} size {} md5 {}",
            s.name,
            s.characteristics,
            s.size,
            md5_hex(o.section_data(s))
        ));
        for r in o.relocations(s) {
            let target = o
                .symbol(r.symbol)
                .map_or_else(|| format!("#{}", r.symbol), |t| t.name.clone());
            g.relocations
                .push(format!("{}+{:#x} type {} -> {target}", s.name, r.va, r.typ));
        }
    }
    for c in o.comdats() {
        g.comdats.push(format!(
            "section {} {}{} {}",
            c.section,
            selection_name(c.selection),
            c.associated
                .map_or_else(String::new, |a| format!(" of {a}")),
            c.symbol.as_deref().unwrap_or("-")
        ));
    }
    for s in &o.symbols {
        if s.class == CLASS_FILE {
            continue;
        }
        g.symbols.push(format!(
            "{} value {:#x} section {} type {:#x} class {:#x}",
            s.name, s.value, s.section, s.typ, s.class
        ));
    }
    Ok(g)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct FirstDiff {
    pub row: usize,
    pub ours_rows: usize,
    pub reference_rows: usize,
    pub ours: Option<String>,
    pub reference: Option<String>,
}

#[must_use]
pub fn first_diff(a: &[String], b: &[String]) -> Option<FirstDiff> {
    if a == b {
        return None;
    }
    let i = a.iter().zip(b).take_while(|(x, y)| x == y).count();
    Some(FirstDiff {
        row: i,
        ours_rows: a.len(),
        reference_rows: b.len(),
        ours: a.get(i).cloned(),
        reference: b.get(i).cloned(),
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct MemberRow {
    pub member: String,
    pub status: &'static str,
    pub ours_comp_id: Option<u32>,
    pub reference_comp_id: Option<u32>,
    pub differences: Vec<(&'static str, FirstDiff)>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct LibDiff {
    pub ours_members: usize,
    pub reference_members: usize,
    pub order: Option<FirstDiff>,
    pub rows: Vec<MemberRow>,
}

impl LibDiff {
    #[must_use]
    pub fn identical(&self) -> bool {
        self.order.is_none() && self.rows.iter().all(|r| r.status == "identical")
    }
}

fn key(n: &str) -> String {
    n.trim_end_matches('/').to_ascii_lowercase()
}

fn content<'a>(a: &'a Archive<'a>) -> Vec<(String, &'a [u8])> {
    let mut seen: std::collections::BTreeMap<String, usize> = std::collections::BTreeMap::new();
    a.content()
        .map(|(m, d)| {
            let k = key(&m.name);
            let n = seen.entry(k.clone()).or_insert(0);
            *n = n.saturating_add(1);
            if *n == 1 {
                (k, d)
            } else {
                (format!("{k}#{n}"), d)
            }
        })
        .collect()
}

pub fn diff(ours: &Archive<'_>, reference: &Archive<'_>) -> Result<LibDiff> {
    let a = content(ours);
    let b = content(reference);
    let an: Vec<String> = a.iter().map(|(n, _)| n.clone()).collect();
    let bn: Vec<String> = b.iter().map(|(n, _)| n.clone()).collect();
    let by_name: std::collections::BTreeMap<&str, &[u8]> =
        b.iter().map(|(n, d)| (n.as_str(), *d)).collect();
    let ours_names: std::collections::BTreeSet<&str> = an.iter().map(String::as_str).collect();
    let mut rows = Vec::new();
    for (n, ad) in &a {
        let Some(bd) = by_name.get(n.as_str()) else {
            rows.push(MemberRow {
                member: n.clone(),
                status: "only-ours",
                ours_comp_id: None,
                reference_comp_id: None,
                differences: Vec::new(),
            });
            continue;
        };
        let (x, y) = (digest(ad)?, digest(bd)?);
        let differences: Vec<(&'static str, FirstDiff)> = [
            ("sections", first_diff(&x.sections, &y.sections)),
            ("relocations", first_diff(&x.relocations, &y.relocations)),
            ("comdats", first_diff(&x.comdats, &y.comdats)),
            ("symbols", first_diff(&x.symbols, &y.symbols)),
        ]
        .into_iter()
        .filter_map(|(k, d)| d.map(|d| (k, d)))
        .collect();
        rows.push(MemberRow {
            member: n.clone(),
            status: if differences.is_empty() {
                "identical"
            } else {
                "differs"
            },
            ours_comp_id: x.comp_id,
            reference_comp_id: y.comp_id,
            differences,
        });
    }
    for n in bn.iter().filter(|n| !ours_names.contains(n.as_str())) {
        rows.push(MemberRow {
            member: n.clone(),
            status: "only-reference",
            ours_comp_id: None,
            reference_comp_id: None,
            differences: Vec::new(),
        });
    }
    Ok(LibDiff {
        ours_members: an.len(),
        reference_members: bn.len(),
        order: first_diff(&an, &bn),
        rows,
    })
}
