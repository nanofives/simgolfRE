use crate::c1price::find_in;
use anyhow::{Context as _, Result, bail};
use msvc6_base::{bounded, srcio};
use msvc6_diff::comdat::{self, Comdat};
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_diff::env::Env;
use msvc6_toolchain::layout::to_win;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const MAX_SOURCE: u64 = 64 << 20;

pub const MAX_LISTING: u64 = 256 << 20;

pub const MAX_IL: u64 = 1 << 30;

pub const MAX_WALK: usize = 4096;

pub const HEADER_EXTENSIONS: [&str; 6] = ["h", "hpp", "hxx", "hh", "inl", "inc"];

pub const PAD_PREFIX: &str = "zz_pad";

#[must_use]
pub fn quoted_includes(text: &str) -> Vec<String> {
    text.lines()
        .filter_map(|l| {
            let l = l.trim_start().strip_prefix('#')?.trim_start();
            let l = l.strip_prefix("include")?.trim_start().strip_prefix('"')?;
            Some(l.split('"').next()?.to_string())
        })
        .collect()
}

fn canon(p: &Path) -> PathBuf {
    p.canonicalize().unwrap_or_else(|_| p.to_path_buf())
}

pub fn reaches(src: &Path, header: &Path, dirs: &[PathBuf]) -> Result<bool> {
    let mut seen = BTreeSet::new();
    let mut stack = vec![src.to_path_buf()];
    while let Some(f) = stack.pop() {
        if seen.len() > MAX_WALK {
            bail!(
                "{}: its quoted-include closure passes {MAX_WALK} files before {} is found or ruled out, \
                 so whether it includes it is not known",
                src.display(),
                header.display()
            );
        }
        let Ok(text) = bounded::source(&f, MAX_SOURCE) else {
            continue;
        };
        let here = f.parent().map(Path::to_path_buf).unwrap_or_default();
        for name in quoted_includes(&text) {
            let Some(p) = std::iter::once(&here)
                .chain(dirs)
                .find_map(|d| find_in(d, &name))
            else {
                continue;
            };
            let p = canon(&p);
            if p == header {
                return Ok(true);
            }
            if seen.insert(p.clone()) {
                stack.push(p);
            }
        }
    }
    Ok(false)
}

#[must_use]
pub fn first_labels(cod: &str) -> BTreeMap<String, u32> {
    let mut out = BTreeMap::new();
    let mut cur: Option<String> = None;
    for l in cod.lines() {
        let mut w = l.split_whitespace();
        let (a, b) = (w.next(), w.next());
        match b {
            Some("PROC") => cur = a.map(str::to_string),
            Some("ENDP") => cur = None,
            _ => {}
        }
        let Some(c) = &cur else { continue };
        let mut rest = l;
        while let Some(i) = rest.find("$L") {
            rest = rest.get(i + 2..).unwrap_or_default();
            let n: String = rest.chars().take_while(char::is_ascii_digit).collect();
            if let Ok(n) = n.parse::<u32>() {
                let e = out.entry(c.clone()).or_insert(n);
                *e = (*e).min(n);
            }
        }
    }
    out
}

#[must_use]
pub fn label_shift(b: &BTreeMap<String, u32>, o: &BTreeMap<String, u32>) -> Vec<i64> {
    let mut d: Vec<i64> = b
        .iter()
        .filter_map(|(k, &x)| o.get(k).map(|&y| i64::from(y) - i64::from(x)))
        .collect();
    d.sort_unstable();
    d.dedup();
    d
}

fn calls(c: &Comdat) -> BTreeMap<String, usize> {
    let mut m = BTreeMap::new();
    for s in msvc6_gate::calls::comdat_calls(c) {
        *m.entry(s.to_string()).or_insert(0) += 1;
    }
    m
}

#[must_use]
pub fn call_moves(b: &BTreeMap<String, usize>, o: &BTreeMap<String, usize>) -> Vec<String> {
    let keys: BTreeSet<&String> = b.keys().chain(o.keys()).collect();
    keys.into_iter()
        .filter_map(|k| {
            let (x, y) = (
                b.get(k).copied().unwrap_or(0),
                o.get(k).copied().unwrap_or(0),
            );
            (x != y).then(|| format!("{k} {x}->{y}"))
        })
        .collect()
}

#[must_use]
pub fn unnumbered(s: &str) -> String {
    if s.starts_with('$') {
        s.trim_end_matches(|c: char| c.is_ascii_digit()).to_string()
    } else {
        s.to_string()
    }
}

#[must_use]
pub fn addends(c: &Comdat) -> Vec<Option<u32>> {
    c.relocations
        .iter()
        .filter(|r| !r.symbol.starts_with('$') && !r.symbol.starts_with('.'))
        .map(|r| {
            let at = r.at as usize;
            c.bytes
                .get(at..at.saturating_add(4))
                .and_then(|b| <[u8; 4]>::try_from(b).ok())
                .map(u32::from_le_bytes)
        })
        .collect()
}

