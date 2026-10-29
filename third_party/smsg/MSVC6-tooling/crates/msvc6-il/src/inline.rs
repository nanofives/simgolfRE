use crate::grammar::Grammar;
use crate::sym::Sym;
use crate::tree::{Call, Trees};
use crate::unit::{FFLAG_CANDIDATE, FFLAG_COMPILE, FFLAG_DTOR_OBJECTS, FFLAG_TRY, Files, Unit};
use std::collections::{BTreeMap, HashMap};
use std::fmt::Write as _;

pub const FREE_CB: i64 = 40;
pub const RUNNING_CAP: i64 = 35000;
pub const BUDGET_FLOOR: i64 = 1000;
pub const MAX_DEPTH: usize = 64;
pub const MAX_STEPS: u64 = 1 << 22;

#[must_use]
pub fn root_budget(cb: i64) -> i64 {
    cb.saturating_mul(2).clamp(BUDGET_FLOOR, RUNNING_CAP)
}

#[must_use]
pub fn expands(cb: i64, budget: i64, running: i64) -> bool {
    (cb <= FREE_CB || cb <= budget) && running <= RUNNING_CAP
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Verdict {
    Expand,
    Free,
    Over,
    Cap,
    Try,
    EhStates,
    Recursive,
    Deep,
    Unread,
}

impl Verdict {
    #[must_use]
    pub fn word(self) -> &'static str {
        match self {
            Self::Expand => "EXPAND",
            Self::Free => "FREE",
            Self::Over => "CALL over budget",
            Self::Cap => "CALL running total past 35000",
            Self::Try => "CALL try block in the callee",
            Self::EhStates => "CALL callee EH states under a root with none",
            Self::Recursive => "CALL recursive",
            Self::Deep => "CALL deeper than the plan walks",
            Self::Unread => "CALL body unread",
        }
    }

    #[must_use]
    pub fn expanded(self) -> bool {
        matches!(self, Self::Expand | Self::Free)
    }

    #[must_use]
    pub fn is_site(self) -> bool {
        !matches!(self, Self::Try | Self::EhStates)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Site {
    pub callee: u32,
    pub name: String,
    pub cb: i64,
    pub left: i64,
    pub remain: i64,
    pub running: i64,
    pub verdict: Verdict,
    pub share: i64,
    pub inner: Vec<Site>,
    pub calls: Vec<u32>,
    pub cleanups: Vec<u32>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Walk {
    pub root: u32,
    pub name: String,
    pub cb: i64,
    pub budget: i64,
    pub sites: Vec<Site>,
    pub calls: Vec<u32>,
    pub cleanups: Vec<u32>,
    pub spent: i64,
}

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Walked {
    pub sites: Vec<Site>,
    pub calls: Vec<u32>,
    pub cleanups: Vec<u32>,
    pub spent: i64,
}

#[derive(Debug, Clone)]
struct Body {
    calls: Vec<Call>,
}

#[derive(Debug)]
pub struct Planner<'a> {
    unit: &'a Unit,
    files: &'a Files,
    grammar: &'a Grammar,
    defs: HashMap<u32, &'a Sym>,
    bodies: HashMap<u32, Option<Body>>,
    pub errors: BTreeMap<String, String>,
    steps: u64,
    pub out_of_steps: bool,
    label_refs: HashMap<u32, i64>,
}

impl<'a> Planner<'a> {
    #[must_use]
    pub fn new(unit: &'a Unit, files: &'a Files, grammar: &'a Grammar) -> Self {
        let defs = unit
            .by_id()
            .into_iter()
            .filter(|(_, s)| s.ex.is_some())
            .collect();
        Self {
            unit,
            files,
            grammar,
            defs,
            bodies: HashMap::new(),
            errors: BTreeMap::new(),
            steps: 0,
            out_of_steps: false,
            label_refs: unit
                .lists
                .values()
                .flatten()
                .filter(|s| s.class == crate::sym::CLASS_LABEL)
                .filter_map(|s| Some((s.id?, s.num("refs")?)))
                .collect(),
        }
    }

    #[must_use]
    pub fn def(&self, id: u32) -> Option<&'a Sym> {
        self.defs.get(&id).copied()
    }

    fn calls_of(&mut self, id: u32) -> Option<Vec<Call>> {
        if let Some(b) = self.bodies.get(&id) {
            return b.as_ref().map(|b| b.calls.clone());
        }
        let def = self.def(id)?;
        let body = match self.unit.body(self.files, self.grammar, def) {
            Ok(recs) => {
                let refs = &self.label_refs;
                let calls = body_calls(self.grammar, self.unit, &recs, &|l| {
                    refs.get(&l).copied().unwrap_or(1)
                });
                Some(Body { calls })
            }
            Err(e) => {
                self.errors.insert(def.name_text(), e.to_string());
                None
            }
        };
        let out = body.as_ref().map(|b| b.calls.clone());
        self.bodies.insert(id, body);
        out
    }

    #[must_use]
    pub fn candidate(&self, id: u32) -> bool {
        self.def(id)
            .is_some_and(|d| d.fflags() & FFLAG_CANDIDATE != 0)
    }

    fn cb(&self, id: u32) -> i64 {
        self.def(id).and_then(Sym::s6d).map_or(0, i64::from)
    }

    fn refusal(&self, root: u32, callee: u32) -> Option<Verdict> {
        let r = self.def(root).map_or(0, Sym::fflags);
        let c = self.def(callee).map_or(0, Sym::fflags);
        if c & FFLAG_TRY != 0 {
            Some(Verdict::Try)
        } else if c & FFLAG_DTOR_OBJECTS != 0 && r & (FFLAG_DTOR_OBJECTS | FFLAG_TRY) == 0 {
            Some(Verdict::EhStates)
        } else {
            None
        }
    }

    #[must_use]
    pub fn steps(&self) -> u64 {
        self.steps
    }

    fn tick(&mut self) -> bool {
        if self.steps >= MAX_STEPS {
            self.out_of_steps = true;
            return false;
        }
        self.steps = self.steps.saturating_add(1);
        true
    }

    fn walk_body(
        &mut self,
        root: u32,
        body: u32,
        init: i64,
        running: &mut i64,
        stack: &mut Vec<u32>,
    ) -> Walked {
        let calls = self.calls_of(body).unwrap_or_default();
        let mut direct = Vec::new();
        let mut cleanups = Vec::new();
        let mut cands: Vec<(u32, Option<Verdict>)> = Vec::new();
        for c in &calls {
            let Some(id) = c.callee.filter(|_| !c.slot && !c.intrinsic && !c.dead) else {
                continue;
            };
            if c.cleanup {
                cleanups.push(id);
            } else if self.candidate(id) {
                cands.push((id, self.refusal(root, id)));
            } else {
                direct.push(id);
            }
        }
        let n =
            i64::try_from(cands.iter().filter(|(_, r)| r.is_none()).count()).unwrap_or(i64::MAX);
        let mut k = 0i64;
        let mut left = init;
        let mut sites = Vec::new();
        for (id, refused) in cands {
            if !self.tick() {
                break;
            }
            let cb = self.cb(id);
            let name = self.unit.short(id);
            if let Some(v) = refused {
                sites.push(Site {
                    callee: id,
                    name,
                    cb,
                    left,
                    remain: 0,
                    running: *running,
                    verdict: v,
                    share: 0,
                    inner: Vec::new(),
                    calls: Vec::new(),
                    cleanups: Vec::new(),
                });
                direct.push(id);
                continue;
            }
            let remain = n.saturating_sub(k);
            k = k.saturating_add(1);
            let mut site = Site {
                callee: id,
                name,
                cb,
                left,
                remain,
                running: *running,
                verdict: Verdict::Over,
                share: 0,
                inner: Vec::new(),
                calls: Vec::new(),
                cleanups: Vec::new(),
            };
            if stack.contains(&id) {
                site.verdict = Verdict::Recursive;
            } else if stack.len() > MAX_DEPTH {
                site.verdict = Verdict::Deep;
            } else if !expands(cb, left, *running) {
                site.verdict = if *running > RUNNING_CAP {
                    Verdict::Cap
                } else {
                    Verdict::Over
                };
            } else if self.calls_of(id).is_none() {
                site.verdict = Verdict::Unread;
            } else {
                *running = running.saturating_add(cb);
                site.verdict = if cb > FREE_CB {
                    left = left.saturating_sub(cb);
                    Verdict::Expand
                } else {
                    Verdict::Free
                };
                let share = if remain > 0 { left / remain } else { 0 };
                site.share = share;
                stack.push(id);
                let w = self.walk_body(root, id, share, running, stack);
                stack.pop();
                left = left.saturating_sub(w.spent);
                site.inner = w.sites;
                site.calls = w.calls;
                site.cleanups = w.cleanups;
            }
            if !site.verdict.expanded() {
                direct.push(id);
            }
            sites.push(site);
        }
        Walked {
            sites,
            calls: direct,
            cleanups,
            spent: init.saturating_sub(left),
        }
    }

    pub fn walk(&mut self, root: u32) -> Option<Walk> {
        let def = self.def(root)?;
        let cb = self.cb(root);
        let budget = root_budget(cb);
        let mut running = cb;
        let mut stack = vec![root];
        let w = self.walk_body(root, root, budget, &mut running, &mut stack);
        Some(Walk {
            root,
            name: def.name_text(),
            cb,
            budget,
            sites: w.sites,
            calls: w.calls,
            cleanups: w.cleanups,
            spent: w.spent,
        })
    }

    #[must_use]
    pub fn roots(&self) -> Vec<u32> {
        let mut v: Vec<(u32, u32)> = self
            .defs
            .iter()
            .filter(|(_, d)| d.fflags() & FFLAG_COMPILE != 0)
            .filter_map(|(id, d)| Some((d.ex?, *id)))
            .collect();
        v.sort_unstable();
        v.into_iter().map(|(_, id)| id).collect()
    }
}

#[must_use]
pub fn called(w: &Walk) -> BTreeMap<u32, usize> {
    let mut m: BTreeMap<u32, usize> = BTreeMap::new();
    fn add(m: &mut BTreeMap<u32, usize>, calls: &[u32], sites: &[Site]) {
        for c in calls {
            *m.entry(*c).or_insert(0) += 1;
        }
        for s in sites {
            if s.verdict.expanded() {
                add(m, &s.calls, &s.inner);
            }
        }
    }
    add(&mut m, &w.calls, &w.sites);
    m
}

#[must_use]
pub fn cleanups(w: &Walk) -> BTreeMap<u32, usize> {
    let mut m: BTreeMap<u32, usize> = BTreeMap::new();
    fn add(m: &mut BTreeMap<u32, usize>, cleanups: &[u32], sites: &[Site]) {
        for c in cleanups {
            *m.entry(*c).or_insert(0) += 1;
        }
        for s in sites {
            if s.verdict.expanded() {
                add(m, &s.cleanups, &s.inner);
            }
        }
    }
    add(&mut m, &w.cleanups, &w.sites);
    m
}

#[must_use]
pub fn expanded(w: &Walk) -> BTreeMap<u32, usize> {
    let mut m: BTreeMap<u32, usize> = BTreeMap::new();
    fn add(m: &mut BTreeMap<u32, usize>, sites: &[Site]) {
        for s in sites {
            if s.verdict.expanded() {
                *m.entry(s.callee).or_insert(0) += 1;
                add(m, &s.inner);
            }
        }
    }
    add(&mut m, &w.sites);
    m
}

fn site_lines(out: &mut String, sites: &[Site], depth: usize) {
    for s in sites {
        let pad = "  ".repeat(depth.saturating_add(2));
        let _ = writeln!(
            out,
            "{pad}{} cb {} left {} remain {} running {}: {}{}",
            s.name,
            s.cb,
            s.left,
            s.remain,
            s.running,
            s.verdict.word(),
            if s.verdict.expanded() {
                format!(" (share {})", s.share)
            } else {
                String::new()
            }
        );
        site_lines(out, &s.inner, depth.saturating_add(1));
    }
}

#[must_use]
pub fn text(w: &Walk) -> String {
    let mut out = String::new();
    let _ = writeln!(
        out,
        "PLAN {} cb {} budget {} spent {}",
        w.name, w.cb, w.budget, w.spent
    );
    site_lines(&mut out, &w.sites, 0);
    out
}

pub const EH_UNWIND: u8 = 0x5d;
pub const EH_LEAVE: u8 = 0x5e;
pub const EH_KIND_NEW: i32 = 2;
pub const EH_POPS: i32 = 0x20;
pub const MAX_LIVE: usize = 4096;

#[must_use]
pub fn body_calls(
    g: &Grammar,
    names: &dyn crate::tree::Names,
    recs: &[crate::ex::Rec],
    label_refs: &dyn Fn(u32) -> i64,
) -> Vec<Call> {
    let mut t = Trees::new().calls_at_end();
    let mut calls: Vec<Call> = Vec::new();
    let mut cleanup = std::collections::BTreeSet::new();
    let mut live: Vec<(i32, Vec<Call>)> = Vec::new();
    let mut dead = false;
    for r in recs {
        let s = t.step(g, names, r);
        calls.extend(s.call.map(|mut c| {
            c.dead = dead;
            c
        }));
        if r.op == crate::ops::GOTO {
            dead = true;
        } else if r.op == crate::ops::LABEL && r.first_id().is_some_and(|l| label_refs(l) > 0) {
            dead = false;
        }
        if r.op == crate::tree::EH_LIVE {
            let kind = r.long().unwrap_or(0) & 7;
            let objs: Vec<Call> = calls
                .iter()
                .filter(|c| s.cleanup.contains(&c.index))
                .copied()
                .collect();
            cleanup.extend(s.cleanup.iter().copied());
            if live.len() < MAX_LIVE {
                live.push((kind, objs));
            }
        } else if r.op == EH_UNWIND || r.op == EH_LEAVE {
            let mut it = r.args.iter().filter_map(|a| match a {
                crate::ex::Arg::Long(l) => Some(*l),
                _ => None,
            });
            let count = usize::try_from(it.next().unwrap_or(0)).unwrap_or(0);
            let flags = it.next().unwrap_or(0);
            let kind = flags & 7;
            let pops = flags & EH_POPS != 0;
            let destroys = kind != EH_KIND_NEW && (r.op == EH_LEAVE || pops);
            let from = live.len().saturating_sub(count);
            if destroys {
                let dying: Vec<Call> = live
                    .get(from..)
                    .unwrap_or_default()
                    .iter()
                    .rev()
                    .filter(|(k, _)| *k != EH_KIND_NEW)
                    .flat_map(|(_, cs)| cs.iter().copied())
                    .collect();
                for mut c in dying {
                    c.exit = true;
                    c.dead = dead;
                    c.at = r.off;
                    calls.push(c);
                }
            }
            if r.op == EH_UNWIND || pops {
                live.truncate(from);
            }
        }
    }
    for c in &mut calls {
        c.cleanup = !c.exit && cleanup.contains(&c.index);
    }
    calls
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Pair {
    pub root: String,
    pub callee: String,
    pub planned: usize,
    pub expanded: usize,
    pub listed: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize)]
pub struct Check {
    pub roots: usize,
    pub pairs: usize,
    pub agree: usize,
    pub sites: usize,
    pub expanded: usize,
    pub recursive: usize,
    pub diffs: Vec<Pair>,
    #[serde(skip)]
    pub walks: Vec<Walk>,
    pub notes: Vec<String>,
}

impl Check {
    pub fn add(&mut self, o: &Check) {
        self.roots = self.roots.saturating_add(o.roots);
        self.pairs = self.pairs.saturating_add(o.pairs);
        self.agree = self.agree.saturating_add(o.agree);
        self.sites = self.sites.saturating_add(o.sites);
        self.expanded = self.expanded.saturating_add(o.expanded);
        self.recursive = self.recursive.saturating_add(o.recursive);
    }

    #[must_use]
    pub fn line(&self) -> String {
        format!(
            "{} function(s), {} candidate pair(s), {} agree; {} site(s), {} expanded; {} self call(s) not judged",
            self.roots, self.pairs, self.agree, self.sites, self.expanded, self.recursive
        )
    }
}

fn c_plain(listed: &str) -> Option<&str> {
    let body = listed
        .strip_prefix('_')
        .or_else(|| listed.strip_prefix('@'))?;
    Some(match body.rsplit_once('@') {
        Some((name, n)) if !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()) => name,
        _ => body,
    })
}

