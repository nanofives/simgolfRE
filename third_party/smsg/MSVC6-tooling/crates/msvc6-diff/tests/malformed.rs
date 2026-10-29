#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::*;
use msvc6_base::srcio;
use msvc6_diff::comdat;
use msvc6_diff::env::Env;
use msvc6_diff::rawdiff::{self, Ctx};
use msvc6_diff::resolve::Names;
use msvc6_diff::seat;
use msvc6_diff::segments::{self, MAX_SEGMENT};
use msvc6_diff::segrank;
use msvc6_diff::{blockmap, commswap, decisions, segasm};
use msvc6_fixture::backstop;
use msvc6_fixture::coff::{
    Object as Obj, REL_DIR32, Reloc as Rel, SCN_COMDAT, SCN_TEXT, SELECT_ANY, Section,
};
use msvc6_image::coff::Object;
use msvc6_project::names::{Kind, Name, NameOracle};
use std::collections::HashSet;
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::path::{Path, PathBuf};
use std::time::Duration;

type Spans = Vec<(u32, u32)>;

struct Run {
    dir: PathBuf,
    fails: Vec<String>,
    cases: usize,
    slowest: (Duration, String),
}

impl Run {
    fn new(tag: &str) -> Self {
        let dir =
            std::env::temp_dir().join(format!("msvc6-diff-malformed-{tag}-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        Self {
            dir,
            fails: Vec::new(),
            cases: 0,
            slowest: (Duration::ZERO, String::new()),
        }
    }

    fn timed<T>(&mut self, what: &str, f: impl FnOnce() -> T) -> Option<T> {
        let (r, took) = backstop::run(what, || catch_unwind(AssertUnwindSafe(f)));
        if took > self.slowest.0 {
            self.slowest = (took, what.to_string());
        }
        match r {
            Ok(v) => Some(v),
            Err(_) => {
                self.fails.push(format!("{what}: panicked"));
                None
            }
        }
    }

    fn object(&mut self, env: &Env, case: &str, d: &[u8]) {
        self.cases += 1;
        let path = self.dir.join(format!("case{}.obj", self.cases));
        srcio::write_bytes(&path, d).unwrap();
        self.reads(env, case, &path, d);
    }

    fn reads(&mut self, env: &Env, case: &str, path: &Path, d: &[u8]) {
        let what = |r: &str| format!("{case}: {r}");
        let text = self
            .timed(&what("text_of"), || comdat::text_of(d))
            .and_then(Result::ok);
        self.timed(&what("data_of"), || comdat::data_of(d).map(|v| v.len()));
        self.timed(&what("defined_externals"), || {
            comdat::defined_externals(d).map(|v| v.len())
        });
        let Some(text) = text else {
            return;
        };
        self.timed(&what("members_of"), || {
            seat::members_of(env, path, d, &text).map(|v| v.len())
        });
        let list = vec![path.to_path_buf()];
        let Some(seats) = self.timed(&what("seat_map"), || seat::seat_map(env, &list, 1).seats)
        else {
            return;
        };
        let definer = segasm::definers(&list);
        for c in text.iter().take(4) {
            for rva in [env.text_rva, env.text_rva + CALLEE] {
                let a = self
                    .timed(&what(&format!("analyze_in {rva:#x}")), || {
                        segments::analyze_in(env, rva, path, c.clone(), MAX_SEGMENT)
                    })
                    .and_then(Result::ok);
                let Some(a) = a else { continue };
                let all: Vec<usize> = (0..a.segs.len()).collect();
                self.timed(&what(&format!("segasm::report {rva:#x}")), || {
                    segasm::report(env, &a, &all, &definer).len()
                });
                self.timed(&what(&format!("decisions::find {rva:#x}")), || {
                    let offs: HashSet<usize> =
                        commswap::find(&a).iter().map(|s| s.img_off).collect();
                    decisions::find(&a, &offs).len()
                });
            }
            self.timed(&what("blockmap"), || {
                let code = blockmap::split(&c.bytes, 0, 0, Some(&[]), u32::MAX);
                blockmap::render_block_map(&blockmap::blocks(&code, 0), &code, 0, 0, Some(0x40))
                    .len()
            });
        }
        let Some(ctx) = self.timed(&what("Ctx::build"), || Ctx::build(env, &list, &seats)) else {
            return;
        };
        for c in text.iter().take(4) {
            for want in [c.bytes.len(), CODE_LEN, 0, usize::MAX] {
                self.timed(&what(&format!("rawdiff::of want {want:#x}")), || {
                    rawdiff::of(env, &ctx, env.text_rva, path, c, want).raw
                });
            }
        }
        let mut rvas: Vec<u32> = seats.keys().copied().collect();
        rvas.extend([env.text_rva, u32::MAX, 0]);
        self.timed(&what("segrank::rank"), || {
            segrank::rank(env, &list, &seats, &rvas, 1).rows.len()
        });
    }

    fn finish(self) {
        println!(
            "malformed: {} case(s), the slowest read {:?} ({})",
            self.cases, self.slowest.0, self.slowest.1
        );
        std::fs::remove_dir_all(&self.dir).ok();
        assert!(self.cases > 0);
        assert!(
            self.fails.is_empty(),
            "{} of {} case(s) failed:\n{}",
            self.fails.len(),
            self.cases,
            self.fails.join("\n")
        );
    }
}

fn put16(d: &mut [u8], at: usize, v: u16) {
    if let Some(s) = d.get_mut(at..at + 2) {
        s.copy_from_slice(&v.to_le_bytes());
    }
}

fn put32(d: &mut [u8], at: usize, v: u32) {
    if let Some(s) = d.get_mut(at..at + 4) {
        s.copy_from_slice(&v.to_le_bytes());
    }
}

fn edges32(len: usize) -> Vec<u32> {
    let l = u32::try_from(len).unwrap();
    vec![0, 1, 0x7fff, 0xffff, 0xffff_ffff, l, l + 1, l - 1]
}

const EDGES16: [u16; 4] = [0, 1, 0x7fff, 0xffff];

#[test]
fn the_base_object_reads_and_seats() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let text = comdat::text_of(&d).unwrap();
    assert_eq!(text.len(), 1);
    assert_eq!(text[0].relocations.len(), 7);
    let mut run = Run::new("base");
    let path = run.dir.join("base.obj");
    srcio::write_bytes(&path, &d).unwrap();
    let seats = seat::seat_map(&e, std::slice::from_ref(&path), 1).seats;
    assert_eq!(seats.keys().copied().collect::<Vec<_>>(), [f.text]);
    let a = segments::analyze_in(&e, f.text, &path, text[0].clone(), MAX_SEGMENT).unwrap();
    assert!(!a.segs.is_empty());
    let r = segrank::rank(&e, std::slice::from_ref(&path), &seats, &[f.text], 1);
    assert_eq!(r.rows.len(), 1, "{:?}", r.errors);
    assert_eq!(r.rows[0].edit, 0);
    run.object(&e, "base", &d);
    run.finish();
}

