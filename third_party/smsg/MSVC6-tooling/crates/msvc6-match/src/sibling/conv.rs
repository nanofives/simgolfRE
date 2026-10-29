use iced_x86::{
    Decoder, DecoderOptions, FlowControl, Instruction, InstructionInfoFactory, Mnemonic, OpAccess,
    OpKind, Register,
};
use std::collections::{BTreeSet, HashMap, HashSet};

pub const WALK_STEPS: usize = 50_000;

pub const THIS_STEPS: usize = 128;

pub const MAX_TABLE: usize = 1024;

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Convention {
    pub pops: Option<u16>,
    pub this: Option<bool>,
    pub args: u8,
    pub exact: bool,
}

impl std::fmt::Display for Convention {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let pops = self.pops.map_or_else(|| "?".to_string(), |p| p.to_string());
        let this = match self.this {
            Some(true) => "t",
            Some(false) => "n",
            None => "?",
        };
        let exact = if self.exact { "=" } else { "+" };
        write!(f, "r{pops}/{this}/{}{exact}", self.args)
    }
}

fn proven_args(s: &Convention) -> (u16, bool) {
    match s.pops {
        Some(p) if p > 0 => (p / 4, true),
        _ => (u16::from(s.args), s.exact),
    }
}

#[must_use]
pub fn clash(a: &Convention, b: &Convention) -> Option<String> {
    if let (Some(x), Some(y)) = (a.pops, b.pops)
        && x != y
    {
        return Some(format!("ret {x} vs {y}"));
    }
    if let (Some(x), Some(y)) = (a.this, b.this)
        && x != y
    {
        return Some("ecx this vs not".to_string());
    }
    let ((na, xa), (nb, xb)) = (proven_args(a), proven_args(b));
    if (xa && nb > na) || (xb && na > nb) {
        return Some(format!("{na} vs {nb} stack argument(s)"));
    }
    None
}

fn body_end(bytes: &[u8], va: u64) -> u64 {
    va.saturating_add(u64::try_from(bytes.len()).unwrap_or(0))
}

fn insn_at(bytes: &[u8], va: u64, ip: u64) -> Option<Instruction> {
    let off = usize::try_from(ip.checked_sub(va)?).ok()?;
    let b = bytes.get(off..).filter(|b| !b.is_empty())?;
    let i = Decoder::with_ip(32, b, ip, DecoderOptions::NONE).decode();
    (!i.is_invalid() && i.next_ip() <= body_end(bytes, va)).then_some(i)
}

fn inside(bytes: &[u8], va: u64, ip: u64) -> bool {
    ip >= va && ip < body_end(bytes, va)
}

fn jump_table(i: &Instruction, bytes: &[u8], va: u64) -> Option<Vec<u64>> {
    if i.op0_kind() != OpKind::Memory
        || i.memory_base() != Register::None
        || i.memory_index_scale() != 4
    {
        return None;
    }
    let mut off = usize::try_from(u64::from(i.memory_displacement32()).checked_sub(va)?).ok()?;
    let mut out = Vec::new();
    while let Some(w) = bytes.get(off..off.saturating_add(4)) {
        let t = u64::from(u32::from_le_bytes(<[u8; 4]>::try_from(w).ok()?));
        if !inside(bytes, va, t) || out.len() >= MAX_TABLE {
            break;
        }
        out.push(t);
        off = off.saturating_add(4);
    }
    (!out.is_empty()).then_some(out)
}

fn successors(i: &Instruction, bytes: &[u8], va: u64) -> Option<Vec<u64>> {
    let near = |i: &Instruction| {
        matches!(i.op0_kind(), OpKind::NearBranch32 | OpKind::NearBranch16)
            && inside(bytes, va, i.near_branch_target())
    };
    Some(match i.flow_control() {
        FlowControl::Return | FlowControl::Interrupt | FlowControl::Exception => Vec::new(),
        FlowControl::UnconditionalBranch => {
            if near(i) {
                vec![i.near_branch_target()]
            } else {
                Vec::new()
            }
        }
        FlowControl::ConditionalBranch => {
            let mut v = vec![i.next_ip()];
            if near(i) {
                v.push(i.near_branch_target());
            }
            v
        }
        FlowControl::IndirectBranch => return jump_table(i, bytes, va),
        _ => vec![i.next_ip()],
    })
}

fn disp(i: &Instruction) -> i32 {
    i32::from_le_bytes(i.memory_displacement32().to_le_bytes())
}

