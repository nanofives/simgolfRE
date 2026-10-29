use crate::cli;
use anyhow::{Context as _, Result, bail};
use msvc6_toolchain::cache::Cache;
use msvc6_toolchain::config::{Config, parse_duration, parse_size};
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::Runner;
use msvc6_toolchain::slots::human;
use msvc6_toolchain::version::{self, ImageReader};
use serde::Serialize;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

fn load(c: Option<&Path>) -> Result<Config> {
    msvc6_project::config::toolchain_config(c)
}

pub fn runner(cfg: &Config) -> Result<Runner> {
    let r = Runner::from_config(cfg)?;
    let Some(pool) = r.pool.as_ref().map(|p| p.root.clone()) else {
        return Ok(r);
    };
    let exe = std::env::current_exe().context("locating this msvc6t")?;
    let argv: Vec<OsString> = vec![
        exe.into_os_string(),
        "tc".into(),
        "reap".into(),
        "--watch-owner".into(),
        std::process::id().to_string().into(),
        "--owner-start".into(),
        msvc6_toolchain::procscan::start_time(std::process::id())
            .context("reading this process's start time")?
            .to_string()
            .into(),
        "--pool".into(),
        pool.into_os_string(),
    ];
    Ok(r.with_watchdog(argv))
}

fn toolchain(cfg: &Config, id: Option<&str>) -> Result<Toolchain> {
    match id {
        Some(i) => Toolchain::named(cfg, i),
        None => {
            let first = cfg
                .toolchains
                .keys()
                .next()
                .context("the config has no [toolchain.*]")?;
            Toolchain::named(cfg, first)
        }
    }
}

fn status_code(s: Option<i32>) -> ExitCode {
    ExitCode::from(s.and_then(|v| u8::try_from(v).ok()).unwrap_or(1))
}

#[derive(Debug, Serialize)]
struct InfoRow {
    toolchain: String,
    role: String,
    file: String,
    path: Option<String>,
    version: Option<String>,
    build: Option<u32>,
    names: Option<String>,
    md5: Option<String>,
    expected: Option<String>,
    verdict: Option<bool>,
}

