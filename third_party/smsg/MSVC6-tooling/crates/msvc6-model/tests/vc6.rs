#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::compile::{Compiler, Unit, compile_in};
use msvc6_model::bk::Driver;
use msvc6_model::hdrprice::Variant;
use msvc6_model::{bss, c1check, dataseat, drain, frameorder};
use msvc6_project::config::Project;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn require() -> bool {
    env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1")
}

fn quoted(s: &str) -> String {
    format!("'{s}'")
}

fn config(scratch: &Path, root: &str) -> Result<PathBuf> {
    let mut text = format!(
        "schema = 1\n\n[toolchain.vc6]\nroot = {}\n\n[runner]\n",
        quoted(root)
    );
    let command: Vec<String> = env("MSVC6T_WINE")
        .unwrap_or_else(|| "wine".into())
        .split_whitespace()
        .map(quoted)
        .collect();
    text.push_str(&format!("command = [{}]\n", command.join(", ")));
    text.push_str(&format!(
        "prefixes = {}\ntokens = {}\n",
        quoted(&scratch.join("prefixes").display().to_string()),
        quoted(&scratch.join("tokens").display().to_string())
    ));
    if let Some(t) = env("MSVC6T_WINE_TEMPLATE") {
        text.push_str(&format!("template = {}\n", quoted(&t)));
    }
    text.push_str(&format!(
        "\n[cache]\ndir = {}\n",
        quoted(&scratch.join("cache").display().to_string())
    ));
    let file = scratch.join("msvc6t.toml");
    srcio::own_write(&file, &text)?;
    Ok(file)
}

fn unit(source: &Path, cflags: &[&str]) -> Unit {
    let file = source.file_name().unwrap().to_string_lossy().into_owned();
    let stem = source.file_stem().unwrap().to_string_lossy().into_owned();
    Unit {
        source: source.to_path_buf(),
        stem,
        file,
        cflags: cflags.iter().map(|s| (*s).to_string()).collect(),
        includes: Vec::new(),
        include_env: None,
        toolchain: "vc6".into(),
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
        let m = u64::try_from(n.max(1)).unwrap_or(1);
        usize::try_from(self.next() % m).unwrap_or(0)
    }

    fn name(&mut self, prefix: &str) -> String {
        let letters = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        let n = 2 + self.below(9);
        let mut s = prefix.to_string();
        for _ in 0..n {
            s.push(char::from(letters[self.below(letters.len())]));
        }
        s
    }
}

const PRELUDE: &str = "void h2(); int o1;\n";

fn toy_list(eh: bool) -> Vec<(String, bool, bool)> {
    let mut toys: Vec<(String, bool, bool)> = Vec::new();
    let plain = common::toys::TOYS
        .iter()
        .chain(common::toys::TEMPLATE_TOYS)
        .chain(common::toys::VBASE_TOYS);
    for (t, _) in plain {
        toys.push(((*t).to_string(), true, false));
    }
    if eh {
        for (t, _) in common::toys::EH_TOYS {
            toys.push(((*t).to_string(), true, true));
        }
    }
    for (t, _, _) in common::toys::NEW_DELETE_TOYS {
        toys.push(((*t).to_string(), true, false));
        toys.push(((*t).to_string(), false, false));
    }
    toys
}

fn flags(gx: bool) -> &'static [&'static str] {
    if gx { &["/O2", "/GX"] } else { &["/O2"] }
}

