use msvc6_base::units::Rva;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::pe::{Dir, Image, ImportDll, ImportEntry};
use crate::{u32_of, us};

pub const PAD_GAP: u32 = 16;
pub const HINT_NAME: &str = "hint/name strings";

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Kind {
    Table,
    Padding,
    Reserve,
    Content,
    Tail,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Part {
    pub lo: u32,
    pub hi: u32,
    pub kind: Kind,
    pub label: String,
}

impl Part {
    #[must_use]
    pub fn len(&self) -> u32 {
        self.hi.saturating_sub(self.lo)
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.hi <= self.lo
    }

    #[must_use]
    pub fn line(&self) -> String {
        let text = match self.kind {
            Kind::Table => self.label.clone(),
            Kind::Padding => "(linker padding)".to_string(),
            Kind::Reserve => "(reserve, zero-filled)".to_string(),
            Kind::Content => "<-- CONTENT, ours to supply".to_string(),
            Kind::Tail => format!("tail ({})", self.label),
        };
        format!(
            "    {:#08x}..{:#08x} {:>7}  {text}",
            self.lo,
            self.hi,
            self.len()
        )
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SectionAnatomy {
    pub section: String,
    pub lo: u32,
    pub size: u32,
    pub generated: u32,
    pub parts: Vec<Part>,
}

impl SectionAnatomy {
    #[must_use]
    pub fn lines(&self) -> Vec<String> {
        let mut out = vec![format!(
            "  {} anatomy: {:#x}..{:#x} ({} bytes)",
            self.section,
            self.lo,
            self.lo.saturating_add(self.size),
            self.size
        )];
        out.extend(self.parts.iter().map(Part::line));
        let pct = f64::from(self.generated) * 100.0 / f64::from(self.size.max(1));
        out.push(format!(
            "    link.exe generates {} of {} bytes ({pct:.1}%)",
            self.generated, self.size
        ));
        out
    }
}

struct Span {
    lo: u32,
    hi: u32,
    label: Option<String>,
}

fn hint_name_len(e: &ImportEntry) -> u32 {
    2u32.saturating_add(u32_of(e.name.len())).saturating_add(1)
}

#[must_use]
pub fn anatomy(img: &Image, tables: &[ImportDll]) -> Vec<SectionAnatomy> {
    let mut spans: Vec<Span> = Vec::new();
    let mut push = |lo: u32, hi: u32, label: Option<String>| {
        if hi > lo {
            spans.push(Span { lo, hi, label });
        }
    };
    for t in tables {
        let len = t.block_len();
        if t.ilt_rva.get() != 0 {
            push(
                t.ilt_rva.get(),
                t.ilt_rva.get().saturating_add(len),
                Some(format!("ILT {}", t.dll)),
            );
        }
        push(
            t.iat_rva.get(),
            t.iat_rva.get().saturating_add(len),
            Some(format!("IAT {}", t.dll)),
        );
        push(
            t.name_rva.get(),
            t.name_rva
                .get()
                .saturating_add(u32_of(t.dll.len()).saturating_add(1)),
            Some(format!("dll name {}", t.dll)),
        );
        for e in &t.entries {
            if let Some(h) = e.hint_rva {
                push(h.get(), h.get().saturating_add(hint_name_len(e)), None);
            }
        }
    }
    for (which, label) in [
        (Dir::Import, "import descriptors"),
        (Dir::Export, "export directory"),
    ] {
        if let Some(d) = img.present(which) {
            push(
                d.rva.get(),
                d.rva.get().saturating_add(d.size),
                Some(label.to_string()),
            );
        }
    }
    spans.sort_by_key(|s| (s.lo, s.hi));
    let mut merged: Vec<Span> = Vec::new();
    for s in spans {
        match merged.last_mut() {
            Some(prev)
                if s.label.is_none()
                    && prev.label.as_deref() == Some(HINT_NAME)
                    && s.lo <= prev.hi.saturating_add(4) =>
            {
                prev.hi = prev.hi.max(s.hi);
            }
            _ => merged.push(Span {
                lo: s.lo,
                hi: s.hi,
                label: Some(s.label.unwrap_or_else(|| HINT_NAME.to_string())),
            }),
        }
    }
    let mut by_section: BTreeMap<String, Vec<&Span>> = BTreeMap::new();
    for s in &merged {
        if let Some(sec) = img.section_of(Rva(s.lo)) {
            by_section.entry(sec.name.clone()).or_default().push(s);
        }
    }
    let mut out = Vec::new();
    for (name, list) in by_section {
        let Some(sec) = img.section(&name) else {
            continue;
        };
        let (lo, size) = (sec.rva.get(), sec.virt_size.get());
        let end = lo.saturating_add(size);
        let zero_at = |a: u32, b: u32| {
            img.at_rva(Rva(a), us(b.saturating_sub(a)))
                .is_some_and(|x| !x.is_empty() && x.iter().all(|c| *c == 0))
        };
        let mut parts = Vec::new();
        let mut generated = 0u32;
        let mut cur = lo;
        for s in list {
            if s.lo > cur {
                let gap = s.lo.saturating_sub(cur);
                let blank = zero_at(cur, s.lo);
                let kind = if cur > lo && gap < PAD_GAP && blank {
                    Kind::Padding
                } else if blank {
                    Kind::Reserve
                } else {
                    Kind::Content
                };
                if kind != Kind::Content {
                    generated = generated.saturating_add(gap);
                }
                parts.push(Part {
                    lo: cur,
                    hi: s.lo,
                    kind,
                    label: String::new(),
                });
            }
            parts.push(Part {
                lo: s.lo,
                hi: s.hi,
                kind: Kind::Table,
                label: s.label.clone().unwrap_or_default(),
            });
            generated = generated.saturating_add(s.hi.saturating_sub(s.lo));
            cur = cur.max(s.hi);
        }
        if cur < end {
            let tail = img
                .at_rva(Rva(cur), us(end.saturating_sub(cur)))
                .unwrap_or_default();
            let label = if tail.iter().all(|b| *b == 0) {
                "all zero".to_string()
            } else {
                let txt: String = tail
                    .iter()
                    .take(20)
                    .map(|b| {
                        if b.is_ascii_graphic() {
                            char::from(*b)
                        } else {
                            '.'
                        }
                    })
                    .collect();
                format!("NONZERO residue '{txt}'")
            };
            parts.push(Part {
                lo: cur,
                hi: end,
                kind: Kind::Tail,
                label,
            });
        }
        out.push(SectionAnatomy {
            section: name,
            lo,
            size,
            generated,
            parts,
        });
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Orders {
    pub descriptors: Vec<String>,
    pub names: Vec<String>,
    pub ilt: Vec<String>,
    pub iat: Vec<String>,
}

#[must_use]
pub fn orders(tables: &[ImportDll]) -> Orders {
    let by = |key: fn(&ImportDll) -> u32| {
        let mut v: Vec<&ImportDll> = tables.iter().collect();
        v.sort_by_key(|t| key(t));
        v.iter().map(|t| t.dll.clone()).collect()
    };
    Orders {
        descriptors: tables.iter().map(|t| t.dll.clone()).collect(),
        names: by(|t| t.name_rva.get()),
        ilt: by(|t| t.ilt_rva.get()),
        iat: by(|t| t.iat_rva.get()),
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct MemberOrder {
    pub dll: String,
    pub in_library: usize,
    pub named: usize,
    pub slot_order: bool,
    pub name_order: bool,
}

#[must_use]
pub fn member_order(t: &ImportDll, library_order: &[String]) -> MemberOrder {
    let named: Vec<&ImportEntry> = t.entries.iter().filter(|e| !e.by_ordinal()).collect();
    let present: BTreeSet<&str> = named.iter().map(|e| e.name.as_str()).collect();
    let sub: Vec<&str> = library_order
        .iter()
        .map(String::as_str)
        .filter(|s| present.contains(s))
        .collect();
    let by_slot: Vec<&str> = named.iter().map(|e| e.name.as_str()).collect();
    let mut sorted = named.clone();
    sorted.sort_by_key(|e| e.hint_rva.map_or(0, Rva::get));
    let by_addr: Vec<&str> = sorted.iter().map(|e| e.name.as_str()).collect();
    MemberOrder {
        dll: t.dll.clone(),
        in_library: sub.len(),
        named: by_slot.len(),
        slot_order: by_slot == sub,
        name_order: by_addr == sub,
    }
}
