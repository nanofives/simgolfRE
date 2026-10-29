use anyhow::Result;
use msvc6_diff::comdat::REL_DIR32;
use msvc6_diff::pairing::{Aligned, slack};
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC, Object, Symbol};
use std::collections::BTreeSet;
use std::ops::Range;

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Mismatch {
    pub function: String,
    pub at: u32,
    pub va: u32,
    pub ours: Vec<u8>,
    pub image: Option<Vec<u8>>,
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Report {
    pub sites: usize,
    pub mismatched: Vec<Mismatch>,
    pub literals_defined: usize,
    pub unplaced: Vec<String>,
    pub dtor_bodies: usize,
    pub dtor_sites: usize,
    pub data_sites: usize,
    pub foreign_sites: usize,
    pub misaligned: Vec<(String, u32)>,
}

impl Report {
    pub fn add(&mut self, r: Self) {
        self.sites += r.sites;
        self.mismatched.extend(r.mismatched);
        self.literals_defined += r.literals_defined;
        self.unplaced.extend(r.unplaced);
        self.dtor_bodies += r.dtor_bodies;
        self.dtor_sites += r.dtor_sites;
        self.data_sites += r.data_sites;
        self.foreign_sites += r.foreign_sites;
        self.misaligned.extend(r.misaligned);
    }
}

fn idx(v: u32) -> Option<usize> {
    usize::try_from(v).ok()
}

fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    d.get(at..at.checked_add(4)?)
        .and_then(|b| b.try_into().ok())
        .map(u32::from_le_bytes)
}

#[must_use]
pub fn owner(o: &Object<'_>, sn: u16) -> Option<String> {
    let at0 = |class: u8| {
        o.symbols.iter().find(|s| {
            s.section_number() == Some(sn)
                && s.value == 0
                && s.class == class
                && !s.name.starts_with('.')
                && !s.name.starts_with("$L")
        })
    };
    at0(CLASS_EXTERNAL)
        .or_else(|| at0(CLASS_STATIC))
        .map(|s| s.name.clone())
}

#[must_use]
pub fn literal(o: &Object<'_>, sym: &Symbol) -> Option<Vec<u8>> {
    let sec = o.section(sym.section_number()?)?;
    let wide = sym.name.starts_with("??_C@_1");
    if sec.raw_ptr == 0 && idx(sym.value)? < sec.size as usize {
        return Some(vec![0; if wide { 2 } else { 1 }]);
    }
    let tail = o.section_data(sec).get(idx(sym.value)?..)?;
    let end = if wide {
        tail.chunks(2).position(|c| c == [0, 0]).map(|p| p * 2 + 2)
    } else {
        tail.iter().position(|c| *c == 0).map(|p| p + 1)
    };
    Some(tail.get(..end.unwrap_or(tail.len()))?.to_vec())
}

pub type Place<'a> = dyn Fn(&str) -> Option<u32> + 'a;
pub type Bytes<'a> = dyn Fn(u32, usize) -> Option<Vec<u8>> + 'a;

