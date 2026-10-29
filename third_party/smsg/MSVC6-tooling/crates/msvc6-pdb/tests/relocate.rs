#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::backstop;
use msvc6_image::pe::Image;
use msvc6_pdb::audit;
use msvc6_pdb::author::Tsv;
use msvc6_pdb::linedir::{self, ByObject, LineDirectives, TuSet};
use msvc6_pdb::msf::Msf2;
use msvc6_pdb::pair;
use msvc6_pdb::reloc::{self, Inputs, Pairing};
use msvc6_pdb::report::Report;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::time::Duration;

const WORK_PER_BYTE: usize = 8;
const WORK_FLOOR: usize = 4096;
const PER_STREAM: usize = 48;

fn work_cap(len: usize) -> usize {
    len.saturating_mul(WORK_PER_BYTE).max(WORK_FLOOR)
}

struct Fx {
    image: Image,
    pdb: Vec<u8>,
    report: Report,
}

fn report_json(s: &msvc6_fixture::pdb::Sample, base: u32) -> String {
    let text = s.procs.first().map_or(0, |p| p.1);
    let data = s
        .publics
        .iter()
        .find(|p| p.0 == "_g_count")
        .map_or(0, |p| p.1);
    let mut named: Vec<(String, u32, u32, &str)> = s
        .procs
        .iter()
        .map(|(name, rva, len)| {
            let obj = if name == "_main" { "b.obj" } else { "a.obj" };
            (name.clone(), *rva, *len, obj)
        })
        .collect();
    named.push(("[thunk]:_f`adjustor{4}'".into(), text + 0x30, 5, "a.obj"));
    named.push(("_vcall".into(), text + 0x38, 6, "a.obj"));
    named.push(("_g_count".into(), data, 4, "a.obj"));
    named.push(("_s_static".into(), data + 0x10, 4, "a.obj"));
    let rows: Vec<serde_json::Value> = named
        .iter()
        .map(|(name, rva, len, obj)| {
            serde_json::json!({
                "address": format!("{:#x}", base + rva),
                "recomp": format!("{:#x}", base + rva),
                "name": name.trim_start_matches('_'),
                "msvc6t": {"symbol": name, "obj": obj, "size": len, "edit": 0},
            })
        })
        .collect();
    serde_json::json!({"file": "sample.dll", "data": rows}).to_string()
}

fn fx() -> Fx {
    let s = msvc6_fixture::pdb::sample();
    let image = Image::parse(s.image.clone()).unwrap();
    let base = image.opt.image_base.get();
    let report = Report::parse(report_json(&s, base).as_bytes(), "msvc6t", "report").unwrap();
    Fx {
        image,
        pdb: s.bytes,
        report,
    }
}

struct Ran {
    relocated: Option<Vec<u8>>,
    work: usize,
    failures: Option<usize>,
    log: String,
}

