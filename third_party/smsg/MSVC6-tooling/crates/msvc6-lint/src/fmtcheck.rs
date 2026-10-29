use crate::{Error, Result};
use std::path::{Path, PathBuf};

pub const OFF_KEYS: [&str; 2] = ["disable_all_formatting", "ignore"];

pub const CONFIGS: [&str; 2] = ["rustfmt.toml", ".rustfmt.toml"];

pub const TARGET_DIRS: [&str; 4] = ["src/bin", "tests", "benches", "examples"];

pub const DEPRECATED_SKIP: &str = "rustfmt_skip";

const CHUNK: usize = 200;

#[must_use]
pub fn crate_roots(crate_dir: &Path) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = Vec::new();
    let mut add = |p: PathBuf| {
        if p.is_file() {
            out.push(p);
        }
    };
    add(crate_dir.join("src/main.rs"));
    add(crate_dir.join("src/lib.rs"));
    add(crate_dir.join("build.rs"));
    for d in TARGET_DIRS {
        let Ok(rd) = std::fs::read_dir(crate_dir.join(d)) else {
            continue;
        };
        for e in rd.flatten() {
            let p = e.path();
            if p.is_dir() {
                add(p.join("main.rs"));
            } else if p.extension().is_some_and(|x| x == "rs") {
                add(p);
            }
        }
    }
    if let Ok(manifest) = crate::srcio::own_read(crate_dir.join("Cargo.toml")) {
        for l in manifest.lines().map(str::trim) {
            if let Some(v) = l.strip_prefix("path").map(str::trim_start)
                && let Some(v) = v.strip_prefix('=')
            {
                let v = v.trim().trim_matches('"');
                if v.ends_with(".rs") {
                    add(crate_dir.join(v));
                }
            }
        }
    }
    out.sort();
    out.dedup();
    out
}

#[must_use]
pub fn workspace_roots(root: &Path) -> Vec<PathBuf> {
    crate::tree::member_dirs(root)
        .iter()
        .flat_map(|d| crate_roots(d))
        .collect()
}

#[must_use]
pub fn edition_of(manifest: &str) -> Option<String> {
    manifest
        .lines()
        .map(str::trim)
        .filter_map(|l| l.strip_prefix("edition"))
        .find_map(|l| {
            let v = l.trim_start().strip_prefix('=')?;
            v.split('"').nth(1).map(str::to_string)
        })
}

#[must_use]
pub fn edition(root: &Path, crate_dir: &Path) -> Option<String> {
    [crate_dir, root].iter().find_map(|d| {
        crate::srcio::own_read(d.join("Cargo.toml"))
            .ok()
            .and_then(|m| edition_of(&m))
    })
}

#[must_use]
pub fn skip_attributes(src: &str) -> Vec<(usize, usize)> {
    let Ok(toks) = crate::lex::tokens(src) else {
        return Vec::new();
    };
    toks.iter()
        .enumerate()
        .filter(|(k, t)| {
            let path = t.is_ident("rustfmt")
                && toks.get(k + 1).is_some_and(|n| n.is_punct(':'))
                && toks.get(k + 2).is_some_and(|n| n.is_punct(':'))
                && toks.get(k + 3).is_some_and(|n| n.is_ident("skip"));
            path || t.is_ident(DEPRECATED_SKIP)
        })
        .map(|(_, t)| (t.line, t.col))
        .collect()
}

#[must_use]
pub fn stray_configs(root: &Path, crate_dir: &Path) -> Vec<PathBuf> {
    let mut found: Vec<PathBuf> = crate::tree::files_under(root, crate_dir)
        .into_iter()
        .filter(|p| {
            p.file_name()
                .is_some_and(|n| CONFIGS.iter().any(|c| n == *c))
        })
        .collect();
    for d in crate_dir.ancestors().skip(1) {
        if d == root || !d.starts_with(root) {
            break;
        }
        found.extend(CONFIGS.iter().map(|c| d.join(c)).filter(|p| p.is_file()));
    }
    found.extend(
        CONFIGS
            .iter()
            .filter(|c| **c != crate::config::ROOT_RUSTFMT)
            .map(|c| root.join(c))
            .filter(|p| p.is_file()),
    );
    found.retain(|p| *p != root.join(crate::config::ROOT_RUSTFMT));
    found.sort();
    found.dedup();
    found
}

