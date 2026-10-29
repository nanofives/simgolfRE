use anyhow::{Result, bail};
use msvc6_diff::env::Env;
use msvc6_diff::seat;
use msvc6_image::pe::{Dir, Image};
use msvc6_project::mapfile::{Map, Owners, is_member};
use std::collections::{BTreeMap, BTreeSet};
use std::path::PathBuf;

pub const UNCLASSIFIED_CAP: usize = 4096;

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize)]
pub struct Split {
    pub differing: usize,
    pub ours: usize,
    pub library: usize,
    pub generated: usize,
    pub unclassified: usize,
    pub symbols: usize,
    pub ilt_count: usize,
}

#[must_use]
pub fn map_symbols(map: &Map, owners: &Owners) -> Vec<(u64, String)> {
    let mut out: Vec<(u64, String)> = map
        .symbols
        .iter()
        .filter_map(|s| {
            let tok = s.owner_token();
            (tok.ends_with(".obj") || tok.ends_with(".lib"))
                .then(|| (u64::from(s.rva), owners.map_owner(&tok).to_string()))
        })
        .collect();
    out.sort_by_key(|s| s.0);
    out.dedup_by_key(|s| s.0);
    out
}

#[must_use]
pub fn in_thunk(rb: &[u8], cb: &[u8], lattice: usize, off: usize) -> bool {
    if off < lattice {
        return false;
    }
    let slot = lattice.saturating_add(((off - lattice) / 5).saturating_mul(5));
    rb.get(slot) == Some(&0xe9) && cb.get(slot) == Some(&0xe9)
}

#[must_use]
pub fn lattice_len(b: &[u8], lattice: usize, limit: usize) -> usize {
    (0..)
        .map(|k: usize| lattice.saturating_add(k.saturating_mul(5)))
        .take_while(|&o| o.saturating_add(5) <= limit && b.get(o) == Some(&0xe9))
        .count()
}

#[must_use]
pub fn in_unpaired_slot(
    rb: &[u8],
    cb: &[u8],
    (ra, ca): (usize, usize),
    (rn, cn): (usize, usize),
    off: usize,
) -> bool {
    let inside = |b: &[u8], a: usize, n: usize| {
        off >= a
            && off < a.saturating_add(n.saturating_mul(5))
            && b.get(a.saturating_add(((off - a) / 5).saturating_mul(5))) == Some(&0xe9)
    };
    (inside(rb, ra, rn) && off >= ca.saturating_add(cn.saturating_mul(5)))
        || (inside(cb, ca, cn) && off >= ra.saturating_add(rn.saturating_mul(5)))
}

#[must_use]
pub fn lattice_start(rb: &[u8], limit: usize) -> Option<usize> {
    (0..limit.min(rb.len())).find(|i| rb.get(*i) == Some(&0xe9))
}

#[derive(Debug, PartialEq, Eq)]
pub enum Residue {
    Padding,
    Shifted,
    Unseen,
}

#[must_use]
pub fn residue_class(r: Option<(&str, usize, bool)>, c: Option<(&str, usize, bool)>) -> Residue {
    match (r, c) {
        (Some((rn, ro, rc)), Some((cn, co, cc))) if rn == cn && ro == co => {
            if rc && cc {
                Residue::Unseen
            } else {
                Residue::Padding
            }
        }
        (None, None) => Residue::Unseen,
        _ => Residue::Shifted,
    }
}

#[must_use]
pub fn codeview_from_dir(b: &[u8]) -> Option<(usize, usize)> {
    let size = u32::from_le_bytes(b.get(0x10..0x14)?.try_into().ok()?);
    let ptr = u32::from_le_bytes(b.get(0x18..0x1c)?.try_into().ok()?);
    let (ptr, size) = (usize::try_from(ptr).ok()?, usize::try_from(size).ok()?);
    if size == 0 {
        return None;
    }
    Some((ptr, ptr.checked_add(size)?))
}

fn codeview_range(img: &Image) -> Option<(usize, usize)> {
    let d = img.present(Dir::Debug)?;
    codeview_from_dir(img.at_rva(d.rva, 28)?)
}

pub type AbsentValue = (u64, [u8; 4]);

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Content {
    pub section: String,
    pub total: usize,
    pub missing: Vec<AbsentValue>,
}

