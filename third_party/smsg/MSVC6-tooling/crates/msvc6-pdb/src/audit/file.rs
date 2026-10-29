use super::{Auditor, Inputs, ModRef, Shipped, Tally, TypeRange, base_lower, fpo, segmap};
use crate::dbi::{DBG_FPO, DBG_SECTION_HEADER, SC_SIZE};
use crate::msf::{Msf2, Unheld};
use crate::paths::Policy;
use crate::reloc::{
    DBI_AGE_AT, DbiView, SEGMAP_SIZE, TS_AGE, TS_SIG, records, type_servers, u16_at, u32_at,
};
use anyhow::{Context as _, Result};
use msvc6_image::pe::Image;
use std::collections::{BTreeMap, BTreeSet};
use std::io::Write;
use std::path::Path;

pub const HOST_PATHS: [&str; 8] = [
    "z:\\",
    "/tmp/",
    "/home/",
    "/local/",
    "/users/",
    "/root/",
    "/private/",
    "/var/folders/",
];
pub const MAX_HOSTNAME: u64 = 4096;

pub(super) fn audit(
    db: &Msf2,
    ship: &Shipped<'_>,
    original: Option<&Msf2>,
    i: &Inputs<'_>,
    policy: Option<&Policy>,
    w: &mut dyn Write,
) -> Result<Tally> {
    let dbi_bytes = db.stream(3)?;
    let dbi = DbiView::parse(&dbi_bytes)?;
    let mut a = Auditor::new(ship, i.examples);
    if let (Some(o), Some(ours)) = (original, &ship.ours) {
        a.before = Some(super::Before::read(o, ours.img)?);
    }
    let info = crate::info::read_info(&db.stream(1)?)?;
    if dbi.age == info.age {
        a.tally.ok("identity: DBI age = stream 1 age");
    } else {
        a.tally.fail(
            "identity: DBI age = stream 1 age",
            format!("DBI header age {} but stream 1 age {}", dbi.age, info.age),
        );
    }
    let cv = match ship.img {
        Some(img) => img.codeview()?,
        None => None,
    };
    match cv {
        Some(cv) if cv.signature == info.signature && cv.age == info.age => {
            a.tally.ok("identity: shipped NB10 record");
            writeln!(
                w,
                "  identity: MATCHES the shipped {} record (signature {:#010x} age {})",
                cv.format, cv.signature, cv.age
            )?;
        }
        Some(cv) => a.tally.fail(
            "identity: shipped NB10 record",
            format!(
                "the image names signature {:#010x} age {}, the PDB says {:#010x} age {}",
                cv.signature, cv.age, info.signature, info.age
            ),
        ),
        None => {
            let stamp = ship.img.map(|img| img.timestamp);
            if stamp.is_some_and(|t| t == info.signature) && info.age == 1 {
                a.tally
                    .ok("identity: a record-less image's PDB is named by its TimeDateStamp");
            } else {
                a.tally.fail(
                    "identity: a record-less image's PDB is named by its TimeDateStamp",
                    format!(
                        "the PDB says signature {:#010x} age {}; the image's TimeDateStamp is {:#010x} (age 1)",
                        info.signature,
                        info.age,
                        stamp.unwrap_or(0)
                    ),
                );
            }
        }
    }
    structure(&mut a.tally, db, w)?;
    hygiene(
        &mut a,
        db,
        &dbi,
        &dbi_bytes,
        &info,
        original,
        (i.host, i.host_paths),
    )?;
    if let Some(p) = policy {
        sources(&mut a.tally, db, p)?;
    }
    if let Some(o) = original {
        held_to_original(&mut a, db, o, &dbi, &dbi_bytes)?;
    }
    let mods: Vec<ModRef> = dbi
        .mods
        .iter()
        .enumerate()
        .map(|(index, m)| ModRef {
            index,
            obj: base_lower(&m.name),
        })
        .collect();
    let tpi = db.stream(2)?;
    a.types = Some(TypeRange {
        min: u32_at(&tpi, 8).unwrap_or(0x1000),
        mac: u32_at(&tpi, 12).unwrap_or(0x1000),
    });
    if let Some(o) = original {
        let mut cmp = |what: &str, ours: &[u8], theirs: &[u8]| {
            match ours
            .iter()
            .zip(theirs)
            .position(|(x, y)| x != y)
        {
            None if ours.len() == theirs.len() => a.tally.ok("TPI = --original"),
            first => a.tally.fail(
                "TPI = --original",
                format!(
                    "the {what} stream differs from --original's (first at byte {}) -- a relocation touches no type",
                    first.unwrap_or(ours.len().min(theirs.len()))
                ),
            ),
        }
        };
        let otpi = o.stream(2)?;
        cmp("TPI", &tpi, &otpi);
        if let Some(sn) = u16_at(&tpi, 20).filter(|s| *s != 0xffff) {
            cmp(
                "TPI hash",
                &db.stream(usize::from(sn))?,
                &o.stream(usize::from(sn))?,
            );
        }
    }
    let recs = db.stream(usize::from(dbi.symrecs))?;
    let was_recs = original
        .map(|o| o.stream(usize::from(dbi.symrecs)))
        .transpose()?;
    a.region_from(&recs, "symbol records", None, was_recs.as_deref());
    let mut streams: Vec<Option<Vec<u8>>> = Vec::with_capacity(dbi.mods.len());
    for (m, r) in dbi.mods.iter().zip(&mods) {
        if m.stream == 0xffff {
            streams.push(None);
            continue;
        }
        let s = db.stream(usize::from(m.stream))?;
        if m.sym_len > 4 {
            let region = s.get(4..m.sym_len.min(s.len())).unwrap_or_default();
            let was = original
                .map(|o| o.stream(usize::from(m.stream)))
                .transpose()?;
            let was_region = was
                .as_deref()
                .and_then(|x| x.get(4..m.sym_len.min(x.len())));
            a.region_from(region, &format!("module {}", r.obj), Some(r), was_region);
        }
        streams.push(Some(s));
    }
    a.cross_module();
    contributions(&mut a, &dbi, &dbi_bytes, &mods);
    for ((m, r), s) in dbi.mods.iter().zip(&mods).zip(&streams) {
        let Some(s) = s else { continue };
        if m.line_len > 0 {
            let lines = s
                .get(m.sym_len..m.sym_len.saturating_add(m.line_len).min(s.len()))
                .unwrap_or_default();
            let was = original
                .map(|o| o.stream(usize::from(m.stream)))
                .transpose()?;
            let was_lines = was
                .as_deref()
                .and_then(|x| x.get(m.sym_len..m.sym_len.saturating_add(m.line_len).min(x.len())));
            a.c11_from(lines, &format!("module {}", r.obj), r, was_lines);
        }
    }
    publics(&mut a, db, &dbi, &recs)?;
    if let Some(n) = dbi
        .dbg_streams
        .get(DBG_FPO)
        .copied()
        .filter(|n| *n != 0xffff)
    {
        fpo(&mut a, &db.stream(usize::from(n))?);
    }
    segmap(&mut a, &dbi_bytes, dbi.segmap);
    if let Some(n) = dbi
        .dbg_streams
        .get(DBG_SECTION_HEADER)
        .copied()
        .filter(|n| *n != 0xffff)
    {
        let s = db.stream(usize::from(n))?;
        let want = match ship.img {
            Some(img) => ship_section_table(img)?,
            None => Vec::new(),
        };
        if s.get(..want.len()) == Some(want.as_slice()) && s.len() == want.len() {
            a.tally.ok("section headers stream");
        } else {
            a.tally.fail(
                "section headers stream",
                format!(
                    "stream {n} ({} bytes) is not the shipped section table ({} bytes)",
                    s.len(),
                    want.len()
                ),
            );
        }
    }
    Ok(a.tally)
}

