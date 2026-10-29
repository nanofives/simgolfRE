use anyhow::{Result, bail};
use serde::Serialize;
use std::collections::BTreeMap;

use crate::archive::{Archive, MemberKind};
use crate::coff::{ImportObject, Object};
use crate::rich::prodid_name;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct MemberRow {
    pub index: usize,
    pub offset: usize,
    pub member: String,
    pub kind: MemberKind,
    pub prodid: Option<u32>,
    pub product: Option<&'static str>,
    pub build: Option<u32>,
    pub code_bytes: u64,
    pub sections: Vec<(String, u32)>,
    pub defines: Vec<String>,
    pub import: Option<String>,
}

impl MemberRow {
    #[must_use]
    pub fn stamp(&self) -> String {
        match (self.kind, self.prodid, self.build) {
            (MemberKind::Import, _, _) => "short import (Rich: Import)".to_string(),
            (_, Some(p), Some(b)) => format!("prodid {p:#04x} {} build {b}", prodid_name(p)),
            _ => "unstamped".to_string(),
        }
    }

    #[must_use]
    pub fn bucket(&self) -> Bucket {
        match self.kind {
            MemberKind::Import => Bucket::Import,
            MemberKind::Object => self.prodid.map_or(Bucket::Unstamped, Bucket::Prodid),
            _ => Bucket::Other,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum Bucket {
    Prodid(u32),
    Unstamped,
    Import,
    Other,
}

impl Bucket {
    #[must_use]
    pub fn label(self) -> String {
        match self {
            Self::Prodid(p) => format!("prodid {p:#04x} {}", prodid_name(p)),
            Self::Unstamped => "unstamped: no @comp.id".to_string(),
            Self::Import => "short import members".to_string(),
            Self::Other => "not a COFF object".to_string(),
        }
    }
}

pub fn members(a: &Archive<'_>) -> Result<Vec<MemberRow>> {
    let mut out = Vec::new();
    for (m, d) in a.content() {
        let mut row = MemberRow {
            index: m.index,
            offset: m.header_off,
            member: m.stem().to_string(),
            kind: m.kind,
            prodid: None,
            product: None,
            build: None,
            code_bytes: 0,
            sections: Vec::new(),
            defines: Vec::new(),
            import: None,
        };
        match m.kind {
            MemberKind::Import => {
                if let Some(i) = ImportObject::parse(d) {
                    row.import = Some(format!(
                        "{} from {}, name type {}, hint {}",
                        i.symbol,
                        i.dll,
                        i.name_type_name(),
                        i.hint
                    ));
                    row.defines.push(i.symbol);
                }
            }
            MemberKind::Object => {
                if let Ok(o) = Object::parse(d) {
                    if let Some(c) = o.comp_id() {
                        row.prodid = Some(c >> 16);
                        row.product = Some(prodid_name(c >> 16));
                        row.build = Some(c & 0xffff);
                    }
                    row.code_bytes = o.code_bytes();
                    row.sections = o
                        .sections
                        .iter()
                        .map(|s| (s.name.clone(), s.size))
                        .collect();
                    row.defines = o.defined_externals().into_iter().collect();
                }
            }
            _ => {}
        }
        out.push(row);
    }
    if out.is_empty() {
        bail!("the archive holds no member");
    }
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct BucketRow {
    pub bucket: Bucket,
    pub label: String,
    pub members: Vec<String>,
    pub codeless: Vec<String>,
}

#[must_use]
pub fn buckets(rows: &[MemberRow]) -> Vec<BucketRow> {
    let mut by: BTreeMap<Bucket, (Vec<String>, Vec<String>)> = BTreeMap::new();
    for r in rows {
        let e = by.entry(r.bucket()).or_default();
        e.0.push(r.member.clone());
        if r.kind == MemberKind::Object && r.code_bytes == 0 {
            e.1.push(r.member.clone());
        }
    }
    by.into_iter()
        .map(|(bucket, (mut members, mut codeless))| {
            members.sort();
            members.dedup();
            codeless.sort();
            codeless.dedup();
            BucketRow {
                bucket,
                label: bucket.label(),
                members,
                codeless,
            }
        })
        .collect()
}
