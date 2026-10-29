use crate::segments::{Analysis, Pairing, Side, Verdict, align_segment};
use iced_x86::{Formatter as _, Instruction, MasmFormatter, Mnemonic, OpKind, Register};
use std::collections::HashMap;

pub const NEAR: usize = 6;
pub const DEPTH: usize = 4;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Swap {
    pub seg: usize,
    pub img_off: usize,
    pub ours_off: usize,
    pub op: String,
    pub class: &'static str,
    pub cause: &'static str,
    pub seg_edit: usize,
    pub image: String,
    pub ours: String,
    pub values: (String, String),
}

const GPRS: [Register; 8] = [
    Register::EAX,
    Register::ECX,
    Register::EDX,
    Register::EBX,
    Register::ESP,
    Register::EBP,
    Register::ESI,
    Register::EDI,
];

fn writes(s: &Side, i: usize) -> Option<Register> {
    let insn = &s.dec.get(i)?.insn;
    if matches!(
        insn.mnemonic(),
        Mnemonic::Cmp | Mnemonic::Test | Mnemonic::Push
    ) || insn.op_count() == 0
        || insn.op0_kind() != OpKind::Register
    {
        return None;
    }
    Some(insn.op0_register().full_register32())
}

#[derive(Debug)]
pub struct Look<'a> {
    s: &'a Side,
    last: Vec<[u32; 8]>,
    anchors: HashMap<usize, &'a str>,
}

impl<'a> Look<'a> {
    #[must_use]
    pub fn new(s: &'a Side) -> Self {
        let mut last = Vec::with_capacity(s.dec.len().saturating_add(1));
        let mut cur = [0u32; 8];
        for k in 0..s.dec.len() {
            last.push(cur);
            if let Some(g) = writes(s, k).and_then(|r| GPRS.iter().position(|x| *x == r))
                && let Some(c) = cur.get_mut(g)
            {
                *c = u32::try_from(k.saturating_add(1)).unwrap_or(u32::MAX);
            }
        }
        last.push(cur);
        let mut anchors = HashMap::new();
        for a in s.anchors.iter().filter(|a| a.key.starts_with("D:")) {
            anchors.entry(a.ix).or_insert(a.key.as_str());
        }
        Self { s, last, anchors }
    }

    fn writer(&self, lo: usize, at: usize, r: Register) -> Option<usize> {
        let Some(g) = GPRS.iter().position(|x| *x == r) else {
            return (lo..at).rev().find(|&k| writes(self.s, k) == Some(r));
        };
        let k = usize::try_from(*self.last.get(at)?.get(g)?)
            .ok()?
            .checked_sub(1)?;
        (k >= lo).then_some(k)
    }

    fn anchor(&self, i: usize) -> Option<&'a str> {
        self.anchors.get(&i).copied()
    }
}

fn reg_value(l: &Look<'_>, lo: usize, at: usize, r: Register, depth: usize) -> Option<String> {
    if r == Register::ESP {
        return Some("esp".into());
    }
    if depth == 0 {
        return None;
    }
    let k = l.writer(lo, at, r)?;
    let insn = &l.s.dec.get(k)?.insn;
    let m = format!("{:?}", insn.mnemonic()).to_lowercase();
    match insn.mnemonic() {
        Mnemonic::Pop | Mnemonic::Call | Mnemonic::Xchg => None,
        Mnemonic::Mov | Mnemonic::Movzx | Mnemonic::Movsx | Mnemonic::Lea => {
            let v = operand(l, lo, k, 1, depth)?;
            Some(if insn.mnemonic() == Mnemonic::Mov {
                v
            } else {
                format!("{m}({v})")
            })
        }
        Mnemonic::Xor | Mnemonic::Sub
            if insn.op_count() == 2
                && insn.op1_kind() == OpKind::Register
                && insn.op1_register().full_register32() == r =>
        {
            Some("#0".into())
        }
        _ => {
            let mut parts = vec![reg_value(l, lo, k, r, depth - 1)?];
            for o in 1..insn.op_count() {
                parts.push(operand(l, lo, k, o, depth - 1)?);
            }
            Some(format!("{m}({})", parts.join(",")))
        }
    }
}

