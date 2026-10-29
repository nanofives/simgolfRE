use anyhow::{Context as _, Result};
use msvc6_diff::comdat::{self, Comdat};
use msvc6_image::archive::Archive;
use std::collections::{BTreeSet, HashMap};
use std::path::{Path, PathBuf};

pub const MAX_ARCHIVE: u64 = 512 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Member {
    pub from: String,
    pub section: Comdat,
    pub off: u32,
}

#[derive(Debug, Clone, Default)]
pub struct Libraries {
    pub members: HashMap<String, Vec<Member>>,
    pub refused: Vec<(PathBuf, String)>,
}

pub fn read_bounded(path: &Path, max: u64) -> Result<Vec<u8>> {
    msvc6_base::srcio::read_bounded(path, max)
        .with_context(|| format!("reading {}", path.display()))
}

fn label(path: &Path) -> String {
    path.file_name().map_or_else(
        || path.display().to_string(),
        |f| f.to_string_lossy().into_owned(),
    )
}

#[must_use]
pub fn load(archives: &[PathBuf], names: &BTreeSet<String>) -> Libraries {
    let mut out = Libraries::default();
    if names.is_empty() {
        return out;
    }
    for a in archives {
        let bytes = match read_bounded(a, MAX_ARCHIVE) {
            Ok(b) => b,
            Err(e) => {
                out.refused.push((a.clone(), format!("{e:#}")));
                continue;
            }
        };
        let ar = match Archive::parse(&bytes) {
            Ok(ar) => ar,
            Err(e) => {
                out.refused.push((a.clone(), format!("{e:#}")));
                continue;
            }
        };
        let file = label(a);
        for (m, obj) in ar.content() {
            if !names
                .iter()
                .any(|n| memchr::memmem::find(obj, n.as_bytes()).is_some())
            {
                continue;
            }
            let Ok(sections) = comdat::text_of(obj) else {
                continue;
            };
            for (idx, off, n) in comdat::defined_externals(obj).unwrap_or_default() {
                if !names.contains(&n) {
                    continue;
                }
                if let Some(s) = sections.iter().find(|s| s.index == idx) {
                    let copies = out.members.entry(n).or_default();
                    if !copies
                        .iter()
                        .any(|c| c.section.bytes == s.bytes && c.off == off)
                    {
                        copies.push(Member {
                            from: format!("{file} {}", m.name),
                            section: s.clone(),
                            off,
                        });
                    }
                }
            }
        }
    }
    out
}
