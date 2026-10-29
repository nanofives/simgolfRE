#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::pe::{DIR_IAT, DIR_IMPORT, Pe, SCN_CODE, SCN_DATA};
use msvc6_fixture::tables::{Dll, idata};
use msvc6_image::pe::Image;
use msvc6_link::iat::{self, Blocks, Control, Pull};
use msvc6_link::qsort::Crt;
use msvc6_link::reserve::{self, Verdict};
use msvc6_link::verbose::Kind;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn read(p: &Path) -> String {
    msvc6_base::srcio::own_read(p).unwrap()
}

fn declared(source: &str) -> Vec<String> {
    source
        .lines()
        .filter(|l| l.trim_start().starts_with("__declspec(dllimport)"))
        .filter_map(|l| {
            let (_, rest) = l.split_once("__stdcall")?;
            Some(rest.trim().split('(').next()?.trim().to_string())
        })
        .collect()
}

struct Probe {
    name: String,
    libs: Vec<String>,
    crt: Crt,
    imports: Vec<(String, String)>,
}

fn probe(expect: &Path) -> Option<Probe> {
    let text = read(expect);
    let mut libs = Vec::new();
    let mut crt = Crt::Vc6;
    let mut imports = Vec::new();
    let mut inside = false;
    for l in text.lines() {
        if l.starts_with(' ') {
            if inside {
                let mut w = l.split_whitespace();
                if let (Some(d), Some(n)) = (w.next(), w.next()) {
                    imports.push((d.to_string(), n.to_string()));
                }
            }
            continue;
        }
        inside = l.trim() == "imports=" || l.trim() == "imports";
        if let Some(r) = l.strip_prefix("link ") {
            libs = r
                .split_whitespace()
                .filter(|t| t.to_ascii_lowercase().ends_with(".lib"))
                .map(str::to_string)
                .collect();
        }
        if l.trim() == "link-crt builtin" {
            crt = Crt::Wine;
        }
    }
    (!imports.is_empty()).then(|| Probe {
        name: expect
            .file_stem()
            .map(|s| s.to_string_lossy().into_owned())
            .unwrap_or_default(),
        libs,
        crt,
        imports,
    })
}

#[test]
fn the_model_writes_every_iat_probe_s_measured_order() {
    let mut checked = 0;
    let mut entries: Vec<PathBuf> = std::fs::read_dir(repo().join("probes"))
        .unwrap()
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == "expect"))
        .collect();
    entries.sort();
    for e in entries {
        let Some(p) = probe(&e) else { continue };
        let src = e.with_extension("cpp");
        let order = declared(&read(&src));
        let dll_of: BTreeMap<String, String> = p
            .imports
            .iter()
            .map(|(d, n)| (n.clone(), d.clone()))
            .collect();
        let mut pulls: Vec<Pull> = Vec::new();
        for lib in &p.libs {
            let stem = lib.to_ascii_lowercase().replace(".lib", "");
            let dll = p
                .imports
                .iter()
                .map(|(d, _)| d.clone())
                .find(|d| d.to_ascii_lowercase().starts_with(&stem))
                .unwrap_or_else(|| format!("{stem}.dll"));
            let names: Vec<&String> = order
                .iter()
                .filter(|n| dll_of.get(*n).is_some_and(|d| *d == dll))
                .collect();
            if names.is_empty() {
                continue;
            }
            for n in names {
                pulls.push(Pull {
                    member: dll.clone(),
                    symbol: format!("__imp__{n}@0"),
                    kind: Kind::Import,
                    first_ref: None,
                });
            }
            pulls.push(Pull {
                member: dll.clone(),
                symbol: format!("{dll}_NULL_THUNK_DATA"),
                kind: Kind::NullThunk,
                first_ref: None,
            });
        }
        let mut want: Blocks = BTreeMap::new();
        for (i, (d, n)) in p.imports.iter().enumerate() {
            want.entry(d.to_ascii_lowercase())
                .or_default()
                .push((u32::try_from(i * 4).unwrap(), n.clone()));
        }
        let sorted = iat::sorted_population(&pulls, p.crt, Control::Model).unwrap();
        let c = iat::compare(&pulls, &sorted, &want, &iat::block_order(&pulls, &sorted));
        assert_eq!(
            (c.same, c.slots),
            (p.imports.len(), p.imports.len()),
            "{}: {:?}",
            p.name,
            c.lines()
        );
        checked += 1;
    }
    assert!(checked >= 7, "{checked} IAT probe(s) checked");
}

fn pe_with(dlls: &[Dll], reserve: &dyn Fn(usize) -> u32) -> Image {
    let mut pe = Pe::dll();
    let text = pe.push(".text", SCN_CODE, vec![0xc3; 16]);
    pe.entry = text;
    let at = pe.next_rva();
    let t = idata(at, dlls, reserve);
    pe.push(".idata", SCN_DATA, t.bytes.clone());
    pe.dir(DIR_IMPORT, t.import_dir.0, t.import_dir.1);
    pe.dir(DIR_IAT, t.iat_dir.0, t.iat_dir.1);
    Image::parse(pe.build()).unwrap()
}

