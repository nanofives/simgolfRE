#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::coff::{
    CLASS_EXTERNAL, CLASS_FUNCTION, CLASS_LABEL, CLASS_STATIC, Object as Obj, SCN_DATA, SCN_DEBUG,
    SCN_TEXT, Section, aux_with, section_aux,
};
use msvc6_image::objcmp::{
    Cmp, Masks, compare, compare_canonical, is_author_label, objects, pdb_signatures,
};

fn zi_object(sig: [u8; 4], age: u32) -> Vec<u8> {
    let mut rec = 0x16u16.to_le_bytes().to_vec();
    rec.extend_from_slice(&sig);
    rec.extend_from_slice(&age.to_le_bytes());
    rec.extend_from_slice(b"\x08vc60.pdb");
    let mut data = 1u32.to_le_bytes().to_vec();
    data.extend_from_slice(&u16::try_from(rec.len()).unwrap().to_le_bytes());
    data.extend_from_slice(&rec);
    let mut o = Obj::new();
    o.section(Section {
        name: ".debug$T".into(),
        characteristics: SCN_DEBUG,
        data,
        ..Section::default()
    });
    o.build()
}

#[test]
fn only_the_time_date_stamp_is_masked() {
    let mut o = Obj::new();
    o.code(".text", &[0x90, 0xc3]);
    let a = o.build();
    o.timestamp ^= 0x55;
    let b = o.build();
    assert_ne!(a, b);
    assert_eq!(compare(&a, &b, false), Cmp::Same);
    let mut c = b.clone();
    let last = c.len() - 1;
    c[last] ^= 1;
    assert!(matches!(compare(&a, &c, false), Cmp::Bytes(_, 1)));
    assert_eq!(
        compare(&a, &a[..a.len() - 1], false),
        Cmp::Length(a.len(), a.len() - 1)
    );
}

#[test]
fn the_pdb_signature_is_masked_only_when_asked_and_nothing_beside_it() {
    let a = zi_object([0x01, 0x02, 0x03, 0x04], 2);
    let b = zi_object([0x05, 0x06, 0x03, 0x04], 2);
    let at = pdb_signatures(&a);
    assert_eq!(at, vec![20 + 40 + 4 + 4]);
    assert_eq!(compare(&a, &b, false), Cmp::Bytes(at[0], 2));
    assert_eq!(compare(&a, &b, true), Cmp::Same);
    let c = zi_object([0x05, 0x06, 0x03, 0x04], 3);
    assert_eq!(compare(&a, &c, true), Cmp::Bytes(at[0] + 4, 1));
}

fn one_symbol(name: &str, value: u32) -> Vec<u8> {
    let mut o = Obj::new();
    o.code(".text", &[0xc3; 0x20]);
    o.plain(name, value, 1, 0, CLASS_LABEL);
    o.build()
}

fn labels() -> Masks {
    Masks {
        labels: true,
        ..Masks::default()
    }
}

#[test]
fn a_renumbered_label_is_the_same_object_and_a_moved_one_is_not() {
    let a = one_symbol("$L1234", 0x10);
    let l = labels();
    assert_eq!(
        compare_canonical(&a, &one_symbol("$L1240", 0x10), l, 4).unwrap(),
        None
    );
    assert!(
        compare_canonical(&a, &one_symbol("$T1234", 0x10), l, 4)
            .unwrap()
            .is_some()
    );
    assert!(
        compare_canonical(&a, &one_symbol("$L1234", 0x14), l, 4)
            .unwrap()
            .is_some()
    );
    assert!(
        compare_canonical(&a, &one_symbol("$SG1234", 0x10), l, 4)
            .unwrap()
            .is_some()
    );
    assert_eq!(
        compare_canonical(&a, &one_symbol("$d$77", 0x10), l, 4).unwrap(),
        None
    );
    assert!(
        compare_canonical(&a, &one_symbol("$d$77", 0x14), l, 4)
            .unwrap()
            .is_some()
    );
    assert!(
        compare_canonical(
            &a,
            &one_symbol("$L1240", 0x10),
            Masks {
                debug: true,
                ..Masks::default()
            },
            4
        )
        .unwrap()
        .is_some()
    );
    assert!(!is_author_label("$SG1234"));
    assert!(!is_author_label("$d$"));
    assert!(is_author_label("$dflt$1234"));
}

