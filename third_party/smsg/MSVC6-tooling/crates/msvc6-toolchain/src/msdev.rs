use crate::config::{BuildSpec, Config, ImageSpec, MsdevStep};
use crate::confine::{Root, dos_rel, rel_join, rel_path};
use crate::layout::{Toolchain, resolve_ci};
use crate::prefix::{copy_tree, remove_any};
use crate::runner::{Runner, Session};
use crate::stage::Stager;
use anyhow::{Context as _, Result, bail};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

pub const DEFAULT_TIMEOUT: Duration = Duration::from_secs(3600);

pub fn glob_match(pattern: &str, path: &str) -> bool {
    fn seg(p: &[u8], s: &[u8]) -> bool {
        match (p.split_first(), s.split_first()) {
            (None, None) => true,
            (Some((b'*', rest)), _) => {
                seg(rest, s) || s.split_first().is_some_and(|(_, t)| seg(p, t))
            }
            (Some((b'?', rest)), Some((_, t))) => seg(rest, t),
            (Some((a, rest)), Some((b, t))) => a.eq_ignore_ascii_case(b) && seg(rest, t),
            _ => false,
        }
    }
    fn parts(p: &[&str], s: &[&str]) -> bool {
        match (p.split_first(), s.split_first()) {
            (None, None) => true,
            (Some((&"**", rest)), _) => {
                parts(rest, s) || s.split_first().is_some_and(|(_, t)| parts(p, t))
            }
            (Some((a, rest)), Some((b, t))) => seg(a.as_bytes(), b.as_bytes()) && parts(rest, t),
            _ => false,
        }
    }
    let p: Vec<&str> = pattern.split('/').collect();
    let s: Vec<&str> = path.split('/').collect();
    parts(&p, &s)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StepLog {
    pub step: usize,
    pub log: PathBuf,
    pub errors: usize,
    pub warnings: usize,
    pub unresolved: usize,
    pub summary: String,
}

#[derive(Debug, Clone)]
pub struct Report {
    pub image: String,
    pub output: PathBuf,
    pub md5: String,
    pub size: u64,
    pub crt_line: String,
    pub crt_ok: bool,
    pub tools: Vec<String>,
    pub steps: Vec<StepLog>,
    pub post: Vec<String>,
    pub staged_files: u64,
    pub seconds: f64,
}

pub fn parse_log(text: &str) -> (usize, usize, usize, String) {
    let mut errors = 0;
    let mut warnings = 0;
    let mut summary = String::new();
    for l in text.lines() {
        if let Some((name, rest)) = l.split_once(" - ")
            && let Some((e, w)) = rest.split_once(" error(s), ")
            && let (Ok(e), Some(w)) = (e.trim().parse::<usize>(), w.split_whitespace().next())
        {
            errors += e;
            warnings += w.parse::<usize>().unwrap_or(0);
            summary = format!("{} - {}", name.trim(), rest.trim());
        }
    }
    let unresolved = text
        .lines()
        .filter(|l| l.contains("LNK2001") || l.contains("LNK2019"))
        .count();
    let fatal = text
        .lines()
        .filter(|l| {
            l.contains("Tool execution canceled")
                || l.contains("error : Internal error")
                || l.contains("Error executing")
        })
        .count();
    (errors + fatal, warnings, unresolved, summary)
}

pub fn win_entry(cfg: &Config, raw: &str) -> Result<String> {
    let s = cfg.expand(raw)?;
    Ok(if s.starts_with('/') {
        crate::layout::to_win(Path::new(&s))
    } else {
        s
    })
}

pub fn step_batch(
    st: &MsdevStep,
    i: usize,
    dir: &str,
    cfg: &Config,
    link_env: Option<&str>,
) -> Result<String> {
    let mut s = format!("@echo off\r\ncd /d {dir}\r\n");
    let list = |v: &[String]| -> Result<String> {
        Ok(v.iter()
            .map(|x| win_entry(cfg, x))
            .collect::<Result<Vec<_>>>()?
            .join(";"))
    };
    if !st.path.is_empty() {
        s.push_str(&format!("set PATH={};%PATH%\r\n", list(&st.path)?));
    }
    if !st.include.is_empty() {
        s.push_str(&format!("set INCLUDE={}\r\n", list(&st.include)?));
    }
    if !st.lib.is_empty() {
        s.push_str(&format!("set LIB={}\r\n", list(&st.lib)?));
    }
    if let Some(l) = link_env {
        if l.chars()
            .any(|c| matches!(c, '&' | '|' | '<' | '>' | '^' | '%' | '"' | '\r' | '\n'))
        {
            bail!(
                "--link-env `{l}`: a batch file cannot set it as written (& | < > ^ % \" or a line break)"
            );
        }
        s.push_str(&format!("set LINK={l}\r\n"));
    }
    s.push_str(&format!(
        "MSDEV.EXE {} /MAKE \"{}\"{} /USEENV /OUT {}\r\n",
        st.open,
        st.config,
        if st.rebuild { " /REBUILD" } else { "" },
        step_log(st, i)
    ));
    s.push_str(&format!("echo MSVC6T_STEP {i} EXIT=%ERRORLEVEL%\r\n"));
    Ok(s)
}

fn step_log(st: &MsdevStep, i: usize) -> String {
    st.log
        .clone()
        .unwrap_or_else(|| format!("msvc6t-build-{i}.log"))
}

#[derive(Debug, Clone, Default)]
pub struct Opts {
    pub out: PathBuf,
    pub keep_staging: bool,
    pub timeout: Option<Duration>,
    pub link_env: Option<String>,
}

fn toolchain_for(cfg: &Config, image: &ImageSpec, b: &BuildSpec) -> Result<Toolchain> {
    let id = b
        .toolchain
        .as_deref()
        .or(image.compiler.as_deref())
        .with_context(|| format!("image {}: no build toolchain and no `compiler`", image.id))?;
    Toolchain::named(cfg, id)
}

pub fn step_toolchain(used: &[Toolchain], build: &Toolchain, s: &MsdevStep) -> Result<Toolchain> {
    match &s.toolchain {
        None => Ok(build.clone()),
        Some(id) => used
            .iter()
            .find(|t| t.id == *id)
            .cloned()
            .with_context(|| format!("msdev step {}: toolchain {id} is not loaded", s.config)),
    }
}

pub fn implibs(
    sess: &Session<'_>,
    cfg: &Config,
    image: &ImageSpec,
    b: &BuildSpec,
    st: &mut Stager<'_>,
    work: &Path,
    configured: &[Toolchain],
) -> Result<Vec<String>> {
    let mut lines = Vec::new();
    let runtime = runtime_dirs(cfg, &st.root, [], &sess.runner.winepath)?;
    for (i, spec) in b.implib.iter().enumerate() {
        let tc = match &spec.toolchain {
            Some(t) => Toolchain::named(cfg, t)?,
            None => toolchain_for(cfg, image, b)?,
        };
        let def_path = cfg.path(&spec.def)?;
        let def_text = if def_path
            .extension()
            .is_some_and(|e| e.eq_ignore_ascii_case("def"))
        {
            msvc6_base::srcio::read(&def_path)
                .with_context(|| format!("reading {}", def_path.display()))?
        } else {
            let name = def_path
                .file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_default();
            crate::implib::def_from_dll(&name, &std::fs::read(&def_path)?, false)?
        };
        let library = crate::implib::def_library(&def_text).unwrap_or_default();
        let hints =
            match spec.hints_from.as_deref() {
                None => Default::default(),
                Some(h) => {
                    let p =
                        if h == "reference" {
                            cfg.path(image.reference.as_deref().context(
                                "hints_from = \"reference\" needs the image's reference",
                            )?)?
                        } else {
                            cfg.path(h)?
                        };
                    crate::implib::reference_hints(&std::fs::read(&p)?, &library)?
                }
            };
        let rel = dos_rel(&spec.out)?;
        let out = st.claim_output(&rel)?;
        let exp = rel.with_extension("exp");
        st.claim_output(&exp)?;
        let built = crate::implib::build(
            sess,
            &tc,
            &def_text,
            &work.join(format!("implib-{i}")),
            &out,
            &crate::implib::Opts {
                link50compat: spec.link50compat,
                verbatim_symbols: spec.verbatim_symbols,
                import_name: spec.import_name,
                hints,
                lib_args: Vec::new(),
            },
        )?;
        let ran = verify_ran(
            &built.trace,
            &st.root,
            &Declared {
                toolchain: &tc,
                runtime: &runtime,
                configured,
            },
        )
        .with_context(|| format!("implib {}", spec.out))?;
        lines.push(format!(
            "implib {} ran on {}: {}",
            spec.out,
            tc.id,
            ran.join(", ")
        ));
        lines.push(format!(
            "implib {}: {} export(s) -> {} ({} member(s), {} symbol(s), {} long, {} renamed, {} hinted) with {}'s LIB",
            built.library,
            built.entries,
            spec.out,
            built.members,
            built.symbols.len(),
            built.long,
            built.renamed,
            built.hinted,
            tc.id
        ));
    }
    Ok(lines)
}

fn fresh_dir(p: &Path) -> Result<()> {
    remove_any(p)?;
    std::fs::create_dir(p).with_context(|| format!("creating {}", p.display()))
}

fn out_dir(p: &Path) -> Result<()> {
    std::fs::create_dir_all(p).with_context(|| format!("creating {}", p.display()))?;
    if std::fs::symlink_metadata(p).is_ok_and(|m| m.file_type().is_symlink()) {
        bail!(
            "{} is a symbolic link: name the output directory itself",
            p.display()
        );
    }
    Ok(())
}

pub fn check_expectations(tcs: &[Toolchain]) -> Result<Vec<String>> {
    let mut lines = Vec::new();
    let mut bad = Vec::new();
    for tc in tcs {
        for k in crate::version::unknown_expectations(tc) {
            bad.push(format!(
                "toolchain {}: expect names `{k}`, which is no tool role",
                tc.id
            ));
        }
        for t in crate::version::detect(tc, &crate::version::ImageReader) {
            let Some(want) = &t.expected else {
                continue;
            };
            match (t.verdict(), t.version) {
                (Some(true), Some(v)) => lines.push(format!("{} {} {v} ok", tc.id, t.file)),
                (_, Some(v)) => bad.push(format!(
                    "toolchain {}: {} is {v}, MISMATCH against expect {want}",
                    tc.id, t.file
                )),
                (_, None) => bad.push(format!(
                    "toolchain {}: {} {}, MISMATCH against expect {want}",
                    tc.id,
                    t.file,
                    if t.path.is_some() {
                        "has no version resource"
                    } else {
                        "is absent"
                    }
                )),
            }
        }
    }
    if !bad.is_empty() {
        bail!(
            "the build's toolchains are not the builds they declare:\n  {}",
            bad.join("\n  ")
        );
    }
    Ok(lines)
}

fn host_of(win: &str, root: &Root) -> Option<PathBuf> {
    let unix = win.replace('\\', "/");
    if let Some(rest) = unix.strip_prefix("Z:").or_else(|| unix.strip_prefix("z:")) {
        return Some(PathBuf::from(rest));
    }
    dos_rel(win).ok().map(|r| root.path(&r))
}

#[derive(Debug, Clone, Copy)]
pub struct Declared<'a> {
    pub toolchain: &'a Toolchain,
    pub runtime: &'a [PathBuf],
    pub configured: &'a [Toolchain],
}

