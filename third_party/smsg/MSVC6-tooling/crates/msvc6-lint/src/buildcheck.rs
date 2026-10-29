use crate::rules::{Refusal, Rule};
use crate::{Error, Result};
use std::path::{Path, PathBuf};

pub const DIFF_LINES: usize = 80;

pub const KEY_ENV: &str = "MSVC6T_TREE_KEY";

pub const FMT_CHECKED_ENV: &str = "MSVC6T_FMT_CHECKED";

pub const ROOT_WATCH: [&str; 6] = [
    "Cargo.toml",
    "rustfmt.toml",
    ".rustfmt.toml",
    "rust-toolchain.toml",
    "clippy.toml",
    ".clippy.toml",
];

pub const SOURCE_DIRS: [&str; 1] = ["crates/"];

#[must_use]
pub fn linked_dirs(root: &Path, files: &[PathBuf]) -> Vec<Refusal> {
    files
        .iter()
        .filter(|p| std::fs::symlink_metadata(p).is_ok_and(|m| m.file_type().is_symlink()))
        .filter(|p| p.is_dir())
        .map(|p| crate::tree::relative(root, p))
        .filter(|rel| SOURCE_DIRS.iter().any(|d| rel.starts_with(d)))
        .map(|rel| Refusal {
            file: rel,
            line: 1,
            col: 1,
            what:
                "is a link to a directory: the source rules do not follow it, so a module reached \
                   through it would build unchecked"
                    .to_string(),
            rule: Rule::Include,
        })
        .collect()
}

#[must_use]
pub fn refusals(root: &Path, dir: &Path) -> (usize, Vec<Refusal>) {
    let files = crate::tree::files_under(root, dir);
    let rs: Vec<PathBuf> = files
        .iter()
        .filter(|p| p.extension().is_some_and(|e| e == "rs"))
        .cloned()
        .collect();
    let rel: Vec<String> = files
        .iter()
        .map(|p| crate::tree::relative(root, p))
        .collect();
    let mut bad = crate::rules::check_files(root, &rs);
    bad.extend(linked_dirs(root, &files));
    bad.extend(crate::config::file_refusals(root, &rel));
    bad.extend(crate::config::root_refusals(root));
    (rs.len(), bad)
}

#[must_use]
pub fn report(whose: &str, bad: &[Refusal]) -> String {
    let mut rules: Vec<Rule> = bad.iter().map(|r| r.rule).collect();
    rules.sort();
    rules.dedup();
    let mut m = String::new();
    for rule in rules {
        let these: Vec<String> = bad
            .iter()
            .filter(|r| r.rule == rule)
            .map(ToString::to_string)
            .collect();
        m.push_str(&format!(
            "{} ({} refusal(s)):\n    {}\n\n  {}\n\n",
            rule.header().replace("{whose}", whose),
            these.len(),
            these.join("\n    "),
            rule.footer()
        ));
    }
    m
}

fn source_rules(root: &Path, dir: &Path, whose: &str) -> Result<usize> {
    let (n, bad) = refusals(root, dir);
    if bad.is_empty() {
        Ok(n)
    } else {
        Err(Error(report(whose, &bad)))
    }
}

