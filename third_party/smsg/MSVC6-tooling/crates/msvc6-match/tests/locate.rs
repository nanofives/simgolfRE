#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::*;
use msvc6_match::locate::Placed;
use msvc6_match::table::{Table, compare};
use std::path::PathBuf;

fn by<'a>(run: &'a msvc6_match::locate::Run, stem: &str) -> &'a Placed {
    run.members
        .iter()
        .find(|p| p.stem() == stem)
        .unwrap_or_else(|| panic!("no member {stem}"))
}

#[test]
fn a_linked_member_is_placed_uniquely_and_a_straddling_one_is_refused() {
    let img = parsed(linked());
    let libs = [offered("x/LIBFIX.LIB", archive())];
    let rep = msvc6_match::run(&img, &libs, &[], 2).unwrap();
    let run = &rep.run;
    assert_eq!(run.considered, 8);
    let a = by(run, "a.obj");
    assert_eq!(a.ranges, vec![(AT_A, AT_A + 0x20)]);
    assert_eq!(a.unique, 1);
    assert_eq!(a.imports, vec![("_helper".to_string(), AT_HELPER)]);
    assert_eq!(a.archive, "libfix");
    assert_eq!(by(run, "c.obj").ranges, vec![(AT_C, AT_C + 0x20)]);
    let s = by(run, "s.obj");
    assert!(s.ranges.is_empty(), "{:?}", s.ranges);
    assert_eq!(s.refused.len(), 1);
    assert_eq!(
        (s.refused[0].lo, s.refused[0].hi),
        (AT_A + 0x18, AT_A + 0x28)
    );
    assert_eq!(s.refused[0].owners, vec!["obj\\a.obj", "obj\\c.obj"]);
    let d = by(run, "d.obj");
    assert_eq!(d.ranges, vec![(AT_D1, AT_D1 + 0x10), (AT_D2, AT_D2 + 4)]);
    assert_eq!(d.resolved_by_order, 1);
    let w = by(run, "w.obj");
    assert_eq!(w.ranges, vec![(AT_W, AT_W + 9)]);
    assert_eq!(w.by_witness, 1);
    let gone = by(run, "gone.obj");
    assert!(gone.ranges.is_empty());
    assert_eq!(gone.absent, 1);
    let present: Vec<&str> = run.present().iter().map(|p| p.stem()).collect();
    assert_eq!(
        present,
        ["a.obj", "c.obj", "helper.obj", "d.obj", "e.obj", "w.obj"]
    );
    assert_eq!(run.covered(), 0x20 + 0x20 + 0x10 + 0x14 + 0x10 + 9);
    assert_eq!(rep.image_base, BASE);
}

#[test]
fn the_table_round_trips_and_re_derives_every_row() {
    let img = parsed(linked());
    let libs = [offered("x/LIBFIX.LIB", archive())];
    let rep = msvc6_match::run(&img, &libs, &[], 1).unwrap();
    let t = Table::of_run(&rep.run);
    assert_eq!(t.member.len(), 7);
    let back = Table::parse(&t.render(), &PathBuf::from("t.toml")).unwrap();
    assert_eq!(back, t);
    let k = compare(&back.recorded(), &rep.run.present());
    assert_eq!((k.rows, k.rederived), (7, 7));
    assert!(k.differences.is_empty() && k.unrecorded.is_empty());
    let mut wrong = back.recorded();
    wrong[0].member = "obj\\other.obj".into();
    wrong[1].len += 1;
    let k = compare(&wrong, &rep.run.present());
    assert_eq!(k.differences.len(), 2);
    assert_eq!(
        k.differences[0].line(),
        format!("    RECORD {AT_A:#08x} len 0x20: obj\\other.obj -- placed obj\\a.obj")
    );
    assert_eq!(
        k.differences[1].line(),
        format!("    RECORD {AT_C:#08x} len 0x21: obj\\c.obj -- placed len 0x20")
    );
    assert_eq!(k.unrecorded.len(), 2);
}

