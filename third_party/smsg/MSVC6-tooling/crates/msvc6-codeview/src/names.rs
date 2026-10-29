use crate::decl::{BYTES_PER_MEMBER, MEMBERS_FLOOR};
use crate::decl::{enum_head, udt_head};
use crate::record::{Field, Record};
use anyhow::{Result, bail};
use serde::Serialize;
use std::collections::BTreeSet;

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum Name {
    Member {
        class: String,
        name: String,
    },
    Enumerator {
        r#enum: String,
        name: String,
    },
    Local {
        class: String,
        function: String,
        name: String,
    },
}

impl Name {
    #[must_use]
    pub fn line(&self) -> String {
        match self {
            Self::Member { class, name } => format!("member\t{class}\t{name}"),
            Self::Enumerator { r#enum, name } => format!("enumerator\t{enum}\t{name}"),
            Self::Local {
                class,
                function,
                name,
            } => format!("local\t{class}\t{function}\t{name}"),
        }
    }
}

pub fn attested(rec: &Record, modules: Option<&str>) -> Result<Vec<Name>> {
    let cap = (rec.bytes / BYTES_PER_MEMBER).max(MEMBERS_FLOOR);
    let mut spent = 0usize;
    let mut spend = |n: usize| -> Result<()> {
        spent = spent.saturating_add(n);
        if spent > cap {
            bail!(
                "the record names over {cap} members, more than its {} bytes hold: a malformed record",
                rec.bytes
            );
        }
        Ok(())
    };
    let mut members: BTreeSet<(String, String)> = BTreeSet::new();
    let mut enumerators: BTreeSet<(String, String)> = BTreeSet::new();
    let mut locals: BTreeSet<(String, String, String)> = BTreeSet::new();
    for (_, t) in rec.types() {
        match t.leaf {
            0x1004..=0x1006 => {
                let Some(h) = udt_head(t.leaf, t.data) else {
                    continue;
                };
                if h.prop & 0x80 != 0 {
                    continue;
                }
                let fields = rec.fields(h.field);
                spend(weight(&fields))?;
                for f in fields {
                    let n = match f {
                        Field::Member { name, .. }
                        | Field::Static { name, .. }
                        | Field::Method { name, .. }
                        | Field::OneMethod { name, .. } => name,
                        _ => continue,
                    };
                    if !n.is_empty() {
                        members.insert((h.name.clone(), n));
                    }
                }
            }
            0x1007 => {
                let Some((prop, _, field, name)) = enum_head(t.data) else {
                    continue;
                };
                if prop & 0x80 != 0 {
                    continue;
                }
                let fields = rec.fields(field);
                spend(weight(&fields))?;
                for f in fields {
                    if let Field::Enumerate { name: e, .. } = f {
                        enumerators.insert((name.clone(), e));
                    }
                }
            }
            _ => {}
        }
    }
    let statics = rec
        .globals
        .iter()
        .chain(rec.modules.iter().flat_map(|m| m.data.iter()));
    for d in statics {
        if let Some((c, n)) = d.name.rsplit_once("::") {
            members.insert((c.to_string(), n.to_string()));
        }
    }
    for m in &rec.modules {
        let ours = match modules {
            Some(pat) => m.objname.contains(pat),
            None => m.ilib == 0,
        };
        if !ours {
            continue;
        }
        for p in &m.procs {
            let Some((c, f)) = p.name.rsplit_once("::") else {
                continue;
            };
            let names = p
                .locals
                .iter()
                .map(|l| l.name.as_str())
                .chain(p.data.iter().map(|d| d.name.as_str()));
            spend(p.locals.len().saturating_add(p.data.len()))?;
            for n in names.filter(|n| *n != "this" && !n.is_empty()) {
                locals.insert((c.to_string(), f.to_string(), n.to_string()));
            }
        }
    }
    let mut out: Vec<Name> = members
        .into_iter()
        .map(|(class, name)| Name::Member { class, name })
        .collect();
    out.extend(
        enumerators
            .into_iter()
            .map(|(r#enum, name)| Name::Enumerator { r#enum, name }),
    );
    out.extend(
        locals
            .into_iter()
            .map(|(class, function, name)| Name::Local {
                class,
                function,
                name,
            }),
    );
    Ok(out)
}

fn weight(fields: &[Field]) -> usize {
    fields
        .iter()
        .map(|f| match f {
            Field::Method { overloads, .. } => overloads.len().max(1),
            _ => 1,
        })
        .fold(0, usize::saturating_add)
}
