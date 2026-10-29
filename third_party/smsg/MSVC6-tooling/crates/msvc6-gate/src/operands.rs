use iced_x86::{
    Decoder, DecoderOptions, FlowControl, Formatter as _, Instruction, MasmFormatter, Mnemonic,
    OpKind, Register,
};
use msvc6_diff::segments::{Analysis, Pairing, Seg, Side, align_segment};
use std::collections::{BTreeMap, BTreeSet, HashMap};

pub const WINDOW: usize = 8;
pub const REBASE: usize = 3;
pub const CHKSTK: u32 = u32::MAX;
pub const POP_WINDOW: usize = 0x4000;

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, serde::Serialize)]
pub enum Val {
    Frame(i32),
    Field(i32),
    Imm(u32),
    Key(String),
}

#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct Hit {
    pub seg: usize,
    pub img_ix: usize,
    pub ours_ix: usize,
    pub class: &'static str,
    pub cert: f64,
    pub image: Val,
    pub ours: Val,
    pub note: &'static str,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Tally {
    pub pairs: usize,
    pub reorders: usize,
}

fn d32(v: u32) -> i32 {
    i32::from_le_bytes(v.to_le_bytes())
}

fn u32l(v: u64) -> u32 {
    u32::try_from(v & 0xffff_ffff).unwrap_or(0)
}

fn s32(v: u64) -> i32 {
    d32(u32l(v))
}

fn gpr(r: Register) -> usize {
    r.number().wrapping_sub(Register::EAX.number()) & 7
}

fn is_saved(r: Register) -> bool {
    matches!(
        r,
        Register::EBX | Register::ESI | Register::EDI | Register::EBP
    )
}

fn writes_reg(i: &Instruction) -> Option<Register> {
    if matches!(
        i.mnemonic(),
        Mnemonic::Cmp | Mnemonic::Test | Mnemonic::Push
    ) || i.op_count() == 0
        || i.op0_kind() != OpKind::Register
    {
        return None;
    }
    Some(i.op0_register().full_register32())
}

fn gz_expect(s: &Side, i: usize, din: &[Option<i32>]) -> Option<i32> {
    let cmp = &s.dec.get(i.checked_add(1)?)?.insn;
    let chk = &s.dec.get(i.checked_add(2)?)?.insn;
    let checks = cmp.mnemonic() == Mnemonic::Cmp
        && cmp.op0_kind() == OpKind::Register
        && cmp.op1_kind() == OpKind::Register
        && cmp.op1_register() == Register::ESP
        && chk.mnemonic() == Mnemonic::Call;
    let r = cmp.op0_register();
    if !checks || !matches!(r, Register::ESI | Register::EDI | Register::EBX) {
        return None;
    }
    let mut info = iced_x86::InstructionInfoFactory::new();
    for j in (0..i).rev().take(64) {
        let x = &s.dec.get(j)?.insn;
        if x.mnemonic() == Mnemonic::Mov
            && x.op0_kind() == OpKind::Register
            && x.op0_register() == r
            && x.op1_kind() == OpKind::Register
            && x.op1_register() == Register::ESP
        {
            return din.get(j).copied().flatten();
        }
        let writes = info.info(x).used_registers().iter().any(|u| {
            u.register().full_register32() == r
                && matches!(
                    u.access(),
                    iced_x86::OpAccess::Write
                        | iced_x86::OpAccess::CondWrite
                        | iced_x86::OpAccess::ReadWrite
                        | iced_x86::OpAccess::ReadCondWrite
                )
        });
        let straight = matches!(
            x.flow_control(),
            FlowControl::Next | FlowControl::Call | FlowControl::IndirectCall
        );
        if writes || !straight {
            return None;
        }
    }
    None
}

fn cleanup_follows(s: &Side, i: usize) -> bool {
    for d in s.dec.iter().skip(i.saturating_add(1)).take(8) {
        let x = &d.insn;
        let op0_esp = x.op_count() > 0
            && x.op0_kind() == OpKind::Register
            && x.op0_register().full_register32() == Register::ESP;
        if x.mnemonic() == Mnemonic::Add && op0_esp && x.op1_kind() != OpKind::Register {
            return true;
        }
        let stack = matches!(
            x.mnemonic(),
            Mnemonic::Push
                | Mnemonic::Pop
                | Mnemonic::Pushad
                | Mnemonic::Popad
                | Mnemonic::Pushfd
                | Mnemonic::Popfd
                | Mnemonic::Enter
                | Mnemonic::Leave
        );
        if stack || op0_esp || x.flow_control() != FlowControl::Next {
            return false;
        }
    }
    false
}

