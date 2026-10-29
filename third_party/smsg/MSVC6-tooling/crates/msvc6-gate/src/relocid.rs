use crate::libs::{self, Member};
use iced_x86::{Decoder, DecoderOptions, FlowControl, OpKind};
use msvc6_base::units::Rva;
use msvc6_diff::comdat::{self, Comdat, REL_DIR32};
use msvc6_diff::env::Env;
use msvc6_diff::par;
use msvc6_diff::rawdiff::{self, Ctx, Linked, Vote};
use msvc6_diff::resolve::ilt_slot;
use msvc6_diff::seat::{Seats, scope_stem};
use msvc6_diff::segments;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, serde::Serialize)]
pub enum Verdict {
    Mismatch,
    WrongDatum,
    LibraryConflict,
    NameConflict,
    NotRelocated,
    ExtraReloc,
    Layout,
    StaleName,
    GhidraDiff,
    Literal,
    LiteralWeak,
    Unresolved,
    Named,
    Weak,
    Ilt,
    Content,
    Library,
    Ghidra,
    Voted,
    Fitted,
}

impl Verdict {
    pub const ALL: [Self; 20] = [
        Self::Mismatch,
        Self::WrongDatum,
        Self::LibraryConflict,
        Self::NameConflict,
        Self::NotRelocated,
        Self::ExtraReloc,
        Self::Layout,
        Self::StaleName,
        Self::GhidraDiff,
        Self::Literal,
        Self::LiteralWeak,
        Self::Unresolved,
        Self::Named,
        Self::Weak,
        Self::Ilt,
        Self::Content,
        Self::Library,
        Self::Ghidra,
        Self::Voted,
        Self::Fitted,
    ];

    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Mismatch => "MISMATCH",
            Self::WrongDatum => "WRONG-DATUM",
            Self::LibraryConflict => "LIBRARY-CONFLICT",
            Self::NameConflict => "NAME-CONFLICT",
            Self::NotRelocated => "NOT-RELOCATED",
            Self::ExtraReloc => "EXTRA-RELOC",
            Self::Layout => "LAYOUT",
            Self::StaleName => "STALE-NAME",
            Self::GhidraDiff => "GHIDRA-DIFF",
            Self::Literal => "LITERAL",
            Self::LiteralWeak => "LITERAL-WEAK",
            Self::Unresolved => "UNRESOLVED",
            Self::Named => "NAMED",
            Self::Weak => "WEAK",
            Self::Ilt => "ILT",
            Self::Content => "CONTENT",
            Self::Library => "LIBRARY",
            Self::Ghidra => "GHIDRA",
            Self::Voted => "VOTED",
            Self::Fitted => "FITTED",
        }
    }

    #[must_use]
    pub const fn finding(self) -> bool {
        matches!(
            self,
            Self::Mismatch
                | Self::WrongDatum
                | Self::LibraryConflict
                | Self::NameConflict
                | Self::NotRelocated
                | Self::ExtraReloc
                | Self::Literal
        )
    }

    #[must_use]
    pub const fn lead(self) -> bool {
        matches!(
            self,
            Self::Layout
                | Self::StaleName
                | Self::GhidraDiff
                | Self::LiteralWeak
                | Self::Unresolved
        )
    }

    #[must_use]
    pub const fn unproven(self) -> bool {
        matches!(self, Self::Voted | Self::Fitted)
    }
}

#[derive(Debug, Clone, serde::Serialize)]
pub struct Row {
    pub rva: u32,
    pub function: String,
    pub off: u32,
    pub symbol: String,
    pub how: &'static str,
    pub verdict: Verdict,
    pub ours: Option<u32>,
    pub image: Option<u32>,
    pub note: String,
}

#[derive(Debug, Default)]
struct Oracles {
    recorded: BTreeMap<u32, BTreeMap<String, bool>>,
    scoped: HashMap<String, HashMap<String, u32>>,
    defined: HashSet<String>,
    library: HashMap<String, Vec<Member>>,
    ghidra: BTreeMap<u32, String>,
    base_relocs: BTreeSet<u32>,
}

fn bookkeeping(source: &str, notes_only: &[PathBuf]) -> bool {
    let p = Path::new(source);
    notes_only.iter().any(|n| p.ends_with(n))
}

