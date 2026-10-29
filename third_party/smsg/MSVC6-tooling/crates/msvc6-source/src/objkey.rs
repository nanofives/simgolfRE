use msvc6_base::le::{u16_at, u32_at};
use msvc6_base::srcio;
use serde::Serialize;
use sha2::Digest as _;

pub const PART_HEX: usize = 16;

pub const MACHINE_I386: u16 = 0x14c;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Section<'a> {
    pub name: String,
    pub characteristics: u32,
    pub size: u32,
    #[serde(skip)]
    pub raw: &'a [u8],
    pub relocs: Vec<(u32, u32, u16)>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Symbol {
    pub name: String,
    pub value: u32,
    pub section: i16,
    pub class: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Object<'a> {
    pub sections: Vec<Section<'a>>,
    pub symbols: Vec<Option<Symbol>>,
}

fn usize_of(v: u32) -> usize {
    usize::try_from(v).unwrap_or(usize::MAX)
}

fn le32(v: u32) -> [u8; 4] {
    v.to_le_bytes()
}

fn sha(parts: &[&[u8]]) -> [u8; 32] {
    let mut h = sha2::Sha256::new();
    for p in parts {
        h.update(p);
    }
    h.finalize().into()
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn name8(d: &[u8], at: usize) -> String {
    let raw = d.get(at..at + 8).unwrap_or_default();
    let end = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
    srcio::decode(raw.get(..end).unwrap_or_default())
}

pub fn parse(d: &[u8]) -> Result<Object<'_>, String> {
    let bad = |what: &str| format!("not a COFF object: {what}");
    let machine = u16_at(d, 0).map_err(|e| bad(&e.to_string()))?;
    if machine != MACHINE_I386 {
        return Err(bad(&format!("machine {machine:#x}, not i386")));
    }
    let nsec = usize::from(u16_at(d, 2).map_err(|e| bad(&e.to_string()))?);
    let symptr = usize_of(u32_at(d, 8).map_err(|e| bad(&e.to_string()))?);
    let nsym = usize_of(u32_at(d, 12).map_err(|e| bad(&e.to_string()))?);
    let opt = usize::from(u16_at(d, 16).map_err(|e| bad(&e.to_string()))?);
    let table = 20 + opt;
    if table + 40 * nsec > d.len() {
        return Err(bad("the section table runs past the file"));
    }
    let strings = symptr.saturating_add(nsym.saturating_mul(18));
    let long_name = |raw: &[u8]| -> Option<String> {
        if raw.get(..4)? != [0, 0, 0, 0] {
            return None;
        }
        let off = usize_of(u32::from_le_bytes(raw.get(4..8)?.try_into().ok()?));
        let s = d.get(strings + off..)?;
        let end = s.iter().position(|&b| b == 0).unwrap_or(s.len());
        Some(srcio::decode(s.get(..end)?))
    };
    let mut sections = Vec::with_capacity(nsec);
    for k in 0..nsec {
        let h = table + 40 * k;
        let get = |o: usize| u32_at(d, h + o).map_err(|e| bad(&e.to_string()));
        let (size, ptr, rel) = (get(16)?, get(20)?, get(24)?);
        let nrel = u16_at(d, h + 32).map_err(|e| bad(&e.to_string()))?;
        let ch = get(36)?;
        let raw = if ptr == 0 {
            &[][..]
        } else {
            d.get(usize_of(ptr)..usize_of(ptr).saturating_add(usize_of(size)))
                .ok_or_else(|| bad("a section's bytes run past the file"))?
        };
        let mut relocs = Vec::with_capacity(usize::from(nrel));
        for r in 0..usize::from(nrel) {
            let o = usize_of(rel) + 10 * r;
            relocs.push((
                u32_at(d, o).map_err(|e| bad(&e.to_string()))?,
                u32_at(d, o + 4).map_err(|e| bad(&e.to_string()))?,
                u16_at(d, o + 8).map_err(|e| bad(&e.to_string()))?,
            ));
        }
        sections.push(Section {
            name: name8(d, h),
            characteristics: ch,
            size,
            raw,
            relocs,
        });
    }
    let mut symbols = Vec::with_capacity(nsym);
    let mut i = 0usize;
    while i < nsym {
        let o = symptr + 18 * i;
        let Some(rec) = d.get(o..o + 18) else {
            symbols.push(None);
            i += 1;
            continue;
        };
        let raw_name = rec.get(..8).unwrap_or_default();
        let name = long_name(raw_name).unwrap_or_else(|| name8(d, o));
        let value = u32_at(d, o + 8).unwrap_or(0);
        let section = i16::from_le_bytes([
            rec.get(12).copied().unwrap_or(0),
            rec.get(13).copied().unwrap_or(0),
        ]);
        let class = rec.get(16).copied().unwrap_or(0);
        let aux = usize::from(rec.get(17).copied().unwrap_or(0));
        symbols.push(Some(Symbol {
            name,
            value,
            section,
            class,
        }));
        for _ in 0..aux {
            symbols.push(None);
        }
        i += 1 + aux;
    }
    Ok(Object { sections, symbols })
}

