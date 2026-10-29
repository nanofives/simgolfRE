#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_diff::env::Env;
use msvc6_diff::seat;
use msvc6_gate::{account, addrsym, calltargets, globals, jumps, libs, literals, relocid};
use msvc6_image::pe::Image;
use msvc6_project::mapfile::{MAX_MAP, Map, Owners};
use std::path::{Path, PathBuf};

mod common;
use common::*;

fn watched<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
}

#[derive(Debug, Default)]
struct Work {
    relocid_rows: usize,
    relocid_seated: usize,
    call_sites: usize,
    jump_functions: usize,
    jump_findings: usize,
    literal_sites: usize,
    refused: usize,
}

fn every_gate(env: &Env, objs: &[PathBuf]) -> Work {
    let mut w = Work::default();
    let seats = seat::seat_map(env, objs, 1);
    w.refused += seats.refused.len();
    let r = relocid::measure(&relocid::Inputs {
        env,
        objs,
        seats: &seats.seats,
        archives: &[],
        notes_only: &[],
        only: None,
        jobs: 1,
    });
    w.relocid_rows = r.rows.len();
    w.relocid_seated = r.seated;
    match calltargets::measure(&calltargets::Inputs {
        env,
        objs,
        census: true,
        map_data: &Default::default(),
    }) {
        Ok(c) => {
            w.call_sites = c.checked + c.uncovered.values().sum::<usize>();
            w.refused += c.refused.len();
            let _ = calltargets::causes(env, &c);
        }
        Err(_) => w.refused += 1,
    }
    let j = jumps::run_all(env, objs, &seats.seats, &jumps::Excuses::default(), 1);
    w.jump_functions = j.totals.functions + j.totals.not_checked;
    w.jump_findings = j
        .done
        .iter()
        .filter_map(|(_, _, r)| r.as_ref().ok())
        .map(|d| d.findings.len())
        .sum();
    let bytes = |rva: u32, n: usize| {
        env.img
            .at_rva(msvc6_base::units::Rva(rva), n)
            .map(<[u8]>::to_vec)
    };
    for o in objs {
        let Ok(d) = msvc6_diff::comdat::read(o) else {
            w.refused += 1;
            continue;
        };
        match literals::screen(
            &d,
            BASE,
            env.img.opt.size_of_image,
            &|f| (f == F_NAME).then_some(F),
            &bytes,
        ) {
            Ok(r) => w.literal_sites += r.sites + r.misaligned.len(),
            Err(_) => w.refused += 1,
        }
    }
    w
}

fn checked(what: &str, unit: Vec<u8>) {
    let dir = scratch(&format!("malformed-{}", what.replace([' ', '/'], "-")));
    let objs = objects(&dir, &unit);
    let env = env();
    let len = unit.len();
    let w = watched(what, move || every_gate(&env, &objs));
    std::fs::remove_dir_all(&dir).ok();
    let fields = len / 10 + 4;
    assert!(w.relocid_seated <= 3, "{what}: {w:?}");
    assert!(w.relocid_rows <= fields + len / 4, "{what}: {w:?}");
    assert!(w.call_sites <= fields, "{what}: {w:?}");
    assert!(w.jump_functions <= 3, "{what}: {w:?}");
    assert!(w.jump_findings <= len, "{what}: {w:?}");
    assert!(w.literal_sites <= fields, "{what}: {w:?}");
}

#[test]
fn every_cut_of_an_object_is_refused_or_read_within_its_bytes() {
    let base = caller("_b", "_wrong");
    for cut in 0..base.len() {
        checked(&format!("cut {cut}"), base[..cut].to_vec());
    }
}

#[test]
fn every_word_of_an_object_set_to_an_edge_value_is_refused_or_read_within_its_bytes() {
    let base = caller("_b", "_wrong");
    let len = u32::try_from(base.len()).unwrap();
    for at in (0..base.len().saturating_sub(3)).step_by(2) {
        for v in [
            0u32,
            1,
            0x7fff_ffff,
            0xffff_ffff,
            len,
            len.saturating_sub(1),
        ] {
            let mut b = base.clone();
            b[at..at + 4].copy_from_slice(&v.to_le_bytes());
            checked(&format!("word {at:#x}={v:#x}"), b);
        }
    }
}