fn contributions(a: &mut Auditor<'_, '_>, dbi: &DbiView, dbi_bytes: &[u8], mods: &[ModRef]) {
    let (sc_at, sc_len) = dbi.contribs;
    if sc_len >= 4 {
        let n = (sc_len - 4) / SC_SIZE;
        let mut prev: Option<(u16, u32)> = None;
        let mut live: Vec<(u16, u32, u32, usize)> = Vec::new();
        for k in 0..n {
            let at = sc_at + 4 + SC_SIZE * k;
            let isect = u16_at(dbi_bytes, at).unwrap_or(0);
            let off = u32_at(dbi_bytes, at + 4).unwrap_or(0);
            let size = u32_at(dbi_bytes, at + 8).unwrap_or(0);
            let imod = usize::from(u16_at(dbi_bytes, at + 16).unwrap_or(0));
            if let Some(p) = a.contrib("section contribution", isect, off, size, mods.get(imod)) {
                live.push((p.0, p.1, p.2, imod));
            }
            let key = (isect, off);
            if prev.is_some_and(|p| p > key) {
                a.tally.fail(
                    "section contributions in order",
                    format!("entry {k} at {isect}:{off:#x} follows {prev:?}"),
                );
            } else {
                a.tally.ok("section contributions in order");
            }
            prev = Some(key);
        }
        live.sort_unstable();
        for pair in live.windows(2) {
            if let [(s0, o0, z0, m0), (s1, o1, _, m1)] = pair {
                if s0 == s1 && o0.saturating_add(*z0) > *o1 && m0 != m1 {
                    let obj = |m: usize| mods.get(m).map_or("?", |r| r.obj.as_str());
                    let why = format!(
                        "{s0}:{o0:#x}+{z0:#x} (module {m0} {}) overlaps {s1}:{o1:#x} (module {m1} {})",
                        obj(*m0),
                        obj(*m1)
                    );
                    if o0 != o1 {
                        a.tally.fail(
                            "section contributions disjoint",
                            format!("{why}: a PARTIAL overlap, which no fold makes"),
                        );
                    } else if a.ship.may_fold {
                        a.tally.folded("section contributions disjoint", why);
                    } else {
                        a.tally.fail(
                            "section contributions disjoint",
                            format!("{why}, on an INCREMENTAL link that folds nothing"),
                        );
                    }
                } else {
                    a.tally.ok("section contributions disjoint");
                }
            }
        }
    }
    for (m, r) in dbi.mods.iter().zip(mods) {
        let isect = u16_at(dbi_bytes, m.sc_at).unwrap_or(0);
        let off = u32_at(dbi_bytes, m.sc_at + 4).unwrap_or(0);
        let size = u32_at(dbi_bytes, m.sc_at + 8).unwrap_or(0);
        if isect == 0xffff {
            continue;
        }
        a.contrib("module record contribution", isect, off, size, Some(r));
    }
}

