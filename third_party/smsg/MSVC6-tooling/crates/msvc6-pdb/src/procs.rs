use crate::convert::walk_of;
use crate::dbi::{Dbi, NO_STREAM, section_rvas};
use crate::info::read_info;
use crate::le::{bytes, u16_at, u32_at, us};
use crate::lines::read_c13;
use crate::{msf, msf7, strtab, tpi};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::codec::{Gen, name_at, walk};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

const PROCEDURE_LEAVES: [u16; 4] = [0x0008, 0x0009, 0x1008, 0x1009];

struct Kinds {
    lproc: u16,
    gproc: u16,
    thunk: u16,
    block: u16,
    label: u16,
    publ: u16,
    ldata: u16,
    gdata: u16,
    lthread: u16,
    gthread: u16,
}

const ST: Kinds = Kinds {
    lproc: 0x100a,
    gproc: 0x100b,
    thunk: 0x0206,
    block: 0x0207,
    label: 0x0209,
    publ: 0x1009,
    ldata: 0x1007,
    gdata: 0x1008,
    lthread: 0x100e,
    gthread: 0x100f,
};

const SZ: Kinds = Kinds {
    lproc: 0x110f,
    gproc: 0x1110,
    thunk: 0x1102,
    block: 0x1103,
    label: 0x1105,
    publ: 0x110e,
    ldata: 0x110c,
    gdata: 0x110d,
    lthread: 0x1112,
    gthread: 0x1113,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Proc {
    pub rva: Option<u32>,
    pub len: u32,
    pub scope: &'static str,
    pub seg: u16,
    pub off: u32,
    pub typind: u32,
    pub leaf: Option<u16>,
    pub lines: usize,
    pub module: String,
    pub name: String,
}

impl Proc {
    #[must_use]
    pub fn typed(&self) -> bool {
        self.leaf.is_some_and(|l| PROCEDURE_LEAVES.contains(&l))
    }
}

#[derive(Debug, Default, Clone, Serialize)]
pub struct Children {
    pub live: usize,
    pub retired: usize,
    pub retired_in_live: usize,
    pub live_in_retired: usize,
    pub outside: usize,
    pub outside_retired: usize,
}

#[derive(Debug, Default, Clone, Serialize)]
pub struct LineStats {
    pub blocks: usize,
    pub pairs: usize,
    pub unsorted: usize,
    pub outside: usize,
    pub end_inclusive: usize,
    pub end_exclusive: usize,
    pub end_other: usize,
}

#[derive(Debug, Default, Clone)]
pub struct Listing {
    pub generation: Option<Gen>,
    pub procs: Vec<Proc>,
    pub thunks: Vec<Proc>,
    pub kinds: BTreeMap<u16, usize>,
    pub lines: LineStats,
    pub children: Children,
    pub names: Vec<Named>,
    pub code: Vec<bool>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum NameKind {
    Function,
    Data,
    Thunk,
    Label,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Named {
    pub rva: u32,
    pub name: String,
    pub kind: NameKind,
    pub size: Option<u32>,
    pub scope: Option<String>,
    pub record: &'static str,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct Block {
    seg: u16,
    start: u32,
    end: u32,
    offs: Vec<u32>,
}

fn c11_blocks(b: &[u8]) -> Result<Vec<Block>> {
    let mut out = Vec::new();
    for f in crate::lines::read_c11(b)? {
        for (seg, start, end, pairs) in f.blocks {
            if pairs.is_empty() {
                continue;
            }
            out.push(Block {
                seg,
                start,
                end,
                offs: pairs.iter().map(|p| p.0).collect(),
            });
        }
    }
    Ok(out)
}

enum Container {
    Small(msf::Msf2),
    Big(msf7::Msf7),
}

impl Container {
    fn get(&self, i: usize) -> Result<Vec<u8>> {
        match self {
            Self::Small(m) => m.stream(i),
            Self::Big(m) => m.stream(i),
        }
    }
}

#[must_use]
pub fn object_stem(module: &str) -> String {
    let base = module.rsplit(['\\', '/']).next().unwrap_or(module);
    base.rsplit_once('.').map_or(base, |(s, _)| s).to_string()
}

#[allow(clippy::too_many_lines)]
pub fn read(file: &[u8]) -> Result<Listing> {
    let (db, g) = if msf7::is_pdb70(file) {
        (Container::Big(msf7::Msf7::parse(file.to_vec())?), Gen::Sz)
    } else {
        (Container::Small(msf::Msf2::parse(file.to_vec())?), Gen::St)
    };
    let k = match g {
        Gen::St => &ST,
        Gen::Sz => &SZ,
    };
    let dbi = Dbi::parse(&db.get(3)?).context("stream 3, the DBI")?;
    let sh = dbi
        .section_headers()
        .context("no section-headers stream in the optional debug header: no rva to resolve to")?;
    let headers = db.get(usize::from(sh))?;
    let secs = section_rvas(&headers)?;
    let code: Vec<bool> = headers
        .as_chunks::<40>()
        .0
        .iter()
        .map(|h| u32_at(h, 36).is_ok_and(|c| c & 0x2000_0000 != 0))
        .collect();
    let t = db.get(2)?;
    let tpi = tpi::read(&t).context("stream 2, the TPI")?;
    let leaves: Vec<u16> = tpi
        .records
        .iter()
        .map(|r| u16_at(r, 2))
        .collect::<Result<_>>()?;
    let names_table = if g == Gen::Sz {
        let info = read_info(&db.get(1)?)?;
        match info.names.get(b"/names".as_slice()) {
            Some(i) => strtab::read(&db.get(us(*i))?)?,
            None => BTreeMap::new(),
        }
    } else {
        BTreeMap::new()
    };
    let rva_of = |seg: u16, off: u32| match seg {
        0 => None,
        s => secs
            .get(usize::from(s) - 1)
            .and_then(|base| base.checked_add(off)),
    };
    let mut l = Listing {
        generation: Some(g),
        code: code.clone(),
        ..Listing::default()
    };
    let mut seen = BTreeSet::new();
    for m in &dbi.mods {
        let stream = m.stream()?;
        let sym_len = m.sym_len()?;
        if stream == NO_STREAM || sym_len <= 4 {
            continue;
        }
        if !seen.insert(stream) {
            bail!(
                "module {}'s stream {stream} is another module's too",
                m.name_text()
            );
        }
        let s = db.get(usize::from(stream))?;
        let syms = s.get(4..sym_len.min(s.len())).ok_or_else(|| {
            anyhow!(
                "module {}'s symbol region runs past its stream",
                m.name_text()
            )
        })?;
        let (c11, c13) = (m.c11_len()?, m.c13_len()?);
        let blocks = if c11 == 0 {
            Vec::new()
        } else {
            c11_blocks(bytes(&s, sym_len, c11)?)?
        };
        let rows: Vec<(u16, u32)> = if c13 == 0 {
            Vec::new()
        } else {
            read_c13(bytes(&s, sym_len.saturating_add(c11), c13)?, &names_table)?
                .0
                .into_iter()
                .map(|(_, seg, off, _)| (seg, off))
                .collect()
        };
        let module = m.name_text();
        let first = l.procs.len();
        let mut scope: Option<(usize, bool)> = None;
        let mut at_lines: Vec<(u16, u32)> = blocks
            .iter()
            .flat_map(|b| b.offs.iter().map(|o| (b.seg, *o)))
            .chain(rows.iter().copied())
            .collect();
        at_lines.sort_unstable();
        let lines_in = |seg: u16, off: u32, plen: u32| {
            if seg == 0 {
                return 0;
            }
            let lo = at_lines.partition_point(|x| *x < (seg, off));
            let end = u64::from(off) + u64::from(plen);
            let hi = at_lines.partition_point(|x| (x.0, u64::from(x.1)) < (seg, end));
            hi.saturating_sub(lo)
        };
        for r in walk(syms, walk_of(g))? {
            *l.kinds.entry(r.kind).or_default() += 1;
            let pos = r.at;
            if scope.is_some_and(|(end, _)| pos + 4 >= end) {
                scope = None;
            }
            let p = r.payload;
            if (r.kind == k.lproc || r.kind == k.gproc) && p.len() >= 35 {
                let pend = u32_at(p, 4)?;
                let plen = u32_at(p, 12)?;
                let typind = u32_at(p, 24)?;
                let off = u32_at(p, 28)?;
                let seg = u16_at(p, 32)?;
                let leaf = typind
                    .checked_sub(tpi.ti_min)
                    .and_then(|i| leaves.get(us(i)).copied());
                let rva = rva_of(seg, off);
                scope = Some((us(pend), rva.is_some()));
                l.procs.push(Proc {
                    rva,
                    len: plen,
                    scope: if r.kind == k.gproc { "global" } else { "local" },
                    seg,
                    off,
                    typind,
                    leaf,
                    lines: lines_in(seg, off, plen),
                    module: module.clone(),
                    name: crate::le::latin1(name_at(p, 35, g)?.0),
                });
            } else if r.kind == k.thunk && p.len() >= 21 {
                let off = u32_at(p, 12)?;
                let seg = u16_at(p, 16)?;
                let tlen = u32::from(u16_at(p, 18)?);
                l.thunks.push(Proc {
                    rva: rva_of(seg, off),
                    len: tlen,
                    scope: "thunk",
                    seg,
                    off,
                    typind: 0,
                    leaf: None,
                    lines: lines_in(seg, off, tlen),
                    module: module.clone(),
                    name: crate::le::latin1(name_at(p, 21, g)?.0),
                });
            } else if r.kind == k.block || r.kind == k.label {
                let (at, name_at_) = if r.kind == k.label { (0, 7) } else { (12, 18) };
                let off = u32_at(p, at)?;
                let seg = u16_at(p, at + 4)?;
                let live = seg != 0;
                let c = &mut l.children;
                match scope {
                    Some((_, true)) if live => c.live += 1,
                    Some((_, true)) => c.retired_in_live += 1,
                    Some((_, false)) if live => c.live_in_retired += 1,
                    Some((_, false)) => c.retired += 1,
                    None if live => c.outside += 1,
                    None => c.outside_retired += 1,
                }
                if r.kind == k.label
                    && let Some(rva) = rva_of(seg, off)
                {
                    let name = crate::le::latin1(name_at(p, name_at_, g)?.0);
                    if !name.is_empty() {
                        l.names.push(Named {
                            rva,
                            name,
                            kind: NameKind::Label,
                            size: None,
                            scope: Some(object_stem(&module)),
                            record: "label",
                        });
                    }
                }
            } else if (r.kind == k.ldata || r.kind == k.lthread) && p.len() >= 10 {
                let off = u32_at(p, 4)?;
                let seg = u16_at(p, 8)?;
                if let Some(rva) = rva_of(seg, off) {
                    l.names.push(Named {
                        rva,
                        name: crate::le::latin1(name_at(p, 10, g)?.0),
                        kind: NameKind::Data,
                        size: None,
                        scope: Some(object_stem(&module)),
                        record: "static",
                    });
                }
            }
        }
        let mine = crate::lines::Holders::new(
            l.procs
                .get(first..)
                .unwrap_or_default()
                .iter()
                .map(|p| (p.seg, p.off, p.len))
                .collect(),
        );
        for b in &blocks {
            let st = &mut l.lines;
            st.blocks += 1;
            st.pairs += b.offs.len();
            st.unsorted += usize::from(b.offs.windows(2).any(|w| w.first() > w.last()));
            st.outside += b
                .offs
                .iter()
                .filter(|o| **o < b.start || **o > b.end)
                .count();
            let last = b.offs.iter().max().copied().unwrap_or(0);
            match mine.holder(b.seg, last).map(|p| p.1.saturating_add(p.2)) {
                Some(e) if b.end.checked_add(1) == Some(e) => st.end_inclusive += 1,
                Some(e) if b.end == e => st.end_exclusive += 1,
                _ => st.end_other += 1,
            }
        }
    }
    let symrec = dbi.symrecs()?;
    if symrec != NO_STREAM {
        let s = db.get(usize::from(symrec))?;
        for r in walk(&s, walk_of(g))? {
            let p = r.payload;
            let kind = if r.kind == k.publ {
                None
            } else if r.kind == k.gdata
                || r.kind == k.ldata
                || r.kind == k.gthread
                || r.kind == k.lthread
            {
                Some(NameKind::Data)
            } else {
                continue;
            };
            if p.len() < 10 {
                continue;
            }
            let off = u32_at(p, 4)?;
            let seg = u16_at(p, 8)?;
            let Some(rva) = rva_of(seg, off) else {
                continue;
            };
            let in_code = code.get(usize::from(seg) - 1).copied().unwrap_or(false);
            let kind = kind.unwrap_or(if in_code {
                NameKind::Function
            } else {
                NameKind::Data
            });
            l.names.push(Named {
                rva,
                name: crate::le::latin1(name_at(p, 10, g)?.0),
                kind,
                size: None,
                scope: None,
                record: if r.kind == k.publ { "public" } else { "global" },
            });
        }
    }
    for p in l.procs.iter().chain(&l.thunks) {
        if let Some(rva) = p.rva {
            l.names.push(Named {
                rva,
                name: p.name.clone(),
                kind: if p.scope == "thunk" {
                    NameKind::Thunk
                } else {
                    NameKind::Function
                },
                size: Some(p.len),
                scope: (p.scope == "local").then(|| object_stem(&p.module)),
                record: match p.scope {
                    "global" => "procedure",
                    "local" => "local procedure",
                    _ => "thunk",
                },
            });
        }
    }
    Ok(l)
}

#[must_use]
pub fn tsv(l: &Listing) -> String {
    let mut out = String::from("rva\tlen\tscope\ttypind\tleaf\tlines\tmodule\tname\n");
    for p in l.procs.iter().chain(&l.thunks) {
        out.push_str(&format!(
            "{}\t{}\t{}\t{:#x}\t{}\t{}\t{}\t{}\n",
            p.rva
                .map_or_else(|| "-".to_string(), |r| format!("{r:#08x}")),
            p.len,
            p.scope,
            p.typind,
            p.leaf
                .map_or_else(|| "-".to_string(), |l| format!("{l:#06x}")),
            p.lines,
            p.module,
            p.name
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_object_stem_drops_the_directory_and_the_extension() {
        assert_eq!(object_stem("C:\\work\\Release\\a.obj"), "a");
        assert_eq!(object_stem(".\\Release\\b.c.obj"), "b.c");
        assert_eq!(object_stem("crt0"), "crt0");
    }
}
