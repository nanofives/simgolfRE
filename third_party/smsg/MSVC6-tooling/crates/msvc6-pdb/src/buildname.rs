use crate::dbi::Dbi;
use crate::hash::hash_v1;
use crate::info::read_info;
use crate::le::{free_slots, latin1, u16_at, u32_at, u32_of, us};
use crate::msf::Msf2;
use crate::paths::{Policy, Score, dir_of};
use crate::strtab::{reachable, run_starts};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::codec::{PDB20_SYMBOLS, sym_record, walk};
use std::collections::BTreeMap;

const S_OBJNAME_ST: u16 = 0x0009;
const S_COMPILE2_ST: u16 = 0x1013;
const COMPILE2_FIXED: usize = 18;
const REFS: [u16; 3] = [0x0400, 0x0401, 0x0403];
const NMT_MAGIC: u32 = 0xeffe_effe;
const TS_HEAD: usize = 16;

pub type RecordMap = BTreeMap<u32, u32>;

fn pointer_fields(kind: u16) -> &'static [usize] {
    match kind {
        0x100a | 0x100b | 0x0206 => &[0, 4, 8],
        0x0207 | 0x0208 => &[0, 4],
        _ => &[],
    }
}

fn foreign_scope(kind: u16) -> bool {
    matches!(
        kind,
        0x0104..=0x0108
            | 0x0204
            | 0x0205
            | 0x0300
            | 0x0301
            | 0x1010
            | 0x1011
            | 0x1015
            | 0x1016
            | 0x101a
            | 0x101b
            | 0x1102..=0x1104
            | 0x110f
            | 0x1110
            | 0x1114
            | 0x1115
            | 0x1118
            | 0x1119
            | 0x112a
            | 0x112b
            | 0x1132
            | 0x1146
            | 0x1147
            | 0x114d
            | 0x1155
            | 0x1156
            | 0x115d
    )
}

fn bytes_of(s: &str) -> Result<Vec<u8>> {
    s.chars()
        .map(|c| u8::try_from(u32::from(c)))
        .collect::<std::result::Result<Vec<u8>, _>>()
        .map_err(|_| anyhow!("`{s}` holds a char no PDB 2.0 name byte encodes"))
}

fn count(score: &mut Score, field: &'static str, old: &str, new: &str) {
    let key = if field.ends_with("cwd") {
        (old.to_string(), new.to_string())
    } else {
        (dir_of(old), dir_of(new))
    };
    *score.build.entry((field, key.0, key.1)).or_insert(0) += 1;
}

fn publish(
    policy: &Policy,
    score: &mut Score,
    field: &'static str,
    path: &str,
) -> Result<Option<String>> {
    let core = path.trim_end_matches('\\');
    let Some((name, product)) = policy.publish_path(core)? else {
        return Ok(None);
    };
    if let Some(p) = product {
        score.claim(&name, &p)?;
    }
    let new = format!("{name}{}", path.get(core.len()..).unwrap_or_default());
    if new == path {
        return Ok(None);
    }
    count(score, field, path, &new);
    Ok(Some(new))
}

const CL_PATH_OPTIONS: [&str; 9] = ["Fo", "Fd", "Fp", "FR", "Fr", "Fe", "Fm", "Fa", "I"];
const LINK_PATH_OPTIONS: [&str; 6] = ["out:", "pdb:", "map:", "implib:", "def:", "libpath:"];

fn path_in(token: &str) -> Option<(usize, usize)> {
    let b = token.as_bytes();
    let unquote = |start: usize| {
        if b.get(start) == Some(&b'"') {
            let s = start + 1;
            let e = token
                .get(s..)
                .and_then(|r| r.find('"'))
                .map_or(b.len(), |e| s + e);
            (s, e)
        } else {
            (start, b.len())
        }
    };
    if let Some(k) = (1..b.len()).find(|&k| {
        b.get(k) == Some(&b':')
            && b.get(k - 1).is_some_and(u8::is_ascii_alphabetic)
            && b.get(k + 1) == Some(&b'\\')
    }) {
        let start = k - 1;
        let quoted = token.get(..start).is_some_and(|p| p.ends_with('"'));
        return Some(if quoted {
            unquote(start - 1)
        } else {
            (start, b.len())
        });
    }
    let (s, e) = match b.first() {
        Some(b'-' | b'/') => {
            let rest = token.get(1..).unwrap_or_default();
            let n = CL_PATH_OPTIONS
                .iter()
                .find(|o| rest.starts_with(**o))
                .or_else(|| {
                    LINK_PATH_OPTIONS.iter().find(|o| {
                        rest.get(..o.len())
                            .is_some_and(|p| p.eq_ignore_ascii_case(o))
                    })
                })?
                .len();
            unquote(1 + n)
        }
        _ => {
            let (s, e) = unquote(0);
            if !token.get(s..e).is_some_and(|p| p.contains('\\')) {
                return None;
            }
            (s, e)
        }
    };
    (s < e).then_some((s, e))
}

fn tokens(cmd: &str) -> Vec<(usize, usize)> {
    let b = cmd.as_bytes();
    let mut out = Vec::new();
    let mut i = 0usize;
    while i < b.len() {
        while b.get(i) == Some(&b' ') {
            i += 1;
        }
        if i >= b.len() {
            break;
        }
        let s = i;
        let mut quoted = false;
        while let Some(c) = b.get(i) {
            if *c == b' ' && !quoted {
                break;
            }
            if *c == b'"' {
                quoted = !quoted;
            }
            i += 1;
        }
        out.push((s, i));
    }
    out
}

#[must_use]
pub fn command_paths(cmd: &str) -> Vec<String> {
    tokens(cmd)
        .into_iter()
        .filter_map(|(s, e)| {
            let t = cmd.get(s..e)?;
            let (a, b) = path_in(t)?;
            t.get(a..b).map(str::to_string)
        })
        .collect()
}

type TokenMap = Vec<(usize, usize, usize, usize)>;

