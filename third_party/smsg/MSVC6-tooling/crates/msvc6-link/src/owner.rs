use crate::growth::{round16, slot};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const VENDOR_CLASSES: [&str; 7] = [
    "ios@@",
    "istream@@",
    "ostream@@",
    "streambuf@@",
    "filebuf@@",
    "fstream@@",
    "ifstream@@",
];

pub const SPLIT_ROWS: usize = 10;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Body {
    pub name: String,
    pub rva: u32,
    pub size: u64,
}

fn after(rva: u32, n: u64) -> u32 {
    rva.saturating_add(u32::try_from(n).unwrap_or(u32::MAX))
}

fn reserved_end(b: &Body) -> u32 {
    after(b.rva, slot(b.size))
}

fn tight_end(b: &Body) -> u32 {
    after(b.rva, round16(b.size))
}

#[must_use]
pub fn fill_bodies(chain: &[(u32, u64)]) -> BTreeSet<u32> {
    let mut sorted = chain.to_vec();
    sorted.sort_by_key(|(r, _)| *r);
    let mut out = BTreeSet::new();
    let mut cursor = sorted.first().map_or(0, |c| c.0);
    for (r, size) in &sorted {
        if *r >= cursor {
            cursor = after(*r, slot(*size));
        } else {
            out.insert(*r);
        }
    }
    out
}

#[must_use]
pub fn symbol_base_name(sym: &str) -> String {
    if let Some(dec) = sym.strip_prefix('?') {
        return dec.split('@').next().unwrap_or("").to_string();
    }
    let c = sym.strip_prefix('_').unwrap_or(sym);
    c.split('@').next().unwrap_or("").to_string()
}

#[must_use]
pub fn is_vendor(name: &str, archived: &BTreeSet<String>, classes: &[String]) -> bool {
    archived.contains(name) || classes.iter().any(|c| name.contains(c.as_str()))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct BlockFacts {
    pub exclusive: bool,
    pub has_relocated: bool,
    pub at_chain_edge: bool,
    pub bracketed: bool,
    pub neighbours_informative: bool,
    pub all_header_inline: bool,
    pub whole_contribution: bool,
}

#[must_use]
pub fn block_verdict(f: &BlockFacts) -> &'static str {
    if !f.exclusive {
        "FOLD   -- shared bodies, owner decided by the fold; says NOTHING"
    } else if f.has_relocated {
        "RELOC  -- a body here is listed as RELOCATED, so bracketing says nothing about its object"
    } else if f.at_chain_edge {
        "EDGE   -- at the chain's edge, bracketed on ONE side; suggestive only"
    } else if f.bracketed && f.neighbours_informative {
        if f.all_header_inline {
            "INLINE -- every body here is defined IN-CLASS IN A HEADER, so the FOLD decides its owner and no source \
             move re-homes it; bracketing cannot force anything"
        } else if f.whole_contribution {
            "WHOLE  -- bracketed, but this block is the object's ENTIRE contribution, which ONE group relocation puts \
             anywhere -- not forced"
        } else {
            "FORCED -- exclusive AND bracketed both sides by one other owner's OWN bodies"
        }
    } else if f.bracketed {
        "WEAK   -- bracketed both sides, but a neighbour is FOLD or RELOC and cannot bracket anything"
    } else {
        "EDGE   -- exclusive but neighbours differ, so no single run brackets it"
    }
}

