use super::file::{c11_layout, find_text, renamed_source, shown, user_profiles, whole_token};
use super::*;

impl Tally {
    fn failed(&self, class: &str) -> usize {
        self.row(class).failed
    }
    fn retired_of(&self, class: &str) -> usize {
        self.row(class).retired
    }
    fn folded_of(&self, class: &str) -> usize {
        self.row(class).folded
    }
    fn ok_of(&self, class: &str) -> usize {
        self.row(class).ok
    }
}

fn entry(size: u32, names: &[&str], obj: Option<&str>, code: bool) -> Entry {
    let mut keys = BTreeSet::new();
    for n in names {
        keys.extend(keys_of(n));
    }
    Entry {
        size,
        keys,
        shown: names.first().map(|n| (*n).to_string()).unwrap_or_default(),
        obj: obj.map(str::to_string),
        code,
        our: None,
        source: Source::Report,
    }
}

fn ship() -> Shipped<'static> {
    let sec = |name: &str, rva, size, exec| ShipSec {
        name: name.into(),
        rva,
        vsize: size,
        mapped: size,
        exec,
    };
    let mut s = Shipped {
        secs: vec![
            sec(".text", 0x1000, 0x1000, true),
            sec(".data", 0x2000, 0x100, false),
        ],
        oracle: BTreeMap::new(),
        by_name: BTreeMap::new(),
        iat_names: BTreeMap::new(),
        img: None,
        ours: None,
        may_fold: true,
        exact: Vec::new(),
        longest: 0,
        least: BTreeMap::new(),
        bounding: BTreeSet::new(),
        retired: None,
        line_dirs: BTreeMap::new(),
    };
    let mut f = entry(0x40, &["f", "?f@@YAXXZ"], Some("a.obj"), true);
    f.our = Some(0x5100);
    s.add(0x1100, f);
    s.add(0x1200, entry(0x20, &["g"], Some("b.obj"), true));
    s.add(0x1300, entry(0x10, &["g"], Some("a.obj"), true));
    s.add(0x2010, entry(4, &["d", "_d"], None, false));
    let mut by = BTreeMap::new();
    for (rva, es) in &s.oracle {
        for e in es {
            if let Some(o) = &e.obj {
                for k in &e.keys {
                    by.insert((k.clone(), o.clone()), *rva);
                }
            }
        }
    }
    s.by_name = by;
    s.index();
    s
}

fn auditor<'s>(s: &'s Shipped<'static>) -> Auditor<'s, 'static> {
    Auditor::new(s, EXAMPLES)
}

fn rec(kind: u16, payload: &[u8]) -> Vec<u8> {
    let mut b = Vec::new();
    b.extend_from_slice(&u16::try_from(payload.len() + 2).unwrap().to_le_bytes());
    b.extend_from_slice(&kind.to_le_bytes());
    b.extend_from_slice(payload);
    while b.len() % 4 != 0 {
        b.push(0);
    }
    b
}

fn st(name: &str) -> Vec<u8> {
    let mut b = vec![u8::try_from(name.len()).unwrap()];
    b.extend_from_slice(name.as_bytes());
    b
}

fn proc(seg: u16, off: u32, len: u32, name: &str) -> Vec<u8> {
    let mut p = vec![0u8; 12];
    p.extend_from_slice(&len.to_le_bytes());
    p.extend_from_slice(&[0u8; 12]);
    p.extend_from_slice(&off.to_le_bytes());
    p.extend_from_slice(&seg.to_le_bytes());
    p.push(0);
    p.extend_from_slice(&st(name));
    rec(0x100b, &p)
}

fn named(kind: u16, seg: u16, off: u32, name: &str) -> Vec<u8> {
    let mut p = vec![0u8; 4];
    p.extend_from_slice(&off.to_le_bytes());
    p.extend_from_slice(&seg.to_le_bytes());
    p.extend_from_slice(&st(name));
    rec(kind, &p)
}

fn label(kind: u16, seg: u16, off: u32, name: &str) -> Vec<u8> {
    let mut p = Vec::new();
    p.extend_from_slice(&off.to_le_bytes());
    p.extend_from_slice(&seg.to_le_bytes());
    p.push(0);
    p.extend_from_slice(&st(name));
    rec(kind, &p)
}

