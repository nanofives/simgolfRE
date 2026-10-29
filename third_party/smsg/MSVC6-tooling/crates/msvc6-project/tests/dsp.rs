#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_project::config::Project;
use msvc6_project::spec::{CompileSpec, Language, Origin, PchMode, Specs, resolve, resolve_with};
use std::path::{Path, PathBuf};

fn fixtures() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../fixtures/project")
}

fn project(config: &str) -> Project {
    let text = format!(
        "schema = 1\n[[image]]\nid = \"APP\"\nreference = \"app/Release/app.exe\"\ncompiler = \"vc6\"\n\
         project = {{ dsw = \"ws.dsw\", config = \"{config}\" }}\n[toolchain.vc6]\nroot = \"/opt/vs6\"\n"
    );
    Project::parse(&text, &fixtures().join("msvc6t.toml")).unwrap()
}

fn specs(config: &str) -> Specs {
    resolve(&project(config), 0).unwrap()
}

fn tu<'a>(s: &'a Specs, project: &str, stem: &str) -> &'a CompileSpec {
    s.specs
        .iter()
        .find(|c| c.project == project && c.stem == stem)
        .unwrap_or_else(|| panic!("no TU {project}/{stem}"))
}

fn words(s: &str) -> Vec<String> {
    s.split_whitespace().map(str::to_string).collect()
}

#[test]
fn every_tu_of_the_release_workspace_resolves_its_switches() {
    let s = specs("app - Win32 Release");
    assert!(s.refused.is_empty(), "{:?}", s.refused);
    assert!(s.notes.is_empty(), "{:?}", s.notes);
    let order: Vec<(String, String)> = s
        .specs
        .iter()
        .map(|c| (c.project.clone(), c.stem.clone()))
        .collect();
    let want: Vec<(String, String)> = [
        ("zip", "inflate"),
        ("zip", "deflate"),
        ("util", "strings"),
        ("util", "table"),
        ("app", "main"),
        ("app", "render"),
        ("app", "legacy"),
        ("app", "profile"),
    ]
    .iter()
    .map(|(p, t)| ((*p).to_string(), (*t).to_string()))
    .collect();
    assert_eq!(order, want);
    let switches = [
        (
            "zip",
            "inflate",
            "/nologo /W3 /MT /O2 /Izip /DWIN32 /DNDEBUG /D_LIB /FD /c",
        ),
        (
            "zip",
            "deflate",
            "/nologo /W3 /MT /Izip /DWIN32 /DNDEBUG /D_LIB /FD /c /Ox",
        ),
        (
            "util",
            "strings",
            "/nologo /W3 /MT /O2 /Iinclude /DWIN32 /DNDEBUG /D_LIB /GR- /Zl /FD /c /Ycutil.h",
        ),
        (
            "util",
            "table",
            "/nologo /W3 /MT /O2 /Iinclude /DWIN32 /DNDEBUG /D_LIB /GR- /Zl /FD /c /Yuutil.h /FIutil.h",
        ),
        (
            "app",
            "main",
            "/nologo /W3 /MT /O2 /Iinclude /I..\\util\\include /DWIN32 /DNDEBUG /D_WINDOWS /GR- /YX /FD /c",
        ),
        (
            "app",
            "render",
            "/nologo /W3 /MT /O2 /Iinclude /I..\\util\\include /DWIN32 /DNDEBUG /D_WINDOWS /GR- /FD /c /GX /Ob2",
        ),
        (
            "app",
            "legacy",
            "/nologo /W3 /MT /O2 /Iinclude /I..\\util\\include /DWIN32 /DNDEBUG /D_WINDOWS /GR- /YX /FD /c /DLEGACY_API=1 /U_WINDOWS",
        ),
        (
            "app",
            "profile",
            "/nologo /W3 /MT /O2 /Iinclude /I..\\util\\include /DWIN32 /DNDEBUG /D_WINDOWS /GR- /YX /FD /c",
        ),
    ];
    for (p, t, sw) in switches {
        assert_eq!(tu(&s, p, t).switches, words(sw), "{p}/{t}");
    }
    let render = tu(&s, "app", "render");
    assert_eq!(
        render.cflags,
        words("/MT /O2 /DWIN32 /DNDEBUG /D_WINDOWS /GR- /GX /Ob2")
    );
    assert_eq!(render.includes, ["include", "..\\util\\include"]);
    let root = fixtures();
    let resolved: Vec<PathBuf> = render.include_paths.iter().map(PathBuf::from).collect();
    assert_eq!(resolved[0], root.join("app/include"));
    assert!(resolved[1].ends_with("util/include"), "{resolved:?}");
    assert!(resolved.iter().all(|p| p.is_dir()), "{resolved:?}");
    assert_eq!(render.pch.mode, PchMode::None);
    assert_eq!(render.origin, Origin::Dsp);
    assert_eq!(render.config.as_deref(), Some("app - Win32 Release"));
    assert_eq!(render.toolchain.as_deref(), Some("vc6"));
    assert!(
        render
            .object
            .as_deref()
            .unwrap()
            .ends_with("app/Release/render.obj")
    );
    assert!(Path::new(&render.path).is_file());
    let legacy = tu(&s, "app", "legacy");
    assert_eq!(legacy.language, Language::C);
    assert_eq!(
        legacy.defines,
        ["WIN32", "NDEBUG", "_WINDOWS", "LEGACY_API=1"]
    );
    assert_eq!(legacy.undefines, ["_WINDOWS"]);
    assert_eq!(tu(&s, "app", "main").pch.mode, PchMode::Auto);
    let strings = tu(&s, "util", "strings");
    assert_eq!(strings.pch.mode, PchMode::Create);
    assert_eq!(strings.pch.header.as_deref(), Some("util.h"));
    let table = tu(&s, "util", "table");
    assert_eq!(table.pch.mode, PchMode::Use);
    assert_eq!(table.forced_includes, ["util.h"]);
    assert!(
        tu(&s, "zip", "deflate")
            .object
            .as_deref()
            .unwrap()
            .ends_with("vendor/Release/deflate/deflate.obj")
    );
    assert!(s.specs.iter().all(|c| Path::new(&c.path).is_file()));
}

