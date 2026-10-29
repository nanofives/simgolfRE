#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_image::clock::{ALL, Field};
use msvc6_project::config::{DroppedSource, FunctionSource, NameSource, Project, TABLES};
use std::path::{Path, PathBuf};

const FILE: &str = "/cfg/proj/msvc6t.toml";

const GOOD: &str = r#"schema = 1

[toolchain.vc6]
root = "/opt/vs6"
expect = { cl = "12.00.8168", link = "6.00.8168" }

[runner]
kind = "wine"
job_memory = "1 GiB"

[cache]
max = "2 GiB"

[[image]]
id = "GAME"
reference = "ref/game.exe"
sha256 = "a78b196737f86600ab95693334a7015e5638cde581c6c78eb2c6227c7b9834ab"
compiler = "vc6"
linker = "vc6"
project = { dsw = "src/game.dsw", config = "game - Win32 Release" }
functions = ["ghidra-json:analysis/game.functions.json", "map:out/game.map", "reccmp-csv:x.csv", "discover"]
names = ["tsv:names/game.tsv", "map", "exports", "imports", "reccmp-markers:src", "reccmp-csv:x.csv"]
dropped = ["tsv:names/gone.tsv"]
map = "out/game.map"
clock = ["coff.timestamp", "debug.timestamp", "pdb.signature", "pdb.age", "export.timestamp"]

[[image.tu]]
source = "src/extra.cpp"
cflags = ["/O2"]

[[image.residue]]
kind = "text-pad"
rva = 0x1234
len = 12

[[image]]
id = "ENGINE"
reference = "${MSVC6T_CONFIG_TEST_UNSET:-ref}/engine.dll"

[image.build]
toolchain = "vc6"
msdev = [{ dir = 'C:\e', open = "engine.dsp", config = "engine - Win32 Debug" }]
output = 'C:\e\Debug\engine.dll'

[reccmp]
project = "reccmp-project.yml"

[source]
root = "src"
exclude = ["third_party"]
"#;

fn parse(text: &str) -> anyhow::Result<Project> {
    Project::parse(text, Path::new(FILE))
}

fn err(text: &str) -> String {
    format!("{:#}", parse(text).unwrap_err())
}

fn line_of(text: &str, needle: &str) -> usize {
    text.lines().position(|l| l.contains(needle)).unwrap() + 1
}

fn names_file_line_key(e: &str, line: usize, key: &str) {
    assert!(
        e.starts_with(&format!("{FILE}:{line}:")),
        "the error does not open on {FILE}:{line}: {e}"
    );
    assert!(
        e.contains(&format!(": {key}: ")),
        "the error does not name `{key}`: {e}"
    );
}

#[test]
fn the_one_schema_reads_every_table() {
    let p = parse(GOOD).unwrap();
    assert_eq!(p.schema, 1);
    assert_eq!(p.dir, PathBuf::from("/cfg/proj"));
    assert_eq!(p.images.len(), 2);
    let g = &p.images[0];
    assert_eq!(g.clock, ALL.to_vec());
    assert_eq!(g.functions.len(), 4);
    assert_eq!(g.functions[3], FunctionSource::Discover);
    assert_eq!(g.names[1], NameSource::Map(None));
    assert_eq!(g.names[4], NameSource::ReccmpMarkers("src".into()));
    assert_eq!(g.dropped, [DroppedSource::Tsv("names/gone.tsv".into())]);
    assert!(p.images[1].dropped.is_empty());
    assert_eq!(g.tu[0].cflags, ["/O2"]);
    assert_eq!(g.residue.len(), 1);
    assert_eq!(
        p.reference_path(0).unwrap(),
        PathBuf::from("/cfg/proj/ref/game.exe")
    );
    assert_eq!(
        p.reference_path(1).unwrap(),
        PathBuf::from("/cfg/proj/ref/engine.dll")
    );
    assert_eq!(p.image("engine").unwrap().0, 1);
    let tc = p.toolchain_config();
    assert_eq!(
        tc.images[1].build.as_ref().unwrap().msdev[0].config,
        "engine - Win32 Debug"
    );
    assert_eq!(tc.images[0].clock, ALL.to_vec());
    assert_eq!(p.source_config().source.exclude, ["third_party"]);
    assert_eq!(TABLES.len(), 7);
}

