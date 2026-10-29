#[must_use]
pub fn matches(pattern: &str, path: &str) -> bool {
    let p: Vec<char> = pattern.chars().collect();
    let s: Vec<char> = path.chars().collect();
    let anchored = p.contains(&'/');
    if anchored {
        return walk(&p, &s, true);
    }
    let name = path.rsplit('/').next().unwrap_or(path);
    walk(&p, &name.chars().collect::<Vec<char>>(), true)
}

#[must_use]
pub fn whole(pattern: &str, path: &str) -> bool {
    let p: Vec<char> = pattern.chars().collect();
    let s: Vec<char> = path.chars().collect();
    walk(&p, &s, false)
}

fn walk(p: &[char], s: &[char], fold: bool) -> bool {
    match p.split_first() {
        None => s.is_empty(),
        Some(('*', rest)) if rest.first() == Some(&'*') => {
            let rest = rest.get(1..).unwrap_or_default();
            let rest = rest.strip_prefix(&['/']).unwrap_or(rest);
            (0..=s.len()).any(|k| {
                let tail = s.get(k..).unwrap_or_default();
                (k == 0 || s.get(k - 1) == Some(&'/') || rest.is_empty()) && walk(rest, tail, fold)
            })
        }
        Some(('*', rest)) => (0..=s.len())
            .take_while(|&k| k == 0 || s.get(k - 1) != Some(&'/'))
            .any(|k| walk(rest, s.get(k..).unwrap_or_default(), fold)),
        Some(('?', rest)) => s
            .split_first()
            .is_some_and(|(c, tail)| *c != '/' && walk(rest, tail, fold)),
        Some((c, rest)) => s.split_first().is_some_and(|(d, tail)| {
            (if fold {
                c.eq_ignore_ascii_case(d)
            } else {
                c == d
            }) && walk(rest, tail, fold)
        }),
    }
}

#[must_use]
pub fn under(prefix: &str, path: &str) -> bool {
    let prefix = prefix.trim_end_matches('/');
    path == prefix
        || path
            .strip_prefix(prefix)
            .is_some_and(|r| r.starts_with('/'))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_pattern_without_a_slash_matches_the_file_name() {
        assert!(matches("*.ksy", "spec/a.ksy"));
        assert!(matches("*.CPP", "src/x.cpp"));
        assert!(!matches("*.ksy", "spec/a.ksyx"));
        assert!(matches("a?c.h", "x/abc.h"));
    }

    #[test]
    fn a_pattern_with_a_slash_matches_the_whole_path() {
        assert!(matches("tool/*.txt", "tool/a.txt"));
        assert!(!matches("tool/*.txt", "tool/sub/a.txt"));
        assert!(matches("tool/**/*.txt", "tool/sub/deep/a.txt"));
        assert!(matches("tool/**/*.txt", "tool/a.txt"));
        assert!(matches("lib/src/*.cpp", "lib/src/Widget.cpp"));
        assert!(!matches("lib/src/*.cpp", "other/lib/src/Widget.cpp"));
        assert!(matches("**", "any/thing"));
    }

    #[test]
    fn a_whole_path_pattern_without_a_slash_names_a_root_file() {
        assert!(whole("Cargo.lock", "Cargo.lock"));
        assert!(!whole("cargo.LOCK", "Cargo.lock"));
        assert!(!whole("crates/*/Cargo.toml", "crates/a/CARGO.TOML"));
        assert!(!whole("fixtures/gate/caller.c", "fixtures/gate/CALLER.C"));
        assert!(matches("crates/*/Cargo.toml", "crates/a/CARGO.TOML"));
        assert!(!whole("Cargo.lock", "crates/a/Cargo.lock"));
        assert!(!whole(
            "rust-toolchain.toml",
            "fixtures/rust-toolchain.toml"
        ));
        assert!(!whole("*.toml", "fixtures/a.toml"));
        assert!(whole("*.toml", "rustfmt.toml"));
        assert!(whole("crates/*/Cargo.toml", "crates/a/Cargo.toml"));
        assert!(!whole("crates/*/Cargo.toml", "crates/a/b/Cargo.toml"));
        assert!(whole("crates/**/*.rs", "crates/a/src/x/y.rs"));
        assert!(whole("**/Cargo.lock", "crates/a/Cargo.lock"));
    }

    #[test]
    fn under_is_a_component_prefix() {
        assert!(under("tools", "tools/a.rs"));
        assert!(under("tools/", "tools"));
        assert!(!under("tools", "toolsx/a.rs"));
    }
}