#[must_use]
pub fn listed(by_name: &BTreeMap<String, u32>, name: &str) -> Option<u32> {
    by_name
        .get(name)
        .or_else(|| c_plain(name).and_then(|p| by_name.get(p)))
        .copied()
}

fn legacy_thunk(name: &str) -> bool {
    let Some((_, rest)) = name.split_once("@@") else {
        return false;
    };
    let mut b = rest.bytes();
    match b.next() {
        Some(b'G' | b'H' | b'O' | b'P' | b'W' | b'X') => true,
        Some(b'$') => matches!(b.next(), Some(b'0'..=b'5')),
        _ => false,
    }
}

#[must_use]
pub fn is_thunk(name: &str) -> bool {
    use msvc_demangler::{FuncClass, Type};
    if name.starts_with("??_9") {
        return true;
    }
    match msvc_demangler::parse(name) {
        Ok(r) => match r.symbol_type {
            Type::MemberFunction(fc, ..) => fc.contains(FuncClass::THUNK),
            Type::VCallThunk(..) => true,
            _ => false,
        },
        Err(e) => {
            let b = name.as_bytes();
            match e.offset() {
                Some(at) if at >= 2 && b.get(at - 2..at) == Some(b"@$".as_slice()) => {
                    matches!(b.get(at), Some(b'0'..=b'5'))
                }
                _ => legacy_thunk(name),
            }
        }
    }
}

