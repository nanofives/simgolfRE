use crate::config::{Loaded, TuSpec};
use crate::lex::{self, Cond, Kind};
use crate::syntax::Syntax;
use anyhow::{Context as _, Result, bail};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Job {
    pub root: PathBuf,
    pub source: String,
    pub cwd: PathBuf,
    pub cflags: Vec<String>,
    pub scratch: PathBuf,
    pub out: PathBuf,
    pub slot: usize,
}

pub trait Compile: Sync {
    fn compile(&self, job: &Job) -> Result<Vec<u8>, String>;
    fn describe(&self) -> String;
    fn isolated(&self) -> bool;
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandCompile {
    pub argv: Vec<String>,
    pub env: Vec<(String, String)>,
}

#[must_use]
pub fn win(p: &Path) -> String {
    let s = p.display().to_string();
    if s.starts_with('/') {
        format!("Z:{}", s.replace('/', "\\"))
    } else {
        s.replace('/', "\\")
    }
}

#[must_use]
pub fn relative(from_dir: &Path, to: &Path) -> PathBuf {
    let a: Vec<_> = from_dir.components().collect();
    let b: Vec<_> = to.components().collect();
    let common = a.iter().zip(&b).take_while(|(x, y)| x == y).count();
    let mut out = PathBuf::new();
    for _ in common..a.len() {
        out.push("..");
    }
    for c in b.iter().skip(common) {
        out.push(c);
    }
    out
}

pub const SCRUBBED_ENV: [&str; 4] = ["CL", "_CL_", "LINK", "_LINK_"];
pub const REGISTRY_ENV: &str = "[Environment]";

#[must_use]
pub fn runs_wine(argv: &[String]) -> bool {
    argv.iter().flat_map(|a| a.split_whitespace()).any(|w| {
        let name = Path::new(w)
            .file_name()
            .map(|n| n.to_string_lossy().to_ascii_lowercase())
            .unwrap_or_default();
        name.starts_with("wine") || (cfg!(not(windows)) && name.ends_with(".exe"))
    })
}

#[must_use]
pub fn registry_env(user_reg: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut inside = false;
    for line in user_reg.lines() {
        let t = line.trim();
        if t.starts_with('[') {
            inside = t
                .get(..REGISTRY_ENV.len())
                .is_some_and(|h| h.eq_ignore_ascii_case(REGISTRY_ENV));
            continue;
        }
        if !inside {
            continue;
        }
        let Some(name) = t.strip_prefix('"').and_then(|r| r.split('"').next()) else {
            continue;
        };
        if SCRUBBED_ENV.iter().any(|s| name.eq_ignore_ascii_case(s)) {
            out.push(name.to_string());
        }
    }
    out
}

fn prefix_env(prefix: &Path) -> Result<(), String> {
    let reg = prefix.join("user.reg");
    let Ok(bytes) = std::fs::read(&reg) else {
        return Ok(());
    };
    let set = registry_env(&msvc6_base::srcio::tool_text(&bytes));
    if set.is_empty() {
        return Ok(());
    }
    Err(format!(
        "{}: HKCU\\Environment sets {}, which CL reads beside its command line: remove it from the prefix",
        reg.display(),
        set.join(", ")
    ))
}

#[must_use]
pub fn scrubbed(names: impl IntoIterator<Item = std::ffi::OsString>) -> Vec<std::ffi::OsString> {
    names
        .into_iter()
        .filter(|n| {
            SCRUBBED_ENV
                .iter()
                .any(|s| n.to_string_lossy().eq_ignore_ascii_case(s))
        })
        .collect()
}

impl CommandCompile {
    pub fn from_config(cfg: &Loaded) -> Result<Self> {
        let p = &cfg.source.prove;
        if p.compile.is_empty() {
            bail!(
                "[source.prove] compile is empty: name the command that compiles one translation unit, e.g. \
                 [\"wine\", \"${{MSVC6T_VC6}}/VC98/Bin/CL.EXE\", \"{{cflags}}\", \"/Fo{{out_win}}\", \"{{source_win}}\"]"
            );
        }
        let argv = p
            .compile
            .iter()
            .map(|a| cfg.expand(a))
            .collect::<Result<Vec<_>>>()?;
        let env = p
            .env
            .iter()
            .map(|(k, v)| Ok((k.clone(), cfg.expand(v)?)))
            .collect::<Result<Vec<_>>>()?;
        let wine = runs_wine(&argv);
        if wine
            && !env
                .iter()
                .any(|(k, v)| k == "WINEPREFIX" && !v.trim().is_empty())
        {
            bail!(
                "[source.prove] compile runs wine (a word named wine* or a .exe), so [source.prove.env] must name \
                 its WINEPREFIX, not empty: a prefix inherited from the caller, or wine's default, may be one \
                 another job is using"
            );
        }
        Ok(Self { argv, env })
    }

