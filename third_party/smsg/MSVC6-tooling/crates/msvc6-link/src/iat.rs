use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

use crate::qsort::{self, Crt};
use crate::verbose::{Kind, Load};

pub const MAX_PULLS: usize = 1 << 20;

pub const NAME_MAX: usize = 512;

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Pull {
    pub member: String,
    pub symbol: String,
    pub kind: Kind,
    pub first_ref: Option<String>,
}

pub fn pulls(loads: &[Load]) -> Result<Vec<Pull>> {
    let out: Vec<Pull> = loads
        .iter()
        .filter(|l| l.is_import())
        .map(|l| Pull {
            member: l.member.clone(),
            kind: l.kind(),
            symbol: l.symbol.clone(),
            first_ref: l.first_ref.clone(),
        })
        .collect();
    if out.len() > MAX_PULLS {
        bail!(
            "{} import-library loads: more than the {MAX_PULLS} this model sorts",
            out.len()
        );
    }
    Ok(out)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Control {
    Model,
    Reverse,
    NoNull,
}

impl Control {
    pub fn parse(s: &str) -> Result<Self> {
        match s {
            "model" => Ok(Self::Model),
            "reverse" => Ok(Self::Reverse),
            "no-null" => Ok(Self::NoNull),
            _ => bail!("`{s}`: the population is model, reverse or no-null"),
        }
    }

    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Model => "Model",
            Self::Reverse => "Reverse",
            Self::NoNull => "NoNull",
        }
    }
}

pub fn sorted_population(pulls: &[Pull], crt: Crt, control: Control) -> Result<Vec<usize>> {
    let mut pop: Vec<usize> = pulls
        .iter()
        .enumerate()
        .filter(|(_, p)| match control {
            Control::NoNull => p.kind == Kind::Import,
            Control::Model | Control::Reverse => matches!(p.kind, Kind::Import | Kind::NullThunk),
        })
        .map(|(i, _)| i)
        .collect();
    if control == Control::Reverse {
        pop.reverse();
    }
    let key = |i: &usize| pulls.get(*i).map_or("", |p| p.member.as_str());
    qsort::sort(crt, &mut pop, &|a: &usize, b: &usize| key(a).cmp(key(b)))?;
    Ok(pop)
}

pub type Blocks = BTreeMap<String, Vec<(u32, String)>>;

pub fn blocks(img: &Image) -> Result<Blocks> {
    let mut out: Blocks = BTreeMap::new();
    for d in img.imports()? {
        let v = out.entry(d.dll.to_ascii_lowercase()).or_default();
        for e in &d.entries {
            v.push((e.iat_rva.get(), e.name.clone()));
        }
        v.sort_by_key(|(r, _)| *r);
    }
    Ok(out)
}

#[must_use]
pub fn resolve<'a>(symbol: &str, names: &'a BTreeSet<String>) -> Option<&'a String> {
    let s = symbol.strip_prefix("__imp_").unwrap_or(symbol);
    let mut cands = vec![s.to_string()];
    if let Some(t) = s.strip_prefix('_') {
        cands.push(t.to_string());
        if let Some(at) = t.rfind('@')
            && t.get(at + 1..)
                .is_some_and(|n| !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()))
        {
            cands.push(t.get(..at).unwrap_or_default().to_string());
        }
    }
    cands.iter().find_map(|c| names.get(c))
}

#[must_use]
pub fn resolve_eq(symbol: &str, name: &str) -> bool {
    let set: BTreeSet<String> = std::iter::once(name.to_string()).collect();
    resolve(symbol, &set).is_some()
}

fn name_sets(blocks: &Blocks) -> BTreeMap<&String, BTreeSet<String>> {
    blocks
        .iter()
        .map(|(k, v)| (k, v.iter().map(|(_, n)| n.clone()).collect()))
        .collect()
}

pub type Predicted = BTreeMap<String, Vec<(usize, Option<String>)>>;

