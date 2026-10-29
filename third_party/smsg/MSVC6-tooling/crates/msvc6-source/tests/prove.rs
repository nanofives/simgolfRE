#[cfg(test)]
mod t {
    use msvc6_base::srcio;
    use msvc6_source::config::{self, Loaded};
    use msvc6_source::prove::{self, CommandCompile, Input, Job};
    use std::io::Write as _;
    use std::path::{Path, PathBuf};
    use std::process::{Command, Stdio};

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-source-it-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn put(root: &Path, rel: &str, bytes: &[u8]) {
        let p = root.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        srcio::write_bytes(p, bytes).unwrap();
    }

    fn cp_config(extra: &str) -> Loaded {
        config::parse(
            &format!(
                "[source.prove]\ncompile = [\"cp\", \"{{source}}\", \"{{out}}\"]\n{extra}[[source.prove.tu]]\nsource = \"src/*.cpp\"\n"
            ),
            Path::new("/"),
            None,
        )
        .unwrap()
    }

    fn git(repo: &Path, args: &[&str], stdin: Option<&[u8]>) -> String {
        let mut c = Command::new("git");
        c.arg("-C")
            .arg(repo)
            .args([
                "-c",
                "user.name=t",
                "-c",
                "user.email=t@example.invalid",
                "-c",
                "commit.gpgsign=false",
            ])
            .args(args)
            .env_remove("GIT_DIR")
            .env_remove("GIT_WORK_TREE")
            .env_remove("GIT_INDEX_FILE")
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());
        let mut child = c.spawn().unwrap();
        if let Some(b) = stdin {
            child.stdin.take().unwrap().write_all(b).unwrap();
        }
        drop(child.stdin.take());
        let o = child.wait_with_output().unwrap();
        assert!(
            o.status.success(),
            "git {args:?}: {}",
            srcio::tool_text(&o.stderr)
        );
        srcio::tool_text(&o.stdout).trim().to_string()
    }

    #[cfg(unix)]
    #[test]
    fn a_cp_compiler_runs_at_one_path_for_both_states() {
        let d = scratch("cp");
        let (before, after, w) = (d.join("before"), d.join("after"), d.join("work"));
        put(&before, "src/a.cpp", b"int a;\n");
        put(&before, "src/b.cpp", b"int b;\n");
        put(&after, "src/a.cpp", b"int a;\n");
        put(&after, "src/b.cpp", b"int  b;\n");
        let cfg = cp_config("");
        let cc = CommandCompile::from_config(&cfg).unwrap();
        let input = Input::Dirs {
            before: before.clone(),
            after: after.clone(),
        };
        let r = prove::prove(&input, &cfg, &cc, &w, 2).unwrap();
        assert!(!r.holds);
        let b = r.tus.iter().find(|t| t.tu == "src/b.cpp").unwrap();
        assert_eq!(b.verdict, "moves");
        assert!(b.at.contains("the bytes decide"), "{}", b.at);
        put(&after, "src/b.cpp", b"int b;\n");
        put(&after, "notes.md", b"new\n");
        let r = prove::prove(&input, &cfg, &cc, &w, 2).unwrap();
        assert!(r.holds, "{r:?}");
        prove::release_work(&w).unwrap();
        assert!(!w.exists());
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn the_proof_clears_only_a_work_directory_it_marked() {
        let d = scratch("work");
        let (before, w) = (d.join("before"), d.join("work"));
        put(&before, "src/a.cpp", b"int a;\n");
        put(&w, "keep.txt", b"someone else's\n");
        let cfg = cp_config("");
        let cc = CommandCompile::from_config(&cfg).unwrap();
        let input = Input::Dirs {
            before: before.clone(),
            after: before.clone(),
        };
        let e = prove::prove(&input, &cfg, &cc, &w, 1).unwrap_err();
        assert!(format!("{e:#}").contains(prove::WORK_MARK), "{e:#}");
        assert!(w.join("keep.txt").is_file());
        prove::release_work(&w).unwrap();
        assert!(w.join("keep.txt").is_file());
        std::fs::remove_file(w.join("keep.txt")).unwrap();
        prove::claim_work(&w).unwrap();
        put(&w, "stale.obj", b"x");
        assert!(prove::prove(&input, &cfg, &cc, &w, 1).unwrap().holds);
        assert!(!w.join("stale.obj").exists());
        assert!(w.join(prove::WORK_MARK).is_file());
        prove::release_work(&w).unwrap();
        assert!(w.is_dir());
        assert_eq!(std::fs::read_dir(&w).unwrap().count(), 0);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_template_is_filled_in_one_pass_and_a_dash_source_is_not_an_option() {
        let c = CommandCompile {
            argv: vec![
                "cc".into(),
                "{source}".into(),
                "/Fo{out}.cpp".into(),
                "{source_win}".into(),
                "{nokey}".into(),
            ],
            env: Vec::new(),
        };
        let job = Job {
            root: PathBuf::from("/w/tree"),
            source: "src/-x.cpp".into(),
            cwd: PathBuf::from("/w/tree/src"),
            cflags: Vec::new(),
            scratch: PathBuf::from("/w/{source}"),
            out: PathBuf::from("/w/{source}/tu.obj"),
            slot: 0,
        };
        assert_eq!(
            c.args(&job),
            [
                "cc",
                "./-x.cpp",
                "/Fo/w/{source}/tu.obj.cpp",
                ".\\-x.cpp",
                "{nokey}"
            ]
        );
    }

    #[test]
    fn a_wine_compile_must_name_its_prefix() {
        let cfg = config::parse(
            "[source.prove]\ncompile = [\"/usr/bin/wine\", \"CL.EXE\"]\n",
            Path::new("/"),
            None,
        )
        .unwrap();
        let e = CommandCompile::from_config(&cfg).unwrap_err();
        assert!(format!("{e:#}").contains("WINEPREFIX"), "{e:#}");
        let cfg = config::parse(
            "[source.prove]\ncompile = [\"wine\", \"CL.EXE\"]\n[source.prove.env]\nWINEPREFIX = \"/p\"\n",
            Path::new("/"),
            None,
        )
        .unwrap();
        assert!(CommandCompile::from_config(&cfg).is_ok());
    }

    #[cfg(unix)]
    #[test]
    fn a_symlink_is_written_as_its_text_and_a_submodule_change_is_unproven() {
        let d = scratch("git");
        let (repo, w) = (d.join("repo"), d.join("work"));
        std::fs::create_dir_all(&repo).unwrap();
        git(&repo, &["init", "-q"], None);
        put(&repo, "src/a.cpp", b"int a;\n");
        std::os::unix::fs::symlink("../../escape", repo.join("src/link")).unwrap();
        git(&repo, &["add", "-A"], None);
        let one = "1".repeat(40);
        let two = "2".repeat(40);
        git(
            &repo,
            &[
                "update-index",
                "--add",
                "--cacheinfo",
                &format!("160000,{one},sub"),
            ],
            None,
        );
        git(&repo, &["commit", "-q", "-m", "before"], None);
        let before = git(&repo, &["rev-parse", "HEAD"], None);
        git(
            &repo,
            &["update-index", "--cacheinfo", &format!("160000,{two},sub")],
            None,
        );
        git(&repo, &["commit", "-q", "-m", "after"], None);
        let after = git(&repo, &["rev-parse", "HEAD"], None);
        let input = Input::Revs {
            repo: repo.clone(),
            before,
            after,
        };
        let cfg = cp_config("");
        let cc = CommandCompile::from_config(&cfg).unwrap();
        let r = prove::prove(&input, &cfg, &cc, &w, 1).unwrap();
        assert!(!r.holds, "{r:?}");
        let sub = r.changed.iter().find(|c| c.path == "sub").unwrap();
        assert_eq!(sub.class, "unproven");
        assert!(r.unproven.iter().any(|u| u.starts_with("sub:")), "{r:?}");
        let link = w.join("tree/src/link");
        assert!(link.symlink_metadata().unwrap().file_type().is_file());
        assert_eq!(std::fs::read(&link).unwrap(), b"../../escape");
        assert!(!d.join("escape").exists());
        let cfg = cp_config("unbuilt = [\"sub\"]\n");
        let r = prove::prove(&input, &cfg, &cc, &w, 1).unwrap();
        assert!(r.holds, "{r:?}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_tree_entry_named_dot_dot_is_refused() {
        let d = scratch("dotdot");
        let repo = d.join("repo");
        std::fs::create_dir_all(&repo).unwrap();
        git(&repo, &["init", "-q"], None);
        let blob = git(&repo, &["hash-object", "-w", "--stdin"], Some(b"x\n"));
        let tree = git(
            &repo,
            &["mktree"],
            Some(format!("100644 blob {blob}\t..\n").as_bytes()),
        );
        let commit = git(&repo, &["commit-tree", &tree, "-m", "x"], Some(b""));
        let dest = d.join("out");
        let e = msvc6_source::git::materialize(&repo, &commit, &dest).unwrap_err();
        assert!(format!("{e:#}").contains("leaves the tree"), "{e:#}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn the_compiler_options_variables_are_cleared_in_any_case() {
        let names = [
            "cl", "Cl", "_cl_", "LINK", "_Link_", "PATH", "clx", "INCLUDE",
        ]
        .map(std::ffi::OsString::from);
        let gone = prove::scrubbed(names);
        assert_eq!(
            gone,
            ["cl", "Cl", "_cl_", "LINK", "_Link_"].map(std::ffi::OsString::from)
        );
    }

    #[test]
    fn a_wine_anywhere_in_the_command_must_name_its_prefix() {
        for compile in [
            "[\"env\", \"WINEDEBUG=-all\", \"wine\", \"CL.EXE\"]",
            "[\"nice\", \"/usr/bin/wine64\", \"CL.EXE\"]",
            "[\"docker\", \"exec\", \"-i\", \"c\", \"Wine\", \"CL.EXE\"]",
        ] {
            let cfg = config::parse(
                &format!("[source.prove]\ncompile = {compile}\n"),
                Path::new("/"),
                None,
            )
            .unwrap();
            let e = CommandCompile::from_config(&cfg).unwrap_err();
            assert!(format!("{e:#}").contains("WINEPREFIX"), "{compile}: {e:#}");
        }
    }

    #[cfg(unix)]
    #[test]
    fn a_marker_must_be_the_proofs_own_regular_file() {
        let d = scratch("mark");
        let (before, w) = (d.join("before"), d.join("work"));
        put(&before, "src/a.cpp", b"int a;\n");
        put(&w, "top.txt", b"keep\n");
        put(&w, "sub/important.txt", b"keep\n");
        put(&d, "elsewhere.txt", b"x\n");
        std::os::unix::fs::symlink(d.join("elsewhere.txt"), w.join(prove::WORK_MARK)).unwrap();
        let cfg = cp_config("");
        let cc = CommandCompile::from_config(&cfg).unwrap();
        let input = Input::Dirs {
            before: before.clone(),
            after: before.clone(),
        };
        assert!(prove::prove(&input, &cfg, &cc, &w, 1).is_err());
        prove::release_work(&w).unwrap();
        assert!(w.join("top.txt").is_file());
        assert!(w.join("sub/important.txt").is_file());
        std::fs::remove_file(w.join(prove::WORK_MARK)).unwrap();
        put(&w, prove::WORK_MARK, b"not the proof's\n");
        assert!(prove::prove(&input, &cfg, &cc, &w, 1).is_err());
        prove::release_work(&w).unwrap();
        assert!(w.join("top.txt").is_file());
        let link = d.join("link");
        std::os::unix::fs::symlink(&w, &link).unwrap();
        assert!(prove::prove(&input, &cfg, &cc, &link, 1).is_err());
        assert!(w.join("top.txt").is_file());
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn an_empty_directory_the_user_made_is_kept() {
        let d = scratch("empty");
        let (before, w) = (d.join("before"), d.join("work"));
        put(&before, "src/a.cpp", b"int a;\n");
        std::fs::create_dir_all(&w).unwrap();
        let cfg = cp_config("");
        let cc = CommandCompile::from_config(&cfg).unwrap();
        let input = Input::Dirs {
            before: before.clone(),
            after: before.clone(),
        };
        assert!(prove::prove(&input, &cfg, &cc, &w, 1).unwrap().holds);
        assert!(prove::prove(&input, &cfg, &cc, &w, 1).unwrap().holds);
        prove::release_work(&w).unwrap();
        assert!(w.is_dir());
        assert_eq!(std::fs::read_dir(&w).unwrap().count(), 0);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn a_linked_directory_in_a_before_tree_is_not_walked() {
        let d = scratch("loop");
        let (before, w) = (d.join("before"), d.join("work"));
        put(&before, "src/a.cpp", b"int a;\n");
        std::os::unix::fs::symlink(&before, before.join("src/again")).unwrap();
        let cfg = cp_config("");
        let cc = CommandCompile::from_config(&cfg).unwrap();
        let input = Input::Dirs {
            before: before.clone(),
            after: before.clone(),
        };
        assert!(prove::prove(&input, &cfg, &cc, &w, 1).is_err());
        prove::release_work(&w).unwrap();
        std::fs::remove_dir_all(&d).unwrap();
    }

    fn wine_cfg(compile: &str, env: &str) -> Loaded {
        config::parse(
            &format!("[source.prove]\ncompile = {compile}\n[source.prove.env]\n{env}"),
            Path::new("/"),
            None,
        )
        .unwrap()
    }

    #[test]
    fn a_wine_compile_by_any_spelling_must_name_a_prefix_that_sets_no_cl() {
        for (compile, env) in [
            ("[\"env\", \"-S\", \"wine /opt/vc/CL.EXE\"]", ""),
            ("[\"/opt/vc/VC98/Bin/CL.EXE\", \"{source}\"]", ""),
            ("[\"wine\", \"CL.EXE\"]", "WINEPREFIX = \"\"\n"),
        ] {
            let e = CommandCompile::from_config(&wine_cfg(compile, env)).unwrap_err();
            assert!(
                format!("{e:#}").contains("must name its WINEPREFIX"),
                "{compile}: {e:#}"
            );
        }
        assert!(prove::runs_wine(&["wrap".into(), "x/WINE64".into()]));
        assert!(!prove::runs_wine(&["cc".into(), "-c".into(), "a.c".into()]));
        let reg = "[Software\\\\X] 1\n\"CL\"=\"/Od\"\n\n[Environment] 1700000000\n#time=1\n\"TEMP\"=\"C:\\\\t\"\n\"cl\"=\"/O2\"\n[Environment\\\\Sub] 2\n\"_CL_\"=\"x\"\n";
        assert_eq!(prove::registry_env(reg), ["cl"]);
        let d = scratch("wineenv");
        put(&d, "prefix/user.reg", reg.as_bytes());
        let cc = CommandCompile::from_config(&wine_cfg(
            "[\"true\", \"wine\"]",
            &format!("WINEPREFIX = \"{}\"\n", d.join("prefix").display()),
        ))
        .unwrap();
        let job = Job {
            root: d.clone(),
            source: "a.cpp".into(),
            cwd: d.clone(),
            cflags: Vec::new(),
            scratch: d.join("obj"),
            out: d.join("obj/a.obj"),
            slot: 0,
        };
        let e = prove::Compile::compile(&cc, &job).unwrap_err();
        assert!(e.contains("HKCU\\Environment sets cl"), "{e}");
        put(&d, "prefix/user.reg", b"[Environment] 1\n\"TEMP\"=\"x\"\n");
        let e = prove::Compile::compile(&cc, &job).unwrap_err();
        assert!(e.contains("left no object"), "{e}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn a_marker_copied_or_linked_into_another_directory_clears_nothing() {
        let d = scratch("marks");
        let (w, other, linked) = (d.join("w"), d.join("other"), d.join("linked"));
        prove::claim_work(&w).unwrap();
        put(&other, "keep.txt", b"theirs\n");
        std::fs::copy(w.join(prove::WORK_MARK), other.join(prove::WORK_MARK)).unwrap();
        let e = prove::claim_work(&other).unwrap_err();
        assert!(format!("{e:#}").contains("no .msvc6t-prove-work"), "{e:#}");
        assert!(other.join("keep.txt").is_file());
        put(&linked, "keep.txt", b"theirs\n");
        std::fs::hard_link(w.join(prove::WORK_MARK), linked.join(prove::WORK_MARK)).unwrap();
        assert!(prove::claim_work(&linked).is_err());
        assert!(linked.join("keep.txt").is_file());
        std::fs::remove_file(linked.join(prove::WORK_MARK)).unwrap();
        prove::release_work(&w).unwrap();
        assert!(!w.exists());
        std::fs::remove_dir_all(&d).unwrap();
    }
}
