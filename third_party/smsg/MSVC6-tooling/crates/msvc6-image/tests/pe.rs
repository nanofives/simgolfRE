#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::{EXPORT_TIMESTAMP, PDB_AGE, PDB_PATH, PDB_SIGNATURE, TIMESTAMP, image, sample};
use msvc6_base::units::{FileOffset, Rva};
use msvc6_fixture::pe::{DIR_DEBUG, DIR_RESOURCE, Pe, SCN_CODE, SCN_RDATA};
use msvc6_fixture::tables::{
    DebugEntry, Id, Resource, codeview_nb10, codeview_rsds, debug_directory, rsrc,
};
use msvc6_image::pe::{Dir, Image, ResId};

#[test]
fn every_header_field_reads_back_as_written() {
    let s = sample();
    let img = image(&s.pe);
    let l = s.pe.layout();
    assert_eq!(img.machine, 0x14c);
    assert_eq!(img.timestamp, TIMESTAMP);
    assert_eq!(img.characteristics, 0x210e);
    assert!(img.is_dll());
    assert_eq!(img.e_lfanew, usize::try_from(l.e_lfanew).unwrap());
    assert_eq!(img.opt_size, 0xe0);
    let o = &img.opt;
    assert_eq!(o.magic, 0x10b);
    assert_eq!((o.linker_major, o.linker_minor), (6, 0));
    assert_eq!(o.image_base.get(), 0x1000_0000);
    assert_eq!(o.entry_point, Rva(s.text));
    assert_eq!(o.base_of_code, Rva(s.text));
    assert_eq!(o.base_of_data, Rva(s.rdata));
    assert_eq!(o.section_alignment, 0x1000);
    assert_eq!(o.file_alignment, 0x200);
    assert_eq!(o.size_of_image, l.size_of_image);
    assert_eq!(o.size_of_headers, l.size_of_headers);
    assert_eq!(o.size_of_code, 0x200);
    assert_eq!(o.size_of_uninit_data, 0);
    assert_eq!(o.number_of_dirs, 16);
    assert_eq!(o.subsystem, 2);
    assert_eq!(img.dirs.len(), 16);
    assert_eq!(img.len(), s.pe.build().len());
}

#[test]
fn the_section_table_reads_back_with_its_file_extents() {
    let s = sample();
    let img = image(&s.pe);
    let l = s.pe.layout();
    let names: Vec<&str> = img.sections.iter().map(|x| x.name.as_str()).collect();
    assert_eq!(
        names,
        [".text", ".rdata", ".data", ".idata", ".rsrc", ".reloc"]
    );
    for (sec, p) in img.sections.iter().zip(&l.sections) {
        assert_eq!(sec.rva.get(), p.rva);
        assert_eq!(sec.virt_size.get(), p.virt_size);
        assert_eq!(sec.raw_size.get(), p.raw_size);
        assert_eq!(sec.raw_off.get(), usize::try_from(p.raw_off).unwrap());
    }
    let data = img.section(".data").unwrap();
    assert_eq!(data.virt_size.get(), 0x3000);
    assert_eq!(data.raw_size.get(), 0x200);
    assert_eq!(data.disk_len(), 0x200);
    assert_eq!(data.mapped_len(), 0x3000);
    assert!(img.section(".text").unwrap().is_code());
    assert!(!data.is_code());
    assert_eq!(img.section_of(Rva(s.data + 0x2fff)).unwrap().name, ".data");
    assert_eq!(img.section_of(Rva(s.data + 0x3000)).unwrap().name, ".idata");
    assert_eq!(
        img.section_bytes(img.section(".text").unwrap()),
        &common::code()[..]
    );
}