#[derive(Debug, Default)]
pub struct Account {
    pub symbols: usize,
    pub per: BTreeMap<String, Split>,
    pub header_diff: usize,
    pub debug_diff: usize,
    pub shifted: usize,
    pub padding: usize,
    pub outside: usize,
    pub file_total: usize,
    pub displaced: Vec<String>,
    pub unclassified: Vec<(String, u64, u64)>,
    pub content: Vec<Content>,
    pub own_ranges: BTreeMap<String, Vec<(u64, u64)>>,
}

impl Account {
    #[must_use]
    pub fn totals(&self) -> Split {
        let mut t = Split::default();
        for s in self.per.values() {
            t.differing = t.differing.saturating_add(s.differing);
            t.ours = t.ours.saturating_add(s.ours);
            t.library = t.library.saturating_add(s.library);
            t.generated = t.generated.saturating_add(s.generated);
            t.unclassified = t.unclassified.saturating_add(s.unclassified);
        }
        for n in [
            self.header_diff,
            self.debug_diff,
            self.shifted,
            self.padding,
        ] {
            t.differing = t.differing.saturating_add(n);
            t.generated = t.generated.saturating_add(n);
        }
        t
    }

    #[must_use]
    pub fn unseen(&self) -> usize {
        let td = self.totals().differing;
        if self.displaced.is_empty() {
            self.file_total.saturating_sub(td).max(self.outside)
        } else {
            self.outside
        }
    }

    #[must_use]
    pub fn content_defects(&self) -> usize {
        self.content.iter().map(|c| c.missing.len()).sum()
    }

    #[must_use]
    pub fn met(&self) -> bool {
        self.unseen() == 0 && self.content_defects() == 0 && self.totals().unclassified == 0
    }
}

fn differing(a: &[u8], b: &[u8], n: usize) -> usize {
    a.iter().zip(b).take(n).filter(|(x, y)| x != y).count()
}

fn directory_ranges(imgs: [&Image; 2]) -> Vec<(u64, u64)> {
    imgs.iter()
        .flat_map(|img| {
            [Dir::Debug, Dir::Import, Dir::Iat]
                .into_iter()
                .filter_map(|which| {
                    let d = img.present(which)?;
                    let lo = u64::from(d.rva.get());
                    let hi = lo.saturating_add(u64::from(d.size));
                    Some((lo, hi.next_multiple_of(16)))
                })
                .collect::<Vec<_>>()
        })
        .collect()
}

fn sec_at(img: &Image, i: usize) -> Option<(String, usize, bool)> {
    img.sections.iter().find_map(|s| {
        let lo = s.raw_off.get();
        let hi = lo.saturating_add(s.raw_size.get() as usize);
        (i >= lo && i < hi).then(|| {
            let content = lo.saturating_add(img.section_bytes(s).len());
            (s.name.clone(), lo, i < content)
        })
    })
}

#[must_use]
pub fn section_past_the_file(img: &Image) -> Option<(String, usize, usize)> {
    img.sections.iter().find_map(|s| {
        let lo = s.raw_off.get();
        let n = s.raw_size.get() as usize;
        (n > 0 && lo.checked_add(n).is_none_or(|hi| hi > img.len()))
            .then(|| (s.name.clone(), lo, n))
    })
}

