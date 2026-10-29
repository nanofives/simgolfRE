use anyhow::{Result, bail};
use iced_x86::{Decoder, DecoderOptions, OpKind};
use msvc6_diff::comdat::{self, REL_DIR32, REL_REL32};
use msvc6_diff::env::Env;
use msvc6_diff::rawdiff::target_at;
use msvc6_diff::resolve::follow_thunk;
use msvc6_diff::score;
use msvc6_diff::seat;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};

pub const MAX_CENSUS_BODY: u32 = 1 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Wrong {
    pub object: String,
    pub from: String,
    pub symbol: String,
    pub site: u32,
    pub reference_targets: u32,
    pub symbol_is_at: u32,
    pub from_rva: u32,
    pub our_len: usize,
    pub kind: u16,
    pub our_op: Option<u8>,
    pub ref_op: Option<u8>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Census {
    pub ours: BTreeMap<u32, usize>,
    pub theirs: BTreeMap<u32, usize>,
}

impl Census {
    #[must_use]
    pub fn ours(&self, t: u32) -> usize {
        self.ours.get(&t).copied().unwrap_or(0)
    }

    #[must_use]
    pub fn theirs(&self, t: u32) -> usize {
        self.theirs.get(&t).copied().unwrap_or(0)
    }

    #[must_use]
    pub fn deficit(&self) -> Vec<u32> {
        self.theirs
            .iter()
            .filter(|&(&t, &n)| n > self.ours(t))
            .map(|(&t, _)| t)
            .collect()
    }

    #[must_use]
    pub fn surplus(&self) -> Vec<u32> {
        self.ours
            .iter()
            .filter(|&(&t, &n)| n > self.theirs(t))
            .map(|(&t, _)| t)
            .collect()
    }
}

pub fn count_by_rva<'a>(
    names: impl IntoIterator<Item = &'a str>,
    resolve: impl Fn(&str) -> Option<u32>,
    labels: &mut BTreeMap<u32, BTreeSet<String>>,
) -> BTreeMap<u32, usize> {
    let mut out: BTreeMap<u32, usize> = BTreeMap::new();
    for n in names {
        if let Some(t) = resolve(n) {
            *out.entry(t).or_default() += 1;
            labels.entry(t).or_default().insert(n.to_string());
        }
    }
    out
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum Cause {
    Misaligned,
    Position,
    WrongName,
    WeCallImageInlines,
    ImageCallsWeInline,
    UnnamedTarget,
}

impl Cause {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Misaligned => "a-misaligned",
            Self::Position => "a-position",
            Self::WrongName => "b-wrong-name",
            Self::WeCallImageInlines => "c-we-call",
            Self::ImageCallsWeInline => "c-image-calls",
            Self::UnnamedTarget => "d-unseated-deficit",
        }
    }
}

pub fn classify(c: &Census, s: u32, t_is_entry: bool, named: impl Fn(u32) -> bool) -> Cause {
    let (o, r) = (c.ours(s), c.theirs(s));
    if o == r {
        return if t_is_entry {
            Cause::Position
        } else {
            Cause::Misaligned
        };
    }
    if o < r {
        return Cause::ImageCallsWeInline;
    }
    let d = c.deficit();
    if d.iter().any(|&t| named(t)) {
        Cause::WrongName
    } else if d.is_empty() {
        Cause::WeCallImageInlines
    } else {
        Cause::UnnamedTarget
    }
}

pub fn ref_census(
    text: &[u8],
    base: u32,
    image_base: u32,
    lo: u32,
    hi: u32,
    is_entry: &dyn Fn(u32) -> bool,
) -> BTreeMap<u32, usize> {
    let mut out: BTreeMap<u32, usize> = BTreeMap::new();
    let hi = hi.min(lo.saturating_add(MAX_CENSUS_BODY));
    let (Some(a), Some(b)) = (lo.checked_sub(base), hi.checked_sub(base)) else {
        return out;
    };
    let (a, b) = (a as usize, (b as usize).min(text.len()));
    let Some(bytes) = text.get(a..b) else {
        return out;
    };
    let mut d = Decoder::with_ip(32, bytes, u64::from(lo), DecoderOptions::NONE);
    while d.can_decode() {
        let i = d.decode();
        let mut raw: Vec<u32> = Vec::new();
        if i.is_call_near() || i.is_jmp_near() {
            raw.extend(u32::try_from(i.near_branch_target()).ok());
        }
        for k in 0..i.op_count() {
            if i.op_kind(k) == OpKind::Immediate32 {
                raw.push(i.immediate32().wrapping_sub(image_base));
            }
        }
        for r in raw {
            if (lo..hi).contains(&r) {
                continue;
            }
            let t = follow_thunk(text, base, r);
            if is_entry(t) {
                *out.entry(t).or_default() += 1;
            }
        }
    }
    out
}