fn count(sites: &[Site], all: &mut usize, exp: &mut usize) {
    for x in sites {
        if x.verdict.is_site() {
            *all = all.saturating_add(1);
        }
        if x.verdict.expanded() {
            *exp = exp.saturating_add(1);
            count(&x.inner, all, exp);
        }
    }
}

#[must_use]
pub fn check(
    unit: &Unit,
    files: &Files,
    grammar: &Grammar,
    listing: &str,
    only: &[String],
    keep_walks: bool,
) -> Check {
    let procs = crate::listing::procs(listing);
    let mut planner = Planner::new(unit, files, grammar);
    let mut t = Check::default();
    let by_name: BTreeMap<String, u32> = unit
        .by_id()
        .into_iter()
        .filter(|(_, s)| s.ex.is_some())
        .map(|(id, s)| (s.name_text(), id))
        .collect();
    for (name, p) in &procs {
        if (!only.is_empty() && !only.contains(name)) || is_thunk(name) {
            continue;
        }
        let Some(root) = listed(&by_name, name) else {
            continue;
        };
        let Some(w) = planner.walk(root) else {
            continue;
        };
        t.roots = t.roots.saturating_add(1);
        let plan = called(&w);
        let done = expanded(&w);
        count(&w.sites, &mut t.sites, &mut t.expanded);
        let mut cands: Vec<u32> = plan
            .keys()
            .chain(done.keys())
            .copied()
            .chain(
                p.calls
                    .iter()
                    .chain(p.jumps.iter())
                    .filter_map(|n| listed(&by_name, n)),
            )
            .filter(|c| planner.candidate(*c))
            .collect();
        cands.sort_unstable();
        cands.dedup();
        let mut differs = false;
        for c in cands {
            if c == root {
                t.recursive = t.recursive.saturating_add(1);
                continue;
            }
            let planned = plan.get(&c).copied().unwrap_or(0);
            let seen = p
                .calls
                .iter()
                .chain(p.jumps.iter())
                .filter(|x| listed(&by_name, x) == Some(c))
                .count();
            t.pairs = t.pairs.saturating_add(1);
            if planned == seen {
                t.agree = t.agree.saturating_add(1);
            } else {
                differs = true;
                t.diffs.push(Pair {
                    root: name.clone(),
                    callee: unit.short(c),
                    planned,
                    expanded: done.get(&c).copied().unwrap_or(0),
                    listed: seen,
                });
            }
        }
        if differs && keep_walks {
            t.walks.push(w);
        }
    }
    t.notes = planner
        .errors
        .iter()
        .map(|(f, e)| format!("{f}: {e}"))
        .collect();
    if planner.out_of_steps {
        t.notes
            .push(format!("the plan stopped at its {MAX_STEPS} site steps"));
    }
    t
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_root_budget_is_twice_its_size_within_the_floor_and_cap() {
        assert_eq!(root_budget(10), 1000);
        assert_eq!(root_budget(600), 1200);
        assert_eq!(root_budget(20000), 35000);
        assert_eq!(root_budget(-4841), 1000);
    }

    #[test]
    fn a_free_site_expands_whatever_the_budget_unless_the_total_is_spent() {
        assert!(expands(40, 0, 0));
        assert!(!expands(41, 40, 0));
        assert!(expands(41, 41, 35000));
        assert!(!expands(10, 1000, 35001));
    }
}