pub fn measure(
    reference: &Image,
    candidate: &Image,
    map: &Map,
    owners: &Owners,
) -> Result<Account> {
    for (img, which) in [(reference, "reference"), (candidate, "candidate")] {
        if let Some((name, lo, n)) = section_past_the_file(img) {
            bail!(
                "the {which} image's {name} holds raw data at {lo:#x}..{:#x}, past the file's {:#x} bytes: an account would compare only the bytes the file holds",
                lo.saturating_add(n),
                img.len()
            );
        }
    }
    let syms = map_symbols(map, owners);
    if syms.is_empty() {
        bail!(
            "the map publishes no symbol row of an object, so every differing byte would classify as generated and the account would pass on a map it could not read"
        );
    }
    let (a, b) = (reference.bytes(), candidate.bytes());
    let mut acc = Account {
        symbols: syms.len(),
        file_total: differing(a, b, usize::MAX),
        ..Account::default()
    };
    let first_raw = reference
        .sections
        .iter()
        .map(|s| s.raw_off.get())
        .min()
        .unwrap_or(0);
    acc.header_diff = differing(a, b, first_raw);
    let mut covered: Vec<(usize, usize)> = vec![(0, first_raw.min(reference.len()))];
    let dir_ranges = directory_ranges([reference, candidate]);
    for rs in &reference.sections {
        let Some(cs) = candidate.sections.iter().find(|c| c.name == rs.name) else {
            continue;
        };
        let rb = reference.section_bytes(rs);
        let cb = candidate.section_bytes(cs);
        let n = rb.len().min(cb.len());
        covered.push((rs.raw_off.get(), rs.raw_off.get().saturating_add(n)));
        let lo = u64::from(cs.rva.get());
        let hi = lo.saturating_add(n as u64);
        let first = syms.partition_point(|s| s.0 < lo);
        let last = syms.partition_point(|s| s.0 < hi);
        let local = syms.get(first..last).unwrap_or_default();
        let e = acc.per.entry(rs.name.clone()).or_default();
        e.symbols = local.len();
        let below = local
            .first()
            .map_or(0, |s| usize::try_from(s.0.saturating_sub(lo)).unwrap_or(0));
        let lattice = lattice_start(rb, below);
        let clattice = lattice_start(cb, below);
        let lens = lattice
            .zip(clattice)
            .map(|(r, c)| (lattice_len(rb, r, below), lattice_len(cb, c, below)));
        let v = acc.own_ranges.entry(rs.name.clone()).or_default();
        for (k, (rva, owner)) in local.iter().enumerate() {
            if is_member(owner) {
                continue;
            }
            if let Some(nx) = local.get(k.saturating_add(1)) {
                v.push((*rva, nx.0.min(hi)));
            }
        }
        for (i, (x, y)) in rb.iter().zip(cb).enumerate() {
            if x == y {
                continue;
            }
            e.differing += 1;
            if local.is_empty() {
                e.generated += 1;
                continue;
            }
            let rva = lo.saturating_add(i as u64);
            let idx = local.partition_point(|s| s.0 <= rva);
            match idx.checked_sub(1).and_then(|k| local.get(k)) {
                Some((_, owner)) if is_member(owner) => e.library += 1,
                Some(_) => e.ours += 1,
                None if lattice.is_some_and(|l| in_thunk(rb, cb, l, i)) => e.generated += 1,
                None if lattice
                    .zip(clattice)
                    .zip(lens)
                    .is_some_and(|(starts, n)| in_unpaired_slot(rb, cb, starts, n, i)) =>
                {
                    e.generated += 1;
                    e.ilt_count += 1;
                }
                None if dir_ranges.iter().any(|(s, x)| rva >= *s && rva < *x) => {
                    e.generated += 1;
                }
                None => {
                    e.unclassified += 1;
                    if acc.unclassified.len() < UNCLASSIFIED_CAP {
                        let head = local.first().map_or(0, |f| f.0);
                        acc.unclassified.push((rs.name.clone(), rva, head));
                    }
                }
            }
        }
    }
    let cv = [codeview_range(reference), codeview_range(candidate)];
    covered.sort_unstable();
    let in_covered = |i: usize| {
        let k = covered.partition_point(|c| c.0 <= i);
        covered
            .get(..k)
            .is_some_and(|c| c.iter().any(|(s, e)| i >= *s && i < *e))
    };
    for (i, (x, y)) in a.iter().zip(b).enumerate() {
        if x == y || in_covered(i) {
            continue;
        }
        if cv
            .iter()
            .flatten()
            .any(|(s, e)| i >= *s && i < *e && *e > *s)
        {
            acc.debug_diff += 1;
            continue;
        }
        let r = sec_at(reference, i);
        let c = sec_at(candidate, i);
        match residue_class(
            r.as_ref().map(|(n, o, k)| (n.as_str(), *o, *k)),
            c.as_ref().map(|(n, o, k)| (n.as_str(), *o, *k)),
        ) {
            Residue::Padding => acc.padding += 1,
            Residue::Shifted => acc.shifted += 1,
            Residue::Unseen => acc.outside += 1,
        }
    }
    acc.displaced = reference
        .sections
        .iter()
        .filter_map(|rs| {
            let cs = candidate.sections.iter().find(|c| c.name == rs.name)?;
            (cs.raw_off.get() != rs.raw_off.get()).then(|| rs.name.clone())
        })
        .collect();
    for (name, s) in &acc.per {
        if s.ours == 0 || name == ".text" {
            continue;
        }
        if let Some((total, missing)) = acc
            .own_ranges
            .get(name)
            .and_then(|r| own_content_present(reference, candidate, name, r))
        {
            acc.content.push(Content {
                section: name.clone(),
                total,
                missing,
            });
        }
    }
    Ok(acc)
}

