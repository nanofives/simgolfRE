use crate::budget::Budget;
use crate::coffx;
use anyhow::{Result, bail};
use msvc6_diff::comdat;
use msvc6_diff::env::Env;
use msvc6_diff::score::{self, Fits, Plan, Scored, Seat};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const GAP: u32 = 0x100;

pub const MAX_MEMBERS: usize = 1 << 14;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum Kind {
    User,
    Inl,
    Gen,
    Thunk,
}

impl Kind {
    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Kind::User => "USER",
            Kind::Inl => "INL",
            Kind::Gen => "GEN",
            Kind::Thunk => "THUNK",
        }
    }
}

#[must_use]
pub fn kind_of(name: &str, any: bool) -> Kind {
    if !any {
        return Kind::User;
    }
    let tail = name.split_once("@@").map_or("", |(_, t)| t);
    if tail.starts_with("$4") || tail.starts_with("$R") {
        return Kind::Thunk;
    }
    if let Some(rest) = tail.strip_prefix('W')
        && rest.find('@').is_some_and(|i| {
            i > 0
                && rest
                    .get(..i)
                    .is_some_and(|s| s.bytes().all(|b| b.is_ascii_uppercase()))
        })
    {
        return Kind::Thunk;
    }
    if name.starts_with("??_G") || name.starts_with("??_E") || name.starts_with("??_D") {
        return Kind::Gen;
    }
    Kind::Inl
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Member {
    pub name: String,
    pub rva: u32,
    pub len: u32,
    pub kind: Kind,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Fate {
    Own,
    Dup(String),
    Icf,
}

#[must_use]
pub fn lis(a: &[u32]) -> BTreeSet<usize> {
    let mut tails: Vec<usize> = Vec::new();
    let mut prev: Vec<Option<usize>> = vec![None; a.len()];
    let val = |i: usize| a.get(i).copied().unwrap_or(0);
    for i in 0..a.len() {
        let pos = tails.partition_point(|&t| val(t) < val(i));
        if pos > 0
            && let Some(p) = prev.get_mut(i)
        {
            *p = tails.get(pos - 1).copied();
        }
        if pos == tails.len() {
            tails.push(i);
        } else if let Some(t) = tails.get_mut(pos) {
            *t = i;
        }
    }
    let mut out = BTreeSet::new();
    let mut cur = tails.last().copied();
    while let Some(i) = cur {
        out.insert(i);
        cur = prev.get(i).copied().flatten();
    }
    out
}

fn lis_of(seq: &[&Member]) -> BTreeSet<usize> {
    let a: Vec<u32> = seq.iter().map(|m| m.rva).collect();
    lis(&a)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Scoring {
    pub run: BTreeSet<usize>,
    pub lo: u32,
    pub hi: u32,
    pub order: Vec<usize>,
    pub away: Vec<usize>,
}

#[must_use]
pub fn score_of(seq: &[&Member]) -> Scoring {
    let run = lis_of(seq);
    let kept = || {
        seq.iter()
            .enumerate()
            .filter(|(i, _)| run.contains(i))
            .map(|(_, m)| *m)
    };
    let mut lo = kept().map(|m| m.rva).min().unwrap_or(0);
    let mut hi = kept()
        .map(|m| m.rva.saturating_add(m.len))
        .max()
        .unwrap_or(0);
    let mut rounds = 0usize;
    loop {
        let mut grew = false;
        for m in seq {
            if m.rva >= hi && m.rva < hi.saturating_add(GAP) {
                hi = m.rva.saturating_add(m.len.max(1));
                grew = true;
            }
            if m.rva < lo && m.rva.saturating_add(m.len).saturating_add(GAP) > lo {
                lo = m.rva;
                grew = true;
            }
        }
        rounds += 1;
        if !grew || rounds > 2 * seq.len() + 2 {
            break;
        }
    }
    let (mut order, mut away) = (Vec::new(), Vec::new());
    for (i, m) in seq.iter().enumerate() {
        if run.contains(&i) {
            continue;
        }
        if m.rva >= lo && m.rva < hi {
            order.push(i);
        } else {
            away.push(i);
        }
    }
    Scoring {
        run,
        lo,
        hi,
        order,
        away,
    }
}

#[must_use]
pub fn bytes(seq: &[&Member], ix: &[usize]) -> u32 {
    ix.iter()
        .filter_map(|&i| seq.get(i))
        .map(|m| m.len)
        .fold(0u32, u32::saturating_add)
}

#[must_use]
pub fn end_drain(v: &[Member]) -> Vec<&Member> {
    v.iter()
        .filter(|m| m.kind == Kind::User)
        .chain(v.iter().filter(|m| m.kind != Kind::User))
        .collect()
}

#[must_use]
pub fn drain_points(v: &[Member], lo: u32, hi: u32) -> Vec<(Option<String>, Option<String>)> {
    let mut last: Option<String> = None;
    let mut ours = Vec::with_capacity(v.len());
    for m in v {
        ours.push(last.clone());
        if m.kind == Kind::User {
            last = Some(m.name.clone());
        }
    }
    let mut by_rva: Vec<(u32, usize, &Member)> = v
        .iter()
        .enumerate()
        .filter(|(_, m)| m.rva >= lo && m.rva < hi)
        .map(|(i, m)| (m.rva, i, m))
        .collect();
    by_rva.sort_unstable_by_key(|x| (x.0, x.1));
    let mut refp: BTreeMap<usize, Option<String>> = BTreeMap::new();
    let mut last: Option<String> = None;
    for (_, i, m) in by_rva {
        refp.insert(i, last.clone());
        if m.kind == Kind::User {
            last = Some(m.name.clone());
        }
    }
    ours.into_iter()
        .enumerate()
        .map(|(i, o)| (o, refp.get(&i).cloned().flatten()))
        .collect()
}

fn short(n: Option<&String>) -> String {
    n.map_or_else(|| "(start)".to_string(), String::clone)
}

#[must_use]
pub fn icf_keepers(
    objects: &[(String, Vec<Member>)],
    at: &BTreeMap<u32, usize>,
) -> BTreeMap<u32, usize> {
    let folded = |m: &Member| at.get(&m.rva).copied().unwrap_or(0) >= 2;
    let mut keep: BTreeMap<u32, usize> = BTreeMap::new();
    for (oi, (_, v)) in objects.iter().enumerate() {
        let plain: Vec<&Member> = v.iter().filter(|m| !folded(m)).collect();
        if plain.is_empty() {
            continue;
        }
        let s = score_of(&plain);
        for m in v.iter().filter(|m| folded(m)) {
            if m.rva >= s.lo && m.rva < s.hi {
                keep.entry(m.rva).or_insert(oi);
            }
        }
    }
    keep
}

#[must_use]
pub fn icf_pick(
    v: &[Member],
    at: &BTreeMap<u32, usize>,
    keep: impl Fn(u32) -> bool,
) -> BTreeSet<usize> {
    let folded = |m: &Member| at.get(&m.rva).copied().unwrap_or(0) >= 2;
    let plain: Vec<(usize, &Member)> = v.iter().enumerate().filter(|(_, m)| !folded(m)).collect();
    let seq: Vec<&Member> = plain.iter().map(|(_, m)| *m).collect();
    let run = lis_of(&seq);
    let ran: Vec<(usize, u32)> = plain
        .iter()
        .enumerate()
        .filter(|(k, _)| run.contains(k))
        .map(|(_, (i, m))| (*i, m.rva))
        .collect();
    let fits = |i: usize, rva: u32| {
        let before = ran.iter().rev().find(|(j, _)| *j < i).map(|(_, r)| *r);
        let after = ran.iter().find(|(j, _)| *j > i).map(|(_, r)| *r);
        before.is_none_or(|r| r < rva) && after.is_none_or(|r| r > rva)
    };
    let mut first: BTreeMap<u32, usize> = BTreeMap::new();
    let mut fitting: BTreeMap<u32, usize> = BTreeMap::new();
    for (i, m) in v
        .iter()
        .enumerate()
        .filter(|(_, m)| folded(m) && keep(m.rva))
    {
        first.entry(m.rva).or_insert(i);
        if fits(i, m.rva) {
            fitting.entry(m.rva).or_insert(i);
        }
    }
    first
        .iter()
        .map(|(rva, i)| fitting.get(rva).copied().unwrap_or(*i))
        .collect()
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Fossil {
    Foreign {
        name: String,
        tu: String,
        rva: u32,
        after: bool,
    },
    AppendPair {
        dtor: String,
    },
    PullPinned {
        ahead_of: String,
        mine: Vec<(String, usize)>,
        theirs: Vec<(String, usize)>,
    },
    Drain {
        point: String,
        past: String,
        cause: String,
    },
}

fn members_list(v: &[(String, usize)]) -> String {
    v.iter()
        .map(|(stem, p)| format!("{stem} {p}"))
        .collect::<Vec<_>>()
        .join(", ")
}

impl Fossil {
    #[must_use]
    pub fn why(&self) -> String {
        match self {
            Fossil::Foreign {
                name,
                tu,
                rva,
                after,
            } => format!(
                "foreign body {} it: {tu}'s {name} at {rva:#x}",
                if *after { "after" } else { "before" }
            ),
            Fossil::AppendPair { dtor } => format!("appended with its destructor {dtor}"),
            Fossil::PullPinned {
                ahead_of,
                mine,
                theirs,
            } => format!(
                "pull order pins it ahead of {ahead_of}: it first pulls {} before that body's {} (reference member \
                 order)",
                members_list(mine),
                members_list(theirs)
            ),
            Fossil::Drain { point, past, cause } => format!(
                "drains after the fossil {point} in ours, the reference has it past {past}; {cause}"
            ),
        }
    }
}

type Body = (Option<usize>, BTreeSet<usize>);

pub type Evidence<'a> = dyn Fn(&[(String, Vec<Member>)], &[PathBuf]) -> Result<LinkEvidence> + 'a;

#[derive(Default, Debug, Clone)]
pub struct LinkEvidence {
    pub bodies: Vec<Vec<Body>>,
    pub members: Vec<(String, String)>,
    pub definer: BTreeMap<String, (String, String)>,
}

#[must_use]
pub fn deleting_dtor_class(name: &str) -> Option<&str> {
    let rest = name.strip_prefix("??_G")?.strip_suffix("AEPAXI@Z")?;
    let class = rest.get(..rest.len().checked_sub(1)?)?;
    class.ends_with("@@").then_some(class)
}

pub const FOSSIL_STEPS_PER_MEMBER: u64 = 1 << 12;

pub const FOSSIL_STEP_FLOOR: u64 = 1 << 24;

struct Pulls<'a> {
    ev: &'a LinkEvidence,
    by: BTreeMap<(usize, usize), Vec<usize>>,
    bud: &'a Budget,
}

fn first_pullers(ev: &LinkEvidence) -> BTreeMap<(usize, usize), Vec<usize>> {
    let mut first = BTreeMap::new();
    for (oi, bodies) in ev.bodies.iter().enumerate() {
        for (b, (_, refs)) in bodies.iter().enumerate() {
            for &p in refs {
                first.entry(p).or_insert((oi, b));
            }
        }
    }
    let mut by: BTreeMap<(usize, usize), Vec<usize>> = BTreeMap::new();
    for (p, w) in first {
        by.entry(w).or_default().push(p);
    }
    by
}

fn owners(bodies: &[Body]) -> BTreeMap<usize, usize> {
    let mut owner = BTreeMap::new();
    for (b, (own, _)) in bodies.iter().enumerate() {
        if let Some(mi) = own {
            owner.entry(*mi).or_insert(b);
        }
    }
    owner
}

fn pull_pinned(
    v: &[Member],
    s: &Scoring,
    i: usize,
    oi: usize,
    owner: &BTreeMap<usize, usize>,
    pl: &Pulls<'_>,
) -> Option<(usize, Fossil)> {
    let (ev, bud) = (pl.ev, pl.bud);
    let x = v.get(i)?;
    let bodies = ev.bodies.get(oi)?;
    let body = |mi: usize| owner.get(&mi).copied();
    let pulled = |b: usize| -> Vec<usize> { pl.by.get(&(oi, b)).cloned().unwrap_or_default() };
    let bx = body(i)?;
    let xs = pulled(bx);
    if xs.is_empty() {
        return None;
    }
    let named = |ps: &[usize]| -> Vec<(String, usize)> {
        ps.iter()
            .filter_map(|&p| ev.members.get(p).map(|(stem, _)| (stem.clone(), p + 1)))
            .collect()
    };
    for j in (i + 1)..v.len() {
        if !bud.tick() {
            return None;
        }
        let Some(r) = v.get(j).filter(|r| s.run.contains(&j) && r.rva < x.rva) else {
            continue;
        };
        let Some(br) = body(j).filter(|&br| br > bx) else {
            continue;
        };
        let ahead_in_ref = bud
            .it(&s.order)
            .filter(|&&k| k > j && v.get(k).is_some_and(|y| y.rva < r.rva))
            .filter_map(|&k| body(k))
            .filter_map(|b| bodies.get(b));
        let between: BTreeSet<usize> = bud
            .it(bodies.get(bx + 1..=br).unwrap_or_default())
            .chain(ahead_in_ref)
            .flat_map(|(_, refs)| refs.iter().copied())
            .collect();
        let mine: Vec<usize> = xs
            .iter()
            .copied()
            .filter(|p| !between.contains(p))
            .collect();
        let theirs = pulled(br);
        let archive = |p: &usize| ev.members.get(*p).map(|(_, a)| a.as_str());
        let archives: BTreeSet<&str> = mine.iter().filter_map(archive).collect();
        for a in archives {
            let m: Vec<usize> = mine
                .iter()
                .copied()
                .filter(|p| archive(p) == Some(a))
                .collect();
            let t: Vec<usize> = theirs
                .iter()
                .copied()
                .filter(|p| archive(p) == Some(a))
                .collect();
            if let (Some(&hi), Some(&lo)) = (m.iter().max(), t.iter().min())
                && hi < lo
            {
                return Some((
                    j,
                    Fossil::PullPinned {
                        ahead_of: r.name.clone(),
                        mine: named(&m),
                        theirs: named(&t),
                    },
                ));
            }
        }
    }
    None
}

fn drained_after_fossil(
    v: &[Member],
    d: usize,
    pins: &BTreeMap<usize, usize>,
    ev: &LinkEvidence,
) -> Option<Fossil> {
    let m = v.get(d)?;
    let cause = match m.kind {
        Kind::Gen => "compiler-generated".to_string(),
        Kind::Inl => {
            let (archive, stem) = ev.definer.get(&m.name)?;
            format!("a toolchain header body: {archive}'s {stem} defines it too")
        }
        Kind::User | Kind::Thunk => return None,
    };
    let p = (0..d)
        .rev()
        .find(|&k| v.get(k).is_some_and(|x| x.kind == Kind::User))?;
    let past = pins
        .get(&p)
        .and_then(|&r| v.get(r))
        .filter(|x| x.rva < m.rva)?;
    Some(Fossil::Drain {
        point: v.get(p)?.name.clone(),
        past: past.name.clone(),
        cause,
    })
}

#[must_use]
pub fn ilink_fossils(
    objects: &[(String, Vec<Member>)],
    scores: &[Scoring],
    ev: &LinkEvidence,
    bud: &Budget,
) -> Vec<BTreeMap<usize, Fossil>> {
    let pl = Pulls {
        ev,
        by: first_pullers(ev),
        bud,
    };
    let mut global: Vec<(u32, usize, usize)> = objects
        .iter()
        .enumerate()
        .flat_map(|(oi, (_, v))| v.iter().enumerate().map(move |(mi, m)| (m.rva, oi, mi)))
        .collect();
    global.sort_unstable();
    let pos: BTreeMap<(usize, usize), usize> = global
        .iter()
        .enumerate()
        .map(|(p, &(_, oi, mi))| ((oi, mi), p))
        .collect();
    let member = |oi: usize, mi: usize| objects.get(oi).and_then(|(_, v)| v.get(mi));
    let mut out = Vec::with_capacity(objects.len());
    for (oi, ((_, v), s)) in objects.iter().zip(scores).enumerate() {
        let mut mine = BTreeMap::new();
        let mut pins: BTreeMap<usize, usize> = BTreeMap::new();
        let owner = owners(ev.bodies.get(oi).map_or(&[][..], Vec::as_slice));
        for &i in &s.order {
            let (Some(m), Some(&p)) = (v.get(i), pos.get(&(oi, i))) else {
                continue;
            };
            let pinned = if m.kind == Kind::User {
                pull_pinned(v, s, i, oi, &owner, &pl)
            } else {
                None
            };
            if let Some((r, _)) = &pinned {
                pins.insert(i, *r);
            }
            let prev = p.checked_sub(1).and_then(|q| global.get(q));
            let next = global.get(p + 1);
            let foreign = [(prev, false), (next, true)]
                .into_iter()
                .find_map(|(nb, after)| {
                    let &(rva, no, nm) = nb?;
                    (no != oi && rva >= s.lo && rva < s.hi).then(|| Fossil::Foreign {
                        name: member(no, nm).map_or_else(String::new, |x| x.name.clone()),
                        tu: objects.get(no).map_or_else(String::new, |o| o.0.clone()),
                        rva,
                        after,
                    })
                });
            if let Some(f) = foreign {
                mine.insert(i, f);
                continue;
            }
            if m.kind == Kind::User {
                if let Some((_, f)) = pinned {
                    mine.insert(i, f);
                }
                continue;
            }
            let Some(class) = deleting_dtor_class(&m.name) else {
                continue;
            };
            let dtor = format!("??1{class}");
            let pair = prev
                .filter(|&&(_, po, _)| po == oi)
                .and_then(|&(_, _, pm)| v.get(pm).map(|d| (pm, d)))
                .filter(|(_, d)| {
                    d.kind == Kind::User
                        && d.name.starts_with(&dtor)
                        && m.rva >= d.rva.saturating_add(d.len)
                        && u64::from(m.rva - d.rva) <= reserve(u64::from(d.len))
                });
            if let Some((pm, d)) = pair
                && i.checked_sub(1) != Some(pm)
            {
                mine.insert(
                    i,
                    Fossil::AppendPair {
                        dtor: d.name.clone(),
                    },
                );
            }
        }
        let drained: Vec<(usize, Fossil)> = s
            .order
            .iter()
            .filter(|i| !mine.contains_key(i))
            .filter_map(|&d| drained_after_fossil(v, d, &pins, ev).map(|f| (d, f)))
            .collect();
        mine.extend(drained);
        out.push(mine);
    }
    out
}

#[must_use]
pub fn reserve(body: u64) -> u64 {
    (body.saturating_mul(5) / 4).div_ceil(16) * 16
}

#[must_use]
pub fn ilt(text: &[u8], base: u32) -> Vec<(u32, u32)> {
    let at = |rva: u32| -> Option<u8> {
        rva.checked_sub(base)
            .and_then(|o| text.get(o as usize))
            .copied()
    };
    let Some(start) = (0..16u32).find(|d| at(base.wrapping_add(*d)) == Some(0xe9)) else {
        return Vec::new();
    };
    let mut out = Vec::new();
    let mut a = base.wrapping_add(start);
    while at(a) == Some(0xe9) {
        let Some(off) = a.checked_sub(base).map(|o| o as usize + 1) else {
            break;
        };
        let Some(raw) = text.get(off..off + 4) else {
            break;
        };
        let Ok(b) = <[u8; 4]>::try_from(raw) else {
            break;
        };
        let Ok(t) = u32::try_from(i64::from(a) + 5 + i64::from(i32::from_le_bytes(b))) else {
            break;
        };
        out.push((a, t));
        let Some(next) = a.checked_add(5) else {
            break;
        };
        a = next;
    }
    out
}

pub const ORDER_HEADER: &str =
    "object\town\torder\torder_bytes\tfossils\tfossil_bytes\tend_drain\tend_drain_bytes\taway";

#[derive(Debug, Clone)]
pub struct SeatedObject {
    pub tu: String,
    pub path: PathBuf,
    pub comdats: Vec<Scored>,
}

#[derive(Debug, Clone, Default)]
pub struct SeatedDir {
    pub objects: Vec<SeatedObject>,
    pub warnings: Vec<String>,
    pub text_base: u32,
}

fn find_ci(dir: &Path, name: &str) -> Option<PathBuf> {
    let p = dir.join(name);
    if p.is_file() {
        return Some(p);
    }
    std::fs::read_dir(dir)
        .ok()?
        .flatten()
        .map(|e| e.path())
        .find(|p| {
            p.file_name()
                .is_some_and(|n| n.to_string_lossy().eq_ignore_ascii_case(name))
        })
}

fn select_any_of(p: &Path) -> BTreeMap<String, Vec<u8>> {
    let Ok(d) = comdat::read(p) else {
        return BTreeMap::new();
    };
    let any = coffx::comdat_selections_of(&d).unwrap_or_default();
    comdat::text_of(&d)
        .unwrap_or_default()
        .into_iter()
        .filter_map(|c| {
            let n = c.symbol?;
            any.get(&n)
                .copied()
                .unwrap_or(false)
                .then_some((n, c.bytes))
        })
        .collect()
}

#[must_use]
pub fn earlier_kept_copy(
    order: &[String],
    own: &str,
    mut def: impl FnMut(&str) -> Option<Vec<u8>>,
    fits: impl Fn(&[u8]) -> bool,
) -> Option<String> {
    for s in order.iter().take_while(|s| s.as_str() != own) {
        if let Some(b) = def(s) {
            return fits(&b).then(|| s.clone());
        }
    }
    None
}

#[must_use]
pub fn seat_dir(env: &Env, objs: &[PathBuf], order: &[String]) -> SeatedDir {
    let mut objs: Vec<PathBuf> = objs
        .iter()
        .filter(|p| p.extension().is_some_and(|x| x.eq_ignore_ascii_case("obj")))
        .cloned()
        .collect();
    objs.sort_by(|a, b| a.file_name().cmp(&b.file_name()));
    let mut out = SeatedDir {
        text_base: env.text_rva,
        ..SeatedDir::default()
    };
    for o in &objs {
        let tu = msvc6_diff::seat::scope_stem(o);
        let d = match comdat::read(o) {
            Ok(d) => d,
            Err(e) => {
                out.warnings.push(format!("{tu}: {e:#}"));
                continue;
            }
        };
        let comdats = match comdat::text_of(&d) {
            Ok(c) => c,
            Err(e) => {
                out.warnings.push(format!("{tu}: {e:#}"));
                continue;
            }
        };
        if comdats.len() > MAX_MEMBERS {
            out.warnings.push(format!(
                "{tu}: {} .text COMDATs, more than {MAX_MEMBERS} -- refused, left out",
                comdats.len()
            ));
            continue;
        }
        let ms = match msvc6_diff::seat::members_of(env, o, &d, &comdats) {
            Ok(m) => m,
            Err(e) => {
                out.warnings.push(format!("{tu}: {e:#}"));
                continue;
            }
        };
        let mut kept = Vec::new();
        let mut at = Vec::new();
        for (m, c) in ms.iter().zip(comdats) {
            match m.rva {
                Some(r) => {
                    kept.push(c);
                    at.push(r);
                }
                None => out.warnings.push(format!(
                    "{tu}: {} resolves to no address (no name oracle row, or a renumbered `_$E`) -- left out",
                    m.name
                )),
            }
        }
        let Some((&root, members)) = at.split_first() else {
            continue;
        };
        let own = order.iter().find(|s| s.eq_ignore_ascii_case(&tu)).cloned();
        let flat = o.parent().map(Path::to_path_buf);
        let cache: std::cell::RefCell<BTreeMap<String, BTreeMap<String, Vec<u8>>>> =
            std::cell::RefCell::new(BTreeMap::new());
        let resolver = |sym: &str, fits: &Fits<'_>| -> Option<String> {
            let own = own.as_deref()?;
            let dir = flat.as_deref()?;
            earlier_kept_copy(
                order,
                own,
                |s| {
                    cache
                        .borrow_mut()
                        .entry(s.to_string())
                        .or_insert_with(|| {
                            find_ci(dir, &format!("{s}.obj"))
                                .map(|p| select_any_of(&p))
                                .unwrap_or_default()
                        })
                        .get(sym)
                        .cloned()
                },
                fits,
            )
        };
        match score::seat_and_score_with(
            env,
            &d,
            &kept,
            root,
            &Plan::Asserted(members.to_vec()),
            &resolver,
        ) {
            Ok(s) => out.objects.push(SeatedObject {
                tu,
                path: o.clone(),
                comdats: s.members,
            }),
            Err(e) => out.warnings.push(format!("{tu}: {e:#}")),
        }
    }
    out
}

#[derive(Debug, Clone, Default)]
pub struct Opts {
    pub all: bool,
    pub dups: bool,
    pub ilink: bool,
    pub show: Vec<String>,
    pub tsv: bool,
}

#[derive(Debug, Clone, Default)]
pub struct Ordered {
    pub objects: Vec<(String, Vec<Member>)>,
    pub paths: Vec<PathBuf>,
    pub scores: Vec<Scoring>,
    pub fossils: Vec<BTreeMap<usize, Fossil>>,
    pub incremental: bool,
    pub lines: Vec<String>,
    pub tsv: String,
}

#[must_use]
pub fn reference_is_incremental(env: &Env) -> bool {
    !ilt(env.text(), env.text_rva).is_empty()
}

#[allow(clippy::too_many_lines)]
pub fn order_objects(
    seated: &SeatedDir,
    link_order: &[String],
    incremental: bool,
    evidence: &Evidence<'_>,
    o: &Opts,
) -> Result<Ordered> {
    let mut out = Ordered {
        incremental,
        ..Ordered::default()
    };
    let lines = &mut out.lines;
    for w in &seated.warnings {
        lines.push(format!("  warning: {w}"));
    }
    let pos = |tu: &str| {
        link_order
            .iter()
            .position(|s| s.eq_ignore_ascii_case(tu))
            .unwrap_or(usize::MAX)
    };
    let mut seated_objs: Vec<&SeatedObject> = seated.objects.iter().collect();
    seated_objs.sort_by_key(|o| (pos(&o.tu), o.tu.clone()));
    let mut first_any: BTreeMap<String, String> = BTreeMap::new();
    let mut sels: Vec<BTreeMap<String, bool>> = Vec::new();
    for so in &seated_objs {
        let sel = comdat::read(&so.path)
            .ok()
            .and_then(|d| coffx::comdat_selections_of(&d).ok())
            .unwrap_or_default();
        for (n, any) in &sel {
            if *any {
                first_any.entry(n.clone()).or_insert_with(|| so.tu.clone());
            }
        }
        sels.push(sel);
    }
    let mut objects: Vec<(String, Vec<Member>)> = Vec::new();
    let mut full: Vec<Vec<(Member, Fate)>> = Vec::new();
    let mut dup = 0usize;
    for (so, sel) in seated_objs.iter().zip(&sels) {
        let mut rows: Vec<&Scored> = so
            .comdats
            .iter()
            .filter(|d| !d.seat.excluded() && d.seat != Seat::Unseated)
            .filter(|d| d.at >= seated.text_base)
            .collect();
        rows.sort_by_key(|d| d.comdat.index);
        let mut v: Vec<Member> = Vec::new();
        let mut f: Vec<(Member, Fate)> = Vec::new();
        for d in rows {
            let name = d.comdat.symbol.clone().unwrap_or_else(|| d.name.clone());
            let any = sel.get(&name).copied().unwrap_or(false);
            let m = Member {
                kind: kind_of(&name, any),
                rva: d.at,
                len: u32::try_from(d.comdat.bytes.len()).unwrap_or(u32::MAX),
                name,
            };
            if let Some(t) = first_any.get(&m.name).filter(|t| any && **t != so.tu) {
                dup += 1;
                f.push((m, Fate::Dup(t.clone())));
                continue;
            }
            f.push((m.clone(), Fate::Own));
            v.push(m);
        }
        objects.push((so.tu.clone(), v));
        full.push(f);
    }
    let mut at_count: BTreeMap<u32, usize> = BTreeMap::new();
    for (_, v) in &objects {
        for m in v {
            *at_count.entry(m.rva).or_default() += 1;
        }
    }
    let keepers = icf_keepers(&objects, &at_count);
    let (mut icf, mut paired) = (0usize, 0usize);
    for (oi, ((_, v), f)) in objects.iter_mut().zip(&mut full).enumerate() {
        let picked = icf_pick(v, &at_count, |rva| keepers.get(&rva) == Some(&oi));
        let mut gone: BTreeSet<String> = BTreeSet::new();
        let mut i = 0usize;
        v.retain(|m| {
            i += 1;
            if at_count.get(&m.rva).copied().unwrap_or(0) < 2 {
                return true;
            }
            if picked.contains(&(i - 1)) {
                paired += 1;
                return true;
            }
            icf += 1;
            gone.insert(m.name.clone());
            false
        });
        for (m, fate) in f.iter_mut() {
            if *fate == Fate::Own && gone.contains(&m.name) {
                *fate = Fate::Icf;
            }
        }
    }
    lines.push(format!(
        "  {dup} SELECT_ANY member(s) dropped as later copies of an earlier .dsp TU's; {paired} ICF-folded \
         rva(s) counted once in the object whose span holds them, {icf} other name(s) at a folded rva dropped"
    ));
    let scores: Vec<Scoring> = objects
        .iter()
        .map(|(_, v)| score_of(&v.iter().collect::<Vec<_>>()))
        .collect();
    let paths: Vec<PathBuf> = seated_objs.iter().map(|so| so.path.clone()).collect();
    let fossils = if incremental {
        let ev = evidence(&objects, &paths).unwrap_or_else(|e| {
            lines.push(format!(
                "  warning: no archive evidence ({e:#}), so the pull-order and drain rules name no fossil"
            ));
            LinkEvidence::default()
        });
        let n: usize = objects.iter().map(|(_, v)| v.len()).sum();
        let bud = Budget::per_item(n, FOSSIL_STEPS_PER_MEMBER, FOSSIL_STEP_FLOOR);
        let f = ilink_fossils(&objects, &scores, &ev, &bud);
        if let Some(why) = bud.over() {
            lines.push(format!(
                "  warning: the pull-order rule stopped at {why} ({} steps over {n} member(s)): fossils after \
                 that point are not named",
                bud.spent()
            ));
        }
        f
    } else {
        vec![BTreeMap::new(); objects.len()]
    };
    let (mut on, mut ob, mut en, mut eb, mut an) = (0usize, 0u32, 0usize, 0u32, 0usize);
    let (mut fnn, mut fb) = (0usize, 0u32);
    let mut summary: Vec<(u32, String)> = Vec::new();
    let mut tsv = format!("{ORDER_HEADER}\n");
    for ((((tu, v), f), s), fos) in objects.iter().zip(&full).zip(&scores).zip(&fossils) {
        let seq: Vec<&Member> = v.iter().collect();
        let ends = end_drain(v);
        let e = score_of(&ends);
        let owed: Vec<usize> = s
            .order
            .iter()
            .copied()
            .filter(|i| !fos.contains_key(i))
            .collect();
        let fossil: Vec<usize> = fos.keys().copied().collect();
        on += owed.len();
        ob = ob.saturating_add(bytes(&seq, &owed));
        fnn += fossil.len();
        fb = fb.saturating_add(bytes(&seq, &fossil));
        en += e.order.len();
        eb = eb.saturating_add(bytes(&ends, &e.order));
        an += s.away.len();
        tsv.push_str(&format!(
            "{tu}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n",
            v.len(),
            owed.len(),
            bytes(&seq, &owed),
            fossil.len(),
            bytes(&seq, &fossil),
            e.order.len(),
            bytes(&ends, &e.order),
            s.away.len()
        ));
        let wanted = o.show.iter().any(|x| x.eq_ignore_ascii_case(tu));
        if s.order.is_empty() && e.order.is_empty() && !wanted && !o.all {
            continue;
        }
        let fossil_note = if fossil.is_empty() {
            String::new()
        } else {
            format!(
                " (+ {}/{} B ILINK fossil)",
                fossil.len(),
                bytes(&seq, &fossil)
            )
        };
        summary.push((
            bytes(&seq, &owed),
            format!(
                "  {tu}: {} own, ORDER {}/{} B{fossil_note}, end-drain model {}/{} B, away {}",
                v.len(),
                owed.len(),
                bytes(&seq, &owed),
                e.order.len(),
                bytes(&ends, &e.order),
                s.away.len()
            ),
        ));
        if !wanted {
            continue;
        }
        lines.push(format!(
            "  OBJECT {tu}: span {:#x}..{:#x}, {} own, ORDER {}/{} B{fossil_note} (end-drain model {}/{} B)",
            s.lo,
            s.hi,
            v.len(),
            owed.len(),
            bytes(&seq, &owed),
            e.order.len(),
            bytes(&ends, &e.order)
        ));
        let dp = drain_points(v, s.lo, s.hi);
        let mut own = v.iter().zip(&dp).enumerate();
        for (fm, fate) in f {
            if *fate != Fate::Own {
                if o.dups {
                    let why = match fate {
                        Fate::Dup(t) => format!("DUP<{t}"),
                        _ => "ICF".to_string(),
                    };
                    lines.push(format!(
                        "      - {:#08x} {:5} {why} {}",
                        fm.rva,
                        fm.kind.tag(),
                        fm.name
                    ));
                }
                continue;
            }
            let Some((i, (m, (ours, refp)))) = own.next() else {
                break;
            };
            let tag = if s.run.contains(&i) {
                "run"
            } else if s.away.contains(&i) {
                "AWAY"
            } else if fos.contains_key(&i) {
                "FOSL"
            } else {
                "OUT"
            };
            let drain = if m.kind == Kind::User || tag == "AWAY" {
                String::new()
            } else if ours == refp {
                format!("  drain after {}", short(ours.as_ref()))
            } else {
                format!(
                    "  DRAIN ours after {} | ref after {}",
                    short(ours.as_ref()),
                    short(refp.as_ref())
                )
            };
            let ilt = fos
                .get(&i)
                .map_or_else(String::new, |x| format!("  ILINK: {}", x.why()));
            lines.push(format!(
                "    {i:3} {:#08x} {:5} {:4} {}{drain}{ilt}",
                m.rva,
                m.kind.tag(),
                tag,
                m.name
            ));
        }
    }
    summary.sort_by(|a, b| b.0.cmp(&a.0).then_with(|| a.1.cmp(&b.1)));
    lines.push(format!(
        "  TOTAL over {} object(s): ORDER {on} bodies / {ob} B owed (object sizes; {fnn} / {fb} B more are ILINK \
         fossils); end-drain model {en} / {eb} B; away {an}",
        objects.len()
    ));
    lines.extend(summary.into_iter().map(|(_, l)| l));
    tsv.push_str(&format!(
        "TOTAL\t{}\t{on}\t{ob}\t{fnn}\t{fb}\t{en}\t{eb}\t{an}\n",
        objects.iter().map(|(_, v)| v.len()).sum::<usize>()
    ));
    out.tsv = tsv;
    if o.ilink {
        ilink_screen(&objects, &scores, &fossils, incremental, lines);
    }
    out.paths = paths;
    out.objects = objects;
    out.scores = scores;
    out.fossils = fossils;
    Ok(out)
}

fn ilink_screen(
    objects: &[(String, Vec<Member>)],
    scores: &[Scoring],
    fossils: &[BTreeMap<usize, Fossil>],
    incremental: bool,
    lines: &mut Vec<String>,
) {
    if !incremental {
        lines.push(
            "  ILINK SCREEN: FULL LINK -- the reference has no ILT, so no body was placed by a relink and every \
             ORDER body is OWED."
                .to_string(),
        );
    }
    let (mut owed, mut fossil) = (0usize, 0usize);
    for (((tu, v), s), fos) in objects.iter().zip(scores).zip(fossils) {
        let mut rows: Vec<usize> = s.order.clone();
        rows.sort_by_key(|&i| v.get(i).map_or(0, |m| m.rva));
        for i in rows {
            let Some(m) = v.get(i) else {
                continue;
            };
            let verdict = fos.get(&i).map_or_else(
                || {
                    owed += 1;
                    "OWED".to_string()
                },
                |f| {
                    fossil += 1;
                    format!("FOSSIL ({})", f.why())
                },
            );
            lines.push(format!(
                "  ILINK {tu} {:#08x} {:5} {}: {verdict}",
                m.rva,
                m.kind.tag(),
                m.name
            ));
        }
    }
    lines.push(format!(
        "  ILINK SCREEN: {owed} ORDER body(ies) OWED, {fossil} ILINK FOSSIL(s)"
    ));
}

pub const SMALL_INLINE_LEN: u32 = 0x20;

pub const SHARED_DEFINERS: usize = 2;

fn member_stem(member: &str) -> String {
    member
        .rsplit(['\\', '/'])
        .next()
        .unwrap_or(member)
        .to_ascii_lowercase()
}

#[derive(Debug, Clone)]
pub struct Archived {
    pub name: String,
    pub path: PathBuf,
}

pub fn link_evidence(
    env: &Env,
    objects: &[(String, Vec<Member>)],
    paths: &[PathBuf],
    recorded: &[msvc6_match::locate::Recorded],
    archives: &[Archived],
) -> Result<LinkEvidence> {
    let mut definer: BTreeMap<String, (String, String)> = BTreeMap::new();
    let mut stem_archive: BTreeMap<String, String> = BTreeMap::new();
    let mut comdats: BTreeMap<(String, String), Vec<comdat::Comdat>> = BTreeMap::new();
    let mut definers: BTreeMap<(String, String), BTreeSet<String>> = BTreeMap::new();
    for a in archives {
        let data = comdat::read(&a.path)?;
        let arch = msvc6_image::archive::Archive::parse(&data)?;
        let astem = a
            .path
            .file_stem()
            .map_or_else(String::new, |s| s.to_string_lossy().to_lowercase());
        for (m, body) in arch.content() {
            if m.kind != msvc6_image::archive::MemberKind::Object {
                continue;
            }
            let stem = member_stem(&m.name);
            stem_archive
                .entry(stem.clone())
                .or_insert_with(|| a.name.clone());
            for (_, _, d) in comdat::defined_externals(body).unwrap_or_default() {
                definer
                    .entry(d)
                    .or_insert_with(|| (a.name.clone(), stem.clone()));
            }
            let cs = comdat::text_of(body).unwrap_or_default();
            for c in &cs {
                if let (true, Some(s)) = (c.comdat, &c.symbol) {
                    definers
                        .entry((astem.clone(), s.clone()))
                        .or_default()
                        .insert(stem.clone());
                }
            }
            comdats.entry((astem.clone(), stem)).or_default().extend(cs);
        }
    }
    if definer.is_empty() {
        bail!("no archive symbol could be read");
    }
    let text = env.text();
    let base = env.text_rva;
    let shared: BTreeSet<u32> = recorded
        .iter()
        .filter(|m| m.len > 0)
        .filter(|m| {
            let arch = m.archive.to_lowercase();
            let Some(cs) = comdats.get(&(arch.clone(), member_stem(&m.member))) else {
                return false;
            };
            let Some(off) = m
                .rva
                .checked_sub(base)
                .and_then(|o| usize::try_from(o).ok())
            else {
                return false;
            };
            let Some(window) = usize::try_from(m.len)
                .ok()
                .and_then(|n| text.get(off..off + n))
            else {
                return false;
            };
            cs.iter().any(|c| {
                c.bytes.len() == window.len()
                    && c.bytes
                        .iter()
                        .zip(&c.relocated)
                        .zip(window)
                        .all(|((b, masked), a)| *masked || a == b)
                    && c.symbol.as_ref().is_some_and(|s| {
                        definers
                            .get(&(arch.clone(), s.clone()))
                            .is_some_and(|d| d.len() >= SHARED_DEFINERS)
                    })
            })
        })
        .map(|m| m.rva)
        .collect();
    let mut key: BTreeMap<String, (Option<u32>, u32)> = BTreeMap::new();
    for m in recorded.iter().filter(|m| m.len > 0) {
        let stem = member_stem(&m.member);
        if !stem.ends_with(".obj") {
            continue;
        }
        let contested = m.len < SMALL_INLINE_LEN || shared.contains(&m.rva);
        let firm = (!contested).then_some(m.rva);
        let e = key.entry(stem).or_insert((firm, m.rva));
        e.1 = e.1.min(m.rva);
        e.0 = match (e.0, firm) {
            (Some(a), Some(b)) => Some(a.min(b)),
            (a, b) => a.or(b),
        };
    }
    let mut seq: Vec<(u32, String)> = key
        .into_iter()
        .map(|(stem, (firm, first))| (firm.unwrap_or(first), stem))
        .collect();
    seq.sort();
    let members: Vec<(String, String)> = seq
        .into_iter()
        .map(|(_, stem)| {
            let a = stem_archive.get(&stem).cloned().unwrap_or_default();
            (stem, a)
        })
        .collect();
    let at: BTreeMap<&str, usize> = members
        .iter()
        .enumerate()
        .rev()
        .map(|(p, (stem, _))| (stem.as_str(), p))
        .collect();
    let mut bodies = Vec::with_capacity(objects.len());
    for ((_, v), path) in objects.iter().zip(paths) {
        let data = comdat::read(path)?;
        let mut cs = comdat::text_of(&data)?;
        cs.sort_by_key(|c| c.index);
        let rows = cs
            .iter()
            .map(|c| {
                let own = c
                    .symbol
                    .as_deref()
                    .and_then(|n| v.iter().position(|m| m.name == n));
                let refs = c
                    .relocations
                    .iter()
                    .filter(|r| r.external)
                    .filter_map(|r| definer.get(&r.symbol))
                    .filter_map(|(_, stem)| at.get(stem.as_str()).copied())
                    .collect();
                (own, refs)
            })
            .collect();
        bodies.push(rows);
    }
    Ok(LinkEvidence {
        bodies,
        members,
        definer,
    })
}
