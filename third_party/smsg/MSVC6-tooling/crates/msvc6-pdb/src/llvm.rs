use crate::eq::Counts;
use anyhow::{Context as _, Result};
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub const ENV: &str = "MSVC6T_LLVM";
pub const TOOL: &str = "llvm-pdbutil";

#[must_use]
pub fn find(dir: Option<&Path>) -> Option<PathBuf> {
    find_named(dir, TOOL)
}

#[must_use]
pub fn find_named(dir: Option<&Path>, tool: &str) -> Option<PathBuf> {
    let exe = |d: &Path| {
        let p = d.join(tool);
        p.is_file().then_some(p)
    };
    if let Some(d) = dir {
        return exe(d);
    }
    if let Some(d) = std::env::var_os(ENV).filter(|v| !v.is_empty()) {
        return exe(Path::new(&d));
    }
    std::env::var_os("PATH").and_then(|p| std::env::split_paths(&p).find_map(|d| exe(&d)))
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Dump {
    pub kinds: BTreeMap<(String, String), usize>,
    pub types: usize,
    pub lines: usize,
    pub errors: Vec<String>,
    pub unresolved: usize,
}

#[must_use]
pub fn read_dump(text: &str) -> Dump {
    let mut d = Dump::default();
    let lines: Vec<&str> = text.lines().collect();
    let mut section = String::new();
    for (i, l) in lines.iter().enumerate() {
        if lines
            .get(i + 1)
            .is_some_and(|n| n.len() > 10 && n.trim().chars().all(|c| c == '='))
        {
            section = l.trim().to_string();
            continue;
        }
        let unquoted: String = l.split('`').step_by(2).collect::<Vec<_>>().join("");
        let t = unquoted.trim();
        if t.starts_with("Error")
            || t.contains("Stream Error")
            || t.contains("(unknown file name offset")
            || t.contains("our hash = ")
            || t.contains("UNKNOWN RECORD")
            || t.contains("is corrupt")
        {
            d.errors.push(format!("[{section}] {}", l.trim()));
        }
        d.unresolved += unquoted.matches("(<unknown UDT>)").count();
        let t = l.trim_start();
        if let Some((head, rest)) = t.split_once(" | ") {
            let kind = rest.split([' ', '[']).next().unwrap_or_default();
            if section == "Types (TPI Stream)" && head.starts_with("0x") && kind.starts_with("LF_")
            {
                d.types += 1;
            } else if head.chars().all(|c| c.is_ascii_digit()) && kind.starts_with("S_") {
                *d.kinds
                    .entry((section.clone(), kind.to_string()))
                    .or_default() += 1;
            }
        }
        if section == "Lines"
            && let Some(n) = t.split("line/addr entries = ").nth(1)
        {
            d.lines += n.trim().parse::<usize>().unwrap_or(0);
        }
    }
    d
}

impl Dump {
    #[must_use]
    pub fn counts(&self) -> Counts {
        let k = |sec: &str, kinds: &[&str]| -> usize {
            kinds
                .iter()
                .map(|n| {
                    self.kinds
                        .get(&(sec.to_string(), (*n).to_string()))
                        .copied()
                        .unwrap_or(0)
                })
                .sum()
        };
        let data = ["S_GDATA32", "S_LDATA32", "S_GTHREAD32", "S_LTHREAD32"];
        Counts {
            procs: k("Symbols", &["S_GPROC32", "S_LPROC32"]),
            publics: k("Public Symbols", &["S_PUB32"]),
            data: k("Symbols", &data) + k("Global Symbols", &data),
            types: self.types,
            lines: self.lines,
            udts: k("Symbols", &["S_UDT"]) + k("Global Symbols", &["S_UDT"]),
            labels: k("Symbols", &["S_LABEL32"]),
            blocks: k("Symbols", &["S_BLOCK32"]),
            thunks: k("Symbols", &["S_THUNK32"]),
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct Reading {
    pub tool: String,
    pub exit: Option<i32>,
    pub stderr: Vec<String>,
    pub errors: Vec<String>,
    pub unresolved: usize,
    pub counts: Counts,
}

impl Reading {
    #[must_use]
    pub fn clean(&self) -> bool {
        self.exit == Some(0) && self.stderr.is_empty() && self.errors.is_empty()
    }
}

pub fn read(tool: &Path, pdb70: &Path) -> Result<Reading> {
    let out = std::process::Command::new(tool)
        .args(["dump", "-all"])
        .arg(pdb70)
        .output()
        .with_context(|| format!("running {}", tool.display()))?;
    let text = msvc6_base::srcio::tool_text(&out.stdout);
    let stderr = msvc6_base::srcio::tool_text(&out.stderr);
    let d = read_dump(&text);
    Ok(Reading {
        tool: tool.display().to_string(),
        exit: out.status.code(),
        stderr: stderr
            .lines()
            .filter(|l| !l.trim().is_empty())
            .map(str::to_string)
            .collect(),
        counts: d.counts(),
        errors: d.errors,
        unresolved: d.unresolved,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_dump_is_counted_by_section_and_a_name_is_not_an_error() {
        let d = read_dump(
            "                          Symbols                           \n\
             ============================================================\n\
             \x20 Mod 0000 | `a.obj`:\n\
             \x20      4 | S_GPROC32 [size = 48] `SetLastError`\n\
             \x20     52 | S_END [size = 4]\n\
             \x20                    Public Symbols                      \n\
             ============================================================\n\
             \x20     0 | S_PUB32 [size = 20] `_main`\n\
             \x20                   Types (TPI Stream)                   \n\
             ============================================================\n\
             \x20 0x1000 | LF_ARGLIST [size = 8, hash = 0x1]\n\
             \x20          - LF_MEMBER [name = `x`]\n\
             \x20                          Lines                          \n\
             ============================================================\n\
             \x20 0001:00001000-0000100F, line/addr entries = 3\n",
        );
        assert_eq!(d.counts().procs, 1);
        assert_eq!(d.counts().publics, 1);
        assert_eq!((d.types, d.lines), (1, 3));
        assert!(d.errors.is_empty(), "{:?}", d.errors);
        let bad = read_dump("Error loading module stream 3.  Module stream not present\n");
        assert_eq!(bad.errors.len(), 1);
        let names = read_dump(
            "  Stream 137 (  1652 bytes): [Module \".\\Release\\JERROR.OBJ\"]\n\
             \x20          type = 0x1817 (jpeg_error_mgr*), offset = 4\n\
             \x20          type = 0x2001222 (<unknown UDT>), addr = 0003:18632\n",
        );
        assert!(names.errors.is_empty(), "{:?}", names.errors);
        assert_eq!(names.unresolved, 1);
    }

    #[test]
    fn a_directory_without_the_tool_finds_nothing() {
        assert_eq!(find(Some(Path::new("/nonexistent-msvc6t-llvm"))), None);
    }
}
