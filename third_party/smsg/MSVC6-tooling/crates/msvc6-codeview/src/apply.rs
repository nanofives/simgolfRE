use crate::diff::{ClassPlan, FieldRename, Fix, PlanMember, Row};
use crate::record::Access;
use crate::source::{Src, leaf};
use crate::ty::Key;
use anyhow::{Context as _, Result};
use msvc6_base::srcio;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Applied {
    pub changed: Vec<String>,
    pub refused: Vec<String>,
    pub count: usize,
}

fn code_words(t: &str) -> Vec<(String, usize, bool)> {
    let b = t.as_bytes();
    let at = |k: usize| b.get(k).copied();
    let mut out = Vec::new();
    let mut i = 0usize;
    while let Some(c) = at(i) {
        if c == b'/' && at(i + 1) == Some(b'/') {
            while at(i).is_some_and(|x| x != b'\n') {
                i += 1;
            }
            continue;
        }
        if c == b'/' && at(i + 1) == Some(b'*') {
            i += 2;
            while i + 1 < b.len() && !(at(i) == Some(b'*') && at(i + 1) == Some(b'/')) {
                i += 1;
            }
            i += 2;
            continue;
        }
        if c == b'"' || c == b'\'' {
            i += 1;
            while at(i).is_some_and(|x| x != c && x != b'\n') {
                if at(i) == Some(b'\\') {
                    i += 1;
                }
                i += 1;
            }
            i += 1;
            continue;
        }
        if c.is_ascii_alphabetic() || c == b'_' {
            let s = i;
            while at(i).is_some_and(|x| x.is_ascii_alphanumeric() || x == b'_') {
                i += 1;
            }
            let before = t.get(..s).unwrap_or_default().trim_end();
            let after = t.get(i..).unwrap_or_default().trim_start();
            let member = before.ends_with('.')
                || before.ends_with("->")
                || before.ends_with("::")
                || after.starts_with("::");
            out.push((t.get(s..i).unwrap_or_default().to_string(), s, member));
            continue;
        }
        if c.is_ascii_digit() {
            while at(i).is_some_and(|x| x.is_ascii_alphanumeric() || x == b'.') {
                i += 1;
            }
            continue;
        }
        i += 1;
    }
    out
}

fn splice(text: &str, hits: &[usize], n: usize, new: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut last = 0usize;
    for &at in hits {
        out.push_str(text.get(last..at).unwrap_or_default());
        out.push_str(new);
        last = at.saturating_add(n);
    }
    out.push_str(text.get(last..).unwrap_or_default());
    out
}

pub fn rename_in(
    text: &str,
    from: usize,
    to: usize,
    old: &str,
    new: &str,
) -> std::result::Result<String, String> {
    let lines: Vec<&str> = text.split('\n').collect();
    let (a, b) = (from.saturating_sub(1), to.min(lines.len()));
    if a >= b {
        return Err(format!("lines {from}..{to} are not in the file"));
    }
    let region = lines.get(a..b).unwrap_or_default().join("\n");
    let words = code_words(&region);
    if words.iter().any(|(w, _, _)| w == new) {
        return Err(format!("`{new}` already appears in lines {from}..{to}"));
    }
    let hits: Vec<usize> = words
        .iter()
        .filter(|(w, _, member)| w == old && !member)
        .map(|(_, at, _)| *at)
        .collect();
    if hits.is_empty() {
        return Err(format!("`{old}` does not appear in lines {from}..{to}"));
    }
    let out = splice(&region, &hits, old.len(), new);
    let mut all: Vec<String> = lines
        .get(..a)
        .unwrap_or_default()
        .iter()
        .map(|s| (*s).to_string())
        .collect();
    all.push(out);
    all.extend(
        lines
            .get(b..)
            .unwrap_or_default()
            .iter()
            .map(|s| (*s).to_string()),
    );
    Ok(all.join("\n"))
}

