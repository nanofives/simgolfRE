#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::digest::sha256_hex;
use msvc6_base::srcio;
use msvc6_fixture::placement::{
    AT_A, AT_C, a, archive, body, helper, image, library, linked, object, older_archive,
};
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-locate-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        Self(d)
    }

    fn file(&self, name: &str, bytes: &[u8]) -> PathBuf {
        let p = self.0.join(name);
        if let Some(d) = p.parent() {
            std::fs::create_dir_all(d).unwrap();
        }
        srcio::write_bytes(&p, bytes).unwrap();
        p
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        std::fs::remove_dir_all(&self.0).ok();
    }
}

fn run(dir: &Path, args: &[&str]) -> (i32, String, String) {
    let o = Command::new(BIN)
        .current_dir(dir)
        .env_remove("MSVC6T_CONFIG")
        .args(args)
        .output()
        .unwrap();
    (
        o.status.code().unwrap_or(-1),
        srcio::tool_text(&o.stdout),
        srcio::tool_text(&o.stderr),
    )
}

fn project(s: &Scratch) {
    let img = image(linked());
    s.file("fix.dll", &img);
    s.file("rtm/LIBFIX.LIB", &older_archive());
    s.file("sp/libfix.lib", &archive());
    s.file(
        "msvc6t.toml",
        format!(
            "schema = 1\n\n[[image]]\nid = \"FIX\"\nreference = \"fix.dll\"\nsha256 = \"{}\"\nlibraries = [\"rtm/LIBFIX.LIB\", \"sp/libfix.lib\"]\n",
            sha256_hex(&img)
        )
        .as_bytes(),
    );
}

#[test]
fn lib_locate_places_the_chosen_copy_writes_its_table_and_compares_with_one() {
    let s = Scratch::new("verb");
    project(&s);
    let d = &s.0;
    let (code, out, err) = run(
        d,
        &[
            "lib",
            "locate",
            "FIX",
            "--json",
            "rows.json",
            "--write",
            "table.toml",
        ],
    );
    assert_eq!(code, 0, "{out}{err}");
    assert!(
        out.starts_with("fix.dll: placing 6 of 8 archive member(s)\n"),
        "{out}"
    );
    assert!(out.contains(
        "  libfix.lib: 2 distinct copies, chosen by the bytes they place: sp/libfix.lib\n"
    ));
    assert!(out.contains(&format!(
        "    witness a.obj _alpha 32 B at {AT_A:#x} (va {:#x}): rtm/LIBFIX.LIB 80 B absent\n",
        AT_A + 0x1000_0000
    )));
    assert!(out.contains(&format!(
        "        refused {:#x}..{:#x}: held by obj\\a.obj, obj\\c.obj\n",
        AT_A + 0x18,
        AT_C + 8
    )));
    assert!(out.contains("  1 COMDAT(s) placed by section order and 1 by relocation witness"));
    let v: serde_json::Value =
        serde_json::from_str(&srcio::own_read(d.join("rows.json")).unwrap()).unwrap();
    assert_eq!(v["command"], "lib locate");
    assert_eq!(v["rows"].as_array().unwrap().len(), 6);
    assert_eq!(v["summary"]["libraries"][0]["chosen"], 1);
    assert_eq!(v["summary"]["considered"], 8);
    let table = srcio::own_read(d.join("table.toml")).unwrap();
    assert!(table.starts_with("format = 1\nimage = 'fix.dll'\n"));
    assert!(table.contains(
        "[[member]]\nrva = 0x1000\nlen = 0x20\nmember = 'obj\\a.obj'\narchive = 'libfix'\n"
    ));
    let (code, out, _) = run(d, &["lib", "locate", "FIX", "--record", "table.toml"]);
    assert_eq!(code, 0, "{out}");
    assert!(
        out.contains("  0 recorded member row(s) this placement does not re-derive as recorded\n")
    );
    assert!(out.contains(
        "  7 of 7 recorded member row(s) re-derived; 0 kept range(s) the record does not hold\n"
    ));
    let edited = table.replacen("member = 'obj\\c.obj'", "member = 'obj\\other.obj'", 1)
        + "\n[[member]]\nrva = 0x1f00\nlen = 0x10\nmember = 'obj\\hand.obj'\narchive = 'libfix'\n";
    s.file("table.toml", edited.as_bytes());
    let (code, out, _) = run(d, &["lib", "locate", "FIX", "--record", "table.toml"]);
    assert_eq!(code, 3, "{out}");
    assert!(out.contains(&format!(
        "    RECORD {AT_C:#08x} len 0x20: obj\\other.obj -- placed obj\\c.obj\n"
    )));
    assert!(out.contains("    RECORD 0x001f00 len 0x10: obj\\hand.obj -- nothing placed there\n"));
    assert!(out.contains(&format!(
        "    UNRECORDED {AT_C:#08x} len 0x20: obj\\c.obj (libfix)\n"
    )));
    let (code, _, err) = run(d, &["lib", "locate", "FIX", "--write", "table.toml"]);
    assert_eq!(code, 0);
    assert!(err.contains(", 1 carried forward"), "{err}");
    let table = srcio::own_read(d.join("table.toml")).unwrap();
    assert!(table.contains("member = 'obj\\hand.obj'\narchive = 'libfix'\ncarried = true\n"));
    assert!(!table.contains("obj\\other.obj"));
}

