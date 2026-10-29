use crate::segments::{AlignRow, Analysis, Pairing, Side, Verdict, align_segment, off};
use iced_x86::{
    ConditionCode, FlowControl, Instruction, InstructionInfoFactory, Mnemonic, OpAccess, OpKind,
    Register,
};
use std::collections::{BTreeMap, HashMap, HashSet};

pub const HEADER: &str =
    "rva\tname\tseg\timage_va\tverdict\tseg_edit\tclass\tbytes\trows\tfirst\tupstream";

pub const CLASSES: [&str; 14] = [
    "frame-slot sort",
    "register priority tie",
    "rotation pick",
    "region split",
    "block layout",
    "branch test form",
    "scheduler (pure order)",
    "scheduler (RA-coupled)",
    "operand rank/hash",
    "CSE/forward-substitution",
    "RA reload/copy-fold",
    "inliner budget",
    "C1 counter",
    "other",
];

const RA_FORMS: [&str; 3] = ["rotation pick", "register priority tie", "C1 counter"];

const WEB_REACH: usize = 24;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Charge {
    pub seg: usize,
    pub img_va: u64,
    pub verdict: Verdict,
    pub seg_edit: usize,
    pub class: &'static str,
    pub bytes: usize,
    pub rows: usize,
    pub first: String,
    pub upstream: String,
}

#[must_use]
pub fn upstream(diffs: &[(Register, Register, u64)]) -> String {
    let mut n: BTreeMap<(Register, Register), (usize, u64)> = BTreeMap::new();
    for &(o, i, va) in diffs {
        let e = n.entry((o, i)).or_insert((0, va));
        e.0 += 1;
        e.1 = e.1.min(va);
    }
    let Some((&(o, i), &(k, va))) = n
        .iter()
        .max_by(|a, b| a.1.0.cmp(&b.1.0).then(b.1.1.cmp(&a.1.1)))
    else {
        return "no register difference (a move with no twin)".into();
    };
    format!(
        "ours {} -> image {} @{va:#x} ({k} of {})",
        format!("{o:?}").to_lowercase(),
        format!("{i:?}").to_lowercase(),
        diffs.len()
    )
}

#[must_use]
pub fn traced_upstream(text: &str) -> HashMap<u64, String> {
    let mut best: HashMap<u64, (usize, String)> = HashMap::new();
    for l in text.lines() {
        let f: Vec<&str> = l.split('\t').collect();
        if f.first() != Some(&"#regs") || f.len() < 9 {
            continue;
        }
        let Some(va) = f
            .get(2)
            .and_then(|v| u64::from_str_radix(v.trim_start_matches("0x"), 16).ok())
        else {
            continue;
        };
        let ours: usize = f.get(5).and_then(|v| v.parse().ok()).unwrap_or(0);
        let text = format!(
            "{}: {}",
            f.get(4).copied().unwrap_or(""),
            f.get(9).copied().unwrap_or("")
        );
        let e = best.entry(va).or_insert((0, String::new()));
        if e.1.is_empty() || ours > e.0 {
            *e = (ours, text);
        }
    }
    best.into_iter().map(|(k, v)| (k, v.1)).collect()
}

fn reg_diffs(img: &Instruction, ours: &Instruction) -> Vec<(Register, Register)> {
    let mut v = Vec::new();
    for op in 0..img.op_count().min(ours.op_count()) {
        match (img.op_kind(op), ours.op_kind(op)) {
            (OpKind::Register, OpKind::Register) => {
                v.push((wide(ours.op_register(op)), wide(img.op_register(op))));
            }
            (OpKind::Memory, OpKind::Memory) => {
                v.push((wide(ours.memory_base()), wide(img.memory_base())));
                v.push((wide(ours.memory_index()), wide(img.memory_index())));
            }
            _ => {}
        }
    }
    v.retain(|(a, b)| a != b && a.is_gpr() && b.is_gpr());
    v
}

fn scratch(r: Register) -> bool {
    matches!(r, Register::EAX | Register::ECX | Register::EDX)
}

fn wide(r: Register) -> Register {
    if r != Register::None && r.is_gpr() {
        r.full_register32()
    } else {
        r
    }
}

fn is_branch(i: &Instruction) -> bool {
    matches!(
        i.flow_control(),
        FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch
    )
}

fn frame_mem(i: &Instruction, op: u32) -> bool {
    i.op_kind(op) == OpKind::Memory
        && matches!(i.memory_base(), Register::ESP | Register::EBP)
        && i.memory_index() == Register::None
}

fn form_class(a: &Instruction, b: &Instruction) -> &'static str {
    if a.mnemonic() != b.mnemonic() {
        return if is_branch(a) && is_branch(b) {
            branch_pair_class(a, b)
        } else {
            "other"
        };
    }
    let mut regs: Vec<(Register, Register)> = Vec::new();
    let mut disp = false;
    for op in 0..a.op_count().min(b.op_count()) {
        match (a.op_kind(op), b.op_kind(op)) {
            (OpKind::Register, OpKind::Register) => {
                regs.push((wide(a.op_register(op)), wide(b.op_register(op))));
            }
            (OpKind::Memory, OpKind::Memory) => {
                regs.push((wide(a.memory_base()), wide(b.memory_base())));
                regs.push((wide(a.memory_index()), wide(b.memory_index())));
                if a.memory_displacement32() != b.memory_displacement32()
                    && frame_mem(a, op)
                    && frame_mem(b, op)
                {
                    disp = true;
                }
            }
            _ => {}
        }
    }
    let differing: Vec<(Register, Register)> = regs.into_iter().filter(|(x, y)| x != y).collect();
    if !differing.is_empty() {
        let imm_mov = a.mnemonic() == Mnemonic::Mov
            && a.op_count() == 2
            && matches!(a.op_kind(1), OpKind::Immediate32 | OpKind::Immediate8to32);
        if imm_mov && differing.iter().any(|(x, y)| !scratch(*x) || !scratch(*y)) {
            return "C1 counter";
        }
        return if differing.iter().all(|(x, y)| scratch(*x) && scratch(*y)) {
            "rotation pick"
        } else {
            "register priority tie"
        };
    }
    if disp {
        return "frame-slot sort";
    }
    "other"
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum MemAs {
    Mem,
    FrameReg,
    AnyReg,
}

