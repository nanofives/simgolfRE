use crate::defs::{self, Def};
use crate::flow::{self, Flow, Opts, Track};
use crate::lex::{self, Kind, Lexed};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

#[must_use]
pub fn myers<T: PartialEq>(a: &[T], b: &[T]) -> Vec<(usize, usize)> {
    let pre = a.iter().zip(b).take_while(|(x, y)| x == y).count();
    let (ra, rb) = (
        a.get(pre..).unwrap_or_default(),
        b.get(pre..).unwrap_or_default(),
    );
    let suf = ra
        .iter()
        .rev()
        .zip(rb.iter().rev())
        .take_while(|(x, y)| x == y)
        .count();
    let (ma, mb) = (
        ra.get(..ra.len() - suf).unwrap_or_default(),
        rb.get(..rb.len() - suf).unwrap_or_default(),
    );
    let mut out: Vec<(usize, usize)> = (0..pre).map(|i| (i, i)).collect();
    let (n, m) = (ma.len(), mb.len());
    let (ni, mi) = (
        isize::try_from(n).unwrap_or(isize::MAX),
        isize::try_from(m).unwrap_or(isize::MAX),
    );
    let max = n + m;
    let off = max + 1;
    let at = |v: &[isize], k: isize| -> isize {
        usize::try_from(isize::try_from(off).unwrap_or(0) + k)
            .ok()
            .and_then(|i| v.get(i))
            .copied()
            .unwrap_or(0)
    };
    let mut v: Vec<isize> = vec![0; 2 * max + 3];
    let mut trace: Vec<Vec<isize>> = Vec::new();
    let eq = |x: isize, y: isize| -> bool {
        match (usize::try_from(x), usize::try_from(y)) {
            (Ok(x), Ok(y)) => matches!((ma.get(x), mb.get(y)), (Some(p), Some(q)) if p == q),
            _ => false,
        }
    };
    let mut done = n == 0 && m == 0;
    let mut d_end = 0isize;
    if !done {
        'outer: for d in 0..=isize::try_from(max).unwrap_or(0) {
            trace.push(v.clone());
            let mut k = -d;
            while k <= d {
                let mut x = if k == -d || (k != d && at(&v, k - 1) < at(&v, k + 1)) {
                    at(&v, k + 1)
                } else {
                    at(&v, k - 1) + 1
                };
                let mut y = x - k;
                while x < ni && y < mi && eq(x, y) {
                    x += 1;
                    y += 1;
                }
                if let Some(slot) = usize::try_from(isize::try_from(off).unwrap_or(0) + k)
                    .ok()
                    .and_then(|i| v.get_mut(i))
                {
                    *slot = x;
                }
                if x >= ni && y >= mi {
                    d_end = d;
                    done = true;
                    break 'outer;
                }
                k += 2;
            }
        }
    }
    let mut mid: Vec<(usize, usize)> = Vec::new();
    if done && (n > 0 || m > 0) {
        let (mut x, mut y) = (ni, mi);
        let mut d = d_end;
        while d >= 0 && (x > 0 || y > 0) {
            let Some(vd) = trace.get(usize::try_from(d).unwrap_or(0)) else {
                break;
            };
            let k = x - y;
            let prev_k = if k == -d || (k != d && at(vd, k - 1) < at(vd, k + 1)) {
                k + 1
            } else {
                k - 1
            };
            let prev_x = if d == 0 { 0 } else { at(vd, prev_k) };
            let prev_y = prev_x - prev_k;
            while x > prev_x.max(0) && y > prev_y.max(0) {
                x -= 1;
                y -= 1;
                if let (Ok(px), Ok(py)) = (usize::try_from(x), usize::try_from(y)) {
                    mid.push((px, py));
                }
            }
            if d == 0 {
                break;
            }
            x = prev_x;
            y = prev_y;
            d -= 1;
        }
        mid.reverse();
    }
    out.extend(mid.into_iter().map(|(x, y)| (x + pre, y + pre)));
    let (la, lb) = (a.len(), b.len());
    out.extend((0..suf).map(|i| (la - suf + i, lb - suf + i)));
    out
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Options {
    pub macros: BTreeMap<String, i128>,
    pub renames: BTreeMap<String, String>,
    pub out_param: Vec<String>,
    pub write_only: Vec<String>,
}

