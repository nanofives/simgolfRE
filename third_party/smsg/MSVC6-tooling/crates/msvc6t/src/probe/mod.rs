mod expect;
mod glob;
mod il;
mod listing;
mod views;

use crate::Outcome;
use crate::cli::ProbeArgs;
use anyhow::{Context as _, Result, anyhow, bail};
use expect::{Crt, Expect, View};
use msvc6_toolchain::cache::{Cache, Outputs};
use msvc6_toolchain::compile::{Job, compile};
use msvc6_toolchain::config::{Config, RunnerKind};
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use msvc6_toolchain::version::{self, ImageReader};
use serde::Serialize;
use std::collections::{BTreeMap, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::Mutex;

pub const MARKER: &str = "MSVC6T-PROBE-RUN";
pub const MAX_INCLUDE: u64 = 1 << 26;
pub const RESULTS_HEADER: &str = "probe\tsource\ttoolchain\tcl\tfront\tc2\tlink\tstatus\tgate";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Probe {
    pub name: String,
    pub source: PathBuf,
    pub expect: PathBuf,
}

impl Probe {
    fn ext(&self) -> String {
        self.source
            .extension()
            .map(|e| e.to_string_lossy().to_ascii_lowercase())
            .unwrap_or_default()
    }

    fn front(&self) -> &'static str {
        if self.ext() == "c" { "c1" } else { "c1xx" }
    }
}

fn source_ext(p: &Path) -> bool {
    p.extension()
        .is_some_and(|e| e.eq_ignore_ascii_case("c") || e.eq_ignore_ascii_case("cpp"))
}

fn expect_ext(p: &Path) -> bool {
    p.extension().is_some_and(|e| e == "expect")
}

fn stem(p: &Path) -> String {
    p.file_stem()
        .map(|s| s.to_string_lossy().into_owned())
        .unwrap_or_default()
}

fn files_of(dir: &Path) -> Result<Vec<PathBuf>> {
    let mut v: Vec<PathBuf> = std::fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.is_file())
        .collect();
    v.sort();
    Ok(v)
}

pub fn discover(paths: &[PathBuf]) -> Result<Vec<Probe>> {
    let mut sources: BTreeMap<(PathBuf, String), Vec<PathBuf>> = BTreeMap::new();
    let mut expects: BTreeMap<(PathBuf, String), PathBuf> = BTreeMap::new();
    for p in paths {
        let files = if p.is_dir() {
            files_of(p)?
        } else if p.is_file() {
            let dir = p.parent().unwrap_or_else(|| Path::new("."));
            let s = stem(p);
            files_of(if dir.as_os_str().is_empty() {
                Path::new(".")
            } else {
                dir
            })?
            .into_iter()
            .filter(|f| stem(f) == s)
            .collect()
        } else {
            bail!("{}: no such probe or directory", p.display());
        };
        for f in files {
            let key = (
                f.parent().map(Path::to_path_buf).unwrap_or_default(),
                stem(&f),
            );
            if source_ext(&f) {
                let v = sources.entry(key).or_default();
                if !v.contains(&f) {
                    v.push(f);
                }
            } else if expect_ext(&f) {
                expects.insert(key, f);
            }
        }
    }
    let mut out = Vec::new();
    for (key, srcs) in &sources {
        let [src] = srcs.as_slice() else {
            bail!(
                "{}: one probe has one source; {} name the stem `{}`",
                key.0.display(),
                srcs.len(),
                key.1
            );
        };
        let Some(exp) = expects.get(key) else {
            bail!("{}: a source with no {}.expect", src.display(), key.1);
        };
        out.push(Probe {
            name: key.1.clone(),
            source: src.clone(),
            expect: exp.clone(),
        });
    }
    if let Some((_, e)) = expects.iter().find(|(k, _)| !sources.contains_key(*k)) {
        bail!("{}: an .expect with no .c or .cpp beside it", e.display());
    }
    let mut seen = BTreeMap::new();
    for p in &out {
        if let Some(other) = seen.insert(p.name.clone(), p.source.clone()) {
            bail!(
                "two probes are named `{}`: {} and {}",
                p.name,
                other.display(),
                p.source.display()
            );
        }
    }
    Ok(out)
}

