use crate::cursor::Cursor;
use crate::ex;
use crate::grammar::Grammar;
use crate::ops;
use crate::sym::{self, Sym};
use crate::unit::{Files, Stream, Unit, stream_of};
use anyhow::{Context as _, Result, bail};
use std::collections::{BTreeMap, HashMap, HashSet};
use std::fmt::Write as _;

pub const MAX_SPAN: u32 = 1 << 22;

pub const BREAKDOWN_PER_ID: usize = 4;

pub const BREAKDOWN_FLOOR: usize = 1 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IdInfo {
    pub kind: String,
    pub name: String,
    pub owner: Option<u32>,
    pub level: Option<u8>,
    pub in_gl: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Func {
    pub id: u32,
    pub name: String,
    pub ex_off: usize,
    pub s6d: i64,
    pub recs: usize,
    pub ex_bytes: usize,
    pub firsts: Vec<u32>,
    pub ctemps: Vec<u32>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Ids {
    pub next_id: u32,
    pub ids: BTreeMap<u32, IdInfo>,
    pub funcs: Vec<Func>,
}

fn kind_of(s: &Sym, in_list: bool) -> String {
    let b30 = s.num("b30");
    let flags = s.flags();
    match (s.class, b30, s.level) {
        (sym::CLASS_GVAR, Some(0x25), _) => "literal".to_string(),
        (sym::CLASS_GVAR, _, _) if flags & 0x400 != 0 => "literal".to_string(),
        (sym::CLASS_VAR | sym::CLASS_GVAR, Some(0x24), _) => {
            format!("{} static", sym::class_name(s.class))
        }
        (sym::CLASS_VAR, _, Some(1)) if in_list => "formal".to_string(),
        (sym::CLASS_VAR, _, _) if in_list => "local".to_string(),
        _ => sym::class_name(s.class).to_string(),
    }
}

pub fn load(files: &Files, g: &Grammar) -> Result<Ids> {
    let u = Unit::read(files)?;
    load_unit(files, &u, g)
}

fn tally(f: &mut Func, seen: &mut HashSet<u32>, r: &ex::Rec) {
    if r.op != ops::PRAGMA {
        f.recs = f.recs.saturating_add(1);
    }
    f.ex_bytes = f.ex_bytes.saturating_add(r.end.saturating_sub(r.off));
    for id in r.ids() {
        if seen.insert(id) {
            f.firsts.push(id);
        }
        if r.op == ops::CTEMP && !f.ctemps.contains(&id) {
            f.ctemps.push(id);
        }
    }
}

pub fn load_unit(files: &Files, u: &Unit, g: &Grammar) -> Result<Ids> {
    let mut ids: BTreeMap<u32, IdInfo> = BTreeMap::new();
    let mut fdefs: Vec<&Sym> = Vec::new();
    for s in &u.globals {
        if let Some(id) = s.id {
            ids.insert(
                id,
                IdInfo {
                    kind: kind_of(s, false),
                    name: s.name_text(),
                    owner: None,
                    level: s.level,
                    in_gl: true,
                },
            );
        }
        if s.ex.is_some() {
            fdefs.push(s);
        }
    }
    for s in u.lists.values().flatten() {
        if s.ex.is_some() {
            fdefs.push(s);
        }
    }
    fdefs.sort_by_key(|s| s.ex);
    let mut funcs: Vec<Func> = Vec::new();
    for f in &fdefs {
        let sy_off = f.sy.and_then(|s| usize::try_from(s).ok()).unwrap_or(0);
        let fid = f.id.unwrap_or(0);
        if stream_of(f) == Stream::Ex
            && let Some(list) = u.lists.get(&sy_off)
        {
            for s in list {
                if let Some(id) = s.id {
                    ids.entry(id).or_insert(IdInfo {
                        kind: kind_of(s, true),
                        name: s.name_text(),
                        owner: Some(fid),
                        level: s.level,
                        in_gl: false,
                    });
                }
            }
        }
        funcs.push(Func {
            id: fid,
            name: f.name_text(),
            ex_off: f.ex.and_then(|o| usize::try_from(o).ok()).unwrap_or(0),
            s6d: f.num("s6d").unwrap_or(-1),
            recs: 0,
            ex_bytes: 0,
            firsts: Vec::new(),
            ctemps: Vec::new(),
        });
    }
    let starts: HashMap<usize, usize> = funcs
        .iter()
        .enumerate()
        .zip(fdefs.iter())
        .filter(|(_, f)| stream_of(f) == Stream::Ex)
        .map(|((i, f), _)| (f.ex_off, i))
        .collect();
    let ex_data = &files.ex;
    let mut c = Cursor::new(ex_data, 0).with_ids(u.header.ids);
    let dlp = u.dlp();
    if let Some(at) = u.body_start(ex_data, g) {
        c.seek(at);
    }
    let mut cur: Option<usize> = None;
    let mut seen: Vec<HashSet<u32>> = vec![HashSet::new(); funcs.len()];
    while !c.at_end() {
        if let Some(&i) = starts.get(&c.pos()) {
            cur = Some(i);
        }
        let r = ex::read(&mut c, g, dlp).with_context(|| format!("{}ex", files.base))?;
        if let Some(i) = cur
            && let (Some(f), Some(s)) = (funcs.get_mut(i), seen.get_mut(i))
        {
            tally(f, s, &r);
            if r.ends_body() {
                cur = None;
            }
        }
        if r.op == ops::EOF {
            break;
        }
    }
    if files.pch.is_some() {
        for (i, d) in fdefs.iter().enumerate() {
            if stream_of(d) != Stream::Pch {
                continue;
            }
            let body = u
                .body(files, g, d)
                .with_context(|| format!("{}: {}'s body in the pch", files.base, d.name_text()))?;
            if let (Some(f), Some(s)) = (funcs.get_mut(i), seen.get_mut(i)) {
                for r in &body {
                    tally(f, s, r);
                }
            }
        }
    }
    for f in &funcs {
        for &id in &f.ctemps {
            ids.entry(id).or_insert(IdInfo {
                kind: "ctemp".into(),
                name: String::new(),
                owner: Some(f.id),
                level: None,
                in_gl: false,
            });
        }
        for &id in &f.firsts {
            if let Some(info) = ids.get_mut(&id)
                && info.owner.is_none()
                && info.in_gl
                && id != f.id
            {
                info.owner = Some(f.id);
            }
        }
    }
    let first = ids.keys().next().copied().unwrap_or(u.header.next_id);
    if u.header.next_id.saturating_sub(first) > MAX_SPAN {
        bail!(
            "{}gl: ids #{first:x}..#{:x}, more than the {MAX_SPAN} ids this reader lists",
            files.base,
            u.header.next_id
        );
    }
    Ok(Ids {
        next_id: u.header.next_id,
        ids,
        funcs,
    })
}

fn gl_runs(u: &Ids) -> BTreeMap<u32, u32> {
    let mut out = BTreeMap::new();
    let mut run: Option<(u32, u32)> = None;
    for (&k, i) in &u.ids {
        run = match run {
            Some((s, e)) if i.in_gl && e.checked_add(1) == Some(k) => Some((s, k)),
            Some((s, e)) => {
                out.insert(s, e);
                i.in_gl.then_some((k, k))
            }
            None => i.in_gl.then_some((k, k)),
        };
    }
    if let Some((s, e)) = run {
        out.insert(s, e);
    }
    out
}

fn all_in_gl(runs: &BTreeMap<u32, u32>, from: u32, to: u32) -> bool {
    from >= to
        || runs
            .range(..=from)
            .next_back()
            .is_some_and(|(_, &e)| e >= from && e >= to - 1)
}

#[must_use]
pub fn starts(u: &Ids) -> Vec<u32> {
    let runs = gl_runs(u);
    let mut own: HashMap<u32, Vec<(u32, &IdInfo)>> = HashMap::new();
    for (k, i) in &u.ids {
        if let Some(o) = i.owner
            && !i.in_gl
        {
            own.entry(o).or_default().push((*k, i));
        }
    }
    let mut out = Vec::new();
    for f in &u.funcs {
        let mine = own.get(&f.id).map(Vec::as_slice).unwrap_or_default();
        let min_own = mine.iter().map(|(k, _)| *k).min();
        let formals: Vec<u32> = mine
            .iter()
            .filter(|(_, i)| i.kind == "formal" && i.name != "this")
            .map(|(k, _)| *k)
            .collect();
        let (min_formal, max_formal) =
            (formals.iter().copied().min(), formals.iter().copied().max());
        let this = mine
            .iter()
            .find(|(_, i)| i.kind == "formal" && i.name == "this")
            .map(|(k, _)| *k);
        let fresh = match (max_formal, min_own) {
            (Some(mf), _) => f.id > mf,
            (None, Some(m)) => m > f.id && all_in_gl(&runs, f.id.saturating_add(1), m),
            (None, None) => true,
        };
        let start = if fresh {
            min_own.map_or(f.id, |m| m.min(f.id))
        } else {
            match (min_formal, this) {
                (Some(m), _) => m,
                (None, Some(t)) if t > 0 && !u.ids.contains_key(&(t - 1)) => t - 1,
                (None, Some(t)) => t,
                _ => min_own.unwrap_or(f.id),
            }
        };
        out.push(start);
    }
    out
}

fn fname(u: &Ids, id: u32) -> String {
    u.funcs
        .iter()
        .find(|f| f.id == id)
        .map_or_else(|| format!("#{id:x}"), |f| f.name.clone())
}

#[must_use]
pub fn ids_text(u: &Ids) -> String {
    let mut out = String::new();
    let first = u.ids.keys().next().copied().unwrap_or(u.next_id);
    let _ = writeln!(
        out,
        "ids #{first:x}..#{:x} (next-id #{:x}); `-` = an id no record carries (a gap)",
        u.next_id.wrapping_sub(1),
        u.next_id
    );
    let mut firstref: HashMap<u32, (u32, usize)> = HashMap::new();
    for f in &u.funcs {
        for (k, id) in f.firsts.iter().enumerate() {
            firstref.entry(*id).or_insert((f.id, k));
        }
    }
    let st = starts(u);
    let mut at: HashMap<u32, usize> = HashMap::new();
    for (k, s) in st.iter().enumerate() {
        at.entry(*s).or_insert(k);
    }
    for id in first..u.next_id {
        if let Some(f) = at.get(&id).and_then(|&k| u.funcs.get(k)) {
            let _ = writeln!(out, "-- function {} #{:x} (start #{id:x})", f.name, f.id);
        }
        match u.ids.get(&id) {
            Some(i) => {
                let own = i
                    .owner
                    .map(|o| format!(" [{}]", fname(u, o)))
                    .unwrap_or_default();
                let lv = i.level.map(|l| format!(" level {l}")).unwrap_or_default();
                let fr = firstref
                    .get(&id)
                    .map(|(f, k)| format!("  first ref: {} #{k}", fname(u, *f)))
                    .unwrap_or_default();
                let _ = writeln!(
                    out,
                    "#{id:x}  {} {:<10} {:?}{lv}{own}{fr}",
                    if i.in_gl { "gl" } else { "  " },
                    i.kind,
                    i.name
                );
            }
            None => {
                let _ = writeln!(out, "#{id:x}  -");
            }
        }
    }
    out
}

#[must_use]
pub fn breakdown_budget(u: &Ids) -> usize {
    BREAKDOWN_FLOOR.max(u.ids.len().saturating_mul(BREAKDOWN_PER_ID))
}

#[must_use]
pub fn funcs_text(u: &Ids) -> String {
    let mut out = String::new();
    let st = starts(u);
    let budget = breakdown_budget(u);
    let mut spent = 0usize;
    let mut unread = 0usize;
    let _ = writeln!(
        out,
        "{:<5} {:<5} {:>4}  {:>5} {:>4}  {:<44} breakdown of the span (ids from start to the next start)",
        "start", "fn", "span", "s6d", "recs", "function"
    );
    for (k, f) in u.funcs.iter().enumerate() {
        let me = st.get(k).copied().unwrap_or(f.id);
        let next = st.get(k.saturating_add(1)).copied().unwrap_or(u.next_id);
        let span = i64::from(next) - i64::from(me);
        let mut by: BTreeMap<String, usize> = BTreeMap::new();
        let mut held = 0usize;
        let mut cut = false;
        if next > me {
            for (_, i) in u.ids.range(me..next) {
                if spent >= budget {
                    cut = true;
                    break;
                }
                spent = spent.saturating_add(1);
                held = held.saturating_add(1);
                *by.entry(i.kind.clone()).or_insert(0) += 1;
            }
        }
        let gaps = usize::try_from(span).unwrap_or(0).saturating_sub(held);
        let parts: Vec<String> = if cut {
            unread = unread.saturating_add(1);
            vec!["not read: the breakdown budget is spent".to_string()]
        } else {
            let mut p: Vec<String> = by.iter().map(|(k, n)| format!("{k} {n}")).collect();
            if gaps > 0 {
                p.push(format!("gap {gaps}"));
            }
            p
        };
        let eq = if i64::try_from(f.recs).is_ok_and(|r| r == f.s6d) {
            ""
        } else {
            " (!= s6d)"
        };
        let _ = writeln!(
            out,
            "#{:<4x} #{:<4x} {:>4}  {:>5} {:>4}{eq}  {:<44} {}",
            me,
            f.id,
            span,
            format!("0x{:x}", f.s6d),
            f.recs,
            f.name,
            parts.join(", ")
        );
    }
    let _ = writeln!(out, "next-id #{:x}", u.next_id);
    if unread > 0 {
        let _ = writeln!(
            out,
            "NOTE: {unread} span breakdown(s) not read: the spans overlap, and the breakdowns read at most \
             {budget} ids ({BREAKDOWN_PER_ID} an id the set holds, at least {BREAKDOWN_FLOOR})"
        );
    }
    out
}