impl Oracles {
    fn load(
        env: &Env,
        objs: &[PathBuf],
        library: HashMap<String, Vec<Member>>,
        notes_only: &[PathBuf],
    ) -> Self {
        let text = env.text();
        let mut recorded: BTreeMap<u32, BTreeMap<String, bool>> = BTreeMap::new();
        let mut add = |n: &str, r: u32, manifest: bool| {
            for at in std::iter::once(r).chain(ilt_slot(text, env.text_rva, r)) {
                let e = recorded
                    .entry(at)
                    .or_default()
                    .entry(n.to_string())
                    .or_insert(manifest);
                *e &= manifest;
            }
        };
        for (n, r) in env.names.exports() {
            add(n, *r, false);
        }
        let mut scoped: HashMap<String, HashMap<String, u32>> = HashMap::new();
        for t in env.names.tiers() {
            let book = bookkeeping(t.source, notes_only);
            for (name, rva, scope) in t.rows {
                match scope {
                    None => add(name, *rva, book),
                    Some(s) => {
                        if t.flat.get(name) == Some(rva) {
                            continue;
                        }
                        add(name, *rva, book);
                        if !book {
                            scoped
                                .entry(s.clone())
                                .or_default()
                                .entry(name.clone())
                                .or_insert(*rva);
                        }
                    }
                }
            }
        }
        for (r, n) in &env.iat {
            add(&format!("__imp_{n}"), *r, false);
        }
        let mut defined = HashSet::new();
        for o in objs {
            let Ok(d) = comdat::read(o) else { continue };
            for (_, _, n) in comdat::defined_externals(&d).unwrap_or_default() {
                defined.insert(n);
            }
            for c in comdat::text_of(&d).unwrap_or_default() {
                defined.extend(c.symbol);
            }
        }
        let base_relocs = env
            .img
            .relocations()
            .into_iter()
            .filter(|(_, t)| *t == 3)
            .map(|(r, _)| r.get())
            .collect();
        Self {
            recorded,
            scoped,
            defined,
            library,
            ghidra: env.function_names.clone(),
            base_relocs,
        }
    }

    fn names_at(&self, t: u32, body: u32, claims: bool) -> BTreeSet<String> {
        [t, body]
            .iter()
            .filter_map(|r| self.recorded.get(r))
            .flatten()
            .filter(|(_, manifest)| !claims || !**manifest)
            .map(|(n, manifest)| {
                if *manifest {
                    format!("{n} [manifest]")
                } else {
                    n.clone()
                }
            })
            .collect()
    }
}

#[must_use]
pub fn c_name(s: &str) -> Option<&str> {
    let s = s.strip_prefix("__imp_").unwrap_or(s);
    if s.starts_with('?') || s.contains("::") || s.is_empty() {
        return None;
    }
    let s = s.trim_start_matches('_');
    Some(match s.rsplit_once('@') {
        Some((a, d)) if !a.is_empty() && !d.is_empty() && d.bytes().all(|b| b.is_ascii_digit()) => {
            a
        }
        _ => s,
    })
}

fn named_symbol(s: &str) -> bool {
    !(s.is_empty() || s.starts_with('.') || s.starts_with('$'))
}

#[must_use]
pub fn weak_default(s: &str) -> Option<String> {
    s.strip_prefix("??_E").map(|t| format!("??_G{t}"))
}

#[must_use]
pub fn hex(x: Option<u32>) -> String {
    x.map_or_else(|| "?".to_string(), |x| format!("{x:#x}"))
}

#[must_use]
pub fn signed_hex(d: i64) -> String {
    if d < 0 {
        format!("-{:#x}", d.unsigned_abs())
    } else {
        format!("+{d:#x}")
    }
}

fn holds_member(env: &Env, at: u32, m: &Member) -> bool {
    rawdiff::same_content(
        &m.section.bytes,
        &m.section.relocated,
        env.img
            .at_rva(Rva(at.wrapping_sub(m.off)), m.section.bytes.len()),
    ) == Some(true)
}

fn library_verdict(
    env: &Env,
    copies: &[Member],
    body: u32,
    names: &dyn Fn(u32) -> String,
    t: u32,
) -> (Verdict, String) {
    if copies.iter().any(|m| holds_member(env, body, m)) {
        return (Verdict::Library, String::new());
    }
    let from: Vec<String> = copies
        .iter()
        .map(|m| format!("{} (+{:#x})", m.from, m.off))
        .collect();
    (
        Verdict::LibraryConflict,
        format!(
            "the image does not hold {} there; recorded: {}",
            from.join(" or "),
            names(t)
        ),
    )
}

