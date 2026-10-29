#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_model::c1count::Count;

fn count(s: &str) -> Count {
    msvc6_model::c1count::count(s).unwrap()
}

fn count_with(s: &str, gx: bool) -> Count {
    msvc6_model::c1count::count_with(s, gx).unwrap()
}

#[test]
fn toys_measured() {
    let cases = common::toys::TOYS;
    let mut bad = Vec::new();
    for (src, want) in cases {
        let got = count(src).total;
        if got != *want {
            bad.push(format!("{src}: want {want} got {got}"));
        }
    }
    assert!(bad.is_empty(), "{}", bad.join("\n"));
}

#[test]
fn eh_toys_measured() {
    let cases = common::toys::EH_TOYS;
    let mut bad = Vec::new();
    for (src, want) in cases {
        let got = count(src).total;
        if got != *want {
            bad.push(format!("{src}: want {want} got {got}"));
        }
    }
    assert!(bad.is_empty(), "{}", bad.join("\n"));
}

#[test]
fn template_toys_measured() {
    let cases = common::toys::TEMPLATE_TOYS;
    let mut bad = Vec::new();
    for (src, want) in cases {
        let got = count(src).total;
        if got != *want {
            bad.push(format!("{src}: want {want} got {got}"));
        }
    }
    assert!(bad.is_empty(), "{}", bad.join("\n"));
    assert!(
        count("template<class T> struct S { T a; S(); }; S<int> si;")
            .unmodelled
            .iter()
            .any(|u| u.contains("template instance"))
    );
    assert!(
        count("struct D { D(); }; void use() { new D; }")
            .unmodelled
            .is_empty()
    );
    let nw = "struct F { F(); virtual ~F(); static F *N(int n) { return new F[n]; } };";
    assert_eq!(
        count_with(nw, true).total - count_with(nw, false).total,
        -20
    );
}
#[test]
fn live_words_drop_comments_and_dead_branches() {
    let w = msvc6_model::c1count::live_words(
        "/* gone */ int kept;\n#if 0\nint dead;\n#endif\nx9 = 1;\n",
    );
    assert!(w.contains("kept") && w.contains("x9") && w.contains("int"));
    assert!(!w.contains("gone") && !w.contains("dead") && !w.contains("1"));
}

#[test]
fn virtual_bases_toys_measured() {
    let cases = common::toys::VBASE_TOYS;
    let bad: Vec<String> = cases
        .iter()
        .filter_map(|(src, want)| {
            let got = count(src).total;
            (got != *want).then(|| format!("{src}: measured {want}, predicted {got}"))
        })
        .collect();
    assert!(bad.is_empty(), "{}", bad.join("\n"));
}

#[test]
fn two_bases_parse_as_a_class_body() {
    let two = count(
        "struct A { int a; }; struct B { int b; }; struct C : public A, public B { int c; };",
    );
    let one = count("struct A { int a; }; struct B { int b; }; struct C : public A { int c; };");
    assert_eq!(two.total, one.total);
    let poly = count(
        "struct A { virtual ~A(); }; struct B { int b; }; struct C : public B, public A { C(); int c; static int s; };",
    );
    assert_eq!(poly.by_kind.get("static member"), Some(&2));
}

#[test]
fn guards_and_includes() {
    let c = count(
        "#ifndef X_H\n#define X_H\n#include \"a.h\"\n#if 0\nint dead;\n#endif\nint live;\n#endif\n",
    );
    assert_eq!(c.total, 1);
    assert_eq!(c.includes, vec!["a.h".to_string()]);
}
#[test]
fn new_delete_toys_measured() {
    let cases = common::toys::NEW_DELETE_TOYS;
    let mut bad = Vec::new();
    for (src, gx, plain) in cases {
        let (a, b) = (count_with(src, true), count_with(src, false));
        if a.total != *gx || b.total != *plain {
            bad.push(format!(
                "{src}: want {gx}/{plain} got {}/{} {:?}",
                a.total, b.total, b.by_kind
            ));
        }
    }
    assert!(bad.is_empty(), "{}", bad.join("\n"));
}
