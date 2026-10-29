use anyhow::{Result, bail};
use iced_x86::{Decoder, DecoderOptions, OpKind};
use msvc6_base::units::Rva;
use msvc6_image::pe::{Dir, Image};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const HIGHLOW: u16 = 3;

#[derive(Debug)]
pub struct View<'a> {
    img: &'a Image,
    base: u32,
    text: (u32, u32),
    pub ilt: (u32, u32),
    eat: Option<(u32, u32)>,
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize)]
pub enum Kind {
    Ilt,
    Rel32,
    Abs,
    Export,
}

impl Kind {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Ilt => "ILT (the thunk table)",
            Self::Rel32 => "rel32 (a decoded call or jump into the ILT)",
            Self::Abs => "abs (a relocated pointer into the ILT)",
            Self::Export => "export (an export address into the ILT)",
        }
    }
}

pub type Fields = BTreeMap<u32, (Kind, u32)>;

#[must_use]
pub fn eat_of(img: &Image) -> Option<(u32, u32)> {
    let d = img.present(Dir::Export)?;
    let n = img.u32_at_rva(Rva(d.rva.get().checked_add(20)?))?;
    let a = img.u32_at_rva(Rva(d.rva.get().checked_add(28)?))?;
    Some((a, a.checked_add(n.checked_mul(4)?)?))
}

impl<'a> View<'a> {
    pub fn new(img: &'a Image, what: &str) -> Result<Self> {
        let Some(s) = img.section(".text") else {
            bail!("{what}: no `.text'");
        };
        let text = img.section_bytes(s);
        let text_rva = s.rva.get();
        let mut i = 0usize;
        while text.get(i) == Some(&0xcc) {
            i += 1;
        }
        let start = i;
        while text.get(i) == Some(&0xe9) {
            i += 5;
        }
        if i == start {
            bail!(
                "{what}: `.text' does not open with an ILT (no `e9 rel32' run after the padding)"
            );
        }
        let len = u32::try_from(text.len())?;
        Ok(Self {
            img,
            base: img.opt.image_base.get(),
            text: (text_rva, text_rva.saturating_add(len)),
            ilt: (
                text_rva.saturating_add(u32::try_from(start)?),
                text_rva.saturating_add(u32::try_from(i)?),
            ),
            eat: eat_of(img),
        })
    }

    fn in_ilt(&self, rva: u32) -> bool {
        rva >= self.ilt.0 && rva < self.ilt.1
    }

    fn byte(&self, rva: u32) -> Option<u8> {
        self.img
            .at_rva(Rva(rva), 1)
            .and_then(|b| b.first().copied())
    }

    fn dword(&self, rva: u32) -> Option<u32> {
        self.img.u32_at_rva(Rva(rva))
    }

    #[must_use]
    pub fn body(&self, rva: u32) -> Option<u32> {
        if !self.in_ilt(rva) || self.byte(rva)? != 0xe9 {
            return None;
        }
        let rel = i64::from(i32::from_le_bytes(
            self.dword(rva.checked_add(1)?)?.to_le_bytes(),
        ));
        u32::try_from(i64::from(rva) + 5 + rel).ok()
    }

