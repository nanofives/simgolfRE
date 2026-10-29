use crate::types::{NoType, T_UCHAR, Types, parse_proto, parse_type};
use crate::write::{Global, Module, Proc, Public, Spec, Udt, Written, build};
use anyhow::Result;
use msvc6_image::pe::Image;
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use std::collections::{BTreeMap, BTreeSet, HashMap};

pub const MAX_ROWS: usize = 1 << 20;

#[derive(Debug, Clone, Default)]
pub struct Tsv {
    cols: HashMap<String, usize>,
    pub rows: Vec<Vec<String>>,
}

impl Tsv {
    #[must_use]
    pub fn parse(text: &str) -> Self {
        let mut lines = text
            .lines()
            .filter(|l| !l.starts_with('#') && !l.trim().is_empty());
        let cols = lines
            .next()
            .map(|h| {
                h.split('\t')
                    .enumerate()
                    .map(|(i, c)| (c.trim().to_string(), i))
                    .collect()
            })
            .unwrap_or_default();
        let rows = lines
            .take(MAX_ROWS)
            .map(|l| l.split('\t').map(str::to_string).collect())
            .collect();
        Self { cols, rows }
    }

    #[must_use]
    pub fn get<'r>(&self, row: &'r [String], col: &str) -> &'r str {
        self.cols
            .get(col)
            .and_then(|i| row.get(*i))
            .map_or("", |s| s.trim())
    }

    #[must_use]
    pub fn has(&self, col: &str) -> bool {
        self.cols.contains_key(col)
    }
}

fn hex(s: &str) -> Option<u32> {
    let t = s.trim();
    u32::from_str_radix(t.strip_prefix("0x").unwrap_or(t), 16).ok()
}

#[derive(Debug, Clone)]
pub struct Knowledge {
    pub functions: Tsv,
    pub data: Tsv,
    pub data_va: String,
    pub globals: Option<Tsv>,
    pub library: Option<Tsv>,
    pub index: Tsv,
    pub locals: usize,
    pub modules: String,
}

