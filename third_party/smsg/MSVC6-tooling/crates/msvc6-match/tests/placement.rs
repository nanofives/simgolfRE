#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_diff::env::Env;
use msvc6_diff::resolve::Names;
use msvc6_fixture::coff::{
    Object, REL_DIR32, Reloc, SCN_BSS, SCN_COMDAT, SCN_DATA, SCN_TEXT, SELECT_NODUPLICATES, Section,
};
use msvc6_fixture::pe::{DIR_BASERELOC, Pe, SCN_CODE, SCN_DATA as PE_DATA, SCN_RELOC};
use msvc6_fixture::tables::base_relocs;
use msvc6_image::pe::Image;
use msvc6_match::{bodies, dataowner, dataruns, dataseat, seated};
use msvc6_project::mapfile::Map;
use msvc6_project::names::{Kind, Name, NameOracle};
use std::path::{Path, PathBuf};

const BASE: u32 = 0x0040_0000;
const F: u32 = 0x1000;
const DATA: u32 = 0x2000;
const BSS: u32 = DATA + 0x10;

fn watched<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
}

fn f_body(op: u8) -> Vec<u8> {
    vec![op, 0, 0, 0, 0, 0x8b, 0x0d, 0, 0, 0, 0, 0xc3]
}

const DATA_BYTES: [u8; 8] = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88];

fn image(op: u8) -> Image {
    let mut pe = Pe::exe();
    let mut f = f_body(op);
    f[1..5].copy_from_slice(&(BASE + DATA + 4).to_le_bytes());
    f[7..11].copy_from_slice(&(BASE + BSS).to_le_bytes());
    let mut t = vec![0xcc; 0x20];
    t[..f.len()].copy_from_slice(&f);
    assert_eq!(pe.push(".text", SCN_CODE, t), F);
    let mut d = DATA_BYTES.to_vec();
    d.resize(0x10, 0);
    assert_eq!(pe.push_virtual(".data", PE_DATA, d, 0x40), DATA);
    let rl = base_relocs(&[F + 1, F + 7], 3);
    let at = pe.push(".reloc", SCN_RELOC, rl.clone());
    pe.dir(DIR_BASERELOC, at, u32::try_from(rl.len()).unwrap());
    Image::parse(pe.build()).unwrap()
}

fn env(op: u8) -> Env {
    let mut o = NameOracle::default();
    o.add(
        "tsv",
        "names.tsv",
        vec![Name {
            rva: F,
            name: "_f".into(),
            kind: Kind::Label,
            size: None,
            scope: None,
            provider: "tsv".into(),
        }],
        Vec::new(),
    );
    Env::new(
        "FIX",
        image(op),
        Names::from_oracle(&o),
        vec![(F, 12)],
        Vec::new(),
    )
    .unwrap()
}

fn object() -> Vec<u8> {
    let mut o = Object::new();
    let t = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: f_body(0xa1),
        ..Section::default()
    });
    let d = o.section(Section {
        name: ".data".into(),
        characteristics: SCN_DATA,
        data: DATA_BYTES.to_vec(),
        ..Section::default()
    });
    let b = o.section(Section {
        name: ".bss".into(),
        characteristics: SCN_BSS,
        uninit: 8,
        ..Section::default()
    });
    o.section_symbol(t, SELECT_NODUPLICATES, 0);
    o.external("_f", 0, t);
    let v = o.external("_v", 4, d);
    let w = o.external("_w", 0, b);
    o.sections[0].relocs = vec![
        Reloc {
            va: 1,
            symbol: v,
            typ: REL_DIR32,
        },
        Reloc {
            va: 7,
            symbol: w,
            typ: REL_DIR32,
        },
    ];
    o.build()
}

fn scratch(tag: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!(
        "msvc6-match-placement-{tag}-{}",
        std::process::id()
    ));
    std::fs::remove_dir_all(&d).ok();
    std::fs::create_dir_all(&d).unwrap();
    d
}

fn write(dir: &Path, bytes: &[u8]) -> Vec<PathBuf> {
    let p = dir.join("Unit.obj");
    srcio::write_bytes(&p, bytes).unwrap();
    vec![p]
}

