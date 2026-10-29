#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::{body, filler, library, object, offered, parsed};
use msvc6_fixture::archive::{Library, MAGIC};
use msvc6_fixture::backstop;
use msvc6_fixture::coff::{Object as Obj, Reloc, SCN_TEXT, Section};
use std::sync::Mutex;
use std::time::Duration;

static SLOWEST: Mutex<(Duration, String)> = Mutex::new((Duration::ZERO, String::new()));

fn quick<T>(what: &str, f: impl FnOnce() -> T) -> T {
    let (r, took) = backstop::run(what, f);
    let slowest = {
        let mut s = SLOWEST.lock().unwrap();
        if took > s.0 {
            *s = (took, what.to_string());
        }
        s.clone()
    };
    eprintln!(
        "{what}: {took:?}; the slowest case so far {:?} ({})",
        slowest.0, slowest.1
    );
    r
}

fn refused(what: &str, lib: Vec<u8>, want: &str) {
    let img = parsed(filler(0x100));
    let libs = [offered("x/BAD.LIB", lib)];
    let r = quick(what, || msvc6_match::run(&img, &libs, &[], 2));
    let e = r.expect_err(what);
    assert!(format!("{e:#}").contains(want), "{what}: {e:#}");
}

fn good() -> Vec<u8> {
    library(&[(
        "obj\\a.obj",
        object(&[body("_a", &[0x55, 0x8b, 0xec, 0x5d, 0xc3], &[])]),
    )])
}

fn header(name: &str, size: &str) -> Vec<u8> {
    let f = |s: &str, w: usize| format!("{s:<w$}");
    let mut h = String::new();
    h.push_str(&f(name, 16));
    h.push_str(&f("0", 12));
    h.push_str(&f("", 6));
    h.push_str(&f("", 6));
    h.push_str(&f("0", 8));
    h.push_str(&f(size, 10));
    let mut b = h.into_bytes();
    b.extend_from_slice(b"`\n");
    b
}

fn raw(members: &[(&str, Vec<u8>)]) -> Vec<u8> {
    let mut b = MAGIC.to_vec();
    for (n, d) in members {
        b.extend_from_slice(&header(n, &d.len().to_string()));
        b.extend_from_slice(d);
        if d.len() % 2 == 1 {
            b.push(b'\n');
        }
    }
    b
}

#[test]
fn a_truncated_archive_or_a_member_past_the_file_is_an_error() {
    let g = good();
    refused("truncated", g[..g.len() - 7].to_vec(), "runs past the file");
    refused("cut header", g[..MAGIC.len() + 30].to_vec(), "truncated");
    let mut b = MAGIC.to_vec();
    b.extend_from_slice(&header("a.obj/", "99999999"));
    b.extend_from_slice(&[0; 64]);
    refused("size", b, "runs past the file");
    let mut b = MAGIC.to_vec();
    b.extend_from_slice(&header("a.obj/", "12x"));
    refused("size text", b, "unreadable size");
    refused(
        "not an archive",
        b"MZ not an archive".to_vec(),
        "not an archive",
    );
}

#[test]
fn a_long_name_past_its_table_or_with_no_end_is_an_error() {
    let o = object(&[body("_a", &[0x55, 0x8b, 0xec, 0x5d, 0xc3], &[])]);
    refused(
        "offset",
        raw(&[("//", b"a.obj/\n".to_vec()), ("/999", o.clone())]),
        "past the 7-byte name table",
    );
    refused(
        "no end",
        raw(&[("//", vec![b'a'; 5000]), ("/0", o.clone())]),
        "runs past 1024 bytes",
    );
}

#[test]
fn members_that_all_share_one_long_name_are_read_once() {
    let o = object(&[body("_a", &[0x55, 0x8b, 0xec, 0x5d, 0xc3], &[])]);
    let mut table = vec![b'a'; 1000];
    table.push(0);
    let mut m: Vec<(&str, Vec<u8>)> = vec![("//", table)];
    for _ in 0..20_000 {
        m.push(("/0", o.clone()));
    }
    let lib = raw(&m);
    let a = quick("shared names", || {
        msvc6_image::archive::Archive::parse(&lib).unwrap()
    });
    assert_eq!(a.members.len(), 20_001);
    assert_eq!(a.long_names_read, 1);
    assert_eq!(a.members[5].name.len(), 1000);
}