#[allow(clippy::too_many_lines)]
pub fn screen(
    obj: &[u8],
    image_base: u32,
    image_size: u32,
    place: &Place<'_>,
    bytes: &Bytes<'_>,
) -> Result<Report> {
    let image: Range<u64> =
        u64::from(image_base)..u64::from(image_base).saturating_add(u64::from(image_size));
    let o = msvc6_diff::comdat::parse(obj)?;
    let mut r = Report {
        literals_defined: o
            .symbols
            .iter()
            .filter(|s| s.section > 0 && s.name.starts_with("??_C@"))
            .count(),
        ..Report::default()
    };
    for sec in &o.sections {
        let sn = sec.number;
        let relocs = o.relocations(sec);
        let lits: Vec<(u32, &Symbol)> = relocs
            .iter()
            .filter(|x| x.typ == REL_DIR32)
            .filter_map(|x| o.symbol(x.symbol).map(|s| (x.va, s)))
            .filter(|(_, s)| s.name.starts_with("??_C@"))
            .collect();
        if !sec.name.starts_with(".text") {
            r.data_sites += lits.len();
            continue;
        }
        let Some(f) = owner(&o, sn) else {
            if !lits.is_empty() || sec.size > 0 {
                r.unplaced.push(format!("<unnamed section {sn}>"));
            }
            continue;
        };
        if f.starts_with("??_G") || f.starts_with("??_E") {
            r.dtor_bodies += 1;
            r.dtor_sites += lits.len();
            continue;
        }
        let Some(base) = place(&f) else {
            r.unplaced.push(f);
            continue;
        };
        let body = o.section_data(sec);
        let relocated: BTreeSet<usize> = relocs
            .iter()
            .filter_map(|x| idx(x.va))
            .flat_map(|a| a..a.saturating_add(4))
            .collect();
        let own: Vec<(u32, Option<Vec<u8>>)> = lits
            .iter()
            .map(|(at, s)| (*at, (s.section > 0).then(|| literal(&o, s)).flatten()))
            .collect();
        let mut al: Option<Aligned> = None;
        for (at, sym) in lits {
            let Some(ours) = (sym.section > 0).then(|| literal(&o, sym)).flatten() else {
                r.foreign_sites += 1;
                continue;
            };
            let a = al.get_or_insert_with(|| {
                let theirs = bytes(base, slack(body.len()))
                    .or_else(|| bytes(base, body.len()))
                    .unwrap_or_default();
                Aligned::new(body, &relocated, theirs, &image)
            });
            let addend = idx(at).and_then(|x| u32_at(body, x)).unwrap_or(0);
            let names = |k: usize, foff: u32| {
                let va = a.field(k, foff).map(|d| d.wrapping_sub(addend));
                (
                    va,
                    va.and_then(|v| bytes(v.wrapping_sub(image_base), ours.len())),
                )
            };
            let paired = a
                .holding(at)
                .and_then(|(i, foff)| Some((i, foff, a.ref_of.get(i).copied().flatten()?)));
            let Some((i, foff, j)) = paired else {
                r.misaligned.push((f.clone(), at));
                continue;
            };
            let (va, theirs) = names(j, foff);
            if theirs.as_deref() == Some(ours.as_slice()) {
                r.sites += 1;
                continue;
            }
            let key = a.ours.get(i).map(|d| d.key.as_str());
            let q = (0..a.theirs.len())
                .filter(|&k| a.theirs.get(k).map(|d| d.key.as_str()) == key)
                .filter(|&k| names(k, foff).1.as_deref() == Some(ours.as_slice()))
                .min_by_key(|k| k.abs_diff(j));
            if q.is_some_and(|q| !a.gap_free(i, j, q)) {
                r.misaligned.push((f.clone(), at));
                continue;
            }
            let p = own
                .iter()
                .filter_map(|(at2, lit)| {
                    let (p, foff2) = a.holding(*at2)?;
                    let lit = lit.as_ref()?;
                    (p != i && foff2 == foff && a.ours.get(p).map(|d| d.key.as_str()) == key)
                        .then_some((p, lit))
                })
                .filter(|(_, lit)| {
                    va.and_then(|v| bytes(v.wrapping_sub(image_base), lit.len()))
                        .as_deref()
                        == Some(lit.as_slice())
                })
                .map(|(p, _)| p)
                .min_by_key(|p| p.abs_diff(i));
            if p.is_some_and(|p| !a.gap_free_ours(i, j, p)) {
                r.misaligned.push((f.clone(), at));
                continue;
            }
            r.sites += 1;
            r.mismatched.push(Mismatch {
                function: f.clone(),
                at,
                va: va.unwrap_or(0),
                ours,
                image: theirs,
            });
        }
    }
    Ok(r)
}

#[must_use]
pub fn show(b: &[u8]) -> String {
    let s: String = b
        .iter()
        .take(40)
        .map(|c| match *c {
            0 => "\\0".to_string(),
            0x20..=0x7e => char::from(*c).to_string(),
            _ => format!("\\x{c:02x}"),
        })
        .collect();
    format!("\"{s}\"")
}

#[must_use]
pub fn object_line(stem: &str, r: &Report) -> String {
    format!(
        "{stem:24} literal sites {:4}  mismatched {}  literals defined {:3}  NOT CHECKED: {} misaligned site(s), {} unplaced text comdat(s), {} ??_G/??_E bod(ies) with {} site(s), {} data-section site(s), {} foreign-literal site(s)",
        r.sites,
        r.mismatched.len(),
        r.literals_defined,
        r.misaligned.len(),
        r.unplaced.len(),
        r.dtor_bodies,
        r.dtor_sites,
        r.data_sites,
        r.foreign_sites,
    )
}