fn loose_op(i: &Instruction, op: u32, mem: MemAs) -> String {
    match i.op_kind(op) {
        OpKind::Register => format!("r{}", i.op_register(op).size()),
        OpKind::Memory => {
            if mem == MemAs::AnyReg || (mem == MemAs::FrameReg && frame_mem(i, op)) {
                return format!("r{}", i.memory_size().size());
            }
            let absolute = i.memory_base() == Register::None;
            format!(
                "m{}:{}:{}:{}",
                i.memory_size().size(),
                i.memory_index() != Register::None,
                i.memory_index_scale(),
                if absolute {
                    String::from("*")
                } else {
                    format!("{:x}", i.memory_displacement32())
                }
            )
        }
        OpKind::Immediate8
        | OpKind::Immediate16
        | OpKind::Immediate32
        | OpKind::Immediate8to16
        | OpKind::Immediate8to32 => format!("i{:x}", i.immediate(op) & 0xffff_ffff),
        k => format!("{k:?}"),
    }
}

fn loose_key(i: &Instruction, mem: MemAs) -> String {
    let mut k = format!("{:?}", i.mnemonic());
    for op in 0..i.op_count() {
        k.push(' ');
        k.push_str(&loose_op(i, op, mem));
    }
    k
}

fn mem_key(i: &Instruction) -> Option<String> {
    (0..i.op_count())
        .find(|&op| i.op_kind(op) == OpKind::Memory && !frame_mem(i, op))
        .map(|op| loose_op(i, op, MemAs::Mem))
}

fn zero_reg(i: &Instruction) -> bool {
    matches!(i.mnemonic(), Mnemonic::Xor | Mnemonic::Sub)
        && i.op_count() == 2
        && i.op_kind(0) == OpKind::Register
        && i.op_kind(1) == OpKind::Register
        && i.op0_register() == i.op1_register()
}

fn has_imm_zero(i: &Instruction) -> bool {
    (0..i.op_count()).any(|op| {
        matches!(
            i.op_kind(op),
            OpKind::Immediate8
                | OpKind::Immediate16
                | OpKind::Immediate32
                | OpKind::Immediate8to16
                | OpKind::Immediate8to32
        ) && i.immediate(op) == 0
    })
}

fn frame_operand(i: &Instruction) -> bool {
    i.mnemonic() != Mnemonic::Mov && (0..i.op_count()).any(|op| frame_mem(i, op))
}

fn negated(c: ConditionCode) -> ConditionCode {
    use ConditionCode as C;
    match c {
        C::o => C::no,
        C::no => C::o,
        C::b => C::ae,
        C::ae => C::b,
        C::e => C::ne,
        C::ne => C::e,
        C::be => C::a,
        C::a => C::be,
        C::s => C::ns,
        C::ns => C::s,
        C::p => C::np,
        C::np => C::p,
        C::l => C::ge,
        C::ge => C::l,
        C::le => C::g,
        C::g => C::le,
        C::None => C::None,
    }
}

fn branch_pair_class(a: &Instruction, b: &Instruction) -> &'static str {
    let (ca, cb) = (a.condition_code(), b.condition_code());
    if ca == ConditionCode::None || cb == ConditionCode::None || negated(ca) == cb {
        "block layout"
    } else if ca == cb {
        "other"
    } else {
        "branch test form"
    }
}

fn tests_in_another_form(x: &Instruction, others: &[Instruction]) -> bool {
    others
        .iter()
        .any(|y| branch_pair_class(x, y) == "branch test form")
}

fn loose_twin(x: &Instruction, y: &Instruction, mem: MemAs, y_moves: bool) -> bool {
    !y_moves
        && loose_key(x, mem) == loose_key(y, mem)
        && match mem {
            MemAs::Mem => true,
            MemAs::FrameReg => frame_operand(x) || frame_operand(y),
            MemAs::AnyReg => {
                mem_key(x).is_some() != mem_key(y).is_some()
                    && lone_class(x) != "region split"
                    && lone_class(y) != "region split"
            }
        }
}

fn lone_class(i: &Instruction) -> &'static str {
    if is_branch(i) {
        return "block layout";
    }
    if i.flow_control() == FlowControl::Call {
        return "inliner budget";
    }
    if i.mnemonic() == Mnemonic::Mov && i.op_count() == 2 {
        let (k0, k1) = (i.op_kind(0), i.op_kind(1));
        if k0 == OpKind::Register && k1 == OpKind::Register {
            return "region split";
        }
        let spill = k0 == OpKind::Memory && frame_mem(i, 0) && k1 == OpKind::Register;
        let reload = k1 == OpKind::Memory && frame_mem(i, 1) && k0 == OpKind::Register;
        if spill || reload {
            return "region split";
        }
    }
    "CSE/forward-substitution"
}

type PerClass = BTreeMap<&'static str, (usize, usize, String)>;

