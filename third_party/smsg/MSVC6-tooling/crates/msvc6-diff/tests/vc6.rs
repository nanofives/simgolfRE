use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::compile::{Compiler, Unit, compile_in};
use msvc6_diff::env::Env;
use msvc6_diff::oracle::{AUTO, ByteOracle as _, Members, Try};
use msvc6_diff::{objs, par, seat, segrank};
use msvc6_image::pe::Image;
use msvc6_project::config::Project;
use msvc6_project::mapfile::Map;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";
const CFLAGS: [&str; 2] = ["/O2", "/Gy"];
const DLL_CFLAGS: [&str; 2] = ["/Od", "/Gy"];

struct Img {
    id: &'static str,
    file: &'static str,
    map: &'static str,
    tus: &'static [&'static str],
    cflags: [&'static str; 2],
    link: &'static [&'static str],
}

const IMAGES: [Img; 2] = [
    Img {
        id: "APP",
        file: "app.exe",
        map: "app.map",
        tus: &["app.c", "shape.cpp"],
        cflags: CFLAGS,
        link: &[
            "/subsystem:console",
            "/entry:entry",
            "/nodefaultlib",
            "/opt:noref",
            "/map:app.map",
            "/out:app.exe",
            "app.obj",
            "shape.obj",
        ],
    },
    Img {
        id: "LIB",
        file: "lib.dll",
        map: "lib.map",
        tus: &["lib.c"],
        cflags: DLL_CFLAGS,
        link: &[
            "/dll",
            "/noentry",
            "/nodefaultlib",
            "/opt:noref",
            "/map:lib.map",
            "/out:lib.dll",
            "lib.obj",
        ],
    },
];

const MUTATE: (&str, &str, &str) = ("shape.cpp", "return w * h;", "return w * h + 1;");

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

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

fn unit(source: &Path, cflags: &[&str]) -> Result<Unit> {
    let file = source
        .file_name()
        .and_then(|f| f.to_str())
        .context("a fixture with no file name")?
        .to_string();
    let stem = source
        .file_stem()
        .and_then(|f| f.to_str())
        .context("a fixture with no stem")?
        .to_string();
    Ok(Unit {
        source: source.to_path_buf(),
        stem,
        file,
        cflags: cflags.iter().map(|s| (*s).to_string()).collect(),
        includes: Vec::new(),
        include_env: None,
        toolchain: "vc6".into(),
    })
}

fn link(
    sess: &Session<'_>,
    tc: &Toolchain,
    runner: &Runner,
    dir: &Path,
    args: &[&str],
) -> Result<()> {
    let mut argv = vec!["/nologo".to_string()];
    argv.extend(args.iter().map(|s| (*s).to_string()));
    let env = msvc6_toolchain::runner::tool_env(tc, &runner.winepath);
    let out = sess.run(&tc.tool("LINK.EXE")?, &argv, Some(dir), &env, None)?;
    if !out.ok() {
        bail!("LINK {} failed\n{}", argv.join(" "), out.text().trim());
    }
    Ok(())
}

fn trailing_fill(b: &[u8]) -> usize {
    b.iter()
        .rev()
        .take_while(|x| matches!(**x, 0xcc | 0x90))
        .count()
}

fn extents(image: &Path, map: &Path) -> Result<(String, usize)> {
    let img = Image::parse(std::fs::read(image)?)?;
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
        let at = (rva - lo) as usize;
        let span = bytes.get(at..(next - lo) as usize).unwrap_or_default();
        let size = span.len().saturating_sub(trailing_fill(span));
        csv.push_str(&format!(
            "{:#x},{name},function,{size}\n",
            base.saturating_add(*rva)
        ));
    }
    Ok((csv, starts.len()))
}

