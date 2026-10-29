use crate::nb::{self, Nb, SST_MODULE};
use anyhow::{Context as _, Result};
use msvc6_image::pe::Image;
use msvc6_image::pe::dirs::debug_kind;
use serde::Serialize;
use std::collections::BTreeMap;
use std::io::Read as _;
use std::path::{Path, PathBuf};

pub const WALK_DEPTH: usize = 64;
pub const WALK_FILES: usize = 1_000_000;
pub const MAX_IMAGE: u64 = 1 << 30;
pub const PDB20_MAGIC: &[u8] = b"Microsoft C/C++ program database 2.00\r\n\x1aJG\0";
pub const MSF70_MAGIC: &[u8] = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0";
pub const DBG_MAGIC: &[u8] = b"DI";

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Entry {
    pub path: String,
    pub kind: &'static str,
    pub size: u64,
    pub format: String,
    pub embedded: bool,
    pub found: Option<nb::Found>,
    pub record_bytes: Option<usize>,
    pub subsections: Option<usize>,
    pub modules: Option<usize>,
    pub pdb: Option<String>,
    pub signature: Option<u32>,
    pub age: Option<u32>,
    pub guid: Option<String>,
    pub debug: Vec<&'static str>,
    pub timestamp: Option<u32>,
    pub dll: Option<bool>,
    pub problem: Option<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Scan {
    pub entries: Vec<Entry>,
    pub files: usize,
    pub skipped_links: Vec<String>,
    pub unreadable: Vec<String>,
    pub duplicates: Vec<String>,
    #[serde(skip)]
    seen: BTreeMap<Id, String>,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum Id {
    #[cfg(unix)]
    Inode(u64, u64),
    #[cfg(not(unix))]
    Path(PathBuf),
}

#[cfg(unix)]
fn id_of(_path: &Path, meta: &std::fs::Metadata) -> Id {
    use std::os::unix::fs::MetadataExt as _;
    Id::Inode(meta.dev(), meta.ino())
}

#[cfg(not(unix))]
fn id_of(path: &Path, _meta: &std::fs::Metadata) -> Id {
    Id::Path(std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf()))
}

impl Scan {
    fn first_visit(&mut self, path: &Path, shown: &str) -> bool {
        let Ok(meta) = std::fs::metadata(path) else {
            return true;
        };
        let id = id_of(path, &meta);
        match self.seen.get(&id) {
            Some(first) => {
                self.duplicates
                    .push(format!("{shown} is {first}, already read"));
                false
            }
            None => {
                self.seen.insert(id, shown.to_string());
                true
            }
        }
    }
}

fn magic(path: &Path) -> std::io::Result<Vec<u8>> {
    let mut f = std::fs::File::open(path)?;
    let mut head = vec![0u8; MSF70_MAGIC.len().max(PDB20_MAGIC.len())];
    let mut n = 0usize;
    while n < head.len() {
        let got = f.read(head.get_mut(n..).unwrap_or_default())?;
        if got == 0 {
            break;
        }
        n = n.saturating_add(got);
    }
    head.truncate(n);
    Ok(head)
}

#[must_use]
pub fn probe_pe(path: &str, bytes: Vec<u8>) -> Entry {
    let size = u64::try_from(bytes.len()).unwrap_or(u64::MAX);
    let mut e = Entry {
        path: path.to_string(),
        kind: "pe",
        size,
        format: "none".into(),
        ..Entry::default()
    };
    let img = match Image::parse(bytes) {
        Ok(i) => i,
        Err(err) => {
            e.kind = "mz";
            e.format = "-".into();
            e.problem = Some(format!("{err:#}"));
            return e;
        }
    };
    e.timestamp = Some(img.timestamp);
    e.dll = Some(img.is_dll());
    e.debug = img
        .debug_entries()
        .iter()
        .map(|d| debug_kind(d.kind))
        .collect();
    match img.codeview() {
        Ok(Some(cv)) => {
            e.format.clone_from(&cv.format);
            if !nb::is_embedded(&cv.format) {
                e.pdb = Some(cv.path).filter(|p| !p.is_empty());
                e.age = Some(cv.age);
                if cv.format == "RSDS" {
                    e.guid = cv.guid;
                } else {
                    e.signature = Some(cv.signature);
                }
            }
        }
        Ok(None) => {}
        Err(err) => e.problem = Some(format!("{err:#}")),
    }
    match nb::locate(&img) {
        Ok(Some(l)) => {
            e.format.clone_from(&l.signature);
            e.embedded = true;
            e.found = Some(l.found);
            e.record_bytes = Some(l.len);
            let rec = img
                .bytes()
                .get(l.offset..l.offset.saturating_add(l.len))
                .unwrap_or_default();
            match Nb::parse(rec) {
                Ok(n) => {
                    e.subsections = Some(n.entries.len());
                    e.modules = Some(n.entries.iter().filter(|x| x.sst == SST_MODULE).count());
                }
                Err(err) => e.problem = Some(format!("{err:#}")),
            }
        }
        Ok(None) => {}
        Err(err) => e.problem = Some(format!("{err:#}")),
    }
    e
}

fn probe(path: &Path, shown: &str, out: &mut Scan) {
    if !out.first_visit(path, shown) {
        return;
    }
    out.files = out.files.saturating_add(1);
    let head = match magic(path) {
        Ok(h) => h,
        Err(err) => {
            out.unreadable.push(format!("{shown}: {err}"));
            return;
        }
    };
    let size = std::fs::metadata(path).map_or(0, |m| m.len());
    let plain = |kind: &'static str, format: &str| Entry {
        path: shown.to_string(),
        kind,
        size,
        format: format.to_string(),
        ..Entry::default()
    };
    if head.starts_with(b"MZ") && size > MAX_IMAGE {
        out.entries.push(Entry {
            problem: Some(format!(
                "{size} bytes, over the {MAX_IMAGE} an image is read up to"
            )),
            ..plain("big", "-")
        });
    } else if head.starts_with(b"MZ") {
        match std::fs::read(path) {
            Ok(b) => out.entries.push(probe_pe(shown, b)),
            Err(err) => out.unreadable.push(format!("{shown}: {err}")),
        }
    } else if head.starts_with(PDB20_MAGIC) {
        out.entries.push(plain("pdb", "PDB 2.00"));
    } else if head.starts_with(MSF70_MAGIC) {
        out.entries.push(plain("pdb", "MSF 7.00"));
    } else if head.starts_with(DBG_MAGIC)
        && path
            .extension()
            .is_some_and(|x| x.eq_ignore_ascii_case("dbg"))
    {
        out.entries.push(plain("dbg", "DBG"));
    }
}

