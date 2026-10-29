use anyhow::{Context as _, Result};
use std::path::Path;

pub fn read(path: &Path, cap: u64) -> Result<Vec<u8>> {
    msvc6_base::srcio::read_bounded(path, cap)
        .with_context(|| format!("reading {}", path.display()))
}

pub fn read_if(path: &Path, cap: u64) -> Result<Option<Vec<u8>>> {
    match std::fs::symlink_metadata(path) {
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(None),
        _ => read(path, cap).map(Some),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_device_a_directory_and_an_oversized_file_are_refused() {
        for p in ["/dev/zero", "/"] {
            let e = format!("{:#}", read(Path::new(p), 16).unwrap_err());
            assert!(
                e.contains("not a regular file (a device, FIFO or directory is not read)"),
                "{p}: {e}"
            );
        }
        let d = std::env::temp_dir().join(format!("msvc6-il-file-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        let f = d.join("x");
        msvc6_base::srcio::own_write(&f, "0123456789").unwrap();
        assert!(read(&f, 4).is_err());
        assert_eq!(read(&f, 64).unwrap().len(), 10);
        assert!(read_if(&d.join("absent"), 4).unwrap().is_none());
        std::fs::remove_dir_all(&d).unwrap();
    }
}
