use crate::archive::Library;
use crate::buf::n32;
use crate::coff::{Object, REL_REL32, Reloc, SCN_COMDAT, SCN_TEXT, SELECT_NODUPLICATES, Section};
use crate::pe::{Pe, SCN_CODE};

pub const TEXT: u32 = 0x1000;
pub const BASE: u32 = 0x1000_0000;
pub const AT_A: u32 = TEXT;
pub const AT_C: u32 = TEXT + 0x20;
pub const AT_HELPER: u32 = TEXT + 0x40;
pub const AT_D1: u32 = TEXT + 0x50;
pub const AT_D2: u32 = TEXT + 0x60;
pub const AT_E: u32 = TEXT + 0x70;
pub const AT_W: u32 = TEXT + 0x80;
pub const AT_DECOY: u32 = TEXT + 0x90;
pub const AT_D2_AGAIN: u32 = TEXT + 0xa0;
pub const TEXT_LEN: usize = 0x100;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Body {
    pub symbol: String,
    pub bytes: Vec<u8>,
    pub calls: Vec<(u32, String)>,
}

#[must_use]
pub fn body(symbol: &str, bytes: &[u8], calls: &[(u32, &str)]) -> Body {
    Body {
        symbol: symbol.to_string(),
        bytes: bytes.to_vec(),
        calls: calls.iter().map(|(a, n)| (*a, (*n).to_string())).collect(),
    }
}

#[must_use]
pub fn object(bodies: &[Body]) -> Vec<u8> {
    let mut o = Object::new();
    o.comp_id(0x0a, 8168);
    for b in bodies {
        let t = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: b.bytes.clone(),
            ..Section::default()
        });
        o.section_symbol(t, SELECT_NODUPLICATES, 0);
        o.external(&b.symbol, 0, t);
        for (at, name) in &b.calls {
            let symbol = o.undefined(name);
            let k = usize::try_from(t.saturating_sub(1)).unwrap_or(usize::MAX);
            if let Some(s) = o.sections.get_mut(k) {
                s.relocs.push(Reloc {
                    va: *at,
                    symbol,
                    typ: REL_REL32,
                });
            }
        }
    }
    o.build()
}

#[must_use]
pub fn library(members: &[(&str, Vec<u8>)]) -> Vec<u8> {
    let mut l = Library::default();
    for (n, d) in members {
        l.member(n, d.clone(), &[]);
    }
    l.build()
}

pub fn put(text: &mut [u8], at: u32, b: &Body, targets: &[(&str, u32)]) {
    let off = usize::try_from(at.saturating_sub(TEXT)).unwrap_or(usize::MAX);
    if let Some(dst) = text.get_mut(off..off.saturating_add(b.bytes.len())) {
        dst.copy_from_slice(&b.bytes);
    }
    for (field, name) in &b.calls {
        let Some(to) = targets.iter().find(|(n, _)| n == name).map(|(_, a)| *a) else {
            continue;
        };
        let site = at.saturating_add(*field);
        let rel = to.wrapping_sub(site.wrapping_add(4));
        let f = off.saturating_add(usize::try_from(*field).unwrap_or(usize::MAX));
        if let Some(dst) = text.get_mut(f..f.saturating_add(4)) {
            dst.copy_from_slice(&rel.to_le_bytes());
        }
    }
}

#[must_use]
pub fn image(text: Vec<u8>) -> Vec<u8> {
    let mut pe = Pe::dll();
    pe.push(".text", SCN_CODE, text);
    pe.build()
}

#[must_use]
pub fn a() -> Body {
    body(
        "_alpha",
        &[
            0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0xe8, 0, 0, 0, 0, 0x85, 0xc0, 0x74, 0x05, 0xb8,
            0x11, 0x22, 0x33, 0x44, 0x8b, 0xe5, 0x5d, 0xc3, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
            0xa7, 0xa8,
        ],
        &[(7, "_helper")],
    )
}