fn classify(env: &Env, or: &Oracles, stem: &str, l: &Linked) -> (Verdict, String) {
    let text = env.text();
    let follow = |x: u32| ilt_slot(text, env.text_rva, x).unwrap_or(x);
    let names = |t: u32| {
        or.names_at(t, follow(t), false)
            .into_iter()
            .collect::<Vec<_>>()
            .join(",")
    };
    let (Some(m), Some(t)) = (l.ours, l.image) else {
        if l.ours.is_none() && l.symbol.starts_with("__imp_") {
            return (
                Verdict::Mismatch,
                format!(
                    "the image imports no such function; it reads {}: {}",
                    hex(l.image),
                    l.image.map(names).unwrap_or_default()
                ),
            );
        }
        let side = if l.ours.is_none() {
            "ours"
        } else {
            "the image's"
        };
        return (Verdict::Unresolved, format!("{side} target unresolved"));
    };
    if matches!(l.how, "vote" | "svote") {
        let own = or.scoped.get(stem).and_then(|s| s.get(&l.symbol));
        let other: Vec<(&String, u32)> = or
            .scoped
            .iter()
            .filter(|(s, _)| s.as_str() != stem)
            .filter_map(|(s, n)| n.get(&l.symbol).map(|r| (s, *r)))
            .collect();
        let (rec, whence) = match (own, other.as_slice()) {
            (Some(&r), _) => (Some(r), format!("[names.{stem}]")),
            (None, [(s, r)]) => (Some(*r), format!("[names.{s}] (another object's scope)")),
            _ => (None, String::new()),
        };
        if let Some(r) = rec {
            return if follow(r) == follow(t) {
                (Verdict::Named, format!("by {whence}"))
            } else {
                (
                    Verdict::Mismatch,
                    format!(
                        "{whence} records it at {r:#x}; the image reads {t:#x}: {}",
                        names(t)
                    ),
                )
            };
        }
    }
    if m != t {
        if follow(m) == follow(t) {
            return (Verdict::Ilt, String::new());
        }
        return match l.how {
            "vote" if l.vote.is_some_and(Vote::wrong) => (
                Verdict::WrongDatum,
                "the image does not hold this datum's bytes where its code reads it".to_string(),
            ),
            "vote" => (
                Verdict::Layout,
                format!(
                    "image {} from the section's elected base; recorded there: {}",
                    signed_hex(i64::from(t) - i64::from(m)),
                    names(t)
                ),
            ),
            _ => (
                Verdict::Mismatch,
                format!("recorded at the image's target: {}", names(t)),
            ),
        };
    }
    let fitted = |v: Option<Vote>| {
        if v.is_some_and(|v| v.sites >= 2) {
            Verdict::Voted
        } else {
            Verdict::Fitted
        }
    };
    let others = || -> Option<(Verdict, String)> {
        if !named_symbol(&l.symbol) {
            return None;
        }
        let at = or.names_at(t, follow(t), true);
        if at.contains(&l.symbol) {
            return None;
        }
        if weak_default(&l.symbol).is_some_and(|g| at.contains(&g)) {
            return Some((Verdict::Weak, String::new()));
        }
        let live: Vec<&str> = at
            .iter()
            .filter(|n| or.defined.contains(*n))
            .map(String::as_str)
            .collect();
        if !live.is_empty() {
            return Some((
                Verdict::NameConflict,
                format!("defined and recorded there: {}", live.join(",")),
            ));
        }
        (!at.is_empty()).then(|| {
            (
                Verdict::StaleName,
                format!(
                    "recorded there, defined by no object: {}",
                    at.into_iter().collect::<Vec<_>>().join(",")
                ),
            )
        })
    };
    match l.how {
        "vote" => {
            let v = l.vote;
            if v.is_some_and(Vote::wrong) {
                return (
                    Verdict::WrongDatum,
                    "the image does not hold this datum's bytes where its code reads it"
                        .to_string(),
                );
            }
            if v.and_then(|v| v.content) == Some(true) {
                return (Verdict::Content, String::new());
            }
            others().unwrap_or_else(|| (fitted(v), String::new()))
        }
        "svote" => {
            let body = follow(t);
            if let Some(copies) = or.library.get(&l.symbol).filter(|c| !c.is_empty()) {
                return library_verdict(env, copies, body, &names, t);
            }
            if let Some(o) = others() {
                return o;
            }
            if let (Some(g), Some(ours)) = (or.ghidra.get(&body), c_name(&l.symbol)) {
                match c_name(g) {
                    Some(gc) if gc == ours => return (Verdict::Ghidra, String::new()),
                    Some(_) => return (Verdict::GhidraDiff, format!("Ghidra names it {g}")),
                    None => {}
                }
            }
            (fitted(l.vote), String::new())
        }
        _ => (Verdict::Named, String::new()),
    }
}

