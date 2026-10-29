#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_RDATA};
use msvc6_image::pe::Image;
use msvc6_match::sibling::code::{Code, is_code_section, is_protector_section, str_key};
use msvc6_match::sibling::conv::{Convention, clash, signature};
use msvc6_match::sibling::diff;
use msvc6_match::sibling::discover::{after_boundary, atoms, discover, prologue_at};
use msvc6_match::sibling::feat::{Callee, Feat, Fun, Side, Widths, build, width_clash};
use msvc6_match::sibling::pair::{
    Structure, Tok, gap_context_pairs, gap_pairs, pair, pair_with, shape, tokens,
};
use msvc6_match::sibling::report::{
    defined_name, file_runs, intruders, inversions, is_source_path, name_tokens, reversed_runs,
};
use msvc6_match::sibling::{parse_functions, parse_oracles};
use std::collections::{BTreeMap, BTreeSet};

fn fun(rva: u32) -> Fun {
    Fun {
        image: "x".into(),
        rva,
        end: rva + 1,
        name: format!("f{rva:x}"),
        file: String::new(),
    }
}

fn side(feats: Vec<Feat>) -> Side {
    let mut s = Side {
        funs: (0..feats.len())
            .map(|i| fun(u32::try_from(i).unwrap() * 16))
            .collect(),
        feats,
        ..Side::default()
    };
    s.finish();
    s
}

fn feat(strs: &[&str], calls: Vec<Callee>, n: usize) -> Feat {
    Feat {
        strs: strs.iter().map(|s| str_key(s)).collect(),
        raw: strs.iter().map(|s| (*s).to_string()).collect(),
        calls,
        mnem: vec![1; n],
        ..Feat::default()
    }
}

#[test]
fn string_keys_drop_trailing_punctuation() {
    assert_eq!(
        str_key("Oh no!! Send failed after many tries!"),
        "Oh no!! Send failed after many tries"
    );
    assert_eq!(str_key("  Seeking to: \n"), "Seeking to");
}

#[test]
fn atoms_join_only_packed_unaligned_starts() {
    let a = atoms(&[(0x10, 0x25), (0x25, 0x40), (0x40, 0x50), (0x55, 0x60)]);
    assert_eq!(a, vec![0, 0, 1, 2]);
}

#[test]
fn gaps_pair_equal_unpaired_runs_between_aligned_calls() {
    let a = vec![
        Tok::I("k!a".into()),
        Tok::U(5),
        Tok::U(6),
        Tok::P(1),
        Tok::U(7),
    ];
    let b = vec![
        Tok::I("k!a".into()),
        Tok::U(50),
        Tok::U(60),
        Tok::P(1),
        Tok::U(70),
    ];
    assert_eq!(
        gap_pairs(&a, &b, &|_, _| true, &mut BTreeSet::new()),
        vec![(5, 50), (6, 60), (7, 70)]
    );
    let c = vec![Tok::P(1), Tok::U(5), Tok::U(5), Tok::P(2)];
    let d = vec![Tok::P(1), Tok::U(9), Tok::P(2)];
    assert_eq!(
        gap_pairs(&c, &d, &|_, _| true, &mut BTreeSet::new()),
        vec![(5, 9)]
    );
    let e = vec![Tok::P(1), Tok::U(5), Tok::U(6), Tok::P(2)];
    assert!(gap_pairs(&e, &d, &|_, _| true, &mut BTreeSet::new()).is_empty());
    let f = vec![Tok::P(1), Tok::U(5), Tok::I("k!b".into()), Tok::P(2)];
    let g = vec![Tok::P(1), Tok::U(9), Tok::U(8), Tok::P(2)];
    assert!(gap_pairs(&f, &g, &|_, _| true, &mut BTreeSet::new()).is_empty());
}

