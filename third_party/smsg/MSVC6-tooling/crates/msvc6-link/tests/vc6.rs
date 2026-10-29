use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_image::pe::Image;
use msvc6_link::iat::{self, Blocks, Control};
use msvc6_link::icf::{self, Entry, Inputs, Model};
use msvc6_link::include;
use msvc6_link::objkey::Keys;
use msvc6_link::owners::RefOwners;
use msvc6_link::predict;
use msvc6_link::qsort::Crt;
use msvc6_link::verbose::{self, Kind};
use msvc6_project::config::Project;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use std::collections::{BTreeMap, HashSet};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";

const KERNEL32: [&str; 10] = [
    "GetTickCount",
    "GetCurrentProcessId",
    "GetCurrentThreadId",
    "GetLastError",
    "GetVersion",
    "GetCommandLineA",
    "GetProcessHeap",
    "GetCurrentProcess",
    "GetCurrentThread",
    "GetACP",
];

const USER32: [&str; 5] = [
    "GetDesktopWindow",
    "GetActiveWindow",
    "GetFocus",
    "GetCapture",
    "GetMessagePos",
];

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn quoted(s: &str) -> String {
    format!("'{s}'")
}

fn config_text(scratch: &Path, root: &str, msvcrt: &str) -> String {
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
        "prefixes = {}\ntokens = {}\nnative_msvcrt = {}\n",
        quoted(&scratch.join("prefixes").display().to_string()),
        quoted(&scratch.join("tokens").display().to_string()),
        quoted(msvcrt)
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

struct Ctx<'a> {
    sess: Session<'a>,
    tc: Toolchain,
}

impl Ctx<'_> {
    fn tool(
        &self,
        exe: &str,
        args: &[String],
        dir: &Path,
        builtin: bool,
    ) -> Result<(String, String)> {
        let mut extra = vec![("WINEDEBUG".to_string(), "-all,+loaddll".to_string())];
        if builtin {
            extra.push(("WINEDLLOVERRIDES".to_string(), "msvcrt=b".to_string()));
        }
        let out = self.sess.run_tool(&self.tc, exe, args, Some(dir), &extra)?;
        let stdout = srcio::tool_text(&out.stdout);
        let stderr = srcio::tool_text(&out.stderr);
        if !out.ok() {
            bail!(
                "{exe} {} failed:\n{stdout}{}",
                args.join(" "),
                msvc6_toolchain::crtguard::non_trace(&stderr)
            );
        }
        Ok((stdout, stderr))
    }

    fn cl(&self, dir: &Path, src: &str) -> Result<()> {
        let a: Vec<String> = ["/nologo", "/c", "/O2", src]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        self.tool("CL.EXE", &a, dir, false).map(|_| ())
    }

    fn link(&self, dir: &Path, args: &[String], builtin: bool) -> Result<String> {
        let mut a = vec!["/nologo".to_string(), "/VERBOSE".to_string()];
        a.extend(args.iter().cloned());
        let (stdout, stderr) = self.tool("LINK.EXE", &a, dir, builtin)?;
        let (line, native) = msvc6_toolchain::crtguard::line(&stderr, &[]);
        println!(
            "vc6: LINK {}: {line}",
            if builtin { "builtin" } else { "native" }
        );
        if native == builtin {
            bail!(
                "LINK was to run on the {} msvcrt: {line}",
                if builtin { "builtin" } else { "native" }
            );
        }
        Ok(stdout)
    }

    fn link_on_c(&self, dir: &Path, win: &str, args: &[String]) -> Result<String> {
        let link = msvc6_toolchain::layout::to_win(&self.tc.tool("LINK.EXE")?);
        let line = format!("cd /d {win} && {link} /nologo /VERBOSE {}", args.join(" "));
        let mut env = msvc6_toolchain::runner::tool_env(&self.tc, &self.sess.runner.winepath);
        env.push(("WINEDEBUG".to_string(), "-all,+loaddll".to_string()));
        let out = self.sess.run(
            Path::new("cmd"),
            &["/c".to_string(), line],
            Some(dir),
            &env,
            None,
        )?;
        let stdout = srcio::tool_text(&out.stdout);
        let stderr = srcio::tool_text(&out.stderr);
        if out.timed_out || out.status != Some(0) {
            bail!(
                "LINK.EXE {} on {win} failed:\n{stdout}{}",
                args.join(" "),
                msvc6_toolchain::crtguard::non_trace(&stderr)
            );
        }
        let (guard, native) = msvc6_toolchain::crtguard::line(&stderr, &["link.exe".to_string()]);
        println!("vc6: LINK on {win}: {guard}");
        if !native {
            bail!("LINK was to run on the native msvcrt: {guard}");
        }
        Ok(stdout)
    }
}

