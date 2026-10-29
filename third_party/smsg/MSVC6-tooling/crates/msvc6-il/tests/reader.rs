#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::il::{self as fx, Body, Def, Set};
use msvc6_il::grammar::{Grammar, Pragma};
use msvc6_il::inline::{Planner, Verdict, called, expanded};
use msvc6_il::unit::{FFLAG_CANDIDATE, FFLAG_COMPILE, FFLAG_DEFINED, FFLAG_TRY, Files, Unit};

fn grammar() -> Grammar {
    Grammar::from_parts(
        fx::fmt_table(0xbc),
        fx::attr_table(0xbe),
        fx::pragma_codes()
            .into_iter()
            .map(|codes| Pragma { codes, flag: 0 })
            .collect(),
    )
}

fn files(s: &Set) -> Files {
    Files {
        base: "toy.".into(),
        gl: s.gl(),
        sy: s.sy.0.clone(),
        ex: s.ex_bytes(),
        init: s.in_bytes(),
        pch: None,
        db: false,
    }
}

const CODE: u32 = FFLAG_DEFINED | FFLAG_COMPILE;
const INLINE: u32 = FFLAG_DEFINED | FFLAG_CANDIDATE | 0x80;

fn leaf(id: u32, name: &str, s6d: i16, fflags: u32) -> (Def, Body) {
    let mut b = Body::default();
    b.open(id, &[]).close(id + 0x1000);
    (fx::def(id, name, s6d, fflags), b)
}

fn calls(id: u32, name: &str, s6d: i16, fflags: u32, callees: &[u32]) -> (Def, Body) {
    let mut b = Body::default();
    b.open(id, &[]);
    for c in callees {
        b.call_void(*c);
    }
    b.close(id + 0x1000);
    (fx::def(id, name, s6d, fflags), b)
}

fn set(defs: Vec<(Def, Body)>) -> Set {
    let mut s = Set::new(0x200);
    s.srcfile = Some((0x1ff, "toy.cpp".into()));
    for (d, b) in defs {
        s.define(d, &b);
    }
    s
}

#[test]
fn a_set_dumps_its_header_definitions_lists_and_trees() {
    let s = set(vec![
        leaf(0x100, "?g@@YAXXZ", 10, INLINE),
        calls(0x101, "?f@@YAXXZ", 30, CODE, &[0x100]),
    ]);
    let f = files(&s);
    let text = msvc6_il::dump::dump(&f, &grammar(), &msvc6_il::dump::Opts::default()).unwrap();
    assert!(text.contains("0000  magic 0x11 tag 0x0386 stamp 0x0130ba96 next-id #200 lang 1 flags 0x2 front-end 12.00.8168"));
    assert!(text.contains("12 srcfile  #1ff \"toy.cpp\""));
    assert!(text.contains("0e fdef     #100 \"?g@@YAXXZ\" b30=0 type=void (0x7)"));
    assert!(text.contains("s6d=0xa fflags=0xc8"));
    assert!(text.contains("0000  5b    exhead body at 0x3f4"));
    assert!(text.contains("-- function #101:?f@@YAXXZ"));
    assert!(text.contains("; discard(call.void(?g@@YAXXZ))"));
    assert!(text.contains("first refs: ?f@@YAXXZ ?g@@YAXXZ"));
    assert!(text.contains("09 function #100:?g@@YAXXZ"));
    assert!(text.trim_end().ends_with("eof"));
}

#[test]
fn every_definition_counts_its_records_and_spans_its_ids() {
    let s = set(vec![
        leaf(0x100, "?g@@YAXXZ", 12, INLINE),
        calls(0x101, "?f@@YAXXZ", 16, CODE, &[0x100]),
    ]);
    let f = files(&s);
    let ids = msvc6_il::ids::load(&f, &grammar()).unwrap();
    assert_eq!(ids.funcs.len(), 2);
    assert_eq!(ids.funcs[0].recs, 12);
    assert_eq!(ids.funcs[1].recs, 16);
    let t = msvc6_il::ids::funcs_text(&ids);
    assert!(t.contains("?g@@YAXXZ"), "{t}");
    assert!(!t.contains("(!= s6d)"), "{t}");
    let i = msvc6_il::ids::ids_text(&ids);
    assert!(i.contains("#1ff  gl srcfile"), "{i}");
    assert!(i.contains("next-id #200"), "{i}");
}

fn plan(s: &Set, root: u32) -> msvc6_il::inline::Walk {
    let f = files(s);
    let u = Unit::read(&f).unwrap();
    let g = grammar();
    let mut p = Planner::new(&u, &f, &g);
    p.walk(root).unwrap()
}