#[must_use]
pub fn numeric_macros(lx: &Lexed) -> BTreeMap<String, i128> {
    let mut out = BTreeMap::new();
    for d in lx.directives.iter().filter(|d| d.name == "define") {
        let args = lx.args(d);
        let Some(name) = args.first().filter(|t| t.kind == Kind::Ident) else {
            continue;
        };
        if args
            .get(1)
            .is_some_and(|p| p.is_punct("(") && !p.space_before)
        {
            continue;
        }
        let body: Vec<&lex::Token> = args
            .iter()
            .skip(1)
            .filter(|t| !t.is_punct("(") && !t.is_punct(")"))
            .collect();
        let value = match body.as_slice() {
            [n] if n.kind == Kind::Number => lex::number_value(&n.text),
            [m, n] if m.is_punct("-") && n.kind == Kind::Number => {
                lex::number_value(&n.text).map(|v| -v)
            }
            _ => None,
        };
        if let Some(v) = value {
            out.insert(name.text.clone(), v);
        }
    }
    let mut ev: Option<i128> = None;
    let toks: Vec<&lex::Token> = lx.code();
    let mut in_enum = false;
    let mut depth = 0i32;
    for (k, t) in toks.iter().enumerate() {
        if t.is("enum") {
            in_enum = true;
            ev = None;
            continue;
        }
        if in_enum && t.is_punct("{") {
            depth += 1;
            ev = Some(-1);
            continue;
        }
        if in_enum && t.is_punct("}") {
            depth -= 1;
            if depth <= 0 {
                in_enum = false;
            }
            continue;
        }
        if !in_enum || depth != 1 || t.kind != Kind::Ident {
            continue;
        }
        let prev = k.checked_sub(1).and_then(|p| toks.get(p));
        if !prev.is_some_and(|p| p.is_punct("{") || p.is_punct(",")) {
            continue;
        }
        let next = toks.get(k + 1);
        let v = if next.is_some_and(|n| n.is_punct("=")) {
            match (toks.get(k + 2), toks.get(k + 3)) {
                (Some(n), Some(e))
                    if n.kind == Kind::Number && (e.is_punct(",") || e.is_punct("}")) =>
                {
                    lex::number_value(&n.text)
                }
                (Some(m), Some(n))
                    if m.is_punct("-")
                        && n.kind == Kind::Number
                        && toks
                            .get(k + 4)
                            .is_some_and(|e| e.is_punct(",") || e.is_punct("}")) =>
                {
                    lex::number_value(&n.text).map(|v| -v)
                }
                _ => None,
            }
        } else {
            ev.map(|p| p + 1)
        };
        ev = v;
        if let Some(v) = v {
            out.insert(t.text.clone(), v);
        }
    }
    out
}

struct Side<'a> {
    def: &'a Def,
    toks: Vec<flow::Tok>,
    keys: Vec<String>,
    flow: Flow,
    scoped: Flow,
}

