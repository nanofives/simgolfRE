use std::fmt;

pub const MAX_DEPTH: usize = 128;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Json {
    Null,
    Bool(bool),
    Num(String),
    Str(String),
    Arr(Vec<Json>),
    Obj(Vec<(String, Json)>),
}

impl Json {
    #[must_use]
    pub fn get(&self, key: &str) -> Option<&Json> {
        match self {
            Self::Obj(items) => items.iter().find(|(k, _)| k == key).map(|(_, v)| v),
            _ => None,
        }
    }

    #[must_use]
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Self::Str(s) => Some(s),
            _ => None,
        }
    }

    #[must_use]
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Self::Bool(b) => Some(*b),
            _ => None,
        }
    }

    #[must_use]
    pub fn items(&self) -> &[Json] {
        match self {
            Self::Arr(v) => v,
            _ => &[],
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct JsonError {
    pub at: usize,
    pub what: &'static str,
}

impl fmt::Display for JsonError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "character {}: {}", self.at, self.what)
    }
}

struct P {
    cs: Vec<char>,
    i: usize,
}

impl P {
    fn err(&self, what: &'static str) -> JsonError {
        JsonError { at: self.i, what }
    }

    fn peek(&self) -> Option<char> {
        self.cs.get(self.i).copied()
    }

    fn ws(&mut self) {
        while self
            .peek()
            .is_some_and(|c| matches!(c, ' ' | '\t' | '\n' | '\r'))
        {
            self.i += 1;
        }
    }

    fn eat(&mut self, c: char) -> bool {
        if self.peek() == Some(c) {
            self.i += 1;
            true
        } else {
            false
        }
    }

    fn digits(&mut self) -> usize {
        let start = self.i;
        while self.peek().is_some_and(|c| c.is_ascii_digit()) {
            self.i += 1;
        }
        self.i - start
    }

    fn word(&mut self, w: &str, v: Json) -> Result<Json, JsonError> {
        for c in w.chars() {
            if !self.eat(c) {
                return Err(self.err("an unknown literal"));
            }
        }
        Ok(v)
    }

    fn hex4(&mut self) -> Result<u32, JsonError> {
        let mut n = 0u32;
        for _ in 0..4 {
            let d = self
                .peek()
                .and_then(|c| c.to_digit(16))
                .ok_or_else(|| self.err("a \\u escape without four hex digits"))?;
            n = n * 16 + d;
            self.i += 1;
        }
        Ok(n)
    }

    fn string(&mut self) -> Result<String, JsonError> {
        if !self.eat('"') {
            return Err(self.err("a string does not open with `\"`"));
        }
        let mut out = String::new();
        loop {
            let c = self
                .peek()
                .ok_or_else(|| self.err("a string is not closed"))?;
            self.i += 1;
            match c {
                '"' => return Ok(out),
                '\\' => {
                    let e = self
                        .peek()
                        .ok_or_else(|| self.err("a string is not closed"))?;
                    self.i += 1;
                    match e {
                        '"' | '\\' | '/' => out.push(e),
                        'b' => out.push('\u{8}'),
                        'f' => out.push('\u{c}'),
                        'n' => out.push('\n'),
                        'r' => out.push('\r'),
                        't' => out.push('\t'),
                        'u' => {
                            let hi = self.hex4()?;
                            let code = if (0xD800..0xDC00).contains(&hi) {
                                if !(self.eat('\\') && self.eat('u')) {
                                    return Err(self.err("a lone high surrogate"));
                                }
                                let lo = self.hex4()?;
                                if !(0xDC00..0xE000).contains(&lo) {
                                    return Err(self.err("a high surrogate without its low half"));
                                }
                                0x10000 + ((hi - 0xD800) << 10) + (lo - 0xDC00)
                            } else {
                                hi
                            };
                            out.push(
                                char::from_u32(code)
                                    .ok_or_else(|| self.err("a \\u escape names no character"))?,
                            );
                        }
                        _ => return Err(self.err("an unknown escape")),
                    }
                }
                c if u32::from(c) < 0x20 => return Err(self.err("a control character in a string")),
                c => out.push(c),
            }
        }
    }