#[test]
fn a_small_site_expands_free_and_a_large_one_over_the_budget_is_called() {
    let s = set(vec![
        leaf(0x100, "?small@@YAXXZ", 40, INLINE),
        leaf(0x102, "?large@@YAXXZ", 1001, INLINE),
        calls(0x101, "?f@@YAXXZ", 10, CODE, &[0x100, 0x102]),
    ]);
    let w = plan(&s, 0x101);
    assert_eq!(w.budget, 1000);
    assert_eq!(w.sites.len(), 2);
    assert_eq!(w.sites[0].verdict, Verdict::Free);
    assert_eq!(w.sites[1].verdict, Verdict::Over);
    assert_eq!(w.sites[1].left, 1000);
    assert_eq!(called(&w).get(&0x102), Some(&1));
    assert_eq!(expanded(&w).get(&0x100), Some(&1));
}

#[test]
fn an_expansion_takes_its_size_and_shares_the_rest_among_the_sites_left() {
    let s = set(vec![
        leaf(0x100, "?h@@YAXXZ", 300, INLINE),
        calls(0x102, "?g@@YAXXZ", 600, INLINE, &[0x100]),
        calls(0x101, "?f@@YAXXZ", 600, CODE, &[0x102, 0x100, 0x100]),
    ]);
    let w = plan(&s, 0x101);
    assert_eq!(w.budget, 1200);
    let g = &w.sites[0];
    assert_eq!(g.verdict, Verdict::Expand);
    assert_eq!(g.remain, 3);
    assert_eq!(g.share, (1200 - 600) / 3);
    assert_eq!(g.inner[0].verdict, Verdict::Over);
    assert_eq!(w.sites[1].left, 600);
    assert_eq!(w.sites[1].verdict, Verdict::Expand);
    assert_eq!(w.sites[2].left, 300);
    assert_eq!(w.sites[2].verdict, Verdict::Expand);
}

#[test]
fn a_callee_with_a_try_block_is_no_site() {
    let s = set(vec![
        leaf(0x100, "?t@@YAXXZ", 20, INLINE | FFLAG_TRY),
        calls(0x101, "?f@@YAXXZ", 10, CODE, &[0x100]),
    ]);
    let w = plan(&s, 0x101);
    assert_eq!(w.sites[0].verdict, Verdict::Try);
    assert!(!w.sites[0].verdict.is_site());
    assert_eq!(called(&w).get(&0x100), Some(&1));
}

#[test]
fn a_call_completes_after_its_arguments_calls() {
    let mut b = Body::default();
    b.open(0x101, &[])
        .call_with_call(0x102, 0x100)
        .close(0x1101);
    let s = set(vec![
        leaf(0x100, "?inner@@YAHXZ", 20, INLINE),
        leaf(0x102, "?outer@@YAXH@Z", 20, INLINE),
        (fx::def(0x101, "?f@@YAXXZ", 10, CODE), b),
    ]);
    let w = plan(&s, 0x101);
    let order: Vec<u32> = w.sites.iter().map(|x| x.callee).collect();
    assert_eq!(order, vec![0x100, 0x102]);
}

#[test]
fn a_local_object_is_destroyed_at_each_scope_exit_and_its_unwind_call_is_no_site() {
    let mut b = Body::default();
    b.open(0x101, &[])
        .local_object(0x103, 0x104, 0x1f0)
        .leave(1, 1)
        .unwind(1, 0xa1)
        .close(0x1101);
    let s = set(vec![
        leaf(0x103, "??0X@@QAE@XZ", 20, INLINE),
        leaf(0x104, "??1X@@QAE@XZ", 20, INLINE),
        (fx::def(0x101, "?f@@YAXXZ", 30, CODE | 0x100), b),
    ]);
    let w = plan(&s, 0x101);
    let order: Vec<u32> = w.sites.iter().map(|x| x.callee).collect();
    assert_eq!(order, vec![0x103, 0x104, 0x104]);
    assert_eq!(w.cleanups, vec![0x104]);
    let mut c = Body::default();
    c.open(0x105, &[])
        .local_object(0x103, 0x104, 0x1f0)
        .unwind(1, 1)
        .close(0x1105);
    let s = set(vec![
        leaf(0x103, "??0X@@QAE@XZ", 20, INLINE),
        leaf(0x104, "??1X@@QAE@XZ", 20, INLINE),
        (fx::def(0x105, "??0Y@@QAE@XZ", 30, CODE | 0x100), c),
    ]);
    let w = plan(&s, 0x105);
    let order: Vec<u32> = w.sites.iter().map(|x| x.callee).collect();
    assert_eq!(
        order,
        vec![0x103],
        "a constructor's end pops its subobjects' states and destroys none"
    );
}

#[test]
fn a_recursive_candidate_is_called_not_expanded() {
    let s = set(vec![
        calls(0x100, "?r@@YAXXZ", 20, INLINE | FFLAG_COMPILE, &[0x100]),
        calls(0x101, "?f@@YAXXZ", 10, CODE, &[0x100]),
    ]);
    let w = plan(&s, 0x101);
    assert_eq!(w.sites[0].verdict, Verdict::Free);
    assert_eq!(w.sites[0].inner[0].verdict, Verdict::Recursive);
    let roots = {
        let f = files(&s);
        let u = Unit::read(&f).unwrap();
        let g = grammar();
        Planner::new(&u, &f, &g).roots()
    };
    assert_eq!(roots, vec![0x100, 0x101]);
}

