#![allow(clippy::disallowed_methods)]

use std::io;
use std::path::Path;

#[must_use]
pub fn decode(bytes: &[u8]) -> String {
    bytes.iter().map(|&b| char::from(b)).collect()
}

pub fn encode(text: &str) -> io::Result<Vec<u8>> {
    let mut out = Vec::with_capacity(text.len());
    let mut line = 1usize;
    for c in text.chars() {
        match u8::try_from(u32::from(c)) {
            Ok(b) => out.push(b),
            Err(_) => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!(
                        "line {line} holds U+{:04X}, which no source byte decodes to (a lossy read upstream, \
                         or UTF-8 text in a code-page source); refusing to write it",
                        u32::from(c)
                    ),
                ));
            }
        }
        if c == '\n' {
            line += 1;
        }
    }
    Ok(out)
}

pub fn read(p: impl AsRef<Path>) -> io::Result<String> {
    Ok(decode(&std::fs::read(p)?))
}

pub fn write(p: impl AsRef<Path>, text: &str) -> io::Result<()> {
    let p = p.as_ref();
    let bytes =
        encode(text).map_err(|e| io::Error::new(e.kind(), format!("{}: {e}", p.display())))?;
    std::fs::write(p, bytes)
}

pub fn write_bytes(p: impl AsRef<Path>, bytes: &[u8]) -> io::Result<()> {
    std::fs::write(p, bytes)
}

pub fn replace_bytes(p: impl AsRef<Path>, bytes: &[u8]) -> io::Result<()> {
    let p = p.as_ref();
    let name = p
        .file_name()
        .map_or_else(String::new, |n| n.to_string_lossy().into_owned());
    let tmp = p.with_file_name(format!(".msvc6t-replace-{}-{name}", std::process::id()));
    let done = (|| {
        std::fs::write(&tmp, bytes)?;
        if let Ok(m) = std::fs::metadata(p) {
            std::fs::set_permissions(&tmp, m.permissions())?;
        }
        std::fs::rename(&tmp, p)
    })();
    if done.is_err() {
        std::fs::remove_file(&tmp).ok();
    }
    done
}

pub fn own_read(p: impl AsRef<Path>) -> io::Result<String> {
    std::fs::read_to_string(p)
}

pub fn own_write(p: impl AsRef<Path>, contents: impl AsRef<[u8]>) -> io::Result<()> {
    std::fs::write(p, contents)
}

#[must_use]
pub fn tool_text(bytes: &[u8]) -> String {
    String::from_utf8_lossy(bytes).into_owned()
}

fn refused(p: &Path, why: String) -> io::Error {
    io::Error::new(
        io::ErrorKind::InvalidInput,
        format!("{}: {why}", p.display()),
    )
}

fn regular(p: &Path, m: &std::fs::Metadata, cap: u64) -> io::Result<()> {
    if !m.is_file() {
        return Err(refused(
            p,
            "not a regular file (a device, FIFO or directory is not read)".into(),
        ));
    }
    if m.len() > cap {
        return Err(refused(
            p,
            format!("{} bytes, more than the {cap} this reader takes", m.len()),
        ));
    }
    Ok(())
}

#[cfg(unix)]
fn open_for_read(p: &Path) -> io::Result<std::fs::File> {
    use rustix::fs::{Mode, OFlags};
    let fd = rustix::fs::open(
        p,
        OFlags::RDONLY | OFlags::NONBLOCK | OFlags::CLOEXEC | OFlags::NOCTTY,
        Mode::empty(),
    )?;
    Ok(std::fs::File::from(fd))
}

#[cfg(not(unix))]
fn open_for_read(p: &Path) -> io::Result<std::fs::File> {
    std::fs::File::open(p)
}

fn open_regular(p: &Path, cap: u64) -> io::Result<(std::fs::File, std::fs::Metadata)> {
    let f = open_for_read(p)?;
    let m = f.metadata()?;
    regular(p, &m, cap)?;
    Ok((f, m))
}

pub fn read_bounded(p: impl AsRef<Path>, cap: u64) -> io::Result<Vec<u8>> {
    use std::io::Read as _;
    let p = p.as_ref();
    regular(p, &std::fs::metadata(p)?, cap)?;
    let (f, m) = open_regular(p, cap)?;
    let mut out = Vec::with_capacity(usize::try_from(m.len()).unwrap_or(0));
    f.take(cap.saturating_add(1)).read_to_end(&mut out)?;
    if u64::try_from(out.len()).unwrap_or(u64::MAX) > cap {
        return Err(refused(
            p,
            format!("grew past the {cap} bytes this reader takes while it was read"),
        ));
    }
    Ok(out)
}

pub fn read_text_bounded(p: impl AsRef<Path>, cap: u64) -> io::Result<String> {
    Ok(decode(&read_bounded(p, cap)?))
}