#[must_use]
pub fn is_debug(name: &str) -> bool {
    name.starts_with(".debug$")
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Key {
    pub code: String,
    pub parts: Vec<(String, String)>,
    pub names: String,
    #[serde(skip)]
    pub name_list: Vec<String>,
}

fn raw_symbol(d: &[u8], i: usize) -> Option<(i16, u32, u8)> {
    let symptr = usize_of(u32_at(d, 8).ok()?);
    let o = symptr.checked_add(i.checked_mul(18)?)?;
    let s = i16::from_le_bytes([*d.get(o + 12)?, *d.get(o + 13)?]);
    Some((s, u32_at(d, o + 8).ok()?, *d.get(o + 16)?))
}

pub fn key(d: &[u8]) -> Result<Key, String> {
    let obj = parse(d)?;
    let nsym = usize_of(u32_at(d, 12).map_err(|e| e.to_string())?);
    let own: Vec<[u8; 32]> = obj
        .sections
        .iter()
        .map(|s| {
            sha(&[
                s.name.as_bytes(),
                &[0],
                &le32(s.characteristics),
                &le32(s.size),
                s.raw,
            ])
        })
        .collect();
    let target = |i: u32| -> Vec<u8> {
        match raw_symbol(d, usize_of(i)) {
            Some((s, v, _)) if s > 0 => {
                let place = usize::try_from(s - 1).ok().and_then(|k| own.get(k));
                let mut t = vec![b's'];
                t.extend(place.map_or(&[0u8; 32][..], |o| &o[..]));
                t.extend(le32(v));
                t
            }
            Some((0, 0, _)) => vec![b'x'],
            Some((0, v, _)) => [&b"c"[..], &le32(v)].concat(),
            Some((s, v, _)) => [&b"a"[..], &s.to_le_bytes(), &le32(v)].concat(),
            None => [&b"?"[..], &le32(i)].concat(),
        }
    };
    let mut groups: Vec<(String, Vec<[u8; 32]>)> = Vec::new();
    let mut names: Vec<String> = Vec::new();
    for (k, s) in obj.sections.iter().enumerate() {
        if is_debug(&s.name) {
            continue;
        }
        let mut rs: Vec<Vec<u8>> = s
            .relocs
            .iter()
            .map(|(off, sym, ty)| [&le32(*off)[..], &ty.to_le_bytes(), &target(*sym)].concat())
            .collect();
        rs.sort();
        let mut parts: Vec<&[u8]> = vec![own.get(k).map_or(&[][..], |o| &o[..])];
        parts.extend(rs.iter().map(Vec::as_slice));
        let digest = sha(&parts);
        match groups.iter_mut().find(|(n, _)| *n == s.name) {
            Some((_, v)) => v.push(digest),
            None => groups.push((s.name.clone(), vec![digest])),
        }
        for (off, sym, _) in &s.relocs {
            if let Some(Some(t)) = obj.symbols.get(usize_of(*sym))
                && t.section == 0
            {
                names.push(format!(
                    "{} {}+{off:#x} -> {}",
                    hex(&own.get(k).copied().unwrap_or([0; 32])[..4]),
                    s.name,
                    t.name
                ));
            }
        }
    }
    for t in obj.symbols.iter().flatten() {
        if t.class == 2 && t.section > 0 {
            let place = usize::try_from(t.section - 1)
                .ok()
                .and_then(|k| own.get(k))
                .map_or_else(String::new, |o| hex(&o[..4]));
            names.push(format!("public {place} {:#x} {}", t.value, t.name));
        }
    }
    names.sort();
    let mut commons: Vec<u32> = (0..nsym)
        .filter_map(|i| match raw_symbol(d, i) {
            Some((0, v, 2)) if v != 0 => Some(v),
            _ => None,
        })
        .collect();
    commons.sort_unstable();
    let mut out: Vec<(String, [u8; 32])> = groups
        .into_iter()
        .map(|(name, mut v)| {
            v.sort_unstable();
            let mut parts: Vec<&[u8]> = vec![name.as_bytes(), &[0]];
            parts.extend(v.iter().map(|x| &x[..]));
            let g = sha(&parts);
            (name, g)
        })
        .collect();
    if !commons.is_empty() {
        let bytes: Vec<u8> = commons.iter().flat_map(|v| le32(*v)).collect();
        out.push(("common".to_string(), sha(&[&bytes])));
    }
    let mut sorted: Vec<&(String, [u8; 32])> = out.iter().collect();
    sorted.sort_by(|a, b| a.0.cmp(&b.0));
    let mut whole: Vec<&[u8]> = Vec::new();
    for (n, g) in &sorted {
        whole.extend([n.as_bytes(), &[0][..], &g[..]]);
    }
    let joined = names.join("\n");
    Ok(Key {
        code: hex(&sha(&whole)),
        parts: out
            .iter()
            .map(|(n, g)| (n.clone(), hex(g.get(..PART_HEX / 2).unwrap_or_default())))
            .collect(),
        names: hex(&sha(&[joined.as_bytes()])),
        name_list: names,
    })
}

#[must_use]
pub fn difference(a: &[u8], b: &[u8]) -> Option<String> {
    let (ka, kb) = match (key(a), key(b)) {
        (Ok(x), Ok(y)) => (x, y),
        (Err(_), Err(_)) => {
            return msvc6_base::digest::first_difference(a, b).map(|at| {
                format!(
                    "neither output is a COFF object, so the bytes decide: they part at {at:#x} ({} and {} bytes)",
                    a.len(),
                    b.len()
                )
            });
        }
        (Err(e), _) | (_, Err(e)) => {
            return Some(format!("one output is a COFF object and the other is {e}"));
        }
    };
    if ka == kb {
        return None;
    }
    if ka.code == kb.code {
        let gone = ka.name_list.iter().find(|n| !kb.name_list.contains(n));
        let new = kb.name_list.iter().find(|n| !ka.name_list.contains(n));
        return Some(format!(
            "the code is the same; a referenced or public symbol name differs: `{}` before, `{}` after",
            gone.map_or("", String::as_str),
            new.map_or("", String::as_str)
        ));
    }
    let parts: Vec<String> = ka
        .parts
        .iter()
        .filter(|p| !kb.parts.contains(p))
        .map(|p| p.0.clone())
        .chain(
            kb.parts
                .iter()
                .filter(|p| !ka.parts.iter().any(|q| q.0 == p.0))
                .map(|p| p.0.clone()),
        )
        .collect();
    let (Ok(oa), Ok(ob)) = (parse(a), parse(b)) else {
        return Some(format!("sections differ: {}", parts.join(", ")));
    };
    for name in &parts {
        let sa: Vec<&Section<'_>> = oa.sections.iter().filter(|s| &s.name == name).collect();
        let sb: Vec<&Section<'_>> = ob.sections.iter().filter(|s| &s.name == name).collect();
        if sa.len() != sb.len() {
            return Some(format!(
                "{name}: {} section(s) before, {} after",
                sa.len(),
                sb.len()
            ));
        }
        let many = sa.len() > 1;
        for (n, (x, y)) in sa.iter().zip(&sb).enumerate() {
            let which = if many {
                format!("{name} #{}", n + 1)
            } else {
                name.clone()
            };
            if let Some(at) = msvc6_base::digest::first_difference(x.raw, y.raw) {
                return Some(format!(
                    "{which}+{at:#x}: {:02x} before, {:02x} after ({} and {} bytes{})",
                    x.raw.get(at).copied().unwrap_or(0),
                    y.raw.get(at).copied().unwrap_or(0),
                    x.raw.len(),
                    y.raw.len(),
                    if many {
                        ", sections of one name paired in file order"
                    } else {
                        ""
                    }
                ));
            }
        }
        if !many {
            return Some(format!("{name}: its relocations differ"));
        }
    }
    Some(format!("sections differ: {}", parts.join(", ")))
}

#[cfg(test)]
pub mod fixture {
    #[derive(Debug)]
    pub struct Sec<'a> {
        pub name: &'a str,
        pub ch: u32,
        pub raw: &'a [u8],
        pub relocs: &'a [(u32, u32, u16)],
    }

    pub fn object(secs: &[Sec<'_>], syms: &[(&str, u32, i16, u8)], stamp: u32) -> Vec<u8> {
        let nsec = u16::try_from(secs.len()).unwrap();
        let mut d = Vec::new();
        d.extend(super::MACHINE_I386.to_le_bytes());
        d.extend(nsec.to_le_bytes());
        d.extend(stamp.to_le_bytes());
        d.extend([0u8; 8]);
        d.extend(0u16.to_le_bytes());
        d.extend(0u16.to_le_bytes());
        let table = d.len();
        d.extend(vec![0u8; 40 * secs.len()]);
        let mut fix = Vec::new();
        for s in secs {
            let raw_at = d.len();
            d.extend(s.raw);
            let rel_at = d.len();
            for (o, sym, ty) in s.relocs {
                d.extend(o.to_le_bytes());
                d.extend(sym.to_le_bytes());
                d.extend(ty.to_le_bytes());
            }
            fix.push((raw_at, rel_at));
        }
        let symptr = d.len();
        for (name, value, sec, class) in syms {
            let mut n = [0u8; 8];
            n[..name.len()].copy_from_slice(name.as_bytes());
            d.extend(n);
            d.extend(value.to_le_bytes());
            d.extend(sec.to_le_bytes());
            d.extend(0u16.to_le_bytes());
            d.push(*class);
            d.push(0);
        }
        d.extend(4u32.to_le_bytes());
        d[8..12].copy_from_slice(&u32::try_from(symptr).unwrap().to_le_bytes());
        d[12..16].copy_from_slice(&u32::try_from(syms.len()).unwrap().to_le_bytes());
        for (k, s) in secs.iter().enumerate() {
            let h = table + 40 * k;
            d[h..h + s.name.len()].copy_from_slice(s.name.as_bytes());
            let (raw_at, rel_at) = fix[k];
            d[h + 16..h + 20].copy_from_slice(&u32::try_from(s.raw.len()).unwrap().to_le_bytes());
            d[h + 20..h + 24].copy_from_slice(&u32::try_from(raw_at).unwrap().to_le_bytes());
            d[h + 24..h + 28].copy_from_slice(&u32::try_from(rel_at).unwrap().to_le_bytes());
            d[h + 32..h + 34]
                .copy_from_slice(&u16::try_from(s.relocs.len()).unwrap().to_le_bytes());
            d[h + 36..h + 40].copy_from_slice(&s.ch.to_le_bytes());
        }
        d
    }
}

#[cfg(test)]
mod tests {
    use super::fixture::{Sec, object};
    use super::*;

    fn text(code: &[u8]) -> Vec<u8> {
        object(
            &[
                Sec {
                    name: ".text",
                    ch: 0x6030_0020,
                    raw: code,
                    relocs: &[(1, 1, 0x14)],
                },
                Sec {
                    name: ".debug$S",
                    ch: 0x4210_0040,
                    raw: b"line records",
                    relocs: &[],
                },
            ],
            &[("_f", 0, 1, 2), ("_g", 0, 0, 2)],
            0x1234,
        )
    }

    #[test]
    fn the_key_ignores_the_clock_and_debug_sections_and_sees_code() {
        let a = text(b"\xe8\0\0\0\0\xc3");
        let mut b = text(b"\xe8\0\0\0\0\xc3");
        b[4] = 0x99;
        let ka = key(&a).unwrap();
        assert_eq!(ka, key(&b).unwrap());
        assert_eq!(ka.parts.len(), 1);
        assert_eq!(ka.parts[0].0, ".text");
        assert_eq!(difference(&a, &b), None);
        let c = text(b"\xe8\0\0\0\0\x90");
        let why = difference(&a, &c).unwrap();
        assert!(why.starts_with(".text+0x5: c3 before, 90 after"), "{why}");
    }

    #[test]
    fn a_renamed_callee_is_a_name_difference_not_a_code_one() {
        let a = text(b"\xe8\0\0\0\0\xc3");
        let renamed = object(
            &[Sec {
                name: ".text",
                ch: 0x6030_0020,
                raw: b"\xe8\0\0\0\0\xc3",
                relocs: &[(1, 1, 0x14)],
            }],
            &[("_f", 0, 1, 2), ("_h", 0, 0, 2)],
            0,
        );
        assert_eq!(key(&a).unwrap().code, key(&renamed).unwrap().code);
        assert!(difference(&a, &renamed).unwrap().contains("symbol name"));
        assert!(key(b"MZ\0\0").is_err());
    }
}