pub(super) fn structure(t: &mut Tally, db: &Msf2, w: &mut dyn Write) -> Result<()> {
    let s = crate::structure::check(db.bytes());
    writeln!(w, "  structure (its own reader): {}", s.summary())?;
    for (class, ok, fails) in s.rows() {
        t.ok_n(class, ok);
        for f in fails {
            t.fail(class, f.clone());
        }
    }
    Ok(())
}

fn hygiene(
    a: &mut Auditor<'_, '_>,
    db: &Msf2,
    dbi: &DbiView,
    dbi_bytes: &[u8],
    info: &crate::info::Info,
    original: Option<&Msf2>,
    (host, host_paths): (&[String], &[String]),
) -> Result<()> {
    let file = db.bytes();
    let lower: Vec<u8> = file.iter().map(u8::to_ascii_lowercase).collect();
    let mut found = 0usize;
    let mut hits: BTreeMap<usize, bool> = BTreeMap::new();
    for needle in path_needles(host_paths) {
        for (at, wide) in find_text(&lower, &needle) {
            hits.entry(at).or_insert(wide);
        }
    }
    for (at, wide) in hits {
        found += 1;
        a.tally.fail(
            "no host path",
            format!(
                "file offset {at:#x}: `{}`{} -- a path of the machine that built it",
                shown(file, at, wide),
                if wide { " (UTF-16)" } else { "" }
            ),
        );
    }
    if found == 0 {
        a.tally.ok("no host path");
    }
    let mut profiles = 0usize;
    for (at, wide, name) in user_profiles(&lower) {
        profiles += 1;
        if name == "wine" {
            a.tally.ok("no user profile but wine's");
        } else {
            a.tally.fail(
                "no user profile but wine's",
                format!(
                    "file offset {at:#x}: `{}` names the user profile `{name}` -- only the wine prefix's own `wine` names nobody",
                    shown(file, at.saturating_sub(if wide { 2 } else { 1 }), wide)
                ),
            );
        }
    }
    if profiles == 0 {
        a.tally.ok("no user profile but wine's");
    }
    let mut named = 0usize;
    for name in host {
        for (at, wide) in find_text(&lower, name)
            .into_iter()
            .filter(|(at, wide)| whole_token(&lower, *at, name.len(), *wide))
        {
            named += 1;
            a.tally.fail(
                "no login or host name",
                format!(
                    "file offset {at:#x}: `{}` holds `{name}`{}, the name of the machine or the user that ran this",
                    shown(file, at, wide),
                    if wide { " (UTF-16)" } else { "" }
                ),
            );
        }
    }
    if named == 0 {
        a.tally.ok("no login or host name");
    }
    let mut pages: Vec<usize> = Vec::new();
    let mut dirty = 0usize;
    for (what, from, to) in db.unheld()? {
        let Some(at) = file
            .get(from..to)
            .and_then(|r| r.iter().position(|c| *c != 0))
        else {
            continue;
        };
        dirty += 1;
        if let Unheld::Page(p) = what {
            pages.push(p);
        } else {
            a.tally.fail(
                "nothing outside the streams",
                format!(
                    "{what} holds data at file offset {:#x} ({:#04x}) -- a stale pointer or copy of the process that wrote it",
                    from + at,
                    file.get(from + at).copied().unwrap_or(0)
                ),
            );
        }
    }
    if let Some(p) = pages.first() {
        a.tally.fail(
            "nothing outside the streams",
            format!(
                "{} page(s) no stream uses hold data, first page {p} (file offset {:#x}) -- a stale copy MSPDB left",
                pages.len(),
                p * db.page_size
            ),
        );
    }
    if dirty == 0 {
        a.tally.ok("nothing outside the streams");
    }
    if dbi.tsm.1 > 0 {
        match type_servers(dbi_bytes, dbi.tsm) {
            Err(e) => a.tally.fail("type-server map parses", format!("{e:#}")),
            Ok(servers) => {
                a.tally.ok("type-server map parses");
                for s in servers {
                    if (s.sig, s.age) == (info.signature, info.age) || (s.sig, s.age) == (0, 0) {
                        a.tally.ok("type-server map names this file or none");
                    } else {
                        a.tally.fail(
                            "type-server map names this file or none",
                            format!(
                                "the entry at DBI +{:#x} ({}) carries signature {:#010x} age {}: neither this file's ({:#010x} age {}) nor zero",
                                s.at, s.pdb, s.sig, s.age, info.signature, info.age
                            ),
                        );
                    }
                }
            }
        }
    }
    if let Some(o) = original {
        let was = crate::info::read_info(&o.stream(1)?)?.signature;
        if was != info.signature {
            let pat = was.to_le_bytes();
            match file.windows(4).position(|x| x == pat) {
                None => a.tally.ok("our link's signature replaced everywhere"),
                Some(k) => a.tally.fail(
                    "our link's signature replaced everywhere",
                    format!(
                        "file offset {k:#x} still holds {was:#010x}, the signature our link's clock gave it"
                    ),
                ),
            }
        }
    }
    Ok(())
}

