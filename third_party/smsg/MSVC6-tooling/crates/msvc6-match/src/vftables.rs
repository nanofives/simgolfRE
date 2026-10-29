use anyhow::{Context as _, Result};
use msvc6_image::pe::Image;
use serde::Serialize;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Run {
    pub rva: u32,
    pub slots: Vec<u32>,
}

#[must_use]
pub fn follow(text: &[u8], text_rva: u32, rva: u32) -> u32 {
    let Some(off) = rva
        .checked_sub(text_rva)
        .and_then(|o| usize::try_from(o).ok())
    else {
        return rva;
    };
    if text.get(off) != Some(&0xe9) {
        return rva;
    }
    let Some(b) = off
        .checked_add(1)
        .and_then(|s| text.get(s..s.checked_add(4)?))
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
    else {
        return rva;
    };
    rva.wrapping_add(5).wrapping_add(u32::from_le_bytes(b))
}

pub fn detect(image: &Image, text: &str, data: &str) -> Result<Vec<Run>> {
    let base = image.opt.image_base.get();
    let tsec = image
        .section(text)
        .with_context(|| format!("the image has no {text} section"))?;
    let rsec = image
        .section(data)
        .with_context(|| format!("the image has no {data} section"))?;
    let tbytes = image.section_bytes(tsec);
    let rdata = image.section_bytes(rsec);
    let (trva, rrva) = (tsec.rva.get(), rsec.rva.get());
    let tend = trva.saturating_add(u32::try_from(tbytes.len()).unwrap_or(u32::MAX));
    let into_text = |i: usize| -> Option<u32> {
        let b = rdata.get(i..i.checked_add(4)?)?;
        let v = u32::from_le_bytes(<[u8; 4]>::try_from(b).ok()?);
        let rva = v.checked_sub(base)?;
        (rva >= trva && rva < tend).then_some(rva)
    };
    let mut runs = Vec::new();
    let mut i = 0usize;
    while i.saturating_add(4) <= rdata.len() {
        if into_text(i).is_none() {
            i = i.saturating_add(4);
            continue;
        }
        let start = i;
        let mut slots = Vec::new();
        while let Some(rva) = into_text(i) {
            slots.push(rva);
            i = i.saturating_add(4);
        }
        runs.push(Run {
            rva: rrva.saturating_add(u32::try_from(start).unwrap_or(u32::MAX)),
            slots,
        });
    }
    Ok(runs)
}