#[must_use]
pub fn callee_pop(text: &[u8], text_rva: u32, rva: u32) -> Option<u32> {
    let off = usize::try_from(rva.checked_sub(text_rva)?).ok()?;
    let bytes = text.get(off..off.saturating_add(POP_WINDOW).min(text.len()))?;
    if bytes.starts_with(&[0x51, 0x3d, 0x00, 0x10, 0x00, 0x00]) {
        return Some(CHKSTK);
    }
    let mut d = Decoder::with_ip(32, bytes, 0, DecoderOptions::NONE);
    let mut insn = Instruction::default();
    while d.can_decode() {
        d.decode_out(&mut insn);
        if insn.is_invalid() {
            return None;
        }
        if insn.flow_control() == FlowControl::Return {
            return Some(if insn.op_count() > 0 {
                u32::from(insn.immediate16())
            } else {
                0
            });
        }
    }
    None
}

fn undecorate(n: &str) -> &str {
    let n = n.strip_prefix('_').unwrap_or(n);
    match n.rsplit_once('@') {
        Some((h, t)) if !t.is_empty() && t.chars().all(|c| c.is_ascii_digit()) => h,
        _ => n,
    }
}

#[must_use]
pub fn call_key(s: &Side, ix: usize) -> Option<String> {
    let k = s
        .anchors
        .iter()
        .find(|a| a.ix == ix && a.key.starts_with("C:"))?
        .key
        .as_str();
    Some(match k.strip_prefix("C:I:") {
        Some(n) => format!("C:I:{}", undecorate(n)),
        None => k.to_string(),
    })
}

fn data_keys(s: &Side, ix: usize) -> Vec<String> {
    let mut v: Vec<String> = s
        .anchors
        .iter()
        .filter(|a| a.ix == ix && !a.key.starts_with("C:") && a.key != "RET" && a.key != "SW")
        .map(|a| match a.key.strip_prefix("D:I:") {
            Some(n) => format!("D:I:{}", undecorate(n)),
            None => a.key.clone(),
        })
        .collect();
    v.sort();
    v
}

