use anyhow::{Context as _, Result, bail};
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::rc::Rc;

pub const HEAD: &str = "image\tva\tkind\tpath\tnth\tanchor\tname";

pub const HEAD_LINES: usize = 12;

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Row {
    pub image: String,
    pub va: u64,
    pub kind: String,
    pub path: String,
    pub nth: usize,
    pub anchor: String,
    pub name: String,
}

impl Row {
    #[must_use]
    pub fn is_library_entry(&self) -> bool {
        self.path == "-"
    }
}

fn unescape(s: &str) -> String {
    let mut o = String::with_capacity(s.len());
    let mut it = s.chars();
    while let Some(c) = it.next() {
        if c != '\\' {
            o.push(c);
            continue;
        }
        match it.next() {
            Some('n') => o.push('\n'),
            Some('t') => o.push('\t'),
            Some('r') => o.push('\r'),
            Some(x) => o.push(x),
            None => o.push('\\'),
        }
    }
    o
}

pub fn parse(text: &str) -> Result<Vec<Row>> {
    let mut out = Vec::new();
    for (i, l) in text.lines().enumerate() {
        if l.is_empty() || l == HEAD || l.starts_with('#') {
            continue;
        }
        let line = i.saturating_add(1);
        let f: Vec<&str> = l.split('\t').collect();
        let [image, va, kind, path, nth, anchor, name] = f.as_slice() else {
            bail!("line {line}: {} field(s), not the 7 of `{HEAD}`", f.len());
        };
        let va = u64::from_str_radix(va.trim_start_matches("0x"), 16)
            .with_context(|| format!("line {line}: address {va:?}"))?;
        let nth: usize = nth
            .parse()
            .with_context(|| format!("line {line}: nth {nth:?}"))?;
        out.push(Row {
            image: (*image).to_string(),
            va,
            kind: (*kind).to_string(),
            path: (*path).to_string(),
            nth,
            anchor: unescape(anchor),
            name: if *name == "-" {
                String::new()
            } else {
                unescape(name)
            },
        });
    }
    Ok(out)
}

#[must_use]
pub fn normalize(line: &str) -> String {
    line.split_whitespace().collect::<Vec<_>>().join(" ")
}

fn directive_word(t: &str) -> Option<&str> {
    let r = t.strip_prefix('#')?.trim_start();
    let end = r
        .find(|c: char| !c.is_ascii_alphanumeric())
        .unwrap_or(r.len());
    r.get(..end)
}

fn is_code(line: &str) -> bool {
    let t = line.trim();
    !t.is_empty()
        && !t.starts_with("//")
        && !directive_word(t).is_some_and(|w| {
            matches!(
                w,
                "line" | "if" | "ifdef" | "ifndef" | "elif" | "else" | "endif"
            )
        })
}

#[derive(Debug, Clone, Default)]
pub struct Code {
    pub lines: Vec<(usize, String)>,
    at: BTreeMap<String, Vec<usize>>,
    found: RefCell<BTreeMap<String, Rc<Vec<usize>>>>,
}

#[must_use]
pub fn code_lines(text: &str) -> Code {
    let lines: Vec<(usize, String)> = text
        .lines()
        .enumerate()
        .filter(|(_, l)| is_code(l))
        .map(|(i, l)| (i, normalize(l)))
        .collect();
    let mut at: BTreeMap<String, Vec<usize>> = BTreeMap::new();
    for (k, (_, t)) in lines.iter().enumerate() {
        at.entry(t.clone()).or_default().push(k);
    }
    Code {
        lines,
        at,
        found: RefCell::new(BTreeMap::new()),
    }
}

fn occurrences(code: &Code, anchor: &str) -> Rc<Vec<usize>> {
    if let Some(v) = code.found.borrow().get(anchor) {
        return Rc::clone(v);
    }
    let want: Vec<&str> = anchor.split('\n').collect();
    let found: Vec<usize> = match want.split_first() {
        Some((first, rest)) => code
            .at
            .get(*first)
            .map(|ks| {
                ks.iter()
                    .copied()
                    .filter(|&k| {
                        rest.iter().enumerate().all(|(d, w)| {
                            code.lines
                                .get(k.saturating_add(d).saturating_add(1))
                                .is_some_and(|(_, t)| t == w)
                        })
                    })
                    .collect()
            })
            .unwrap_or_default(),
        None => Vec::new(),
    };
    let found = Rc::new(found);
    code.found
        .borrow_mut()
        .insert(anchor.to_string(), Rc::clone(&found));
    found
}

pub fn resolve(row: &Row, code: &Code) -> Result<usize, String> {
    if row.is_library_entry() {
        return Err("a library entry names no definition".to_string());
    }
    let occ = occurrences(code, &row.anchor);
    let head = row.anchor.split('\n').next().unwrap_or_default();
    let k = match (row.nth, occ.as_slice()) {
        (_, []) => return Err(format!("its anchor `{head}` is not in the file")),
        (0, [k]) => *k,
        (0, _) => {
            return Err(format!(
                "its anchor `{head}` occurs {} times and the row names none",
                occ.len()
            ));
        }
        (n, _) => *occ.get(n.saturating_sub(1)).ok_or_else(|| {
            format!(
                "its anchor occurs {} time(s), and the row names occurrence {n}",
                occ.len()
            )
        })?,
    };
    code.lines
        .get(k)
        .map(|(i, _)| i.saturating_add(1))
        .ok_or_else(|| "anchor out of range".to_string())
}

#[must_use]
pub fn head_of(text: &str) -> String {
    let mut out = String::new();
    let mut depth = 0i32;
    for l in text.split('\n') {
        if !out.is_empty() {
            out.push(' ');
        }
        out.push_str(l);
        for c in l.chars() {
            match c {
                '(' => depth = depth.saturating_add(1),
                ')' => depth = depth.saturating_sub(1),
                _ => {}
            }
        }
        if depth <= 0 && out.contains('(') {
            break;
        }
    }
    out
}

#[must_use]
pub fn definition_at(text: &str, line: usize) -> String {
    let lines: Vec<&str> = text
        .lines()
        .skip(line.saturating_sub(1))
        .take(HEAD_LINES)
        .collect();
    head_of(&lines.join("\n"))
}

#[must_use]
pub fn enclosing_class(text: &str, line: usize) -> Option<String> {
    let mut stack: Vec<(i32, Option<String>)> = Vec::new();
    let mut depth = 0i32;
    let mut pending: Option<String> = None;
    for l in text.lines().take(line) {
        let t = l.trim();
        let first = t
            .split(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
            .next()
            .unwrap_or("");
        let head = if first == "template" {
            t.split('>').nth(1).unwrap_or("").trim()
        } else {
            t
        };
        let mut hw = head.split_whitespace();
        if let (Some(kw), Some(name)) = (hw.next(), hw.next())
            && (kw == "struct" || kw == "class")
            && !head.ends_with(';')
        {
            let nm: String = name
                .chars()
                .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
                .collect();
            if !nm.is_empty() {
                pending = Some(nm);
            }
        }
        for c in l.chars() {
            match c {
                '{' => {
                    depth = depth.saturating_add(1);
                    stack.push((depth, pending.take()));
                }
                '}' => {
                    while stack.last().is_some_and(|(d, _)| *d >= depth) {
                        stack.pop();
                    }
                    depth = depth.saturating_sub(1);
                }
                ';' if stack.last().is_none_or(|(d, _)| *d < depth) => pending = None,
                _ => {}
            }
        }
    }
    stack.iter().rev().find_map(|(_, n)| n.clone())
}