#[test]
fn context_aligns_unequal_gaps_by_shared_anchors() {
    let a = vec![Tok::P(0), Tok::U(1), Tok::U(2), Tok::U(3), Tok::P(9)];
    let b = vec![Tok::P(0), Tok::U(7), Tok::U(8), Tok::P(9)];
    let compat = |p: usize, q: usize| match (p, q) {
        (1, 7) | (3, 8) => 0.5,
        (2, 8) => 0.2,
        _ => 0.0,
    };
    assert_eq!(gap_context_pairs(&a, &b, &compat), vec![(1, 7), (3, 8)]);
    let c = vec![Tok::U(1), Tok::U(3)];
    let d = vec![Tok::U(8), Tok::U(7)];
    assert_eq!(gap_context_pairs(&c, &d, &compat).len(), 1);
}

#[test]
fn a_callee_mapped_two_ways_is_dropped() {
    let a = vec![Tok::U(5), Tok::P(1), Tok::U(5)];
    let b = vec![Tok::U(8), Tok::P(1), Tok::U(9)];
    assert!(gap_pairs(&a, &b, &|_, _| true, &mut BTreeSet::new()).is_empty());
}

#[test]
fn unique_strings_pair_and_calls_spread() {
    let o = side(vec![
        feat(
            &["Engine init sequence"],
            vec![Callee::Fn(1), Callee::Fn(2)],
            30,
        ),
        feat(&[], vec![], 10),
        feat(&[], vec![Callee::Fn(3)], 10),
        feat(&[], vec![], 5),
    ]);
    let s = side(vec![
        feat(&[], vec![Callee::Fn(2)], 12),
        feat(
            &["Engine init sequence!"],
            vec![Callee::Fn(3), Callee::Fn(0)],
            40,
        ),
        feat(&[], vec![], 6),
        feat(&[], vec![], 11),
    ]);
    let p = pair(&o, &s, &[]);
    assert_eq!(p.o2s.get(&0), Some(&1));
    assert_eq!(p.o2s.get(&1), Some(&3));
    assert_eq!(p.o2s.get(&2), Some(&0));
    assert_eq!(p.o2s.get(&3), Some(&2));
    assert_eq!(p.why.get(&0).map(|w| w.0), Some(0));
    assert!(p.why.get(&1).is_some_and(|w| w.1.starts_with("call")));
}

#[test]
fn a_tie_lands_nothing() {
    let o = side(vec![
        feat(&["shared message text"], vec![], 10),
        feat(&["shared message text"], vec![], 10),
    ]);
    let s = side(vec![feat(&["shared message text"], vec![], 10)]);
    assert!(pair(&o, &s, &[]).o2s.is_empty());
}

#[test]
fn sole_callers_pair() {
    let o = side(vec![
        feat(&[], vec![Callee::Fn(1)], 10),
        feat(&["unique leaf string"], vec![], 10),
    ]);
    let s = side(vec![
        feat(&["unique leaf string"], vec![], 10),
        feat(&[], vec![Callee::Fn(0)], 10),
    ]);
    let p = pair(&o, &s, &[]);
    assert_eq!(p.o2s.get(&1), Some(&0));
    assert_eq!(p.o2s.get(&0), Some(&1));
    assert!(p.why.get(&0).is_some_and(|w| w.1.starts_with("caller")));
}

#[test]
fn name_tokens_read_code_names() {
    assert_eq!(name_tokens("Net::send"), vec!["Net::send"]);
    assert_eq!(name_tokens("read_frame()"), vec!["read_frame()"]);
    assert_eq!(
        name_tokens("Oh no!! Send failed! SendPkType / ALL <%d:%d>"),
        vec!["SendPkType"]
    );
    assert_eq!(
        name_tokens("InitMP: Engine_Init"),
        vec!["InitMP", "Engine_Init"]
    );
    assert!(name_tokens("Aborting due to a heap shortage!").is_empty());
}

