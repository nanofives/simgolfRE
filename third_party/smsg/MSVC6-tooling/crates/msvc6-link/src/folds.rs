use anyhow::{Context as _, Result};
use msvc6_image::coff::{Object, SELECT_ASSOCIATIVE};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet, HashMap};

use crate::comdat::SCN_MEM_EXECUTE;
use crate::icf::Inputs;
use crate::objkey::Keys;
use crate::owners::RefOwners;
use crate::verbose::{Group, Sym};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Props {
    pub chr: u32,
    pub sel: u8,
    pub size: u32,
    pub nreloc: u16,
    pub bytes: Vec<u8>,
}

#[derive(Clone, Debug, Default)]
pub struct ObjProps {
    sections: Vec<Props>,
    by_name: HashMap<String, usize>,
}

impl ObjProps {
    #[must_use]
    pub fn get(&self, name: &str) -> Option<&Props> {
        self.by_name.get(name).and_then(|&i| self.sections.get(i))
    }
}

fn sel_kind(s: u8) -> u8 {
    if matches!(s, 1 | 2 | 3 | 4 | 6 | 7) {
        s
    } else {
        0
    }
}

pub fn props_of(d: &[u8]) -> Result<ObjProps> {
    let f = Object::parse(d).context("not a COFF object")?;
    let is_sec = |s: &msvc6_image::coff::Symbol| !s.aux.is_empty() && s.class == 3 && s.typ == 0;
    let mut sel: HashMap<u16, u8> = HashMap::new();
    let defs: Vec<(u16, u8, u16)> = f
        .symbols
        .iter()
        .filter(|s| is_sec(s))
        .filter_map(|s| {
            let n = s.section_number()?;
            let a = s.aux.first()?;
            Some((
                n,
                a.get(14).copied().unwrap_or(0),
                msvc6_base::le::u16_at(a, 12).unwrap_or(0),
            ))
        })
        .collect();
    for &(n, s, _) in &defs {
        if s == 0 || s == SELECT_ASSOCIATIVE {
            continue;
        }
        let k = sel_kind(s);
        sel.entry(n).or_insert(k);
        for &(m, t, parent) in &defs {
            if t == SELECT_ASSOCIATIVE && parent == n {
                sel.entry(m).or_insert(k);
            }
        }
    }
    let mut out = ObjProps::default();
    let mut index: HashMap<u16, usize> = HashMap::new();
    let mut copied = 0usize;
    let budget = d
        .len()
        .saturating_mul(crate::comdat::BYTES_BUDGET)
        .saturating_add(1 << 16);
    for s in &f.sections {
        if s.characteristics & SCN_MEM_EXECUTE == 0 {
            continue;
        }
        let bytes = f.section_data(s).to_vec();
        copied = copied.saturating_add(bytes.len());
        if copied > budget {
            anyhow::bail!(
                "not a COFF object LINK wrote: its code sections read {copied} bytes of a {}-byte file",
                d.len()
            );
        }
        index.insert(s.number, out.sections.len());
        out.sections.push(Props {
            chr: s.characteristics,
            sel: sel.get(&s.number).copied().unwrap_or(0),
            size: s.size,
            nreloc: s.relocs,
            bytes,
        });
    }
    for y in &f.symbols {
        if y.name.is_empty() || y.name.starts_with('.') {
            continue;
        }
        let Some(n) = y.section_number() else {
            continue;
        };
        if let Some(&i) = index.get(&n) {
            out.by_name.entry(y.name.clone()).or_insert(i);
        }
    }
    Ok(out)
}

pub struct Objects<'a> {
    pub src: &'a mut dyn Inputs,
    cache: HashMap<String, Option<ObjProps>>,
}

impl std::fmt::Debug for Objects<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Objects")
            .field("cached", &self.cache.len())
            .finish_non_exhaustive()
    }
}

impl<'a> Objects<'a> {
    pub fn new(src: &'a mut dyn Inputs) -> Self {
        Self {
            src,
            cache: HashMap::new(),
        }
    }

    fn load(&mut self, obj: &str) -> Option<ObjProps> {
        let bytes = match obj.strip_suffix(')').and_then(|o| o.rsplit_once('(')) {
            Some((lib, member)) => self.src.member(lib, member)?,
            None => self.src.object(obj)?,
        };
        props_of(&bytes).ok()
    }

    pub fn props(&mut self, s: &Sym) -> Option<&Props> {
        if !self.cache.contains_key(&s.obj) {
            let loaded = self.load(&s.obj);
            self.cache.insert(s.obj.clone(), loaded);
        }
        self.cache.get(&s.obj)?.as_ref()?.get(&s.name)
    }
}

#[must_use]
pub fn sel_tag(p: Option<&Props>) -> String {
    match p {
        None => "?".into(),
        Some(p) => {
            let s = match p.sel {
                1 => "NODUP",
                2 => "ANY",
                0 => "SECT",
                _ => "SEL?",
            };
            format!("{s}/{:08x}", p.chr)
        }
    }
}

