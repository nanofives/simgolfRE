#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_class::decl::{self, ClassDecl};
use msvc6_class::receiver::{self, Seed};
use msvc6_class::slots::{self, MAX_STEPS, Model, Rule};
use msvc6_class::vft::{self, Run, Sections, Text};
use msvc6_class::{
    callconv, ehcensus, funclets, insns, join, layout, listing, read, sigs, slotmap, sources,
};
use msvc6_fixture::coff::{Object as Obj, SCN_COMDAT, SCN_TEXT, Section};
use msvc6_fixture::pe::{Pe, SCN_CODE, SCN_RDATA};
use msvc6_fixture::{backstop, classes};
use msvc6_image::pe::Image;
use std::collections::BTreeSet;
use std::path::PathBuf;

fn watched<T>(what: &str, f: impl FnOnce() -> T) -> T {
    backstop::run(what, f).0
}

fn junk(n: usize, seed: u32) -> Vec<u8> {
    let mut x = seed.wrapping_mul(2_654_435_761).wrapping_add(1);
    (0..n)
        .map(|_| {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            x.to_le_bytes()[0]
        })
        .collect()
}

fn scratch(tag: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!(
        "msvc6-class-malformed-{tag}-{}",
        std::process::id()
    ));
    std::fs::create_dir_all(&d).unwrap();
    d
}

fn image(text: Vec<u8>, rdata: Vec<u8>) -> Image {
    let mut pe = Pe::dll();
    pe.image_base = classes::BASE;
    pe.push(".text", SCN_CODE, text);
    pe.push(".rdata", SCN_RDATA, rdata);
    Image::parse(pe.build()).unwrap()
}

fn pointers(n: usize, seed: u32, text_len: u32) -> Vec<u8> {
    junk(n * 4, seed)
        .chunks(4)
        .flat_map(|c| {
            let k = u32::from_le_bytes([c[0], c[1], c[2], c[3]]) % text_len.saturating_add(8);
            (classes::BASE + classes::TEXT + k).to_le_bytes()
        })
        .collect()
}

#[test]
fn a_fifo_a_device_and_a_directory_are_refused_before_a_byte_is_read() {
    let d = scratch("fifo");
    let fifo = d.join("pipe.h");
    rustix::fs::mknodat(
        rustix::fs::CWD,
        &fifo,
        rustix::fs::FileType::Fifo,
        rustix::fs::Mode::from_raw_mode(0o600),
        0,
    )
    .unwrap();
    let plain = d.join("plain.h");
    msvc6_base::srcio::write_bytes(&plain, b"class A { virtual void f(); };\n").unwrap();
    for (what, p) in [
        ("fifo", fifo.clone()),
        ("device", PathBuf::from("/dev/zero")),
        ("directory", d.clone()),
    ] {
        let e = watched(what, || read::bytes(&p, read::MAX_SOURCE)).unwrap_err();
        assert!(
            format!("{e:#}").contains("not a regular file"),
            "{what}: {e:#}"
        );
        assert!(watched(what, || read::text(&p, read::MAX_TEXT)).is_err());
        let e = watched(&format!("{what}: open"), || {
            read::open_regular(&p, read::MAX_SOURCE)
        })
        .unwrap_err();
        assert!(
            format!("{e:#}").contains("not a regular file"),
            "{what}: {e:#}"
        );
    }
    assert!(read::open_regular(&plain, read::MAX_SOURCE).is_ok_and(|(_, n)| n == 31));
    assert_eq!(read::files(&d, &["h"]).unwrap(), vec![plain.clone()]);
    let all = watched("read_all", || {
        sources::read_all(std::slice::from_ref(&d), &["h"])
    })
    .unwrap();
    assert_eq!(all.len(), 1);
    assert_eq!(all[0].path, plain);
    std::fs::remove_dir_all(&d).unwrap();
}

