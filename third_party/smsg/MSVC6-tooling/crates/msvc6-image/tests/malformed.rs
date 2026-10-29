#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_base::units::Rva;
use msvc6_fixture::archive::Library;
use msvc6_fixture::buf::Buf;
use msvc6_fixture::coff::Object as Obj;
use msvc6_fixture::pe::{DIR_IMPORT, DIR_RESOURCE, Pe, SCN_CODE, SCN_DATA, SCN_RDATA};
use msvc6_image::dump::textbss;
use msvc6_image::objcmp::{Masks, canonical};
use msvc6_image::pe::Image;
use msvc6_image::postlink::{Anchor, Kind, Row, resolve};

fn quick<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
}

fn peak_rss_kb() -> Option<u64> {
    let status = msvc6_base::srcio::own_read("/proc/self/status").ok()?;
    status
        .lines()
        .find_map(|l| l.strip_prefix("VmHWM:"))?
        .trim()
        .strip_suffix("kB")?
        .trim()
        .parse()
        .ok()
}

fn err<T: std::fmt::Debug>(what: &str, r: anyhow::Result<T>, want: &str) {
    let e = r.expect_err(what);
    assert!(format!("{e:#}").contains(want), "{what}: {e:#}");
}

fn patched(b: &[u8], img: &Image, rva: u32, v: u32) -> Image {
    let mut b = b.to_vec();
    let at = img.rva_to_off(Rva(rva)).unwrap().get();
    b[at..at + 4].copy_from_slice(&v.to_le_bytes());
    Image::parse(b).unwrap()
}

#[test]
fn an_export_count_past_its_tables_is_an_error_not_a_loop() {
    let s = common::sample();
    let b = s.pe.build();
    let img = Image::parse(b.clone()).unwrap();
    for n in [0xffff_ffff, 0x0100_0000, 0x400] {
        let bad = patched(&b, &img, s.edata + 24, n);
        err("names", quick("exports", || bad.exports()), "NumberOfNames");
    }
    let bad = patched(&b, &img, s.edata + 20, 0xffff_ffff);
    err(
        "functions",
        quick("exports", || bad.exports()),
        "NumberOfFunctions",
    );
    let ords = u32::from_le_bytes(
        img.at_rva(Rva(s.edata + 36), 4)
            .unwrap()
            .try_into()
            .unwrap(),
    );
    let bad = patched(&b, &img, ords, 0x00ff);
    err(
        "ordinal",
        quick("exports", || bad.exports()),
        "past NumberOfFunctions",
    );
}

fn idata_image(build: impl FnOnce(u32) -> Vec<u8>) -> Image {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let rva = pe.next_rva();
    let body = build(rva);
    pe.push(".idata", SCN_DATA, body);
    pe.dir(DIR_IMPORT, rva, 40);
    Image::parse(pe.build()).unwrap()
}

fn shared_thunks() -> Image {
    idata_image(|r| {
        let (dlls, thunks) = (800u32, 3000u32);
        let table = r + (dlls + 1) * 20;
        let name = table + (thunks + 1) * 4;
        let mut b = Buf::new();
        for _ in 0..dlls {
            b.u32(table).u32(0).u32(0).u32(name).u32(table);
        }
        b.zeros(20);
        for _ in 0..thunks {
            b.u32(name);
        }
        b.u32(0);
        b.cstr("a.dll");
        b.into_vec()
    })
}

#[test]
fn import_tables_that_share_their_thunks_are_refused() {
    let img = shared_thunks();
    err(
        "shared",
        quick("imports", || img.imports()),
        "more import entries",
    );
    let row = Row {
        kind: Kind::ImportStamps,
        off: Some(
            img.rva_to_off(Rva(img.dirs[1].rva.get() + 4))
                .unwrap()
                .get(),
        ),
        rva: None,
        len: 8,
        at: Some(Anchor::ImportStamps),
        fill: None,
        note: String::new(),
    };
    err(
        "postlink",
        quick("postlink", || resolve(&[row], &img, &img)),
        "more import entries",
    );
}