pub fn info(config: Option<&Path>, a: &cli::TcInfoArgs) -> Result<ExitCode> {
    let cfg = load(config)?;
    let ids: Vec<String> = if a.toolchains.is_empty() {
        cfg.toolchains.keys().cloned().collect()
    } else {
        a.toolchains.clone()
    };
    let mut rows = Vec::new();
    let mut failed = false;
    for id in &ids {
        let tc = Toolchain::named(&cfg, id)?;
        println!("toolchain {id}: {}", tc.root.display());
        println!("  bin      {}", tc.bin.display());
        let opt = |p: &Option<PathBuf>| {
            p.as_ref()
                .map_or_else(|| "(none)".to_string(), |p| p.display().to_string())
        };
        println!("  common   {}", opt(&tc.common));
        println!("  lib      {}", opt(&tc.lib));
        println!("  include  {}", opt(&tc.include));
        for bad in version::unknown_expectations(&tc) {
            println!("  EXPECT `{bad}` names no tool role");
            failed = true;
        }
        let found = version::detect(&tc, &ImageReader);
        for t in &found {
            let v = t.version.map(|v| v.to_string());
            let name = t.version.as_ref().and_then(version::build_name);
            let verdict = t.verdict();
            let mark = match (verdict, &t.expected) {
                (Some(true), Some(e)) => format!("  expect {e} ok"),
                (Some(false), Some(e)) => {
                    failed = true;
                    format!("  expect {e} MISMATCH")
                }
                _ => String::new(),
            };
            println!(
                "  {:<6} {:<12} {:<11} {:<52} {}{mark}",
                t.role,
                t.file,
                v.clone().unwrap_or_else(|| {
                    if t.path.is_some() {
                        "no version".into()
                    } else {
                        "absent".into()
                    }
                }),
                name.unwrap_or(""),
                t.md5.as_deref().unwrap_or("")
            );
            rows.push(InfoRow {
                toolchain: id.clone(),
                role: t.role.to_string(),
                file: t.file.to_string(),
                path: t.path.as_ref().map(|p| p.display().to_string()),
                version: v,
                build: t.version.map(|v| v.build),
                names: name.map(str::to_string),
                md5: t.md5.clone(),
                expected: t.expected.clone(),
                verdict,
            });
        }
        if let Some(c2) = version::compiler_build(&found) {
            println!(
                "  => compiler build {} ({})",
                c2.build,
                version::build_name(&c2).unwrap_or("a build this table does not name")
            );
        }
    }
    let r = Runner::from_config(&cfg)?;
    println!("runner: {:?}", r.kind);
    if let Some(p) = &r.pool {
        println!(
            "  prefixes {}  (template {})",
            p.root.display(),
            p.template.display()
        );
    }
    if let Some(m) = &r.msvcrt {
        let md5 = std::fs::read(m).map_or_else(
            |_| "UNREADABLE".to_string(),
            |b| msvc6_base::digest::md5_hex(&b),
        );
        println!(
            "  native msvcrt {} md5 {md5}{}",
            m.display(),
            r.msvcrt_md5
                .as_deref()
                .map(|p| if p.eq_ignore_ascii_case(&md5) {
                    " (pinned)".to_string()
                } else {
                    format!(" NOT THE PIN {p}")
                })
                .unwrap_or_default()
        );
    }
    let mem = msvc6_toolchain::slots::memory();
    println!(
        "  jobs {} (tokens {}), {} usable memory ({} available{})",
        r.jobs,
        r.tokens.dir.display(),
        human(mem.usable()),
        human(mem.available),
        mem.cgroup_headroom
            .map(|h| format!(", cgroup headroom {}", human(h)))
            .unwrap_or_default()
    );
    if let Some(d) = &r.display {
        println!(
            "  display {} {} ({})",
            d.name,
            if d.up() { "up" } else { "down" },
            d.socket().display()
        );
    }
    if let Some(j) = &a.json {
        msvc6_base::rowjson::write(j, "tc info", "", &rows)?;
    }
    Ok(if failed {
        ExitCode::from(1)
    } else {
        ExitCode::SUCCESS
    })
}