fn scheduler_class(per: &PerClass, twinless: bool) -> &'static str {
    if twinless || per.keys().any(|c| RA_FORMS.contains(c)) {
        "scheduler (RA-coupled)"
    } else {
        "scheduler (pure order)"
    }
}

fn charge(per: &mut PerClass, c: &'static str, b: usize, at: String) {
    if b == 0 {
        return;
    }
    let e = per.entry(c).or_insert((0, 0, at));
    e.0 += b;
    e.1 += 1;
}

fn len(s: &Side, i: usize) -> usize {
    off(s, i + 1).saturating_sub(off(s, i))
}

fn access(f: &mut InstructionInfoFactory, x: &Instruction, r: Register) -> (bool, bool) {
    let (mut rd, mut wr) = (false, false);
    for u in f.info(x).used_registers() {
        if wide(u.register()) != r {
            continue;
        }
        match u.access() {
            OpAccess::Read | OpAccess::CondRead => rd = true,
            OpAccess::Write | OpAccess::CondWrite => wr = true,
            OpAccess::ReadWrite | OpAccess::ReadCondWrite => (rd, wr) = (true, true),
            _ => {}
        }
    }
    (rd, wr)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Holds {
    Reg,
    Operand,
}

fn holds(a: &Instruction, b: &Instruction, v: Register) -> Option<Holds> {
    let kind = |k: OpKind| match k {
        OpKind::Register => Holds::Reg,
        _ => Holds::Operand,
    };
    for op in 0..a.op_count() {
        let bk = (op < b.op_count()).then(|| b.op_kind(op));
        match a.op_kind(op) {
            OpKind::Register if wide(a.op_register(op)) == v => return bk.map(kind),
            OpKind::Memory => {
                let other = if wide(a.memory_base()) == v {
                    b.memory_base()
                } else if wide(a.memory_index()) == v {
                    b.memory_index()
                } else {
                    continue;
                };
                return Some(if bk == Some(OpKind::Memory) && other != Register::None {
                    Holds::Reg
                } else {
                    Holds::Operand
                });
            }
            _ => {}
        }
    }
    (a.mnemonic() == b.mnemonic()).then_some(Holds::Reg)
}

fn web_on_other_side(
    f: &mut InstructionInfoFactory,
    (me, them): (&Side, &Side),
    partner: &dyn Fn(usize) -> Option<usize>,
    i: usize,
    v: Register,
    spill: bool,
) -> Option<Holds> {
    let at = |j: usize| -> Option<Holds> {
        let o = partner(j)?;
        holds(&me.dec.get(j)?.insn, &them.dec.get(o)?.insn, v)
    };
    if spill {
        for j in (i.saturating_sub(WEB_REACH)..i).rev() {
            let Some(d) = me.dec.get(j) else { break };
            if access(f, &d.insn, v).1 {
                if let Some(h) = at(j) {
                    return Some(h);
                }
                break;
            }
        }
    }
    for j in i + 1..(i + 1 + WEB_REACH).min(me.dec.len()) {
        let Some(d) = me.dec.get(j) else { break };
        let (rd, wr) = access(f, &d.insn, v);
        if rd && let Some(h) = at(j) {
            return Some(h);
        }
        if wr {
            break;
        }
    }
    None
}

type Lone = (u8, usize, usize);

struct Ctx<'a> {
    a: &'a Analysis,
    pairs: &'a [HashMap<usize, usize>; 2],
    replaced: &'a [HashMap<usize, usize>; 2],
    lone_keys: &'a [[HashSet<String>; 2]],
    seg: usize,
    lone: &'a [Lone],
    avail: &'a HashMap<(u8, String), usize>,
}

fn slot(x: &Instruction) -> Option<(bool, u32)> {
    if x.mnemonic() != Mnemonic::Mov || x.op_count() != 2 {
        return None;
    }
    let spill = x.op_kind(0) == OpKind::Memory && frame_mem(x, 0);
    let reload = x.op_kind(1) == OpKind::Memory && frame_mem(x, 1);
    (spill || reload).then(|| (spill, x.memory_displacement32()))
}

