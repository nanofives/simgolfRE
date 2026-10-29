use crate::le::{bytes, u16_at, u32_at};
use anyhow::{Result, anyhow, bail};
use msvc6_codeview::codec::{
    Gen, LF_FIELDLIST, PDB20_SYMBOLS, Shape, SymTail, Walk, parse_sym, parse_type, sym_record,
    walk, write_sym, write_type,
};
use std::collections::BTreeMap;

pub const PDB70_SYMBOLS: Walk = Walk {
    aligned: true,
    short_refs: false,
};

#[must_use]
pub fn walk_of(g: Gen) -> Walk {
    match g {
        Gen::St => PDB20_SYMBOLS,
        Gen::Sz => PDB70_SYMBOLS,
    }
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct SymStats {
    pub by_kind: BTreeMap<u16, usize>,
    pub thunk_ords: BTreeMap<u8, usize>,
    pub compile2_strings: usize,
    pub dirty_pads: usize,
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct TypeStats {
    pub by_leaf: BTreeMap<u16, usize>,
    pub members: BTreeMap<u16, usize>,
}

pub fn conv_sym(kind: u16, payload: &[u8], from: Gen, st: &mut SymStats) -> Result<(u16, Vec<u8>)> {
    let p = parse_sym(kind, payload, from)?;
    *st.by_kind.entry(p.kind.sz).or_default() += 1;
    match p.kind.shape {
        Shape::Thunk => {
            let ord = p.fixed.get(20).copied().unwrap_or(0);
            *st.thunk_ords.entry(ord).or_default() += 1;
        }
        Shape::Compile2 => {
            for t in &p.tail {
                if let SymTail::Strings(ss) = t {
                    st.compile2_strings += ss.len();
                }
            }
            if p.pad.iter().any(|b| *b != 0) {
                st.dirty_pads += 1;
            }
        }
        Shape::Ref if from == Gen::St => {
            let fill = u16_at(p.fixed, 10)?;
            if fill != 0 {
                bail!(
                    "{} record with a nonzero fill word {fill:#06x}: VC6 writes 0 there",
                    p.kind.name
                );
            }
        }
        _ => {}
    }
    write_sym(&p, Gen::Sz, false)
}

pub fn conv_type(rec: &[u8], from: Gen, st: &mut TypeStats) -> Result<Vec<u8>> {
    let len = usize::from(u16_at(rec, 0)?);
    let kind = u16_at(rec, 2)?;
    let payload = bytes(rec, 4, len.saturating_sub(2))?;
    let parsed = parse_type(kind, payload, from)?;
    if kind == LF_FIELDLIST {
        for m in &parsed {
            let l = m
                .leaf
                .ok_or_else(|| anyhow!("field-list member {:#06x} has no layout", m.kind))?;
            *st.members.entry(l.sz).or_default() += 1;
        }
    } else if parsed.first().and_then(|p| p.leaf).is_none() {
        bail!("type leaf {kind:#06x} is not one this converter knows");
    }
    let out = write_type(kind, &parsed, from, Gen::Sz)?;
    *st.by_leaf.entry(u16_at(&out, 2)?).or_default() += 1;
    Ok(out)
}

pub fn conv_region(
    region: &[u8],
    base: usize,
    st: &mut SymStats,
) -> Result<(Vec<u8>, BTreeMap<u32, u32>)> {
    let mut out = Vec::with_capacity(region.len());
    let mut map = BTreeMap::new();
    for r in walk(region, PDB20_SYMBOLS)? {
        let (k70, p70) = conv_sym(r.kind, r.payload, Gen::St, st)
            .map_err(|e| e.context(format!("the record at {:#x}", base + r.at)))?;
        map.insert(
            crate::le::u32_of(base + r.at, "record offset")?,
            crate::le::u32_of(base + out.len(), "record offset")?,
        );
        out.extend_from_slice(&sym_record(k70, &p70)?);
    }
    Ok((out, map))
}

#[must_use]
pub fn pointer_fields70(kind70: u16) -> &'static [usize] {
    match kind70 {
        0x110f | 0x1110 | 0x1102 => &[0, 4, 8],
        0x1103 | 0x1104 => &[0, 4],
        _ => &[],
    }
}

pub fn repoint(region: &mut [u8], base: usize, map: &BTreeMap<u32, u32>) -> Result<usize> {
    let mut moved = 0usize;
    let recs: Vec<(usize, u16)> = walk(region, PDB70_SYMBOLS)?
        .into_iter()
        .map(|r| (r.at, r.kind))
        .collect();
    for (at, kind) in recs {
        for f in pointer_fields70(kind) {
            let p = at + 4 + f;
            let old = u32_at(region, p)?;
            if old == 0 {
                continue;
            }
            let new = *map.get(&old).ok_or_else(|| {
                anyhow!(
                    "{} at {:#x}: its field +{f} points at {old:#x}, which is not a record start",
                    msvc6_codeview::codec::sym_name(kind),
                    base + at
                )
            })?;
            if new != old {
                moved += 1;
            }
            crate::le::set32(region, p, new)?;
        }
    }
    Ok(moved)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_st_procedure_becomes_its_7_0_kind_with_the_same_length() {
        let mut p = vec![0u8; 35];
        p[28..32].copy_from_slice(&0x1234u32.to_le_bytes());
        p[32..34].copy_from_slice(&1u16.to_le_bytes());
        p.push(5);
        p.extend_from_slice(b"Hello");
        let rec = sym_record(0x100b, &p).unwrap();
        let (k, out) = conv_sym(0x100b, &rec[4..], Gen::St, &mut SymStats::default()).unwrap();
        assert_eq!(k, 0x1110);
        assert_eq!(&out[..35], &p[..35]);
        assert_eq!(&out[35..], b"Hello\0");
        assert_eq!(sym_record(k, &out).unwrap().len(), rec.len());
    }

    #[test]
    fn a_vc6_reference_loses_its_fill_word_and_counts_its_name() {
        let mut r = 14u16.to_le_bytes().to_vec();
        r.extend_from_slice(&0x0400u16.to_le_bytes());
        r.extend_from_slice(&0u32.to_le_bytes());
        r.extend_from_slice(&0x40u32.to_le_bytes());
        r.extend_from_slice(&3u16.to_le_bytes());
        r.extend_from_slice(&0u16.to_le_bytes());
        r.extend_from_slice(&[1, b'f']);
        r.resize(20, 0);
        let mut region = r.clone();
        region.extend_from_slice(&sym_record(0x0006, &[]).unwrap());
        let w = walk(&region, PDB20_SYMBOLS).unwrap();
        assert_eq!(w.len(), 2);
        let (k, out) =
            conv_sym(w[0].kind, w[0].payload, Gen::St, &mut SymStats::default()).unwrap();
        assert_eq!(k, 0x1125);
        assert_eq!(out, [0, 0, 0, 0, 0x40, 0, 0, 0, 3, 0, b'f', 0]);
        let rec = sym_record(k, &out).unwrap();
        assert_eq!(u16::from_le_bytes([rec[0], rec[1]]), 14);
        let mut bad = r;
        bad[14] = 1;
        let w = walk(&bad, PDB20_SYMBOLS).unwrap();
        assert!(conv_sym(w[0].kind, w[0].payload, Gen::St, &mut SymStats::default()).is_err());
    }

    #[test]
    fn a_label_a_thunk_and_compile2_convert_and_an_unknown_kind_is_refused() {
        let mut lab = vec![0u8; 7];
        lab.extend_from_slice(&[3, b'$', b'L', b'1']);
        let (k, out) = conv_sym(0x0209, &lab, Gen::St, &mut SymStats::default()).unwrap();
        assert_eq!((k, &out[7..]), (0x1105, b"$L1\0".as_slice()));
        let mut th = vec![0u8; 21];
        th.extend_from_slice(&[2, b'j', b'_']);
        let mut st = SymStats::default();
        let (k, _) = conv_sym(0x0206, &th, Gen::St, &mut st).unwrap();
        assert_eq!((k, st.thunk_ords.get(&0)), (0x1102, Some(&1)));
        assert!(conv_sym(0x1234, &[], Gen::St, &mut SymStats::default()).is_err());
        let mut bad = lab;
        bad.push(7);
        assert!(conv_sym(0x0209, &bad, Gen::St, &mut SymStats::default()).is_err());
        let mut p = vec![0u8; 18];
        p.extend_from_slice(&[2, b'c', b'l', 0, 0x77, 0x10]);
        let mut st = SymStats::default();
        let (k, out) = conv_sym(0x1013, &p, Gen::St, &mut st).unwrap();
        assert_eq!(
            (k, &out[18..], st.dirty_pads),
            (0x1116, b"cl\0\0".as_slice(), 1)
        );
        let (k2, again) = conv_sym(k, &out, Gen::Sz, &mut SymStats::default()).unwrap();
        assert_eq!((k2, again), (k, out));
    }

    #[test]
    fn a_struct_and_its_field_list_convert_with_lf_pad() {
        let mut p = Vec::new();
        p.extend_from_slice(&1u16.to_le_bytes());
        p.extend_from_slice(&0u16.to_le_bytes());
        p.extend_from_slice(&0x1001u32.to_le_bytes());
        p.extend_from_slice(&0u32.to_le_bytes());
        p.extend_from_slice(&0u32.to_le_bytes());
        p.extend_from_slice(&4u16.to_le_bytes());
        p.extend_from_slice(&[1, b'S']);
        let rec = msvc6_codeview::codec::type_record(0x1005, &p).unwrap();
        let out = conv_type(&rec, Gen::St, &mut TypeStats::default()).unwrap();
        assert_eq!(u16::from_le_bytes([out[2], out[3]]), 0x1505);
        assert_eq!(out.len() % 4, 0);
        assert_eq!(&out[22..24], b"S\0");
        let mut f = Vec::new();
        f.extend_from_slice(&0x1405u16.to_le_bytes());
        f.extend_from_slice(&3u16.to_le_bytes());
        f.extend_from_slice(&0x74u32.to_le_bytes());
        f.extend_from_slice(&0u16.to_le_bytes());
        f.extend_from_slice(&[1, b'x']);
        f.extend_from_slice(&[0xf2, 0xf1]);
        f.extend_from_slice(&0x0403u16.to_le_bytes());
        f.extend_from_slice(&3u16.to_le_bytes());
        f.extend_from_slice(&0x8004u16.to_le_bytes());
        f.extend_from_slice(&0x1_0000u32.to_le_bytes());
        f.extend_from_slice(&[2, b'E', b'1']);
        let fl = msvc6_codeview::codec::type_record(0x1203, &f).unwrap();
        let mut stats = TypeStats::default();
        let out = conv_type(&fl, Gen::St, &mut stats).unwrap();
        assert_eq!(stats.members.get(&0x150d), Some(&1));
        assert_eq!(stats.members.get(&0x1502), Some(&1));
        let again = conv_type(&out, Gen::Sz, &mut TypeStats::default()).unwrap();
        assert_eq!(again, out);
        assert!(
            conv_type(
                &[4, 0, 0x99, 0x77, 0, 0],
                Gen::St,
                &mut TypeStats::default()
            )
            .is_err()
        );
    }
}
