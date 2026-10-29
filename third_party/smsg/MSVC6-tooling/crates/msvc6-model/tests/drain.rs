#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::il::{Body, Set, def};
use msvc6_model::drain;

const EXPORTED: u8 = 9;
const PLAIN: u8 = 5;
const HEADER_INLINE: u32 = 0x20;
const EXPORTED_INLINE: u32 = 0x34;

fn body(id: u32, callee: Option<u32>) -> Body {
    let mut b = Body::default();
    b.open(id, &[]);
    if let Some(c) = callee {
        b.call_void(c);
    }
    b.close(id + 0x100);
    b
}

fn set(rows: &[(u32, &str, u8, u32, Option<u32>)]) -> Set {
    let mut s = Set::new(0x200);
    for &(id, name, fclass, flags, callee) in rows {
        let mut d = def(id, name, 0x10, 0x28);
        d.fclass = fclass;
        d.flags = flags;
        s.define(d, &body(id, callee));
    }
    s
}

fn names(v: &[&str]) -> Vec<String> {
    v.iter().map(|s| (*s).to_string()).collect()
}

#[test]
fn an_exported_inline_member_is_emitted_at_its_offset_and_a_header_inline_on_demand() {
    let s = set(&[
        (0x10, "?f@T@@QAEXXZ", EXPORTED, EXPORTED_INLINE, None),
        (0x11, "?h@@YAXXZ", PLAIN, HEADER_INLINE, None),
        (0x12, "?u1@@YAXXZ", PLAIN, 0, Some(0x11)),
        (0x13, "?u2@@YAXXZ", PLAIN, 0, None),
    ]);
    let (fns, odd) = drain::gl_functions(&s.gl());
    assert_eq!(odd, 0);
    assert_eq!(
        fns.iter().map(|f| f.exported).collect::<Vec<_>>(),
        [true, false, false, false]
    );
    let il = drain::read_tu(&s.gl(), &s.ex_bytes(), &[]);
    assert!(!il.fns["?f@T@@QAEXXZ"].deferred());
    assert!(il.fns["?h@@YAXXZ"].deferred());
    let object = names(&["?f@T@@QAEXXZ", "?u1@@YAXXZ", "?h@@YAXXZ", "?u2@@YAXXZ"]);
    let mut refs = drain::Refs::default();
    refs.text.insert("?u1@@YAXXZ".into(), names(&["?h@@YAXXZ"]));
    assert_eq!(drain::predicted_demand(&object, &il, &refs), object);
}

#[test]
fn a_header_inline_member_no_one_references_drains_last() {
    let s = set(&[
        (0x10, "?f@T@@QAEXXZ", PLAIN, HEADER_INLINE, None),
        (0x12, "?u1@@YAXXZ", PLAIN, 0, None),
    ]);
    let il = drain::read_tu(&s.gl(), &s.ex_bytes(), &[]);
    let object = names(&["?f@T@@QAEXXZ", "?u1@@YAXXZ"]);
    assert_eq!(
        drain::predicted_demand(&object, &il, &drain::Refs::default()),
        names(&["?u1@@YAXXZ", "?f@T@@QAEXXZ"])
    );
}
