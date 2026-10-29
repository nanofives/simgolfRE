#[cfg(test)]
mod tests {
    use msvc6_lint::srcio::{own_read, own_write, tool_text};
    use msvc6_lint::tree;
    use std::path::{Path, PathBuf};
    use std::process::{Command, Output};

    fn toolkit() -> PathBuf {
        tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR"))).unwrap()
    }

    fn plant(root: &Path, rel: &str, text: &str) {
        let p = root.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        own_write(p, text).unwrap();
    }

    fn cargo_build(ws: &Path, target: &Path) -> Output {
        let cargo = PathBuf::from(std::env::var_os("CARGO").unwrap_or_else(|| "cargo".into()));
        let mut cmd = Command::new(&cargo);
        cmd.args(["build", "--offline"])
            .current_dir(ws)
            .env("CARGO_TARGET_DIR", target);
        if let Some(rustc) = cargo
            .parent()
            .map(|d| d.join("rustc"))
            .filter(|p| p.is_file())
        {
            cmd.env("RUSTC", rustc);
        }
        for v in [
            "RUSTFLAGS",
            "CARGO_ENCODED_RUSTFLAGS",
            "RUSTC_WRAPPER",
            "RUSTC_WORKSPACE_WRAPPER",
            "CARGO_BUILD_TARGET_DIR",
        ] {
            cmd.env_remove(v);
        }
        cmd.output().unwrap()
    }

    fn stderr(o: &Output) -> String {
        tool_text(&o.stderr)
    }

    #[test]
    fn a_new_root_entry_reruns_the_workspace_check_without_a_touch() {
        let base = std::env::temp_dir().join(format!("msvc6-lint-rerun-{}", std::process::id()));
        if base.exists() {
            std::fs::remove_dir_all(&base).unwrap();
        }
        let (ws, target) = (base.join("ws"), base.join("target"));
        let kit = toolkit();
        plant(
            &ws,
            "Cargo.toml",
            "[workspace]\nresolver = \"3\"\nmembers = [\"crates/*\"]\n\n[workspace.package]\nedition = \"2024\"\n\n[workspace.lints.rust]\nunsafe_code = \"forbid\"\n",
        );
        for f in ["rustfmt.toml", "clippy.toml"] {
            plant(&ws, f, &own_read(kit.join(f)).unwrap());
        }
        plant(
            &ws,
            "crates/app/Cargo.toml",
            &format!(
                "[package]\nname = \"app\"\nversion = \"0.1.0\"\nedition.workspace = true\npublish = false\n\n\
             [build-dependencies]\nmsvc6-lint = {{ path = \"{}\" }}\n\n[lints]\nworkspace = true\n",
                kit.join("crates/msvc6-lint").display()
            ),
        );
        plant(
            &ws,
            "crates/app/build.rs",
            "fn main() {\n    msvc6_lint::buildcheck::workspace_main();\n}\n",
        );
        plant(&ws, "crates/app/src/main.rs", "fn main() {}\n");

        let o = cargo_build(&ws, &target);
        assert!(
            o.status.success(),
            "the clean scratch workspace builds:\n{}",
            stderr(&o)
        );

        plant(&ws, "stray.rs", "/* planted block */\n");
        let o = cargo_build(&ws, &target);
        assert!(!o.status.success(), "a stray root .rs refuses the build");
        assert!(
            stderr(&o).contains("stray.rs:1:1: a block comment"),
            "{}",
            stderr(&o)
        );

        std::fs::remove_file(ws.join("stray.rs")).unwrap();
        let o = cargo_build(&ws, &target);
        assert!(o.status.success(), "{}", stderr(&o));

        plant(&ws, "newdir/x.rs", "// planted\n");
        let o = cargo_build(&ws, &target);
        assert!(
            !o.status.success(),
            "a .rs in a new top-level dir refuses the build"
        );
        assert!(
            stderr(&o).contains("newdir/x.rs:1:1: a line comment"),
            "{}",
            stderr(&o)
        );
        std::fs::remove_dir_all(&base).unwrap();
    }
}