fn block(seg: u16, off: u32, len: u32) -> Vec<u8> {
    let mut p = vec![0u8; 8];
    p.extend_from_slice(&len.to_le_bytes());
    p.extend_from_slice(&off.to_le_bytes());
    p.extend_from_slice(&seg.to_le_bytes());
    p.push(0);
    rec(0x0207, &p)
}

fn end() -> Vec<u8> {
    rec(0x0006, &[])
}

fn module(index: usize, obj: &str) -> ModRef {
    ModRef {
        index,
        obj: obj.into(),
    }
}

#[test]
fn a_name_key_removes_spelling_and_nothing_else() {
    assert_eq!(
        name_key("std::list<class Cell *,class std::allocator<class Cell *> >::insert"),
        name_key("std::list<Cell *,std::allocator<Cell *> >::insert")
    );
    assert_eq!(name_key("_malloc"), name_key("malloc"));
    assert_eq!(
        name_key("Foo::w06`vtordisp{4294967292,0}'"),
        name_key("Foo::w06`vtordisp'")
    );
    assert_ne!(name_key("getNormalArray"), name_key("getVertexArray"));
}

#[test]
fn a_display_name_drops_decoration() {
    assert_eq!(display_name("?Send@Link@@QAEHPAXHHHH@Z"), "Link::Send");
    assert_eq!(display_name("?Run@@YAXXZ"), "Run");
    assert_eq!(display_name("_CreateThing"), "CreateThing");
    assert_eq!(display_name("_WinMain@16"), "WinMain");
    assert_eq!(display_name("_png_read@x"), "png_read@x");
    assert_eq!(display_name("?g_table@@3PAUItem@@A+0x21"), "g_table+0x21");
    assert_eq!(display_name("_g_mode+0x4"), "g_mode+0x4");
    assert_eq!(display_name("a-b"), "a-b");
    assert_eq!(display_name(".text+0x1c"), ".text+0x1c");
    assert_eq!(
        keys_of("?w06@List@@$4PPPPPPPM@A@AEXXZ"),
        [
            "?w06@List@@$4PPPPPPPM@A@AEXXZ",
            "List::w06vtordisp",
            "List::w06",
            "List::w06"
        ]
    );
    assert_eq!(display_name("_g_mode+0x4-0x8"), "g_mode+0x4-0x8");
    assert_eq!(display_name("_g_mode+0x4-x+0x8"), "g_mode+0x4-x+0x8");
}

#[test]
fn a_function_is_found_past_200000_pairs_rows_without_walking_them() {
    let mut s = ship();
    let n = 200_000u32;
    for k in 0..n {
        let mut e = entry(1, &["p"], None, true);
        e.source = Source::Pairs;
        s.add(0x1_0000 + k, e);
    }
    s.index();
    let (found, took) = msvc6_fixture::backstop::run("200000 pairs rows past a function", || {
        (0..n)
            .filter(|k| {
                s.code_at(0x1_0000 + k)
                    .is_some_and(|(at, e)| at == 0x1_0000 + k && e.source == Source::Pairs)
            })
            .count()
    });
    assert_eq!(found, 200_000, "{took:?}");
    assert_eq!(s.code_at(0x1108).map(|(at, _)| at), Some(0x1100));
    assert_eq!(
        s.next_after(0x1100, &s.oracle[&0x1100][0])
            .map(|(at, _)| at),
        Some(0x1200)
    );
}

#[test]
fn a_display_name_with_200000_address_suffixes_is_read_in_one_pass() {
    let tail = "+0x1".repeat(200_000);
    let name = format!("?a{tail}");
    assert_eq!(display_name(&name), name);
    assert_eq!(display_name(&format!("_g{tail}")), format!("g{tail}"));
}

#[test]
fn a_child_outside_its_procedure_fails_and_a_retired_one_is_counted() {
    let s = ship();
    let mut a = auditor(&s);
    let region = [
        proc(1, 0x100, 0x40, "f"),
        label(0x0209, 1, 0x120, "$L1"),
        label(0x0209, 1, 0x200, "$L2"),
        label(0x0209, 0, 0, "$L3"),
        end(),
    ]
    .concat();
    a.region(&region, "test", Some(&module(0, "a.obj")));
    assert_eq!(a.tally.failed("module S_GPROC32"), 0);
    assert_eq!(
        a.tally.failed("module S_LABEL32"),
        1,
        "{:?}",
        a.tally.examples
    );
    assert_eq!(a.tally.retired_of("module S_LABEL32"), 1);
}

