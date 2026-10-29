use crate::decode::decode;
use iced_x86::{Formatter as _, Instruction, MasmFormatter, Mnemonic, OpKind, Register};
use std::collections::BTreeMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    RelocatedOnly,
    RegisterOnly,
    Different,
    Moved,
}

#[derive(Debug, Clone)]
pub struct Pair {
    pub at: u64,
    pub kind: Kind,
    pub want: String,
    pub got: String,
}

#[derive(Debug, Default, Clone)]
pub struct Report {
    pub pairs: Vec<Pair>,
    pub permutation: Option<BTreeMap<Register, Register>>,
    pub same: usize,
    pub register_only: usize,
    pub different: usize,
    pub moved: usize,
    pub stack_cleanup: Option<(u32, u32)>,
}

pub type Aligned = Vec<(Option<usize>, Option<usize>)>;

#[must_use]
pub fn align<K: Eq + std::hash::Hash>(want: &[K], got: &[K]) -> Aligned {
    let mut aligned: Aligned = Vec::new();
    for op in similar::capture_diff_slices(similar::Algorithm::Myers, want, got) {
        match op {
            similar::DiffOp::Equal {
                old_index,
                new_index,
                len,
            } => {
                for d in 0..len {
                    aligned.push((Some(old_index + d), Some(new_index + d)));
                }
            }
            similar::DiffOp::Replace {
                old_index,
                old_len,
                new_index,
                new_len,
            } => {
                for d in 0..old_len.max(new_len) {
                    aligned.push((
                        (d < old_len).then_some(old_index + d),
                        (d < new_len).then_some(new_index + d),
                    ));
                }
            }
            similar::DiffOp::Delete {
                old_index, old_len, ..
            } => {
                for d in 0..old_len {
                    aligned.push((Some(old_index + d), None));
                }
            }
            similar::DiffOp::Insert {
                new_index, new_len, ..
            } => {
                for d in 0..new_len {
                    aligned.push((None, Some(new_index + d)));
                }
            }
        }
    }
    aligned
}

fn stack_cleanup(ins: &[Instruction]) -> Option<u32> {
    ins.iter()
        .find(|i| i.mnemonic() == Mnemonic::Ret)
        .map(|i| if i.op_count() > 0 { i.immediate32() } else { 0 })
}

#[must_use]
pub fn text(f: &mut MasmFormatter, i: &Instruction) -> String {
    let mut s = String::new();
    f.format(i, &mut s);
    s
}

fn registers(i: &Instruction) -> Vec<Register> {
    let mut v = Vec::new();
    for k in 0..i.op_count() {
        if i.op_kind(k) == OpKind::Register {
            v.push(i.op_register(k));
        }
    }
    if i.memory_base() != Register::None {
        v.push(i.memory_base());
    }
    if i.memory_index() != Register::None {
        v.push(i.memory_index());
    }
    v
}

fn register_variant(a: &Instruction, b: &Instruction) -> Option<Vec<(Register, Register)>> {
    if a.mnemonic() != b.mnemonic() || a.op_count() != b.op_count() {
        return None;
    }
    for k in 0..a.op_count() {
        if a.op_kind(k) != b.op_kind(k) {
            return None;
        }
        if matches!(
            a.op_kind(k),
            OpKind::Immediate8
                | OpKind::Immediate16
                | OpKind::Immediate32
                | OpKind::Immediate64
                | OpKind::Immediate8to32
                | OpKind::Immediate8to16
        ) && a.immediate(k) != b.immediate(k)
        {
            return None;
        }
    }
    if a.memory_displacement64() != b.memory_displacement64()
        || a.memory_index_scale() != b.memory_index_scale()
    {
        return None;
    }
    let (ra, rb) = (registers(a), registers(b));
    if ra.len() != rb.len() {
        return None;
    }
    Some(ra.into_iter().zip(rb).filter(|(x, y)| x != y).collect())
}

fn covered(relocated: &[bool], off: usize, len: usize) -> (bool, bool) {
    let any = (off..off.saturating_add(len)).any(|i| relocated.get(i).copied().unwrap_or(false));
    let all = len > 0
        && (off..off.saturating_add(len)).all(|i| relocated.get(i).copied().unwrap_or(false));
    (any, all)
}

