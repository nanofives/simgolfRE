use anyhow::Result;
use msvc6_image::archive::{Archive, MemberKind};
use msvc6_image::census::members;
use msvc6_image::rich::{Rich, prodid_name};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::locate::Run;

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Count {
    pub library: String,
    pub members: usize,
    pub codeless: usize,
    pub linked: usize,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Bucket {
    pub prodid: Option<u32>,
    pub product: String,
    pub build: Option<u32>,
    pub rich: Option<u32>,
    pub libraries: Vec<Count>,
}

impl Bucket {
    #[must_use]
    pub fn label(&self) -> String {
        match (self.prodid, self.build) {
            (Some(p), Some(b)) => format!("{} {b}", prodid_name(p)),
            _ => self.product.clone(),
        }
    }

    #[must_use]
    pub fn linked(&self) -> usize {
        self.libraries
            .iter()
            .map(|c| c.linked)
            .fold(0, usize::saturating_add)
    }
}

pub fn join(
    rich: Option<&Rich>,
    libs: &[(&str, &Archive<'_>)],
    run: &Run,
    imported: &BTreeSet<(String, usize)>,
) -> Result<Vec<Bucket>> {
    let placed: BTreeSet<(&str, usize)> = run
        .members
        .iter()
        .filter(|p| !p.ranges.is_empty())
        .map(|p| (p.archive.as_str(), p.index))
        .collect();
    let mut by: BTreeMap<(u8, u32, u32), BTreeMap<String, Count>> = BTreeMap::new();
    for (lib, a) in libs {
        for r in members(a)? {
            let key = match (r.kind, r.prodid, r.build) {
                (MemberKind::Object, Some(p), Some(b)) => (0, p, b),
                (MemberKind::Object, _, _) => (1, 0, 0),
                (MemberKind::Import, _, _) => (2, 0, 0),
                _ => continue,
            };
            let c = by
                .entry(key)
                .or_default()
                .entry((*lib).to_string())
                .or_insert_with(|| Count {
                    library: (*lib).to_string(),
                    ..Count::default()
                });
            c.members = c.members.saturating_add(1);
            if r.kind == MemberKind::Object && r.code_bytes == 0 {
                c.codeless = c.codeless.saturating_add(1);
            }
            if placed.contains(&(*lib, r.index))
                || imported.contains(&((*lib).to_string(), r.index))
            {
                c.linked = c.linked.saturating_add(1);
            }
        }
    }
    Ok(by
        .into_iter()
        .map(|((kind, p, b), libs)| {
            let stamped = kind == 0;
            let rich_count = rich.map(|h| {
                h.stamps
                    .iter()
                    .filter(|s| match kind {
                        0 => s.prodid == p && s.build == b,
                        2 => s.prodid == 1,
                        _ => s.prodid == 0,
                    })
                    .map(|s| s.objects)
                    .fold(0u32, u32::saturating_add)
            });
            Bucket {
                prodid: stamped.then_some(p),
                product: match kind {
                    0 => prodid_name(p).to_string(),
                    1 => "unstamped".to_string(),
                    _ => "short import".to_string(),
                },
                build: stamped.then_some(b),
                rich: rich_count,
                libraries: libs.into_values().collect(),
            }
        })
        .collect())
}