#[test]
fn an_rva_maps_to_the_file_only_where_bytes_exist() {
    let s = sample();
    let img = image(&s.pe);
    let data = img.section(".data").unwrap().clone();
    assert_eq!(
        img.rva_to_off(Rva(s.data + 0x10)),
        Some(FileOffset(data.raw_off.get() + 0x10))
    );
    assert_eq!(img.rva_to_off(Rva(s.data + 0x1000)), None);
    assert_eq!(img.at_rva(Rva(s.data + 0x1000), 4), None);
    assert_eq!(img.rva_to_off(Rva(0x3c)), Some(FileOffset(0x3c)));
    assert_eq!(img.rva_to_off(Rva(0x0fff_0000)), None);
    assert_eq!(img.u32_at_rva(Rva(s.data)), Some(0x0707_0707));
    assert_eq!(
        img.off_to_rva(data.raw_off.get() + 4),
        Some(Rva(s.data + 4))
    );
    assert_eq!(img.at_rva(Rva(s.text), 3), Some(&[0x55, 0x8b, 0xec][..]));
    let (content, tail) = img.content_and_zero_tail(img.section(".text").unwrap());
    assert_eq!((content, tail), (0x40, 0));
    let (content, tail) = img.content_and_zero_tail(&data);
    assert_eq!((content, tail), (0x80, 0x3000 - 0x80));
}

#[test]
fn the_import_tables_read_back_name_hint_and_ordinal() {
    let s = sample();
    let img = image(&s.pe);
    let t = img.imports().unwrap();
    assert_eq!(t.len(), 2);
    assert_eq!(t[0].dll, "KERNEL32.dll");
    assert_eq!(t[0].ilt_rva, Rva(s.ilt[0]));
    assert_eq!(t[0].iat_rva, Rva(s.iat[0]));
    assert_eq!(t[0].name_rva, Rva(s.names[0]));
    assert_eq!(t[0].descriptor, Rva(s.idata));
    assert_eq!(t[1].descriptor, Rva(s.idata + 20));
    let names: Vec<&str> = t[0].entries.iter().map(|e| e.name.as_str()).collect();
    assert_eq!(names, ["ExitProcess", "GetVersion", "HeapAlloc"]);
    assert_eq!(t[0].entries[2].hint, Some(2));
    assert_eq!(t[0].entries[1].iat_rva, Rva(s.iat[0] + 4));
    assert_eq!(t[0].slots(), 4);
    assert_eq!(t[0].block_len(), 16);
    assert!(t[1].entries[0].by_ordinal());
    assert_eq!(t[1].entries[0].ordinal, Some(23));
    assert_eq!(t[1].entries[0].name, "Ordinal(23)");
    assert_eq!(t[1].entries[1].name, "send");
    assert_eq!(t[1].entries[1].hint, Some(5));
    assert_eq!(img.present(Dir::Iat).unwrap().rva, Rva(s.iat[0]));
}

#[test]
fn exports_list_named_entries_then_ordinal_only_ones() {
    let s = sample();
    let img = image(&s.pe);
    let e = img.exports().unwrap().unwrap();
    assert_eq!(e.dll_name, "sample.dll");
    assert_eq!(e.ordinal_base, 1);
    assert_eq!(e.timestamp, EXPORT_TIMESTAMP);
    assert_eq!((e.functions, e.names), (3, 2));
    let got: Vec<(Option<&str>, u32, u32)> = e
        .entries
        .iter()
        .map(|x| (x.name.as_deref(), x.ordinal, x.target.get()))
        .collect();
    assert_eq!(
        got,
        [
            (Some("alpha"), 3, s.text),
            (Some("beta"), 1, s.text + 0x30),
            (None, 2, s.text + 0x33)
        ]
    );
    assert!(e.entries.iter().all(|x| x.forwarder.is_none()));
}