#[test]
fn junk_images_yield_runs_tables_and_slot_maps_bounded_by_their_rdata() {
    let mut cases: Vec<(String, Vec<u8>, Vec<u8>)> = Vec::new();
    for s in 0..6u32 {
        let text = junk(1 << 14, s);
        let len = u32::try_from(text.len()).unwrap();
        cases.push((
            format!("pointers {s}"),
            text.clone(),
            pointers(1 << 12, s, len),
        ));
        cases.push((format!("junk {s}"), text, junk(1 << 14, s + 100)));
    }
    let hops: Vec<u8> = [0xe9, 0xfb, 0xff, 0xff, 0xff].repeat(1 << 12);
    let hop_len = u32::try_from(hops.len()).unwrap();
    cases.push(("self jumps".into(), hops, pointers(1 << 12, 7, hop_len)));
    let edge: Vec<u8> = [
        classes::TEXT - 1,
        classes::TEXT,
        classes::TEXT + 0x3fff,
        u32::MAX,
    ]
    .iter()
    .flat_map(|o| classes::BASE.wrapping_add(*o).to_le_bytes())
    .collect();
    cases.push(("edges".into(), vec![0xe9; 0x4000], edge));
    for (what, text, rdata) in cases {
        let dwords = rdata.len() / 4;
        let img = image(text.clone(), rdata);
        let (runs, tables, rows) = watched(&what, || {
            let t = Text::of(&img, vft::CODE).unwrap();
            let runs = vft::detect(&img, Sections::default()).unwrap();
            let followed: Vec<Run> = runs
                .iter()
                .map(|r| Run {
                    rva: r.rva,
                    slots: r.slots.iter().map(|s| t.follow(*s)).collect(),
                })
                .collect();
            let extents: Vec<(u32, Option<u32>)> =
                (0..64).map(|k| (classes::TEXT + k * 0x100, None)).collect();
            let funcs = insns::decode(&text, classes::TEXT, &extents);
            let decoded: usize = funcs.iter().map(|f| f.insns.len()).sum();
            assert!(
                decoded <= 2 * text.len() + 1,
                "{what}: {decoded} instructions"
            );
            let stores = vft::stores(&funcs, classes::BASE, &followed);
            let tables = vft::split(&followed, &stores);
            let census = vft::census(&followed, &followed, vft::MIN_COMPARED);
            let _ = vft::census_report(&census, "ours", vft::MIN_COMPARED);
            let mut rows = 0usize;
            for r in &followed {
                let m = slotmap::map(t, &r.slots, None);
                assert!(m.rows.len() <= r.slots.len());
                rows += m.rows.len();
                let _ = slotmap::report(&m, "x", r.rva, r.slots.len(), None);
            }
            (
                runs.iter().map(|r| r.slots.len()).sum::<usize>(),
                tables.iter().map(|t| t.slots.len()).sum::<usize>(),
                rows,
            )
        });
        assert!(runs <= dwords, "{what}: {runs} slots from {dwords} dwords");
        assert!(tables <= runs, "{what}: {tables} table slots from {runs}");
        assert!(rows <= runs, "{what}");
    }
}

#[test]
fn junk_bodies_read_their_receiver_and_convention_within_their_bytes() {
    let pop = receiver::population(&[(
        "a.h".to_string(),
        "class A : public A {\n    int x; /* +0x4 */\n};\nclass B : public A, virtual public B { int y; };\n"
            .to_string(),
    )]);
    for s in 0..16u32 {
        let body = junk(1 << 12, s);
        let (t, r) = watched(&format!("body {s}"), || {
            let _ = callconv::this_of(&body);
            let a = receiver::touched(&body, 0x40_1000, Seed::Ecx);
            let b = receiver::touched(&body, 0x40_1000, Seed::Stack);
            let r = receiver::rank(&body, 0x1000, None, &pop);
            let _ = receiver::report(&r, "x", 0x1000, 0x1000);
            (a.offsets.len() + b.offsets.len(), r.candidates.len())
        });
        assert!(t <= 2 * body.len(), "body {s}: {t} offsets");
        assert!(r <= pop.classes().len());
    }
    let mut seen = BTreeSet::new();
    let flat = watched("cyclic flatten", || receiver::flatten("B", &pop, &mut seen));
    assert!(seen.len() <= receiver::MAX_BASE_DEPTH);
    let _ = flat;
}