fn ships(tc: &Toolchain, name: &str) -> bool {
    std::iter::once(&tc.bin)
        .chain(tc.common.iter())
        .any(|d| resolve_ci(&d.join(name)).is_some())
}

pub fn runtime_dirs<'a>(
    cfg: &Config,
    root: &Root,
    entries: impl IntoIterator<Item = &'a String>,
    winepath: &[String],
) -> Result<Vec<PathBuf>> {
    let mut wins = entries
        .into_iter()
        .map(|e| win_entry(cfg, e))
        .collect::<Result<Vec<_>>>()?;
    wins.extend(winepath.iter().cloned());
    let mut out = Vec::new();
    for w in wins {
        if let Some(p) = host_of(&w, root)
            .and_then(|h| resolve_ci(&h))
            .and_then(|h| std::fs::canonicalize(h).ok())
            && !out.contains(&p)
        {
            out.push(p);
        }
    }
    Ok(out)
}

fn homes(tc: &Toolchain) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = Vec::new();
    for d in std::iter::once(&tc.root)
        .chain(std::iter::once(&tc.bin))
        .chain(tc.common.iter())
    {
        if let Ok(p) = std::fs::canonicalize(d)
            && !out.contains(&p)
        {
            out.push(p);
        }
    }
    out
}

pub fn verify_ran(log: &str, root: &Root, d: &Declared<'_>) -> Result<Vec<String>> {
    let tc = d.toolchain;
    std::fs::canonicalize(&tc.root)
        .with_context(|| format!("toolchain {}: {}", tc.id, tc.root.display()))?;
    let mine = homes(tc);
    let windows = std::fs::canonicalize(root.root.join("windows")).ok();
    let others: Vec<(Vec<PathBuf>, &Toolchain)> = d
        .configured
        .iter()
        .filter(|o| o.id != tc.id)
        .map(|o| (homes(o), o))
        .filter(|(h, _)| !h.is_empty())
        .collect();
    let mut seen = Vec::new();
    let mut bad = Vec::new();
    for m in crate::crtguard::native_modules(log) {
        let base = m
            .rsplit(['\\', '/'])
            .next()
            .unwrap_or(&m)
            .to_ascii_uppercase();
        let Some(found) = host_of(&m, root).and_then(|h| resolve_ci(&h)) else {
            bad.push(format!(
                "{m}: the module that ran cannot be found on this host"
            ));
            continue;
        };
        let Ok(real) = std::fs::canonicalize(&found) else {
            bad.push(format!("{m}: {} does not resolve", found.display()));
            continue;
        };
        if base == "MSVCRT.DLL" && windows.as_ref().is_some_and(|w| real.starts_with(w)) {
            continue;
        }
        let version = std::fs::read(&real).ok().and_then(|b| {
            crate::version::VersionReader::file_version(&crate::version::ImageReader, &b)
        });
        let shown = version.map_or_else(|| "no version".to_string(), |v| v.to_string());
        if mine.iter().any(|h| real.starts_with(h)) {
            if let Some((role, _, _)) = crate::layout::ROLES.iter().find(|(_, f, _)| *f == base)
                && let Some(want) = tc.expect.get(*role)
                && version.map(|v| v.to_string()).as_deref() != Some(want.as_str())
            {
                bad.push(format!(
                    "{m} ran from toolchain {} as {shown}, MISMATCH against expect {want}",
                    tc.id
                ));
                continue;
            }
            let line = format!("{base} {shown}");
            if !seen.contains(&line) {
                seen.push(line);
            }
            continue;
        }
        if let Some((_, o)) = others
            .iter()
            .find(|(h, _)| h.iter().any(|r| real.starts_with(r)))
        {
            bad.push(format!(
                "{m} ({}): it lies in toolchain {}, and this step declares {}",
                real.display(),
                o.id,
                tc.id
            ));
            continue;
        }
        let dir = found.parent().and_then(|p| std::fs::canonicalize(p).ok());
        let runtime = dir.as_ref().filter(|p| d.runtime.contains(p));
        let program = base.ends_with(".EXE");
        let shipper = d.configured.iter().find(|o| ships(o, &base));
        if let Some(r) = runtime
            && !program
            && shipper.is_none()
        {
            let line = format!("{base} {shown} (runtime {})", r.display());
            if !seen.contains(&line) {
                seen.push(line);
            }
            continue;
        }
        let why = if runtime.is_some() && program {
            format!(
                "a program from a runtime directory; programs come from the declared toolchain {}",
                tc.id
            )
        } else if let (Some(_), Some(o)) = (runtime, shipper) {
            format!(
                "toolchain {} ships {base}, so it comes from the declared toolchain {}, not a runtime directory",
                o.id, tc.id
            )
        } else {
            format!(
                "it lies outside the declared toolchain {} ({}) and every runtime directory",
                tc.id,
                mine.iter()
                    .map(|h| h.display().to_string())
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        };
        bad.push(format!("{m} ({}): {why}", real.display()));
    }
    if !bad.is_empty() {
        bail!(
            "the modules that ran are not the declared toolchain's:\n  {}",
            bad.join("\n  ")
        );
    }
    Ok(seen)
}

#[derive(Debug, Clone, Copy)]
pub struct Step<'a> {
    pub at: usize,
    pub config: &'a str,
    pub errors: usize,
    pub summary: &'a str,
    pub log: &'a Path,
}