#[test]
fn an_object_that_cannot_be_read_is_refused_by_name_not_dropped() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let run = Run::new("refused");
    let good = run.dir.join("good.obj");
    srcio::write_bytes(&good, &d).unwrap();
    let mut cut = d.clone();
    cut.truncate(d.len() / 2);
    let half = run.dir.join("half.obj");
    srcio::write_bytes(&half, &cut).unwrap();
    let junk = run.dir.join("junk.obj");
    srcio::write_bytes(&junk, b"not an object").unwrap();
    let gone = run.dir.join("gone.obj");
    let list = vec![half.clone(), good.clone(), junk.clone(), gone.clone()];
    let map = seat::seat_map(&e, &list, 2);
    assert_eq!(map.seats.keys().copied().collect::<Vec<_>>(), [f.text]);
    assert_eq!(map.seats[&f.text].0, good);
    let refused: Vec<&PathBuf> = map.refused.iter().map(|(o, _)| o).collect();
    assert_eq!(refused, [&half, &junk, &gone]);
    assert!(map.refused.iter().all(|(_, e)| !e.is_empty()));
    assert_eq!(
        segrank::refused_line(&map.refused, list.len()),
        "REFUSED: 3 of 4 object(s) unread, their COMDATs seat nothing: half.obj, junk.obj, gone.obj"
    );
    let clean = seat::seat_map(&e, std::slice::from_ref(&good), 1);
    assert!(clean.refused.is_empty(), "{:?}", clean.refused);
    std::fs::remove_dir_all(&run.dir).ok();
}

