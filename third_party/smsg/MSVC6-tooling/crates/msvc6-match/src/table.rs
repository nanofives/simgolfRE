use anyhow::{Context as _, Result, bail};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;
use std::path::Path;

use crate::locate::{Placed, Recorded, Run};

pub const FORMAT: u32 = 1;

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct ArchiveRow {
    pub name: String,
    pub path: String,
    pub sha256: String,
    #[serde(default)]
    pub copies: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct MemberRow {
    pub rva: u32,
    pub len: u32,
    pub member: String,
    #[serde(default)]
    pub archive: String,
    #[serde(default, skip_serializing_if = "std::ops::Not::not")]
    pub carried: bool,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct PointRow {
    pub rva: u32,
    pub name: String,
    pub member: String,
    #[serde(default, skip_serializing_if = "std::ops::Not::not")]
    pub carried: bool,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Table {
    pub format: Option<u32>,
    pub image: Option<String>,
    pub image_sha256: Option<String>,
    #[serde(default)]
    pub archive: Vec<ArchiveRow>,
    #[serde(default)]
    pub import: Vec<PointRow>,
    #[serde(default)]
    pub member: Vec<MemberRow>,
    #[serde(default)]
    pub byname: Vec<PointRow>,
}

impl Table {
    pub fn parse(text: &str, file: &Path) -> Result<Self> {
        let t: Self =
            toml::from_str(text).map_err(|e| anyhow::anyhow!("{}: {e}", file.display()))?;
        if let Some(f) = t.format
            && f != FORMAT
        {
            bail!(
                "{}: library table format {f}; this build reads {FORMAT}",
                file.display()
            );
        }
        Ok(t)
    }

    pub fn read(file: &Path) -> Result<Self> {
        let text = msvc6_base::srcio::own_read(file)
            .with_context(|| format!("reading {}", file.display()))?;
        Self::parse(&text, file)
    }

    #[must_use]
    pub fn recorded(&self) -> Vec<Recorded> {
        self.member
            .iter()
            .map(|m| Recorded {
                rva: m.rva,
                len: m.len,
                member: m.member.clone(),
                archive: m.archive.clone(),
            })
            .collect()
    }

    #[must_use]
    pub fn of_run(run: &Run) -> Self {
        let mut member: Vec<MemberRow> = Vec::new();
        let mut import: Vec<PointRow> = Vec::new();
        for p in run
            .members
            .iter()
            .filter(|p| p.present() && !p.ranges.is_empty())
        {
            for (a, b) in &p.ranges {
                member.push(MemberRow {
                    rva: *a,
                    len: b.saturating_sub(*a),
                    member: p.member.clone(),
                    archive: p.archive.clone(),
                    carried: false,
                });
            }
            for (name, rva) in &p.imports {
                import.push(PointRow {
                    rva: *rva,
                    name: name.clone(),
                    member: p.member.clone(),
                    carried: false,
                });
            }
        }
        member.sort_by(|a, b| {
            (a.rva, a.len, &a.member, &a.archive).cmp(&(b.rva, b.len, &b.member, &b.archive))
        });
        member.dedup();
        import.sort_by(|a, b| (&a.member, a.rva, &a.name).cmp(&(&b.member, b.rva, &b.name)));
        import.dedup();
        Self {
            format: Some(FORMAT),
            member,
            import,
            ..Self::default()
        }
    }

    pub fn carry(&mut self, previous: &Self) -> usize {
        let derived: BTreeSet<(&str, u32)> = self
            .member
            .iter()
            .map(|m| ("member", m.rva))
            .chain(self.import.iter().map(|m| ("import", m.rva)))
            .chain(self.byname.iter().map(|m| ("byname", m.rva)))
            .collect();
        let members: Vec<MemberRow> = previous
            .member
            .iter()
            .filter(|m| !derived.contains(&("member", m.rva)))
            .map(|m| MemberRow {
                carried: true,
                ..m.clone()
            })
            .collect();
        let point = |kind: &str, rows: &[PointRow]| -> Vec<PointRow> {
            rows.iter()
                .filter(|m| !derived.contains(&(kind, m.rva)))
                .map(|m| PointRow {
                    carried: true,
                    ..m.clone()
                })
                .collect()
        };
        let imports = point("import", &previous.import);
        let bynames = point("byname", &previous.byname);
        let n = members
            .len()
            .saturating_add(imports.len())
            .saturating_add(bynames.len());
        self.member.extend(members);
        self.import.extend(imports);
        self.byname.extend(bynames);
        n
    }

    #[must_use]
    pub fn render(&self) -> String {
        let mut s = String::new();
        let _ = writeln!(s, "format = {}", self.format.unwrap_or(FORMAT));
        if let Some(i) = &self.image {
            let _ = writeln!(s, "image = {}", quote(i));
        }
        if let Some(i) = &self.image_sha256 {
            let _ = writeln!(s, "image_sha256 = {}", quote(i));
        }
        for a in &self.archive {
            let _ = write!(
                s,
                "\n[[archive]]\nname = {}\npath = {}\nsha256 = {}\ncopies = {}\n",
                quote(&a.name),
                quote(&a.path),
                quote(&a.sha256),
                a.copies
            );
        }
        let carried = |c: bool| if c { "carried = true\n" } else { "" };
        for m in &self.import {
            let _ = write!(
                s,
                "\n[[import]]\nrva = {:#x}\nname = {}\nmember = {}\n{}",
                m.rva,
                quote(&m.name),
                quote(&m.member),
                carried(m.carried)
            );
        }
        for m in &self.member {
            let _ = write!(
                s,
                "\n[[member]]\nrva = {:#x}\nlen = {:#x}\nmember = {}\narchive = {}\n{}",
                m.rva,
                m.len,
                quote(&m.member),
                quote(&m.archive),
                carried(m.carried)
            );
        }
        for m in &self.byname {
            let _ = write!(
                s,
                "\n[[byname]]\nrva = {:#x}\nname = {}\nmember = {}\n{}",
                m.rva,
                quote(&m.name),
                quote(&m.member),
                carried(m.carried)
            );
        }
        s
    }
}

#[must_use]
pub fn quote(s: &str) -> String {
    if !s.contains('\'') && !s.chars().any(char::is_control) {
        return format!("'{s}'");
    }
    let mut out = String::from("\"");
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            c if c.is_control() => {
                let _ = write!(out, "\\u{:04x}", u32::from(c));
            }
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Difference {
    pub rva: u32,
    pub len: u32,
    pub member: String,
    pub placed: Option<String>,
    pub placed_len: Option<u32>,
}

impl Difference {
    #[must_use]
    pub fn line(&self) -> String {
        let head = format!(
            "    RECORD {:#08x} len {:#x}: {}",
            self.rva, self.len, self.member
        );
        match (&self.placed, self.placed_len) {
            (Some(_), Some(l)) => format!("{head} -- placed len {l:#x}"),
            (Some(p), None) => format!("{head} -- placed {p}"),
            (None, _) => format!("{head} -- nothing placed there"),
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Unrecorded {
    pub rva: u32,
    pub len: u32,
    pub member: String,
    pub archive: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Comparison {
    pub rows: usize,
    pub judged: usize,
    pub rederived: usize,
    pub differences: Vec<Difference>,
    pub unrecorded: Vec<Unrecorded>,
}

#[must_use]
pub fn compare(record: &[Recorded], present: &[&Placed]) -> Comparison {
    let kept: BTreeMap<u32, (u32, &Placed)> = present
        .iter()
        .flat_map(|p| p.ranges.iter().map(move |(a, b)| (*a, (*b, *p))))
        .collect();
    let read = |archive: &str| {
        present
            .iter()
            .any(|p| p.archive.eq_ignore_ascii_case(archive))
    };
    let mut c = Comparison {
        rows: record.len(),
        ..Comparison::default()
    };
    for m in record {
        let end = m.rva.saturating_add(m.len);
        match kept.get(&m.rva) {
            Some((e, p)) if p.member.eq_ignore_ascii_case(&m.member) => {
                c.judged = c.judged.saturating_add(1);
                if *e == end {
                    c.rederived = c.rederived.saturating_add(1);
                } else {
                    c.differences.push(Difference {
                        rva: m.rva,
                        len: m.len,
                        member: m.member.clone(),
                        placed: Some(p.member.clone()),
                        placed_len: Some(e.saturating_sub(m.rva)),
                    });
                }
            }
            Some((_, p)) => {
                c.judged = c.judged.saturating_add(1);
                c.differences.push(Difference {
                    rva: m.rva,
                    len: m.len,
                    member: m.member.clone(),
                    placed: Some(p.member.clone()),
                    placed_len: None,
                });
            }
            None if read(&m.archive) => {
                c.judged = c.judged.saturating_add(1);
                c.differences.push(Difference {
                    rva: m.rva,
                    len: m.len,
                    member: m.member.clone(),
                    placed: None,
                    placed_len: None,
                });
            }
            None => {}
        }
    }
    let held: BTreeSet<(u32, u32, String)> = record
        .iter()
        .map(|m| {
            (
                m.rva,
                m.rva.saturating_add(m.len),
                m.member.to_ascii_lowercase(),
            )
        })
        .collect();
    for (a, (b, p)) in &kept {
        if !held.contains(&(*a, *b, p.member.to_ascii_lowercase())) {
            c.unrecorded.push(Unrecorded {
                rva: *a,
                len: b.saturating_sub(*a),
                member: p.member.clone(),
                archive: p.archive.clone(),
            });
        }
    }
    c
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn a_table_round_trips_through_its_text() {
        let t = Table {
            format: Some(FORMAT),
            image: Some("sample.dll".into()),
            image_sha256: None,
            archive: vec![ArchiveRow {
                name: "libc".into(),
                path: "/x/LIBC.LIB".into(),
                sha256: "00".into(),
                copies: 2,
            }],
            import: vec![PointRow {
                rva: 0x2000,
                name: "__amsg_exit".into(),
                member: "build\\obj\\a.obj".into(),
                carried: false,
            }],
            member: vec![
                MemberRow {
                    rva: 0x1000,
                    len: 0x31,
                    member: "build\\obj\\a.obj".into(),
                    archive: "libc".into(),
                    carried: false,
                },
                MemberRow {
                    rva: 0x1040,
                    len: 0x8,
                    member: "it's.obj".into(),
                    archive: "libc".into(),
                    carried: true,
                },
            ],
            byname: Vec::new(),
        };
        let text = t.render();
        assert!(text.contains("member = 'build\\obj\\a.obj'"));
        assert!(text.contains("member = \"it's.obj\""));
        assert!(!text.contains('#'));
        assert_eq!(Table::parse(&text, &PathBuf::from("t.toml")).unwrap(), t);
    }

    #[test]
    fn a_hand_row_survives_a_point_row_at_its_address_and_a_rederived_row_does_not() {
        let previous = Table::parse(
            "[[member]]\nrva = 0x17d70\nlen = 0x3e\nmember = 'fread.obj'\narchive = 'libc'\n\n\
             [[member]]\nrva = 0x22222\nlen = 0x10\nmember = 'other.obj'\narchive = 'libc'\n",
            &PathBuf::from("p.toml"),
        )
        .unwrap();
        let mut fresh = Table {
            import: vec![PointRow {
                rva: 0x17d70,
                name: "_fread".into(),
                member: "x.obj".into(),
                carried: false,
            }],
            member: vec![MemberRow {
                rva: 0x22222,
                len: 0x10,
                member: "other.obj".into(),
                archive: "libc".into(),
                carried: false,
            }],
            ..Table::default()
        };
        assert_eq!(fresh.carry(&previous), 1);
        let carried: Vec<&MemberRow> = fresh.member.iter().filter(|m| m.carried).collect();
        assert_eq!(carried.len(), 1);
        assert_eq!(carried[0].rva, 0x17d70);
    }

    #[test]
    fn an_unknown_key_or_another_format_is_refused() {
        let f = PathBuf::from("t.toml");
        assert!(
            Table::parse("[[member]]\nrva = 1\nlen = 1\nmember = 'a'\nsize = 3\n", &f).is_err()
        );
        assert!(Table::parse("format = 2\n", &f).is_err());
        assert!(
            Table::parse(
                "# a comment\n[[byname]]\nrva = 1\nname = 'x'\nmember = 'a'\n",
                &f
            )
            .is_ok()
        );
    }
}