pub fn source_problem(bytes: &[u8]) -> Option<String> {
    let lx = msvc6_source::lex::lex(bytes);
    if let Some(p) = lx.problems.first() {
        return Some(format!(
            "line {}: the lexer cannot read it as CL does: {}",
            p.line, p.what
        ));
    }
    lx.comments.first().map(|c| {
        format!(
            "line {}: a comment ({} in all); a probe source is comment-free and its `rule` lines say what it shows",
            c.first,
            lx.comments.len()
        )
    })
}

pub fn load(p: &Probe) -> Result<Expect> {
    let text = msvc6_base::srcio::own_read(&p.expect)
        .with_context(|| format!("reading {}", p.expect.display()))?;
    let e = expect::parse(&text).with_context(|| p.expect.display().to_string())?;
    let bytes =
        std::fs::read(&p.source).with_context(|| format!("reading {}", p.source.display()))?;
    if let Some(why) = source_problem(&bytes) {
        bail!("{}: {why}", p.source.display());
    }
    for inc in &e.includes {
        let path = include_path(p, inc);
        let bytes = msvc6_il::file::read(&path, MAX_INCLUDE).with_context(|| {
            format!(
                "{}: `include {inc}` names no file beside the probe",
                p.expect.display()
            )
        })?;
        if let Some(why) = source_problem(&bytes) {
            bail!("{}: {why}", path.display());
        }
    }
    Ok(e)
}

fn include_path(p: &Probe, name: &str) -> PathBuf {
    p.source
        .parent()
        .map_or_else(|| PathBuf::from(name), |d| d.join(name))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "UPPERCASE")]
pub enum Status {
    Pass,
    Fail,
    Xfail,
    Xpass,
    Error,
    Skip,
}

impl Status {
    #[must_use]
    pub fn word(self) -> &'static str {
        match self {
            Self::Pass => "PASS",
            Self::Fail => "FAIL",
            Self::Xfail => "XFAIL",
            Self::Xpass => "XPASS",
            Self::Error => "ERROR",
            Self::Skip => "SKIP",
        }
    }

    fn bad(self) -> bool {
        matches!(self, Self::Fail | Self::Xpass | Self::Error)
    }
}

#[derive(Debug, Clone)]
struct Tc {
    tc: Toolchain,
    builds: BTreeMap<&'static str, u32>,
    gate: bool,
}

impl Tc {
    fn build(&self, role: &str) -> Option<u32> {
        self.builds.get(role).copied()
    }

    fn label(&self) -> String {
        let b = |r: &str| {
            self.build(r)
                .map_or_else(|| "?".to_string(), |n| n.to_string())
        };
        format!(
            "{} (CL {}, C1 {}, C1XX {}, C2 {}, LINK {})",
            self.tc.id,
            b("cl"),
            b("c1"),
            b("c1xx"),
            b("c2"),
            b("link")
        )
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct Row {
    pub probe: String,
    pub source: String,
    pub toolchain: String,
    pub cl: Option<u32>,
    pub front: Option<u32>,
    pub c2: Option<u32>,
    pub link: Option<u32>,
    pub status: Status,
    pub gate: bool,
    pub rules: Vec<String>,
    pub detail: Vec<String>,
}

impl Row {
    #[must_use]
    pub fn tsv(&self) -> String {
        let n = |v: Option<u32>| v.map_or_else(|| "?".to_string(), |x| x.to_string());
        format!(
            "{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
            self.probe,
            self.source,
            self.toolchain,
            n(self.cl),
            n(self.front),
            n(self.c2),
            n(self.link),
            self.status.word(),
            if self.gate { "gate" } else { "record" }
        )
    }
}

fn args(v: &[&str]) -> Vec<String> {
    v.iter().map(|s| (*s).to_string()).collect()
}

fn reset(dir: &Path) -> Result<()> {
    if dir.exists() {
        std::fs::remove_dir_all(dir).with_context(|| format!("clearing {}", dir.display()))?;
    }
    std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))
}

struct Views {
    asm: String,
    clout: Vec<String>,
    obj: Result<Vec<String>, String>,
    link: Option<Linked>,
    il: Option<Result<Vec<String>, String>>,
}

struct Linked {
    text: Vec<String>,
    map: Vec<String>,
    image: Vec<u8>,
}

