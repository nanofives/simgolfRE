#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::archive::{Library, short_import};
use msvc6_fixture::coff::Object as Obj;
use msvc6_image::archive::{Archive, MemberKind, is_archive, undecorate_import};

fn object(name: &str, prodid: u16) -> Vec<u8> {
    let mut o = Obj::new();
    let t = o.code(".text", &[0xc3]);
    o.comp_id(prodid, 8168);
    o.external(name, 0, t);
    o.build()
}

fn library() -> Vec<u8> {
    let mut l = Library {
        date: 900_000_000,
        ..Library::default()
    };
    l.member("strlen.obj", object("_strlen", 0x0a), &["_strlen"]);
    l.member(
        "..\\obj\\release\\a_long_member.obj",
        object("_long", 0x0b),
        &["_long"],
    );
    l.member(
        "KERNEL32.dll",
        short_import("KERNEL32.dll", "_GetVersion@0", 1, 0, 3),
        &["_GetVersion@0", "__imp__GetVersion@0"],
    );
    l.member(
        "KERNEL32.dll",
        short_import("KERNEL32.dll", "_ExitProcess@4", 0, 0, 3),
        &["_ExitProcess@4", "__imp__ExitProcess@4"],
    );
    l.build()
}

#[test]
fn members_resolve_long_names_and_are_classified() {
    let d = library();
    assert!(is_archive(&d));
    let a = Archive::parse(&d).unwrap();
    let kinds: Vec<(MemberKind, &str)> = a
        .members
        .iter()
        .map(|m| (m.kind, m.name.as_str()))
        .collect();
    assert_eq!(
        kinds,
        [
            (MemberKind::Linker, "/"),
            (MemberKind::Linker, "/"),
            (MemberKind::Longnames, "//"),
            (MemberKind::Object, "strlen.obj"),
            (MemberKind::Object, "..\\obj\\release\\a_long_member.obj"),
            (MemberKind::Import, "KERNEL32.dll"),
            (MemberKind::Import, "KERNEL32.dll"),
        ]
    );
    assert_eq!(a.members[4].stem(), "a_long_member.obj");
    assert_eq!(a.members[3].date, 900_000_000);
    assert_eq!(a.content().count(), 4);
    let objects = a.objects();
    assert_eq!(objects.len(), 2);
    assert_eq!(objects[1].1.comp_id(), Some((0x0b << 16) | 8168));
    assert_eq!(a.data(&a.members[3]), &object("_strlen", 0x0a)[..]);
}

#[test]
fn the_symbol_index_maps_each_symbol_to_its_member() {
    let d = library();
    let a = Archive::parse(&d).unwrap();
    let idx = a.symbol_index();
    assert_eq!(idx.len(), 6);
    let (s, at) = &idx[0];
    assert_eq!(s, "_strlen");
    assert_eq!(a.member_at(*at).unwrap().name, "strlen.obj");
    assert_eq!(a.import_order(), ["GetVersion", "ExitProcess"]);
}

#[test]
fn an_import_name_loses_its_prefix_and_stdcall_suffix_only() {
    assert_eq!(undecorate_import("__imp__GetVersion@0"), "GetVersion");
    assert_eq!(undecorate_import("__imp__wsprintfA"), "wsprintfA");
    assert_eq!(undecorate_import("__imp__a@b"), "a@b");
}

#[test]
fn a_damaged_archive_is_refused() {
    assert!(Archive::parse(b"!<arch>").is_err());
    assert!(Archive::parse(b"not an archive at all").is_err());
    let d = library();
    assert!(Archive::parse(&d[..100]).is_err());
    let mut bad = d.clone();
    bad[8 + 58] = b'x';
    assert!(
        Archive::parse(&bad)
            .unwrap_err()
            .to_string()
            .contains("end marker")
    );
}

#[test]
fn a_gnu_style_longnames_entry_ends_at_its_slash() {
    let mut d = b"!<arch>\n".to_vec();
    let names = b"a_long_object_name.o/\n";
    let header = |name: &str, size: usize| {
        format!("{name:<16}{:<12}{:<6}{:<6}{:<8}{size:<10}`\n", 0, "", "", 0).into_bytes()
    };
    d.extend_from_slice(&header("//", names.len()));
    d.extend_from_slice(names);
    let body = object("_x", 0x0a);
    d.extend_from_slice(&header("/0", body.len()));
    d.extend_from_slice(&body);
    if body.len() % 2 == 1 {
        d.push(b'\n');
    }
    let a = Archive::parse(&d).unwrap();
    assert_eq!(a.members[1].name, "a_long_object_name.o");
    assert_eq!(a.members[1].kind, MemberKind::Object);
}
