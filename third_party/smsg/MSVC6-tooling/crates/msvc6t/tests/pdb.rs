#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::digest::{md5_hex, sha256_hex};
use msvc6_base::srcio;
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-pdb-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        Self(d)
    }

    fn file(&self, name: &str, bytes: &[u8]) -> PathBuf {
        let p = self.0.join(name);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        srcio::write_bytes(&p, bytes).unwrap();
        p
    }

    fn path(&self, name: &str) -> PathBuf {
        self.0.join(name)
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        std::fs::remove_dir_all(&self.0).ok();
    }
}

fn run(dir: &Path, args: &[&str]) -> (i32, String) {
    let o = Command::new(BIN)
        .current_dir(dir)
        .env_remove("MSVC6T_CONFIG")
        .env("MSVC6T_LLVM", dir.join("no-llvm-here"))
        .env("PATH", dir.join("no-path-here"))
        .args(args)
        .output()
        .unwrap();
    (
        o.status.code().unwrap_or(-1),
        format!(
            "{}{}",
            srcio::tool_text(&o.stdout),
            srcio::tool_text(&o.stderr)
        ),
    )
}

fn project(s: &Scratch, shipped_md5: &str, twin: Option<&str>) {
    let smp = msvc6_fixture::pdb::sample();
    s.file("ref/sample.dll", &smp.image);
    s.file("pdb/sample.pdb", &smp.bytes);
    let twin = twin.map_or_else(String::new, |t| format!("twin = \"{t}\"\n"));
    let cfg = format!(
        "schema = 1\n\n[[image]]\nid = \"SMP\"\nreference = \"ref/sample.dll\"\nsha256 = \"{}\"\n\n[image.pdb]\nfile = \"pdb/sample.pdb\"\n{twin}\
         shipped = [{{ md5 = \"{shipped_md5}\", what = \"the sample DLL\", file = \"ref/sample.dll\" }}]\n",
        sha256_hex(&smp.image)
    );
    s.file("msvc6t.toml", cfg.as_bytes());
}

#[test]
fn release_writes_the_pdbs_their_twins_the_recipes_and_a_manifest() {
    let s = Scratch::new("release");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, &md5_hex(&smp.image), None);
    let (code, out) = run(&s.0, &["pdb", "release", "out", "--rev", "product=abc123"]);
    assert_eq!(code, 0, "{out}");
    assert!(out.contains("SKIP llvm: no llvm-pdbutil"), "{out}");
    assert!(
        out.contains("  source names: no [image.pdb.paths], not judged"),
        "{out}"
    );
    assert!(out.contains("  7.0: EQUIVALENT, "), "{out}");
    assert_eq!(std::fs::read(s.path("out/sample.pdb")).unwrap(), smp.bytes);
    let p70 = std::fs::read(s.path("out/7.0/sample.pdb")).unwrap();
    assert!(
        msvc6_pdb::eq::equivalence(&smp.bytes, &p70)
            .unwrap()
            .equivalent()
    );
    let doc = srcio::own_read(s.path("out/ATTACH.md")).unwrap();
    assert!(doc.starts_with(msvc6_pdb::release::INTRO));
    assert_eq!(
        doc.matches("\n### ").count() + usize::from(doc.starts_with("### ")),
        2
    );
    let m = srcio::own_read(s.path("out/MANIFEST.tsv")).unwrap();
    let rows: Vec<&str> = m.lines().collect();
    assert_eq!(rows[0], "file\tbytes\tsha256\tproduct");
    assert_eq!(
        rows[1],
        format!(
            "sample.pdb\t{}\t{}\tabc123",
            smp.bytes.len(),
            sha256_hex(&smp.bytes)
        )
    );
    assert!(rows[2].starts_with("7.0/sample.pdb\t"));
    assert_eq!(
        rows[3],
        format!(
            "ATTACH.md\t{}\t{}\tabc123",
            doc.len(),
            sha256_hex(doc.as_bytes())
        )
    );
    assert!(!s.path("out/.msvc6t-release-tmp").exists());
    s.file("intro.md", b"# Mine\n\n");
    let t = s.path("out/7.0/sample.pdb").display().to_string();
    project(&s, &md5_hex(&smp.image), Some(&t));
    let (code, out) = run(&s.0, &["pdb", "release", "out2", "--intro", "intro.md"]);
    assert_eq!(code, 0, "{out}");
    assert!(out.contains("  7.0: IDENTICAL to the twin "), "{out}");
    let doc2 = srcio::own_read(s.path("out2/ATTACH.md")).unwrap();
    assert_eq!(
        doc2,
        doc.replacen(msvc6_pdb::release::INTRO, "# Mine\n\n", 1)
    );
    let (code, out) = run(&s.0, &["pdb", "release", "out", "--image", "SMP"]);
    assert_ne!(code, 0, "OUT holds the twin itself: {out}");
    assert!(out.contains("release into another directory"), "{out}");
    let (code, out) = run(&s.0, &["pdb", "release", "pdb"]);
    assert_ne!(code, 0, "OUT holds the PDB itself: {out}");
    let (code, out) = run(&s.0, &["pdb", "release", "out3", "--require-llvm"]);
    assert_ne!(code, 0, "{out}");
    assert!(!s.path("out3/sample.pdb").exists());
}

