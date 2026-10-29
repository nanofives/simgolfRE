#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::mac::{
    ATTR_PURE_INSTRUCTIONS, ATTR_SOME_INSTRUCTIONS, BLR, CPU_I386, CPU_PPC, Function, Library,
    MachO, MachSection, MachSymbol, N_EXT, N_SECT, NON_LAZY_POINTERS, Pef, SYMBOL_STUBS, Traceback,
    code_section, fat, pidata,
};
use msvc6_image::macho;
use msvc6_image::pef::{self, Kind, Target};
use msvc6_image::traceback;

fn sample_pef(pattern: bool) -> Pef {
    let mut data = vec![0u8; 0x40];
    data[0x24..0x24 + 12].copy_from_slice(b"Pool notice\0");
    data[8..12].copy_from_slice(&0x0000_0004u32.to_be_bytes());
    data[12..16].copy_from_slice(&0x0000_8000u32.to_be_bytes());
    data[0x10..0x14].copy_from_slice(&0x0000_0020u32.to_be_bytes());
    Pef {
        functions: vec![
            Function {
                words: vec![0x8062_0000, 0x3883_0004, BLR],
                name: ".alloc__4PoolFi".into(),
            },
            Function {
                words: vec![0x3860_0000, BLR],
                name: ".__sinit_pool_cpp".into(),
            },
        ],
        data,
        pattern,
        libraries: vec![Library {
            name: "InterfaceLib".into(),
            imports: vec!["NewPtr".into(), "DisposePtr".into()],
        }],
        relocations: vec![0x0082, 0x6001],
        main: 8,
    }
}

#[test]
fn a_pef_reads_its_sections_loader_relocations_toc_and_tracebacks() {
    for pattern in [false, true] {
        let bytes = sample_pef(pattern).build();
        let p = pef::load(&bytes).unwrap();
        assert_eq!(p.header.architecture, "pwpc");
        assert_eq!(p.header.sections.len(), 3);
        assert_eq!(
            p.header.sections[1].kind,
            if pattern {
                Kind::PatternData
            } else {
                Kind::UnpackedData
            }
        );
        assert_eq!(p.data.len(), 0x40);
        assert_eq!(&p.data[0x24..0x2f], b"Pool notice");
        assert_eq!(p.loader.libraries.len(), 1);
        assert_eq!(p.loader.libraries[0].name, "InterfaceLib");
        assert_eq!(p.import_name(1), Some("DisposePtr"));
        assert!(p.loader.imports[0].class == 2 && !p.loader.imports[0].weak);
        assert_eq!(p.target(8), Some(Target::Data(4)));
        assert_eq!(p.target(12), Some(Target::Data(0x8000)));
        assert_eq!(p.target(0x10), Some(Target::Import(1)));
        assert_eq!(p.toc, 0x8000);
        let names: Vec<&str> = p.tables.iter().map(traceback::Table::function).collect();
        assert_eq!(names, ["alloc__4PoolFi", "__sinit_pool_cpp"]);
        let (_, spans) = code_section(&sample_pef(pattern).functions);
        for (t, (start, at)) in p.tables.iter().zip(spans) {
            assert_eq!((t.start, t.at), (start, at));
            assert_eq!(t.lang, 9);
        }
    }
}

#[test]
fn pattern_data_round_trips_every_run_kind() {
    let mut raw = vec![0u8; 100];
    raw.extend(b"abc\0\0\0xyz");
    raw.extend(vec![7u8; 300]);
    raw.extend(vec![0u8; 3]);
    let packed = pidata(&raw);
    assert!(packed.len() < raw.len());
    assert_eq!(pef::unpack_pidata(&packed, raw.len()).unwrap(), raw);
    assert!(pef::unpack_pidata(&packed, raw.len() - 1).is_err());
}

#[test]
fn a_container_without_a_loader_or_a_main_is_refused() {
    let p = sample_pef(false);
    let mut bytes = p.build();
    bytes[40 + 2 * 28 + 24] = 3;
    let e = pef::load(&bytes).unwrap_err();
    assert!(format!("{e:#}").contains("loader"), "{e:#}");
    let mut odd = sample_pef(false);
    odd.main = 0x30;
    assert!(pef::load(&odd.build()).is_ok());
    assert!(pef::load(b"Joy!pef").is_err());
    assert!(pef::load(b"MZ\0\0").is_err());
}