fn image_text(scratch: &Path, i: &Img) -> Result<String> {
    let reference = scratch.join("ref").join(i.file);
    let sha = msvc6_base::digest::sha256_hex(&std::fs::read(&reference)?);
    let mut t = format!(
        "\n[[image]]\nid = \"{}\"\nreference = {}\nsha256 = \"{sha}\"\ncompiler = \"vc6\"\nlinker = \"vc6\"\n\
         functions = [\"reccmp-csv:{}.extents.csv\", \"map:ref/{}\"]\nnames = [\"exports\", \"map:ref/{}\"]\n",
        i.id,
        quoted(&path_str(&reference)),
        i.id,
        i.map,
        i.map
    );
    let flags: Vec<String> = i.cflags.iter().map(|c| format!("\"{c}\"")).collect();
    for tu in i.tus {
        t.push_str(&format!(
            "\n[[image.tu]]\nsource = \"src/{tu}\"\ncflags = [{}]\n",
            flags.join(", ")
        ));
    }
    Ok(t)
}

fn reference(scratch: &Path, root: &str) -> Result<PathBuf> {
    let text = toolchain_text(scratch, root);
    let file = scratch.join("msvc6t.toml");
    srcio::own_write(&file, &text)?;
    let p = Project::load(&file)?;
    let cfg = p.toolchain_config();
    let tc = Toolchain::named(&cfg, "vc6")?;
    let runner = Runner::from_config(&cfg)?;
    println!("vc6: toolchain {} at {}", tc.id, tc.root.display());
    let refdir = scratch.join("ref");
    let sess = runner.session()?;
    for i in &IMAGES {
        for tu in i.tus {
            let src = scratch.join("src").join(tu);
            compile_in(&sess, None, &tc, &unit(&src, &i.cflags)?, &refdir, false)
                .with_context(|| format!("compiling the reference {tu}"))?;
        }
        link(&sess, &tc, &runner, &refdir, i.link)?;
        let (csv, n) = extents(&refdir.join(i.file), &refdir.join(i.map))?;
        srcio::own_write(scratch.join(format!("{}.extents.csv", i.id)), &csv)?;
        println!(
            "vc6: reference {} linked: {n} function(s) in its map",
            i.file
        );
    }
    let mut full = text;
    for i in &IMAGES {
        full.push_str(&image_text(scratch, i)?);
    }
    srcio::own_write(&file, &full)?;
    Ok(file)
}

fn tried(env: &Env, obj: &Path, stem: &str) -> Result<msvc6_diff::score::Score> {
    Try::new(env, AUTO, Members::Auto(vec![stem.to_string()])).score_file(obj)
}