fn link_env(runner: &Runner, e: &Expect) -> Vec<(String, String)> {
    let mut env = Vec::new();
    if runner.kind != RunnerKind::Native {
        env.push(("WINEDEBUG".to_string(), "-all,+loaddll".to_string()));
        if e.crt == Crt::Builtin {
            env.push(("WINEDLLOVERRIDES".to_string(), "msvcrt=b".to_string()));
        }
    }
    env
}

fn run_link(
    sess: &Session<'_>,
    tc: &Toolchain,
    e: &Expect,
    dir: &Path,
    name: &str,
) -> Result<Linked> {
    let switches = e.link.clone().unwrap_or_default();
    let dll = switches.iter().any(|s| s.eq_ignore_ascii_case("/dll"));
    let out = format!("{name}.{}", if dll { "dll" } else { "exe" });
    let mut a = vec!["/nologo".to_string()];
    a.extend(switches);
    a.push(format!("/out:{out}"));
    a.push(format!("/map:{name}.map"));
    a.push(format!("{name}.obj"));
    let mut env = msvc6_toolchain::runner::tool_env(tc, &sess.runner.winepath);
    for (k, v) in link_env(sess.runner, e) {
        env.retain(|(x, _)| x != &k);
        env.push((k, v));
    }
    let o = sess.run(&tc.tool("LINK.EXE")?, &a, Some(dir), &env, None)?;
    let stderr = msvc6_base::srcio::tool_text(&o.stderr);
    let mut text = msvc6_base::srcio::tool_text(&o.stdout);
    text.push_str(&msvc6_toolchain::crtguard::non_trace(&stderr));
    if !o.ok() {
        bail!(
            "LINK exited {}:\n{}",
            o.status
                .map_or_else(|| "on a signal".to_string(), |s| s.to_string()),
            text.trim()
        );
    }
    if sess.runner.kind != RunnerKind::Native {
        let (line, native) = msvc6_toolchain::crtguard::line(&stderr, &[]);
        match (e.crt, native) {
            (Crt::Native, false) => bail!("LINK did not run on the native msvcrt: {line}"),
            (Crt::Builtin, true) => bail!("`link-crt builtin`, and LINK ran on the native msvcrt"),
            _ => {}
        }
    }
    let map = msvc6_base::srcio::read(dir.join(format!("{name}.map")))
        .with_context(|| format!("LINK wrote no {name}.map"))?;
    let image = std::fs::read(dir.join(&out)).with_context(|| format!("LINK wrote no {out}"))?;
    Ok(Linked {
        text: listing::text_lines(&text),
        map: views::map(&map),
        image,
    })
}

fn build_views(
    sess: &Session<'_>,
    cache: Option<&Cache>,
    tc: &Toolchain,
    p: &Probe,
    e: &Expect,
    dir: &Path,
) -> Result<Views> {
    reset(dir)?;
    let src_name = format!("{}.{}", p.name, p.ext());
    std::fs::copy(&p.source, dir.join(&src_name))
        .with_context(|| format!("copying {}", p.source.display()))?;
    for inc in &e.includes {
        let from = include_path(p, inc);
        std::fs::copy(&from, dir.join(inc))
            .with_context(|| format!("copying {}", from.display()))?;
    }
    let keep_il = e.blocks.iter().any(|b| b.view == View::Il);
    let mut a = args(&["/nologo", "/c"]);
    a.extend(e.cl.iter().cloned());
    if keep_il {
        a.push(il::bk_switch(&p.name));
    }
    a.push("/FAs".to_string());
    a.push(format!("/Fa{}.asm", p.name));
    a.push(format!("/Fo{}.obj", p.name));
    a.push(src_name);
    let job = Job {
        tc,
        dir: dir.to_path_buf(),
        args: a,
        outs: Outputs {
            obj: dir.join(format!("{}.obj", p.name)),
            cod: Some(dir.join(format!("{}.asm", p.name))),
        },
        env: Vec::new(),
        receipt: false,
    };
    let compiled = compile(sess, cache, &job)?;
    msvc6_base::srcio::own_write(dir.join("cl.out"), &compiled.text)?;
    let asm = msvc6_base::srcio::read(dir.join(format!("{}.asm", p.name)))
        .context("CL wrote no listing")?;
    let obj_bytes = std::fs::read(&job.outs.obj).context("CL wrote no object")?;
    let link = match &e.link {
        None => None,
        Some(_) => Some(run_link(sess, tc, e, dir, &p.name)?),
    };
    let il = keep_il.then(|| {
        tc.tool("C2.DLL")
            .and_then(|c2| il::view(dir, &p.name, &c2))
            .map_err(|x| format!("{x:#}"))
    });
    let v = Views {
        asm,
        clout: listing::text_lines(&compiled.text),
        obj: views::obj(&obj_bytes).map_err(|x| format!("{x:#}")),
        link,
        il,
    };
    write_views(&v, dir)?;
    Ok(v)
}

