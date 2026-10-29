use anyhow::{Context as _, Result};
use msvc6_base::units::Rva;
use msvc6_image::coff::Object;
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const REL_DIR32: u16 = 6;
pub const REL_DIR32NB: u16 = 7;
pub const MAX_OBJECT: u64 = 256 << 20;
pub const MAX_DEPTH: usize = 16;
pub const MAX_OBJECTS: usize = 1 << 16;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Row {
    pub ours: u32,
    pub refr: u32,
    pub object: String,
    pub section: String,
    pub name: String,
    pub compared: usize,
    pub differ: usize,
    pub skipped: usize,
    pub verdict: &'static str,
}

pub type Defs = BTreeMap<String, (String, String, Vec<u8>, BTreeSet<usize>)>;

pub fn defs_of(file: &str, data: &[u8], out: &mut Defs) -> Result<()> {
    let o = Object::parse(data)?;
    for (si, sec) in o.sections.iter().enumerate() {
        if !(sec.name.starts_with(".data") || sec.name.starts_with(".rdata")) || sec.size == 0 {
            continue;
        }
        let raw = o.section_data(sec);
        let relocated: BTreeSet<usize> = o
            .relocations(sec)
            .iter()
            .filter(|r| r.typ == REL_DIR32 || r.typ == REL_DIR32NB)
            .filter_map(|r| usize::try_from(r.va).ok())
            .flat_map(|at| at..at.saturating_add(4))
            .collect();
        let mut syms: Vec<(u32, &str)> = o
            .symbols
            .iter()
            .filter(|y| usize::try_from(y.section).ok() == Some(si + 1))
            .filter(|y| !y.name.starts_with('.') && !y.name.is_empty())
            .map(|y| (y.value, y.name.as_str()))
            .collect();
        syms.sort_unstable();
        syms.dedup();
        for (k, (at, name)) in syms.iter().enumerate() {
            let end = syms
                .get(k + 1)
                .map_or(raw.len(), |(n, _)| usize::try_from(*n).unwrap_or(raw.len()));
            let from = usize::try_from(*at).unwrap_or(0);
            let Some(b) = raw.get(from..end.min(raw.len())) else {
                continue;
            };
            if b.is_empty() {
                continue;
            }
            let rel: BTreeSet<usize> = relocated
                .range(from..end.max(from))
                .map(|r| r - from)
                .collect();

            out.entry((*name).to_string()).or_insert((
                file.to_string(),
                sec.name.clone(),
                b.to_vec(),
                rel,
            ));
        }
    }
    Ok(())
}

pub fn objects(dir: &Path) -> Result<Vec<PathBuf>> {
    let mut out = Vec::new();
    let mut stack = vec![(dir.to_path_buf(), 0usize)];
    while let Some((d, depth)) = stack.pop() {
        let Ok(rd) = std::fs::read_dir(&d) else {
            continue;
        };
        for e in rd.flatten() {
            let Ok(t) = e.file_type() else { continue };
            let p = e.path();
            if t.is_dir() {
                if depth < MAX_DEPTH {
                    stack.push((p, depth + 1));
                }
            } else if t.is_file() && p.extension().is_some_and(|x| x.eq_ignore_ascii_case("obj")) {
                out.push(p);
                if out.len() > MAX_OBJECTS {
                    anyhow::bail!(
                        "{}: more than {MAX_OBJECTS} objects under it",
                        dir.display()
                    );
                }
            }
        }
    }
    out.sort();
    Ok(out)
}

pub fn collect(dirs: &[PathBuf]) -> Result<Defs> {
    let mut defs: Defs = BTreeMap::new();
    for d in dirs {
        let mut here: Defs = BTreeMap::new();
        for p in objects(d)? {
            let data = msvc6_base::srcio::read_bounded(&p, MAX_OBJECT)
                .with_context(|| format!("reading {}", p.display()))?;
            let file = p
                .file_name()
                .and_then(|s| s.to_str())
                .unwrap_or_default()
                .to_string();
            if defs_of(&file, &data, &mut here).is_err() {
                continue;
            }
        }
        for (k, v) in here {
            defs.entry(k).or_insert(v);
        }
    }
    Ok(defs)
}