    fn value(&mut self, depth: usize) -> Result<Json, JsonError> {
        if depth > MAX_DEPTH {
            return Err(self.err("nested deeper than the reader follows"));
        }
        self.ws();
        match self.peek() {
            Some('{') => {
                self.i += 1;
                let mut items = Vec::new();
                self.ws();
                if self.eat('}') {
                    return Ok(Json::Obj(items));
                }
                loop {
                    self.ws();
                    let k = self.string()?;
                    self.ws();
                    if !self.eat(':') {
                        return Err(self.err("a key is not followed by `:`"));
                    }
                    let v = self.value(depth + 1)?;
                    items.push((k, v));
                    self.ws();
                    if self.eat('}') {
                        return Ok(Json::Obj(items));
                    }
                    if !self.eat(',') {
                        return Err(self.err("an object member is not followed by `,` or `}`"));
                    }
                }
            }
            Some('[') => {
                self.i += 1;
                let mut items = Vec::new();
                self.ws();
                if self.eat(']') {
                    return Ok(Json::Arr(items));
                }
                loop {
                    items.push(self.value(depth + 1)?);
                    self.ws();
                    if self.eat(']') {
                        return Ok(Json::Arr(items));
                    }
                    if !self.eat(',') {
                        return Err(self.err("an array item is not followed by `,` or `]`"));
                    }
                }
            }
            Some('"') => self.string().map(Json::Str),
            Some('t') => self.word("true", Json::Bool(true)),
            Some('f') => self.word("false", Json::Bool(false)),
            Some('n') => self.word("null", Json::Null),
            Some(c) if c == '-' || c.is_ascii_digit() => {
                let start = self.i;
                self.eat('-');
                if !self.eat('0') && self.digits() == 0 {
                    return Err(self.err("a number without an integer part"));
                }
                if self.eat('.') && self.digits() == 0 {
                    return Err(self.err("a fraction without a digit"));
                }
                if self.eat('e') || self.eat('E') {
                    if !self.eat('+') {
                        self.eat('-');
                    }
                    if self.digits() == 0 {
                        return Err(self.err("an exponent without a digit"));
                    }
                }
                Ok(Json::Num(
                    self.cs
                        .get(start..self.i)
                        .unwrap_or_default()
                        .iter()
                        .collect(),
                ))
            }
            _ => Err(self.err("no value")),
        }
    }
}

pub fn parse(text: &str) -> Result<Json, JsonError> {
    let mut p = P {
        cs: text.chars().collect(),
        i: 0,
    };
    let v = p.value(0)?;
    p.ws();
    if p.peek().is_some() {
        return Err(p.err("text after the value"));
    }
    Ok(v)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn objects_arrays_strings_and_scalars_read_back() {
        let v = parse(
            "{\"a\": [1, -2.5e3, true, false, null], \"s\": \"q\\\"\\\\\\/\\n\\u00e9\\ud83d\\ude00\", \"o\": {}}",
        )
        .unwrap();
        assert_eq!(
            v.get("a").unwrap().items(),
            [
                Json::Num("1".to_string()),
                Json::Num("-2.5e3".to_string()),
                Json::Bool(true),
                Json::Bool(false),
                Json::Null
            ]
        );
        assert_eq!(
            v.get("s").and_then(Json::as_str),
            Some("q\"\\/\n\u{e9}\u{1f600}")
        );
        assert_eq!(v.get("o"), Some(&Json::Obj(Vec::new())));
        assert_eq!(v.get("missing"), None);
        for n in ["0", "-0", "0.5", "1E+2", "2e-3", "-12.25E7", "90"] {
            assert_eq!(parse(n), Ok(Json::Num(n.to_string())), "{n}");
        }
    }

    #[test]
    fn malformed_text_is_an_error_not_a_value() {
        for bad in [
            "",
            "{",
            "{\"a\" 1}",
            "[1,]",
            "\"open",
            "\"\\ud800\"",
            "\"\\x\"",
            "tru",
            "{} x",
            "\"a\u{1}\"",
            "01",
            "1.2.3",
            "-",
            "1e",
            "--1",
            "1-2",
            "[01]",
            "1.",
            ".5",
            "1e+",
            "+1",
        ] {
            assert!(parse(bad).is_err(), "{bad:?}");
        }
        let deep = "[".repeat(MAX_DEPTH + 2) + &"]".repeat(MAX_DEPTH + 2);
        assert!(parse(&deep).is_err());
    }
}