#[must_use]
pub fn trimmed(b: &[u8]) -> &[u8] {
    let mut n = b.len();
    while n > 1 && matches!(b.get(n - 1), Some(0x90 | 0xcc)) {
        n -= 1;
    }
    b.get(..n).unwrap_or(b)
}

#[must_use]
pub fn short(b: &[u8]) -> String {
    let h: String = b.iter().take(8).map(|x| format!("{x:02x}")).collect();
    if b.len() > 8 { format!("{h}..") } else { h }
}

#[derive(Clone, Debug, Default, Serialize)]
pub struct Summary {
    pub groups: usize,
    pub same: usize,
    pub survivor_differs: usize,
    pub split: usize,
    pub ref_folds: usize,
    pub unplaced: usize,
    pub misfolded: usize,
    pub folded_apart: usize,
    pub reference_folds: usize,
    pub listings: usize,
}

#[derive(Clone, Debug, Serialize)]
pub struct Row {
    pub group: usize,
    pub verdict: &'static str,
    pub survivor: String,
    pub object: String,
    pub members: usize,
    pub unresolved: usize,
    pub refs: Vec<u32>,
}

pub struct Input<'a> {
    pub label: &'a str,
    pub log: &'a str,
    pub map: &'a str,
    pub groups: &'a [Group],
    pub ours: &'a BTreeMap<String, u32>,
    pub objof: &'a BTreeMap<String, String>,
    pub owners: &'a RefOwners,
    pub resolve: &'a crate::owners::Resolver<'a>,
    pub functions: &'a [(u32, Vec<u8>)],
    pub keys: &'a Keys,
    pub verbose: bool,
}

impl std::fmt::Debug for Input<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Input")
            .field("label", &self.label)
            .field("groups", &self.groups.len())
            .finish_non_exhaustive()
    }
}

#[derive(Debug, Default)]
pub struct Report {
    pub lines: Vec<String>,
    pub summary: Summary,
    pub rows: Vec<Row>,
}

