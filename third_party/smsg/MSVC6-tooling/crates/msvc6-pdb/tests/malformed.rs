#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_fixture::backstop;
use msvc6_pdb::config::{AttestedDir, Lane, PathsSpec};
use msvc6_pdb::paths::Policy;
use msvc6_pdb::{attach, buildname, eq, msf, msf7, paths, procs, structure, to70};
use std::path::PathBuf;
use std::time::Duration;

const WORK_PER_BYTE: usize = 8;
const WORK_FLOOR: usize = 4096;
const PER_STREAM: usize = 160;

fn work_cap(len: usize) -> usize {
    len.saturating_mul(WORK_PER_BYTE).max(WORK_FLOOR)
}

struct Sweep {
    cases: usize,
    slowest: (Duration, String),
    heaviest: (usize, usize, String),
}

impl Sweep {
    fn new() -> Self {
        Self {
            cases: 0,
            slowest: (Duration::ZERO, String::new()),
            heaviest: (0, 0, String::new()),
        }
    }

    fn timed<T>(&mut self, what: &str, f: impl FnOnce() -> T) -> T {
        let (r, took) = backstop::run(what, f);
        self.cases += 1;
        if took > self.slowest.0 {
            self.slowest = (took, what.to_string());
        }
        r
    }

    fn case(&mut self, policy: &Policy, what: &str, file: &[u8]) -> usize {
        let n = self.timed(what, || every_reader(policy, file));
        if n > self.heaviest.0 {
            self.heaviest = (n, file.len(), what.to_string());
        }
        let cap = work_cap(file.len());
        assert!(
            n <= cap,
            "{what}: the readers returned {n} items from {} bytes, past {cap}",
            file.len()
        );
        n
    }

    fn report(&self, test: &str) {
        eprintln!(
            "{test}: {} case(s); the slowest {:?} ({}); the heaviest {} items from {} bytes ({})",
            self.cases,
            self.slowest.0,
            self.slowest.1,
            self.heaviest.0,
            self.heaviest.1,
            self.heaviest.2
        );
    }
}

struct Ctx {
    dir: PathBuf,
    policy: Policy,
}

impl Ctx {
    fn new() -> Self {
        static N: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);
        let k = N.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        let dir = std::env::temp_dir().join(format!("msvc6t-malformed-{}-{k}", std::process::id()));
        for f in ["proj/app/a.cpp", "proj/app/a.h", "proj/app/b.cpp"] {
            let at = dir.join(f);
            std::fs::create_dir_all(at.parent().unwrap()).unwrap();
            msvc6_base::srcio::own_write(&at, "int x;\n").unwrap();
        }
        std::fs::create_dir_all(dir.join("proj/app/Release")).unwrap();
        let attested = |product: &str, at: &str, output: bool| AttestedDir {
            product: product.into(),
            dir: at.into(),
            output,
            evidence: "a test".into(),
        };
        let spec = PathsSpec {
            product: String::new(),
            product_win: None,
            configs: vec!["Release".into(), "Debug".into()],
            lanes: vec![Lane {
                at: "C:\\work".into(),
                product: "proj/app".into(),
            }],
            attested: vec![
                attested("proj/app", "C:\\Orig", false),
                attested("proj/app/Release", "C:\\Orig\\Release", true),
            ],
            files: Vec::new(),
        };
        let policy = Policy::new(&spec, &dir, std::collections::BTreeMap::new()).unwrap();
        Self { dir, policy }
    }
}

impl Drop for Ctx {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.dir);
    }
}

fn path_readers(policy: &Policy, file: &[u8]) -> usize {
    let mut n = 0usize;
    let Ok(m) = msf::Msf2::parse(file.to_vec()) else {
        return 0;
    };
    if let Ok(d) = m.stream(3) {
        n += buildname::dbi_paths(&d).map_or(0, |p| p.len());
    }
    for i in 0..m.streams.len() {
        if let Ok(s) = m.stream(i) {
            n += buildname::symbol_paths(s.get(4..).unwrap_or_default()).map_or(0, |p| p.len());
            n += buildname::link_info_paths(&s).map_or(0, |p| p.len());
            n += paths::c11_names(&s).map_or(0, |p| p.len());
        }
    }
    n += paths::check(file, policy).map_or(0, |j| j.failures());
    let mut db = m;
    n += paths::rename(&mut db, policy).map_or(0, |s| s.refs);
    n
}

