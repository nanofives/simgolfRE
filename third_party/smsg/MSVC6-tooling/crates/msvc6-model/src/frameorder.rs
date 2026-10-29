use crate::bss::{ident_key, longest_ordered};
use anyhow::{Context as _, Result};
use msvc6_base::srcio;
use msvc6_image::archive::Archive;
use msvc6_image::coff::Object;
use std::path::{Path, PathBuf};

pub const MAX_DEPTH: usize = 32;

pub const MAX_INPUT: u64 = 1 << 30;

pub const MAX_LOCALS: usize = crate::bss::MAX_VARS;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Local {
    pub name: String,
    pub off: i32,
    pub scope: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Proc {
    pub name: String,
    pub locals: Vec<Local>,
}

#[must_use]
pub fn bucket(name: &str) -> u32 {
    ident_key(name) & 0xf
}

fn u16_at(d: &[u8], i: usize) -> Option<u16> {
    Some(u16::from_le_bytes(
        d.get(i..i.checked_add(2)?)?.try_into().ok()?,
    ))
}

fn i32_at(d: &[u8], i: usize) -> Option<i32> {
    Some(i32::from_le_bytes(
        d.get(i..i.checked_add(4)?)?.try_into().ok()?,
    ))
}

fn pascal(d: &[u8], i: usize) -> Option<String> {
    let n = usize::from(*d.get(i)?);
    Some(srcio::tool_text(d.get(i + 1..i + 1 + n)?))
}

#[must_use]
pub fn procs_of_debug_s(d: &[u8]) -> Vec<Proc> {
    let mut out = Vec::new();
    let mut cur: Option<Proc> = None;
    let mut stack: Vec<usize> = Vec::new();
    let mut scopes = 0usize;
    let mut i = if matches!(d.get(..4), Some([1 | 2 | 4, 0, 0, 0])) {
        4
    } else {
        0
    };
    while i + 4 <= d.len() {
        let Some(len) = u16_at(d, i).map(usize::from) else {
            break;
        };
        let Some(ty) = u16_at(d, i + 2) else { break };
        let p = i + 4;
        let next = i + 2 + len;
        if len < 2 || next > d.len() {
            break;
        }
        match ty {
            0x100a | 0x100b | 0x0204 | 0x0205 => {
                if let Some(pr) = cur.take() {
                    out.push(pr);
                }
                let name_at = if ty >= 0x1000 { p + 35 } else { p + 33 };
                cur = Some(Proc {
                    name: pascal(d, name_at).unwrap_or_default(),
                    locals: Vec::new(),
                });
                stack = vec![0];
                scopes = 1;
            }
            0x0207 | 0x0208 | 0x0206 | 0x1105 | 0x1103 if cur.is_some() => {
                stack.push(scopes);
                scopes += 1;
            }
            0x0006 if cur.is_some() => {
                stack.pop();
                if stack.is_empty()
                    && let Some(pr) = cur.take()
                {
                    out.push(pr);
                }
            }
            0x1006 | 0x0200 if cur.is_some() => {
                let name_at = if ty == 0x1006 { p + 8 } else { p + 6 };
                if let (Some(off), Some(name), Some(pr)) =
                    (i32_at(d, p), pascal(d, name_at), cur.as_mut())
                {
                    pr.locals.push(Local {
                        name,
                        off,
                        scope: stack.last().copied().unwrap_or(0),
                    });
                }
            }
            _ => {}
        }
        i = next;
    }
    if let Some(pr) = cur.take() {
        out.push(pr);
    }
    out
}

#[must_use]
pub fn zi_of_debug_s(d: &[u8]) -> Option<bool> {
    let at = d.windows(4).position(|w| w == b"cmd\0")? + 4;
    let tail = d.get(at..)?;
    let end = tail.iter().position(|&c| c == 0).unwrap_or(tail.len());
    let cmd = srcio::tool_text(tail.get(..end)?);
    Some(cmd.split_whitespace().any(|w| w == "-ZI" || w == "/ZI"))
}

pub fn read_object(d: &[u8]) -> Result<(Option<bool>, Vec<Proc>)> {
    let f = Object::parse(d).context("not a COFF object")?;
    let mut zi = None;
    let mut procs = Vec::new();
    for s in &f.sections {
        if s.name != ".debug$S" {
            continue;
        }
        let data = f.section_data(s);
        zi = zi.or_else(|| zi_of_debug_s(data));
        procs.extend(procs_of_debug_s(data));
    }
    Ok((zi, procs))
}

fn frame(pr: &Proc) -> (Vec<&Local>, Option<&Local>) {
    let mut named = Vec::new();
    let mut this = None;
    for l in &pr.locals {
        if l.off >= 0 || l.name.starts_with('$') || l.name.starts_with("__$") {
            continue;
        }
        if l.name == "this" {
            this = Some(l);
        } else {
            named.push(l);
        }
    }
    (named, this)
}

fn hash_key(l: &Local) -> (usize, u32) {
    (l.scope, bucket(&l.name))
}

#[must_use]
pub fn hash_holds(pr: &Proc) -> bool {
    let (named, this) = frame(pr);
    if this.is_some_and(|t| named.iter().any(|l| l.off < t.off)) {
        return false;
    }
    let mut v: Vec<&Local> = named;
    v.sort_by_key(|l| std::cmp::Reverse(l.off));
    v.windows(2)
        .all(|w| matches!(w, [a, b] if hash_key(a) <= hash_key(b)))
}

#[must_use]
pub fn decl_holds(pr: &Proc) -> bool {
    let (named, this) = frame(pr);
    !this.is_some_and(|t| named.iter().any(|l| l.off > t.off))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Misfit {
    pub name: String,
    pub bucket: u32,
    pub lo: u32,
    pub hi: u32,
}

#[must_use]
pub fn misfits(pr: &Proc) -> Vec<Misfit> {
    let (named, _) = frame(pr);
    let mut v: Vec<&Local> = named;
    v.sort_by_key(|l| std::cmp::Reverse(l.off));
    let mut out = Vec::new();
    let mut scopes: Vec<usize> = v.iter().map(|l| l.scope).collect();
    scopes.sort_unstable();
    scopes.dedup();
    for s in scopes {
        let run: Vec<&Local> = v.iter().copied().filter(|l| l.scope == s).collect();
        let b: Vec<u32> = run.iter().map(|l| bucket(&l.name)).collect();
        let keep = longest_ordered(&b);
        for (k, l) in run.iter().enumerate() {
            if keep.contains(&k) {
                continue;
            }
            let lo = keep
                .iter()
                .filter(|&&j| j < k)
                .filter_map(|&j| b.get(j).copied())
                .max()
                .unwrap_or(0);
            let hi = keep
                .iter()
                .filter(|&&j| j > k)
                .filter_map(|&j| b.get(j).copied())
                .min()
                .unwrap_or(15);
            out.push(Misfit {
                name: l.name.clone(),
                bucket: b.get(k).copied().unwrap_or(0),
                lo,
                hi,
            });
        }
    }
    out
}

#[derive(Debug, Default)]
pub struct Inputs {
    pub objects: Vec<(String, Vec<u8>)>,
    pub skipped: Vec<String>,
}

fn read_bounded(p: &Path) -> Result<Vec<u8>> {
    srcio::read_bounded(p, MAX_INPUT).with_context(|| format!("reading {}", p.display()))
}

fn walk(p: &Path, depth: usize, follow: bool, out: &mut Inputs) -> Result<()> {
    let meta = if follow {
        std::fs::metadata(p)
    } else {
        std::fs::symlink_metadata(p)
    }
    .with_context(|| format!("reading {}", p.display()))?;
    if meta.is_dir() {
        if depth >= MAX_DEPTH {
            out.skipped.push(format!(
                "{}: deeper than {MAX_DEPTH} directories, not read",
                p.display()
            ));
            return Ok(());
        }
        let mut es: Vec<PathBuf> = std::fs::read_dir(p)
            .with_context(|| format!("reading {}", p.display()))?
            .flatten()
            .map(|e| e.path())
            .collect();
        es.sort();
        for e in es {
            walk(&e, depth + 1, false, out)?;
        }
        return Ok(());
    }
    if meta.file_type().is_symlink() {
        out.skipped
            .push(format!("{}: a link, not followed", p.display()));
        return Ok(());
    }
    let ext = p
        .extension()
        .map(|x| x.to_string_lossy().to_ascii_lowercase());
    match ext.as_deref() {
        Some("obj") => out
            .objects
            .push((p.display().to_string(), read_bounded(p)?)),
        Some("lib") => {
            let d = read_bounded(p)?;
            let a = Archive::parse(&d).with_context(|| p.display().to_string())?;
            for (m, body) in a.content() {
                if msvc6_image::coff::looks_like_object(body) {
                    out.objects
                        .push((format!("{}({})", p.display(), m.name), body.to_vec()));
                }
            }
        }
        _ => {}
    }
    Ok(())
}

pub fn collect(paths: &[PathBuf]) -> Result<Inputs> {
    let mut out = Inputs::default();
    for p in paths {
        walk(p, 0, true, &mut out)?;
    }
    Ok(out)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Model {
    Hash,
    Decl,
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Totals {
    pub objects: usize,
    pub hash: usize,
    pub hash_ok: usize,
    pub decl: usize,
    pub decl_ok: usize,
    pub decl_hashy: usize,
    pub trivial: usize,
    pub contradicted: usize,
    pub unreadable: usize,
    pub refused: usize,
}

pub fn report(
    objs: &[(String, Vec<u8>)],
    model: Option<Model>,
    verbose: bool,
) -> (Vec<String>, Totals) {
    let mut lines = Vec::new();
    let mut t = Totals {
        objects: objs.len(),
        ..Totals::default()
    };
    for (path, d) in objs {
        let Ok((zi, procs)) = read_object(d) else {
            t.unreadable += 1;
            continue;
        };
        let want_hash = match model {
            Some(Model::Hash) => true,
            Some(Model::Decl) => false,
            None => !zi.unwrap_or(false),
        };
        for pr in &procs {
            let (named, _) = frame(pr);
            if named.len() < 2 {
                t.trivial += 1;
                continue;
            }
            if named.len() > MAX_LOCALS {
                t.refused += 1;
                lines.push(format!(
                    "REFUSED  {}  {} local(s), more than the {MAX_LOCALS} the frame model reads  ({})",
                    pr.name,
                    named.len(),
                    path
                ));
                continue;
            }
            let hash_ok = hash_holds(pr);
            let decl_ok = decl_holds(pr);
            if want_hash {
                t.hash += 1;
                t.hash_ok += usize::from(hash_ok);
            } else {
                t.decl += 1;
                t.decl_ok += usize::from(decl_ok);
                t.decl_hashy += usize::from(hash_ok);
            }
            let fits = if want_hash { hash_ok } else { decl_ok };
            if !fits || verbose {
                lines.push(format!(
                    "{}  {}  {}  {} local(s)  HASH {}  ({})",
                    if fits { "FITS  " } else { "MISFIT" },
                    if want_hash { "hash" } else { "decl" },
                    pr.name,
                    named.len(),
                    if hash_ok { "holds" } else { "differs" },
                    path
                ));
                let (named, this) = frame(pr);
                let mut v: Vec<&Local> = named.into_iter().chain(this).collect();
                v.sort_by_key(|l| std::cmp::Reverse(l.off));
                let row: Vec<String> = v
                    .iter()
                    .map(|l| format!("{}@{}:s{}b{}", l.name, l.off, l.scope, bucket(&l.name)))
                    .collect();
                lines.push(format!("    top first: {}", row.join(" ")));
                if want_hash && !hash_ok {
                    for m in misfits(pr) {
                        t.contradicted += 1;
                        lines.push(format!(
                            "    {} hashes to bucket {}; its slot needs {}..={}",
                            m.name, m.bucket, m.lo, m.hi
                        ));
                    }
                }
            }
        }
    }
    lines.push(format!(
        "TOTAL {} object(s): HASH-domain (no /ZI) {}/{} function(s) hold; DECL-domain (/ZI) \
         {}/{} put `this` first, {} of them happen to be bucket-ordered too; {} with \
         fewer than 2 named locals; {} local(s) whose name the oracle contradicts",
        t.objects, t.hash_ok, t.hash, t.decl_ok, t.decl, t.decl_hashy, t.trivial, t.contradicted
    ));
    if t.refused > 0 {
        lines.push(format!(
            "REFUSED {} function(s) with more than {MAX_LOCALS} named locals: not judged",
            t.refused
        ));
    }
    (lines, t)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pr(locals: &[(&str, i32, usize)]) -> Proc {
        Proc {
            name: "t".into(),
            locals: locals
                .iter()
                .map(|&(name, off, scope)| Local {
                    name: name.into(),
                    off,
                    scope,
                })
                .collect(),
        }
    }

    #[test]
    fn single_letters_bucket_by_their_low_nibble() {
        assert_eq!(bucket("p"), 0);
        assert_eq!(bucket("a"), 1);
        assert_eq!(bucket("o"), 15);
    }

    #[test]
    fn the_toy_frames_follow_the_hash_and_the_zi_ones_declaration_order() {
        let t1 = pr(&[
            ("zeta", -24, 0),
            ("alpha", -16, 0),
            ("count", -28, 0),
            ("result", -8, 0),
            ("p", -4, 0),
            ("a", -12, 0),
            ("hi", -44, 0),
            ("buf", -20, 0),
            ("sw", -32, 0),
            ("dv", -40, 0),
        ]);
        assert!(hash_holds(&t1));
        assert!(misfits(&t1).is_empty());
        let n = pr(&[
            ("this", -44, 0),
            ("outer1", -16, 0),
            ("zz", -12, 0),
            ("inner1", -24, 1),
            ("a", -20, 1),
            ("deep", -28, 2),
            ("i", -8, 0),
            ("p", -32, 3),
            ("sib", -36, 4),
            ("late", -4, 0),
            ("dd", -40, 5),
        ]);
        assert!(hash_holds(&n));
        let g = pr(&[
            ("zeta", -4, 0),
            ("alpha", -8, 0),
            ("count", -12, 0),
            ("result", -16, 0),
            ("p", -20, 0),
            ("a", -24, 0),
            ("hi", -28, 0),
        ]);
        assert!(decl_holds(&g));
        assert!(!hash_holds(&g));
        let bad = pr(&[("p", -8, 0), ("a", -4, 0), ("zeta", -12, 0)]);
        let m = misfits(&bad);
        assert_eq!(m.len(), 1);
    }

    #[test]
    fn zi_is_read_off_the_command_line() {
        let mut d = vec![2, 0, 0, 0];
        d.extend_from_slice(b"cwd\0C:\\x\0cmd\0-nologo -MTd -GX -ZI -Od\0");
        assert_eq!(zi_of_debug_s(&d), Some(true));
        let mut e = vec![2, 0, 0, 0];
        e.extend_from_slice(b"cmd\0-nologo -MTd -GX -Zi -Od\0");
        assert_eq!(zi_of_debug_s(&e), Some(false));
        assert_eq!(zi_of_debug_s(&[2, 0, 0, 0]), None);
    }
}
