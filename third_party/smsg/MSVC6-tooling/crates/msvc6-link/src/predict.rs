use anyhow::{Result, anyhow};
use serde::Serialize;
use std::collections::{HashMap, HashSet};

use crate::icf::{
    self, Array, Definer, Dump, Model, Move, Parsed, apply_move, assemble, by_label, first_of,
    fold, key, log_pairs, resolve_group, show, sorted,
};
use crate::joint::{self, Attested, Joint};
use crate::objkey::Keys;
use crate::owners::RefOwners;
use crate::pools::{Scored, focus_pool, ref_pools, score, score_lines, search_inserts, tally};
use crate::verbose::Group;

#[derive(Debug)]
pub struct JointInput {
    pub rows: Vec<Attested>,
    pub sources: String,
    pub other: Option<Array>,
}

#[derive(Debug)]
pub struct Input<'a> {
    pub label: &'a str,
    pub parsed: &'a Parsed,
    pub base: Option<&'a Parsed>,
    pub dead: &'a HashSet<(String, String)>,
    pub groups: &'a [Group],
    pub model: Model,
    pub want: &'a [String],
    pub moves: &'a [Move],
    pub defs: &'a [Definer],
    pub dump: &'a [Dump],
    pub owners: &'a RefOwners,
    pub keys: &'a Keys,
    pub search_insert: Option<&'a str>,
    pub focus: Option<u32>,
    pub joint: Option<JointInput>,
    pub verbose: bool,
    pub jobs: usize,
}

#[derive(Clone, Debug, Default, Serialize)]
pub struct Summary {
    pub objects: usize,
    pub missing: usize,
    pub refused: usize,
    pub candidates: usize,
    pub dead: usize,
    pub groups: usize,
    pub order_ok: usize,
    pub replay_ok: usize,
    pub absent: usize,
    pub classes: usize,
    pub pairs_agree: usize,
    pub pairs: usize,
    pub pools: usize,
    pub score_ok: usize,
    pub score_edge: usize,
    pub score_miss: usize,
    pub score_unknown: usize,
}

#[derive(Clone, Debug, Serialize)]
pub struct GroupRow {
    pub group: usize,
    pub order_ok: bool,
    pub replay_ok: bool,
    pub absent: bool,
    pub size: u32,
    pub relocs: u16,
    pub members: usize,
    pub log: String,
    pub predicted: String,
}

#[derive(Debug, Default)]
pub struct Report {
    pub lines: Vec<String>,
    pub summary: Summary,
    pub rows: Vec<GroupRow>,
}

pub fn joint_other(
    other: &Parsed,
    rows: &[Attested],
    dead: &HashSet<(String, String)>,
    model: Model,
    defs: &[Definer],
) -> Result<Array> {
    let mut dead_other = dead.clone();
    for r in rows {
        dead_other.insert((r.name.clone(), r.obj.to_ascii_lowercase()));
    }
    assemble(other, &dead_other, model, defs)
}