fn write_views(v: &Views, dir: &Path) -> Result<()> {
    let put = |name: &str, lines: &[String]| -> Result<()> {
        let mut text = lines.join("\n");
        text.push('\n');
        msvc6_base::srcio::own_write(dir.join(format!("view-{name}.txt")), text)
            .with_context(|| format!("writing the {name} view in {}", dir.display()))
    };
    put("data", &listing::data(&v.asm))?;
    put("listing", &il::listing(&v.asm))?;
    put("clout", &v.clout)?;
    match &v.il {
        Some(Ok(l)) => put("il", l)?,
        Some(Err(e)) => put("il", std::slice::from_ref(e))?,
        None => {}
    }
    match &v.obj {
        Ok(l) => put("obj", l)?,
        Err(e) => put("obj", std::slice::from_ref(e))?,
    }
    if let Some(l) = &v.link {
        put("map", &l.map)?;
        put("linkout", &l.text)?;
        put("rich", &views::rich_lines(&l.image))?;
        put(
            "imports",
            &views::imports(&l.image).unwrap_or_else(|e| vec![format!("{e:#}")]),
        )?;
    }
    Ok(())
}

fn view_lines(v: &Views, b: &expect::Block) -> Result<Vec<String>, String> {
    let linked = || {
        v.link
            .as_ref()
            .ok_or_else(|| "nothing was linked".to_string())
    };
    match b.view {
        View::Asm | View::Frame => {
            let name = b.func.as_deref().unwrap_or_default();
            let fns = listing::functions(&v.asm);
            let f = fns.get(name).ok_or_else(|| {
                format!(
                    "no function `{name}` in the listing; it has {}",
                    fns.keys().cloned().collect::<Vec<_>>().join(", ")
                )
            })?;
            Ok(if b.view == View::Asm {
                f.code.clone()
            } else {
                f.frame.clone()
            })
        }
        View::Data => Ok(listing::data(&v.asm)),
        View::ClOut => Ok(v.clout.clone()),
        View::Obj => v.obj.clone(),
        View::Map => linked().map(|l| l.map.clone()),
        View::LinkOut => linked().map(|l| l.text.clone()),
        View::Rich => linked().map(|l| views::rich_lines(&l.image)),
        View::Imports => {
            linked().and_then(|l| views::imports(&l.image).map_err(|x| format!("{x:#}")))
        }
        View::Il => {
            v.il.clone()
                .unwrap_or_else(|| Err("no IL was kept".to_string()))
        }
        View::Listing => Ok(il::listing(&v.asm)),
    }
}

fn judge(v: &Views, e: &Expect, build: &dyn Fn(&str) -> Option<u32>) -> Vec<String> {
    let mut failed = Vec::new();
    for b in &e.blocks {
        let pats: Vec<glob::Pat> = b
            .pats
            .iter()
            .map(|p| glob::Pat {
                text: expect::with_builds(&p.text, build),
                ..p.clone()
            })
            .collect();
        let r = view_lines(v, b).and_then(|lines| glob::check(&pats, &lines, b.exact));
        if let Err(why) = r {
            let what = b.func.as_ref().map_or_else(
                || b.view.word().to_string(),
                |f| format!("{} {f}", b.view.word()),
            );
            failed.push(format!("line {}: `{what}`: {why}", b.line));
        }
    }
    failed
}

struct Job1<'a> {
    tc: &'a Tc,
    probe: &'a Probe,
}

