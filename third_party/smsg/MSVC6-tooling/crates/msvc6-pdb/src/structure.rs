use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const MSF: &str = "structure: MSF container";
pub const OWNER: &str = "structure: MSF page has one owner";
pub const FREE: &str = "structure: free page map";
pub const DBI: &str = "structure: DBI substreams";
pub const LENGTH: &str = "structure: module stream length";
pub const REGION: &str = "structure: cbSyms = symbol region";
pub const KIND: &str = "structure: scope kind VC6 writes";
pub const PARENT: &str = "structure: pParent = enclosing scope";
pub const END: &str = "structure: pEnd = its closing S_END";
pub const NEXT: &str = "structure: pNext a later sibling";
pub const CLOSE: &str = "structure: S_END closes a scope";
pub const SYMRECS: &str = "structure: symbol records tile";
pub const REF: &str = "structure: reference ibSym";
pub const ONCE: &str = "structure: procedure referenced once";
pub const EC: &str = "structure: EC name reachable by hash";
pub const SRC: &str = "structure: niSrcFile = S_COMPILE2 src";
pub const LINK: &str = "structure: /LinkInfo ichOutfile";

const MAGIC: &[u8] = b"Microsoft C/C++ program database 2.00\r\n\x1aJG";
const S_END: u16 = 0x0006;
const S_COMPILE2_ST: u16 = 0x1013;
const COMPILE2_FIXED: usize = 18;
const NMT_MAGIC: u32 = 0xeffe_effe;

fn fpm_span(page_size: usize) -> Option<usize> {
    match page_size {
        0x400 => Some(8),
        0x800 => Some(4),
        0x1000 => Some(1),
        _ => None,
    }
}

#[derive(Debug, Default)]
pub struct Report {
    rows: BTreeMap<&'static str, (usize, Vec<String>)>,
    pub seen: Seen,
}

#[derive(Debug, Default, Clone, Serialize)]
pub struct Seen {
    pub pages: usize,
    pub modules: usize,
    pub records: usize,
    pub scopes: usize,
    pub refs: usize,
    pub ec_names: usize,
    pub src_names: usize,
    pub link_info: String,
}

impl Report {
    fn ok(&mut self, class: &'static str) {
        self.rows.entry(class).or_default().0 += 1;
    }
    fn fail(&mut self, class: &'static str, why: String) {
        self.rows.entry(class).or_default().1.push(why);
    }
    fn judge(&mut self, class: &'static str, r: Result<(), String>) {
        match r {
            Ok(()) => self.ok(class),
            Err(why) => self.fail(class, why),
        }
    }
    pub fn rows(&self) -> impl Iterator<Item = (&'static str, usize, &[String])> {
        self.rows.iter().map(|(c, (n, f))| (*c, *n, f.as_slice()))
    }
    #[must_use]
    pub fn failures(&self) -> usize {
        self.rows.values().map(|(_, f)| f.len()).sum()
    }
    #[must_use]
    pub fn passed(&self, class: &str) -> usize {
        self.rows
            .iter()
            .find(|(c, _)| **c == class)
            .map_or(0, |(_, (n, _))| *n)
    }
    #[must_use]
    pub fn failed(&self, class: &str) -> Vec<String> {
        self.rows
            .iter()
            .find(|(c, _)| **c == class)
            .map(|(_, (_, f))| f.clone())
            .unwrap_or_default()
    }
    #[must_use]
    pub fn summary(&self) -> String {
        let s = &self.seen;
        format!(
            "{} page(s), {} module(s), {} module record(s), {} scope(s), {} reference(s), {} EC name(s), \
             {} niSrcFile(s); /LinkInfo {}",
            s.pages, s.modules, s.records, s.scopes, s.refs, s.ec_names, s.src_names, s.link_info
        )
    }
}

fn u16_at(d: &[u8], at: usize) -> Option<u16> {
    let b = d.get(at..at.checked_add(2)?)?;
    Some(u16::from_le_bytes([*b.first()?, *b.get(1)?]))
}

fn u32_at(d: &[u8], at: usize) -> Option<u32> {
    let b = d.get(at..at.checked_add(4)?)?;
    Some(u32::from_le_bytes([
        *b.first()?,
        *b.get(1)?,
        *b.get(2)?,
        *b.get(3)?,
    ]))
}

fn us(v: u32) -> usize {
    usize::try_from(v).unwrap_or(usize::MAX)
}

fn latin1(b: &[u8]) -> String {
    b.iter().map(|c| char::from(*c)).collect()
}

fn cstr(d: &[u8], at: usize) -> Option<(String, usize)> {
    let rest = d.get(at..)?;
    let n = rest.iter().position(|c| *c == 0)?;
    Some((latin1(rest.get(..n)?), at + n + 1))
}

fn st(d: &[u8], at: usize) -> Option<String> {
    let n = usize::from(*d.get(at)?);
    Some(latin1(d.get(at + 1..at + 1 + n)?))
}

fn hash_string_v1(b: &[u8]) -> u32 {
    let mut h = 0u32;
    let mut at = 0usize;
    while let Some(w) = u32_at(b, at) {
        h ^= w;
        at += 4;
    }
    if b.len() - at >= 2 {
        h ^= u32::from(u16_at(b, at).unwrap_or(0));
        at += 2;
    }
    if let Some(c) = b.get(at) {
        h ^= u32::from(*c);
    }
    h |= 0x2020_2020;
    h ^= h >> 11;
    h ^ (h >> 16)
}

fn vc6_scope(kind: u16) -> Option<(&'static str, usize, bool)> {
    Some(match kind {
        0x100a => ("S_LPROC32_ST", 35, true),
        0x100b => ("S_GPROC32_ST", 35, true),
        0x0206 => ("S_THUNK32_ST", 21, true),
        0x0207 => ("S_BLOCK32_ST", 18, false),
        0x0208 => ("S_WITH32_ST", 18, false),
        _ => return None,
    })
}

fn foreign_scope(kind: u16) -> Option<&'static str> {
    Some(match kind {
        0x0104 => "S_LPROC16",
        0x0105 => "S_GPROC16",
        0x0106 => "S_THUNK16",
        0x0107 => "S_BLOCK16",
        0x0108 => "S_WITH16",
        0x0204 => "S_LPROC32_16t",
        0x0205 => "S_GPROC32_16t",
        0x0300 => "S_LPROCMIPS_16t",
        0x0301 => "S_GPROCMIPS_16t",
        0x1010 => "S_LPROCMIPS_ST",
        0x1011 => "S_GPROCMIPS_ST",
        0x1015 => "S_LPROCIA64_ST",
        0x1016 => "S_GPROCIA64_ST",
        0x101a => "S_GMANPROC_ST",
        0x101b => "S_LMANPROC_ST",
        0x1102 => "S_THUNK32",
        0x1103 => "S_BLOCK32",
        0x1104 => "S_WITH32",
        0x110f => "S_LPROC32",
        0x1110 => "S_GPROC32",
        0x1114 => "S_LPROCMIPS",
        0x1115 => "S_GPROCMIPS",
        0x1118 => "S_LPROCIA64",
        0x1119 => "S_GPROCIA64",
        0x112a => "S_GMANPROC",
        0x112b => "S_LMANPROC",
        0x1132 => "S_SEPCODE",
        0x1146 => "S_LPROC32_ID",
        0x1147 => "S_GPROC32_ID",
        0x114d => "S_INLINESITE",
        0x1155 => "S_LPROC32_DPC",
        0x1156 => "S_LPROC32_DPC_ID",
        0x115d => "S_INLINESITE2",
        _ => return None,
    })
}