#[must_use]
pub fn abs_fields(
    insn: &iced_x86::Instruction,
    co: &iced_x86::ConstantOffsets,
    base: u64,
    top: u64,
) -> Vec<(usize, u32)> {
    let mut v: Vec<(usize, u64)> = Vec::new();
    let call = insn.flow_control() == FlowControl::Call
        && (0..insn.op_count()).any(|op| insn.op_kind(op) == OpKind::NearBranch32);
    if call {
        v.push((insn.len().saturating_sub(4), insn.near_branch_target()));
    }
    if co.has_displacement() && co.displacement_size() == 4 {
        v.push((
            co.displacement_offset(),
            u64::from(insn.memory_displacement32()),
        ));
    }
    if !call && co.has_immediate() && co.immediate_size() == 4 {
        let imm = (0..insn.op_count())
            .find(|op| {
                matches!(
                    insn.op_kind(*op),
                    OpKind::Immediate32 | OpKind::Immediate8to32
                )
            })
            .map(|op| insn.immediate(op));
        if let Some(x) = imm {
            v.push((co.immediate_offset(), x & 0xffff_ffff));
        }
    }
    v.sort_unstable();
    v.into_iter()
        .filter(|(_, x)| *x >= base && *x < top)
        .filter_map(|(o, x)| u32::try_from(x.checked_sub(base)?).ok().map(|r| (o, r)))
        .collect()
}

fn literals(
    env: &Env,
    or: &Oracles,
    at: u32,
    c: &Comdat,
    ext: usize,
) -> Vec<(u32, u32, String, bool)> {
    let Some(b) = env.img.at_rva(Rva(at), ext) else {
        return Vec::new();
    };
    let mut d = Decoder::with_ip(
        32,
        b,
        env.base.saturating_add(u64::from(at)),
        DecoderOptions::NONE,
    );
    let mut out = Vec::new();
    while d.can_decode() {
        let pos = d.position();
        let i = d.decode();
        if i.is_invalid() {
            break;
        }
        let co = d.get_constant_offsets(&i);
        if i.flow_control() == FlowControl::Call && i.op0_kind() == OpKind::NearBranch32 {
            continue;
        }
        for (fo, r) in abs_fields(&i, &co, env.base, env.top) {
            let k = pos.saturating_add(fo);
            if c.relocated.get(k).copied().unwrap_or(false) {
                continue;
            }
            let mut n: Vec<String> = or.names_at(r, r, false).into_iter().collect();
            if let Some(g) = or.ghidra.get(&r) {
                n.push(format!("{g} [ghidra]"));
            }
            let live = or
                .recorded
                .get(&r)
                .is_some_and(|m| m.keys().any(|k| or.defined.contains(k)));
            if !n.is_empty() {
                out.push((u32::try_from(k).unwrap_or(u32::MAX), r, n.join(","), live));
            }
        }
    }
    out
}

fn linked(env: &Env, ctx: &Ctx, at: u32, obj: &Path, c: &Comdat) -> Option<(Vec<Linked>, usize)> {
    let (img_want, whence) = segments::image_want(env, at, c.bytes.len());
    let (want, _) = segments::raw_extent(img_want, whence, c.bytes.len());
    let (raw, diffs, links) = rawdiff::of_listed(env, ctx, at, obj, c, want);
    let ext = diffs
        .iter()
        .find(|d| d.kind == "pad")
        .map_or(c.bytes.len(), |d| d.off.min(c.bytes.len()));
    (raw.code == 0 && raw.len == 0).then_some((links, ext))
}