pub type Copies = BTreeMap<String, Vec<(usize, PathBuf, Vec<u8>)>>;

#[must_use]
pub fn discarded_select_any(copies: Copies) -> BTreeSet<(PathBuf, String)> {
    let mut out = BTreeSet::new();
    for (n, mut cs) in copies {
        cs.sort_by_key(|(p, _, _)| *p);
        let Some((_, _, kept)) = cs.first().cloned() else {
            continue;
        };
        for (_, o, b) in cs.into_iter().skip(1) {
            if b != kept {
                out.insert((o, n.clone()));
            }
        }
    }
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    Import,
    OwnData,
    Library,
}

impl Kind {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Import => "IMPORT  ",
            Self::OwnData => "OWN DATA",
            Self::Library => "LIBRARY ",
        }
    }
}

#[derive(Debug, Default)]
pub struct Report {
    pub objects: usize,
    pub seated_names: usize,
    pub checked: usize,
    pub agreed: usize,
    pub through_thunk: usize,
    pub thunk_named: usize,
    pub body_named: usize,
    pub uncovered: BTreeMap<String, usize>,
    pub unseated: usize,
    pub dropped: BTreeSet<(PathBuf, String)>,
    pub duplicate: BTreeSet<(PathBuf, String)>,
    pub wrong: Vec<Wrong>,
    pub ours_census: BTreeMap<u32, BTreeMap<u32, usize>>,
    pub labels: BTreeMap<u32, BTreeSet<String>>,
    pub at: BTreeMap<String, u32>,
    pub own_data: BTreeSet<String>,
    pub refused: Vec<(PathBuf, String)>,
}

impl Report {
    #[must_use]
    pub fn kind_of(&self, env: &Env, name: &str) -> Kind {
        if self.own_data.contains(name) {
            return Kind::OwnData;
        }
        let bare = name
            .trim_start_matches("__imp_")
            .trim_start_matches('_')
            .split('@')
            .next()
            .unwrap_or(name);
        if env.iat.values().any(|n| n == name || n == bare) {
            Kind::Import
        } else {
            Kind::Library
        }
    }

    #[must_use]
    pub fn uncovered_by_kind(&self, env: &Env) -> (usize, usize, usize) {
        let (mut imp, mut own, mut lib) = (0usize, 0usize, 0usize);
        for (s, n) in &self.uncovered {
            match self.kind_of(env, s) {
                Kind::Import => imp += n,
                Kind::OwnData => own += n,
                Kind::Library => lib += n,
            }
        }
        (imp, own, lib)
    }
}

fn op_before(bytes: &[u8], k: u32) -> Option<u8> {
    k.checked_sub(1)
        .and_then(|i| usize::try_from(i).ok())
        .and_then(|i| bytes.get(i).copied())
}

#[derive(Debug)]
pub struct Inputs<'a> {
    pub env: &'a Env,
    pub objs: &'a [PathBuf],
    pub census: bool,
    pub map_data: &'a BTreeSet<String>,
}

