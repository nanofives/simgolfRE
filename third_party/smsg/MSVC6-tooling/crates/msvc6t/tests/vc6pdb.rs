use anyhow::{Context as _, Result, bail};
use msvc6_base::digest::{md5_hex, sha256_hex};
use msvc6_base::srcio;
use msvc6_image::pe::Image;
use msvc6_project::config::Project;
use msvc6_project::mapfile::Map;
use msvc6_toolchain::confine::Root;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use msvc6_toolchain::stage::Stager;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");
const SKIP: &str = "SKIP vc6pdb: no toolchain configured (set MSVC6T_VC6)";
const GUARD_REFUSAL: &str = "RUST GUARD: this msvc6t was built from a Rust tree never linted clean";
const SOURCES: [&str; 4] = ["fx.h", "a.c", "b.c", "c.cpp"];
const TUS: [&str; 3] = ["a.c", "b.c", "c.cpp"];
const CFLAGS: [&str; 8] = [
    "/nologo",
    "/c",
    "/W3",
    "/Zi",
    "/O2",
    "/Gy",
    "/FoDebug\\",
    "/FdDebug\\vc60.pdb",
];
const TC_AT: &str = "C:\\msvc6tfx\\vc6";
const WORK_AT: &str = "C:\\fx";
const OURS_ORDER: [&str; 3] = ["Debug\\a.obj", "Debug\\b.obj", "Debug\\c.obj"];
const REF_ORDER: [&str; 3] = ["Debug\\c.obj", "Debug\\b.obj", "Debug\\a.obj"];

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn quoted(s: &str) -> String {
    format!("'{s}'")
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
    text
}

struct Build<'a> {
    sess: &'a Session<'a>,
    tc_bin: PathBuf,
    work: PathBuf,
    env: Vec<(String, String)>,
}

impl Build<'_> {
    fn tool(&self, exe: &str, args: &[String]) -> Result<String> {
        let out = self.sess.run(
            &self.tc_bin.join(exe),
            args,
            Some(&self.work),
            &self.env,
            None,
        )?;
        if !out.ok() {
            bail!("{exe} {} failed\n{}", args.join(" "), out.text().trim());
        }
        Ok(out.text())
    }

    fn link(&self, dir: &str, host: &Path, order: &[&str]) -> Result<()> {
        let mut args: Vec<String> = [
            "/nologo",
            "/dll",
            "/entry:start",
            "/nodefaultlib",
            "/debug",
            "/pdbtype:con",
        ]
        .into_iter()
        .map(str::to_string)
        .collect();
        args.push(format!("/pdb:{dir}\\fx.pdb"));
        args.push(format!("/map:{dir}\\fx.map"));
        args.push(format!("/out:{dir}\\fx.dll"));
        args.extend(order.iter().map(|o| (*o).to_string()));
        args.push("kernel32.lib".to_string());
        std::fs::create_dir_all(host)?;
        self.tool("LINK.EXE", &args)?;
        Ok(())
    }
}

fn build(scratch: &Path, root: &str) -> Result<()> {
    let file = scratch.join("msvc6t.toml");
    srcio::own_write(&file, toolchain_text(scratch, root))?;
    let p = Project::load(&file)?;
    let cfg = p.toolchain_config();
    let tc = Toolchain::named(&cfg, "vc6")?;
    let runner = Runner::from_config(&cfg)?;
    let sess = runner.session()?;
    let drive_c = sess
        .drive_c()
        .context("the fixture compiles in a leased wine prefix; this runner has none")?;
    println!(
        "vc6pdb: toolchain {} at {}, prefix {}",
        tc.id,
        tc.root.display(),
        drive_c.display()
    );
    let manifest = scratch.join("staged.tsv");
    let r = Root::new(&drive_c)?;
    let mut st = Stager::new(r.clone(), &cfg, manifest.clone());
    st.mount(TC_AT, root)?;
    for s in SOURCES {
        st.put_file(
            &PathBuf::from("fx").join("src").join(s),
            &repo().join("fixtures/pdb").join(s),
        )?;
    }
    let work = drive_c.join("fx");
    let vc = |rel: &str| format!("{TC_AT}\\{rel}");
    let b = Build {
        sess: &sess,
        tc_bin: drive_c.join("msvc6tfx/vc6/VC98/Bin"),
        work: work.clone(),
        env: vec![
            ("INCLUDE".into(), vc("VC98\\Include")),
            ("LIB".into(), vc("VC98\\Lib")),
            (
                "WINEPATH".into(),
                format!("{};{}", vc("VC98\\Bin"), vc("Common\\MSDev98\\Bin")),
            ),
        ],
    };
    let debug = work.join("Debug");
    let shipped = drive_c.join("fxref");
    let result = (|| -> Result<()> {
        std::fs::create_dir_all(&debug)?;
        for tu in TUS {
            let mut args: Vec<String> = CFLAGS.iter().map(|s| (*s).to_string()).collect();
            args.push(format!("src\\{tu}"));
            b.tool("CL.EXE", &args)
                .with_context(|| format!("compiling {tu}"))?;
        }
        b.link("Debug", &debug, &OURS_ORDER)?;
        b.link("..\\fxref", &shipped, &REF_ORDER)?;
        for (from, to) in [
            (debug.join("fx.dll"), "ours/fx.dll"),
            (debug.join("fx.pdb"), "ours/fx.pdb"),
            (debug.join("fx.map"), "ours/fx.map"),
            (shipped.join("fx.dll"), "ref/fx.dll"),
            (shipped.join("fx.map"), "ref/fx.map"),
            (debug.join("a.obj"), "ours/a.obj"),
            (debug.join("b.obj"), "ours/b.obj"),
            (debug.join("c.obj"), "ours/c.obj"),
        ] {
            let dst = scratch.join(to);
            std::fs::create_dir_all(dst.parent().context("a parent")?)?;
            std::fs::copy(&from, &dst)
                .with_context(|| format!("copying {} out of the prefix", from.display()))?;
        }
        Ok(())
    })();
    let cleaned = msvc6_toolchain::stage::clean(&r, &manifest);
    for d in [&debug, &shipped] {
        std::fs::remove_dir_all(d).ok();
    }
    for f in std::fs::read_dir(&work).into_iter().flatten().flatten() {
        let p = f.path();
        if p.is_file() {
            std::fs::remove_file(p).ok();
        }
    }
    result?;
    cleaned?;
    Ok(())
}