fn code_span(img: &Image) -> Option<(u64, u64)> {
    img.section(".text").map(|t| {
        let base = u64::from(img.opt.image_base.get());
        let lo = base.saturating_add(u64::from(t.rva.get()));
        let len = u64::from(t.virt_size.get()).max(t.disk_len() as u64);
        (lo, lo.saturating_add(len))
    })
}

#[must_use]
pub fn own_content_present(
    reference: &Image,
    candidate: &Image,
    name: &str,
    own_ranges: &[(u64, u64)],
) -> Option<(usize, Vec<AbsentValue>)> {
    let rs = reference.section(name)?;
    let cs = candidate.section(name)?;
    let rb = reference.section_bytes(rs);
    let cb = candidate.section_bytes(cs);
    let lo = u64::from(cs.rva.get());
    let fixups: BTreeSet<u64> = candidate
        .relocations()
        .iter()
        .map(|(r, _)| u64::from(r.get()))
        .collect();
    let spans = if fixups.is_empty() {
        code_span(candidate).zip(code_span(reference))
    } else {
        None
    };
    let in_span = |v: [u8; 4], (lo, hi): (u64, u64)| {
        let n = u64::from(u32::from_le_bytes(v));
        n >= lo && n < hi
    };
    let refset: std::collections::HashSet<[u8; 4]> = rb
        .windows(4)
        .filter_map(|w| <[u8; 4]>::try_from(w).ok())
        .collect();
    let mut ours: BTreeMap<[u8; 4], u64> = BTreeMap::new();
    for (a, b) in own_ranges {
        let (Ok(s), Ok(e)) = (
            usize::try_from(a.saturating_sub(lo)),
            usize::try_from(b.saturating_sub(lo)),
        ) else {
            continue;
        };
        let mut i = s.saturating_add((4 - (s % 4)) % 4);
        while i.saturating_add(4) <= e.min(cb.len()) {
            let at = lo.saturating_add(i as u64);
            let touched = fixups
                .range(at.saturating_sub(3)..=at.saturating_add(3))
                .next()
                .is_some();
            if !touched && let Some(v) = cb.get(i..i + 4).and_then(|x| <[u8; 4]>::try_from(x).ok())
            {
                let paired_address = spans.is_some_and(|(cspan, rspan)| {
                    rb.get(i..i + 4)
                        .and_then(|x| <[u8; 4]>::try_from(x).ok())
                        .is_some_and(|rv| in_span(v, cspan) && in_span(rv, rspan))
                });
                if !paired_address {
                    ours.entry(v).or_insert(at);
                }
            }
            i += 4;
        }
    }
    let missing: Vec<AbsentValue> = ours
        .iter()
        .filter(|(v, _)| !refset.contains(*v))
        .map(|(v, rva)| (*rva, *v))
        .collect();
    Some((ours.len(), missing))
}

pub fn own_text_bytes(env: &Env, candidate: &Image, objs: &[PathBuf]) -> Result<(usize, usize)> {
    if objs.is_empty() {
        bail!("no objects");
    }
    let sec = env.section();
    let csec = candidate
        .section(".text")
        .ok_or_else(|| anyhow::anyhow!("our image has no `.text`"))?;
    let rb = env.text();
    let cb = candidate.section_bytes(csec);
    let base = sec.rva.get();
    let mut seen: BTreeSet<String> = BTreeSet::new();
    let (mut differ, mut total) = (0usize, 0usize);
    for o in objs {
        let (_, members) = seat::derive(env, o)?;
        for m in members {
            let Some(r) = m.rva else { continue };
            if !seen.insert(m.name.clone()) {
                continue;
            }
            let Ok(off) = usize::try_from(r.saturating_sub(base)) else {
                continue;
            };
            for k in 0..m.size {
                match (rb.get(off.saturating_add(k)), cb.get(off.saturating_add(k))) {
                    (Some(a), Some(b)) => {
                        total += 1;
                        if a != b {
                            differ += 1;
                        }
                    }
                    _ => break,
                }
            }
        }
    }
    Ok((differ, total))
}

fn row(label: &str, s: &Split) -> String {
    format!(
        "  {label:<8} {:>10} {:>9} {:>9} {:>10} {:>13} {:>8}",
        s.differing, s.ours, s.library, s.generated, s.unclassified, s.symbols
    )
}