pub(super) fn whole_token(lower: &[u8], at: usize, len: usize, wide: bool) -> bool {
    let step = if wide { 2 } else { 1 };
    let word = |i: Option<usize>| {
        i.and_then(|i| lower.get(i))
            .is_some_and(|c| c.is_ascii_alphanumeric() || *c == b'_')
    };
    !word(at.checked_sub(step)) && !word(Some(at + len * step))
}

pub(super) fn find_text(lower: &[u8], needle: &str) -> Vec<(usize, bool)> {
    let narrow = needle.as_bytes().to_vec();
    let wide: Vec<u8> = narrow.iter().flat_map(|c| [*c, 0]).collect();
    let mut out = Vec::new();
    for (pat, is_wide) in [(narrow, false), (wide, true)] {
        if pat.is_empty() {
            continue;
        }
        let finder = memchr::memmem::Finder::new(&pat);
        let mut from = 0usize;
        while let Some(k) = lower.get(from..).and_then(|h| finder.find(h)) {
            out.push((from + k, is_wide));
            from += k + pat.len();
        }
    }
    out
}

pub(super) fn user_profiles(lower: &[u8]) -> Vec<(usize, bool, String)> {
    let mut out = Vec::new();
    for needle in [":\\users\\", "\\documents and settings\\"] {
        for (at, wide) in find_text(lower, needle) {
            let step = if wide { 2 } else { 1 };
            let name: String = lower
                .get(at + needle.len() * step..)
                .unwrap_or_default()
                .iter()
                .step_by(step)
                .take_while(|c| **c > 0x20 && **c < 0x7f && **c != b'\\' && **c != b'/')
                .map(|c| char::from(*c))
                .collect();
            out.push((at, wide, name));
        }
    }
    out
}

pub(super) fn shown(file: &[u8], at: usize, wide: bool) -> String {
    file.get(at..(at + 144).min(file.len()))
        .unwrap_or_default()
        .iter()
        .step_by(if wide { 2 } else { 1 })
        .take(72)
        .take_while(|c| **c >= 0x20 && **c < 0x7f)
        .map(|c| char::from(*c))
        .collect()
}

#[must_use]
pub fn path_needles(host_paths: &[String]) -> Vec<String> {
    let mut out: Vec<String> = HOST_PATHS
        .iter()
        .map(ToString::to_string)
        .chain(host_paths.iter().map(|p| p.to_ascii_lowercase()))
        .collect();
    out.sort();
    out.dedup();
    out
}

#[must_use]
pub fn path_roots(dirs: &[&Path]) -> Vec<String> {
    let mut out: Vec<String> = dirs
        .iter()
        .filter(|d| d.has_root())
        .filter_map(|d| {
            d.components().find_map(|c| match c {
                std::path::Component::Normal(n) => Some(n.to_string_lossy().to_ascii_lowercase()),
                _ => None,
            })
        })
        .filter(|n| n.is_ascii() && !n.is_empty())
        .map(|n| format!("/{n}/"))
        .collect();
    out.sort();
    out.dedup();
    out
}

#[must_use]
pub fn host_paths() -> Vec<String> {
    if !cfg!(unix) {
        return Vec::new();
    }
    let mut dirs: Vec<std::path::PathBuf> = vec![std::env::temp_dir()];
    if let Some(h) = std::env::var_os("HOME") {
        dirs.push(h.into());
    }
    if let Ok(c) = std::env::current_dir() {
        dirs.push(c);
    }
    let refs: Vec<&Path> = dirs.iter().map(std::path::PathBuf::as_path).collect();
    path_roots(&refs)
}

