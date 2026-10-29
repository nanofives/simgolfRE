#![allow(
    dead_code,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing
)]

use msvc6_base::srcio;
use msvc6_diff::env::Env;
use msvc6_diff::resolve::Names;
use msvc6_fixture::coff::{
    Object, REL_DIR32, REL_REL32, Reloc, SCN_COMDAT, SCN_TEXT, SELECT_NODUPLICATES, Section,
};
use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_DATA};
use msvc6_image::pe::Image;
use msvc6_project::names::{Kind, Name, NameOracle};
use std::path::{Path, PathBuf};

pub const BASE: u32 = 0x0040_0000;
pub const F: u32 = 0x1000;
pub const RIGHT: u32 = 0x1020;
pub const WRONG: u32 = 0x1030;
pub const DATA: u32 = 0x2000;
pub const A: u32 = DATA;
pub const B: u32 = DATA + 4;
pub const F_NAME: &str = "?f@@YAXXZ";

pub fn f_body() -> Vec<u8> {
    vec![
        0x55, 0x8b, 0xec, 0xa1, 0, 0, 0, 0, 0x50, 0xe8, 0, 0, 0, 0, 0x83, 0xc4, 0x04, 0x5d, 0xc3,
    ]
}

pub const RIGHT_BODY: [u8; 6] = [0xb8, 1, 0, 0, 0, 0xc3];
pub const WRONG_BODY: [u8; 6] = [0xb8, 2, 0, 0, 0, 0xc3];

pub fn linked_text(global: u32, callee: u32) -> Vec<u8> {
    let mut f = f_body();
    f[4..8].copy_from_slice(&(BASE + global).to_le_bytes());
    let rel = callee.wrapping_sub(F + 0x0e);
    f[0x0a..0x0e].copy_from_slice(&rel.to_le_bytes());
    let mut t = vec![0xcc; 0x40];
    t[..f.len()].copy_from_slice(&f);
    t[0x20..0x26].copy_from_slice(&RIGHT_BODY);
    t[0x30..0x36].copy_from_slice(&WRONG_BODY);
    t
}

pub fn image(global: u32, callee: u32) -> Image {
    let mut pe = Pe::exe();
    assert_eq!(pe.push(".text", SCN_CODE, linked_text(global, callee)), F);
    let mut d = 0x11u32.to_le_bytes().to_vec();
    d.extend_from_slice(&0x22u32.to_le_bytes());
    assert_eq!(pe.push(".data", SCN_DATA, d), DATA);
    Image::parse(pe.build()).unwrap()
}

pub fn names() -> Names {
    let row = |rva: u32, n: &str| Name {
        rva,
        name: n.into(),
        kind: Kind::Label,
        size: None,
        scope: None,
        provider: "tsv".into(),
    };
    let mut o = NameOracle::default();
    o.add(
        "tsv",
        "names.tsv",
        vec![
            row(F, F_NAME),
            row(RIGHT, "_right"),
            row(WRONG, "_wrong"),
            row(A, "_a"),
            row(B, "_b"),
        ],
        Vec::new(),
    );
    Names::from_oracle(&o)
}

pub fn env() -> Env {
    Env::new(
        "FIX",
        image(A, RIGHT),
        names(),
        vec![(F, 0x13), (RIGHT, 6), (WRONG, 6)],
        Vec::new(),
    )
    .unwrap()
}

pub fn caller(global: &str, callee: &str) -> Vec<u8> {
    let mut o = Object::new();
    let f = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: f_body(),
        ..Section::default()
    });
    o.section_symbol(f, SELECT_NODUPLICATES, 0);
    o.external(F_NAME, 0, f);
    let a = o.undefined("_a");
    let b = o.undefined("_b");
    let right = o.undefined("_right");
    let wrong = o.undefined("_wrong");
    let pick = |n: &str| match n {
        "_a" => a,
        "_b" => b,
        "_right" => right,
        _ => wrong,
    };
    o.sections[0].relocs = vec![
        Reloc {
            va: 0x04,
            symbol: pick(global),
            typ: REL_DIR32,
        },
        Reloc {
            va: 0x0a,
            symbol: pick(callee),
            typ: REL_REL32,
        },
    ];
    o.build()
}

pub fn callees() -> Vec<u8> {
    let mut o = Object::new();
    for (name, body) in [("_right", RIGHT_BODY), ("_wrong", WRONG_BODY)] {
        let s = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: body.to_vec(),
            ..Section::default()
        });
        o.section_symbol(s, SELECT_NODUPLICATES, 0);
        o.external(name, 0, s);
    }
    o.build()
}

pub fn scratch(tag: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!("msvc6-gate-{tag}-{}", std::process::id()));
    std::fs::remove_dir_all(&d).ok();
    std::fs::create_dir_all(&d).unwrap();
    d
}

pub fn objects(dir: &Path, unit: &[u8]) -> Vec<PathBuf> {
    let p = dir.join("Unit.obj");
    srcio::write_bytes(&p, unit).unwrap();
    let q = dir.join("Callees.obj");
    srcio::write_bytes(&q, &callees()).unwrap();
    vec![p, q]
}

pub fn map_text() -> String {
    format!(
        " ours\r\n\r\n Preferred load address is {BASE:08x}\r\n\r\n\
         \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
         \x200001:00000000       {F_NAME}                  {:08x} f   Unit.obj\r\n\
         \x200001:00000020       _right                     {:08x} f   Callees.obj\r\n\
         \x200001:00000030       _wrong                     {:08x} f   Callees.obj\r\n\
         \x200002:00000000       _a                         {:08x}     Data.obj\r\n\
         \x200002:00000004       _b                         {:08x}     Data.obj\r\n",
        BASE + F,
        BASE + RIGHT,
        BASE + WRONG,
        BASE + A,
        BASE + B
    )
}