#[test]
fn a_descriptor_walk_or_a_thunk_table_with_no_terminator_is_an_error() {
    let selfish = idata_image(|r| {
        let mut b = Buf::new();
        for _ in 0..0x1000 {
            b.u32(r);
        }
        b.into_vec()
    });
    err(
        "no null descriptor",
        quick("imports", || selfish.imports()),
        "no null descriptor",
    );
    let inside = idata_image(|r| {
        let mut b = Buf::new();
        b.u32(r + 4).u32(0).u32(0).u32(r + 60).u32(r + 44);
        b.zeros(20);
        b.u32(0).u32(0).u32(0);
        b.cstr("a.dll");
        b.into_vec()
    });
    err(
        "inside",
        quick("imports", || inside.imports()),
        "inside the descriptor array",
    );
    let endless = idata_image(|r| {
        let mut b = Buf::new();
        b.u32(r + 48).u32(0).u32(0).u32(r + 40).u32(r + 48);
        b.zeros(20);
        b.cstr("a.dll");
        b.pad_to(48);
        for _ in 0..(512 - 48) / 4 {
            b.u32(r + 40);
        }
        b.into_vec()
    });
    err(
        "endless",
        quick("imports", || endless.imports()),
        "no null thunk",
    );
}

fn rsrc_image(body: Vec<u8>) -> Image {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let rva = pe.next_rva();
    let n = u32::try_from(body.len()).unwrap();
    pe.push(".rsrc", SCN_RDATA, body);
    pe.dir(DIR_RESOURCE, rva, n);
    Image::parse(pe.build()).unwrap()
}

fn dir(b: &mut Buf, named: u16, ids: u16) {
    b.u32(0).u32(0).u16(0).u16(0).u16(named).u16(ids);
}

#[test]
fn a_resource_tree_that_fans_out_or_shares_a_node_is_refused() {
    let mut b = Buf::new();
    dir(&mut b, 0, 64);
    let node_b = 16 + 64 * 8;
    for i in 0..64u32 {
        b.u32(i + 1).u32(0x8000_0000 | node_b);
    }
    dir(&mut b, 0, 64);
    let node_c = node_b + 16 + 64 * 8;
    for i in 0..64u32 {
        b.u32(i + 1).u32(0x8000_0000 | node_c);
    }
    dir(&mut b, 0xffff, 0xffff);
    let fan = rsrc_image(b.into_vec());
    err(
        "fan-out",
        quick("resources", || fan.resources()),
        "entries run past",
    );
    let mut b = Buf::new();
    dir(&mut b, 0, 2);
    let leafdir = 16 + 2 * 8;
    b.u32(1).u32(0x8000_0000 | leafdir);
    b.u32(2).u32(0x8000_0000 | leafdir);
    dir(&mut b, 0, 1);
    b.u32(1033).u32(leafdir + 24);
    b.zeros(16);
    let shared = rsrc_image(b.into_vec());
    err(
        "shared",
        quick("resources", || shared.resources()),
        "reached twice",
    );
}

#[test]
fn more_leaves_than_the_section_can_hold_is_an_error() {
    let mut b = Buf::new();
    let n = 600u32;
    dir(&mut b, 0, u16::try_from(n).unwrap());
    let leaf = 16 + n * 8;
    for i in 0..n {
        b.u32(i).u32(leaf);
    }
    b.zeros(16);
    let img = rsrc_image(b.into_vec());
    err(
        "leaves",
        quick("resources", || img.resources()),
        "more resource leaves",
    );
}

#[test]
fn a_truncated_or_unnamed_resource_directory_is_an_error_not_empty() {
    let s = common::sample();
    let b = s.pe.build();
    let img = Image::parse(b.clone()).unwrap();
    let at = img.rva_to_off(Rva(s.rsrc)).unwrap().get();
    let cut = Image::parse(b[..at + 20].to_vec()).unwrap();
    err(
        "truncated",
        quick("resources", || cut.resources()),
        "run past the file bytes",
    );
    let header_only = Image::parse(b[..at + 8].to_vec()).unwrap();
    err(
        "header",
        quick("resources", || header_only.resources()),
        "root does not fit",
    );
    let mut nb = Buf::new();
    dir(&mut nb, 1, 0);
    nb.u32(0x8000_ff00).u32(24);
    nb.zeros(16);
    let unnamed = rsrc_image(nb.into_vec());
    err(
        "name",
        quick("resources", || unnamed.resources()),
        "resource name",
    );
}

#[test]
fn canonical_refuses_an_archive_and_a_symbol_count_past_the_file() {
    let mut l = Library::default();
    let mut o = Obj::new();
    o.code(".text", &[0xc3]);
    l.member("a.obj", o.build(), &["_a"]);
    let lib = l.build();
    let m = Masks {
        labels: true,
        ..Masks::default()
    };
    assert!(quick("archive", || canonical(&lib, m)).is_err());
    let mut obj = o.build();
    obj[12..16].copy_from_slice(&0xffff_ffffu32.to_le_bytes());
    err("nsym", quick("nsym", || canonical(&obj, m)), "run past");
    obj[12..16].copy_from_slice(&0x0fff_ffffu32.to_le_bytes());
    assert!(quick("nsym", || canonical(&obj, m)).is_err());
}

