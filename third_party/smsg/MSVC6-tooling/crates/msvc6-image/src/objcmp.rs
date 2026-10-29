use anyhow::{Result, anyhow, bail};
use msvc6_base::le;
use msvc6_base::srcio;
use serde::Serialize;
use std::collections::HashMap;
use std::ops::Range;
use std::path::{Path, PathBuf};

use crate::coff::{
    CLASS_EXTERNAL, CLASS_FILE, CLASS_FUNCTION, CLASS_LABEL, CLASS_STATIC, HEADER, MAX_NAME,
    NAME_ALLOWANCE, NAME_BUDGET, Object, RELOC, SECTION_HEADER, SYMBOL, check_tables,
};
use crate::us;

pub const STAMP: Range<usize> = 4..8;
pub const LF_TYPESERVER: u16 = 0x0016;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize)]
pub struct Masks {
    pub labels: bool,
    pub lines: bool,
    pub pdb_sig: bool,
    pub debug: bool,
}

impl Masks {
    #[must_use]
    pub fn canonical(&self) -> bool {
        self.labels || self.lines || self.debug
    }
}

fn u16_at(b: &[u8], o: usize) -> Option<u16> {
    le::u16_at(b, o).ok()
}

fn u32_at(b: &[u8], o: usize) -> Option<usize> {
    le::u32_at(b, o).ok().map(us)
}

#[must_use]
pub fn pdb_signatures(obj: &[u8]) -> Vec<usize> {
    let mut out = Vec::new();
    let nsec = u16_at(obj, 2).map_or(0, usize::from);
    let opt = u16_at(obj, 16).map_or(0, usize::from);
    for k in 0..nsec {
        let h = HEADER
            .saturating_add(opt)
            .saturating_add(SECTION_HEADER.saturating_mul(k));
        if obj.get(h..h.saturating_add(8)) != Some(&b".debug$T"[..]) {
            continue;
        }
        let (Some(size), Some(ptr)) = (
            u32_at(obj, h.saturating_add(16)),
            u32_at(obj, h.saturating_add(20)),
        ) else {
            continue;
        };
        let (mut o, end) = (
            ptr.saturating_add(4),
            ptr.saturating_add(size).min(obj.len()),
        );
        while o.saturating_add(4) <= end {
            let (Some(len), Some(leaf)) = (u16_at(obj, o), u16_at(obj, o.saturating_add(2))) else {
                break;
            };
            if leaf == LF_TYPESERVER && o.saturating_add(8) <= end {
                out.push(o.saturating_add(4));
            }
            if len < 2 {
                break;
            }
            o = o.saturating_add(2).saturating_add(usize::from(len));
        }
    }
    out
}

