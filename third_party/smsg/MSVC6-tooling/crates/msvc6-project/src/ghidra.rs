use crate::functions::Function;
use anyhow::{Context as _, Result, bail};
use serde_json::Value;
use std::path::Path;

pub const AUTO_PREFIXES: [&str; 6] = ["FUN_", "thunk_FUN", "SUB_", "sub_", "LAB_", "switchD"];

#[must_use]
pub fn is_auto_name(name: &str) -> bool {
    AUTO_PREFIXES.iter().any(|p| name.starts_with(p))
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Stats {
    pub records: usize,
    pub thunks: usize,
    pub contaminated: usize,
    pub auto_names: usize,
}

fn hex(v: &Value) -> Option<u32> {
    match v {
        Value::String(s) => {
            let t = s.trim();
            let t = t
                .strip_prefix("0x")
                .or_else(|| t.strip_prefix("0X"))
                .unwrap_or(t);
            u32::from_str_radix(t, 16).ok()
        }
        Value::Number(n) => n.as_u64().and_then(|n| u32::try_from(n).ok()),
        _ => None,
    }
}

fn byte_len(v: &Value) -> Option<u32> {
    let s = v.as_str()?;
    let mut n = 0u32;
    for b in s.split_whitespace() {
        u8::from_str_radix(b, 16).ok()?;
        n = n.checked_add(1)?;
    }
    Some(n)
}

pub fn parse(file: &Path, text: &str, image_base: u32) -> Result<(Vec<Function>, Stats)> {
    let v: Value =
        serde_json::from_str(text).with_context(|| format!("{}: not JSON", file.display()))?;
    let (records, base) = match &v {
        Value::Array(a) => (a, image_base),
        Value::Object(o) => {
            match o.get("format").and_then(Value::as_u64) {
                Some(1) => {}
                other => bail!(
                    "{}: functions.json format {other:?}; this build reads format 1 (or a bare array of records)",
                    file.display()
                ),
            }
            let base = match o.get("image_base") {
                Some(b) => hex(b).with_context(|| {
                    format!("{}: `image_base` is not an address", file.display())
                })?,
                None => image_base,
            };
            let a = o
                .get("functions")
                .and_then(Value::as_array)
                .with_context(|| format!("{}: no `functions` array", file.display()))?;
            (a, base)
        }
        _ => bail!(
            "{}: neither a record array nor a format-1 object",
            file.display()
        ),
    };
    let mut out = Vec::with_capacity(records.len());
    let mut stats = Stats::default();
    for (i, r) in records.iter().enumerate() {
        let at = || format!("{}: record {i}", file.display());
        let o = r
            .as_object()
            .with_context(|| format!("{}: not an object", at()))?;
        let entry_va = o
            .get("addr")
            .or_else(|| o.get("entry"))
            .and_then(hex)
            .with_context(|| format!("{}: no hexadecimal `addr`", at()))?;
        let rva = entry_va.checked_sub(base).with_context(|| {
            format!(
                "{}: address {entry_va:#x} is below the image base {base:#x}",
                at()
            )
        })?;
        let name = o.get("name").and_then(Value::as_str).map(str::to_string);
        let thunk = match o.get("thunk") {
            Some(Value::Bool(b)) => *b,
            Some(Value::Number(n)) => n.as_i64().is_some_and(|n| n != 0),
            _ => false,
        };
        let mut end: Option<u32> = None;
        let mut below = false;
        if let Some(ins) = o.get("ins").and_then(Value::as_array) {
            for (k, ins) in ins.iter().enumerate() {
                let a = ins
                    .as_array()
                    .with_context(|| format!("{}: instruction {k} is not an array", at()))?;
                let va = a
                    .first()
                    .and_then(hex)
                    .with_context(|| format!("{}: instruction {k} has no address", at()))?;
                let len = a.get(2).and_then(byte_len).with_context(|| {
                    format!("{}: instruction {k} has no hexadecimal bytes", at())
                })?;
                if va < entry_va {
                    below = true;
                    continue;
                }
                let stop = va
                    .checked_add(len)
                    .and_then(|e| e.checked_sub(base))
                    .with_context(|| format!("{}: instruction {k} overflows", at()))?;
                end = Some(end.map_or(stop, |e: u32| e.max(stop)));
            }
        }
        if let Some(sz) = o.get("size").and_then(Value::as_u64) {
            let sz = u32::try_from(sz).with_context(|| format!("{}: size out of range", at()))?;
            end = Some(
                rva.checked_add(sz)
                    .with_context(|| format!("{}: size overflows", at()))?,
            );
        }
        stats.records = stats.records.saturating_add(1);
        if thunk {
            stats.thunks = stats.thunks.saturating_add(1);
        }
        if below {
            stats.contaminated = stats.contaminated.saturating_add(1);
        }
        let name = match name {
            Some(n) if is_auto_name(&n) => {
                stats.auto_names = stats.auto_names.saturating_add(1);
                None
            }
            n => n,
        };
        out.push(Function {
            rva,
            end,
            name,
            thunk,
            provider: "ghidra-json".to_string(),
            object: None,
        });
    }
    Ok((out, stats))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_record_keys_at_its_own_entry_and_ignores_instructions_below_it() {
        let text = r#"[
            {"name": "Run", "addr": "00401010", "thunk": 0, "ins": [["00401010", "PUSH EBP", "55", 0], ["00401011", "RET", "c3", 0], ["00401000", "MOV", "8b ec", 0]]},
            {"name": "thunk_FUN_00401010", "addr": "00401020", "thunk": 1, "ins": [["00401020", "JMP", "e9 eb ff ff ff", 0]]}
        ]"#;
        let (f, s) = parse(Path::new("f.json"), text, 0x40_0000).unwrap();
        assert_eq!(f[0].rva, 0x1010);
        assert_eq!(f[0].end, Some(0x1012));
        assert_eq!(f[0].name.as_deref(), Some("Run"));
        assert!(f[1].thunk && f[1].name.is_none());
        assert_eq!(
            s,
            Stats {
                records: 2,
                thunks: 1,
                contaminated: 1,
                auto_names: 1
            }
        );
        let v1 = r#"{"format": 1, "image_base": "0x10000000", "functions": [{"name": "A", "addr": "0x10001000", "size": 16}]}"#;
        let (f, _) = parse(Path::new("f.json"), v1, 0).unwrap();
        assert_eq!((f[0].rva, f[0].end), (0x1000, Some(0x1010)));
        assert!(parse(Path::new("f.json"), r#"{"format": 2, "functions": []}"#, 0).is_err());
        assert!(parse(Path::new("f.json"), r#"[{"name": "A"}]"#, 0).is_err());
    }
}
