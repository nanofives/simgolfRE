#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::{authored, linked, typed};
use msvc6_pdb::{dbi, eq, info, msf, msf7, structure, to70};

fn chain(name: &str, pdb: &[u8]) -> (to70::Converted, eq::Verdict) {
    let m = msf::Msf2::parse(pdb.to_vec()).unwrap();
    assert!(m.streams.len() >= 4, "{name}");
    let r = structure::check(pdb);
    assert_eq!(
        r.failures(),
        0,
        "{name}: {:?}",
        r.rows().filter(|r| !r.2.is_empty()).collect::<Vec<_>>()
    );
    let cv = to70::convert(pdb).unwrap();
    let m7 = msf7::Msf7::parse(cv.bytes.clone()).unwrap();
    assert_eq!(m7.used_but_free().unwrap().1, 0, "{name}");
    assert!(
        m7.stream(4).unwrap().is_empty(),
        "{name}: stream 4 is the absent IPI"
    );
    let v = eq::equivalence(pdb, &cv.bytes).unwrap();
    assert!(v.equivalent(), "{name}: {:?}", v.lines("2.0", "7.0"));
    assert_eq!(v.a, v.b);
    assert_eq!(
        to70::convert(pdb).unwrap().bytes,
        cv.bytes,
        "{name}: one 2.0 file always converts to the same bytes"
    );
    (cv, v)
}

#[test]
fn a_linked_fixture_pdb_writes_reads_checks_converts_and_reads_back_equivalent() {
    let pdb = linked();
    let (cv, v) = chain("linked", &pdb);
    assert_eq!(
        (cv.signature, cv.age),
        (msvc6_fixture::pdb::SIGNATURE, msvc6_fixture::pdb::AGE)
    );
    assert_eq!(cv.modules, 3);
    assert_eq!(cv.empty_modules, 1);
    assert_eq!(cv.lines.rows, 9);
    assert_eq!(cv.lines.in_procs, 8);
    assert_eq!(cv.lines.orphans, 1);
    assert_eq!(cv.module_syms.thunk_ords.get(&1), Some(&1));
    assert_eq!(cv.module_syms.thunk_ords.get(&2), Some(&1));
    assert_eq!(cv.module_syms.dirty_pads, 1);
    assert!(cv.global_refs > 0);
    assert!(!cv.empty_ec);
    assert_eq!(cv.info_tail, 4);
    assert_eq!(
        (
            v.a.procs,
            v.a.publics,
            v.a.thunks,
            v.a.labels,
            v.a.blocks,
            v.a.lines
        ),
        (3, 3, 2, 2, 1, 9)
    );
    let m7 = msf7::Msf7::parse(cv.bytes.clone()).unwrap();
    let i7 = info::read_info(&m7.stream(1).unwrap()).unwrap();
    assert_eq!(i7.version, info::VC70);
    assert_eq!(i7.guid, Some(info::guid_of(&pdb)));
    assert!(i7.names.contains_key(b"/LinkInfo".as_slice()));
    let names = msvc6_pdb::strtab::read(
        &m7.stream(msvc6_pdb::le::us(i7.names[b"/names".as_slice()]))
            .unwrap(),
    )
    .unwrap();
    assert!(names.values().any(|n| n == b"C:\\work\\a.h"));
    assert!(
        names.get(&1).is_some_and(|n| n == b"C:\\work\\a.cpp"),
        "the 2.0 table's offsets are kept"
    );
    let d = dbi::Dbi::parse(&m7.stream(3).unwrap()).unwrap();
    assert_eq!(d.version().unwrap(), dbi::V70);
    assert!(d.mods.iter().all(|m| m.stream().unwrap() != dbi::NO_STREAM));
}

#[test]
fn an_authored_pdb_writes_reads_checks_converts_and_reads_back_equivalent() {
    let pdb = authored();
    let (cv, v) = chain("authored", &pdb);
    assert_eq!((cv.signature, cv.age), (0x1234_5678, 2));
    assert!(cv.empty_ec);
    assert_eq!((v.a.procs, v.a.publics, v.a.data, v.a.types), (3, 1, 1, 2));
}

#[test]
fn a_changed_twin_is_not_equivalent() {
    let pdb = linked();
    let cv = to70::convert(&pdb).unwrap();
    let other = to70::convert(&authored()).unwrap();
    let v = eq::equivalence(&pdb, &other.bytes).unwrap();
    assert!(!v.equivalent());
    assert!(v.differences.iter().any(|d| d.contains("signature")));
    let mut m = msf7::Msf7::parse(cv.bytes).unwrap();
    let _ = &mut m;
}

#[test]
fn a_pdb_typed_from_c_headers_writes_reads_checks_converts_and_reads_back_equivalent() {
    let (pdb, t) = typed();
    assert!(
        t.skipped.is_empty() && t.unlaid.is_empty(),
        "{:?} {:?}",
        t.skipped,
        t.unlaid
    );
    assert_eq!(t.placeholders.iter().collect::<Vec<_>>(), ["FILE"]);
    let names: Vec<&str> = t.udts.iter().map(|(n, _)| n.as_str()).collect();
    assert_eq!(
        names,
        ["node", "packed", "cell", "table", "node", "u8", "visit_fn"]
            .iter()
            .copied()
            .filter({
                let mut seen = std::collections::BTreeSet::new();
                move |n| seen.insert(*n)
            })
            .collect::<Vec<_>>()
    );
    let (cv, v) = chain("typed", &pdb);
    assert_eq!(cv.modules, 1);
    assert_eq!(v.a.types, t.records.len());
    assert_eq!(v.a.procs, 3);
    assert_eq!(v.a.udts, t.udts.len());
}
