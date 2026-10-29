use super::pair::{self, Evidence, Pairs, Side};
use super::side::{Ours, SINIT, Source, SourceKind};
use crate::mangle::{Sig, SigLevel};
use anyhow::{Result, bail};
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::fmt::Write as _;

pub const MAP_HEAD: &str =
    "kind\tstate\treason\timage\tva\told\tnew\tsource\tmac\tmangled\ttwin\tsig\tevidence\tdetail";

pub const DIRECT_HEAD: &str = "mac_va\tmac\tour_va\tours\tround\tevidence\tsig\tdisp_j\tconst_j";

pub const RECORDED_REASONS: [&str; 6] = [
    "byte-cost",
    "attests-ours",
    "analogy-only",
    "no-twin",
    "supersedes",
    "out-of-scope",
];

#[must_use]
pub fn is_reason(r: &str) -> bool {
    !r.is_empty()
        && !matches!(r, "evidence" | "already")
        && r.chars()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '-')
}

pub const LAYOUT_ROUNDS: usize = 6;

pub const LAYOUT_GAP: usize = 64;

#[derive(Debug, Clone)]
pub struct Link {
    pub source: usize,
    pub mac: usize,
    pub path: String,
    pub kinds: BTreeSet<&'static str>,
    pub items: Vec<String>,
    pub sig: SigLevel,
    pub strong: bool,
}

#[must_use]
pub fn strong(ev: &Evidence) -> bool {
    let k = ev.kinds();
    k.contains("STR")
        || k.contains("FLT")
        || k.contains("BODY")
        || (ev.how == "anchor" && ev.items.len() >= 2)
}

#[must_use]
pub fn ev_text(ev: &Evidence) -> String {
    let mut items: Vec<String> = ev
        .items
        .iter()
        .map(|s| s.replace(['\t', '\n'], " "))
        .collect();
    items.sort();
    format!("{}{}:{:.1}:{}", ev.how, ev.round, ev.votes, items.join(","))
}

#[derive(Debug, Clone, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct MapRow {
    pub kind: String,
    pub state: String,
    pub reason: String,
    pub image: String,
    pub va: String,
    pub old: String,
    pub new: String,
    pub source: String,
    pub mac: String,
    pub mangled: String,
    pub twin: String,
    pub sig: String,
    pub evidence: String,
    pub detail: String,
}

impl MapRow {
    fn cells(&self) -> [&String; 14] {
        [
            &self.kind,
            &self.state,
            &self.reason,
            &self.image,
            &self.va,
            &self.old,
            &self.new,
            &self.source,
            &self.mac,
            &self.mangled,
            &self.twin,
            &self.sig,
            &self.evidence,
            &self.detail,
        ]
    }

    fn applied(&self) -> bool {
        self.state == "APPLIED"
    }

    fn function(&self) -> bool {
        self.kind == "method" || self.kind == "function"
    }
}

#[must_use]
pub fn cell(s: &str) -> String {
    let t = s.replace(['\t', '\n', '\r'], " ");
    if t.is_empty() { "-".to_string() } else { t }
}

#[must_use]
pub fn render_map(rows: &[MapRow], preamble: &str) -> String {
    let mut s = String::from(preamble);
    s.push_str(MAP_HEAD);
    s.push('\n');
    for r in rows {
        let line: Vec<String> = r.cells().iter().map(|c| cell(c)).collect();
        let _ = writeln!(s, "{}", line.join("\t"));
    }
    s
}

pub fn parse_map(text: &str) -> Result<Vec<MapRow>> {
    let mut out = Vec::new();
    for (n, l) in text.lines().enumerate() {
        if l.trim().is_empty() || l.starts_with('#') || l.starts_with("kind\t") {
            continue;
        }
        let c: Vec<String> = l
            .split('\t')
            .map(|x| {
                if x == "-" {
                    String::new()
                } else {
                    x.to_string()
                }
            })
            .collect();
        if c.len() < 14 {
            bail!(
                "map line {}: {} columns, want 14",
                n.saturating_add(1),
                c.len()
            );
        }
        let g = |i: usize| c.get(i).cloned().unwrap_or_default();
        out.push(MapRow {
            kind: g(0),
            state: g(1),
            reason: g(2),
            image: g(3),
            va: g(4),
            old: g(5),
            new: g(6),
            source: g(7),
            mac: g(8),
            mangled: g(9),
            twin: g(10),
            sig: g(11),
            evidence: g(12),
            detail: g(13),
        });
    }
    Ok(out)
}

#[must_use]
pub fn class_key(c: &str) -> String {
    c.replace("struct ", "")
        .replace("class ", "")
        .replace(' ', "")
}

#[derive(Debug)]
pub struct Chain {
    pub twin: String,
    pub pairs: Pairs,
}

#[derive(Debug)]
pub struct Twin {
    pub side: Side,
    pub pairs: Pairs,
}

#[derive(Debug)]
pub struct World<'a> {
    pub sources: &'a [Source],
    pub sides: &'a [Side],
    pub direct: &'a [Pairs],
    pub chains: &'a [Option<Chain>],
    pub twins: &'a BTreeMap<String, Twin>,
    pub ours: &'a Side,
    pub info: &'a Ours,
}