#[test]
fn a_label_at_the_procedures_end_and_an_overlong_block_fail() {
    let s = ship();
    let mut a = auditor(&s);
    let region = [
        proc(1, 0x100, 0x40, "f"),
        label(0x0209, 1, 0x13f, "$Llast"),
        label(0x0209, 1, 0x140, "$Lpast"),
        block(1, 0x110, 0x30),
        end(),
        block(1, 0x110, 0x10000),
        end(),
        end(),
    ]
    .concat();
    a.region(&region, "test", Some(&module(0, "a.obj")));
    assert_eq!(a.tally.ok_of("module S_LABEL32"), 1);
    assert_eq!(a.tally.failed("module S_LABEL32"), 1);
    assert_eq!(a.tally.ok_of("module S_BLOCK32"), 1);
    assert_eq!(
        a.tally.failed("module S_BLOCK32"),
        1,
        "{:?}",
        a.tally.examples
    );
}

#[test]
fn a_live_child_under_a_retired_procedure_fails() {
    let s = ship();
    let mut a = auditor(&s);
    let region = [proc(0, 0, 0x40, "f"), block(1, 0x110, 4), end(), end()].concat();
    a.region(&region, "test", Some(&module(0, "a.obj")));
    assert_eq!(a.tally.retired_of("module S_GPROC32"), 1);
    assert_eq!(a.tally.failed("module S_BLOCK32"), 1);
}

#[test]
fn a_procedure_on_another_function_fails_by_name() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x200, 0x20, "f"), end()].concat(),
        "test",
        Some(&module(0, "b.obj")),
    );
    assert_eq!(a.tally.failed("module S_GPROC32"), 1);
    assert!(a.tally.examples["module S_GPROC32"][0].contains("not this name"));
}

#[test]
fn a_procedure_on_another_objects_same_named_function_is_a_wrong_tu() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x200, 0x20, "g"), end()].concat(),
        "test",
        Some(&module(0, "a.obj")),
    );
    assert_eq!(a.tally.failed("module S_GPROC32"), 1);
    assert!(a.tally.examples["module S_GPROC32"][0].contains("WRONG TU"));
}

#[test]
fn a_procedure_reaching_into_the_next_function_fails() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x100, 0x101, "f"), end()].concat(),
        "test",
        Some(&module(0, "a.obj")),
    );
    assert_eq!(a.tally.failed("module S_GPROC32"), 1);
}

#[test]
fn a_name_the_oracle_does_not_carry_fails_without_our_link() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x100, 0x40, "h"), end()].concat(),
        "test",
        Some(&module(0, "a.obj")),
    );
    assert_eq!(a.tally.failed("module S_GPROC32"), 1);
    assert_eq!(a.tally.folded_of("module S_GPROC32"), 0);
}

#[test]
fn a_public_on_another_function_and_a_global_moved_into_code_fail() {
    let s = ship();
    let mut a = auditor(&s);
    let r = [
        named(0x1009, 1, 0x100, "?f@@YAXXZ"),
        named(0x1009, 1, 0x200, "?f@@YAXXZ"),
        named(0x1008, 2, 0x10, "d"),
        named(0x1008, 1, 0x240, "d"),
    ]
    .concat();
    a.region(&r, "test", None);
    assert_eq!(a.tally.ok_of("symrecs S_PUB32"), 1);
    assert_eq!(a.tally.failed("symrecs S_PUB32"), 1);
    assert_eq!(a.tally.ok_of("symrecs S_GDATA32"), 1);
    assert_eq!(a.tally.failed("symrecs S_GDATA32"), 1);
}

#[test]
fn an_unclassified_kind_fails_rather_than_passing_unseen() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(&label(0x1105, 1, 0x120, "$L1"), "test", None);
    assert_eq!(a.tally.failed("unclassified record kind"), 1);
}

#[test]
fn a_half_retired_field_fails() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(&label(0x0209, 0, 0x120, "$L1"), "test", None);
    assert_eq!(a.tally.failed("symrecs S_LABEL32"), 1);
}

