use crate::lex::{ASM_KEYWORDS, Asm};
use std::collections::BTreeMap;

pub const MACRO_BUDGET: usize = 100_000;

pub type Path = Vec<(usize, usize)>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Def {
    pub params: Option<Vec<String>>,
    pub body: Vec<String>,
}

#[derive(Debug, Clone)]
struct Event {
    path: Path,
    def: Option<Def>,
}

#[derive(Debug, Clone, Default)]
pub struct Table {
    events: BTreeMap<String, Vec<Event>>,
}

#[must_use]
pub fn step(s: Asm, t: &str) -> Asm {
    if s != Asm::Block && ASM_KEYWORDS.contains(&t) {
        return Asm::Pending;
    }
    match (s, t) {
        (Asm::Pending, "{") => Asm::Block,
        (Asm::Pending, _) => Asm::Line,
        (Asm::Line | Asm::Block, "}") => Asm::Off,
        _ => s,
    }
}

struct Cx<'a> {
    at: &'a [(usize, usize)],
    seen: Vec<String>,
    budget: usize,
}

fn split_args(toks: &[String], open: usize) -> Option<(Vec<Vec<String>>, usize)> {
    let mut args: Vec<Vec<String>> = vec![Vec::new()];
    let mut depth = 0usize;
    for (j, t) in toks.iter().enumerate().skip(open) {
        match t.as_str() {
            "(" if depth == 0 => {
                depth = 1;
                continue;
            }
            "(" => depth += 1,
            ")" if depth == 1 => return Some((args, j + 1)),
            ")" => depth = depth.saturating_sub(1),
            "," if depth == 1 => {
                args.push(Vec::new());
                continue;
            }
            _ => {}
        }
        if let Some(a) = args.last_mut() {
            a.push(t.clone());
        }
    }
    None
}

fn substitute(
    body: &[String],
    params: &[String],
    args: &[Vec<String>],
) -> Result<Vec<String>, String> {
    let arg = |t: &str| {
        params
            .iter()
            .position(|p| p == t)
            .map(|k| args.get(k).cloned().unwrap_or_default())
    };
    let mut out: Vec<String> = Vec::new();
    let mut glue = false;
    let mut i = 0usize;
    while let Some(t) = body.get(i) {
        i += 1;
        if t == "##" {
            glue = true;
            continue;
        }
        let piece = match body.get(i) {
            Some(n) if t == "#" && params.contains(n) => {
                i += 1;
                vec![format!("\"{}\"", arg(n).unwrap_or_default().join(" "))]
            }
            _ => arg(t).unwrap_or_else(|| vec![t.clone()]),
        };
        if glue {
            glue = false;
            let left = out.pop().unwrap_or_default();
            let mut rest = piece.into_iter();
            let joined = format!("{left}{}", rest.next().unwrap_or_default());
            if joined.contains("asm") {
                return Err(format!(
                    "`##` pastes `{joined}`: the lexer does not read an `__asm` built by token pasting"
                ));
            }
            out.push(joined);
            out.extend(rest);
        } else {
            out.extend(piece);
        }
    }
    Ok(out)
}

impl Table {
    pub fn define(&mut self, name: &str, def: Option<Def>, path: Path) {
        self.events
            .entry(name.to_string())
            .or_default()
            .push(Event { path, def });
    }

    #[must_use]
    pub fn is_macro(&self, name: &str) -> bool {
        self.events.contains_key(name)
    }

    fn readings(&self, name: &str, at: &[(usize, usize)]) -> Vec<Option<&Def>> {
        let mut out: Vec<Option<&Def>> = Vec::new();
        for e in self.events.get(name).into_iter().flatten().rev() {
            let apart = e
                .path
                .iter()
                .any(|(g, b)| at.iter().any(|(h, c)| g == h && b != c));
            if apart {
                continue;
            }
            let d = e.def.as_ref();
            if !out.contains(&d) {
                out.push(d);
            }
            if e.path.iter().all(|x| at.contains(x)) {
                return out;
            }
        }
        if !out.contains(&None) {
            out.push(None);
        }
        out
    }

    #[must_use]
    pub fn takes_arguments(&self, name: &str, at: &[(usize, usize)]) -> bool {
        self.readings(name, at)
            .iter()
            .any(|r| r.is_some_and(|d| d.params.is_some()))
    }

    pub fn run(&self, toks: &[String], s: Asm, at: &[(usize, usize)]) -> Result<Asm, String> {
        let mut cx = Cx {
            at,
            seen: Vec::new(),
            budget: MACRO_BUDGET,
        };
        self.go(toks, s, &mut cx)
    }

    fn go(&self, toks: &[String], mut s: Asm, cx: &mut Cx<'_>) -> Result<Asm, String> {
        let mut i = 0usize;
        while let Some(t) = toks.get(i) {
            cx.budget = cx
                .budget
                .checked_sub(1)
                .ok_or_else(|| format!("a macro expansion runs past {MACRO_BUDGET} tokens"))?;
            if cx.seen.contains(t) || !self.is_macro(t) {
                s = step(s, t);
                i += 1;
                continue;
            }
            let readings = self.readings(t, cx.at);
            if let [one] = readings.as_slice() {
                (s, i) = self.apply(t, *one, toks, i, s, cx)?;
                continue;
            }
            let mut finals: Vec<Asm> = Vec::new();
            for r in &readings {
                let (mid, next) = self.apply(t, *r, toks, i, s, cx)?;
                let f = self.go(toks.get(next..).unwrap_or_default(), mid, cx)?;
                if !finals.contains(&f) {
                    finals.push(f);
                }
            }
            return match finals.as_slice() {
                [f] => Ok(*f),
                _ => Err(format!(
                    "`{t}` is defined or undefined in branches the macros decide, and its readings leave `__asm` \
                     in different states"
                )),
            };
        }
        Ok(s)
    }

    fn apply(
        &self,
        t: &str,
        r: Option<&Def>,
        toks: &[String],
        i: usize,
        s: Asm,
        cx: &mut Cx<'_>,
    ) -> Result<(Asm, usize), String> {
        let Some(d) = r else {
            return Ok((step(s, t), i + 1));
        };
        let Some(params) = &d.params else {
            let body = substitute(&d.body, &[], &[])?;
            return Ok((self.nest(t, &body, s, cx)?, i + 1));
        };
        match toks.get(i + 1).map(String::as_str) {
            Some("(") => {
                let (args, next) = split_args(toks, i + 1)
                    .ok_or_else(|| format!("the call of `{t}` has no closing parenthesis"))?;
                let body = substitute(&d.body, params, &args)?;
                Ok((self.nest(t, &body, s, cx)?, next))
            }
            None => {
                let body = substitute(&d.body, params, &[])?;
                if self.nest(t, &body, s, cx)? != step(s, t) {
                    return Err(format!(
                        "`{t}` takes arguments, moves `__asm` when called, and is used with no `(` after it: whether \
                         it is called depends on text the lexer does not follow"
                    ));
                }
                Ok((step(s, t), i + 1))
            }
            Some(_) => Ok((step(s, t), i + 1)),
        }
    }

    fn nest(&self, name: &str, body: &[String], s: Asm, cx: &mut Cx<'_>) -> Result<Asm, String> {
        cx.seen.push(name.to_string());
        let r = self.go(body, s, cx);
        cx.seen.pop();
        r
    }
}