#[must_use]
pub fn collect_links(w: &World<'_>) -> BTreeMap<usize, Vec<Link>> {
    let mut out: BTreeMap<usize, Vec<Link>> = BTreeMap::new();
    for (si, (side, dp)) in w.sides.iter().zip(w.direct.iter()).enumerate() {
        for (m, o) in &dp.a2b {
            let Some(ev) = dp.why.get(m) else { continue };
            if ev.how == "seed" {
                continue;
            }
            out.entry(*o).or_default().push(Link {
                source: si,
                mac: *m,
                path: "direct".to_string(),
                kinds: ev.kinds(),
                items: vec![ev_text(ev)],
                sig: pair::gate(side, *m, w.ours, *o),
                strong: strong(ev),
            });
        }
        let Some(Some(ch)) = w.chains.get(si) else {
            continue;
        };
        let Some(tw) = w.twins.get(&ch.twin) else {
            continue;
        };
        for (m, t) in &ch.pairs.a2b {
            let Some(o) = tw.pairs.a2b.get(t) else {
                continue;
            };
            let (Some(e1), Some(e2)) = (ch.pairs.why.get(m), tw.pairs.why.get(t)) else {
                continue;
            };
            let mut kinds = e1.kinds();
            kinds.extend(e2.kinds());
            let tva = tw.side.funs.get(*t).map_or(0, |f| f.va);
            out.entry(*o).or_default().push(Link {
                source: si,
                mac: *m,
                path: format!("chain:{}@{tva:#x}", ch.twin),
                kinds,
                items: vec![
                    format!("mac>pc {}", ev_text(e1)),
                    format!("pc>ours {}", ev_text(e2)),
                ],
                sig: pair::gate(side, *m, w.ours, *o),
                strong: strong(e1) && strong(e2),
            });
        }
    }
    out
}

#[derive(Debug, Clone)]
pub struct Choice {
    pub key: String,
    pub source: usize,
    pub mac: usize,
    pub links: Vec<Link>,
    pub sig: SigLevel,
}

fn rank_of(sources: &[Source], i: usize) -> u32 {
    sources.get(i).map_or(u32::MAX, |s| s.rank)
}

fn path_class(p: &str) -> &'static str {
    if p == "direct" {
        "direct"
    } else if p == "layout" {
        "layout"
    } else {
        "chain"
    }
}

#[must_use]
pub fn choose(links: &[Link], sources: &[Source], sides: &[Side]) -> (Option<Choice>, Vec<Choice>) {
    let mut by_key: BTreeMap<String, Vec<&Link>> = BTreeMap::new();
    for l in links {
        let Some(s) = sides
            .get(l.source)
            .and_then(|s| s.funs.get(l.mac))
            .and_then(|f| f.sig.as_ref())
        else {
            continue;
        };
        by_key.entry(s.key()).or_default().push(l);
    }
    let mut cands: Vec<Choice> = Vec::new();
    for (key, ls) in by_key {
        let best = ls.iter().map(|l| l.sig).max().unwrap_or(SigLevel::Mismatch);
        let any_strong = ls.iter().any(|l| l.strong);
        let distinct: BTreeSet<(usize, &str)> =
            ls.iter().map(|l| (l.source, path_class(&l.path))).collect();
        let ok = ls.iter().all(|l| l.sig != SigLevel::Mismatch)
            && best >= SigLevel::Count
            && (any_strong || (distinct.len() >= 2 && best >= SigLevel::Kinds));
        let Some(top) = ls
            .iter()
            .min_by_key(|l| (rank_of(sources, l.source), !l.strong))
        else {
            continue;
        };
        cands.push(Choice {
            key,
            source: top.source,
            mac: top.mac,
            links: ls.iter().map(|l| (*l).clone()).collect(),
            sig: if ok { best } else { SigLevel::Unknown },
        });
    }
    cands.sort_by_key(|c| {
        (
            c.sig == SigLevel::Unknown,
            !c.links.iter().any(|l| l.strong),
            rank_of(sources, c.source),
            std::cmp::Reverse(c.links.len()),
        )
    });
    let accepted: Vec<&Choice> = cands
        .iter()
        .filter(|c| c.sig != SigLevel::Unknown)
        .collect();
    let Some(first) = accepted.first() else {
        return (None, cands);
    };
    let family = |c: &Choice| sources.get(c.source).map_or("", |s| s.family.as_str());
    let has_strong = |c: &Choice| c.links.iter().any(|l| l.strong);
    let stem = |k: &str| k.split('(').next().unwrap_or("").to_string();
    let rival = accepted.iter().skip(1).any(|c| {
        family(c) == family(first)
            && has_strong(c)
            && has_strong(first)
            && stem(&first.key) != stem(&c.key)
    });
    if rival {
        return (None, cands);
    }
    ((*first).clone().into(), cands)
}

pub type Picks = (BTreeMap<usize, Choice>, BTreeMap<usize, Vec<Choice>>);