fn no_scope(kind: u16) -> bool {
    matches!(
        kind,
        0x0001..=0x000e
            | 0x0200..=0x0203
            | 0x0209..=0x020f
            | 0x0400..=0x0404
            | 0x1001..=0x1009
            | 0x100c..=0x100f
            | 0x1012..=0x1014
            | 0x1017..=0x1019
    )
}

fn ref_targets(kind: u16) -> Option<(&'static str, &'static [u16])> {
    Some(match kind {
        0x0400 => ("S_PROCREF_ST", &[0x100b, 0x0206]),
        0x0403 => ("S_LPROCREF_ST", &[0x100a]),
        0x0401 => ("S_DATAREF_ST", &[0x1007, 0x1008, 0x100e, 0x100f]),
        _ => return None,
    })
}

fn name_at(kind: u16) -> Option<usize> {
    match kind {
        0x1007 | 0x1008 | 0x100e | 0x100f => Some(10),
        k => vc6_scope(k).map(|(_, at, _)| at),
    }
}

#[derive(Debug, Clone)]
struct Rec<'a> {
    at: usize,
    kind: u16,
    bytes: &'a [u8],
}

impl Rec<'_> {
    fn word(&self, at: usize) -> Option<usize> {
        u32_at(self.bytes, 4 + at).map(us)
    }
    fn name(&self) -> Option<String> {
        name_at(self.kind).and_then(|at| st(self.bytes, 4 + at))
    }
}

fn walk(d: &[u8], from: usize, to: usize) -> (Vec<Rec<'_>>, Option<String>) {
    let mut out = Vec::new();
    let mut at = from;
    while at < to {
        if !at.is_multiple_of(4) {
            return (out, Some(format!("a record at {at:#x} is not 4-aligned")));
        }
        let (Some(len), Some(kind)) = (u16_at(d, at), u16_at(d, at + 2)) else {
            return (
                out,
                Some(format!(
                    "a record at {at:#x} has no room for its header before {to:#x}"
                )),
            );
        };
        let len = usize::from(len);
        if len < 2 {
            return (out, Some(format!("the record at {at:#x} has length {len}")));
        }
        let end = if matches!(kind, 0x0400 | 0x0401 | 0x0403) && len == 14 {
            at + 17 + usize::from(d.get(at + 16).copied().unwrap_or(0))
        } else {
            at + 2 + len
        };
        if end > to {
            return (
                out,
                Some(format!(
                    "the record at {at:#x} (kind {kind:#06x}) ends at {end:#x}, past {to:#x}"
                )),
            );
        }
        let Some(bytes) = d.get(at..end) else {
            return (
                out,
                Some(format!("the record at {at:#x} runs past the stream")),
            );
        };
        out.push(Rec { at, kind, bytes });
        let next = end.next_multiple_of(4).min(to);
        if d.get(end..next).is_none_or(|p| p.iter().any(|c| *c != 0)) {
            return (
                out,
                Some(format!(
                    "the bytes between the record ending {end:#x} and {next:#x} are not zero"
                )),
            );
        }
        at = next;
    }
    (out, None)
}

#[derive(Debug)]
struct Msf<'a> {
    file: &'a [u8],
    ps: usize,
    fpm: usize,
    span: usize,
    pages: usize,
    dir: Vec<usize>,
    streams: Vec<(Option<usize>, Vec<usize>)>,
}

impl<'a> Msf<'a> {
    fn read(file: &'a [u8]) -> Result<Self, String> {
        if !file.starts_with(MAGIC) {
            return Err("not a PDB 2.0 (small MSF) file".into());
        }
        let ps = u32_at(file, 44).map_or(0, us);
        let span = fpm_span(ps).ok_or_else(|| {
            format!("page size {ps}: not one whose free page map this reader has measured")
        })?;
        let fpm = u16_at(file, 48).map_or(0, usize::from);
        let pages = u16_at(file, 50).map_or(0, usize::from);
        let dir_size = u32_at(file, 52).map_or(0, us);
        if Some(file.len()) != pages.checked_mul(ps) {
            return Err(format!(
                "the file is {} bytes, the header's {pages} page(s) of {ps} are {}",
                file.len(),
                pages.saturating_mul(ps)
            ));
        }
        if fpm != 1 && fpm != 1 + span {
            return Err(format!(
                "the header's free page map is at page {fpm}, neither of the two regions (pages 1 and {}) MSPDB alternates between",
                1 + span
            ));
        }
        let n = dir_size.div_ceil(ps);
        if n.saturating_mul(2).saturating_add(60) > ps {
            return Err(format!(
                "the directory's {n} page(s) do not fit the header page"
            ));
        }
        let dir: Vec<usize> = (0..n)
            .filter_map(|i| u16_at(file, 60 + 2 * i).map(usize::from))
            .collect();
        let mut me = Self {
            file,
            ps,
            fpm,
            span,
            pages,
            dir: dir.clone(),
            streams: Vec::new(),
        };
        let d = me
            .gather(&dir, dir_size)
            .ok_or("a directory page is past the file")?;
        let count = u32_at(&d, 0).map_or(usize::MAX, us);
        if count.saturating_mul(8).saturating_add(4) > d.len() {
            return Err(format!(
                "the directory's {count} stream(s) do not fit its {} bytes",
                d.len()
            ));
        }
        let mut at = 4 + 8 * count;
        for i in 0..count {
            let size = u32_at(&d, 4 + 8 * i).unwrap_or(0);
            let len = (size != u32::MAX).then_some(us(size));
            let np = len.map_or(0, |l| l.div_ceil(ps));
            if np.saturating_mul(2).saturating_add(at) > d.len() {
                return Err(format!("stream {i}'s page list runs past the directory"));
            }
            let mut pg = Vec::with_capacity(np);
            for _ in 0..np {
                let p = u16_at(&d, at)
                    .ok_or_else(|| format!("stream {i}'s page list runs past the directory"))?;
                pg.push(usize::from(p));
                at += 2;
            }
            me.streams.push((len, pg));
        }
        if at != d.len() {
            return Err(format!(
                "the directory's page lists end at {at}, its size is {}",
                d.len()
            ));
        }
        Ok(me)
    }

    fn gather(&self, pages: &[usize], size: usize) -> Option<Vec<u8>> {
        if pages.len().saturating_mul(self.ps) < size {
            return None;
        }
        let mut v = Vec::with_capacity(size);
        for p in pages {
            if *p >= self.pages {
                return None;
            }
            v.extend_from_slice(self.file.get(p * self.ps..(p + 1) * self.ps)?);
        }
        v.truncate(size);
        (v.len() == size).then_some(v)
    }

    fn stream(&self, i: usize) -> Option<Vec<u8>> {
        let (len, pages) = self.streams.get(i)?;
        self.gather(pages, len.unwrap_or(0))
    }