#[test]
fn the_debug_configuration_reads_its_own_arms_and_a_one_configuration_dependency() {
    let s = specs("app - Win32 Debug");
    assert!(s.refused.is_empty(), "{:?}", s.refused);
    assert_eq!(s.notes.len(), 1, "{:?}", s.notes);
    assert!(s.notes[0].contains("zip - Win32 Release"), "{:?}", s.notes);
    let stems: Vec<&str> = s.specs.iter().map(|c| c.stem.as_str()).collect();
    assert_eq!(
        stems,
        [
            "inflate", "deflate", "strings", "table", "main", "render", "legacy"
        ]
    );
    assert_eq!(
        tu(&s, "app", "main").switches,
        words(
            "/nologo /MTd /W3 /Gm /ZI /Od /Iinclude /I..\\util\\include /DWIN32 /D_DEBUG /D_WINDOWS /FD /GZ /c"
        )
    );
    assert_eq!(
        tu(&s, "app", "main").cflags,
        words("/MTd /ZI /Od /DWIN32 /D_DEBUG /D_WINDOWS /GZ")
    );
    assert_eq!(
        tu(&s, "app", "render").switches,
        words(
            "/nologo /MTd /W3 /Gm /ZI /Od /Iinclude /I..\\util\\include /DWIN32 /D_DEBUG /D_WINDOWS /FD /GZ /c /GX"
        )
    );
    assert_eq!(
        tu(&s, "util", "strings").switches,
        words(
            "/nologo /MTd /W3 /ZI /Od /Iinclude /DWIN32 /D_DEBUG /D_LIB /Zl /FD /GZ /c /Ycutil.h"
        )
    );
    assert!(
        tu(&s, "app", "main")
            .object
            .as_deref()
            .unwrap()
            .ends_with("app/Debug/main.obj")
    );
    assert_eq!(tu(&s, "app", "main").pch.mode, PchMode::None);
}

#[test]
fn a_configuration_the_workspace_cannot_build_is_refused() {
    let s = specs("app - Win32 Profile");
    assert_eq!(
        s.specs.len(),
        2,
        "zip still builds in its one configuration"
    );
    assert_eq!(s.refused.len(), 2, "{:?}", s.refused);
    assert!(
        s.refused
            .iter()
            .any(|r| r.project == "util" && r.why.contains("util - Win32 Profile")),
        "{:?}",
        s.refused
    );
    assert!(
        s.refused
            .iter()
            .any(|r| r.project == "app" && r.why.contains("no configuration")),
        "{:?}",
        s.refused
    );
    let p = project("nope - Win32 Release");
    let e = format!("{:#}", resolve(&p, 0).unwrap_err());
    assert!(
        e.contains("msvc6t.toml:") && e.contains("image[0].project.config") && e.contains("nope"),
        "{e}"
    );
}