#[test]
fn a_walk_that_stops_early_fails() {
    let s = ship();
    let mut a = auditor(&s);
    let mut r = label(0x0209, 1, 0x120, "$L1");
    r.extend_from_slice(&[0xff, 0x00, 0x09, 0x02, 0, 0, 0, 0]);
    a.region(&r, "test", None);
    assert_eq!(a.tally.failed("record walk"), 1);
}

fn fpo_entry(start: u32, size: u32, locals: u32) -> Vec<u8> {
    let mut e = Vec::new();
    e.extend_from_slice(&start.to_le_bytes());
    e.extend_from_slice(&size.to_le_bytes());
    e.extend_from_slice(&locals.to_le_bytes());
    e.extend_from_slice(&1u16.to_le_bytes());
    e.extend_from_slice(&0u16.to_le_bytes());
    e
}

#[test]
fn fpo_must_be_sorted_and_start_its_own_procedure() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x100, 0x40, "f"), end()].concat(),
        "test",
        Some(&module(0, "a.obj")),
    );
    a.region(
        &[proc(1, 0x200, 0x20, "g"), end()].concat(),
        "test",
        Some(&module(1, "b.obj")),
    );
    let stream = [
        fpo_entry(0, 0, 0),
        fpo_entry(0x1100, 0x40, 0),
        fpo_entry(0x1200, 0x40, 0),
        fpo_entry(0x1240, 0x8, 0),
        fpo_entry(0x1100, 0x40, 0),
    ]
    .concat();
    fpo(&mut a, &stream);
    assert_eq!(a.tally.retired_of("FPO"), 1);
    assert_eq!(a.tally.ok_of("FPO"), 2);
    assert_eq!(a.tally.failed("FPO"), 2, "{:?}", a.tally.examples);
    assert_eq!(a.tally.failed("FPO sorted by start"), 1);
}

#[test]
fn a_contribution_must_be_its_modules_own_code() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x100, 0x40, "f"), end()].concat(),
        "test",
        Some(&module(0, "a.obj")),
    );
    let m = module(0, "a.obj");
    assert!(a.contrib("c", 1, 0x100, 0x40, Some(&m)).is_some());
    assert!(a.contrib("c", 1, 0x200, 0x20, Some(&m)).is_none());
    assert!(a.contrib("c", 2, 0x10, 4, Some(&m)).is_some());
    assert!(a.contrib("c", 2, 0x80, 4, Some(&m)).is_none());
    a.contrib("c", 0, 0, 0, None);
    a.contrib("c", 0, 0x40, 0, None);
    assert_eq!(a.tally.ok_of("c"), 2);
    assert_eq!(a.tally.retired_of("c"), 1);
    assert_eq!(a.tally.failed("c"), 3, "{:?}", a.tally.examples);
}

fn c11_region(seg: u16, files: &[(&str, &[(u32, u16)])], range: (u32, u32)) -> Vec<u8> {
    let u32_of = |v: usize| u32::try_from(v).unwrap();
    let pad4 = |b: &mut Vec<u8>| {
        while !b.len().is_multiple_of(4) {
            b.push(0);
        }
    };
    let mut b = Vec::new();
    b.extend_from_slice(&u16::try_from(files.len()).unwrap().to_le_bytes());
    b.extend_from_slice(&1u16.to_le_bytes());
    let table = b.len();
    b.resize(table + 4 * files.len(), 0);
    b.extend_from_slice(&range.0.to_le_bytes());
    b.extend_from_slice(&range.1.to_le_bytes());
    b.extend_from_slice(&seg.to_le_bytes());
    pad4(&mut b);
    for (i, (name, pairs)) in files.iter().enumerate() {
        let base = b.len();
        b[table + 4 * i..table + 4 * i + 4].copy_from_slice(&u32_of(base).to_le_bytes());
        b.extend_from_slice(&1u16.to_le_bytes());
        b.extend_from_slice(&0u16.to_le_bytes());
        let ln_at = b.len();
        b.extend_from_slice(&0u32.to_le_bytes());
        let start = pairs.first().map_or(range.0, |p| p.0);
        b.extend_from_slice(&start.to_le_bytes());
        b.extend_from_slice(&range.1.to_le_bytes());
        b.push(u8::try_from(name.len()).unwrap());
        b.extend_from_slice(name.as_bytes());
        pad4(&mut b);
        let at = b.len();
        b[ln_at..ln_at + 4].copy_from_slice(&u32_of(at).to_le_bytes());
        b.extend_from_slice(&seg.to_le_bytes());
        b.extend_from_slice(&u16::try_from(pairs.len()).unwrap().to_le_bytes());
        for (o, _) in *pairs {
            b.extend_from_slice(&o.to_le_bytes());
        }
        for (_, l) in *pairs {
            b.extend_from_slice(&l.to_le_bytes());
        }
        pad4(&mut b);
    }
    b
}