struct Dir(PathBuf);

impl Inputs for Dir {
    fn object(&mut self, name: &str) -> Option<Vec<u8>> {
        std::fs::read(self.0.join(name)).ok()
    }

    fn member(&mut self, _lib: &str, _member: &str) -> Option<Vec<u8>> {
        None
    }

    fn member_defining(&mut self, _lib: &str, _names: &[String]) -> Option<Vec<u8>> {
        None
    }
}

fn replay(dir: &Path, log: &str, model: Model) -> Result<predict::Report> {
    let mut order = vec![Entry::Object("icf.obj".into())];
    order.extend(icf::loaded_entries(&verbose::loads(log)));
    let parsed = icf::parse(&order, &mut Dir(dir.to_path_buf()));
    let groups = verbose::groups(log);
    let dead = verbose::discarded(log);
    let keys = Keys::default();
    let none = |_: &str| None;
    let owners = RefOwners::new(&BTreeMap::new(), &BTreeMap::new(), &none, &keys);
    predict::run(&predict::Input {
        label: "icf.exe",
        parsed: &parsed,
        base: None,
        dead: &dead,
        groups: &groups,
        model,
        want: &[],
        moves: &[],
        defs: &[],
        dump: &[],
        owners: &owners,
        keys: &keys,
        search_insert: None,
        focus: None,
        joint: None,
        verbose: true,
        jobs: 1,
    })
}

fn survivor(log: &str) -> Result<String> {
    let g = verbose::groups(log);
    let [one] = g.as_slice() else {
        bail!("the link narrates {} fold group(s), not one", g.len());
    };
    Ok(one.survivor.name.clone())
}

fn icf_contrast(c: &Ctx<'_>, scratch: &Path) -> Result<()> {
    let dir = scratch.join("icf");
    std::fs::create_dir_all(&dir)?;
    std::fs::copy(repo().join("fixtures/link/icf.cpp"), dir.join("icf.cpp"))?;
    c.cl(&dir, "icf.cpp")?;
    let args: Vec<String> = [
        "/entry:main",
        "/subsystem:console",
        "/nodefaultlib",
        "/map:icf.map",
        "/out:icf.exe",
        "icf.obj",
    ]
    .iter()
    .map(|s| (*s).to_string())
    .collect();
    let native = c.link(&dir, &args, false)?;
    let builtin = c.link(&dir, &args, true)?;
    let (ns, bs) = (survivor(&native)?, survivor(&builtin)?);
    println!("vc6: ICF survivor under the native msvcrt {ns}, under the builtin {bs}");
    if ns == bs {
        bail!("the two CRTs kept the same survivor {ns}: the fixture does not contrast them");
    }
    let vc6 = Model::default();
    let wine = Model {
        crt: Some(Crt::Wine),
        ..Model::default()
    };
    for (what, log, model, want) in [
        ("native log, VC6 qsort", &native, vc6, true),
        ("builtin log, VC6 qsort", &builtin, vc6, false),
        ("builtin log, wine qsort", &builtin, wine, true),
        ("native log, wine qsort", &native, wine, false),
    ] {
        let r = replay(&dir, log, model)?;
        let confirmed = r.summary.groups == 1 && r.summary.order_ok == 1;
        let line = r
            .lines
            .iter()
            .find(|l| l.starts_with("G1 "))
            .cloned()
            .unwrap_or_default();
        println!(
            "vc6: {what}: {} -- {line}",
            if confirmed { "CONFIRMED" } else { "REFUTED" }
        );
        if confirmed != want {
            for l in &r.lines {
                println!("{l}");
            }
            bail!(
                "{what}: the prediction is {}, wanted {}",
                if confirmed { "confirmed" } else { "refuted" },
                if want { "confirmed" } else { "refuted" }
            );
        }
    }
    Ok(())
}

fn blocks_of(k: &[&str], u: &[&str]) -> Blocks {
    let mut b: Blocks = BTreeMap::new();
    for (dll, names) in [("kernel32.dll", k), ("user32.dll", u)] {
        b.insert(
            dll.to_string(),
            names
                .iter()
                .enumerate()
                .map(|(j, n)| (u32::try_from(j * 4).unwrap_or(0), (*n).to_string()))
                .collect(),
        );
    }
    b
}

const KERNEL32_FIRST: [&str; 2] = ["kernel32.lib", "user32.lib"];

