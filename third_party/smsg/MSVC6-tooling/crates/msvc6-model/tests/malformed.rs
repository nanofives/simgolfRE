#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::codeview::{bprel, gproc};
use msvc6_fixture::coff::{Object as Obj, SCN_BSS, SCN_DEBUG, Section};
use msvc6_model::budget::{Budget, NESTING};
use msvc6_model::tuorder::{
    FOSSIL_STEP_FLOOR, FOSSIL_STEPS_PER_MEMBER, Kind, LinkEvidence, Member, ilink_fossils, ilt,
    reserve, score_of,
};
use msvc6_model::{bss, bsssuggest, c1check, c1count, c1price, coffx, drain, frameorder};
use std::collections::BTreeSet;
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::path::{Path, PathBuf};

struct Run {
    fails: Vec<String>,
    cases: usize,
}

impl Run {
    fn new() -> Self {
        Self {
            fails: Vec::new(),
            cases: 0,
        }
    }

    fn case<T>(&mut self, what: &str, f: impl FnOnce() -> T) -> Option<T> {
        self.cases += 1;
        match msvc6_fixture::backstop::run(what, || catch_unwind(AssertUnwindSafe(f))).0 {
            Ok(v) => Some(v),
            Err(_) => {
                self.fails.push(format!("{what}: panicked"));
                None
            }
        }
    }

    fn check(&mut self, what: &str, ok: bool, got: &dyn std::fmt::Debug) {
        if !ok {
            self.fails.push(format!("{what}: {got:?}"));
        }
    }

    fn finish(self, tag: &str) {
        assert!(self.cases > 0, "{tag}: no case ran");
        assert!(
            self.fails.is_empty(),
            "{tag}: {} of {} case(s) failed:\n{}",
            self.fails.len(),
            self.cases,
            self.fails.join("\n")
        );
    }
}

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }

    fn below(&mut self, n: usize) -> usize {
        usize::try_from(self.next() % u64::try_from(n.max(1)).unwrap()).unwrap()
    }

    fn bytes(&mut self, n: usize) -> Vec<u8> {
        (0..n).map(|_| self.next().to_le_bytes()[0]).collect()
    }
}

fn scratch(tag: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!(
        "msvc6-model-malformed-{tag}-{}",
        std::process::id()
    ));
    let _ = std::fs::remove_dir_all(&d);
    std::fs::create_dir_all(&d).unwrap();
    d
}

struct Gone(PathBuf);

