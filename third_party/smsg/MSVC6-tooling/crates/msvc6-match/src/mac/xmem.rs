use super::feat::{Callee, Feat, api_key, float_key, int_key};
use crate::sibling::code::{MIN_STR, plausible_string, str_key};
use crate::sibling::feat::{MIN_BODY, body_hash};
use iced_x86::{
    Decoder, DecoderOptions, FlowControl, Instruction, InstructionInfoFactory, MemorySize,
    Mnemonic, OpAccess, OpKind, Register,
};
use msvc6_image::macho::MachO;
use msvc6_image::pe::Image;
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use std::collections::{BTreeMap, HashMap, HashSet};

pub const MAX_STRING: usize = 1024;

pub const ECX_LOOKAHEAD: usize = 40;

#[derive(Debug, Clone)]
pub struct XSec {
    pub va: u64,
    pub bytes: Vec<u8>,
    pub exec: bool,
}

impl XSec {
    fn end(&self) -> u64 {
        self.va
            .saturating_add(u64::try_from(self.bytes.len()).unwrap_or(0))
    }
}

#[derive(Debug, Clone, Default)]
pub struct XMem {
    pub secs: Vec<XSec>,
    pub stubs: BTreeMap<u64, String>,
    pub slots: BTreeMap<u64, String>,
}

impl XMem {
    #[must_use]
    pub fn from_pe(img: &Image) -> Self {
        let base = u64::from(img.opt.image_base.get());
        let secs = img
            .sections
            .iter()
            .map(|s| XSec {
                va: base.saturating_add(u64::from(s.rva.get())),
                bytes: img.at_rva(s.rva, s.disk_len()).unwrap_or(&[]).to_vec(),
                exec: s.flags & IMAGE_SCN_MEM_EXECUTE != 0,
            })
            .collect();
        let mut slots = BTreeMap::new();
        for d in img.imports().unwrap_or_default() {
            for e in d.entries {
                slots.insert(base.saturating_add(u64::from(e.iat_rva.get())), e.name);
            }
        }
        Self {
            secs,
            stubs: BTreeMap::new(),
            slots,
        }
    }

    #[must_use]
    pub fn from_macho(m: &MachO) -> Self {
        let secs = m
            .sections
            .iter()
            .filter_map(|s| {
                Some(XSec {
                    va: s.addr,
                    bytes: m.section_bytes(s)?.to_vec(),
                    exec: s.is_code(),
                })
            })
            .collect();
        Self {
            secs,
            stubs: m.stubs.clone(),
            slots: m.slots.clone(),
        }
    }

    fn sec(&self, va: u64) -> Option<&XSec> {
        self.secs.iter().find(|s| va >= s.va && va < s.end())
    }

    #[must_use]
    pub fn bytes(&self, va: u64, n: usize) -> Option<&[u8]> {
        let s = self.sec(va)?;
        let o = usize::try_from(va.checked_sub(s.va)?).ok()?;
        s.bytes.get(o..o.checked_add(n)?)
    }

    #[must_use]
    pub fn is_exec(&self, va: u64) -> bool {
        self.sec(va).is_some_and(|s| s.exec)
    }

    #[must_use]
    pub fn in_image(&self, va: u64) -> bool {
        self.sec(va).is_some()
    }

    #[must_use]
    pub fn string_at(&self, va: u64, min: usize) -> Option<String> {
        let s = self.sec(va)?;
        if s.exec {
            return None;
        }
        let o = usize::try_from(va.checked_sub(s.va)?).ok()?;
        let rest = s.bytes.get(o..)?;
        let end = rest.iter().take(MAX_STRING).position(|c| *c == 0)?;
        let b = rest.get(..end)?;
        plausible_string(b, min).then(|| msvc6_base::srcio::decode(b))
    }

    fn le_u32(&self, va: u64) -> Option<u32> {
        let b = self.bytes(va, 4)?;
        Some(u32::from_le_bytes(<[u8; 4]>::try_from(b).ok()?))
    }

    fn f64_at(&self, va: u64) -> Option<f64> {
        let b: [u8; 8] = self.bytes(va, 8)?.try_into().ok()?;
        Some(f64::from_le_bytes(b))
    }

    fn f32_at(&self, va: u64) -> Option<f64> {
        let b: [u8; 4] = self.bytes(va, 4)?.try_into().ok()?;
        Some(f64::from(f32::from_le_bytes(b)))
    }