fn c1_totals(c: &Compiler, scratch: &Path) -> Result<()> {
    let src = scratch.join("c1-total");
    std::fs::create_dir_all(&src)?;
    let toys = toy_list(true);
    let texts: Vec<(String, bool, PathBuf)> = toys
        .iter()
        .enumerate()
        .map(|(k, (t, gx, _))| {
            let text = format!(
                "{PRELUDE}{t}\nvoid c1hook(int);\n{}",
                c1check::probe_text(0)
            );
            let p = src.join(format!("tot{k:03}.cpp"));
            srcio::own_write(&p, &text)?;
            Ok((text, *gx, p))
        })
        .collect::<Result<_>>()?;
    let tc = c.toolchain("vc6")?;
    let got = msvc6_diff::par::map(&texts, msvc6_diff::par::default_jobs(), |(text, gx, p)| {
        let sess = c.runner.session()?;
        let out = compile_in(
            &sess,
            c.cache.as_ref(),
            &tc,
            &unit(p, flags(*gx)),
            &src,
            true,
        )?;
        let cod = srcio::read(out.cod.as_ref().context("no listing")?)?;
        let label = c1check::probe_labels(&cod, 1)
            .first()
            .copied()
            .flatten()
            .context("the probe has no label")?;
        let mark = *msvc6_model::c1count::count_with(text, *gx)?
            .marks
            .first()
            .context("the model saw no probe")?;
        Ok::<_, anyhow::Error>(label - mark)
    });
    let mut settled: BTreeMap<i64, Vec<String>> = BTreeMap::new();
    let mut eh: BTreeMap<i64, usize> = BTreeMap::new();
    for ((t, gx, is_eh), r) in toys.iter().zip(got) {
        let off = r.with_context(|| format!("`{t}`"))?;
        if *is_eh {
            *eh.entry(off).or_default() += 1;
        } else {
            settled
                .entry(off)
                .or_default()
                .push(format!("{t} (/GX {gx})"));
        }
    }
    let n: usize = settled.values().map(Vec::len).sum();
    println!(
        "vc6: c1 price over {n} generated TU(s): the probe after each construct reads the model's counter plus {:?}",
        settled.keys().collect::<Vec<_>>()
    );
    let at = settled.keys().next().copied().unwrap_or(0);
    println!(
        "vc6: c1 price over {} /GX exception-handling TU(s), not settled (the model prices /GX exception handling, but these land off its counter): {} at that offset, the \
         rest at {:?}",
        eh.values().sum::<usize>(),
        eh.get(&at).copied().unwrap_or(0),
        eh.iter().filter(|(o, _)| **o != at).collect::<Vec<_>>()
    );
    if settled.len() != 1 || n == 0 {
        let mut text = String::new();
        for (o, v) in &settled {
            let _ = writeln!(text, "offset {o}: {} TU(s), first {:?}", v.len(), v.first());
        }
        bail!("the counter model's totals do not sit at one offset from CL's labels:\n{text}");
    }
    Ok(())
}

fn c1_spans(c: &Compiler, scratch: &Path) -> Result<()> {
    let src = scratch.join("c1");
    std::fs::create_dir_all(&src)?;
    let toys = toy_list(false);
    let units: Vec<Unit> = toys
        .iter()
        .enumerate()
        .map(|(k, (t, gx, _))| {
            let p = src.join(format!("toy{k:03}.cpp"));
            srcio::own_write(&p, format!("{PRELUDE}{t}\n"))?;
            Ok(unit(&p, flags(*gx)))
        })
        .collect::<Result<_>>()?;
    let roots = [src.clone()];
    let project = c1check::project_test(&roots);
    let work = scratch.join("c1-work");
    let got = msvc6_diff::par::map(&units, msvc6_diff::par::default_jobs(), |u| {
        c1check::check_tu(c, u, &work, &project, false)
    });
    let (mut spans, mut misses) = (0usize, 0usize);
    let mut bad = Vec::new();
    for ((t, gx, _), r) in toys.iter().zip(got) {
        let r = r.with_context(|| format!("c1 check of `{t}`"))?;
        spans += r.spans;
        misses += r.misses;
        if r.misses > 0 || r.sys > 0 || r.spans == 0 {
            bad.push(format!("{t} (/GX {gx}): {}", r.report.join(" | ")));
        }
    }
    println!(
        "vc6: c1 check over {} generated TU(s) with no exception handling: {spans} project span(s), {misses} MISS",
        toys.len()
    );
    if !bad.is_empty() {
        bail!(
            "the C1 counter model misses on {} TU(s):\n{}",
            bad.len(),
            bad.join("\n")
        );
    }
    Ok(())
}

