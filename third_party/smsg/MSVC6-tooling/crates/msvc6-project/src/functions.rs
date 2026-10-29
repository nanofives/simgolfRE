use crate::config::{FunctionSource, Project};
use crate::registry::Resolved;
use crate::spans::{index, join};
use anyhow::Result;
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Function {
    pub rva: u32,
    pub end: Option<u32>,
    pub name: Option<String>,
    pub thunk: bool,
    pub provider: String,
    pub object: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ProviderStat {
    pub provider: String,
    pub source: String,
    pub records: usize,
    pub new: usize,
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Default)]
pub struct FunctionIndex {
    by_rva: BTreeMap<u32, Function>,
    pub stats: Vec<ProviderStat>,
}

impl FunctionIndex {
    pub fn add(&mut self, provider: &str, source: &str, fs: Vec<Function>, notes: Vec<String>) {
        let records = fs.len();
        let mut new = 0usize;
        for f in fs {
            match self.by_rva.get_mut(&f.rva) {
                Some(have) => {
                    if have.end.is_none() {
                        have.end = f.end;
                        have.thunk |= f.thunk;
                    }
                    if have.name.is_none() {
                        have.name = f.name;
                    }
                    if have.object.is_none() {
                        have.object = f.object;
                    }
                }
                None => {
                    new = new.saturating_add(1);
                    self.by_rva.insert(f.rva, f);
                }
            }
        }
        self.stats.push(ProviderStat {
            provider: provider.to_string(),
            source: source.to_string(),
            records,
            new,
            notes,
        });
    }

    #[must_use]
    pub fn get(&self, rva: u32) -> Option<&Function> {
        self.by_rva.get(&rva)
    }

    #[must_use]
    pub fn containing(&self, rva: u32) -> Option<&Function> {
        self.by_rva
            .range(..=rva)
            .next_back()
            .map(|(_, f)| f)
            .filter(|f| f.end.is_some_and(|e| rva < e) || f.rva == rva)
    }

    pub fn iter(&self) -> impl Iterator<Item = &Function> {
        self.by_rva.values()
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.by_rva.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.by_rva.is_empty()
    }
}

