#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::{EXPORT_TIMESTAMP, PDB_AGE, PDB_SIGNATURE, TIMESTAMP, image, sample};
use msvc6_base::units::Rva;
use msvc6_fixture::pe::{DIR_EXPORT, Pe, SCN_CODE};
use msvc6_image::clock::{self, ALL, Field, MAX_BYTES, apply, compare, render, resolve};
use msvc6_image::pe::Image;

fn relinked() -> Pe {
    let mut s = sample();
    s.pe.timestamp = 0x3b9a_ffff;
    s.pe
}

#[test]
fn every_field_is_located_by_walking_the_files_own_headers() {
    let s = sample();
    let img = image(&s.pe);
    let sites = resolve(&img, &ALL).unwrap();
    assert_eq!(sites.len(), 5);
    let at = |f: Field| sites.iter().find(|x| x.field == f).unwrap().off;
    assert_eq!(at(Field::CoffTimestamp), img.e_lfanew + 8);
    let read = |o: usize| u32::from_le_bytes(img.bytes()[o..o + 4].try_into().unwrap());
    assert_eq!(read(at(Field::CoffTimestamp)), TIMESTAMP);
    assert_eq!(read(at(Field::DebugTimestamp)), EXPORT_TIMESTAMP);
    assert_eq!(read(at(Field::PdbSignature)), PDB_SIGNATURE);
    assert_eq!(read(at(Field::PdbAge)), PDB_AGE);
    assert_eq!(read(at(Field::ExportTimestamp)), EXPORT_TIMESTAMP);
    assert_eq!(
        at(Field::ExportTimestamp),
        img.rva_to_off(Rva(s.edata)).unwrap().get() + 4
    );
}

#[test]
fn a_copy_writes_only_the_declared_fields_and_is_idempotent() {
    let reference = image(&sample().pe);
    let ours = image(&relinked());
    let r = compare(&ours, &reference, &[Field::CoffTimestamp, Field::PdbAge]).unwrap();
    assert!(!r[0].identical() && r[1].identical());
    assert!(!render(&r).1);
    let (bytes, applied) =
        apply(&ours, &reference, &[Field::CoffTimestamp, Field::PdbAge]).unwrap();
    assert_eq!(applied.bytes, 4);
    assert_eq!(bytes, reference.bytes());
    let again = Image::parse(bytes).unwrap();
    let (_, twice) = apply(&again, &reference, &[Field::CoffTimestamp]).unwrap();
    assert_eq!(twice.bytes, 0);
    let (lines, all) = render(&compare(&again, &reference, &ALL).unwrap());
    assert!(all);
    assert!(lines.last().unwrap().contains("5 of 5"));
}

#[test]
fn a_duplicate_or_an_oversized_request_is_refused() {
    let img = image(&sample().pe);
    assert!(resolve(&img, &[Field::PdbAge, Field::PdbAge]).is_err());
    assert!(clock::check_request(&ALL).is_ok());
    let six = [ALL.as_slice(), &[Field::CoffTimestamp]].concat();
    assert!(clock::check_request(&six).is_err());
    assert_eq!(MAX_BYTES, 20);
}

#[test]
fn an_image_that_exports_nothing_skips_the_field_and_a_short_directory_refuses() {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3; 64]);
    let none = image(&pe);
    let sites = resolve(&none, &[Field::CoffTimestamp, Field::ExportTimestamp]).unwrap();
    assert_eq!(sites.len(), 1);
    let r = compare(&none, &image(&sample().pe), &[Field::ExportTimestamp]).unwrap();
    assert_eq!(r[0].ours, None);
    assert!(!r[0].identical());
    pe.dir(DIR_EXPORT, 0x1000, 8);
    assert!(resolve(&image(&pe), &[Field::ExportTimestamp]).is_err());
    assert!(resolve(&none, &[Field::PdbAge]).is_err());
}

#[test]
fn fields_are_named_in_the_config_by_key() {
    assert_eq!(Field::parse("pdb.age").unwrap(), Field::PdbAge);
    assert!(Field::parse("text").is_err());
    #[derive(Debug, serde::Deserialize)]
    struct C {
        clock: Vec<Field>,
    }
    let c: C = toml::from_str("clock = [\"coff.timestamp\", \"export.timestamp\"]").unwrap();
    assert_eq!(c.clock, [Field::CoffTimestamp, Field::ExportTimestamp]);
    assert!(toml::from_str::<C>("clock = [\"coff_timestamp\"]").is_err());
    for f in ALL {
        assert_eq!(Field::parse(f.key()).unwrap(), f);
    }
}
