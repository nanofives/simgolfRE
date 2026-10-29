#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::mac::{
    ATTR_PURE_INSTRUCTIONS, BLR, CPU_I386, CPU_PPC, Function, Library, MachO, MachSection,
    MachSymbol, N_EXT, N_SECT, Pef, Traceback, fat,
};
use msvc6_image::{macho, pef, traceback};

const EDGES: [u32; 6] = [0, 1, 0x7fff_ffff, 0x8000_0000, 0xffff_fffe, 0xffff_ffff];

fn quick<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
}

fn sample() -> Vec<u8> {
    let mut data = vec![0u8; 0x40];
    data[12..16].copy_from_slice(&0x8000u32.to_be_bytes());
    Pef {
        functions: (0..8)
            .map(|k| Function {
                words: vec![0x3860_0000 | k, BLR],
                name: format!(".f{k}__4PoolFi"),
            })
            .collect(),
        data,
        pattern: true,
        libraries: vec![Library {
            name: "InterfaceLib".into(),
            imports: vec!["NewPtr".into()],
        }],
        relocations: vec![0x0082, 0x6000],
        main: 8,
    }
    .build()
}

fn bounded(p: &pef::Pef) {
    assert!(p.data.len() <= pef::MAX_SECTION);
    assert!(p.tables.len() <= p.code.len() / 16 + 1);
    let budget = pef::STEPS_PER_WORD
        .saturating_mul(u64::try_from(p.data.len()).unwrap() + 4096)
        .max(pef::STEP_FLOOR);
    assert!(u64::try_from(p.relocs.len()).unwrap() <= budget);
}

fn put(b: &mut [u8], at: usize, v: u32, big: bool) {
    if let Some(s) = b.get_mut(at..at + 4) {
        s.copy_from_slice(&if big {
            v.to_be_bytes()
        } else {
            v.to_le_bytes()
        });
    }
}

#[test]
fn every_pef_header_and_loader_word_at_an_edge_value_is_read_or_refused() {
    let base = sample();
    let p = pef::load(&base).unwrap();
    let loader = p
        .header
        .sections
        .iter()
        .find(|s| s.kind == pef::Kind::Loader)
        .unwrap()
        .offset as usize;
    let mut words: Vec<usize> = (8..40 + 3 * 28).step_by(4).collect();
    words.extend((0..56 + 24 + 4 + 12).step_by(4).map(|k| loader + k));
    for at in words {
        for v in EDGES {
            let mut b = base.clone();
            put(&mut b, at, v, true);
            if let Ok(p) = quick(&format!("pef word {at:#x}={v:#x}"), || pef::load(&b)) {
                bounded(&p);
            }
        }
    }
}

#[test]
fn a_pef_cut_at_every_length_is_read_or_refused() {
    let base = sample();
    for n in 0..base.len() {
        if let Ok(p) = quick(&format!("pef cut at {n}"), || pef::load(&base[..n])) {
            bounded(&p);
        }
    }
}

#[test]
fn pattern_data_and_relocation_bombs_stop_at_their_bounds() {
    let bomb = [0x41u8, 0x8f, 0xff, 0xff, 0xff, 0x7f, b'x'];
    let e = quick("pidata repeat bomb", || pef::unpack_pidata(&bomb, 1 << 20)).unwrap_err();
    assert!(format!("{e:#}").contains("past"), "{e:#}");
    let zeros = [0x00u8, 0x8f, 0xff, 0xff, 0xff, 0x7f];
    assert!(quick("pidata zero bomb", || pef::unpack_pidata(&zeros, 1 << 20)).is_err());
    let interleave = [0x61u8, 0x01, 0x8f, 0xff, 0xff, 0xff, 0x7f, b'-', b'a'];
    assert!(
        quick("pidata interleave bomb", || pef::unpack_pidata(
            &interleave,
            1 << 20
        ))
        .is_err()
    );
    let mut p = Pef {
        functions: vec![Function {
            words: vec![BLR],
            name: ".f".into(),
        }],
        data: vec![0; 0x40],
        main: 8,
        ..Pef::default()
    };
    p.relocations = vec![0xa000, 0x0000, 0x4200, 0xb0bf, 0xffff];
    let e = quick("relocation repeat flood", || pef::load(&p.build())).unwrap_err();
    assert!(format!("{e:#}").contains("budget"), "{e:#}");
    p.relocations = vec![0x9000];
    assert!(quick("repeat before the start", || pef::load(&p.build())).is_err());
}