    fn slots(&self) -> impl Iterator<Item = u32> + '_ {
        (self.ilt.0..self.ilt.1).step_by(5)
    }

    fn body_starts(&self) -> BTreeSet<u32> {
        self.slots()
            .filter_map(|t| self.body(t))
            .filter(|b| *b >= self.ilt.1 && *b < self.text.1)
            .collect()
    }

    fn ilt_bodies(&self) -> BTreeMap<u32, u32> {
        let mut m = BTreeMap::new();
        for t in self.slots() {
            if let Some(b) = self.body(t) {
                *m.entry(b).or_insert(0u32) += 1;
            }
        }
        m
    }

    #[must_use]
    pub fn fields(&self) -> Fields {
        let mut out = Fields::new();
        for (r, ty) in self.img.relocations() {
            if ty != HIGHLOW {
                continue;
            }
            let p = r.get();
            if let Some(t) = self.dword(p).map(|v| v.wrapping_sub(self.base))
                && self.in_ilt(t)
            {
                out.insert(p, (Kind::Abs, t));
            }
        }
        if let Some((a, e)) = self.eat {
            let mut p = a;
            while p < e {
                if let Some(v) = self.dword(p).filter(|v| self.in_ilt(*v)) {
                    out.insert(p, (Kind::Export, v));
                }
                p = p.saturating_add(4);
            }
        }
        let starts: Vec<u32> = self.body_starts().into_iter().collect();
        for (i, s) in starts.iter().enumerate() {
            let end = starts.get(i + 1).copied().unwrap_or(self.text.1);
            let Some(bytes) = usize::try_from(end - s)
                .ok()
                .and_then(|n| self.img.at_rva(Rva(*s), n))
            else {
                continue;
            };
            let mut d = Decoder::with_ip(
                32,
                bytes,
                u64::from(self.base) + u64::from(*s),
                DecoderOptions::NONE,
            );
            while d.can_decode() {
                let ins = d.decode();
                if ins.is_invalid() || ins.op0_kind() != OpKind::NearBranch32 || ins.len() < 5 {
                    continue;
                }
                let (Ok(ip), Ok(len), Ok(target)) = (
                    u32::try_from(ins.ip()),
                    u32::try_from(ins.len()),
                    u32::try_from(ins.near_branch_target()),
                ) else {
                    continue;
                };
                let t = target.wrapping_sub(self.base);
                if self.in_ilt(t) {
                    out.insert(
                        ip.wrapping_sub(self.base).wrapping_add(len).wrapping_sub(4),
                        (Kind::Rel32, t),
                    );
                }
            }
        }
        out
    }
}

#[derive(Debug, Default, Serialize)]
pub struct Diff {
    pub differing: usize,
    pub runs: usize,
    pub longest: usize,
    pub bytes_by: BTreeMap<Kind, (usize, BTreeSet<u32>)>,
    pub unclassified: Vec<(usize, Option<u32>)>,
    pub followed: BTreeMap<Kind, usize>,
    pub moved: Vec<(Kind, u32, Option<u32>, Option<u32>)>,
    pub one_sided: Vec<u32>,
    pub ilt_bodies_changed: usize,
}

impl Diff {
    #[must_use]
    pub fn findings(&self) -> usize {
        self.unclassified.len() + self.moved.len() + self.one_sided.len()
    }
}

#[must_use]
pub fn rva_of(img: &Image, off: usize) -> Option<u32> {
    img.sections.iter().find_map(|s| {
        let o = s.raw_off.get();
        (off >= o && off < o.saturating_add(s.disk_len()))
            .then(|| {
                u32::try_from(off - o)
                    .ok()
                    .map(|d| s.rva.get().saturating_add(d))
            })
            .flatten()
    })
}

fn owner(b: &View<'_>, n: &View<'_>, fb: &Fields, fnw: &Fields, r: u32) -> Option<(Kind, u32)> {
    if b.in_ilt(r) && n.in_ilt(r) {
        return Some((Kind::Ilt, b.ilt.0 + (r - b.ilt.0) / 5 * 5));
    }
    for f in [fb, fnw] {
        if let Some((p, (k, _))) = f.range(..=r).next_back()
            && r < p.saturating_add(4)
        {
            return Some((*k, *p));
        }
    }
    None
}

pub fn compare(bi: &Image, ni: &Image) -> Result<Diff> {
    if bi.len() != ni.len() {
        bail!(
            "the two images differ in length ({} vs {} bytes): not a placement-only change",
            bi.len(),
            ni.len()
        );
    }
    let b = View::new(bi, "base")?;
    let n = View::new(ni, "new")?;
    let (fb, fnw) = (b.fields(), n.fields());
    let mut rep = Diff::default();
    let mut run = 0usize;
    for (off, (x, y)) in bi.bytes().iter().zip(ni.bytes().iter()).enumerate() {
        if x == y {
            run = 0;
            continue;
        }
        rep.differing += 1;
        if run == 0 {
            rep.runs += 1;
        }
        run += 1;
        rep.longest = rep.longest.max(run);
        let rva = rva_of(ni, off);
        match rva.and_then(|r| owner(&b, &n, &fb, &fnw, r)) {
            Some((k, field)) => {
                let e = rep.bytes_by.entry(k).or_default();
                e.0 += 1;
                e.1.insert(field);
            }
            None => rep.unclassified.push((off, rva)),
        }
    }
    let all: BTreeSet<u32> = fb.keys().chain(fnw.keys()).copied().collect();
    for p in all {
        match (fb.get(&p), fnw.get(&p)) {
            (Some((kb, tb)), Some((kn, tn))) if kb == kn => {
                *rep.followed.entry(*kb).or_insert(0) += 1;
                let (bb, bn) = (b.body(*tb), n.body(*tn));
                if bb.is_none() || bb != bn {
                    rep.moved.push((*kb, p, bb, bn));
                }
            }
            _ => rep.one_sided.push(p),
        }
    }
    let (sb, sn) = (b.ilt_bodies(), n.ilt_bodies());
    rep.ilt_bodies_changed = sb.iter().filter(|(k, v)| sn.get(k) != Some(v)).count()
        + sn.iter().filter(|(k, v)| sb.get(k) != Some(v)).count();
    Ok(rep)
}

