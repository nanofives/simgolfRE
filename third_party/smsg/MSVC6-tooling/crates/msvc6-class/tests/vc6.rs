use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;

use msvc6_class::join::{self, Inputs, Slot};
use msvc6_class::slotmap;
use msvc6_class::slots::Rule;
use msvc6_class::vft::{self, Run, Sections, Text};
use msvc6_image::pe::Image;
use msvc6_project::config::Project;
use msvc6_project::mapfile::Map;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::runner::{Runner, Session};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const SKIP: &str = "SKIP vc6: no toolchain configured (set MSVC6T_VC6)";

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn quoted(s: &str) -> String {
    format!("'{s}'")
}

fn config_text(scratch: &Path, root: &str, msvcrt: &str) -> String {
    let mut text = format!(
        "schema = 1\n\n[toolchain.vc6]\nroot = {}\n\n[runner]\n",
        quoted(root)
    );
    let command: Vec<String> = env("MSVC6T_WINE")
        .unwrap_or_else(|| "wine".into())
        .split_whitespace()
        .map(quoted)
        .collect();
    text.push_str(&format!("command = [{}]\n", command.join(", ")));
    text.push_str(&format!(
        "prefixes = {}\ntokens = {}\nnative_msvcrt = {}\n",
        quoted(&scratch.join("prefixes").display().to_string()),
        quoted(&scratch.join("tokens").display().to_string()),
        quoted(msvcrt)
    ));
    if let Some(t) = env("MSVC6T_WINE_TEMPLATE") {
        text.push_str(&format!("template = {}\n", quoted(&t)));
    }
    text.push_str(&format!(
        "\n[cache]\ndir = {}\n",
        quoted(&scratch.join("cache").display().to_string())
    ));
    text
}

fn tool(sess: &Session<'_>, tc: &Toolchain, exe: &str, args: &[&str], dir: &Path) -> Result<()> {
    let a: Vec<String> = args.iter().map(|s| (*s).to_string()).collect();
    let out = sess.run_tool(tc, exe, &a, Some(dir), &[])?;
    if !out.ok() {
        bail!(
            "{exe} {} failed:\n{}{}",
            a.join(" "),
            srcio::tool_text(&out.stdout),
            srcio::tool_text(&out.stderr)
        );
    }
    Ok(())
}

const EXPECTED: [(&str, &[&str]); 6] = [
    (
        "Base",
        &[
            "??_EBase@@UAEPAXI@Z",
            "?load@Base@@UAEHXZ",
            "?load@Base@@UAEHPADK@Z",
            "?load@Base@@UAEHPBD@Z",
            "?play@Base@@UAEHXZ",
            "?play@Base@@UAEHH@Z",
            "?play@Base@@UAEHPAH@Z",
            "?stop@Base@@UAEHXZ",
        ],
    ),
    (
        "Derived",
        &[
            "??_EDerived@@UAEPAXI@Z",
            "?load@Base@@UAEHXZ",
            "?load@Base@@UAEHPADK@Z",
            "?load@Base@@UAEHPBD@Z",
            "?play@Base@@UAEHXZ",
            "?play@Derived@@UAEHH@Z",
            "?play@Base@@UAEHPAH@Z",
            "?stop@Base@@UAEHXZ",
            "?pause@Derived@@UAEHH@Z",
            "?load@Derived@@UAEHH@Z",
        ],
    ),
    ("Mixin", &["?mix@Mixin@@UAEHH@Z", "?unmix@Mixin@@UAEHXZ"]),
    ("Holder", &["?stop@Holder@@UAEHH@Z"]),
    (
        "Voice",
        &["?stop@Voice@@UAGHH@Z", "?position@Voice@@UAGHPAH@Z"],
    ),
    (
        "Combined",
        &[
            "??_ECombined@@UAEPAXI@Z",
            "?load@Base@@UAEHXZ",
            "?load@Base@@UAEHPADK@Z",
            "?load@Base@@UAEHPBD@Z",
            "?play@Base@@UAEHXZ",
            "?play@Derived@@UAEHH@Z",
            "?play@Base@@UAEHPAH@Z",
            "?stop@Base@@UAEHXZ",
            "?pause@Derived@@UAEHH@Z",
            "?load@Derived@@UAEHH@Z",
            "?extra@Combined@@UAEHXZ",
        ],
    ),
];

