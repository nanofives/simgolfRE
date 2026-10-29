#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_fixture::archive::Library;
use msvc6_fixture::coff::Object as Obj;
use msvc6_fixture::pe::{Pe, SCN_CODE, comp_id, rich_block};
use msvc6_image::archive::Archive;
use msvc6_image::rich::{
    self, Stamp, archive_builds, census_rows, deciding_archives, prodid_name, prodid_of, witnesses,
};

fn header(key: u32, pad: usize, stamps: &[(u32, u32)]) -> Vec<u8> {
    let mut f = vec![0u8; 0x10];
    f.extend_from_slice(&(rich::DANS ^ key).to_le_bytes());
    for _ in 0..pad {
        f.extend_from_slice(&key.to_le_bytes());
    }
    for (cid, cnt) in stamps {
        f.extend_from_slice(&(cid ^ key).to_le_bytes());
        f.extend_from_slice(&(cnt ^ key).to_le_bytes());
    }
    f.extend_from_slice(b"Rich");
    f.extend_from_slice(&key.to_le_bytes());
    f
}

#[test]
fn a_linked_header_decodes_and_its_key_is_the_checksum() {
    let s = common::sample();
    let img = s.pe.build();
    let h = rich::decode(&img).unwrap();
    assert_eq!(h.dans, 0x80);
    assert!(h.key_is_checksum(), "{h:?}");
    assert_eq!(h.stamps.len(), 4);
    assert_eq!(
        h.stamps[0],
        Stamp {
            prodid: 0x0b,
            build: 8168,
            objects: 12
        }
    );
    assert_eq!(h.objects(0x0a), 40);
    assert_eq!(h.end, 0x80 + 16 + 32 + 8);
    assert_eq!(h.classes().len(), 4);
}

#[test]
fn a_key_that_is_not_the_checksum_is_reported() {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    pe.rich = vec![(comp_id(0x0a, 8047), 210)];
    pe.rich_key = Some(0x1234_5678);
    let h = rich::decode(&pe.build()).unwrap();
    assert_eq!(h.key, 0x1234_5678);
    assert!(!h.key_is_checksum());
    assert_eq!(h.stamps[0].build, 8047);
    assert_eq!(rich_block(0, &[(1, 2)]).len(), 32);
}

#[test]
fn a_phase_error_an_absurd_count_and_an_empty_header_are_rejected() {
    let key = 0x1234_5678;
    assert!(rich::decode(&header(key, 3, &[((0x0a << 16) | 8168, 77)])).is_some());
    assert!(rich::decode(&header(key, 2, &[((0x0a << 16) | 8168, 77)])).is_none());
    assert!(rich::decode(&header(1, 3, &[((0x0a << 16) | 8168, 800_000)])).is_none());
    assert!(rich::decode(&header(0x0f0f_0f0f, 3, &[])).is_none());
    assert!(rich::decode(b"no header here").is_none());
}

#[test]
fn census_rows_are_the_union_of_both_headers() {
    let cpp = (0x0b << 16) | 8168;
    let c = (0x0a << 16) | 8168;
    let masm = (0x0e << 16) | 7299;
    let r = rich::decode(&header(0x1234_5678, 3, &[(cpp, 12), (c, 4)])).unwrap();
    let o = rich::decode(&header(0x0bad_f00d, 3, &[(cpp, 14), (masm, 2)])).unwrap();
    let rows = census_rows(Some(&r), Some(Some(&o)));
    let got: Vec<(u32, u32, Option<u32>)> = rows
        .iter()
        .map(|x| (x.prodid, x.reference, x.candidate))
        .collect();
    assert_eq!(
        got,
        vec![(0x0a, 4, Some(0)), (0x0b, 12, Some(14)), (0x0e, 0, Some(2))]
    );
    let alone = census_rows(Some(&r), None);
    assert!(alone.iter().all(|x| x.candidate.is_none()));
    let unknown = census_rows(Some(&r), Some(None));
    assert!(unknown.iter().all(|x| x.candidate == Some(0)));
    let v = serde_json::to_value(&alone[0]).unwrap();
    assert!(v.get("candidate").is_none());
    assert_eq!(v["product"], "Utc12_C");
}

fn lib(builds: &[u16]) -> Vec<u8> {
    let mut l = Library::default();
    for (i, b) in builds.iter().enumerate() {
        let mut o = Obj::new();
        o.code(".text", &[0xc3]);
        o.comp_id(0x0a, *b);
        l.member(&format!("m{i}.obj"), o.build(), &[]);
    }
    l.build()
}

#[test]
fn only_a_build_one_archive_alone_stamps_decides_the_copy() {
    let gold = lib(&[8168, 8168, 7299]);
    let sp5 = lib(&[8047, 7299]);
    let (ga, sa) = (
        Archive::parse(&gold).unwrap(),
        Archive::parse(&sp5).unwrap(),
    );
    let gb = archive_builds(&ga);
    assert_eq!(gb.get(&8168), Some(&2));
    let image = rich::decode(&header(
        7,
        3,
        &[((0x0a << 16) | 8168, 9), ((0x0e << 16) | 7299, 3)],
    ))
    .unwrap();
    let per = vec![
        ("gold".to_string(), gb),
        ("sp5".to_string(), archive_builds(&sa)),
    ];
    let w = witnesses(&image, &per);
    let shared = w
        .iter()
        .find(|x| x.build == 7299 && x.archive == "gold")
        .unwrap();
    assert!(!shared.unique && shared.in_image);
    assert_eq!(deciding_archives(&w), ["gold"]);
    let alone = witnesses(&image, &per[..1]);
    assert!(deciding_archives(&alone).is_empty());
}

#[test]
fn products_are_named_both_ways() {
    assert_eq!(prodid_name(0x0b), "Utc12_CPP");
    assert_eq!(prodid_name(0x99), "?");
    assert_eq!(prodid_of("utc12_cpp"), Some(0x0b));
    assert_eq!(prodid_of("nothing"), None);
}