#[must_use]
pub fn host_names() -> Vec<String> {
    let mut v: Vec<String> = Vec::new();
    for k in ["USER", "LOGNAME"] {
        if let Ok(s) = std::env::var(k) {
            v.push(s);
        }
    }
    if let Some(h) = std::env::var_os("HOME")
        && let Some(n) = Path::new(&h).file_name()
    {
        v.push(n.to_string_lossy().into_owned());
    }
    let mut hosts: Vec<String> = Vec::new();
    if let Ok(b) = msvc6_base::srcio::read_bounded("/proc/sys/kernel/hostname", MAX_HOSTNAME) {
        hosts.push(b.iter().map(|c| char::from(*c)).collect());
    }
    if let Ok(s) = std::env::var("HOSTNAME") {
        hosts.push(s);
    }
    for h in hosts {
        let h = h.trim().to_string();
        if let Some(first) = h.split('.').next() {
            v.push(first.to_string());
        }
        v.push(h);
    }
    let mut out: Vec<String> = v
        .into_iter()
        .map(|s| s.trim().to_ascii_lowercase())
        .filter(|s| s.len() >= 3 && s != "wine" && s.is_ascii())
        .collect();
    out.sort();
    out.dedup();
    out
}

fn sources(t: &mut Tally, db: &Msf2, policy: &Policy) -> Result<()> {
    let j = crate::paths::check(db.bytes(), policy)?;
    for (class, (ok, fails)) in &j.rows {
        t.ok_n(class, *ok);
        for f in fails {
            t.fail(class, f.clone());
        }
    }
    Ok(())
}

