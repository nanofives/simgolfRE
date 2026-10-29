use iced_x86::{Code, Decoder, DecoderOptions};
use msvc6_diff::comdat::{self, Comdat, REL_REL32};
use msvc6_diff::env::Env;
use msvc6_diff::memtable::{self, Table};
use msvc6_diff::score::Seat;
use msvc6_diff::seat::scope_stem;
use std::collections::{BTreeMap, BTreeSet};
use std::path::PathBuf;

pub const THUNK_HOPS: usize = 4;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Func {
    At(u32),
    Unresolved(String),
}

#[must_use]
pub fn sweep(text: &[u8], base: u32) -> (Vec<(u32, u32)>, BTreeMap<u32, u32>) {
    let mut calls = Vec::new();
    let mut jumps = BTreeMap::new();
    let mut d = Decoder::with_ip(32, text, u64::from(base), DecoderOptions::NONE);
    while d.can_decode() {
        let pos = d.position();
        let i = d.decode();
        let op = text.get(pos).copied();
        let (Ok(site), Ok(t)) = (
            u32::try_from(i.ip()),
            u32::try_from(i.near_branch_target() & 0xffff_ffff),
        ) else {
            continue;
        };
        match i.code() {
            Code::Call_rel32_32 if op == Some(0xe8) => calls.push((site, t)),
            Code::Jmp_rel32_32 if op == Some(0xe9) => {
                jumps.insert(site, t);
            }
            _ => {}
        }
    }
    (calls, jumps)
}

#[must_use]
pub fn through_thunks(mut t: u32, jumps: &BTreeMap<u32, u32>) -> u32 {
    for _ in 0..THUNK_HOPS {
        match jumps.get(&t) {
            Some(&n) => t = n,
            None => break,
        }
    }
    t
}

#[must_use]
pub fn credit(
    calls: &[(u32, u32)],
    jumps: &BTreeMap<u32, u32>,
    starts: &BTreeSet<u32>,
) -> Vec<(u32, u32, u32)> {
    let resolved: Vec<u32> = calls
        .iter()
        .map(|&(_, t)| through_thunks(t, jumps))
        .collect();
    let mut all = starts.clone();
    all.extend(resolved.iter().copied());
    calls
        .iter()
        .zip(resolved)
        .filter_map(|(&(site, _), t)| all.range(..=site).next_back().map(|&o| (o, site, t)))
        .collect()
}

#[must_use]
pub fn comdat_calls(c: &Comdat) -> Vec<&str> {
    c.relocations
        .iter()
        .filter(|r| r.kind == REL_REL32)
        .filter(|r| {
            r.at.checked_sub(1)
                .and_then(|p| c.bytes.get(p as usize))
                .copied()
                == Some(0xe8)
        })
        .map(|r| r.symbol.as_str())
        .collect()
}

#[derive(Debug, Default)]
pub struct Names {
    at: BTreeMap<u32, Vec<String>>,
}

impl Names {
    fn add(&mut self, rva: u32, name: &str) {
        let v = self.at.entry(rva).or_default();
        if !v.iter().any(|n| n == name) {
            v.push(name.to_string());
        }
    }

    #[must_use]
    pub fn of(&self, f: &Func) -> String {
        match f {
            Func::At(r) => self
                .at
                .get(r)
                .and_then(|v| v.first())
                .cloned()
                .unwrap_or_else(|| "-".to_string()),
            Func::Unresolved(n) => n.clone(),
        }
    }

    #[must_use]
    pub fn all(&self, f: &Func) -> Vec<String> {
        match f {
            Func::At(r) => self.at.get(r).cloned().unwrap_or_default(),
            Func::Unresolved(n) => vec![n.clone()],
        }
    }
}

#[must_use]
pub fn show(f: &Func) -> String {
    match f {
        Func::At(r) => format!("{r:#08x}"),
        Func::Unresolved(_) => "?".to_string(),
    }
}

