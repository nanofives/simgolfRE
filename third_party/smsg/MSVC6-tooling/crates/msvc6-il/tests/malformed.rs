#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::il::{self as fx, Body, Set};
use msvc6_il::grammar::{Grammar, Pragma};
use msvc6_il::inline::{MAX_STEPS, Planner};
use msvc6_il::unit::{FFLAG_CANDIDATE, FFLAG_COMPILE, FFLAG_DEFINED, Files, Unit};
use std::panic::{AssertUnwindSafe, catch_unwind};

struct Run {
    fails: Vec<String>,
    cases: usize,
}

impl Run {
    fn new() -> Self {
        Self {
            fails: Vec::new(),
            cases: 0,
        }
    }

    fn case<T>(&mut self, what: &str, f: impl FnOnce() -> T) -> Option<T> {
        self.cases += 1;
        match msvc6_fixture::backstop::run(what, || catch_unwind(AssertUnwindSafe(f))).0 {
            Ok(v) => Some(v),
            Err(_) => {
                self.fails.push(format!("{what}: panicked"));
                None
            }
        }
    }

    fn check(&mut self, what: &str, ok: bool, got: &dyn std::fmt::Debug) {
        if !ok {
            self.fails.push(format!("{what}: {got:?}"));
        }
    }

    fn finish(self, tag: &str) {
        assert!(self.cases > 0, "{tag}: no case ran");
        assert!(
            self.fails.is_empty(),
            "{tag}: {} of {} case(s) failed:\n{}",
            self.fails.len(),
            self.cases,
            self.fails.join("\n")
        );
    }
}

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }

    fn bytes(&mut self, n: usize) -> Vec<u8> {
        (0..n).map(|_| self.next().to_le_bytes()[0]).collect()
    }
}

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

fn files(gl: Vec<u8>, sy: Vec<u8>, ex: Vec<u8>, init: Vec<u8>) -> Files {
    Files {
        base: "bad.".into(),
        gl,
        sy,
        ex,
        init,
        pch: None,
        db: false,
    }
}

const CODE: u32 = FFLAG_DEFINED | FFLAG_COMPILE;
const INLINE: u32 = FFLAG_DEFINED | FFLAG_CANDIDATE;

fn every_reader(f: &Files, g: &Grammar) -> (bool, bool, bool, usize) {
    let dump = msvc6_il::dump::dump(f, g, &msvc6_il::dump::Opts::default()).is_ok();
    let ids = msvc6_il::ids::load(f, g).is_ok();
    let unit = Unit::read(f);
    let mut walked = 0usize;
    if let Ok(u) = &unit {
        let mut p = Planner::new(u, f, g);
        for r in p.roots() {
            walked += usize::from(p.walk(r).is_some());
        }
    }
    (dump, ids, unit.is_ok(), walked)
}

fn valid() -> Set {
    let mut s = Set::new(0x300);
    let mut leaf = Body::default();
    leaf.open(0x100, &[]).close(0x1100);
    s.define(fx::def(0x100, "?g@@YAXXZ", 12, INLINE), &leaf);
    let mut b = Body::default();
    b.open(0x101, &[])
        .call_void(0x100)
        .local_object(0x100, 0x100, 0x2f0)
        .leave(1, 1)
        .unwind(1, 0xa1)
        .call_with_call(0x100, 0x100)
        .close(0x1101);
    s.define(fx::def(0x101, "?f@@YAXXZ", 40, CODE | 0x100), &b);
    s
}

#[test]
fn random_bytes_are_refused_or_read_within_their_bytes() {
    let mut run = Run::new();
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    let g = grammar();
    for n in [0usize, 1, 7, 21, 64, 4096, 1 << 20] {
        let b = rng.bytes(n);
        let what = format!("{n} random bytes in every stream");
        let gr = g.clone();
        if let Some(r) = run.case(&what, move || {
            let f = files(b.clone(), b.clone(), b.clone(), b.clone());
            let all = every_reader(&f, &gr);
            let mut c = msvc6_il::cursor::Cursor::new(&b, 0);
            let mut recs = 0usize;
            while msvc6_il::ex::read(&mut c, &gr, false).is_ok() && !c.at_end() {
                recs += 1;
            }
            let procs = msvc6_il::listing::procs(&msvc6_base::srcio::tool_text(&b)).len();
            (all, recs, procs)
        }) {
            run.check(&what, r.1 <= n && r.2 <= n, &r);
        }
    }
    run.finish("random");
}