fn rename_command(
    policy: &Policy,
    score: &mut Score,
    field: &'static str,
    cmd: &str,
) -> Result<Option<(String, TokenMap)>> {
    let mut out = String::with_capacity(cmd.len());
    let mut map = Vec::new();
    let mut last = 0usize;
    let mut changed = false;
    for (s, e) in tokens(cmd) {
        out.push_str(cmd.get(last..s).unwrap_or_default());
        let t = cmd.get(s..e).unwrap_or_default();
        let ns = out.len();
        match path_in(t) {
            Some((a, b)) => {
                let p = t.get(a..b).unwrap_or_default();
                let np = publish(policy, score, field, p)
                    .with_context(|| format!("the command line `{cmd}`"))?;
                changed |= np.is_some();
                out.push_str(t.get(..a).unwrap_or_default());
                out.push_str(np.as_deref().unwrap_or(p));
                out.push_str(t.get(b..).unwrap_or_default());
                map.push((s, e, ns, a));
            }
            None => {
                out.push_str(t);
                map.push((s, e, ns, t.len()));
            }
        }
        last = e;
    }
    out.push_str(cmd.get(last..).unwrap_or_default());
    Ok(changed.then_some((out, map)))
}

fn compile2_strings(payload: &[u8]) -> Result<Option<(usize, Vec<String>)>> {
    let n = usize::from(
        *payload
            .get(COMPILE2_FIXED)
            .context("S_COMPILE2 without its version")?,
    );
    let mut at = COMPILE2_FIXED + 1 + n;
    if at >= payload.len() {
        return Ok(None);
    }
    let mut out = Vec::new();
    loop {
        let rest = payload
            .get(at..)
            .context("S_COMPILE2's strings run past it")?;
        let k = rest
            .iter()
            .position(|c| *c == 0)
            .context("S_COMPILE2's strings are not terminated")?;
        if k == 0 {
            break;
        }
        out.push(latin1(rest.get(..k).unwrap_or_default()));
        at += k + 1;
    }
    Ok(Some((COMPILE2_FIXED + 1 + n, out)))
}

fn field_of(key: &str) -> &'static str {
    match key {
        "cwd" => "S_COMPILE2 cwd",
        "cl" => "S_COMPILE2 cl",
        "src" => "S_COMPILE2 src",
        "pdb" => "S_COMPILE2 pdb",
        "cmd" => "S_COMPILE2 cmd",
        _ => "S_COMPILE2 string",
    }
}

fn rename_record(
    policy: &Policy,
    score: &mut Score,
    kind: u16,
    payload: &[u8],
) -> Result<Option<Vec<u8>>> {
    match kind {
        S_OBJNAME_ST => {
            let n = usize::from(*payload.get(4).context("S_OBJNAME without its name")?);
            let name = latin1(
                payload
                    .get(5..5 + n)
                    .context("S_OBJNAME's name runs past it")?,
            );
            let Some(new) = publish(policy, score, "S_OBJNAME", &name)? else {
                return Ok(None);
            };
            let b = bytes_of(&new)?;
            let mut p = payload.get(..4).unwrap_or_default().to_vec();
            p.push(
                u8::try_from(b.len())
                    .with_context(|| format!("`{new}`: an ST name holds 255 bytes"))?,
            );
            p.extend_from_slice(&b);
            Ok(Some(sym_record(kind, &p)?))
        }
        S_COMPILE2_ST => {
            let Some((head, strings)) = compile2_strings(payload)? else {
                return Ok(None);
            };
            let mut out = Vec::with_capacity(strings.len());
            let mut changed = false;
            let mut key = "";
            for (i, s) in strings.iter().enumerate() {
                let new = if i % 2 == 0 {
                    None
                } else {
                    let f = field_of(key);
                    match key {
                        "cwd" | "cl" | "src" | "pdb" => publish(policy, score, f, s)?,
                        _ => rename_command(policy, score, f, s)?.map(|(c, _)| c),
                    }
                };
                if i % 2 == 0 {
                    key = s.as_str();
                }
                changed |= new.is_some();
                out.push(new.unwrap_or_else(|| s.clone()));
            }
            if !changed {
                return Ok(None);
            }
            let mut p = payload.get(..head).unwrap_or_default().to_vec();
            for s in &out {
                p.extend_from_slice(&bytes_of(s)?);
                p.push(0);
            }
            p.push(0);
            Ok(Some(sym_record(kind, &p)?))
        }
        _ => Ok(None),
    }
}

pub fn rename_symbols(
    region: &[u8],
    policy: &Policy,
    score: &mut Score,
) -> Result<Option<(Vec<u8>, RecordMap)>> {
    let recs = walk(region, PDB20_SYMBOLS)?;
    let mut news: Vec<Option<Vec<u8>>> = Vec::with_capacity(recs.len());
    for r in &recs {
        news.push(
            rename_record(policy, score, r.kind, r.payload)
                .with_context(|| format!("the record at {:#x}", 4 + r.at))?,
        );
    }
    if news.iter().all(Option::is_none) {
        return Ok(None);
    }
    if let Some(r) = recs.iter().find(|r| foreign_scope(r.kind)) {
        bail!(
            "the record at {:#x} opens a scope of kind {:#06x}, which VC6 does not write and whose pointers this \
             rename does not re-point -- REFUSED, not re-emitted with them left behind",
            4 + r.at,
            r.kind
        );
    }
    let mut out = Vec::with_capacity(region.len() + 64);
    let mut map = RecordMap::new();
    let mut end = 0usize;
    for (r, new) in recs.iter().zip(&news) {
        let stop = r.end.next_multiple_of(4).min(region.len());
        map.insert(
            u32_of(4 + r.at, "record offset")?,
            u32_of(4 + out.len(), "record offset")?,
        );
        match new {
            Some(b) => out.extend_from_slice(b),
            None => {
                out.extend_from_slice(region.get(r.at..stop).context("a record past its region")?)
            }
        }
        end = stop;
    }
    out.extend_from_slice(region.get(end..).unwrap_or_default());
    let placed: Vec<(usize, u16)> = walk(&out, PDB20_SYMBOLS)?
        .into_iter()
        .map(|r| (r.at, r.kind))
        .collect();
    for (at, kind) in placed {
        for f in pointer_fields(kind) {
            let p = at + 4 + f;
            let old = u32_at(&out, p)?;
            if old == 0 {
                continue;
            }
            let new = *map.get(&old).ok_or_else(|| {
                anyhow!(
                    "the record of kind {kind:#06x} at {:#x}: its field +{f} points at {old:#x}, which is no \
                     record start",
                    4 + at
                )
            })?;
            crate::le::set32(&mut out, p, new)?;
        }
    }
    Ok(Some((out, map)))
}

