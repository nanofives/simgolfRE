use anyhow::{Context as _, Result, bail};
use std::path::{Component, Path, PathBuf};

pub fn rel_path(raw: &str) -> Result<PathBuf> {
    let mut out = PathBuf::new();
    if raw.starts_with(['/', '\\']) {
        bail!("`{raw}` is absolute where a relative path is wanted");
    }
    for part in raw.split(['\\', '/']).filter(|p| !p.is_empty()) {
        if part == "." || part == ".." {
            bail!("`{raw}` holds `{part}`: a path here may not climb or loop");
        }
        if part.contains(':') || part.chars().any(char::is_control) {
            bail!(
                "`{raw}` holds `{part}`, which names a drive or a stream, or holds a control character"
            );
        }
        out.push(part);
    }
    if out.as_os_str().is_empty() {
        bail!("`{raw}` names nothing");
    }
    Ok(out)
}

pub fn dos_rel(dos: &str) -> Result<PathBuf> {
    let rest = dos
        .strip_prefix("C:")
        .or_else(|| dos.strip_prefix("c:"))
        .filter(|r| r.starts_with(['\\', '/']))
        .with_context(|| format!("`{dos}` is not a C:\\ path; a prefix has only drive_c"))?;
    rel_path(rest.trim_start_matches(['\\', '/']))
}

