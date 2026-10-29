use anyhow::{Result, anyhow, bail};
use std::collections::{HashMap, HashSet};

use crate::comdat::{ObjComdat, is_candidate};
use crate::icf::{Array, Cand, Model, Parsed, Unit, show, sorted};
use crate::objkey::Keys;
use crate::owners::RefOwners;
use crate::pools::{Pool, score_sorted, tally, verdict_code};
use crate::seq::lis_strict;
use crate::verbose::Group;

pub const JOINT_EXHAUSTIVE_ROWS: usize = 14;
pub const JOINT_BUDGET: usize = 200_000;
pub const JOINT_SHOW: usize = 12;
pub const ROWS_MAX: usize = 4096;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Attested {
    pub obj: String,
    pub before: Vec<String>,
    pub open_end: bool,
    pub keys: Vec<Vec<(u32, u16)>>,
    pub name: String,
    pub evidence: String,
}

impl Attested {
    #[must_use]
    pub fn source(&self) -> &'static str {
        if self.open_end { "derived" } else { "written" }
    }

    pub fn parse_row(line: &str) -> Result<Option<Self>> {
        let t = line.trim_end_matches(['\r', '\n']);
        if t.trim().is_empty() || t.trim_start().starts_with('#') {
            return Ok(None);
        }
        let f: Vec<&str> = t.split('\t').collect();
        let field = |i: usize| f.get(i).map_or("", |s| s.trim());
        if f.len() < 4 || field(0).is_empty() || field(3).is_empty() {
            bail!(
                "--candidates row '{t}': want OBJECT, BEFORE, KEYS, NAME, EVIDENCE separated by tabs"
            );
        }
        if field(4).is_empty() {
            bail!(
                "--candidates row '{t}': no EVIDENCE -- a row names what attests the function (a sibling build's \
                 address, an import only it pulls, its strings); `--search-insert' is the phantom search"
            );
        }
        let mut keys = Vec::new();
        for alt in field(2).split(',').map(str::trim).filter(|a| !a.is_empty()) {
            let mut elems = Vec::new();
            for k in alt.split('+').map(str::trim) {
                let (s, r) = k
                    .split_once(':')
                    .ok_or_else(|| anyhow!("--candidates key '{k}': want SIZE:NRELOC"))?;
                elems.push((s.trim().parse::<u32>()?, r.trim().parse::<u16>()?));
            }
            keys.push(elems);
        }
        if keys.is_empty() {
            bail!("--candidates row '{t}': no SIZE:NRELOC key");
        }
        Ok(Some(Self {
            obj: field(0).to_string(),
            before: field(1)
                .split('|')
                .map(str::trim)
                .filter(|b| !b.is_empty())
                .map(str::to_string)
                .collect(),
            open_end: false,
            keys,
            name: field(3).to_string(),
            evidence: field(4).to_string(),
        }))
    }
}

pub fn parse_candidates(text: &str) -> Result<Vec<Attested>> {
    let mut out = Vec::new();
    for l in text.lines() {
        if let Some(r) = Attested::parse_row(l)? {
            out.push(r);
            if out.len() > ROWS_MAX {
                bail!("--candidates: more than {ROWS_MAX} rows");
            }
        }
    }
    Ok(out)
}

