use super::names::{MapRow, class_key};
use super::pair::Side;
use super::side::{Ours, SINIT, Source, SourceKind};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug)]
pub struct FileInputs<'a> {
    pub rows: &'a [MapRow],
    pub sources: &'a [Source],
    pub sides: &'a [Side],
    pub ours: &'a Side,
    pub info: &'a Ours,
    pub prior: &'a [MapRow],
    pub tracked: &'a [String],
    pub scope: &'a [String],
    pub system_headers: &'a BTreeSet<String>,
    pub image: &'a str,
}

struct Run {
    name: String,
    last: Option<String>,
}

struct Table {
    label: String,
    runs: Vec<Run>,
    at: BTreeMap<String, (usize, usize, Option<String>)>,
}

fn in_scope(scope: &[String], f: &str) -> bool {
    scope.iter().any(|s| f.contains(s.as_str()))
}

fn source_file(scope: &[String], f: &str) -> bool {
    in_scope(scope, f) && f.ends_with(".cpp")
}

fn trusted(r: &MapRow) -> bool {
    (r.kind == "method" || r.kind == "function")
        && !r.mac.is_empty()
        && (r.state == "APPLIED" || r.reason == "byte-cost" || r.reason == "attests-ours")
}

fn class_of(side: &Side, i: usize) -> Option<String> {
    side.funs
        .get(i)
        .and_then(|f| f.sig.as_ref())
        .and_then(|s| s.class().map(class_key))
}

fn tail_kind(side: &Side, i: usize) -> bool {
    side.funs.get(i).is_some_and(|f| {
        f.raw.starts_with(SINIT)
            || f.sig.as_ref().is_some_and(|s| {
                s.class().is_some_and(|c| c.contains('<'))
                    || s.name == "{ctor}"
                    || s.name == "{dtor}"
            })
    })
}

fn spans_of(side: &Side) -> Vec<(usize, usize, String)> {
    let n = side.funs.len();
    let mut spans = Vec::new();
    let mut k = 0usize;
    while k < n {
        let Some(run) = side
            .funs
            .get(k)
            .map(|f| f.file.clone())
            .filter(|r| r.starts_with("run:"))
        else {
            k = k.saturating_add(1);
            continue;
        };
        let mut e = k;
        while side.funs.get(e).is_some_and(|f| f.file == run) {
            e = e.saturating_add(1);
        }
        spans.push((k, e, run.trim_start_matches("run:").to_string()));
        k = e;
    }
    spans
}

fn homes(side: &Side, spans: &[(usize, usize, String)]) -> BTreeMap<String, (usize, usize)> {
    let mut count: BTreeMap<(String, usize), usize> = BTreeMap::new();
    for (ri, (k, e, _)) in spans.iter().enumerate() {
        for i in *k..*e {
            if let Some(c) = class_of(side, i).filter(|_| !tail_kind(side, i)) {
                let x = count.entry((c, ri)).or_insert(0);
                *x = x.saturating_add(1);
            }
        }
    }
    let mut home: BTreeMap<String, (usize, usize)> = BTreeMap::new();
    for ((c, ri), n) in &count {
        let e = home.entry(c.clone()).or_insert((*ri, *n));
        if *n > e.1 || (*n == e.1 && *ri < e.0) {
            *e = (*ri, *n);
        }
    }
    home
}

fn last_class(
    side: &Side,
    span: (usize, usize),
    ri: usize,
    home: &BTreeMap<String, (usize, usize)>,
) -> Option<String> {
    let (k, e) = span;
    let mut i = e;
    while i > k {
        i -= 1;
        if tail_kind(side, i) {
            continue;
        }
        let cn = class_of(side, i)?;
        let other = |j: usize| !tail_kind(side, j) && class_of(side, j).is_some_and(|x| x != cn);
        let own = |j: usize| !tail_kind(side, j) && class_of(side, j).as_ref() == Some(&cn);
        let mut b = i;
        while b > k && !other(b - 1) {
            b -= 1;
        }
        let size = (b..=i).filter(|j| own(*j)).count();
        let (mut earlier, mut run) = (0usize, 0usize);
        for j in k..b {
            if other(j) {
                earlier = earlier.max(run);
                run = 0;
            } else if own(j) {
                run = run.saturating_add(1);
            }
        }
        earlier = earlier.max(run);
        let away = home.get(&cn).is_some_and(|h| h.0 != ri);
        if away || earlier > size {
            i = b;
            continue;
        }
        return Some(cn);
    }
    None
}