fn check(dir: &Path, source: &str) -> Result<()> {
    let img = Image::parse(std::fs::read(dir.join("shapes.dll"))?)?;
    let map = Map::parse(
        &dir.join("shapes.map"),
        &srcio::read(dir.join("shapes.map"))?,
    )?;
    let names: Vec<(String, u32)> = map
        .symbols
        .iter()
        .map(|s| (s.name.clone(), s.rva))
        .collect();
    let mut at: BTreeMap<u32, Vec<&str>> = BTreeMap::new();
    for (n, r) in &names {
        at.entry(*r).or_default().push(n);
    }
    let text = Text::of(&img, vft::CODE)?;
    let runs: Vec<Run> = vft::detect(&img, Sections::default())?
        .into_iter()
        .map(|r| Run {
            rva: r.rva,
            slots: r.slots.iter().map(|s| text.follow(*s)).collect(),
        })
        .collect();
    for (class, want) in EXPECTED {
        let table = names
            .iter()
            .find(|(n, _)| {
                *n == format!("??_7{class}@@6B@") || *n == format!("??_7{class}@@6BDerived@@@")
            })
            .map(|(_, r)| *r)
            .with_context(|| format!("no vftable of {class} in the map"))?;
        let run = runs
            .iter()
            .find_map(|r| {
                let k = usize::try_from(table.checked_sub(r.rva)? / 4).ok()?;
                r.slots.get(k..)
            })
            .with_context(|| format!("{class}'s table {table:#x} lies in no run"))?;
        let scalar = |n: &str| n.replacen("??_E", "??_G", 1);
        let want: Vec<String> = want.iter().map(|n| scalar(n)).collect();
        let got: Vec<String> = run
            .iter()
            .take(want.len())
            .map(|b| {
                at.get(b)
                    .and_then(|v| {
                        v.iter()
                            .find(|n| want.contains(&scalar(n)))
                            .or_else(|| v.first())
                    })
                    .map_or_else(|| format!("{b:#x}"), |n| scalar(n))
            })
            .collect();
        if got != want {
            bail!("{class}'s table as LINK wrote it is\n  {got:?}\nnot\n  {want:?}");
        }
    }
    let files = vec![("src/shapes.cpp".to_string(), source.to_string())];
    let run_join = |rule| {
        join::join(&Inputs {
            runs: &runs,
            names: &names,
            map: &[],
            placed: &[],
            files: &files,
            headers: &[],
            rule,
            text: Some(text),
        })
    };
    let j = run_join(Rule::Vc6);
    for (class, want) in EXPECTED {
        let a = j
            .assigned
            .iter()
            .find(|a| a.class == class)
            .with_context(|| format!("{class} was not assigned a table: {j:?}"))?;
        if a.slots.len() != want.len() || a.slots.iter().any(|s| *s != Slot::Agrees) {
            bail!("{class}: {:?} {:?}", a.names, a.slots);
        }
    }
    if !j.arities.is_empty() {
        bail!(
            "a slot's body pops another count than its declaration: {:?}",
            j.arities
        );
    }
    println!(
        "vc6: the MSVC6 slot rule joins {} classes, every slot agreeing ({} placed by `??_7')",
        j.assigned.len(),
        j.table_pinned
    );
    let l = run_join(Rule::Legacy);
    let wrong: usize = l
        .assigned
        .iter()
        .flat_map(|a| a.slots.iter())
        .filter(|s| matches!(s, Slot::Disagrees(_)))
        .count();
    if wrong == 0 {
        bail!(
            "the declaration-order reading agreed everywhere, so the fixture shows no overload group"
        );
    }
    println!("vc6: the declaration-order reading disagrees at {wrong} slot(s)");
    let table_of = |class: &str, n: usize| -> Result<Vec<u32>> {
        let at = names
            .iter()
            .find(|(s, _)| *s == format!("??_7{class}@@6B@"))
            .map(|(_, r)| *r)
            .with_context(|| format!("no vftable of {class} in the map"))?;
        vft::detect(&img, Sections::default())?
            .into_iter()
            .find_map(|r| {
                let k = usize::try_from(at.checked_sub(r.rva)? / 4).ok()?;
                r.slots.get(k..k + n).map(<[u32]>::to_vec)
            })
            .with_context(|| format!("{class}'s table lies in no run"))
    };
    let holder = slotmap::map(text, &table_of("Holder", 1)?, None);
    if !holder.rows.iter().all(|r| r.slot.pushes_object) {
        bail!(
            "Holder's forward to a __stdcall virtual did not read as pushing its object: {:?}",
            holder.rows
        );
    }
    let base = slotmap::map(text, &table_of("Base", 8)?, None);
    if base.rows.iter().any(|r| r.slot.pushes_object) {
        bail!(
            "a __thiscall slot of Base read as a __stdcall call: {:?}",
            base.rows
        );
    }
    println!(
        "vc6: every slot's body pops what its declaration takes, Voice's __stdcall `this' counted; slot-map reads Holder's call as __stdcall"
    );
    Ok(())
}

