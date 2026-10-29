use msvc6_base::digest::{differ, md5_hex};
use serde::Serialize;

use crate::clock::RELINK_NOISE_BYTES;
use crate::pe::Image;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SectionCost {
    pub name: String,
    pub reference: usize,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ours: Option<usize>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub differing: Option<usize>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Summary {
    pub reference_size: usize,
    pub reference_md5: String,
    pub candidate_size: usize,
    pub candidate_md5: String,
    pub md5_identical: bool,
    pub differing: usize,
    pub sections_sum: usize,
}

#[must_use]
pub fn section_costs(reference: &Image, ours: &Image) -> Vec<SectionCost> {
    reference
        .sections
        .iter()
        .map(|s| match ours.section(&s.name) {
            None => SectionCost {
                name: s.name.clone(),
                reference: s.disk_len(),
                ours: None,
                differing: None,
            },
            Some(o) => {
                let (a, b) = (reference.section_bytes(s), ours.section_bytes(o));
                SectionCost {
                    name: s.name.clone(),
                    reference: a.len(),
                    ours: Some(b.len()),
                    differing: Some(differ(a, b)),
                }
            }
        })
        .collect()
}

#[must_use]
pub fn summary(reference: &[u8], candidate: &[u8], costs: &[SectionCost]) -> Summary {
    let (rmd5, cmd5) = (md5_hex(reference), md5_hex(candidate));
    Summary {
        reference_size: reference.len(),
        candidate_size: candidate.len(),
        md5_identical: rmd5 == cmd5,
        reference_md5: rmd5,
        candidate_md5: cmd5,
        differing: differ(reference, candidate),
        sections_sum: costs
            .iter()
            .filter_map(|c| c.differing)
            .fold(0, usize::saturating_add),
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub enum Delta {
    ClockBand,
    Better,
    Worse,
}

#[must_use]
pub fn baseline_verdict(delta: i64) -> Delta {
    if usize::try_from(delta.unsigned_abs()).unwrap_or(usize::MAX) <= RELINK_NOISE_BYTES {
        Delta::ClockBand
    } else if delta < 0 {
        Delta::Better
    } else {
        Delta::Worse
    }
}

#[must_use]
pub fn signed(n: usize) -> i64 {
    i64::try_from(n).unwrap_or(i64::MAX)
}

#[must_use]
pub fn percent(part: usize, whole: usize) -> f64 {
    let p = u32::try_from(part).map_or(f64::MAX, f64::from);
    let w = u32::try_from(whole.max(1)).map_or(f64::MAX, f64::from);
    100.0 * p / w
}
