use crate::defs::Def;
use crate::flow::{self, Tok};
use crate::lex::{Kind, Lexed};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, PartialEq, Eq)]
struct Stmt {
    block: usize,
    index: usize,
    from: usize,
    to: usize,
    line: usize,
    loop_body: bool,
}

struct Cutter<'a> {
    toks: &'a [Tok],
    i: usize,
    out: Vec<Stmt>,
    blocks: usize,
    loop_blocks: BTreeSet<usize>,
    tails: Vec<(usize, usize, usize)>,
}

fn group_end(toks: &[Tok], open: usize) -> usize {
    let mut depth = 0i32;
    for (k, t) in toks.iter().enumerate().skip(open) {
        if t.kind == Kind::Punct {
            match t.text.as_str() {
                "(" | "[" | "{" => depth += 1,
                ")" | "]" | "}" => {
                    depth -= 1;
                    if depth == 0 {
                        return k;
                    }
                }
                _ => {}
            }
        }
    }
    toks.len()
}

impl Cutter<'_> {
    fn new_block(&mut self, is_loop: bool) -> usize {
        self.blocks += 1;
        if is_loop {
            self.loop_blocks.insert(self.blocks);
        }
        self.blocks
    }

    fn push(&mut self, block: usize, index: &mut usize, from: usize, to: usize) {
        let line = self.toks.get(from).map_or(0, |t| t.line);
        self.out.push(Stmt {
            block,
            index: *index,
            from,
            to,
            line,
            loop_body: self.loop_blocks.contains(&block),
        });
        *index += 1;
    }

    fn arm(&mut self, is_loop: bool) -> usize {
        let b = self.new_block(is_loop);
        let mut idx = 0usize;
        if self.toks.get(self.i).is_some_and(|t| t.punct_is("{")) {
            self.i += 1;
            while self.i < self.toks.len()
                && !self.toks.get(self.i).is_some_and(|t| t.punct_is("}"))
            {
                let before = self.i;
                self.stmt(b, &mut idx);
                if self.i == before {
                    self.i += 1;
                }
            }
            self.i += 1;
        } else {
            self.stmt(b, &mut idx);
        }
        b
    }

    fn stmt(&mut self, block: usize, index: &mut usize) {
        let Some(t) = self.toks.get(self.i) else {
            return;
        };
        let kw = if t.kind == Kind::Ident {
            t.text.as_str()
        } else {
            ""
        };
        match kw {
            "if" | "while" | "switch" => {
                let open = self.i + 1;
                let close = group_end(self.toks, open);
                self.push(block, index, open, close + 1);
                self.i = close + 1;
                let is_loop = kw == "while";
                let b = self.arm(is_loop);
                let _ = b;
                if kw == "if" && self.toks.get(self.i).is_some_and(|e| e.ident_is("else")) {
                    self.i += 1;
                    self.arm(false);
                }
            }
            "for" => {
                let open = self.i + 1;
                let close = group_end(self.toks, open);
                let mut semis = Vec::new();
                let mut depth = 0i32;
                for k in open + 1..close {
                    if let Some(x) = self.toks.get(k) {
                        if x.punct_is("(") {
                            depth += 1;
                        } else if x.punct_is(")") {
                            depth -= 1;
                        } else if x.punct_is(";") && depth == 0 {
                            semis.push(k);
                        }
                    }
                }
                let (s1, s2) = (
                    semis.first().copied().unwrap_or(close),
                    semis.get(1).copied().unwrap_or(close),
                );
                self.push(block, index, open + 1, s1);
                self.i = close + 1;
                let b = self.arm(true);
                let body_len = self.out.iter().filter(|s| s.block == b).count();
                self.tails.push((b, body_len, s2 + 1));
                let line = self.toks.get(s2 + 1).map_or(0, |x| x.line);
                self.out.push(Stmt {
                    block: b,
                    index: body_len,
                    from: s2 + 1,
                    to: close,
                    line,
                    loop_body: true,
                });
            }
            "do" => {
                self.i += 1;
                let b = self.arm(true);
                if self.toks.get(self.i).is_some_and(|w| w.ident_is("while")) {
                    let open = self.i + 1;
                    let close = group_end(self.toks, open);
                    let body_len = self.out.iter().filter(|s| s.block == b).count();
                    let line = self.toks.get(open).map_or(0, |x| x.line);
                    self.out.push(Stmt {
                        block: b,
                        index: body_len,
                        from: open,
                        to: close + 1,
                        line,
                        loop_body: true,
                    });
                    self.i = close + 1;
                    if self.toks.get(self.i).is_some_and(|s| s.punct_is(";")) {
                        self.i += 1;
                    }
                }
            }
            "else" => {
                self.i += 1;
            }
            _ if t.punct_is("{") => {
                let _ = self.arm(false);
            }
            _ => {
                let from = self.i;
                let mut depth = 0i32;
                while let Some(x) = self.toks.get(self.i) {
                    self.i += 1;
                    if x.kind == Kind::Punct {
                        match x.text.as_str() {
                            "(" | "[" | "{" => depth += 1,
                            ")" | "]" => depth -= 1,
                            "}" => {
                                depth -= 1;
                                if depth < 0 {
                                    self.i -= 1;
                                    break;
                                }
                            }
                            ";" | ":" if depth == 0 => break,
                            _ => {}
                        }
                    }
                }
                self.push(block, index, from, self.i);
            }
        }
    }
}

