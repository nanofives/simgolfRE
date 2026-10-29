use crate::comdat::{self, Comdat};
use crate::env::Env;
use crate::par;
use anyhow::{Context as _, Result, bail};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Member {
    pub name: String,
    pub size: usize,
    pub rva: Option<u32>,
    pub via_thunk: Option<u32>,
    pub attested: bool,
    pub external: bool,
}

#[must_use]
pub fn scope_stem(obj: &Path) -> String {
    obj.file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or_default()
        .to_string()
}

pub fn members_of(env: &Env, obj: &Path, d: &[u8], comdats: &[Comdat]) -> Result<Vec<Member>> {
    let stem = scope_stem(obj);
    members_scoped(env, &[stem.as_str()], d, comdats).with_context(|| obj.display().to_string())
}

pub fn members_scoped(
    env: &Env,
    scopes: &[&str],
    d: &[u8],
    comdats: &[Comdat],
) -> Result<Vec<Member>> {
    if comdats.is_empty() {
        if comdat::has_code(d)? {
            bail!(
                "the object has code and no `.text` section: without /Gy each function is not its own section"
            );
        }
        return Ok(Vec::new());
    }
    let text = env.text();
    Ok(comdats
        .iter()
        .map(|c| {
            let name = c.name().to_string();
            let r = env.names.resolve(&name, scopes, text, env.text_rva);
            Member {
                size: c.bytes.len(),
                rva: r.map(|r| r.rva),
                via_thunk: r.and_then(|r| r.via_thunk),
                attested: r.is_some_and(|r| r.exported),
                external: c.external,
                name,
            }
        })
        .collect())
}

pub fn derive(env: &Env, obj: &Path) -> Result<(Vec<Comdat>, Vec<Member>)> {
    let d = comdat::read(obj)?;
    let comdats = comdat::text_of(&d)?;
    let members = members_of(env, obj, &d, &comdats)?;
    Ok((comdats, members))
}

pub type Seats = BTreeMap<u32, (PathBuf, Comdat)>;

#[derive(Debug, Clone, Default)]
pub struct SeatMap {
    pub seats: Seats,
    pub refused: Vec<(PathBuf, String)>,
}

fn supersedes(new: &Comdat, seated: &Comdat) -> bool {
    let (Some(n), Some(s)) = (new.symbol.as_deref(), seated.symbol.as_deref()) else {
        return false;
    };
    n.strip_prefix("??_E")
        .is_some_and(|t| s.strip_prefix("??_G") == Some(t))
}

type Seated = Result<Vec<(u32, PathBuf, Comdat)>, String>;

fn seats_of(env: &Env, o: &Path) -> Seated {
    let (comdats, members) = derive(env, o).map_err(|e| format!("{e:#}"))?;
    if members.len() != comdats.len() {
        return Err(format!(
            "{} member(s) named for {} COMDAT(s)",
            members.len(),
            comdats.len()
        ));
    }
    Ok(members
        .iter()
        .zip(comdats)
        .filter_map(|(m, c)| Some((m.rva?, o.to_path_buf(), c)))
        .collect())
}

#[must_use]
pub fn seat_map(env: &Env, objs: &[PathBuf], jobs: usize) -> SeatMap {
    let per: Vec<Seated> = par::map(objs, jobs, |o| seats_of(env, o));
    let mut out = SeatMap::default();
    for (o, r) in objs.iter().zip(per) {
        let seated = match r {
            Ok(s) => s,
            Err(e) => {
                out.refused.push((o.clone(), e));
                continue;
            }
        };
        for (rva, o, c) in seated {
            match out.seats.get(&rva) {
                Some((_, seated)) if !supersedes(&c, seated) => {}
                _ => {
                    out.seats.insert(rva, (o, c));
                }
            }
        }
    }
    out
}

pub fn find(
    env: &Env,
    objs: &[PathBuf],
    rva: u32,
    symbol: Option<&str>,
) -> Result<(PathBuf, Comdat)> {
    for o in objs {
        let d = comdat::read(o)?;
        let comdats = comdat::text_of(&d)?;
        if let Some(s) = symbol {
            if let Some(c) = comdats.iter().find(|c| c.symbol.as_deref() == Some(s)) {
                return Ok((o.clone(), c.clone()));
            }
            continue;
        }
        let Ok(ms) = members_of(env, o, &d, &comdats) else {
            continue;
        };
        if let Some(c) = ms
            .iter()
            .position(|m| m.rva == Some(rva))
            .and_then(|i| comdats.get(i))
        {
            return Ok((o.clone(), c.clone()));
        }
    }
    bail!(
        "no COMDAT among {} object(s) seats at {rva:#x}{}",
        objs.len(),
        symbol.map_or(String::new(), |s| format!(" or is named `{s}`"))
    )
}