#[test]
fn ghidra_records_that_lie_are_refused_or_read_within_their_text() {
    let deep = "[".repeat(1 << 16);
    let wide = format!(
        "[{}]",
        vec!["{\"addr\":\"0x1000\",\"ins\":[[\"0x1000\",\"m\",\"zz\"]]}"; 1 << 12].join(",")
    );
    let huge_ins = format!(
        "[{{\"addr\":\"0x10001000\",\"ins\":[{}]}}]",
        vec!["[\"0x10001000\",\"nop\",\"90\"]"; 1 << 16].join(",")
    );
    let below = "[{\"addr\":\"0x10\",\"ins\":[[\"0x10\",\"nop\",\"90\"]]}]".to_string();
    let format = "{\"format\":2,\"functions\":[]}".to_string();
    for (what, text) in [
        ("deep", deep),
        ("wide", wide),
        ("huge", huge_ins.clone()),
        ("below the base", below),
        ("format", format),
        ("junk", msvc6_base::srcio::tool_text(&junk(1 << 16, 3))),
    ] {
        let len = text.len();
        let r = watched(what, || insns::from_ghidra(&text, classes::BASE));
        if let Ok(f) = r {
            let n: usize = f.iter().map(|x| x.insns.len()).sum();
            assert!(n <= len, "{what}: {n} instructions from {len} bytes");
        }
    }
    let f = insns::from_ghidra(&huge_ins, classes::BASE).unwrap();
    assert_eq!(f[0].insns.len(), 1 << 16);
    let text = [0x75u8, 0xfe].repeat(1 << 12);
    let overlapping: Vec<(u32, Option<u32>)> = (0..1u32 << 10)
        .map(|k| (0x1000 + k, Some(u32::MAX)))
        .collect();
    let funcs = watched("overlapping extents", || {
        insns::decode(&text, 0x1000, &overlapping)
    });
    let n: usize = funcs.iter().map(|f| f.insns.len()).sum();
    assert!(n <= 2 * text.len() + 1, "{n} instructions");
}

fn chain(n: usize) -> String {
    let mut s = String::from("class C0 { public: virtual void f0(); virtual ~C0(); };\n");
    for k in 1..n {
        s.push_str(&format!(
            "class C{k} : public C{} {{ public: virtual void f{k}(); }};\n",
            k - 1
        ));
    }
    s
}

fn fan(width: usize) -> String {
    let mut s = String::new();
    for i in 0..width {
        for j in 0..width {
            s.push_str(&format!(
                "class A{i}x{j} {{ public: virtual void a{j}(); }};\n"
            ));
        }
        let bases: Vec<String> = (0..width).map(|j| format!("public A{i}x{j}")).collect();
        s.push_str(&format!(
            "class B{i} : {} {{ public: virtual void b{i}(); }};\n",
            bases.join(", ")
        ));
    }
    let bases: Vec<String> = (0..width).map(|i| format!("public B{i}")).collect();
    s.push_str(&format!(
        "class T : {} {{ public: virtual void t(); }};\n",
        bases.join(", ")
    ));
    s
}

fn lattice(levels: usize, width: usize) -> String {
    let mut s = String::new();
    for w in 0..width {
        s.push_str(&format!(
            "class L0x{w} {{ public: virtual void g{w}(); virtual void h(int); }};\n"
        ));
    }
    for l in 1..levels {
        for w in 0..width {
            let bases: Vec<String> = (0..width)
                .map(|b| format!("public L{}x{b}", l - 1))
                .collect();
            s.push_str(&format!(
                "class L{l}x{w} : {} {{ public: virtual void g{w}(); void h(int); }};\n",
                bases.join(", ")
            ));
        }
    }
    s
}

