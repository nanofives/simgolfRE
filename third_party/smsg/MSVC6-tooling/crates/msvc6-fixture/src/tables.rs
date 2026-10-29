use crate::buf::{Buf, n16, n32, us};
use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Import {
    Name(u16, String),
    Ordinal(u16),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dll {
    pub name: String,
    pub imports: Vec<Import>,
}

impl Dll {
    #[must_use]
    pub fn named(name: &str, imports: &[&str]) -> Self {
        Self {
            name: name.to_string(),
            imports: imports
                .iter()
                .enumerate()
                .map(|(i, n)| Import::Name(n16(i), (*n).to_string()))
                .collect(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Idata {
    pub bytes: Vec<u8>,
    pub import_dir: (u32, u32),
    pub iat_dir: (u32, u32),
    pub ilt: Vec<u32>,
    pub iat: Vec<u32>,
    pub names: Vec<u32>,
}

#[must_use]
pub fn idata(rva: u32, dlls: &[Dll], reserve: &dyn Fn(usize) -> u32) -> Idata {
    let desc = n32(dlls.len().saturating_add(1).saturating_mul(20));
    let block = |d: &Dll| {
        n32(d.imports.len().saturating_add(1).saturating_mul(4))
            .saturating_add(reserve(d.imports.len()))
    };
    let mut at = desc;
    let mut ilt = Vec::new();
    for d in dlls {
        ilt.push(rva.saturating_add(at));
        at = at.saturating_add(block(d));
    }
    let iat_start = at;
    let mut iat = Vec::new();
    for d in dlls {
        iat.push(rva.saturating_add(at));
        at = at.saturating_add(block(d));
    }
    let iat_end = at;
    let mut strings = Buf::new();
    let mut thunks: Vec<Vec<u32>> = Vec::new();
    let mut names = Vec::new();
    for d in dlls {
        let mut t = Vec::new();
        for i in &d.imports {
            match i {
                Import::Ordinal(o) => t.push(0x8000_0000 | u32::from(*o)),
                Import::Name(hint, name) => {
                    t.push(rva.saturating_add(at).saturating_add(strings.pos()));
                    strings.u16(*hint).cstr(name).align(2);
                }
            }
        }
        thunks.push(t);
        names.push(rva.saturating_add(at).saturating_add(strings.pos()));
        strings.cstr(&d.name).align(2);
    }
    let mut b = Buf::new();
    for (k, _) in dlls.iter().enumerate() {
        b.u32(ilt.get(k).copied().unwrap_or(0))
            .u32(0)
            .u32(0)
            .u32(names.get(k).copied().unwrap_or(0))
            .u32(iat.get(k).copied().unwrap_or(0));
    }
    b.zeros(20);
    for _ in 0..2 {
        for (d, t) in dlls.iter().zip(&thunks) {
            for v in t {
                b.u32(*v);
            }
            b.u32(0);
            b.zeros(us(reserve(d.imports.len())));
        }
    }
    b.bytes(strings.as_slice());
    Idata {
        bytes: b.into_vec(),
        import_dir: (rva, desc),
        iat_dir: (
            rva.saturating_add(iat_start),
            iat_end.saturating_sub(iat_start),
        ),
        ilt,
        iat,
        names,
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Export {
    pub name: Option<String>,
    pub target: u32,
}

#[must_use]
pub fn edata(rva: u32, dll: &str, base: u32, timestamp: u32, functions: &[Export]) -> Vec<u8> {
    let mut named: Vec<(&str, u16)> = functions
        .iter()
        .enumerate()
        .filter_map(|(i, f)| f.name.as_deref().map(|n| (n, n16(i))))
        .collect();
    named.sort_by(|a, b| a.0.as_bytes().cmp(b.0.as_bytes()));
    let nf = n32(functions.len());
    let nn = n32(named.len());
    let addr = 40u32;
    let ptrs = addr.saturating_add(nf.saturating_mul(4));
    let ords = ptrs.saturating_add(nn.saturating_mul(4));
    let dll_at = ords.saturating_add(nn.saturating_mul(2));
    let mut strings = Buf::new();
    strings.cstr(dll);
    let mut name_rvas = Vec::new();
    for (n, _) in &named {
        name_rvas.push(rva.saturating_add(dll_at).saturating_add(strings.pos()));
        strings.cstr(n);
    }
    let mut b = Buf::new();
    b.u32(0)
        .u32(timestamp)
        .u16(0)
        .u16(0)
        .u32(rva.saturating_add(dll_at))
        .u32(base)
        .u32(nf)
        .u32(nn)
        .u32(rva.saturating_add(addr))
        .u32(rva.saturating_add(ptrs))
        .u32(rva.saturating_add(ords));
    for f in functions {
        b.u32(f.target);
    }
    for r in &name_rvas {
        b.u32(*r);
    }
    for (_, i) in &named {
        b.u16(*i);
    }
    b.bytes(strings.as_slice());
    b.into_vec()
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum Id {
    Name(String),
    Ordinal(u16),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Resource {
    pub type_id: Id,
    pub name: Id,
    pub lang: u16,
    pub codepage: u32,
    pub data: Vec<u8>,
}

type Tree<'a> = BTreeMap<&'a Id, BTreeMap<&'a Id, BTreeMap<u16, &'a Resource>>>;

fn dir_header(b: &mut Buf, named: usize, ids: usize) {
    b.u32(0).u32(0).u16(0).u16(0).u16(n16(named)).u16(n16(ids));
}

#[must_use]
pub fn rsrc(rva: u32, resources: &[Resource]) -> Vec<u8> {
    let mut tree: Tree<'_> = BTreeMap::new();
    for r in resources {
        tree.entry(&r.type_id)
            .or_default()
            .entry(&r.name)
            .or_default()
            .insert(r.lang, r);
    }
    let table = |n: usize| n32(16usize.saturating_add(n.saturating_mul(8)));
    let mut at = table(tree.len());
    let mut type_dirs = Vec::new();
    for names in tree.values() {
        type_dirs.push(at);
        at = at.saturating_add(table(names.len()));
    }
    let mut name_dirs = Vec::new();
    for names in tree.values() {
        for langs in names.values() {
            name_dirs.push(at);
            at = at.saturating_add(table(langs.len()));
        }
    }
    let leaves = at;
    let nleaves: usize = tree
        .values()
        .flat_map(BTreeMap::values)
        .map(BTreeMap::len)
        .sum();
    at = at.saturating_add(n32(nleaves.saturating_mul(16)));
    let mut strings = Buf::new();
    let mut string_at: BTreeMap<&str, u32> = BTreeMap::new();
    let all_ids = tree
        .iter()
        .flat_map(|(t, names)| std::iter::once(*t).chain(names.keys().copied()));
    for id in all_ids {
        if let Id::Name(s) = id
            && !string_at.contains_key(s.as_str())
        {
            string_at.insert(s.as_str(), at.saturating_add(strings.pos()));
            let units: Vec<u16> = s.encode_utf16().collect();
            strings.u16(n16(units.len()));
            for u in units {
                strings.u16(u);
            }
        }
    }
    strings.align(4);
    let data_at = at.saturating_add(strings.pos());
    let id_field = |id: &Id| match id {
        Id::Ordinal(n) => u32::from(*n),
        Id::Name(s) => 0x8000_0000 | string_at.get(s.as_str()).copied().unwrap_or(0),
    };
    let count = |ids: &mut dyn Iterator<Item = &&Id>| {
        let mut named = 0usize;
        let mut ordinal = 0usize;
        for id in ids {
            match id {
                Id::Name(_) => named = named.saturating_add(1),
                Id::Ordinal(_) => ordinal = ordinal.saturating_add(1),
            }
        }
        (named, ordinal)
    };
    let mut b = Buf::new();
    let (n, o) = count(&mut tree.keys());
    dir_header(&mut b, n, o);
    for (t, dir) in tree.keys().zip(&type_dirs) {
        b.u32(id_field(t)).u32(0x8000_0000 | dir);
    }
    let mut name_dir = name_dirs.iter();
    for names in tree.values() {
        let (n, o) = count(&mut names.keys());
        dir_header(&mut b, n, o);
        for nm in names.keys() {
            b.u32(id_field(nm))
                .u32(0x8000_0000 | name_dir.next().copied().unwrap_or(0));
        }
    }
    let mut leaf = 0u32;
    for names in tree.values() {
        for langs in names.values() {
            dir_header(&mut b, 0, langs.len());
            for lang in langs.keys() {
                b.u32(u32::from(*lang))
                    .u32(leaves.saturating_add(leaf.saturating_mul(16)));
                leaf = leaf.saturating_add(1);
            }
        }
    }
    let mut data = Buf::new();
    for names in tree.values() {
        for langs in names.values() {
            for r in langs.values() {
                b.u32(rva.saturating_add(data_at).saturating_add(data.pos()))
                    .u32(n32(r.data.len()))
                    .u32(r.codepage)
                    .u32(0);
                data.bytes(&r.data).align(8);
            }
        }
    }
    b.bytes(strings.as_slice());
    b.bytes(data.as_slice());
    b.into_vec()
}

#[must_use]
pub fn codeview_nb10(signature: u32, age: u32, path: &str) -> Vec<u8> {
    let mut b = Buf::new();
    b.bytes(b"NB10").u32(0).u32(signature).u32(age).cstr(path);
    b.into_vec()
}

#[must_use]
pub fn codeview_rsds(guid: [u8; 16], age: u32, path: &str) -> Vec<u8> {
    let mut b = Buf::new();
    b.bytes(b"RSDS").bytes(&guid).u32(age).cstr(path);
    b.into_vec()
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DebugEntry {
    pub kind: u32,
    pub timestamp: u32,
    pub size: u32,
    pub rva: u32,
}

#[must_use]
pub fn debug_directory(entries: &[DebugEntry]) -> Vec<u8> {
    let mut b = Buf::new();
    for e in entries {
        b.u32(0)
            .u32(e.timestamp)
            .u16(0)
            .u16(0)
            .u32(e.kind)
            .u32(e.size)
            .u32(e.rva)
            .u32(0);
    }
    b.into_vec()
}

#[must_use]
pub fn base_relocs(sites: &[u32], kind: u16) -> Vec<u8> {
    let mut pages: BTreeMap<u32, Vec<u16>> = BTreeMap::new();
    for s in sites {
        pages
            .entry(s & !0xfff)
            .or_default()
            .push((kind << 12) | n16(us(s & 0xfff)));
    }
    let mut b = Buf::new();
    for (page, mut entries) in pages {
        if entries.len() % 2 == 1 {
            entries.push(0);
        }
        b.u32(page)
            .u32(n32(entries.len().saturating_mul(2).saturating_add(8)));
        for e in entries {
            b.u16(e);
        }
    }
    b.into_vec()
}

#[must_use]
pub fn version_info(major: u16, minor: u16, build: u16, revision: u16) -> Vec<u8> {
    let mut b = Buf::new();
    b.u16(0).u16(52).u16(0);
    for u in "VS_VERSION_INFO".encode_utf16().chain(std::iter::once(0)) {
        b.u16(u);
    }
    b.align(4);
    b.u32(0xFEEF_04BD)
        .u32(0x0001_0000)
        .u32((u32::from(major) << 16) | u32::from(minor))
        .u32((u32::from(build) << 16) | u32::from(revision))
        .u32((u32::from(major) << 16) | u32::from(minor))
        .u32((u32::from(build) << 16) | u32::from(revision))
        .zeros(28);
    let len = n16(b.len());
    b.put_u16(0, len);
    b.into_vec()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_import_tables_point_into_their_own_section() {
        let dlls = [
            Dll::named("KERNEL32.dll", &["ExitProcess", "GetVersion"]),
            Dll {
                name: "WS2_32.dll".into(),
                imports: vec![Import::Ordinal(23)],
            },
        ];
        let t = idata(0x5000, &dlls, &|n| 40 + 4 * u32::try_from(n / 5).unwrap());
        assert_eq!(t.import_dir, (0x5000, 60));
        assert_eq!(t.ilt, vec![0x5000 + 60, 0x5000 + 60 + 12 + 40]);
        assert_eq!(t.iat[0], 0x5000 + 60 + 52 + 48);
        assert_eq!(t.iat_dir.1, 100);
        let ilt0 = u32::from_le_bytes(t.bytes[60..64].try_into().unwrap());
        let off = usize::try_from(ilt0 - 0x5000).unwrap();
        assert_eq!(&t.bytes[off + 2..off + 13], b"ExitProcess");
        let ord = u32::from_le_bytes(t.bytes[112..116].try_into().unwrap());
        assert_eq!(ord, 0x8000_0017);
    }

    #[test]
    fn export_names_are_sorted_and_the_ordinal_table_follows_them() {
        let f = [
            Export {
                name: Some("b".into()),
                target: 0x1010,
            },
            Export {
                name: None,
                target: 0x1020,
            },
            Export {
                name: Some("a".into()),
                target: 0x1030,
            },
        ];
        let e = edata(0x2000, "x.dll", 1, 7, &f);
        let ords_at = 40 + 12 + 8;
        assert_eq!(&e[ords_at..ords_at + 4], &[2, 0, 0, 0]);
        assert_eq!(&e[ords_at + 4..ords_at + 10], b"x.dll\0");
    }

    #[test]
    fn a_page_of_relocations_is_padded_to_a_dword() {
        let r = base_relocs(&[0x1004, 0x1010, 0x2000], 3);
        assert_eq!(&r[..8], &[0, 0x10, 0, 0, 12, 0, 0, 0]);
        assert_eq!(&r[8..10], &(0x3004u16).to_le_bytes());
        assert_eq!(r.len(), 12 + 12);
    }
}
