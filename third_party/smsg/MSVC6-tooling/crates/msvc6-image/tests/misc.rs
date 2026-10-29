#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_base::units::Rva;
use msvc6_image::cmp::{Delta, baseline_verdict, percent, section_costs, summary};
use msvc6_image::digest::{Function, code_digest, feed, first_disagreement};
use msvc6_image::dump::{Where, final_zero_run, hex_lines, locate, map_textbss, sections, textbss};
use msvc6_image::pe::{Image, ResId};
use msvc6_image::res::{res_file, type_name};

fn u32_at(b: &[u8], o: usize) -> usize {
    usize::try_from(u32::from_le_bytes(b[o..o + 4].try_into().unwrap())).unwrap()
}

#[test]
fn a_res_file_holds_a_null_entry_then_every_resource() {
    let s = common::sample();
    let img = common::image(&s.pe);
    let (blob, items) = res_file(&img).unwrap().unwrap();
    assert_eq!(items.len(), 3);
    assert_eq!(
        &blob[..16],
        &[0, 0, 0, 0, 32, 0, 0, 0, 0xff, 0xff, 0, 0, 0xff, 0xff, 0, 0]
    );
    let mut at = 32;
    let mut found = Vec::new();
    while at < blob.len() {
        let (data, header) = (u32_at(&blob, at), u32_at(&blob, at + 4));
        found.push(blob[at + header..at + header + data].to_vec());
        at = (at + header + data).div_ceil(4) * 4;
    }
    assert_eq!(found.len(), 3);
    assert!(found.contains(&b"RIFF....".to_vec()));
    let wave = blob
        .windows(8)
        .position(|w| w == [b'W', 0, b'A', 0, b'V', 0, b'E', 0]);
    assert!(wave.is_some());
    assert_eq!(type_name(&ResId::Ordinal(14)), "GROUP_ICON");
    assert_eq!(type_name(&ResId::Ordinal(99)), "99");
    let mut pe = msvc6_fixture::pe::Pe::exe();
    pe.push(".text", msvc6_fixture::pe::SCN_CODE, vec![0xc3]);
    assert!(res_file(&common::image(&pe)).unwrap().is_none());
}

#[test]
fn the_code_digest_masks_relocated_fields_and_ignores_order() {
    let f = |name: &str, bytes: &[u8], relocs: &[u32]| Function {
        name: name.into(),
        bytes: bytes.to_vec(),
        relocs: relocs.to_vec(),
    };
    let (mut x, mut y) = (Vec::new(), Vec::new());
    feed(&mut x, "f", &[0xe8, 1, 2, 3, 4, 0x90], &[1]);
    feed(&mut y, "f", &[0xe8, 9, 9, 9, 9, 0x90], &[1]);
    assert_eq!(x, y);
    let mut z = Vec::new();
    feed(&mut z, "f", &[0xe8, 1, 2, 3, 4, 0x91], &[1]);
    assert_ne!(x, z);
    let mut far = Vec::new();
    feed(&mut far, "f", &[0x55, 0x8b], &[99]);
    assert_eq!(far.len(), 2 + 4 + 2);
    let a = [f("b", &[1, 2], &[]), f("a", &[0xe8, 0, 0, 0, 0], &[1])];
    let b = [f("a", &[0xe8, 7, 7, 7, 7], &[1]), f("b", &[1, 2], &[])];
    assert_eq!(code_digest(&a), code_digest(&b));
    assert_eq!(code_digest(&a).unwrap().functions, 2);
    assert!(code_digest(&[]).is_none());
    assert_eq!(first_disagreement(&a, &b), None);
    let c = [f("a", &[0xe8, 7, 7, 7, 7], &[1]), f("b", &[1, 3], &[])];
    assert_eq!(first_disagreement(&a, &c), Some("b"));
    assert_eq!(first_disagreement(&a, &c[..1]), Some("b"));
}

