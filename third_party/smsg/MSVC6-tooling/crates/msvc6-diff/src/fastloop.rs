use crate::attrib::{self, Listing, Proc};
use crate::env::Env;
use crate::seat;
use crate::segments::{self, Analysis, Pairing, Seg, off};
use anyhow::Result;
use iced_x86::{Decoder, DecoderOptions, Formatter as _, Instruction, MasmFormatter, OpKind};
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::Path;

pub const FRONTIER_CONTEXT: usize = 3;
pub const FRONTIER_LINES: usize = 60;
pub const SHOWN: usize = 20;
pub const ASM_LINES: usize = 24;
pub const LINE_WIDTH: usize = 170;
pub const MAX_HISTORY: u64 = 16 << 20;
const IMG_TEXT: usize = 34;
const LEFT_WIDTH: usize = 84;

pub trait Oracle: Sync {
    fn analyze(&self, obj: &Path, symbol: &str) -> Result<Analysis>;
}

#[derive(Debug)]
pub struct Segments<'a> {
    pub env: &'a Env,
    pub rva: u32,
    pub max: usize,
}

impl Oracle for Segments<'_> {
    fn analyze(&self, obj: &Path, symbol: &str) -> Result<Analysis> {
        let (o, c) = seat::find(self.env, &[obj.to_path_buf()], self.rva, Some(symbol))?;
        segments::analyze_in(self.env, self.rva, &o, c, self.max)
    }
}

