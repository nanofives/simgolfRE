use crate::linkmap::Map;
use crate::memberorder::{self, member_stem};
use crate::objkey::Keys;
use crate::seq::lcs_len;
use crate::symbols;
use anyhow::{Result, bail};
use msvc6_image::coff::Object;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const MAX_SEARCH_OBJECTS: usize = 8;

pub const OBJECT_ROWS: usize = 60;

pub const LOAD_BUDGET: usize = 1 << 24;

pub const ENTRY_LABEL: &str = "(entry)";

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct LibMember {
    pub lib: usize,
    pub stem: String,
    pub defs: BTreeSet<String>,
    pub undefs: Vec<String>,
}

pub fn lib_member(lib: usize, stem: &str, obj: &[u8]) -> Result<LibMember> {
    let o = Object::parse(obj)?;
    let mut defs = symbols::defined_externals(&o);
    let mut undefs = symbols::undefined_ordered(&o);
    let weak: BTreeMap<String, String> = symbols::weak_defaults(&o).into_iter().collect();
    for (w, default) in weak {
        undefs.retain(|u| *u != w);
        defs.insert(w);
        if !defs.contains(&default) {
            undefs.push(default);
        }
    }
    Ok(LibMember {
        lib,
        stem: stem.to_ascii_lowercase(),
        defs,
        undefs,
    })
}

pub fn archive_members(lib: usize, archive: &[u8]) -> Result<Vec<LibMember>> {
    let a = msvc6_image::archive::Archive::parse(archive)?;
    let mut out = Vec::new();
    for (m, body) in a.content() {
        if body.len() <= memberorder::MIN_MEMBER || !msvc6_image::coff::looks_like_object(body) {
            continue;
        }
        if let Ok(x) = lib_member(lib, &member_stem(&m.name), body) {
            out.push(x);
        }
    }
    Ok(out)
}

#[derive(Debug, Clone, Default)]
pub struct Archives {
    pub definer: BTreeMap<String, String>,
}

pub fn definers(archives: &[&[u8]]) -> Result<Archives> {
    let mut definer = BTreeMap::new();
    for bytes in archives {
        let a = msvc6_image::archive::Archive::parse(bytes)?;
        for (m, body) in a.content() {
            if body.len() <= memberorder::MIN_MEMBER || !msvc6_image::coff::looks_like_object(body)
            {
                continue;
            }
            let Ok(o) = Object::parse(body) else { continue };
            let stem = member_stem(&m.name);
            for d in symbols::defined_externals(&o) {
                definer.entry(d).or_insert_with(|| stem.clone());
            }
        }
    }
    if definer.is_empty() {
        bail!("no archive symbol could be read, so nothing can be predicted");
    }
    Ok(Archives { definer })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Pull {
    pub member: String,
    pub name: String,
    pub object: String,
    pub index: usize,
}

#[must_use]
pub fn predict_traced(refs: &[(String, Vec<String>)], arch: &Archives) -> Vec<Pull> {
    let mut seen = BTreeSet::new();
    let mut out = Vec::new();
    for (object, syms) in refs {
        for (index, s) in syms.iter().enumerate() {
            if let Some(m) = arch.definer.get(s)
                && seen.insert(m.clone())
            {
                out.push(Pull {
                    member: m.clone(),
                    name: s.clone(),
                    object: object.clone(),
                    index,
                });
            }
        }
    }
    out
}

#[must_use]
pub fn predict(order: &[usize], refs: &[(String, Vec<String>)], arch: &Archives) -> Vec<String> {
    let mut seen = BTreeSet::new();
    let mut out = Vec::new();
    for &i in order {
        let Some((_, syms)) = refs.get(i) else {
            continue;
        };
        for s in syms {
            if let Some(m) = arch.definer.get(s)
                && seen.insert(m.clone())
            {
                out.push(m.clone());
            }
        }
    }
    out
}

#[must_use]
pub fn map_object_order(map: &Map, own: &Keys) -> Vec<String> {
    let code = map.code_sections();
    let mut out: Vec<String> = Vec::new();
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        let owner = own.map_owner(&r.obj);
        if owner.to_ascii_lowercase().ends_with(".obj")
            && !owner.contains(':')
            && !out.iter().any(|o| o == owner)
        {
            out.push(owner.to_string());
        }
    }
    out
}

