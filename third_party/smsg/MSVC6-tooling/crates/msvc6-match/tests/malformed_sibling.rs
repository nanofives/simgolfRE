#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_RDATA};
use msvc6_image::pe::Image;
use msvc6_match::sibling::code::Code;
use msvc6_match::sibling::conv::{WALK_STEPS, signature};
use msvc6_match::sibling::discover::discover;
use msvc6_match::sibling::feat::{Feat, Side, build};
use msvc6_match::sibling::pair::{LCS_CELLS, Tok, lcs, pair};
use msvc6_match::vftables;

fn quick<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
}

fn image(text: Vec<u8>, rdata: Vec<u8>) -> Image {
    let mut pe = Pe::exe();
    let t = pe.push(".text", SCN_CODE, text);
    pe.push(".rdata", SCN_RDATA, rdata);
    pe.entry = t;
    Image::parse(pe.build()).unwrap()
}

fn noise(n: usize, seed: u32) -> Vec<u8> {
    let mut x = seed;
    (0..n)
        .map(|_| {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            u8::try_from(x & 0xff).unwrap()
        })
        .collect()
}

fn side_of(img: &Image) -> Side {
    let code = Code::new(img);
    let funs = discover(&code, "x");
    let mut s = Side::default();
    build(&code, funs, &mut s);
    s.finish();
    s
}

fn bounded(s: &Side, text: usize) {
    assert!(s.funs.len() <= text);
    for f in &s.feats {
        assert!(f.calls.len() <= f.mnem.len());
        assert!(f.strs.len() <= f.mnem.len().saturating_mul(2));
    }
}

#[test]
fn random_code_and_pointer_soup_is_discovered_and_read_in_bounds() {
    for seed in [1u32, 7, 99] {
        let text = noise(1 << 16, seed);
        let mut rdata = Vec::new();
        for k in 0..4096u32 {
            rdata.extend_from_slice(&(0x40_1000 + (k * 37) % 0x1_0000).to_le_bytes());
        }
        let img = image(text, rdata);
        let s = quick(&format!("noise {seed}"), || side_of(&img));
        bounded(&s, 1 << 16);
        let runs = quick("vftable runs", || vftables::detect(&img, ".text", ".rdata")).unwrap();
        assert!(runs.iter().map(|r| r.slots.len()).sum::<usize>() <= 4096);
    }
}

#[test]
fn thunk_chains_and_self_loops_stop_at_their_hop_bound() {
    let mut text = Vec::new();
    for k in 0..4096u32 {
        let at = k * 5;
        let to = if k % 2 == 0 {
            at + 5
        } else {
            at.wrapping_sub(5)
        };
        text.push(0xe9);
        text.extend_from_slice(&to.wrapping_sub(at + 5).to_le_bytes());
    }
    let img = image(text, vec![0; 16]);
    let s = quick("thunk soup", || side_of(&img));
    bounded(&s, 4096 * 5);
}

#[test]
fn the_convention_walk_stops_at_its_step_bound() {
    let mut body = Vec::new();
    for _ in 0..(WALK_STEPS / 2) {
        body.extend_from_slice(&[0x74, 0x00]);
    }
    body.push(0xc3);
    let s = quick("a long branch chain", || {
        signature(&body, 0x1000, &|_| None)
    });
    assert!(s.pops.is_none() || s.pops == Some(0));
    let mut table = vec![0xff, 0x24, 0x85, 0x0d, 0x10, 0, 0];
    for _ in 0..5000 {
        table.extend_from_slice(&0x1007u32.to_le_bytes());
    }
    let s = quick("a jump table of five thousand slots", || {
        signature(&table, 0x1000, &|_| None)
    });
    assert!(!s.exact || s.pops.is_none());
    let back = [0xeb, 0xfe];
    let s = quick("a self loop", || signature(&back, 0x1000, &|_| None));
    assert_eq!(s.this, None);
}

