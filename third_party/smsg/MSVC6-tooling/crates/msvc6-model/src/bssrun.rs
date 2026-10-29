use crate::bss::{
    GUARD_MAX, Var, Verdict, compact, guard_fits, inversion_reading, judge, mark_zero_inits,
    misplaced_with_free_guards, vars_of, verdict_lines,
};
use crate::bssplan::{PlanOpts, plan_lines};
use anyhow::{Result, anyhow};
use msvc6_diff::env::Env;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
struct MergeBlock {
    stem: String,
    lo: u32,
    hi: u32,
    walked: Vec<u32>,
    appended: usize,
}

impl MergeBlock {
    fn of(stem: &str, vars: &[Var]) -> Option<Self> {
        let mut placed: Vec<&Var> = vars
            .iter()
            .filter(|v| v.image.is_some() && !v.foreign)
            .collect();
        placed.sort_by_key(|v| v.image);
        let lo = placed.first()?.image?;
        let last = placed.last()?;
        let hi = last.image?.saturating_add(last.size);
        Some(Self {
            stem: stem.to_string(),
            lo,
            hi,
            walked: placed
                .iter()
                .filter(|v| v.zero_init.is_none())
                .map(|v| v.bucket)
                .collect(),
            appended: placed.iter().filter(|v| v.zero_init.is_some()).count(),
        })
    }
}

fn choose(n: usize, k: usize) -> f64 {
    (0..k).fold(1.0, |acc, i| {
        acc * f64::from(u32::try_from(n.saturating_sub(i)).unwrap_or(u32::MAX))
            / f64::from(u32::try_from(i.saturating_add(1)).unwrap_or(u32::MAX))
    })
}

fn merge_screen(program: &str, blocks: &mut [MergeBlock]) -> Vec<String> {
    blocks.sort_by_key(|b| b.lo);
    let mut out = vec![
        format!(
            "{program}: TUs ADJACENT IN THE REFERENCE .bss, judged as ONE original TU (walked buckets must not cross)"
        ),
        "  verdict      odds 1-in  A-walked  B-walked  A  ->  B   (A block, B block)".to_string(),
    ];
    let (mut refuted, mut compatible) = (0usize, 0usize);
    for w in blocks.windows(2) {
        let [a, b] = w else { continue };
        let a_max = a.walked.iter().max();
        let b_min = b.walked.iter().min();
        let (verdict, odds) = if a.appended > 0 && !b.walked.is_empty() {
            ("REFUTED-ZERO", String::from("-"))
        } else if let (Some(x), Some(y)) = (a_max, b_min) {
            if x > y {
                ("REFUTED", String::from("-"))
            } else {
                (
                    if x == y { "SAME-BUCKET" } else { "COMPATIBLE" },
                    format!(
                        "{:.0}",
                        choose(a.walked.len() + b.walked.len(), a.walked.len())
                    ),
                )
            }
        } else {
            ("UNJUDGED", String::from("-"))
        };
        if verdict.starts_with("REFUTED") {
            refuted = refuted.saturating_add(1);
        } else if verdict != "UNJUDGED" {
            compatible = compatible.saturating_add(1);
        }
        out.push(format!(
            "  {verdict:<12} {odds:>9}  {:>8}  {:>8}  {} -> {}   ({:#x}..{:#x}, {:#x}..{:#x}){}",
            a.walked.len(),
            b.walked.len(),
            a.stem,
            b.stem,
            a.lo,
            a.hi,
            b.lo,
            b.hi,
            if b.lo < a.hi { "  OVERLAP" } else { "" }
        ));
    }
    out.push(format!(
        "TOTAL {program}: {} TU block(s), {} adjacent pair(s): {refuted} refuted, {compatible} compatible",
        blocks.len(),
        blocks.len().saturating_sub(1)
    ));
    out
}

#[derive(Debug, Default)]
pub struct Run {
    pub lines: Vec<String>,
    pub warnings: Vec<String>,
    pub refused: Vec<(String, String)>,
    pub verdicts: Vec<Verdict>,
}

#[derive(Debug, Clone, Default)]
pub struct Opts {
    pub src: Option<String>,
    pub detail: bool,
    pub plan: Option<PlanOpts>,
    pub merge_screen: bool,
}

fn stem(o: &Path) -> String {
    o.file_stem()
        .map_or_else(String::new, |f| f.to_string_lossy().into_owned())
}

fn zero_inits(
    stem: &str,
    vars: &mut [Var],
    source_of: &dyn Fn(&str) -> Option<PathBuf>,
) -> Result<(), String> {
    let Some(p) = source_of(stem) else {
        return Err(
            "no source for this TU in the image's project or a workspace library its objects come from, so \
             its `= 0` variables (appended in definition order) cannot be told from the walked ones"
                .to_string(),
        );
    };
    match msvc6_base::srcio::read(&p) {
        Ok(text) => {
            mark_zero_inits(&text, vars).map_err(|e| format!("its source {}: {e:#}", p.display()))
        }
        Err(e) => Err(format!("its source {} cannot be read: {e}", p.display())),
    }
}