fn longest_chain(bodies: &[Body]) -> (usize, usize) {
    let mut best = (0usize, 0usize);
    for (i, s) in bodies.iter().enumerate() {
        let mut n = 0usize;
        let mut cursor = s.rva;
        for b in bodies.iter().skip(i) {
            if b.rva != cursor {
                break;
            }
            n += 1;
            let (reserved, tight) = (reserved_end(b), tight_end(b));
            cursor = match bodies.get(i + n) {
                Some(next) if next.rva == tight && tight != reserved => tight,
                _ => reserved,
            };
        }
        if n > best.0 {
            best = (n, i);
        }
    }
    best
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Span {
    pub object: String,
    pub members: usize,
    pub first: u32,
    pub end: u32,
    pub span: u64,
    pub slots: u64,
    pub longest: usize,
    pub divergence: Option<(u32, u32)>,
}

impl Span {
    #[must_use]
    pub fn contiguous(&self) -> bool {
        self.longest == self.members
    }

    #[must_use]
    pub fn closed(&self) -> bool {
        self.contiguous() && self.slots == self.span
    }

    #[must_use]
    pub fn verdict(&self) -> String {
        if self.closed() {
            "CONTIGUOUS -- each member one slot after the last, so the run admits no further body"
                .to_string()
        } else if self.contiguous() {
            "CONTIGUOUS yet the sums DISAGREE -- a defect in this model, not a finding about the image".to_string()
        } else {
            let where_ = self.divergence.map_or_else(
                || "the chain ends at the last member".to_string(),
                |(want, got)| format!("chain breaks at {got:#08x}, expected {want:#08x}"),
            );
            format!(
                "LONGEST CHAIN {} of {}, {} outlier(s) -- {where_}. An outlier is EITHER a body ILINK relocated OR one \
                 we attribute to the wrong object",
                self.longest,
                self.members,
                self.members - self.longest
            )
        }
    }
}

#[must_use]
pub fn span_of(object: &str, bodies: &[Body]) -> Option<Span> {
    let (first, last) = (bodies.first()?, bodies.last()?);
    let (longest, at) = longest_chain(bodies);
    let divergence = bodies.get(at + longest).map(|b| {
        let want = bodies
            .get((at + longest).saturating_sub(1))
            .map_or(0, reserved_end);
        (want, b.rva)
    });
    let end = after(last.rva, last.size);
    let slots = bodies
        .iter()
        .take(bodies.len().saturating_sub(1))
        .map(|b| slot(b.size))
        .sum::<u64>()
        .saturating_add(last.size);
    Some(Span {
        object: object.to_string(),
        members: bodies.len(),
        first: first.rva,
        end,
        span: u64::from(end.saturating_sub(first.rva)),
        slots,
        longest,
        divergence,
    })
}

#[derive(Debug, Clone)]
pub struct Input<'a> {
    pub label: &'a str,
    pub per: &'a BTreeMap<String, Vec<Body>>,
    pub defined_by: &'a BTreeMap<String, BTreeSet<String>>,
    pub objects: &'a [String],
    pub relocated_by_fill: &'a BTreeSet<u32>,
    pub archived: &'a BTreeSet<String>,
    pub vendor_classes: &'a [String],
    pub defined_in_cpp: &'a BTreeSet<String>,
    pub chain_at: Option<u32>,
}

type Flat<'a> = (u32, u64, &'a str, &'a str);

