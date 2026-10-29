#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::offered;
use msvc6_fixture::archive::{Library, short_import};
use msvc6_fixture::coff::{Object as Obj, SCN_DATA, Section};

fn defining(names: &[&str]) -> Vec<u8> {
    let mut o = Obj::new();
    o.comp_id(0x13, 8034);
    let s = o.section(Section {
        name: ".idata$2".into(),
        characteristics: SCN_DATA,
        data: vec![0; 20],
        ..Section::default()
    });
    for n in names {
        o.external(n, 0, s);
    }
    o.build()
}

fn import_lib(dll: &str, stem: &str, names: &[(&str, u16)]) -> Vec<u8> {
    let mut l = Library::default();
    l.member(
        dll,
        defining(&[&format!("__IMPORT_DESCRIPTOR_{stem}")]),
        &[],
    );
    l.member(dll, defining(&["__NULL_IMPORT_DESCRIPTOR"]), &[]);
    l.member(
        dll,
        defining(&[&format!("\u{7f}{stem}_NULL_THUNK_DATA")]),
        &[],
    );
    for (n, hint) in names {
        l.member(dll, short_import(dll, n, *hint, 0, 3), &[]);
    }
    l.build()
}

#[test]
fn each_named_import_is_joined_to_the_member_that_supplies_it() {
    let s = common_sample();
    let kernel = import_lib(
        "KERNEL32.dll",
        "KERNEL32",
        &[
            ("_ExitProcess@4", 0),
            ("_GetVersion@0", 1),
            ("_HeapAlloc@12", 2),
            ("_Sleep@4", 3),
        ],
    );
    let ws = import_lib("WS2_32.dll", "WS2_32", &[("_send@16", 5)]);
    let mut two = Library::default();
    two.member(
        "two.obj",
        defining(&["__imp__GetVersion@0", "__imp__HeapAlloc@12"]),
        &[],
    );
    let libs = [
        offered("lib/KERNEL32.LIB", kernel),
        offered("lib/WS2_32.LIB", ws),
        offered("lib/TWO.LIB", two.build()),
    ];
    let rep = msvc6_match::run(&s, &libs, &[], 1).unwrap();
    let j = rep.imports.expect("an import join");
    assert_eq!((j.named, j.by_ordinal, j.supplied), (4, 1, 4));
    assert!(j.unsupplied.is_empty());
    assert_eq!(j.multi.len(), 1);
    assert_eq!(j.multi[0].names, vec!["GetVersion", "HeapAlloc"]);
    assert!(j.multi[0].member.starts_with("two["));
    assert_eq!(j.descriptors, 5);
    let short = rep
        .census
        .iter()
        .find(|b| b.product == "short import")
        .unwrap();
    assert_eq!(short.linked(), 4);
    assert_eq!(short.rich, Some(5));
    let desc = rep.census.iter().find(|b| b.prodid == Some(0x13)).unwrap();
    assert_eq!(desc.linked(), 6);
    let gv = j.rows.iter().find(|r| r.name == "GetVersion").unwrap();
    assert_eq!(gv.members.len(), 2);
    assert_eq!(gv.dll, "kernel32.dll");
}

fn common_sample() -> msvc6_image::pe::Image {
    msvc6_image::pe::Image::parse(msvc6_fixture::sample::sample().pe.build()).unwrap()
}