#[test]
fn defined_names() {
    assert_eq!(
        defined_name("int Player::play_frame()").as_deref(),
        Some("Player::play_frame")
    );
    assert_eq!(
        defined_name("Pool::~Pool()").as_deref(),
        Some("Pool::~Pool")
    );
    assert_eq!(
        defined_name("void *Pool::Alloc(int n)").as_deref(),
        Some("Pool::Alloc")
    );
    assert_eq!(
        defined_name("int Rect::operator==(const Rect &r)").as_deref(),
        Some("Rect::operator==")
    );
    assert_eq!(defined_name("struct X {"), None);
}

#[test]
fn file_runs_intruders_inversions() {
    let s = ["a", "a", "b", "a", "c", "c", "a"];
    assert_eq!(file_runs(&s), vec![(0, 2), (2, 1), (3, 1), (4, 2), (6, 1)]);
    assert_eq!(
        intruders(&s, &[(0, 2), (3, 1), (6, 1)]),
        vec![(2, 1), (4, 2)]
    );
    assert_eq!(
        reversed_runs(&[1, 9, 8, 7, 10, 3, 2], 3),
        vec![(1, 3), (4, 3)]
    );
    assert_eq!(inversions(&[1, 2, 3]), (0, 3));
    assert_eq!(inversions(&[3, 2, 1]), (3, 3));
    assert!(is_source_path("C:\\dev\\sprite.cpp"));
    assert!(!is_source_path("engine.pcx"));
}

#[test]
fn oracle_and_function_tables_parse() {
    let v = parse_oracles("# c\nfirst\t/x/a.exe\nsecond\t/x/b.exe\t0x4855b0=0x4f2587,10=20\n\n")
        .unwrap();
    assert_eq!(v.len(), 2);
    assert_eq!(v[1].seeds, vec![(0x48_55b0, 0x4f_2587), (0x10, 0x20)]);
    assert!(parse_oracles("x\t/p\tnot-a-seed").is_err());
    assert!(parse_oracles("lonely").is_err());
    let v = parse_oracles("a\t/x/f.exe\t\t1.0\nb\t/x/b.exe\t1=2\t2.0\n").unwrap();
    assert_eq!(
        v.iter().map(|o| o.branch.as_str()).collect::<Vec<_>>(),
        ["1.0", "2.0"]
    );
    assert!(parse_oracles("x\t/p\t\t1.0\tmore").is_err());
    let f = parse_functions(
        "0x401000\tsrc/a.cpp\tA\n1010\tsrc/b.cpp\tB\n# c\n",
        0x40_0000,
    );
    assert_eq!(f.get(&0x1000).map(|x| x.1.as_str()), Some("A"));
    assert_eq!(f.get(&0x1010).map(|x| x.0.as_str()), Some("src/b.cpp"));
}

#[test]
fn prologues_follow_a_return_or_pad() {
    let code = [
        0xc3, 0x55, 0x8b, 0xec, 0x90, 0x55, 0x8b, 0xec, 0x01, 0x55, 0x8b, 0xec, 0xc2, 0x04, 0x00,
        0x55, 0x8b, 0xec,
    ];
    assert!(prologue_at(&code, 1));
    assert!(prologue_at(&code, 5));
    assert!(!prologue_at(&code, 9));
    assert!(prologue_at(&code, 15));
    assert!(after_boundary(&code, 0));
}

fn sig(b: &[u8]) -> Convention {
    signature(b, 0x1000, &|_| None)
}

#[test]
fn conventions_read_off_synthetic_bodies() {
    let cdecl = [0x56, 0x8b, 0x74, 0x24, 0x08, 0x5e, 0xc3];
    assert_eq!(sig(&cdecl).to_string(), "r0/?/1=");
    let thiscall = [0x8b, 0xc1, 0xc2, 0x04, 0x00];
    assert_eq!(sig(&thiscall).to_string(), "r4/t/0=");
    let stdcall = [
        0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x03, 0x45, 0x0c, 0x5d, 0xc2, 0x08, 0x00,
    ];
    assert_eq!(sig(&stdcall).to_string(), "r8/?/2=");
    let zeroes = [0x33, 0xc9, 0xc3];
    assert_eq!(sig(&zeroes).to_string(), "r0/n/0=");
    let saved = [0x51, 0xb9, 0x10, 0, 0, 0, 0x59, 0x8b, 0x01, 0xc3];
    assert_eq!(sig(&saved).this, Some(true));
    assert!(clash(&sig(&cdecl), &sig(&thiscall)).is_some());
    assert!(clash(&sig(&cdecl), &sig(&stdcall)).is_some());
    assert!(clash(&sig(&thiscall), &sig(&zeroes)).is_some());
    assert!(clash(&sig(&cdecl), &sig(&cdecl)).is_none());
}

