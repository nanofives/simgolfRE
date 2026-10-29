use anyhow::{Result, bail};
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;

pub const COLS: usize = 9;
pub const C_RVA: usize = 0;
pub const C_KIND: usize = 1;
pub const C_OLD: usize = 2;
pub const C_NEW: usize = 3;
pub const C_STATE: usize = 4;
pub const C_MANGLED: usize = 5;
pub const C_EVIDENCE: usize = 6;
pub const C_MAC: usize = 7;
pub const C_TYPES: usize = 8;

pub const HEADER: &str = "rva\tkind\told\tnew\tstate\tmangled\tevidence\tmac\ttypes";

pub const REPORT_HEAD: &str = "rva\tkind\tours\tnew\tname\tours_params\tmac\ttypes\treason";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Line {
    Text(String),
    Row(Vec<String>),
}

#[must_use]
pub fn parse(text: &str) -> Vec<Line> {
    text.lines()
        .map(|l| {
            if l.starts_with('#') || l.trim().is_empty() || l.starts_with("rva\t") {
                Line::Text(l.to_string())
            } else {
                let mut c: Vec<String> = l.split('\t').map(str::to_string).collect();
                while c.len() < COLS {
                    c.push("-".into());
                }
                Line::Row(c)
            }
        })
        .collect()
}

#[must_use]
pub fn render(lines: &[Line]) -> String {
    let mut out = String::new();
    for l in lines {
        match l {
            Line::Text(t) if t.starts_with("rva\t") => out.push_str(HEADER),
            Line::Text(t) => out.push_str(t),
            Line::Row(c) => out.push_str(&c.join("\t")),
        }
        out.push('\n');
    }
    out
}

#[must_use]
pub fn col(c: &[String], i: usize) -> &str {
    c.get(i).map_or("-", String::as_str)
}

fn close_of(s: &[u8], open: usize) -> Option<usize> {
    let mut depth = 0i32;
    for (k, &b) in s.iter().enumerate().skip(open) {
        match b {
            b'(' | b'<' => depth = depth.saturating_add(1),
            b')' | b'>' => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    return Some(k.saturating_add(1));
                }
            }
            _ => {}
        }
    }
    None
}

fn split_top(s: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut cur = String::new();
    for ch in s.chars() {
        match ch {
            '(' | '<' => {
                depth = depth.saturating_add(1);
                cur.push(ch);
            }
            ')' | '>' => {
                depth = depth.saturating_sub(1);
                cur.push(ch);
            }
            ',' if depth == 0 => out.push(std::mem::take(&mut cur)),
            _ => cur.push(ch),
        }
    }
    if !cur.trim().is_empty() {
        out.push(cur);
    }
    out
}

#[must_use]
pub fn text_sig(sig: &str) -> Option<(String, Vec<String>, bool)> {
    let b = sig.as_bytes();
    let mut i = 0usize;
    while i < b.len() {
        match b.get(i) {
            Some(b'<') => i = close_of(b, i)?,
            Some(b'(') => {
                let end = close_of(b, i)?;
                let name = sig.get(..i)?.trim().to_string();
                let inner = sig.get(i.saturating_add(1)..end.saturating_sub(1))?;
                let rest = sig.get(end..).unwrap_or("").trim();
                let params = if inner.trim().is_empty() || inner.trim() == "void" {
                    Vec::new()
                } else {
                    split_top(inner).iter().map(|p| canon_type(p)).collect()
                };
                return Some((name, params, rest == "const"));
            }
            _ => i = i.saturating_add(1),
        }
    }
    None
}

#[must_use]
pub fn class_of(q: &str) -> String {
    let b = q.as_bytes();
    let mut depth = 0i32;
    let mut last = None;
    let mut k = 0usize;
    while k < b.len() {
        match b.get(k) {
            Some(b'<') => depth = depth.saturating_add(1),
            Some(b'>') => depth = depth.saturating_sub(1),
            Some(b':') if depth == 0 && b.get(k.saturating_add(1)) == Some(&b':') => {
                last = Some(k);
                k = k.saturating_add(1);
            }
            _ => {}
        }
        k = k.saturating_add(1);
    }
    last.and_then(|l| q.get(..l)).unwrap_or("").to_string()
}

