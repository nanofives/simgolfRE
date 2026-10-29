#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_image::anatomy::{Kind, anatomy, member_order, orders};
use msvc6_image::pe::Image;

fn idata_parts(img: &Image) -> Vec<(u32, u32, Kind, String)> {
    let t = img.imports().unwrap();
    let a = anatomy(img, &t);
    let s = a.iter().find(|s| s.section == ".idata").unwrap();
    s.parts
        .iter()
        .map(|p| (p.lo - s.lo, p.hi - s.lo, p.kind, p.label.clone()))
        .collect()
}

#[test]
fn every_table_reserve_padding_and_tail_is_named() {
    let s = common::sample();
    let img = common::image(&s.pe);
    let got = idata_parts(&img);
    let t = |lo, hi, l: &str| (lo, hi, Kind::Table, l.to_string());
    let g = |lo, hi, k| (lo, hi, k, String::new());
    assert_eq!(
        got,
        vec![
            t(0, 60, "import descriptors"),
            t(60, 76, "ILT KERNEL32.dll"),
            g(76, 116, Kind::Reserve),
            t(116, 128, "ILT WS2_32.dll"),
            g(128, 168, Kind::Reserve),
            t(168, 184, "IAT KERNEL32.dll"),
            g(184, 224, Kind::Reserve),
            t(224, 236, "IAT WS2_32.dll"),
            g(236, 276, Kind::Reserve),
            t(276, 316, "hint/name strings"),
            t(316, 329, "dll name KERNEL32.dll"),
            g(329, 330, Kind::Padding),
            t(330, 337, "hint/name strings"),
            g(337, 338, Kind::Padding),
            t(338, 349, "dll name WS2_32.dll"),
            (349, 350, Kind::Tail, "all zero".to_string()),
        ]
    );
    let a = anatomy(&img, &img.imports().unwrap());
    let idata = a.iter().find(|x| x.section == ".idata").unwrap();
    assert_eq!(idata.generated, 349);
    let lines = idata.lines();
    assert_eq!(
        lines[2],
        format!(
            "    {:#08x}..{:#08x}      16  ILT KERNEL32.dll",
            s.idata + 60,
            s.idata + 76
        )
    );
    assert!(lines.last().unwrap().contains("generates 349 of 350 bytes"));
    let rdata = a.iter().find(|x| x.section == ".rdata").unwrap();
    assert_eq!(rdata.parts[0].kind, Kind::Content);
    assert_eq!(rdata.parts[1].label, "export directory");
}

#[test]
fn a_small_gap_holding_bytes_is_content_not_padding() {
    let s = common::sample();
    let mut b = s.pe.build();
    let img = Image::parse(b.clone()).unwrap();
    let off = img
        .rva_to_off(msvc6_base::units::Rva(s.idata + 329))
        .unwrap()
        .get();
    b[off] = b'X';
    let img = Image::parse(b).unwrap();
    let got = idata_parts(&img);
    assert!(
        got.contains(&(329, 330, Kind::Content, String::new())),
        "{got:?}"
    );
}

#[test]
fn the_orders_follow_each_table() {
    let s = common::sample();
    let img = common::image(&s.pe);
    let t = img.imports().unwrap();
    let o = orders(&t);
    assert_eq!(o.descriptors, ["KERNEL32.dll", "WS2_32.dll"]);
    assert_eq!(o.names, o.descriptors);
    assert_eq!(o.ilt, o.descriptors);
    assert_eq!(o.iat, o.descriptors);
    let k = &t[0];
    let wrong: Vec<String> = ["GetVersion", "ExitProcess", "HeapAlloc"]
        .map(String::from)
        .to_vec();
    let m = member_order(k, &wrong);
    assert_eq!(
        (m.in_library, m.named, m.slot_order, m.name_order),
        (3, 3, false, false)
    );
    let right: Vec<String> = ["ExitProcess", "Other", "GetVersion", "HeapAlloc"]
        .map(String::from)
        .to_vec();
    let m = member_order(k, &right);
    assert!(m.slot_order && m.name_order);
}