fn imm(i: &Instruction, op: u32) -> Option<i32> {
    match i.op_kind(op) {
        OpKind::Immediate8to32 => Some(i.immediate8to32()),
        OpKind::Immediate32 => Some(i32::from_le_bytes(i.immediate32().to_le_bytes())),
        OpKind::Immediate8 => Some(i32::from(i.immediate8())),
        OpKind::Immediate16 => Some(i32::from(i.immediate16())),
        _ => None,
    }
}

fn arg_slot(i: &Instruction, depth: Option<i32>, frame: Option<i32>) -> Result<Option<u8>, ()> {
    if !(0..i.op_count()).any(|o| i.op_kind(o) == OpKind::Memory) {
        return Ok(None);
    }
    let below = match i.memory_base() {
        Register::ESP => depth.ok_or(())?,
        Register::EBP => match frame {
            Some(c) => c,
            None => return Ok(None),
        },
        _ => return Ok(None),
    };
    let off = disp(i).saturating_sub(below);
    if off < 4 {
        return Ok(None);
    }
    Ok(Some(u8::try_from((off - 4) / 4).unwrap_or(u8::MAX).min(63)))
}

fn step(
    i: &Instruction,
    depth: Option<i32>,
    frame: Option<i32>,
    eax: Option<i32>,
    pops_of: &dyn Fn(u64) -> Option<u16>,
) -> (Option<i32>, Option<i32>) {
    let reg0 =
        |r: Register| i.op_count() > 0 && i.op0_kind() == OpKind::Register && i.op0_register() == r;
    let reg1 =
        |r: Register| i.op_count() > 1 && i.op1_kind() == OpKind::Register && i.op1_register() == r;
    match i.mnemonic() {
        Mnemonic::Call => {
            if i.op0_kind() == OpKind::NearBranch32 {
                if let Some(n) = eax.filter(|n| *n >= 0x1000) {
                    return (depth.map(|d| d.saturating_add(n)), frame);
                }
                let p = pops_of(i.near_branch_target());
                return (
                    depth.zip(p).map(|(d, p)| d.saturating_sub(i32::from(p))),
                    frame,
                );
            }
            return (None, frame);
        }
        Mnemonic::Leave => return (frame.map(|c| c.saturating_sub(4)), None),
        _ => {}
    }
    let mut d = depth.map(|d| d.saturating_sub(i.stack_pointer_increment()));
    let mut f = frame;
    let explicit = !matches!(
        i.mnemonic(),
        Mnemonic::Push | Mnemonic::Pop | Mnemonic::Cmp | Mnemonic::Test
    );
    if reg0(Register::ESP) && explicit {
        d = match (i.mnemonic(), imm(i, 1)) {
            (Mnemonic::Sub, Some(n)) => d.map(|d| d.saturating_add(n)),
            (Mnemonic::Add, Some(n)) => d.map(|d| d.saturating_sub(n)),
            (Mnemonic::Mov, _) if reg1(Register::EBP) => frame,
            (Mnemonic::Lea, _)
                if i.memory_base() == Register::EBP && i.memory_index() == Register::None =>
            {
                frame.map(|c| c.saturating_sub(disp(i)))
            }
            _ => None,
        };
    } else if i.mnemonic() == Mnemonic::Xchg && (reg0(Register::ESP) || reg1(Register::ESP)) {
        d = None;
    }
    let ebp_written = (reg0(Register::EBP) && explicit)
        || (i.mnemonic() == Mnemonic::Pop && reg0(Register::EBP))
        || (i.mnemonic() == Mnemonic::Xchg && reg1(Register::EBP));
    if ebp_written {
        f = if i.mnemonic() == Mnemonic::Mov && reg1(Register::ESP) {
            depth
        } else {
            None
        };
    }
    (d, f)
}

fn self_idiom(i: &Instruction) -> bool {
    matches!(i.mnemonic(), Mnemonic::Xor | Mnemonic::Sub | Mnemonic::Sbb)
        && i.op_count() == 2
        && i.op0_kind() == OpKind::Register
        && i.op1_kind() == OpKind::Register
        && i.op0_register() == i.op1_register()
}

