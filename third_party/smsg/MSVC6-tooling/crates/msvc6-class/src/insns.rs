use anyhow::{Context as _, Result, bail};
use iced_x86::{Decoder, DecoderOptions, FlowControl, OpKind};
use serde_json::Value;
use std::collections::{BTreeMap, BTreeSet};

pub const MAX_INSNS: usize = 1 << 24;
pub const MAX_BODY: u32 = 1 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Insn {
    pub rva: u32,
    pub bytes: Vec<u8>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Function {
    pub entry: u32,
    pub name: Option<String>,
    pub insns: Vec<Insn>,
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

fn hex_bytes(s: &str) -> Option<Vec<u8>> {
    s.split_whitespace()
        .map(|b| u8::from_str_radix(b, 16).ok())
        .collect()
}

pub fn from_ghidra(text: &str, image_base: u32) -> Result<Vec<Function>> {
    let v: Value = serde_json::from_str(text).context("a functions.json that is not JSON")?;
    let (records, base) = match &v {
        Value::Array(a) => (a, image_base),
        Value::Object(o) => {
            if o.get("format").and_then(Value::as_u64) != Some(1) {
                bail!("a functions.json object that is not format 1");
            }
            let base = o.get("image_base").and_then(hex).unwrap_or(image_base);
            let a = o
                .get("functions")
                .and_then(Value::as_array)
                .context("a format-1 functions.json with no `functions` array")?;
            (a, base)
        }
        _ => bail!("a functions.json that is neither a record array nor a format-1 object"),
    };
    let mut out = Vec::with_capacity(records.len());
    let mut total = 0usize;
    for (i, r) in records.iter().enumerate() {
        let entry = r
            .get("addr")
            .or_else(|| r.get("entry"))
            .and_then(hex)
            .with_context(|| format!("record {i}: no hexadecimal `addr`"))?;
        let Some(entry) = entry.checked_sub(base) else {
            continue;
        };
        let name = r.get("name").and_then(Value::as_str).map(str::to_string);
        let mut insns = Vec::new();
        for (k, ins) in r
            .get("ins")
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
            .enumerate()
        {
            let a = ins
                .as_array()
                .with_context(|| format!("record {i}: instruction {k} is not an array"))?;
            let va = a
                .first()
                .and_then(hex)
                .with_context(|| format!("record {i}: instruction {k} has no address"))?;
            let bytes = a
                .get(2)
                .and_then(Value::as_str)
                .and_then(hex_bytes)
                .with_context(|| format!("record {i}: instruction {k} has no hexadecimal bytes"))?;
            total = total.saturating_add(1);
            if total > MAX_INSNS {
                bail!("more than {MAX_INSNS} instructions, more than a function index here holds");
            }
            if let Some(rva) = va.checked_sub(base) {
                insns.push(Insn { rva, bytes });
            }
        }
        out.push(Function { entry, name, insns });
    }
    Ok(out)
}

fn targets(i: &iced_x86::Instruction) -> Option<u32> {
    matches!(i.op0_kind(), OpKind::NearBranch32)
        .then(|| u32::try_from(i.near_branch_target()).ok())
        .flatten()
}

pub fn decode(text: &[u8], text_rva: u32, extents: &[(u32, Option<u32>)]) -> Vec<Function> {
    decode_within(text, text_rva, extents).0
}

#[must_use]
pub fn decode_within(
    text: &[u8],
    text_rva: u32,
    extents: &[(u32, Option<u32>)],
) -> (Vec<Function>, bool) {
    let starts: BTreeSet<u32> = extents.iter().map(|(s, _)| *s).collect();
    let text_end = text_rva.saturating_add(u32::try_from(text.len()).unwrap_or(u32::MAX));
    let mut out = Vec::new();
    let mut budget = MAX_INSNS.min(text.len().saturating_mul(2).saturating_add(1));
    let mut starved = false;
    let mut seen_entry = BTreeSet::new();
    for &(start, end) in extents {
        if !seen_entry.insert(start) || start < text_rva || start >= text_end {
            continue;
        }
        let next = starts
            .range(start.saturating_add(1)..)
            .next()
            .copied()
            .unwrap_or(text_end);
        let stop = end
            .unwrap_or(next)
            .min(text_end)
            .min(start.saturating_add(MAX_BODY));
        let mut body: BTreeMap<u32, Insn> = BTreeMap::new();
        let mut work = vec![start];
        while let Some(at) = work.pop() {
            let mut ip = at;
            while ip >= start && ip < stop && !body.contains_key(&ip) {
                if budget == 0 {
                    starved = true;
                    break;
                }
                let Some(off) = ip
                    .checked_sub(text_rva)
                    .and_then(|o| usize::try_from(o).ok())
                else {
                    break;
                };
                let lim = usize::try_from(stop - text_rva).unwrap_or(text.len());
                let Some(window) = text.get(off..lim.min(text.len())) else {
                    break;
                };
                let mut d = Decoder::with_ip(32, window, u64::from(ip), DecoderOptions::NONE);
                let i = d.decode();
                if i.is_invalid() || i.len() == 0 {
                    break;
                }
                let len = u32::try_from(i.len()).unwrap_or(1);
                budget -= 1;
                body.insert(
                    ip,
                    Insn {
                        rva: ip,
                        bytes: window.get(..i.len()).unwrap_or_default().to_vec(),
                    },
                );
                match i.flow_control() {
                    FlowControl::Return | FlowControl::Interrupt | FlowControl::Exception => {
                        break;
                    }
                    FlowControl::UnconditionalBranch => {
                        if let Some(t) = targets(&i) {
                            work.push(t);
                        }
                        break;
                    }
                    FlowControl::IndirectBranch => break,
                    FlowControl::ConditionalBranch => {
                        if let Some(t) = targets(&i) {
                            work.push(t);
                        }
                    }
                    _ => {}
                }
                ip = ip.saturating_add(len);
            }
        }
        out.push(Function {
            entry: start,
            name: None,
            insns: body.into_values().collect(),
        });
    }
    (out, starved)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_record_array_and_a_format_one_object_read_alike() {
        let a = r#"[{"addr":"401000","name":"f","thunk":0,"ins":[["401000","mov","8b ff",0],["401002","ret","c3",0]]}]"#;
        let f = from_ghidra(a, 0x40_0000).unwrap();
        assert_eq!(f.len(), 1);
        assert_eq!(f[0].entry, 0x1000);
        assert_eq!(
            f[0].insns,
            vec![
                Insn {
                    rva: 0x1000,
                    bytes: vec![0x8b, 0xff]
                },
                Insn {
                    rva: 0x1002,
                    bytes: vec![0xc3]
                }
            ]
        );
        let o = r#"{"format":1,"image_base":"0x10000000","functions":[{"addr":"0x10001000","ins":[]}]}"#;
        assert_eq!(from_ghidra(o, 0x40_0000).unwrap()[0].entry, 0x1000);
        assert!(from_ghidra(r#"{"format":2}"#, 0).is_err());
        assert!(from_ghidra(r#"[{"addr":"1","ins":[["1","x","zz",0]]}]"#, 0).is_err());
    }

    #[test]
    fn decoding_follows_branches_and_skips_the_table_between_them() {
        let text = [
            0x85, 0xc0, 0x74, 0x06, 0xeb, 0x07, 0xff, 0xff, 0xff, 0xff, 0x33, 0xc0, 0xc3, 0xb0,
            0x01, 0xc3,
        ];
        let f = decode(&text, 0x1000, &[(0x1000, Some(0x1010))]);
        let at: Vec<u32> = f[0].insns.iter().map(|i| i.rva).collect();
        assert_eq!(
            at,
            vec![0x1000, 0x1002, 0x1004, 0x100a, 0x100c, 0x100d, 0x100f]
        );
        let none = decode(&text, 0x1000, &[(0x2000, None), (0x0800, None)]);
        assert!(none.is_empty());
    }
}
