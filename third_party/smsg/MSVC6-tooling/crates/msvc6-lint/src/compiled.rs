use std::collections::BTreeSet;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

pub const DEPTH: usize = 16;

#[must_use]
pub fn depinfo_paths(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    for line in text.lines().filter(|l| !l.starts_with('#')) {
        let mut cur = String::new();
        let mut chars = line.chars().peekable();
        let push = |cur: &mut String, out: &mut Vec<String>| {
            let t = cur.strip_suffix(':').unwrap_or(cur.as_str()).to_string();
            if !t.is_empty() {
                out.push(t);
            }
            cur.clear();
        };
        while let Some(c) = chars.next() {
            match c {
                '\\' if chars.peek() == Some(&' ') => {
                    cur.push(' ');
                    chars.next();
                }
                c if c.is_whitespace() => push(&mut cur, &mut out),
                c => cur.push(c),
            }
        }
        push(&mut cur, &mut out);
    }
    out
}

fn is_build_out(p: &Path) -> bool {
    p.file_name().is_some_and(|n| n == "out")
        && p.parent()
            .and_then(Path::parent)
            .and_then(Path::file_name)
            .is_some_and(|n| n == "build")
}

fn depinfo_files(dir: &Path, depth: usize, out: &mut Vec<PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else {
        return;
    };
    for e in entries.flatten() {
        let Ok(t) = e.file_type() else { continue };
        let p = e.path();
        if t.is_dir() && depth < DEPTH && !is_build_out(&p) {
            depinfo_files(&p, depth + 1, out);
        } else if t.is_file() && p.extension().is_some_and(|x| x == "d") {
            out.push(p);
        }
    }
}

fn written_since(f: &Path, since: SystemTime) -> bool {
    std::fs::metadata(f)
        .and_then(|m| m.modified())
        .is_ok_and(|t| t >= since)
}

#[must_use]
pub fn compiled_sources(root: &Path, target: &Path, since: SystemTime) -> BTreeSet<String> {
    let mut files = Vec::new();
    depinfo_files(target, 0, &mut files);
    let roots: Vec<PathBuf> = [Some(root.to_path_buf()), std::fs::canonicalize(root).ok()]
        .into_iter()
        .flatten()
        .collect();
    let mut out = BTreeSet::new();
    for f in files {
        if !written_since(&f, since) {
            continue;
        }
        let Ok(text) = crate::srcio::own_read(&f) else {
            continue;
        };
        for t in depinfo_paths(&text) {
            let p = crate::modtree::normal(Path::new(&t));
            let rel = if p.is_absolute() {
                match roots.iter().find_map(|r| p.strip_prefix(r).ok()) {
                    Some(r) => r.to_path_buf(),
                    None => continue,
                }
            } else {
                p
            };
            out.insert(crate::tree::relative(Path::new(""), &rel));
        }
    }
    out
}

#[must_use]
pub fn crate_sources(listed: &[String]) -> Vec<&String> {
    listed
        .iter()
        .filter(|f| f.starts_with("crates/") && f.ends_with(".rs"))
        .collect()
}