#[must_use]
pub fn matches(filter: Option<&str>, f: &Func, names: &Names, image_base: u32) -> bool {
    let Some(x) = filter else { return true };
    if let Some(h) = x.strip_prefix("0x").or_else(|| x.strip_prefix("0X"))
        && let Ok(v) = u32::from_str_radix(h, 16)
    {
        let v = if v >= image_base { v - image_base } else { v };
        return *f == Func::At(v);
    }
    names.all(f).iter().any(|n| n.contains(x))
}

#[derive(Debug, Default)]
pub struct Ours {
    pub pairs: BTreeMap<(Func, Func), usize>,
    pub raw: usize,
    pub counted: usize,
    pub callers: BTreeSet<Func>,
    pub aliases: BTreeMap<u32, BTreeSet<String>>,
    pub superseded: usize,
    pub funclet: usize,
    pub refused: Vec<(PathBuf, String)>,
}

struct Lookup<'a> {
    env: &'a Env,
    db: &'a BTreeMap<String, u32>,
    jumps: &'a BTreeMap<u32, u32>,
}

type ByTu<'a> = BTreeMap<(&'a str, &'a str), (u32, Seat)>;

fn image_call_at(text: &[u8], base: u32, field: u32) -> Option<u32> {
    let off = usize::try_from(field.checked_sub(base)?).ok()?;
    if text.get(off.checked_sub(1)?) != Some(&0xe8) {
        return None;
    }
    let rel = i32::from_le_bytes(text.get(off..off.checked_add(4)?)?.try_into().ok()?);
    Some(field.wrapping_add(4).wrapping_add_signed(rel))
}

type Objects = Vec<(String, Vec<Comdat>)>;

fn read_out(objects: &Objects, by_tu: &ByTu<'_>, env: &Env) -> BTreeMap<String, u32> {
    let text = env.text();
    let mut votes: BTreeMap<&str, BTreeMap<u32, usize>> = BTreeMap::new();
    for (tu, comdats) in objects {
        for c in comdats {
            if c.section != ".text" {
                continue;
            }
            let name = c.symbol.as_deref().unwrap_or_default();
            let Some(&(seat, s)) = by_tu.get(&(tu.as_str(), name)) else {
                continue;
            };
            if s.excluded() {
                continue;
            }
            for r in c
                .relocations
                .iter()
                .filter(|r| r.kind == REL_REL32 && r.global)
            {
                if let Some(t) = image_call_at(text, env.text_rva, seat.wrapping_add(r.at)) {
                    *votes
                        .entry(r.symbol.as_str())
                        .or_default()
                        .entry(t)
                        .or_default() += 1;
                }
            }
        }
    }
    votes
        .into_iter()
        .filter_map(|(n, v)| {
            let total: usize = v.values().sum();
            let (&t, &k) = v.iter().max_by_key(|(_, k)| **k)?;
            (2 * k > total).then(|| (n.to_string(), t))
        })
        .collect()
}