#[test]
fn a_huge_virtual_size_is_counted_not_materialised() {
    let mut pe = Pe::exe();
    pe.push_virtual(".text", SCN_CODE, vec![0xc3, 0, 0, 0], 0xffff_e000);
    let img = Image::parse(pe.build()).unwrap();
    let before = peak_rss_kb();
    let t = quick("textbss", || textbss(&img, ".text")).unwrap();
    if let (Some(a), Some(b)) = (before, peak_rss_kb()) {
        assert!(
            b.saturating_sub(a) < 512 * 1024,
            "the peak resident set grew by {} kB: the 4 GB virtual size was materialised",
            b.saturating_sub(a)
        );
    }
    assert_eq!(t.run, 0xffff_e000 - 1);
    assert_eq!(t.run_from, 0x1000 + 1);
}

#[test]
fn import_names_that_share_one_long_run_are_refused_within_their_budget() {
    let thunks = 8192u32;
    let img = idata_image(|r| {
        let table = r + 40;
        let dll = table + (thunks + 1) * 4;
        let hint = dll + 8;
        let mut b = Buf::new();
        b.u32(table).u32(0).u32(0).u32(dll).u32(table);
        b.zeros(20);
        for _ in 0..thunks {
            b.u32(hint);
        }
        b.u32(0);
        b.cstr("a.dll");
        b.pad_to(usize::try_from(hint - r).unwrap());
        b.u16(0).fill(b'A', 1 << 16).u8(0);
        b.into_vec()
    });
    err(
        "imports",
        quick("imports", || img.imports()),
        "names overlap",
    );
}

#[test]
fn export_names_that_share_one_long_run_are_refused_within_their_budget() {
    let n = 8192u32;
    let mut pe = Pe::dll();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let r = pe.next_rva();
    let (funcs, names, ords) = (r + 40, r + 40 + n * 4, r + 40 + n * 8);
    let (dll, run) = (ords + n * 2, ords + n * 2 + 8);
    let mut b = Buf::new();
    b.u32(0).u32(0).u32(0).u32(dll).u32(1).u32(n).u32(n);
    b.u32(funcs).u32(names).u32(ords);
    for _ in 0..n {
        b.u32(0x1000);
    }
    for _ in 0..n {
        b.u32(run);
    }
    for i in 0..n {
        b.u16(u16::try_from(i).unwrap());
    }
    b.cstr("a.dll");
    b.pad_to(usize::try_from(run - r).unwrap());
    b.fill(b'A', 1 << 16).u8(0);
    pe.push(".edata", SCN_RDATA, b.into_vec());
    pe.dir(msvc6_fixture::pe::DIR_EXPORT, r, 40);
    let img = Image::parse(pe.build()).unwrap();
    err(
        "exports",
        quick("exports", || img.exports()),
        "names overlap",
    );
}

#[test]
fn an_export_name_with_no_nul_is_read_to_max_name_bytes() {
    use msvc6_image::pe::image::MAX_NAME;
    let mut pe = Pe::dll();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let r = pe.next_rva();
    let (funcs, names, ords) = (r + 40, r + 44, r + 48);
    let (dll, name) = (r + 50, r + 58);
    let mut b = Buf::new();
    b.u32(0).u32(0).u32(0).u32(dll).u32(1).u32(1).u32(1);
    b.u32(funcs).u32(names).u32(ords);
    b.u32(0x1000).u32(name).u16(0);
    b.cstr("a.dll");
    b.pad_to(usize::try_from(name - r).unwrap());
    b.fill(b'A', 2 * MAX_NAME);
    let size = u32::try_from(b.len()).unwrap();
    pe.push(".edata", SCN_RDATA, b.into_vec());
    pe.dir(msvc6_fixture::pe::DIR_EXPORT, r, size);
    let img = Image::parse(pe.build()).unwrap();
    let s = img.cstr_at_rva(Rva(name)).unwrap();
    assert_eq!(s.len(), MAX_NAME);
    assert!(s.bytes().all(|c| c == b'A'));
    let e = img.exports().unwrap().unwrap();
    let got: Vec<usize> = e
        .entries
        .iter()
        .filter_map(|x| x.name.as_ref().map(String::len))
        .collect();
    assert_eq!(got, [MAX_NAME]);
}

