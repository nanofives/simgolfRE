#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::archive::{Library, short_import};
use msvc6_fixture::coff::{
    CLASS_FILE, Object as Obj, REL_DIR32, Reloc, SCN_DATA, Section, aux_with,
};
use msvc6_image::archive::Archive;
use msvc6_image::census::{Bucket, buckets, members};
use msvc6_image::libdiff::{diff, first_diff};

fn object(stamp: u32, file: &str, body: &[u8], prodid: Option<u16>) -> Vec<u8> {
    let mut o = Obj::new();
    o.timestamp = stamp;
    o.symbol(msvc6_fixture::coff::Symbol {
        name: ".file".into(),
        value: 0,
        section: -2,
        typ: 0,
        class: CLASS_FILE,
        aux: vec![aux_with(0, file.as_bytes())],
    });
    let t = o.code(".text", body);
    if let Some(p) = prodid {
        o.comp_id(p, 8168);
    }
    o.external("_f", 0, t);
    o.build()
}

fn lib(date: u32, members: &[(&str, Vec<u8>)]) -> Vec<u8> {
    let mut l = Library {
        date,
        ..Library::default()
    };
    for (n, d) in members {
        l.member(n, d.clone(), &[]);
    }
    l.build()
}

#[test]
fn dates_timestamps_and_file_records_are_not_compared() {
    let a = lib(
        1,
        &[("a.obj", object(10, "C:\\one\\a.c", &[0xc3], Some(0x0a)))],
    );
    let b = lib(
        2,
        &[("A.OBJ", object(20, "D:\\two\\a.c", &[0xc3], Some(0x0a)))],
    );
    let (x, y) = (Archive::parse(&a).unwrap(), Archive::parse(&b).unwrap());
    let d = diff(&x, &y).unwrap();
    assert!(d.identical(), "{d:?}");
}

#[test]
fn order_content_and_membership_differences_are_each_reported() {
    let one = object(1, "a.c", &[0xc3], Some(0x0a));
    let two = object(1, "b.c", &[0x90, 0xc3], Some(0x0b));
    let a = lib(
        1,
        &[
            ("a.obj", one.clone()),
            ("b.obj", two.clone()),
            ("c.obj", one.clone()),
        ],
    );
    let b = lib(
        1,
        &[
            ("b.obj", two.clone()),
            ("a.obj", object(1, "a.c", &[0xcc], Some(0x0a))),
            ("d.obj", one),
        ],
    );
    let d = diff(&Archive::parse(&a).unwrap(), &Archive::parse(&b).unwrap()).unwrap();
    assert!(!d.identical());
    assert_eq!(d.order.as_ref().unwrap().row, 0);
    let status: Vec<(&str, &str)> = d
        .rows
        .iter()
        .map(|r| (r.member.as_str(), r.status))
        .collect();
    assert_eq!(
        status,
        [
            ("a.obj", "differs"),
            ("b.obj", "identical"),
            ("c.obj", "only-ours"),
            ("d.obj", "only-reference")
        ]
    );
    assert_eq!(d.rows[0].differences[0].0, "sections");
    assert_eq!(d.rows[0].ours_comp_id, Some((0x0a << 16) | 8168));
}

#[test]
fn a_relocation_target_difference_is_named() {
    let with = |target: &str| {
        let mut o = Obj::new();
        let t = o.section(Section {
            name: ".data".into(),
            characteristics: SCN_DATA,
            data: vec![0; 4],
            relocs: vec![Reloc {
                va: 0,
                symbol: 0,
                typ: REL_DIR32,
            }],
            ..Section::default()
        });
        o.undefined(target);
        let _ = t;
        o.build()
    };
    let a = lib(1, &[("r.obj", with("_x"))]);
    let b = lib(1, &[("r.obj", with("_y"))]);
    let d = diff(&Archive::parse(&a).unwrap(), &Archive::parse(&b).unwrap()).unwrap();
    let r = &d.rows[0];
    let rel = r
        .differences
        .iter()
        .find(|(k, _)| *k == "relocations")
        .unwrap();
    assert_eq!(rel.1.ours.as_deref(), Some(".data+0x0 type 6 -> _x"));
}

#[test]
fn repeated_member_names_pair_by_occurrence() {
    let imp = |s: &str| short_import("K.dll", s, 0, 0, 3);
    let a = lib(1, &[("K.dll", imp("_a@0")), ("K.dll", imp("_b@0"))]);
    let b = lib(1, &[("K.dll", imp("_a@0")), ("K.dll", imp("_c@0"))]);
    let d = diff(&Archive::parse(&a).unwrap(), &Archive::parse(&b).unwrap()).unwrap();
    assert_eq!(d.rows.len(), 2);
    assert_eq!(d.rows[0].status, "identical");
    assert_eq!(d.rows[1].member, "k.dll#2");
    assert_eq!(d.rows[1].status, "differs");
}

#[test]
fn first_diff_names_the_first_disagreeing_row() {
    let s = |v: &[&str]| v.iter().map(ToString::to_string).collect::<Vec<String>>();
    assert!(first_diff(&s(&["a", "b"]), &s(&["a", "b"])).is_none());
    let d = first_diff(&s(&["a", "b"]), &s(&["a", "c", "d"])).unwrap();
    assert_eq!((d.row, d.ours_rows, d.reference_rows), (1, 2, 3));
    assert_eq!(
        (d.ours.as_deref(), d.reference.as_deref()),
        (Some("b"), Some("c"))
    );
    let e = first_diff(&s(&["a"]), &s(&["a", "z"])).unwrap();
    assert_eq!((e.ours, e.reference.as_deref()), (None, Some("z")));
}

#[test]
fn the_census_buckets_members_by_their_stamp() {
    let mut codeless = Obj::new();
    codeless.section(Section {
        name: ".data".into(),
        characteristics: SCN_DATA,
        data: vec![1],
        ..Section::default()
    });
    codeless.comp_id(0x0a, 8168);
    let d = lib(
        1,
        &[
            ("..\\obj\\c1.obj", object(1, "c1.c", &[0xc3], Some(0x0a))),
            ("c2.obj", object(1, "c2.c", &[0xc3], Some(0x0a))),
            ("p.obj", object(1, "p.cpp", &[0xc3], Some(0x0b))),
            ("asm.obj", object(1, "x.asm", &[0xc3], None)),
            ("data.obj", codeless.build()),
            ("U.dll", short_import("U.dll", "_MessageBoxA@16", 0, 0, 3)),
        ],
    );
    let a = Archive::parse(&d).unwrap();
    let rows = members(&a).unwrap();
    assert_eq!(rows.len(), 6);
    assert_eq!(rows[0].member, "c1.obj");
    assert_eq!(rows[0].stamp(), "prodid 0x0a Utc12_C build 8168");
    assert_eq!(rows[3].stamp(), "unstamped");
    assert_eq!(rows[5].defines, ["_MessageBoxA@16"]);
    let b = buckets(&rows);
    let got: Vec<(Bucket, usize, usize)> = b
        .iter()
        .map(|x| (x.bucket, x.members.len(), x.codeless.len()))
        .collect();
    assert_eq!(
        got,
        [
            (Bucket::Prodid(0x0a), 3, 1),
            (Bucket::Prodid(0x0b), 1, 0),
            (Bucket::Unstamped, 1, 0),
            (Bucket::Import, 1, 0)
        ]
    );
    assert!(members(&Archive::parse(b"!<arch>\n").unwrap()).is_err());
}
