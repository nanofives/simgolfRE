#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::digest::{md5_hex, sha256_hex};
use msvc6_base::srcio;
use msvc6_image::pe::Image;
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-pdbreloc-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        Self(d)
    }

    fn file(&self, name: &str, bytes: &[u8]) -> PathBuf {
        let p = self.0.join(name);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        srcio::write_bytes(&p, bytes).unwrap();
        p
    }

    fn path(&self, name: &str) -> PathBuf {
        self.0.join(name)
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        std::fs::remove_dir_all(&self.0).ok();
    }
}

fn run(dir: &Path, args: &[&str]) -> (i32, String) {
    let o = Command::new(BIN)
        .current_dir(dir)
        .env_remove("MSVC6T_CONFIG")
        .env("MSVC6T_LLVM", dir.join("no-llvm-here"))
        .env("PATH", dir.join("no-path-here"))
        .args(args)
        .output()
        .unwrap();
    (
        o.status.code().unwrap_or(-1),
        format!(
            "{}{}",
            srcio::tool_text(&o.stdout),
            srcio::tool_text(&o.stderr)
        ),
    )
}

fn report(smp: &msvc6_fixture::pdb::Sample) -> String {
    let img = Image::parse(smp.image.clone()).unwrap();
    let base = img.opt.image_base.get();
    let text = smp.procs[0].1;
    let data = smp.publics.iter().find(|p| p.0 == "_g_count").unwrap().1;
    let mut named: Vec<(String, u32, u32, &str)> = smp
        .procs
        .iter()
        .map(|(n, rva, len)| {
            let obj = if n == "_main" { "b.obj" } else { "a.obj" };
            (n.clone(), *rva, *len, obj)
        })
        .collect();
    named.push(("[thunk]:_f`adjustor{4}'".into(), text + 0x30, 5, "a.obj"));
    named.push(("_vcall".into(), text + 0x38, 6, "a.obj"));
    named.push(("_g_count".into(), data, 4, "a.obj"));
    named.push(("_s_static".into(), data + 0x10, 4, "a.obj"));
    let rows: Vec<serde_json::Value> = named
        .iter()
        .map(|(n, rva, len, obj)| {
            serde_json::json!({
                "address": format!("{:#x}", base + rva),
                "recomp": format!("{:#x}", base + rva),
                "name": n.trim_start_matches('_'),
                "msvc6t": {"symbol": n, "obj": obj, "size": len, "edit": 0},
            })
        })
        .collect();
    serde_json::json!({"file": "sample.dll", "data": rows}).to_string()
}

fn project(s: &Scratch, extra: &str) -> msvc6_fixture::pdb::Sample {
    let smp = msvc6_fixture::pdb::sample();
    s.file("ref/sample.dll", &smp.image);
    s.file("pdb/sample.pdb", &smp.bytes);
    s.file("report.json", report(&smp).as_bytes());
    let base = Image::parse(smp.image.clone())
        .unwrap()
        .opt
        .image_base
        .get();
    s.file(
        "ref/sample.map",
        format!("sample\n\n Preferred load address is {base:08x}\n\n  Address         Publics by Value\n\n").as_bytes(),
    );
    let cfg = format!(
        "schema = 1\n\n[[image]]\nid = \"SMP\"\nreference = \"ref/sample.dll\"\nsha256 = \"{}\"\n\n[image.pdb]\nfile = \"pdb/sample.pdb\"\n\
         shipped = [{{ md5 = \"{}\", what = \"the sample DLL\", file = \"ref/sample.dll\" }}]\n{extra}",
        sha256_hex(&smp.image),
        md5_hex(&smp.image)
    );
    s.file("msvc6t.toml", cfg.as_bytes());
    smp
}

const RELOCATE: [&str; 11] = [
    "pdb",
    "relocate",
    "SMP",
    "pdb/sample.pdb",
    "--ours",
    "ref/sample.dll",
    "--report",
    "report.json",
    "--out",
    "out/sample.pdb",
    "--pairs",
];

#[test]
fn relocate_writes_a_same_length_pdb_its_pairs_and_its_retired_list_and_the_audit_reads_them() {
    let s = Scratch::new("relocate");
    let smp = project(&s, "");
    std::fs::create_dir_all(s.path("out")).unwrap();
    let mut args = RELOCATE.to_vec();
    args.push("out/sample.pairs.tsv");
    let (code, out) = run(&s.0, &args);
    assert_eq!(code, 0, "{out}");
    assert!(
        out.contains("sample.dll: 7 address pairing(s) from report.json"),
        "{out}"
    );
    assert!(out.contains("  identity: signature"), "{out}");
    assert!(out.contains("MATCHES the shipped NB10 record"), "{out}");
    let pdb = std::fs::read(s.path("out/sample.pdb")).unwrap();
    assert_eq!(pdb.len(), smp.bytes.len());
    assert!(
        out.contains(&format!("  sha256 {}", sha256_hex(&pdb))),
        "{out}"
    );
    assert!(s.path("out/sample.pairs.tsv").is_file());
    assert!(s.path("out/sample.pairs.retired.tsv").is_file());
    let audit = [
        "pdb",
        "audit",
        "SMP",
        "out/sample.pdb",
        "--report",
        "report.json",
        "--pairs",
        "out/sample.pairs.tsv",
        "--ours",
        "ref/sample.dll",
        "--original",
        "pdb/sample.pdb",
    ];
    let (code, out) = run(&s.0, &audit);
    assert_eq!(code, 3, "{out}");
    assert!(out.contains("RESULT: 3 failure(s)"), "{out}");
    assert_eq!(out.matches("FAIL C11").count(), 3, "{out}");
    let (code, out) = run(&s.0, &audit[..6]);
    assert_eq!(
        code, 3,
        "the unrelocated input names no shipped thing: {out}"
    );
    let mut same = RELOCATE.to_vec();
    same[9] = "pdb/sample.pdb";
    same.push("out/again.pairs.tsv");
    let (code, out) = run(&s.0, &same);
    assert_ne!(code, 0, "{out}");
    assert_eq!(std::fs::read(s.path("pdb/sample.pdb")).unwrap(), smp.bytes);
}

