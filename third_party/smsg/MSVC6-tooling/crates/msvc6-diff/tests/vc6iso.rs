use anyhow::{Context as _, Result, bail};
use msvc6_diff::attrib::{self, Listing};
use msvc6_diff::comdat::{self, Comdat};
use msvc6_diff::compile::{Unit, compile_in};
use msvc6_diff::iso::{self, Fate, Target};
use msvc6_toolchain::cache::Cache;
use msvc6_toolchain::config::Config;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";
const SYMBOL: &str = "?Target@@YAHH@Z";

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
    let file = scratch.join("msvc6t.toml");
    let cfg = msvc6_project::config::Project::parse(&text, &file)?.toolchain_config();
    Ok(Some((cfg, "vc6".to_string())))
}

struct Bench<'a> {
    sess: &'a Session<'a>,
    cache: &'a Cache,
    tc: &'a Toolchain,
    cflags: Vec<String>,
}

fn compile(b: &Bench<'_>, text: &[u8], dir: &Path) -> Result<(Comdat, Listing)> {
    if dir.exists() {
        std::fs::remove_dir_all(dir)?;
    }
    std::fs::create_dir_all(dir)?;
    let src = dir.join("iso.cpp");
    msvc6_base::srcio::write_bytes(&src, text)?;
    let u = Unit {
        source: src,
        stem: "iso".into(),
        file: "iso.cpp".into(),
        cflags: b.cflags.clone(),
        includes: Vec::new(),
        include_env: None,
        toolchain: b.tc.id.clone(),
    };
    let out = compile_in(b.sess, Some(b.cache), b.tc, &u, dir, true)?;
    let c = comdat::text(&out.obj)?
        .into_iter()
        .find(|c| c.symbol.as_deref() == Some(SYMBOL))
        .with_context(|| format!("{} defines no `{SYMBOL}`", out.obj.display()))?;
    Ok((c, out.cod.as_deref().map(attrib::read).unwrap_or_default()))
}

fn lines(l: &Listing) -> Vec<(u32, String)> {
    let mut v: Vec<(u32, String)> = l
        .procs
        .get(SYMBOL)
        .map(|p| {
            p.sites
                .iter()
                .filter(|s| s.line != 0)
                .map(|s| (s.line, s.text.clone()))
                .collect()
        })
        .unwrap_or_default();
    v.sort_unstable();
    v.dedup();
    v
}

fn prove_one(b: &Bench<'_>, src: &[u8], scratch: &Path, tag: &str) -> Result<()> {
    let (tu, tl) = compile(b, src, &scratch.join(format!("{tag}-tu")))?;
    let target = Target::of_symbol(SYMBOL).context("the fixture's target symbol")?;
    let mut pad = 0usize;
    let mut r = iso::reduce(src, &target, pad)?;
    let dropped = r.names(Fate::Dropped);
    if dropped != ["Unused", "Other", "Later"] {
        bail!("{tag}: dropped {dropped:?}, not Unused, Other and Later");
    }
    let helpers = r.names(Fate::Helper);
    for h in [
        "Point::Point",
        "Point::~Point",
        "Point::Move",
        "Helper",
        "Twice",
        "Biggest",
        "Plain",
    ] {
        if !helpers.iter().any(|x| x == h) {
            bail!("{tag}: {h} is not kept as a helper: {helpers:?}");
        }
    }
    let dir = scratch.join(format!("{tag}-iso"));
    let (mut mine, mut ml) = compile(b, &r.text, &dir)?;
    for _ in 0..iso::PAD_TRIES {
        let (Some(x), Some(y)) = (tl.first_label(SYMBOL), ml.first_label(SYMBOL)) else {
            break;
        };
        let Some(next) = iso::pad_step(x, y, pad) else {
            break;
        };
        pad = next;
        r = iso::reduce(src, &target, pad)?;
        (mine, ml) = compile(b, &r.text, &dir)?;
    }
    let labels = (tl.first_label(SYMBOL), ml.first_label(SYMBOL));
    let p = iso::prove(&tu, &mine);
    println!(
        "vc6 iso {tag}: {} of {} lines, {} helper(s), {} dropped, pad {pad}, labels {labels:?}, {:#x} bytes, \
         {} renumbered: {}",
        r.lines(),
        iso::count_lines(src),
        helpers.len(),
        dropped.len(),
        p.len.0,
        p.renumbered.len(),
        if p.faithful() { "FAITHFUL" } else { "DIFFERS" }
    );
    if !p.faithful() {
        bail!("{tag}: the iso DIFFERS: {p:?}");
    }
    if let (Some(x), Some(y)) = labels
        && x % iso::LABEL_MODULUS != y % iso::LABEL_MODULUS
    {
        bail!("{tag}: the counter pad left the label counter moved: $L{x} vs $L{y}");
    }
    ml.retext(&iso::line_texts(&r.text));
    if lines(&tl).is_empty() || lines(&tl) != lines(&ml) {
        bail!(
            "{tag}: the listings name other source lines: TU {:?}, iso {:?}",
            lines(&tl),
            lines(&ml)
        );
    }
    println!(
        "vc6 iso {tag}: the iso's listing names the TU's {} source line(s) of the target, text and number",
        lines(&tl).len()
    );
    let text = msvc6_base::srcio::decode(&r.text);
    let stripped: String = text
        .lines()
        .filter(|l| !l.starts_with("#line"))
        .map(|l| format!("{l}\n"))
        .collect();
    let (bad, _) = compile(
        b,
        stripped.as_bytes(),
        &scratch.join(format!("{tag}-noline")),
    )?;
    if iso::prove(&tu, &bad).faithful() {
        bail!("{tag}: the proof did not see __LINE__ move with the #line directives removed");
    }
    println!("vc6 iso {tag}: without its #line directives the iso DIFFERS, as it must");
    Ok(())
}

fn run() -> Result<bool> {
    let scratch = std::env::temp_dir().join(format!("msvc6t-vc6iso-test-{}", std::process::id()));
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
    let cache = Cache::new(scratch.join("cache"))?;
    let src = std::fs::read(repo().join("fixtures/c/iso.cpp")).context("fixtures/c/iso.cpp")?;
    {
        let sess = runner.session()?;
        for (tag, flags) in [("o2", ["/O2", "/GX-"]), ("od", ["/Od", "/Gy"])] {
            let b = Bench {
                sess: &sess,
                cache: &cache,
                tc: &tc,
                cflags: flags.iter().map(|s| (*s).to_string()).collect(),
            };
            prove_one(&b, &src, &scratch, tag)?;
        }
    }
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
            println!("vc6 iso: PASS");
            ExitCode::SUCCESS
        }
        Ok(false) => ExitCode::SUCCESS,
        Err(e) => {
            println!("vc6 iso: FAIL: {e:#}");
            ExitCode::FAILURE
        }
    }
}
