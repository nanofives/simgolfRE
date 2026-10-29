#![allow(
    dead_code,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing
)]

use msvc6_diff::env::Env;
use msvc6_diff::resolve::Names;
use msvc6_fixture::coff::{
    Object as Obj, REL_DIR32, REL_REL32, Reloc as Rel, SCN_COMDAT, SCN_DATA, SCN_RDATA, SCN_TEXT,
    SELECT_ANY, Section,
};
use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_DATA as PE_DATA};
use msvc6_image::pe::Image;
use msvc6_project::names::{Kind, Name, NameOracle};

pub const CODE_LEN: usize = 0x38;
pub const CALLEE: u32 = 0x40;

pub fn code() -> Vec<u8> {
    let mut c = vec![
        0x55, 0x8b, 0xec, 0xa1, 0, 0, 0, 0, 0x68, 0, 0, 0, 0, 0xe8, 0, 0, 0, 0, 0x83, 0xc4, 0x04,
        0x8b, 0x45, 0x08, 0x83, 0xf8, 0x02, 0x77, 0x0c, 0xff, 0x24, 0x85, 0x2c, 0, 0, 0, 0x33,
        0xc0, 0x5d, 0xc3, 0x40, 0x5d, 0xc3, 0xcc,
    ];
    for t in [0x24u32, 0x28, 0x29] {
        c.extend_from_slice(&t.to_le_bytes());
    }
    assert_eq!(c.len(), CODE_LEN);
    c
}

pub struct Syms {
    pub text: u32,
    pub g: u32,
    pub callee: u32,
    pub lit: u32,
}

pub fn relocs(s: &Syms) -> Vec<Rel> {
    let r = |va: u32, symbol: u32, typ: u16| Rel { va, symbol, typ };
    vec![
        r(0x04, s.g, REL_DIR32),
        r(0x09, s.lit, REL_DIR32),
        r(0x0e, s.callee, REL_REL32),
        r(0x20, s.text, REL_DIR32),
        r(0x2c, s.text, REL_DIR32),
        r(0x30, s.text, REL_DIR32),
        r(0x34, s.text, REL_DIR32),
    ]
}

pub fn object_with(code: Vec<u8>, extra: usize) -> Vec<u8> {
    let mut o = Obj::new();
    let text = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: code,
        ..Section::default()
    });
    let rdata = o.section(Section {
        name: ".rdata".into(),
        characteristics: SCN_RDATA | SCN_COMDAT,
        data: b"hello\0".to_vec(),
        ..Section::default()
    });
    let data = o.section(Section {
        name: ".data".into(),
        characteristics: SCN_DATA,
        data: b"Summer\0\0\x01\x02\x03\x04".to_vec(),
        ..Section::default()
    });
    let s_text = o.section_symbol(text, SELECT_ANY, 0);
    o.external("?f@@YAXXZ", 0, text);
    let g = o.undefined("_g");
    let callee = o.undefined("_callee");
    o.section_symbol(rdata, SELECT_ANY, 0);
    let lit = o.external("??_C@_05hello@", 0, rdata);
    o.section_symbol(data, 0, 0);
    o.external("?g_table_with_a_long_name@@3PAHA", 0, data);
    o.plain("$SG100", 0, data, 0, 3);
    for k in 0..extra {
        o.undefined(&format!("?filler_{k:05}@@3HA"));
    }
    let s = Syms {
        text: s_text,
        g,
        callee,
        lit,
    };
    o.sections[0].relocs = relocs(&s);
    o.build()
}

pub fn object() -> Vec<u8> {
    object_with(code(), 0)
}

pub fn linked(t: u32, d: u32, base: u32) -> Vec<u8> {
    let mut c = code();
    let put = |c: &mut Vec<u8>, at: usize, v: u32| c[at..at + 4].copy_from_slice(&v.to_le_bytes());
    put(&mut c, 0x04, base + d);
    put(&mut c, 0x09, base + d + 8);
    put(&mut c, 0x0e, (t + CALLEE).wrapping_sub(t + 0x0e + 4));
    put(&mut c, 0x20, base + t + 0x2c);
    for (k, x) in [0x24u32, 0x28, 0x29].into_iter().enumerate() {
        put(&mut c, 0x2c + 4 * k, base + t + x);
    }
    c.resize(CALLEE as usize, 0xcc);
    c.push(0xc3);
    c.resize(0x50, 0xcc);
    c
}

pub struct Fix {
    pub img: Image,
    pub names: Names,
    pub text: u32,
    pub text_len: u32,
}

pub fn fixture() -> Fix {
    let mut pe = Pe::dll();
    let base = pe.image_base;
    let t = pe.next_rva();
    let d = t + 0x1000;
    pe.push(".text", SCN_CODE, linked(t, d, base));
    let dd = pe.push(".data", PE_DATA, b"\x01\0\0\0\0\0\0\0hello\0".to_vec());
    assert_eq!(dd, d);
    let img = Image::parse(pe.build()).unwrap();
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
        "fixture",
        vec![
            row(t, "?f@@YAXXZ"),
            row(t + CALLEE, "_callee"),
            row(d, "_g"),
            row(d + 8, "??_C@_05hello@"),
        ],
        Vec::new(),
    );
    Fix {
        img,
        names: Names::from_oracle(&o),
        text: t,
        text_len: 0x50,
    }
}

pub fn env(f: &Fix, seats: Vec<(u32, u32)>, pad: Vec<(u32, u32)>) -> Env {
    Env::new("FIX", f.img.clone(), f.names.clone(), seats, pad).unwrap()
}

pub fn base_env(f: &Fix) -> Env {
    env(
        f,
        vec![
            (f.text, u32::try_from(CODE_LEN).unwrap()),
            (f.text + CALLEE, 1),
        ],
        Vec::new(),
    )
}
