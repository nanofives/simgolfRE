use crate::{Error, Result};
use std::path::{Path, PathBuf};

pub const MARKERS_ENV: &str = "MSVC6T_LINT_MARKERS";

pub const TARGET_ENV: &str = crate::tree::LINT_TARGET_ENV;

pub const MARKER_DIR: &str = "target/msvc6t-lint/markers";

pub const LINT_TARGET: &str = crate::tree::LINT_BUILD_DIR;

pub const SCRUB: [&str; 16] = crate::config::LINT_ENV;

#[must_use]
pub fn marker_dir(root: &Path) -> PathBuf {
    std::env::var_os(MARKERS_ENV).map_or_else(|| root.join(MARKER_DIR), PathBuf::from)
}

#[must_use]
pub fn lint_target(root: &Path) -> PathBuf {
    crate::tree::lint_build_dir(root)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Pinned {
    pub bin: PathBuf,
    pub channel: String,
    pub identity: String,
}

#[must_use]
pub fn channel_of(toolchain_toml: &str) -> Option<String> {
    toolchain_toml
        .lines()
        .map(str::trim)
        .find_map(|l| l.strip_prefix("channel"))
        .and_then(|l| l.split('"').nth(1))
        .map(str::to_string)
}

pub fn run_parts(
    program: &Path,
    args: &[&str],
    cwd: &Path,
    set: &[(&str, String)],
    scrub: &[&str],
) -> Result<(bool, String, String)> {
    let mut cmd = std::process::Command::new(program);
    cmd.args(args).current_dir(cwd);
    for v in scrub {
        cmd.env_remove(v);
    }
    for (k, v) in set {
        cmd.env(k, v);
    }
    let out = cmd
        .output()
        .map_err(|e| Error(format!("{} did not run: {e}", program.display())))?;
    Ok((
        out.status.success(),
        crate::srcio::tool_text(&out.stdout),
        crate::srcio::tool_text(&out.stderr),
    ))
}

pub fn run(
    program: &Path,
    args: &[&str],
    cwd: &Path,
    set: &[(&str, String)],
    scrub: &[&str],
) -> Result<(bool, String)> {
    let (ok, mut text, err) = run_parts(program, args, cwd, set, scrub)?;
    text.push_str(&err);
    Ok((ok, text))
}

pub fn pinned(root: &Path) -> Result<Pinned> {
    let toml = crate::srcio::own_read(root.join("rust-toolchain.toml")).map_err(|e| {
        Error(format!(
            "RUST GUARD: no rust-toolchain.toml in {} ({e})",
            root.display()
        ))
    })?;
    let channel = channel_of(&toml)
        .ok_or_else(|| Error::new("RUST GUARD: rust-toolchain.toml names no channel"))?;
    let (ok, which) = run(
        Path::new("rustup"),
        &["which", "--toolchain", &channel, "cargo"],
        root,
        &[],
        &[],
    )
    .map_err(|e| {
        Error(format!(
            "RUST GUARD: {e}. The Rust toolchain is pinned (rust-toolchain.toml: {channel}) and resolved \
             through rustup: `rustup toolchain install {channel} --component clippy rustfmt`"
        ))
    })?;
    let cargo = PathBuf::from(which.lines().next().unwrap_or("").trim());
    if !ok || !cargo.is_file() {
        return Err(Error(format!(
            "RUST GUARD: rustup has no toolchain {channel}. Install it: rustup toolchain install {channel} \
             --component clippy rustfmt\n  rustup said: {}",
            which.trim()
        )));
    }
    let bin = cargo
        .parent()
        .map(Path::to_path_buf)
        .ok_or_else(|| Error(format!("RUST GUARD: {} has no directory", cargo.display())))?;
    let mut parts = vec![bin.display().to_string()];
    for (tool, want) in [
        ("cargo", format!("cargo {channel} ")),
        ("rustc", format!("rustc {channel} ")),
        ("rustfmt", "rustfmt ".to_string()),
        ("clippy-driver", "clippy ".to_string()),
    ] {
        let p = bin.join(tool);
        let (ok, v) = run(&p, &["--version"], root, &[], &SCRUB)?;
        let v = v.lines().next().unwrap_or("").trim().to_string();
        if !ok || !v.starts_with(&want) {
            return Err(Error(format!(
                "RUST GUARD: {} answered `{v}`, not `{want}...`: the pinned toolchain is incomplete. \
                 Reinstall it: rustup toolchain install {channel} --component clippy rustfmt --force",
                p.display()
            )));
        }
        parts.push(v);
    }
    Ok(Pinned {
        bin,
        channel,
        identity: parts.join(" | "),
    })
}

fn marker_text(identity: &str) -> String {
    format!("rustfmt --check and clippy -D warnings: clean\ntoolchain: {identity}\n")
}

#[must_use]
pub fn marked(dir: &Path, key: &str, identity: &str) -> bool {
    crate::srcio::own_read(dir.join(key)).is_ok_and(|t| {
        t.lines()
            .any(|l| l.strip_prefix("toolchain: ") == Some(identity))
    })
}

pub fn mark(dir: &Path, key: &str, identity: &str) -> Result<PathBuf> {
    std::fs::create_dir_all(dir)?;
    let p = dir.join(key);
    crate::srcio::own_write(&p, marker_text(identity))?;
    Ok(p)
}

fn lock(dir: &Path) -> Option<std::fs::File> {
    std::fs::create_dir_all(dir).ok()?;
    let f = std::fs::OpenOptions::new()
        .create(true)
        .truncate(false)
        .write(true)
        .open(dir.join(".lock"))
        .ok()?;
    f.lock().ok()?;
    Some(f)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Gated {
    Known(PathBuf),
    Marked(PathBuf),
}

fn key_of(root: &Path) -> Result<String> {
    crate::tree::content_key(root).ok_or_else(|| {
        Error::new("RUST GUARD: an input of this tree could not be read, so it has no key")
    })
}

fn gated(
    dir: &Path,
    root: &Path,
    identity: &str,
    consult: bool,
    lint: impl FnOnce() -> Result<()>,
) -> Result<Gated> {
    crate::tree::tag_lint_dir(root, dir)?;
    let key = key_of(root)?;
    let _held = lock(dir);
    if consult && marked(dir, &key, identity) {
        return Ok(Gated::Known(dir.join(&key)));
    }
    lint()?;
    let after = key_of(root)?;
    if after != key {
        return Err(Error(format!(
            "RUST GUARD: the tree changed while it was linted ({} became {}), so nothing is marked; lint it again",
            crate::tree::short(&key),
            crate::tree::short(&after)
        )));
    }
    mark(dir, &key, identity).map(Gated::Marked)
}

pub fn gate(
    dir: &Path,
    root: &Path,
    identity: &str,
    lint: impl FnOnce() -> Result<()>,
) -> Result<Gated> {
    gated(dir, root, identity, true, lint)
}

pub fn lint_and_mark(
    dir: &Path,
    root: &Path,
    identity: &str,
    lint: impl FnOnce() -> Result<()>,
) -> Result<Gated> {
    gated(dir, root, identity, false, lint)
}

pub fn provenance(
    root: &Path,
    embedded: &str,
    fmt_checked: usize,
    lint: impl FnOnce(&Pinned) -> Result<()>,
) -> Result<String> {
    let tc = pinned(root)?;
    verdict(&marker_dir(root), root, embedded, fmt_checked, &tc, lint)
}

pub fn verdict(
    dir: &Path,
    root: &Path,
    embedded: &str,
    fmt_checked: usize,
    tc: &Pinned,
    lint: impl FnOnce(&Pinned) -> Result<()>,
) -> Result<String> {
    let here = crate::tree::content_key(root);
    if marked(dir, embedded, &tc.identity) {
        let moved = match here.as_deref() {
            Some(k) if k == embedded => String::new(),
            Some(k) => format!(
                "; the checkout at {} has changed since: it is tree {} now",
                root.display(),
                crate::tree::short(k)
            ),
            None => format!(
                "; the checkout at {} cannot be read to compare",
                root.display()
            ),
        };
        return Ok(format!(
            "this msvc6t was built from Rust tree {} (rustfmt checked {fmt_checked} crate root(s)), marked clean \
             by {}{moved}",
            crate::tree::short(embedded),
            tc.channel
        ));
    }
    if here.as_deref() == Some(embedded) {
        gate(dir, root, &tc.identity, || lint(tc))?;
        return Ok(format!(
            "this msvc6t was built from Rust tree {}, linted clean just now",
            crate::tree::short(embedded)
        ));
    }
    Err(Error(format!(
        "RUST GUARD: this msvc6t was built from a Rust tree never linted clean on this host, so it runs no \
         compiler.\n  its tree:       {}\n  this checkout:  {} ({})\n  Rebuild msvc6t from this checkout, or run \
         `msvc6t lint --gate` in the checkout it was built from.",
        crate::tree::short(embedded),
        here.as_deref().map_or("(unreadable)", crate::tree::short),
        root.display()
    )))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TreeReport {
    pub lines: Vec<String>,
}

pub fn tree_checks(root: &Path) -> Result<TreeReport> {
    let (rs, refusals) = crate::buildcheck::refusals(root, root);
    let h = crate::hygiene::scan(root)?;
    let mut bad: Vec<String> = refusals.iter().map(ToString::to_string).collect();
    bad.extend(h.refusals());
    if !bad.is_empty() {
        return Err(Error(format!(
            "TREE CHECKS: {} refusal(s)\n  {}",
            bad.len(),
            bad.join("\n  ")
        )));
    }
    Ok(TreeReport {
        lines: vec![
            format!("comments: none in {rs} .rs file(s)"),
            "configs: comment-free; rustfmt.toml and clippy.toml are the only ones, and no lint level is \
             overridden"
                .to_string(),
            h.summary(),
        ],
    })
}

fn tail(text: &str, n: usize) -> String {
    let lines: Vec<&str> = text.lines().collect();
    lines
        .get(lines.len().saturating_sub(n)..)
        .unwrap_or_default()
        .join("\n")
}

pub fn full_lint(root: &Path, tc: &Pinned, fix: bool) -> Result<Vec<String>> {
    let mut lines = Vec::new();
    for d in crate::tree::member_dirs(root) {
        let off = crate::fmtcheck::optouts(root, &d);
        if !off.is_empty() {
            return Err(Error(format!(
                "rustfmt: switched off, so `cargo fmt --check` would pass without reading:\n  {}",
                off.join("\n  ")
            )));
        }
    }
    let path =
        std::env::var_os("PATH").map_or_else(String::new, |p| p.to_string_lossy().into_owned());
    let target = lint_target(root);
    crate::tree::tag_lint_dir(root, &target)?;
    let set = [
        ("PATH", format!("{}:{path}", tc.bin.display())),
        ("CARGO_TARGET_DIR", target.display().to_string()),
    ];
    let cargo = tc.bin.join("cargo");
    let cargo_run = |args: &[&str]| run(&cargo, args, root, &set, &SCRUB);

    let fmt_args: &[&str] = if fix {
        &["fmt", "--all"]
    } else {
        &["fmt", "--all", "--check"]
    };
    let (fmt_ok, fmt_out) = cargo_run(fmt_args)?;
    if fix {
        lines.push(format!("rustfmt: applied ({})", tc.channel));
    } else if fmt_ok {
        lines.push(format!("rustfmt: clean ({})", tc.channel));
    } else {
        let files = crate::fmtcheck::diff_files(&fmt_out);
        return Err(Error(format!(
            "rustfmt: {} file(s) need formatting ({}): run `cargo fmt --all` or `msvc6t lint --fix`",
            files.len(),
            files.join(", ")
        )));
    }

    if !fix {
        let names: Vec<String> = crate::tree::member_dirs(root)
            .iter()
            .filter_map(|d| crate::srcio::own_read(d.join("Cargo.toml")).ok())
            .filter_map(|m| crate::tree::package_name(&m))
            .collect();
        let mut args = vec!["clean"];
        for n in &names {
            args.push("-p");
            args.push(n);
        }
        let (ok, out) = cargo_run(&args)?;
        if !ok {
            return Err(Error(format!(
                "cargo clean of the workspace's own packages failed, so clippy's verdict could be stale:\n{}",
                tail(&out, 40)
            )));
        }
    }
    let mut args = vec!["clippy", "--workspace", "--all-targets", "--locked"];
    if fix {
        args.extend_from_slice(&["--fix", "--allow-dirty", "--allow-staged"]);
    }
    args.extend_from_slice(&["--", "-D", "warnings"]);
    let started = std::time::SystemTime::now();
    let (ok, out) = cargo_run(&args)?;
    if !ok {
        return Err(Error(format!(
            "clippy: not clean ({}, target {})\n{}",
            tc.channel,
            target.display(),
            tail(&out, 80)
        )));
    }
    lines.push(format!(
        "clippy: clean{} ({}, target {})",
        if fix { ", fixes applied" } else { "" },
        tc.channel,
        target.display()
    ));
    if !fix {
        let (ok, meta, err) = run_parts(
            &cargo,
            &["metadata", "--format-version", "1", "--no-deps", "--locked"],
            root,
            &set,
            &SCRUB,
        )?;
        if !ok {
            return Err(Error(format!(
                "cargo metadata failed, so no target's module tree can be walked:\n{}",
                tail(&err, 40)
            )));
        }
        lines.push(compiled_check(root, &target, started, &meta)?);
    }
    Ok(lines)
}

pub fn compiled_check(
    root: &Path,
    target: &Path,
    started: std::time::SystemTime,
    metadata: &str,
) -> Result<String> {
    let meta = crate::jsonread::parse(metadata)
        .map_err(|e| Error(format!("cargo metadata's output is not JSON: {e}")))?;
    let roots = crate::modtree::roots(&meta).map_err(Error)?;
    let walk = crate::modtree::walk(root, &roots);
    let listed = crate::tree::listed_files(root);
    let read = crate::compiled::compiled_sources(root, target, started);
    let mut bad = walk.unseen.clone();
    bad.extend(crate::compiled::refusals(
        &listed,
        &read,
        &walk.module_names(),
    ));
    if !bad.is_empty() {
        return Err(Error(format!(
            "compiled: {} refusal(s): every .rs file under crates/ is a module some target compiles (in the \
             module tree of one of cargo metadata's {} target roots, and named by the dep-info this clippy run \
             wrote under {}); a release would publish the rest as code no build reads:\n  {}",
            bad.len(),
            roots.len(),
            target.display(),
            bad.join("\n  ")
        )));
    }
    let n = crate::compiled::crate_sources(&listed).len();
    Ok(format!(
        "compiled: {n} of {n} .rs file(s) under crates/ are modules some target compiles (in the module tree of \
         one of {} target roots, and named by the dep-info this clippy run wrote)",
        roots.len()
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::srcio::own_write;

    fn tree(tag: &str) -> (PathBuf, PathBuf, PathBuf) {
        let base =
            std::env::temp_dir().join(format!("msvc6-lint-guard-{tag}-{}", std::process::id()));
        if base.exists() {
            std::fs::remove_dir_all(&base).unwrap();
        }
        let root = base.join("tree");
        std::fs::create_dir_all(root.join("crates/x/src")).unwrap();
        own_write(root.join("Cargo.toml"), "[workspace]\n").unwrap();
        own_write(root.join("crates/x/src/lib.rs"), "pub fn x() {}\n").unwrap();
        let markers = base.join("markers");
        (base, root, markers)
    }

    #[test]
    fn a_marker_is_a_verdict_of_one_toolchain_and_a_failing_lint_writes_none() {
        let (base, root, markers) = tree("gate");
        let k = crate::tree::content_key(&root).unwrap();

        let e = gate(&markers, &root, "tc-A", || {
            Err(Error::new("clippy: 3 finding(s)"))
        })
        .unwrap_err();
        assert!(e.0.contains("clippy: 3 finding(s)"));
        assert!(!markers.join(&k).is_file(), "a failed lint marks nothing");

        assert_eq!(
            gate(&markers, &root, "tc-A", || Ok(())).unwrap(),
            Gated::Marked(markers.join(&k))
        );
        assert!(marked(&markers, &k, "tc-A"));
        assert_eq!(
            gate(&markers, &root, "tc-A", || panic!("the marker answers")).unwrap(),
            Gated::Known(markers.join(&k))
        );

        assert!(!marked(&markers, &k, "tc-B"));
        let mut ran = false;
        gate(&markers, &root, "tc-B", || {
            ran = true;
            Ok(())
        })
        .unwrap();
        assert!(ran, "another toolchain's marker is not this one's verdict");

        own_write(root.join("crates/x/src/lib.rs"), "pub fn x() { }\n").unwrap();
        let mut ran = false;
        gate(&markers, &root, "tc-B", || {
            ran = true;
            Ok(())
        })
        .unwrap();
        assert!(ran, "an edited tree is not covered by the old marker");

        let k2 = crate::tree::content_key(&root).unwrap();
        assert_eq!(
            lint_and_mark(&markers, &root, "tc-B", || Ok(())).unwrap(),
            Gated::Marked(markers.join(&k2)),
            "the plain lint runs and marks even when a marker exists"
        );
        let e = lint_and_mark(&markers, &root, "tc-C", || {
            own_write(root.join("crates/x/src/lib.rs"), "pub fn y() {}\n").unwrap();
            Ok(())
        })
        .unwrap_err();
        assert!(e.0.contains("changed while it was linted"), "{e}");
        let k3 = crate::tree::content_key(&root).unwrap();
        assert!(!marked(&markers, &k2, "tc-C") && !marked(&markers, &k3, "tc-C"));
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn provenance_says_when_the_checkout_has_changed_since_the_marked_tree() {
        let (base, root, markers) = tree("moved");
        let k = crate::tree::content_key(&root).unwrap();
        let tc = Pinned {
            bin: PathBuf::new(),
            channel: "1.98.0".to_string(),
            identity: "tc-P".to_string(),
        };
        mark(&markers, &k, "tc-P").unwrap();
        let same = verdict(&markers, &root, &k, 1, &tc, |_| panic!("marked")).unwrap();
        assert!(
            same.ends_with("marked clean by 1.98.0") && !same.contains("changed"),
            "{same}"
        );
        own_write(root.join("crates/x/src/lib.rs"), "pub fn z() {}\n").unwrap();
        let k2 = crate::tree::content_key(&root).unwrap();
        let moved = verdict(&markers, &root, &k, 1, &tc, |_| panic!("marked")).unwrap();
        assert!(
            moved.contains(&format!(
                "marked clean by 1.98.0; the checkout at {} has changed since: it is tree {} now",
                root.display(),
                crate::tree::short(&k2)
            )),
            "{moved}"
        );
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn a_tree_without_git_keeps_its_key_while_its_own_lint_directory_fills() {
        let (base, root, _) = tree("nogit");
        let markers = root.join(MARKER_DIR);
        let k = crate::tree::content_key(&root).unwrap();
        let build = crate::tree::lint_build_dir_under(&root, &root.join(LINT_TARGET));
        let g = gate(&markers, &root, "tc-N", || {
            std::fs::create_dir_all(build.join("debug")).unwrap();
            own_write(
                build.join(crate::tree::CACHE_TAG),
                format!("{}\n", crate::tree::CARGO_TAG_SIGNATURE),
            )
            .unwrap();
            own_write(build.join("debug/out.rs"), "").unwrap();
            own_write(root.join(crate::tree::LINT_DIR).join("stray"), "x\n").unwrap();
            Ok(())
        })
        .unwrap();
        assert_eq!(g, Gated::Marked(markers.join(&k)));
        assert!(crate::tree::is_cargo_target(
            &root.join(crate::tree::LINT_DIR)
        ));
        assert_eq!(crate::tree::content_key(&root).unwrap(), k);
        assert_eq!(
            gate(&markers, &root, "tc-N", || panic!("the marker answers")).unwrap(),
            Gated::Known(markers.join(&k))
        );
        own_write(root.join("crates/x/src/lib.rs"), "pub fn y() {}\n").unwrap();
        assert_ne!(crate::tree::content_key(&root).unwrap(), k);
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn every_tree_lints_in_a_target_directory_of_its_own() {
        let a = lint_target(Path::new("/tmp/a/MSVC6Tooling"));
        let b = lint_target(Path::new("/tmp/b/MSVC6Tooling"));
        assert_ne!(a, b);
        assert_eq!(a, lint_target(Path::new("/tmp/a/MSVC6Tooling")));
    }

    #[test]
    fn the_channel_is_read_off_the_toolchain_file() {
        assert_eq!(
            channel_of("[toolchain]\nchannel = \"1.98.0\"\n").as_deref(),
            Some("1.98.0")
        );
        assert_eq!(channel_of("[toolchain]\n"), None);
    }

    #[test]
    fn the_pinned_toolchain_answers_as_its_channel() {
        let here = crate::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR"))).unwrap();
        let tc = pinned(&here).unwrap();
        assert_eq!(tc.channel, "1.98.0");
        assert!(tc.identity.contains("rustc 1.98.0 "), "{}", tc.identity);
        assert!(tc.bin.join("clippy-driver").is_file());
    }
}