trait TokIs {
    fn punct_is(&self, s: &str) -> bool;
    fn ident_is(&self, s: &str) -> bool;
}

impl TokIs for Tok {
    fn punct_is(&self, s: &str) -> bool {
        self.kind == Kind::Punct && self.text == s
    }

    fn ident_is(&self, s: &str) -> bool {
        self.kind == Kind::Ident && self.text == s
    }
}

#[derive(Debug, Clone, PartialEq)]
enum W {
    Init(BTreeSet<String>),
    Step(Option<f64>),
    Other,
}

fn number(t: &Tok) -> Option<f64> {
    if t.kind != Kind::Number {
        return None;
    }
    let v = crate::lex::number_value(&t.text)?;
    i32::try_from(v).ok().map(f64::from)
}

fn writes_in(toks: &[Tok], s: &Stmt, locals: &BTreeSet<String>) -> Vec<(String, W)> {
    let st = toks.get(s.from..s.to).unwrap_or_default();
    let mut out = Vec::new();
    for (k, t) in st.iter().enumerate() {
        if t.kind != Kind::Ident || !locals.contains(&t.text) {
            continue;
        }
        let prev = k.checked_sub(1).and_then(|p| st.get(p));
        if prev.is_some_and(|p| p.punct_is(".") || p.punct_is("->") || p.punct_is("::")) {
            continue;
        }
        let next = st.get(k + 1);
        let incdec = |x: Option<&Tok>| x.is_some_and(|x| x.punct_is("++") || x.punct_is("--"));
        if incdec(next) || incdec(prev) {
            let neg =
                next.is_some_and(|x| x.punct_is("--")) || prev.is_some_and(|x| x.punct_is("--"));
            out.push((t.text.clone(), W::Step(Some(if neg { -1.0 } else { 1.0 }))));
            continue;
        }
        let Some(op) = next.filter(|n| n.kind == Kind::Punct) else {
            continue;
        };
        let rhs_end = st
            .iter()
            .enumerate()
            .skip(k + 2)
            .find(|(_, x)| x.punct_is(",") || x.punct_is(";") || x.punct_is(")"))
            .map_or(st.len(), |(j, _)| j);
        let rhs = st.get(k + 2..rhs_end).unwrap_or_default();
        match op.text.as_str() {
            "+=" | "-=" => {
                let sign = if op.text == "-=" { -1.0 } else { 1.0 };
                let amount = match rhs {
                    [n] => number(n).map(|v| v * sign),
                    _ => None,
                };
                let w = if rhs.iter().any(|x| x.text == t.text) {
                    W::Other
                } else {
                    W::Step(amount)
                };
                out.push((t.text.clone(), w));
            }
            "=" => {
                let mentions_self = rhs
                    .iter()
                    .any(|x| x.kind == Kind::Ident && x.text == t.text);
                let w = match rhs {
                    [a, p, n] if a.text == t.text && (p.punct_is("+") || p.punct_is("-")) => {
                        let sign = if p.punct_is("-") { -1.0 } else { 1.0 };
                        W::Step(number(n).map(|v| v * sign))
                    }
                    [n, p, a] if a.text == t.text && p.punct_is("+") => W::Step(number(n)),
                    _ if mentions_self => W::Other,
                    _ => {
                        let used: BTreeSet<String> = rhs
                            .iter()
                            .filter(|x| x.kind == Kind::Ident && locals.contains(&x.text))
                            .map(|x| x.text.clone())
                            .collect();
                        W::Init(used)
                    }
                };
                out.push((t.text.clone(), w));
            }
            s if flow::COMPOUND.contains(&s) => out.push((t.text.clone(), W::Other)),
            _ => {}
        }
    }
    out
}

