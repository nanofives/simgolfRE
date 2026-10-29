use msvc6_base::units::Rva;
use msvc6_diff::env::Env;
use msvc6_image::pe::Image;
use msvc6_project::mapfile::{Map, Owners};
use std::collections::{BTreeMap, BTreeSet};

pub const ILT_PROBE: u32 = 16;

#[must_use]
pub fn ilt(text: &[u8], base: u32) -> Vec<(u32, u32)> {
    let at = |rva: u32| -> Option<u8> {
        rva.checked_sub(base)
            .and_then(|o| text.get(o as usize))
            .copied()
    };
    let Some(start) = (0..ILT_PROBE).find(|d| at(base.wrapping_add(*d)) == Some(0xe9)) else {
        return Vec::new();
    };
    let mut out = Vec::new();
    let mut a = base.wrapping_add(start);
    while at(a) == Some(0xe9) {
        let Some(off) = a.checked_sub(base).map(|o| (o as usize).saturating_add(1)) else {
            break;
        };
        let Some(raw) = text.get(off..off.saturating_add(4)) else {
            break;
        };
        let Ok(b) = <[u8; 4]>::try_from(raw) else {
            break;
        };
        let rel = i32::from_le_bytes(b);
        let Ok(t) = u32::try_from(i64::from(a) + 5 + i64::from(rel)) else {
            break;
        };
        out.push((a, t));
        a = a.saturating_add(5);
    }
    out
}

#[must_use]
pub fn map_symbols(map: &Map) -> BTreeMap<String, u32> {
    map.symbols
        .iter()
        .filter(|s| s.section == 1)
        .map(|s| (s.name.clone(), s.rva))
        .collect()
}

#[must_use]
pub fn map_objects(map: &Map, owners: &Owners) -> BTreeMap<String, String> {
    map.symbols
        .iter()
        .filter(|s| s.section == 1 && s.object.to_ascii_lowercase().ends_with(".obj"))
        .map(|s| {
            let tok = s.owner_token();
            (s.name.clone(), owners.map_owner(&tok).to_string())
        })
        .collect()
}

pub type ObjRun = (u32, String, usize);

#[must_use]
pub fn object_runs(rows: &[(u32, String)]) -> Vec<ObjRun> {
    let mut runs: Vec<ObjRun> = Vec::new();
    for (rva, o) in rows {
        match runs.last_mut() {
            Some(last) if last.1 == *o => last.2 += 1,
            _ => runs.push((*rva, o.clone(), 1)),
        }
    }
    runs
}

#[must_use]
pub fn split_objects(runs: &[ObjRun]) -> BTreeMap<String, Vec<u32>> {
    let mut by: BTreeMap<String, Vec<u32>> = BTreeMap::new();
    for (rva, o, _) in runs {
        by.entry(o.clone()).or_default().push(*rva);
    }
    by.retain(|_, v| v.len() > 1);
    by
}

#[must_use]
pub fn reserved_slot(body: u32) -> u32 {
    let with_room = body.saturating_mul(5) / 4;
    with_room.div_ceil(16).saturating_mul(16)
}

pub type Deviation = (u32, u32, u32, i64);

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct LawFit {
    pub fit: usize,
    pub deviations: Vec<Deviation>,
    pub signed: i64,
}

#[must_use]
pub fn law_fit(addrs: &[u32], body_of: &dyn Fn(u32) -> Option<u32>) -> LawFit {
    let mut out = LawFit::default();
    for (a, next) in addrs.iter().zip(addrs.iter().skip(1)) {
        let Some(body) = body_of(*a) else { continue };
        let Some(slot) = next.checked_sub(*a) else {
            continue;
        };
        if slot < body {
            continue;
        }
        let pred = reserved_slot(body);
        if pred == slot {
            out.fit += 1;
        } else {
            let d = i64::from(slot) - i64::from(pred);
            out.signed += d;
            out.deviations.push((*a, body, slot, d));
        }
    }
    out.deviations.sort_by_key(|(_, _, _, d)| -d.abs());
    out
}

#[must_use]
pub fn relocated_bytes(img: &Image) -> BTreeSet<u32> {
    img.relocations()
        .into_iter()
        .flat_map(|(r, _)| (0..4).map(move |k| r.get().saturating_add(k)))
        .collect()
}

