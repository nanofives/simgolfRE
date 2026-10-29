use anyhow::{Result, anyhow, bail};
use std::collections::{BTreeMap, HashMap};

use crate::icf::{Array, Cand, Model, fold, key, show, sorted};
use crate::objkey::Keys;
use crate::owners::{Owner, RefOwners};
use crate::verbose::Group;

pub const INSERT_TRIALS_MAX: usize = 1 << 20;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Pool {
    pub group: usize,
    pub rva: u32,
    pub names: Vec<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Verdict {
    Ok,
    Edge,
    Miss,
    Unknown,
}

impl Verdict {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::Ok => "ok",
            Self::Edge => "EDGE",
            Self::Miss => "MISS",
            Self::Unknown => "?",
        }
    }

    #[must_use]
    pub fn code(self) -> char {
        match self {
            Self::Ok => 'o',
            Self::Edge => 'e',
            Self::Miss => 'M',
            Self::Unknown => '?',
        }
    }
}

pub type Scored = (Option<usize>, Verdict);

#[must_use]
pub fn ref_pools(groups: &[Group], owners: &RefOwners) -> Vec<Pool> {
    let mut out = Vec::new();
    for (gi, g) in groups.iter().enumerate() {
        let mut by: BTreeMap<u32, Vec<String>> = BTreeMap::new();
        for m in g.members() {
            if let Some(&r) = owners.refof.get(&m.name) {
                by.entry(r).or_default().push(m.name.clone());
            }
        }
        out.extend(by.into_iter().map(|(rva, names)| Pool {
            group: gi,
            rva,
            names,
        }));
    }
    out
}

pub fn score(
    arr: &Array,
    seq: &[usize],
    model: Model,
    pools: &[Pool],
    owners: &RefOwners,
    keys: &Keys,
) -> Result<Vec<Scored>> {
    score_sorted(arr, &sorted(arr, seq, model)?, pools, owners, keys)
}

pub fn score_sorted(
    arr: &Array,
    srt: &[usize],
    pools: &[Pool],
    owners: &RefOwners,
    keys: &Keys,
) -> Result<Vec<Scored>> {
    let root = fold(arr, srt)?;
    let pos: HashMap<usize, usize> = srt.iter().enumerate().map(|(p, &c)| (c, p)).collect();
    let by_name: HashMap<&str, usize> = arr
        .cands
        .iter()
        .enumerate()
        .filter(|(_, c)| c.external && !c.dup)
        .map(|(i, c)| (c.name.as_str(), i))
        .collect();
    let cand_of = |n: &str| -> Option<usize> {
        by_name.get(n).copied().or_else(|| {
            arr.aliases
                .get(n)
                .and_then(|d| by_name.get(d.as_str()).copied())
        })
    };
    Ok(pools
        .iter()
        .map(|p| {
            let mut by_root: HashMap<usize, usize> = HashMap::new();
            for c in p
                .names
                .iter()
                .filter_map(|n| cand_of(n))
                .filter(|&c| arr.cands.get(c).is_some_and(|x| !x.dead))
            {
                *by_root
                    .entry(root.get(c).copied().unwrap_or(c))
                    .or_default() += 1;
            }
            let surv = by_root
                .iter()
                .max_by_key(|&(r, n)| {
                    (
                        *n,
                        std::cmp::Reverse(pos.get(r).copied().unwrap_or(usize::MAX)),
                    )
                })
                .map(|(&r, _)| r);
            let obj = surv
                .and_then(|c| arr.cands.get(c))
                .and_then(|c| arr.objects.get(c.obj))
                .map(|o| keys.object_key(o));
            let v = match (obj, owners.owner(p.rva)) {
                (None, _) | (_, Owner::Unknown) => Verdict::Unknown,
                (Some(o), Owner::Known(set)) => {
                    if set.iter().any(|s| s.eq_ignore_ascii_case(o)) {
                        Verdict::Ok
                    } else {
                        Verdict::Miss
                    }
                }
                (Some(o), Owner::Between(b, a)) => {
                    if b.eq_ignore_ascii_case(o) || a.eq_ignore_ascii_case(o) {
                        Verdict::Edge
                    } else {
                        Verdict::Miss
                    }
                }
            };
            (surv, v)
        })
        .collect())
}

#[must_use]
pub fn tally(s: &[Scored]) -> (usize, usize, usize, usize) {
    let n = |v: Verdict| s.iter().filter(|(_, x)| *x == v).count();
    (
        n(Verdict::Ok),
        n(Verdict::Edge),
        n(Verdict::Miss),
        n(Verdict::Unknown),
    )
}

#[must_use]
pub fn verdict_code(s: &[Scored]) -> String {
    s.iter().map(|(_, v)| v.code()).collect()
}