impl Drop for Gone {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

fn says_a_bound(e: &anyhow::Error) -> bool {
    let m = format!("{e:#}");
    m.contains("step budget") || m.contains("nesting limit") || m.contains("more than")
}

type Counted = anyhow::Result<c1count::Count>;

fn bounded_count(r: &Counted) -> bool {
    match r {
        Ok(c) => {
            let tokens = u64::try_from(c.tokens).unwrap();
            c.steps <= tokens * c1count::STEPS_PER_TOKEN + c1count::STEP_FLOOR && c.depth <= NESTING
        }
        Err(e) => says_a_bound(e),
    }
}

#[test]
fn the_c1_counter_ends_on_hostile_text_and_names_the_bound_it_hit() {
    let mut run = Run::new();
    let deep = format!(
        "void f() {}{} }}\n",
        "{ ".repeat(100_000),
        "} ".repeat(100_000)
    );
    if let Some(r) = run.case("deep braces", move || c1count::count(&deep)) {
        let ok = r.as_ref().is_ok_and(|c| c.depth <= NESTING) && bounded_count(&r);
        run.check(
            "deep braces in one body are skipped within the budget",
            ok,
            &r.map(|c| (c.total, c.depth)),
        );
    }
    for (what, open) in [
        ("deep namespaces", "namespace n { "),
        ("deep classes", "struct S { "),
        ("deep extern blocks", "extern \"C\" { "),
    ] {
        let text = format!("{}int x;{}\n", open.repeat(100_000), " }".repeat(100_000));
        if let Some(r) = run.case(what, move || c1count::count(&text)) {
            let ok = matches!(&r, Err(e) if format!("{e:#}").contains("nesting limit"));
            run.check(
                &format!("{what} stop at the nesting limit"),
                ok,
                &r.map(|c| c.depth),
            );
        }
    }
    let parens = format!("int x = {}1{};\n", "(".repeat(200_000), ")".repeat(200_000));
    if let Some(r) = run.case("paren storm", move || c1count::count(&parens)) {
        run.check(
            "paren storm bounded",
            bounded_count(&r),
            &r.map(|c| c.steps),
        );
    }
    let templ = "template <;\n".repeat(20_000);
    if let Some(r) = run.case("template <; storm", move || c1count::count(&templ)) {
        run.check(
            "template <; bounded",
            bounded_count(&r),
            &r.map(|c| c.steps),
        );
    }
    for (what, text) in [
        (
            "unterminated string",
            "char* s = \"abc\nint y;\n".to_string(),
        ),
        ("unterminated char", "char c = 'a\nint y;\n".to_string()),
        (
            "unterminated comment",
            "int a; /* never closed\n".repeat(1000),
        ),
        ("lone closers", "}}}} ) ) ] ] > > ;;;\n".repeat(5000)),
        (
            "unterminated #if",
            "#if 1\n#ifdef X\n#if defined(Y\n".repeat(30_000),
        ),
        (
            "macro loop",
            "#define A B\n#define B A\nint A;\n".to_string(),
        ),
    ] {
        let t = text.clone();
        if let Some(r) = run.case(what, move || c1count::count(&t)) {
            run.check(what, bounded_count(&r), &r.map(|c| c.total));
        }
    }
    let flat: String = (0..200_000).map(|k| format!("int a{k};\n")).collect();
    if let Some(r) = run.case("200000 declarations", move || c1count::count(&flat)) {
        let ok = r
            .as_ref()
            .is_ok_and(|c| c.tokens == 600_000 && bounded_count(&r));
        run.check(
            "200000 declarations counted within the per-token budget",
            ok,
            &r.map(|c| (c.tokens, c.steps)),
        );
    }
    let named = format!(
        "#line 1 \"{}.h\"\n{}",
        "n".repeat(4 << 20),
        "int a;\n".repeat(200_000)
    );
    if let Some(r) = run.case("a 4 MiB #line name over 200000 lines", move || {
        c1count::count(&named)
    }) {
        let ok = r
            .as_ref()
            .is_ok_and(|c| c.tokens == 600_001 && bounded_count(&r));
        run.check(
            "a #line name is kept once, not copied for every line it covers",
            ok,
            &r.map(|c| (c.tokens, c.steps)),
        );
    }
    let dtors: String = std::iter::once("struct D { ~D(); };\nvoid f() {\n".to_string())
        .chain((0..20_000).map(|k| format!("  D d{k};\n")))
        .chain(std::iter::once("}\n".to_string()))
        .collect();
    if let Some(r) = run.case("20000 destructor locals", move || {
        c1count::count_with(&dtors, true)
    }) {
        let ok = r.as_ref().is_ok_and(|c| c.total > 20_000) && bounded_count(&r);
        run.check(
            "20000 destructor locals are charged",
            ok,
            &r.map(|c| c.total),
        );
    }
    const SOUP: &[&str] = &[
        "{",
        "}",
        "(",
        ")",
        "<",
        ">",
        ";",
        ":",
        "::",
        ",",
        "*",
        "&",
        "=",
        "~",
        "#",
        "\"",
        "'",
        " ",
        "\n",
        "template",
        "class",
        "struct",
        "operator",
        "virtual",
        "try",
        "catch",
        "throw",
        "new",
        "delete",
        "x",
        "T",
        "int",
        "0",
        "#line 3 \"a.h\"\n",
        "/*",
        "*/",
        "//",
        "\\",
    ];
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    for k in 0..4 {
        let n = 50_000 + rng.below(100_000);
        let text: String = (0..n).map(|_| SOUP[rng.below(SOUP.len())]).collect();
        let what = format!("token soup {k}");
        if let Some(r) = run.case(&what, move || c1count::count(&text)) {
            run.check(&what, bounded_count(&r), &r.map(|c| c.steps));
        }
    }
    run.finish("c1count");
}

#[test]
fn the_probe_writer_and_label_reader_end_on_hostile_listings() {
    let mut run = Run::new();
    let pre: String = (0..50_000)
        .map(|k| match k % 5 {
            0 => format!("#line {} \"c:\\\\p\\\\a{k}.h\"\n", k * 7),
            1 => "#line 99999999999999999999 \"x\n".to_string(),
            2 => "}}}} {{ (( ;\n".to_string(),
            3 => "#line\n".to_string(),
            _ => format!("int v{k};\n"),
        })
        .collect();
    if let Some(p) = run.case("probe on hostile preprocessed text", move || {
        let p = c1check::probe(&pre, &|f: &str| f.contains("a1"));
        let n = p.probes.len();
        let counted = c1count::count_with(&p.text, true).map(|c| c.marks.len());
        (n, p.sys.len(), p.spans.len(), counted)
    }) {
        let (n, sys, spans, counted) = &p;
        let ok = *n >= 1
            && *sys + 1 == *n
            && *spans + 1 == *n
            && counted.as_ref().map_or_else(says_a_bound, |_| true);
        run.check("probe text is well formed", ok, &(n, sys, spans));
    }
    let cod: String = (0..100_000)
        .map(|k| match k % 4 {
            0 => "?c1probe_99999999999999999999@@YAXH@Z PROC NEAR\n".to_string(),
            1 => " jle SHORT $L\n".to_string(),
            2 => format!("?c1probe_{}@@YAXH@Z PROC NEAR\n jle $L{k}\n", k % 7),
            _ => " $L99999999999999999999999\n".to_string(),
        })
        .collect();
    if let Some(v) = run.case("labels from a hostile listing", move || {
        c1check::probe_labels(&cod, 7)
    }) {
        let ok = v.len() == 7 && v.iter().flatten().all(|&x| x >= 0);
        run.check("labels read per probe only", ok, &v);
    }
    for l in [
        "#line 99999999999999999999 \"a\"",
        "#line",
        "#line \"a\"",
        "#  line 4294967296 x",
    ] {
        let s = l.to_string();
        if let Some(r) = run.case(l, move || c1check::line_directive(&s)) {
            run.check(l, r.is_none(), &r);
        }
    }
    run.finish("c1check");
}

fn write(p: &Path, s: &str) {
    msvc6_base::srcio::write(p, s).unwrap();
}

#[test]
fn include_walks_end_on_cycles_fan_out_and_oversized_files() {
    let mut run = Run::new();
    let d = scratch("c1price");
    write(&d.join("a.h"), "#include \"b.h\"\nint a;\n");
    write(&d.join("b.h"), "#include \"a.h\"\nint b;\n");
    write(&d.join("cyc.cpp"), "#include \"a.h\"\nint c;\n");
    write(
        &d.join("miss.cpp"),
        "#include \"a.h\"\n#include \"missing.h\"\nint c;\n",
    );
    for (tu, want) in [
        ("cyc.cpp", "closure 3 files"),
        ("miss.cpp", "closure 4 files"),
    ] {
        let (dd, p) = (d.clone(), d.join(tu));
        if let Some(r) = run.case(tu, move || c1price::predict_tu(&p, &[dd], &[], &[])) {
            let ok = r
                .as_ref()
                .is_ok_and(|v| v.first().is_some_and(|l| l.contains(want)));
            run.check(
                &format!("{tu}: a cycle opens each file once, a missing file reads as empty"),
                ok,
                &r.map(|v| v.first().cloned()),
            );
        }
    }
    for k in 0..300 {
        write(
            &d.join(format!("c{k}.h")),
            &format!("#include \"c{}.h\"\nint c{k};\n", k + 1),
        );
    }
    write(&d.join("deep.cpp"), "#include \"c0.h\"\n");
    let (dd, tu) = (d.clone(), d.join("deep.cpp"));
    if let Some(r) = run.case("300-deep include chain", move || {
        c1price::predict_tu(&tu, &[dd], &[], &[])
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("include levels"));
        run.check(
            "a deep chain stops at the nesting bound",
            ok,
            &r.map(|v| v.len()),
        );
    }
    let all: String = (0..64).map(|k| format!("#include \"f{k}.h\"\n")).collect();
    for k in 0..64 {
        write(&d.join(format!("f{k}.h")), &format!("{all}int f{k};\n"));
    }
    write(&d.join("fan.cpp"), &all);
    let (dd, tu) = (d.clone(), d.join("fan.cpp"));
    if let Some(r) = run.case("fan-out against a small open limit", move || {
        let shared = c1price::Shared::new(10);
        let r = c1price::predict_tu_with(&shared, &tu, &[dd], &[], &[]);
        (r.map(|v| v.len()), shared.opens())
    }) {
        let (res, opens) = &r;
        let ok = *opens <= 11 && matches!(res, Err(e) if format!("{e:#}").contains("opens in all"));
        run.check("fan-out stops at the open limit", ok, &r);
    }
    let big = d.join("big.h");
    std::fs::File::create(&big)
        .unwrap()
        .set_len(c1price::MAX_FILE_BYTES + 1)
        .unwrap();
    if let Some(r) = run.case("oversized header", move || {
        c1price::predict(&big, None, true)
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("more than"));
        run.check("an oversized header is refused", ok, &r.map(|v| v.len()));
    }
    write(&d.join("inc.cpp"), "#include \"big.h\"\nint x;\n");
    let (dd, tu) = (d.clone(), d.join("inc.cpp"));
    if let Some(r) = run.case("oversized include", move || {
        c1price::predict_tu(&tu, &[dd], &[], &[])
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("more than"));
        run.check(
            "an oversized include is refused, not read as empty",
            ok,
            &r.map(|v| v.len()),
        );
    }
    let _ = std::fs::remove_dir_all(&d);
    run.finish("c1price");
}

#[test]
fn the_gl_counter_states_a_build_only_where_the_header_holds_one() {
    let gold = msvc6_fixture::il::Set::new(0x1234).gl();
    assert_eq!(drain::gl_counter(&gold).unwrap(), (0x1234, Some(8168)));
    let mut fixed = vec![0x11u8, 0x86, 0x03];
    fixed.extend_from_slice(&0x0130_937du32.to_le_bytes());
    fixed.extend_from_slice(&0x55u32.to_le_bytes());
    fixed.push(1);
    let (next, build) = drain::gl_counter(&fixed).unwrap();
    assert_eq!((next, build), (0x55, None));
    assert_eq!(
        msvc6_model::hdrprice::front_end_build(build),
        "build unstated"
    );
    assert_eq!(msvc6_model::hdrprice::front_end_build(Some(8168)), "8168");
}

#[test]
fn the_il_readers_end_on_random_and_adversarial_bytes() {
    let mut run = Run::new();
    let mut rng = Rng(0x2545_f491_4f6c_dd1d);
    for n in [0usize, 1, 6, 7, 20, 4096, 1 << 22] {
        let b = rng.bytes(n);
        let what = format!("{n} random bytes");
        if let Some((c, fns, odd, refs, comdats)) = run.case(&what, move || {
            let (fns, odd) = drain::gl_functions(&b);
            let t = drain::read_tu(&b, &b, &b);
            (
                drain::gl_counter(&b).is_err(),
                fns.len().max(t.fns.len()),
                odd,
                drain::refs_of_bytes(&b).is_err(),
                coffx::code_comdats_named_of(&b).is_err(),
            )
        }) {
            let ok = c && fns + odd <= n / 2 + 1 && refs && comdats;
            run.check(&what, ok, &(c, fns, odd, refs, comdats));
        }
    }
    let names = 200_000u32;
    let mut set = msvc6_fixture::il::Set::new(0x40_0000);
    for k in 0..names {
        set.defs.push(msvc6_fixture::il::def(
            0x100 + k,
            &format!("?f{k}@@YAXXZ"),
            8,
            0,
        ));
    }
    let mut gl = set.gl();
    gl.truncate(gl.len() - 30);
    if let Some(r) = run.case("200000 .gl records, the last one cut", move || {
        let (fns, odd) = drain::gl_functions(&gl);
        (fns.len(), odd)
    }) {
        run.check(
            "every whole record is read once and the cut one is counted",
            r == (usize::try_from(names - 1).unwrap(), 1),
            &r,
        );
    }
    let mut storm = Vec::new();
    for _ in 0..1000 {
        storm.push(0);
        storm.extend(std::iter::repeat_n(b'A', 8000));
    }
    if let Some(r) = run.case("unterminated names", move || {
        drain::gl_functions(&storm).0.len()
    }) {
        run.check("unterminated names yield nothing", r == 0, &r);
    }
    let known: Vec<String> = (0..64).map(|k| format!("f{k}")).collect();
    if let Some(r) = run.case("a shift sweep over an empty IL", move || {
        let il = drain::TuIl::default();
        let refs = drain::Refs::default();
        drain::best_shift(&known, &il, &refs, &known, 0, true, 2)
    }) {
        run.check("an empty IL sweeps to nothing displaced", r.best == 0, &r);
    }
    run.finish("drain");
}

fn bss_object(n: usize) -> Vec<u8> {
    let mut o = Obj::new();
    let size = u32::try_from(n * 4).unwrap();
    let s = o.section(Section {
        name: ".bss".into(),
        characteristics: SCN_BSS,
        uninit: size,
        ..Section::default()
    });
    for k in 0..n {
        o.external(&format!("?g{k}@@3HA"), u32::try_from(k * 4).unwrap(), s);
    }
    o.build()
}

#[test]
fn the_bss_readers_refuse_oversized_tus_and_end_on_hostile_text() {
    let mut run = Run::new();
    let over = bss_object(bss::MAX_VARS + 1);
    if let Some(r) = run.case("4097 .bss symbols", move || {
        bss::vars_of(&over, None).map(|v| v.len())
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("more than the 4096"));
        run.check("more than MAX_VARS is refused", ok, &r);
    }
    let full = bss_object(bss::MAX_VARS);
    if let Some(r) = run.case("4096 .bss symbols judged", move || {
        let v = bss::vars_of(&full, None)?;
        let n = v.len();
        let verdict = bss::judge("full", v)?;
        anyhow::Ok((n, bss::verdict_lines(&verdict).len()))
    }) {
        let ok = r.as_ref().is_ok_and(|(n, _)| *n == bss::MAX_VARS);
        run.check(
            "MAX_VARS variables are read and judged",
            ok,
            &r.map(|x| x.0),
        );
    }
    let mut rng = Rng(0xdead_beef_cafe_f00d);
    let junk = rng.bytes(1 << 16);
    if let Some(r) = run.case("random object bytes", move || {
        bss::vars_of(&junk, None).is_err()
    }) {
        run.check("random bytes are not an object", r, &r);
    }
    let src = "int a = (".repeat(20_000);
    if let Some(r) = run.case("quadratic initialiser scan", move || {
        let bud = Budget::new(1_000_000, NESTING);
        let got = bss::initialised_within(&src, "a", &bud);
        (got, bud.over(), bud.spent())
    }) {
        run.check(
            "the scan stops at its step budget",
            r.0.is_none() && r.1 == Some("its step budget") && r.2 <= 1_000_000,
            &r,
        );
    }
    let text = "int a = (".repeat(20_000);
    let one = bss_object(1);
    if let Some(r) = run.case("zero-init marking over a quadratic source", move || {
        let mut v = bss::vars_of(&one, None)?;
        for x in &mut v {
            x.symbol = "?a@@3HA".into();
        }
        bss::mark_zero_inits(&text, &mut v)
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("step budget"));
        run.check("zero-init marking says it stopped", ok, &r);
    }
    for (what, n) in [("empty", 0usize), ("one", 1), ("big", 100_000)] {
        let b: Vec<u32> = (0..n)
            .map(|k| u32::try_from((k * 7919) % 1024).unwrap())
            .collect();
        if let Some(r) = run.case(what, move || {
            let pinned = vec![false; b.len()];
            (
                bss::longest_ordered(&b).len(),
                bss::plan(&b, 1, &pinned).is_some(),
            )
        }) {
            let ok = r.0 <= n.min(bss::MAX_VARS) && (n <= bss::MAX_VARS || !r.1);
            run.check(&format!("plan over {what}"), ok, &r);
        }
    }
    run.finish("bss");
}

#[test]
fn the_vocabulary_and_spelling_expansion_refuse_blow_ups() {
    let mut run = Run::new();
    let f = PathBuf::from("v.txt");
    for (what, text, want) in [
        (
            "one-word group",
            "group alone\n".to_string(),
            "at least two",
        ),
        ("unknown line", "bogus x y\n".to_string(), "a line is"),
    ] {
        let (t, f) = (text.clone(), f.clone());
        if let Some(r) = run.case(what, move || {
            bsssuggest::Vocabulary::parse(&t, &f).map(|v| v.groups.len())
        }) {
            let ok = matches!(&r, Err(e) if format!("{e:#}").contains(want));
            run.check(what, ok, &r);
        }
    }
    let d = scratch("vocab");
    let big = d.join("big.txt");
    std::fs::File::create(&big)
        .unwrap()
        .set_len(bsssuggest::MAX_VOCABULARY_BYTES + 1)
        .unwrap();
    if let Some(r) = run.case("oversized vocabulary", move || {
        bsssuggest::Vocabulary::read(&big).map(|v| v.groups.len())
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("more than"));
        run.check("an oversized vocabulary is refused", ok, &r);
    }
    let many = "[a,b]".repeat(30);
    if let Some(r) = run.case("2^30 spellings", move || {
        bsssuggest::expand(&many, 0, 1023).map(|v| v.len())
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("spellings"));
        run.check("2^30 spellings are refused", ok, &r);
    }
    let wide = format!("[{},y]", "x".repeat(4000)).repeat(16);
    if let Some(r) = run.case("wide spellings", move || {
        bsssuggest::expand(&wide, 0, 1023).map(|v| v.len())
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("bytes"));
        run.check("spellings past the byte limit are refused", ok, &r);
    }
    let voc = bsssuggest::Vocabulary {
        groups: vec![(0..64).map(|k| format!("w{k}")).collect()],
        qualifiers: (0..2_100_000).map(|k| format!("q{k}")).collect(),
    };
    let name = "?g_w1W2W3W4@@3HA".to_string();
    if let Some(r) = run.case("qualifier blow-up", move || {
        bsssuggest::suggest(&name, 0, 1023, &voc).map(|v| v.len())
    }) {
        let ok = matches!(&r, Err(e) if format!("{e:#}").contains("more than"));
        run.check("too many qualifier forms are refused", ok, &r);
    }
    let _ = std::fs::remove_dir_all(&d);
    run.finish("bsssuggest");
}

fn debug_s(records: &[Vec<u8>]) -> Vec<u8> {
    let mut d = vec![1u8, 0, 0, 0];
    for r in records {
        d.extend_from_slice(r);
    }
    d
}

fn object_with_debug_s(d: Vec<u8>) -> Vec<u8> {
    let mut o = Obj::new();
    o.section(Section {
        name: ".debug$S".into(),
        characteristics: SCN_DEBUG,
        data: d,
        ..Section::default()
    });
    o.build()
}

#[test]
fn the_frame_reader_ends_on_hostile_debug_records_and_refuses_huge_frames() {
    let mut run = Run::new();
    let mut rng = Rng(0x1234_5678_9abc_def1);
    for n in [0usize, 3, 4, 5, 1 << 20] {
        let b = rng.bytes(n);
        let what = format!("{n} random .debug$S bytes");
        if let Some(r) = run.case(&what, move || {
            let procs = frameorder::procs_of_debug_s(&b);
            let locals: usize = procs.iter().map(|p| p.locals.len()).sum();
            (procs.len(), locals, frameorder::read_object(&b).is_err())
        }) {
            run.check(&what, r.0 + r.1 <= n / 4 + 1 && r.2, &r);
        }
    }
    let mut recs = vec![gproc("?huge@@YAXXZ", 1, 0, 16, 0)];
    let n = frameorder::MAX_LOCALS + 1;
    for k in 0..n {
        recs.push(bprel(
            -4 - i32::try_from(k * 4).unwrap(),
            0x74,
            &format!("v{k}"),
        ));
    }
    let obj = object_with_debug_s(debug_s(&recs));
    if let Some(r) = run.case("a frame past MAX_LOCALS", move || {
        frameorder::report(&[("huge.obj".to_string(), obj)], None, false)
    }) {
        let (lines, t) = &r;
        let ok = t.refused == 1 && lines.iter().any(|l| l.starts_with("REFUSED"));
        run.check(
            "a huge frame is refused loudly",
            ok,
            &(t.refused, lines.len()),
        );
    }
    let mut recs = vec![gproc("?nest@@YAXXZ", 1, 0, 16, 0)];
    for _ in 0..200_000 {
        recs.push(msvc6_fixture::codeview::block(0, 4));
    }
    let obj = object_with_debug_s(debug_s(&recs));
    if let Some(r) = run.case("200000 unclosed blocks", move || {
        frameorder::read_object(&obj).map(|(_, p)| p.len())
    }) {
        run.check(
            "unclosed blocks read as one function",
            r.as_ref().is_ok_and(|&n| n == 1),
            &r.map_err(|e| format!("{e:#}")),
        );
    }
    run.finish("frameorder");
}

#[test]
fn the_ilink_screens_end_on_dense_jumps_and_dense_pull_evidence() {
    let mut run = Run::new();
    let text: Vec<u8> = [0xe9u8, 0, 0, 0, 0].repeat(200_000);
    if let Some(r) = run.case("a megabyte of jumps", move || ilt(&text, 0x1000).len()) {
        run.check("one thunk per five bytes", r == 200_000, &r);
    }
    let wild = vec![0xe9u8; 1 << 20];
    if let Some(r) = run.case("jumps out of the image", move || ilt(&wild, 0x1000).len()) {
        run.check("a jump below the image ends the table", r == 0, &r);
    }
    if let Some(r) = run.case("reserve at the top", || reserve(u64::MAX)) {
        run.check("reserve saturates", r > 0, &r);
    }
    let n = 4096usize;
    let members: Vec<Member> = (0..n)
        .map(|mi| {
            let k = u32::try_from(mi).unwrap();
            let n32 = u32::try_from(n).unwrap();
            Member {
                name: format!("?f{mi}@@YAXXZ"),
                rva: if mi % 2 == 0 {
                    0x1000 + k * 0x10
                } else {
                    0x1000 + (n32 - k) * 0x10 + 8
                },
                len: 8,
                kind: Kind::User,
            }
        })
        .collect();
    let ev = LinkEvidence {
        bodies: vec![
            (0..n)
                .map(|mi| (Some(mi), std::iter::once(mi).collect::<BTreeSet<usize>>()))
                .collect(),
        ],
        members: (0..n)
            .map(|p| (format!("m{p}.obj"), format!("a{p}.lib")))
            .collect(),
        definer: std::collections::BTreeMap::new(),
    };
    if let Some(r) = run.case("dense pull evidence", move || {
        let refs: Vec<&Member> = members.iter().collect();
        let scores = vec![score_of(&refs)];
        let objects = vec![("dense".to_string(), members)];
        let bud = Budget::per_item(n, FOSSIL_STEPS_PER_MEMBER, FOSSIL_STEP_FLOOR);
        let f = ilink_fossils(&objects, &scores, &ev, &bud);
        (f.len(), bud.over(), bud.spent())
    }) {
        let cap = u64::try_from(n).unwrap() * FOSSIL_STEPS_PER_MEMBER + FOSSIL_STEP_FLOOR;
        run.check(
            "the pull-order rule stops at its step budget",
            r.0 == 1 && r.1 == Some("its step budget") && r.2 <= cap,
            &r,
        );
    }
    run.finish("tuorder");
}

#[test]
fn an_atom_file_of_junk_is_refused_by_hunk_and_a_hunk_never_applies_at_a_guess() {
    use msvc6_model::hdrcombo::{apply_hunks, combos, parse_atoms};
    let roots = vec!["include".to_string(), "src".to_string()];
    let mut rng = Rng(0x51d_7e2b);
    let mut run = Run::new();
    for _ in 0..8 {
        let t = msvc6_base::srcio::tool_text(&rng.bytes(1 << 14));
        let roots = roots.clone();
        if let Some(Ok(n)) = run.case("atoms", move || parse_atoms(&t, &roots).map(|v| v.len())) {
            run.check("atom count", n < 1 << 14, &n);
        }
    }
    let one = "=== one  an added line\n--- a/include/x.h\n+++ b/include/x.h\n@@ -1,2 +1,3 @@\n a\n+b\n c\n";
    let atoms = parse_atoms(one, &roots).unwrap();
    run.check("one atom", atoms.len() == 1, &atoms.len());
    let h = &atoms[0].files[0].hunks;
    let applied = apply_hunks("a\nc\n", h);
    run.check(
        "applied",
        applied.as_deref().ok() == Some("a\nb\nc\n"),
        &applied,
    );
    let miss = apply_hunks("x\ny\nz\n", h);
    run.check("no context, no apply", miss.is_err(), &miss);
    let empty = parse_atoms("", &roots);
    run.check(
        "an empty file is no atoms, not an error",
        empty.as_ref().map(Vec::len).unwrap_or(99) == 0,
        &empty,
    );
    run.check(
        "a repeated atom name is refused",
        parse_atoms("=== a\n=== a\n", &roots).is_err(),
        &(),
    );
    run.check(
        "no free atom is one empty combination",
        combos(0, &[], None).map(|c| c.len()).unwrap_or(99) == 1,
        &combos(0, &[], None),
    );
    run.check("past the mask width", combos(64, &[], None).is_err(), &());
    let capped = combos(3, &[], Some(1)).unwrap();
    run.check(
        "a cap counts only the free atoms it adds, the empty combination included",
        capped.len() == 4,
        &capped,
    );
    run.finish("hdrcombo");
}

#[test]
fn an_atom_naming_a_file_outside_the_tree_is_refused_and_nothing_outside_is_written() {
    use msvc6_model::hdrcombo::{apply, parse_atoms, write_inside};
    let dir = scratch("hdrcombo-confine");
    let (root, victim) = (dir.join("root"), dir.join("victim"));
    std::fs::create_dir_all(root.join("include")).unwrap();
    std::fs::create_dir_all(&victim).unwrap();
    let original = "a\nc\n";
    msvc6_base::srcio::own_write(victim.join("x.h"), original).unwrap();
    msvc6_base::srcio::own_write(root.join("include/x.h"), original).unwrap();
    std::os::unix::fs::symlink(&victim, root.join("out")).unwrap();
    std::os::unix::fs::symlink(victim.join("x.h"), root.join("include/y.h")).unwrap();
    let roots = vec!["project/game".to_string()];
    let hunk = "@@ -1,2 +1,3 @@\n a\n+b\n c\n";
    let mut run = Run::new();
    for target in [
        victim.join("x.h").display().to_string(),
        "b/../victim/x.h".to_string(),
        "b/project/game/include/../../../victim/x.h".to_string(),
        "C:\\victim\\x.h".to_string(),
        "b/..\\victim\\x.h".to_string(),
        "\\victim\\x.h".to_string(),
        "b/".to_string(),
    ] {
        let text = format!("=== evil\n--- a/x\n+++ {target}\n{hunk}");
        let roots = roots.clone();
        let r = run.case("a hostile +++ path", move || {
            parse_atoms(&text, &roots).map(|a| a.len())
        });
        run.check(
            &format!("+++ {target} is refused"),
            matches!(r, Some(Err(_))),
            &r.map(|x| x.map_err(|e| format!("{e:#}"))),
        );
    }
    for (name, path) in [("dir", "out/x.h"), ("file", "include/y.h")] {
        let text = format!("=== {name}\n+++ b/project/game/{path}\n{hunk}");
        let atoms = parse_atoms(&text, &roots).unwrap();
        let (root, want) = (root.clone(), vec![name.to_string()]);
        let r = run.case("a write through a symbolic link", move || {
            apply(&root, &atoms, &want).map_err(|e| format!("{e:#}"))
        });
        run.check(
            &format!("{path} through a link is refused"),
            matches!(r, Some(Err(_))),
            &r,
        );
    }
    let r = run.case("a direct write outside", || {
        write_inside(&root, "../victim/x.h", "pwned\n", true).map_err(|e| format!("{e:#}"))
    });
    run.check("write_inside refuses `..'", matches!(r, Some(Err(_))), &r);
    let text = format!("=== ok\n+++ b/project/game/include/x.h\n{hunk}");
    let atoms = parse_atoms(&text, &roots).unwrap();
    let wrote = apply(&root, &atoms, &["ok".to_string()]);
    run.check(
        "an atom inside the tree is applied",
        wrote.as_ref().is_ok_and(|w| w == &["wrote include/x.h"])
            && msvc6_base::srcio::read(root.join("include/x.h")).unwrap() == "a\nb\nc\n",
        &wrote.map_err(|e| format!("{e:#}")),
    );
    let after = msvc6_base::srcio::read(victim.join("x.h")).unwrap();
    run.check(
        "the file outside the tree is untouched",
        after == original,
        &after,
    );
    std::fs::remove_dir_all(&dir).ok();
    run.finish("hdrcombo confinement");
}

#[test]
fn a_delta_names_a_table_this_run_can_pair_or_it_is_refused() {
    use msvc6_model::labelsafe::{LABELS_HEADER, parse_delta};
    let base: std::collections::BTreeMap<(String, String), u32> =
        [(("Tile".to_string(), "?f@@YAXXZ".to_string()), 100u32)]
            .into_iter()
            .collect();
    let dir = scratch("labelsafe");
    let _gone = Gone(dir.clone());
    let write = |name: &str, text: &str| -> String {
        let p = dir.join(name);
        msvc6_base::srcio::own_write(&p, text).unwrap();
        p.display().to_string()
    };
    let shifts = write(
        "c1.txt",
        "Tile: C1 counter 48678 -> 48681, shift +3 (mod 64 3)\n",
    );
    let labels = write(
        "labels.tsv",
        &format!("{LABELS_HEADER}\nTile\t?f@@YAXXZ\t103\nOther\t?g@@YAXXZ\t7\n"),
    );
    let empty = write("empty.txt", "nothing this reader pairs\n");
    let mut run = Run::new();
    for bad in [
        String::new(),
        "Tile".to_string(),
        format!("={shifts}"),
        "Tile=/nonesuch/none".to_string(),
        format!("Tile={empty}"),
    ] {
        let r = parse_delta(&bad, &base);
        run.check(&bad, r.is_err(), &r.map(|d| d.touched()));
    }
    let d = parse_delta(&format!("edit={shifts}"), &base).unwrap();
    run.check(
        "a `--c1' row gives the whole TU its shift",
        d.shift("Tile", "?f@@YAXXZ") == 3 && d.shift("Other", "?g@@YAXXZ") == 0,
        &(d.shift("Tile", "?f@@YAXXZ"), d.shift("Other", "?g@@YAXXZ")),
    );
    let per = parse_delta(&format!("edit={labels}"), &base).unwrap();
    run.check(
        "a label row is a shift only where this run has the base label",
        per.shift("Tile", "?f@@YAXXZ") == 3 && per.shift("Other", "?g@@YAXXZ") == 0,
        &(
            per.shift("Tile", "?f@@YAXXZ"),
            per.shift("Other", "?g@@YAXXZ"),
        ),
    );
    run.check("one touched", per.touched().len() == 1, &per.touched());
    let mut rng = Rng(0x9ac_31f7);
    for k in 0..6 {
        let p = write(
            &format!("junk{k}.txt"),
            &msvc6_base::srcio::tool_text(&rng.bytes(1 << 13)),
        );
        let spec = format!("edit={p}");
        let base = base.clone();
        run.case("junk table", move || parse_delta(&spec, &base).is_ok());
    }
    run.finish("labelsafe");
}

#[test]
fn a_tu_flag_candidate_of_junk_names_no_member_and_one_unknown_refutes_nothing() {
    use msvc6_model::tuflags::{Carve, Flag, classify, member_rvas, refuted, resolve_rate};
    let mut rng = Rng(0x7e3_11ac);
    let mut run = Run::new();
    for _ in 0..6 {
        let t = msvc6_base::srcio::tool_text(&rng.bytes(1 << 14));
        if let Some(rvas) = run.case("tu-flags rvas", move || member_rvas(&t)) {
            run.check("member count", rvas.len() < 1 << 14, &rvas.len());
        }
    }
    let text = "  0x001000 +0x40\n *  0x001040 +0x10\n// 0x002000 +0x8\nnot a row\n0x1000\n";
    let rvas = member_rvas(text);
    run.check(
        "only a row whose first token is the address is read",
        rvas == vec![0x1000, 0x1040],
        &rvas,
    );
    let eh = Carve {
        eh_data: true,
        cflags: None,
    };
    let plain = Carve::default();
    let gx = Carve {
        eh_data: false,
        cflags: Some(vec!["/GX".to_string(), "/Od".to_string()]),
    };
    for (what, got, want) in [
        ("eh data", classify(Some(&eh), &[], "/GX"), Flag::Certain),
        (
            "no evidence",
            classify(Some(&plain), &[], "/GX"),
            Flag::Absent,
        ),
        (
            "case-blind switch",
            classify(Some(&gx), &[], "/gx"),
            Flag::Certain,
        ),
        (
            "no carve",
            classify(None, &["/GX".to_string()], "/GX"),
            Flag::Unknown,
        ),
        (
            "a row with no flags takes the default",
            classify(Some(&plain), &["/GX".to_string()], "/GX"),
            Flag::Certain,
        ),
    ] {
        run.check(what, got == want, &got);
    }
    for (what, kinds, want) in [
        ("both ends", vec![Flag::Certain, Flag::Absent], true),
        (
            "certain and unknown",
            vec![Flag::Certain, Flag::Unknown],
            false,
        ),
        (
            "absent and unknown",
            vec![Flag::Absent, Flag::Unknown],
            false,
        ),
        ("all unknown", vec![Flag::Unknown, Flag::Unknown], false),
    ] {
        run.check(what, refuted(&kinds) == want, &kinds);
    }
    let known: BTreeSet<u32> = [0x1000u32, 0x1040].into_iter().collect();
    let rate = resolve_rate(&[0x1000, 0x1040, 0xdead], &|r| known.contains(&r));
    run.check("resolve rate", rate == (2, 3), &rate);
    run.finish("tuflags");
}

#[test]
fn a_fifo_or_a_device_given_as_an_object_or_library_is_refused_before_it_blocks() {
    let mut run = Run::new();
    let d = std::env::temp_dir().join(format!("msvc6-model-fifo-{}", std::process::id()));
    std::fs::create_dir_all(&d).unwrap();
    let mut paths = Vec::new();
    for name in ["x.obj", "y.lib"] {
        let p = d.join(name);
        let made = std::process::Command::new("mkfifo")
            .arg(&p)
            .status()
            .is_ok_and(|s| s.success());
        if made {
            paths.push(p);
        }
    }
    let link = d.join("zero.obj");
    std::os::unix::fs::symlink("/dev/zero", &link).unwrap();
    paths.push(link);
    for p in paths {
        let what = format!("frame-order input {}", p.display());
        let q = p.clone();
        if let Some(e) = run.case(&what, move || {
            frameorder::collect(std::slice::from_ref(&q)).map(|i| i.objects.len())
        }) {
            let refused = e
                .as_ref()
                .err()
                .is_some_and(|e| format!("{e:#}").contains("not a regular file"));
            run.check(&what, refused, &e);
        }
    }
    std::fs::remove_dir_all(&d).ok();
    run.finish("fifo");
}
