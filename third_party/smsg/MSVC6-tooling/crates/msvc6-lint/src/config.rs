use crate::rules::{Refusal, Rule};
use crate::tomlread::{self, Entry, Value};
use std::path::Path;

pub const ROOT_RUSTFMT: &str = "rustfmt.toml";

pub const RUSTFMT_CONFIGS: [&str; 2] = ["rustfmt.toml", ".rustfmt.toml"];

pub const RUSTFMT_KEYS: [&str; 2] = ["newline_style", "use_field_init_shorthand"];

pub const ROOT_CLIPPY: &str = "clippy.toml";

pub const CLIPPY_CONFIGS: [&str; 2] = ["clippy.toml", ".clippy.toml"];

pub const DISALLOWED_METHODS: [&str; 6] = [
    "std::fs::read_to_string",
    "std::fs::write",
    "std::string::String::from_utf8_lossy",
    "std::string::String::from_utf8",
    "std::str::from_utf8",
    "std::io::Read::read_to_string",
];

pub const SILENCERS: [&str; 7] = [
    "disallowed_methods",
    "disallowed_method",
    "all",
    "style",
    "restriction",
    "warnings",
    "renamed_and_removed_lints",
];

pub const LINT_FLAGS: [&str; 10] = [
    "-A",
    "-W",
    "-D",
    "-F",
    "--allow",
    "--warn",
    "--deny",
    "--forbid",
    "--force-warn",
    "--cap-lints",
];

pub const LINT_ENV: [&str; 16] = [
    "RUSTFLAGS",
    "CARGO_ENCODED_RUSTFLAGS",
    "CARGO_BUILD_RUSTFLAGS",
    "CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_RUSTFLAGS",
    "RUSTC_WRAPPER",
    "RUSTC_WORKSPACE_WRAPPER",
    "CARGO_BUILD_RUSTC_WRAPPER",
    "CARGO_BUILD_RUSTC_WORKSPACE_WRAPPER",
    "CLIPPY_CONF_DIR",
    "CLIPPY_ARGS",
    "RUSTUP_TOOLCHAIN",
    "RUSTC",
    "CARGO",
    "RUSTFMT",
    "CARGO_TARGET_DIR",
    "CARGO_BUILD_TARGET_DIR",
];

pub const CARGO_CONFIGS: [&str; 2] = ["config", "config.toml"];

fn refusal(rule: Rule, file: &str, line: usize, what: String) -> Refusal {
    Refusal {
        file: file.to_string(),
        line,
        col: 1,
        what,
        rule,
    }
}

fn name_of(rel: &str) -> &str {
    rel.rsplit('/').next().unwrap_or(rel)
}

#[must_use]
pub fn is_cargo_config(rel: &str) -> bool {
    let mut parts = rel.rsplit('/');
    let name = parts.next().unwrap_or("");
    CARGO_CONFIGS.contains(&name) && parts.next() == Some(".cargo")
}

#[must_use]
pub fn is_toml(rel: &str) -> bool {
    Path::new(rel).extension().is_some_and(|e| e == "toml") || is_cargo_config(rel)
}

fn read(path: &Path) -> std::result::Result<String, String> {
    crate::srcio::own_read(path).map_err(|e| e.to_string())
}

fn parsed(shown: &str, path: &Path, out: &mut Vec<Refusal>) -> Option<Vec<Entry>> {
    let text = match read(path) {
        Ok(t) => t,
        Err(e) => {
            out.push(refusal(
                Rule::Config,
                shown,
                1,
                format!("cannot be read: {e}"),
            ));
            return None;
        }
    };
    match tomlread::parse(&text) {
        Ok(e) => Some(e),
        Err(e) => {
            out.push(refusal(
                Rule::Config,
                shown,
                e.line,
                format!("cannot be read as TOML: {}", e.what),
            ));
            None
        }
    }
}