#[test]
fn under_a_line_directive_the_lines_are_the_tus_physical_ones() {
    let mut s = ship();
    let mut src: Vec<String> = (1..=931).map(|i| format!("x{i};")).collect();
    src.push("#line 1458 \"C:\\\\A\\\\T.cpp\"".into());
    src.extend((933..=940).map(|i| format!("y{i};")));
    s.line_dirs.insert(
        "a.obj".into(),
        crate::linedir::LineDirectives::parse(&src.join("\n"), "T.cpp")
            .unwrap()
            .unwrap(),
    );
    let region = |own: [u16; 2], alias: [u16; 2]| {
        c11_region(
            1,
            &[
                ("c:\\a\\src\\t.cpp", &[(0x100, own[0]), (0x108, own[1])]),
                ("C:\\A\\T.cpp", &[(0x110, alias[0]), (0x118, alias[1])]),
            ],
            (0x100, 0x13f),
        )
    };
    let ours = region([10, 11], [1459, 1460]);
    let audit = |b: &[u8]| {
        let mut a = auditor(&s);
        let m = module(0, "a.obj");
        a.own_code.insert(0, vec![(1, 0x100, 0x40)]);
        a.our_procs.insert(0, vec![(1, 0x100, 0x40)]);
        a.moved.insert(0, vec![(1, 0x100, 0x40, 1, 0x100)]);
        a.c11_from(b, "test", &m, Some(&ours));
        (
            a.tally.ok_of("C11 lines = --original, moved"),
            a.tally.failed("C11 lines = --original, moved"),
        )
    };
    assert_eq!(audit(&region([10, 11], [934, 935])), (2, 0), "physical");
    assert_eq!(audit(&region([10, 11], [1459, 1460])).1, 1, "left presumed");
    assert_eq!(audit(&region([10, 11], [934, 936])).1, 1, "off by one");
    assert_eq!(
        audit(&region([11, 12], [934, 935])).1,
        1,
        "the TU's own record renumbered"
    );
}

#[test]
fn a_c11_block_must_sit_in_its_own_procedure() {
    let s = ship();
    let mut a = auditor(&s);
    let m = module(0, "a.obj");
    a.region(
        &[proc(1, 0x100, 0x40, "f"), end()].concat(),
        "test",
        Some(&m),
    );
    let mut b = Vec::new();
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&0u16.to_le_bytes());
    b.extend_from_slice(&8u32.to_le_bytes());
    let blocks_at = 8 + 4 + 8 + 16;
    b.extend_from_slice(&2u16.to_le_bytes());
    b.extend_from_slice(&0u16.to_le_bytes());
    b.extend_from_slice(&u32::try_from(blocks_at).unwrap().to_le_bytes());
    b.extend_from_slice(&u32::try_from(blocks_at + 16).unwrap().to_le_bytes());
    b.extend_from_slice(&0x100u32.to_le_bytes());
    b.extend_from_slice(&0x13fu32.to_le_bytes());
    b.extend_from_slice(&0x200u32.to_le_bytes());
    b.extend_from_slice(&0x21fu32.to_le_bytes());
    assert_eq!(b.len(), blocks_at);
    for start in [0x100u32, 0x200] {
        b.extend_from_slice(&1u16.to_le_bytes());
        b.extend_from_slice(&2u16.to_le_bytes());
        b.extend_from_slice(&start.to_le_bytes());
        b.extend_from_slice(&(start + 8).to_le_bytes());
        b.extend_from_slice(&[1, 0, 2, 0]);
    }
    a.c11(&b, "test", &m);
    assert_eq!(a.tally.ok_of("C11 block"), 1);
    assert_eq!(a.tally.failed("C11 block"), 1, "{:?}", a.tally.examples);
}