pub fn run(inp: &Input<'_>, objs: &mut Objects<'_>) -> Report {
    let mut rep = Report::default();
    let groups = inp.groups;
    let refof = &inp.owners.refof;
    let in_group: HashMap<&str, usize> = groups
        .iter()
        .enumerate()
        .flat_map(|(i, g)| g.members().map(move |m| (m.name.as_str(), i)))
        .collect();
    let owner_at = |r: u32| inp.owners.owner_at(r);
    rep.lines.push(format!(
        "icf {}: {} fold group(s) in {}",
        inp.label,
        groups.len(),
        inp.log
    ));
    rep.lines.push(format!("  map {}", inp.map));
    let (mut n_split, mut n_folds, mut n_differs, mut n_same, mut n_unknown) = (0, 0, 0, 0, 0);
    let mut split_members = 0usize;
    let mut fold_names: BTreeSet<String> = BTreeSet::new();
    for (gi, g) in groups.iter().enumerate() {
        let sp = objs.props(&g.survivor).cloned();
        let our = inp.ours.get(&g.survivor.name).copied();
        let mut by_ref: BTreeMap<u32, Vec<&Sym>> = BTreeMap::new();
        let mut unresolved = 0usize;
        for m in g.members() {
            match (inp.resolve)(&m.name) {
                Some(r) => by_ref.entry(r).or_default().push(m),
                None => unresolved += 1,
            }
        }
        let mut copies: Vec<u32> = Vec::new();
        if let Some(p) = &sp
            && p.nreloc == 0
        {
            let body = trimmed(&p.bytes);
            for (rva, b) in inp.functions {
                if trimmed(b) == body {
                    copies.push(*rva);
                }
            }
            copies.sort_unstable();
            copies.dedup();
        }
        let mut folds: Vec<(u32, String)> = Vec::new();
        let wanted: BTreeSet<u32> = by_ref.keys().chain(copies.iter()).copied().collect();
        for (n, rr) in refof {
            if wanted.contains(rr) && in_group.get(n.as_str()) != Some(&gi) {
                folds.push((*rr, n.clone()));
            }
        }
        folds.sort();
        folds.dedup();
        let modal = by_ref.values().map(Vec::len).max().unwrap_or(0);
        split_members += by_ref.values().map(Vec::len).sum::<usize>() - modal;
        fold_names.extend(folds.iter().map(|(_, n)| n.clone()));
        let ref_survivor_obj: Vec<String> = by_ref.keys().map(|r| owner_at(*r)).collect();
        let verdict = if by_ref.len() > 1 {
            n_split += 1;
            "SPLIT"
        } else if !folds.is_empty() {
            n_folds += 1;
            "REF-FOLDS"
        } else if by_ref.is_empty() {
            n_unknown += 1;
            "UNPLACED"
        } else if ref_survivor_obj.iter().any(|o| {
            !o.split('|')
                .any(|x| x.eq_ignore_ascii_case(inp.keys.object_key(&g.survivor.obj)))
        }) {
            n_differs += 1;
            "SURVIVOR-DIFFERS"
        } else {
            n_same += 1;
            "SAME"
        };
        rep.rows.push(Row {
            group: gi + 1,
            verdict,
            survivor: g.survivor.name.clone(),
            object: g.survivor.obj.clone(),
            members: g.replaced.len() + 1,
            unresolved,
            refs: by_ref.keys().copied().collect(),
        });
        rep.lines.push(String::new());
        rep.lines.push(format!(
            "G{:<3} {verdict:<16} size {} relocs {} body {}  {} member(s), {} unresolved",
            gi + 1,
            sp.as_ref().map_or(0, |p| p.size),
            sp.as_ref().map_or(0, |p| p.nreloc),
            sp.as_ref()
                .map_or_else(|| "?".into(), |p| short(trimmed(&p.bytes))),
            g.replaced.len() + 1,
            unresolved
        ));
        rep.lines.push(format!(
            "  survivor {} {} ours {} [{}]",
            g.survivor.name,
            g.survivor.obj,
            our.map_or_else(|| "?".into(), |r| format!("{r:#x}")),
            sel_tag(sp.as_ref())
        ));
        for (r, ms) in &by_ref {
            let mut kinds: BTreeMap<String, usize> = BTreeMap::new();
            for m in ms {
                let t = sel_tag(objs.props(m));
                *kinds.entry(format!("{} {t}", m.obj)).or_default() += 1;
            }
            rep.lines.push(format!(
                "  ref {r:#07x} (in {}): {} name(s): {}",
                owner_at(*r),
                ms.len(),
                kinds
                    .iter()
                    .map(|(k, n)| format!("{n}x {k}"))
                    .collect::<Vec<_>>()
                    .join(", ")
            ));
            if inp.verbose {
                for m in ms {
                    let t = sel_tag(objs.props(m));
                    rep.lines.push(format!("      {} {} [{t}]", m.name, m.obj));
                }
            }
        }
        if !copies.is_empty() {
            rep.lines.push(format!(
                "  reference copies of this body: {}",
                copies
                    .iter()
                    .map(|r| format!("{r:#x}"))
                    .collect::<Vec<_>>()
                    .join(" ")
            ));
        }
        for (r, n) in folds.iter().take(if inp.verbose { usize::MAX } else { 6 }) {
            let o = inp.objof.get(n).map_or("?", String::as_str);
            let our_at = inp
                .ours
                .get(n)
                .map_or_else(|| "?".into(), |x| format!("{x:#x}"));
            let tag = sel_tag(objs.props(&Sym {
                name: n.clone(),
                obj: o.to_string(),
            }));
            rep.lines.push(format!(
                "  REF-FOLDS at {r:#x}: {n} {o} ours {our_at} [{tag}]"
            ));
        }
        if folds.len() > 6 && !inp.verbose {
            rep.lines
                .push(format!("  ... {} more (--verbose)", folds.len() - 6));
        }
    }
    let group_refs: BTreeSet<u32> = groups
        .iter()
        .flat_map(|g| g.members().filter_map(|m| refof.get(&m.name).copied()))
        .collect();
    let mut at_ref: BTreeMap<u32, BTreeMap<String, usize>> = BTreeMap::new();
    for (n, r) in refof {
        if group_refs.contains(r) {
            let class = in_group.get(n.as_str()).map_or_else(
                || format!("own {:#x}", inp.ours.get(n).copied().unwrap_or(0)),
                |g| format!("G{g}"),
            );
            *at_ref.entry(*r).or_default().entry(class).or_default() += 1;
        }
    }
    let ref_folds: usize = at_ref
        .values()
        .map(|c| c.values().sum::<usize>() - c.values().copied().max().unwrap_or(0))
        .sum();
    rep.lines.push(String::new());
    rep.lines.push(format!(
        "CENSUS misfolded name(s): {} -- {split_members} folded together that the reference keeps apart, \
         {ref_folds} the reference folds that ours keeps apart ({} REF-FOLDS listing(s))",
        split_members + ref_folds,
        fold_names.len()
    ));
    rep.lines.push(String::new());
    rep.lines.push(format!(
        "SUMMARY {} group(s): SAME {n_same}, SURVIVOR-DIFFERS {n_differs}, SPLIT {n_split} (the reference keeps them \
         apart), REF-FOLDS {n_folds} (the reference folds a name ours keeps elsewhere), UNPLACED {n_unknown}",
        groups.len()
    ));
    rep.summary = Summary {
        groups: groups.len(),
        same: n_same,
        survivor_differs: n_differs,
        split: n_split,
        ref_folds: n_folds,
        unplaced: n_unknown,
        misfolded: split_members + ref_folds,
        folded_apart: split_members,
        reference_folds: ref_folds,
        listings: fold_names.len(),
    };
    rep
}