#[must_use]
pub fn toml_comments(shown: &str, text: &str) -> Vec<Refusal> {
    match tomlread::comments(text) {
        Ok(found) => found
            .into_iter()
            .map(|(line, col)| Refusal {
                file: shown.to_string(),
                line,
                col,
                what: "a TOML comment: every config is comment-free, like the Rust".to_string(),
                rule: Rule::Config,
            })
            .collect(),
        Err(e) => vec![refusal(
            Rule::Config,
            shown,
            e.line,
            format!("cannot be read as TOML: {}", e.what),
        )],
    }
}

fn flags(v: &Value) -> Vec<String> {
    match v {
        Value::Str(s) => s.split_whitespace().map(str::to_string).collect(),
        Value::Array(items) => items
            .iter()
            .filter_map(Value::as_str)
            .map(str::to_string)
            .collect(),
        _ => Vec::new(),
    }
}

#[must_use]
pub fn is_lint_flag(flag: &str) -> bool {
    LINT_FLAGS.iter().any(|f| {
        flag == *f
            || if f.starts_with("--") {
                flag.starts_with(&format!("{f}="))
            } else {
                flag.starts_with(f)
            }
    })
}

#[must_use]
pub fn cargo_config_refusals(shown: &str, entries: &[Entry]) -> Vec<Refusal> {
    let mut out = Vec::new();
    for e in entries {
        if e.key.last().is_some_and(|k| k == "rustflags") {
            for f in flags(&e.value).iter().filter(|f| is_lint_flag(f)) {
                out.push(refusal(
                    Rule::Config,
                    shown,
                    e.line,
                    format!(
                        "`{}` carries the lint flag `{f}`: lint levels come from [workspace.lints] and clippy.toml",
                        e.dotted()
                    ),
                ));
            }
        }
        let k: Vec<&str> = e.key.iter().map(String::as_str).collect();
        match k.as_slice() {
            ["build", t] if BUILD_TOOL_KEYS.contains(&t.replace('_', "-").as_str()) => {
                out.push(refusal(
                    Rule::Config,
                    shown,
                    e.line,
                    format!(
                        "`{}` replaces the compiler cargo runs, so the source rules and clippy's would not hold",
                        e.dotted()
                    ),
                ));
            }
            ["env", v] if LINT_ENV.contains(v) => {
                out.push(refusal(
                    Rule::Config,
                    shown,
                    e.line,
                    format!(
                        "`{}` sets {v} for every compile, which the lint removes from its own environment",
                        e.dotted()
                    ),
                ));
            }
            _ => {}
        }
    }
    out
}

pub const BUILD_TOOL_KEYS: [&str; 3] = ["rustc", "rustc-wrapper", "rustc-workspace-wrapper"];

fn is_silencer(e: &Entry, at: usize) -> Option<String> {
    if e.key.len() != at + 2 {
        return None;
    }
    let tool = e.key.get(at)?;
    let lint = e.key.get(at + 1)?;
    SILENCERS
        .contains(&lint.replace('-', "_").as_str())
        .then(|| format!("{tool}::{lint}"))
}

pub const DOUBLED_SEPARATORS: [&str; 5] = [",,", ", ,", ";;", "; ;", "  "];

#[must_use]
pub fn doubled_separator(text: &str) -> Option<&'static str> {
    DOUBLED_SEPARATORS
        .iter()
        .copied()
        .find(|t| text.contains(t))
}

pub const TARGET_TABLES: [&str; 5] = ["lib", "bin", "test", "bench", "example"];

#[must_use]
pub fn leaves_its_directory(rel: &str) -> bool {
    if rel.starts_with(['/', '\\']) || rel.contains(':') {
        return true;
    }
    let mut depth = 0usize;
    for c in rel.split(['/', '\\']) {
        match c {
            "" | "." => {}
            ".." => match depth.checked_sub(1) {
                Some(d) => depth = d,
                None => return true,
            },
            _ => depth = depth.saturating_add(1),
        }
    }
    false
}