pub fn step_verdict(s: &Step<'_>, ran: Result<Vec<String>>) -> Result<Vec<String>> {
    let i = s.at;
    match ran {
        Err(e) if s.errors > 0 => Err(e.context(format!(
            "msdev step {i} ({}), which also failed: {} ({} error line(s)); see {}",
            s.config,
            s.summary,
            s.errors,
            s.log.display()
        ))),
        Err(e) => Err(e.context(format!("msdev step {i} ({})", s.config))),
        Ok(_) if s.errors > 0 => bail!(
            "MSDEV step {i} failed: {} ({} error line(s)); see {}",
            s.summary,
            s.errors,
            s.log.display()
        ),
        Ok(ran) => Ok(ran),
    }
}

pub fn build(cfg: &Config, runner: &Runner, image: &ImageSpec, opts: &Opts) -> Result<Report> {
    let started = Instant::now();
    let b = image
        .build
        .as_ref()
        .with_context(|| format!("image {} has no [image.build]", image.id))?;
    if b.msdev.is_empty() {
        bail!("image {}: [image.build] lists no msdev step", image.id);
    }
    let tc = toolchain_for(cfg, image, b)?;
    let mut used = vec![tc.clone()];
    for id in b
        .msdev
        .iter()
        .filter_map(|s| s.toolchain.as_deref())
        .chain(b.implib.iter().filter_map(|i| i.toolchain.as_deref()))
    {
        if !used.iter().any(|t| t.id == id) {
            used.push(Toolchain::named(cfg, id)?);
        }
    }
    check_expectations(&used)?;
    let sess = runner.session()?;
    let lease = sess
        .lease
        .as_ref()
        .context("an MSDEV build needs a wine prefix: [runner] kind is native")?;
    let prefix = lease.path.clone();
    let manifest = lease.pool.join(format!("wp{}.staged", lease.slot));
    let root = Root::new(&prefix.join("drive_c"))?;
    let earlier = crate::stage::clean(&root, &manifest)?;
    for s in &earlier.skipped {
        eprintln!("note: {s}");
    }
    let mut st = Stager::new(root.clone(), cfg, manifest.clone());
    for m in &b.mount {
        st.mount(&m.at, &m.from)?;
    }
    let mut staged_files = 0;
    for s in &b.stage {
        staged_files += st.stage(s)?;
    }
    let work = prefix.join("msvc6t-work");
    fresh_dir(&work)?;
    let configured: Vec<Toolchain> = cfg
        .toolchains
        .iter()
        .filter_map(|(id, s)| Toolchain::from_spec(id, s, cfg).ok())
        .collect();
    let mut post = implibs(&sess, cfg, image, b, &mut st, &work, &configured)?;
    runner.ensure_display()?;
    let first = b.msdev.first().context("no msdev step")?;
    let first_rel = dos_rel(&first.dir)?;
    if !st.in_staged_dir(&first_rel) {
        bail!(
            "msdev step 0's dir {} is not a directory this build staged",
            first.dir
        );
    }
    let bat_dir = root.check(&first_rel, false)?;
    let join = |v: &[String]| -> Result<String> {
        Ok(v.iter()
            .map(|x| win_entry(cfg, x))
            .collect::<Result<Vec<_>>>()?
            .join(";"))
    };
    let mut env = crate::runner::tool_env(&tc, &runner.winepath);
    let mut set = |k: &str, v: String| {
        env.retain(|(x, _)| x != k);
        env.push((k.to_string(), v));
    };
    if !b.include.is_empty() {
        set("INCLUDE", join(&b.include)?);
    }
    if !b.lib.is_empty() {
        set("LIB", join(&b.lib)?);
    }
    if !b.path.is_empty() {
        set("WINEPATH", join(&b.path)?);
    }
    set("WINEDEBUG", "-all,+loaddll".to_string());
    out_dir(&opts.out)?;
    let deadline = started + opts.timeout.unwrap_or(DEFAULT_TIMEOUT);
    let mut stderr = String::new();
    let mut stdout = String::new();
    let mut steps = Vec::new();
    let mut tools = Vec::new();
    let mut archives: Vec<PathBuf> = Vec::new();
    let mut dir = first_rel.clone();
    let mut dir_win = first.dir.clone();
    for (i, s) in b.msdev.iter().enumerate() {
        if !s.dir.is_empty() {
            dir = dos_rel(&s.dir)?;
            dir_win.clone_from(&s.dir);
        }
        let step_tc = step_toolchain(&used, &tc, s)?;
        let bat = format!("msvc6t-step{i}.bat");
        st.write_text(
            &first_rel.join(&bat),
            &step_batch(s, i, &dir_win, cfg, opts.link_env.as_deref())?,
        )?;
        let run = sess.run(
            Path::new("cmd"),
            &["/c".to_string(), bat],
            Some(&bat_dir),
            &env,
            Some(deadline.saturating_duration_since(Instant::now())),
        )?;
        let trace = msvc6_base::srcio::tool_text(&run.stderr);
        stderr.push_str(&trace);
        stdout.push_str(&msvc6_base::srcio::tool_text(&run.stdout));
        msvc6_base::srcio::own_write(opts.out.join("loaddll.log"), &stderr)?;
        msvc6_base::srcio::own_write(opts.out.join("build.out"), &stdout)?;
        if run.timed_out {
            bail!(
                "the MSDEV build of {} timed out in step {i}; its prefix was reaped",
                image.id
            );
        }
        let log = root.check(&dir.join(rel_path(&step_log(s, i))?), false)?;
        let text = std::fs::read(&log)
            .map(|b| msvc6_base::srcio::tool_text(&b))
            .with_context(|| {
                format!(
                    "MSDEV wrote no log {} for step {i}: it did not run to completion",
                    log.display()
                )
            })?;
        let keep = opts.out.join(format!(
            "step{i}-{}",
            step_log(s, i).replace(['\\', '/'], "_")
        ));
        msvc6_base::srcio::own_write(&keep, &text)?;
        let (errors, warnings, unresolved, summary) = parse_log(&text);
        let runtime = runtime_dirs(cfg, &root, b.path.iter().chain(&s.path), &runner.winepath)?;
        let ran = step_verdict(
            &Step {
                at: i,
                config: &s.config,
                errors,
                summary: &summary,
                log: &keep,
            },
            verify_ran(
                &trace,
                &root,
                &Declared {
                    toolchain: &step_tc,
                    runtime: &runtime,
                    configured: &configured,
                },
            ),
        )?;
        steps.push(StepLog {
            step: i,
            log: keep,
            errors,
            warnings,
            unresolved,
            summary,
        });
        for r in &s.requires {
            let p = root.check(&rel_join(&dir, r)?, false)?;
            match resolve_ci(&p) {
                Some(found) => archives.push(found),
                None => bail!("step {i} ({}) built no {r}", s.config),
            }
        }
        tools.push(format!("step {i} on {}: {}", step_tc.id, ran.join(", ")));
    }
    let (crt_line, crt_ok) = crate::crtguard::line(&stderr, &b.require_crt);
    if runner.msvcrt.is_some() && !crt_ok {
        bail!("{crt_line}");
    }
    let out_rel = dos_rel(&b.output)?;
    let produced = resolve_ci(&root.check(&out_rel, false)?)
        .with_context(|| format!("MSDEV produced no {}", b.output))?;
    let produced_rel = produced
        .strip_prefix(&root.root)
        .context("the output resolved outside drive_c")?
        .to_path_buf();
    root.check(&produced_rel, false)?;
    let name = produced
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_default();
    if let Some(d) = produced.parent() {
        let keep = opts.out.join(d.file_name().unwrap_or_default());
        remove_any(&keep)?;
        copy_tree(d, &keep, false)?;
    }
    for spec in &b.implib {
        if let Some(found) = resolve_ci(&root.check(&dos_rel(&spec.out)?, false)?) {
            archives.push(found);
        }
    }
    let mut kept: Vec<String> = Vec::new();
    for a in &archives {
        let n = a
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default();
        if n.is_empty()
            || n.eq_ignore_ascii_case(&name)
            || kept.iter().any(|k| k.eq_ignore_ascii_case(&n))
        {
            continue;
        }
        let dest = opts.out.join(&n);
        remove_any(&dest)?;
        std::fs::copy(a, &dest).with_context(|| format!("copying {}", a.display()))?;
        kept.push(n);
    }
    if !kept.is_empty() {
        post.push(format!(
            "kept the libraries the link read beside the output: {}",
            kept.join(", ")
        ));
    }
    let final_path = opts.out.join(&name);
    remove_any(&final_path)?;
    std::fs::copy(&produced, &final_path)?;
    post.extend(apply_post(cfg, image, b, &final_path)?);
    let bytes = std::fs::read(&final_path)?;
    if !opts.keep_staging {
        let c = crate::stage::clean(&root, &manifest)?;
        for s in &c.skipped {
            eprintln!("note: {s}");
        }
        remove_any(&work)?;
    }
    Ok(Report {
        image: image.id.clone(),
        md5: msvc6_base::digest::md5_hex(&bytes),
        size: u64::try_from(bytes.len()).unwrap_or(u64::MAX),
        output: final_path,
        crt_line,
        crt_ok,
        tools,
        steps,
        post,
        staged_files,
        seconds: started.elapsed().as_secs_f64(),
    })
}