#[test]
fn every_cut_of_a_valid_set_is_refused_or_read() {
    let mut run = Run::new();
    let s = valid();
    let (gl, sy, ex, init) = (s.gl(), s.sy.0.clone(), s.ex_bytes(), s.in_bytes());
    let g = grammar();
    let whole = files(gl.clone(), sy.clone(), ex.clone(), init.clone());
    let (dump, ids, unit, walked) = every_reader(&whole, &g);
    assert!(dump && ids && unit && walked == 1, "the valid set reads");
    for (k, len) in [
        (0usize, gl.len()),
        (1, sy.len()),
        (2, ex.len()),
        (3, init.len()),
    ] {
        let what = format!("stream {k} cut at every length up to {len}");
        let (gl, sy, ex, init, gr) = (gl.clone(), sy.clone(), ex.clone(), init.clone(), g.clone());
        if let Some(r) = run.case(&what, move || {
            let mut ok = 0usize;
            for cut in 0..len {
                let pick = |i: usize, v: &[u8]| {
                    if i == k {
                        v[..cut].to_vec()
                    } else {
                        v.to_vec()
                    }
                };
                let f = files(pick(0, &gl), pick(1, &sy), pick(2, &ex), pick(3, &init));
                let (d, _, _, _) = every_reader(&f, &gr);
                ok += usize::from(d);
            }
            ok
        }) {
            run.check(&what, r <= len, &r);
        }
    }
    run.finish("cuts");
}