fn ours(objs: &[PathBuf], rows: &[memtable::Row], look: &Lookup<'_>, names: &mut Names) -> Ours {
    let mut by_tu: ByTu<'_> = BTreeMap::new();
    let mut by_name: BTreeMap<&str, u32> = BTreeMap::new();
    for r in rows {
        by_tu.entry((&r.tu, &r.name)).or_insert((r.rva, r.seat));
        if !r.seat.excluded() {
            by_name.entry(&r.name).or_insert(r.rva);
            names.add(r.rva, &r.name);
        }
    }
    let mut paths: Vec<PathBuf> = objs.to_vec();
    paths.sort();
    let mut out = Ours::default();
    let mut objects: Objects = Vec::new();
    for p in &paths {
        match comdat::read(p).and_then(|d| comdat::comdats_of(&d, &[".text", ".text$x"])) {
            Ok(c) => objects.push((scope_stem(p), c)),
            Err(e) => out.refused.push((p.clone(), format!("{e:#}"))),
        }
    }
    let text = look.env.text();
    let base = look.env.text_rva;
    let readout = read_out(&objects, &by_tu, look.env);
    let mut owner: BTreeMap<Func, String> = BTreeMap::new();
    for (tu, comdats) in &objects {
        let mut resolve_callee = |callee: &str| {
            let r = by_tu
                .get(&(tu.as_str(), callee))
                .filter(|(_, s)| *s != Seat::Superseded)
                .map(|(r, _)| *r)
                .or_else(|| by_name.get(callee).copied())
                .or_else(|| look.env.names.rva(callee, text, base))
                .or_else(|| look.db.get(callee).copied())
                .or_else(|| readout.get(callee).copied())
                .map(|r| through_thunks(r, look.jumps));
            match r {
                Some(r) => {
                    names.add(r, callee);
                    Func::At(r)
                }
                None => Func::Unresolved(callee.to_string()),
            }
        };
        let mut parent = String::new();
        let mut nth = 0usize;
        for c in comdats {
            let name = c.symbol.clone().unwrap_or_default();
            let calls = comdat_calls(c);
            out.raw += calls.len();
            let funclet = c.section != ".text";
            if funclet {
                nth += 1;
            } else {
                parent.clone_from(&name);
                nth = 0;
            }
            let seat = if funclet {
                None
            } else {
                by_tu.get(&(tu.as_str(), name.as_str())).copied()
            };
            if seat.is_some_and(|(_, s)| s == Seat::Superseded) {
                out.superseded += calls.len();
                continue;
            }
            let caller = if funclet {
                Func::Unresolved(format!("{parent} [unwind funclet {nth}]"))
            } else {
                seat.map_or_else(|| Func::Unresolved(name.clone()), |(r, _)| Func::At(r))
            };
            if funclet && !owner.contains_key(&caller) {
                out.funclet += calls.len();
            }
            if let Some(first) = owner.get(&caller) {
                if let Func::At(r) = caller
                    && *first != name
                {
                    out.aliases.entry(r).or_default().insert(name.clone());
                }
                continue;
            }
            owner.insert(caller.clone(), name.clone());
            if !funclet {
                out.callers.insert(caller.clone());
            }
            for callee in calls {
                let f = resolve_callee(callee);
                *out.pairs.entry((caller.clone(), f)).or_default() += 1;
                out.counted += 1;
            }
        }
    }
    out
}

#[derive(Debug)]
pub struct Tables {
    pub img: BTreeMap<(Func, Func), usize>,
    pub ours: Ours,
    pub names: Names,
    pub members: Table,
    pub image_base: u32,
    pub stats: [usize; 5],
}

#[must_use]
pub fn tables(env: &Env, objs: &[PathBuf], jobs: usize) -> Tables {
    let text = env.text();
    let base = env.text_rva;
    let image_base = u32::try_from(env.base).unwrap_or(0);
    let table = memtable::score_objects(env, objs, jobs);
    let mut starts: BTreeSet<u32> = env.starts.clone();
    let mut db_names: BTreeMap<String, u32> = BTreeMap::new();
    for (r, n) in &env.function_names {
        db_names.entry(n.clone()).or_insert(*r);
    }
    starts.extend(
        table
            .rows
            .iter()
            .filter(|r| r.seat != Seat::Superseded)
            .map(|r| r.rva),
    );
    let (calls, jumps) = sweep(text, base);
    let mut names = Names::default();
    let look = Lookup {
        env,
        db: &db_names,
        jumps: &jumps,
    };
    let ours = ours(objs, &table.rows, &look, &mut names);
    for t in env.names.tiers() {
        for (n, r, scope) in t.rows {
            if scope.is_none() {
                names.add(*r, n);
            }
        }
    }
    for (n, r) in &db_names {
        names.add(*r, n);
    }
    let credited = credit(&calls, &jumps, &starts);
    let mut img_pairs: BTreeMap<(Func, Func), usize> = BTreeMap::new();
    for &(own, _, callee) in &credited {
        *img_pairs
            .entry((Func::At(own), Func::At(callee)))
            .or_default() += 1;
    }
    Tables {
        img: img_pairs,
        stats: [
            calls.len(),
            credited.len(),
            jumps.len(),
            starts.len(),
            env.starts.len(),
        ],
        ours,
        names,
        members: table,
        image_base,
    }
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Opts<'a> {
    pub caller: Option<&'a str>,
    pub callee: Option<&'a str>,
    pub diff: bool,
    pub by_callee: bool,
}