#[test]
fn with_the_original_a_contribution_is_the_one_our_link_recorded() {
    let s = ship();
    let mut a = auditor(&s);
    let m = module(0, "a.obj");
    a.region(
        &[proc(1, 0x100, 0x40, "f"), end()].concat(),
        "test",
        Some(&m),
    );
    let mut before = Before::default();
    before.contribs.insert((0, 0x5100), vec![0x50, 0x40]);
    a.before = Some(before);
    assert!(a.contrib("c", 1, 0x100, 0x40, Some(&m)).is_some());
    assert!(
        a.contrib("c", 1, 0x100, 0x40, Some(&module(1, "b.obj")))
            .is_none()
    );
    assert!(a.contrib("c", 1, 0x100, 0x20, Some(&m)).is_none());
    assert!(a.contrib("c", 1, 0x100, 0x40, Some(&m)).is_some());
    assert!(a.contrib("c", 1, 0x100, 0x40, Some(&m)).is_none());
    let mut before = Before::default();
    before.contribs.insert((0, 0x5100), vec![0x10]);
    a.before = Some(before);
    assert!(a.contrib("c", 1, 0x100, 0x20, Some(&m)).is_none());
    assert_eq!(a.tally.ok_of("c"), 2);
    assert_eq!(a.tally.failed("c"), 4, "{:?}", a.tally.examples);
}

#[test]
fn with_the_original_an_fpo_entry_is_the_one_our_link_wrote() {
    let s = ship();
    let mut a = auditor(&s);
    a.region(
        &[proc(1, 0x100, 0x40, "f"), end()].concat(),
        "test",
        Some(&module(0, "a.obj")),
    );
    let mut before = Before::default();
    before
        .fpo
        .insert(0x5100, vec![(0x40, vec![2, 0, 0, 0, 1, 0, 0, 0])]);
    a.before = Some(before);
    fpo(
        &mut a,
        &[fpo_entry(0x1100, 0x40, 2), fpo_entry(0x1100, 0x40, 3)].concat(),
    );
    assert_eq!(a.tally.ok_of("FPO"), 1);
    assert_eq!(a.tally.failed("FPO"), 1, "{:?}", a.tally.examples);
}

#[test]
fn two_modules_at_one_function_fold_only_on_a_link_that_can() {
    for (may, failed, folded) in [(true, 0, 1), (false, 1, 0)] {
        let mut s = ship();
        s.may_fold = may;
        let mut a = auditor(&s);
        for (i, obj) in [(0, "a.obj"), (1, "b.obj")] {
            a.region(
                &[proc(1, 0x100, 0x40, "f"), end()].concat(),
                "test",
                Some(&module(i, obj)),
            );
        }
        a.cross_module();
        let class = "procedure claimed by two modules";
        assert_eq!(a.tally.failed(class), failed, "may_fold {may}");
        assert_eq!(a.tally.folded_of(class), folded, "may_fold {may}");
    }
}

#[test]
fn only_a_vector_deleting_destructor_has_a_weak_default() {
    assert_eq!(
        weak_default(&name_key("??_EPalette@@UAEPAXI@Z")),
        Some(name_key("??_GPalette@@UAEPAXI@Z"))
    );
    assert_eq!(weak_default(&name_key("??_GPalette@@UAEPAXI@Z")), None);
    assert_eq!(weak_default(&name_key("?f@@YAXXZ")), None);
}

#[test]
fn a_record_moved_into_another_functions_body_fails() {
    let mut s = ship();
    s.exact = vec![(0x1100, 0x40, 0x5100, "f".to_string())];
    s.index();
    assert!(s.moved_into(0x1110, 0x5110).is_none());
    assert!(s.moved_into(0x1110, 0x9000).is_some());
    assert!(s.moved_into(0x1100, 0x9000).is_none());
    assert!(s.moved_into(0x1140, 0x9000).is_none());
}

