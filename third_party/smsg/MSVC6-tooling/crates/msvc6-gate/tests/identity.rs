#![allow(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing)]

use msvc6_diff::oracle::{ByteOracle as _, Members, Try};
use msvc6_diff::seat;
use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_DATA};
use msvc6_gate::{addrsym, calltargets, relocid};
use msvc6_image::pe::Image;
use msvc6_project::mapfile::Map;
use std::path::Path;

mod common;
use common::*;

struct Run {
    identical: bool,
    calltargets: calltargets::Report,
    addrsym: addrsym::Report,
    relocid: relocid::Report,
}

fn run(tag: &str, global: &str, callee: &str, global_rva: u32, callee_rva: u32) -> Run {
    let env = env();
    let dir = scratch(tag);
    let unit = caller(global, callee);
    let objs = objects(&dir, &unit);
    let identical = Try::new(&env, F, Members::Auto(Vec::new()))
        .score(&unit)
        .unwrap()
        .identical();
    let ct = calltargets::measure(&calltargets::Inputs {
        env: &env,
        objs: &objs,
        census: true,
        map_data: &Default::default(),
    })
    .unwrap();
    let seats = seat::seat_map(&env, &objs, 1);
    assert!(seats.refused.is_empty(), "{:?}", seats.refused);
    let ours = image(global_rva, callee_rva);
    let map = Map::parse(Path::new("ours.map"), &map_text()).unwrap();
    let linked = addrsym::Linked::new(&ours, &map);
    let ad = addrsym::run(&env, &seats.seats, &linked, &[F], true, false, 1).unwrap();
    let rel = relocid::measure(&relocid::Inputs {
        env: &env,
        objs: &objs,
        seats: &seats.seats,
        archives: &[],
        notes_only: &[],
        only: None,
        jobs: 1,
    });
    std::fs::remove_dir_all(&dir).ok();
    Run {
        identical,
        calltargets: ct,
        addrsym: ad,
        relocid: rel,
    }
}

#[test]
fn a_planted_wrong_callee_and_wrong_global_pass_try_and_fail_the_identity_gates() {
    let r = run("planted", "_b", "_wrong", B, WRONG);
    assert!(r.identical, "try masks both relocated fields");
    let w = &r.calltargets.wrong;
    assert_eq!(w.len(), 1, "{w:?}");
    assert_eq!(
        (
            w[0].symbol.as_str(),
            w[0].symbol_is_at,
            w[0].reference_targets
        ),
        ("_wrong", WRONG, RIGHT)
    );
    assert_eq!(r.calltargets.uncovered.get("_b"), Some(&1));
    let mismatched: Vec<(&str, &str, &str)> = r
        .addrsym
        .rows
        .iter()
        .filter(|x| x.verdict == "MISMATCH")
        .map(|x| (x.reloc.as_str(), x.ours.as_str(), x.image.as_str()))
        .collect();
    assert_eq!(
        mismatched,
        [("_b", "_b", "_a"), ("_wrong", "_wrong", "_right")],
        "{:?}",
        r.addrsym.rows
    );
    assert_eq!(r.addrsym.mismatches(), 2);
    let found: Vec<(&str, relocid::Verdict)> = r
        .relocid
        .rows
        .iter()
        .filter(|x| x.verdict.finding())
        .map(|x| (x.symbol.as_str(), x.verdict))
        .collect();
    assert_eq!(
        found,
        [
            ("_b", relocid::Verdict::Mismatch),
            ("_wrong", relocid::Verdict::Mismatch)
        ]
    );
    let lines = calltargets::report_lines(&env(), &r.calltargets, false);
    assert!(
        lines.iter().any(|l| l.contains("WRONG CALLEE")),
        "{lines:#?}"
    );
    let c = calltargets::causes(&env(), &r.calltargets);
    assert!(
        c.rows.lines().nth(1).is_some_and(|l| l.contains("_wrong")),
        "{}",
        c.rows
    );
}