#[must_use]
pub fn pick_all(
    links: &BTreeMap<usize, Vec<Link>>,
    sources: &[Source],
    sides: &[Side],
    info: &Ours,
) -> Picks {
    let mut picked: BTreeMap<usize, Choice> = BTreeMap::new();
    let mut leads: BTreeMap<usize, Vec<Choice>> = BTreeMap::new();
    for (i, in_scope) in info.in_scope.iter().enumerate() {
        if !in_scope {
            continue;
        }
        let ls = links.get(&i).cloned().unwrap_or_default();
        let (pick, all) = choose(&ls, sources, sides);
        if let Some(p) = pick {
            picked.insert(i, p);
        }
        leads.insert(i, all);
    }
    let mut by_key: BTreeMap<String, Vec<usize>> = BTreeMap::new();
    for (i, c) in &picked {
        by_key.entry(c.key.clone()).or_default().push(*i);
    }
    for is in by_key.values().filter(|v| v.len() > 1) {
        let strong_ones: Vec<usize> = is
            .iter()
            .copied()
            .filter(|i| {
                picked
                    .get(i)
                    .is_some_and(|c| c.links.iter().any(|l| l.strong))
            })
            .collect();
        for i in is {
            if strong_ones.len() != 1 || !strong_ones.contains(i) {
                picked.remove(i);
            }
        }
    }
    (picked, leads)
}

#[derive(Debug)]
pub struct Context<'a> {
    pub image: &'a str,
    pub attested: &'a BTreeSet<(String, String)>,
    pub classes: &'a BTreeSet<String>,
    pub class_families: &'a [String],
}

fn class_decisions(
    picked: &BTreeMap<usize, Choice>,
    sides: &[Side],
    ours: &Side,
    info: &Ours,
    cx: &Context<'_>,
) -> (BTreeMap<String, String>, Vec<MapRow>) {
    let our_class = |i: usize| ours.funs.get(i)?.sig.as_ref()?.class().map(class_key);
    let mac_sig = |c: &Choice| sides.get(c.source)?.funs.get(c.mac)?.sig.as_ref();
    let mut votes: BTreeMap<String, BTreeMap<String, (usize, bool)>> = BTreeMap::new();
    for (i, c) in picked {
        let (Some(oc), Some(ms)) = (our_class(*i), mac_sig(c)) else {
            continue;
        };
        let Some(mc) = ms.class().map(class_key) else {
            continue;
        };
        let e = votes.entry(oc).or_default().entry(mc).or_insert((0, false));
        e.0 = e.0.saturating_add(1);
        if ms.name == "{ctor}" || ms.name == "{dtor}" {
            e.1 = true;
        }
    }
    let mut class_map: BTreeMap<String, String> = BTreeMap::new();
    let mut claimed: BTreeMap<String, (String, usize)> = BTreeMap::new();
    let mut rows: Vec<MapRow> = Vec::new();
    for (oc, vs) in &votes {
        let total: usize = vs.values().map(|v| v.0).sum();
        let Some((mc, (n, ctor))) = vs
            .iter()
            .max_by_key(|(k, v)| (v.0, std::cmp::Reverse((*k).clone())))
        else {
            continue;
        };
        let agree = n.saturating_mul(4) >= total.saturating_mul(3);
        let enough = (*n >= 3 || (*n >= 1 && *ctor)) && agree;
        let tally = format!(
            "{n} of {total} picked methods agree{}",
            if *ctor { ", ctor" } else { "" }
        );
        let mut row = MapRow {
            kind: "class".to_string(),
            image: cx.image.to_string(),
            old: oc.clone(),
            new: mc.clone(),
            evidence: vs
                .iter()
                .map(|(k, v)| format!("{k}:{}{}", v.0, if v.1 { "+ctor" } else { "" }))
                .collect::<Vec<_>>()
                .join(","),
            ..MapRow::default()
        };
        if oc == mc {
            row.state = "APPLIED".into();
            row.reason = "already".into();
            class_map.insert(oc.clone(), mc.clone());
        } else if cx.attested.contains(&("class".to_string(), oc.clone()))
            || info.words.contains(oc)
        {
            row.state = "RECORDED".into();
            row.reason = "attests-ours".into();
            row.detail = "our own binaries spell our name".into();
        } else if !enough {
            row.state = "RECORDED".into();
            row.reason = "analogy-only".into();
            row.detail = tally;
        } else {
            row.state = "APPLIED".into();
            row.reason = "evidence".into();
            row.detail = tally;
            if let Some((other, m)) = claimed.get(mc)
                && *m >= *n
            {
                row.state = "RECORDED".into();
                row.reason = "analogy-only".into();
                row.detail = format!(
                    "{mc} is also {other}'s ({m} methods): two of our classes are one of theirs"
                );
            }
            if row.applied() {
                claimed.insert(mc.clone(), (oc.clone(), *n));
                class_map.insert(oc.clone(), mc.clone());
            }
        }
        rows.push(row);
    }
    for r in &mut rows {
        if r.applied()
            && r.reason == "evidence"
            && let Some((owner, _)) = claimed.get(&r.new)
            && owner != &r.old
        {
            r.state = "RECORDED".into();
            r.reason = "analogy-only".into();
            r.detail = format!(
                "{} is {owner}'s: two of our classes are one of theirs",
                r.new
            );
            class_map.remove(&r.old);
        }
    }
    (class_map, rows)
}

