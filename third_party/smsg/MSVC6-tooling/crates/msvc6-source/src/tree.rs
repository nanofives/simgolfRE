use crate::config::Loaded;
use anyhow::Result;
use std::path::{Path, PathBuf};

#[must_use]
pub fn safe_rel(p: &str) -> bool {
    !p.is_empty()
        && Path::new(p)
            .components()
            .all(|c| matches!(c, std::path::Component::Normal(_)))
}

#[must_use]
pub fn rel_str(p: &Path) -> String {
    p.components()
        .map(|c| c.as_os_str().to_string_lossy().into_owned())
        .collect::<Vec<_>>()
        .join("/")
}

fn walk(root: &Path, dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return;
    };
    let mut entries: Vec<_> = rd.flatten().collect();
    entries.sort_by_key(std::fs::DirEntry::file_name);
    for e in entries {
        let p = e.path();
        if e.file_name() == ".git" {
            continue;
        }
        let Ok(t) = e.file_type() else {
            continue;
        };
        if t.is_dir() {
            if p.join(".git").exists() {
                continue;
            }
            walk(root, &p, out);
        } else if p.is_file()
            && let Ok(r) = p.strip_prefix(root)
        {
            out.push(r.to_path_buf());
        }
    }
}

pub fn files(root: &Path, cfg: &Loaded) -> Result<Vec<PathBuf>> {
    let all = if crate::git::is_repo(root) {
        crate::git::listed(root)?
    } else {
        let mut v = Vec::new();
        walk(root, root, &mut v);
        v
    };
    let subs = crate::git::submodules(root);
    Ok(all
        .into_iter()
        .filter(|p| {
            let r = rel_str(p);
            !cfg.excluded(&r) && !subs.iter().any(|s| crate::glob::under(s, &r))
        })
        .collect())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_plain_directory_is_walked_without_nested_repositories() {
        let d = std::env::temp_dir().join(format!("msvc6-source-tree-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(d.join("a/b")).unwrap();
        std::fs::create_dir_all(d.join("vendor/.git")).unwrap();
        std::fs::create_dir_all(d.join("tools")).unwrap();
        for f in ["a/b/x.cpp", "a/y.h", "vendor/z.c", "tools/t.rs", "top.md"] {
            msvc6_base::srcio::write_bytes(d.join(f), b"x").unwrap();
        }
        let cfg = crate::config::parse("[source]\nexclude = [\"tools\"]\n", &d, None).unwrap();
        let got: Vec<String> = files(&d, &cfg)
            .unwrap()
            .iter()
            .map(|p| rel_str(p))
            .collect();
        assert_eq!(got, ["a/b/x.cpp", "a/y.h", "top.md"]);
        std::fs::remove_dir_all(&d).unwrap();
    }
}
