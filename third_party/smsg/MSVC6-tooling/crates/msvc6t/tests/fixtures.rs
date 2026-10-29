#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_fixture::archive::{Library, short_import};
use msvc6_fixture::coff::{CLASS_LABEL, Object as Obj};
use msvc6_fixture::pe::{Pe, SCN_CODE};
use msvc6_fixture::sample::{TIMESTAMP, postlinked, sample};
use msvc6_toolchain::exec::Cmd;
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-verbs-{tag}-{}", std::process::id()));
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

    fn path(&self, name: &str) -> PathBuf {
        self.0.join(name)
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

fn ok(dir: &Path, args: &[&str]) -> String {
    let o = run(dir, args);
    assert_eq!(o.code, 0, "{args:?}\n{}\n{}", o.stdout, o.stderr);
    o.stdout
}

fn json(p: &Path) -> serde_json::Value {
    serde_json::from_str(&srcio::own_read(p).unwrap()).unwrap()
}

fn sample_dll(s: &Scratch) -> PathBuf {
    s.file("sample.dll", &sample().pe.build())
}

#[test]
fn image_info_prints_the_headers_and_writes_the_section_rows() {
    let s = Scratch::new("info");
    sample_dll(&s);
    let out = ok(
        &s.0,
        &["image", "info", "sample.dll", "--json", "info.json"],
    );
    for want in [
        "PE32 i386 DLL, 6 section(s)",
        &format!("COFF timestamp {TIMESTAMP:#010x}"),
        "Rich header at 0x80: 4 stamp(s)",
        "= the checksum",
        "CodeView NB10 signature 0x3b9ac5b0 age 7",
        "Import ",
        ".idata",
    ] {
        assert!(out.contains(want), "{want}\n{out}");
    }
    let j = json(&s.path("info.json"));
    assert_eq!(j["command"], "image info");
    assert_eq!(j["image"], "sample.dll");
    assert_eq!(j["rows"].as_array().unwrap().len(), 6);
    assert_eq!(j["summary"]["machine"], 0x14c);
    assert_eq!(j["summary"]["rich"]["key_is_checksum"], true);
}

fn stamped_lib(builds: &[u16]) -> Vec<u8> {
    let mut l = Library::default();
    for (i, b) in builds.iter().enumerate() {
        let mut o = Obj::new();
        o.code(".text", &[0xc3]);
        o.comp_id(0x0a, *b);
        l.member(&format!("m{i}.obj"), o.build(), &[]);
    }
    l.build()
}

#[test]
fn image_rich_censuses_a_candidate_archives_and_a_response_file() {
    let s = Scratch::new("rich");
    sample_dll(&s);
    let mut bare = Pe::exe();
    bare.push(".text", SCN_CODE, vec![0xc3]);
    s.file("bare.exe", &bare.build());
    s.file("gold.lib", &stamped_lib(&[8168, 7299]));
    s.file("sp5.lib", &stamped_lib(&[8047, 7299]));
    let mut a = Obj::new();
    a.code(".text", &[0xc3]);
    a.comp_id(0x0a, 8168);
    s.file("objs/a.obj", &a.build());
    s.file("objs/b.obj", &a.build());
    s.file(
        "objs/link.rsp",
        b"/OUT:x.dll\r\n\"a.obj\" b.obj\r\nkernel32.lib\r\n",
    );
    let out = ok(
        &s.0,
        &[
            "image",
            "rich",
            "sample.dll",
            "--candidate",
            "sample.dll",
            "--archive",
            "gold.lib",
            "--archive",
            "sp5.lib",
            "--rsp",
            "objs/link.rsp",
            "--json",
            "r.json",
        ],
    );
    for want in [
        "xor key",
        "(the checksum)",
        "prodid 0x0b Utc12_CPP      build 8168   objects 12",
        "compiled objects in the link: 40 C, 12 C++, 0 MASM = 52",
        "census   C 40 vs 40 (MATCH);  C++ 12 vs 12 (MATCH)",
        "stamp classes: the same 4 in both",
        "gold.lib: build 7299 x1 shared, decides nothing; build 8168 x1 IN IMAGE",
        "VERDICT: gold.lib, decided by a build only that copy stamps",
        "2 object(s) read from objs/link.rsp, 0 unreadable",
        "40 C",
    ] {
        assert!(out.contains(want), "{want}\n{out}");
    }
    let j = json(&s.path("r.json"));
    assert_eq!(j["rows"][0]["candidate"], 5);
    let none = ok(
        &s.0,
        &["image", "rich", "sample.dll", "--candidate", "bare.exe"],
    );
    assert!(none.contains("UNKNOWN, not zero"), "{none}");
    let gone = run(
        &s.0,
        &["image", "rich", "sample.dll", "--candidate", "nothing.dll"],
    );
    assert_eq!(gone.code, 1);
    let plain = ok(&s.0, &["image", "rich", "bare.exe"]);
    assert!(plain.contains("no Rich header"), "{plain}");
}

#[test]
fn image_dump_reads_the_table_an_address_and_the_zero_run() {
    let s = Scratch::new("dump");
    sample_dll(&s);
    let table = ok(&s.0, &["image", "dump", "sample.dll", "--json", "d.json"]);
    assert!(table.contains("0x2e00 byte(s) from 0x3200"), "{table}");
    assert_eq!(json(&s.path("d.json"))["rows"].as_array().unwrap().len(), 6);
    let at = ok(
        &s.0,
        &[
            "image",
            "dump",
            "sample.dll",
            "--at",
            "0x1000",
            "--len",
            "4",
        ],
    );
    assert!(
        at.contains("is in `.text` at +0x0; file bytes at offset 0x400"),
        "{at}"
    );
    assert!(at.contains("55 8b ec a1"), "{at}");
    let va = ok(
        &s.0,
        &["image", "dump", "sample.dll", "--at", "0x10003010", "--va"],
    );
    assert!(va.contains("is in `.data` at +0x10"), "{va}");
    let tail = ok(&s.0, &["image", "dump", "sample.dll", "--at", "0x5000"]);
    assert!(tail.contains("virtual-only tail"), "{tail}");
    assert!(tail.contains("no 64 byte(s) to dump"), "{tail}");
    let out = ok(&s.0, &["image", "dump", "sample.dll", "--at", "0x7000000"]);
    assert!(out.contains("in no section"), "{out}");
    let z = ok(
        &s.0,
        &[
            "image",
            "dump",
            "sample.dll",
            "--textbss",
            "--section",
            ".data",
        ],
    );
    assert!(z.contains("final zero run starts at rva 0x3080"), "{z}");
    assert_eq!(
        run(&s.0, &["image", "dump", "sample.dll", "--at", "zz"]).code,
        1
    );
}

#[test]
fn image_imports_prints_rows_orders_and_anatomy() {
    let s = Scratch::new("imports");
    sample_dll(&s);
    let mut kernel = Library::default();
    for n in ["ExitProcess", "GetVersion", "HeapAlloc"] {
        kernel.member(
            "KERNEL32.dll",
            short_import("KERNEL32.dll", &format!("_{n}@0"), 0, 0, 3),
            &[&format!("__imp__{n}@0")],
        );
    }
    s.file("lib/kernel32.LIB", &kernel.build());
    let out = ok(
        &s.0,
        &[
            "image",
            "imports",
            "sample.dll",
            "--libdir",
            "lib",
            "--json",
            "i.json",
        ],
    );
    for want in [
        "import directory 0x6000 + 60 in .idata: 2 DLLs, 5 symbols",
        "descriptors  .idata$2  per LIBRARY   KERNEL32.dll, WS2_32.dll",
        "KERNEL32.dll       3/3   in lib   ILT slot order: true    name layout order: true",
        "WS2_32.dll       no import library",
        "  .idata anatomy: 0x6000..0x615e (350 bytes)",
        "    0x00603c..0x00604c      16  ILT KERNEL32.dll",
        "    0x00604c..0x006074      40  (reserve, zero-filled)",
        "    0x006149..0x00614a       1  (linker padding)",
        "link.exe generates 349 of 350 bytes",
        "  .rdata anatomy:",
        "export directory",
    ] {
        assert!(out.contains(want), "{want}\n{out}");
    }
    let j = json(&s.path("i.json"));
    assert_eq!(j["rows"][0]["dll"], "KERNEL32.dll");
    assert_eq!(j["rows"][1]["ordinals"], 1);
    assert_eq!(j["summary"]["symbols"], 5);
    ok(
        &s.0,
        &[
            "image",
            "imports",
            "sample.dll",
            "--entries",
            "--json",
            "e.json",
        ],
    );
    assert_eq!(json(&s.path("e.json"))["rows"].as_array().unwrap().len(), 5);
}

#[test]
fn image_exports_lists_names_ordinals_and_the_relink_stamp() {
    let s = Scratch::new("exports");
    sample_dll(&s);
    let out = ok(
        &s.0,
        &["image", "exports", "sample.dll", "--json", "x.json"],
    );
    for want in [
        "'sample.dll', ordinal base 1, 3 function(s), 2 name(s)",
        "is 600s OLDER than the PE header's",
        "@3    0x001000  alpha",
        "@2    0x001033  (by ordinal)",
    ] {
        assert!(out.contains(want), "{want}\n{out}");
    }
    assert_eq!(json(&s.path("x.json"))["rows"].as_array().unwrap().len(), 3);
    let mut bare = Pe::exe();
    bare.push(".text", SCN_CODE, vec![0xc3]);
    s.file("bare.exe", &bare.build());
    assert!(ok(&s.0, &["image", "exports", "bare.exe"]).contains("no export directory"));
}

#[test]
fn image_cmp_exits_on_the_md5_and_reports_sections_clock_and_baseline() {
    let s = Scratch::new("cmp");
    sample_dll(&s);
    let mut c = sample();
    c.pe.timestamp ^= 1;
    c.pe.sections[0].data[0x20] = 0xcc;
    s.file("cand.dll", &c.pe.build());
    let same = run(&s.0, &["image", "cmp", "sample.dll", "sample.dll"]);
    assert_eq!(same.code, 0, "{}", same.stderr);
    assert!(same.stdout.contains("IDENTICAL: the md5s are equal"));
    let o = run(
        &s.0,
        &[
            "image",
            "cmp",
            "sample.dll",
            "cand.dll",
            "--clock",
            "coff.timestamp,pdb.age",
            "--against",
            "sample.dll",
            "--json",
            "c.json",
        ],
    );
    assert_eq!(o.code, 3, "{}\n{}", o.stdout, o.stderr);
    for want in [
        "DIFFERS: the md5s differ, though the size is equal",
        "differing bytes 2 of",
        "  .text              64          1",
        "1 byte(s) of the whole-file figure lie outside every section",
        "COFF TimeDateStamp",
        "DIFFERS",
        "1 of 2 declared clock field(s) identical",
        "delta +2",
        "inside the 12-byte band",
    ] {
        assert!(o.stdout.contains(want), "{want}\n{}", o.stdout);
    }
    let j = json(&s.path("c.json"));
    assert_eq!(j["summary"]["md5_identical"], false);
    assert_eq!(j["summary"]["differing"], 2);
    assert_eq!(j["rows"][0]["name"], ".text");
    assert_eq!(j["rows"][0]["differing"], 1);
    assert_eq!(
        run(
            &s.0,
            &["image", "cmp", "sample.dll", "cand.dll", "--clock", "text"]
        )
        .code,
        1
    );
}

#[test]
fn an_image_id_resolves_through_the_config_after_its_sha256() {
    let s = Scratch::new("config");
    let bytes = sample().pe.build();
    s.file("ref/sample.dll", &bytes);
    let sha = msvc6_base::digest::sha256_hex(&bytes);
    let cfg = format!(
        "schema = 1\n[[image]]\nid = \"APP\"\nreference = \"${{MSVC6T_TEST_REF}}/sample.dll\"\nsha256 = \"{sha}\"\n\
         clock = [\"coff.timestamp\"]\n[[image]]\nid = \"BAD\"\nreference = \"ref/sample.dll\"\nsha256 = \"{zeros}\"\n",
        zeros = "0".repeat(64)
    );
    s.file("msvc6t.toml", cfg.as_bytes());
    let with_env = |args: &[&str]| {
        let o = Command::new(BIN)
            .current_dir(&s.0)
            .env("MSVC6T_TEST_REF", s.path("ref"))
            .args(args)
            .output()
            .unwrap();
        (
            o.status.code(),
            srcio::tool_text(&o.stdout),
            srcio::tool_text(&o.stderr),
        )
    };
    let (code, out, _) = with_env(&["image", "info", "app"]);
    assert_eq!(code, Some(0));
    assert!(out.starts_with("APP: "), "{out}");
    let (code, out, _) = with_env(&["image", "cmp", "APP", "ref/sample.dll"]);
    assert_eq!(code, Some(0));
    assert!(
        out.contains("1 of 1 declared clock field(s) identical"),
        "{out}"
    );
    let (code, _, err) = with_env(&["image", "info", "BAD"]);
    assert_eq!(code, Some(1));
    assert!(
        err.contains(&format!("not the {} declared", "0".repeat(64))),
        "{err}"
    );
    assert!(
        err.contains("msvc6t.toml:10:") && err.contains("image[1].sha256"),
        "{err}"
    );
    let short = Scratch::new("config-short");
    short.file(
        "msvc6t.toml",
        b"schema = 1\n[[image]]\nid = \"A\"\nreference = \"a.dll\"\nsha256 = \"00\"\n",
    );
    let bad = run(&short.0, &["image", "info", "A"]);
    assert_eq!(bad.code, 1);
    assert!(
        bad.stderr.contains("msvc6t.toml:5:") && bad.stderr.contains("image[0].sha256"),
        "{}",
        bad.stderr
    );
    let unset = run(&s.0, &["image", "info", "APP"]);
    assert_eq!(unset.code, 1);
    assert!(unset.stderr.contains("MSVC6T_TEST_REF"), "{}", unset.stderr);
    let none = run(&s.0, &["image", "info", "NOPE"]);
    assert!(
        none.stderr.contains("not an [[image]] id"),
        "{}",
        none.stderr
    );
    let missing = run(&s.0, &["--config", "no.toml", "image", "info", "APP"]);
    assert!(
        missing.stderr.contains("no such file"),
        "{}",
        missing.stderr
    );
}

#[test]
fn image_postlink_writes_the_declared_rows_and_clock_to_a_fixed_point() {
    let s = Scratch::new("postlink");
    let p = postlinked();
    s.file("ref.dll", &p.reference);
    let mut ours = p.ours.clone();
    ours[usize::try_from(p.sample.pe.layout().e_lfanew).unwrap() + 8] ^= 0x40;
    s.file("ours.dll", &ours);
    let rows = format!(
        "[[residue]]\nkind = \"header-slack\"\noff = {}\nlen = 8\n\
         [[residue]]\nkind = \"text-pad\"\nrva = {}\nlen = 6\nnote = \"a stub\"\n\
         [[residue]]\nkind = \"header-field\"\nat = \"size-of-code\"\noff = {}\nlen = 4\n\
         [[residue]]\nkind = \"import-stamps\"\nat = \"import-stamps\"\noff = {}\nlen = 8\n\
         [[residue]]\nkind = \"header-field\"\nat = \"section-tail\"\noff = {}\nlen = 8\n",
        p.slack,
        p.sample.text + 0x10,
        p.size_of_code,
        p.stamps,
        p.tail
    );
    s.file("rows.toml", rows.as_bytes());
    let dry = ok(
        &s.0,
        &[
            "image",
            "postlink",
            "ref.dll",
            "ours.dll",
            "--rows",
            "rows.toml",
            "--clock",
            "coff.timestamp",
            "--verbose",
        ],
    );
    assert!(dry.contains("5 residue row(s), 1 clock field(s)"), "{dry}");
    assert!(dry.contains("(a stub)"), "{dry}");
    assert!(
        dry.contains("27 B owed by the step in 5 row(s); 0 BLOCKED"),
        "{dry}"
    );
    assert_eq!(std::fs::read(s.path("ours.dll")).unwrap(), ours);
    let applied = ok(
        &s.0,
        &[
            "image",
            "postlink",
            "ref.dll",
            "ours.dll",
            "--rows",
            "rows.toml",
            "--clock",
            "coff.timestamp",
            "--apply",
            "--json",
            "p.json",
        ],
    );
    assert!(applied.contains("WROTE 32 byte(s)"), "{applied}");
    assert!(
        applied.contains("every non-blocked row and clock field now matches"),
        "{applied}"
    );
    assert_eq!(std::fs::read(s.path("ours.dll")).unwrap(), p.reference);
    let j = json(&s.path("p.json"));
    assert_eq!(j["rows"].as_array().unwrap().len(), 5);
    assert_eq!(j["summary"]["totals"]["owed"], 0);
    s.file(
        "bad.toml",
        b"[[residue]]\nkind = \"text-pad\"\nlen = 1\nwhat = 2\n",
    );
    assert_eq!(
        run(
            &s.0,
            &[
                "image", "postlink", "ref.dll", "ours.dll", "--rows", "bad.toml"
            ]
        )
        .code,
        1
    );
}

#[test]
fn image_res_lists_resources_and_writes_a_res_file() {
    let s = Scratch::new("res");
    sample_dll(&s);
    let out = ok(
        &s.0,
        &[
            "image",
            "res",
            "sample.dll",
            "--out",
            "x.res",
            "--json",
            "r.json",
        ],
    );
    assert!(
        out.contains("3 resource(s) of 3 type(s), 54 byte(s) of data"),
        "{out}"
    );
    assert!(out.contains("GROUP_ICON     102"), "{out}");
    assert!(out.contains("WAVE           CLICK"), "{out}");
    let res = std::fs::read(s.path("x.res")).unwrap();
    assert_eq!(&res[..8], &[0, 0, 0, 0, 32, 0, 0, 0]);
    assert_eq!(json(&s.path("r.json"))["rows"].as_array().unwrap().len(), 3);
}

fn obj(stamp: u32, label: &str, code: u8) -> Vec<u8> {
    let mut o = Obj::new();
    o.timestamp = stamp;
    o.code(".text", &[code, 0xc3]);
    o.plain(label, 0, 1, 0, CLASS_LABEL);
    o.build()
}

#[test]
fn obj_diff_masks_the_stamp_and_exits_3_on_a_difference() {
    let s = Scratch::new("obj");
    s.file("a/x.obj", &obj(1, "$L100", 0x90));
    s.file("b/x.obj", &obj(2, "$L100", 0x90));
    s.file("a/sub/y.obj", &obj(1, "$L100", 0x90));
    s.file("b/sub/y.obj", &obj(1, "$L200", 0x90));
    let out = run(&s.0, &["obj", "diff", "a", "b", "--json", "o.json"]);
    assert_eq!(out.code, 3, "{}", out.stdout);
    assert!(out.stdout.contains("BYTES    sub/y.obj"), "{}", out.stdout);
    assert!(
        out.stdout.contains("2 object(s): 1 identical"),
        "{}",
        out.stdout
    );
    let norm = ok(&s.0, &["obj", "diff", "a", "b", "--normalize-labels"]);
    assert!(norm.contains("2 identical"), "{norm}");
    s.file("a/z.obj", &obj(1, "$L1", 0xcc));
    let missing = run(&s.0, &["obj", "diff", "a", "b"]);
    assert_eq!(missing.code, 3);
    assert!(
        missing.stdout.contains("MISSING  z.obj"),
        "{}",
        missing.stdout
    );
    let pair = run(
        &s.0,
        &["obj", "diff", "a/z.obj", "a/x.obj", "--normalize-labels"],
    );
    assert_eq!(pair.code, 3);
    assert!(
        pair.stdout.contains("canonical line(s) differ"),
        "{}",
        pair.stdout
    );
    let j = json(&s.path("o.json"));
    assert_eq!(j["rows"][0]["status"], "bytes");
    assert_eq!(run(&s.0, &["obj", "diff", "nothing", "b"]).code, 1);
}

#[test]
fn lib_diff_and_census_read_archives() {
    let s = Scratch::new("lib");
    let mut a = Library::default();
    a.member("one.obj", obj(1, "$L1", 0x90), &["_one"]);
    a.member(
        "U.dll",
        short_import("U.dll", "_Beep@8", 0, 0, 3),
        &["__imp__Beep@8"],
    );
    let mut b = a.clone();
    b.date = 99;
    s.file("a.lib", &a.build());
    s.file("b.lib", &b.build());
    let same = ok(&s.0, &["lib", "diff", "a.lib", "b.lib", "--json", "l.json"]);
    assert!(same.contains("VERDICT: IDENTICAL MEMBERS"), "{same}");
    b.members[0].data = obj(1, "$L1", 0xcc);
    s.file("b.lib", &b.build());
    let d = run(&s.0, &["lib", "diff", "a.lib", "b.lib"]);
    assert_eq!(d.code, 3);
    assert!(
        d.stdout
            .contains("one.obj: @comp.id ours none / reference none"),
        "{}",
        d.stdout
    );
    assert!(d.stdout.contains("sections: row 0 of 1/1"), "{}", d.stdout);
    let c = ok(&s.0, &["lib", "census", "a.lib", "--json", "c.json"]);
    assert!(c.contains("unstamped: no @comp.id: 1 member(s)"), "{c}");
    assert!(c.contains("short import members: 1 member(s)"), "{c}");
    let m = ok(
        &s.0,
        &["lib", "census", "a.lib", "--members", "--only", "Beep"],
    );
    assert!(
        m.contains("_Beep@8 from U.dll, name type undecorate"),
        "{m}"
    );
    assert!(m.contains("1 member(s) shown"), "{m}");
    assert_eq!(json(&s.path("c.json"))["rows"].as_array().unwrap().len(), 2);
    s.file("not.lib", b"plain text");
    assert_eq!(run(&s.0, &["lib", "census", "not.lib"]).code, 1);
}

fn quick(dir: &Path, args: &[&str]) -> Out {
    let limit = msvc6_fixture::backstop::LIMIT;
    let o = msvc6_toolchain::exec::run(&Cmd {
        env_remove: vec!["MSVC6T_CONFIG".into()],
        ..Cmd::new(BIN).args(args).cwd(dir).timeout(Some(limit))
    })
    .unwrap();
    assert!(
        !o.timed_out,
        "{args:?}: still running after {limit:?}, so it loops; killed"
    );
    Out {
        code: o.status.unwrap_or(-1),
        stdout: srcio::tool_text(&o.stdout),
        stderr: srcio::tool_text(&o.stderr),
    }
}

#[test]
fn malformed_inputs_fail_fast_with_a_reason() {
    let s = Scratch::new("malformed");
    let sm = sample();
    let b = sm.pe.build();
    let at = usize::try_from(sm.pe.rva_to_file(sm.rsrc).unwrap()).unwrap();
    s.file("cut.dll", &b[..at + 20]);
    let res = quick(&s.0, &["image", "res", "cut.dll"]);
    assert_eq!(res.code, 1, "{}", res.stdout);
    assert!(
        res.stderr.contains("run past the file bytes"),
        "{}",
        res.stderr
    );
    let mut l = Library::default();
    l.member("one.obj", obj(1, "$L1", 0x90), &["_one"]);
    s.file("x.lib", &l.build());
    s.file("good.obj", &obj(1, "$L1", 0x90));
    for mask in [
        "--normalize-labels",
        "--mask-line-numbers",
        "--ignore-debug",
    ] {
        let o = quick(&s.0, &["obj", "diff", "x.lib", "good.obj", mask]);
        assert_eq!(o.code, 1, "{mask}: {}", o.stdout);
        assert!(o.stderr.contains("x.lib"), "{}", o.stderr);
    }
    s.file("empty.dll", b"");
    let rich = quick(&s.0, &["image", "rich", "empty.dll"]);
    assert_eq!(rich.code, 1);
    assert!(rich.stderr.contains("not an MZ image"), "{}", rich.stderr);
}

#[test]
fn segrank_names_an_object_it_cannot_read_and_exits_1() {
    use msvc6_fixture::coff::{SCN_COMDAT, SCN_TEXT, SELECT_ANY, Section};
    let s = Scratch::new("segrank-refused");
    s.file("ref/sample.dll", &sample().pe.build());
    s.file(
        "msvc6t.toml",
        b"schema = 1\n[[image]]\nid = \"APP\"\nreference = \"ref/sample.dll\"\n",
    );
    let mut o = Obj::new();
    let t = o.section(Section {
        name: ".text".into(),
        characteristics: SCN_TEXT | SCN_COMDAT,
        data: vec![0x33, 0xc0, 0xc3],
        ..Section::default()
    });
    o.section_symbol(t, SELECT_ANY, 0);
    o.external("?unnamed@@YAXXZ", 0, t);
    s.file("objs/a.obj", &o.build());
    let args = |j: &'static str| ["segrank", "APP", "--objs", "objs", "--no-libs", "--json", j];
    let clean = run(&s.0, &args("c.json"));
    assert_eq!(clean.code, 0, "{}\n{}", clean.stdout, clean.stderr);
    assert!(!clean.stdout.contains("REFUSED"), "{}", clean.stdout);
    assert_eq!(
        json(&s.path("c.json"))["summary"]["refused"],
        serde_json::json!([])
    );
    s.file("objs/b.obj", b"not an object");
    let out = run(&s.0, &args("r.json"));
    assert_eq!(out.code, 1, "{}\n{}", out.stdout, out.stderr);
    assert!(
        out.stderr.contains("segrank: REFUSED") && out.stderr.contains("b.obj"),
        "{}",
        out.stderr
    );
    assert!(
        out.stdout
            .contains("REFUSED: 1 of 2 object(s) unread, their COMDATs seat nothing: b.obj"),
        "{}",
        out.stdout
    );
    assert!(
        out.stdout
            .contains("total: 0 differing over 0 function(s) of 0 asked"),
        "{}",
        out.stdout
    );
    let j = json(&s.path("r.json"));
    let refused = j["summary"]["refused"].as_array().unwrap();
    assert_eq!(refused.len(), 1, "{j}");
    assert!(refused[0]["object"].as_str().unwrap().ends_with("b.obj"));
    assert!(!refused[0]["error"].as_str().unwrap().is_empty());
}

#[test]
fn mac_tracebacks_prints_a_table_without_a_tb_offset_at_start_0_size_minus_1_as_its_help_says() {
    use msvc6_fixture::mac::{
        ATTR_PURE_INSTRUCTIONS, BLR, CPU_PPC, Function, MachO, MachSection, Traceback, code_section,
    };
    let s = Scratch::new("tracebacks");
    let (mut code, spans) = code_section(&[Function {
        words: vec![0x3860_0001, BLR],
        name: ".tick__5ClockFv".into(),
    }]);
    code.extend_from_slice(&BLR.to_be_bytes());
    let at = code.len();
    code.extend(Traceback::named(".tock__5ClockFv", None).bytes());
    let m = MachO {
        cpu: CPU_PPC,
        big_endian: true,
        sections: vec![MachSection {
            segment: "__TEXT".into(),
            name: "__text".into(),
            addr: 0x1000,
            data: code,
            flags: ATTR_PURE_INSTRUCTIONS,
            ..MachSection::default()
        }],
        ..MachO::default()
    };
    s.file("ppc.macho", &m.build());
    let out = ok(&s.0, &["mac", "tracebacks", "ppc.macho"]);
    let (start, first) = spans[0];
    assert!(
        out.contains(&format!(
            "0x{start:06x}\t0x{first:06x}\t{}\t.tick__5ClockFv",
            first - start
        )),
        "{out}"
    );
    assert!(
        out.contains(&format!("0x000000\t0x{at:06x}\t-1\t.tock__5ClockFv")),
        "{out}"
    );
    let help = ok(&s.0, &["mac", "tracebacks", "--help"]);
    assert!(
        help.split_whitespace()
            .collect::<Vec<_>>()
            .join(" ")
            .contains("a table with none prints 0 and a size of -1"),
        "{help}"
    );
}