fn iat_link(
    c: &Ctx<'_>,
    dir: &Path,
    out: &str,
    libs: &[&str],
    includes: &[String],
) -> Result<(String, Image)> {
    let mut args: Vec<String> = vec![
        "/dll".into(),
        "/noentry".into(),
        "/nodefaultlib".into(),
        format!("/out:{out}"),
        "tiny.obj".into(),
    ];
    args.extend(libs.iter().map(|l| (*l).to_string()));
    args.extend(includes.iter().map(|s| format!("/INCLUDE:{s}")));
    let log = c.link(dir, &args, false)?;
    let img = Image::parse(std::fs::read(dir.join(out))?).with_context(|| out.to_string())?;
    Ok((log, img))
}

fn same(want: &Blocks, got: &Blocks) -> (usize, usize) {
    include::check(want, got)
        .iter()
        .fold((0, 0), |(s, n), r| (s + r.same, n + r.slots))
}

fn iat_round_trip(c: &Ctx<'_>, scratch: &Path) -> Result<()> {
    let dir = scratch.join("iat");
    std::fs::create_dir_all(&dir)?;
    std::fs::copy(repo().join("fixtures/link/tiny.c"), dir.join("tiny.c"))?;
    c.cl(&dir, "tiny.c")?;
    let lib =
        c.tc.lib
            .clone()
            .context("the toolchain has no lib directory")?;
    let mut libs = Vec::new();
    for name in ["KERNEL32.LIB", "USER32.LIB"] {
        let p = std::fs::read_dir(&lib)?
            .flatten()
            .map(|e| e.path())
            .find(|p| {
                p.file_name()
                    .is_some_and(|f| f.to_string_lossy().eq_ignore_ascii_case(name))
            })
            .with_context(|| format!("no {name} in {}", lib.display()))?;
        libs.extend(include::read_import_lib(&std::fs::read(&p)?)?);
    }
    let k: Vec<&str> = KERNEL32.iter().rev().copied().collect();
    let u: Vec<&str> = [3usize, 0, 4, 1, 2]
        .iter()
        .filter_map(|&i| USER32.get(i).copied())
        .collect();
    let want = blocks_of(&k, &u);
    let dlls = vec!["KERNEL32.dll".to_string(), "USER32.dll".to_string()];
    let plan = include::plan(&want, &dlls, &libs, Crt::Vc6)?;
    let switches: Vec<String> = plan.includes.iter().map(|i| i.symbol.clone()).collect();
    let (log, img) = iat_link(c, &dir, "iat.dll", &KERNEL32_FIRST, &switches)?;
    let got = iat::blocks(&img)?;
    let (s, n) = same(&want, &got);
    println!("vc6: the /INCLUDE: order linked: {s} of {n} IAT slots as wanted");
    if s != n || n != KERNEL32.len() + USER32.len() {
        bail!("the include-order link wrote {s} of {n} slots as wanted: {got:?}");
    }
    let pulls = iat::pulls(&verbose::loads(&log))?;
    let sorted = iat::sorted_population(&pulls, Crt::Vc6, Control::Model)?;
    let c1 = iat::compare(&pulls, &sorted, &got, &iat::block_order(&pulls, &sorted));
    println!(
        "vc6: predicted from its own /VERBOSE log: {} of {} slots",
        c1.same, c1.slots
    );
    if c1.same != c1.slots {
        bail!(
            "the model does not predict the link from its log: {:?}",
            c1.lines()
        );
    }
    let k2: Vec<&str> = KERNEL32
        .iter()
        .copied()
        .cycle()
        .skip(3)
        .take(KERNEL32.len())
        .collect();
    let u2: Vec<&str> = USER32.iter().rev().copied().collect();
    let want2 = blocks_of(&k2, &u2);
    let inv = iat::invert(&pulls, &sorted, &want2);
    let q = iat::apply_inversion(&pulls, &inv)?;
    let switches2: Vec<String> = q
        .iter()
        .filter(|p| p.kind == Kind::Import)
        .map(|p| p.symbol.clone())
        .collect();
    let seen: HashSet<&String> = switches2.iter().collect();
    if seen.len() != switches2.len() {
        bail!("the inverted pull order repeats a symbol: {switches2:?}");
    }
    let (_, img2) = iat_link(c, &dir, "inverted.dll", &KERNEL32_FIRST, &switches2)?;
    let (s2, n2) = same(&want2, &iat::blocks(&img2)?);
    println!("vc6: --invert's pull order linked: {s2} of {n2} IAT slots as wanted");
    if s2 != n2 {
        bail!("the inverted pull order linked {s2} of {n2} slots as wanted");
    }
    let dlls3 = vec!["USER32.dll".to_string(), "KERNEL32.dll".to_string()];
    let plan3 = include::plan(&want, &dlls3, &libs, Crt::Vc6)?;
    let switches3: Vec<String> = plan3.includes.iter().map(|i| i.symbol.clone()).collect();
    let (log3, img3) = iat_link(
        c,
        &dir,
        "user32first.dll",
        &["user32.lib", "kernel32.lib"],
        &switches3,
    )?;
    let got3 = iat::blocks(&img3)?;
    let (s3, n3) = same(&want, &got3);
    let pulls3 = iat::pulls(&verbose::loads(&log3))?;
    let sorted3 = iat::sorted_population(&pulls3, Crt::Vc6, Control::Model)?;
    let c3 = iat::compare(
        &pulls3,
        &sorted3,
        &got3,
        &iat::block_order(&pulls3, &sorted3),
    );
    println!(
        "vc6: user32.lib first: the /INCLUDE: order linked {s3} of {n3} IAT slots as wanted; predicted from its log {} of {}",
        c3.same, c3.slots
    );
    if s3 != n3 || c3.same != c3.slots {
        bail!(
            "user32.lib first: {s3} of {n3} slots as wanted, {} of {} predicted: {:?}",
            c3.same,
            c3.slots,
            c3.lines()
        );
    }
    Ok(())
}
fn growth_source(grown: bool) -> Result<String> {
    let mut src = String::from("extern \"C\" int probe(int);\n");
    for k in 0..12u32 {
        let terms = usize::try_from(k).unwrap_or(0) * 3 + 1 + usize::from(grown && k == 5) * 40;
        let pad = "+ x".repeat(terms);
        writeln!(
            src,
            "extern \"C\" int body{k}(int x) {{ return probe(x {pad}); }}"
        )?;
    }
    src.push_str("extern \"C\" int probe(int x) { return x; }\n");
    src.push_str("int main() {\n    int t = 0;\n");
    for k in 0..12u32 {
        writeln!(src, "    t += body{k}(t);")?;
    }
    src.push_str("    return t;\n}\n");
    Ok(src)
}