pub type Namer<'a> = dyn Fn(u32) -> Option<String> + Sync + 'a;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct SegEdit {
    pub va: u64,
    pub seg: usize,
    pub edit: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Scored {
    pub edit: usize,
    pub segs: Vec<SegEdit>,
    pub blocks: Vec<String>,
    pub full: String,
    pub frontier: String,
}

fn mark(p: Pairing) -> char {
    match p {
        Pairing::Same => ' ',
        Pairing::Form => '~',
        Pairing::Replaced => '*',
        Pairing::ImageOnly => '<',
        Pairing::OursOnly => '>',
    }
}

fn one(bytes: &[u8], at: usize, ip: u64) -> Option<(Instruction, String)> {
    let b = bytes.get(at..)?;
    let mut d = Decoder::with_ip(32, b, ip, DecoderOptions::NONE);
    let i = d.decode();
    if i.is_invalid() {
        return None;
    }
    let mut s = String::new();
    MasmFormatter::new().format(&i, &mut s);
    Some((i, s))
}

fn hexb(b: &[u8]) -> String {
    b.iter()
        .map(|x| format!("{x:02x}"))
        .collect::<Vec<_>>()
        .join(" ")
}

fn with_comment(t: &str, c: &str, width: usize) -> String {
    if c.is_empty() {
        t.to_string()
    } else {
        format!("{t:<width$} ; {c}")
    }
}

fn image_names(i: &Instruction, base: u64, namer: &Namer<'_>) -> String {
    let mut vals = Vec::new();
    for k in 0..i.op_count() {
        match i.op_kind(k) {
            OpKind::NearBranch32 | OpKind::NearBranch16 => vals.push(i.near_branch_target()),
            OpKind::Memory => vals.push(u64::from(i.memory_displacement32())),
            OpKind::Immediate32 => vals.push(i.immediate(k)),
            _ => {}
        }
    }
    let names: Vec<String> = vals
        .into_iter()
        .filter_map(|v| v.checked_sub(base).and_then(|r| u32::try_from(r).ok()))
        .filter_map(namer)
        .collect();
    names.join(", ")
}

fn ours_names(a: &Analysis, lo: usize, hi: usize) -> String {
    a.comdat
        .relocations
        .iter()
        .filter(|r| (r.at as usize) >= lo && (r.at as usize) < hi)
        .map(|r| r.symbol.clone())
        .collect::<Vec<_>>()
        .join(", ")
}

#[must_use]
pub fn segment_listing(a: &Analysis, n: usize, s: &Seg, namer: &Namer<'_>) -> String {
    let va = |o: usize| a.start.saturating_add(o as u64);
    let base = a.start.saturating_sub(u64::from(a.rva));
    let mut out = format!(
        "== seg {n}  image {:#x}..{:#x}  ours +{:#x}..+{:#x}  {}  diff {}  edit {}  at {}\n",
        va(s.img.0),
        va(s.img.1),
        s.ours.0,
        s.ours.1,
        s.verdict.tag(),
        s.diff,
        s.edit,
        if s.at.is_empty() { "(head)" } else { &s.at }
    );
    for r in segments::align_segment(&a.img, &a.ours, s.ia, s.oa) {
        let left = r
            .img
            .and_then(|i| {
                let o = off(&a.img, i);
                let end = off(&a.img, i + 1);
                let (insn, t) = one(&a.img.bytes, o, va(o))?;
                let raw = hexb(a.img.bytes.get(o..end).unwrap_or_default());
                let c = image_names(&insn, base, namer);
                Some(format!(
                    "{:#010x} {raw:<14} {}",
                    va(o),
                    with_comment(&t, &c, IMG_TEXT)
                ))
            })
            .unwrap_or_default();
        let right = r
            .ours
            .and_then(|j| {
                let o = off(&a.ours, j);
                let end = off(&a.ours, j + 1);
                let (_, t) = one(&a.ours.bytes, o, o as u64)?;
                let raw = hexb(a.ours.bytes.get(o..end).unwrap_or_default());
                let c = ours_names(a, o, end);
                Some(format!("+{o:04x} {raw:<14} {}", with_comment(&t, &c, 0)))
            })
            .unwrap_or_default();
        let line = format!("{} {left:<LEFT_WIDTH$} | {right}", mark(r.pairing));
        out.push_str(line.trim_end());
        out.push('\n');
    }
    out
}

#[must_use]
pub fn differing_rows(listing: &str) -> Vec<String> {
    let lines: Vec<&str> = listing.lines().collect();
    let header = |l: &str| l.starts_with("==") || l.starts_with("   source");
    let mut keep = vec![false; lines.len()];
    for (k, l) in lines.iter().enumerate() {
        if header(l) {
            if let Some(x) = keep.get_mut(k) {
                *x = true;
            }
        } else if !l.starts_with(' ') {
            let hi = (k + 1).min(lines.len().saturating_sub(1));
            for x in keep.iter_mut().take(hi + 1).skip(k.saturating_sub(1)) {
                *x = true;
            }
        }
    }
    let mut out = Vec::new();
    let mut gap = false;
    for (k, l) in lines.iter().enumerate() {
        if keep.get(k) == Some(&true) {
            if gap {
                out.push("   ...".to_string());
            }
            out.push((*l).to_string());
            gap = false;
        } else {
            gap = true;
        }
    }
    out
}

fn source_text(p: Option<&Proc>, lo: usize, hi: usize) -> String {
    let Some(p) = p else {
        return String::new();
    };
    let span = attrib::line_span(&attrib::lines_in(p, lo, hi));
    if span.is_empty() {
        span
    } else {
        format!("   {span}")
    }
}

#[must_use]
pub fn score(a: &Analysis, listing: &Listing, symbol: &str, namer: &Namer<'_>) -> Scored {
    let proc = listing.proc(symbol);
    let mut out = Scored {
        edit: segments::edit_totals(&a.segs).0,
        ..Scored::default()
    };
    for (k, s) in a.segs.iter().enumerate() {
        out.segs.push(SegEdit {
            va: a.start.saturating_add(s.img.0 as u64),
            seg: k,
            edit: s.edit,
        });
        if s.edit == 0 {
            continue;
        }
        let l = segment_listing(a, k, s, namer);
        let src = source_text(proc, s.ours.0, s.ours.1);
        out.full.push_str(&l);
        if !src.is_empty() {
            out.full.push_str(&src);
            out.full.push('\n');
        }
        let mut rows = differing_rows(&l);
        if !src.is_empty() {
            rows.insert(1.min(rows.len()), src);
        }
        out.blocks.push(rows.join("\n"));
    }
    out.frontier = frontier(a, proc, namer);
    out
}

#[must_use]
pub fn frontier(a: &Analysis, proc: Option<&Proc>, namer: &Namer<'_>) -> String {
    let Some((k, s)) = a.segs.iter().enumerate().find(|(_, s)| s.edit > 0) else {
        return String::new();
    };
    let listing = segment_listing(a, k, s, namer);
    let rows = segments::align_segment(&a.img, &a.ours, s.ia, s.oa);
    let first = rows.iter().position(|r| r.cost > 0).unwrap_or(0);
    let lines: Vec<&str> = listing.lines().collect();
    let mut out = String::new();
    if let Some(h) = lines.first() {
        out.push_str(h);
        out.push('\n');
    }
    let from = first.saturating_sub(FRONTIER_CONTEXT);
    if from > 0 {
        out.push_str(&format!("   ... {from} matching row(s)\n"));
    }
    for l in lines.iter().skip(1 + from) {
        out.push_str(l);
        out.push('\n');
    }
    let Some(p) = proc else {
        out.push_str("   (no listing: the source lines are not known)\n");
        return out;
    };
    let first_off = rows
        .iter()
        .skip(first)
        .find_map(|r| r.ours)
        .map(|j| off(&a.ours, j));
    let at = first_off
        .and_then(|o| attrib::site_at(&p.sites, u32::try_from(o).unwrap_or(u32::MAX)))
        .map(|x| x.line);
    let seen = attrib::lines_in(p, s.ours.0, s.ours.1);
    out.push_str(&format!(
        "   source lines compiled into seg {k} ({} line(s)):\n",
        seen.len()
    ));
    for (l, t) in &seen {
        let t: String = t.chars().take(140).collect();
        out.push_str(&format!(
            "   {} {l:>5}: {t}\n",
            if Some(*l) == at { "=>" } else { "  " }
        ));
    }
    out
}

#[must_use]
pub fn moved(base: &[SegEdit], v: &[SegEdit]) -> String {
    let b: BTreeMap<u64, usize> = base.iter().map(|s| (s.va, s.edit)).collect();
    let mut out = Vec::new();
    for s in v {
        match b.get(&s.va) {
            Some(be) if *be == s.edit => {}
            Some(be) => out.push(format!("seg{}@{:x} {be}->{}", s.seg, s.va, s.edit)),
            None if s.edit > 0 => out.push(format!("seg{}@{:x}(recut) {}", s.seg, s.va, s.edit)),
            None => {}
        }
    }
    if out.is_empty() {
        "-".to_string()
    } else {
        out.join(" ")
    }
}

#[must_use]
pub fn differing(segs: &[SegEdit]) -> String {
    let d: Vec<String> = segs
        .iter()
        .filter(|s| s.edit > 0)
        .map(|s| format!("seg{}@{:x} {}", s.seg, s.va, s.edit))
        .collect();
    if d.is_empty() {
        "none".to_string()
    } else {
        d.join(" ")
    }
}

#[must_use]
pub fn rank(edits: &[Option<usize>]) -> Vec<usize> {
    let mut order: Vec<usize> = (1..edits.len()).collect();
    order.sort_by_key(|&k| (edits.get(k).copied().flatten().unwrap_or(usize::MAX), k));
    order
}

#[must_use]
pub fn best(edits: &[Option<usize>], order: &[usize]) -> usize {
    let base = edits.first().copied().flatten();
    order
        .first()
        .copied()
        .filter(|&k| {
            let e = edits.get(k).copied().flatten();
            match (e, base) {
                (Some(e), Some(b)) => e < b,
                (Some(_), None) => true,
                _ => false,
            }
        })
        .unwrap_or(0)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub time: u64,
    pub run: String,
    pub index: usize,
    pub file: String,
    pub md5: String,
    pub edit: usize,
    pub differing: String,
}

fn field(s: &str) -> String {
    s.replace(['\t', '\n', '\r'], " ")
}

impl Row {
    #[must_use]
    pub fn line(&self) -> String {
        format!(
            "{}\t{}\t{}\t{}\t{}\t{}\t{}\n",
            self.time,
            field(&self.run),
            self.index,
            field(&self.file),
            field(&self.md5),
            self.edit,
            field(&self.differing)
        )
    }
}

#[must_use]
pub fn parse_history(text: &str) -> Vec<Row> {
    text.lines()
        .filter_map(|l| {
            let f: Vec<&str> = l.split('\t').collect();
            let [time, run, index, file, md5, edit, differing] = f.as_slice() else {
                return None;
            };
            Some(Row {
                time: time.parse().ok()?,
                run: (*run).to_string(),
                index: index.parse().ok()?,
                file: (*file).to_string(),
                md5: (*md5).to_string(),
                edit: edit.parse().ok()?,
                differing: (*differing).to_string(),
            })
        })
        .collect()
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Previous {
    pub base: Option<(u64, usize)>,
    pub best: Option<(usize, String)>,
    pub runs: usize,
}

#[must_use]
pub fn previous(rows: &[Row]) -> Previous {
    let mut p = Previous::default();
    let mut last_run: Option<&str> = None;
    for r in rows {
        if last_run != Some(r.run.as_str()) {
            p.runs += 1;
            last_run = Some(r.run.as_str());
        }
        if r.index == 0 {
            p.base = Some((r.time, r.edit));
        }
        if p.best.as_ref().is_none_or(|(b, _)| r.edit < *b) {
            p.best = Some((r.edit, r.file.clone()));
        }
    }
    p
}

fn signed(x: usize) -> i64 {
    i64::try_from(x).unwrap_or(i64::MAX)
}

#[must_use]
pub fn delta_of(prev: &Previous, edit: usize) -> Option<i64> {
    prev.base
        .map(|(_, e)| signed(edit).saturating_sub(signed(e)))
}

#[must_use]
pub fn delta_text(prev: &Previous, edit: usize, now: u64) -> String {
    let last = match (prev.base, delta_of(prev, edit)) {
        (Some((t, e)), Some(d)) => format!(
            "last run {e} ({} min ago) -> {d:+}",
            now.saturating_sub(t) / 60
        ),
        _ => "first run".to_string(),
    };
    match &prev.best {
        Some((e, f)) => format!("{last}; best ever {e} {f}"),
        None => last,
    }
}

#[derive(Debug, Clone)]
pub struct Measured {
    pub file: String,
    pub md5: String,
    pub secs: f64,
    pub scored: std::result::Result<Scored, String>,
}

impl Measured {
    #[must_use]
    pub fn edit(&self) -> Option<usize> {
        self.scored.as_ref().ok().map(|s| s.edit)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Shape {
    pub frontier: bool,
    pub asm_lines: usize,
}

fn clip(l: &str) -> String {
    l.chars().take(LINE_WIDTH).collect()
}

fn first_error(e: &str) -> String {
    e.lines()
        .map(str::trim)
        .find(|l| l.contains("error") || l.contains("fatal"))
        .or_else(|| e.lines().map(str::trim).find(|l| !l.is_empty()))
        .map(clip)
        .unwrap_or_default()
}

#[must_use]
pub fn report(
    files: &[Measured],
    prev: &Previous,
    now: u64,
    shape: Shape,
    kept: (&Path, &Path),
) -> (Vec<String>, usize) {
    let mut out = Vec::new();
    let edits: Vec<Option<usize>> = files.iter().map(Measured::edit).collect();
    let Some(base) = files.first() else {
        return (out, 0);
    };
    let base_scored = match &base.scored {
        Ok(s) => s,
        Err(e) => {
            out.push(format!("  BASE {}  FAILED: {}", base.file, first_error(e)));
            return (out, 0);
        }
    };
    out.push(format!(
        "  BASE {}  edit {}  ({})  differing: {}",
        base.file,
        base_scored.edit,
        delta_text(prev, base_scored.edit, now),
        differing(&base_scored.segs)
    ));
    let order = rank(&edits);
    let best = best(&edits, &order);
    if !order.is_empty() {
        out.push("  rank  edit  vs base  file  (segments moved)".to_string());
    }
    for (n, &k) in order.iter().enumerate().take(SHOWN) {
        let Some(f) = files.get(k) else { continue };
        match &f.scored {
            Ok(s) => {
                let d = signed(s.edit).saturating_sub(signed(base_scored.edit));
                out.push(format!(
                    "  {:>4}  {:>4}  {d:>+7}  {}  ({})",
                    n + 1,
                    s.edit,
                    f.file,
                    moved(&base_scored.segs, &s.segs)
                ));
            }
            Err(e) => out.push(format!(
                "     -     -        -  {}  FAILED: {}",
                f.file,
                first_error(e)
            )),
        }
    }
    if order.len() > SHOWN {
        out.push(format!("  ... {} more", order.len() - SHOWN));
    }
    let shown = files.get(best).unwrap_or(base);
    let show = shown.scored.as_ref().unwrap_or(base_scored);
    if show.edit == 0 {
        out.push(format!("  EXACT: edit 0 for {}", shown.file));
    } else if shape.frontier {
        out.push(format!(
            "  FRONTIER of {}: its first differing segment ({}):",
            shown.file,
            kept.1.display()
        ));
        let cap = shape.asm_lines.max(FRONTIER_LINES);
        let lines: Vec<&str> = show.frontier.lines().collect();
        let split = lines
            .iter()
            .position(|l| l.trim_start().starts_with("source lines compiled into"))
            .unwrap_or(lines.len());
        let (asm, src) = lines.split_at(split);
        let room = cap.saturating_sub(src.len().min(cap / 2)).max(8);
        out.extend(asm.iter().take(room).map(|l| format!("  {}", clip(l))));
        if asm.len() > room {
            out.push(format!(
                "     ... {} more row(s) in {}",
                asm.len() - room,
                kept.1.display()
            ));
        }
        out.extend(src.iter().take(cap / 2).map(|l| format!("  {}", clip(l))));
        if src.len() > cap / 2 {
            out.push(format!(
                "     ... {} more line(s) in {}",
                src.len() - cap / 2,
                kept.1.display()
            ));
        }
    } else {
        out.push(format!(
            "  differing segments of {} (image | ours, differing rows; all rows: {}):",
            shown.file,
            kept.0.display()
        ));
        let mut budget = shape.asm_lines;
        let mut capped = false;
        for l in show.blocks.iter().flat_map(|b| b.lines()) {
            if budget == 0 {
                capped = true;
                break;
            }
            out.push(format!("  {}", clip(l)));
            budget -= 1;
        }
        if capped {
            out.push(format!(
                "  (asm capped at {} lines; --asm-lines N, or {})",
                shape.asm_lines,
                kept.0.display()
            ));
        }
    }
    (out, best)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn seg(va: u64, seg: usize, edit: usize) -> SegEdit {
        SegEdit { va, seg, edit }
    }

    fn measured(file: &str, segs: Vec<SegEdit>) -> Measured {
        Measured {
            file: file.to_string(),
            md5: String::new(),
            secs: 0.0,
            scored: Ok(Scored {
                edit: segs.iter().map(|s| s.edit).sum(),
                blocks: if segs.iter().any(|s| s.edit > 0) {
                    vec!["== seg 1  image ...\n* a | b".to_string()]
                } else {
                    Vec::new()
                },
                segs,
                ..Scored::default()
            }),
        }
    }

    #[test]
    fn differing_rows_keep_headers_and_one_row_of_context() {
        let l = "== seg 1  image ...\n  a\n  b\n* c | d\n  e\n  f\n  g\n< h |\n";
        assert_eq!(
            differing_rows(l),
            vec![
                "== seg 1  image ...",
                "   ...",
                "  b",
                "* c | d",
                "  e",
                "   ...",
                "  g",
                "< h |"
            ]
        );
    }

    #[test]
    fn segments_are_keyed_by_their_image_address() {
        let base = vec![seg(0x10, 0, 0), seg(0x20, 1, 24), seg(0x30, 2, 4)];
        let v = vec![seg(0x10, 0, 0), seg(0x20, 1, 15), seg(0x30, 2, 4)];
        assert_eq!(moved(&base, &v), "seg1@20 24->15");
        assert_eq!(moved(&base, &base), "-");
        let recut = vec![seg(0x10, 0, 0), seg(0x28, 1, 3)];
        assert_eq!(moved(&base, &recut), "seg1@28(recut) 3");
        assert_eq!(differing(&base), "seg1@20 24 seg2@30 4");
        assert_eq!(differing(&[seg(0x10, 0, 0)]), "none");
    }

    #[test]
    fn variants_rank_by_edit_and_the_best_must_beat_the_base() {
        let edits = [Some(10), Some(12), None, Some(3), Some(3)];
        let order = rank(&edits);
        assert_eq!(order, vec![3, 4, 1, 2]);
        assert_eq!(best(&edits, &order), 3);
        let worse = [Some(2), Some(5)];
        assert_eq!(best(&worse, &rank(&worse)), 0);
        assert_eq!(best(&[Some(0)], &rank(&[Some(0)])), 0);
        assert!(rank(&[]).is_empty());
    }

    #[test]
    fn the_history_gives_the_last_base_and_the_best_ever() {
        let rows = [
            Row {
                time: 100,
                run: "r1".into(),
                index: 0,
                file: "a.cpp".into(),
                md5: "m".into(),
                edit: 40,
                differing: "seg1@10 40".into(),
            },
            Row {
                time: 100,
                run: "r1".into(),
                index: 1,
                file: "b.cpp".into(),
                md5: "m".into(),
                edit: 12,
                differing: "seg1@10 12".into(),
            },
            Row {
                time: 700,
                run: "r2".into(),
                index: 0,
                file: "a\tc.cpp".into(),
                md5: "m".into(),
                edit: 30,
                differing: "seg1@10 30".into(),
            },
        ];
        let text: String = rows.iter().map(Row::line).collect();
        let back = parse_history(&format!("garbage line\n{text}1\t2\n"));
        assert_eq!(back.len(), 3);
        assert_eq!(back[2].file, "a c.cpp");
        let p = previous(&back);
        assert_eq!(p.base, Some((700, 30)));
        assert_eq!(p.best, Some((12, "b.cpp".to_string())));
        assert_eq!(p.runs, 2);
        assert_eq!(delta_of(&p, 25), Some(-5));
        assert_eq!(
            delta_text(&p, 25, 760),
            "last run 30 (1 min ago) -> -5; best ever 12 b.cpp"
        );
        assert_eq!(delta_text(&Previous::default(), 25, 0), "first run");
    }

    #[test]
    fn the_report_ranks_the_variants_and_shows_the_best_files_differing_rows() {
        let files = vec![
            measured("base.cpp", vec![seg(0x10, 0, 0), seg(0x20, 1, 24)]),
            measured("v1.cpp", vec![seg(0x10, 0, 0), seg(0x20, 1, 15)]),
            Measured {
                file: "v2.cpp".into(),
                md5: String::new(),
                secs: 0.0,
                scored: Err("v2.cpp(12) : error C2065: 'x' : undeclared identifier".into()),
            },
            measured("v3.cpp", vec![seg(0x10, 0, 0), seg(0x20, 1, 30)]),
        ];
        let shape = Shape {
            frontier: false,
            asm_lines: ASM_LINES,
        };
        let (lines, best) = report(
            &files,
            &Previous::default(),
            0,
            shape,
            (Path::new("asm.txt"), Path::new("frontier.txt")),
        );
        assert_eq!(best, 1);
        assert!(lines[0].contains("BASE base.cpp  edit 24  (first run)"));
        assert!(lines[2].contains("15") && lines[2].contains("-9") && lines[2].contains("v1.cpp"));
        assert!(lines[3].contains("v3.cpp") && lines[3].contains("+6"));
        assert!(lines[4].contains("FAILED: v2.cpp(12) : error C2065"));
        assert!(lines[5].contains("differing segments of v1.cpp"));
        assert_eq!(lines[6], "  == seg 1  image ...");
        let exact = vec![measured("base.cpp", vec![seg(0x10, 0, 0)])];
        let (lines, best) = report(
            &exact,
            &Previous::default(),
            0,
            shape,
            (Path::new("a"), Path::new("f")),
        );
        assert_eq!(best, 0);
        assert_eq!(
            lines.last().map(String::as_str),
            Some("  EXACT: edit 0 for base.cpp")
        );
    }
}
