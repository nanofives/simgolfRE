use std::path::{Path, PathBuf};

pub const SKIP_NAMES: [&str; 1] = [".git"];

pub const ROOT_SKIP_NAMES: [&str; 1] = ["target"];

pub const CACHE_TAG: &str = "CACHEDIR.TAG";

pub const CARGO_TAG_SIGNATURE: &str = "Signature: 8a477f597d28d172789f06886806bc55";

pub const LINT_TARGET_ENV: &str = "MSVC6T_LINT_TARGET";

pub const LINT_DIR: &str = "target/msvc6t-lint";

pub const LINT_BUILD_DIR: &str = "target/msvc6t-lint/build";

pub const LINT_TAG_NOTE: &str =
    "# msvc6t lint's builds and markers; made again when it is missing.";

pub const KEY_DIRS: [&str; 5] = ["crates", ".cargo", "fixtures", "probes", "shim"];

pub const KEY_FILES: [&str; 8] = [
    "Cargo.toml",
    "Cargo.lock",
    "clippy.toml",
    ".clippy.toml",
    "rustfmt.toml",
    ".rustfmt.toml",
    "rust-toolchain.toml",
    "rust-toolchain",
];

pub const MEMBERS_DIR: &str = "crates";

#[must_use]
pub fn is_workspace_manifest(text: &str) -> bool {
    text.lines().any(|l| l.trim() == "[workspace]")
}

#[must_use]
pub fn workspace_root(start: &Path) -> Option<PathBuf> {
    let abs = std::fs::canonicalize(start).unwrap_or_else(|_| start.to_path_buf());
    abs.ancestors()
        .find(|a| {
            crate::srcio::own_read(a.join("Cargo.toml")).is_ok_and(|t| is_workspace_manifest(&t))
        })
        .map(Path::to_path_buf)
}

#[must_use]
pub fn lint_build_dir(root: &Path) -> PathBuf {
    let base =
        std::env::var_os(LINT_TARGET_ENV).map_or_else(|| root.join(LINT_BUILD_DIR), PathBuf::from);
    lint_build_dir_under(root, &base)
}

#[must_use]
pub fn lint_build_dir_under(root: &Path, base: &Path) -> PathBuf {
    let abs = std::fs::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
    let h = crate::sha256::sha256_hex(abs.as_os_str().as_encoded_bytes());
    base.join(short(&h))
}

#[must_use]
pub fn is_cargo_target(dir: &Path) -> bool {
    crate::srcio::own_read(dir.join(CACHE_TAG))
        .is_ok_and(|t| t.lines().next().map(str::trim_end) == Some(CARGO_TAG_SIGNATURE))
}

pub fn tag_lint_dir(root: &Path, inside: &Path) -> std::io::Result<()> {
    let own = root.join(LINT_DIR);
    if !inside.starts_with(&own) || is_cargo_target(&own) {
        return Ok(());
    }
    std::fs::create_dir_all(&own)?;
    crate::srcio::own_write(
        own.join(CACHE_TAG),
        format!("{CARGO_TAG_SIGNATURE}\n{LINT_TAG_NOTE}\n"),
    )
}

#[must_use]
pub fn skipped(root: &Path, dir: &Path) -> bool {
    let name = dir.file_name().and_then(|n| n.to_str()).unwrap_or("");
    let at_root = dir.parent() == Some(root);
    if at_root && SKIP_NAMES.contains(&name) {
        return true;
    }
    if !dir.join(CACHE_TAG).is_file() {
        return false;
    }
    let known_target = (at_root && ROOT_SKIP_NAMES.contains(&name))
        || dir == root.join(LINT_DIR)
        || dir.parent() == Some(root.join(LINT_BUILD_DIR).as_path())
        || dir == lint_build_dir(root);
    known_target && is_cargo_target(dir)
}

