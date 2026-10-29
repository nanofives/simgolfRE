#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::archive::short_import;
use msvc6_fixture::backstop;
use msvc6_fixture::coff::{
    CLASS_STATIC, Object as Obj, REL_DIR32, REL_REL32, Reloc as Rel, SCN_COMDAT, SCN_DATA,
    SCN_TEXT, SELECT_ANY, SELECT_ASSOCIATIVE, SELECT_NODUPLICATES, Section,
};
use msvc6_image::coff::{ImportObject, Object, looks_like_object, section_align};

fn sample() -> Vec<u8> {
    let mut o = Obj::new();
    let drectve = o.section(Section {
        name: ".drectve".into(),
        characteristics: 0x0010_0a00,
        data: b"-defaultlib:LIBCMT ".to_vec(),
        ..Section::default()
    });
    let text = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: vec![0xe8, 0, 0, 0, 0, 0xa1, 0, 0, 0, 0, 0xc3],
        relocs: vec![
            Rel {
                va: 1,
                symbol: 8,
                typ: REL_REL32,
            },
            Rel {
                va: 6,
                symbol: 9,
                typ: REL_DIR32,
            },
        ],
        ..Section::default()
    });
    let xdata = o.section(Section {
        name: ".xdata$x".into(),
        characteristics: 0x4030_1040,
        data: vec![0x22, 0x05, 0x93, 0x19],
        ..Section::default()
    });
    let data = o.section(Section {
        name: ".data".into(),
        characteristics: SCN_DATA,
        data: vec![1, 2, 3, 4],
        ..Section::default()
    });
    o.comp_id(0x0b, 8168);
    o.section_symbol(drectve, 0, 0);
    o.section_symbol(text, SELECT_NODUPLICATES, 0);
    o.external("?Render@Widget@@QAEXXZ", 0, text);
    o.section_symbol(xdata, SELECT_ASSOCIATIVE, 2);
    o.undefined("?Draw@Sprite@@QAEXXZ");
    o.undefined("_g_counter");
    o.section_symbol(data, 0, 0);
    o.plain("_local", 0, data, 0, CLASS_STATIC);
    o.plain("_common", 16, 0, 0, 2);
    o.undefined("?Render@Widget@@QAEXXZ");
    o.build()
}

#[test]
fn sections_symbols_and_relocations_read_back() {
    let d = sample();
    let o = Object::parse(&d).unwrap();
    assert_eq!(o.header.machine, 0x14c);
    assert_eq!(o.header.sections, 4);
    let names: Vec<&str> = o.sections.iter().map(|s| s.name.as_str()).collect();
    assert_eq!(names, [".drectve", ".text", ".xdata$x", ".data"]);
    let text = o.section(2).unwrap();
    assert!(text.is_comdat());
    assert_eq!(text.align(), 16);
    assert_eq!(o.section_data(text).len(), 11);
    let r = o.relocations(text);
    assert_eq!(r.len(), 2);
    assert_eq!((r[0].va, r[0].typ), (1, REL_REL32));
    assert_eq!(o.symbol(r[0].symbol).unwrap().name, "?Draw@Sprite@@QAEXXZ");
    assert_eq!(o.symbol(r[1].symbol).unwrap().name, "_g_counter");
    assert_eq!(o.comp_id(), Some((0x0b << 16) | 8168));
    let def = o.symbol(3).unwrap().section_definition().unwrap();
    assert_eq!(
        (def.len, def.relocs, def.selection),
        (11, 2, SELECT_NODUPLICATES)
    );
    assert_eq!(o.symbol(5).unwrap().name, "?Render@Widget@@QAEXXZ");
    assert!(o.symbol(4).is_none());
}

