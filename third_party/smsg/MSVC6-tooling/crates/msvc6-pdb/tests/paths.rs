#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_pdb::config::{AttestedDir, AttestedFiles, Lane, PathsSpec, Record};
use msvc6_pdb::paths::{self, Module, Policy, Published, Score, relayout};
use msvc6_pdb::{dbi, msf, structure, to70};
use std::collections::BTreeMap;
use std::path::PathBuf;

struct Product(PathBuf);

impl Product {
    fn new(tag: &str, dirs: &[&str], files: &[&str]) -> Self {
        let p = std::env::temp_dir().join(format!("msvc6t-paths-{tag}-{}", std::process::id()));
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

fn game_spec() -> PathsSpec {
    PathsSpec {
        product: String::new(),
        product_win: Some("C:\\Repo\\work".into()),
        configs: vec!["Release".into(), "Debug".into()],
        lanes: vec![
            lane("C:\\lk\\app.exe", "game/app.exe"),
            lane("C:\\lk\\engine.lib", "game/engine.lib"),
            lane("C:\\lk\\codec.lib", "game/codec.lib"),
        ],
        attested: vec![attested(
            "game/codec.lib/src",
            "C:\\Code\\CODEC\\Src",
            false,
        )],
        files: vec![AttestedFiles {
            product: "game/codec.lib/src".into(),
            dir: "C:\\Code\\CODEC\\Src".into(),
            names: vec!["DECODE.C".into()],
            record: None,
            evidence: "a test".into(),
        }],
    }
}

const FILES: [&str; 6] = [
    "game/app.exe/src/Plot.cpp",
    "game/engine.lib/src/Arena.cpp",
    "game/engine.lib/include/ArenaList.h",
    "game/codec.lib/src/decode.c",
    "game/codec.lib/src/Encode.c",
    "lib/World.dll/include/Tile.h",
];

fn game(tag: &str) -> Product {
    Product::new(
        tag,
        &["game/app.exe", "game/engine.lib", "game/codec.lib/src"],
        &FILES,
    )
}

fn module(name: &str, obj: &str) -> Module {
    Module {
        name: name.into(),
        obj: obj.into(),
    }
}

fn main_mod() -> Module {
    module(".\\Release\\Plot.obj", ".\\Release\\Plot.obj")
}

fn engine_mod() -> Module {
    module(
        ".\\Release\\Arena.obj",
        "..\\engine.lib\\Release\\engine.lib",
    )
}

fn crt_mod() -> Module {
    module(
        "build\\intel\\st_obj\\memcpy.obj",
        "C:\\Program Files\\Microsoft Visual Studio\\VC98\\LIB\\libcmt.lib",
    )
}

fn renamed(p: &Policy, name: &str, m: &Module) -> String {
    match p.publish(name, m).unwrap() {
        Published::Renamed { name, .. } => name,
        Published::Stays(why) => panic!("`{name}` stays: {why}"),
    }
}

#[test]
fn each_name_is_its_product_file_spelled_by_the_rule() {
    let pr = game("publish");
    let mut lines = BTreeMap::new();
    lines.insert(
        "plot.obj".to_string(),
        vec!["C:\\orig\\Plot.cpp".to_string()],
    );
    let p = Policy::new(&game_spec(), &pr.0, lines).unwrap();
    let (m, e, c) = (main_mod(), engine_mod(), crt_mod());
    assert_eq!(
        renamed(&p, "C:\\lk\\app.exe\\src\\plot.cpp", &m),
        "game\\app.exe\\src\\Plot.cpp"
    );
    assert_eq!(
        renamed(&p, "..\\engine.lib\\include\\arenalist.h", &m),
        "game\\engine.lib\\include\\ArenaList.h"
    );
    assert_eq!(
        renamed(&p, "include\\ArenaList.h", &e),
        "game\\engine.lib\\include\\ArenaList.h"
    );
    assert_eq!(
        renamed(&p, "C:\\lk\\codec.lib\\src\\decode.c", &m),
        "C:\\Code\\CODEC\\Src\\DECODE.C"
    );
    assert_eq!(
        renamed(&p, "C:\\lk\\codec.lib\\src\\encode.c", &m),
        "C:\\Code\\CODEC\\Src\\Encode.c"
    );
    assert_eq!(
        renamed(&p, "C:\\Repo\\work\\lib\\World.dll\\include\\Tile.h", &m),
        "lib\\World.dll\\include\\Tile.h"
    );
    assert!(matches!(
        p.publish("intel\\memcpy.asm", &c).unwrap(),
        Published::Stays(_)
    ));
    assert!(matches!(
        p.publish("E:\\8337\\crt\\memcpy.c", &c).unwrap(),
        Published::Stays(_)
    ));
    assert!(matches!(
        p.publish("C:\\orig\\Plot.cpp", &m).unwrap(),
        Published::Stays(_)
    ));
    let e = format!(
        "{:#}",
        p.publish("C:\\lk\\app.exe\\src\\Gone.cpp", &m).unwrap_err()
    );
    assert!(
        e.contains("game/app.exe/src/Gone.cpp") && e.contains("no file of the product"),
        "{e}"
    );
    assert!(Policy::new(&game_spec(), &pr.0.join("nowhere"), BTreeMap::new()).is_err());
}

#[test]
fn a_published_name_passes_and_a_lane_name_fails() {
    let pr = game("judge");
    let p = Policy::new(&game_spec(), &pr.0, BTreeMap::new()).unwrap();
    let (m, e, c) = (main_mod(), engine_mod(), crt_mod());
    for (n, md) in [
        ("game\\app.exe\\src\\Plot.cpp", &m),
        ("game\\engine.lib\\include\\ArenaList.h", &e),
        ("C:\\Code\\CODEC\\Src\\DECODE.C", &m),
        ("C:\\Code\\CODEC\\Src\\Encode.c", &m),
        ("intel\\memcpy.asm", &c),
        (
            "C:\\Program Files\\Microsoft Visual Studio\\VC98\\INCLUDE\\new",
            &m,
        ),
    ] {
        assert!(p.judge(n, md).is_ok(), "{n}: {:?}", p.judge(n, md));
    }
    for (n, md, why) in [
        ("C:\\lk\\app.exe\\src\\Plot.cpp", &m, "build lane"),
        (
            "C:\\Repo\\work\\lib\\World.dll\\include\\Tile.h",
            &m,
            "build lane",
        ),
        (
            "game\\app.exe\\src\\plot.cpp",
            &m,
            "publishes as `game\\app.exe\\src\\Plot.cpp`",
        ),
        ("include\\ArenaList.h", &e, "relative"),
        ("C:\\Code\\CODEC\\Src\\NOPE.C", &m, "names no file"),
        ("c:\\code\\codec\\src\\Encode.c", &m, "held in its own case"),
        (
            "C:\\Code\\CODEC\\Src\\decode.c",
            &m,
            "spells `C:\\Code\\CODEC\\Src\\DECODE.C`",
        ),
    ] {
        let err = p.judge(n, md).unwrap_err();
        assert!(err.contains(why), "{n}: {err}");
    }
}

#[test]
fn a_files_own_attested_spelling_wins_record_by_record() {
    let pr = Product::new(
        "own",
        &["snd/snd.dll/src"],
        &["snd/voice.lib/v/analys.c", "snd/voice.lib/v/bsynz.c"],
    );
    let spec = PathsSpec {
        product: String::new(),
        product_win: None,
        configs: vec!["Release".into()],
        lanes: vec![
            lane("C:\\lk\\ide", "snd/snd.dll"),
            lane("C:\\lk\\voice.lib", "snd/voice.lib"),
        ],
        attested: vec![attested("snd/voice.lib/v", "C:\\voice-15\\v", false)],
        files: vec![AttestedFiles {
            product: "snd/voice.lib/v".into(),
            dir: "c:\\voice-15\\v".into(),
            names: vec!["analys.c".into()],
            record: Some(Record::Line),
            evidence: "a test".into(),
        }],
    };
    let p = Policy::new(&spec, &pr.0, BTreeMap::new()).unwrap();
    let l = module(
        ".\\Release\\analys.obj",
        "..\\voice.lib\\Release\\voice.lib",
    );
    assert_eq!(
        renamed(&p, "C:\\lk\\voice.lib\\v\\analys.c", &l),
        "c:\\voice-15\\v\\analys.c"
    );
    assert_eq!(
        p.publish_path("C:\\lk\\voice.lib\\v\\analys.c")
            .unwrap()
            .unwrap()
            .0,
        "C:\\voice-15\\v\\analys.c"
    );
    assert_eq!(
        renamed(&p, "C:\\lk\\voice.lib\\v\\BSYNZ.C", &l),
        "C:\\voice-15\\v\\bsynz.c"
    );
    assert!(p.judge("c:\\voice-15\\v\\analys.c", &l).is_ok());
    assert!(
        p.judge("C:\\voice-15\\v\\analys.c", &l)
            .unwrap_err()
            .contains("own case")
    );
    assert!(p.judge_path("C:\\voice-15\\v\\analys.c", false).is_ok());
    assert!(p.judge_path("c:\\voice-15\\v\\analys.c", false).is_err());
    assert!(p.judge_path("c:\\voice-15\\v\\analys.c", true).is_ok());
}

#[test]
fn a_build_records_paths_are_published_by_the_same_rule() {
    let pr = Product::new(
        "build",
        &["dev/dev.dll/include", "dev/z.lib"],
        &["dev/dev.dll/src/pump.cpp", "tools/vc6/VC98/Bin/CL.EXE"],
    );
    let spec = PathsSpec {
        product: String::new(),
        product_win: Some("C:\\Repo\\work".into()),
        configs: vec!["Release".into(), "Debug".into()],
        lanes: vec![
            lane("C:\\Dev", "dev/dev.dll"),
            lane("C:\\z.lib", "dev/z.lib"),
        ],
        attested: vec![
            attested("dev/dev.dll/src", "C:\\Dev", false),
            attested("dev/dev.dll", "C:\\Dev", false),
            attested("dev/dev.dll/Debug", "C:\\Dev\\Debug", true),
        ],
        files: Vec::new(),
    };
    let p = Policy::new(&spec, &pr.0, BTreeMap::new()).unwrap();
    for (ours, want) in [
        ("C:\\Dev", "C:\\Dev"),
        ("C:\\Dev\\include", "dev\\dev.dll\\include"),
        ("C:\\Dev\\Debug", "C:\\Dev\\Debug"),
        ("C:\\Dev\\debug\\dev.pch", "C:\\Dev\\Debug\\dev.pch"),
        ("C:\\Dev\\src\\pump.cpp", "C:\\Dev\\pump.cpp"),
        ("C:\\z.lib", "dev\\z.lib"),
        ("C:\\z.lib\\Debug\\z.obj", "dev\\z.lib\\Debug\\z.obj"),
        (
            "C:\\Repo\\work\\tools\\vc6\\VC98\\Bin\\cl.exe",
            "tools\\vc6\\VC98\\Bin\\CL.EXE",
        ),
    ] {
        assert_eq!(p.publish_path(ours).unwrap().unwrap().0, want, "{ours}");
        assert!(
            p.judge_path(want, false).is_ok(),
            "{want}: {:?}",
            p.judge_path(want, false)
        );
    }
    for stays in [
        ".\\Debug\\pump.obj",
        "dev.exp",
        "E:\\8337\\x.obj",
        "C:\\DevX\\y",
    ] {
        assert!(p.publish_path(stays).unwrap().is_none(), "{stays}");
        assert!(p.judge_path(stays, false).is_ok(), "{stays}");
    }
    for gone in ["C:\\Dev\\src\\gone.cpp", "C:\\Dev\\Debug\\x\\y.obj"] {
        assert!(p.publish_path(gone).is_err(), "{gone}");
    }
    for (bad, why) in [
        ("C:\\Dev\\src\\pump.cpp", "build lane"),
        ("C:\\Dev\\include", "build lane"),
        ("dev\\DEV.dll\\include", "publishes as"),
        ("c:\\Dev\\Debug\\pump.obj", "own case"),
    ] {
        let e = p.judge_path(bad, false).unwrap_err();
        assert!(e.contains(why), "{bad}: {e}");
    }
}

fn c11_region(files: &[(&str, &[(u32, u16)])]) -> Vec<u8> {
    let cf = files.len();
    let head = (4 + 4 * cf + 10).next_multiple_of(4);
    let mut bodies = Vec::new();
    let mut offs = Vec::new();
    for (name, pairs) in files {
        let fo = head + bodies.len();
        offs.push(u32::try_from(fo).unwrap());
        let mut fe = Vec::new();
        let blk = (fo + 4 + 12 + 1 + name.len()).next_multiple_of(4);
        fe.extend_from_slice(&1u16.to_le_bytes());
        fe.extend_from_slice(&0u16.to_le_bytes());
        fe.extend_from_slice(&u32::try_from(blk).unwrap().to_le_bytes());
        fe.extend_from_slice(&pairs[0].0.to_le_bytes());
        fe.extend_from_slice(&0x40u32.to_le_bytes());
        fe.push(u8::try_from(name.len()).unwrap());
        fe.extend_from_slice(name.as_bytes());
        while !(fo + fe.len()).is_multiple_of(4) {
            fe.push(0);
        }
        fe.extend_from_slice(&1u16.to_le_bytes());
        fe.extend_from_slice(&u16::try_from(pairs.len()).unwrap().to_le_bytes());
        for (o, _) in *pairs {
            fe.extend_from_slice(&o.to_le_bytes());
        }
        for (_, l) in *pairs {
            fe.extend_from_slice(&l.to_le_bytes());
        }
        while !fe.len().is_multiple_of(4) {
            fe.push(0);
        }
        bodies.extend_from_slice(&fe);
    }
    let mut b = Vec::new();
    b.extend_from_slice(&u16::try_from(cf).unwrap().to_le_bytes());
    b.extend_from_slice(&1u16.to_le_bytes());
    for o in offs {
        b.extend_from_slice(&o.to_le_bytes());
    }
    b.extend_from_slice(&0x10u32.to_le_bytes());
    b.extend_from_slice(&0x40u32.to_le_bytes());
    b.extend_from_slice(&1u16.to_le_bytes());
    b.resize(head, 0);
    b.extend_from_slice(&bodies);
    b
}

fn pairs_of(b: &[u8]) -> Vec<(String, Vec<(u32, u16)>)> {
    msvc6_pdb::lines::read_c11(b)
        .unwrap()
        .into_iter()
        .map(|f| {
            (
                f.name.iter().map(|c| char::from(*c)).collect(),
                f.blocks.into_iter().flat_map(|b| b.3).collect(),
            )
        })
        .collect()
}

#[test]
fn a_region_is_relaid_with_each_name_at_its_length_and_one_that_does_not_tile_is_refused() {
    let files: [(&str, &[(u32, u16)]); 3] = [
        ("C:\\lk\\app.exe\\src\\plot.cpp", &[(0x10, 5), (0x18, 6)]),
        ("include\\ArenaList.h", &[(0x20, 9)]),
        ("intel\\memcpy.asm", &[(0x28, 1), (0x2c, 2), (0x2e, 3)]),
    ];
    let b = c11_region(&files);
    let new = relayout(
        &b,
        &[
            Some(b"game\\app.exe\\src\\Plot.cpp".to_vec()),
            Some(b"game\\engine.lib\\include\\ArenaList.h".to_vec()),
            None,
        ],
    )
    .unwrap();
    assert!(new.len() > b.len());
    let back = pairs_of(&new);
    assert_eq!(back[1].0, "game\\engine.lib\\include\\ArenaList.h");
    for ((_, got), (_, had)) in back.iter().zip(files) {
        assert_eq!(got.as_slice(), had);
    }
    let short = relayout(&b, &[Some(b"a.c".to_vec()), Some(b"b.h".to_vec()), None]).unwrap();
    assert_eq!(short.len(), b.len(), "padded to the old length");
    let mut bad = b.clone();
    bad.extend_from_slice(&[0, 0, 0, 0]);
    assert!(relayout(&bad, &[None, None, None]).is_err(), "a stray tail");
    assert!(relayout(&b, &[None]).is_err(), "one name for three records");
    let mut overlap = b.clone();
    let base = usize::try_from(u32::from_le_bytes(b[4..8].try_into().unwrap())).unwrap();
    let blk = usize::try_from(u32::from_le_bytes(
        b[base + 4..base + 8].try_into().unwrap(),
    ))
    .unwrap();
    let n = u16::from_le_bytes([b[blk + 2], b[blk + 3]]);
    overlap[blk + 2..blk + 4].copy_from_slice(&(n + 1).to_le_bytes());
    let e = format!("{:#}", relayout(&overlap, &[None, None, None]).unwrap_err());
    assert!(e.contains("does not tile"), "{e}");
}

#[test]
fn two_product_files_may_not_publish_one_name() {
    let mut s = Score::default();
    s.claim("C:\\flat\\dup.h", "a/src/dup.h").unwrap();
    s.claim("c:\\FLAT\\dup.h", "a/src/dup.h").unwrap();
    let e = s.claim("C:\\flat\\dup.h", "a/include/dup.h").unwrap_err();
    assert!(e.to_string().contains("two product files"), "{e}");
}

fn fixture_spec() -> PathsSpec {
    PathsSpec {
        product: String::new(),
        product_win: None,
        configs: vec!["Release".into(), "Debug".into()],
        lanes: vec![lane("C:\\work", "proj/app")],
        attested: vec![
            attested("proj/app/src", "C:\\Orig", false),
            attested("proj/app/Release", "C:\\Orig\\Release", true),
        ],
        files: Vec::new(),
    }
}

fn fixture_product(tag: &str) -> Product {
    Product::new(
        tag,
        &["proj/app/src"],
        &["proj/app/src/a.cpp", "proj/app/a.h", "proj/app/src/b.cpp"],
    )
}

#[test]
fn a_whole_pdb_is_renamed_deterministically_and_its_names_then_pass() {
    let pr = fixture_product("whole");
    let spec = PathsSpec {
        lanes: vec![lane("C:\\work", "proj/app")],
        ..fixture_spec()
    };
    let raw = msvc6_fixture::pdb::sample().bytes;
    let pre = Product::new(
        "whole-flat",
        &["proj/app/Release"],
        &["proj/app/a.cpp", "proj/app/a.h", "proj/app/b.cpp"],
    );
    let p = Policy::new(
        &PathsSpec {
            attested: vec![
                attested("proj/app", "C:\\Orig", false),
                attested("proj/app/Release", "C:\\Orig\\Release", true),
            ],
            ..spec
        },
        &pre.0,
        BTreeMap::new(),
    )
    .unwrap();
    let before = paths::check(&raw, &p).unwrap();
    assert!(before.failures() > 0, "our link's names are the lane's");
    let mut db = msf::Msf2::parse(raw.clone()).unwrap();
    let score = paths::rename(&mut db, &p).unwrap();
    assert_eq!(score.records.values().sum::<usize>(), 3);
    assert_eq!(score.file_info.values().sum::<usize>(), 3);
    assert!(score.build.values().sum::<usize>() > 5, "{:?}", score.build);
    assert_eq!(score.refs, 5, "module 1's references follow its records");
    let out = db.bytes().to_vec();
    let after = paths::check(&out, &p).unwrap();
    assert_eq!(after.failures(), 0, "{:?}", after.rows);
    assert_eq!(structure::check(&out).failures(), 0);
    assert!(
        to70::convert(&out).is_ok(),
        "every pointer still lands on a record"
    );
    let d = dbi::Dbi::parse(&msf::Msf2::parse(out.clone()).unwrap().stream(3).unwrap()).unwrap();
    assert_eq!(d.mods[0].name_text(), "C:\\Orig\\Release\\a.obj");
    let mut again = msf::Msf2::parse(raw).unwrap();
    paths::rename(&mut again, &p).unwrap();
    assert!(again.bytes() == out.as_slice(), "renamed twice, one file");
    let _ = pr;
}