fn word_char(c: char) -> bool {
    c.is_alphanumeric() || c == '_' || c == ':'
}

#[must_use]
pub fn canon_type(t: &str) -> String {
    let mut toks: Vec<String> = Vec::new();
    let mut cur = String::new();
    for ch in t.chars() {
        if word_char(ch) {
            cur.push(ch);
        } else {
            if !cur.is_empty() {
                toks.push(std::mem::take(&mut cur));
            }
            if !ch.is_whitespace() {
                toks.push(ch.to_string());
            }
        }
    }
    if !cur.is_empty() {
        toks.push(cur);
    }
    toks.retain(|w| {
        !matches!(
            w.as_str(),
            "class"
                | "struct"
                | "union"
                | "enum"
                | "__cdecl"
                | "__stdcall"
                | "__thiscall"
                | "__fastcall"
        )
    });
    if toks.first().map(String::as_str) == Some("const") {
        toks.remove(0);
        let mut k = toks
            .iter()
            .take_while(|w| {
                matches!(
                    w.as_str(),
                    "unsigned" | "signed" | "short" | "long" | "int" | "char"
                )
            })
            .count();
        if k == 0 && toks.first().is_some_and(|w| w.chars().all(word_char)) {
            k = 1;
        }
        toks.insert(k.min(toks.len()), "const".into());
    }
    let mut out = String::new();
    for w in &toks {
        let word = w.chars().all(word_char);
        let prev_word = out
            .chars()
            .last()
            .is_some_and(|c| c.is_alphanumeric() || c == '_');
        if word && prev_word {
            out.push(' ');
        }
        out.push_str(w);
    }
    out.replace(", ", ",")
}

#[must_use]
pub fn msvc_text(decorated: &str) -> Option<String> {
    use msvc_demangler::DemangleFlags as F;
    let flags =
        F::NO_ACCESS_SPECIFIERS | F::NO_FUNCTION_RETURNS | F::NO_MS_KEYWORDS | F::NO_MEMBER_TYPE;
    msvc_demangler::demangle(decorated, flags).ok()
}

#[must_use]
pub fn msvc_sig(decorated: &str) -> Option<(Vec<String>, bool)> {
    let (_, p, k) = text_sig(&msvc_text(decorated)?)?;
    Some((p, k))
}

#[must_use]
pub fn msvc_qualified(decorated: &str) -> Option<String> {
    msvc_demangler::demangle(decorated, msvc_demangler::DemangleFlags::NAME_ONLY).ok()
}

fn rename_classes(t: &str, classes: &BTreeMap<String, String>) -> String {
    let mut out = String::new();
    let mut cur = String::new();
    let flush = |cur: &mut String, out: &mut String| {
        if !cur.is_empty() {
            let w = std::mem::take(cur);
            out.push_str(classes.get(&w).map_or(w.as_str(), String::as_str));
        }
    };
    for ch in t.chars() {
        if ch.is_alphanumeric() || ch == '_' {
            cur.push(ch);
        } else {
            flush(&mut cur, &mut out);
            out.push(ch);
        }
    }
    flush(&mut cur, &mut out);
    out
}

#[must_use]
pub fn class_renames(lines: &[Line]) -> BTreeMap<String, String> {
    lines
        .iter()
        .filter_map(|l| match l {
            Line::Row(c)
                if col(c, C_KIND) == "class"
                    && col(c, C_STATE) == "APPLIED"
                    && !col(c, C_NEW).contains('<') =>
            {
                Some((col(c, C_OLD).to_string(), col(c, C_NEW).to_string()))
            }
            _ => None,
        })
        .collect()
}