fn walk_into(root: &Path, dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return;
    };
    for e in rd.flatten() {
        let p = e.path();
        match e.file_type() {
            Ok(t) if t.is_dir() => {
                if !skipped(root, &p) {
                    walk_into(root, &p, out);
                }
            }
            Ok(_) => out.push(p),
            Err(_) => {}
        }
    }
}

#[must_use]
pub fn files_under(root: &Path, dir: &Path) -> Vec<PathBuf> {
    let mut out = Vec::new();
    if dir.is_dir() && !(dir != root && skipped(root, dir)) {
        walk_into(root, dir, &mut out);
    }
    out.sort();
    out
}

#[must_use]
pub fn rust_files(root: &Path, dir: &Path) -> Vec<PathBuf> {
    files_under(root, dir)
        .into_iter()
        .filter(|p| p.extension().is_some_and(|e| e == "rs"))
        .collect()
}

#[must_use]
pub fn relative(root: &Path, p: &Path) -> String {
    p.strip_prefix(root)
        .unwrap_or(p)
        .to_string_lossy()
        .replace('\\', "/")
}

#[must_use]
pub fn listed_files(root: &Path) -> Vec<String> {
    let git = std::process::Command::new("git")
        .arg("-C")
        .arg(root)
        .args([
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
        ])
        .output();
    if let Ok(o) = git
        && o.status.success()
    {
        let mut out: Vec<String> = crate::srcio::tool_text(&o.stdout)
            .split('\0')
            .filter(|f| !f.is_empty() && root.join(f).is_file())
            .map(str::to_string)
            .collect();
        out.sort();
        out.dedup();
        return out;
    }
    files_under(root, root)
        .iter()
        .map(|p| relative(root, p))
        .collect()
}

#[must_use]
pub fn cargo_home() -> Option<PathBuf> {
    std::env::var_os("CARGO_HOME")
        .filter(|h| !h.is_empty())
        .map(PathBuf::from)
        .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".cargo")))
}

#[must_use]
pub fn ancestor_configs(root: &Path) -> Vec<PathBuf> {
    configs_for(root, cargo_home().as_deref())
}

#[must_use]
pub fn configs_for(root: &Path, home: Option<&Path>) -> Vec<PathBuf> {
    let abs = std::fs::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
    let mut out = Vec::new();
    for a in abs.ancestors().skip(1) {
        for n in ["config", "config.toml"] {
            let p = a.join(".cargo").join(n);
            if p.is_file() {
                out.push(p);
            }
        }
    }
    if let Some(h) = home {
        let h = std::fs::canonicalize(h).unwrap_or_else(|_| h.to_path_buf());
        for n in ["config", "config.toml"] {
            let p = h.join(n);
            if p.is_file() && !out.contains(&p) {
                out.push(p);
            }
        }
    }
    out
}

#[must_use]
pub fn key_inputs(root: &Path) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = listed_files(root).iter().map(|f| root.join(f)).collect();
    out.extend(rust_files(root, root));
    for d in KEY_DIRS {
        out.extend(files_under(root, &root.join(d)));
    }
    for f in KEY_FILES {
        let p = root.join(f);
        if p.is_file() {
            out.push(p);
        }
    }
    out.extend(ancestor_configs(root));
    out.sort();
    out.dedup();
    out
}

#[must_use]
pub fn content_key(root: &Path) -> Option<String> {
    let files = key_inputs(root);
    if files.is_empty() {
        return None;
    }
    let mut h = crate::sha256::Sha256::new();
    for p in &files {
        let rel = p.strip_prefix(root).unwrap_or(p);
        h.update(rel.as_os_str().as_encoded_bytes());
        h.update(&[0]);
        h.update(&std::fs::read(p).ok()?);
        h.update(&[0]);
    }
    Some(crate::sha256::hex(&h.finish()))
}

#[must_use]
pub fn member_dirs(root: &Path) -> Vec<PathBuf> {
    let Ok(rd) = std::fs::read_dir(root.join(MEMBERS_DIR)) else {
        return Vec::new();
    };
    let mut out: Vec<PathBuf> = rd
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.join("Cargo.toml").is_file())
        .collect();
    out.sort();
    out
}

