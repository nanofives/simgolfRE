use std::collections::BTreeMap;

pub const FUNCLET_SEGMENT: &str = "text$x";

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Proc {
    pub name: String,
    pub calls: Vec<String>,
    pub jumps: Vec<String>,
    pub funclet_calls: Vec<String>,
}

fn words(raw: &str) -> Vec<&str> {
    raw.split(';')
        .next()
        .unwrap_or_default()
        .split_whitespace()
        .collect()
}

fn target_of(first: &str, rest: &[&str]) -> String {
    match (first, rest) {
        ("DWORD", ["PTR", t, ..]) => (*t).to_string(),
        _ => first.to_string(),
    }
}

#[must_use]
pub fn procs(text: &str) -> BTreeMap<String, Proc> {
    let mut out: BTreeMap<String, Proc> = BTreeMap::new();
    let mut cur: Option<String> = None;
    let mut funclet = false;
    for raw in text.lines() {
        let w = words(raw);
        let lead = !raw.starts_with(char::is_whitespace);
        match w.as_slice() {
            [name, "PROC", ..] if lead => {
                let n = (*name).to_string();
                out.entry(n.clone()).or_insert_with(|| Proc {
                    name: n.clone(),
                    ..Proc::default()
                });
                cur = Some(n);
                funclet = false;
            }
            [_, "ENDP", ..] if lead => {
                cur = None;
                funclet = false;
            }
            [seg, "SEGMENT", ..] if lead => funclet = *seg == FUNCLET_SEGMENT,
            [_, "ENDS", ..] if lead => funclet = false,
            _ => {
                let code = w
                    .iter()
                    .position(|x| !x.bytes().all(|b| b.is_ascii_hexdigit()))
                    .and_then(|k| w.get(k..))
                    .unwrap_or_default();
                if let ([op @ ("call" | "jmp"), target, rest @ ..], Some(p)) =
                    (code, cur.as_ref().and_then(|c| out.get_mut(c)))
                {
                    let t = target_of(target, rest);
                    match (*op, funclet) {
                        (_, true) => p.funclet_calls.push(t),
                        ("call", false) => p.calls.push(t),
                        _ => p.jumps.push(t),
                    }
                }
            }
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_listing_gives_each_procs_call_targets_and_its_funclets_apart() {
        let l = "_TEXT\tSEGMENT\n?f@@YAHXZ PROC NEAR\t\t\t\t\t; f, COMDAT\n  0000b\te8 00 00 00 00\t call\t ?g@@YAHXZ\t\t\t; g\n\tcall\tDWORD PTR __imp__Sleep@4\n\tjmp\t$L12\n_TEXT\tENDS\ntext$x\tSEGMENT\n$L9:\n\tcall\t??3@YAXPAX@Z\n\tjmp\t??1X@@QAE@XZ\ntext$x\tENDS\n?f@@YAHXZ ENDP\t\t\t\t\t; f\n\tcall\t?h@@YAXXZ\n";
        let p = procs(l);
        let f = &p["?f@@YAHXZ"];
        assert_eq!(f.calls, vec!["?g@@YAHXZ", "__imp__Sleep@4"]);
        assert_eq!(f.jumps, vec!["$L12"]);
        assert_eq!(f.funclet_calls, vec!["??3@YAXPAX@Z", "??1X@@QAE@XZ"]);
        assert_eq!(p.len(), 1);
    }
}