fn coff(sections: &[(u32, u32, u32, u16)], tail: usize) -> Vec<u8> {
    let mut b = Vec::new();
    b.extend_from_slice(&0x14cu16.to_le_bytes());
    b.extend_from_slice(&u16::try_from(sections.len()).unwrap().to_le_bytes());
    b.extend_from_slice(&[0; 4]);
    b.extend_from_slice(&0u32.to_le_bytes());
    b.extend_from_slice(&0u32.to_le_bytes());
    b.extend_from_slice(&[0; 4]);
    for (size, raw, relocs_at, relocs) in sections {
        b.extend_from_slice(b".text\0\0\0");
        b.extend_from_slice(&[0; 8]);
        b.extend_from_slice(&size.to_le_bytes());
        b.extend_from_slice(&raw.to_le_bytes());
        b.extend_from_slice(&relocs_at.to_le_bytes());
        b.extend_from_slice(&0u32.to_le_bytes());
        b.extend_from_slice(&relocs.to_le_bytes());
        b.extend_from_slice(&0u16.to_le_bytes());
        b.extend_from_slice(&SCN_TEXT.to_le_bytes());
    }
    b.resize(b.len() + tail, 0);
    b
}

#[test]
fn relocation_tables_that_overlap_or_code_past_the_member_are_errors() {
    let shared = coff(&[(16, 100, 120, 0xffff), (16, 100, 120, 0xffff)], 2000);
    refused(
        "shared relocations",
        library(&[("obj\\r.obj", shared)]),
        "the tables overlap",
    );
    let past = coff(&[(0x10000, 100, 0, 0)], 200);
    refused(
        "code past the member",
        library(&[("obj\\p.obj", past)]),
        "past its",
    );
}

#[test]
fn symbol_names_that_all_read_one_long_string_are_an_error() {
    let n = 20_000u32;
    let mut b = Vec::new();
    b.extend_from_slice(&0x14cu16.to_le_bytes());
    b.extend_from_slice(&0u16.to_le_bytes());
    b.extend_from_slice(&[0; 4]);
    b.extend_from_slice(&20u32.to_le_bytes());
    b.extend_from_slice(&n.to_le_bytes());
    b.extend_from_slice(&[0; 4]);
    for _ in 0..n {
        b.extend_from_slice(&[0; 4]);
        b.extend_from_slice(&4u32.to_le_bytes());
        b.extend_from_slice(&[0; 4]);
        b.extend_from_slice(&1u16.to_le_bytes());
        b.extend_from_slice(&0x20u16.to_le_bytes());
        b.extend_from_slice(&[2, 0]);
    }
    let strings = 4000usize;
    b.extend_from_slice(&u32::try_from(strings + 5).unwrap().to_le_bytes());
    b.resize(b.len() + strings, b'a');
    b.push(0);
    refused("names", library(&[("obj\\n.obj", b)]), "names overlap");
}

#[test]
fn a_body_that_matches_everywhere_is_bounded_and_reported() {
    let mut z = vec![0u8; 0x10000];
    let mut o = Obj::new();
    let t = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT,
        data: z.clone(),
        ..Section::default()
    });
    o.external("_zero", 0, t);
    let x = o.undefined("_x");
    let k = usize::try_from(t - 1).unwrap();
    for at in (4..0x10000u32).step_by(8) {
        o.sections[k].relocs.push(Reloc {
            va: at,
            symbol: x,
            typ: 6,
        });
    }
    let mut l = Library::default();
    l.member("obj\\z.obj", o.build(), &[]);
    z.resize(0x10_0000, 0);
    let img = parsed(z);
    let libs = [offered("x/ZERO.LIB", l.build())];
    let rep = quick("zeros", || msvc6_match::run(&img, &libs, &[], 1).unwrap());
    assert_eq!(rep.run.members[0].exhausted, 1);
    assert!(rep.run.present().is_empty());
}
