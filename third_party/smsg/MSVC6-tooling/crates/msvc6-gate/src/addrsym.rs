use crate::relocid::abs_fields;
use anyhow::{Result, anyhow, bail};
use iced_x86::{
    ConstantOffsets, Decoder, DecoderOptions, Formatter as _, Instruction, MasmFormatter,
};
use msvc6_base::units::Rva;
use msvc6_diff::env::Env;
use msvc6_diff::seat::{Seats, scope_stem};
use msvc6_diff::segments::{self, MAX_SEGMENT, Pairing};
use msvc6_image::pe::Image;
use msvc6_project::mapfile::Map;
use std::collections::BTreeMap;
use std::fmt::Write as _;

pub const SWAP_WINDOW: usize = 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MapRow {
    pub rva: u32,
    pub name: String,
    pub stem: String,
    pub is_static: bool,
}

#[must_use]
pub fn map_rows(map: &Map) -> Vec<MapRow> {
    let mut out: Vec<MapRow> = map
        .symbols
        .iter()
        .map(|s| MapRow {
            rva: s.rva,
            name: s.name.clone(),
            stem: s
                .object
                .strip_suffix(".obj")
                .or_else(|| s.object.strip_suffix(".OBJ"))
                .unwrap_or("")
                .to_string(),
            is_static: s.is_static,
        })
        .collect();
    out.sort_by(|a, b| a.rva.cmp(&b.rva).then_with(|| a.name.cmp(&b.name)));
    out
}