#[must_use]
pub fn package_name(manifest: &str) -> Option<String> {
    let mut in_package = false;
    for l in manifest.lines().map(str::trim) {
        if l.starts_with('[') {
            in_package = l == "[package]";
            continue;
        }
        if in_package
            && let Some(v) = l.strip_prefix("name")
            && let Some(v) = v.trim_start().strip_prefix('=')
        {
            return Some(v.trim().trim_matches('"').to_string());
        }
    }
    None
}

#[must_use]
pub fn short(key: &str) -> &str {
    key.get(..16).unwrap_or(key)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::srcio::own_write;

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-lint-tree-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn plant(root: &Path, rel: &str, text: &str) {
        let p = root.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        own_write(p, text).unwrap();
    }

    fn git_init(root: &Path) {
        let ok = std::process::Command::new("git")
            .arg("-C")
            .arg(root)
            .args(["init", "-q"])
            .status()
            .unwrap()
            .success();
        assert!(ok, "git init {}", root.display());
    }

    #[test]
    fn the_key_is_every_listed_file_and_every_rs_file_and_not_its_path() {
        let base = scratch("key");
        let (a, b) = (base.join("a"), base.join("b"));
        for d in [&a, &b] {
            plant(d, "Cargo.toml", "[workspace]\nmembers = [\"crates/*\"]\n");
            plant(d, "Cargo.lock", "version = 4\n");
            plant(d, ".gitignore", "ignored/\n");
            plant(d, "crates/x/src/lib.rs", "pub fn x() {}\n");
            plant(d, "crates/x/tests/t.rs", "#[test]\nfn t() {}\n");
            plant(d, "fixtures/c/a.c", "int a;\n");
            plant(d, "design/notes.md", "one\n");
            plant(d, "design/planted_mod.rs", "pub fn p() {}\n");
            plant(d, "ignored/notes.txt", "one\n");
            plant(d, "ignored/hidden.rs", "pub fn h() {}\n");
            git_init(d);
        }
        let ka = content_key(&a).unwrap();
        assert_eq!(ka, content_key(&b).unwrap());
        for (f, edit) in [
            ("crates/x/src/lib.rs", "pub fn x() { }\n"),
            ("Cargo.lock", "version = 3\n"),
            ("crates/x/tests/t.rs", "#[test]\nfn  t() {}\n"),
            ("fixtures/c/a.c", "int b;\n"),
            ("design/notes.md", "two\n"),
            ("design/planted_mod.rs", "pub fn p() { x.unwrap() }\n"),
            ("ignored/hidden.rs", "pub fn h() { x.unwrap() }\n"),
        ] {
            let keep = crate::srcio::own_read(b.join(f)).unwrap();
            own_write(b.join(f), edit).unwrap();
            assert_ne!(ka, content_key(&b).unwrap(), "{f} is an input");
            own_write(b.join(f), keep).unwrap();
        }
        own_write(b.join("ignored/notes.txt"), "two\n").unwrap();
        assert_eq!(
            ka,
            content_key(&b).unwrap(),
            "an ignored file that is not .rs is not an input"
        );
        plant(&b, "stray.rs", "pub fn s() {}\n");
        assert_ne!(ka, content_key(&b).unwrap(), "a new file is an input");
        std::fs::remove_file(b.join("stray.rs")).unwrap();
        plant(
            &b,
            ".cargo/config.toml",
            "[build]\nrustflags = [\"-Awarnings\"]\n",
        );
        assert_ne!(ka, content_key(&b).unwrap(), ".cargo is an input");
        assert_eq!(content_key(&base.join("empty")), None);
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn walks_skip_only_the_root_git_and_signed_cargo_targets() {
        let root = scratch("walk");
        let sig = format!("{CARGO_TAG_SIGNATURE}\n# cargo\n");
        plant(&root, "Cargo.toml", "[workspace]\n");
        plant(&root, "crates/x/src/lib.rs", "");
        plant(&root, ".git/HEAD", "");
        plant(&root, "target/CACHEDIR.TAG", &sig);
        plant(&root, "target/debug/a.rs", "");
        plant(&root, "crates/x/CACHEDIR.TAG", "");
        plant(&root, "crates/x/src/deep/CACHEDIR.TAG", &sig);
        plant(&root, "crates/x/src/deep/b.rs", "");
        plant(&root, "crates/x/.git/evil.rs", "");
        plant(&root, "fixtures/target/c.rs", "");
        let rel: Vec<String> = files_under(&root, &root)
            .iter()
            .map(|p| relative(&root, p))
            .collect();
        assert_eq!(
            rel,
            [
                "Cargo.toml",
                "crates/x/.git/evil.rs",
                "crates/x/CACHEDIR.TAG",
                "crates/x/src/deep/CACHEDIR.TAG",
                "crates/x/src/deep/b.rs",
                "crates/x/src/lib.rs",
                "fixtures/target/c.rs"
            ]
        );
        own_write(root.join("target/CACHEDIR.TAG"), "").unwrap();
        assert!(
            files_under(&root, &root).contains(&root.join("target/debug/a.rs")),
            "an unsigned tag skips nothing"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_lint_build_dir_is_skipped_when_the_target_holds_only_the_lint() {
        let root = scratch("lintdir");
        let sig = format!("{CARGO_TAG_SIGNATURE}\n");
        let own = relative(
            &root,
            &lint_build_dir_under(&root, &root.join(LINT_BUILD_DIR)),
        );
        assert!(own.starts_with("target/msvc6t-lint/build/"), "{own}");
        plant(&root, "target/msvc6t-lint/markers/k", "clean\n");
        plant(&root, &format!("{own}/CACHEDIR.TAG"), &sig);
        plant(&root, &format!("{own}/debug/x.rs"), "");
        plant(&root, "target/msvc6t-lint/build/0000/CACHEDIR.TAG", &sig);
        plant(&root, "target/msvc6t-lint/build/0000/y.rs", "");
        plant(&root, "target/msvc6t-lint/build/0001/CACHEDIR.TAG", "");
        plant(&root, "target/msvc6t-lint/other/CACHEDIR.TAG", &sig);
        let rel: Vec<String> = files_under(&root, &root)
            .iter()
            .map(|p| relative(&root, p))
            .collect();
        assert_eq!(
            rel,
            [
                "target/msvc6t-lint/build/0001/CACHEDIR.TAG",
                "target/msvc6t-lint/markers/k",
                "target/msvc6t-lint/other/CACHEDIR.TAG"
            ]
        );
        tag_lint_dir(&root, &root.join("elsewhere")).unwrap();
        assert!(!is_cargo_target(&root.join(LINT_DIR)));
        tag_lint_dir(&root, &root.join(LINT_DIR).join("markers")).unwrap();
        assert!(is_cargo_target(&root.join(LINT_DIR)));
        assert!(files_under(&root, &root).is_empty());
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_workspace_root_is_found_from_a_member() {
        let root = scratch("root");
        plant(
            &root,
            "Cargo.toml",
            "[workspace]\nmembers = [\"crates/*\"]\n",
        );
        plant(&root, "crates/x/Cargo.toml", "[package]\nname = \"x\"\n");
        let found = workspace_root(&root.join("crates/x")).unwrap();
        assert_eq!(found, std::fs::canonicalize(&root).unwrap());
        assert_eq!(member_dirs(&root), [root.join("crates/x")]);
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_package_name_is_read_from_its_table() {
        let m =
            "[package]\nname = \"msvc6-base\"\nversion = \"1\"\n[dependencies]\nname = \"no\"\n";
        assert_eq!(package_name(m).as_deref(), Some("msvc6-base"));
        assert_eq!(package_name("[workspace]\n"), None);
    }
}