#[test]
fn an_exact_tie_is_reported_and_the_record_decides_it() {
    let twin = body("_alpha_twin", &a().bytes, &[(7, "_helper")]);
    let lib = library(&[
        ("obj\\a.obj", object(&[a()])),
        ("obj\\b.obj", object(&[twin])),
        ("obj\\helper.obj", object(&[helper()])),
    ]);
    let img = parsed(linked());
    let libs = [offered("x/LIBFIX.LIB", lib)];
    let rep = msvc6_match::run(&img, &libs, &[], 1).unwrap();
    let a = by(&rep.run, "a.obj");
    assert_eq!(a.tied_with, vec!["obj\\b.obj"]);
    assert!(a.tie_break.is_empty());
    assert_eq!(a.ranges.len(), 1);
    let record = vec![msvc6_match::locate::Recorded {
        rva: AT_A,
        len: 0x20,
        member: "obj\\b.obj".into(),
        archive: "libfix".into(),
    }];
    let rep = msvc6_match::run(&img, &libs, &record, 1).unwrap();
    let b = by(&rep.run, "b.obj");
    assert_eq!(b.ranges, vec![(AT_A, AT_A + 0x20)]);
    assert!(b.tie_break[0].starts_with("RECORD names obj\\b.obj"));
    assert!(by(&rep.run, "a.obj").ranges.is_empty());
}

#[test]
fn the_copy_that_places_more_bytes_is_chosen_and_its_sections_are_the_witnesses() {
    let img = parsed(linked());
    let mut longer = a().bytes;
    longer.extend_from_slice(&[0x90; 0x30]);
    let old = library(&[
        (
            "obj\\a.obj",
            object(&[body("_alpha", &longer, &[(7, "_helper")])]),
        ),
        ("obj\\c.obj", object(&[c()])),
    ]);
    let new = library(&[
        ("obj\\a.obj", object(&[a()])),
        ("obj\\c.obj", object(&[c()])),
    ]);
    let libs = [
        offered("rtm/LIBFIX.LIB", old.clone()),
        offered("sp/libfix.lib", new.clone()),
        offered("again/LIBFIX.LIB", new),
    ];
    let rep = msvc6_match::run(&img, &libs, &[], 1).unwrap();
    assert_eq!(rep.groups.len(), 1);
    let g = &rep.groups[0];
    assert_eq!(g.file, "libfix.lib");
    assert_eq!(g.chosen_copy().unwrap().label, "sp/libfix.lib");
    assert!(!g.tie);
    assert_eq!(g.copies[2].same_as.as_deref(), Some("sp/libfix.lib"));
    assert_eq!((g.copies[0].covered, g.copies[1].covered), (0x20, 0x40));
    assert_eq!(g.witnesses.len(), 1);
    let w = &g.witnesses[0];
    assert_eq!((w.member.as_str(), w.symbol.as_str()), ("a.obj", "_alpha"));
    assert_eq!((w.rva, w.len), (AT_A, 0x20));
    assert_eq!(w.others[0].label, "rtm/LIBFIX.LIB");
    assert_eq!((w.others[0].len, w.others[0].placed), (Some(0x50), None));
    assert_eq!(rep.run.present().len(), 2);
    let tied = library(&[
        ("obj\\a.obj", object(&[a()])),
        ("obj\\c.obj", object(&[c()])),
        ("obj\\x.obj", object(&[gone()])),
    ]);
    let libs = [
        offered(
            "one/LIBFIX.LIB",
            library(&[
                ("obj\\a.obj", object(&[a()])),
                ("obj\\c.obj", object(&[c()])),
            ]),
        ),
        offered("two/LIBFIX.LIB", tied),
    ];
    let rep = msvc6_match::run(&img, &libs, &[], 1).unwrap();
    assert!(rep.groups[0].tie);
    assert_eq!(rep.groups[0].chosen_copy().unwrap().label, "one/LIBFIX.LIB");
}

#[test]
fn the_census_joins_each_stamp_with_the_placement_and_the_rich_header() {
    let mut pe = msvc6_fixture::pe::Pe::dll();
    pe.rich = vec![(msvc6_fixture::pe::comp_id(0x0a, 8168), 9)];
    pe.push(".text", msvc6_fixture::pe::SCN_CODE, linked());
    let img = msvc6_image::pe::Image::parse(pe.build()).unwrap();
    let rep = msvc6_match::run(&img, &[offered("x/LIBFIX.LIB", archive())], &[], 1).unwrap();
    let c = rep.census.iter().find(|b| b.prodid == Some(0x0a)).unwrap();
    assert_eq!(c.build, Some(8168));
    assert_eq!(c.rich, Some(9));
    assert_eq!(c.libraries[0].members, 8);
    assert_eq!(c.linked(), 6);
}