#[test]
fn a_typo_is_refused_with_its_file_line_and_key() {
    for (good, typo, key) in [
        ("sha256 = \"a78b", "sha265 = \"a78b", "image[0].sha265"),
        ("clock = [\"coff", "clok = [\"coff", "image[0].clok"),
        (
            "compiler = \"vc6\"",
            "compilr = \"vc6\"",
            "image[0].compilr",
        ),
        ("job_memory", "job_memroy", "runner.job_memroy"),
        ("max = \"2 GiB\"", "mxa = \"2 GiB\"", "cache.mxa"),
        (
            "cflags = [\"/O2\"]",
            "cflag = [\"/O2\"]",
            "image[0].tu[0].cflag",
        ),
        ("output = 'C:", "outptu = 'C:", "image[1].build.outptu"),
        ("project = \"reccmp", "projet = \"reccmp", "reccmp.projet"),
        ("exclude = [\"third", "exclud = [\"third", "source.exclud"),
        ("len = 12", "length = 12", "image[0].residue[0].length"),
    ] {
        let bad = GOOD.replacen(good, typo, 1);
        assert_ne!(bad, GOOD, "{typo}");
        let e = err(&bad);
        names_file_line_key(&e, line_of(&bad, typo), key);
        assert!(e.contains("unknown field"), "{e}");
    }
}

#[test]
fn an_unknown_table_and_a_bad_value_are_refused_by_line() {
    let bad = GOOD.replacen("[runner]", "[runnr]", 1);
    let e = err(&bad);
    names_file_line_key(&e, line_of(&bad, "[runnr]"), "runnr");
    let bad = GOOD.replacen("\"pdb.age\"", "\"pdb.agee\"", 1);
    let e = err(&bad);
    names_file_line_key(&e, line_of(&bad, "pdb.agee"), "image[0].clock[3]");
    assert!(e.contains("pdb.agee"), "{e}");
    let bad = GOOD.replacen("sha256 = \"a78b", "sha256 = \"78b", 1);
    let e = err(&bad);
    names_file_line_key(&e, line_of(&bad, "sha256"), "image[0].sha256");
    assert!(e.contains("64 hexadecimal"), "{e}");
    let bad = GOOD.replacen("\"discover\"", "\"discovr\"", 1);
    let e = err(&bad);
    names_file_line_key(&e, line_of(&bad, "discovr"), "image[0].functions[3]");
    let bad = GOOD.replacen("\"exports\"", "\"exports:x\"", 1);
    let e = err(&bad);
    names_file_line_key(&e, line_of(&bad, "exports:x"), "image[0].names[2]");
    for (spec, why) in [
        ("map:names/gone.tsv", "the providers are tsv:<file>"),
        ("tsv:", "`tsv` needs a path"),
    ] {
        let bad = GOOD.replacen("tsv:names/gone.tsv", spec, 1);
        let e = err(&bad);
        names_file_line_key(&e, line_of(&bad, "dropped = "), "image[0].dropped[0]");
        assert!(e.contains(why), "{e}");
    }
    let bad = GOOD.replacen("schema = 1", "schema = \"1\"", 1);
    names_file_line_key(&err(&bad), 1, "schema");
    let bad = GOOD.replacen("kind = \"text-pad\"", "kind = \"text-padding\"", 1);
    let e = err(&bad);
    names_file_line_key(
        &e,
        line_of(&bad, "text-padding"),
        "image[0].residue[0].kind",
    );
}

#[test]
fn a_missing_key_names_the_key_it_wants() {
    let bad = GOOD.replacen("reference = \"ref/game.exe\"\n", "", 1);
    let e = err(&bad);
    assert!(e.starts_with(FILE), "{e}");
    assert!(
        e.contains("image[0].reference") || e.contains("reference"),
        "{e}"
    );
    assert!(e.contains("missing field `reference`"), "{e}");
    let e = err(&GOOD.replacen("schema = 1\n", "", 1));
    assert!(e.contains(": schema: missing field `schema`"), "{e}");
}

