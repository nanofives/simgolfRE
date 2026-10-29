#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use common::*;
use msvc6_base::srcio;
use msvc6_diff::comdat;
use msvc6_diff::env::Env;
use msvc6_diff::segments::{self, Analysis, MAX_SEGMENT, Verdict};
use msvc6_diff::{commswap, decisions, segasm};
use std::collections::HashSet;
use std::path::PathBuf;

fn analysed(e: &Env, f: &Fix, d: &[u8], tag: &str) -> (Analysis, PathBuf) {
    let dir = std::env::temp_dir().join(format!("msvc6-diff-segasm-{tag}-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("f.obj");
    srcio::write_bytes(&path, d).unwrap();
    let c = comdat::text_of(d).unwrap().remove(0);
    let a = segments::analyze_in(e, f.text, &path, c, MAX_SEGMENT).unwrap();
    (a, path)
}

fn listing_line<'a>(text: &'a str, ours_at: &str) -> &'a str {
    text.lines()
        .find(|l| l.contains(&format!("| {ours_at} ")))
        .unwrap_or_else(|| panic!("no row of ours at {ours_at} in\n{text}"))
}

#[test]
fn a_listing_names_each_operand_by_its_own_relocation_on_both_sides() {
    let f = fixture();
    let e = base_env(&f);
    let (a, path) = analysed(&e, &f, &object(), "names");
    assert!(a.segs.iter().all(|s| s.verdict == Verdict::Exact));
    let all: Vec<usize> = (0..a.segs.len()).collect();
    let text = segasm::report(&e, &a, &all, &segasm::definers(std::slice::from_ref(&path)));
    let load = listing_line(&text, "+0003");
    assert!(load.starts_with("  "), "{load}");
    assert_eq!(load.matches("; _g").count(), 2, "{load}");
    let push = listing_line(&text, "+0008");
    assert_eq!(push.matches("; \"hello\"").count(), 2, "{push}");
    let call = listing_line(&text, "+000d");
    assert_eq!(call.matches("; _callee").count(), 2, "{call}");
    let jump = listing_line(&text, "+001d");
    assert!(jump.ends_with("; .text+0x2c"), "{jump}");
    assert!(
        text.contains("names: 0 paired row(s) of the function address alike and read two ways\n"),
        "{text}"
    );
    assert!(
        text.contains(
            "names: 0 name(s) shown for a relocation of ours that are not its own symbol\n"
        ),
        "{text}"
    );
    assert!(text.starts_with("== seg 0  image "), "{text}");
    assert_eq!(segasm::regmap_text(&a, &a.segs[0]), "-");
    std::fs::remove_dir_all(path.parent().unwrap()).ok();
}

#[test]
fn a_register_swap_reads_as_a_rename_and_is_charged_to_the_rotation_pick() {
    let f = fixture();
    let e = base_env(&f);
    let mut c = code();
    assert_eq!(c[0x16], 0x45);
    c[0x16] = 0x4d;
    let (a, path) = analysed(&e, &f, &object_with(c, 0), "swap");
    assert_eq!(a.segs.len(), 1);
    assert_eq!(a.segs[0].verdict, Verdict::RegOnly);
    assert_eq!(a.segs[0].edit, 1);
    let regmap = segasm::regmap_text(&a, &a.segs[0]);
    let va = segments::off(&a.img, 0) as u64 + a.start + 0x15;
    assert!(
        regmap.starts_with(&format!("ecx->eax@{va:#x} 1/1")),
        "{regmap}"
    );
    let offs: HashSet<usize> = commswap::find(&a).iter().map(|s| s.img_off).collect();
    let charges = decisions::find(&a, &offs);
    assert_eq!(charges.len(), 1, "{charges:?}");
    assert_eq!(charges[0].class, "rotation pick");
    assert_eq!(charges[0].bytes, 1);
    assert_eq!(charges[0].first, format!("{va:#x}"));
    let rows = [(f.text, charges[0].clone())];
    let tsv = decisions::tsv(&rows, &|_| "?f@@YAXXZ".to_string());
    assert_eq!(
        tsv.lines().nth(1),
        Some(
            format!(
                "{:#08x}\t?f@@YAXXZ\t0\t{:#x}\tREG-ONLY\t1\trotation pick\t1\t1\t{va:#x}\t",
                f.text, a.start
            )
            .as_str()
        )
    );
    let text = segasm::report(&e, &a, &[0], &segasm::definers(std::slice::from_ref(&path)));
    let swapped = listing_line(&text, "+0015");
    assert!(swapped.starts_with("~ "), "{swapped}");
    std::fs::remove_dir_all(path.parent().unwrap()).ok();
}