fn one_config_dsp(name: &str, kind: &str, code: &str, source: &str) -> String {
    format!(
        "# Microsoft Developer Studio Project File - Name=\"{name}\" - Package Owner=<4>\r\n\
         # Microsoft Developer Studio Generated Build File, Format Version 6.00\r\n\r\n\
         # TARGTYPE \"Win32 (x86) {kind}\" {code}\r\n\r\n\
         CFG={name} - Win32 Release\r\n\
         !MESSAGE \"{name} - Win32 Release\" (based on \"Win32 (x86) {kind}\")\r\n\r\n\
         # Begin Project\r\nCPP=cl.exe\r\n\r\n\
         !IF  \"$(CFG)\" == \"{name} - Win32 Release\"\r\n\r\n\
         # PROP Intermediate_Dir \"Release\"\r\n\
         # ADD CPP /nologo /W3 /MT /O2 /FD /c\r\n\r\n\
         !ENDIF \r\n\r\n\
         # Begin Target\r\n\r\n# Name \"{name} - Win32 Release\"\r\n\
         # Begin Source File\r\n\r\nSOURCE={source}\r\n# End Source File\r\n\
         # End Target\r\n# End Project\r\n"
    )
}

#[test]
fn a_workspace_library_outside_the_build_order_resolves_only_when_named() {
    let root = std::env::temp_dir().join(format!("msvc6-project-named-{}", std::process::id()));
    std::fs::remove_dir_all(&root).ok();
    std::fs::create_dir_all(root.join("app")).unwrap();
    std::fs::create_dir_all(root.join("codec.lib")).unwrap();
    let member = |name: &str, dsp: &str| {
        format!(
            "Project: \"{name}\"={dsp} - Package Owner=<4>\r\n\r\nPackage=<5>\r\n{{{{{{\r\n}}}}}}\r\n\r\n\
             Package=<4>\r\n{{{{{{\r\n}}}}}}\r\n\r\n"
        )
    };
    msvc6_base::srcio::write(
        root.join("ws.dsw"),
        &format!(
            "Microsoft Developer Studio Workspace File, Format Version 6.00\r\n\r\n{}{}Global:\r\n\r\n\
             Package=<5>\r\n{{{{{{\r\n}}}}}}\r\n",
            member("app", ".\\app\\app.dsp"),
            member("codec", ".\\codec.lib\\codec.dsp")
        ),
    )
    .unwrap();
    msvc6_base::srcio::write(
        root.join("app").join("app.dsp"),
        &one_config_dsp("app", "Application", "0x0101", ".\\main.c"),
    )
    .unwrap();
    msvc6_base::srcio::write(
        root.join("codec.lib").join("codec.dsp"),
        &one_config_dsp("codec", "Static Library", "0x0104", ".\\src\\pack.c"),
    )
    .unwrap();
    let p = Project::parse(
        "schema = 1\n[[image]]\nid = \"APP\"\nreference = \"app/Release/app.exe\"\ncompiler = \"vc6\"\n\
         project = { dsw = \"ws.dsw\", config = \"app - Win32 Release\" }\n[toolchain.vc6]\nroot = \"/opt/vs6\"\n",
        &root.join("msvc6t.toml"),
    )
    .unwrap();
    let tus = |s: &Specs| -> Vec<(String, String)> {
        s.specs
            .iter()
            .map(|c| (c.project.clone(), c.stem.clone()))
            .collect()
    };
    let own = resolve(&p, 0).unwrap();
    assert!(own.refused.is_empty(), "{:?}", own.refused);
    assert_eq!(tus(&own), [("app".to_string(), "main".to_string())]);
    let named = resolve_with(&p, 0, &["codec".to_string()]).unwrap();
    assert!(named.refused.is_empty(), "{:?}", named.refused);
    assert_eq!(
        tus(&named),
        [
            ("app".to_string(), "main".to_string()),
            ("codec".to_string(), "pack".to_string())
        ]
    );
    assert!(
        Path::new(&tu(&named, "codec", "pack").path).ends_with("codec.lib/src/pack.c"),
        "{}",
        tu(&named, "codec", "pack").path
    );
    let again = resolve_with(
        &p,
        0,
        &["CODEC", "app", "nosuch", "codec"].map(str::to_string),
    )
    .unwrap();
    assert_eq!(tus(&again), tus(&named));
    std::fs::remove_dir_all(&root).ok();
}
