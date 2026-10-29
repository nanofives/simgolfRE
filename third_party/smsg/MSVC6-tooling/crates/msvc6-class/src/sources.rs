use crate::read;
use anyhow::{Context as _, Result, bail};
use std::path::{Path, PathBuf};

pub const EXTENSIONS: [&str; 3] = ["cpp", "h", "inc"];
pub const MAX_FILES: usize = 1 << 16;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Source {
    pub key: String,
    pub path: PathBuf,
    pub text: String,
}

fn group(dir: &Path) -> String {
    dir.file_name().map_or_else(
        || dir.display().to_string(),
        |n| n.to_string_lossy().into_owned(),
    )
}

pub fn read_all(dirs: &[PathBuf], exts: &[&str]) -> Result<Vec<Source>> {
    let mut groups: Vec<(String, Vec<PathBuf>)> = Vec::new();
    for d in dirs {
        let g = group(d);
        let files = read::files(d, exts)
            .with_context(|| format!("[image.classes] source {}", d.display()))?;
        match groups.iter_mut().find(|(k, _)| *k == g) {
            Some((_, v)) => v.extend(files),
            None => groups.push((g, files)),
        }
    }
    let mut out = Vec::new();
    for (g, mut files) in groups {
        files.sort_by(|a, b| a.file_name().cmp(&b.file_name()));
        for p in files {
            if out.len() >= MAX_FILES {
                bail!(
                    "more than {MAX_FILES} source files, more than a class population here reads"
                );
            }
            let name = p
                .file_name()
                .map_or_else(String::new, |n| n.to_string_lossy().into_owned());
            let text = read::text(&p, read::MAX_SOURCE)?;
            out.push(Source {
                key: format!("{g}/{name}"),
                path: p,
                text,
            });
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn directories_of_one_name_read_as_one_in_file_name_order() {
        let t = std::env::temp_dir().join(format!("msvc6-class-sources-{}", std::process::id()));
        let (a, b, c) = (t.join("app/src"), t.join("lib/src"), t.join("app/include"));
        for d in [&a, &b, &c] {
            std::fs::create_dir_all(d).unwrap();
        }
        msvc6_base::srcio::write_bytes(a.join("z.cpp"), b"z").unwrap();
        msvc6_base::srcio::write_bytes(b.join("m.cpp"), b"m").unwrap();
        msvc6_base::srcio::write_bytes(c.join("a.h"), b"a").unwrap();
        msvc6_base::srcio::write_bytes(c.join("note.md"), b"x").unwrap();
        let got = read_all(&[a, c, b], &EXTENSIONS).unwrap();
        let keys: Vec<&str> = got.iter().map(|s| s.key.as_str()).collect();
        assert_eq!(keys, vec!["src/m.cpp", "src/z.cpp", "include/a.h"]);
        assert!(read_all(&[t.join("absent")], &EXTENSIONS).is_err());
        std::fs::remove_dir_all(&t).unwrap();
    }
}