#[test]
fn a_nameless_datum_that_moved_with_its_byte_identical_body_is_accepted() {
    let ours: &'static Image = Box::leak(Box::new(
        Image::parse(msvc6_fixture::sample::sample().pe.build()).unwrap(),
    ));
    let text = ours.sections.first().unwrap().rva.get();
    let mut s = ship();
    s.ours = Some(Ours {
        img: ours,
        by_key: BTreeMap::new(),
    });
    s.exact = vec![(0x1100, 0x40, text + 0x200, "f".to_string())];
    s.index();
    let moved = named(0x1007, 1, 0x110, "");
    let judged = |orig_off: u32| {
        let mut a = auditor(&s);
        let orig = named(0x1007, 1, orig_off, "");
        a.region_from(&moved, "test", None, Some(&orig));
        (
            a.tally.ok_of("symrecs S_LDATA32"),
            a.tally.failed("symrecs S_LDATA32"),
        )
    };
    assert_eq!(judged(0x210), (1, 0), "the jump table moved with its body");
    assert_eq!(judged(0x300), (0, 1), "the body moved without it");
    let mut a = auditor(&s);
    a.region(&moved, "test", None);
    assert_eq!(
        a.tally.failed("symrecs S_LDATA32"),
        1,
        "with nothing to say where it was, a nameless datum names nothing"
    );
}

#[test]
fn a_record_differing_from_the_original_outside_its_address_fails() {
    let s = ship();
    let orig = [proc(1, 0x200, 0x40, "f"), end()].concat();
    let mut a = auditor(&s);
    let moved = [proc(1, 0x100, 0x40, "f"), end()].concat();
    a.region_from(&moved, "test", Some(&module(0, "a.obj")), Some(&orig));
    assert_eq!(a.tally.failed("record bytes = --original"), 0);
    let mut a = auditor(&s);
    let longer = [proc(1, 0x100, 0x41, "f"), end()].concat();
    a.region_from(&longer, "test", Some(&module(0, "a.obj")), Some(&orig));
    assert_eq!(a.tally.failed("record bytes = --original"), 1);
}

#[test]
fn a_type_index_resolves_only_where_a_type_is() {
    let t = TypeRange {
        min: 0x1000,
        mac: 0x1200,
    };
    assert!(t.resolves(0x0074));
    assert!(t.resolves(0x11ff));
    assert!(!t.resolves(0x1200));
    assert!(!t.resolves(0x0100_1001));
    assert!(!t.resolves(0x0200_1001));
}

#[test]
fn a_shorter_length_is_a_clamp_only_at_a_body_or_section_end() {
    let s = ship();
    let a = auditor(&s);
    assert!(a.clamp_ok(0x1100, 0x40, 0x5100));
    assert!(a.clamp_ok(0x1110, 0x30, 0x5110));
    assert!(!a.clamp_ok(0x1100, 0x1, 0x5100));
    assert!(!a.clamp_ok(0x1100, 0x40, 0x9000));
    assert!(a.clamp_ok(0x1f00, 0x100, 0x1234));
}

#[test]
fn a_path_naming_a_user_is_found_in_either_encoding() {
    let wide = |s: &str| s.bytes().flat_map(|c| [c, 0]).collect::<Vec<u8>>();
    let mut file = b"x\0C:\\users\\wine\\Temp\\lnk2&\0".to_vec();
    file.extend(wide("D:\\Users\\Annik\\src\\a.cpp"));
    file.extend_from_slice(b"\0C:\\Documents and Settings\\pat\\x\0Z:\\tmp\\y");
    let lower: Vec<u8> = file.iter().map(u8::to_ascii_lowercase).collect();
    let names: Vec<(bool, String)> = user_profiles(&lower)
        .into_iter()
        .map(|(_, w, n)| (w, n))
        .collect();
    assert_eq!(
        names,
        [
            (false, "wine".to_string()),
            (true, "annik".to_string()),
            (false, "pat".to_string())
        ]
    );
    assert_eq!(find_text(&lower, "z:\\").len(), 1);
    assert_eq!(find_text(&lower, "annik"), [(45, true)]);
    assert_eq!(shown(&file, 45, true), "Annik\\src\\a.cpp");
    assert!(whole_token(&lower, 45, 5, true));
    let t = b"x numeric eric dev-box-eric-01 device c:\\users\\eric\\";
    let hits: Vec<bool> = find_text(t, "eric")
        .into_iter()
        .map(|(at, w)| whole_token(t, at, 4, w))
        .collect();
    assert_eq!(hits, [false, true, true, true]);
    assert!(!whole_token(t, 31, 3, false), "`dev` inside `device`");
    assert!(whole_token(t, 15, 3, false), "`dev` before `-box`");
}