#[test]
fn comdats_name_their_symbol_and_an_associative_section_its_parent() {
    let d = sample();
    let o = Object::parse(&d).unwrap();
    let c = o.comdats();
    assert_eq!(c.len(), 2);
    assert_eq!(c[0].section, 2);
    assert_eq!(c[0].selection, SELECT_NODUPLICATES);
    assert_eq!(c[0].symbol.as_deref(), Some("?Render@Widget@@QAEXXZ"));
    assert_eq!(c[1].section, 3);
    assert_eq!(c[1].associated, Some(2));
    assert_eq!(c[1].symbol, None);
}

#[test]
fn a_forward_reference_to_a_name_the_object_defines_is_not_undefined() {
    let d = sample();
    let o = Object::parse(&d).unwrap();
    let (undefined, common) = o.undefined_externals();
    let u: Vec<&str> = undefined.iter().map(String::as_str).collect();
    assert_eq!(u, ["?Draw@Sprite@@QAEXXZ", "_g_counter"]);
    assert_eq!(common.into_iter().collect::<Vec<_>>(), ["_common"]);
    let defined: Vec<String> = o.defined_externals().into_iter().collect();
    assert_eq!(defined, ["?Render@Widget@@QAEXXZ"]);
    let by_section = o.section_symbols();
    assert_eq!(
        by_section.get(&4).unwrap(),
        &vec![(0, "_local".to_string(), false)]
    );
    assert_eq!(o.code_bytes(), 11);
}

#[test]
fn an_alias_object_with_no_machine_is_still_an_object() {
    let mut o = Obj::new();
    o.machine = 0;
    o.code(".text", &[]);
    o.comp_id(0x0c, 7291);
    let d = o.build();
    assert!(looks_like_object(&d));
    assert_eq!(
        Object::parse(&d).unwrap().comp_id(),
        Some((0x0c << 16) | 7291)
    );
}

#[test]
fn a_short_import_member_is_read_and_refused_as_an_object() {
    let s = short_import("USER32.dll", "_MessageBoxA@16", 7, 0, 3);
    let i = ImportObject::parse(&s).unwrap();
    assert_eq!(i.symbol, "_MessageBoxA@16");
    assert_eq!(i.dll, "USER32.dll");
    assert_eq!((i.hint, i.kind, i.name_type), (7, 0, 3));
    assert_eq!(i.name_type_name(), "undecorate");
    assert!(Object::parse(&s).is_err());
    assert!(ImportObject::parse(&sample()).is_none());
}

#[test]
fn a_truncated_object_is_refused() {
    let d = sample();
    assert!(Object::parse(&d[..10]).is_err());
    assert!(Object::parse(&d[..30]).is_err());
    let symptr = usize::try_from(u32::from_le_bytes(d[8..12].try_into().unwrap())).unwrap();
    assert!(Object::parse(&d[..symptr + 18]).is_err());
}

#[test]
fn the_alignment_field_is_a_biased_power_of_two() {
    assert_eq!(section_align(0), 1);
    assert_eq!(section_align(0x0010_0000), 1);
    assert_eq!(section_align(0x0030_0000), 4);
    assert_eq!(section_align(0x0050_0000), 16);
    assert_eq!(section_align(0x00e0_0000), 8192);
    let mut o = Obj::new();
    o.code(".text$mn_a_long_name", &[0xc3]);
    let d = o.build();
    let p = Object::parse(&d).unwrap();
    assert_eq!(p.sections[0].name, ".text$mn_a_long_name");
    let _ = SELECT_ANY;
}

fn string_table_of(d: &[u8]) -> usize {
    let symptr = usize::try_from(u32::from_le_bytes(d[8..12].try_into().unwrap())).unwrap();
    let nsyms = usize::try_from(u32::from_le_bytes(d[12..16].try_into().unwrap())).unwrap();
    symptr + nsyms * 18
}