#[allow(clippy::too_many_lines)]
#[must_use]
pub fn depths(s: &Side, pop_of: &dyn Fn(usize) -> Option<u32>) -> (Vec<Option<i32>>, Option<i32>) {
    let n = s.dec.len();
    let by_ip: HashMap<u32, usize> = s.dec.iter().enumerate().map(|(i, d)| (d.ip, i)).collect();
    let mut din: Vec<Option<i32>> = vec![None; n];
    if n == 0 {
        return (din, None);
    }
    if let Some(x) = din.first_mut() {
        *x = Some(0);
    }
    let mut ebp: Option<i32> = None;
    let mut ebp_other = false;
    for _pass in 0..2 {
        let mut written = [false; 8];
        let mut pending: i32 = 0;
        let mut fallback: Option<i32> = None;
        for i in 0..n {
            let Some(dec) = s.dec.get(i) else {
                continue;
            };
            let Some(d) = din.get(i).copied().flatten().or(fallback) else {
                continue;
            };
            if let Some(slot) = din.get_mut(i) {
                *slot = Some(d);
            }
            let insn = &dec.insn;
            let mut out = Some(d);
            let op0_esp = insn.op_count() > 0
                && insn.op0_kind() == OpKind::Register
                && insn.op0_register() == Register::ESP;
            match insn.mnemonic() {
                Mnemonic::Push => {
                    let r = (insn.op0_kind() == OpKind::Register)
                        .then(|| insn.op0_register().full_register32());
                    let save = r.is_some_and(|r| {
                        is_saved(r) && !written.get(gpr(r)).copied().unwrap_or(false)
                    });
                    if !save {
                        pending = pending.saturating_add(4);
                    }
                    out = Some(d.saturating_sub(4));
                }
                Mnemonic::Pop => {
                    out = Some(d.saturating_add(4));
                    pending = pending.saturating_sub(4).max(0);
                }
                Mnemonic::Pushf => out = Some(d.saturating_sub(2)),
                Mnemonic::Pushfd => out = Some(d.saturating_sub(4)),
                Mnemonic::Popf => out = Some(d.saturating_add(2)),
                Mnemonic::Popfd => out = Some(d.saturating_add(4)),
                Mnemonic::Mov
                    if insn.memory_segment() == Register::FS
                        && insn.op0_kind() == OpKind::Memory =>
                {
                    pending = 0;
                }
                Mnemonic::Sub if op0_esp && insn.op1_kind() != OpKind::Register => {
                    out = Some(d.saturating_sub(s32(insn.immediate(1))));
                }
                Mnemonic::Add if op0_esp && insn.op1_kind() != OpKind::Register => {
                    out = Some(d.saturating_add(s32(insn.immediate(1))));
                    pending = 0;
                }
                Mnemonic::Mov
                    if op0_esp
                        && insn.op1_kind() == OpKind::Register
                        && insn.op1_register() == Register::EBP =>
                {
                    out = ebp;
                }
                Mnemonic::Mov
                    if insn.op0_kind() == OpKind::Register
                        && insn.op0_register() == Register::EBP
                        && insn.op1_kind() == OpKind::Register
                        && insn.op1_register() == Register::ESP =>
                {
                    if ebp.is_some_and(|e| e != d) {
                        ebp_other = true;
                    }
                    ebp = Some(d);
                }
                Mnemonic::Leave => out = ebp.map(|e| e.saturating_add(4)),
                Mnemonic::Lea if op0_esp && insn.memory_base() == Register::EBP => {
                    out = ebp.map(|e| e.saturating_add(d32(insn.memory_displacement32())));
                }
                Mnemonic::Call => {
                    let next_add = s.dec.get(i.saturating_add(1)).is_some_and(|x| {
                        x.insn.mnemonic() == Mnemonic::Add
                            && x.insn.op0_kind() == OpKind::Register
                            && x.insn.op0_register() == Register::ESP
                    });
                    let probe = (i.saturating_sub(4)..i).rev().find_map(|k| {
                        let x = &s.dec.get(k)?.insn;
                        (x.mnemonic() == Mnemonic::Mov
                            && x.op0_kind() == OpKind::Register
                            && x.op0_register() == Register::EAX
                            && x.op1_kind() == OpKind::Immediate32)
                            .then(|| s32(x.immediate(1)))
                    });
                    let pop = match pop_of(i) {
                        Some(CHKSTK) => probe.unwrap_or(0).saturating_neg(),
                        _ if next_add => 0,
                        Some(p) => i32::try_from(p).unwrap_or(0),
                        None if cleanup_follows(s, i) => 0,
                        None => match gz_expect(s, i, &din) {
                            Some(e) if e >= d => e.saturating_sub(d),
                            _ => pending,
                        },
                    };
                    out = Some(d.saturating_add(pop));
                    pending = 0;
                }
                _ if op0_esp => out = None,
                _ => {}
            }
            if let Some(r) = writes_reg(insn) {
                if r.is_gpr32()
                    && let Some(w) = written.get_mut(gpr(r))
                {
                    *w = true;
                }
                if r == Register::EBP
                    && !(insn.mnemonic() == Mnemonic::Mov
                        && insn.op1_kind() == OpKind::Register
                        && insn.op1_register() == Register::ESP)
                    && insn.mnemonic() != Mnemonic::Pop
                {
                    ebp_other = true;
                }
            }
            let fc = insn.flow_control();
            if matches!(
                fc,
                FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch
            ) && matches!(insn.op0_kind(), OpKind::NearBranch32 | OpKind::NearBranch16)
                && let Some(&t) = u32::try_from(insn.near_branch_target())
                    .ok()
                    .and_then(|t| by_ip.get(&t))
                && let Some(x) = din.get_mut(t)
                && x.is_none()
            {
                *x = out;
            }
            let falls = !matches!(
                fc,
                FlowControl::UnconditionalBranch
                    | FlowControl::IndirectBranch
                    | FlowControl::Return
            );
            if fc == FlowControl::IndirectBranch {
                fallback = out;
            } else if fc == FlowControl::Return {
                fallback = None;
            }
            if falls
                && let Some(x) = din.get_mut(i.saturating_add(1))
                && x.is_none()
            {
                *x = out;
            }
        }
    }
    (din, if ebp_other { None } else { ebp })
}

#[must_use]
pub fn frame_rel(s: &Side, depth: &[Option<i32>], ebp: Option<i32>, i: usize) -> Option<i32> {
    let insn = &s.dec.get(i)?.insn;
    let disp = d32(insn.memory_displacement32());
    match insn.memory_base() {
        Register::ESP => Some(depth.get(i).copied().flatten()?.saturating_add(disp)),
        Register::EBP => Some(ebp?.saturating_add(disp)),
        _ => None,
    }
}

fn eh_slot(s: &Side, depth: &[Option<i32>]) -> Option<i32> {
    (0..s.dec.len().min(6)).find_map(|i| {
        let x = &s.dec.get(i)?.insn;
        (x.mnemonic() == Mnemonic::Push
            && matches!(x.op0_kind(), OpKind::Immediate8to32 | OpKind::Immediate32)
            && x.immediate(0) & 0xffff_ffff == 0xffff_ffff)
            .then(|| depth.get(i).copied().flatten().map(|d| d.saturating_sub(4)))
            .flatten()
    })
}

