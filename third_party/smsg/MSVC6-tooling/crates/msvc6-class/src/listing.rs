use anyhow::{Result, bail};

pub const MAX_FUNCTIONS: usize = 1 << 16;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Insn {
    pub off: u32,
    pub text: String,
    pub line: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Function {
    pub insns: Vec<Insn>,
}

fn join_text<'a>(fields: impl Iterator<Item = &'a str>) -> String {
    let joined = fields
        .map(str::trim)
        .filter(|f| !f.is_empty())
        .collect::<Vec<_>>()
        .join(" ");
    joined.split(';').next().unwrap_or("").trim().to_string()
}

fn source_line(line: &str) -> Option<u32> {
    line.strip_prefix("; ")
        .and_then(|r| r.split_once(':'))
        .and_then(|(n, _)| n.trim().parse::<u32>().ok())
        .or_else(|| {
            line.strip_prefix("; Line ")
                .and_then(|n| n.trim().parse::<u32>().ok())
        })
}

#[must_use]
pub fn functions(text: &str) -> Vec<(String, Function)> {
    let mut funcs: Vec<(String, Function)> = Vec::new();
    let mut cur: Option<(String, Function)> = None;
    let mut src_line = 0u32;
    for raw in text.lines() {
        let line = raw.trim_end_matches('\r');
        if let Some(n) = source_line(line) {
            src_line = n;
        }
        if let Some((name, f)) = cur.as_mut() {
            if line.starts_with(name.as_str()) && line.contains(" ENDP") {
                if let Some(done) = cur.take()
                    && funcs.len() < MAX_FUNCTIONS
                {
                    funcs.push(done);
                }
                continue;
            }
            let t = line.trim_start_matches(' ');
            let lead = line.len() - t.len();
            let mut fields = t.split('\t');
            let first = fields.next().unwrap_or("");
            if lead == 2 && first.len() == 5 && first.chars().all(|c| c.is_ascii_hexdigit()) {
                f.insns.push(Insn {
                    off: u32::from_str_radix(first, 16).unwrap_or(0),
                    text: join_text(fields.skip(1)),
                    line: src_line,
                });
            } else if line.starts_with('\t')
                && let Some(last) = f.insns.last_mut()
                && last.text.is_empty()
            {
                let body = join_text(line.split('\t').skip(2));
                if !body.is_empty() {
                    last.text = body;
                }
            }
            continue;
        }
        if let Some(p) = line.find(" PROC NEAR") {
            cur = Some((
                line.get(..p).unwrap_or("").trim().to_string(),
                Function::default(),
            ));
        }
    }
    funcs
}

pub fn function(text: &str, symbol: Option<&str>) -> Result<(String, Function)> {
    let mut funcs = functions(text);
    let names = |f: &[(String, Function)]| {
        f.iter()
            .map(|(n, _)| n.as_str())
            .collect::<Vec<_>>()
            .join(", ")
    };
    match symbol {
        Some(want) => match funcs.iter().position(|(n, _)| n == want) {
            Some(i) => Ok(funcs.swap_remove(i)),
            None => bail!(
                "no function `{want}' in the listing (it has: {})",
                names(&funcs)
            ),
        },
        None if funcs.len() == 1 => Ok(funcs.remove(0)),
        None => bail!(
            "the listing holds {} functions; name one with --symbol ({})",
            funcs.len(),
            names(&funcs)
        ),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_listing_gives_each_instruction_its_offset_text_and_line() {
        let cod = concat!(
            ";\tCOMDAT ?f@N@@QAEHH@Z\r\n",
            "?f@N@@QAEHH@Z PROC NEAR\t\t\t\t\t; N::f, COMDAT\r\n",
            "; 12   :     return g();\r\n",
            "  00000\tc7 84 24 60 22\r\n",
            "\t00 00 09 00 00\r\n",
            "\t00\t\t mov\t DWORD PTR __$EHRec$[esp+8768], 9\r\n",
            "  0000b\te8 00 00 00 00\t call\t ?g@@YAHXZ ; g\r\n",
            "?f@N@@QAEHH@Z ENDP\t\t\t\t\t; N::f\r\n",
            "?h@@YAXXZ PROC NEAR\r\n",
            "?h@@YAXXZ ENDP\r\n",
        );
        let all = functions(cod);
        assert_eq!(all.len(), 2);
        let (name, f) = function(cod, Some("?f@N@@QAEHH@Z")).unwrap();
        assert_eq!(name, "?f@N@@QAEHH@Z");
        assert_eq!(
            f.insns,
            vec![
                Insn {
                    off: 0,
                    text: "mov DWORD PTR __$EHRec$[esp+8768], 9".into(),
                    line: 12
                },
                Insn {
                    off: 0xb,
                    text: "call ?g@@YAHXZ".into(),
                    line: 12
                },
            ]
        );
        assert!(function(cod, None).is_err());
        assert!(function(cod, Some("?x@@YAXXZ")).is_err());
    }
}
