use crate::hdrprice::{self, Variant};
use anyhow::{Context as _, Result, bail};
use msvc6_base::{bounded, srcio};
use msvc6_diff::comdat::Comdat;
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_diff::env::Env;
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::path::{Path, PathBuf};
use std::sync::Mutex;

pub const MAX_ATOMS: usize = 20;

pub const MAX_ATOM_FILE: u64 = 16 << 20;

pub const FUZZ: usize = 3;

pub const OPENS: &str = "=== ";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Hunk {
    pub start: usize,
    pub old: Vec<String>,
    pub new: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FilePatch {
    pub path: String,
    pub hunks: Vec<Hunk>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Atom {
    pub name: String,
    pub note: String,
    pub files: Vec<FilePatch>,
}

#[must_use]
pub fn rel_path(p: &str, roots: &[String]) -> String {
    let p = p.split('\t').next().unwrap_or(p).trim();
    for r in roots {
        let marker = format!("{}/", r.trim_end_matches('/'));
        if let Some(i) = p.find(&marker) {
            return p.get(i + marker.len()..).unwrap_or(p).to_string();
        }
    }
    p.strip_prefix("a/")
        .or_else(|| p.strip_prefix("b/"))
        .unwrap_or(p)
        .to_string()
}

#[must_use]
pub fn outside_trees(atoms: &[Atom], trees: &[String]) -> Vec<String> {
    let comps = |p: &str| -> Vec<String> {
        p.split(['/', '\\'])
            .filter(|c| !c.is_empty() && *c != ".")
            .map(str::to_string)
            .collect()
    };
    let roots: Vec<Vec<String>> = trees.iter().map(|t| comps(t)).collect();
    let mut out = Vec::new();
    for a in atoms {
        for f in &a.files {
            let p = comps(&f.path);
            if !roots.iter().any(|r| p.len() > r.len() && p.starts_with(r)) {
                out.push(format!("{}: {}", a.name, f.path));
            }
        }
    }
    out
}

pub fn confined(p: &str) -> Result<()> {
    let outside = p.trim().is_empty()
        || p.starts_with(['/', '\\'])
        || p.as_bytes().get(1) == Some(&b':')
        || Path::new(p).is_absolute()
        || p.split(['/', '\\']).any(|c| c == "..");
    if outside {
        bail!(
            "`{p}' is not a path inside the tree: it is empty, absolute, names a drive or climbs with `..'"
        );
    }
    Ok(())
}

pub fn write_inside(base: &Path, rel: &str, text: &str, unlink: bool) -> Result<PathBuf> {
    confined(rel)?;
    let top = base
        .canonicalize()
        .with_context(|| format!("resolving {}", base.display()))?;
    let q = base.join(rel);
    let parent = q.parent().unwrap_or(base);
    let held = parent
        .ancestors()
        .find(|a| a.exists())
        .unwrap_or(base)
        .canonicalize()
        .with_context(|| format!("resolving {}", parent.display()))?;
    if !held.starts_with(&top) {
        bail!(
            "{rel}: its directory resolves to {}, outside {}",
            held.display(),
            top.display()
        );
    }
    std::fs::create_dir_all(parent).with_context(|| format!("creating {}", parent.display()))?;
    let dir = parent
        .canonicalize()
        .with_context(|| format!("resolving {}", parent.display()))?;
    if !dir.starts_with(&top) {
        bail!(
            "{rel}: its directory resolves to {}, outside {}",
            dir.display(),
            top.display()
        );
    }
    if std::fs::symlink_metadata(&q).is_ok_and(|m| m.file_type().is_symlink()) {
        bail!("{rel}: a symbolic link, which a write would follow out of the tree");
    }
    if unlink {
        let _ = std::fs::remove_file(&q);
    }
    srcio::write(&q, text).with_context(|| format!("writing {}", q.display()))?;
    Ok(q)
}

fn hunk_start(h: &str) -> Result<usize> {
    let old = h
        .split_whitespace()
        .nth(1)
        .and_then(|s| s.strip_prefix('-'))
        .with_context(|| format!("bad hunk header: {h}"))?;
    old.split(',')
        .next()
        .unwrap_or("0")
        .parse()
        .with_context(|| format!("bad hunk header: {h}"))
}

pub fn parse_atoms(text: &str, roots: &[String]) -> Result<Vec<Atom>> {
    let mut atoms: Vec<Atom> = Vec::new();
    for l in text.lines() {
        if let Some(rest) = l.strip_prefix(OPENS) {
            let mut w = rest.trim().splitn(2, char::is_whitespace);
            let name = w.next().unwrap_or_default().to_string();
            if name.is_empty() || atoms.iter().any(|a| a.name == name) {
                bail!("atom name missing or repeated: {l}");
            }
            atoms.push(Atom {
                name,
                note: w.next().unwrap_or_default().trim().to_string(),
                files: Vec::new(),
            });
            continue;
        }
        let Some(a) = atoms.last_mut() else {
            if l.trim().is_empty() || l.starts_with('#') {
                continue;
            }
            bail!("text before the first `{OPENS}NAME' line: {l}");
        };
        if let Some(p) = l.strip_prefix("+++ ") {
            let path = rel_path(p, roots);
            confined(&path).with_context(|| format!("{}: {l}", a.name))?;
            a.files.push(FilePatch {
                path,
                hunks: Vec::new(),
            });
        } else if l.starts_with("--- ")
            || l.starts_with("diff ")
            || l.starts_with("index ")
            || l.starts_with('\\')
        {
        } else if l.starts_with("@@") {
            let f = a
                .files
                .last_mut()
                .with_context(|| format!("{}: hunk before a +++ line", a.name))?;
            f.hunks.push(Hunk {
                start: hunk_start(l)?,
                old: Vec::new(),
                new: Vec::new(),
            });
        } else if let Some(h) = a.files.last_mut().and_then(|f| f.hunks.last_mut()) {
            let body = l.get(1..).unwrap_or_default().to_string();
            match l.chars().next() {
                Some(' ') => {
                    h.old.push(body.clone());
                    h.new.push(body);
                }
                None => {
                    h.old.push(String::new());
                    h.new.push(String::new());
                }
                Some('-') => h.old.push(body),
                Some('+') => h.new.push(body),
                _ => bail!("{}: unreadable diff line: {l}", a.name),
            }
        }
    }
    if let Some(a) = atoms.iter().find(|a| a.files.is_empty()) {
        bail!("atom {} carries no patch", a.name);
    }
    Ok(atoms)
}

fn locate<'h>(
    lines: &[String],
    h: &'h Hunk,
    want: i64,
) -> Option<(usize, &'h [String], &'h [String])> {
    let common = |a: &[String], b: &[String]| a.iter().zip(b).take_while(|(x, y)| x == y).count();
    let lead = common(&h.old, &h.new);
    let tail = h
        .old
        .iter()
        .rev()
        .zip(h.new.iter().rev())
        .take_while(|(x, y)| x == y)
        .count();
    for fuzz in 0..=FUZZ {
        if fuzz > lead.min(tail) || 2 * fuzz >= h.old.len() {
            break;
        }
        let old = h.old.get(fuzz..h.old.len() - fuzz)?;
        let new = h.new.get(fuzz..h.new.len() - fuzz)?;
        let target = want + i64::try_from(fuzz).unwrap_or(0);
        let at = (0..=lines.len().saturating_sub(old.len()))
            .filter(|&p| lines.get(p..p + old.len()) == Some(old))
            .min_by_key(|&p| (i64::try_from(p).unwrap_or(0) - target).abs());
        if let Some(p) = at {
            return Some((p, old, new));
        }
    }
    None
}

pub fn apply_hunks(text: &str, hunks: &[Hunk]) -> Result<String> {
    let trailing = text.ends_with('\n');
    let mut lines: Vec<String> = text.lines().map(str::to_string).collect();
    let mut offset: i64 = 0;
    for h in hunks {
        let stated = i64::try_from(h.start).unwrap_or(0) - i64::from(!h.old.is_empty());
        let want = stated + offset;
        let (pos, old, new) = if h.old.is_empty() {
            (
                usize::try_from(want).unwrap_or(0).min(lines.len()),
                h.old.as_slice(),
                h.new.as_slice(),
            )
        } else {
            locate(&lines, h, want).with_context(|| {
                format!(
                    "hunk at line {} does not apply (first expected line: {:?})",
                    h.start,
                    h.old.first()
                )
            })?
        };
        lines.splice(pos..pos + old.len(), new.iter().cloned());
        let fuzz = (h.old.len() - old.len()) / 2;
        offset = i64::try_from(pos).unwrap_or(0) - i64::try_from(fuzz).unwrap_or(0) - stated
            + i64::try_from(h.new.len()).unwrap_or(0)
            - i64::try_from(h.old.len()).unwrap_or(0);
    }
    let mut out = lines.join("\n");
    if trailing {
        out.push('\n');
    }
    Ok(out)
}

pub fn patched(root: &Path, atoms: &[Atom], combo: &[usize]) -> Result<BTreeMap<String, String>> {
    let mut out: BTreeMap<String, String> = BTreeMap::new();
    for a in combo.iter().filter_map(|&i| atoms.get(i)) {
        for f in &a.files {
            let cur = match out.remove(&f.path) {
                Some(t) => t,
                None => bounded::source(&root.join(&f.path), hdrprice::MAX_SOURCE)
                    .with_context(|| format!("{}: {}", a.name, f.path))?,
            };
            let t =
                apply_hunks(&cur, &f.hunks).with_context(|| format!("{}: {}", a.name, f.path))?;
            out.insert(f.path.clone(), t);
        }
    }
    Ok(out)
}

pub fn combos(n: usize, required: &[usize], max: Option<usize>) -> Result<Vec<Vec<usize>>> {
    let free: Vec<usize> = (0..n).filter(|i| !required.contains(i)).collect();
    if free.len() > MAX_ATOMS {
        bail!(
            "{} free atom(s): at most {MAX_ATOMS} (2^{MAX_ATOMS} combinations); name more of them --require",
            free.len()
        );
    }
    let cap = max.unwrap_or(free.len()).min(free.len());
    let mut out: Vec<Vec<usize>> = Vec::new();
    for mask in 0u64..(1u64 << free.len()) {
        if usize::try_from(mask.count_ones()).unwrap_or(usize::MAX) > cap {
            continue;
        }
        let mut c: Vec<usize> = required.to_vec();
        c.extend(
            free.iter()
                .enumerate()
                .filter(|(k, _)| mask >> k & 1 == 1)
                .map(|(_, &i)| i),
        );
        c.sort_unstable();
        out.push(c);
    }
    out.sort_by_key(|c| (c.len(), c.clone()));
    Ok(out)
}

pub fn link_tree(from: &Path, to: &Path) -> Result<()> {
    std::fs::create_dir_all(to).with_context(|| format!("creating {}", to.display()))?;
    for e in std::fs::read_dir(from).with_context(|| format!("reading {}", from.display()))? {
        let p = e?.path();
        let q = to.join(p.file_name().unwrap_or_default());
        let m = std::fs::symlink_metadata(&p)?;
        if m.is_dir() {
            link_tree(&p, &q)?;
        } else if m.is_file() && std::fs::hard_link(&p, &q).is_err() {
            std::fs::copy(&p, &q).with_context(|| format!("copying {}", p.display()))?;
        }
    }
    Ok(())
}

#[must_use]
pub fn closure_key(src: &Path, dirs: &[PathBuf]) -> u64 {
    use std::hash::{Hash as _, Hasher as _};
    let mut h = std::collections::hash_map::DefaultHasher::new();
    let mut seen = BTreeSet::new();
    let mut stack = vec![src.to_path_buf()];
    while let Some(f) = stack.pop() {
        if seen.len() > hdrprice::MAX_WALK {
            break;
        }
        let Ok(text) = bounded::source(&f, hdrprice::MAX_SOURCE) else {
            continue;
        };
        f.file_name().hash(&mut h);
        text.hash(&mut h);
        let here = f.parent().map(Path::to_path_buf).unwrap_or_default();
        for name in hdrprice::quoted_includes(&text) {
            let Some(p) = std::iter::once(&here)
                .chain(dirs)
                .find_map(|d| crate::c1price::find_in(d, &name))
            else {
                continue;
            };
            if seen.insert(p.clone()) {
                stack.push(p);
            }
        }
    }
    h.finish()
}

#[must_use]
pub fn fingerprint(c: &Comdat) -> Vec<u8> {
    let mut k: Vec<u8> = c.symbol.clone().unwrap_or_default().into_bytes();
    k.push(0);
    k.extend(&c.bytes);
    for r in &c.relocations {
        k.push(0);
        k.extend(hdrprice::unnumbered(&r.symbol).into_bytes());
    }
    k
}

pub type Seated = BTreeMap<String, (u32, usize, Vec<u8>)>;

pub type Moves = Vec<(String, u32, usize, usize)>;

#[derive(Debug, Clone, Default)]
pub struct Res {
    pub combo: Vec<usize>,
    pub moved: BTreeMap<String, (u32, usize, usize)>,
    pub failed: Vec<String>,
}

fn signed(now: usize, before: usize) -> i64 {
    i64::try_from(now).unwrap_or(i64::MAX) - i64::try_from(before).unwrap_or(i64::MAX)
}

#[must_use]
pub fn short_sym(s: &str) -> &str {
    let s = s.strip_prefix('?').unwrap_or(s);
    s.split("@@").next().unwrap_or(s)
}

impl Res {
    #[must_use]
    pub fn delta(&self) -> i64 {
        self.moved.values().map(|&(_, a, b)| signed(b, a)).sum()
    }

    #[must_use]
    pub fn risers(&self) -> usize {
        self.moved.values().filter(|&&(_, a, b)| b > a).count()
    }

    #[must_use]
    pub fn changes(&self) -> String {
        self.moved
            .iter()
            .map(|(k, &(r, a, b))| format!("{r:#x} {} {a}->{b}", short_sym(k)))
            .collect::<Vec<_>>()
            .join("  ")
    }
}

#[must_use]
pub fn names(atoms: &[Atom], combo: &[usize]) -> String {
    if combo.is_empty() {
        return "(none)".to_string();
    }
    combo
        .iter()
        .filter_map(|&i| atoms.get(i).map(|a| a.name.as_str()))
        .collect::<Vec<_>>()
        .join("+")
}

pub fn indices(atoms: &[Atom], want: &[String]) -> Result<Vec<usize>> {
    want.iter()
        .map(|w| {
            atoms
                .iter()
                .position(|a| &a.name == w)
                .with_context(|| format!("no atom named {w}"))
        })
        .collect()
}

#[derive(Debug)]
pub struct Opts<'a> {
    pub label: &'a str,
    pub root: &'a Path,
    pub trees: &'a [String],
    pub max: Option<usize>,
    pub require: &'a [String],
    pub near: usize,
    pub jobs: usize,
    pub work: &'a Path,
}