fn map_header() -> String {
    format!(
        " ours\r\n\r\n Preferred load address is {BASE:08x}\r\n\r\n\
         \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n"
    )
}

#[test]
fn a_hostile_map_is_read_within_its_lines_and_an_oversized_one_is_refused() {
    let mut text = map_header();
    let rows = 200_000usize;
    for k in 0..rows {
        let v = BASE.wrapping_add(u32::try_from(k).unwrap().wrapping_mul(0x101));
        match k % 5 {
            0 => text.push_str(&format!(" 0001:{k:08x}  _f{k}  {v:08x} f   A.obj\r\n")),
            1 => text.push_str(&format!(" 0002:{k:08x}  _d{k}  {v:08x}     L:{k}.obj\r\n")),
            2 => text.push_str(&format!(" ffff:ffffffff  ?  ffffffff q z   {k}\r\n")),
            3 => text.push_str(" 0003:zz  _x  1234 \r\n"),
            _ => text.push_str(&"x".repeat(k % 4000)),
        }
    }
    let lines = text.lines().count();
    let ours = image(A, RIGHT);
    let got = watched("hostile map", move || {
        let m = Map::parse(Path::new("h.map"), &text).unwrap();
        let linked = addrsym::Linked::new(&ours, &m);
        let a = account::map_symbols(&m, &Owners::new(&[]));
        let g = globals::map_code_symbols(&m);
        (m.symbols.len(), linked.rows.len(), a.len(), g.len())
    });
    assert!(
        got.0 <= lines && got.1 <= got.0 && got.2 <= got.0 && got.3 <= got.0,
        "{got:?}"
    );
    assert!(got.0 >= rows / 5, "{got:?}");
    let dir = scratch("bigmap");
    let p = dir.join("big.map");
    std::fs::File::create(&p)
        .unwrap()
        .set_len(MAX_MAP + 1)
        .unwrap();
    let e = Map::read(&p).unwrap_err();
    assert!(format!("{e:#}").contains("more than"), "{e:#}");
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn an_excuse_file_too_big_or_with_an_unknown_key_is_refused() {
    let dir = scratch("excuses");
    let p = dir.join("big.toml");
    std::fs::File::create(&p)
        .unwrap()
        .set_len(jumps::MAX_EXCUSES + 1)
        .unwrap();
    let e = jumps::read_excuses(&p).unwrap_err();
    assert!(format!("{e:#}").contains("more than"), "{e:#}");
    let q = dir.join("bad.toml");
    srcio::own_write(
        &q,
        "[[named]]\nimage = \"A\"\nrva = 1\nours = 2\nwhy = \"w\"\nmore = 3\n",
    )
    .unwrap();
    assert!(jumps::read_excuses(&q).is_err());
    let r = dir.join("junk.toml");
    srcio::write_bytes(&r, &[0xff, 0xfe, 0x00, b'[', b'[']).unwrap();
    assert!(jumps::read_excuses(&r).is_err());
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn a_corrupt_or_oversized_archive_is_refused_and_never_read_past() {
    let dir = scratch("archives");
    let junk = dir.join("junk.lib");
    srcio::write_bytes(&junk, b"!<arch>\nnot a member header at all").unwrap();
    let big = dir.join("big.lib");
    std::fs::File::create(&big)
        .unwrap()
        .set_len(libs::MAX_ARCHIVE + 1)
        .unwrap();
    let gone = dir.join("gone.lib");
    let names: std::collections::BTreeSet<String> = ["_strlen".to_string()].into_iter().collect();
    let paths = vec![junk, big, gone];
    let l = watched("archives", move || libs::load(&paths, &names));
    assert_eq!(l.refused.len(), 3, "{:?}", l.refused);
    assert!(l.members.is_empty());
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn an_image_cut_at_every_length_is_refused_or_accounted_within_its_bytes() {
    let whole = {
        let mut pe = msvc6_fixture::pe::Pe::exe();
        pe.push(".text", msvc6_fixture::pe::SCN_CODE, linked_text(B, WRONG));
        pe.push(
            ".data",
            msvc6_fixture::pe::SCN_DATA,
            vec![0x11, 0, 0, 0, 0x23, 0, 0, 0],
        );
        pe.build()
    };
    let map = Map::parse(Path::new("ours.map"), &map_text()).unwrap();
    for cut in (0..whole.len()).step_by(7) {
        let bytes = whole[..cut].to_vec();
        let env = env();
        let m = map.clone();
        let what = format!("image cut {cut}");
        let got = watched(&what, move || {
            let Ok(cand) = Image::parse(bytes) else {
                return None;
            };
            let acc = account::measure(&env.img, &cand, &m, &Owners::new(&[])).ok()?;
            let g = globals::measure(&env, &cand, &m).ok();
            Some((
                acc.totals().differing,
                acc.unseen(),
                g.map(|g| g.pairs.len()),
            ))
        });
        if let Some((differing, unseen, pairs)) = got {
            assert!(differing <= whole.len().max(cut) * 2, "{what}: {differing}");
            assert!(unseen <= whole.len(), "{what}: {unseen}");
            assert!(pairs.unwrap_or(0) <= whole.len() / 4, "{what}: {pairs:?}");
        }
    }
}

#[test]
fn an_image_whose_section_runs_past_the_file_is_refused_by_the_account() {
    let map = Map::parse(Path::new("ours.map"), &map_text()).unwrap();
    let whole = {
        let mut pe = msvc6_fixture::pe::Pe::exe();
        pe.push(".text", msvc6_fixture::pe::SCN_CODE, linked_text(B, WRONG));
        pe.push(
            ".data",
            msvc6_fixture::pe::SCN_DATA,
            vec![0x11, 0, 0, 0, 0x23, 0, 0, 0],
        );
        pe.build()
    };
    let env = env();
    let full = Image::parse(whole.clone()).unwrap();
    assert!(account::section_past_the_file(&full).is_none());
    assert!(account::measure(&env.img, &full, &map, &Owners::new(&[])).is_ok());
    let data = full.section(".data").unwrap();
    let cut = data.raw_off.get() + 4;
    let short = Image::parse(whole[..cut].to_vec()).unwrap();
    assert_eq!(
        account::section_past_the_file(&short).map(|s| s.0),
        Some(".data".to_string())
    );
    let e = account::measure(&env.img, &short, &map, &Owners::new(&[]))
        .unwrap_err()
        .to_string();
    assert!(
        e.contains("the candidate image's .data holds raw data at")
            && e.contains("past the file's"),
        "{e}"
    );
    let e = account::measure(&short, &full, &map, &Owners::new(&[]))
        .unwrap_err()
        .to_string();
    assert!(e.contains("the reference image's .data"), "{e}");
}

#[test]
fn a_map_archive_or_excuse_file_that_is_no_regular_file_is_refused_before_it_blocks() {
    let dir = scratch("fifo");
    let mut paths = Vec::new();
    for name in ["ours.map", "x.lib", "excuses.toml"] {
        let p = dir.join(name);
        let made = std::process::Command::new("mkfifo")
            .arg(&p)
            .status()
            .is_ok_and(|s| s.success());
        if made {
            paths.push(p);
        }
    }
    for name in ["zero.map", "zero.lib", "zero.toml"] {
        let p = dir.join(name);
        std::os::unix::fs::symlink("/dev/zero", &p).unwrap();
        paths.push(p);
    }
    for p in paths {
        let what = format!("{}", p.display());
        let q = p.clone();
        let e = watched(&what, move || {
            let ext = q.extension().and_then(|e| e.to_str()).unwrap_or_default();
            match ext {
                "map" => Map::read(&q).err().map(|e| format!("{e:#}")),
                "lib" => {
                    let names: std::collections::BTreeSet<String> =
                        ["_f".to_string()].into_iter().collect();
                    libs::load(std::slice::from_ref(&q), &names)
                        .refused
                        .first()
                        .map(|r| r.1.clone())
                }
                _ => libs::read_bounded(&q, jumps::MAX_EXCUSES)
                    .err()
                    .map(|e| format!("{e:#}")),
            }
        });
        assert!(
            e.as_deref()
                .is_some_and(|e| e.contains("not a regular file")),
            "{what}: {e:?}"
        );
    }
    std::fs::remove_dir_all(&dir).ok();
}
