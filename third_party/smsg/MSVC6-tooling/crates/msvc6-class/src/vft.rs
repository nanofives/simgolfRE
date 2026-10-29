use crate::insns::Function;
use anyhow::{Result, anyhow};
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const CODE: &str = ".text";
pub const DATA: &str = ".rdata";
pub const MIN_COMPARED: usize = 4;
pub const OURS_LISTED: usize = 12;
pub const PURECALL: &str = "purecall";
pub const CRT_PURECALL: &str = "__purecall";

#[derive(Debug, Clone, Copy)]
pub struct Text<'a> {
    pub bytes: &'a [u8],
    pub rva: u32,
}

impl<'a> Text<'a> {
    pub fn of(img: &'a Image, code: &str) -> Result<Self> {
        let s = img
            .section(code)
            .ok_or_else(|| anyhow!("no `{code}' section"))?;
        Ok(Self {
            bytes: img.section_bytes(s),
            rva: s.rva.get(),
        })
    }

    #[must_use]
    pub fn end(&self) -> u32 {
        self.rva
            .saturating_add(u32::try_from(self.bytes.len()).unwrap_or(u32::MAX))
    }

    #[must_use]
    pub fn follow(&self, rva: u32) -> u32 {
        follow(self.bytes, self.rva, rva)
    }

    #[must_use]
    pub fn at(&self, rva: u32, len: usize) -> Option<&'a [u8]> {
        let off = usize::try_from(rva.checked_sub(self.rva)?).ok()?;
        self.bytes.get(off..off.checked_add(len)?)
    }
}

#[must_use]
pub fn follow(text: &[u8], text_rva: u32, rva: u32) -> u32 {
    let Some(off) = rva
        .checked_sub(text_rva)
        .and_then(|o| usize::try_from(o).ok())
    else {
        return rva;
    };
    if text.get(off) != Some(&0xe9) {
        return rva;
    }
    let Some(arr) = text
        .get(off.saturating_add(1)..off.saturating_add(5))
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
    else {
        return rva;
    };
    rva.wrapping_add(5).wrapping_add(u32::from_le_bytes(arr))
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Run {
    pub rva: u32,
    pub slots: Vec<u32>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Sections<'a> {
    pub code: &'a str,
    pub data: &'a str,
}

impl Default for Sections<'_> {
    fn default() -> Self {
        Self {
            code: CODE,
            data: DATA,
        }
    }
}