#[derive(Debug, Default)]
pub struct Search {
    pub lines: Vec<String>,
    pub tsv: String,
    pub notes: Vec<String>,
    pub ok: usize,
    pub failed: usize,
}

fn seated_of(env: &Env, built: &hdrprice::Built) -> Result<Seated> {
    let seats = msvc6_diff::seat::seat_map(env, std::slice::from_ref(&built.obj), 1).seats;
    let mut m = Seated::new();
    for (rva, (obj, c)) in seats {
        let Some(sym) = c.symbol.clone() else {
            continue;
        };
        let Some(((_, e), _)) = hdrprice::figure(env, &obj, &c, rva) else {
            continue;
        };
        m.insert(sym, (rva, e, fingerprint(&c)));
    }
    Ok(m)
}

#[allow(clippy::too_many_lines)]
pub fn search(
    c: &Compiler,
    env: &Env,
    units: &[&Unit],
    atoms: &[Atom],
    o: &Opts<'_>,
) -> Result<Search> {
    let stray = outside_trees(atoms, o.trees);
    if !stray.is_empty() {
        bail!(
            "{} atom file(s) lie outside every tree copied for a compile ({}), so their edits would never be \
             compiled; name a --tree that holds them:\n  {}",
            stray.len(),
            o.trees.join(", "),
            stray.join("\n  ")
        );
    }
    let required = indices(atoms, o.require)?;
    let all = combos(atoms.len(), &required, o.max)?;
    for (i, a) in atoms.iter().enumerate() {
        patched(o.root, atoms, &[i]).with_context(|| format!("atom {}", a.name))?;
    }
    let trees: Vec<(String, PathBuf)> = o
        .trees
        .iter()
        .map(|t| {
            let p = o.root.join(t);
            let abs = p.canonicalize().unwrap_or(p);
            (t.clone(), abs)
        })
        .collect();
    let root = hdrprice::run_root(o.work, "header-combo", o.label);
    let _scratch = hdrprice::scratch_root(&root)?;
    let base = msvc6_diff::par::map(units, o.jobs, |u| {
        hdrprice::compile(c, u, &root.join("base").join(&u.stem), Variant::Base)
    });
    let mut seated: BTreeMap<String, Seated> = BTreeMap::new();
    for (u, b) in units.iter().zip(base) {
        let b =
            b.with_context(|| format!("{}: the unpatched compile fails", u.source.display()))?;
        seated.insert(u.stem.clone(), seated_of(env, &b)?);
    }
    let cache: Mutex<HashMap<(u32, Vec<u8>), usize>> = Mutex::new(HashMap::new());
    let compiled: Mutex<HashMap<(String, u64), Moves>> = Mutex::new(HashMap::new());
    let run_one = |k: usize, combo: &Vec<usize>| -> Res {
        let mut res = Res {
            combo: combo.clone(),
            ..Res::default()
        };
        let files = match patched(o.root, atoms, combo) {
            Ok(f) => f,
            Err(e) => {
                res.failed.push(format!("CONFLICT {e:#}"));
                return res;
            }
        };
        if files.is_empty() {
            return res;
        }
        let ovl = root.join(format!("c{k:05}"));
        let made = trees
            .iter()
            .try_for_each(|(rel, from)| link_tree(from, &ovl.join(rel)))
            .and_then(|()| {
                std::fs::create_dir_all(&ovl)
                    .with_context(|| format!("creating {}", ovl.display()))?;
                for (p, text) in &files {
                    write_inside(&ovl, p, text, true)?;
                }
                Ok(())
            });
        if let Err(e) = made {
            res.failed.push(format!("overlay: {e:#}"));
            return res;
        }
        let map: Vec<(PathBuf, PathBuf)> = trees
            .iter()
            .map(|(rel, from)| {
                let to = ovl.join(rel);
                (from.clone(), to.canonicalize().unwrap_or(to))
            })
            .collect();
        let touched: Vec<PathBuf> = files
            .keys()
            .map(|p| {
                let q = ovl.join(p);
                q.canonicalize().unwrap_or(q)
            })
            .collect();
        let swap = |d: &PathBuf| -> PathBuf {
            map.iter()
                .find(|(f, _)| f == d)
                .map_or_else(|| d.clone(), |(_, t)| t.clone())
        };
        let mut hit: Vec<&&Unit> = Vec::new();
        for u in units {
            let own = u
                .source
                .canonicalize()
                .ok()
                .and_then(|p| p.parent().map(Path::to_path_buf))
                .map(|p| swap(&p));
            let dirs: Vec<PathBuf> = u
                .includes
                .iter()
                .map(|d| swap(&d.canonicalize().unwrap_or_else(|_| d.clone())))
                .chain(own)
                .collect();
            let staged = dirs
                .iter()
                .map(|d| d.join(&u.file))
                .find(|p| p.is_file())
                .unwrap_or_else(|| u.source.clone());
            let mut reached = false;
            for h in &touched {
                if *h == staged {
                    reached = true;
                    break;
                }
                match hdrprice::reaches(&staged, h, &dirs) {
                    Ok(true) => {
                        reached = true;
                        break;
                    }
                    Ok(false) => {}
                    Err(e) => {
                        res.failed.push(format!("{} {e:#}", u.stem));
                        break;
                    }
                }
            }
            if reached {
                hit.push(u);
            }
        }
        if !res.failed.is_empty() {
            return res;
        }
        let per: Vec<(String, Result<Moves>)> = msvc6_diff::par::map(&hit, o.jobs, |u| {
            let own = u
                .source
                .canonicalize()
                .ok()
                .and_then(|p| p.parent().map(Path::to_path_buf))
                .map(|p| swap(&p));
            let dirs: Vec<PathBuf> = u
                .includes
                .iter()
                .map(|d| swap(&d.canonicalize().unwrap_or_else(|_| d.clone())))
                .chain(own)
                .collect();
            let text = dirs.iter().map(|d| d.join(&u.file)).find(|p| p.is_file());
            let key = text
                .as_deref()
                .map(|t| (u.stem.clone(), closure_key(t, &dirs)));
            let cached = key
                .as_ref()
                .and_then(|k| compiled.lock().ok().and_then(|m| m.get(k).cloned()));
            if let Some(m) = cached {
                return (u.stem.clone(), Ok(m));
            }
            let v = Variant::Overlay {
                map: &map,
                text: text.as_deref(),
            };
            let r = hdrprice::compile(c, u, &ovl.join("obj").join(&u.stem), v).and_then(|b| {
                let base = seated
                    .get(&u.stem)
                    .with_context(|| format!("{}: no unpatched compile", u.stem))?;
                let mut moved = Vec::new();
                for cd in msvc6_diff::comdat::text(&b.obj)? {
                    let Some(sym) = cd.symbol.clone() else {
                        continue;
                    };
                    let Some(&(rva, e0, ref f0)) = base.get(&sym) else {
                        continue;
                    };
                    let f = fingerprint(&cd);
                    if f == *f0 {
                        continue;
                    }
                    let ck = (rva, f);
                    let hit = cache.lock().ok().and_then(|m| m.get(&ck).copied());
                    let e1 = match hit {
                        Some(e) => e,
                        None => {
                            let e = hdrprice::figure(env, &b.obj, &cd, rva)
                                .map(|((_, e), _)| e)
                                .with_context(|| format!("{sym}: no score at {rva:#x}"))?;
                            if let Ok(mut m) = cache.lock() {
                                m.insert(ck, e);
                            }
                            e
                        }
                    };
                    if e1 != e0 {
                        moved.push((sym, rva, e0, e1));
                    }
                }
                Ok(moved)
            });
            if let (Some(k), Ok(m)) = (key, &r)
                && let Ok(mut lock) = compiled.lock()
            {
                lock.insert(k, m.clone());
            }
            (u.stem.clone(), r)
        });
        for (stem, r) in per {
            match r {
                Ok(v) => {
                    for (sym, rva, a, b) in v {
                        res.moved.insert(sym, (rva, a, b));
                    }
                }
                Err(e) => res.failed.push(format!("{stem}: {e:#}")),
            }
        }
        let _ = std::fs::remove_dir_all(&ovl);
        res
    };
    let mut s = Search {
        tsv: String::from("combo\tdelta\trisers\tfailed\tchanges\n"),
        ..Search::default()
    };
    s.notes.push(format!(
        "{} atom(s), {} combination(s), {} TU(s) screened",
        atoms.len(),
        all.len(),
        units.len()
    ));
    let indexed: Vec<(usize, Vec<usize>)> = all.into_iter().enumerate().collect();
    let mut results: Vec<Res> = msvc6_diff::par::map(&indexed, o.jobs, |(k, c)| run_one(*k, c));
    results.sort_by_key(|r| (r.risers(), r.delta(), r.combo.len()));
    for r in &results {
        s.tsv.push_str(&format!(
            "{}\t{}\t{}\t{}\t{}\n",
            names(atoms, &r.combo),
            r.delta(),
            r.risers(),
            r.failed.join("; "),
            r.changes()
        ));
    }
    let ok: Vec<&Res> = results
        .iter()
        .filter(|r| r.failed.is_empty() && r.risers() == 0 && !r.moved.is_empty())
        .collect();
    s.ok = ok.len();
    for r in &ok {
        s.lines.push(format!(
            "NO-RISER {:+} [{}] {}",
            r.delta(),
            names(atoms, &r.combo),
            r.changes()
        ));
    }
    for r in results
        .iter()
        .filter(|r| r.failed.is_empty() && r.risers() > 0)
        .take(o.near)
    {
        s.lines.push(format!(
            "near {:+} risers {} [{}] {}",
            r.delta(),
            r.risers(),
            names(atoms, &r.combo),
            r.changes()
        ));
    }
    let failed: Vec<&Res> = results.iter().filter(|r| !r.failed.is_empty()).collect();
    s.failed = failed.len();
    for r in failed.iter().take(5) {
        s.lines.push(format!(
            "FAILED [{}] {}",
            names(atoms, &r.combo),
            r.failed.join("; ")
        ));
    }
    let neutral = results
        .iter()
        .filter(|r| r.failed.is_empty() && r.moved.is_empty())
        .count();
    s.lines.push(format!(
        "{} combination(s): {} with no riser and a move, {neutral} move nothing, {} failed; atoms: {}",
        results.len(),
        s.ok,
        s.failed,
        atoms
            .iter()
            .map(|a| if a.note.is_empty() {
                a.name.clone()
            } else {
                format!("{} ({})", a.name, a.note)
            })
            .collect::<Vec<_>>()
            .join(", ")
    ));
    Ok(s)
}