#[must_use]
pub fn derive_candidates(ours: &Parsed, other: &Parsed, from: &str) -> (Vec<Attested>, String) {
    let ident = |c: &ObjComdat| (c.external, c.name.clone());
    let mut first_def: HashMap<&str, usize> = HashMap::new();
    for (i, u) in other.units.iter().enumerate() {
        if let Unit::Obj(_, x) = u {
            for n in x
                .comdats
                .iter()
                .filter(|c| c.external)
                .map(|c| c.name.as_str())
                .chain(x.plain_defs.iter().map(String::as_str))
            {
                first_def.entry(n).or_insert(i);
            }
        }
    }
    let (mut rekeyed, mut gone, mut reordered) = (Vec::new(), 0usize, 0usize);
    let ours_ext: HashSet<&str> = ours
        .units
        .iter()
        .filter_map(|u| match u {
            Unit::Obj(_, i) => Some(i),
            _ => None,
        })
        .flat_map(|i| {
            i.comdats
                .iter()
                .filter(|c| c.external)
                .map(|c| c.name.as_str())
                .chain(i.plain_defs.iter().map(String::as_str))
        })
        .collect();
    let mut out = Vec::new();
    for (ui, (uo, ux)) in ours.units.iter().zip(&other.units).enumerate() {
        let (Unit::Obj(on, oi), Unit::Obj(xn, xi)) = (uo, ux) else {
            continue;
        };
        if !on.eq_ignore_ascii_case(xn) {
            continue;
        }
        let have: HashMap<(bool, String), (u32, u16)> = oi
            .comdats
            .iter()
            .filter(|c| c.parent.is_none())
            .map(|c| (ident(c), (c.size, c.nreloc)))
            .collect();
        let parents: Vec<&ObjComdat> = xi.comdats.iter().filter(|c| c.parent.is_none()).collect();
        let theirs: HashSet<(bool, String)> = parents.iter().map(|c| ident(c)).collect();
        gone += have.keys().filter(|k| !theirs.contains(*k)).count();
        let at_ours: HashMap<(bool, String), u32> = oi
            .comdats
            .iter()
            .filter(|c| c.parent.is_none())
            .enumerate()
            .map(|(i, c)| (ident(c), u32::try_from(i).unwrap_or(u32::MAX)))
            .collect();
        let common: Vec<u32> = parents
            .iter()
            .filter_map(|p| at_ours.get(&ident(p)).copied())
            .collect();
        reordered += common.len() - lis_strict(&common).len();
        for (k, p) in parents.iter().enumerate() {
            if let Some(&key0) = have.get(&ident(p)) {
                if key0 != (p.size, p.nreloc) && is_candidate(p) {
                    rekeyed.push(format!(
                        "{} {}:{} -> {}:{} {}",
                        p.name, key0.0, key0.1, p.size, p.nreloc, on
                    ));
                }
                continue;
            }
            if !is_candidate(p)
                || (p.external
                    && (ours_ext.contains(p.name.as_str())
                        || first_def.get(p.name.as_str()).is_some_and(|&f| f < ui)))
            {
                continue;
            }
            let mut elems = vec![(p.size, p.nreloc)];
            elems.extend(
                xi.comdats
                    .iter()
                    .filter(|c| c.parent == Some(p.sec) && is_candidate(c))
                    .map(|c| (c.size, c.nreloc)),
            );
            let before = parents
                .iter()
                .skip(k + 1)
                .filter(|q| have.contains_key(&ident(q)) && is_candidate(q))
                .map(|q| q.name.clone())
                .collect();
            out.push(Attested {
                obj: on.clone(),
                before,
                open_end: true,
                keys: vec![elems],
                name: p.name.clone(),
                evidence: format!("defined in {from}: {xn}"),
            });
        }
    }
    let note = format!(
        "not carried: {} function(s) both define at another key{}{}, {reordered} both define in another order, \
         {gone} this link's objects define and those do not",
        rekeyed.len(),
        if rekeyed.is_empty() { "" } else { " (" },
        if rekeyed.is_empty() {
            String::new()
        } else {
            let mut s = rekeyed
                .iter()
                .take(6)
                .cloned()
                .collect::<Vec<_>>()
                .join("; ");
            if rekeyed.len() > 6 {
                s.push_str("; ...");
            }
            s.push(')');
            s
        }
    );
    (out, note)
}

pub fn anchor(
    arr: &Array,
    seq: &[usize],
    r: &Attested,
    keys: &Keys,
) -> Result<(usize, usize), String> {
    let key_of = |s: &str| keys.object_key(s).to_ascii_lowercase();
    let member = |s: &str| -> String {
        s.strip_suffix(')')
            .and_then(|x| x.rsplit_once('('))
            .map_or(s, |(_, m)| m)
            .to_ascii_lowercase()
    };
    let o = arr
        .objects
        .iter()
        .position(|x| x.eq_ignore_ascii_case(&r.obj))
        .or_else(|| {
            let k = key_of(&r.obj);
            arr.objects.iter().position(|x| key_of(x) == k)
        })
        .or_else(|| {
            let m = member(&r.obj);
            let hits: Vec<usize> = (0..arr.objects.len())
                .filter(|&i| arr.objects.get(i).is_some_and(|x| member(x) == m))
                .collect();
            match hits.as_slice() {
                [one] => Some(*one),
                _ => None,
            }
        })
        .ok_or_else(|| format!("{}: not in the link", r.obj))?;
    let of = |i: &usize| arr.cands.get(*i).map(|c| c.obj);
    let last = || {
        seq.iter()
            .rposition(|i| of(i).is_some_and(|x| x <= o))
            .map_or(0, |p| p + 1)
    };
    let at = r.before.iter().find_map(|b| {
        seq.iter().position(|i| {
            arr.cands
                .get(*i)
                .is_some_and(|c| c.obj == o && (&c.name == b || &c.label == b))
        })
    });
    match at {
        Some(p) => Ok((p, o)),
        None if r.before.is_empty() || r.open_end => Ok((last(), o)),
        None => Err(format!(
            "{}: no candidate of {} is {}",
            r.name,
            r.obj,
            r.before.join(" | ")
        )),
    }
}

