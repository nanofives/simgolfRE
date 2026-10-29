use crate::vft::Text;
use serde::Serialize;

pub const FILL_RUN: usize = 4;
pub const READ_WINDOW: usize = 96;
pub const MAX_PLAUSIBLE_CONST: u32 = 0xffff;
pub const MAX_PLAUSIBLE_ARITY: usize = 16;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Suspect {
    RetsDisagree(Vec<usize>),
    TargetOffTable { target: usize, slots: usize },
    ConstImplausible(u32),
    ArityImplausible(usize),
    ForeignTarget,
    NoExit,
}

#[derive(Debug, Clone, Copy)]
struct Insn {
    off: usize,
    op: u8,
    ext: u8,
    modrm: Option<u8>,
}

impl Insn {
    fn mode(self) -> u8 {
        self.modrm.map_or(3, |m| m >> 6)
    }

    fn reg(self) -> u8 {
        self.modrm.map_or(8, |m| (m >> 3) & 7)
    }
}

fn modrm_len(b: &[u8], i: usize) -> Option<usize> {
    let m = *b.get(i)?;
    let (md, rm) = (m >> 6, m & 7);
    let sib = usize::from(md != 3 && rm == 4);
    let disp = match md {
        0 if rm == 5 => 4,
        0 if sib == 1 && b.get(i + 1).is_some_and(|s| s & 7 == 5) => 4,
        1 => 1,
        2 => 4,
        _ => 0,
    };
    Some(1 + sib + disp)
}

fn test_form(b: &[u8], i: usize) -> Option<bool> {
    b.get(i + 1).map(|m| (m >> 3) & 7 <= 1)
}

fn insn_fields(b: &[u8], i: usize) -> Option<(usize, Option<u8>)> {
    Some(match *b.get(i)? {
        0x40..=0x5f | 0x90..=0x99 | 0xc3 | 0xc9 | 0xcc | 0xf8..=0xfd => (0, None),
        0x00..=0x03
        | 0x08..=0x0b
        | 0x10..=0x13
        | 0x18..=0x1b
        | 0x20..=0x23
        | 0x28..=0x2b
        | 0x30..=0x33
        | 0x38..=0x3b
        | 0x62
        | 0x84..=0x8f
        | 0xd1
        | 0xd3
        | 0xfe
        | 0xff => (modrm_len(b, i + 1)?, b.get(i + 1).copied()),
        0x6b | 0x80 | 0x83 | 0xc0 | 0xc1 | 0xc6 => {
            (modrm_len(b, i + 1)? + 1, b.get(i + 1).copied())
        }
        0xf6 => (
            modrm_len(b, i + 1)? + usize::from(test_form(b, i)?),
            b.get(i + 1).copied(),
        ),
        0xf7 => (
            modrm_len(b, i + 1)? + 4 * usize::from(test_form(b, i)?),
            b.get(i + 1).copied(),
        ),
        0x69 | 0x81 | 0xc7 => (modrm_len(b, i + 1)? + 4, b.get(i + 1).copied()),
        0x04
        | 0x0c
        | 0x14
        | 0x1c
        | 0x24
        | 0x2c
        | 0x34
        | 0x3c
        | 0x6a
        | 0x70..=0x7f
        | 0xa8
        | 0xb0..=0xb7
        | 0xcd
        | 0xeb => (1, None),
        0xc2 => (2, None),
        0x05
        | 0x0d
        | 0x15
        | 0x1d
        | 0x25
        | 0x2d
        | 0x35
        | 0x3d
        | 0x68
        | 0xa0..=0xa3
        | 0xa9
        | 0xb8..=0xbf
        | 0xe8
        | 0xe9 => (4, None),
        0x0f => match *b.get(i + 1)? {
            0x80..=0x8f => (5, None),
            0x40..=0x4f | 0x90..=0x9f | 0xaf | 0xb6 | 0xb7 | 0xbe | 0xbf => {
                (1 + modrm_len(b, i + 2)?, b.get(i + 2).copied())
            }
            _ => return None,
        },
        _ => return None,
    })
}