#[test]
fn a_reserve_that_disagrees_with_its_import_count_is_a_fossil() {
    let dlls = vec![
        Dll::named("A.dll", &["a0", "a1", "a2", "a3", "a4", "a5", "a6"]),
        Dll::named("B.dll", &["b0", "b1"]),
    ];
    let good = pe_with(&dlls, &|n| reserve::expected_reserve(n));
    let (b, s) = reserve::blocks(&good).unwrap();
    assert_eq!((s.blocks, s.due, s.fossils), (4, 4, 0), "{b:?}");
    let short = pe_with(&dlls, &|n| {
        if n == 7 {
            40
        } else {
            reserve::expected_reserve(n)
        }
    });
    let (b, s) = reserve::blocks(&short).unwrap();
    assert_eq!(s.fossils, 2, "{b:?}");
    let f = b.iter().find(|x| x.verdict == Verdict::Fossil).unwrap();
    assert_eq!(
        (f.dll.as_str(), f.due, f.reserve, f.implied),
        ("A.dll", 44, 40, Some((0, 4)))
    );
    assert!(
        f.line().contains("FOSSIL: 40 B implies 0..4 imports"),
        "{}",
        f.line()
    );
    let none = pe_with(&dlls, &|_| 0);
    let (_, s) = reserve::blocks(&none).unwrap();
    assert_eq!((s.none, s.fossils), (4, 0));
}

fn pulls_of(dlls: &[(&str, &[&str])]) -> Vec<Pull> {
    let mut out = Vec::new();
    for (d, names) in dlls {
        for n in *names {
            out.push(Pull {
                member: (*d).to_string(),
                symbol: format!("__imp__{n}@4"),
                kind: Kind::Import,
                first_ref: Some("a.obj".into()),
            });
        }
        out.push(Pull {
            member: (*d).to_string(),
            symbol: format!("{d}_NULL_THUNK_DATA"),
            kind: Kind::NullThunk,
            first_ref: None,
        });
    }
    out
}

#[test]
fn an_image_written_in_the_predicted_order_compares_slot_for_slot_and_inverts_back() {
    let k: Vec<String> = (0..12).map(|i| format!("K{i:02}")).collect();
    let u: Vec<String> = (0..6).map(|i| format!("U{i}")).collect();
    let kr: Vec<&str> = k.iter().map(String::as_str).collect();
    let ur: Vec<&str> = u.iter().map(String::as_str).collect();
    let pulls = pulls_of(&[("KERNEL32.dll", &kr), ("USER32.dll", &ur)]);
    let sorted = iat::sorted_population(&pulls, Crt::Vc6, Control::Model).unwrap();
    let pred = iat::block_names(&pulls, &sorted, &{
        let mut b: Blocks = BTreeMap::new();
        b.insert(
            "kernel32.dll".into(),
            k.iter().map(|n| (0, n.clone())).collect(),
        );
        b.insert(
            "user32.dll".into(),
            u.iter().map(|n| (0, n.clone())).collect(),
        );
        b
    });
    let order =
        |dll: &str| -> Vec<String> { pred[dll].iter().filter_map(|(_, n)| n.clone()).collect() };
    let (ko, uo) = (order("kernel32.dll"), order("user32.dll"));
    let kb: Vec<&str> = ko.iter().map(String::as_str).collect();
    let ub: Vec<&str> = uo.iter().map(String::as_str).collect();
    let img = pe_with(
        &[
            Dll::named("KERNEL32.dll", &kb),
            Dll::named("USER32.dll", &ub),
        ],
        &|n| reserve::expected_reserve(n),
    );
    let blocks = iat::blocks(&img).unwrap();
    let c = iat::compare(&pulls, &sorted, &blocks, &iat::block_order(&pulls, &sorted));
    assert_eq!((c.same, c.slots), (18, 18), "{:?}", c.lines());
    let kr2: Vec<&str> = kr.iter().rev().copied().collect();
    let other = pe_with(
        &[
            Dll::named("KERNEL32.dll", &kr2),
            Dll::named("USER32.dll", &ur),
        ],
        &|n| reserve::expected_reserve(n),
    );
    let ob = iat::blocks(&other).unwrap();
    let c2 = iat::compare(&pulls, &sorted, &ob, &iat::block_order(&pulls, &sorted));
    assert!(c2.same < 18);
    let inv = iat::invert(&pulls, &sorted, &ob);
    let q = iat::apply_inversion(&pulls, &inv).unwrap();
    let s2 = iat::sorted_population(&q, Crt::Vc6, Control::Model).unwrap();
    let c3 = iat::compare(&q, &s2, &ob, &iat::block_order(&q, &s2));
    assert_eq!((c3.same, c3.slots), (18, 18), "{:?}", c3.lines());
    let lines = iat::inversion_lines(&inv);
    assert!(
        lines
            .iter()
            .any(|l| l.contains("load position(s) already hold")),
        "{lines:?}"
    );
}
