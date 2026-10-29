#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_base::srcio;
use msvc6_fixture::coff;
use std::process::Command;

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

#[test]
fn a_placement_verb_names_itself_in_its_json_and_its_census_names_what_it_leaves_out() {
    let d = std::env::temp_dir().join(format!("msvc6t-placement-{}", std::process::id()));
    if d.exists() {
        std::fs::remove_dir_all(&d).unwrap();
    }
    std::fs::create_dir_all(&d).unwrap();
    let mut o = coff::Object::new();
    o.section(coff::Section {
        name: ".text".to_string(),
        characteristics: coff::SCN_TEXT | coff::SCN_COMDAT,
        data: vec![0xc3; 16],
        ..coff::Section::default()
    });
    o.section(coff::Section {
        name: ".text$x".to_string(),
        characteristics: coff::SCN_TEXT | coff::SCN_COMDAT,
        data: vec![0xc3; 8],
        ..coff::Section::default()
    });
    srcio::write_bytes(d.join("a.obj"), &o.build()).unwrap();
    srcio::own_write(d.join("link.rsp"), "/nologo /dll\na.obj\n").unwrap();
    let json = d.join("out.json");
    let out = Command::new(BIN)
        .current_dir(&d)
        .env_remove("MSVC6T_CONFIG")
        .args(["placement", "comdats", "IMG", "--rsp"])
        .arg(d.join("link.rsp"))
        .arg("--json")
        .arg(&json)
        .output()
        .unwrap();
    let text = srcio::tool_text(&out.stdout) + &srcio::tool_text(&out.stderr);
    assert!(out.status.success(), "{text}");
    assert!(
        text.contains("NOT COUNTED: 1 code or data section(s) of another name"),
        "{text}"
    );
    let v: serde_json::Value = serde_json::from_str(&srcio::own_read(&json).unwrap()).unwrap();
    assert_eq!(v["command"], "placement comdats", "{v}");
    assert_eq!(v["summary"]["comdats"], 1, "{v}");
    assert_eq!(v["summary"]["uncounted"], 1, "{v}");
    std::fs::remove_dir_all(&d).unwrap();
}