pub fn read_atoms(p: &Path, roots: &[String]) -> Result<Vec<Atom>> {
    let text =
        bounded::source(p, MAX_ATOM_FILE).with_context(|| format!("reading {}", p.display()))?;
    parse_atoms(&text, roots)
}

pub fn apply(root: &Path, atoms: &[Atom], want: &[String]) -> Result<Vec<String>> {
    let c = indices(atoms, want)?;
    let mut out = Vec::new();
    for (p, t) in patched(root, atoms, &c)? {
        write_inside(root, &p, &t, false)?;
        out.push(format!("wrote {p}"));
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_diff::comdat::Reloc;

    const ATOMS: &str = "# comment\n=== DROP drop b\ndiff --git a/project/game/include/A.h b/project/game/include/A.h\nindex 1..2 \
                         100644\n--- a/project/game/include/A.h\n+++ b/project/game/include/A.h\n@@ -1,3 +1,2 @@\n a\n-b\n \
                         c\n=== ADD\n--- a/include/A.h\n+++ b/include/A.h\n@@ -3,2 +3,3 @@\n c\n+x\n d\n";

    fn roots() -> Vec<String> {
        vec!["project/game".to_string()]
    }

    #[test]
    fn atoms_parse_names_notes_and_project_relative_paths() {
        let a = parse_atoms(ATOMS, &roots()).unwrap();
        assert_eq!(a.len(), 2);
        assert_eq!((a[0].name.as_str(), a[0].note.as_str()), ("DROP", "drop b"));
        assert_eq!(a[0].files[0].path, "include/A.h");
        assert_eq!(a[1].files[0].path, "include/A.h");
        assert_eq!(
            a[0].files[0].hunks[0],
            Hunk {
                start: 1,
                old: vec!["a".into(), "b".into(), "c".into()],
                new: vec!["a".into(), "c".into()],
            }
        );
        assert!(parse_atoms("=== X\n=== X\n", &roots()).is_err());
        assert!(parse_atoms("=== X\n", &roots()).is_err());
        assert!(parse_atoms("stray\n", &roots()).is_err());
    }

    #[test]
    fn an_atom_file_outside_every_copied_tree_is_named() {
        let a = parse_atoms(ATOMS, &roots()).unwrap();
        assert!(outside_trees(&a, &["include".to_string()]).is_empty());
        assert!(outside_trees(&a, &["./include/".to_string()]).is_empty());
        assert_eq!(
            outside_trees(&a, &["src".to_string()]),
            ["DROP: include/A.h", "ADD: include/A.h"]
        );
        assert_eq!(
            outside_trees(&a, &["include/A.h".to_string(), "inc".to_string()]).len(),
            2
        );
    }

    #[test]
    fn atoms_written_against_one_tree_compose_in_either_subset() {
        let a = parse_atoms(ATOMS, &roots()).unwrap();
        let base = "a\nb\nc\nd\n";
        assert_eq!(
            apply_hunks(base, &a[0].files[0].hunks).unwrap(),
            "a\nc\nd\n"
        );
        assert_eq!(
            apply_hunks(base, &a[1].files[0].hunks).unwrap(),
            "a\nb\nc\nx\nd\n"
        );
        let one = apply_hunks(base, &a[0].files[0].hunks).unwrap();
        assert_eq!(
            apply_hunks(&one, &a[1].files[0].hunks).unwrap(),
            "a\nc\nx\nd\n"
        );
        assert!(apply_hunks("q\n", &a[0].files[0].hunks).is_err());
    }

    #[test]
    fn the_nearest_of_repeated_contexts_is_patched_and_fuzz_takes_a_changed_outer_line() {
        let h = Hunk {
            start: 4,
            old: vec!["x".into()],
            new: vec!["y".into()],
        };
        assert_eq!(
            apply_hunks("x\n.\n.\nx\n.\n", &[h]).unwrap(),
            "x\n.\n.\ny\n.\n"
        );
        let h = Hunk {
            start: 1,
            old: vec!["a".into(), "b".into(), "c".into(), "d".into(), "e".into()],
            new: vec!["a".into(), "b".into(), "d".into(), "e".into()],
        };
        assert_eq!(
            apply_hunks("A\nb\nc\nd\ne\nf\n", std::slice::from_ref(&h)).unwrap(),
            "A\nb\nd\ne\nf\n"
        );
        assert!(apply_hunks("a\nb\nC\nd\ne\n", &[h]).is_err());
    }

    #[test]
    fn combinations_honour_required_atoms_and_the_cap() {
        assert_eq!(
            combos(3, &[1], Some(1)).unwrap(),
            vec![vec![1], vec![0, 1], vec![1, 2]]
        );
        assert_eq!(combos(3, &[], None).unwrap().len(), 8);
        assert_eq!(combos(3, &[], None).unwrap()[0], Vec::<usize>::new());
        let keep: Vec<usize> = (0..=MAX_ATOMS).collect();
        assert!(
            combos(MAX_ATOMS + 2, &keep, None).is_ok(),
            "one free atom is enumerable"
        );
        assert!(
            combos(MAX_ATOMS + 2, &keep[..1], None).is_err(),
            "{} free atoms are refused",
            MAX_ATOMS + 1
        );
    }

    #[test]
    fn a_patched_combination_leaves_the_tree_alone_until_apply() {
        let d = std::env::temp_dir().join(format!("hc-{}", std::process::id()));
        std::fs::create_dir_all(d.join("include")).unwrap();
        srcio::write(d.join("include/A.h"), "a\nb\nc\nd\n").unwrap();
        let a = parse_atoms(ATOMS, &roots()).unwrap();
        let p = patched(&d, &a, &[0, 1]).unwrap();
        assert_eq!(p["include/A.h"], "a\nc\nx\nd\n");
        assert_eq!(srcio::read(d.join("include/A.h")).unwrap(), "a\nb\nc\nd\n");
        let wrote = apply(&d, &a, &["DROP".to_string()]).unwrap();
        assert_eq!(wrote, ["wrote include/A.h"]);
        assert_eq!(srcio::read(d.join("include/A.h")).unwrap(), "a\nc\nd\n");
        std::fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn an_addend_alone_changes_the_fingerprint_and_a_symbol_number_does_not() {
        let mk = |addend: u8, sym: &str| Comdat {
            symbol: Some("?F@@YAXXZ".into()),
            bytes: vec![0xa1, addend, 0, 0, 0],
            relocated: vec![false, true, true, true, true],
            relocations: vec![Reloc {
                at: 1,
                symbol: sym.into(),
                kind: 6,
                external: true,
                global: true,
                target_section: None,
                target_index: None,
                target_offset: 0,
            }],
            ..Comdat::default()
        };
        assert_ne!(fingerprint(&mk(0x10, "_g")), fingerprint(&mk(0x14, "_g")));
        assert_eq!(fingerprint(&mk(0, "$SG12")), fingerprint(&mk(0, "$SG99")));
        assert!(!hdrprice::same(&mk(0x10, "_g"), &mk(0x14, "_g")));
        assert_eq!(short_sym("?Draw@Widget@@QAEXXZ"), "Draw@Widget");
        assert_eq!(short_sym("_run"), "_run");
    }

    #[test]
    fn a_hard_linked_tree_copies_every_file_and_a_closure_key_follows_an_include() {
        let d = std::env::temp_dir().join(format!("hc-tree-{}", std::process::id()));
        let (from, to) = (d.join("a"), d.join("b"));
        std::fs::create_dir_all(from.join("sub")).unwrap();
        srcio::own_write(from.join("x.h"), b"#include \"sub/y.h\"\n").unwrap();
        srcio::own_write(from.join("sub/y.h"), b"int y;\n").unwrap();
        link_tree(&from, &to).unwrap();
        assert_eq!(srcio::read(to.join("sub/y.h")).unwrap(), "int y;\n");
        let dirs = vec![to.clone()];
        let before = closure_key(&to.join("x.h"), &dirs);
        srcio::own_write(to.join("sub/y.h"), b"int z;\n").unwrap();
        assert_ne!(closure_key(&to.join("x.h"), &dirs), before);
        std::fs::remove_dir_all(&d).ok();
    }
}