#[must_use]
pub fn rename_all(text: &str, old: &str, new: &str) -> Option<String> {
    let hits: Vec<usize> = code_words(text)
        .into_iter()
        .filter(|(w, _, _)| w == old)
        .map(|(_, at, _)| at)
        .collect();
    if hits.is_empty() {
        return None;
    }
    Some(splice(text, &hits, old.len(), new))
}

#[must_use]
pub fn read_except(text: &str) -> BTreeSet<String> {
    let mut out = BTreeSet::new();
    for l in text.lines().map(str::trim) {
        if let Some((_, rest)) = l.split_once("error C2248: '") {
            let Some((m, rest)) = rest.split_once('\'') else {
                continue;
            };
            if let Some((_, c)) = rest.split_once("declared in class '")
                && let Some((c, _)) = c.split_once('\'')
            {
                out.insert(format!("{c}::{m}"));
            }
            continue;
        }
        if !l.is_empty() && !l.starts_with('#') && !l.contains(' ') {
            out.insert(l.to_string());
        }
    }
    out
}

pub fn apply_fixes(
    root: &Path,
    rows: &[Row],
    kind: &str,
    protected: &BTreeSet<PathBuf>,
) -> Result<Applied> {
    let mut fixes: Vec<&Fix> = rows
        .iter()
        .filter(|r| r.kind == kind)
        .filter_map(|r| r.fix.as_ref())
        .collect();
    fixes.sort();
    fixes.dedup();
    let mut by_file: BTreeMap<&Path, Vec<&Fix>> = BTreeMap::new();
    for f in fixes {
        by_file.entry(f.file.as_path()).or_default().push(f);
    }
    let mut out = Applied::default();
    for (file, fs) in by_file {
        if protected.contains(file) {
            for f in fs {
                out.refused.push(format!(
                    "{}:{}: {} -> {}: another view reads this file",
                    file.display(),
                    f.from,
                    f.old,
                    f.new
                ));
            }
            continue;
        }
        let path = root.join(file);
        let mut text = srcio::read(&path).with_context(|| format!("reading {}", path.display()))?;
        let mut changed = false;
        for f in fs {
            match rename_in(&text, f.from, f.to, &f.old, &f.new) {
                Ok(t) => {
                    text = t;
                    out.count += 1;
                    out.changed.push(format!(
                        "{}:{}: {} -> {}",
                        file.display(),
                        f.from,
                        f.old,
                        f.new
                    ));
                    changed = true;
                }
                Err(e) => out.refused.push(format!(
                    "{}:{}: {} -> {}: {e}",
                    file.display(),
                    f.from,
                    f.old,
                    f.new
                )),
            }
        }
        if changed {
            srcio::write(&path, &text).with_context(|| format!("writing {}", path.display()))?;
        }
    }
    Ok(out)
}

fn declarations(srcs: &[&Src]) -> BTreeMap<String, BTreeSet<String>> {
    let mut d: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut add = |n: &str, owner: String| {
        d.entry(leaf(n).to_string()).or_default().insert(owner);
    };
    for s in srcs {
        for c in &s.classes {
            add(&c.name, format!("class {}", c.name));
            for m in &c.members {
                add(&m.name, format!("member {}::{}", c.name, m.name));
            }
        }
        for e in &s.enums {
            add(&e.name, format!("enum {}", e.name));
            for (n, _) in &e.values {
                add(n, format!("enumerator {n}"));
            }
        }
        for t in s.typedefs.keys() {
            add(t, format!("typedef {t}"));
        }
        for f in &s.defs {
            if !f.name.contains("::") {
                add(&f.name, format!("function {}", f.name));
            }
        }
    }
    d
}

#[must_use]
pub fn declaration_count(srcs: &[&Src], name: &str) -> usize {
    declarations(srcs).get(name).map_or(0, BTreeSet::len)
}