fn held_to_original(
    a: &mut Auditor<'_, '_>,
    db: &Msf2,
    o: &Msf2,
    dbi: &DbiView,
    dbi_bytes: &[u8],
) -> Result<()> {
    let (file, was) = (db.bytes(), o.bytes());
    let ps = db.page_size;
    let ndir = db.dir_pages()?.len();
    let mut header = vec![(0usize, 56usize), (60, 60 + 2 * ndir)];
    held(
        a,
        "MSF header = --original",
        "the MSF header",
        file,
        was,
        &mut header,
        &[],
    );
    let mut fpm: Vec<(usize, usize)> = db
        .fpm_pages()?
        .into_iter()
        .map(|p| (p * ps, (p + 1) * ps))
        .collect();
    held(
        a,
        "free page maps = --original",
        "a free page map",
        file,
        was,
        &mut fpm,
        &[],
    );
    if db.streams.len() != o.streams.len() {
        a.tally.fail(
            "directory = --original",
            format!(
                "{} streams, --original {}: a relocation adds and drops none",
                db.streams.len(),
                o.streams.len()
            ),
        );
        return Ok(());
    }
    for (k, (now, then)) in db.streams.iter().zip(&o.streams).enumerate() {
        if now == then {
            a.tally.ok("directory = --original");
        } else {
            a.tally.fail(
                "directory = --original",
                format!(
                    "stream {k} is {} bytes on {} page(s), --original {} on {}: a relocation is in place",
                    now.size(),
                    now.pages.len(),
                    then.size(),
                    then.pages.len()
                ),
            );
        }
    }
    let tpi = db.stream(2)?;
    let tpi_hash = u16_at(&tpi, 20).filter(|s| *s != 0xffff).map(usize::from);
    let mut written: BTreeSet<usize> = [1usize, 2, 3].into_iter().collect();
    written.extend(tpi_hash);
    written.insert(usize::from(dbi.symrecs));
    if dbi.publics != 0xffff {
        written.insert(usize::from(dbi.publics));
    }
    for m in &dbi.mods {
        if m.stream != 0xffff {
            written.insert(usize::from(m.stream));
        }
    }
    for k in [DBG_FPO, DBG_SECTION_HEADER] {
        if let Some(n) = dbi.dbg_streams.get(k).copied().filter(|n| *n != 0xffff) {
            written.insert(usize::from(n));
        }
    }
    for k in 0..db.streams.len() {
        if written.contains(&k) {
            continue;
        }
        let what = if k == usize::from(dbi.globals) {
            format!("stream {k} (the globals hash)")
        } else {
            format!("stream {k}")
        };
        same(
            a,
            "stream = --original (never relocated)",
            &what,
            &db.stream(k)?,
            &o.stream(k)?,
            &[],
        );
    }
    same(
        a,
        "stream 1 = --original outside the identity",
        "stream 1",
        &db.stream(1)?,
        &o.stream(1)?,
        &[(4, 12)],
    );
    let odbi = o.stream(3)?;
    let od = DbiView::parse(&odbi)?;
    let mut masks: Vec<(usize, usize)> = vec![(DBI_AGE_AT, DBI_AGE_AT + 4)];
    for m in &od.mods {
        masks.push((m.sc_at, m.sc_at + 2));
        masks.push((m.sc_at + 4, m.sc_at + 12));
    }
    let (sc_at, sc_len) = od.contribs;
    if sc_len > 4 {
        masks.push((sc_at + 4, sc_at + sc_len));
    }
    let (sm_at, sm_len) = od.segmap;
    if sm_len >= 4 {
        let count = usize::from(u16_at(&odbi, sm_at).unwrap_or(0));
        for k in 0..count {
            let rec = sm_at + 4 + SEGMAP_SIZE * k;
            if rec + SEGMAP_SIZE <= sm_at + sm_len && u16_at(&odbi, rec).unwrap_or(0) & 0x0200 == 0
            {
                masks.push((rec + 12, rec + 20));
            }
        }
    }
    if od.tsm.1 > 0 {
        match type_servers(&odbi, od.tsm) {
            Ok(servers) => {
                for s in servers {
                    masks.push((s.at + TS_SIG, s.at + TS_AGE + 4));
                }
            }
            Err(e) => a.tally.fail(
                "type-server map = --original",
                format!("--original's map does not parse: {e:#}"),
            ),
        }
    }
    let ec_end = od.ec.0 + od.ec.1;
    let dbg_end = ec_end + 2 * od.dbg_streams.len();
    let regions = [
        ("DBI header = --original", "the DBI header", 0, 64),
        ("DBI module list = --original", "the module list", 64, sc_at),
        (
            "DBI section contributions = --original",
            "the contributions' version",
            sc_at,
            sc_at + sc_len.min(4),
        ),
        (
            "DBI segment map = --original",
            "the segment map",
            sm_at,
            sm_at + sm_len,
        ),
        (
            "DBI file info = --original",
            "the file info",
            od.file_info.0,
            od.file_info.0 + od.file_info.1,
        ),
        (
            "type-server map = --original",
            "the type-server map",
            od.tsm.0,
            od.tsm.0 + od.tsm.1,
        ),
        (
            "DBI EC info = --original",
            "the edit-and-continue names",
            od.ec.0,
            ec_end,
        ),
        (
            "DBI debug header = --original",
            "the optional debug header",
            ec_end,
            dbg_end,
        ),
        (
            "DBI header = --original",
            "the DBI's tail",
            dbg_end,
            odbi.len().max(dbi_bytes.len()),
        ),
    ];
    for (class, what, from, to) in regions {
        if from >= to {
            continue;
        }
        let mut r = vec![(from, to)];
        held(a, class, what, dbi_bytes, &odbi, &mut r, &masks);
    }
    if dbi.publics != 0xffff {
        let n = usize::from(dbi.publics);
        let (s, os) = (db.stream(n)?, o.stream(n)?);
        let g = |off: usize| {
            u32_at(&os, off)
                .and_then(|v| usize::try_from(v).ok())
                .unwrap_or(0)
        };
        let (hash, map, thunks, sects) = (g(0), g(4), g(8), g(24));
        let thunks_at = 28usize.saturating_add(hash).saturating_add(map);
        let sects_at = thunks_at.saturating_add(thunks.saturating_mul(4));
        let mut m = vec![(28usize.saturating_add(hash), sects_at)];
        if thunks > 0 {
            m.push((20, 24));
        }
        for j in 0..sects.min(os.len() / 8) {
            m.push((sects_at + 8 * j, sects_at + 8 * j + 4));
        }
        same(
            a,
            "publics stream = --original outside its maps",
            "the publics stream",
            &s,
            &os,
            &m,
        );
    }
    for m in &od.mods {
        if m.stream == 0xffff {
            continue;
        }
        let n = usize::from(m.stream);
        let (s, os) = (db.stream(n)?, o.stream(n)?);
        let obj = base_lower(&m.name);
        let lines_end = m.sym_len + m.line_len;
        same(
            a,
            "module stream = --original outside its records and lines",
            &format!("module {obj}'s stream"),
            &s,
            &os,
            &[(4, lines_end.max(4))],
        );
        if m.line_len == 0 {
            continue;
        }
        let (Some(now), Some(then)) = (s.get(m.sym_len..lines_end), os.get(m.sym_len..lines_end))
        else {
            a.tally.fail(
                "C11 file table = --original",
                format!(
                    "module {obj}: its {} bytes of lines run past the stream",
                    m.line_len
                ),
            );
            continue;
        };
        match c11_layout(then) {
            Err(e) => a.tally.fail(
                "C11 file table = --original",
                format!("module {obj}: --original's lines do not parse: {e:#}"),
            ),
            Ok((mutable, names)) => {
                let mut mask = vec![false; then.len().max(now.len())];
                for (x, y) in mutable {
                    for b in mask.iter_mut().take(y).skip(x) {
                        *b = true;
                    }
                }
                let first = (0..now.len().max(then.len())).find(|k| {
                    !mask.get(*k).copied().unwrap_or(false) && now.get(*k) != then.get(*k)
                });
                match first {
                    None => a.tally.ok("C11 file table = --original"),
                    Some(k) => {
                        let why = match names.iter().find(|(x, y, _)| k >= *x && k < *y) {
                            Some((x, _, f)) => renamed_source(&obj, *f, now, then, *x),
                            None => format!(
                                "module {obj}: C11 byte +{k} is {:#04x}, --original {:#04x} -- a count, a table offset or a pad, which no relocation writes",
                                now.get(k).copied().unwrap_or(0),
                                then.get(k).copied().unwrap_or(0)
                            ),
                        };
                        a.tally.fail("C11 file table = --original", why);
                    }
                }
            }
        }
    }
    Ok(())
}

