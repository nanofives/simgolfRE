use super::feat::{Callee, Feat, api_key, float_key, int_key};
use crate::sibling::code::{MIN_STR, plausible_string, str_key};
use msvc6_image::pef::{Pef, Target};
use std::collections::{HashMap, HashSet};

pub const MAX_STRING: usize = 1024;

pub const MAX_DISP: u32 = 0x4000;

const GLUE: [u32; 5] = [
    0x9041_0014,
    0x800c_0000,
    0x804c_0004,
    0x7c09_03a6,
    0x4e80_0420,
];

#[derive(Debug, Clone, Copy, PartialEq)]
enum Val {
    Unknown,
    Const(i64),
    Data(u32),
}

fn sext16(w: u32) -> i64 {
    i64::from(i16::from_be_bytes(
        u16::try_from(w & 0xffff).unwrap_or(0).to_be_bytes(),
    ))
}

fn field(w: u32, shift: u32) -> usize {
    usize::try_from((w >> shift) & 31).unwrap_or(0)
}

fn add(a: u32, d: i64) -> Option<u32> {
    u32::try_from(i64::from(a).checked_add(d)?).ok()
}

#[must_use]
pub fn string_at(pef: &Pef, off: u32, min: usize) -> Option<String> {
    let s = pef.data.get(usize::try_from(off).ok()?..)?;
    let end = s.iter().take(MAX_STRING).position(|c| *c == 0)?;
    let b = s.get(..end)?;
    plausible_string(b, min).then(|| msvc6_base::srcio::decode(b))
}

#[must_use]
pub fn glue_import(pef: &Pef, t: u32) -> Option<usize> {
    let w0 = pef.code_word(t)?;
    if w0 >> 16 != 0x8182 {
        return None;
    }
    for (k, want) in GLUE.iter().enumerate() {
        let at = t.checked_add(u32::try_from(k).ok()?.checked_add(1)?.checked_mul(4)?)?;
        if pef.code_word(at)? != *want {
            return None;
        }
    }
    match pef.target(add(pef.toc, sext16(w0))?)? {
        Target::Import(i) => Some(i),
        _ => None,
    }
}

struct Walk<'a> {
    pef: &'a Pef,
    feat: Feat,
    seen_s: HashSet<String>,
    seen_g: HashSet<u32>,
    regs: [Val; 32],
}

impl Walk<'_> {
    fn set(&mut self, r: usize, v: Val) {
        if let Some(x) = self.regs.get_mut(r) {
            *x = v;
        }
    }

    fn get(&self, r: usize) -> Val {
        self.regs.get(r).copied().unwrap_or(Val::Unknown)
    }

    fn clobber_call(&mut self) {
        for r in 3..=12 {
            self.set(r, Val::Unknown);
        }
    }

    fn note_addr(&mut self, a: u32) {
        if let Some(s) = string_at(self.pef, a, MIN_STR) {
            let k = str_key(&s);
            if k.len() >= MIN_STR && self.seen_s.insert(k.clone()) {
                self.feat.strs.push(k);
            }
        } else if self.seen_g.insert(a) {
            self.feat.globals.push(u64::from(a));
        }
    }

    fn konst(&mut self, v: i64) {
        if let Some(k) = int_key(v) {
            self.feat.consts.insert(k);
        }
    }

    fn disp(&mut self, d: i64) {
        if let Ok(dd) = u32::try_from(d)
            && dd > 0
            && dd < MAX_DISP
        {
            self.feat.disps.insert(dd);
        }
    }

    fn float(&mut self, op: u32, ra: usize, d: i64) {
        let at = match self.get(ra) {
            Val::Data(a) => add(a, d),
            _ if ra == 2 => add(self.pef.toc, d),
            _ => None,
        };
        if let Some(a) = at {
            let v = if op == 50 {
                self.pef
                    .data_bytes(a, 8)
                    .and_then(|b| <[u8; 8]>::try_from(b).ok())
                    .map(f64::from_be_bytes)
            } else {
                self.pef
                    .data_bytes(a, 4)
                    .and_then(|b| <[u8; 4]>::try_from(b).ok())
                    .map(|b| f64::from(f32::from_be_bytes(b)))
            };
            if let Some(k) = v.and_then(float_key) {
                self.feat.consts.insert(k);
            }
        } else if ra != 1 && ra != 2 {
            self.disp(d);
        }
    }

    fn load_store(&mut self, op: u32, rd: usize, ra: usize, d: i64) {
        let base = self.get(ra);
        if ra != 1 && ra != 2 && ra != 0 && !matches!(base, Val::Data(_)) {
            self.disp(d);
        }
        if let Val::Data(a) = base
            && let Some(na) = add(a, d)
            && self.seen_g.insert(na)
        {
            self.feat.globals.push(u64::from(na));
        }
        if matches!(op, 32..=35 | 40..=43 | 46) {
            self.set(rd, Val::Unknown);
        }
        if matches!(op, 33 | 35 | 37 | 39 | 41 | 43 | 45 | 49 | 51 | 53 | 55) {
            self.set(ra, Val::Unknown);
        }
    }
}