    fn api_at(&self, va: u64) -> Option<String> {
        if let Some(n) = self.stubs.get(&va) {
            return Some(n.clone());
        }
        let b = self.bytes(va, 6)?;
        if b.first() == Some(&0xff) && b.get(1) == Some(&0x25) {
            let slot = u64::from(self.le_u32(va.checked_add(2)?)?);
            return self.slots.get(&slot).cloned();
        }
        None
    }

    fn resolve(&self, mut va: u64) -> u64 {
        for _ in 0..crate::sibling::code::THUNK_HOPS {
            let Some(b) = self.bytes(va, 5) else {
                return va;
            };
            if b.first() != Some(&0xe9) {
                return va;
            }
            let Some(rel) = va.checked_add(1).and_then(|a| self.le_u32(a)) else {
                return va;
            };
            let rel = i64::from(i32::from_le_bytes(rel.to_le_bytes()));
            let Some(t) = i64::try_from(va)
                .ok()
                .and_then(|v| u64::try_from(v + 5 + rel).ok())
            else {
                return va;
            };
            if !self.is_exec(t) {
                return va;
            }
            va = t;
        }
        va
    }

    #[must_use]
    pub fn decode(&self, va: u64, end: u64) -> Vec<Instruction> {
        let n = usize::try_from(end.saturating_sub(va)).unwrap_or(0);
        let Some(bytes) = self.bytes(va, n).or_else(|| {
            let s = self.sec(va)?;
            let o = usize::try_from(va.checked_sub(s.va)?).ok()?;
            s.bytes.get(o..)
        }) else {
            return Vec::new();
        };
        let mut d = Decoder::with_ip(32, bytes, va, DecoderOptions::NONE);
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

    fn code_bounds(&self) -> (u64, u64) {
        self.secs
            .iter()
            .filter(|s| s.exec)
            .fold((u64::MAX, 0u64), |(a, b), s| (a.min(s.va), b.max(s.end())))
    }
}

fn self_xor(i: &Instruction) -> bool {
    i.mnemonic() == Mnemonic::Xor
        && i.op_count() == 2
        && i.op0_kind() == OpKind::Register
        && i.op1_kind() == OpKind::Register
        && i.op0_register() == i.op1_register()
}

#[must_use]
pub fn reads_ecx_first(insns: &[Instruction]) -> Option<bool> {
    let mut f = InstructionInfoFactory::new();
    for i in insns.iter().take(ECX_LOOKAHEAD) {
        let (mut read, mut written) = (false, false);
        for r in f.info(i).used_registers() {
            if r.register().full_register32() != Register::ECX {
                continue;
            }
            match r.access() {
                OpAccess::Read
                | OpAccess::CondRead
                | OpAccess::ReadWrite
                | OpAccess::ReadCondWrite => {
                    read = true;
                }
                OpAccess::Write | OpAccess::CondWrite => written = true,
                _ => {}
            }
        }
        if self_xor(i) {
            return Some(false);
        }
        if read {
            return Some(true);
        }
        if written {
            return Some(false);
        }
        match i.flow_control() {
            FlowControl::Call | FlowControl::IndirectCall | FlowControl::Return => {
                return Some(false);
            }
            FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch => return None,
            _ => {}
        }
    }
    None
}

fn has_memory(i: &Instruction) -> bool {
    (0..i.op_count()).any(|o| i.op_kind(o) == OpKind::Memory)
}

struct Walk<'a> {
    mem: &'a XMem,
    entry: &'a HashMap<u64, usize>,
    api_names: &'a dyn Fn(u64) -> Option<String>,
    feat: Feat,
    seen_s: HashSet<String>,
    seen_g: HashSet<u64>,
    reg_api: HashMap<Register, String>,
}

