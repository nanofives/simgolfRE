use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

#[test]
fn a_usage_error_exits_2() {
    let out = Command::new(BIN).arg("no-such-verb").output().unwrap();
    assert_eq!(out.status.code(), Some(2));
    let out = Command::new(BIN)
        .args(["lint", "--gate", "--fix"])
        .output()
        .unwrap();
    assert_eq!(out.status.code(), Some(2));
}

#[test]
fn help_names_a_verb_or_fails() {
    let out = Command::new(BIN).args(["help", "lint"]).output().unwrap();
    assert!(out.status.success());
    assert!(msvc6_base::srcio::tool_text(&out.stdout).contains("Usage: msvc6t lint"));
    let out = Command::new(BIN)
        .args(["help", "nothing"])
        .output()
        .unwrap();
    assert_eq!(out.status.code(), Some(2));
    assert!(msvc6_base::srcio::tool_text(&out.stderr).contains("no verb `nothing`"));
}