#[derive(Debug, Default)]
pub struct Sampled {
    pub rows: Vec<Row>,
    pub pool: usize,
    pub with_bytes: usize,
    pub step: usize,
}

#[must_use]
pub fn run(
    reference: &Image,
    defs: &Defs,
    pairs: &BTreeMap<u32, (u32, Vec<String>)>,
    want: usize,
) -> Sampled {
    let mut candidates: Vec<(u32, u32, &String)> = pairs
        .iter()
        .filter_map(|(o, (r, names))| {
            names
                .iter()
                .find(|n| defs.contains_key(*n))
                .map(|n| (*o, *r, n))
        })
        .collect();
    if candidates.is_empty() || want == 0 {
        return Sampled::default();
    }
    let bytes_to_compare = |n: &String| {
        defs.get(n)
            .is_some_and(|(_, _, b, rel)| (0..b.len()).any(|k| !rel.contains(&k)))
    };
    candidates.sort_by_key(|(o, _, n)| (!bytes_to_compare(n), *o));
    let n = candidates
        .iter()
        .filter(|(_, _, n)| bytes_to_compare(n))
        .count();
    let pool = if n >= want { n } else { candidates.len() };
    let step = (pool / want).max(1);
    let mut out = Sampled {
        pool: candidates.len(),
        with_bytes: n,
        step,
        rows: Vec::new(),
    };
    for (ours, refr, name) in candidates.iter().take(pool).step_by(step).take(want) {
        let Some((object, section, bytes, rel)) = defs.get(*name) else {
            continue;
        };
        let theirs = reference
            .at_rva(Rva(*refr), bytes.len())
            .map(<[u8]>::to_vec)
            .unwrap_or_default();
        let mut compared = 0usize;
        let mut differ = 0usize;
        for (k, b) in bytes.iter().enumerate() {
            if rel.contains(&k) {
                continue;
            }
            compared += 1;
            if theirs.get(k).copied().unwrap_or(0) != *b {
                differ += 1;
            }
        }
        let verdict = if compared == 0 {
            "RELOCATED-ONLY"
        } else if differ == 0 {
            "IDENTICAL"
        } else {
            "DIFFERS"
        };
        out.rows.push(Row {
            ours: *ours,
            refr: *refr,
            object: object.clone(),
            section: section.clone(),
            name: (*name).clone(),
            compared,
            differ,
            skipped: rel.len(),
            verdict,
        });
    }
    out
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;

    #[test]
    fn the_object_walk_follows_no_symlink_and_stops_at_its_depth() {
        let d = std::env::temp_dir().join(format!("msvc6-pdb-sample-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        let elsewhere = d.join("elsewhere");
        std::fs::create_dir_all(elsewhere.join("deep")).unwrap();
        let objs = d.join("objs");
        let mut deep = objs.clone();
        for k in 0..=MAX_DEPTH {
            deep.push(format!("d{k}"));
        }
        std::fs::create_dir_all(&deep).unwrap();
        for p in [
            objs.join("a.obj"),
            objs.join("B.OBJ"),
            objs.join("notes.txt"),
            elsewhere.join("x.obj"),
            elsewhere.join("deep").join("y.obj"),
            deep.join("too-deep.obj"),
        ] {
            msvc6_base::srcio::write_bytes(&p, b"x").unwrap();
        }
        std::os::unix::fs::symlink(elsewhere.join("x.obj"), objs.join("link.obj")).unwrap();
        std::os::unix::fs::symlink(&elsewhere, objs.join("linked")).unwrap();
        let names: Vec<String> = objects(&objs)
            .unwrap()
            .iter()
            .map(|p| p.strip_prefix(&objs).unwrap().display().to_string())
            .collect();
        assert_eq!(names, ["B.OBJ", "a.obj"]);
        std::fs::remove_dir_all(&d).unwrap();
    }
}