#[must_use]
pub fn block_names(pulls: &[Pull], order: &[usize], blocks: &Blocks) -> Predicted {
    let sets = name_sets(blocks);
    let mut out: Predicted = BTreeMap::new();
    for &i in order {
        let Some(p) = pulls.get(i) else { continue };
        if p.kind != Kind::Import {
            continue;
        }
        let dll = p.member.to_ascii_lowercase();
        let name = sets.get(&dll).and_then(|s| resolve(&p.symbol, s)).cloned();
        out.entry(dll).or_default().push((i, name));
    }
    for (dll, v) in &mut out {
        let Some(img) = blocks.get(dll) else { continue };
        let claimed: BTreeSet<&String> = v.iter().filter_map(|(_, n)| n.as_ref()).collect();
        let free: Vec<String> = img
            .iter()
            .map(|(_, n)| n)
            .filter(|n| n.starts_with("Ordinal(") && !claimed.contains(n))
            .cloned()
            .collect();
        let open = v.iter().filter(|(_, n)| n.is_none()).count();
        if open > 0 && open == free.len() {
            let mut it = free.into_iter();
            for (_, n) in v.iter_mut().filter(|(_, n)| n.is_none()) {
                *n = it.next();
            }
        }
    }
    out
}

#[must_use]
pub fn agree(
    pred: &[(usize, Option<String>)],
    image: &[(u32, String)],
) -> (usize, usize, Option<usize>) {
    let live: Vec<&String> = pred.iter().filter_map(|(_, n)| n.as_ref()).collect();
    let same = live
        .iter()
        .zip(image.iter())
        .filter(|(a, (_, b))| **a == b)
        .count();
    let first = live
        .iter()
        .zip(image.iter())
        .position(|(a, (_, b))| *a != b)
        .or_else(|| (live.len() != image.len()).then_some(live.len().min(image.len())));
    (same, live.len(), first)
}