#[test]
fn what_parses_but_is_wrong_is_refused_by_line_too() {
    let cases = [
        ("schema = 1", "schema = 2", "schema = 2", "schema"),
        (
            "compiler = \"vc6\"",
            "compiler = \"vc7\"",
            "compiler = \"vc7\"",
            "image[0].compiler",
        ),
        (
            "toolchain = \"vc6\"",
            "toolchain = \"sp5\"",
            "toolchain = \"sp5\"",
            "image[1].build.toolchain",
        ),
        (
            "id = \"ENGINE\"",
            "id = \"game\"",
            "id = \"game\"",
            "image[1].id",
        ),
        (
            "id = \"GAME\"",
            "id = \"GA ME\"",
            "id = \"GA ME\"",
            "image[0].id",
        ),
        (
            "link = \"6.00.8168\"",
            "lnk = \"6.00.8168\"",
            "expect = {",
            "toolchain.vc6.expect.lnk",
        ),
        (
            "layout = \"vs6\"",
            "layout = \"vs7\"",
            "layout",
            "toolchain.vc6.layout",
        ),
        (
            "job_memory = \"1 GiB\"",
            "job_memory = \"1 parsec\"",
            "job_memory",
            "runner.job_memory",
        ),
        (
            "\"pdb.age\", ",
            "\"pdb.age\", \"pdb.age\", ",
            "clock = [",
            "image[0].clock",
        ),
        (
            "config = \"game - Win32 Release\" }",
            "config = \"game - Win32 Release\", dsp = \"a.dsp\" }",
            "project = {",
            "image[0].project",
        ),
    ];
    for (good, bad_text, at, key) in cases {
        let text = if good == "layout = \"vs6\"" {
            GOOD.replacen(
                "[toolchain.vc6]\n",
                "[toolchain.vc6]\nlayout = \"vs7\"\n",
                1,
            )
        } else {
            GOOD.replacen(good, bad_text, 1)
        };
        assert_ne!(text, GOOD, "{bad_text}");
        let e = err(&text);
        names_file_line_key(&e, line_of(&text, at), key);
    }
}