#[test]
fn the_scan_finds_a_table_without_an_offset_after_the_previous_table() {
    let mut code = vec![0u8; 4];
    code.extend_from_slice(&BLR.to_be_bytes());
    code.extend(Traceback::named(".first", Some(4)).bytes());
    let second = u32::try_from(code.len()).unwrap();
    code.extend_from_slice(&BLR.to_be_bytes());
    code.extend(Traceback::named(".second", None).bytes());
    let t = traceback::scan(&code);
    assert_eq!(t.len(), 2);
    assert_eq!(t[1].start, second);
    assert_eq!(t[1].tb_offset, None);
    assert_eq!(t[1].len(), 4);
}

fn sample_macho(cpu: u32, big: bool) -> MachO {
    let mut text = vec![0x55u8, 0x89, 0xe5, 0xc9, 0xc3];
    text.resize(0x20, 0x90);
    MachO {
        cpu,
        big_endian: big,
        sections: vec![
            MachSection {
                segment: "__TEXT".into(),
                name: "__text".into(),
                addr: 0x1000,
                data: text,
                flags: ATTR_PURE_INSTRUCTIONS | ATTR_SOME_INSTRUCTIONS,
                ..MachSection::default()
            },
            MachSection {
                segment: "__IMPORT".into(),
                name: "__jump_table".into(),
                addr: 0x2000,
                data: vec![0xf4; 10],
                flags: SYMBOL_STUBS | ATTR_SOME_INSTRUCTIONS,
                reserved1: 0,
                reserved2: 5,
            },
            MachSection {
                segment: "__DATA".into(),
                name: "__nl_symbol_ptr".into(),
                addr: 0x3000,
                data: vec![0; 4],
                flags: NON_LAZY_POINTERS,
                reserved1: 2,
                reserved2: 0,
            },
        ],
        symbols: vec![
            MachSymbol {
                name: "__ZN6Widget4drawEii".into(),
                ty: N_SECT | N_EXT,
                sect: 1,
                value: 0x1000,
            },
            MachSymbol {
                name: "__ZN6WidgetC1Ev".into(),
                ty: N_SECT | N_EXT,
                sect: 1,
                value: 0x1010,
            },
            MachSymbol {
                name: "__ZN6WidgetC2Ev".into(),
                ty: N_SECT | N_EXT,
                sect: 1,
                value: 0x1010,
            },
            MachSymbol {
                name: "_strcpy".into(),
                ty: N_EXT,
                sect: 0,
                value: 0,
            },
            MachSymbol {
                name: "_sprintf".into(),
                ty: N_EXT,
                sect: 0,
                value: 0,
            },
        ],
        indirect: vec![3, 4, 3],
    }
}

#[test]
fn a_thin_i386_slice_names_its_functions_stubs_and_pointers() {
    let m = macho::load(&sample_macho(CPU_I386, false).build(), CPU_I386).unwrap();
    assert!(!m.big_endian);
    let f = m.functions();
    assert_eq!(
        f,
        [
            (0x1000, 0x1010, "_ZN6Widget4drawEii".to_string()),
            (0x1010, 0x1020, "_ZN6WidgetC1Ev".to_string()),
        ]
    );
    assert_eq!(m.stubs.get(&0x2000).map(String::as_str), Some("strcpy"));
    assert_eq!(m.stubs.get(&0x2005).map(String::as_str), Some("sprintf"));
    assert_eq!(m.slots.get(&0x3000).map(String::as_str), Some("strcpy"));
    assert_eq!(m.section_bytes(&m.sections[0]).map(<[u8]>::len), Some(0x20));
}

#[test]
fn a_fat_file_gives_each_slice_and_its_ppc_tracebacks() {
    let mut ppc = sample_macho(CPU_PPC, true);
    let (code, _) = code_section(&[Function {
        words: vec![0x3860_0001, BLR],
        name: ".tick__5ClockFv".into(),
    }]);
    ppc.sections[0].data = code;
    let file = fat(&[
        (CPU_I386, sample_macho(CPU_I386, false).build()),
        (CPU_PPC, ppc.build()),
    ]);
    let s = macho::slices(&file).unwrap();
    assert_eq!(
        s.iter().map(|x| x.cpu).collect::<Vec<_>>(),
        [CPU_I386, CPU_PPC]
    );
    let m = macho::load(&file, CPU_PPC).unwrap();
    assert!(m.big_endian);
    let t = traceback::scan(m.section_bytes(&m.sections[0]).unwrap());
    assert_eq!(t.len(), 1);
    assert_eq!(t[0].function(), "tick__5ClockFv");
    assert!(macho::load(&fat(&[(CPU_PPC, ppc.build())]), CPU_I386).is_err());
}