#[must_use]
pub fn lis(v: &[usize]) -> Vec<usize> {
    let n = v.len();
    let mut len = vec![1usize; n];
    let mut prev = vec![usize::MAX; n];
    for i in 0..n {
        for j in 0..i {
            let lj = len.get(j).copied().unwrap_or(0).saturating_add(1);
            if v.get(j) < v.get(i) && lj > len.get(i).copied().unwrap_or(0) {
                if let Some(x) = len.get_mut(i) {
                    *x = lj;
                }
                if let Some(x) = prev.get_mut(i) {
                    *x = j;
                }
            }
        }
    }
    let Some((mut at, _)) = len
        .iter()
        .enumerate()
        .max_by_key(|(i, l)| (**l, std::cmp::Reverse(*i)))
    else {
        return Vec::new();
    };
    let mut out = vec![at];
    while let Some(&p) = prev.get(at) {
        if p == usize::MAX {
            break;
        }
        out.push(p);
        at = p;
    }
    out.reverse();
    out
}

struct Gap<'a> {
    w: &'a World<'a>,
    si: usize,
    side: &'a Side,
    class_map: &'a BTreeMap<String, String>,
    x86_by_key: &'a BTreeMap<String, (usize, usize)>,
}

impl Gap<'_> {
    fn compat(&self, g: usize, m: usize) -> Option<(SigLevel, bool, String)> {
        let of = self.w.ours.funs.get(g)?;
        let mf = self.side.funs.get(m)?;
        let (os, ms) = (of.sig.as_ref()?, mf.sig.as_ref()?);
        if !ms.params_known || mf.raw.starts_with(SINIT) {
            return None;
        }
        let lvl = pair::gate(self.side, m, self.w.ours, g);
        if lvl < SigLevel::Kinds {
            return None;
        }
        let class_ok = match (os.class().map(class_key), ms.class().map(class_key)) {
            (Some(a), Some(b)) => self.class_map.get(&a).map_or(a == b, |x| *x == b),
            (None, None) => true,
            _ => false,
        };
        if !class_ok {
            return None;
        }
        let label = self.w.sources.get(self.si).map_or("", |s| s.label.as_str());
        let xb = self.w.ours.feats.get(g)?;
        let (xa, via) = match self.x86_by_key.get(&ms.key()) {
            Some((xs, xm)) => (
                self.w.sides.get(*xs)?.feats.get(*xm)?,
                self.w.sources.get(*xs).map_or("", |s| s.label.as_str()),
            ),
            None => (self.side.feats.get(m)?, label),
        };
        let shared_d = xa.disps.intersection(&xb.disps).count();
        let shared_c = xa.consts.intersection(&xb.consts).count();
        let (dj, cj) = pair::konst(xa, xb);
        let trivial = xa.disps.is_empty()
            && xb.disps.is_empty()
            && xa.consts.is_empty()
            && xb.consts.is_empty();
        let konst_ok = (shared_d >= 1 && dj >= 0.5)
            || (shared_c >= 1 && cj >= 0.5)
            || (trivial && lvl == SigLevel::Exact);
        if !konst_ok {
            return None;
        }
        let strong = (shared_d >= 1 && dj >= 0.5 && via != label) || shared_c >= 1;
        Some((
            lvl,
            strong,
            format!("offsets by {via}: disp {dj:.2} ({shared_d}) const {cj:.2} ({shared_c})"),
        ))
    }
}