#[test]
fn signatures_track_the_stack_and_a_switch_table() {
    let body = [
        0x56, 0x8b, 0x44, 0x24, 0x0c, 0x50, 0xe8, 0, 0, 0, 0, 0x8b, 0x4c, 0x24, 0x08, 0x5e, 0xc3,
    ];
    let s = sig(&body);
    assert_eq!((s.pops, s.args, s.exact), (Some(0), 2, false));
    let s = signature(&body, 0x1000, &|_| Some(4));
    assert_eq!((s.args, s.exact), (2, true));
    let mut sw = vec![
        0xff, 0x24, 0x85, 0x0d, 0x10, 0, 0, 0xc2, 0x04, 0x00, 0xc2, 0x04, 0x00,
    ];
    sw.extend_from_slice(&0x1007u32.to_le_bytes());
    sw.extend_from_slice(&0x100au32.to_le_bytes());
    let s = sig(&sw);
    assert_eq!((s.pops, s.exact), (Some(4), true));
}

#[test]
fn a_shifted_gap_with_one_clash_pairs_nothing_and_two_crossed_arms_pair() {
    let a = vec![Tok::P(0), Tok::U(1), Tok::U(2), Tok::U(3), Tok::U(4)];
    let b = vec![Tok::P(0), Tok::U(11), Tok::U(12), Tok::U(13), Tok::U(14)];
    let fits = |p: usize, q: usize| (p, q) != (4, 14);
    let mut refused = BTreeSet::new();
    assert!(gap_pairs(&a, &b, &fits, &mut refused).is_empty());
    assert_eq!(refused.len(), 4);
    let c = vec![
        Tok::I("k!a".into()),
        Tok::U(5),
        Tok::U(6),
        Tok::I("k!b".into()),
    ];
    let d = vec![
        Tok::I("k!a".into()),
        Tok::U(60),
        Tok::U(50),
        Tok::I("k!b".into()),
    ];
    let crossed = |p: usize, q: usize| (p, q) == (5, 50) || (p, q) == (6, 60);
    let mut none = BTreeSet::new();
    assert_eq!(
        gap_pairs(&c, &d, &crossed, &mut none),
        vec![(5, 50), (6, 60)]
    );
    assert!(none.is_empty());
}

#[test]
fn widths_and_shapes() {
    let (b, w) = (Widths { bytes: 8, words: 0 }, Widths { bytes: 0, words: 8 });
    assert!(width_clash(b, w) && width_clash(w, b));
    assert!(!width_clash(b, b));
    assert!(!width_clash(Widths { bytes: 3, words: 1 }, w));
    assert!((shape(&[1, 2, 3, 4], &[1, 2, 3, 4]) - 1.0).abs() < 1e-6);
    assert!(shape(&[1, 2, 3], &[4, 5, 6]) < 1e-6);
}

#[test]
fn an_unpaired_callee_of_every_function_holds_no_position() {
    let mut feats: Vec<Feat> = vec![feat(&[], vec![], 10), feat(&[], vec![], 10)];
    feats.extend((0..41).map(|_| feat(&[], vec![Callee::Fn(0)], 10)));
    feats.push(feat(&[], vec![Callee::Fn(0), Callee::Fn(1)], 10));
    let s = side(feats);
    let last = s.funs.len() - 1;
    assert_eq!(tokens(&s, last, &BTreeMap::new(), true), vec![Tok::U(1)]);
    let paired = BTreeMap::from([(0usize, 0usize)]);
    assert_eq!(tokens(&s, last, &paired, true), vec![Tok::P(0), Tok::U(1)]);
}

