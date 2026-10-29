use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

fn root() -> Option<PathBuf> {
    msvc6_lint::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR")))
}

fn env(k: &str) -> Option<String> {
    std::env::var(k).ok().filter(|v| !v.trim().is_empty())
}

fn variables(s: &str) -> Vec<String> {
    s.split("${")
        .skip(1)
        .filter_map(|rest| rest.split('}').next())
        .filter(|v| !v.contains(":-"))
        .map(str::to_string)
        .collect()
}

fn needed(doc: &toml::Table, only: Option<&[String]>) -> Vec<String> {
    let mut v = Vec::new();
    if let Some(t) = doc.get("toolchain").and_then(toml::Value::as_table) {
        for (id, spec) in t {
            if only.is_some_and(|o| !o.contains(id)) {
                continue;
            }
            if let Some(r) = spec.get("root").and_then(toml::Value::as_str) {
                v.extend(variables(r));
            }
        }
    }
    if let Some(m) = doc
        .get("runner")
        .and_then(|r| r.get("native_msvcrt"))
        .and_then(toml::Value::as_str)
    {
        v.extend(variables(m));
    }
    v.sort();
    v.dedup();
    v
}

fn configs(root: &Path) -> Vec<PathBuf> {
    let mut v: Vec<PathBuf> = std::fs::read_dir(root.join("examples"))
        .map(|d| {
            d.flatten()
                .map(|e| e.path().join("msvc6t.toml"))
                .filter(|p| p.is_file())
                .collect()
        })
        .unwrap_or_default();
    v.sort();
    v
}

fn run() -> Result<usize, String> {
    let root = root().ok_or("no workspace root")?;
    let require = env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1");
    let mut ran = 0usize;
    for cfg in configs(&root) {
        let example = cfg
            .parent()
            .and_then(Path::file_name)
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default();
        let text =
            msvc6_base::srcio::own_read(&cfg).map_err(|e| format!("{}: {e}", cfg.display()))?;
        let doc: toml::Table =
            toml::from_str(&text).map_err(|e| format!("{}: {e}", cfg.display()))?;
        let missing: Vec<String> = needed(&doc, None)
            .into_iter()
            .filter(|k| env(k).is_none())
            .collect();
        if !missing.is_empty() {
            println!("SKIP probes {example}: set {}", missing.join(", "));
            if require {
                return Err(format!(
                    "MSVC6T_REQUIRE_TOOLCHAIN=1 and {example} needs {}",
                    missing.join(", ")
                ));
            }
            continue;
        }
        let out = std::env::temp_dir().join(format!("msvc6t-probesuite-{example}"));
        let jobs = env("MSVC6T_JOBS").unwrap_or_else(|| "4".to_string());
        let o = Command::new(BIN)
            .arg("probe")
            .arg("--config")
            .arg(&cfg)
            .arg("--out")
            .arg(&out)
            .args(["-j", &jobs])
            .arg(root.join("probes"))
            .output()
            .map_err(|e| format!("running msvc6t probe: {e}"))?;
        let stdout = msvc6_base::srcio::tool_text(&o.stdout);
        for l in stdout
            .lines()
            .filter(|l| !l.starts_with("PASS ") && !l.starts_with(' '))
        {
            println!("probes {example}: {l}");
        }
        if !o.status.success() {
            for l in stdout.lines().filter(|l| l.starts_with(' ')) {
                println!("probes {example}: {l}");
            }
            return Err(format!(
                "msvc6t probe on {example} exited {:?}\n{}",
                o.status.code(),
                msvc6_base::srcio::tool_text(&o.stderr).trim()
            ));
        }
        ran += 1;
    }
    Ok(ran)
}