#[test]
fn the_right_callee_and_global_pass_every_gate() {
    let r = run("control", "_a", "_right", A, RIGHT);
    assert!(r.identical);
    assert!(r.calltargets.wrong.is_empty());
    assert_eq!(r.calltargets.agreed, 1);
    assert_eq!(r.addrsym.mismatches(), 0, "{:?}", r.addrsym.rows);
    assert_eq!(r.addrsym.total.get("AGREE"), Some(&2));
    assert_eq!(r.relocid.findings(), 0, "{:?}", r.relocid.rows);
    assert_eq!(r.relocid.checked, 3);
}

#[test]
fn a_stale_linked_image_is_refused_by_the_address_screen() {
    let env = env();
    let dir = scratch("stale");
    let objs = objects(&dir, &caller("_a", "_right"));
    let seats = seat::seat_map(&env, &objs, 1);
    let mut pe = Pe::exe();
    let mut t = linked_text(A, RIGHT);
    t[0] = 0x90;
    pe.push(".text", SCN_CODE, t);
    pe.push(".data", SCN_DATA, vec![0; 8]);
    let ours = Image::parse(pe.build()).unwrap();
    let map = Map::parse(Path::new("ours.map"), &map_text()).unwrap();
    let linked = addrsym::Linked::new(&ours, &map);
    let e = addrsym::run(&env, &seats.seats, &linked, &[F], true, false, 1).unwrap_err();
    assert!(format!("{e:#}").contains("not this build's"), "{e:#}");
    std::fs::remove_dir_all(&dir).ok();
}

fn one_object_caller(callee: &str) -> Vec<u8> {
    use msvc6_fixture::coff::{
        Object, REL_DIR32, REL_REL32, Reloc, SCN_COMDAT, SCN_TEXT, SELECT_NODUPLICATES, Section,
    };
    let mut o = Object::new();
    let mut syms = Vec::new();
    for (name, body) in [
        (F_NAME, f_body()),
        ("_right", RIGHT_BODY.to_vec()),
        ("_wrong", WRONG_BODY.to_vec()),
    ] {
        let s = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: body,
            ..Section::default()
        });
        o.section_symbol(s, SELECT_NODUPLICATES, 0);
        syms.push((name, o.external(name, 0, s)));
    }
    let a = o.undefined("_a");
    let target = syms.iter().find(|(n, _)| *n == callee).unwrap().1;
    o.sections[0].relocs = vec![
        Reloc {
            va: 0x04,
            symbol: a,
            typ: REL_DIR32,
        },
        Reloc {
            va: 0x0a,
            symbol: target,
            typ: REL_REL32,
        },
    ];
    o.build()
}

#[test]
fn a_wrong_callee_the_caller_s_own_object_defines_is_relocid_s_and_not_calltargets() {
    let env = env();
    for (callee, wrong) in [("_right", false), ("_wrong", true)] {
        let dir = scratch(&format!("own-callee{callee}"));
        let p = dir.join("Unit.obj");
        msvc6_base::srcio::write_bytes(&p, &one_object_caller(callee)).unwrap();
        let objs = vec![p];
        let ct = calltargets::measure(&calltargets::Inputs {
            env: &env,
            objs: &objs,
            census: true,
            map_data: &Default::default(),
        })
        .unwrap();
        assert_eq!((ct.checked, ct.wrong.len()), (0, 0), "{callee}");
        let seats = seat::seat_map(&env, &objs, 1);
        assert!(seats.refused.is_empty(), "{:?}", seats.refused);
        let rel = relocid::measure(&relocid::Inputs {
            env: &env,
            objs: &objs,
            seats: &seats.seats,
            archives: &[],
            notes_only: &[],
            only: None,
            jobs: 1,
        });
        let found: Vec<(&str, relocid::Verdict)> = rel
            .rows
            .iter()
            .filter(|x| x.verdict.finding())
            .map(|x| (x.symbol.as_str(), x.verdict))
            .collect();
        if wrong {
            assert_eq!(
                found,
                [("_wrong", relocid::Verdict::Mismatch)],
                "{:?}",
                rel.rows
            );
        } else {
            assert!(found.is_empty(), "{:?}", rel.rows);
        }
        std::fs::remove_dir_all(&dir).ok();
    }
}
