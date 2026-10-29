#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_codeview::decl::Model;
use msvc6_codeview::diff::{self, Reasons, Row};
use msvc6_codeview::record::Record;
use msvc6_codeview::source::{Src, parse_bytes};
use msvc6_fixture::codeview::sample;
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

const HEADERS: &str = "lib/a.dll/include";

fn model() -> Model {
    let s = sample();
    Model::build(&Record::from_image(&Image::parse(s.image).unwrap()).unwrap()).unwrap()
}

fn src(files: &[(&str, &str)]) -> Src {
    let mut defs = BTreeMap::new();
    let mut s = Src::default();
    for (path, text) in files {
        parse_bytes(text.as_bytes(), Path::new(path), &mut defs, &mut s);
        s.files.insert(PathBuf::from(path));
    }
    s
}

fn rows(files: &[(&str, &str)], owner: bool) -> Vec<Row> {
    diff::compare_in(&model(), &src(files), Path::new(HEADERS), owner).rows
}

fn kinds(rows: &[Row]) -> Vec<(&'static str, String)> {
    rows.iter().map(|r| (r.kind, r.subject.clone())).collect()
}

const OURS: &str = "class Device {\npublic:\n    Device();\n    virtual void set_volume(unsigned long vol);\n    unsigned long m_flags;\n    static Device *m_head;\n    void helper();\n};\n";
const BODIES: &str = "void Device::set_volume(unsigned long volume) { }\nDevice::Device() { }\n";

#[test]
fn every_kind_of_mismatch_is_one_line() {
    let r = rows(
        &[
            ("lib/a.dll/include/dev.h", OURS),
            ("lib/a.dll/src/dev.cpp", BODIES),
        ],
        true,
    );
    let got = kinds(&r);
    for want in [
        ("class-name", "Dev"),
        ("header-placement", "Dev"),
        ("member-missing", "Dev::is_on"),
        ("member-extra", "Dev::helper"),
        ("access", "Dev::m_flags"),
        ("access", "Dev::m_head"),
        ("param-name", "Dev::set_volume#1"),
        ("inline-placement", "Dev::set_volume"),
        ("order", "Dev::set_volume"),
        ("class-missing", "Snd"),
    ] {
        assert!(
            got.contains(&(want.0, want.1.to_string())),
            "missing {want:?} in {got:#?}"
        );
    }
    assert!(!got.contains(&("param-name", "Dev::set_volume#1 (definition)".into())));
    assert!(
        !got.iter()
            .any(|(k, s)| *k == "inline-placement" && s == "Dev::Dev")
    );
    let fix = r
        .iter()
        .find(|x| x.kind == "param-name")
        .and_then(|x| x.fix.clone())
        .unwrap();
    assert_eq!(
        (fix.old.as_str(), fix.new.as_str(), fix.from),
        ("vol", "volume", 4)
    );
    let view = rows(&[("lib/a.dll/include/dev.h", OURS)], false);
    assert!(!kinds(&view).iter().any(|(k, _)| *k == "class-missing"));
}

#[test]
fn the_explained_table_follows_the_configured_reasons() {
    let r = rows(&[("lib/a.dll/include/dev.h", OURS)], true);
    let rules = Reasons {
        categories: vec!["MEASURED".into(), "OTHER".into()],
        cite: BTreeMap::from([("OTHER".to_string(), "ROW".to_string())]),
    };
    let ex =
        diff::read_explained("a.dll\taccess\tDev::m_*\tMEASURED: both\n", "t.tsv", &rules).unwrap();
    let n = r
        .iter()
        .filter(|x| diff::explains(&ex, "a.dll", x).is_some())
        .count();
    assert_eq!(n, 2);
    assert!(diff::read_explained("*\taccess\tX\tbecause\n", "t.tsv", &rules).is_err());
    assert!(diff::read_explained("v\taccess\tX\tOTHER: no citation\n", "t.tsv", &rules).is_err());
    assert!(diff::read_explained("v\taccess\tX\n", "t.tsv", &rules).is_err());
    assert!(diff::read_explained("v\taccess\tX\tanything\n", "t.tsv", &Reasons::default()).is_ok());
    let (text, shown, summary) =
        diff::report("a.dll", &r, &BTreeSet::new(), &ex, diff::Print::default());
    assert_eq!(summary.explained, 2);
    assert_eq!(shown.len(), r.len());
    assert!(text.ends_with(&format!(
        "CV-DIFF a.dll: {} mismatch(es), 2 explained (MEASURED 2), {} UNEXPLAINED\n",
        r.len(),
        r.len() - 2
    )));
}