#[must_use]
pub fn same(a: &Comdat, b: &Comdat) -> bool {
    let masked = |c: &Comdat| -> Vec<u8> {
        c.bytes
            .iter()
            .zip(&c.relocated)
            .map(|(&x, &r)| if r { 0 } else { x })
            .collect()
    };
    let targets = |c: &Comdat| -> Vec<String> {
        c.relocations
            .iter()
            .map(|r| unnumbered(&r.symbol))
            .collect()
    };
    masked(a) == masked(b) && targets(a) == targets(b) && addends(a) == addends(b)
}

#[must_use]
pub fn key(c: &Comdat) -> String {
    c.symbol.clone().unwrap_or_else(|| format!("#{}", c.index))
}

#[must_use]
pub fn pad_text(n: usize) -> String {
    let decls: String = (0..n)
        .map(|k| format!("extern int {PAD_PREFIX}{k}; "))
        .collect();
    format!("{decls}\n#line 1\n")
}

#[must_use]
pub fn insert_pad(text: &[u8], n: usize, at: usize) -> Vec<u8> {
    let at = at.max(1);
    let cut = match at.checked_sub(2) {
        None => 0,
        Some(k) => text
            .iter()
            .enumerate()
            .filter(|&(_, &b)| b == b'\n')
            .nth(k)
            .map_or(text.len(), |(i, _)| i + 1),
    };
    let (head, tail) = text.split_at(cut);
    let pad = pad_text(n).replace("#line 1", &format!("#line {at}"));
    [head, pad.as_bytes(), tail].concat()
}

#[derive(Clone, Copy, Debug)]
pub enum Variant<'a> {
    Base,
    Header {
        from: &'a Path,
        to: &'a Path,
        src: Option<&'a Path>,
    },
    Pad(usize, usize),
    Overlay {
        map: &'a [(PathBuf, PathBuf)],
        text: Option<&'a Path>,
    },
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Built {
    pub obj: PathBuf,
    pub cod: PathBuf,
}

fn stem_of(p: &Path) -> String {
    p.file_stem()
        .unwrap_or_default()
        .to_string_lossy()
        .to_string()
}

pub fn stage(u: &Unit, v: Variant<'_>) -> Result<(Vec<u8>, Vec<PathBuf>)> {
    let read = |p: &Path| -> Result<Vec<u8>> {
        bounded::read(p, MAX_SOURCE).with_context(|| format!("reading {}", p.display()))
    };
    let text = match v {
        Variant::Pad(n, at) => insert_pad(&read(&u.source)?, n, at),
        Variant::Header { src: Some(p), .. } if stem_of(p) == stem_of(&u.source) => read(p)?,
        Variant::Overlay { text: Some(p), .. } => read(p)?,
        _ => read(&u.source)?,
    };
    let own = u
        .source
        .canonicalize()
        .ok()
        .and_then(|p| p.parent().map(Path::to_path_buf));
    let includes = u
        .includes
        .iter()
        .map(|d| canon(d))
        .chain(own)
        .map(|abs| match v {
            Variant::Header { from, to, .. } if abs == from => to.to_path_buf(),
            Variant::Overlay { map, .. } => map
                .iter()
                .find(|(f, _)| *f == abs)
                .map_or(abs, |(_, t)| t.clone()),
            _ => abs,
        })
        .collect();
    Ok((text, includes))
}

fn args_of(u: &Unit, includes: &[PathBuf], extra: &[String]) -> Vec<String> {
    let mut args = vec!["/nologo".to_string(), "/c".to_string()];
    args.extend(u.cflags.iter().cloned());
    args.extend(includes.iter().map(|d| format!("/I{}", to_win(d))));
    args.extend(extra.iter().cloned());
    args
}

fn run_cl(c: &Compiler, u: &Unit, dir: &Path, args: &[String]) -> Result<()> {
    let tc = c.toolchain(&u.toolchain)?;
    let mut env = Vec::new();
    if let Some(i) = &u.include_env {
        env.push(("INCLUDE".to_string(), i.clone()));
    }
    let sess = c.runner.session()?;
    sess.run_tool(&tc, "CL.EXE", args, Some(dir), &env)
        .map(|_| ())
}

fn prepare(u: &Unit, dir: &Path, v: Variant<'_>) -> Result<Vec<PathBuf>> {
    let (text, includes) = stage(u, v)?;
    std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))?;
    srcio::write_bytes(dir.join(&u.file), &text)
        .with_context(|| format!("staging {} in {}", u.file, dir.display()))?;
    if msvc6_toolchain::compile::creates_pch(&u.cflags) {
        msvc6_toolchain::compile::clear_pch(dir);
    }
    Ok(includes)
}