#[test]
fn hostile_class_sources_parse_in_work_bounded_by_their_text() {
    let cases: Vec<(&str, String)> = vec![
        ("unclosed nesting", "class A {\n".repeat(1 << 17)),
        (
            "closed nesting",
            format!(
                "{}{}",
                "class A {\n".repeat(1 << 12),
                "};\n".repeat(1 << 12)
            ),
        ),
        ("heads without a body", "class A\n".repeat(1 << 17)),
        (
            "heads sharing one brace",
            format!(
                "{}{{ virtual void f(); }};\n",
                "class A :\n".repeat(1 << 15)
            ),
        ),
        (
            "many bases",
            format!(
                "class Z : {} {{ virtual void f(); }};\n",
                (0..4000)
                    .map(|k| format!("public B{k}"))
                    .collect::<Vec<_>>()
                    .join(", ")
            ),
        ),
        (
            "a fat class",
            format!(
                "class F {{\n{}}};\n",
                (0..1 << 13)
                    .map(|k| format!("    virtual void m(int a{k});\n    virtual void m{k}();\n"))
                    .collect::<String>()
            ),
        ),
        (
            "macro bombs",
            format!(
                "#define V(x) virtual void x##a(); V(x##b) V(x##c)\n#define W(x) V(x) V(x)\n{}",
                "class M { W(q) W(r) W(s) };\n".repeat(1 << 10)
            ),
        ),
        ("junk", msvc6_base::srcio::tool_text(&junk(1 << 18, 9))),
        ("braces", "{}{{}}}{".repeat(1 << 16)),
        ("crlf chain", chain(64).replace('\n', "\r\n")),
    ];
    for (what, src) in cases {
        let lines = src.lines().count().max(1);
        let files = vec![(format!("{what}.h"), src)];
        let pop = watched(what, || join::population(&files, true));
        assert!(
            pop.len() <= lines,
            "{what}: {} classes from {lines} lines",
            pop.len()
        );
        assert!(pop.iter().all(|c| c.bases.len() <= decl::MAX_BASES));
        watched(&format!("{what}: slots"), || {
            let model = Model::new(&pop);
            for c in pop.iter().take(64) {
                for rule in [Rule::Vc6, Rule::Legacy] {
                    let _ = model.slots(c, rule);
                    assert!(model.steps_taken() <= MAX_STEPS);
                    let _ = model.implied(c, rule);
                    let _ = model.flat_layout(c, rule);
                }
            }
        });
        let _ = watched(&format!("{what}: rvas"), || {
            decl::declaration_rvas(&files[0].1)
        });
        let _ = watched(&format!("{what}: sigs"), || sigs::screen(&files, false));
        let _ = watched(&format!("{what}: layout"), || {
            layout::screen(&files, &BTreeSet::new(), &["?".to_string()])
        });
        let _ = watched(&format!("{what}: receiver"), || {
            receiver::population(&files)
        });
    }
}

#[test]
fn cyclic_deep_and_lattice_hierarchies_resolve_or_decline_within_the_step_budget() {
    let cyc = decl::class_decls(
        "class A : public B { virtual void f(); };\nclass B : public A { virtual void g(); };\n",
        "c.h",
    );
    let m = Model::new(&cyc);
    for c in &cyc {
        assert_eq!(m.slots(c, Rule::Vc6), None);
        assert_eq!(m.slots(c, Rule::Legacy), None);
        assert_eq!(
            m.implied(c, Rule::Legacy),
            slots::Implied::UnknownBase(c.bases[0].clone())
        );
        assert_eq!(
            m.implied(c, Rule::Vc6),
            slots::Implied::UnknownBase(c.bases[0].clone())
        );
    }
    let deep = decl::class_decls(&chain(64), "d.h");
    let m = Model::new(&deep);
    let resolved: Vec<usize> = deep
        .iter()
        .filter_map(|c| m.slots(c, Rule::Vc6).map(|s| s.len()))
        .collect();
    assert_eq!(resolved, (2..=decl::MAX_DEPTH + 2).collect::<Vec<_>>());
    let wide = decl::class_decls(&lattice(10, 24), "l.h");
    assert_eq!(wide.len(), 240);
    let m = Model::new(&wide);
    let (steps, tables) = watched("lattice", || {
        let mut steps = 0usize;
        let mut tables = 0usize;
        for c in &wide {
            for rule in [Rule::Vc6, Rule::Legacy] {
                tables += usize::from(m.slots(c, rule).is_some());
                steps = steps.max(m.steps_taken());
                let _ = m.implied(c, rule);
                let _ = m.flat_layout(c, rule);
            }
        }
        (steps, tables)
    });
    assert!(steps <= MAX_STEPS);
    assert!(tables <= 2 * wide.len());
    let files = vec![("l.h".to_string(), lattice(10, 24))];
    let names: Vec<(String, u32)> = Vec::new();
    let runs = vec![Run {
        rva: 0x2000,
        slots: (0..64).map(|k| 0x1000 + 0x10 * k).collect(),
    }];
    let j = watched("lattice join", || {
        join::join(&join::Inputs {
            runs: &runs,
            names: &names,
            map: &[],
            placed: &[],
            files: &files,
            headers: &[],
            rule: Rule::Vc6,
            text: None,
        })
    });
    assert!(j.population <= wide.len());
}