fn ecx_access(i: &Instruction, info: &mut InstructionInfoFactory) -> (bool, bool) {
    if self_idiom(i) && i.op0_register().full_register32() == Register::ECX {
        return (false, true);
    }
    let (mut read, mut write) = (false, false);
    for u in info.info(i).used_registers() {
        if u.register().full_register32() != Register::ECX {
            continue;
        }
        match u.access() {
            OpAccess::Read | OpAccess::CondRead => read = true,
            OpAccess::ReadWrite | OpAccess::ReadCondWrite => {
                read = true;
                write = true;
            }
            OpAccess::Write | OpAccess::CondWrite => write = true,
            _ => {}
        }
    }
    (read, write)
}

fn reads_this(bytes: &[u8], va: u64, info: &mut InstructionInfoFactory) -> Option<bool> {
    let (mut live, mut saved) = (true, false);
    let mut ip = va;
    let mut seen = HashSet::new();
    for _ in 0..THIS_STEPS {
        if !seen.insert(ip) {
            return None;
        }
        let i = insn_at(bytes, va, ip)?;
        let ecx0 = i.op_count() == 1
            && i.op0_kind() == OpKind::Register
            && i.op0_register() == Register::ECX;
        if ecx0 && matches!(i.mnemonic(), Mnemonic::Push | Mnemonic::Pop) {
            if i.mnemonic() == Mnemonic::Push {
                saved = live;
            } else {
                live = saved;
                saved = false;
            }
            ip = i.next_ip();
            continue;
        }
        if !matches!(
            i.flow_control(),
            FlowControl::Next | FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch
        ) {
            return None;
        }
        let (read, write) = ecx_access(&i, info);
        if read && live {
            return Some(true);
        }
        if write {
            live = false;
            if !saved {
                return Some(false);
            }
        }
        ip = if i.flow_control() == FlowControl::UnconditionalBranch {
            let t = i.near_branch_target();
            if i.op0_kind() != OpKind::NearBranch32 || !inside(bytes, va, t) {
                return None;
            }
            t
        } else {
            i.next_ip()
        };
    }
    None
}

type At = (u64, Option<i32>, Option<i32>, Option<i32>);

#[must_use]
pub fn signature(bytes: &[u8], va: u64, pops_of: &dyn Fn(u64) -> Option<u16>) -> Convention {
    let mut info = InstructionInfoFactory::new();
    let this = reads_this(bytes, va, &mut info);
    let mut seen: HashMap<u64, (Option<i32>, Option<i32>)> = HashMap::new();
    let mut work: Vec<At> = vec![(va, Some(0), None, None)];
    let mut rets: BTreeSet<u16> = BTreeSet::new();
    let mut blind: BTreeSet<u64> = BTreeSet::new();
    let (mut args, mut exact, mut steps) = (0u8, true, 0usize);
    while let Some((ip, depth, frame, eax)) = work.pop() {
        steps = steps.saturating_add(1);
        if steps > WALK_STEPS {
            exact = false;
            break;
        }
        match seen.get(&ip) {
            Some(s) if *s == (depth, frame) => continue,
            Some((d0, f0)) if *f0 == frame && d0.is_some() && depth.is_none() => continue,
            Some((d0, f0)) if *f0 == frame && d0.is_none() && depth.is_some() => {}
            Some(_) => {
                exact = false;
                continue;
            }
            None => {}
        }
        seen.insert(ip, (depth, frame));
        let Some(i) = insn_at(bytes, va, ip) else {
            exact = false;
            continue;
        };
        match arg_slot(&i, depth, frame) {
            Ok(Some(s)) => {
                args = args.max(s.saturating_add(1));
                blind.remove(&ip);
            }
            Ok(None) => {
                blind.remove(&ip);
            }
            Err(()) => {
                blind.insert(ip);
            }
        }
        if i.flow_control() == FlowControl::Return {
            rets.insert(if i.op_count() > 0 { i.immediate16() } else { 0 });
            continue;
        }
        let (d, f) = step(&i, depth, frame, eax, pops_of);
        let next_eax = (i.mnemonic() == Mnemonic::Mov
            && i.op0_kind() == OpKind::Register
            && i.op0_register() == Register::EAX)
            .then(|| imm(&i, 1))
            .flatten();
        let Some(next) = successors(&i, bytes, va) else {
            exact = false;
            continue;
        };
        for n in next {
            work.push((n, d, f, next_eax));
        }
    }
    Convention {
        pops: if rets.len() == 1 {
            rets.iter().next().copied()
        } else {
            None
        },
        this,
        args,
        exact: exact && blind.is_empty(),
    }
}