#[must_use]
pub fn first_unfolded(rows: &[(String, u32)]) -> Vec<(String, u32)> {
    let mut held: BTreeMap<u32, usize> = BTreeMap::new();
    for (_, a) in rows {
        *held.entry(*a).or_insert(0) += 1;
    }
    let mut first: BTreeMap<String, u32> = BTreeMap::new();
    for (m, a) in rows {
        if held.get(a).copied().unwrap_or(0) > 1 {
            continue;
        }
        let e = first.entry(m.clone()).or_insert(*a);
        *e = (*e).min(*a);
    }
    let mut v: Vec<(String, u32)> = first.into_iter().collect();
    v.sort_by_key(|(m, a)| (*a, m.clone()));
    v
}

#[must_use]
pub fn own_member_order(
    map: &Map,
    own: &Keys,
    any: &BTreeMap<String, BTreeSet<String>>,
) -> Vec<(String, u32)> {
    let code = map.code_sections();
    let base = if map.load == 0 { 0x0040_0000 } else { map.load };
    let mut rows: Vec<(String, u32)> = Vec::new();
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        let member = own.map_owner(&r.obj);
        if member == r.obj {
            continue;
        }
        let stem = member.to_ascii_lowercase();
        if any.get(&stem).is_some_and(|s| s.contains(&r.name)) {
            continue;
        }
        rows.push((stem, r.va.wrapping_sub(base)));
    }
    first_unfolded(&rows)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Load {
    pub stem: String,
    pub lib: usize,
    pub name: String,
    pub origin: String,
    pub index: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Closure {
    pub loads: Vec<Load>,
    pub exhausted: bool,
}

#[must_use]
pub fn closure(
    objs: &[(String, Vec<String>)],
    obj_defs: &BTreeSet<String>,
    members: &[LibMember],
    eager: &BTreeSet<String>,
) -> Closure {
    closure_within(objs, obj_defs, members, eager, LOAD_BUDGET)
}

#[must_use]
pub fn closure_within(
    objs: &[(String, Vec<String>)],
    obj_defs: &BTreeSet<String>,
    members: &[LibMember],
    eager: &BTreeSet<String>,
    budget: usize,
) -> Closure {
    let nlibs = members.iter().map(|m| m.lib + 1).max().unwrap_or(0);
    let mut definer: Vec<BTreeMap<&str, usize>> = vec![BTreeMap::new(); nlibs];
    for (i, m) in members.iter().enumerate() {
        if let Some(d) = definer.get_mut(m.lib) {
            for s in &m.defs {
                d.entry(s.as_str()).or_insert(i);
            }
        }
    }
    let mut resolved: BTreeSet<String> = obj_defs.clone();
    let mut listed: BTreeSet<String> = BTreeSet::new();
    let mut list: Vec<(String, String, usize)> = Vec::new();
    for (object, syms) in objs {
        for (index, s) in syms.iter().enumerate() {
            if !resolved.contains(s) && listed.insert(s.clone()) {
                list.push((s.clone(), object.clone(), index));
            }
        }
    }
    let mut loaded: BTreeSet<usize> = BTreeSet::new();
    let mut out = Closure::default();
    let mut steps = 0usize;
    loop {
        let mut changed = false;
        for d in &definer {
            let mut i = 0;
            while let Some((s, origin, index)) = list.get(i) {
                steps += 1;
                if steps > budget {
                    out.exhausted = true;
                    return out;
                }
                let hit = if resolved.contains(s) {
                    None
                } else {
                    d.get(s.as_str()).copied()
                };
                if let Some(mi) = hit.filter(|mi| loaded.insert(*mi))
                    && let Some(m) = members.get(mi)
                {
                    out.loads.push(Load {
                        stem: m.stem.clone(),
                        lib: m.lib,
                        name: s.clone(),
                        origin: origin.clone(),
                        index: *index,
                    });
                    resolved.extend(m.defs.iter().cloned());
                    let fresh: Vec<(String, String, usize)> = m
                        .undefs
                        .iter()
                        .enumerate()
                        .filter(|(_, u)| !resolved.contains(*u) && listed.insert((*u).clone()))
                        .map(|(k, u)| (u.clone(), m.stem.clone(), k))
                        .collect();
                    if eager.contains(&m.stem) {
                        let at = (i + 1).min(list.len());
                        list.splice(at..at, fresh);
                    } else {
                        list.extend(fresh);
                    }
                    changed = true;
                }
                i += 1;
            }
        }
        if !changed {
            break;
        }
    }
    out
}

#[must_use]
pub fn perms(n: usize) -> Vec<Vec<usize>> {
    fn go(k: usize, cur: &mut Vec<usize>, out: &mut Vec<Vec<usize>>) {
        if k == cur.len() {
            out.push(cur.clone());
            return;
        }
        for i in k..cur.len() {
            cur.swap(k, i);
            go(k + 1, cur, out);
            cur.swap(k, i);
        }
    }
    let mut out = Vec::new();
    if n > MAX_SEARCH_OBJECTS {
        return out;
    }
    let mut cur: Vec<usize> = (0..n).collect();
    go(0, &mut cur, &mut out);
    out
}

#[must_use]
pub fn default_entry(dll: bool, subsystem: u16) -> &'static str {
    if dll {
        "__DllMainCRTStartup@12"
    } else if subsystem == 3 {
        "_mainCRTStartup"
    } else {
        "_WinMainCRTStartup"
    }
}