fn gate_rows(text: &str) -> (Vec<String>, Vec<String>) {
    let mut gates: Vec<String> = Vec::new();
    let mut rows: Vec<String> = Vec::new();
    for l in text.lines().filter(|l| !l.starts_with("probe\t")) {
        let f: Vec<&str> = l.split('\t').collect();
        if f.last() != Some(&"gate") {
            continue;
        }
        if let Some(t) = f.get(2) {
            gates.push((*t).to_string());
        }
        rows.push(l.to_string());
    }
    gates.sort();
    gates.dedup();
    rows.sort();
    (gates, rows)
}

fn read(p: &Path) -> Result<String, String> {
    msvc6_base::srcio::own_read(p).map_err(|e| format!("{}: {e}", p.display()))
}

fn il_suite(root: &Path) -> Result<Option<usize>, String> {
    let require = env("MSVC6T_REQUIRE_TOOLCHAIN").as_deref() == Some("1");
    let il = root.join("probes").join("il");
    let (gates, want) = gate_rows(&read(&il.join("results.tsv"))?);
    if gates.is_empty() {
        return Err("probes/il/results.tsv holds no gate row".to_string());
    }
    let cfg = root.join("probes").join("msvc6t.toml");
    let doc: toml::Table =
        toml::from_str(&read(&cfg)?).map_err(|e| format!("{}: {e}", cfg.display()))?;
    let missing: Vec<String> = needed(&doc, Some(&gates))
        .into_iter()
        .filter(|k| env(k).is_none())
        .collect();
    if !missing.is_empty() {
        println!(
            "SKIP IL suite on {}: set {}",
            gates.join(", "),
            missing.join(", ")
        );
        if require {
            return Err(format!(
                "MSVC6T_REQUIRE_TOOLCHAIN=1 and the IL suite needs {}",
                missing.join(", ")
            ));
        }
        return Ok(None);
    }
    let out = std::env::temp_dir().join("msvc6t-probesuite-il");
    let jobs = env("MSVC6T_JOBS").unwrap_or_else(|| "4".to_string());
    let mut cmd = Command::new(BIN);
    cmd.arg("probe")
        .arg("--config")
        .arg(&cfg)
        .arg("--out")
        .arg(&out)
        .args(["-j", &jobs]);
    for g in &gates {
        cmd.args(["--toolchain", g, "--gate", g]);
    }
    let o = cmd
        .arg(&il)
        .output()
        .map_err(|e| format!("running msvc6t probe: {e}"))?;
    let stdout = msvc6_base::srcio::tool_text(&o.stdout);
    for l in stdout
        .lines()
        .filter(|l| !l.starts_with("PASS ") && !l.starts_with(' '))
    {
        println!("IL suite: {l}");
    }
    if !o.status.success() {
        return Err(format!(
            "msvc6t probe on probes/il exited {:?}\n{}",
            o.status.code(),
            msvc6_base::srcio::tool_text(&o.stderr).trim()
        ));
    }
    let (_, got) = gate_rows(&read(&out.join("results.tsv"))?);
    if got != want {
        let first = want
            .iter()
            .zip(&got)
            .find(|(a, b)| a != b)
            .map_or_else(String::new, |(a, b)| {
                format!(": recorded `{a}`, recompiled `{b}`")
            });
        return Err(format!(
            "the IL suite recompiled {} gate row(s) where probes/il/results.tsv records {}{first}",
            got.len(),
            want.len()
        ));
    }
    println!(
        "IL suite: {} gate row(s) on {} recompiled, each equal to probes/il/results.tsv",
        got.len(),
        gates.join(", ")
    );
    Ok(Some(got.len()))
}

fn main() -> ExitCode {
    let il = match root().ok_or_else(|| "no workspace root".to_string()) {
        Ok(r) => il_suite(&r),
        Err(e) => Err(e),
    };
    if let Err(e) = &il {
        println!("probesuite: FAIL: {e}");
        return ExitCode::FAILURE;
    }
    match run() {
        Ok(n) => {
            println!("probesuite: PASS ({n} example(s) run)");
            ExitCode::SUCCESS
        }
        Err(e) => {
            println!("probesuite: FAIL: {e}");
            ExitCode::FAILURE
        }
    }
}
