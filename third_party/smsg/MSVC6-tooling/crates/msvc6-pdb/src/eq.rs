use crate::convert::{SymStats, TypeStats, conv_sym, conv_type, pointer_fields70, walk_of};
use crate::dbi::{Dbi, NO_STREAM, V60, V70, read_files};
use crate::hash::{TPI70_BUCKETS, hash_v1, type_hash70};
use crate::info::{VC70, read_info};
use crate::le::{bytes, u16_at, u32_at, u32_of, us};
use crate::lines::{LineRow, read_c11, read_c13};
use crate::strtab::StrTab;
use crate::{gsi, msf, msf7, strtab, tpi};
use anyhow::{Context as _, Result, anyhow, bail};
use msvc6_codeview::codec::{Gen, walk};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

type CSym = (u16, Vec<u8>);

#[derive(Debug, Default)]
struct CMod {
    head: Vec<u8>,
    name: Vec<u8>,
    obj: Vec<u8>,
    syms: Vec<CSym>,
    lines: Vec<LineRow>,
}

#[derive(Debug, Default)]
pub struct Canon {
    signature: u32,
    age: u32,
    named: BTreeMap<Vec<u8>, Vec<u8>>,
    info_tail: Vec<u8>,
    names_walk: BTreeMap<u32, Vec<u8>>,
    dbi_fields: Vec<u8>,
    modules: Vec<CMod>,
    symrec: Vec<CSym>,
    globals: Vec<(usize, u32)>,
    publics: Vec<(usize, u32)>,
    addr_map: Vec<usize>,
    psi_fields: Vec<u8>,
    psi_tail: Vec<u8>,
    ti_min: u32,
    types: Vec<Vec<u8>>,
    sc: Vec<u8>,
    segmap: Vec<u8>,
    tsm: Vec<u8>,
    ec: Vec<u8>,
    files: Vec<Vec<Vec<u8>>>,
    dbg: Vec<Option<Vec<u8>>>,
    gsi_checked: (usize, usize, usize),
    tpi_hashes: (usize, usize),
    tpi_offsets: (usize, usize),
    fpm: (usize, usize),
}

enum Container {
    Small(msf::Msf2),
    Big(msf7::Msf7),
}

impl Container {
    fn get(&self, i: usize) -> Result<Vec<u8>> {
        match self {
            Self::Small(m) => m.stream(i),
            Self::Big(m) => m.stream(i),
        }
    }
}

fn dbi_fields(h: &[u8]) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(24);
    for (o, n) in [
        (8, 4),
        (14, 2),
        (18, 2),
        (22, 2),
        (44, 4),
        (56, 2),
        (58, 2),
        (60, 4),
    ] {
        out.extend_from_slice(bytes(h, o, n)?);
    }
    Ok(out)
}

fn region(
    r: &[u8],
    base: usize,
    g: Gen,
    st: &mut SymStats,
) -> Result<(Vec<CSym>, BTreeMap<u32, usize>)> {
    let w = walk(r, walk_of(g))?;
    let mut ord = BTreeMap::new();
    for (i, rec) in w.iter().enumerate() {
        ord.insert(u32_of(base + rec.at, "offset")?, i);
    }
    let mut out = Vec::with_capacity(w.len());
    for rec in w {
        let (k, mut p) = conv_sym(rec.kind, rec.payload, g, st)
            .with_context(|| format!("the record at {:#x}", base + rec.at))?;
        for f in pointer_fields70(k) {
            let v = u32_at(&p, *f)?;
            if v == 0 {
                continue;
            }
            let o = ord.get(&v).ok_or_else(|| {
                anyhow!(
                    "{} at {:#x}: field +{f} = {v:#x} is not a record",
                    msvc6_codeview::codec::sym_name(k),
                    base + rec.at
                )
            })?;
            crate::le::set32(&mut p, *f, u32_of(o + 1, "ordinal")?)?;
        }
        out.push((k, p));
    }
    Ok((out, ord))
}

