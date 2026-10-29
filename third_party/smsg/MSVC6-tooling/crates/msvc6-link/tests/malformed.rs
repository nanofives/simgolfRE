#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::archive::{Library, short_import};
use msvc6_fixture::coff::{
    CLASS_EXTERNAL, Object as Obj, SCN_COMDAT, SCN_TEXT, SELECT_ANY, Section,
};
use msvc6_fixture::sample;
use msvc6_image::pe::Image;
use msvc6_link::comdat::object_comdats;
use msvc6_link::icf::{Array, Cand, fold};
use msvc6_link::{iat, icfdiff, ilt, iltdiff, include, joint, linkmap, verbose};

fn watched<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
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

#[test]
fn logs_of_junk_read_in_work_bounded_by_their_lines() {
    let mut texts: Vec<String> = (0..8)
        .map(|s| msvc6_base::srcio::tool_text(&junk(1 << 16, s)))
        .collect();
    texts.push("    Selected symbol:\n".repeat(50_000));
    texts.push("    Replaced symbol(s):\n        x from y\n".repeat(50_000));
    texts.push("        Loaded (((((((((x)))))))))\n      Found \"(\" (\n".repeat(20_000));
    texts.push(format!("Loaded a.lib({})\n", "x".repeat(4 << 20)));
    texts.push(
        "Start Generating .exp file\n      Found _x\n        Loaded k.lib(K.dll)\n".repeat(10_000),
    );
    for (k, t) in texts.into_iter().enumerate() {
        let lines = t.lines().count();
        let (g, l, d) = watched(&format!("log {k}"), move || {
            let g = verbose::groups(&t);
            let l = verbose::loads(&t);
            let d = verbose::discarded(&t);
            let p = iat::pulls(&l).map(|v| v.len());
            (
                g.len() + g.iter().map(|x| x.replaced.len()).sum::<usize>(),
                l.len(),
                (d.len(), p),
            )
        });
        assert!(
            g <= lines && l <= lines && d.0 <= lines,
            "log {k}: {g} {l} {d:?} of {lines} lines"
        );
        assert!(d.1.as_ref().map_or(true, |n| *n <= l), "log {k}: {:?}", d.1);
    }
}

#[test]
fn maps_of_junk_hold_no_more_rows_than_lines() {
    for s in 0..8 {
        let t = msvc6_base::srcio::tool_text(&junk(1 << 16, s + 100));
        let lines = t.lines().count();
        let n = watched("map", move || {
            let m = linkmap::parse(&t);
            m.rows.len() + m.contribs.len()
        });
        assert!(n <= lines);
    }
    let flood = " 0001:00000000 x 00401000 f a.obj\n".repeat(100_000);
    let n = watched("map flood", move || linkmap::parse(&flood).rows.len());
    assert_eq!(n, 0, "rows before `Publics by Value` are no publics");
}

fn comdat_object(sections: usize, body: &[u8]) -> Vec<u8> {
    let mut o = Obj::new();
    for i in 0..sections {
        let n = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: body.to_vec(),
            ..Section::default()
        });
        o.section_symbol(n, SELECT_ANY, 0);
        o.plain(&format!("_f{i}"), 0, n, 0x20, CLASS_EXTERNAL);
    }
    o.build()
}

fn share_section_data(mut d: Vec<u8>, sections: usize) -> Vec<u8> {
    let first = u32::from_le_bytes(d[20 + 20..20 + 24].try_into().unwrap());
    for i in 1..sections {
        let at = 20 + i * 40;
        d[at + 20..at + 24].copy_from_slice(&first.to_le_bytes());
        d[at + 16..at + 20].copy_from_slice(&(1u32 << 16).to_le_bytes());
    }
    d[20 + 16..20 + 20].copy_from_slice(&(1u32 << 16).to_le_bytes());
    d.resize(d.len().max(first as usize + (1 << 16)), 0xc3);
    d
}

#[test]
fn an_object_whose_comdats_share_their_bytes_is_refused() {
    let d = share_section_data(comdat_object(200, &[0xc3]), 200);
    let r = watched("shared", move || {
        object_comdats(&d, 0).map(|i| i.comdats.len())
    });
    let e = r.expect_err("200 sections over one 64 KiB range");
    assert!(
        format!("{e:#}").contains("sections share their data"),
        "{e:#}"
    );
    let ok = comdat_object(3, &[0x33, 0xc0, 0xc3]);
    let n = watched("plain", move || {
        object_comdats(&ok, 0).map(|i| i.comdats.len())
    });
    assert_eq!(n.unwrap(), 3);
}

