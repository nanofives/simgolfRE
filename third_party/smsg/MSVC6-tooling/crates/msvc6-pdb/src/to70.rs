use crate::convert::{PDB70_SYMBOLS, SymStats, TypeStats, conv_region, conv_type, repoint};
use crate::dbi::{Dbi, NO_STREAM, V60, V70, read_files, write_files};
use crate::info::{guid_of, read_info, write_info70};
use crate::le::{bytes, set16, set32, u16_at, u16_of, u32_at, u32_of, us};
use crate::lines::{LineStats, c13_lines, read_c11};
use crate::msf::Msf2;
use crate::strtab::StrTab;
use crate::{gsi, msf7, tpi};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::codec::{Gen, walk};
use std::collections::BTreeMap;

#[must_use]
pub fn new_index(old: u16) -> Option<u16> {
    match old {
        NO_STREAM => Some(NO_STREAM),
        0..4 => Some(old),
        _ => old.checked_add(1).filter(|n| *n != NO_STREAM),
    }
}

fn renumber(old: u16) -> Result<u16> {
    new_index(old).context("stream index overflow")
}

#[derive(Debug, Default, Clone)]
pub struct Converted {
    pub bytes: Vec<u8>,
    pub guid: [u8; 16],
    pub signature: u32,
    pub age: u32,
    pub streams_in: usize,
    pub streams_out: usize,
    pub modules: usize,
    pub module_syms: SymStats,
    pub global_syms: SymStats,
    pub types: TypeStats,
    pub lines: LineStats,
    pub repointed: usize,
    pub refs_repointed: usize,
    pub globals: usize,
    pub publics: usize,
    pub copied: Vec<(u16, String, usize)>,
    pub tpi_version_in: u32,
    pub module_sigs: BTreeMap<u32, usize>,
    pub module_tails: BTreeMap<usize, usize>,
    pub global_refs: usize,
    pub empty_ec: bool,
    pub empty_modules: usize,
    pub info_tail: usize,
}

struct Roles(BTreeMap<usize, String>);

impl Roles {
    fn claim(&mut self, i: usize, what: &str) -> Result<()> {
        if let Some(prev) = self.0.insert(i, what.to_string()) {
            bail!("stream {i} is both {prev} and {what}");
        }
        Ok(())
    }
}