fn stored_params(s: &Side, depth: &[Option<i32>], ebp: Option<i32>) -> BTreeSet<i32> {
    (0..s.dec.len())
        .filter(|&i| {
            let Some(x) = s.dec.get(i).map(|d| &d.insn) else {
                return false;
            };
            x.op_count() > 0
                && x.op0_kind() == OpKind::Memory
                && !matches!(
                    x.mnemonic(),
                    Mnemonic::Cmp | Mnemonic::Test | Mnemonic::Push
                )
        })
        .filter_map(|i| frame_rel(s, depth, ebp, i))
        .filter(|r| *r >= 4)
        .collect()
}

fn masked(s: &Side, at: usize, len: usize) -> bool {
    len > 0 && (at..at.saturating_add(len)).any(|k| s.masked.get(k).copied().unwrap_or(false))
}

fn fields(s: &Side, i: usize) -> (usize, usize, usize, usize) {
    let Some(d) = s.dec.get(i) else {
        return (0, 0, 0, 0);
    };
    let lo = d.ip as usize;
    let b = s
        .bytes
        .get(lo..lo.saturating_add(d.insn.len()))
        .unwrap_or(&[]);
    let mut dec = Decoder::with_ip(32, b, 0, DecoderOptions::NONE);
    let insn = dec.decode();
    let c = dec.get_constant_offsets(&insn);
    (
        lo.saturating_add(c.displacement_offset()),
        c.displacement_size(),
        lo.saturating_add(c.immediate_offset()),
        c.immediate_size(),
    )
}

fn values(s: &Side, depth: &[Option<i32>], ebp: Option<i32>, i: usize) -> Vec<Val> {
    let Some(d) = s.dec.get(i) else {
        return Vec::new();
    };
    let insn = &d.insn;
    let (da, dl, ia, il) = fields(s, i);
    let mut out = Vec::new();
    for op in 0..insn.op_count() {
        match insn.op_kind(op) {
            OpKind::Memory => {
                if let Some(r) = frame_rel(s, depth, ebp, i) {
                    out.push(Val::Frame(r));
                } else if insn.memory_base() != Register::None && !masked(s, da, dl) {
                    out.push(Val::Field(d32(insn.memory_displacement32())));
                }
            }
            OpKind::Immediate8
            | OpKind::Immediate8to32
            | OpKind::Immediate16
            | OpKind::Immediate32
            | OpKind::Immediate8to16
                if !masked(s, ia, il) =>
            {
                out.push(Val::Imm(u32l(insn.immediate(op))));
            }
            _ => {}
        }
    }
    out.extend(call_key(s, i).map(Val::Key));
    out.extend(data_keys(s, i).into_iter().map(Val::Key));
    out
}

#[must_use]
pub fn text(s: &Side, i: usize) -> String {
    let mut f = MasmFormatter::new();
    let mut out = String::new();
    if let Some(d) = s.dec.get(i) {
        f.format(&d.insn, &mut out);
    }
    out
}

struct Cand {
    seg: usize,
    row: usize,
    i: usize,
    j: usize,
    class: &'static str,
    base: f64,
    image: Val,
    ours: Val,
}

pub type Depths = (Vec<Option<i32>>, Option<i32>);

#[must_use]
pub fn frame_depths(
    a: &Analysis,
    pop_at: &dyn Fn(u32) -> Option<u32>,
) -> (HashMap<usize, usize>, Depths, Depths) {
    let pop_of = |s: &Side, ix: usize| {
        call_key(s, ix)
            .and_then(|k| {
                k.strip_prefix("C:")
                    .and_then(|h| u32::from_str_radix(h, 16).ok())
            })
            .and_then(pop_at)
    };
    let mut paired: HashMap<usize, usize> = HashMap::new();
    for seg in &a.segs {
        for r in align_segment(&a.img, &a.ours, seg.ia, seg.oa) {
            if let (Some(i), Some(j), Pairing::Same | Pairing::Form) = (r.img, r.ours, r.pairing) {
                paired.insert(j, i);
            }
        }
    }
    let (idep, iebp) = depths(&a.img, &|ix| pop_of(&a.img, ix));
    let (odep, oebp) = depths(&a.ours, &|ix| {
        if call_key(&a.ours, ix).is_some() {
            return pop_of(&a.ours, ix);
        }
        paired.get(&ix).and_then(|&i| pop_of(&a.img, i))
    });
    (paired, (idep, iebp), (odep, oebp))
}

pub type Frame<'a> = (&'a [Option<i32>], Option<i32>);

#[must_use]
pub fn find(a: &Analysis, pop_at: &dyn Fn(u32) -> Option<u32>) -> (Vec<Hit>, Tally) {
    let (_, (idep, iebp), (odep, oebp)) = frame_depths(a, pop_at);
    find_sides(&a.img, &a.ours, &a.segs, (&idep, iebp), (&odep, oebp))
}