#[test]
fn lib_locate_explains_a_member_and_needs_an_archive() {
    let s = Scratch::new("explain");
    project(&s);
    let d = &s.0;
    let (code, out, _) = run(d, &["lib", "locate", "FIX", "--explain", "d.obj"]);
    assert_eq!(code, 0);
    assert!(out.contains("sp/libfix.lib  obj\\d.obj\n"), "{out}");
    assert!(out.contains("_delta_zero"));
    assert!(out.contains("AMBIGUOUS x2"));
    let (code, _, err) = run(d, &["lib", "locate", "fix.dll"]);
    assert_eq!(code, 1);
    assert!(err.contains("pass --lib"), "{err}");
    let (code, out, _) = run(d, &["lib", "locate", "fix.dll", "--lib", "sp/libfix.lib"]);
    assert_eq!(code, 0);
    assert!(out.contains("  libfix.lib: sp/libfix.lib\n"), "{out}");
    s.file("bad.lib", b"!<arch>\nnot a member header");
    let (code, _, err) = run(d, &["lib", "locate", "fix.dll", "--lib", "bad.lib"]);
    assert_eq!(code, 1);
    assert!(err.contains("truncated"), "{err}");
}

#[test]
fn a_rewrite_keeps_the_tie_its_table_decides() {
    let s = Scratch::new("rewrite");
    let d = &s.0;
    s.file("fix.dll", &image(linked()));
    let twin = body("_alpha_twin", &a().bytes, &[(7, "_helper")]);
    s.file(
        "LIBTIE.LIB",
        &library(&[
            ("obj\\a.obj", object(&[a()])),
            ("obj\\b.obj", object(&[twin])),
            ("obj\\helper.obj", object(&[helper()])),
        ]),
    );
    let args = [
        "lib",
        "locate",
        "fix.dll",
        "--lib",
        "LIBTIE.LIB",
        "--write",
        "t.toml",
    ];
    let (code, out, err) = run(d, &args);
    assert_eq!(code, 0, "{out}{err}");
    assert!(
        out.contains("UNDECIDED: ties exactly on covered bytes with obj\\b.obj"),
        "{out}"
    );
    let first = srcio::own_read(d.join("t.toml")).unwrap();
    let row = format!("rva = {AT_A:#x}\nlen = 0x20\nmember = 'obj\\a.obj'");
    assert!(first.contains(&row), "{first}");
    let hand = first.replace(
        &row,
        &format!("rva = {AT_A:#x}\nlen = 0x20\nmember = 'obj\\b.obj'"),
    );
    s.file("t.toml", hand.as_bytes());
    let (code, out, _) = run(d, &args);
    assert_eq!(code, 0, "{out}");
    assert!(out.contains(
        "  an exact tie is decided by the rows of t.toml, the table this run replaces\n"
    ));
    assert!(
        out.contains("decided: RECORD names obj\\b.obj over obj\\a.obj"),
        "{out}"
    );
    assert!(!out.contains("RECORD 0x"), "{out}");
    let again = srcio::own_read(d.join("t.toml")).unwrap();
    assert!(
        again.contains(&format!(
            "rva = {AT_A:#x}\nlen = 0x20\nmember = 'obj\\b.obj'"
        )),
        "{again}"
    );
    assert!(!again.contains("member = 'obj\\a.obj'"), "{again}");
    let (code, out, _) = run(
        d,
        &[
            "lib",
            "locate",
            "fix.dll",
            "--lib",
            "LIBTIE.LIB",
            "--record",
            "t.toml",
        ],
    );
    assert_eq!(code, 0, "{out}");
    assert!(
        out.contains("  0 recorded member row(s) this placement does not re-derive as recorded\n")
    );
}
