use anyhow::Result;
use msvc6_image::anatomy::{self, Kind as PartKind};
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::BTreeMap;
use std::ops::RangeInclusive;

#[must_use]
pub fn expected_reserve(entries: usize) -> u32 {
    40u32.saturating_add(4u32.saturating_mul(u32::try_from(entries / 5).unwrap_or(u32::MAX / 8)))
}

#[must_use]
pub fn reserve_implies_entries(bytes: u32) -> Option<RangeInclusive<usize>> {
    if bytes < 40 || !(bytes - 40).is_multiple_of(4) {
        return None;
    }
    let k = usize::try_from((bytes - 40) / 4).ok()?;
    Some(k.saturating_mul(5)..=k.saturating_mul(5).saturating_add(4))
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Verdict {
    Due,
    Fossil,
    None,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Block {
    pub dll: String,
    pub table: &'static str,
    pub rva: u32,
    pub end: u32,
    pub entries: usize,
    pub due: u32,
    pub reserve: u32,
    pub verdict: Verdict,
    pub implied: Option<(usize, usize)>,
}

impl Block {
    #[must_use]
    pub fn line(&self) -> String {
        let tail = match self.verdict {
            Verdict::Due => "ok".to_string(),
            Verdict::None => "no reserve".to_string(),
            Verdict::Fossil => format!(
                "FOSSIL: {} B implies {}",
                self.reserve,
                self.implied.map_or_else(
                    || "no import count".to_string(),
                    |(a, b)| format!("{a}..{b} imports")
                )
            ),
        };
        format!(
            "  {:<16} {} {:#08x}  {:>3} import(s)  due {:>3}  reserve {:>3}  {tail}",
            self.dll, self.table, self.rva, self.entries, self.due, self.reserve
        )
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Summary {
    pub blocks: usize,
    pub due: usize,
    pub fossils: usize,
    pub none: usize,
}

pub fn blocks(img: &Image) -> Result<(Vec<Block>, Summary)> {
    let tables = img.imports()?;
    let mut reserves: BTreeMap<u32, u32> = BTreeMap::new();
    for s in anatomy::anatomy(img, &tables) {
        for p in s.parts.iter().filter(|p| p.kind == PartKind::Reserve) {
            reserves.insert(p.lo, p.len());
        }
    }
    let mut out = Vec::new();
    for t in &tables {
        let len = t.block_len();
        let due = expected_reserve(t.entries.len());
        for (table, rva) in [("ILT", t.ilt_rva.get()), ("IAT", t.iat_rva.get())] {
            if rva == 0 {
                continue;
            }
            let end = rva.saturating_add(len);
            let reserve = reserves.get(&end).copied().unwrap_or(0);
            let verdict = if reserve == 0 {
                Verdict::None
            } else if reserve == due {
                Verdict::Due
            } else {
                Verdict::Fossil
            };
            out.push(Block {
                dll: t.dll.clone(),
                table,
                rva,
                end,
                entries: t.entries.len(),
                due,
                reserve,
                verdict,
                implied: (verdict == Verdict::Fossil)
                    .then(|| reserve_implies_entries(reserve).map(|r| (*r.start(), *r.end())))
                    .flatten(),
            });
        }
    }
    let n = |v: Verdict| out.iter().filter(|b| b.verdict == v).count();
    let sum = Summary {
        blocks: out.len(),
        due: n(Verdict::Due),
        fossils: n(Verdict::Fossil),
        none: n(Verdict::None),
    };
    Ok((out, sum))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_reserve_inverts_to_the_import_count_that_would_grant_it() {
        assert_eq!(reserve_implies_entries(76), Some(45..=49));
        assert_eq!(reserve_implies_entries(68), Some(35..=39));
        assert_eq!(reserve_implies_entries(40), Some(0..=4));
        for n in 0usize..=200 {
            let bytes = expected_reserve(n);
            let r = reserve_implies_entries(bytes);
            assert!(
                r.clone().is_some_and(|r| r.contains(&n)),
                "{n} imports -> {bytes} B, which inverted to {r:?}"
            );
            for m in r.into_iter().flatten() {
                assert_eq!(expected_reserve(m), bytes, "{m} imports -> {bytes} B");
            }
        }
        assert_eq!(reserve_implies_entries(38), None);
        assert_eq!(reserve_implies_entries(70), None);
    }

    #[test]
    fn the_rule_reads_the_measured_blocks() {
        for (entries, due) in [
            (25, 60),
            (87, 108),
            (27, 60),
            (0, 40),
            (83, 104),
            (10, 48),
            (31, 64),
            (3, 40),
            (2, 40),
            (80, 104),
            (45, 76),
            (1, 40),
        ] {
            assert_eq!(expected_reserve(entries), due, "{entries}");
        }
    }
}