#[must_use]
pub fn import_bare(s: &str) -> &str {
    let s = s.strip_prefix("__imp_").unwrap_or(s);
    let s = s.strip_prefix('_').unwrap_or(s);
    match s.rsplit_once('@') {
        Some((h, t)) if !h.is_empty() && t.chars().all(|c| c.is_ascii_digit()) => h,
        _ => s,
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum How {
    Scope,
    Flat,
    OtherScope(String),
    Import,
}

#[derive(Debug)]
struct Names {
    scopes: BTreeMap<String, BTreeMap<String, u32>>,
    by_rva: BTreeMap<u32, String>,
    iat_by_name: BTreeMap<String, u32>,
}

impl Names {
    fn load(env: &Env) -> Self {
        let mut scopes: BTreeMap<String, BTreeMap<String, u32>> = BTreeMap::new();
        for t in env.names.tiers() {
            for (name, rva, scope) in t.rows {
                let Some(s) = scope else { continue };
                if t.flat.get(name) == Some(rva) {
                    continue;
                }
                scopes
                    .entry(s.clone())
                    .or_default()
                    .entry(name.clone())
                    .or_insert(*rva);
            }
        }
        let mut by_rva = env.names.by_rva().clone();
        for s in scopes.values() {
            for (k, v) in s {
                by_rva.entry(*v).or_insert_with(|| k.clone());
            }
        }
        for (r, n) in &env.iat {
            by_rva.entry(*r).or_insert_with(|| format!("__imp_{n}"));
        }
        let iat_by_name = env
            .iat
            .iter()
            .map(|(r, n)| (import_bare(n).to_string(), *r))
            .collect();
        Self {
            scopes,
            by_rva,
            iat_by_name,
        }
    }

    fn expect(&self, env: &Env, name: &str, stem: &str) -> Option<(u32, How)> {
        if let Some(r) = self.scopes.get(stem).and_then(|s| s.get(name)) {
            return Some((*r, How::Scope));
        }
        if let Some(r) = env.names.rva(name, env.text(), env.text_rva) {
            return Some((r, How::Flat));
        }
        let other: Vec<(&String, u32)> = self
            .scopes
            .iter()
            .filter_map(|(s, t)| t.get(name).map(|r| (s, *r)))
            .collect();
        if let [(s, r)] = other.as_slice() {
            return Some((*r, How::OtherScope((*s).clone())));
        }
        if name.starts_with("__imp_") {
            return self
                .iat_by_name
                .get(import_bare(name))
                .map(|r| (*r, How::Import));
        }
        None
    }

    fn image_name(&self, img: &Image, rva: u32) -> String {
        let sec = img.section_of(Rva(rva));
        self.by_rva
            .range(..=rva)
            .next_back()
            .filter(|(r, _)| sec.is_some_and(|s| s.contains(Rva(**r))))
            .map_or_else(
                || "?".to_string(),
                |(r, n)| {
                    if *r == rva {
                        n.clone()
                    } else {
                        format!("{n}+{:#x}", rva.wrapping_sub(*r))
                    }
                },
            )
    }
}

fn decode_at(img: &Image, rva: u32, len: usize) -> Option<(Instruction, ConstantOffsets)> {
    let base = u64::from(img.opt.image_base.get());
    let b = img.at_rva(Rva(rva), len)?;
    let mut d = Decoder::with_ip(
        32,
        b,
        base.saturating_add(u64::from(rva)),
        DecoderOptions::NONE,
    );
    let i = d.decode();
    let co = d.get_constant_offsets(&i);
    (!i.is_invalid() && i.len() == len).then_some((i, co))
}

fn text_of(i: &Instruction) -> String {
    let mut f = MasmFormatter::new();
    let mut s = String::new();
    f.format(i, &mut s);
    s
}

#[derive(Debug, Clone, serde::Serialize)]
pub struct Row {
    pub func: u32,
    pub seg: Option<usize>,
    pub pairing: &'static str,
    pub img_va: u64,
    pub ours_va: u64,
    pub verdict: String,
    pub ours: String,
    pub reloc: String,
    pub expect: String,
    pub image: String,
    pub img_text: String,
    pub ours_text: String,
}

#[must_use]
pub fn pairing_tag(p: Pairing) -> &'static str {
    match p {
        Pairing::Same => "same",
        Pairing::Form => "form",
        Pairing::Replaced => "replaced",
        Pairing::ImageOnly => "image-only",
        Pairing::OursOnly => "ours-only",
    }
}

fn bytes_equal(
    ours: &Image,
    ours_rva: u32,
    reference: &Image,
    img_rva: u32,
    len: usize,
) -> Option<bool> {
    let a = ours.at_rva(Rva(ours_rva), len)?;
    let b = reference.at_rva(Rva(img_rva), len)?;
    Some(a == b)
}

fn in_text(img: &Image, rva: u32) -> bool {
    img.section_of(Rva(rva))
        .is_some_and(|s| s.name.starts_with(".text"))
}

#[derive(Debug, Clone, Default)]
pub struct Scored {
    pub rows: Vec<Row>,
    pub line: String,
    pub tally: BTreeMap<String, usize>,
    pub implied: Vec<(String, u32)>,
}

#[derive(Debug)]
pub struct Linked<'a> {
    pub image: &'a Image,
    pub rows: Vec<MapRow>,
    by_name: BTreeMap<String, Vec<usize>>,
}

impl<'a> Linked<'a> {
    #[must_use]
    pub fn new(image: &'a Image, map: &Map) -> Self {
        let rows = map_rows(map);
        let mut by_name: BTreeMap<String, Vec<usize>> = BTreeMap::new();
        for (k, r) in rows.iter().enumerate() {
            by_name.entry(r.name.clone()).or_default().push(k);
        }
        Self {
            image,
            rows,
            by_name,
        }
    }

    #[must_use]
    pub fn lookup(&self, name: &str, stem: &str) -> Option<&MapRow> {
        let v = self.by_name.get(name)?;
        let rows: Vec<&MapRow> = v.iter().filter_map(|k| self.rows.get(*k)).collect();
        rows.iter()
            .find(|r| r.stem.eq_ignore_ascii_case(stem))
            .or(match rows.as_slice() {
                [one] if !one.is_static => Some(one),
                _ => None,
            })
            .copied()
    }

    fn defined_more_than_once(&self, name: &str) -> bool {
        self.by_name.get(name).is_some_and(|v| v.len() > 1)
    }

    fn next_after(&self, rva: u32) -> Option<u32> {
        let i = self.rows.partition_point(|q| q.rva <= rva);
        self.rows
            .get(i..)?
            .iter()
            .find(|q| q.rva > rva)
            .map(|q| q.rva)
    }
}