pub fn run(inp: &Input<'_>) -> Result<Report> {
    let mut rep = Report::default();
    let out = &mut rep.lines;
    let model = inp.model;
    let arr = assemble(inp.parsed, inp.dead, model, inp.defs)?;
    let mut seq: Vec<usize> = (0..arr.cands.len()).collect();
    for m in inp.moves {
        seq = apply_move(&arr, &seq, m)
            .ok_or_else(|| anyhow!("--move {}: no such candidate", m.name))?;
    }
    let srt = sorted(&arr, &seq, model)?;
    let root = fold(&arr, &srt)?;
    let pos: HashMap<usize, usize> = srt.iter().enumerate().map(|(p, &c)| (c, p)).collect();
    let p = |c: usize| pos.get(&c).copied().unwrap_or(usize::MAX);
    let s = &mut rep.summary;
    s.objects = arr.objects.len();
    s.missing = arr.missing.len();
    s.refused = inp.parsed.refused.len();
    s.candidates = arr.cands.len();
    s.dead = arr.cands.iter().filter(|c| c.dead).count();
    out.push(format!(
        "icf-predict {}: {} object(s) ({} missing), {} candidate(s) ({} dead, {} static, {} associative{}), \
         sorted {}",
        inp.label,
        arr.objects.len(),
        arr.missing.len(),
        arr.cands.len(),
        s.dead,
        arr.cands.iter().filter(|c| !c.external && c.sel != 5).count(),
        arr.cands.iter().filter(|c| c.sel == 5).count(),
        if model.externals_only { " LEFT OUT" } else { "" },
        srt.len()
    ));
    for m in &arr.missing {
        out.push(format!("  MISSING {m}"));
    }
    for (o, e) in &inp.parsed.refused {
        out.push(format!("  REFUSED {o}: {e}"));
    }
    for k in inp.dump {
        let hit = |x: &icf::Cand| match k {
            Dump::Key(s, r) => key(x) == (*s, *r),
            Dump::Obj(o) => arr
                .objects
                .get(x.obj)
                .is_some_and(|n| n.eq_ignore_ascii_case(o)),
        };
        match k {
            Dump::Key(s, r) => out.push(format!("DUMP key {s}:{r} in append order")),
            Dump::Obj(o) => out.push(format!("DUMP object {o} in append order")),
        }
        for (ai, &c) in seq.iter().enumerate() {
            let Some(x) = arr.cands.get(c) else { continue };
            if hit(x) {
                out.push(format!(
                    "  append {ai:<5} key {:>5}:{:<3} sorted @{:<5} {}{}{}",
                    x.size,
                    x.nreloc,
                    p(c),
                    show(&arr, c),
                    if x.dead { " DEAD" } else { "" },
                    if x.dup { " DUP" } else { "" }
                ));
            }
        }
    }
    let labels = by_label(&arr);
    let groups = inp.groups;
    let resolved: Vec<Option<Vec<usize>>> = groups
        .iter()
        .map(|g| resolve_group(&arr, &labels, g))
        .collect();
    let (mut ok_order, mut ok_replay, mut absent) = (0usize, 0usize, 0usize);
    for (gi, (g, r)) in groups.iter().zip(&resolved).enumerate() {
        let Some(found) = r else {
            absent += 1;
            out.push(format!(
                "G{:<3} ABSENT  a member is not in the array (survivor {} {})",
                gi + 1,
                g.survivor.name,
                g.survivor.obj
            ));
            rep.rows.push(GroupRow {
                group: gi + 1,
                order_ok: false,
                replay_ok: false,
                absent: true,
                size: 0,
                relocs: 0,
                members: g.replaced.len() + 1,
                log: format!("{} {}", g.survivor.name, g.survivor.obj),
                predicted: String::new(),
            });
            continue;
        };
        let first = found.iter().copied().min_by_key(|&c| p(c));
        let surv = found.first().copied();
        let order_ok = first.is_some() && first == surv;
        let replay_root = surv.and_then(|c| root.get(c)).copied();
        let class: HashSet<usize> = (0..arr.cands.len())
            .filter(|&c| root.get(c).copied() == replay_root)
            .collect();
        let replay_ok = replay_root == surv
            && class.len() == found.len()
            && found.iter().all(|c| class.contains(c));
        ok_order += usize::from(order_ok);
        ok_replay += usize::from(replay_ok);
        let x = first.and_then(|c| arr.cands.get(c));
        let log = show(&arr, surv.unwrap_or(usize::MAX));
        let predicted = show(&arr, first.unwrap_or(usize::MAX));
        out.push(format!(
            "G{:<3} ORDER {}  REPLAY {}  size {} relocs {}  {} member(s)  log {}  predicted {} @{}",
            gi + 1,
            if order_ok { "ok  " } else { "MISS" },
            if replay_ok { "ok  " } else { "MISS" },
            x.map_or(0, |c| c.size),
            x.map_or(0, |c| c.nreloc),
            found.len(),
            log,
            predicted,
            first.map_or(0, p),
        ));
        rep.rows.push(GroupRow {
            group: gi + 1,
            order_ok,
            replay_ok,
            absent: false,
            size: x.map_or(0, |c| c.size),
            relocs: x.map_or(0, |c| c.nreloc),
            members: found.len(),
            log,
            predicted,
        });
        if inp.verbose || !order_ok {
            let mut ms = found.clone();
            ms.sort_by_key(|&c| p(c));
            for c in ms {
                out.push(format!("      @{:<5} {}", p(c), show(&arr, c)));
            }
        }
        if !replay_ok {
            let logged: HashSet<usize> = found.iter().copied().collect();
            let mut extra: Vec<usize> = class.difference(&logged).copied().collect();
            extra.sort_by_key(|&c| p(c));
            for c in extra {
                let dead = arr.cands.get(c).is_some_and(|x| x.dead);
                out.push(format!(
                    "      replay also folds @{:<5} {}{}",
                    p(c),
                    show(&arr, c),
                    if dead { " DEAD" } else { "" }
                ));
            }
            for c in found.iter().filter(|c| !class.contains(c)) {
                out.push(format!(
                    "      replay leaves out @{:<5} {}",
                    p(*c),
                    show(&arr, *c)
                ));
            }
        }
    }
    let (agree, pairs) = log_pairs(&arr, &resolved, &p);
    out.push(format!(
        "PAIRS {agree} of {pairs} log-order pair(s) the replay sorts the same way"
    ));
    let mut members_of: HashMap<usize, usize> = HashMap::new();
    for &r in &root {
        *members_of.entry(r).or_default() += 1;
    }
    let n_classes = (0..arr.cands.len())
        .filter(|&c| root.get(c) == Some(&c) && members_of.get(&c).copied().unwrap_or(0) > 1)
        .count();
    out.push(String::new());
    out.push(format!(
        "SUMMARY {} log group(s): survivor by ORDER {ok_order}, by REPLAY {ok_replay} (survivor and members), \
         {absent} with a member outside the array; the replay makes {n_classes} group(s)",
        groups.len(),
    ));
    let s = &mut rep.summary;
    s.groups = groups.len();
    s.order_ok = ok_order;
    s.replay_ok = ok_replay;
    s.absent = absent;
    s.classes = n_classes;
    s.pairs_agree = agree;
    s.pairs = pairs;
    let base: Vec<Option<usize>> = if inp.want.is_empty() {
        Vec::new()
    } else {
        resolved
            .iter()
            .map(|r| match r {
                Some(m) => first_of(&arr, &seq, model, m),
                None => Ok(None),
            })
            .collect::<Result<_>>()?
    };
    for w in inp.want {
        let Some((gi, Some(members))) = groups
            .iter()
            .zip(&resolved)
            .enumerate()
            .find(|(_, (g, _))| g.members().any(|s| &s.name == w))
            .map(|(i, (_, r))| (i, r.clone()))
        else {
            out.push(String::new());
            out.push(format!("WANT {w}: in no resolved log group"));
            continue;
        };
        let Some(&target_c) = members.iter().find(|&&c| {
            arr.cands
                .get(c)
                .is_some_and(|x| &x.name == w || &x.label == w)
        }) else {
            continue;
        };
        out.push(String::new());
        out.push(format!(
            "WANT {} to survive G{} ({} member(s)); now {}",
            show(&arr, target_c),
            gi + 1,
            members.len(),
            base.get(gi)
                .copied()
                .flatten()
                .map_or_else(|| "?".into(), |c| show(&arr, c))
        ));
        let mut hits = 0usize;
        for &m in &members {
            let Some(mc) = arr.cands.get(m) else { continue };
            let mut slots: Vec<Option<String>> = seq
                .iter()
                .filter_map(|&i| arr.cands.get(i).map(|c| (i, c)))
                .filter(|(i, c)| c.obj == mc.obj && *i != m)
                .map(|(_, c)| Some(c.name.clone()))
                .collect();
            slots.push(None);
            for before in slots {
                let mv = Move {
                    name: mc.name.clone(),
                    before,
                };
                let Some(s2) = apply_move(&arr, &seq, &mv) else {
                    continue;
                };
                if first_of(&arr, &s2, model, &members)? != Some(target_c) {
                    continue;
                }
                let mut rerolls = 0usize;
                for (hi, (r, b)) in resolved.iter().zip(&base).enumerate() {
                    if hi == gi {
                        continue;
                    }
                    let now = match r {
                        Some(ms) => first_of(&arr, &s2, model, ms)?,
                        None => None,
                    };
                    rerolls += usize::from(now != *b);
                }
                hits += 1;
                out.push(format!(
                    "  MOVE {}@{}  ({}): {} other log group(s) change survivor",
                    mc.name,
                    mv.before.as_deref().unwrap_or(""),
                    arr.objects.get(mc.obj).map_or("?", String::as_str),
                    rerolls
                ));
            }
        }
        out.push(format!("  {hits} move(s)"));
    }
    let pools = ref_pools(groups, inp.owners);
    let now = score(&arr, &seq, model, &pools, inp.owners, inp.keys)?;
    let base_scored: Option<(Array, Vec<Scored>)> = match inp.base {
        Some(p0) => {
            let a0 = assemble(p0, inp.dead, model, &[])?;
            let s0: Vec<usize> = (0..a0.cands.len()).collect();
            let before = score(&a0, &s0, model, &pools, inp.owners, inp.keys)?;
            Some((a0, before))
        }
        None => None,
    };
    out.extend(score_lines(
        &pools,
        inp.owners,
        (&arr, &now),
        base_scored.as_ref().map(|(a, b)| (a, b.as_slice())),
        inp.verbose,
    ));
    let (ok, edge, miss, unk) = tally(&now);
    let s = &mut rep.summary;
    s.pools = pools.len();
    s.score_ok = ok;
    s.score_edge = edge;
    s.score_miss = miss;
    s.score_unknown = unk;
    let focus = focus_pool(&pools, inp.focus);
    if let Some(keys) = inp.search_insert {
        out.extend(search_inserts(
            &arr, &seq, model, &pools, inp.owners, keys, focus, inp.keys,
        )?);
    }
    if let Some(j) = &inp.joint {
        let wants = joint::wanted_members(&arr, groups, &resolved, inp.want);
        out.extend(joint::joint_search(&Joint {
            arr: &arr,
            seq: &seq,
            model,
            pools: &pools,
            owners: inp.owners,
            keys: inp.keys,
            wants: &wants,
            rows: &j.rows,
            sources: &j.sources,
            other: j.other.as_ref(),
            jobs: inp.jobs,
        })?);
    }
    Ok(rep)
}