#[test]
fn hostile_sets_stop_at_their_bounds() {
    let mut run = Run::new();
    let g = grammar();

    let gr = g.clone();
    if let Some(r) = run.case("a chain of 200000 unclosed calls", move || {
        let mut s = Set::new(0x300);
        let mut b = Body::default();
        b.open(0x101, &[]);
        for _ in 0..200_000 {
            b.0.byte(fx::OP_SYM)
                .id(0x100)
                .byte(fx::OP_CALL)
                .ty(fx::TY_VOID)
                .byte(0);
        }
        b.close(0x1101);
        s.define(fx::def(0x101, "?f@@YAXXZ", 10, CODE), &b);
        let mut leaf = Body::default();
        leaf.open(0x100, &[]).close(0x1100);
        s.define(fx::def(0x100, "?g@@YAXXZ", 10, INLINE), &leaf);
        let f = files(s.gl(), s.sy.0.clone(), s.ex_bytes(), s.in_bytes());
        let text = msvc6_il::dump::dump(&f, &gr, &msvc6_il::dump::Opts::default())
            .map(|t| t.len())
            .unwrap_or(0);
        let u = Unit::read(&f).unwrap();
        let mut p = Planner::new(&u, &f, &gr);
        let sites = p.walk(0x101).map_or(0, |w| w.sites.len());
        (text, sites, p.steps())
    }) {
        run.check(
            "nesting is bounded and every call is one site at most",
            r.1 <= 200_000 && r.2 <= MAX_STEPS,
            &r,
        );
    }

    let gr = g.clone();
    if let Some(r) = run.case(
        "free candidates calling each other twice, 24 deep",
        move || {
            let mut s = Set::new(0x300);
            for k in 0..24u32 {
                let mut b = Body::default();
                b.open(0x100 + k, &[]);
                if k < 23 {
                    b.call_void(0x101 + k).call_void(0x101 + k);
                }
                b.close(0x1100 + k);
                let fl = if k == 0 { CODE } else { INLINE };
                s.define(fx::def(0x100 + k, &format!("?f{k}@@YAXXZ"), 1, fl), &b);
            }
            let f = files(s.gl(), s.sy.0.clone(), s.ex_bytes(), s.in_bytes());
            let u = Unit::read(&f).unwrap();
            let mut p = Planner::new(&u, &f, &gr);
            let _ = p.walk(0x100);
            (p.steps(), p.out_of_steps)
        },
    ) {
        run.check("the fan-out stops at the step budget", r.0 <= MAX_STEPS, &r);
    }

    let gr = g.clone();
    if let Some(r) = run.case("a next-id far past the ids", move || {
        let mut s = valid();
        s.next_id = u32::MAX;
        let f = files(s.gl(), s.sy.0.clone(), s.ex_bytes(), s.in_bytes());
        msvc6_il::ids::load(&f, &gr).is_err()
    }) {
        run.check("the id span is refused", r, &r);
    }

    let gr = g.clone();
    if let Some(r) = run.case("definition ids far above next-id", move || {
        let mut s = Set::new(0x300);
        for id in [0x100u32, 0x7fff_0000] {
            let mut b = Body::default();
            b.open(id, &[]).close(0x1100);
            s.define(fx::def(id, &format!("?f{id:x}@@YAXXZ"), 1, CODE), &b);
        }
        let f = files(s.gl(), s.sy.0.clone(), s.ex_bytes(), s.in_bytes());
        let u = msvc6_il::ids::load(&f, &gr).unwrap();
        let funcs = msvc6_il::ids::funcs_text(&u);
        (
            funcs.lines().count(),
            msvc6_il::ids::ids_text(&u).lines().count(),
        )
    }) {
        run.check(
            "each listing is a line a function or id",
            r.0 <= 8 && r.1 <= 0x300,
            &r,
        );
    }

    let gr = g.clone();
    if let Some(r) = run.case(
        "2000 definitions whose starts alternate low and high",
        move || {
            let mut s = Set::new(0x40_0001);
            for k in 0..2000u32 {
                let id = if k % 2 == 0 { 0x100 + k } else { 0x40_0000 - k };
                let mut b = Body::default();
                b.open(id, &[]).close(0x1100);
                s.define(fx::def(id, &format!("?f{k}@@YAXXZ"), 1, CODE), &b);
            }
            let f = files(s.gl(), s.sy.0.clone(), s.ex_bytes(), s.in_bytes());
            let u = msvc6_il::ids::load(&f, &gr).unwrap();
            msvc6_il::ids::funcs_text(&u)
        },
    ) {
        run.check(
            "the span breakdowns stop at their budget and say so",
            r.lines().count() <= 2003 && r.contains("NOTE: "),
            &r.lines().last(),
        );
    }

    if let Some(r) = run.case("an unterminated warning list", || {
        let mut g = grammar();
        g.pragma[0x23].codes = Some(vec![0x17]);
        let mut b = vec![fx::OP_PRAGMA, 0x23];
        for _ in 0..100_000 {
            b.extend_from_slice(&[0x01, 0x0e]);
        }
        msvc6_il::ex::read(&mut msvc6_il::cursor::Cursor::new(&b, 0), &g, false).is_err()
    }) {
        run.check("the list is refused at its bound", r, &r);
    }

    if let Some(r) = run.case("a long expression chain", || {
        let gr = grammar();
        let mut b = vec![fx::OP_CONST, 0x41, 0x01];
        b.extend(std::iter::repeat_n(0x08u8, 50_000));
        let mut g2 = gr.clone();
        g2.fmt[0x08] = 0;
        let mut t = msvc6_il::tree::Trees::new();
        let mut c = msvc6_il::cursor::Cursor::new(&b, 0);
        let mut n = 0usize;
        while let Ok(r) = msvc6_il::ex::read(&mut c, &g2, false) {
            let _ = t.step(&g2, &NoNames, &r);
            n += 1;
            if c.at_end() {
                break;
            }
        }
        n
    }) {
        run.check("every record is read once", r == 50_001, &r);
    }

    run.finish("hostile");
}

struct NoNames;

impl msvc6_il::tree::Names for NoNames {
    fn short(&self, id: u32) -> String {
        format!("#{id:x}")
    }

    fn full(&self, id: u32) -> String {
        format!("#{id:x}")
    }
}

