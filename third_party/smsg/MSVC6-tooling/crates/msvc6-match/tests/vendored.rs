#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::*;
use msvc6_match::closure;
use msvc6_match::vendored::{blocks, locate_object, place_code, trailing_padding};
use std::collections::BTreeSet;

#[test]
fn vendored_objects_place_by_their_bytes_and_absent_bodies_are_named() {
    let img = parsed(linked());
    let t = msvc6_match::text_of(&img).unwrap();
    let la = locate_object("a.obj", &object(&[a()]), &t).unwrap();
    assert_eq!(
        (la.unique, la.lo, la.hi),
        (1, Some(AT_A), Some(AT_A + 0x20))
    );
    let lc = locate_object("c.obj", &object(&[c()]), &t).unwrap();
    assert_eq!(lc.lo, Some(AT_C));
    let lg = locate_object("gone.obj", &object(&[gone()]), &t).unwrap();
    assert_eq!((lg.unique, lg.absent), (0, 1));
    assert_eq!(lg.sections[0].verdict, "absent");
    let b = blocks(&[la, lc, lg], 16);
    assert_eq!(b.len(), 1);
    assert_eq!((b[0].lo, b[0].hi), (AT_A, AT_C + 0x20));
    assert!(b[0].in_order);
}

#[test]
fn a_trailing_nop_pad_the_image_fills_otherwise_is_masked() {
    let mut text = filler(TEXT_LEN);
    let mut code = vec![0x8b, 0x44, 0x24, 0x04, 0x03, 0x44, 0x24, 0x08, 0xc3];
    text[0x10..0x10 + code.len()].copy_from_slice(&code);
    let img = parsed(text);
    let t = msvc6_match::text_of(&img).unwrap();
    code.extend_from_slice(&[0x90; 7]);
    let obj = object(&[body("_add", &code, &[])]);
    let l = locate_object("add.obj", &obj, &t).unwrap();
    assert_eq!(l.sections[0].verdict, "unique");
    assert_eq!(l.sections[0].pad_masked, 7);
    assert_eq!(l.lo, Some(TEXT + 0x10));
    let o = msvc6_image::coff::Object::parse(&obj).unwrap();
    let c = &msvc6_match::code::sections(&o).unwrap()[0];
    assert_eq!(trailing_padding(c), 7);
    assert_eq!(place_code(c, t.bytes).1, 7);
}

#[test]
fn objects_out_of_their_address_order_are_flagged() {
    let img = parsed(linked());
    let t = msvc6_match::text_of(&img).unwrap();
    let lc = locate_object("c.obj", &object(&[c()]), &t).unwrap();
    let la = locate_object("a.obj", &object(&[a()]), &t).unwrap();
    let b = blocks(&[lc, la], 16);
    assert_eq!(b.len(), 1);
    assert!(!b[0].in_order);
}

fn call(from: u32, to: u32) -> Vec<u8> {
    let mut v = vec![0xe8];
    v.extend_from_slice(&to.wrapping_sub(from + 5).to_le_bytes());
    v
}

#[test]
fn the_closure_counts_undefined_direct_callees_through_one_thunk() {
    let base = 0x40_0000u32;
    let mut text = vec![0xcc; 0x100];
    let (thunk, f0, f1, f2) = (0x1000u32, 0x1010u32, 0x1040u32, 0x1080u32);
    let mut jmp = vec![0xe9];
    jmp.extend_from_slice(&f2.wrapping_sub(thunk + 5).to_le_bytes());
    text[0..5].copy_from_slice(&jmp);
    let mut b0 = call(f0, f1);
    b0.extend(call(f0 + 5, thunk));
    b0.extend(call(f0 + 10, 0x9000));
    b0.extend_from_slice(&[0xff, 0x50, 0x08, 0xc3]);
    text[0x10..0x10 + b0.len()].copy_from_slice(&b0);
    let b1 = [call(f1, thunk), vec![0xc3]].concat();
    text[0x40..0x40 + b1.len()].copy_from_slice(&b1);
    text[0x80] = 0xc3;
    let t = closure::Text {
        bytes: &text,
        rva: 0x1000,
        image_base: base,
    };
    let extents = [(f0, 0x30), (f1, 0x40), (f2, 0x10)];
    let rows = closure::closure(&t, &extents, &BTreeSet::new());
    let r0 = rows.iter().find(|r| r.rva == f0).unwrap();
    assert_eq!(r0.blockers, [f1, f2].into_iter().collect());
    assert_eq!((r0.foreign, r0.indirect), (1, 1));
    let defined: BTreeSet<u32> = [f2].into_iter().collect();
    let rows = closure::closure(&t, &extents, &defined);
    assert_eq!(rows.len(), 2);
    let r1 = rows.iter().find(|r| r.rva == f1).unwrap();
    assert!(r1.blockers.is_empty());
    assert_eq!(rows[0].rva, f1);
    let h = closure::histogram(&rows);
    assert_eq!(h.get(&0), Some(&1));
    assert_eq!(h.get(&1), Some(&1));
}

#[test]
fn an_e8_byte_inside_an_operand_is_not_a_call() {
    let base = 0x40_0000u32;
    let mut text = vec![0xcc; 0x200];
    let (f3, f1) = (0x1000u32, 0x10c0u32);
    let body = [
        0xb8, 0x90, 0x90, 0x90, 0xe8, 0xb8, 0x00, 0x00, 0x00, 0x00, 0xc3,
    ];
    text[..body.len()].copy_from_slice(&body);
    text[0xc0] = 0xc3;
    let e8 = body.iter().position(|b| *b == 0xe8).unwrap();
    let rel = u32::from_le_bytes(body[e8 + 1..e8 + 5].try_into().unwrap());
    let naive = f3 + u32::try_from(e8).unwrap() + 5 + rel;
    assert!((f1..f1 + 0x10).contains(&naive), "{naive:#x}");
    let t = closure::Text {
        bytes: &text,
        rva: 0x1000,
        image_base: base,
    };
    let extents = [(f3, 0x20), (f1, 0x10)];
    let rows = closure::closure(&t, &extents, &BTreeSet::new());
    let r3 = rows.iter().find(|r| r.rva == f3).unwrap();
    assert!(r3.blockers.is_empty(), "{:?}", r3.blockers);
    assert_eq!(r3.foreign, 0);
}