struct Header {
    text: String,
    ctors: String,
    methods: Vec<(String, String)>,
}

fn predict_header(k: usize, rng: &mut Rng) -> Header {
    let classes = 1 + rng.below(3);
    let mut h = format!("#ifndef P{k}_H\n#define P{k}_H\n");
    let mut ctors = String::new();
    let mut methods: Vec<(String, String)> = Vec::new();
    for c in 0..classes {
        let name = format!("C{k}x{c}");
        let base = (c > 0 && rng.below(2) == 0).then(|| format!("C{k}x{}", c - 1));
        let _ = write!(
            h,
            "struct {name}{} {{\n  {name}();\n  virtual ~{name}();\n",
            base.as_ref().map_or(String::new(), |b| format!(" : {b}"))
        );
        for m in 0..1 + rng.below(3) {
            let mname = format!("m{m}");
            if rng.below(2) == 0 {
                let _ = writeln!(
                    h,
                    "  virtual int {mname}(int a) {{ return a + {}; }}",
                    rng.below(100)
                );
            } else {
                let _ = writeln!(
                    h,
                    "  int {mname}(int a) {{ return a * {}; }}",
                    rng.below(9) + 2
                );
            }
            methods.push((name.clone(), mname));
        }
        let _ = writeln!(h, "  int v;\n}};");
        let _ = writeln!(ctors, "{name}::{name}() {{ v = {}; }}", rng.below(50));
        let _ = writeln!(ctors, "{name}::~{name}() {{ v = 0; }}");
    }
    let _ = writeln!(
        h,
        "inline int helper{k}(int x) {{ return x * {} + 1; }}\n#endif",
        rng.below(7) + 2
    );
    Header {
        text: h,
        ctors,
        methods,
    }
}

fn predict_body(k: usize, methods: &[(String, String)], rng: &mut Rng, tag: &str) -> String {
    let mut cpp = String::new();
    for f in 0..1 + rng.below(4) {
        let (c, m) = &methods[rng.below(methods.len())];
        if rng.below(2) == 0 {
            let _ = writeln!(
                cpp,
                "int {tag}f{f}({c} *p) {{ return p->{m}({}) + helper{k}({f}); }}",
                rng.below(9)
            );
        } else {
            let _ = writeln!(cpp, "int {tag}f{f}() {{ {c} o; return o.{m}({f}); }}");
        }
    }
    cpp
}

fn predict_tu(k: usize, rng: &mut Rng) -> (String, String) {
    let h = predict_header(k, rng);
    let body = predict_body(k, &h.methods, rng, "");
    (h.text, format!("#include \"p{k}.h\"\n{}{body}", h.ctors))
}