fn run_job(
    sess: &Session<'_>,
    cache: Option<&Cache>,
    out: &Path,
    j: &Job1<'_>,
    link_ok: bool,
) -> Row {
    let mut row = Row {
        probe: j.probe.name.clone(),
        source: j
            .probe
            .source
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default(),
        toolchain: j.tc.tc.id.clone(),
        cl: j.tc.build("cl"),
        front: j.tc.build(j.probe.front()),
        c2: j.tc.build("c2"),
        link: j.tc.build("link"),
        status: Status::Error,
        gate: j.tc.gate,
        rules: Vec::new(),
        detail: Vec::new(),
    };
    let e = match load(j.probe) {
        Ok(e) => e,
        Err(x) => {
            row.detail.push(format!("{x:#}"));
            return row;
        }
    };
    row.rules.clone_from(&e.rules);
    if e.link.is_some() && !link_ok {
        row.status = Status::Skip;
        row.detail.push(
            "a link probe: [runner] names no native_msvcrt, and LINK on wine's builtin qsort is another link"
                .to_string(),
        );
        return row;
    }
    let dir = out.join(&j.tc.tc.id).join(&j.probe.name);
    match build_views(sess, cache, &j.tc.tc, j.probe, &e, &dir) {
        Err(x) => row.detail.push(format!("{x:#}")),
        Ok(v) => {
            row.detail = judge(&v, &e, &|r| j.tc.build(r));
            row.status = match (row.detail.is_empty(), e.xfail.is_some()) {
                (true, false) => Status::Pass,
                (false, false) => Status::Fail,
                (false, true) => Status::Xfail,
                (true, true) => Status::Xpass,
            };
            if let Some(why) = &e.xfail {
                row.detail.insert(0, format!("xfail: {why}"));
            }
        }
    }
    row
}

fn out_dir(a: &ProbeArgs) -> Result<(PathBuf, std::fs::File)> {
    let out = a
        .out
        .clone()
        .unwrap_or_else(|| std::env::temp_dir().join("msvc6t-probe"));
    std::fs::create_dir_all(&out).with_context(|| format!("creating {}", out.display()))?;
    let marker = out.join(MARKER);
    let empty = std::fs::read_dir(&out).is_ok_and(|mut d| d.next().is_none());
    if !empty && !marker.is_file() {
        bail!(
            "{} holds files and no {MARKER}: it is not a probe run directory (pass --out)",
            out.display()
        );
    }
    msvc6_base::srcio::own_write(&marker, "msvc6t probe runs\n")?;
    let lock = std::fs::File::create(out.join(".lock"))?;
    lock.try_lock()
        .map_err(|_| anyhow!("another msvc6t probe is running in {}", out.display()))?;
    Ok((out, lock))
}

fn toolchains(cfg: &Config, a: &ProbeArgs) -> Result<Vec<Tc>> {
    let ids: Vec<String> = if a.toolchains.is_empty() {
        cfg.toolchains.keys().cloned().collect()
    } else {
        a.toolchains.clone()
    };
    if ids.is_empty() {
        bail!("the config names no [toolchain.*]");
    }
    if let Some(g) = a.gate.iter().find(|g| !ids.contains(g)) {
        bail!(
            "--gate {g}: not a toolchain this run uses ({})",
            ids.join(", ")
        );
    }
    let mut out = Vec::new();
    for id in &ids {
        let tc = Toolchain::named(cfg, id)?;
        let found = version::detect(&tc, &ImageReader);
        if let Some(bad) = version::unknown_expectations(&tc).first() {
            bail!("toolchain {id}: `expect` names `{bad}`, which is no tool role");
        }
        if let Some(t) = found.iter().find(|t| t.verdict() == Some(false)) {
            bail!(
                "toolchain {id}: {} is {}, and `expect` says {}",
                t.file,
                t.version
                    .map_or_else(|| "absent".to_string(), |v| v.to_string()),
                t.expected.clone().unwrap_or_default()
            );
        }
        let builds = found
            .iter()
            .filter_map(|t| t.version.map(|v| (t.role, v.build)))
            .collect();
        out.push(Tc {
            tc,
            builds,
            gate: a.gate.is_empty() || a.gate.contains(id),
        });
    }
    Ok(out)
}

