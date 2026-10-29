pub mod feat;
pub mod files;
pub mod names;
pub mod pair;
pub mod ppc;
pub mod side;
pub mod types;
pub mod xmem;

use crate::mangle::SigLevel;
use anyhow::Result;
use names::{Chain, Context, MapRow, Twin, World};
use pair::{Pairs, Rules, Side};
use side::{Ours, Source};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug)]
pub struct Inputs<'a> {
    pub sources: &'a [Source],
    pub sides: Vec<Side>,
    pub twins: BTreeMap<String, Side>,
    pub ours: &'a Side,
    pub info: &'a Ours,
    pub rules: Rules,
}

#[derive(Debug)]
pub struct Outputs {
    pub sides: Vec<Side>,
    pub direct: Vec<Pairs>,
    pub chains: Vec<Option<Chain>>,
    pub twins: BTreeMap<String, Twin>,
    pub log: Vec<String>,
}

#[must_use]
pub fn pair_all(i: Inputs<'_>) -> Outputs {
    let mut log = Vec::new();
    let mut twins: BTreeMap<String, Twin> = BTreeMap::new();
    for (t, side) in i.twins {
        let p = pair::pair_with(&side, i.ours, &[], false, i.rules);
        log.push(format!(
            "twin {t}: {} functions, {} anchor pairs with ours",
            side.funs.len(),
            p.a2b.len()
        ));
        twins.insert(t, Twin { side, pairs: p });
    }
    let chains: Vec<Option<Chain>> = i
        .sources
        .iter()
        .zip(i.sides.iter())
        .map(|(src, s)| {
            let t = src.twin.as_ref()?;
            let tw = twins.get(t)?;
            Some(Chain {
                twin: t.clone(),
                pairs: pair::pair_with(s, &tw.side, &[], true, i.rules),
            })
        })
        .collect();
    let mut direct = Vec::new();
    for (s, ch) in i.sides.iter().zip(chains.iter()) {
        let mut seeds: Vec<(usize, usize)> = Vec::new();
        if let Some(ch) = ch
            && let Some(tw) = twins.get(&ch.twin)
        {
            let mut used: BTreeSet<usize> = BTreeSet::new();
            for (m, t) in &ch.pairs.a2b {
                let Some(o) = tw.pairs.a2b.get(t) else {
                    continue;
                };
                let (Some(e1), Some(e2)) = (ch.pairs.why.get(m), tw.pairs.why.get(t)) else {
                    continue;
                };
                if names::strong(e1)
                    && names::strong(e2)
                    && pair::gate(s, *m, i.ours, *o) != SigLevel::Mismatch
                    && used.insert(*o)
                {
                    seeds.push((*m, *o));
                }
            }
        }
        let p = pair::pair_with(s, i.ours, &seeds, true, i.rules);
        log.push(format!(
            "{}: {} named functions, {} seeds from its chain, {} direct pairs, {} with its twin, {} held for a second link",
            s.label,
            s.funs.len(),
            seeds.len(),
            p.a2b.len(),
            ch.as_ref().map_or(0, |c| c.pairs.a2b.len()),
            p.held_twins.len()
        ));
        direct.push(p);
    }
    Outputs {
        sides: i.sides,
        direct,
        chains,
        twins,
        log,
    }
}

#[derive(Debug)]
pub struct Decisions<'a> {
    pub context: Context<'a>,
    pub prior: &'a [MapRow],
    pub measured: Option<&'a str>,
}

pub fn decide(
    o: &Outputs,
    sources: &[Source],
    ours: &Side,
    info: &Ours,
    d: &Decisions<'_>,
) -> Result<(Vec<MapRow>, Vec<String>)> {
    let w = World {
        sources,
        sides: &o.sides,
        direct: &o.direct,
        chains: &o.chains,
        twins: &o.twins,
        ours,
        info,
    };
    let links = names::collect_links(&w);
    let mut rows = names::decide(&w, &links, &d.context);
    let stale = match d.measured {
        Some(t) => names::apply_measured(&mut rows, t)?,
        None => Vec::new(),
    };
    names::restore_history(&mut rows, d.prior);
    names::sync_mac_rows(&mut rows);
    Ok((rows, stale))
}