#[allow(clippy::too_many_lines)]
pub fn convert(raw: &[u8]) -> Result<Converted> {
    let db = Msf2::parse(raw.to_vec())?;
    let n_in = db.streams.len();
    if n_in < 4 {
        bail!("the PDB lists {n_in} stream(s); a PDB has at least the identity, TPI and DBI");
    }
    let mut out: Vec<Option<Vec<u8>>> = vec![None; n_in + 1];
    let mut cv = Converted {
        streams_in: n_in,
        guid: guid_of(raw),
        ..Converted::default()
    };
    let mut role = Roles(BTreeMap::new());
    role.claim(0, "the old directory")?;
    role.claim(1, "identity")?;
    role.claim(2, "TPI")?;
    role.claim(3, "DBI")?;

    let info = read_info(&db.stream(1)?).context("stream 1, the identity")?;
    cv.signature = info.signature;
    cv.age = info.age;
    cv.info_tail = info.tail.len();
    for (n, s) in &info.names {
        let i = us(*s);
        if !db.has(i) {
            bail!(
                "the named stream {} is stream {i}, which the directory does not list",
                crate::le::latin1(n)
            );
        }
        role.claim(i, &format!("named stream {}", crate::le::latin1(n)))?;
    }

    let mut dbi = Dbi::parse(&db.stream(3)?).context("stream 3, the DBI")?;
    if dbi.version()? != V60 {
        bail!("DBI version {}, not VC6's {V60}", dbi.version()?);
    }
    if dbi.age()? != info.age {
        bail!(
            "the DBI's age {} differs from stream 1's {}",
            dbi.age()?,
            info.age
        );
    }
    let symrec_i = usize::from(dbi.symrecs()?);
    let globals_i = usize::from(dbi.globals()?);
    let publics_i = usize::from(dbi.publics()?);
    role.claim(symrec_i, "symbol records")?;
    role.claim(globals_i, "globals hash")?;
    role.claim(publics_i, "publics")?;
    for (i, d) in dbi.dbg.iter().enumerate() {
        if *d != NO_STREAM {
            role.claim(usize::from(*d), &format!("debug header entry {i}"))?;
        }
    }

    let old_names = info.names.get(b"/names".as_slice()).copied();
    let mut names = match old_names {
        Some(i) => StrTab::seeded(&db.stream(us(i))?).context("the 2.0 /names")?,
        None => StrTab::new(),
    };
    let mut mod_maps: Vec<BTreeMap<u32, u32>> = Vec::with_capacity(dbi.mods.len());
    let mut pending: Vec<(u16, Vec<u8>, Vec<u32>)> = Vec::with_capacity(dbi.mods.len());
    cv.modules = dbi.mods.len();
    for (imod, m) in dbi.mods.iter_mut().enumerate() {
        let si = m.stream()?;
        if si == NO_STREAM {
            let ni = u16_of(out.len(), "stream")?;
            out.push(None);
            set16(&mut m.head, 34, ni)?;
            set32(&mut m.head, 36, 4)?;
            set32(&mut m.head, 40, 0)?;
            set32(&mut m.head, 44, 0)?;
            pending.push((ni, 4u32.to_le_bytes().to_vec(), Vec::new()));
            mod_maps.push(BTreeMap::new());
            cv.empty_modules += 1;
            continue;
        }
        role.claim(usize::from(si), &format!("module {imod}"))?;
        let s = db.stream(usize::from(si))?;
        let (sym_len, c11, c13) = (m.sym_len()?, m.c11_len()?, m.c13_len()?);
        if c13 != 0 {
            bail!("module {imod} already carries {c13} bytes of C13 lines");
        }
        let tail = s.get(sym_len.saturating_add(c11)..).unwrap_or_default();
        let refs = if tail.is_empty() {
            0
        } else {
            us(u32_at(tail, 0)?)
        };
        if (!tail.is_empty() && tail.len() != refs.saturating_add(4)) || !refs.is_multiple_of(4) {
            bail!(
                "module {imod} ({}): {sym_len} symbol + {c11} line bytes, stream {} bytes, \
                 global refs {refs}: not the layout this converter carries",
                m.name_text(),
                s.len()
            );
        }
        *cv.module_tails.entry(tail.len()).or_default() += 1;
        let mut new = Vec::with_capacity(s.len() + 64);
        let mut map = BTreeMap::new();
        if sym_len >= 4 {
            *cv.module_sigs.entry(u32_at(&s, 0)?).or_default() += 1;
            let (mut region, rmap) =
                conv_region(bytes(&s, 4, sym_len - 4)?, 4, &mut cv.module_syms)
                    .with_context(|| format!("module {imod} ({})", m.name_text()))?;
            cv.repointed += repoint(&mut region, 4, &rmap)?;
            new.extend_from_slice(&4u32.to_le_bytes());
            new.extend_from_slice(&region);
            map = rmap;
        } else if sym_len != 0 {
            bail!("module {imod} has {sym_len} symbol bytes, less than its signature");
        }
        let new_sym = new.len();
        let files = read_c11(bytes(&s, sym_len, c11)?)
            .with_context(|| format!("module {imod} ({}): its C11 lines", m.name_text()))?;
        let procs: Vec<(u16, u32, u32)> = walk(new.get(4..).unwrap_or_default(), PDB70_SYMBOLS)?
            .into_iter()
            .filter(|r| matches!(r.kind, 0x110f | 0x1110))
            .map(|r| {
                Ok((
                    u16_at(r.payload, 32)?,
                    u32_at(r.payload, 28)?,
                    u32_at(r.payload, 12)?,
                ))
            })
            .collect::<Result<_>>()?;
        let c13b = c13_lines(&files, &procs, &mut names, &mut cv.lines)?;
        new.extend_from_slice(&c13b);
        let old_refs = (0..refs / 4)
            .map(|i| u32_at(tail, 4 + 4 * i))
            .collect::<Result<Vec<u32>>>()?;
        cv.global_refs += old_refs.len();
        set16(&mut m.head, 34, renumber(si)?)?;
        set32(&mut m.head, 36, u32_of(new_sym, "module symbols")?)?;
        set32(&mut m.head, 40, 0)?;
        set32(&mut m.head, 44, u32_of(c13b.len(), "module C13")?)?;
        pending.push((renumber(si)?, new, old_refs));
        mod_maps.push(map);
    }

    let recs = db.stream(symrec_i)?;
    let (mut symrec, rec_map) = conv_region(&recs, 0, &mut cv.global_syms)?;
    let walked: Vec<(usize, u16)> = walk(&symrec, PDB70_SYMBOLS)?
        .into_iter()
        .map(|r| (r.at, r.kind))
        .collect();
    for (at, kind) in walked {
        if !matches!(kind, 0x1125..=0x1127) {
            continue;
        }
        let ib = u32_at(&symrec, at + 8)?;
        let imod = usize::from(u16_at(&symrec, at + 12)?);
        let map = imod
            .checked_sub(1)
            .and_then(|i| mod_maps.get(i))
            .ok_or_else(|| anyhow!("a reference at {at:#x} names module {imod}"))?;
        let new = *map.get(&ib).ok_or_else(|| {
            anyhow!("a reference at {at:#x} points at {ib:#x} of module {imod}, not a record")
        })?;
        if new != ib {
            cv.refs_repointed += 1;
        }
        set32(&mut symrec, at + 8, new)?;
    }
    let name_of = |off: u32| -> Result<Vec<u8>> {
        let at = us(off);
        let kind = u16_at(&symrec, at + 2)?;
        let len = usize::from(u16_at(&symrec, at)?);
        gsi::hashed_name70(kind, bytes(&symrec, at + 4, len.saturating_sub(2))?)
    };
    let remap = |off: u32| -> Result<u32> {
        rec_map
            .get(&off)
            .copied()
            .ok_or_else(|| anyhow!("a hash record points at {off:#x}, not a symbol record"))
    };
    for (ni, mut stream, refs) in pending {
        stream.extend_from_slice(&u32_of(4 * refs.len(), "global refs")?.to_le_bytes());
        for r in refs {
            let n = remap(r).context("a module's global ref")?;
            stream.extend_from_slice(&n.to_le_bytes());
        }
        if let Some(slot) = out.get_mut(usize::from(ni)) {
            *slot = Some(stream);
        }
    }

    let g = db.stream(globals_i)?;
    let mut gl = Vec::new();
    for (off, cref) in gsi::read20(&g).context("the globals hash")? {
        let n = remap(off)?;
        gl.push((n, cref, name_of(n)?));
    }
    cv.globals = gl.len();
    let p = db.stream(publics_i)?;
    let hash_len = us(u32_at(&p, 0)?);
    let addr_len = us(u32_at(&p, 4)?);
    let nthunk = us(u32_at(&p, 8)?);
    let nsect = us(u32_at(&p, 24)?);
    let tail_at = 28usize.saturating_add(hash_len).saturating_add(addr_len);
    let need = tail_at
        .saturating_add(nthunk.saturating_mul(4))
        .saturating_add(nsect.saturating_mul(8));
    if need != p.len() {
        bail!(
            "the publics stream is {} bytes and its tables account for {need}",
            p.len()
        );
    }
    let mut pl = Vec::new();
    for (off, cref) in gsi::read20(bytes(&p, 28, hash_len)?).context("the publics hash")? {
        let n = remap(off)?;
        pl.push((n, cref, name_of(n)?));
    }
    cv.publics = pl.len();
    let phash = gsi::write70(&pl)?;
    let mut psi = Vec::with_capacity(p.len() + 1024);
    psi.extend_from_slice(&u32_of(phash.len(), "publics hash")?.to_le_bytes());
    psi.extend_from_slice(bytes(&p, 4, 24)?);
    psi.extend_from_slice(&phash);
    for i in 0..addr_len / 4 {
        psi.extend_from_slice(&remap(u32_at(&p, 28 + hash_len + 4 * i)?)?.to_le_bytes());
    }
    psi.extend_from_slice(p.get(tail_at..).unwrap_or_default());

    let tpi_bytes = db.stream(2)?;
    let t = tpi::read(&tpi_bytes).context("stream 2, the TPI")?;
    cv.tpi_version_in = t.version;
    if t.adj.1 != 0 {
        bail!(
            "the TPI carries {} bytes of hash adjusters, which this converter does not carry",
            t.adj.1
        );
    }
    if t.hash_stream != NO_STREAM {
        if !db.has(usize::from(t.hash_stream)) {
            bail!(
                "the TPI names hash stream {}, which the directory does not list",
                t.hash_stream
            );
        }
        role.claim(usize::from(t.hash_stream), "TPI hash")?;
    }
    if t.hash_aux != NO_STREAM {
        bail!("the TPI names an auxiliary hash stream {}", t.hash_aux);
    }
    let mut types = Vec::with_capacity(t.records.len());
    for (i, r) in t.records.iter().enumerate() {
        types
            .push(conv_type(r, Gen::St, &mut cv.types).with_context(|| {
                format!("type {:#x}", t.ti_min + u32::try_from(i).unwrap_or(0))
            })?);
    }
    let new_hash_i = if t.hash_stream == NO_STREAM {
        out.push(None);
        u16_of(out.len() - 1, "stream")?
    } else {
        renumber(t.hash_stream)?
    };
    let (tpi70, tpi70_hash) = tpi::write70(&types, t.ti_min, new_hash_i)?;

    let files = read_files(&dbi.files, Gen::St).context("the DBI file info")?;
    if !dbi.files.is_empty() && files.len() != dbi.mods.len() {
        bail!(
            "the file info lists {} modules and the DBI {}",
            files.len(),
            dbi.mods.len()
        );
    }
    if !dbi.files.is_empty() {
        dbi.files = write_files(&files, Gen::Sz)?;
    }
    set32(&mut dbi.header, 4, V70)?;
    let (gi, pi, si) = (
        renumber(dbi.globals()?)?,
        renumber(dbi.publics()?)?,
        renumber(dbi.symrecs()?)?,
    );
    set16(&mut dbi.header, 12, gi)?;
    set16(&mut dbi.header, 16, pi)?;
    set16(&mut dbi.header, 20, si)?;
    for d in &mut dbi.dbg {
        *d = renumber(*d)?;
    }
    if !dbi.ec.is_empty() && u32_at(&dbi.ec, 0)? != crate::strtab::MAGIC {
        bail!("the DBI's edit-and-continue substream is not a 0xeffeeffe string table");
    }
    if dbi.ec.is_empty() {
        dbi.ec = StrTab::new().stream()?;
        cv.empty_ec = true;
    }

    let mut info70 = info.clone();
    for v in info70.names.values_mut() {
        *v = u32::from(renumber(u16_of(us(*v), "stream")?)?);
    }
    let names_i = match old_names {
        Some(i) => renumber(u16_of(us(i), "stream")?)?,
        None => {
            out.push(None);
            let n = u16_of(out.len() - 1, "stream")?;
            info70.names.insert(b"/names".to_vec(), u32::from(n));
            n
        }
    };

    let mut put = |i: u16, b: Vec<u8>| {
        if let Some(slot) = out.get_mut(usize::from(i)) {
            *slot = Some(b);
        }
    };
    put(0, Vec::new());
    put(1, write_info70(&info70, &cv.guid)?);
    put(2, tpi70);
    put(3, dbi.bytes()?);
    put(4, Vec::new());
    put(renumber(u16_of(symrec_i, "stream")?)?, symrec);
    put(renumber(u16_of(globals_i, "stream")?)?, gsi::write70(&gl)?);
    put(renumber(u16_of(publics_i, "stream")?)?, psi);
    put(new_hash_i, tpi70_hash);
    put(names_i, names.stream()?);
    for i in 4..n_in {
        let ni = usize::from(renumber(u16_of(i, "stream")?)?);
        if out.get(ni).is_some_and(Option::is_some) {
            continue;
        }
        let b = db.stream(i)?;
        let what = role.0.get(&i).cloned().unwrap_or_else(|| {
            if b.is_empty() {
                "empty".to_string()
            } else {
                "UNCLASSIFIED".to_string()
            }
        });
        if what == "UNCLASSIFIED" {
            bail!(
                "stream {i} ({} bytes) is not one this converter can classify",
                b.len()
            );
        }
        cv.copied.push((u16_of(ni, "stream")?, what, b.len()));
        if let Some(slot) = out.get_mut(ni) {
            *slot = Some(b);
        }
    }
    while out.last().is_some_and(Option::is_none) {
        out.pop();
    }
    let streams: Vec<Vec<u8>> = out.into_iter().map(Option::unwrap_or_default).collect();
    cv.streams_out = streams.len();
    cv.bytes = msf7::write(&streams)?;
    Ok(cv)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_stream_renumbering_keeps_four_free() {
        assert_eq!(new_index(3), Some(3));
        assert_eq!(new_index(4), Some(5));
        assert_eq!(new_index(NO_STREAM), Some(NO_STREAM));
        assert_eq!(new_index(0xfffe), None);
    }
}
