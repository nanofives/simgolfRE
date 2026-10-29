use anyhow::Result;
use iced_x86::{Decoder, DecoderOptions, OpKind};
use msvc6_base::units::Rva;
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use msvc6_image::pe::{Dir, Image};
use std::collections::{BTreeMap, BTreeSet, HashSet};

pub const CONTENT_SPAN: u32 = 1024;
pub const CONTENT_BUDGET: u64 = 1 << 34;
pub const MAX_ROUNDS: usize = 1 << 16;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Budget {
    pub content: u64,
    pub rounds: usize,
}

impl Default for Budget {
    fn default() -> Self {
        Self {
            content: CONTENT_BUDGET,
            rounds: MAX_ROUNDS,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum How {
    Report,
    Import,
    Stub,
    Code,
    Data,
    Contrib,
    Content,
    Bracket,
    Library,
    Name,
}

impl How {
    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Report => "report",
            Self::Import => "import",
            Self::Stub => "import-thunk",
            Self::Code => "code",
            Self::Data => "data",
            Self::Contrib => "contribution",
            Self::Content => "content",
            Self::Bracket => "bracket",
            Self::Library => "library-row",
            Self::Name => "name",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Proof {
    Given,
    Code { insns: usize, fields: usize },
    Bytes { bytes: usize, fields: usize },
    Uninit,
}

impl Proof {
    #[must_use]
    pub fn label(self) -> String {
        match self {
            Self::Given => "given".to_string(),
            Self::Code { insns, fields } => format!("code {insns} insn / {fields} addr"),
            Self::Bytes { bytes, fields } => format!("bytes {bytes} / {fields} ptr"),
            Self::Uninit => "uninitialised".to_string(),
        }
    }

    #[must_use]
    pub fn kind(self) -> &'static str {
        match self {
            Self::Given => "given",
            Self::Code { .. } => "code",
            Self::Bytes { .. } => "bytes",
            Self::Uninit => "uninitialised",
        }
    }
}

#[derive(Debug, Clone)]
pub struct Pair {
    pub refr: u32,
    pub len: u32,
    pub how: How,
    pub proof: Proof,
    pub votes: usize,
}

#[derive(Debug, Clone, Default)]
pub struct Anchor {
    pub names: Vec<String>,
    pub stem: String,
    pub data: bool,
}

#[derive(Debug)]
pub struct Inputs<'a> {
    pub reference: &'a Image,
    pub ours: &'a Image,
    pub given: &'a BTreeMap<u32, (u32, u32)>,
    pub fallback: &'a BTreeMap<u32, (u32, u32)>,
    pub anchors: &'a BTreeMap<u32, Anchor>,
    pub contribs: &'a [(u32, u32)],
    pub names: &'a BTreeMap<String, u32>,
    pub scoped: &'a BTreeMap<String, BTreeMap<String, u32>>,
    pub dropped: &'a BTreeMap<String, String>,
    pub budget: Budget,
}

#[derive(Debug, Default)]
pub struct Derived {
    pub pairs: BTreeMap<u32, Pair>,
    pub agree: usize,
    pub contradict: Vec<(u32, u32, u32, u32)>,
    pub icf_twins: usize,
    pub refused: Vec<(u32, u32, String)>,
    pub audited_out: Vec<(u32, u32)>,
    pub stray: usize,
    pub rounds: usize,
    pub names: BTreeMap<u32, Vec<String>>,
    pub account: Vec<(u32, &'static str, String)>,
    pub corrected: Vec<(u32, u32, u32)>,
    pub confirmed: usize,
    pub unverified: Vec<(u32, u32, String)>,
    pub content_work: u64,
    pub content_spent: bool,
    pub rounds_capped: bool,
    pub budget: Budget,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Point {
    ours: u32,
    refr: u32,
}

type Pattern = Vec<(usize, Option<Option<u32>>)>;

#[derive(Debug, Default)]
struct Walk {
    ok: bool,
    stop: usize,
    insns: usize,
    fields: Vec<Point>,
    bytes: usize,
}

#[derive(Debug)]
struct Fields {
    ours: HashSet<u32>,
    refr: HashSet<u32>,
    reloc_mode: bool,
    base: u32,
    our_map: Vec<(u32, u32)>,
    ref_map: Vec<(u32, u32)>,
}

fn mapped(img: &Image) -> Vec<(u32, u32)> {
    img.sections
        .iter()
        .map(|s| {
            let n = s.virt_size.get().max(s.raw_size.get());
            (s.rva.get(), s.rva.get().saturating_add(n))
        })
        .collect()
}

fn in_map(m: &[(u32, u32)], rva: u32) -> bool {
    m.iter().any(|(lo, hi)| rva >= *lo && rva < *hi)
}

impl Fields {
    fn new(ours: &Image, reference: &Image) -> Self {
        let highlow = |img: &Image| -> HashSet<u32> {
            img.relocations()
                .into_iter()
                .filter(|(_, t)| *t == 3)
                .map(|(r, _)| r.get())
                .collect()
        };
        let o = highlow(ours);
        let r = highlow(reference);
        let reloc_mode = !o.is_empty() && !r.is_empty();
        Self {
            ours: o,
            refr: r,
            reloc_mode,
            base: reference.opt.image_base.get(),
            our_map: mapped(ours),
            ref_map: mapped(reference),
        }
    }

    fn address(&self, fo: u32, vo: u32, fr: u32, vr: u32) -> Option<Point> {
        let ro = vo.checked_sub(self.base)?;
        let rr = vr.checked_sub(self.base)?;
        if self.reloc_mode {
            return (self.ours.contains(&fo) && self.refr.contains(&fr))
                .then_some(Point { ours: ro, refr: rr });
        }
        (vo != vr && in_map(&self.our_map, ro) && in_map(&self.ref_map, rr))
            .then_some(Point { ours: ro, refr: rr })
    }

    fn one_sided(&self, fo: u32, fr: u32) -> bool {
        self.reloc_mode && (self.ours.contains(&fo) != self.refr.contains(&fr))
    }
}

fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    let b = d.get(at..at.checked_add(4)?)?;
    Some(u32::from_le_bytes(b.try_into().ok()?))
}

fn inside_a_pairing(pairs: &BTreeMap<u32, Pair>, r: u32) -> bool {
    pairs
        .values()
        .any(|p| p.refr < r && r < p.refr.saturating_add(p.len))
}

fn init_bytes(img: &Image, rva: u32, len: u32) -> &[u8] {
    let Some(s) = img.section_of(Rva(rva)) else {
        return &[];
    };
    let Some(d) = rva.checked_sub(s.rva.get()) else {
        return &[];
    };
    let disk = u32::try_from(s.disk_len()).unwrap_or(0);
    let n = len.min(disk.saturating_sub(d));
    img.at_rva(Rva(rva), usize::try_from(n).unwrap_or(0))
        .unwrap_or(&[])
}

fn walk_code(f: &Fields, ours: &Image, a: u32, reference: &Image, b: u32, n: u32) -> Walk {
    let mut w = Walk::default();
    let ob = init_bytes(ours, a, n);
    let rb = init_bytes(reference, b, n);
    let len = ob.len().min(rb.len());
    if len == 0 {
        return w;
    }
    let (Some(oip), Some(rip)) = (a.checked_add(f.base), b.checked_add(f.base)) else {
        return w;
    };
    let mut d_o = Decoder::with_ip(
        32,
        ob.get(..len).unwrap_or(&[]),
        u64::from(oip),
        DecoderOptions::NONE,
    );
    let mut d_r = Decoder::with_ip(
        32,
        rb.get(..len).unwrap_or(&[]),
        u64::from(rip),
        DecoderOptions::NONE,
    );
    let mut table = len;
    let pad_at = ob
        .get(..len)
        .unwrap_or(&[])
        .iter()
        .rposition(|b| *b != 0x90 && *b != 0xcc)
        .map_or(0, |p| p + 1);
    let mut padded = false;
    let mut at = 0usize;
    let inside = |v: u32, start: u32| {
        v.checked_sub(start)
            .and_then(|d| usize::try_from(d).ok())
            .filter(|d| *d < len)
    };
    while at < len.min(table) {
        if at > 0 && at == pad_at && pad_at < len {
            padded = true;
            break;
        }
        let io = d_o.decode();
        let ir = d_r.decode();
        if io.is_invalid() || ir.is_invalid() || io.len() != ir.len() || io.code() != ir.code() {
            break;
        }
        let il = io.len();
        if at + il > len {
            break;
        }
        let co = d_o.get_constant_offsets(&io);
        let mut masked: Vec<(usize, u32, u32)> = Vec::new();
        let near = matches!(io.op0_kind(), OpKind::NearBranch32);
        let mut bad = false;
        if near && il >= 5 {
            let to = io.near_branch32();
            let tr = ir.near_branch32();
            match (inside(to, oip), inside(tr, rip)) {
                (Some(x), Some(y)) => bad |= x != y,
                (None, None) => {
                    w.fields.push(Point {
                        ours: to.wrapping_sub(f.base),
                        refr: tr.wrapping_sub(f.base),
                    });
                }
                _ => bad = true,
            }
            masked.push((il - 4, to, tr));
        }
        let mut field = |off: usize, vo: u32, vr: u32, w: &mut Walk, table: &mut usize| -> bool {
            let Ok(o32) = u32::try_from(at + off) else {
                return false;
            };
            let fo = a.wrapping_add(o32);
            let fr = b.wrapping_add(o32);
            if f.one_sided(fo, fr) {
                return false;
            }
            match f.address(fo, vo, fr, vr) {
                Some(p) => {
                    match (inside(vo, oip), inside(vr, rip)) {
                        (Some(x), Some(y)) => {
                            if x != y {
                                return false;
                            }
                            *table = (*table).min(x);
                        }
                        (None, None) => w.fields.push(p),
                        _ => return false,
                    }
                    masked.push((off, vo, vr));
                    true
                }
                None => vo == vr,
            }
        };
        if !near && co.has_immediate() && co.immediate_size() == 4 {
            bad |= !field(
                co.immediate_offset(),
                io.immediate32(),
                ir.immediate32(),
                &mut w,
                &mut table,
            );
        }
        if co.has_displacement() && co.displacement_size() == 4 {
            bad |= !field(
                co.displacement_offset(),
                io.memory_displacement32(),
                ir.memory_displacement32(),
                &mut w,
                &mut table,
            );
        }
        if bad {
            break;
        }
        let skip: BTreeSet<usize> = masked.iter().flat_map(|(o, _, _)| *o..*o + 4).collect();
        let same = (0..il)
            .filter(|k| !skip.contains(k))
            .all(|k| ob.get(at + k) == rb.get(at + k));
        if !same {
            break;
        }
        w.insns += 1;
        at += il;
    }
    if at >= table && table < len {
        at = table;
        while at < len {
            let (Ok(o32), Some(vo), Some(vr)) = (u32::try_from(at), u32_at(ob, at), u32_at(rb, at))
            else {
                break;
            };
            let fo = a.wrapping_add(o32);
            let fr = b.wrapping_add(o32);
            if f.one_sided(fo, fr) {
                break;
            }
            let is_ptr = if f.reloc_mode {
                f.ours.contains(&fo)
            } else {
                inside(vo, oip).is_some() && inside(vr, rip).is_some()
            };
            if is_ptr {
                if inside(vo, oip) != inside(vr, rip) || inside(vo, oip).is_none() {
                    break;
                }
                at += 4;
                continue;
            }
            if ob.get(at) != rb.get(at) {
                break;
            }
            at += 1;
        }
        while at < len && ob.get(at) == rb.get(at) {
            at += 1;
        }
    }
    w.bytes = at;
    w.stop = at;
    w.ok = (padded || at >= len) && len == usize::try_from(n).unwrap_or(usize::MAX);
    w
}

fn walk_data(f: &Fields, ours: &Image, a: u32, reference: &Image, b: u32, n: u32) -> Walk {
    let mut w = Walk::default();
    let ob = init_bytes(ours, a, n);
    let rb_all = init_bytes(reference, b, n);
    let at_r = |k: usize| -> u8 { rb_all.get(k).copied().unwrap_or(0) };
    let mut k = 0usize;
    while k < ob.len() {
        let Ok(k32) = u32::try_from(k) else { break };
        let fo = a.wrapping_add(k32);
        let fr = b.wrapping_add(k32);
        if f.one_sided(fo, fr) {
            break;
        }
        if let (Some(vo), Some(vr)) = (u32_at(ob, k), u32_at(rb_all, k)) {
            let aligned = f.reloc_mode || fo.is_multiple_of(4);
            if aligned && let Some(p) = f.address(fo, vo, fr, vr) {
                w.fields.push(p);
                k += 4;
                continue;
            }
        }
        if ob.get(k).copied() != Some(at_r(k)) {
            break;
        }
        k += 1;
    }
    w.bytes = k;
    w.stop = k;
    w.ok = k >= ob.len();
    w
}

struct State<'a> {
    i: &'a Inputs<'a>,
    f: Fields,
    anchors: BTreeMap<u32, Anchor>,
    pairs: BTreeMap<u32, Pair>,
    ref_starts: BTreeSet<u32>,
    claimed: HashSet<u32>,
    contribs: BTreeMap<u32, u32>,
    votes: BTreeMap<u32, BTreeMap<u32, usize>>,
    queue: Vec<u32>,
    walked: HashSet<u32>,
    agree: usize,
    contradict: Vec<(u32, u32, u32, u32)>,
    stray: usize,
    icf_twins: usize,
    refused: BTreeMap<u32, (u32, String)>,
    our_iat: HashSet<u32>,
    stub_sites: BTreeMap<u32, Vec<u32>>,
    content_work: std::cell::Cell<u64>,
}

impl State<'_> {
    fn is_exec(img: &Image, rva: u32) -> bool {
        img.section_of(Rva(rva))
            .is_some_and(|s| s.flags & IMAGE_SCN_MEM_EXECUTE != 0)
    }

    fn is_code(&self, rva: u32) -> bool {
        Self::is_exec(self.i.ours, rva) && !self.anchors.get(&rva).is_some_and(|a| a.data)
    }

    fn sec_name(img: &Image, rva: u32) -> Option<&str> {
        img.section_of(Rva(rva)).map(|s| s.name.as_str())
    }

    fn translate(&self, ours: u32) -> Option<u32> {
        let (start, p) = self.pairs.range(..=ours).next_back()?;
        let d = ours.checked_sub(*start)?;
        (d < p.len.max(1)).then(|| p.refr.checked_add(d)).flatten()
    }

    fn extent(&self, a: u32) -> u32 {
        let Some(s) = self.i.ours.section_of(Rva(a)) else {
            return 0;
        };
        let sec_end = s
            .rva
            .get()
            .saturating_add(s.virt_size.get().max(s.raw_size.get()));
        let mut end = sec_end;
        if let Some((n, _)) = self.anchors.range(a.saturating_add(1)..).next() {
            end = end.min(*n);
        }
        if let Some((cs, ce)) = self.contribs.range(..=a).next_back()
            && a >= *cs
            && a < *ce
        {
            end = end.min(*ce);
        }
        let mut n = end.saturating_sub(a);
        if self.is_code(a) {
            let b = init_bytes(self.i.ours, a, n);
            let keep = b.iter().rposition(|x| *x != 0xcc).map_or(0, |p| p + 1);
            n = u32::try_from(keep).unwrap_or(n);
        }
        n
    }

    fn ref_extent(&self, r: u32) -> u32 {
        let Some(s) = self.i.reference.section_of(Rva(r)) else {
            return 0;
        };
        let sec_end = s
            .rva
            .get()
            .saturating_add(s.virt_size.get().max(s.raw_size.get()));
        let next = self
            .ref_starts
            .range(r.saturating_add(1)..)
            .next()
            .copied()
            .unwrap_or(sec_end)
            .min(sec_end);
        next.saturating_sub(r)
    }

    fn thunk_target(img: &Image, rva: u32) -> Option<u32> {
        let b = img.at_rva(Rva(rva), 5)?;
        if b.first() != Some(&0xe9) {
            return None;
        }
        let rel = u32_at(b, 1)?;
        Some(rva.wrapping_add(5).wrapping_add(rel))
    }

    fn icf_twin(&self, p: Point) -> bool {
        if !self.is_code(p.ours) || !self.anchors.contains_key(&p.ours) {
            return false;
        }
        let n = self.extent(p.ours);
        n > 0 && walk_code(&self.f, self.i.ours, p.ours, self.i.reference, p.refr, n).ok
    }

    fn vote(&mut self, p: Point, from: u32) {
        let mut p = p;
        if !self.anchors.contains_key(&p.ours)
            && Self::is_exec(self.i.ours, p.ours)
            && let (Some(to), Some(tr)) = (
                Self::thunk_target(self.i.ours, p.ours),
                Self::thunk_target(self.i.reference, p.refr),
            )
        {
            p = Point { ours: to, refr: tr };
        }
        if let Some(t) = self.translate(p.ours) {
            if t == p.refr {
                self.agree += 1;
            } else if self.icf_twin(p) {
                self.icf_twins += 1;
            } else {
                self.contradict.push((p.ours, t, p.refr, from));
            }
            return;
        }
        let Some((a, _)) = self.anchors.range(..=p.ours).next_back() else {
            self.stray += 1;
            return;
        };
        let a = *a;
        let d = p.ours - a;
        if d >= self.extent(a).max(1) {
            self.stray += 1;
            return;
        }
        let Some(c) = p.refr.checked_sub(d) else {
            return;
        };
        *self.votes.entry(a).or_default().entry(c).or_default() += 1;
    }

    fn walk(&self, a: u32, p: &Pair) -> Walk {
        if self.is_code(a) {
            walk_code(&self.f, self.i.ours, a, self.i.reference, p.refr, p.len)
        } else {
            walk_data(&self.f, self.i.ours, a, self.i.reference, p.refr, p.len)
        }
    }

    fn disagreeing(&self, w: &Walk) -> Option<Point> {
        w.fields
            .iter()
            .find(|p| self.translate(p.ours).is_some_and(|t| t != p.refr))
            .copied()
    }

    fn check(&self, a: u32, c: u32, votes: usize, how: How) -> std::result::Result<Pair, String> {
        let (Some(so), Some(sr)) = (
            Self::sec_name(self.i.ours, a),
            Self::sec_name(self.i.reference, c),
        ) else {
            return Err("candidate outside the reference's sections".to_string());
        };
        if so != sr {
            return Err(format!("our {so}, the reference's {sr}"));
        }
        let n = self.extent(a);
        if n == 0 {
            return Err("empty extent".to_string());
        }
        if self.is_code(a) {
            let c = match (
                Self::thunk_target(self.i.reference, c),
                Self::thunk_target(self.i.ours, a),
            ) {
                (Some(t), None) => t,
                _ => c,
            };
            let w = walk_code(&self.f, self.i.ours, a, self.i.reference, c, n);
            let w = if w.ok {
                w
            } else {
                let d = walk_data(&self.f, self.i.ours, a, self.i.reference, c, n);
                if !d.ok {
                    return Err(format!("code differs at +{:#x} of {n:#x}", w.stop));
                }
                if let Some(bad) = self.disagreeing(&d) {
                    return Err(format!(
                        "operand {:#x} translates elsewhere than the reference's {:#x}",
                        bad.ours, bad.refr
                    ));
                }
                return Ok(Pair {
                    refr: c,
                    len: n,
                    how,
                    proof: Proof::Bytes {
                        bytes: d.bytes,
                        fields: d.fields.len(),
                    },
                    votes,
                });
            };
            if let Some(bad) = self.disagreeing(&w) {
                return Err(format!(
                    "operand {:#x} translates elsewhere than the reference's {:#x}",
                    bad.ours, bad.refr
                ));
            }
            return Ok(Pair {
                refr: c,
                len: n,
                how,
                proof: Proof::Code {
                    insns: w.insns,
                    fields: w.fields.len(),
                },
                votes,
            });
        }
        let n = n.min(self.ref_extent(c).max(1));
        let init = u32::try_from(init_bytes(self.i.ours, a, n).len()).unwrap_or(0);
        if init == 0 {
            return Ok(Pair {
                refr: c,
                len: n,
                how,
                proof: Proof::Uninit,
                votes,
            });
        }
        let w = walk_data(&self.f, self.i.ours, a, self.i.reference, c, n);
        if !w.ok {
            return Err(format!("bytes differ at +{:#x} of {init:#x}", w.stop));
        }
        if let Some(bad) = self.disagreeing(&w) {
            return Err(format!(
                "pointer {:#x} translates elsewhere than the reference's {:#x}",
                bad.ours, bad.refr
            ));
        }
        Ok(Pair {
            refr: c,
            len: n,
            how,
            proof: Proof::Bytes {
                bytes: w.bytes,
                fields: w.fields.len(),
            },
            votes,
        })
    }

    fn accept(&mut self, a: u32, p: Pair) {
        self.ref_starts.insert(p.refr);
        self.claimed.insert(p.refr);
        self.pairs.insert(a, p);
        self.queue.push(a);
        self.refused.remove(&a);
    }

    fn open(&self) -> Vec<u32> {
        self.anchors
            .keys()
            .copied()
            .filter(|a| self.translate(*a).is_none())
            .collect()
    }

    fn drain(&mut self) {
        while let Some(a) = self.queue.pop() {
            if !self.walked.insert(a) {
                continue;
            }
            let Some(p) = self.pairs.get(&a).cloned() else {
                continue;
            };
            let w = self.walk(a, &p);
            for pt in w.fields {
                self.vote(pt, a);
            }
        }
    }

    fn resolve(&mut self) -> usize {
        let votes = std::mem::take(&mut self.votes);
        let mut made = 0usize;
        for (a, cands) in votes {
            if self.translate(a).is_some() {
                continue;
            }
            let mut order: Vec<(u32, usize)> = cands.into_iter().collect();
            order.sort_by(|x, y| y.1.cmp(&x.1).then(x.0.cmp(&y.0)));
            let total: usize = order.iter().map(|(_, n)| n).sum();
            let how = if self.is_code(a) {
                How::Code
            } else {
                How::Data
            };
            let mut why = String::new();
            for (c, n) in &order {
                match self.check(a, *c, *n, how) {
                    Ok(p) => {
                        if p.proof == Proof::Uninit && *n != total {
                            why = format!("uninitialised and the votes split {n} of {total}");
                            break;
                        }
                        self.accept(a, p);
                        made += 1;
                        why.clear();
                        break;
                    }
                    Err(e) => {
                        if why.is_empty() {
                            why = e;
                        }
                    }
                }
            }
            if !why.is_empty() {
                let best = order.first().map_or(0, |(c, _)| *c);
                self.refused.insert(a, (best, why));
            }
        }
        made
    }

    fn ref_stubs(reference: &Image, iat: &HashSet<u32>, f: &Fields) -> BTreeMap<u32, Vec<u32>> {
        let base = f.base;
        let mut theirs: BTreeMap<u32, Vec<u32>> = BTreeMap::new();
        for s in &reference.sections {
            if s.flags & IMAGE_SCN_MEM_EXECUTE == 0 {
                continue;
            }
            let b = reference.section_bytes(s);
            for k in 0..b.len().saturating_sub(5) {
                if b.get(k) != Some(&0xff) || b.get(k + 1) != Some(&0x25) {
                    continue;
                }
                let (Some(v), Ok(k32)) = (u32_at(b, k + 2), u32::try_from(k)) else {
                    continue;
                };
                let slot = v.wrapping_sub(base);
                let at = s.rva.get().wrapping_add(k32);
                let attested = !f.reloc_mode || f.refr.contains(&at.wrapping_add(2));
                if iat.contains(&slot) && attested {
                    theirs.entry(slot).or_default().push(at);
                }
            }
        }
        theirs
    }

    fn our_stub(&self, a: u32) -> Option<u32> {
        if !Self::is_exec(self.i.ours, a) {
            return None;
        }
        let b = self.i.ours.at_rva(Rva(a), 6)?;
        if b.first() != Some(&0xff) || b.get(1) != Some(&0x25) {
            return None;
        }
        let slot = u32_at(b, 2)?.wrapping_sub(self.f.base);
        self.our_iat.contains(&slot).then_some(slot)
    }

    fn stubs(&mut self) -> usize {
        let mut made = 0usize;
        for a in self.open() {
            let Some(slot) = self.our_stub(a) else {
                continue;
            };
            let Some(rs) = self.translate(slot) else {
                continue;
            };
            if let Some([r]) = self.stub_sites.get(&rs).map(Vec::as_slice) {
                if inside_a_pairing(&self.pairs, *r) {
                    self.refused.insert(
                        a,
                        (
                            *r,
                            "the reference's only jmp through this import is inside a paired body"
                                .to_string(),
                        ),
                    );
                    continue;
                }
                let r = *r;
                self.accept(
                    a,
                    Pair {
                        refr: r,
                        len: 6,
                        how: How::Stub,
                        proof: Proof::Code {
                            insns: 1,
                            fields: 1,
                        },
                        votes: 0,
                    },
                );
                made += 1;
            }
        }
        made
    }

    fn by_contribution(&mut self) -> usize {
        let mut made = 0usize;
        let contribs: Vec<(u32, u32)> = self.contribs.iter().map(|(s, e)| (*s, *e)).collect();
        for (s, e) in contribs {
            let deltas: BTreeSet<i64> = self
                .pairs
                .range(s..e)
                .map(|(o, p)| i64::from(p.refr) - i64::from(*o))
                .collect();
            let [delta] = deltas.iter().copied().collect::<Vec<_>>()[..] else {
                continue;
            };
            let members = self.pairs.range(s..e).count();
            let open: Vec<u32> = self
                .anchors
                .range(s..e)
                .map(|(a, _)| *a)
                .filter(|a| self.translate(*a).is_none())
                .collect();
            for a in open {
                let Ok(c) = u32::try_from(i64::from(a) + delta) else {
                    continue;
                };
                match self.check(a, c, members, How::Contrib) {
                    Ok(p) if p.proof == Proof::Uninit && members < 2 => {}
                    Ok(p) => {
                        self.accept(a, p);
                        made += 1;
                    }
                    Err(e) => {
                        self.refused
                            .entry(a)
                            .or_insert((c, format!("contribution: {e}")));
                    }
                }
            }
        }
        made
    }

    fn pattern(&self, a: u32, n: u32) -> (Pattern, usize, usize) {
        let ob = init_bytes(self.i.ours, a, n);
        let mut pat: Pattern = Vec::new();
        let mut known = 0usize;
        let here = |s: &Self, v: u32| -> Option<Option<u32>> {
            let r = v.checked_sub(s.f.base)?;
            in_map(&s.f.our_map, r).then(|| s.translate(r).and_then(|t| t.checked_add(s.f.base)))
        };
        let mut fields: BTreeMap<usize, u32> = BTreeMap::new();
        if self.is_code(a) {
            let Some(ip) = a.checked_add(self.f.base) else {
                return (pat, 0, 0);
            };
            let mut d = Decoder::with_ip(32, ob, u64::from(ip), DecoderOptions::NONE);
            let mut at = 0usize;
            while at < ob.len() {
                let ins = d.decode();
                if ins.is_invalid() || at + ins.len() > ob.len() {
                    break;
                }
                let co = d.get_constant_offsets(&ins);
                if matches!(ins.op0_kind(), OpKind::NearBranch32) && ins.len() >= 5 {
                    fields.insert(at + ins.len() - 4, ins.near_branch32());
                } else if co.has_immediate() && co.immediate_size() == 4 {
                    fields.insert(at + co.immediate_offset(), ins.immediate32());
                }
                if co.has_displacement() && co.displacement_size() == 4 {
                    fields.insert(at + co.displacement_offset(), ins.memory_displacement32());
                }
                at += ins.len();
            }
        } else {
            let mut k = 0usize;
            while k + 4 <= ob.len() {
                let Ok(k32) = u32::try_from(k) else { break };
                let fo = a.wrapping_add(k32);
                let relocated = if self.f.reloc_mode {
                    self.f.ours.contains(&fo)
                } else {
                    fo.is_multiple_of(4)
                };
                match u32_at(ob, k) {
                    Some(v) if relocated && here(self, v).is_some() => {
                        fields.insert(k, v);
                        k += 4;
                    }
                    _ => k += 1,
                }
            }
        }
        let mut k = 0usize;
        while k < ob.len() {
            match fields.get(&k) {
                Some(v) if k + 4 <= ob.len() => {
                    let t = here(self, *v).flatten();
                    known += usize::from(t.is_some());
                    pat.push((k, Some(t)));
                    k += 4;
                }
                _ => {
                    pat.push((k, None));
                    k += 1;
                }
            }
        }
        let plain = pat.iter().filter(|(_, x)| x.is_none()).count();
        (pat, known, plain)
    }

    fn content_hits(&self, a: u32, n: u32, cap: usize) -> Vec<u32> {
        let ob = init_bytes(self.i.ours, a, n);
        let mut hits = Vec::new();
        if ob.is_empty() || self.content_work.get() >= self.i.budget.content {
            return hits;
        }
        let (pat, _, _) = self.pattern(a, n);
        let Some(rs) = Self::sec_name(self.i.ours, a).and_then(|s| self.i.reference.section(s))
        else {
            return hits;
        };
        let rb = self.i.reference.section_bytes(rs);
        let rbase = rs.rva.get();
        let step = if a.is_multiple_of(4) { 4 } else { 1 };
        let mut at = if step == 4 {
            usize::try_from(rbase % 4).map_or(0, |p| (4 - p) % 4)
        } else {
            0
        };
        let mut tried = 0u64;
        while at + ob.len() <= rb.len() {
            if self.content_work.get().saturating_add(tried) >= self.i.budget.content {
                break;
            }
            let fits = pat.iter().all(|(k, x)| {
                tried += 1;
                match x {
                    None => rb.get(at + k) == ob.get(*k),
                    Some(want) => {
                        let Some(v) = u32_at(rb, at + k) else {
                            return false;
                        };
                        let site = rbase.wrapping_add(u32::try_from(at + k).unwrap_or(0));
                        match want {
                            Some(w) => v == *w,
                            None => {
                                if self.f.reloc_mode && !self.is_code(a) {
                                    self.f.refr.contains(&site)
                                } else {
                                    in_map(&self.f.ref_map, v.wrapping_sub(self.f.base))
                                }
                            }
                        }
                    }
                }
            });
            if fits {
                let r = rbase.wrapping_add(u32::try_from(at).unwrap_or(0));
                if !self.claimed.contains(&r) {
                    hits.push(r);
                }
                if hits.len() >= cap {
                    break;
                }
            }
            at += step;
        }
        self.content_work
            .set(self.content_work.get().saturating_add(tried));
        hits
    }

    fn by_content(&mut self) -> usize {
        let mut made = 0usize;
        for a in self.open() {
            let n = self.extent(a).min(CONTENT_SPAN);
            let ob = init_bytes(self.i.ours, a, n);
            if ob.is_empty() || ob.iter().all(|b| *b == 0) {
                continue;
            }
            let literal = self
                .anchors
                .get(&a)
                .is_some_and(|x| x.names.iter().any(|m| m.starts_with("??_C@")));
            let (_, known, plain) = self.pattern(a, n);
            if known == 0 && !(literal || plain >= 8) {
                continue;
            }
            if let [r] = self.content_hits(a, n, 2)[..] {
                match self.check(a, r, 1, How::Content) {
                    Ok(p) if p.proof != Proof::Uninit => {
                        self.accept(a, p);
                        made += 1;
                    }
                    Ok(_) => {}
                    Err(e) => {
                        self.refused
                            .entry(a)
                            .or_insert((r, format!("content: {e}")));
                    }
                }
            }
        }
        made
    }

    fn account(&self) -> Vec<(u32, &'static str, String)> {
        let mut out = Vec::new();
        for a in self.open() {
            let n = self.extent(a);
            if n == 0 {
                out.push((a, "EMPTY", "no extent (a zero-length symbol)".to_string()));
                continue;
            }
            if let Some(x) = self.anchors.get(&a)
                && x.names.iter().any(|m| self.i.dropped.contains_key(m))
            {
                out.push((
                    a,
                    "OPT:REF-DROPPED",
                    "the image's dropped list: the original link removed it".to_string(),
                ));
                continue;
            }
            if let Some(s) = self.our_stub(a) {
                match self.translate(s) {
                    None => {
                        out.push((
                            a,
                            "IMPORT-ABSENT",
                            format!(
                                "a thunk through our IAT slot {s:#x}, which the reference does not import"
                            ),
                        ));
                        continue;
                    }
                    Some(rs) if !self.stub_sites.contains_key(&rs) => {
                        out.push((
                            a,
                            "THUNK-ABSENT",
                            format!(
                                "a thunk through IAT slot {rs:#x} of the reference, which holds no \
                                 `jmp [{rs:#x}]` anywhere"
                            ),
                        ));
                        continue;
                    }
                    Some(_) => {}
                }
            }
            if self.our_iat.contains(&a) {
                out.push((
                    a,
                    "IMPORT-ABSENT",
                    "an IAT slot of ours the reference does not import".to_string(),
                ));
                continue;
            }
            if init_bytes(self.i.ours, a, n).is_empty() {
                out.push((
                    a,
                    "UNINITIALISED",
                    format!("{n:#x} byte(s) of .bss, nothing to match"),
                ));
                continue;
            }
            let (_, known, plain) = self.pattern(a, n.min(CONTENT_SPAN));
            let short = if known == 0 && plain < 8 {
                " (and too short to identify: no address we translate, under 8 literal bytes)"
            } else {
                ""
            };
            let hits = self.content_hits(a, n.min(CONTENT_SPAN), 3);
            let why = self
                .refused
                .get(&a)
                .map_or_else(String::new, |(c, w)| format!("; refused {c:#x}: {w}"));
            match hits.len() {
                0 => out.push((
                    a,
                    "ABSENT",
                    format!("{n:#x} byte(s) occur nowhere in the reference's section{why}"),
                )),
                1 => out.push((
                    a,
                    "ONE-HIT",
                    format!(
                        "one candidate {:#x} that did not verify{short}{why}",
                        hits.first().copied().unwrap_or(0)
                    ),
                )),
                _ => out.push((
                    a,
                    "AMBIGUOUS",
                    format!("{}+ candidates, none decided{short}{why}", hits.len()),
                )),
            }
        }
        out
    }

    fn by_fallback(&mut self) -> usize {
        let mut made = 0usize;
        let rows: Vec<(u32, u32)> = self.i.fallback.iter().map(|(a, (r, _))| (*a, *r)).collect();
        for (a, r) in rows {
            if self.translate(a).is_some() || !self.anchors.contains_key(&a) {
                continue;
            }
            match self.check(a, r, 0, How::Library) {
                Ok(p) => {
                    self.accept(a, p);
                    made += 1;
                }
                Err(e) => {
                    self.refused
                        .entry(a)
                        .or_insert((r, format!("library row: {e}")));
                }
            }
        }
        made
    }

    fn by_bracket(&mut self) -> usize {
        let mut made = 0usize;
        let open = self.open();
        let mut k = 0usize;
        while k < open.len() {
            let Some(&first) = open.get(k) else { break };
            let mut run = vec![first];
            let mut j = k + 1;
            while let Some(&n) = open.get(j) {
                let last = run.last().copied().unwrap_or(first);
                if self.pairs.range(last.saturating_add(1)..n).next().is_some() {
                    break;
                }
                run.push(n);
                j += 1;
            }
            k = j;
            let last = run.last().copied().unwrap_or(first);
            let prev = self
                .pairs
                .range(..first)
                .next_back()
                .map(|(a, p)| (*a, p.clone()));
            let next = self
                .pairs
                .range(last.saturating_add(1)..)
                .next()
                .map(|(a, p)| (*a, p.clone()));
            let (Some((pa, pp)), Some((na, np))) = (prev, next) else {
                continue;
            };
            let sec = |x: u32| self.i.ours.section_of(Rva(x)).map(|s| s.rva);
            if sec(pa) != sec(first) || sec(na) != sec(last) {
                continue;
            }
            let dp = i64::from(pp.refr) - i64::from(pa);
            let dn = i64::from(np.refr) - i64::from(na);
            if dp != dn || pa.saturating_add(pp.len) > first {
                continue;
            }
            for a in &run {
                let Ok(c) = u32::try_from(i64::from(*a) + dp) else {
                    continue;
                };
                match self.check(*a, c, 2, How::Bracket) {
                    Ok(p) if p.proof == Proof::Uninit && run.len() > 1 => {}
                    Ok(p) => {
                        self.accept(*a, p);
                        made += 1;
                    }
                    Err(e) => {
                        self.refused
                            .entry(*a)
                            .or_insert((c, format!("bracket: {e}")));
                    }
                }
            }
        }
        made
    }

    fn by_name(&mut self) -> usize {
        let mut made = 0usize;
        for a in self.open() {
            let Some(an) = self.anchors.get(&a).cloned() else {
                continue;
            };
            let scope = self.i.scoped.get(&an.stem);
            let mut cands: BTreeSet<u32> = BTreeSet::new();
            for n in &an.names {
                if let Some(r) = scope.and_then(|s| s.get(n)).or_else(|| self.i.names.get(n)) {
                    cands.insert(*r);
                }
            }
            for c in cands {
                match self.check(a, c, 1, How::Name) {
                    Ok(p) if p.proof == Proof::Uninit && self.claimed.contains(&c) => {}
                    Ok(p) => {
                        self.accept(a, p);
                        made += 1;
                        break;
                    }
                    Err(e) => {
                        self.refused.entry(a).or_insert((c, format!("name: {e}")));
                    }
                }
            }
        }
        made
    }
}

fn imports(ours: &Image, reference: &Image) -> Result<Vec<(u32, u32, u32)>> {
    let mut out = Vec::new();
    let o = ours.imports()?;
    let r = reference.imports()?;
    let od = ours.dir(Dir::Import);
    let rd = reference.dir(Dir::Import);
    for (oi, odll) in o.iter().enumerate() {
        let Some((ri, rdll)) = r
            .iter()
            .enumerate()
            .find(|(_, d)| d.dll.eq_ignore_ascii_case(&odll.dll))
        else {
            continue;
        };
        if let (Some(od), Some(rd), Ok(oi32), Ok(ri32)) =
            (od, rd, u32::try_from(oi), u32::try_from(ri))
        {
            out.push((
                od.rva.get().wrapping_add(20u32.wrapping_mul(oi32)),
                rd.rva.get().wrapping_add(20u32.wrapping_mul(ri32)),
                20,
            ));
        }
        let nlen = u32::try_from(odll.dll.len() + 1).unwrap_or(1);
        out.push((odll.name_rva.get(), rdll.name_rva.get(), nlen));
        let mut matched = 0usize;
        for (k, e) in odll.entries.iter().enumerate() {
            let Some((j, re)) = rdll
                .entries
                .iter()
                .enumerate()
                .find(|(_, x)| x.name == e.name && x.by_ordinal() == e.by_ordinal())
            else {
                continue;
            };
            matched += 1;
            out.push((e.iat_rva.get(), re.iat_rva.get(), 4));
            if let (Ok(k), Ok(j)) = (u32::try_from(k), u32::try_from(j))
                && odll.ilt_rva.get() != 0
                && rdll.ilt_rva.get() != 0
            {
                out.push((
                    odll.ilt_rva.get().wrapping_add(4u32.wrapping_mul(k)),
                    rdll.ilt_rva.get().wrapping_add(4u32.wrapping_mul(j)),
                    4,
                ));
            }
            if let (Some(oh), Some(rh)) = (e.hint_rva, re.hint_rva) {
                let l = u32::try_from(2 + e.name.len() + 1).unwrap_or(2);
                out.push((oh.get(), rh.get(), l + (l & 1)));
            }
        }
        if matched == odll.entries.len()
            && odll.entries.len() == rdll.entries.len()
            && let Ok(n) = u32::try_from(odll.entries.len())
        {
            out.push((
                odll.iat_rva.get().wrapping_add(4u32.wrapping_mul(n)),
                rdll.iat_rva.get().wrapping_add(4u32.wrapping_mul(n)),
                4,
            ));
            if odll.ilt_rva.get() != 0 && rdll.ilt_rva.get() != 0 {
                out.push((
                    odll.ilt_rva.get().wrapping_add(4u32.wrapping_mul(n)),
                    rdll.ilt_rva.get().wrapping_add(4u32.wrapping_mul(n)),
                    4,
                ));
            }
        }
    }
    if let (Some(od), Some(rd), Ok(on), Ok(rn)) =
        (od, rd, u32::try_from(o.len()), u32::try_from(r.len()))
        && on == rn
    {
        out.push((
            od.rva.get().wrapping_add(20u32.wrapping_mul(on)),
            rd.rva.get().wrapping_add(20u32.wrapping_mul(rn)),
            20,
        ));
    }
    Ok(out)
}

fn iat_slots(img: &Image) -> Result<HashSet<u32>> {
    let mut out = HashSet::new();
    for d in img.imports()? {
        for e in &d.entries {
            out.insert(e.iat_rva.get());
        }
    }
    Ok(out)
}

pub fn derive(i: &Inputs<'_>) -> Result<Derived> {
    let f = Fields::new(i.ours, i.reference);
    let mut contribs = BTreeMap::new();
    let mut anchors = i.anchors.clone();
    for (s, n) in i.contribs {
        if *n > 0 {
            let e = s.saturating_add(*n);
            let cur = contribs.entry(*s).or_insert(e);
            *cur = (*cur).max(e);
            anchors.entry(*s).or_default();
        }
    }
    for a in i.given.keys().chain(i.fallback.keys()) {
        anchors.entry(*a).or_default();
    }
    let ref_iat = iat_slots(i.reference)?;
    let stub_sites = State::ref_stubs(i.reference, &ref_iat, &f);
    let mut st = State {
        i,
        f,
        anchors,
        pairs: BTreeMap::new(),
        ref_starts: BTreeSet::new(),
        claimed: HashSet::new(),
        contribs,
        votes: BTreeMap::new(),
        queue: Vec::new(),
        walked: HashSet::new(),
        agree: 0,
        contradict: Vec::new(),
        stray: 0,
        icf_twins: 0,
        refused: BTreeMap::new(),
        our_iat: iat_slots(i.ours)?,
        stub_sites,
        content_work: std::cell::Cell::new(0),
    };
    for (o, (r, n)) in i.given {
        st.pairs.insert(
            *o,
            Pair {
                refr: *r,
                len: *n,
                how: How::Report,
                proof: Proof::Given,
                votes: 0,
            },
        );
        st.claimed.insert(*r);
        st.ref_starts.insert(*r);
        st.queue.push(*o);
    }
    for (o, r, n) in imports(i.ours, i.reference)? {
        if st.translate(o).is_none() {
            st.pairs.insert(
                o,
                Pair {
                    refr: r,
                    len: n,
                    how: How::Import,
                    proof: Proof::Given,
                    votes: 0,
                },
            );
            st.claimed.insert(r);
            st.ref_starts.insert(r);
        }
    }
    for (r, _) in i.fallback.values() {
        st.ref_starts.insert(*r);
    }
    for v in i.names.values() {
        st.ref_starts.insert(*v);
    }
    st.stubs();
    let mut rounds = 0usize;
    let mut capped = true;
    while rounds < i.budget.rounds {
        rounds += 1;
        st.drain();
        if st.resolve() > 0 || st.stubs() > 0 || st.by_contribution() > 0 || st.by_content() > 0 {
            continue;
        }
        if st.by_fallback() > 0 || st.by_bracket() > 0 || st.by_name() > 0 {
            continue;
        }
        capped = false;
        break;
    }
    let mut audited_out = Vec::new();
    let derived: Vec<u32> = st
        .pairs
        .iter()
        .filter(|(_, p)| !matches!(p.how, How::Report | How::Import))
        .map(|(a, _)| *a)
        .collect();
    for a in derived {
        let Some(p) = st.pairs.get(&a).cloned() else {
            continue;
        };
        let inside = p.how == How::Stub && inside_a_pairing(&st.pairs, p.refr);
        let w = st.walk(a, &p);
        if inside || st.disagreeing(&w).is_some() {
            audited_out.push((a, p.refr));
            st.pairs.remove(&a);
        }
    }
    let account = st.account();
    let mut corrected = Vec::new();
    let mut confirmed = 0usize;
    let mut unverified = Vec::new();
    for (a, (r, _)) in i.fallback {
        match st.pairs.get(a) {
            Some(p) if p.refr == *r => confirmed += 1,
            Some(p) => corrected.push((*a, *r, p.refr)),
            None => {
                let why = st
                    .refused
                    .get(a)
                    .map_or_else(|| "no check ran".to_string(), |(_, w)| w.clone());
                unverified.push((*a, *r, why));
            }
        }
    }
    let pairs: BTreeMap<u32, Pair> = st
        .pairs
        .iter()
        .filter(|(_, p)| p.how != How::Report)
        .map(|(a, p)| (*a, p.clone()))
        .collect();
    let names = pairs
        .keys()
        .filter_map(|a| st.anchors.get(a).map(|x| (*a, x.names.clone())))
        .collect();
    Ok(Derived {
        pairs,
        agree: st.agree,
        contradict: st.contradict,
        icf_twins: st.icf_twins,
        refused: st
            .refused
            .into_iter()
            .map(|(a, (c, why))| (a, c, why))
            .collect(),
        audited_out,
        stray: st.stray,
        rounds,
        names,
        account,
        corrected,
        confirmed,
        unverified,
        content_work: st.content_work.get(),
        content_spent: st.content_work.get() >= i.budget.content,
        rounds_capped: capped,
        budget: i.budget,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_fixed_image_reads_only_a_differing_in_image_pair_as_an_address() {
        let f = Fields {
            ours: HashSet::new(),
            refr: HashSet::new(),
            reloc_mode: false,
            base: 0x40_0000,
            our_map: vec![(0x1000, 0x10_0000)],
            ref_map: vec![(0x1000, 0x10_0000)],
        };
        assert_eq!(
            f.address(0, 0x4c_5000, 0, 0x4b_d000),
            Some(Point {
                ours: 0xc_5000,
                refr: 0xb_d000
            })
        );
        assert_eq!(f.address(0, 0x4c_5000, 0, 0x4c_5000), None, "equal: bytes");
        assert_eq!(f.address(0, 0x10, 0, 0x20), None, "not in the image");
    }

    #[test]
    fn a_relocated_image_needs_both_sides_listed() {
        let f = Fields {
            ours: [0x100u32].into_iter().collect(),
            refr: [0x200u32].into_iter().collect(),
            reloc_mode: true,
            base: 0x1000_0000,
            our_map: vec![(0x1000, 0x10_0000)],
            ref_map: vec![(0x1000, 0x10_0000)],
        };
        assert!(f.address(0x100, 0x1000_5000, 0x200, 0x1000_6000).is_some());
        assert!(f.address(0x104, 0x1000_5000, 0x204, 0x1000_6000).is_none());
        assert!(f.one_sided(0x100, 0x204));
        assert!(!f.one_sided(0x100, 0x200));
    }

    #[test]
    fn a_site_inside_a_paired_body_is_not_a_start_of_its_own() {
        let mut pairs = BTreeMap::new();
        pairs.insert(
            0xb2df9,
            Pair {
                refr: 0xac691,
                len: 74,
                how: How::Report,
                proof: Proof::Given,
                votes: 0,
            },
        );
        assert!(inside_a_pairing(&pairs, 0xac6ab));
        assert!(!inside_a_pairing(&pairs, 0xac691));
        assert!(!inside_a_pairing(&pairs, 0xac691 + 74));
    }
}