pub fn read_one(
    p: &Project,
    n: usize,
    i: usize,
    src: &FunctionSource,
    img: &Image,
) -> Result<(String, String, Vec<Function>, Vec<String>)> {
    let d = p
        .images
        .get(n)
        .ok_or_else(|| anyhow::anyhow!("no image[{n}]"))?;
    let key = index(&join(&p.image_key(n), "functions"), i);
    let base = img.opt.image_base.get();
    let fail = |e: anyhow::Error| p.error(&key, format!("{e:#}"));
    Ok(match src {
        FunctionSource::GhidraJson(f) => {
            let path = p.path_at(&key, f)?;
            let text = msvc6_base::srcio::own_read(&path)
                .map_err(|e| fail(anyhow::anyhow!("reading {}: {e}", path.display())))?;
            let (fs, s) = crate::ghidra::parse(&path, &text, base).map_err(fail)?;
            let mut notes = vec![format!("{} thunk(s)", s.thunks)];
            if s.contaminated > 0 {
                notes.push(format!(
                    "{} record(s) hold instructions below their entry (a followed tail jump), keyed at their own entry",
                    s.contaminated
                ));
            }
            if s.auto_names > 0 {
                notes.push(format!("{} generated name(s) dropped", s.auto_names));
            }
            ("ghidra-json".into(), path.display().to_string(), fs, notes)
        }
        FunctionSource::Map(f) => {
            let raw = match f.as_ref().or(d.map.as_ref()) {
                Some(r) => r.clone(),
                None => {
                    return Err(p.error(&key, "`map` names no file and the image has no `map`"));
                }
            };
            let path = p.path_at(&key, &raw)?;
            let m = crate::mapfile::Map::read(&path).map_err(fail)?;
            let mut notes = Vec::new();
            if m.base != base {
                notes.push(format!(
                    "the map's load address {:#x} is not the image's {base:#x}; rvas are the map's",
                    m.base
                ));
            }
            let fs = m
                .symbols
                .iter()
                .filter(|s| s.function)
                .map(|s| Function {
                    rva: s.rva,
                    end: None,
                    name: Some(s.name.clone()),
                    thunk: false,
                    provider: "map".into(),
                    object: Some(match &s.lib {
                        Some(l) => format!("{l}:{}", s.object),
                        None => s.object.clone(),
                    }),
                })
                .collect();
            ("map".into(), path.display().to_string(), fs, notes)
        }
        FunctionSource::ReccmpCsv(f) => {
            let path = p.path_at(&key, f)?;
            let rows = crate::reccmp::read_csv(&path).map_err(fail)?;
            let mut below = 0usize;
            let fs = rows
                .iter()
                .filter(|r| crate::reccmp::FUNCTION_TYPES.contains(&r.kind.as_str()))
                .filter_map(|r| {
                    let rva = r.va.checked_sub(base);
                    if rva.is_none() {
                        below = below.saturating_add(1);
                    }
                    Some(Function {
                        rva: rva?,
                        end: r.size.and_then(|s| rva?.checked_add(s)),
                        name: r.name.clone(),
                        thunk: r.kind == "thunk",
                        provider: "reccmp-csv".into(),
                        object: None,
                    })
                })
                .collect();
            let notes = if below > 0 {
                vec![format!("{below} row(s) below the image base skipped")]
            } else {
                Vec::new()
            };
            ("reccmp-csv".into(), path.display().to_string(), fs, notes)
        }
        FunctionSource::Pdb(f) => {
            let path = pdb_path(p, n, &key, f.as_ref())?;
            let bytes = std::fs::read(&path)
                .map_err(|e| fail(anyhow::anyhow!("reading {}: {e}", path.display())))?;
            let l = msvc6_pdb::procs::read(&bytes)
                .map_err(|e| fail(e.context(path.display().to_string())))?;
            let retired = l
                .procs
                .iter()
                .chain(&l.thunks)
                .filter(|q| q.rva.is_none())
                .count();
            let fs = l
                .procs
                .iter()
                .chain(&l.thunks)
                .filter_map(|q| {
                    let rva = q.rva?;
                    Some(Function {
                        rva,
                        end: rva.checked_add(q.len),
                        name: Some(q.name.clone()),
                        thunk: q.scope == "thunk",
                        provider: "pdb".into(),
                        object: Some(q.module.clone()),
                    })
                })
                .collect();
            let mut notes = vec![format!(
                "{} procedure(s), {} thunk(s), resolved through the PDB's own section table",
                l.procs.len(),
                l.thunks.len()
            )];
            if retired > 0 {
                notes.push(format!(
                    "{retired} record(s) at segment 0 (retired) skipped"
                ));
            }
            ("pdb".into(), path.display().to_string(), fs, notes)
        }
        FunctionSource::Discover => {
            let fs = crate::discover::discover(img);
            let notes = vec![format!(
                "{} thunk(s); found from the entry, exports, call targets, prologues and code pointers",
                fs.iter().filter(|f| f.thunk).count()
            )];
            ("discover".into(), "the image".into(), fs, notes)
        }
    })
}

pub fn pdb_path(
    p: &Project,
    n: usize,
    key: &str,
    given: Option<&String>,
) -> Result<std::path::PathBuf> {
    let raw = match given {
        Some(f) => f.clone(),
        None => p
            .images
            .get(n)
            .and_then(|d| d.pdb.as_ref())
            .and_then(|s| s.file.clone())
            .ok_or_else(|| {
                p.error(
                    key,
                    "`pdb` names no file and the image's [image.pdb] has no `file`",
                )
            })?,
    };
    p.path_at(key, &raw)
}

pub fn index_of(p: &Project, n: usize, r: &Resolved) -> Result<FunctionIndex> {
    let img = Image::parse(r.bytes.clone())?;
    let d = p
        .images
        .get(n)
        .ok_or_else(|| anyhow::anyhow!("no image[{n}]"))?;
    let mut ix = FunctionIndex::default();
    for (i, s) in d.functions.iter().enumerate() {
        if let Some((raw, v)) = s.input().and_then(|raw| p.absent(raw).map(|v| (raw, v))) {
            ix.add(
                s.kind(),
                raw,
                Vec::new(),
                vec![crate::config::absent_note(&v)],
            );
            continue;
        }
        let (kind, src, fs, notes) = read_one(p, n, i, s, &img)?;
        ix.add(&kind, &src, fs, notes);
    }
    Ok(ix)
}