pub fn repoint_refs(symrec: &mut [u8], maps: &BTreeMap<usize, RecordMap>) -> Result<usize> {
    let refs: Vec<usize> = walk(symrec, PDB20_SYMBOLS)?
        .into_iter()
        .filter(|r| REFS.contains(&r.kind))
        .map(|r| r.at)
        .collect();
    let mut moved = 0usize;
    for at in refs {
        let imod = usize::from(u16_at(symrec, at + 12)?);
        let Some(map) = maps.get(&imod) else {
            continue;
        };
        let ib = u32_at(symrec, at + 8)?;
        let new = *map.get(&ib).ok_or_else(|| {
            anyhow!("the reference at {at:#x} points at {ib:#x} of module {imod}, which is no record start")
        })?;
        if new != ib {
            moved += 1;
            crate::le::set32(symrec, at + 8, new)?;
        }
    }
    Ok(moved)
}

struct Nmt {
    names: BTreeMap<u32, Vec<u8>>,
    buckets: usize,
}

fn nmt(b: &[u8]) -> Result<Nmt> {
    if u32_at(b, 0)? != NMT_MAGIC || u32_at(b, 4)? != 1 {
        bail!("the DBI's EC names are not a version-1 {NMT_MAGIC:#x} name table");
    }
    let cb = us(u32_at(b, 8)?);
    let nb = us(u32_at(b, 12usize.saturating_add(cb))?);
    if nb.saturating_mul(4).saturating_add(20 + cb) != b.len() {
        bail!(
            "the EC name table is {} bytes and its {cb} name bytes and {nb} buckets make {}",
            b.len(),
            nb.saturating_mul(4).saturating_add(20 + cb)
        );
    }
    let names = crate::strtab::walk(b)?;
    let mut slots = Vec::with_capacity(nb);
    for i in 0..nb {
        slots.push(u32_at(b, 16 + cb + 4 * i)?);
    }
    let runs = run_starts(&slots);
    for (i, id) in slots.iter().enumerate().filter(|(_, id)| **id != 0) {
        let n = names
            .get(id)
            .ok_or_else(|| anyhow!("EC bucket {i} holds {id:#x}, no name's offset"))?;
        let h = us(hash_v1(n)) % nb;
        if !reachable(h, i, runs.get(i).copied().flatten(), nb) {
            bail!(
                "EC bucket {i} holds `{}`, which hashes to {h} with an empty bucket between: not the \
                 hashStringV1 / linear-probing table this rewrite writes",
                latin1(n)
            );
        }
    }
    Ok(Nmt { names, buckets: nb })
}

