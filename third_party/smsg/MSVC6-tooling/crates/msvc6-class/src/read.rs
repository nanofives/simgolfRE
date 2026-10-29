use anyhow::{Context as _, Result, bail};
use std::io::Read as _;
use std::path::{Path, PathBuf};

pub const MAX_SOURCE: u64 = 64 << 20;
pub const MAX_IMAGE: u64 = 512 << 20;
pub const MAX_OBJECT: u64 = 256 << 20;
pub const MAX_ARCHIVE: u64 = 512 << 20;
pub const MAX_TEXT: u64 = 256 << 20;
pub const MAX_ENTRIES: usize = 1 << 20;

#[cfg(unix)]
fn open_nonblocking(path: &Path) -> std::io::Result<std::fs::File> {
    use rustix::fs::{Mode, OFlags};
    let fd = rustix::fs::open(
        path,
        OFlags::RDONLY | OFlags::NONBLOCK | OFlags::CLOEXEC | OFlags::NOCTTY,
        Mode::empty(),
    )?;
    Ok(std::fs::File::from(fd))
}

#[cfg(not(unix))]
fn open_nonblocking(path: &Path) -> std::io::Result<std::fs::File> {
    std::fs::File::open(path)
}

pub fn open_regular(path: &Path, cap: u64) -> Result<(std::fs::File, u64)> {
    let f = open_nonblocking(path).with_context(|| format!("reading {}", path.display()))?;
    let meta = f
        .metadata()
        .with_context(|| format!("reading {}", path.display()))?;
    if !meta.is_file() {
        bail!("{}: not a regular file", path.display());
    }
    if meta.len() > cap {
        bail!(
            "{}: {} bytes, more than the {cap} this reader takes",
            path.display(),
            meta.len()
        );
    }
    Ok((f, meta.len()))
}

pub fn bytes(path: &Path, cap: u64) -> Result<Vec<u8>> {
    let (f, len) = open_regular(path, cap)?;
    let mut out = Vec::with_capacity(usize::try_from(len).unwrap_or(0));
    f.take(cap.saturating_add(1))
        .read_to_end(&mut out)
        .with_context(|| format!("reading {}", path.display()))?;
    if u64::try_from(out.len()).unwrap_or(u64::MAX) > cap {
        bail!(
            "{}: grew past the {cap} bytes this reader takes while it was read",
            path.display()
        );
    }
    Ok(out)
}

pub fn text(path: &Path, cap: u64) -> Result<String> {
    Ok(msvc6_base::srcio::decode(&bytes(path, cap)?))
}

pub fn files(dir: &Path, exts: &[&str]) -> Result<Vec<PathBuf>> {
    let rd = std::fs::read_dir(dir).with_context(|| format!("listing {}", dir.display()))?;
    let mut out = Vec::new();
    for e in rd {
        let e = e.with_context(|| format!("listing {}", dir.display()))?;
        if out.len() >= MAX_ENTRIES {
            bail!(
                "{}: more than {MAX_ENTRIES} entries, more than a listing here reads",
                dir.display()
            );
        }
        let p = e.path();
        let wanted = p
            .extension()
            .and_then(|x| x.to_str())
            .is_some_and(|x| exts.iter().any(|w| w.eq_ignore_ascii_case(x)));
        if wanted && std::fs::metadata(&p).is_ok_and(|m| m.is_file()) {
            out.push(p);
        }
    }
    out.sort();
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_device_or_a_directory_is_refused_before_it_is_opened() {
        let e = bytes(Path::new("/dev/zero"), MAX_SOURCE).unwrap_err();
        assert!(format!("{e:#}").contains("not a regular file"), "{e:#}");
        let e = bytes(Path::new("/"), MAX_SOURCE).unwrap_err();
        assert!(format!("{e:#}").contains("not a regular file"), "{e:#}");
    }

    #[test]
    fn a_file_over_the_cap_is_refused_and_one_under_it_is_read() {
        let d = std::env::temp_dir().join(format!("msvc6-class-read-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        let f = d.join("x.h");
        msvc6_base::srcio::write_bytes(&f, b"class A {};\n").unwrap();
        assert_eq!(bytes(&f, 64).unwrap(), b"class A {};\n");
        assert!(bytes(&f, 4).is_err());
        assert_eq!(files(&d, &["H"]).unwrap(), vec![f.clone()]);
        assert!(files(&d, &["cpp"]).unwrap().is_empty());
        std::fs::remove_dir_all(&d).unwrap();
    }
}