#[test]
fn a_truncated_object_is_read_or_refused_at_every_length() {
    let full = comdat_object(4, &[0x55, 0x8b, 0xec, 0x5d, 0xc3]);
    let n = full.len();
    let reads = watched("truncated", move || {
        let mut ok = 0usize;
        for cut in 0..n {
            if let Ok(i) = object_comdats(&full[..cut], 0) {
                assert!(i.comdats.len() <= 4);
                assert!(i.comdats.iter().all(|c| c.bytes.len() <= cut));
                ok += 1;
            }
        }
        ok
    });
    assert!(
        reads < n,
        "the cut objects that read are fewer than the cuts"
    );
}

fn cand(i: u32) -> Cand {
    Cand {
        name: format!("_f{i}"),
        obj: 0,
        external: true,
        dead: false,
        dup: false,
        size: 16,
        nreloc: 0,
        chr: 0x6050_1020,
        sel: 2,
        bytes: i.to_le_bytes().to_vec(),
        relocs: Vec::new(),
        cksum: 0,
        label: format!("_f{i}"),
    }
}

#[test]
fn an_equal_key_array_built_to_be_quadratic_spends_its_budget() {
    let arr = Array {
        objects: vec!["A.obj".into()],
        cands: (0..20_000).map(cand).collect(),
        ..Array::default()
    };
    let order: Vec<usize> = (0..arr.cands.len()).collect();
    let e = watched("fold", move || fold(&arr, &order).map(|r| r.len())).expect_err("2e8 pairs");
    assert!(format!("{e:#}").contains("past its budget"), "{e:#}");
    let small = Array {
        objects: vec!["A.obj".into()],
        cands: (0..2000).map(cand).collect(),
        ..Array::default()
    };
    let order: Vec<usize> = (0..small.cands.len()).collect();
    let r = watched("fold small", move || fold(&small, &order).map(|r| r.len()));
    assert_eq!(r.unwrap(), 2000);
}

#[test]
fn an_all_thunk_text_has_one_slot_per_five_bytes() {
    let text = vec![0xe9u8; 1 << 20];
    let n = watched("ilt", move || ilt::read_ilt(&text, 0x1000).slots.len());
    assert!(n <= (1 << 20) / 5);
}

fn sample_image() -> Image {
    Image::parse(sample::sample().pe.build()).unwrap()
}

#[test]
fn proofs_refuse_images_of_two_lengths_and_maps_naming_no_section() {
    let a = sample_image();
    let mut longer = sample::sample().pe.build();
    longer.extend_from_slice(&[0u8; 0x200]);
    let b = Image::parse(longer).unwrap();
    let r = watched("ilt-diff lengths", move || {
        iltdiff::compare(&a, &b).map(|d| d.differing)
    });
    assert!(r.is_err());
    let map = " x\r\n\r\n Preferred load address is 10000000\r\n\r\n Start Length Name Class\r\n \
               0009:00000000 00000010H .text CODE\r\n\r\n  Address Publics by Value Rva+Base Lib:Object\r\n\r\n \
               0009:00000000 _f 10001000 f a.obj\r\n";
    let link = |m: &str| icfdiff::Link {
        img: sample_image(),
        map: linkmap::parse(m),
        groups: Vec::new(),
    };
    let (l1, l2) = (link(map), link(map));
    let r = watched("icf-diff map", move || {
        icfdiff::compare(
            &l1,
            &l2,
            &icfdiff::Residue::default(),
            &icfdiff::Options {
                renames: None,
                content: true,
            },
        )
        .map(|r| r.findings())
    });
    let e = r.expect_err("a contribution naming section 9 of a 6-section image");
    assert!(format!("{e:#}").contains("names no section"), "{e:#}");
}