fn rows_of(
    env: &Env,
    or: &Oracles,
    at: u32,
    c: &Comdat,
    stem: &str,
    links: &[Linked],
    ext: usize,
) -> Vec<Row> {
    let function = c.symbol.clone().unwrap_or_else(|| "?".to_string());
    let row = |off: u32, symbol: &str, how, verdict, ours, image, note: String| Row {
        rva: at,
        function: function.clone(),
        off,
        symbol: symbol.to_string(),
        how,
        verdict,
        ours,
        image,
        note,
    };
    let mut rows: Vec<Row> = links
        .iter()
        .map(|l| {
            let (v, note) = classify(env, or, stem, l);
            row(
                u32::try_from(l.at).unwrap_or(u32::MAX),
                &l.symbol,
                l.how,
                v,
                l.ours,
                l.image,
                note,
            )
        })
        .collect();
    let len = u32::try_from(ext).unwrap_or(u32::MAX);
    if or.base_relocs.is_empty() {
        for (k, r, n, live) in literals(env, or, at, c, ext) {
            rows.push(row(
                k,
                "",
                "-",
                if live {
                    Verdict::Literal
                } else {
                    Verdict::LiteralWeak
                },
                None,
                Some(r),
                format!("an unrelocated operand on {n}"),
            ));
        }
    } else {
        let ours: BTreeMap<u32, &Linked> = links
            .iter()
            .filter(|l| l.kind == REL_DIR32 && l.how != "abs")
            .filter_map(|l| Some((u32::try_from(l.at).ok()?, l)))
            .collect();
        let theirs: BTreeSet<u32> = or
            .base_relocs
            .range(at..at.saturating_add(len))
            .map(|r| r.wrapping_sub(at))
            .collect();
        let base = u32::try_from(env.base).unwrap_or(0);
        for &k in theirs.iter().filter(|k| !ours.contains_key(k)) {
            let v = env
                .img
                .u32_at_rva(Rva(at.wrapping_add(k)))
                .map(|v| v.wrapping_sub(base));
            rows.push(row(
                k,
                "",
                "-",
                Verdict::NotRelocated,
                None,
                v,
                "the image relocates this dword; ours is an integer literal".to_string(),
            ));
        }
        for (&k, l) in ours.iter().filter(|(k, _)| !theirs.contains(k)) {
            rows.push(row(
                k,
                &l.symbol,
                l.how,
                Verdict::ExtraReloc,
                l.ours,
                l.image,
                "we relocate a dword the image hard-codes".to_string(),
            ));
        }
    }
    rows.sort_by_key(|r| r.off);
    rows
}

#[must_use]
pub fn is_finding_line(l: &str) -> bool {
    Verdict::ALL.iter().filter(|v| v.finding()).any(|v| {
        l.strip_prefix(v.as_str())
            .is_some_and(|r| r.starts_with(' '))
    })
}

#[derive(Debug, Default)]
pub struct Report {
    pub seated: usize,
    pub checked: usize,
    pub rows: Vec<Row>,
    pub seats: Vec<(u32, bool)>,
    pub refused: Vec<(PathBuf, String)>,
}

impl Report {
    #[must_use]
    pub fn findings(&self) -> usize {
        self.rows.iter().filter(|r| r.verdict.finding()).count()
    }
}

#[derive(Debug)]
pub struct Inputs<'a> {
    pub env: &'a Env,
    pub objs: &'a [PathBuf],
    pub seats: &'a Seats,
    pub archives: &'a [PathBuf],
    pub notes_only: &'a [PathBuf],
    pub only: Option<u32>,
    pub jobs: usize,
}

type Checked<'a> = (u32, &'a Comdat, String, Option<(Vec<Linked>, usize)>);