fn content_len(ours: &Image, name: &str, m: &MapRow, next: Option<u32>) -> usize {
    let mut len = next.map_or(64, |n| n.saturating_sub(m.rva)).min(64) as usize;
    if name.starts_with("__real@4@") {
        len = len.min(4);
    } else if name.starts_with("__real@") {
        len = len.min(8);
    }
    if name.starts_with("??_C@")
        && let Some(b) = ours.at_rva(Rva(m.rva), len)
    {
        len = b.iter().position(|c| *c == 0).map_or(len, |p| p + 1);
    }
    len
}

pub fn score_one(
    env: &Env,
    seats: &Seats,
    linked: &Linked<'_>,
    all: bool,
    rva: u32,
) -> Result<Scored> {
    let names = Names::load(env);
    score_with(env, seats, linked, &names, all, rva)
}

#[allow(clippy::too_many_lines)]
fn score_with(
    env: &Env,
    seats: &Seats,
    linked: &Linked<'_>,
    names: &Names,
    all: bool,
    rva: u32,
) -> Result<Scored> {
    let ours = linked.image;
    let (path, comdat) = seats
        .get(&rva)
        .ok_or_else(|| anyhow!("{rva:#x}: no object seats it"))?;
    let a = segments::analyze_in(env, rva, path, comdat.clone(), MAX_SEGMENT)?;
    let obase = ours.opt.image_base.get();
    let otop = u64::from(obase).saturating_add(u64::from(ours.opt.size_of_image));
    let ibase = u32::try_from(env.base).map_err(|_| anyhow!("an image base past 4 GiB"))?;
    let sym = a
        .comdat
        .symbol
        .clone()
        .ok_or_else(|| anyhow!("{rva:#x}: our COMDAT has no symbol"))?;
    let stem = scope_stem(path);
    let orow = linked
        .lookup(&sym, &stem)
        .ok_or_else(|| anyhow!("{sym}: not in the map"))?;
    let orva = orow.rva;
    let placed = ours
        .at_rva(Rva(orva), a.comdat.bytes.len())
        .ok_or_else(|| anyhow!("{sym}: {orva:#x} is not in our image"))?;
    let stale = placed
        .iter()
        .zip(&a.comdat.bytes)
        .zip(&a.comdat.relocated)
        .filter(|((x, y), m)| !**m && x != y)
        .count();
    if stale > 0 {
        bail!(
            "{sym}: {stale} unrelocated byte(s) differ from the object at {orva:#x}: the image is not this build's"
        );
    }
    let irva = a.rva;
    let olen = a.comdat.bytes.len();
    let ilen = env
        .seats
        .get(env.seats.partition_point(|s| s.0 <= irva))
        .map_or(olen, |n| n.0.saturating_sub(irva) as usize)
        .max(a.img.bytes.len());
    let seg_of = |i: usize| a.segs.iter().position(|s| i >= s.ia.0 && i < s.ia.1);
    let mut out = Scored::default();
    let img_fields: Vec<Vec<u32>> = a
        .ours
        .wide
        .iter()
        .map(|r| {
            r.img
                .and_then(|ii| a.img.dec.get(ii))
                .and_then(|id| decode_at(&env.img, irva.wrapping_add(id.ip), id.insn.len()))
                .map(|(i, co)| {
                    abs_fields(&i, &co, env.base, env.top)
                        .into_iter()
                        .map(|(_, v)| v)
                        .collect()
                })
                .unwrap_or_default()
        })
        .collect();
    let crosswise = |ri: usize, e: u32| {
        let hi = (ri.saturating_add(SWAP_WINDOW)).min(img_fields.len().saturating_sub(1));
        (ri.saturating_sub(SWAP_WINDOW)..=hi)
            .any(|k| k != ri && img_fields.get(k).is_some_and(|f| f.contains(&e)))
    };
    for (ri, r) in a.ours.wide.iter().enumerate() {
        if !matches!(r.pairing, Pairing::Same | Pairing::Form) {
            continue;
        }
        let (Some(ii), Some(oi)) = (r.img, r.ours) else {
            continue;
        };
        let (Some(id), Some(od)) = (a.img.dec.get(ii), a.ours.dec.get(oi)) else {
            continue;
        };
        let (Some((iins, ico)), Some((oins, oco))) = (
            decode_at(&env.img, irva.wrapping_add(id.ip), id.insn.len()),
            decode_at(ours, orva.wrapping_add(od.ip), od.insn.len()),
        ) else {
            continue;
        };
        let ofields: Vec<(usize, u32, &str)> = abs_fields(&oins, &oco, u64::from(obase), otop)
            .into_iter()
            .filter_map(|(off, y)| {
                let at = od.ip.checked_add(u32::try_from(off).ok()?)?;
                let s = a.comdat.relocations.iter().find(|x| x.at == at)?;
                Some((off, y, s.symbol.as_str()))
            })
            .collect();
        if ofields.is_empty() {
            continue;
        }
        let ifields = abs_fields(&iins, &ico, env.base, env.top);
        let mk =
            |verdict: String, ours_s: String, reloc: &str, expect: String, image: String| Row {
                func: irva,
                seg: seg_of(ii),
                pairing: pairing_tag(r.pairing),
                img_va: env.base.saturating_add(u64::from(irva.wrapping_add(id.ip))),
                ours_va: u64::from(obase).saturating_add(u64::from(orva.wrapping_add(od.ip))),
                verdict,
                ours: ours_s,
                reloc: reloc.to_string(),
                expect,
                image,
                img_text: text_of(&iins),
                ours_text: text_of(&oins),
            };
        for (off, y, rs) in ofields {
            let Some(&(_, x)) = ifields.iter().find(|(io, _)| *io == off) else {
                *out.tally.entry("COUNT".into()).or_default() += 1;
                out.rows.push(mk(
                    "COUNT".into(),
                    format!("{y:#x}"),
                    rs,
                    String::new(),
                    "-".into(),
                ));
                continue;
            };
            let img_self = x >= irva && usize::try_from(x - irva).is_ok_and(|d| d < ilen);
            let named_other = linked.lookup(rs, &stem).is_some_and(|m| m.rva != orva);
            let our_self =
                !named_other && y >= orva && usize::try_from(y - orva).is_ok_and(|d| d < olen);
            if !named_other && (img_self || our_self) {
                let (io, oo) = (x.wrapping_sub(irva), y.wrapping_sub(orva));
                let v = match (img_self && our_self, io == oo) {
                    (true, true) | (false, true) => "SELF",
                    (true, false) => "SELF-SHIFT",
                    (false, false) => "MISMATCH",
                };
                *out.tally.entry(v.into()).or_default() += 1;
                if all || v == "MISMATCH" {
                    out.rows.push(mk(
                        v.into(),
                        format!("self+{oo:#x}"),
                        rs,
                        String::new(),
                        format!("self+{io:#x}"),
                    ));
                }
                continue;
            }
            let image = names.image_name(&env.img, x);
            let (verdict, ours_s, expect) = match linked.lookup(rs, &stem) {
                Some(m) => {
                    let d = y.wrapping_sub(m.rva);
                    let key = if linked.defined_more_than_once(&m.name) {
                        format!("{}@{}", m.name, m.stem)
                    } else {
                        m.name.clone()
                    };
                    out.implied.push((key, x.wrapping_sub(d)));
                    let ours_s = if d == 0 {
                        m.name.clone()
                    } else {
                        format!("{}+{:#x}", m.name, d.cast_signed())
                    };
                    match names.expect(env, &m.name, &m.stem) {
                        Some((e, how)) => {
                            let e = e.wrapping_add(d);
                            let v = match (e == x, &how) {
                                (true, How::OtherScope(s)) => format!("OTHER-SCOPE:{s}"),
                                (true, _) => "AGREE".to_string(),
                                (false, _) if crosswise(ri, e) => "SWAP".to_string(),
                                (false, _) => "MISMATCH".to_string(),
                            };
                            (
                                v,
                                ours_s,
                                format!("{:#x}", u64::from(e).saturating_add(u64::from(ibase))),
                            )
                        }
                        None => {
                            let len = content_len(ours, &m.name, m, linked.next_after(m.rva));
                            let v = if in_text(ours, m.rva) {
                                "CODE"
                            } else if m.name.starts_with("??_7") || m.name.starts_with("??_8") {
                                "VFTABLE"
                            } else {
                                match bytes_equal(ours, m.rva, &env.img, x.wrapping_sub(d), len) {
                                    Some(true) => "CONTENT",
                                    Some(false) => "CONTENT-DIFF",
                                    None => "UNNAMED",
                                }
                            };
                            (v.to_string(), ours_s, String::new())
                        }
                    }
                }
                None if in_text(ours, y) => ("LABEL".to_string(), format!("{y:#x}"), String::new()),
                None => {
                    let v = match bytes_equal(ours, y, &env.img, x, 16) {
                        Some(true) => "CONTENT",
                        Some(false) => "CONTENT-DIFF",
                        None => "UNNAMED",
                    };
                    (v.to_string(), format!("{y:#x}"), String::new())
                }
            };
            *out.tally.entry(verdict.clone()).or_default() += 1;
            if all || verdict != "AGREE" {
                out.rows.push(mk(verdict, ours_s, rs, expect, image));
            }
        }
    }
    let t: Vec<String> = out.tally.iter().map(|(k, v)| format!("{k} {v}")).collect();
    out.line = format!("{irva:#x}\t{sym}\t{}", t.join(", "));
    Ok(out)
}