#[test]
fn a_name_is_read_inside_its_string_table_and_capped() {
    use msvc6_image::coff::MAX_NAME;
    let mut o = Obj::new();
    let t = o.code(".text", &[0xc3]);
    o.section_symbol(t, SELECT_ANY, 0);
    let huge = "?a".repeat(3000);
    o.external(&huge, 0, t);
    o.external("?short_but_long@@3HA", 0, t);
    let d = o.build();
    let p = Object::parse(&d).unwrap();
    assert_eq!(p.symbols[1].name.len(), MAX_NAME);
    assert!(huge.starts_with(&p.symbols[1].name));
    assert_eq!(p.symbols[2].name, "?short_but_long@@3HA");
    let strtab = string_table_of(&d);
    let size = usize::try_from(u32::from_le_bytes(
        d[strtab..strtab + 4].try_into().unwrap(),
    ))
    .unwrap();
    assert_eq!(strtab + size, d.len());
    assert_eq!(p.string_table_end(), d.len());
    assert_eq!(p.string(size), Some(String::new()));
    assert_eq!(p.string(size + 1), None);
    assert_eq!(p.string(usize::MAX), None);

    let mut open = d.clone();
    *open.last_mut().unwrap() = b'x';
    open.extend(std::iter::repeat_n(b'y', 100_000));
    let q = Object::parse(&open).unwrap();
    assert_eq!(q.symbols[2].name, "?short_but_long@@3HAx");
    assert_eq!(q.string_table_end(), strtab + size);

    let mut wide = open.clone();
    wide[strtab..strtab + 4].copy_from_slice(&u32::MAX.to_le_bytes());
    let r = Object::parse(&wide).unwrap();
    assert_eq!(r.string_table_end(), wide.len());
    assert_eq!(r.symbols[2].name.len(), MAX_NAME);
    assert!(r.symbols[2].name.starts_with("?short_but_long@@3HAxyyy"));

    let mut none = d.clone();
    none.truncate(strtab + 2);
    let n = Object::parse(&none).unwrap();
    assert_eq!(n.string_table_end(), strtab);
    assert_eq!(n.symbols[2].name, "");
}

#[test]
fn many_names_with_no_nul_read_within_the_name_budget() {
    let mut o = Obj::new();
    let t = o.code(".text", &[0xc3]);
    for k in 0..2_000 {
        o.external(&format!("?name_number_{k:06}@@3HA"), 0, t);
    }
    let mut d = o.build();
    let strtab = string_table_of(&d);
    for b in d.iter_mut().skip(strtab + 4) {
        if *b == 0 {
            *b = b'z';
        }
    }
    d.extend(std::iter::repeat_n(b'w', 1 << 20));
    d[strtab..strtab + 4].copy_from_slice(&u32::MAX.to_le_bytes());
    let (p, _) = backstop::run("2000 names with no NUL", || Object::parse(&d).unwrap());
    assert_eq!(p.symbols.len(), 2_000);
    assert!(
        p.symbols
            .iter()
            .all(|s| s.name.len() <= msvc6_image::coff::MAX_NAME)
    );
    let read: usize = p.symbols.iter().map(|s| s.name.len()).sum();
    let budget = d.len() * msvc6_image::coff::NAME_BUDGET
        + msvc6_image::coff::MAX_NAME * msvc6_image::coff::NAME_ALLOWANCE;
    assert!(
        read <= budget,
        "{read} name bytes, past the {budget} budget"
    );
    let mut o = Obj::new();
    let t = o.code(".text", &[0xc3]);
    for k in 0..4_000 {
        o.external(&format!("?name_number_{k:06}@@3HA"), 0, t);
    }
    let mut many = o.build();
    let strtab = string_table_of(&many);
    for b in many.iter_mut().skip(strtab + 4) {
        if *b == 0 {
            *b = b'z';
        }
    }
    many.extend(std::iter::repeat_n(b'w', 1 << 20));
    many[strtab..strtab + 4].copy_from_slice(&u32::MAX.to_le_bytes());
    let e = Object::parse(&many).expect_err("4,000 capped names read past the budget");
    assert!(format!("{e:#}").contains("names overlap"), "{e:#}");
}