fn relocate_and_audit(fx: &Fx, report: &Report, pdb: &[u8], pairs: Option<&Path>) -> Ran {
    let names = BTreeMap::new();
    let scoped = BTreeMap::new();
    let dropped = BTreeMap::new();
    let dirs: ByObject = BTreeMap::new();
    let i = Inputs {
        name: "FX",
        reference: &fx.image,
        ours: &fx.image,
        report,
        report_label: "report",
        map: None,
        names: &names,
        scoped: &scoped,
        dropped: &dropped,
        dirs: &dirs,
        policy: None,
        pairs,
        no_derive: false,
    };
    let mut ran = Ran {
        relocated: None,
        work: 0,
        failures: None,
        log: String::new(),
    };
    let Ok(mut db) = Msf2::parse(pdb.to_vec()) else {
        return ran;
    };
    let mut sink = Vec::new();
    let Ok(done) = reloc::relocate(&i, &mut db, &mut sink) else {
        return ran;
    };
    let s = &done.score;
    ran.work = s.records + s.patched + s.retired + s.lines + s.ranges + s.retired_list.len();
    ran.work += done.derived.as_ref().map_or(0, |d| d.pairs.len());
    let out = db.bytes().to_vec();
    let host: Vec<String> = Vec::new();
    let retired = done.retired_tsv();
    let pairs_text = pairs.and_then(|p| msvc6_base::srcio::own_read(p).ok());
    let a = audit::Inputs {
        image: &fx.image,
        image_label: "fx.dll",
        report: Some((report, "report")),
        expect: None,
        pairs: pairs_text.as_deref().map(|t| (t, "pairs")),
        retired: Some(&retired),
        ours: Some((&fx.image, &[], "ours")),
        line_dirs: &dirs,
        policy: None,
        host: &host,
        host_paths: &host,
        examples: 2,
    };
    if let (Ok(relocated), Ok(original)) = (Msf2::parse(out.clone()), Msf2::parse(pdb.to_vec()))
        && let Ok(t) = audit::run(&a, "fx.pdb", &relocated, Some(original), &mut sink)
    {
        ran.work += t
            .rows
            .values()
            .map(|r| r.ok + r.retired + r.folded + r.linked + r.failed)
            .sum::<usize>();
        ran.failures = Some(t.failures());
        let _ = t.print(&mut sink);
    }
    ran.log = msvc6_base::srcio::tool_text(&sink);
    ran.relocated = Some(out);
    ran
}

struct Sweep {
    cases: usize,
    slowest: (Duration, String),
    heaviest: (usize, String),
}

impl Sweep {
    fn new() -> Self {
        Self {
            cases: 0,
            slowest: (Duration::ZERO, String::new()),
            heaviest: (0, String::new()),
        }
    }

    fn case(&mut self, fx: &Fx, report: &Report, what: &str, pdb: &[u8]) -> Ran {
        let (r, took) = backstop::run(what, || relocate_and_audit(fx, report, pdb, None));
        self.cases += 1;
        if took > self.slowest.0 {
            self.slowest = (took, what.to_string());
        }
        if r.work > self.heaviest.0 {
            self.heaviest = (r.work, what.to_string());
        }
        let cap = work_cap(pdb.len());
        assert!(
            r.work <= cap,
            "{what}: relocate and audit did {} items of work on {} bytes, past {cap}",
            r.work,
            pdb.len()
        );
        if let Some(out) = &r.relocated {
            assert_eq!(
                out.len(),
                pdb.len(),
                "{what}: a relocation is length-preserving"
            );
        }
        r
    }

    fn report(&self, test: &str) {
        eprintln!(
            "{test}: {} case(s); the slowest {:?} ({}); the heaviest {} items ({})",
            self.cases, self.slowest.0, self.slowest.1, self.heaviest.0, self.heaviest.1
        );
    }
}

fn positions(file: &[u8]) -> Vec<usize> {
    let mut out: Vec<usize> = (0..file.len().min(64)).step_by(4).collect();
    let m = Msf2::parse(file.to_vec()).unwrap();
    let ps = m.page_size;
    for p in m.dir_pages().unwrap_or_default() {
        out.extend((p * ps..(p + 1) * ps).step_by(4).take(64));
    }
    for s in &m.streams {
        let mut at: Vec<usize> = Vec::new();
        for (k, p) in s.pages.iter().enumerate() {
            let len = s.size().saturating_sub(k * ps).min(ps);
            at.extend((p * ps..p * ps + len).step_by(2));
        }
        let every = at.len().div_ceil(PER_STREAM).max(1);
        out.extend(at.into_iter().step_by(every));
    }
    out.sort_unstable();
    out.dedup();
    out
}