#[derive(Debug, Default, Clone)]
pub struct Tally {
    pub functions: usize,
    pub typed: usize,
    pub custom_storage: usize,
    pub no_prototype: usize,
    pub unparsed: BTreeMap<String, usize>,
    pub type_unstated: usize,
    pub name_mismatch: usize,
    pub clipped: usize,
    pub no_size: usize,
    pub data: usize,
    pub data_typed: usize,
    pub data_untyped_type: BTreeMap<String, usize>,
    pub data_no_type: usize,
    pub data_as_byte: usize,
    pub data_in_code_public: usize,
    pub library_publics: usize,
    pub publics: usize,
    pub udts: usize,
    pub types: usize,
    pub placeholders: usize,
    pub unlaid: usize,
    pub skipped_decls: usize,
    pub outside: usize,
    pub unnamed_typed_globals: usize,
    pub locals: usize,
    pub modules: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Expect {
    pub va: u32,
    pub kind: &'static str,
    pub names: Vec<String>,
}

fn section_of(secs: &[(u32, u32)], rva: u32) -> Option<(u16, u32)> {
    for (i, (va, len)) in secs.iter().enumerate() {
        if rva >= *va && rva - *va < *len {
            return Some((u16::try_from(i + 1).ok()?, rva - *va));
        }
    }
    None
}

#[must_use]
pub fn datum_type(stated: Option<u32>, in_code: bool) -> Option<u32> {
    match stated {
        Some(t) => Some(t),
        None if in_code => None,
        None => Some(T_UCHAR),
    }
}

#[derive(Debug)]
pub struct Authored {
    pub tally: Tally,
    pub expect: Vec<Expect>,
    pub written: Written,
    pub identity: (u32, u32),
}

const CODE_KINDS: [&str; 5] = [
    "function",
    "import-thunk",
    "delay-thunk",
    "delay-tailmerge",
    "callee",
];

pub fn author(img: &Image, k: &Knowledge, mut types: Types) -> Result<Authored> {
    let mut tally = Tally {
        locals: k.locals,
        ..Tally::default()
    };
    let base = img.opt.image_base.get();
    let secs: Vec<(u32, u32)> = img
        .sections
        .iter()
        .map(|s| (s.rva.get(), s.virt_size.get().max(s.raw_size.get())))
        .collect();
    let headers = crate::write::section_headers(img)?;
    let identity = match img.codeview()? {
        Some(c) => (c.signature, c.age),
        None => (img.timestamp, 1),
    };
    let mut size_of: HashMap<u32, u32> = HashMap::new();
    let mut module_of: HashMap<u32, String> = HashMap::new();
    let mut starts: BTreeSet<u32> = BTreeSet::new();
    for r in &k.index.rows {
        let Some(va) = hex(k.index.get(r, "va")) else {
            continue;
        };
        starts.insert(va);
        if let Ok(s) = k.index.get(r, "size").parse::<u32>() {
            size_of.insert(va, s);
        }
        let m = k.index.get(r, "module");
        if !m.is_empty() {
            module_of.insert(va, m.to_string());
        }
    }
    let mut lib_symbol: HashMap<u32, (String, String, String)> = HashMap::new();
    if let Some(l) = &k.library {
        for r in &l.rows {
            let Some(va) = hex(l.get(r, "va")) else {
                continue;
            };
            let kind = l.get(r, "kind").to_string();
            if CODE_KINDS.contains(&kind.as_str()) {
                starts.insert(va);
            }
            lib_symbol.insert(
                va,
                (
                    kind,
                    l.get(r, "name").to_string(),
                    l.get(r, "symbol").to_string(),
                ),
            );
        }
    }
    for r in &k.functions.rows {
        if let Some(va) = hex(k.functions.get(r, "va")) {
            starts.insert(va);
        }
    }
    let mut modules: BTreeMap<String, Vec<Proc>> = BTreeMap::new();
    let mut publics: Vec<Public> = Vec::new();
    let mut seen_pub: BTreeSet<(u32, String)> = BTreeSet::new();
    let mut expect = Vec::new();
    let mut add_pub = |publics: &mut Vec<Public>, va: u32, name: &str, isect: u16, off: u32| {
        if seen_pub.insert((va, name.to_string())) {
            publics.push(Public {
                name: name.to_string(),
                isect,
                off,
            });
        }
    };
    for r in &k.functions.rows {
        let Some(va) = hex(k.functions.get(r, "va")) else {
            continue;
        };
        let name = k.functions.get(r, "name");
        if name.is_empty() {
            continue;
        }
        let Some((isect, off)) = va.checked_sub(base).and_then(|rva| section_of(&secs, rva)) else {
            tally.outside += 1;
            continue;
        };
        tally.functions += 1;
        let next = starts.range(va.saturating_add(1)..).next().copied();
        let gap = next.map(|n| n - va);
        let len = match (size_of.get(&va).copied(), gap) {
            (Some(s), Some(g)) if s > g => {
                tally.clipped += 1;
                g
            }
            (Some(s), _) => s,
            (None, Some(g)) => {
                tally.no_size += 1;
                g
            }
            (None, None) => {
                tally.no_size += 1;
                1
            }
        };
        let proto = k.functions.get(r, "prototype");
        let mut ti = 0u32;
        let mut flags = 0u8;
        if proto.is_empty() {
            tally.no_prototype += 1;
        } else {
            match parse_proto(proto) {
                Ok(p) => {
                    if p.name != name {
                        tally.name_mismatch += 1;
                    }
                    if p.noreturn {
                        flags |= 0x08;
                    }
                    match types.procedure(&p.func)? {
                        Some(t) => {
                            ti = t;
                            tally.typed += 1;
                        }
                        None => tally.type_unstated += 1,
                    }
                }
                Err(NoType::CustomStorage) => tally.custom_storage += 1,
                Err(NoType::Unparsed(why)) => *tally.unparsed.entry(why).or_default() += 1,
            }
        }
        let module = module_of.get(&va).cloned().unwrap_or_else(|| "misc".into());
        modules.entry(module).or_default().push(Proc {
            name: name.to_string(),
            isect,
            off,
            len,
            ti,
            flags,
        });
        let pub_name = match lib_symbol.get(&va) {
            Some((kind, _, sym)) if !sym.is_empty() && kind != "chunk" => {
                tally.library_publics += 1;
                sym.clone()
            }
            _ => name.to_string(),
        };
        add_pub(&mut publics, va, &pub_name, isect, off);
        let mut want = vec![name.to_string()];
        if pub_name != name {
            want.push(pub_name.clone());
        }
        expect.push(Expect {
            va,
            kind: "function",
            names: want,
        });
    }
    let fn_vas: BTreeSet<u32> = expect.iter().map(|e| e.va).collect();
    let mut gtype: HashMap<u32, String> = HashMap::new();
    if let Some(g) = &k.globals {
        for r in &g.rows {
            if let Some(va) = hex(g.get(r, "va")) {
                gtype.insert(va, g.get(r, "type").to_string());
            }
        }
    }
    let mut globs = Vec::new();
    let mut data_vas = BTreeSet::new();
    for r in &k.data.rows {
        let Some(va) = hex(k.data.get(r, &k.data_va)) else {
            continue;
        };
        let name = k.data.get(r, "name");
        if name.is_empty() {
            continue;
        }
        let Some((isect, off)) = va.checked_sub(base).and_then(|rva| section_of(&secs, rva)) else {
            tally.outside += 1;
            continue;
        };
        tally.data += 1;
        data_vas.insert(va);
        let pub_name = match lib_symbol.get(&va) {
            Some((_, _, sym)) if !sym.is_empty() => {
                tally.library_publics += 1;
                sym.clone()
            }
            _ => name.to_string(),
        };
        add_pub(&mut publics, va, &pub_name, isect, off);
        let mut want = vec![name.to_string()];
        if pub_name != name {
            want.push(pub_name.clone());
        }
        expect.push(Expect {
            va,
            kind: "data",
            names: want,
        });
        let mut ty = k.data.get(r, "type").to_string();
        if ty.is_empty() {
            ty = gtype.get(&va).cloned().unwrap_or_default();
        }
        let ti = if ty.is_empty() {
            tally.data_no_type += 1;
            None
        } else {
            match parse_type(&ty) {
                Ok(c) => types.ti(&c)?,
                Err(_) => None,
            }
        };
        let in_code = img
            .sections
            .get(usize::from(isect).saturating_sub(1))
            .is_some_and(|s| s.flags & IMAGE_SCN_MEM_EXECUTE != 0);
        match (ti, in_code) {
            (Some(_), _) => tally.data_typed += 1,
            (None, true) => tally.data_in_code_public += 1,
            (None, false) => tally.data_as_byte += 1,
        }
        if ti.is_none() && !ty.is_empty() {
            *tally.data_untyped_type.entry(ty).or_default() += 1;
        }
        let Some(ti) = datum_type(ti, in_code) else {
            continue;
        };
        globs.push(Global {
            name: name.to_string(),
            isect,
            off,
            ti,
        });
    }
    tally.unnamed_typed_globals = gtype.keys().filter(|va| !data_vas.contains(va)).count();
    if let Some(l) = &k.library {
        for r in &l.rows {
            let Some(va) = hex(l.get(r, "va")) else {
                continue;
            };
            let kind = l.get(r, "kind");
            if kind == "chunk" || fn_vas.contains(&va) || data_vas.contains(&va) {
                continue;
            }
            let sym = l.get(r, "symbol");
            let name = if sym.is_empty() {
                l.get(r, "name")
            } else {
                sym
            };
            if name.is_empty() {
                continue;
            }
            let Some((isect, off)) = va.checked_sub(base).and_then(|rva| section_of(&secs, rva))
            else {
                tally.outside += 1;
                continue;
            };
            tally.library_publics += 1;
            add_pub(&mut publics, va, name, isect, off);
            let mut want = vec![name.to_string()];
            let plain = l.get(r, "name");
            if !plain.is_empty() && plain != name {
                want.push(plain.to_string());
            }
            expect.push(Expect {
                va,
                kind: "library",
                names: want,
            });
        }
    }
    tally.publics = publics.len();
    tally.udts = types.udts.len();
    tally.placeholders = types.placeholders.len();
    tally.unlaid = types.unlaid.len();
    tally.skipped_decls = types.skipped.len();
    tally.modules = modules.len();
    let spec = Spec {
        signature: identity.0,
        age: identity.1,
        sections: headers,
        modules: modules
            .into_iter()
            .map(|(m, mut procs)| {
                procs.sort_by_key(|p| (p.isect, p.off));
                let path = if k.modules.is_empty() {
                    m
                } else {
                    format!("{}/{m}", k.modules)
                };
                Module {
                    name: path.clone(),
                    obj: path,
                    procs,
                }
            })
            .collect(),
        publics,
        globals: globs,
        udts: types
            .udts
            .iter()
            .map(|(n, ti)| Udt {
                name: n.clone(),
                ti: *ti,
            })
            .collect(),
        types: std::mem::take(&mut types.records),
    };
    tally.types = spec.types.len();
    let written = build(&spec)?;
    expect.sort_by_key(|e| e.va);
    Ok(Authored {
        tally,
        expect,
        written,
        identity,
    })
}

#[must_use]
pub fn expect_tsv(expect: &[Expect]) -> String {
    let mut tsv = String::from("va\tkind\tnames\n");
    for e in expect {
        tsv.push_str(&format!(
            "{:08x}\t{}\t{}\n",
            e.va,
            e.kind,
            e.names.join("|")
        ));
    }
    tsv
}

#[must_use]
pub fn lines(a: &Authored) -> Vec<String> {
    let t = &a.tally;
    let w = &a.written;
    let mut out = vec![format!(
        "  functions {}: {} S_GPROC32 in {} module(s); typed {} ({} custom storage, {} no prototype, {} type not statable, {} unparsed); sizes clipped at the next code start {} (split bodies), none known {}",
        t.functions,
        w.procs,
        t.modules,
        t.typed,
        t.custom_storage,
        t.no_prototype,
        t.type_unstated,
        t.unparsed.values().sum::<usize>(),
        t.clipped,
        t.no_size
    )];
    for (why, n) in &t.unparsed {
        out.push(format!("    unparsed {n:4}  {why}"));
    }
    if t.name_mismatch > 0 {
        out.push(format!(
            "    {} prototype(s) spell a name other than the row's (the row's name is used)",
            t.name_mismatch
        ));
    }
    out.push(format!(
        "  data {}: {} typed S_GDATA32; {} with a type that cannot be stated and {} with none, of which {} outside code are S_GDATA32 T_UCHAR and {} inside code stay publics; {} typed global(s) the knowledge does not name (not emitted)",
        t.data,
        t.data_typed,
        t.data_untyped_type.values().sum::<usize>(),
        t.data_no_type,
        t.data_as_byte,
        t.data_in_code_public,
        t.unnamed_typed_globals
    ));
    for (ty, n) in t.data_untyped_type.iter().take(8) {
        out.push(format!("    unstated type {n:4}  {ty}"));
    }
    out.push(format!(
        "  publics {} ({} of them the library table's decorated linker names); type names (S_UDT) {}; type records {}",
        t.publics, t.library_publics, t.udts, t.types
    ));
    out.push(format!(
        "  types: {} name(s) no header declares (forward references); {} definition(s) not laid out; {} header declaration(s) outside the subset",
        t.placeholders, t.unlaid, t.skipped_decls
    ));
    out.push(format!(
        "  NOT carried: {} locals row(s) (decompiler variables, no frame offset), comments; {} row(s) outside every section",
        t.locals, t.outside
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_datum_is_data_outside_code_and_only_a_typed_one_is_data_inside_it() {
        assert_eq!(datum_type(None, false), Some(T_UCHAR));
        assert_eq!(datum_type(None, true), None);
        assert_eq!(datum_type(Some(0x1003), true), Some(0x1003));
        assert_eq!(datum_type(Some(0x1003), false), Some(0x1003));
    }

    #[test]
    fn a_tsv_reads_by_its_header_and_skips_comments() {
        let t = Tsv::parse("# a note\nva\tname\n\n00401000\tf\n00401010\t g \n");
        assert_eq!(t.rows.len(), 2);
        assert_eq!(t.get(&t.rows[1], "name"), "g");
        assert_eq!(t.get(&t.rows[0], "missing"), "");
        assert!(t.has("va") && !t.has("size"));
        assert_eq!(hex("0x10"), Some(0x10));
        assert_eq!(hex("00401000"), Some(0x40_1000));
    }
}
