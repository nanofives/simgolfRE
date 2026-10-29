use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::compile::{Unit, compile_in};
use msvc6_diff::env::Env;
use msvc6_diff::seat;
use msvc6_gate::{addrsym, calltargets, relocid};
use msvc6_image::pe::Image;
use msvc6_project::config::Project;
use msvc6_project::mapfile::Map;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";
const CFLAGS: [&str; 2] = ["/Od", "/Gy"];
const TUS: [&str; 2] = ["caller.c", "callees.c"];
const LINK: [&str; 8] = [
    "/subsystem:console",
    "/entry:entry",
    "/nodefaultlib",
    "/opt:noref",
    "/map:app.map",
    "/out:app.exe",
    "caller.obj",
    "callees.obj",
];
const PLANT: (&str, &str, &str) = (
    "caller.c",
    "g_a = 3;\n    return right();",
    "g_b = 3;\n    return wrong();",
);

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn quoted(s: &str) -> String {
    format!("'{s}'")
}

fn path_str(p: &Path) -> String {
    p.display().to_string()
}

fn toolchain_text(scratch: &Path, root: &str) -> String {
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
        quoted(&path_str(&scratch.join("prefixes"))),
        quoted(&path_str(&scratch.join("tokens")))
    ));
    if let Some(t) = env("MSVC6T_WINE_TEMPLATE") {
        text.push_str(&format!("template = {}\n", quoted(&t)));
    }
    if let Some(m) = env("MSVC6T_MSVCRT") {
        text.push_str(&format!("native_msvcrt = {}\n", quoted(&m)));
    }
    text.push_str(&format!(
        "\n[cache]\ndir = {}\n",
        quoted(&path_str(&scratch.join("cache")))
    ));
    text
}

fn unit(source: &Path) -> Result<Unit> {
    let name = |s: Option<&std::ffi::OsStr>| {
        s.and_then(|f| f.to_str())
            .map(str::to_string)
            .context("a fixture with no file name")
    };
    Ok(Unit {
        source: source.to_path_buf(),
        stem: name(source.file_stem())?,
        file: name(source.file_name())?,
        cflags: CFLAGS.iter().map(|s| (*s).to_string()).collect(),
        includes: Vec::new(),
        include_env: None,
        toolchain: "vc6".into(),
    })
}

fn build(
    sess: &Session<'_>,
    tc: &Toolchain,
    runner: &Runner,
    src: &Path,
    dir: &Path,
) -> Result<()> {
    for tu in TUS {
        compile_in(sess, None, tc, &unit(&src.join(tu))?, dir, false)
            .with_context(|| format!("compiling {tu} into {}", dir.display()))?;
    }
    let mut argv = vec!["/nologo".to_string()];
    argv.extend(LINK.iter().map(|s| (*s).to_string()));
    let env = msvc6_toolchain::runner::tool_env(tc, &runner.winepath);
    let out = sess.run(&tc.tool("LINK.EXE")?, &argv, Some(dir), &env, None)?;
    if !out.ok() {
        bail!("LINK {} failed\n{}", argv.join(" "), out.text().trim());
    }
    Ok(())
}

fn extents(image: &Path, map: &Path) -> Result<String> {
    let img = Image::load(image)?;
    let text = img.section(".text").context("the reference has no .text")?;
    let lo = text.rva.get();
    let bytes = img.section_bytes(text);
    let hi = lo.saturating_add(u32::try_from(bytes.len())?);
    let m = Map::read(map)?;
    let mut starts: Vec<(u32, String)> = m
        .symbols
        .iter()
        .filter(|s| s.function && s.rva >= lo && s.rva < hi)
        .map(|s| (s.rva, s.name.clone()))
        .collect();
    starts.sort();
    starts.dedup_by_key(|s| s.0);
    let base = img.opt.image_base.get();
    let mut csv = String::from("address,name,type,size\n");
    for (k, (rva, name)) in starts.iter().enumerate() {
        let next = starts.get(k + 1).map_or(hi, |n| n.0);
        let span = bytes
            .get(usize::try_from(rva - lo)?..usize::try_from(next - lo)?)
            .unwrap_or_default();
        let fill = span
            .iter()
            .rev()
            .take_while(|x| matches!(**x, 0xcc | 0x90))
            .count();
        csv.push_str(&format!(
            "{:#x},{name},function,{}\n",
            base.saturating_add(*rva),
            span.len().saturating_sub(fill)
        ));
    }
    Ok(csv)
}

struct Gates {
    checked: usize,
    wrong: Vec<(String, u32, u32)>,
    relocid: Vec<(String, relocid::Verdict)>,
    relocid_checked: usize,
    addrsym: usize,
}