fn tables(sources: &[Source], sides: &[Side]) -> Vec<Table> {
    let mut order: Vec<usize> = (0..sources.len()).collect();
    order.sort_by_key(|i| sources.get(*i).map_or(u32::MAX, |s| s.rank));
    let mut out = Vec::new();
    for si in order {
        let (Some(src), Some(side)) = (sources.get(si), sides.get(si)) else {
            continue;
        };
        if src.kind != SourceKind::Pef {
            continue;
        }
        let spans = spans_of(side);
        let home = homes(side, &spans);
        let mut t = Table {
            label: src.label.clone(),
            runs: Vec::new(),
            at: BTreeMap::new(),
        };
        for (ri, (k, e, name)) in spans.iter().enumerate() {
            t.runs.push(Run {
                name: name.clone(),
                last: last_class(side, (*k, *e), ri, &home),
            });
            for i in *k..*e {
                if let Some(s) = side.funs.get(i).and_then(|f| f.sig.as_ref()) {
                    t.at.insert(s.key(), (ri, i, class_of(side, i)));
                }
            }
        }
        out.push(t);
    }
    out
}

struct Cand {
    file: String,
    new: String,
    label: String,
    pairs: usize,
    state: &'static str,
    reason: &'static str,
    detail: String,
    evidence: String,
}

fn judge(
    file: &str,
    rs: &[&MapRow],
    tables: &[Table],
    hits_all: &[Vec<(String, usize, usize)>],
) -> Cand {
    let mut c = Cand {
        file: file.to_string(),
        new: String::new(),
        label: String::new(),
        pairs: 0,
        state: "RECORDED",
        reason: "no-twin",
        detail: if rs.is_empty() {
            "no function of the file is paired".into()
        } else {
            "no paired function lies in a PEF build's attested run".into()
        },
        evidence: String::new(),
    };
    for (ti, t) in tables.iter().enumerate() {
        let hits: Vec<&(usize, usize, Option<String>)> =
            rs.iter().filter_map(|r| t.at.get(&r.mac)).collect();
        if hits.is_empty() {
            continue;
        }
        c.label.clone_from(&t.label);
        c.pairs = hits.len();
        let mut per_run: BTreeMap<usize, usize> = BTreeMap::new();
        for h in &hits {
            let e = per_run.entry(h.0).or_insert(0);
            *e = e.saturating_add(1);
        }
        let run_name = |ri: &usize| t.runs.get(*ri).map_or("?", |r| r.name.as_str()).to_string();
        c.evidence = format!(
            "{}: {} of {} pair(s) in {}",
            t.label,
            hits.len(),
            rs.len(),
            per_run
                .iter()
                .map(|(ri, n)| format!("{} ({n})", run_name(ri)))
                .collect::<Vec<_>>()
                .join(", ")
        );
        c.reason = "analogy-only";
        if per_run.len() > 1 {
            c.detail = "the membership differs: the file's pairs fall in more than one run".into();
            return c;
        }
        let Some((&ri, _)) = per_run.iter().next() else {
            return c;
        };
        let Some(run) = t.runs.get(ri) else { return c };
        let first = hits.iter().map(|h| h.1).min().unwrap_or(0);
        let after: BTreeSet<&str> = hits_all
            .get(ti)
            .into_iter()
            .flatten()
            .filter(|(f, r, p)| f != file && *r == ri && *p > first)
            .map(|(f, _, _)| f.rsplit('/').next().unwrap_or(f))
            .collect();
        if !after.is_empty() {
            c.detail = format!(
                "not the run's last file: {} pair(s) follow this file's first in {}'s run",
                after.iter().copied().collect::<Vec<_>>().join(", "),
                run.name
            );
            return c;
        }
        let classes: BTreeSet<Option<String>> = hits.iter().map(|h| h.2.clone()).collect();
        if !classes.contains(&run.last) {
            c.detail = format!(
                "the run's last class ({}) is not this file's ({})",
                run.last.as_deref().unwrap_or("a free function"),
                classes
                    .iter()
                    .map(|x| x.as_deref().unwrap_or("free"))
                    .collect::<Vec<_>>()
                    .join(", ")
            );
            return c;
        }
        let dir = file.rsplit_once('/').map_or("", |x| x.0);
        c.new = format!("{dir}/{}", run.name);
        c.state = "APPLIED";
        c.reason = "evidence";
        let (stem, ext) = run
            .name
            .rsplit_once('.')
            .map_or((run.name.as_str(), "cpp"), |(s, e)| (s, e));
        c.detail = format!(
            "{SINIT}{stem}_{ext} closes the run; the file's pairs end it (last class {})",
            run.last.as_deref().unwrap_or("free")
        );
        return c;
    }
    c
}