#[must_use]
pub fn features(
    pef: &Pef,
    start: u32,
    end: u32,
    me: usize,
    entry: &HashMap<u32, usize>,
    api_name: &dyn Fn(usize) -> Option<String>,
) -> Feat {
    let mut w = Walk {
        pef,
        feat: Feat::default(),
        seen_s: HashSet::new(),
        seen_g: HashSet::new(),
        regs: [Val::Unknown; 32],
    };
    let call_of = |t: u32| -> Callee {
        if let Some(i) = glue_import(pef, t) {
            return Callee::Api(api_key(pef.import_name(i).unwrap_or("")));
        }
        match entry.get(&t) {
            Some(x) => api_name(*x).map_or(Callee::Fn(*x), Callee::Api),
            None => Callee::Indirect,
        }
    };
    let mut pc = start;
    while pc < end {
        let Some(word) = pef.code_word(pc) else {
            break;
        };
        w.feat.size = w.feat.size.saturating_add(1);
        let op = word >> 26;
        let rd = field(word, 21);
        let ra = field(word, 16);
        match op {
            18 => {
                let li =
                    i64::from(i32::from_be_bytes(((word & 0x03ff_fffc) << 6).to_be_bytes()) >> 6);
                let t = if word & 2 != 0 {
                    u32::try_from(li).unwrap_or(0)
                } else {
                    add(pc, li).unwrap_or(0)
                };
                if word & 1 != 0 {
                    w.feat.calls.push(call_of(t));
                    w.clobber_call();
                    w.set(0, Val::Unknown);
                } else if t < start || t >= end {
                    match call_of(t) {
                        Callee::Fn(x) if x == me => {}
                        Callee::Indirect => {}
                        c => w.feat.calls.push(c),
                    }
                }
            }
            19 => {
                if (word >> 1) & 0x3ff == 528 && word & 1 != 0 {
                    w.feat.calls.push(Callee::Indirect);
                    w.clobber_call();
                }
            }
            14 => {
                let simm = sext16(word);
                if ra == 0 {
                    w.set(rd, Val::Const(simm));
                    w.konst(simm);
                } else {
                    match w.get(ra) {
                        Val::Data(a) => {
                            let na = add(a, simm).unwrap_or(0);
                            w.note_addr(na);
                            w.set(rd, Val::Data(na));
                        }
                        Val::Const(c) => w.set(rd, Val::Const(c.saturating_add(simm))),
                        Val::Unknown => w.set(rd, Val::Unknown),
                    }
                }
            }
            15 => {
                let simm = sext16(word) << 16;
                if ra == 0 {
                    w.set(rd, Val::Const(simm));
                } else {
                    match w.get(ra) {
                        Val::Const(c) => w.set(rd, Val::Const(c.saturating_add(simm))),
                        _ => w.set(rd, Val::Unknown),
                    }
                }
            }
            24 | 25 => {
                let ui = i64::from(word & 0xffff) << if op == 25 { 16 } else { 0 };
                match w.get(rd) {
                    Val::Const(c) => {
                        let v = c | ui;
                        w.konst(v);
                        w.set(ra, Val::Const(v));
                    }
                    _ => w.set(ra, Val::Unknown),
                }
            }
            10 | 11 => {
                let v = if op == 11 {
                    sext16(word)
                } else {
                    i64::from(word & 0xffff)
                };
                w.konst(v);
            }
            7 => {
                w.konst(sext16(word));
                w.set(rd, Val::Unknown);
            }
            32 if ra == 2 => {
                let slot = add(pef.toc, sext16(word)).unwrap_or(0);
                match pef.target(slot) {
                    Some(Target::Data(a)) => {
                        w.note_addr(a);
                        w.set(rd, Val::Data(a));
                    }
                    _ => w.set(rd, Val::Unknown),
                }
            }
            48 | 50 => w.float(op, ra, sext16(word)),
            32..=47 | 52..=55 => w.load_store(op, rd, ra, sext16(word)),
            31 => {
                let xo = (word >> 1) & 0x3ff;
                let rb = field(word, 11);
                if xo == 444 && rd == rb {
                    let v = w.get(rd);
                    w.set(ra, v);
                } else {
                    w.set(rd, Val::Unknown);
                    w.set(ra, Val::Unknown);
                }
            }
            8 | 12 | 13 => w.set(rd, Val::Unknown),
            20 | 21 | 23 | 26..=30 => w.set(ra, Val::Unknown),
            _ => {}
        }
        pc = pc.saturating_add(4);
    }
    w.feat
}
