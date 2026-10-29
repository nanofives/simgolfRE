use crate::hdrprice::{
    self, Batch, Built, Job, Scratch, Variant, copy_headers, diff_tu, insert_pad, overlay_dir,
    run_root, scratch_root, search_dirs, sweep_chunk, tree_dir, work_dir,
};
use anyhow::{Context as _, Result};
use msvc6_base::{bounded, srcio};
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_diff::env::Env;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub const PERIOD: usize = 64;

pub type Fig = (usize, usize, usize);

pub type Curve = Vec<(usize, usize, usize, usize)>;

#[must_use]
pub fn pads_of(step: usize, given: &[usize]) -> Vec<usize> {
    let mut pads: Vec<usize> = if given.is_empty() {
        (0..PERIOD).step_by(step.max(1)).collect()
    } else {
        given.to_vec()
    };
    pads.push(0);
    pads.sort_unstable();
    pads.dedup();
    pads
}

#[must_use]
pub fn best(base: usize, curve: &Curve) -> (usize, Vec<usize>) {
    let low = curve.iter().map(|c| c.2).min().unwrap_or(base).min(base);
    let at = curve.iter().filter(|c| c.2 == low).map(|c| c.0).collect();
    (low, at)
}

#[must_use]
pub fn pad_list(at: &[usize]) -> String {
    let mut v: Vec<String> = at
        .iter()
        .take(8)
        .map(|p| format!("+{p}/-{}", PERIOD.saturating_sub(*p)))
        .collect();
    if at.len() > 8 {
        v.push(format!("(+{} more of {})", at.len() - 8, PERIOD - 1));
    }
    format!("pad {}", v.join(" "))
}

#[must_use]
pub fn best_addend(e0: usize, a0: usize, curve: &Curve) -> (usize, Vec<usize>) {
    let held = || curve.iter().filter(|c| c.2 <= e0);
    let low = held().map(|c| c.3).min().unwrap_or(a0).min(a0);
    let at = if low < a0 {
        held().filter(|c| c.3 == low).map(|c| c.0).collect()
    } else {
        Vec::new()
    };
    (low, at)
}

#[must_use]
pub fn curve_rows(tu: &str, rva: u32, sym: &str, base: Fig, curve: &Curve) -> String {
    std::iter::once((0, base.0, base.1, base.2))
        .chain(curve.iter().copied())
        .map(|(n, d, e, a)| format!("{tu}\t{rva:#x}\t{sym}\t{n}\t{d}\t{e}\t{a}\n"))
        .collect()
}

pub fn base_rows(
    env: &Env,
    stem: &str,
    b: &Built,
    moved: &BTreeMap<String, (u32, Fig, Curve)>,
    npads: usize,
) -> Result<Vec<String>> {
    let seated = hdrprice::seats(env, &b.obj);
    let mut rows = Vec::new();
    for c in msvc6_diff::comdat::text(&b.obj)? {
        let k = hdrprice::key(&c);
        let Some(&rva) = seated.get(&k) else { continue };
        let Some(((d, e), a)) = hdrprice::figure(env, &b.obj, &c, rva) else {
            continue;
        };
        if d == 0 && e == 0 && a == 0 {
            continue;
        }
        let state = match moved.get(&k) {
            Some((_, _, curve)) => format!("MOVED at {} of {npads} pad(s)", curve.len()),
            None => format!("FLAT (bytes identical at all {npads} pad(s))"),
        };
        rows.push(format!(
            "BASE {rva:#x} {stem} {k} pad 0 differing {d} edit {e} addend {a} -- {state}"
        ));
    }
    Ok(rows)
}

#[derive(Debug)]
pub struct SweepOpts<'a> {
    pub label: &'a str,
    pub step: usize,
    pub at: usize,
    pub header: Option<&'a Path>,
    pub pads: &'a [usize],
    pub jobs: usize,
    pub work: &'a Path,
}