#[test]
fn import_libraries_with_junk_members_read_what_they_can() {
    let mut lib = Library::default();
    lib.member(
        "K.dll",
        short_import("K.dll", "_Sleep@4", 7, 0, 3),
        &["__imp__Sleep@4", "_Sleep@4"],
    );
    lib.member(
        "K.dll",
        short_import("K.dll", "_Beep@8", 3, 0, 0),
        &["__imp__Beep@8"],
    );
    lib.member("junk.obj", junk(4096, 9), &[]);
    lib.member(
        "cut.dll",
        short_import("cut.dll", "_X@0", 1, 0, 1)[..22].to_vec(),
        &[],
    );
    let data = lib.build();
    let libs = watched("implib", move || include::read_import_lib(&data).unwrap());
    let k = libs.iter().find(|l| l.dll == "K.dll").expect("K.dll");
    assert_eq!(
        k.symbol_for("Sleep").map(String::as_str),
        Some("__imp__Sleep@4")
    );
    assert_eq!(
        k.symbol_for("Ordinal(3)").map(String::as_str),
        Some("__imp__Beep@8")
    );
    for cut in [0usize, 8, 60, 100] {
        let mut b = Library::default();
        b.member(
            "K.dll",
            short_import("K.dll", "_Sleep@4", 7, 0, 3),
            &["__imp__Sleep@4"],
        );
        let d = b.build();
        let d = d[..cut.min(d.len())].to_vec();
        let r = watched("implib cut", move || {
            include::read_import_lib(&d).map(|v| v.len())
        });
        assert!(r.map_or(true, |n| n <= 1));
    }
}

#[test]
fn scripts_and_tables_of_junk_are_refused_by_line() {
    for s in 0..4 {
        let t = msvc6_base::srcio::tool_text(&junk(4096, s + 50));
        let _ = watched("edits", move || iat::parse_edits(&t).map(|v| v.len()));
    }
    let e = iat::parse_edits("drop k.dll!A\nmove k.dll!B\n").unwrap_err();
    assert!(format!("{e:#}").contains("edit line 2"), "{e:#}");
    let many = "A.obj\t\t16:0\t_f\tseen\n".repeat(joint::ROWS_MAX + 1);
    let e = watched("candidates", move || {
        joint::parse_candidates(&many).map(|v| v.len())
    })
    .expect_err("too many rows");
    assert!(format!("{e:#}").contains("more than"), "{e:#}");
    let mut r = icfdiff::Renames::default();
    assert!(r.parse_file("a\tb\nno-tab-here\n").is_err());
}

#[test]
fn the_reference_population_of_a_sample_image_is_its_imports() {
    let img = sample_image();
    let (pulls, orphans) = watched("population", move || {
        iat::reference_population(&img).unwrap()
    });
    assert!(orphans.is_empty(), "{orphans:?}");
    assert_eq!(
        pulls
            .iter()
            .filter(|p| p.kind == verbose::Kind::Import)
            .count(),
        5
    );
    assert_eq!(
        pulls
            .iter()
            .filter(|p| p.kind == verbose::Kind::NullThunk)
            .count(),
        2
    );
}

#[test]
fn a_linker_database_of_junk_finds_no_record_and_never_walks_off_its_end() {
    for s in 0..6u32 {
        let mut d = junk(1 << 16, s + 700);
        d.splice(
            0..msvc6_link::ilk::MAGIC.len(),
            msvc6_link::ilk::MAGIC.to_vec(),
        );
        let base = u32::from_le_bytes([d[0x100], d[0x101], d[0x102], d[0x103]]);
        let rs = watched("ilk records", {
            let d = d.clone();
            move || msvc6_link::ilk::section_records(&d, base)
        });
        assert!(rs.kept.len() <= msvc6_link::ilk::RECORD_CAP);
        assert!(rs.kept.iter().all(|r| r.name.starts_with('.')));
        let (control, lines) = msvc6_link::ilk::record_lines(&rs, &[]);
        assert_eq!(control, msvc6_link::ilk::Control::NotRun);
        assert!(
            lines.last().unwrap().contains("NO CONTROL RUN"),
            "{lines:?}"
        );
    }
    for n in [0usize, 1, 0x20, 0x21, 0x1d4, 0x1d7] {
        assert!(msvc6_link::ilk::header(&junk(n, 9)).is_err());
    }
    let short = msvc6_link::ilk::MAGIC.to_vec();
    assert!(msvc6_link::ilk::header(&short).is_err());
    let known = std::collections::BTreeSet::new();
    let (longest, lines) = watched("ilk walk", || {
        msvc6_link::ilk::walk_lines(&junk(1 << 16, 3), &known)
    });
    assert_eq!(longest, 0);
    assert!(lines.last().unwrap().contains("NO VERDICT"), "{lines:?}");
}