pub fn measure(inp: &Inputs<'_>) -> Result<Report> {
    let env = inp.env;
    let text = env.text();
    let base = env.text_rva;
    let image_base = u32::try_from(env.base).unwrap_or(0);
    let mut rep = Report {
        objects: inp.objs.len(),
        ..Report::default()
    };
    let mut copies: Copies = BTreeMap::new();
    let mut loaded: Vec<(PathBuf, Vec<comdat::Comdat>)> = Vec::new();
    for (pos, o) in inp.objs.iter().enumerate() {
        let parsed = comdat::read(o).and_then(|d| {
            let bodies = comdat::text_of(&d)?;
            let members = seat::members_of(env, o, &d, &bodies)?;
            Ok((d, bodies, members))
        });
        let (d, bodies, members) = match parsed {
            Ok(x) => x,
            Err(e) => {
                rep.refused.push((o.clone(), format!("{e:#}")));
                continue;
            }
        };
        let weak = comdat::weak_defaults_of(&d).unwrap_or_default();
        let any = score::select_any_sections(&d);
        for c in &bodies {
            let Some(n) = c.symbol.as_deref() else {
                continue;
            };
            if c.external && any.contains(&c.index) {
                copies
                    .entry(n.to_string())
                    .or_default()
                    .push((pos, o.clone(), c.bytes.clone()));
            }
        }
        for m in members {
            let Some(r) = m.rva else {
                continue;
            };
            let image_from_seat = r
                .checked_sub(base)
                .and_then(|k| usize::try_from(k).ok())
                .and_then(|k| text.get(k..));
            let superseded = image_from_seat.is_some_and(|img| {
                bodies
                    .iter()
                    .filter(|c| c.symbol.as_deref() == Some(m.name.as_str()))
                    .any(|c| score::superseded_at(c.symbol.as_deref(), &c.bytes, &weak, img, r))
            });
            if superseded {
                rep.dropped.insert((o.clone(), m.name));
                continue;
            }
            rep.at.entry(m.name).or_insert(r);
        }
        if let Ok(v) = comdat::data_of(&d) {
            rep.own_data.extend(v.into_iter().filter_map(|c| c.symbol));
        }
        if let Ok(b) = crate::coffx::bss(&d) {
            rep.own_data
                .extend(b.into_iter().flat_map(|b| b.symbols).map(|(_, s)| s));
        }
        loaded.push((o.clone(), bodies));
    }
    rep.own_data.extend(inp.map_data.iter().cloned());
    rep.duplicate = discarded_select_any(copies);
    rep.seated_names = rep.at.len();
    if rep.at.is_empty() {
        bail!(
            "no COMDAT of the {} object(s) seats at a reference address, so every call site would be uncovered and the check would pass while testing nothing",
            inp.objs.len()
        );
    }
    let at = rep.at.clone();
    let mut db_named: BTreeMap<&str, Option<u32>> = BTreeMap::new();
    for (&r, n) in &env.function_names {
        db_named
            .entry(n.as_str())
            .and_modify(|v| *v = v.filter(|&x| x == r))
            .or_insert(Some(r));
    }
    let resolve = |n: &str| -> Option<u32> {
        at.get(n)
            .map(|&v| follow_thunk(text, base, v))
            .or_else(|| env.names.rva(n, text, base))
            .or_else(|| db_named.get(n).copied().flatten())
    };
    for (o, comdats) in &loaded {
        let stem = seat::scope_stem(o);
        for c in comdats {
            let Some(from) = c.symbol.clone().filter(|_| c.external) else {
                continue;
            };
            let key = (o.clone(), from.clone());
            if rep.dropped.contains(&key) || rep.duplicate.contains(&key) {
                continue;
            }
            let Some(&r) = rep.at.get(&from) else {
                rep.unseated += 1;
                continue;
            };
            if inp.census && !rep.ours_census.contains_key(&r) {
                let n = count_by_rva(
                    c.relocations.iter().map(|x| x.symbol.as_str()),
                    resolve,
                    &mut rep.labels,
                );
                rep.ours_census.insert(r, n);
            }
            for reloc in &c.relocations {
                if !reloc.external || reloc.symbol.is_empty() {
                    continue;
                }
                let Some(raw) = target_at(image_base, r, c, reloc, text, base) else {
                    *rep.uncovered.entry(reloc.symbol.clone()).or_default() += 1;
                    continue;
                };
                let a = follow_thunk(text, base, raw);
                if a != raw {
                    rep.through_thunk += 1;
                }
                let Some(&rs) = rep.at.get(&reloc.symbol) else {
                    *rep.uncovered.entry(reloc.symbol.clone()).or_default() += 1;
                    continue;
                };
                rep.checked += 1;
                if a != raw {
                    if rs == raw {
                        rep.thunk_named += 1;
                    } else if rs == a {
                        rep.body_named += 1;
                    }
                }
                if a == rs || raw == rs {
                    rep.agreed += 1;
                } else {
                    let site = r.saturating_add(reloc.at);
                    rep.wrong.push(Wrong {
                        from_rva: r,
                        our_len: c.bytes.len(),
                        kind: reloc.kind,
                        our_op: op_before(&c.bytes, reloc.at),
                        ref_op: site.checked_sub(base).and_then(|k| op_before(text, k)),
                        object: stem.clone(),
                        from: from.clone(),
                        symbol: reloc.symbol.clone(),
                        site,
                        reference_targets: a,
                        symbol_is_at: rs,
                    });
                }
            }
        }
    }
    Ok(rep)
}