pub fn apply_renames(
    root: &Path,
    renames: &[FieldRename],
    srcs: &[&Src],
    protected: &BTreeSet<PathBuf>,
) -> Result<Applied> {
    let decl = declarations(srcs);
    let files: BTreeSet<&PathBuf> = srcs.iter().flat_map(|s| s.files.iter()).collect();
    let mut out = Applied::default();
    for f in renames {
        let why = |w: &str| format!("{}: {} -> {}: {w}", f.class, f.old, f.new);
        if protected.contains(&f.file) {
            out.refused.push(why("another view reads its header"));
            continue;
        }
        let n_old = decl.get(&f.old).map_or(0, BTreeSet::len);
        if n_old != 1 {
            out.refused.push(why(&format!(
                "`{}` names {n_old} declaration(s) in the product",
                f.old
            )));
            continue;
        }
        if decl.contains_key(&f.new) {
            out.refused.push(why(&format!(
                "`{}` is already declared in the product",
                f.new
            )));
            continue;
        }
        let mut texts: Vec<(PathBuf, String)> = Vec::new();
        let mut capture = None;
        for file in &files {
            let path = root.join(file);
            let text = srcio::read(&path).with_context(|| format!("reading {}", path.display()))?;
            let words = code_words(&text);
            if !words.iter().any(|(w, _, _)| *w == f.old) {
                continue;
            }
            if words.iter().any(|(w, _, _)| *w == f.new) {
                capture = Some(file.display().to_string());
                break;
            }
            texts.push((path, text));
        }
        if let Some(c) = capture {
            out.refused
                .push(why(&format!("`{}` already appears in {c}", f.new)));
            continue;
        }
        for (path, text) in texts {
            if let Some(t) = rename_all(&text, &f.old, &f.new) {
                srcio::write(&path, &t).with_context(|| format!("writing {}", path.display()))?;
            }
        }
        out.changed
            .push(format!("{}: {} -> {}", f.class, f.old, f.new));
        out.count += 1;
    }
    Ok(out)
}

pub fn apply_plans(
    root: &Path,
    plans: &[ClassPlan],
    access: bool,
    order: bool,
    protected: &BTreeSet<PathBuf>,
    except: &BTreeSet<String>,
) -> Result<Applied> {
    let mut by_file: BTreeMap<&Path, Vec<&ClassPlan>> = BTreeMap::new();
    for p in plans {
        by_file.entry(p.file.as_path()).or_default().push(p);
    }
    let mut out = Applied::default();
    for (file, mut ps) in by_file {
        if protected.contains(file) {
            for p in ps {
                out.refused.push(format!(
                    "{}:{}: {}: another view reads this file",
                    file.display(),
                    p.line,
                    p.class
                ));
            }
            continue;
        }
        let path = root.join(file);
        let mut text = srcio::read(&path).with_context(|| format!("reading {}", path.display()))?;
        let mut rewritten: Vec<usize> = Vec::new();
        ps.sort_by_key(|p| std::cmp::Reverse(p.line));
        for p in ps {
            let last = p.members.iter().map(|m| m.end).max().unwrap_or(p.line);
            if rewritten.iter().any(|&l| l > p.line && l <= last) {
                out.refused.push(format!(
                    "{}:{}: {}: holds a nested class rewritten in this run (apply again)",
                    file.display(),
                    p.line,
                    p.class
                ));
                continue;
            }
            match rewrite_class(&text, p, access, order, except) {
                Ok((t, ch)) => {
                    if !ch.is_empty() {
                        rewritten.push(p.line);
                    }
                    for c in &ch {
                        out.changed
                            .push(format!("{}:{}: {c}", file.display(), p.line));
                    }
                    out.count += ch.len();
                    text = t;
                }
                Err(e) => {
                    out.refused
                        .push(format!("{}:{}: {}: {e}", file.display(), p.line, p.class))
                }
            }
        }
        if !rewritten.is_empty() {
            srcio::write(&path, &text).with_context(|| format!("writing {}", path.display()))?;
        }
    }
    Ok(out)
}

#[derive(Debug, Clone)]
struct Unit {
    first: usize,
    last: usize,
    blanks: usize,
    access: Access,
    want: Option<Access>,
    rec: Option<usize>,
    movable: bool,
    slots: bool,
    friend: bool,
    nested_name: Option<String>,
    subject: String,
}