fn layout_links(
    w: &World<'_>,
    picked: &BTreeMap<usize, Choice>,
    class_map: &BTreeMap<String, String>,
) -> BTreeMap<usize, Vec<Link>> {
    let mut out: BTreeMap<usize, Vec<Link>> = BTreeMap::new();
    let mut by_file: BTreeMap<&str, Vec<usize>> = BTreeMap::new();
    for (i, f) in w.ours.funs.iter().enumerate() {
        if w.info.in_scope.get(i).copied().unwrap_or(false) && !f.file.is_empty() {
            by_file.entry(f.file.as_str()).or_default().push(i);
        }
    }
    for v in by_file.values_mut() {
        v.sort_by_key(|i| w.ours.funs.get(*i).map_or(0, |f| f.va));
    }
    let mut x86_by_key: BTreeMap<String, (usize, usize)> = BTreeMap::new();
    let mut order: Vec<usize> = (0..w.sources.len()).collect();
    order.sort_by_key(|i| rank_of(w.sources, *i));
    for si in order {
        let (Some(src), Some(side)) = (w.sources.get(si), w.sides.get(si)) else {
            continue;
        };
        if src.kind != SourceKind::MachO {
            continue;
        }
        for (m, f) in side.funs.iter().enumerate() {
            if let Some(s) = &f.sig {
                x86_by_key.entry(s.key()).or_insert((si, m));
            }
        }
    }
    for (si, (src, side)) in w.sources.iter().zip(w.sides.iter()).enumerate() {
        if src.kind != SourceKind::Pef {
            continue;
        }
        let mut key_idx: BTreeMap<String, Vec<usize>> = BTreeMap::new();
        for (m, f) in side.funs.iter().enumerate() {
            if let Some(s) = &f.sig {
                key_idx.entry(s.key()).or_default().push(m);
            }
        }
        let gap = Gap {
            w,
            si,
            side,
            class_map,
            x86_by_key: &x86_by_key,
        };
        let name_of = |m: usize| {
            side.funs
                .get(m)
                .and_then(|f| f.sig.as_ref())
                .map_or_else(String::new, Sig::qual_name)
        };
        for gl in by_file.values() {
            let mut anchors: Vec<(usize, usize)> = Vec::new();
            for (p, g) in gl.iter().enumerate() {
                let Some(c) = picked.get(g) else { continue };
                if let Some([m]) = key_idx.get(&c.key).map(Vec::as_slice) {
                    anchors.push((p, *m));
                }
            }
            let keep = lis(&anchors.iter().map(|a| a.1).collect::<Vec<_>>());
            let anchors: Vec<(usize, usize)> = keep
                .iter()
                .filter_map(|k| anchors.get(*k).copied())
                .collect();
            for win in anchors.windows(2) {
                let (Some(&(p0, m0)), Some(&(p1, m1))) = (win.first(), win.get(1)) else {
                    continue;
                };
                if p1 <= p0.saturating_add(1) || m1 <= m0.saturating_add(1) || m1 - m0 > LAYOUT_GAP
                {
                    continue;
                }
                let ours_gap: Vec<usize> = gl
                    .get(p0 + 1..p1)
                    .map(<[usize]>::to_vec)
                    .unwrap_or_default();
                let mut cands: Vec<(usize, usize, SigLevel, bool, String)> = Vec::new();
                for g in &ours_gap {
                    for m in m0 + 1..m1 {
                        if let Some((lvl, strong, why)) = gap.compat(*g, m) {
                            cands.push((*g, m, lvl, strong, why));
                        }
                    }
                }
                let n_g = |g: usize| cands.iter().filter(|c| c.0 == g).count();
                let n_m = |m: usize| cands.iter().filter(|c| c.1 == m).count();
                let mut unique: Vec<(usize, usize, SigLevel, bool, String)> = cands
                    .iter()
                    .filter(|c| n_g(c.0) == 1 && n_m(c.1) == 1)
                    .cloned()
                    .collect();
                unique.sort_by_key(|c| w.ours.funs.get(c.0).map_or(0, |f| f.va));
                let keep = lis(&unique.iter().map(|c| c.1).collect::<Vec<_>>());
                let (a0, a1) = (name_of(m0), name_of(m1));
                for k in keep {
                    let Some((g, m, lvl, strong, why)) = unique.get(k).cloned() else {
                        continue;
                    };
                    out.entry(g).or_default().push(Link {
                        source: si,
                        mac: m,
                        path: "layout".to_string(),
                        kinds: ["LAYOUT", "CONST"].into_iter().collect(),
                        items: vec![format!("layout between {a0} and {a1}: {why}")],
                        sig: lvl,
                        strong,
                    });
                }
            }
        }
    }
    out
}

fn fill_row(row: &mut MapRow, c: &Choice, w: &World<'_>) {
    let mf = w.sides.get(c.source).and_then(|s| s.funs.get(c.mac));
    row.source = w
        .sources
        .get(c.source)
        .map_or_else(String::new, |s| s.label.clone());
    row.mac.clone_from(&c.key);
    row.mangled = mf.map_or_else(String::new, |m| m.raw.clone());
    if let Some(run) = mf.map(|m| m.file.as_str()).filter(|x| !x.is_empty()) {
        row.detail = run.to_string();
    }
    row.twin = c
        .links
        .iter()
        .filter_map(|l| l.path.strip_prefix("chain:"))
        .collect::<Vec<_>>()
        .join(",");
    row.sig = c
        .links
        .iter()
        .map(|l| l.sig)
        .max()
        .unwrap_or(SigLevel::Unknown)
        .tag()
        .to_string();
    row.evidence = c
        .links
        .iter()
        .map(|l| {
            format!(
                "{} {} [{}] {}",
                w.sources.get(l.source).map_or("?", |s| s.label.as_str()),
                l.path,
                l.kinds.iter().copied().collect::<Vec<_>>().join("+"),
                l.items.join(" | ")
            )
        })
        .collect::<Vec<_>>()
        .join(" ;; ");
}

fn our_name_attested(osig: &Sig, info: &Ours, cx: &Context<'_>) -> bool {
    osig.class().is_some_and(|cl| {
        cx.attested.contains(&(
            "member".to_string(),
            format!("{}::{}", class_key(cl), osig.name),
        ))
    }) || (osig.scope.is_empty()
        && cx
            .attested
            .contains(&("function".to_string(), osig.name.clone())))
        || (!osig.name.starts_with('{') && info.words.contains(&osig.name) && osig.name.len() >= 6)
}