    fn pages(&self, r: &mut Report) {
        let mut owner: Vec<Option<String>> = vec![None; self.pages];
        let mut claim = |p: usize, who: String, r: &mut Report| match owner.get_mut(p) {
            None => r.fail(
                OWNER,
                format!(
                    "{who} names page {p}, past the file's {} page(s)",
                    self.pages
                ),
            ),
            Some(Some(o)) => r.fail(OWNER, format!("page {p} is both {o}'s and {who}'s")),
            Some(slot) => *slot = Some(who),
        };
        claim(0, "the header".into(), r);
        for p in 1..1 + 2 * self.span {
            if p < self.pages {
                claim(
                    p,
                    format!("free page map region {}", 1 + (p - 1) / self.span),
                    r,
                );
            }
        }
        for p in &self.dir {
            claim(*p, "the directory".into(), r);
        }
        for (i, (_, pages)) in self.streams.iter().enumerate() {
            for p in pages {
                claim(*p, format!("stream {i}"), r);
            }
        }
        let bits = self.span * self.ps * 8;
        let bit = |p: usize| {
            (p < bits)
                .then(|| self.file.get(self.fpm * self.ps + p / 8))
                .flatten()
                .map(|b| b >> (p % 8) & 1 == 1)
        };
        for (p, o) in owner.iter().enumerate() {
            match (o, bit(p)) {
                (_, None) => r.fail(
                    FREE,
                    format!("page {p} has no bit in the map at page {}", self.fpm),
                ),
                (Some(o), Some(_)) if o == "stream 0" => r.ok(FREE),
                (Some(_), Some(false)) | (None, Some(true)) => r.ok(FREE),
                (Some(o), Some(true)) => r.fail(FREE, format!("page {p} ({o}'s) is marked free")),
                (None, Some(false)) => {
                    r.fail(FREE, format!("page {p} is no one's and is marked used"));
                }
            }
            if o.is_some() {
                r.ok(OWNER);
            }
        }
        let used_past = (self.pages..bits)
            .filter(|p| bit(*p) == Some(false))
            .count();
        if used_past == 0 {
            r.ok(FREE);
        } else {
            r.fail(
                FREE,
                format!(
                    "{used_past} page(s) past the file's {} are marked used",
                    self.pages
                ),
            );
        }
    }
}

#[derive(Debug)]
struct Module {
    name: String,
    stream: u16,
    cb_syms: usize,
    cb_lines: usize,
    cb_c13: usize,
    ni_src: u32,
}

#[derive(Debug)]
struct Dbi {
    symrecs: u16,
    ec: (usize, usize),
    mods: Vec<Module>,
}

fn dbi(d: &[u8]) -> Result<Dbi, String> {
    if u32_at(d, 0) != Some(u32::MAX) {
        return Err("the DBI does not open with the new header's 0xffffffff".into());
    }
    let w = |at: usize| u32_at(d, at).map_or(0, us);
    let lens = [w(24), w(28), w(32), w(36), w(40), w(52), w(48)];
    let total = lens.iter().fold(64usize, |a, n| a.saturating_add(*n));
    if total != d.len() {
        return Err(format!(
            "the DBI's substreams sum to {total} bytes, the stream is {}",
            d.len()
        ));
    }
    let modi_end = 64 + w(24);
    let ec_at = 64 + w(24) + w(28) + w(32) + w(36) + w(40);
    let mut mods = Vec::new();
    let mut at = 64usize;
    while at < modi_end {
        let field = |o: usize| u32_at(d, at + o).map_or(0, us);
        let (name, next) =
            cstr(d, at + 64).ok_or_else(|| format!("the module record at {at:#x} has no name"))?;
        let (_, next) =
            cstr(d, next).ok_or_else(|| format!("module `{name}` has no object name"))?;
        mods.push(Module {
            stream: u16_at(d, at + 34).unwrap_or(0xffff),
            cb_syms: field(36),
            cb_lines: field(40),
            cb_c13: field(44),
            ni_src: u32_at(d, at + 56).unwrap_or(0),
            name,
        });
        at = next.next_multiple_of(4);
    }
    if at != modi_end {
        return Err(format!(
            "the module list ends at {at:#x}, its substream at {modi_end:#x}"
        ));
    }
    Ok(Dbi {
        symrecs: u16_at(d, 20).unwrap_or(0xffff),
        ec: (ec_at, w(52)),
        mods,
    })
}

#[derive(Debug)]
struct Nmt {
    buf: Vec<u8>,
    buckets: Vec<u32>,
    count: u32,
}

fn nmt(t: &[u8]) -> Result<Option<Nmt>, String> {
    if t.is_empty() {
        return Ok(None);
    }
    if u32_at(t, 0) != Some(NMT_MAGIC) || u32_at(t, 4) != Some(1) {
        return Err("the EC names are not a version-1 0xeffeeffe name table".into());
    }
    let cb = u32_at(t, 8).map_or(usize::MAX, us);
    let buf = t
        .get(12..12usize.saturating_add(cb))
        .ok_or("the EC buffer runs past its substream")?
        .to_vec();
    let nb = u32_at(t, 12 + cb).map_or(usize::MAX, us);
    if nb.saturating_mul(4).saturating_add(20 + cb) != t.len() {
        return Err(format!(
            "the EC table is {} bytes, its parts {}",
            t.len(),
            nb.saturating_mul(4).saturating_add(20 + cb)
        ));
    }
    let buckets: Option<Vec<u32>> = (0..nb).map(|i| u32_at(t, 16 + cb + 4 * i)).collect();
    let buckets = buckets.ok_or("the EC buckets run past their substream")?;
    let count = u32_at(t, 16 + cb + 4 * nb).ok_or("the EC table has no count")?;
    Ok(Some(Nmt {
        buf,
        buckets,
        count,
    }))
}

impl Nmt {
    fn name(&self, off: u32) -> Option<String> {
        let o = us(off);
        if o == 0 || self.buf.get(o - 1) != Some(&0) {
            return None;
        }
        cstr(&self.buf, o).map(|(s, _)| s)
    }

    fn run_starts(&self) -> Vec<Option<usize>> {
        let nb = self.buckets.len();
        let mut out = vec![None; nb];
        let Some(zero) = self.buckets.iter().position(|b| *b == 0) else {
            return out;
        };
        let mut start = None;
        for k in 1..=nb {
            let j = (zero + k) % nb;
            if self.buckets.get(j).copied().unwrap_or(0) == 0 {
                start = None;
                continue;
            }
            let s = *start.get_or_insert(j);
            if let Some(o) = out.get_mut(j) {
                *o = Some(s);
            }
        }
        out
    }

