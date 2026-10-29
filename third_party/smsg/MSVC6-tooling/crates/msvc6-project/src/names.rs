use crate::config::{NameSource, Project};
use crate::registry::Resolved;
use crate::spans::{index, join};
use anyhow::{Context as _, Result, bail};
use msvc6_image::pe::Image;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Kind {
    Function,
    Data,
    Import,
    Thunk,
    String,
    Vftable,
    Label,
    Absolute,
}

pub const KINDS: [Kind; 8] = [
    Kind::Function,
    Kind::Data,
    Kind::Import,
    Kind::Thunk,
    Kind::String,
    Kind::Vftable,
    Kind::Label,
    Kind::Absolute,
];

impl Kind {
    #[must_use]
    pub fn key(self) -> &'static str {
        match self {
            Self::Function => "function",
            Self::Data => "data",
            Self::Import => "import",
            Self::Thunk => "thunk",
            Self::String => "string",
            Self::Vftable => "vftable",
            Self::Label => "label",
            Self::Absolute => "absolute",
        }
    }

    pub fn parse(s: &str) -> Result<Self> {
        KINDS.into_iter().find(|k| k.key() == s).with_context(|| {
            format!(
                "`{s}` is no name kind; the kinds are {}",
                KINDS.map(Kind::key).join(", ")
            )
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Name {
    pub rva: u32,
    pub name: String,
    pub kind: Kind,
    pub size: Option<u32>,
    pub scope: Option<String>,
    pub provider: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ProviderStat {
    pub provider: String,
    pub source: String,
    pub names: usize,
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Default)]
pub struct NameOracle {
    names: Vec<Name>,
    by_rva: BTreeMap<u32, Vec<usize>>,
    pub stats: Vec<ProviderStat>,
}

impl NameOracle {
    pub fn add(&mut self, provider: &str, source: &str, ns: Vec<Name>, notes: Vec<String>) {
        let count = ns.len();
        for n in ns {
            self.by_rva.entry(n.rva).or_default().push(self.names.len());
            self.names.push(n);
        }
        self.stats.push(ProviderStat {
            provider: provider.to_string(),
            source: source.to_string(),
            names: count,
            notes,
        });
    }

    #[must_use]
    pub fn at(&self, rva: u32) -> Option<&Name> {
        self.by_rva
            .get(&rva)
            .and_then(|v| v.first())
            .and_then(|i| self.names.get(*i))
    }

    #[must_use]
    pub fn all_at(&self, rva: u32) -> Vec<&Name> {
        self.by_rva
            .get(&rva)
            .map(|v| v.iter().filter_map(|i| self.names.get(*i)).collect())
            .unwrap_or_default()
    }

    #[must_use]
    pub fn lookup(&self, name: &str) -> Vec<&Name> {
        self.names.iter().filter(|n| n.name == name).collect()
    }

    #[must_use]
    pub fn bodies(&self, name: &str) -> Vec<&Name> {
        let all = self.lookup(name);
        if all.iter().any(|n| n.kind != Kind::Thunk) {
            all.into_iter().filter(|n| n.kind != Kind::Thunk).collect()
        } else {
            all
        }
    }

    #[must_use]
    pub fn named_bodies(&self) -> std::collections::HashSet<&str> {
        self.names
            .iter()
            .filter(|n| n.kind != Kind::Thunk && n.kind != Kind::Import && n.scope.is_none())
            .map(|n| n.name.as_str())
            .collect()
    }

    pub fn iter(&self) -> impl Iterator<Item = &Name> {
        self.names.iter()
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.names.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.names.is_empty()
    }
}

fn hex(s: &str) -> Option<u32> {
    let t = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X"))?;
    u32::from_str_radix(t, 16).ok()
}

pub fn parse_tsv(path: &Path, text: &str) -> Result<Vec<Name>> {
    let mut out = Vec::new();
    for (i, raw) in text.lines().enumerate() {
        let n = i.saturating_add(1);
        let l = raw.trim_end_matches('\r');
        if l.trim().is_empty() || (n == 1 && l.starts_with("rva\t")) {
            continue;
        }
        let f: Vec<&str> = l.split('\t').collect();
        if !(3..=5).contains(&f.len()) {
            bail!(
                "{}:{n}: {} column(s); a row is `rva name kind [size] [scope]`, tab-separated",
                path.display(),
                f.len()
            );
        }
        let col = |k: usize| f.get(k).copied().unwrap_or_default();
        let rva = hex(col(0)).with_context(|| {
            format!(
                "{}:{n}: rva `{}` is not 0x-hexadecimal",
                path.display(),
                col(0)
            )
        })?;
        let name = col(1);
        if name.is_empty() || name.contains(char::is_whitespace) {
            bail!(
                "{}:{n}: name `{name}` is empty or holds white space",
                path.display()
            );
        }
        let kind = Kind::parse(col(2)).with_context(|| format!("{}:{n}", path.display()))?;
        let size =
            match f.get(3).filter(|s| !s.is_empty()) {
                Some(s) => Some(s.parse::<u32>().ok().or_else(|| hex(s)).with_context(|| {
                    format!("{}:{n}: size `{s}` is not a number", path.display())
                })?),
                None => None,
            };
        out.push(Name {
            rva,
            name: name.to_string(),
            kind,
            size,
            scope: f.get(4).filter(|s| !s.is_empty()).map(|s| (*s).to_string()),
            provider: "tsv".to_string(),
        });
    }
    Ok(out)
}

fn stem(object: &str) -> String {
    object
        .rsplit_once('.')
        .map_or_else(|| object.to_string(), |(s, _)| s.to_string())
}

pub fn read_one(
    p: &Project,
    n: usize,
    i: usize,
    src: &NameSource,
    img: &Image,
) -> Result<(String, String, Vec<Name>, Vec<String>)> {
    let d = p
        .images
        .get(n)
        .ok_or_else(|| anyhow::anyhow!("no image[{n}]"))?;
    let key = index(&join(&p.image_key(n), "names"), i);
    let base = img.opt.image_base.get();
    let fail = |e: anyhow::Error| p.error(&key, format!("{e:#}"));
    Ok(match src {
        NameSource::Tsv(f) => {
            let path = p.path_at(&key, f)?;
            let text = msvc6_base::srcio::own_read(&path)
                .map_err(|e| fail(anyhow::anyhow!("reading {}: {e}", path.display())))?;
            let ns = parse_tsv(&path, &text).map_err(fail)?;
            ("tsv".into(), path.display().to_string(), ns, Vec::new())
        }
        NameSource::Map(f) => {
            let raw = match f.as_ref().or(d.map.as_ref()) {
                Some(r) => r.clone(),
                None => return Err(p.error(&key, "`map` names no file and the image has no `map`")),
            };
            let path = p.path_at(&key, &raw)?;
            let m = crate::mapfile::Map::read(&path).map_err(fail)?;
            let ns = m
                .symbols
                .iter()
                .map(|s| Name {
                    rva: s.rva,
                    name: s.name.clone(),
                    kind: if s.function {
                        Kind::Function
                    } else {
                        Kind::Data
                    },
                    size: None,
                    scope: s.is_static.then(|| stem(&s.object)),
                    provider: "map".into(),
                })
                .collect();
            ("map".into(), path.display().to_string(), ns, Vec::new())
        }
        NameSource::Exports => {
            let ns = match img.exports()? {
                Some(e) => e
                    .entries
                    .iter()
                    .filter(|x| x.forwarder.is_none())
                    .map(|x| Name {
                        rva: x.target.get(),
                        name: x.name.clone().unwrap_or_else(|| format!("#{}", x.ordinal)),
                        kind: if img
                            .section_of(x.target)
                            .is_some_and(|s| crate::discover::is_code_section(&s.name, s.flags))
                        {
                            Kind::Function
                        } else {
                            Kind::Data
                        },
                        size: None,
                        scope: None,
                        provider: "exports".into(),
                    })
                    .collect(),
                None => Vec::new(),
            };
            ("exports".into(), "the image".into(), ns, Vec::new())
        }
        NameSource::Imports => {
            let mut ns = Vec::new();
            for dll in img.imports()? {
                for e in &dll.entries {
                    let what = match e.ordinal {
                        Some(o) => format!("#{o}"),
                        _ => e.name.clone(),
                    };
                    ns.push(Name {
                        rva: e.iat_rva.get(),
                        name: format!("{}!{what}", dll.dll),
                        kind: Kind::Import,
                        size: Some(4),
                        scope: None,
                        provider: "imports".into(),
                    });
                }
            }
            ("imports".into(), "the image".into(), ns, Vec::new())
        }
        NameSource::ReccmpMarkers(dir) => {
            let root = p.path_at(&key, dir)?;
            let (found, files) = crate::reccmp::scan_dir(&root, &d.id).map_err(fail)?;
            let mut notes = vec![format!("{files} source file(s) read, module `{}`", d.id)];
            let mut skipped = 0usize;
            let ns = found
                .iter()
                .filter_map(|f| {
                    let rva = u32::try_from(f.marker.va)
                        .ok()
                        .and_then(|v| v.checked_sub(base));
                    let (Some(rva), Some(name)) = (rva, f.name.clone()) else {
                        skipped = skipped.saturating_add(1);
                        return None;
                    };
                    let kind = match f.marker.kind.as_str() {
                        "GLOBAL" => Kind::Data,
                        "VTABLE" => Kind::Vftable,
                        "STRING" => Kind::String,
                        _ => Kind::Function,
                    };
                    Some(Name {
                        rva,
                        name,
                        kind,
                        size: None,
                        scope: None,
                        provider: "reccmp-markers".into(),
                    })
                })
                .collect();
            if skipped > 0 {
                notes.push(format!(
                    "{skipped} marker(s) with no name below them or below the image base"
                ));
            }
            (
                "reccmp-markers".into(),
                root.display().to_string(),
                ns,
                notes,
            )
        }
        NameSource::Pdb(f) => {
            let path = crate::functions::pdb_path(p, n, &key, f.as_ref())?;
            let bytes = std::fs::read(&path)
                .map_err(|e| fail(anyhow::anyhow!("reading {}: {e}", path.display())))?;
            let l = msvc6_pdb::procs::read(&bytes)
                .map_err(|e| fail(e.context(path.display().to_string())))?;
            let ns = l
                .names
                .iter()
                .map(|x| Name {
                    rva: x.rva,
                    name: x.name.clone(),
                    kind: match x.kind {
                        msvc6_pdb::procs::NameKind::Function => Kind::Function,
                        msvc6_pdb::procs::NameKind::Data => Kind::Data,
                        msvc6_pdb::procs::NameKind::Thunk => Kind::Thunk,
                        msvc6_pdb::procs::NameKind::Label => Kind::Label,
                    },
                    size: x.size,
                    scope: x.scope.clone(),
                    provider: "pdb".into(),
                })
                .collect();
            let count = |r: &str| l.names.iter().filter(|x| x.record == r).count();
            let notes = vec![format!(
                "{} public(s), {} global datum(s), {} static(s), {} procedure name(s), {} label(s)",
                count("public"),
                count("global"),
                count("static"),
                count("procedure") + count("local procedure") + count("thunk"),
                count("label")
            )];
            ("pdb".into(), path.display().to_string(), ns, notes)
        }
        NameSource::ReccmpCsv(f) => {
            let path = p.path_at(&key, f)?;
            let rows = crate::reccmp::read_csv(&path).map_err(fail)?;
            let ns = rows
                .iter()
                .filter_map(|r| {
                    Some(Name {
                        rva: r.va.checked_sub(base)?,
                        name: r.name.clone()?,
                        kind: match r.kind.as_str() {
                            k if crate::reccmp::FUNCTION_TYPES.contains(&k) => Kind::Function,
                            "vtable" => Kind::Vftable,
                            "string" | "widechar" => Kind::String,
                            _ => Kind::Data,
                        },
                        size: r.size,
                        scope: None,
                        provider: "reccmp-csv".into(),
                    })
                })
                .collect();
            (
                "reccmp-csv".into(),
                path.display().to_string(),
                ns,
                Vec::new(),
            )
        }
    })
}

pub fn oracle_of(p: &Project, n: usize, r: &Resolved) -> Result<NameOracle> {
    let img = Image::parse(r.bytes.clone())?;
    let d = p
        .images
        .get(n)
        .ok_or_else(|| anyhow::anyhow!("no image[{n}]"))?;
    let mut o = NameOracle::default();
    for (i, s) in d.names.iter().enumerate() {
        if let Some((raw, v)) = s.input().and_then(|raw| p.absent(raw).map(|v| (raw, v))) {
            o.add(
                s.kind(),
                raw,
                Vec::new(),
                vec![crate::config::absent_note(&v)],
            );
            continue;
        }
        let (kind, src, ns, notes) = read_one(p, n, i, s, &img)?;
        o.add(&kind, &src, ns, notes);
    }
    Ok(o)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_names_tsv_row_is_rva_name_kind_then_size_and_scope() {
        let t = "rva\tname\tkind\tsize\tscope\n0x1000\t_main\tfunction\n0x2000\t_g\tdata\t4\n0x1100\t$E12\tfunction\t\tfoo\n";
        let ns = parse_tsv(Path::new("n.tsv"), t).unwrap();
        assert_eq!(ns.len(), 3);
        assert_eq!(ns[1].size, Some(4));
        assert_eq!(ns[2].scope.as_deref(), Some("foo"));
        let e = parse_tsv(Path::new("n.tsv"), "0x1\ta\tfunc\n").unwrap_err();
        assert!(
            format!("{e:#}").contains("n.tsv:1") && format!("{e:#}").contains("func"),
            "{e:#}"
        );
        assert!(parse_tsv(Path::new("n.tsv"), "1000\ta\tfunction\n").is_err());
        assert!(parse_tsv(Path::new("n.tsv"), "0x1\ta\n").is_err());
        let mut o = NameOracle::default();
        o.add("tsv", "n.tsv", ns, Vec::new());
        assert_eq!(o.at(0x1000).unwrap().name, "_main");
        assert_eq!(o.lookup("_g")[0].rva, 0x2000);
    }

    #[test]
    fn a_name_at_an_ilt_thunk_and_at_its_body_resolves_to_the_body() {
        let ns = parse_tsv(
            Path::new("n.tsv"),
            "rva\tname\tkind\tsize\tscope\n0x1005\t_f\tthunk\n0x2000\t_f\tlabel\n0x100a\t_t\tthunk\n0x100f\t_q\tthunk\n0x3000\t_q\tlabel\t\tobj\n",
        )
        .unwrap();
        let mut o = NameOracle::default();
        o.add("tsv", "n.tsv", ns, Vec::new());
        assert_eq!(o.lookup("_f").len(), 2);
        let f: Vec<u32> = o.bodies("_f").iter().map(|n| n.rva).collect();
        assert_eq!(f, [0x2000]);
        let t: Vec<u32> = o.bodies("_t").iter().map(|n| n.rva).collect();
        assert_eq!(t, [0x100a]);
        let b = o.named_bodies();
        assert!(b.contains("_f") && !b.contains("_t") && !b.contains("_q"));
        let q: Vec<u32> = o.bodies("_q").iter().map(|n| n.rva).collect();
        assert_eq!(q, [0x3000]);
    }
}