fn function_row(
    i: usize,
    w: &World<'_>,
    picks: &Picks,
    class_map: &BTreeMap<String, String>,
    cx: &Context<'_>,
) -> Option<MapRow> {
    let f = w.ours.funs.get(i)?;
    let osig = f.sig.clone().unwrap_or_default();
    let mut row = MapRow {
        kind: if osig.scope.is_empty() {
            "function"
        } else {
            "method"
        }
        .to_string(),
        image: cx.image.to_string(),
        va: format!("{:#x}", f.va),
        old: osig.key(),
        ..MapRow::default()
    };
    let Some(c) = picks.0.get(&i) else {
        row.state = "RECORDED".into();
        match picks.1.get(&i).and_then(|l| l.first()) {
            Some(c) => {
                fill_row(&mut row, c, w);
                row.reason = "analogy-only".into();
                row.detail = if c.sig == SigLevel::Unknown {
                    "a lead: no strong link, or the signature does not agree".into()
                } else {
                    "two names of one family conflict, or the Mac name is another function's".into()
                };
            }
            None => row.reason = "no-twin".into(),
        }
        return Some(row);
    };
    fill_row(&mut row, c, w);
    let ms = w.sides.get(c.source)?.funs.get(c.mac)?.sig.as_ref()?;
    let oc = osig.class().map(class_key);
    let mc = ms.class().map(class_key);
    let mut new = osig.clone();
    if let Some(tc) = oc
        .as_ref()
        .map(|c| class_map.get(c).cloned().unwrap_or_else(|| c.clone()))
    {
        new.scope = vec![tc];
    }
    new.name.clone_from(&ms.name);
    row.new = new.key();
    let same_shape = osig.scope.is_empty() == ms.scope.is_empty()
        && (osig.name == "{ctor}") == (ms.name == "{ctor}")
        && (osig.name == "{dtor}") == (ms.name == "{dtor}");
    let mapped = oc.as_ref().and_then(|o| class_map.get(o));
    if !same_shape {
        row.state = "RECORDED".into();
        row.reason = "analogy-only".into();
        row.detail = format!("shape differs: ours {} theirs {}", osig.key(), ms.key());
    } else if mc.is_some() && mapped.is_some_and(|m| Some(m) != mc.as_ref()) {
        row.state = "RECORDED".into();
        row.reason = "analogy-only".into();
        row.detail = format!(
            "their class {} is not our class's ({} -> {})",
            mc.clone().unwrap_or_default(),
            oc.clone().unwrap_or_default(),
            mapped.cloned().unwrap_or_default()
        );
    } else if our_name_attested(&osig, w.info, cx) && osig.name != ms.name {
        row.state = "RECORDED".into();
        row.reason = "attests-ours".into();
        row.detail = "our own binaries spell our name".into();
    } else if osig.name == ms.name {
        row.state = "APPLIED".into();
        row.reason = "already".into();
    } else {
        row.state = "APPLIED".into();
        row.reason = "evidence".into();
        row.detail = format!(
            "{} name",
            w.sources.get(c.source).map_or("?", |s| s.family.as_str())
        );
    }
    Some(row)
}

fn mac_rows(
    w: &World<'_>,
    rows: &[MapRow],
    picks: &Picks,
    applied_keys: &BTreeMap<String, usize>,
    cx: &Context<'_>,
) -> Vec<MapRow> {
    let mut lead_of: BTreeMap<String, (usize, String)> = BTreeMap::new();
    for (i, cs) in &picks.1 {
        for c in cs {
            lead_of.entry(c.key.clone()).or_insert((
                *i,
                w.sources
                    .get(c.source)
                    .map_or_else(String::new, |s| s.label.clone()),
            ));
        }
    }
    let va_of = |i: usize| {
        w.ours
            .funs
            .get(i)
            .map_or_else(String::new, |o| format!("{:#x}", o.va))
    };
    let key_of = |i: usize| {
        w.ours
            .funs
            .get(i)
            .and_then(|o| o.sig.as_ref())
            .map_or_else(String::new, Sig::key)
    };
    let applied_new: BTreeMap<String, String> = rows
        .iter()
        .filter(|r| r.applied() && r.function())
        .map(|r| (r.va.clone(), r.mac.clone()))
        .collect();
    let mut seen: BTreeSet<String> = BTreeSet::new();
    let mut out = Vec::new();
    for (si, src) in w.sources.iter().enumerate() {
        if !cx.class_families.is_empty() && !cx.class_families.contains(&src.family) {
            continue;
        }
        let Some(side) = w.sides.get(si) else {
            continue;
        };
        for f in &side.funs {
            let Some(s) = &f.sig else { continue };
            let Some(cl) = s.class() else { continue };
            if !cx.classes.contains(cl) && !cx.classes.contains(cl.split('<').next().unwrap_or(cl))
            {
                continue;
            }
            if !seen.insert(f.raw.clone()) {
                continue;
            }
            let key = s.key();
            let mut row = MapRow {
                kind: "mac".into(),
                source: src.label.clone(),
                mac: key.clone(),
                mangled: f.raw.clone(),
                state: "RECORDED".into(),
                ..MapRow::default()
            };
            if let Some(i) = applied_keys.get(&key) {
                row.state = "APPLIED".into();
                row.reason = "evidence".into();
                row.image = cx.image.to_string();
                row.va = va_of(*i);
                row.old = key_of(*i);
            } else if let Some((i, by)) = lead_of.get(&key) {
                row.image = cx.image.to_string();
                row.va = va_of(*i);
                row.old = key_of(*i);
                match applied_new.get(&row.va) {
                    Some(other) if other != &key => {
                        row.reason = "supersedes".into();
                        row.detail = format!("our function takes {other} (lead by {by})");
                    }
                    _ => {
                        let attests = rows
                            .iter()
                            .find(|r| r.va == row.va && r.function())
                            .is_some_and(|r| r.reason == "attests-ours");
                        row.reason = if attests {
                            "attests-ours"
                        } else {
                            "analogy-only"
                        }
                        .into();
                        row.detail = format!("lead by {by}");
                    }
                }
            } else {
                row.reason = "no-twin".into();
            }
            out.push(row);
        }
    }
    out
}