#[test]
fn every_file_header_field_at_an_edge_value_is_read_or_refused_quickly() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let mut run = Run::new("header");
    for (at, wide) in [
        (0, false),
        (2, false),
        (4, true),
        (8, true),
        (12, true),
        (16, false),
        (18, false),
    ] {
        if wide {
            for v in edges32(d.len()) {
                let mut m = d.clone();
                put32(&mut m, at, v);
                run.object(&e, &format!("header +{at} = {v:#x}"), &m);
            }
        } else {
            for v in EDGES16 {
                let mut m = d.clone();
                put16(&mut m, at, v);
                run.object(&e, &format!("header +{at} = {v:#x}"), &m);
            }
        }
    }
    run.finish();
}

#[test]
fn every_section_header_field_at_an_edge_value_is_read_or_refused_quickly() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let nsec = Object::parse(&d).unwrap().sections.len();
    let mut run = Run::new("section");
    for s in 0..nsec {
        let h = 20 + 40 * s;
        for name in [
            &b"/9999999"[..],
            b"/0\0\0\0\0\0\0",
            b"/x\0\0\0\0\0\0",
            &[0xff; 8],
        ] {
            let mut m = d.clone();
            m[h..h + 8].copy_from_slice(name);
            run.object(&e, &format!("section {s} name {name:?}"), &m);
        }
        for at in [8, 12, 16, 20, 24, 28, 36] {
            for v in edges32(d.len()) {
                let mut m = d.clone();
                put32(&mut m, h + at, v);
                run.object(&e, &format!("section {s} +{at} = {v:#x}"), &m);
            }
        }
        for at in [32, 34] {
            for v in EDGES16 {
                let mut m = d.clone();
                put16(&mut m, h + at, v);
                run.object(&e, &format!("section {s} +{at} = {v:#x}"), &m);
            }
        }
    }
    run.finish();
}

#[test]
fn a_truncated_object_is_read_or_refused_quickly() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let mut run = Run::new("truncated");
    for n in (0..d.len()).step_by(3) {
        run.object(&e, &format!("cut at {n}"), &d[..n]);
    }
    run.finish();
}

#[test]
fn sections_sharing_a_relocation_table_are_refused() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let o = Object::parse(&d).unwrap();
    let text = o.sections[0].reloc_ptr;
    let mut m = d.clone();
    put32(&mut m, 20 + 40 + 24, text);
    put16(&mut m, 20 + 40 + 32, 2);
    let err = comdat::text_of(&m).unwrap_err();
    assert!(
        format!("{err:#}").contains("share relocation bytes"),
        "{err:#}"
    );
    let mut run = Run::new("shared");
    run.object(&e, "shared relocation table", &m);
    let mut inside = d.clone();
    put32(&mut inside, 20 + 24, o.sections[0].raw_ptr);
    run.object(&e, "relocation table over the section data", &inside);
    run.finish();
}

fn reloc_at(d: &[u8], k: usize) -> usize {
    let o = Object::parse(d).unwrap();
    o.sections[0].reloc_ptr as usize + 10 * k
}

#[test]
fn a_relocation_naming_an_aux_record_or_past_the_table_is_bounded() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let nsyms = Object::parse(&d).unwrap().header.symbols;
    let mut run = Run::new("relsym");
    for k in 0..7 {
        for v in [1, 6, nsyms, nsyms + 1, 0xffff_ffff] {
            let mut m = d.clone();
            put32(&mut m, reloc_at(&d, k) + 4, v);
            run.object(&e, &format!("reloc {k} symbol {v:#x}"), &m);
        }
        for t in [0u16, 1, 2, 0xffff] {
            let mut m = d.clone();
            put16(&mut m, reloc_at(&d, k) + 8, t);
            run.object(&e, &format!("reloc {k} type {t:#x}"), &m);
        }
    }
    run.finish();
}