fn split_class(
    f: &mut InstructionInfoFactory,
    c: &Ctx<'_>,
    s: u8,
    i: usize,
    x: &Instruction,
) -> &'static str {
    let a = c.a;
    let side = usize::from(s);
    let other = 1 - side;
    let (me, them) = if s == 0 {
        (&a.img, &a.ours)
    } else {
        (&a.ours, &a.img)
    };
    let copy = x.op_kind(0) == OpKind::Register && x.op_kind(1) == OpKind::Register;
    let spill = x.op_kind(0) == OpKind::Memory;
    let moved = if copy {
        format!("R:{}", me.reg_keys.get(i).cloned().unwrap_or_default())
    } else {
        format!("X:{x}")
    };
    for m in [c.seg.wrapping_sub(1), c.seg.saturating_add(1)] {
        if c.lone_keys
            .get(m)
            .and_then(|k| k.get(other))
            .is_some_and(|k| k.contains(&moved))
        {
            return "scheduler (RA-coupled)";
        }
    }
    if !copy {
        let y = c
            .replaced
            .get(side)
            .and_then(|p| p.get(&i))
            .and_then(|&o| them.dec.get(o));
        if let Some(y) = y.map(|d| d.insn) {
            let same_dir = if spill {
                y.op_kind(0) == OpKind::Memory && frame_mem(&y, 0)
            } else {
                y.op_count() == 2 && y.op_kind(1) == OpKind::Memory && frame_mem(&y, 1)
            };
            if y.mnemonic() == x.mnemonic() && same_dir {
                return "other";
            }
        }
        let (lo, hi) =
            c.a.segs
                .get(c.seg)
                .map_or((0, 0), |g| if s == 0 { g.oa } else { g.ia });
        let mine = slot(x);
        let mispaired = mine.is_some()
            && (lo..hi).any(|o| {
                them.dec.get(o).and_then(|d| slot(&d.insn)) == mine
                    && c.pairs
                        .get(other)
                        .and_then(|p| p.get(&o))
                        .and_then(|&q| me.dec.get(q))
                        .is_some_and(|d| slot(&d.insn) != mine)
            });
        if mispaired {
            return "scheduler (RA-coupled)";
        }
    }
    if copy {
        let src = wide(x.op_register(1));
        let lo = a
            .segs
            .get(c.seg)
            .map_or(0, |g| if s == 0 { g.ia.0 } else { g.oa.0 });
        let def = (lo..i)
            .rev()
            .find(|&j| me.dec.get(j).is_some_and(|d| access(f, &d.insn, src).1));
        if let Some(j) = def {
            let is_lone = c.lone.iter().any(|&(ls, li, _)| ls == s && li == j);
            let k = me.reg_keys.get(j).cloned().unwrap_or_default();
            if is_lone && c.avail.get(&(1 - s, k)).copied().unwrap_or(0) > 0 {
                return "CSE/forward-substitution";
            }
        }
    }
    let v = wide(x.op_register(u32::from(spill)));
    let partner = |j: usize| -> Option<usize> {
        if let Some(&o) = c.pairs.get(side).and_then(|p| p.get(&j)) {
            return Some(o);
        }
        if !c.lone.iter().any(|&(ls, li, _)| ls == s && li == j) {
            return None;
        }
        let k = me.reg_keys.get(j)?;
        c.lone
            .iter()
            .find(|&&(ls, li, _)| ls == 1 - s && them.reg_keys.get(li) == Some(k))
            .map(|&(_, li, _)| li)
    };
    match web_on_other_side(f, (me, them), &partner, i, v, spill) {
        Some(Holds::Reg) => "region split",
        _ => "CSE/forward-substitution",
    }
}

struct Pass {
    rows: Vec<Vec<AlignRow>>,
    pairs: [HashMap<usize, usize>; 2],
    replaced: [HashMap<usize, usize>; 2],
    lone_keys: Vec<[HashSet<String>; 2]>,
}

fn same_key(a: &Analysis, i: usize, j: usize) -> bool {
    a.img.reg_keys.get(i).is_some() && a.img.reg_keys.get(i) == a.ours.reg_keys.get(j)
}

fn first_pass(a: &Analysis) -> Pass {
    let rows: Vec<Vec<AlignRow>> = a
        .segs
        .iter()
        .map(|seg| align_segment(&a.img, &a.ours, seg.ia, seg.oa))
        .collect();
    let mut pairs: [HashMap<usize, usize>; 2] = [HashMap::new(), HashMap::new()];
    let mut replaced: [HashMap<usize, usize>; 2] = [HashMap::new(), HashMap::new()];
    let mut lone_keys: Vec<[HashSet<String>; 2]> = Vec::new();
    for seg_rows in &rows {
        let mut keys: [HashSet<String>; 2] = [HashSet::new(), HashSet::new()];
        for r in seg_rows {
            let same = matches!((r.img, r.ours), (Some(i), Some(j)) if same_key(a, i, j));
            if let (Some(i), Some(j)) = (r.img, r.ours) {
                let m = if r.pairing != Pairing::Replaced || same {
                    &mut pairs
                } else {
                    &mut replaced
                };
                let [to_ours, to_img] = m;
                to_ours.insert(i, j);
                to_img.insert(j, i);
            }
            if r.pairing == Pairing::Replaced && !same || r.img.is_none() || r.ours.is_none() {
                for (s, x, side) in [(0usize, r.img, &a.img), (1, r.ours, &a.ours)] {
                    let Some(ks) = keys.get_mut(s) else { continue };
                    if let Some(k) = x.and_then(|x| side.reg_keys.get(x)) {
                        ks.insert(format!("R:{k}"));
                    }
                    if let Some(d) = x.and_then(|x| side.dec.get(x)) {
                        ks.insert(format!("X:{}", d.insn));
                    }
                }
            }
        }
        lone_keys.push(keys);
    }
    Pass {
        rows,
        pairs,
        replaced,
        lone_keys,
    }
}

#[must_use]
pub fn find(a: &Analysis, swap_offs: &HashSet<usize>) -> Vec<Charge> {
    let mut out = Vec::new();
    let pass = first_pass(a);
    let mut info = InstructionInfoFactory::new();
    for (n, seg) in a.segs.iter().enumerate() {
        if seg.verdict == Verdict::Exact || seg.edit == 0 {
            continue;
        }
        let Some(rows) = pass.rows.get(n) else {
            continue;
        };
        out.extend(segment_charges(a, &pass, &mut info, swap_offs, n, rows));
    }
    out
}

fn va_of(a: &Analysis, s: &Side, i: usize) -> String {
    format!("{:#x}", a.start.saturating_add(off(s, i) as u64))
}