#[test]
fn a_device_or_a_fifo_named_as_the_report_or_the_pdb_is_refused() {
    let s = Scratch::new("device");
    project(&s, "");
    std::fs::create_dir_all(s.path("out")).unwrap();
    let mut args = RELOCATE.to_vec();
    args[7] = "/dev/zero";
    args.push("out/x.pairs.tsv");
    let (code, out) = run(&s.0, &args);
    assert_ne!(code, 0, "{out}");
    assert!(out.contains("not a regular file"), "{out}");
    let fifo = s.path("pdb/fifo.pdb");
    let made = Command::new("mkfifo").arg(&fifo).status();
    if !made.is_ok_and(|st| st.success()) {
        eprintln!("SKIP fifo: mkfifo is not available here");
        return;
    }
    let (code, out) = run(
        &s.0,
        &[
            "pdb",
            "audit",
            "SMP",
            "pdb/fifo.pdb",
            "--report",
            "report.json",
        ],
    );
    assert_ne!(code, 0, "{out}");
    assert!(out.contains("not a regular file"), "{out}");
}

#[test]
fn an_author_table_the_config_names_and_that_cannot_be_read_is_an_error() {
    let s = Scratch::new("author");
    let author = |extra: &str| {
        format!(
            "\n[image.pdb.author]\nroot = \"kb\"\nfunctions = \"functions.tsv\"\n\
             data = {{ file = \"data.tsv\", va = \"va\" }}\nindex = \"index.tsv\"\n{extra}"
        )
    };
    s.file("kb/functions.tsv", b"va\tname\n");
    s.file("kb/data.tsv", b"va\tname\n");
    s.file("kb/index.tsv", b"name\tfile\n");
    s.file("kb/globals.tsv", b"name\ttype\n");
    std::fs::create_dir_all(s.path("out")).unwrap();
    let args = ["pdb", "author", "SMP", "--out", "out/a.pdb"];
    project(&s, &author("globals = \"globals.tsv\"\n"));
    let (code, out) = run(&s.0, &args);
    assert_eq!(code, 0, "{out}");
    for (key, extra) in [
        ("globals", "globals = \"global.tsv\"\n"),
        ("library", "library = \"library.tsv\"\n"),
        ("locals", "locals = \"locals.tsv\"\n"),
    ] {
        project(&s, &author(extra));
        let (code, out) = run(&s.0, &args);
        assert_eq!(code, 1, "{key}: {out}");
        assert!(
            out.contains(&format!("[image.pdb.author] {key} names `")),
            "{key}: {out}"
        );
    }
}

#[test]
fn symbolize_skips_loudly_without_llvm_and_fails_when_it_is_required() {
    let s = Scratch::new("symbolize");
    project(&s, "");
    let args = [
        "pdb",
        "symbolize",
        "SMP",
        "pdb/sample.pdb",
        "--report",
        "report.json",
        "--tool",
        "/nonexistent",
        "--copy",
        "ref/sample.dll",
    ];
    let (code, out) = run(&s.0, &args);
    assert_eq!(code, 0, "{out}");
    assert!(out.starts_with("SKIP llvm-symbolizer: "), "{out}");
    let mut req = args.to_vec();
    req.push("--require");
    let (code, out) = run(&s.0, &req);
    assert_ne!(code, 0, "{out}");
    assert!(out.contains("llvm-symbolizer: not found"), "{out}");
}

#[test]
fn dbghelp_skips_loudly_without_a_toolchain_and_fails_when_it_is_required() {
    let s = Scratch::new("dbghelp");
    project(&s, "");
    let args = [
        "pdb",
        "dbghelp",
        "SMP",
        "pdb/sample.pdb",
        "--report",
        "report.json",
    ];
    let (code, out) = run(&s.0, &args);
    assert_eq!(code, 0, "{out}");
    assert!(
        out.contains("SKIP dbghelp: no [toolchain.*] is configured"),
        "{out}"
    );
    let mut req = args.to_vec();
    req.push("--require");
    let (code, out) = run(&s.0, &req);
    assert_ne!(code, 0, "{out}");
    let (code, out) = run(&s.0, &["pdb", "dbghelp", "SMP", "pdb/sample.pdb"]);
    assert_ne!(code, 0, "{out}");
    assert!(out.contains("give --report"), "{out}");
}
