use crate::layout::{Toolchain, to_win};
use crate::runner::Session;
use anyhow::{Context as _, Result, anyhow, bail};
use std::collections::{BTreeMap, BTreeSet};
use std::ops::Range;
use std::path::{Path, PathBuf};

fn le16(b: &[u8], at: usize) -> Option<u16> {
    Some(u16::from_le_bytes([*b.get(at)?, *b.get(at + 1)?]))
}

fn le32(b: &[u8], at: usize) -> Option<usize> {
    let w: [u8; 4] = b.get(at..at + 4)?.try_into().ok()?;
    usize::try_from(u32::from_le_bytes(w)).ok()
}

fn be32(b: &[u8], at: usize) -> Option<usize> {
    let w: [u8; 4] = b.get(at..at + 4)?.try_into().ok()?;
    usize::try_from(u32::from_be_bytes(w)).ok()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Member {
    pub name: String,
    pub header: usize,
    pub body: Range<usize>,
}

pub fn members(data: &[u8]) -> Vec<Member> {
    let mut out = Vec::new();
    if data.get(..8) != Some(b"!<arch>\n") {
        return out;
    }
    let mut pos = 8usize;
    while let Some(hdr) = data.get(pos..pos + 60) {
        let size: usize = msvc6_base::srcio::decode(hdr.get(48..58).unwrap_or_default())
            .trim()
            .parse()
            .unwrap_or(0);
        let name = msvc6_base::srcio::decode(hdr.get(..16).unwrap_or_default())
            .trim_end()
            .to_string();
        let start = pos + 60;
        let end = (start + size).min(data.len());
        out.push(Member {
            name,
            header: pos,
            body: start..end,
        });
        pos = start + size + (size & 1);
    }
    out
}

pub fn short_import(body: &[u8]) -> Option<(String, u16, u16)> {
    if le16(body, 0)? != 0 || le16(body, 2)? != 0xFFFF {
        return None;
    }
    let hint = le16(body, 16)?;
    let nametype = (le16(body, 18)? >> 2) & 0x7;
    let rest = body.get(20..)?;
    let end = rest.iter().position(|&b| b == 0)?;
    Some((msvc6_base::srcio::decode(rest.get(..end)?), hint, nametype))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CoffSym {
    pub rec: usize,
    pub name: Vec<u8>,
    pub str_off: Option<usize>,
    pub section: i16,
    pub class: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Coff {
    pub symptr: usize,
    pub nsyms: usize,
    pub strtab: usize,
    pub strsize: usize,
    pub sections: Vec<(String, usize, usize)>,
    pub syms: Vec<CoffSym>,
}

pub fn coff(obj: &[u8]) -> Option<Coff> {
    if le16(obj, 0)? != 0x14c {
        return None;
    }
    let nsec = usize::from(le16(obj, 2)?);
    let symptr = le32(obj, 8)?;
    let nsyms = le32(obj, 12)?;
    let optsize = usize::from(le16(obj, 16)?);
    let strtab = symptr + 18 * nsyms;
    let strsize = le32(obj, strtab).unwrap_or(4);
    let strings = obj.get(strtab..strtab + strsize)?;
    let name_at = |raw: &[u8]| -> Option<(Vec<u8>, Option<usize>)> {
        if raw.get(..4) == Some(&[0, 0, 0, 0]) {
            let off = le32(raw, 4)?;
            let s = strings.get(off..)?;
            let end = s.iter().position(|&b| b == 0)?;
            Some((s.get(..end)?.to_vec(), Some(off)))
        } else {
            let r = raw.get(..8)?;
            let end = r.iter().position(|&b| b == 0).unwrap_or(8);
            Some((r.get(..end)?.to_vec(), None))
        }
    };
    let mut sections = Vec::new();
    for i in 0..nsec {
        let s = 20 + optsize + 40 * i;
        let raw = obj.get(s..s + 8)?;
        let end = raw.iter().position(|&b| b == 0).unwrap_or(8);
        let mut name = msvc6_base::srcio::decode(raw.get(..end)?);
        if let Some(off) = name.strip_prefix('/').and_then(|n| n.parse::<usize>().ok()) {
            let st = strings.get(off..)?;
            let e = st.iter().position(|&b| b == 0)?;
            name = msvc6_base::srcio::decode(st.get(..e)?);
        }
        sections.push((name, le32(obj, s + 20)?, le32(obj, s + 16)?));
    }
    let mut syms = Vec::new();
    let mut i = 0usize;
    while i < nsyms {
        let e = symptr + 18 * i;
        let rec = obj.get(e..e + 18)?;
        let (name, str_off) = name_at(rec)?;
        syms.push(CoffSym {
            rec: e,
            name,
            str_off,
            section: i16::from_le_bytes([*rec.get(12)?, *rec.get(13)?]),
            class: *rec.get(16)?,
        });
        i += 1 + usize::from(*rec.get(17)?);
    }
    Some(Coff {
        symptr,
        nsyms,
        strtab,
        strsize,
        sections,
        syms,
    })
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LongRecord {
    pub member: Range<usize>,
    pub hint_at: usize,
    pub symbol: String,
    pub hint: u16,
    pub name: String,
}

pub fn long_records(data: &[u8]) -> Vec<LongRecord> {
    let mut out = Vec::new();
    for m in members(data) {
        let Some(body) = data.get(m.body.clone()) else {
            continue;
        };
        if m.name == "/" || m.name == "//" || short_import(body).is_some() {
            continue;
        }
        let Some(c) = coff(body) else {
            continue;
        };
        let Some(symbol) = c.syms.iter().find_map(|s| {
            (s.class == 2 && s.section > 0)
                .then(|| msvc6_base::srcio::decode(&s.name))
                .and_then(|n| n.strip_prefix("__imp_").map(str::to_string))
        }) else {
            continue;
        };
        let Some((_, off, len)) = c.sections.iter().find(|(n, _, _)| n == ".idata$6") else {
            continue;
        };
        let Some(raw) = body.get(*off..off + len) else {
            continue;
        };
        let text = raw.get(2..).unwrap_or_default();
        let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
        out.push(LongRecord {
            member: m.body.clone(),
            hint_at: m.body.start + off,
            symbol,
            hint: le16(raw, 0).unwrap_or(0),
            name: msvc6_base::srcio::decode(text.get(..end).unwrap_or_default()),
        });
    }
    out
}

pub fn rename_coff_externals(obj: &mut [u8], renames: &[(String, String)]) -> Result<usize> {
    let c = coff(obj).ok_or_else(|| anyhow!("malformed COFF member"))?;
    let bad = || anyhow!("malformed COFF member");
    let mut renamed = 0usize;
    let mut new_names: Vec<Vec<u8>> = Vec::with_capacity(c.syms.len());
    for s in &c.syms {
        let external = s.class == 2 && s.section > 0;
        let new = renames
            .iter()
            .find(|(o, _)| external && o.as_bytes() == s.name.as_slice())
            .map(|(_, n)| n.as_bytes().to_vec());
        if let Some(n) = &new {
            if n.len() > s.name.len() {
                bail!("a rename may only shorten a name, so it fits in place");
            }
            renamed += 1;
        }
        new_names.push(new.unwrap_or_else(|| s.name.clone()));
    }
    for (s, n) in c.syms.iter().zip(&new_names) {
        if s.str_off.is_none() && &s.name != n {
            let slot = obj.get_mut(s.rec..s.rec + 8).ok_or_else(bad)?;
            slot.fill(0);
            slot.get_mut(..n.len()).ok_or_else(bad)?.copy_from_slice(n);
        }
    }
    let strings = obj
        .get(c.strtab..c.strtab + c.strsize)
        .ok_or_else(bad)?
        .to_vec();
    let mut start = 4usize;
    while start < c.strsize {
        let nul = start
            + strings
                .get(start..)
                .and_then(|s| s.iter().position(|&b| b == 0))
                .ok_or_else(bad)?;
        let inside: Vec<(&CoffSym, &Vec<u8>)> = c
            .syms
            .iter()
            .zip(&new_names)
            .filter(|(s, _)| s.str_off.is_some_and(|o| o >= start && o < nul))
            .collect();
        if inside.iter().any(|(s, n)| &s.name != *n) {
            let longest = inside
                .iter()
                .map(|(_, n)| n.as_slice())
                .max_by_key(|n| n.len())
                .ok_or_else(bad)?
                .to_vec();
            if longest.len() > nul - start {
                bail!("a renamed string-table string grew");
            }
            for (_, n) in &inside {
                if !longest.ends_with(n) {
                    bail!(
                        "{:?} and {:?} shared one string-table string and no longer share a suffix",
                        msvc6_base::srcio::decode(&longest),
                        msvc6_base::srcio::decode(n)
                    );
                }
            }
            let span = obj
                .get_mut(c.strtab + start..c.strtab + nul + 1)
                .ok_or_else(bad)?;
            span.fill(0);
            span.get_mut(..longest.len())
                .ok_or_else(bad)?
                .copy_from_slice(&longest);
            for (s, n) in &inside {
                let o = u32::try_from(start + longest.len() - n.len())?;
                obj.get_mut(s.rec + 4..s.rec + 8)
                    .ok_or_else(bad)?
                    .copy_from_slice(&o.to_le_bytes());
            }
        }
        start = nul + 1;
    }
    Ok(renamed)
}

fn nul_strings(block: &[u8], count: usize) -> Option<Vec<Vec<u8>>> {
    let mut out = Vec::with_capacity(count);
    let mut at = 0usize;
    for _ in 0..count {
        let s = block.get(at..)?;
        let end = s.iter().position(|&b| b == 0)?;
        out.push(s.get(..end)?.to_vec());
        at += end + 1;
    }
    Some(out)
}

fn put_strings(block: &mut [u8], names: &[Vec<u8>]) -> Result<()> {
    let total: usize = names.iter().map(|n| n.len() + 1).sum();
    if total > block.len() {
        bail!("renamed linker-member strings no longer fit");
    }
    block.fill(0);
    let mut at = 0usize;
    for n in names {
        block
            .get_mut(at..at + n.len())
            .ok_or_else(|| anyhow!("linker member past the end"))?
            .copy_from_slice(n);
        at += n.len() + 1;
    }
    Ok(())
}

type NameMap<'a> = BTreeMap<&'a [u8], &'a [u8]>;

fn rewrite_first_linker_member(body: &mut [u8], map: &NameMap<'_>) -> Result<usize> {
    let bad = || anyhow!("malformed first linker member");
    let n = be32(body, 0).ok_or_else(bad)?;
    let block = body.get_mut(4 + 4 * n..).ok_or_else(bad)?;
    let mut names = nul_strings(block, n).ok_or_else(bad)?;
    let mut hit = 0usize;
    for s in &mut names {
        if let Some(new) = map.get(s.as_slice()) {
            *s = new.to_vec();
            hit += 1;
        }
    }
    put_strings(block, &names)?;
    Ok(hit)
}

fn rewrite_second_linker_member(body: &mut [u8], map: &NameMap<'_>) -> Result<usize> {
    let bad = || anyhow!("malformed second linker member");
    let m = le32(body, 0).ok_or_else(bad)?;
    let n = le32(body, 4 + 4 * m).ok_or_else(bad)?;
    let idx_at = 8 + 4 * m;
    let str_at = idx_at + 2 * n;
    let names = nul_strings(body.get(str_at..).ok_or_else(bad)?, n).ok_or_else(bad)?;
    if !names.windows(2).all(|w| w.first() <= w.get(1)) {
        bail!("the second linker member LIB wrote is not in byte order");
    }
    let mut pairs = Vec::with_capacity(n);
    let mut hit = 0usize;
    for (k, s) in names.into_iter().enumerate() {
        let idx = le16(body, idx_at + 2 * k).ok_or_else(bad)?;
        let s = match map.get(s.as_slice()) {
            Some(new) => {
                hit += 1;
                new.to_vec()
            }
            None => s,
        };
        pairs.push((s, idx));
    }
    pairs.sort();
    for (k, (_, idx)) in pairs.iter().enumerate() {
        let a = idx_at + 2 * k;
        body.get_mut(a..a + 2)
            .ok_or_else(bad)?
            .copy_from_slice(&idx.to_le_bytes());
    }
    let names: Vec<Vec<u8>> = pairs.into_iter().map(|(s, _)| s).collect();
    put_strings(body.get_mut(str_at..).ok_or_else(bad)?, &names)?;
    Ok(hit)
}

pub fn symbol_table(data: &[u8]) -> Result<BTreeMap<Vec<u8>, usize>> {
    let ms = members(data);
    let second = ms
        .iter()
        .filter(|m| m.name == "/")
        .nth(1)
        .context("the archive has no second linker member")?;
    let body = data.get(second.body.clone()).context("truncated archive")?;
    let bad = || anyhow!("malformed second linker member");
    let m = le32(body, 0).ok_or_else(bad)?;
    let offsets: Vec<usize> = (0..m)
        .map(|i| le32(body, 4 + 4 * i).ok_or_else(bad))
        .collect::<Result<_>>()?;
    let n = le32(body, 4 + 4 * m).ok_or_else(bad)?;
    let idx_at = 8 + 4 * m;
    let names = nul_strings(body.get(idx_at + 2 * n..).ok_or_else(bad)?, n).ok_or_else(bad)?;
    let mut out = BTreeMap::new();
    for (k, s) in names.into_iter().enumerate() {
        let idx = usize::from(le16(body, idx_at + 2 * k).ok_or_else(bad)?);
        let off = *offsets
            .get(idx.checked_sub(1).ok_or_else(bad)?)
            .ok_or_else(bad)?;
        out.insert(s, off);
    }
    Ok(out)
}

pub fn verbatim_symbols(data: &mut [u8]) -> Result<Vec<(String, String)>> {
    let mut renames: Vec<(String, String)> = Vec::new();
    for r in long_records(data) {
        if r.symbol != format!("_{}", r.name) {
            bail!(
                "long-format member `__imp_{}` carries hint/name {:?}: not the `_` + entry LIB writes",
                r.symbol,
                r.name
            );
        }
        let want = [
            (r.symbol.clone(), r.name.clone()),
            (format!("__imp_{}", r.symbol), format!("__imp_{}", r.name)),
        ];
        let body = data
            .get_mut(r.member.clone())
            .ok_or_else(|| anyhow!("long member past the end"))?;
        let n = rename_coff_externals(body, &want)?;
        if n != want.len() {
            bail!(
                "long-format member `__imp_{}`: {n} of its {} externals renamed",
                r.symbol,
                want.len()
            );
        }
        renames.extend(want);
    }
    if renames.is_empty() {
        return Ok(renames);
    }
    let map: NameMap<'_> = renames
        .iter()
        .map(|(o, n)| (o.as_bytes(), n.as_bytes()))
        .collect();
    let linkers: Vec<Range<usize>> = members(data)
        .into_iter()
        .filter(|m| m.name == "/")
        .map(|m| m.body)
        .collect();
    let (Some(first), Some(second)) = (linkers.first().cloned(), linkers.get(1).cloned()) else {
        bail!(
            "{} linker member(s): an archive LIB writes has two",
            linkers.len()
        );
    };
    let a = rewrite_first_linker_member(data.get_mut(first).context("truncated")?, &map)?;
    let b = rewrite_second_linker_member(data.get_mut(second).context("truncated")?, &map)?;
    if a != renames.len() || b != renames.len() {
        bail!(
            "{} rename(s), but the first linker member names {a} of them and the second {b}",
            renames.len()
        );
    }
    Ok(renames)
}

pub fn check_symbols(data: &[u8], renames: &[(String, String)]) -> Result<()> {
    let table = symbol_table(data)?;
    for (old, new) in renames {
        if table.contains_key(old.as_bytes()) {
            bail!("the archive symbol table still names `{old}`");
        }
        let off = *table
            .get(new.as_bytes())
            .with_context(|| format!("the archive symbol table lacks `{new}`"))?;
        let m = members(data)
            .into_iter()
            .find(|m| m.header == off)
            .with_context(|| format!("`{new}`: no member at {off:#x}"))?;
        let c = data
            .get(m.body)
            .and_then(coff)
            .with_context(|| format!("`{new}`: its member is not COFF"))?;
        if !c
            .syms
            .iter()
            .any(|s| s.class == 2 && s.section > 0 && s.name == new.as_bytes())
        {
            bail!("`{new}`: the member the symbol table names does not define it");
        }
    }
    Ok(())
}

pub fn fix_records(
    data: &mut [u8],
    import_name: bool,
    hints: &BTreeMap<String, u16>,
) -> Result<usize> {
    let mut hinted = 0usize;
    let shorts: Vec<(usize, String)> = members(data)
        .into_iter()
        .filter_map(|m| {
            let (sym, _, _) = short_import(data.get(m.body.clone())?)?;
            Some((m.body.start, sym))
        })
        .collect();
    for (at, symbol) in shorts {
        if import_name {
            let f = at + 18;
            let flags = le16(data, f).context("record past the end")?;
            let flags = (flags & !(0x7 << 2)) | (1 << 2);
            data.get_mut(f..f + 2)
                .context("record past the end")?
                .copy_from_slice(&flags.to_le_bytes());
        }
        if let Some(h) = hints.get(&symbol) {
            data.get_mut(at + 16..at + 18)
                .context("record past the end")?
                .copy_from_slice(&h.to_le_bytes());
            hinted += 1;
        }
    }
    for r in long_records(data) {
        if let Some(h) = hints.get(&r.symbol) {
            data.get_mut(r.hint_at..r.hint_at + 2)
                .context("long record past the end")?
                .copy_from_slice(&h.to_le_bytes());
            hinted += 1;
        }
    }
    Ok(hinted)
}

pub fn def_library(def: &str) -> Option<String> {
    def.lines()
        .find_map(|l| l.trim().strip_prefix("LIBRARY"))
        .map(|rest| rest.trim().trim_matches('"').to_string())
        .filter(|n| !n.is_empty())
}

pub fn def_entries(def: &str) -> usize {
    def.lines()
        .skip_while(|l| l.trim() != "EXPORTS")
        .skip(1)
        .filter(|l| {
            let l = l.trim();
            !l.is_empty() && !l.starts_with(';')
        })
        .count()
}

pub fn def_from_dll(file_name: &str, dll: &[u8], ordinals: bool) -> Result<String> {
    let ex = msvc6_image::pe::Image::parse(dll.to_vec())?
        .exports()?
        .with_context(|| format!("{file_name}: no export directory"))?;
    if ex.entries.is_empty() {
        bail!("{file_name}: no exports, so there is nothing to import");
    }
    let mut def = format!("LIBRARY {file_name}\nEXPORTS\n");
    for e in &ex.entries {
        let Some(name) = &e.name else {
            continue;
        };
        let defname = name.strip_prefix('_').unwrap_or(name);
        if ordinals {
            def.push_str(&format!("    {defname} @{}\n", e.ordinal));
        } else {
            def.push_str(&format!("    {defname}\n"));
        }
    }
    Ok(def)
}

pub fn reference_hints(reference: &[u8], dll: &str) -> Result<BTreeMap<String, u16>> {
    let out: BTreeMap<String, u16> = msvc6_image::pe::Image::parse(reference.to_vec())?
        .imports()?
        .into_iter()
        .filter(|d| d.dll.eq_ignore_ascii_case(dll))
        .flat_map(|d| d.entries)
        .filter(|e| !e.by_ordinal())
        .filter_map(|e| Some((e.name, e.hint?)))
        .collect();
    if out.is_empty() {
        bail!("the reference imports nothing by name from {dll}");
    }
    Ok(out)
}

#[derive(Debug, Clone, Default)]
pub struct Opts {
    pub link50compat: bool,
    pub verbatim_symbols: bool,
    pub import_name: bool,
    pub hints: BTreeMap<String, u16>,
    pub lib_args: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Built {
    pub library: String,
    pub entries: usize,
    pub members: usize,
    pub symbols: BTreeSet<String>,
    pub renamed: usize,
    pub hinted: usize,
    pub long: usize,
    pub lines: Vec<String>,
    pub trace: String,
}

pub fn build(
    sess: &Session<'_>,
    tc: &Toolchain,
    def_text: &str,
    work: &Path,
    out: &Path,
    opts: &Opts,
) -> Result<Built> {
    let library = def_library(def_text).context("the .def has no `LIBRARY <dll>` line")?;
    let entries = def_entries(def_text);
    std::fs::create_dir_all(work)?;
    let def_path = work.join(format!("{library}.def"));
    msvc6_base::srcio::write(&def_path, def_text)?;
    let out_abs: PathBuf = if out.is_absolute() {
        out.to_path_buf()
    } else {
        std::env::current_dir()?.join(out)
    };
    if let Some(d) = out_abs.parent() {
        std::fs::create_dir_all(d)?;
    }
    let mut args = vec![
        "/NOLOGO".to_string(),
        "/MACHINE:IX86".to_string(),
        format!("/DEF:{}", to_win(&def_path)),
        format!("/OUT:{}", to_win(&out_abs)),
    ];
    if opts.link50compat {
        args.push("/LINK50COMPAT".to_string());
    }
    args.extend(opts.lib_args.iter().cloned());
    let run = sess.run_tool(
        tc,
        "LIB.EXE",
        &args,
        Some(work),
        &[("WINEDEBUG".to_string(), "-all,+loaddll".to_string())],
    )?;
    let trace = msvc6_base::srcio::tool_text(&run.stderr);
    let mut lines: Vec<String> = run
        .text()
        .lines()
        .filter(|l| !l.contains(":trace:loaddll:"))
        .filter(|l| {
            let l = l.to_ascii_lowercase();
            l.contains("error") || l.contains("warning")
        })
        .map(str::to_string)
        .collect();
    let mut data = std::fs::read(&out_abs)
        .with_context(|| format!("LIB.EXE wrote no archive at {}", out_abs.display()))?;
    let mut renamed = 0;
    if opts.verbatim_symbols {
        let renames = verbatim_symbols(&mut data)?;
        if renames.is_empty() {
            bail!("verbatim symbols: LIB wrote no long-format member to rename");
        }
        check_symbols(&data, &renames)?;
        renamed = renames.len();
        lines.push(format!(
            "{renamed} symbol(s) renamed in place to the .def entry verbatim"
        ));
    }
    let mut hinted = 0;
    if opts.import_name || !opts.hints.is_empty() {
        hinted = fix_records(&mut data, opts.import_name, &opts.hints)?;
        if !opts.hints.is_empty() && hinted != opts.hints.len() {
            bail!(
                "{hinted} of the reference's {} {library} import(s) have a record here: the .def lacks the rest",
                opts.hints.len()
            );
        }
    }
    let longs = long_records(&data);
    if opts.import_name
        && let Some(w) = longs.iter().find(|r| r.name != r.symbol)
    {
        bail!(
            "a long-format member carries a hint/name string other than its symbol ({} -> {:?}); \
             import-name cannot rewrite a string in place",
            w.symbol,
            w.name
        );
    }
    msvc6_base::srcio::replace_bytes(&out_abs, &data)?;
    let ms = members(&data);
    let symbols: BTreeSet<String> = ms
        .iter()
        .filter_map(|m| short_import(data.get(m.body.clone())?).map(|(s, _, _)| s))
        .chain(longs.iter().map(|r| r.symbol.clone()))
        .collect();
    Ok(Built {
        library,
        entries,
        members: ms
            .iter()
            .filter(|m| m.name != "/" && m.name != "//")
            .count(),
        symbols,
        renamed,
        hinted,
        long: longs.len(),
        lines,
        trace,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn header(name: &str, size: usize) -> Vec<u8> {
        let mut h = format!(
            "{name:<16}{:<12}{:<6}{:<6}{:<8}{size:<10}",
            "0", "", "", "0"
        )
        .into_bytes();
        h.extend_from_slice(b"`\n");
        h
    }

    fn long_member(sym: &str, entry: &str) -> Vec<u8> {
        let imp = format!("__imp_{sym}");
        let mut strtab = vec![0u8; 4];
        strtab.extend_from_slice(imp.as_bytes());
        strtab.push(0);
        let sym_off = 4 + imp.len() - sym.len();
        let total = u32::try_from(strtab.len()).unwrap();
        strtab[..4].copy_from_slice(&total.to_le_bytes());
        let mut idata6 = vec![0u8, 0u8];
        idata6.extend_from_slice(entry.as_bytes());
        idata6.push(0);
        let mut o = vec![0u8; 20];
        o[0..2].copy_from_slice(&0x14cu16.to_le_bytes());
        o[2..4].copy_from_slice(&1u16.to_le_bytes());
        let raw_ptr = 20 + 40;
        let symptr = raw_ptr + idata6.len();
        o[8..12].copy_from_slice(&u32::try_from(symptr).unwrap().to_le_bytes());
        o[12..16].copy_from_slice(&2u32.to_le_bytes());
        let mut sec = vec![0u8; 40];
        sec[..8].copy_from_slice(b".idata$6");
        sec[16..20].copy_from_slice(&u32::try_from(idata6.len()).unwrap().to_le_bytes());
        sec[20..24].copy_from_slice(&u32::try_from(raw_ptr).unwrap().to_le_bytes());
        o.extend(sec);
        o.extend(&idata6);
        for off in [4usize, sym_off] {
            let mut s = vec![0u8; 18];
            s[4..8].copy_from_slice(&u32::try_from(off).unwrap().to_le_bytes());
            s[12..14].copy_from_slice(&1i16.to_le_bytes());
            s[16] = 2;
            o.extend(s);
        }
        o.extend(strtab);
        o
    }

    fn archive(sym: &str, entry: &str) -> Vec<u8> {
        let imp = format!("__imp_{sym}");
        let member = long_member(sym, entry);
        let names = [sym, imp.as_str()];
        let strings: Vec<u8> = names.iter().flat_map(|n| n.bytes().chain([0])).collect();
        let first_len = 4 + 4 * 2 + strings.len();
        let mut sorted: Vec<&str> = names.to_vec();
        sorted.sort();
        let sorted_strings: Vec<u8> = sorted.iter().flat_map(|n| n.bytes().chain([0])).collect();
        let second_len = 4 + 4 + 4 + 2 * 2 + sorted_strings.len();
        let pad = |n: usize| n + (n & 1);
        let member_at = 8 + 60 + pad(first_len) + 60 + pad(second_len);
        let mut a = b"!<arch>\n".to_vec();
        a.extend(header("/", first_len));
        a.extend(&2u32.to_be_bytes());
        a.extend(&u32::try_from(member_at).unwrap().to_be_bytes());
        a.extend(&u32::try_from(member_at).unwrap().to_be_bytes());
        a.extend(&strings);
        if first_len & 1 == 1 {
            a.push(b'\n');
        }
        a.extend(header("/", second_len));
        a.extend(&1u32.to_le_bytes());
        a.extend(&u32::try_from(member_at).unwrap().to_le_bytes());
        a.extend(&2u32.to_le_bytes());
        a.extend(&1u16.to_le_bytes());
        a.extend(&1u16.to_le_bytes());
        a.extend(&sorted_strings);
        if second_len & 1 == 1 {
            a.push(b'\n');
        }
        assert_eq!(a.len(), member_at);
        a.extend(header("x.dll/", member.len()));
        a.extend(member);
        a
    }

    #[test]
    fn a_long_member_is_renamed_in_place_and_reads_back() {
        let mut a = archive("__F@4", "_F@4");
        let len = a.len();
        let r = long_records(&a);
        assert_eq!(r.len(), 1);
        assert_eq!(
            (r[0].symbol.as_str(), r[0].name.as_str()),
            ("__F@4", "_F@4")
        );
        let renames = verbatim_symbols(&mut a).unwrap();
        assert_eq!(a.len(), len);
        assert_eq!(renames[0], ("__F@4".to_string(), "_F@4".to_string()));
        check_symbols(&a, &renames).unwrap();
        let hints = BTreeMap::from([("_F@4".to_string(), 0x36u16)]);
        assert_eq!(fix_records(&mut a, false, &hints).unwrap(), 1);
        let r = long_records(&a);
        assert_eq!((r[0].symbol.as_str(), r[0].hint), ("_F@4", 0x36));
        let t = symbol_table(&a).unwrap();
        assert!(t.contains_key(b"__imp__F@4".as_slice()));
    }

    #[test]
    fn a_short_record_and_a_def_read_back() {
        let mut rec = vec![0u8, 0, 0xff, 0xff];
        rec.resize(16, 0);
        rec.extend(&7u16.to_le_bytes());
        rec.extend(&(2u16 << 2).to_le_bytes());
        rec.extend(b"_G@8\0y.dll\0");
        assert_eq!(short_import(&rec), Some(("_G@8".to_string(), 7, 2)));
        let def = "; c\nLIBRARY \"y.dll\"\nEXPORTS\n    G@8\n ; skip\n    H\n";
        assert_eq!(def_library(def).as_deref(), Some("y.dll"));
        assert_eq!(def_entries(def), 2);
    }
}