fn depths(lines: &[&str], from: usize, to: usize) -> Vec<(i32, i32)> {
    let mut d = 0i32;
    let mut out = Vec::new();
    for l in lines.get(from..to).unwrap_or_default() {
        let start = d;
        let b = l.as_bytes();
        let mut i = 0;
        while let Some(&c) = b.get(i) {
            match c {
                b'"' | b'\'' => {
                    i += 1;
                    while b.get(i).is_some_and(|x| *x != c) {
                        if b.get(i) == Some(&b'\\') {
                            i += 1;
                        }
                        i += 1;
                    }
                }
                b'/' if b.get(i + 1) == Some(&b'/') => break,
                b'{' => d = d.saturating_add(1),
                b'}' => d = d.saturating_sub(1),
                _ => {}
            }
            i += 1;
        }
        out.push((start, d));
    }
    out
}

fn is_label(l: &str) -> bool {
    matches!(
        l.trim().replace(' ', "").as_str(),
        "public:" | "private:" | "protected:"
    )
}

fn body(lines: &[&str], line: usize) -> std::result::Result<(usize, usize), String> {
    let start = line.saturating_sub(1);
    let open = (start..lines.len())
        .find(|&i| lines.get(i).is_some_and(|l| l.contains('{')))
        .ok_or("no `{` after the class line")?;
    if !lines
        .get(open)
        .and_then(|l| l.split_once('{'))
        .is_some_and(|(_, after)| after.trim().is_empty())
    {
        return Err(format!("line {}: the `{{` line holds more", open + 1));
    }
    let ds = depths(lines, open, lines.len());
    let close = ds
        .iter()
        .enumerate()
        .skip(1)
        .find(|(_, (s, e))| *s >= 1 && *e == 0)
        .map(|(k, _)| open + k)
        .ok_or("no closing `}`")?;
    if !lines
        .get(close)
        .is_some_and(|l| l.trim_start().starts_with('}'))
    {
        return Err(format!("line {}: the `}}` line holds more", close + 1));
    }
    Ok((open, close))
}

