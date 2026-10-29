#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_fixture::codeview::sample;
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-cv-{tag}-{}", std::process::id()));
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

struct Out {
    code: i32,
    stdout: String,
    stderr: String,
}

fn run(dir: &Path, args: &[&str]) -> Out {
    let o = Command::new(BIN)
        .current_dir(dir)
        .env_remove("MSVC6T_CONFIG")
        .args(args)
        .output()
        .unwrap();
    Out {
        code: o.status.code().unwrap_or(-1),
        stdout: srcio::tool_text(&o.stdout),
        stderr: srcio::tool_text(&o.stderr),
    }
}

fn expect(dir: &Path, code: i32, args: &[&str]) -> String {
    let o = run(dir, args);
    assert_eq!(o.code, code, "{args:?}\n{}\n{}", o.stdout, o.stderr);
    o.stdout
}

fn json(p: &Path) -> serde_json::Value {
    serde_json::from_str(&srcio::own_read(p).unwrap()).unwrap()
}

#[test]
fn cv_decl_check_and_names_read_an_image_carrying_a_record() {
    let s = Scratch::new("decl");
    let img = s.file("dev.dll", &sample().image);
    let img = img.to_str().unwrap();
    let a = expect(&s.0, 0, &["cv", "decl", img]);
    assert!(a.starts_with("// The record's declarations: 1 class(es) in 1 header(s)"));
    assert!(a.contains(
        "    virtual void set_volume(unsigned long volume);  // vtbl +0x0; inline dev h.h:42\n"
    ));
    assert!(a.contains("    virtual void play(unsigned long n);  // vtbl +0x4; body snd.cpp:9\n"));
    assert_eq!(expect(&s.0, 0, &["cv", "decl", img]), a);
    let j = s.0.join("decl.json");
    expect(
        &s.0,
        0,
        &["cv", "decl", img, "--all", "--json", j.to_str().unwrap()],
    );
    let v = json(&j);
    assert_eq!(v["command"], "cv decl");
    assert_eq!(v["rows"].as_array().unwrap().len(), 14);
    assert_eq!(v["summary"]["classes"], 2);
    let c = expect(&s.0, 0, &["cv", "check", img]);
    assert!(c.contains("the container writes back byte for byte"), "{c}");
    assert!(c.ends_with("CV-CHECK: IDENTICAL\n"), "{c}");
    let n = expect(&s.0, 0, &["cv", "names", img, "--modules", "dev.obj"]);
    for line in [
        "member\tDev\tm_flags",
        "member\tDev\tm_head",
        "member\tSnd\tplay",
        "enumerator\tERRCODE\tERR_FAIL",
        "local\tDev\tset_volume\tvolume",
        "local\tDev\tset_volume\tstatus",
    ] {
        assert!(n.lines().any(|l| l == line), "{line:?} in\n{n}");
    }
    assert!(!n.contains("local\tSnd\tplay\tn"), "{n}");
    assert!(!n.contains("\tthis\n"));
    let plain = s.file("plain.dll", &msvc6_fixture::sample::sample().pe.build());
    let o = run(&s.0, &["cv", "decl", plain.to_str().unwrap()]);
    assert_eq!(o.code, 1);
    assert!(
        o.stderr.contains("no NB09 or NB11 CodeView record"),
        "{}",
        o.stderr
    );
}

const DSP: &str = "# ADD CPP /nologo /I \"include\" /D \"_USRDLL\" /c\nSOURCE=.\\src\\dev.cpp\n";
const APP_DSP: &str = "# ADD CPP /nologo /I \"..\\lib\\a.dll\\include\" /c\nSOURCE=.\\main.cpp\n";
const HEADER: &str = "class Device {\npublic:\n    Device();\n    virtual void set_volume(unsigned long vol);\n    unsigned long m_flags;\n    static Device *m_head;\n#ifdef _USRDLL\n    void helper();\n#endif\n};\n";
const BODY: &str = "#include \"devh.h\"\nvoid Device::set_volume(unsigned long volume) { }\nDevice::Device() { }\n";