#[test]
fn the_traceback_scan_of_hostile_code_is_linear_in_its_bytes() {
    let mut many = Vec::new();
    for k in 0..20_000u32 {
        many.extend_from_slice(&BLR.to_be_bytes());
        many.extend(Traceback::named(&format!(".n{k}"), Some(4)).bytes());
    }
    let t = quick("twenty thousand tables", || traceback::scan(&many));
    assert_eq!(t.len(), 20_000);
    let zeros = vec![0u8; 1 << 20];
    assert!(quick("a megabyte of zeros", || traceback::scan(&zeros)).is_empty());
    let mut ctl = vec![0u8; 4];
    ctl.extend_from_slice(&[0, 0, 0, 0, 0, 9, 0x08, 0x40, 0, 0, 0, 0]);
    ctl.extend_from_slice(&u32::MAX.to_be_bytes());
    ctl.extend(vec![0u8; 512]);
    assert!(
        quick("a controlled-storage count of 2^32", || traceback::scan(
            &ctl
        ))
        .is_empty()
    );
    let mut x: u32 = 0x1234_5678;
    let noise: Vec<u8> = (0..1 << 18)
        .map(|_| {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            u8::try_from(x & 0xff).unwrap()
        })
        .collect();
    let t = quick("random bytes", || traceback::scan(&noise));
    assert!(t.len() <= noise.len() / 16);
}

fn thin() -> Vec<u8> {
    MachO {
        cpu: CPU_I386,
        big_endian: false,
        sections: vec![MachSection {
            segment: "__TEXT".into(),
            name: "__text".into(),
            addr: 0x1000,
            data: vec![0xc3; 0x40],
            flags: ATTR_PURE_INSTRUCTIONS,
            ..MachSection::default()
        }],
        symbols: (0..4)
            .map(|k| MachSymbol {
                name: format!("_f{k}"),
                ty: N_SECT | N_EXT,
                sect: 1,
                value: 0x1000 + k * 0x10,
            })
            .collect(),
        indirect: vec![0, 1, 2],
    }
    .build()
}

#[test]
fn every_macho_header_and_command_word_at_an_edge_value_is_read_or_refused() {
    let base = thin();
    for at in (0..base.len().min(0x200)).step_by(4) {
        for v in EDGES {
            let mut b = base.clone();
            put(&mut b, at, v, false);
            if let Ok(m) = quick(&format!("macho word {at:#x}={v:#x}"), || {
                macho::load(&b, CPU_I386)
            }) {
                assert!(m.symbols.len() <= b.len() / macho::NLIST);
                assert!(m.functions().len() <= m.symbols.len());
                assert!(m.stubs.len() + m.slots.len() <= b.len() / 4);
            }
        }
    }
    for n in 0..base.len() {
        let _ = quick(&format!("macho cut at {n}"), || {
            macho::load(&base[..n], CPU_I386)
        });
    }
}

#[test]
fn hostile_fat_headers_and_counts_are_refused() {
    let mut f = fat(&[(CPU_PPC, thin())]);
    put(&mut f, 4, 0x7fff_ffff, true);
    assert!(quick("fat count", || macho::slices(&f)).is_err());
    let mut g = fat(&[(CPU_I386, thin())]);
    put(&mut g, 16, 0xffff_ff00, true);
    assert!(quick("fat extent", || macho::load(&g, CPU_I386)).is_err());
    let mut h = thin();
    put(&mut h, 16, 0x0100_0000, false);
    assert!(quick("ncmds", || macho::load(&h, CPU_I386)).is_err());
    let mut k = thin();
    put(&mut k, 0, macho::MAGIC_64, false);
    assert!(quick("64-bit", || macho::load(&k, CPU_I386)).is_err());
}

#[test]
fn a_relocation_past_the_data_section_is_refused() {
    let p = Pef {
        functions: vec![Function {
            words: vec![BLR],
            name: ".f".into(),
        }],
        data: vec![0; 0x10],
        relocations: vec![0x4200, 0xb03f, 0xffff],
        main: 8,
        ..Pef::default()
    };
    let e = quick("a run past the data", || pef::load(&p.build())).unwrap_err();
    assert!(
        format!("{e:#}").contains("past the 16-byte data section"),
        "{e:#}"
    );
}

#[test]
fn a_relocation_program_that_relocates_every_word_of_a_packed_section_is_refused() {
    let p = Pef {
        functions: vec![Function {
            words: vec![BLR],
            name: ".f".into(),
        }],
        data: vec![0; 64 << 20],
        pattern: true,
        relocations: vec![0x43ff, 0xb000, 0x7fff],
        main: 8,
        ..Pef::default()
    };
    let bytes = p.build();
    assert!(bytes.len() < 1 << 20, "{}", bytes.len());
    let e = quick("a packed section relocated whole", || pef::load(&bytes)).unwrap_err();
    assert!(format!("{e:#}").contains("relocate more than"), "{e:#}");
}
