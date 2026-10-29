use msvc6_lint::{hygiene, rules, tree};
use std::path::{Path, PathBuf};

fn root() -> Option<PathBuf> {
    tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR")))
}

fn listed(root: &Path) -> Vec<String> {
    tree::listed_files(root)
}

#[test]
fn no_tool_script_is_in_the_tree() {
    let root = root().unwrap();
    let bad: Vec<String> = listed(&root)
        .iter()
        .filter_map(|f| hygiene::script(&root, f))
        .collect();
    assert!(bad.is_empty(), "\n{}", bad.join("\n"));
}

#[test]
fn no_binary_is_in_the_tree() {
    let root = root().unwrap();
    let bad: Vec<String> = listed(&root)
        .iter()
        .filter_map(|f| hygiene::binary(&root, f))
        .collect();
    assert!(bad.is_empty(), "\n{}", bad.join("\n"));
}

#[test]
fn no_denylisted_project_name_is_in_the_code() {
    let root = root().unwrap();
    let files = listed(&root);
    let names = hygiene::denylist(&root, &files).unwrap();
    assert!(
        !names.is_empty(),
        "no examples/*/denylist.txt names anything, so this test would pass by checking nothing"
    );
    let bad: Vec<String> = files
        .iter()
        .filter(|f| hygiene::in_name_scope(f))
        .flat_map(|f| hygiene::named(&root, f, &names))
        .collect();
    assert!(bad.is_empty(), "\n{}", bad.join("\n"));
}

#[test]
fn no_rust_file_holds_a_comment() {
    let root = root().unwrap();
    let rs: Vec<PathBuf> = listed(&root)
        .iter()
        .filter(|f| f.ends_with(".rs"))
        .map(|f| root.join(f))
        .collect();
    assert!(!rs.is_empty());
    let bad: Vec<String> = rules::check_files(&root, &rs)
        .iter()
        .map(ToString::to_string)
        .collect();
    assert!(bad.is_empty(), "\n{}", bad.join("\n"));
}

#[test]
fn no_config_rule_is_broken() {
    let root = root().unwrap();
    let mut bad = msvc6_lint::config::file_refusals(&root, &listed(&root));
    bad.extend(msvc6_lint::config::root_refusals(&root));
    let bad: Vec<String> = bad.iter().map(ToString::to_string).collect();
    assert!(bad.is_empty(), "\n{}", bad.join("\n"));
}