pub type Disagreements = BTreeMap<u32, (u32, Option<u32>)>;

#[must_use]
pub fn slot_map(v: &View<'_>, s: &View<'_>) -> (usize, Disagreements) {
    let rdata = v.img.section(".rdata").map(|r| {
        (
            r.rva.get(),
            r.rva
                .get()
                .saturating_add(u32::try_from(r.disk_len()).unwrap_or(0)),
        )
    });
    let mut slots = 0usize;
    let mut disagree = Disagreements::new();
    for (p, (k, t)) in v.fields() {
        let in_rdata = rdata.is_some_and(|(a, e)| p >= a && p < e);
        if !(k == Kind::Export || (k == Kind::Abs && in_rdata)) {
            continue;
        }
        let Some(vbody) = v.body(t) else { continue };
        slots += 1;
        let sv = s.dword(p).map(|x| {
            if k == Kind::Abs {
                x.wrapping_sub(s.base)
            } else {
                x
            }
        });
        let sbody = sv.map(|r| s.body(r).unwrap_or(r));
        if sbody != Some(vbody) {
            disagree.insert(p, (vbody, sbody));
        }
    }
    (slots, disagree)
}

#[derive(Debug, Default, Serialize)]
pub struct Summary {
    pub differing: usize,
    pub unclassified: usize,
    pub followed: usize,
    pub moved: usize,
    pub one_sided: usize,
    pub slots: usize,
    pub fresh: usize,
    pub findings: usize,
}

#[derive(Debug)]
pub struct Labels<'a> {
    pub base: &'a str,
    pub new: &'a str,
    pub shipped: Option<&'a str>,
}

pub fn report(
    bi: &Image,
    ni: &Image,
    si: Option<&Image>,
    names: &Labels<'_>,
    all: bool,
) -> Result<(Vec<String>, Summary)> {
    let cap = if all { usize::MAX } else { 20 };
    let rep = compare(bi, ni)?;
    let md5 = |i: &Image| msvc6_base::digest::md5_hex(i.bytes());
    let mut out = vec![
        format!("ILT-DIFF base {} ({})", names.base, md5(bi)),
        format!("         new  {} ({})", names.new, md5(ni)),
        format!(
            "  {} byte(s) differ in {} run(s), the longest {}",
            rep.differing, rep.runs, rep.longest
        ),
    ];
    for (k, (nbytes, fields)) in &rep.bytes_by {
        out.push(format!(
            "    {:<46} {nbytes:>6} byte(s) in {} field(s)",
            k.label(),
            fields.len()
        ));
    }
    out.push(format!(
        "    {:<46} {:>6} byte(s)",
        "UNCLASSIFIED",
        rep.unclassified.len()
    ));
    for (off, rva) in rep.unclassified.iter().take(cap) {
        match rva {
            Some(r) => out.push(format!("      file {off:#x} rva {r:#x}")),
            None => out.push(format!("      file {off:#x} (headers)")),
        }
    }
    let followed: usize = rep.followed.values().sum();
    let per: Vec<String> = rep
        .followed
        .iter()
        .map(|(k, n)| format!("{k:?} {n}"))
        .collect();
    out.push(format!(
        "  THUNK-TARGET: {followed} pointer field(s) of the whole image followed through both ILTs ({}), {} \
         reach another body, {} in one image only",
        per.join(", "),
        rep.moved.len(),
        rep.one_sided.len()
    ));
    for (k, p, bb, bn) in rep.moved.iter().take(cap) {
        out.push(format!(
            "      {k:?} field at rva {p:#x}: base reaches {bb:x?}, new {bn:x?}"
        ));
    }
    for p in rep.one_sided.iter().take(cap) {
        out.push(format!("      field at rva {p:#x} in one image only"));
    }
    out.push(format!(
        "  ILT BODIES: {} body(ies) changed their count of thunks",
        rep.ilt_bodies_changed
    ));
    let mut findings = rep.findings();
    let mut sum = Summary {
        differing: rep.differing,
        unclassified: rep.unclassified.len(),
        followed,
        moved: rep.moved.len(),
        one_sided: rep.one_sided.len(),
        ..Summary::default()
    };
    if let Some(si) = si {
        let s = View::new(si, "shipped")?;
        let b = View::new(bi, "base")?;
        let n = View::new(ni, "new")?;
        let (nslots, nd) = slot_map(&n, &s);
        let (_, bd) = slot_map(&b, &s);
        let fresh: Vec<_> = nd.iter().filter(|(p, v)| bd.get(p) != Some(v)).collect();
        out.push(format!(
            "  SLOTS vs shipped {} ({}): {nslots} relocated `.rdata' / export slot(s) into the ILT followed, {} \
             reach another body ({} of them as in base: body placement), {} new",
            names.shipped.unwrap_or("-"),
            md5(si),
            nd.len(),
            nd.len() - fresh.len(),
            fresh.len()
        ));
        for (p, (nbody, sbody)) in fresh.iter().take(cap) {
            match sbody {
                Some(sbody) => out.push(format!(
                    "      rva {p:#x}: ours reaches {nbody:#x}, shipped {sbody:#x}"
                )),
                None => out.push(format!(
                    "      rva {p:#x}: ours reaches {nbody:#x}, shipped has no dword there"
                )),
            }
        }
        findings += fresh.len();
        sum.slots = nslots;
        sum.fresh = fresh.len();
    }
    sum.findings = findings;
    out.push(format!(
        "  VERDICT: {}",
        if findings == 0 {
            "PLACEMENT ONLY -- every differing byte is a thunk seat or a pointer field, and every pointer \
             field reaches the same body"
        } else {
            "NOT PLACEMENT ONLY -- see the findings above"
        }
    ));
    Ok((out, sum))
}