#[must_use]
pub fn block_order(pulls: &[Pull], sorted: &[usize]) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    for &i in sorted {
        let Some(p) = pulls.get(i) else { continue };
        if out.last() != Some(&p.member) {
            out.push(p.member.clone());
        }
    }
    out
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct BlockRow {
    pub member: String,
    pub same: usize,
    pub slots: usize,
    pub live: usize,
    pub dead: usize,
    pub first: Option<usize>,
    pub predicted: Option<String>,
    pub image: Option<String>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Comparison {
    pub rows: Vec<BlockRow>,
    pub same: usize,
    pub slots: usize,
}

impl Comparison {
    #[must_use]
    pub fn lines(&self) -> Vec<String> {
        let mut out = Vec::new();
        for r in &self.rows {
            out.push(format!(
                "    {:14} {:3}/{:3} in sequence ({} predicted live, {} pulled but not in this image)",
                r.member, r.same, r.slots, r.live, r.dead
            ));
            if let Some(f) = r.first {
                out.push(format!(
                    "      first divergence at slot {f}: predicted {}, image {}",
                    r.predicted.as_deref().unwrap_or("-"),
                    r.image.as_deref().unwrap_or("-")
                ));
            }
        }
        out.push(format!(
            "    TOTAL {}/{} slot(s) in sequence",
            self.same, self.slots
        ));
        out
    }
}

#[must_use]
pub fn compare(
    pulls: &[Pull],
    sorted: &[usize],
    blocks: &Blocks,
    block_order: &[String],
) -> Comparison {
    let pred = block_names(pulls, sorted, blocks);
    let mut c = Comparison::default();
    let empty = Vec::new();
    for member in block_order {
        let dll = member.to_ascii_lowercase();
        let Some(pb) = pred.get(&dll) else { continue };
        let ib = blocks.get(&dll).unwrap_or(&empty);
        let (same, live, first) = agree(pb, ib);
        c.same += same;
        c.slots += ib.len();
        let lv: Vec<&String> = pb.iter().filter_map(|(_, n)| n.as_ref()).collect();
        c.rows.push(BlockRow {
            member: member.clone(),
            same,
            slots: ib.len(),
            live,
            dead: pb.len() - live,
            first,
            predicted: first.and_then(|f| lv.get(f).map(|s| (*s).clone())),
            image: first.and_then(|f| ib.get(f).map(|(_, s)| s.clone())),
        });
    }
    c
}

fn bounded_cstr(img: &Image, rva: u32) -> Option<(String, usize)> {
    let b = img.at_rva(Rva(rva), img.raw_left(Rva(rva)))?;
    let end = b.iter().position(|x| *x == 0).unwrap_or(b.len());
    let name = if end > NAME_MAX {
        String::new()
    } else {
        msvc6_base::srcio::decode(b.get(..end)?)
    };
    Some((name, end))
}

pub type Six = Vec<(u32, String, Option<String>)>;

pub fn idata6(img: &Image) -> Result<Six> {
    let mut v: Vec<(u32, u8, String, Option<String>)> = Vec::new();
    for d in img.imports()? {
        let dll = d.dll.to_ascii_lowercase();
        v.push((d.name_rva.get(), 1, dll.clone(), None));
        for e in &d.entries {
            match e.hint_rva {
                Some(h) => v.push((h.get(), 0, dll.clone(), Some(e.name.clone()))),
                None => v.push((d.name_rva.get(), 0, dll.clone(), Some(e.name.clone()))),
            }
        }
    }
    v.sort_by_key(|(r, k, _, _)| (*r, *k));
    Ok(v.into_iter().map(|(r, _, d, n)| (r, d, n)).collect())
}

fn even_len(n: usize, extra: u32) -> u32 {
    (u32::try_from(n)
        .unwrap_or(u32::MAX - 4)
        .saturating_add(extra))
        & !1
}

pub fn orphan_dll_names(img: &Image) -> Result<Vec<(u32, String)>> {
    let mut known: BTreeMap<u32, u32> = BTreeMap::new();
    for d in img.imports()? {
        known.insert(d.name_rva.get(), even_len(d.dll.len(), 2));
        for e in &d.entries {
            if let Some(h) = e.hint_rva {
                known.insert(h.get(), even_len(e.name.len(), 4));
            }
        }
    }
    let (Some((&lo, _)), Some((&hi, &hlen))) = (known.first_key_value(), known.last_key_value())
    else {
        return Ok(Vec::new());
    };
    let mut out = Vec::new();
    let mut pos = lo;
    let end = hi.saturating_add(hlen);
    while pos < end {
        if let Some(len) = known.get(&pos) {
            pos = pos.saturating_add((*len).max(2));
            continue;
        }
        let (s, len) = bounded_cstr(img, pos).unwrap_or_default();
        if len == 0 {
            pos = pos.saturating_add(1);
            continue;
        }
        if s.to_ascii_lowercase().ends_with(".dll") {
            out.push((pos, s));
        }
        pos = pos.saturating_add(even_len(len, 2).max(2));
    }
    Ok(out)
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Spec {
    pub dll: String,
    pub name: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Edit {
    Move {
        what: Spec,
        before: bool,
        anchor: Spec,
    },
    Add {
        what: Spec,
        before: bool,
        anchor: Spec,
    },
    Drop {
        what: Spec,
    },
}

fn parse_spec(s: &str) -> Result<Spec> {
    match s.split_once('!') {
        Some((d, n)) if !d.is_empty() && !n.is_empty() => Ok(Spec {
            dll: d.to_string(),
            name: n.to_string(),
        }),
        _ => Err(anyhow!("`{s}': want DLL!NAME")),
    }
}

pub fn parse_edits(text: &str) -> Result<Vec<Edit>> {
    let mut out = Vec::new();
    for (k, raw) in text.lines().enumerate() {
        let line = raw.split('#').next().unwrap_or_default().trim();
        if line.is_empty() {
            continue;
        }
        let w: Vec<&str> = line.split_whitespace().collect();
        let e = match w.as_slice() {
            ["drop", what] => Edit::Drop {
                what: parse_spec(what)?,
            },
            [
                verb @ ("move" | "add"),
                what,
                rel @ ("after" | "before"),
                anchor,
            ] => {
                let (what, anchor, before) =
                    (parse_spec(what)?, parse_spec(anchor)?, *rel == "before");
                if *verb == "move" {
                    Edit::Move {
                        what,
                        before,
                        anchor,
                    }
                } else {
                    Edit::Add {
                        what,
                        before,
                        anchor,
                    }
                }
            }
            _ => {
                bail!(
                    "edit line {}: `{line}' -- want `move|add D!N after|before D!N' or `drop D!N'",
                    k + 1
                )
            }
        };
        out.push(e);
    }
    Ok(out)
}

fn spec_matches(p: &Pull, s: &Spec) -> bool {
    p.member.eq_ignore_ascii_case(&s.dll)
        && match s.name.as_str() {
            "<descriptor>" => p.kind == Kind::Descriptor,
            "<null-thunk>" => p.kind == Kind::NullThunk,
            n => p.kind == Kind::Import && (p.symbol == n || resolve_eq(&p.symbol, n)),
        }
}

fn find_pull(pulls: &[Pull], s: &Spec) -> Result<usize> {
    pulls
        .iter()
        .position(|p| spec_matches(p, s))
        .ok_or_else(|| anyhow!("{}!{}: no such pull", s.dll, s.name))
}

pub fn apply_edits(pulls: &mut Vec<Pull>, edits: &[Edit]) -> Result<()> {
    for e in edits {
        match e {
            Edit::Drop { what } => {
                let i = find_pull(pulls, what)?;
                pulls.remove(i);
            }
            Edit::Move {
                what,
                before,
                anchor,
            } => {
                let i = find_pull(pulls, what)?;
                let p = pulls.remove(i);
                let j = find_pull(pulls, anchor)?;
                pulls.insert(if *before { j } else { j + 1 }, p);
            }
            Edit::Add {
                what,
                before,
                anchor,
            } => {
                if find_pull(pulls, what).is_ok() {
                    bail!(
                        "add {}!{}: already pulled -- `move' it",
                        what.dll,
                        what.name
                    );
                }
                let member = pulls
                    .iter()
                    .find(|p| p.member.eq_ignore_ascii_case(&what.dll))
                    .map_or_else(|| what.dll.clone(), |p| p.member.clone());
                let (symbol, kind) = if what.name == "<null-thunk>" {
                    (format!("{member}_NULL_THUNK_DATA"), Kind::NullThunk)
                } else {
                    (what.name.clone(), Kind::Import)
                };
                let j = find_pull(pulls, anchor)?;
                pulls.insert(
                    if *before { j } else { j + 1 },
                    Pull {
                        member,
                        symbol,
                        kind,
                        first_ref: Some("(edit)".into()),
                    },
                );
            }
        }
    }
    if pulls.len() > MAX_PULLS {
        bail!("the edits made more than {MAX_PULLS} pulls");
    }
    Ok(())
}

#[must_use]
pub fn spec_of(p: &Pull) -> String {
    let name = match p.kind {
        Kind::Descriptor => "<descriptor>",
        Kind::NullThunk => "<null-thunk>",
        Kind::NullDescriptor => "<null-descriptor>",
        Kind::Import => p.symbol.as_str(),
    };
    format!("{}!{name}", p.member.to_ascii_lowercase())
}

pub fn total_same(pulls: &[Pull], crt: Crt, blocks: &Blocks) -> Result<usize> {
    let sorted = sorted_population(pulls, crt, Control::Model)?;
    let empty = Vec::new();
    Ok(block_names(pulls, &sorted, blocks)
        .iter()
        .map(|(dll, pb)| agree(pb, blocks.get(dll).unwrap_or(&empty)).0)
        .sum())
}

pub const PHANTOM_STEPS_MAX: usize = 64;

pub const PHANTOM_TRIALS_MAX: usize = 1 << 22;

pub fn phantom_search(
    pulls: &mut Vec<Pull>,
    crt: Crt,
    blocks: &Blocks,
    steps: usize,
) -> Result<Vec<String>> {
    let mut lines = Vec::new();
    let mut keys: Vec<String> = Vec::new();
    for p in pulls.iter() {
        if !keys.contains(&p.member) {
            keys.push(p.member.clone());
        }
    }
    let steps = steps.min(PHANTOM_STEPS_MAX);
    let trials = steps
        .saturating_mul(pulls.len().saturating_add(1 + steps))
        .saturating_mul(keys.len());
    if trials > PHANTOM_TRIALS_MAX {
        bail!(
            "--phantom-search {steps}: {trials} trial sorts, more than the {PHANTOM_TRIALS_MAX} this search runs"
        );
    }
    let mut cur = total_same(pulls, crt, blocks)?;
    lines.push(String::new());
    lines.push(format!(
        "  PHANTOM SEARCH (greedy, up to {steps}): start {cur} in sequence"
    ));
    for step in 0..steps {
        let mut best: Option<(usize, usize, usize)> = None;
        for pos in 0..=pulls.len() {
            for (k, key) in keys.iter().enumerate() {
                pulls.insert(
                    pos,
                    Pull {
                        member: key.clone(),
                        symbol: format!("<phantom{step}>"),
                        kind: Kind::Import,
                        first_ref: None,
                    },
                );
                let s = total_same(pulls, crt, blocks);
                pulls.remove(pos);
                let s = s?;
                if s > best.map_or(cur, |b| b.0) {
                    best = Some((s, pos, k));
                }
            }
        }
        let Some((s, pos, k)) = best else {
            lines.push(format!(
                "    no single phantom raises {cur}: stop after {step}"
            ));
            break;
        };
        let key = keys.get(k).cloned().unwrap_or_default();
        let anchor = if pos == 0 {
            pulls.first().map(|p| format!("before {}", spec_of(p)))
        } else {
            pulls.get(pos - 1).map(|p| format!("after {}", spec_of(p)))
        }
        .unwrap_or_default();
        pulls.insert(
            pos,
            Pull {
                member: key.clone(),
                symbol: format!("<phantom{step}>"),
                kind: Kind::Import,
                first_ref: None,
            },
        );
        lines.push(format!(
            "    add {}!<phantom{step}> {anchor}    # load #{pos}: {cur} -> {s}",
            key.to_ascii_lowercase()
        ));
        cur = s;
    }
    Ok(lines)
}

fn same_pull(ours: &Pull, theirs: &Pull) -> bool {
    ours.member.eq_ignore_ascii_case(&theirs.member)
        && ours.kind == theirs.kind
        && (ours.kind == Kind::NullThunk || resolve_eq(&ours.symbol, &theirs.symbol))
}

pub fn seed_dead(pulls: &mut Vec<Pull>, log_pulls: &[Pull], blocks: &Blocks) -> Vec<String> {
    let sets = name_sets(blocks);
    let mut lines = Vec::new();
    let mut at: Option<usize> = None;
    for p in log_pulls {
        if !matches!(p.kind, Kind::Import | Kind::NullThunk) {
            continue;
        }
        if let Some(q) = pulls.iter().position(|r| same_pull(p, r)) {
            at = Some(q);
            continue;
        }
        let live = p.kind == Kind::Import
            && sets
                .get(&p.member.to_ascii_lowercase())
                .is_some_and(|s| resolve(&p.symbol, s).is_some());
        if live {
            continue;
        }
        let pos = at.map_or(0, |q| q + 1);
        lines.push(format!(
            "      seed {} {} by {} after {}",
            p.member,
            p.symbol,
            p.first_ref.as_deref().unwrap_or("-"),
            at.and_then(|q| pulls.get(q))
                .map_or_else(|| "-".to_string(), spec_of)
        ));
        pulls.insert(pos, p.clone());
        at = Some(pos);
    }
    lines
}

pub type Population = (Vec<Pull>, Vec<(u32, String)>);

pub fn reference_population(img: &Image) -> Result<Population> {
    let members: BTreeMap<String, String> = img
        .imports()?
        .iter()
        .map(|d| (d.dll.to_ascii_lowercase(), d.dll.clone()))
        .collect();
    let mut six = idata6(img)?;
    let orphans = orphan_dll_names(img)?;
    for (r, name) in &orphans {
        six.push((*r, name.to_ascii_lowercase(), None));
    }
    six.sort_by_key(|(r, _, n)| (*r, n.is_none()));
    let mut pulls: Vec<Pull> = Vec::new();
    for (_, dll, name) in six {
        let member = members
            .get(&dll)
            .cloned()
            .or_else(|| {
                orphans
                    .iter()
                    .find(|(_, o)| o.eq_ignore_ascii_case(&dll))
                    .map(|(_, o)| o.clone())
            })
            .unwrap_or_else(|| dll.clone());
        let (symbol, kind) = match name {
            Some(n) => (n, Kind::Import),
            None => (format!("{member}_NULL_THUNK_DATA"), Kind::NullThunk),
        };
        pulls.push(Pull {
            member,
            symbol,
            kind,
            first_ref: None,
        });
    }
    Ok((pulls, orphans))
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Want {
    pub load: usize,
    pub want: String,
    pub ours: String,
    pub first_ref: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Inverted {
    pub dll: String,
    pub differs: Option<(usize, String)>,
    pub held: usize,
    pub positions: usize,
    pub rows: Vec<Want>,
}

#[must_use]
pub fn invert(pulls: &[Pull], sorted: &[usize], blocks: &Blocks) -> Vec<Inverted> {
    let pred = block_names(pulls, sorted, blocks);
    let empty = Vec::new();
    let mut out = Vec::new();
    for (dll, pb) in &pred {
        let ib = blocks.get(dll).unwrap_or(&empty);
        let live_out: Vec<usize> = pb
            .iter()
            .filter(|(_, n)| n.is_some())
            .map(|(i, _)| *i)
            .collect();
        let ours_names: BTreeSet<&String> = pb.iter().filter_map(|(_, n)| n.as_ref()).collect();
        let ref_names: BTreeSet<&String> = ib.iter().map(|(_, n)| n).collect();
        if ours_names != ref_names {
            let missing: Vec<&&String> = ref_names.difference(&ours_names).collect();
            out.push(Inverted {
                dll: dll.clone(),
                differs: Some((
                    missing.len(),
                    missing.first().map_or("-", |s| s.as_str()).to_string(),
                )),
                held: 0,
                positions: 0,
                rows: Vec::new(),
            });
            continue;
        }
        let mut want: Vec<(usize, &String)> = live_out
            .iter()
            .zip(ib.iter())
            .map(|(i, (_, n))| (*i, n))
            .collect();
        want.sort_by_key(|(i, _)| *i);
        let mut have: Vec<(usize, &String)> = pb
            .iter()
            .filter_map(|(i, n)| n.as_ref().map(|n| (*i, n)))
            .collect();
        have.sort_by_key(|(i, _)| *i);
        let same = want
            .iter()
            .zip(have.iter())
            .filter(|(a, b)| a.1 == b.1)
            .count();
        let mut rows = Vec::new();
        if same != want.len() {
            for ((pos, w), (_, h)) in want.iter().zip(have.iter()) {
                let by = pulls
                    .iter()
                    .find(|p| {
                        p.kind == Kind::Import
                            && p.member.eq_ignore_ascii_case(dll)
                            && resolve_eq(&p.symbol, w)
                    })
                    .and_then(|p| p.first_ref.as_deref())
                    .unwrap_or("-");
                rows.push(Want {
                    load: *pos,
                    want: (*w).clone(),
                    ours: (*h).clone(),
                    first_ref: by.to_string(),
                });
            }
        }
        out.push(Inverted {
            dll: dll.clone(),
            differs: None,
            held: same,
            positions: want.len(),
            rows,
        });
    }
    out
}

#[must_use]
pub fn inversion_lines(inv: &[Inverted]) -> Vec<String> {
    let mut out = vec![
        String::new(),
        "  INVERTED: the pull order that gives the REFERENCE's order, same key sequence and population. A pull the"
            .to_string(),
        "  reference does not bind (dead) keeps its own load position. `ours' is this log's order.".to_string(),
    ];
    for x in inv {
        if let Some((n, e)) = &x.differs {
            out.push(format!(
                "    {}: SET DIFFERS ({n} reference name(s) never pulled, e.g. {e}) -- not invertible until it agrees",
                x.dll
            ));
            continue;
        }
        out.push(format!(
            "    {}: {}/{} load position(s) already hold the wanted import",
            x.dll, x.held, x.positions
        ));
        for r in &x.rows {
            let mark = if r.want == r.ours { "   " } else { "<<<" };
            out.push(format!(
                "      {mark} load #{:<4} want {:44} ours {:44} (want's first ref: {})",
                r.load, r.want, r.ours, r.first_ref
            ));
        }
    }
    out
}

pub fn apply_inversion(pulls: &[Pull], inv: &[Inverted]) -> Result<Vec<Pull>> {
    let mut out = pulls.to_vec();
    for x in inv {
        if let Some((n, e)) = &x.differs {
            bail!(
                "{}: {n} reference name(s) never pulled (e.g. {e}): not invertible",
                x.dll
            );
        }
        for r in &x.rows {
            let from = pulls
                .iter()
                .find(|p| {
                    p.kind == Kind::Import
                        && p.member.eq_ignore_ascii_case(&x.dll)
                        && resolve_eq(&p.symbol, &r.want)
                })
                .with_context(|| format!("{}!{}: no pull names it", x.dll, r.want))?;
            let slot = out
                .get_mut(r.load)
                .with_context(|| format!("load #{} is past the pulls", r.load))?;
            *slot = from.clone();
        }
    }
    Ok(out)
}

pub fn load_order_lines(pulls: &[Pull], img: &Image) -> Result<Vec<String>> {
    let blocks = blocks(img)?;
    let sets = name_sets(&blocks);
    let mut ours: BTreeMap<(String, String), (usize, String)> = BTreeMap::new();
    for (i, p) in pulls
        .iter()
        .enumerate()
        .filter(|(_, p)| p.kind == Kind::Import)
    {
        let dll = p.member.to_ascii_lowercase();
        if let Some(n) = sets.get(&dll).and_then(|s| resolve(&p.symbol, s)) {
            ours.entry((dll, n.clone()))
                .or_insert((i, p.first_ref.clone().unwrap_or_else(|| "-".into())));
        }
    }
    let mut out = vec![
        String::new(),
        "  THE REFERENCE'S LOAD ORDER (its .idata$6) against OURS: `#' is our load position, `by' our first".to_string(),
        "  referencing object. `<<<' = the reference loaded it before something we load earlier.".to_string(),
    ];
    type RunRow = (String, String, usize, usize, usize, bool);
    let flush = |out: &mut Vec<String>, r: &Option<RunRow>| {
        if let Some((dll, by, a, b, n, back)) = r {
            out.push(format!(
                "    {} {dll:13} {n:3} import(s)  ours #{a}..#{b}  by {by}",
                if *back { "<<<" } else { "   " }
            ));
        }
    };
    let mut prev: Option<usize> = None;
    let mut run: Option<RunRow> = None;
    let mut seq = 0usize;
    for (_, dll, name) in idata6(img)? {
        let Some(name) = name else { continue };
        seq += 1;
        let Some((pos, by)) = ours.get(&(dll.clone(), name.clone())) else {
            flush(&mut out, &run);
            run = None;
            out.push(format!(
                "    !!! {dll:13} {name} -- the reference loads it, our log never does"
            ));
            continue;
        };
        let back = prev.is_some_and(|p| *pos < p);
        let extend = run
            .as_ref()
            .is_some_and(|(d, b, _, last, _, _)| *d == dll && b == by && !back && *pos > *last);
        if extend {
            if let Some(r) = run.as_mut() {
                r.3 = *pos;
                r.4 += 1;
            }
        } else {
            flush(&mut out, &run);
            run = Some((dll.clone(), by.clone(), *pos, *pos, 1, back));
        }
        prev = Some(*pos);
    }
    flush(&mut out, &run);
    out.push(format!(
        "    ({seq} live import(s) in the reference's load order)"
    ));
    Ok(out)
}

pub fn six_agreement(pulls: &[Pull], img: &Image, blocks: &Blocks) -> Result<String> {
    let six = idata6(img)?;
    let sets = name_sets(blocks);
    let logged: Vec<(String, String)> = pulls
        .iter()
        .filter(|p| p.kind == Kind::Import)
        .filter_map(|p| {
            let dll = p.member.to_ascii_lowercase();
            let name = sets.get(&dll).and_then(|s| resolve(&p.symbol, s))?.clone();
            Some((dll, name))
        })
        .collect();
    let read: Vec<(String, String)> = six
        .iter()
        .filter_map(|(_, d, n)| n.as_ref().map(|n| (d.clone(), n.clone())))
        .collect();
    let same = logged
        .iter()
        .zip(read.iter())
        .take_while(|(a, b)| a == b)
        .count();
    let head = format!(
        "    .idata$6 (unsorted, = load order): the first {same} of {} hint/name pair(s) follow this log's pulls",
        read.len()
    );
    Ok(match (logged.get(same), read.get(same)) {
        (None, None) => format!("{head} -- ALL"),
        (a, b) => format!(
            "{head}; then the log loads {} where the image holds {}",
            a.map_or("-".to_string(), |(d, n)| format!("{d}!{n}")),
            b.map_or("-".to_string(), |(d, n)| format!("{d}!{n}"))
        ),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pull(member: &str, symbol: &str, kind: Kind) -> Pull {
        Pull {
            member: member.into(),
            symbol: symbol.into(),
            kind,
            first_ref: None,
        }
    }

    #[test]
    fn pull_edits_move_add_and_drop() {
        let mut p = vec![
            pull("KERNEL32.dll", "__imp__CreateFileA@28", Kind::Import),
            pull("KERNEL32.dll", "__imp__CloseHandle@4", Kind::Import),
            pull("KERNEL32.dll", "__imp__SetEndOfFile@4", Kind::Import),
        ];
        let e = parse_edits(
            "move kernel32.dll!SetEndOfFile after kernel32.dll!CreateFileA # a comment\n\
             add kernel32.dll!CreateEventA before kernel32.dll!CloseHandle\n\
             add other.dll!<null-thunk> after kernel32.dll!CloseHandle\n\
             drop kernel32.dll!CreateFileA\n",
        )
        .unwrap();
        apply_edits(&mut p, &e).unwrap();
        let s: Vec<&str> = p.iter().map(|x| x.symbol.as_str()).collect();
        assert_eq!(
            s,
            vec![
                "__imp__SetEndOfFile@4",
                "CreateEventA",
                "__imp__CloseHandle@4",
                "other.dll_NULL_THUNK_DATA"
            ]
        );
        assert_eq!(p[3].kind, Kind::NullThunk);
        assert!(parse_edits("move kernel32.dll!X").is_err());
        assert!(apply_edits(&mut p, &parse_edits("drop kernel32.dll!Nope").unwrap()).is_err());
    }

    #[test]
    fn resolve_follows_the_name_type() {
        let names: BTreeSet<String> = ["Sleep", "_Open@8", "?f@T@@QAEXXZ"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        assert_eq!(
            resolve("__imp__Sleep@4", &names).map(String::as_str),
            Some("Sleep")
        );
        assert_eq!(
            resolve("__imp__Open@8", &names).map(String::as_str),
            Some("_Open@8")
        );
        assert_eq!(
            resolve("__imp_?f@T@@QAEXXZ", &names).map(String::as_str),
            Some("?f@T@@QAEXXZ")
        );
        assert_eq!(resolve("__imp__Beep@8", &names), None);
    }

    fn two_dll_pulls() -> Vec<Pull> {
        let mut p: Vec<Pull> = (0..10)
            .map(|i| pull("KERNEL32.dll", &format!("__imp__K{i}@0"), Kind::Import))
            .collect();
        p.push(pull(
            "KERNEL32.dll",
            "__IMPORT_DESCRIPTOR_KERNEL32",
            Kind::Descriptor,
        ));
        p.push(pull(
            "KERNEL32.dll",
            "\u{7f}KERNEL32_NULL_THUNK_DATA",
            Kind::NullThunk,
        ));
        p.extend((0..5).map(|i| pull("USER32.dll", &format!("__imp__U{i}@0"), Kind::Import)));
        p.push(pull(
            "USER32.dll",
            "\u{7f}USER32_NULL_THUNK_DATA",
            Kind::NullThunk,
        ));
        p
    }

    #[test]
    fn one_sort_over_two_dlls_takes_the_ninth_kernel32_pull_first_and_rotates_user32() {
        let p = two_dll_pulls();
        let s = sorted_population(&p, Crt::Vc6, Control::Model).unwrap();
        let names: Vec<&str> = s
            .iter()
            .filter_map(|&i| p.get(i))
            .filter(|x| x.kind == Kind::Import)
            .map(|x| x.symbol.as_str())
            .collect();
        assert_eq!(
            names,
            vec![
                "__imp__K8@0",
                "__imp__K1@0",
                "__imp__K2@0",
                "__imp__K3@0",
                "__imp__K4@0",
                "__imp__K5@0",
                "__imp__K6@0",
                "__imp__K7@0",
                "__imp__K0@0",
                "__imp__K9@0",
                "__imp__U1@0",
                "__imp__U2@0",
                "__imp__U3@0",
                "__imp__U4@0",
                "__imp__U0@0",
            ]
        );
    }

    #[test]
    fn an_inverted_pull_order_sorts_into_the_wanted_iat() {
        let p = two_dll_pulls();
        let mut want: Blocks = BTreeMap::new();
        let k: Vec<String> = (0..10).rev().map(|i| format!("K{i}")).collect();
        let u: Vec<String> = ["U3", "U0", "U4", "U1", "U2"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        for (dll, names) in [("kernel32.dll", &k), ("user32.dll", &u)] {
            want.insert(
                dll.to_string(),
                names
                    .iter()
                    .enumerate()
                    .map(|(j, n)| (u32::try_from(j * 4).unwrap(), n.clone()))
                    .collect(),
            );
        }
        let s = sorted_population(&p, Crt::Vc6, Control::Model).unwrap();
        let inv = invert(&p, &s, &want);
        assert!(inv.iter().all(|x| x.differs.is_none()));
        let q = apply_inversion(&p, &inv).unwrap();
        let s2 = sorted_population(&q, Crt::Vc6, Control::Model).unwrap();
        let c = compare(&q, &s2, &want, &block_order(&q, &s2));
        assert_eq!((c.same, c.slots), (15, 15), "{:?}", c.lines());
        let again = invert(&q, &s2, &want);
        assert!(again.iter().all(|x| x.held == x.positions));
    }
}