fn segment_charges(
    a: &Analysis,
    pass: &Pass,
    info: &mut InstructionInfoFactory,
    swap_offs: &HashSet<usize>,
    n: usize,
    rows: &[AlignRow],
) -> Vec<Charge> {
    let Some(seg) = a.segs.get(n) else {
        return Vec::new();
    };
    let mut lone: Vec<Lone> = Vec::new();
    let mut per: PerClass = BTreeMap::new();
    let mut diffs: Vec<(Register, Register, u64)> = Vec::new();
    for r in rows {
        if r.cost == 0 {
            continue;
        }
        match (r.pairing, r.img, r.ours) {
            (Pairing::Form, Some(i), Some(j)) => {
                let (Some(di), Some(dj)) = (a.img.dec.get(i), a.ours.dec.get(j)) else {
                    continue;
                };
                let c = if swap_offs.contains(&(di.ip as usize)) {
                    "operand rank/hash"
                } else {
                    form_class(&di.insn, &dj.insn)
                };
                let va = a.start.saturating_add(u64::from(di.ip));
                if RA_FORMS.contains(&c) {
                    diffs.extend(
                        reg_diffs(&di.insn, &dj.insn)
                            .into_iter()
                            .map(|(o, i)| (o, i, va)),
                    );
                }
                charge(&mut per, c, r.cost, format!("{va:#x}"));
            }
            (_, Some(i), Some(j)) if same_key(a, i, j) => {
                let (Some(di), Some(dj)) = (a.img.dec.get(i), a.ours.dec.get(j)) else {
                    continue;
                };
                let va = a.start.saturating_add(u64::from(di.ip));
                diffs.extend(
                    reg_diffs(&di.insn, &dj.insn)
                        .into_iter()
                        .map(|(o, i)| (o, i, va)),
                );
                charge(
                    &mut per,
                    form_class(&di.insn, &dj.insn),
                    r.cost,
                    format!("{va:#x}"),
                );
            }
            (_, i, j) => {
                let ib = i.map_or(0, |i| len(&a.img, i));
                if let Some(i) = i {
                    lone.push((0, i, ib.min(r.cost)));
                }
                if let Some(j) = j {
                    lone.push((1, j, r.cost.saturating_sub(ib)));
                }
            }
        }
    }
    let key = |s: u8, i: usize| {
        if s == 0 {
            a.img.reg_keys.get(i).cloned().unwrap_or_default()
        } else {
            a.ours.reg_keys.get(i).cloned().unwrap_or_default()
        }
    };
    let insn = |s: u8, i: usize| {
        if s == 0 {
            a.img.dec.get(i).map(|d| d.insn)
        } else {
            a.ours.dec.get(i).map(|d| d.insn)
        }
    };
    let mut avail: HashMap<(u8, String), usize> = HashMap::new();
    for &(s, i, _) in &lone {
        *avail.entry((s, key(s, i))).or_default() += 1;
    }
    let mut unpaired_br: HashMap<(u8, Mnemonic), usize> = HashMap::new();
    for &(s, i, _) in &lone {
        if let Some(x) = insn(s, i).filter(is_branch) {
            *unpaired_br.entry((s, x.mnemonic())).or_default() += 1;
        }
    }
    let recoded = |s: u8, i: usize| {
        insn(s, i).is_some_and(|x| {
            unpaired_br
                .get(&(1 - s, x.mnemonic()))
                .copied()
                .unwrap_or(0)
                > 0
        })
    };
    let lone_br = |s: u8| -> Vec<Instruction> {
        lone.iter()
            .filter(|&&(t, _, _)| t == s)
            .filter_map(|&(t, i, _)| insn(t, i).filter(is_branch))
            .collect()
    };
    let (br_img, br_ours) = (lone_br(0), lone_br(1));
    let test_form = |s: u8, i: usize| {
        insn(s, i).is_some_and(|x| {
            is_branch(&x) && tests_in_another_form(&x, if s == 0 { &br_ours } else { &br_img })
        })
    };
    let branchy = lone.iter().any(|&(s, i, _)| {
        insn(s, i).is_some_and(|x| is_branch(&x)) && !recoded(s, i) && !test_form(s, i)
    });
    let exact = |s: u8, i: usize| {
        if s == 0 {
            a.img.dec.get(i).map(|d| d.key.clone()).unwrap_or_default()
        } else {
            a.ours.dec.get(i).map(|d| d.key.clone()).unwrap_or_default()
        }
    };
    let mut avail_exact: HashMap<(u8, String), usize> = HashMap::new();
    for &(s, i, _) in &lone {
        *avail_exact.entry((s, exact(s, i))).or_default() += 1;
    }
    let mut settled: HashSet<(u8, usize)> = HashSet::new();
    let pooled = lone
        .iter()
        .any(|&(s, i, _)| insn(s, i).is_some_and(|x| zero_reg(&x)));
    for &(s, i, _) in &lone {
        let Some(x) = insn(s, i) else { continue };
        if s != 0 || is_branch(&x) || avail.get(&(1, key(0, i))).copied().unwrap_or(0) > 0 {
            continue;
        }
        for mem in [MemAs::Mem, MemAs::FrameReg, MemAs::AnyReg] {
            let twin = lone.iter().find(|&&(t, j, _)| {
                t == 1
                    && !settled.contains(&(1, j))
                    && insn(1, j).is_some_and(|y| {
                        loose_twin(
                            &x,
                            &y,
                            mem,
                            avail.get(&(0, key(1, j))).copied().unwrap_or(0) > 0,
                        )
                    })
            });
            if let Some(&(_, j, _)) = twin {
                settled.insert((0, i));
                settled.insert((1, j));
                let (bi, bj) = lone
                    .iter()
                    .filter(|&&(t, k, _)| (t, k) == (0, i) || (t, k) == (1, j))
                    .fold((0, 0), |acc, &(t, _, b)| {
                        if t == 0 {
                            (acc.0 + b, acc.1)
                        } else {
                            (acc.0, acc.1 + b)
                        }
                    });
                let c = match mem {
                    MemAs::FrameReg => "RA reload/copy-fold",
                    MemAs::AnyReg => "CSE/forward-substitution",
                    MemAs::Mem => insn(1, j).map_or("other", |y| form_class(&x, &y)),
                };
                charge(&mut per, c, bi + bj, va_of(a, &a.img, i));
                break;
            }
        }
    }
    let mut ra = false;
    let mut moves: Vec<(usize, String)> = Vec::new();
    for &(s, i, b) in &lone {
        if settled.contains(&(s, i)) {
            continue;
        }
        let Some(x) = insn(s, i) else { continue };
        let k = key(s, i);
        let other = avail.get(&(1 - s, k.clone())).copied().unwrap_or(0);
        let at = if s == 0 {
            va_of(a, &a.img, i)
        } else {
            format!("+{:#x}", off(&a.ours, i))
        };
        let c = if is_branch(&x) {
            if recoded(s, i) {
                "other"
            } else if test_form(s, i) {
                "branch test form"
            } else {
                "block layout"
            }
        } else if other > 0 {
            if branchy {
                "block layout"
            } else {
                if avail_exact.get(&(1 - s, exact(s, i))).copied().unwrap_or(0) == 0 {
                    ra = true;
                    if s == 0 {
                        let twin = lone
                            .iter()
                            .find(|&&(t, j, _)| t == 1 && key(1, j) == k)
                            .and_then(|&(_, j, _)| insn(1, j));
                        if let Some(y) = twin {
                            let va = a.start.saturating_add(off(&a.img, i) as u64);
                            diffs.extend(reg_diffs(&x, &y).into_iter().map(|(o, r)| (o, r, va)));
                        }
                    }
                }
                moves.push((b, at));
                continue;
            }
        } else {
            match lone_class(&x) {
                "CSE/forward-substitution"
                    if frame_operand(&x)
                        || (pooled
                            && (zero_reg(&x)
                                || has_imm_zero(&x)
                                || (x.mnemonic() == Mnemonic::Push
                                    && x.op_kind(0) == OpKind::Register))) =>
                {
                    "RA reload/copy-fold"
                }
                "region split" => {
                    let ctx = Ctx {
                        a,
                        pairs: &pass.pairs,
                        replaced: &pass.replaced,
                        lone_keys: &pass.lone_keys,
                        seg: n,
                        lone: &lone,
                        avail: &avail,
                    };
                    split_class(info, &ctx, s, i, &x)
                }
                c => c,
            }
        };
        charge(&mut per, c, b, at);
    }
    let sched = scheduler_class(&per, ra);
    for (b, at) in moves {
        charge(&mut per, sched, b, at);
    }
    let up = if sched == "scheduler (RA-coupled)" {
        upstream(&diffs)
    } else {
        String::new()
    };
    per.into_iter()
        .map(|(class, (bytes, n_rows, first))| Charge {
            upstream: if class == "scheduler (RA-coupled)" {
                up.clone()
            } else {
                String::new()
            },
            seg: n,
            img_va: a.start.saturating_add(off(&a.img, seg.ia.0) as u64),
            verdict: seg.verdict,
            seg_edit: seg.edit,
            class,
            bytes,
            rows: n_rows,
            first,
        })
        .collect()
}