#[must_use]
pub fn optouts(root: &Path, crate_dir: &Path) -> Vec<String> {
    let mut out: Vec<String> = crate::config::rustfmt_root_refusals(root)
        .iter()
        .map(ToString::to_string)
        .collect();
    for p in stray_configs(root, crate_dir) {
        out.push(format!(
            "{}: a rustfmt config other than the root rustfmt.toml",
            crate::tree::relative(root, &p)
        ));
    }
    for p in crate::tree::rust_files(root, crate_dir) {
        let Ok(text) = crate::srcio::own_read(&p) else {
            continue;
        };
        for (line, _) in skip_attributes(&text) {
            out.push(format!(
                "{}:{line}: `{}`",
                crate::tree::relative(root, &p),
                text.lines()
                    .nth(line.saturating_sub(1))
                    .unwrap_or("")
                    .trim()
            ));
        }
    }
    out
}

#[must_use]
pub fn diff_files(diff: &str) -> Vec<&str> {
    let mut out: Vec<&str> = Vec::new();
    for l in diff.lines() {
        let Some(rest) = l.strip_prefix("Diff in ") else {
            continue;
        };
        let rest = rest.trim_end().trim_end_matches(':');
        let f = if let Some((f, _)) = rest.split_once(" at line ") {
            f
        } else {
            match rest.rsplit_once(':') {
                Some((f, n)) if !n.is_empty() && n.chars().all(|c| c.is_ascii_digit()) => f,
                _ => rest,
            }
        };
        if !out.contains(&f) {
            out.push(f);
        }
    }
    out
}

#[must_use]
pub fn rustfmt_beside_rustc() -> PathBuf {
    let rustc = std::env::var_os("RUSTC").map(PathBuf::from);
    if let Some(dir) = rustc.as_deref().and_then(Path::parent) {
        let beside = dir.join("rustfmt");
        if beside.is_file() {
            return beside;
        }
    }
    PathBuf::from("rustfmt")
}

