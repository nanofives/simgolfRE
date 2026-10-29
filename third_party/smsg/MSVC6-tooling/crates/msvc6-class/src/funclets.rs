use anyhow::{Result, bail};
use iced_x86::{Decoder, DecoderOptions, Mnemonic, OpKind};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const ALIGN_BUDGET: usize = 1 << 26;
pub const MAX_GROUPS: usize = 1 << 20;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Group {
    pub rva: u32,
    pub len: u32,
    pub funclets: usize,
    #[serde(skip)]
    pub sig: Vec<u8>,
    pub stub: u32,
}

#[must_use]
pub fn cut(bytes: &[u8], rva: u32, base: u32) -> Vec<Group> {
    let mut out = Vec::new();
    let mut pos = 0usize;
    loop {
        while bytes.get(pos) == Some(&0xcc) {
            pos += 1;
        }
        if pos >= bytes.len() || out.len() >= MAX_GROUPS {
            break;
        }
        let Some(rest) = bytes.get(pos..) else { break };
        let Ok(start) = u32::try_from(pos) else { break };
        let ip = u64::from(base) + u64::from(rva) + u64::from(start);
        let mut d = Decoder::with_ip(32, rest, ip, DecoderOptions::NONE);
        let mut sig: Vec<u8> = Vec::new();
        let mut funclets = 0usize;
        let mut prev_mov_eax: Option<u32> = None;
        let mut end: Option<(usize, u32)> = None;
        while d.can_decode() {
            let at = d.position();
            let ins = d.decode();
            if ins.is_invalid() {
                break;
            }
            let Some(raw) = rest.get(at..at + ins.len()) else {
                break;
            };
            let mut masked = raw.to_vec();
            let near = matches!(ins.op0_kind(), OpKind::NearBranch32);
            let co = d.get_constant_offsets(&ins);
            let mut fields: Vec<(usize, usize)> = Vec::new();
            if near {
                fields.push((ins.len().saturating_sub(4), 4));
            }
            if co.has_immediate() && co.immediate_size() == 4 {
                fields.push((co.immediate_offset(), 4));
            }
            if co.has_displacement()
                && co.displacement_size() == 4
                && ins.memory_base() == iced_x86::Register::None
            {
                fields.push((co.displacement_offset(), 4));
            }
            for (off, n) in fields {
                for b in masked.iter_mut().skip(off).take(n) {
                    *b = 0;
                }
            }
            sig.extend_from_slice(&masked);
            let here = u32::try_from(at)
                .ok()
                .map(|a| rva.wrapping_add(start).wrapping_add(a));
            if ins.mnemonic() == Mnemonic::Jmp && near {
                if let Some(stub) = prev_mov_eax {
                    end = Some((at + ins.len(), stub));
                    break;
                }
                funclets += 1;
            } else if ins.mnemonic() == Mnemonic::Ret {
                funclets += 1;
            }
            prev_mov_eax = (ins.mnemonic() == Mnemonic::Mov && raw.first() == Some(&0xb8))
                .then_some(here)
                .flatten();
        }
        let Some((len, stub)) = end else { break };
        let Ok(len32) = u32::try_from(len) else { break };
        out.push(Group {
            rva: rva.wrapping_add(start),
            len: len32,
            funclets,
            sig,
            stub,
        });
        pos += len;
    }
    out
}

#[must_use]
pub fn pushers(text: &[u8], text_rva: u32, base: u32, stubs: &[u32]) -> BTreeMap<u32, u32> {
    let want: BTreeMap<[u8; 4], u32> = stubs
        .iter()
        .map(|s| (base.wrapping_add(*s).to_le_bytes(), *s))
        .collect();
    let mut out = BTreeMap::new();
    for (i, w) in text.windows(7).enumerate() {
        if w.get(..3) != Some(&[0x6a, 0xff, 0x68][..]) {
            continue;
        }
        let Some(key) = w.get(3..7).and_then(|b| <[u8; 4]>::try_from(b).ok()) else {
            continue;
        };
        if let (Some(stub), Ok(off)) = (want.get(&key), u32::try_from(i)) {
            out.entry(*stub).or_insert(text_rva.wrapping_add(off));
        }
    }
    out
}

#[must_use]
pub fn read_map(map: &str, image_base: u32) -> (BTreeMap<u32, String>, Option<(u32, u32)>) {
    let mut names = BTreeMap::new();
    let mut textx = None;
    for line in map.lines() {
        let t: Vec<&str> = line.split_whitespace().collect();
        if t.get(2) == Some(&".text$x") {
            if let (Some(start), Some(len)) = (t.first(), t.get(1)) {
                let off = start
                    .strip_prefix("0001:")
                    .and_then(|o| u32::from_str_radix(o, 16).ok());
                let len = u32::from_str_radix(len.trim_end_matches('H'), 16).ok();
                if let (Some(o), Some(l)) = (off, len) {
                    textx = Some((o, l));
                }
            }
            continue;
        }
        if !t.first().is_some_and(|f| f.starts_with("0001:")) {
            continue;
        }
        let (Some(name), Some(va)) = (t.get(1), t.get(2)) else {
            continue;
        };
        if let Ok(va) = u32::from_str_radix(va, 16)
            && va >= image_base
        {
            names
                .entry(va - image_base)
                .or_insert_with(|| (*name).to_string());
        }
    }
    (names, textx)
}