#[must_use]
pub fn extend_array(
    arr: &Array,
    rows: &[Attested],
    objs: &[Option<usize>],
) -> (Array, Vec<Vec<Vec<usize>>>) {
    let mut a = arr.clone();
    let mut adds = Vec::with_capacity(rows.len());
    for (r, o) in rows.iter().zip(objs) {
        let mut per = Vec::new();
        if let Some(o) = *o {
            for alt in &r.keys {
                let mut idx = Vec::new();
                for (k, &(size, nreloc)) in alt.iter().enumerate() {
                    idx.push(a.cands.len());
                    a.cands.push(Cand {
                        name: if k == 0 {
                            r.name.clone()
                        } else {
                            format!("{}$x{k}", r.name)
                        },
                        obj: o,
                        external: k == 0,
                        dead: true,
                        dup: false,
                        size,
                        nreloc,
                        chr: 0x6050_1020,
                        sel: if k == 0 { 2 } else { 5 },
                        bytes: Vec::new(),
                        relocs: Vec::new(),
                        cksum: 0,
                        label: if k == 0 {
                            r.name.clone()
                        } else {
                            ".text$x".into()
                        },
                    });
                }
                per.push(idx);
            }
        }
        adds.push(per);
    }
    (a, adds)
}

#[must_use]
pub fn trial_seq(
    seq: &[usize],
    anchors: &[usize],
    adds: &[Vec<Vec<usize>>],
    choice: &[usize],
) -> Vec<usize> {
    let mut ins: Vec<(usize, usize, &[usize])> = Vec::new();
    for (r, &c) in choice.iter().enumerate() {
        let Some(k) = c.checked_sub(1) else { continue };
        if let (Some(&at), Some(e)) = (anchors.get(r), adds.get(r).and_then(|v| v.get(k))) {
            ins.push((at, r, e.as_slice()));
        }
    }
    ins.sort_by_key(|x| (x.0, x.1));
    let mut out = Vec::with_capacity(seq.len() + ins.len());
    let mut it = ins.iter().peekable();
    for (p, &c) in seq.iter().enumerate() {
        while let Some((_, _, e)) = it.next_if(|x| x.0 <= p) {
            out.extend_from_slice(e);
        }
        out.push(c);
    }
    for (_, _, e) in it {
        out.extend_from_slice(e);
    }
    out
}

pub type Rank = (usize, usize, std::cmp::Reverse<usize>);

pub type Judge<'a> = dyn Fn(&[usize]) -> Result<(usize, usize)> + Sync + 'a;