type C11Layout = (Vec<(usize, usize)>, Vec<(usize, usize, usize)>);

pub(super) fn c11_layout(b: &[u8]) -> Result<C11Layout> {
    let w16 = |at: usize| u16_at(b, at).map(usize::from).context("C11 truncated");
    let w32 = |at: usize| {
        u32_at(b, at)
            .and_then(|v| usize::try_from(v).ok())
            .context("C11 truncated")
    };
    let (c_file, c_seg) = (w16(0)?, w16(2)?);
    if c_file.saturating_mul(4) > b.len() {
        anyhow::bail!(
            "C11 file table of {c_file} entries runs past {} bytes",
            b.len()
        );
    }
    let se_at = 4 + 4 * c_file;
    let mut mutable = vec![(se_at, se_at + 10 * c_seg)];
    let mut names = Vec::new();
    for k in 0..c_file {
        let base = w32(4 + 4 * k)?;
        let cc = w16(base)?;
        if cc.saturating_mul(12) > b.len() {
            anyhow::bail!("C11 file record at {base:#x} names {cc} blocks");
        }
        let fse = base + 4 + 4 * cc;
        mutable.push((fse, fse + 8 * cc));
        let name_at = fse + 8 * cc;
        let n = usize::from(*b.get(name_at).context("C11 file name truncated")?);
        names.push((name_at, name_at + 1 + n, k));
        for j in 0..cc {
            let at = w32(base + 4 + 4 * j)?;
            let pairs = w16(at + 2)?;
            mutable.push((at, at + 4 + 6 * pairs));
        }
    }
    Ok((mutable, names))
}

fn latin1(b: &[u8]) -> String {
    b.iter().map(|c| char::from(*c)).collect()
}

pub(super) fn renamed_source(obj: &str, k: usize, now: &[u8], then: &[u8], x: usize) -> String {
    let full = |b: &[u8]| {
        let n = usize::from(b.get(x).copied().unwrap_or(0));
        latin1(
            b.get(x + 1..x + 1 + n)
                .or_else(|| b.get(x + 1..))
                .unwrap_or_default(),
        )
    };
    format!(
        "module {obj}: source file {k}'s name is `{}`, --original `{}` -- a renamed file moves no address, and a \
         debugger shows the wrong source",
        full(now),
        full(then)
    )
}

fn held(
    a: &mut Auditor<'_, '_>,
    class: &str,
    what: &str,
    now: &[u8],
    was: &[u8],
    ranges: &mut [(usize, usize)],
    masks: &[(usize, usize)],
) {
    let printable = |c: Option<&u8>| c.is_some_and(|c| *c >= 0x20 && *c < 0x7f);
    for (from, to) in ranges.iter() {
        let mut masked = vec![false; to.saturating_sub(*from)];
        for (x, y) in masks {
            for m in masked
                .iter_mut()
                .take(y.saturating_sub(*from))
                .skip(x.saturating_sub(*from))
            {
                *m = true;
            }
        }
        let differs =
            |k: usize| !masked.get(k - from).copied().unwrap_or(false) && now.get(k) != was.get(k);
        let mut runs = 0usize;
        let mut k = *from;
        while k < *to {
            if !differs(k) {
                k += 1;
                continue;
            }
            runs += 1;
            let side = if printable(now.get(k)) { now } else { was };
            let start = (*from..=k)
                .rev()
                .take_while(|j| printable(side.get(*j)))
                .last()
                .unwrap_or(k);
            let text = if printable(now.get(k)) || printable(was.get(k)) {
                format!(
                    " -- in `{}`, --original `{}`",
                    shown(now, start, false),
                    shown(was, start, false)
                )
            } else {
                String::new()
            };
            a.tally.fail(
                class,
                format!(
                    "{what}: byte +{} is {:#04x}, --original {:#04x}{text}",
                    k - from,
                    now.get(k).copied().unwrap_or(0),
                    was.get(k).copied().unwrap_or(0)
                ),
            );
            while k < *to && differs(k) {
                k += 1;
            }
        }
        if runs == 0 {
            a.tally.ok(class);
        }
    }
}

fn same(
    a: &mut Auditor<'_, '_>,
    class: &str,
    what: &str,
    now: &[u8],
    was: &[u8],
    masks: &[(usize, usize)],
) {
    if now.len() != was.len() {
        a.tally.fail(
            class,
            format!("{what} is {} bytes, --original {}", now.len(), was.len()),
        );
        return;
    }
    held(a, class, what, now, was, &mut [(0, now.len())], masks);
}

fn ship_section_table(img: &Image) -> Result<Vec<u8>> {
    let at = img.opt_off + usize::from(img.opt_size);
    let len = img.sections.len() * 40;
    Ok(img
        .bytes()
        .get(at..at + len)
        .context("section table runs past the image")?
        .to_vec())
}