#[must_use]
pub fn refusals(
    listed: &[String],
    depinfo: &BTreeSet<String>,
    modules: &BTreeSet<String>,
) -> Vec<String> {
    crate_sources(listed)
        .into_iter()
        .filter_map(|f| match (modules.contains(f), depinfo.contains(f)) {
            (true, true) => None,
            (false, true) => Some(format!(
                "{f}: read by a compile but in no crate's module tree"
            )),
            (_, false) => Some(format!("{f}: no crate compiles it")),
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_depinfo_file_names_every_source_once_escaped_spaces_and_targets_alike() {
        let text = "/t/debug/deps/libx-1.rmeta: crates/x/src/lib.rs crates/x/src/a\\ b.rs\n\n\
                    crates/x/src/lib.rs:\ncrates/x/src/a\\ b.rs:\n# env-dep:X=1\n";
        assert_eq!(
            depinfo_paths(text),
            [
                "/t/debug/deps/libx-1.rmeta",
                "crates/x/src/lib.rs",
                "crates/x/src/a b.rs",
                "crates/x/src/lib.rs",
                "crates/x/src/a b.rs",
            ]
        );
    }

    #[test]
    fn a_tracked_source_no_depinfo_lists_is_uncompiled() {
        let base = std::env::temp_dir().join(format!("msvc6-lint-compiled-{}", std::process::id()));
        let root = base.join("tree");
        let target = base.join("target/debug/deps");
        std::fs::create_dir_all(&root).unwrap();
        std::fs::create_dir_all(&target).unwrap();
        let abs = root.join("crates/x/src/m.rs");
        crate::srcio::own_write(
            target.join("x-1.d"),
            format!(
                "/t/libx-1.rmeta: crates/x/src/lib.rs {}\n",
                abs.display().to_string().replace(' ', "\\ ")
            ),
        )
        .unwrap();
        let compiled = compiled_sources(&root, &base.join("target"), SystemTime::UNIX_EPOCH);
        assert!(compiled.contains("crates/x/src/lib.rs"));
        assert!(compiled.contains("crates/x/src/m.rs"));
        let listed: Vec<String> = [
            "Cargo.toml",
            "crates/x/src/lib.rs",
            "crates/x/src/m.rs",
            "crates/x/src/notes.rs",
            "fixtures/x.rs",
        ]
        .iter()
        .map(|s| (*s).to_string())
        .collect();
        let modules: BTreeSet<String> = listed.iter().cloned().collect();
        assert_eq!(
            refusals(&listed, &compiled, &modules),
            ["crates/x/src/notes.rs: no crate compiles it"]
        );
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn a_depinfo_file_older_than_the_run_is_left_by_a_target_no_longer_built_and_names_nothing() {
        let base =
            std::env::temp_dir().join(format!("msvc6-lint-compiled-stale-{}", std::process::id()));
        if base.exists() {
            std::fs::remove_dir_all(&base).unwrap();
        }
        let root = base.join("tree");
        let deps = base.join("target/debug/deps");
        std::fs::create_dir_all(&root).unwrap();
        std::fs::create_dir_all(&deps).unwrap();
        let since = SystemTime::now() - std::time::Duration::from_secs(5);
        crate::srcio::own_write(deps.join("x-1.d"), "/t/libx-1.rmeta: crates/x/src/lib.rs\n")
            .unwrap();
        let stale = deps.join("old-2.d");
        crate::srcio::own_write(&stale, "/t/old-2: crates/x/src/bin/old.rs\n").unwrap();
        std::fs::File::options()
            .write(true)
            .open(&stale)
            .unwrap()
            .set_modified(since - std::time::Duration::from_secs(3600))
            .unwrap();
        let target = base.join("target");
        let compiled = compiled_sources(&root, &target, since);
        assert!(compiled.contains("crates/x/src/lib.rs"), "{compiled:?}");
        assert!(
            !compiled.contains("crates/x/src/bin/old.rs"),
            "{compiled:?}"
        );
        let listed: Vec<String> = ["crates/x/src/lib.rs", "crates/x/src/bin/old.rs"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        let modules: BTreeSet<String> = listed.iter().cloned().collect();
        assert_eq!(
            refusals(&listed, &compiled, &modules),
            ["crates/x/src/bin/old.rs: no crate compiles it"]
        );
        let earlier = since - std::time::Duration::from_secs(7200);
        assert!(compiled_sources(&root, &target, earlier).contains("crates/x/src/bin/old.rs"));
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn a_file_a_compile_reads_is_compiled_only_in_a_module_tree_and_a_build_script_s_d_names_nothing()
     {
        let base =
            std::env::temp_dir().join(format!("msvc6-lint-compiled-data-{}", std::process::id()));
        if base.exists() {
            std::fs::remove_dir_all(&base).unwrap();
        }
        let root = base.join("tree");
        let deps = base.join("target/debug/deps");
        let out = base.join("target/debug/build/x-0123/out");
        std::fs::create_dir_all(&root).unwrap();
        std::fs::create_dir_all(&deps).unwrap();
        std::fs::create_dir_all(&out).unwrap();
        crate::srcio::own_write(
            deps.join("x-1.d"),
            "/t/libx-1.rmeta: crates/x/src/lib.rs crates/x/src/./m.rs crates/x/src/y1.rs \
             crates/x/src/sub/../y3.rs\n",
        )
        .unwrap();
        crate::srcio::own_write(out.join("made.d"), "made: crates/x/src/y7.rs\n").unwrap();
        let compiled = compiled_sources(&root, &base.join("target"), SystemTime::UNIX_EPOCH);
        assert!(compiled.contains("crates/x/src/m.rs"), "{compiled:?}");
        assert!(compiled.contains("crates/x/src/y3.rs"), "{compiled:?}");
        assert!(!compiled.contains("crates/x/src/y7.rs"), "{compiled:?}");
        let listed: Vec<String> = [
            "crates/x/src/lib.rs",
            "crates/x/src/m.rs",
            "crates/x/src/y1.rs",
            "crates/x/src/y3.rs",
            "crates/x/src/y7.rs",
        ]
        .iter()
        .map(|s| (*s).to_string())
        .collect();
        let modules: BTreeSet<String> = ["crates/x/src/lib.rs", "crates/x/src/m.rs"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        assert_eq!(
            refusals(&listed, &compiled, &modules),
            [
                "crates/x/src/y1.rs: read by a compile but in no crate's module tree",
                "crates/x/src/y3.rs: read by a compile but in no crate's module tree",
                "crates/x/src/y7.rs: no crate compiles it",
            ]
        );
        std::fs::remove_dir_all(&base).unwrap();
    }
}