#[test]
fn resources_carry_their_type_name_and_language_path() {
    let s = sample();
    let img = image(&s.pe);
    let r = img.resources().unwrap();
    assert_eq!(r.len(), 3);
    let wave = r
        .iter()
        .find(|x| x.type_id == ResId::Name("WAVE".into()))
        .unwrap();
    assert_eq!(wave.name_id, ResId::Name("CLICK".into()));
    assert_eq!(wave.lang_id, ResId::Ordinal(0));
    assert_eq!(wave.codepage, 1252);
    assert_eq!(
        img.at_rva(wave.data_rva, wave.data_len),
        Some(&b"RIFF...."[..])
    );
    let icon = r.iter().find(|x| x.type_id == ResId::Ordinal(3)).unwrap();
    assert_eq!(
        (icon.name_id.clone(), icon.lang_id.clone()),
        (ResId::Ordinal(1), ResId::Ordinal(1033))
    );
    assert_eq!(icon.data_len, 40);
}

#[test]
fn the_codeview_record_and_relocations_read_back() {
    let s = sample();
    let img = image(&s.pe);
    let d = img.debug_entries();
    assert_eq!(d.len(), 1);
    assert_eq!(d[0].kind, 2);
    assert_eq!(d[0].data_rva, Rva(s.nb10));
    assert_eq!(d[0].timestamp, EXPORT_TIMESTAMP);
    let cv = img.codeview().unwrap().unwrap();
    assert_eq!(cv.format, "NB10");
    assert_eq!(cv.signature, PDB_SIGNATURE);
    assert_eq!(cv.age, PDB_AGE);
    assert_eq!(cv.path, PDB_PATH);
    assert_eq!(
        usize::try_from(cv.file_offset).unwrap(),
        img.rva_to_off(Rva(s.nb10)).unwrap().get()
    );
    let r = img.relocations();
    assert_eq!(
        r,
        s.reloc_sites
            .iter()
            .map(|x| (Rva(*x), 3))
            .collect::<Vec<_>>()
    );
}

fn with_debug(entries: &[DebugEntry], records: &[Vec<u8>]) -> Image {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let rva = pe.next_rva();
    let mut body = debug_directory(entries);
    for r in records {
        body.extend_from_slice(r);
    }
    pe.push(".rdata", SCN_RDATA, body);
    pe.dir(DIR_DEBUG, rva, u32::try_from(entries.len() * 28).unwrap());
    image(&pe)
}

#[test]
fn the_codeview_entry_is_found_behind_another_debug_entry() {
    let nb10 = codeview_nb10(0x1234_5678, 9, "a.pdb");
    let n = u32::try_from(nb10.len()).unwrap();
    let base = 0x2000;
    let img = with_debug(
        &[
            DebugEntry {
                kind: 4,
                timestamp: 1,
                size: 4,
                rva: base + 56,
            },
            DebugEntry {
                kind: 2,
                timestamp: 1,
                size: n,
                rva: base + 60,
            },
        ],
        &[vec![0xaa; 4], nb10],
    );
    let cv = img.codeview().unwrap().unwrap();
    assert_eq!(
        (cv.signature, cv.age, cv.path.as_str()),
        (0x1234_5678, 9, "a.pdb")
    );
}

#[test]
fn an_rsds_record_reads_its_guid_and_age() {
    let rsds = codeview_rsds([0x11; 16], 2, "b.pdb");
    let img = with_debug(
        &[DebugEntry {
            kind: 2,
            timestamp: 1,
            size: u32::try_from(rsds.len()).unwrap(),
            rva: 0x2000 + 28,
        }],
        &[rsds],
    );
    let cv = img.codeview().unwrap().unwrap();
    assert_eq!(cv.format, "RSDS");
    assert_eq!(cv.guid.as_deref(), Some("11111111111111111111111111111111"));
    assert_eq!((cv.age, cv.path.as_str()), (2, "b.pdb"));
}