#[test]
fn the_fixture_pdb_relocated_onto_its_own_image_fails_only_its_line_outside_every_procedure() {
    let fx = fx();
    let t = Tree::new("baseline", &[]);
    std::fs::create_dir_all(&t.0).unwrap();
    let pairs = t.0.join("fx.pairs.tsv");
    let (r, _) = backstop::run("the fixture", || {
        relocate_and_audit(&fx, &fx.report, &fx.pdb, Some(&pairs))
    });
    assert!(r.relocated.is_some(), "the fixture relocates");
    assert!(r.work > 0);
    let fails: Vec<&str> = r
        .log
        .lines()
        .filter(|l| l.trim_start().starts_with("FAIL "))
        .collect();
    assert_eq!(r.failures, Some(fails.len()), "{}", r.log);
    assert_eq!(fails.len(), 3, "{}", r.log);
    for f in fails {
        assert!(
            f.contains("1:0x30") || f.contains("1:0x0..0x33"),
            "a failure other than the fixture's line at 1:0x30, which lies in a thunk: {f}"
        );
    }
}

#[test]
fn every_word_of_the_fixture_pdb_set_to_an_edge_value_is_relocated_audited_or_refused_within_bounds()
 {
    let fx = fx();
    let mut sw = Sweep::new();
    let base = fx.pdb.clone();
    let len = u32::try_from(base.len()).unwrap();
    for at in positions(&base) {
        for v in [u32::MAX, 0x7fff_ffff, 0xffff, 0x1000, len, 0] {
            let mut b = base.clone();
            if let Some(w) = b.get_mut(at..at + 4) {
                w.copy_from_slice(&v.to_le_bytes());
            }
            sw.case(&fx, &fx.report, &format!("u32 {v:#x} at {at:#x}"), &b);
        }
    }
    for n in (0..base.len()).step_by(997) {
        sw.case(&fx, &fx.report, &format!("{n} bytes"), &base[..n]);
    }
    assert!(sw.cases > 1000, "{} cases", sw.cases);
    sw.report("edge values");
}