fn by_name(set: &BTreeSet<(PathBuf, String)>) -> BTreeMap<String, usize> {
    let mut m: BTreeMap<String, usize> = BTreeMap::new();
    for (_, n) in set {
        *m.entry(n.clone()).or_default() += 1;
    }
    m
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
pub fn report_lines(env: &Env, rep: &Report, list_uncovered: bool) -> Vec<String> {
    let mut out = vec![
        format!(
            "{}: CALL TARGETS -- does each of our call sites name the function the REFERENCE calls there?",
            env.label
        ),
        format!(
            "  {} object(s), {} name(s) seated at a reference address by name",
            rep.objects, rep.seated_names
        ),
        format!("    resolved THROUGH an ILT thunk {:>5}", rep.through_thunk),
        format!("    call sites CHECKED          {:>6}", rep.checked),
        format!("    agreed with the reference   {:>6}", rep.agreed),
        format!("    DISAGREED                   {:>6}", rep.wrong.len()),
    ];
    if rep.through_thunk > 0 {
        out.push(format!(
            "  of the {} site(s) the reference reached through a thunk, we name the THUNK at {} and the BODY at {}",
            rep.through_thunk, rep.thunk_named, rep.body_named
        ));
    }
    let unc: usize = rep.uncovered.values().sum();
    out.push(format!(
        "    uncovered (no reference address for the callee) {unc:>6} site(s) over {} name(s)",
        rep.uncovered.len()
    ));
    out.push(format!(
        "    COMDATs whose own body did not seat, so their sites were skipped: {}",
        rep.unseated
    ));
    out.push(format!(
        "    COMDATs LINK drops for the image's vector deleting dtor (SUPERSEDED in `try'), not checked: {}",
        rep.dropped.len()
    ));
    for (n, k) in &by_name(&rep.dropped) {
        out.push(format!("      {k:>3} object(s): {n}"));
    }
    out.push(format!(
        "    SELECT_ANY copies LINK discards for an earlier TU's different copy (DUPLICATE in `try'), not checked: {}",
        rep.duplicate.len()
    ));
    for (n, k) in &by_name(&rep.duplicate) {
        out.push(format!("      {k:>3} object(s): {n}"));
    }
    let (imp, own, lib) = rep.uncovered_by_kind(env);
    out.push(format!(
        "    of those uncovered site(s): {imp} IMPORTED, {own} OUR OWN DATA, {lib} an ARCHIVE MEMBER's name."
    ));
    if list_uncovered {
        for (s, n) in &rep.uncovered {
            out.push(format!(
                "      {n:>4} site(s)  {}  {s}",
                rep.kind_of(env, s).label()
            ));
        }
    } else if !rep.uncovered.is_empty() {
        for (s, n) in rep.uncovered.iter().take(6) {
            out.push(format!("      {n:>4} site(s) -> {}", clip(s, 58)));
        }
        out.push(format!(
            "      ^^ first 6 only; pass `--list-uncovered' for all {} with their kind",
            rep.uncovered.len()
        ));
    }
    for w in rep.wrong.iter().take(10) {
        out.push(format!(
            "    WRONG CALLEE  {}:{}",
            w.object,
            clip(&w.from, 46)
        ));
        out.push(format!(
            "      site {:#08x} names `{}', which the reference places at {:#08x},",
            w.site,
            clip(&w.symbol, 46),
            w.symbol_is_at
        ));
        out.push(format!(
            "      but the reference calls {:#08x} from that site",
            w.reference_targets
        ));
    }
    if rep.wrong.is_empty() {
        out.push(format!("  ALL {} CHECKED CALL SITE(S) AGREE.", rep.agreed));
    } else {
        out.push(format!(
            "CALLTARGETS {}: {} call site(s) target a different function than the reference does",
            env.label,
            rep.wrong.len()
        ));
    }
    out
}

#[derive(Debug, Default)]
pub struct Causes {
    pub rows: String,
    pub bodies: String,
    pub tally: BTreeMap<Cause, usize>,
    pub per_object: BTreeMap<(String, Cause), usize>,
    pub moved: usize,
    pub calling: usize,
}

#[must_use]
pub fn causes(env: &Env, rep: &Report) -> Causes {
    let text = env.text();
    let base = env.text_rva;
    let image_base = u32::try_from(env.base).unwrap_or(0);
    let mut names_at: BTreeMap<u32, Vec<&str>> = BTreeMap::new();
    for (n, &r) in &rep.at {
        names_at
            .entry(follow_thunk(text, base, r))
            .or_default()
            .push(n.as_str());
    }
    let is_entry = |t: u32| {
        env.recorded(t).is_some()
            || env.function_names.contains_key(&t)
            || names_at.contains_key(&t)
    };
    let name_of = |t: u32| -> String {
        let mut v: BTreeSet<&str> = names_at.get(&t).into_iter().flatten().copied().collect();
        v.extend(rep.labels.get(&t).into_iter().flatten().map(String::as_str));
        if v.is_empty() {
            env.function_names
                .get(&t)
                .map_or_else(|| "-".to_string(), |f| format!("(unseated){f}"))
        } else {
            v.into_iter().collect::<Vec<_>>().join(",")
        }
    };
    let ref_len = |w: &Wrong| -> u32 {
        env.recorded(w.from_rva)
            .unwrap_or_else(|| u32::try_from(w.our_len).unwrap_or(0))
    };
    let mut census: BTreeMap<u32, Census> = BTreeMap::new();
    for w in &rep.wrong {
        census.entry(w.from_rva).or_insert_with(|| {
            let o: BTreeMap<u32, usize> = rep
                .ours_census
                .get(&w.from_rva)
                .into_iter()
                .flatten()
                .filter(|&(&t, _)| t != w.from_rva && is_entry(t))
                .map(|(&t, &n)| (t, n))
                .collect();
            Census {
                ours: o,
                theirs: ref_census(
                    text,
                    base,
                    image_base,
                    w.from_rva,
                    w.from_rva.saturating_add(ref_len(w)),
                    &is_entry,
                ),
            }
        });
    }
    let list = |c: &Census, v: Vec<u32>| -> String {
        let s: Vec<String> = v
            .into_iter()
            .map(|t| format!("{t:#x}={}:{}/{}", name_of(t), c.ours(t), c.theirs(t)))
            .collect();
        if s.is_empty() {
            "-".into()
        } else {
            s.join(" ")
        }
    };
    let hexb = |b: Option<u8>| b.map_or_else(|| "-".to_string(), |b| format!("{b:02x}"));
    let mut out = Causes {
        rows: String::from(
            "cause\tobject\tfrom\tfrom_rva\tour_len\tref_len\tsite\tkind\tour_op\tref_op\tsymbol\tsymbol_at\tref_target\tref_target_name\tours_s\tref_s\n",
        ),
        bodies: String::from(
            "object\tfrom\tfrom_rva\tour_len\tref_len\tsurplus(ours/ref)\tdeficit(ours/ref)\n",
        ),
        ..Causes::default()
    };
    for w in &rep.wrong {
        let Some(c) = census.get(&w.from_rva) else {
            continue;
        };
        let cause = classify(c, w.symbol_is_at, is_entry(w.reference_targets), |t| {
            names_at.contains_key(&t)
        });
        *out.tally.entry(cause).or_default() += 1;
        *out.per_object.entry((w.object.clone(), cause)).or_default() += 1;
        let kind = match w.kind {
            REL_REL32 => "rel32",
            REL_DIR32 => "dir32",
            _ => "other",
        };
        let _ = writeln!(
            out.rows,
            "{}\t{}\t{}\t{:#x}\t{}\t{}\t{:#x}\t{}\t{}\t{}\t{}\t{:#x}\t{:#x}\t{}\t{}\t{}",
            cause.label(),
            w.object,
            w.from,
            w.from_rva,
            w.our_len,
            ref_len(w),
            w.site,
            kind,
            hexb(w.our_op),
            hexb(w.ref_op),
            w.symbol,
            w.symbol_is_at,
            w.reference_targets,
            name_of(w.reference_targets),
            c.ours(w.symbol_is_at),
            c.theirs(w.symbol_is_at),
        );
    }
    let mut seen: BTreeSet<u32> = BTreeSet::new();
    for w in &rep.wrong {
        if !seen.insert(w.from_rva) {
            continue;
        }
        let Some(c) = census.get(&w.from_rva) else {
            continue;
        };
        let (s, d) = (c.surplus(), c.deficit());
        if s.is_empty() && d.is_empty() {
            continue;
        }
        out.moved += 1;
        let _ = writeln!(
            out.bodies,
            "{}\t{}\t{:#x}\t{}\t{}\t{}\t{}",
            w.object,
            w.from,
            w.from_rva,
            w.our_len,
            ref_len(w),
            list(c, s),
            list(c, d)
        );
    }
    out.calling = seen.len();
    out
}

#[must_use]
pub fn causes_lines(c: &Causes, wrong: usize, rows: &Path, bodies: &Path) -> Vec<String> {
    let mut out = vec![format!(
        "  CAUSES ({wrong} row(s) over {} calling bod(ies), {} of them with a moved callee census)",
        c.calling, c.moved
    )];
    out.push(format!("    rows:   {}", rows.display()));
    out.push(format!("    bodies: {}", bodies.display()));
    for (k, n) in &c.tally {
        out.push(format!("    {:<18} {n:>6}", k.label()));
    }
    out.push("  PER OBJECT, rows not a-*:".to_string());
    for ((o, k), n) in &c.per_object {
        if !matches!(k, Cause::Misaligned | Cause::Position) {
            out.push(format!("    {o:<28} {:<18} {n:>5}", k.label()));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn two_names_at_one_rva_are_one_row_with_equal_counts() {
        let rows: BTreeMap<&str, u32> = [
            ("__itoa", 0xad425),
            ("?IntToString@@YAPADHPADH@Z", 0xad425),
            ("?Other@@YAXXZ", 0x1000),
        ]
        .into_iter()
        .collect();
        let calls = [
            "__itoa",
            "?IntToString@@YAPADHPADH@Z",
            "__itoa",
            "?Other@@YAXXZ",
            "?unresolved@@YAXXZ",
        ];
        let mut labels = BTreeMap::new();
        let ours = count_by_rva(calls, |n| rows.get(n).copied(), &mut labels);
        assert_eq!(ours.len(), 2);
        assert_eq!(ours.get(&0xad425), Some(&3));
        let c = Census {
            ours,
            theirs: [(0xad425, 3), (0x1000, 1)].into_iter().collect(),
        };
        assert!(c.deficit().is_empty() && c.surplus().is_empty());
        assert_eq!(
            labels.get(&0xad425).map(|s| s.len()),
            Some(2),
            "both names kept as the label"
        );
    }

    #[test]
    fn only_a_later_different_select_any_copy_is_discarded() {
        let n = "??1Base@@QAE@XZ".to_string();
        let gx = vec![0x6a, 0xff, 0x68, 0, 0, 0, 0];
        let jmp = vec![0xe9, 0, 0, 0, 0];
        let mut copies: Copies = BTreeMap::new();
        copies.insert(
            n.clone(),
            vec![
                (9, "Late.obj".into(), jmp),
                (7, "Early.obj".into(), gx.clone()),
                (8, "Same.obj".into(), gx),
            ],
        );
        copies.insert(
            "?only@@YAXXZ".to_string(),
            vec![(1, "One.obj".into(), vec![0xc3])],
        );
        assert_eq!(
            discarded_select_any(copies).into_iter().collect::<Vec<_>>(),
            [(PathBuf::from("Late.obj"), n)]
        );
    }

    #[test]
    fn a_cause_is_read_from_the_body_census() {
        let c = Census {
            ours: [(0x10, 2), (0x20, 1)].into_iter().collect(),
            theirs: [(0x10, 1), (0x30, 1), (0x20, 1)].into_iter().collect(),
        };
        assert_eq!(classify(&c, 0x20, true, |_| false), Cause::Position);
        assert_eq!(classify(&c, 0x20, false, |_| false), Cause::Misaligned);
        assert_eq!(classify(&c, 0x10, true, |t| t == 0x30), Cause::WrongName);
        assert_eq!(classify(&c, 0x10, true, |_| false), Cause::UnnamedTarget);
        assert_eq!(
            classify(&c, 0x30, true, |_| false),
            Cause::ImageCallsWeInline
        );
        let none = Census {
            ours: [(0x10, 2)].into_iter().collect(),
            theirs: [(0x10, 1)].into_iter().collect(),
        };
        assert_eq!(
            classify(&none, 0x10, true, |_| true),
            Cause::WeCallImageInlines
        );
    }

    #[test]
    fn clipping_keeps_whole_characters() {
        assert_eq!(clip("abcdef", 3), "abc");
        assert_eq!(clip("ab", 58), "ab");
        assert_eq!(clip("\u{e9}\u{e9}", 3), "\u{e9}");
    }
}
