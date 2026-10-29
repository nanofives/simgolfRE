#[cfg(test)]
mod t {
    use msvc6_base::srcio;
    use std::path::{Path, PathBuf};
    use std::process::{Command, Output};

    const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

    const SPEC: &str = "message = \"Release\"\n\
                        include = [\"Cargo.toml\", \"crates\", \"examples\"]\n\
                        exclude = [\"examples/*/README.md\"]\n\n\
                        [overlay]\n\
                        \"README.md\" = \"release/README.md\"\n\
                        \"LICENSE\" = \"release/LICENSE\"\n";

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6t-publish-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn put(root: &Path, rel: &str, bytes: &[u8]) {
        let p = root.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        srcio::write_bytes(&p, bytes).unwrap();
    }

    fn quiet(c: &mut Command) -> &mut Command {
        c.env_remove("GIT_DIR")
            .env_remove("GIT_WORK_TREE")
            .env_remove("GIT_INDEX_FILE")
            .env("GIT_AUTHOR_NAME", "t")
            .env("GIT_AUTHOR_EMAIL", "t@t")
            .env("GIT_COMMITTER_NAME", "t")
            .env("GIT_COMMITTER_EMAIL", "t@t")
            .env("GIT_CONFIG_COUNT", "1")
            .env("GIT_CONFIG_KEY_0", "commit.gpgsign")
            .env("GIT_CONFIG_VALUE_0", "false")
    }

    fn git(repo: &Path, args: &[&str]) -> String {
        let o = quiet(Command::new("git").arg("-C").arg(repo).args(args))
            .output()
            .unwrap();
        assert!(
            o.status.success(),
            "git {args:?}: {}",
            srcio::tool_text(&o.stderr)
        );
        srcio::tool_text(&o.stdout).trim().to_string()
    }

    fn publish(repo: &Path, args: &[&str]) -> Output {
        quiet(
            Command::new(BIN)
                .env_remove("MSVC6T_CONFIG")
                .arg("publish")
                .arg("--repo")
                .arg(repo)
                .args(args),
        )
        .output()
        .unwrap()
    }

    fn text(o: &Output) -> String {
        srcio::tool_text(&o.stdout) + &srcio::tool_text(&o.stderr)
    }

    fn repo(d: &Path) -> PathBuf {
        let r = d.join("repo");
        put(&r, "Cargo.toml", b"[workspace]\n");
        put(&r, "crates/a/src/lib.rs", b"pub fn a() {}\n");
        put(&r, "docs/x.md", b"# X\n");
        put(&r, "examples/p/README.md", b"# P\n");
        put(&r, "examples/p/cfg.toml", b"id = \"P\"\n");
        put(&r, "README.md", b"# Development\n");
        put(&r, "release/README.md", b"# Release\n");
        put(&r, "release/LICENSE", b"Licence text\n");
        put(&r, "release/publish.toml", SPEC.as_bytes());
        git(&r, &["init", "-q", "-b", "main"]);
        git(&r, &["add", "-A"]);
        git(&r, &["commit", "-q", "-m", "one"]);
        r
    }