fn paths_in_array(v: &Value) -> Vec<&str> {
    match v {
        Value::Array(items) => items
            .iter()
            .filter_map(|t| t.field("path").and_then(Value::as_str))
            .collect(),
        _ => Vec::new(),
    }
}

#[must_use]
pub fn target_paths(e: &Entry) -> Vec<(String, &str)> {
    let k: Vec<&str> = e.key.iter().map(String::as_str).collect();
    match k.as_slice() {
        ["package", "build"] => e
            .value
            .as_str()
            .map(|s| ("package.build".to_string(), s))
            .into_iter()
            .collect(),
        [t, "path"] if TARGET_TABLES.contains(t) => e
            .value
            .as_str()
            .map(|s| (format!("{t}.path"), s))
            .into_iter()
            .collect(),
        [t] if TARGET_TABLES.contains(t) => paths_in_array(&e.value)
            .into_iter()
            .map(|s| (format!("{t}.path"), s))
            .collect(),
        _ => Vec::new(),
    }
}

#[must_use]
pub fn manifest_refusals(shown: &str, entries: &[Entry], member: bool) -> Vec<Refusal> {
    let mut out = Vec::new();
    let mut inherits = false;
    let is_package = entries
        .iter()
        .any(|e| e.key.first().is_some_and(|k| k == "package"));
    for e in entries {
        let k: Vec<&str> = e.key.iter().map(String::as_str).collect();
        match k.as_slice() {
            ["workspace", "lints", ..] => {
                if let Some(l) = is_silencer(e, 2) {
                    out.push(refusal(
                        Rule::Config,
                        shown,
                        e.line,
                        format!(
                            "sets a level for `{l}`, which would override clippy's disallowed_methods: \
                             the workspace denies it through clippy.toml"
                        ),
                    ));
                }
            }
            ["package", "description"] => {
                if let Some(t) = e.value.as_str().and_then(doubled_separator) {
                    out.push(refusal(
                        Rule::Config,
                        shown,
                        e.line,
                        format!("the package description repeats a separator (`{t}`)"),
                    ));
                }
            }
            ["lints"] => {}
            ["lints", "workspace"] if e.value == Value::Bool(true) => inherits = true,
            ["lints", ..] if member => out.push(refusal(
                Rule::Config,
                shown,
                e.line,
                format!(
                    "`{}` sets a lint level in a member manifest: a crate's lints come from [workspace.lints]",
                    e.dotted()
                ),
            )),
            ["lints", ..] => {
                if let Some(l) = is_silencer(e, 1) {
                    out.push(refusal(
                        Rule::Config,
                        shown,
                        e.line,
                        format!("sets a level for `{l}`, which would override clippy's disallowed_methods"),
                    ));
                }
            }
            _ => {}
        }
        if member {
            for (key, p) in target_paths(e) {
                if leaves_its_directory(p) {
                    out.push(refusal(
                        Rule::Config,
                        shown,
                        e.line,
                        format!(
                            "`{key} = \"{p}\"` leaves the crate's directory, which the source rules check"
                        ),
                    ));
                }
            }
        }
    }
    if member && is_package && !inherits {
        out.push(refusal(
            Rule::Config,
            shown,
            1,
            "has no `[lints] workspace = true`: the crate would build without the workspace's lint set"
                .to_string(),
        ));
    }
    out
}

#[must_use]
pub fn rustfmt_refusals(shown: &str, entries: &[Entry]) -> Vec<Refusal> {
    entries
        .iter()
        .filter(|e| {
            e.header
                || !matches!(e.key.as_slice(), [k] if RUSTFMT_KEYS.contains(&k.as_str()))
                || matches!(e.value, Value::Array(_) | Value::Table(_))
        })
        .map(|e| {
            refusal(
                Rule::Config,
                shown,
                e.line,
                format!(
                    "sets `{}`, which is not one of the keys the toolkit's rustfmt.toml may set ({})",
                    e.dotted(),
                    RUSTFMT_KEYS.join(", ")
                ),
            )
        })
        .collect()
}