#[must_use]
pub fn compare(want: &[u8], got: &[u8], relocated: &[bool], ip: u64) -> Report {
    let wi = decode(want, ip);
    let gi = decode(got, ip);
    let mut r = Report::default();
    let mut f = MasmFormatter::new();
    let mut subs: Vec<(Register, Register)> = Vec::new();
    let mut only_registers = true;
    let wtext: Vec<String> = wi.iter().map(|i| text(&mut f, i)).collect();
    let gtext: Vec<String> = gi.iter().map(|i| text(&mut f, i)).collect();
    for (wk, gk) in align(&wtext, &gtext) {
        match (wk.and_then(|k| wi.get(k)), gk.and_then(|k| gi.get(k))) {
            (Some(a), Some(b)) => {
                let woff = usize::try_from(a.ip().saturating_sub(ip)).unwrap_or(0);
                let goff = usize::try_from(b.ip().saturating_sub(ip)).unwrap_or(0);
                let wb = want.get(woff..woff.saturating_add(a.len()));
                let gb = got.get(goff..goff.saturating_add(b.len()));
                if wb == gb {
                    r.same += 1;
                    continue;
                }
                let (reloc, all_reloc) = covered(relocated, goff, a.len());
                if all_reloc
                    || (reloc && a.mnemonic() == b.mnemonic() && registers(a) == registers(b))
                {
                    r.same += 1;
                    r.pairs.push(Pair {
                        at: a.ip(),
                        kind: Kind::RelocatedOnly,
                        want: text(&mut f, a),
                        got: text(&mut f, b),
                    });
                    continue;
                }
                let kind = match register_variant(a, b) {
                    Some(s) => {
                        subs.extend(s);
                        r.register_only += 1;
                        Kind::RegisterOnly
                    }
                    None => {
                        only_registers = false;
                        r.different += 1;
                        Kind::Different
                    }
                };
                r.pairs.push(Pair {
                    at: a.ip(),
                    kind,
                    want: text(&mut f, a),
                    got: text(&mut f, b),
                });
            }
            (a, b) => {
                only_registers = false;
                r.moved += 1;
                r.pairs.push(Pair {
                    at: a.or(b).map_or(ip, Instruction::ip),
                    kind: Kind::Moved,
                    want: a.map(|i| text(&mut f, i)).unwrap_or_default(),
                    got: b.map(|i| text(&mut f, i)).unwrap_or_default(),
                });
            }
        }
    }
    if let (Some(w), Some(g)) = (stack_cleanup(&wi), stack_cleanup(&gi))
        && w != g
    {
        r.stack_cleanup = Some((w, g));
    }
    if only_registers && !subs.is_empty() {
        let mut map: BTreeMap<Register, Register> = BTreeMap::new();
        let mut consistent = true;
        for (a, b) in subs {
            if *map.entry(a).or_insert(b) != b {
                consistent = false;
            }
        }
        if consistent {
            r.permutation = Some(map);
        }
    }
    r
}

impl Report {
    #[must_use]
    pub fn lines(&self, indent: &str) -> Vec<String> {
        let mut out = Vec::new();
        if let Some((w, g)) = self.stack_cleanup {
            out.push(format!(
                "{indent}CALLING CONVENTION: the reference pops {w} byte(s) of arguments, ours pops {g}."
            ));
            out.push(format!(
                "{indent}That is the declaration, not the codegen -- wrong convention or wrong argument count."
            ));
        }
        for p in &self.pairs {
            let tag = match p.kind {
                Kind::RelocatedOnly => continue,
                Kind::RegisterOnly => "reg",
                Kind::Moved => "ord",
                Kind::Different => "   ",
            };
            out.push(format!("{indent}{:#010x} {tag}  want {}", p.at, p.want));
            out.push(format!("{indent}                got  {}", p.got));
        }
        out.push(format!(
            "{indent}{} wrong instruction(s), {} register-only, {} reordered",
            self.different, self.register_only, self.moved
        ));
        if let Some(m) = &self.permutation {
            let subs: Vec<String> = m.iter().map(|(a, b)| format!("{a:?}->{b:?}")).collect();
            out.push(format!(
                "{indent}ALL {} difference(s) are REGISTER ALLOCATION: {}",
                self.register_only,
                subs.join(", ")
            ));
        } else if self.different == 0 && (self.register_only > 0 || self.moved > 0) {
            out.push(format!(
                "{indent}NO WRONG INSTRUCTIONS -- the statements are right, the allocator and scheduler disagreed."
            ));
        }
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_insertion_aligns_as_one_unpaired_row() {
        let a = ["x", "y", "z"];
        let b = ["x", "q", "y", "z"];
        let al = align(&a, &b);
        assert_eq!(
            al,
            vec![
                (Some(0), Some(0)),
                (None, Some(1)),
                (Some(1), Some(2)),
                (Some(2), Some(3))
            ]
        );
    }

    #[test]
    fn a_register_swap_is_register_only_and_a_permutation() {
        let want = [0x89, 0xd8, 0xc3];
        let got = [0x89, 0xd9, 0xc3];
        let r = compare(&want, &got, &[false; 3], 0x1000);
        assert_eq!(r.register_only, 1);
        assert_eq!(r.different, 0);
        assert!(r.permutation.is_some());
        let r = compare(&want, &want, &[false; 3], 0x1000);
        assert_eq!(r.same, 2);
    }
}