#[must_use]
pub fn lines(t: &Tables, o: &Opts<'_>, objs: &str) -> Vec<String> {
    let [n_calls, n_credited, n_jumps, n_starts, n_db] = t.stats;
    let mut out = vec![
        format!(
            "  image: {n_calls} direct call(s) in .text, {n_credited} credited, {n_jumps} thunk jump(s), {n_starts} start(s) ({n_db} function index + member seats)"
        ),
        format!(
            "  ours:  {} direct call(s) in {objs}, {} counted over {} caller(s) + {} in unwind funclets ({} in superseded COMDATs, the rest in duplicate or ICF-alias COMDATs), {} warning(s)",
            t.ours.raw,
            t.ours.counted.saturating_sub(t.ours.funclet),
            t.ours.callers.len(),
            t.ours.funclet,
            t.ours.superseded,
            t.members.warnings.len()
        ),
    ];
    let shared = |c: &Func| t.ours.callers.contains(c);
    let keep = |(a, b): &(Func, Func)| {
        matches(o.caller, a, &t.names, t.image_base) && matches(o.callee, b, &t.names, t.image_base)
    };
    let mut keys: BTreeSet<(Func, Func)> = BTreeSet::new();
    keys.extend(t.img.keys().filter(|k| keep(k)).cloned());
    keys.extend(t.ours.pairs.keys().filter(|k| keep(k)).cloned());
    let get = |m: &BTreeMap<(Func, Func), usize>, k: &(Func, Func)| m.get(k).copied().unwrap_or(0);
    let mut shown: BTreeSet<u32> = BTreeSet::new();
    if o.by_callee {
        let mut tot: BTreeMap<Func, (usize, usize)> = BTreeMap::new();
        for k in keys.iter().filter(|k| shared(&k.0)) {
            let e = tot.entry(k.1.clone()).or_default();
            e.0 += get(&t.img, k);
            e.1 += get(&t.ours.pairs, k);
        }
        out.push("callee\tname\timg\tours\tours-img".to_string());
        for (f, (i, u)) in &tot {
            if o.diff && i == u {
                continue;
            }
            if let Func::At(r) = f {
                shown.insert(*r);
            }
            out.push(format!(
                "{}\t{}\t{i}\t{u}\t{}",
                show(f),
                t.names.of(f),
                i64::try_from(*u).unwrap_or(0) - i64::try_from(*i).unwrap_or(0)
            ));
        }
    } else {
        out.push("caller\tname\tcallee\tname\timg\tours".to_string());
        let mut n = 0usize;
        for k in &keys {
            let (i, u) = (get(&t.img, k), get(&t.ours.pairs, k));
            if o.diff && (i == u || !shared(&k.0)) {
                continue;
            }
            n += 1;
            for f in [&k.0, &k.1] {
                if let Func::At(r) = f {
                    shown.insert(*r);
                }
            }
            out.push(format!(
                "{}\t{}\t{}\t{}\t{i}\t{u}",
                show(&k.0),
                t.names.of(&k.0),
                show(&k.1),
                t.names.of(&k.1)
            ));
        }
        if o.diff {
            out.push(format!(
                "  {n} differing pair(s), over callers both sides define"
            ));
        }
    }
    let aliases: Vec<_> = t
        .ours
        .aliases
        .iter()
        .filter(|(r, _)| shown.contains(r))
        .collect();
    if !aliases.is_empty() {
        out.push(
            "aliases: rvas our objects seat more than one name at (identical-function folding)"
                .to_string(),
        );
        for (r, other) in aliases {
            let o: Vec<&str> = other.iter().map(String::as_str).collect();
            out.push(format!(
                "  {r:#08x}\t{}\t= {}",
                t.names.of(&Func::At(*r)),
                o.join(" ")
            ));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_diff::comdat::{REL_DIR32, Reloc};

    fn put(t: &mut [u8], at: usize, op: u8, rel: i32) {
        if let Some(b) = t.get_mut(at) {
            *b = op;
        }
        if let Some(s) = t.get_mut(at + 1..at + 5) {
            s.copy_from_slice(&rel.to_le_bytes());
        }
    }

    #[test]
    fn a_call_through_a_thunk_chain_is_credited_to_the_body() {
        let mut t = vec![0x90u8; 0x40];
        put(&mut t, 0x00, 0xe9, 0x1010 - 0x1005);
        put(&mut t, 0x10, 0xe9, 0x1020 - 0x1015);
        put(&mut t, 0x30, 0xe8, 0x1000 - 0x1035);
        let (calls, jumps) = sweep(&t, 0x1000);
        assert_eq!(calls, vec![(0x1030, 0x1000)]);
        assert_eq!(jumps.len(), 2);
        assert_eq!(through_thunks(0x1000, &jumps), 0x1020);
        let starts: BTreeSet<u32> = [0x1030].into_iter().collect();
        assert_eq!(
            credit(&calls, &jumps, &starts),
            vec![(0x1030, 0x1030, 0x1020)]
        );
    }

    #[test]
    fn the_image_call_is_read_only_behind_an_e8_and_thunks_stop_after_four_hops() {
        let mut t = vec![0x90u8; 0x20];
        put(&mut t, 0x04, 0xe8, 0x10);
        assert_eq!(image_call_at(&t, 0x1000, 0x1005), Some(0x1019));
        assert_eq!(image_call_at(&t, 0x1000, 0x1006), None);
        assert_eq!(image_call_at(&t, 0x1000, 0x0fff), None);
        let jumps: BTreeMap<u32, u32> = (0..10u32).map(|i| (i, i + 1)).collect();
        assert_eq!(through_thunks(0, &jumps), 4);
    }

    #[test]
    fn a_resolved_call_target_bounds_its_predecessor_and_a_site_before_every_start_drops() {
        let calls = vec![(0x110, 0x180), (0x190, 0x100)];
        let starts: BTreeSet<u32> = [0x100].into_iter().collect();
        assert_eq!(
            credit(&calls, &BTreeMap::new(), &starts),
            vec![(0x100, 0x110, 0x180), (0x180, 0x190, 0x100)]
        );
        let starts: BTreeSet<u32> = [0x200].into_iter().collect();
        assert!(credit(&[(0x100, 0x200)], &BTreeMap::new(), &starts).is_empty());
    }

    #[test]
    fn only_a_rel32_after_e8_is_a_call() {
        let reloc = |at: u32, symbol: &str, kind: u16| Reloc {
            at,
            symbol: symbol.to_string(),
            kind,
            external: true,
            global: true,
            target_section: None,
            target_offset: 0,
            target_index: None,
        };
        let c = Comdat {
            symbol: Some("x".into()),
            bytes: vec![0xe8, 0, 0, 0, 0, 0xe9, 0, 0, 0, 0, 0x68, 0, 0, 0, 0],
            relocations: vec![
                reloc(1, "f", REL_REL32),
                reloc(6, "g", REL_REL32),
                reloc(11, "h", REL_DIR32),
            ],
            ..Comdat::default()
        };
        assert_eq!(comdat_calls(&c), vec!["f"]);
    }
}