#[allow(clippy::too_many_lines)]
pub fn run(
    program: &str,
    env: &Env,
    objs: &[PathBuf],
    o: &Opts,
    source_of: &dyn Fn(&str) -> Option<PathBuf>,
) -> Result<Run> {
    let want = o.src.as_ref().map(|s| {
        s.trim_end_matches(".cpp")
            .trim_end_matches(".c")
            .to_string()
    });
    if let Some(w) = &want
        && !objs.iter().any(|x| stem(x).eq_ignore_ascii_case(w))
    {
        return Err(anyhow!("no object for TU '{w}'"));
    }
    let mut run = Run::default();
    let mut blocks: Vec<MergeBlock> = Vec::new();
    for (path, r) in crate::dataseat::seat_all(env, objs) {
        let s = stem(&path);
        if want.as_ref().is_some_and(|w| !s.eq_ignore_ascii_case(w)) {
            continue;
        }
        let r = match r {
            Ok(r) => Some(r),
            Err(e) => {
                run.warnings.push(format!("{}: {e:#}", path.display()));
                None
            }
        };
        let bytes = msvc6_diff::comdat::read(&path)?;
        let mut vars = vars_of(&bytes, r.as_ref())?;
        if o.merge_screen {
            if MergeBlock::of(&s, &vars).is_none() {
                continue;
            }
            if let Err(why) = zero_inits(&s, &mut vars, source_of) {
                run.refused.push((s.clone(), why));
                continue;
            }
            if let Some(b) = MergeBlock::of(&s, &vars) {
                blocks.push(b);
            }
            continue;
        }
        if vars.len() < 2 {
            continue;
        }
        if let Err(why) = zero_inits(&s, &mut vars, source_of) {
            run.refused.push((s.clone(), why));
            continue;
        }
        run.verdicts.push(judge(&s, vars)?);
    }
    if o.merge_screen {
        run.lines = merge_screen(program, &mut blocks);
        return Ok(run);
    }
    let single = run.verdicts.len() == 1;
    if let Some(p) = &o.plan {
        let [v] = run.verdicts.as_slice() else {
            return Err(anyhow!("--plan reads one TU: name it with --src"));
        };
        run.lines = plan_lines(v, p)?;
        return Ok(run);
    }
    let mut out = Vec::new();
    for v in &run.verdicts {
        if single || o.detail && !v.image_exact {
            out.extend(verdict_lines(v));
        }
    }
    if !single {
        out.push("model  reference  vars  placed  misplaced  TU".to_string());
        for v in &run.verdicts {
            out.push(format!(
                "{:<5}  {:<9}  {:>4}  {:>6}  {:>9}  {}",
                if v.model_exact { "EXACT" } else { "DIFF" },
                if v.image_order.len() < 2 {
                    "n/a"
                } else if v.image_exact {
                    "EXACT"
                } else {
                    "DIFF"
                },
                v.vars.len(),
                v.image_order.len(),
                v.misplaced.len(),
                v.stem
            ));
        }
    }
    let guards: Vec<(&Verdict, crate::bss::GuardFit)> = run
        .verdicts
        .iter()
        .filter(|v| v.image_order.len() >= 2)
        .flat_map(|v| guard_fits(v).into_iter().map(move |g| (v, g)))
        .collect();
    if !single && !guards.is_empty() {
        out.push(
            "GUARDS  ours  need-buckets  shift  TU  (fitting $S numbers; shift = nearest original minus ours)"
                .to_string(),
        );
        for (v, g) in &guards {
            out.push(format!(
                "  $S{:<4} {:>4}..={:<4}  {:>5}  {}  {{{}}}",
                g.n,
                g.lo,
                g.hi,
                g.nearest_shift()
                    .map_or_else(|| "none".to_string(), |d| format!("{d:+}")),
                v.stem,
                compact(&g.fits, 4)
            ));
        }
        let common: Vec<i64> = (-(i64::from(GUARD_MAX))..=i64::from(GUARD_MAX))
            .filter(|&d| {
                guards.iter().all(|(_, g)| {
                    u32::try_from(i64::from(g.n) + d)
                        .is_ok_and(|m| g.fits.binary_search(&m).is_ok())
                })
            })
            .collect();
        out.push(format!(
            "  one shift fits all {} guard(s): {}",
            guards.len(),
            if common.is_empty() {
                "NONE (no single header offset explains every guard)".to_string()
            } else {
                format!("{{{}}}", compact(&common, 6))
            }
        ));
    }
    let judged: Vec<&Verdict> = run
        .verdicts
        .iter()
        .filter(|v| v.image_order.len() >= 2)
        .collect();
    if !single {
        for v in &judged {
            for &(a, b) in &v.inverted {
                out.push(format!("  {}: {}", v.stem, inversion_reading(v, a, b)));
            }
        }
    }
    out.push(format!(
        "TOTAL {program}: model exact {}/{} TU(s) with >=2 .bss vars ({} vars); reference bucket-ordered {}/{} \
         judgeable TU(s), {} misplaced var(s) ({} with every $S guard number free), {} same-bucket \
         inversion(s); {} var(s) out of image order",
        run.verdicts.iter().filter(|v| v.model_exact).count(),
        run.verdicts.len(),
        run.verdicts.iter().map(|v| v.vars.len()).sum::<usize>(),
        judged.iter().filter(|v| v.image_exact).count(),
        judged.len(),
        judged.iter().map(|v| v.misplaced.len()).sum::<usize>(),
        judged
            .iter()
            .map(|v| misplaced_with_free_guards(v))
            .sum::<usize>(),
        judged.iter().map(|v| v.inverted.len()).sum::<usize>(),
        judged.iter().map(|v| v.out_of_order).sum::<usize>()
    ));
    run.lines = out;
    Ok(run)
}