fn union_chains(all: &[Flat<'_>]) -> Vec<(usize, usize)> {
    let mut chains = Vec::new();
    let mut i = 0usize;
    while let Some(&(base, _, _, _)) = all.get(i) {
        let mut n = 0usize;
        let mut cursor = base;
        while let Some((rva, size, _, _)) = all.get(i + n) {
            if *rva != cursor {
                break;
            }
            n += 1;
            let reserved = after(*rva, slot(*size));
            let tight = after(*rva, round16(*size));
            cursor = match all.get(i + n) {
                Some(next) if next.0 == tight && tight != reserved => tight,
                _ => reserved,
            };
        }
        chains.push((i, n.max(1)));
        i += n.max(1);
    }
    chains
}

fn isolated(per: &BTreeMap<String, Vec<Body>>) -> Vec<(u32, String, String, u64, usize)> {
    let adjacent = |p: &Body, b: &Body| b.rva == reserved_end(p) || b.rva == tight_end(p);
    let mut out = Vec::new();
    for (obj, bodies) in per {
        if bodies.len() < 2 {
            continue;
        }
        for (i, b) in bodies.iter().enumerate() {
            let prev = i
                .checked_sub(1)
                .and_then(|j| bodies.get(j))
                .is_some_and(|p| adjacent(p, b));
            let next = bodies.get(i + 1).is_some_and(|n| adjacent(b, n));
            if !prev && !next {
                let followers = bodies.iter().filter(|o| o.rva > b.rva).count();
                out.push((b.rva, obj.clone(), b.name.clone(), b.size, followers));
            }
        }
    }
    out.sort_by_key(|r| r.0);
    out
}

fn groups(per: &BTreeMap<String, Vec<Body>>) -> Vec<(u32, String, usize, u64)> {
    let mut out = Vec::new();
    for (obj, bodies) in per {
        let mut chains: Vec<(usize, usize)> = Vec::new();
        let mut i = 0usize;
        while i < bodies.len() {
            let mut n = 1usize;
            while let (Some(a), Some(b)) = (bodies.get(i + n - 1), bodies.get(i + n)) {
                let res = reserved_end(a);
                if b.rva == res || b.rva == tight_end(a) || (b.rva > a.rva && b.rva < res) {
                    n += 1;
                } else {
                    break;
                }
            }
            chains.push((i, n));
            i += n;
        }
        if chains.len() < 2 {
            continue;
        }
        let longest = chains.iter().map(|(_, n)| *n).max().unwrap_or(0);
        let mut kept = false;
        for (st, n) in &chains {
            if *n == longest && !kept {
                kept = true;
                continue;
            }
            if let Some(b) = bodies.get(*st) {
                out.push((b.rva, obj.clone(), *n, b.size));
            }
        }
    }
    out.sort_by_key(|r| r.0);
    out
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn report(i: &Input<'_>) -> Vec<String> {
    let deduped: usize = i.per.values().map(Vec::len).sum();
    let header_inline = |dec: &str| {
        !i.defined_in_cpp.is_empty() && !i.defined_in_cpp.contains(&symbol_base_name(dec))
    };
    let mut out = vec![
        format!("{}: WHICH OBJECT THE REFERENCE'S LAYOUT SAYS EACH BODY BELONGS TO", i.label),
        String::new(),
        format!(
            "  POPULATION: {deduped} fold-deduped own COMDAT(s) over {} object(s), first definer in link order.",
            i.per.len()
        ),
        "  SLOT = round16(body * 5/4); the LAST member of an object is TIGHT, because ILINK extends a contribution \
         rather than reserving past its end."
            .to_string(),
        String::new(),
    ];
    let mut spans: Vec<Span> = Vec::new();
    for (obj, bodies) in i.per {
        let Some(s) = span_of(obj, bodies) else {
            continue;
        };
        out.push(format!(
            "  {obj:<14} {:3} member(s)  {:#08x}..{:#08x}  span {:6}  slots {:6}",
            s.members, s.first, s.end, s.span, s.slots
        ));
        out.push(format!("                 {}", s.verdict()));
        if s.contiguous() {
            spans.push(s);
        }
    }
    let mut all: Vec<Flat<'_>> = i
        .per
        .iter()
        .flat_map(|(obj, bodies)| {
            bodies
                .iter()
                .map(move |b| (b.rva, b.size, obj.as_str(), b.name.as_str()))
        })
        .collect();
    all.sort_by_key(|(r, _, _, _)| *r);
    let chains = union_chains(&all);
    let mut split: Vec<(u32, usize, Vec<&str>)> = Vec::new();
    for (start, len) in &chains {
        let mut blocks: Vec<&str> = Vec::new();
        for (_, _, o, _) in all.iter().skip(*start).take(*len) {
            if blocks.last() != Some(o) {
                blocks.push(o);
            }
        }
        let mut uniq = blocks.clone();
        uniq.sort_unstable();
        uniq.dedup();
        if blocks.len() > uniq.len()
            && let Some((rva, _, _, _)) = all.get(*start)
        {
            split.push((*rva, *len, blocks));
        }
    }
    let iso = isolated(i.per);
    let grp = groups(i.per);
    let grows: Vec<String> = grp
        .iter()
        .map(|(rva, obj, n, size)| {
            let already = i.relocated_by_fill.contains(rva) || iso.iter().any(|r| r.0 == *rva);
            format!(
                "    {rva:#08x}  {obj:<13} {n:2} body(ies)  first is {size:5} bytes  {}",
                if already { "already listed" } else { "NEW" }
            )
        })
        .collect();
    out.push(String::new());
    out.push(format!(
        "  RELOCATED GROUPS -- {} same-owner chain(s) that are NOT their object's longest, so each was moved as a \
         GROUP (the population the isolation test cannot see):",
        grows.len()
    ));
    out.extend(grows);
    let mut relocated = i.relocated_by_fill.clone();
    relocated.extend(iso.iter().map(|r| r.0));
    out.push(String::new());
    out.push(format!(
        "  UNION CHAINS -- {} maximal chain(s) over {} own COMDAT(s); {} carry bodies we attribute to INTERLEAVED \
         owners (A, then B, then A again), which LINK cannot produce:",
        chains.len(),
        all.len(),
        split.len()
    ));
    split.sort_by_key(|(r, l, _)| (std::cmp::Reverse(*l), *r));
    for (rva, len, blocks) in split.iter().take(SPLIT_ROWS) {
        out.push(format!(
            "    {rva:#08x}  {len:3} consecutive bodies, owners INTERLEAVE as: {}",
            blocks.join(" -> ")
        ));
    }
    out.push(String::new());
    out.push(
        "  THE SAME BLOCKS RANKED BY WHAT THE EVIDENCE CAN FORCE (FORCED: exclusive bodies bracketed on both sides by \
         one other owner's informative bodies, and not the owner's whole contribution):"
            .to_string(),
    );
    let exclusive = |name: &str| {
        i.defined_by
            .values()
            .filter(|set| set.contains(name))
            .count()
            == 1
    };
    let mut totals: BTreeMap<&str, usize> = BTreeMap::new();
    for t in &all {
        *totals.entry(t.2).or_insert(0) += 1;
    }
    for (rva, _, _) in split.iter().take(SPLIT_ROWS) {
        let Some((start, len)) = chains
            .iter()
            .find(|(st, _)| all.get(*st).is_some_and(|(r, _, _, _)| r == rva))
        else {
            continue;
        };
        let mut blocks: Vec<(&str, usize, usize)> = Vec::new();
        for (k, (_, _, owner, _)) in all.iter().enumerate().skip(*start).take(*len) {
            match blocks.last_mut() {
                Some(b) if b.0 == *owner => b.2 = k,
                _ => blocks.push((owner, k, k)),
            }
        }
        let nb = blocks.len();
        let excl =
            |lo: usize, hi: usize| (lo..=hi).all(|k| all.get(k).is_some_and(|t| exclusive(t.3)));
        let moved = |lo: usize, hi: usize| {
            (lo..=hi).any(|k| all.get(k).is_some_and(|t| relocated.contains(&t.0)))
        };
        let informative: Vec<bool> = blocks
            .iter()
            .map(|(_, lo, hi)| excl(*lo, *hi) && !moved(*lo, *hi))
            .collect();
        for (bi, (owner, lo, hi)) in blocks.iter().enumerate() {
            let prev = bi.checked_sub(1).and_then(|k| blocks.get(k)).map(|b| b.0);
            let next = blocks.get(bi + 1).map(|b| b.0);
            let verdict = block_verdict(&BlockFacts {
                exclusive: excl(*lo, *hi),
                has_relocated: moved(*lo, *hi),
                at_chain_edge: bi == 0 || bi + 1 == nb,
                bracketed: prev == next,
                neighbours_informative: bi
                    .checked_sub(1)
                    .and_then(|k| informative.get(k))
                    .copied()
                    .unwrap_or(false)
                    && informative.get(bi + 1).copied().unwrap_or(false),
                all_header_inline: (*lo..=*hi)
                    .all(|k| all.get(k).is_some_and(|t| header_inline(t.3))),
                whole_contribution: totals.get(owner).copied().unwrap_or(0) == hi - lo + 1,
            });
            let at = all.get(*lo).map_or(0, |t| t.0);
            out.push(format!(
                "    {rva:#08x} block {:2}  {at:#08x}  {:3} body(ies)  {owner:<13} {verdict}",
                bi + 1,
                hi - lo + 1
            ));
        }
    }
    if let Some(want) = i.chain_at {
        match chains
            .iter()
            .find(|(st, _)| all.get(*st).is_some_and(|t| t.0 == want))
        {
            None => {
                out.push(String::new());
                out.push(format!(
                    "  --chain {want:#08x}: NO CHAIN STARTS THERE (a chain start is the first body of a maximal run)."
                ));
            }
            Some((st, len)) => {
                out.push(String::new());
                out.push(format!(
                    "  CHAIN AT {want:#08x} -- {len} body(ies), and WHICH OBJECTS DEFINE EACH:"
                ));
                let mut counts: BTreeMap<&str, usize> = BTreeMap::new();
                for (rva, size, owner, name) in all.iter().skip(*st).take(*len) {
                    let definers: Vec<&str> = i
                        .objects
                        .iter()
                        .map(String::as_str)
                        .filter(|stem| i.defined_by.get(*stem).is_some_and(|s| s.contains(*name)))
                        .collect();
                    for d in &definers {
                        *counts.entry(d).or_default() += 1;
                    }
                    let short: String = name.chars().take(44).collect();
                    out.push(format!(
                        "    {rva:#08x} {size:5}  fold->{owner:<13} defined by {:<28} {short}",
                        definers.join(",")
                    ));
                }
                out.push(String::new());
                out.push(
                    "  DEFINITION COUNTS over this chain -- the object that defines ALL of them is the only one that \
                     could have emitted it as one contribution:"
                        .to_string(),
                );
                for (o, n) in &counts {
                    out.push(format!("    {o:<14} defines {n:3} of {len}"));
                }
            }
        }
    }
    let interleaved: usize = split.iter().map(|(_, l, _)| *l).sum();
    out.push(format!(
        "  {interleaved} of {} own COMDAT(s) sit in a chain whose owners INTERLEAVE.",
        all.len()
    ));
    let mut foreign: Vec<(u32, &str, &str, &str)> = Vec::new();
    for (obj, bodies) in i.per {
        for b in bodies {
            for s in spans.iter().filter(|s| s.closed() && s.object != *obj) {
                if b.rva >= s.first && b.rva < s.end {
                    foreign.push((b.rva, s.object.as_str(), obj.as_str(), b.name.as_str()));
                }
            }
        }
    }
    foreign.sort_by_key(|r| r.0);
    let vendor = |n: &str| is_vendor(n, i.archived, i.vendor_classes);
    out.push(String::new());
    if foreign.is_empty() {
        out.push(
            "  NO FOREIGN BODY FOUND IN ANY CONTIGUOUS SPAN: consistent with our attribution, not proof of it."
                .to_string(),
        );
    } else {
        out.push(format!(
            "  FOREIGN BODIES -- {} address(es) inside a CONTIGUOUS span whose owner did NOT emit them; ILINK \
             relocated them there:",
            foreign.len()
        ));
        for (rva, span_obj, our_obj, name) in &foreign {
            let short: String = name.chars().take(46).collect();
            out.push(format!(
                "    {rva:#08x}  inside {span_obj}'s CONTIGUOUS span, we attribute it to {our_obj:<13} {short}{}",
                if vendor(name) {
                    "  LIBRARY'S -- fold order decides its owner, no source edit of ours can re-home it"
                } else {
                    ""
                }
            ));
        }
    }
    let rows: Vec<String> = iso
        .iter()
        .map(|(rva, obj, name, size, followers)| {
            let short: String = name.chars().take(40).collect();
            format!(
                "    {rva:#08x}  {obj:<13} {:<12}  b_final {size:5}  followers {followers:3}  {short}{}",
                if i.relocated_by_fill.contains(rva) {
                    "in fill set"
                } else {
                    "NEW to fill"
                },
                if vendor(name) { "  LIBRARY'S" } else { "" }
            )
        })
        .collect();
    let also = iso
        .iter()
        .filter(|r| i.relocated_by_fill.contains(&r.0))
        .count();
    out.push(String::new());
    out.push(format!(
        "  RELOCATED BY ISOLATION -- {} body(ies) with NO same-owner sibling one slot away on either side; {also} of \
         them the fill walk also lists, {} are NEW (a lower bound: a pair relocated together passes).",
        rows.len(),
        rows.len() - also
    ));
    out.extend(rows);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn forcible() -> BlockFacts {
        BlockFacts {
            exclusive: true,
            has_relocated: false,
            at_chain_edge: false,
            bracketed: true,
            neighbours_informative: true,
            all_header_inline: false,
            whole_contribution: false,
        }
    }

    #[test]
    fn every_rung_above_forced_outranks_it_and_a_partial_block_is_forced() {
        assert!(block_verdict(&forcible()).starts_with("FORCED"));
        let whole = BlockFacts {
            whole_contribution: true,
            ..forcible()
        };
        assert!(block_verdict(&whole).starts_with("WHOLE"));
        for (f, want) in [
            (
                BlockFacts {
                    exclusive: false,
                    ..forcible()
                },
                "FOLD",
            ),
            (
                BlockFacts {
                    has_relocated: true,
                    ..forcible()
                },
                "RELOC",
            ),
            (
                BlockFacts {
                    at_chain_edge: true,
                    ..forcible()
                },
                "EDGE",
            ),
            (
                BlockFacts {
                    all_header_inline: true,
                    ..forcible()
                },
                "INLINE",
            ),
            (
                BlockFacts {
                    neighbours_informative: false,
                    ..forcible()
                },
                "WEAK",
            ),
        ] {
            assert!(block_verdict(&f).starts_with(want), "{want}");
        }
    }

    #[test]
    fn an_extern_c_name_reads_without_its_prefix_and_suffix() {
        assert_eq!(symbol_base_name("?getFlag@Tile@@QAE_NXZ"), "getFlag");
        assert_eq!(symbol_base_name("_GetRectWidth"), "GetRectWidth");
        assert_eq!(symbol_base_name("_DllMain@12"), "DllMain");
    }

    fn body(name: &str, rva: u32, size: u64) -> Body {
        Body {
            name: name.to_string(),
            rva,
            size,
        }
    }

    #[test]
    fn a_run_closes_only_when_its_last_member_is_tight() {
        let fb: [u64; 7] = [48, 36, 0x37, 0x4ac, 0xd4, 0x14f, 0x1e0];
        let mut bodies = Vec::new();
        let mut at = 0x1fa0u32;
        for (k, s) in fb.iter().enumerate() {
            bodies.push(body(&format!("f{k}"), at, *s));
            at = after(at, slot(*s));
        }
        let s = span_of("X", &bodies).unwrap();
        assert_eq!(s.span, 0x2ae0 - 0x1fa0);
        assert_eq!(s.slots, 2880);
        assert!(s.closed());
        let wrong: u64 = fb.iter().map(|b| slot(*b)).sum();
        assert_ne!(wrong, 2880);
    }

    #[test]
    fn a_body_inside_its_predecessors_reserve_is_a_fill_body() {
        let chain = [(0x1000, 100), (0x1090, 50), (0x10a0, 10), (0x10e0, 4)];
        assert_eq!(fill_bodies(&chain), [0x10a0].into());
    }

    #[test]
    fn a_tight_step_continues_a_chain_and_an_outlier_breaks_it() {
        let bodies = vec![
            body("a", 0x100, 39),
            body("b", 0x130, 16),
            body("c", 0x150, 16),
            body("far", 0x900, 16),
        ];
        let s = span_of("X", &bodies).unwrap();
        assert_eq!(s.longest, 3);
        assert_eq!(s.divergence, Some((0x170, 0x900)));
        assert!(!s.contiguous());
    }

    #[test]
    fn the_report_names_an_interleaved_chain_and_a_foreign_body() {
        let mut per: BTreeMap<String, Vec<Body>> = BTreeMap::new();
        per.insert(
            "A".to_string(),
            vec![
                body("a1", 0x1000, 16),
                body("a2", 0x1040, 16),
                body("a3", 0x1060, 10),
            ],
        );
        per.insert("B".to_string(), vec![body("b1", 0x1020, 16)]);
        let defined_by: BTreeMap<String, BTreeSet<String>> = per
            .iter()
            .map(|(o, bs)| (o.clone(), bs.iter().map(|b| b.name.clone()).collect()))
            .collect();
        let rvas: BTreeSet<u32> = BTreeSet::new();
        let names: BTreeSet<String> = BTreeSet::new();
        let lines = report(&Input {
            label: "T",
            per: &per,
            defined_by: &defined_by,
            objects: &["A".to_string(), "B".to_string()],
            relocated_by_fill: &rvas,
            archived: &names,
            vendor_classes: &[],
            defined_in_cpp: &names,
            chain_at: Some(0x1000),
        });
        let text = lines.join("\n");
        assert!(text.contains("owners INTERLEAVE as: A -> B -> A"), "{text}");
        assert!(text.contains("CHAIN AT 0x001000 -- 4 body(ies)"), "{text}");
    }
}