fn rsrc_at(build: impl FnOnce(u32) -> Vec<u8>) -> Image {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let rva = pe.next_rva();
    let body = build(rva);
    let n = u32::try_from(body.len()).unwrap();
    pe.push(".rsrc", SCN_RDATA, body);
    pe.dir(DIR_RESOURCE, rva, n);
    Image::parse(pe.build()).unwrap()
}

#[test]
fn resource_entries_that_share_one_long_name_are_refused_within_their_budget() {
    let n = 4096u32;
    let img = rsrc_at(|_| {
        let mut b = Buf::new();
        dir(&mut b, u16::try_from(n).unwrap(), 0);
        let leaf = 16 + n * 8;
        let name = leaf + 16;
        for _ in 0..n {
            b.u32(0x8000_0000 | name).u32(leaf);
        }
        b.zeros(16);
        b.u16(0xffff);
        for _ in 0..0xffff {
            b.u16(u16::from(b'A'));
        }
        b.into_vec()
    });
    err(
        "names",
        quick("resources", || img.resources()),
        "resource names read",
    );
}

#[test]
fn resource_leaves_that_share_one_data_entry_build_no_res_file() {
    let n = 2048u32;
    let img = rsrc_at(|r| {
        let mut b = Buf::new();
        dir(&mut b, 0, u16::try_from(n).unwrap());
        let leaf = 16 + n * 8;
        for i in 0..n {
            b.u32(i + 1).u32(leaf);
        }
        b.u32(r + leaf + 16).u32(1 << 16).u32(0).u32(0);
        b.fill(0x55, 1 << 16);
        b.into_vec()
    });
    assert_eq!(quick("resources", || img.resources()).unwrap().len(), 2048);
    err(
        "res",
        quick("res", || msvc6_image::res::res_file(&img)),
        "their data overlap",
    );
}

fn object_with_relocations() -> (Vec<u8>, msvc6_image::coff::SectionHeader) {
    let mut o = Obj::new();
    let a = o.code(".text", &[0xe8, 0, 0, 0, 0, 0xe8, 0, 0, 0, 0, 0xc3]);
    o.code(".text", &[0xc3; 64]);
    let f = o.undefined("_f");
    o.sections[0].relocs = vec![
        msvc6_fixture::coff::Reloc {
            va: 1,
            symbol: f,
            typ: 20,
        },
        msvc6_fixture::coff::Reloc {
            va: 6,
            symbol: f,
            typ: 20,
        },
    ];
    o.external("_g", 0, a);
    let b = o.build();
    let first = msvc6_image::coff::Object::parse(&b).unwrap().sections[0].clone();
    (b, first)
}

#[test]
fn canonical_refuses_sections_that_share_a_relocation_table_or_their_data() {
    let m = Masks::default();
    let (b, first) = object_with_relocations();
    assert!(quick("clean", || canonical(&b, m)).is_ok());
    let second = 20 + 40;
    let mut shared = b.clone();
    shared[second + 24..second + 28].copy_from_slice(&first.reloc_ptr.to_le_bytes());
    shared[second + 32..second + 34].copy_from_slice(&first.relocs.to_le_bytes());
    err(
        "relocs",
        quick("relocs", || canonical(&shared, m)),
        "share relocation bytes",
    );
    let mut past = b.clone();
    past[second + 24..second + 28]
        .copy_from_slice(&u32::try_from(b.len() - 4).unwrap().to_le_bytes());
    past[second + 32..second + 34].copy_from_slice(&0xffffu16.to_le_bytes());
    err("past", quick("past", || canonical(&past, m)), "run past");
    let mut data = b.clone();
    let size = u32::try_from(b.len()).unwrap();
    for h in [20, second] {
        data[h + 16..h + 20].copy_from_slice(&(size / 2 + 1).to_le_bytes());
        data[h + 20..h + 24].copy_from_slice(&20u32.to_le_bytes());
    }
    err(
        "data",
        quick("data", || canonical(&data, m)),
        "their data overlap",
    );
}

#[test]
fn canonical_reads_a_long_name_inside_its_string_table_only() {
    let mut o = Obj::new();
    let s = o.code(".text", &[0xc3]);
    o.external("_a_long_symbol_name", 0, s);
    let mut b = o.build();
    assert_eq!(b.pop(), Some(0));
    b.extend_from_slice(b"_and_the_bytes_after_the_table");
    let m = Masks::default();
    let lines = quick("canonical", || canonical(&b, m)).unwrap();
    let sym = lines
        .iter()
        .find(|l| l.starts_with("sym 0 "))
        .unwrap()
        .clone();
    assert!(sym.starts_with("sym 0 _a_long_symbol_nam"), "{sym}");
    assert!(!sym.contains("after_the_table"), "{sym}");
}

