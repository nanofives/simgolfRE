use anyhow::{Context as _, Result, bail};
use msvc6_base::le;
use msvc6_base::srcio;
use msvc6_base::units::Rva;
use serde::Serialize;
use std::collections::BTreeSet;

use super::image::{Dir, Image};
use crate::us;

pub const DESCRIPTOR: u32 = 20;
pub const DEBUG_ENTRY: u32 = 28;
pub const DEBUG_TYPE_COFF: u32 = 1;
pub const DEBUG_TYPE_CODEVIEW: u32 = 2;
pub const DEBUG_TYPE_FPO: u32 = 3;
pub const DEBUG_TYPE_MISC: u32 = 4;
pub const RESOURCE_DEPTH: usize = 3;
pub const RES_NAME_ALLOWANCE: usize = 1 << 20;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ImportEntry {
    pub iat_rva: Rva,
    pub hint_rva: Option<Rva>,
    pub hint: Option<u16>,
    pub name: String,
    pub ordinal: Option<u16>,
}

impl ImportEntry {
    #[must_use]
    pub fn by_ordinal(&self) -> bool {
        self.ordinal.is_some()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ImportDll {
    pub index: usize,
    pub dll: String,
    pub descriptor: Rva,
    pub name_rva: Rva,
    pub ilt_rva: Rva,
    pub iat_rva: Rva,
    pub timestamp: u32,
    pub forwarder_chain: u32,
    pub entries: Vec<ImportEntry>,
}

impl ImportDll {
    #[must_use]
    pub fn slots(&self) -> u32 {
        u32::try_from(self.entries.len())
            .unwrap_or(u32::MAX)
            .saturating_add(1)
    }

    #[must_use]
    pub fn block_len(&self) -> u32 {
        self.slots().saturating_mul(4)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ExportEntry {
    pub name: Option<String>,
    pub ordinal: u32,
    pub target: Rva,
    pub forwarder: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Exports {
    pub dll_name: String,
    pub ordinal_base: u32,
    pub timestamp: u32,
    pub functions: u32,
    pub names: u32,
    pub entries: Vec<ExportEntry>,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum ResId {
    Name(String),
    Ordinal(u32),
}

impl std::fmt::Display for ResId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Ordinal(n) => write!(f, "{n}"),
            Self::Name(s) => write!(f, "{s}"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ResourceEntry {
    pub type_id: ResId,
    pub name_id: ResId,
    pub lang_id: ResId,
    pub codepage: u32,
    pub data_rva: Rva,
    pub data_len: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct DebugEntry {
    pub at: Rva,
    pub kind: u32,
    pub timestamp: u32,
    pub major: u16,
    pub minor: u16,
    pub size: u32,
    pub data_rva: Rva,
    pub data_ptr: u32,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CodeView {
    pub format: String,
    pub signature: u32,
    pub guid: Option<String>,
    pub age: u32,
    pub path: String,
    pub file_offset: u32,
    pub len: u32,
}

#[must_use]
pub fn debug_kind(kind: u32) -> &'static str {
    match kind {
        DEBUG_TYPE_COFF => "COFF",
        DEBUG_TYPE_CODEVIEW => "CodeView",
        DEBUG_TYPE_FPO => "FPO",
        DEBUG_TYPE_MISC => "MISC",
        5 => "EXCEPTION",
        6 => "FIXUP",
        7 => "OMAP_TO_SRC",
        8 => "OMAP_FROM_SRC",
        9 => "BORLAND",
        _ => "?",
    }
}

fn add(r: Rva, n: u32) -> Option<Rva> {
    r.checked_add(n)
}

fn dword(b: &[u8], k: usize) -> u32 {
    le::u32_at(b, k).unwrap_or(0)
}

#[derive(Debug)]
struct ResWalk {
    base: Rva,
    visited: BTreeSet<u32>,
    leaves: usize,
    leaf_cap: usize,
    units: usize,
    unit_cap: usize,
    out: Vec<ResourceEntry>,
}

impl ResWalk {
    fn charge(&mut self, ids: &[ResId]) -> Result<()> {
        for id in ids {
            if let ResId::Name(n) = id {
                self.units = self.units.saturating_add(n.len());
            }
        }
        if self.units > self.unit_cap {
            bail!(
                "resource names read {} bytes, more than the directory's file bytes can hold ({}); names overlap",
                self.units,
                self.unit_cap
            );
        }
        Ok(())
    }
}

struct Names<'a> {
    img: &'a Image,
    scanned: std::cell::Cell<usize>,
    budget: usize,
}

impl<'a> Names<'a> {
    fn new(img: &'a Image) -> Self {
        Self {
            img,
            scanned: std::cell::Cell::new(0),
            budget: img.name_budget(),
        }
    }

    fn read(&self, rva: Rva) -> Result<Option<String>> {
        let Some((s, n)) = self.img.cstr_scan(rva) else {
            return Ok(None);
        };
        let total = self.scanned.get().saturating_add(n);
        self.scanned.set(total);
        if total > self.budget {
            bail!(
                "the names read {total} bytes of a {}-byte file, more than {}; names overlap",
                self.img.len(),
                self.budget
            );
        }
        Ok(Some(s))
    }
}

impl Image {
    pub fn imports(&self) -> Result<Vec<ImportDll>> {
        let mut out = Vec::new();
        let Some(d) = self.present(Dir::Import) else {
            return Ok(out);
        };
        let room = self.raw_left(d.rva);
        let array = self.at_rva(d.rva, room).with_context(|| {
            format!(
                "import directory at rva {:#x} has no file bytes",
                d.rva.get()
            )
        })?;
        let mut count = 0usize;
        loop {
            let Some(desc) =
                array.get(count.saturating_mul(20)..count.saturating_add(1).saturating_mul(20))
            else {
                bail!(
                    "the import descriptors at rva {:#x} run past their section's file bytes with no null \
                     descriptor ({count} read)",
                    d.rva.get()
                );
            };
            let (ilt, name, iat) = (dword(desc, 0), dword(desc, 12), dword(desc, 16));
            if (ilt == 0 && iat == 0) || name == 0 {
                break;
            }
            count = count.saturating_add(1);
        }
        let array_end = d.rva.get().saturating_add(
            u32::try_from(count.saturating_add(1).saturating_mul(20)).unwrap_or(u32::MAX),
        );
        let inside = |r: u32| r >= d.rva.get() && r < array_end;
        let budget = self.len() / 4;
        let mut total = 0usize;
        let read = Names::new(self);
        for k in 0..count {
            let desc = array
                .get(k.saturating_mul(20)..k.saturating_add(1).saturating_mul(20))
                .unwrap_or_default();
            let (ilt, timestamp, forwarder_chain, name_rva, iat) = (
                dword(desc, 0),
                dword(desc, 4),
                dword(desc, 8),
                dword(desc, 12),
                dword(desc, 16),
            );
            let descriptor = Rva(d
                .rva
                .get()
                .saturating_add(u32::try_from(k.saturating_mul(20)).unwrap_or(u32::MAX)));
            for (what, r) in [("ILT", ilt), ("IAT", iat)] {
                if r != 0 && inside(r) {
                    bail!(
                        "import descriptor {k} at rva {:#x}: its {what} rva {r:#x} lies inside the descriptor array",
                        descriptor.get()
                    );
                }
            }
            let dll = read
                .read(Rva(name_rva))?
                .with_context(|| format!("import DLL name unreadable at {name_rva:#x}"))?;
            let table = Rva(if ilt != 0 { ilt } else { iat });
            let slots = self
                .at_rva(table, self.raw_left(table) / 4 * 4)
                .with_context(|| {
                    format!(
                        "{dll}: the thunk table at rva {:#x} has no file bytes",
                        table.get()
                    )
                })?;
            let mut entries = Vec::new();
            let mut ended = false;
            for (i, c) in slots.as_chunks::<4>().0.iter().enumerate() {
                let v = u32::from_le_bytes(*c);
                if v == 0 {
                    ended = true;
                    break;
                }
                total = total.saturating_add(1);
                if total > budget {
                    bail!(
                        "{dll}: more import entries than a {}-byte file can hold ({budget}); the thunk tables overlap",
                        self.len()
                    );
                }
                let slot =
                    Rva(iat.saturating_add(u32::try_from(i.saturating_mul(4)).unwrap_or(u32::MAX)));
                if v & 0x8000_0000 != 0 {
                    let ord = u16::try_from(v & 0xffff).unwrap_or(0);
                    entries.push(ImportEntry {
                        iat_rva: slot,
                        hint_rva: None,
                        hint: None,
                        name: format!("Ordinal({ord})"),
                        ordinal: Some(ord),
                    });
                } else {
                    let hint_rva = Rva(v);
                    entries.push(ImportEntry {
                        iat_rva: slot,
                        hint_rva: Some(hint_rva),
                        hint: self.u16_at_rva(hint_rva),
                        name: add(hint_rva, 2)
                            .map(|r| read.read(r))
                            .transpose()?
                            .flatten()
                            .unwrap_or_else(|| "<unreadable>".into()),
                        ordinal: None,
                    });
                }
            }
            if !ended {
                bail!(
                    "{dll}: the thunk table at rva {:#x} runs past its section's file bytes with no null thunk",
                    table.get()
                );
            }
            out.push(ImportDll {
                index: k,
                dll,
                descriptor,
                name_rva: Rva(name_rva),
                ilt_rva: Rva(ilt),
                iat_rva: Rva(iat),
                timestamp,
                forwarder_chain,
                entries,
            });
        }
        Ok(out)
    }

    fn table(&self, rva: Rva, n: u32, width: usize, what: &str) -> Result<&[u8]> {
        let len = us(n)
            .checked_mul(width)
            .with_context(|| format!("{what} {n} overflows"))?;
        self.at_rva(rva, len).with_context(|| {
            format!(
                "{what} {n}: the {len}-byte table at rva {:#x} runs past its section's file bytes",
                rva.get()
            )
        })
    }

    pub fn exports(&self) -> Result<Option<Exports>> {
        let Some(d) = self.present(Dir::Export) else {
            return Ok(None);
        };
        let head = self
            .at_rva(d.rva, 40)
            .context("export directory unreadable")?;
        let timestamp = dword(head, 4);
        let (name_rva, ordinal_base) = (dword(head, 12), dword(head, 16));
        let (functions, names) = (dword(head, 20), dword(head, 24));
        let addr = self.table(Rva(dword(head, 28)), functions, 4, "NumberOfFunctions")?;
        let ptrs = self.table(Rva(dword(head, 32)), names, 4, "NumberOfNames")?;
        let ords = self.table(Rva(dword(head, 36)), names, 2, "NumberOfNames")?;
        let in_dir = |r: u32| r >= d.rva.get() && r < d.rva.get().saturating_add(d.size);
        let read = Names::new(self);
        let entry = |index: u32, name: Option<String>| -> Result<ExportEntry> {
            let target = le::u32_at(addr, us(index).saturating_mul(4)).with_context(|| {
                format!("export ordinal index {index} is past NumberOfFunctions {functions}")
            })?;
            Ok(ExportEntry {
                name,
                ordinal: ordinal_base.saturating_add(index),
                target: Rva(target),
                forwarder: if in_dir(target) {
                    read.read(Rva(target))?
                } else {
                    None
                },
            })
        };
        let mut entries = Vec::with_capacity(us(names));
        let mut named = BTreeSet::new();
        for (p, o) in ptrs.as_chunks::<4>().0.iter().zip(ords.as_chunks::<2>().0) {
            let nr = u32::from_le_bytes(*p);
            let index = u32::from(u16::from_le_bytes(*o));
            let name = read
                .read(Rva(nr))?
                .with_context(|| format!("export name unreadable at rva {nr:#x}"))?;
            named.insert(index);
            entries.push(entry(index, Some(name))?);
        }
        for index in 0..functions {
            if named.contains(&index) {
                continue;
            }
            let e = entry(index, None)?;
            if e.target.get() != 0 {
                entries.push(e);
            }
        }
        Ok(Some(Exports {
            dll_name: read
                .read(Rva(name_rva))?
                .with_context(|| format!("export DLL name unreadable at rva {name_rva:#x}"))?,
            ordinal_base,
            timestamp,
            functions,
            names,
            entries,
        }))
    }

    pub fn resources(&self) -> Result<Vec<ResourceEntry>> {
        let Some(d) = self.present(Dir::Resource) else {
            return Ok(Vec::new());
        };
        let room = self.raw_left(d.rva);
        if room < 16 {
            bail!(
                "resource directory at rva {:#x}: its root does not fit in the {room} file byte(s) there",
                d.rva.get()
            );
        }
        let mut w = ResWalk {
            base: d.rva,
            visited: BTreeSet::new(),
            leaves: 0,
            leaf_cap: room / 16,
            units: 0,
            unit_cap: room
                .saturating_mul(super::image::NAME_BUDGET)
                .saturating_add(RES_NAME_ALLOWANCE),
            out: Vec::new(),
        };
        self.walk_res(&mut w, d.rva, &mut Vec::new())?;
        Ok(w.out)
    }

    fn res_id(&self, base: Rva, raw: u32) -> Result<ResId> {
        if raw & 0x8000_0000 == 0 {
            return Ok(ResId::Ordinal(raw));
        }
        let at = add(base, raw & 0x7fff_ffff).context("resource name offset overflows")?;
        let len = self
            .u16_at_rva(at)
            .with_context(|| format!("resource name at rva {:#x} unreadable", at.get()))?;
        let bytes = add(at, 2)
            .and_then(|r| self.at_rva(r, usize::from(len).saturating_mul(2)))
            .with_context(|| {
                format!(
                    "resource name at rva {:#x} runs past the file bytes",
                    at.get()
                )
            })?;
        let units: Vec<u16> = bytes
            .as_chunks::<2>()
            .0
            .iter()
            .map(|c| u16::from_le_bytes(*c))
            .collect();
        Ok(ResId::Name(String::from_utf16_lossy(&units)))
    }

    fn walk_res(&self, w: &mut ResWalk, node: Rva, path: &mut Vec<ResId>) -> Result<()> {
        if path.len() >= RESOURCE_DEPTH {
            bail!(
                "resource directory deeper than {RESOURCE_DEPTH} levels at {:#x}",
                node.get()
            );
        }
        if !w.visited.insert(node.get()) {
            bail!(
                "resource directory at {:#x} is reached twice: the tree has a cycle or a shared node",
                node.get()
            );
        }
        let head = self.at_rva(node, 16).with_context(|| {
            format!(
                "resource directory at {:#x}: its header runs past the file bytes",
                node.get()
            )
        })?;
        let total = usize::from(le::u16_at(head, 12).unwrap_or(0))
            .saturating_add(usize::from(le::u16_at(head, 14).unwrap_or(0)));
        let entries = add(node, 16)
            .and_then(|r| self.at_rva(r, total.saturating_mul(8)))
            .with_context(|| {
                format!(
                    "resource directory at {:#x}: its {total} entries run past the file bytes",
                    node.get()
                )
            })?;
        for e in entries.as_chunks::<8>().0 {
            let raw_id = dword(e, 0);
            let raw_next = dword(e, 4);
            let id = self.res_id(w.base, raw_id)?;
            w.charge(std::slice::from_ref(&id))?;
            path.push(id);
            if raw_next & 0x8000_0000 != 0 {
                let sub =
                    add(w.base, raw_next & 0x7fff_ffff).context("resource offset overflows")?;
                self.walk_res(w, sub, path)?;
            } else {
                let leaf = add(w.base, raw_next).context("resource offset overflows")?;
                w.leaves = w.leaves.saturating_add(1);
                if w.leaves > w.leaf_cap {
                    bail!(
                        "more resource leaves than the section's file bytes can hold ({})",
                        w.leaf_cap
                    );
                }
                let data = self.at_rva(leaf, 16).with_context(|| {
                    format!(
                        "resource data entry at {:#x} runs past the file bytes",
                        leaf.get()
                    )
                })?;
                w.charge(path)?;
                w.out.push(ResourceEntry {
                    type_id: path.first().cloned().unwrap_or(ResId::Ordinal(0)),
                    name_id: path.get(1).cloned().unwrap_or(ResId::Ordinal(0)),
                    lang_id: path.get(2).cloned().unwrap_or(ResId::Ordinal(0)),
                    codepage: dword(data, 8),
                    data_rva: Rva(dword(data, 0)),
                    data_len: us(dword(data, 4)),
                });
            }
            path.pop();
        }
        Ok(())
    }

    #[must_use]
    pub fn debug_entries(&self) -> Vec<DebugEntry> {
        let Some(d) = self.present(Dir::Debug) else {
            return Vec::new();
        };
        let mut out = Vec::new();
        for i in 0..d.size / DEBUG_ENTRY {
            let Some(e) = i.checked_mul(DEBUG_ENTRY).and_then(|k| add(d.rva, k)) else {
                break;
            };
            let Some(b) = self.at_rva(e, us(DEBUG_ENTRY)) else {
                break;
            };
            let g = |k: usize| le::u32_at(b, k).unwrap_or(0);
            let w = |k: usize| le::u16_at(b, k).unwrap_or(0);
            out.push(DebugEntry {
                at: e,
                timestamp: g(4),
                major: w(8),
                minor: w(10),
                kind: g(12),
                size: g(16),
                data_rva: Rva(g(20)),
                data_ptr: g(24),
            });
        }
        out
    }

    pub fn codeview(&self) -> Result<Option<CodeView>> {
        let Some(e) = self
            .debug_entries()
            .into_iter()
            .find(|e| e.kind == DEBUG_TYPE_CODEVIEW)
        else {
            return Ok(None);
        };
        let bytes = le::bytes_at(self.bytes(), us(e.data_ptr), us(e.size)).with_context(|| {
            format!(
                "CodeView record at {:#x} + {} runs past the file",
                e.data_ptr, e.size
            )
        })?;
        let format = srcio::decode(bytes.get(..4).unwrap_or_default());
        let g = |k: usize| le::u32_at(bytes, k).unwrap_or(0);
        let cstr = |from: usize| {
            let tail = bytes.get(from..).unwrap_or_default();
            let end = tail.iter().position(|b| *b == 0).unwrap_or(tail.len());
            srcio::decode(tail.get(..end).unwrap_or_default())
        };
        let cv = if format == "RSDS" {
            let guid = bytes
                .get(4..20)
                .map(|g| g.iter().map(|b| format!("{b:02x}")).collect::<String>());
            CodeView {
                format,
                signature: 0,
                guid,
                age: g(20),
                path: cstr(24),
                file_offset: e.data_ptr,
                len: e.size,
            }
        } else if format.starts_with("NB") && format != "NB10" {
            CodeView {
                format,
                signature: 0,
                guid: None,
                age: 0,
                path: String::new(),
                file_offset: e.data_ptr,
                len: e.size,
            }
        } else {
            CodeView {
                format,
                signature: g(8),
                guid: None,
                age: g(12),
                path: cstr(16),
                file_offset: e.data_ptr,
                len: e.size,
            }
        };
        Ok(Some(cv))
    }
}