#[derive(Debug, Default)]
pub struct Report {
    pub functions: usize,
    pub rows: Vec<Row>,
    pub lines: Vec<String>,
    pub total: BTreeMap<String, usize>,
    pub failed: Vec<String>,
    pub splits: Vec<String>,
}

impl Report {
    #[must_use]
    pub fn mismatches(&self) -> usize {
        self.total.get("MISMATCH").copied().unwrap_or(0)
    }
}

pub const FINDING_VERDICTS: [&str; 3] = ["MISMATCH", "CONTENT-DIFF", "COUNT"];

#[must_use]
pub fn findings(rep: &Report) -> usize {
    FINDING_VERDICTS
        .iter()
        .map(|v| rep.total.get(*v).copied().unwrap_or(0))
        .sum()
}

pub fn run(
    env: &Env,
    seats: &Seats,
    linked: &Linked<'_>,
    targets: &[u32],
    named: bool,
    all: bool,
    jobs: usize,
) -> Result<Report> {
    let names = Names::load(env);
    let results: Vec<(u32, Result<Scored>)> = msvc6_diff::par::map(targets, jobs, |&rva| {
        (rva, score_with(env, seats, linked, &names, all, rva))
    });
    let mut rep = Report::default();
    let ibase = u32::try_from(env.base).unwrap_or(0);
    let mut bases: BTreeMap<String, BTreeMap<u32, usize>> = BTreeMap::new();
    for (rva, r) in results {
        match r {
            Ok(s) => {
                rep.functions += 1;
                for (k, b) in s.implied {
                    *bases.entry(k).or_default().entry(b).or_insert(0) += 1;
                }
                rep.rows.extend(s.rows);
                if named {
                    rep.lines.push(s.line);
                }
                for (k, v) in s.tally {
                    *rep.total.entry(k).or_insert(0) += v;
                }
            }
            Err(e) if named => return Err(e.context(format!("{rva:#x}"))),
            Err(e) => rep.failed.push(format!("{rva:#x}: {e:#}")),
        }
    }
    for (s, b) in bases.iter().filter(|(_, b)| b.len() > 1) {
        let parts: Vec<String> = b
            .iter()
            .map(|(r, n)| {
                format!(
                    "{:#x} {} ({n})",
                    u64::from(*r).saturating_add(u64::from(ibase)),
                    names.image_name(&env.img, *r)
                )
            })
            .collect();
        let kind = if s.starts_with("??_C@") || s.starts_with("__real@") {
            "SPLIT-LITERAL"
        } else {
            "SPLIT"
        };
        rep.splits
            .push(format!("{kind}\t{s}\t{}", parts.join("; ")));
    }
    Ok(rep)
}

