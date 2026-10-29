use crate::dbghelp::{Row, base, line_close, same_name};
use anyhow::{Context as _, Result, anyhow, bail};
use serde_json::Value;
use std::path::Path;

pub const TOOL: &str = "llvm-symbolizer";

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Answer {
    pub func: String,
    pub file: String,
    pub line: u32,
}

impl Answer {
    #[must_use]
    pub fn named(&self) -> bool {
        !self.func.is_empty() && self.func != "??"
    }
}

pub fn answers(json: &str) -> Result<Vec<Answer>> {
    let v: Value = serde_json::from_str(json).context("llvm-symbolizer's JSON")?;
    let arr = v
        .as_array()
        .ok_or_else(|| anyhow!("llvm-symbolizer did not print a JSON array"))?;
    let mut out = Vec::with_capacity(arr.len());
    for a in arr {
        let s = a
            .get("Symbol")
            .and_then(Value::as_array)
            .and_then(|s| s.first());
        let g = |k: &str| {
            s.and_then(|s| s.get(k))
                .and_then(Value::as_str)
                .unwrap_or_default()
                .to_string()
        };
        out.push(Answer {
            func: g("FunctionName"),
            file: g("FileName"),
            line: s
                .and_then(|s| s.get("Line"))
                .and_then(Value::as_u64)
                .and_then(|n| u32::try_from(n).ok())
                .unwrap_or(0),
        });
    }
    Ok(out)
}

pub fn run(tool: &Path, dir: &Path, image: &str, addrs: &[u32]) -> Result<Vec<Answer>> {
    let out = std::process::Command::new(tool)
        .current_dir(dir)
        .arg(format!("--obj={image}"))
        .args(["--functions=short", "--no-demangle", "--output-style=JSON"])
        .args(addrs.iter().map(|a| format!("{a:#x}")))
        .output()
        .with_context(|| format!("running {}", tool.display()))?;
    if !out.status.success() {
        bail!(
            "llvm-symbolizer failed: {}",
            msvc6_base::srcio::tool_text(&out.stderr)
        );
    }
    let ans = answers(&msvc6_base::srcio::tool_text(&out.stdout))?;
    if ans.len() != addrs.len() {
        bail!(
            "llvm-symbolizer answered {} of {} queries",
            ans.len(),
            addrs.len()
        );
    }
    Ok(ans)
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Tally {
    pub rows: usize,
    pub named: usize,
    pub same: usize,
    pub with_line: usize,
    pub file_ok: usize,
    pub line_close: usize,
    pub compared: usize,
    pub inside: usize,
    pub inside_rows: usize,
}

pub fn score(rows: &[&Row], ans: &[Answer], interior: bool, notes: &mut Vec<String>) -> Tally {
    let mut t = Tally::default();
    let mut it = ans.iter();
    for r in rows {
        let Some(a) = it.next() else { break };
        t.rows += 1;
        if a.named() {
            t.named += 1;
        }
        if a.named() && same_name(&r.name, &r.symbol, &a.func) {
            t.same += 1;
        } else {
            notes.push(format!(
                "  NAME  {:#010x}: expected {} and llvm-symbolizer says {}",
                r.address, r.name, a.func
            ));
        }
        if a.line > 0 && !a.file.is_empty() {
            t.with_line += 1;
            if r.tu.as_deref().is_some_and(|tu| base(tu) == base(&a.file)) {
                t.file_ok += 1;
            }
            if let Some(rl) = r.line {
                t.compared += 1;
                if line_close(rl, a.line) {
                    t.line_close += 1;
                }
            }
        }
        if interior && r.size > 1 {
            let Some(m) = it.next() else { break };
            t.inside_rows += 1;
            if same_name(&r.name, &r.symbol, &m.func) {
                t.inside += 1;
            }
        }
    }
    t
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_json_answer_is_read_per_address() {
        let a = answers(
            r#"[{"Address":"0x1000","ModuleName":"a.dll","Symbol":[{"FunctionName":"f","FileName":"C:\\w\\a.cpp","Line":12}]},
                {"Address":"0x1010","ModuleName":"a.dll","Symbol":[{"FunctionName":"??","FileName":"","Line":0}]}]"#,
        )
        .unwrap();
        assert_eq!(a.len(), 2);
        assert_eq!((a[0].func.as_str(), a[0].line), ("f", 12));
        assert!(a[0].named() && !a[1].named());
        assert!(answers("{}").is_err());
        assert!(answers("[").is_err());
    }

    #[test]
    fn a_name_and_a_line_are_scored_against_the_rows() {
        let rows = crate::dbghelp::rows_of_expect(
            "va\tkind\tnames\n00401000\tfunction\tf\n00401100\tfunction\tg\n",
            "e",
        )
        .unwrap();
        let picked: Vec<&Row> = rows.iter().collect();
        let ans = vec![
            Answer {
                func: "f".into(),
                file: "a.cpp".into(),
                line: 3,
            },
            Answer {
                func: "h".into(),
                ..Answer::default()
            },
        ];
        let mut notes = Vec::new();
        let t = score(&picked, &ans, false, &mut notes);
        assert_eq!((t.rows, t.named, t.same, t.with_line), (2, 2, 1, 1));
        assert_eq!(notes.len(), 1);
        assert!(notes[0].contains("says h"));
    }
}
