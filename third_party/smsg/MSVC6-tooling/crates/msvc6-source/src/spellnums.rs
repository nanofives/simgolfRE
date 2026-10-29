use crate::defs;
use crate::lex::{self, Kind};
use crate::renamecheck;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Constant {
    pub name: String,
    pub spelled: String,
    pub value: i128,
    pub lines: BTreeSet<usize>,
    pub meaning: String,
    pub group: String,
    pub define: bool,
}

#[must_use]
pub fn line_set(spec: &str) -> BTreeSet<usize> {
    let mut out = BTreeSet::new();
    for w in spec.split([' ', ',']).filter(|w| !w.is_empty()) {
        match w.split_once('-') {
            Some((a, b)) => {
                if let (Ok(a), Ok(b)) = (a.parse::<usize>(), b.parse::<usize>()) {
                    out.extend(a..=b);
                }
            }
            None => {
                if let Ok(a) = w.parse::<usize>() {
                    out.insert(a);
                }
            }
        }
    }
    out
}

pub fn table(text: &str) -> Result<Vec<Constant>, String> {
    let mut out = Vec::new();
    for (n, row) in text.lines().enumerate() {
        if row.trim().is_empty() {
            continue;
        }
        let f: Vec<&str> = row.split('\t').map(str::trim).collect();
        let (Some(name), Some(value), Some(lines)) = (f.first(), f.get(1), f.get(2)) else {
            return Err(format!("row {}: name, value and lines are required", n + 1));
        };
        let v = lex::number_value(value)
            .or_else(|| lex::char_value(value))
            .ok_or_else(|| format!("row {}: `{value}` is not a number", n + 1))?;
        let literal = name.starts_with('\'');
        if !literal
            && (name.is_empty()
                || !name.bytes().all(lex::ident_byte)
                || !name.bytes().next().is_some_and(lex::ident_start))
        {
            return Err(format!("row {}: `{name}` is not an identifier", n + 1));
        }
        if literal && lex::char_value(name) != Some(v) {
            return Err(format!("row {}: `{name}` does not spell {value}", n + 1));
        }
        out.push(Constant {
            name: (*name).to_string(),
            spelled: (*value).to_string(),
            value: v,
            lines: line_set(lines),
            meaning: f.get(3).copied().unwrap_or_default().to_string(),
            group: f.get(4).copied().unwrap_or_default().to_string(),
            define: !literal && f.get(5).is_none_or(|d| *d != "no"),
        });
    }
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Spelt {
    #[serde(skip)]
    pub bytes: Vec<u8>,
    pub edits: usize,
    pub used: Vec<String>,
    pub defined: Vec<String>,
    pub ambiguous: usize,
    pub clashes: Vec<String>,
    pub verdict: Option<renamecheck::Verdict>,
}

pub fn spell(
    src: &[u8],
    function: &str,
    consts: &[Constant],
    after: Option<&str>,
) -> Result<Spelt, String> {
    let lx = lex::lex(src);
    let ds = defs::definitions(&lx);
    let d = defs::one(&ds, function)?;
    let used_ids: BTreeSet<&str> = lx
        .tokens
        .iter()
        .filter(|t| t.kind == Kind::Ident)
        .map(|t| t.text.as_str())
        .collect();
    let mut clashes = Vec::new();
    let mut usable: Vec<&Constant> = Vec::new();
    for c in consts {
        if c.define && used_ids.contains(c.name.as_str()) {
            clashes.push(c.name.clone());
        } else {
            usable.push(c);
        }
    }
    let mut at: BTreeMap<(usize, i128), BTreeSet<usize>> = BTreeMap::new();
    for (k, c) in usable.iter().enumerate() {
        for l in &c.lines {
            at.entry((*l, c.value)).or_default().insert(k);
        }
    }
    let mut edits: Vec<(usize, usize, String)> = Vec::new();
    let mut used: BTreeSet<usize> = BTreeSet::new();
    let mut ambiguous = 0usize;
    for t in lx
        .tokens
        .get(d.open..=d.close)
        .unwrap_or_default()
        .iter()
        .filter(|t| t.in_code() && t.kind == Kind::Number)
    {
        let Some(v) = lex::number_value(&t.text) else {
            continue;
        };
        let Some(ks) = at.get(&(t.line, v)) else {
            continue;
        };
        let names: BTreeSet<&str> = ks
            .iter()
            .filter_map(|k| usable.get(*k).map(|c| c.name.as_str()))
            .collect();
        if names.len() != 1 {
            ambiguous += 1;
            continue;
        }
        let Some(&k) = ks.iter().next() else {
            continue;
        };
        let Some(c) = usable.get(k) else {
            continue;
        };
        edits.push((t.start, t.end, c.name.clone()));
        used.insert(k);
    }
    let mut out = src.to_vec();
    for (s, e, name) in edits.iter().rev() {
        out.splice(*s..*e, name.bytes());
    }
    let mut groups: Vec<&str> = Vec::new();
    for k in &used {
        if let Some(c) = usable.get(*k).filter(|c| c.define)
            && !groups.contains(&c.group.as_str())
        {
            groups.push(&c.group);
        }
    }
    let eol: &[u8] = if src.windows(2).any(|w| w == b"\r\n") {
        b"\r\n"
    } else {
        b"\n"
    };
    let mut block: Vec<u8> = Vec::new();
    let mut defined = Vec::new();
    let width = used
        .iter()
        .filter_map(|k| usable.get(*k))
        .map(|c| c.name.len())
        .max()
        .unwrap_or(0);
    for (gi, g) in groups.iter().enumerate() {
        if gi > 0 {
            block.extend_from_slice(eol);
        }
        for k in &used {
            let Some(c) = usable.get(*k) else {
                continue;
            };
            if !c.define || c.group != *g || defined.contains(&c.name) {
                continue;
            }
            block.extend(format!("#define {:<width$} {}", c.name, c.spelled).into_bytes());
            block.extend_from_slice(eol);
            defined.push(c.name.clone());
        }
    }
    if !block.is_empty() {
        let lines = lex::physical_lines(&out);
        let insert_at = match after {
            Some(text) => {
                let needle = text.as_bytes();
                lines
                    .iter()
                    .find(|l| {
                        out.get(l.0..l.1)
                            .is_some_and(|b| b.windows(needle.len().max(1)).any(|w| w == needle))
                    })
                    .map(|l| l.2)
                    .ok_or_else(|| format!("no line holds `{text}`"))?
            }
            None => {
                let lx2 = lex::lex(&out);
                let ds2 = defs::definitions(&lx2);
                let d2 = defs::one(&ds2, function)?;
                lx2.tokens.get(d2.head).map_or(0, |t| {
                    lines
                        .iter()
                        .find(|l| l.0 <= t.start && t.start <= l.2)
                        .map_or(0, |l| l.0)
                })
            }
        };
        let mut chunk = block;
        if after.is_none() {
            chunk.extend_from_slice(eol);
        } else {
            let mut lead = eol.to_vec();
            lead.extend(chunk);
            chunk = lead;
        }
        out.splice(insert_at..insert_at, chunk);
    }
    let verdict = renamecheck::check_named(
        src,
        &out,
        &[(function.to_string(), function.to_string())],
        &renamecheck::Options::default(),
    )
    .into_iter()
    .next()
    .and_then(Result::ok);
    Ok(Spelt {
        bytes: out,
        edits: edits.len(),
        used: used
            .iter()
            .filter_map(|k| usable.get(*k).map(|c| c.name.clone()))
            .collect(),
        defined,
        ambiguous,
        clashes,
        verdict,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn numbers_on_listed_lines_take_their_constants_and_the_proof_holds() {
        let src = b"#include \"a.h\"\nint f(int i)\n{\n    if (i >= 40)\n        return 0x0a;\n    return 40 + 1;\n}\n";
        let consts = table("LIMIT\t40\t4\tthe slot count\tslots\nBAD_PARAM\t0x0a\t5-5\tan error\terrors\nCHAR_A\t'A'\t6\n").unwrap();
        let s = spell(src, "f", &consts, Some("#include")).unwrap();
        let text = msvc6_base::srcio::decode(&s.bytes);
        assert_eq!(
            text,
            "#include \"a.h\"\n\n#define LIMIT     40\n\n#define BAD_PARAM 0x0a\nint f(int i)\n{\n    if (i >= LIMIT)\n        return BAD_PARAM;\n    return 40 + 1;\n}\n"
        );
        assert_eq!(s.edits, 2);
        assert!(s.verdict.unwrap().proven);
        assert!(!text.contains("/*") && !text.contains("//"));
    }

    #[test]
    fn a_name_the_file_spells_is_refused_and_a_contested_number_stays() {
        let src = b"int LIMIT;\nint f(int i)\n{\n    return i + 4;\n}\n";
        let consts = table("LIMIT\t4\t4\nFOUR\t4\t4\nQUAD\t4\t4\n").unwrap();
        let s = spell(src, "f", &consts, None).unwrap();
        assert_eq!(s.clashes, ["LIMIT"]);
        assert_eq!(s.edits, 0);
        assert_eq!(s.ambiguous, 1);
        assert!(table("1BAD\t3\t4\n").is_err());
        assert_eq!(line_set("3 5-7,9"), BTreeSet::from([3, 5, 6, 7, 9]));
    }
}