fn trailing_fill(b: &[u8]) -> usize {
    b.iter()
        .rev()
        .take_while(|x| matches!(**x, 0xcc | 0x90))
        .count()
}

fn report(scratch: &Path) -> Result<(String, usize)> {
    let img = Image::parse(srcio::read_bounded(scratch.join("ref/fx.dll"), 1 << 26)?)?;
    let base = img.opt.image_base.get();
    let text = img.section(".text").context("the reference has no .text")?;
    let lo = text.rva.get();
    let bytes = img.section_bytes(text);
    let hi = lo.saturating_add(u32::try_from(bytes.len())?);
    let theirs = Map::read(&scratch.join("ref/fx.map"))?;
    let ours = Map::read(&scratch.join("ours/fx.map"))?;
    let mut bounds: Vec<u32> = theirs
        .symbols
        .iter()
        .map(|s| s.rva)
        .filter(|r| *r >= lo && *r < hi)
        .collect();
    bounds.sort_unstable();
    bounds.dedup();
    let mut starts: Vec<(u32, String, String)> = theirs
        .symbols
        .iter()
        .filter(|s| s.function && s.lib.is_none() && s.rva >= lo && s.rva < hi)
        .map(|s| (s.rva, s.name.clone(), s.object.clone()))
        .collect();
    starts.sort();
    starts.dedup_by_key(|s| s.0);
    let mut rows = Vec::new();
    for (rva, name, obj) in &starts {
        let next = bounds
            .get(bounds.partition_point(|b| b <= rva))
            .copied()
            .unwrap_or(hi);
        let at = usize::try_from(rva - lo)?;
        let span = bytes
            .get(at..usize::try_from(next - lo)?)
            .unwrap_or_default();
        let size = span.len().saturating_sub(trailing_fill(span));
        let Some(o) = ours
            .symbols
            .iter()
            .find(|s| s.name == *name && s.object == *obj)
        else {
            bail!("{name} of {obj} is not in our map");
        };
        let tu = TUS
            .iter()
            .find(|t| {
                obj.strip_suffix(".obj")
                    .is_some_and(|s| t.starts_with(&format!("{s}.")))
            })
            .map(|t| format!("fx/src/{t}"))
            .unwrap_or_default();
        rows.push(serde_json::json!({
            "address": format!("{:#x}", base + rva),
            "recomp": format!("{:#x}", base + o.rva),
            "name": msvc6_pdb::audit::display_name(name),
            "msvc6t": {"symbol": name, "obj": obj, "size": size, "tu": tu, "seat": "seated", "edit": 0},
        }));
    }
    let n = rows.len();
    Ok((
        serde_json::json!({"file": "fx.dll", "format": 1, "data": rows}).to_string(),
        n,
    ))
}