fn place(_: &str, f: &str) -> Option<u32> {
    (f == "_f").then_some(F)
}

#[test]
fn data_and_bss_are_seated_by_their_relocations_votes_and_placed_by_points() {
    let dir = scratch("seat");
    let objs = write(&dir, &object());
    let env = env(0xa1);
    let all = dataseat::seat_all(&env, &objs, dataseat::CONTENT_BUDGET);
    let r = all.first().unwrap().1.as_ref().unwrap();
    let data = r.sections.iter().find(|s| s.name == ".data").unwrap();
    assert_eq!((data.base, data.votes, data.differ), (Some(DATA), 1, 0));
    let bss = r.sections.iter().find(|s| s.name == ".bss").unwrap();
    assert_eq!(bss.base, Some(BSS));
    let runs = dataruns::measure(&env, &objs, &place, dataruns::CONTENT_BUDGET).unwrap();
    assert!(
        runs.points
            .iter()
            .any(|p| p.rva == DATA + 4 && p.off == 4 && p.via == "_f"),
        "{:?}",
        runs.points
    );
    let s = seated::measure(&env, env.img.bytes(), &objs, None).unwrap();
    assert_eq!((s.population, s.exact), (1, 1));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn a_site_whose_context_disagrees_is_seated_by_content_and_a_spent_budget_says_so() {
    let dir = scratch("content");
    let objs = write(&dir, &object());
    let env = env(0xa3);
    let all = dataseat::seat_all(&env, &objs, dataseat::CONTENT_BUDGET);
    let r = all.first().unwrap().1.as_ref().unwrap();
    let data = r.sections.iter().find(|s| s.name == ".data").unwrap();
    assert_eq!((data.base, data.by_content), (Some(DATA), true), "{r:?}");
    let all = dataseat::seat_all(&env, &objs, 0);
    let r = all.first().unwrap().1.as_ref().unwrap();
    assert!(r.content_exhausted);
    assert_eq!(r.unseated(), 1, "{r:?}");
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn a_content_search_that_matches_everywhere_runs_out_of_its_budget() {
    let mut o = Object::new();
    let t = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: vec![0xc3],
        ..Section::default()
    });
    let mut raw = vec![0u8; 4096];
    raw.extend_from_slice(&[0, 0, 0, 0]);
    let d = o.section(Section {
        name: ".data".into(),
        characteristics: SCN_DATA,
        data: raw,
        ..Section::default()
    });
    o.section_symbol(t, SELECT_NODUPLICATES, 0);
    let f = o.external("_f", 0, t);
    o.external("_table", 0, d);
    o.sections[1].relocs = vec![Reloc {
        va: 4096,
        symbol: f,
        typ: REL_DIR32,
    }];
    let mut pe = Pe::exe();
    pe.push(".text", SCN_CODE, vec![0xc3; 0x10]);
    pe.push(".data", PE_DATA, vec![0; 0x10000]);
    let img = Image::parse(pe.build()).unwrap();
    let mut on = NameOracle::default();
    on.add(
        "tsv",
        "n.tsv",
        vec![Name {
            rva: 0x1000,
            name: "_f".into(),
            kind: Kind::Label,
            size: None,
            scope: None,
            provider: "tsv".into(),
        }],
        Vec::new(),
    );
    let env = Env::new(
        "FIX",
        img,
        Names::from_oracle(&on),
        vec![(0x1000, 1)],
        Vec::new(),
    )
    .unwrap();
    let dir = scratch("budget");
    let objs = write(&dir, &o.build());
    let r = watched("content budget", move || {
        dataruns::measure(&env, &objs, &|_, f| (f == "_f").then_some(0x1000), 1 << 16).unwrap()
    });
    assert!(r.content_exhausted);
    assert!(
        r.points.iter().all(|p| p.via != "content"),
        "{:?}",
        r.points
    );
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn a_hull_spanning_every_row_runs_out_of_its_pair_budget() {
    let mut text = " t\r\n\r\n Preferred load address is 00400000\r\n\r\n\
        \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n"
        .to_string();
    text.push_str(" 0002:00000000  _lo  00402000     Wide.obj\r\n");
    for k in 1..5000u32 {
        text.push_str(&format!(
            " 0002:{k:08x}  _x{k}  {:08x}     Mid.obj\r\n",
            0x0040_2000 + k
        ));
    }
    text.push_str(" 0002:00010000  _hi  00412000     Wide.obj\r\n");
    let m = Map::parse(Path::new("w.map"), &text).unwrap();
    let got = watched("hull", move || {
        let sp = dataowner::spans(&m);
        (
            dataowner::interruptions(&sp, &m, 1000),
            dataowner::interruptions(&sp, &m, dataowner::PAIR_BUDGET),
        )
    });
    assert!(got.0.exhausted && got.0.rows.len() <= 1000);
    assert!(!got.1.exhausted);
    assert_eq!(got.1.rows.len(), 4999);
}

fn bounded(what: &str, bytes: Vec<u8>) {
    let dir = scratch(&what.replace([' ', '='], "-"));
    let objs = write(&dir, &bytes);
    let env = env(0xa1);
    let len = bytes.len();
    let got = watched(what, move || {
        let s = dataseat::seat_all(&env, &objs, dataseat::CONTENT_BUDGET);
        let sections: usize = s
            .iter()
            .filter_map(|(_, r)| r.as_ref().ok())
            .map(|r| r.sections.len() + r.bss.len())
            .sum();
        let points = dataruns::measure(&env, &objs, &place, 1 << 20)
            .map(|r| r.points.len())
            .unwrap_or(0);
        let seat = seated::measure(&env, env.img.bytes(), &objs, None)
            .map(|r| r.population)
            .unwrap_or(0);
        (sections, points, seat)
    });
    std::fs::remove_dir_all(&dir).ok();
    assert!(got.0 <= len / 40 + 8, "{what}: {got:?}");
    assert!(got.1 <= len, "{what}: {got:?}");
    assert!(got.2 <= len / 40 + 2, "{what}: {got:?}");
}

#[test]
fn every_cut_and_edge_word_of_an_object_is_refused_or_placed_within_its_bytes() {
    let base = object();
    for cut in 0..base.len() {
        bounded(&format!("cut {cut}"), base[..cut].to_vec());
    }
    let len = u32::try_from(base.len()).unwrap();
    for at in (0..base.len().saturating_sub(3)).step_by(2) {
        for v in [0u32, 1, 0x7fff_ffff, 0xffff_ffff, len] {
            let mut b = base.clone();
            b[at..at + 4].copy_from_slice(&v.to_le_bytes());
            bounded(&format!("word {at:#x}={v:#x}"), b);
        }
    }
}

#[test]
fn a_body_is_located_under_the_mask_and_a_cut_candidate_is_answered_within_its_bytes() {
    let reference = image(0xa1);
    let whole = {
        let mut pe = Pe::exe();
        let mut t = vec![0x90; 0x30];
        let mut f = f_body(0xa1);
        f[1..5].copy_from_slice(&0x1234_5678u32.to_le_bytes());
        f[7..11].copy_from_slice(&0x9abc_def0u32.to_le_bytes());
        t[0x10..0x10 + f.len()].copy_from_slice(&f);
        pe.push(".text", SCN_CODE, t);
        pe.build()
    };
    let cand = Image::parse(whole.clone()).unwrap();
    let l = bodies::locate(&reference, &cand, F, 12).unwrap();
    assert_eq!((l.masked, l.from_relocs), (8, 8));
    assert_eq!(
        l.hits.iter().map(|h| (h.rva, h.shift)).collect::<Vec<_>>(),
        vec![(F + 0x10, 0x10)]
    );
    assert!(!l.exhausted);
    for cut in (0..whole.len()).step_by(11) {
        let b = whole[..cut].to_vec();
        let r = image(0xa1);
        let n = watched(&format!("locate cut {cut}"), move || {
            Image::parse(b)
                .ok()
                .and_then(|c| bodies::locate(&r, &c, F, 12).ok())
                .map(|l| l.hits.len())
        });
        assert!(n.unwrap_or(0) <= whole.len());
    }
}