#[must_use]
pub fn listed_methods(entries: &[Entry]) -> Vec<String> {
    entries
        .iter()
        .filter(|e| matches!(e.key.as_slice(), [k] if k == "disallowed-methods" || k == "disallowed_methods"))
        .flat_map(|e| match &e.value {
            Value::Array(items) => items
                .iter()
                .filter_map(|v| v.as_str().or_else(|| v.field("path").and_then(Value::as_str)))
                .map(str::to_string)
                .collect(),
            _ => Vec::new(),
        })
        .collect()
}

#[must_use]
pub fn clippy_refusals(shown: &str, entries: &[Entry]) -> Vec<Refusal> {
    let have = listed_methods(entries);
    DISALLOWED_METHODS
        .iter()
        .filter(|m| !have.iter().any(|h| h == *m))
        .map(|m| {
            refusal(
                Rule::Config,
                shown,
                1,
                format!(
                    "no longer lists `{m}` in disallowed-methods: the strict and lossy reads stay denied outside srcio"
                ),
            )
        })
        .collect()
}

#[must_use]
pub fn rustfmt_root_refusals(root: &Path) -> Vec<Refusal> {
    let p = root.join(ROOT_RUSTFMT);
    let mut out = Vec::new();
    if !p.is_file() {
        out.push(refusal(
            Rule::Config,
            ROOT_RUSTFMT,
            1,
            "is missing: without it rustfmt reads whatever config is nearest each file".to_string(),
        ));
        return out;
    }
    if let Some(e) = parsed(ROOT_RUSTFMT, &p, &mut out) {
        out.extend(rustfmt_refusals(ROOT_RUSTFMT, &e));
    }
    out
}

#[must_use]
pub fn root_refusals(root: &Path) -> Vec<Refusal> {
    let mut out = rustfmt_root_refusals(root);
    let clippy = root.join(ROOT_CLIPPY);
    if clippy.is_file() {
        if let Some(e) = parsed(ROOT_CLIPPY, &clippy, &mut out) {
            out.extend(clippy_refusals(ROOT_CLIPPY, &e));
        }
    } else {
        out.push(refusal(
            Rule::Config,
            ROOT_CLIPPY,
            1,
            "is missing: it is where disallowed-methods denies the strict and lossy reads"
                .to_string(),
        ));
    }
    if let Some(e) = parsed("Cargo.toml", &root.join("Cargo.toml"), &mut out) {
        out.extend(manifest_refusals("Cargo.toml", &e, false));
    }
    let mut configs: Vec<(String, std::path::PathBuf)> = CARGO_CONFIGS
        .iter()
        .map(|n| root.join(".cargo").join(n))
        .filter(|p| p.is_file())
        .map(|p| (crate::tree::relative(root, &p), p))
        .collect();
    configs.extend(
        crate::tree::ancestor_configs(root)
            .into_iter()
            .map(|p| (p.display().to_string(), p)),
    );
    for (shown, p) in configs {
        if let Some(e) = parsed(&shown, &p, &mut out) {
            out.extend(cargo_config_refusals(&shown, &e));
        }
    }
    out
}