#[must_use]
pub fn owner(names: &BTreeMap<u32, String>, rva: Option<u32>) -> String {
    rva.and_then(|r| names.range(..=r).next_back())
        .map_or_else(|| "?".to_string(), |(_, n)| n.clone())
}

pub fn align(a: &[Group], b: &[Group]) -> Result<Vec<(usize, usize)>> {
    let (n, m) = (a.len(), b.len());
    if n.saturating_add(1).saturating_mul(m.saturating_add(1)) > ALIGN_BUDGET {
        bail!(
            "{n} against {m} funclet groups: more than the {ALIGN_BUDGET} cells this alignment takes"
        );
    }
    let mut t = vec![vec![0u32; m + 1]; n + 1];
    let same =
        |i: usize, j: usize| matches!((a.get(i), b.get(j)), (Some(x), Some(y)) if x.sig == y.sig);
    let cell = |t: &Vec<Vec<u32>>, i: usize, j: usize| {
        t.get(i).and_then(|r| r.get(j)).copied().unwrap_or(0)
    };
    for i in (0..n).rev() {
        for j in (0..m).rev() {
            let v = if same(i, j) {
                cell(&t, i + 1, j + 1) + 1
            } else {
                cell(&t, i + 1, j).max(cell(&t, i, j + 1))
            };
            if let Some(c) = t.get_mut(i).and_then(|r| r.get_mut(j)) {
                *c = v;
            }
        }
    }
    let (mut i, mut j) = (0, 0);
    let mut out = Vec::new();
    while i < n && j < m {
        if same(i, j) {
            out.push((i, j));
            i += 1;
            j += 1;
        } else if cell(&t, i + 1, j) >= cell(&t, i, j + 1) {
            i += 1;
        } else {
            j += 1;
        }
    }
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Side {
    pub start: u32,
    pub len: u32,
    pub groups: Vec<Group>,
    pub owners: Vec<String>,
}

impl Side {
    fn cover(&self) -> u32 {
        self.groups.last().map_or(0, |g| {
            g.rva.saturating_add(g.len).saturating_sub(self.start)
        })
    }

    fn line(&self, who: &str) -> String {
        let cover = self.cover();
        let mut s = format!(
            "  {who:<9} {:#x}..{:#x} ({:#x} B): {} group(s), {} funclet(s), cut {cover:#x} B",
            self.start,
            self.start.saturating_add(self.len),
            self.len,
            self.groups.len(),
            self.groups.iter().map(|g| g.funclets).sum::<usize>()
        );
        if cover != self.len {
            s.push_str(&format!(
                "\n            ** UNCUT TAIL {:#x}..{:#x} ({} B): no handler stub ends it, so it is NOT a funclet group **",
                self.start.saturating_add(cover),
                self.start.saturating_add(self.len),
                self.len.saturating_sub(cover)
            ));
        }
        s
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub tag: &'static str,
    pub side: &'static str,
    pub rva: u32,
    pub len: u32,
    pub funclets: usize,
    pub owner: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Report {
    pub reference: Side,
    pub ours: Side,
    pub aligned: usize,
    pub rows: Vec<Row>,
    pub net: i64,
}

pub fn compare(reference: Side, ours: Side) -> Result<Report> {
    let pairs = align(&reference.groups, &ours.groups)?;
    let rp: BTreeSet<usize> = pairs.iter().map(|p| p.0).collect();
    let op: BTreeSet<usize> = pairs.iter().map(|p| p.1).collect();
    let unpaired = |gs: &[Group], paired: &BTreeSet<usize>| -> BTreeSet<Vec<u8>> {
        gs.iter()
            .enumerate()
            .filter(|(i, _)| !paired.contains(i))
            .map(|(_, g)| g.sig.clone())
            .collect()
    };
    let o_un = unpaired(&ours.groups, &op);
    let r_un = unpaired(&reference.groups, &rp);
    let mut rows = Vec::new();
    let mut net = 0i64;
    let (mut i, mut j) = (0usize, 0usize);
    let mut k = 0usize;
    loop {
        let next = pairs.get(k).copied();
        let (ri, oj) = next.unwrap_or((reference.groups.len(), ours.groups.len()));
        while i < ri {
            if let Some(g) = reference.groups.get(i) {
                net += i64::from(g.len);
                rows.push(Row {
                    tag: if o_un.contains(&g.sig) {
                        "MOVED"
                    } else {
                        "REF ONLY"
                    },
                    side: "ref",
                    rva: g.rva,
                    len: g.len,
                    funclets: g.funclets,
                    owner: reference.owners.get(i).cloned().unwrap_or_default(),
                });
            }
            i += 1;
        }
        while j < oj {
            if let Some(g) = ours.groups.get(j) {
                net -= i64::from(g.len);
                rows.push(Row {
                    tag: if r_un.contains(&g.sig) {
                        "MOVED"
                    } else {
                        "OURS ONLY"
                    },
                    side: "ours",
                    rva: g.rva,
                    len: g.len,
                    funclets: g.funclets,
                    owner: ours.owners.get(j).cloned().unwrap_or_default(),
                });
            }
            j += 1;
        }
        if next.is_none() {
            break;
        }
        i += 1;
        j += 1;
        k += 1;
    }
    Ok(Report {
        aligned: pairs.len(),
        reference,
        ours,
        rows,
        net,
    })
}

#[must_use]
pub fn report(r: &Report, label: &str, candidate: &str) -> Vec<String> {
    let mut out = vec![
        format!("{label}: `.text$x' FUNCLET GROUPS, the reference against {candidate}"),
        r.reference.line("reference"),
        r.ours.line("ours"),
        format!(
            "  aligned (identical masked bytes, in order): {} of {} / {}",
            r.aligned,
            r.reference.groups.len(),
            r.ours.groups.len()
        ),
        String::new(),
        "  DISAGREEMENTS (MOVED = an identical group sits unpaired on the other side):".to_string(),
    ];
    for row in &r.rows {
        out.push(format!(
            "    {:<9} {:<4} {:#08x} {:>3} B {:>2} fn  {}",
            row.tag, row.side, row.rva, row.len, row.funclets, row.owner
        ));
    }
    out.push(String::new());
    out.push(format!(
        "  unpadded bytes in unpaired groups, reference minus ours: {:+}",
        r.net
    ));
    out.push(
        "  A group is the contribution of ONE function; its owner is the body that pushes its stub. A REF ONLY"
            .to_string(),
    );
    out.push(
        "  row is a function the original compiled with an EH frame (or with more funclets) and ours does not."
            .to_string(),
    );
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bytes(s: &str) -> Vec<u8> {
        s.split_whitespace()
            .map(|h| u8::from_str_radix(h, 16).unwrap())
            .collect()
    }

    const TWO: &str = "8b 4d f0 83 c1 08 e9 15 cf f4 ff b8 d0 d2 4b 00 e9 a7 df fe ff cc cc cc cc cc cc cc cc cc cc cc 8b 4d f0 e9 08 cf f4 ff 8b 4d f0 83 c1 08 e9 ed ce f4 ff b8 f8 d2 4b 00 e9 7f df fe ff";

    #[test]
    fn a_region_cuts_into_groups_at_each_handler_stub_with_addresses_masked() {
        let g = cut(&bytes(TWO), 0xb72d0, 0x40_0000);
        let at: Vec<(u32, u32, usize, u32)> = g
            .iter()
            .map(|g| (g.rva, g.len, g.funclets, g.stub))
            .collect();
        assert_eq!(
            at,
            vec![(0xb72d0, 0x15, 1, 0xb72db), (0xb72f0, 0x1d, 2, 0xb7303)]
        );
        assert_eq!(g[1].sig.get(8..16), g[0].sig.get(0..8));
        assert_eq!(g[0].sig.get(11..16), Some(&[0xb8, 0, 0, 0, 0][..]));
    }

    #[test]
    fn the_alignment_pairs_identical_groups_in_order_and_names_the_rest() {
        let mk = |s: u8, rva: u32| Group {
            rva,
            len: 1,
            funclets: 1,
            sig: vec![s],
            stub: 0,
        };
        let side = |gs: Vec<Group>| Side {
            start: 0,
            len: 3,
            owners: vec!["a".into(), "b".into(), "c".into()],
            groups: gs,
        };
        let r = compare(
            side(vec![mk(1, 0), mk(2, 1), mk(3, 2)]),
            side(vec![mk(2, 0), mk(1, 1), mk(3, 2)]),
        )
        .unwrap();
        assert_eq!(r.aligned, 2);
        let tags: Vec<(&str, &str)> = r.rows.iter().map(|x| (x.tag, x.side)).collect();
        assert_eq!(tags, vec![("MOVED", "ref"), ("MOVED", "ours")]);
        let big: Vec<Group> = (0..9000)
            .map(|k| mk(u8::try_from(k % 200).unwrap_or(0), k))
            .collect();
        assert!(align(&big, &big).is_err());
    }

    #[test]
    fn a_map_gives_the_funclet_contribution_and_the_code_names() {
        let map = " 0001:00000000 000a5f3eH .text                   CODE\n 0001:000b72d0 00002143H .text$x                 CODE\n 0001:00000010       ?f@@YAXXZ                  00401010 f   a.obj\n";
        let (names, x) = read_map(map, 0x40_0000);
        assert_eq!(x, Some((0xb72d0, 0x2143)));
        assert_eq!(names.get(&0x1010).map(String::as_str), Some("?f@@YAXXZ"));
        assert_eq!(owner(&names, Some(0x1020)), "?f@@YAXXZ");
        assert_eq!(owner(&names, None), "?");
    }
}
