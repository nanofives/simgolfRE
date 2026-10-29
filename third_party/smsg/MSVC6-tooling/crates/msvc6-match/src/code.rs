use anyhow::{Result, bail};
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC, Object, RELOC, section_align};
use std::collections::BTreeMap;

pub const SECTION: &str = ".text";
pub const FIELD: usize = 4;
pub const REL_DIR32: u16 = 6;
pub const REL_DIR32NB: u16 = 7;
pub const REL_REL32: u16 = 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Reloc {
    pub at: u32,
    pub symbol: String,
    pub kind: u16,
    pub external: bool,
    pub global: bool,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Code {
    pub index: u16,
    pub symbol: Option<String>,
    pub external: bool,
    pub bytes: Vec<u8>,
    pub relocated: Vec<bool>,
    pub comdat: bool,
    pub align: u32,
    pub relocations: Vec<Reloc>,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct SectionSymbol {
    pub offset: u32,
    pub name: String,
    pub external: bool,
}

#[must_use]
pub fn u32_of(n: usize) -> u32 {
    u32::try_from(n).unwrap_or(u32::MAX)
}

impl Code {
    #[must_use]
    pub fn len32(&self) -> u32 {
        u32_of(self.bytes.len())
    }

    #[must_use]
    pub fn literal_len(&self) -> usize {
        self.relocated.iter().filter(|r| !**r).count()
    }

    #[must_use]
    pub fn anchor(&self) -> Option<(usize, &[u8])> {
        let mut best: Option<(usize, usize)> = None;
        let mut i = 0usize;
        let n = self.relocated.len();
        while i < n {
            if self.relocated.get(i).copied().unwrap_or(true) {
                i = i.saturating_add(1);
                continue;
            }
            let mut j = i;
            while j < n && !self.relocated.get(j).copied().unwrap_or(true) {
                j = j.saturating_add(1);
            }
            if best.is_none_or(|(a, b)| j.saturating_sub(i) > b.saturating_sub(a)) {
                best = Some((i, j));
            }
            i = j;
        }
        let (a, b) = best?;
        Some((a, self.bytes.get(a..b)?))
    }

    #[must_use]
    pub fn masked(&self, window: &[u8], budget: &mut u64) -> Option<bool> {
        if window.len() != self.bytes.len() {
            return Some(false);
        }
        for ((a, b), m) in window.iter().zip(&self.bytes).zip(&self.relocated) {
            *budget = budget.checked_sub(1)?;
            if !*m && a != b {
                return Some(false);
            }
        }
        Some(true)
    }

    #[must_use]
    pub fn hex(&self) -> String {
        self.bytes
            .iter()
            .zip(&self.relocated)
            .map(|(b, r)| {
                if *r {
                    "..".to_string()
                } else {
                    format!("{b:02x}")
                }
            })
            .collect::<Vec<_>>()
            .join(" ")
    }
}

pub fn sections(o: &Object<'_>) -> Result<Vec<Code>> {
    let size = o.data().len();
    let wanted: Vec<_> = o.sections.iter().filter(|s| s.name == SECTION).collect();
    let relocs = wanted
        .iter()
        .map(|s| usize::from(s.relocs))
        .fold(0usize, usize::saturating_add);
    if relocs.saturating_mul(RELOC) > size {
        bail!(
            "{relocs} relocation(s) in its {SECTION} sections cannot fit its {size} bytes: the tables overlap"
        );
    }
    let mut owner: BTreeMap<u16, (String, bool)> = BTreeMap::new();
    for s in &o.symbols {
        let Some(sec) = s.section_number() else {
            continue;
        };
        if s.name.is_empty() {
            continue;
        }
        let is_ext = s.class == CLASS_EXTERNAL;
        match owner.get(&sec) {
            Some((_, true)) => {}
            _ if is_ext => {
                owner.insert(sec, (s.name.clone(), true));
            }
            None if !s.name.starts_with('.') && s.class == CLASS_STATIC => {
                owner.insert(sec, (s.name.clone(), false));
            }
            _ => {}
        }
    }
    let mut total = 0usize;
    let mut out = Vec::with_capacity(wanted.len());
    for s in wanted {
        let bytes = o.section_data(s).to_vec();
        if s.raw_ptr != 0 && bytes.len() != msvc6_image::us(s.size) {
            bail!(
                "its {SECTION} section {} holds {} bytes at {:#x}, past its {size} bytes",
                s.number,
                s.size,
                s.raw_ptr
            );
        }
        total = total.saturating_add(bytes.len());
        if total > size {
            bail!("its {SECTION} sections overlap: {total} bytes in {size}");
        }
        let mut relocated = vec![false; bytes.len()];
        let mut relocations = Vec::new();
        for r in o.relocations(s) {
            let at = msvc6_image::us(r.va);
            for k in 0..FIELD {
                if let Some(slot) = relocated.get_mut(at.saturating_add(k)) {
                    *slot = true;
                }
            }
            let (symbol, external, global) = o.symbol(r.symbol).map_or_else(
                || (String::new(), false, false),
                |t| (t.name.clone(), t.section == 0, t.class == CLASS_EXTERNAL),
            );
            relocations.push(Reloc {
                at: r.va,
                symbol,
                kind: r.typ,
                external,
                global,
            });
        }
        let own = owner.get(&s.number);
        out.push(Code {
            index: s.number,
            symbol: own.map(|(n, _)| n.clone()),
            external: own.is_some_and(|(_, e)| *e),
            bytes,
            relocated,
            comdat: s.is_comdat(),
            align: section_align(s.characteristics),
            relocations,
        });
    }
    Ok(out)
}

#[must_use]
pub fn section_symbols(o: &Object<'_>) -> BTreeMap<u16, Vec<SectionSymbol>> {
    o.section_symbols()
        .into_iter()
        .map(|(n, v)| {
            (
                n,
                v.into_iter()
                    .map(|(offset, name, external)| SectionSymbol {
                        offset,
                        name,
                        external,
                    })
                    .collect(),
            )
        })
        .collect()
}