pub fn check(root: &Path, crate_dir: &Path, rustfmt: &Path, diff_lines: usize) -> Result<usize> {
    let roots = crate_roots(crate_dir);
    let shown = crate::tree::relative(root, crate_dir);
    let mut files = roots.clone();
    files.extend(crate::tree::rust_files(root, crate_dir));
    files.sort();
    files.dedup();
    if roots.is_empty() {
        return Err(Error(format!(
            "RUST FORMATTING GATE: {shown} has no crate root to check, and nothing checked is no pass"
        )));
    }
    let off = optouts(root, crate_dir);
    if !off.is_empty() {
        return Err(Error(format!(
            "RUST FORMATTING GATE: rustfmt is switched off, so `--check` would pass without reading:\n    {}\n\n  \
             Remove it: every crate is formatted whole.\n",
            off.join("\n    ")
        )));
    }
    let edition = edition(root, crate_dir).ok_or_else(|| {
        Error(format!(
            "RUST FORMATTING GATE: neither {shown}/Cargo.toml nor the workspace's declares an edition"
        ))
    })?;
    let config = root.join(crate::config::ROOT_RUSTFMT);
    let mut diff = String::new();
    let mut failed = false;
    for chunk in files.chunks(CHUNK) {
        let mut cmd = std::process::Command::new(rustfmt);
        cmd.current_dir(crate_dir)
            .args(["--check", "--color=never", "--edition", &edition])
            .arg("--config-path")
            .arg(&config);
        let out = cmd.args(chunk).output().map_err(|e| {
            Error(format!(
                "RUST FORMATTING GATE: {} did not run ({e}). It is part of the pinned toolchain \
                 (rust-toolchain.toml); install it with `rustup component add rustfmt`",
                rustfmt.display()
            ))
        })?;
        if !out.status.success() {
            failed = true;
            diff.push_str(&crate::srcio::tool_text(&out.stdout));
            diff.push_str(&crate::srcio::tool_text(&out.stderr));
        }
    }
    if !failed {
        return Ok(roots.len());
    }
    let named = diff_files(&diff);
    let mut m = format!("RUST FORMATTING GATE: {shown} does not build unformatted.\n\n");
    if named.is_empty() {
        m.push_str(&format!(
            "  rustfmt --check failed with no `Diff in` line. Its own output:\n{}\n",
            diff.trim()
        ));
    } else {
        m.push_str(&format!(
            "  {} file(s) rustfmt would change:\n",
            named.len()
        ));
        for f in &named {
            m.push_str(&format!("    {f}\n"));
        }
        m.push_str("\n  rustfmt's diff:\n");
        let total = diff.lines().count();
        for l in diff.lines().take(diff_lines) {
            m.push_str(&format!("    {l}\n"));
        }
        if total > diff_lines {
            m.push_str(&format!(
                "    ... and {} more line(s)\n",
                total.saturating_sub(diff_lines)
            ));
        }
    }
    m.push_str(&format!(
        "\n  FIX: cd {} && cargo fmt --all   (or `msvc6t lint --fix`)\n",
        root.display()
    ));
    Err(Error(m))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::srcio::own_write;

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-lint-fmt-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(d.join("crates/x/src")).unwrap();
        std::fs::create_dir_all(d.join("crates/x/tests")).unwrap();
        own_write(
            d.join("Cargo.toml"),
            "[workspace]\nmembers = [\"crates/*\"]\n[workspace.package]\nedition = \"2024\"\n",
        )
        .unwrap();
        own_write(
            d.join("crates/x/Cargo.toml"),
            "[package]\nname = \"x\"\nedition.workspace = true\n",
        )
        .unwrap();
        own_write(d.join("rustfmt.toml"), "newline_style = \"Unix\"\n").unwrap();
        own_write(d.join("crates/x/src/lib.rs"), "pub fn x() {}\n").unwrap();
        own_write(d.join("crates/x/build.rs"), "fn main() {}\n").unwrap();
        own_write(d.join("crates/x/tests/t.rs"), "#[test]\nfn t() {}\n").unwrap();
        d
    }

    #[test]
    fn the_roots_are_every_cargo_target() {
        let d = scratch("roots");
        let rel: Vec<String> = crate_roots(&d.join("crates/x"))
            .iter()
            .map(|p| crate::tree::relative(&d, p))
            .collect();
        assert_eq!(
            rel,
            [
                "crates/x/build.rs",
                "crates/x/src/lib.rs",
                "crates/x/tests/t.rs"
            ]
        );
        assert_eq!(workspace_roots(&d).len(), 3);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn the_edition_falls_back_to_the_workspace() {
        let d = scratch("edition");
        assert_eq!(edition(&d, &d.join("crates/x")).as_deref(), Some("2024"));
        assert_eq!(edition_of("edition.workspace = true\n"), None);
        assert_eq!(edition_of("edition = \"2021\"\n").as_deref(), Some("2021"));
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn every_way_of_switching_rustfmt_off_is_an_optout() {
        let d = scratch("off");
        let x = d.join("crates/x");
        assert!(optouts(&d, &x).is_empty());
        own_write(
            x.join("src/lib.rs"),
            "#[rustfmt::skip]\npub fn x() {}\npub const S: &str = \"#[rustfmt::skip]\";\n",
        )
        .unwrap();
        let o = optouts(&d, &x);
        assert_eq!(o.len(), 1, "{o:?}");
        assert!(o[0].starts_with("crates/x/src/lib.rs:1:"), "{o:?}");
        own_write(d.join("rustfmt.toml"), "disable_all_formatting = true\n").unwrap();
        assert_eq!(optouts(&d, &x).len(), 2);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_skip_is_found_however_it_is_spelled() {
        for (src, line) in [
            (
                "#[cfg_attr(any(), rustfmt_skip)]\npub fn f( x:u32)->u32{x}\n",
                1,
            ),
            (
                "pub fn g() {}\n#[rustfmt::\nskip]\npub fn f( x:u32)->u32{x}\n",
                2,
            ),
            ("#[cfg_attr(rustfmt, rustfmt_skip)]\npub fn f() {}\n", 1),
            ("#![rustfmt :: skip]\n", 1),
            ("#[r#rustfmt::r#skip]\nfn f() {}\n", 1),
            ("#[rustfmt::skip::macros(m)]\nfn f() {}\n", 1),
        ] {
            assert_eq!(
                skip_attributes(src).first().map(|p| p.0),
                Some(line),
                "{src}"
            );
        }
        assert!(skip_attributes("const S: &str = \"rustfmt::skip rustfmt_skip\";\n").is_empty());
        assert!(skip_attributes("use rustfmt_config::skipper;\n").is_empty());
    }

    #[test]
    fn a_config_other_than_the_root_one_or_none_at_the_root_is_an_optout() {
        let d = scratch("configs");
        let x = d.join("crates/x");
        for (rel, text) in [
            (
                "crates/x/src/rustfmt.toml",
                "disable_all_formatting = true\n",
            ),
            ("crates/.rustfmt.toml", "ignore = [\"x\"]\n"),
            (".rustfmt.toml", "newline_style = \"Unix\"\n"),
        ] {
            own_write(d.join(rel), text).unwrap();
            let o = optouts(&d, &x);
            assert_eq!(o.len(), 1, "{rel}: {o:?}");
            assert!(o[0].starts_with(rel), "{rel}: {o:?}");
            std::fs::remove_file(d.join(rel)).unwrap();
        }
        own_write(
            d.join("rustfmt.toml"),
            "\"disable_all_formatting\" = true\n",
        )
        .unwrap();
        let o = optouts(&d, &x);
        assert!(
            o.len() == 1 && o[0].contains("disable_all_formatting"),
            "{o:?}"
        );
        std::fs::remove_file(d.join("rustfmt.toml")).unwrap();
        own_write(
            d.join("crates/x/src/rustfmt.toml"),
            "disable_all_formatting = true\n",
        )
        .unwrap();
        let o = optouts(&d, &x);
        assert_eq!(o.len(), 2, "{o:?}");
        assert!(o[0].starts_with("rustfmt.toml:1:1: is missing"), "{o:?}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn an_included_file_is_format_checked_too() {
        let here = crate::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR"))).unwrap();
        let rustfmt = crate::guard::pinned(&here).unwrap().bin.join("rustfmt");
        let d = scratch("included");
        let x = d.join("crates/x");
        own_write(x.join("src/lib.rs"), "include!(\"planted_inc.rs\");\n").unwrap();
        own_write(x.join("src/planted_inc.rs"), "pub fn planted_fmt() {}\n").unwrap();
        assert_eq!(check(&d, &x, &rustfmt, 40), Ok(3));
        own_write(
            x.join("src/planted_inc.rs"),
            "pub fn planted_fmt( x:u32)->u32{x}\n",
        )
        .unwrap();
        let e = check(&d, &x, &rustfmt, 40).unwrap_err().0;
        assert!(e.contains("planted_inc.rs"), "{e}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn the_report_counts_files_not_blocks() {
        let diff = "Diff in /w/src/a.rs:336:\n-old\n+new\nDiff in /w/src/a.rs:1010:\n-o\n+n\n\
                    Diff in /w/src/b.rs at line 7:\n-old\n+new\n";
        assert_eq!(diff_files(diff), ["/w/src/a.rs", "/w/src/b.rs"]);
        assert!(diff_files("").is_empty());
    }

    #[test]
    fn the_check_refuses_an_unformatted_target_with_the_pinned_rustfmt() {
        let here = crate::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR"))).unwrap();
        let tc = crate::guard::pinned(&here).unwrap();
        let rustfmt = tc.bin.join("rustfmt");
        let d = scratch("check");
        let x = d.join("crates/x");
        assert_eq!(check(&d, &x, &rustfmt, 40), Ok(3));
        own_write(x.join("tests/t.rs"), "#[test]\nfn t( ) {}\n").unwrap();
        let e = check(&d, &x, &rustfmt, 40).unwrap_err().0;
        assert!(
            e.contains("RUST FORMATTING GATE") && e.contains("tests/t.rs"),
            "{e}"
        );
        assert!(e.contains("-fn t( ) {}") && e.contains("cargo fmt"), "{e}");
        std::fs::remove_dir_all(&d).unwrap();
    }
}