fn every_reader(policy: &Policy, file: &[u8]) -> usize {
    let mut n = 0usize;
    if msf7::is_pdb70(file) {
        if let Ok(m) = msf7::Msf7::parse(file.to_vec()) {
            for i in 0..m.streams.len() {
                n += m.stream(i).map_or(0, |s| s.len());
            }
            n += m.used_but_free().map_or(0, |u| u.0);
        }
        n += eq::read(file).map_or(0, |c| eq::counts(&c).procs);
    } else {
        if let Ok(m) = msf::Msf2::parse(file.to_vec()) {
            for i in 0..m.streams.len() {
                n += m.stream(i).map_or(0, |s| s.len());
            }
            n += m.unheld().map_or(0, |u| u.len());
        }
        n += structure::check(file).failures();
        n += path_readers(policy, file);
        match to70::convert(file) {
            Ok(c) => {
                n += c.bytes.len();
                n += eq::equivalence(file, &c.bytes).map_or(0, |v| v.differences.len());
            }
            Err(_) => n += eq::read(file).map_or(0, |c| eq::counts(&c).procs),
        }
    }
    n += procs::read(file).map_or(0, |l| l.procs.len() + l.names.len());
    n += attach::pdb_id(file).map_or(0, |i| i.sections.len());
    n
}

fn positions(file: &[u8], step: usize) -> Vec<usize> {
    let mut out: Vec<usize> = (0..file.len().min(96)).step_by(step).collect();
    if msf::is_pdb20(file)
        && let Ok(m) = msf::Msf2::parse(file.to_vec())
    {
        let ps = m.page_size;
        for p in m.dir_pages().unwrap_or_default() {
            out.extend((p * ps..(p + 1) * ps).step_by(step));
        }
        for s in &m.streams {
            let mut at: Vec<usize> = Vec::new();
            for (k, p) in s.pages.iter().enumerate() {
                let len = s.size().saturating_sub(k * ps).min(ps);
                at.extend((p * ps..p * ps + len).step_by(step));
            }
            let every = at.len().div_ceil(PER_STREAM).max(1);
            out.extend(at.into_iter().step_by(every));
        }
    }
    if msf7::is_pdb70(file)
        && let Ok(m) = msf7::Msf7::parse(file.to_vec())
    {
        let bs = m.block_size;
        for b in m.dir_blocks.iter().chain([&m.map_block]) {
            out.extend((b * bs..(b + 1) * bs).step_by(step).take(256));
        }
        for s in &m.streams {
            let mut at: Vec<usize> = Vec::new();
            for (k, b) in s.blocks.iter().enumerate() {
                let len = s.len.unwrap_or(0).saturating_sub(k * bs).min(bs);
                at.extend((b * bs..b * bs + len).step_by(step));
            }
            let every = at.len().div_ceil(PER_STREAM).max(1);
            out.extend(at.into_iter().step_by(every));
        }
    }
    out.sort_unstable();
    out.dedup();
    out
}

fn files() -> Vec<(&'static str, Vec<u8>)> {
    let linked = msvc6_fixture::pdb::sample().bytes;
    let twin = to70::convert(&linked).unwrap().bytes;
    vec![
        ("linked 2.0", linked),
        ("linked 7.0", twin),
        ("typed 2.0", common::typed().0),
    ]
}