fn predict_corpus(c: &Compiler, scratch: &Path) -> Result<()> {
    let src = scratch.join("tu");
    std::fs::create_dir_all(&src)?;
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    let mut units = BTreeMap::new();
    let mut stems = Vec::new();
    for k in 0..24 {
        let (h, cpp) = predict_tu(k, &mut rng);
        srcio::own_write(src.join(format!("p{k}.h")), &h)?;
        let p = src.join(format!("t{k}.cpp"));
        srcio::own_write(&p, &cpp)?;
        let mut u = unit(&p, &["/O2", "/GX", "/YX"]);
        u.includes.push(src.clone());
        stems.push(u.stem.clone());
        units.insert(u.stem.to_ascii_lowercase(), u);
    }
    let d = Driver {
        compiler: c,
        units,
        order: stems.clone(),
        batches: BTreeMap::new(),
        work: scratch.join("tu-work"),
        jobs: msvc6_diff::par::default_jobs(),
    };
    let got = msvc6_diff::par::map(&stems, d.jobs, |s| d.bk(s, None, &[]));
    let (mut n, mut demand_exact, mut records) = (0usize, 0usize, 0usize);
    let mut bad = Vec::new();
    for (s, r) in stems.iter().zip(got) {
        let (il, obj) = r?.with_context(|| format!("{s}: no unit"))?;
        let names: Vec<String> = msvc6_diff::comdat::text(&obj)?
            .into_iter()
            .filter_map(|c| c.symbol)
            .collect();
        let refs = drain::refs_of_bytes(&msvc6_diff::comdat::read(&obj)?)?;
        let known: Vec<String> = names
            .iter()
            .filter(|n| il.fns.contains_key(*n))
            .cloned()
            .collect();
        records += known.len();
        n += 1;
        let pred = drain::predicted_demand(&known, &il, &refs);
        if drain::exact(&known, &pred) {
            demand_exact += 1;
        } else {
            bad.push(format!("{s}: object {known:?}\n   predicted {pred:?}"));
        }
    }
    println!(
        "vc6: tu-order --predict over {n} generated pch-create TU(s): {records} COMDAT(s) with a .gl record, \
         {demand_exact}/{n} EXACT with demand"
    );
    if records == 0 {
        bail!("no generated TU's .gl named its COMDATs");
    }
    if !bad.is_empty() {
        bail!(
            "the demand merge misses {} object(s):\n{}",
            bad.len(),
            bad.join("\n")
        );
    }
    let replay = d.replay(&stems)?;
    let creators = replay.iter().filter(|r| r.creator == r.stem).count();
    println!(
        "vc6: the shared-pch replay compiled {} TU(s), {creators} of them creating the pch",
        replay.len()
    );
    if replay.len() != stems.len() || creators != stems.len() {
        bail!("every generated TU has its own header, so each must create the shared pch");
    }
    shared_pch(c, scratch, &mut rng)
}

fn shared_pch(c: &Compiler, scratch: &Path, rng: &mut Rng) -> Result<()> {
    let src = scratch.join("use");
    std::fs::create_dir_all(&src)?;
    let h = predict_header(99, rng);
    srcio::own_write(src.join("p99.h"), &h.text)?;
    let mut units = BTreeMap::new();
    let mut stems = Vec::new();
    for k in 0..8 {
        let ctors = if k == 0 { h.ctors.as_str() } else { "" };
        let body = predict_body(99, &h.methods, rng, &format!("u{k}"));
        let p = src.join(format!("u{k}.cpp"));
        srcio::own_write(&p, format!("#include \"p99.h\"\n{ctors}{body}"))?;
        let mut u = unit(&p, &["/O2", "/GX", "/YX"]);
        u.includes.push(src.clone());
        stems.push(u.stem.clone());
        units.insert(u.stem.to_ascii_lowercase(), u);
    }
    let d = Driver {
        compiler: c,
        units,
        order: stems.clone(),
        batches: BTreeMap::new(),
        work: scratch.join("use-work"),
        jobs: 1,
    };
    let replay = d.replay(&stems)?;
    let users = replay.iter().filter(|r| r.creator != r.stem).count();
    let mut exact = 0usize;
    let mut bad = Vec::new();
    for r in &replay {
        let names: Vec<String> = msvc6_diff::comdat::text(&r.obj)?
            .into_iter()
            .filter_map(|c| c.symbol)
            .collect();
        let refs = drain::refs_of_bytes(&msvc6_diff::comdat::read(&r.obj)?)?;
        let known: Vec<String> = names
            .iter()
            .filter(|n| r.il.fns.contains_key(*n))
            .cloned()
            .collect();
        let pred = drain::predicted_demand(&known, &r.il, &refs);
        if drain::exact(&known, &pred) {
            exact += 1;
        } else {
            bad.push(format!(
                "{}: object {known:?}\n   predicted {pred:?}",
                r.stem
            ));
        }
    }
    println!(
        "vc6: a shared pch over {} TU(s) of one header: {users} USE an earlier TU's pch, {exact}/{} EXACT with demand",
        replay.len(),
        replay.len()
    );
    if users + 1 != replay.len() {
        bail!("every TU after the first should use the first one's pch");
    }
    if !bad.is_empty() {
        bail!("the demand merge misses under pch USE:\n{}", bad.join("\n"));
    }
    Ok(())
}

