use crate::comdat::{self, Comdat, REL_DIR32, REL_DIR32NB, REL_REL32, Reloc};
use crate::env::Env;
use crate::resolve::ilt_slot;
use crate::seat::{Seats, scope_stem};
use msvc6_base::units::Rva;
use std::collections::{BTreeMap, HashMap, HashSet};
use std::path::{Path, PathBuf};

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub struct Raw {
    pub raw: usize,
    pub code: usize,
    pub field: usize,
    pub data: usize,
    pub len: usize,
    pub pad: usize,
    pub unres: usize,
    pub ilt: usize,
    pub lone: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Field {
    pub at: usize,
    pub value: Option<u32>,
    pub voted: bool,
    pub wrong: bool,
}

const FILL: [u8; 2] = [0xcc, 0x90];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Diff {
    pub off: usize,
    pub ours: Option<u8>,
    pub image: Option<u8>,
    pub kind: &'static str,
}

#[must_use]
pub fn diffs(
    ours: &[u8],
    relocated: &[bool],
    fields: &[Field],
    image: &[u8],
    want: usize,
) -> (Raw, Vec<Diff>) {
    let mut r = Raw::default();
    let mut d = Vec::new();
    let fill_from = ours.len() - ours.iter().rev().take_while(|b| FILL.contains(b)).count();
    for (i, &b) in ours.iter().enumerate() {
        if relocated.get(i).copied().unwrap_or(false) {
            continue;
        }
        let img = image.get(i).copied();
        if img == Some(b) {
            continue;
        }
        let kind = match (i < want, i >= fill_from) {
            (true, false) => "code",
            (_, true) => "tail",
            (false, false) => "ours-long",
        };
        d.push(Diff {
            off: i,
            ours: Some(b),
            image: img,
            kind,
        });
    }
    for f in fields {
        if f.wrong {
            for k in 0..comdat::FIELD {
                d.push(Diff {
                    off: f.at.saturating_add(k),
                    ours: None,
                    image: image.get(f.at.saturating_add(k)).copied(),
                    kind: "data",
                });
            }
            continue;
        }
        let Some(v) = f.value else {
            r.unres += 1;
            continue;
        };
        for (k, e) in v.to_le_bytes().iter().enumerate() {
            let img = image.get(f.at.saturating_add(k)).copied();
            if img != Some(*e) {
                d.push(Diff {
                    off: f.at.saturating_add(k),
                    ours: Some(*e),
                    image: img,
                    kind: if f.voted { "data" } else { "field" },
                });
            }
        }
    }
    let body_differs = d.iter().any(|x| matches!(x.kind, "code" | "ours-long"));
    for x in d.iter_mut().filter(|x| x.kind == "tail") {
        x.kind = if body_differs { "image-long" } else { "pad" };
    }
    if want > ours.len() {
        let tail = image.get(ours.len()..want).unwrap_or(&[]);
        let fill = tail.len() == want - ours.len() && tail.iter().all(|b| FILL.contains(b));
        let kind = if fill { "pad" } else { "image-long" };
        for i in ours.len()..want {
            d.push(Diff {
                off: i,
                ours: None,
                image: image.get(i).copied(),
                kind,
            });
        }
    }
    d.sort_by_key(|x| x.off);
    let n = |k: &str| d.iter().filter(|x| x.kind == k).count();
    r.code = n("code");
    r.field = n("field");
    r.data = n("data");
    r.len = n("ours-long") + n("image-long");
    r.pad = n("pad");
    r.raw = r.code + r.field + r.data + r.len;
    (r, d)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Vote {
    pub base: u32,
    pub sites: usize,
    pub content: Option<bool>,
    pub single: bool,
}

impl Vote {
    #[must_use]
    pub fn lone(self) -> bool {
        self.sites == 1 && self.content != Some(true)
    }

    #[must_use]
    pub fn wrong(self) -> bool {
        self.single && self.content == Some(false)
    }
}

#[derive(Debug, Default)]
struct Scopes {
    stems: HashMap<PathBuf, String>,
    definer: HashMap<String, String>,
}

#[derive(Debug, Default)]
pub struct Ctx {
    seated: HashMap<(PathBuf, u16), u32>,
    voted: HashMap<(PathBuf, u16), Vote>,
    seat_names: HashMap<String, u32>,
    vote_names: HashMap<String, (u32, Vote)>,
    sym_votes: HashMap<String, Vote>,
    iat: HashMap<String, u32>,
    scopes: Option<Scopes>,
    pub by_definer: bool,
}

fn read4(b: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(
        b.get(at..at.checked_add(4)?)?.try_into().ok()?,
    ))
}

#[must_use]
pub fn target_at(
    image_base: u32,
    placed_at: u32,
    obj: &Comdat,
    r: &Reloc,
    text: &[u8],
    text_rva: u32,
) -> Option<u32> {
    let addend = read4(&obj.bytes, r.at as usize)?;
    let field_rva = placed_at.checked_add(r.at)?;
    let off = usize::try_from(field_rva.checked_sub(text_rva)?).ok()?;
    let raw = read4(text, off)?;
    match r.kind {
        REL_REL32 => Some(
            raw.wrapping_add(field_rva)
                .wrapping_add(4)
                .wrapping_sub(addend),
        ),
        REL_DIR32 => raw.wrapping_sub(addend).checked_sub(image_base),
        REL_DIR32NB => Some(raw.wrapping_sub(addend)),
        _ => None,
    }
}

fn image_target(env: &Env, rva: u32, c: &Comdat, r: &Reloc) -> Option<u32> {
    let base = u32::try_from(env.base).ok()?;
    target_at(base, rva, c, r, env.text(), env.text_rva)
}

fn elect(v: &BTreeMap<u32, usize>) -> Option<(u32, usize)> {
    let (&base, _) = v.iter().max_by(|a, b| a.1.cmp(b.1).then(b.0.cmp(a.0)))?;
    Some((base, v.values().sum()))
}

#[must_use]
pub fn same_content(ours: &[u8], relocated: &[bool], image: Option<&[u8]>) -> Option<bool> {
    let image = image.filter(|i| i.len() >= ours.len())?;
    Some(
        ours.iter()
            .zip(image)
            .enumerate()
            .all(|(i, (a, b))| a == b || relocated.get(i).copied().unwrap_or(false)),
    )
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Target {
    pub at: Option<u32>,
    pub how: &'static str,
    pub vote: Option<Vote>,
}

impl Target {
    const fn new(at: Option<u32>, how: &'static str) -> Self {
        Self {
            at,
            how,
            vote: None,
        }
    }

    const fn voted(at: u32, how: &'static str, vote: Vote) -> Self {
        Self {
            at: Some(at),
            how,
            vote: Some(vote),
        }
    }
}

impl Ctx {
    #[must_use]
    pub fn build(env: &Env, objs: &[PathBuf], seats: &Seats) -> Self {
        Self::build_in(env, objs, seats, true)
    }

    #[must_use]
    pub fn build_flat(env: &Env, objs: &[PathBuf], seats: &Seats) -> Self {
        Self::build_in(env, objs, seats, false)
    }

    fn build_in(env: &Env, objs: &[PathBuf], seats: &Seats, scoped: bool) -> Self {
        let mut seated: HashMap<(PathBuf, u16), u32> = HashMap::new();
        let mut seat_names: HashMap<String, u32> = HashMap::new();
        for (&rva, (o, c)) in seats {
            seated.insert((o.clone(), c.index), rva);
            if let Some(s) = c.symbol.as_ref().filter(|_| c.external) {
                seat_names.entry(s.clone()).or_insert(rva);
            }
        }
        let mut seen: HashSet<&PathBuf> = HashSet::new();
        let objs: Vec<&PathBuf> = objs.iter().filter(|o| seen.insert(*o)).collect();
        let mut defs: HashMap<String, (PathBuf, u16, u32)> = HashMap::new();
        let mut data: HashMap<(PathBuf, u16), Comdat> = HashMap::new();
        for o in &objs {
            let Ok(d) = comdat::read(o) else { continue };
            for (idx, off, name) in comdat::defined_externals(&d).unwrap_or_default() {
                defs.entry(name).or_insert_with(|| ((*o).clone(), idx, off));
            }
            for s in comdat::data_of(&d).unwrap_or_default() {
                data.insert(((*o).clone(), s.index), s);
            }
        }
        let mut votes: HashMap<(PathBuf, u16), BTreeMap<u32, usize>> = HashMap::new();
        for (&rva, (o, c)) in seats {
            for r in &c.relocations {
                let (key, off) = match (r.external, r.target_index) {
                    (false, Some(idx)) => ((o.clone(), idx), r.target_offset),
                    (true, _) => match defs.get(&r.symbol) {
                        Some((d, idx, off)) => ((d.clone(), *idx), *off),
                        None => continue,
                    },
                    (false, None) => continue,
                };
                if seated.contains_key(&key) {
                    continue;
                }
                if let Some(t) = image_target(env, rva, c, r) {
                    *votes
                        .entry(key)
                        .or_default()
                        .entry(t.wrapping_sub(off))
                        .or_default() += 1;
                }
            }
        }
        let voted: HashMap<(PathBuf, u16), Vote> = votes
            .iter()
            .filter_map(|(k, v)| {
                let (base, sites) = elect(v)?;
                let sec = data.get(k);
                let content = sec.and_then(|s| {
                    same_content(
                        &s.bytes,
                        &s.relocated,
                        env.img.at_rva(Rva(base), s.bytes.len()),
                    )
                });
                Some((
                    k.clone(),
                    Vote {
                        base,
                        sites,
                        content,
                        single: sec.is_some_and(|s| s.comdat),
                    },
                ))
            })
            .collect();
        let vote_names: HashMap<String, (u32, Vote)> = defs
            .iter()
            .filter_map(|(n, (o, idx, off))| {
                let v = voted.get(&(o.clone(), *idx))?;
                Some((n.clone(), (v.base.wrapping_add(*off), *v)))
            })
            .collect();
        let iat = env.iat.iter().map(|(&a, n)| (n.clone(), a)).collect();
        let scopes = scoped.then(|| {
            let stems: HashMap<PathBuf, String> =
                objs.iter().map(|o| ((*o).clone(), scope_stem(o))).collect();
            let definer = defs
                .iter()
                .filter_map(|(n, (o, _, _))| Some((n.clone(), stems.get(o)?.clone())))
                .collect();
            Scopes { stems, definer }
        });
        let mut ctx = Self {
            seated,
            voted,
            seat_names,
            vote_names,
            sym_votes: HashMap::new(),
            iat,
            scopes,
            by_definer: false,
        };
        let text = env.text();
        let mut sym: HashMap<String, BTreeMap<u32, usize>> = HashMap::new();
        for (&rva, (o, c)) in seats {
            for r in &c.relocations {
                if r.symbol.is_empty() || ctx.ours(env, o, r).how != "unresolved" {
                    continue;
                }
                if let Some(t) = image_target(env, rva, c, r) {
                    let t = ilt_slot(text, env.text_rva, t).unwrap_or(t);
                    *sym.entry(r.symbol.clone())
                        .or_default()
                        .entry(t)
                        .or_default() += 1;
                }
            }
        }
        ctx.sym_votes = sym
            .iter()
            .filter_map(|(n, v)| {
                let (base, sites) = elect(v)?;
                Some((
                    n.clone(),
                    Vote {
                        base,
                        sites,
                        content: None,
                        single: false,
                    },
                ))
            })
            .collect();
        ctx
    }

    fn stem(&self, obj: &Path) -> Option<String> {
        let s = self.scopes.as_ref()?;
        Some(s.stems.get(obj).cloned().unwrap_or_else(|| scope_stem(obj)))
    }

    #[must_use]
    pub fn ours(&self, env: &Env, obj: &Path, r: &Reloc) -> Target {
        let local = r.target_index.filter(|_| !r.external);
        if let Some(&b) = local.and_then(|i| self.seated.get(&(obj.to_path_buf(), i))) {
            return Target::new(Some(b.wrapping_add(r.target_offset)), "seat");
        }
        if let Some(v) = env.names.absolute(&r.symbol) {
            return Target::new(Some(v), "abs");
        }
        if let Some(&a) = comdat::import_name(&r.symbol).and_then(|n| self.iat.get(&n)) {
            return Target::new(Some(a), "iat");
        }
        if !r.symbol.is_empty() {
            if let Some(&a) = self.seat_names.get(&r.symbol).filter(|_| r.global) {
                return Target::new(Some(a), "seat");
            }
            if r.global {
                if self.by_definer
                    && let Some(&(a, v)) = self.vote_names.get(&r.symbol)
                {
                    return Target::voted(a, "vote", v);
                }
                let mut scopes: Vec<String> = Vec::new();
                if let (Some(sc), Some(own)) = (self.scopes.as_ref(), self.stem(obj)) {
                    if env.names.has_scope(&own) {
                        scopes.push(own.clone());
                    }
                    if let Some(d) = sc.definer.get(&r.symbol).filter(|d| **d != own)
                        && env.names.has_scope(d)
                    {
                        scopes.push(d.clone());
                    }
                }
                let s: Vec<&str> = scopes.iter().map(String::as_str).collect();
                if let Some(a) = env.names.rva_in(&r.symbol, &s, env.text(), env.text_rva) {
                    return Target::new(Some(a), "name");
                }
            } else if !self.by_definer
                && let Some(a) = self
                    .stem(obj)
                    .and_then(|st| env.names.scoped(&st, &r.symbol))
            {
                return Target::new(Some(a), "name");
            }
        }
        if let Some(&v) = local.and_then(|i| self.voted.get(&(obj.to_path_buf(), i))) {
            return Target::voted(v.base.wrapping_add(r.target_offset), "vote", v);
        }
        if r.symbol.is_empty() {
            return Target::new(None, "unresolved");
        }
        if let Some(&(a, v)) = self.vote_names.get(&r.symbol) {
            return Target::voted(a, "vote", v);
        }
        match self.sym_votes.get(&r.symbol) {
            Some(&v) => Target::voted(v.base, "svote", v),
            None => Target::new(None, "unresolved"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Linked {
    pub at: usize,
    pub symbol: String,
    pub how: &'static str,
    pub ours: Option<u32>,
    pub image: Option<u32>,
    pub vote: Option<Vote>,
    pub kind: u16,
}

#[must_use]
pub fn link_value(kind: u16, t: u32, addend: u32, field_rva: u32, base: u32) -> Option<u32> {
    match kind {
        REL_REL32 => Some(
            t.wrapping_add(addend)
                .wrapping_sub(field_rva.wrapping_add(4)),
        ),
        REL_DIR32 => Some(t.wrapping_add(addend).wrapping_add(base)),
        REL_DIR32NB => Some(t.wrapping_add(addend)),
        _ => None,
    }
}

#[must_use]
pub fn of(env: &Env, ctx: &Ctx, rva: u32, obj: &Path, c: &Comdat, want: usize) -> Raw {
    of_listed(env, ctx, rva, obj, c, want).0
}

#[must_use]
pub fn of_listed(
    env: &Env,
    ctx: &Ctx,
    rva: u32,
    obj: &Path,
    c: &Comdat,
    want: usize,
) -> (Raw, Vec<Diff>, Vec<Linked>) {
    let text = env.text();
    let base = u32::try_from(env.base).unwrap_or(0);
    let mut ilt = 0;
    let mut lone = 0;
    let mut links = Vec::new();
    let fields: Vec<Field> = c
        .relocations
        .iter()
        .map(|r| {
            let at = r.at as usize;
            let Target {
                at: mine,
                how,
                vote,
            } = ctx.ours(env, obj, r);
            let wrong = vote.is_some_and(Vote::wrong);
            lone += usize::from(vote.is_some_and(Vote::lone) && !wrong);
            let addend = read4(&c.bytes, at).unwrap_or(0);
            let field_rva = rva.wrapping_add(r.at);
            let theirs = if how == "abs" {
                let off = field_rva.wrapping_sub(env.text_rva) as usize;
                read4(text, off).map(|v| v.wrapping_sub(addend))
            } else {
                image_target(env, rva, c, r)
            };
            links.push(Linked {
                at,
                symbol: r.symbol.clone(),
                how,
                ours: mine,
                image: theirs,
                vote,
                kind: r.kind,
            });
            let target = match (mine, theirs) {
                (Some(m), Some(t)) if m != t => {
                    let f = |x| ilt_slot(text, env.text_rva, x).unwrap_or(x);
                    if f(m) == f(t) {
                        ilt += 1;
                        Some(t)
                    } else {
                        Some(m)
                    }
                }
                (m, _) => m,
            };
            let based = if how == "abs" { 0 } else { base };
            Field {
                at,
                value: target.and_then(|t| link_value(r.kind, t, addend, field_rva, based)),
                voted: how == "vote",
                wrong,
            }
        })
        .collect();
    let off = rva.wrapping_sub(env.text_rva) as usize;
    let image = text.get(off..).unwrap_or(&[]);
    let want = want.min(image.len().max(c.bytes.len()));
    let (mut r, d) = diffs(&c.bytes, &c.relocated, &fields, image, want);
    r.ilt = ilt;
    r.lone = lone;
    (r, d, links)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_trailing_fill_against_a_stub_is_pad_when_the_body_agrees() {
        let ours = [0x55, 0xc3, 0x90, 0x90];
        let image = [0x55, 0xc3, 0xff, 0x15];
        let (r, _) = diffs(&ours, &[false; 4], &[], &image, 2);
        assert_eq!(r.raw, 0);
        assert_eq!(r.pad, 2);
        let image = [0x56, 0xc3, 0xff, 0x15];
        let (r, _) = diffs(&ours, &[false; 4], &[], &image, 2);
        assert_eq!(r.code, 1);
        assert_eq!(r.len, 2);
    }

    #[test]
    fn a_field_is_free_only_when_it_links_to_the_image_value() {
        let ours = [0xe8, 0, 0, 0, 0];
        let rel = [false, true, true, true, true];
        let image = [0xe8, 0x10, 0, 0, 0];
        let ok = Field {
            at: 1,
            value: Some(0x10),
            voted: false,
            wrong: false,
        };
        assert_eq!(diffs(&ours, &rel, &[ok], &image, 5).0.raw, 0);
        let bad = Field {
            at: 1,
            value: Some(0x11),
            voted: false,
            wrong: false,
        };
        assert_eq!(diffs(&ours, &rel, &[bad], &image, 5).0.field, 1);
        let none = Field {
            at: 1,
            value: None,
            voted: false,
            wrong: false,
        };
        let (r, _) = diffs(&ours, &rel, &[none], &image, 5);
        assert_eq!((r.raw, r.unres), (0, 1));
    }

    #[test]
    fn image_bytes_past_our_end_are_pad_only_when_all_fill() {
        let (r, _) = diffs(&[0xc3], &[false], &[], &[0xc3, 0xcc, 0xcc], 3);
        assert_eq!((r.raw, r.pad), (0, 2));
        let (r, _) = diffs(&[0xc3], &[false], &[], &[0xc3, 0xcc, 0x55], 3);
        assert_eq!(r.len, 2);
    }

    #[test]
    fn a_link_value_is_written_as_link_writes_it() {
        assert_eq!(link_value(REL_REL32, 0x2000, 0, 0x1001, 0), Some(0xffb));
        assert_eq!(
            link_value(REL_DIR32, 0x2000, 4, 0, 0x40_0000),
            Some(0x40_2004)
        );
        assert_eq!(
            link_value(REL_DIR32NB, 0x2000, 4, 0, 0x40_0000),
            Some(0x2004)
        );
        assert_eq!(link_value(9, 0, 0, 0, 0), None);
    }
}