#[test]
fn a_relocation_field_past_its_section_is_refused() {
    let f = fixture();
    let e = base_env(&f);
    let d = object();
    let size = u32::try_from(CODE_LEN).unwrap();
    let mut run = Run::new("relva");
    for v in [size - 3, size - 1, size, size + 1, 0xffff_fffc, 0xffff_ffff] {
        let mut m = d.clone();
        put32(&mut m, reloc_at(&d, 3), v);
        let err = comdat::text_of(&m).unwrap_err();
        assert!(
            format!("{err:#}").contains("runs past the section"),
            "{err:#}"
        );
        run.object(&e, &format!("reloc va {v:#x}"), &m);
    }
    let mut mid = d.clone();
    put32(&mut mid, reloc_at(&d, 3), 0x1e);
    run.object(&e, "reloc inside an opcode", &mid);
    run.finish();
}

#[test]
fn a_string_table_with_no_nul_and_long_names_is_bounded() {
    let f = fixture();
    let e = base_env(&f);
    let mut run = Run::new("strings");
    let d = object_with(code(), 2_000);
    let o = Object::parse(&d).unwrap();
    let strtab = o.header.symbol_table as usize + 18 * o.header.symbols as usize;
    let mut open = d.clone();
    for b in open.iter_mut().skip(strtab + 4) {
        if *b == 0 {
            *b = b'z';
        }
    }
    open.extend(std::iter::repeat_n(b'q', 1 << 20));
    run.object(&e, "no NUL, a megabyte after the table", &open);
    let mut wide = open.clone();
    put32(&mut wide, strtab, u32::MAX);
    run.object(&e, "no NUL, the table's size past the file", &wide);
    let mut zero = d.clone();
    put32(&mut zero, strtab, 0);
    run.object(&e, "a string table of size 0", &zero);
    run.finish();
}

#[test]
fn many_relocations_and_a_chained_switch_table_are_bounded() {
    let f = fixture();
    let e = base_env(&f);
    let mut run = Run::new("many");
    let mut code = Vec::new();
    let mut rel = Vec::new();
    for k in 0..3000u32 {
        code.extend_from_slice(&[0xa1, 0, 0, 0, 0]);
        rel.push(Rel {
            va: 5 * k + 1,
            symbol: 3,
            typ: REL_DIR32,
        });
    }
    code.push(0xc3);
    let mut o = Obj::new();
    let text = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: code,
        relocs: rel,
        ..Section::default()
    });
    o.section_symbol(text, SELECT_ANY, 0);
    o.external("?f@@YAXXZ", 0, text);
    o.undefined("_g");
    run.object(&e, "3000 DIR32 loads", &o.build());

    let jumps = 400u32;
    let entries = 2000u32;
    let mut code = Vec::new();
    let mut rel = Vec::new();
    let table = 7 * jumps + 1;
    for k in 0..jumps {
        code.extend_from_slice(&[0xff, 0x24, 0x85]);
        code.extend_from_slice(&(table + 4 * (k % entries)).to_le_bytes());
        rel.push(Rel {
            va: 7 * k + 3,
            symbol: 0,
            typ: REL_DIR32,
        });
    }
    code.push(0xc3);
    for k in 0..entries {
        let next = table + 4 * ((k + 1) % entries);
        code.extend_from_slice(&next.to_le_bytes());
        rel.push(Rel {
            va: table + 4 * k,
            symbol: 0,
            typ: REL_DIR32,
        });
    }
    let mut o = Obj::new();
    let text = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: code,
        relocs: rel,
        ..Section::default()
    });
    o.section_symbol(text, SELECT_ANY, 0);
    o.external("?f@@YAXXZ", 0, text);
    let chained = o.build();
    let (end, _) = backstop::run("a chained switch table's reachable end", || {
        let c = comdat::text_of(&chained).unwrap();
        segments::reachable_end(&c[0].bytes, &c[0].relocated, &|o| {
            segments::our_table_targets(&c[0], o)
        })
    });
    assert!(end > 0);
    run.object(&e, "a chained switch table read by 400 jumps", &chained);
    run.finish();
}

#[test]
fn a_jump_into_the_middle_of_an_instruction_is_bounded() {
    let f = fixture();
    let e = base_env(&f);
    let mut run = Run::new("midjump");
    let mut code = vec![0xeb, 0x01, 0xb8, 0xc3, 0x00, 0x00, 0x00, 0x74, 0xfa, 0xe9];
    code.extend_from_slice(&(-8i32).to_le_bytes());
    code.extend_from_slice(&[0xc3, 0xcc]);
    let mut o = Obj::new();
    let text = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: code.clone(),
        ..Section::default()
    });
    o.section_symbol(text, SELECT_ANY, 0);
    o.external("?f@@YAXXZ", 0, text);
    let d = o.build();
    let end = segments::reachable_end(&code, &[false; 16], &|_| Vec::new());
    assert!(end <= code.len());
    run.object(&e, "jumps into an instruction", &d);
    run.finish();
}