#[must_use]
pub fn c() -> Body {
    body(
        "_gamma",
        &[
            0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c,
            0x8b, 0x74, 0x24, 0x10, 0xf3, 0xa4, 0x5f, 0x5e, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
            0xc9, 0xca, 0xcb, 0xcd,
        ],
        &[],
    )
}

#[must_use]
pub fn helper() -> Body {
    body(
        "_helper",
        &[
            0x8b, 0x44, 0x24, 0x04, 0x0f, 0xaf, 0xc0, 0x40, 0xd1, 0xe0, 0x2d, 0x07, 0x00, 0x00,
            0x00, 0xc3,
        ],
        &[],
    )
}

#[must_use]
pub fn straddler() -> Body {
    let mut b: Vec<u8> = a().bytes.get(0x18..0x20).unwrap_or_default().to_vec();
    b.extend_from_slice(c().bytes.get(..8).unwrap_or_default());
    body("_straddle", &b, &[])
}

#[must_use]
pub fn d1() -> Body {
    body(
        "_delta",
        &[
            0x31, 0xd2, 0x8b, 0x4c, 0x24, 0x08, 0x8b, 0x44, 0x24, 0x04, 0xf7, 0xf1, 0x92, 0xc3,
            0x90, 0x90,
        ],
        &[],
    )
}

#[must_use]
pub fn d2() -> Body {
    body("_delta_zero", &[0x33, 0xc0, 0x40, 0xc3], &[])
}

#[must_use]
pub fn e() -> Body {
    body(
        "_epsilon",
        &[
            0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
            0xef, 0xc3,
        ],
        &[],
    )
}

#[must_use]
pub fn w() -> Body {
    body(
        "_trap",
        &[0x6a, 0x02, 0xe8, 0, 0, 0, 0, 0x59, 0xc3],
        &[(3, "_helper")],
    )
}

#[must_use]
pub fn decoy() -> Body {
    body(
        "_decoy",
        &[0x6a, 0x02, 0xe8, 0, 0, 0, 0, 0x59, 0xc3],
        &[(3, "_alpha")],
    )
}

#[must_use]
pub fn gone() -> Body {
    body("_gone", &[0x0f, 0x0b, 0x0f, 0x0b, 0x0f, 0x0b, 0xc3], &[])
}

#[must_use]
pub fn linked() -> Vec<u8> {
    let targets = [("_helper", AT_HELPER), ("_alpha", AT_A)];
    let mut t = vec![0xcc; TEXT_LEN];
    for (at, b) in [
        (AT_A, a()),
        (AT_C, c()),
        (AT_HELPER, helper()),
        (AT_D1, d1()),
        (AT_D2, d2()),
        (AT_E, e()),
        (AT_W, w()),
        (AT_DECOY, decoy()),
        (AT_D2_AGAIN, d2()),
    ] {
        put(&mut t, at, &b, &targets);
    }
    t
}

#[must_use]
pub fn archive() -> Vec<u8> {
    library(&[
        ("obj\\a.obj", object(&[a()])),
        ("obj\\c.obj", object(&[c()])),
        ("obj\\helper.obj", object(&[helper()])),
        ("obj\\s.obj", object(&[straddler()])),
        ("obj\\d.obj", object(&[d1(), d2()])),
        ("obj\\e.obj", object(&[e()])),
        ("obj\\w.obj", object(&[w()])),
        ("obj\\gone.obj", object(&[gone()])),
    ])
}

#[must_use]
pub fn older_archive() -> Vec<u8> {
    let mut longer = a().bytes;
    longer.extend_from_slice(&[0x90; 0x30]);
    library(&[
        (
            "obj\\a.obj",
            object(&[body("_alpha", &longer, &[(7, "_helper")])]),
        ),
        ("obj\\c.obj", object(&[c()])),
        ("obj\\helper.obj", object(&[helper()])),
    ])
}

#[must_use]
pub fn covered() -> u32 {
    n32(0x20 + 0x20 + 0x10 + 0x14 + 0x10 + 9)
}