fn default_paths(a: &ProbeArgs, root: &dyn Fn() -> Result<PathBuf>) -> Result<Vec<PathBuf>> {
    if a.probes.is_empty() {
        Ok(vec![root()?.join("probes")])
    } else {
        Ok(a.probes.clone())
    }
}

pub fn check_only(probes: &[Probe]) -> Result<Outcome> {
    let mut bad = 0usize;
    for p in probes {
        match load(p) {
            Ok(e) => println!(
                "ok     {} ({}, {} block(s){})",
                p.name,
                p.front(),
                e.blocks.len(),
                if e.link.is_some() { ", linked" } else { "" }
            ),
            Err(x) => {
                bad = bad.saturating_add(1);
                println!("BROKEN {}: {x:#}", p.name);
            }
        }
    }
    println!("probe --check: {} probe(s), {bad} broken", probes.len());
    if bad > 0 {
        bail!("{bad} probe(s) cannot run");
    }
    Ok(Outcome::Ok)
}

pub fn write_results(path: &Path, rows: &[Row]) -> Result<()> {
    let mut text = format!("{RESULTS_HEADER}\n");
    for r in rows {
        text.push_str(&r.tsv());
        text.push('\n');
    }
    msvc6_base::srcio::own_write(path, text).with_context(|| format!("writing {}", path.display()))
}