#[test]
fn a_hostile_report_is_read_or_refused_and_relocates_within_bounds() {
    let fx = fx();
    let mut sw = Sweep::new();
    let deep = format!("{}{}", "[".repeat(100_000), "]".repeat(100_000));
    let many = {
        let row = r#"{"address":"0x10001000","recomp":"0x10001000","name":"f","msvc6t":{"obj":"a.obj","size":4294967295}}"#;
        let rows = vec![row; 50_000].join(",");
        format!(r#"{{"data":[{rows}]}}"#)
    };
    let distinct = |edit: &str| {
        let rows: Vec<String> = (0..50_000u32)
            .map(|i| {
                format!(
                    r#"{{"address":"0x10001000","recomp":"{:#x}","name":"f{i}","msvc6t":{{"obj":"a.obj","size":{}{edit}}}}}"#,
                    0x1000_1000 + i,
                    0x10_0000 - i
                )
            })
            .collect();
        format!(r#"{{"data":[{}]}}"#, rows.join(","))
    };
    let cases: Vec<(&str, String)> = vec![
        ("empty", String::new()),
        ("not JSON", "\u{0}\u{1}{".into()),
        ("an array", "[]".into()),
        ("data is a number", r#"{"data":5}"#.into()),
        ("data of scalars", r#"{"data":[5,"x",null,true]}"#.into()),
        (
            "unparsable addresses",
            r#"{"data":[{"address":"0xzz","recomp":"--","name":"f"}]}"#.into(),
        ),
        (
            "addresses past 32 bits",
            r#"{"data":[{"address":"0xffffffffffffffffff","recomp":"0x1ffffffff","msvc6t":{"size":-5}}]}"#
                .into(),
        ),
        (
            "an extension of the wrong type",
            r#"{"data":[{"address":"0x10001000","recomp":"0x10001000","msvc6t":5}]}"#.into(),
        ),
        (
            "a size past the image",
            r#"{"data":[{"address":"0x10001000","recomp":"0x10001000","msvc6t":{"size":4294967295}}]}"#
                .into(),
        ),
        ("deep nesting", deep),
        ("one row fifty thousand times", many),
        (
            "fifty thousand distinct rows over one address",
            distinct(""),
        ),
        (
            "fifty thousand distinct exact rows over one address",
            distinct(",\"edit\":0"),
        ),
    ];
    for (what, text) in cases {
        let (parsed, _) = backstop::run(what, || Report::parse(text.as_bytes(), "msvc6t", what));
        let Ok(r) = parsed else {
            continue;
        };
        let rows = text.len();
        assert!(r.rows.len() <= rows, "{what}: more rows than bytes");
        let base = fx.image.opt.image_base.get();
        let (p, _) = backstop::run(what, || Pairing::from_rows(&r.rows, base));
        assert!(p.by_ours.len() <= r.rows.len());
        sw.case(&fx, &r, what, &fx.pdb);
    }
    sw.report("hostile reports");
}

fn relocated_with_pairs(fx: &Fx, pairs: &Path) -> (Vec<u8>, String, String) {
    let names = BTreeMap::new();
    let scoped = BTreeMap::new();
    let dropped = BTreeMap::new();
    let dirs: ByObject = BTreeMap::new();
    let i = Inputs {
        name: "FX",
        reference: &fx.image,
        ours: &fx.image,
        report: &fx.report,
        report_label: "report",
        map: None,
        names: &names,
        scoped: &scoped,
        dropped: &dropped,
        dirs: &dirs,
        policy: None,
        pairs: Some(pairs),
        no_derive: false,
    };
    let mut db = Msf2::parse(fx.pdb.clone()).unwrap();
    let done = reloc::relocate(&i, &mut db, &mut Vec::new()).unwrap();
    (
        db.bytes().to_vec(),
        msvc6_base::srcio::own_read(pairs).unwrap(),
        done.retired_tsv(),
    )
}

fn audited(
    fx: &Fx,
    db: &[u8],
    tables: (Option<&str>, Option<&str>, Option<&str>),
) -> Option<usize> {
    let (pairs, retired, expect) = tables;
    let dirs: ByObject = BTreeMap::new();
    let none: Vec<String> = Vec::new();
    let a = audit::Inputs {
        image: &fx.image,
        image_label: "fx.dll",
        report: expect.is_none().then_some((&fx.report, "report")),
        expect: expect.map(|e| (e, "expect")),
        pairs: pairs.map(|p| (p, "pairs")),
        retired,
        ours: None,
        line_dirs: &dirs,
        policy: None,
        host: &none,
        host_paths: &none,
        examples: 2,
    };
    let db = Msf2::parse(db.to_vec()).ok()?;
    let t = audit::run(&a, "fx.pdb", &db, None, &mut Vec::new()).ok()?;
    Some(
        t.rows
            .values()
            .map(|r| r.ok + r.retired + r.folded + r.linked + r.failed)
            .sum(),
    )
}

#[test]
fn hostile_pairs_and_expect_tables_are_audited_or_refused_within_bounds() {
    let fx = fx();
    let t = Tree::new("tables", &[]);
    std::fs::create_dir_all(&t.0).unwrap();
    let (db, pairs, retired) = relocated_with_pairs(&fx, &t.0.join("fx.pairs.tsv"));
    let base = fx.image.opt.image_base.get();
    let text = fx
        .image
        .sections
        .iter()
        .find(|s| s.name == ".text")
        .map_or(0x1000, |s| s.rva.get());
    let n = 200_000u32;
    let many: String = (0..n)
        .map(|k| {
            format!(
                "{:#x}\t{:#x}\t1\t.text\tcontent\t\t\tp{k}\n",
                text + k,
                text + k
            )
        })
        .collect();
    let wide = format!(
        "{:#x}\t{:#x}\t4\t.text\tcode\t\t\t{}\n",
        text,
        text,
        vec!["n"; 100_000].join(" ")
    );
    let junk = "\t\t\t\n0xzz\t--\t-1\n0x1\t0x1\t4294967295\t\tcode\n\u{0}\u{1}\n".to_string();
    let pairs_cases: Vec<(&str, String)> = vec![
        ("200000 pairs rows", format!("{pairs}{many}")),
        ("a pairs row of 100000 names", format!("{pairs}{wide}")),
        ("junk pairs rows", format!("{pairs}{junk}")),
    ];
    for (what, table) in &pairs_cases {
        let (work, took) = backstop::run(what, || {
            audited(&fx, &db, (Some(table), Some(&retired), None))
        });
        let cap = work_cap(db.len() + table.len());
        assert!(
            work.is_some_and(|w| w <= cap),
            "{what}: {work:?} items of work, past {cap}"
        );
        eprintln!("{what}: {work:?} items in {took:?}");
    }
    let rows: String = (0..n)
        .map(|k| format!("{:08x}\tcode\tf{k}\n", base + text + k))
        .collect();
    let expect_cases: Vec<(&str, String)> = vec![
        ("200000 expect rows", format!("va\tkind\tnames\n{rows}")),
        (
            "an expect row of 100000 names",
            format!(
                "va\tkind\tnames\n{:08x}\tcode\t{}\n",
                base + text,
                vec!["n"; 100_000].join("|")
            ),
        ),
        ("junk expect rows", format!("va\tkind\tnames\n{junk}")),
    ];
    for (what, table) in &expect_cases {
        let (work, took) = backstop::run(what, || audited(&fx, &db, (None, None, Some(table))));
        let cap = work_cap(db.len() + table.len());
        assert!(
            work.is_none_or(|w| w <= cap),
            "{what}: {work:?} items of work, past {cap}"
        );
        eprintln!("{what}: {work:?} items in {took:?}");
    }
}

#[test]
fn a_tu_with_many_line_directives_is_read_in_one_pass() {
    let n = 200_000usize;
    let mut src = String::new();
    for i in 0..n {
        src.push_str(&format!("#line {} \"f{}.c\"\nint x{i};\n", i + 1, i % 7));
    }
    let (d, took) = backstop::run("many directives", || {
        LineDirectives::parse(&src, "big.c").unwrap().unwrap()
    });
    assert_eq!(d.files().len(), 7);
    assert_eq!(d.physical("f3.c", 4), Some(8));
    assert_eq!(d.physical("f3.c", 5), None);
    eprintln!("{n} directives parsed in {took:?}");
    let mut one = String::new();
    for i in 0..n {
        one.push_str(&format!("#line {} \"F.c\"\nint x{i};\n", i + 1));
    }
    let d = LineDirectives::parse(&one, "one.c").unwrap().unwrap();
    let (hits, took) = backstop::run("every line of one file under many directives", || {
        (1..=u16::MAX)
            .filter(|&l| {
                d.names("f.c") && d.physical("f.c", l).map(u32::from) == Some(2 * u32::from(l))
            })
            .count()
    });
    assert_eq!(hits, usize::from(u16::MAX / 2), "{took:?}");
    assert_eq!(d.physical("f.c", u16::MAX / 2 + 1), None, "past u16");
    eprintln!(
        "{} lines looked up over {n} directives in {took:?}",
        u16::MAX
    );
    assert!(LineDirectives::parse("#line 9\nint x;\n", "t.c").is_err());
    assert_eq!(LineDirectives::parse("int x;\n", "t.c").unwrap(), None);
}

struct Tree(PathBuf);

impl Tree {
    fn new(tag: &str, files: &[(&str, &str)]) -> Self {
        let dir = std::env::temp_dir().join(format!("msvc6t-reloc-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for (f, text) in files {
            let at = dir.join(f);
            std::fs::create_dir_all(at.parent().unwrap()).unwrap();
            msvc6_base::srcio::own_write(&at, text).unwrap();
        }
        Self(dir)
    }
}

impl Drop for Tree {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

fn tus(rows: &[(&str, &str)]) -> TuSet {
    let data: Vec<serde_json::Value> = rows
        .iter()
        .map(|(obj, tu)| serde_json::json!({"address": "0x1000", "msvc6t": {"obj": obj, "tu": tu}}))
        .collect();
    let r = Report::parse(
        serde_json::json!({ "data": data }).to_string().as_bytes(),
        "msvc6t",
        "r",
    )
    .unwrap();
    TuSet::of_report(&r)
}

fn no_includes(_: &Path) -> anyhow::Result<Vec<PathBuf>> {
    Ok(Vec::new())
}

#[test]
fn an_include_cycle_ends_and_a_header_directive_is_refused() {
    let t = Tree::new(
        "cycle",
        &[
            (
                "src/a.c",
                "#include \"x.h\"\n#line 5 \"C:\\\\o\\\\a.c\"\nint a;\n",
            ),
            ("src/x.h", "#include \"y.h\"\n"),
            ("src/y.h", "#include \"X.H\"\n#include \"y.h\"\n"),
            ("src/b.c", "#include \"bad.h\"\n"),
            ("src/bad.h", "#line 3 \"z.h\"\n"),
        ],
    );
    let (r, _) = backstop::run("an include cycle", || {
        linedir::for_tus(&tus(&[("A.obj", "src/a.c")]), &t.0, &no_includes)
    });
    let d = r.unwrap();
    assert_eq!(d.len(), 1);
    assert!(d["a.obj"].names("c:\\o\\a.c"));
    let (r, _) = backstop::run("a header directive", || {
        linedir::for_tus(&tus(&[("B.obj", "src/b.c")]), &t.0, &no_includes)
    });
    assert!(format!("{:#}", r.unwrap_err()).contains("a `#line` in a header"));
    let (r, _) = backstop::run("a missing TU", || {
        linedir::for_tus(&tus(&[("C.obj", "src/none.c")]), &t.0, &no_includes)
    });
    assert!(r.is_err());
}

#[test]
fn a_ragged_knowledge_table_is_read_by_its_header_and_capped() {
    let t = Tsv::parse("va\tname\tsize\n\n1\n2\t\t\t\t\t\t\textra\n\t\t\n# note\n");
    assert_eq!(t.rows.len(), 2);
    assert_eq!(t.get(&t.rows[0], "name"), "");
    assert_eq!(t.get(&t.rows[1], "size"), "");
    let big = format!(
        "va\tname\n{}",
        "1\tf\n".repeat(msvc6_pdb::author::MAX_ROWS + 10)
    );
    let (t, _) = backstop::run("a long table", || Tsv::parse(&big));
    assert_eq!(t.rows.len(), msvc6_pdb::author::MAX_ROWS);
    assert!(Tsv::parse("").rows.is_empty());
}

#[test]
fn a_fifo_or_a_device_named_as_a_report_or_a_tu_is_refused() {
    let t = Tree::new("fifo", &[("src/keep.c", "int k;\n")]);
    let fifo = t.0.join("src/a.c");
    let made = std::process::Command::new("mkfifo").arg(&fifo).status();
    if !made.is_ok_and(|s| s.success()) {
        eprintln!("SKIP fifo: mkfifo is not available here; /dev/zero only");
    }
    for p in [Path::new("/dev/zero"), fifo.as_path()] {
        if !p.exists() {
            continue;
        }
        let (r, _) = backstop::run("a report that is not a file", || Report::read(p, "msvc6t"));
        assert!(r.is_err(), "{}", p.display());
        let (r, _) = backstop::run("a bounded read", || {
            msvc6_base::srcio::read_bounded(p, 1 << 20)
        });
        assert!(r.is_err(), "{}", p.display());
    }
    if fifo.exists() {
        let (r, _) = backstop::run("a TU that is a fifo", || {
            linedir::for_tus(&tus(&[("A.obj", "src/a.c")]), &t.0, &no_includes)
        });
        assert!(r.is_err());
    }
}

fn derived(ours: &Image, reference: &Image, budget: pair::Budget) -> anyhow::Result<pair::Derived> {
    let s = msvc6_fixture::pdb::sample();
    let contribs: Vec<(u32, u32)> = s.procs.iter().map(|(_, r, l)| (*r, *l)).collect();
    let given: BTreeMap<u32, (u32, u32)> = s
        .procs
        .iter()
        .take(1)
        .map(|(_, r, l)| (*r, (*r, *l)))
        .collect();
    let none = BTreeMap::new();
    pair::derive(&pair::Inputs {
        reference,
        ours,
        given: &given,
        fallback: &BTreeMap::new(),
        anchors: &BTreeMap::new(),
        contribs: &contribs,
        names: &none,
        scoped: &BTreeMap::new(),
        dropped: &BTreeMap::new(),
        budget,
    })
}

#[test]
fn the_derivation_stops_at_its_budgets_and_says_so() {
    let fx = fx();
    let full = derived(&fx.image, &fx.image, pair::Budget::default()).unwrap();
    assert!(!full.content_spent && !full.rounds_capped, "{full:?}");
    assert!(
        full.content_work > 0,
        "the fixture's anchors are searched for"
    );
    assert!(full.rounds >= 1, "{} round(s)", full.rounds);
    let exact = pair::Budget {
        rounds: full.rounds,
        ..pair::Budget::default()
    };
    let d = derived(&fx.image, &fx.image, exact).unwrap();
    assert!(!d.rounds_capped, "the last round needed is no cap");
    assert_eq!(d.pairs.len(), full.pairs.len());
    let short = pair::Budget {
        rounds: full.rounds - 1,
        ..pair::Budget::default()
    };
    let d = derived(&fx.image, &fx.image, short).unwrap();
    assert!(d.rounds_capped && d.rounds == full.rounds - 1, "{d:?}");
    let spent = pair::Budget {
        content: 1,
        ..pair::Budget::default()
    };
    let d = derived(&fx.image, &fx.image, spent).unwrap();
    assert!(d.content_spent && d.content_work <= 1 + 1024, "{d:?}");
}

#[test]
fn a_hostile_image_into_the_derivation_is_read_or_refused_within_bounds() {
    let fx = fx();
    let base = msvc6_fixture::pdb::sample().image;
    let mut cases: Vec<(String, Vec<u8>)> = Vec::new();
    for at in (0..base.len().saturating_sub(4)).step_by(13) {
        for v in [u32::MAX, 0x7fff_ffff, 0x1000, 0] {
            let mut b = base.clone();
            b[at..at + 4].copy_from_slice(&v.to_le_bytes());
            cases.push((format!("u32 {v:#x} at {at:#x}"), b));
        }
    }
    for n in (0..base.len()).step_by(97) {
        cases.push((format!("{n} bytes"), base[..n].to_vec()));
    }
    let mut read = 0usize;
    for (what, bytes) in &cases {
        let Ok(img) = Image::parse(bytes.clone()) else {
            continue;
        };
        for (ours, reference) in [(&img, &fx.image), (&fx.image, &img)] {
            let (d, _) = backstop::run(what, || derived(ours, reference, pair::Budget::default()));
            if let Ok(d) = d {
                read += 1;
                assert!(
                    d.pairs.len() <= bytes.len(),
                    "{what}: {} pairs",
                    d.pairs.len()
                );
                assert!(!d.rounds_capped, "{what}: {} rounds", d.rounds);
            }
        }
    }
    assert!(read > 100, "{read} hostile image(s) derived");
    eprintln!("{} hostile image case(s), {read} derived", cases.len());
}