#[cfg(test)]
mod tests {
    use super::*;

    const BASE: u32 = 0x1000_0000;
    const BODY_A: u32 = 0x1100;
    const BODY_B: u32 = 0x1120;
    const SEAT_1: u32 = 0x1005;
    const SEAT_2: u32 = 0x100a;

    fn put32(d: &mut [u8], off: usize, v: u32) {
        d[off..off + 4].copy_from_slice(&v.to_le_bytes());
    }

    fn rel(from_next: u32, to: u32) -> u32 {
        to.wrapping_sub(from_next)
    }

    fn fixture(thunks: [(u32, u32); 2], call_to: u32, vft: [u32; 2]) -> Image {
        let mut d = vec![0u8; 0xa00];
        d[0..2].copy_from_slice(b"MZ");
        put32(&mut d, 0x3c, 0x80);
        d[0x80..0x84].copy_from_slice(b"PE\0\0");
        d[0x84..0x86].copy_from_slice(&0x14cu16.to_le_bytes());
        d[0x86..0x88].copy_from_slice(&3u16.to_le_bytes());
        d[0x94..0x96].copy_from_slice(&0xe0u16.to_le_bytes());
        let opt = 0x98;
        d[opt..opt + 2].copy_from_slice(&0x10bu16.to_le_bytes());
        put32(&mut d, opt + 28, BASE);
        put32(&mut d, opt + 32, 0x1000);
        put32(&mut d, opt + 36, 0x200);
        put32(&mut d, opt + 56, 0x4000);
        put32(&mut d, opt + 60, 0x400);
        put32(&mut d, opt + 92, 16);
        put32(&mut d, opt + 96 + 5 * 8, 0x3000);
        put32(&mut d, opt + 96 + 5 * 8 + 4, 12);
        for (i, (name, va, ptr)) in [
            (b".text\0\0\0", 0x1000u32, 0x400u32),
            (b".rdata\0\0", 0x2000, 0x600),
            (b".reloc\0\0", 0x3000, 0x800),
        ]
        .iter()
        .enumerate()
        {
            let a = opt + 0xe0 + 40 * i;
            d[a..a + 8].copy_from_slice(*name);
            put32(&mut d, a + 8, 0x200);
            put32(&mut d, a + 12, *va);
            put32(&mut d, a + 16, 0x200);
            put32(&mut d, a + 20, *ptr);
        }
        let text = |rva: u32| 0x400 + (rva - 0x1000) as usize;
        for b in &mut d[0x400..0x600] {
            *b = 0xcc;
        }
        for (seat, body) in thunks {
            d[text(seat)] = 0xe9;
            put32(&mut d, text(seat) + 1, rel(seat + 5, body));
        }
        d[text(BODY_A)] = 0xe8;
        put32(&mut d, text(BODY_A) + 1, rel(BODY_A + 5, call_to));
        d[text(BODY_A) + 5] = 0xc3;
        d[text(BODY_B)] = 0xc3;
        put32(&mut d, 0x600, BASE + vft[0]);
        put32(&mut d, 0x604, BASE + vft[1]);
        put32(&mut d, 0x800, 0x2000);
        put32(&mut d, 0x804, 12);
        d[0x808..0x80a].copy_from_slice(&0x3000u16.to_le_bytes());
        d[0x80a..0x80c].copy_from_slice(&0x3004u16.to_le_bytes());
        Image::parse(d).unwrap()
    }