    fn check(&self, r: &mut Report) -> BTreeSet<u32> {
        let nb = self.buckets.len();
        let mut held = BTreeSet::new();
        let runs = self.run_starts();
        let dist = |a: usize, b: usize| (b + nb - a) % nb.max(1);
        let names: Vec<Option<String>> = self.buckets.iter().map(|b| self.name(*b)).collect();
        let mut by_name: BTreeMap<&str, Vec<usize>> = BTreeMap::new();
        for (j, n) in names.iter().enumerate() {
            if let Some(n) = n
                && self.buckets.get(j).is_some_and(|b| *b != 0)
            {
                by_name.entry(n.as_str()).or_default().push(j);
            }
        }
        for (i, b) in self.buckets.iter().enumerate().filter(|(_, b)| **b != 0) {
            held.insert(*b);
            let Some(Some(name)) = names.get(i) else {
                r.fail(
                    EC,
                    format!("bucket {i} holds {b:#x}, which is no string start of the buffer"),
                );
                continue;
            };
            let h = us(hash_string_v1(name.as_bytes())) % nb;
            let reach = |j: usize| match runs.get(j).copied().flatten() {
                None => true,
                Some(s) => dist(s, h) <= dist(s, j),
            };
            let order = |j: usize| dist(h, j);
            let found = by_name
                .get(name.as_str())
                .and_then(|ps| {
                    ps.iter()
                        .copied()
                        .filter(|j| reach(*j))
                        .min_by_key(|j| order(*j))
                })
                .and_then(|p| self.buckets.get(p));
            if found == Some(b) {
                r.ok(EC);
            } else {
                r.fail(
                    EC,
                    format!("`{name}` (bucket {i}) is not found from its hash's bucket {h}"),
                );
            }
        }
        if held.len() != us(self.count) {
            r.fail(
                EC,
                format!(
                    "the table holds {} name(s), its count says {}",
                    held.len(),
                    self.count
                ),
            );
        }
        held
    }
}