fn library_loop(file: &Path, scratch: &Path) -> Result<Vec<(String, String)>> {
    let p = Project::load(file)?;
    let c = Compiler::new(&p)?;
    let sess = c.runner.session()?;
    let mut verdicts = Vec::new();
    for (n, i) in IMAGES.iter().enumerate() {
        let specs = msvc6_project::spec::resolve(&p, n)?;
        let env = Env::load(&p, n)?;
        let dir = scratch.join("objs").join(i.id);
        for tu in i.tus {
            let src = scratch.join("src").join(tu);
            let spec = msvc6_diff::compile::spec_for(&specs, i.id, &src)
                .with_context(|| format!("{tu} names no TU"))?;
            let u = Unit::of_spec(spec, &src, "vc6")?;
            let out = c.compile(&sess, &u, &dir, false)?;
            let s = tried(&env, &out.obj, &u.stem)?;
            println!(
                "vc6: {} {tu}: {} member(s){}",
                i.id,
                s.members.len(),
                s.verdict_line()
            );
            if !s.identical() || s.void() > 0 {
                for l in s.lines(true) {
                    println!("{l}");
                }
                bail!("{} {tu} is not IDENTICAL on its own source", i.id);
            }
            let predicted = Try::new(&env, s.rva, Members::Predict).score_file(&out.obj)?;
            let tags: Vec<&str> = predicted.members.iter().map(|m| m.seat.tag()).collect();
            println!(
                "vc6: {} {tu}: predicted from {:#x} by the placement laws, seats {}:{}",
                i.id,
                s.rva,
                tags.join(","),
                predicted.verdict_line()
            );
            if !predicted.identical() || predicted.void() > 0 {
                for l in predicted.lines(true) {
                    println!("{l}");
                }
                bail!("{} {tu}: the placement laws do not seat its members", i.id);
            }
            verdicts.push((tu.to_string(), s.verdict_line()));
        }
        let list = objs::build_list(std::slice::from_ref(&dir), false)?;
        let map = seat::seat_map(&env, &list, par::default_jobs());
        if !map.refused.is_empty() {
            bail!(
                "{} segrank refused its own objects: {:?}",
                i.id,
                map.refused
            );
        }
        let seats = map.seats;
        let rvas: Vec<u32> = seats.keys().copied().collect();
        let r = segrank::rank(&env, &list, &seats, &rvas, par::default_jobs());
        let total = segrank::total_line(&r.rows, r.asked);
        let raw = segrank::raw_line(&r.rows);
        println!("vc6: {} segrank {total}", i.id);
        println!("vc6: {} segrank {raw}", i.id);
        let edit: usize = r.rows.iter().map(|x| x.edit).sum();
        let rawb: usize = r.rows.iter().map(|x| x.raw).sum();
        if edit != 0 || rawb != 0 || !r.errors.is_empty() || r.rows.is_empty() {
            print!("{}", segrank::tsv(&r.rows));
            bail!(
                "{} segrank over its own objects: {edit} edit, {rawb} raw, {} error(s), {} row(s)",
                i.id,
                r.errors.len(),
                r.rows.len()
            );
        }
    }
    let (tu, from, to) = MUTATE;
    let text = srcio::own_read(scratch.join("src").join(tu))?;
    if !text.contains(from) {
        bail!("the mutation's text is not in {tu}");
    }
    let mdir = scratch.join("mutated");
    std::fs::create_dir_all(&mdir)?;
    let msrc = mdir.join(tu);
    srcio::own_write(&msrc, text.replacen(from, to, 1))?;
    let specs = msvc6_project::spec::resolve(&p, 0)?;
    let env = Env::load(&p, 0)?;
    let spec = msvc6_diff::compile::spec_for(&specs, "APP", &msrc)
        .context("the mutated copy names no TU")?;
    let u = Unit::of_spec(spec, &msrc, "vc6")?;
    let out = c.compile(&sess, &u, &scratch.join("objs").join("mutated"), false)?;
    let s = tried(&env, &out.obj, &u.stem)?;
    println!("vc6: APP mutated {tu}:{}", s.verdict_line());
    let wrong: Vec<&str> = s
        .members
        .iter()
        .filter(|m| !m.bad.is_empty())
        .map(|m| m.name.as_str())
        .collect();
    println!("vc6: APP mutated {tu}: differing in {wrong:?}");
    if s.identical() || wrong.len() != 1 || !wrong.iter().all(|n| n.contains("Shape::area")) {
        bail!(
            "a mutated {tu} reads {} with differing members {wrong:?}",
            s.verdict()
        );
    }
    verdicts.push((format!("mutated/{tu}"), s.verdict_line()));
    Ok(verdicts)
}

fn verb_binary() -> Option<PathBuf> {
    if let Some(b) = env("MSVC6T_BIN") {
        return Some(PathBuf::from(b));
    }
    let exe = std::env::current_exe().ok()?;
    let dir = exe.parent()?.parent()?;
    let b = dir.join("msvc6t");
    b.is_file().then_some(b)
}

fn verb(bin: &Path, file: &Path, scratch: &Path, args: &[String]) -> Result<(i32, String)> {
    let out = Command::new(bin)
        .arg("--config")
        .arg(file)
        .args(args)
        .env("TMPDIR", scratch.join("tmp"))
        .output()
        .with_context(|| format!("running {}", bin.display()))?;
    let stdout = srcio::tool_text(&out.stdout);
    let code = out.status.code().unwrap_or(-1);
    if code == 1 || code == 2 {
        bail!(
            "msvc6t {} exited {code}\n{stdout}{}",
            args.join(" "),
            srcio::tool_text(&out.stderr)
        );
    }
    Ok((code, stdout))
}

fn verdict_of(stdout: &str) -> Option<String> {
    stdout
        .lines()
        .find(|l| l.starts_with("  VERDICT: "))
        .map(str::to_string)
}