#[test]
fn the_clock_vocabulary_is_msvc6_images_and_the_build_applies_it() {
    for f in ALL {
        let text = GOOD.replacen(
            "clock = [\"coff.timestamp\", \"debug.timestamp\", \"pdb.signature\", \"pdb.age\", \"export.timestamp\"]",
            &format!("clock = [\"{}\"]", f.key()),
            1,
        );
        let p = parse(&text).unwrap();
        assert_eq!(p.images[0].clock, vec![f]);
        assert_eq!(p.toolchain_config().images[0].clock, vec![f]);
        assert_eq!(Field::parse(f.key()).unwrap(), f);
    }
    assert_eq!(msvc6_toolchain::clock::FIELDS, ALL);
    let sample = msvc6_fixture::sample::sample();
    let dir = std::env::temp_dir().join(format!("msvc6-project-clock-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    let reference = sample.pe.build();
    let mut ours = reference.clone();
    let ref_img = msvc6_image::pe::Image::parse(reference.clone()).unwrap();
    let fields: Vec<Field> = ALL
        .into_iter()
        .filter(|f| msvc6_image::clock::resolve(&ref_img, &[*f]).is_ok_and(|s| !s.is_empty()))
        .collect();
    assert!(!fields.is_empty());
    let sites = msvc6_image::clock::resolve(&ref_img, &fields).unwrap();
    for s in &sites {
        for b in &mut ours[s.off..s.off + 4] {
            *b ^= 0x5a;
        }
    }
    let (rp, op) = (dir.join("ref.dll"), dir.join("ours.dll"));
    msvc6_base::srcio::write_bytes(&rp, &reference).unwrap();
    msvc6_base::srcio::write_bytes(&op, &ours).unwrap();
    let lines = msvc6_toolchain::clock::copy_file(&op, &rp, &fields).unwrap();
    assert_eq!(lines.len(), sites.len());
    assert_eq!(std::fs::read(&op).unwrap(), reference);
    let after = msvc6_image::pe::Image::parse(std::fs::read(&op).unwrap()).unwrap();
    assert!(
        msvc6_image::clock::compare(&after, &ref_img, &fields)
            .unwrap()
            .iter()
            .all(msvc6_image::clock::Reading::identical)
    );
    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn a_config_is_found_given_here_or_by_the_environment() {
    let e = format!(
        "{:#}",
        Project::locate(Some(Path::new("/nonexistent/msvc6t.toml"))).unwrap_err()
    );
    assert!(e.contains("no such file"), "{e}");
}

#[test]
fn a_syntax_error_names_the_key_it_is_at() {
    let bad = GOOD.replacen(
        "linker = \"vc6\"\n",
        "linker = \"vc6\"\nlinker = \"vc6\"\n",
        1,
    );
    let e = err(&bad);
    names_file_line_key(&e, line_of(&bad, "linker") + 1, "image[0].linker");
    assert!(e.contains("duplicate key"), "{e}");
    let e = err("[source]\nroot = \".\"\n");
    names_file_line_key(&e, 1, "schema");
}

#[test]
fn an_optional_variable_marks_its_inputs_absent_while_it_is_unset() {
    let p = parse(&GOOD.replacen(
        "schema = 1\n",
        "schema = 1\noptional = [\"MSVC6T_SURELY_UNSET_OPTIONAL\"]\n",
        1,
    ))
    .unwrap();
    assert_eq!(p.optional, ["MSVC6T_SURELY_UNSET_OPTIONAL"]);
    assert_eq!(
        p.absent("${MSVC6T_SURELY_UNSET_OPTIONAL}/a.json")
            .as_deref(),
        Some("MSVC6T_SURELY_UNSET_OPTIONAL")
    );
    assert_eq!(p.absent("${MSVC6T_SURELY_UNSET_OPTIONAL:-x}/a.json"), None);
    assert_eq!(p.absent("$${MSVC6T_SURELY_UNSET_OPTIONAL}/a.json"), None);
    assert_eq!(p.absent("${MSVC6T_SURELY_UNSET_OTHER}/a.json"), None);
    assert_eq!(p.absent("analysis/a.json"), None);
    let f = FunctionSource::try_from("ghidra-json:${A}/x.json".to_string()).unwrap();
    assert_eq!((f.input(), f.kind()), (Some("${A}/x.json"), "ghidra-json"));
    assert_eq!(FunctionSource::Discover.input(), None);
    for bad in ["\"1X\"", "\"A-B\"", "\"A\", \"A\""] {
        let e = err(&GOOD.replacen(
            "schema = 1\n",
            &format!("schema = 1\noptional = [{bad}]\n"),
            1,
        ));
        assert!(
            e.contains("is not a variable name listed once"),
            "{bad}: {e}"
        );
    }
}

#[test]
fn a_future_schema_a_control_character_a_deep_key_and_a_bom_are_reported_by_line() {
    let future = GOOD.replacen("schema = 1", "schema = 2\nnewkey = true", 1);
    let e = err(&future);
    assert!(
        e.starts_with(&format!(
            "{FILE}:1:1: schema: schema 2 is not one this build reads"
        )),
        "{e}"
    );
    let tab = GOOD.replacen(
        "reference = \"ref/game.exe\"",
        "reference = \"C:\\temp\\new.exe\"",
        1,
    );
    let e = err(&tab);
    let line = line_of(&tab, "C:\\temp");
    assert!(
        e.starts_with(&format!("{FILE}:{line}:"))
            && e.contains("image[0].reference: a control character (U+0009)"),
        "{e}"
    );
    let deep = format!("schema = 1\n{} = 1\n", vec!["k"; 100].join("."));
    let e = err(&deep);
    assert!(
        e.contains(&format!(
            "{FILE}:2:1: a key or value nested 100 levels deep"
        )),
        "{e}"
    );
    let e = err("\u{feff}schema = 1 x\n");
    assert!(e.starts_with(&format!("{FILE}:1:10:")), "{e}");
}
