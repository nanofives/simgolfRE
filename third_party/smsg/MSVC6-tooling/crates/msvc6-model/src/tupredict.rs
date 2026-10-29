use crate::bk::Driver;
use crate::drain::{self, Refs, Stream, TuIl};
use crate::tuorder::{Member, Ordered, score_of};
use anyhow::Result;
use msvc6_diff::comdat;
use std::collections::{BTreeMap, BTreeSet};
use std::path::PathBuf;

#[derive(Debug, Clone, Default)]
#[allow(clippy::struct_excessive_bools)]
pub struct Opts {
    pub il_table: bool,
    pub all: bool,
    pub pch_use: bool,
    pub ob0: bool,
    pub show: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Totals {
    pub n: usize,
    pub exact: usize,
    pub demand_exact: usize,
    pub levers: usize,
    pub ex_bodies: usize,
    pub ob0_n: usize,
    pub ob0_exact: usize,
    pub hybrid_exact: usize,
}

struct Common {
    stem: String,
    known: Vec<String>,
    il: TuIl,
    refs: Refs,
    want: Vec<String>,
    demand: bool,
    base: usize,
}

fn names_of(path: &std::path::Path) -> Result<Vec<String>> {
    Ok(comdat::text(path)?
        .into_iter()
        .filter_map(|c| c.symbol)
        .collect())
}

type CompiledIl = Result<Option<(TuIl, PathBuf)>>;

#[allow(clippy::too_many_lines)]
pub fn predict(d: &Driver<'_>, ordered: &Ordered, o: &Opts) -> Result<(Vec<String>, Totals)> {
    let mut out = Vec::new();
    let mut t = Totals::default();
    if o.pch_use {
        out.push(format!(
            "  PREDICT: merge-by-IL-offset (MSDEV replay: every TU in batch order sharing one pch, under \
             {}/pch-use/)",
            d.work.display()
        ));
    } else {
        out.push(format!(
            "  PREDICT: merge-by-IL-offset (pch-create /Bk compiles under {}/)",
            d.work.display()
        ));
    }
    let wanted = |tu: &str, v: &[Member]| {
        let seq: Vec<&Member> = v.iter().collect();
        if o.all {
            true
        } else if o.show.is_empty() {
            !score_of(&seq).order.is_empty()
        } else {
            o.show.iter().any(|x| x.eq_ignore_ascii_case(tu))
        }
    };
    let paths: BTreeMap<&str, &PathBuf> = ordered
        .objects
        .iter()
        .zip(&ordered.paths)
        .map(|((tu, _), p)| (tu.as_str(), p))
        .collect();
    let stems: Vec<String> = ordered
        .objects
        .iter()
        .filter(|(tu, v)| wanted(tu, v) && paths.contains_key(tu.as_str()))
        .map(|(tu, _)| tu.clone())
        .collect();
    let mut creators: BTreeMap<String, String> = BTreeMap::new();
    let mut compiled: BTreeMap<String, CompiledIl> = if o.pch_use {
        let seq = d.msdev_order();
        let got = d.replay(&seq)?;
        let mut m = BTreeMap::new();
        for r in got {
            let Some(s) = stems.iter().find(|s| s.eq_ignore_ascii_case(&r.stem)) else {
                continue;
            };
            creators.insert(r.stem.clone(), r.creator.clone());
            m.insert(s.clone(), Ok(Some((r.il, r.obj))));
        }
        m
    } else {
        let got = msvc6_diff::par::map(&stems, d.jobs, |s| d.bk(s, None, &[]));
        stems.iter().cloned().zip(got).collect()
    };
    let mut ob0_orders: BTreeMap<String, (Vec<String>, PathBuf)> = BTreeMap::new();
    if o.ob0 {
        let got = msvc6_diff::par::map(&stems, d.jobs, |s| {
            d.bk(s, Some("ob0"), &["/Ob0"]).and_then(|r| match r {
                Some((_, obj)) => Ok(Some((names_of(&obj)?, obj))),
                None => Ok(None),
            })
        });
        for (s, r) in stems.iter().zip(got) {
            match r {
                Ok(Some(v)) => {
                    ob0_orders.insert(s.clone(), v);
                }
                Ok(None) => {}
                Err(e) => eprintln!("  /Ob0 {s}: {e:#}"),
            }
        }
    }
    let mut common: Vec<Common> = Vec::new();
    let mut sweep_left = SWEEP_WORK;
    for (tu, v) in &ordered.objects {
        let seq: Vec<&Member> = v.iter().collect();
        let s = score_of(&seq);
        let Some(c) = compiled.remove(tu.as_str()) else {
            continue;
        };
        let Some(path) = paths.get(tu.as_str()) else {
            continue;
        };
        let names = names_of(path)?;
        let Some((il, obj)) = c? else {
            out.push(format!("    PREDICT {tu}: no project TU of that stem"));
            continue;
        };
        let own: Vec<String> = names_of(&obj).unwrap_or_default();
        let refs = comdat::read(&obj)
            .ok()
            .and_then(|b| drain::refs_of_bytes(&b).ok())
            .unwrap_or_default();
        if !names.is_empty()
            && !names.iter().any(|x| il.fns.contains_key(x))
            && let Some(e) = &il.error
        {
            out.push(format!(
                "    PREDICT {tu}: NOTE the .gl of front-end build {} names none of the object's COMDATs: it reads \
                 no further than {e}",
                il.build
                    .map_or_else(|| "?".to_string(), |b| b.to_string())
            ));
        }
        let (il, ok, refs) = drain::report_compiled(tu, &names, il, &own, refs, &mut out);
        if let Some((v0, obj0)) = ob0_orders.remove(tu.as_str()) {
            t.ob0_n += 1;
            t.ob0_exact += usize::from(drain::report_ob0(&names, &v0, &il, &mut out) == Some(0));
            let refs0 = comdat::read(&obj0)
                .ok()
                .and_then(|b| drain::refs_of_bytes(&b).ok())
                .unwrap_or_default();
            let known: Vec<String> = names
                .iter()
                .filter(|x| il.fns.contains_key(*x))
                .cloned()
                .collect();
            let h = drain::predicted_hybrid(&known, &v0, &il, &refs0);
            let dd = drain::against(&known, &h);
            let hit = drain::exact(&known, &h);
            t.hybrid_exact += usize::from(hit);
            out.push(format!(
                "      HYBRID (real IL offsets, /Ob0 references): {dd} displaced{}",
                if hit { " (EXACT)" } else { "" }
            ));
        }
        if let Some(w) = creators.get(tu.as_str()) {
            out.push(format!(
                "      pch: {}",
                if w.eq_ignore_ascii_case(tu) {
                    "CREATE (its own compile wrote it)".to_string()
                } else {
                    format!("USE of {w}'s (that compile wrote it; this one left it unchanged)")
                }
            ));
        }
        if o.il_table {
            let emitted: BTreeSet<&String> = names.iter().collect();
            let mut rows: Vec<&drain::IlFn> = il.fns.values().collect();
            rows.sort_by_key(|f| (f.off, u8::from(f.stream != Stream::Ex), f.name.clone()));
            for f in rows {
                out.push(format!(
                    "      IL {:>6} {:#07x} {:>6} B {} {}",
                    f.stream.tag(),
                    f.off,
                    f.size,
                    if emitted.contains(&f.name) {
                        "OBJ"
                    } else {
                        "-  "
                    },
                    f.name
                ));
            }
        }
        t.n += 1;
        t.exact += usize::from(ok.0);
        t.demand_exact += usize::from(ok.1);
        let known: Vec<String> = names
            .iter()
            .filter(|x| il.fns.contains_key(*x))
            .cloned()
            .collect();
        let (dp, dd) = (
            drain::against(&known, &drain::predicted(&known, &il)),
            drain::against(&known, &drain::predicted_demand(&known, &il, &refs)),
        );
        let demand = dd < dp;
        let away: BTreeSet<&str> = s
            .away
            .iter()
            .filter_map(|&i| v.get(i))
            .map(|m| m.name.as_str())
            .collect();
        let mut by_rva: Vec<&Member> = v
            .iter()
            .filter(|m| il.fns.contains_key(&m.name) && !away.contains(m.name.as_str()))
            .collect();
        by_rva.sort_by_key(|m| (m.rva, m.name.clone()));
        let want: Vec<String> = by_rva.iter().map(|m| m.name.clone()).collect();
        let base = drain::against(
            &want,
            &drain::predicted_shifted(&known, &il, &refs, u32::MAX, 0, demand),
        );
        out.push(format!(
            "      reference order vs the {} merge (the closer of the two on this object): {base} displaced",
            if demand { "demand" } else { "offset" }
        ));
        if !o.show.is_empty() {
            let pred = drain::predicted_shifted(&known, &il, &refs, u32::MAX, 0, demand);
            let rva: BTreeMap<&str, u32> = v.iter().map(|m| (m.name.as_str(), m.rva)).collect();
            let ref_prev: BTreeMap<&str, &str> = want
                .windows(2)
                .filter_map(|w| match w {
                    [a, b] => Some((b.as_str(), a.as_str())),
                    _ => None,
                })
                .collect();
            for (i, n) in pred.iter().enumerate() {
                let (tag, off, size) = il
                    .fns
                    .get(n)
                    .map_or(("-", 0, 0), |f| (f.stream.tag(), f.off, f.size));
                let prev = i
                    .checked_sub(1)
                    .and_then(|p| pred.get(p))
                    .map(String::as_str);
                let moved = match (ref_prev.get(n.as_str()), prev) {
                    (Some(r), Some(p)) if *r != p => format!("  REF<- {r}"),
                    (Some(r), None) => format!("  REF<- {r}"),
                    (None, Some(_)) if want.first().is_some_and(|w| w == n) => {
                        "  REF<- (start)".to_string()
                    }
                    _ => String::new(),
                };
                out.push(format!(
                    "      {i:3} {tag:>3}@{off:#07x} {size:6} B  ref {}  {n}{moved}",
                    rva.get(n.as_str())
                        .map_or_else(|| "-".to_string(), |r| format!("{r:#08x}"))
                ));
            }
        }
        for &i in &s.order {
            let Some(m) = v.get(i) else {
                continue;
            };
            let Some(f) = il.fns.get(&m.name) else {
                out.push(format!(
                    "      ORDER {} : no .gl record (a thunk: not predicted)",
                    m.name
                ));
                continue;
            };
            if f.stream != Stream::Pch {
                out.push(format!(
                    "      ORDER {:#08x} {} : {}@{:#x} {} B -- a .cpp body: it moves with its definition or \
                     generation point, not with a header",
                    m.rva,
                    m.name,
                    f.stream.tag(),
                    f.off,
                    f.size
                ));
                t.ex_bodies += 1;
                continue;
            }
            let cost = drain::shift_points().saturating_mul(work_of(&known));
            if cost > sweep_left {
                out.push(format!(
                    "      ORDER {:#08x} {} : pch@{:#x} {} B; not swept ({cost} name-evaluations, more than the {} \
                     left of this run's {SWEEP_WORK})",
                    m.rva, m.name, f.off, f.size, sweep_left
                ));
                continue;
            }
            sweep_left -= cost;
            let b = drain::best_shift(&known, &il, &refs, &want, f.off, demand, d.jobs);
            if b.best < base {
                t.levers += 1;
            }
            out.push(format!(
                "      ORDER {:#08x} {} : pch@{:#x} {} B; {} IL B inserted in the pch ahead of it -> {} displaced \
                 (now {base})",
                m.rva,
                m.name,
                f.off,
                f.size,
                if b.lo == b.hi {
                    format!("{:+}", b.lo)
                } else {
                    format!("{:+}..{:+}", b.lo, b.hi)
                },
                b.best
            ));
        }
        common.push(Common {
            stem: tu.clone(),
            known,
            il,
            refs,
            want,
            demand,
            base,
        });
    }
    out.push(format!(
        "  PREDICT TOTAL: {}/{} object(s) EXACT by offset, {}/{} with demand; ORDER bodies: {} pch body(ies) with a \
         pch shift that lowers their object's displacement, {} .cpp-stream body(ies)",
        t.exact, t.n, t.demand_exact, t.n, t.levers, t.ex_bodies
    ));
    if o.ob0 {
        out.push(format!(
            "  /Ob0 TOTAL: {}/{} object(s) a SUBSEQUENCE of their /Ob0 compile's order",
            t.ob0_exact, t.ob0_n
        ));
        out.push(format!(
            "  HYBRID TOTAL: {}/{} object(s) EXACT (the demand merge over the real IL offsets with the /Ob0 references)",
            t.hybrid_exact, t.ob0_n
        ));
    }
    common_prefix(&common, d.jobs, sweep_left, &mut out);
    Ok((out, t))
}

pub const SWEEP_WORK: u64 = 1 << 32;

pub const PREFIX_POINTS: i64 = 0x2000;

pub const PREFIX_STEP: i64 = 16;

fn work_of(known: &[String]) -> u64 {
    u64::try_from(known.len()).unwrap_or(u64::MAX).max(1)
}

fn common_prefix(common: &[Common], jobs: usize, left: u64, out: &mut Vec<String>) {
    if common.is_empty() {
        return;
    }
    let points = PREFIX_POINTS.unsigned_abs() + 1;
    let cost = common
        .iter()
        .map(|c| points.saturating_mul(work_of(&c.known)))
        .fold(0u64, u64::saturating_add);
    if cost > left {
        out.push(format!(
            "  COMMON PREFIX: not swept ({cost} name-evaluations over {} object(s), more than the {left} left of this \
             run's {SWEEP_WORK})",
            common.len()
        ));
        return;
    }
    let xs: Vec<i64> = (0..=PREFIX_POINTS).map(|k| k * PREFIX_STEP).collect();
    let curves: Vec<Vec<usize>> = common
        .iter()
        .map(|c| {
            msvc6_diff::par::map(&xs, jobs, |&x| {
                drain::against(
                    &c.want,
                    &drain::predicted_shifted(&c.known, &c.il, &c.refs, 0, x, c.demand),
                )
            })
        })
        .collect();
    let total: Vec<usize> = (0..xs.len())
        .map(|i| curves.iter().filter_map(|c| c.get(i)).sum())
        .collect();
    let best = total.iter().copied().min().unwrap_or(0);
    let run = |c: &[usize], b: usize| -> String {
        let mut o = Vec::new();
        let mut i = 0usize;
        while i < c.len() && o.len() < 3 {
            if c.get(i) == Some(&b) {
                let s = i;
                while c.get(i + 1) == Some(&b) {
                    i += 1;
                }
                o.push(format!(
                    "+{}..+{}",
                    xs.get(s).copied().unwrap_or(0),
                    xs.get(i).copied().unwrap_or(0)
                ));
            }
            i += 1;
        }
        o.join(" ")
    };
    out.push(format!(
        "  COMMON PREFIX (every pch body of every object +x IL bytes): total displaced {} at x=0, best {best} at x {}",
        total.first().copied().unwrap_or(0),
        run(&total, best)
    ));
    let at = total.iter().position(|&t| t == best).unwrap_or(0);
    for (c, curve) in common.iter().zip(&curves) {
        let ob = curve.iter().copied().min().unwrap_or(0);
        out.push(format!(
            "    {}: {} now, {} at the best total's first x (+{}), own best {ob} at x {}",
            c.stem,
            c.base,
            curve.get(at).copied().unwrap_or(0),
            xs.get(at).copied().unwrap_or(0),
            run(curve, ob)
        ));
    }
}
