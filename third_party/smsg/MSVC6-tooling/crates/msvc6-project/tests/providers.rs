#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::pe::{Pe, SCN_CODE};
use msvc6_image::pe::Image;
use msvc6_project::discover::discover;
use msvc6_project::functions::{Function, FunctionIndex};

fn rel(from: u32, to: u32) -> [u8; 4] {
    i32::try_from(i64::from(to) - i64::from(from) - 5)
        .unwrap()
        .to_le_bytes()
}

#[test]
fn discover_finds_the_entry_call_targets_aligned_starts_and_thunks() {
    let mut pe = Pe::dll();
    let base = pe.next_rva();
    let mut code = vec![0x55, 0x8b, 0xec, 0x5d, 0xc3];
    code.resize(0x10, 0xcc);
    code.push(0xe8);
    code.extend_from_slice(&rel(base + 0x10, base + 0x20));
    code.push(0xc3);
    code.resize(0x20, 0xcc);
    code.extend_from_slice(&[0x33, 0xc0, 0xc3]);
    code.resize(0x30, 0xcc);
    code.push(0xe9);
    code.extend_from_slice(&rel(base + 0x30, base + 0x20));
    code.resize(0x40, 0xcc);
    let text = pe.push(".text", SCN_CODE, code);
    assert_eq!(text, base);
    pe.entry = text;
    let img = Image::parse(pe.build()).unwrap();
    let fs = discover(&img);
    let got: Vec<(u32, Option<u32>, bool)> = fs
        .iter()
        .map(|f| (f.rva - text, f.end.map(|e| e - text), f.thunk))
        .collect();
    assert_eq!(
        got,
        [
            (0x00, Some(0x05), false),
            (0x10, Some(0x16), false),
            (0x20, Some(0x23), false),
            (0x30, Some(0x35), true),
        ]
    );
    assert!(
        fs.iter()
            .all(|f| f.provider == "discover" && f.name.is_none())
    );
}

#[test]
fn the_index_keeps_the_first_providers_row_and_fills_what_it_lacks() {
    let f = |rva, end, name: Option<&str>, provider: &str| Function {
        rva,
        end,
        name: name.map(str::to_string),
        thunk: false,
        provider: provider.to_string(),
        object: None,
    };
    let mut ix = FunctionIndex::default();
    ix.add(
        "a",
        "a",
        vec![
            f(0x10, None, Some("A"), "a"),
            f(0x30, Some(0x40), None, "a"),
        ],
        Vec::new(),
    );
    ix.add(
        "b",
        "b",
        vec![
            f(0x10, Some(0x20), Some("B"), "b"),
            f(0x50, Some(0x60), Some("C"), "b"),
        ],
        Vec::new(),
    );
    assert_eq!(ix.len(), 3);
    let a = ix.get(0x10).unwrap();
    assert_eq!(
        (a.name.as_deref(), a.end, a.provider.as_str()),
        (Some("A"), Some(0x20), "a")
    );
    assert_eq!(ix.containing(0x35).unwrap().rva, 0x30);
    assert!(ix.containing(0x45).is_none());
    assert_eq!(ix.stats[1].new, 1);
}

#[test]
fn a_thunk_flag_follows_the_provider_that_gave_the_extent() {
    let f = |rva, end, thunk, provider: &str| Function {
        rva,
        end,
        name: None,
        thunk,
        provider: provider.to_string(),
        object: None,
    };
    let mut ix = FunctionIndex::default();
    ix.add(
        "reccmp-csv",
        "carves.csv",
        vec![f(0x617e0, Some(0x617f0), false, "reccmp-csv")],
        Vec::new(),
    );
    ix.add(
        "map",
        "app.map",
        vec![f(0x700, None, false, "map")],
        Vec::new(),
    );
    ix.add(
        "ghidra-json",
        "functions.json",
        vec![
            f(0x617e0, Some(0x617e5), true, "ghidra-json"),
            f(0x700, Some(0x705), true, "ghidra-json"),
        ],
        Vec::new(),
    );
    let carved = ix.get(0x617e0).unwrap();
    assert_eq!(
        (carved.end, carved.thunk, carved.provider.as_str()),
        (Some(0x617f0), false, "reccmp-csv")
    );
    let mapped = ix.get(0x700).unwrap();
    assert_eq!((mapped.end, mapped.thunk), (Some(0x705), true));
}
