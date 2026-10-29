#[cfg(test)]
mod t {
    use msvc6_base::srcio;
    use std::path::{Path, PathBuf};
    use std::process::{Command, Output};

    const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6t-src-{tag}-{}", std::process::id()));
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

    fn run(dir: &Path, args: &[&str]) -> Output {
        Command::new(BIN)
            .current_dir(dir)
            .env_remove("MSVC6T_CONFIG")
            .args(args)
            .output()
            .unwrap()
    }

    fn text(o: &Output) -> String {
        srcio::tool_text(&o.stdout) + &srcio::tool_text(&o.stderr)
    }

    fn tree(root: &Path) {
        put(
            root,
            "src/a.cpp",
            b"/* \xa7 head */\r\nchar *s = \"// not ??/\" here\";\r\nint f(int n) { return n; } // tail\r\n",
        );
        put(
            root,
            "proj/p.dsp",
            b"# Microsoft Developer Studio Project File - Name=\"p\" - Package Owner=<4>\r\n# Microsoft Developer Studio Generated Build File, Format Version 6.00\r\n# ** DO NOT EDIT **\r\n# PROP BASE Use_MFC 0\r\n",
        );
        put(root, "run.sh", b"#!/bin/sh\n# a comment\necho hi\n");
        put(root, "README.md", b"# Title\n<!-- hidden -->\ntext\n");
        put(root, "data.json", b"{\"a\": \"# no\"}\n");
    }

    #[test]
    fn strip_check_names_each_comment_and_a_preview_is_clean() {
        let d = scratch("strip");
        let root = d.join("tree");
        tree(&root);
        let o = run(
            &d,
            &["src", "--root", root.to_str().unwrap(), "strip", "--check"],
        );
        let t = text(&o);
        assert_eq!(o.status.code(), Some(3), "{t}");
        for want in [
            "src/a.cpp:1: /* \u{a7} head */",
            "src/a.cpp:3: // tail",
            "proj/p.dsp:3: # ** DO NOT EDIT **",
            "run.sh:2: # a comment",
            "README.md:2: <!-- hidden -->",
            "5 comment(s)",
        ] {
            assert!(t.contains(want), "{want} in {t}");
        }
        assert!(!t.contains("Microsoft Developer Studio"), "{t}");
        assert!(!t.contains("#!/bin/sh"), "{t}");
        let preview = d.join("preview");
        let o = run(
            &d,
            &[
                "src",
                "--root",
                root.to_str().unwrap(),
                "strip",
                "--preview",
                preview.to_str().unwrap(),
            ],
        );
        assert_eq!(o.status.code(), Some(0), "{}", text(&o));
        assert_eq!(
            std::fs::read(preview.join("src/a.cpp")).unwrap(),
            b"char *s = \"// not ??/\" here\";\r\nint f(int n) { return n; }\r\n"
        );
        let o = run(
            &d,
            &[
                "src",
                "--root",
                preview.to_str().unwrap(),
                "strip",
                "--check",
            ],
        );
        assert_eq!(o.status.code(), Some(0), "{}", text(&o));
        assert!(text(&o).contains("strip ok"));
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn lex_reports_what_the_compiler_would_refuse() {
        let d = scratch("lex");
        put(&d, "ok.c", b"int a = '??/'';\n");
        put(&d, "bad.c", b"char *s = \"x??/\";\n");
        let o = run(&d, &["src", "lex", "ok.c"]);
        assert_eq!(o.status.code(), Some(0), "{}", text(&o));
        assert!(text(&o).contains("1 trigraph(s)"));
        let o = run(&d, &["src", "lex", "bad.c"]);
        assert_eq!(o.status.code(), Some(3));
        assert!(text(&o).contains("PROBLEM line 1"));
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn rename_check_and_lines_exit_by_their_verdicts() {
        let d = scratch("rename");
        put(
            &d,
            "old.cpp",
            b"int f(int cb)\n{\n    assert(cb);\n    return cb + 1;\n}\n",
        );
        put(
            &d,
            "new.cpp",
            b"int f(int count)\n{\n    assert(count);\n    return count + 1;\n}\n",
        );
        put(
            &d,
            "bad.cpp",
            b"int f(int count)\n{\n    assert(count);\n    return count + 2;\n}\n",
        );
        put(
            &d,
            "moved.cpp",
            b"int f(int cb)\n{\n\n    assert(cb);\n    return cb + 1;\n}\n",
        );
        let o = run(&d, &["src", "rename-check", "old.cpp", "new.cpp"]);
        assert_eq!(o.status.code(), Some(0), "{}", text(&o));
        assert!(text(&o).contains("cb -> count"));
        let o = run(&d, &["src", "rename-check", "old.cpp", "bad.cpp"]);
        assert_eq!(o.status.code(), Some(3), "{}", text(&o));
        let o = run(&d, &["src", "lines", "moved.cpp", "--against", "old.cpp"]);
        assert_eq!(o.status.code(), Some(3), "{}", text(&o));
        let o = run(&d, &["src", "lines", "old.cpp", "--against", "old.cpp"]);
        assert_eq!(o.status.code(), Some(0), "{}", text(&o));
        std::fs::remove_dir_all(&d).unwrap();
    }
}