#[test]
fn the_listing_check_reads_each_procs_calls() {
    let l = "?f@@YAXXZ PROC NEAR\n\tcall\t?large@@YAXXZ\n?f@@YAXXZ ENDP\n";
    let p = msvc6_il::listing::procs(l);
    assert_eq!(p["?f@@YAXXZ"].calls, vec!["?large@@YAXXZ"]);
}

#[test]
fn a_call_after_an_unconditional_goto_is_dead_and_no_site() {
    let mut b = Body::default();
    b.open(0x101, &[]).call_void(0x100);
    b.0.byte(fx::OP_GOTO).id(0x1101);
    b.call_void(0x102).close(0x1101);
    let s = set(vec![
        leaf(0x100, "?live@@YAXXZ", 20, INLINE),
        leaf(0x102, "?dead@@YAXXZ", 2000, INLINE),
        (fx::def(0x101, "?f@@YAXXZ", 10, CODE), b),
    ]);
    let w = plan(&s, 0x101);
    let order: Vec<u32> = w.sites.iter().map(|x| x.callee).collect();
    assert_eq!(order, vec![0x100]);
    assert_eq!(called(&w).get(&0x102), None);
}

#[test]
fn the_listing_check_counts_calls_and_tail_jumps_and_skips_thunks() {
    let s = set(vec![
        leaf(0x102, "?large@@YAXXZ", 1001, INLINE),
        calls(0x101, "?f@@YAXXZ", 10, CODE, &[0x102, 0x102]),
    ]);
    let f = files(&s);
    let u = Unit::read(&f).unwrap();
    let g = grammar();
    let good = "?f@@YAXXZ PROC NEAR\n\tcall\t?large@@YAXXZ\n\tjmp\t?large@@YAXXZ\n?f@@YAXXZ ENDP\n";
    let c = msvc6_il::inline::check(&u, &f, &g, good, &[], false);
    assert_eq!((c.roots, c.pairs, c.agree), (1, 1, 1), "{c:?}");
    let bad = "?f@@YAXXZ PROC NEAR\n\tcall\t?large@@YAXXZ\n?f@@YAXXZ ENDP\n";
    let c = msvc6_il::inline::check(&u, &f, &g, bad, &[], true);
    assert_eq!((c.pairs, c.agree), (1, 0));
    assert_eq!(c.diffs[0].planned, 2);
    assert_eq!(c.diffs[0].listed, 1);
    assert_eq!(c.walks.len(), 1);
    assert!(msvc6_il::inline::is_thunk("?f@C@@W3AEXXZ"));
    assert!(msvc6_il::inline::is_thunk("?f@C@@$4PPPPPPPM@A@AEXXZ"));
    assert!(!msvc6_il::inline::is_thunk("?f@C@@QAEXXZ"));
    for (name, thunk) in [
        ("?f@?$T@UX@@H@@QAEXXZ", false),
        ("?f@?$T@UX@@PAH@@QAEXXZ", false),
        ("?f@?$T@VX@@X@@UAEXXZ", false),
        ("?f@?$T@UX@@H@@W3AEXXZ", true),
        ("?f@?$T@UX@@H@@$4PPPPPPPM@A@AEXXZ", true),
        ("??_9C@@$BA@AE", true),
        ("?g@@YAXXZ", false),
    ] {
        assert_eq!(msvc6_il::inline::is_thunk(name), thunk, "{name}");
    }
}

#[test]
fn a_pch_stream_definition_counts_the_records_of_its_body_in_the_pch() {
    let mut pch = Set::new(0x200);
    let (gd, gb) = leaf(0x102, "?h@@YAXXZ", 0, CODE);
    pch.define(gd, &gb);
    let mut held = pch.defs[0].clone();
    held.fflags |= msvc6_il::unit::FFLAG_PCH;
    let mut s = set(vec![leaf(0x101, "?f@@YAXXZ", 0, CODE)]);
    s.defs.push(held);
    let mut f = files(&s);
    let g = grammar();
    let recs = |f: &Files| -> Vec<(String, usize)> {
        let ids = msvc6_il::ids::load(f, &g).unwrap();
        ids.funcs.iter().map(|x| (x.name.clone(), x.recs)).collect()
    };
    let without = recs(&f);
    let own = without.iter().find(|(n, _)| n == "?f@@YAXXZ").unwrap().1;
    assert!(own > 0, "{without:?}");
    assert_eq!(
        without.iter().find(|(n, _)| n == "?h@@YAXXZ").map(|x| x.1),
        Some(0)
    );
    f.pch = Some(pch.ex_bytes());
    let with = recs(&f);
    assert_eq!(
        with.iter().find(|(n, _)| n == "?h@@YAXXZ").map(|x| x.1),
        Some(own),
        "{with:?}"
    );
    assert_eq!(
        with.iter().find(|(n, _)| n == "?f@@YAXXZ").map(|x| x.1),
        Some(own)
    );
}