pub fn rewrite_class(
    text: &str,
    plan: &ClassPlan,
    access: bool,
    order: bool,
    except: &BTreeSet<String>,
) -> std::result::Result<(String, Vec<String>), String> {
    let lines: Vec<&str> = text.split('\n').collect();
    let (open, close) = body(&lines, plan.line)?;
    let ds = depths(&lines, open + 1, close);
    let ds_at = |i: usize| i.checked_sub(open + 1).and_then(|k| ds.get(k)).copied();
    let depth_at = |i: usize| ds_at(i).map_or(-1, |d| d.0);
    let depth_end = |i: usize| ds_at(i).map_or(-1, |d| d.1);
    let mut spans: Vec<(usize, usize, &PlanMember)> = Vec::new();
    for m in &plan.members {
        let (mut first, mut last) = (
            m.line.saturating_sub(1),
            m.end.max(m.line).saturating_sub(1),
        );
        if first <= open || last >= close {
            return Err(format!("line {}: a member outside the body", first + 1));
        }
        while first > open + 1 && depth_at(first) != 0 {
            first -= 1;
        }
        while last + 1 < close && depth_end(last) != 0 {
            last += 1;
        }
        if depth_at(first) != 0 || depth_end(last) != 0 {
            return Err(format!(
                "line {}: a member inside a nested block",
                first + 1
            ));
        }
        spans.push((first, last, m));
    }
    spans.sort_by_key(|s| (s.0, s.1));
    let mut groups: Vec<(usize, usize, Vec<&PlanMember>)> = Vec::new();
    for (f, l, m) in spans {
        match groups.last_mut() {
            Some(g) if f <= g.1 => {
                g.1 = g.1.max(l);
                g.2.push(m);
            }
            _ => groups.push((f, l, vec![m])),
        }
    }
    let mut units: Vec<Unit> = Vec::new();
    for (first, last, ms) in groups {
        let Some(m0) = ms.first().copied() else {
            continue;
        };
        let wants: BTreeSet<Access> = ms
            .iter()
            .filter(|m| !except.contains(&m.subject) && !except.contains(&m.ours))
            .filter_map(|m| m.want)
            .collect();
        let blocked = ms
            .iter()
            .any(|m| except.contains(&m.subject) || except.contains(&m.ours));
        units.push(Unit {
            first,
            last,
            blanks: 0,
            access: m0.access,
            want: if wants.len() == 1 && !blocked {
                wants.into_iter().next()
            } else {
                None
            },
            rec: ms.iter().filter_map(|m| m.rec).min(),
            movable: ms.len() == 1 && m0.movable && !blocked,
            slots: ms.len() == 1 && m0.slots,
            friend: ms.iter().all(|m| m.friend),
            nested_name: ms
                .iter()
                .find(|m| m.nested)
                .map(|m| m.ours.rsplit("::").next().unwrap_or("").to_string()),
            subject: m0.subject.clone(),
        });
    }
    for u in &mut units {
        while u.first > open + 1 && {
            let t = lines.get(u.first - 1).map_or("", |l| l.trim_start());
            t.starts_with("//") || t.starts_with("/*")
        } {
            u.first -= 1;
        }
    }
    for w in units.windows(2) {
        if let [a, b] = w
            && b.first <= a.last
        {
            return Err(format!("line {}: two members share a line", b.first + 1));
        }
    }
    let mut owner: Vec<Option<usize>> = vec![None; close.saturating_sub(open + 1)];
    for (k, u) in units.iter().enumerate() {
        for i in u.first..=u.last {
            if let Some(o) = i.checked_sub(open + 1).and_then(|j| owner.get_mut(j)) {
                *o = Some(k);
            }
        }
    }
    let owned = |i: usize| {
        i.checked_sub(open + 1)
            .and_then(|j| owner.get(j))
            .copied()
            .flatten()
    };
    let mut lead_blanks = 0usize;
    for i in open + 1..close {
        let l = lines.get(i).copied().unwrap_or_default();
        if owned(i).is_some() {
            continue;
        }
        if l.trim_start().starts_with('#') {
            return Err(format!("line {}: a preprocessor line in the body", i + 1));
        }
        if l.trim().is_empty() {
            match (open + 1..i).rev().find_map(|j| {
                owned(j).map(Some).or_else(|| {
                    (!lines.get(j).copied().unwrap_or_default().trim().is_empty()).then_some(None)
                })
            }) {
                Some(Some(k)) => {
                    if let Some(u) = units.get_mut(k) {
                        u.blanks += 1;
                    }
                }
                Some(None) => {}
                None => lead_blanks += 1,
            }
            continue;
        }
        if !is_label(l) {
            return Err(format!("line {}: a body line no member owns", i + 1));
        }
    }
    let indent = |l: &str| l.len().saturating_sub(l.trim_start().len());
    let label_indent = (open + 1..close)
        .find(|&i| lines.get(i).is_some_and(|l| is_label(l)))
        .and_then(|i| lines.get(i))
        .map(|l| indent(l))
        .unwrap_or_else(|| lines.get(open).map_or(0, |l| indent(l)));
    let mut seq: Vec<usize> = (0..units.len()).collect();
    if order {
        let names: Vec<Option<String>> = units.iter().map(|u| u.nested_name.clone()).collect();
        for u in &mut units {
            if !u.movable {
                continue;
            }
            let text: String = lines.get(u.first..=u.last).unwrap_or_default().join(" ");
            let words: BTreeSet<&str> = text
                .split(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
                .collect();
            if names
                .iter()
                .any(|n| n.as_deref().is_some_and(|x| words.contains(x)))
            {
                u.movable = false;
            }
        }
        let unit = |k: usize| units.get(k);
        let mut movers: Vec<usize> = (0..units.len())
            .filter(|&k| unit(k).is_some_and(|u| u.movable))
            .collect();
        movers.sort_by_key(|&k| unit(k).and_then(|u| u.rec));
        let chain = |slots: bool| -> Vec<(usize, Option<usize>)> {
            let mut last = None;
            seq.iter()
                .copied()
                .filter(|k| unit(*k).is_some_and(|u| !u.movable && u.slots == slots))
                .map(|k| {
                    if let Some(r) = unit(k).and_then(|u| u.rec) {
                        last = Some(r);
                    }
                    (k, last)
                })
                .collect()
        };
        let (a, b) = (chain(false), chain(true));
        let (mut i, mut j) = (0, 0);
        let mut merged = Vec::with_capacity(a.len() + b.len());
        while i < a.len() || j < b.len() {
            match (a.get(i), b.get(j)) {
                (Some(x), Some(y)) if x.1 <= y.1 => {
                    merged.push(x.0);
                    i += 1;
                }
                (Some(x), None) => {
                    merged.push(x.0);
                    i += 1;
                }
                (_, Some(y)) => {
                    merged.push(y.0);
                    j += 1;
                }
                (None, None) => break,
            }
        }
        seq = merged;
        for m in movers {
            let r = unit(m).and_then(|u| u.rec);
            let rec_of = |k: &usize| unit(*k).and_then(|u| u.rec);
            let before = seq
                .iter()
                .enumerate()
                .filter(|(_, k)| rec_of(k).is_some() && rec_of(k) < r)
                .max_by_key(|(_, k)| rec_of(k))
                .map(|(p, _)| p);
            let at = match before {
                Some(p) => p + 1,
                None => seq
                    .iter()
                    .position(|k| rec_of(k).is_some())
                    .unwrap_or(seq.len()),
            };
            seq.insert(at.min(seq.len()), m);
        }
    }
    let level = |u: &Unit| {
        if access {
            u.want.unwrap_or(u.access)
        } else {
            u.access
        }
    };
    let mut changed: Vec<String> = Vec::new();
    for (pos, &k) in seq.iter().enumerate() {
        let Some(u) = units.get(k) else { continue };
        let moved = if u.movable {
            pos.checked_sub(1).and_then(|p| seq.get(p).copied()) != k.checked_sub(1)
        } else {
            let before = |s: &[usize]| {
                s.iter()
                    .copied()
                    .rfind(|j| units.get(*j).is_some_and(|x| !x.movable))
            };
            let was: Vec<usize> = (0..k).collect();
            before(seq.get(..pos).unwrap_or_default()) != before(&was)
        };
        if moved || level(u) != u.access {
            changed.push(u.subject.clone());
        }
    }
    if changed.is_empty() {
        return Ok((text.to_string(), changed));
    }
    let default = if plan.key == Key::Class {
        Access::Private
    } else {
        Access::Public
    };
    let mut out: Vec<String> = lines
        .get(..=open)
        .unwrap_or_default()
        .iter()
        .map(|s| (*s).to_string())
        .collect();
    out.extend(std::iter::repeat_n(String::new(), lead_blanks));
    let mut cur = default;
    for &k in &seq {
        let Some(u) = units.get(k) else { continue };
        let lvl = level(u);
        if !u.friend && lvl != cur {
            if out.len() > open + 1 && out.last().is_some_and(|l| !l.trim().is_empty()) {
                out.push(String::new());
            }
            out.push(format!("{}{}:", " ".repeat(label_indent), lvl.word()));
            cur = lvl;
        }
        out.extend(
            lines
                .get(u.first..=u.last)
                .unwrap_or_default()
                .iter()
                .map(|s| (*s).to_string()),
        );
        out.extend(std::iter::repeat_n(String::new(), u.blanks));
    }
    while out.len() > open + 1 && out.last().is_some_and(|l| l.trim().is_empty()) {
        out.pop();
    }
    out.extend(
        lines
            .get(close..)
            .unwrap_or_default()
            .iter()
            .map(|s| (*s).to_string()),
    );
    Ok((out.join("\n"), changed))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pm(
        subject: &str,
        line: usize,
        access: Access,
        want: Option<Access>,
        rec: Option<usize>,
        movable: bool,
    ) -> PlanMember {
        PlanMember {
            subject: subject.into(),
            ours: subject.into(),
            line,
            end: line,
            access,
            want,
            rec,
            movable,
            slots: false,
            friend: false,
            nested: false,
        }
    }

    const TEXT: &str = "class C {\npublic:\n    int m_a;\n    void g();\n\n    void f();\n    virtual void v();\n    int m_b;\n};\n";

    fn plan() -> ClassPlan {
        use Access::{Private, Public};
        ClassPlan {
            class: "C".into(),
            file: PathBuf::from("x.h"),
            line: 1,
            key: Key::Class,
            members: vec![
                pm("C::m_a", 3, Public, Some(Private), Some(0), false),
                pm("C::g", 4, Public, Some(Public), Some(3), true),
                pm("C::f", 6, Public, Some(Public), Some(1), true),
                pm("C::v", 7, Public, Some(Public), Some(2), false),
                pm("C::m_b", 8, Public, Some(Private), Some(4), false),
            ],
        }
    }

    #[test]
    fn access_rewrites_the_labels_only() {
        let (t, ch) = rewrite_class(TEXT, &plan(), true, false, &BTreeSet::new()).unwrap();
        assert_eq!(
            t,
            "class C {\n    int m_a;\n\npublic:\n    void g();\n\n    void f();\n    virtual void v();\n\nprivate:\n    int m_b;\n};\n"
        );
        assert_eq!(ch, ["C::m_a", "C::m_b"]);
        let ex = BTreeSet::from(["C::m_a".to_string()]);
        let (t, _) = rewrite_class(TEXT, &plan(), true, false, &ex).unwrap();
        assert!(t.starts_with("class C {\npublic:\n    int m_a;\n"), "{t}");
    }

    #[test]
    fn order_moves_the_non_virtual_methods_only() {
        let (t, ch) = rewrite_class(TEXT, &plan(), false, true, &BTreeSet::new()).unwrap();
        assert_eq!(
            t,
            "class C {\npublic:\n    int m_a;\n    void f();\n    virtual void v();\n    void g();\n\n    int m_b;\n};\n"
        );
        assert_eq!(ch, ["C::f", "C::g"]);
    }

    #[test]
    fn shared_statements_are_one_unit_and_annotations_go_with_their_member() {
        use Access::{Private, Public};
        let text = "class C {\npublic:\n    int m_a, m_b;\n    struct F {\n        int x : 1;\n    } m_f;\n    // FUNCTION: DLL 0x10001000\n    void g() { }\n    void f();\n};\n";
        let mut a = pm("C::m_a", 3, Public, Some(Private), Some(0), false);
        let b = pm("C::m_b", 3, Public, Some(Private), Some(1), false);
        let mut nested = pm("C::F", 4, Public, None, None, false);
        nested.end = 6;
        nested.nested = true;
        let f_field = pm("C::m_f", 6, Public, Some(Private), Some(2), false);
        let mut p = plan();
        p.members = vec![
            a.clone(),
            b,
            nested,
            f_field,
            pm("C::g", 8, Public, Some(Public), Some(4), true),
            pm("C::f", 9, Public, Some(Public), Some(3), true),
        ];
        let (t, _) = rewrite_class(text, &p, true, true, &BTreeSet::new()).unwrap();
        assert_eq!(
            t,
            "class C {\n    int m_a, m_b;\n    struct F {\n        int x : 1;\n    } m_f;\n\npublic:\n    void f();\n    // FUNCTION: DLL 0x10001000\n    void g() { }\n};\n"
        );
        a.want = Some(Public);
        p.members[0] = a;
        let (t, _) = rewrite_class(text, &p, true, false, &BTreeSet::new()).unwrap();
        assert!(
            t.starts_with("class C {\npublic:\n    int m_a, m_b;\n\nprivate:\n    struct F {"),
            "{t}"
        );
    }

    #[test]
    fn the_slot_chain_keeps_its_order_and_meets_the_fields_where_the_record_does() {
        use Access::Public;
        let text = "class C {\npublic:\n    virtual void v1();\n    int m_a;\n    virtual void v2();\n    int m_b;\n    virtual void o();\n};\n";
        let mut v1 = pm("C::v1", 3, Public, Some(Public), Some(2), false);
        let mut v2 = pm("C::v2", 5, Public, Some(Public), Some(3), false);
        v1.slots = true;
        v2.slots = true;
        let mut p = plan();
        p.members = vec![
            v1,
            pm("C::m_a", 4, Public, Some(Public), Some(0), false),
            v2,
            pm("C::m_b", 6, Public, Some(Public), Some(1), false),
            pm("C::o", 7, Public, Some(Public), Some(4), true),
        ];
        let (t, _) = rewrite_class(text, &p, false, true, &BTreeSet::new()).unwrap();
        assert_eq!(
            t,
            "class C {\npublic:\n    int m_a;\n    int m_b;\n    virtual void v1();\n    virtual void v2();\n    virtual void o();\n};\n"
        );
    }

    #[test]
    fn a_body_the_rewrite_cannot_own_is_refused() {
        let pp = "class C {\npublic:\n#ifdef X\n    int m_a;\n#endif\n};\n";
        let mut p = plan();
        p.members.truncate(1);
        p.members[0].line = 4;
        p.members[0].end = 4;
        assert!(rewrite_class(pp, &p, true, false, &BTreeSet::new()).is_err());
        let stray = "class C {\npublic:\n    int m_a;\n    stray;\n};\n";
        p.members[0].line = 3;
        p.members[0].end = 3;
        assert!(rewrite_class(stray, &p, true, false, &BTreeSet::new()).is_err());
        p.members[0].want = Some(Access::Public);
        let (t, ch) = rewrite_class(
            "class C {\npublic:\n    int m_a;\n};\n",
            &p,
            true,
            true,
            &BTreeSet::new(),
        )
        .unwrap();
        assert!(ch.is_empty());
        assert_eq!(t, "class C {\npublic:\n    int m_a;\n};\n");
    }

    #[test]
    fn a_rename_takes_the_uses_and_refuses_a_capture() {
        let t = "int x;\nvoid Dev::set(int v) {\n    m.v = v; // v\n    p->v = v + Dev::v;\n    s(\"v\");\n}\nint v;\n";
        let got = rename_in(t, 2, 6, "v", "vol").unwrap();
        assert_eq!(
            got,
            "int x;\nvoid Dev::set(int vol) {\n    m.v = vol; // v\n    p->v = vol + Dev::v;\n    s(\"v\");\n}\nint v;\n"
        );
        assert!(rename_in(t, 2, 6, "v", "m").is_err(), "captures `m`");
        assert!(
            rename_in(t, 1, 1, "v", "w").is_err(),
            "`v` is not on line 1"
        );
        assert_eq!(
            rename_all(
                "x->m_bits = a.m_bits + C::m_bits; s(\"m_bits\");",
                "m_bits",
                "m_flags"
            )
            .unwrap(),
            "x->m_flags = a.m_flags + C::m_flags; s(\"m_bits\");"
        );
        assert!(rename_all("int y;", "m_bits", "m_flags").is_none());
    }

    #[test]
    fn an_except_list_reads_names_and_c2248_errors() {
        let t = "# kept\nWave::m_pitch\nC:\\w\\src\\x.cpp(290) : error C2248: 'm_info' : cannot access private member declared in class 'Buffer'\n        include\\file.h(41) : see declaration of 'm_info'\n";
        assert_eq!(
            read_except(t),
            BTreeSet::from(["Wave::m_pitch".to_string(), "Buffer::m_info".to_string()])
        );
    }
}