fn project(s: &Scratch) -> PathBuf {
    let img = s.file("dev.dll", &sample().image);
    s.file("tree/lib/a.dll/a.dsp", DSP.as_bytes());
    s.file("tree/lib/a.dll/include/devh.h", HEADER.as_bytes());
    s.file("tree/lib/a.dll/src/dev.cpp", BODY.as_bytes());
    s.file("tree/app/app.dsp", APP_DSP.as_bytes());
    s.file(
        "tree/app/main.cpp",
        b"#include \"devh.h\"\nint main() { return 0; }\n",
    );
    s.file(
        "explained.tsv",
        b"a.dll\tmember-extra\tDev::helper\tMEASURED: the retail image keeps it\n",
    );
    let cfg = format!(
        "schema = 1\n\n[[image]]\nid = \"DEV\"\nreference = \"{img}\"\n\n[image.codeview]\nrecord = \"{img}\"\nroot = \"tree\"\nheaders = \"lib/a.dll/include\"\nsearch = [\"lib/*/include\"]\nexplained = \"explained.tsv\"\nreasons = [\"MEASURED\"]\n\n[[image.codeview.view]]\nname = \"a.dll\"\nprojects = [\"lib/a.dll\"]\nowner = true\n\n[[image.codeview.view]]\nname = \"app\"\nprojects = [\"app\"]\n",
        img = img.display()
    );
    s.file("msvc6t.toml", cfg.as_bytes())
}

#[test]
fn cv_diff_reads_a_view_and_cv_apply_rewrites_only_its_own_files() {
    let s = Scratch::new("diff");
    project(&s);
    let j = s.0.join("diff.json");
    let d = expect(
        &s.0,
        3,
        &["cv", "diff", "DEV", "a.dll", "--json", j.to_str().unwrap()],
    );
    for want in [
        "class-name       Dev (ours Device)  record: Dev  ours: Device",
        "param-name       Dev::set_volume#1 (ours Device::set_volume)  record: volume  ours: vol",
        "class-missing    Snd  record: class Snd (no header recorded)",
    ] {
        assert!(d.contains(want), "{want:?} in\n{d}");
    }
    assert!(!d.lines().any(|l| l.starts_with("member-extra")), "{d}");
    let v = json(&j);
    assert_eq!(v["summary"]["explained"], 1);
    let extra = v["rows"]
        .as_array()
        .unwrap()
        .iter()
        .find(|r| r["kind"] == "member-extra")
        .unwrap();
    assert_eq!(extra["explained"], "MEASURED: the retail image keeps it");
    let all = expect(&s.0, 3, &["cv", "diff", "DEV", "a.dll", "--all"]);
    assert!(
        all.contains("EXPLAINED member-extra     Dev::helper"),
        "{all}"
    );
    let app = expect(&s.0, 3, &["cv", "diff", "DEV", "app"]);
    assert!(
        !app.contains("member-extra") && !app.contains("class-missing"),
        "{app}"
    );
    let ap = expect(&s.0, 0, &["cv", "apply", "DEV", "a.dll", "param-name"]);
    assert!(
        ap.contains(
            "REFUSED lib/a.dll/include/devh.h:4: vol -> volume: another view reads this file"
        ),
        "{ap}"
    );
    assert!(
        ap.ends_with("APPLIED param-name: 0 fix(es) to a.dll, 1 refused\n"),
        "{ap}"
    );
    let ap = expect(
        &s.0,
        0,
        &["cv", "apply", "DEV", "a.dll", "param-name", "--shared"],
    );
    assert!(
        ap.ends_with("APPLIED param-name: 1 fix(es) to a.dll, 0 refused\n"),
        "{ap}"
    );
    let h = srcio::read(s.0.join("tree/lib/a.dll/include/devh.h")).unwrap();
    assert!(
        h.contains("virtual void set_volume(unsigned long volume);"),
        "{h}"
    );
    let after = expect(&s.0, 3, &["cv", "diff", "DEV", "a.dll"]);
    assert!(!after.contains("param-name"), "{after}");
    let o = run(&s.0, &["cv", "diff", "DEV", "nowhere"]);
    assert_eq!(o.code, 1);
    assert!(o.stderr.contains("no view `nowhere`"), "{}", o.stderr);
}

#[test]
fn a_misspelt_codeview_key_is_refused_when_the_config_loads() {
    let s = Scratch::new("strict");
    let cfg = project(&s);
    let text = srcio::own_read(&cfg)
        .unwrap()
        .replace("owner = true", "ownr = true");
    srcio::own_write(&cfg, text).unwrap();
    let o = run(&s.0, &["cv", "diff", "DEV", "a.dll"]);
    assert_eq!(o.code, 1);
    assert!(o.stderr.contains("codeview"), "{}", o.stderr);
}