pub fn apply_post(
    cfg: &Config,
    image: &ImageSpec,
    b: &BuildSpec,
    out: &Path,
) -> Result<Vec<String>> {
    let mut lines = Vec::new();
    let reference = image
        .reference
        .as_deref()
        .map(|r| cfg.path(r))
        .transpose()?;
    for p in &b.post {
        let subst = |a: &String| -> Result<String> {
            let a = a.replace("{image}", &out.display().to_string()).replace(
                "{reference}",
                &reference
                    .as_ref()
                    .map(|r| r.display().to_string())
                    .unwrap_or_default(),
            );
            cfg.expand(&a)
        };
        let argv: Vec<String> = p.run.iter().map(subst).collect::<Result<_>>()?;
        let (prog, rest) = argv.split_first().context("an empty post `run`")?;
        let mut c = crate::exec::Cmd::new(prog).args(rest);
        if let Some(d) = &p.cwd {
            c = c.cwd(cfg.path(d)?);
        }
        let o = crate::exec::run_ok(&c)?;
        lines.push(format!("post: {} (exit 0)", argv.join(" ")));
        lines.extend(o.text().lines().take(8).map(|l| format!("    {l}")));
    }
    if !image.clock.is_empty() {
        let r = reference
            .as_ref()
            .with_context(|| format!("image {}: its clock fields need `reference`", image.id))?;
        lines.extend(crate::clock::copy_file(out, r, &image.clock)?);
    }
    Ok(lines)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn globs_match_one_component_or_any_depth() {
        assert!(glob_match("src/*.cpp", "src/A.cpp"));
        assert!(!glob_match("src/*.cpp", "src/sub/A.cpp"));
        assert!(glob_match("include/**", "include/a/b.h"));
        assert!(glob_match("x.DSP", "x.dsp"));
        assert!(!glob_match("src/*.cpp", "res/a.cpp"));
    }

    #[test]
    fn each_step_runs_alone_on_its_own_toolchain_and_environment() {
        let cfg = crate::config::test_config("", None);
        let step = |open: &str, path: Vec<String>, toolchain: Option<&str>| MsdevStep {
            dir: String::new(),
            open: open.into(),
            config: "p - Win32 Release".into(),
            toolchain: toolchain.map(str::to_string),
            include: Vec::new(),
            lib: Vec::new(),
            path,
            rebuild: true,
            log: Some("build.log".into()),
            requires: Vec::new(),
        };
        let lpc = step("..\\l\\l.dsp", vec!["C:\\t\\BIN".into()], Some("old"));
        let main = step("a.dsp", Vec::new(), None);
        assert_eq!(
            step_batch(&lpc, 0, "C:\\a", &cfg, None).unwrap(),
            "@echo off\r\ncd /d C:\\a\r\nset PATH=C:\\t\\BIN;%PATH%\r\nMSDEV.EXE ..\\l\\l.dsp /MAKE \
             \"p - Win32 Release\" /REBUILD /USEENV /OUT build.log\r\necho MSVC6T_STEP 0 EXIT=%ERRORLEVEL%\r\n"
        );
        assert_eq!(
            step_batch(&main, 1, "C:\\a", &cfg, None).unwrap(),
            "@echo off\r\ncd /d C:\\a\r\nMSDEV.EXE a.dsp /MAKE \"p - Win32 Release\" /REBUILD /USEENV /OUT \
             build.log\r\necho MSVC6T_STEP 1 EXIT=%ERRORLEVEL%\r\n"
        );
        assert_eq!(
            step_batch(&main, 1, "C:\\a", &cfg, Some("/VERBOSE /MAP")).unwrap(),
            "@echo off\r\ncd /d C:\\a\r\nset LINK=/VERBOSE /MAP\r\nMSDEV.EXE a.dsp /MAKE \"p - Win32 Release\" \
             /REBUILD /USEENV /OUT build.log\r\necho MSVC6T_STEP 1 EXIT=%ERRORLEVEL%\r\n"
        );
        assert!(step_batch(&main, 1, "C:\\a", &cfg, Some("/VERBOSE & del x")).is_err());
        let tc = |id: &str| Toolchain {
            id: id.into(),
            root: PathBuf::from("/r").join(id),
            bin: PathBuf::from("/r").join(id).join("VC98/BIN"),
            common: None,
            lib: None,
            include: None,
            expect: Default::default(),
        };
        let used = [tc("new"), tc("old")];
        assert_eq!(step_toolchain(&used, &used[0], &lpc).unwrap().id, "old");
        assert_eq!(step_toolchain(&used, &used[0], &main).unwrap().id, "new");
        assert!(step_toolchain(&used[..1], &used[0], &lpc).is_err());
    }

    #[test]
    fn the_log_summary_counts_errors_and_unresolved_names() {
        let (e, w, u, s) = parse_log(
            "Linking...\nx.obj : error LNK2001: unresolved external symbol _f\n\
                                     a.dll - 1 error(s), 3 warning(s)\n",
        );
        assert_eq!((e, w, u), (1, 3, 1));
        assert_eq!(s, "a.dll - 1 error(s), 3 warning(s)");
        assert_eq!(parse_log("Tool execution canceled by user.\n").0, 1);
    }

    struct Scratch(PathBuf);

    impl Drop for Scratch {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn fake_tree(root: &Path, files: &[(&str, u16, u16)]) {
        std::fs::create_dir_all(root.join("VC98/BIN")).unwrap();
        for (f, major, build) in files {
            msvc6_base::srcio::write_bytes(
                root.join("VC98/BIN").join(f),
                &msvc6_fixture::sample::versioned(*major, 0, *build),
            )
            .unwrap();
        }
    }

    #[test]
    fn a_build_on_a_toolchain_of_another_build_fails_before_it_runs() {
        let d = std::env::temp_dir().join(format!("msvc6t-expect-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let s = Scratch(d.clone());
        fake_tree(
            &s.0.join("sp5"),
            &[("C2.DLL", 12, 8966), ("CL.EXE", 12, 8804)],
        );
        let text = format!(
            "[toolchain.t]\nroot = '{}'\nexpect = {{ c2 = \"12.00.8168\" }}\n[runner]\nkind = \"native\"\n\
             [[image]]\nid = \"X\"\ncompiler = \"t\"\n[image.build]\nmsdev = [{{ dir = 'C:\\a', open = \"a.dsp\", \
             config = \"a - Win32 Release\" }}]\noutput = 'C:\\a\\a.exe'\n",
            s.0.join("sp5").display()
        );
        let cfg = crate::config::test_config(&text, None);
        let runner = Runner::from_config(&cfg).unwrap();
        let e = build(
            &cfg,
            &runner,
            cfg.image("X").unwrap(),
            &Opts {
                out: s.0.join("out"),
                ..Opts::default()
            },
        )
        .unwrap_err();
        let e = format!("{e:#}");
        assert!(
            e.contains("C2.DLL is 12.00.8966, MISMATCH against expect 12.00.8168"),
            "{e}"
        );
    }

    struct Ran {
        dir: Scratch,
        root: Root,
        tcs: Vec<Toolchain>,
        runtime: Vec<PathBuf>,
    }

    fn ran_fixture(tag: &str) -> Ran {
        let d = std::env::temp_dir().join(format!("msvc6t-ran-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let s = Scratch(d.clone());
        fake_tree(
            &d.join("good"),
            &[
                ("CL.EXE", 12, 8168),
                ("C2.DLL", 12, 8168),
                ("RC.EXE", 5, 2195),
            ],
        );
        fake_tree(&d.join("other"), &[("CL.EXE", 12, 8168)]);
        fake_tree(&d.join("sp5"), &[("CL.EXE", 12, 8966)]);
        fake_tree(
            &d.join("rogue"),
            &[("CL.EXE", 12, 8168), ("RC.EXE", 5, 2195)],
        );
        fake_tree(
            &d.join("rt"),
            &[
                ("MFC42.DLL", 6, 8447),
                ("RC.EXE", 5, 2195),
                ("C2.DLL", 12, 8168),
            ],
        );
        let drive_c = d.join("wp0/drive_c");
        std::fs::create_dir_all(drive_c.join("windows/system32")).unwrap();
        for f in ["msvcrt.dll", "mfc42.dll"] {
            msvc6_base::srcio::write_bytes(
                drive_c.join("windows/system32").join(f),
                &msvc6_fixture::sample::versioned(6, 0, 8797),
            )
            .unwrap();
        }
        std::os::unix::fs::symlink(d.join("rogue"), drive_c.join("x")).unwrap();
        std::os::unix::fs::symlink(d.join("good"), drive_c.join("g")).unwrap();
        let text = format!(
            "[toolchain.good]\nroot = '{}'\nexpect = {{ cl = \"12.00.8168\" }}\n[toolchain.other]\nroot = '{}'\n\
             expect = {{ cl = \"12.00.8168\" }}\n[toolchain.sp5]\nroot = '{}'\nexpect = {{ cl = \"12.00.8168\" }}\n",
            d.join("good").display(),
            d.join("other").display(),
            d.join("sp5").display()
        );
        let cfg = crate::config::test_config(&text, None);
        Ran {
            root: Root::new(&drive_c).unwrap(),
            tcs: Toolchain::all(&cfg).unwrap(),
            runtime: vec![std::fs::canonicalize(d.join("rt/VC98/BIN")).unwrap()],
            dir: s,
        }
    }

    fn loaded(p: &str, how: &str) -> String {
        format!(
            "0040:trace:loaddll:build_module Loaded L\"{}\" at 00400000: {how}\n",
            p.replace('\\', "\\\\")
        )
    }

    fn host(r: &Ran, rel: &str) -> String {
        format!("Z:{}", r.dir.0.join(rel).display()).replace('/', "\\")
    }

    fn ran(r: &Ran, declared: &str, log: &str) -> Result<Vec<String>> {
        let tc = r.tcs.iter().find(|t| t.id == declared).unwrap();
        verify_ran(
            log,
            &r.root,
            &Declared {
                toolchain: tc,
                runtime: &r.runtime,
                configured: &r.tcs,
            },
        )
    }

    #[test]
    fn a_tool_from_another_configured_toolchain_fails_even_at_the_expected_version() {
        let r = ran_fixture("tc");
        let good = loaded("C:\\g\\VC98\\BIN\\CL.EXE", "native");
        assert_eq!(
            ran(&r, "good", &good).unwrap(),
            vec!["CL.EXE 12.00.8168".to_string()]
        );
        let other = loaded(&host(&r, "other/VC98/BIN/CL.EXE"), "native");
        assert_eq!(
            ran(&r, "other", &other).unwrap(),
            vec!["CL.EXE 12.00.8168".to_string()]
        );
        let e = ran(&r, "good", &other).unwrap_err().to_string();
        assert!(
            e.contains("it lies in toolchain other, and this step declares good"),
            "{e}"
        );
        let sp5 = loaded(&host(&r, "sp5/VC98/BIN/CL.EXE"), "native");
        let e = ran(&r, "sp5", &sp5).unwrap_err().to_string();
        assert!(e.contains("MISMATCH against expect 12.00.8168"), "{e}");
    }

    #[test]
    fn every_native_module_is_checked_against_the_declared_toolchain() {
        let r = ran_fixture("all");
        let e = ran(&r, "good", &loaded("C:\\x\\VC98\\BIN\\RC.EXE", "native"))
            .unwrap_err()
            .to_string();
        assert!(e.contains("outside the declared toolchain good"), "{e}");
        let rc = ran(&r, "good", &loaded("C:\\g\\VC98\\BIN\\RC.EXE", "native")).unwrap();
        assert!(rc.len() == 1 && rc[0].starts_with("RC.EXE "), "{rc:?}");
        let mfc = ran(
            &r,
            "good",
            &loaded(&host(&r, "rt/VC98/BIN/MFC42.DLL"), "native"),
        )
        .unwrap();
        assert!(
            mfc.len() == 1 && mfc[0].starts_with("MFC42.DLL ") && mfc[0].contains("(runtime "),
            "{mfc:?}"
        );
        let e = ran(
            &r,
            "good",
            &loaded(&host(&r, "rt/VC98/BIN/RC.EXE"), "native"),
        )
        .unwrap_err()
        .to_string();
        assert!(e.contains("a program from a runtime directory"), "{e}");
        let e = ran(
            &r,
            "good",
            &loaded(&host(&r, "rt/VC98/BIN/C2.DLL"), "native"),
        )
        .unwrap_err()
        .to_string();
        assert!(e.contains("toolchain good ships C2.DLL"), "{e}");
        let e = ran(
            &r,
            "good",
            &loaded("C:\\windows\\system32\\mfc42.dll", "native"),
        )
        .unwrap_err()
        .to_string();
        assert!(e.contains("outside the declared toolchain good"), "{e}");
        let quiet = format!(
            "{}{}",
            loaded("C:\\windows\\system32\\msvcrt.dll", "native"),
            loaded("C:\\x\\VC98\\BIN\\RC.EXE", "builtin")
        );
        assert!(ran(&r, "good", &quiet).unwrap().is_empty());
    }

    #[test]
    fn a_foreign_module_is_named_when_its_msdev_step_fails_too() {
        let r = ran_fixture("failed");
        let step = |errors| Step {
            at: 2,
            config: "a - Win32 Release",
            errors,
            summary: "a.dll - 1 error(s), 0 warning(s)",
            log: Path::new("out/step2-build.log"),
        };
        let foreign = || ran(&r, "good", &loaded("C:\\x\\VC98\\BIN\\RC.EXE", "native"));
        let e = format!("{:#}", step_verdict(&step(1), foreign()).unwrap_err());
        assert!(
            e.contains("msdev step 2 (a - Win32 Release), which also failed: a.dll - 1 error(s)")
                && e.contains("RC.EXE")
                && e.contains("outside the declared toolchain good"),
            "{e}"
        );
        let e = format!("{:#}", step_verdict(&step(0), foreign()).unwrap_err());
        assert!(
            e.starts_with("msdev step 2 (a - Win32 Release): ") && !e.contains("also failed"),
            "{e}"
        );
        let own = || ran(&r, "good", &loaded("C:\\g\\VC98\\BIN\\RC.EXE", "native"));
        let e = format!("{:#}", step_verdict(&step(1), own()).unwrap_err());
        assert!(
            e.starts_with("MSDEV step 2 failed: a.dll - 1 error(s)"),
            "{e}"
        );
        assert_eq!(step_verdict(&step(0), own()).unwrap().len(), 1);
    }

    #[test]
    fn a_linked_vc98_is_its_toolchain_s_and_another_tree_s_runtime_directory_is_not() {
        let d = std::env::temp_dir().join(format!("msvc6t-ran-link-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let s = Scratch(d.clone());
        fake_tree(&d.join("store"), &[("CL.EXE", 12, 8168)]);
        std::fs::create_dir_all(d.join("linked")).unwrap();
        std::os::unix::fs::symlink(d.join("store/VC98"), d.join("linked/VC98")).unwrap();
        fake_tree(&d.join("other"), &[("CL.EXE", 12, 8168)]);
        std::fs::create_dir_all(d.join("other/VC98/REDIST")).unwrap();
        msvc6_base::srcio::write_bytes(
            d.join("other/VC98/REDIST/MFC42.DLL"),
            &msvc6_fixture::sample::versioned(6, 0, 8447),
        )
        .unwrap();
        let drive_c = d.join("wp0/drive_c");
        std::fs::create_dir_all(drive_c.join("windows/system32")).unwrap();
        let text = format!(
            "[toolchain.linked]\nroot = '{}'\nexpect = {{ cl = \"12.00.8168\" }}\n[toolchain.other]\nroot = '{}'\n",
            d.join("linked").display(),
            d.join("other").display()
        );
        let cfg = crate::config::test_config(&text, None);
        let r = Ran {
            root: Root::new(&drive_c).unwrap(),
            tcs: Toolchain::all(&cfg).unwrap(),
            runtime: vec![std::fs::canonicalize(d.join("other/VC98/REDIST")).unwrap()],
            dir: s,
        };
        let cl = loaded(&host(&r, "linked/VC98/BIN/CL.EXE"), "native");
        assert_eq!(
            ran(&r, "linked", &cl).unwrap(),
            vec!["CL.EXE 12.00.8168".to_string()]
        );
        let mfc = loaded(&host(&r, "other/VC98/REDIST/MFC42.DLL"), "native");
        let e = ran(&r, "linked", &mfc).unwrap_err().to_string();
        assert!(
            e.contains("it lies in toolchain other, and this step declares linked"),
            "{e}"
        );
    }

    #[test]
    fn the_post_link_step_copies_the_declared_clock_fields_from_the_reference() {
        let d = std::env::temp_dir().join(format!("msvc6t-post-clock-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        let s = Scratch(d.clone());
        let reference = msvc6_fixture::sample::sample().pe.build();
        let img = msvc6_image::pe::Image::parse(reference.clone()).unwrap();
        let sites = msvc6_image::clock::resolve(&img, &msvc6_image::clock::ALL).unwrap();
        assert_eq!(sites.len(), msvc6_image::clock::ALL.len());
        let mut ours = reference.clone();
        for site in &sites {
            for b in &mut ours[site.off..site.off + 4] {
                *b ^= 0x5a;
            }
        }
        let (rp, op) = (s.0.join("ref.dll"), s.0.join("out.dll"));
        msvc6_base::srcio::write_bytes(&rp, &reference).unwrap();
        let keys: Vec<String> = msvc6_image::clock::ALL
            .iter()
            .map(|f| format!("\"{}\"", f.key()))
            .collect();
        let build = "[image.build]\nmsdev = [{ dir = 'C:\\a', open = \"a.dsp\", config = \"a - Win32 Release\" }]\n\
                     output = 'C:\\a\\a.dll'\n";
        let text = format!(
            "[[image]]\nid = \"X\"\nreference = 'ref.dll'\nclock = [{}]\n{build}[[image]]\nid = \"N\"\nreference = 'ref.dll'\n{build}",
            keys.join(", ")
        );
        let cfg = crate::config::test_config(&text, Some(&s.0.join("msvc6t.toml")));
        let x = cfg.image("X").unwrap();
        msvc6_base::srcio::write_bytes(&op, &ours).unwrap();
        let lines = apply_post(&cfg, x, x.build.as_ref().unwrap(), &op).unwrap();
        assert_eq!(std::fs::read(&op).unwrap(), reference);
        assert_eq!(
            lines.iter().filter(|l| l.starts_with("clock ")).count(),
            sites.len(),
            "{lines:?}"
        );
        assert!(
            lines.iter().all(|l| l.ends_with("(the reference's)")),
            "{lines:?}"
        );
        let again = apply_post(&cfg, x, x.build.as_ref().unwrap(), &op).unwrap();
        assert!(
            again
                .iter()
                .all(|l| l.ends_with("(already the reference's)")),
            "{again:?}"
        );
        let n = cfg.image("N").unwrap();
        msvc6_base::srcio::write_bytes(&op, &ours).unwrap();
        assert!(
            apply_post(&cfg, n, n.build.as_ref().unwrap(), &op)
                .unwrap()
                .is_empty()
        );
        assert_eq!(std::fs::read(&op).unwrap(), ours);
    }
}
