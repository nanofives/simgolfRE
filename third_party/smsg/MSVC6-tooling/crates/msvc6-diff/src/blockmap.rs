use crate::insndiff::{Kind, Pair};
use iced_x86::{Decoder, DecoderOptions, FlowControl, Instruction, OpKind, Register};
use std::collections::BTreeSet;
use std::fmt::Write as _;

#[derive(Debug, Clone, Default)]
pub struct Views {
    pub block_map: bool,
    pub window: Option<u32>,
    pub align: bool,
    pub step: u32,
    pub keep_regs: bool,
}

impl Views {
    #[must_use]
    pub fn any(&self) -> bool {
        self.block_map || self.align
    }
}

#[must_use]
pub fn strip_pad(b: &[u8]) -> &[u8] {
    let n = b
        .iter()
        .rposition(|x| *x != 0x90 && *x != 0xcc)
        .map_or(0, |p| p + 1);
    b.get(..n).unwrap_or(b)
}

#[derive(Debug, Default)]
pub struct Code {
    pub ins: Vec<Instruction>,
    pub code_end: u32,
    pub tables: Vec<(u32, u32)>,
    pub table_targets: Vec<u32>,
}

fn decode_all(bytes: &[u8], ip: u64) -> Vec<Instruction> {
    let mut d = Decoder::with_ip(32, bytes, ip, DecoderOptions::NONE);
    let mut out = Vec::new();
    while d.can_decode() {
        out.push(d.decode());
    }
    out
}

fn read_u32(b: &[u8], at: usize) -> Option<u32> {
    b.get(at..at.checked_add(4)?)
        .and_then(|s| <[u8; 4]>::try_from(s).ok())
        .map(u32::from_le_bytes)
}

fn offset_in(i: &Instruction, ip: u32) -> u32 {
    u32::try_from(i.ip()).unwrap_or(0).saturating_sub(ip)
}

