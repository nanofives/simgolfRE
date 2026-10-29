#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::compile::{Compiler, Unit, compile_in};
use msvc6_il::grammar::Grammar;
use msvc6_il::unit::Files;
use msvc6_project::config::Project;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
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
    let file = scratch.join("msvc6t.toml");
    srcio::own_write(&file, &text)?;
    Ok(file)
}

fn fixture(name: &str) -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../fixtures/c")
        .join(name)
}

fn unit(source: &Path, cflags: &[&str]) -> Unit {
    let stem = source.file_stem().unwrap().to_string_lossy().into_owned();
    let mut cflags: Vec<String> = cflags.iter().map(|s| (*s).to_string()).collect();
    cflags.push(format!("/Bk{stem}."));
    Unit {
        source: source.to_path_buf(),
        file: source.file_name().unwrap().to_string_lossy().into_owned(),
        stem,
        cflags,
        includes: Vec::new(),
        include_env: None,
        toolchain: "vc6".into(),
    }
}

fn kept(
    c: &Compiler,
    scratch: &Path,
    source: &Path,
    cflags: &[&str],
    tag: &str,
) -> Result<(Files, String)> {
    let u = unit(source, cflags);
    let dir = scratch.join(tag);
    let tc = c.toolchain("vc6")?;
    let sess = c.runner.session()?;
    let out = compile_in(&sess, None, &tc, &u, &dir, true)?;
    let listing = srcio::tool_text(&std::fs::read(out.cod.context("no listing")?)?);
    let files = Files::open(&dir.join(format!("{}.", u.stem)), None)?;
    Ok((files, listing))
}

fn run() -> Result<bool> {
    let Some(root) = env("MSVC6T_VC6") else {
        println!("{SKIP}");
        if env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1") {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no toolchain is configured");
        }
        return Ok(false);
    };
    let scratch = std::env::temp_dir().join(format!("msvc6t-il-test-{}", std::process::id()));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch)?;
    }
    std::fs::create_dir_all(&scratch)?;
    let p = Project::load(&config(&scratch, &root)?)?;
    let c = Compiler::new(&p)?;
    let g = Grammar::load(&c.toolchain("vc6")?.tool("C2.DLL")?)?;
    println!(
        "vc6: the IL grammar of {} has {} ops and {} sub-ops",
        g.source,
        g.fmt.len(),
        g.pragma.len()
    );
    let mut failed = Vec::new();
    for (source, flags, tag) in [
        (fixture("hello.c"), vec!["/O2"], "hello"),
        (fixture("inline.cpp"), vec!["/O2", "/GX"], "inline-gx"),
        (fixture("inline.cpp"), vec!["/O2", "/Ob2"], "inline-ob2"),
        (fixture("inline.cpp"), vec!["/Od"], "inline-od"),
    ] {
        let (files, listing) = kept(&c, &scratch, &source, &flags, tag)?;
        let text = msvc6_il::dump::dump(&files, &g, &msvc6_il::dump::Opts::default())?;
        let ids = msvc6_il::ids::load(&files, &g)?;
        let off: Vec<&str> = ids
            .funcs
            .iter()
            .filter(|f| i64::try_from(f.recs).ok() != Some(f.s6d))
            .map(|f| f.name.as_str())
            .collect();
        let u = msvc6_il::unit::Unit::read(&files)?;
        let check = msvc6_il::inline::check(&u, &files, &g, &listing, &[], false);
        println!(
            "vc6: {tag}: {} dump line(s), {} definition(s), {check_line}",
            text.lines().count(),
            ids.funcs.len(),
            check_line = check.line()
        );
        if !off.is_empty() {
            failed.push(format!("{tag}: records != s6d for {}", off.join(", ")));
        }
        if check.agree != check.pairs {
            for d in &check.diffs {
                failed.push(format!(
                    "{tag}: {} -> {}: plan {} call(s), listing {}",
                    d.root, d.callee, d.planned, d.listed
                ));
            }
        }
        if tag.starts_with("inline-o") && tag != "inline-od" && check.expanded == 0 {
            failed.push(format!("{tag}: nothing planned to expand"));
        }
    }
    let pool = scratch.join("prefixes");
    for (_, prefix) in msvc6_toolchain::reap::slot_locks(&pool) {
        msvc6_toolchain::reap::retire(&pool, &prefix)?;
    }
    if failed.is_empty() {
        std::fs::remove_dir_all(&scratch)?;
        return Ok(true);
    }
    println!("vc6: the scratch directory is kept: {}", scratch.display());
    bail!("{}", failed.join("\n"))
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