    #[test]
    fn a_release_holds_the_spec_s_files_and_regenerates_to_the_same_tree() {
        let d = scratch("cut");
        let r = repo(&d);
        let o = publish(&r, &["--list"]);
        assert!(o.status.success(), "{}", text(&o));
        let out = text(&o);
        assert!(out.contains("README.md <- release/README.md"), "{out}");
        assert!(out.contains("publish: release/publish.toml at "), "{out}");
        assert!(
            out.contains("5 file(s)") && out.contains("crates 1, examples 1"),
            "{out}"
        );
        assert!(git(&r, &["branch", "--list", "rel"]).is_empty());
        let o = publish(&r, &["--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        assert!(
            text(&o).contains("publish: rel created at "),
            "{}",
            text(&o)
        );
        let files = git(&r, &["ls-tree", "-r", "--name-only", "rel"]);
        assert_eq!(
            files.lines().collect::<Vec<_>>(),
            [
                "Cargo.toml",
                "LICENSE",
                "README.md",
                "crates/a/src/lib.rs",
                "examples/p/cfg.toml"
            ]
        );
        assert_eq!(git(&r, &["show", "rel:README.md"]), "# Release");
        assert_eq!(git(&r, &["log", "-1", "--format=%s%n%P", "rel"]), "Release");
        let first = git(&r, &["rev-parse", "rel"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(text(&o).contains("unchanged"), "{}", text(&o));
        assert_eq!(git(&r, &["rev-parse", "rel"]), first);
        put(&r, "docs/x.md", b"# X, again\n");
        git(&r, &["commit", "-q", "-am", "docs"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(text(&o).contains("unchanged"), "{}", text(&o));
        put(&r, "crates/a/src/lib.rs", b"pub fn a() -> u8 { 1 }\n");
        git(&r, &["commit", "-q", "-am", "code"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        assert!(text(&o).contains("publish: rel replaced: "), "{}", text(&o));
        let second = git(&r, &["rev-parse", "rel"]);
        assert_ne!(second, first);
        assert_eq!(git(&r, &["rev-list", "--count", "rel"]), "1");
        assert_eq!(
            git(&r, &["show", "rel:crates/a/src/lib.rs"]),
            "pub fn a() -> u8 { 1 }"
        );
        let o = publish(&r, &["--rev", "HEAD~2"]);
        assert!(
            text(&o).contains(&format!(
                "tree {}",
                git(&r, &["rev-parse", &format!("{first}^{{tree}}")])
            )),
            "{}",
            text(&o)
        );
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_branch_with_the_source_s_history_or_checked_out_is_refused() {
        let d = scratch("refuse");
        let r = repo(&d);
        let o = publish(&r, &["--branch", "main"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(text(&o).contains("is checked out"), "{}", text(&o));
        git(&r, &["branch", "lane"]);
        let o = publish(&r, &["--branch", "lane"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(text(&o).contains("shares history"), "{}", text(&o));
        let o = publish(&r, &["--branch", "bad..name"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(text(&o).contains("not a branch name"), "{}", text(&o));
        put(
            &r,
            "release/publish.toml",
            b"message = \"r\"\ninclude = [\"gone\"]\n",
        );
        git(&r, &["commit", "-q", "-am", "stale"]);
        let o = publish(&r, &[]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("name nothing at the commit: gone"),
            "{}",
            text(&o)
        );
        let o = publish(&r, &["--spec", "release/none.toml"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("has no release/none.toml"),
            "{}",
            text(&o)
        );
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn a_symlink_or_a_screened_text_keeps_the_release_from_being_cut() {
        let d = scratch("screen");
        let r = repo(&d);
        let screened = format!(
            "{SPEC}\n[screen]\nrefuse = [\"/home/\"]\nprose = \"overlays\"\ncomments = []\n"
        );
        put(&r, "release/publish.toml", screened.as_bytes());
        git(&r, &["commit", "-q", "-am", "screen"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        assert!(
            text(&o).contains("publish: screen: 5 file(s) clean"),
            "{}",
            text(&o)
        );
        let tip = git(&r, &["rev-parse", "rel"]);
        put(
            &r,
            "crates/a/src/lib.rs",
            b"pub const P: &str = \"/home/me/x\";\n",
        );
        git(&r, &["commit", "-q", "-am", "home"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("crates/a/src/lib.rs:1: `/home/me/x` names /home/"),
            "{}",
            text(&o)
        );
        put(&r, "crates/a/src/lib.rs", b"pub fn a() {}\n");
        put(&r, "crates/a/src/home/me.rs", b"pub fn m() {}\n");
        git(&r, &["add", "-A"]);
        git(&r, &["commit", "-q", "-m", "path"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("crates/a/src/home/me.rs: its path names /home/ (`/home/me.rs`)"),
            "{}",
            text(&o)
        );
        std::fs::remove_file(r.join("crates/a/src/home/me.rs")).unwrap();
        put(&r, "crates/a/src/c.c", b"int c; /* why */\n");
        git(&r, &["add", "-A"]);
        git(&r, &["commit", "-q", "-m", "comment"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("crates/a/src/c.c:1: a comment"),
            "{}",
            text(&o)
        );
        std::fs::remove_file(r.join("crates/a/src/c.c")).unwrap();
        std::os::unix::fs::symlink("/etc/hostname", r.join("crates/a/src/host")).unwrap();
        git(&r, &["add", "-A"]);
        git(&r, &["commit", "-q", "-m", "link"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("crates/a/src/host: mode 120000, not a regular file"),
            "{}",
            text(&o)
        );
        assert_eq!(git(&r, &["rev-parse", "rel"]), tip);
        std::fs::remove_dir_all(&d).unwrap();
    }

    const SOURCE_ONLY: &str = "message = \"Initial commit\"\n\
                               credits = \"release/README.md\"\n\
                               include = [\"Cargo.toml\", \"crates\", \"examples\"]\n\
                               exclude = [\"examples/*/README.md\"]\n\n\
                               [overlay]\n\
                               \"LICENSE\" = \"release/LICENSE\"\n\n\
                               [screen]\n\
                               prose = \"overlays\"\n\n\
                               [screen.readers]\n\
                               \"Cargo.toml\" = \"cargo\"\n\
                               \"crates/**/*.rs\" = \"rustc\"\n\
                               \"examples/*/cfg.toml\" = \"crates/a/tests/examples.rs\"\n\
                               \"LICENSE\" = \"the licence\"\n";

    #[test]
    fn a_readme_or_a_docs_file_no_reader_rule_names_keeps_the_release_from_being_cut() {
        let d = scratch("readers");
        let r = repo(&d);
        put(
            &r,
            "release/README.md",
            b"# Release\n\n## Credits\n\n- one.\n",
        );
        put(&r, "release/publish.toml", SOURCE_ONLY.as_bytes());
        git(&r, &["commit", "-q", "-am", "source only"]);
        let o = publish(&r, &["--list", "--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        let out = text(&o);
        assert!(
            out.contains("examples/p/cfg.toml\tcrates/a/tests/examples.rs"),
            "{out}"
        );
        assert!(
            out.contains("LICENSE <- release/LICENSE\tthe licence"),
            "{out}"
        );
        assert!(
            out.contains(
                "publish: screen: 4 file(s) clean, each with a reader 4 `screen.readers` rule(s) name"
            ),
            "{out}"
        );
        assert_eq!(
            git(&r, &["ls-tree", "-r", "--name-only", "rel"])
                .lines()
                .collect::<Vec<_>>(),
            [
                "Cargo.toml",
                "LICENSE",
                "crates/a/src/lib.rs",
                "examples/p/cfg.toml"
            ]
        );
        assert_eq!(
            git(&r, &["log", "-1", "--format=%B", "rel"]),
            "Initial commit\n\n- one."
        );
        let tip = git(&r, &["rev-parse", "rel"]);
        for (what, spec, plant) in [
            (
                "README.md",
                SOURCE_ONLY.replace(
                    "[overlay]\n",
                    "[overlay]\n\"README.md\" = \"release/README.md\"\n",
                ),
                None,
            ),
            (
                "docs/x.md",
                SOURCE_ONLY.replace("\"examples\"]", "\"examples\", \"docs\"]"),
                None,
            ),
            (
                "crates/a/README.md",
                SOURCE_ONLY.to_string(),
                Some("crates/a/README.md"),
            ),
        ] {
            put(&r, "release/publish.toml", spec.as_bytes());
            if let Some(p) = plant {
                put(&r, p, b"# a\n\nHow it works.\n");
            }
            git(&r, &["add", "-A"]);
            git(&r, &["commit", "-q", "-m", what]);
            let o = publish(&r, &["--branch", "rel"]);
            assert_eq!(o.status.code(), Some(1), "{what}: {}", text(&o));
            assert!(
                text(&o).contains(&format!(
                    "{what}: no `screen.readers` rule names what reads it"
                )),
                "{what}: {}",
                text(&o)
            );
            assert_eq!(git(&r, &["rev-parse", "rel"]), tip, "{what}");
            if let Some(p) = plant {
                std::fs::remove_file(r.join(p)).unwrap();
            }
        }
        put(
            &r,
            "release/publish.toml",
            SOURCE_ONLY
                .replace("release/README.md", "release/NONE.md")
                .as_bytes(),
        );
        git(&r, &["add", "-A"]);
        git(&r, &["commit", "-q", "-m", "no credits file"]);
        let o = publish(&r, &[]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("`credits` = `release/NONE.md`: the commit holds no such file"),
            "{}",
            text(&o)
        );
        std::fs::remove_dir_all(&d).unwrap();
    }

    fn raw_commit(repo: &Path, rev: &str) -> String {
        git(repo, &["cat-file", "commit", rev])
    }

    #[test]
    fn a_release_is_one_anonymous_root_commit_with_the_readme_s_credits() {
        let d = scratch("anon");
        let r = repo(&d);
        let spec = SPEC.replace(
            "message = \"Release\"\n",
            "message = \"Initial commit\"\nauthor = \"Anonymous <anonymous@example.invalid>\"\ncredits = \"release/README.md\"\n",
        );
        put(&r, "release/publish.toml", spec.as_bytes());
        put(
            &r,
            "release/README.md",
            b"# Release\n\nText.\n\n## Credits\n\n- [one](https://one.example): the first.\n- two.\n",
        );
        git(&r, &["commit", "-q", "-am", "credits"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        assert!(
            text(&o).contains(
                "publish: commit: Anonymous <anonymous@example.invalid> +0000, message `Initial commit` and \
                 release/README.md's Credits section"
            ),
            "{}",
            text(&o)
        );
        let raw = raw_commit(&r, "rel");
        assert!(!raw.contains("\nparent "), "{raw}");
        for role in ["author", "committer"] {
            let l = raw
                .lines()
                .find(|l| l.starts_with(role))
                .unwrap_or_default()
                .to_string();
            assert!(
                l.starts_with(&format!("{role} Anonymous <anonymous@example.invalid> "))
                    && l.ends_with(" +0000"),
                "{l}"
            );
        }
        assert_eq!(
            git(&r, &["log", "-1", "--format=%B", "rel"]),
            "Initial commit\n\n- [one](https://one.example): the first.\n- two."
        );
        let first = git(&r, &["rev-parse", "rel"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(text(&o).contains("unchanged"), "{}", text(&o));
        assert_eq!(git(&r, &["rev-parse", "rel"]), first);
        let o = publish(&r, &["--check", "--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        assert!(text(&o).contains("one root commit"), "{}", text(&o));
        let tree = git(&r, &["rev-parse", "rel^{tree}"]);
        let forged = git(
            &r,
            &["commit-tree", &tree, "-p", &first, "-m", "Initial commit"],
        );
        git(&r, &["update-ref", "refs/heads/rel", &forged, &first]);
        let o = publish(&r, &["--check", "--branch", "rel"]);
        assert_eq!(o.status.code(), Some(1));
        let out = text(&o);
        assert!(out.contains("it has 1 parent(s)"), "{out}");
        assert!(out.contains("its author is `t <t@t>"), "{out}");
        assert!(out.contains("its committer is `t <t@t>"), "{out}");
        assert!(
            out.contains(
                "its message is not the spec's: line 3: (its end) where the spec gives `- [one]"
            ),
            "{out}"
        );
        let o = publish(&r, &["--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        assert!(text(&o).contains("publish: rel replaced: "), "{}", text(&o));
        assert_eq!(git(&r, &["rev-list", "--count", "rel"]), "1");
        put(&r, "release/README.md", b"# Release\n\nNo credits.\n");
        git(&r, &["commit", "-q", "-am", "none"]);
        let o = publish(&r, &[]);
        assert_eq!(o.status.code(), Some(1));
        assert!(
            text(&o).contains("`credits` = `release/README.md`: no `Credits` heading"),
            "{}",
            text(&o)
        );
        std::fs::remove_dir_all(&d).unwrap();
    }

    fn raw_bytes(repo: &Path, rev: &str) -> Vec<u8> {
        let o = quiet(
            Command::new("git")
                .arg("-C")
                .arg(repo)
                .args(["cat-file", "commit", rev]),
        )
        .output()
        .unwrap();
        assert!(o.status.success(), "{}", srcio::tool_text(&o.stderr));
        o.stdout
    }

    #[test]
    fn a_tip_with_another_header_is_refused_by_check_and_replaced_by_publish() {
        let d = scratch("header");
        let r = repo(&d);
        let spec = SPEC.replace(
            "message = \"Release\"\n",
            "message = \"Initial commit\"\nauthor = \"Anonymous <anonymous@example.invalid>\"\n",
        );
        put(&r, "release/publish.toml", spec.as_bytes());
        git(&r, &["commit", "-q", "-am", "anonymous"]);
        let o = publish(&r, &["--branch", "rel"]);
        assert!(o.status.success(), "{}", text(&o));
        let good = srcio::tool_text(&raw_bytes(&r, "rel"));
        let tree = git(&r, &["rev-parse", "rel^{tree}"]);
        for (name, extra) in [
            (
                "gpgsig",
                "gpgsig -----BEGIN PGP SIGNATURE-----\n \n iQEzBAABCAAdFiEE\n -----END PGP SIGNATURE-----\n",
            ),
            ("encoding", "encoding ISO-8859-1\n"),
            (
                "mergetag",
                "mergetag object TREE\n type tree\n tag v1\n tagger t <t@t> 0 +0000\n \n v1\n",
            ),
        ] {
            let extra = extra.replace("TREE", &tree);
            let planted = good.replacen("\n\n", &format!("\n{extra}\n"), 1);
            put(&d, &format!("{name}.commit"), planted.as_bytes());
            let file = d.join(format!("{name}.commit")).display().to_string();
            let id = git(&r, &["hash-object", "-t", "commit", "-w", &file]);
            let old = git(&r, &["rev-parse", "rel"]);
            git(&r, &["update-ref", "refs/heads/rel", &id, &old]);
            let o = publish(&r, &["--check", "--branch", "rel"]);
            assert_eq!(o.status.code(), Some(1), "{name}: {}", text(&o));
            let first = extra.lines().next().unwrap_or_default();
            assert!(
                text(&o).contains("it is not the commit the spec gives at its own time, ")
                    && text(&o).contains(&format!("line 4: `{first}` where the spec gives ``")),
                "{name}: {}",
                text(&o)
            );
            let o = publish(&r, &["--branch", "rel"]);
            assert!(o.status.success(), "{name}: {}", text(&o));
            assert!(
                text(&o).contains("publish: rel replaced: "),
                "{name}: {}",
                text(&o)
            );
            let now = srcio::tool_text(&raw_bytes(&r, "rel"));
            assert!(!now.contains(name), "{name}: {now}");
            let o = publish(&r, &["--check", "--branch", "rel"]);
            assert!(o.status.success(), "{name}: {}", text(&o));
        }
        std::fs::remove_dir_all(&d).unwrap();
    }
}
