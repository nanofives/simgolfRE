use super::code::{Code, MIN_STR, str_key};
use super::conv::{Convention, signature};
use iced_x86::{Decoder, DecoderOptions, FlowControl, Instruction, Mnemonic, OpKind, Register};
use std::collections::{BTreeSet, HashMap, HashSet};
use std::hash::{Hash, Hasher};

pub const MIN_BODY: usize = 12;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Fun {
    pub image: String,
    pub rva: u32,
    pub end: u32,
    pub name: String,
    pub file: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Callee {
    Fn(usize),
    Import(String),
    Indirect,
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Widths {
    pub bytes: u16,
    pub words: u16,
}

#[must_use]
pub fn width_clash(a: Widths, b: Widths) -> bool {
    let only_bytes = |w: Widths| w.bytes >= 2 && w.words == 0;
    let only_words = |w: Widths| w.words >= 2 && w.bytes == 0;
    (only_bytes(a) && only_words(b)) || (only_words(a) && only_bytes(b))
}

#[derive(Debug, Default, Clone)]
pub struct Feat {
    pub calls: Vec<Callee>,
    pub strs: Vec<String>,
    pub raw: Vec<String>,
    pub imports: BTreeSet<String>,
    pub mnem: Vec<u16>,
    pub body: u64,
    pub loose: u64,
    pub conv: Convention,
    pub widths: Widths,
}

#[derive(Debug, Default)]
pub struct Side {
    pub funs: Vec<Fun>,
    pub feats: Vec<Feat>,
    pub callers: Vec<BTreeSet<usize>>,
    pub atom: Vec<usize>,
}

impl Side {
    pub fn finish(&mut self) {
        self.callers = vec![BTreeSet::new(); self.funs.len()];
        for (i, f) in self.feats.iter().enumerate() {
            for c in &f.calls {
                if let Callee::Fn(t) = c
                    && *t != i
                    && let Some(s) = self.callers.get_mut(*t)
                {
                    s.insert(i);
                }
            }
        }
    }
}

fn mnemonic(i: &Instruction) -> u16 {
    i.mnemonic() as u16
}

fn register(r: Register) -> u16 {
    r as u16
}

#[must_use]
pub fn body_hash(insns: &[Instruction], code_lo: u64, code_hi: u64, loose: bool) -> u64 {
    let mut h = std::collections::hash_map::DefaultHasher::new();
    for i in insns {
        mnemonic(i).hash(&mut h);
        for op in 0..i.op_count() {
            match i.op_kind(op) {
                OpKind::Register => register(i.op_register(op)).hash(&mut h),
                OpKind::Memory => {
                    register(i.memory_base()).hash(&mut h);
                    register(i.memory_index()).hash(&mut h);
                    i.memory_index_scale().hash(&mut h);
                    let d = u64::from(i.memory_displacement32());
                    if loose || i.memory_base() == Register::None || d >= 0x10000 {
                        0xa5a5u16.hash(&mut h);
                    } else {
                        d.hash(&mut h);
                    }
                }
                OpKind::NearBranch32 | OpKind::NearBranch16 | OpKind::FarBranch32 => {
                    0xb6b6u16.hash(&mut h);
                }
                _ => {
                    let v = i.try_immediate(op).unwrap_or(0);
                    if loose || v >= 0x10000 || (v >= code_lo && v < code_hi) {
                        0xc7c7u16.hash(&mut h);
                    } else {
                        v.hash(&mut h);
                    }
                }
            }
        }
    }
    h.finish()
}

#[must_use]
pub fn decode(code: &Code<'_>, rva: u32, end: u32) -> Vec<Instruction> {
    let len = usize::try_from(end.saturating_sub(rva)).unwrap_or(0);
    let Some(bytes) = code.bytes(rva, len) else {
        return Vec::new();
    };
    let mut d = Decoder::with_ip(32, bytes, code.va(rva), DecoderOptions::NONE);
    let mut out = Vec::new();
    while d.can_decode() {
        let i = d.decode();
        if i.is_invalid() {
            break;
        }
        out.push(i);
    }
    out
}

fn has_memory(i: &Instruction) -> bool {
    (0..i.op_count()).any(|o| i.op_kind(o) == OpKind::Memory)
}

fn ebp_frame(insns: &[Instruction]) -> bool {
    matches!(
        (insns.first(), insns.get(1)),
        (Some(a), Some(b)) if a.mnemonic() == Mnemonic::Push
            && a.op0_register() == Register::EBP
            && b.mnemonic() == Mnemonic::Mov
            && b.op0_register() == Register::EBP
            && b.op1_register() == Register::ESP
    )
}

struct Walk<'c, 'a> {
    code: &'c Code<'a>,
    me: usize,
    lo: u32,
    hi: u32,
    entry: &'c HashMap<u32, usize>,
    feat: Feat,
    seen: HashSet<String>,
    reg_import: HashMap<Register, String>,
}

impl Walk<'_, '_> {
    fn call(&mut self, i: &Instruction, imp: Option<&String>) {
        if i.op0_kind() == OpKind::NearBranch32 {
            let t = self.code.rva_of(i.near_branch_target()).unwrap_or(0);
            match self.code.resolve(t) {
                Err(n) => {
                    self.feat.imports.insert(n.clone());
                    self.feat.calls.push(Callee::Import(n));
                }
                Ok(r) => self.feat.calls.push(
                    self.entry
                        .get(&r)
                        .map_or(Callee::Indirect, |x| Callee::Fn(*x)),
                ),
            }
        } else if let Some(n) = imp {
            self.feat.calls.push(Callee::Import(n.clone()));
        } else if i.op0_kind() == OpKind::Register {
            let c = self
                .reg_import
                .get(&i.op0_register())
                .map_or(Callee::Indirect, |n| Callee::Import(n.clone()));
            self.feat.calls.push(c);
        } else {
            self.feat.calls.push(Callee::Indirect);
        }
    }

    fn tail(&mut self, i: &Instruction) {
        let t = self.code.rva_of(i.near_branch_target()).unwrap_or(0);
        if t >= self.lo && t < self.hi {
            return;
        }
        match self.code.resolve(t) {
            Err(n) => self.feat.calls.push(Callee::Import(n)),
            Ok(r) => {
                if let Some(x) = self.entry.get(&r).filter(|x| **x != self.me) {
                    self.feat.calls.push(Callee::Fn(*x));
                }
            }
        }
    }

    fn strings(&mut self, i: &Instruction) {
        let mut vals: Vec<u64> = Vec::new();
        for o in 0..i.op_count() {
            match i.op_kind(o) {
                OpKind::Immediate32 | OpKind::Immediate8to32 => {
                    vals.push(i.try_immediate(o).unwrap_or(0))
                }
                OpKind::Memory if i.memory_base() == Register::None => {
                    vals.push(u64::from(i.memory_displacement32()));
                }
                _ => {}
            }
        }
        for v in vals {
            let Some(r) = self.code.rva_of(v) else {
                continue;
            };
            if let Some(s) = self.code.string_at(r) {
                let k = str_key(&s);
                if k.len() >= MIN_STR && self.seen.insert(k.clone()) {
                    self.feat.strs.push(k);
                    self.feat.raw.push(s);
                }
            }
        }
    }
}

#[must_use]
pub fn features(code: &Code<'_>, f: &Fun, me: usize, entry: &HashMap<u32, usize>) -> Feat {
    let insns = decode(code, f.rva, f.end);
    let (code_lo, code_hi) = code.code_bounds();
    let frame = ebp_frame(&insns);
    let mut w = Walk {
        code,
        me,
        lo: f.rva,
        hi: f.end,
        entry,
        feat: Feat::default(),
        seen: HashSet::new(),
        reg_import: HashMap::new(),
    };
    for i in &insns {
        w.feat.mnem.push(mnemonic(i));
        if i.mnemonic() == Mnemonic::Mov
            && i.op0_kind() == OpKind::Memory
            && i.memory_base() != Register::ESP
            && !(frame && i.memory_base() == Register::EBP)
        {
            match i.memory_size().size() {
                1 => w.feat.widths.bytes = w.feat.widths.bytes.saturating_add(1),
                2 => w.feat.widths.words = w.feat.widths.words.saturating_add(1),
                _ => {}
            }
        }
        let abs = (i.op_count() > 0
            && has_memory(i)
            && i.memory_base() == Register::None
            && i.memory_index() == Register::None)
            .then(|| code.rva_of(u64::from(i.memory_displacement32())))
            .flatten();
        let imp = abs.and_then(|r| code.iat.get(&r).cloned());
        if let Some(n) = &imp {
            w.feat.imports.insert(n.clone());
        }
        match i.flow_control() {
            FlowControl::Call | FlowControl::IndirectCall => w.call(i, imp.as_ref()),
            FlowControl::UnconditionalBranch if i.op0_kind() == OpKind::NearBranch32 => w.tail(i),
            _ => {}
        }
        if i.mnemonic() == Mnemonic::Mov && i.op0_kind() == OpKind::Register {
            match &imp {
                Some(n) => {
                    w.reg_import.insert(i.op0_register(), n.clone());
                }
                None => {
                    w.reg_import.remove(&i.op0_register());
                }
            }
        }
        w.strings(i);
    }
    let mut feat = w.feat;
    if insns.len() >= MIN_BODY {
        feat.body = body_hash(&insns, code_lo, code_hi, false);
        feat.loose = body_hash(&insns, code_lo, code_hi, true);
    }
    feat
}

pub fn build(code: &Code<'_>, funs: Vec<Fun>, side: &mut Side) {
    let off = side.funs.len();
    let entry: HashMap<u32, usize> = funs
        .iter()
        .enumerate()
        .map(|(i, f)| (f.rva, off.saturating_add(i)))
        .collect();
    for (i, f) in funs.iter().enumerate() {
        side.feats
            .push(features(code, f, off.saturating_add(i), &entry));
    }
    let body = |f: &Fun| {
        code.bytes(
            f.rva,
            usize::try_from(f.end.saturating_sub(f.rva)).unwrap_or(0),
        )
        .unwrap_or(&[])
    };
    let first: Vec<Option<u16>> = funs
        .iter()
        .map(|f| signature(body(f), code.va(f.rva), &|_| None).pops)
        .collect();
    let pops_of = |t: u64| -> Option<u16> {
        let r = code.resolve(code.rva_of(t)?).ok()?;
        first
            .get(entry.get(&r)?.checked_sub(off)?)
            .copied()
            .flatten()
    };
    for (k, f) in funs.iter().enumerate() {
        if let Some(ft) = side.feats.get_mut(off.saturating_add(k)) {
            ft.conv = signature(body(f), code.va(f.rva), &pops_of);
        }
    }
    side.funs.extend(funs);
}