#[test]
fn shared_strings_and_long_call_lists_stay_within_their_fan_out_and_cell_caps() {
    let feats: Vec<Feat> = (0..3000)
        .map(|k| Feat {
            strs: vec![
                "every function holds this".into(),
                format!("own string {k}"),
            ],
            mnem: vec![1; 20],
            ..Feat::default()
        })
        .collect();
    let mk = |feats: &[Feat]| {
        let mut s = Side {
            funs: (0..feats.len())
                .map(|k| msvc6_match::sibling::feat::Fun {
                    image: "x".into(),
                    rva: u32::try_from(k).unwrap() * 16,
                    end: u32::try_from(k).unwrap() * 16 + 1,
                    name: format!("f{k}"),
                    file: String::new(),
                })
                .collect(),
            feats: feats.to_vec(),
            ..Side::default()
        };
        s.finish();
        s
    };
    let (o, s) = (mk(&feats), mk(&feats));
    let p = quick("three thousand string anchors", || pair(&o, &s, &[]));
    assert_eq!(p.o2s.len(), 3000);
    let long: Vec<Tok> = (0..5000).map(Tok::U).collect();
    const { assert!(5000usize * 5000 > LCS_CELLS) };
    assert!(quick("an LCS past its cell cap", || lcs(&long, &long)).is_empty());
}

#[test]
fn hostile_annotation_tables_and_pairs_tables_are_refused_or_read_in_bounds() {
    use msvc6_match::annotations;
    let mut rows = String::from("image\tva\tkind\tpath\tnth\tanchor\tname\n");
    for k in 0..2000 {
        rows.push_str(&format!(
            "x.exe\t{k:#x}\tFUNCTION\ta.cpp\t{k}\tint f()\t-\n"
        ));
    }
    let parsed = quick("two thousand rows", || annotations::parse(&rows)).unwrap();
    let text = "int f()\n".repeat(2000);
    let code = annotations::code_lines(&text);
    let resolved = quick("two thousand occurrences", || {
        parsed
            .iter()
            .filter(|r| annotations::resolve(r, &code).is_ok())
            .count()
    });
    assert_eq!(resolved, 1999);
    assert!(annotations::parse("x\t0xzz\tF\tp\t0\ta\t-\n").is_err());
    assert!(annotations::parse("x\t0x1\tF\tp\tnth\ta\t-\n").is_err());
    let deep = format!("{}{}", "struct A {\n".repeat(5000), "}\n".repeat(5000));
    let _ = quick("deep classes", || annotations::enclosing_class(&deep, 5000));
    let head = msvc6_match::sibling::report::PAIRS_HEAD;
    let short = format!("{head}\na\tb\n");
    assert!(msvc6_match::sibling::diff::parse_pairs(&short).is_err());
    let twice = format!("{head}\n{0}\n{0}\n", vec!["x"; 16].join("\t"));
    assert!(msvc6_match::sibling::diff::parse_pairs(&twice).is_err());
}

#[test]
fn annotation_rows_resolve_through_a_line_index() {
    use msvc6_match::annotations;
    let lines = 200_000usize;
    let mut text = String::new();
    for k in 0..lines {
        text.push_str(&format!("int f{k}()\n"));
    }
    let mut rows = String::from("image\tva\tkind\tpath\tnth\tanchor\tname\n");
    for k in (0..lines).step_by(4) {
        rows.push_str(&format!(
            "x.exe\t{k:#x}\tFUNCTION\ta.cpp\t0\tint f{k}()\t-\n"
        ));
    }
    let parsed = annotations::parse(&rows).unwrap();
    let code = annotations::code_lines(&text);
    let resolved = quick(
        "fifty thousand rows over two hundred thousand lines",
        || {
            parsed
                .iter()
                .filter_map(|r| annotations::resolve(r, &code).ok())
                .collect::<Vec<_>>()
        },
    );
    assert_eq!(resolved.len(), lines / 4);
    assert_eq!(resolved.last().copied(), Some(lines - 3));
}

#[test]
fn traceback_spans_that_all_reach_back_to_the_code_start_are_refused() {
    use msvc6_fixture::mac::{BLR, Function, Pef, code_section};
    let functions: Vec<Function> = (0..50_000)
        .map(|k| Function {
            words: vec![BLR],
            name: format!(".f{k}"),
        })
        .collect();
    let (_, spans) = code_section(&functions);
    let mut bytes = Pef {
        functions,
        data: vec![0; 0x20],
        main: 8,
        ..Pef::default()
    }
    .build();
    let code_at = 40 + 3 * 28;
    for (_, at) in &spans {
        let field = code_at + usize::try_from(*at).unwrap() + 12;
        bytes[field..field + 4].copy_from_slice(&at.to_be_bytes());
    }
    let r = quick("fifty thousand tables over one span", || {
        msvc6_match::mac::side::pef_side("mac", &bytes, &std::collections::BTreeSet::new())
    });
    let Err(e) = r else {
        panic!("overlapping spans read")
    };
    assert!(format!("{e:#}").contains("tb_offsets overlap"), "{e:#}");
}