pub fn decide(w: &World<'_>, links: &BTreeMap<usize, Vec<Link>>, cx: &Context<'_>) -> Vec<MapRow> {
    let mut picks = pick_all(links, w.sources, w.sides, w.info);
    let (mut class_map, mut class_rows) = class_decisions(&picks.0, w.sides, w.ours, w.info, cx);
    let mut merged = links.clone();
    let mut seen: BTreeSet<(usize, usize, usize)> = BTreeSet::new();
    for _ in 0..LAYOUT_ROUNDS {
        let mut added = 0usize;
        for (g, ls) in layout_links(w, &picks.0, &class_map) {
            for l in ls {
                if seen.insert((g, l.source, l.mac)) {
                    merged.entry(g).or_default().push(l);
                    added = added.saturating_add(1);
                }
            }
        }
        let next = pick_all(&merged, w.sources, w.sides, w.info);
        let grew = next.0.len() > picks.0.len();
        picks = next;
        (class_map, class_rows) = class_decisions(&picks.0, w.sides, w.ours, w.info, cx);
        if !grew && added == 0 {
            break;
        }
    }
    let mut rows = class_rows;
    let mut applied_keys: BTreeMap<String, usize> = BTreeMap::new();
    for (i, in_scope) in w.info.in_scope.iter().enumerate() {
        if !in_scope {
            continue;
        }
        let Some(row) = function_row(i, w, &picks, &class_map, cx) else {
            continue;
        };
        if row.applied()
            && let Some(c) = picks.0.get(&i)
        {
            applied_keys.insert(c.key.clone(), i);
        }
        rows.push(row);
    }
    let macs = mac_rows(w, &rows, &picks, &applied_keys, cx);
    rows.extend(macs);
    rows
}

pub fn apply_measured(rows: &mut [MapRow], text: &str) -> Result<Vec<String>> {
    let mut stale = Vec::new();
    for (n, l) in text.lines().enumerate() {
        if l.trim().is_empty() || l.starts_with('#') {
            continue;
        }
        let c: Vec<&str> = l.split('\t').collect();
        let (Some(kind), Some(key), Some(reason)) = (c.first(), c.get(1), c.get(2)) else {
            bail!(
                "measured line {}: want kind key reason detail",
                n.saturating_add(1)
            );
        };
        if !is_reason(reason) {
            bail!(
                "measured line {}: reason `{reason}` is not a RECORDED reason (one lower-case word, not evidence \
                 or already; the usual ones are {})",
                n.saturating_add(1),
                RECORDED_REASONS.join(", ")
            );
        }
        let detail = c.get(3).copied().unwrap_or("");
        let mut hit = false;
        for r in rows.iter_mut() {
            let k = if r.kind == "class" { &r.old } else { &r.va };
            if r.kind == *kind && k == key && r.applied() && r.reason == "evidence" {
                r.state = "RECORDED".into();
                r.reason = (*reason).to_string();
                r.detail = format!("measured: {detail}");
                hit = true;
            }
        }
        if !hit {
            stale.push(format!("{kind} {key}"));
        }
    }
    Ok(stale)
}

pub fn restore_history(rows: &mut [MapRow], prior: &[MapRow]) {
    let by_va: BTreeMap<&str, &MapRow> = prior
        .iter()
        .filter(|r| r.function() && r.applied() && r.reason == "evidence")
        .map(|r| (r.va.as_str(), r))
        .collect();
    let by_class: BTreeMap<&str, &MapRow> = prior
        .iter()
        .filter(|r| r.kind == "class" && r.applied() && r.reason == "evidence")
        .map(|r| (r.new.as_str(), r))
        .collect();
    for r in rows
        .iter_mut()
        .filter(|r| r.applied() && r.reason == "already")
    {
        let p = if r.kind == "class" {
            by_class.get(r.old.as_str())
        } else {
            by_va.get(r.va.as_str())
        };
        let Some(p) = p else { continue };
        let same = if r.kind == "class" {
            p.new == r.old
        } else {
            p.new == r.new || p.new == r.old
        };
        if same {
            r.old.clone_from(&p.old);
            r.reason = "evidence".into();
            if r.detail.is_empty() {
                r.detail.clone_from(&p.detail);
            }
        }
    }
}

