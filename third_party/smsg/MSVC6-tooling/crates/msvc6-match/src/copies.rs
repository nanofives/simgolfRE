use anyhow::{Context as _, Result};
use msvc6_base::digest::sha256_hex;
use msvc6_image::archive::Archive;
use msvc6_image::coff::Object;
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::PathBuf;

use crate::code;
use crate::locate::{Lib, Recorded, Run, locate, objects};
use crate::place::Text;

#[derive(Debug, Clone)]
pub struct Offered {
    pub label: String,
    pub path: PathBuf,
    pub bytes: Vec<u8>,
}

impl Offered {
    #[must_use]
    pub fn file_name(&self) -> String {
        self.path
            .file_name()
            .map_or_else(|| self.label.clone(), |n| n.to_string_lossy().into_owned())
            .to_ascii_lowercase()
    }

    #[must_use]
    pub fn library(&self) -> String {
        let f = self.file_name();
        match f.rsplit_once('.') {
            Some((stem, _)) if !stem.is_empty() => stem.to_string(),
            _ => f,
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct Copy {
    pub label: String,
    pub path: String,
    pub sha256: String,
    pub same_as: Option<String>,
    pub considered: usize,
    pub present: usize,
    pub covered: u32,
}

#[derive(Debug, Clone, Serialize)]
pub struct Other {
    pub label: String,
    pub len: Option<u32>,
    pub placed: Option<u32>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Witness {
    pub member: String,
    pub symbol: String,
    pub rva: u32,
    pub len: u32,
    pub others: Vec<Other>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Group {
    pub library: String,
    pub file: String,
    pub chosen: usize,
    pub tie: bool,
    pub copies: Vec<Copy>,
    pub witnesses: Vec<Witness>,
}

impl Group {
    #[must_use]
    pub fn chosen_copy(&self) -> Option<&Copy> {
        self.copies.get(self.chosen)
    }
}

type Key = (String, String);
type Kept = BTreeMap<Key, (u32, u32)>;
type Lengths = BTreeMap<Key, u32>;

fn kept_sections(run: &Run) -> Kept {
    let mut out = BTreeMap::new();
    for p in &run.members {
        for s in &p.sections {
            let end = s.rva.saturating_add(s.len);
            if !p.ranges.contains(&(s.rva, end)) {
                continue;
            }
            let Some(sym) = &s.symbol else { continue };
            out.entry((p.stem().to_ascii_lowercase(), sym.clone()))
                .or_insert((s.rva, s.len));
        }
    }
    out
}

fn lengths(a: &Archive<'_>) -> Lengths {
    let mut out = BTreeMap::new();
    for (_, name, d) in objects(a) {
        let Ok(o) = Object::parse(d) else { continue };
        let Ok(codes) = code::sections(&o) else {
            continue;
        };
        let stem = name
            .rsplit(['\\', '/'])
            .next()
            .unwrap_or(name)
            .to_ascii_lowercase();
        for c in codes {
            let len = c.len32();
            if let Some(s) = c.symbol {
                out.entry((stem.clone(), s)).or_insert(len);
            }
        }
    }
    out
}

#[derive(Debug)]
pub struct Choice {
    pub groups: Vec<Group>,
    pub picks: Vec<usize>,
}

pub fn choose(
    t: &Text<'_>,
    offered: &[Offered],
    archives: &[Archive<'_>],
    record: &[Recorded],
    jobs: usize,
) -> Result<Choice> {
    let mut order: Vec<String> = Vec::new();
    let mut by: BTreeMap<String, Vec<usize>> = BTreeMap::new();
    for (i, o) in offered.iter().enumerate() {
        let f = o.file_name();
        if !by.contains_key(&f) {
            order.push(f.clone());
        }
        by.entry(f).or_default().push(i);
    }
    let mut groups = Vec::new();
    let mut picks = Vec::new();
    for f in order {
        let members = by.get(&f).cloned().unwrap_or_default();
        let mut copies: Vec<Copy> = Vec::new();
        let mut distinct: Vec<usize> = Vec::new();
        let mut first_of: BTreeMap<String, usize> = BTreeMap::new();
        for &i in &members {
            let (Some(o), Some(_)) = (offered.get(i), archives.get(i)) else {
                continue;
            };
            let sha = sha256_hex(&o.bytes);
            let same_as = first_of
                .get(&sha)
                .and_then(|k| offered.get(*k))
                .map(|x| x.label.clone());
            if same_as.is_none() {
                first_of.insert(sha.clone(), i);
                distinct.push(i);
            }
            copies.push(Copy {
                label: o.label.clone(),
                path: o.path.display().to_string(),
                sha256: sha,
                same_as,
                considered: 0,
                present: 0,
                covered: 0,
            });
        }
        let library = offered
            .get(members.first().copied().unwrap_or(0))
            .map(Offered::library)
            .unwrap_or_default();
        let mut runs: BTreeMap<usize, Run> = BTreeMap::new();
        if distinct.len() > 1 {
            for &i in &distinct {
                let Some(a) = archives.get(i) else { continue };
                let run = locate(
                    t,
                    &[Lib {
                        name: &library,
                        archive: a,
                    }],
                    record,
                    jobs,
                )
                .with_context(|| {
                    offered
                        .get(i)
                        .map_or_else(String::new, |o| o.path.display().to_string())
                })?;
                runs.insert(i, run);
            }
        }
        for (k, &i) in members.iter().enumerate() {
            let Some(run) = runs.get(&i) else { continue };
            if let Some(c) = copies.get_mut(k) {
                c.considered = run.considered;
                c.present = run.present().len();
                c.covered = run.covered();
            }
        }
        let score = |i: usize| {
            runs.get(&i)
                .map_or((0, 0), |r| (r.covered(), r.present().len()))
        };
        let best = distinct
            .iter()
            .copied()
            .max_by(|a, b| score(*a).cmp(&score(*b)).then(b.cmp(a)))
            .or_else(|| members.first().copied())
            .unwrap_or(0);
        let tie = distinct
            .iter()
            .filter(|i| **i != best && score(**i) == score(best))
            .count()
            > 0;
        let chosen = members.iter().position(|i| *i == best).unwrap_or(0);
        let mut witnesses = Vec::new();
        if let Some(run) = runs.get(&best) {
            let mine = kept_sections(run);
            let others: Vec<(usize, Kept, Lengths)> = distinct
                .iter()
                .filter(|i| **i != best)
                .filter_map(|&i| {
                    let r = runs.get(&i)?;
                    let a = archives.get(i)?;
                    Some((i, kept_sections(r), lengths(a)))
                })
                .collect();
            for ((stem, sym), (rva, len)) in &mine {
                let key = (stem.clone(), sym.clone());
                let same_everywhere = others
                    .iter()
                    .all(|(_, kept, _)| kept.get(&key) == Some(&(*rva, *len)));
                if same_everywhere {
                    continue;
                }
                witnesses.push(Witness {
                    member: stem.clone(),
                    symbol: sym.clone(),
                    rva: *rva,
                    len: *len,
                    others: others
                        .iter()
                        .map(|(i, kept, lens)| Other {
                            label: offered
                                .get(*i)
                                .map_or_else(String::new, |o| o.label.clone()),
                            len: lens.get(&key).copied(),
                            placed: kept.get(&key).map(|(a, _)| *a),
                        })
                        .collect(),
                });
            }
            witnesses.sort_by(|a, b| {
                b.len
                    .cmp(&a.len)
                    .then(a.rva.cmp(&b.rva))
                    .then_with(|| a.member.cmp(&b.member))
            });
        }
        picks.push(best);
        groups.push(Group {
            library,
            file: f,
            chosen,
            tie,
            copies,
            witnesses,
        });
    }
    Ok(Choice { groups, picks })
}
