use crate::bss::{
    Pair, Pick, RENAME_COST, Slot, Verdict, assign, classify, longest_ordered, plan, room,
};
use crate::bsssuggest::{self, Vocabulary};
use anyhow::{Result, anyhow};
use std::collections::{BTreeMap, HashSet};

#[derive(Debug, Clone)]
pub struct PlanOpts {
    pub k: u32,
    pub pins: Vec<String>,
    pub suggest: usize,
    pub assign: Option<String>,
    pub synonyms: usize,
    pub rename_cost: u64,
    pub vocabulary: Vocabulary,
}

impl Default for PlanOpts {
    fn default() -> Self {
        Self {
            k: 1,
            pins: Vec::new(),
            suggest: 0,
            assign: None,
            synonyms: 0,
            rename_cost: RENAME_COST,
            vocabulary: Vocabulary::default(),
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
enum AssignToken<'a> {
    Exclude(&'a str),
    Pattern(u32, &'a str),
}

fn assign_token(t: &str) -> AssignToken<'_> {
    if let Some(name) = t.strip_prefix('-') {
        return AssignToken::Exclude(name);
    }
    if let Some((n, rest)) = t.split_once(':')
        && !n.is_empty()
        && n.chars().all(|c| c.is_ascii_digit())
        && let Ok(k) = n.parse::<u32>()
    {
        return AssignToken::Pattern(k, rest);
    }
    AssignToken::Pattern(0, t)
}

#[derive(Debug, Default, Clone)]
struct Offer {
    cands: Vec<(u32, u32, String)>,
    refused: bool,
    excluded: Vec<String>,
}

fn sym(v: &Verdict, i: usize) -> &str {
    v.vars.get(i).map_or("", |x| x.symbol.as_str())
}

fn candidates_of(
    text: &str,
    walked: &[usize],
    v: &Verdict,
    synonyms: usize,
    voc: &Vocabulary,
) -> Result<(Vec<Offer>, HashSet<String>)> {
    let mut by_ident: BTreeMap<String, Vec<String>> = BTreeMap::new();
    let mut taken: HashSet<String> = HashSet::new();
    for line in text.lines() {
        let line = line.split('#').next().unwrap_or("");
        let mut w = line.split_whitespace();
        match w.next() {
            Some("-") => taken.extend(w.map(str::to_string)),
            Some(id) if !id.starts_with('=') => by_ident
                .entry(id.to_string())
                .or_default()
                .extend(w.map(str::to_string)),
            _ => {}
        }
    }
    let idents: Vec<String> = walked.iter().map(|&i| classify(sym(v, i)).0).collect();
    let stray: Vec<&String> = by_ident.keys().filter(|k| !idents.contains(k)).collect();
    if !stray.is_empty() {
        return Err(anyhow!(
            "--assign names no walked variable of {}: {}",
            v.stem,
            stray
                .iter()
                .map(|s| s.as_str())
                .collect::<Vec<_>>()
                .join(" ")
        ));
    }
    let mut offers = Vec::with_capacity(idents.len());
    for id in &idents {
        let mut o = Offer::default();
        for p in by_ident.get(id).into_iter().flatten() {
            match assign_token(p) {
                AssignToken::Exclude(name) => o.excluded.push(name.to_string()),
                AssignToken::Pattern(tier, pat) => {
                    for (cost, b, s) in bsssuggest::expand(pat, 0, 0x3ff)? {
                        let cost = u32::try_from(cost).unwrap_or(u32::MAX);
                        o.cands.push((b, cost.saturating_add(tier), s));
                    }
                }
            }
        }
        if synonyms > 0 {
            for (cost, b, s) in bsssuggest::suggest(id, 0, 0x3ff, voc)? {
                if cost <= synonyms {
                    o.cands
                        .push((b, 10 + u32::try_from(cost).unwrap_or(u32::MAX), s));
                }
            }
        }
        o.cands
            .retain(|x| x.2 != *id && !o.excluded.contains(&x.2) && !taken.contains(&x.2));
        o.cands.sort();
        o.cands.dedup_by(|a, b| a.2 == b.2);
        o.refused = o.excluded.contains(id);
        offers.push(o);
    }
    Ok((offers, taken))
}

#[derive(Debug, PartialEq, Eq)]
struct FamilyLine {
    label: String,
    tier: u32,
    members: Vec<(String, String)>,
}

fn family_line(line: &str) -> Result<Option<FamilyLine>> {
    let line = line.split('#').next().unwrap_or("");
    let mut w = line.split_whitespace();
    let Some(label) = w.next().and_then(|t| t.strip_prefix('=')) else {
        return Ok(None);
    };
    let tier = w
        .next()
        .and_then(|t| t.parse::<u32>().ok())
        .ok_or_else(|| anyhow!("family `={label}`: a TIER (a number) must follow the label"))?;
    let members = w
        .map(|m| {
            m.split_once('=')
                .map(|(id, p)| (id.to_string(), p.to_string()))
                .ok_or_else(|| anyhow!("family `={label}`: `{m}` is not IDENT=PATTERN"))
        })
        .collect::<Result<Vec<_>>>()?;
    if members.is_empty() {
        return Err(anyhow!("family `={label}` names no member"));
    }
    Ok(Some(FamilyLine {
        label: label.to_string(),
        tier,
        members,
    }))
}

fn shared_groups(p: &str) -> (Vec<&str>, Vec<Vec<&str>>) {
    let (mut lits, mut groups) = (Vec::new(), Vec::new());
    let mut rest = p;
    while let Some(open) = rest.find('{') {
        let Some(close) = rest.get(open..).and_then(|r| r.find('}')) else {
            break;
        };
        lits.push(rest.get(..open).unwrap_or_default());
        groups.push(
            rest.get(open + 1..open + close)
                .unwrap_or_default()
                .split(',')
                .collect(),
        );
        rest = rest.get(open + close + 1..).unwrap_or_default();
    }
    lits.push(rest);
    (lits, groups)
}

type Tuple = (u32, Vec<(String, String, u32)>);

type Fitting = (Vec<usize>, Tuple);

type Saved = (Vec<(u32, u32)>, u32, bool);

const FAMILY_MAX: usize = 4_000_000;

fn family_tuples(lines: &[&FamilyLine], cap: usize) -> Result<Vec<Tuple>> {
    let mut out: Vec<Tuple> = Vec::new();
    for l in lines {
        let parsed: Vec<(Vec<&str>, Vec<Vec<&str>>)> =
            l.members.iter().map(|(_, p)| shared_groups(p)).collect();
        let lens: Vec<usize> = parsed
            .first()
            .map(|p| p.1.iter().map(Vec::len).collect())
            .unwrap_or_default();
        if parsed
            .iter()
            .any(|(_, g)| g.iter().map(Vec::len).ne(lens.iter().copied()))
        {
            return Err(anyhow!(
                "family `={}`: every member needs the same `{{}}` groups, of the same sizes",
                l.label
            ));
        }
        let mut at = vec![0usize; lens.len()];
        loop {
            let shared = at.iter().filter(|&&i| i != 0).count();
            let per: Vec<Vec<(usize, u32, String)>> = parsed
                .iter()
                .map(|(lits, groups)| {
                    let mut s = String::new();
                    for (k, lit) in lits.iter().enumerate() {
                        s.push_str(lit);
                        if let Some(g) = groups.get(k)
                            && let Some(x) = at.get(k).and_then(|&a| g.get(a))
                        {
                            s.push_str(x);
                        }
                    }
                    bsssuggest::expand(&s, 0, 0x3ff)
                })
                .collect::<Result<_>>()?;
            let plens: Vec<usize> = per.iter().map(Vec::len).collect();
            let count = plens
                .iter()
                .try_fold(1usize, |a, &n| a.checked_mul(n))
                .unwrap_or(usize::MAX);
            if out.len().saturating_add(count) > FAMILY_MAX {
                return Err(anyhow!(
                    "family `={}` offers over {FAMILY_MAX} tuples: share more of its slots",
                    l.label
                ));
            }
            let mut pick = vec![0usize; per.len()];
            while plens.iter().all(|&n| n > 0) {
                let names: Vec<(String, String, u32)> = l
                    .members
                    .iter()
                    .zip(&per)
                    .zip(&pick)
                    .filter_map(|(((id, _), e), &i)| {
                        e.get(i).map(|x| (id.clone(), x.2.clone(), x.1))
                    })
                    .collect();
                let own: usize = per
                    .iter()
                    .zip(&pick)
                    .filter_map(|(e, &i)| e.get(i).map(|x| x.0))
                    .sum();
                if names.iter().enumerate().all(|(a, x)| {
                    names
                        .get(..a)
                        .unwrap_or_default()
                        .iter()
                        .all(|y| y.1 != x.1)
                }) {
                    let cost = l.tier as usize + shared + own;
                    out.push((u32::try_from(cost).unwrap_or(u32::MAX), names));
                }
                if !bsssuggest::advance(&mut pick, &plens) {
                    break;
                }
            }
            if !bsssuggest::advance(&mut at, &lens) {
                break;
            }
        }
    }
    out.sort();
    let mut seen = HashSet::new();
    out.retain(|t| {
        let mut key: Vec<String> = t.1.iter().map(|x| format!("{}={}", x.0, x.1)).collect();
        key.sort_unstable();
        seen.insert(key.join(" "))
    });
    out.truncate(cap);
    Ok(out)
}

const FAMILY_TRIALS: usize = 1_000_000;

const FAMILY_SCORED: usize = 400;

fn room_total(r: &[Option<(u32, u32)>]) -> u64 {
    r.iter()
        .flatten()
        .map(|(lo, hi)| u64::from(hi - lo + 1))
        .sum()
}

struct Fixing<'a> {
    cur: &'a [u32],
    bc: Vec<Vec<(u32, u32)>>,
    offered: &'a mut [u32],
    pinned: &'a mut [bool],
    rename: u64,
    pairs: Vec<Pair>,
    floor: usize,
    base: Vec<Option<u32>>,
    room: Vec<Option<(u32, u32)>>,
}

impl<'a> Fixing<'a> {
    fn new(
        cur: &'a [u32],
        offers: &[Offer],
        offered: &'a mut [u32],
        pinned: &'a mut [bool],
        rename: u64,
        pairs: Vec<Pair>,
    ) -> Self {
        let mut fx = Fixing {
            cur,
            bc: Vec::new(),
            offered,
            pinned,
            rename,
            pairs,
            floor: 0,
            room: Vec::new(),
            base: Vec::new(),
        };
        fx.refresh(offers);
        let picks = fx.assign();
        fx.floor = fx.unplaced(&picks);
        let n = u32::try_from(picks.len()).unwrap_or(u32::MAX).max(1);
        fx.base = (0..n).map(|j| Some((2 * j + 1) * 1024 / (2 * n))).collect();
        fx
    }

    fn refresh(&mut self, offers: &[Offer]) {
        self.bc = offers
            .iter()
            .map(|o| o.cands.iter().map(|x| (x.0, x.1)).collect())
            .collect();
        self.room = room(self.offered, &self.bc, self.pinned);
    }

    fn assign(&self) -> Vec<Pick> {
        assign(
            self.offered,
            &self.bc,
            self.pinned,
            self.rename,
            &self.pairs,
        )
    }

    fn unplaced(&self, p: &[Pick]) -> usize {
        p.iter().filter(|x| **x == Pick::Unplaced).count()
    }

    fn try_tuple(
        &mut self,
        js: &[usize],
        t: &[(String, String, u32)],
        commit: bool,
        measure: bool,
    ) -> Option<u64> {
        let roomy = js.iter().zip(t).all(|(&j, (id, name, b))| {
            name == id
                || self
                    .room
                    .get(j)
                    .copied()
                    .flatten()
                    .is_some_and(|(lo, hi)| (lo..=hi).contains(b))
        });
        if !roomy {
            return None;
        }
        let saved: Vec<Saved> = js
            .iter()
            .map(|&j| {
                (
                    self.bc.get(j).cloned().unwrap_or_default(),
                    self.offered.get(j).copied().unwrap_or(u32::MAX),
                    self.pinned.get(j).copied().unwrap_or(false),
                )
            })
            .collect();
        for (&j, (id, name, b)) in js.iter().zip(t) {
            let c = self.cur.get(j).copied().unwrap_or(u32::MAX);
            let (bc, off, pin) = if name == id {
                (None, c, true)
            } else {
                (Some(vec![(*b, 0)]), u32::MAX, false)
            };
            if let Some(bc) = bc
                && let Some(x) = self.bc.get_mut(j)
            {
                *x = bc;
            }
            if let Some(x) = self.offered.get_mut(j) {
                *x = off;
            }
            if let Some(x) = self.pinned.get_mut(j) {
                *x = pin;
            }
        }
        let picks = self.assign();
        let fits = self.unplaced(&picks) <= self.floor
            && js.iter().zip(t).all(|(&j, (id, name, _))| {
                picks.get(j).copied()
                    == Some(if name == id {
                        Pick::Keep
                    } else {
                        Pick::Rename(0)
                    })
            });
        let kept = fits.then(|| {
            if measure {
                room_total(&room(self.offered, &self.bc, self.pinned))
            } else {
                0
            }
        });
        if !(fits && commit) {
            for (&j, s) in js.iter().zip(saved) {
                if let Some(x) = self.bc.get_mut(j) {
                    *x = s.0;
                }
                if let Some(x) = self.offered.get_mut(j) {
                    *x = s.1;
                }
                if let Some(x) = self.pinned.get_mut(j) {
                    *x = s.2;
                }
            }
        }
        kept
    }
}

fn pick_of(id: &str, name: &str, b: u32, c: &mut Vec<(u32, u32, String)>) -> Pick {
    if name == id {
        return Pick::Keep;
    }
    let k = c.iter().position(|e| e.2 == name).unwrap_or_else(|| {
        c.push((b, 0, name.to_string()));
        c.len() - 1
    });
    Pick::Rename(k)
}

fn labels_of(families: &[FamilyLine]) -> Vec<&str> {
    let mut labels: Vec<&str> = Vec::new();
    for f in families {
        if !labels.contains(&f.label.as_str()) {
            labels.push(&f.label);
        }
    }
    labels
}

fn families_of(text: &str) -> Result<Vec<FamilyLine>> {
    Ok(text
        .lines()
        .map(family_line)
        .collect::<Result<Vec<_>>>()?
        .into_iter()
        .flatten()
        .collect())
}

fn pair_families(
    text: &str,
    idents: &[String],
    offers: &mut [Offer],
    taken: &HashSet<String>,
) -> Result<(Vec<Pair>, Vec<String>)> {
    let families = families_of(text)?;
    let (mut pairs, mut paired) = (Vec::new(), Vec::new());
    for label in labels_of(&families) {
        let lines: Vec<&FamilyLine> = families.iter().filter(|f| f.label == label).collect();
        let mut rows: Vec<usize> = lines
            .iter()
            .flat_map(|l| l.members.iter())
            .filter_map(|(id, _)| idents.iter().position(|x| x == id))
            .collect();
        rows.sort_unstable();
        rows.dedup();
        let [x, y] = rows[..] else {
            continue;
        };
        if y != x + 1 {
            continue;
        }
        let ident = |r: usize| idents.get(r).map_or("", String::as_str);
        let (mut cx, mut cy) = (Vec::new(), Vec::new());
        let mut opts = Vec::new();
        for (cost, t) in family_tuples(&lines, FAMILY_TRIALS)? {
            let name_of = |row: usize| t.iter().find(|m| m.0 == ident(row));
            let (Some(mx), Some(my)) = (name_of(x), name_of(y)) else {
                continue;
            };
            let clean = |row: usize, name: &String| {
                name == ident(row)
                    || !(taken.contains(name)
                        || offers.get(row).is_some_and(|o| o.excluded.contains(name)))
            };
            if !(clean(x, &mx.1) && clean(y, &my.1)) {
                continue;
            }
            let px = pick_of(ident(x), &mx.1, mx.2, &mut cx);
            let py = pick_of(ident(y), &my.1, my.2, &mut cy);
            opts.push((px, py, cost));
        }
        if let Some(o) = offers.get_mut(x) {
            o.cands = cx;
        }
        if let Some(o) = offers.get_mut(y) {
            o.cands = cy;
        }
        pairs.push(Pair { row: x, opts });
        paired.push(label.to_string());
    }
    Ok((pairs, paired))
}

#[allow(clippy::too_many_lines)]
fn fix_families(
    text: &str,
    idents: &[String],
    offers: &mut [Offer],
    taken: &HashSet<String>,
    fx: &mut Fixing,
    paired: &HashSet<String>,
) -> Result<Vec<String>> {
    let families = families_of(text)?;
    let mut labels: Vec<&str> = Vec::new();
    for f in &families {
        if let Some((id, _)) = f.members.iter().find(|(id, _)| !idents.contains(id)) {
            return Err(anyhow!(
                "family `={}` names {id}, no walked variable",
                f.label
            ));
        }
        if !labels.contains(&f.label.as_str()) && !paired.contains(&f.label) {
            labels.push(&f.label);
        }
    }
    let mut fams: Vec<(&str, usize, Vec<Fitting>)> = Vec::new();
    for label in labels {
        let lines: Vec<&FamilyLine> = families.iter().filter(|f| f.label == label).collect();
        let all = family_tuples(&lines, FAMILY_TRIALS)?;
        let n = all.len();
        let mut fit: Vec<Fitting> = Vec::new();
        for t in all {
            let js: Vec<usize> =
                t.1.iter()
                    .filter_map(|(id, _, _)| idents.iter().position(|x| x == id))
                    .collect();
            let clean = js.iter().zip(&t.1).all(|(&j, (id, name, _))| {
                name == id
                    || !(taken.contains(name)
                        || offers.get(j).is_some_and(|o| o.excluded.contains(name)))
            });
            if clean && fx.try_tuple(&js, &t.1, false, false).is_some() {
                fit.push((js, t));
            }
        }
        fams.push((label, n, fit));
    }
    fams.sort_by_key(|f| {
        if f.0.starts_with('!') {
            (false, 0)
        } else {
            (true, f.2.len())
        }
    });
    let mut report = Vec::new();
    let mut fixed: HashSet<usize> = HashSet::new();
    for (label, n, fit) in fams {
        if fit
            .first()
            .is_some_and(|(js, _)| js.iter().any(|j| fixed.contains(j)))
        {
            report.push(format!("  FAMILY {label}: a member is fixed already"));
            continue;
        }
        let total = room_total(&fx.room).max(1);
        let mut scored: Vec<(u64, u64, u64, usize)> = Vec::new();
        for (w, (js, t)) in fit.iter().enumerate() {
            if scored.len() >= FAMILY_SCORED {
                break;
            }
            let Some(kept) = fx.try_tuple(js, &t.1, false, true) else {
                continue;
            };
            let lost = total.saturating_sub(kept) * 10_000 / total;
            let off: u64 = js
                .iter()
                .zip(&t.1)
                .map(|(&j, (_, _, b))| {
                    fx.base
                        .get(j)
                        .copied()
                        .flatten()
                        .map_or(0, |x| u64::from(x.abs_diff(*b)))
                })
                .sum();
            scored.push((u64::from(t.0) * 100 + off * 100 + lost, off, lost, w));
        }
        let best = scored.iter().min().copied();
        let won = best.map(|b| b.3);
        if let Some(w) = won
            && let Some((js, t)) = fit.get(w)
        {
            fx.try_tuple(js, &t.1, true, false);
        }
        let Some((js, (cost, t))) = won.and_then(|w| fit.get(w)) else {
            report.push(format!(
                "  FAMILY {label}: {} of its {n} tuple(s) fit alone, none with the families fixed before it",
                fit.len()
            ));
            continue;
        };
        for (&j, (id, name, b)) in js.iter().zip(t) {
            if name != id
                && let Some(o) = offers.get_mut(j)
            {
                o.cands = vec![(*b, 0, name.clone())];
            }
        }
        for (k, o) in offers.iter_mut().enumerate() {
            if !js.contains(&k) {
                o.cands
                    .retain(|x| !t.iter().any(|(_, name, _)| *name == x.2));
            }
        }
        fx.refresh(offers);
        fixed.extend(js.iter().copied());
        report.push(format!(
            "  FAMILY {label}: fit {} of {} alone (of {n}; {} scored), cost {cost}, off {}, lost {}: {}",
            won.map_or(0, |w| w + 1),
            fit.len(),
            scored.len(),
            best.map_or(0, |b| b.1),
            best.map_or(0, |b| b.2),
            t.iter()
                .map(|(id, name, b)| if name == id {
                    format!("{id} KEEP")
                } else {
                    format!("{id} -> {name} ({b})")
                })
                .collect::<Vec<_>>()
                .join(", ")
        ));
    }
    Ok(report)
}

#[allow(clippy::too_many_lines)]
fn assign_lines(
    v: &Verdict,
    walked: &[usize],
    pinned: &[bool],
    o: &PlanOpts,
    text: &str,
) -> Result<Vec<String>> {
    let (mut offers, taken) = candidates_of(text, walked, v, o.synonyms, &o.vocabulary)?;
    let cur: Vec<u32> = walked
        .iter()
        .map(|&i| v.vars.get(i).map_or(0, |x| x.bucket))
        .collect();
    let mut offered: Vec<u32> = cur
        .iter()
        .zip(&offers)
        .map(|(&b, o)| if o.refused { u32::MAX } else { b })
        .collect();
    let mut pinned = pinned.to_vec();
    let idents: Vec<String> = walked.iter().map(|&i| classify(sym(v, i)).0).collect();
    let (pairs, paired) = pair_families(text, &idents, &mut offers, &taken)?;
    let mut fx = Fixing::new(
        &cur,
        &offers,
        &mut offered,
        &mut pinned,
        o.rename_cost,
        pairs,
    );
    let skip: HashSet<String> = paired.iter().cloned().collect();
    let families = fix_families(text, &idents, &mut offers, &taken, &mut fx, &skip)?;
    let pairs = fx.pairs.clone();
    let picks = fx.assign();
    let room = fx.room.clone();
    drop(fx);
    let cands: Vec<Vec<(u32, u32, String)>> = offers.into_iter().map(|o| o.cands).collect();
    let cand = |j: usize, k: usize| cands.get(j).and_then(|c| c.get(k));
    let placed: Vec<Option<u32>> = picks
        .iter()
        .enumerate()
        .map(|(j, p)| match p {
            Pick::Keep => cur.get(j).copied(),
            Pick::Rename(k) => cand(j, *k).map(|c| c.0),
            Pick::Unplaced => None,
        })
        .collect();
    let count = |f: fn(&Pick) -> bool| picks.iter().filter(|p| f(p)).count();
    let mut out = vec![format!(
        "ASSIGN {}: {} walked, {} with candidates; {} kept, {} renamed, {} unplaced (longest ordered run: {} \
         misplaced)",
        v.stem,
        walked.len(),
        cands.iter().filter(|c| !c.is_empty()).count(),
        count(|p| *p == Pick::Keep),
        count(|p| matches!(p, Pick::Rename(_))),
        count(|p| *p == Pick::Unplaced),
        cur.len() - longest_ordered(&cur).len()
    )];
    out.push(
        "  image-rva  size    bucket  pick                                 room      symbol   (room: the buckets \
         a new name of this variable alone may take, the rest re-placed)"
            .to_string(),
    );
    for (j, &i) in walked.iter().enumerate() {
        let Some(x) = v.vars.get(i) else { continue };
        let rva = x
            .image
            .map_or_else(|| "unseated".to_string(), |r| format!("{r:#x}"));
        let pick = match picks.get(j).copied().unwrap_or(Pick::Unplaced) {
            Pick::Keep if pinned.get(j).copied().unwrap_or(false) => "KEEP (pinned)".to_string(),
            Pick::Keep => "KEEP".to_string(),
            Pick::Rename(k) => {
                cand(j, k).map_or_else(String::new, |c| format!("-> {} ({})", c.2, c.0))
            }
            Pick::Unplaced => {
                let lo = placed
                    .get(..j)
                    .unwrap_or_default()
                    .iter()
                    .rev()
                    .find_map(|p| *p)
                    .unwrap_or(0);
                let hi = placed
                    .get(j + 1..)
                    .unwrap_or_default()
                    .iter()
                    .find_map(|p| *p)
                    .unwrap_or(0x3ff);
                format!("UNPLACED: name into {lo}..={hi}")
            }
        };
        let r = room
            .get(j)
            .copied()
            .flatten()
            .map_or_else(|| "-".to_string(), |(lo, hi)| format!("{lo}..={hi}"));
        out.push(format!(
            "  {rva:<9}  {:>6}  {:>6}  {pick:<35}  {r:<9} {}",
            x.size, x.bucket, x.symbol
        ));
    }
    if !families.is_empty() {
        out.push("FAMILIES, in the order fixed (see `fix_families`):".to_string());
        out.extend(families);
    }
    if !pairs.is_empty() {
        out.push("PAIRS, named jointly with the rest:".to_string());
        let name = |j: usize| {
            let id = idents.get(j).map_or("", String::as_str);
            match picks.get(j).copied().unwrap_or(Pick::Unplaced) {
                Pick::Keep => format!("{id} KEEP"),
                Pick::Rename(k) => {
                    cand(j, k).map_or_else(String::new, |c| format!("{id} -> {} ({})", c.2, c.0))
                }
                Pick::Unplaced => format!("{id} UNPLACED"),
            }
        };
        for (p, label) in pairs.iter().zip(&paired) {
            out.push(format!(
                "  PAIR {label}: {}, {} (of {} option(s))",
                name(p.row),
                name(p.row + 1),
                p.opts.len()
            ));
        }
    }
    Ok(out)
}

pub fn plan_lines(v: &Verdict, o: &PlanOpts) -> Result<Vec<String>> {
    let walked: Vec<usize> = v
        .image_order
        .iter()
        .copied()
        .filter(|&i| v.vars.get(i).is_some_and(|x| x.zero_init.is_none()))
        .collect();
    let b: Vec<u32> = walked
        .iter()
        .map(|&i| v.vars.get(i).map_or(0, |x| x.bucket))
        .collect();
    let pinned: Vec<bool> = walked
        .iter()
        .map(|&i| {
            let s = sym(v, i);
            o.pins.iter().any(|p| p == s || *p == classify(s).0)
        })
        .collect();
    let unknown: Vec<&String> = o
        .pins
        .iter()
        .filter(|p| {
            !walked
                .iter()
                .any(|&i| **p == sym(v, i) || **p == classify(sym(v, i)).0)
        })
        .collect();
    if !unknown.is_empty() {
        return Err(anyhow!(
            "--pin names no walked variable of {}: {}",
            v.stem,
            unknown
                .iter()
                .map(|s| s.as_str())
                .collect::<Vec<_>>()
                .join(" ")
        ));
    }
    if let Some(text) = &o.assign {
        return assign_lines(v, &walked, &pinned, o, text);
    }
    let renames = |p: &[Slot]| p.iter().filter(|s| **s != Slot::Keep).count();
    let narrowest = |p: &[Slot]| {
        p.iter()
            .filter_map(|s| match s {
                Slot::Rename { lo, hi } => Some(hi - lo + 1),
                Slot::Keep => None,
            })
            .min()
    };
    let mut out = vec![format!(
        "{}: {} walked variable(s) placed, {} pinned; the longest ordered run leaves {} rename(s)",
        v.stem,
        b.len(),
        pinned.iter().filter(|&&p| p).count(),
        b.len() - longest_ordered(&b).len()
    )];
    out.push("  k (narrowest window)  renames  narrowest  anchors".to_string());
    for k in [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64] {
        match plan(&b, k, &pinned) {
            Some(p) => out.push(format!(
                "  {k:>20}  {:>7}  {:>9}  {:>7}",
                renames(&p),
                narrowest(&p).map_or_else(|| "-".to_string(), |w| w.to_string()),
                p.len() - renames(&p)
            )),
            None => out.push(format!("  {k:>20}  no plan")),
        }
    }
    let p = plan(&b, o.k, &pinned)
        .ok_or_else(|| anyhow!("no plan gives every rename {} bucket(s)", o.k))?;
    out.push(format!(
        "PLAN k={}: {} rename(s), {} anchor(s); each RENAME's window is its own (disjoint, image order), so any \
         subset lands independently",
        o.k,
        renames(&p),
        p.len() - renames(&p)
    ));
    out.push("  image-rva  size    bucket  plan           symbol".to_string());
    for (j, &i) in walked.iter().enumerate() {
        let Some(x) = v.vars.get(i) else { continue };
        let rva = x
            .image
            .map_or_else(|| "unseated".to_string(), |r| format!("{r:#x}"));
        match p.get(j).copied().unwrap_or(Slot::Keep) {
            Slot::Keep => out.push(format!(
                "  {rva:<9}  {:>6}  {:>6}  {:<13}  {}",
                x.size,
                x.bucket,
                if pinned.get(j).copied().unwrap_or(false) {
                    "KEEP (pinned)"
                } else {
                    "KEEP"
                },
                x.symbol
            )),
            Slot::Rename { lo, hi } => {
                out.push(format!(
                    "  {rva:<9}  {:>6}  {:>6}  {:<13}  {}",
                    x.size,
                    x.bucket,
                    format!("-> {lo}..={hi}"),
                    x.symbol
                ));
                if o.suggest > 0 {
                    let id = classify(&x.symbol).0;
                    let s: Vec<String> = bsssuggest::suggest(&id, lo, hi, &o.vocabulary)?
                        .into_iter()
                        .take(o.suggest)
                        .map(|(_, b, s)| format!("{s}({b})"))
                        .collect();
                    if !s.is_empty() {
                        out.push(format!("             suggest: {}", s.join(" ")));
                    }
                }
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_assign_token_is_an_exclusion_a_tiered_pattern_or_a_pattern() {
        assert_eq!(assign_token("-g_foo"), AssignToken::Exclude("g_foo"));
        assert_eq!(
            assign_token("40:g_[a,b]X"),
            AssignToken::Pattern(40, "g_[a,b]X")
        );
        assert_eq!(
            assign_token("g_[a,b]X"),
            AssignToken::Pattern(0, "g_[a,b]X")
        );
        assert_eq!(assign_token("x:g_a"), AssignToken::Pattern(0, "x:g_a"));
    }

    #[test]
    fn a_family_shares_its_brace_slots_and_varies_its_bracket_slots() -> Result<()> {
        assert_eq!(family_line("g_penX 0:g_a")?, None);
        let l = family_line("=pen 2 g_penX=g_{pen,line}[X,PosX] g_penY=g_{pen,line}Y # c")?
            .ok_or_else(|| anyhow!("a family line"))?;
        assert_eq!((l.label.as_str(), l.tier, l.members.len()), ("pen", 2, 2));
        let names: Vec<(u32, String)> = family_tuples(&[&l], 100)?
            .iter()
            .map(|(c, m)| {
                (
                    *c,
                    format!(
                        "{}+{}",
                        m.first().map_or("", |x| x.1.as_str()),
                        m.get(1).map_or("", |x| x.1.as_str())
                    ),
                )
            })
            .collect();
        assert_eq!(
            names,
            vec![
                (2, "g_penX+g_penY".to_string()),
                (3, "g_lineX+g_lineY".to_string()),
                (3, "g_penPosX+g_penY".to_string()),
                (4, "g_linePosX+g_lineY".to_string()),
            ]
        );
        let unequal =
            family_line("=x 0 g_a=g_{a,b} g_b=g_{a}")?.ok_or_else(|| anyhow!("a family line"))?;
        assert!(family_tuples(&[&unequal], 9).is_err());
        assert!(family_line("=x 0").is_err());
        assert!(family_line("=x g_a=g_a g_b=g_b").is_err());
        assert_eq!(
            family_line("=x 0 g_a=g_a")?.map(|l| l.members.len()),
            Some(1)
        );
        let alike =
            family_line("=x 0 g_a=g_[a,b] g_b=g_[b,a]")?.ok_or_else(|| anyhow!("a family line"))?;
        assert_eq!(family_tuples(&[&alike], 9)?.len(), 2);
        Ok(())
    }
}
