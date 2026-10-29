#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_fixture::classes;
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-class-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        Self(d)
    }

    fn file(&self, name: &str, bytes: &[u8]) -> PathBuf {
        let p = self.0.join(name);
        if let Some(d) = p.parent() {
            std::fs::create_dir_all(d).unwrap();
        }
        srcio::write_bytes(&p, bytes).unwrap();
        p
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        std::fs::remove_dir_all(&self.0).ok();
    }
}

fn run(dir: &Path, args: &[&str]) -> (i32, String, String) {
    let o = Command::new(BIN)
        .current_dir(dir)
        .env_remove("MSVC6T_CONFIG")
        .args(args)
        .output()
        .unwrap();
    (
        o.status.code().unwrap_or(-1),
        srcio::tool_text(&o.stdout),
        srcio::tool_text(&o.stderr),
    )
}

fn project(s: &Scratch, classes_table: &str) {
    s.file("fix.dll", &classes::image());
    s.file("names.tsv", classes::names_tsv().as_bytes());
    s.file("functions.json", classes::ghidra_json().as_bytes());
    s.file("include/shapes.h", classes::HEADER.as_bytes());
    s.file(
        "msvc6t.toml",
        format!(
            "schema = 1\n\n[[image]]\nid = \"FIX\"\nreference = \"fix.dll\"\nfunctions = [\"ghidra-json:functions.json\"]\nnames = [\"tsv:names.tsv\"]\n\n[image.classes]\n{classes_table}"
        )
        .as_bytes(),
    );
}

const CONTROLLED: &str = "sources = [\"include\"]\ntables = [{ rva = 0x2000, slots = 9, what = \"Base and Derived, one run\" }]\n";

#[test]
fn a_control_the_config_records_is_reproduced_or_the_verb_says_so() {
    let s = Scratch::new("control");
    project(&s, CONTROLLED);
    let (code, out, err) = run(&s.0, &["class", "vftables", "FIX"]);
    assert_eq!(code, 0, "{out}{err}");
    assert!(
        out.contains("    0x0002000    9 slots  FOUND, length agrees   Base and Derived, one run"),
        "{out}"
    );
    assert!(
        out.contains("  0x0002000      9  7 distinct, 0x1020..0x1080"),
        "{out}"
    );
    project(
        &s,
        "sources = [\"include\"]\ntables = [{ rva = 0x2000, slots = 4, what = \"Base alone\" }, { rva = 0x2010, slots = 5, what = \"Derived alone\" }]\n",
    );
    let (code, out, _) = run(&s.0, &["class", "vftables", "FIX"]);
    assert_eq!(code, 3, "{out}");
    assert!(out.contains("FOUND but 9 slots"), "{out}");
    assert!(
        out.contains("NOT FOUND -- detector misses a known table   Derived alone"),
        "{out}"
    );
    project(&s, "sources = [\"include\"]\n");
    let (code, out, _) = run(&s.0, &["class", "vftables", "FIX"]);
    assert_eq!(code, 0, "{out}");
    assert!(out.contains("  NO CONTROL: "), "{out}");
}

#[test]
fn the_stores_split_the_run_and_each_class_joins_its_own_table() {
    let s = Scratch::new("join");
    project(&s, CONTROLLED);
    let (code, out, err) = run(&s.0, &["class", "vftables", "FIX", "--stores", "--bodies"]);
    assert_eq!(code, 0, "{out}{err}");
    assert!(out.contains("  0x0002000      4       yes  0x1000@+0x0\n             bodies: 0x1020 0x1030 0x1040 0x1050\n"), "{out}");
    assert!(
        out.contains("  0x0002010      5       yes  0x1010@+0x0\n"),
        "{out}"
    );
    let (code, out, err) = run(
        &s.0,
        &["class", "join", "FIX", "--tsv", "t.tsv", "--json", "j.json"],
    );
    assert_eq!(code, 0, "{out}{err}");
    assert!(
        out.contains("  ASSIGNED 2 class(es): 9 slot(s) agree, 0 unpinned, 0 DISAGREE."),
        "{out}"
    );
    assert!(out.contains("    0x002010  Derived                        5 slots  3 own / 5 total hit(s)  include/shapes.h"), "{out}");
    let tsv = srcio::own_read(s.0.join("t.tsv")).unwrap();
    assert!(
        tsv.contains("0x10002000\tBase\t!~Base\t!f\t!f\t!g\n"),
        "{tsv}"
    );
    let j: serde_json::Value =
        serde_json::from_str(&srcio::own_read(s.0.join("j.json")).unwrap()).unwrap();
    assert_eq!(j["rows"].as_array().map(Vec::len), Some(2));
    assert_eq!(j["summary"]["agree"], 9);
    let (code, out, _) = run(&s.0, &["class", "join", "FIX", "--legacy-slots"]);
    assert_eq!(code, 0, "{out}");
    assert!(out.contains("  AMBIGUOUS -- "), "{out}");
    let (code, out, _) = run(&s.0, &["class", "vftables", "FIX", "--table", "0x2010"]);
    assert_eq!(code, 0, "{out}");
    assert!(
        out.contains("    written at 0x001010 in function 0x001010  to [base+0x0]"),
        "{out}"
    );
    assert!(
        out.contains("    [  4]  +0x10   0x001080  ->  0x001080 (direct)  ?h@Derived@@UAEXXZ"),
        "{out}"
    );
}

#[test]
fn a_misspelt_classes_key_is_refused_by_the_loader_and_the_sources_are_checked() {
    let s = Scratch::new("strict");
    project(&s, "source = [\"include\"]\n");
    let (code, _, err) = run(&s.0, &["class", "vftables", "FIX"]);
    assert_eq!(code, 1, "{err}");
    assert!(
        err.contains("image[0].classes") && err.contains("source"),
        "{err}"
    );
    project(&s, "sources = [\"absent\"]\n");
    let (code, out, _) = run(&s.0, &["check"]);
    assert_eq!(code, 1, "{out}");
    assert!(
        out.contains("FAIL image[0] FIX") && out.contains("classes sources"),
        "{out}"
    );
    let (code, _, err) = run(&s.0, &["class", "join", "FIX"]);
    assert_eq!(code, 1, "{err}");
}