struct GrowthDir {
    host: PathBuf,
    win: Option<String>,
}

const GROWTH_DIR: &str = "msvc6t-growth";

fn growth_dir(c: &Ctx<'_>, scratch: &Path) -> Result<GrowthDir> {
    let d = match c.sess.drive_c() {
        Some(drive_c) => GrowthDir {
            host: drive_c.join(GROWTH_DIR),
            win: Some(format!("C:\\{GROWTH_DIR}")),
        },
        None => GrowthDir {
            host: scratch.join(GROWTH_DIR),
            win: None,
        },
    };
    std::fs::create_dir_all(&d.host)?;
    Ok(d)
}

fn growth_pass(c: &Ctx<'_>, d: &GrowthDir, grown: bool) -> Result<BTreeMap<&'static str, usize>> {
    let dir = d.host.as_path();
    srcio::own_write(dir.join("growth.cpp"), &growth_source(grown)?)?;
    let cl: Vec<String> = ["/nologo", "/c", "/Od", "/Gy", "growth.cpp"]
        .iter()
        .map(|s| (*s).to_string())
        .collect();
    c.tool("CL.EXE", &cl, dir, false)?;
    let args: Vec<String> = [
        "/INCREMENTAL:YES",
        "/OPT:NOREF",
        "/MAP:growth.map",
        "/OUT:growth.exe",
        "growth.obj",
    ]
    .iter()
    .map(|s| (*s).to_string())
    .collect();
    let log = match &d.win {
        Some(win) => c.link_on_c(dir, win, &args)?,
        None => c.link(dir, &args, false)?,
    };
    if let Some(l) = log.lines().find(|l| l.contains("LNK4073")) {
        bail!("LINK did not link incrementally, so its layout says nothing about the reserve: {l}");
    }
    if !dir.join("growth.ilk").is_file() {
        bail!(
            "LINK wrote no growth.ilk in {}: the link was not incremental",
            dir.display()
        );
    }
    let map = srcio::decode(&std::fs::read(dir.join("growth.map"))?);
    let parsed = msvc6_link::linkmap::parse(&map);
    let mut bodies: Vec<(u32, String)> = parsed
        .rows
        .iter()
        .filter(|r| r.name.starts_with("_body"))
        .map(|r| (r.va, r.name.clone()))
        .collect();
    bodies.sort_unstable();
    if bodies.len() < 8 {
        bail!(
            "the map lists {} `body' function(s), too few to read a reserve from:\n{map}",
            bodies.len()
        );
    }
    let img = msvc6_image::pe::Image::parse(std::fs::read(dir.join("growth.exe"))?)?;
    let sec = img
        .section(".text")
        .context("the linked image has no .text")?;
    let text = img.section_bytes(sec);
    let text_rva = sec.rva.get();
    let mut counts: BTreeMap<&'static str, usize> = BTreeMap::new();
    let mut rows: Vec<String> = Vec::new();
    for w in bodies.windows(2) {
        let [(rva, name), (next, _)] = w else {
            continue;
        };
        let off = usize::try_from(rva.saturating_sub(parsed.load + text_rva))?;
        let gap = u64::from(next.saturating_sub(*rva));
        let Some(body) = msvc6_link::growth::fill_body(
            text.get(off..).unwrap_or_default(),
            0,
            usize::try_from(gap)?,
        ) else {
            continue;
        };
        let body = u64::try_from(body)?;
        let v = msvc6_link::growth::classify(body, gap);
        *counts.entry(v.tag()).or_insert(0) += 1;
        rows.push(format!(
            "    {rva:#010x}  body {body:>4}  gap {gap:>4}  round16 {:>4}  slot {:>4}  {:<9} {name}",
            msvc6_link::growth::round16(body),
            msvc6_link::growth::slot(body),
            v.tag()
        ));
    }
    println!(
        "vc6: growth, {} over {} consecutive COMDAT pair(s): {}",
        if grown {
            "the incremental relink"
        } else {
            "the first incremental link"
        },
        rows.len(),
        counts
            .iter()
            .map(|(k, n)| format!("{k} {n}"))
            .collect::<Vec<_>>()
            .join(", ")
    );
    for r in &rows {
        println!("{r}");
    }
    if counts.contains_key("OVERLAP") {
        bail!(
            "a gap below round16(body) means the fill scan read past a body:\n{}",
            rows.join("\n")
        );
    }
    Ok(counts)
}

const FIRST_LINK: [(&str, usize); 2] = [("AMBIGUOUS", 2), ("RESERVED", 9)];

const RELINK: [(&str, usize); 3] = [("AMBIGUOUS", 2), ("OVER", 2), ("RESERVED", 7)];

fn pinned(what: &str, got: &BTreeMap<&'static str, usize>, want: &[(&str, usize)]) -> Result<()> {
    let want: BTreeMap<&str, usize> = want.iter().copied().collect();
    if got
        .iter()
        .map(|(k, n)| (*k, *n))
        .collect::<BTreeMap<_, _>>()
        != want
    {
        bail!("{what} was measured as {want:?}; this one reads {got:?}");
    }
    Ok(())
}

fn growth_reserve(c: &Ctx<'_>, scratch: &Path) -> Result<()> {
    let d = growth_dir(c, scratch)?;
    let first = growth_pass(c, &d, false)?;
    pinned("a first incremental link", &first, &FIRST_LINK)?;
    let again = growth_pass(c, &d, true)?;
    pinned("the relink after body5 grew", &again, &RELINK)?;
    println!(
        "vc6: growth: a first `/INCREMENTAL:YES' link lays every COMDAT in its 5/4 slot ({first:?}); \
         the relink after body5 outgrew its slot moves body5 past the rest and keeps every other \
         slot ({again:?})"
    );
    Ok(())
}

fn require() -> bool {
    env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1")
}

fn run() -> Result<bool> {
    let (Some(root), Some(msvcrt)) = (env("MSVC6T_VC6"), env("MSVC6T_MSVCRT")) else {
        println!("{SKIP}; the ICF contrast also needs MSVC6T_MSVCRT, the native msvcrt");
        if require() {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no toolchain or native msvcrt is configured");
        }
        return Ok(false);
    };
    let scratch = std::env::temp_dir().join(format!("msvc6t-link-test-{}", std::process::id()));
    std::fs::create_dir_all(&scratch)?;
    let file = scratch.join("msvc6t.toml");
    srcio::own_write(&file, config_text(&scratch, &root, &msvcrt))?;
    let p = Project::load(&file)?;
    let cfg = p.toolchain_config();
    let runner = Runner::from_config(&cfg)?;
    let result = (|| -> Result<()> {
        let c = Ctx {
            sess: runner.session()?,
            tc: Toolchain::named(&cfg, "vc6")?,
        };
        icf_contrast(&c, &scratch)?;
        iat_round_trip(&c, &scratch)?;
        growth_reserve(&c, &scratch)
    })();
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