fn compile2_src(recs: &[Rec<'_>]) -> Option<String> {
    let r = recs.iter().find(|r| r.kind == S_COMPILE2_ST)?;
    let n = usize::from(*r.bytes.get(4 + COMPILE2_FIXED)?);
    let mut at = 4 + COMPILE2_FIXED + 1 + n;
    let mut strings = Vec::new();
    while let Some((s, next)) = cstr(r.bytes, at) {
        if s.is_empty() {
            break;
        }
        strings.push(s);
        at = next;
    }
    strings
        .chunks(2)
        .find(|kv| kv.first().is_some_and(|k| k == "src"))
        .and_then(|kv| kv.get(1).cloned())
}

fn scopes(recs: &[Rec<'_>], tag: &str, r: &mut Report) {
    let starts: BTreeMap<usize, &Rec<'_>> = recs.iter().map(|x| (x.at, x)).collect();
    let mut stack: Vec<(usize, usize)> = Vec::new();
    let mut nexts: Vec<(usize, usize, usize, usize)> = Vec::new();
    for x in recs {
        if let Some((kname, _, has_next)) = vc6_scope(x.kind) {
            r.ok(KIND);
            r.seen.scopes += 1;
            let (parent, pend) = (
                x.word(0).unwrap_or(usize::MAX),
                x.word(4).unwrap_or(usize::MAX),
            );
            let want = stack.last().map_or(0, |s| s.0);
            r.judge(
                PARENT,
                (parent == want).then_some(()).ok_or_else(|| {
                    format!(
                        "{tag}: {kname} at {:#x} has pParent {parent:#x}; the scope enclosing it is {want:#x}",
                        x.at
                    )
                }),
            );
            if has_next {
                nexts.push((x.at, parent, pend, x.word(8).unwrap_or(usize::MAX)));
            }
            stack.push((x.at, pend));
        } else if let Some(k) = foreign_scope(x.kind) {
            r.fail(
                KIND,
                format!(
                    "{tag}: {k} ({:#06x}) at {:#x} opens a scope of a kind VC6 does not write -- REFUSED, its pointers are \
                     not read as if it were one of VC6's five",
                    x.kind, x.at
                ),
            );
            return;
        } else if !no_scope(x.kind) {
            r.fail(
                KIND,
                format!(
                    "{tag}: kind {:#06x} at {:#x} is not known to open no scope -- REFUSED",
                    x.kind, x.at
                ),
            );
            return;
        } else if x.kind == S_END {
            match stack.pop() {
                None => r.fail(
                    CLOSE,
                    format!("{tag}: the S_END at {:#x} closes no scope", x.at),
                ),
                Some((open, pend)) => {
                    r.ok(CLOSE);
                    r.judge(
                        END,
                        (pend == x.at).then_some(()).ok_or_else(|| {
                            let what = starts.get(&pend).map_or_else(
                                || "no record start".to_string(),
                                |t| {
                                    if t.kind == S_END {
                                        "another S_END".to_string()
                                    } else {
                                        format!("a {:#06x}", t.kind)
                                    }
                                },
                            );
                            format!(
                                "{tag}: the scope at {open:#x} closes at {:#x}, its pEnd says {pend:#x} ({what})",
                                x.at
                            )
                        }),
                    );
                }
            }
        }
    }
    for (open, pend) in stack {
        r.fail(
            END,
            format!("{tag}: the scope at {open:#x} (pEnd {pend:#x}) is never closed"),
        );
    }
    for (at, parent, pend, next) in nexts {
        let ok = next == 0
            || starts.get(&next).is_some_and(|t| {
                matches!(t.kind, 0x100a | 0x100b | 0x0206)
                    && next > pend
                    && t.word(0) == Some(parent)
            });
        r.judge(
            NEXT,
            ok.then_some(()).ok_or_else(|| {
                format!(
                    "{tag}: the scope at {at:#x} has pNext {next:#x}, no later procedure or thunk of its scope {parent:#x}"
                )
            }),
        );
    }
}

fn link_info_stream(info: &[u8]) -> Result<Option<usize>, String> {
    let cb = u32_at(info, 12).map_or(usize::MAX, us);
    let names = info
        .get(16..16usize.saturating_add(cb))
        .ok_or("stream 1's names run past it")?;
    let mut at = 16 + cb;
    let size = u32_at(info, at).map_or(0, us);
    let cap = u32_at(info, at + 4).map_or(0, us);
    at += 8;
    let pw = u32_at(info, at).map_or(0, us);
    if pw.saturating_mul(4) > info.len() {
        return Err("stream 1's present bits run past it".into());
    }
    let present: Vec<u32> = (0..pw)
        .filter_map(|i| u32_at(info, at + 4 + 4 * i))
        .collect();
    at = at.saturating_add(4).saturating_add(4 * pw);
    let dw = u32_at(info, at).map_or(0, us);
    at = at.saturating_add(4).saturating_add(dw.saturating_mul(4));
    let bits = (0..cap.min(32 * present.len()))
        .filter(|i| present.get(i / 32).is_some_and(|w| w >> (i % 32) & 1 == 1))
        .count();
    if bits != size {
        return Err(format!(
            "the named-stream map holds {size} entr(ies) but {bits} present bit(s)"
        ));
    }
    for k in 0..size {
        let (Some(key), Some(sn)) = (
            u32_at(info, at.saturating_add(8 * k)),
            u32_at(info, at.saturating_add(8 * k + 4)),
        ) else {
            return Err("the named-stream map's entries run past stream 1".into());
        };
        if cstr(names, us(key)).is_some_and(|(n, _)| n == "/LinkInfo") {
            return Ok(Some(us(sn)));
        }
    }
    Ok(None)
}

fn link_info(s: &[u8]) -> Result<String, String> {
    let w = |at: usize| {
        u32_at(s, at)
            .map(us)
            .ok_or("/LinkInfo is shorter than its header")
    };
    if w(0)? != s.len() {
        return Err(format!(
            "/LinkInfo's cb is {} and the stream {} bytes",
            w(0)?,
            s.len()
        ));
    }
    let (_, _) = cstr(s, w(8)?).ok_or("/LinkInfo's cwd is no string")?;
    let (cmd, _) = cstr(s, w(12)?).ok_or("/LinkInfo's command is no string")?;
    let ich = w(16)?;
    let lower = cmd.to_ascii_lowercase();
    let outs: Vec<usize> = ["-out:", "/out:"]
        .iter()
        .flat_map(|o| lower.match_indices(o).map(|(i, _)| i).collect::<Vec<_>>())
        .filter(|i| {
            *i == 0
                || lower
                    .as_bytes()
                    .get(i - 1)
                    .is_some_and(u8::is_ascii_whitespace)
        })
        .collect();
    match outs.as_slice() {
        [o] if o + 5 == ich => Ok(format!(
            "ichOutfile {ich} = `{}`",
            cmd.get(ich..).unwrap_or_default()
        )),
        [o] => Err(format!(
            "ichOutfile {ich} (`{}`), but the command's -out: path starts at {} (`{}`)",
            cmd.get(ich..).unwrap_or_default(),
            o + 5,
            cmd.get(o + 5..).unwrap_or_default()
        )),
        _ => Err(format!(
            "the command `{cmd}` holds {} -out: option(s), not one",
            outs.len()
        )),
    }
}

#[must_use]
pub fn check(file: &[u8]) -> Report {
    let mut r = Report::default();
    let msf = match Msf::read(file) {
        Ok(m) => m,
        Err(e) => {
            r.fail(MSF, e);
            return r;
        }
    };
    r.ok(MSF);
    r.seen.pages = msf.pages;
    msf.pages(&mut r);
    let Some(d) = msf.stream(3) else {
        r.fail(DBI, "no DBI stream".into());
        return r;
    };
    if d.is_empty() {
        r.seen.link_info = "none (an empty DBI: types only)".into();
        return r;
    }
    let dbi = match dbi(&d) {
        Ok(x) => x,
        Err(e) => {
            r.fail(DBI, e);
            return r;
        }
    };
    r.ok(DBI);
    r.seen.modules = dbi.mods.len();
    let mut owner: BTreeMap<u16, usize> = BTreeMap::new();
    let mut streams: BTreeMap<u16, Vec<u8>> = BTreeMap::new();
    let mut regions: Vec<Vec<Rec<'_>>> = Vec::with_capacity(dbi.mods.len());
    let mut held: Vec<Option<u16>> = Vec::with_capacity(dbi.mods.len());
    for (i, m) in dbi.mods.iter().enumerate() {
        let tag = format!("module {} `{}`", i + 1, m.name);
        if m.stream == 0xffff {
            if m.cb_syms + m.cb_lines + m.cb_c13 != 0 {
                r.fail(
                    LENGTH,
                    format!(
                        "{tag}: no stream, but cbSyms {} cbLines {} cbC13 {}",
                        m.cb_syms, m.cb_lines, m.cb_c13
                    ),
                );
            } else {
                r.ok(LENGTH);
            }
            held.push(None);
            continue;
        }
        if let Some(first) = owner.insert(m.stream, i) {
            r.fail(
                LENGTH,
                format!("{tag}: stream {} is module {}'s too", m.stream, first + 1),
            );
            held.push(None);
            continue;
        }
        match msf.stream(usize::from(m.stream)) {
            Some(s) => {
                streams.insert(m.stream, s);
                held.push(Some(m.stream));
            }
            None => {
                r.fail(
                    LENGTH,
                    format!("{tag}: stream {} is not in the directory", m.stream),
                );
                held.push(None);
            }
        }
    }
    for (i, (m, h)) in dbi.mods.iter().zip(&held).enumerate() {
        let tag = format!("module {} `{}`", i + 1, m.name);
        let Some(s) = h.and_then(|k| streams.get(&k)) else {
            regions.push(Vec::new());
            continue;
        };
        let mut need = m
            .cb_syms
            .saturating_add(m.cb_lines)
            .saturating_add(m.cb_c13);
        if let Some(g) = u32_at(s, need) {
            need = need.saturating_add(4).saturating_add(us(g));
        }
        r.judge(
            LENGTH,
            (need == s.len()).then_some(()).ok_or_else(|| {
                format!(
                    "{tag}: cbSyms {} + cbLines {} + cbC13 {} and the global references make {need} bytes, the stream {}",
                    m.cb_syms,
                    m.cb_lines,
                    m.cb_c13,
                    s.len()
                )
            }),
        );
        if m.cb_syms == 0 {
            regions.push(Vec::new());
            continue;
        }
        if m.cb_syms < 4 || m.cb_syms > s.len() {
            r.fail(
                REGION,
                format!("{tag}: cbSyms {} in a {}-byte stream", m.cb_syms, s.len()),
            );
            regions.push(Vec::new());
            continue;
        }
        let (recs, broke) = walk(s, 4, m.cb_syms);
        r.seen.records += recs.len();
        match broke {
            Some(why) => r.fail(REGION, format!("{tag}: {why} (cbSyms {:#x})", m.cb_syms)),
            None => r.ok(REGION),
        }
        scopes(&recs, &tag, &mut r);
        regions.push(recs);
    }
    let symrecs = (dbi.symrecs != 0xffff)
        .then(|| msf.stream(usize::from(dbi.symrecs)))
        .flatten();
    let mut referenced: BTreeMap<(usize, usize), usize> = BTreeMap::new();
    if let Some(sr) = &symrecs {
        let (recs, broke) = walk(sr, 0, sr.len());
        match broke {
            Some(why) => r.fail(SYMRECS, why),
            None => r.ok(SYMRECS),
        }
        let index: Vec<BTreeMap<usize, &Rec<'_>>> = regions
            .iter()
            .map(|recs| recs.iter().map(|t| (t.at, t)).collect())
            .collect();
        for x in &recs {
            let Some((rname, kinds)) = ref_targets(x.kind) else {
                continue;
            };
            r.seen.refs += 1;
            let ib = u32_at(x.bytes, 8).map_or(usize::MAX, us);
            let imod = u16_at(x.bytes, 12).map_or(0, usize::from);
            let name = st(x.bytes, 16).unwrap_or_default();
            let why = format!("{rname} `{name}` at {:#x} -> module {imod} {ib:#x}", x.at);
            let target = imod
                .checked_sub(1)
                .and_then(|m| index.get(m))
                .and_then(|recs| recs.get(&ib));
            match target {
                None => r.fail(REF, format!("{why}: no record starts there")),
                Some(t) if !kinds.contains(&t.kind) => {
                    r.fail(
                        REF,
                        format!("{why}: a {:#06x} record, not what {rname} names", t.kind),
                    );
                }
                Some(t) => {
                    let tname = t.name().unwrap_or_default();
                    if tname == name {
                        r.ok(REF);
                        *referenced.entry((imod - 1, ib)).or_insert(0) += 1;
                    } else {
                        r.fail(REF, format!("{why}: that record is `{tname}`"));
                    }
                }
            }
        }
    }
    for (i, recs) in regions.iter().enumerate() {
        for t in recs
            .iter()
            .filter(|t| matches!(t.kind, 0x100a | 0x100b | 0x0206))
        {
            let n = referenced.get(&(i, t.at)).copied().unwrap_or(0);
            r.judge(
                ONCE,
                (n == 1).then_some(()).ok_or_else(|| {
                    format!(
                        "module {} `{}`: `{}` at {:#x} is the target of {n} reference(s)",
                        i + 1,
                        dbi.mods.get(i).map_or("", |m| m.name.as_str()),
                        t.name().unwrap_or_default(),
                        t.at
                    )
                }),
            );
        }
    }
    let (ec_at, ec_len) = dbi.ec;
    let table = match nmt(d
        .get(ec_at..ec_at.saturating_add(ec_len))
        .unwrap_or_default())
    {
        Ok(t) => t,
        Err(e) => {
            r.fail(EC, e);
            None
        }
    };
    let held = table.as_ref().map(|t| t.check(&mut r)).unwrap_or_default();
    r.seen.ec_names = held.len();
    let ec_names: BTreeMap<String, u32> = table
        .as_ref()
        .map(|t| {
            held.iter()
                .filter_map(|o| t.name(*o).map(|n| (n, *o)))
                .collect()
        })
        .unwrap_or_default();
    for (i, (m, recs)) in dbi.mods.iter().zip(&regions).enumerate() {
        let src = compile2_src(recs);
        if m.ni_src == 0 && src.is_none() {
            continue;
        }
        let tag = format!("module {} `{}`", i + 1, m.name);
        let named = table.as_ref().and_then(|t| t.name(m.ni_src));
        let verdict = match (m.ni_src, &src, named) {
            (0, Some(s), _) => Err(match ec_names.get(s) {
                None => format!("{tag}: niSrcFile 0, but its S_COMPILE2 names src `{s}`"),
                Some(o) => format!(
                    "{tag}: niSrcFile 0, but its S_COMPILE2 src `{s}` is the EC name at {o:#x}"
                ),
            }),
            (ni, _, None) => Err(format!("{tag}: niSrcFile {ni:#x} is no EC string start")),
            (ni, _, Some(_)) if !held.contains(&ni) => Err(format!(
                "{tag}: niSrcFile {ni:#x} names a string no bucket holds"
            )),
            (ni, None, Some(n)) => Err(format!(
                "{tag}: niSrcFile {ni:#x} names `{n}`, and the module has no S_COMPILE2 src"
            )),
            (ni, Some(s), Some(n)) if *s != n => Err(format!(
                "{tag}: niSrcFile {ni:#x} names `{n}`, its S_COMPILE2 src is `{s}`"
            )),
            _ => {
                r.seen.src_names += 1;
                Ok(())
            }
        };
        r.judge(SRC, verdict);
    }
    r.seen.link_info = match msf.stream(1).as_deref().map(link_info_stream) {
        None => {
            r.fail(LINK, "no stream 1".into());
            "unread".into()
        }
        Some(Err(e)) => {
            r.fail(LINK, e);
            "unread".into()
        }
        Some(Ok(None)) => "not named".into(),
        Some(Ok(Some(sn))) => match msf.stream(sn) {
            None => {
                r.fail(
                    LINK,
                    format!("/LinkInfo names stream {sn}, which is not in the directory"),
                );
                "unread".into()
            }
            Some(s) if s.is_empty() => "empty".into(),
            Some(s) => match link_info(&s) {
                Ok(what) => {
                    r.ok(LINK);
                    what
                }
                Err(e) => {
                    r.fail(LINK, e);
                    "FAILED".into()
                }
            },
        },
    };
    r
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    fn rec(kind: u16, payload: &[u8]) -> Vec<u8> {
        let len = (4 + payload.len()).next_multiple_of(4);
        let mut b = u16::try_from(len - 2).unwrap().to_le_bytes().to_vec();
        b.extend_from_slice(&kind.to_le_bytes());
        b.extend_from_slice(payload);
        b.resize(len, 0);
        b
    }

    fn st_of(s: &str) -> Vec<u8> {
        let mut b = vec![u8::try_from(s.len()).unwrap()];
        b.extend_from_slice(s.as_bytes());
        b
    }

    fn put32(d: &mut [u8], at: usize, v: usize) {
        d[at..at + 4].copy_from_slice(&u32::try_from(v).unwrap().to_le_bytes());
    }

    fn get32(d: &[u8], at: usize) -> usize {
        us(u32_at(d, at).unwrap())
    }

    const SRC_NAME: &str = "C:\\W\\a.cpp";

    #[derive(Debug, Clone)]
    pub(crate) struct Fixture {
        pub(crate) info: Vec<u8>,
        pub(crate) dbi: Vec<u8>,
        pub(crate) module: Vec<u8>,
        pub(crate) symrecs: Vec<u8>,
        pub(crate) link: Vec<u8>,
        pub(crate) at: BTreeMap<&'static str, usize>,
        pub(crate) refs: BTreeMap<&'static str, usize>,
    }

    impl Fixture {
        pub(crate) fn new() -> Self {
            let proc = |name: &str, off: usize| {
                let mut p = vec![0u8; 12];
                p.extend_from_slice(&0x10u32.to_le_bytes());
                p.extend_from_slice(&[0u8; 12]);
                p.extend_from_slice(&u32::try_from(off).unwrap().to_le_bytes());
                p.extend_from_slice(&[1, 0, 0]);
                p.extend_from_slice(&st_of(name));
                rec(0x100b, &p)
            };
            let block = {
                let mut p = vec![0u8; 8];
                p.extend_from_slice(&4u32.to_le_bytes());
                p.extend_from_slice(&0x1014u32.to_le_bytes());
                p.extend_from_slice(&[1, 0]);
                p.extend_from_slice(&st_of(""));
                rec(0x0207, &p)
            };
            let thunk = {
                let mut p = vec![0u8; 12];
                p.extend_from_slice(&0x1040u32.to_le_bytes());
                p.extend_from_slice(&[1, 0, 5, 0, 0]);
                p.extend_from_slice(&st_of("t"));
                rec(0x0206, &p)
            };
            let objname = rec(
                0x0009,
                &[vec![0u8; 4], st_of("C:\\W\\Debug\\a.obj")].concat(),
            );
            let mut c2 = vec![0u8; COMPILE2_FIXED];
            c2.extend_from_slice(&st_of("Microsoft (R) 32-bit C/C++ Optimizing Compiler"));
            for s in ["cwd", "C:\\W", "src", SRC_NAME] {
                c2.extend_from_slice(s.as_bytes());
                c2.push(0);
            }
            c2.push(0);
            let end = rec(S_END, &[]);
            let names = [
                "obj",
                "c2",
                "f",
                "f.block",
                "f.block.end",
                "f.end",
                "g",
                "g.end",
                "t",
                "t.end",
            ];
            let parts = [
                objname,
                rec(S_COMPILE2_ST, &c2),
                proc("f", 0x1010),
                block,
                end.clone(),
                end.clone(),
                proc("g", 0x1020),
                end.clone(),
                thunk,
                end,
            ];
            let mut at = BTreeMap::new();
            let mut o = 4usize;
            for (n, p) in names.iter().zip(&parts) {
                at.insert(*n, o);
                o += p.len();
            }
            let mut region = parts.concat();
            let mut ptr = |rec: &str, field: usize, v: usize| {
                put32(&mut region, at[rec] - 4 + 4 + field, v);
            };
            ptr("f", 4, at["f.end"]);
            ptr("f.block", 0, at["f"]);
            ptr("f.block", 4, at["f.block.end"]);
            ptr("g", 4, at["g.end"]);
            ptr("t", 4, at["t.end"]);
            let mut module = 2u32.to_le_bytes().to_vec();
            module.extend_from_slice(&region);
            module.extend_from_slice(&0u32.to_le_bytes());
            let mut symrecs = Vec::new();
            let mut refs = BTreeMap::new();
            for n in ["f", "g", "t"] {
                refs.insert(n, symrecs.len());
                let mut r = 14u16.to_le_bytes().to_vec();
                r.extend_from_slice(&0x0400u16.to_le_bytes());
                r.extend_from_slice(&0u32.to_le_bytes());
                r.extend_from_slice(&u32::try_from(at[n]).unwrap().to_le_bytes());
                r.extend_from_slice(&1u16.to_le_bytes());
                r.extend_from_slice(&0u16.to_le_bytes());
                r.extend_from_slice(&st_of(n));
                r.resize(r.len().next_multiple_of(4), 0);
                symrecs.extend_from_slice(&r);
            }
            symrecs.extend_from_slice(&rec(
                0x1009,
                &[
                    vec![0u8; 4],
                    0x1010u32.to_le_bytes().to_vec(),
                    vec![1, 0],
                    st_of("_f"),
                ]
                .concat(),
            ));
            let mut ec = NMT_MAGIC.to_le_bytes().to_vec();
            ec.extend_from_slice(&1u32.to_le_bytes());
            let buf = [b"\0".as_slice(), SRC_NAME.as_bytes(), b"\0"].concat();
            ec.extend_from_slice(&u32::try_from(buf.len()).unwrap().to_le_bytes());
            ec.extend_from_slice(&buf);
            ec.extend_from_slice(&5u32.to_le_bytes());
            let h = us(hash_string_v1(SRC_NAME.as_bytes())) % 5;
            for i in 0..5 {
                ec.extend_from_slice(&u32::from(i == h).to_le_bytes());
            }
            ec.extend_from_slice(&1u32.to_le_bytes());
            let mut m = vec![0u8; 64];
            m[34..36].copy_from_slice(&4u16.to_le_bytes());
            put32(&mut m, 36, 4 + region.len());
            put32(&mut m, 56, 1);
            m.extend_from_slice(b"C:\\W\\Debug\\a.obj\0C:\\W\\Debug\\a.obj\0");
            m.resize(m.len().next_multiple_of(4), 0);
            let mut dbi = vec![0u8; 64];
            put32(&mut dbi, 0, 0xffff_ffff);
            put32(&mut dbi, 4, 19_970_606);
            put32(&mut dbi, 8, 1);
            dbi[12..14].copy_from_slice(&0xffffu16.to_le_bytes());
            dbi[16..18].copy_from_slice(&0xffffu16.to_le_bytes());
            dbi[20..22].copy_from_slice(&5u16.to_le_bytes());
            put32(&mut dbi, 24, m.len());
            put32(&mut dbi, 52, ec.len());
            dbi.extend_from_slice(&m);
            dbi.extend_from_slice(&ec);
            let cmd = "\"LINK.EXE\" -re -out:\"C:\\W\\Debug\\w.dll\"";
            let cwd = "C:\\W";
            let mut link = vec![0u8; 24];
            link.extend_from_slice(cwd.as_bytes());
            link.push(0);
            let at_cmd = link.len();
            link.extend_from_slice(cmd.as_bytes());
            link.push(0);
            let at_libs = link.len();
            link.push(0);
            let n = link.len();
            put32(&mut link, 0, n);
            put32(&mut link, 4, 1);
            put32(&mut link, 8, 24);
            put32(&mut link, 12, at_cmd);
            put32(&mut link, 16, cmd.find("-out:").unwrap() + 5);
            put32(&mut link, 20, at_libs);
            let mut info = Vec::new();
            for v in [19_970_604u32, 0x1234_5678, 1, 10] {
                info.extend_from_slice(&v.to_le_bytes());
            }
            info.extend_from_slice(b"/LinkInfo\0");
            for v in [1u32, 1, 1, 1, 0, 0, 6] {
                info.extend_from_slice(&v.to_le_bytes());
            }
            Self {
                info,
                dbi,
                module,
                symrecs,
                link,
                at,
                refs,
            }
        }

        pub(crate) fn file(&self) -> Vec<u8> {
            let streams = [
                Vec::new(),
                self.info.clone(),
                Vec::new(),
                self.dbi.clone(),
                self.module.clone(),
                self.symrecs.clone(),
                self.link.clone(),
            ];
            crate::msf::layout(&streams).unwrap().bytes
        }

        pub(crate) fn point(&mut self, rec: &str, field: usize, v: usize) {
            put32(&mut self.module, self.at[rec] + 4 + field, v);
        }

        pub(crate) fn ref_to(&mut self, name: &str, ib: usize) {
            put32(&mut self.symrecs, self.refs[name] + 8, ib);
        }
    }

    #[test]
    fn the_name_hash_is_vc6s() {
        assert_eq!(hash_string_v1(b"__lc_codepage") % 4096, 3);
        assert_eq!(hash_string_v1(b"iPrimaryLen") % 4096, 14);
        assert_eq!(hash_string_v1(b"wchar_t") % 4096, 18);
        assert_eq!(hash_string_v1(b"LPBYTE") % 4096, 19);
    }

    #[test]
    fn a_clean_pdb_passes_every_class() {
        let r = check(&Fixture::new().file());
        assert_eq!(r.failures(), 0, "{:?}", r.rows().collect::<Vec<_>>());
        for (class, n) in [
            (MSF, 1),
            (DBI, 1),
            (LENGTH, 1),
            (REGION, 1),
            (KIND, 4),
            (PARENT, 4),
            (END, 4),
            (CLOSE, 4),
            (NEXT, 3),
            (SYMRECS, 1),
            (REF, 3),
            (ONCE, 3),
            (EC, 1),
            (SRC, 1),
            (LINK, 1),
        ] {
            assert_eq!(r.passed(class), n, "{class}");
        }
        assert!(r.passed(OWNER) > 0 && r.passed(FREE) > 0);
    }

    #[test]
    fn a_pend_on_another_valid_s_end_fails() {
        let mut fx = Fixture::new();
        let g_end = fx.at["g.end"];
        fx.point("f", 4, g_end);
        let r = check(&fx.file());
        let f = r.failed(END);
        assert_eq!(f.len(), 1, "{f:?}");
        assert!(f[0].contains("another S_END"), "{}", f[0]);
        assert_eq!(r.failures(), 1);
        let mut fx = Fixture::new();
        let b_end = fx.at["f.block.end"];
        fx.point("f", 4, b_end);
        assert_eq!(check(&fx.file()).failed(END).len(), 1);
    }

    #[test]
    fn a_wrong_ibsym_fails() {
        for (to, says) in [
            ("g", "that record is `g`"),
            ("f.block", "a 0x0207 record"),
            ("obj", "a 0x0009 record"),
        ] {
            let mut fx = Fixture::new();
            let ib = fx.at[to];
            fx.ref_to("f", ib);
            let f = check(&fx.file()).failed(REF);
            assert_eq!(f.len(), 1, "{to}: {f:?}");
            assert!(f[0].contains(says), "{to}: {}", f[0]);
        }
        let mut fx = Fixture::new();
        let ib = fx.at["g"] + 4;
        fx.ref_to("f", ib);
        assert!(check(&fx.file()).failed(REF)[0].contains("no record starts there"));
        let mut fx = Fixture::new();
        let g = fx.at["g"];
        fx.module[g + 4 + 36] = b'f';
        let name_at_ref = fx.refs["g"] + 17;
        fx.symrecs[name_at_ref] = b'f';
        let f_at = fx.at["f"];
        fx.ref_to("g", f_at);
        let r = check(&fx.file());
        assert_eq!(r.failed(REF).len(), 0);
        let once = r.failed(ONCE);
        assert_eq!(once.len(), 2, "{once:?}");
    }

    #[test]
    fn a_wrong_pparent_or_pnext_fails() {
        let mut fx = Fixture::new();
        let g = fx.at["g"];
        fx.point("f.block", 0, g);
        assert_eq!(check(&fx.file()).failed(PARENT).len(), 1);
        let mut fx = Fixture::new();
        let b = fx.at["f.block"];
        fx.point("f", 8, b);
        assert_eq!(check(&fx.file()).failed(NEXT).len(), 1);
        let mut fx = Fixture::new();
        let g = fx.at["g"];
        fx.point("f", 8, g);
        let r = check(&fx.file());
        assert_eq!(r.failures(), 0, "{:?}", r.rows().collect::<Vec<_>>());
    }

    #[test]
    fn a_foreign_scope_kind_is_refused() {
        for kind in [0x0205u16, 0x1011, 0x7777] {
            let mut fx = Fixture::new();
            let g = fx.at["g"];
            fx.module[g + 2..g + 4].copy_from_slice(&kind.to_le_bytes());
            let f = check(&fx.file()).failed(KIND);
            assert_eq!(f.len(), 1, "{kind:#x}: {f:?}");
            assert!(f[0].contains("REFUSED"), "{}", f[0]);
        }
    }

    #[test]
    fn a_cbsyms_that_is_not_the_region_fails() {
        let mut fx = Fixture::new();
        let cb = get32(&fx.dbi, 64 + 36);
        put32(&mut fx.dbi, 64 + 36, cb - 2);
        let r = check(&fx.file());
        assert!(r.failed(REGION)[0].contains("past"));
        assert_eq!(r.failed(LENGTH).len(), 1);
        let mut fx = Fixture::new();
        put32(&mut fx.dbi, 64 + 36, cb - 4);
        let r = check(&fx.file());
        assert_eq!(r.failed(REGION).len(), 0);
        assert_eq!(r.failed(LENGTH).len(), 1);
        assert!(r.failed(END)[0].contains("never closed"));
        let mut fx = Fixture::new();
        fx.module.push(0);
        assert_eq!(check(&fx.file()).failed(LENGTH).len(), 1);
    }

    #[test]
    fn the_ec_table_and_nisrcfile_are_held() {
        let fx = Fixture::new();
        let ec = 64 + get32(&fx.dbi, 24);
        let cb = get32(&fx.dbi, ec + 8);
        let slot = |i: usize| ec + 16 + cb + 4 * i;
        let h = us(hash_string_v1(SRC_NAME.as_bytes())) % 5;
        let mut moved = fx.clone();
        put32(&mut moved.dbi, slot(h), 0);
        put32(&mut moved.dbi, slot((h + 2) % 5), 1);
        assert_eq!(check(&moved.file()).failed(EC).len(), 1);
        let mut count = fx.clone();
        put32(&mut count.dbi, slot(5), 2);
        assert_eq!(check(&count.file()).failed(EC).len(), 1);
        let mut ni = fx.clone();
        put32(&mut ni.dbi, 64 + 56, 2);
        assert!(check(&ni.file()).failed(SRC)[0].contains("no EC string start"));
        let mut other = fx.clone();
        let c2 = other.at["c2"];
        let rel = other.module[c2..c2 + 120]
            .windows(SRC_NAME.len())
            .position(|w| w == SRC_NAME.as_bytes())
            .unwrap();
        other.module[c2 + rel + SRC_NAME.len() - 1] = b'x';
        assert!(check(&other.file()).failed(SRC)[0].contains("its S_COMPILE2 src is"));
        let mut zero = fx;
        put32(&mut zero.dbi, 64 + 56, 0);
        assert!(check(&zero.file()).failed(SRC)[0].contains("is the EC name at 0x1"));
    }

    #[test]
    fn link_infos_outfile_index_is_held() {
        let mut fx = Fixture::new();
        let ich = get32(&fx.link, 16);
        put32(&mut fx.link, 16, ich + 1);
        assert!(check(&fx.file()).failed(LINK)[0].contains("-out: path starts at"));
        let mut fx = Fixture::new();
        let n = fx.link.len();
        put32(&mut fx.link, 0, n + 4);
        assert_eq!(check(&fx.file()).failed(LINK).len(), 1);
    }

    #[test]
    fn the_free_page_map_is_held() {
        let clean = Fixture::new().file();
        let ps = 1024usize;
        let fpm = ps;
        let mut free = clean.clone();
        free[fpm + 17 / 8] |= 1 << (17 % 8);
        assert_eq!(check(&free).failed(FREE).len(), 1);
        let pages = clean.len() / ps;
        let mut past = clean.clone();
        past[fpm + pages / 8] &= !(1 << (pages % 8));
        assert!(check(&past).failed(FREE)[0].contains("past the file"));
        let mut extra = clean.clone();
        extra.extend_from_slice(&vec![0u8; ps]);
        extra[50..52].copy_from_slice(&u16::try_from(pages + 1).unwrap().to_le_bytes());
        extra[fpm + pages / 8] &= !(1 << (pages % 8));
        assert!(check(&extra).failed(FREE)[0].contains("no one's"));
        let mut region = clean.clone();
        let d = usize::from(u16_at(&clean, 60).unwrap());
        region.copy_within(d * ps..(d + 1) * ps, 9 * ps);
        region[60..62].copy_from_slice(&9u16.to_le_bytes());
        assert!(
            check(&region)
                .failed(OWNER)
                .iter()
                .any(|e| e.contains("page 9 is both free page map region 2's"))
        );
        let mut head = clean;
        head[48..50].copy_from_slice(&5u16.to_le_bytes());
        assert!(check(&head).failed(MSF)[0].contains("neither of the two regions"));
    }

    #[test]
    fn two_modules_on_one_stream_are_refused_not_walked_twice() {
        let fx = Fixture::new();
        let mut dbi = fx.dbi.clone();
        let m_len = get32(&dbi, 24);
        let rec = dbi[64..64 + m_len].to_vec();
        let mut more = dbi[..64].to_vec();
        for _ in 0..3 {
            more.extend_from_slice(&rec);
        }
        more.extend_from_slice(&dbi[64 + m_len..]);
        put32(&mut more, 24, 3 * m_len);
        dbi = more;
        let mut f = fx.clone();
        f.dbi = dbi;
        let r = check(&f.file());
        assert_eq!(r.failed(LENGTH).len(), 2, "{:?}", r.failed(LENGTH));
    }
}
