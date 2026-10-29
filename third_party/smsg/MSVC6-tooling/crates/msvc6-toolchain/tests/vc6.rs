use anyhow::{Context as _, Result, bail};
use msvc6_toolchain::cache::{Cache, Outcome, Outputs};
use msvc6_toolchain::compile::{Job, compile};
use msvc6_toolchain::config::Config;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::Runner;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn toml_str(s: &str) -> String {
    format!("'{s}'")
}

fn config(scratch: &Path) -> Result<Option<(Config, String)>> {
    if let Some(file) = env("MSVC6T_CONFIG") {
        let cfg = msvc6_project::config::Project::load(Path::new(&file))?.toolchain_config();
        let tc = env("MSVC6T_TOOLCHAIN")
            .or_else(|| cfg.toolchains.keys().next().cloned())
            .context("MSVC6T_CONFIG names a config with no [toolchain.*]")?;
        return Ok(Some((cfg, tc)));
    }
    let Some(root) = env("MSVC6T_VC6") else {
        return Ok(None);
    };
    let mut text = format!(
        "schema = 1\n[toolchain.vc6]\nroot = {}\n[runner]\n",
        toml_str(&root)
    );
    let command: Vec<String> = env("MSVC6T_WINE")
        .unwrap_or_else(|| "wine".into())
        .split_whitespace()
        .map(toml_str)
        .collect();
    text.push_str(&format!("command = [{}]\n", command.join(", ")));
    text.push_str(&format!(
        "prefixes = {}\ntokens = {}\n",
        toml_str(&scratch.join("prefixes").display().to_string()),
        toml_str(&scratch.join("tokens").display().to_string())
    ));
    if let Some(t) = env("MSVC6T_WINE_TEMPLATE") {
        text.push_str(&format!("template = {}\n", toml_str(&t)));
    }
    if let Some(m) = env("MSVC6T_MSVCRT") {
        text.push_str(&format!("native_msvcrt = {}\n", toml_str(&m)));
    }
    let file = scratch.join("msvc6t.toml");
    let cfg = msvc6_project::config::Project::parse(&text, &file)?.toolchain_config();
    Ok(Some((cfg, "vc6".to_string())))
}

fn is_coff_i386(b: &[u8]) -> bool {
    b.get(..2) == Some(&[0x4c, 0x01])
}

fn run() -> Result<bool> {
    let scratch = std::env::temp_dir().join(format!("msvc6t-vc6-test-{}", std::process::id()));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch)?;
    }
    let Some((cfg, tc_id)) = config(&scratch)? else {
        println!("{SKIP}");
        if env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1") {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no toolchain is configured");
        }
        return Ok(false);
    };
    std::fs::create_dir_all(&scratch)?;
    let tc = Toolchain::named(&cfg, &tc_id)?;
    let runner = Runner::from_config(&cfg)?;
    println!("vc6: toolchain {} at {}", tc.id, tc.root.display());
    let dir = scratch.join("hello");
    std::fs::create_dir_all(&dir)?;
    let fixture = repo().join("fixtures/c/hello.c");
    std::fs::copy(&fixture, dir.join("hello.c"))
        .with_context(|| format!("copying {}", fixture.display()))?;
    let cache = Cache::new(scratch.join("cache"))?;
    let job = Job {
        tc: &tc,
        dir: dir.clone(),
        args: ["/nologo", "/c", "/O2", "/Fohello.obj", "hello.c"]
            .iter()
            .map(|s| (*s).to_string())
            .collect(),
        outs: Outputs {
            obj: dir.join("hello.obj"),
            cod: None,
        },
        env: Vec::new(),
        receipt: true,
    };
    let sess = runner.session()?;
    let first = compile(&sess, Some(&cache), &job)?;
    let obj = std::fs::read(&job.outs.obj).context("the first compile wrote no hello.obj")?;
    println!(
        "vc6: first compile: {} ({} bytes)",
        first.outcome.label(),
        obj.len()
    );
    if !is_coff_i386(&obj) {
        bail!("hello.obj is not an i386 COFF object");
    }
    if first.outcome != Outcome::Stored {
        bail!(
            "the first compile into an empty cache was {}, not stored",
            first.outcome.label()
        );
    }
    std::fs::remove_file(&job.outs.obj)?;
    let second = compile(&sess, Some(&cache), &job)?;
    println!("vc6: second compile: {}", second.outcome.label());
    if !second.outcome.is_hit() {
        bail!(
            "the second compile was {}, not a cache hit",
            second.outcome.label()
        );
    }
    if std::fs::read(&job.outs.obj)? != obj {
        bail!("the cache restored another object than the one it stored");
    }
    msvc6_toolchain::receipt::current(&job.outs.obj, Some(&tc.identity()))
        .context("the receipt of hello.obj")?;
    println!("vc6: receipt current");
    if runner.msvcrt.is_none() {
        println!("SKIP vc6 crtguard: no native msvcrt configured (set MSVC6T_MSVCRT)");
        if env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1") {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no native msvcrt is configured");
        }
        return Ok(true);
    }
    let link = |overrides: Option<&str>| -> Result<(String, bool)> {
        let mut extra = vec![("WINEDEBUG".to_string(), "-all,+loaddll".to_string())];
        if let Some(o) = overrides {
            extra.push(("WINEDLLOVERRIDES".to_string(), o.to_string()));
        }
        let mut env = msvc6_toolchain::runner::tool_env(&tc, &runner.winepath);
        env.extend(extra);
        let out = sess.run(
            &tc.tool("LINK.EXE")?,
            &[
                "/nologo".to_string(),
                "/subsystem:console".to_string(),
                "/out:hello.exe".to_string(),
                "hello.obj".to_string(),
            ],
            Some(&dir),
            &env,
            None,
        )?;
        if overrides.is_none() && !out.ok() {
            bail!("LINK on the native msvcrt failed\n{}", out.text().trim());
        }
        Ok(msvc6_toolchain::crtguard::line(
            &msvc6_base::srcio::tool_text(&out.stderr),
            &[],
        ))
    };
    let (native, ok) = link(None)?;
    println!("vc6: native msvcrt: {native}");
    if !ok {
        bail!("the crt guard failed a LINK run on the native msvcrt");
    }
    let (builtin, ok) = link(Some("msvcrt=b"))?;
    println!("vc6: builtin msvcrt: {builtin}");
    if ok {
        bail!("the crt guard passed a LINK run on wine's builtin msvcrt");
    }
    drop(sess);
    let pool = scratch.join("prefixes");
    for (_, prefix) in msvc6_toolchain::reap::slot_locks(&pool) {
        msvc6_toolchain::reap::retire(&pool, &prefix)?;
    }
    std::fs::remove_dir_all(&scratch)?;
    Ok(true)
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
