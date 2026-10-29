use anyhow::{Context as _, Result, bail};
use msvc6_base::le;
use msvc6_base::srcio;
use serde::Serialize;
use std::collections::BTreeMap;

use crate::coff::{ImportObject, Object, looks_like_object};
use crate::us;

pub const MAGIC: &[u8; 8] = b"!<arch>\n";
pub const MEMBER_HEADER: usize = 60;
pub const LONG_NAME_MAX: usize = 1024;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub enum MemberKind {
    Linker,
    Longnames,
    Object,
    Import,
    Other,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Member {
    pub index: usize,
    pub name: String,
    pub header_name: String,
    pub date: u64,
    pub header_off: usize,
    pub data_off: usize,
    pub size: usize,
    pub kind: MemberKind,
}

impl Member {
    #[must_use]
    pub fn stem(&self) -> &str {
        self.name.rsplit(['\\', '/']).next().unwrap_or(&self.name)
    }

    #[must_use]
    pub fn is_content(&self) -> bool {
        !matches!(self.kind, MemberKind::Linker | MemberKind::Longnames)
    }
}

#[derive(Debug, Clone)]
pub struct Archive<'a> {
    data: &'a [u8],
    pub members: Vec<Member>,
    pub long_names_read: usize,
}

fn field(h: &[u8], at: usize, len: usize) -> String {
    srcio::decode(h.get(at..at.saturating_add(len)).unwrap_or_default())
        .trim_end()
        .to_string()
}

fn long_name(table: &[u8], off: usize) -> Result<String> {
    let s = table.get(off..).with_context(|| {
        format!(
            "long name offset {off} is past the {}-byte name table",
            table.len()
        )
    })?;
    let window = s.get(..LONG_NAME_MAX.saturating_add(1)).unwrap_or(s);
    let end = match window.iter().position(|b| *b == 0 || *b == b'\n') {
        Some(e) => e,
        None if s.len() <= LONG_NAME_MAX => s.len(),
        None => bail!("the long name at offset {off} runs past {LONG_NAME_MAX} bytes"),
    };
    let name = srcio::decode(s.get(..end).unwrap_or_default());
    Ok(name.strip_suffix('/').unwrap_or(&name).to_string())
}

#[must_use]
pub fn is_archive(d: &[u8]) -> bool {
    d.starts_with(MAGIC)
}

impl<'a> Archive<'a> {
    pub fn parse(data: &'a [u8]) -> Result<Self> {
        if !is_archive(data) {
            bail!("not an archive: no `!<arch>` signature");
        }
        let mut members = Vec::new();
        let mut longnames: &[u8] = &[];
        let mut names: BTreeMap<usize, String> = BTreeMap::new();
        let mut long_names_read = 0usize;
        let mut at = MAGIC.len();
        while at < data.len() {
            let h = le::bytes_at(data, at, MEMBER_HEADER)
                .with_context(|| format!("archive member header at {at:#x} is truncated"))?;
            if h.get(58..60) != Some(&b"`\n"[..]) {
                bail!("archive member header at {at:#x} has no end marker");
            }
            let header_name = field(h, 0, 16);
            let size: usize = field(h, 48, 10)
                .parse()
                .with_context(|| format!("archive member at {at:#x}: unreadable size"))?;
            let data_off = at.saturating_add(MEMBER_HEADER);
            let body = le::bytes_at(data, data_off, size)
                .with_context(|| format!("archive member at {at:#x} runs past the file"))?;
            let (name, kind) = match header_name.as_str() {
                "/" | "/SYM64/" | "__.SYMDEF" => (header_name.clone(), MemberKind::Linker),
                "//" => {
                    longnames = body;
                    names.clear();
                    (header_name.clone(), MemberKind::Longnames)
                }
                n => {
                    let name = match n.strip_prefix('/').and_then(|k| k.parse::<usize>().ok()) {
                        Some(off) => match names.get(&off) {
                            Some(s) => s.clone(),
                            None => {
                                let s = long_name(longnames, off).with_context(|| {
                                    format!("archive member at {at:#x} names `{n}`")
                                })?;
                                names.insert(off, s.clone());
                                long_names_read += 1;
                                s
                            }
                        },
                        None => n.strip_suffix('/').unwrap_or(n).to_string(),
                    };
                    let kind = if ImportObject::parse(body).is_some() {
                        MemberKind::Import
                    } else if looks_like_object(body) {
                        MemberKind::Object
                    } else {
                        MemberKind::Other
                    };
                    (name, kind)
                }
            };
            members.push(Member {
                index: members.len(),
                name,
                header_name,
                date: field(h, 16, 12).parse().unwrap_or(0),
                header_off: at,
                data_off,
                size,
                kind,
            });
            at = data_off.saturating_add(size).saturating_add(size % 2);
        }
        Ok(Self {
            data,
            members,
            long_names_read,
        })
    }

    #[must_use]
    pub fn data(&self, m: &Member) -> &'a [u8] {
        le::bytes_at(self.data, m.data_off, m.size).unwrap_or_default()
    }

    pub fn content(&self) -> impl Iterator<Item = (&Member, &'a [u8])> {
        self.members
            .iter()
            .filter(|m| m.is_content())
            .map(|m| (m, self.data(m)))
    }

    #[must_use]
    pub fn objects(&self) -> Vec<(&Member, Object<'a>)> {
        self.content()
            .filter(|(m, _)| m.kind == MemberKind::Object)
            .filter_map(|(m, d)| Object::parse(d).ok().map(|o| (m, o)))
            .collect()
    }

    #[must_use]
    pub fn symbol_index(&self) -> Vec<(String, usize)> {
        let Some(first) = self.members.iter().find(|m| m.kind == MemberKind::Linker) else {
            return Vec::new();
        };
        let d = self.data(first);
        let Some(n) = d
            .get(..4)
            .and_then(|b| <[u8; 4]>::try_from(b).ok())
            .map(u32::from_be_bytes)
        else {
            return Vec::new();
        };
        let n = us(n);
        let mut offsets = Vec::with_capacity(n.min(d.len() / 4));
        for i in 0..n {
            let at = 4usize.saturating_add(i.saturating_mul(4));
            let Some(v) = d
                .get(at..at.saturating_add(4))
                .and_then(|b| <[u8; 4]>::try_from(b).ok())
                .map(u32::from_be_bytes)
            else {
                return Vec::new();
            };
            offsets.push(us(v));
        }
        let names_at = 4usize.saturating_add(n.saturating_mul(4));
        let names = d.get(names_at..).unwrap_or_default();
        names
            .split(|b| *b == 0)
            .zip(offsets)
            .map(|(s, o)| (srcio::decode(s), o))
            .collect()
    }

    #[must_use]
    pub fn member_at(&self, header_off: usize) -> Option<&Member> {
        self.members.iter().find(|m| m.header_off == header_off)
    }

    #[must_use]
    pub fn import_order(&self) -> Vec<String> {
        let mut by_member: BTreeMap<usize, Vec<String>> = BTreeMap::new();
        for (s, o) in self.symbol_index() {
            by_member.entry(o).or_default().push(s);
        }
        let mut out = Vec::new();
        for syms in by_member.values() {
            if let Some(s) = syms.iter().find(|s| s.starts_with("__imp__")) {
                out.push(undecorate_import(s));
            }
        }
        out
    }
}

#[must_use]
pub fn undecorate_import(s: &str) -> String {
    let bare = s.strip_prefix("__imp__").unwrap_or(s);
    bare.rsplit_once('@')
        .filter(|(_, n)| !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()))
        .map_or(bare, |(head, _)| head)
        .to_string()
}