#[test]
fn the_host_path_needles_are_the_generic_roots_and_this_host_s_own() {
    use std::path::Path;
    assert_eq!(
        path_roots(&[
            Path::new("/srv/build/me"),
            Path::new("/Scratch/tmp"),
            Path::new("/home"),
            Path::new("relative/dir"),
            Path::new("/"),
        ]),
        ["/home/", "/scratch/", "/srv/"]
    );
    let needles = path_needles(&["/srv/".to_string(), "/HOME/".to_string()]);
    for n in ["z:\\", "/tmp/", "/home/", "/users/", "/private/", "/srv/"] {
        assert!(needles.iter().any(|x| x == n), "{n}: {needles:?}");
    }
    assert_eq!(needles.iter().filter(|x| *x == "/home/").count(), 1);
    let file = b"\0x:/srv/build/me/a.obj\0".to_ascii_lowercase();
    assert!(needles.iter().any(|n| !find_text(&file, n).is_empty()));
    assert!(
        path_needles(&[])
            .iter()
            .all(|n| find_text(&file, n).is_empty())
    );
}

#[test]
fn a_renamed_source_is_named_in_full_on_both_sides() {
    let region = |name: &str| {
        let mut b = vec![0u8; 36];
        b.push(u8::try_from(name.len()).unwrap());
        b.extend_from_slice(name.as_bytes());
        b.resize(b.len().next_multiple_of(4) + 8, 0);
        b
    };
    let (long, short) = ("app\\app.dll\\src\\Plot.cpp", "a\\x.cp");
    let m = renamed_source("plot.obj", 0, &region(long), &region(short), 36);
    assert!(
        m.contains(&format!("name is `{long}`, --original `{short}`")),
        "{m}"
    );
    let m = renamed_source("plot.obj", 0, &region(short), &region(long), 36);
    assert!(
        m.contains(&format!("name is `{short}`, --original `{long}`")),
        "{m}"
    );
}

#[test]
fn a_c11_file_name_is_outside_what_a_relocation_writes() {
    let mut b = Vec::new();
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&20u32.to_le_bytes());
    b.extend_from_slice(&0x10u32.to_le_bytes());
    b.extend_from_slice(&0x40u32.to_le_bytes());
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&[0, 0]);
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&[0, 0]);
    b.extend_from_slice(&44u32.to_le_bytes());
    b.extend_from_slice(&0x10u32.to_le_bytes());
    b.extend_from_slice(&0x40u32.to_le_bytes());
    b.push(6);
    b.extend_from_slice(b"a\\x.cp");
    b.push(0);
    assert_eq!(b.len(), 44);
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&2u16.to_le_bytes());
    b.extend_from_slice(&0x10u32.to_le_bytes());
    b.extend_from_slice(&0x20u32.to_le_bytes());
    b.extend_from_slice(&7u16.to_le_bytes());
    b.extend_from_slice(&8u16.to_le_bytes());
    let (mutable, names) = c11_layout(&b).unwrap();
    let written = |i: usize| mutable.iter().any(|(x, y)| i >= *x && i < *y);
    assert_eq!(names, [(36, 43, 0)]);
    assert!((36..44).all(|i| !written(i)));
    assert!((8..18).all(written));
    assert!((28..36).all(written));
    assert!((44..60).all(written));
    assert!([0, 2, 4, 18, 20, 22, 24].into_iter().all(|i| !written(i)));
}

#[test]
fn the_structure_is_judged_by_its_own_reader() {
    let run = |fx: &crate::structure::tests::Fixture| {
        let db = Msf2::parse(fx.file()).unwrap();
        let mut t = Tally::default();
        let mut out = Vec::new();
        super::file::structure(&mut t, &db, &mut out).unwrap();
        t
    };
    let fx = crate::structure::tests::Fixture::new();
    let t = run(&fx);
    assert_eq!(t.failures(), 0);
    assert_eq!(t.ok_of(crate::structure::END), 4);
    let mut poked = fx.clone();
    let g_end = poked.at["g.end"];
    poked.point("f", 4, g_end);
    let t = run(&poked);
    assert_eq!(t.failed(crate::structure::END), 1);
    assert_eq!(t.failures(), 1);
}
