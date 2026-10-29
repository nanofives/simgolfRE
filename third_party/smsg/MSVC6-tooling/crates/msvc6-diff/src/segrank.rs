use crate::commswap;
use crate::decisions::{self, Charge};
use crate::env::Env;
use crate::par;
use crate::rawdiff::{self, Ctx};
use crate::seat::Seats;
use crate::segments::{
    MAX_SEGMENT, Verdict, addend_mismatches, analyze_in, edit_totals, image_want, percent,
    raw_extent,
};
use anyhow::{Result, anyhow};
use std::collections::{BTreeMap, HashSet};
use std::path::PathBuf;

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
pub struct Row {
    pub rva: u32,
    pub name: String,
    pub differing: usize,
    pub segs: usize,
    pub exact: usize,
    pub reg: usize,
    pub shape: usize,
    pub edit: usize,
    pub image: usize,
    pub addend: usize,
    pub raw: usize,
    pub unres: usize,
    pub rawflat: usize,
}

pub const HEADER: &str =
    "rva\tname\tdiffering\tsegs\texact/reg/shape\tedit\timage\tpct\taddend\traw\tunres\trawflat";

impl Row {
    #[must_use]
    pub fn line(&self) -> String {
        format!(
            "{:#08x}\t{}\t{}\t{}\t{}/{}/{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
            self.rva,
            self.name,
            self.differing,
            self.segs,
            self.exact,
            self.reg,
            self.shape,
            self.edit,
            self.image,
            percent(self.edit, self.image),
            self.addend,
            self.raw,
            self.unres,
            self.rawflat
        )
    }
}

fn parse_hex(s: &str) -> Option<u32> {
    let h = s
        .strip_prefix("0x")
        .or_else(|| s.strip_prefix("0X"))
        .unwrap_or(s);
    u32::from_str_radix(h, 16).ok()
}

#[must_use]
pub fn parse_rvas(text: &str) -> Vec<u32> {
    text.lines()
        .filter_map(|l| l.split_whitespace().next())
        .filter_map(parse_hex)
        .collect()
}

pub type Prev = BTreeMap<u32, (usize, Option<usize>)>;

#[must_use]
pub fn parse_prev(text: &str) -> Prev {
    let mut out = BTreeMap::new();
    let mut cols: Option<(usize, Option<usize>)> = None;
    for l in text.lines() {
        let f: Vec<&str> = l.split('\t').collect();
        if f.first() == Some(&"rva") {
            let at = |n: &str| f.iter().position(|x| *x == n);
            cols = at("differing").map(|d| (d, at("edit")));
            continue;
        }
        let Some(rva) = l.split_whitespace().next().and_then(parse_hex) else {
            continue;
        };
        if let Some((d, e)) = cols {
            let num = |i: usize| f.get(i).and_then(|x| x.trim().parse::<usize>().ok());
            if let Some(v) = num(d) {
                out.insert(rva, (v, e.and_then(num)));
            }
            continue;
        }
        let w: Vec<&str> = l.split_whitespace().collect();
        let before = |tag: &str| {
            w.iter()
                .position(|x| *x == tag)
                .and_then(|i| i.checked_sub(1))
                .and_then(|i| w.get(i))
                .and_then(|x| x.parse::<usize>().ok())
        };
        if let Some(v) = before("differing;") {
            out.insert(rva, (v, before("edit")));
        }
    }
    out
}

fn signed(now: usize, before: usize) -> i64 {
    i64::try_from(now).unwrap_or(i64::MAX) - i64::try_from(before).unwrap_or(i64::MAX)
}

#[must_use]
pub fn deltas(rows: &[Row], prev: &Prev) -> Vec<String> {
    let mut out = Vec::new();
    let now: BTreeMap<u32, &Row> = rows.iter().map(|r| (r.rva, r)).collect();
    let (mut p_tot, mut n_tot, mut common) = (0usize, 0usize, 0usize);
    let (mut pe_tot, mut ne_tot, mut e_common) = (0usize, 0usize, 0usize);
    for r in rows {
        match prev.get(&r.rva) {
            Some(&(p, pe)) => {
                common += 1;
                p_tot += p;
                n_tot += r.differing;
                if let Some(pe) = pe {
                    e_common += 1;
                    pe_tot += pe;
                    ne_tot += r.edit;
                }
                let moved_edit = pe.is_some_and(|pe| pe != r.edit);
                if p != r.differing || moved_edit {
                    let edit = match pe {
                        Some(pe) => format!("\tedit {pe} -> {} ({:+})", r.edit, signed(r.edit, pe)),
                        None => String::new(),
                    };
                    out.push(format!(
                        "delta\t{:#08x}\t{}\t{p} -> {}\t{:+}{edit}",
                        r.rva,
                        r.name,
                        r.differing,
                        signed(r.differing, p)
                    ));
                }
            }
            None => out.push(format!("new\t{:#08x}\t{}\t{}", r.rva, r.name, r.differing)),
        }
    }
    for (rva, (p, _)) in prev {
        if !now.contains_key(rva) {
            out.push(format!("gone\t{rva:#08x}\t{p}"));
        }
    }
    out.push(format!(
        "total delta: {p_tot} -> {n_tot} ({:+}) differing over {common} rva(s) in both",
        signed(n_tot, p_tot)
    ));
    if e_common > 0 {
        out.push(format!(
            "edit delta: {pe_tot} -> {ne_tot} ({:+}) over {e_common} rva(s) with a previous edit figure",
            signed(ne_tot, pe_tot)
        ));
    }
    out
}

