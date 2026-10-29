use anyhow::{Context as _, Result};
use std::path::Path;

pub const MAX_IMAGE: u64 = 256 << 20;

pub const MAX_TEXT: u64 = 64 << 20;

pub fn read(path: &Path, cap: u64) -> Result<Vec<u8>> {
    msvc6_base::srcio::read_bounded(path, cap)
        .with_context(|| format!("reading {}", path.display()))
}

pub fn text(path: &Path, cap: u64) -> Result<String> {
    Ok(msvc6_base::srcio::tool_text(&read(path, cap)?))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_device_a_directory_a_fifo_and_a_file_past_its_cap_are_refused_by_the_bounded_reader() {
        let d = std::env::temp_dir().join(format!("msvc6-match-input-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        let fifo = d.join("p.tsv");
        let made = std::process::Command::new("mkfifo")
            .arg(&fifo)
            .status()
            .is_ok_and(|s| s.success());
        let mut odd = vec![Path::new("/dev/zero").to_path_buf(), d.clone()];
        if made {
            odd.push(fifo);
        }
        for p in &odd {
            let e = format!("{:#}", read(p, 16).unwrap_err());
            assert!(
                e.contains("not a regular file (a device, FIFO or directory is not read)"),
                "{}: {e}",
                p.display()
            );
        }
        let f = d.join("x.tsv");
        msvc6_base::srcio::own_write(&f, "0123456789").unwrap();
        let e = format!("{:#}", read(&f, 4).unwrap_err());
        assert!(e.contains("more than the 4"), "{e}");
        assert_eq!(text(&f, 64).unwrap(), "0123456789");
        std::fs::remove_dir_all(&d).unwrap();
    }
}
