use crate::config::{DroppedSource, Project};
use crate::spans::{index, join};
use anyhow::{Result, anyhow, bail};
use std::collections::BTreeMap;
use std::path::Path;

pub const HEADER: &str = "name\twhy";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dropped {
    pub name: String,
    pub why: String,
}

pub fn parse_tsv(path: &Path, text: &str) -> Result<Vec<Dropped>> {
    let mut out = Vec::new();
    let mut seen: BTreeMap<String, usize> = BTreeMap::new();
    for (i, raw) in text.lines().enumerate() {
        let n = i.saturating_add(1);
        let l = raw.trim_end_matches('\r');
        if l.trim().is_empty() || (n == 1 && l == HEADER) {
            continue;
        }
        let f: Vec<&str> = l.split('\t').collect();
        let (Some(name), Some(why), None) = (f.first(), f.get(1), f.get(2)) else {
            bail!(
                "{}:{n}: {} column(s); a row is `name why`, tab-separated",
                path.display(),
                f.len()
            );
        };
        if name.is_empty() || name.contains(char::is_whitespace) {
            bail!(
                "{}:{n}: name `{name}` is empty or holds white space",
                path.display()
            );
        }
        if why.trim().is_empty() {
            bail!(
                "{}:{n}: `{name}` gives no reason the image holds no body for it",
                path.display()
            );
        }
        if let Some(first) = seen.insert((*name).to_string(), n) {
            bail!(
                "{}:{n}: `{name}` is already listed at line {first}",
                path.display()
            );
        }
        out.push(Dropped {
            name: (*name).to_string(),
            why: (*why).to_string(),
        });
    }
    Ok(out)
}

pub fn read_one(
    p: &Project,
    n: usize,
    i: usize,
    src: &DroppedSource,
) -> Result<(String, String, Vec<Dropped>)> {
    let key = index(&join(&p.image_key(n), "dropped"), i);
    match src {
        DroppedSource::Tsv(f) => {
            let path = p.path_at(&key, f)?;
            let text = msvc6_base::srcio::own_read(&path)
                .map_err(|e| p.error(&key, format!("reading {}: {e}", path.display())))?;
            let rows = parse_tsv(&path, &text).map_err(|e| p.error(&key, format!("{e:#}")))?;
            Ok(("tsv".into(), path.display().to_string(), rows))
        }
    }
}

pub fn dropped_of(p: &Project, n: usize) -> Result<BTreeMap<String, String>> {
    let d = p.images.get(n).ok_or_else(|| anyhow!("no image[{n}]"))?;
    let mut out = BTreeMap::new();
    for (i, s) in d.dropped.iter().enumerate() {
        if s.input().and_then(|raw| p.absent(raw)).is_some() {
            continue;
        }
        let (_, _, rows) = read_one(p, n, i, s)?;
        for r in rows {
            out.entry(r.name).or_insert(r.why);
        }
    }
    Ok(out)
}