fn one_section_def(checksum: u32, selection: u8) -> Vec<u8> {
    let mut o = Obj::new();
    let d = o.section(Section {
        name: ".data".into(),
        characteristics: SCN_DATA,
        data: vec![1; 0x78],
        ..Section::default()
    });
    o.symbol(msvc6_fixture::coff::Symbol {
        name: ".data".into(),
        value: 0,
        section: d,
        typ: 0,
        class: CLASS_STATIC,
        aux: vec![section_aux(0x78, 0, 0, checksum, 0, selection)],
    });
    o.build()
}

#[test]
fn only_a_plain_sections_checksum_is_masked() {
    let l = labels();
    let plain = one_section_def(0x1111_2222, 0);
    assert_eq!(
        compare_canonical(&plain, &one_section_def(0x3333_4444, 0), l, 4).unwrap(),
        None
    );
    let comdat = one_section_def(0x1111_2222, 2);
    assert!(
        compare_canonical(&comdat, &one_section_def(0x3333_4444, 2), l, 4)
            .unwrap()
            .is_some()
    );
}

fn lined(nlines: u16, lptr: u32, begin: u16, count: u32, end: u16, code: [u8; 2]) -> Vec<u8> {
    let mut o = Obj::new();
    let t = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT,
        data: code.to_vec(),
        lines: (0..nlines).map(|i| (u32::from(i), i + 1)).collect(),
        ..Section::default()
    });
    o.symbol(msvc6_fixture::coff::Symbol {
        name: ".text".into(),
        value: 0,
        section: t,
        typ: 0,
        class: CLASS_STATIC,
        aux: vec![section_aux(2, 0, nlines, 0, 0, 0)],
    });
    o.symbol(msvc6_fixture::coff::Symbol {
        name: "_f".into(),
        value: 0,
        section: t,
        typ: 0x20,
        class: CLASS_EXTERNAL,
        aux: vec![aux_with(8, &lptr.to_le_bytes())],
    });
    o.symbol(msvc6_fixture::coff::Symbol {
        name: ".bf".into(),
        value: 0,
        section: t,
        typ: 0,
        class: CLASS_FUNCTION,
        aux: vec![aux_with(4, &begin.to_le_bytes())],
    });
    o.plain(".lf", count, t, 0, CLASS_FUNCTION);
    o.symbol(msvc6_fixture::coff::Symbol {
        name: ".ef".into(),
        value: 2,
        section: t,
        typ: 0,
        class: CLASS_FUNCTION,
        aux: vec![aux_with(4, &end.to_le_bytes())],
    });
    o.build()
}

#[test]
fn line_numbers_are_masked_only_when_asked_and_code_never() {
    let lines = Masks {
        lines: true,
        ..Masks::default()
    };
    let a = lined(3, 0x100, 40, 3, 44, [0x90, 0xc3]);
    let moved = lined(2, 0x10c, 44, 2, 47, [0x90, 0xc3]);
    assert!(compare(&a, &moved, false) != Cmp::Same);
    assert_eq!(compare_canonical(&a, &moved, lines, 4).unwrap(), None);
    assert!(
        compare_canonical(&a, &moved, labels(), 4)
            .unwrap()
            .is_some()
    );
    let recoded = lined(2, 0x10c, 44, 2, 47, [0xcc, 0xc3]);
    let d = compare_canonical(&a, &recoded, lines, 4).unwrap().unwrap();
    assert_eq!(d.count, 1);
    assert!(d.shown[0].0.as_deref().unwrap().contains("90c3"));
}

#[test]
fn a_symlinked_directory_is_not_counted_twice() {
    let root = std::env::temp_dir().join(format!("msvc6-image-objcmp-{}", std::process::id()));
    if root.exists() {
        std::fs::remove_dir_all(&root).unwrap();
    }
    std::fs::create_dir_all(root.join("app/Release")).unwrap();
    msvc6_base::srcio::write_bytes(root.join("app/Release/a.obj"), &[0u8; 4]).unwrap();
    msvc6_base::srcio::write_bytes(root.join("app/Release/notes.txt"), b"x").unwrap();
    std::os::unix::fs::symlink(root.join("app"), root.join("ide")).unwrap();
    assert_eq!(
        objects(&root),
        vec![std::path::PathBuf::from("app/Release/a.obj")]
    );
    std::fs::remove_dir_all(&root).unwrap();
}