    fn value(key: &str, job: &Job) -> Option<String> {
        let source_abs = job.root.join(&job.source);
        let mut rel = relative(&job.cwd, &source_abs).display().to_string();
        if rel.starts_with(['-', '/', '@']) {
            rel = format!("./{rel}");
        }
        Some(match key {
            "source_win" => rel.replace('/', "\\"),
            "source_abs" => source_abs.display().to_string(),
            "source" => rel,
            "out_win" => win(&job.out),
            "out" => job.out.display().to_string(),
            "scratch_win" => win(&job.scratch),
            "scratch" => job.scratch.display().to_string(),
            "root_win" => win(&job.root),
            "root" => job.root.display().to_string(),
            "cwd" => job.cwd.display().to_string(),
            "slot" => job.slot.to_string(),
            _ => return key.strip_prefix("win:").map(|p| win(Path::new(p))),
        })
    }

    fn fill(&self, s: &str, job: &Job) -> String {
        let mut out = String::with_capacity(s.len());
        let mut rest = s;
        while let Some(at) = rest.find('{') {
            out.push_str(rest.get(..at).unwrap_or_default());
            let tail = rest.get(at + 1..).unwrap_or_default();
            match tail
                .find('}')
                .and_then(|e| Self::value(tail.get(..e)?, job).map(|v| (e, v)))
            {
                Some((e, v)) => {
                    out.push_str(&v);
                    rest = tail.get(e + 1..).unwrap_or_default();
                }
                None => {
                    out.push('{');
                    rest = tail;
                }
            }
        }
        out.push_str(rest);
        out
    }

    #[must_use]
    pub fn args(&self, job: &Job) -> Vec<String> {
        let mut out = Vec::new();
        for a in &self.argv {
            if a == "{cflags}" {
                out.extend(job.cflags.iter().map(|f| self.fill(f, job)));
            } else {
                out.push(self.fill(a, job));
            }
        }
        out
    }
}

impl Compile for CommandCompile {
    fn compile(&self, job: &Job) -> Result<Vec<u8>, String> {
        let args = self.args(job);
        let (exe, rest) = args.split_first().ok_or("an empty compile command")?;
        if job.scratch.exists() {
            std::fs::remove_dir_all(&job.scratch).map_err(|e| e.to_string())?;
        }
        std::fs::create_dir_all(&job.scratch).map_err(|e| e.to_string())?;
        let mut cmd = std::process::Command::new(exe);
        cmd.args(rest).current_dir(&job.cwd);
        for k in scrubbed(std::env::vars_os().map(|(k, _)| k)) {
            cmd.env_remove(k);
        }
        for (k, v) in &self.env {
            let v = self.fill(v, job);
            if k == "WINEPREFIX" {
                prefix_env(Path::new(&v))?;
            }
            cmd.env(k, v);
        }
        let out = cmd.output().map_err(|e| format!("running {exe}: {e}"))?;
        if !out.status.success() {
            let text = msvc6_base::srcio::tool_text(&out.stdout)
                + &msvc6_base::srcio::tool_text(&out.stderr);
            let tail: Vec<&str> = text.lines().rev().take(12).collect();
            return Err(format!(
                "{} exited {}: {}",
                job.source,
                out.status,
                tail.into_iter().rev().collect::<Vec<_>>().join(" | ")
            ));
        }
        std::fs::read(&job.out).map_err(|e| {
            format!(
                "{} compiled but left no object at {}: {e}",
                job.source,
                job.out.display()
            )
        })
    }