fn gates(env: &Env, dir: &Path, f: u32) -> Result<Gates> {
    let objs: Vec<PathBuf> = TUS
        .iter()
        .map(|t| dir.join(t.replace(".c", ".obj")))
        .collect();
    let ct = calltargets::measure(&calltargets::Inputs {
        env,
        objs: &objs,
        census: true,
        map_data: &Default::default(),
    })?;
    let seats = seat::seat_map(env, &objs, 1);
    if !seats.refused.is_empty() {
        bail!("objects refused: {:?}", seats.refused);
    }
    let rel = relocid::measure(&relocid::Inputs {
        env,
        objs: &objs,
        seats: &seats.seats,
        archives: &[],
        notes_only: &[],
        only: None,
        jobs: 1,
    });
    let ours = Image::load(dir.join("app.exe"))?;
    let map = Map::read(&dir.join("app.map"))?;
    let linked = addrsym::Linked::new(&ours, &map);
    let ad = addrsym::run(env, &seats.seats, &linked, &[f], true, false, 1)?;
    Ok(Gates {
        checked: ct.checked,
        wrong: ct
            .wrong
            .iter()
            .map(|w| (w.symbol.clone(), w.symbol_is_at, w.reference_targets))
            .collect(),
        relocid: rel
            .rows
            .iter()
            .filter(|r| r.verdict.finding())
            .map(|r| (r.symbol.clone(), r.verdict))
            .collect(),
        relocid_checked: rel.checked,
        addrsym: ad.mismatches(),
    })
}

fn require() -> bool {
    env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1")
}

fn closed_loop(scratch: &Path, root: &str) -> Result<()> {
    let repo = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let (src, planted) = (scratch.join("src"), scratch.join("planted"));
    for d in [&src, &planted] {
        std::fs::create_dir_all(d)?;
    }
    for tu in TUS {
        let text = srcio::own_read(repo.join("fixtures/gate").join(tu))?;
        srcio::own_write(src.join(tu), &text)?;
        let (at, from, to) = PLANT;
        let text = if tu == at {
            if !text.contains(from) {
                bail!("the plant's text is not in {tu}");
            }
            text.replacen(from, to, 1)
        } else {
            text
        };
        srcio::own_write(planted.join(tu), &text)?;
    }
    let file = scratch.join("msvc6t.toml");
    let text = toolchain_text(scratch, root);
    srcio::own_write(&file, &text)?;
    let p = Project::load(&file)?;
    let cfg = p.toolchain_config();
    let tc = Toolchain::named(&cfg, "vc6")?;
    let runner = Runner::from_config(&cfg)?;
    let sess = runner.session()?;
    println!("vc6: toolchain {} at {}", tc.id, tc.root.display());
    let (refdir, ours, control) = (
        scratch.join("ref"),
        scratch.join("ours"),
        scratch.join("control"),
    );
    build(&sess, &tc, &runner, &src, &refdir)?;
    build(&sess, &tc, &runner, &src, &control)?;
    build(&sess, &tc, &runner, &planted, &ours)?;
    let reference = refdir.join("app.exe");
    srcio::own_write(
        scratch.join("APP.extents.csv"),
        &extents(&reference, &refdir.join("app.map"))?,
    )?;
    let sha = msvc6_base::digest::sha256_hex(&std::fs::read(&reference)?);
    let full = format!(
        "{text}\n[[image]]\nid = \"APP\"\nreference = {}\nsha256 = \"{sha}\"\ncompiler = \"vc6\"\nlinker = \"vc6\"\n\
         functions = [\"reccmp-csv:APP.extents.csv\", \"map:ref/app.map\"]\nnames = [\"exports\", \"map:ref/app.map\"]\n",
        quoted(&path_str(&reference))
    );
    srcio::own_write(&file, &full)?;
    let p = Project::load(&file)?;
    let env = Env::load(&p, 0)?;
    let f = env.rva_of("_f")?;
    let (right, wrong) = (env.rva_of("_right")?, env.rva_of("_wrong")?);
    let c = gates(&env, &control, f)?;
    println!(
        "vc6: control: calltargets {} checked, {} wrong; relocid {} checked, findings {:?}; addrsym {} mismatch(es)",
        c.checked,
        c.wrong.len(),
        c.relocid_checked,
        c.relocid,
        c.addrsym
    );
    if c.checked < 2
        || !c.wrong.is_empty()
        || !c.relocid.is_empty()
        || c.addrsym != 0
        || c.relocid_checked == 0
    {
        bail!("the reference's own objects do not pass every gate");
    }
    let g = gates(&env, &ours, f)?;
    println!(
        "vc6: planted: calltargets {} checked, wrong {:?}; relocid findings {:?}; addrsym {} mismatch(es)",
        g.checked, g.wrong, g.relocid, g.addrsym
    );
    let want_wrong = vec![("_wrong".to_string(), wrong, right)];
    let want_rel = vec![
        ("_g_b".to_string(), relocid::Verdict::Mismatch),
        ("_wrong".to_string(), relocid::Verdict::Mismatch),
    ];
    let mut got_rel = g.relocid.clone();
    got_rel.sort();
    if g.wrong != want_wrong || got_rel != want_rel || g.addrsym != 2 {
        bail!("the planted wrong callee and global are not caught by every gate");
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
    let scratch = std::env::temp_dir().join(format!("msvc6t-gate-test-{}", std::process::id()));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch)?;
    }
    std::fs::create_dir_all(&scratch)?;
    let result = closed_loop(&scratch, &root);
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