#[must_use]
pub fn with_entry(
    mut refs: Vec<(String, Vec<String>)>,
    cmdline: &BTreeSet<String>,
    entry: &str,
) -> Vec<(String, Vec<String>)> {
    let at = refs
        .iter()
        .rposition(|(n, _)| cmdline.contains(&n.to_ascii_lowercase()))
        .map_or(refs.len(), |i| i + 1);
    refs.insert(at, (ENTRY_LABEL.to_string(), vec![entry.to_string()]));
    refs
}

fn pct(n: usize, of: usize) -> f64 {
    f64::from(u32::try_from(n).unwrap_or(u32::MAX)) * 100.0
        / f64::from(u32::try_from(of.max(1)).unwrap_or(u32::MAX))
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Search {
    pub orders: usize,
    pub steerable: usize,
    pub current: usize,
    pub best: Vec<(usize, Vec<String>)>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Direct {
    pub objects: usize,
    pub names: usize,
    pub reference: usize,
    pub direct: usize,
    pub overlap: usize,
    pub control: Option<(usize, usize, usize)>,
    pub search: Option<Search>,
}

pub struct DirectInputs<'a> {
    pub label: &'a str,
    pub objdir: &'a str,
    pub refs: &'a [(String, Vec<String>)],
    pub arch: &'a Archives,
    pub refseq: &'a [String],
    pub actual: &'a [String],
    pub trace: bool,
    pub objects: &'a [String],
}

impl std::fmt::Debug for DirectInputs<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DirectInputs")
            .field("label", &self.label)
            .field("refs", &self.refs.len())
            .finish_non_exhaustive()
    }
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn direct(i: &DirectInputs<'_>) -> (Direct, Vec<String>) {
    let ours_now = predict(&(0..i.refs.len()).collect::<Vec<_>>(), i.refs, i.arch);
    let direct: BTreeSet<&String> = ours_now.iter().collect();
    let overlap: Vec<String> = i
        .refseq
        .iter()
        .filter(|m| direct.contains(m))
        .cloned()
        .collect();
    let mut d = Direct {
        objects: i.refs.len(),
        names: i.refs.iter().map(|(_, s)| s.len()).sum(),
        reference: i.refseq.len(),
        direct: ours_now.len(),
        overlap: overlap.len(),
        ..Direct::default()
    };
    let mut out = vec![
        format!(
            "{}: PREDICTED ARCHIVE PULL ORDER from our objects' symbol references",
            i.label
        ),
        format!(
            "  {} object(s) read from {}, referencing {} distinct archive-defined name(s)",
            d.objects, i.objdir, d.names
        ),
        String::new(),
        "  STEERING POWER first, because it bounds every figure below:".to_string(),
        format!(
            "    members the reference places                {:>4}",
            d.reference
        ),
        format!(
            "    members OUR objects pull DIRECTLY            {:>4}",
            d.direct
        ),
        format!(
            "    of those, also placed in the reference       {:>4}   <- the only ones any object order can move",
            d.overlap
        ),
    ];
    if d.overlap < 8 {
        out.push(format!(
            "  THE SUBSET IS TOO SMALL TO STEER BY: an object order can rearrange at most {} member(s).",
            d.overlap
        ));
        return (d, out);
    }
    let actual_direct: Vec<String> = i
        .actual
        .iter()
        .filter(|m| direct.contains(m))
        .cloned()
        .collect();
    let control = lcs_len(&ours_now, &actual_direct);
    d.control = Some((ours_now.len(), actual_direct.len(), control));
    out.push(String::new());
    out.push(
        "  CONTROL -- the predictor against the REAL link it models, on the directly-pulled subset:".to_string(),
    );
    out.push(format!(
        "    predicted {:>4}   actual {:>4}   agreeing subsequence {:>4}  ({:.0}%)",
        ours_now.len(),
        actual_direct.len(),
        control,
        pct(control, ours_now.len())
    ));
    if control * 4 < ours_now.len() * 3 {
        out.push(
            "    THE PREDICTOR DOES NOT MODEL OUR OWN LINK (below 75%): no search result is printed.".to_string(),
        );
        return (d, out);
    }
    if i.trace {
        out.push(String::new());
        out.push(
            "  TRACE -- each directly-pulled member, the name and object that pull it (index = position among that \
             object's undefined externals), and the member's position in the reference:"
                .to_string(),
        );
        out.push(format!(
            "    {:>3}  {:<14} {:>4}  {:<24} {:>4}  name",
            "pos", "member", "ref", "object", "idx"
        ));
        let at: BTreeMap<&String, usize> = i
            .refseq
            .iter()
            .enumerate()
            .rev()
            .map(|(k, m)| (m, k + 1))
            .collect();
        for (k, p) in predict_traced(i.refs, i.arch).iter().enumerate() {
            out.push(format!(
                "    {:>3}  {:<14} {:>4}  {:<24} {:>4}  {}",
                k + 1,
                p.member,
                at.get(&p.member)
                    .map_or_else(|| "-".to_string(), ToString::to_string),
                p.object,
                p.index,
                p.name
            ));
        }
    }
    for want in i.objects {
        let Some((name, syms)) = i.refs.iter().find(|(n, _)| {
            n.eq_ignore_ascii_case(want) || n.eq_ignore_ascii_case(&format!("{want}.obj"))
        }) else {
            out.push(String::new());
            out.push(format!("  OBJECT {want}: not among the objects read"));
            continue;
        };
        out.push(String::new());
        out.push(format!(
            "  OBJECT {name}: its {} undefined external(s) in symbol-table order; `->' marks an archive member's name",
            syms.len()
        ));
        for (k, s) in syms.iter().enumerate() {
            match i.arch.definer.get(s) {
                Some(m) => out.push(format!("    {k:>4}  -> {m:<14} {s}")),
                None => out.push(format!("    {k:>4}     {:<14} {s}", "")),
            }
        }
    }
    if i.refs.len() > MAX_SEARCH_OBJECTS {
        out.push(String::new());
        out.push(format!(
            "  SEARCH SKIPPED: {} objects is {}! orders, past the {MAX_SEARCH_OBJECTS}-object enumeration.",
            i.refs.len(),
            i.refs.len()
        ));
        return (d, out);
    }
    let in_overlap: BTreeSet<&String> = overlap.iter().collect();
    let score = |seq: Vec<String>| {
        let s: Vec<String> = seq.into_iter().filter(|m| in_overlap.contains(m)).collect();
        lcs_len(&s, &overlap)
    };
    let mut best: Vec<(usize, Vec<usize>)> = perms(i.refs.len())
        .into_iter()
        .map(|p| (score(predict(&p, i.refs, i.arch)), p))
        .collect();
    best.sort_by_key(|(s, _)| std::cmp::Reverse(*s));
    let current = score(ours_now.clone());
    let named = |p: &[usize]| -> Vec<String> {
        p.iter()
            .filter_map(|k| i.refs.get(*k).map(|(n, _)| n.clone()))
            .collect()
    };
    let search = Search {
        orders: best.len(),
        steerable: overlap.len(),
        current,
        best: best.iter().take(4).map(|(s, p)| (*s, named(p))).collect(),
    };
    out.push(String::new());
    out.push(format!(
        "  SEARCH over {} object order(s), scored on the {} steerable member(s):",
        search.orders, search.steerable
    ));
    out.push(format!("    current order  {current} of {}", overlap.len()));
    out.push(format!(
        "    best found     {} of {}",
        best.first().map_or(0, |b| b.0),
        overlap.len()
    ));
    for (s, names) in &search.best {
        out.push(format!("      {s:>3}  {}", names.join(" ")));
    }
    out.push(
        "  A winner is a candidate to link, never a byte claim: link it and measure it with `placement member-order`."
            .to_string(),
    );
    d.search = Some(search);
    (d, out)
}