pub fn detect(img: &Image, s: Sections<'_>) -> Result<Vec<Run>> {
    let base = img.opt.image_base.get();
    let text = Text::of(img, s.code)?;
    let rsec = img
        .section(s.data)
        .ok_or_else(|| anyhow!("no `{}' section", s.data))?;
    let rdata = img.section_bytes(rsec);
    let rrva = rsec.rva.get();
    let (trva, tend) = (text.rva, text.end());
    let points = |i: usize| -> Option<u32> {
        let b = rdata.get(i..i.checked_add(4)?)?;
        let v = u32::from_le_bytes(<[u8; 4]>::try_from(b).ok()?);
        let rva = v.checked_sub(base)?;
        (rva >= trva && rva < tend).then_some(rva)
    };
    let mut out: Vec<Run> = Vec::new();
    let mut i = 0usize;
    while i.saturating_add(4) <= rdata.len() {
        if points(i).is_none() {
            i += 4;
            continue;
        }
        let start = i;
        let mut slots = Vec::new();
        while let Some(rva) = points(i) {
            slots.push(rva);
            i += 4;
        }
        out.push(Run {
            rva: rrva.saturating_add(u32::try_from(start).unwrap_or(u32::MAX)),
            slots,
        });
    }
    Ok(out)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Store {
    pub table: u32,
    pub func: u32,
    pub site: u32,
    pub disp: Option<i32>,
}

fn store_disp(b: &[u8]) -> Option<Option<i32>> {
    match b.first() {
        Some(0xc7) if b.len() >= 6 => {
            let m = *b.get(1)?;
            if (m >> 3) & 7 != 0 || m >> 6 == 3 {
                return None;
            }
            let at = 2 + usize::from(m & 7 == 4);
            Some(match m >> 6 {
                1 => b.get(at).map(|d| i32::from(d.cast_signed())),
                2 => b
                    .get(at..at + 4)
                    .and_then(|d| <[u8; 4]>::try_from(d).ok())
                    .map(i32::from_le_bytes),
                _ => Some(0),
            })
        }
        Some(0xb8..=0xbf) if b.len() == 5 => Some(None),
        _ => None,
    }
}

#[must_use]
pub fn stores(funcs: &[Function], base: u32, runs: &[Run]) -> Vec<Store> {
    let slots: BTreeSet<u32> = runs
        .iter()
        .flat_map(|r| {
            (0..r.slots.len())
                .map(move |i| r.rva.wrapping_add(u32::try_from(i * 4).unwrap_or(u32::MAX)))
        })
        .collect();
    let mut out = Vec::new();
    for f in funcs {
        for ins in &f.insns {
            let b = &ins.bytes;
            let Some(disp) = store_disp(b) else { continue };
            let Some(tail) = b.get(b.len().saturating_sub(4)..) else {
                continue;
            };
            let Ok(arr) = <[u8; 4]>::try_from(tail) else {
                continue;
            };
            let Some(table) = u32::from_le_bytes(arr).checked_sub(base) else {
                continue;
            };
            if slots.contains(&table) {
                out.push(Store {
                    table,
                    func: f.entry,
                    site: ins.rva,
                    disp,
                });
            }
        }
    }
    out.sort_by_key(|s| (s.table, s.site));
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Table {
    pub rva: u32,
    pub slots: Vec<u32>,
    pub attested: bool,
}

#[must_use]
pub fn split(runs: &[Run], stores: &[Store]) -> Vec<Table> {
    let starts: BTreeSet<u32> = stores.iter().map(|s| s.table).collect();
    let mut out = Vec::new();
    for r in runs {
        let mut cur = r.rva;
        let mut acc: Vec<u32> = Vec::new();
        for (i, slot) in r.slots.iter().enumerate() {
            let at = r.rva.wrapping_add(u32::try_from(i * 4).unwrap_or(u32::MAX));
            if i > 0 && starts.contains(&at) {
                out.push(Table {
                    rva: cur,
                    slots: std::mem::take(&mut acc),
                    attested: starts.contains(&cur),
                });
                cur = at;
            }
            acc.push(*slot);
        }
        out.push(Table {
            rva: cur,
            slots: acc,
            attested: starts.contains(&cur),
        });
    }
    out
}

#[must_use]
pub fn signed_hex(d: i32) -> String {
    if d < 0 {
        format!("-{:#x}", d.unsigned_abs())
    } else {
        format!("+{d:#x}")
    }
}

#[must_use]
pub fn mask_addresses(bytes: &[u8], base: u32, size: u32) -> Vec<u8> {
    let mut b = bytes.to_vec();
    let n = b.len();
    let rel = matches!(b.first(), Some(0xe8 | 0xe9))
        || (b.first() == Some(&0x0f) && matches!(b.get(1), Some(0x80..=0x8f)));
    if rel && n >= 5 {
        b.iter_mut().skip(n - 4).for_each(|x| *x = 0);
    }
    let mut k = 1;
    while k + 4 <= n {
        let v = b
            .get(k..k + 4)
            .and_then(|w| <[u8; 4]>::try_from(w).ok())
            .map_or(0, u32::from_le_bytes);
        if v >= base && v - base < size {
            b.iter_mut().skip(k).take(4).for_each(|x| *x = 0);
            k += 4;
        } else {
            k += 1;
        }
    }
    b
}

#[must_use]
pub fn masked_body(funcs: &[Function], rva: u32, base: u32, size: u32) -> Option<Vec<Vec<u8>>> {
    let f = funcs.iter().find(|f| f.entry == rva)?;
    let mut ins: Vec<_> = f.insns.iter().collect();
    ins.sort_by_key(|i| i.rva);
    Some(
        ins.iter()
            .map(|i| mask_addresses(&i.bytes, base, size))
            .collect(),
    )
}

#[must_use]
pub fn body_line(bodies: &[u32]) -> String {
    bodies
        .iter()
        .map(|b| format!("{b:#x}"))
        .collect::<Vec<_>>()
        .join(" ")
}

#[must_use]
pub fn summary_row(rva: u32, bodies: &[u32]) -> String {
    let mut uniq: Vec<u32> = bodies.to_vec();
    uniq.sort_unstable();
    uniq.dedup();
    let lo = uniq.first().copied().unwrap_or(0);
    let hi = uniq.last().copied().unwrap_or(0);
    format!(
        "  {rva:#09x}  {:>5}  {} distinct, {lo:#x}..{hi:#x}\n             bodies: {}",
        bodies.len(),
        uniq.len(),
        body_line(bodies)
    )
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct OwedRow {
    pub table: u32,
    pub slots: usize,
    pub distinct: usize,
    pub defined: usize,
    pub owed: Vec<u32>,
}

#[must_use]
pub fn owed_rows(
    tables: &[(u32, Vec<u32>)],
    defined: &BTreeSet<u32>,
    skip: &BTreeSet<u32>,
) -> Vec<OwedRow> {
    let mut rows: Vec<OwedRow> = tables
        .iter()
        .map(|(t, bodies)| {
            let uniq: BTreeSet<u32> = bodies
                .iter()
                .copied()
                .filter(|b| !skip.contains(b))
                .collect();
            let owed: Vec<u32> = uniq
                .iter()
                .copied()
                .filter(|b| !defined.contains(b))
                .collect();
            OwedRow {
                table: *t,
                slots: bodies.len(),
                distinct: uniq.len(),
                defined: uniq.len() - owed.len(),
                owed,
            }
        })
        .collect();
    rows.sort_by(|a, b| b.owed.len().cmp(&a.owed.len()).then(a.table.cmp(&b.table)));
    rows
}

#[must_use]
pub fn owed_line(r: &OwedRow) -> String {
    format!(
        "  {:#09x}  {:>5}  {:>8}  {:>7}  {:>4}{}  {}",
        r.table,
        r.slots,
        r.distinct,
        r.defined,
        r.owed.len(),
        if r.distinct > 0 && r.defined == 0 {
            " ALL"
        } else {
            "    "
        },
        body_line(&r.owed)
    )
}
#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct OwedCounts {
    pub objects: usize,
    pub names: usize,
    pub placed: usize,
    pub purecalls: usize,
    pub member_read: bool,
    pub archive: usize,
    pub missing: Vec<String>,
}

#[must_use]
pub fn owed_report(rows: &[OwedRow], c: &OwedCounts, dir: &str) -> Vec<String> {
    let all: BTreeSet<u32> = rows.iter().flat_map(|r| r.owed.iter().copied()).collect();
    let mut out = vec![
        String::new(),
        format!(
            "  OWED BODIES PER TABLE -- tables split at STORE-ATTESTED starts; a body is OWED when no object of {dir} defines a name the image's names place at it."
        ),
        format!(
            "  {} object(s), {} defined name(s), {} placed at an rva (scoped to the object's stem, then flat, exports first). {} purecall bod(y/ies) excluded (named `{PURECALL}', or byte-equal to a library's `{CRT_PURECALL}' under its relocations; {}).",
            c.objects,
            c.names,
            c.placed,
            c.purecalls,
            if c.member_read {
                "a library member read"
            } else {
                "no library `__purecall' found, so only the name route ran"
            }
        ),
        format!(
            "  {} slot bod(y/ies) inside a library member's span, or in a gap between two spans of one member, count as DEFINED. A name not placed is no body defined; ALL = a class no object models.",
            c.archive
        ),
    ];
    if !c.missing.is_empty() {
        out.push(format!(
            "  {} project TU(s) have no object there, so their bodies read owed: {}",
            c.missing.len(),
            c.missing.join(" ")
        ));
    }
    out.push(String::new());
    out.push(format!(
        "  {:>9}  {:>5}  {:>8}  {:>7}  {:>4}      owed bodies",
        "table", "slots", "distinct", "defined", "owed"
    ));
    for r in rows {
        out.push(owed_line(r));
    }
    out.push(String::new());
    out.push(format!(
        "  {} DISTINCT OWED body/bodies image-wide across {} table(s).",
        all.len(),
        rows.len()
    ));
    out
}

#[must_use]
pub fn bodies_matching(
    text: Text<'_>,
    bodies: impl Iterator<Item = u32>,
    bytes: &[u8],
    relocated: &[bool],
) -> BTreeSet<u32> {
    let compared = relocated.iter().filter(|m| !**m).count();
    if compared < MIN_COMPARED || relocated.len() != bytes.len() {
        return BTreeSet::new();
    }
    bodies
        .filter(|b| {
            text.at(*b, bytes.len()).is_some_and(|w| {
                w.iter()
                    .zip(bytes)
                    .zip(relocated)
                    .all(|((a, b), masked)| *masked || a == b)
            })
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Span {
    pub rva: u32,
    pub len: u32,
    pub member: String,
    pub archive: String,
}

#[must_use]
pub fn archive_bodies(spans: &[Span], bodies: impl Iterator<Item = u32>) -> BTreeSet<u32> {
    let spans: Vec<&Span> = spans.iter().filter(|m| m.len > 0).collect();
    let end = |m: &Span| m.rva.saturating_add(m.len);
    bodies
        .filter(|b| {
            if spans.iter().any(|m| (m.rva..end(m)).contains(b)) {
                return true;
            }
            let below = spans.iter().filter(|m| end(m) <= *b).max_by_key(|m| end(m));
            let above = spans.iter().filter(|m| m.rva > *b).min_by_key(|m| m.rva);
            matches!((below, above), (Some(l), Some(r))
                if l.member == r.member && l.archive == r.archive)
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Control {
    pub rva: u32,
    pub slots: usize,
    pub what: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Found {
    Agrees,
    Length(usize),
    Missing,
}

#[must_use]
pub fn check_controls(tables: &[Run], controls: &[Control]) -> Vec<(Control, Found)> {
    let found: BTreeMap<u32, usize> = tables.iter().map(|r| (r.rva, r.slots.len())).collect();
    controls
        .iter()
        .map(|c| {
            let f = match found.get(&c.rva) {
                Some(got) if *got == c.slots => Found::Agrees,
                Some(got) => Found::Length(*got),
                None => Found::Missing,
            };
            (c.clone(), f)
        })
        .collect()
}

#[must_use]
pub fn control_line(c: &Control, f: &Found) -> String {
    let (rva, want, what) = (c.rva, c.slots, &c.what);
    match f {
        Found::Agrees => format!("    {rva:#09x}  {want:>3} slots  FOUND, length agrees   {what}"),
        Found::Length(got) => format!(
            "    {rva:#09x}  {want:>3} slots  FOUND but {got} slots -- the run merged with a neighbour or stopped short   {what}"
        ),
        Found::Missing => format!(
            "    {rva:#09x}  {want:>3} slots  NOT FOUND -- detector misses a known table   {what}"
        ),
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Labels<'a> {
    pub image: &'a str,
    pub sections: Sections<'a>,
}

#[must_use]
pub fn header(l: Labels<'_>, runs: &[Run], min_slots: usize) -> Vec<String> {
    let kept = runs.iter().filter(|r| r.slots.len() >= min_slots).count();
    vec![
        format!("{}: CANDIDATE VFTABLES IN `{}'", l.image, l.sections.data),
        String::new(),
        format!(
            "  POPULATION: {} maximal run(s) of 4-byte-aligned `{}' dwords landing in `{}'; {kept} of them have >= {min_slots} slot(s).",
            runs.len(),
            l.sections.data,
            l.sections.code
        ),
    ]
}

#[must_use]
pub fn summary(
    text: Text<'_>,
    runs: &[Run],
    min_slots: usize,
    controls: &[Control],
) -> Vec<String> {
    let mut out = vec![
        String::new(),
        format!(
            "  {:>9}  {:>5}  bodies (maximal runs; `--stores' splits them into tables)",
            "table", "slots"
        ),
    ];
    let kept: Vec<Run> = runs
        .iter()
        .filter(|r| r.slots.len() >= min_slots)
        .cloned()
        .collect();
    for r in &kept {
        let bodies: Vec<u32> = r.slots.iter().map(|s| text.follow(*s)).collect();
        out.push(summary_row(r.rva, &bodies));
    }
    out.push(String::new());
    if controls.is_empty() {
        out.push(
            "  NO CONTROL: the image's [image.classes] records no resolved table, so the rows above are unvalidated."
                .to_string(),
        );
        return out;
    }
    out.push(
        "  CONTROL -- the tables [image.classes] records as resolved; a table not found at its length makes every row above suspect:"
            .to_string(),
    );
    for (c, f) in check_controls(&kept, controls) {
        out.push(control_line(&c, &f));
    }
    out
}

#[must_use]
pub fn stores_report(
    text: Text<'_>,
    tables: &[Table],
    stores: &[Store],
    min_slots: usize,
    bodies: bool,
) -> Vec<String> {
    let mut out = vec![
        String::new(),
        "  RUNS SPLIT AT STORE-ATTESTED STARTS: a start is a slot address a decoded `c7 /0 imm32' or `b8+r imm32' writes;"
            .to_string(),
        "  a row whose next row is not attested may hold more than one table, so its length is a ceiling.".to_string(),
        String::new(),
        format!(
            "  {:>9}  {:>5}  attested  written by (function entry @ +disp)",
            "table", "slots"
        ),
    ];
    for t in tables
        .iter()
        .filter(|t| t.slots.len() >= min_slots || t.attested)
    {
        let mut by: Vec<String> = stores
            .iter()
            .filter(|x| x.table == t.rva)
            .map(|x| {
                format!(
                    "{:#x}{}",
                    x.func,
                    x.disp
                        .map_or("@reg".to_string(), |d| format!("@{}", signed_hex(d)))
                )
            })
            .collect();
        by.dedup();
        out.push(format!(
            "  {:#09x}  {:>5}  {:>8}  {}",
            t.rva,
            t.slots.len(),
            if t.attested { "yes" } else { "-" },
            by.join(" ")
        ));
        if bodies {
            let b: Vec<String> = t
                .slots
                .iter()
                .map(|x| format!("{:#x}", text.follow(*x)))
                .collect();
            out.push(format!("             bodies: {}", b.join(" ")));
        }
    }
    out
}

pub fn table_report(
    text: Text<'_>,
    runs: &[Run],
    tables: &[Table],
    stores: &[Store],
    want: u32,
    name_of: &dyn Fn(u32) -> Option<String>,
) -> Result<Vec<String>> {
    let Some(t) = tables.iter().find(|t| t.rva == want) else {
        return Err(anyhow!(
            "no candidate table at {want:#x}: neither a run's first dword nor an address any decoded \
             `mov [m], imm32' / `mov r32, imm32' writes. Rerun with `--stores' and read the split first."
        ));
    };
    let first_len = t.slots.len();
    let starts: BTreeMap<u32, bool> = tables.iter().map(|x| (x.rva, x.attested)).collect();
    let slots = runs
        .iter()
        .find(|r| r.rva == t.rva)
        .map_or(&t.slots, |r| &r.slots);
    let how = if t.attested {
        "a STORE-ATTESTED start, ending at the next attested start or the run's end"
    } else {
        "a run's first dword that NO decoded instruction writes -- a jump table, an install in undecoded \
         code, or a class with no emitted ctor/dtor; its length is a CEILING"
    };
    let mut out = vec![
        String::new(),
        format!("  TABLE {:#x}, {first_len} slot(s) -- {how}.", t.rva),
        "  slot -> thunk -> body -> name; `(direct)' marks a slot that already is a body."
            .to_string(),
    ];
    if slots.len() > first_len {
        out.push(format!(
            "  THE MAXIMAL RUN IS {} slot(s): printed whole, each later table start marked `----' where it falls.",
            slots.len()
        ));
    }
    for s in stores.iter().filter(|s| s.table == t.rva) {
        out.push(format!(
            "    written at {:#08x} in function {:#08x}  {}",
            s.site,
            s.func,
            s.disp.map_or(
                "via a register (`mov r32, imm32')".to_string(),
                |d| format!("to [base{}]", signed_hex(d))
            )
        ));
    }
    for (i, s) in slots.iter().enumerate() {
        let at = t.rva.wrapping_add(u32::try_from(i * 4).unwrap_or(u32::MAX));
        if i > 0
            && let Some(a) = starts.get(&at)
        {
            out.push(format!(
                "    ---- {at:#x}  {} table start",
                if *a { "STORE-ATTESTED" } else { "unattested" }
            ));
            for s in stores.iter().filter(|s| s.table == at) {
                out.push(format!(
                    "         written at {:#08x} in function {:#08x}",
                    s.site, s.func
                ));
            }
        }
        let b = text.follow(*s);
        out.push(format!(
            "    [{i:>3}]  +{:<#5x}  {s:#08x}  ->  {b:#08x}{}  {}",
            i * 4,
            if b == *s { " (direct)" } else { "         " },
            name_of(b).unwrap_or_else(|| "?".to_string())
        ));
    }
    let mut uniq: Vec<u32> = slots.iter().map(|s| text.follow(*s)).collect();
    uniq.sort_unstable();
    uniq.dedup();
    out.push(String::new());
    out.push(format!("  {} DISTINCT body/bodies:", uniq.len()));
    for chunk in uniq.chunks(8) {
        out.push(format!(
            "    {}",
            chunk
                .iter()
                .map(|b| format!("{b:#08x}"))
                .collect::<Vec<_>>()
                .join(" ")
        ));
    }
    Ok(out)
}

#[derive(Debug, Clone, Copy)]
pub struct Side<'a> {
    pub label: &'a str,
    pub funcs: &'a [Function],
    pub base: u32,
    pub size: u32,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct TwinRow {
    pub slot: usize,
    pub ours: Option<u32>,
    pub theirs: Option<u32>,
    pub same: Option<bool>,
}

#[derive(Debug, Clone, Copy)]
pub struct Twin<'a> {
    pub ours: Side<'a>,
    pub theirs: Side<'a>,
    pub table: u32,
    pub attested: bool,
}

#[must_use]
pub fn twin_report(
    t: &Twin<'_>,
    ours: &[u32],
    theirs: &[u32],
    names: &dyn Fn(u32) -> Vec<String>,
) -> (Vec<String>, Vec<TwinRow>) {
    let mut out = vec![
        String::new(),
        format!(
            "  TWIN: {} table {:#x}, {} slot(s){} -- {} slot(s) here. Bodies compared with every address masked (rel32",
            t.theirs.label,
            t.table,
            theirs.len(),
            if t.attested {
                ""
            } else {
                " (UNATTESTED start: a ceiling)"
            },
            ours.len()
        ),
        format!(
            "  and any dword inside the image); the name column is {}'s own names.",
            t.theirs.label
        ),
    ];
    let mut rows = Vec::new();
    let (mut same, mut n) = (0usize, 0usize);
    for i in 0..ours.len().max(theirs.len()) {
        let (o, th) = (ours.get(i).copied(), theirs.get(i).copied());
        let mut row = TwinRow {
            slot: i,
            ours: o,
            theirs: th,
            same: None,
        };
        let verdict = match (o, th) {
            (Some(o), Some(th)) => {
                n += 1;
                match (
                    masked_body(t.ours.funcs, o, t.ours.base, t.ours.size),
                    masked_body(t.theirs.funcs, th, t.theirs.base, t.theirs.size),
                ) {
                    (Some(a), Some(b)) if a == b => {
                        same += 1;
                        row.same = Some(true);
                        format!("SAME ({} ins)", a.len())
                    }
                    (Some(a), Some(b)) => {
                        row.same = Some(false);
                        let k = a.iter().zip(&b).take_while(|(x, y)| x == y).count();
                        format!("DIFFERS at ins {k} of {}/{}", a.len(), b.len())
                    }
                    _ => "no decoded function at one side".to_string(),
                }
            }
            (Some(_), None) => "only HERE".to_string(),
            _ => "only THERE".to_string(),
        };
        let names = th.map(names).unwrap_or_default();
        out.push(format!(
            "    [{i:>3}]  {}  {}  {verdict:<26}  {}",
            o.map_or("        ".to_string(), |x| format!("{x:#08x}")),
            th.map_or("        ".to_string(), |x| format!("{x:#08x}")),
            if names.is_empty() {
                "?".to_string()
            } else {
                names.join(" | ")
            }
        ));
        rows.push(row);
    }
    out.push(String::new());
    out.push(format!(
        "  {same} of {n} paired slot(s) SAME with addresses masked."
    ));
    (out, rows)
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Census {
    pub lengths: Vec<(usize, usize, usize)>,
    pub short: usize,
    pub all: (usize, usize),
    pub kept: (usize, usize),
    pub longest: usize,
    pub ours: Vec<(u32, usize, usize)>,
}

#[must_use]
pub fn census(reference: &[Run], ours: &[Run], min_slots: usize) -> Census {
    let count = |t: &[Run]| -> BTreeMap<usize, usize> {
        let mut m = BTreeMap::new();
        for r in t.iter().filter(|r| r.slots.len() >= min_slots) {
            *m.entry(r.slots.len()).or_insert(0) += 1;
        }
        m
    };
    let (r, o) = (count(reference), count(ours));
    let mut lens: Vec<usize> = r.keys().chain(o.keys()).copied().collect();
    lens.sort_unstable();
    lens.dedup();
    let mut c = Census::default();
    for l in lens {
        let (a, b) = (
            r.get(&l).copied().unwrap_or(0),
            o.get(&l).copied().unwrap_or(0),
        );
        if a != b {
            c.lengths.push((l, a, b));
            c.short += a.saturating_sub(b);
        }
    }
    let all = |t: &[Run]| -> usize { t.iter().map(|r| r.slots.len()).sum() };
    let kept = |t: &[Run]| -> usize {
        t.iter()
            .filter(|r| r.slots.len() >= min_slots)
            .map(|r| r.slots.len())
            .sum()
    };
    c.all = (all(reference), all(ours));
    c.kept = (kept(reference), kept(ours));
    c.longest = reference.iter().map(|r| r.slots.len()).max().unwrap_or(0);
    let mut big: Vec<&Run> = ours.iter().filter(|r| r.slots.len() >= min_slots).collect();
    big.sort_by_key(|r| (std::cmp::Reverse(r.slots.len()), r.rva));
    c.ours = big
        .iter()
        .take(OURS_LISTED)
        .map(|r| {
            let d: BTreeSet<u32> = r.slots.iter().copied().collect();
            (r.rva, r.slots.len(), d.len())
        })
        .collect();
    c
}

#[must_use]
pub fn census_report(c: &Census, candidate: &str, min_slots: usize) -> Vec<String> {
    let mut out = vec![
        String::new(),
        format!(
            "  CANDIDATE {candidate}: its tables against the reference's BY LENGTH MULTISET (our `.rdata' is at our own addresses). Runs of >= {min_slots} slot(s)."
        ),
        "    slots   reference   ours   note".to_string(),
    ];
    for (l, a, b) in &c.lengths {
        out.push(format!(
            "    {l:>5}   {a:>9}   {b:>4}   {}",
            if a > b {
                format!("{} of the reference's are MISSING from ours", a - b)
            } else {
                format!("{} extra in ours -- expected for blob/carve runs", b - a)
            }
        ));
    }
    if c.short == 0 {
        out.push(
            "    every reference length is present in ours at the same multiplicity".to_string(),
        );
    }
    let (rt, ot) = c.all;
    let (rk, ok) = c.kept;
    out.push(String::new());
    out.push(format!(
        "    ALL runs: reference {rt}, ours {ot}.   Runs of >= {min_slots} slot(s): reference {rk}, ours {ok}."
    ));
    out.push(format!(
        "    TOTAL SLOTS OVER ALL RUNS, WHICH IS MERGE-INVARIANT: reference {rt}, ours {ot} ({}).",
        match ot.cmp(&rt) {
            std::cmp::Ordering::Equal =>
                "equal -- no slot is missing, only boundaries differ".to_string(),
            std::cmp::Ordering::Greater => format!("+{} in ours", ot - rt),
            std::cmp::Ordering::Less => format!("-{} in ours: SLOTS ARE MISSING", rt - ot),
        }
    ));
    out.push(format!(
        "  {} reference length(s) have no counterpart of ours. NO VERDICT IS DRAWN.",
        c.short
    ));
    out.push(String::new());
    out.push(
        "  OUR OWN RUNS, largest first (our addresses; only the lengths compare):".to_string(),
    );
    for (rva, n, d) in &c.ours {
        out.push(format!(
            "    {rva:#011x}  {n:>4} slot(s)  {d} distinct body/bodies"
        ));
    }
    out.push(format!(
        "    the reference's longest run is {} slot(s).",
        c.longest
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::insns::Insn;

    #[test]
    fn a_thunk_slot_resolves_to_its_body_and_anything_else_is_kept() {
        let text = [0xe9u8, 0xfb, 0x0f, 0x00, 0x00];
        assert_eq!(follow(&text, 0x1000, 0x1000), 0x2000);
        assert_eq!(follow(&[0x55, 0x8b, 0xec], 0x1000, 0x1000), 0x1000);
        assert_eq!(follow(&text, 0x1000, 0x500), 0x500);
        assert_eq!(follow(&text, 0x1000, 0x9999), 0x9999);
        assert_eq!(follow(&[0xe9, 0x01], 0x1000, 0x1000), 0x1000);
    }

    #[test]
    fn a_store_inside_a_run_splits_it_and_every_slot_is_kept() {
        let runs = vec![Run {
            rva: 0x100,
            slots: vec![1, 2, 3, 4, 5],
        }];
        let st = |t| Store {
            table: t,
            func: 0,
            site: 0,
            disp: Some(0),
        };
        let got = split(&runs, &[st(0x108), st(0x110)]);
        let rows: Vec<(u32, Vec<u32>, bool)> = got
            .into_iter()
            .map(|t| (t.rva, t.slots, t.attested))
            .collect();
        assert_eq!(
            rows,
            vec![
                (0x100, vec![1, 2], false),
                (0x108, vec![3, 4], true),
                (0x110, vec![5], true)
            ]
        );
        assert!(split(&runs, &[st(0x100)])[0].attested);
    }

    #[test]
    fn a_store_is_read_off_a_decoded_instruction_only() {
        let runs = vec![Run {
            rva: 0x2000,
            slots: vec![0x1000, 0x1010],
        }];
        let f = Function {
            entry: 0x1000,
            name: None,
            insns: vec![
                Insn {
                    rva: 0x1000,
                    bytes: vec![0xc7, 0x01, 0x00, 0x20, 0x40, 0x00],
                },
                Insn {
                    rva: 0x1006,
                    bytes: vec![0xc7, 0x46, 0x10, 0x04, 0x20, 0x40, 0x00],
                },
                Insn {
                    rva: 0x100d,
                    bytes: vec![0xb8, 0x04, 0x20, 0x40, 0x00],
                },
                Insn {
                    rva: 0x1012,
                    bytes: vec![0xc7, 0xc0, 0x00, 0x20, 0x40, 0x00],
                },
                Insn {
                    rva: 0x1018,
                    bytes: vec![0x68, 0x00, 0x20, 0x40, 0x00],
                },
            ],
        };
        let s = stores(&[f], 0x40_0000, &runs);
        assert_eq!(
            s.iter()
                .map(|s| (s.table, s.site, s.disp))
                .collect::<Vec<_>>(),
            vec![
                (0x2000, 0x1000, Some(0)),
                (0x2004, 0x1006, Some(0x10)),
                (0x2004, 0x100d, None)
            ]
        );
    }

    #[test]
    fn masking_hides_addresses_and_keeps_field_offsets() {
        let (base, size) = (0x0040_0000, 0x000c_5000);
        assert_eq!(
            mask_addresses(&[0xc7, 0x01, 0x14, 0xb0, 0x4b, 0x00], base, size),
            vec![0xc7, 0x01, 0, 0, 0, 0]
        );
        assert_eq!(
            mask_addresses(&[0x8b, 0x41, 0x14], base, size),
            vec![0x8b, 0x41, 0x14]
        );
        assert_eq!(
            mask_addresses(&[0xe8, 0x10, 0x00, 0x00, 0x00], base, size),
            vec![0xe8, 0, 0, 0, 0]
        );
        assert_eq!(
            mask_addresses(&[0x68, 0x64, 0x04, 0x00, 0x00], base, size),
            vec![0x68, 0x64, 0x04, 0x00, 0x00]
        );
    }

    #[test]
    fn a_summary_row_lists_every_slot_body_in_slot_order() {
        assert_eq!(
            summary_row(0xba26c, &[0x28b0, 0x1c30, 0x28b0]),
            "  0x00ba26c      3  2 distinct, 0x1c30..0x28b0\n             bodies: 0x28b0 0x1c30 0x28b0"
        );
    }

    #[test]
    fn owed_rows_rank_by_undefined_bodies_and_skip_the_excluded() {
        let tables = vec![
            (0x100, vec![0x10, 0x20, 0x99]),
            (0x200, vec![0x30, 0x40, 0x40, 0x99]),
        ];
        let rows = owed_rows(
            &tables,
            &[0x10].into_iter().collect(),
            &[0x99].into_iter().collect(),
        );
        let lines: Vec<String> = rows.iter().map(owed_line).collect();
        assert_eq!(
            lines,
            vec![
                "  0x0000200      4         2        0     2 ALL  0x30 0x40",
                "  0x0000100      3         2        1     1      0x20"
            ]
        );
    }

    #[test]
    fn a_body_equal_to_a_member_under_its_mask_is_recognised() {
        let member = [0x6a, 0x19, 0xe8, 0, 0, 0, 0, 0x59, 0xc3];
        let mask = [false, false, false, true, true, true, true, false, false];
        let mut bytes = vec![0xccu8; 0x40];
        bytes.splice(
            5..14,
            [0x6a, 0x19, 0xe8, 0xaf, 0x0c, 0x00, 0x00, 0x59, 0xc3],
        );
        bytes.splice(
            0x20..0x29,
            [0x6a, 0x18, 0xe8, 0xaf, 0x0c, 0x00, 0x00, 0x59, 0xc3],
        );
        bytes.truncate(0x40);
        let text = Text {
            bytes: &bytes,
            rva: 0x1000,
        };
        let got = bodies_matching(
            text,
            [0x1005, 0x1020, 0x103c, 0x500].into_iter(),
            &member,
            &mask,
        );
        assert_eq!(got.into_iter().collect::<Vec<_>>(), vec![0x1005]);
        let vacuous = [false, false, false, true, true, true, true, true, true];
        assert!(bodies_matching(text, [0x1005].into_iter(), &member, &vacuous).is_empty());
    }

    #[test]
    fn only_a_span_or_a_gap_inside_one_member_claims_a_body() {
        let row = |rva, len, member: &str| Span {
            rva,
            len,
            member: member.into(),
            archive: "crt.lib".into(),
        };
        let spans = vec![
            row(0x41979, 0x36, "ostream.obj"),
            row(0x419da, 0x20, "ostream.obj"),
            row(0x42000, 0x10, "ios.obj"),
            row(0x50000, 0, "zero.obj"),
        ];
        let got = archive_bodies(
            &spans,
            [0x41979, 0x419af, 0x419fa, 0x42010, 0x50000].into_iter(),
        );
        assert_eq!(got, [0x41979, 0x419af].into_iter().collect());
    }

    #[test]
    fn only_the_unfiltered_total_is_merge_invariant() {
        let split = vec![
            Run {
                rva: 0x100,
                slots: vec![1],
            },
            Run {
                rva: 0x104,
                slots: vec![2],
            },
        ];
        let merged = vec![Run {
            rva: 0x100,
            slots: vec![1, 2],
        }];
        let c = census(&split, &merged, 2);
        assert_eq!(c.all, (2, 2));
        assert_eq!(c.kept, (0, 2));
        assert_eq!(c.lengths, vec![(2, 0, 1)]);
        assert_eq!(c.short, 0);
    }

    #[test]
    fn a_control_reads_found_short_or_missing() {
        let runs = vec![Run {
            rva: 0x100,
            slots: vec![1, 2, 3],
        }];
        let c = |rva, slots| Control {
            rva,
            slots,
            what: "x".into(),
        };
        let got: Vec<Found> = check_controls(&runs, &[c(0x100, 3), c(0x100, 2), c(0x200, 1)])
            .into_iter()
            .map(|(_, f)| f)
            .collect();
        assert_eq!(got, vec![Found::Agrees, Found::Length(3), Found::Missing]);
    }
}
