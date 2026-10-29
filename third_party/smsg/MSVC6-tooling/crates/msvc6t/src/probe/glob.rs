use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq, Eq)]
enum Tok {
    Lit(char),
    Star,
    Cap(String),
}

pub type Caps = BTreeMap<String, String>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Pat {
    pub neg: bool,
    pub text: String,
    pub line: usize,
}

#[must_use]
pub fn collapse(s: &str) -> String {
    s.split_whitespace().collect::<Vec<_>>().join(" ")
}

fn tokens(p: &str) -> Vec<Tok> {
    let chars: Vec<char> = p.chars().collect();
    let mut out = Vec::new();
    let mut i = 0usize;
    while let Some(&c) = chars.get(i) {
        i = i.saturating_add(1);
        match c {
            '*' => out.push(Tok::Star),
            '{' => {
                let rest = chars.get(i..).unwrap_or_default();
                let name: String = rest.iter().take_while(|&&d| d != '}').collect();
                let closed = rest.get(name.chars().count()) == Some(&'}');
                let ident =
                    !name.is_empty() && name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_');
                if closed && ident {
                    i = i.saturating_add(name.chars().count()).saturating_add(1);
                    out.push(Tok::Cap(name));
                } else {
                    out.push(Tok::Lit('{'));
                }
            }
            c => out.push(Tok::Lit(c)),
        }
    }
    out
}

fn cap_char(c: char) -> bool {
    c.is_ascii_alphanumeric() || matches!(c, '_' | '$' | '@' | '?' | '.')
}

fn glob(p: &[Tok], s: &[char], caps: &mut Caps) -> bool {
    let Some((first, rest)) = p.split_first() else {
        return s.is_empty();
    };
    match first {
        Tok::Lit(c) => match s.split_first() {
            Some((h, tail)) if h == c => glob(rest, tail, caps),
            _ => false,
        },
        Tok::Star => (0..=s.len()).any(|k| glob(rest, s.get(k..).unwrap_or_default(), caps)),
        Tok::Cap(name) => {
            if let Some(v) = caps.get(name).cloned() {
                let v: Vec<char> = v.chars().collect();
                return s.starts_with(&v) && glob(rest, s.get(v.len()..).unwrap_or_default(), caps);
            }
            let run = s.iter().take_while(|c| cap_char(**c)).count();
            for k in (1..=run).rev() {
                let mut trial = caps.clone();
                trial.insert(
                    name.clone(),
                    s.get(..k).unwrap_or_default().iter().collect(),
                );
                if glob(rest, s.get(k..).unwrap_or_default(), &mut trial) {
                    *caps = trial;
                    return true;
                }
            }
            false
        }
    }
}

pub fn matches(pat: &str, line: &str, caps: &mut Caps) -> bool {
    let p = tokens(pat);
    let s: Vec<char> = line.chars().collect();
    let mut trial = caps.clone();
    if glob(&p, &s, &mut trial) {
        *caps = trial;
        true
    } else {
        false
    }
}

fn line_at(lines: &[String], i: usize) -> &str {
    lines.get(i).map_or("", String::as_str)
}

fn forbidden(n: &Pat, lines: &[String], from: usize, to: usize, caps: &Caps) -> Option<String> {
    (from..to)
        .find(|&j| matches(&n.text, line_at(lines, j), &mut caps.clone()))
        .map(|j| {
            format!(
                "expect line {}: `!{}` matched line {}: `{}`",
                n.line,
                n.text,
                j.saturating_add(1),
                line_at(lines, j)
            )
        })
}