fn bss_tu(k: usize, rng: &mut Rng) -> String {
    let mut s = String::new();
    let mut uses = Vec::new();
    for i in 0..4 + rng.below(20) {
        let n = rng.name(&format!("g{i}_"));
        if rng.below(5) == 0 {
            let _ = writeln!(s, "int {n} = 0;");
        } else {
            let _ = writeln!(s, "int {n};");
        }
        uses.push(n);
    }
    for i in 0..rng.below(5) {
        let n = rng.name(&format!("s{i}_"));
        let _ = writeln!(s, "static int {n};");
        uses.push(n);
    }
    let ns = format!("N{k}");
    let nv = rng.name("nv_");
    let _ = writeln!(s, "namespace {ns} {{ int {nv}; }}");
    uses.push(format!("{ns}::{nv}"));
    let cm = rng.name("m_");
    let _ = writeln!(s, "struct K{k} {{ static int {cm}; }};\nint K{k}::{cm};");
    uses.push(format!("K{k}::{cm}"));
    let ls = rng.name("l_");
    let _ = writeln!(
        s,
        "int use{k}() {{ static int {ls}; return ++{ls} + {}; }}",
        uses.join(" + ")
    );
    s
}

fn bss_corpus(c: &Compiler, scratch: &Path) -> Result<()> {
    let src = scratch.join("bss");
    std::fs::create_dir_all(&src)?;
    let mut rng = Rng(0x2545_f491_4f6c_dd1d);
    let sess = c.runner.session()?;
    let tc = c.toolchain("vc6")?;
    let (mut tus, mut vars, mut appended) = (0usize, 0usize, 0usize);
    let mut bad = Vec::new();
    for k in 0..24 {
        let text = bss_tu(k, &mut rng);
        let p = src.join(format!("b{k}.cpp"));
        srcio::own_write(&p, &text)?;
        let out = compile_in(
            &sess,
            c.cache.as_ref(),
            &tc,
            &unit(&p, &["/O2"]),
            &src,
            false,
        )?;
        let bytes = msvc6_diff::comdat::read(&out.obj)?;
        let mut v = bss::vars_of(&bytes, None)?;
        bss::mark_zero_inits(&text, &mut v)?;
        vars += v.len();
        appended += v.iter().filter(|x| x.zero_init.is_some()).count();
        let verdict = bss::judge(&format!("b{k}"), v)?;
        tus += 1;
        if !verdict.model_exact {
            bad.push(bss::verdict_lines(&verdict).join("\n"));
        }
    }
    println!(
        "vc6: bss-order over {tus} generated TU(s): {vars} .bss variable(s), {appended} appended `= 0`; \
         the model is exact on {}",
        tus - bad.len()
    );
    if vars == 0 {
        bail!("no generated TU put a variable in .bss");
    }
    if !bad.is_empty() {
        bail!("the .bss model misses:\n{}", bad.join("\n"));
    }
    let seat = dataseat::bss_of(&msvc6_diff::comdat::read(&src.join("b0.obj"))?)?;
    if seat.is_none() {
        bail!("b0.obj holds no .bss");
    }
    Ok(())
}

fn frame_tu(k: usize, rng: &mut Rng) -> String {
    let mut s = String::new();
    for f in 0..6 {
        let mut decls = Vec::new();
        let _ = writeln!(s, "int fr{k}x{f}(int arg) {{");
        for _ in 0..2 + rng.below(8) {
            let n = rng.name("v");
            let _ = writeln!(s, "  int {n} = arg + {};", rng.below(99));
            decls.push(n);
        }
        let inner = rng.name("in");
        let _ = writeln!(s, "  if (arg) {{ int {inner} = arg * 2; arg += {inner}; }}");
        let _ = writeln!(s, "  return arg + {};\n}}", decls.join(" + "));
    }
    let _ = writeln!(
        s,
        "struct F{k} {{ int a; int run(int x); }};\nint F{k}::run(int x) {{ int q = x; int zz = a; return q + zz; }}"
    );
    s
}

