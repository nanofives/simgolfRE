#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use msvc6_image::postlink::{Anchor, Kind, Row, State, resolve, step, totals};

struct Case {
    reference: Vec<u8>,
    ours: Vec<u8>,
    rows: Vec<Row>,
}

fn row(kind: Kind, off: usize, len: usize, at: Option<Anchor>) -> Row {
    Row {
        kind,
        off: Some(off),
        rva: None,
        len,
        at,
        fill: None,
        note: String::new(),
    }
}

fn case() -> Case {
    let p = common::postlinked();
    let img = Image::parse(p.ours.clone()).unwrap();
    assert_eq!(p.slack, img.header_structs_end() + 8);
    assert_eq!(
        p.pad,
        img.rva_to_off(Rva(p.sample.text + 0x10)).unwrap().get()
    );
    assert_eq!(p.size_of_code, img.opt_off + 4);
    let rd = img.section(".rdata").unwrap();
    assert_eq!(
        p.tail,
        rd.raw_off.get() + usize::try_from(rd.virt_size.get()).unwrap()
    );
    Case {
        reference: p.reference,
        ours: p.ours,
        rows: vec![
            row(Kind::HeaderSlack, p.slack, 8, None),
            row(Kind::TextPad, p.pad, 6, None),
            row(
                Kind::HeaderField,
                p.size_of_code,
                4,
                Some(Anchor::SizeOfCode),
            ),
            row(Kind::ImportStamps, p.stamps, 8, Some(Anchor::ImportStamps)),
            row(Kind::HeaderField, p.tail, 8, Some(Anchor::SectionTail)),
        ],
    }
}

#[test]
fn on_the_reference_itself_every_row_matches_in_place() {
    let c = case();
    let r = Image::parse(c.reference).unwrap();
    let sites = resolve(&c.rows, &r, &r).unwrap();
    for s in &sites {
        assert_eq!(s.state, State::Matches, "{s:?}");
        assert!(s.in_place(), "{s:?}");
    }
    assert_eq!(totals(&sites).owed, 0);
}

#[test]
fn a_link_shaped_image_becomes_the_reference_byte_for_byte() {
    let c = case();
    let r = Image::parse(c.reference.clone()).unwrap();
    let o = Image::parse(c.ours.clone()).unwrap();
    let sites = resolve(&c.rows, &r, &o).unwrap();
    assert!(
        sites.iter().all(|s| s.state == State::Writable),
        "{sites:?}"
    );
    let tail = sites.last().unwrap();
    assert!(tail.extra.is_some());
    assert_eq!(totals(&sites).owed, 5 + 6 + 1 + 8 + 7);
    let (out, stepped) = step(&c.rows, &r, c.ours).unwrap();
    assert_eq!(out, c.reference);
    assert_eq!(stepped.rows_written, 5);
    assert!(stepped.rows.iter().all(|s| s.state == State::Matches));
    let (again, second) = step(&c.rows, &r, out).unwrap();
    assert_eq!(again, c.reference);
    assert_eq!(second.written, 0);
}

#[test]
fn a_pad_holding_code_or_following_other_code_is_blocked() {
    let c = case();
    let r = Image::parse(c.reference.clone()).unwrap();
    let pad = c.rows[1].off.unwrap();
    let mut code = c.ours.clone();
    code[pad + 2] = 0x8b;
    let sites = resolve(&c.rows, &r, &Image::parse(code.clone()).unwrap()).unwrap();
    assert!(
        matches!(sites[1].state, State::Blocked(_)),
        "{:?}",
        sites[1]
    );
    assert_eq!(totals(&sites).blocked_rows, 1);
    let (out, s) = step(&c.rows, &r, code).unwrap();
    assert_eq!(out[pad + 2], 0x8b);
    assert!(s.rows[1].blocked());
    let mut moved = c.ours.clone();
    moved[pad - 3] = 0xcc;
    let sites = resolve(&c.rows, &r, &Image::parse(moved).unwrap()).unwrap();
    let State::Blocked(why) = &sites[1].state else {
        panic!("{:?}", sites[1]);
    };
    assert!(why.contains("bytes before the row"), "{why}");
}

#[test]
fn a_row_that_is_not_its_declared_field_is_a_manifest_defect() {
    let c = case();
    let r = Image::parse(c.reference.clone()).unwrap();
    let o = Image::parse(c.ours.clone()).unwrap();
    let mut shifted = c.rows.clone();
    shifted[2].off = shifted[2].off.map(|x| x + 4);
    assert!(resolve(&shifted, &r, &o).is_err());
    let mut wrong_len = c.rows.clone();
    wrong_len[3].len = 4;
    assert!(resolve(&wrong_len, &r, &o).is_err());
    let positional_with_anchor = vec![row(
        Kind::TextPad,
        c.rows[1].off.unwrap(),
        6,
        Some(Anchor::SizeOfCode),
    )];
    assert!(resolve(&positional_with_anchor, &r, &o).is_err());
    let anchored_without = vec![row(Kind::HeaderField, 0x100, 4, None)];
    assert!(resolve(&anchored_without, &r, &o).is_err());
    let outside_slack = vec![row(Kind::HeaderSlack, 0x10, 4, None)];
    assert!(resolve(&outside_slack, &r, &o).is_err());
    let data = r.section(".data").unwrap().raw_off.get();
    let pad_in_data = vec![row(Kind::TextPad, data, 4, None)];
    assert!(resolve(&pad_in_data, &r, &o).is_err());
    let mut both = row(Kind::HeaderSlack, 0x300, 4, None);
    both.rva = Some(0x300);
    assert!(resolve(&[both], &r, &o).is_err());
}

#[test]
fn a_row_is_read_from_toml_by_kind_and_anchor_name() {
    #[derive(Debug, serde::Deserialize)]
    struct F {
        residue: Vec<Row>,
    }
    let f: F = toml::from_str(
        "[[residue]]\nkind = \"text-pad\"\nrva = 0x1010\nlen = 6\nnote = \"stub\"\n\
         [[residue]]\nkind = \"header-field\"\noff = 0x10c\nlen = 4\nat = \"size-of-code\"\n",
    )
    .unwrap();
    assert_eq!(f.residue[0].kind, Kind::TextPad);
    assert_eq!(f.residue[0].rva, Some(0x1010));
    assert_eq!(f.residue[1].at, Some(Anchor::SizeOfCode));
    assert!(toml::from_str::<F>("[[residue]]\nkind = \"text_pad\"\nlen = 1\n").is_err());
    assert!(toml::from_str::<F>("[[residue]]\nkind = \"text-pad\"\nlen = 1\nvalue = 3\n").is_err());
    let c = case();
    let r = Image::parse(c.reference.clone()).unwrap();
    let by_rva = Row {
        rva: Some(common::sample().text + 0x10),
        off: None,
        ..c.rows[1].clone()
    };
    let s = resolve(&[by_rva], &r, &r).unwrap();
    assert_eq!(s[0].off, c.rows[1].off.unwrap());
}