#[must_use]
pub fn lines(acc: &Account, ours: &str, map: &str) -> Vec<String> {
    let mut out = vec![
        format!("ACCOUNT -- {ours} against the reference"),
        "  every differing byte gets a CLASS, and an UNCLASSIFIED byte fails the account."
            .to_string(),
        format!(
            "  POPULATION: {} `/MAP' symbol row(s) from {map}.",
            acc.symbols
        ),
        String::new(),
        format!(
            "  {:<8} {:>10} {:>9} {:>9} {:>10} {:>13} {:>8}",
            "section", "differing", "OURS", "library", "generated", "UNCLASSIFIED", "symbols"
        ),
    ];
    for (name, s) in &acc.per {
        if s.differing == 0 {
            continue;
        }
        out.push(row(name, s));
        if s.ilt_count > 0 {
            out.push(format!(
                "  {:<8}   of generated, {} in ILT slots only one link has: the two links thunk a different number of functions",
                "", s.ilt_count
            ));
        }
    }
    for (label, n) in [
        ("HEADERS", acc.header_diff),
        ("DEBUG", acc.debug_diff),
        ("SHIFTED", acc.shifted),
        ("PADDING", acc.padding),
    ] {
        if n > 0 {
            out.push(format!(
                "  {label:<8} {n:>10} {:>9} {:>9} {n:>10} {:>13} {:>8}",
                0, 0, 0, 0
            ));
        }
    }
    let t = acc.totals();
    out.push(format!(
        "  {:<8} {:>10} {:>9} {:>9} {:>10} {:>13}",
        "TOTAL", t.differing, t.ours, t.library, t.generated, t.unclassified
    ));
    let unseen = acc.unseen();
    out.push(format!(
        "  COVERAGE: {} classified against {} -- {}",
        t.differing,
        if acc.displaced.is_empty() {
            format!("{} differing in the whole file", acc.file_total)
        } else {
            format!(
                "{} differing at equal FILE offsets, an OVERCOUNT here: {} section(s) sit at another raw offset in the two images ({}); coverage is asserted on ranges",
                acc.file_total,
                acc.displaced.len(),
                acc.displaced.join(", ")
            )
        },
        if unseen == 0 {
            "COMPLETE".to_string()
        } else {
            format!("{unseen} byte(s) UNSEEN")
        }
    ));
    for (name, s) in &acc.per {
        if s.ours == 0 {
            continue;
        }
        if name == ".text" {
            out.push(format!(
                "    {name:<8} NOT APPLICABLE -- own bodies hold relocated operands with our addresses"
            ));
            continue;
        }
        match acc.content.iter().find(|c| &c.section == name) {
            Some(c) => {
                out.push(format!(
                    "    {name:<8} {} of {} distinct own value(s) ABSENT from the reference -> {}",
                    c.missing.len(),
                    c.total,
                    if c.missing.is_empty() {
                        "ORDERING, not content"
                    } else {
                        "CONTENT DIFFERENCE"
                    }
                ));
                for (rva, v) in c.missing.iter().take(12) {
                    out.push(format!(
                        "               0x{rva:06x}  {:02x} {:02x} {:02x} {:02x}   (as dword 0x{:08x})",
                        v[0],
                        v[1],
                        v[2],
                        v[3],
                        u32::from_le_bytes(*v)
                    ));
                }
                if c.missing.len() > 12 {
                    out.push(format!("               ... {} more", c.missing.len() - 12));
                }
            }
            None => out.push(format!("    {name:<8} no own span to test")),
        }
    }
    out
}

#[must_use]
pub fn unclassified_runs(acc: &Account) -> Vec<String> {
    let mut runs: Vec<String> = Vec::new();
    let mut open: Option<(String, u64, u64, u64)> = None;
    let fmt = |sec: &str, start: u64, end: u64, head: u64| {
        format!(
            "    {sec} {start:#010x}..{:#010x}  {} byte(s), {} below that section's first symbol {head:#010x}",
            end.saturating_add(1),
            end.saturating_add(1).saturating_sub(start),
            head.saturating_sub(start)
        )
    };
    for (sec, rva, head) in &acc.unclassified {
        match open.take() {
            Some((osec, start, end, ohead)) if osec == *sec && *rva == end.saturating_add(1) => {
                open = Some((osec, start, *rva, ohead));
            }
            Some((osec, start, end, ohead)) => {
                if runs.len() < 12 {
                    runs.push(fmt(&osec, start, end, ohead));
                }
                open = Some((sec.clone(), *rva, *rva, *head));
            }
            None => open = Some((sec.clone(), *rva, *rva, *head)),
        }
    }
    if let Some((osec, start, end, ohead)) = open
        && runs.len() < 12
    {
        runs.push(fmt(&osec, start, end, ohead));
    }
    runs
}