fn rename_nmt(
    b: &[u8],
    policy: &Policy,
    score: &mut Score,
) -> Result<Option<(Vec<u8>, RecordMap)>> {
    let t = nmt(b)?;
    let mut changed = false;
    let mut buf = vec![0u8];
    let mut at: BTreeMap<Vec<u8>, u32> = BTreeMap::new();
    let mut map = BTreeMap::new();
    map.insert(0u32, 0u32);
    let mut order = Vec::new();
    for (off, n) in &t.names {
        if n.is_empty() {
            map.insert(*off, 0);
            continue;
        }
        let new = match publish(policy, score, "EC name", &latin1(n))? {
            Some(s) => {
                changed = true;
                bytes_of(&s)?
            }
            None => n.clone(),
        };
        let o = match at.get(&new) {
            Some(o) => *o,
            None => {
                let o = u32_of(buf.len(), "EC offset")?;
                buf.extend_from_slice(&new);
                buf.push(0);
                at.insert(new.clone(), o);
                order.push((new, o));
                o
            }
        };
        map.insert(*off, o);
    }
    if !changed {
        return Ok(None);
    }
    let nb = t.buckets;
    let mut slots = vec![0u32; nb];
    let mut free = free_slots(nb);
    for (n, o) in &order {
        let slot = free
            .take_from(us(hash_v1(n)) % nb.max(1))
            .context("the EC name table is full")?;
        if let Some(s) = slots.get_mut(slot) {
            *s = *o;
        }
    }
    let mut out = Vec::with_capacity(20 + buf.len() + 4 * nb);
    out.extend_from_slice(&NMT_MAGIC.to_le_bytes());
    out.extend_from_slice(&1u32.to_le_bytes());
    out.extend_from_slice(&u32_of(buf.len(), "EC bytes")?.to_le_bytes());
    out.extend_from_slice(&buf);
    out.extend_from_slice(&u32_of(nb, "EC buckets")?.to_le_bytes());
    for s in slots {
        out.extend_from_slice(&s.to_le_bytes());
    }
    out.extend_from_slice(&u32_of(order.len(), "EC names")?.to_le_bytes());
    Ok(Some((out, map)))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TypeServer {
    pub at: usize,
    pub sig: u32,
    pub age: u32,
    pub pdb: String,
    pub via: String,
}

pub fn type_servers(map: &[u8]) -> Result<Vec<TypeServer>> {
    let len = map.len();
    let mut out = Vec::new();
    let mut p = 0usize;
    while p < len {
        let head = |o: usize| {
            u32_at(map, p + o).with_context(|| {
                format!(
                    "type-server entry at +{p:#x} has no room for its 16-byte head ({len:#x} bytes)"
                )
            })
        };
        let (sig, age) = (head(8)?, head(12)?);
        let name = |from: usize| -> Result<(String, usize)> {
            let rest = map.get(from..).unwrap_or_default();
            let n = rest.iter().position(|c| *c == 0).with_context(|| {
                format!("type-server entry at +{p:#x}: a name at +{from:#x} runs past the map")
            })?;
            Ok((latin1(rest.get(..n).unwrap_or_default()), from + n + 1))
        };
        let (pdb, q) = name(p + TS_HEAD)?;
        let (via, q) = name(q)?;
        let next = q.div_ceil(4) * 4;
        if next > len {
            bail!(
                "type-server entry at +{p:#x} ({pdb}) ends at +{q:#x}, and its 4-byte padding runs past the {len:#x}-byte map"
            );
        }
        out.push(TypeServer {
            at: p,
            sig,
            age,
            pdb,
            via,
        });
        p = next;
    }
    Ok(out)
}

fn rename_tsm(tsm: &[u8], policy: &Policy, score: &mut Score) -> Result<Option<Vec<u8>>> {
    let servers = type_servers(tsm)?;
    let mut out = Vec::with_capacity(tsm.len() + 64);
    let mut changed = false;
    for s in &servers {
        out.extend_from_slice(
            tsm.get(s.at..s.at + TS_HEAD)
                .context("a type-server entry's head")?,
        );
        for (field, n) in [("type-server PDB", &s.pdb), ("type-server module", &s.via)] {
            let new = publish(policy, score, field, n)?;
            changed |= new.is_some();
            out.extend_from_slice(&bytes_of(new.as_deref().unwrap_or(n))?);
            out.push(0);
        }
        out.resize(out.len().next_multiple_of(4), 0);
    }
    Ok(changed.then_some(out))
}

pub fn rename_dbi(dbi: &[u8], policy: &Policy, score: &mut Score) -> Result<Option<Vec<u8>>> {
    let mut d = Dbi::parse(dbi)?;
    let mut changed = false;
    for m in &mut d.mods {
        for (field, n) in [
            ("DBI module name", &mut m.name),
            ("DBI module library", &mut m.obj),
        ] {
            if let Some(new) = publish(policy, score, field, &latin1(n))? {
                *n = bytes_of(&new)?;
                changed = true;
            }
        }
    }
    if !d.ec.is_empty()
        && let Some((ec, map)) = rename_nmt(&d.ec, policy, score)?
    {
        for m in &mut d.mods {
            for at in [56usize, 60] {
                let ni = u32_at(&m.head, at)?;
                let new = *map.get(&ni).ok_or_else(|| {
                    anyhow!(
                        "module {} names EC offset {ni:#x} at +{at}, which is no name's",
                        latin1(&m.name)
                    )
                })?;
                crate::le::set32(&mut m.head, at, new)?;
            }
        }
        d.ec = ec;
        changed = true;
    }
    if !d.tsm.is_empty()
        && let Some(t) = rename_tsm(&d.tsm, policy, score)?
    {
        d.tsm = t;
        changed = true;
    }
    Ok(if changed { Some(d.bytes()?) } else { None })
}

struct LinkInfo {
    cwd: String,
    cmd: String,
    ich: usize,
}

fn link_info(s: &[u8]) -> Result<LinkInfo> {
    let w = |at: usize| -> Result<usize> { Ok(us(u32_at(s, at)?)) };
    let (cb, ver, off_cwd, off_cmd, ich) = (w(0)?, w(4)?, w(8)?, w(12)?, w(16)?);
    let cstr = |at: usize| -> Result<String> {
        let rest = s.get(at..).context("a /LinkInfo string past the stream")?;
        let n = rest
            .iter()
            .position(|c| *c == 0)
            .context("an unterminated /LinkInfo string")?;
        Ok(latin1(rest.get(..n).unwrap_or_default()))
    };
    if ver != 1 || cb != s.len() || off_cwd != 20 {
        bail!(
            "/LinkInfo is {} bytes, cb {cb}, version {ver}, cwd at {off_cwd}: not the {{cb, 1, 20, ...}} layout VC6's LINK writes",
            s.len()
        );
    }
    let cwd = cstr(off_cwd)?;
    let cmd = cstr(off_cmd)?;
    if off_cmd != off_cwd + cwd.len() + 1 || off_cmd + cmd.len() + 1 != cb {
        bail!(
            "/LinkInfo's command at {off_cmd} does not follow its cwd and end the stream ({cb} bytes)"
        );
    }
    Ok(LinkInfo { cwd, cmd, ich })
}

pub fn link_info_paths(s: &[u8]) -> Result<Vec<(&'static str, String)>> {
    if s.is_empty() {
        return Ok(Vec::new());
    }
    let l = link_info(s)?;
    let mut out = vec![("/LinkInfo cwd", l.cwd)];
    out.extend(
        command_paths(&l.cmd)
            .into_iter()
            .map(|p| ("/LinkInfo command", p)),
    );
    Ok(out)
}

pub fn rename_link_info(s: &[u8], policy: &Policy, score: &mut Score) -> Result<Option<Vec<u8>>> {
    if s.is_empty() {
        return Ok(None);
    }
    let l = link_info(s)?;
    let cwd = publish(policy, score, "/LinkInfo cwd", &l.cwd)?;
    let cmd = rename_command(policy, score, "/LinkInfo command", &l.cmd)?;
    if cwd.is_none() && cmd.is_none() {
        return Ok(None);
    }
    let cwd = cwd.unwrap_or(l.cwd);
    let (new_cmd, ich) = match cmd {
        None => (l.cmd, l.ich),
        Some((c, map)) => {
            let ich = map
                .iter()
                .find(|(s, e, _, _)| l.ich >= *s && l.ich < *e)
                .filter(|(s, _, _, path)| l.ich - s <= *path)
                .map(|(s, _, ns, _)| ns + (l.ich - s))
                .ok_or_else(|| {
                    anyhow!(
                        "/LinkInfo's ichOutfile {} is not at an option or the start of a path in `{}`",
                        l.ich,
                        l.cmd
                    )
                })?;
            (c, ich)
        }
    };
    let (cwd_b, cmd_b) = (bytes_of(&cwd)?, bytes_of(&new_cmd)?);
    let off_cmd = 20 + cwd_b.len() + 1;
    let cb = off_cmd + cmd_b.len() + 1;
    let mut out = Vec::with_capacity(cb);
    for v in [cb, 1, 20, off_cmd, ich] {
        out.extend_from_slice(&u32_of(v, "/LinkInfo field")?.to_le_bytes());
    }
    out.extend_from_slice(&cwd_b);
    out.push(0);
    out.extend_from_slice(&cmd_b);
    out.push(0);
    Ok(Some(out))
}

pub fn symbol_paths(region: &[u8]) -> Result<Vec<(&'static str, String)>> {
    let mut out = Vec::new();
    for r in walk(region, PDB20_SYMBOLS)? {
        match r.kind {
            S_OBJNAME_ST => {
                let n = usize::from(*r.payload.get(4).context("S_OBJNAME without its name")?);
                out.push((
                    "S_OBJNAME",
                    latin1(
                        r.payload
                            .get(5..5 + n)
                            .context("S_OBJNAME's name runs past it")?,
                    ),
                ));
            }
            S_COMPILE2_ST => {
                let Some((_, strings)) = compile2_strings(r.payload)? else {
                    continue;
                };
                for pair in strings.chunks(2) {
                    let (Some(k), Some(v)) = (pair.first(), pair.get(1)) else {
                        continue;
                    };
                    match k.as_str() {
                        "cwd" | "cl" | "src" | "pdb" => out.push((field_of(k), v.clone())),
                        _ => out.extend(command_paths(v).into_iter().map(|p| (field_of(k), p))),
                    }
                }
            }
            _ => {}
        }
    }
    Ok(out)
}

pub fn dbi_paths(dbi: &[u8]) -> Result<Vec<(String, String)>> {
    let d = Dbi::parse(dbi)?;
    let mut out = Vec::new();
    for (i, m) in d.mods.iter().enumerate() {
        out.push((format!("module {i}'s name"), latin1(&m.name)));
        out.push((format!("module {i}'s library"), latin1(&m.obj)));
    }
    if !d.ec.is_empty() {
        for n in nmt(&d.ec)?.names.values().filter(|n| !n.is_empty()) {
            out.push(("an EC name".to_string(), latin1(n)));
        }
    }
    if !d.tsm.is_empty() {
        for s in type_servers(&d.tsm)? {
            out.push(("a type-server PDB".to_string(), s.pdb));
            out.push(("a type-server module".to_string(), s.via));
        }
    }
    Ok(out)
}

pub fn link_info_stream(db: &Msf2) -> Result<Option<usize>> {
    let info = read_info(&db.stream(1)?)?;
    Ok(info.names.get(b"/LinkInfo".as_slice()).map(|s| us(*s)))
}

pub fn lane_text(db: &Msf2, policy: &Policy) -> Result<Vec<(usize, usize, String)>> {
    let path_char = |c: u8| c.is_ascii_alphanumeric() || b"._-$~".contains(&c);
    let mut out = Vec::new();
    let lanes = policy.lanes_for_scan();
    for i in 0..db.streams.len() {
        let s = db.stream(i)?;
        let lower: Vec<u8> = s.iter().map(u8::to_ascii_lowercase).collect();
        for (lane, sub, rooted) in &lanes {
            let needle = lane.to_ascii_lowercase();
            let n = needle.as_bytes();
            if n.is_empty() {
                continue;
            }
            let mut from = 0usize;
            while let Some(k) = lower
                .get(from..)
                .and_then(|h| h.windows(n.len()).position(|w| w == n))
            {
                let at = from + k;
                from = at + 1;
                let next = lower.get(at + n.len()).copied();
                if next.is_some_and(path_char) {
                    continue;
                }
                let hit = if next == Some(b'\\') {
                    let c: Vec<u8> = lower
                        .get(at + n.len() + 1..)
                        .unwrap_or_default()
                        .iter()
                        .copied()
                        .take_while(|c| path_char(*c))
                        .collect();
                    !*rooted || sub.contains(&latin1(&c))
                } else {
                    !*rooted
                };
                if hit {
                    let text: Vec<u8> = s
                        .get(at..)
                        .unwrap_or_default()
                        .iter()
                        .copied()
                        .take_while(|c| c.is_ascii_graphic() && *c != b'"')
                        .take(120)
                        .collect();
                    out.push((i, at, latin1(&text)));
                }
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]
mod tests {
    use super::*;
    use crate::config::{AttestedDir, Lane, PathsSpec};
    use crate::dbi::{ModRec, Offsets};
    use crate::write::st;
    use std::path::PathBuf;

    struct Product(PathBuf);

    impl Product {
        fn new(tag: &str, dirs: &[&str], files: &[&str]) -> Self {
            let p =
                std::env::temp_dir().join(format!("msvc6t-buildname-{tag}-{}", std::process::id()));
            let _ = std::fs::remove_dir_all(&p);
            for d in dirs {
                std::fs::create_dir_all(p.join(d)).unwrap();
            }
            for f in files {
                let at = p.join(f);
                std::fs::create_dir_all(at.parent().unwrap()).unwrap();
                msvc6_base::srcio::own_write(&at, "int x;\n").unwrap();
            }
            Self(p)
        }
    }

    impl Drop for Product {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn lane(at: &str, product: &str) -> Lane {
        Lane {
            at: at.into(),
            product: product.into(),
        }
    }

    fn attested(product: &str, dir: &str, output: bool) -> AttestedDir {
        AttestedDir {
            product: product.into(),
            dir: dir.into(),
            output,
            evidence: "a test".into(),
        }
    }

    fn policy(pr: &Product, lanes: Vec<Lane>, attested: Vec<AttestedDir>) -> Policy {
        let spec = PathsSpec {
            product: String::new(),
            product_win: Some("C:\\Repo\\work".into()),
            configs: vec!["Release".into(), "Debug".into()],
            lanes,
            attested,
            files: Vec::new(),
        };
        Policy::new(&spec, &pr.0, BTreeMap::new()).unwrap()
    }

    fn dev(tag: &str) -> (Product, Policy) {
        let pr = Product::new(
            tag,
            &[
                "dev/dev.dll/include",
                "dev/dev.dll/Debug",
                "dev/png.lib",
                "dev/zlib.lib/zlib",
            ],
            &["dev/dev.dll/src/pump.cpp", "tools/vc6/VC98/Bin/LINK.EXE"],
        );
        let p = policy(
            &pr,
            vec![
                lane("C:\\Dev", "dev/dev.dll"),
                lane("C:\\png.lib", "dev/png.lib"),
            ],
            vec![
                attested("dev/dev.dll/src", "C:\\Dev", false),
                attested("dev/dev.dll", "C:\\Dev", false),
                attested("dev/dev.dll/Debug", "C:\\Dev\\Debug", true),
            ],
        );
        (pr, p)
    }

    const VOICE: &str = "snd/voice.lib/v/v-1.5/c/voice";

    fn snd(tag: &str) -> (Product, Policy) {
        let pr = Product::new(
            tag,
            &[
                "snd/snd.dll/src",
                "snd/snd.dll/include",
                "snd/DXSDK",
                "tools/vc6/VC98/Include",
            ],
            &[
                &format!("{VOICE}/difmag.c"),
                "tools/vc6/VC98/Bin/CL.EXE",
                "tools/vc6sp5/VC98/Lib/LIBCMT.lib",
                "snd/DXSDK/lib/dinput.lib",
            ],
        );
        let p = policy(
            &pr,
            vec![
                lane("C:\\lk\\ide", "snd/snd.dll"),
                lane("C:\\lk\\voice.lib", "snd/voice.lib"),
                lane("C:\\DXSDK", "snd/DXSDK"),
            ],
            vec![
                attested(
                    "snd/voice.lib/v/v-1.5/c/voice",
                    "C:\\voice-15\\c\\voice",
                    false,
                ),
                attested("snd/voice.lib/v/v-1.5/c", "C:\\voice-15\\c", false),
            ],
        );
        (pr, p)
    }

    #[test]
    fn a_command_lines_paths_are_published_and_every_other_byte_kept() {
        let (_pr, p) = dev("cmd");
        let mut s = Score::default();
        let cmd = "-nologo -IC:\\Dev\\include -FpC:\\Dev\\Debug\\dev.pch -FoC:\\Dev\\Debug\\ \
                   -I\"C:\\Program Files\\x y\" -IC:\\Repo\\work\\dev\\zlib.lib\\zlib  -TP";
        let (new, map) = rename_command(&p, &mut s, "S_COMPILE2 cmd", cmd)
            .unwrap()
            .unwrap();
        assert_eq!(
            new,
            "-nologo -Idev\\dev.dll\\include -FpC:\\Dev\\Debug\\dev.pch -FoC:\\Dev\\Debug\\ \
             -I\"C:\\Program Files\\x y\" -Idev\\zlib.lib\\zlib  -TP"
        );
        assert_eq!(map.len(), 7, "one row per token");
        for (os, oe, ns, path) in &map {
            let (old, now) = (&cmd[*os..*oe], &new[*ns..]);
            let k = (*path).min(old.len());
            assert_eq!(old[..k], now[..k], "{old}");
        }
        assert_eq!(s.build.values().sum::<usize>(), 2);
        assert!(
            rename_command(&p, &mut s, "x", "-nologo -TP -IC:\\Other")
                .unwrap()
                .is_none()
        );
        assert_eq!(
            command_paths(cmd),
            [
                "C:\\Dev\\include",
                "C:\\Dev\\Debug\\dev.pch",
                "C:\\Dev\\Debug\\",
                "C:\\Program Files\\x y",
                "C:\\Repo\\work\\dev\\zlib.lib\\zlib"
            ]
        );
    }

    #[test]
    fn link_info_is_republished_with_its_outfile_index() {
        let (_pr, p) = dev("linkinfo");
        let cwd = "C:\\png.lib";
        let cmd = "\"C:\\Repo\\work\\tools\\vc6\\VC98\\Bin\\LINK.EXE\" -re -out:\"C:\\Dev\\Debug\\dev.dll\"";
        let ich = cmd.find("\"C:\\Dev").unwrap();
        let mut b = Vec::new();
        let off_cmd = 20 + cwd.len() + 1;
        for v in [off_cmd + cmd.len() + 1, 1, 20, off_cmd, ich] {
            b.extend_from_slice(&u32::try_from(v).unwrap().to_le_bytes());
        }
        b.extend_from_slice(cwd.as_bytes());
        b.push(0);
        b.extend_from_slice(cmd.as_bytes());
        b.push(0);
        let mut s = Score::default();
        let n = rename_link_info(&b, &p, &mut s).unwrap().unwrap();
        let l = link_info(&n).unwrap();
        assert_eq!(l.cwd, "dev\\png.lib");
        assert_eq!(
            l.cmd,
            "\"tools\\vc6\\VC98\\Bin\\LINK.EXE\" -re -out:\"C:\\Dev\\Debug\\dev.dll\""
        );
        assert!(
            l.cmd[l.ich..].starts_with("\"C:\\Dev\\Debug\\dev.dll\""),
            "{}",
            l.ich
        );
        assert_eq!(
            link_info_paths(&n).unwrap(),
            [
                ("/LinkInfo cwd", "dev\\png.lib".to_string()),
                (
                    "/LinkInfo command",
                    "tools\\vc6\\VC98\\Bin\\LINK.EXE".to_string()
                ),
                ("/LinkInfo command", "C:\\Dev\\Debug\\dev.dll".to_string()),
            ]
        );
        assert!(
            rename_link_info(&n, &p, &mut s).unwrap().is_none(),
            "published once"
        );
        assert!(
            rename_link_info(&[], &p, &mut s).unwrap().is_none(),
            "an empty stream"
        );
        let mut bad = b.clone();
        bad[8] = 24;
        assert!(
            rename_link_info(&bad, &p, &mut s).is_err(),
            "a cwd not at 20"
        );
    }

    fn compile2(strings: &[&str]) -> Vec<u8> {
        let mut p = vec![0u8; COMPILE2_FIXED];
        p.extend_from_slice(&st("Microsoft (R) 32-bit C/C++ Optimizing Compiler").unwrap());
        for s in strings {
            p.extend_from_slice(s.as_bytes());
            p.push(0);
        }
        p.push(0);
        p.extend_from_slice(&[0x77, 0x10]);
        sym_record(S_COMPILE2_ST, &p).unwrap()
    }

    fn objname(name: &str) -> Vec<u8> {
        let mut p = 0u32.to_le_bytes().to_vec();
        p.extend_from_slice(&st(name).unwrap());
        sym_record(S_OBJNAME_ST, &p).unwrap()
    }

    fn scoped(kind: u16, ptrs: &[u32], rest: &[u8], name: &str) -> Vec<u8> {
        let mut p = Vec::new();
        for v in ptrs {
            p.extend_from_slice(&v.to_le_bytes());
        }
        p.extend_from_slice(rest);
        p.extend_from_slice(&st(name).unwrap());
        sym_record(kind, &p).unwrap()
    }

    fn voice_region() -> (Vec<u8>, u32, u32) {
        let lane = "C:\\lk\\voice.lib";
        let cmd = format!(
            "-nologo -IC:\\lk\\voice.lib\\v\\v-1.5\\c -Fo{lane}\\Release\\ \
             -IC:\\Repo\\work\\tools\\vc6\\VC98\\INCLUDE -TC"
        );
        let head = [
            objname(&format!("{lane}\\Release\\difmag.obj")),
            compile2(&[
                "cwd",
                lane,
                "cl",
                "C:\\Repo\\work\\tools\\vc6\\VC98\\Bin\\CL.EXE",
                "cmd",
                &cmd,
                "src",
                &format!("{lane}\\v\\v-1.5\\c\\voice\\difmag.c"),
                "pdb",
                &format!("{lane}\\Release\\vc60.pdb"),
            ]),
        ]
        .concat();
        let proc_at = u32::try_from(4 + head.len()).unwrap();
        let rest = [
            vec![0u8; 4 * 4],
            0x10u32.to_le_bytes().to_vec(),
            vec![1, 0, 0],
        ]
        .concat();
        let proc_len = scoped(0x100b, &[0, 0, 0], &rest, "_difmag_").len();
        let block_at = proc_at + u32::try_from(proc_len).unwrap();
        let block_len = scoped(0x0207, &[0, 0], &[0u8; 10], "").len();
        let block_end = block_at + u32::try_from(block_len).unwrap();
        let proc_end = block_end + 4;
        let region = [
            head,
            scoped(0x100b, &[0, proc_end, 0], &rest, "_difmag_"),
            scoped(0x0207, &[proc_at, block_end], &[0u8; 10], ""),
            sym_record(0x0006, &[]).unwrap(),
            sym_record(0x0006, &[]).unwrap(),
        ]
        .concat();
        (region, proc_at, block_at)
    }

    #[test]
    fn a_symbol_region_is_reemitted_and_every_pointer_follows() {
        let (_pr, p) = snd("syms");
        let (region, proc_at, block_at) = voice_region();
        let mut s = Score::default();
        let (new, map) = rename_symbols(&region, &p, &mut s).unwrap().unwrap();
        assert_ne!(new.len(), region.len());
        let paths = symbol_paths(&new).unwrap();
        let text: Vec<&str> = paths.iter().map(|(_, p)| p.as_str()).collect();
        assert_eq!(
            text,
            [
                "snd\\voice.lib\\Release\\difmag.obj",
                "snd\\voice.lib",
                "tools\\vc6\\VC98\\Bin\\CL.EXE",
                "C:\\voice-15\\c",
                "snd\\voice.lib\\Release\\",
                "tools\\vc6\\VC98\\Include",
                "C:\\voice-15\\c\\voice\\difmag.c",
                "snd\\voice.lib\\Release\\vc60.pdb",
            ]
        );
        for (_, path) in &paths {
            assert!(
                p.judge_path(path, false).is_ok(),
                "{path}: {:?}",
                p.judge_path(path, false)
            );
        }
        let recs = walk(&new, PDB20_SYMBOLS).unwrap();
        let kind_at = |off: u32| {
            recs.iter()
                .find(|r| u32::try_from(4 + r.at).unwrap() == off)
                .map(|r| r.kind)
        };
        let (np, nb) = (map[&proc_at], map[&block_at]);
        assert_eq!(kind_at(np), Some(0x100b));
        assert_eq!(kind_at(nb), Some(0x0207));
        let at = |off: u32| usize::try_from(off).unwrap() - 4;
        let pend = u32_at(&new, at(np) + 8).unwrap();
        assert_eq!(
            (u32_at(&new, at(np) + 4).unwrap(), kind_at(pend)),
            (0, Some(0x0006))
        );
        assert_eq!(u32_at(&new, at(nb) + 4).unwrap(), np, "the block's pParent");
        assert_eq!(kind_at(u32_at(&new, at(nb) + 8).unwrap()), Some(0x0006));
        assert_eq!(
            pend,
            u32_at(&new, at(nb) + 8).unwrap() + 4,
            "the procedure's S_END follows the block's"
        );
        assert!(
            rename_symbols(&new, &p, &mut s).unwrap().is_none(),
            "published once"
        );
        let refsym = |imod: u16, ib: u32| {
            let mut r = 14u16.to_le_bytes().to_vec();
            r.extend_from_slice(&0x0400u16.to_le_bytes());
            r.extend_from_slice(&0u32.to_le_bytes());
            r.extend_from_slice(&ib.to_le_bytes());
            r.extend_from_slice(&imod.to_le_bytes());
            r.extend_from_slice(&0u16.to_le_bytes());
            r.extend_from_slice(&st("_difmag_").unwrap());
            r.resize(r.len().next_multiple_of(4), 0);
            r
        };
        let mut symrec = [refsym(1, proc_at), refsym(2, proc_at)].concat();
        let maps: BTreeMap<usize, RecordMap> = [(1usize, map)].into_iter().collect();
        assert_eq!(repoint_refs(&mut symrec, &maps).unwrap(), 1);
        let half = symrec.len() / 2;
        assert_eq!(
            u32_at(&symrec, 8).unwrap(),
            np,
            "module 1's reference follows its procedure"
        );
        assert_eq!(
            u32_at(&symrec, half + 8).unwrap(),
            proc_at,
            "module 2's is not touched"
        );
        let mut stray = refsym(1, proc_at + 4);
        assert!(
            repoint_refs(&mut stray, &maps).is_err(),
            "a reference onto no record"
        );
    }

    fn nmt_of(names: &[&str], nb: usize) -> (Vec<u8>, Vec<u32>) {
        let mut buf = vec![0u8];
        let mut offs = Vec::new();
        let mut slots = vec![0u32; nb];
        for n in names {
            let o = u32::try_from(buf.len()).unwrap();
            buf.extend_from_slice(n.as_bytes());
            buf.push(0);
            offs.push(o);
            let h = us(hash_v1(n.as_bytes())) % nb;
            let j = (0..nb)
                .map(|k| (h + k) % nb)
                .find(|j| slots[*j] == 0)
                .unwrap();
            slots[j] = o;
        }
        let mut b = NMT_MAGIC.to_le_bytes().to_vec();
        b.extend_from_slice(&1u32.to_le_bytes());
        b.extend_from_slice(&u32::try_from(buf.len()).unwrap().to_le_bytes());
        b.extend_from_slice(&buf);
        b.extend_from_slice(&u32::try_from(nb).unwrap().to_le_bytes());
        for s in slots {
            b.extend_from_slice(&s.to_le_bytes());
        }
        b.extend_from_slice(&u32::try_from(names.len()).unwrap().to_le_bytes());
        (b, offs)
    }

    #[test]
    fn the_dbis_names_and_ec_table_are_republished_and_each_module_follows() {
        let (_pr, p) = snd("dbi");
        let a = "C:\\lk\\voice.lib\\v\\v-1.5\\c\\voice\\difmag.c";
        let b = "E:\\8337\\vc98\\crtbld\\memcpy.c";
        let (ec, offs) = nmt_of(&[b, a], 5);
        let head = |ni: u32| {
            let mut h = vec![0u8; 64];
            h[56..60].copy_from_slice(&ni.to_le_bytes());
            h
        };
        let module = |ni: u32, name: &str, obj: &str| ModRec {
            at: 0,
            head: head(ni),
            name: name.as_bytes().to_vec(),
            obj: obj.as_bytes().to_vec(),
        };
        let mut header = vec![0u8; 64];
        header[..4].copy_from_slice(&u32::MAX.to_le_bytes());
        header[4..8].copy_from_slice(&19_970_606u32.to_le_bytes());
        let d = Dbi {
            header,
            mods: vec![
                module(
                    offs[1],
                    ".\\Release\\difmag.obj",
                    "..\\voice.lib\\Release\\voice.lib",
                ),
                module(
                    offs[0],
                    "build\\intel\\mt_obj\\memcpy.obj",
                    "C:\\Repo\\work\\tools\\vc6sp5\\VC98\\Lib\\LIBCMT.lib",
                ),
                module(0, "dinput.dll", "C:\\DXSDK\\lib\\dinput.lib"),
            ],
            sc: Vec::new(),
            segmap: Vec::new(),
            files: Vec::new(),
            tsm: Vec::new(),
            ec,
            dbg: vec![0xffff; 2],
            offsets: Offsets::default(),
        };
        let mut s = Score::default();
        let new = rename_dbi(&d.bytes().unwrap(), &p, &mut s)
            .unwrap()
            .unwrap();
        let n = Dbi::parse(&new).unwrap();
        let objs: Vec<String> = n.mods.iter().map(|m| latin1(&m.obj)).collect();
        assert_eq!(
            objs,
            [
                "..\\voice.lib\\Release\\voice.lib",
                "tools\\vc6sp5\\VC98\\Lib\\LIBCMT.lib",
                "snd\\DXSDK\\lib\\dinput.lib"
            ]
        );
        let t = nmt(&n.ec).unwrap();
        let name_of = |m: usize| latin1(&t.names[&u32_at(&n.mods[m].head, 56).unwrap()]);
        assert_eq!(name_of(0), "C:\\voice-15\\c\\voice\\difmag.c");
        assert_eq!(name_of(1), b);
        assert_eq!(
            u32_at(&n.mods[2].head, 56).unwrap(),
            0,
            "no name stays no name"
        );
        assert_eq!(n.dbg, d.dbg, "the substreams after the EC table");
        assert!(
            rename_dbi(&new, &p, &mut s).unwrap().is_none(),
            "published once"
        );
        let (one, _) = nmt_of(&[a], 5);
        let cb = us(u32_at(&one, 8).unwrap());
        let slot = |i: usize| 16 + cb + 4 * i;
        let h = us(hash_v1(a.as_bytes())) % 5;
        let mut bad = one.clone();
        let id = bad[slot(h)..slot(h) + 4].to_vec();
        bad[slot(h)..slot(h) + 4].fill(0);
        bad[slot((h + 2) % 5)..slot((h + 2) % 5) + 4].copy_from_slice(&id);
        assert!(nmt(&one).is_ok());
        assert!(
            nmt(&bad).is_err(),
            "a table whose buckets are not VC6's hash is refused"
        );
    }

    #[test]
    fn a_lane_directory_anywhere_in_a_stream_is_found() {
        let (_pr, p) = dev("scan");
        let spec = crate::write::Spec {
            modules: vec![crate::write::Module {
                name: "a".into(),
                obj: "a".into(),
                procs: Vec::new(),
            }],
            ..crate::write::Spec::default()
        };
        let mut db = Msf2::parse(crate::write::build(&spec).unwrap().bytes).unwrap();
        let text = b"C:\\Dev\0C:\\Dev\\pump.cpp\0C:\\Dev\\Debug\\x.obj\0C:\\DevX\\y\0\
                     c:\\dev\\SRC\\a.cpp\0-IC:\\Dev\\include -I\0C:\\png.lib\0x\
                     C:\\Repo\\work\\tools\0";
        let i = db.streams.len() - 1;
        db.set_stream(i, text).unwrap();
        let hits: Vec<String> = lane_text(&db, &p)
            .unwrap()
            .into_iter()
            .map(|(_, _, t)| t)
            .collect();
        assert_eq!(
            hits,
            [
                "c:\\dev\\SRC\\a.cpp",
                "C:\\Dev\\include",
                "C:\\png.lib",
                "C:\\Repo\\work\\tools"
            ]
        );
    }

    #[test]
    fn a_renamed_region_holding_a_foreign_scope_is_refused() {
        let (_pr, p) = snd("foreign");
        let (region, proc_at, _) = voice_region();
        let at = usize::try_from(proc_at).unwrap() - 4;
        for kind in [0x0205u16, 0x1011] {
            let mut r = region.clone();
            r[at + 2..at + 4].copy_from_slice(&kind.to_le_bytes());
            let e = rename_symbols(&r, &p, &mut Score::default()).unwrap_err();
            assert!(format!("{e:#}").contains("REFUSED"), "{kind:#x}: {e:#}");
        }
        let (published, map) = rename_symbols(&region, &p, &mut Score::default())
            .unwrap()
            .unwrap();
        let mut r = published;
        let at = usize::try_from(map[&proc_at]).unwrap() - 4;
        r[at + 2..at + 4].copy_from_slice(&0x0205u16.to_le_bytes());
        assert!(
            rename_symbols(&r, &p, &mut Score::default())
                .unwrap()
                .is_none()
        );
    }
}