fn walk(dir: &Path, depth: usize, out: &mut Scan) -> Result<()> {
    if !out.first_visit(dir, &dir.display().to_string()) {
        return Ok(());
    }
    if depth > WALK_DEPTH {
        out.unreadable.push(format!(
            "{}: deeper than {WALK_DEPTH} levels",
            dir.display()
        ));
        return Ok(());
    }
    let mut entries: Vec<PathBuf> = match std::fs::read_dir(dir) {
        Ok(rd) => rd.filter_map(|e| e.ok().map(|e| e.path())).collect(),
        Err(err) => {
            out.unreadable.push(format!("{}: {err}", dir.display()));
            return Ok(());
        }
    };
    entries.sort();
    for p in entries {
        if out.files > WALK_FILES {
            anyhow::bail!("over {WALK_FILES} files under the paths given");
        }
        let Ok(meta) = std::fs::symlink_metadata(&p) else {
            out.unreadable.push(format!("{}: no metadata", p.display()));
            continue;
        };
        let shown = p.display().to_string();
        if meta.file_type().is_symlink() {
            if p.is_dir() {
                out.skipped_links.push(shown);
            } else if p.is_file() {
                probe(&p, &shown, out);
            }
            continue;
        }
        if meta.is_dir() {
            walk(&p, depth.saturating_add(1), out)?;
        } else if meta.is_file() {
            probe(&p, &shown, out);
        }
    }
    Ok(())
}

pub fn scan(paths: &[PathBuf]) -> Result<Scan> {
    let mut out = Scan::default();
    for p in paths {
        let meta = std::fs::metadata(p).with_context(|| format!("{}", p.display()))?;
        if meta.is_dir() {
            walk(p, 0, &mut out)?;
        } else {
            probe(p, &p.display().to_string(), &mut out);
        }
    }
    Ok(out)
}

#[must_use]
pub fn line(e: &Entry) -> String {
    let what = match (e.kind, e.embedded) {
        ("pe", true) => format!(
            "CodeView {} embedded, {} byte(s), {} subsection(s), {} module(s){}",
            e.format,
            e.record_bytes.unwrap_or(0),
            e.subsections.unwrap_or(0),
            e.modules.unwrap_or(0),
            match e.found {
                Some(nb::Found::Trailer) => ", found by its trailer",
                _ => "",
            }
        ),
        ("pe", false) if e.format == "none" => "no CodeView record".to_string(),
        ("pe", false) => format!(
            "CodeView {} -> {}{}{}",
            e.format,
            e.pdb.as_deref().unwrap_or("(no path)"),
            e.signature
                .map_or(String::new(), |s| format!(" signature {s:#010x}")),
            e.guid
                .as_deref()
                .map_or(String::new(), |g| format!(" guid {g}"))
                + &e.age.map_or(String::new(), |a| format!(" age {a}"))
        ),
        _ => e.format.clone(),
    };
    let debug = if e.debug.is_empty() {
        String::new()
    } else {
        format!("; debug directory: {}", e.debug.join(", "))
    };
    let problem = e
        .problem
        .as_deref()
        .map_or(String::new(), |p| format!("; PROBLEM {p}"));
    format!(
        "  {:<4} {:>10}  {}  {what}{debug}{problem}",
        e.kind, e.size, e.path
    )
}