fn require() -> bool {
    env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1")
}

fn run() -> Result<bool> {
    let (Some(root), Some(msvcrt)) = (env("MSVC6T_VC6"), env("MSVC6T_MSVCRT")) else {
        println!("{SKIP}; the link also needs MSVC6T_MSVCRT, the native msvcrt");
        if require() {
            bail!("MSVC6T_REQUIRE_TOOLCHAIN=1 and no toolchain or native msvcrt is configured");
        }
        return Ok(false);
    };
    let scratch = std::env::temp_dir().join(format!("msvc6t-class-test-{}", std::process::id()));
    std::fs::create_dir_all(&scratch)?;
    let file = scratch.join("msvc6t.toml");
    srcio::own_write(&file, config_text(&scratch, &root, &msvcrt))?;
    let p = Project::load(&file)?;
    let cfg = p.toolchain_config();
    let runner = Runner::from_config(&cfg)?;
    let source = srcio::read(repo().join("fixtures/classes/shapes.cpp"))?;
    let work = scratch.join("work");
    std::fs::create_dir_all(&work)?;
    srcio::write(work.join("shapes.cpp"), &source)?;
    let result = (|| -> Result<()> {
        let sess = runner.session()?;
        let tc = Toolchain::named(&cfg, "vc6")?;
        tool(
            &sess,
            &tc,
            "CL.EXE",
            &["/nologo", "/c", "/O2", "/GR-", "shapes.cpp"],
            &work,
        )?;
        tool(
            &sess,
            &tc,
            "LINK.EXE",
            &[
                "/nologo",
                "/DLL",
                "/NOENTRY",
                "/NODEFAULTLIB",
                "/MAP:shapes.map",
                "/OUT:shapes.dll",
                "shapes.obj",
            ],
            &work,
        )?;
        check(&work, &source)
    })();
    let pool = scratch.join("prefixes");
    for (_, prefix) in msvc6_toolchain::reap::slot_locks(&pool) {
        msvc6_toolchain::reap::retire(&pool, &prefix)?;
    }
    if result.is_ok() {
        std::fs::remove_dir_all(&scratch)?;
    } else {
        println!("vc6: the scratch directory is kept: {}", scratch.display());
    }
    result.map(|()| true)
}

fn main() -> ExitCode {
    match run() {
        Ok(true) => {
            println!("vc6: PASS");
            ExitCode::SUCCESS
        }
        Ok(false) => ExitCode::SUCCESS,
        Err(e) => {
            println!("vc6: FAIL: {e:#}");
            ExitCode::FAILURE
        }
    }
}