#[test]
fn comdats_are_found_in_one_pass_over_the_symbols() {
    let n = usize::try_from(i16::MAX).unwrap();
    let fillers = 400_000usize;
    let mut o = Obj::new();
    for _ in 0..fillers {
        o.symbols.push(msvc6_fixture::coff::Symbol {
            name: "_u".into(),
            value: 0,
            section: 0,
            typ: 0x20,
            class: msvc6_fixture::coff::CLASS_EXTERNAL,
            aux: Vec::new(),
        });
    }
    for k in 0..n {
        let s = o.section(msvc6_fixture::coff::Section {
            name: ".text".into(),
            characteristics: msvc6_fixture::coff::SCN_TEXT | msvc6_fixture::coff::SCN_COMDAT,
            data: vec![0xc3],
            ..msvc6_fixture::coff::Section::default()
        });
        o.symbols.push(msvc6_fixture::coff::Symbol {
            name: ".text".into(),
            value: 0,
            section: s,
            typ: 0,
            class: msvc6_fixture::coff::CLASS_STATIC,
            aux: vec![msvc6_fixture::coff::section_aux(
                1,
                0,
                0,
                0,
                0,
                msvc6_fixture::coff::SELECT_ANY,
            )],
        });
        o.symbols.push(msvc6_fixture::coff::Symbol {
            name: format!("_f{k}"),
            value: 0,
            section: s,
            typ: 0x20,
            class: msvc6_fixture::coff::CLASS_EXTERNAL,
            aux: Vec::new(),
        });
    }
    let b = o.build();
    let obj = msvc6_image::coff::Object::parse(&b).unwrap();
    let c = quick("comdats", || obj.comdats());
    assert_eq!(c.len(), n);
    assert_eq!(c[n - 1].symbol.as_deref(), Some("_f32766"));
    assert!(c.iter().all(|x| x.symbol.is_some()));
}

#[test]
fn a_library_diff_pairs_its_members_by_name_in_one_pass() {
    let n = 100_000usize;
    let (mut a, mut b) = (Library::default(), Library::default());
    for k in 0..n {
        a.member(&format!("a{k}.obj"), vec![0, 0], &[]);
        b.member(&format!("b{k}.obj"), vec![0, 0], &[]);
    }
    let (la, lb) = (a.build(), b.build());
    let (xa, xb) = (
        msvc6_image::archive::Archive::parse(&la).unwrap(),
        msvc6_image::archive::Archive::parse(&lb).unwrap(),
    );
    let d = quick("libdiff", || msvc6_image::libdiff::diff(&xa, &xb)).unwrap();
    assert_eq!(d.rows.len(), 2 * n);
    assert_eq!(
        d.rows
            .iter()
            .filter(|r| r.status == "only-reference")
            .count(),
        n
    );
}

#[test]
fn an_image_file_past_its_cap_or_no_regular_file_is_refused_before_it_is_read() {
    let d = std::env::temp_dir().join(format!("msvc6-image-load-{}", std::process::id()));
    std::fs::create_dir_all(&d).unwrap();
    let big = d.join("big.exe");
    std::fs::File::create(&big)
        .unwrap()
        .set_len(msvc6_image::pe::MAX_IMAGE + 1)
        .unwrap();
    let zero = d.join("zero.exe");
    std::os::unix::fs::symlink("/dev/zero", &zero).unwrap();
    let fifo = d.join("fifo.exe");
    let made = std::process::Command::new("mkfifo")
        .arg(&fifo)
        .status()
        .is_ok_and(|s| s.success());
    let mut cases = vec![(big, "more than the"), (zero, "not a regular file")];
    if made {
        cases.push((fifo, "not a regular file"));
    }
    for (p, want) in cases {
        let what = p.display().to_string();
        let q = p.clone();
        let e = quick(&what, move || Image::load(&q).map(|i| i.len()));
        let e = format!("{:#}", e.unwrap_err());
        assert!(e.contains(want), "{what}: {e}");
    }
    let ok = d.join("ok.exe");
    msvc6_base::srcio::own_write(&ok, Pe::exe().build()).unwrap();
    assert!(Image::load(&ok).is_ok());
    std::fs::remove_dir_all(&d).ok();
}