    fn base() -> Image {
        fixture(
            [(SEAT_1, BODY_A), (SEAT_2, BODY_B)],
            SEAT_2,
            [SEAT_1, SEAT_2],
        )
    }

    #[test]
    fn the_fixture_has_one_field_of_each_kind_it_holds() {
        let img = base();
        let v = View::new(&img, "fixture").unwrap();
        assert_eq!(v.ilt, (SEAT_1, SEAT_2 + 5));
        let f = v.fields();
        assert_eq!(f.get(&(BODY_A + 1)), Some(&(Kind::Rel32, SEAT_2)));
        assert_eq!(f.get(&0x2000), Some(&(Kind::Abs, SEAT_1)));
        assert_eq!(f.get(&0x2004), Some(&(Kind::Abs, SEAT_2)));
        assert_eq!(f.len(), 3);
    }

    #[test]
    fn a_reseated_thunk_table_whose_referrers_follow_is_placement_only() {
        let new = fixture(
            [(SEAT_2, BODY_A), (SEAT_1, BODY_B)],
            SEAT_1,
            [SEAT_2, SEAT_1],
        );
        let rep = compare(&base(), &new).unwrap();
        assert!(rep.differing > 0);
        assert!(rep.unclassified.is_empty(), "{rep:?}");
        assert_eq!(rep.findings(), 0, "{rep:?}");
        assert_eq!(rep.followed.values().sum::<usize>(), 3);
    }

    #[test]
    fn two_thunks_trading_targets_under_unchanged_referrers_is_caught() {
        let new = fixture(
            [(SEAT_1, BODY_B), (SEAT_2, BODY_A)],
            SEAT_2,
            [SEAT_1, SEAT_2],
        );
        let rep = compare(&base(), &new).unwrap();
        assert!(rep.unclassified.is_empty());
        assert_eq!(rep.moved.len(), 3, "{rep:?}");
        assert!(rep.findings() > 0);
    }

    #[test]
    fn a_changed_opcode_is_unclassified() {
        let b = base();
        let mut d = b.bytes().to_vec();
        d[0x400 + (BODY_A - 0x1000) as usize] = 0xe9;
        let new = Image::parse(d).unwrap();
        let rep = compare(&b, &new).unwrap();
        assert_eq!(rep.unclassified.len(), 1, "{rep:?}");
        assert!(rep.findings() > 0);
    }

    #[test]
    fn a_swapped_vftable_slot_is_caught() {
        let new = fixture(
            [(SEAT_1, BODY_A), (SEAT_2, BODY_B)],
            SEAT_2,
            [SEAT_2, SEAT_1],
        );
        let rep = compare(&base(), &new).unwrap();
        assert!(rep.unclassified.is_empty());
        assert_eq!(rep.moved.len(), 2, "{rep:?}");
    }

    #[test]
    fn identical_images_have_no_findings_and_a_shipped_twin_agrees() {
        let rep = compare(&base(), &base()).unwrap();
        assert_eq!(rep.differing, 0);
        assert_eq!(rep.findings(), 0);
        let (lines, sum) = report(
            &base(),
            &base(),
            Some(&base()),
            &Labels {
                base: "a",
                new: "b",
                shipped: Some("c"),
            },
            false,
        )
        .unwrap();
        assert_eq!(sum.findings, 0);
        assert!(
            lines.last().is_some_and(|l| l.contains("PLACEMENT ONLY")),
            "{lines:?}"
        );
    }
}