#[must_use]
pub fn raw_line(rows: &[Row]) -> String {
    let raw: usize = rows.iter().map(|r| r.raw).sum();
    let fns = rows.iter().filter(|r| r.raw > 0).count();
    let hid: Vec<&Row> = rows.iter().filter(|r| r.raw > 0 && r.edit == 0).collect();
    let unres: usize = rows.iter().map(|r| r.unres).sum();
    format!(
        "raw: {raw} raw byte(s) over {fns} function(s); {} byte(s) in {} function(s) at edit 0; {unres} unresolved field(s)",
        hid.iter().map(|r| r.raw).sum::<usize>(),
        hid.len()
    )
}

#[must_use]
pub fn total_line(rows: &[Row], asked: usize) -> String {
    let differing: usize = rows.iter().map(|r| r.differing).sum();
    let edit: usize = rows.iter().map(|r| r.edit).sum();
    let image: usize = rows.iter().map(|r| r.image).sum();
    format!(
        "total: {differing} differing over {} function(s) of {asked} asked; {edit} edit bytes of {image} image bytes ({} divergent)",
        rows.len(),
        percent(edit, image)
    )
}

#[must_use]
pub fn refused_line(refused: &[(PathBuf, String)], given: usize) -> String {
    let names: Vec<String> = refused
        .iter()
        .map(|(o, _)| {
            o.file_name().map_or_else(
                || o.display().to_string(),
                |f| f.to_string_lossy().into_owned(),
            )
        })
        .collect();
    format!(
        "REFUSED: {} of {given} object(s) unread, their COMDATs seat nothing: {}",
        refused.len(),
        names.join(", ")
    )
}

pub fn score(env: &Env, rva: u32, seat: &(PathBuf, crate::comdat::Comdat)) -> Result<Row> {
    let a = analyze_in(env, rva, &seat.0, seat.1.clone(), MAX_SEGMENT)?;
    let count = |v: Verdict| a.segs.iter().filter(|s| s.verdict == v).count();
    let (edit, image) = edit_totals(&a.segs);
    Ok(Row {
        rva,
        name: seat.1.symbol.clone().unwrap_or_else(|| "?".to_string()),
        differing: a
            .segs
            .iter()
            .filter(|s| s.verdict == Verdict::Shape)
            .map(|s| s.diff)
            .sum(),
        segs: a.segs.len(),
        exact: count(Verdict::Exact),
        reg: count(Verdict::RegOnly),
        shape: count(Verdict::Shape),
        edit,
        image,
        addend: addend_mismatches(&a.img, &a.ours, &a.segs).0.len(),
        raw: 0,
        unres: 0,
        rawflat: 0,
    })
}

pub fn fill_raw(env: &Env, ctx: &Ctx, flat: Option<&Ctx>, seats: &Seats, r: &mut Row) {
    if let Some((o, c)) = seats.get(&r.rva) {
        let (want, whence) = image_want(env, r.rva, c.bytes.len());
        let (want, _) = raw_extent(want, whence, c.bytes.len());
        let x = rawdiff::of(env, ctx, r.rva, o, c, want);
        (r.raw, r.unres) = (x.raw, x.unres);
        if let Some(f) = flat {
            r.rawflat = rawdiff::of(env, f, r.rva, o, c, want).raw;
        }
    }
}

#[derive(Debug)]
pub struct Ranked {
    pub rows: Vec<Row>,
    pub errors: Vec<(u32, String)>,
    pub asked: usize,
}

pub fn rank(env: &Env, list: &[PathBuf], seats: &Seats, rvas: &[u32], jobs: usize) -> Ranked {
    let scored: Vec<(u32, Result<Row>)> = par::map(rvas, jobs, |&rva| {
        let r = match seats.get(&rva) {
            Some(seat) => score(env, rva, seat),
            None => Err(anyhow!(
                "no COMDAT among {} object(s) seats here",
                list.len()
            )),
        };
        (rva, r)
    });
    let ctx = Ctx::build(env, list, seats);
    let flat = Ctx::build_flat(env, list, seats);
    let mut rows = Vec::new();
    let mut errors = Vec::new();
    for (rva, r) in scored {
        match r {
            Ok(mut r) => {
                fill_raw(env, &ctx, Some(&flat), seats, &mut r);
                rows.push(r);
            }
            Err(e) => errors.push((rva, format!("{e:#}"))),
        }
    }
    rows.sort_by(|a, b| b.differing.cmp(&a.differing).then(a.rva.cmp(&b.rva)));
    Ranked {
        rows,
        errors,
        asked: rvas.len(),
    }
}