    fn describe(&self) -> String {
        self.argv.join(" ")
    }

    fn isolated(&self) -> bool {
        self.argv
            .iter()
            .chain(self.env.iter().map(|(_, v)| v))
            .any(|a| a.contains("{slot}"))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Include {
    Name(String),
    Computed,
}

#[must_use]
pub fn includes(src: &[u8]) -> Vec<Include> {
    let lx = lex::lex(src);
    lx.directives
        .iter()
        .filter(|d| matches!(d.name.as_str(), "include" | "import") && d.cond != Cond::Dead)
        .map(|d| match lx.args(d).first() {
            Some(t) if matches!(t.kind, Kind::Str | Kind::Header) && t.text.len() >= 2 => {
                let inner = t.text.get(1..t.text.len() - 1).unwrap_or_default();
                let name = inner.rsplit(['/', '\\']).next().unwrap_or(inner);
                Include::Name(name.to_ascii_lowercase())
            }
            _ => Include::Computed,
        })
        .collect()
}

#[derive(Debug, Clone, Default)]
pub struct Reach {
    pub files: BTreeSet<String>,
    pub computed: bool,
}

pub fn reach(
    tus: &[String],
    contents: &dyn Fn(&str) -> Vec<Vec<u8>>,
    all: &BTreeSet<String>,
) -> BTreeMap<String, Reach> {
    let mut by_name: BTreeMap<String, Vec<String>> = BTreeMap::new();
    for f in all {
        let name = f.rsplit('/').next().unwrap_or(f).to_ascii_lowercase();
        by_name.entry(name).or_default().push(f.clone());
    }
    let mut memo: BTreeMap<String, Vec<Include>> = BTreeMap::new();
    let mut out = BTreeMap::new();
    for tu in tus {
        let mut r = Reach::default();
        let mut todo = vec![tu.clone()];
        while let Some(f) = todo.pop() {
            if !r.files.insert(f.clone()) {
                continue;
            }
            let incs = memo
                .entry(f.clone())
                .or_insert_with(|| contents(&f).iter().flat_map(|b| includes(b)).collect());
            for i in incs.iter() {
                match i {
                    Include::Computed => r.computed = true,
                    Include::Name(n) => {
                        for hit in by_name.get(n).into_iter().flatten() {
                            if !r.files.contains(hit) {
                                todo.push(hit.clone());
                            }
                        }
                    }
                }
            }
        }
        out.insert(tu.clone(), r);
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Input {
    Revs {
        repo: PathBuf,
        before: String,
        after: String,
    },
    Dirs {
        before: PathBuf,
        after: PathBuf,
    },
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Changed {
    pub path: String,
    pub status: char,
    pub class: String,
    pub readers: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct TuRow {
    pub tu: String,
    pub before: String,
    pub after: String,
    pub verdict: String,
    pub at: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Report {
    pub holds: bool,
    pub compiler: String,
    pub work: String,
    pub changed: Vec<Changed>,
    pub tus: Vec<TuRow>,
    pub unproven: Vec<String>,
}

fn spec_for<'a>(specs: &'a [TuSpec], rel: &str) -> Option<&'a TuSpec> {
    specs.iter().find(|s| {
        (!s.source.is_empty() && crate::glob::matches(&s.source, rel))
            || s.sources.iter().any(|g| crate::glob::matches(g, rel))
    })
}

fn walk_files(root: &Path) -> Vec<String> {
    let mut v = Vec::new();
    let mut stack = vec![root.to_path_buf()];
    while let Some(d) = stack.pop() {
        let Ok(rd) = std::fs::read_dir(&d) else {
            continue;
        };
        for e in rd.flatten() {
            let p = e.path();
            if e.file_name() == ".git" {
                continue;
            }
            if e.file_type().is_ok_and(|t| t.is_dir()) {
                stack.push(p);
            } else if let Ok(r) = p.strip_prefix(root) {
                v.push(crate::tree::rel_str(r));
            }
        }
    }
    v.sort();
    v
}

fn copy_tree(from: &Path, to: &Path) -> Result<Vec<String>> {
    let files = walk_files(from);
    for f in &files {
        let dst = to.join(f);
        if let Some(p) = dst.parent() {
            std::fs::create_dir_all(p)?;
        }
        std::fs::copy(from.join(f), &dst).with_context(|| format!("copying {f}"))?;
    }
    Ok(files)
}

struct Tree {
    before: BTreeSet<String>,
    after: BTreeMap<String, Option<Vec<u8>>>,
    statuses: BTreeMap<String, char>,
    gitlinks: BTreeSet<String>,
}

fn setup(input: &Input, tree: &Path) -> Result<Tree> {
    match input {
        Input::Revs {
            repo,
            before,
            after,
        } => {
            let files = crate::git::materialize(repo, before, tree)?;
            let changes = crate::git::changes(repo, before, after)?;
            let mut gitlinks: BTreeSet<String> = BTreeSet::new();
            for rev in [before, after] {
                gitlinks.extend(
                    crate::git::ls_tree(repo, rev)?
                        .into_iter()
                        .filter(|e| e.kind == "commit")
                        .map(|e| e.path),
                );
            }
            let mut a = BTreeMap::new();
            let mut st = BTreeMap::new();
            for c in changes {
                if !crate::tree::safe_rel(&c.path) {
                    bail!("the change names `{}`, a path that leaves the tree", c.path);
                }
                let bytes = if gitlinks.contains(&c.path) {
                    None
                } else {
                    crate::git::file_at(repo, after, &c.path)?
                };
                st.insert(c.path.clone(), c.status);
                a.insert(c.path, bytes);
            }
            Ok(Tree {
                before: files.iter().map(|p| crate::tree::rel_str(p)).collect(),
                after: a,
                statuses: st,
                gitlinks,
            })
        }
        Input::Dirs { before, after } => {
            let files = copy_tree(before, tree)?;
            let later = walk_files(after);
            let mut a = BTreeMap::new();
            let mut st = BTreeMap::new();
            for f in &later {
                let nb = std::fs::read(after.join(f))?;
                match std::fs::read(before.join(f)) {
                    Ok(ob) if ob == nb => {}
                    Ok(_) => {
                        st.insert(f.clone(), 'M');
                        a.insert(f.clone(), Some(nb));
                    }
                    Err(_) => {
                        st.insert(f.clone(), 'A');
                        a.insert(f.clone(), Some(nb));
                    }
                }
            }
            for f in files.iter().filter(|f| !later.contains(f)) {
                st.insert(f.clone(), 'D');
                a.insert(f.clone(), None);
            }
            Ok(Tree {
                before: files.into_iter().collect(),
                after: a,
                statuses: st,
                gitlinks: BTreeSet::new(),
            })
        }
    }
}

fn build_input(cfg: &Loaded, rel: &str) -> bool {
    matches!(cfg.syntax_of(rel), Syntax::C | Syntax::Dsp | Syntax::Def)
}

fn run_jobs(
    jobs: &[Job],
    compiler: &dyn Compile,
    width: usize,
) -> Result<BTreeMap<String, Result<Vec<u8>, String>>> {
    if let Some(m) = mem_available()
        && m < MIN_AVAILABLE
    {
        bail!(
            "MemAvailable is {} MiB: the proof starts no batch of compiles below {} MiB",
            m / 1024 / 1024,
            MIN_AVAILABLE / 1024 / 1024
        );
    }
    let queue = std::sync::Mutex::new((0..jobs.len()).rev().collect::<Vec<usize>>());
    let out = std::sync::Mutex::new(BTreeMap::new());
    std::thread::scope(|s| {
        for slot in 0..width.max(1) {
            let queue = &queue;
            let out = &out;
            s.spawn(move || {
                loop {
                    let next = queue
                        .lock()
                        .unwrap_or_else(std::sync::PoisonError::into_inner)
                        .pop();
                    let Some(k) = next else {
                        break;
                    };
                    let Some(job) = jobs.get(k) else {
                        continue;
                    };
                    let mut job = job.clone();
                    job.slot = slot;
                    job.scratch = job.scratch.join(slot.to_string());
                    job.out = job.scratch.join("tu.obj");
                    let r = compiler.compile(&job);
                    out.lock()
                        .unwrap_or_else(std::sync::PoisonError::into_inner)
                        .insert(job.source.clone(), r);
                }
            });
        }
    });
    Ok(out
        .into_inner()
        .unwrap_or_else(std::sync::PoisonError::into_inner))
}

pub const MIN_AVAILABLE: u64 = 2 * 1024 * 1024 * 1024;

pub const WORK_MARK: &str = ".msvc6t-prove-work";

pub const MARK_MADE: &str = "msvc6t src prove made this directory\n";

pub const MARK_KEPT: &str = "msvc6t src prove clears this directory and keeps it\n";

#[cfg(unix)]
fn dir_id(work: &Path) -> Option<String> {
    use std::os::unix::fs::MetadataExt as _;
    let m = work.symlink_metadata().ok()?;
    Some(format!("{}:{}", m.dev(), m.ino()))
}

#[cfg(not(unix))]
fn dir_id(work: &Path) -> Option<String> {
    std::fs::canonicalize(work)
        .ok()
        .map(|p| p.display().to_string())
}

#[cfg(unix)]
fn single_link(p: &Path) -> bool {
    use std::os::unix::fs::MetadataExt as _;
    p.symlink_metadata()
        .is_ok_and(|m| m.is_file() && m.nlink() == 1)
}

#[cfg(not(unix))]
fn single_link(p: &Path) -> bool {
    p.symlink_metadata().is_ok_and(|m| m.is_file())
}

#[must_use]
pub fn mark_text(work: &Path, made: bool) -> Option<String> {
    Some(format!(
        "{}dir {}\n",
        if made { MARK_MADE } else { MARK_KEPT },
        dir_id(work)?
    ))
}

fn marked(work: &Path) -> Option<bool> {
    let p = work.join(WORK_MARK);
    if !single_link(&p) {
        return None;
    }
    let text = msvc6_base::srcio::own_read(&p).ok()?;
    [true, false]
        .into_iter()
        .find(|made| mark_text(work, *made).as_deref() == Some(text.as_str()))
}

fn clear(work: &Path) -> Result<()> {
    for e in std::fs::read_dir(work)? {
        let e = e?;
        let p = e.path();
        if e.file_type()?.is_dir() {
            std::fs::remove_dir_all(&p)?;
        } else {
            std::fs::remove_file(&p)?;
        }
    }
    Ok(())
}

pub fn claim_work(work: &Path) -> Result<()> {
    let made = match work.symlink_metadata() {
        Err(_) => {
            std::fs::create_dir_all(work)?;
            true
        }
        Ok(m) if !m.is_dir() => bail!(
            "{} is a link or a file: the proof works only in a directory",
            work.display()
        ),
        Ok(_) => match marked(work) {
            Some(made) => {
                clear(work).with_context(|| {
                    format!("clearing the proof's work directory {}", work.display())
                })?;
                made
            }
            None if std::fs::read_dir(work).is_ok_and(|mut d| d.next().is_none()) => false,
            None => bail!(
                "{} exists, holds files and no {WORK_MARK} the proof wrote: the proof clears only a work directory it marked",
                work.display()
            ),
        },
    };
    msvc6_base::srcio::own_write(
        work.join(WORK_MARK),
        mark_text(work, made).context("reading back the work directory's identity")?,
    )?;
    Ok(())
}

pub fn release_work(work: &Path) -> Result<()> {
    if !work.symlink_metadata().is_ok_and(|m| m.is_dir()) {
        return Ok(());
    }
    match marked(work) {
        Some(true) => std::fs::remove_dir_all(work)
            .with_context(|| format!("removing the work directory {}", work.display())),
        Some(false) => {
            clear(work).with_context(|| format!("clearing the work directory {}", work.display()))
        }
        None => Ok(()),
    }
}

pub fn mem_available() -> Option<u64> {
    let text = msvc6_base::srcio::own_read("/proc/meminfo").ok()?;
    let line = text.lines().find(|l| l.starts_with("MemAvailable:"))?;
    let kb: u64 = line.split_whitespace().nth(1)?.parse().ok()?;
    Some(kb * 1024)
}

pub fn prove(
    input: &Input,
    cfg: &Loaded,
    compiler: &dyn Compile,
    work: &Path,
    jobs: usize,
) -> Result<Report> {
    claim_work(work)?;
    let tree_dir = work.join("tree");
    std::fs::create_dir_all(&tree_dir)?;
    let t = setup(input, &tree_dir)?;
    let mut all: BTreeSet<String> = t.before.clone();
    all.extend(
        t.after
            .iter()
            .filter(|(_, b)| b.is_some())
            .map(|(p, _)| p.clone()),
    );
    let specs = &cfg.source.prove.tu;
    let tus: Vec<String> = all
        .iter()
        .filter(|f| spec_for(specs, f).is_some())
        .cloned()
        .collect();
    let contents = |f: &str| -> Vec<Vec<u8>> {
        let mut v = Vec::new();
        if t.before.contains(f)
            && let Ok(b) = std::fs::read(tree_dir.join(f))
        {
            v.push(b);
        }
        if let Some(Some(b)) = t.after.get(f) {
            v.push(b.clone());
        }
        v
    };
    let reach = reach(&tus, &contents, &all);
    let mut changed = Vec::new();
    let mut unproven = Vec::new();
    let mut needed: BTreeSet<String> = BTreeSet::new();
    for (path, status) in &t.statuses {
        let readers: Vec<String> = reach
            .iter()
            .filter(|(_, r)| r.computed || r.files.contains(path))
            .map(|(tu, _)| tu.clone())
            .collect();
        let unbuilt = cfg
            .source
            .prove
            .unbuilt
            .iter()
            .any(|g| crate::glob::matches(g, path));
        let class = if t.gitlinks.contains(path) && !unbuilt {
            unproven.push(format!(
                "{path}: a submodule whose commit changed; the proof compiles no submodule, so name it in \
                 [source.prove] unbuilt when no unit reads it"
            ));
            "unproven".to_string()
        } else if !readers.is_empty() {
            needed.extend(readers.iter().cloned());
            "read".to_string()
        } else if unbuilt {
            "declared unbuilt".to_string()
        } else if build_input(cfg, path) {
            unproven.push(format!(
                "{path}: a build input that no configured translation unit reads ([source.prove] tu), and not \
                 declared [source.prove] unbuilt"
            ));
            "unproven".to_string()
        } else {
            "not a build input".to_string()
        };
        changed.push(Changed {
            path: path.clone(),
            status: *status,
            class,
            readers,
        });
    }
    let width = if compiler.isolated() { jobs.max(1) } else { 1 };
    let job_for = |tu: &str| -> Result<Job> {
        let spec = spec_for(specs, tu).with_context(|| format!("{tu}: no [[source.prove.tu]]"))?;
        let cwd = match &spec.cwd {
            Some(c) => tree_dir.join(c),
            None => tree_dir
                .join(tu)
                .parent()
                .map_or_else(|| tree_dir.clone(), Path::to_path_buf),
        };
        let mut cflags: Vec<String> = spec.cflags.clone();
        for i in spec.include.iter().chain(&cfg.source.prove.include) {
            cflags.push(format!("/I{}", cfg.expand(i)?));
        }
        Ok(Job {
            root: tree_dir.clone(),
            source: tu.to_string(),
            cwd,
            cflags,
            scratch: work.join("obj"),
            out: PathBuf::new(),
            slot: 0,
        })
    };
    let job_list: Vec<Job> = needed.iter().map(|tu| job_for(tu)).collect::<Result<_>>()?;
    eprintln!(
        "prove: {} changed file(s), {} translation unit(s) read them; compiling each before and after ({} at a time)",
        t.statuses.len(),
        job_list.len(),
        width
    );
    let before = run_jobs(&job_list, compiler, width)?;
    for (path, bytes) in &t.after {
        if t.gitlinks.contains(path) {
            continue;
        }
        if !crate::tree::safe_rel(path) {
            bail!("the change names `{path}`, a path that leaves the tree");
        }
        let dst = tree_dir.join(path);
        if dst.symlink_metadata().is_ok_and(|m| !m.is_dir()) {
            std::fs::remove_file(&dst)?;
        }
        if let Some(b) = bytes {
            if let Some(p) = dst.parent() {
                std::fs::create_dir_all(p)?;
            }
            msvc6_base::srcio::write_bytes(&dst, b)?;
        }
    }
    let after = run_jobs(&job_list, compiler, width)?;
    let mut rows = Vec::new();
    for tu in &needed {
        let b = before
            .get(tu)
            .cloned()
            .unwrap_or_else(|| Err("not compiled".into()));
        let a = after
            .get(tu)
            .cloned()
            .unwrap_or_else(|| Err("not compiled".into()));
        let k = |r: &Result<Vec<u8>, String>| match r {
            Ok(o) => {
                crate::objkey::key(o).map_or_else(|_| msvc6_base::digest::sha256_hex(o), |k| k.code)
            }
            Err(_) => "-".to_string(),
        };
        let (verdict, at) = match (&b, &a) {
            (Ok(x), Ok(y)) => match crate::objkey::difference(x, y) {
                None => ("identical".to_string(), String::new()),
                Some(d) => ("moves".to_string(), d),
            },
            (Err(e), _) => ("unprovable".to_string(), format!("before: {e}")),
            (_, Err(e)) => (
                "moves".to_string(),
                format!("after: it no longer compiles: {e}"),
            ),
        };
        rows.push(TuRow {
            tu: tu.clone(),
            before: k(&b),
            after: k(&a),
            verdict,
            at,
        });
    }
    let holds = unproven.is_empty() && rows.iter().all(|r| r.verdict == "identical");
    Ok(Report {
        holds,
        compiler: compiler.describe(),
        work: work.display().to_string(),
        changed,
        tus: rows,
        unproven,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::objkey::fixture::{Sec, object};

    struct Double;

    impl Compile for Double {
        fn compile(&self, job: &Job) -> Result<Vec<u8>, String> {
            let src = std::fs::read(job.root.join(&job.source)).map_err(|e| e.to_string())?;
            let lx = lex::lex(&src);
            let mut code: Vec<u8> = Vec::new();
            for t in lx.tokens.iter().filter(|t| t.in_code()) {
                code.extend(t.text.as_bytes());
                code.push(b' ');
                if t.text == "__LINE__" {
                    code.extend(t.line.to_string().as_bytes());
                }
            }
            for d in lx.directives.iter().filter(|d| d.name == "include") {
                let name = lx.args(d)[0].text.trim_matches('"').to_string();
                let dir = job.root.join(&job.source);
                let inc = dir.parent().unwrap().join(name);
                let hdr = std::fs::read(inc).unwrap_or_default();
                for t in lex::lex(&hdr).tokens.iter().filter(|t| t.in_code()) {
                    code.extend(t.text.as_bytes());
                }
            }
            Ok(object(
                &[Sec {
                    name: ".text",
                    ch: 0x6030_0020,
                    raw: &code,
                    relocs: &[],
                }],
                &[],
                u32::try_from(src.len()).unwrap(),
            ))
        }

        fn describe(&self) -> String {
            "a test double: the object holds the live tokens, and each __LINE__'s line".into()
        }

        fn isolated(&self) -> bool {
            true
        }
    }

    fn scratch(tag: &str) -> PathBuf {
        let d =
            std::env::temp_dir().join(format!("msvc6-source-prove-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn put(d: &Path, rel: &str, bytes: &[u8]) {
        let p = d.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        msvc6_base::srcio::write_bytes(p, bytes).unwrap();
    }

    fn cfg() -> Loaded {
        crate::config::parse(
            "[source.prove]\ncompile = [\"x\"]\n[[source.prove.tu]]\nsource = \"src/*.cpp\"\n",
            Path::new("/"),
            None,
        )
        .unwrap()
    }

    #[test]
    fn a_reformatted_source_holds_and_a_moved_line_position_does_not() {
        let d = scratch("dirs");
        let (b, a, w) = (d.join("before"), d.join("after"), d.join("work"));
        put(
            &b,
            "src/a.cpp",
            b"#include \"a.h\"\nint f(int x){return x+1;}\n",
        );
        put(&b, "src/a.h", b"int f(int x);\n");
        put(&b, "src/b.cpp", b"int g() { return __LINE__; }\n");
        put(&b, "text/n.md", b"text\n");
        put(
            &a,
            "src/a.cpp",
            b"#include \"a.h\"\n\nint f(int x)\n{\n    return x + 1;\n}\n",
        );
        put(&a, "src/a.h", b"int f(int x);\n");
        put(&a, "src/b.cpp", b"int g() { return __LINE__; }\n");
        put(&a, "text/n.md", b"other text\n");
        let input = Input::Dirs {
            before: b.clone(),
            after: a.clone(),
        };
        let r = prove(&input, &cfg(), &Double, &w, 2).unwrap();
        assert!(r.holds, "{r:?}");
        assert_eq!(r.tus.len(), 1);
        assert_eq!(r.tus[0].verdict, "identical");
        assert_eq!(r.changed.len(), 2);
        assert!(r.changed.iter().any(|c| c.class == "not a build input"));
        put(&a, "src/b.cpp", b"\nint g() { return __LINE__; }\n");
        let r = prove(&input, &cfg(), &Double, &w, 2).unwrap();
        assert!(!r.holds);
        let moved = r.tus.iter().find(|t| t.tu == "src/b.cpp").unwrap();
        assert_eq!(moved.verdict, "moves");
        assert!(moved.at.starts_with(".text+"), "{}", moved.at);
        put(&a, "src/b.cpp", b"int g() { return __LINE__; }\n");
        put(&a, "src/a.h", b"int f(long x);\n");
        let r = prove(&input, &cfg(), &Double, &w, 1).unwrap();
        assert!(!r.holds);
        let ch = r.changed.iter().find(|c| c.path == "src/a.h").unwrap();
        assert_eq!(ch.readers, ["src/a.cpp"]);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_changed_build_input_no_unit_reads_is_unproven() {
        let d = scratch("unread");
        let (b, a, w) = (d.join("before"), d.join("after"), d.join("work"));
        put(&b, "lib/x.c", b"int x;\n");
        put(&a, "lib/x.c", b"int x ;\n");
        let input = Input::Dirs {
            before: b,
            after: a,
        };
        let r = prove(&input, &cfg(), &Double, &w, 1).unwrap();
        assert!(!r.holds);
        assert_eq!(r.unproven.len(), 1);
        let mut c = cfg();
        c.source.prove.unbuilt.push("lib/*.c".into());
        let r = prove(&input, &c, &Double, &w, 1).unwrap();
        assert!(r.holds, "{r:?}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn the_command_template_fills_paths_for_wine() {
        let c = CommandCompile {
            argv: vec![
                "wine".into(),
                "CL.EXE".into(),
                "{cflags}".into(),
                "/Fo{out_win}".into(),
                "{source_win}".into(),
            ],
            env: vec![(
                "INCLUDE".into(),
                "{win:/opt/vc/include};{root_win}\\inc".into(),
            )],
        };
        let job = Job {
            root: PathBuf::from("/w/tree"),
            source: "lib/src/a.cpp".into(),
            cwd: PathBuf::from("/w/tree/lib"),
            cflags: vec!["/O2".into(), "/I{root_win}\\x".into()],
            scratch: PathBuf::from("/w/obj/0"),
            out: PathBuf::from("/w/obj/0/tu.obj"),
            slot: 0,
        };
        assert_eq!(
            c.args(&job),
            [
                "wine",
                "CL.EXE",
                "/O2",
                "/IZ:\\w\\tree\\x",
                "/FoZ:\\w\\obj\\0\\tu.obj",
                "src\\a.cpp"
            ]
        );
        assert_eq!(
            c.fill(&c.env[0].1, &job),
            "Z:\\opt\\vc\\include;Z:\\w\\tree\\inc"
        );
        assert!(!c.isolated());
        assert_eq!(
            relative(Path::new("/a/b/c"), Path::new("/a/x/y.cpp")),
            PathBuf::from("../../x/y.cpp")
        );
    }

    #[test]
    fn includes_are_read_by_file_name_and_a_computed_one_reads_everything() {
        let v = includes(b"#include \"../inc/A.h\"\n#include <sys\\b.h>\n#if 0\n#include \"dead.h\"\n#endif\n#include HDR\n");
        assert_eq!(
            v,
            [
                Include::Name("a.h".into()),
                Include::Name("b.h".into()),
                Include::Computed
            ]
        );
    }
}