#[test]
fn release_and_attach_refuse_a_file_the_list_does_not_name() {
    let s = Scratch::new("refuse");
    project(&s, "00000000000000000000000000000000", None);
    let (code, out) = run(&s.0, &["pdb", "release", "out"]);
    assert_eq!(code, 3, "{out}");
    assert!(
        out.contains("the list says 00000000000000000000000000000000"),
        "{out}"
    );
    assert!(
        out.contains("release: 1 failure(s); nothing written to out"),
        "{out}"
    );
    assert!(
        !s.path("out").exists(),
        "an OUT made for the release outlives it"
    );
    std::fs::create_dir_all(s.path("kept")).unwrap();
    let (code, out) = run(&s.0, &["pdb", "release", "kept"]);
    assert_eq!(code, 3, "{out}");
    assert!(
        s.path("kept").is_dir(),
        "an OUT that was there before is kept"
    );
    assert_eq!(std::fs::read_dir(s.path("kept")).unwrap().count(), 0);
    let (code, out) = run(&s.0, &["pdb", "attach", "SMP", "ref/sample.dll"]);
    assert_eq!(code, 1, "{out}");
    assert!(
        out.starts_with("REFUSED: ref/sample.dll (sample.dll) for image SMP: "),
        "{out}"
    );
    let smp = msvc6_fixture::pdb::sample();
    project(&s, &md5_hex(&smp.image), None);
    let (code, out) = run(&s.0, &["pdb", "attach", "SMP", "ref/sample.dll", "--twin"]);
    assert_ne!(code, 0, "no twin configured: {out}");
    let (code, out) = run(
        &s.0,
        &["pdb", "attach", "SMP", "ref/sample.dll", "--recipe", "-"],
    );
    assert_eq!(code, 0, "{out}");
    assert!(out.contains("Nothing to change"), "{out}");
}

#[test]
fn release_refuses_bad_revision_columns_before_it_makes_anything() {
    let s = Scratch::new("revs");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, &md5_hex(&smp.image), None);
    for revs in [
        ["--rev", "product=a", "--rev", "product=b"],
        ["--rev", "product=a", "--rev", "sha256=b"],
    ] {
        for out in ["out", "n1/n2/out"] {
            let mut args = vec!["pdb", "release", out];
            args.extend(revs);
            let (code, text) = run(&s.0, &args);
            assert_eq!(code, 1, "{args:?}: {text}");
            assert!(
                text.contains("is named twice") || text.contains("is a manifest column of its own"),
                "{args:?}: {text}"
            );
            assert!(!s.path("out").exists(), "{args:?} made out");
            assert!(!s.path("n1").exists(), "{args:?} made n1");
        }
    }
}