fn remove(p: &Path) -> Result<()> {
    match std::fs::remove_file(p) {
        Ok(()) => Ok(()),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(e) => Err(e).with_context(|| format!("removing {}", p.display())),
    }
}

pub fn compile(c: &Compiler, u: &Unit, dir: &Path, v: Variant<'_>) -> Result<Built> {
    let includes = prepare(u, dir, v)?;
    let obj = dir.join(format!("{}.obj", u.stem));
    let cod = dir.join(format!("{}.cod", u.stem));
    remove(&obj)?;
    remove(&cod)?;
    let extra = [
        "/FAcs".to_string(),
        format!("/Fa{}.cod", u.stem),
        format!("/Fo{}.obj", u.stem),
        u.file.clone(),
    ];
    run_cl(c, u, dir, &args_of(u, &includes, &extra))
        .with_context(|| format!("compiling {}", u.source.display()))?;
    if !obj.is_file() {
        bail!("CL wrote no {}", obj.display());
    }
    Ok(Built { obj, cod })
}

#[must_use]
pub fn front_end_build(b: Option<u16>) -> String {
    b.map_or_else(|| "build unstated".to_string(), |b| b.to_string())
}

pub fn c1_counter(
    c: &Compiler,
    u: &Unit,
    dir: &Path,
    v: Variant<'_>,
) -> Result<(u32, Option<u16>)> {
    let includes = prepare(u, dir, v)?;
    for s in ["ex", "gl", "in", "sy", "db"] {
        remove(&dir.join(format!("c1_{s}")))?;
    }
    let gl = dir.join("c1_gl");
    let extra = [
        "/Bkc1_".to_string(),
        format!("/B2{}", to_win(&canon(dir).join("no-c2.dll"))),
        u.file.clone(),
    ];
    match run_cl(c, u, dir, &args_of(u, &includes, &extra)) {
        Err(e) if format!("{e:#}").contains("D2027") && gl.is_file() => {}
        Err(e) => return Err(e).with_context(|| format!("C1 of {}", u.source.display())),
        Ok(()) => bail!(
            "CL ran a C2 for {} although /B2 named none: the C1-only run is not C1 only",
            u.source.display()
        ),
    }
    let bytes = bounded::read(&gl, MAX_IL).with_context(|| format!("reading {}", gl.display()))?;
    crate::drain::gl_counter(&bytes).with_context(|| gl.display().to_string())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FnRow {
    pub key: String,
    pub kind: &'static str,
    pub rva: Option<u32>,
    pub fig: Option<((usize, usize), (usize, usize))>,
    pub addend: Option<(usize, usize)>,
    pub moves: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TuDiff {
    pub shift: Vec<i64>,
    pub rows: Vec<FnRow>,
}

fn signed(now: usize, before: usize) -> i64 {
    i64::try_from(now).unwrap_or(i64::MAX) - i64::try_from(before).unwrap_or(i64::MAX)
}

impl TuDiff {
    #[must_use]
    pub fn totals(&self) -> (i64, i64) {
        self.rows
            .iter()
            .filter_map(|r| r.fig)
            .fold((0, 0), |(d, e), ((d0, e0), (d1, e1))| {
                (d + signed(d1, d0), e + signed(e1, e0))
            })
    }

    #[must_use]
    pub fn shift_text(&self) -> String {
        match self.shift.as_slice() {
            [] => "?".to_string(),
            [d] => format!("{d:+} (mod 64 {})", d.rem_euclid(64)),
            many => format!("{many:?}"),
        }
    }
}

pub fn listing(p: &Path) -> String {
    bounded::source(p, MAX_LISTING).unwrap_or_default()
}

pub fn seats(env: &Env, obj: &Path) -> BTreeMap<String, u32> {
    msvc6_diff::seat::seat_map(env, &[obj.to_path_buf()], 1)
        .seats
        .into_iter()
        .map(|(rva, (_, c))| (key(&c), rva))
        .collect()
}

pub fn figure(env: &Env, obj: &Path, c: &Comdat, rva: u32) -> Option<((usize, usize), usize)> {
    msvc6_diff::segrank::score(env, rva, &(obj.to_path_buf(), c.clone()))
        .ok()
        .map(|r| ((r.differing, r.edit), r.addend))
}

pub fn diff_tu(env: &Env, b: &Built, v: &Built) -> Result<TuDiff> {
    let shift = label_shift(
        &first_labels(&listing(&b.cod)),
        &first_labels(&listing(&v.cod)),
    );
    let cb = comdat::text(&b.obj)?;
    let cv = comdat::text(&v.obj)?;
    let mb: BTreeMap<String, &Comdat> = cb.iter().map(|c| (key(c), c)).collect();
    let mv: BTreeMap<String, &Comdat> = cv.iter().map(|c| (key(c), c)).collect();
    let names: BTreeSet<&String> = mb.keys().chain(mv.keys()).collect();
    let changed: Vec<&String> = names
        .into_iter()
        .filter(|k| match (mb.get(*k), mv.get(*k)) {
            (Some(x), Some(y)) => !same(x, y),
            _ => true,
        })
        .collect();
    if changed.is_empty() {
        return Ok(TuDiff {
            shift,
            rows: Vec::new(),
        });
    }
    let (sb, sv) = (seats(env, &b.obj), seats(env, &v.obj));
    let rows = changed
        .into_iter()
        .map(|k| {
            let (x, y) = (mb.get(k).copied(), mv.get(k).copied());
            let moves = call_moves(
                &x.map(calls).unwrap_or_default(),
                &y.map(calls).unwrap_or_default(),
            );
            let kind = match (x, y) {
                (None, _) => "ADDED",
                (_, None) => "DROPPED",
                _ if !moves.is_empty() => "INLINE",
                _ if shift.iter().any(|&d| d != 0) => "NUMBERING",
                _ => "CODEGEN",
            };
            let rva = sb.get(k).or_else(|| sv.get(k)).copied();
            let both = match (rva, x, y) {
                (Some(r), Some(x), Some(y)) => {
                    figure(env, &b.obj, x, r).zip(figure(env, &v.obj, y, r))
                }
                _ => None,
            };
            FnRow {
                key: k.clone(),
                kind,
                rva,
                fig: both.map(|((f0, _), (f1, _))| (f0, f1)),
                addend: both.map(|((_, a0), (_, a1))| (a0, a1)),
                moves,
            }
        })
        .collect();
    Ok(TuDiff { shift, rows })
}

#[must_use]
pub fn row_line(r: &FnRow) -> String {
    let fig = match (r.rva, r.fig) {
        (Some(a), Some(((d0, e0), (d1, e1)))) => format!(
            "{a:#x} differing {d0}->{d1} ({:+}) edit {e0}->{e1} ({:+})",
            signed(d1, d0),
            signed(e1, e0)
        ),
        (Some(a), None) => format!("{a:#x}"),
        _ => "unseated".to_string(),
    };
    let l = format!("  {:<9} {} {fig} {}", r.kind, r.key, r.moves.join(", "));
    l.trim_end().to_string()
}

#[must_use]
pub fn is_header_file(p: &Path) -> bool {
    p.extension()
        .and_then(|e| e.to_str())
        .is_some_and(|e| HEADER_EXTENSIONS.iter().any(|x| e.eq_ignore_ascii_case(x)))
}

pub fn tree_dir(header: &Path, dirs: &[PathBuf]) -> Result<PathBuf> {
    let hdir = header.parent().map(Path::to_path_buf).unwrap_or_default();
    if !dirs
        .iter()
        .any(|d| d.canonicalize().is_ok_and(|d| d == hdir))
    {
        bail!(
            "{}: not in one of the directories the image's TUs search -- name the header as it stands in the \
             project tree and pass the edit as --with",
            header.display()
        );
    }
    Ok(hdir)
}

pub fn copy_headers(hdir: &Path, to: &Path) -> Result<usize> {
    match std::fs::remove_dir_all(to) {
        Ok(()) => {}
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
        Err(e) => return Err(e).with_context(|| format!("clearing {}", to.display())),
    }
    std::fs::create_dir_all(to).with_context(|| format!("creating {}", to.display()))?;
    let mut n = 0;
    for e in std::fs::read_dir(hdir).with_context(|| format!("reading {}", hdir.display()))? {
        let p = e?.path();
        if std::fs::symlink_metadata(&p).is_ok_and(|m| m.is_file()) && is_header_file(&p) {
            std::fs::copy(&p, to.join(p.file_name().unwrap_or_default()))
                .with_context(|| format!("copying {}", p.display()))?;
            n += 1;
        }
    }
    Ok(n)
}

#[must_use]
pub fn overlay_dir(root: &Path, tag: &str, hdir: &Path) -> PathBuf {
    let name = hdir.file_name().and_then(|n| n.to_str()).unwrap_or("dir");
    root.join("ovl").join(tag).join(name)
}

#[must_use]
pub fn run_root(work: &Path, tool: &str, label: &str) -> PathBuf {
    work.join(tool)
        .join(label)
        .join(std::process::id().to_string())
}

#[derive(Debug)]
pub struct Scratch(pub Vec<PathBuf>);

impl Drop for Scratch {
    fn drop(&mut self) {
        for d in &self.0 {
            let _ = std::fs::remove_dir_all(d);
        }
    }
}

pub fn scratch_root(root: &Path) -> Result<Scratch> {
    match std::fs::remove_dir_all(root) {
        Ok(()) => {}
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
        Err(e) => return Err(e).with_context(|| format!("clearing {}", root.display())),
    }
    std::fs::create_dir_all(root).with_context(|| format!("creating {}", root.display()))?;
    Ok(Scratch(vec![root.to_path_buf()]))
}

pub fn read_with(p: &Path) -> Result<Vec<u8>> {
    bounded::read(p, MAX_SOURCE)
        .with_context(|| format!("--with {}: cannot read the edited header", p.display()))
}

pub fn need_includers(n: usize, label: &str, header: &Path) -> Result<()> {
    if n == 0 {
        bail!(
            "no TU of {label} includes {}: nothing to measure",
            header.display()
        );
    }
    Ok(())
}

pub type Job<'a> = (&'a Unit, String, Variant<'a>);

pub type Builds = BTreeMap<(String, String), Built>;

#[must_use]
pub fn failed_compiles(work: &[Job<'_>], have: &Builds) -> Vec<String> {
    let mut out: Vec<String> = work
        .iter()
        .filter(|(u, tag, _)| !have.contains_key(&(u.stem.clone(), tag.clone())))
        .map(|(u, tag, _)| format!("{} {tag}", u.stem))
        .collect();
    out.dedup();
    out
}

pub fn compiles_verdict(failed: &[String], of: usize) -> Result<()> {
    if failed.is_empty() {
        return Ok(());
    }
    bail!(
        "{} of {of} compile(s) FAILED ({}): the totals above leave them out",
        failed.len(),
        failed.join(", ")
    )
}

#[must_use]
pub fn failed_lines(failed: &[String]) -> Vec<String> {
    failed
        .iter()
        .map(|f| format!("FAILED {f} (its compile failed)"))
        .collect()
}

#[must_use]
pub fn sweep_chunk(jobs: usize, pads: usize) -> usize {
    (4 * jobs.max(1)).div_ceil(pads.max(1)).max(1)
}

#[must_use]
pub fn work_dir(root: &Path, stem: &str, tag: &str) -> PathBuf {
    root.join(stem).join(tag)
}

#[derive(Debug, Default)]
pub struct Batch {
    pub builds: Builds,
    pub errors: Vec<String>,
}

pub fn build_all(c: &Compiler, root: &Path, work: &[Job<'_>], jobs: usize) -> Batch {
    let done = msvc6_diff::par::map(work, jobs, |(u, tag, v)| {
        compile(c, u, &work_dir(root, &u.stem, tag), *v)
    });
    let mut out = Batch::default();
    for ((u, tag, _), r) in work.iter().zip(done) {
        match r {
            Ok(b) => {
                out.builds.insert((u.stem.clone(), tag.clone()), b);
            }
            Err(e) => out
                .errors
                .push(format!("{}: {tag}: {e:#}", u.source.display())),
        }
    }
    out
}

#[must_use]
pub fn search_dirs(units: &[Unit]) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = Vec::new();
    for u in units {
        let own = u
            .source
            .canonicalize()
            .ok()
            .and_then(|p| p.parent().map(Path::to_path_buf));
        for d in u.includes.iter().map(|d| canon(d)).chain(own) {
            if !out.contains(&d) {
                out.push(d);
            }
        }
    }
    out
}

pub fn includers<'u>(units: &'u [Unit], header: &Path) -> Result<Vec<&'u Unit>> {
    let mut out = Vec::new();
    for u in units {
        let own = u
            .source
            .canonicalize()
            .ok()
            .and_then(|p| p.parent().map(Path::to_path_buf));
        let dirs: Vec<PathBuf> = u.includes.iter().map(|d| canon(d)).chain(own).collect();
        if reaches(&u.source, header, &dirs)? {
            out.push(u);
        }
    }
    Ok(out)
}

#[derive(Debug)]
pub struct PriceOpts<'a> {
    pub label: &'a str,
    pub header: &'a Path,
    pub with: &'a Path,
    pub src: Option<&'a Path>,
    pub c1: bool,
    pub jobs: usize,
    pub work: &'a Path,
}