#[must_use]
pub fn measure(inp: &Inputs<'_>) -> Report {
    let env = inp.env;
    let ctx = Ctx::build(env, inp.objs, inp.seats);
    let picked: Vec<(&u32, &(PathBuf, Comdat))> = inp
        .seats
        .iter()
        .filter(|(at, _)| inp.only.is_none_or(|o| o == **at))
        .collect();
    let per: Vec<Checked<'_>> = par::map(&picked, inp.jobs, |&(&at, (o, c))| {
        (at, c, scope_stem(o), linked(env, &ctx, at, o, c))
    });
    let library_names: BTreeSet<String> = per
        .iter()
        .filter_map(|p| p.3.as_ref().map(|x| &x.0))
        .flatten()
        .filter(|l| l.how == "svote")
        .map(|l| l.symbol.clone())
        .collect();
    let lib = libs::load(inp.archives, &library_names);
    let or = Oracles::load(env, inp.objs, lib.members, inp.notes_only);
    let checked = per.iter().filter(|p| p.3.is_some()).count();
    let rows: Vec<Row> = par::map(&per, inp.jobs, |(at, c, stem, l)| {
        l.as_ref()
            .map(|(l, ext)| rows_of(env, &or, *at, c, stem, l, *ext))
            .unwrap_or_default()
    })
    .into_iter()
    .flatten()
    .collect();
    let mut seats: Vec<(u32, bool)> = per.iter().map(|p| (p.0, p.3.is_some())).collect();
    seats.sort_unstable();
    Report {
        seated: per.len(),
        checked,
        rows,
        seats,
        refused: lib.refused,
    }
}

pub const TSV_HEAD: &str =
    "image\trva\tfunction\toff\tsymbol\thow\tverdict\tours\timage_target\tnote\n";

#[must_use]
pub fn tsv_line(label: &str, r: &Row) -> String {
    format!(
        "{label}\t{:#08x}\t{}\t{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\n",
        r.rva,
        r.function,
        r.off,
        r.symbol,
        r.how,
        r.verdict.as_str(),
        hex(r.ours),
        hex(r.image),
        r.note
    )
}

#[must_use]
pub fn finding_line(label: &str, r: &Row) -> String {
    format!(
        "{} {label} {:#x} {} +{:#x} {} ({}): ours {} image {} -- {}",
        r.verdict.as_str(),
        r.rva,
        r.function,
        r.off,
        r.symbol,
        r.how,
        hex(r.ours),
        hex(r.image),
        r.note
    )
}

#[must_use]
pub fn tally(rep: &Report) -> BTreeMap<Verdict, usize> {
    let mut by: BTreeMap<Verdict, usize> = BTreeMap::new();
    for r in &rep.rows {
        *by.entry(r.verdict).or_default() += 1;
    }
    by
}