#[must_use]
pub fn score_lines(
    pools: &[Pool],
    owners: &RefOwners,
    now: (&Array, &[Scored]),
    base: Option<(&Array, &[Scored])>,
    verbose: bool,
) -> Vec<String> {
    let mut out = Vec::new();
    let (arr, sc) = now;
    let obj_of = |a: &Array, c: Option<usize>| -> String {
        c.and_then(|c| a.cands.get(c))
            .and_then(|x| a.objects.get(x.obj))
            .cloned()
            .unwrap_or_else(|| "-".into())
    };
    out.push(String::new());
    out.push(
        "SCORE each reference fold target's survivor against the object the reference holds there"
            .to_string(),
    );
    for (i, p) in pools.iter().enumerate() {
        let Some(&(c, v)) = sc.get(i) else { continue };
        let was = base.and_then(|(a, b)| b.get(i).map(|&(c0, v0)| (a, c0, v0)));
        let changed = was.is_some_and(|(a, c0, v0)| v0 != v || obj_of(a, c0) != obj_of(arr, c));
        if !(verbose || changed || base.is_none()) {
            continue;
        }
        let from = was.map_or_else(String::new, |(a, c0, v0)| {
            format!("{} {} -> ", obj_of(a, c0), v0.tag())
        });
        out.push(format!(
            "  G{:<3} {:#07x} {:>3} name(s)  ref {:<34} {from}{} {}",
            p.group + 1,
            p.rva,
            p.names.len(),
            owners.owner_at(p.rva),
            obj_of(arr, c),
            v.tag()
        ));
    }
    let (ok, edge, miss, unk) = tally(sc);
    let mut last = format!(
        "SCORE {ok} of {} pool(s) hold the reference's object ({edge} more at an object edge), {miss} MISS, {unk} unknown",
        ok + edge + miss
    );
    if let Some((_, b)) = base {
        let (ok0, edge0, miss0, _) = tally(b);
        last.push_str(&format!("; base {ok0} ok, {edge0} edge, {miss0} MISS"));
    }
    out.push(last);
    out
}

#[must_use]
pub fn focus_pool(pools: &[Pool], rva: Option<u32>) -> Option<usize> {
    match rva {
        Some(r) => pools.iter().position(|p| p.rva == r),
        None => pools
            .iter()
            .enumerate()
            .max_by_key(|(i, p)| (p.names.len(), std::cmp::Reverse(*i)))
            .map(|(i, _)| i),
    }
}

type Trial = (bool, usize, usize, (u32, u16), String);