pub fn sync_mac_rows(rows: &mut [MapRow]) {
    let by_va: HashMap<String, (String, String, String, String)> = rows
        .iter()
        .filter(|r| r.function())
        .map(|r| {
            (
                r.va.clone(),
                (
                    r.state.clone(),
                    r.reason.clone(),
                    r.mac.clone(),
                    r.detail.clone(),
                ),
            )
        })
        .collect();
    for r in rows.iter_mut().filter(|r| r.kind == "mac" && r.applied()) {
        let Some((state, reason, mac, detail)) = by_va.get(&r.va) else {
            continue;
        };
        if state != "APPLIED" || *mac != r.mac {
            r.state = "RECORDED".into();
            r.reason = if state == "APPLIED" {
                "supersedes".into()
            } else {
                reason.clone()
            };
            r.detail = format!("our function: {detail}");
        }
    }
}

#[must_use]
pub fn direct_table(side: &Side, ours: &Side, dp: &Pairs) -> String {
    let mut t = format!("{DIRECT_HEAD}\n");
    for (m, ov) in &dp.a2b {
        let (Some(mf), Some(of)) = (side.funs.get(*m), ours.funs.get(*ov)) else {
            continue;
        };
        let ev = dp.why.get(m);
        let (dj, cj) = match (side.feats.get(*m), ours.feats.get(*ov)) {
            (Some(a), Some(b)) => pair::konst(a, b),
            _ => (0.0, 0.0),
        };
        let _ = writeln!(
            t,
            "{:#x}\t{}\t{:#x}\t{}\t{}\t{}\t{}\t{dj:.2}\t{cj:.2}",
            mf.va,
            mf.sig.as_ref().map_or_else(|| mf.raw.clone(), Sig::key),
            of.va,
            cell(&of.raw),
            ev.map_or(0, |e| e.round),
            ev.map_or_else(String::new, ev_text),
            ev.map_or("", |e| e.sig.tag())
        );
    }
    t
}

#[must_use]
pub fn runs_table(rows: &[MapRow], side: &Side, ours: &Side) -> String {
    let file_of: BTreeMap<String, String> = ours
        .funs
        .iter()
        .map(|f| (format!("{:#x}", f.va), f.file.clone()))
        .collect();
    let pair_of: BTreeMap<&str, (&str, &str)> = rows
        .iter()
        .filter(|r| {
            r.function()
                && !r.mac.is_empty()
                && (r.applied() || r.reason == "byte-cost" || r.reason == "attests-ours")
        })
        .map(|r| {
            (
                r.mac.as_str(),
                (r.va.as_str(), file_of.get(&r.va).map_or("", String::as_str)),
            )
        })
        .collect();
    let key = |f: &pair::Fun| f.sig.as_ref().map_or_else(|| f.raw.clone(), Sig::key);
    let held: BTreeSet<&str> = side
        .funs
        .iter()
        .filter(|f| pair_of.contains_key(key(f).as_str()))
        .map(|f| f.file.as_str())
        .filter(|r| r.starts_with("run:"))
        .collect();
    let mut t = String::from("pos\trun\tmac\tour_va\tour_file\n");
    for (i, f) in side.funs.iter().enumerate() {
        if !held.contains(f.file.as_str()) {
            continue;
        }
        let k = key(f);
        let (va, file) = pair_of.get(k.as_str()).copied().unwrap_or(("", ""));
        let _ = writeln!(
            t,
            "{i}\t{}\t{}\t{va}\t{file}",
            f.file.trim_start_matches("run:"),
            cell(&k)
        );
    }
    t
}

#[must_use]
pub fn summary(rows: &[MapRow]) -> Vec<String> {
    let mut c: BTreeMap<(String, String, String), usize> = BTreeMap::new();
    for r in rows {
        let e = c
            .entry((r.kind.clone(), r.state.clone(), r.reason.clone()))
            .or_insert(0);
        *e = e.saturating_add(1);
    }
    c.iter()
        .map(|((k, s, r), n)| format!("{k} {s} {r}: {n}"))
        .collect()
}

#[must_use]
pub fn preamble(sources: &[Source], image: &str) -> String {
    let mut s = format!(
        "# Names for {image}'s functions from Mac builds of shared code (msvc6t mac pair). Sources, best first:\n"
    );
    let mut by_rank: Vec<&Source> = sources.iter().collect();
    by_rank.sort_by_key(|s| s.rank);
    for x in by_rank {
        let _ = writeln!(
            s,
            "#   {} ({}, {}): {}{}",
            x.label,
            x.family,
            match x.kind {
                SourceKind::Pef => "pef",
                SourceKind::MachO => "macho",
            },
            x.path
                .file_name()
                .map_or_else(String::new, |f| f.to_string_lossy().into_owned()),
            x.twin
                .as_ref()
                .map_or_else(String::new, |t| format!(", PC twin {t}"))
        );
    }
    s.push_str("# state APPLIED: evidence (a function-specific pairing, signature agreeing, a strong or two independent links)\n");
    s.push_str("#   or already (our name is theirs). RECORDED: no-twin, analogy-only, attests-ours, supersedes, byte-cost.\n");
    s.push_str("#   evidence: per link `<source> <path> <how><round>:<votes>:<anchors>`.\n");
    s
}