#[test]
fn vftable_slots_and_layout_pair_by_position() {
    let four = || side((0..4).map(|_| feat(&[], vec![], 10)).collect());
    let (o, s) = (four(), four());
    let st = Structure {
        ours: vec![Some(0), Some(1), None, Some(2)],
        sib: vec![Some(3), Some(2), Some(1), Some(0)],
        layout: false,
    };
    let p = pair_with(&o, &s, &[(0, 3)], &st);
    assert_eq!((p.o2s.get(&1), p.o2s.get(&2)), (Some(&2), Some(&0)));
    assert!(p.why.get(&1).is_some_and(|w| w.1.starts_with("vftable")));
    let st2 = Structure {
        ours: vec![Some(0), Some(1), Some(2), Some(3)],
        sib: vec![Some(3), Some(0), Some(2), Some(0)],
        layout: false,
    };
    let q = pair_with(&o, &s, &[(0, 3)], &st2);
    assert_eq!(q.o2s.get(&2), Some(&2));
    assert_eq!((q.o2s.get(&1), q.o2s.get(&3)), (None, None));
    let n = |k: usize| side((0..k).map(|_| feat(&[], vec![], 10)).collect());
    let layout = Structure {
        layout: true,
        ..Structure::default()
    };
    let r = pair_with(&n(4), &n(4), &[(0, 0), (3, 3)], &layout);
    assert_eq!((r.o2s.get(&1), r.o2s.get(&2)), (Some(&1), Some(&2)));
    assert!(r.why.get(&1).is_some_and(|w| w.1.starts_with("layout")));
    assert_eq!(
        pair_with(&n(5), &n(4), &[(0, 0), (4, 3)], &layout)
            .o2s
            .len(),
        2
    );
    assert_eq!(
        pair_with(&n(4), &n(4), &[(0, 0), (3, 3)], &Structure::default())
            .o2s
            .len(),
        2
    );
}

#[test]
fn protector_sections_are_never_code() {
    assert!(is_code_section(".text", 0x6000_0020));
    assert!(!is_code_section(".cms_t", 0x6000_0020));
    assert!(!is_code_section(".rdata", 0x4000_0040));
    assert!(is_protector_section("stxt371") && is_protector_section(".cms_d"));
    assert!(!is_protector_section("_SELFMOD"));
}

struct Built {
    image: Image,
}

fn pe_with(funcs: &[Vec<u8>], strings: &[&str]) -> (Built, Vec<u32>, Vec<u32>) {
    let mut pe = Pe::exe();
    let text_rva = pe.next_rva();
    let mut text = Vec::new();
    let mut starts = Vec::new();
    for f in funcs {
        starts.push(text_rva + u32::try_from(text.len()).unwrap());
        text.extend_from_slice(f);
        while text.len() % 16 != 0 {
            text.push(0xcc);
        }
    }
    pe.push(".text", SCN_CODE, text);
    let rdata_rva = pe.next_rva();
    let mut rdata = Vec::new();
    let mut at = Vec::new();
    for s in strings {
        at.push(rdata_rva + u32::try_from(rdata.len()).unwrap());
        rdata.extend_from_slice(s.as_bytes());
        rdata.push(0);
        while rdata.len() % 4 != 0 {
            rdata.push(0);
        }
    }
    pe.push(".rdata", SCN_RDATA, rdata);
    pe.entry = starts[0];
    let image = Image::parse(pe.build()).unwrap();
    (Built { image }, starts, at)
}

fn rel(from: u32, to: u32) -> [u8; 4] {
    (to.wrapping_sub(from + 5)).to_le_bytes()
}

fn caller(at: u32, string_va: u32, callees: &[u32]) -> Vec<u8> {
    let mut v = vec![0x68];
    v.extend_from_slice(&string_va.to_le_bytes());
    let mut ip = at + 5;
    for c in callees {
        v.push(0xe8);
        v.extend_from_slice(&rel(ip, *c));
        ip += 5;
    }
    v.extend_from_slice(&[0x59, 0xc3]);
    v
}