pub fn check_crate(root: &Path, crate_dir: &Path, rustfmt: &Path) -> Result<usize> {
    let whose = crate::tree::relative(root, crate_dir);
    source_rules(root, crate_dir, &whose)?;
    crate::fmtcheck::check(root, crate_dir, rustfmt, DIFF_LINES)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WorkspaceBuild {
    pub rust_files: usize,
    pub fmt_checked: usize,
    pub key: String,
}

pub fn check_workspace(root: &Path, rustfmt: &Path) -> Result<WorkspaceBuild> {
    let rust_files = source_rules(root, root, "the workspace")?;
    let mut fmt_checked = 0;
    let mut errors = Vec::new();
    for d in crate::tree::member_dirs(root) {
        match crate::fmtcheck::check(root, &d, rustfmt, DIFF_LINES) {
            Ok(n) => fmt_checked += n,
            Err(e) => errors.push(e.0),
        }
    }
    if !errors.is_empty() {
        return Err(Error(errors.join("\n")));
    }
    let key = crate::tree::content_key(root).ok_or_else(|| {
        Error::new(
            "SOURCE RULE: an input of the workspace could not be read, so it has no content key",
        )
    })?;
    Ok(WorkspaceBuild {
        rust_files,
        fmt_checked,
        key,
    })
}

fn manifest_dir() -> PathBuf {
    PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").unwrap_or_else(|| ".".into()))
}

fn root_of(crate_dir: &Path) -> PathBuf {
    crate::tree::workspace_root(crate_dir).unwrap_or_else(|| crate_dir.to_path_buf())
}

#[must_use]
pub fn crate_watches(root: &Path, crate_dir: &Path) -> Vec<PathBuf> {
    let mut out = vec![crate_dir.to_path_buf()];
    out.extend(
        ROOT_WATCH
            .iter()
            .map(|f| root.join(f))
            .chain(
                crate::config::CARGO_CONFIGS
                    .iter()
                    .map(|n| root.join(".cargo").join(n)),
            )
            .filter(|p| p.is_file()),
    );
    out.extend(crate::tree::ancestor_configs(root));
    out
}

#[must_use]
pub fn workspace_watches(root: &Path) -> Vec<PathBuf> {
    let mut out = vec![root.to_path_buf()];
    out.extend(crate::tree::ancestor_configs(root));
    out
}

fn watch(paths: &[PathBuf]) {
    for p in paths {
        println!("cargo:rerun-if-changed={}", p.display());
    }
}

fn refuse(e: &Error) -> ! {
    eprintln!("{e}");
    std::process::exit(1);
}

pub fn crate_main() {
    let crate_dir = manifest_dir();
    let root = root_of(&crate_dir);
    watch(&crate_watches(&root, &crate_dir));
    if let Err(e) = check_crate(&root, &crate_dir, &crate::fmtcheck::rustfmt_beside_rustc()) {
        refuse(&e);
    }
}

pub fn workspace_main() {
    let crate_dir = manifest_dir();
    let root = root_of(&crate_dir);
    watch(&workspace_watches(&root));
    match check_workspace(&root, &crate::fmtcheck::rustfmt_beside_rustc()) {
        Ok(b) => {
            println!("cargo:rustc-env={KEY_ENV}={}", b.key);
            println!("cargo:rustc-env={FMT_CHECKED_ENV}={}", b.fmt_checked);
        }
        Err(e) => refuse(&e),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::srcio::own_write;

    const CLEAN_ROOT: [(&str, &str); 2] = [
        (
            "Cargo.toml",
            "[workspace]\nmembers = [\"crates/*\"]\n\n[workspace.lints.clippy]\nunwrap_used = \"deny\"\n",
        ),
        ("rustfmt.toml", "newline_style = \"Unix\"\n"),
    ];

    fn clippy_toml() -> String {
        let rows: Vec<String> = crate::config::DISALLOWED_METHODS
            .iter()
            .map(|m| format!("    {{ path = \"{m}\", reason = \"srcio\" }},"))
            .collect();
        format!("disallowed-methods = [\n{}\n]\n", rows.join("\n"))
    }

    fn scratch(tag: &str) -> PathBuf {
        let root =
            std::env::temp_dir().join(format!("msvc6-lint-build-{tag}-{}", std::process::id()));
        if root.exists() {
            std::fs::remove_dir_all(&root).unwrap();
        }
        let x = root.join("crates/x");
        std::fs::create_dir_all(x.join("src")).unwrap();
        for (f, text) in CLEAN_ROOT {
            own_write(root.join(f), text).unwrap();
        }
        own_write(root.join("clippy.toml"), clippy_toml()).unwrap();
        own_write(
            x.join("Cargo.toml"),
            "[package]\nname = \"x\"\n\n[lints]\nworkspace = true\n",
        )
        .unwrap();
        own_write(x.join("src/lib.rs"), "pub fn x() {}\n").unwrap();
        own_write(x.join("build.rs"), "fn main() {}\n").unwrap();
        root
    }

    fn plant(root: &Path, rel: &str, text: &str) {
        let p = root.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        own_write(p, text).unwrap();
    }

    fn refused(root: &Path, dir: &Path) -> String {
        let (_, bad) = refusals(root, dir);
        let whose = if dir == root {
            "the workspace".to_string()
        } else {
            crate::tree::relative(root, dir)
        };
        report(&whose, &bad)
    }

    #[test]
    fn a_comment_anywhere_in_a_crate_refuses_its_build() {
        let root = scratch("comment");
        let x = root.join("crates/x");
        assert_eq!(source_rules(&root, &x, "crates/x"), Ok(2));
        own_write(x.join("build.rs"), "fn main() {} /* why */\n").unwrap();
        let e = source_rules(&root, &x, "crates/x").unwrap_err().0;
        assert!(
            e.contains("crates/x/build.rs:1:14: a block comment `/* why */`"),
            "{e}"
        );
        assert!(
            e.starts_with("SOURCE RULE: crates/x does not build with a comment"),
            "{e}"
        );
        own_write(x.join("build.rs"), "fn main() {}\n").unwrap();
        own_write(x.join("stray.rs"), "//! stray\n").unwrap();
        let e = source_rules(&root, &root, "the workspace").unwrap_err().0;
        assert!(
            e.contains("crates/x/stray.rs:1:1: an inner doc comment"),
            "{e}"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn a_linked_directory_in_a_crate_refuses_its_build() {
        let root = scratch("linkdir");
        let x = root.join("crates/x");
        let outside = root.join("outside");
        plant(&root, "outside/mod.rs", "// unchecked\npub fn o() {}\n");
        std::os::unix::fs::symlink(&outside, x.join("src/o")).unwrap();
        let e = refused(&root, &x);
        assert!(
            e.contains(
                "crates/x/src/o:1:1: is a link to a directory: the source rules do not follow it"
            ),
            "{e}"
        );
        assert!(
            e.starts_with(
                "SOURCE RULE: crates/x does not build with a module or include that reaches past"
            ),
            "{e}"
        );
        std::fs::remove_file(x.join("src/o")).unwrap();
        assert_eq!(source_rules(&root, &x, "crates/x"), Ok(2));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_srcio_refusal_has_its_own_header() {
        let root = scratch("srcio");
        let x = root.join("crates/x");
        plant(
            &root,
            "crates/x/src/le.rs",
            "#[allow(clippy::disallowed_methods)]\npub fn f() {}\n",
        );
        let e = refused(&root, &x);
        assert!(
            e.starts_with(
                "SOURCE RULE: crates/x does not build with clippy::disallowed_methods silenced outside a srcio.rs"
            ),
            "{e}"
        );
        assert!(!e.contains("comment"), "{e}");
        plant(
            &root,
            "crates/x/src/le.rs",
            "#[allow(renamed_and_removed_lints, clippy::disallowed_method)]\npub fn f() {}\n",
        );
        let e = refused(&root, &x);
        assert!(
            e.contains("silenced outside a srcio.rs (2 refusal(s))"),
            "{e}"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn a_cache_tag_hides_nothing_outside_the_cargo_target() {
        let root = scratch("cachetag");
        let x = root.join("crates/x");
        plant(&root, "crates/x/CACHEDIR.TAG", "");
        plant(&root, "crates/x/src/le.rs", "// planted\npub fn f() {}\n");
        let e = refused(&root, &root);
        assert!(e.contains("crates/x/src/le.rs:1:1: a line comment"), "{e}");
        assert!(
            e.contains("crates/x/CACHEDIR.TAG:1:1: is a CACHEDIR.TAG"),
            "{e}"
        );
        std::fs::remove_file(x.join("CACHEDIR.TAG")).unwrap();
        plant(
            &root,
            "crates/x/src/deep/CACHEDIR.TAG",
            &format!("{}\n", crate::tree::CARGO_TAG_SIGNATURE),
        );
        plant(&root, "crates/x/src/deep/hidden.rs", "// planted\n");
        let e = refused(&root, &x);
        assert!(
            e.contains("crates/x/src/deep/hidden.rs:1:1: a line comment"),
            "{e}"
        );
        assert!(e.contains("TREE RULE"), "{e}");
        let before = crate::tree::content_key(&root).unwrap();
        own_write(x.join("src/deep/hidden.rs"), "pub fn h() { x.unwrap() }\n").unwrap();
        assert_ne!(before, crate::tree::content_key(&root).unwrap());
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn every_planted_config_refuses_the_build() {
        for (rel, text, why) in [
            (
                "crates/x/Cargo.toml",
                "[package]\nname = \"x\"\n# why\n[lints]\nworkspace = true\n",
                "a TOML comment",
            ),
            (
                "crates/x/clippy.toml",
                "disallowed-methods = []\n",
                "a clippy config other than the root",
            ),
            (
                ".clippy.toml",
                "disallowed-methods = []\n",
                "a clippy config other than the root",
            ),
            (
                "crates/x/src/rustfmt.toml",
                "disable_all_formatting = true\n",
                "a rustfmt config other than the root",
            ),
            (
                "clippy.toml",
                "disallowed-methods = []\n",
                "no longer lists `std::fs::read_to_string`",
            ),
            (
                "rustfmt.toml",
                "\"disable_all_formatting\" = true\n",
                "not one of the keys",
            ),
            (
                "Cargo.toml",
                "[workspace]\nmembers = [\"crates/*\"]\n\n[workspace.lints.clippy]\ndisallowed_methods = \"allow\"\n",
                "sets a level for `clippy::disallowed_methods`",
            ),
            (
                "Cargo.toml",
                "[workspace]\nmembers = [\"crates/*\"]\n\n[workspace.lints.clippy]\n\
                 \"disallowed-methods\" = { level = \"allow\", priority = 1 }\n",
                "sets a level for `clippy::disallowed-methods`",
            ),
            (
                "Cargo.toml",
                "[workspace]\nmembers = [\"crates/*\"]\n\n[workspace.lints.clippy]\ndisallowed_method = \"allow\"\n\
                 [workspace.lints.rust]\nrenamed_and_removed_lints = \"allow\"\n",
                "sets a level for `rust::renamed_and_removed_lints`",
            ),
            (
                "crates/x/Cargo.toml",
                "[package]\nname = \"x\"\n\n[lints.clippy]\nstyle = \"allow\"\n",
                "sets a lint level in a member manifest",
            ),
            (
                ".cargo/config.toml",
                "[build]\nrustflags = [\"-Aclippy::style\"]\n",
                "carries the lint flag",
            ),
            (
                "crates/x/.cargo/config.toml",
                "[build]\nrustflags = \"-A warnings\"\n",
                "carries the lint flag",
            ),
            (
                "examples/acme/msvc6t.toml",
                "id = \"A\" # tail\n",
                "a TOML comment",
            ),
        ] {
            let root = scratch("config");
            plant(&root, rel, text);
            let e = refused(&root, &root);
            assert!(e.contains("CONFIG RULE: the workspace"), "{rel}: {e}");
            assert!(e.contains(why), "{rel}: {e}");
            std::fs::remove_dir_all(&root).unwrap();
        }
        let root = scratch("config");
        std::fs::remove_file(root.join("rustfmt.toml")).unwrap();
        assert!(refused(&root, &root).contains("rustfmt.toml:1:1: is missing"));
        std::fs::remove_dir_all(&root).unwrap();
        let root = scratch("config");
        assert!(
            refusals(&root, &root).1.is_empty(),
            "{}",
            refused(&root, &root)
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_workspace_check_reruns_on_any_change_under_the_root() {
        let root = scratch("watch");
        let w = workspace_watches(&root);
        assert_eq!(w.first(), Some(&root));
        let c = crate_watches(&root, &root.join("crates/x"));
        assert!(c.contains(&root.join("crates/x")) && c.contains(&root.join("clippy.toml")));
        std::fs::remove_dir_all(&root).unwrap();
    }
}