#[test]
fn a_field_the_record_names_otherwise_is_renamed_where_the_fields_agree() {
    let class = |ty: &str| {
        format!(
            "class Device {{\npublic:\n    Device();\n    virtual void set_volume(unsigned long volume);\n    {ty} m_bits;\n    static Device *m_head;\n    bool is_on();\n}};\n"
        )
    };
    let renames = |t: &str| -> Vec<(String, String)> {
        diff::compare_in(
            &model(),
            &src(&[("lib/a.dll/include/dev.h", t)]),
            Path::new(HEADERS),
            true,
        )
        .renames
        .into_iter()
        .map(|f| (f.old, f.new))
        .collect()
    };
    assert_eq!(
        renames(&class("unsigned long")),
        [("m_bits".to_string(), "m_flags".to_string())]
    );
    assert_eq!(renames(&class("char *")), []);
    let s = src(&[("lib/a.dll/include/dev.h", &class("unsigned long"))]);
    assert_eq!(msvc6_codeview::apply::declaration_count(&[&s], "m_bits"), 1);
    assert_eq!(
        msvc6_codeview::apply::declaration_count(&[&s], "m_flags"),
        0
    );
}

#[test]
fn an_overload_group_is_ordered_by_declaration_not_by_its_method_list() {
    let orders = |t: &str| -> Vec<(String, String)> {
        rows(&[("lib/a.dll/include/snd.h", t)], true)
            .into_iter()
            .filter(|r| r.kind == "order")
            .map(|r| (r.subject, r.record))
            .collect()
    };
    let as_declared = "class Snd : public Dev {\npublic:\n    virtual void load(char *s);\n    virtual void load(unsigned long n);\n    virtual void play();\n    virtual void play(unsigned long n);\n};\n";
    assert_eq!(orders(as_declared), []);
    let apart = "class Snd : public Dev {\npublic:\n    virtual void load(char *s);\n    virtual void play();\n    virtual void play(unsigned long n);\n    virtual void load(unsigned long n);\n};\n";
    assert_eq!(orders(apart), []);
    let swapped = "class Snd : public Dev {\npublic:\n    virtual void load(char *s);\n    virtual void load(unsigned long n);\n    virtual void play(unsigned long n);\n    virtual void play();\n};\n";
    assert_eq!(
        orders(swapped),
        [(
            "Snd::play".to_string(),
            "virtual void play(); first in its overload group".to_string()
        )]
    );
}

#[test]
fn an_override_matches_the_signature_not_only_the_name() {
    let virt = |base_param: &str| -> Vec<String> {
        let t = format!(
            "class Base {{\npublic:\n    virtual bool is_on({base_param});\n}};\nclass Device : public Base {{\npublic:\n    Device();\n    virtual void set_volume(unsigned long volume);\n    unsigned long m_flags;\n    static Device *m_head;\n    bool is_on();\n}};\n"
        );
        rows(&[("lib/a.dll/include/dev.h", &t)], true)
            .into_iter()
            .filter(|r| r.kind == "static-virtual" && r.subject == "Dev::is_on")
            .map(|r| r.record)
            .collect()
    };
    assert_eq!(virt("int x"), Vec::<String>::new());
    assert_eq!(virt(""), ["non-virtual"]);
}

#[test]
fn an_enum_is_compared_enumerator_by_enumerator() {
    let r = rows(
        &[(
            "lib/a.dll/include/err.h",
            "#define BASE 1\nenum ERRCODE { ERR_OK = 0, ERR_FAIL = BASE + 1, ERR_EXTRA };\n",
        )],
        true,
    );
    let e: Vec<(String, String)> = r
        .into_iter()
        .filter(|x| x.kind == "enumerator")
        .map(|x| (x.subject, x.our_says))
        .collect();
    assert_eq!(
        e,
        [
            ("ERRCODE::ERR_FAIL".to_string(), "ERR_FAIL = 2".to_string()),
            ("ERRCODE::ERR_EXTRA".to_string(), "ERR_EXTRA".to_string()),
        ]
    );
}