#[test]
fn a_malformed_file_is_refused_with_a_reason() {
    assert!(
        Image::parse(b"not an image".to_vec())
            .unwrap_err()
            .to_string()
            .contains("MZ")
    );
    let mut b = sample().pe.build();
    let lfanew = usize::try_from(u32::from_le_bytes(b[0x3c..0x40].try_into().unwrap())).unwrap();
    let mut no_sig = b.clone();
    no_sig[lfanew] = b'X';
    assert!(
        Image::parse(no_sig)
            .unwrap_err()
            .to_string()
            .contains("no PE signature")
    );
    let mut plus = b.clone();
    plus[lfanew + 24..lfanew + 26].copy_from_slice(&0x20bu16.to_le_bytes());
    assert!(
        Image::parse(plus)
            .unwrap_err()
            .to_string()
            .contains("PE32+")
    );
    let mut far = b.clone();
    far[0x3c..0x40].copy_from_slice(&0x00ff_0000u32.to_le_bytes());
    assert!(Image::parse(far).is_err());
    b.truncate(lfanew + 24 + 0xe0 + 40);
    assert!(
        Image::parse(b)
            .unwrap_err()
            .to_string()
            .contains("section header")
    );
}

#[test]
fn data_directories_past_the_optional_header_are_not_read() {
    let s = sample();
    let mut b = s.pe.build();
    let lfanew = usize::try_from(u32::from_le_bytes(b[0x3c..0x40].try_into().unwrap())).unwrap();
    b[lfanew + 20..lfanew + 22].copy_from_slice(&(96u16 + 2 * 8).to_le_bytes());
    let img = Image::parse(b).unwrap();
    assert_eq!(img.dirs.len(), 2);
    assert!(img.present(Dir::Resource).is_none());
}

#[test]
fn a_cyclic_resource_directory_is_refused_not_followed() {
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3]);
    let rva = pe.next_rva();
    let mut r = rsrc(
        rva,
        &[Resource {
            type_id: Id::Ordinal(3),
            name: Id::Ordinal(1),
            lang: 0,
            codepage: 0,
            data: vec![1],
        }],
    );
    r[20..24].copy_from_slice(&0x8000_0000u32.to_le_bytes());
    pe.push(".rsrc", SCN_RDATA, r.clone());
    pe.dir(DIR_RESOURCE, rva, u32::try_from(r.len()).unwrap());
    let img = image(&pe);
    let e = img.resources().unwrap_err();
    assert!(e.to_string().contains("reached twice"), "{e}");
}

#[test]
fn an_import_descriptor_array_over_its_own_thunks_is_refused() {
    let s = sample();
    let mut b = s.pe.build();
    let img = Image::parse(b.clone()).unwrap();
    let off = img.rva_to_off(Rva(s.idata + 40)).unwrap().get();
    let copy = b[off - 40..off - 20].to_vec();
    b[off..off + 20].copy_from_slice(&copy);
    let e = Image::parse(b).unwrap().imports().unwrap_err();
    assert!(e.to_string().contains("inside the descriptor array"), "{e}");
}

#[test]
fn a_cstring_reads_to_its_terminator() {
    let s = sample();
    let img = image(&s.pe);
    assert_eq!(
        img.cstr_at_rva(Rva(s.names[1])).as_deref(),
        Some("WS2_32.dll")
    );
    assert_eq!(img.cstr_at_rva(Rva(0x7fff_0000)), None);
    assert_eq!(
        img.header_structs_end(),
        usize::try_from(s.pe.layout().headers_end).unwrap()
    );
    assert_eq!(img.overlay(), 0);
}

#[test]
fn an_embedded_record_is_not_read_as_a_pdb_reference() {
    let s = msvc6_fixture::codeview::sample();
    let img = Image::parse(s.image).unwrap();
    let cv = img.codeview().unwrap().unwrap();
    assert_eq!(cv.format, "NB11");
    assert_eq!((cv.signature, cv.age, cv.path.as_str()), (0, 0, ""));
    assert_eq!(usize::try_from(cv.len).unwrap(), s.record.len());
    let at = usize::try_from(cv.file_offset).unwrap();
    assert_eq!(&img.bytes()[at..at + 4], b"NB11");
}