pub fn rel_join(base: &Path, raw: &str) -> Result<PathBuf> {
    let mut parts: Vec<std::ffi::OsString> = base
        .components()
        .filter_map(|c| match c {
            Component::Normal(n) => Some(n.to_os_string()),
            _ => None,
        })
        .collect();
    for part in raw.split(['\\', '/']).filter(|p| !p.is_empty()) {
        match part {
            "." => {}
            ".." => {
                if parts.pop().is_none() {
                    bail!("`{raw}` climbs above drive_c from {}", base.display());
                }
            }
            p if p.contains(':') || p.contains('\0') => {
                bail!("`{raw}` holds `{p}`, which names a drive or a stream")
            }
            p => parts.push(p.into()),
        }
    }
    Ok(parts.iter().collect())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Root {
    pub root: PathBuf,
}

fn is_link(p: &Path) -> bool {
    std::fs::symlink_metadata(p).is_ok_and(|m| m.file_type().is_symlink())
}

impl Root {
    pub fn new(root: &Path) -> Result<Self> {
        let m = std::fs::symlink_metadata(root)
            .with_context(|| format!("{} does not exist", root.display()))?;
        if m.file_type().is_symlink() || !m.is_dir() {
            bail!("{} is not a real directory", root.display());
        }
        Ok(Self {
            root: root.to_path_buf(),
        })
    }

    pub fn path(&self, rel: &Path) -> PathBuf {
        self.root.join(rel)
    }

    pub fn check(&self, rel: &Path, last_may_link: bool) -> Result<PathBuf> {
        let mut at = self.root.clone();
        let comps: Vec<_> = rel.components().collect();
        for (i, c) in comps.iter().enumerate() {
            let Component::Normal(n) = c else {
                bail!("{} is not a plain relative path", rel.display());
            };
            at.push(n);
            let last = i + 1 == comps.len();
            match std::fs::symlink_metadata(&at) {
                Err(_) => {}
                Ok(m) if m.file_type().is_symlink() => {
                    if !(last && last_may_link) {
                        bail!(
                            "{} passes through the symbolic link {}: nothing is written or removed through a link",
                            self.path(rel).display(),
                            at.display()
                        );
                    }
                }
                Ok(m) if !last && !m.is_dir() => {
                    bail!("{} is not a directory", at.display());
                }
                Ok(_) => {}
            }
        }
        Ok(at)
    }

    pub fn create_dirs(&self, rel: &Path) -> Result<(PathBuf, Option<PathBuf>)> {
        self.check(rel, false)?;
        let mut at = self.root.clone();
        let mut first: Option<PathBuf> = None;
        for c in rel.components() {
            at.push(c);
            match std::fs::create_dir(&at) {
                Ok(()) => {
                    if first.is_none() {
                        first = Some(at.strip_prefix(&self.root).unwrap_or(&at).to_path_buf());
                    }
                }
                Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
                    if !is_link(&at) && !at.is_dir() {
                        bail!("{} is not a directory", at.display());
                    }
                }
                Err(e) => {
                    return Err(e).with_context(|| format!("creating {}", at.display()));
                }
            }
            if is_link(&at) {
                bail!(
                    "{} became a symbolic link while it was created",
                    at.display()
                );
            }
        }
        Ok((at, first))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn both_separators_are_one_and_neither_climbs() {
        assert_eq!(dos_rel("C:\\a/b\\c").unwrap(), PathBuf::from("a/b/c"));
        assert_eq!(
            dos_rel("c:/Program Files\\X").unwrap(),
            PathBuf::from("Program Files/X")
        );
        assert!(dos_rel("C:\\n/../../escaped").is_err());
        assert!(dos_rel("C:\\a\\..\\b").is_err());
        assert!(dos_rel("D:\\x").is_err());
        assert!(dos_rel("C:x").is_err());
        assert!(dos_rel("C:\\").is_err());
        assert!(rel_path("../x").is_err());
        assert!(rel_path("a/./b").is_err());
        assert!(rel_path("/etc/passwd").is_err());
        assert!(rel_path("a:b").is_err());
        assert_eq!(
            rel_join(Path::new("lk/ide"), "..\\lpc10.lib\\Release\\x.lib").unwrap(),
            PathBuf::from("lk/lpc10.lib/Release/x.lib")
        );
        assert!(rel_join(Path::new("lk"), "..\\..\\x").is_err());
    }

    #[test]
    fn a_path_through_a_link_is_refused() {
        let d = std::env::temp_dir().join(format!("msvc6t-confine-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(d.join("root")).unwrap();
        std::fs::create_dir_all(d.join("outside")).unwrap();
        std::os::unix::fs::symlink(d.join("outside"), d.join("root/m")).unwrap();
        let r = Root::new(&d.join("root")).unwrap();
        assert!(r.check(Path::new("m/x"), false).is_err());
        assert!(r.check(Path::new("m"), true).is_ok());
        assert!(r.check(Path::new("m"), false).is_err());
        let (made, first) = r.create_dirs(Path::new("a/b")).unwrap();
        assert!(made.is_dir());
        assert_eq!(first, Some(PathBuf::from("a")));
        let (again, none) = r.create_dirs(Path::new("a/b")).unwrap();
        assert_eq!((again, none), (made.clone(), None));
        msvc6_base::srcio::own_write(d.join("root/f"), b"x").unwrap();
        assert!(r.create_dirs(Path::new("f/g")).is_err());
        assert!(r.create_dirs(Path::new("m/z")).is_err());
        assert!(!d.join("outside/z").exists());
        assert!(Root::new(&d.join("root/m")).is_err());
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_file_or_a_link_where_the_last_directory_goes_is_refused() {
        let d = std::env::temp_dir().join(format!("msvc6t-confine-last-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(d.join("root/a")).unwrap();
        std::fs::create_dir_all(d.join("outside")).unwrap();
        msvc6_base::srcio::own_write(d.join("root/f"), b"x").unwrap();
        msvc6_base::srcio::own_write(d.join("root/a/f"), b"x").unwrap();
        std::os::unix::fs::symlink(d.join("outside"), d.join("root/a/m")).unwrap();
        std::os::unix::fs::symlink(d.join("nowhere"), d.join("root/a/dangling")).unwrap();
        let r = Root::new(&d.join("root")).unwrap();
        assert!(r.create_dirs(Path::new("f")).is_err());
        assert!(r.create_dirs(Path::new("a/f")).is_err());
        assert!(r.create_dirs(Path::new("a/m")).is_err());
        assert!(r.create_dirs(Path::new("a/dangling")).is_err());
        assert!(r.create_dirs(Path::new("a/m/z")).is_err());
        assert!(!d.join("outside/z").exists());
        assert!(!d.join("nowhere").exists());
        let (made, first) = r.create_dirs(Path::new("a/b")).unwrap();
        assert!(made.is_dir());
        assert_eq!(first, Some(PathBuf::from("a/b")));
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn jobs_racing_on_one_directory_all_get_it() {
        let d = std::env::temp_dir().join(format!("msvc6t-confine-race-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(d.join("root")).unwrap();
        let r = Root::new(&d.join("root")).unwrap();
        for round in 0..200 {
            let rel = PathBuf::from(format!("o{round}/ab/cd"));
            let gate = std::sync::Barrier::new(8);
            let results: Vec<Result<PathBuf>> = std::thread::scope(|s| {
                let hs: Vec<_> = (0..8)
                    .map(|_| {
                        s.spawn(|| {
                            gate.wait();
                            r.create_dirs(&rel).map(|(p, _)| p)
                        })
                    })
                    .collect();
                hs.into_iter().map(|h| h.join().unwrap()).collect()
            });
            for x in results {
                assert!(x.is_ok_and(|p| p.is_dir()), "round {round}");
            }
        }
        std::fs::remove_dir_all(&d).unwrap();
    }
}