#[allow(clippy::too_many_arguments)]
pub fn search_inserts(
    arr: &Array,
    seq: &[usize],
    model: Model,
    pools: &[Pool],
    owners: &RefOwners,
    keys_text: &str,
    focus: Option<usize>,
    obj_keys: &Keys,
) -> Result<Vec<String>> {
    let mut out = Vec::new();
    let mut keyv: Vec<(u32, u16)> = Vec::new();
    for k in keys_text
        .split(',')
        .map(str::trim)
        .filter(|k| !k.is_empty())
    {
        let (s, r) = k
            .split_once(':')
            .ok_or_else(|| anyhow!("--search-insert: want SIZE:NRELOC, got '{k}'"))?;
        keyv.push((s.parse()?, r.parse()?));
    }
    let trials = keyv.len().saturating_mul(seq.len().saturating_add(1));
    if trials > INSERT_TRIALS_MAX {
        bail!(
            "--search-insert: {trials} insertions, more than the {INSERT_TRIALS_MAX} this search replays"
        );
    }
    let label = focus
        .and_then(|i| pools.get(i))
        .map_or_else(|| "G?".to_string(), |p| format!("G{}", p.group + 1));
    let blank = " ".repeat(label.len());
    let mark = |ok: bool| if ok { label.as_str() } else { blank.as_str() };
    let base = score(arr, seq, model, pools, owners, obj_keys)?;
    let (bo, _, _, _) = tally(&base);
    out.push(String::new());
    out.push(format!(
        "SEARCH-INSERT base {} ({bo} ok); pools {}",
        verdict_code(&base),
        pools
            .iter()
            .map(|p| format!("G{}@{:#x}", p.group + 1, p.rva))
            .collect::<Vec<_>>()
            .join(" ")
    ));
    let focus_ok = |sc: &[Scored]| {
        focus
            .and_then(|i| sc.get(i))
            .is_some_and(|(_, v)| *v == crate::pools::Verdict::Ok)
    };
    let mut a2 = arr.clone();
    let ph = a2.cands.len();
    let mut rows: Vec<Trial> = Vec::new();
    for &(size, nreloc) in &keyv {
        for at in 0..=seq.len() {
            let owner_obj = seq
                .get(at.saturating_sub(1))
                .and_then(|&c| arr.cands.get(c))
                .map_or(0, |c| c.obj);
            let phantom = Cand {
                name: "<dead>".into(),
                obj: owner_obj,
                external: false,
                dead: true,
                dup: false,
                size,
                nreloc,
                chr: 0,
                sel: 0,
                bytes: Vec::new(),
                relocs: Vec::new(),
                cksum: 0,
                label: "<dead>".into(),
            };
            if a2.cands.len() == ph {
                a2.cands.push(phantom);
            } else if let Some(slot) = a2.cands.get_mut(ph) {
                *slot = phantom;
            }
            let mut s2 = seq.to_vec();
            s2.insert(at, ph);
            let sc = score(&a2, &s2, model, pools, owners, obj_keys)?;
            let (o, _, _, _) = tally(&sc);
            rows.push((focus_ok(&sc), o, at, (size, nreloc), verdict_code(&sc)));
        }
    }
    rows.sort_by(|x, y| (y.0, y.1).cmp(&(x.0, x.1)).then(x.2.cmp(&y.2)));
    let mut hist: BTreeMap<String, usize> = BTreeMap::new();
    for r in &rows {
        *hist.entry(r.4.clone()).or_default() += 1;
    }
    let mut hv: Vec<(&String, &usize)> = hist.iter().collect();
    hv.sort_by(|a, b| b.1.cmp(a.1));
    for (c, n) in hv.iter().take(12) {
        out.push(format!("  {n:>5} x {c}"));
    }
    let n_focus = rows.iter().filter(|r| r.0).count();
    out.push(format!(
        "  {} insertion(s) tried, {n_focus} give {label} the reference's object",
        rows.len()
    ));
    for (ok, o, at, (s, r), c) in rows.iter().take(25) {
        let after = seq
            .get(at.saturating_sub(1))
            .map_or_else(|| "(start)".into(), |&c| show(arr, c));
        out.push(format!(
            "  {} ok {o:<3} key {s}:{r:<2} at {at:<5} after {after}  {c}",
            mark(*ok)
        ));
    }
    let mut removed: Vec<(bool, usize, usize, String)> = Vec::new();
    for (i, &c) in seq.iter().enumerate() {
        if !arr.cands.get(c).is_some_and(|x| x.dead) {
            continue;
        }
        let s2: Vec<usize> = seq.iter().copied().filter(|&d| d != c).collect();
        let sc = score(arr, &s2, model, pools, owners, obj_keys)?;
        let (o, _, _, _) = tally(&sc);
        removed.push((focus_ok(&sc), o, i, verdict_code(&sc)));
    }
    removed.sort_by(|x, y| (y.0, y.1).cmp(&(x.0, x.1)).then(x.2.cmp(&y.2)));
    out.push(format!(
        "  {} dead candidate(s) left out in turn, {} give {label} the reference's object",
        removed.len(),
        removed.iter().filter(|r| r.0).count()
    ));
    for (ok, o, i, c) in removed.iter().take(12) {
        let name = seq.get(*i).map_or_else(|| "?".into(), |&c| show(arr, c));
        out.push(format!(
            "  {} ok {o:<3} remove append {i:<5} {name}  {c}",
            mark(*ok)
        ));
    }
    let own = |c: &Cand| {
        arr.objects
            .get(c.obj)
            .is_some_and(|o| obj_keys.is_own_or_object(o))
    };
    let dead_own = seq
        .iter()
        .filter(|&&c| arr.cands.get(c).is_some_and(|x| x.dead && own(x)))
        .count();
    if dead_own.saturating_mul(32 * 13) > INSERT_TRIALS_MAX {
        bail!(
            "--search-insert: {dead_own} dead own candidate(s) to re-key, {} replays, more than the {INSERT_TRIALS_MAX} this search runs",
            dead_own.saturating_mul(32 * 13)
        );
    }
    let mut rekeyed: Vec<Trial> = Vec::new();
    let mut a3 = arr.clone();
    for &c in seq {
        if !arr.cands.get(c).is_some_and(|x| x.dead && own(x)) {
            continue;
        }
        let orig = arr.cands.get(c).map(key).unwrap_or_default();
        for size in (1..=32u32).map(|k| k * 16) {
            for nreloc in 0..=12u16 {
                if (size, nreloc) == orig {
                    continue;
                }
                if let Some(x) = a3.cands.get_mut(c) {
                    x.size = size;
                    x.nreloc = nreloc;
                }
                let sc = score(&a3, seq, model, pools, owners, obj_keys)?;
                let (o, _, _, _) = tally(&sc);
                let fok = focus_ok(&sc);
                if fok || o > bo {
                    rekeyed.push((fok, o, c, (size, nreloc), verdict_code(&sc)));
                }
            }
        }
        if let (Some(x), Some(y)) = (a3.cands.get_mut(c), arr.cands.get(c)) {
            x.size = y.size;
            x.nreloc = y.nreloc;
        }
    }
    rekeyed.sort_by(|x, y| (y.0, y.1).cmp(&(x.0, x.1)).then(x.2.cmp(&y.2)));
    out.push(format!(
        "  {} re-keyings of a dead own candidate beat the base or give {label} the reference's object ({} give {label})",
        rekeyed.len(),
        rekeyed.iter().filter(|r| r.0).count()
    ));
    for (ok, o, c, (s, r), code) in rekeyed.iter().take(20) {
        let k0 = arr.cands.get(*c).map(key).unwrap_or_default();
        out.push(format!(
            "  {} ok {o:<3} rekey {}:{} -> {s}:{r}  {}  {code}",
            mark(*ok),
            k0.0,
            k0.1,
            show(arr, *c)
        ));
    }
    Ok(out)
}