#[derive(Debug, Default)]
pub struct Swept {
    pub lines: Vec<String>,
    pub tsv: String,
    pub labels: BTreeMap<(String, String), u32>,
    pub errors: Vec<String>,
    pub failed: Vec<String>,
    pub compiles: usize,
}

fn signed(now: usize, before: usize) -> i64 {
    i64::try_from(now).unwrap_or(i64::MAX) - i64::try_from(before).unwrap_or(i64::MAX)
}

#[allow(clippy::too_many_lines)]
pub fn sweep(
    c: &Compiler,
    env: &Env,
    all: &[Unit],
    tus: &[&Unit],
    o: &SweepOpts<'_>,
) -> Result<Swept> {
    let root = run_root(o.work, "label-sweep", o.label);
    let _run = scratch_root(&root)?;
    let pads = pads_of(o.step, o.pads);
    let mut ovls: Vec<(usize, PathBuf)> = Vec::new();
    let hdir = match o.header {
        Some(h) => {
            let h = h.canonicalize().with_context(|| h.display().to_string())?;
            let hdir = tree_dir(&h, &search_dirs(all))?;
            let text = bounded::read(&h, hdrprice::MAX_SOURCE)
                .with_context(|| format!("reading {}", h.display()))?;
            for &n in &pads {
                let ovl = overlay_dir(&root, &format!("pad{n:02}"), &hdir);
                copy_headers(&hdir, &ovl)?;
                srcio::write_bytes(
                    ovl.join(h.file_name().unwrap_or_default()),
                    &insert_pad(&text, n, o.at),
                )
                .with_context(|| format!("writing the pad into {}", ovl.display()))?;
                ovls.push((n, ovl));
            }
            Some(hdir)
        }
        None => None,
    };
    let mut s = Swept {
        tsv: String::from("tu\trva\tsymbol\tpad\tdiffering\tedit\taddend\n"),
        ..Swept::default()
    };
    let mut out: Vec<(i64, String)> = Vec::new();
    let (mut sensitive, mut tu_lines, mut addend_lines, mut base_lines) =
        (0usize, Vec::new(), Vec::new(), Vec::new());
    for group in tus.chunks(sweep_chunk(o.jobs, pads.len())) {
        let work: Vec<Job<'_>> = group
            .iter()
            .flat_map(|u| {
                let (hdir, ovls) = (&hdir, &ovls);
                pads.iter().enumerate().map(move |(k, &n)| {
                    let v = match (hdir, ovls.get(k)) {
                        (Some(from), Some((_, to))) => Variant::Header {
                            from,
                            to,
                            src: None,
                        },
                        _ => Variant::Pad(n, o.at),
                    };
                    (*u, format!("pad{n:02}"), v)
                })
            })
            .collect();
        let Batch { builds, errors } = hdrprice::build_all(c, &root, &work, o.jobs);
        s.errors.extend(errors);
        s.failed.extend(hdrprice::failed_compiles(&work, &builds));
        s.compiles += work.len();
        for u in group {
            let _measured = Scratch(
                pads.iter()
                    .map(|n| work_dir(&root, &u.stem, &format!("pad{n:02}")))
                    .collect(),
            );
            let get = |n: usize| builds.get(&(u.stem.clone(), format!("pad{n:02}")));
            let Some(b) = get(0) else { continue };
            for (sym, l) in hdrprice::first_labels(&hdrprice::listing(&b.cod)) {
                s.labels.insert((u.stem.clone(), sym), l);
            }
            let mut curves: BTreeMap<String, (u32, Fig, Curve)> = BTreeMap::new();
            for &n in pads.iter().skip(1) {
                let Some(v) = get(n) else { continue };
                for r in diff_tu(env, b, v)?.rows {
                    let (Some(a), Some(((d0, e0), (d1, e1))), Some((a0, a1))) =
                        (r.rva, r.fig, r.addend)
                    else {
                        continue;
                    };
                    let e = curves.entry(r.key).or_insert((a, (d0, e0, a0), Vec::new()));
                    e.2.push((n, d1, e1, a1));
                }
            }
            base_lines.extend(base_rows(env, &u.stem, b, &curves, pads.len() - 1)?);
            let mut per_pad: BTreeMap<usize, i64> = pads.iter().skip(1).map(|&n| (n, 0)).collect();
            for (k, (a, (d0, e0, a0), curve)) in curves {
                sensitive += 1;
                s.tsv
                    .push_str(&curve_rows(&u.stem, a, &k, (d0, e0, a0), &curve));
                for &(n, _, e, _) in &curve {
                    *per_pad.entry(n).or_insert(0) += signed(e, e0);
                }
                let (low, at) = best(e0, &curve);
                if low < e0 {
                    out.push((
                        signed(low, e0),
                        format!(
                            "{a:#x} {} {k} differing {d0} edit {e0} -> {low} at {}",
                            u.stem,
                            pad_list(&at)
                        ),
                    ));
                }
                let (alow, aat) = best_addend(e0, a0, &curve);
                if alow < a0 {
                    addend_lines.push(format!(
                        "{a:#x} {} {k} ADDEND {a0} -> {alow} (edit held <= {e0}) at {}",
                        u.stem,
                        pad_list(&aat)
                    ));
                }
            }
            let low = per_pad.values().copied().min().unwrap_or(0);
            if low < 0 {
                let at: Vec<usize> = per_pad
                    .iter()
                    .filter(|(_, t)| **t == low)
                    .map(|(n, _)| *n)
                    .collect();
                tu_lines.push(format!(
                    "TU {}: whole-TU edit {low:+} at {}",
                    u.stem,
                    pad_list(&at)
                ));
            }
        }
    }
    out.sort();
    let gain: i64 = out.iter().map(|(g, _)| g).sum();
    let falls = out.len();
    s.lines.extend(out.into_iter().map(|(_, l)| l));
    s.lines.extend(tu_lines);
    s.lines.extend(addend_lines.iter().cloned());
    s.lines.extend(base_lines);
    s.lines.push(format!(
        "{} TU(s), {} pad(s): {sensitive} function(s) move under renumbering, {falls} fall ({gain:+} edit at each \
         one's best pad), {} lose addend mismatches",
        tus.len(),
        pads.len(),
        addend_lines.len()
    ));
    Ok(s)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn best_takes_the_least_edit_and_every_pad_reaching_it() {
        let c: Curve = vec![(4, 9, 30, 0), (8, 9, 12, 0), (12, 9, 12, 0)];
        assert_eq!(best(20, &c), (12, vec![8, 12]));
        assert_eq!(best(5, &c), (5, vec![]));
    }

    #[test]
    fn an_addend_only_pad_is_a_hit_when_the_edit_holds() {
        let c: Curve = vec![(7, 3, 5, 0), (13, 0, 0, 0), (14, 0, 0, 0), (20, 0, 0, 4)];
        assert_eq!(best_addend(0, 2, &c), (0, vec![13, 14]));
        assert_eq!(best_addend(0, 0, &c), (0, vec![]));
        assert_eq!(best_addend(5, 2, &c), (0, vec![7, 13, 14]));
    }

    #[test]
    fn rows_and_lists_read_as_documented() {
        let r = curve_rows("T", 0x10, "?f@@YAXXZ", (5, 4, 2), &vec![(3, 0, 0, 0)]);
        assert_eq!(
            r,
            "T\t0x10\t?f@@YAXXZ\t0\t5\t4\t2\nT\t0x10\t?f@@YAXXZ\t3\t0\t0\t0\n"
        );
        assert_eq!(pad_list(&[4]), "pad +4/-60");
        let many: Vec<usize> = (1..=10).collect();
        assert!(pad_list(&many).ends_with("+8/-56 (+2 more of 63)"));
        assert_eq!(pads_of(16, &[]), vec![0, 16, 32, 48]);
        assert_eq!(pads_of(16, &[64, 3]), vec![0, 3, 64]);
    }
}