#[derive(Debug)]
pub struct SetSearch {
    pub mode: String,
    pub trials: usize,
    pub best: Vec<(Rank, Vec<usize>, &'static str)>,
}

pub fn search_sets(
    alts: &[usize],
    budget: usize,
    jobs: usize,
    judge: &Judge<'_>,
) -> Result<SetSearch> {
    let rank = |c: &[usize]| -> Result<Rank> {
        let (w, o) = judge(c)?;
        Ok((
            w,
            o,
            std::cmp::Reverse(c.iter().filter(|&&x| x > 0).count()),
        ))
    };
    let total = alts
        .iter()
        .try_fold(1usize, |acc, &k| acc.checked_mul(k + 1));
    if let Some(total) = total.filter(|&t| alts.len() <= JOINT_EXHAUSTIVE_ROWS && t <= budget) {
        let decode = |mut i: usize| -> Vec<usize> {
            alts.iter()
                .map(|&k| {
                    let d = i % (k + 1);
                    i /= k + 1;
                    d
                })
                .collect()
        };
        let jobs = jobs.clamp(1, total.max(1));
        let chunk = total.div_ceil(jobs);
        let mut best: Vec<(Rank, Vec<usize>, &'static str)> = Vec::new();
        let mut err: Option<anyhow::Error> = None;
        std::thread::scope(|s| {
            let hs: Vec<_> = (0..jobs)
                .map(|j| {
                    let (rank, decode) = (&rank, &decode);
                    s.spawn(move || -> Result<Vec<(Rank, Vec<usize>, &'static str)>> {
                        let mut top: Vec<(Rank, Vec<usize>, &'static str)> = Vec::new();
                        for i in (j * chunk)..((j + 1) * chunk).min(total) {
                            let c = decode(i);
                            top.push((rank(&c)?, c, "every set"));
                            if top.len() > 4 * JOINT_SHOW {
                                top.sort_by(|a, b| b.0.cmp(&a.0).then_with(|| a.1.cmp(&b.1)));
                                top.truncate(JOINT_SHOW);
                            }
                        }
                        Ok(top)
                    })
                })
                .collect();
            for h in hs {
                match h.join() {
                    Ok(Ok(v)) => best.extend(v),
                    Ok(Err(e)) => {
                        err.get_or_insert(e);
                    }
                    Err(_) => {
                        err.get_or_insert(anyhow!("a joint-search worker panicked"));
                    }
                }
            }
        });
        if let Some(e) = err {
            return Err(e);
        }
        best.sort_by(|a, b| b.0.cmp(&a.0).then_with(|| a.1.cmp(&b.1)));
        best.truncate(JOINT_SHOW);
        return Ok(SetSearch {
            mode: format!("EXHAUSTIVE: all {total} choice(s) of {} row(s)", alts.len()),
            trials: total,
            best,
        });
    }
    let mut trials = 0usize;
    let mut cur = vec![0usize; alts.len()];
    let mut cur_rank = rank(&cur)?;
    trials += 1;
    loop {
        let mut step: Option<(Rank, Vec<usize>)> = None;
        for (r, &k) in alts.iter().enumerate() {
            if cur.get(r) != Some(&0) {
                continue;
            }
            for a in 1..=k {
                let mut c = cur.clone();
                if let Some(x) = c.get_mut(r) {
                    *x = a;
                }
                let rk = rank(&c)?;
                trials += 1;
                if step.as_ref().is_none_or(|s| rk > s.0) {
                    step = Some((rk, c));
                }
            }
        }
        match step {
            Some((rk, c)) if rk > cur_rank => {
                cur = c;
                cur_rank = rk;
            }
            _ => break,
        }
    }
    let forward = (cur_rank, cur);
    let mut cur: Vec<usize> = alts.iter().map(|&k| usize::from(k > 0)).collect();
    let mut cur_rank = rank(&cur)?;
    trials += 1;
    loop {
        let mut step: Option<(Rank, Vec<usize>)> = None;
        for r in 0..alts.len() {
            if cur.get(r) == Some(&0) {
                continue;
            }
            let mut c = cur.clone();
            if let Some(x) = c.get_mut(r) {
                *x = 0;
            }
            let rk = rank(&c)?;
            trials += 1;
            if step.as_ref().is_none_or(|s| rk > s.0) {
                step = Some((rk, c));
            }
        }
        match step {
            Some((rk, c)) if rk > cur_rank => {
                cur = c;
                cur_rank = rk;
            }
            _ => break,
        }
    }
    let mut best = vec![
        (forward.0, forward.1, "greedy forward"),
        (cur_rank, cur, "backward elimination"),
    ];
    best.sort_by_key(|b| std::cmp::Reverse(b.0));
    Ok(SetSearch {
        mode: format!(
            "GREEDY: {} row(s) (exhaustive up to {JOINT_EXHAUSTIVE_ROWS} rows and {JOINT_BUDGET} choices)",
            alts.len()
        ),
        trials,
        best,
    })
}

#[derive(Clone, Debug)]
pub struct WantedMember {
    pub label: String,
    pub members: Vec<usize>,
    pub target: usize,
}

#[must_use]
pub fn wanted_members(
    arr: &Array,
    groups: &[Group],
    resolved: &[Option<Vec<usize>>],
    want: &[String],
) -> Vec<WantedMember> {
    want.iter()
        .filter_map(|w| {
            let (gi, members) = groups
                .iter()
                .zip(resolved)
                .enumerate()
                .find(|(_, (g, _))| g.members().any(|s| &s.name == w))
                .and_then(|(i, (_, r))| r.clone().map(|m| (i, m)))?;
            let target = *members.iter().find(|&&c| {
                arr.cands
                    .get(c)
                    .is_some_and(|x| &x.name == w || &x.label == w)
            })?;
            Some(WantedMember {
                label: format!("G{} {}", gi + 1, show(arr, target)),
                members,
                target,
            })
        })
        .collect()
}

#[derive(Debug)]
pub struct Joint<'a> {
    pub arr: &'a Array,
    pub seq: &'a [usize],
    pub model: Model,
    pub pools: &'a [Pool],
    pub owners: &'a RefOwners,
    pub keys: &'a Keys,
    pub wants: &'a [WantedMember],
    pub rows: &'a [Attested],
    pub sources: &'a str,
    pub other: Option<&'a Array>,
    pub jobs: usize,
}

fn key_text(r: &Attested) -> String {
    r.keys
        .iter()
        .map(|alt| {
            alt.iter()
                .map(|(s, n)| format!("{s}:{n}"))
                .collect::<Vec<_>>()
                .join("+")
        })
        .collect::<Vec<_>>()
        .join(",")
}

pub fn joint_search(j: &Joint<'_>) -> Result<Vec<String>> {
    let mut out = Vec::new();
    let (written, derived) = (
        j.rows.iter().filter(|r| r.source() == "written").count(),
        j.rows.iter().filter(|r| r.source() == "derived").count(),
    );
    out.push(String::new());
    out.push(format!(
        "LEADS ({written} row(s) WRITTEN in a --candidates file, attested by the evidence each names; {derived} \
         DERIVED by --candidates-from, functions another build compiled; check each against the siblings; never a \
         source edit)"
    ));
    let placed: Vec<Result<(usize, usize), String>> = j
        .rows
        .iter()
        .map(|r| anchor(j.arr, j.seq, r, j.keys))
        .collect();
    let held: HashSet<&str> = j.arr.cands.iter().map(|c| c.name.as_str()).collect();
    let mut keep: Vec<usize> = Vec::new();
    for (i, (r, p)) in j.rows.iter().zip(&placed).enumerate() {
        match p {
            Err(e) => out.push(format!("  NOT PLACED {e}")),
            Ok(_) if held.contains(r.name.as_str()) => {
                out.push(format!(
                    "  NOT PLACED {}: the link already holds that name",
                    r.name
                ));
            }
            Ok(_) => keep.push(i),
        }
    }
    let rows_k: Vec<Attested> = keep
        .iter()
        .filter_map(|&i| j.rows.get(i).cloned())
        .collect();
    let objs: Vec<Option<usize>> = keep
        .iter()
        .map(|&i| placed.get(i).and_then(|p| p.as_ref().ok()).map(|x| x.1))
        .collect();
    let anchors: Vec<usize> = keep
        .iter()
        .map(|&i| {
            placed
                .get(i)
                .and_then(|p| p.as_ref().ok())
                .map_or(0, |x| x.0)
        })
        .collect();
    let (ext, adds) = extend_array(j.arr, &rows_k, &objs);
    let alts: Vec<usize> = rows_k.iter().map(|r| r.keys.len()).collect();
    let outcome = |c: &[usize]| -> Result<(usize, usize, String)> {
        let s2 = trial_seq(j.seq, &anchors, &adds, c);
        let srt = sorted(&ext, &s2, j.model)?;
        let sc = score_sorted(&ext, &srt, j.pools, j.owners, j.keys)?;
        let w = j
            .wants
            .iter()
            .filter(|w| srt.iter().find(|c| w.members.contains(c)) == Some(&w.target))
            .count();
        Ok((w, tally(&sc).0, verdict_code(&sc)))
    };
    let judge = |c: &[usize]| -> Result<(usize, usize)> {
        let (w, o, _) = outcome(c)?;
        Ok((w, o))
    };
    let none = vec![0usize; rows_k.len()];
    let (bw, bo, bc) = outcome(&none)?;
    out.push(format!(
        "  {} row(s) placed of {} ({}); base: want {bw}/{} ok {bo} {bc}",
        rows_k.len(),
        j.rows.len(),
        j.sources,
        j.wants.len()
    ));
    for w in j.wants {
        out.push(format!(
            "  WANT {} ({} member(s))",
            w.label,
            w.members.len()
        ));
    }
    out.push("  ROWS (each alone, at its best key; each left out of the full set)".to_string());
    let full: Vec<usize> = alts.iter().map(|&k| usize::from(k > 0)).collect();
    let (fw, fo, fc) = outcome(&full)?;
    for (r, row) in rows_k.iter().enumerate() {
        let mut alone: Option<((usize, usize, String), usize)> = None;
        for a in 1..=row.keys.len() {
            let mut c = none.clone();
            if let Some(x) = c.get_mut(r) {
                *x = a;
            }
            let o = outcome(&c)?;
            let better = alone.as_ref().is_none_or(|(b, _)| (o.0, o.1) > (b.0, b.1));
            if better {
                alone = Some((o, a));
            }
        }
        let mut out_c = full.clone();
        if let Some(x) = out_c.get_mut(r) {
            *x = 0;
        }
        let (ow, oo, oc) = outcome(&out_c)?;
        let (aw, ao, ac) = alone.map_or((0, 0, String::new()), |x| x.0);
        out.push(format!(
            "    R{:<3} {} key {} in {} before {}: alone want {aw} ok {ao} {ac} | left out want {ow} ok {oo} {oc}",
            r + 1,
            row.name,
            key_text(row),
            row.obj,
            anchors
                .get(r)
                .and_then(|&a| j.seq.get(a))
                .and_then(|&c| ext.cands.get(c))
                .filter(|c| objs.get(r).copied().flatten() == Some(c.obj))
                .map_or("(last)", |c| c.name.as_str()),
        ));
    }
    out.push(format!(
        "  FULL SET ({} row(s)): want {fw}/{} ok {fo} {fc}",
        rows_k.len(),
        j.wants.len()
    ));
    if let Some(oa) = j.other {
        let s: Vec<usize> = (0..oa.cands.len()).collect();
        let srt = sorted(oa, &s, j.model)?;
        let sc = score_sorted(oa, &srt, j.pools, j.owners, j.keys)?;
        let id = |a: &Array, c: usize| {
            a.cands.get(c).map(|x| {
                (
                    x.label.clone(),
                    a.objects.get(x.obj).cloned().unwrap_or_default(),
                )
            })
        };
        let w = j
            .wants
            .iter()
            .filter(|w| {
                let target = id(j.arr, w.target);
                let ms: Vec<Option<(String, String)>> =
                    w.members.iter().map(|&m| id(j.arr, m)).collect();
                let first = srt.iter().copied().find(|&c| ms.contains(&id(oa, c)));
                first.and_then(|c| id(oa, c)) == target
            })
            .count();
        out.push(format!(
            "  THE OTHER BUILD ITSELF ({} candidate(s), {} dead; this link's pools): want {w}/{} ok {} {}",
            oa.cands.len(),
            oa.cands.iter().filter(|c| c.dead).count(),
            j.wants.len(),
            tally(&sc).0,
            verdict_code(&sc)
        ));
    }
    let found = search_sets(&alts, JOINT_BUDGET, j.jobs, &judge)?;
    out.push(format!(
        "  SEARCH {} -- {} replay(s)",
        found.mode, found.trials
    ));
    for (i, ((w, o, n), c, how)) in found.best.iter().enumerate() {
        let (_, _, code) = outcome(c)?;
        let set: Vec<usize> = (0..c.len())
            .filter(|&r| c.get(r).is_some_and(|&x| x > 0))
            .collect();
        out.push(format!(
            "  SET #{} ({how}) want {w}/{} ok {o} rows {}: {code}",
            i + 1,
            j.wants.len(),
            n.0
        ));
        for r in set {
            let (Some(row), Some(&a)) = (rows_k.get(r), c.get(r)) else {
                continue;
            };
            let k = row.keys.get(a - 1).map_or_else(String::new, |alt| {
                alt.iter()
                    .map(|(s, n)| format!("{s}:{n}"))
                    .collect::<Vec<_>>()
                    .join("+")
            });
            out.push(format!(
                "      R{:<3} {} {k} in {}  [{}: {}]",
                r + 1,
                row.name,
                row.obj,
                row.source(),
                row.evidence
            ));
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::comdat::ObjComdat;
    use crate::icf::first_of;

    fn cand(name: &str, obj: usize, size: u32) -> Cand {
        Cand {
            name: name.into(),
            obj,
            external: true,
            dead: false,
            dup: false,
            size,
            nreloc: 0,
            chr: 0x6050_1020,
            sel: 2,
            bytes: vec![0xc3],
            relocs: Vec::new(),
            cksum: 0,
            label: name.into(),
        }
    }

    #[test]
    fn a_candidates_row_parses_keys_funclets_and_an_empty_before() -> Result<()> {
        let r = Attested::parse_row(
            "engine.lib(Table.obj)\t\t112:3+20:1,128:3\t?Hide@Table@@UAEXXZ\tsibling 0x5e98a0",
        )?;
        assert_eq!(
            r,
            Some(Attested {
                obj: "engine.lib(Table.obj)".into(),
                before: Vec::new(),
                open_end: false,
                keys: vec![vec![(112, 3), (20, 1)], vec![(128, 3)]],
                name: "?Hide@Table@@UAEXXZ".into(),
                evidence: "sibling 0x5e98a0".into(),
            })
        );
        let b = Attested::parse_row(
            "Plot.obj\t?Fill@@YAXXZ | ?Release@@YAXXZ\t48:0\t?Sqrt@@YAHH@Z\tsibling 0x423cb0",
        )?;
        assert_eq!(
            b.as_ref().map(|x| x.before.clone()),
            Some(vec![
                "?Fill@@YAXXZ".to_string(),
                "?Release@@YAXXZ".to_string()
            ])
        );
        assert!(Attested::parse_row("Plot.obj\t?Fill@@YAXXZ\t48:0\t?Sqrt@@YAHH@Z").is_err());
        assert!(Attested::parse_row("Plot.obj\t\t48:0\t?Sqrt@@YAHH@Z\t  ").is_err());
        assert_eq!(Attested::parse_row("# a comment")?, None);
        assert_eq!(Attested::parse_row("   ")?, None);
        assert!(Attested::parse_row("Plot.obj\t\t48\t?f@@YAXXZ").is_err());
        Ok(())
    }

    #[test]
    fn a_joint_insertion_lands_before_its_anchor_and_is_dead() {
        let arr = Array {
            objects: vec!["A.obj".into(), "engine.lib(B.obj)".into()],
            cands: vec![cand("a0", 0, 16), cand("a1", 0, 16), cand("b0", 1, 32)],
            ..Array::default()
        };
        let keys = Keys::new(["engine"]);
        let seq = vec![0, 1, 2];
        let rows = vec![
            Attested {
                obj: "A.obj".into(),
                before: vec!["dup".into(), "a1".into()],
                open_end: false,
                keys: vec![vec![(48, 0)]],
                name: "r1".into(),
                evidence: "e1".into(),
            },
            Attested {
                obj: "B.obj".into(),
                before: Vec::new(),
                open_end: false,
                keys: vec![vec![(64, 2), (12, 1)]],
                name: "r2".into(),
                evidence: "e2".into(),
            },
        ];
        let placed: Vec<(usize, usize)> = rows
            .iter()
            .filter_map(|r| anchor(&arr, &seq, r, &keys).ok())
            .collect();
        assert_eq!(placed, vec![(1, 0), (3, 1)]);
        let (ext, adds) = extend_array(&arr, &rows, &[Some(0), Some(1)]);
        let at: Vec<usize> = placed.iter().map(|p| p.0).collect();
        let s = trial_seq(&seq, &at, &adds, &[1, 1]);
        let names: Vec<&str> = s
            .iter()
            .filter_map(|&i| ext.cands.get(i).map(|c| c.name.as_str()))
            .collect();
        assert_eq!(names, vec!["a0", "r1", "a1", "b0", "r2", "r2$x1"]);
        assert_eq!(trial_seq(&seq, &at, &adds, &[0, 0]), seq);
        let mut bad = rows[0].clone();
        bad.before = vec!["zz".into()];
        assert!(anchor(&arr, &seq, &bad, &keys).is_err());
        bad.open_end = true;
        assert_eq!(anchor(&arr, &seq, &bad, &keys), Ok((2, 0)));
    }

    #[test]
    fn the_joint_search_finds_a_pair_no_single_insertion_finds() -> Result<()> {
        let mut arr = Array {
            objects: vec!["A.obj".into()],
            ..Array::default()
        };
        for (i, k) in [2u32, 2, 1, 1, 1, 1, 1, 1, 1].iter().enumerate() {
            arr.cands.push(cand(&format!("c{i}"), 0, k * 16));
        }
        let seq: Vec<usize> = (0..arr.cands.len()).collect();
        let (t, u) = (1usize, 0usize);
        let members = vec![t, u];
        assert_eq!(first_of(&arr, &seq, Model::default(), &members)?, Some(u));
        let row = |name: &str, size: u32| Attested {
            obj: "A.obj".into(),
            before: Vec::new(),
            open_end: false,
            keys: vec![vec![(size, 0)]],
            name: name.into(),
            evidence: String::new(),
        };
        let rows = vec![row("r1", 16), row("r2", 32)];
        let (ext, adds) = extend_array(&arr, &rows, &[Some(0), Some(0)]);
        let at = [1usize, 5];
        let judge = |c: &[usize]| -> Result<(usize, usize)> {
            let s = trial_seq(&seq, &at, &adds, c);
            Ok((
                usize::from(first_of(&ext, &s, Model::default(), &members)? == Some(t)),
                0,
            ))
        };
        assert_eq!(judge(&[1, 0])?.0, 0);
        assert_eq!(judge(&[0, 1])?.0, 0);
        let found = search_sets(&[1, 1], JOINT_BUDGET, 2, &judge)?;
        assert_eq!(found.trials, 4);
        assert_eq!(
            found.best.first().map(|b| (b.0.0, b.1.clone())),
            Some((1, vec![1, 1]))
        );
        let greedy = search_sets(&[1, 1], 0, 1, &judge)?;
        assert_eq!(
            greedy.best.first().map(|b| (b.0.0, b.1.clone())),
            Some((1, vec![1, 1]))
        );
        Ok(())
    }

    fn comdat(name: &str, sec: usize) -> ObjComdat {
        ObjComdat {
            name: name.into(),
            external: true,
            size: 16,
            nreloc: 0,
            chr: 0x6050_1020,
            sel: 2,
            bytes: vec![0xc2, 8, 0],
            relocs: Vec::new(),
            sec,
            sec_name: ".text".into(),
            parent: None,
            sym: sec,
            cksum: 0,
        }
    }

    fn unit(obj: &str, names: &[&str]) -> Unit {
        Unit::Obj(
            obj.into(),
            crate::comdat::ObjInfo {
                comdats: names
                    .iter()
                    .enumerate()
                    .map(|(i, n)| comdat(n, i + 1))
                    .collect(),
                ..crate::comdat::ObjInfo::default()
            },
        )
    }

    #[test]
    fn candidates_from_another_build_are_the_functions_its_objects_add() {
        let mut other_b = unit("engine.lib(B.obj)", &["z", "w", "n2"]);
        if let Unit::Obj(_, info) = &mut other_b {
            let mut f = comdat("$x", 9);
            f.external = false;
            f.sel = 5;
            f.size = 12;
            f.nreloc = 1;
            f.parent = Some(2);
            info.comdats.push(f);
        }
        let ours = Parsed {
            units: vec![
                unit("A.obj", &["x", "y"]),
                unit("engine.lib(B.obj)", &["z"]),
            ],
            refused: Vec::new(),
        };
        let mut other_a = unit("A.obj", &["x", "n1", "y", "z", "n2"]);
        if let Unit::Obj(_, info) = &mut other_a
            && let Some(y) = info.comdats.iter_mut().find(|c| c.name == "y")
        {
            y.size = 32;
        }
        let other = Parsed {
            units: vec![other_a, other_b],
            refused: Vec::new(),
        };
        let (rows, note) = derive_candidates(&ours, &other, "R3");
        assert!(
            note.starts_with(
                "not carried: 1 function(s) both define at another key (y 16:0 -> 32:0 A.obj)"
            ),
            "{note}"
        );
        assert!(
            note.contains(", 0 both define in another order, 0 this link's"),
            "{note}"
        );
        let got: Vec<String> = rows
            .iter()
            .map(|r| format!("{} {} {:?} {:?}", r.obj, r.name, r.before, r.keys))
            .collect();
        assert_eq!(
            got,
            vec![
                r#"A.obj n1 ["y"] [[(16, 0)]]"#,
                "A.obj n2 [] [[(16, 0)]]",
                "engine.lib(B.obj) w [] [[(16, 0), (12, 1)]]",
            ]
        );
        assert!(
            rows.iter()
                .all(|r| r.source() == "derived" && r.evidence.starts_with("defined in R3: "))
        );
    }
}
