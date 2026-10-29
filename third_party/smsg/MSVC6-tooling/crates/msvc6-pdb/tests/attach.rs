#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::digest::md5_hex;
use msvc6_image::pe::Image;
use msvc6_pdb::attach::{Shipped, attach};
use msvc6_pdb::{info, to70};

fn shipped(image: &[u8]) -> Vec<Shipped> {
    vec![Shipped {
        md5: md5_hex(image),
        what: "the sample DLL".into(),
        file: None,
    }]
}

#[test]
fn a_2_0_pdb_the_image_already_names_needs_no_change() {
    let s = msvc6_fixture::pdb::sample();
    let a = attach(&s.image, &shipped(&s.image), &s.bytes, "sample.pdb").unwrap();
    assert!(a.changes.is_empty());
    assert_eq!(a.bytes, s.image);
    assert!(a.recipe().contains("Nothing to change"));
}

#[test]
fn a_7_0_twin_gets_an_rsds_record_in_the_rdata_slack_and_nothing_else_moves() {
    let s = msvc6_fixture::pdb::sample();
    let twin = to70::convert(&s.bytes).unwrap().bytes;
    let a = attach(&s.image, &shipped(&s.image), &twin, "sample.pdb").unwrap();
    assert_eq!(a.changes.len(), 4, "{:?}", a.changes);
    assert_eq!(a.bytes.len(), s.image.len());
    let touched = |i: usize| {
        a.changes
            .iter()
            .any(|c| i >= c.at && i < c.at + c.new.len())
    };
    assert!((0..a.bytes.len()).all(|i| touched(i) || a.bytes[i] == s.image[i]));
    let img = Image::parse(a.bytes.clone()).unwrap();
    let cv = img.codeview().unwrap().unwrap();
    assert_eq!(cv.format, "RSDS");
    assert_eq!(cv.age, msvc6_fixture::pdb::AGE);
    assert_eq!(cv.path, "sample.pdb");
    let g = info::guid_of(&s.bytes);
    assert_eq!(
        cv.guid.unwrap(),
        g.iter().map(|b| format!("{b:02x}")).collect::<String>()
    );
    assert_eq!(
        img.debug_entries().len(),
        1,
        "the one CodeView entry, re-pointed"
    );
    assert_eq!(a.md5_out, md5_hex(&a.bytes));
    let r = a.recipe();
    assert!(r.contains("RSDS record (in the slack)") && r.contains(&a.md5_out));
    assert!(attach(&a.bytes, &shipped(&s.image), &twin, "sample.pdb").is_err());
}

#[test]
fn an_image_with_no_debug_directory_gets_one() {
    let s = msvc6_fixture::pdb::sample();
    let mut dll = msvc6_fixture::sample::sample();
    dll.pe.dirs[6] = (0, 0);
    let image = dll.pe.build();
    let a = attach(&image, &shipped(&image), &s.bytes, "sample.pdb").unwrap();
    let img = Image::parse(a.bytes.clone()).unwrap();
    let cv = img.codeview().unwrap().unwrap();
    assert_eq!(
        (cv.format.as_str(), cv.signature, cv.age),
        (
            "NB10",
            msvc6_fixture::pdb::SIGNATURE,
            msvc6_fixture::pdb::AGE
        )
    );
    assert_eq!(img.debug_entries()[0].timestamp, img.timestamp);
}

#[test]
fn a_copy_that_is_not_shipped_or_a_pdb_for_another_image_is_refused() {
    let s = msvc6_fixture::pdb::sample();
    let mut other = s.image.clone();
    let last = other.len() - 1;
    other[last] ^= 1;
    let e = attach(&other, &shipped(&s.image), &s.bytes, "sample.pdb").unwrap_err();
    let msg = format!("{e:#}");
    assert!(msg.contains("is not a shipped image"), "{msg}");
    assert!(msg.contains(&md5_hex(&s.image)), "{msg}");
    assert!(attach(&s.image, &[], &s.bytes, "sample.pdb").is_err());
    let mut dll = msvc6_fixture::sample::sample();
    dll.pe.sections[0].data.push(0x90);
    let moved = dll.pe.build();
    let e = attach(&moved, &shipped(&moved), &s.bytes, "sample.pdb").unwrap_err();
    assert!(format!("{e:#}").contains("section-headers stream"), "{e:#}");
}

#[test]
fn a_pdb_whose_identity_disagrees_with_the_record_is_refused() {
    let s = msvc6_fixture::pdb::sample();
    let sections: Vec<[u8; 40]> = s.sections.as_chunks::<40>().0.to_vec();
    let other = msvc6_pdb::write::build(&msvc6_pdb::write::Spec {
        signature: 1,
        age: 1,
        sections,
        ..Default::default()
    })
    .unwrap()
    .bytes;
    let e = attach(&s.image, &shipped(&s.image), &other, "x.pdb").unwrap_err();
    assert!(format!("{e:#}").contains("not this image's PDB"), "{e:#}");
}