pub fn own_read_bounded(p: impl AsRef<Path>, cap: u64) -> io::Result<String> {
    let p = p.as_ref();
    String::from_utf8(read_bounded(p, cap)?)
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, format!("{}: {e}", p.display())))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(tag: &str) -> std::path::PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-base-srcio-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn read_then_write_with_no_edit_is_byte_identical_on_every_high_byte() {
        let dir = scratch("rt");
        let (a, b) = (dir.join("a.cpp"), dir.join("b.cpp"));
        let mut bytes = b"strcpy(g_text,\"\xa7\");\r\n/* \x80\x93\xe9\xff */\n".to_vec();
        bytes.extend(0x80u8..=0xff);
        std::fs::write(&a, &bytes).unwrap();
        let text = read(&a).unwrap();
        assert_eq!(text.lines().count(), 3);
        assert!(!text.contains('\u{fffd}'));
        write(&b, &text).unwrap();
        assert_eq!(std::fs::read(&b).unwrap(), bytes);
        write(&b, &text.replacen("strcpy", "strcat", 1)).unwrap();
        let out = std::fs::read(&b).unwrap();
        assert_eq!(&out[..20], b"strcat(g_text,\"\xa7\");\r");
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn a_char_no_byte_decodes_to_is_refused_by_line() {
        let e = encode("a\nb\n\"\u{fffd}\"\n").unwrap_err();
        assert_eq!(e.kind(), io::ErrorKind::InvalidData);
        assert!(e.to_string().starts_with("line 3 holds U+FFFD"), "{e}");
        let dir = scratch("bad");
        let p = dir.join("bad.cpp");
        let e = write(&p, "\u{2014}").unwrap_err();
        assert!(
            e.to_string().contains("bad.cpp") && e.to_string().contains("U+2014"),
            "{e}"
        );
        assert!(!p.exists());
        assert_eq!(encode("\u{a7}").unwrap(), [0xa7]);
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn a_bounded_read_refuses_a_device_a_directory_and_an_oversized_file() {
        let dir = scratch("bounded");
        let p = dir.join("a.bin");
        std::fs::write(&p, b"abcdef").unwrap();
        assert_eq!(read_bounded(&p, 6).unwrap(), b"abcdef");
        assert_eq!(read_text_bounded(&p, 6).unwrap(), "abcdef");
        let e = read_bounded(&p, 5).unwrap_err();
        assert!(e.to_string().contains("more than the 5"), "{e}");
        for d in [std::path::Path::new("/dev/zero"), dir.as_path()] {
            let e = read_bounded(d, 1 << 20).unwrap_err();
            assert_eq!(e.kind(), io::ErrorKind::InvalidInput, "{e}");
            assert!(e.to_string().contains("not a regular file"), "{e}");
        }
        std::fs::write(&p, b"\xff").unwrap();
        assert_eq!(
            own_read_bounded(&p, 4).unwrap_err().kind(),
            io::ErrorKind::InvalidData
        );
        assert_eq!(read_text_bounded(&p, 4).unwrap(), "\u{ff}");
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn a_fifo_swapped_in_after_the_check_is_refused_at_the_open_without_blocking() {
        let dir = scratch("fifo");
        let fifo = dir.join("swapped.tsv");
        rustix::fs::mknodat(
            rustix::fs::CWD,
            &fifo,
            rustix::fs::FileType::Fifo,
            rustix::fs::Mode::from_raw_mode(0o600),
            0,
        )
        .unwrap();
        let (tx, rx) = std::sync::mpsc::channel();
        let at = fifo.clone();
        std::thread::spawn(move || {
            let _ = tx.send(open_regular(&at, 1 << 20).map(|_| ()));
        });
        let e = rx
            .recv_timeout(std::time::Duration::from_secs(10))
            .expect("the open of a FIFO with no writer blocked")
            .unwrap_err();
        assert_eq!(e.kind(), io::ErrorKind::InvalidInput, "{e}");
        assert!(e.to_string().contains("not a regular file"), "{e}");
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn replacing_a_file_keeps_its_mode_and_leaves_no_temp() {
        use std::os::unix::fs::PermissionsExt as _;
        let dir = scratch("mode");
        let p = dir.join("run.bat");
        std::fs::write(&p, b"echo 0\r\n").unwrap();
        std::fs::set_permissions(&p, std::fs::Permissions::from_mode(0o755)).unwrap();
        replace_bytes(&p, b"echo 1\r\n").unwrap();
        assert_eq!(std::fs::read(&p).unwrap(), b"echo 1\r\n");
        assert_eq!(
            std::fs::metadata(&p).unwrap().permissions().mode() & 0o777,
            0o755
        );
        let left: Vec<_> = std::fs::read_dir(&dir)
            .unwrap()
            .map(|e| e.unwrap().file_name())
            .collect();
        assert_eq!(left, vec![std::ffi::OsString::from("run.bat")]);
        let fresh = dir.join("new.cpp");
        replace_bytes(&fresh, b"int a;\n").unwrap();
        assert_eq!(std::fs::read(&fresh).unwrap(), b"int a;\n");
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