fn operand(l: &Look<'_>, lo: usize, k: usize, o: u32, depth: usize) -> Option<String> {
    let insn = &l.s.dec.get(k)?.insn;
    match insn.op_kind(o) {
        OpKind::Register => reg_value(l, lo, k, insn.op_register(o).full_register32(), depth),
        OpKind::Memory => {
            if let Some(a) = l.anchor(k)
                && insn.memory_base() == Register::None
                && insn.memory_index() == Register::None
            {
                return Some(a.to_string());
            }
            if insn.memory_base() == Register::ESP {
                return Some("[frame]".into());
            }
            Some(format!("[{}]", address(l, lo, k, depth)?))
        }
        OpKind::Immediate8
        | OpKind::Immediate8to32
        | OpKind::Immediate16
        | OpKind::Immediate32
        | OpKind::Immediate8to16 => Some(format!("#{:x}", insn.immediate(o))),
        _ => None,
    }
}

fn address(l: &Look<'_>, lo: usize, k: usize, depth: usize) -> Option<String> {
    let insn = &l.s.dec.get(k)?.insn;
    let mut parts = Vec::new();
    if insn.memory_base() != Register::None {
        parts.push(reg_value(
            l,
            lo,
            k,
            insn.memory_base().full_register32(),
            depth,
        )?);
    }
    if insn.memory_index() != Register::None {
        let v = reg_value(l, lo, k, insn.memory_index().full_register32(), depth)?;
        parts.push(format!("{v}*{}", insn.memory_index_scale()));
    }
    match l.anchor(k) {
        Some(a) => parts.push(a.to_string()),
        None => parts.push(format!("{:x}", insn.memory_displacement32())),
    }
    Some(parts.join("+"))
}

fn is_fold(insn: &Instruction) -> bool {
    matches!(
        insn.mnemonic(),
        Mnemonic::Add | Mnemonic::And | Mnemonic::Or | Mnemonic::Xor | Mnemonic::Imul
    ) && insn.op_count() == 2
        && insn.op0_kind() == OpKind::Register
}

fn fold_values(l: &Look<'_>, lo: usize, i: usize) -> Option<(String, String)> {
    let insn = &l.s.dec.get(i)?.insn;
    if !is_fold(insn) {
        return None;
    }
    let d = reg_value(l, lo, i, insn.op0_register().full_register32(), DEPTH)?;
    Some((d, operand(l, lo, i, 1, DEPTH)?))
}

fn cmp_values(l: &Look<'_>, lo: usize, i: usize) -> Option<(String, String)> {
    let insn = &l.s.dec.get(i)?.insn;
    if insn.mnemonic() != Mnemonic::Cmp
        || insn.op_count() != 2
        || !matches!(insn.op1_kind(), OpKind::Register | OpKind::Memory)
    {
        return None;
    }
    Some((operand(l, lo, i, 0, DEPTH)?, operand(l, lo, i, 1, DEPTH)?))
}

fn lea_values(l: &Look<'_>, lo: usize, i: usize) -> Option<(String, String)> {
    let insn = &l.s.dec.get(i)?.insn;
    if insn.mnemonic() != Mnemonic::Lea || insn.memory_displacement32() != 0 {
        return None;
    }
    addr_values(l, lo, i)
}

fn add_values(l: &Look<'_>, lo: usize, i: usize) -> Option<(String, String)> {
    let insn = &l.s.dec.get(i)?.insn;
    if insn.mnemonic() != Mnemonic::Add || insn.op1_kind() != OpKind::Register {
        return None;
    }
    fold_values(l, lo, i)
}

fn addr_values(l: &Look<'_>, lo: usize, i: usize) -> Option<(String, String)> {
    let insn = &l.s.dec.get(i)?.insn;
    let mem = (0..insn.op_count()).any(|o| insn.op_kind(o) == OpKind::Memory);
    if !mem
        || insn.memory_base() == Register::None
        || insn.memory_index() == Register::None
        || insn.memory_index_scale() != 1
    {
        return None;
    }
    let b = reg_value(l, lo, i, insn.memory_base().full_register32(), DEPTH)?;
    let x = reg_value(l, lo, i, insn.memory_index().full_register32(), DEPTH)?;
    Some((b, x))
}