#[must_use]
pub fn split(
    bytes: &[u8],
    ip: u32,
    table_base: u32,
    self_refs: Option<&[(u32, u32)]>,
    code_limit: u32,
) -> Code {
    let len = u32::try_from(bytes.len()).unwrap_or(u32::MAX);
    let limit = code_limit.min(len);
    let first = decode_all(bytes.get(..limit as usize).unwrap_or(bytes), u64::from(ip));
    let mut starts: BTreeSet<u32> = BTreeSet::new();
    let mut dword: BTreeSet<u32> = BTreeSet::new();
    for i in &first {
        let has_mem = (0..i.op_count()).any(|k| i.op_kind(k) == OpKind::Memory);
        if !has_mem || (i.memory_base() != Register::None && i.memory_index() != Register::None) {
            continue;
        }
        let here = offset_in(i, ip);
        let disp = i.memory_displacement32();
        let off = match self_refs {
            None => disp.checked_sub(table_base),
            Some(refs) => {
                let end = here.saturating_add(u32::try_from(i.len()).unwrap_or(0));
                refs.iter()
                    .find(|(f, _)| *f >= here && *f < end)
                    .map(|(_, t)| disp.wrapping_add(*t))
            }
        };
        let Some(off) = off else {
            continue;
        };
        if off < len && off > here {
            starts.insert(off);
            if i.memory_index_scale() == 4 && i.flow_control() == FlowControl::IndirectBranch {
                dword.insert(off);
            }
        }
    }
    let code_end = starts.first().copied().unwrap_or(limit).min(limit);
    let ins = decode_all(
        bytes.get(..code_end as usize).unwrap_or(bytes),
        u64::from(ip),
    );
    let bounds: Vec<u32> = starts.iter().copied().chain(std::iter::once(len)).collect();
    let tables: Vec<(u32, u32)> = bounds
        .windows(2)
        .filter_map(|w| Some((*w.first()?, *w.get(1)?)))
        .collect();
    let mut table_targets = Vec::new();
    for (lo, hi) in &tables {
        if !dword.contains(lo) {
            continue;
        }
        let mut at = *lo;
        while at.saturating_add(4) <= *hi {
            let Some(v) = read_u32(bytes, at as usize) else {
                break;
            };
            match v.checked_sub(table_base) {
                Some(t) if t < code_end => table_targets.push(t),
                _ => break,
            }
            at = at.saturating_add(4);
        }
    }
    Code {
        ins,
        code_end,
        tables,
        table_targets,
    }
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Block {
    pub start: u32,
    pub end: u32,
    pub insns: usize,
    pub wrong: usize,
    pub reg: usize,
    pub ord: usize,
    pub ours_only: usize,
}

impl Block {
    #[must_use]
    pub fn flagged(&self) -> usize {
        self.wrong + self.reg + self.ord + self.ours_only
    }

    #[must_use]
    pub fn bytes(&self) -> u32 {
        self.end.saturating_sub(self.start)
    }
}

#[must_use]
pub fn blocks(code: &Code, ip: u32) -> Vec<Block> {
    let mut leaders: BTreeSet<u32> = BTreeSet::new();
    leaders.insert(0);
    leaders.extend(code.table_targets.iter().copied());
    for (k, i) in code.ins.iter().enumerate() {
        let fc = i.flow_control();
        if matches!(
            fc,
            FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch
        ) && let Some(t) = u32::try_from(i.near_branch_target())
            .ok()
            .and_then(|t| t.checked_sub(ip))
            && t < code.code_end
        {
            leaders.insert(t);
        }
        if matches!(
            fc,
            FlowControl::ConditionalBranch
                | FlowControl::UnconditionalBranch
                | FlowControl::IndirectBranch
                | FlowControl::Return
        ) && let Some(n) = code.ins.get(k + 1)
        {
            leaders.insert(offset_in(n, ip));
        }
    }
    let mut out: Vec<Block> = Vec::new();
    for i in &code.ins {
        let o = offset_in(i, ip);
        if leaders.contains(&o) || out.is_empty() {
            if let Some(b) = out.last_mut() {
                b.end = o;
            }
            out.push(Block {
                start: o,
                ..Block::default()
            });
        }
        if let Some(b) = out.last_mut() {
            b.insns += 1;
        }
    }
    if let Some(b) = out.last_mut() {
        b.end = code.code_end;
    }
    out
}

pub fn attribute(blocks: &mut [Block], rows: &[Pair], ip: u32) -> usize {
    let mut in_tables = 0usize;
    for p in rows {
        let Ok(at) = u32::try_from(p.at) else {
            continue;
        };
        let o = at.saturating_sub(ip);
        let k = blocks.partition_point(|b| b.start <= o);
        let Some(b) = k
            .checked_sub(1)
            .and_then(|k| blocks.get_mut(k))
            .filter(|b| o < b.end)
        else {
            if p.kind != Kind::RelocatedOnly {
                in_tables += 1;
            }
            continue;
        };
        match p.kind {
            Kind::RelocatedOnly => {}
            Kind::Different => b.wrong += 1,
            Kind::RegisterOnly => b.reg += 1,
            Kind::Moved if p.want.is_empty() => b.ours_only += 1,
            Kind::Moved => b.ord += 1,
        }
    }
    in_tables
}

fn pct(a: u64, b: u64) -> u64 {
    a.saturating_mul(100).checked_div(b).unwrap_or(0)
}

#[must_use]
pub fn render_block_map(
    blocks: &[Block],
    code: &Code,
    in_tables: usize,
    ip: u32,
    window: Option<u32>,
) -> String {
    let mut s = String::new();
    let exact: Vec<&Block> = blocks.iter().filter(|b| b.flagged() == 0).collect();
    let total: u32 = blocks.iter().map(Block::bytes).fold(0, u32::saturating_add);
    let exact_bytes: u32 = exact.iter().map(|b| b.bytes()).fold(0, u32::saturating_add);
    let sum = |f: fn(&Block) -> usize| blocks.iter().map(f).sum::<usize>();
    let _ = writeln!(
        s,
        "      blocks {}, exact {}, exact bytes {exact_bytes} of {total} ({}%)",
        blocks.len(),
        exact.len(),
        pct(u64::from(exact_bytes), u64::from(total))
    );
    let _ = writeln!(
        s,
        "      instructions {}: wrong {}, reg {}, ord {}, ours-only {}",
        sum(|b| b.insns),
        sum(|b| b.wrong),
        sum(|b| b.reg),
        sum(|b| b.ord),
        sum(|b| b.ours_only)
    );
    if !code.tables.is_empty() {
        let tb: u32 = code
            .tables
            .iter()
            .map(|(a, b)| b.saturating_sub(*a))
            .fold(0, u32::saturating_add);
        let _ = writeln!(
            s,
            "      jump/index tables: {} ({tb} bytes from +{:#x}), {} table target(s) as leaders; {in_tables} \
             diff row(s) fell inside a table and are not counted above",
            code.tables.len(),
            code.code_end,
            code.table_targets.len()
        );
    }
    if let Some(w) = window.filter(|w| *w > 0) {
        let _ = writeln!(
            s,
            "      per {w:#x}-byte window (a block counts in the window it STARTS in; empty windows skipped):"
        );
        let mut lo = 0u32;
        while lo < code.code_end {
            let hi = lo.saturating_add(w);
            let inw: Vec<&Block> = blocks
                .iter()
                .filter(|b| b.start >= lo && b.start < hi)
                .collect();
            if !inw.is_empty() {
                let bytes: u32 = inw.iter().map(|b| b.bytes()).fold(0, u32::saturating_add);
                let ex: Vec<&&Block> = inw.iter().filter(|b| b.flagged() == 0).collect();
                let exb: u32 = ex.iter().map(|b| b.bytes()).fold(0, u32::saturating_add);
                let _ = writeln!(
                    s,
                    "      {:#07x}..{:#07x} blocks {:4} exact {:4}  instr {:5} flagged {:5}  bytes {:6} exact-bytes {:6} ({:3}%)",
                    ip.saturating_add(lo),
                    ip.saturating_add(hi),
                    inw.len(),
                    ex.len(),
                    inw.iter().map(|b| b.insns).sum::<usize>(),
                    inw.iter().map(|b| b.flagged()).sum::<usize>(),
                    bytes,
                    exb,
                    pct(u64::from(exb), u64::from(bytes))
                );
            }
            if hi == lo {
                break;
            }
            lo = hi;
        }
    }
    let _ = writeln!(
        s,
        "      LIMITATION: rows are placed by ADDRESS. Once the two lengths drift, every row after the drift \
         is attributed to the wrong block (an ours-only row by OUR offset). `--align` localises the drift."
    );
    s
}

#[must_use]
pub fn shape(i: &Instruction, keep_regs: bool) -> String {
    let reg = |r: Register| {
        if !keep_regs && r.is_gpr() {
            "R".to_string()
        } else {
            format!("{r:?}")
        }
    };
    let mut s = format!("{:?}", i.mnemonic());
    for k in 0..i.op_count() {
        s.push(' ');
        match i.op_kind(k) {
            OpKind::Register => s.push_str(&reg(i.op_register(k))),
            OpKind::Memory => {
                let _ = write!(s, "{:?}[", i.memory_size());
                if i.memory_base() != Register::None {
                    s.push_str(&reg(i.memory_base()));
                    s.push('+');
                }
                if i.memory_index() != Register::None {
                    let _ = write!(s, "{}*{}+", reg(i.memory_index()), i.memory_index_scale());
                }
                s.push_str("N]");
            }
            OpKind::Immediate8
            | OpKind::Immediate8_2nd
            | OpKind::Immediate16
            | OpKind::Immediate32
            | OpKind::Immediate64
            | OpKind::Immediate8to16
            | OpKind::Immediate8to32
            | OpKind::Immediate8to64
            | OpKind::Immediate32to64
            | OpKind::NearBranch16
            | OpKind::NearBranch32
            | OpKind::NearBranch64
            | OpKind::FarBranch16
            | OpKind::FarBranch32 => s.push('N'),
            other => {
                let _ = write!(s, "{other:?}");
            }
        }
    }
    s
}

#[must_use]
pub fn anchors(
    img: &[Instruction],
    ours: &[Instruction],
    img_ip: u32,
    keep_regs: bool,
) -> Vec<(u32, u32)> {
    let a: Vec<String> = img.iter().map(|i| shape(i, keep_regs)).collect();
    let b: Vec<String> = ours.iter().map(|i| shape(i, keep_regs)).collect();
    let mut out = Vec::new();
    for op in similar::capture_diff_slices(similar::Algorithm::Myers, &a, &b) {
        if let similar::DiffOp::Equal {
            old_index,
            new_index,
            len,
        } = op
        {
            for d in 0..len {
                if let (Some(x), Some(y)) = (img.get(old_index + d), ours.get(new_index + d)) {
                    out.push((offset_in(x, img_ip), u32::try_from(y.ip()).unwrap_or(0)));
                }
            }
        }
    }
    out
}

fn delta(p: (u32, u32)) -> i64 {
    i64::from(p.1) - i64::from(p.0)
}

#[must_use]
pub fn render_align(img: &Code, ours: &Code, img_ip: u32, step: u32, keep_regs: bool) -> String {
    let mut s = String::new();
    let shape_m = anchors(&img.ins, &ours.ins, img_ip, false);
    let exact_m = anchors(&img.ins, &ours.ins, img_ip, true);
    let _ = writeln!(
        s,
        "      image instr {} (code +{:#x}), ours {} (code +{:#x}); shape-matched {}, exact-register-matched {}",
        img.ins.len(),
        img.code_end,
        ours.ins.len(),
        ours.code_end,
        shape_m.len(),
        exact_m.len()
    );
    if shape_m.len() > exact_m.len() {
        let _ = writeln!(
            s,
            "      {} instruction(s) match in SHAPE but not in registers: allocation, not statements",
            shape_m.len() - exact_m.len()
        );
    }
    let rows = if keep_regs { &exact_m } else { &shape_m };
    let step = step.max(1);
    let mut next = 0u32;
    let mut prev = 0i64;
    let mut steps = 0usize;
    for p in rows {
        if p.0 >= next {
            let d = delta(*p);
            if d != prev {
                steps += 1;
            }
            let _ = writeln!(
                s,
                "      img +{:#06x}  delta {d:+}  (change {:+})",
                p.0,
                d - prev
            );
            prev = d;
            next = (p.0 / step).saturating_add(1).saturating_mul(step);
        }
    }
    let fin = rows.last().map_or(0, |p| delta(*p));
    let _ = writeln!(
        s,
        "      final delta {fin:+}; code-length delta {:+}; {steps} row(s) where the delta changed ({} alignment)",
        i64::from(ours.code_end) - i64::from(img.code_end),
        if keep_regs {
            "exact-register"
        } else {
            "register-normalised"
        }
    );
    s
}

#[must_use]
pub fn table_refs(c: &crate::comdat::Comdat) -> Vec<(u32, u32)> {
    c.relocations
        .iter()
        .filter(|r| {
            r.kind == crate::comdat::REL_DIR32
                && r.target_section
                    .as_deref()
                    .is_some_and(|t| t.starts_with(".text"))
        })
        .map(|r| (r.at, r.target_offset))
        .collect()
}

#[must_use]
pub fn render_views(env: &crate::env::Env, m: &crate::score::Scored, views: &Views) -> String {
    use crate::score::Seat;
    let mut s = String::new();
    if !views.any()
        || !matches!(
            m.seat,
            Seat::Given
                | Seat::Image
                | Seat::Padded { .. }
                | Seat::NopTail { .. }
                | Seat::JumpTable { .. }
                | Seat::Unrecorded
        )
    {
        return s;
    }
    let got = &m.comdat.bytes;
    let recorded = env.recorded(m.at);
    if m.bad.is_empty() && recorded == u32::try_from(got.len()).ok() {
        let _ = writeln!(
            s,
            "      (views: exact at its recorded length -- nothing to map)"
        );
        return s;
    }
    let (ext, whence) = match recorded {
        Some(l) => (l, "recorded extent"),
        None => (
            u32::try_from(got.len()).unwrap_or(0),
            "NO RECORDED EXTENT -- our length, so a length error is invisible here",
        ),
    };
    let next = env
        .seats
        .iter()
        .find(|(r, _)| *r > m.at)
        .map_or(ext, |(r, _)| r.saturating_sub(m.at).max(ext));
    let off = m.at.saturating_sub(env.text_rva) as usize;
    let Some(raw) = env.text().get(off..off.saturating_add(next as usize)) else {
        let _ = writeln!(s, "      (views: {:#x} +{next:#x} is outside .text)", m.at);
        return s;
    };
    let want = strip_pad(raw);
    let base = u32::try_from(env.base).unwrap_or(0);
    let img_code = split(want, m.at, base.wrapping_add(m.at), None, ext);
    let want = if img_code.tables.is_empty() {
        want.get(..img_code.code_end as usize).unwrap_or(want)
    } else {
        want
    };
    let self_refs = table_refs(&m.comdat);
    let ours = strip_pad(got);
    let our_code = split(ours, 0, 0, Some(&self_refs), u32::MAX);
    if views.block_map {
        let _ = writeln!(
            s,
            "      BLOCK MAP of image {:#x} +{ext:#x} code, +{:#x} with tables ({whence}; rows re-diffed against it):",
            m.at,
            want.len()
        );
        let d = crate::insndiff::compare(want, got, &m.comdat.relocated, u64::from(m.at));
        let mut bl = blocks(&img_code, m.at);
        let in_tables = attribute(&mut bl, &d.pairs, m.at);
        s.push_str(&render_block_map(
            &bl,
            &img_code,
            in_tables,
            m.at,
            views.window,
        ));
    }
    if views.align {
        let _ = writeln!(
            s,
            "      ALIGN image {:#x} +{ext:#x} ({whence}) against ours +{:#x}, every {:#x} image bytes:",
            m.at,
            ours.len(),
            views.step
        );
        s.push_str(&render_align(
            &img_code,
            &our_code,
            m.at,
            views.step,
            views.keep_regs,
        ));
    }
    s
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn padding_is_stripped_from_the_end_only() {
        assert_eq!(strip_pad(&[0x90, 0xc3, 0xcc, 0xcc, 0x90]), &[0x90, 0xc3]);
        assert_eq!(strip_pad(&[0xcc]), &[] as &[u8]);
    }

    #[test]
    fn a_branch_starts_two_blocks() {
        let b = [0x83, 0xf8, 0x01, 0x74, 0x02, 0x33, 0xc0, 0xc3];
        let code = split(&b, 0x1000, 0x40_1000, None, u32::MAX);
        let bl = blocks(&code, 0x1000);
        let starts: Vec<u32> = bl.iter().map(|b| b.start).collect();
        assert_eq!(starts, vec![0, 5, 7]);
        assert_eq!(bl.last().map(|b| b.end), Some(8));
    }

    fn table_function() -> Vec<u8> {
        let mut b = vec![
            0xff, 0x24, 0x85, 0x10, 0x10, 0x40, 0x00, 0x33, 0xc0, 0xc3, 0x40, 0xc3, 0xcc, 0xcc,
            0xcc, 0xcc,
        ];
        b.extend_from_slice(&0x0040_1007u32.to_le_bytes());
        b.extend_from_slice(&0x0040_100au32.to_le_bytes());
        b
    }

    #[test]
    fn a_jump_table_is_data_and_its_targets_are_leaders() {
        let b = table_function();
        let code = split(&b, 0x1000, 0x40_1000, None, u32::MAX);
        assert_eq!(code.code_end, 16);
        assert_eq!(code.tables, vec![(16, 24)]);
        assert_eq!(code.table_targets, vec![7, 10]);
        let starts: Vec<u32> = blocks(&code, 0x1000).iter().map(|b| b.start).collect();
        assert_eq!(starts, vec![0, 7, 10, 12]);
    }

    #[test]
    fn a_table_past_the_recorded_extent_is_still_a_table() {
        let b = table_function();
        let code = split(&b, 0x1000, 0x40_1000, None, 12);
        assert_eq!(code.code_end, 12);
        assert_eq!(code.tables, vec![(16, 24)]);
        assert_eq!(code.table_targets, vec![7, 10]);
    }

    #[test]
    fn our_table_needs_a_self_relocation() {
        let mut b = vec![
            0xff, 0x24, 0x85, 0x10, 0x00, 0x00, 0x00, 0x33, 0xc0, 0xc3, 0x40, 0xc3, 0xcc, 0xcc,
            0xcc, 0xcc,
        ];
        b.extend_from_slice(&7u32.to_le_bytes());
        b.extend_from_slice(&10u32.to_le_bytes());
        let with = split(&b, 0, 0, Some(&[(3, 0)]), u32::MAX);
        assert_eq!(with.tables, vec![(16, 24)]);
        assert_eq!(with.table_targets, vec![7, 10]);
        let without = split(&b, 0, 0, Some(&[]), u32::MAX);
        assert!(without.tables.is_empty());
    }

    #[test]
    fn an_interior_drift_that_cancels_is_localised() {
        let one = [0x8b, 0x45, 0xf8];
        let mut img: Vec<u8> = one.iter().copied().cycle().take(48).collect();
        img.extend_from_slice(&[0x40, 0xc3]);
        let mut ours: Vec<u8> = one.iter().copied().cycle().take(24).collect();
        ours.push(0x50);
        ours.extend(one.iter().copied().cycle().take(24));
        ours.push(0xc3);
        let a = split(&img, 0x2000, 0x40_2000, None, u32::MAX);
        let b = split(&ours, 0, 0, Some(&[]), u32::MAX);
        let t = render_align(&a, &b, 0x2000, 0x10, false);
        assert!(t.contains("delta +1  (change +1)"), "{t}");
        assert!(t.contains("delta +0  (change -1)"), "{t}");
        assert!(
            t.contains("final delta +0; code-length delta +0; 2 row(s)"),
            "{t}"
        );
    }

    #[test]
    fn a_register_permutation_is_shape_equal() {
        let a = split(&[0x8b, 0x45, 0xf8, 0xc3], 0x10, 0x40_0010, None, u32::MAX);
        let b = split(&[0x8b, 0x4d, 0xf8, 0xc3], 0, 0, Some(&[]), u32::MAX);
        let t = render_align(&a, &b, 0x10, 0x400, false);
        assert!(
            t.contains("shape-matched 2, exact-register-matched 1"),
            "{t}"
        );
    }

    #[test]
    fn rows_are_placed_in_their_blocks() {
        let b = [0x83, 0xf8, 0x01, 0x74, 0x02, 0x33, 0xc0, 0xc3];
        let code = split(&b, 0x1000, 0x40_1000, None, u32::MAX);
        let mut bl = blocks(&code, 0x1000);
        let row = |at: u64, kind: Kind, want: &str| Pair {
            at,
            kind,
            want: want.to_string(),
            got: String::new(),
        };
        let rows = [
            row(0x1005, Kind::Different, "xor"),
            row(0x1007, Kind::RegisterOnly, "ret"),
            row(0x1006, Kind::Moved, ""),
            row(0x1020, Kind::Different, "x"),
        ];
        let outside = attribute(&mut bl, &rows, 0x1000);
        assert_eq!(outside, 1);
        assert_eq!(bl.get(1).map(|b| (b.wrong, b.ours_only)), Some((1, 1)));
        assert_eq!(bl.get(2).map(|b| b.reg), Some(1));
        let t = render_block_map(&bl, &code, outside, 0x1000, Some(4));
        assert!(
            t.contains("blocks 3, exact 1, exact bytes 5 of 8 (62%)"),
            "{t}"
        );
        assert!(t.contains("0x01000..0x01004 blocks    1"), "{t}");
    }

    #[test]
    fn a_hostile_split_is_bounded() {
        let mut b = vec![0xff, 0x24, 0x85, 0x08, 0, 0, 0];
        b.push(0xc3);
        b.extend(std::iter::repeat_n(0u8, 0x1000));
        let code = split(&b, 0, 0, None, u32::MAX);
        assert_eq!(code.code_end, 8);
        assert_eq!(code.table_targets.len(), 0x400);
        let code = split(&[0xe9, 0xff, 0xff, 0xff, 0x7f], u32::MAX, 0, None, u32::MAX);
        let bl = blocks(&code, u32::MAX);
        assert_eq!(bl.len(), 1);
        let t = render_block_map(&bl, &code, 0, u32::MAX, Some(u32::MAX));
        assert!(t.contains("blocks 1"), "{t}");
    }
}