fn verb_loop(bin: &Path, file: &Path, scratch: &Path, want: &[(String, String)]) -> Result<()> {
    std::fs::create_dir_all(scratch.join("tmp"))?;
    let s = |x: &str| x.to_string();
    for (n, i) in IMAGES.iter().enumerate() {
        let vobjs = scratch.join("vobjs").join(i.id);
        for tu in i.tus {
            let src = path_str(&scratch.join("src").join(tu));
            let (code, out) = verb(
                bin,
                file,
                scratch,
                &[
                    s("try"),
                    s(i.id),
                    s("auto"),
                    src.clone(),
                    s("--auto-members"),
                ],
            )?;
            let line = verdict_of(&out).with_context(|| format!("no VERDICT line:\n{out}"))?;
            println!("vc6: msvc6t try {} {tu}: exit {code}:{line}", i.id);
            let expected = want.iter().find(|(t, _)| t == tu).map(|(_, v)| v.as_str());
            if code != 0 || expected != Some(line.as_str()) {
                bail!(
                    "msvc6t try {} {tu}: exit {code}, {line:?}, the oracle read {expected:?}",
                    i.id
                );
            }
            verb(
                bin,
                file,
                scratch,
                &[s("compile"), s(i.id), src, s("--out"), path_str(&vobjs)],
            )?;
        }
        let (code, out) = verb(
            bin,
            file,
            scratch,
            &[
                s("segrank"),
                s(i.id),
                s("--objs"),
                path_str(&vobjs),
                s("--no-libs"),
            ],
        )?;
        let tail: Vec<&str> = out.lines().rev().take(2).collect();
        println!(
            "vc6: msvc6t segrank {} (image {n}): exit {code}: {}",
            i.id,
            tail.iter().rev().copied().collect::<Vec<_>>().join(" | ")
        );
        if code != 0 || !out.contains(" 0 edit bytes of ") || !out.contains("raw: 0 raw byte(s)") {
            bail!("msvc6t segrank {} is not 0 edit / 0 raw:\n{out}", i.id);
        }
    }
    let (tu, _, _) = MUTATE;
    let msrc = path_str(&scratch.join("mutated").join(tu));
    let (code, out) = verb(
        bin,
        file,
        scratch,
        &[s("try"), s("APP"), s("auto"), msrc, s("--auto-members")],
    )?;
    let line = verdict_of(&out).with_context(|| format!("no VERDICT line:\n{out}"))?;
    println!("vc6: msvc6t try APP mutated {tu}: exit {code}:{line}");
    let expected = want
        .iter()
        .find(|(t, _)| t == &format!("mutated/{tu}"))
        .map(|(_, v)| v.as_str());
    if code != 3 || !line.contains("VERDICT: DIFFERS") || expected != Some(line.as_str()) {
        bail!("msvc6t try of the mutated {tu}: exit {code}, {line:?}");
    }
    Ok(())
}

fn require() -> bool {
    env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1")
}

fn run() -> Result<bool> {
    let Some(root) = env("MSVC6T_VC6") else {
        println!("{SKIP}");
        if require() {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no toolchain is configured");
        }
        return Ok(false);
    };
    let scratch = std::env::temp_dir().join(format!("msvc6t-loop-test-{}", std::process::id()));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch)?;
    }
    let src = scratch.join("src");
    std::fs::create_dir_all(&src)?;
    std::fs::create_dir_all(scratch.join("ref"))?;
    for i in &IMAGES {
        for tu in i.tus {
            let from = repo().join("fixtures/loop").join(tu);
            std::fs::copy(&from, src.join(tu))
                .with_context(|| format!("copying {}", from.display()))?;
        }
    }
    let result = reference(&scratch, &root).and_then(|file| {
        let want = library_loop(&file, &scratch)?;
        match verb_binary() {
            Some(bin) => verb_loop(&bin, &file, &scratch, &want),
            None => {
                println!("SKIP vc6 verbs: no msvc6t binary beside this test (build the workspace, or set MSVC6T_BIN)");
                if require() {
                    bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no msvc6t binary was found");
                }
                Ok(())
            }
        }
    });
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