#[must_use]
pub fn line(rva: u32, name: &str, c: &Charge) -> String {
    format!(
        "{rva:#08x}\t{name}\t{}\t{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
        c.seg,
        c.img_va,
        c.verdict.tag(),
        c.seg_edit,
        c.class,
        c.bytes,
        c.rows,
        c.first,
        c.upstream
    )
}

#[must_use]
pub fn summary(charges: &[(u32, Charge)]) -> Vec<(&'static str, usize, usize, usize)> {
    let mut out: Vec<(&'static str, usize, usize, usize)> = CLASSES
        .iter()
        .map(|&c| {
            let hit: Vec<&(u32, Charge)> = charges.iter().filter(|(_, x)| x.class == c).collect();
            let fns: HashSet<u32> = hit.iter().map(|(r, _)| *r).collect();
            (
                c,
                hit.iter().map(|(_, x)| x.bytes).sum(),
                fns.len(),
                hit.len(),
            )
        })
        .collect();
    out.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(b.0)));
    out
}

pub fn apply_upstream(charges: &mut [(u32, Charge)], traced: &HashMap<u64, String>) {
    for (_, c) in charges {
        if c.upstream.is_empty() {
            continue;
        }
        if let Some(t) = traced.get(&c.img_va) {
            c.upstream = format!("{t} | {}", c.upstream);
        }
    }
}

#[must_use]
pub fn tsv(charges: &[(u32, Charge)], name: &dyn Fn(u32) -> String) -> String {
    let mut t = format!("{HEADER}\n");
    for (rva, c) in charges {
        t.push_str(&line(*rva, &name(*rva), c));
        t.push('\n');
    }
    t
}

