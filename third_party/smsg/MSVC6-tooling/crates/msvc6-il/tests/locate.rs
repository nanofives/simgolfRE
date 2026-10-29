#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::il as fx;
use msvc6_il::grammar;
use msvc6_image::pe::image::Image;

#[test]
fn the_grammar_tables_are_found_by_their_structure() {
    let fmt = fx::fmt_table(0xbc);
    let attr = fx::attr_table(0xbe);
    let bytes = fx::grammar_image(&fmt, &attr, &fx::pragma_codes());
    let g = grammar::locate(&Image::parse(bytes).unwrap()).unwrap();
    assert_eq!(g.fmt, fmt);
    assert_eq!(g.attr, attr);
    assert_eq!(g.pragma.len(), 49);
    assert_eq!(g.pragma[1].codes.as_deref(), Some(&b"l"[..]));
    assert_eq!(g.pragma[0x1f].codes.as_deref(), Some(&[0x14u8][..]));
    let f = g.found.unwrap();
    assert_eq!(f.fmt, f.attr + 2 * 0xbe);
    assert!(g.ends_statement(0x4b));
    assert!(!g.ends_statement(0x26));
}

#[test]
fn an_image_without_the_tables_or_with_broken_anchors_is_refused() {
    let none = fx::grammar_image(&[], &[], &[]);
    assert!(grammar::locate(&Image::parse(none).unwrap()).is_err());
    let mut fmt = fx::fmt_table(0xbc);
    fmt[0x33] = 1;
    let bytes = fx::grammar_image(&fmt, &fx::attr_table(0xbe), &fx::pragma_codes());
    let e = grammar::locate(&Image::parse(bytes).unwrap()).unwrap_err();
    assert!(e.to_string().contains("operand format table"), "{e}");
    let mut attr = fx::attr_table(0xbe);
    attr[0x4b] = 0;
    let bytes = fx::grammar_image(&fx::fmt_table(0xbc), &attr, &fx::pragma_codes());
    let e = grammar::locate(&Image::parse(bytes).unwrap()).unwrap_err();
    assert!(e.to_string().contains("attribute table"), "{e}");
}

#[test]
fn the_front_ends_writer_table_is_matched_by_cs_attribute_words() {
    let fmt = fx::fmt_table(0xbc);
    let attr = fx::attr_table(0xbe);
    let g = grammar::locate(
        &Image::parse(fx::grammar_image(&fmt, &attr, &fx::pragma_codes())).unwrap(),
    )
    .unwrap();
    let ops: Vec<(u16, Option<Vec<u8>>)> = attr
        .iter()
        .enumerate()
        .map(|(i, a)| (*a & 0x7fff, (i == 0x26).then(|| vec![0x08])))
        .collect();
    let mut syms = vec![vec![3u8]; 0x19];
    syms[1] = vec![3, 7, 5, 8, 9, 0x0d, 0x0b, 0x0c, 0x10, 1, 2, 6];
    syms[3] = vec![3, 7, 5, 8, 9, 0x0e, 0x0a];
    let w = grammar::writer(
        &Image::parse(fx::writer_image(&ops, &syms)).unwrap(),
        &g,
        &msvc6_il::sym::WRITER_ANCHORS,
    )
    .unwrap();
    assert_eq!(w.ops.len(), 0xbe);
    assert_eq!(w.ops[0x26].1.as_deref(), Some(&[8u8][..]));
    assert_eq!(
        grammar::writer_tokens(&[8]),
        grammar::fmt_tokens(g.fmt[0x26])
    );
    assert!(w.syms_at.is_some());
    assert_eq!(w.syms[1].len(), 12);
}
