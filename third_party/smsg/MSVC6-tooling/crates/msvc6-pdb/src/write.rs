use crate::le::{u16_of, u32_of};
use crate::{gsi, info, msf, tpi};
use anyhow::{Context as _, Result, bail};
use msvc6_codeview::codec::{Gen, push_name, sym_record};

pub const S_END: u16 = 0x0006;
pub const S_OBJNAME_ST: u16 = 0x0009;
pub const S_PROCREF_ST: u16 = 0x0400;
pub const S_UDT_ST: u16 = 0x1003;
pub const S_GDATA32_ST: u16 = 0x1008;
pub const S_PUB32_ST: u16 = 0x1009;
pub const S_GPROC32_ST: u16 = 0x100b;
const SC_VERSION_600: u32 = 0xeffe_0000 + 19_970_605;
const MODULE_SIGNATURE: u32 = 2;

const ST_OLD_DIR: usize = 0;
const ST_INFO: usize = 1;
const ST_TPI: usize = 2;
const ST_DBI: usize = 3;
const ST_TPI_HASH: usize = 4;
const ST_GSI: usize = 5;
const ST_PSI: usize = 6;
const ST_SYMREC: usize = 7;
const ST_SECHDR: usize = 8;
const ST_FIRST_MODULE: usize = 9;