#[must_use]
pub fn file_refusals(root: &Path, files: &[String]) -> Vec<Refusal> {
    let mut out = Vec::new();
    for rel in files {
        let name = name_of(rel);
        let path = root.join(rel);
        if is_toml(rel) {
            match read(&path) {
                Ok(text) => out.extend(toml_comments(rel, &text)),
                Err(e) => out.push(refusal(
                    Rule::Config,
                    rel,
                    1,
                    format!("cannot be read: {e}"),
                )),
            }
        }
        if RUSTFMT_CONFIGS.contains(&name) && rel != ROOT_RUSTFMT {
            out.push(refusal(
                Rule::Config,
                rel,
                1,
                "is a rustfmt config other than the root rustfmt.toml: rustfmt reads the nearest one, so it \
                 could switch formatting off for the files below it"
                    .to_string(),
            ));
        }
        if CLIPPY_CONFIGS.contains(&name) && rel != ROOT_CLIPPY {
            out.push(refusal(
                Rule::Config,
                rel,
                1,
                "is a clippy config other than the root clippy.toml: clippy reads the nearest one, so it \
                 could empty disallowed-methods for the crates below it"
                    .to_string(),
            ));
        }
        if name == crate::tree::CACHE_TAG {
            out.push(refusal(
                Rule::Cache,
                rel,
                1,
                "is a CACHEDIR.TAG outside the cargo target: only target/ and the lint's own build directory \
                 are skipped, and only when cargo's signature heads their tag"
                    .to_string(),
            ));
        }
        if name == "Cargo.toml"
            && rel != "Cargo.toml"
            && let Some(e) = parsed(rel, &path, &mut out)
        {
            out.extend(manifest_refusals(rel, &e, true));
        }
        if is_cargo_config(rel)
            && !CARGO_CONFIGS.iter().any(|n| rel == &format!(".cargo/{n}"))
            && let Some(e) = parsed(rel, &path, &mut out)
        {
            out.extend(cargo_config_refusals(rel, &e));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn whats(r: &[Refusal]) -> Vec<String> {
        r.iter().map(ToString::to_string).collect()
    }

    #[test]
    fn the_rustfmt_config_sets_only_listed_keys_however_spelled() {
        let ok =
            tomlread::parse("newline_style = \"Unix\"\nuse_field_init_shorthand = true\n").unwrap();
        assert!(rustfmt_refusals("rustfmt.toml", &ok).is_empty());
        for bad in [
            "disable_all_formatting = true\n",
            "\"disable_all_formatting\" = true\n",
            "'ignore' = [\"crates\"]\n",
            "\"disable\\u005fall_formatting\" = true\n",
            "[x]\nnewline_style = \"Unix\"\n",
            "newline_style = [\"Unix\"]\n",
        ] {
            let e = tomlread::parse(bad).unwrap();
            assert!(!rustfmt_refusals("rustfmt.toml", &e).is_empty(), "{bad}");
        }
    }

    #[test]
    fn a_lint_level_that_silences_the_srcio_lint_is_refused_in_a_manifest() {
        let root = tomlread::parse(
            "[workspace.lints.clippy]\nunwrap_used = \"deny\"\ndisallowed_methods = \"allow\"\n\
             style = { level = \"allow\", priority = 1 }\n[workspace.lints.rust]\nwarnings = \"allow\"\n",
        )
        .unwrap();
        assert_eq!(manifest_refusals("Cargo.toml", &root, false).len(), 3);
        let member =
            tomlread::parse("[package]\nname = \"x\"\n[lints]\nworkspace = true\n").unwrap();
        assert!(manifest_refusals("crates/x/Cargo.toml", &member, true).is_empty());
        let over = tomlread::parse(
            "[package]\nname = \"x\"\n[lints]\nworkspace = true\n[lints.clippy]\nunwrap_used = \"allow\"\n",
        )
        .unwrap();
        let r = whats(&manifest_refusals("crates/x/Cargo.toml", &over, true));
        assert_eq!(r.len(), 2, "{r:?}");
        assert!(r[0].contains("lints.clippy"), "{r:?}");
        let none = tomlread::parse("[package]\nname = \"x\"\n").unwrap();
        let r = whats(&manifest_refusals("crates/x/Cargo.toml", &none, true));
        assert!(r[0].contains("no `[lints] workspace = true`"), "{r:?}");
        let dotted = tomlread::parse(
            "lints.clippy.disallowed_methods = \"allow\"\n[package]\nname = \"x\"\n",
        )
        .unwrap();
        assert!(!manifest_refusals("crates/x/Cargo.toml", &dotted, true).is_empty());
    }

    #[test]
    fn a_member_target_path_that_leaves_its_crate_is_refused() {
        let head = "[package]\nname = \"x\"\n[lints]\nworkspace = true\n";
        let ok = tomlread::parse(&format!(
            "{head}[lib]\npath = \"src/lib.rs\"\n[[bin]]\nname = \"a\"\npath = \"./src/bin/../a.rs\"\n\
             [dependencies]\nb = {{ path = \"../b\" }}\n"
        ))
        .unwrap();
        assert!(manifest_refusals("crates/x/Cargo.toml", &ok, true).is_empty());
        for (bad, key) in [
            ("[lib]\npath = \"../y/src/lib.rs\"\n", "lib.path"),
            (
                "[[test]]\nname = \"t\"\npath = \"/abs/t.rs\"\n",
                "test.path",
            ),
            (
                "bench = [{ name = \"b\", path = \"src/../../b.rs\" }]\n",
                "bench.path",
            ),
            ("example = { path = 'C:\\x.rs' }\n", "example.path"),
        ] {
            let e = tomlread::parse(&format!("{bad}{head}")).unwrap();
            let r = whats(&manifest_refusals("crates/x/Cargo.toml", &e, true));
            assert_eq!(r.len(), 1, "{bad}: {r:?}");
            assert!(
                r[0].contains(&format!("`{key} = "))
                    && r[0].contains("leaves the crate's directory"),
                "{bad}: {r:?}"
            );
        }
        let e = tomlread::parse(
            "[package]\nname = \"x\"\nbuild = \"../build.rs\"\n[lints]\nworkspace = true\n",
        )
        .unwrap();
        let r = whats(&manifest_refusals("crates/x/Cargo.toml", &e, true));
        assert!(
            r.len() == 1 && r[0].contains("`package.build = \"../build.rs\"`"),
            "{r:?}"
        );
        assert!(!leaves_its_directory("src/./a/../b.rs") && leaves_its_directory("a/../../b"));
    }

    #[test]
    fn a_package_description_that_repeats_a_separator_is_refused() {
        let ok = tomlread::parse(
            "[package]\nname = \"x\"\ndescription = \"the TU partition, bodies located\"\n\
             [lints]\nworkspace = true\n",
        )
        .unwrap();
        assert!(manifest_refusals("crates/x/Cargo.toml", &ok, true).is_empty());
        for (bad, sep) in [
            ("the TU partition,, bodies located", ",,"),
            ("a, , b", ", ,"),
            ("a;; b", ";;"),
            ("a  b", "  "),
        ] {
            let e = tomlread::parse(&format!(
                "[package]\nname = \"x\"\ndescription = \"{bad}\"\n[lints]\nworkspace = true\n"
            ))
            .unwrap();
            let r = whats(&manifest_refusals("crates/x/Cargo.toml", &e, true));
            assert_eq!(r.len(), 1, "{bad}: {r:?}");
            assert!(
                r[0].contains(&format!("repeats a separator (`{sep}`)")),
                "{bad}: {r:?}"
            );
        }
    }

    #[test]
    fn a_hyphenated_or_renamed_srcio_lint_name_is_refused_in_a_manifest() {
        for (text, n) in [
            (
                "[workspace.lints.clippy]\ndisallowed-methods = \"allow\"\n",
                1,
            ),
            (
                "[workspace.lints.clippy]\n\"disallowed-methods\" = { level = \"allow\", priority = 1 }\n",
                1,
            ),
            (
                "[workspace.lints.clippy]\ndisallowed_method = \"allow\"\n\
                 [workspace.lints.rust]\nrenamed_and_removed_lints = \"allow\"\n",
                2,
            ),
            (
                "[workspace.lints.rust]\nrenamed-and-removed-lints = \"allow\"\n",
                1,
            ),
        ] {
            let e = tomlread::parse(text).unwrap();
            let r = whats(&manifest_refusals("Cargo.toml", &e, false));
            assert_eq!(r.len(), n, "{text}: {r:?}");
        }
    }

    #[test]
    fn a_cargo_config_that_swaps_the_compiler_or_sets_the_lint_s_environment_is_refused() {
        let e = tomlread::parse(
            "[build]\nrustc = \"/x/rustc\"\nrustc-wrapper = \"w\"\nrustc_workspace_wrapper = \"w\"\n\
             jobs = 4\n[env]\nCLIPPY_CONF_DIR = \"/x\"\nCLIPPY_ARGS = { value = \"-Aclippy::all\", force = true }\n\
             OTHER = \"fine\"\n",
        )
        .unwrap();
        let r = whats(&cargo_config_refusals(".cargo/config.toml", &e));
        assert_eq!(r.len(), 5, "{r:?}");
        assert!(
            r[0].contains("`build.rustc` replaces the compiler cargo runs"),
            "{r:?}"
        );
        assert!(
            r[3].contains("`env.CLIPPY_CONF_DIR` sets CLIPPY_CONF_DIR for every compile"),
            "{r:?}"
        );
        assert!(r[4].contains("`env.CLIPPY_ARGS` sets CLIPPY_ARGS"), "{r:?}");
    }

    #[test]
    fn the_cargo_home_config_is_read_when_it_is_no_ancestor_of_the_tree() {
        let base =
            std::env::temp_dir().join(format!("msvc6-lint-cargohome-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&base);
        let root = base.join("tree");
        let home = base.join("home");
        std::fs::create_dir_all(&root).unwrap();
        std::fs::create_dir_all(&home).unwrap();
        crate::srcio::own_write(home.join("config.toml"), "[build]\nrustc-wrapper = \"w\"\n")
            .unwrap();
        let found = crate::tree::configs_for(&root, Some(&home));
        assert_eq!(
            found,
            [std::fs::canonicalize(home.join("config.toml")).unwrap()]
        );
        let none = crate::tree::configs_for(&root, None);
        assert!(none.is_empty(), "{none:?}");
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn rustflags_carry_no_lint_level() {
        let e = tomlread::parse(
            "[build]\nrustflags = [\"-C\", \"opt-level=1\", \"-Aclippy::disallowed_methods\"]\n\
             [target.'cfg(all())']\nrustflags = \"--cap-lints allow\"\n[host]\nrustflags = [\"-A\", \"warnings\"]\n",
        )
        .unwrap();
        let r = whats(&cargo_config_refusals(".cargo/config.toml", &e));
        assert_eq!(r.len(), 3, "{r:?}");
        assert!(r[0].contains("-Aclippy::disallowed_methods"), "{r:?}");
        assert!(
            !is_lint_flag("-Copt-level=2") && !is_lint_flag("--cfg") && !is_lint_flag("--allow-x")
        );
        assert!(is_lint_flag("--allow=warnings") && is_lint_flag("-Dwarnings"));
    }

    #[test]
    fn clippy_toml_must_list_every_denied_read() {
        let all: Vec<String> = DISALLOWED_METHODS
            .iter()
            .map(|m| format!("{{ path = \"{m}\", reason = \"r\" }}"))
            .collect();
        let full =
            tomlread::parse(&format!("disallowed-methods = [{}]\n", all.join(", "))).unwrap();
        assert!(clippy_refusals("clippy.toml", &full).is_empty());
        let empty = tomlread::parse("disallowed-methods = []\n").unwrap();
        assert_eq!(clippy_refusals("clippy.toml", &empty).len(), 6);
        let gone = tomlread::parse("allow-unwrap-in-tests = true\n").unwrap();
        assert_eq!(clippy_refusals("clippy.toml", &gone).len(), 6);
    }

    #[test]
    fn config_files_are_named_by_their_place() {
        assert!(is_cargo_config(".cargo/config.toml") && is_cargo_config("crates/x/.cargo/config"));
        assert!(!is_cargo_config("crates/x/config") && !is_cargo_config("config.toml"));
        assert!(is_toml("a/b.toml") && !is_toml("Cargo.lock"));
    }
}