fn publics(a: &mut Auditor<'_, '_>, db: &Msf2, dbi: &DbiView, recs: &[u8]) -> Result<()> {
    if dbi.publics == 0xffff {
        return Ok(());
    }
    let s = db.stream(usize::from(dbi.publics))?;
    let h = |o: usize| u32_at(&s, o).context("publics header truncated");
    let hash = usize::try_from(h(0)?)?;
    let map = usize::try_from(h(4)?)?;
    let n_thunks = usize::try_from(h(8)?)?;
    let thunk_size = h(12)?;
    let thunk_sect = u16_at(&s, 16).context("publics header truncated")?;
    let thunk_off = h(20)?;
    let n_sects = usize::try_from(h(24)?)?;
    let map_at = 28usize.saturating_add(hash);
    let starts: BTreeMap<usize, u16> = records(recs).into_iter().map(|r| (r.0, r.1)).collect();
    let mut prev: Option<(u16, u32)> = None;
    let mut seen: BTreeMap<usize, usize> = BTreeMap::new();
    for k in 0..(map / 4).min(s.len() / 4) {
        let Some(pos) = u32_at(&s, map_at + 4 * k).and_then(|v| usize::try_from(v).ok()) else {
            continue;
        };
        if starts.get(&pos) != Some(&0x1009) {
            a.tally.fail(
                "publics address map",
                format!("entry {k} = {pos} is not an S_PUB32 record"),
            );
            continue;
        }
        let key = (
            u16_at(recs, pos + 12).unwrap_or(0),
            u32_at(recs, pos + 8).unwrap_or(0),
        );
        if prev.is_some_and(|p| p > key) {
            a.tally.fail(
                "publics address map",
                format!(
                    "entry {k} at {}:{:#x} follows {:?} -- not ascending (segment, offset)",
                    key.0, key.1, prev
                ),
            );
        } else {
            a.tally.ok("publics address map");
        }
        prev = Some(key);
        *seen.entry(pos).or_default() += 1;
    }
    for (pos, kind) in &starts {
        if *kind != 0x1009 {
            continue;
        }
        match seen.get(pos).copied().unwrap_or(0) {
            1 => a.tally.ok("publics address map is a permutation"),
            n => a.tally.fail(
                "publics address map is a permutation",
                format!(
                    "the S_PUB32 record at {pos} appears {n} time(s) in the address map, not once"
                ),
            ),
        }
    }
    let thunks_at = map_at.saturating_add(map);
    let ilt = |k: usize| -> Option<u32> {
        let sec = a.ship.sec(thunk_sect)?;
        let slot = sec
            .rva
            .checked_add(thunk_off)?
            .checked_add(u32::try_from(k).ok()?.checked_mul(thunk_size)?)?;
        let b = a.ship.bytes(slot, 5)?;
        if b.first() != Some(&0xe9) {
            return None;
        }
        let rel = i32::from_le_bytes([*b.get(1)?, *b.get(2)?, *b.get(3)?, *b.get(4)?]);
        u32::try_from(i64::from(slot) + 5 + i64::from(rel)).ok()
    };
    let mut thunk_rows = Vec::with_capacity(n_thunks.min(s.len() / 4));
    for k in 0..n_thunks.min(s.len() / 4) {
        thunk_rows.push((k, u32_at(&s, thunks_at + 4 * k).unwrap_or(0), ilt(k)));
    }
    for (k, v, want) in thunk_rows {
        match (v, want) {
            (0, None) => a.tally.retired("publics thunk map"),
            (0, Some(x)) => a.tally.fail(
                "publics thunk map",
                format!("slot {k} is zeroed, but the shipped ILT slot jumps to {x:#x}"),
            ),
            (v, Some(x)) if v == x => a.tally.ok("publics thunk map"),
            (v, x) => a.tally.fail(
                "publics thunk map",
                format!(
                    "slot {k} names {v:#x}; the shipped ILT slot at {thunk_sect}:{:#x} jumps to {}",
                    u64::from(thunk_off) + u64::from(thunk_size) * u64::try_from(k).unwrap_or(0),
                    x.map_or_else(
                        || "nothing (not an e9 jmp)".to_string(),
                        |x| format!("{x:#x}")
                    )
                ),
            ),
        }
    }
    let sects_at = thunks_at.saturating_add(n_thunks.saturating_mul(4));
    for j in 0..n_sects.min(s.len() / 8) {
        let off = u32_at(&s, sects_at + 8 * j).unwrap_or(0);
        let isect = u16_at(&s, sects_at + 8 * j + 4).unwrap_or(0);
        match a.ship.sec(isect) {
            Some(sec) if sec.rva == off => a.tally.ok("publics section map"),
            Some(sec) => a.tally.fail(
                "publics section map",
                format!(
                    "entry {j}: section {isect} at {off:#x}, shipped {} is at {:#x}",
                    sec.name, sec.rva
                ),
            ),
            None if isect == 0 && off == 0 => a.tally.retired("publics section map"),
            None => a.tally.fail(
                "publics section map",
                format!("entry {j}: section {isect} is not a shipped section"),
            ),
        }
    }
    Ok(())
}