#[derive(Debug, Default)]
pub struct Priced {
    pub lines: Vec<String>,
    pub errors: Vec<String>,
    pub failed: Vec<String>,
    pub compiles: usize,
}

#[allow(clippy::too_many_lines)]
pub fn price(c: &Compiler, env: Option<&Env>, units: &[Unit], o: &PriceOpts<'_>) -> Result<Priced> {
    let with_text = read_with(o.with)?;
    let header = o
        .header
        .canonicalize()
        .with_context(|| o.header.display().to_string())?;
    let tus = includers(units, &header)?;
    let hdir = tree_dir(&header, &search_dirs(units))?;
    let mut p = Priced::default();
    p.lines
        .push(format!("{} includer(s) of {}", tus.len(), header.display()));
    need_includers(tus.len(), o.label, &header)?;
    let root = run_root(o.work, "header-price", o.label);
    let _scratch = scratch_root(&root)?;
    let ovl = overlay_dir(&root, "run", &hdir);
    copy_headers(&hdir, &ovl)?;
    srcio::write_bytes(ovl.join(header.file_name().unwrap_or_default()), &with_text)
        .with_context(|| format!("writing the edit into {}", ovl.display()))?;
    let alt = Variant::Header {
        from: &hdir,
        to: &ovl,
        src: o.src,
    };
    let work: Vec<Job<'_>> = tus
        .iter()
        .flat_map(|u| {
            [
                (*u, "base".to_string(), Variant::Base),
                (*u, "ovl".to_string(), alt),
            ]
        })
        .collect();
    p.compiles = work.len();
    if o.c1 {
        let got = msvc6_diff::par::map(&work, o.jobs, |(u, tag, v)| {
            c1_counter(c, u, &root.join(&u.stem).join(format!("c1-{tag}")), *v)
        });
        let mut by: BTreeMap<(String, String), (u32, Option<u16>)> = BTreeMap::new();
        for ((u, tag, _), r) in work.iter().zip(got) {
            match r {
                Ok(x) => {
                    by.insert((u.stem.clone(), tag.clone()), x);
                }
                Err(e) => p
                    .errors
                    .push(format!("{}: {tag}: {e:#}", u.source.display())),
            }
        }
        p.failed = work
            .iter()
            .filter(|(u, tag, _)| !by.contains_key(&(u.stem.clone(), tag.clone())))
            .map(|(u, tag, _)| format!("{} {tag}", u.stem))
            .collect();
        let mut shifts: BTreeMap<i64, usize> = BTreeMap::new();
        for u in &tus {
            let get = |tag: &str| by.get(&(u.stem.clone(), tag.to_string())).copied();
            let (Some((b, fb)), Some((v, fv))) = (get("base"), get("ovl")) else {
                p.lines.push(format!(
                    "{}: FAILED, NOT MEASURED (a C1 run failed)",
                    u.stem
                ));
                continue;
            };
            let d = i64::from(v) - i64::from(b);
            *shifts.entry(d).or_default() += 1;
            let build = if fb == fv {
                format!("C1XX {}", front_end_build(fb))
            } else {
                format!("C1XX {} vs {}!", front_end_build(fb), front_end_build(fv))
            };
            p.lines.push(format!(
                "{}: C1 counter {b} -> {v}, shift {d:+} (mod 64 {}) [{build}]",
                u.stem,
                d.rem_euclid(64)
            ));
        }
        let list: Vec<String> = shifts
            .iter()
            .map(|(d, n)| format!("{d:+} (mod 64 {}) x{n}", d.rem_euclid(64)))
            .collect();
        p.lines.push(format!(
            "C1 shift over {} includer(s), MEASURED by C1-only runs (no C2, no object): {}",
            tus.len(),
            if list.is_empty() {
                "none measured".to_string()
            } else {
                list.join(", ")
            }
        ));
        return Ok(p);
    }
    let Some(env) = env else {
        bail!("pricing the compiled edit seats its functions in the reference: an image is needed");
    };
    let built = build_all(c, &root, &work, o.jobs);
    p.errors = built.errors;
    p.failed = failed_compiles(&work, &built.builds);
    let (mut all_d, mut all_e) = (0i64, 0i64);
    let mut measured = 0usize;
    for u in &tus {
        let get = |tag: &str| built.builds.get(&(u.stem.clone(), tag.to_string()));
        let (Some(b), Some(v)) = (get("base"), get("ovl")) else {
            p.lines.push(format!(
                "{}: FAILED (a compile failed): left out of the total",
                u.stem
            ));
            continue;
        };
        measured += 1;
        let d = diff_tu(env, b, v)?;
        let (td, te) = d.totals();
        all_d += td;
        all_e += te;
        p.lines.push(format!(
            "{}: $L {}; {} changed; differing {td:+} edit {te:+}",
            u.stem,
            d.shift_text(),
            d.rows.len()
        ));
        p.lines.extend(d.rows.iter().map(row_line));
    }
    p.lines.push(format!(
        "total over {measured} of {} includer(s): differing {all_d:+} edit {all_e:+}",
        tus.len()
    ));
    Ok(p)
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_diff::comdat::Reloc;

    fn scratch(name: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-model-hp-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn an_overlay_copies_only_header_files() {
        let d = scratch("ovl");
        let hdir = d.join("include");
        std::fs::create_dir_all(&hdir).unwrap();
        for f in ["A.h", "B.HPP", "c.inl", "notes.txt", "x.cpp"] {
            srcio::own_write(hdir.join(f), b"x").unwrap();
        }
        let ovl = overlay_dir(&d, "pad01", &hdir);
        assert_eq!(copy_headers(&hdir, &ovl).unwrap(), 3);
        let mut got: Vec<String> = std::fs::read_dir(&ovl)
            .unwrap()
            .map(|e| e.unwrap().file_name().to_string_lossy().into_owned())
            .collect();
        got.sort();
        assert_eq!(got, ["A.h", "B.HPP", "c.inl"]);
        std::fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn a_header_outside_the_searched_directories_is_refused() {
        let d = scratch("tree");
        let (inc, tmp) = (d.join("include"), d.join("tmp"));
        std::fs::create_dir_all(&inc).unwrap();
        std::fs::create_dir_all(&tmp).unwrap();
        srcio::own_write(inc.join("X.h"), b"int x;\n").unwrap();
        srcio::own_write(tmp.join("X.h"), b"int x;\n").unwrap();
        let dirs = vec![inc.clone()];
        let inside = inc.join("X.h").canonicalize().unwrap();
        assert_eq!(
            tree_dir(&inside, &dirs).unwrap(),
            inc.canonicalize().unwrap()
        );
        let outside = tmp.join("X.h").canonicalize().unwrap();
        let e = tree_dir(&outside, &dirs).unwrap_err().to_string();
        assert!(e.contains("not in one of the directories"), "{e}");
        std::fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn a_run_root_is_its_own_and_an_early_error_leaves_nothing() {
        let d = scratch("root");
        let r = run_root(&d, "label-sweep", "IMG");
        assert!(
            r.ends_with(
                Path::new("label-sweep")
                    .join("IMG")
                    .join(std::process::id().to_string())
            )
        );
        {
            let _s = scratch_root(&r).unwrap();
            assert!(r.is_dir());
        }
        assert!(!r.exists());
        let a = overlay_dir(&r, "run", Path::new("/p/include"));
        let b = overlay_dir(&r, "pad07", Path::new("/p/include"));
        assert_ne!(a, b);
        assert!(work_dir(&r, "Shot", "pad07").starts_with(&r));
        std::fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn a_missing_with_file_is_named_and_no_includer_is_refused() {
        let p = std::env::temp_dir().join(format!("hp-no-such-with-{}.h", std::process::id()));
        let e = read_with(&p).unwrap_err().to_string();
        assert!(
            e.contains(&p.display().to_string()) && e.contains("--with"),
            "{e}"
        );
        let e = need_includers(0, "IMG", Path::new("/tmp/X.h"))
            .unwrap_err()
            .to_string();
        assert!(e.contains("no TU of IMG includes /tmp/X.h"), "{e}");
        assert!(need_includers(3, "IMG", Path::new("/tmp/X.h")).is_ok());
    }

    #[test]
    fn a_failed_compile_is_named_and_fails_the_run() {
        let u = |stem: &str| Unit {
            source: PathBuf::from(format!("/p/{stem}.cpp")),
            stem: stem.to_string(),
            file: format!("{stem}.cpp"),
            cflags: vec![],
            includes: vec![],
            include_env: None,
            toolchain: "vc6".to_string(),
        };
        let (a, b) = (u("Shot"), u("Bar"));
        let work = vec![
            (&a, "base".to_string(), Variant::Base),
            (&a, "ovl".to_string(), Variant::Base),
            (&b, "base".to_string(), Variant::Base),
            (&b, "ovl".to_string(), Variant::Base),
        ];
        let mut have = Builds::new();
        for (s, t) in [("Shot", "base"), ("Shot", "ovl"), ("Bar", "base")] {
            have.insert(
                (s.to_string(), t.to_string()),
                Built {
                    obj: PathBuf::new(),
                    cod: PathBuf::new(),
                },
            );
        }
        let failed = failed_compiles(&work, &have);
        assert_eq!(failed, ["Bar ovl"]);
        let e = compiles_verdict(&failed, work.len())
            .unwrap_err()
            .to_string();
        assert!(e.contains("1 of 4 compile(s) FAILED (Bar ovl)"), "{e}");
        assert!(compiles_verdict(&[], 4).is_ok());
        assert_eq!(sweep_chunk(4, 64), 1);
        assert_eq!(sweep_chunk(4, 2), 8);
        assert_eq!(sweep_chunk(0, 0), 4);
    }

    #[test]
    fn listings_shifts_pads_and_targets() {
        let cod = "?f@@YAXXZ PROC NEAR\n$L100:\n jmp SHORT $L98\n?f@@YAXXZ ENDP\n_g PROC NEAR\n $L7:\n_g ENDP\n $L1:\n";
        let m = first_labels(cod);
        assert_eq!(m.get("?f@@YAXXZ"), Some(&98));
        assert_eq!(m.get("_g"), Some(&7));
        assert_eq!(m.len(), 2);
        let b: BTreeMap<String, u32> = [("a".into(), 10), ("b".into(), 20)].into();
        let o: BTreeMap<String, u32> = [("a".into(), 14), ("b".into(), 24)].into();
        assert_eq!(label_shift(&b, &o), vec![4]);
        assert_eq!(unnumbered("$SG1234"), unnumbered("$SG1238"));
        assert_eq!(unnumbered("_g_x2"), "_g_x2");
        assert_eq!(
            pad_text(2),
            "extern int zz_pad0; extern int zz_pad1; \n#line 1\n"
        );
        let t = b"a\nb\nc\n";
        assert_eq!(insert_pad(t, 0, 1), [b"\n#line 1\n".as_slice(), t].concat());
        assert_eq!(
            insert_pad(t, 1, 2),
            b"a\nextern int zz_pad0; \n#line 2\nb\nc\n"
        );
        assert_eq!(insert_pad(t, 0, 9), b"a\nb\nc\n\n#line 9\n");
        let bc: BTreeMap<String, usize> = [("x".into(), 2), ("y".into(), 1)].into();
        let oc: BTreeMap<String, usize> = [("x".into(), 2), ("z".into(), 1)].into();
        assert_eq!(call_moves(&bc, &oc), vec!["y 1->0", "z 0->1"]);
        assert_eq!(
            quoted_includes("#include <w.h>\n# include \"A.h\"\n  #include \"sub/B.h\" x\n"),
            vec!["A.h", "sub/B.h"]
        );
    }

    fn reloc(at: u32, symbol: &str) -> Reloc {
        Reloc {
            at,
            symbol: symbol.to_string(),
            kind: 6,
            external: true,
            global: true,
            target_section: None,
            target_offset: 0,
            target_index: None,
        }
    }

    #[test]
    fn same_reads_the_addend_of_a_named_target_only() {
        let mk = |a: u32, b: u32| Comdat {
            bytes: [a.to_le_bytes(), b.to_le_bytes()].concat(),
            relocated: vec![true; 8],
            relocations: vec![reloc(0, "?g_table@@3PAUT@@A"), reloc(4, "$SG123")],
            ..Comdat::default()
        };
        assert!(same(&mk(0x418, 0), &mk(0x418, 8)));
        assert!(!same(&mk(0x418, 0), &mk(0x428, 0)));
        assert_eq!(addends(&mk(0x418, 0)), vec![Some(0x418)]);
    }

    #[test]
    fn an_includer_is_found_through_nested_quoted_includes() {
        let d = scratch("reach");
        let inc = d.join("include");
        std::fs::create_dir_all(&inc).unwrap();
        srcio::own_write(inc.join("A.h"), "#include \"B.h\"\n").unwrap();
        srcio::own_write(inc.join("B.h"), "int b;\n").unwrap();
        srcio::own_write(d.join("t.cpp"), "#include \"A.h\"\n").unwrap();
        srcio::own_write(d.join("u.cpp"), "int u;\n").unwrap();
        let unit = |f: &str| Unit {
            source: d.join(f),
            stem: f.trim_end_matches(".cpp").to_string(),
            file: f.to_string(),
            cflags: vec![],
            includes: vec![inc.clone()],
            include_env: None,
            toolchain: "vc6".to_string(),
        };
        let units = vec![unit("t.cpp"), unit("u.cpp")];
        let header = inc.join("B.h").canonicalize().unwrap();
        let got: Vec<&str> = includers(&units, &header)
            .unwrap()
            .iter()
            .map(|u| u.stem.as_str())
            .collect();
        assert_eq!(got, ["t"]);
        let (text, dirs) = stage(
            units.first().unwrap(),
            Variant::Header {
                from: &inc.canonicalize().unwrap(),
                to: Path::new("/ovl"),
                src: None,
            },
        )
        .unwrap();
        assert_eq!(text, b"#include \"A.h\"\n");
        assert_eq!(dirs.first().map(PathBuf::as_path), Some(Path::new("/ovl")));
        std::fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn an_includer_past_the_walk_bound_is_refused_not_dropped() {
        let d = scratch("walk");
        let inc = d.join("include");
        std::fs::create_dir_all(&inc).unwrap();
        let mut tu = String::new();
        for k in 0..=MAX_WALK + 1 {
            srcio::own_write(inc.join(format!("h{k}.h")), "int x;\n").unwrap();
            tu.push_str(&format!("#include \"h{k}.h\"\n"));
        }
        srcio::own_write(inc.join("X.h"), "int y;\n").unwrap();
        srcio::own_write(d.join("t.cpp"), &tu).unwrap();
        let units = vec![Unit {
            source: d.join("t.cpp"),
            stem: "t".to_string(),
            file: "t.cpp".to_string(),
            cflags: vec![],
            includes: vec![inc.clone()],
            include_env: None,
            toolchain: "vc6".to_string(),
        }];
        let header = inc.join("X.h").canonicalize().unwrap();
        let e = includers(&units, &header).unwrap_err().to_string();
        assert!(
            e.contains("t.cpp: its quoted-include closure passes 4096 files"),
            "{e}"
        );
        std::fs::remove_dir_all(&d).ok();
    }
}