#[test]
fn release_removes_every_directory_it_made_when_it_writes_nothing() {
    let s = Scratch::new("nested");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, "00000000000000000000000000000000", None);
    let (code, out) = run(&s.0, &["pdb", "release", "n1/n2/out"]);
    assert_eq!(code, 3, "{out}");
    assert!(!s.path("n1").exists(), "a failed release left n1: {out}");
    project(&s, &md5_hex(&smp.image), None);
    std::fs::remove_file(s.path("pdb/sample.pdb")).unwrap();
    std::fs::create_dir_all(s.path("kept")).unwrap();
    for out in ["n1/n2/out", "kept/n3/out", "kept"] {
        let (code, text) = run(&s.0, &["pdb", "release", out]);
        assert_eq!(code, 1, "{out}: {text}");
        assert!(text.contains("sample.pdb"), "{out}: {text}");
        assert!(!s.path("n1").exists(), "an error left n1: {text}");
        assert!(!s.path("kept/n3").exists(), "an error left kept/n3: {text}");
        assert!(s.path("kept").is_dir(), "an OUT that was there is kept");
        assert_eq!(std::fs::read_dir(s.path("kept")).unwrap().count(), 0);
    }
    s.file("pdb/sample.pdb", &smp.bytes);
    let (code, text) = run(&s.0, &["pdb", "release", "n1/n2/out"]);
    assert_eq!(code, 0, "{text}");
    assert_eq!(
        std::fs::read(s.path("n1/n2/out/sample.pdb")).unwrap(),
        smp.bytes
    );
    assert!(s.path("n1/n2/out/7.0/sample.pdb").is_file());
    assert!(s.path("n1/n2/out/ATTACH.md").is_file());
    assert!(s.path("n1/n2/out/MANIFEST.tsv").is_file());
    assert!(!s.path("n1/n2/out/.msvc6t-release-tmp").exists());
}

#[test]
fn release_never_removes_a_directory_it_did_not_make() {
    let s = Scratch::new("ghost");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, "00000000000000000000000000000000", None);
    s.file("kept/mine.txt", b"keep me");
    let (code, out) = run(&s.0, &["pdb", "release", "ghost/../kept"]);
    assert_eq!(code, 3, "{out}");
    assert_eq!(std::fs::read(s.path("kept/mine.txt")).unwrap(), b"keep me");
    assert_eq!(std::fs::read_dir(s.path("kept")).unwrap().count(), 1);
    assert!(
        !s.path("ghost").exists(),
        "the run made ghost and left it: {out}"
    );
    s.file("kept/.msvc6t-release-tmp", b"not ours");
    let (code, out) = run(&s.0, &["pdb", "release", "kept"]);
    assert_eq!(code, 1, "{out}");
    assert!(out.contains("is in the way"), "{out}");
    assert_eq!(
        std::fs::read(s.path("kept/.msvc6t-release-tmp")).unwrap(),
        b"not ours"
    );
    std::fs::remove_file(s.path("kept/.msvc6t-release-tmp")).unwrap();
    project(&s, &md5_hex(&smp.image), None);
    let (code, out) = run(&s.0, &["pdb", "release", "ghost/../kept"]);
    assert_eq!(code, 0, "{out}");
    assert!(s.path("kept/sample.pdb").is_file() && s.path("kept/mine.txt").is_file());
    assert!(!s.path("kept/.msvc6t-release-tmp").exists());
}

#[cfg(unix)]
#[test]
fn release_through_a_symlinked_out_keeps_the_link_and_its_target() {
    let s = Scratch::new("link");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, "00000000000000000000000000000000", None);
    s.file("real/mine.txt", b"keep me");
    std::os::unix::fs::symlink(s.path("real"), s.path("link")).unwrap();
    std::os::unix::fs::symlink(s.path("real"), s.path("real/.msvc6t-release-tmp")).unwrap();
    let (code, out) = run(&s.0, &["pdb", "release", "link"]);
    assert_eq!(code, 1, "{out}");
    assert!(out.contains("is in the way"), "{out}");
    assert!(
        std::fs::symlink_metadata(s.path("real/.msvc6t-release-tmp"))
            .unwrap()
            .file_type()
            .is_symlink()
    );
    std::fs::remove_file(s.path("real/.msvc6t-release-tmp")).unwrap();
    let (code, out) = run(&s.0, &["pdb", "release", "link"]);
    assert_eq!(code, 3, "{out}");
    let link = |p: &str| std::fs::symlink_metadata(s.path(p)).unwrap();
    assert!(link("link").file_type().is_symlink());
    assert_eq!(std::fs::read(s.path("real/mine.txt")).unwrap(), b"keep me");
    assert_eq!(std::fs::read_dir(s.path("real")).unwrap().count(), 1);
    std::os::unix::fs::symlink(s.path("nowhere"), s.path("dangling")).unwrap();
    let (code, out) = run(&s.0, &["pdb", "release", "dangling/out"]);
    assert_eq!(code, 1, "{out}");
    assert!(link("dangling").file_type().is_symlink());
    project(&s, &md5_hex(&smp.image), None);
    let (code, out) = run(&s.0, &["pdb", "release", "link"]);
    assert_eq!(code, 0, "{out}");
    assert!(link("link").file_type().is_symlink());
    assert_eq!(std::fs::read(s.path("real/sample.pdb")).unwrap(), smp.bytes);
    assert!(s.path("real/7.0/sample.pdb").is_file() && s.path("real/mine.txt").is_file());
}