pub fn st(name: &str) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(name.len() + 1);
    push_name(&mut out, name.as_bytes(), Gen::St)?;
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Proc {
    pub name: String,
    pub isect: u16,
    pub off: u32,
    pub len: u32,
    pub ti: u32,
    pub flags: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Module {
    pub name: String,
    pub obj: String,
    pub procs: Vec<Proc>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Public {
    pub name: String,
    pub isect: u16,
    pub off: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Global {
    pub name: String,
    pub isect: u16,
    pub off: u32,
    pub ti: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Udt {
    pub name: String,
    pub ti: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Spec {
    pub signature: u32,
    pub age: u32,
    pub sections: Vec<[u8; 40]>,
    pub modules: Vec<Module>,
    pub publics: Vec<Public>,
    pub globals: Vec<Global>,
    pub udts: Vec<Udt>,
    pub types: Vec<Vec<u8>>,
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Written {
    pub bytes: Vec<u8>,
    pub page_size: usize,
    pub pages: usize,
    pub streams: Vec<usize>,
    pub symrec_len: usize,
    pub procs: usize,
}

fn word(h: &[u8; 40], at: usize) -> u32 {
    let mut b = [0u8; 4];
    if let Some(s) = h.get(at..at + 4) {
        b.copy_from_slice(s);
    }
    u32::from_le_bytes(b)
}

fn module_stream(m: &Module) -> Result<(Vec<u8>, Vec<u32>)> {
    let mut s = MODULE_SIGNATURE.to_le_bytes().to_vec();
    let mut obj = 0u32.to_le_bytes().to_vec();
    obj.extend_from_slice(&st(&m.obj)?);
    s.extend_from_slice(&sym_record(S_OBJNAME_ST, &obj)?);
    let mut ats = Vec::with_capacity(m.procs.len());
    for p in &m.procs {
        let name = st(&p.name)?;
        let head = 4 + 7 * 4 + 4 + 2 + 1 + name.len();
        let rec_len = head.next_multiple_of(4);
        let at = s.len();
        let end_at = at + rec_len;
        let mut pl = Vec::with_capacity(rec_len);
        pl.extend_from_slice(&0u32.to_le_bytes());
        pl.extend_from_slice(&u32_of(end_at, "S_END offset")?.to_le_bytes());
        pl.extend_from_slice(&0u32.to_le_bytes());
        pl.extend_from_slice(&p.len.to_le_bytes());
        pl.extend_from_slice(&0u32.to_le_bytes());
        pl.extend_from_slice(&p.len.to_le_bytes());
        pl.extend_from_slice(&p.ti.to_le_bytes());
        pl.extend_from_slice(&p.off.to_le_bytes());
        pl.extend_from_slice(&p.isect.to_le_bytes());
        pl.push(p.flags);
        pl.extend_from_slice(&name);
        let r = sym_record(S_GPROC32_ST, &pl)?;
        if r.len() != rec_len {
            bail!(
                "S_GPROC32_ST for {} is {} bytes, planned {rec_len}",
                p.name,
                r.len()
            );
        }
        s.extend_from_slice(&r);
        s.extend_from_slice(&sym_record(S_END, &[])?);
        ats.push(u32_of(at, "procedure offset")?);
    }
    Ok((s, ats))
}

pub fn build(spec: &Spec) -> Result<Written> {
    let nmods = spec.modules.len();
    let mut streams: Vec<Vec<u8>> = vec![Vec::new(); ST_FIRST_MODULE + nmods];
    let mut proc_at: Vec<Vec<u32>> = Vec::with_capacity(nmods);
    let mut sym_lens = Vec::with_capacity(nmods);
    let mut procs = 0usize;
    for (i, m) in spec.modules.iter().enumerate() {
        let (s, ats) = module_stream(m)?;
        procs += ats.len();
        sym_lens.push(s.len());
        proc_at.push(ats);
        if let Some(slot) = streams.get_mut(ST_FIRST_MODULE + i) {
            *slot = s;
        }
    }
    let mut rec = Vec::new();
    let mut pubs: Vec<(u32, &Public)> = Vec::with_capacity(spec.publics.len());
    for p in &spec.publics {
        let mut pl = 0u32.to_le_bytes().to_vec();
        pl.extend_from_slice(&p.off.to_le_bytes());
        pl.extend_from_slice(&p.isect.to_le_bytes());
        pl.extend_from_slice(&st(&p.name)?);
        pubs.push((u32_of(rec.len(), "record offset")?, p));
        rec.extend_from_slice(&sym_record(S_PUB32_ST, &pl)?);
    }
    let mut glob: Vec<(u32, &[u8])> = Vec::new();
    for g in &spec.globals {
        let mut pl = g.ti.to_le_bytes().to_vec();
        pl.extend_from_slice(&g.off.to_le_bytes());
        pl.extend_from_slice(&g.isect.to_le_bytes());
        pl.extend_from_slice(&st(&g.name)?);
        glob.push((u32_of(rec.len(), "record offset")?, g.name.as_bytes()));
        rec.extend_from_slice(&sym_record(S_GDATA32_ST, &pl)?);
    }
    for u in &spec.udts {
        let mut pl = u.ti.to_le_bytes().to_vec();
        pl.extend_from_slice(&st(&u.name)?);
        glob.push((u32_of(rec.len(), "record offset")?, u.name.as_bytes()));
        rec.extend_from_slice(&sym_record(S_UDT_ST, &pl)?);
    }
    for (imod, (m, ats)) in spec.modules.iter().zip(&proc_at).enumerate() {
        for (p, at) in m.procs.iter().zip(ats) {
            let mut r = 14u16.to_le_bytes().to_vec();
            r.extend_from_slice(&S_PROCREF_ST.to_le_bytes());
            r.extend_from_slice(&0u32.to_le_bytes());
            r.extend_from_slice(&at.to_le_bytes());
            r.extend_from_slice(&u16_of(imod + 1, "module index")?.to_le_bytes());
            r.extend_from_slice(&0u16.to_le_bytes());
            r.extend_from_slice(&st(&p.name)?);
            r.resize(r.len().next_multiple_of(4), 0);
            glob.push((u32_of(rec.len(), "record offset")?, p.name.as_bytes()));
            rec.extend_from_slice(&r);
        }
    }
    let symrec_len = rec.len();
    let gsi = gsi::write20(&glob)?;
    let pub_names: Vec<(u32, &[u8])> = pubs.iter().map(|(o, p)| (*o, p.name.as_bytes())).collect();
    let pub_hash = gsi::write20(&pub_names)?;
    let mut by_addr = pubs.clone();
    by_addr.sort_by(|(_, a), (_, b)| {
        (a.isect, a.off, a.name.as_bytes()).cmp(&(b.isect, b.off, b.name.as_bytes()))
    });
    let mut psi = Vec::with_capacity(28 + pub_hash.len() + 4 * by_addr.len());
    psi.extend_from_slice(&u32_of(pub_hash.len(), "publics hash")?.to_le_bytes());
    psi.extend_from_slice(&u32_of(4 * by_addr.len(), "address map")?.to_le_bytes());
    psi.extend_from_slice(&[0u8; 20]);
    psi.extend_from_slice(&pub_hash);
    for (o, _) in &by_addr {
        psi.extend_from_slice(&o.to_le_bytes());
    }
    let (tpi_bytes, tpi_hash) = tpi::write20(&spec.types, u16_of(ST_TPI_HASH, "stream")?)?;
    let dbi = dbi(spec, &sym_lens)?;
    let info = info::write_info20_empty(spec.signature, spec.age);
    let mut sechdr = Vec::with_capacity(40 * spec.sections.len());
    for h in &spec.sections {
        sechdr.extend_from_slice(h);
    }
    for (i, s) in [
        (ST_OLD_DIR, Vec::new()),
        (ST_INFO, info),
        (ST_TPI, tpi_bytes),
        (ST_DBI, dbi),
        (ST_TPI_HASH, tpi_hash),
        (ST_GSI, gsi),
        (ST_PSI, psi),
        (ST_SYMREC, rec),
        (ST_SECHDR, sechdr),
    ] {
        if let Some(slot) = streams.get_mut(i) {
            *slot = s;
        }
    }
    let b = msf::layout(&streams)?;
    Ok(Written {
        bytes: b.bytes,
        page_size: b.page_size,
        pages: b.pages,
        streams: streams.iter().map(Vec::len).collect(),
        symrec_len,
        procs,
    })
}

type Contribution = (u16, u32, u32, u32, u16);

fn contribution(c: Option<&Contribution>) -> Vec<u8> {
    let (isect, off, size, chars, imod) = c.copied().unwrap_or((0, 0, 0, 0, 0));
    let mut b = Vec::with_capacity(28);
    b.extend_from_slice(&isect.to_le_bytes());
    b.extend_from_slice(&0u16.to_le_bytes());
    b.extend_from_slice(&off.to_le_bytes());
    b.extend_from_slice(&size.to_le_bytes());
    b.extend_from_slice(&chars.to_le_bytes());
    b.extend_from_slice(&imod.to_le_bytes());
    b.extend_from_slice(&0u16.to_le_bytes());
    b.extend_from_slice(&0u32.to_le_bytes());
    b.extend_from_slice(&0u32.to_le_bytes());
    b
}

fn dbi(spec: &Spec, sym_lens: &[usize]) -> Result<Vec<u8>> {
    let mut contribs: Vec<Contribution> = Vec::new();
    for (imod, m) in spec.modules.iter().enumerate() {
        for p in &m.procs {
            let chars = spec
                .sections
                .get(usize::from(p.isect).saturating_sub(1))
                .map_or(0, |h| word(h, 36));
            contribs.push((p.isect, p.off, p.len, chars, u16_of(imod, "module index")?));
        }
    }
    contribs.sort_unstable();
    let mut modi = Vec::new();
    for (imod, (m, len)) in spec.modules.iter().zip(sym_lens).enumerate() {
        let first = contribs
            .iter()
            .find(|c| usize::from(c.4) == imod)
            .copied()
            .unwrap_or((0, 0, 0, 0, u16_of(imod, "module index")?));
        let start = modi.len();
        modi.extend_from_slice(&0u32.to_le_bytes());
        modi.extend_from_slice(&contribution(Some(&first)));
        modi.extend_from_slice(&0u16.to_le_bytes());
        modi.extend_from_slice(&u16_of(ST_FIRST_MODULE + imod, "module stream")?.to_le_bytes());
        modi.extend_from_slice(&u32_of(*len, "module symbols")?.to_le_bytes());
        for _ in 0..2 {
            modi.extend_from_slice(&0u32.to_le_bytes());
        }
        modi.extend_from_slice(&0u16.to_le_bytes());
        modi.extend_from_slice(&0u16.to_le_bytes());
        for _ in 0..3 {
            modi.extend_from_slice(&0u32.to_le_bytes());
        }
        if modi.len() - start != 64 {
            bail!(
                "module record header is {} bytes, not 64",
                modi.len() - start
            );
        }
        push_name(&mut modi, m.name.as_bytes(), Gen::Sz)?;
        push_name(&mut modi, m.obj.as_bytes(), Gen::Sz)?;
        modi.resize(modi.len().next_multiple_of(4), 0);
    }
    let mut scs = SC_VERSION_600.to_le_bytes().to_vec();
    for c in &contribs {
        scs.extend_from_slice(&contribution(Some(c)));
    }
    let nseg = u16_of(spec.sections.len() + 1, "segment count")?;
    let mut segmap = nseg.to_le_bytes().to_vec();
    segmap.extend_from_slice(&nseg.to_le_bytes());
    for (i, h) in spec.sections.iter().enumerate() {
        let ch = word(h, 36);
        let mut flags = 0x0100u16 | 0x0008;
        if ch & 0x4000_0000 != 0 {
            flags |= 0x1;
        }
        if ch & 0x8000_0000 != 0 {
            flags |= 0x2;
        }
        if ch & 0x2000_0000 != 0 {
            flags |= 0x4;
        }
        segmap.extend_from_slice(&flags.to_le_bytes());
        segmap.extend_from_slice(&0u16.to_le_bytes());
        segmap.extend_from_slice(&0u16.to_le_bytes());
        segmap.extend_from_slice(&u16_of(i + 1, "frame")?.to_le_bytes());
        segmap.extend_from_slice(&0xffffu16.to_le_bytes());
        segmap.extend_from_slice(&0xffffu16.to_le_bytes());
        segmap.extend_from_slice(&0u32.to_le_bytes());
        segmap.extend_from_slice(&word(h, 8).to_le_bytes());
    }
    for v in [0x0208u16, 0, 0, 0, 0xffff, 0xffff] {
        segmap.extend_from_slice(&v.to_le_bytes());
    }
    segmap.extend_from_slice(&0u32.to_le_bytes());
    segmap.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
    let nm = u16_of(spec.modules.len(), "module count")?;
    let mut files = nm.to_le_bytes().to_vec();
    files.extend_from_slice(&0u16.to_le_bytes());
    for _ in 0..2 * usize::from(nm) {
        files.extend_from_slice(&0u16.to_le_bytes());
    }
    files.resize(files.len().next_multiple_of(4), 0);
    let mut dbg = Vec::with_capacity(12);
    for _ in 0..5 {
        dbg.extend_from_slice(&0xffffu16.to_le_bytes());
    }
    dbg.extend_from_slice(&u16_of(ST_SECHDR, "stream")?.to_le_bytes());
    let mut d = Vec::with_capacity(64 + modi.len() + scs.len() + segmap.len() + files.len() + 12);
    d.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
    d.extend_from_slice(&crate::dbi::V60.to_le_bytes());
    d.extend_from_slice(&spec.age.to_le_bytes());
    for s in [ST_GSI, ST_PSI, ST_SYMREC] {
        d.extend_from_slice(&u16_of(s, "stream")?.to_le_bytes());
        d.extend_from_slice(&0u16.to_le_bytes());
    }
    for n in [modi.len(), scs.len(), segmap.len(), files.len(), 0] {
        d.extend_from_slice(&u32_of(n, "DBI substream")?.to_le_bytes());
    }
    d.extend_from_slice(&0u32.to_le_bytes());
    d.extend_from_slice(&u32_of(dbg.len(), "debug header")?.to_le_bytes());
    d.extend_from_slice(&0u32.to_le_bytes());
    d.extend_from_slice(&0u16.to_le_bytes());
    d.extend_from_slice(&0u16.to_le_bytes());
    d.extend_from_slice(&0u32.to_le_bytes());
    d.extend_from_slice(&modi);
    d.extend_from_slice(&scs);
    d.extend_from_slice(&segmap);
    d.extend_from_slice(&files);
    d.extend_from_slice(&dbg);
    Ok(d)
}

pub fn section_headers(img: &msvc6_image::pe::Image) -> Result<Vec<[u8; 40]>> {
    let at = img.opt_off + usize::from(img.opt_size);
    let mut out = Vec::with_capacity(img.sections.len());
    for i in 0..img.sections.len() {
        let mut h = [0u8; 40];
        h.copy_from_slice(
            img.bytes()
                .get(at + 40 * i..at + 40 * i + 40)
                .context("the section table runs past the image")?,
        );
        out.push(h);
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_symbol_record_pads_inside_its_length_as_vc6_does() {
        let mut pl = vec![0u8; 10];
        pl.extend_from_slice(&st("??1Surface@@QAE@XZ").unwrap());
        let r = sym_record(S_PUB32_ST, &pl).unwrap();
        assert_eq!(&r[..4], &[0x22, 0x00, 0x09, 0x10]);
        assert_eq!(r.len() % 4, 0);
    }

    #[test]
    fn an_empty_spec_builds_a_container_with_its_nine_streams() {
        let w = build(&Spec::default()).unwrap();
        assert_eq!(w.streams.len(), 9);
        assert_eq!(w.page_size, 1024);
        let m = msf::Msf2::parse(w.bytes).unwrap();
        assert!(m.streams.iter().flat_map(|s| &s.pages).all(|p| *p >= 17));
        let d = crate::dbi::Dbi::parse(&m.stream(3).unwrap()).unwrap();
        assert!(d.mods.is_empty());
        assert_eq!(d.section_headers(), Some(8));
    }
}