#[must_use]
pub fn depth_lines(a: &Analysis, pop_at: &dyn Fn(u32) -> Option<u32>) -> Vec<String> {
    let (paired, (idep, _), (odep, _)) = frame_depths(a, pop_at);
    let mut last: Option<i32> = None;
    let mut rows: Vec<(usize, usize)> = paired.iter().map(|(j, i)| (*i, *j)).collect();
    rows.sort_unstable();
    let mut out = Vec::new();
    for (i, j) in rows {
        let (Some(Some(p)), Some(Some(q))) = (idep.get(i), odep.get(j)) else {
            continue;
        };
        if last != Some(q.saturating_sub(*p)) {
            out.push(format!(
                "depth {:#x} img {p} ours {q}  | {} | {}",
                a.start
                    .saturating_add(a.img.dec.get(i).map_or(0, |d| u64::from(d.ip))),
                text(&a.img, i),
                text(&a.ours, j)
            ));
            last = Some(q.saturating_sub(*p));
        }
    }
    out
}

#[allow(clippy::too_many_lines)]
#[must_use]
pub fn find_sides(
    img: &Side,
    ours: &Side,
    segs: &[Seg],
    (idep, iebp): Frame<'_>,
    (odep, oebp): Frame<'_>,
) -> (Vec<Hit>, Tally) {
    let mut tally = Tally::default();
    let mut cands: Vec<Cand> = Vec::new();
    let (iw, ow) = (
        stored_params(img, idep, iebp),
        stored_params(ours, odep, oebp),
    );
    let (ieh, oeh) = (eh_slot(img, idep), eh_slot(ours, odep));
    let mut votes: BTreeMap<i32, BTreeMap<i32, usize>> = BTreeMap::new();
    let mut rows_of: Vec<Vec<Pairing>> = Vec::new();
    for (n, seg) in segs.iter().enumerate() {
        let rows = align_segment(img, ours, seg.ia, seg.oa);
        rows_of.push(rows.iter().map(|r| r.pairing).collect());
        for (ri, r) in rows.iter().enumerate() {
            if !matches!(r.pairing, Pairing::Same | Pairing::Form) {
                continue;
            }
            let (Some(i), Some(j)) = (r.img, r.ours) else {
                continue;
            };
            let (Some(di), Some(dj)) = (img.dec.get(i), ours.dec.get(j)) else {
                continue;
            };
            let (x, y) = (&di.insn, &dj.insn);
            if x.op_count() != y.op_count() || x.code() != y.code() {
                continue;
            }
            tally.pairs += 1;
            let mut push = |class, base, image, ours_v| {
                cands.push(Cand {
                    seg: n,
                    row: ri,
                    i,
                    j,
                    class,
                    base,
                    image,
                    ours: ours_v,
                });
            };
            if let (Some(p), Some(q)) = (call_key(img, i), call_key(ours, j))
                && p != q
            {
                push("CALL", 0.9, Val::Key(p), Val::Key(q));
            }
            let (p, q) = (data_keys(img, i), data_keys(ours, j));
            if !p.is_empty() && p.len() == q.len() && p != q {
                for (u, v) in p.iter().zip(&q) {
                    if u == v || u.get(..2) != v.get(..2) {
                        continue;
                    }
                    let hexv = |k: &str| {
                        k.strip_prefix("D:")
                            .and_then(|h| u32::from_str_radix(h, 16).ok())
                    };
                    let (class, base) = match (hexv(u), hexv(v)) {
                        (Some(a), Some(b)) if a.abs_diff(b) < 0x400 => {
                            let indexed = x.memory_index() != Register::None;
                            ("GLOBAL+", if indexed { 0.4 } else { 0.6 })
                        }
                        (Some(_), Some(_)) => ("GLOBAL", 0.8),
                        _ => ("STRING", 0.8),
                    };
                    push(class, base, Val::Key(u.clone()), Val::Key(v.clone()));
                }
            }
            let (ida, idl, iia, iil) = fields(img, i);
            let (oda, odl, oia, oil) = fields(ours, j);
            for op in 0..x.op_count() {
                match (x.op_kind(op), y.op_kind(op)) {
                    (OpKind::Memory, OpKind::Memory) => {
                        let (fi, fo) = (
                            frame_rel(img, idep, iebp, i),
                            frame_rel(ours, odep, oebp, j),
                        );
                        match (fi, fo) {
                            (Some(p), Some(q)) => {
                                let ip = p >= 4 && !iw.contains(&p);
                                let oq = q >= 4 && !ow.contains(&q);
                                let (il, ol) = (!ip && p != 0, !oq && q != 0);
                                if il && ol {
                                    *votes.entry(q).or_default().entry(p).or_default() += 1;
                                }
                                if p == q {
                                    continue;
                                }
                                if ip && q >= 4 {
                                    push("PARAM", 0.9, Val::Frame(p), Val::Frame(q));
                                } else if ip || oq {
                                    push("PARAM/LOCAL", 0.5, Val::Frame(p), Val::Frame(q));
                                } else if il && ol {
                                    push("LOCAL", 0.3, Val::Frame(p), Val::Frame(q));
                                }
                            }
                            (None, None)
                                if x.memory_base() != Register::None
                                    && !matches!(x.memory_base(), Register::ESP)
                                    && !matches!(y.memory_base(), Register::ESP)
                                    && !masked(img, ida, idl)
                                    && !masked(ours, oda, odl) =>
                            {
                                let (p, q) = (
                                    d32(x.memory_displacement32()),
                                    d32(y.memory_displacement32()),
                                );
                                if p != q {
                                    push("FIELD", 0.5, Val::Field(p), Val::Field(q));
                                }
                            }
                            _ => {}
                        }
                    }
                    (
                        OpKind::Immediate8
                        | OpKind::Immediate8to32
                        | OpKind::Immediate16
                        | OpKind::Immediate32
                        | OpKind::Immediate8to16,
                        _,
                    ) => {
                        let esp_op =
                            x.op0_kind() == OpKind::Register && x.op0_register() == Register::ESP;
                        let eh_state = x.op0_kind() == OpKind::Memory
                            && ieh.is_some()
                            && frame_rel(img, idep, iebp, i) == ieh
                            && frame_rel(ours, odep, oebp, j) == oeh;
                        if esp_op || eh_state || masked(img, iia, iil) || masked(ours, oia, oil) {
                            continue;
                        }
                        let (p, q) = (u32l(x.immediate(op)), u32l(y.immediate(op)));
                        if p == q {
                            continue;
                        }
                        let (class, base) = match x.mnemonic() {
                            Mnemonic::Ret => ("RETN", 0.9),
                            Mnemonic::Push => ("IMM", 0.7),
                            _ => ("IMM", 0.5),
                        };
                        push(class, base, Val::Imm(p), Val::Imm(q));
                    }
                    _ => {}
                }
            }
        }
    }
    let fwd: BTreeMap<i32, (i32, usize)> = votes
        .iter()
        .filter_map(|(q, m)| {
            m.iter()
                .max_by_key(|(p, c)| (**c, std::cmp::Reverse(**p)))
                .map(|(p, c)| (*q, (*p, *c)))
        })
        .collect();
    let mut back: BTreeMap<i32, BTreeMap<i32, usize>> = BTreeMap::new();
    for (q, m) in &votes {
        for (p, c) in m {
            *back.entry(*p).or_default().entry(*q).or_default() += c;
        }
    }
    let back_major = |p: i32| {
        back.get(&p)
            .and_then(|m| m.iter().max_by_key(|(_, c)| **c).map(|(q, _)| *q))
    };
    let ivals = |i: usize| values(img, idep, iebp, i);
    let ovals = |j: usize| values(ours, odep, oebp, j);
    let near = |c: usize, len: usize| {
        c.saturating_sub(WINDOW)..c.saturating_add(WINDOW).saturating_add(1).min(len)
    };
    let mut field_deltas: HashMap<i64, usize> = HashMap::new();
    for c in &cands {
        if let (Val::Field(p), Val::Field(q)) = (&c.image, &c.ours) {
            *field_deltas
                .entry(i64::from(*q) - i64::from(*p))
                .or_default() += 1;
        }
    }
    let mut hits = Vec::new();
    for c in cands {
        let mut cert = c.base;
        let mut note = "-";
        if c.class == "LOCAL" {
            let (Val::Frame(p), Val::Frame(q)) = (&c.image, &c.ours) else {
                continue;
            };
            let Some(&(maj, cnt)) = fwd.get(q) else {
                continue;
            };
            if maj == *p || cnt < 2 || back_major(*p).is_none_or(|b| b == *q) {
                continue;
            }
            let reorder = near(c.j, ours.dec.len()).any(|k| {
                ovals(k)
                    .iter()
                    .any(|v| matches!(v, Val::Frame(s) if fwd.get(s).is_some_and(|m| m.0 == *p)))
            });
            if reorder {
                tally.reorders += 1;
                continue;
            }
        } else {
            let reorder = near(c.j, ours.dec.len()).any(|k| ovals(k).contains(&c.image));
            if reorder {
                tally.reorders += 1;
                continue;
            }
            let both = near(c.i, img.dec.len()).any(|k| k != c.i && ivals(k).contains(&c.ours));
            if both {
                cert *= 0.6;
                note = "both";
            }
            if let (Val::Field(p), Val::Field(q)) = (&c.image, &c.ours)
                && field_deltas
                    .get(&(i64::from(*q) - i64::from(*p)))
                    .is_some_and(|n| *n >= REBASE)
            {
                cert *= 0.3;
                note = "rebase?";
            }
        }
        let rows = rows_of.get(c.seg).map_or(&[][..], Vec::as_slice);
        let lo = c.row.saturating_sub(3);
        let hi = c.row.saturating_add(4).min(rows.len());
        let same = (lo..hi)
            .filter(|k| *k != c.row && rows.get(*k) == Some(&Pairing::Same))
            .count();
        cert *= 0.5 + 0.5 * f64::from(u32::try_from(same).unwrap_or(0)) / 6.0;
        hits.push(Hit {
            seg: c.seg,
            img_ix: c.i,
            ours_ix: c.j,
            class: c.class,
            cert,
            image: c.image,
            ours: c.ours,
            note,
        });
    }
    (hits, tally)
}