#[cfg(unix)]
#[test]
fn release_takes_over_no_temporary_directory_it_finds() {
    let s = Scratch::new("tmpdir");
    let smp = msvc6_fixture::pdb::sample();
    s.file("victim.txt", b"victim");
    std::fs::create_dir_all(s.path("kept/.msvc6t-release-tmp/7.0")).unwrap();
    std::os::unix::fs::symlink(
        "../../../victim.txt",
        s.path("kept/.msvc6t-release-tmp/7.0/sample.pdb"),
    )
    .unwrap();
    for shipped in [
        md5_hex(&smp.image),
        "00000000000000000000000000000000".into(),
    ] {
        project(&s, &shipped, None);
        let (code, out) = run(&s.0, &["pdb", "release", "kept"]);
        assert_eq!(code, 1, "{out}");
        assert!(out.contains("is in the way"), "{out}");
        assert_eq!(std::fs::read(s.path("victim.txt")).unwrap(), b"victim");
        assert!(
            std::fs::symlink_metadata(s.path("kept/.msvc6t-release-tmp/7.0/sample.pdb"))
                .unwrap()
                .file_type()
                .is_symlink()
        );
        assert_eq!(std::fs::read_dir(s.path("kept")).unwrap().count(), 1);
    }
}

#[test]
fn release_refuses_a_destination_that_is_no_regular_file_before_it_moves_anything() {
    let s = Scratch::new("dest");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, &md5_hex(&smp.image), None);
    s.file("kept/sample.pdb", b"old");
    std::fs::create_dir_all(s.path("kept/ATTACH.md")).unwrap();
    let (code, out) = run(&s.0, &["pdb", "release", "kept"]);
    assert_eq!(code, 1, "{out}");
    assert!(out.contains("ATTACH.md is in the way"), "{out}");
    assert_eq!(std::fs::read(s.path("kept/sample.pdb")).unwrap(), b"old");
    assert!(s.path("kept/ATTACH.md").is_dir());
    assert!(!s.path("kept/7.0").exists() && !s.path("kept/.msvc6t-release-tmp").exists());
    std::fs::remove_dir(s.path("kept/ATTACH.md")).unwrap();
    let (code, out) = run(&s.0, &["pdb", "release", "kept"]);
    assert_eq!(code, 0, "{out}");
    assert_eq!(std::fs::read(s.path("kept/sample.pdb")).unwrap(), smp.bytes);
    assert!(!s.path("kept/.msvc6t-release-tmp").exists());
}

#[cfg(unix)]
#[test]
fn a_move_that_fails_puts_back_every_file_it_replaced() {
    use std::os::unix::fs::PermissionsExt as _;
    let s = Scratch::new("restore");
    let smp = msvc6_fixture::pdb::sample();
    project(&s, &md5_hex(&smp.image), None);
    s.file("kept/sample.pdb", b"old");
    std::fs::create_dir_all(s.path("kept/7.0")).unwrap();
    let mode = |m: u32| {
        std::fs::set_permissions(s.path("kept/7.0"), std::fs::Permissions::from_mode(m)).unwrap();
    };
    mode(0o555);
    if srcio::write_bytes(s.path("kept/7.0/probe"), b"x").is_ok() {
        mode(0o755);
        eprintln!(
            "SKIP a_move_that_fails_puts_back_every_file_it_replaced: a read-only directory is writable here"
        );
        return;
    }
    let (code, out) = run(&s.0, &["pdb", "release", "kept"]);
    mode(0o755);
    assert_eq!(code, 1, "{out}");
    assert!(out.contains("7.0/sample.pdb"), "{out}");
    assert!(
        out.contains("its filesystem refuses a hard link from")
            && out.contains("pdb release places every file by a no-replace hard link"),
        "{out}"
    );
    assert_eq!(std::fs::read(s.path("kept/sample.pdb")).unwrap(), b"old");
    assert_eq!(std::fs::read_dir(s.path("kept/7.0")).unwrap().count(), 0);
    assert_eq!(std::fs::read_dir(s.path("kept")).unwrap().count(), 2);
}