#[test]
fn a_verbose_log_of_junk_loads_nothing_and_a_rule_spec_is_refused_by_shape() {
    for s in 0..6u32 {
        let t = msvc6_base::srcio::tool_text(&junk(1 << 15, s + 800));
        let log = watched("dead-data log", move || msvc6_link::deaddata::parse_log(&t));
        assert!(msvc6_link::deaddata::loaded_as(&log, "x.obj").is_none());
        assert!(!msvc6_link::deaddata::loaded_members(
            &log, "a.lib", "b.obj"
        ));
    }
    let real = msvc6_link::deaddata::parse_log(
        "      Loaded libcmtd.lib(crt0.obj)\n    Discarded _x from libcmtd.lib(crt0.obj)\n",
    );
    assert_eq!(
        msvc6_link::deaddata::loaded_as(&real, "crt0.obj").map(String::as_str),
        Some("libcmtd.lib(crt0.obj)")
    );
    assert!(msvc6_link::deaddata::loaded_members(
        &real,
        "libcmtd.lib",
        "crt0.obj"
    ));
    assert!(!msvc6_link::deaddata::loaded_members(
        &real,
        "libcimtd.lib",
        "crt0.obj"
    ));
    for bad in ["", "noequals", "=glob", "class=", "a=b=c"] {
        let r = msvc6_link::census::parse_rule(bad);
        assert_eq!(
            r.is_err(),
            bad.split_once('=')
                .is_none_or(|(c, g)| c.trim().is_empty() || g.trim().is_empty()),
            "{bad}"
        );
    }
    for bad in ["", "1", "a..b", "5..", "..5", "9..3"] {
        assert!(msvc6_link::memberorder::parse_window(bad).is_err(), "{bad}");
    }
    assert_eq!(
        msvc6_link::memberorder::parse_window("3..9").unwrap(),
        (2, 9),
        "a --window is 1-based and inclusive, so it yields a 0-based half-open pair"
    );
}

#[test]
fn an_object_of_junk_is_refused_rather_than_read_as_sections() {
    let log = msvc6_link::deaddata::parse_log("");
    for s in 0..8u32 {
        let d = junk(4096, s + 900);
        let r = watched("dead-data object", {
            let d = d.clone();
            let log = log.clone();
            move || msvc6_link::deaddata::read_object("j.obj", &d, &log)
        });
        if let Ok(o) = r {
            assert!(
                o.discarded_sections() < 4096,
                "a junk object claimed {} discarded sections",
                o.discarded_sections()
            );
        }
    }
    assert!(msvc6_link::deaddata::read_object("e.obj", &[], &log).is_err());
    let mut o = Obj::new();
    o.code("text", &[0x90, 0xc3]);
    let good = o.build();
    assert!(msvc6_link::deaddata::read_object("g.obj", &good, &log).is_ok());
    for cut in [1usize, 8, 20, 40, good.len() / 2] {
        let _ = watched("truncated object", {
            let part = good.get(..cut.min(good.len())).unwrap().to_vec();
            let log = log.clone();
            move || msvc6_link::deaddata::read_object("t.obj", &part, &log).is_ok()
        });
    }
}

#[test]
fn an_over_long_string_in_the_import_area_is_skipped_whole() {
    use msvc6_fixture::buf::Buf;
    use msvc6_fixture::pe::{DIR_IMPORT, Pe, SCN_CODE, SCN_DATA};
    let mut pe = Pe::dll();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let r = pe.next_rva();
    let table = r + 40;
    let dll = table + 8;
    let long = dll + 6;
    let tail = "b.dll";
    let stray = long + u32::try_from(iat::NAME_MAX + 88 + tail.len() + 1).unwrap();
    let hint = stray + 6;
    let mut b = Buf::new();
    b.u32(table).u32(0).u32(0).u32(dll).u32(table);
    b.zeros(20);
    b.u32(hint).u32(0);
    b.cstr("a.dll");
    b.fill(b'A', iat::NAME_MAX + 88).cstr(tail);
    b.cstr("c.dll");
    b.u16(0).cstr("f").u8(0);
    pe.push(".idata", SCN_DATA, b.into_vec());
    pe.dir(DIR_IMPORT, r, 40);
    let img = Image::parse(pe.build()).unwrap();
    let orphans = watched("orphans", move || iat::orphan_dll_names(&img).unwrap());
    assert_eq!(orphans, vec![(stray, "c.dll".to_string())]);
}
