use msvc6_image::archive::{Archive, MemberKind};
use msvc6_image::coff::{ImportObject, Object};
use msvc6_image::pe::dirs::ImportDll;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

#[must_use]
pub fn normalise(sym: &str) -> String {
    let s = sym
        .strip_prefix("__imp__")
        .or_else(|| sym.strip_prefix('_'))
        .unwrap_or(sym);
    match s.rsplit_once('@') {
        Some((head, tail)) if !tail.is_empty() && tail.bytes().all(|b| b.is_ascii_digit()) => {
            head.to_string()
        }
        _ => s.to_string(),
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Supply {
    pub dll: String,
    pub name: String,
    pub members: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Multi {
    pub member: String,
    pub names: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Join {
    pub named: usize,
    pub by_ordinal: usize,
    pub members_read: usize,
    pub short: usize,
    pub objects: usize,
    pub supplied: usize,
    pub rows: Vec<Supply>,
    pub multi: Vec<Multi>,
    pub unsupplied: Vec<String>,
    pub descriptors: usize,
    #[serde(skip)]
    pub linked: BTreeSet<(String, usize)>,
}

const DESCRIPTOR: &str = "__IMPORT_DESCRIPTOR_";
const NULL_THUNK: &str = "_NULL_THUNK_DATA";
const NULL_DESCRIPTOR: &str = "__NULL_IMPORT_DESCRIPTOR";

fn descriptors(lib: &str, a: &Archive<'_>, null_taken: &mut bool, j: &mut Join) {
    for (m, d) in a.content() {
        if m.kind != MemberKind::Object {
            continue;
        }
        let Ok(o) = Object::parse(d) else { continue };
        let defs = o.defined_externals();
        let own = defs
            .iter()
            .any(|s| s.starts_with(DESCRIPTOR) || s.ends_with(NULL_THUNK));
        let null = !*null_taken && defs.contains(NULL_DESCRIPTOR);
        if own || null {
            *null_taken |= null;
            j.descriptors = j.descriptors.saturating_add(1);
            j.linked.insert((lib.to_string(), m.index));
        }
    }
}

#[must_use]
pub fn has_imports(a: &Archive<'_>) -> bool {
    a.members.iter().any(|m| m.kind == MemberKind::Import)
        || a.objects().iter().any(|(_, o)| {
            o.defined_externals()
                .iter()
                .any(|s| s.starts_with("__imp_"))
        })
}

#[must_use]
pub fn join(imports: &[ImportDll], libs: &[(&str, &Archive<'_>)]) -> Join {
    let mut wanted: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut j = Join::default();
    for d in imports {
        for e in &d.entries {
            if e.by_ordinal() {
                j.by_ordinal = j.by_ordinal.saturating_add(1);
            } else {
                wanted
                    .entry(e.name.clone())
                    .or_default()
                    .insert(d.dll.to_ascii_lowercase());
            }
        }
    }
    j.named = wanted.len();
    let mut supplies: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut by_name: BTreeMap<String, Vec<String>> = BTreeMap::new();
    for (lib, a) in libs {
        if !has_imports(a) {
            continue;
        }
        for (m, d) in a.content() {
            j.members_read = j.members_read.saturating_add(1);
            let hit: BTreeSet<String> = match m.kind {
                MemberKind::Import => {
                    j.short = j.short.saturating_add(1);
                    ImportObject::parse(d)
                        .map(|i| {
                            let n = normalise(&i.symbol);
                            let dll = i.dll.to_ascii_lowercase();
                            wanted.get(&n).filter(|dlls| dlls.contains(&dll)).map(|_| n)
                        })
                        .into_iter()
                        .flatten()
                        .collect()
                }
                MemberKind::Object => {
                    j.objects = j.objects.saturating_add(1);
                    Object::parse(d)
                        .map(|o| {
                            o.defined_externals()
                                .iter()
                                .map(|s| normalise(s))
                                .filter(|n| wanted.contains_key(n))
                                .collect()
                        })
                        .unwrap_or_default()
                }
                _ => BTreeSet::new(),
            };
            if hit.is_empty() {
                continue;
            }
            j.linked.insert(((*lib).to_string(), m.index));
            let key = format!("{lib}[{}] {}", m.index, m.name);
            for n in &hit {
                by_name.entry(n.clone()).or_default().push(key.clone());
            }
            supplies.entry(key).or_default().extend(hit);
        }
    }
    for (name, dlls) in &wanted {
        let members = by_name.get(name).cloned().unwrap_or_default();
        if members.is_empty() {
            j.unsupplied.push(name.clone());
        } else {
            j.supplied = j.supplied.saturating_add(1);
        }
        j.rows.push(Supply {
            dll: dlls.iter().cloned().collect::<Vec<_>>().join(","),
            name: name.clone(),
            members,
        });
    }
    let supplying: BTreeSet<String> = j.linked.iter().map(|(l, _)| l.clone()).collect();
    let mut null_taken = false;
    for (lib, a) in libs {
        if supplying.contains(*lib) {
            descriptors(lib, a, &mut null_taken, &mut j);
        }
    }
    j.multi = supplies
        .into_iter()
        .filter(|(_, v)| v.len() > 1)
        .map(|(member, v)| Multi {
            member,
            names: v.into_iter().collect(),
        })
        .collect();
    j
}

#[cfg(test)]
mod tests {
    use super::normalise;

    #[test]
    fn one_prefix_and_one_stdcall_suffix_come_off() {
        assert_eq!(normalise("__imp__ChoosePixelFormat@8"), "ChoosePixelFormat");
        assert_eq!(normalise("_ChoosePixelFormat@8"), "ChoosePixelFormat");
        assert_eq!(normalise("ChoosePixelFormat"), "ChoosePixelFormat");
        assert_eq!(normalise("?f@S@@QAEHXZ"), "?f@S@@QAEHXZ");
        assert_eq!(normalise("_foo@bar"), "foo@bar");
        assert_eq!(normalise("__fltused"), "_fltused");
        assert_ne!(normalise("__foo"), normalise("_foo"));
    }
}