#[test]
fn debuginfo_lists_embedded_records_pdb_references_and_pdb_files() {
    let s = Scratch::new("debuginfo");
    s.file("demo/game/dev.dll", &sample().image);
    s.file(
        "retail/sample.dll",
        &msvc6_fixture::sample::sample().pe.build(),
    );
    s.file(
        "retail/plain.dll",
        &msvc6_fixture::sample::versioned(1, 0, 0),
    );
    let mut pdb = b"Microsoft C/C++ program database 2.00\r\n\x1aJG\0\0".to_vec();
    pdb.resize(64, 0);
    s.file("retail/sample.pdb", &pdb);
    s.file("retail/readme.txt", b"MZ is not enough");
    #[cfg(unix)]
    std::os::unix::fs::symlink(s.0.join("demo"), s.0.join("retail/loop")).unwrap();
    let j = s.0.join("di.json");
    let out = expect(
        &s.0,
        0,
        &["debuginfo", "demo", "retail", "--json", j.to_str().unwrap()],
    );
    assert!(out.starts_with("debuginfo: 5 file(s) read, 3 image(s): 1 with an embedded NB09 / NB11 record, 1 naming a PDB, 1 with no CodeView record; 1 PDB or DBG file(s)\n"), "{out}");
    let nb11 = out.lines().find(|l| l.contains("dev.dll")).unwrap();
    assert!(nb11.contains("CodeView NB11 embedded"), "{nb11}");
    assert!(nb11.contains("12 subsection(s), 3 module(s)"), "{nb11}");
    let nb10 = out.lines().find(|l| l.contains("sample.dll")).unwrap();
    assert!(
        nb10.contains("CodeView NB10 -> C:\\work\\Release\\sample.pdb"),
        "{nb10}"
    );
    assert!(out.contains("PDB 2.00"), "{out}");
    assert!(!out.contains("plain.dll"), "{out}");
    #[cfg(unix)]
    assert!(out.contains("skipped a symlinked directory"), "{out}");
    let v = json(&j);
    assert_eq!(v["summary"]["embedded"], 1);
    let rows = v["rows"].as_array().unwrap();
    assert_eq!(rows.len(), 3);
    assert_eq!(rows[0]["format"], "NB11");
    assert_eq!(rows[0]["embedded"], true);
    let all = expect(&s.0, 0, &["debuginfo", "retail", "--all"]);
    assert!(
        all.contains("plain.dll") && all.contains("no CodeView record"),
        "{all}"
    );
}

#[test]
fn debuginfo_reads_each_file_and_directory_once() {
    let s = Scratch::new("debuginfo-once");
    s.file("demo/game/dev.dll", &sample().image);
    s.file(
        "retail/sample.dll",
        &msvc6_fixture::sample::sample().pe.build(),
    );
    std::fs::hard_link(s.0.join("demo/game/dev.dll"), s.0.join("retail/linked.dll")).unwrap();
    let j = s.0.join("once.json");
    let out = expect(
        &s.0,
        0,
        &[
            "debuginfo",
            "demo",
            "demo",
            "./demo/game/../game/dev.dll",
            "retail",
            "--json",
            j.to_str().unwrap(),
        ],
    );
    assert!(
        out.starts_with("debuginfo: 2 file(s) read, 2 image(s): 1 with an embedded NB09 / NB11 record, 1 naming a PDB"),
        "{out}"
    );
    assert_eq!(
        out.lines().filter(|l| l.contains("NB11 embedded")).count(),
        1,
        "{out}"
    );
    for d in [
        "  duplicate: demo is demo, already read",
        "  duplicate: ./demo/game/../game/dev.dll is demo/game/dev.dll, already read",
        "  duplicate: retail/linked.dll is demo/game/dev.dll, already read",
    ] {
        assert!(out.lines().any(|l| l == d), "{d:?} in\n{out}");
    }
    let v = json(&j);
    assert_eq!(v["rows"].as_array().unwrap().len(), 2);
    assert_eq!(v["summary"]["duplicates"].as_array().unwrap().len(), 3);
}