fn key_of(new: &str) -> &str {
    new.strip_prefix("::").unwrap_or(new)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Origin {
    Names,
    Mangled,
}

#[must_use]
pub fn ours_sig(
    c: &[String],
    by_rva: &BTreeMap<u32, Vec<String>>,
    classes: &BTreeMap<String, String>,
) -> Option<(Vec<String>, bool, Origin)> {
    let rva = u32::from_str_radix(col(c, C_RVA).trim_start_matches("0x"), 16).ok();
    let want = key_of(col(c, C_NEW));
    if let Some(names) = rva.and_then(|r| by_rva.get(&r)) {
        for n in names {
            if msvc_qualified(n).is_some_and(|q| q == want)
                && let Some((p, k)) = msvc_sig(n)
            {
                return Some((p, k, Origin::Names));
            }
        }
    }
    let m = col(c, C_MANGLED);
    if m.starts_with('?') {
        let (p, k) = msvc_sig(m)?;
        return Some((
            p.iter().map(|t| rename_classes(t, classes)).collect(),
            k,
            Origin::Mangled,
        ));
    }
    None
}

#[must_use]
pub fn sig_text(params: &[String], konst: bool) -> String {
    format!(
        "({}){}",
        params.join(", "),
        if konst { " const" } else { "" }
    )
}

#[derive(Debug, Default)]
pub struct Filled {
    pub set: usize,
    pub kept: usize,
    pub none: usize,
    pub ambiguous: Vec<String>,
    pub added: usize,
}

fn pick(
    pass: usize,
    free: &[&String],
    ours: Option<&(Vec<String>, bool, Origin)>,
) -> (Option<String>, bool) {
    let score = |d: &String| -> Option<usize> {
        let (_, mp, mk) = text_sig(d)?;
        let (op, ok, _) = ours?;
        (op.len() == mp.len()).then(|| {
            op.iter()
                .zip(&mp)
                .filter(|(a, b)| a == b)
                .count()
                .saturating_add(usize::from(*ok == mk))
        })
    };
    match pass {
        0 => {
            let full = ours.map_or(0, |(op, _, _)| op.len().saturating_add(1));
            let exact: Vec<&&String> = free.iter().filter(|d| score(d) == Some(full)).collect();
            match exact.as_slice() {
                [one] => (Some((**one).clone()), false),
                _ => (None, false),
            }
        }
        1 => {
            let best = free.iter().filter_map(|d| score(d)).max();
            let tops: Vec<&&String> = free
                .iter()
                .filter(|d| best.is_some() && score(d) == best)
                .collect();
            match tops.as_slice() {
                [one] => (Some((**one).clone()), false),
                [] => (None, false),
                _ => (None, true),
            }
        }
        _ => match free {
            [one] => (Some((*one).clone()), false),
            _ => (None, false),
        },
    }
}

pub fn fill(lines: &mut Vec<Line>, dump: &str, by_rva: &BTreeMap<u32, Vec<String>>) -> Filled {
    let classes = class_renames(lines);
    let mapped: BTreeSet<String> = lines
        .iter()
        .filter_map(|l| match l {
            Line::Row(c) if col(c, C_KIND) == "class" && col(c, C_NEW) != "-" => {
                Some(col(c, C_NEW).replace(", ", ","))
            }
            _ => None,
        })
        .collect();
    let mut groups: BTreeMap<String, Vec<String>> = BTreeMap::new();
    let mut mangled: BTreeMap<String, String> = BTreeMap::new();
    for l in dump.lines() {
        let mut f = l.split('\t');
        let (Some(d), Some(m)) = (f.next(), f.next()) else {
            continue;
        };
        let Some((q, _, _)) = text_sig(d) else {
            continue;
        };
        let g = groups.entry(q.replace(", ", ",")).or_default();
        if !g.iter().any(|x| x == d) {
            g.push(d.to_string());
        }
        mangled
            .entry(d.to_string())
            .or_insert_with(|| m.to_string());
    }
    let mut taken: BTreeSet<String> = BTreeSet::new();
    let mut r = Filled::default();
    let mut open: Vec<usize> = Vec::new();
    for (k, l) in lines.iter().enumerate() {
        let Line::Row(c) = l else { continue };
        if !matches!(col(c, C_KIND), "function" | "method" | "mac-only") {
            continue;
        }
        if col(c, C_MAC) != "-" {
            taken.insert(col(c, C_MAC).to_string());
            r.kept = r.kept.saturating_add(1);
        } else if groups.contains_key(&key_of(col(c, C_NEW)).replace(", ", ",")) {
            open.push(k);
        } else {
            r.none = r.none.saturating_add(1);
        }
    }
    for pass in 0..3 {
        let mut still = Vec::new();
        for &k in &open {
            let Some(Line::Row(c)) = lines.get(k) else {
                continue;
            };
            let key = key_of(col(c, C_NEW)).replace(", ", ",");
            let free: Vec<&String> = groups
                .get(&key)
                .map(|g| g.iter().filter(|d| !taken.contains(*d)).collect())
                .unwrap_or_default();
            let ours = ours_sig(c, by_rva, &classes);
            let (choice, tie) = pick(pass, &free, ours.as_ref());
            if tie {
                r.ambiguous
                    .push(format!("{} {}", col(c, C_RVA), col(c, C_NEW)));
            }
            match choice {
                Some(p) => {
                    if let Some(Line::Row(c)) = lines.get_mut(k)
                        && let Some(x) = c.get_mut(C_MAC)
                    {
                        x.clone_from(&p);
                    }
                    taken.insert(p);
                    r.set = r.set.saturating_add(1);
                }
                None => still.push(k),
            }
        }
        open = still;
    }
    r.none = r.none.saturating_add(open.len());
    r.ambiguous.sort();
    r.ambiguous.dedup();
    for (q, g) in &groups {
        if !mapped.contains(&class_of(q)) {
            continue;
        }
        for d in g.iter().filter(|d| !taken.contains(*d)) {
            lines.push(Line::Row(vec![
                "-".into(),
                "mac-only".into(),
                "-".into(),
                q.clone(),
                "RECORDED".into(),
                mangled.get(d).cloned().unwrap_or_else(|| "-".into()),
                "mac-only: no function of ours pairs with it".into(),
                d.clone(),
                "-".into(),
            ]));
            r.added = r.added.saturating_add(1);
        }
    }
    r
}

#[must_use]
pub fn parse_verdicts(text: &str) -> BTreeMap<String, String> {
    text.lines()
        .filter(|l| !l.starts_with('#') && !l.trim().is_empty())
        .filter_map(|l| l.split_once('\t'))
        .map(|(k, v)| (k.trim().to_string(), v.trim().to_string()))
        .collect()
}

#[must_use]
pub fn difference_kinds(op: &[String], ok: bool, mp: &[String], mk: bool) -> Vec<String> {
    if op.len() != mp.len() {
        return vec!["ARITY".into()];
    }
    let mut k: Vec<String> = op
        .iter()
        .zip(mp)
        .filter(|(a, b)| a != b)
        .map(|(a, b)| format!("{a}=>{b}"))
        .collect();
    if ok != mk {
        k.push("CONST".into());
    }
    k.sort();
    k.dedup();
    k
}

#[derive(Debug, Default)]
pub struct Table {
    pub text: String,
    pub names: BTreeMap<(String, String, String), usize>,
    pub types: BTreeMap<String, usize>,
    pub bad: Vec<String>,
}

fn reason_of(evidence: &str) -> String {
    evidence.split(':').next().unwrap_or("").trim().to_string()
}

fn after_recorded(v: &str) -> String {
    v.trim_start_matches("RECORDED")
        .trim_start_matches(':')
        .trim()
        .to_string()
}

struct Verdict {
    state: String,
    reason: String,
}

fn type_verdict(
    c: &[String],
    ours: &(Vec<String>, bool, Origin),
    mac: &(String, Vec<String>, bool),
    classes: &BTreeMap<String, String>,
    verdicts: &BTreeMap<String, String>,
    bad: &mut Vec<String>,
) -> Verdict {
    let (op, ok, _) = ours;
    let (_, mp, mk) = mac;
    let before = col(c, C_MANGLED)
        .starts_with('?')
        .then(|| msvc_sig(col(c, C_MANGLED)))
        .flatten()
        .map(|(p, k)| {
            (
                p.iter()
                    .map(|t| rename_classes(t, classes))
                    .collect::<Vec<_>>(),
                k,
            )
        });
    let decided = col(c, C_TYPES);
    let v = |s: &str, r: String| Verdict {
        state: s.to_string(),
        reason: r,
    };
    if op == mp && ok == mk {
        let taken = before.as_ref().is_some_and(|(bp, bk)| bp != mp || bk != mk);
        return v(
            if taken || decided.starts_with("APPLIED") {
                "APPLIED"
            } else {
                "same"
            },
            String::new(),
        );
    }
    if decided.starts_with("RECORDED") {
        return v("RECORDED", after_recorded(decided));
    }
    let mut why = Vec::new();
    let mut open = Vec::new();
    let mut owed = Vec::new();
    for k in &difference_kinds(op, *ok, mp, *mk) {
        match verdicts.get(k) {
            Some(x) if x.starts_with("RECORDED") => why.push(format!("{k}: {}", after_recorded(x))),
            Some(_) => owed.push(k.clone()),
            None => open.push(k.clone()),
        }
    }
    let (rva, new) = (col(c, C_RVA), col(c, C_NEW));
    if !owed.is_empty() {
        bad.push(format!(
            "{rva} {new}: {} decided APPLIED but ours {} is not the Mac's {}",
            owed.join(", "),
            sig_text(op, *ok),
            sig_text(mp, *mk)
        ));
        return v("CONTRADICTED", String::new());
    }
    if !open.is_empty() {
        bad.push(format!(
            "{rva} {new}: {} undecided -- ours {} against the Mac's {}",
            open.join(", "),
            sig_text(op, *ok),
            sig_text(mp, *mk)
        ));
        return v("UNDECIDED", String::new());
    }
    v("RECORDED", why.join("; "))
}

#[must_use]
pub fn table(
    lines: &[Line],
    by_rva: &BTreeMap<u32, Vec<String>>,
    verdicts: &BTreeMap<String, String>,
) -> Table {
    let classes = class_renames(lines);
    let mut t = Table {
        text: format!("{REPORT_HEAD}\n"),
        ..Table::default()
    };
    for l in lines {
        let Line::Row(c) = l else { continue };
        let (kind, state) = (col(c, C_KIND), col(c, C_STATE));
        let reason = if state == "RECORDED" {
            reason_of(col(c, C_EVIDENCE))
        } else {
            String::new()
        };
        let e = t
            .names
            .entry((kind.to_string(), state.to_string(), reason.clone()))
            .or_default();
        *e = e.saturating_add(1);
        let mut ours_txt = "-".to_string();
        let mut verdict = Verdict {
            state: "-".into(),
            reason: String::new(),
        };
        if matches!(kind, "function" | "method") && state == "APPLIED" {
            let ours = ours_sig(c, by_rva, &classes);
            if let Some((op, ok, _)) = &ours {
                ours_txt = sig_text(op, *ok);
            }
            if let (Some(o), Some(m)) = (&ours, text_sig(col(c, C_MAC))) {
                verdict = type_verdict(c, o, &m, &classes, verdicts, &mut t.bad);
            }
        }
        let e = t.types.entry(verdict.state.clone()).or_default();
        *e = e.saturating_add(1);
        let why = match (reason.is_empty(), verdict.reason.is_empty()) {
            (_, true) => reason,
            (true, false) => format!("types: {}", verdict.reason),
            (false, false) => format!("{reason}; types: {}", verdict.reason),
        };
        let _ = writeln!(
            t.text,
            "{}\t{kind}\t{}\t{}\t{state}\t{ours_txt}\t{}\t{}\t{}",
            col(c, C_RVA),
            col(c, C_OLD),
            col(c, C_NEW),
            col(c, C_MAC),
            verdict.state,
            if why.is_empty() { "-" } else { why.as_str() }
        );
    }
    t
}

#[must_use]
pub fn summary(t: &Table) -> String {
    let mut msg = String::from("mac types: per kind, state and reason\n");
    for ((k, s, r), n) in &t.names {
        let _ = writeln!(
            msg,
            "  {k:<9} {s:<9} {:<14} {n}",
            if r.is_empty() { "-" } else { r }
        );
    }
    let _ = writeln!(msg, "parameter types (functions with a Mac signature):");
    for (k, n) in t.types.iter().filter(|(k, _)| k.as_str() != "-") {
        let _ = writeln!(msg, "  {k:<12} {n}");
    }
    msg
}

pub fn check(t: &Table) -> Result<()> {
    if t.bad.is_empty() {
        return Ok(());
    }
    bail!(
        "{} function row(s) differ from the Mac's parameter types with no decision in the map's `types` column \
         (APPLIED once the source takes them, or RECORDED: why, with the figure)",
        t.bad.len()
    )
}
