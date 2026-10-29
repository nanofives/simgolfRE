#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");
const GOLD: &str = "vc6";

fn dir() -> PathBuf {
    msvc6_lint::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR")))
        .unwrap()
        .join("probes/il")
}

fn names() -> BTreeSet<String> {
    std::fs::read_dir(dir())
        .unwrap()
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == "expect"))
        .map(|p| p.file_stem().unwrap().to_string_lossy().into_owned())
        .collect()
}

#[test]
fn every_il_probe_reads_and_every_source_is_comment_free() {
    let out = Command::new(BIN)
        .arg("probe")
        .arg("--check")
        .arg(dir())
        .output()
        .unwrap();
    let text = msvc6_base::srcio::tool_text(&out.stdout);
    assert!(out.status.success(), "{text}");
    assert!(
        text.contains(&format!("{} probe(s), 0 broken", names().len())),
        "{text}"
    );
    for f in std::fs::read_dir(dir()).unwrap().flatten() {
        let p = f.path();
        if p.extension()
            .is_some_and(|x| x == "c" || x == "cpp" || x == "h")
        {
            let lx = msvc6_source::lex::lex(&std::fs::read(&p).unwrap());
            assert!(lx.comments.is_empty(), "{}: a comment", p.display());
            assert!(lx.problems.is_empty(), "{}: {:?}", p.display(), lx.problems);
        }
    }
}

#[test]
fn the_results_record_every_il_probe_passing_on_gold() {
    let text = msvc6_base::srcio::own_read(dir().join("results.tsv")).unwrap();
    let mut lines = text.lines();
    assert_eq!(
        lines.next(),
        Some("probe\tsource\ttoolchain\tcl\tfront\tc2\tlink\tstatus\tgate")
    );
    let mut gold: BTreeMap<String, String> = BTreeMap::new();
    let mut seen = BTreeSet::new();
    for l in lines {
        let f: Vec<&str> = l.split('\t').collect();
        assert_eq!(f.len(), 9, "{l}");
        seen.insert(f[0].to_string());
        if f[2] == GOLD {
            gold.insert(f[0].to_string(), f[7].to_string());
        }
    }
    assert_eq!(
        seen,
        names(),
        "probes/il/results.tsv lists other probes than probes/il holds"
    );
    let failing: Vec<String> = names()
        .into_iter()
        .filter(|n| gold.get(n).map(String::as_str) != Some("PASS"))
        .collect();
    assert!(failing.is_empty(), "not PASS on {GOLD}: {failing:?}");
}
