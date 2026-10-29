use anyhow::{Result, bail};
use msvc6_diff::env::Env;
use msvc6_diff::seat::{self, scope_stem};
use msvc6_project::mapfile::Map;
use std::collections::BTreeMap;
use std::path::PathBuf;

pub const DULL: u64 = 32;

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Placed {
    pub rva: u32,
    pub ours: Option<u32>,
    pub name: String,
    pub object: String,
}

#[derive(Debug, Default)]
pub struct Report {
    pub population: usize,
    pub objects: usize,
    pub comparable: bool,
    pub per: BTreeMap<String, (usize, usize)>,
    pub at_addr: BTreeMap<String, (usize, usize)>,
    pub exact: usize,
    pub dull_exact: usize,
    pub bytes_ok: u64,
    pub bytes_tot: u64,
    pub misplaced: Vec<(u32, String, String)>,
    pub placement: Vec<Placed>,
    pub with_map: bool,
    pub refused: Vec<(PathBuf, String)>,
}

impl Report {
    #[must_use]
    pub fn at_address(&self) -> (usize, usize) {
        self.at_addr
            .values()
            .fold((0, 0), |(a, b), (x, y)| (a + x, b + y))
    }
}

pub fn measure(env: &Env, ours: &[u8], objs: &[PathBuf], map: Option<&Map>) -> Result<Report> {
    if objs.is_empty() {
        bail!("no objects");
    }
    let refbytes = env.img.bytes();
    let sec = env.section();
    let mut rep = Report {
        comparable: refbytes.len() == ours.len(),
        objects: objs.len(),
        with_map: map.is_some(),
        ..Report::default()
    };
    let mut own: BTreeMap<String, (String, u32, u64)> = BTreeMap::new();
    for o in objs {
        let stem = scope_stem(o);
        match seat::derive(env, o) {
            Ok((_, members)) => {
                for m in members {
                    if let Some(r) = m.rva {
                        own.entry(m.name)
                            .or_insert((stem.clone(), r, m.size as u64));
                    }
                }
            }
            Err(e) => rep.refused.push((o.clone(), format!("{e:#}"))),
        }
    }
    rep.population = own.len();
    let ourva: BTreeMap<String, u32> = map
        .map(|m| {
            m.symbols
                .iter()
                .filter(|s| s.section == 1)
                .map(|s| (s.name.clone(), s.rva))
                .collect()
        })
        .unwrap_or_default();
    let base = sec.rva.get();
    let raw = sec.raw_off.get();
    for (name, (obj, rva, size)) in &own {
        let off = raw.saturating_add(rva.saturating_sub(base) as usize);
        let Ok(n) = usize::try_from(*size) else {
            continue;
        };
        let a = refbytes.get(off..off.saturating_add(n));
        let b = ours.get(off..off.saturating_add(n));
        let e = rep.per.entry(obj.clone()).or_insert((0, 0));
        e.1 += 1;
        if map.is_some() {
            let at = rep.at_addr.entry(obj.clone()).or_insert((0, 0));
            at.1 += 1;
            if ourva.get(name) == Some(rva) {
                at.0 += 1;
            }
            rep.placement.push(Placed {
                rva: *rva,
                ours: ourva.get(name).copied(),
                name: name.clone(),
                object: obj.clone(),
            });
        }
        rep.bytes_tot = rep.bytes_tot.saturating_add(*size);
        match (a, b) {
            (Some(a), Some(b)) if rep.comparable => {
                rep.bytes_ok = rep
                    .bytes_ok
                    .saturating_add(a.iter().zip(b).filter(|(x, y)| x == y).count() as u64);
                if a == b {
                    rep.exact += 1;
                    if let Some(e) = rep.per.get_mut(obj) {
                        e.0 += 1;
                    }
                    if *size <= DULL {
                        rep.dull_exact += 1;
                    }
                } else {
                    rep.misplaced.push((*rva, obj.clone(), name.clone()));
                }
            }
            _ => rep.misplaced.push((*rva, obj.clone(), name.clone())),
        }
    }
    rep.misplaced.sort_by_key(|(r, _, _)| *r);
    rep.placement.sort_by_key(|p| p.rva);
    Ok(rep)
}

fn clip(s: &str, n: usize) -> &str {
    let end = s
        .char_indices()
        .map(|(i, c)| i + c.len_utf8())
        .take_while(|e| *e <= n)
        .last()
        .unwrap_or(0);
    s.get(..end).unwrap_or(s)
}

#[must_use]
pub fn lines(label: &str, ours: &str, rep: &Report, list: bool) -> Vec<String> {
    let mut out = vec![
        format!("{label}: OWN COMDATs BYTE-IDENTICAL AT THEIR REFERENCE ADDRESS in {ours}"),
        format!(
            "  POPULATION: {} fold-deduped own COMDAT(s) over {} object(s); a name several TUs instantiate is owned by the FIRST object in link order",
            rep.population, rep.objects
        ),
    ];
    if !rep.comparable {
        out.push(
            "  SEATED is UNAVAILABLE: the two images differ in size, and it reads ours at the reference's file offsets; AT ADDRESS needs only the map".to_string(),
        );
    }
    for (obj, (ok, tot)) in &rep.per {
        let at = rep.at_addr.get(obj).map_or_else(
            || "  at address UNKNOWN (no --map)".to_string(),
            |(a, t)| format!("  {a:3} of {t:3} AT ADDRESS"),
        );
        out.push(format!("  {obj:<14} {ok:3} of {tot:3} seated {at}"));
    }
    let (aok, atot) = rep.at_address();
    if rep.with_map {
        out.push(format!("  AT ADDRESS: {aok} of {atot}"));
    } else {
        out.push("  AT ADDRESS: UNKNOWN -- pass --map from a LINK /MAP".to_string());
    }
    if list {
        if rep.placement.is_empty() {
            out.push("  --list-placement needs --map".to_string());
        } else {
            let (mut above, mut below, mut same, mut unk) = (0usize, 0usize, 0usize, 0usize);
            for p in &rep.placement {
                match p.ours {
                    Some(o) if o == p.rva => same += 1,
                    Some(o) if o < p.rva => below += 1,
                    Some(_) => above += 1,
                    None => unk += 1,
                }
                let d = p.ours.map_or_else(
                    || "        ?  NOT IN MAP".to_string(),
                    |o| format!("{o:#08x}  delta {:+}", i64::from(o) - i64::from(p.rva)),
                );
                out.push(format!(
                    "    ref {:#08x}  ours {d}   {:<34} {}",
                    p.rva,
                    clip(&p.name, 32),
                    p.object
                ));
            }
            out.push(format!(
                "    AT ADDRESS {same}   we place it EARLIER {below}   LATER {above}   absent from the map {unk}"
            ));
        }
    }
    out.push(format!(
        "  SEATED: {} of {} own COMDAT(s), of which {} are <= {DULL} bytes and so could agree by luck.",
        rep.exact, rep.population, rep.dull_exact
    ));
    out.push(format!(
        "  own-COMDAT bytes matching in place: {} of {}",
        rep.bytes_ok, rep.bytes_tot
    ));
    if !rep.misplaced.is_empty() {
        out.push("  first few NOT seated, in reference-rva order:".to_string());
        for (r, obj, name) in rep.misplaced.iter().take(8) {
            out.push(format!("    {r:#08x}  {obj:<14} {name}"));
        }
    }
    out
}
