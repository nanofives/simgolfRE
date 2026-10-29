use anyhow::{Context as _, Result, bail};
use msvc6_base::le;
use serde::{Deserialize, Serialize};

use crate::pe::dirs::DEBUG_TYPE_CODEVIEW;
use crate::pe::{Dir, Image};
use crate::us;

pub const MAX_BYTES: usize = 20;
pub const RELINK_NOISE_BYTES: usize = 12;

const _: () = assert!(RELINK_NOISE_BYTES < MAX_BYTES);

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Deserialize, Serialize)]
pub enum Field {
    #[serde(rename = "coff.timestamp")]
    CoffTimestamp,
    #[serde(rename = "debug.timestamp")]
    DebugTimestamp,
    #[serde(rename = "pdb.signature")]
    PdbSignature,
    #[serde(rename = "pdb.age")]
    PdbAge,
    #[serde(rename = "export.timestamp")]
    ExportTimestamp,
}

pub const ALL: [Field; 5] = [
    Field::CoffTimestamp,
    Field::DebugTimestamp,
    Field::PdbSignature,
    Field::PdbAge,
    Field::ExportTimestamp,
];

const _: () = assert!(ALL.len() * 4 == MAX_BYTES);

impl Field {
    #[must_use]
    pub fn key(self) -> &'static str {
        match self {
            Self::CoffTimestamp => "coff.timestamp",
            Self::DebugTimestamp => "debug.timestamp",
            Self::PdbSignature => "pdb.signature",
            Self::PdbAge => "pdb.age",
            Self::ExportTimestamp => "export.timestamp",
        }
    }

    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::CoffTimestamp => "COFF TimeDateStamp",
            Self::DebugTimestamp => "IMAGE_DEBUG_DIRECTORY.TimeDateStamp",
            Self::PdbSignature => "CodeView NB10 signature",
            Self::PdbAge => "CodeView NB10 age",
            Self::ExportTimestamp => "IMAGE_EXPORT_DIRECTORY.TimeDateStamp",
        }
    }

    pub fn parse(s: &str) -> Result<Self> {
        ALL.into_iter().find(|f| f.key() == s).with_context(|| {
            format!(
                "no clock field `{s}`; the fields are {}",
                ALL.map(Field::key).join(", ")
            )
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Site {
    pub field: Field,
    pub off: usize,
}

pub fn check_request(fields: &[Field]) -> Result<()> {
    let mut sorted = fields.to_vec();
    sorted.sort_unstable();
    sorted.dedup();
    if sorted.len() != fields.len() {
        bail!("clock: a field is listed more than once");
    }
    if fields.len().saturating_mul(4) > MAX_BYTES {
        bail!(
            "clock: {} field(s) is {} byte(s), over the {MAX_BYTES}-byte cap",
            fields.len(),
            fields.len().saturating_mul(4)
        );
    }
    Ok(())
}

fn debug_sites(img: &Image) -> Result<(usize, Option<usize>)> {
    let entries = img.debug_entries();
    let e = entries
        .iter()
        .find(|e| e.kind == DEBUG_TYPE_CODEVIEW)
        .or_else(|| entries.first())
        .context(
            "clock: no debug directory entry, so the debug and CodeView clocks cannot be located",
        )?;
    let entry = img
        .rva_to_off(e.at)
        .with_context(|| {
            format!(
                "clock: debug directory entry at rva {:#x} has no file bytes",
                e.at.get()
            )
        })?
        .get();
    if e.kind != DEBUG_TYPE_CODEVIEW {
        return Ok((entry, None));
    }
    let raw = us(e.data_ptr);
    if le::bytes_at(img.bytes(), raw, 16)
        .ok()
        .and_then(|b| b.get(..4))
        != Some(&b"NB10"[..])
    {
        bail!(
            "clock: the CodeView record at {raw:#x} is not NB10, so its signature and age are not located"
        );
    }
    Ok((entry, Some(raw)))
}

fn export_site(img: &Image) -> Result<Option<usize>> {
    let Some(d) = img.dir(Dir::Export).filter(|d| d.rva.get() != 0) else {
        return Ok(None);
    };
    if d.size < 40 {
        bail!(
            "clock: the export directory at rva {:#x} is {} byte(s), under the 40 an IMAGE_EXPORT_DIRECTORY holds",
            d.rva.get(),
            d.size
        );
    }
    let off = img.rva_to_off(d.rva).with_context(|| {
        format!(
            "clock: the export directory at rva {:#x} has no file bytes",
            d.rva.get()
        )
    })?;
    Ok(Some(off.get().saturating_add(4)))
}

pub fn resolve(img: &Image, fields: &[Field]) -> Result<Vec<Site>> {
    check_request(fields)?;
    let need_debug = fields.iter().any(|f| {
        matches!(
            f,
            Field::DebugTimestamp | Field::PdbSignature | Field::PdbAge
        )
    });
    let debug = if need_debug {
        Some(debug_sites(img)?)
    } else {
        None
    };
    let export = if fields.contains(&Field::ExportTimestamp) {
        export_site(img)?
    } else {
        None
    };
    let cv = || {
        debug
            .and_then(|(_, cv)| cv)
            .context("clock: no CodeView debug entry")
    };
    let mut out = Vec::new();
    for &f in fields {
        let off = match f {
            Field::CoffTimestamp => img.e_lfanew.saturating_add(8),
            Field::DebugTimestamp => debug.map_or(0, |(e, _)| e).saturating_add(4),
            Field::PdbSignature => cv()?.saturating_add(8),
            Field::PdbAge => cv()?.saturating_add(12),
            Field::ExportTimestamp => match export {
                Some(o) => o,
                None => continue,
            },
        };
        out.push(Site { field: f, off });
    }
    Ok(out)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Reading {
    pub field: Field,
    pub reference: Option<u32>,
    pub ours: Option<u32>,
}

impl Reading {
    #[must_use]
    pub fn identical(&self) -> bool {
        self.reference.is_some() && self.reference == self.ours
    }
}

fn value(img: &Image, sites: &[Site], f: Field) -> Option<u32> {
    sites
        .iter()
        .find(|s| s.field == f)
        .and_then(|s| le::u32_at(img.bytes(), s.off).ok())
}

pub fn compare(ours: &Image, reference: &Image, fields: &[Field]) -> Result<Vec<Reading>> {
    let want = resolve(reference, fields)?;
    let have = resolve(ours, fields)?;
    Ok(fields
        .iter()
        .map(|&f| Reading {
            field: f,
            reference: value(reference, &want, f),
            ours: value(ours, &have, f),
        })
        .collect())
}

#[must_use]
pub fn render(readings: &[Reading]) -> (Vec<String>, bool) {
    let show = |v: Option<u32>| v.map_or_else(|| "absent".to_string(), |v| format!("{v:#010x}"));
    let mut out = Vec::new();
    for r in readings {
        out.push(format!(
            "    {:<38} ref {:<10}  ours {:<10}  {}",
            r.field.label(),
            show(r.reference),
            show(r.ours),
            if r.identical() {
                "IDENTICAL"
            } else {
                "DIFFERS"
            }
        ));
    }
    let same = readings.iter().filter(|r| r.identical()).count();
    out.push(format!(
        "    {same} of {} declared clock field(s) identical",
        readings.len()
    ));
    (out, same == readings.len())
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Applied {
    pub sites: Vec<Site>,
    pub bytes: usize,
}

pub fn apply(ours: &Image, reference: &Image, fields: &[Field]) -> Result<(Vec<u8>, Applied)> {
    let want = resolve(reference, fields)?;
    let dest = resolve(ours, fields)?;
    let mut out = ours.bytes().to_vec();
    let mut applied = Applied::default();
    for d in &dest {
        let Some(w) = want.iter().find(|w| w.field == d.field) else {
            continue;
        };
        let src = le::bytes_at(reference.bytes(), w.off, 4)
            .with_context(|| format!("the reference is too short for {}", w.field.label()))?;
        let slot = out
            .get_mut(d.off..d.off.saturating_add(4))
            .with_context(|| format!("the image is too short for {}", d.field.label()))?;
        if slot == src {
            continue;
        }
        slot.copy_from_slice(src);
        applied.sites.push(*d);
        applied.bytes = applied.bytes.saturating_add(4);
    }
    Ok((out, applied))
}