fn frame_corpus(c: &Compiler, scratch: &Path) -> Result<()> {
    let src = scratch.join("frame");
    std::fs::create_dir_all(&src)?;
    let mut rng = Rng(0x6a09_e667_f3bc_c908);
    let sess = c.runner.session()?;
    let tc = c.toolchain("vc6")?;
    let mut objs = Vec::new();
    let mut zi = Vec::new();
    for k in 0..8 {
        let text = frame_tu(k, &mut rng);
        let p = src.join(format!("f{k}.cpp"));
        srcio::own_write(&p, &text)?;
        let out = compile_in(&sess, None, &tc, &unit(&p, &["/Od", "/Z7"]), &src, false)?;
        objs.push((
            out.obj.display().to_string(),
            msvc6_diff::comdat::read(&out.obj)?,
        ));
        let zdir = src.join("zi");
        let out = compile_in(&sess, None, &tc, &unit(&p, &["/Od", "/ZI"]), &zdir, false)?;
        zi.push((
            out.obj.display().to_string(),
            msvc6_diff::comdat::read(&out.obj)?,
        ));
    }
    let (lines, t) = frameorder::report(&objs, None, false);
    println!(
        "vc6: frame-order over {} generated /Od /Z7 object(s): {}/{} HASH-domain function(s) hold",
        objs.len(),
        t.hash_ok,
        t.hash
    );
    if t.hash == 0 || t.hash_ok != t.hash || t.decl != 0 {
        bail!("the frame model misses:\n{}", lines.join("\n"));
    }
    let (lines, t) = frameorder::report(&zi, None, false);
    println!(
        "vc6: frame-order over {} generated /Od /ZI object(s): {}/{} DECL-domain function(s) put `this` first",
        zi.len(),
        t.decl_ok,
        t.decl
    );
    if t.decl == 0 || t.decl_ok != t.decl || t.hash != 0 {
        bail!(
            "the /ZI frames do not read as declaration order:\n{}",
            lines.join("\n")
        );
    }
    Ok(())
}

const HEADER_BASE: &str = "struct S { int a; void f(); };\n";

const HEADER_EDITS: [(&str, &str); 3] = [
    ("a blank line", "struct S { int a; void f(); };\n\n"),
    (
        "g(int, int)",
        "struct S { int a; void f(); void g(int, int); };\n",
    ),
    (
        "g(double*, unsigned short, char**)",
        "struct S { int a; void f(); void g(double*, unsigned short, char**); };\n",
    ),
];