fn side<'a>(
    lx: &'a Lexed,
    def: &'a Def,
    o: &Options,
    macros: &BTreeMap<String, i128>,
) -> Result<Side<'a>, String> {
    let opts = Opts {
        track: Track::Locals,
        out_param: o.out_param.clone(),
        write_only: o.write_only.clone(),
        short_circuit: true,
        decl_defs: false,
    };
    let fl = flow::analyse(lx, def, &opts).map_err(|p| p.0)?;
    let scoped = flow::analyse(
        lx,
        def,
        &Opts {
            decl_defs: true,
            ..opts
        },
    )
    .map_err(|p| p.0)?;
    let locals: BTreeSet<String> = fl.decls.names();
    let toks = flow::tokens(lx, def.head, def.close);
    let mut fn_toks: BTreeSet<usize> = BTreeSet::new();
    let mut k = def.name_tok;
    fn_toks.insert(k);
    while k >= 2 {
        let before = lx.tokens.get(k - 1);
        if before.is_some_and(|t| t.is_punct("~")) {
            k -= 1;
            fn_toks.insert(k);
        } else if before.is_some_and(|t| t.is_punct("::")) {
            k -= 2;
            fn_toks.insert(k);
            fn_toks.insert(k + 1);
        } else {
            break;
        }
    }
    let keys = toks
        .iter()
        .enumerate()
        .map(|(n, t)| {
            if fn_toks.contains(&t.idx) {
                return "F".to_string();
            }
            match t.kind {
                Kind::Ident if locals.contains(&t.text) => {
                    if toks
                        .get(n + 1)
                        .is_some_and(|x| x.kind == Kind::Punct && x.text == "=")
                    {
                        "L=".to_string()
                    } else {
                        "L".to_string()
                    }
                }
                Kind::Ident => {
                    let name = o
                        .renames
                        .get(&t.text)
                        .cloned()
                        .unwrap_or_else(|| t.text.clone());
                    match macros.get(&name) {
                        Some(v) => format!("N:{v}"),
                        None => format!("T:{name}"),
                    }
                }
                Kind::Number => lex::number_value(&t.text)
                    .map_or_else(|| format!("T:{}", t.text), |v| format!("N:{v}")),
                _ => format!("T:{}", t.text),
            }
        })
        .collect();
    Ok(Side {
        def,
        toks,
        keys,
        flow: fl,
        scoped,
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Pair {
    pub old: String,
    pub new: String,
    pub count: usize,
    pub first_line: usize,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Verdict {
    pub function: String,
    pub new_function: String,
    pub proven: bool,
    pub identical: bool,
    pub alignment: String,
    pub findings: Vec<String>,
    pub pairs: Vec<Pair>,
    pub reads: usize,
    pub writes: usize,
}

fn type_at(fl: &Flow, name: &str, tok: usize) -> Option<String> {
    fl.decls
        .declarators
        .iter()
        .filter(|d| d.name == name && (d.param || d.tok <= tok))
        .max_by_key(|d| if d.param { 0 } else { d.tok })
        .map(|d| d.ty.split_whitespace().collect::<Vec<_>>().join(" "))
}

fn reversed(a: &[String], b: &[String]) -> Vec<(usize, usize)> {
    let ra: Vec<&String> = a.iter().rev().collect();
    let rb: Vec<&String> = b.iter().rev().collect();
    let mut v: Vec<(usize, usize)> = myers(&ra, &rb)
        .into_iter()
        .map(|(i, j)| (a.len() - 1 - i, b.len() - 1 - j))
        .collect();
    v.reverse();
    v
}

type Judged = (Vec<String>, Vec<Pair>, usize, usize);

fn judge(a: &Side<'_>, b: &Side<'_>, pairs: &[(usize, usize)]) -> Judged {
    let mut findings = Vec::new();
    let mut a2b: BTreeMap<usize, usize> = BTreeMap::new();
    let mut b2a: BTreeMap<usize, usize> = BTreeMap::new();
    for &(i, j) in pairs {
        if let (Some(x), Some(y)) = (a.toks.get(i), b.toks.get(j)) {
            a2b.insert(x.idx, y.idx);
            b2a.insert(y.idx, x.idx);
        }
    }
    let unpaired = |s: &Side<'_>,
                    map: &BTreeMap<usize, usize>,
                    what: &str,
                    out: &mut Vec<String>| {
        for t in &s.toks {
            if map.contains_key(&t.idx) || s.flow.decls.toks.contains(&t.idx) {
                continue;
            }
            if t.idx > s.def.params.0 && t.idx < s.def.params.1 {
                out.push(format!(
                    "{what} line {}: `{}` in the parameter list has no partner: a parameter's type or count changed",
                    t.line, t.text
                ));
            } else {
                out.push(format!(
                    "{what} line {}: `{}` has no partner and declares nothing: the rewrite changed code, not only \
                     names and declarations",
                    t.line, t.text
                ));
            }
        }
    };
    unpaired(a, &a2b, "the original's", &mut findings);
    unpaired(b, &b2a, "the rewrite's", &mut findings);
    let reads_b: BTreeMap<usize, &flow::Use> = b.flow.uses.iter().map(|u| (u.tok, u)).collect();
    let reads_a: BTreeSet<usize> = a.flow.uses.iter().map(|u| u.tok).collect();
    let writes_a: BTreeSet<usize> = a.flow.writes.iter().map(|w| w.tok).collect();
    let writes_b: BTreeMap<usize, &flow::Write> =
        b.flow.writes.iter().map(|w| (w.tok, w)).collect();
    let mut counts: BTreeMap<(String, String), (usize, usize)> = BTreeMap::new();
    let (mut reads, mut writes) = (0usize, 0usize);
    for u in &a.flow.uses {
        reads += 1;
        let Some(nt) = a2b.get(&u.tok) else {
            continue;
        };
        let Some(nu) = reads_b.get(nt) else {
            findings.push(format!(
                "line {}: the original reads `{}` where the rewrite reads no local",
                u.line, u.var
            ));
            continue;
        };
        let e = counts
            .entry((u.var.clone(), nu.var.clone()))
            .or_insert((0, u.line));
        e.0 += 1;
        let got: BTreeSet<Option<Option<usize>>> = u
            .reached
            .iter()
            .map(|d| match d {
                None => Some(None),
                Some(t) => a2b.get(t).map(|x| Some(*x)),
            })
            .collect();
        let want: BTreeSet<Option<Option<usize>>> = nu.reached.iter().map(|d| Some(*d)).collect();
        if got != want {
            findings.push(format!(
                "line {}: the read of `{}` (now `{}`) is reached by other writes in the rewrite ({} before, {} after)",
                u.line,
                u.var,
                nu.var,
                u.reached.len(),
                nu.reached.len()
            ));
        }
        let (ta, tb) = (
            type_at(&a.flow, &u.var, u.tok),
            type_at(&b.flow, &nu.var, *nt),
        );
        if ta != tb {
            findings.push(format!(
                "line {}: `{}` is `{}` and `{}` is `{}`: a declared type changed",
                u.line,
                u.var,
                ta.unwrap_or_default(),
                nu.var,
                tb.unwrap_or_default()
            ));
        }
    }
    for u in &b.flow.uses {
        if !b2a.get(&u.tok).is_some_and(|t| reads_a.contains(t)) {
            findings.push(format!(
                "line {}: the rewrite reads `{}` where the original reads no local",
                u.line, u.var
            ));
        }
    }
    for w in &a.flow.writes {
        writes += 1;
        let paired = a2b
            .get(&w.tok)
            .and_then(|t| writes_b.get(t))
            .is_some_and(|nw| nw.maybe == w.maybe);
        if !paired {
            findings.push(format!(
                "line {}: the original's write of `{}` has no partner in the rewrite",
                w.line, w.var
            ));
        }
    }
    for w in &b.flow.writes {
        if !b2a.get(&w.tok).is_some_and(|t| writes_a.contains(t)) {
            findings.push(format!(
                "line {}: the rewrite's write of `{}` has no partner in the original",
                w.line, w.var
            ));
        }
    }
    let losses = |s: &Side<'_>| -> BTreeMap<usize, BTreeSet<usize>> {
        s.flow
            .uses
            .iter()
            .zip(&s.scoped.uses)
            .filter(|(f, c)| f.tok == c.tok)
            .map(|(f, c)| {
                let lost: BTreeSet<usize> = f
                    .real
                    .iter()
                    .filter(|d| !c.real.contains(d))
                    .filter_map(|d| *d)
                    .collect();
                (f.tok, lost)
            })
            .filter(|(_, l)| !l.is_empty())
            .collect()
    };
    let (la, lb) = (losses(a), losses(b));
    for u in &b.flow.uses {
        let Some(lost) = lb.get(&u.tok) else {
            continue;
        };
        let before: BTreeSet<usize> = b2a
            .get(&u.tok)
            .and_then(|t| la.get(t))
            .map(|l| l.iter().filter_map(|d| a2b.get(d).copied()).collect())
            .unwrap_or_default();
        if !lost.is_subset(&before) {
            findings.push(format!(
                "line {}: `{}` carries a value across the end of its block in the rewrite, which the original does \
                 not: C++ ends a block-local's value there",
                u.line, u.var
            ));
        }
    }
    findings.sort();
    findings.dedup();
    let pairs = counts
        .into_iter()
        .map(|((old, new), (count, first_line))| Pair {
            old,
            new,
            count,
            first_line,
        })
        .collect();
    (findings, pairs, reads, writes)
}

#[must_use]
pub fn check_pair(
    old: (&Lexed, &Def, &BTreeMap<String, i128>),
    new: (&Lexed, &Def, &BTreeMap<String, i128>),
    o: &Options,
) -> Verdict {
    let mut v = Verdict {
        function: old.1.qualified(),
        new_function: new.1.qualified(),
        proven: false,
        identical: false,
        alignment: String::new(),
        findings: Vec::new(),
        pairs: Vec::new(),
        reads: 0,
        writes: 0,
    };
    let (a, b) = match (side(old.0, old.1, o, old.2), side(new.0, new.1, o, new.2)) {
        (Ok(a), Ok(b)) => (a, b),
        (Err(e), _) => {
            v.findings.push(format!("the original does not parse: {e}"));
            return v;
        }
        (_, Err(e)) => {
            v.findings.push(format!("the rewrite does not parse: {e}"));
            return v;
        }
    };
    v.identical = a
        .toks
        .iter()
        .map(|t| &t.text)
        .eq(b.toks.iter().map(|t| &t.text));
    let mut name = "forward";
    let mut best: Judged = judge(&a, &b, &myers(&a.keys, &b.keys));
    if !best.0.is_empty() {
        let back = judge(&a, &b, &reversed(&a.keys, &b.keys));
        if back.0.len() < best.0.len() {
            best = back;
            name = "backward";
        }
    }
    let (findings, pairs, reads, writes) = best;
    v.alignment = name.to_string();
    v.findings = findings;
    v.pairs = pairs;
    v.reads = reads;
    v.writes = writes;
    v.proven = v.findings.is_empty();
    v
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct FileVerdict {
    pub path: String,
    pub functions: Vec<Verdict>,
    pub unchanged: usize,
    pub outside: Vec<String>,
    pub problems: Vec<String>,
}

impl FileVerdict {
    #[must_use]
    pub fn proven(&self) -> bool {
        self.problems.is_empty()
            && self.outside.is_empty()
            && self.functions.iter().all(|f| f.proven)
    }
}

fn outside_keys(lx: &Lexed, defs: &[Def], o: &Options) -> Vec<(String, usize, String)> {
    let inside: Vec<(usize, usize)> = defs.iter().map(|d| (d.head, d.close)).collect();
    let code: Vec<usize> = lx
        .live_code()
        .into_iter()
        .filter(|i| !inside.iter().any(|(a, b)| a <= i && i <= b))
        .collect();
    let toks: Vec<&lex::Token> = code.iter().filter_map(|&i| lx.tokens.get(i)).collect();
    let mut depth = 0usize;
    let mut list_at: Option<usize> = None;
    let mut out = Vec::with_capacity(toks.len());
    for (k, t) in toks.iter().enumerate() {
        let prev = k.checked_sub(1).and_then(|p| toks.get(p));
        if t.is_punct("(") {
            if depth == 0 && prev.is_some_and(|p| p.kind == Kind::Ident) {
                list_at = Some(depth + 1);
            }
            depth += 1;
        } else if t.is_punct(")") {
            if list_at == Some(depth) {
                list_at = None;
            }
            depth = depth.saturating_sub(1);
        }
        let next = toks.get(k + 1);
        let param_name = list_at == Some(depth)
            && t.kind == Kind::Ident
            && prev.is_some_and(|p| p.kind == Kind::Ident || p.is_punct("*") || p.is_punct("&"))
            && !prev
                .is_some_and(|p| p.is("struct") || p.is("class") || p.is("enum") || p.is("union"))
            && next.is_some_and(|n| {
                n.is_punct(",") || n.is_punct(")") || n.is_punct("[") || n.is_punct("=")
            });
        let key = if param_name {
            "P".to_string()
        } else if t.kind == Kind::Ident {
            o.renames
                .get(&t.text)
                .cloned()
                .unwrap_or_else(|| t.text.clone())
        } else {
            t.text.clone()
        };
        out.push((key, t.line, t.text.clone()));
    }
    out
}

#[must_use]
pub fn check_file(path: &str, old: &[u8], new: &[u8], o: &Options) -> FileVerdict {
    let (la, lb) = (lex::lex(old), lex::lex(new));
    let mut fv = FileVerdict {
        path: path.to_string(),
        functions: Vec::new(),
        unchanged: 0,
        outside: Vec::new(),
        problems: Vec::new(),
    };
    let with_own = |lx: &Lexed| {
        let mut m = o.macros.clone();
        m.extend(numeric_macros(lx));
        m
    };
    let (ma, mb) = (with_own(&la), with_own(&lb));
    let (da, db) = (defs::definitions(&la), defs::definitions(&lb));
    if da.len() != db.len() {
        fv.problems.push(format!(
            "{} function definition(s) before and {} after: definitions are paired by position, so the rewrite may \
             not add, remove or reorder them",
            da.len(),
            db.len()
        ));
        return fv;
    }
    for (x, y) in da.iter().zip(&db) {
        let same = flow::tokens(&la, x.head, x.close)
            .iter()
            .map(|t| t.text.clone())
            .eq(flow::tokens(&lb, y.head, y.close)
                .iter()
                .map(|t| t.text.clone()));
        if same {
            fv.unchanged += 1;
            continue;
        }
        fv.functions
            .push(check_pair((&la, x, &ma), (&lb, y, &mb), o));
    }
    let (oa, ob) = (outside_keys(&la, &da, o), outside_keys(&lb, &db, o));
    let ka: Vec<&String> = oa.iter().map(|x| &x.0).collect();
    let kb: Vec<&String> = ob.iter().map(|x| &x.0).collect();
    if ka != kb {
        let at = ka
            .iter()
            .zip(&kb)
            .position(|(x, y)| x != y)
            .unwrap_or(ka.len().min(kb.len()));
        let say = |v: &[(String, usize, String)]| {
            v.get(at).map_or("the end of the file".to_string(), |x| {
                format!("`{}` at line {}", x.2, x.1)
            })
        };
        fv.outside.push(format!(
            "outside the function bodies the code changed beyond parameter names: {} before, {} after",
            say(&oa),
            say(&ob)
        ));
    }
    fv
}

#[must_use]
pub fn implied(old: &[u8], new: &[u8]) -> Vec<(String, String)> {
    let (da, db) = (
        defs::definitions(&lex::lex(old)),
        defs::definitions(&lex::lex(new)),
    );
    if da.len() != db.len() {
        return Vec::new();
    }
    da.iter()
        .zip(&db)
        .filter(|(x, y)| x.base != y.base)
        .map(|(x, y)| (y.base.clone(), x.base.clone()))
        .collect()
}

#[must_use]
pub fn consistent(pairs: &[(String, String)]) -> BTreeMap<String, String> {
    let mut seen: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut back: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    for (new, old) in pairs {
        seen.entry(new.clone()).or_default().insert(old.clone());
        back.entry(old.clone()).or_default().insert(new.clone());
    }
    seen.into_iter()
        .filter_map(|(new, olds)| {
            let old = olds.iter().next()?.clone();
            (olds.len() == 1 && back.get(&old).is_some_and(|n| n.len() == 1)).then_some((new, old))
        })
        .collect()
}

#[must_use]
pub fn check_named(
    old: &[u8],
    new: &[u8],
    names: &[(String, String)],
    o: &Options,
) -> Vec<Result<Verdict, String>> {
    let (la, lb) = (lex::lex(old), lex::lex(new));
    let with_own = |lx: &Lexed| {
        let mut m = o.macros.clone();
        m.extend(numeric_macros(lx));
        m
    };
    let (ma, mb) = (with_own(&la), with_own(&lb));
    let (da, db) = (defs::definitions(&la), defs::definitions(&lb));
    names
        .iter()
        .map(|(on, nn)| {
            let x = defs::one(&da, on).map_err(|e| format!("the original: {e}"))?;
            let y = defs::one(&db, nn).map_err(|e| format!("the rewrite: {e}"))?;
            Ok(check_pair((&la, x, &ma), (&lb, y, &mb), o))
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn check(old: &str, new: &str) -> FileVerdict {
        check_file("t.cpp", old.as_bytes(), new.as_bytes(), &Options::default())
    }

    #[test]
    fn myers_aligns_a_rename_and_an_insertion() {
        let a = ["a", "b", "c", "d"];
        let b = ["a", "x", "b", "c", "d"];
        assert_eq!(myers(&a, &b), vec![(0, 0), (1, 2), (2, 3), (3, 4)]);
        let c: [&str; 0] = [];
        assert!(myers(&c, &b).is_empty());
        assert_eq!(myers(&a, &a).len(), 4);
        let x = ["p", "q", "r", "s", "t"];
        let y = ["q", "s", "u", "t"];
        assert_eq!(myers(&x, &y), vec![(1, 0), (3, 1), (4, 3)]);
    }

    #[test]
    fn renamed_parameters_and_locals_are_proven() {
        let old = "int Cls::f(int cb, char *p)\n{\n    int i, n = 0;\n    for (i = 0; i < cb; i++) n += p[i];\n    return n;\n}\nint Cls::g(int cb);\n";
        let new = "int Cls::f(int count, char *text)\n{\n    int sum = 0;\n    for (int k = 0; k < count; k++) sum += text[k];\n    return sum;\n}\nint Cls::g(int count);\n";
        let v = check(old, new);
        assert!(v.proven(), "{v:#?}");
        assert_eq!(v.functions.len(), 1);
        assert!(
            v.functions[0]
                .pairs
                .iter()
                .any(|p| p.old == "cb" && p.new == "count")
        );
    }

    #[test]
    fn a_swapped_read_is_refused() {
        let old = "int f(int a, int b)\n{\n    return a - b;\n}\n";
        let new = "int f(int a, int b)\n{\n    return b - a;\n}\n";
        let v = check(old, new);
        assert!(!v.proven());
        assert!(
            v.functions[0]
                .findings
                .iter()
                .any(|f| f.contains("reached"))
        );
    }

    #[test]
    fn a_number_spelt_as_its_constant_is_proven_and_another_is_not() {
        let old = "#define LIMIT 40\nint f(int i)\n{\n    return i >= 40;\n}\n";
        let new = "#define LIMIT 40\nint f(int i)\n{\n    return i >= LIMIT;\n}\n";
        assert!(check(old, new).proven());
        let wrong = "#define LIMIT 41\nint f(int i)\n{\n    return i >= LIMIT;\n}\n";
        assert!(!check(old, wrong).proven());
        let en = "enum { A_LIMIT = 40 };\nint f(int i)\n{\n    return i >= A_LIMIT;\n}\n";
        let en_old = "enum { A_LIMIT = 40 };\nint f(int i)\n{\n    return i >= 40;\n}\n";
        assert!(check(en_old, en).proven());
    }

    #[test]
    fn a_changed_type_or_constant_is_refused() {
        let old = "int f(int a)\n{\n    int x = a;\n    return x;\n}\n";
        let new = "int f(int a)\n{\n    unsigned x = a;\n    return x;\n}\n";
        let v = check(old, new);
        assert!(!v.proven());
        assert!(
            v.functions[0].findings.iter().any(|f| f.contains("type")),
            "{v:#?}"
        );
        let new2 = "int f(int a)\n{\n    int x = a + 1;\n    return x;\n}\n";
        assert!(!check(old, new2).proven());
    }

    #[test]
    fn a_loop_carried_value_lost_to_a_block_declaration_is_refused() {
        let old = "int f(int n)\n{\n    int s;\n    int i;\n    s = 0;\n    for (i = 0; i < n; i++) {\n        if (i) s = s + i;\n        else s = 1;\n    }\n    return 0;\n}\n";
        let new = "int f(int n)\n{\n    int i;\n    for (i = 0; i < n; i++) {\n        int s;\n        if (i) s = s + i;\n        else s = 1;\n    }\n    return 0;\n}\n";
        let v = check(old, new);
        assert!(!v.proven(), "{v:#?}");
        let old = "int f(int n)\n{\n    int s;\n    int i;\n    for (i = 0; i < n; i++) {\n        if (i) s = s + i;\n        else s = 1;\n    }\n    return 0;\n}\n";
        let v = check(old, new);
        assert!(!v.proven(), "{v:#?}");
        assert!(
            v.functions[0]
                .findings
                .iter()
                .all(|f| f.contains("across the end of its block")),
            "{v:#?}"
        );
        let same_scope = "int f(int n)\n{\n    int i;\n    int s;\n    for (i = 0; i < n; i++) {\n        if (i) s = s + i;\n        else s = 1;\n    }\n    return 0;\n}\n";
        assert!(check(old, same_scope).proven());
        assert!(
            check(
                new,
                new.replace("int s;", "int t;")
                    .replace("s =", "t =")
                    .replace("s +", "t +")
                    .as_str()
            )
            .proven()
        );
    }

    #[test]
    fn code_changed_outside_the_bodies_is_refused_but_prototype_names_are_free() {
        let old = "int g(int a, int b);\nint x = 1;\n";
        let names = "int g(int left, int right);\nint x = 1;\n";
        assert!(check(old, names).proven());
        let changed = "int g(int a, long b);\nint x = 1;\n";
        assert!(!check(old, changed).proven());
    }

    #[test]
    fn a_renamed_member_is_free_only_when_declared() {
        let old = "int f(S *p)\n{\n    return p->m_old;\n}\n";
        let new = "int f(S *p)\n{\n    return p->m_new;\n}\n";
        assert!(!check(old, new).proven());
        let mut o = Options::default();
        o.renames.insert("m_new".into(), "m_old".into());
        assert!(check_file("t.cpp", old.as_bytes(), new.as_bytes(), &o).proven());
    }
}