#[must_use]
pub fn mismatch_line(stem: &str, m: &Mismatch) -> String {
    format!(
        "  MISMATCH {stem}: {} +{:#x} -> {:#x}  ours {}  reference {}",
        m.function,
        m.at,
        m.va,
        show(&m.ours),
        m.image
            .as_deref()
            .map_or_else(|| "<no on-disk bytes>".to_string(), show)
    )
}

#[must_use]
pub fn total_lines(objects: usize, t: &Report) -> Vec<String> {
    vec![
        format!(
            "TOTAL {objects} object(s): literal sites {}  mismatched {}  literals defined {}",
            t.sites,
            t.mismatched.len(),
            t.literals_defined
        ),
        format!(
            "NOT CHECKED: {} site(s) where our body's layout differs from the reference's; {} unplaced text comdat(s); {} ??_G/??_E bod(ies) holding {} literal site(s); {} literal site(s) in data sections (string tables); {} site(s) naming a literal defined elsewhere",
            t.misaligned.len(),
            t.unplaced.len(),
            t.dtor_bodies,
            t.dtor_sites,
            t.data_sites,
            t.foreign_sites
        ),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_fixture::coff::{Object as Writer, Reloc, SCN_DATA, SCN_TEXT, Section};

    fn object_with(func: &str, text: &[u8], trelocs: &[(u32, u32)]) -> Vec<u8> {
        let mut o = Writer::new();
        let t = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT,
            data: text.to_vec(),
            ..Section::default()
        });
        let d = o.section(Section {
            name: ".data".into(),
            characteristics: SCN_DATA,
            data: b"one\0two\0".to_vec(),
            ..Section::default()
        });
        let tab = o.section(Section {
            name: ".data".into(),
            characteristics: SCN_DATA,
            data: vec![0; 4],
            ..Section::default()
        });
        o.external(func, 0, t);
        o.external("??_C@_03one@", 0, d);
        o.external("??_C@_03two@", 4, d);
        o.external("?table@@3PAPADA", 0, tab);
        o.sections[0].relocs = trelocs
            .iter()
            .map(|&(va, symbol)| Reloc {
                va,
                symbol,
                typ: REL_DIR32,
            })
            .collect();
        o.sections[2].relocs = vec![Reloc {
            va: 0,
            symbol: 1,
            typ: REL_DIR32,
        }];
        o.build()
    }

    fn object(func: &str) -> Vec<u8> {
        object_with(
            func,
            &[0xb8, 0, 0, 0, 0, 0xb9, 0, 0, 0, 0, 0xc3],
            &[(1, 1), (6, 2)],
        )
    }

    const SIZE: u32 = 0x1_0000;
    const ONE: [u8; 4] = [0x00, 0x20, 0x40, 0x00];
    const TWO: [u8; 4] = [0x04, 0x20, 0x40, 0x00];

    fn reference(code: Vec<u8>) -> impl Fn(u32, usize) -> Option<Vec<u8>> {
        move |rva, n| {
            let (d, at) = if rva >= 0x2000 {
                (b"one\0two\0".to_vec(), rva.checked_sub(0x2000)?)
            } else {
                (code.clone(), rva.checked_sub(0x1000)?)
            };
            let at = usize::try_from(at).ok()?;
            Some(d.get(at..(at + n).min(d.len()))?.to_vec())
        }
    }

    fn two_movs(a: [u8; 4], b: [u8; 4]) -> Vec<u8> {
        [&[0xb8][..], &a, &[0xb9], &b, &[0xc3]].concat()
    }

    fn strlen_idiom(lit: [u8; 4]) -> Vec<u8> {
        [&[0xbf][..], &lit, &[0x83, 0xc9, 0xff, 0x33, 0xc0]].concat()
    }

    fn idioms(lits: &[u32]) -> Vec<u8> {
        let mut text = Vec::new();
        let mut rel = Vec::new();
        for (n, &l) in lits.iter().enumerate() {
            rel.push((u32::try_from(n * 10 + 1).unwrap_or(0), l));
            text.extend(strlen_idiom([0; 4]));
        }
        text.push(0xc3);
        object_with("?F@@YAXXZ", &text, &rel)
    }

    fn place(f: &str) -> Option<u32> {
        (f == "?F@@YAXXZ").then_some(0x1000)
    }

    fn ok(r: Result<Report>) -> Report {
        match r {
            Ok(r) => r,
            Err(e) => panic!("screen failed: {e:#}"),
        }
    }

    #[test]
    fn identical_literals_pass_and_a_data_table_is_counted_not_checked() {
        let r = ok(screen(
            &object("?F@@YAXXZ"),
            0x40_0000,
            SIZE,
            &place,
            &reference(two_movs(ONE, TWO)),
        ));
        assert_eq!(r.sites, 2);
        assert!(r.mismatched.is_empty(), "{:?}", r.mismatched);
        assert_eq!(r.literals_defined, 2);
        assert_eq!(r.data_sites, 1);
    }

    #[test]
    fn swapped_literals_are_both_reported() {
        let r = ok(screen(
            &object("?F@@YAXXZ"),
            0x40_0000,
            SIZE,
            &place,
            &reference(two_movs(TWO, ONE)),
        ));
        assert_eq!(r.sites, 2);
        assert_eq!(r.mismatched.len(), 2);
        assert_eq!(
            r.mismatched
                .first()
                .map(|m| (m.at, m.va, m.ours.clone(), m.image.clone())),
            Some((1, 0x40_2004, b"one\0".to_vec(), Some(b"two\0".to_vec())))
        );
    }

    #[test]
    fn swapped_idioms_with_no_drift_are_both_reported() {
        let code = [strlen_idiom(TWO), strlen_idiom(ONE), vec![0xc3]].concat();
        let r = ok(screen(
            &idioms(&[1, 2]),
            0x40_0000,
            SIZE,
            &place,
            &reference(code),
        ));
        assert_eq!((r.sites, r.mismatched.len(), r.misaligned.len()), (2, 2, 0));
    }

    #[test]
    fn a_drifted_site_in_a_shared_idiom_is_not_a_mismatch() {
        let code = [
            strlen_idiom(ONE),
            strlen_idiom(ONE),
            strlen_idiom(TWO),
            vec![0xc3],
        ]
        .concat();
        let r = ok(screen(
            &idioms(&[1, 2]),
            0x40_0000,
            SIZE,
            &place,
            &reference(code),
        ));
        assert!(r.mismatched.is_empty(), "{:?}", r.mismatched);
        assert_eq!(r.sites + r.misaligned.len(), 2);
        let code = [strlen_idiom(ONE), strlen_idiom(TWO), vec![0xc3]].concat();
        let r = ok(screen(
            &idioms(&[1, 1, 2]),
            0x40_0000,
            SIZE,
            &place,
            &reference(code),
        ));
        assert!(r.mismatched.is_empty(), "{:?}", r.mismatched);
        assert_eq!(r.sites + r.misaligned.len(), 3);
    }

    #[test]
    fn unplaced_and_dtor_bodies_are_not_checked_not_dropped() {
        let img = reference(two_movs(TWO, ONE));
        let r = ok(screen(
            &object("?F@@YAXXZ"),
            0x40_0000,
            SIZE,
            &|_| None,
            &img,
        ));
        assert_eq!((r.sites, r.unplaced.len()), (0, 1));
        let r = ok(screen(
            &object("??_GFoo@@UAEPAXI@Z"),
            0x40_0000,
            SIZE,
            &|_| Some(0x1000),
            &img,
        ));
        assert_eq!((r.sites, r.dtor_bodies, r.dtor_sites), (0, 1, 2));
    }

    #[test]
    fn a_site_outside_the_reference_is_a_mismatch_and_a_shifted_body_is_paired() {
        let far = [0x00, 0x90, 0x40, 0x00];
        let r = ok(screen(
            &object("?F@@YAXXZ"),
            0x40_0000,
            SIZE,
            &place,
            &reference(two_movs(far, far)),
        ));
        assert_eq!(r.mismatched.len(), 2);
        assert!(r.mismatched.iter().all(|m| m.image.is_none()));
        let shifted = [vec![0x90], two_movs(ONE, TWO)].concat();
        let r = ok(screen(
            &object("?F@@YAXXZ"),
            0x40_0000,
            SIZE,
            &place,
            &reference(shifted),
        ));
        assert_eq!((r.sites, r.mismatched.len(), r.misaligned.len()), (2, 0, 0));
        let pushes = [&[0x68][..], &ONE, &[0x68], &TWO, &[0xc3]].concat();
        let r = ok(screen(
            &object("?F@@YAXXZ"),
            0x40_0000,
            SIZE,
            &place,
            &reference(pushes),
        ));
        assert_eq!((r.sites, r.mismatched.len(), r.misaligned.len()), (0, 0, 2));
    }
}