pub fn check(pats: &[Pat], lines: &[String], exact: bool) -> Result<(), String> {
    let mut caps = Caps::new();
    if exact {
        for (i, p) in pats.iter().enumerate() {
            let Some(l) = lines.get(i) else {
                return Err(format!(
                    "expect line {}: `{}`: the view ended after {} line(s)",
                    p.line,
                    p.text,
                    lines.len()
                ));
            };
            if !matches(&p.text, l, &mut caps) {
                return Err(format!(
                    "expect line {}: `{}` against line {}: `{l}`",
                    p.line,
                    p.text,
                    i.saturating_add(1)
                ));
            }
        }
        if let Some(extra) = lines.get(pats.len()) {
            return Err(format!(
                "the view has {} more line(s) after the last pattern, the first `{extra}`",
                lines.len().saturating_sub(pats.len())
            ));
        }
        return Ok(());
    }
    let mut pos = 0usize;
    let mut pending: Vec<&Pat> = Vec::new();
    for p in pats {
        if p.neg {
            pending.push(p);
            continue;
        }
        let found =
            (pos..lines.len()).find(|&i| matches(&p.text, line_at(lines, i), &mut caps.clone()));
        let Some(i) = found else {
            let next: Vec<&str> = lines
                .iter()
                .skip(pos)
                .take(12)
                .map(String::as_str)
                .collect();
            return Err(format!(
                "expect line {}: `{}`: no match after line {pos}; the next lines:\n      {}",
                p.line,
                p.text,
                next.join("\n      ")
            ));
        };
        matches(&p.text, line_at(lines, i), &mut caps);
        for n in pending.drain(..) {
            if let Some(e) = forbidden(n, lines, pos, i, &caps) {
                return Err(e);
            }
        }
        pos = i.saturating_add(1);
    }
    for n in pending {
        if let Some(e) = forbidden(n, lines, pos, lines.len(), &caps) {
            return Err(e);
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pats(spec: &[&str]) -> Vec<Pat> {
        spec.iter()
            .enumerate()
            .map(|(i, s)| match s.strip_prefix('!') {
                Some(r) => Pat {
                    neg: true,
                    text: r.to_string(),
                    line: i + 1,
                },
                None => Pat {
                    neg: false,
                    text: (*s).to_string(),
                    line: i + 1,
                },
            })
            .collect()
    }

    fn lines(spec: &[&str]) -> Vec<String> {
        spec.iter().map(|s| (*s).to_string()).collect()
    }

    #[test]
    fn a_star_matches_any_run_and_a_capture_binds_once() {
        let mut c = Caps::new();
        assert!(matches(
            "mov {r}, DWORD PTR *",
            "mov esi, DWORD PTR _a$[esp]",
            &mut c
        ));
        assert_eq!(c.get("r").map(String::as_str), Some("esi"));
        assert!(matches("push {r}", "push esi", &mut c));
        assert!(!matches("push {r}", "push edi", &mut c));
        assert!(matches("a{b", "a{b", &mut Caps::new()));
        assert!(matches("x {not a cap}", "x {not a cap}", &mut Caps::new()));
        assert!(matches(
            "*; switch(v) {*",
            "0a53 3b switch ; switch(v) { (body to endlist)",
            &mut Caps::new()
        ));
    }

    #[test]
    fn positive_patterns_match_in_order_and_negatives_forbid_between() {
        let l = lines(&["push ebp", "mov ebp, esp", "call _g", "pop ebp", "ret 0"]);
        assert!(check(&pats(&["push ebp", "call _g", "ret 0"]), &l, false).is_ok());
        assert!(check(&pats(&["call _g", "push ebp"]), &l, false).is_err());
        assert!(check(&pats(&["push ebp", "!call *", "pop ebp"]), &l, false).is_err());
        assert!(check(&pats(&["call _g", "!call *"]), &l, false).is_ok());
        assert!(check(&pats(&["!jmp *"]), &l, false).is_ok());
    }

    #[test]
    fn an_exact_block_matches_every_line() {
        let l = lines(&["_a$ = -4", "_b$ = -8"]);
        assert!(check(&pats(&["_a$ = -4", "_b$ = -8"]), &l, true).is_ok());
        assert!(check(&pats(&["_a$ = -4"]), &l, true).is_err());
        assert!(check(&pats(&["_b$ = -8", "_a$ = -4"]), &l, true).is_err());
        assert!(check(&pats(&["_a$ = -4", "_b$ = -8", "x"]), &l, true).is_err());
    }

    #[test]
    fn whitespace_runs_collapse() {
        assert_eq!(collapse("  mov\teax,   1 "), "mov eax, 1");
    }
}