#[must_use]
pub fn in_rel32(body: &[u8], k: usize) -> bool {
    (1..=4).any(|back| {
        k.checked_sub(back)
            .and_then(|i| body.get(i))
            .is_some_and(|op| *op == 0xe8 || *op == 0xe9)
    })
}

#[derive(Debug, Default)]
pub struct Placement {
    pub published: usize,
    pub rows: Vec<(String, u32, u32)>,
    pub unresolved: usize,
}

impl Placement {
    #[must_use]
    pub fn exact(&self) -> usize {
        self.rows.iter().filter(|(_, r, o)| r == o).count()
    }
}

#[must_use]
pub fn resolve(env: &Env, map: &Map) -> Placement {
    let ours = map_symbols(map);
    let text = env.text();
    let mut p = Placement {
        published: ours.len(),
        ..Placement::default()
    };
    for (name, our_rva) in &ours {
        match env.names.rva(name, text, env.text_rva) {
            Some(r) => p.rows.push((name.clone(), r, *our_rva)),
            None => p.unresolved += 1,
        }
    }
    p.rows.sort_by_key(|(_, r, _)| *r);
    p
}

#[derive(Debug, Default)]
pub struct Lattice {
    pub reference: Vec<(u32, u32)>,
    pub ours: Vec<(u32, u32)>,
}

#[must_use]
pub fn lattice(env: &Env, ours: &Image) -> Option<Lattice> {
    let os = ours.section(".text")?;
    Some(Lattice {
        reference: ilt(env.text(), env.text_rva),
        ours: ilt(ours.section_bytes(os), os.rva.get()),
    })
}

fn span(v: &[(u32, u32)]) -> (u32, u32) {
    v.iter()
        .map(|(_, t)| *t)
        .fold((u32::MAX, 0u32), |(lo, hi), t| (lo.min(t), hi.max(t)))
}

