#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_fixture::sample::sample;
use std::path::{Path, PathBuf};
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

struct Scratch(PathBuf);

impl Scratch {
    fn new(tag: &str) -> Self {
        let d = std::env::temp_dir().join(format!("msvc6t-project-{tag}-{}", std::process::id()));
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

fn fixtures() -> PathBuf {
    std::fs::canonicalize(Path::new(env!("CARGO_MANIFEST_DIR")).join("../../fixtures/project"))
        .unwrap()
}

fn project(s: &Scratch) -> u32 {
    let smp = sample();
    let bytes = smp.pe.build();
    s.file("ref/app.dll", &bytes);
    let t = smp.text;
    let va = |r: u32| 0x1000_0000 + r;
    let ghidra = format!(
        "[{{\"name\": \"alpha\", \"addr\": \"{:08x}\", \"thunk\": 0, \"ins\": [[\"{:08x}\", \"PUSH EBP\", \"55\", 0], \
         [\"{:08x}\", \"RET\", \"c3\", 0]]}}, {{\"name\": \"FUN_{:08x}\", \"addr\": \"{:08x}\", \"ins\": []}}]",
        va(t),
        va(t),
        va(t + 9),
        va(t + 0x30),
        va(t + 0x30)
    );
    s.file("analysis/app.functions.json", ghidra.as_bytes());
    let map = format!(
        " app\r\n\r\n Preferred load address is 10000000\r\n\r\n  Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
         0001:00000000       _alpha                     {:08x} f   main.obj\r\n\
         0001:00000030       _beta                      {:08x} f   util:table.obj\r\n\r\n Static symbols\r\n\r\n\
         0001:00000033       _helper                    {:08x} f   main.obj\r\n",
        va(t),
        va(t + 0x30),
        va(t + 0x33)
    );
    s.file("out/app.map", map.as_bytes());
    s.file(
        "names/app.tsv",
        format!(
            "rva\tname\tkind\tsize\tscope\n{:#x}\tAlpha\tfunction\t10\n",
            t
        )
        .as_bytes(),
    );
    s.file(
        "app.csv",
        format!(
            "address,name,type,size\n{:#x},Beta,function,3\n{:#x},g_table,global,\n",
            va(t + 0x30),
            va(smp.data)
        )
        .as_bytes(),
    );
    s.file(
        "src/a.cpp",
        format!(
            "// FUNCTION: APP {:#x}\nint __cdecl Helper(void)\n{{\n}}\n",
            va(t + 0x33)
        )
        .as_bytes(),
    );
    let cfg = format!(
        "schema = 1\n\n[[image]]\nid = \"APP\"\nreference = \"ref/app.dll\"\nsha256 = \"{}\"\nproject = {{ dsw = '{}', config = \"app - Win32 Release\" }}\n\
         map = \"out/app.map\"\n\
         functions = [\"ghidra-json:analysis/app.functions.json\", \"map\", \"reccmp-csv:app.csv\", \"discover\"]\n\
         names = [\"tsv:names/app.tsv\", \"map\", \"exports\", \"imports\", \"reccmp-markers:src\", \"reccmp-csv:app.csv\"]\n\
         clock = [\"coff.timestamp\", \"debug.timestamp\", \"pdb.signature\", \"pdb.age\", \"export.timestamp\"]\n",
        msvc6_base::digest::sha256_hex(&bytes),
        fixtures().join("ws.dsw").display()
    );
    s.file("msvc6t.toml", cfg.as_bytes());
    t
}

#[test]
fn dsp_show_resolves_every_tu_of_the_fixture_workspace() {
    let s = Scratch::new("dsp");
    let ws = fixtures().join("ws.dsw");
    let ws = ws.display().to_string();
    let j = s.path("d.json");
    let out = ok(
        &s.0,
        &[
            "dsp",
            "show",
            "--dsw",
            &ws,
            "--project-config",
            "app - Win32 Release",
            "--json",
            j.to_str().unwrap(),
        ],
    );
    assert!(
        out.contains("dsp show (none): 8 TU(s) in 3 project(s), 0 refused"),
        "{out}"
    );
    let v = json(&j);
    let rows = v["rows"].as_array().unwrap();
    assert_eq!(rows.len(), 8);
    let render = rows.iter().find(|r| r["stem"] == "render").unwrap();
    assert_eq!(render["cflags"].as_array().unwrap().len(), 8);
    assert_eq!(
        render["switches"][render["switches"].as_array().unwrap().len() - 1],
        "/Ob2"
    );
    let out = ok(&s.0, &["dsp", "show", "--dsw", &ws, "--tu", "main"]);
    assert!(
        out.contains("[app - Win32 Debug]"),
        "the default configuration is app.dsp's CFG= line: {out}"
    );
    assert!(
        out.contains("note: zip: built in its one configuration `zip - Win32 Release`"),
        "{out}"
    );
    let missing = run(&s.0, &["dsp", "show", "--dsw", &ws, "--tu", "nothing"]);
    assert_eq!(missing.code, 1);
    assert!(
        missing.stdout.contains("NO TU `nothing`"),
        "{}",
        missing.stdout
    );
    let dsp = fixtures().join("util/util.dsp");
    let out = ok(
        &s.0,
        &[
            "dsp",
            "show",
            "--dsp",
            dsp.to_str().unwrap(),
            "--project-config",
            "util - Win32 Release",
        ],
    );
    assert!(
        out.contains("/Ycutil.h") && out.contains("2 TU(s) in 1 project(s)"),
        "{out}"
    );
    let bad = run(
        &s.0,
        &[
            "dsp",
            "show",
            "--dsw",
            &ws,
            "--project-config",
            "app - Win32 Profile",
        ],
    );
    assert_eq!(bad.code, 1);
    assert!(
        bad.stdout.contains("REFUSED app (every file)"),
        "{}",
        bad.stdout
    );
}

#[test]
fn check_functions_and_names_read_every_provider() {
    let s = Scratch::new("providers");
    let t = project(&s);
    let out = ok(&s.0, &["check"]);
    assert!(
        out.contains("8 TU(s) in 3 project(s), every source and /I directory present"),
        "{out}"
    );
    assert!(
        out.contains("check: ") && out.contains(", 0 failed"),
        "{out}"
    );
    let j = s.path("f.json");
    let out = ok(&s.0, &["functions", "APP", "--json", j.to_str().unwrap()]);
    assert!(out.contains("ghidra-json: "), "{out}");
    let v = json(&j);
    let rows = v["rows"].as_array().unwrap();
    let at = |rva: u32| {
        rows.iter()
            .find(|r| r["rva"] == rva)
            .unwrap_or_else(|| panic!("no row {rva:#x}: {rows:?}"))
    };
    let alpha = at(t);
    assert_eq!(alpha["provider"], "ghidra-json");
    assert_eq!(alpha["name"], "alpha");
    assert_eq!(alpha["end"], t + 10);
    assert_eq!(alpha["object"], "main.obj");
    let beta = at(t + 0x30);
    assert_eq!(beta["provider"], "ghidra-json");
    assert_eq!(
        beta["name"], "_beta",
        "the generated name is dropped and the map fills it"
    );
    assert_eq!(beta["object"], "util:table.obj");
    assert_eq!(beta["end"], t + 0x33, "the reccmp size fills the extent");
    assert_eq!(at(t + 0x33)["provider"], "map");
    assert!(v["summary"]["providers"].as_array().unwrap().len() == 4);
    let out = ok(
        &s.0,
        &["functions", "APP", "--at", &format!("{:#x}", t + 4)],
    );
    assert!(out.lines().next().unwrap().contains("alpha"), "{out}");
    let j = s.path("n.json");
    ok(&s.0, &["names", "APP", "--json", j.to_str().unwrap()]);
    let v = json(&j);
    let rows = v["rows"].as_array().unwrap();
    let providers: Vec<&str> = v["summary"]["providers"]
        .as_array()
        .unwrap()
        .iter()
        .map(|p| p["provider"].as_str().unwrap())
        .collect();
    assert_eq!(
        providers,
        [
            "tsv",
            "map",
            "exports",
            "imports",
            "reccmp-markers",
            "reccmp-csv"
        ]
    );
    let first = rows.iter().find(|r| r["rva"] == t).unwrap();
    assert_eq!(first["name"], "Alpha");
    assert!(
        rows.iter()
            .any(|r| r["name"] == "KERNEL32.dll!GetVersion" && r["kind"] == "import")
    );
    assert!(rows.iter().any(|r| r["name"] == "WS2_32.dll!#23"));
    assert!(
        rows.iter()
            .any(|r| r["name"] == "Helper" && r["provider"] == "reccmp-markers")
    );
    assert!(
        rows.iter()
            .any(|r| r["name"] == "_helper" && r["scope"] == "main")
    );
    assert!(
        rows.iter()
            .any(|r| r["name"] == "g_table" && r["kind"] == "data")
    );
    let out = ok(&s.0, &["names", "APP", "--at", &format!("{t:#x}")]);
    assert!(out.lines().next().unwrap().contains("Alpha"), "{out}");
    let none = run(&s.0, &["names", "APP", "--name", "nobody"]);
    assert_eq!(none.code, 1);
}

#[test]
fn check_fails_on_a_wrong_input_and_names_where() {
    let s = Scratch::new("checkfail");
    project(&s);
    let text = srcio::own_read(s.path("msvc6t.toml")).unwrap();
    s.file(
        "msvc6t.toml",
        text.replace("tsv:names/app.tsv", "tsv:names/none.tsv")
            .as_bytes(),
    );
    let out = run(&s.0, &["check"]);
    assert_eq!(out.code, 1, "{}", out.stdout);
    assert!(
        out.stdout.contains("FAIL") && out.stdout.contains("image[0].names[0]"),
        "{}",
        out.stdout
    );
    s.file(
        "msvc6t.toml",
        text.replace("sha256 =", "sha265 =").as_bytes(),
    );
    let out = run(&s.0, &["check"]);
    assert_eq!(out.code, 1);
    assert!(
        out.stderr
            .contains("msvc6t.toml:6:1: image[0].sha265: unknown field `sha265`"),
        "{}",
        out.stderr
    );
    s.file("msvc6t.toml", text.replace("clock =", "clok =").as_bytes());
    let out = run(&s.0, &["dsp", "show", "APP"]);
    assert_eq!(out.code, 1);
    assert!(
        out.stderr
            .contains("msvc6t.toml:11:1: image[0].clok: unknown field `clok`"),
        "{}",
        out.stderr
    );
}

#[test]
fn init_writes_a_config_the_strict_loader_and_check_accept() {
    let s = Scratch::new("init");
    s.file("ref/app.dll", &sample().pe.build());
    std::fs::create_dir_all(s.path("vs6/VC98/BIN")).unwrap();
    let ws = fixtures().join("ws.dsw");
    let vs6 = s.path("vs6");
    let out = ok(
        &s.0,
        &[
            "init",
            "--image",
            "ref/app.dll",
            "--dsw",
            ws.to_str().unwrap(),
            "--vc6",
            vs6.to_str().unwrap(),
        ],
    );
    assert!(
        out.contains("id = 'APP'") && out.contains("config = 'app - Win32 Debug'"),
        "{out}"
    );
    assert!(out.contains("clock = [\"coff.timestamp\", \"debug.timestamp\", \"pdb.signature\", \"pdb.age\", \"export.timestamp\"]"), "{out}");
    let again = run(&s.0, &["init", "--image", "ref/app.dll"]);
    assert_eq!(again.code, 1);
    assert!(again.stderr.contains("--force"), "{}", again.stderr);
    let out = ok(&s.0, &["check"]);
    assert!(out.contains("7 TU(s) in 3 project(s)"), "{out}");
    let out = ok(&s.0, &["dsp", "show", "APP", "--tu", "render"]);
    assert!(
        out.contains("/GX") && out.contains("[app - Win32 Debug]"),
        "{out}"
    );
}

#[test]
fn an_input_naming_an_unset_optional_variable_is_skipped_and_left_out() {
    let s = Scratch::new("optional");
    project(&s);
    let var = "MSVC6T_TEST_OPTIONAL_ANALYSIS";
    let cfg = srcio::own_read(s.path("msvc6t.toml"))
        .unwrap()
        .replacen(
            "schema = 1\n",
            &format!("schema = 1\noptional = [\"{var}\"]\n"),
            1,
        )
        .replace(
            "\"ghidra-json:analysis/app.functions.json\"",
            &format!("\"ghidra-json:${{{var}}}/app.functions.json\""),
        );
    s.file("msvc6t.toml", cfg.as_bytes());
    let with = |set: bool, args: &[&str]| {
        let mut c = Command::new(BIN);
        c.current_dir(&s.0).env_remove("MSVC6T_CONFIG").args(args);
        if set {
            c.env(var, s.path("analysis"));
        } else {
            c.env_remove(var);
        }
        let o = c.output().unwrap();
        (o.status.code(), srcio::tool_text(&o.stdout))
    };
    let (code, out) = with(false, &["check"]);
    assert_eq!(code, Some(0), "{out}");
    assert!(
        out.contains(&format!(
            "absent: ${{{var}}} is not set, and the config lists it as optional"
        )) && out.contains(", 0 failed, 1 skipped (an optional input is unset)"),
        "{out}"
    );
    let j = s.path("absent.json");
    let (code, _) = with(false, &["functions", "APP", "--json", j.to_str().unwrap()]);
    assert_eq!(code, Some(0));
    let v = json(&j);
    let first = &v["summary"]["providers"][0];
    assert_eq!(first["provider"], "ghidra-json");
    assert_eq!(first["records"], 0);
    assert!(
        first["notes"][0].as_str().unwrap().starts_with("absent: "),
        "{first}"
    );
    let (code, out) = with(true, &["check"]);
    assert_eq!(code, Some(0), "{out}");
    assert!(
        !out.contains("skipped") && out.contains(", 0 failed"),
        "{out}"
    );
    let (code, _) = with(true, &["functions", "APP", "--json", j.to_str().unwrap()]);
    assert_eq!(code, Some(0));
    assert_eq!(json(&j)["summary"]["providers"][0]["records"], 2);
}