#[must_use]
pub fn engineer_line(acc: &Account) -> String {
    let t = acc.totals();
    let pct = if t.differing > 0 {
        100.0 * (t.ours as f64) / (t.differing as f64)
    } else {
        0.0
    };
    format!(
        "  ENGINEER-OWNED: {} differing byte(s) ({pct:.1}% of the total); {} LIBRARY, {} generated",
        t.ours, t.library, t.generated
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    fn dir_bytes(ptr: u32, size: u32) -> Vec<u8> {
        let mut b = Vec::with_capacity(28);
        b.extend_from_slice(&[0u8; 12]);
        b.extend_from_slice(&2u32.to_le_bytes());
        b.extend_from_slice(&size.to_le_bytes());
        b.extend_from_slice(&[0u8; 4]);
        b.extend_from_slice(&ptr.to_le_bytes());
        b
    }

    #[test]
    fn the_codeview_range_comes_from_size_and_pointer_fields() {
        assert_eq!(
            codeview_from_dir(&dir_bytes(0x0013_7000, 42)),
            Some((0x0013_7000, 0x0013_702a))
        );
        assert_eq!(codeview_from_dir(&dir_bytes(0x0013_7000, 0)), None);
        let b = dir_bytes(0x0013_7000, 42);
        assert_eq!(codeview_from_dir(b.get(..24).unwrap_or(&b)), None);
    }

    #[test]
    fn an_archive_member_is_told_from_our_object_by_its_colon_and_an_own_library_is_ours() {
        let o = Owners::new(&["engine".to_string()]);
        assert!(is_member("libcmtd:xtoa.obj"));
        assert!(!is_member("Bmp.obj"));
        assert_eq!(o.map_owner("ENGINE:Arena.obj"), "Arena.obj");
        assert_eq!(o.map_owner("LIBCMT:wincrt0.obj"), "LIBCMT:wincrt0.obj");
    }

    #[test]
    fn an_ilt_slot_only_one_lattice_has_is_the_slot_count() {
        let slot = [0xe9u8, 1, 2, 3, 4];
        let mut rb = vec![0xccu8; 5];
        let mut cb = vec![0xccu8; 5];
        for _ in 0..4 {
            rb.extend_from_slice(&slot);
        }
        for _ in 0..2 {
            cb.extend_from_slice(&slot);
        }
        rb.extend_from_slice(&[0xcc; 10]);
        cb.resize(rb.len(), 0xcc);
        let limit = rb.len();
        let n = (lattice_len(&rb, 5, limit), lattice_len(&cb, 5, limit));
        assert_eq!(n, (4, 2));
        assert!((15..25).all(|o| in_unpaired_slot(&rb, &cb, (5, 5), n, o)));
        assert!(!(5..15).any(|o| in_unpaired_slot(&rb, &cb, (5, 5), n, o)));
        assert!(!in_unpaired_slot(&rb, &cb, (5, 5), n, 0));
        assert!(!(25..limit).any(|o| in_unpaired_slot(&rb, &cb, (5, 5), n, o)));
        assert!((15..25).all(|o| in_unpaired_slot(&cb, &rb, (5, 5), (2, 4), o)));
    }

    #[test]
    fn a_section_at_a_different_raw_start_is_displacement_and_filler_is_padding() {
        assert_eq!(
            residue_class(
                Some((".reloc", 0x12b000, true)),
                Some((".reloc", 0x12c000, true))
            ),
            Residue::Shifted
        );
        assert_eq!(
            residue_class(
                Some((".reloc", 0x12b000, true)),
                Some((".data", 0x12b000, true))
            ),
            Residue::Shifted
        );
        assert_eq!(
            residue_class(
                Some((".text", 0x1000, false)),
                Some((".text", 0x1000, true))
            ),
            Residue::Padding
        );
        assert_eq!(
            residue_class(
                Some((".data", 0x122000, true)),
                Some((".data", 0x122000, true))
            ),
            Residue::Unseen
        );
        assert_eq!(residue_class(None, None), Residue::Unseen);
    }
}