#[test]
fn a_sibling_pe_pairs_end_to_end_by_string_then_call_order() {
    let base = 0x40_0000u32;
    let probe = pe_with(
        &[vec![0xc3; 32], vec![0xc3], vec![0xc3]],
        &["an anchor string for pairing"],
    );
    let (s0, s1, s2) = (probe.1[0], probe.1[1], probe.1[2]);
    let sva = base + probe.2[0];
    let ours_f0 = caller(base + s0, sva, &[base + s1, base + s2]);
    let ours = pe_with(
        &[
            ours_f0,
            vec![0xb8, 1, 0, 0, 0, 0xc3],
            vec![0xb8, 2, 0, 0, 0, 0xc2, 4, 0],
        ],
        &["an anchor string for pairing"],
    );
    let sib_f0 = caller(base + s0, sva, &[base + s2, base + s1]);
    let sib = pe_with(
        &[
            sib_f0,
            vec![0x33, 0xc0, 0xc2, 4, 0],
            vec![0x83, 0xc8, 0xff, 0xc3],
        ],
        &["an anchor string for pairing"],
    );
    let mut o = Side::default();
    let funs: Vec<Fun> = ours
        .1
        .iter()
        .zip(["Run", "One", "Two"])
        .map(|(r, n)| Fun {
            image: "ours.exe".into(),
            rva: *r,
            end: r + 16,
            name: n.into(),
            file: "src/run.cpp".into(),
        })
        .collect();
    build(&Code::new(&ours.0.image), funs, &mut o);
    o.finish();
    let scode = Code::new(&sib.0.image);
    let mut s = Side::default();
    let found = discover(&scode, "sib");
    assert_eq!(found.iter().map(|f| f.rva).collect::<Vec<_>>(), sib.1);
    build(&scode, found, &mut s);
    s.finish();
    let p = pair(&o, &s, &[]);
    assert_eq!(p.o2s.get(&0), Some(&0));
    assert_eq!(p.o2s.get(&1), Some(&2), "{p:?}");
    assert_eq!(p.o2s.get(&2), Some(&1), "{p:?}");
    assert!(
        p.why
            .get(&0)
            .is_some_and(|w| w.1.contains("str:an anchor string"))
    );
    assert_eq!(o.feats[2].conv.pops, Some(4));
    assert_eq!(s.feats[1].conv.pops, Some(4));
}

#[test]
fn two_pairs_tables_compare_by_our_function() {
    let head = msvc6_match::sibling::report::PAIRS_HEAD;
    let row = |va: &str, sib: &str, ev: &str| {
        format!(
            "x.exe\t{va}\tf.cpp\tF\t{sib}\t1\t1\t0\t{ev}\t1.00\t10\t10\t\t\tr0/?/0=|r0/?/0=\t\n"
        )
    };
    let a = format!(
        "{head}\n{}{}",
        row("0x1", "0x10", "anchor:2.0:str:a,str:b"),
        row("0x2", "0x20", "anchor:1.0:imp:k!x")
    );
    let b = format!(
        "{head}\n{}{}",
        row("0x1", "0x10", "anchor:2.0:str:b,str:a"),
        row("0x3", "0x30", "anchor:1.0:imp:k!y")
    );
    let (h, x) = diff::parse_pairs(&a).unwrap();
    let (_, y) = diff::parse_pairs(&b).unwrap();
    let d = diff::pairs(&h, &x, &y);
    assert_eq!((d.both, d.same, d.reordered), (1, 1, 1));
    assert_eq!(d.lost.len(), 1);
    assert_eq!(d.gained.len(), 1);
    assert!(!d.equal());
    assert!(diff::parse_pairs("nope\n").is_err());
    assert_eq!(
        diff::first_difference("# x\na\nb\n", "a\nc\n"),
        Some((2, "b".into(), "c".into()))
    );
}