fn image_text(scratch: &Path) -> Result<String> {
    let r = scratch.join("ref/fx.dll");
    let bytes = std::fs::read(&r)?;
    let product = scratch.join("product");
    std::fs::create_dir_all(product.join("fx/src"))?;
    for s in SOURCES {
        std::fs::copy(
            repo().join("fixtures/pdb").join(s),
            product.join("fx/src").join(s),
        )?;
    }
    Ok(format!(
        "\n[[image]]\nid = \"FX\"\nreference = {}\nsha256 = \"{}\"\ncompiler = \"vc6\"\nnames = [\"exports\"]\n\n\
         [image.pdb]\nshipped = [{{ md5 = \"{}\", what = \"the fixture reference\" }}]\n\n\
         [image.pdb.paths]\nproduct = {}\nlanes = [{{ at = {}, product = \"fx\" }}]\n",
        quoted(&r.display().to_string()),
        sha256_hex(&bytes),
        md5_hex(&bytes),
        quoted(&product.display().to_string()),
        quoted(WORK_AT)
    ))
}

struct Ran {
    code: i32,
    stdout: String,
    stderr: String,
}

fn msvc6t(scratch: &Path, args: &[&str]) -> Result<Ran> {
    let o = Command::new(BIN)
        .current_dir(scratch)
        .arg("--config")
        .arg(scratch.join("msvc6t.toml"))
        .args(args)
        .output()?;
    let stdout = srcio::tool_text(&o.stdout);
    let stderr = srcio::tool_text(&o.stderr);
    println!(
        "vc6pdb: msvc6t {}: exit {:?}",
        args.join(" "),
        o.status.code()
    );
    if !stderr.trim().is_empty() {
        println!("{}", stderr.trim());
    }
    Ok(Ran {
        code: o.status.code().unwrap_or(-1),
        stdout,
        stderr,
    })
}

fn expect(r: &Ran, code: i32, line: &str) -> Result<()> {
    if r.code != code || !r.stdout.lines().any(|l| l.contains(line)) {
        println!("{}", r.stdout);
        bail!(
            "want exit {code} and a line holding `{line}`; got exit {}",
            r.code
        );
    }
    Ok(())
}

fn poke_a_procedure(pdb: &Path, out: &Path) -> Result<()> {
    let mut db = msvc6_pdb::msf::Msf2::parse(srcio::read_bounded(pdb, 1 << 26)?)?;
    let dbi = msvc6_pdb::reloc::DbiView::parse(&db.stream(3)?)?;
    for m in &dbi.mods {
        if m.stream == 0xffff || m.sym_len <= 4 {
            continue;
        }
        let mut s = db.stream(usize::from(m.stream))?;
        let region = s.get(4..m.sym_len).context("a module region")?.to_vec();
        let Some((_, _, payload, _)) = msvc6_pdb::reloc::records(&region)
            .into_iter()
            .find(|(_, k, _, len)| *k == 0x100b && *len >= 36)
        else {
            continue;
        };
        let at = 4 + payload + 28;
        let off = msvc6_pdb::reloc::u32_at(&s, at).context("an offset")?;
        msvc6_pdb::reloc::put_u32(&mut s, at, off + 2)?;
        db.put_stream(usize::from(m.stream), &s)?;
        srcio::own_write(out, db.bytes())?;
        return Ok(());
    }
    bail!("no procedure record to move")
}