#[test]
fn listings_and_funclet_regions_of_junk_cut_into_bounded_pieces() {
    let mut texts: Vec<String> = (0..4)
        .map(|s| msvc6_base::srcio::tool_text(&junk(1 << 16, s)))
        .collect();
    texts.push("_f PROC NEAR\n".repeat(1 << 17));
    texts.push("_f PROC NEAR\n  00000 c3 ret 0\n_f ENDP\n".repeat(1 << 15));
    texts.push(format!(
        "_f PROC NEAR\n{}",
        "  00000 90 nop\n".repeat(1 << 17)
    ));
    for (k, t) in texts.iter().enumerate() {
        let lines = t.lines().count();
        let f = watched(&format!("listing {k}"), || listing::functions(t));
        assert!(f.len() <= lines.min(listing::MAX_FUNCTIONS));
        let _ = watched(&format!("listing {k} pick"), || listing::function(t, None));
    }
    let stub = [0xb8u8, 0, 0, 0, 0, 0xe9, 0, 0, 0, 0];
    let mut regions: Vec<Vec<u8>> = (0..4).map(|s| junk(1 << 16, s + 40)).collect();
    regions.push(vec![0xcc; 1 << 16]);
    regions.push(stub.repeat(1 << 13));
    regions.push([0xc3u8].repeat(1 << 16));
    for (k, r) in regions.iter().enumerate() {
        let g = watched(&format!("region {k}"), || {
            funclets::cut(r, 0x1000, 0x40_0000)
        });
        let covered: u64 = g.iter().map(|x| u64::from(x.len)).sum();
        assert!(covered <= r.len() as u64, "region {k}");
        assert!(g.len() <= funclets::MAX_GROUPS);
        let stubs: Vec<u32> = g.iter().map(|x| x.stub).collect();
        let p = watched(&format!("region {k} pushers"), || {
            funclets::pushers(r, 0x1000, 0x40_0000, &stubs)
        });
        assert!(p.len() <= stubs.len());
    }
    let map = format!(
        "{}{}",
        " 0001:00000000 0000ffffH .text$x CODE\n".repeat(1 << 12),
        " 0001:zzzz _f ffffffff f a.obj\n".repeat(1 << 12)
    );
    let (names, x) = watched("map", || funclets::read_map(&map, 0x40_0000));
    assert!(names.len() <= 1 << 13);
    assert_eq!(x, Some((0, 0xffff)));
}

#[test]
fn objects_and_archives_that_lie_are_refused_or_counted_within_their_bytes() {
    let mut o = Obj::new();
    for k in 0..32u8 {
        let t = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: vec![0xc3; 16],
            ..Section::default()
        });
        o.section_symbol(t, 2, 0);
        o.external(&format!("_f{k}"), 0, t);
        let x = o.section(Section {
            name: ".text$x".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: vec![0xcc; 16 + usize::from(k)],
            ..Section::default()
        });
        o.section_symbol(x, 5, u16::try_from(t).unwrap());
    }
    let good = o.build();
    let total = good.len();
    let mut cases: Vec<(String, Vec<u8>)> = vec![("fixture".into(), good.clone())];
    for cut in (0..total).step_by(7) {
        cases.push((format!("truncated at {cut}"), good[..cut].to_vec()));
    }
    for s in 0..8 {
        cases.push((format!("junk {s}"), junk(1 << 12, s)));
        let mut flipped = good.clone();
        for (i, b) in junk(total, s + 9).iter().enumerate() {
            if b % 13 == 0 {
                flipped[i] ^= *b;
            }
        }
        cases.push((format!("flipped {s}"), flipped));
    }
    for (what, data) in &cases {
        let r = watched(what, || ehcensus::object("x", data));
        if let Ok(rows) = r {
            assert!(rows.len() <= 1 << 16, "{what}");
        }
        let a = watched(what, || ehcensus::archive(data, &[]));
        if let Ok((walked, carried)) = a {
            assert!(carried.len() <= walked, "{what}");
        }
    }
    let rows = ehcensus::object("x", &good).unwrap();
    assert_eq!(rows.len(), 32);
    assert!(rows.iter().all(|r| !r.owner.is_empty()));
}