fn text(s: &Side, i: usize) -> String {
    let mut f = MasmFormatter::new();
    let mut out = String::new();
    if let Some(d) = s.dec.get(i) {
        f.format(&d.insn, &mut out);
    }
    out
}

fn leaf(v: &str) -> bool {
    v.starts_with('[') || v.starts_with("D:")
}

fn cause(x: &str, y: &str) -> &'static str {
    if leaf(x) && leaf(y) {
        "hash"
    } else {
        "depth/size"
    }
}

type Values = fn(&Look<'_>, usize, usize) -> Option<(String, String)>;

const KINDS: [(&str, Values, Values); 5] = [
    ("fold", fold_values, fold_values),
    ("cmp", cmp_values, cmp_values),
    ("addr", addr_values, addr_values),
    ("lea", lea_values, add_values),
    ("lea", add_values, lea_values),
];

#[must_use]
pub fn find(a: &Analysis) -> Vec<Swap> {
    let mut out = Vec::new();
    let (img, ours) = (Look::new(&a.img), Look::new(&a.ours));
    for (n, seg) in a.segs.iter().enumerate() {
        if seg.verdict == Verdict::Exact {
            continue;
        }
        let rows = align_segment(&a.img, &a.ours, seg.ia, seg.oa);
        let mut taken = vec![false; rows.len()];
        for (pi, row) in rows.iter().enumerate() {
            let Some(i) = row.img else { continue };
            let Some(xi) = a.img.dec.get(i).map(|d| &d.insn) else {
                continue;
            };
            if row.pairing == Pairing::Same && xi.mnemonic() != Mnemonic::Cmp {
                continue;
            }
            for (class, values, ours_values) in KINDS {
                let Some((p, q)) = values(&img, 0, i) else {
                    continue;
                };
                if p == q {
                    continue;
                }
                let same_op = class != "lea";
                let lo = pi.saturating_sub(NEAR);
                let hit = (lo..rows.len().min(pi.saturating_add(NEAR + 1))).find(|&pj| {
                    let Some(j) = rows.get(pj).and_then(|r| r.ours) else {
                        return false;
                    };
                    !taken.get(pj).copied().unwrap_or(true)
                        && (!same_op
                            || a.ours
                                .dec
                                .get(j)
                                .is_some_and(|d| d.insn.mnemonic() == xi.mnemonic()))
                        && ours_values(&ours, 0, j) == Some((q.clone(), p.clone()))
                });
                let Some(pj) = hit else { continue };
                let Some(j) = rows.get(pj).and_then(|r| r.ours) else {
                    continue;
                };
                if let Some(t) = taken.get_mut(pj) {
                    *t = true;
                }
                out.push(Swap {
                    seg: n,
                    img_off: a.img.dec.get(i).map_or(0, |d| d.ip as usize),
                    ours_off: a.ours.dec.get(j).map_or(0, |d| d.ip as usize),
                    op: format!("{:?}", xi.mnemonic()).to_lowercase(),
                    class,
                    cause: if class == "cmp" {
                        "source"
                    } else {
                        cause(&p, &q)
                    },
                    seg_edit: seg.edit,
                    image: text(&a.img, i),
                    ours: text(&a.ours, j),
                    values: (p, q),
                });
                break;
            }
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::segments::test_side as side;

    #[test]
    fn a_fold_names_its_operands_by_the_values_they_hold() {
        let s = side(&[0x8b, 0x46, 0x08, 0x8b, 0x77, 0x04, 0x03, 0xc6]);
        assert_eq!(fold_values(&Look::new(&s), 0, 2), None);
        let t = side(&[
            0x8b, 0x74, 0x24, 0x04, 0x8b, 0x46, 0x08, 0x8b, 0x4e, 0x04, 0x03, 0xc1,
        ]);
        let (d, s2) = fold_values(&Look::new(&t), 0, 3).unwrap();
        assert_eq!(d, "[[frame]+8]");
        assert_eq!(s2, "[[frame]+4]");
        assert_eq!(cause(&d, &s2), "hash");
        let u = side(&[
            0x8b, 0x74, 0x24, 0x04, 0x8b, 0x4e, 0x08, 0x8b, 0x46, 0x04, 0x03, 0xc1,
        ]);
        assert_eq!(fold_values(&Look::new(&u), 0, 3), Some((s2, d)));
    }

    #[test]
    fn a_copy_spends_no_depth_and_a_self_xor_is_zero() {
        let s = side(&[
            0x8b, 0x74, 0x24, 0x04, 0x8a, 0x46, 0x20, 0x8a, 0xc8, 0x80, 0xe1, 0x03, 0xc0, 0xe1,
            0x04, 0x8a, 0x56, 0x22, 0x0a, 0xd1,
        ]);
        assert_eq!(
            fold_values(&Look::new(&s), 0, 6),
            Some(("[[frame]+22]".into(), "shl(and([[frame]+20],#3),#4)".into()))
        );
        let t = side(&[0x33, 0xc9, 0x8b, 0x44, 0x24, 0x04, 0x03, 0xc1]);
        assert_eq!(
            fold_values(&Look::new(&t), 0, 2),
            Some(("[frame]".into(), "#0".into()))
        );
    }

    #[test]
    fn a_three_address_lea_pairs_with_a_two_address_add_of_the_other_order() {
        let img = side(&[0xbe, 0x05, 0, 0, 0, 0xb9, 0x07, 0, 0, 0, 0x8d, 0x14, 0x0e]);
        let ours = side(&[0xbe, 0x05, 0, 0, 0, 0xb9, 0x07, 0, 0, 0, 0x03, 0xce]);
        let (p, q) = lea_values(&Look::new(&img), 0, 2).unwrap();
        assert_eq!((p.as_str(), q.as_str()), ("#5", "#7"));
        assert_eq!(add_values(&Look::new(&ours), 0, 2), Some((q, p)));
        let disp = side(&[
            0xbe, 0x05, 0, 0, 0, 0xb9, 0x07, 0, 0, 0, 0x8d, 0x54, 0x0e, 0x04,
        ]);
        assert_eq!(lea_values(&Look::new(&disp), 0, 2), None);
    }

    #[test]
    fn a_compare_writes_no_register_and_an_address_names_base_and_index() {
        let s = side(&[0x0a, 0xd1, 0x3b, 0xc1, 0x8d, 0x04, 0x3a]);
        assert_eq!(writes(&s, 0), Some(Register::EDX));
        assert_eq!(writes(&s, 1), None);
        assert_eq!(writes(&s, 2), Some(Register::EAX));
        let t = side(&[
            0x8b, 0x54, 0x24, 0x04, 0x8b, 0x7c, 0x24, 0x08, 0x8d, 0x04, 0x3a,
        ]);
        assert_eq!(
            addr_values(&Look::new(&t), 0, 2),
            Some(("[frame]".into(), "[frame]".into()))
        );
    }

    #[test]
    fn a_compare_names_both_values_in_source_order_and_skips_an_immediate() {
        let img = side(&[
            0xa1, 0xcc, 0x2f, 0x4c, 0x00, 0x8b, 0x4c, 0x24, 0x04, 0x3b, 0xc1,
        ]);
        let ours = side(&[
            0x8b, 0x0d, 0xcc, 0x2f, 0x4c, 0x00, 0x8b, 0x44, 0x24, 0x04, 0x3b, 0xc1,
        ]);
        let (p, q) = cmp_values(&Look::new(&img), 0, 2).unwrap();
        assert_eq!((p.as_str(), q.as_str()), ("[4c2fcc]", "[frame]"));
        assert_eq!(cmp_values(&Look::new(&ours), 0, 2), Some((q, p)));
        let k = side(&[0x83, 0xf8, 0x05]);
        assert_eq!(cmp_values(&Look::new(&k), 0, 0), None);
    }
}