fn run(scratch: &Path, root: &str) -> Result<()> {
    build(scratch, root)?;
    let (json, rows) = report(scratch)?;
    srcio::own_write(scratch.join("report.json"), json)?;
    println!("vc6pdb: the report pairs {rows} function(s) by name and object across the two maps");
    let mut cfg = srcio::own_read(scratch.join("msvc6t.toml"))?;
    cfg.push_str(&image_text(scratch)?);
    srcio::own_write(scratch.join("msvc6t.toml"), cfg)?;
    std::fs::create_dir_all(scratch.join("reloc"))?;
    let r = msvc6t(
        scratch,
        &[
            "pdb",
            "relocate",
            "FX",
            "ours/fx.pdb",
            "--ours",
            "ours/fx.dll",
            "--report",
            "report.json",
            "--out",
            "reloc/fx.pdb",
            "--pairs",
            "reloc/fx.pairs.tsv",
            "--sample",
            "4",
        ],
    )?;
    expect(&r, 0, "identity: signature")?;
    for l in r.stdout.lines().filter(|l| {
        l.contains("pairing(s)")
            || l.contains("patched")
            || l.contains("C11 line")
            || l.contains("SAMPLE VERDICT")
    }) {
        println!("  {l}");
    }
    if !r.stdout.lines().any(|l| {
        l.contains("SAMPLE VERDICT") && !l.contains(": 0 IDENTICAL") && l.contains(" 0 DIFFERS")
    }) {
        bail!("the derived data pairings were not proved against our objects");
    }
    let audit = [
        "pdb",
        "audit",
        "FX",
        "reloc/fx.pdb",
        "--report",
        "report.json",
        "--pairs",
        "reloc/fx.pairs.tsv",
        "--ours",
        "ours/fx.dll",
        "--original",
        "ours/fx.pdb",
    ];
    let a = msvc6t(scratch, &audit)?;
    expect(&a, 0, "RESULT: 0 failures")?;
    let fields: usize = a
        .stdout
        .lines()
        .filter_map(|l| {
            let t: Vec<&str> = l.split_whitespace().collect();
            let n = t.len();
            (n > 5 && l.starts_with("  ") && !l.starts_with("    "))
                .then(|| t.get(n - 5).and_then(|x| x.parse::<usize>().ok()))
                .flatten()
        })
        .sum();
    println!(
        "vc6pdb: the relocated PDB passes the audit: {fields} live field(s) the shipped thing they name, 0 failures"
    );
    let unrelocated = msvc6t(
        scratch,
        &[
            "pdb",
            "audit",
            "FX",
            "ours/fx.pdb",
            "--report",
            "report.json",
            "--ours",
            "ours/fx.dll",
        ],
    )?;
    expect(&unrelocated, 3, "RESULT:")?;
    poke_a_procedure(
        &scratch.join("reloc/fx.pdb"),
        &scratch.join("reloc/poked.pdb"),
    )?;
    let poked: Vec<&str> = audit
        .iter()
        .map(|a| {
            if *a == "reloc/fx.pdb" {
                "reloc/poked.pdb"
            } else {
                a
            }
        })
        .collect();
    let p = msvc6t(scratch, &poked)?;
    expect(&p, 3, "FAIL module S_GPROC32")?;
    println!("vc6pdb: our own link's PDB and a procedure moved by two bytes FAIL the audit");
    let d = msvc6t(
        scratch,
        &[
            "pdb",
            "dbghelp",
            "FX",
            "reloc/fx.pdb",
            "--report",
            "report.json",
        ],
    )?;
    if d.code == 1 && d.stderr.contains(GUARD_REFUSAL) {
        println!(
            "SKIP vc6pdb dbghelp: msvc6t refused to run the probe's compiler (`msvc6t lint --gate` marks the tree)"
        );
    } else {
        expect(&d, 0, "SymType 3(SymPdb)")?;
        expect(&d, 0, &format!("{rows} row(s) queried"))?;
        expect(&d, 0, &format!("named {rows} |"))?;
        println!(
            "vc6pdb: wine's dbghelp loads the relocated PDB through the reference's own record"
        );
    }
    std::fs::create_dir_all(scratch.join("reloc/7.0"))?;
    let t = msvc6t(
        scratch,
        &["pdb", "to70", "reloc/fx.pdb", "reloc/7.0/fx.pdb"],
    )?;
    expect(&t, 0, "EQUIVALENT")?;
    let at = msvc6t(
        scratch,
        &[
            "pdb",
            "attach",
            "FX",
            "ref/fx.dll",
            "--pdb",
            "reloc/7.0/fx.pdb",
            "--out",
            "reloc/fx-rsds.dll",
        ],
    )?;
    expect(&at, 0, "wrote reloc/fx-rsds.dll")?;
    let s = msvc6t(
        scratch,
        &[
            "pdb",
            "symbolize",
            "FX",
            "reloc/7.0/fx.pdb",
            "--copy",
            "reloc/fx-rsds.dll",
            "--report",
            "report.json",
        ],
    )?;
    if s.stdout.starts_with("SKIP") {
        println!("{}", s.stdout.trim());
    } else {
        expect(&s, 0, &format!("{rows} row(s) queried: named {rows} |"))?;
        println!("{}", s.stdout.trim());
    }
    Ok(())
}

fn main() -> ExitCode {
    let require = env("MSVC6T_REQUIRE_TOOLCHAIN").is_some_and(|v| v == "1");
    let Some(root) = env("MSVC6T_VC6") else {
        println!("{SKIP}");
        return if require {
            ExitCode::FAILURE
        } else {
            ExitCode::SUCCESS
        };
    };
    let scratch = std::env::temp_dir().join(format!("msvc6t-vc6pdb-{}", std::process::id()));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch).ok();
    }
    if let Err(e) = std::fs::create_dir_all(&scratch) {
        println!("vc6pdb: {e}");
        return ExitCode::FAILURE;
    }
    let r = run(&scratch, &root);
    match r {
        Ok(()) => {
            std::fs::remove_dir_all(&scratch).ok();
            println!("vc6pdb: ok");
            ExitCode::SUCCESS
        }
        Err(e) => {
            println!("vc6pdb: FAIL: {e:#} (scratch kept: {})", scratch.display());
            ExitCode::FAILURE
        }
    }
}