#[must_use]
pub fn lattice_lines(l: &Lattice) -> Vec<String> {
    let (rlo, rhi) = span(&l.reference);
    let (olo, ohi) = span(&l.ours);
    let delta =
        i64::try_from(l.ours.len()).unwrap_or(0) - i64::try_from(l.reference.len()).unwrap_or(0);
    let mut out = vec![
        "  ILT LATTICE (`/INCREMENTAL' thunks at .text's start; its SIZE offsets every function after it)"
            .to_string(),
        format!(
            "    reference {:>5} thunk(s), {} bytes, targets {rlo:#x}..{rhi:#x}",
            l.reference.len(),
            l.reference.len().saturating_mul(5)
        ),
        format!(
            "    ours      {:>5} thunk(s), {} bytes, targets {olo:#x}..{ohi:#x}",
            l.ours.len(),
            l.ours.len().saturating_mul(5)
        ),
        format!(
            "    DELTA     {delta:>+5} thunk(s) = {:+} bytes of shift applied to everything downstream",
            delta.saturating_mul(5)
        ),
    ];
    let above: Vec<String> = l
        .reference
        .iter()
        .filter(|(_, t)| *t > ohi)
        .map(|(_, t)| format!("{t:#x}"))
        .collect();
    if !above.is_empty() {
        out.push(format!(
            "    {} REFERENCE THUNK(S) TARGET ABOVE OUR HIGHEST TARGET ({ohi:#x}): {}",
            above.len(),
            above.join(" ")
        ));
    }
    out
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Content {
    pub functions: u64,
    pub bytes: u64,
    pub reloc: u64,
    pub rel32: u64,
    pub other: u64,
}

#[must_use]
pub fn content(env: &Env, ours: &Image, p: &Placement) -> Option<(Content, Vec<String>)> {
    let os = ours.section(".text")?;
    let otext = ours.section_bytes(os);
    let obase = os.rva.get();
    let text = env.text();
    let base = env.text_rva;
    let relocs = relocated_bytes(&env.img);
    let known: BTreeSet<u32> = p.rows.iter().map(|(_, r, _)| *r).collect();
    let mut c = Content::default();
    let mut first = Vec::new();
    for (name, r, o) in &p.rows {
        let Some(len) = env.recorded(*r) else {
            continue;
        };
        if known
            .range(r.saturating_add(1)..r.saturating_add(len))
            .next()
            .is_some()
        {
            continue;
        }
        let ul = len as usize;
        let (Some(a), Some(b)) = (
            r.checked_sub(base)
                .and_then(|x| text.get(x as usize..(x as usize).saturating_add(ul))),
            o.checked_sub(obase)
                .and_then(|x| otext.get(x as usize..(x as usize).saturating_add(ul))),
        ) else {
            continue;
        };
        c.functions += 1;
        c.bytes = c.bytes.saturating_add(u64::from(len));
        for (k, (x, y)) in a.iter().zip(b).enumerate() {
            if x == y {
                continue;
            }
            let at = r.saturating_add(u32::try_from(k).unwrap_or(0));
            if relocs.contains(&at) {
                c.reloc += 1;
            } else if in_rel32(a, k) {
                c.rel32 += 1;
            } else {
                c.other += 1;
                if first.len() < 4 {
                    first.push(format!("{at:#x} in {name}"));
                }
            }
        }
    }
    Some((c, first))
}

#[must_use]
pub fn rows_by_object(
    env: &Env,
    map: &Map,
    owners: &Owners,
    self_control: bool,
) -> (Vec<(u32, String, String)>, usize) {
    let objof = map_objects(map, owners);
    let ours = map_symbols(map);
    let text = env.text();
    let mut rows = Vec::new();
    let mut unresolved = 0usize;
    for (n, r) in &ours {
        let at = if self_control {
            Some(*r)
        } else {
            env.names.rva(n, text, env.text_rva)
        };
        match (at, objof.get(n)) {
            (Some(r), Some(o)) => rows.push((r, o.clone(), n.clone())),
            _ if self_control => {}
            _ => unresolved += 1,
        }
    }
    rows.sort();
    (rows, unresolved)
}

#[must_use]
pub fn section_of(img: &Image, rva: u32) -> Option<&str> {
    img.section_of(Rva(rva)).map(|s| s.name.as_str())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn runs_collapse_by_object_and_a_split_object_is_interleaved() {
        let rows: Vec<(u32, String)> = [
            (0x10, "a.obj"),
            (0x20, "a.obj"),
            (0x30, "b.obj"),
            (0x40, "a.obj"),
        ]
        .iter()
        .map(|(r, o)| (*r, (*o).to_string()))
        .collect();
        let runs = object_runs(&rows);
        assert_eq!(runs.len(), 3);
        let split = split_objects(&runs);
        assert_eq!(split.get("a.obj"), Some(&vec![0x10, 0x40]));
    }

    #[test]
    fn the_reservation_law_rounds_a_quarter_more_up_to_sixteen() {
        assert_eq!(reserved_slot(480), 608);
        assert_eq!(reserved_slot(40), 64);
        assert_eq!(reserved_slot(39), 48);
        assert_eq!(reserved_slot(141), 176);
        let f = law_fit(&[0x1000, 0x1260, 0x1300], &|a| (a == 0x1000).then_some(480));
        assert_eq!((f.fit, f.deviations.len()), (1, 0));
    }

    #[test]
    fn ilt_walks_the_e9_chain_and_stops_and_survives_a_truncated_one() {
        let mut t = vec![0u8; 64];
        for (i, target) in [0x2000i64, 0x3000, 0x0500].iter().enumerate() {
            let at = 5 + i * 5;
            let thunk = 0x1005 + i64::try_from(i).unwrap_or(0) * 5;
            let rel = i32::try_from(target - (thunk + 5)).unwrap_or(0);
            if let Some(op) = t.get_mut(at) {
                *op = 0xe9;
            }
            if let Some(s) = t.get_mut(at + 1..at + 5) {
                s.copy_from_slice(&rel.to_le_bytes());
            }
        }
        if let Some(b) = t.get_mut(20) {
            *b = 0x55;
        }
        assert_eq!(
            ilt(&t, 0x1000),
            vec![(0x1005, 0x2000), (0x100a, 0x3000), (0x100f, 0x0500)]
        );
        assert!(ilt(&[0x55, 0x8b, 0xec, 0x90], 0x1000).is_empty());
        assert!(ilt(&[0, 0, 0, 0, 0, 0xe9, 0x01], 0x1000).is_empty());
        assert!(in_rel32(&[0xe8, 1, 2, 3, 4], 3));
        assert!(!in_rel32(&[0x90, 1, 2, 3, 4], 3));
    }
}
