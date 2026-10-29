#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

mod common;

use msvc6_pdb::{eq, llvm, to70};
use std::process::ExitCode;

const REQUIRE: &str = "MSVC6T_REQUIRE_LLVM";

fn main() -> ExitCode {
    let Some(tool) = llvm::find(None) else {
        println!(
            "SKIP llvm: no {} in ${} or on PATH: LLVM's reading of the fixture 7.0 PDBs was NOT checked (set {} to LLVM's bin directory)",
            llvm::TOOL,
            llvm::ENV,
            llvm::ENV
        );
        if std::env::var(REQUIRE).is_ok_and(|v| v == "1") {
            println!("FAIL llvm: {REQUIRE}=1 and the tool is missing");
            return ExitCode::FAILURE;
        }
        return ExitCode::SUCCESS;
    };
    let dir = std::env::temp_dir().join(format!("msvc6t-llvm-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let mut failed = 0usize;
    for (name, pdb) in [
        ("linked", common::linked()),
        ("authored", common::authored()),
        ("typed", common::typed().0),
    ] {
        let cv = to70::convert(&pdb).unwrap();
        let p70 = dir.join(format!("{name}.pdb"));
        msvc6_base::srcio::own_write(&p70, &cv.bytes).unwrap();
        let r = llvm::read(&tool, &p70).unwrap();
        let ours20 = eq::counts_of(&pdb).unwrap();
        let ours70 = eq::counts_of(&cv.bytes).unwrap();
        let same = r.counts == ours70 && ours20 == ours70;
        println!(
            "{} llvm {name}: {} exit {:?}, {} stderr line(s), {} error line(s); counts 2.0 {:?} / 7.0 {:?} / llvm {:?}",
            if r.clean() && same { "PASS" } else { "FAIL" },
            r.tool,
            r.exit,
            r.stderr.len(),
            r.errors.len(),
            ours20,
            ours70,
            r.counts
        );
        for e in r.errors.iter().chain(&r.stderr).take(20) {
            println!("    {e}");
        }
        if !(r.clean() && same) {
            failed += 1;
        }
    }
    std::fs::remove_dir_all(&dir).ok();
    if failed > 0 {
        return ExitCode::FAILURE;
    }
    ExitCode::SUCCESS
}