pub struct ClosureInputs<'a> {
    pub label: &'a str,
    pub map: &'a str,
    pub refs: &'a [(String, Vec<String>)],
    pub obj_defs: &'a BTreeSet<String>,
    pub members: &'a [LibMember],
    pub eager: &'a BTreeSet<String>,
    pub refseq: &'a [String],
    pub actual: &'a [String],
    pub window: Option<(usize, usize)>,
}

impl std::fmt::Debug for ClosureInputs<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ClosureInputs")
            .field("label", &self.label)
            .field("members", &self.members.len())
            .finish_non_exhaustive()
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct ClosureReport {
    pub predicted: usize,
    pub map: usize,
    pub common: usize,
    pub control: usize,
    pub identical: bool,
    pub map_only: Vec<String>,
    pub model_only: Vec<String>,
    pub plain: Option<(usize, usize)>,
    pub eager: Option<(usize, usize)>,
    pub exhausted: bool,
}

fn only(a: &[String], b: &[String]) -> Vec<String> {
    let sb: BTreeSet<&String> = b.iter().collect();
    a.iter().filter(|m| !sb.contains(m)).cloned().collect()
}

#[must_use]
pub fn closure_report(i: &ClosureInputs<'_>) -> (ClosureReport, Vec<String>) {
    let none = BTreeSet::new();
    let plain_c = closure(i.refs, i.obj_defs, i.members, &none);
    let plain: Vec<String> = plain_c.loads.iter().map(|l| l.stem.clone()).collect();
    let (pa, aa) = memberorder::common_pair(&plain, i.actual);
    let control = lcs_len(&pa, &aa);
    let mut r = ClosureReport {
        predicted: plain.len(),
        map: i.actual.len(),
        common: pa.len(),
        control,
        identical: pa == aa,
        map_only: only(i.actual, &plain),
        model_only: only(&plain, i.actual),
        exhausted: plain_c.exhausted,
        ..ClosureReport::default()
    };
    let mut out = vec![
        format!(
            "{}: LINK LIBRARY-SEARCH CLOSURE over {} object(s) and {} archive member(s)",
            i.label,
            i.refs.len(),
            i.members.len()
        ),
        String::new(),
        format!(
            "  CONTROL -- the plain model against OUR real link ({}):",
            i.map
        ),
        format!(
            "    predicted {} member(s), map {}, common {}, agreeing subsequence {} ({})",
            r.predicted,
            r.map,
            r.common,
            r.control,
            if r.identical {
                "IDENTICAL order"
            } else {
                "NOT identical"
            }
        ),
        format!(
            "    map-only   {:>3}  {}",
            r.map_only.len(),
            r.map_only.join(" ")
        ),
        format!(
            "    model-only {:>3}  {}",
            r.model_only.len(),
            r.model_only.join(" ")
        ),
    ];
    if plain_c.exhausted {
        out.push(format!(
            "    THE MODEL STOPPED after {LOAD_BUDGET} list steps: the figures above cover the loads before it."
        ));
    }
    if control * 20 < pa.len() * 19 {
        out.push(
            "    THE MODEL DOES NOT REPRODUCE OUR OWN LINK (below 95%), so nothing it says about the reference is \
             scored."
                .to_string(),
        );
        return (r, out);
    }
    let (ps, pr) = memberorder::common_pair(&plain, i.refseq);
    let pl = lcs_len(&ps, &pr);
    r.plain = Some((pl, pr.len()));
    out.push(String::new());
    out.push("  AGAINST THE REFERENCE:".to_string());
    out.push(format!(
        "    plain model                LCS {pl} of {}",
        pr.len()
    ));
    let (shown_s, shown_r, label) = if i.eager.is_empty() {
        (ps, pr, "model")
    } else {
        let hyp: Vec<String> = closure(i.refs, i.obj_defs, i.members, i.eager)
            .loads
            .into_iter()
            .map(|l| l.stem)
            .collect();
        let (hs, hr) = memberorder::common_pair(&hyp, i.refseq);
        let hl = lcs_len(&hs, &hr);
        r.eager = Some((hl, hr.len()));
        out.push(format!(
            "    eager {:20} LCS {hl} of {}   ({:+})",
            i.eager.iter().cloned().collect::<Vec<_>>().join(","),
            hr.len(),
            i64::try_from(hl).unwrap_or(i64::MAX) - i64::try_from(pl).unwrap_or(i64::MAX)
        ));
        (hs, hr, "eager")
    };
    if let Some((lo, hi)) = i.window {
        out.push(String::new());
        out.push(format!(
            "  WINDOW {}..{hi}: reference against the {label} model (the other side's position in brackets):",
            lo + 1
        ));
        out.extend(memberorder::window_lines(
            &memberorder::window_rows(&shown_r, &shown_s, lo, hi),
            "reference",
            "model",
        ));
    }
    out.push(String::new());
    out.push(
        "  A model, not a link: a hypothesis that scores here is a reason to compile a source variant and link it."
            .to_string(),
    );
    (r, out)
}

