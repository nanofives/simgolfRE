#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_diff::env::Env;
use msvc6_diff::resolve::Names;
use msvc6_fixture::coff::{
    Object, REL_DIR32, Reloc, SCN_BSS, SCN_COMDAT, SCN_TEXT, SELECT_NODUPLICATES, Section,
};
use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_DATA};
use msvc6_image::pe::Image;
use msvc6_model::bssrun::{Opts, Run, run};
use msvc6_project::names::{Kind, Name, NameOracle};
use std::path::{Path, PathBuf};

const BASE: u32 = 0x0040_0000;
const F: u32 = 0x1000;
const DATA: u32 = 0x2000;
const BSS: u32 = DATA + 0x10;

fn body() -> Vec<u8> {
    vec![0x8b, 0x0d, 0, 0, 0, 0, 0xc3]
}

fn env() -> Env {
    let mut pe = Pe::exe();
    let mut f = body();
    f[2..6].copy_from_slice(&(BASE + BSS).to_le_bytes());
    let mut t = vec![0xcc; 0x10];
    t[..f.len()].copy_from_slice(&f);
    assert_eq!(pe.push(".text", SCN_CODE, t), F);
    assert_eq!(
        pe.push_virtual(".data", SCN_DATA, vec![0; 0x10], 0x40),
        DATA
    );
    let mut o = NameOracle::default();
    o.add(
        "tsv",
        "names.tsv",
        vec![Name {
            rva: F,
            name: "_f".into(),
            kind: Kind::Label,
            size: None,
            scope: None,
            provider: "tsv".into(),
        }],
        Vec::new(),
    );
    Env::new(
        "FIX",
        Image::parse(pe.build()).unwrap(),
        Names::from_oracle(&o),
        vec![(F, 7)],
        Vec::new(),
    )
    .unwrap()
}

fn object(function: &str, var: Option<&str>) -> Vec<u8> {
    let mut o = Object::new();
    let t = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: body(),
        ..Section::default()
    });
    o.section_symbol(t, SELECT_NODUPLICATES, 0);
    o.external(function, 0, t);
    if let Some(v) = var {
        let b = o.section(Section {
            name: ".bss".into(),
            characteristics: SCN_BSS,
            uninit: 4,
            ..Section::default()
        });
        let w = o.external(v, 0, b);
        o.sections[0].relocs = vec![Reloc {
            va: 2,
            symbol: w,
            typ: REL_DIR32,
        }];
    }
    o.build()
}

struct Scratch(PathBuf);

impl Drop for Scratch {
    fn drop(&mut self) {
        std::fs::remove_dir_all(&self.0).ok();
    }
}

fn scratch() -> Scratch {
    let d = std::env::temp_dir().join(format!("msvc6-model-bssrun-{}", std::process::id()));
    std::fs::remove_dir_all(&d).ok();
    std::fs::create_dir_all(&d).unwrap();
    Scratch(d)
}

fn objects(dir: &Path) -> Vec<PathBuf> {
    let mut out = Vec::new();
    for (stem, bytes) in [
        ("Unit", object("_f", Some("_w"))),
        ("Codec", object("_g", Some("_u"))),
        ("Plain", object("_h", None)),
    ] {
        let p = dir.join(format!("{stem}.obj"));
        srcio::write_bytes(&p, &bytes).unwrap();
        out.push(p);
    }
    out
}

fn screen(objs: &[PathBuf], source_of: &dyn Fn(&str) -> Option<PathBuf>) -> Run {
    run(
        "FIX",
        &env(),
        objs,
        &Opts {
            merge_screen: true,
            ..Opts::default()
        },
        source_of,
    )
    .unwrap()
}

#[test]
fn the_merge_screen_asks_only_a_tu_holding_a_seated_bss_variable_for_its_source() {
    let s = scratch();
    let objs = objects(&s.0);
    let src = s.0.join("Unit.c");
    srcio::write(&src, "int w;\n").unwrap();
    let asked = std::cell::RefCell::new(Vec::new());
    let found = |stem: &str| -> Option<PathBuf> {
        asked.borrow_mut().push(stem.to_string());
        (stem == "Unit").then(|| src.clone())
    };
    let r = screen(&objs, &found);
    assert!(r.refused.is_empty(), "{:?}", r.refused);
    assert_eq!(*asked.borrow(), ["Unit"]);
    assert_eq!(
        r.lines.last().map(String::as_str),
        Some("TOTAL FIX: 1 TU block(s), 0 adjacent pair(s): 0 refuted, 0 compatible")
    );

    let none = |_: &str| -> Option<PathBuf> { None };
    let r = screen(&objs, &none);
    assert_eq!(
        r.refused
            .iter()
            .map(|(t, _)| t.as_str())
            .collect::<Vec<_>>(),
        ["Unit"]
    );
    assert!(
        r.refused[0].1.contains("no source for this TU"),
        "{:?}",
        r.refused
    );
    assert_eq!(
        r.lines.last().map(String::as_str),
        Some("TOTAL FIX: 0 TU block(s), 0 adjacent pair(s): 0 refuted, 0 compatible")
    );
}
