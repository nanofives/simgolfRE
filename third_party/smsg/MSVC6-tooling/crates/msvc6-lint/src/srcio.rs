#![allow(clippy::disallowed_methods)]

use std::io;
use std::path::Path;

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
