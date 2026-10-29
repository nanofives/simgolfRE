use iced_x86::{FlowControl, Formatter as _, MasmFormatter, OpKind};
use msvc6_base::digest::md5_hex;
use msvc6_diff::comdat::{Comdat, REL_REL32};
use msvc6_diff::env::Env;
use msvc6_diff::rawdiff::{self, Ctx};
use msvc6_diff::resolve::ilt_slot;
use msvc6_diff::seat::Seats;
use msvc6_diff::segments::{self, AlignRow, Analysis, Side};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub type Target = std::result::Result<usize, ()>;

#[must_use]
pub fn is_branch(insn: &iced_x86::Instruction) -> bool {
    matches!(
        insn.flow_control(),
        FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch
    ) && matches!(
        insn.op0_kind(),
        OpKind::NearBranch16 | OpKind::NearBranch32 | OpKind::NearBranch64
    )
}

#[must_use]
pub fn targets(s: &Side) -> Vec<Option<Target>> {
    let ips: Vec<u64> = s.dec.iter().map(|d| u64::from(d.ip)).collect();
    let end = u64::try_from(s.bytes.len()).unwrap_or(u64::MAX);
    s.dec
        .iter()
        .map(|d| {
            is_branch(&d.insn).then(|| {
                let t = d.insn.near_branch_target();
                if t == end {
                    Ok(ips.len())
                } else {
                    ips.binary_search(&t).map_err(|_| ())
                }
            })
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Verdict {
    Paired {
        img: usize,
        ours: usize,
    },
    Mismatch {
        img: usize,
        ours: usize,
        img_target: Target,
        ours_target: Target,
    },
    TargetUnaligned {
        img: usize,
        ours: usize,
        img_target: Target,
        ours_target: Target,
        next: Option<(usize, usize)>,
    },
    Unpaired {
        img: Option<usize>,
        ours: Option<usize>,
    },
    Destination {
        img: usize,
        ours: usize,
        img_dest: u32,
        ours_dest: Option<u32>,
    },
}

#[must_use]
pub fn check(
    img_t: &[Option<Target>],
    ours_t: &[Option<Target>],
    rows: &[(Option<usize>, Option<usize>)],
) -> Vec<Verdict> {
    let mut row_img = vec![rows.len(); img_t.len().saturating_add(1)];
    let mut row_ours = vec![rows.len(); ours_t.len().saturating_add(1)];
    for (r, (i, j)) in rows.iter().enumerate() {
        if let Some(x) = i.and_then(|i| row_img.get_mut(i)) {
            *x = r;
        }
        if let Some(x) = j.and_then(|j| row_ours.get_mut(j)) {
            *x = r;
        }
    }
    let both = |r: usize| rows.get(r).is_some_and(|(i, j)| i.is_some() && j.is_some());
    let mut next_pair = vec![rows.len(); rows.len().saturating_add(1)];
    for r in (0..rows.len()).rev() {
        let after = next_pair.get(r + 1).copied().unwrap_or(rows.len());
        if let Some(x) = next_pair.get_mut(r) {
            *x = if both(r) { r } else { after };
        }
    }
    let row = |t: Target, rowmap: &[usize]| -> std::result::Result<usize, ()> {
        Ok(rowmap.get(t?).copied().unwrap_or(rows.len()))
    };
    let lands = |r: usize| next_pair.get(r).copied().unwrap_or(rows.len());
    let aligned = |r: usize| r == rows.len() || both(r);
    let mut out = Vec::new();
    for (i, j) in rows {
        let ib = i.and_then(|i| img_t.get(i).copied().flatten());
        let ob = j.and_then(|j| ours_t.get(j).copied().flatten());
        match (ib, ob, i, j) {
            (Some(it), Some(ot), Some(i), Some(j)) => {
                let (img, ours) = (*i, *j);
                let mismatch = Verdict::Mismatch {
                    img,
                    ours,
                    img_target: it,
                    ours_target: ot,
                };
                out.push(match (row(it, &row_img), row(ot, &row_ours)) {
                    (Err(()), Err(())) => Verdict::Paired { img, ours },
                    (Ok(ri), Ok(rj)) if lands(ri) == lands(rj) => {
                        if aligned(ri) && aligned(rj) {
                            Verdict::Paired { img, ours }
                        } else {
                            Verdict::TargetUnaligned {
                                img,
                                ours,
                                img_target: it,
                                ours_target: ot,
                                next: rows.get(lands(ri)).and_then(|(a, b)| a.zip(*b)),
                            }
                        }
                    }
                    _ => mismatch,
                });
            }
            (Some(_), _, Some(i), _) => out.push(Verdict::Unpaired {
                img: Some(*i),
                ours: None,
            }),
            (None, Some(_), _, Some(j)) => out.push(Verdict::Unpaired {
                img: None,
                ours: Some(*j),
            }),
            _ => {}
        }
    }
    out
}

#[must_use]
pub fn summary(v: &[Verdict]) -> (usize, usize, usize, usize) {
    let count = |f: fn(&Verdict) -> bool| v.iter().filter(|x| f(x)).count();
    let mis = count(|x| matches!(x, Verdict::Mismatch { .. } | Verdict::Destination { .. }));
    let una = count(|x| matches!(x, Verdict::TargetUnaligned { .. }));
    let paired = mis + una + count(|x| matches!(x, Verdict::Paired { .. }));
    (paired, mis, una, v.len().saturating_sub(paired))
}

#[derive(Debug, Clone)]
pub struct Checked {
    pub rows: Vec<(Option<usize>, Option<usize>)>,
    pub img_t: Vec<Option<Target>>,
    pub ours_t: Vec<Option<Target>>,
    pub verdicts: Vec<Verdict>,
    pub relocated: usize,
    pub own: usize,
    pub outside: usize,
}

fn read4(b: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(
        b.get(at..at.checked_add(4)?)?.try_into().ok()?,
    ))
}

fn dest_of(s: &Side, i: usize, rva: u32) -> u32 {
    let t = s.dec.get(i).map_or(0, |d| d.insn.near_branch_target());
    rva.wrapping_add(u32::try_from(t & 0xffff_ffff).unwrap_or(0))
}

#[must_use]
pub fn relocated_dests_in(
    branches: &[(u32, u32)],
    c: &Comdat,
    rva: u32,
    resolve: &dyn Fn(usize) -> Option<u32>,
) -> BTreeMap<u32, Option<u32>> {
    let mut out = BTreeMap::new();
    for &(ip, len) in branches {
        let hi = ip.saturating_add(len);
        let Some(r) = c
            .relocations
            .iter()
            .find(|r| r.at >= ip && r.at < hi && r.kind == REL_REL32)
        else {
            continue;
        };
        let at = usize::try_from(r.at).unwrap_or(usize::MAX);
        let addend = read4(&c.bytes, at).unwrap_or(0);
        let dest = if r.target_index == Some(c.index) {
            Some(rva.wrapping_add(r.target_offset).wrapping_add(addend))
        } else {
            resolve(at).map(|s| s.wrapping_add(addend))
        };
        out.insert(ip, dest);
    }
    out
}

#[must_use]
pub fn relocated_dests(
    a: &Analysis,
    resolve: &dyn Fn(usize) -> Option<u32>,
) -> BTreeMap<u32, Option<u32>> {
    let branches: Vec<(u32, u32)> = a
        .ours
        .dec
        .iter()
        .filter(|d| is_branch(&d.insn))
        .map(|d| (d.ip, u32::try_from(d.insn.len()).unwrap_or(0)))
        .collect();
    relocated_dests_in(&branches, &a.comdat, a.rva, resolve)
}

#[must_use]
pub fn check_analysis(
    a: &Analysis,
    dests: &BTreeMap<u32, Option<u32>>,
    ilt: &dyn Fn(u32) -> u32,
) -> Checked {
    let wide: Vec<AlignRow> = if a.ours.wide.is_empty() {
        segments::align_range(&a.img, &a.ours, (0, a.img.dec.len()), (0, a.ours.dec.len()))
    } else {
        a.ours.wide.clone()
    };
    let rows: Vec<(Option<usize>, Option<usize>)> = wide.iter().map(|r| (r.img, r.ours)).collect();
    let (img_t, mut ours_t) = (targets(&a.img), targets(&a.ours));
    let len = u32::try_from(a.ours.bytes.len()).unwrap_or(u32::MAX);
    let ips: Vec<u32> = a.ours.dec.iter().map(|d| d.ip).collect();
    let (mut relocated, mut own) = (0usize, 0usize);
    for (d, slot) in a.ours.dec.iter().zip(ours_t.iter_mut()) {
        let Some(dest) = dests.get(&d.ip) else {
            continue;
        };
        relocated += 1;
        let inside = dest
            .and_then(|x| x.checked_sub(a.rva))
            .filter(|o| *o <= len);
        *slot = Some(match inside {
            Some(o) => {
                own += 1;
                if o == len {
                    Ok(ips.len())
                } else {
                    ips.binary_search(&o).map_err(|_| ())
                }
            }
            None => Err(()),
        });
    }
    let mut verdicts = check(&img_t, &ours_t, &rows);
    let mut outside = 0usize;
    for v in &mut verdicts {
        let Verdict::Paired { img, ours } = *v else {
            continue;
        };
        let leaves = |t: &[Option<Target>], i: usize| matches!(t.get(i), Some(Some(Err(()))));
        if !leaves(&img_t, img) || !leaves(&ours_t, ours) {
            continue;
        }
        outside += 1;
        let img_dest = dest_of(&a.img, img, a.rva);
        let ip = a.ours.dec.get(ours).map_or(0, |d| d.ip);
        let ours_dest = match dests.get(&ip) {
            Some(d) => *d,
            None => Some(dest_of(&a.ours, ours, a.rva)),
        };
        if ours_dest.map(ilt) != Some(ilt(img_dest)) {
            *v = Verdict::Destination {
                img,
                ours,
                img_dest,
                ours_dest,
            };
        }
    }
    Checked {
        rows,
        img_t,
        ours_t,
        verdicts,
        relocated,
        own,
        outside,
    }
}

#[must_use]
pub fn check_linked(env: &Env, ctx: &Ctx, a: &Analysis) -> Checked {
    let (_, _, links) = rawdiff::of_listed(env, ctx, a.rva, &a.obj, &a.comdat, a.img.bytes.len());
    let resolve = |at: usize| links.iter().find(|l| l.at == at).and_then(|l| l.ours);
    let dests = relocated_dests(a, &resolve);
    let text = env.text();
    let ilt = |x: u32| ilt_slot(text, env.text_rva, x).unwrap_or(x);
    check_analysis(a, &dests, &ilt)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub enum Kind {
    Mismatch,
    TargetUnaligned,
    Unpaired,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Finding {
    pub kind: Kind,
    pub ours: Option<u32>,
    pub line: String,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Named {
    pub image: String,
    pub rva: u32,
    pub ours: u32,
    pub why: String,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Duplicate {
    pub image: String,
    pub name: String,
    pub object: String,
    pub md5: String,
    pub why: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Excuses {
    #[serde(default)]
    pub named: Vec<Named>,
    #[serde(default)]
    pub duplicate: Vec<Duplicate>,
}

pub const MAX_EXCUSES: u64 = 1 << 20;

pub fn read_excuses(path: &Path) -> anyhow::Result<Excuses> {
    let bytes = crate::libs::read_bounded(path, MAX_EXCUSES)?;
    let text = msvc6_base::srcio::tool_text(&bytes);
    toml::from_str(&text).map_err(|e| anyhow::anyhow!("{}: {e}", path.display()))
}

fn named_as<'n>(named: &'n [Named], image: &str, rva: u32, f: &Finding) -> Option<&'n Named> {
    if f.kind != Kind::TargetUnaligned {
        return None;
    }
    named
        .iter()
        .find(|n| n.image.eq_ignore_ascii_case(image) && n.rva == rva && Some(n.ours) == f.ours)
}

#[must_use]
pub fn failing(
    findings: &[Finding],
    named: &[Named],
    image: &str,
    rva: u32,
) -> (usize, usize, usize) {
    let mis = findings.iter().filter(|f| f.kind == Kind::Mismatch).count();
    let una = findings
        .iter()
        .filter(|f| f.kind == Kind::TargetUnaligned)
        .count();
    let ok = findings
        .iter()
        .filter(|f| named_as(named, image, rva, f).is_some())
        .count();
    (mis, una.saturating_sub(ok), ok)
}

#[must_use]
pub fn landing_line(m: usize, t: usize, named: usize) -> String {
    let n = if named > 0 {
        format!(" ({named} named TARGET-UNALIGNED row(s) excused)")
    } else {
        String::new()
    };
    match (m, t) {
        (0, 0) => {
            format!("landing check: PASS (no mismatched or unnamed target-unaligned branch){n}")
        }
        (0, t) => format!(
            "landing check: FAIL unless each of the {t} TARGET-UNALIGNED row(s) is named by its ours offset, with why the unaligned instruction is the label's own code{n}"
        ),
        (m, _) => {
            format!("landing check: FAIL ({m} mismatched; a MISMATCH is never named away){n}")
        }
    }
}

#[must_use]
pub fn reloc_symbol(s: &str) -> &str {
    match s.strip_prefix("$L") {
        Some(n) if !n.is_empty() && n.chars().all(|c| c.is_ascii_digit()) => "$L",
        _ => s,
    }
}

#[must_use]
pub fn copy_md5(c: &Comdat) -> String {
    let mut b = c.bytes.clone();
    for r in &c.relocations {
        b.extend_from_slice(
            format!("\n{:08x} {:04x} {}", r.at, r.kind, reloc_symbol(&r.symbol)).as_bytes(),
        );
    }
    md5_hex(&b)
}

#[derive(Debug, Clone, Default)]
pub struct Dups {
    pub names: usize,
    pub compared: usize,
    pub bad: Vec<String>,
    pub ok: Vec<String>,
    pub refused: Vec<(PathBuf, String)>,
}

type Copy = (PathBuf, Comdat);

#[must_use]
pub fn duplicates(
    image: &str,
    list: &[PathBuf],
    seats: &Seats,
    excused: &[Duplicate],
    jobs: usize,
) -> Dups {
    let mut out = Dups::default();
    let per: Vec<Result<Vec<(String, Copy)>, String>> = msvc6_diff::par::map(list, jobs, |o| {
        msvc6_diff::comdat::text(o)
            .map(|cs| {
                cs.into_iter()
                    .filter(|c| c.external)
                    .filter_map(|c| Some((c.symbol.clone()?, (o.clone(), c))))
                    .collect()
            })
            .map_err(|e| format!("{e:#}"))
    });
    let mut by: BTreeMap<String, Vec<Copy>> = BTreeMap::new();
    for (o, r) in list.iter().zip(per) {
        match r {
            Ok(v) => {
                for (s, c) in v {
                    by.entry(s).or_default().push(c);
                }
            }
            Err(e) => out.refused.push((o.clone(), e)),
        }
    }
    let seated: BTreeSet<(&Path, u16)> = seats
        .values()
        .map(|(o, c)| (o.as_path(), c.index))
        .collect();
    let key = |c: &Comdat| -> Vec<(u32, u16, String)> {
        c.relocations
            .iter()
            .map(|r| (r.at, r.kind, reloc_symbol(&r.symbol).to_string()))
            .collect()
    };
    let file = |o: &Path| {
        o.file_name()
            .map_or_else(String::new, |f| f.to_string_lossy().into_owned())
    };
    for (s, copies) in &by {
        if copies.len() < 2 {
            continue;
        }
        out.names += 1;
        let refi = copies
            .iter()
            .position(|(o, c)| seated.contains(&(o.as_path(), c.index)))
            .unwrap_or(0);
        let Some((ro, rc)) = copies.get(refi) else {
            continue;
        };
        for (i, (o, c)) in copies.iter().enumerate() {
            if i == refi {
                continue;
            }
            out.compared += 1;
            let (ka, kb) = (key(c), key(rc));
            if c.bytes == rc.bytes && ka == kb {
                continue;
            }
            let why = c
                .bytes
                .iter()
                .zip(&rc.bytes)
                .position(|(x, y)| x != y)
                .map(|p| format!("byte +{p:#x}"))
                .or_else(|| (c.bytes.len() != rc.bytes.len()).then(|| "length".to_string()))
                .unwrap_or_else(|| match ka.iter().zip(&kb).find(|(x, y)| x != y) {
                    Some((x, y)) => format!("relocation at +{:#x}: {} vs {}", x.0, x.2, y.2),
                    None => format!("{} vs {} relocations", ka.len(), kb.len()),
                });
            let md5 = copy_md5(c);
            let row = format!(
                "{s}: {} differs from the {} copy in {} ({} vs {} bytes; {why}; md5 {md5})",
                o.display(),
                if seated.contains(&(ro.as_path(), rc.index)) {
                    "seated"
                } else {
                    "first"
                },
                ro.display(),
                c.bytes.len(),
                rc.bytes.len()
            );
            match excused.iter().find(|d| {
                d.image.eq_ignore_ascii_case(image)
                    && d.name == *s
                    && d.object.eq_ignore_ascii_case(&file(o))
                    && d.md5.eq_ignore_ascii_case(&md5)
            }) {
                Some(d) => out.ok.push(format!("DUP-NAMED {row}  [NAMED: {}]", d.why)),
                None => out.bad.push(format!("DUP-DIFFERS {row}")),
            }
        }
    }
    out
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn findings(a: &Analysis, c: &Checked) -> Vec<Finding> {
    let (rows, v) = (&c.rows, &c.verdicts);
    let mut out = Vec::new();
    let mut f = MasmFormatter::new();
    let mut text = |s: &Side, i: usize| -> String {
        s.dec.get(i).map_or_else(String::new, |d| {
            let mut t = String::new();
            f.format_mnemonic(&d.insn, &mut t);
            t
        })
    };
    let va = |i: Option<usize>| -> String {
        i.map_or("-".to_string(), |i| {
            format!(
                "{:#x}",
                a.start
                    .saturating_add(u64::from(a.img.dec.get(i).map_or(0, |d| d.ip)))
            )
        })
    };
    let off = |i: Option<usize>| -> String {
        i.map_or("-".to_string(), |i| {
            format!("+{:#x}", a.ours.dec.get(i).map_or(0, |d| d.ip))
        })
    };
    let tgt = |t: Target, img: bool| -> String {
        match t {
            Err(()) => "outside the function".to_string(),
            Ok(i) if img && i >= a.img.dec.len() => "the function end".to_string(),
            Ok(i) if img => va(Some(i)),
            Ok(i) if i >= a.ours.dec.len() => "the function end".to_string(),
            Ok(i) => off(Some(i)),
        }
    };
    let at = |j: usize| a.ours.dec.get(j).map(|d| d.ip);
    for x in v {
        match x {
            Verdict::Mismatch {
                img,
                ours,
                img_target,
                ours_target,
            } => {
                let across = |t: Target, img_side: bool| -> String {
                    let Ok(t) = t else {
                        return String::new();
                    };
                    let hit = rows
                        .iter()
                        .skip_while(|(i, j)| {
                            if img_side {
                                *i != Some(t)
                            } else {
                                *j != Some(t)
                            }
                        })
                        .find_map(|(i, j)| if img_side { *j } else { *i });
                    match (hit, img_side) {
                        (Some(j), true) => format!(" (= ours {})", off(Some(j))),
                        (Some(i), false) => format!(" (= image {})", va(Some(i))),
                        (None, _) => String::new(),
                    }
                };
                let (ti, to) = (text(&a.img, *img), text(&a.ours, *ours));
                out.push(Finding {
                    kind: Kind::Mismatch,
                    ours: at(*ours),
                    line: format!(
                        "MISMATCH  image {} {ti} -> {}{}  |  ours {} {to} -> {}{}",
                        va(Some(*img)),
                        tgt(*img_target, true),
                        across(*img_target, true),
                        off(Some(*ours)),
                        tgt(*ours_target, false),
                        across(*ours_target, false)
                    ),
                });
            }
            Verdict::TargetUnaligned {
                img,
                ours,
                img_target,
                ours_target,
                next,
            } => {
                let state = |t: Target, img_side: bool| -> &'static str {
                    let Ok(t) = t else { return "" };
                    let pairs = rows.iter().any(|(i, j)| {
                        if img_side {
                            *i == Some(t) && j.is_some()
                        } else {
                            *j == Some(t) && i.is_some()
                        }
                    });
                    let end = if img_side {
                        t >= a.img.dec.len()
                    } else {
                        t >= a.ours.dec.len()
                    };
                    match (pairs || end, img_side) {
                        (true, _) => " (aligned)",
                        (false, true) => " (UNALIGNED: deleted, no ours counterpart)",
                        (false, false) => " (UNALIGNED: inserted, no image counterpart)",
                    }
                };
                let (ti, to) = (text(&a.img, *img), text(&a.ours, *ours));
                out.push(Finding {
                    kind: Kind::TargetUnaligned,
                    ours: at(*ours),
                    line: format!(
                        "TARGET-UNALIGNED  image {} {ti} -> {}{}  |  ours {} {to} -> {}{}; both fall into {}",
                        va(Some(*img)),
                        tgt(*img_target, true),
                        state(*img_target, true),
                        off(Some(*ours)),
                        tgt(*ours_target, false),
                        state(*ours_target, false),
                        next.map_or_else(
                            || "the function end".to_string(),
                            |(i, j)| format!(
                                "the aligned pair image {} = ours {}",
                                va(Some(i)),
                                off(Some(j))
                            )
                        )
                    ),
                });
            }
            Verdict::Unpaired { img, ours } => {
                let line = format!(
                    "UNPAIRED  image {} {}  |  ours {} {}",
                    va(*img),
                    img.map_or_else(|| "(no branch)".to_string(), |i| text(&a.img, i)),
                    off(*ours),
                    ours.map_or_else(|| "(no branch)".to_string(), |j| text(&a.ours, j))
                );
                out.push(Finding {
                    kind: Kind::Unpaired,
                    ours: ours.and_then(at),
                    line,
                });
            }
            Verdict::Destination {
                img,
                ours,
                img_dest,
                ours_dest,
            } => {
                let vaf = |r: u32| {
                    format!(
                        "{:#x}",
                        a.start
                            .wrapping_sub(u64::from(a.rva))
                            .wrapping_add(u64::from(r))
                    )
                };
                let (ti, to) = (text(&a.img, *img), text(&a.ours, *ours));
                out.push(Finding {
                    kind: Kind::Mismatch,
                    ours: at(*ours),
                    line: format!(
                        "MISMATCH  image {} {ti} -> {} (leaves the function)  |  ours {} {to} -> {} (destination)",
                        va(Some(*img)),
                        vaf(*img_dest),
                        off(Some(*ours)),
                        ours_dest.map_or_else(
                            || "UNRESOLVED (its relocation's target has no address)".to_string(),
                            vaf
                        )
                    ),
                });
            }
            Verdict::Paired { .. } => {}
        }
    }
    out
}

#[derive(Debug, Clone)]
pub struct Done {
    pub rva: u32,
    pub name: String,
    pub summary: (usize, usize, usize, usize),
    pub relocated: (usize, usize, usize),
    pub findings: Vec<Finding>,
}

pub fn check_one(env: &Env, ctx: &Ctx, rva: u32, obj: &Path, c: &Comdat) -> anyhow::Result<Done> {
    let a = segments::analyze_in(env, rva, obj, c.clone(), segments::MAX_SEGMENT)?;
    let ch = check_linked(env, ctx, &a);
    Ok(Done {
        rva,
        name: c.symbol.clone().unwrap_or_else(|| "?".to_string()),
        summary: summary(&ch.verdicts),
        relocated: (ch.relocated, ch.own, ch.outside),
        findings: findings(&a, &ch),
    })
}

#[derive(Debug, Default)]
pub struct All {
    pub done: Vec<(u32, String, Result<Done, String>)>,
    pub dups: Dups,
    pub totals: Totals,
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub struct Totals {
    pub functions: usize,
    pub paired: usize,
    pub mismatched: usize,
    pub unnamed: usize,
    pub named: usize,
    pub unpaired: usize,
    pub relocated: usize,
    pub own: usize,
    pub outside: usize,
    pub bad_functions: usize,
    pub not_checked: usize,
}

#[must_use]
pub fn run_all(env: &Env, objs: &[PathBuf], seats: &Seats, excuses: &Excuses, jobs: usize) -> All {
    let ctx = Ctx::build(env, objs, seats);
    let picked: Vec<(&u32, &(PathBuf, Comdat))> = seats.iter().collect();
    let done: Vec<(u32, String, Result<Done, String>)> =
        msvc6_diff::par::map(&picked, jobs, |&(&rva, (o, c))| {
            (
                rva,
                c.symbol.clone().unwrap_or_else(|| "?".to_string()),
                check_one(env, &ctx, rva, o, c).map_err(|e| format!("{e:#}")),
            )
        });
    let dups = duplicates(&env.label, objs, seats, &excuses.duplicate, jobs);
    let mut t = Totals::default();
    for (rva, _, r) in &done {
        match r {
            Ok(d) => {
                t.functions += 1;
                t.paired += d.summary.0;
                t.unpaired += d.summary.3;
                t.relocated += d.relocated.0;
                t.own += d.relocated.1;
                t.outside += d.relocated.2;
                let (fm, ft, fok) = failing(&d.findings, &excuses.named, &env.label, *rva);
                t.mismatched += fm;
                t.unnamed += ft;
                t.named += fok;
                if fm + ft > 0 {
                    t.bad_functions += 1;
                }
            }
            Err(_) => t.not_checked += 1,
        }
    }
    All {
        done,
        dups,
        totals: t,
    }
}

#[must_use]
pub fn tag(named: &[Named], image: &str, rva: u32, f: &Finding) -> String {
    named_as(named, image, rva, f).map_or_else(String::new, |x| format!("  [NAMED: {}]", x.why))
}

#[must_use]
pub fn stale_named<'n>(named: &'n [Named], image: &str, all: &All) -> Vec<&'n Named> {
    named
        .iter()
        .filter(|x| x.image.eq_ignore_ascii_case(image))
        .filter(|x| {
            !all.done.iter().any(|(rva, _, r)| {
                *rva == x.rva
                    && r.as_ref().is_ok_and(|d| {
                        d.findings
                            .iter()
                            .any(|f| f.kind == Kind::TargetUnaligned && f.ours == Some(x.ours))
                    })
            })
        })
        .collect()
}

#[must_use]
pub fn total_line(all: &All) -> String {
    let t = &all.totals;
    format!(
        "{} function(s) checked, {} paired, {} mismatched, {} unnamed target-unaligned ({} named), {} unpaired; {} pair(s) leave a function, each compared by destination ({} relocated branch(es) resolved through their relocation, {} of them into their own function); {} duplicate cop(ies) of {} name(s) compared with the copy the link takes, {} differ unnamed, {} differ named; {} not checked",
        t.functions,
        t.paired,
        t.mismatched,
        t.unnamed,
        t.named,
        t.unpaired,
        t.outside,
        t.relocated,
        t.own,
        all.dups.compared,
        all.dups.names,
        all.dups.bad.len(),
        all.dups.ok.len(),
        t.not_checked
    )
}

#[must_use]
pub fn passes(all: &All) -> bool {
    all.totals.mismatched + all.totals.unnamed == 0
        && all.dups.bad.is_empty()
        && all.totals.not_checked == 0
        && all.dups.refused.is_empty()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_branch_landing_on_another_aligned_instruction_is_a_mismatch() {
        let diag: Vec<(Option<usize>, Option<usize>)> =
            (0..5).map(|i| (Some(i), Some(i))).collect();
        let img: Vec<Option<Target>> = vec![Some(Ok(3)), None, None, None, Some(Ok(0))];
        let bad: Vec<Option<Target>> = vec![Some(Ok(2)), None, None, None, Some(Ok(0))];
        let v = check(&img, &bad, &diag);
        assert_eq!(summary(&v), (2, 1, 0, 0));
        assert!(matches!(
            v.first(),
            Some(Verdict::Mismatch {
                img: 0,
                ours: 0,
                img_target: Ok(3),
                ours_target: Ok(2)
            })
        ));
        assert_eq!(summary(&check(&img, &img, &diag)), (2, 0, 0, 0));
        let plain: Vec<Option<Target>> = vec![Some(Ok(3)), None, None, None, None];
        let v = check(&img, &plain, &diag);
        assert_eq!(summary(&v), (1, 0, 0, 1));
        let out_img: Vec<Option<Target>> = vec![Some(Err(())), None, None, None, Some(Ok(0))];
        assert_eq!(summary(&check(&out_img, &out_img, &diag)), (2, 0, 0, 0));
        assert_eq!(summary(&check(&out_img, &img, &diag)), (2, 1, 0, 0));
    }

    #[test]
    fn a_branch_into_an_inserted_or_deleted_instruction_is_target_unaligned() {
        let rows = vec![
            (Some(0), Some(0)),
            (Some(1), Some(1)),
            (Some(2), Some(2)),
            (Some(3), Some(3)),
            (None, Some(4)),
            (Some(4), Some(5)),
            (Some(5), Some(6)),
        ];
        let img: Vec<Option<Target>> = vec![Some(Ok(4)), None, None, None, None, None];
        let ours: Vec<Option<Target>> = vec![Some(Ok(4)), None, None, None, None, None, None];
        let v = check(&img, &ours, &rows);
        assert_eq!(
            v,
            vec![Verdict::TargetUnaligned {
                img: 0,
                ours: 0,
                img_target: Ok(4),
                ours_target: Ok(4),
                next: Some((4, 5)),
            }]
        );
        let fixed: Vec<Option<Target>> = vec![Some(Ok(5)), None, None, None, None, None, None];
        assert_eq!(summary(&check(&img, &fixed, &rows)), (1, 0, 0, 0));
        let img_c: Vec<Option<Target>> = vec![Some(Ok(3)), None, None, None, None, None];
        assert_eq!(summary(&check(&img_c, &ours, &rows)), (1, 1, 0, 0));
        let end_i: Vec<Option<Target>> = vec![Some(Ok(6)), None, None, None, None, None];
        let end_o: Vec<Option<Target>> = vec![Some(Ok(7)), None, None, None, None, None, None];
        assert_eq!(summary(&check(&end_i, &end_o, &rows)), (1, 0, 0, 0));
    }

    #[test]
    fn only_a_named_target_unaligned_row_is_excused() {
        let f = |kind, ours| Finding {
            kind,
            ours: Some(ours),
            line: String::new(),
        };
        let found = [
            f(Kind::TargetUnaligned, 0x3354),
            f(Kind::Mismatch, 0x10),
            f(Kind::Unpaired, 0x20),
        ];
        let named = |image: &str, rva, ours| Named {
            image: image.to_string(),
            rva,
            ours,
            why: "the pad's own test".to_string(),
        };
        assert_eq!(failing(&found, &[], "A", 0xf5c0), (1, 1, 0));
        let ok = [named("A", 0xf5c0, 0x3354)];
        assert_eq!(failing(&found, &ok, "a", 0xf5c0), (1, 0, 1));
        for other in [
            named("A", 0xf5c0, 0x3355),
            named("A", 0x24120, 0x3354),
            named("B", 0xf5c0, 0x3354),
        ] {
            assert_eq!(failing(&found, &[other], "A", 0xf5c0), (1, 1, 0));
        }
        let mis = [f(Kind::Mismatch, 0x3354)];
        assert_eq!(failing(&mis, &ok, "A", 0xf5c0), (1, 0, 0));
        assert!(landing_line(0, 0, 1).starts_with("landing check: PASS"));
        assert!(landing_line(0, 1, 0).starts_with("landing check: FAIL unless"));
        assert!(landing_line(1, 0, 0).contains("never named away"));
        assert_eq!(reloc_symbol("$L4101"), "$L");
        assert_eq!(reloc_symbol("$Lx"), "$Lx");
    }

    #[test]
    fn an_excuse_file_is_read_strictly() {
        let ok: Excuses = toml::from_str(
            "[[named]]\nimage = \"A\"\nrva = 0x10\nours = 0x4\nwhy = \"w\"\n[[duplicate]]\nimage = \"A\"\nname = \"_f\"\nobject = \"x.obj\"\nmd5 = \"00\"\nwhy = \"w\"\n",
        )
        .unwrap_or_default();
        assert_eq!((ok.named.len(), ok.duplicate.len()), (1, 1));
        assert!(
            toml::from_str::<Excuses>(
                "[[named]]\nimage = \"A\"\nrva = 1\nours = 2\nwhy = \"w\"\nextra = 1\n"
            )
            .is_err()
        );
    }
}