#[allow(clippy::too_many_lines)]
pub fn read(file: &[u8]) -> Result<Canon> {
    let is70 = msf7::is_pdb70(file);
    let g = if is70 { Gen::Sz } else { Gen::St };
    let mut c = Canon::default();
    let db = if is70 {
        let m = msf7::Msf7::parse(file.to_vec())?;
        c.fpm = m.used_but_free()?;
        Container::Big(m)
    } else {
        Container::Small(msf::Msf2::parse(file.to_vec())?)
    };
    let info = read_info(&db.get(1)?).context("stream 1, the identity")?;
    c.signature = info.signature;
    c.age = info.age;
    c.info_tail.clone_from(&info.tail);
    if is70 && info.version != VC70 {
        bail!("a 7.0 file with identity version {}", info.version);
    }
    let mut strtab = BTreeMap::new();
    for (n, s) in &info.names {
        let b = db.get(us(*s))?;
        if n.as_slice() == b"/names" {
            c.names_walk = strtab::walk(&b)?;
            if is70 {
                strtab = strtab::read(&b)?;
            }
            continue;
        }
        c.named.insert(n.clone(), b);
    }
    let dbi = Dbi::parse(&db.get(3)?).context("stream 3, the DBI")?;
    let want = if is70 { V70 } else { V60 };
    if dbi.version()? != want {
        bail!("DBI version {}, expected {want}", dbi.version()?);
    }
    c.dbi_fields = dbi_fields(&dbi.header)?;
    c.sc.clone_from(&dbi.sc);
    c.segmap.clone_from(&dbi.segmap);
    c.tsm.clone_from(&dbi.tsm);
    c.ec = if dbi.ec.is_empty() {
        StrTab::new().stream()?
    } else {
        dbi.ec.clone()
    };
    c.files = read_files(&dbi.files, g).context("the DBI file info")?;
    for d in &dbi.dbg {
        c.dbg.push(if *d == NO_STREAM {
            None
        } else {
            Some(db.get(usize::from(*d))?)
        });
    }
    let mut st = SymStats::default();
    let mut maps = Vec::with_capacity(dbi.mods.len());
    let mut seen = BTreeSet::new();
    for (imod, m) in dbi.mods.iter().enumerate() {
        let mut h = m.head.clone();
        h.get_mut(34..48).context("module header")?.fill(0);
        let mut cm = CMod {
            head: h,
            name: m.name.clone(),
            obj: m.obj.clone(),
            ..CMod::default()
        };
        let si = m.stream()?;
        if si == NO_STREAM {
            maps.push(BTreeMap::new());
            c.modules.push(cm);
            continue;
        }
        if !seen.insert(si) {
            bail!("module {imod}'s stream {si} is another module's too");
        }
        let s = db.get(usize::from(si))?;
        let (sym, c11, c13) = (m.sym_len()?, m.c11_len()?, m.c13_len()?);
        let (syms, ord) = if sym >= 4 {
            region(bytes(&s, 4, sym - 4)?, 4, g, &mut st)
                .with_context(|| format!("module {imod}"))?
        } else {
            (Vec::new(), BTreeMap::new())
        };
        cm.syms = syms;
        if c11 > 0 {
            for f in read_c11(bytes(&s, sym, c11)?)? {
                for (seg, _, _, pairs) in &f.blocks {
                    for (off, line) in pairs {
                        cm.lines.push((f.name.clone(), *seg, *off, *line));
                    }
                }
            }
        }
        if c13 > 0 {
            let (rows, _) = read_c13(bytes(&s, sym.saturating_add(c11), c13)?, &strtab)?;
            cm.lines.extend(rows);
        }
        if is70 {
            let at = sym.saturating_add(c11).saturating_add(c13);
            let refs = us(u32_at(&s, at)?);
            if at.saturating_add(4).saturating_add(refs) != s.len() {
                bail!("module {imod}: bytes after its global refs");
            }
        }
        cm.lines.sort();
        maps.push(ord);
        c.modules.push(cm);
    }
    let recs = db.get(usize::from(dbi.symrecs()?))?;
    let (mut symrec, rec_ord) = region(&recs, 0, g, &mut st)?;
    for (k, p) in &mut symrec {
        if matches!(*k, 0x1125..=0x1127) {
            let ib = u32_at(p, 4)?;
            let imod = usize::from(u16_at(p, 8)?);
            let o = imod
                .checked_sub(1)
                .and_then(|i| maps.get(i))
                .and_then(|m| m.get(&ib))
                .ok_or_else(|| anyhow!("a reference to {ib:#x} of module {imod}"))?;
            crate::le::set32(p, 4, u32_of(o + 1, "ordinal")?)?;
        }
    }
    let ordinal = |off: u32| -> Result<usize> {
        rec_ord
            .get(&off)
            .copied()
            .ok_or_else(|| anyhow!("a hash or map entry at {off:#x} is not a record"))
    };
    let name_of = |i: usize| -> Result<Vec<u8>> {
        let (k, p) = symrec.get(i).context("ordinal")?;
        gsi::hashed_name70(*k, p)
    };
    let check = |entries: &[(u32, u32, u32)], c: &mut Canon| -> Result<Vec<(usize, u32)>> {
        let mut out = Vec::with_capacity(entries.len());
        let mut prev: Option<(u32, Vec<u8>, u32)> = None;
        for (off, cref, bucket) in entries {
            let i = ordinal(*off)?;
            let n = name_of(i)?;
            c.gsi_checked.0 += 1;
            if hash_v1(&n) % gsi::IPHR_HASH == *bucket {
                c.gsi_checked.1 += 1;
            }
            let ordered = match &prev {
                Some((b, pn, po)) if b == bucket => {
                    gsi::gsi_cmp(pn, &n).then(po.cmp(off)) != std::cmp::Ordering::Greater
                }
                _ => true,
            };
            if ordered {
                c.gsi_checked.2 += 1;
            }
            prev = Some((*bucket, n, *off));
            out.push((i, *cref));
        }
        out.sort_unstable();
        Ok(out)
    };
    let gs = db.get(usize::from(dbi.globals()?))?;
    let ps = db.get(usize::from(dbi.publics()?))?;
    let hash_len = us(u32_at(&ps, 0)?);
    let addr_len = us(u32_at(&ps, 4)?);
    c.psi_fields = bytes(&ps, 8, 20)?.to_vec();
    let tail_at = 28usize.saturating_add(hash_len).saturating_add(addr_len);
    if tail_at > ps.len() {
        bail!("the publics stream's hash and address map run past it");
    }
    c.psi_tail = ps.get(tail_at..).unwrap_or_default().to_vec();
    for i in 0..addr_len / 4 {
        c.addr_map
            .push(ordinal(u32_at(&ps, 28 + hash_len + 4 * i)?)?);
    }
    let phash = bytes(&ps, 28, hash_len)?;
    if is70 {
        c.globals = check(&gsi::read70(&gs)?, &mut c)?;
        c.publics = check(&gsi::read70(phash)?, &mut c)?;
    } else {
        let plain = |v: Vec<(u32, u32)>| -> Result<Vec<(usize, u32)>> {
            let mut out = v
                .into_iter()
                .map(|(o, r)| Ok((ordinal(o)?, r)))
                .collect::<Result<Vec<_>>>()?;
            out.sort_unstable();
            Ok(out)
        };
        c.globals = plain(gsi::read20(&gs)?)?;
        c.publics = plain(gsi::read20(phash)?)?;
    }
    c.symrec = symrec;
    let tpi_b = db.get(2)?;
    let t = tpi::read(&tpi_b)?;
    c.ti_min = t.ti_min;
    let mut ts = TypeStats::default();
    for r in &t.records {
        c.types.push(conv_type(r, g, &mut ts)?);
    }
    if is70 {
        if t.version != tpi::V80 || t.key_size != 4 || t.buckets != TPI70_BUCKETS {
            bail!("the 7.0 TPI header is not V80 / 4-byte keys / {TPI70_BUCKETS:#x} buckets");
        }
        let h = db.get(usize::from(t.hash_stream))?;
        c.tpi_hashes.0 = t.records.len();
        for (i, r) in t.records.iter().enumerate() {
            if tpi::stored_hash(&t, &h, i) == Some(type_hash70(r)? % TPI70_BUCKETS) {
                c.tpi_hashes.1 += 1;
            }
        }
        let mut starts = Vec::with_capacity(t.records.len());
        let mut at = 0u32;
        for r in &t.records {
            starts.push(at);
            at = at.saturating_add(u32_of(r.len(), "record")?);
        }
        let base = us(t.offs.0);
        for i in 0..us(t.offs.1) / 8 {
            let ti = u32_at(&h, base.saturating_add(8 * i))?;
            let off = u32_at(&h, base.saturating_add(8 * i + 4))?;
            c.tpi_offsets.0 += 1;
            let idx = us(ti.saturating_sub(t.ti_min));
            if ti >= t.ti_min && starts.get(idx) == Some(&off) {
                c.tpi_offsets.1 += 1;
            }
        }
    }
    Ok(c)
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Counts {
    pub procs: usize,
    pub publics: usize,
    pub data: usize,
    pub types: usize,
    pub lines: usize,
    pub udts: usize,
    pub labels: usize,
    pub blocks: usize,
    pub thunks: usize,
}

#[must_use]
pub fn counts(c: &Canon) -> Counts {
    let mut n = Counts {
        types: c.types.len(),
        ..Counts::default()
    };
    let mut tally = |k: u16| match k {
        0x110f | 0x1110 => n.procs += 1,
        0x110e => n.publics += 1,
        0x110c | 0x110d | 0x1112 | 0x1113 => n.data += 1,
        0x1108 => n.udts += 1,
        0x1105 => n.labels += 1,
        0x1103 => n.blocks += 1,
        0x1102 => n.thunks += 1,
        _ => {}
    };
    for m in &c.modules {
        for (k, _) in &m.syms {
            tally(*k);
        }
    }
    for (k, _) in &c.symrec {
        tally(*k);
    }
    n.lines = c.modules.iter().map(|m| m.lines.len()).sum();
    n
}

pub fn counts_of(file: &[u8]) -> Result<Counts> {
    Ok(counts(&read(file)?))
}

fn diff(a: &Canon, b: &Canon) -> Vec<String> {
    let mut d = Vec::new();
    let mut eq = |what: &str, same: bool| {
        if !same {
            d.push(format!("DIFFERS: {what}"));
        }
    };
    eq(
        "signature / age",
        (a.signature, a.age) == (b.signature, b.age),
    );
    eq("named streams", a.named == b.named);
    eq("named-stream table niMac", a.info_tail == b.info_tail);
    eq(
        "/names: every 2.0 string at its own offset",
        a.names_walk
            .iter()
            .all(|(o, s)| b.names_walk.get(o) == Some(s)),
    );
    eq("DBI header fields", a.dbi_fields == b.dbi_fields);
    eq("section contributions", a.sc == b.sc);
    eq("segment map", a.segmap == b.segmap);
    eq("type-server map", a.tsm == b.tsm);
    eq("edit-and-continue names", a.ec == b.ec);
    eq("file info", a.files == b.files);
    eq("optional debug header streams", a.dbg == b.dbg);
    eq("symbol records", a.symrec == b.symrec);
    eq("globals hash membership", a.globals == b.globals);
    eq("publics hash membership", a.publics == b.publics);
    eq("publics address map", a.addr_map == b.addr_map);
    eq("publics header fields", a.psi_fields == b.psi_fields);
    eq("publics thunk / section maps", a.psi_tail == b.psi_tail);
    eq("first type index", a.ti_min == b.ti_min);
    eq("module count", a.modules.len() == b.modules.len());
    for (i, (x, y)) in a.types.iter().zip(&b.types).enumerate() {
        if x != y && d.len() < 40 {
            d.push(format!(
                "DIFFERS: type {:#x}",
                a.ti_min + u32::try_from(i).unwrap_or(0)
            ));
        }
    }
    if a.types.len() != b.types.len() {
        d.push(format!(
            "DIFFERS: {} types against {}",
            a.types.len(),
            b.types.len()
        ));
    }
    for (i, (x, y)) in a.modules.iter().zip(&b.modules).enumerate() {
        let name = crate::le::latin1(&x.name);
        if (&x.head, &x.name, &x.obj) != (&y.head, &y.name, &y.obj) {
            d.push(format!("DIFFERS: module {i} ({name}) record"));
        }
        if x.syms != y.syms {
            let first = x
                .syms
                .iter()
                .zip(&y.syms)
                .position(|(p, q)| p != q)
                .unwrap_or(x.syms.len().min(y.syms.len()));
            d.push(format!(
                "DIFFERS: module {i} ({name}) symbols ({} against {}, first at record {first})",
                x.syms.len(),
                y.syms.len()
            ));
        }
        if x.lines != y.lines {
            d.push(format!(
                "DIFFERS: module {i} ({name}) line rows ({} against {})",
                x.lines.len(),
                y.lines.len()
            ));
        }
    }
    d
}

#[derive(Debug, Clone, Serialize)]
pub struct Verdict {
    pub a: Counts,
    pub b: Counts,
    pub modules: usize,
    pub symbols: usize,
    pub globals: usize,
    pub publics: usize,
    pub addr_map: usize,
    pub gsi_checked: (usize, usize, usize),
    pub tpi_hashes: (usize, usize),
    pub tpi_offsets: (usize, usize),
    pub fpm: (usize, usize),
    pub differences: Vec<String>,
    pub hashes_ok: bool,
}

impl Verdict {
    #[must_use]
    pub fn equivalent(&self) -> bool {
        self.differences.is_empty() && self.hashes_ok
    }

    #[must_use]
    pub fn lines(&self, a: &str, b: &str) -> Vec<String> {
        let (na, nb) = (&self.a, &self.b);
        let mut out = vec![
            format!("equivalence {a} <-> {b}:"),
            format!(
                "  procs {} / {} | publics {} / {} | data {} / {} | UDT names {} / {} | types {} / {} | line rows {} / {} | labels {} / {} | blocks {} / {} | thunks {} / {}",
                na.procs,
                nb.procs,
                na.publics,
                nb.publics,
                na.data,
                nb.data,
                na.udts,
                nb.udts,
                na.types,
                nb.types,
                na.lines,
                nb.lines,
                na.labels,
                nb.labels,
                na.blocks,
                nb.blocks,
                na.thunks,
                nb.thunks
            ),
            format!(
                "  {} modules, {} symbol records, {} globals + {} publics hashed, {} address-map entries compared",
                self.modules, self.symbols, self.globals, self.publics, self.addr_map
            ),
            format!(
                "  7.0 side: {} of {} hash records in their name's bucket, {} in the reference's bucket order; {} of {} TPI hash values = 7.0's hash of the record; {} of {} TPI index offsets at their record; {} block(s) in use, {} of them marked free",
                self.gsi_checked.1,
                self.gsi_checked.0,
                self.gsi_checked.2,
                self.tpi_hashes.1,
                self.tpi_hashes.0,
                self.tpi_offsets.1,
                self.tpi_offsets.0,
                self.fpm.0,
                self.fpm.1
            ),
        ];
        if self.equivalent() {
            out.push("  EQUIVALENT: every compared item is identical".into());
        } else {
            for l in self.differences.iter().take(40) {
                out.push(format!("  {l}"));
            }
            if !self.hashes_ok {
                out.push("  the 7.0 hashes do not all check".into());
            }
        }
        out
    }
}

pub fn equivalence(a: &[u8], b: &[u8]) -> Result<Verdict> {
    let ca = read(a).context("reading the first file")?;
    let cb = read(b).context("reading the second file")?;
    let g7 = if cb.gsi_checked.0 > 0 || msf7::is_pdb70(b) {
        &cb
    } else {
        &ca
    };
    let hashes_ok = g7.gsi_checked.0 == g7.gsi_checked.1
        && g7.gsi_checked.0 == g7.gsi_checked.2
        && g7.tpi_hashes.0 == g7.tpi_hashes.1
        && g7.tpi_offsets.0 == g7.tpi_offsets.1
        && g7.fpm.1 == 0;
    Ok(Verdict {
        a: counts(&ca),
        b: counts(&cb),
        modules: ca.modules.len(),
        symbols: ca.modules.iter().map(|m| m.syms.len()).sum::<usize>() + ca.symrec.len(),
        globals: ca.globals.len(),
        publics: ca.publics.len(),
        addr_map: ca.addr_map.len(),
        gsi_checked: g7.gsi_checked,
        tpi_hashes: g7.tpi_hashes,
        tpi_offsets: g7.tpi_offsets,
        fpm: g7.fpm,
        differences: diff(&ca, &cb),
        hashes_ok,
    })
}