fn walk(b: &[u8]) -> Option<Vec<Insn>> {
    let mut out = Vec::new();
    let mut i = 0usize;
    while i < b.len() {
        let Some(&op) = b.get(i) else { break };
        let Some((extra, modrm)) = insn_fields(b, i) else {
            break;
        };
        if i + 1 + extra > b.len() {
            break;
        }
        let ext = if op == 0x0f {
            b.get(i + 1).copied().unwrap_or(0)
        } else {
            0
        };
        out.push(Insn {
            off: i,
            op,
            ext,
            modrm,
        });
        i += 1 + extra;
    }
    (!out.is_empty()).then_some(out)
}

fn dispatch_count(b: &[u8]) -> usize {
    walk(b).map_or(0, |ns| {
        ns.iter()
            .filter(|n| n.op == 0xff && matches!(n.reg(), 2 | 4) && n.mode() != 3)
            .count()
    })
}

fn vptr_load_before(b: &[u8], i: usize) -> bool {
    walk(b).is_some_and(|ns| {
        ns.iter()
            .any(|n| n.op == 0x8b && n.mode() != 3 && n.off < i)
    })
}

fn does_other_work(b: &[u8]) -> bool {
    let Some(insns) = walk(b) else { return true };
    insns.iter().any(|n| {
        let foreign_call = n.op == 0xff && matches!(n.reg(), 2 | 3) && !vptr_load_before(b, n.off);
        n.op == 0xe8 || foreign_call || (matches!(n.op, 0x88 | 0x89) && n.mode() != 3)
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Shape {
    Empty,
    Constant(u32),
    Forward { target: usize, discards: bool },
    RealWithDispatch { target: usize },
    Real,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum NoChild {
    Zero,
    ByteZero,
    MinusOne,
    Const(u32),
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Slot {
    pub index: usize,
    pub rva: u32,
    pub arity: usize,
    pub rets: Vec<usize>,
    pub shape: Shape,
    pub nochild: Option<NoChild>,
    pub pushes_object: bool,
}

fn body_end(b: &[u8]) -> usize {
    let mut i = 0usize;
    while i < b.len() {
        let run = b
            .get(i..)
            .unwrap_or(&[])
            .iter()
            .take_while(|&&x| x == 0xcc || x == 0x90)
            .count();
        if run >= FILL_RUN {
            return i;
        }
        i += 1 + run;
    }
    b.len()
}

#[must_use]
pub fn rets(b: &[u8]) -> Vec<usize> {
    let Some(insns) = walk(b) else {
        return Vec::new();
    };
    insns
        .iter()
        .filter_map(|n| match n.op {
            0xc3 => Some(0),
            0xc2 => match (b.get(n.off + 1), b.get(n.off + 2)) {
                (Some(&lo), Some(&hi)) => Some(usize::from(u16::from_le_bytes([lo, hi])) / 4),
                _ => None,
            },
            _ => None,
        })
        .collect()
}

fn forward_at(b: &[u8]) -> Option<(usize, usize)> {
    let insns = walk(b)?;
    for (k, n) in insns.iter().enumerate() {
        if n.op == 0xff && matches!(n.reg(), 2 | 4) && n.mode() != 3 && vptr_load_before(b, n.off) {
            let disp = match n.mode() {
                1 => usize::from(*b.get(n.off + 2)?),
                2 => usize::try_from(u32::from_le_bytes([
                    *b.get(n.off + 2)?,
                    *b.get(n.off + 3)?,
                    *b.get(n.off + 4)?,
                    *b.get(n.off + 5)?,
                ]))
                .ok()?,
                _ => 0,
            };
            let end = insns.get(k + 1).map_or(b.len(), |nx| nx.off);
            return Some((end, disp / 4));
        }
    }
    None
}

fn pushes_object(b: &[u8]) -> bool {
    let Some(insns) = walk(b) else {
        return false;
    };
    let Some(call) = insns
        .iter()
        .position(|n| n.op == 0xff && matches!(n.reg(), 2 | 4) && n.mode() != 3)
    else {
        return false;
    };
    let Some(through) = insns.get(call).and_then(|n| n.modrm).map(|m| m & 7) else {
        return false;
    };
    let Some(load) = insns.get(..call).and_then(|before| {
        before.iter().rposition(|n| {
            n.op == 0x8b && n.mode() != 3 && n.modrm.is_some_and(|m| (m >> 3) & 7 == through)
        })
    }) else {
        return false;
    };
    let Some(object) = insns
        .get(load)
        .and_then(|n| n.modrm)
        .map(|m| m & 7)
        .filter(|rm| *rm != 4 && *rm != 5)
    else {
        return false;
    };
    let before = insns.get(..call).unwrap_or_default();
    let set = before.iter().rposition(|n| writes(*n, object));
    let pushed = before
        .get(set.map_or(0, |w| w + 1)..)
        .unwrap_or_default()
        .iter()
        .any(|n| n.op == 0x50 + object);
    pushed && (object == ECX || !ecx_holds(before, set, object))
}

const ECX: u8 = 1;

fn writes(n: Insn, r: u8) -> bool {
    let (mode, reg, rm) = (n.mode(), n.reg(), n.modrm.map_or(8, |m| m & 7));
    let to_rm = mode == 3 && rm == r;
    let to_rm8 = mode == 3 && rm < 8 && rm & 3 == r;
    match n.op {
        0x40..=0x4f | 0x58..=0x5f | 0xb8..=0xbf => n.op & 7 == r,
        0xb0..=0xb7 => n.op & 3 == r,
        0x91..=0x97 => r == 0 || n.op & 7 == r,
        0x98 | 0xa0 | 0xa1 => r == 0,
        0x99 => r == 2,
        0xe8 => r <= 2,
        0x03 | 0x0b | 0x13 | 0x1b | 0x23 | 0x2b | 0x33 | 0x8b | 0x8d | 0x69 | 0x6b => reg == r,
        0x02 | 0x0a | 0x12 | 0x1a | 0x22 | 0x2a | 0x32 | 0x8a => reg & 3 == r,
        0x01 | 0x09 | 0x11 | 0x19 | 0x21 | 0x29 | 0x31 | 0x89 | 0x8f | 0xc1 | 0xc7 | 0xd1
        | 0xd3 => to_rm,
        0x00 | 0x08 | 0x10 | 0x18 | 0x20 | 0x28 | 0x30 | 0x88 | 0xc0 | 0xc6 | 0xfe => to_rm8,
        0x81 | 0x83 => to_rm && reg != 7,
        0x80 => to_rm8 && reg != 7,
        0xf6 => (to_rm8 && matches!(reg, 2 | 3)) || (reg >= 4 && r == 0),
        0xf7 => (to_rm && matches!(reg, 2 | 3)) || (reg >= 4 && matches!(r, 0 | 2)),
        0xff => to_rm && reg <= 1,
        0x86 => reg & 3 == r || to_rm8,
        0x87 => reg == r || to_rm,
        0x0f if (0x90..=0x9f).contains(&n.ext) => to_rm8,
        0x0f => reg == r,
        _ => false,
    }
}

fn copies(n: Insn, to: u8, from: u8) -> bool {
    let (reg, rm) = (n.reg(), n.modrm.map_or(8, |m| m & 7));
    n.mode() == 3
        && ((n.op == 0x8b && reg == to && rm == from) || (n.op == 0x89 && reg == from && rm == to))
}

fn ecx_holds(before: &[Insn], set: Option<usize>, object: u8) -> bool {
    match before.iter().rposition(|n| writes(*n, ECX)) {
        Some(w) => {
            before.get(w).is_some_and(|n| copies(*n, ECX, object)) && set.is_none_or(|s| s < w)
        }
        None => set
            .and_then(|s| before.get(s))
            .is_some_and(|n| copies(*n, object, ECX)),
    }
}

fn nochild_of(tail: &[u8]) -> Option<NoChild> {
    let insns = walk(tail)?;
    for n in &insns {
        match n.op {
            0x33 if n.modrm == Some(0xc0) => return Some(NoChild::Zero),
            0x32 if n.modrm == Some(0xc0) => return Some(NoChild::ByteZero),
            0x83 if n.modrm == Some(0xc8) && tail.get(n.off + 2) == Some(&0xff) => {
                return Some(NoChild::MinusOne);
            }
            0xb8 => {
                return Some(NoChild::Const(u32::from_le_bytes([
                    *tail.get(n.off + 1)?,
                    *tail.get(n.off + 2)?,
                    *tail.get(n.off + 3)?,
                    *tail.get(n.off + 4)?,
                ])));
            }
            _ => {}
        }
    }
    None
}

#[must_use]
pub fn decode(index: usize, rva: u32, body: &[u8]) -> Slot {
    let b = body.get(..body_end(body)).unwrap_or(body);
    let r = rets(b);
    let arity = r.first().copied().unwrap_or(0);
    let fwd = forward_at(b);
    let after = match fwd {
        Some((end, _)) => b.get(end.min(b.len())..).unwrap_or(&[]),
        None => b,
    };
    let nochild = nochild_of(after);
    let shape = match fwd {
        Some((_, t)) if does_other_work(b) || dispatch_count(b) > 1 => {
            Shape::RealWithDispatch { target: t }
        }
        Some((_, t)) => Shape::Forward {
            target: t,
            discards: r.len() <= 1,
        },
        None if b.len() <= 1 || (b.len() <= 3 && b.first() == Some(&0xc2)) => Shape::Empty,
        None if does_other_work(b) => Shape::Real,
        None => match nochild {
            Some(NoChild::Const(c)) => Shape::Constant(c),
            Some(_) => Shape::Constant(0),
            None => Shape::Real,
        },
    };
    Slot {
        index,
        rva,
        arity,
        rets: r,
        shape,
        nochild,
        pushes_object: pushes_object(b),
    }
}

fn no_exit(body: &[u8]) -> bool {
    rets(body.get(..body_end(body)).unwrap_or(body)).is_empty()
}

fn target_mismatch(shape: &Shape, index: usize) -> bool {
    matches!(shape, Shape::Forward { target, .. } if *target != index)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub slot: Slot,
    pub rel: &'static str,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Map {
    pub rows: Vec<Row>,
    pub suspects: Vec<(usize, Suspect)>,
}

#[must_use]
pub fn map(text: Text<'_>, slots: &[u32], base: Option<&[u32]>) -> Map {
    let mut rows = Vec::new();
    let mut suspects = Vec::new();
    for (i, &raw) in slots.iter().enumerate() {
        let s = text.follow(raw);
        let Some(off) = s
            .checked_sub(text.rva)
            .and_then(|o| usize::try_from(o).ok())
            .filter(|o| *o < text.bytes.len())
        else {
            continue;
        };
        let window = text
            .bytes
            .get(off..text.bytes.len().min(off + READ_WINDOW))
            .unwrap_or(&[]);
        let d = decode(i, s, window);
        let rel = match base {
            Some(b) => match b.get(i) {
                Some(&bs) if text.follow(bs) == s => "inh",
                Some(_) => "ovr",
                None => "new",
            },
            None => "-",
        };
        let distinct: std::collections::BTreeSet<_> = d.rets.iter().collect();
        if distinct.len() > 1 {
            suspects.push((i, Suspect::RetsDisagree(d.rets.clone())));
        }
        if target_mismatch(&d.shape, i) {
            suspects.push((i, Suspect::ForeignTarget));
        }
        if no_exit(window) {
            suspects.push((i, Suspect::NoExit));
        }
        if let Shape::Forward { target, .. } = &d.shape
            && *target >= slots.len()
        {
            suspects.push((
                i,
                Suspect::TargetOffTable {
                    target: *target,
                    slots: slots.len(),
                },
            ));
        }
        if d.arity > MAX_PLAUSIBLE_ARITY {
            suspects.push((i, Suspect::ArityImplausible(d.arity)));
        }
        if let Some(NoChild::Const(c)) = &d.nochild
            && *c > MAX_PLAUSIBLE_CONST
        {
            suspects.push((i, Suspect::ConstImplausible(*c)));
        }
        rows.push(Row { slot: d, rel });
    }
    Map { rows, suspects }
}

fn shape_text(d: &Slot) -> String {
    let shape = match &d.shape {
        Shape::Empty => "EMPTY".to_string(),
        Shape::Constant(c) => format!("return {c:#x}"),
        Shape::Forward { target, discards } => format!(
            "forward -> slot {target}{}{}",
            if *discards { ", DISCARDS" } else { "" },
            match &d.nochild {
                Some(NoChild::Zero) => ", else 0 (int)",
                Some(NoChild::ByteZero) => ", else 0 (BYTE -> char)",
                Some(NoChild::MinusOne) => ", else -1",
                Some(NoChild::Const(_)) => ", else const",
                None => ", else void",
            }
        ),
        Shape::RealWithDispatch { target } => {
            format!("REAL BODY, dispatches slot {target} -- read by hand")
        }
        Shape::Real => "REAL BODY -- read by hand".to_string(),
    };
    let extra = match (&d.shape, &d.nochild) {
        (Shape::Forward { .. }, Some(NoChild::Const(c))) => format!(" ({c:#x})"),
        _ => String::new(),
    };
    format!("{shape}{extra}")
}

#[must_use]
pub fn report(
    m: &Map,
    label: &str,
    table: u32,
    slots: usize,
    base: Option<(u32, usize)>,
) -> Vec<String> {
    let mut out = vec![
        format!("{label}: SLOT MAP of the candidate run at {table:#x} -- {slots} slot(s)"),
        String::new(),
        "  arity from every `ret', forward target from the dispatch displacement over four, the no-child answer from"
            .to_string(),
        "  the constant after it; `?' wants reading by hand. A dispatch that pushes the object it calls through, once"
            .to_string(),
        "  its register holds it and while ECX does not, is a __stdcall virtual call: `this' goes on the stack, so the"
            .to_string(),
        "  callee's `ret' counts it.".to_string(),
    ];
    if let Some((b, n)) = base {
        out.push(format!(
            "  BASE {b:#x} has {n} slot(s): `inh' holds the base's own body, `ovr' overrides it, `new' is past it."
        ));
    }
    out.push(String::new());
    out.push(format!(
        "  {:>4} {:>9} {:>4} {:>4}  {:<34} rets",
        "slot", "rva", "ar", "rel", "shape"
    ));
    for r in &m.rows {
        let d = &r.slot;
        let mut shape = shape_text(d);
        if d.pushes_object {
            shape.push_str(", __stdcall (pushes its object)");
        }
        out.push(format!(
            "  {:>4} {:>9} {:>4} {:>4}  {:<34} {:?}",
            d.index,
            format!("{:#x}", d.rva),
            d.arity,
            r.rel,
            shape,
            d.rets
        ));
    }
    if m.suspects.is_empty() {
        out.push(String::new());
        out.push(
            "  NO SUSPECT ROW. Every body's `ret's agree on one arity and every forward targets ITS OWN slot."
                .to_string(),
        );
        return out;
    }
    out.push(String::new());
    out.push(format!(
        "  {} SUSPECT ROW(S) -- read these by hand before generating:",
        m.suspects.len()
    ));
    for (i, why) in &m.suspects {
        out.push(match why {
            Suspect::ForeignTarget => format!(
                "    slot {i}: forwards to a DIFFERENT slot than its own -- not the same-slot template"
            ),
            Suspect::RetsDisagree(r) => format!(
                "    slot {i}: `ret' immediates DISAGREE {r:?} -- the read window or the body is wrong"
            ),
            Suspect::TargetOffTable { target, slots } => format!(
                "    slot {i}: forwards to slot {target} but this table has only {slots} -- the dispatch was read mid-instruction"
            ),
            Suspect::ArityImplausible(a) => format!(
                "    slot {i}: `ret' implies {a} argument(s) -- past {MAX_PLAUSIBLE_ARITY}, so these bytes are not the body's exit"
            ),
            Suspect::ConstImplausible(c) => format!(
                "    slot {i}: no-child constant {c:#x} is too large for an error code -- read mid-instruction"
            ),
            Suspect::NoExit => format!(
                "    slot {i}: NO `ret' AT ALL -- these bytes are not a body. Suspect an unfollowed ILT thunk or a slot dword that is not code."
            ),
        });
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    const SLOT25: &[u8] = &[
        0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x05, 0x8b, 0x01, 0xff, 0x50, 0x64, 0x33, 0xc0, 0xc3,
        0x90, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    ];

    const THUNKS: &[u8] = &[
        0xe9, 0xe5, 0x7a, 0x02, 0x00, 0xe9, 0x30, 0x67, 0x03, 0x00, 0xe9, 0xbb, 0x39, 0x02, 0x00,
        0xe9,
    ];

    const SLOT42: &[u8] = &[
        0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x52, 0xff,
        0x90, 0xa8, 0x00, 0x00, 0x00, 0xc2, 0x04, 0x00, 0xb8, 0x14, 0x00, 0x00, 0x00, 0xc2, 0x04,
        0x00, 0x90, 0xcc, 0xcc, 0xcc, 0xcc,
    ];

    const NOT_A_FORWARD: &[u8] = &[
        0x8a, 0x44, 0x24, 0x08, 0x56, 0xa8, 0x02, 0x8b, 0xf1, 0x74, 0x1f, 0x6a, 0x00, 0x6a, 0x00,
        0x6a, 0x00, 0x8d, 0x46, 0x3c, 0x6a, 0xff, 0x50, 0xff, 0x15, 0xf8, 0x85, 0x0b, 0x10, 0x85,
        0xc0, 0x74, 0x09, 0xb8, 0x07, 0x00, 0x00, 0x00, 0x5e, 0xc2, 0x08, 0x00,
    ];

    #[test]
    fn a_ret_immediate_over_four_is_the_arity_and_every_exit_is_read() {
        assert_eq!(rets(&[0xc3]), vec![0]);
        assert_eq!(rets(&[0xc2, 0x0c, 0x00]), vec![3]);
        assert_eq!(
            rets(&[0xc2, 0x04, 0x00, 0x90, 0xc2, 0x04, 0x00]),
            vec![1, 1]
        );
        assert_eq!(rets(&[0xc2, 0x04, 0x01]), vec![65]);
        assert_eq!(rets(&[0xb9, 0xc2, 0x04, 0x01, 0x00, 0xc3]), vec![0]);
    }

    #[test]
    fn only_a_run_of_fill_ends_a_body() {
        assert_eq!(body_end(SLOT25), 15);
        let disp = [0x8b, 0x89, 0xcc, 0x00, 0x00, 0x00, 0xc3];
        assert_eq!(body_end(&disp), disp.len());
    }

    #[test]
    fn the_dispatch_displacement_over_four_is_the_forwarded_slot() {
        assert_eq!(forward_at(SLOT25).map(|f| f.1), Some(25));
        assert_eq!(forward_at(SLOT42).map(|f| f.1), Some(42));
        assert_eq!(forward_at(&[0x8b, 0x01, 0xc3]), None);
        assert_eq!(forward_at(NOT_A_FORWARD), None);
        assert_eq!(
            rets(
                NOT_A_FORWARD
                    .get(..body_end(NOT_A_FORWARD))
                    .unwrap_or_default()
            ),
            vec![2]
        );
    }

    #[test]
    fn a_same_slot_forwarder_decodes_whole() {
        let s = decode(25, 0x29660, SLOT25);
        assert_eq!(
            (s.arity, s.rets.clone(), s.nochild.clone()),
            (0, vec![0], Some(NoChild::Zero))
        );
        assert_eq!(
            s.shape,
            Shape::Forward {
                target: 25,
                discards: true
            }
        );
        assert!(!target_mismatch(&s.shape, 25));
        assert!(target_mismatch(&s.shape, 24));
        let one = decode(42, 0x29e00, SLOT42);
        assert_eq!(
            one.shape,
            Shape::Forward {
                target: 42,
                discards: false
            }
        );
        assert_eq!(one.nochild, Some(NoChild::Const(0x14)));
    }

    #[test]
    fn the_no_child_answer_is_read_from_the_tail() {
        assert_eq!(nochild_of(&[0x32, 0xc0, 0xc3]), Some(NoChild::ByteZero));
        assert_eq!(
            nochild_of(&[0x83, 0xc8, 0xff, 0xc3]),
            Some(NoChild::MinusOne)
        );
        assert_eq!(nochild_of(&[0x8b, 0x01, 0xc3]), None);
    }

    #[test]
    fn an_unfollowed_thunk_lattice_has_no_exit() {
        assert!(rets(THUNKS).is_empty());
        assert!(no_exit(THUNKS));
        assert!(!no_exit(SLOT25));
    }

    #[test]
    fn a_dispatch_that_pushes_its_object_is_a_stdcall_virtual_call() {
        let stdcall = [
            0x8b, 0x41, 0x60, 0x8b, 0x08, 0x57, 0x50, 0xff, 0x51, 0x34, 0xc2, 0x04, 0x00,
        ];
        let s = decode(3, 0x1000, &stdcall);
        assert!(s.pushes_object);
        assert_eq!(
            s.shape,
            Shape::Forward {
                target: 13,
                discards: true
            }
        );
        let compiled = [
            0x8b, 0x41, 0x04, 0x8b, 0x54, 0x24, 0x04, 0x52, 0x50, 0x8b, 0x08, 0xff, 0x11, 0xc2,
            0x04, 0x00,
        ];
        assert!(decode(0, 0x1000, &compiled).pushes_object);
        let thiscall = [
            0x8b, 0x49, 0x60, 0x8b, 0x01, 0x57, 0xff, 0x50, 0x34, 0xc2, 0x04, 0x00,
        ];
        assert!(!decode(3, 0x1000, &thiscall).pushes_object);
        assert!(!decode(25, 0x1000, SLOT25).pushes_object);
        let saved_before_set = [
            0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0x06, 0x57, 0xff, 0x50, 0x04,
            0x85, 0xc0, 0x75, 0x24, 0x8b, 0x4e, 0x14, 0x8b, 0x44, 0x24, 0x0c, 0x57, 0x50, 0x8b,
            0x11, 0xff, 0x52, 0x10, 0x8b, 0xf8, 0x85, 0xff, 0x74, 0x0e, 0x8b, 0x16, 0x8b, 0xce,
            0xff, 0x52, 0x08, 0x8b, 0xc7, 0x5f,
        ];
        assert!(!pushes_object(&saved_before_set));
        let loaded_after_save = [
            0x51, 0x8b, 0x41, 0x14, 0x55, 0x57, 0x33, 0xed, 0x33, 0xff, 0x3b, 0xc5, 0x89, 0x4c,
            0x24, 0x08, 0x75, 0x0b, 0x5f, 0xb8, 0x02, 0x00, 0x00, 0x00, 0x5d, 0x59, 0xc2, 0x04,
            0x00, 0x56, 0x8b, 0x35, 0x70, 0x50, 0x0b, 0x10, 0x3b, 0xf5, 0x74, 0x55, 0x53, 0x8b,
            0x06, 0x8b, 0xce, 0xff, 0x50, 0x70,
        ];
        assert!(!pushes_object(&loaded_after_save));
        let also_in_ecx = [0x8b, 0xf1, 0x8b, 0x06, 0x56, 0xff, 0x50, 0x08, 0xc3];
        assert!(!pushes_object(&also_in_ecx));
        let pushed_after_set = [
            0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0x06, 0x56, 0xff, 0x50, 0x08, 0x5e, 0xc3,
        ];
        assert!(pushes_object(&pushed_after_set));
        let text = Text {
            bytes: &stdcall,
            rva: 0x1000,
        };
        let lines = report(&map(text, &[0x1000], None), "IMG", 0x2000, 1, None);
        assert!(
            lines
                .iter()
                .any(|l| l.contains("__stdcall (pushes its object)")),
            "{lines:#?}"
        );
    }

    #[test]
    fn every_register_write_the_decoder_reads_is_the_object_s_last_write() {
        for (body, pushes, what) in [
            (
                &[0x56, 0x8f, 0xc6, 0x8b, 0x06, 0xff, 0x50, 0x08, 0xc3][..],
                false,
                "pop esi (8f /0) after the push",
            ),
            (
                &[
                    0x50, 0xa1, 0x00, 0x10, 0x00, 0x00, 0x8b, 0x08, 0xff, 0x51, 0x08, 0xc3,
                ][..],
                false,
                "mov eax, moffs32 after the push",
            ),
            (
                &[
                    0x96, 0x8b, 0x74, 0x24, 0x08, 0x56, 0x8b, 0x06, 0xff, 0x50, 0x08, 0xc3,
                ][..],
                true,
                "xchg eax, esi before the set",
            ),
            (
                &[
                    0x8b, 0x74, 0x24, 0x08, 0x56, 0x96, 0x8b, 0x06, 0xff, 0x50, 0x08, 0xc3,
                ][..],
                false,
                "xchg eax, esi after the push",
            ),
            (
                &[0x50, 0xf7, 0xe1, 0x8b, 0x08, 0xff, 0x51, 0x08, 0xc3][..],
                false,
                "mul ecx (f7 /4) writes eax after the push",
            ),
            (
                &[0x52, 0xf7, 0xf9, 0x8b, 0x0a, 0xff, 0x51, 0x08, 0xc3][..],
                false,
                "idiv ecx (f7 /7) writes edx after the push",
            ),
            (
                &[
                    0xf7, 0xe1, 0x8b, 0x44, 0x24, 0x08, 0x50, 0x8b, 0x08, 0xff, 0x51, 0x08, 0xc3,
                ][..],
                true,
                "mul ecx before the set",
            ),
            (
                &[
                    0x8b, 0x44, 0x24, 0x08, 0x50, 0x0f, 0x94, 0xc1, 0x8b, 0x08, 0xff, 0x51, 0x08,
                    0xc3,
                ][..],
                true,
                "sete cl writes its r/m, not eax",
            ),
            (
                &[
                    0x8b, 0x74, 0x24, 0x08, 0x56, 0xf6, 0xd8, 0x8b, 0x06, 0xff, 0x50, 0x08, 0xc3,
                ][..],
                true,
                "neg al (f6 /3) has no immediate",
            ),
            (
                &[
                    0x8b, 0x44, 0x24, 0x08, 0x50, 0xb4, 0x01, 0x8b, 0x08, 0xff, 0x51, 0x08, 0xc3,
                ][..],
                false,
                "mov ah, 1 writes eax after the push",
            ),
            (
                &[
                    0x8b, 0x74, 0x24, 0x08, 0x56, 0x8a, 0x35, 0x00, 0x10, 0x00, 0x00, 0x8b, 0x06,
                    0xff, 0x50, 0x08, 0xc3,
                ][..],
                true,
                "mov dh, [m] writes edx, not esi",
            ),
        ] {
            assert_eq!(pushes_object(body), pushes, "{what}");
        }
    }
}