#[test]
fn hostile_extents_and_seats_are_refused_or_bounded() {
    let f = fixture();
    let d = object();
    let mut run = Run::new("extents");
    let last = f.text + f.text_len - 1;
    let cases: Vec<(&str, Spans, Spans)> = vec![
        (
            "an extent past the section",
            vec![(f.text, 0x7fff_ffff)],
            Vec::new(),
        ),
        (
            "an extent of u32::MAX",
            vec![(f.text, u32::MAX)],
            Vec::new(),
        ),
        ("no recorded extent", Vec::new(), Vec::new()),
        ("a seat at the last byte", vec![(last, 1)], Vec::new()),
        ("a seat past the image", vec![(u32::MAX - 1, 1)], Vec::new()),
        (
            "a next start one byte on",
            vec![(f.text + 1, 4)],
            Vec::new(),
        ),
        (
            "a text pad past everything",
            vec![(f.text, u32::try_from(CODE_LEN).unwrap())],
            vec![(f.text + 4, u32::MAX), (u32::MAX, 4)],
        ),
    ];
    for (name, seats, pad) in cases {
        let e = env(&f, seats, pad);
        run.object(&e, name, &d);
    }
    let mut names = NameOracle::default();
    names.add(
        "tsv",
        "fixture",
        vec![Name {
            rva: last,
            name: "?f@@YAXXZ".into(),
            kind: Kind::Label,
            size: None,
            scope: None,
            provider: "tsv".into(),
        }],
        Vec::new(),
    );
    let at_end = Env::new(
        "FIX",
        f.img.clone(),
        Names::from_oracle(&names),
        vec![(last, 1)],
        Vec::new(),
    )
    .unwrap();
    run.object(&at_end, "our function seated at the last byte", &d);
    let path = run.dir.join("last.obj");
    srcio::write_bytes(&path, &d).unwrap();
    let c = comdat::text_of(&d).unwrap().remove(0);
    let ((seats, a, r), _) = backstop::run("seat, segments and rank at the last byte", || {
        let seats = seat::seat_map(&at_end, std::slice::from_ref(&path), 1).seats;
        let a = segments::analyze_in(&at_end, last, &path, c, MAX_SEGMENT);
        let r = segrank::rank(&at_end, std::slice::from_ref(&path), &seats, &[last], 1);
        (seats, a, r)
    });
    assert_eq!(seats.keys().copied().collect::<Vec<_>>(), [last]);
    assert!(a.is_ok(), "{a:?}");
    assert_eq!(r.rows.len() + r.errors.len(), 1);
    run.finish();
}

#[test]
fn commutative_swap_naming_on_a_giant_differing_function_is_linear() {
    let body = |swap: bool| {
        let mut b = vec![0x8b, 0x74, 0x24, 0x04];
        for _ in 0..32_000 {
            b.extend_from_slice(&[0x8b, 0x43, 0x04]);
            b.extend_from_slice(if swap { &[0x03, 0xc8] } else { &[0x03, 0xc1] });
            b.extend_from_slice(&[0x8b, 0xd0]);
        }
        b.push(0xc3);
        b
    };
    let img = segments::test_side(&body(false));
    let ours = segments::test_side(&body(true));
    let n = img.dec.len();
    let (segs, _, _) = segments::segment(&img, &ours, usize::MAX);
    let a = segments::Analysis {
        header: String::new(),
        segs,
        start: 0,
        rva: 0,
        img,
        ours,
        comdat: comdat::Comdat::default(),
        obj: PathBuf::new(),
        want: 0,
        whence: "",
    };
    let (swaps, took) = backstop::run("commswap::find on 96001 instructions", || {
        commswap::find(&a).len()
    });
    println!("commswap: {n} instructions, {swaps} swap(s) in {took:?}");
    assert!(n > 96_000 && swaps <= n, "{n} {swaps}");
}
