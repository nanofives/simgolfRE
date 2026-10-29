use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Value {
    Str(String),
    Bool(bool),
    Scalar(String),
    Array(Vec<Value>),
    Table(Vec<(Vec<String>, Value)>),
}

impl Value {
    #[must_use]
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Self::Str(s) => Some(s),
            _ => None,
        }
    }

    #[must_use]
    pub fn field(&self, name: &str) -> Option<&Value> {
        match self {
            Self::Table(items) => items
                .iter()
                .find(|(k, _)| k.len() == 1 && k.first().is_some_and(|f| f == name))
                .map(|(_, v)| v),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    pub key: Vec<String>,
    pub value: Value,
    pub line: usize,
    pub header: bool,
}

impl Entry {
    #[must_use]
    pub fn dotted(&self) -> String {
        self.key.join(".")
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TomlError {
    pub line: usize,
    pub col: usize,
    pub what: &'static str,
}

impl fmt::Display for TomlError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}: {}", self.line, self.col, self.what)
    }
}

type Result<T> = std::result::Result<T, TomlError>;

struct P {
    cs: Vec<char>,
    starts: Vec<usize>,
    i: usize,
    comments: Vec<(usize, usize)>,
}

fn is_bare(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_' || c == '-'
}

fn is_scalar(c: char) -> bool {
    c.is_ascii_alphanumeric() || matches!(c, '_' | '-' | '+' | '.' | ':')
}

impl P {
    fn new(text: &str) -> Self {
        let cs: Vec<char> = text.chars().collect();
        let mut starts = vec![0];
        starts.extend(
            cs.iter()
                .enumerate()
                .filter(|(_, c)| **c == '\n')
                .map(|(k, _)| k + 1),
        );
        Self {
            cs,
            starts,
            i: 0,
            comments: Vec::new(),
        }
    }

    fn peek(&self) -> Option<char> {
        self.cs.get(self.i).copied()
    }

    fn peek_at(&self, k: usize) -> Option<char> {
        self.cs.get(self.i + k).copied()
    }

    fn starts_with(&self, s: &str) -> bool {
        s.chars()
            .enumerate()
            .all(|(k, c)| self.peek_at(k) == Some(c))
    }

    fn pos(&self) -> (usize, usize) {
        let line = self.starts.partition_point(|&s| s <= self.i);
        let start = self
            .starts
            .get(line.saturating_sub(1))
            .copied()
            .unwrap_or(0);
        (line, self.i.saturating_sub(start) + 1)
    }

    fn err(&self, what: &'static str) -> TomlError {
        let (line, col) = self.pos();
        TomlError { line, col, what }
    }

    fn eat(&mut self, c: char) -> bool {
        let hit = self.peek() == Some(c);
        if hit {
            self.i += 1;
        }
        hit
    }

    fn ws(&mut self) {
        while matches!(self.peek(), Some(' ' | '\t')) {
            self.i += 1;
        }
    }

    fn comment(&mut self) {
        if self.peek() == Some('#') {
            self.comments.push(self.pos());
            while self.peek().is_some_and(|c| c != '\n') {
                self.i += 1;
            }
        }
    }

    fn blank(&mut self) {
        loop {
            self.ws();
            self.comment();
            match self.peek() {
                Some('\n' | '\r') => self.i += 1,
                _ => return,
            }
        }
    }

    fn line_end(&mut self) -> Result<()> {
        self.ws();
        self.comment();
        if self.starts_with("\r\n") {
            self.i += 2;
            return Ok(());
        }
        match self.peek() {
            None => Ok(()),
            Some('\n') => {
                self.i += 1;
                Ok(())
            }
            Some(_) => Err(self.err("more on the line than one key and its value")),
        }
    }

    fn hex(&mut self, n: usize) -> Result<char> {
        let digits: String = self
            .cs
            .get(self.i..self.i + n)
            .unwrap_or_default()
            .iter()
            .collect();
        self.i += n;
        u32::from_str_radix(&digits, 16)
            .ok()
            .filter(|_| digits.chars().count() == n)
            .and_then(char::from_u32)
            .ok_or_else(|| self.err("a bad unicode escape"))
    }

