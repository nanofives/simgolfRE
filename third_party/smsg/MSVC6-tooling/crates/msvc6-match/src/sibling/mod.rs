pub mod code;
pub mod conv;
pub mod diff;
pub mod discover;
pub mod feat;
pub mod pair;
pub mod report;

use anyhow::{Result, bail};
use std::collections::BTreeMap;
use std::path::PathBuf;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Oracle {
    pub label: String,
    pub path: PathBuf,
    pub seeds: Vec<(u64, u64)>,
    pub branch: String,
}

fn hex(x: &str) -> Option<u64> {
    let t = x.trim();
    let t = t
        .strip_prefix("0x")
        .or_else(|| t.strip_prefix("0X"))
        .unwrap_or(t);
    u64::from_str_radix(t, 16).ok()
}

#[must_use]
pub fn home_path(p: &str) -> PathBuf {
    match p.strip_prefix("~/") {
        Some(rest) => std::env::var_os("HOME")
            .map_or_else(|| PathBuf::from(p), |h| PathBuf::from(h).join(rest)),
        None => PathBuf::from(p),
    }
}

pub fn parse_seed(s: &str) -> Result<(u64, u64)> {
    match s
        .split_once('=')
        .and_then(|(a, b)| Some((hex(a)?, hex(b)?)))
    {
        Some(p) => Ok(p),
        None => bail!("seed `{s}` is not OURVA=SIBVA (hexadecimal)"),
    }
}

pub fn parse_oracles(text: &str) -> Result<Vec<Oracle>> {
    let mut out = Vec::new();
    for (n, l) in text.lines().enumerate() {
        if l.trim().is_empty() || l.starts_with('#') {
            continue;
        }
        let line = n.saturating_add(1);
        let mut c = l.split('\t');
        let (Some(label), Some(path)) = (c.next(), c.next()) else {
            bail!("line {line}: `{l}` is not label<TAB>path");
        };
        let mut seeds = Vec::new();
        for s in c
            .next()
            .unwrap_or("")
            .split(',')
            .filter(|s| !s.trim().is_empty())
        {
            seeds.push(parse_seed(s).map_err(|e| anyhow::anyhow!("line {line}: {e}"))?);
        }
        let branch = c.next().unwrap_or("").trim().to_string();
        if let Some(extra) = c.next() {
            bail!("line {line}: `{extra}` after the branch column");
        }
        out.push(Oracle {
            label: label.trim().to_string(),
            path: home_path(path.trim()),
            seeds,
            branch,
        });
    }
    Ok(out)
}

pub fn parse_functions(text: &str, base: u32) -> BTreeMap<u32, (String, String)> {
    let mut out = BTreeMap::new();
    for l in text.lines().filter(|l| !l.starts_with('#')) {
        let mut c = l.split('\t');
        let (Some(a), Some(f), Some(n)) = (c.next(), c.next(), c.next()) else {
            continue;
        };
        let Some(v) = hex(a).and_then(|v| u32::try_from(v).ok()) else {
            continue;
        };
        let rva = if v >= base { v - base } else { v };
        out.insert(rva, (f.to_string(), n.to_string()));
    }
    out
}