pub fn run(
    config: Option<&Path>,
    a: &ProbeArgs,
    guard: &dyn Fn() -> Result<()>,
    root: &dyn Fn() -> Result<PathBuf>,
) -> Result<Outcome> {
    let probes = discover(&default_paths(a, root)?)?;
    if probes.is_empty() {
        bail!("no probe: a probe is <name>.c or <name>.cpp beside <name>.expect");
    }
    if a.check {
        return check_only(&probes);
    }
    guard()?;
    let cfg = msvc6_project::config::toolchain_config(config)?;
    let tcs = toolchains(&cfg, a)?;
    let runner = crate::tc::runner(&cfg)?;
    let link_ok = runner.kind == RunnerKind::Native || runner.msvcrt.is_some();
    let cache = Cache::from_config(&cfg)?;
    let (out, _lock) = out_dir(a)?;
    for t in &tcs {
        println!(
            "toolchain {}{}",
            t.label(),
            if t.gate {
                ""
            } else {
                "  (recorded, not gated)"
            }
        );
    }
    let mut queue: VecDeque<Job1<'_>> = VecDeque::new();
    for t in &tcs {
        for p in &probes {
            queue.push_back(Job1 { tc: t, probe: p });
        }
    }
    let want = a.jobs.unwrap_or(runner.jobs).max(1);
    let workers = runner.batch(want.min(queue.len()))?;
    let queue = Mutex::new(queue);
    let rows: Mutex<Vec<Row>> = Mutex::new(Vec::new());
    let failures: Mutex<Vec<String>> = Mutex::new(Vec::new());
    std::thread::scope(|s| {
        for _ in 0..workers {
            s.spawn(|| {
                let sess = match runner.session() {
                    Ok(x) => x,
                    Err(e) => {
                        if let Ok(mut f) = failures.lock() {
                            f.push(format!("a worker got no wine prefix: {e:#}"));
                        }
                        return;
                    }
                };
                loop {
                    let next = queue.lock().ok().and_then(|mut q| q.pop_front());
                    let Some(j) = next else {
                        break;
                    };
                    let row = run_job(&sess, Some(&cache), &out, &j, link_ok);
                    let mut say = format!(
                        "{:<6} {:<10} {}",
                        row.status.word(),
                        row.toolchain,
                        row.probe
                    );
                    if row.status != Status::Pass {
                        for d in &row.detail {
                            say.push_str(&format!("\n         {}", d.replace('\n', "\n         ")));
                        }
                    }
                    println!("{say}");
                    if let Ok(mut r) = rows.lock() {
                        r.push(row);
                    }
                }
            });
        }
    });
    let mut rows = rows
        .into_inner()
        .map_err(|_| anyhow!("a worker panicked"))?;
    let failures = failures
        .into_inner()
        .map_err(|_| anyhow!("a worker panicked"))?;
    let order: BTreeMap<&str, usize> = tcs
        .iter()
        .enumerate()
        .map(|(i, t)| (t.tc.id.as_str(), i))
        .collect();
    rows.sort_by(|x, y| {
        x.probe.cmp(&y.probe).then(
            order
                .get(x.toolchain.as_str())
                .cmp(&order.get(y.toolchain.as_str())),
        )
    });
    let expected = tcs.len().saturating_mul(probes.len());
    for t in &tcs {
        let mine: Vec<&Row> = rows.iter().filter(|r| r.toolchain == t.tc.id).collect();
        let count = |s: Status| mine.iter().filter(|r| r.status == s).count();
        println!(
            "{}: {} PASS, {} FAIL, {} XFAIL, {} XPASS, {} ERROR, {} SKIP{}",
            t.label(),
            count(Status::Pass),
            count(Status::Fail),
            count(Status::Xfail),
            count(Status::Xpass),
            count(Status::Error),
            count(Status::Skip),
            if t.gate { "" } else { " (recorded)" }
        );
    }
    let results = a.results.clone().unwrap_or_else(|| out.join("results.tsv"));
    write_results(&results, &rows)?;
    println!("results: {}", results.display());
    if let Some(j) = &a.json {
        msvc6_base::rowjson::write(j, "probe", "", &rows)?;
    }
    if let Some(f) = failures.first() {
        bail!("{f}");
    }
    if rows.len() != expected {
        bail!("{} of {expected} probe runs finished", rows.len());
    }
    let gated_bad = rows.iter().filter(|r| r.gate && r.status.bad()).count();
    if rows.iter().any(|r| r.gate && r.status == Status::Error) {
        bail!("{gated_bad} gated probe run(s) failed, an error among them");
    }
    Ok(if gated_bad > 0 {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6t-probe-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn put(d: &Path, name: &str, text: &str) {
        msvc6_base::srcio::own_write(d.join(name), text).unwrap();
    }

    #[test]
    fn a_probe_is_a_source_beside_its_expect() {
        let d = scratch("discover");
        put(&d, "a.cpp", "int f;\n");
        put(&d, "a.expect", "rule c2: r\ndata\n  x\n");
        put(&d, "b.c", "int g;\n");
        put(&d, "b.expect", "rule c2: r\ndata\n  x\n");
        put(&d, "results.tsv", "x\n");
        let all = discover(std::slice::from_ref(&d)).unwrap();
        let names: Vec<&str> = all.iter().map(|p| p.name.as_str()).collect();
        assert_eq!(names, vec!["a", "b"]);
        assert_eq!(all[1].front(), "c1");
        assert_eq!(all[0].front(), "c1xx");
        let one = discover(&[d.join("b.expect")]).unwrap();
        assert_eq!(one.len(), 1);
        assert_eq!(one[0].source, d.join("b.c"));
        put(&d, "c.cpp", "int h;\n");
        assert!(discover(std::slice::from_ref(&d)).is_err());
        std::fs::remove_file(d.join("c.cpp")).unwrap();
        put(&d, "d.expect", "rule c2: r\ndata\n  x\n");
        assert!(discover(std::slice::from_ref(&d)).is_err());
        std::fs::remove_file(d.join("d.expect")).unwrap();
        put(&d, "a.c", "int f;\n");
        assert!(discover(std::slice::from_ref(&d)).is_err());
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_source_with_a_comment_is_refused() {
        assert!(source_problem(b"int f;\n").is_none());
        assert!(source_problem(b"int f; // why\n").is_some());
        assert!(source_problem(b"int f; /* why */\n").is_some());
        assert!(source_problem(b"const char *s = \"// not one\";\n").is_none());
    }

    #[test]
    fn a_results_row_is_one_tab_separated_line() {
        let r = Row {
            probe: "p".into(),
            source: "p.cpp".into(),
            toolchain: "vc6".into(),
            cl: Some(8168),
            front: Some(8168),
            c2: Some(8447),
            link: None,
            status: Status::Pass,
            gate: false,
            rules: Vec::new(),
            detail: Vec::new(),
        };
        assert_eq!(r.tsv(), "p\tp.cpp\tvc6\t8168\t8168\t8447\t?\tPASS\trecord");
        assert_eq!(
            RESULTS_HEADER.split('\t').count(),
            r.tsv().split('\t').count()
        );
    }
}