    fn escape(&mut self, out: &mut String, multiline: bool) -> Result<()> {
        self.i += 1;
        let Some(c) = self.peek() else {
            return Err(self.err("an unterminated string"));
        };
        self.i += 1;
        match c {
            'b' => out.push('\u{8}'),
            't' => out.push('\t'),
            'n' => out.push('\n'),
            'f' => out.push('\u{c}'),
            'r' => out.push('\r'),
            'e' => out.push('\u{1b}'),
            '"' => out.push('"'),
            '\\' => out.push('\\'),
            'x' => out.push(self.hex(2)?),
            'u' => out.push(self.hex(4)?),
            'U' => out.push(self.hex(8)?),
            ' ' | '\t' | '\r' | '\n' if multiline => {
                self.i -= 1;
                while matches!(self.peek(), Some(' ' | '\t' | '\r' | '\n')) {
                    self.i += 1;
                }
            }
            _ => return Err(self.err("an escape TOML does not know")),
        }
        Ok(())
    }

    fn quoted(&mut self, quote: char) -> Result<String> {
        let fence: String = std::iter::repeat_n(quote, 3).collect();
        let multiline = self.starts_with(&fence);
        self.i += if multiline { 3 } else { 1 };
        if multiline {
            if self.starts_with("\r\n") {
                self.i += 2;
            } else if self.peek() == Some('\n') {
                self.i += 1;
            }
        }
        let mut out = String::new();
        loop {
            match self.peek() {
                None => return Err(self.err("an unterminated string")),
                Some('\n') if !multiline => return Err(self.err("an unterminated string")),
                Some('\\') if quote == '"' => self.escape(&mut out, multiline)?,
                Some(c) if c == quote && !multiline => {
                    self.i += 1;
                    return Ok(out);
                }
                Some(c) if c == quote && self.starts_with(&fence) => {
                    let mut run = 0;
                    while self.peek() == Some(quote) && run < 5 {
                        self.i += 1;
                        run += 1;
                    }
                    out.extend(std::iter::repeat_n(quote, run - 3));
                    return Ok(out);
                }
                Some(c) => {
                    out.push(c);
                    self.i += 1;
                }
            }
        }
    }

    fn simple_key(&mut self) -> Result<String> {
        match self.peek() {
            Some(q @ ('"' | '\'')) => {
                if self.starts_with(&std::iter::repeat_n(q, 3).collect::<String>()) {
                    return Err(self.err("a multi-line string as a key"));
                }
                self.quoted(q)
            }
            Some(c) if is_bare(c) => {
                let mut s = String::new();
                while let Some(c) = self.peek().filter(|&c| is_bare(c)) {
                    s.push(c);
                    self.i += 1;
                }
                Ok(s)
            }
            _ => Err(self.err("a key is missing")),
        }
    }

    fn key(&mut self) -> Result<Vec<String>> {
        let mut parts = vec![self.simple_key()?];
        loop {
            let save = self.i;
            self.ws();
            if self.eat('.') {
                self.ws();
                parts.push(self.simple_key()?);
            } else {
                self.i = save;
                return Ok(parts);
            }
        }
    }

    fn array(&mut self) -> Result<Value> {
        self.i += 1;
        let mut items = Vec::new();
        loop {
            self.blank();
            if self.eat(']') {
                return Ok(Value::Array(items));
            }
            items.push(self.value()?);
            self.blank();
            if self.eat(']') {
                return Ok(Value::Array(items));
            }
            if !self.eat(',') {
                return Err(self.err("an array item is not followed by `,` or `]`"));
            }
        }
    }

    fn inline_table(&mut self) -> Result<Value> {
        self.i += 1;
        let mut items = Vec::new();
        loop {
            self.blank();
            if self.eat('}') {
                return Ok(Value::Table(items));
            }
            let k = self.key()?;
            self.ws();
            if !self.eat('=') {
                return Err(self.err("a key is not followed by `=`"));
            }
            self.ws();
            items.push((k, self.value()?));
            self.blank();
            if self.eat('}') {
                return Ok(Value::Table(items));
            }
            if !self.eat(',') {
                return Err(self.err("an inline table item is not followed by `,` or `}`"));
            }
        }
    }

    fn value(&mut self) -> Result<Value> {
        match self.peek() {
            Some(q @ ('"' | '\'')) => self.quoted(q).map(Value::Str),
            Some('[') => self.array(),
            Some('{') => self.inline_table(),
            Some(c) if is_scalar(c) => {
                let mut s = String::new();
                while let Some(c) = self.peek().filter(|&c| is_scalar(c)) {
                    s.push(c);
                    self.i += 1;
                }
                Ok(match s.as_str() {
                    "true" => Value::Bool(true),
                    "false" => Value::Bool(false),
                    _ => Value::Scalar(s),
                })
            }
            _ => Err(self.err("a value is missing")),
        }
    }
}

