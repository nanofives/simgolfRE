use anyhow::{Result, bail};
use msvc6_base::units::Rva;
use msvc6_diff::env::Env;
use msvc6_image::pe::Image;
use msvc6_project::mapfile::Map;
use std::collections::BTreeMap;

pub const MAX_DELTA: u32 = 4096;
pub const CTX: usize = 8;

#[must_use]
pub fn text_relocs(img: &Image) -> Vec<u32> {
    let Some(text) = img.section(".text") else {
        return Vec::new();
    };
    let lo = text.rva.get();
    let hi = lo.saturating_add(text.virt_size.get().max(text.raw_size.get()));
    let mut out: Vec<u32> = img
        .relocations()
        .into_iter()
        .map(|(r, _)| r.get())
        .filter(|at| (lo..hi).contains(at))
        .collect();
    out.sort_unstable();
    out
}

#[must_use]
pub fn map_code_symbols(map: &Map) -> BTreeMap<String, u32> {
    map.symbols
        .iter()
        .filter(|s| s.section == 1)
        .map(|s| (s.name.clone(), s.va))
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Pair {
    pub reference: u32,
    pub ours: u32,
    pub delta: i64,
    pub refs: usize,
    pub first_user: String,
}

#[derive(Debug, Default)]
pub struct Report {
    pub pairable: usize,
    pub unpaired: usize,
    pub pairs: Vec<Pair>,
}

impl Report {
    #[must_use]
    pub fn exact(&self) -> usize {
        self.pairs.iter().filter(|p| p.delta == 0).count()
    }

    #[must_use]
    pub fn misplaced(&self) -> usize {
        self.pairs.len().saturating_sub(self.exact())
    }
}

fn is_data(img: &Image, r: u32) -> bool {
    img.sections
        .iter()
        .any(|s| s.name != ".text" && s.contains(Rva(r)))
}

pub fn measure(env: &Env, ours: &Image, map: &Map) -> Result<Report> {
    let syms = map_code_symbols(map);
    if syms.is_empty() {
        bail!(
            "the map has no `0001:` code symbols; pairing is impossible and printing zero deltas would read as a clean result"
        );
    }
    let refimg = &env.img;
    let rbase = refimg.opt.image_base.get();
    let obase = ours.opt.image_base.get();
    let text = env.text();
    let mut pairs: Vec<(u32, u32, String)> = syms
        .iter()
        .filter_map(|(name, ova)| {
            env.names
                .rva(name, text, env.text_rva)
                .map(|r| (r, ova.wrapping_sub(obase), name.clone()))
        })
        .collect();
    pairs.sort();
    let starts: Vec<u32> = pairs.iter().map(|(r, _, _)| *r).collect();
    let mut seen: BTreeMap<(u32, u32), (usize, String)> = BTreeMap::new();
    let mut rep = Report {
        pairable: pairs.len(),
        ..Report::default()
    };
    let ctx32 = u32::try_from(CTX).unwrap_or(8);
    for site in text_relocs(refimg) {
        let k = starts.partition_point(|s| *s <= site);
        let Some((rstart, ostart, name)) = k.checked_sub(1).and_then(|i| pairs.get(i)) else {
            rep.unpaired += 1;
            continue;
        };
        let delta = site.wrapping_sub(*rstart);
        if delta > MAX_DELTA {
            rep.unpaired += 1;
            continue;
        }
        let osite = ostart.wrapping_add(delta);
        let (Some(rprev), Some(oprev)) = (
            site.checked_sub(ctx32)
                .and_then(|a| refimg.at_rva(Rva(a), CTX)),
            osite
                .checked_sub(ctx32)
                .and_then(|a| ours.at_rva(Rva(a), CTX)),
        ) else {
            rep.unpaired += 1;
            continue;
        };
        if rprev != oprev {
            rep.unpaired += 1;
            continue;
        }
        let (Some(rv), Some(ov)) = (
            refimg.u32_at_rva(Rva(site)).map(|v| v.wrapping_sub(rbase)),
            ours.u32_at_rva(Rva(osite)).map(|v| v.wrapping_sub(obase)),
        ) else {
            rep.unpaired += 1;
            continue;
        };
        if !is_data(refimg, rv) || !is_data(ours, ov) {
            rep.unpaired += 1;
            continue;
        }
        let e = seen.entry((rv, ov)).or_insert((0, name.clone()));
        e.0 += 1;
    }
    let mut rows: Vec<Pair> = seen
        .into_iter()
        .map(|((r, o), (n, who))| Pair {
            reference: r,
            ours: o,
            delta: i64::from(o) - i64::from(r),
            refs: n,
            first_user: who,
        })
        .collect();
    rows.sort_by_key(|p| (-p.delta.abs(), p.reference));
    rep.pairs = rows;
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
pub fn lines(label: &str, rep: &Report, all: bool) -> Vec<String> {
    let mut out = vec![
        format!("{label}: GLOBALS PAIRED BY (FUNCTION, OFFSET) ON BYTE-EXACT BODIES"),
        format!(
            "  {} function(s) pairable from the map; {} distinct (reference, ours) operand pair(s); {} site(s) unpaired",
            rep.pairable,
            rep.pairs.len(),
            rep.unpaired
        ),
        format!(
            "  AT THE REFERENCE'S ADDRESS: {}   MISPLACED: {}",
            rep.exact(),
            rep.misplaced()
        ),
        format!(
            "  {:>10} {:>10} {:>11}  {:>4}  first user",
            "reference", "ours", "delta", "refs"
        ),
    ];
    let mut shown = 0usize;
    for p in &rep.pairs {
        if p.delta == 0 && !all {
            continue;
        }
        if shown >= 40 && !all {
            out.push(format!(
                "  ... {} more (pass --all)",
                rep.pairs.len().saturating_sub(shown)
            ));
            break;
        }
        out.push(format!(
            "  {:#010x} {:#010x} {:+11}  x{:<3}  {}",
            p.reference,
            p.ours,
            p.delta,
            p.refs,
            clip(&p.first_user, 46)
        ));
        shown += 1;
    }
    if shown == 0 {
        out.push("  every paired operand is at the reference's address".to_string());
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_map_s_code_rows_give_the_absolute_va() {
        let text = " t\r\n\r\n Preferred load address is 10000000\r\n\r\n\
                    \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
                    \x200001:00000690  ?LoadImage@@YAXXZ 100016a0 f   image.obj\r\n\
                    \x200003:00000000  _data               10063000     grid.obj\r\n";
        let m = Map::parse(std::path::Path::new("x.map"), text).unwrap_or_else(|e| panic!("{e:#}"));
        let s = map_code_symbols(&m);
        assert_eq!(s.get("?LoadImage@@YAXXZ").copied(), Some(0x1000_16a0));
        assert_eq!(s.len(), 1);
    }
}