#[test]
fn macro_expansion_stops_at_its_budget_and_never_reexpands_itself() {
    let macros = decl::virtual_macros(&[(
        "m.h".to_string(),
        "#define SELF(x) virtual void x(); SELF(x)\n#define BIG(x) virtual int x##_(); BIG(x) BIG(x) BIG(x)\n".to_string(),
    )]);
    assert_eq!(macros.len(), 2);
    let src = "class A { SELF(a) BIG(b) };\n".repeat(1 << 14);
    let out = watched("expand", || decl::expand(&src, &macros));
    assert!(
        out.len() <= src.len() + decl::MAX_EXPAND_WORK,
        "{} bytes",
        out.len()
    );
    assert!(out.lines().count() == src.lines().count());
    let one = decl::expand("SELF(z)\n", &macros);
    assert_eq!(one.matches("SELF(").count(), 1, "{one}");
}

#[test]
fn class_decls_of_a_flat_file_read_every_class_and_stop_only_past_the_scan_budget() {
    let flat: String = (0..1 << 12)
        .map(|k| format!("class K{k} {{ virtual void f(); int a; }};\n"))
        .collect();
    let d: Vec<ClassDecl> = watched("flat", || decl::class_decls(&flat, "f.h"));
    assert_eq!(d.len(), 1 << 12);
    let nested = format!(
        "{}{}",
        "class N {\n".repeat(1 << 14),
        "};\n".repeat(1 << 14)
    );
    let n = watched("nested", || decl::class_decls(&nested, "n.h"));
    assert!(
        !n.is_empty() && n.len() <= decl::SCAN_FACTOR + 1,
        "{}",
        n.len()
    );
}

#[test]
fn every_budget_that_stops_a_screen_says_so() {
    let nested = format!(
        "{}{}",
        "class N {\n".repeat(1 << 10),
        "};\n".repeat(1 << 10)
    );
    let p = watched("nested population", || {
        join::population_noted(&[("n.h".to_string(), nested)], true)
    });
    assert!(
        p.notes.len() == 1
            && p.notes[0].starts_with("NOTE: n.h: ")
            && p.notes[0].contains("not read"),
        "{:?}",
        p.notes
    );
    let clean = join::population_noted(
        &[(
            "c.h".to_string(),
            "class C { virtual void f(); };\n".to_string(),
        )],
        true,
    );
    assert_eq!((clean.decls.len(), clean.notes.len()), (1, 0));
    let uses = format!(
        "#define V(x) virtual void x();\nclass A {{\n{}}};\n",
        "V(a)\n".repeat(decl::MAX_EXPANSIONS + 1)
    );
    let p = watched("expansion population", || {
        join::population_noted(&[("v.h".to_string(), uses)], true)
    });
    assert!(
        p.notes
            .iter()
            .any(|n| n.contains("macro expansion stopped at its budget")),
        "{:?}",
        p.notes
    );
    let wide = decl::class_decls(&fan(256), "f.h");
    assert_eq!(wide.len(), 256 * 256 + 257);
    let m = Model::new(&wide);
    let top = wide.iter().find(|c| c.name == "T").unwrap();
    let got = watched("a query past its steps", || {
        (
            m.slots(top, Rule::Vc6).is_none(),
            m.implied(top, Rule::Legacy),
        )
    });
    let spent = m.spent_queries();
    assert!(got.0, "{got:?}");
    assert!(
        spent > 0
            && m.note()
                .is_some_and(|n| n.contains(&format!("{spent} slot quer"))),
        "{spent} {:?}",
        m.note()
    );
    assert!(Model::new(&clean.decls).note().is_none());
    let text = vec![0x90u8; 4096];
    let starts: Vec<(u32, Option<u32>)> = (0..4096u32)
        .map(|k| (0x1000 + k, Some(0x1000 + 4096)))
        .collect();
    let (funcs, starved) = watched("overlapping bodies", || {
        insns::decode_within(&text, 0x1000, &starts)
    });
    assert!(starved && funcs.len() == 4096, "{starved} {}", funcs.len());
    let (_, fed) = insns::decode_within(&text, 0x1000, &[(0x1000, None)]);
    assert!(!fed);
}