pub fn run(
    config: Option<&Path>,
    a: &cli::TcRunArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<ExitCode> {
    guard()?;
    let cfg = load(config)?;
    let tc = toolchain(&cfg, a.toolchain.as_deref())?;
    let r = runner(&cfg)?;
    let sess = r.session()?;
    let tool = tc.tool(&a.tool)?;
    let env = msvc6_toolchain::runner::tool_env(&tc, &r.winepath);
    let cwd = match &a.cwd {
        Some(d) => d.clone(),
        None => std::env::current_dir()?,
    };
    let timeout = a.timeout.as_deref().map(parse_duration).transpose()?;
    let out = sess.run(&tool, &a.args, Some(&cwd), &env, timeout)?;
    print!("{}", msvc6_base::srcio::tool_text(&out.stdout));
    eprint!("{}", msvc6_base::srcio::tool_text(&out.stderr));
    if out.timed_out {
        bail!("{} timed out; its prefix was reaped", a.tool);
    }
    Ok(status_code(out.status))
}

pub fn reap(config: Option<&Path>, a: &cli::TcReapArgs) -> Result<ExitCode> {
    let pool = match &a.pool {
        Some(p) => p.clone(),
        None => {
            let cfg = load(config)?;
            Runner::from_config(&cfg)?
                .pool
                .map(|p| p.root)
                .context("[runner] kind is native: there is no prefix pool")?
        }
    };
    let s = match (a.watch_owner, a.owner_start) {
        (Some(owner), Some(start)) => msvc6_toolchain::reap::watch(owner, start, &pool),
        _ => msvc6_toolchain::reap::sweep(&pool, a.dry_run, None),
    };
    if a.watch_owner.is_some() && s.orphans.is_empty() && s.released.is_empty() {
        return Ok(ExitCode::SUCCESS);
    }
    let verb = if a.dry_run { "would kill" } else { "killed" };
    println!(
        "reap {}: {} slot(s), {} held by a live holder",
        pool.display(),
        s.slots,
        s.held
    );
    for (p, proc_) in &s.orphans {
        println!("  {verb} {} {} in {}", proc_.pid, proc_.comm, p.display());
    }
    for (p, m) in &s.released {
        println!(
            "  {} {} ({m}): its holder is gone",
            if a.dry_run {
                "would release"
            } else {
                "released"
            },
            p.display()
        );
    }
    for (p, why) in &s.refused {
        println!("  REFUSED {}: {why}", p.display());
    }
    Ok(ExitCode::SUCCESS)
}

pub fn implib(
    config: Option<&Path>,
    a: &cli::TcImplibArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<ExitCode> {
    guard()?;
    let cfg = load(config)?;
    let tc = toolchain(&cfg, a.toolchain.as_deref())?;
    let r = runner(&cfg)?;
    let is_def = a
        .def
        .extension()
        .is_some_and(|e| e.eq_ignore_ascii_case("def"));
    let def = if is_def {
        msvc6_base::srcio::read(&a.def).with_context(|| format!("reading {}", a.def.display()))?
    } else {
        let name = a
            .def
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default();
        msvc6_toolchain::implib::def_from_dll(&name, &std::fs::read(&a.def)?, false)?
    };
    let library =
        msvc6_toolchain::implib::def_library(&def).context("the .def has no LIBRARY line")?;
    let hints = match &a.hints_from {
        Some(p) => msvc6_toolchain::implib::reference_hints(&std::fs::read(p)?, &library)?,
        None => Default::default(),
    };
    let sess = r.session()?;
    let work = std::env::temp_dir().join(format!("msvc6t-implib-{}", std::process::id()));
    let built = msvc6_toolchain::implib::build(
        &sess,
        &tc,
        &def,
        &work,
        &a.out,
        &msvc6_toolchain::implib::Opts {
            link50compat: a.link50compat,
            verbatim_symbols: a.verbatim_symbols,
            import_name: a.import_name,
            hints,
            lib_args: Vec::new(),
        },
    )?;
    let _ = std::fs::remove_dir_all(&work);
    for l in &built.lines {
        println!("  {l}");
    }
    println!(
        "{}: {} export(s) -> {} ({} member(s), {} long, {} hinted, LIB of {})",
        built.library,
        built.entries,
        a.out.display(),
        built.members,
        built.long,
        built.hinted,
        tc.id
    );
    for s in &built.symbols {
        println!("    {s}");
    }
    Ok(ExitCode::SUCCESS)
}

pub fn clmap(a: &cli::TcClmapArgs) -> Result<ExitCode> {
    use std::io::Read as _;
    let sources: Vec<msvc6_toolchain::clmap::Source> = a
        .sources
        .iter()
        .map(|p| {
            let text =
                msvc6_base::srcio::read(p).with_context(|| format!("reading {}", p.display()))?;
            Ok(msvc6_toolchain::clmap::Source::new(p, &text))
        })
        .collect::<Result<_>>()?;
    let mut raw = Vec::new();
    std::io::stdin().read_to_end(&mut raw)?;
    let t = msvc6_toolchain::clmap::translate(&msvc6_base::srcio::tool_text(&raw), &sources);
    print!("{}", t.text);
    eprintln!(
        "clmap: {} diagnostic(s), {} annotated, {} naming no SOURCE (sources are paired by file name)",
        t.seen, t.annotated, t.unmatched
    );
    Ok(ExitCode::SUCCESS)
}

#[derive(Debug, Serialize)]
struct BuildRow {
    image: String,
    output: String,
    md5: String,
    size: u64,
    crt: String,
    seconds: f64,
    steps: Vec<String>,
    post: Vec<String>,
}

fn print_report(rep: &msvc6_toolchain::msdev::Report) {
    for s in &rep.steps {
        println!(
            "  step {}: {}{}",
            s.step,
            s.summary,
            if s.unresolved > 0 {
                format!(", {} unresolved reference(s)", s.unresolved)
            } else {
                String::new()
            }
        );
    }
    println!("  {}", rep.crt_line);
    if !rep.tools.is_empty() {
        println!("  tools that ran: {}", rep.tools.join(", "));
    }
    for p in &rep.post {
        println!("  {p}");
    }
    println!(
        "  {}  size {}  md5 {}  ({} file(s) staged, {:.1} s)",
        rep.output.display(),
        rep.size,
        rep.md5,
        rep.staged_files,
        rep.seconds
    );
}

pub fn build(
    config: Option<&Path>,
    a: &cli::BuildArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<ExitCode> {
    guard()?;
    let cfg = load(config)?;
    let ids: Vec<String> = if a.all {
        cfg.images
            .iter()
            .filter(|i| i.build.is_some())
            .map(|i| i.id.clone())
            .collect()
    } else {
        a.images.clone()
    };
    if ids.is_empty() {
        bail!("name the images to build, or pass --all");
    }
    let images = ids
        .iter()
        .map(|id| cfg.image(id))
        .collect::<Result<Vec<_>>>()?;
    let r = runner(&cfg)?;
    let out_root = a.out.clone().unwrap_or_else(|| PathBuf::from("msvc6t-out"));
    let timeout = a.timeout.as_deref().map(parse_duration).transpose()?;
    let width = r.batch(a.jobs.unwrap_or(images.len()).min(images.len()))?;
    if width > 1 {
        println!("build: {} image(s), {width} at once", images.len());
    }
    let next = std::sync::atomic::AtomicUsize::new(0);
    let results: Vec<std::sync::Mutex<Option<Result<msvc6_toolchain::msdev::Report>>>> =
        images.iter().map(|_| std::sync::Mutex::new(None)).collect();
    std::thread::scope(|s| {
        for _ in 0..width {
            s.spawn(|| {
                loop {
                    let i = next.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                    let (Some(image), Some(slot)) = (images.get(i), results.get(i)) else {
                        break;
                    };
                    let done = msvc6_toolchain::msdev::build(
                        &cfg,
                        &r,
                        image,
                        &msvc6_toolchain::msdev::Opts {
                            out: out_root.join(&image.id),
                            keep_staging: a.keep_staging,
                            link_env: a.link_env.clone(),
                            timeout,
                        },
                    );
                    if let Ok(mut g) = slot.lock() {
                        *g = Some(done);
                    }
                }
            });
        }
    });
    let mut rows = Vec::new();
    let mut failed = 0usize;
    for (image, slot) in images.iter().zip(results) {
        println!(
            "build {}: -> {}",
            image.id,
            out_root.join(&image.id).display()
        );
        match slot.into_inner().ok().flatten() {
            Some(Ok(rep)) => {
                print_report(&rep);
                rows.push(BuildRow {
                    image: rep.image.clone(),
                    output: rep.output.display().to_string(),
                    md5: rep.md5.clone(),
                    size: rep.size,
                    crt: rep.crt_line.clone(),
                    seconds: rep.seconds,
                    steps: rep.steps.iter().map(|s| s.summary.clone()).collect(),
                    post: rep.post.clone(),
                });
            }
            Some(Err(e)) => {
                failed += 1;
                println!("  BUILD FAILED: {e:#}");
            }
            None => {
                failed += 1;
                println!("  BUILD FAILED: no result");
            }
        }
    }
    if let Some(j) = &a.json {
        msvc6_base::rowjson::write(j, "build", "", &rows)?;
    }
    if failed > 0 {
        bail!("{failed} of {} image(s) did not build", images.len());
    }
    Ok(ExitCode::SUCCESS)
}

pub fn crtguard(a: &cli::CrtguardArgs) -> Result<ExitCode> {
    let mut bad = 0;
    for l in &a.logs {
        let b = std::fs::read(l).with_context(|| format!("reading {}", l.display()))?;
        let (line, ok) =
            msvc6_toolchain::crtguard::line(&msvc6_base::srcio::tool_text(&b), &a.require);
        println!("{}: {line}", l.display());
        if !ok {
            bad += 1;
        }
    }
    Ok(if bad > 0 {
        ExitCode::from(1)
    } else {
        ExitCode::SUCCESS
    })
}

fn cache_of(c: Option<&Path>) -> Result<Cache> {
    match msvc6_project::config::Project::find(c)? {
        Some(p) => Cache::from_config(&p.toolchain_config()),
        None => Cache::from_config(&Config::empty(&std::env::current_dir()?)),
    }
}

pub fn cache(config: Option<&Path>, a: &cli::CacheArgs) -> Result<ExitCode> {
    use msvc6_toolchain::gc;
    match &a.cmd {
        cli::CacheCmd::Stats(_) => {
            let c = cache_of(config)?;
            let scan = gc::scan(&c.root);
            let now = std::time::SystemTime::now();
            println!(
                "cache {}: {} entr{} in {}{}",
                c.root.display(),
                scan.entries.len(),
                if scan.entries.len() == 1 { "y" } else { "ies" },
                human(scan.bytes),
                c.cap
                    .map(|m| format!(" (cap {})", human(m)))
                    .unwrap_or_default()
            );
            let buckets: [(&str, u64); 5] = [
                ("< 1 h", 3600),
                ("< 1 day", 86_400),
                ("< 1 week", 604_800),
                ("< 30 days", 2_592_000),
                ("older", u64::MAX),
            ];
            let mut lo = 0u64;
            for (label, hi) in buckets {
                let (n, b) = scan
                    .entries
                    .iter()
                    .filter(|e| {
                        let age = now.duration_since(e.used).map_or(0, |d| d.as_secs());
                        age >= lo && age < hi
                    })
                    .fold((0usize, 0u64), |(n, b), e| (n + 1, b + e.bytes));
                println!("  last used {label:<10} {n:>7} entries {:>12}", human(b));
                lo = hi;
            }
            if !scan.broken.is_empty() {
                println!("  {} entr(ies) without an OK record", scan.broken.len());
            }
            Ok(ExitCode::SUCCESS)
        }
        cli::CacheCmd::Gc(g) => {
            let c = cache_of(config)?;
            let cap = g.max.as_deref().map(parse_size).transpose()?.or(c.cap);
            let older = g.older_than.as_deref().map(parse_duration).transpose()?;
            if cap.is_none() && older.is_none() {
                bail!("no cap: pass --max or --older-than, or set [cache] max");
            }
            let r = gc::gc(&c, cap, older, g.dry_run)?
                .context("another gc holds the cache's gc.lock: it is running now")?;
            println!(
                "gc {}: {} entries, {} -> {} {} entr(ies) ({}), {} manifest(s) {}, {} trashed entr(ies) {}; {} name(s) not the cache's left alone",
                c.root.display(),
                r.entries,
                human(r.before),
                if g.dry_run { "would evict" } else { "evicted" },
                r.evicted,
                human(r.evicted_bytes),
                r.manifests,
                if g.dry_run {
                    "would be pruned"
                } else {
                    "pruned"
                },
                r.purged,
                if g.dry_run {
                    "would be purged"
                } else {
                    "purged"
                },
                r.foreign
            );
            Ok(ExitCode::SUCCESS)
        }
        cli::CacheCmd::Verify(_) => {
            let c = cache_of(config)?;
            let bad = gc::verify(&c)?;
            for (k, why) in &bad {
                println!("  BAD {k}: {why}");
            }
            println!("verify {}: {} bad entr(ies)", c.root.display(), bad.len());
            Ok(if bad.is_empty() {
                ExitCode::SUCCESS
            } else {
                ExitCode::from(1)
            })
        }
    }
}