#[must_use]
pub fn summary(label: &str, rep: &Report, refused: usize) -> Vec<String> {
    let by = tally(rep);
    let line = |f: fn(Verdict) -> bool| {
        Verdict::ALL
            .iter()
            .filter(|v| f(**v))
            .map(|v| format!("{} {}", v.as_str(), by.get(v).copied().unwrap_or(0)))
            .collect::<Vec<_>>()
            .join(", ")
    };
    let n = rep.findings();
    let funcs = rep
        .rows
        .iter()
        .filter(|r| r.verdict.finding())
        .map(|r| r.rva)
        .collect::<BTreeSet<_>>()
        .len();
    let mut unres: BTreeMap<&str, usize> = BTreeMap::new();
    for r in rep.rows.iter().filter(|r| r.verdict == Verdict::Unresolved) {
        *unres.entry(r.symbol.as_str()).or_default() += 1;
    }
    let mut out = vec![
        format!(
            "relocid {label}: {} seated, {} byte-identical under try's mask (checked), {} not (not checked)",
            rep.seated,
            rep.checked,
            rep.seated.saturating_sub(rep.checked)
        ),
        format!("  findings: {}", line(Verdict::finding)),
        format!("  leads:    {}", line(Verdict::lead)),
        format!(
            "  proofs:   {}",
            line(|v| !v.finding() && !v.lead() && !v.unproven())
        ),
        format!(
            "  no evidence (fitted to the image, agreeing by construction): {}",
            line(Verdict::unproven)
        ),
    ];
    if !unres.is_empty() {
        let mut s = String::from("  unresolved:");
        for (k, v) in &unres {
            let _ = write!(s, " {k} x{v}");
        }
        out.push(s);
    }
    out.push(if refused > 0 {
        format!(
            "RELOCID {label}: REFUSED -- {refused} object(s) or archive(s) not read, {n} finding(s) in the rest (exit 1)"
        )
    } else if n == 0 {
        format!(
            "RELOCID {label}: CLEAN -- no finding in {} byte-identical function(s) (exit 0)",
            rep.checked
        )
    } else {
        format!("RELOCID {label}: {n} FINDING(S) in {funcs} function(s) (exit 3)")
    });
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn c_names_compare_across_decoration() {
        assert_eq!(c_name("_strlen"), Some("strlen"));
        assert_eq!(c_name("__ftol"), Some("ftol"));
        assert_eq!(c_name("__imp__CreateFileA@28"), Some("CreateFileA"));
        assert_eq!(c_name("free"), Some("free"));
        assert_eq!(c_name("?Foo@@YAXXZ"), None);
        assert_eq!(c_name("Widget::Draw"), None);
        assert_eq!(c_name("_a@b"), Some("a@b"));
    }

    #[test]
    fn a_vector_deleting_destructor_defaults_to_the_scalar_one() {
        assert_eq!(
            weak_default("??_EIconGrid@@UAEPAXI@Z").as_deref(),
            Some("??_GIconGrid@@UAEPAXI@Z")
        );
        assert_eq!(weak_default("??_GIconGrid@@UAEPAXI@Z"), None);
    }

    #[test]
    fn findings_gate_and_leads_do_not() {
        for v in Verdict::ALL {
            let classes = [v.finding(), v.lead(), v.unproven()];
            assert!(
                classes.iter().filter(|c| **c).count() <= 1,
                "{v:?} in two classes"
            );
        }
        assert!(Verdict::Mismatch.finding());
        assert!(Verdict::NotRelocated.finding());
        assert!(Verdict::LibraryConflict.finding());
        assert!(Verdict::Literal.finding());
        assert!(!Verdict::LiteralWeak.finding() && Verdict::LiteralWeak.lead());
        assert!(!Verdict::Layout.finding() && Verdict::Layout.lead());
        assert!(!Verdict::StaleName.finding());
        assert!(!Verdict::Fitted.finding());
        assert!(!Verdict::Named.lead());
    }

    #[test]
    fn a_run_with_a_refused_input_is_never_clean_or_findings() {
        let mut rep = Report {
            seated: 2,
            checked: 2,
            ..Report::default()
        };
        let last = |rep: &Report, refused: usize| summary("IMG", rep, refused).pop().unwrap();
        assert_eq!(
            last(&rep, 0),
            "RELOCID IMG: CLEAN -- no finding in 2 byte-identical function(s) (exit 0)"
        );
        assert_eq!(
            last(&rep, 1),
            "RELOCID IMG: REFUSED -- 1 object(s) or archive(s) not read, 0 finding(s) in the rest (exit 1)"
        );
        rep.rows.push(Row {
            rva: 0x10,
            function: "f".to_string(),
            off: 1,
            symbol: "_g".to_string(),
            how: "seat",
            verdict: Verdict::Mismatch,
            ours: Some(1),
            image: Some(2),
            note: String::new(),
        });
        assert_eq!(
            last(&rep, 0),
            "RELOCID IMG: 1 FINDING(S) in 1 function(s) (exit 3)"
        );
        assert!(
            last(&rep, 2).contains("REFUSED -- 2 object(s) or archive(s) not read, 1 finding(s)")
        );
    }

    #[test]
    fn a_negative_offset_prints_with_its_sign() {
        assert_eq!(signed_hex(-0x8c), "-0x8c");
        assert_eq!(signed_hex(0x11fc), "+0x11fc");
        assert_eq!(signed_hex(0), "+0x0");
    }

    #[test]
    fn finding_lines_are_the_gating_rows() {
        assert!(is_finding_line(
            "MISMATCH IMG 0x1d4a0 ?Close@Reader@@QAEXXZ +0x11 __imp__mmioStringToFOURCCA@8 (iat)"
        ));
        assert!(is_finding_line(
            "NOT-RELOCATED IMG 0x1d4a0 ?Close@Reader@@QAEXXZ +0x5e  (-)"
        ));
        assert!(!is_finding_line("LAYOUT IMG 0x1 f +0x2 s (vote)"));
        assert!(!is_finding_line("MISMATCHED"));
        assert!(!is_finding_line("RELOCID IMG: CLEAN"));
    }

    #[test]
    fn a_names_source_is_bookkeeping_when_it_ends_with_a_named_path() {
        let notes = [PathBuf::from("tables/A.symbols.tsv")];
        assert!(bookkeeping("/x/examples/p/tables/A.symbols.tsv", &notes));
        assert!(!bookkeeping("/x/examples/p/tables/A.addresses.tsv", &notes));
        assert!(!bookkeeping("/x/examples/p/tables/A.symbols.tsv", &[]));
    }
}