#[must_use]
pub fn summary_lines(charges: &[(u32, Charge)], written_to: &str) -> Vec<String> {
    let total: usize = charges.iter().map(|(_, c)| c.bytes).sum();
    let mut out = vec![format!(
        "decisions: {total} edit byte(s) in {} row(s) written to {written_to}",
        charges.len()
    )];
    for (class, bytes, fns, segs) in summary(charges) {
        out.push(format!(
            "decisions:\t{class}\t{bytes}\t{fns} fn(s)\t{segs} seg(s)"
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::segments::test_side as side;

    fn one(bytes: &[u8]) -> Instruction {
        let mut d = iced_x86::Decoder::with_ip(32, bytes, 0, iced_x86::DecoderOptions::NONE);
        d.decode()
    }

    #[test]
    fn an_ra_coupled_segment_names_its_upstream_register_decision() {
        let (img, ours) = (one(&[0x8b, 0x46, 0x08]), one(&[0x8b, 0x4e, 0x08]));
        let d = reg_diffs(&img, &ours);
        assert_eq!(d, vec![(Register::ECX, Register::EAX)]);
        let mut diffs: Vec<(Register, Register, u64)> =
            d.into_iter().map(|(o, i)| (o, i, 0x41_1960)).collect();
        diffs.push((Register::ECX, Register::EAX, 0x41_1950));
        diffs.push((Register::EDX, Register::ESI, 0x41_1940));
        assert_eq!(upstream(&diffs), "ours ecx -> image eax @0x411950 (2 of 3)");
        assert!(upstream(&[]).starts_with("no register difference"));
        let t = "seg 4 ...\n#regs\t4\t0x462cd2\t8\tregister colour (partial)\t7\t6\t6\t1\tours eax -> image ecx, def n12 line 755\n\
                 #regs\t4\t0x462cd2\t9\tpass-made tuple\t2\t2\t2\t0\tfirst out of order n3\n";
        let u = traced_upstream(t);
        assert_eq!(
            u.get(&0x46_2cd2).map(String::as_str),
            Some("register colour (partial): ours eax -> image ecx, def n12 line 755")
        );
    }

    #[test]
    fn a_form_pair_is_charged_to_the_decision_its_operands_name() {
        assert_eq!(
            form_class(&one(&[0x8b, 0x46, 0x08]), &one(&[0x8b, 0x4e, 0x08])),
            "rotation pick"
        );
        assert_eq!(
            form_class(&one(&[0x8b, 0x30]), &one(&[0x8b, 0x38])),
            "register priority tie"
        );
        assert_eq!(
            form_class(
                &one(&[0x8b, 0x44, 0x24, 0x10]),
                &one(&[0x8b, 0x44, 0x24, 0x14])
            ),
            "frame-slot sort"
        );
        assert_eq!(
            form_class(&one(&[0xbe, 1, 0, 0, 0]), &one(&[0xbf, 1, 0, 0, 0])),
            "C1 counter"
        );
        assert_eq!(
            form_class(&one(&[0x74, 0x05]), &one(&[0x75, 0x05])),
            "block layout"
        );
        assert_eq!(
            form_class(&one(&[0x74, 0x05]), &one(&[0xeb, 0x05])),
            "block layout"
        );
    }

    #[test]
    fn a_lone_jcc_against_a_lone_jcc_of_another_test_is_test_form() {
        let jns = one(&[0x79, 0x05]);
        assert!(tests_in_another_form(&jns, &[one(&[0x7f, 0x05])]));
        assert!(!tests_in_another_form(
            &jns,
            &[one(&[0x78, 0x05]), one(&[0x79, 0x05])]
        ));
        assert!(!tests_in_another_form(&jns, &[]));
    }

    #[test]
    fn only_a_negated_or_unconditional_branch_pair_is_block_layout() {
        assert_eq!(
            form_class(&one(&[0x79, 0x05]), &one(&[0x7f, 0x05])),
            "branch test form"
        );
        assert_eq!(
            form_class(&one(&[0x7d, 0x05]), &one(&[0x7c, 0x05])),
            "block layout"
        );
        assert_eq!(
            branch_pair_class(&one(&[0x7d, 0x05]), &one(&[0x0f, 0x8d, 0, 1, 0, 0])),
            "other"
        );
        for c in [
            ConditionCode::o,
            ConditionCode::be,
            ConditionCode::p,
            ConditionCode::le,
        ] {
            assert_eq!(negated(negated(c)), c);
            assert_ne!(negated(c), c);
        }
    }

    #[test]
    fn a_scheduler_move_is_pure_only_with_exact_twins_and_no_register_pair() {
        let mut per = PerClass::new();
        charge(&mut per, "frame-slot sort", 4, "0x1".into());
        assert_eq!(scheduler_class(&per, false), "scheduler (pure order)");
        assert_eq!(scheduler_class(&per, true), "scheduler (RA-coupled)");
        charge(&mut per, "rotation pick", 2, "0x2".into());
        assert_eq!(scheduler_class(&per, false), "scheduler (RA-coupled)");
        charge(&mut per, "rotation pick", 0, "0x3".into());
        assert_eq!(per.get("rotation pick").map(|e| e.1), Some(1));
    }

    #[test]
    fn a_split_is_charged_only_when_the_web_is_a_register_on_both_sides() {
        let mut f = InstructionInfoFactory::new();
        let (img, ours) = (side(&[0x8b, 0x44, 0x24, 0x34, 0x50]), side(&[0x56]));
        let p = |j: usize| (j == 1).then_some(0);
        assert_eq!(
            web_on_other_side(&mut f, (&img, &ours), &p, 0, Register::EAX, false),
            Some(Holds::Reg)
        );
        let img = side(&[0x8b, 0x1d, 0, 0x10, 0, 0, 0x0f, 0xaf, 0xc3]);
        let ours = side(&[0x0f, 0xaf, 0x05, 0, 0x10, 0, 0]);
        assert_eq!(
            web_on_other_side(&mut f, (&img, &ours), &p, 0, Register::EBX, false),
            Some(Holds::Operand)
        );
        let img = side(&[0x8b, 0xc6, 0xf7, 0xd0, 0x83, 0xe0, 0x03]);
        let ours = side(&[0x83, 0xc8, 0xff, 0x2b, 0xc6, 0x83, 0xe0, 0x03]);
        let p = |j: usize| (j == 2).then_some(2);
        assert_eq!(
            web_on_other_side(&mut f, (&img, &ours), &p, 0, Register::EAX, false),
            None
        );
        let img = side(&[0x83, 0xca, 0x01, 0x89, 0x54, 0x24, 0x30]);
        let ours = side(&[0x83, 0xce, 0x01]);
        let p = |j: usize| (j == 0).then_some(0);
        assert_eq!(
            web_on_other_side(&mut f, (&img, &ours), &p, 1, Register::EDX, true),
            Some(Holds::Reg)
        );
        assert_eq!(slot(&one(&[0x8b, 0x44, 0x24, 0x34])), Some((false, 0x34)));
        assert_eq!(slot(&one(&[0x8b, 0x4c, 0x24, 0x34])), Some((false, 0x34)));
        assert_eq!(slot(&one(&[0x89, 0x44, 0x24, 0x34])), Some((true, 0x34)));
        assert_eq!(slot(&one(&[0x8b, 0xc6])), None);
    }

    #[test]
    fn a_register_choice_is_not_charged_to_cse() {
        let (img, ours) = (
            one(&[0x8b, 0x0d, 0x40, 0x28, 0x4c, 0x00]),
            one(&[0xa1, 0, 0, 0, 0]),
        );
        assert_eq!(loose_key(&img, MemAs::Mem), loose_key(&ours, MemAs::Mem));
        assert_eq!(form_class(&img, &ours), "rotation pick");
        assert_eq!(
            loose_key(&one(&[0x25, 0, 0x10, 0, 0]), MemAs::Mem),
            loose_key(&one(&[0x81, 0xe1, 0, 0x10, 0, 0]), MemAs::Mem)
        );
        let (home, reg) = (one(&[0x85, 0x84, 0x24, 0x90, 0, 0, 0]), one(&[0x85, 0xc2]));
        assert!(frame_operand(&home) && !frame_operand(&reg));
        assert_ne!(loose_key(&home, MemAs::Mem), loose_key(&reg, MemAs::Mem));
        assert_eq!(
            loose_key(&home, MemAs::FrameReg),
            loose_key(&reg, MemAs::FrameReg)
        );
        assert!(zero_reg(&one(&[0x33, 0xdb])));
        assert!(!zero_reg(&one(&[0x33, 0xd8])));
        assert!(has_imm_zero(&one(&[0x6a, 0x00])));
        let (fold, held) = (
            one(&[0x0f, 0xaf, 0x0d, 0x44, 0x28, 0x4c, 0x00]),
            one(&[0x0f, 0xaf, 0xcb]),
        );
        assert_ne!(
            loose_key(&fold, MemAs::FrameReg),
            loose_key(&held, MemAs::FrameReg)
        );
        assert_eq!(
            loose_key(&fold, MemAs::AnyReg),
            loose_key(&held, MemAs::AnyReg)
        );
        assert!(loose_twin(&fold, &held, MemAs::AnyReg, false));
        assert!(!loose_twin(&fold, &held, MemAs::AnyReg, true));
        let (reload, copy) = (one(&[0xa1, 0x70, 0x41, 0x83, 0x00]), one(&[0x8b, 0xf0]));
        assert_eq!(
            loose_key(&reload, MemAs::AnyReg),
            loose_key(&copy, MemAs::AnyReg)
        );
        assert!(!loose_twin(&reload, &copy, MemAs::AnyReg, false));
        let load = one(&[0x8b, 0x1d, 0, 0, 0, 0]);
        assert_eq!(mem_key(&load), mem_key(&fold));
        assert_ne!(
            loose_key(&one(&[0x83, 0xf8, 0xff]), MemAs::Mem),
            loose_key(&one(&[0x66, 0x83, 0xf8, 0xff]), MemAs::Mem)
        );
    }

    #[test]
    fn a_lone_instruction_is_charged_by_what_it_does() {
        assert_eq!(lone_class(&one(&[0x8b, 0xf0])), "region split");
        assert_eq!(lone_class(&one(&[0x89, 0x44, 0x24, 0x10])), "region split");
        assert_eq!(lone_class(&one(&[0xe8, 0, 0, 0, 0])), "inliner budget");
        assert_eq!(lone_class(&one(&[0x33, 0xc0])), "CSE/forward-substitution");
        assert_eq!(lone_class(&one(&[0x75, 0x02])), "block layout");
    }

    #[test]
    fn the_summary_lists_every_class_largest_first() {
        let c = |class: &'static str, bytes: usize| Charge {
            seg: 0,
            img_va: 0x1000,
            verdict: Verdict::Shape,
            seg_edit: bytes,
            class,
            bytes,
            rows: 1,
            first: "0x1000".into(),
            upstream: String::new(),
        };
        let all = [
            (1, c("other", 3)),
            (2, c("other", 2)),
            (2, c("block layout", 7)),
        ];
        let lines = summary_lines(&all, "f.tsv");
        assert_eq!(lines.len(), CLASSES.len() + 1);
        assert_eq!(
            lines[0],
            "decisions: 12 edit byte(s) in 3 row(s) written to f.tsv"
        );
        assert_eq!(lines[1], "decisions:\tblock layout\t7\t1 fn(s)\t1 seg(s)");
        assert_eq!(lines[2], "decisions:\tother\t5\t2 fn(s)\t2 seg(s)");
        assert_eq!(lines[3], "decisions:\tC1 counter\t0\t0 fn(s)\t0 seg(s)");
    }
}
