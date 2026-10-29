use anyhow::{Context as _, Result, bail};

use crate::pe::{Image, ResId, ResourceEntry};
use crate::u32_of;

pub const MEMORY_FLAGS: u16 = 0x1030;

pub const TYPE_NAMES: [(u32, &str); 17] = [
    (1, "CURSOR"),
    (2, "BITMAP"),
    (3, "ICON"),
    (4, "MENU"),
    (5, "DIALOG"),
    (6, "STRING"),
    (7, "FONTDIR"),
    (8, "FONT"),
    (9, "ACCELERATOR"),
    (10, "RCDATA"),
    (11, "MESSAGETABLE"),
    (12, "GROUP_CURSOR"),
    (14, "GROUP_ICON"),
    (16, "VERSION"),
    (17, "DLGINCLUDE"),
    (23, "HTML"),
    (24, "MANIFEST"),
];

#[must_use]
pub fn type_name(id: &ResId) -> String {
    match id {
        ResId::Ordinal(n) => TYPE_NAMES
            .iter()
            .find(|(k, _)| k == n)
            .map_or_else(|| n.to_string(), |(_, s)| (*s).to_string()),
        ResId::Name(s) => s.clone(),
    }
}

fn encode_id(id: &ResId) -> Vec<u8> {
    match id {
        ResId::Ordinal(n) => {
            let mut v = 0xffff_u16.to_le_bytes().to_vec();
            v.extend_from_slice(&u16::try_from(*n).unwrap_or(u16::MAX).to_le_bytes());
            v
        }
        ResId::Name(s) => {
            let mut v: Vec<u8> = s.encode_utf16().flat_map(u16::to_le_bytes).collect();
            v.extend_from_slice(&[0, 0]);
            v
        }
    }
}

#[must_use]
pub fn entry(type_id: &ResId, name_id: &ResId, lang: u16, data: &[u8], memflags: u16) -> Vec<u8> {
    let mut head = encode_id(type_id);
    head.extend_from_slice(&encode_id(name_id));
    while !head.len().is_multiple_of(4) {
        head.push(0);
    }
    let header_size = u32_of(head.len().saturating_add(24));
    let mut out = Vec::with_capacity(data.len().saturating_add(64));
    out.extend_from_slice(&u32_of(data.len()).to_le_bytes());
    out.extend_from_slice(&header_size.to_le_bytes());
    out.extend_from_slice(&head);
    out.extend_from_slice(&0u32.to_le_bytes());
    out.extend_from_slice(&memflags.to_le_bytes());
    out.extend_from_slice(&lang.to_le_bytes());
    out.extend_from_slice(&0u32.to_le_bytes());
    out.extend_from_slice(&0u32.to_le_bytes());
    out.extend_from_slice(data);
    while !out.len().is_multiple_of(4) {
        out.push(0);
    }
    out
}

fn data_of<'a>(img: &'a Image, r: &ResourceEntry) -> Result<&'a [u8]> {
    img.at_rva(r.data_rva, r.data_len).with_context(|| {
        format!(
            "resource {}/{}/{} data unreadable at rva {:#x}",
            r.type_id,
            r.name_id,
            r.lang_id,
            r.data_rva.get()
        )
    })
}

pub fn res_items(img: &Image) -> Result<Vec<ResourceEntry>> {
    let mut items = img.resources()?;
    items.sort_by(|a, b| {
        (&a.type_id, &a.name_id, &a.lang_id).cmp(&(&b.type_id, &b.name_id, &b.lang_id))
    });
    let mut total = 0usize;
    for r in &items {
        data_of(img, r)?;
        total = total.saturating_add(r.data_len);
    }
    if total > img.len() {
        bail!(
            "the resources' data entries hold {total} bytes, more than the {}-byte file: their data overlap",
            img.len()
        );
    }
    Ok(items)
}

pub fn res_blob(img: &Image, items: &[ResourceEntry]) -> Result<Vec<u8>> {
    let mut blob = entry(&ResId::Ordinal(0), &ResId::Ordinal(0), 0, &[], 0);
    for r in items {
        let data = data_of(img, r)?;
        let lang = match &r.lang_id {
            ResId::Ordinal(n) => u16::try_from(*n).unwrap_or(0),
            ResId::Name(_) => 0,
        };
        blob.extend_from_slice(&entry(&r.type_id, &r.name_id, lang, data, MEMORY_FLAGS));
    }
    Ok(blob)
}

pub fn res_file(img: &Image) -> Result<Option<(Vec<u8>, Vec<ResourceEntry>)>> {
    let items = res_items(img)?;
    if items.is_empty() {
        return Ok(None);
    }
    let blob = res_blob(img, &items)?;
    Ok(Some((blob, items)))
}

pub const RT_VERSION: u32 = 16;
pub const FIXED_SIGNATURE: u32 = 0xFEEF_04BD;
pub const VERSION_KEY: &str = "VS_VERSION_INFO";

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, serde::Serialize)]
pub struct FileVersion {
    pub major: u32,
    pub minor: u32,
    pub build: u32,
    pub revision: u32,
}

impl std::fmt::Display for FileVersion {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{:02}.{}", self.major, self.minor, self.build)
    }
}

pub fn fixed_file_version(block: &[u8]) -> Result<FileVersion> {
    let key: Vec<u8> = VERSION_KEY
        .encode_utf16()
        .chain(std::iter::once(0))
        .flat_map(u16::to_le_bytes)
        .collect();
    let value_len = msvc6_base::le::u16_at(block, 2).context("VS_VERSIONINFO header truncated")?;
    if block.get(6..6 + key.len()) != Some(key.as_slice()) {
        anyhow::bail!("the version resource does not open on {VERSION_KEY}");
    }
    let at = (6 + key.len()).next_multiple_of(4);
    if usize::from(value_len) < 52 {
        anyhow::bail!(
            "the version resource holds a {value_len}-byte value, under VS_FIXEDFILEINFO's 52"
        );
    }
    let sig = msvc6_base::le::u32_at(block, at).context("VS_FIXEDFILEINFO truncated")?;
    if sig != FIXED_SIGNATURE {
        anyhow::bail!("VS_FIXEDFILEINFO signature {sig:#x}, not {FIXED_SIGNATURE:#x}");
    }
    let ms = msvc6_base::le::u32_at(block, at + 8).context("VS_FIXEDFILEINFO truncated")?;
    let ls = msvc6_base::le::u32_at(block, at + 12).context("VS_FIXEDFILEINFO truncated")?;
    Ok(FileVersion {
        major: ms >> 16,
        minor: ms & 0xffff,
        build: ls >> 16,
        revision: ls & 0xffff,
    })
}

pub fn file_version(img: &Image) -> Result<Option<FileVersion>> {
    let Some(e) = img
        .resources()?
        .into_iter()
        .find(|e| e.type_id == ResId::Ordinal(RT_VERSION))
    else {
        return Ok(None);
    };
    let block = img.at_rva(e.data_rva, e.data_len).with_context(|| {
        format!(
            "the version resource at rva {:#x} runs past the file bytes",
            e.data_rva.get()
        )
    })?;
    fixed_file_version(block).map(Some)
}