pub struct OwnInputs<'a> {
    pub label: &'a str,
    pub refs: &'a [(String, Vec<String>)],
    pub obj_defs: &'a BTreeSet<String>,
    pub members: &'a [LibMember],
    pub ours_seat: &'a [(String, u32)],
    pub ref_seat: &'a [(String, u32)],
    pub own_count: usize,
    pub trace: bool,
    pub objects: &'a [String],
}

impl std::fmt::Debug for OwnInputs<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("OwnInputs")
            .field("label", &self.label)
            .field("members", &self.members.len())
            .finish_non_exhaustive()
    }
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn own_library_report(i: &OwnInputs<'_>) -> Vec<String> {
    let loads: Vec<Load> = closure(i.refs, i.obj_defs, i.members, &BTreeSet::new())
        .loads
        .into_iter()
        .filter(|l| l.lib == 0)
        .collect();
    let model: Vec<String> = loads.iter().map(|l| l.stem.clone()).collect();
    let ours: Vec<String> = i.ours_seat.iter().map(|(m, _)| m.clone()).collect();
    let reference: Vec<String> = i.ref_seat.iter().map(|(m, _)| m.clone()).collect();
    let pair = |a: &[String], b: &[String]| {
        let (x, y) = memberorder::common_pair(a, b);
        (lcs_len(&x, &y), x.len(), x == y)
    };
    let (mo, mo_n, mo_same) = pair(&model, &ours);
    let (mr, mr_n, _) = pair(&model, &reference);
    let (or, or_n, _) = pair(&ours, &reference);
    let ours_at: BTreeMap<&str, u32> = i.ours_seat.iter().map(|(m, a)| (m.as_str(), *a)).collect();
    let ref_at: BTreeMap<&str, u32> = i.ref_seat.iter().map(|(m, a)| (m.as_str(), *a)).collect();
    let shift =
        |m: &str| -> Option<i64> { Some(i64::from(*ours_at.get(m)?) - i64::from(*ref_at.get(m)?)) };
    let shifts: Vec<i64> = ours.iter().filter_map(|m| shift(m)).collect();
    let at_ref = shifts.iter().filter(|s| **s == 0).count();
    let mut by: BTreeMap<i64, usize> = BTreeMap::new();
    for s in &shifts {
        *by.entry(*s).or_insert(0) += 1;
    }
    let (mode, at_mode) = by
        .iter()
        .max_by_key(|(s, n)| (**n, std::cmp::Reverse(s.unsigned_abs())))
        .map_or((0, 0), |(s, n)| (*s, *n));
    let spread: u64 = shifts.iter().map(|s| (s - mode).unsigned_abs()).sum();
    let mut out = vec![
        format!(
            "{}: OWN-SOURCE LIBRARY SEARCH ({} member(s), searched before the CRT)",
            i.label, i.own_count
        ),
        format!(
            "  CONTROL  model against OUR link's /MAP     LCS {mo} of {mo_n}{}",
            if mo_same { "  (IDENTICAL order)" } else { "" }
        ),
        format!("  model against the reference             LCS {mr} of {mr_n}"),
        format!("  OUR link against the reference          LCS {or} of {or_n}"),
        format!(
            "  SEATS ({} seated both sides): at the reference rva {at_ref}, at the modal shift {mode:+} {at_mode}, \
             sum of |shift - mode| {spread}",
            shifts.len()
        ),
        String::new(),
    ];
    if i.trace {
        let pos = |v: &[String], m: &str| {
            v.iter()
                .position(|x| x == m)
                .map_or_else(|| "-".to_string(), |p| (p + 1).to_string())
        };
        out.push(format!(
            "  {:>3} {:22} {:>4} {:>4} {:>7}  loaded by (origin #index among its references)",
            "pos", "model", "ours", "ref", "shift"
        ));
        for (k, l) in loads.iter().enumerate() {
            out.push(format!(
                "  {:>3} {:22} {:>4} {:>4} {:>7}  {} ({} #{})",
                k + 1,
                l.stem,
                pos(&ours, &l.stem),
                pos(&reference, &l.stem),
                shift(&l.stem).map_or_else(|| "-".to_string(), |s| format!("{s:+}")),
                l.name,
                l.origin,
                l.index
            ));
        }
        out.push(String::new());
    }
    let definer: BTreeMap<&str, &str> = i
        .members
        .iter()
        .filter(|m| m.lib == 0)
        .flat_map(|m| m.defs.iter().map(move |d| (d.as_str(), m.stem.as_str())))
        .collect();
    for want in i.objects {
        let key = want.to_ascii_lowercase();
        if let Some(m) = i.members.iter().find(|m| m.lib == 0 && m.stem == key) {
            out.push(format!(
                "  WHO REFERENCES {}'s {} name(s), in list order:",
                m.stem,
                m.defs.len()
            ));
            let by_member = loads.iter().filter_map(|l| {
                i.members
                    .iter()
                    .find(|x| x.lib == 0 && x.stem == l.stem)
                    .map(|x| (x.stem.clone(), x.undefs.clone()))
            });
            let rows = i
                .refs
                .iter()
                .cloned()
                .chain(by_member)
                .flat_map(|(o, syms)| {
                    syms.into_iter()
                        .enumerate()
                        .filter(|(_, s)| m.defs.contains(s))
                        .map(move |(k, s)| (o.clone(), k, s))
                })
                .take(OBJECT_ROWS);
            for (o, k, s) in rows {
                out.push(format!("    {o:24} #{k:<4} {s}"));
            }
        } else if let Some((o, syms)) = i.refs.iter().find(|(o, _)| o.eq_ignore_ascii_case(want)) {
            out.push(format!(
                "  {o}: references an own-library member defines, in symbol-table order:"
            ));
            for (k, s) in syms.iter().enumerate() {
                if let Some(d) = definer.get(s.as_str()) {
                    out.push(format!("    #{k:<4} {s:60} {d}"));
                }
            }
        } else {
            out.push(format!(
                "  {want}: neither a command-line object nor an own-library member"
            ));
        }
        out.push(String::new());
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn v(xs: &[&str]) -> Vec<String> {
        xs.iter().map(|x| (*x).to_string()).collect()
    }

    #[test]
    fn every_order_is_enumerated_once_and_a_large_count_is_refused() {
        let p = perms(4);
        assert_eq!(p.len(), 24);
        assert_eq!(p.iter().collect::<BTreeSet<_>>().len(), 24);
        assert_eq!(perms(1).len(), 1);
        assert!(perms(MAX_SEARCH_OBJECTS + 1).is_empty());
    }

    #[test]
    fn the_first_reference_wins_and_each_member_appears_once() {
        let arch = Archives {
            definer: [("s1", "m1.obj"), ("s2", "m2.obj"), ("s3", "m1.obj")]
                .into_iter()
                .map(|(a, b)| (a.to_string(), b.to_string()))
                .collect(),
        };
        let refs = vec![
            ("A.obj".to_string(), v(&["s1", "s2"])),
            ("B.obj".to_string(), v(&["s3"])),
        ];
        assert_eq!(predict(&[0, 1], &refs, &arch), v(&["m1.obj", "m2.obj"]));
        assert_eq!(predict(&[1, 0], &refs, &arch), v(&["m1.obj", "m2.obj"]));
        let refs2 = vec![("A.obj".to_string(), v(&["unknown", "s2"]))];
        assert_eq!(predict(&[0], &refs2, &arch), v(&["m2.obj"]));
        let t = predict_traced(&refs, &arch);
        assert_eq!(t.len(), 2);
        assert_eq!((t[1].object.as_str(), t[1].index), ("A.obj", 1));
    }

    fn mem(lib: usize, stem: &str, defs: &[&str], undefs: &[&str]) -> LibMember {
        LibMember {
            lib,
            stem: stem.to_string(),
            defs: defs.iter().map(|d| (*d).to_string()).collect(),
            undefs: v(undefs),
        }
    }

    #[test]
    fn the_closure_appends_plainly_and_splices_eagerly() {
        let members = vec![
            mem(0, "a.obj", &["a"], &["x"]),
            mem(0, "b.obj", &["b"], &[]),
            mem(0, "x.obj", &["x"], &[]),
            mem(1, "late.obj", &["l"], &[]),
            mem(0, "own.obj", &["o"], &[]),
        ];
        let objs = vec![("T.obj".to_string(), v(&["l", "a", "b", "o"]))];
        let defs: BTreeSet<String> = ["o".to_string()].into_iter().collect();
        let stems = |c: Closure| -> Vec<String> { c.loads.into_iter().map(|l| l.stem).collect() };
        assert_eq!(
            stems(closure(&objs, &defs, &members, &BTreeSet::new())),
            v(&["a.obj", "b.obj", "x.obj", "late.obj"])
        );
        let eager: BTreeSet<String> = ["a.obj".to_string()].into_iter().collect();
        assert_eq!(
            stems(closure(&objs, &defs, &members, &eager)),
            v(&["a.obj", "x.obj", "b.obj", "late.obj"])
        );
    }

    #[test]
    fn a_seat_is_the_first_address_no_other_name_holds() {
        let rows = vec![
            ("a.obj".to_string(), 0x100),
            ("a.obj".to_string(), 0x40),
            ("b.obj".to_string(), 0x40),
            ("b.obj".to_string(), 0x200),
            ("c.obj".to_string(), 0x40),
        ];
        assert_eq!(
            first_unfolded(&rows),
            vec![("a.obj".to_string(), 0x100), ("b.obj".to_string(), 0x200)]
        );
    }

    #[test]
    fn the_entry_follows_the_last_command_line_object() {
        let refs = vec![
            ("A.obj".to_string(), v(&["x"])),
            ("B.obj".to_string(), v(&["y"])),
            ("lib1.obj".to_string(), v(&["z"])),
        ];
        let cmd: BTreeSet<String> = ["a.obj".to_string(), "b.obj".to_string()]
            .into_iter()
            .collect();
        let r = with_entry(refs, &cmd, default_entry(true, 2));
        assert_eq!(r[2].0, ENTRY_LABEL);
        assert_eq!(r[2].1, v(&["__DllMainCRTStartup@12"]));
        assert_eq!(default_entry(false, 3), "_mainCRTStartup");
        assert_eq!(default_entry(false, 2), "_WinMainCRTStartup");
    }

    #[test]
    fn a_flooded_list_stops_at_its_budget() {
        let members: Vec<LibMember> = (0..4)
            .map(|k| LibMember {
                lib: k,
                ..mem(0, &format!("m{k}.obj"), &[&format!("d{k}")], &[])
            })
            .collect();
        let syms: Vec<String> = (0..1000).map(|k| format!("u{k}")).collect();
        let objs = vec![("T.obj".to_string(), syms)];
        let none = BTreeSet::new();
        let c = closure_within(&objs, &none, &members, &none, 4000);
        assert!(!c.exhausted && c.loads.is_empty());
        let c = closure_within(&objs, &none, &members, &none, 3999);
        assert!(c.exhausted);
    }
}
