use super::pair::MAX_EVIDENCE;
use super::report::PAIRS_HEAD;
use anyhow::{Result, bail};
use serde::Serialize;
use std::collections::BTreeMap;

pub const EVIDENCE_COLUMN: usize = 8;

pub const SIB_COLUMN: usize = 4;

type Key = (String, String);

pub type Rows = BTreeMap<Key, Vec<String>>;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Changed {
    pub image: String,
    pub va: String,
    pub column: String,
    pub old: String,
    pub new: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct PairDiff {
    pub both: usize,
    pub same: usize,
    pub reordered: usize,
    pub capped: usize,
    pub moved: Vec<Changed>,
    pub lost: Vec<Key>,
    pub gained: Vec<Key>,
    pub changed: Vec<Changed>,
}

impl PairDiff {
    #[must_use]
    pub fn equal(&self) -> bool {
        self.moved.is_empty()
            && self.lost.is_empty()
            && self.gained.is_empty()
            && self.changed.is_empty()
    }
}

pub fn parse_pairs(text: &str) -> Result<(Vec<String>, Rows)> {
    let mut lines = text.lines().filter(|l| !l.starts_with('#'));
    let head: Vec<String> = match lines.next() {
        Some(h) if h == PAIRS_HEAD => h.split('\t').map(str::to_string).collect(),
        Some(h) => bail!("not a pairs table: its header is `{h}`"),
        None => bail!("an empty pairs table"),
    };
    let mut rows = BTreeMap::new();
    for (n, l) in lines.enumerate() {
        let c: Vec<String> = l.split('\t').map(str::to_string).collect();
        if c.len() != head.len() {
            bail!(
                "row {}: {} column(s), the header has {}",
                n.saturating_add(2),
                c.len(),
                head.len()
            );
        }
        let key = (
            c.first().cloned().unwrap_or_default(),
            c.get(1).cloned().unwrap_or_default(),
        );
        if rows.insert(key.clone(), c).is_some() {
            bail!("{} {} is paired twice", key.0, key.1);
        }
    }
    Ok((head, rows))
}

pub const ITEM_PREFIXES: [&str; 10] = [
    "str:",
    "imp:",
    "body",
    "loose",
    "shape ",
    "in ",
    "of ",
    "seed",
    "slot gap",
    "gap after ",
];

struct Evidence<'a> {
    kind: &'a str,
    votes: &'a str,
    items: Vec<String>,
}

fn item_kind(item: &str) -> &'static str {
    ITEM_PREFIXES
        .iter()
        .find(|p| item.starts_with(**p))
        .copied()
        .unwrap_or("")
}

fn items(rest: &str) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    for piece in rest.split(',') {
        match out.last_mut() {
            Some(last) if item_kind(piece).is_empty() => {
                last.push(',');
                last.push_str(piece);
            }
            _ => out.push(piece.to_string()),
        }
    }
    out
}

fn evidence(s: &str) -> Evidence<'_> {
    let mut p = s.splitn(3, ':');
    let kind = p.next().unwrap_or("");
    let votes = p.next().unwrap_or("");
    let rest = p.next().unwrap_or("");
    let mut items = if rest.is_empty() {
        Vec::new()
    } else {
        items(rest)
    };
    items.sort_unstable();
    Evidence { kind, votes, items }
}

fn kinds(items: &[String]) -> BTreeMap<&'static str, usize> {
    let mut m = BTreeMap::new();
    for i in items {
        let e = m.entry(item_kind(i)).or_insert(0usize);
        *e = e.saturating_add(1);
    }
    m
}

enum Same {
    Exact,
    Reordered,
    Capped,
    Differs,
}

fn same_evidence(a: &str, b: &str) -> Same {
    if a == b {
        return Same::Exact;
    }
    let (x, y) = (evidence(a), evidence(b));
    if x.kind != y.kind || x.votes != y.votes {
        return Same::Differs;
    }
    if x.items == y.items {
        return Same::Reordered;
    }
    let anchors = |e: &Evidence<'_>| {
        e.items
            .iter()
            .filter(|i| matches!(item_kind(i), "str:" | "imp:" | "body" | "loose"))
            .count()
    };
    if kinds(&x.items) == kinds(&y.items) && anchors(&x) >= MAX_EVIDENCE {
        return Same::Capped;
    }
    Same::Differs
}

#[must_use]
pub fn pairs(
    head: &[String],
    old: &BTreeMap<Key, Vec<String>>,
    new: &BTreeMap<Key, Vec<String>>,
) -> PairDiff {
    let mut d = PairDiff::default();
    for (k, a) in old {
        let Some(b) = new.get(k) else {
            d.lost.push(k.clone());
            continue;
        };
        d.both = d.both.saturating_add(1);
        let cell = |r: &Vec<String>, i: usize| r.get(i).cloned().unwrap_or_default();
        if cell(a, SIB_COLUMN) != cell(b, SIB_COLUMN) {
            d.moved.push(Changed {
                image: k.0.clone(),
                va: k.1.clone(),
                column: "sib_va".into(),
                old: cell(a, SIB_COLUMN),
                new: cell(b, SIB_COLUMN),
            });
            continue;
        }
        let mut clean = true;
        for (i, name) in head.iter().enumerate() {
            let (x, y) = (cell(a, i), cell(b, i));
            if i == EVIDENCE_COLUMN {
                match same_evidence(&x, &y) {
                    Same::Exact => {}
                    Same::Reordered => d.reordered = d.reordered.saturating_add(1),
                    Same::Capped => d.capped = d.capped.saturating_add(1),
                    Same::Differs => clean = false,
                }
                if matches!(same_evidence(&x, &y), Same::Differs) {
                    d.changed.push(Changed {
                        image: k.0.clone(),
                        va: k.1.clone(),
                        column: name.clone(),
                        old: x,
                        new: y,
                    });
                }
                continue;
            }
            if x != y {
                clean = false;
                d.changed.push(Changed {
                    image: k.0.clone(),
                    va: k.1.clone(),
                    column: name.clone(),
                    old: x,
                    new: y,
                });
            }
        }
        if clean {
            d.same = d.same.saturating_add(1);
        }
    }
    d.gained = new
        .keys()
        .filter(|k| !old.contains_key(*k))
        .cloned()
        .collect();
    d
}

#[must_use]
pub fn first_difference(a: &str, b: &str) -> Option<(usize, String, String)> {
    let keep = |t: &str| -> Vec<String> {
        t.lines()
            .filter(|l| !l.starts_with('#'))
            .map(str::to_string)
            .collect()
    };
    let (x, y) = (keep(a), keep(b));
    for i in 0..x.len().max(y.len()) {
        let (p, q) = (x.get(i), y.get(i));
        if p != q {
            return Some((
                i.saturating_add(1),
                p.cloned().unwrap_or_else(|| "(end)".into()),
                q.cloned().unwrap_or_else(|| "(end)".into()),
            ));
        }
    }
    None
}
