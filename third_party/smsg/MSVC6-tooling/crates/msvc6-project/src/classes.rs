use crate::config::Project;
use crate::spans::{index, join};
use anyhow::{Result, bail};
use serde::Deserialize;
use std::collections::BTreeSet;

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ClassesSpec {
    #[serde(default)]
    pub sources: Vec<String>,
    #[serde(default)]
    pub headers: Vec<String>,
    #[serde(default)]
    pub tables: Vec<TableControl>,
    #[serde(default)]
    pub counts: Vec<CountControl>,
    #[serde(default)]
    pub placeholders: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TableControl {
    pub rva: u32,
    pub slots: usize,
    pub what: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CountControl {
    pub class: String,
    pub slots: usize,
    pub what: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EhSpec {
    pub textx_ceiling: Option<u32>,
    pub ceiling_evidence: Option<String>,
}

impl ClassesSpec {
    pub fn check(&self) -> Result<()> {
        for (i, p) in self.placeholders.iter().enumerate() {
            if p.trim().is_empty() {
                bail!("placeholders[{i}]: an empty pattern");
            }
        }
        for (i, s) in self.sources.iter().chain(&self.headers).enumerate() {
            if s.trim().is_empty() {
                bail!("sources and headers: entry {i} is an empty path");
            }
        }
        let mut seen = BTreeSet::new();
        for (i, t) in self.tables.iter().enumerate() {
            if t.slots == 0 {
                bail!("tables[{i}]: a table of 0 slots controls nothing");
            }
            if t.rva % 4 != 0 {
                bail!("tables[{i}]: rva {:#x} is not a dword address", t.rva);
            }
            if !seen.insert(t.rva) {
                bail!("tables[{i}]: rva {:#x} is listed twice", t.rva);
            }
        }
        let mut names = BTreeSet::new();
        for (i, c) in self.counts.iter().enumerate() {
            if c.class.trim().is_empty() || c.slots == 0 {
                bail!("counts[{i}]: a count needs a class and a slot count above 0");
            }
            if !names.insert(c.class.as_str()) {
                bail!("counts[{i}]: class `{}` is listed twice", c.class);
            }
        }
        Ok(())
    }
}

impl EhSpec {
    pub fn check(&self) -> Result<()> {
        if self.ceiling_evidence.is_some() && self.textx_ceiling.is_none() {
            bail!("ceiling_evidence names the evidence of no textx_ceiling");
        }
        Ok(())
    }
}

#[must_use]
pub fn check(p: &Project, n: usize) -> Vec<(bool, String, String)> {
    let Some(spec) = p.images.get(n).and_then(|d| d.classes.as_ref()) else {
        return Vec::new();
    };
    let key = join(&p.image_key(n), "classes");
    let mut out = Vec::new();
    for (list, dirs) in [("sources", &spec.sources), ("headers", &spec.headers)] {
        for (i, s) in dirs.iter().enumerate() {
            let k = index(&join(&key, list), i);
            let (ok, detail) = match p.path_at(&k, s) {
                Ok(path) if path.is_dir() => (true, path.display().to_string()),
                Ok(path) => (
                    false,
                    format!("{}: {} is not a directory", p.at(&k), path.display()),
                ),
                Err(e) => (false, format!("{e:#}")),
            };
            out.push((ok, format!("classes {list}"), detail));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn spec(text: &str) -> Result<ClassesSpec> {
        Ok(toml::from_str(text)?)
    }

    #[test]
    fn a_control_table_is_checked_for_what_parses_but_is_wrong() {
        let ok = spec(
            "sources = [\"src\"]\ntables = [{ rva = 0x100, slots = 9, what = \"a class\" }]\ncounts = [{ class = \"A\", slots = 9, what = \"its table\" }]\n",
        )
        .unwrap();
        assert!(ok.check().is_ok());
        assert!(spec("tables = [{ rva = 0x100, slots = 9 }]\n").is_err());
        assert!(spec("table = []\n").is_err());
        let twice = spec(
            "tables = [{ rva = 0x100, slots = 9, what = \"a\" }, { rva = 0x100, slots = 2, what = \"b\" }]\n",
        )
        .unwrap();
        assert!(format!("{:#}", twice.check().unwrap_err()).contains("listed twice"));
        let odd = spec("tables = [{ rva = 0x102, slots = 9, what = \"a\" }]\n").unwrap();
        assert!(odd.check().is_err());
        let empty = spec("counts = [{ class = \"\", slots = 9, what = \"a\" }]\n").unwrap();
        assert!(empty.check().is_err());
        let eh: EhSpec = toml::from_str("ceiling_evidence = \"x\"\n").unwrap();
        assert!(eh.check().is_err());
    }
}