#[test]
fn a_device_or_a_directory_is_no_il_file() {
    let mut run = Run::new();
    if let Some(r) = run.case("/dev/zero and / as IL members", || {
        (
            msvc6_il::file::read(std::path::Path::new("/dev/zero"), 1 << 20).is_err(),
            msvc6_il::unit::Files::open(std::path::Path::new("/dev/zer"), None).is_err(),
            msvc6_il::grammar::Grammar::load(std::path::Path::new("/")).is_err(),
        )
    }) {
        run.check("each is refused", r.0 && r.1 && r.2, &r);
    }
    let d = std::env::temp_dir().join(format!("msvc6-il-fifo-{}", std::process::id()));
    std::fs::create_dir_all(&d).unwrap();
    let fifo = d.join("x.gl");
    let made = std::process::Command::new("mkfifo")
        .arg(&fifo)
        .status()
        .is_ok_and(|s| s.success());
    if made
        && let Some(r) = run.case("a FIFO as an IL member", || {
            msvc6_il::file::read(&fifo, 1 << 20).is_err()
        })
    {
        run.check("a FIFO is refused before it is opened", r, &r);
    }
    std::fs::remove_dir_all(&d).ok();
    run.finish("devices");
}

fn writer_scan_image(attr: &[u16], entries: usize) -> Vec<u8> {
    let mut pe = msvc6_fixture::pe::Pe::dll();
    pe.push(".text", 0x6000_0020, vec![0xc3; 16]);
    let base = pe.image_base.saturating_add(pe.next_rva());
    let mut d = Vec::new();
    for a in attr {
        d.extend_from_slice(&(a & 0x7fff).to_le_bytes());
        d.extend_from_slice(&0u16.to_le_bytes());
        d.extend_from_slice(&0u32.to_le_bytes());
    }
    d.extend_from_slice(&0x0002_0018u32.to_le_bytes());
    d.extend_from_slice(&0x0071_0017u32.to_le_bytes());
    let codes = base + u32::try_from(d.len()).unwrap();
    d.extend(std::iter::repeat_n(b'A', 63));
    d.extend_from_slice(&[0; 9]);
    for _ in 0..entries {
        d.extend_from_slice(&codes.to_le_bytes());
        d.extend_from_slice(&0u32.to_le_bytes());
    }
    pe.push(".rdata", 0x4000_0040, d);
    pe.build()
}

#[test]
fn the_grammar_scans_read_hostile_images_within_their_bytes() {
    let mut run = Run::new();
    let g = msvc6_il::grammar::locate(
        &msvc6_image::pe::image::Image::parse(fx::grammar_image(
            &fx::fmt_table(0xbc),
            &fx::attr_table(0xbe),
            &fx::pragma_codes(),
        ))
        .unwrap(),
    )
    .unwrap();
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    for n in [0usize, 64, 4096, 1 << 22] {
        let mut pe = msvc6_fixture::pe::Pe::dll();
        pe.push(".text", 0x6000_0020, vec![0xc3; 16]);
        pe.push(".rdata", 0x4000_0040, rng.bytes(n));
        pe.push(".data", 0xc000_0040, vec![0; n]);
        let img = msvc6_image::pe::image::Image::parse(pe.build()).unwrap();
        let gr = g.clone();
        if let Some(r) = run.case(&format!("{n} random and {n} zero data bytes"), move || {
            (
                msvc6_il::grammar::locate(&img).is_err(),
                msvc6_il::grammar::writer(&img, &gr, &msvc6_il::sym::WRITER_ANCHORS).is_err(),
            )
        }) {
            run.check("locate finds no grammar in them", r.0, &r);
        }
    }
    let entries = 1usize << 19;
    let img = msvc6_image::pe::image::Image::parse(writer_scan_image(&g.attr, entries)).unwrap();
    let gr = g.clone();
    if let Some(r) = run.case(
        "a writer image of 2^19 symbol entries that each name 63 codes",
        move || {
            let w = msvc6_il::grammar::writer(&img, &gr, &msvc6_il::sym::WRITER_ANCHORS).unwrap();
            (w.ops.len(), w.syms_at, w.sym_reads)
        },
    ) {
        run.check(
            "the op table is read, no symbol table agrees, and each entry is read once",
            r.0 == 0xbe && r.1.is_none() && r.2 <= 2 * entries + 0x1000,
            &r,
        );
    }
    run.finish("grammar");
}