#[must_use]
pub fn file_rows(x: &FileInputs<'_>) -> Vec<MapRow> {
    let tables = tables(x.sources, x.sides);
    let file_of: BTreeMap<String, String> = x
        .ours
        .funs
        .iter()
        .zip(x.info.in_scope.iter())
        .filter(|(_, s)| **s)
        .map(|(f, _)| (format!("{:#x}", f.va), f.file.clone()))
        .collect();
    let mut by_file: BTreeMap<String, Vec<&MapRow>> = BTreeMap::new();
    for f in x.tracked.iter().filter(|f| source_file(x.scope, f)) {
        by_file.entry(f.clone()).or_default();
    }
    for r in x.rows.iter().filter(|r| trusted(r)) {
        if let Some(file) = file_of.get(&r.va).filter(|f| source_file(x.scope, f)) {
            by_file.entry(file.clone()).or_default().push(r);
        }
    }
    let hits_all: Vec<Vec<(String, usize, usize)>> = tables
        .iter()
        .map(|t| {
            by_file
                .iter()
                .flat_map(|(f, rs)| {
                    rs.iter()
                        .filter_map(|r| t.at.get(&r.mac).map(|h| (f.clone(), h.0, h.1)))
                })
                .collect()
        })
        .collect();
    let mut cands: Vec<Cand> = by_file
        .iter()
        .map(|(file, rs)| judge(file, rs, &tables, &hits_all))
        .collect();
    let mut owner: BTreeMap<String, (String, usize)> = BTreeMap::new();
    for c in cands.iter().filter(|c| c.state == "APPLIED") {
        let e = owner
            .entry(c.new.to_ascii_lowercase())
            .or_insert((c.file.clone(), c.pairs));
        if c.pairs > e.1 {
            *e = (c.file.clone(), c.pairs);
        }
    }
    let lower: BTreeSet<String> = x.tracked.iter().map(|f| f.to_ascii_lowercase()).collect();
    for c in cands.iter_mut().filter(|c| c.state == "APPLIED") {
        if let Some((f, n)) = owner
            .get(&c.new.to_ascii_lowercase())
            .filter(|(f, _)| *f != c.file)
        {
            c.state = "RECORDED";
            c.reason = "analogy-only";
            c.detail = format!(
                "{} is {}'s ({n} pair(s) to {}): one object takes the name",
                c.new.rsplit('/').next().unwrap_or(""),
                f.rsplit('/').next().unwrap_or(f),
                c.pairs
            );
            c.new.clear();
        } else if c.new == c.file {
            c.reason = "already";
        } else if !c.new.eq_ignore_ascii_case(&c.file)
            && lower.contains(&c.new.to_ascii_lowercase())
        {
            c.state = "RECORDED";
            c.reason = "name-clash";
            c.detail = format!("{} is another tracked file", c.new);
        }
    }
    let class_ren: BTreeMap<&str, &MapRow> = x
        .rows
        .iter()
        .filter(|r| r.kind == "class" && r.state == "APPLIED" && r.reason == "evidence")
        .map(|r| (r.old.as_str(), r))
        .collect();
    let mut out: Vec<MapRow> = Vec::new();
    for f in x
        .tracked
        .iter()
        .filter(|f| in_scope(x.scope, f) && (f.ends_with(".cpp") || f.ends_with(".h")))
    {
        let (dir, base) = f.rsplit_once('/').unwrap_or(("", f.as_str()));
        let Some((stem, ext)) = base.rsplit_once('.') else {
            continue;
        };
        let Some(cr) = class_ren.get(stem) else {
            continue;
        };
        if cands
            .iter()
            .any(|c| c.file == *f && c.state == "APPLIED" && c.reason == "evidence")
        {
            continue;
        }
        let new = format!("{dir}/{}.{ext}", cr.new);
        let mut row = MapRow {
            kind: "file".into(),
            image: x.image.to_string(),
            old: f.clone(),
            new: new.clone(),
            source: cr.source.clone(),
            evidence: format!("class {} -> {} ({})", cr.old, cr.new, cr.reason),
            state: "APPLIED".into(),
            reason: "follows-class".into(),
            detail: format!(
                "OURS: named after its class {}, now {}; the file name is not attested",
                cr.old, cr.new
            ),
            ..MapRow::default()
        };
        let header = format!("{}.{ext}", cr.new).to_ascii_lowercase();
        if lower.contains(&new.to_ascii_lowercase()) {
            row.state = "RECORDED".into();
            row.reason = "name-clash".into();
            row.detail = format!("{new} is another tracked file");
        } else if ext == "h" && x.system_headers.contains(&header) {
            row.state = "RECORDED".into();
            row.reason = "name-clash".into();
            row.detail = format!(
                "{}.h would shadow the toolchain's {} (/I is searched first; the build ignores case)",
                cr.new,
                header.to_ascii_uppercase()
            );
        }
        out.push(row);
    }
    for c in cands {
        out.push(MapRow {
            kind: "file".into(),
            state: c.state.into(),
            reason: c.reason.into(),
            image: x.image.to_string(),
            old: c.file,
            new: c.new,
            source: c.label,
            evidence: c.evidence,
            detail: c.detail,
            ..MapRow::default()
        });
    }
    for p in x
        .prior
        .iter()
        .filter(|p| p.kind == "file" && p.state == "APPLIED" && p.old != p.new && !p.new.is_empty())
    {
        if !x.tracked.contains(&p.new) || x.tracked.contains(&p.old) {
            continue;
        }
        out.retain(|r| r.old != p.new && r.old != p.old);
        out.extend(
            x.prior
                .iter()
                .filter(|q| q.kind == "file" && q.old == p.old)
                .cloned(),
        );
    }
    out.sort_by(|a, b| a.old.cmp(&b.old));
    out
}