#[test]
fn an_address_is_placed_in_headers_a_section_its_tail_or_nowhere() {
    let s = common::sample();
    let img = common::image(&s.pe);
    assert_eq!(locate(&img, Rva(0x40)), Where::Headers { file_off: 0x40 });
    let data = img.section(".data").unwrap().clone();
    match locate(&img, Rva(s.data + 0x10)) {
        Where::Section {
            name,
            offset,
            file_off,
            ..
        } => {
            assert_eq!((name.as_str(), offset), (".data", 0x10));
            assert_eq!(file_off, Some(data.raw_off.get() + 0x10));
        }
        w => panic!("{w:?}"),
    }
    match locate(&img, Rva(s.data + 0x2000)) {
        Where::Section {
            file_off, disk_end, ..
        } => {
            assert_eq!(file_off, None);
            assert_eq!(disk_end, s.data + 0x200);
        }
        w => panic!("{w:?}"),
    }
    assert_eq!(locate(&img, Rva(0x7000_0000)), Where::Outside);
    let rows = sections(&img);
    let d = rows.iter().find(|r| r.name == ".data").unwrap();
    assert_eq!(
        (d.tail, d.tail_from),
        (0x3000 - 0x200, Some(s.data + 0x200))
    );
    let t = rows.iter().find(|r| r.name == ".text").unwrap();
    assert_eq!((t.tail, t.tail_from), (0, None));
    let lines = hex_lines(0x1000, &[0x41; 20]);
    assert_eq!(lines.len(), 2);
    assert!(lines[0].starts_with("  0x00001000  41 41"));
    assert!(lines[0].ends_with("|AAAAAAAAAAAAAAAA|"));
}

#[test]
fn the_final_zero_run_bounds_textbss_and_a_map_pins_it() {
    let s = common::sample();
    let img = common::image(&s.pe);
    let t = textbss(&img, ".data").unwrap();
    assert_eq!(t.run, 0x3000 - 0x80);
    assert_eq!(t.run_from, s.data + 0x80);
    assert_eq!(final_zero_run(&[0xcc, 0, 0]), 2);
    assert!(textbss(&img, ".nothing").is_none());
    let map = " 0001:00000000 0004193fH .text                   CODE\r\n \
               0001:0004193f 0001ba01H .textbss                CODE\r\n \
               0001:00041940       __enc$textbss$begin        10042940     <linker-defined>\r\n \
               0001:0005d340       __enc$textbss$end          1005e340     <linker-defined>\r\n";
    let m = map_textbss(map, "0001").unwrap();
    assert_eq!(
        (m.begin, m.end, m.row_len),
        (0x41940, 0x5d340, Some(0x1ba01))
    );
    assert!(map_textbss(map, "0002").is_none());
}

#[test]
fn section_costs_are_read_at_the_same_section_offset() {
    let s = common::sample();
    let reference = s.pe.build();
    let mut c = common::sample();
    c.pe.sections[0].data[0] = 0xcc;
    c.pe.sections[2].data.push(9);
    c.pe.sections.pop();
    let candidate = c.pe.build();
    let (ri, ci) = (
        Image::parse(reference.clone()).unwrap(),
        Image::parse(candidate.clone()).unwrap(),
    );
    let costs = section_costs(&ri, &ci);
    assert_eq!(costs.len(), 6);
    assert_eq!(costs[0].differing, Some(1));
    assert_eq!(costs[2].ours, Some(0x200));
    assert_eq!(costs[2].differing, Some(1));
    assert_eq!((costs[5].ours, costs[5].differing), (None, None));
    let sm = summary(&reference, &candidate, &costs);
    assert!(!sm.md5_identical);
    assert_eq!(
        sm.sections_sum,
        costs.iter().filter_map(|c| c.differing).sum::<usize>()
    );
    let same = summary(&reference, &reference, &section_costs(&ri, &ri));
    assert!(same.md5_identical && same.differing == 0 && same.sections_sum == 0);
    let v = serde_json::to_value(&costs[5]).unwrap();
    assert!(v.get("ours").is_none() && v.get("differing").is_none());
}

#[test]
fn a_delta_inside_the_clock_band_is_not_a_result() {
    for d in [-12, -1, 0, 8, 12] {
        assert_eq!(baseline_verdict(d), Delta::ClockBand, "{d}");
    }
    assert_eq!(baseline_verdict(-13), Delta::Better);
    assert_eq!(baseline_verdict(13), Delta::Worse);
    assert!((percent(1, 4) - 25.0).abs() < 1e-9);
    assert!((percent(0, 0)).abs() < 1e-9);
}