#[derive(Debug, Clone, PartialEq, Serialize)]
pub struct Mirror {
    pub kind: String,
    pub var: String,
    pub partner: String,
    pub ratio: Option<f64>,
    pub strict: bool,
    pub steps: usize,
    pub lines: Vec<usize>,
}

pub fn screen(lx: &Lexed, def: &Def, window: usize) -> Result<Vec<Mirror>, String> {
    let fl = flow::analyse(
        lx,
        def,
        &flow::Opts {
            short_circuit: true,
            ..flow::Opts::default()
        },
    )
    .map_err(|p| p.0)?;
    let locals: BTreeSet<String> = fl
        .decls
        .declarators
        .iter()
        .map(|d| d.name.clone())
        .filter(|n| !fl.escaped.contains(n))
        .collect();
    let toks = flow::tokens(lx, def.open + 1, def.close.saturating_sub(1));
    let mut c = Cutter {
        toks: &toks,
        i: 0,
        out: Vec::new(),
        blocks: 0,
        loop_blocks: BTreeSet::new(),
        tails: Vec::new(),
    };
    let mut idx = 0usize;
    while c.i < toks.len() {
        let before = c.i;
        c.stmt(0, &mut idx);
        if c.i == before {
            c.i += 1;
        }
    }
    let stmts = c.out;
    let mut writes: BTreeMap<String, Vec<(usize, W)>> = BTreeMap::new();
    for (si, s) in stmts.iter().enumerate() {
        for (v, w) in writes_in(&toks, s, &locals) {
            writes.entry(v).or_default().push((si, w));
        }
    }
    let steps_of = |v: &str| -> Vec<(usize, Option<f64>)> {
        writes
            .get(v)
            .into_iter()
            .flatten()
            .filter_map(|(si, w)| match w {
                W::Step(a) => Some((*si, *a)),
                _ => None,
            })
            .collect()
    };
    let near = |a: usize, b: usize| -> bool {
        match (stmts.get(a), stmts.get(b)) {
            (Some(x), Some(y)) => x.block == y.block && x.index.abs_diff(y.index) <= window,
            _ => false,
        }
    };
    let mut out = Vec::new();
    for (v, ws) in &writes {
        let all_init_step = ws.iter().all(|(_, w)| matches!(w, W::Init(_) | W::Step(_)));
        let my_steps = steps_of(v);
        if all_init_step && !my_steps.is_empty() {
            let mut partner: Option<String> = None;
            let mut ok = true;
            for (si, _) in &my_steps {
                let cands: BTreeSet<String> = writes
                    .iter()
                    .filter(|(o, _)| *o != v)
                    .filter(|(o, _)| steps_of(o).iter().any(|(sj, _)| near(*si, *sj)))
                    .map(|(o, _)| o.clone())
                    .collect();
                match &partner {
                    None => {
                        if let Some(p) = cands.iter().next() {
                            partner = Some(p.clone());
                        } else {
                            ok = false;
                            break;
                        }
                    }
                    Some(p) if cands.contains(p) => {}
                    Some(_) => {
                        ok = false;
                        break;
                    }
                }
            }
            if let (true, Some(p)) = (ok, partner) {
                let theirs = steps_of(&p);
                let strict = theirs
                    .iter()
                    .all(|(sj, _)| my_steps.iter().any(|(si, _)| near(*si, *sj)));
                let mut ratios: Vec<Option<f64>> = Vec::new();
                for (si, a) in &my_steps {
                    if let Some((_, b)) = theirs.iter().find(|(sj, _)| near(*si, *sj)) {
                        ratios.push(match (a, b) {
                            (Some(x), Some(y)) if *y != 0.0 => Some(x / y),
                            _ => None,
                        });
                    }
                }
                let ratio = match ratios.first() {
                    Some(Some(r)) if ratios.iter().all(|x| *x == Some(*r)) => Some(*r),
                    _ => None,
                };
                out.push(Mirror {
                    kind: "lockstep".to_string(),
                    var: v.clone(),
                    partner: p,
                    ratio,
                    strict,
                    steps: my_steps.len(),
                    lines: my_steps
                        .iter()
                        .filter_map(|(si, _)| stmts.get(*si).map(|s| s.line))
                        .collect(),
                });
                continue;
            }
        }
        let derived: Vec<(usize, &BTreeSet<String>)> = ws
            .iter()
            .filter_map(|(si, w)| match w {
                W::Init(u) if !u.is_empty() => Some((*si, u)),
                _ => None,
            })
            .collect();
        if derived.len() == ws.len() && !derived.is_empty() {
            let common: BTreeSet<String> = derived
                .iter()
                .map(|(_, u)| (*u).clone())
                .reduce(|a, b| a.intersection(&b).cloned().collect())
                .unwrap_or_default();
            for y in common {
                let placed = derived.iter().all(|(si, _)| {
                    let Some(s) = stmts.get(*si) else {
                        return false;
                    };
                    let after_write = stmts.iter().enumerate().any(|(sj, t)| {
                        t.block == s.block
                            && t.index + 1 == s.index
                            && writes
                                .get(&y)
                                .is_some_and(|w| w.iter().any(|(k, _)| *k == sj))
                    });
                    let loop_head = s.loop_body
                        && s.index == 0
                        && stmts.iter().enumerate().any(|(sj, t)| {
                            t.block == s.block
                                && t.index > s.index
                                && steps_of(&y).iter().any(|(k, _)| *k == sj)
                        });
                    after_write || loop_head
                });
                if placed {
                    out.push(Mirror {
                        kind: "derived".to_string(),
                        var: v.clone(),
                        partner: y,
                        ratio: None,
                        strict: false,
                        steps: derived.len(),
                        lines: derived
                            .iter()
                            .filter_map(|(si, _)| stmts.get(*si).map(|s| s.line))
                            .collect(),
                    });
                    break;
                }
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::defs::{definitions, one};
    use crate::lex::lex;

    fn run(src: &str) -> Vec<Mirror> {
        let lx = lex(src.as_bytes());
        let ds = definitions(&lx);
        screen(&lx, one(&ds, "f").unwrap(), 2).unwrap()
    }

    #[test]
    fn a_counter_stepped_beside_another_is_its_mirror() {
        let src = "int f(int total)\n{\n    int taken = 0;\n    int remain = total;\n    while (remain > 0) {\n        taken++;\n        remain--;\n    }\n    return taken;\n}\n";
        let m = run(src);
        let remain = m.iter().find(|m| m.var == "remain").unwrap();
        assert_eq!(remain.kind, "lockstep");
        assert_eq!(remain.partner, "taken");
        assert_eq!(remain.ratio, Some(-1.0));
        assert!(remain.strict);
    }

    #[test]
    fn a_product_recomputed_after_its_index_is_derived() {
        let src = "int f(int *base, int n)\n{\n    int i;\n    int off;\n    for (i = 0; i < n; i++) {\n        off = i * 16;\n        g(off);\n    }\n    return 0;\n}\n";
        let m = run(src);
        let off = m.iter().find(|m| m.var == "off").unwrap();
        assert_eq!(off.kind, "derived");
        assert_eq!(off.partner, "i");
    }

    #[test]
    fn an_address_taken_local_is_never_a_candidate() {
        let src = "int f(int n)\n{\n    int a = 0;\n    int b = 0;\n    while (n--) {\n        a++;\n        b++;\n        h(&b);\n    }\n    return a;\n}\n";
        let m = run(src);
        assert!(m.iter().all(|m| m.var != "b"));
    }
}