impl Walk<'_> {
    fn target(&self, t: u64) -> Callee {
        if let Some(n) = self.mem.api_at(t) {
            return Callee::Api(api_key(&n));
        }
        let r = self.mem.resolve(t);
        if let Some(n) = self.mem.api_at(r) {
            return Callee::Api(api_key(&n));
        }
        match self.entry.get(&r) {
            Some(x) => (self.api_names)(r).map_or(Callee::Fn(*x), Callee::Api),
            None => Callee::Indirect,
        }
    }

    fn values(&mut self, i: &Instruction) {
        let mut vals: Vec<(u64, bool)> = Vec::new();
        for o in 0..i.op_count() {
            match i.op_kind(o) {
                OpKind::Immediate32 | OpKind::Immediate8to32 | OpKind::Immediate16 => {
                    vals.push((i.try_immediate(o).unwrap_or(0), true));
                }
                OpKind::Memory if i.memory_base() == Register::None => {
                    vals.push((u64::from(i.memory_displacement32()), false));
                }
                _ => {}
            }
        }
        for (v, imm) in vals {
            let v32 = v & 0xffff_ffff;
            if self.mem.in_image(v32) && !self.mem.is_exec(v32) {
                if let Some(s) = self.mem.string_at(v32, MIN_STR) {
                    let k = str_key(&s);
                    if k.len() >= MIN_STR && self.seen_s.insert(k.clone()) {
                        self.feat.strs.push(k);
                    }
                } else if !self.mem.slots.contains_key(&v32) && self.seen_g.insert(v32) {
                    self.feat.globals.push(v32);
                }
            } else if imm && !self.mem.in_image(v32) {
                let sv = i64::from(i32::from_le_bytes(
                    u32::try_from(v32).unwrap_or(0).to_le_bytes(),
                ));
                if let Some(k) = int_key(sv) {
                    self.feat.consts.insert(k);
                }
            }
        }
    }
}

#[must_use]
pub fn features(
    mem: &XMem,
    va: u64,
    end: u64,
    me: usize,
    entry: &HashMap<u64, usize>,
    api_names: &dyn Fn(u64) -> Option<String>,
    msvc: bool,
) -> Feat {
    let insns = mem.decode(va, end);
    let (lo, hi) = mem.code_bounds();
    let mut w = Walk {
        mem,
        entry,
        api_names,
        feat: Feat::default(),
        seen_s: HashSet::new(),
        seen_g: HashSet::new(),
        reg_api: HashMap::new(),
    };
    for i in &insns {
        w.feat.size = w.feat.size.saturating_add(1);
        let abs = (has_memory(i)
            && i.memory_base() == Register::None
            && i.memory_index() == Register::None)
            .then(|| u64::from(i.memory_displacement32()));
        let slot_api = abs.and_then(|a| mem.slots.get(&a).map(|n| api_key(n)));
        match i.flow_control() {
            FlowControl::Call | FlowControl::IndirectCall => {
                let c = if i.op0_kind() == OpKind::NearBranch32 {
                    w.target(i.near_branch_target())
                } else if let Some(n) = &slot_api {
                    Callee::Api(n.clone())
                } else if i.op0_kind() == OpKind::Register {
                    w.reg_api
                        .get(&i.op0_register())
                        .map_or(Callee::Indirect, |n| Callee::Api(n.clone()))
                } else {
                    Callee::Indirect
                };
                w.feat.calls.push(c);
            }
            FlowControl::UnconditionalBranch if i.op0_kind() == OpKind::NearBranch32 => {
                let t = i.near_branch_target();
                if t < va || t >= end {
                    match w.target(t) {
                        Callee::Fn(x) if x == me => {}
                        Callee::Indirect => {}
                        c => w.feat.calls.push(c),
                    }
                }
            }
            FlowControl::Return => {
                if i.op_count() == 1 && i.op0_kind() == OpKind::Immediate16 {
                    w.feat.ret_pop = Some(i.immediate16());
                } else if w.feat.ret_pop.is_none() {
                    w.feat.ret_pop = Some(0);
                }
            }
            _ => {}
        }
        if i.mnemonic() == Mnemonic::Mov && i.op0_kind() == OpKind::Register {
            match &slot_api {
                Some(n) => {
                    w.reg_api.insert(i.op0_register(), n.clone());
                }
                None => {
                    w.reg_api.remove(&i.op0_register());
                }
            }
        }
        if has_memory(i) {
            let b = i.memory_base();
            if b != Register::None
                && b != Register::ESP
                && b != Register::EBP
                && i.memory_index() == Register::None
            {
                let d = i.memory_displacement32();
                if d > 0 && d < 0x4000 {
                    w.feat.disps.insert(d);
                }
            }
        }
        let msz = i.memory_size();
        if let Some(a) = abs.filter(|_| matches!(msz, MemorySize::Float64 | MemorySize::Float32)) {
            let v = if msz == MemorySize::Float64 {
                mem.f64_at(a)
            } else {
                mem.f32_at(a)
            };
            if let Some(k) = v.and_then(float_key) {
                w.feat.consts.insert(k);
            }
        }
        w.values(i);
    }
    let mut feat = w.feat;
    if msvc {
        feat.this_ecx = reads_ecx_first(&insns);
    }
    if insns.len() >= MIN_BODY {
        feat.body = body_hash(&insns, lo, hi, false);
        feat.loose = body_hash(&insns, lo, hi, true);
    }
    feat
}
