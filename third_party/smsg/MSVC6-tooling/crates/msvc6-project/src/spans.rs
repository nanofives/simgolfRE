use std::ops::Range;
use toml::de::{DeTable, DeValue};

#[derive(Debug, Clone, PartialEq, Eq)]
struct Entry {
    path: String,
    key: Option<Range<usize>>,
    value: Range<usize>,
}

#[derive(Debug, Clone, Default)]
pub struct Spans {
    entries: Vec<Entry>,
    lines: Vec<usize>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Location {
    pub line: usize,
    pub col: usize,
}

impl std::fmt::Display for Location {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

fn bare(key: &str) -> bool {
    !key.is_empty()
        && key
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '-')
}

#[must_use]
pub fn join(prefix: &str, key: &str) -> String {
    let k = if bare(key) {
        key.to_string()
    } else {
        format!("{key:?}")
    };
    if prefix.is_empty() {
        k
    } else {
        format!("{prefix}.{k}")
    }
}

#[must_use]
pub fn index(prefix: &str, n: usize) -> String {
    format!("{prefix}[{n}]")
}

fn walk_value(path: &str, v: &DeValue<'_>, out: &mut Vec<Entry>) {
    match v {
        DeValue::Table(t) => walk_table(path, t, out),
        DeValue::Array(a) => {
            for (n, item) in a.iter().enumerate() {
                let p = index(path, n);
                out.push(Entry {
                    path: p.clone(),
                    key: None,
                    value: item.span(),
                });
                walk_value(&p, item.get_ref(), out);
            }
        }
        _ => {}
    }
}

fn walk_table(prefix: &str, t: &DeTable<'_>, out: &mut Vec<Entry>) {
    for (k, v) in t.iter() {
        let p = join(prefix, k.get_ref());
        out.push(Entry {
            path: p.clone(),
            key: Some(k.span()),
            value: v.span(),
        });
        walk_value(&p, v.get_ref(), out);
    }
}

impl Spans {
    #[must_use]
    pub fn of(text: &str) -> Self {
        let (doc, _) = DeTable::parse_recoverable(text);
        let mut entries = Vec::new();
        walk_table("", doc.get_ref(), &mut entries);
        let mut lines = vec![0];
        lines.extend(
            text.char_indices()
                .filter(|(_, c)| *c == '\n')
                .map(|(i, _)| i.saturating_add(1)),
        );
        Self { entries, lines }
    }

    #[must_use]
    pub fn location(&self, text: &str, pos: usize) -> Location {
        let line = self.lines.partition_point(|&s| s <= pos).max(1);
        let start = self.lines.get(line.saturating_sub(1)).copied().unwrap_or(0);
        let col = text.get(start..pos.min(text.len())).map_or(1, |s| {
            let bom = usize::from(s.starts_with('\u{feff}'));
            s.chars().count().saturating_add(1).saturating_sub(bom)
        });
        Location { line, col }
    }

    #[must_use]
    pub fn path_at(&self, pos: usize) -> Option<String> {
        let inside =
            |r: &Range<usize>| r.start <= pos && (pos < r.end || r.is_empty() && pos == r.start);
        self.entries
            .iter()
            .filter_map(|e| {
                let hit = e
                    .key
                    .iter()
                    .chain(std::iter::once(&e.value))
                    .filter(|r| inside(r))
                    .map(|r| r.end.saturating_sub(r.start))
                    .min()?;
                Some((hit, e.path.len(), &e.path))
            })
            .min_by(|a, b| a.0.cmp(&b.0).then(b.1.cmp(&a.1)))
            .map(|(_, _, p)| p.clone())
    }

    #[must_use]
    pub fn key_start(&self, path: &str) -> Option<usize> {
        let e = self.entries.iter().find(|e| e.path == path)?;
        Some(e.key.as_ref().map_or(e.value.start, |k| k.start))
    }

    #[must_use]
    pub fn value_start(&self, path: &str) -> Option<usize> {
        self.entries
            .iter()
            .find(|e| e.path == path)
            .map(|e| e.value.start)
    }

    #[must_use]
    pub fn nearest(&self, path: &str) -> Option<(String, usize)> {
        let mut p = path.to_string();
        loop {
            if let Some(s) = self.key_start(&p) {
                return Some((p, s));
            }
            let cut = p.rfind(['.', '['])?;
            p.truncate(cut);
        }
    }

    #[must_use]
    pub fn guess(&self, text: &str, pos: usize) -> String {
        let token: String = text
            .get(pos..)
            .unwrap_or_default()
            .chars()
            .take_while(|c| !c.is_whitespace() && !matches!(c, '=' | ']' | '['))
            .collect();
        let token = token.trim_matches('"');
        let prev = self
            .entries
            .iter()
            .filter_map(|e| e.key.as_ref().map(|k| (k.start, &e.path)))
            .filter(|(s, _)| *s < pos)
            .max_by_key(|(s, _)| *s)
            .map(|(_, p)| parent(p));
        match (prev, token.is_empty()) {
            (_, true) => "(document)".to_string(),
            (Some(p), false) => join(&p, token),
            (None, false) => token.to_string(),
        }
    }
}

fn parent(path: &str) -> String {
    let mut quoted = false;
    let mut cut = 0;
    for (i, c) in path.char_indices() {
        match c {
            '"' => quoted = !quoted,
            '.' if !quoted => cut = i,
            _ => {}
        }
    }
    path.get(..cut).unwrap_or_default().to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    const DOC: &str = "schema = 1\n[[image]]\nid = \"A\"\nclock = [\"x\", \"y\"]\n[[image]]\nid = \"B\"\n[image.build]\noutput = 'C:\\a'\n[source.layout]\n\"a/b.h\" = \"line-keep\"\n";

    #[test]
    fn every_key_and_element_has_a_path_and_a_line() {
        let s = Spans::of(DOC);
        let at = |p: &str| s.location(DOC, s.key_start(p).unwrap()).line;
        assert_eq!(at("schema"), 1);
        assert_eq!(at("image[0].id"), 3);
        assert_eq!(at("image[0].clock"), 4);
        assert_eq!(at("image[0].clock[1]"), 4);
        assert_eq!(at("image[1].id"), 6);
        assert_eq!(at("image[1].build.output"), 8);
        assert_eq!(at("source.layout.\"a/b.h\""), 10);
        let y = DOC.find("\"y\"").unwrap();
        assert_eq!(s.path_at(y).as_deref(), Some("image[0].clock[1]"));
        assert_eq!(s.location(DOC, y), Location { line: 4, col: 15 });
        let out = DOC.find("output").unwrap();
        assert_eq!(s.path_at(out).as_deref(), Some("image[1].build.output"));
        assert_eq!(
            s.nearest("image[1].build.msdev[2].config"),
            Some(("image[1].build".to_string(), DOC.find("build").unwrap()))
        );
    }
}
