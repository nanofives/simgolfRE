use std::collections::BTreeMap;
use std::path::Path;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Source {
    pub base: String,
    pub disk: String,
    pub map: BTreeMap<u32, Vec<u32>>,
    pub directives: usize,
}

pub fn mapping(src: &str) -> (BTreeMap<u32, Vec<u32>>, usize) {
    let mut out: BTreeMap<u32, Vec<u32>> = BTreeMap::new();
    let mut reported: u32 = 1;
    let mut directives = 0usize;
    for (i, line) in src.lines().enumerate() {
        let file_line = u32::try_from(i + 1).unwrap_or(u32::MAX);
        if let Some(n) = line
            .trim_start()
            .strip_prefix("#line")
            .and_then(|rest| rest.split_whitespace().next())
            .and_then(|w| w.parse::<u32>().ok())
        {
            directives += 1;
            reported = n;
            continue;
        }
        out.entry(reported).or_default().push(file_line);
        reported = reported.saturating_add(1);
    }
    (out, directives)
}

pub fn parse_diagnostic(line: &str) -> Option<(&str, u32)> {
    let open = line.find('(')?;
    let close = line.get(open..)?.find(')')? + open;
    if !line.get(close..)?.starts_with(") :") {
        return None;
    }
    let n = line.get(open + 1..close)?.trim().parse::<u32>().ok()?;
    Some((line.get(..open)?, n))
}

pub fn basename(p: &str) -> &str {
    p.rsplit(['\\', '/']).next().unwrap_or(p)
}

impl Source {
    pub fn new(path: &Path, text: &str) -> Self {
        let (map, directives) = mapping(text);
        Self {
            base: basename(&path.to_string_lossy()).to_ascii_lowercase(),
            disk: path.display().to_string(),
            map,
            directives,
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Translated {
    pub text: String,
    pub seen: usize,
    pub annotated: usize,
    pub unmatched: usize,
}

pub fn translate(input: &str, sources: &[Source]) -> Translated {
    let mut t = Translated::default();
    for line in input.lines() {
        t.text.push_str(line);
        t.text.push('\n');
        let Some((path, n)) = parse_diagnostic(line) else {
            continue;
        };
        t.seen += 1;
        let base = basename(path.trim()).to_ascii_lowercase();
        let Some(src) = sources.iter().find(|s| s.base == base) else {
            t.unmatched += 1;
            continue;
        };
        t.annotated += 1;
        let note = if src.directives == 0 {
            format!("line {n} is a file line: this source has no #line directive")
        } else {
            match src.map.get(&n) {
                Some(fl) if fl.len() == 1 => format!(
                    "reported line {n} is {}:{}",
                    src.disk,
                    fl.first().copied().unwrap_or(0)
                ),
                Some(fl) => format!(
                    "reported line {n} is ambiguous: file lines {}",
                    fl.iter()
                        .map(ToString::to_string)
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
                None => format!(
                    "no file line maps to reported line {n}; it is not file line {n} either"
                ),
            }
        };
        t.text.push_str(&format!("    ^ {note}\n"));
    }
    t
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_line_after_a_directive_carries_its_number() {
        let (m, d) = mapping("a\n#line 100\nb\nc\n");
        assert_eq!(d, 1);
        assert_eq!(m[&1], vec![1]);
        assert_eq!(m[&100], vec![3]);
        assert_eq!(m[&101], vec![4]);
        let (m, _) = mapping("x\n#line 1458 \"C:\\\\P\\\\T.cpp\"\ny\n");
        assert_eq!(m[&1458], vec![3]);
        let (m, _) = mapping("#line 10\na\n#line 10\nb\n");
        assert_eq!(m[&10], vec![2, 4]);
    }

    #[test]
    fn diagnostics_are_annotated_and_prose_passes_through() {
        assert_eq!(
            parse_diagnostic("C:\\P\\T.cpp(2353) : error C2143: x"),
            Some(("C:\\P\\T.cpp", 2353))
        );
        assert!(parse_diagnostic("Copyright (C) Microsoft Corp 1984-1998.").is_none());
        assert!(parse_diagnostic("foo(bar) : baz").is_none());
        let s = Source::new(Path::new("/w/T.cpp"), "a\n#line 100\nb\n");
        let t = translate(
            "C:\\P\\T.cpp(100) : error C1: x\nC:\\P\\T.cpp(50) : error C1: y\nother.c(3) : warning\nplain\n",
            &[s],
        );
        assert_eq!((t.seen, t.annotated, t.unmatched), (3, 2, 1));
        assert!(
            t.text.contains("reported line 100 is /w/T.cpp:3"),
            "{}",
            t.text
        );
        assert!(
            t.text.contains("no file line maps to reported line 50"),
            "{}",
            t.text
        );
        assert!(t.text.ends_with("plain\n"));
    }
}