pub const TSV_HEAD: &str = "fn\tseg\tpairing\timage_va\tours_va\tverdict\tours\treloc\texpect\timage\timage_insn\tours_insn\n";

#[must_use]
pub fn tsv(rows: &[Row]) -> String {
    let mut t = String::from(TSV_HEAD);
    for r in rows {
        let seg = r.seg.map_or_else(|| "-".to_string(), |s| s.to_string());
        let _ = writeln!(
            t,
            "{:#x}\t{seg}\t{}\t{:#x}\t{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
            r.func,
            r.pairing,
            r.img_va,
            r.ours_va,
            r.verdict,
            r.ours,
            r.reloc,
            r.expect,
            r.image,
            r.img_text,
            r.ours_text
        );
    }
    t
}

#[must_use]
pub fn total_line(rep: &Report) -> String {
    let t: Vec<String> = rep.total.iter().map(|(k, v)| format!("{k} {v}")).collect();
    format!(
        "TOTAL over {} function(s): {}{}",
        rep.functions,
        t.join(", "),
        if rep.failed.is_empty() {
            String::new()
        } else {
            format!(
                "; {} not read (first: {})",
                rep.failed.len(),
                rep.failed
                    .iter()
                    .take(3)
                    .cloned()
                    .collect::<Vec<_>>()
                    .join("; ")
            )
        }
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn import_bare_strips_the_thunk_prefix_and_the_stdcall_suffix() {
        assert_eq!(import_bare("__imp__GetTickCount@0"), "GetTickCount");
        assert_eq!(import_bare("__imp__timeGetTime@0"), "timeGetTime");
        assert_eq!(import_bare("GetTickCount"), "GetTickCount");
        assert_eq!(import_bare("__imp_?x@@YAXXZ"), "?x@@YAXXZ");
    }

    #[test]
    fn abs_fields_reads_disp_imm_and_call_target_in_range() {
        let code = [
            0x66, 0xa1, 0x9c, 0x2c, 0x4c, 0x00, 0x68, 0xa8, 0x1e, 0x4c, 0x00, 0xe8, 0xf5, 0xff,
            0xff, 0xff,
        ];
        let mut d = Decoder::with_ip(32, &code, 0x40_0ffb, DecoderOptions::NONE);
        let v: Vec<Vec<u32>> = (0..3)
            .map(|_| {
                let i = d.decode();
                let co = d.get_constant_offsets(&i);
                abs_fields(&i, &co, 0x40_0000, 0x60_0000)
                    .into_iter()
                    .map(|(_, r)| r)
                    .collect()
            })
            .collect();
        assert_eq!(v, vec![vec![0xc_2c9c], vec![0xc_1ea8], vec![0x1000]]);
    }

    #[test]
    fn map_rows_name_the_defining_object_and_the_statics() {
        let text = " app\r\n\r\n Preferred load address is 00400000\r\n\r\n\
                    \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
                    \x200001:0005de00       ?Render@@YAHH@Z            0045ee00 f   Screen.obj\r\n\
                    \x200003:000da434       ?g_mask@@3FA               0059a434     Assets.obj\r\n\
                    \x200001:00000000       _memset                    00401000 f   LIBC:memset.obj\r\n\
                    \r\n Static symbols\r\n\r\n\
                    \x200001:00000100       $L123                      00401100 f   Screen.obj\r\n";
        let m = Map::parse(std::path::Path::new("x.map"), text).unwrap();
        let r = map_rows(&m);
        let got: Vec<(u32, &str, &str, bool)> = r
            .iter()
            .map(|x| (x.rva, x.name.as_str(), x.stem.as_str(), x.is_static))
            .collect();
        assert_eq!(
            got,
            vec![
                (0x1000, "_memset", "memset", false),
                (0x1100, "$L123", "Screen", true),
                (0x5_ee00, "?Render@@YAHH@Z", "Screen", false),
                (0x19_a434, "?g_mask@@3FA", "Assets", false),
            ]
        );
    }
}
