use anyhow::{Context as _, Result, bail};
use std::path::{Path, PathBuf};

pub const MARKER_TYPES: [&str; 9] = [
    "FUNCTION",
    "STUB",
    "SYNTHETIC",
    "TEMPLATE",
    "LIBRARY",
    "GLOBAL",
    "VTABLE",
    "STRING",
    "LINE",
];

pub const FUNCTION_TYPES: [&str; 6] = [
    "function",
    "stub",
    "synthetic",
    "template",
    "library",
    "thunk",
];

pub const SOURCE_EXTENSIONS: [&str; 8] = ["c", "cpp", "cxx", "cc", "h", "hpp", "hxx", "inl"];

pub const MAX_FILES: usize = 100_000;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CsvRow {
    pub line: usize,
    pub va: u32,
    pub name: Option<String>,
    pub kind: String,
    pub size: Option<u32>,
}

fn split_csv(line: &str, delim: char) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut quoted = false;
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '"' if quoted && chars.peek() == Some(&'"') => {
                cur.push('"');
                chars.next();
            }
            '"' => quoted = !quoted,
            c if c == delim && !quoted => out.push(std::mem::take(&mut cur)),
            c => cur.push(c),
        }
    }
    out.push(cur);
    out
}

fn hex(s: &str) -> Option<u32> {
    let t = s.trim();
    let t = t
        .strip_prefix("0x")
        .or_else(|| t.strip_prefix("0X"))
        .unwrap_or(t);
    u32::from_str_radix(t, 16).ok()
}

pub fn read_csv(path: &Path) -> Result<Vec<CsvRow>> {
    let text =
        msvc6_base::srcio::read(path).with_context(|| format!("reading {}", path.display()))?;
    parse_csv(path, &text)
}

