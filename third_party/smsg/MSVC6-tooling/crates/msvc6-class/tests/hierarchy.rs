#![allow(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing)]

use msvc6_class::insns;
use msvc6_class::join::{self, Inputs, Slot};
use msvc6_class::slots::Rule;
use msvc6_class::vft::{self, Control, Found, Run, Sections, Text};
use msvc6_image::pe::Image;

use msvc6_fixture::classes::*;

fn followed(img: &Image) -> Vec<Run> {
    let text = Text::of(img, vft::CODE).unwrap();
    vft::detect(img, Sections::default())
        .unwrap()
        .into_iter()
        .map(|r| Run {
            rva: r.rva,
            slots: r.slots.iter().map(|s| text.follow(*s)).collect(),
        })
        .collect()
}

fn joined(rule: Rule) -> join::Join {
    let img = Image::parse(image()).unwrap();
    let runs = followed(&img);
    let names = names();
    let files = vec![("include/shapes.h".to_string(), HEADER.to_string())];
    join::join(&Inputs {
        runs: &runs,
        names: &names,
        map: &[],
        placed: &[],
        files: &files,
        headers: &[],
        rule,
        text: Text::of(&img, vft::CODE).ok(),
    })
}

#[test]
fn two_abutting_tables_read_as_one_run_and_their_constructors_split_it() {
    let img = Image::parse(image()).unwrap();
    let runs = vft::detect(&img, Sections::default()).unwrap();
    assert_eq!(runs.len(), 1);
    assert_eq!((runs[0].rva, runs[0].slots.len()), (BASE_VT, 9));
    let text = Text::of(&img, vft::CODE).unwrap();
    let funcs = insns::decode(text.bytes, text.rva, &extents());
    let stores = vft::stores(&funcs, BASE, &runs);
    let at: Vec<(u32, u32, Option<i32>)> =
        stores.iter().map(|s| (s.table, s.func, s.disp)).collect();
    assert_eq!(
        at,
        vec![(BASE_VT, 0x1000, Some(0)), (DERIVED_VT, 0x1010, Some(0))]
    );
    let tables = vft::split(&runs, &stores);
    let rows: Vec<(u32, usize, bool)> = tables
        .iter()
        .map(|t| (t.rva, t.slots.len(), t.attested))
        .collect();
    assert_eq!(rows, vec![(BASE_VT, 4, true), (DERIVED_VT, 5, true)]);
    let checks = vft::check_controls(
        &runs,
        &[
            Control {
                rva: BASE_VT,
                slots: 9,
                what: "the merged run".into(),
            },
            Control {
                rva: BASE_VT,
                slots: 4,
                what: "Base alone".into(),
            },
            Control {
                rva: DERIVED_VT,
                slots: 5,
                what: "Derived alone".into(),
            },
        ],
    );
    let found: Vec<Found> = checks.into_iter().map(|(_, f)| f).collect();
    assert_eq!(found, vec![Found::Agrees, Found::Length(9), Found::Missing]);
    let lines = vft::table_report(text, &runs, &tables, &stores, DERIVED_VT, &|b| {
        BODIES
            .iter()
            .find(|(r, _)| *r == b)
            .map(|(_, n)| (*n).to_string())
    })
    .unwrap();
    assert!(lines.iter().any(|l| l == "    [  2]  +0x8    0x001070  ->  0x001070 (direct)  ?f@Derived@@UAEXH@Z"), "{lines:#?}");
}

#[test]
fn each_class_joins_its_own_table_with_every_slot_agreeing_under_the_overload_rule() {
    let j = joined(Rule::Vc6);
    let got: Vec<(&str, u32, usize)> = j
        .assigned
        .iter()
        .map(|a| (a.class.as_str(), a.table, a.names.len()))
        .collect();
    assert_eq!(got, vec![("Base", BASE_VT, 4), ("Derived", DERIVED_VT, 5)]);
    for a in &j.assigned {
        assert!(
            a.slots.iter().all(|s| *s == Slot::Agrees),
            "{}: {:?}",
            a.class,
            a.slots
        );
    }
    assert_eq!(j.assigned[0].names, vec!["~", "f", "f", "g"]);
    assert_eq!(j.assigned[1].names, vec!["~", "f", "f", "g", "h"]);
    assert!(j.unpinned.is_empty() && j.ambiguous.is_empty() && j.unresolved.is_empty());
    assert!(j.arities.is_empty(), "{:?}", j.arities);
    let tsv = join::tsv(&j, BASE);
    assert!(
        tsv.contains("0x10002010\tDerived\t!~Derived\t!f\t!f\t!g\t!h\n"),
        "{tsv}"
    );
}

#[test]
fn the_declaration_order_reading_cannot_place_the_overloaded_base() {
    let j = joined(Rule::Legacy);
    assert!(
        j.assigned.iter().all(|a| a.class != "Base"),
        "{:?}",
        j.assigned
    );
    assert_eq!(j.ambiguous.len(), 1, "{:?}", j.ambiguous);
    assert_eq!(j.ambiguous[0].0, "Base");
}
