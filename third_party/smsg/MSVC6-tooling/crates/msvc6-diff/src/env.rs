use crate::resolve::Names;
use anyhow::{Context as _, Result, anyhow};
use msvc6_base::units::Rva;
use msvc6_image::pe::{Image, Section};
use msvc6_image::postlink::Kind as Residue;
use msvc6_project::config::Project;
use msvc6_project::functions::FunctionIndex;
use std::collections::{BTreeMap, BTreeSet};

pub const UNRECORDED_PROVIDERS: [&str; 1] = ["discover"];

#[derive(Debug, Clone)]
pub struct Env {
    pub label: String,
    pub img: Image,
    sec: Section,
    pub text_rva: u32,
    pub names: Names,
    pub seats: Vec<(u32, u32)>,
    pub iat: BTreeMap<u32, String>,
    pub base: u64,
    pub top: u64,
    pub text_pad: Vec<(u32, u32)>,
    pub function_names: BTreeMap<u32, String>,
    pub dropped: BTreeMap<String, String>,
    pub starts: BTreeSet<u32>,
}

#[must_use]
pub fn recorded_seats(ix: &FunctionIndex) -> Vec<(u32, u32)> {
    ix.iter()
        .filter(|f| !f.thunk && !UNRECORDED_PROVIDERS.contains(&f.provider.as_str()))
        .filter_map(|f| Some((f.rva, f.end?.checked_sub(f.rva)?)))
        .filter(|(_, len)| *len > 0)
        .collect()
}

#[must_use]
pub fn function_names(ix: &FunctionIndex) -> BTreeMap<u32, String> {
    ix.iter()
        .filter_map(|f| Some((f.rva, f.name.clone().filter(|n| !n.is_empty())?)))
        .collect()
}

impl Env {
    pub fn load(p: &Project, n: usize) -> Result<Self> {
        let d = p.images.get(n).with_context(|| format!("no image[{n}]"))?;
        let r = p.reference(n)?;
        let img = Image::parse(r.bytes.clone())
            .with_context(|| format!("image `{}`: {}", d.id, r.path.display()))?;
        let names = Names::from_oracle(&msvc6_project::names::oracle_of(p, n, &r)?);
        let ix = msvc6_project::functions::index_of(p, n, &r)?;
        let seats = recorded_seats(&ix);
        let text_pad = d
            .residue
            .iter()
            .filter(|row| row.kind == Residue::TextPad && row.len > 0)
            .filter_map(|row| {
                let rva = row
                    .rva
                    .or_else(|| row.off.and_then(|o| img.off_to_rva(o)).map(Rva::get))?;
                Some((rva, u32::try_from(row.len).ok()?))
            })
            .collect();
        let mut env = Self::new(&d.id, img, names, seats, text_pad)?;
        env.function_names = function_names(&ix);
        env.dropped = msvc6_project::dropped::dropped_of(p, n)?;
        env.starts = ix
            .iter()
            .filter(|f| !UNRECORDED_PROVIDERS.contains(&f.provider.as_str()))
            .map(|f| f.rva)
            .collect();
        Ok(env)
    }

    pub fn new(
        label: &str,
        img: Image,
        names: Names,
        mut seats: Vec<(u32, u32)>,
        mut text_pad: Vec<(u32, u32)>,
    ) -> Result<Self> {
        let sec = img
            .section(".text")
            .ok_or_else(|| anyhow!("{label}: no .text section"))?
            .clone();
        let iat: BTreeMap<u32, String> = img
            .imports()?
            .into_iter()
            .flat_map(|dll| dll.entries.into_iter().map(|e| (e.iat_rva.get(), e.name)))
            .collect();
        let base = u64::from(img.opt.image_base.get());
        let top = base.saturating_add(u64::from(img.opt.size_of_image));
        seats.sort_unstable();
        seats.dedup_by_key(|s| s.0);
        text_pad.sort_unstable();
        Ok(Self {
            label: label.to_string(),
            text_rva: sec.rva.get(),
            img,
            sec,
            names,
            seats,
            iat,
            base,
            top,
            text_pad,
            function_names: BTreeMap::new(),
            dropped: BTreeMap::new(),
            starts: BTreeSet::new(),
        })
    }

    #[must_use]
    pub fn text(&self) -> &[u8] {
        self.img.section_bytes(&self.sec)
    }

    #[must_use]
    pub fn section(&self) -> &Section {
        &self.sec
    }

    #[must_use]
    pub fn named(&self) -> &BTreeSet<u32> {
        self.names.named()
    }

    #[must_use]
    pub fn recorded(&self, rva: u32) -> Option<u32> {
        self.seats
            .binary_search_by_key(&rva, |s| s.0)
            .ok()
            .and_then(|k| self.seats.get(k))
            .map(|s| s.1)
    }

    #[must_use]
    pub fn in_text(&self, rva: u32) -> bool {
        rva >= self.text_rva
            && usize::try_from(rva - self.text_rva).is_ok_and(|o| o < self.text().len())
    }

    pub fn rva_of(&self, func: &str) -> Result<u32> {
        let s = func.trim();
        if let Some(r) = s
            .strip_prefix("0x")
            .or_else(|| s.strip_prefix("0X"))
            .and_then(|h| u32::from_str_radix(h, 16).ok())
        {
            return Ok(r);
        }
        self.names
            .rva(s, self.text(), self.text_rva)
            .ok_or_else(|| anyhow!("`{s}` is not a 0x-hexadecimal rva and resolves to no address"))
    }

    #[must_use]
    pub fn extent(&self, rva: u32, ours_len: usize) -> (usize, &'static str) {
        if let Some(l) = self.recorded(rva) {
            return (l as usize, RECORDED);
        }
        let k = self.seats.partition_point(|s| s.0 <= rva);
        if let Some((next, _)) = self.seats.get(k) {
            return ((next - rva) as usize, NEXT_START);
        }
        (ours_len.saturating_add(ours_len / 8), UNRECORDED)
    }
}

pub const RECORDED: &str = "recorded length";
pub const NEXT_START: &str = "up to the next recorded start";
pub const UNRECORDED: &str = "NO RECORDED EXTENT -- our length plus an eighth";
