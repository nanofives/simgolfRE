use anyhow::Result;
use msvc6_image::clock::{self, Field, Site};
use msvc6_image::pe::Image;
use std::path::Path;

pub use msvc6_image::clock::ALL as FIELDS;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Copied {
    pub site: Site,
    pub was: u32,
    pub now: u32,
    pub written: bool,
}

pub fn copy(ours: &Image, reference: &Image, fields: &[Field]) -> Result<(Vec<u8>, Vec<Copied>)> {
    let before = clock::resolve(ours, fields)?;
    let (bytes, applied) = clock::apply(ours, reference, fields)?;
    let rows = before
        .into_iter()
        .map(|s| Copied {
            site: s,
            was: msvc6_base::le::u32_at(ours.bytes(), s.off).unwrap_or(0),
            now: msvc6_base::le::u32_at(&bytes, s.off).unwrap_or(0),
            written: applied.sites.iter().any(|a| a.field == s.field),
        })
        .collect();
    Ok((bytes, rows))
}

pub fn copy_file(out: &Path, reference: &Path, fields: &[Field]) -> Result<Vec<String>> {
    let refi = Image::load(reference)?;
    let ours = Image::load(out)?;
    let (bytes, rows) = copy(&ours, &refi, fields)?;
    msvc6_base::srcio::replace_bytes(out, &bytes)?;
    Ok(rows
        .iter()
        .map(|c| {
            format!(
                "clock {:<38} file {:#08x}  {:#010x} -> {:#010x}{}",
                c.site.field.label(),
                c.site.off,
                c.was,
                c.now,
                if c.written {
                    " (the reference's)"
                } else {
                    " (already the reference's)"
                }
            )
        })
        .collect())
}
