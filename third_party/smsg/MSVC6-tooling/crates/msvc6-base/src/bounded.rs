use std::io;
use std::path::Path;

pub const TEXT_CAP: u64 = 256 << 20;

pub fn read(path: &Path, cap: u64) -> io::Result<Vec<u8>> {
    crate::srcio::read_bounded(path, cap)
}

pub fn source(path: &Path, cap: u64) -> io::Result<String> {
    read(path, cap).map(|b| crate::srcio::decode(&b))
}

pub fn tool(path: &Path, cap: u64) -> io::Result<String> {
    read(path, cap).map(|b| crate::srcio::tool_text(&b))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(tag: &str) -> std::path::PathBuf {
        let d =
            std::env::temp_dir().join(format!("msvc6-base-bounded-{tag}-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn a_device_and_a_directory_are_refused_and_a_file_past_its_cap_too() {
        let e = read(Path::new("/dev/zero"), 16).unwrap_err();
        assert!(
            e.to_string()
                .contains("not a regular file (a device, FIFO or directory is not read)"),
            "{e}"
        );
        let d = scratch("dir");
        assert!(read(&d, 16).is_err());
        let f = d.join("f.txt");
        crate::srcio::own_write(&f, b"0123456789").unwrap();
        assert_eq!(read(&f, 10).unwrap(), b"0123456789");
        let e = read(&f, 9).unwrap_err();
        assert!(e.to_string().contains("more than the 9"), "{e}");
        assert_eq!(source(&f, 10).unwrap(), "0123456789");
        std::fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn a_socket_and_a_symlink_to_a_device_are_refused_before_any_read_blocks() {
        let d = scratch("socket");
        let s = d.join("s.sock");
        let _l = std::os::unix::net::UnixListener::bind(&s).unwrap();
        let e = read(&s, 1 << 20).unwrap_err();
        assert!(e.to_string().contains("not a regular file"), "{e}");
        let link = d.join("zero");
        std::os::unix::fs::symlink("/dev/zero", &link).unwrap();
        let e = read(&link, 1 << 20).unwrap_err();
        assert!(
            e.to_string().contains("not a regular file"),
            "a symlink is followed, so the TARGET decides: {e}"
        );
        let e = tool(&d.join("nothing-here"), 16).unwrap_err();
        assert_eq!(e.kind(), io::ErrorKind::NotFound, "{e}");
        std::fs::remove_dir_all(&d).ok();
    }
}