#[test]
fn every_word_of_the_fixture_pdbs_set_to_an_edge_value_is_read_or_refused_within_its_bounds() {
    let cx = Ctx::new();
    let mut sw = Sweep::new();
    for (name, base) in files() {
        assert!(sw.case(&cx.policy, name, &base) > 0, "{name}");
        for at in positions(&base, 2) {
            for v in [0u16, 0xffff, 0x8000] {
                let mut b = base.clone();
                if let Some(w) = b.get_mut(at..at + 2) {
                    w.copy_from_slice(&v.to_le_bytes());
                }
                sw.case(&cx.policy, &format!("{name}: u16 {v:#x} at {at:#x}"), &b);
            }
        }
        for at in positions(&base, 4) {
            for v in [
                u32::MAX,
                0x7fff_ffff,
                0x1000,
                u32::try_from(base.len()).unwrap(),
                u32::try_from(base.len()).unwrap() + 1,
            ] {
                let mut b = base.clone();
                if let Some(w) = b.get_mut(at..at + 4) {
                    w.copy_from_slice(&v.to_le_bytes());
                }
                sw.case(&cx.policy, &format!("{name}: u32 {v:#x} at {at:#x}"), &b);
            }
        }
    }
    sw.report("edge values");
}

#[test]
fn every_truncation_of_the_fixture_pdbs_is_read_or_refused_within_its_bounds() {
    let cx = Ctx::new();
    let mut sw = Sweep::new();
    for (name, base) in files() {
        let mut cuts: Vec<usize> = (0..256).collect();
        cuts.extend((256..base.len()).step_by(509));
        for n in cuts {
            sw.case(&cx.policy, &format!("{name}: {n} bytes"), &base[..n]);
            if n < 64 {
                assert!(msf::Msf2::parse(base[..n].to_vec()).is_err());
                assert!(msf7::Msf7::parse(base[..n].to_vec()).is_err());
            }
        }
    }
    sw.report("truncations");
}

fn small(streams: &[Vec<u8>]) -> Vec<u8> {
    msvc6_fixture::pdb::small_msf(streams)
}

#[test]
fn a_directory_that_claims_more_than_the_file_holds_is_refused_without_allocating_it() {
    let cx = Ctx::new();
    let mut sw = Sweep::new();
    let base = msvc6_fixture::pdb::sample().bytes;
    let m = msf::Msf2::parse(base.clone()).unwrap();
    let ps = m.page_size;
    let d = m.dir_pages().unwrap()[0] * ps;
    for (at, v) in [
        (d, 0x0fff_ffffu32),
        (d + 4, 0xffff_fff0),
        (d + 4 + 8, 0x7fff_ffff),
        (52, 0x7fff_ffff),
    ] {
        let mut b = base.clone();
        b[at..at + 4].copy_from_slice(&v.to_le_bytes());
        sw.timed(&format!("{at:#x} = {v:#x}"), || {
            assert!(msf::Msf2::parse(b.clone()).is_err(), "{at:#x} = {v:#x}");
            assert!(structure::check(&b).failures() > 0);
            assert!(to70::convert(&b).is_err());
        });
    }
    let mut all_one = base.clone();
    let count = u32::from_le_bytes(all_one[d..d + 4].try_into().unwrap());
    let lists = d + 4 + 8 * usize::try_from(count).unwrap();
    let first = all_one[lists..lists + 2].to_vec();
    let end = d + usize::try_from(u32::from_le_bytes(all_one[52..56].try_into().unwrap())).unwrap();
    for k in (lists..end).step_by(2) {
        all_one[k..k + 2].copy_from_slice(&first);
    }
    sw.case(&cx.policy, "every stream on one page", &all_one);
    sw.report("directories");
}