fn header_price_corpus(c: &Compiler, scratch: &Path) -> Result<()> {
    let dir = scratch.join("hdr-price");
    let src = dir.join("src");
    let base = dir.join("base");
    for d in [&src, &base] {
        std::fs::create_dir_all(d)?;
    }
    srcio::own_write(base.join("shared.h"), HEADER_BASE)?;
    let mut units: Vec<(String, PathBuf)> = Vec::new();
    for k in 0..3u32 {
        let p = src.join(format!("inc{k}.cpp"));
        let mut text = String::from("#include \"shared.h\"\n");
        for j in 0..=k {
            writeln!(
                text,
                "int use{j}(S* s, int x) {{ if (x > {j}) {{ s->f(); return x + {j}; }} return s->a; }}"
            )?;
        }
        srcio::own_write(&p, &text)?;
        units.push((format!("inc{k}"), p));
    }
    let work = dir.join("work");
    std::fs::create_dir_all(&work)?;
    let before = msvc6_model::c1count::count_with(HEADER_BASE, false)?.total;
    let mut prices: BTreeSet<i64> = BTreeSet::new();
    let mut misses: Vec<String> = Vec::new();
    let mut bases: BTreeSet<u32> = BTreeSet::new();
    for (e, (what, text)) in HEADER_EDITS.iter().enumerate() {
        let with = dir.join(format!("with{e}"));
        std::fs::create_dir_all(&with)?;
        srcio::own_write(with.join("shared.h"), text)?;
        let price = msvc6_model::c1count::count_with(text, false)?.total - before;
        prices.insert(price);
        for (stem, p) in &units {
            let mut u = unit(p, &["/Od"]);
            u.includes = vec![base.clone()];
            let b = msvc6_model::hdrprice::c1_counter(c, &u, &work, Variant::Base)?;
            let o = msvc6_model::hdrprice::c1_counter(
                c,
                &u,
                &work,
                Variant::Header {
                    from: &base,
                    to: &with,
                    src: None,
                },
            )?;
            let shift = i64::from(o.0) - i64::from(b.0);
            println!(
                "vc6: hdr price {stem}, {what}: C1 counter {} -> {}, shift {shift:+}, the model's price {price:+} [C1XX {}]",
                b.0,
                o.0,
                msvc6_model::hdrprice::front_end_build(b.1)
            );
            bases.insert(b.0);
            if shift != price {
                misses.push(format!(
                    "{stem}, {what}: measured {shift:+}, priced {price:+}"
                ));
            }
        }
    }
    println!(
        "vc6: hdr price over {} includer(s) of one header and {} edit(s): {} distinct base counter(s), prices {prices:?}",
        units.len(),
        HEADER_EDITS.len(),
        bases.len()
    );
    if bases.len() < units.len() {
        bail!(
            "each includer was to have its own C1 counter (they differ in body count); the bases \
             are {bases:?}"
        );
    }
    if prices.len() < HEADER_EDITS.len() {
        bail!(
            "the edits were to carry distinct prices, so a measurement that ignores the edit cannot pass; \
             the model prices them {prices:?}"
        );
    }
    if !misses.is_empty() {
        bail!(
            "hdr price's measured C1 shift is not the model's price of the edit in every includer:\n  {}",
            misses.join("\n  ")
        );
    }
    Ok(())
}

fn run() -> Result<bool> {
    let Some(root) = env("MSVC6T_VC6") else {
        println!("{SKIP}");
        if require() {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no toolchain is configured");
        }
        return Ok(false);
    };
    let scratch = std::env::temp_dir().join(format!("msvc6t-model-test-{}", std::process::id()));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch)?;
    }
    std::fs::create_dir_all(&scratch)?;
    let file = config(&scratch, &root)?;
    let p = Project::load(&file)?;
    let c = Compiler::new(&p)?;
    let result = c1_totals(&c, &scratch)
        .and_then(|()| c1_spans(&c, &scratch))
        .and_then(|()| predict_corpus(&c, &scratch))
        .and_then(|()| bss_corpus(&c, &scratch))
        .and_then(|()| frame_corpus(&c, &scratch))
        .and_then(|()| header_price_corpus(&c, &scratch));
    let pool = scratch.join("prefixes");
    for (_, prefix) in msvc6_toolchain::reap::slot_locks(&pool) {
        msvc6_toolchain::reap::retire(&pool, &prefix)?;
    }
    if result.is_ok() {
        std::fs::remove_dir_all(&scratch)?;
    } else {
        println!("vc6: the scratch directory is kept: {}", scratch.display());
    }
    result.map(|()| true)
}

fn main() -> ExitCode {
    match run() {
        Ok(true) => {
            println!("vc6: PASS");
            ExitCode::SUCCESS
        }
        Ok(false) => ExitCode::SUCCESS,
        Err(e) => {
            println!("vc6: FAIL: {e:#}");
            ExitCode::FAILURE
        }
    }
}