fn without_pdb_signatures(obj: &[u8]) -> Vec<u8> {
    let mut out = obj.to_vec();
    for s in pdb_signatures(obj) {
        if let Some(b) = out.get_mut(s..s.saturating_add(4)) {
            b.fill(0);
        }
    }
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub enum Cmp {
    Same,
    Length(usize, usize),
    Bytes(usize, usize),
}

#[must_use]
pub fn compare(a: &[u8], b: &[u8], pdb_sig: bool) -> Cmp {
    if a.len() != b.len() {
        return Cmp::Length(a.len(), b.len());
    }
    let sigs = if pdb_sig {
        pdb_signatures(a)
    } else {
        Vec::new()
    };
    let masked = |i: usize| {
        STAMP.contains(&i) || sigs.iter().any(|&s| (s..s.saturating_add(4)).contains(&i))
    };
    let mut first = None;
    let mut n = 0usize;
    for (i, (x, y)) in a.iter().zip(b).enumerate() {
        if x != y && !masked(i) {
            first.get_or_insert(i);
            n = n.saturating_add(1);
        }
    }
    first.map_or(Cmp::Same, |f| Cmp::Bytes(f, n))
}

fn masked_aux(aux: &[u8], range: Range<usize>) -> Vec<u8> {
    let mut out = aux.to_vec();
    if let Some(b) = out.get_mut(range) {
        b.fill(0);
    }
    out
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

#[must_use]
pub fn is_author_label(n: &str) -> bool {
    let Some(rest) = n.strip_prefix('$') else {
        return false;
    };
    let Some((ident, num)) = rest.split_once('$') else {
        return false;
    };
    let mut ic = ident.chars();
    ic.next()
        .is_some_and(|c| c.is_ascii_alphabetic() || c == '_')
        && ic.all(|c| c.is_ascii_alphanumeric() || c == '_')
        && !num.is_empty()
        && num.chars().all(|c| c.is_ascii_digit())
}

#[derive(Default)]
struct Labels {
    on: bool,
    renames: HashMap<String, String>,
    counts: [usize; 2],
}

impl Labels {
    fn fresh(&mut self, kind: usize) -> String {
        let next = self.counts.get(kind).copied().unwrap_or(0);
        if let Some(c) = self.counts.get_mut(kind) {
            *c = c.saturating_add(1);
        }
        format!("${}#{next}", if kind == 0 { 'L' } else { 'T' })
    }

    fn token(&mut self, tok: &str, kind: usize) -> String {
        if let Some(r) = self.renames.get(tok) {
            return r.clone();
        }
        let r = self.fresh(kind);
        self.renames.insert(tok.to_string(), r.clone());
        r
    }

    fn normalize(&mut self, n: &str, author_label: bool) -> String {
        if !self.on {
            return n.to_string();
        }
        if author_label {
            return self.token(n, 0);
        }
        let b = n.as_bytes();
        let mut out = String::new();
        let mut i = 0;
        while i < b.len() {
            let kind = match (b.get(i), b.get(i.saturating_add(1))) {
                (Some(b'$'), Some(b'L')) => Some(0),
                (Some(b'$'), Some(b'T')) => Some(1),
                _ => None,
            };
            let digits = b
                .get(i.saturating_add(2)..)
                .unwrap_or_default()
                .iter()
                .take_while(|c| c.is_ascii_digit())
                .count();
            if let (Some(k), true) = (kind, digits > 0) {
                let end = i.saturating_add(2).saturating_add(digits);
                let tok = n.get(i..end).unwrap_or_default().to_string();
                out.push_str(&self.token(&tok, k));
                i = end;
            } else {
                out.push(char::from(b.get(i).copied().unwrap_or(b'?')));
                i = i.saturating_add(1);
            }
        }
        out
    }
}

pub fn canonical(obj: &[u8], m: Masks) -> Result<Vec<String>> {
    let unsigned;
    let d: &[u8] = if m.pdb_sig {
        unsigned = without_pdb_signatures(obj);
        &unsigned
    } else {
        obj
    };
    let bad = || anyhow!("not a COFF object (truncated header)");
    let o = Object::parse(d)?;
    check_tables(&o)?;
    let raw_total = o
        .sections
        .iter()
        .filter(|s| s.raw_ptr != 0)
        .fold(0usize, |t, s| t.saturating_add(us(s.size)));
    if raw_total > d.len() {
        bail!(
            "not a COFF object: its sections hold {raw_total} bytes of a {}-byte file; their data overlap",
            d.len()
        );
    }
    let nsec = usize::from(u16_at(d, 2).ok_or_else(bad)?);
    let symptr = u32_at(d, 8).ok_or_else(bad)?;
    let nsym = u32_at(d, 12).ok_or_else(bad)?;
    let opt = usize::from(u16_at(d, 16).ok_or_else(bad)?);
    let budget = d
        .len()
        .saturating_mul(NAME_BUDGET)
        .saturating_add(MAX_NAME.saturating_mul(NAME_ALLOWANCE));
    let scanned = std::cell::Cell::new(0usize);
    let name_of = |raw: &[u8]| -> Result<String> {
        if raw.get(0..4) == Some(&[0, 0, 0, 0][..]) {
            let s = o.string(u32_at(raw, 4).unwrap_or(0)).unwrap_or_default();
            let n = scanned.get().saturating_add(s.len()).saturating_add(1);
            scanned.set(n);
            if n > budget {
                bail!(
                    "not a COFF object: its names read {n} bytes, more than {budget}; names overlap"
                );
            }
            Ok(s)
        } else {
            let end = raw.iter().position(|&c| c == 0).unwrap_or(raw.len());
            Ok(srcio::decode(raw.get(..end).unwrap_or_default()))
        }
    };
    let mut labels = Labels {
        on: m.labels,
        ..Labels::default()
    };
    let mut syms: Vec<String> =
        Vec::with_capacity(nsym.min(d.len().saturating_sub(symptr) / SYMBOL));
    let mut lines = vec![format!(
        "header machine={:#x} sections={nsec} symbols={nsym} opt={opt} ch={:#x}",
        u16_at(d, 0).unwrap_or(0),
        u16_at(d, 18).unwrap_or(0)
    )];
    let mut k = 0usize;
    while k < nsym {
        let o = symptr.saturating_add(SYMBOL.saturating_mul(k));
        let raw = d.get(o..o.saturating_add(SYMBOL)).ok_or_else(bad)?;
        let class = raw.get(16).copied().unwrap_or(0);
        let raw_name = name_of(raw.get(0..8).unwrap_or_default())?;
        let name = labels.normalize(
            &raw_name,
            class == CLASS_LABEL && is_author_label(&raw_name),
        );
        let naux = usize::from(raw.get(17).copied().unwrap_or(0));
        let typ = u16_at(raw, 14).unwrap_or(0);
        let value = if m.lines && class == CLASS_FUNCTION && raw_name == ".lf" {
            "(line count, masked)".to_string()
        } else {
            format!("{:#x}", u32_at(raw, 8).unwrap_or(0))
        };
        let desc = format!(
            "{name} value={value} section={} type={typ:#x} class={class}",
            u16_at(raw, 12).map_or(0, |s| i32::from(s.cast_signed()))
        );
        lines.push(format!("sym {k} {desc} aux={naux}"));
        syms.push(desc.clone());
        let fn_def = (class == CLASS_EXTERNAL || class == CLASS_STATIC) && typ & 0x30 == 0x20;
        for a in 0..naux {
            let at = k.saturating_add(1).saturating_add(a);
            let ao = o.saturating_add(SYMBOL.saturating_mul(a.saturating_add(1)));
            let raw_aux = d.get(ao..ao.saturating_add(SYMBOL)).unwrap_or_default();
            if m.debug && class == CLASS_STATIC && raw_name.starts_with(".debug$") {
                lines.push(format!("sym {at} aux (debug section, ignored)"));
                syms.push(format!("aux of {desc}"));
                continue;
            }
            let section_aux;
            let aux = if m.lines && class == CLASS_STATIC && !fn_def && a == 0 {
                section_aux = masked_aux(raw_aux, 6..8);
                &section_aux[..]
            } else {
                raw_aux
            };
            if m.lines && class == CLASS_FUNCTION {
                lines.push(format!(
                    "sym {at} aux {} (line masked)",
                    hex(&masked_aux(aux, 4..6))
                ));
            } else if m.lines && fn_def && a == 0 {
                lines.push(format!(
                    "sym {at} aux {} (line-table pointer masked)",
                    hex(&masked_aux(aux, 8..12))
                ));
            } else if class == CLASS_FILE {
                lines.push(format!("sym {at} aux (.file name, ignored)"));
            } else if class == CLASS_STATIC && naux == 1 && aux.get(14) == Some(&0) {
                lines.push(format!(
                    "sym {at} aux {} checksum (not COMDAT, ignored) {}",
                    hex(aux.get(..8).unwrap_or_default()),
                    hex(aux.get(12..).unwrap_or_default())
                ));
            } else {
                lines.push(format!("sym {at} aux {}", hex(aux)));
            }
            syms.push(format!("aux of {desc}"));
        }
        k = k.saturating_add(1).saturating_add(naux);
    }
    for s in 0..nsec {
        let h = HEADER
            .saturating_add(opt)
            .saturating_add(SECTION_HEADER.saturating_mul(s));
        let hdr = d.get(h..h.saturating_add(SECTION_HEADER)).ok_or_else(bad)?;
        let name = name_of(hdr.get(0..8).unwrap_or_default())?;
        let number = s.saturating_add(1);
        let size = u32_at(hdr, 16).unwrap_or(0);
        let ptr = u32_at(hdr, 20).unwrap_or(0);
        let rptr = u32_at(hdr, 24).unwrap_or(0);
        let nrel = usize::from(u16_at(hdr, 32).unwrap_or(0));
        let ch = u32_at(hdr, 36).unwrap_or(0);
        if m.debug && name.starts_with(".debug$") {
            lines.push(format!("section {number} {name} (debug, ignored)"));
            continue;
        }
        let nlines = if m.lines {
            "(masked)".to_string()
        } else {
            u16_at(hdr, 34).unwrap_or(0).to_string()
        };
        lines.push(format!(
            "section {number} {name} vsize={:#x} va={:#x} size={size:#x} relocs={nrel} lines={nlines} ch={ch:#x}",
            u32_at(hdr, 8).unwrap_or(0),
            u32_at(hdr, 12).unwrap_or(0),
        ));
        if ptr != 0 {
            let data = d.get(ptr..ptr.saturating_add(size)).unwrap_or_default();
            for (c, chunk) in data.chunks(32).enumerate() {
                lines.push(format!(
                    "section {number} {name}+{:#06x} {}",
                    c.saturating_mul(32),
                    hex(chunk)
                ));
            }
        }
        for r in 0..nrel {
            let ro = rptr.saturating_add(RELOC.saturating_mul(r));
            let rel = d.get(ro..ro.saturating_add(RELOC)).unwrap_or_default();
            let target = u32_at(rel, 4).unwrap_or(usize::MAX);
            lines.push(format!(
                "section {number} {name} reloc +{:#06x} type={} -> {}",
                u32_at(rel, 0).unwrap_or(0),
                u16_at(rel, 8).unwrap_or(0),
                syms.get(target)
                    .map_or("<bad symbol index>", String::as_str)
            ));
        }
    }
    Ok(lines)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CanonicalDiff {
    pub count: usize,
    pub shown: Vec<(Option<String>, Option<String>)>,
}

pub fn compare_canonical(
    a: &[u8],
    b: &[u8],
    m: Masks,
    show: usize,
) -> Result<Option<CanonicalDiff>> {
    let (ca, cb) = (canonical(a, m)?, canonical(b, m)?);
    if ca == cb {
        return Ok(None);
    }
    let mut d = CanonicalDiff {
        count: 0,
        shown: Vec::new(),
    };
    for i in 0..ca.len().max(cb.len()) {
        let (x, y) = (ca.get(i), cb.get(i));
        if x != y {
            d.count = d.count.saturating_add(1);
            if d.shown.len() < show {
                d.shown.push((x.cloned(), y.cloned()));
            }
        }
    }
    Ok(Some(d))
}

#[must_use]
pub fn objects(dir: &Path) -> Vec<PathBuf> {
    let mut out = Vec::new();
    let mut stack = vec![dir.to_path_buf()];
    while let Some(d) = stack.pop() {
        let Ok(rd) = std::fs::read_dir(&d) else {
            continue;
        };
        for e in rd.flatten() {
            let p = e.path();
            let Ok(kind) = e.file_type() else {
                continue;
            };
            if kind.is_symlink() {
                continue;
            }
            if kind.is_dir() {
                stack.push(p);
            } else if p.extension().is_some_and(|x| x.eq_ignore_ascii_case("obj"))
                && let Ok(r) = p.strip_prefix(dir)
            {
                out.push(r.to_path_buf());
            }
        }
    }
    out.sort();
    out
}