fn signed(v: i32) -> String {
    let sign = if v < 0 { '-' } else { '+' };
    format!("{sign}{:#x}", v.unsigned_abs())
}

#[must_use]
pub fn show(v: &Val) -> String {
    match v {
        Val::Frame(r) if *r >= 4 => format!("param+{r:#x}"),
        Val::Frame(r) => format!("local{}", signed(*r)),
        Val::Field(d) => format!("field{}", signed(*d)),
        Val::Imm(x) => format!("#{x:#x}"),
        Val::Key(k) => k.replace(['\t', '\n', '\r'], " "),
    }
}

pub const HEADER: &str =
    "cert\tclass\trva\tname\tseg\timage_va\tours_off\timage_op\tours_op\tnote\timage\tours";

#[must_use]
pub fn line(a: &Analysis, name: &str, h: &Hit) -> String {
    let ip = |s: &Side, i: usize| s.dec.get(i).map_or(0, |d| u64::from(d.ip));
    format!(
        "{:.2}\t{}\t{:#08x}\t{}\t{}\t{:#x}\t+{:#x}\t{}\t{}\t{}\t{}\t{}",
        h.cert,
        h.class,
        a.rva,
        name,
        h.seg,
        a.start.saturating_add(ip(&a.img, h.img_ix)),
        ip(&a.ours, h.ours_ix),
        show(&h.image),
        show(&h.ours),
        h.note,
        text(&a.img, h.img_ix),
        text(&a.ours, h.ours_ix)
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_diff::segments::{Verdict, test_side};

    fn seg(img: &Side, ours: &Side) -> Seg {
        Seg {
            img: (0, img.bytes.len()),
            ours: (0, ours.bytes.len()),
            verdict: Verdict::Shape,
            diff: 1,
            edit: 1,
            ia: (0, img.dec.len()),
            oa: (0, ours.dec.len()),
            at: String::new(),
            oversize: false,
        }
    }

    fn run_pair(img: &[u8], ours: &[u8]) -> (Vec<Hit>, Tally) {
        let (i, o) = (test_side(img), test_side(ours));
        let none = |_: usize| None;
        let (di, ei) = depths(&i, &none);
        let (dj, ej) = depths(&o, &none);
        find_sides(&i, &o, &[seg(&i, &o)], (&di, ei), (&dj, ej))
    }

    #[test]
    fn a_different_parameter_slot_is_a_param_hit() {
        let pre = [0x83, 0xec, 0x10, 0x56, 0x57, 0x8b, 0xf0, 0x8b, 0xf9];
        let tail = [0x85, 0xc9, 0x5f, 0x5e, 0x83, 0xc4, 0x10, 0xc3];
        let img = [&pre[..], &[0x8b, 0x4c, 0x24, 0x20], &tail].concat();
        let ours = [&pre[..], &[0x8b, 0x4c, 0x24, 0x2c], &tail].concat();
        let (hits, _) = run_pair(&img, &ours);
        assert_eq!(hits.len(), 1, "{hits:?}");
        assert_eq!(hits.first().map(|h| h.class), Some("PARAM"));
        assert_eq!(
            hits.first().map(|h| (h.image.clone(), h.ours.clone())),
            Some((Val::Frame(8), Val::Frame(0x14)))
        );
    }

    #[test]
    fn a_push_depth_shift_is_not_a_hit_and_crosswise_loads_are_a_reorder() {
        let img = [0x6a, 0x01, 0x8b, 0x4c, 0x24, 0x08, 0x59, 0xc3];
        let ours = [
            0x6a, 0x01, 0x6a, 0x02, 0x8b, 0x4c, 0x24, 0x0c, 0x59, 0x59, 0xc3,
        ];
        let (hits, _) = run_pair(&img, &ours);
        assert!(hits.iter().all(|h| h.class != "PARAM"), "{hits:?}");
        let img = [0x8b, 0x44, 0x24, 0x04, 0x8b, 0x4c, 0x24, 0x08, 0xc3];
        let ours = [0x8b, 0x44, 0x24, 0x08, 0x8b, 0x4c, 0x24, 0x04, 0xc3];
        let (hits, t) = run_pair(&img, &ours);
        assert!(hits.is_empty(), "{hits:?}");
        assert_eq!(t.reorders, 2);
    }

    #[test]
    fn a_pushed_constant_is_an_imm_hit_and_a_rename_is_not() {
        let (hits, _) = run_pair(
            &[0x6a, 0x05, 0x8b, 0xc1, 0x59, 0xc3],
            &[0x6a, 0x06, 0x8b, 0xd1, 0x59, 0xc3],
        );
        assert_eq!(hits.len(), 1, "{hits:?}");
        assert_eq!(hits.first().map(|h| h.class), Some("IMM"));
    }

    #[test]
    fn depth_follows_branch_edges_callee_pops_and_cleanups() {
        let b = [
            0x6a, 0x01, 0xe8, 0, 0, 0, 0, 0x85, 0xc0, 0x74, 0x04, 0x6a, 0x02, 0xeb, 0x02, 0x6a,
            0x03, 0x8b, 0x44, 0x24, 0x08, 0xc3,
        ];
        let (d, _) = depths(&test_side(&b), &|i| (i == 1).then_some(4));
        assert_eq!(d.get(7).copied().flatten(), Some(-4), "{d:?}");
        let b = [
            0x6a, 0x01, 0xe8, 0, 0, 0, 0, 0x8b, 0xcb, 0x83, 0xc4, 0x04, 0x8b, 0x44, 0x24, 0x04,
            0xc3,
        ];
        let (d, _) = depths(&test_side(&b), &|_| None);
        assert_eq!(d.get(2).copied().flatten(), Some(-4), "{d:?}");
        assert_eq!(d.get(4).copied().flatten(), Some(0), "{d:?}");
        let b = [
            0x6a, 0x01, 0xe8, 0, 0, 0, 0, 0x6a, 0x02, 0x83, 0xc4, 0x04, 0x8b, 0x44, 0x24, 0x04,
            0xc3,
        ];
        let (d, _) = depths(&test_side(&b), &|_| None);
        assert_eq!(d.get(2).copied().flatten(), Some(0), "{d:?}");
    }

    #[test]
    fn the_gz_check_says_where_a_call_leaves_esp() {
        let b = [
            0x8B, 0xF4, 0x8B, 0x45, 0x08, 0x50, 0x8B, 0x4D, 0xFC, 0x8B, 0x11, 0x8B, 0xFC, 0x8B,
            0x4D, 0xFC, 0xFF, 0x92, 0xCC, 0, 0, 0, 0x3B, 0xFC, 0xE8, 0, 0, 0, 0, 0x50, 0x8B, 0x45,
            0xFC, 0x8B, 0x10, 0x8B, 0x4D, 0xFC, 0xFF, 0x52, 0x44, 0x3B, 0xF4, 0xE8, 0, 0, 0, 0,
            0xC3,
        ];
        let (d, _) = depths(&test_side(&b), &|i| (i == 9 || i == 16).then_some(0));
        assert_eq!(d.get(8).copied().flatten(), Some(-4), "{d:?}");
        assert_eq!(d.get(15).copied().flatten(), Some(0), "{d:?}");
        let b = [
            0x6A, 0xFF, 0x68, 0x00, 0x10, 0x40, 0x00, 0x64, 0xA1, 0, 0, 0, 0, 0x50, 0x64, 0x89,
            0x25, 0, 0, 0, 0, 0x6A, 0x01, 0xFF, 0x10, 0xC3,
        ];
        let (d, _) = depths(&test_side(&b), &|_| None);
        assert_eq!(d.get(7).copied().flatten(), Some(-12), "{d:?}");
        let (d, _) = depths(
            &test_side(&[0x66, 0x9C, 0x57, 0x5F, 0x66, 0x9D, 0xC3]),
            &|_| None,
        );
        assert_eq!(
            (
                d.get(1).copied().flatten(),
                d.get(3).copied().flatten(),
                d.get(4).copied().flatten()
            ),
            (Some(-2), Some(-2), Some(0))
        );
    }

    #[test]
    fn a_stack_probe_reads_as_the_probe_and_a_callee_pop_is_read_off_its_ret() {
        let text = [0x51, 0x3d, 0x00, 0x10, 0x00, 0x00, 0x90, 0xc2, 0x08, 0x00];
        assert_eq!(callee_pop(&text, 0x1000, 0x1000), Some(CHKSTK));
        assert_eq!(callee_pop(&text, 0x1000, 0x1006), Some(8));
        assert_eq!(callee_pop(&text, 0x1000, 0x0fff), None);
    }
}