pub type Charged = (Vec<(u32, Charge)>, Vec<(u32, String)>);

#[must_use]
pub fn decisions(env: &Env, seats: &Seats, rows: &[Row], jobs: usize) -> Charged {
    let per: Vec<(u32, Result<Vec<Charge>>)> = par::map(rows, jobs, |r| {
        let Some(seat) = seats.get(&r.rva) else {
            return (r.rva, Ok(Vec::new()));
        };
        let found = analyze_in(env, r.rva, &seat.0, seat.1.clone(), MAX_SEGMENT).map(|a| {
            let offs: HashSet<usize> = commswap::find(&a).iter().map(|s| s.img_off).collect();
            decisions::find(&a, &offs)
        });
        (r.rva, found)
    });
    let (mut all, mut errors) = (Vec::new(), Vec::new());
    for (rva, found) in per {
        match found {
            Ok(cs) => all.extend(cs.into_iter().map(|c| (rva, c))),
            Err(e) => errors.push((rva, format!("{e:#}"))),
        }
    }
    (all, errors)
}

#[must_use]
pub fn tsv(rows: &[Row]) -> String {
    let mut t = format!("{HEADER}\n");
    for r in rows {
        t.push_str(&r.line());
        t.push('\n');
    }
    t
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(rva: u32, differing: usize, edit: usize) -> Row {
        Row {
            rva,
            name: format!("f{rva:x}"),
            differing,
            segs: 3,
            exact: 1,
            reg: 1,
            shape: 1,
            edit,
            image: 100,
            addend: 0,
            raw: 0,
            unres: 0,
            rawflat: 0,
        }
    }

    #[test]
    fn the_raw_line_counts_raw_bytes_hidden_at_edit_zero() {
        let mut a = row(0x289e0, 0, 0);
        a.raw = 2;
        let mut b = row(0x8d480, 0, 0);
        b.raw = 1;
        let mut c = row(0x1000, 9, 9);
        c.raw = 12;
        c.unres = 1;
        assert_eq!(
            raw_line(&[a, b, c, row(0x2000, 0, 0)]),
            "raw: 15 raw byte(s) over 3 function(s); 3 byte(s) in 2 function(s) at edit 0; 1 unresolved field(s)"
        );
    }

    #[test]
    fn rvas_and_previous_figures_read_both_formats() {
        let old = "0x00f5c0 summary: 1073 segment(s): EXACT 518 / REG-ONLY 251 / SHAPE 304; SHAPE bytes \
                   22190 (image), 6699 differing; median segment 61 bytes\n";
        assert_eq!(parse_rvas(old), [0xf5c0]);
        assert_eq!(parse_prev(old)[&0xf5c0], (6699, None));
        let tsv = format!("{HEADER}\n{}\n", row(0x99140, 1887, 50).line());
        assert_eq!(parse_rvas(&tsv), [0x99140]);
        assert_eq!(parse_prev(&tsv)[&0x99140], (1887, Some(50)));
        assert_eq!(
            row(0x99140, 1887, 50).line(),
            "0x099140\tf99140\t1887\t3\t1/1/1\t50\t100\t50.0%\t0\t0\t0\t0"
        );
    }

    #[test]
    fn deltas_list_moved_new_and_gone_rows_and_the_common_total() {
        let prev: Prev = [(1, (10, Some(5))), (2, (20, None)), (3, (30, None))]
            .into_iter()
            .collect();
        let rows = [row(1, 7, 4), row(2, 20, 9), row(4, 5, 5)];
        assert_eq!(
            deltas(&rows, &prev),
            [
                "delta\t0x000001\tf1\t10 -> 7\t-3\tedit 5 -> 4 (-1)",
                "new\t0x000004\tf4\t5",
                "gone\t0x000003\t30",
                "total delta: 30 -> 27 (-3) differing over 2 rva(s) in both",
                "edit delta: 5 -> 4 (-1) over 1 rva(s) with a previous edit figure",
            ]
        );
    }

    #[test]
    fn the_total_line_reads_as_the_ranking_reads() {
        assert_eq!(
            total_line(&[row(1, 0, 0)], 2),
            "total: 0 differing over 1 function(s) of 2 asked; 0 edit bytes of 100 image bytes (0.0% divergent)"
        );
    }
}