#[test]
fn hostile_streams_inside_a_valid_container_are_read_or_refused_within_their_bounds() {
    let cx = Ctx::new();
    let mut sw = Sweep::new();
    let base = msvc6_fixture::pdb::sample();
    let m = msf::Msf2::parse(base.bytes.clone()).unwrap();
    let mut streams: Vec<Vec<u8>> = (0..m.streams.len()).map(|i| m.stream(i).unwrap()).collect();
    let module = streams[11].clone();
    let mut c11_bomb = module.clone();
    let dbi = msvc6_pdb::dbi::Dbi::parse(&streams[3]).unwrap();
    let sym = dbi.mods[0].sym_len().unwrap();
    c11_bomb[sym..sym + 2].copy_from_slice(&0xffffu16.to_le_bytes());
    for k in 0..64 {
        let at = sym + 4 + 4 * k;
        if at + 4 <= c11_bomb.len() {
            c11_bomb[at..at + 4].copy_from_slice(&4u32.to_le_bytes());
        }
    }
    let mut big_strtab = vec![0u8; 0];
    big_strtab.extend_from_slice(&0xeffe_effeu32.to_le_bytes());
    big_strtab.extend_from_slice(&1u32.to_le_bytes());
    let names: Vec<u8> = (0..60_000u32)
        .flat_map(|i| {
            let mut v = format!("n{i}").into_bytes();
            v.push(0);
            v
        })
        .collect();
    let mut buf = vec![0u8];
    buf.extend_from_slice(&names);
    big_strtab.extend_from_slice(&u32::try_from(buf.len()).unwrap().to_le_bytes());
    big_strtab.extend_from_slice(&buf);
    let nb = 70_000usize;
    big_strtab.extend_from_slice(&u32::try_from(nb).unwrap().to_le_bytes());
    for i in 0..nb {
        big_strtab.extend_from_slice(&u32::from(i % 7 == 0).to_le_bytes());
    }
    big_strtab.extend_from_slice(&1u32.to_le_bytes());
    for (i, bad) in [(11usize, c11_bomb), (10, big_strtab)] {
        let mut s = streams.clone();
        s[i] = bad;
        let file = small(&s);
        sw.case(&cx.policy, &format!("stream {i}"), &file);
    }
    let mut two = streams.clone();
    let mut d = two[3].clone();
    let modi = usize::try_from(u32::from_le_bytes(d[24..28].try_into().unwrap())).unwrap();
    let mut more = d[..64].to_vec();
    for _ in 0..200 {
        more.extend_from_slice(&d[64..64 + modi]);
    }
    more.extend_from_slice(&d[64 + modi..]);
    more[24..28].copy_from_slice(&u32::try_from(200 * modi).unwrap().to_le_bytes());
    d = more;
    two[3] = d;
    let file = small(&two);
    sw.timed("200 module records", || {
        assert!(structure::check(&file).failures() > 0);
        assert!(to70::convert(&file).is_err());
        assert!(procs::read(&file).is_err());
    });
    streams.truncate(4);
    sw.case(&cx.policy, "four streams", &small(&streams));
    sw.report("hostile streams");
}

#[test]
fn a_7_0_globals_hash_with_no_records_and_a_bucket_bitmap_is_refused() {
    let cx = Ctx::new();
    let mut sw = Sweep::new();
    let twin = to70::convert(&msvc6_fixture::pdb::sample().bytes)
        .unwrap()
        .bytes;
    let m = msf7::Msf7::parse(twin).unwrap();
    let mut streams: Vec<Vec<u8>> = (0..m.streams.len()).map(|i| m.stream(i).unwrap()).collect();
    let again = msf7::write(&streams).unwrap();
    assert!(eq::read(&again).is_ok());
    let dbi = msvc6_pdb::dbi::Dbi::parse(&streams[3]).unwrap();
    let g = usize::from(dbi.globals().unwrap());
    let words = msvc6_pdb::gsi::BITMAP_WORDS;
    for bits in [0u32, 1] {
        let mut hash = msvc6_pdb::gsi::write70(&[]).unwrap();
        hash[12..16].copy_from_slice(&u32::try_from(4 * words).unwrap().to_le_bytes());
        hash.extend_from_slice(&bits.to_le_bytes());
        hash.resize(16 + 4 * words, 0);
        streams[g] = hash;
        let file = msf7::write(&streams).unwrap();
        let e = eq::read(&file).unwrap_err();
        assert!(format!("{e:#}").contains("no records"), "{e:#}");
        sw.case(
            &cx.policy,
            &format!("7.0 globals: no records, a bucket bitmap of word {bits:#x}"),
            &file,
        );
    }
    sw.report("an empty GSI with buckets");
}