pub fn parse_csv(path: &Path, text: &str) -> Result<Vec<CsvRow>> {
    let mut lines = text
        .lines()
        .enumerate()
        .filter(|(_, l)| !l.trim().is_empty());
    let Some((_, head)) = lines.next() else {
        bail!("{}: empty: no header row", path.display());
    };
    let head = head.trim_end_matches('\r');
    let delim = if head.contains('|') && !head.contains(',') {
        '|'
    } else {
        ','
    };
    let cols: Vec<String> = split_csv(head, delim)
        .into_iter()
        .map(|c| c.trim().to_ascii_lowercase())
        .collect();
    let col = |n: &str| cols.iter().position(|c| c == n);
    let addr = col("address").with_context(|| {
        format!(
            "{}:1: no `address` column (the header is `{head}`)",
            path.display()
        )
    })?;
    let (name, kind, size) = (col("name"), col("type"), col("size"));
    let mut out = Vec::new();
    for (i, l) in lines {
        let n = i.saturating_add(1);
        let f = split_csv(l.trim_end_matches('\r'), delim);
        let get = |c: Option<usize>| c.and_then(|c| f.get(c)).map(|s| s.trim().to_string());
        let a = get(Some(addr)).unwrap_or_default();
        let va =
            hex(&a).with_context(|| format!("{}:{n}: `{a}` is not an address", path.display()))?;
        let size =
            match get(size).filter(|s| !s.is_empty()) {
                Some(s) => Some(s.parse::<u32>().ok().or_else(|| hex(&s)).with_context(|| {
                    format!("{}:{n}: size `{s}` is not a number", path.display())
                })?),
                None => None,
            };
        out.push(CsvRow {
            line: n,
            va,
            name: get(name).filter(|s| !s.is_empty()),
            kind: get(kind).unwrap_or_default().to_ascii_lowercase(),
            size,
        });
    }
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Marker {
    pub kind: String,
    pub module: String,
    pub va: u64,
    pub extra: Option<String>,
}

fn word(s: &str) -> (&str, &str) {
    let end = s
        .find(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
        .unwrap_or(s.len());
    (s.get(..end).unwrap_or(""), s.get(end..).unwrap_or(""))
}

#[must_use]
pub fn parse_marker(line: &str) -> Option<Marker> {
    let s = line.trim().strip_prefix("//")?.trim_start();
    let (kind, rest) = word(s);
    if kind.is_empty() {
        return None;
    }
    let rest = rest.strip_prefix(':')?.trim_start();
    let (module, rest) = word(rest);
    if module.is_empty() || !rest.starts_with(char::is_whitespace) {
        return None;
    }
    let rest = rest.trim_start();
    let h = rest
        .strip_prefix("0x")
        .or_else(|| rest.strip_prefix("0X"))?;
    let end = h.find(|c: char| !c.is_ascii_hexdigit()).unwrap_or(h.len());
    let va = u64::from_str_radix(h.get(..end)?, 16).ok()?;
    let tail = h.get(end..).unwrap_or("");
    if !tail.is_empty() && !tail.starts_with(' ') {
        return None;
    }
    let extra = tail.trim();
    Some(Marker {
        kind: kind.to_ascii_uppercase(),
        module: module.to_ascii_uppercase(),
        va,
        extra: (extra.len() >= 3).then(|| extra.to_string()),
    })
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Found {
    pub file: PathBuf,
    pub line: usize,
    pub marker: Marker,
    pub name: Option<String>,
}

fn ident_before(s: &str, stop: &[char]) -> Option<String> {
    let cut = s.find(stop).unwrap_or(s.len());
    let head = s.get(..cut)?.trim_end();
    let start = head
        .rfind(|c: char| {
            !(c.is_ascii_alphanumeric() || matches!(c, '_' | ':' | '~' | '$' | '?' | '@'))
        })
        .map_or(0, |i| i.saturating_add(1));
    let id = head.get(start..)?.trim_start_matches(':');
    (!id.is_empty() && !id.starts_with(|c: char| c.is_ascii_digit())).then(|| id.to_string())
}

fn joined_to(lines: &[&str], stop: &[char]) -> String {
    let mut out = String::new();
    for l in lines {
        if !out.is_empty() {
            out.push(' ');
        }
        out.push_str(l);
        if l.contains(stop) {
            break;
        }
    }
    out
}

fn name_of(kind: &str, nameref: bool, below: &[&str]) -> Option<String> {
    let mut it = below.iter().map(|l| l.trim()).filter(|l| !l.is_empty());
    if nameref {
        let l = it.find(|l| parse_marker(l).is_none())?;
        let n = l.strip_prefix("//")?.trim();
        return (!n.is_empty()).then(|| n.to_string());
    }
    let code: Vec<&str> = it
        .skip_while(|l| parse_marker(l).is_some() || l.starts_with("//"))
        .take_while(|l| parse_marker(l).is_none() && !l.starts_with("//"))
        .collect();
    code.first()?;
    match kind {
        "GLOBAL" => ident_before(&joined_to(&code, &['=', ';', '[']), &['=', ';', '[']),
        "VTABLE" => {
            let j = joined_to(&code, &['{', ':', ';']);
            let r = j
                .strip_prefix("class ")
                .or_else(|| j.strip_prefix("struct "))?;
            ident_before(r.trim_start(), &[' ', ':', '{', ';'])
        }
        "STRING" | "LINE" => None,
        _ => ident_before(&joined_to(&code, &['(']), &['(']),
    }
}

pub fn scan_text(file: &Path, text: &str, module: &str, out: &mut Vec<Found>) {
    let lines: Vec<&str> = text.lines().collect();
    for (i, l) in lines.iter().enumerate() {
        let Some(m) = parse_marker(l) else {
            continue;
        };
        if !m.module.eq_ignore_ascii_case(module) || !MARKER_TYPES.contains(&m.kind.as_str()) {
            continue;
        }
        let nameref = matches!(m.kind.as_str(), "SYNTHETIC" | "TEMPLATE")
            || m.extra
                .as_deref()
                .is_some_and(|e| e.eq_ignore_ascii_case("SYMBOL"));
        let below = lines.get(i.saturating_add(1)..).unwrap_or_default();
        let name = name_of(
            &m.kind,
            nameref,
            below.get(..below.len().min(8)).unwrap_or_default(),
        );
        out.push(Found {
            file: file.to_path_buf(),
            line: i.saturating_add(1),
            marker: m,
            name,
        });
    }
}

pub fn scan_dir(root: &Path, module: &str) -> Result<(Vec<Found>, usize)> {
    let mut files = Vec::new();
    let mut stack = vec![root.to_path_buf()];
    while let Some(d) = stack.pop() {
        let rd = std::fs::read_dir(&d).with_context(|| format!("reading {}", d.display()))?;
        for e in rd.flatten() {
            let p = e.path();
            let Ok(t) = e.file_type() else {
                continue;
            };
            if t.is_dir() {
                if p.file_name().is_some_and(|n| n != ".git") {
                    stack.push(p);
                }
            } else if t.is_file()
                && p.extension().is_some_and(|x| {
                    SOURCE_EXTENSIONS.contains(&x.to_string_lossy().to_ascii_lowercase().as_str())
                })
            {
                files.push(p);
                if files.len() > MAX_FILES {
                    bail!(
                        "{}: more than {MAX_FILES} source files; point the provider at a source directory",
                        root.display()
                    );
                }
            }
        }
    }
    files.sort();
    let mut out = Vec::new();
    for f in &files {
        let text =
            msvc6_base::srcio::read(f).with_context(|| format!("reading {}", f.display()))?;
        scan_text(f, &text, module, &mut out);
    }
    Ok((out, files.len()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn markers_follow_the_reccmp_grammar_and_name_what_is_below_them() {
        let src = "// FUNCTION: APP 0x401000\nint __cdecl Run(int a)\n{\n}\n\n// GLOBAL: APP 0x402000\nint g_count = 0;\n\
                   // VTABLE: APP 0x403000\nclass Widget : public Base {\n};\n// SYNTHETIC: APP 0x401100\n// Widget::`scalar deleting destructor'\n\
                   // FUNCTION: OTHER 0x401200\nvoid Other();\n// STRING: APP 0x404000\nconst char* s = \"x\";\n\
                   //FUNCTION:APP 0x401300\nvoid App::Tick()\n// FUNCTION: APP 0x40zz\n\
                   // FUNCTION: APP 0x401400\nstatic int __stdcall\nLater(int a,\n      int b)\n\
                   // GLOBAL: APP 0x402100\nconst char*\n    g_table[] = {0};\n";
        let mut out = Vec::new();
        scan_text(Path::new("a.cpp"), src, "app", &mut out);
        let got: Vec<(String, u64, Option<&str>)> = out
            .iter()
            .map(|f| (f.marker.kind.clone(), f.marker.va, f.name.as_deref()))
            .collect();
        assert_eq!(
            got,
            [
                ("FUNCTION".to_string(), 0x40_1000, Some("Run")),
                ("GLOBAL".to_string(), 0x40_2000, Some("g_count")),
                ("VTABLE".to_string(), 0x40_3000, Some("Widget")),
                (
                    "SYNTHETIC".to_string(),
                    0x40_1100,
                    Some("Widget::`scalar deleting destructor'")
                ),
                ("STRING".to_string(), 0x40_4000, None),
                ("FUNCTION".to_string(), 0x40_1300, Some("App::Tick")),
                ("FUNCTION".to_string(), 0x40_1400, Some("Later")),
                ("GLOBAL".to_string(), 0x40_2100, Some("g_table")),
            ]
        );
        assert_eq!(out[0].line, 1);
    }

    #[test]
    fn a_csv_is_read_by_its_header() {
        let t = "address,name,type,size\n0x401000,Run,function,32\n0x402000,\"a,b\",global,\n";
        let r = parse_csv(Path::new("x.csv"), t).unwrap();
        assert_eq!(r.len(), 2);
        assert_eq!(r[0].size, Some(32));
        assert_eq!(r[1].name.as_deref(), Some("a,b"));
        assert_eq!(r[1].size, None);
        let piped = parse_csv(Path::new("x.csv"), "address|name|type\n0x10|A|function\n").unwrap();
        assert_eq!(piped[0].name.as_deref(), Some("A"));
        assert!(parse_csv(Path::new("x.csv"), "name,type\nA,function\n").is_err());
        let e = parse_csv(Path::new("x.csv"), "address,name\n0xQ,A\n").unwrap_err();
        assert!(e.to_string().contains("x.csv:2"), "{e}");
    }
}