fn push_flat(out: &mut Vec<Entry>, key: Vec<String>, value: Value, line: usize) {
    if let Value::Table(items) = &value {
        for (k, v) in items {
            let mut full = key.clone();
            full.extend(k.iter().cloned());
            push_flat(out, full, v.clone(), line);
        }
    }
    out.push(Entry {
        key,
        value,
        line,
        header: false,
    });
}

pub fn parse(text: &str) -> Result<Vec<Entry>> {
    let mut p = P::new(text);
    let mut prefix: Vec<String> = Vec::new();
    let mut out = Vec::new();
    loop {
        p.blank();
        let Some(c) = p.peek() else {
            return Ok(out);
        };
        let line = p.pos().0;
        if c == '[' {
            let array = p.peek_at(1) == Some('[');
            p.i += if array { 2 } else { 1 };
            p.ws();
            let k = p.key()?;
            p.ws();
            if !p.eat(']') || (array && !p.eat(']')) {
                return Err(p.err("a table header is not closed"));
            }
            prefix.clone_from(&k);
            out.push(Entry {
                key: k,
                value: Value::Table(Vec::new()),
                line,
                header: true,
            });
        } else {
            let k = p.key()?;
            p.ws();
            if !p.eat('=') {
                return Err(p.err("a key is not followed by `=`"));
            }
            p.ws();
            let v = p.value()?;
            let mut full = prefix.clone();
            full.extend(k);
            push_flat(&mut out, full, v, line);
        }
        p.line_end()?;
    }
}

pub fn comments(text: &str) -> Result<Vec<(usize, usize)>> {
    let mut p = P::new(text);
    while let Some(c) = p.peek() {
        match c {
            '#' => p.comment(),
            '"' | '\'' => {
                p.quoted(c)?;
            }
            _ => p.i += 1,
        }
    }
    Ok(p.comments)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn keys(text: &str) -> Vec<String> {
        parse(text)
            .unwrap()
            .iter()
            .filter(|e| !e.header)
            .map(Entry::dotted)
            .collect()
    }

    #[test]
    fn a_comment_is_a_hash_outside_every_string() {
        let t = "a = \"#x\" # one\nb = '#' \nc = \"\"\"\n# not\n\"\"\"\n# two\nd = ['''#''']\n";
        assert_eq!(comments(t).unwrap(), [(1, 10), (6, 1)]);
        assert!(comments("x = \"a\\\"#\"\n").unwrap().is_empty());
        assert_eq!(
            comments("x = \"open\n").unwrap_err().what,
            "an unterminated string"
        );
    }

    #[test]
    fn keys_are_decoded_dotted_and_prefixed_by_their_table() {
        let t = "[workspace.lints.clippy]\nunwrap_used = \"deny\"\n\"disallowed_methods\" = \"allow\"\n\
                 [package]\nversion.workspace = true\nlints = { clippy = { all = \"allow\" } }\n\
                 'quoted.dot' = 1\n\"\\u0069gnore\" = true\n";
        assert_eq!(
            keys(t),
            [
                "workspace.lints.clippy.unwrap_used",
                "workspace.lints.clippy.disallowed_methods",
                "package.version.workspace",
                "package.lints.clippy.all",
                "package.lints.clippy",
                "package.lints",
                "package.quoted.dot",
                "package.ignore"
            ]
        );
    }

    #[test]
    fn values_hold_arrays_of_inline_tables_over_lines() {
        let t = "disallowed-methods = [\n    { path = \"std::fs::write\", reason = \"r\" },\n    \"std::str::from_utf8\",\n]\n";
        let e = parse(t).unwrap();
        let Value::Array(items) = &e[0].value else {
            panic!("{e:?}")
        };
        assert_eq!(
            items[0].field("path").and_then(Value::as_str),
            Some("std::fs::write")
        );
        assert_eq!(items[1].as_str(), Some("std::str::from_utf8"));
        assert_eq!(e[0].line, 1);
    }

    #[test]
    fn a_shape_it_cannot_read_is_an_error() {
        assert!(parse("a = 1 b = 2\n").is_err());
        assert!(parse("[a\nb = 1\n").is_err());
        assert!(parse("a = [1 2]\n").is_err());
        assert!(parse("= 1\n").is_err());
        assert!(parse("a = \"\\q\"\n").is_err());
        assert_eq!(
            parse("s = \"\"\"\nx\"\"\"\"\n").unwrap()[0].value,
            Value::Str("x\"".to_string())
        );
    }
}
