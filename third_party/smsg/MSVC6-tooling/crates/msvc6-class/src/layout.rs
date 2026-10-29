use crate::decl;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const PAD_MIN: u32 = 16;
pub const PACK: u32 = 8;
pub const LISTED_AGREE: usize = 10;
pub const LISTED_CAUSES: usize = 8;

fn is_placeholder(cls: &str, placeholders: &[String]) -> bool {
    placeholders
        .iter()
        .any(|p| msvc6_source::glob::whole(p, cls))
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Field {
    pub off: u32,
    pub size: u32,
    pub name: String,
    pub pad: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Layout {
    pub fields: Vec<Field>,
    pub unknown: Option<String>,
}

#[must_use]
pub fn scalar_size(ty: &str) -> Option<u32> {
    Some(match ty {
        "char" | "signed char" | "unsigned char" | "BYTE" | "bool" => 1,
        "short" | "unsigned short" | "signed short" | "WORD" | "USHORT" | "SHORT" => 2,
        "int" | "unsigned" | "unsigned int" | "signed int" | "long" | "unsigned long"
        | "signed long" | "float" | "DWORD" | "LONG" | "ULONG" | "UINT" | "INT" | "BOOL"
        | "COLORREF" | "LRESULT" | "WPARAM" | "LPARAM" | "size_t" | "time_t" => 4,
        "double" | "__int64" | "unsigned __int64" | "LONGLONG" | "DWORDLONG" => 8,
        "CRITICAL_SECTION" | "CRIT_SECT" | "RTL_CRITICAL_SECTION" => 24,
        _ => return None,
    })
}

#[must_use]
pub fn align_to(off: u32, align: u32) -> u32 {
    let a = align.clamp(1, PACK);
    off.div_ceil(a) * a
}

fn opens_plain(line: &str) -> Option<&str> {
    let rest = line
        .strip_prefix("class ")
        .or_else(|| line.strip_prefix("struct "))?;
    let head = rest.split('{').next().unwrap_or_default().trim();
    if head.contains(':') || head.is_empty() || !rest.contains('{') {
        return None;
    }
    head.chars()
        .all(|c| c.is_alphanumeric() || c == '_')
        .then_some(head)
}

fn split_decl(stmt: &str) -> Option<(String, Vec<String>)> {
    let mut parts = stmt.split(',');
    let first = parts.next()?.trim();
    let upto = first.find('[').unwrap_or(first.len());
    let cut = first.get(..upto)?.rfind(char::is_whitespace)?;
    let (ty, d0) = first.split_at(cut);
    let mut decls = vec![d0.trim().to_string()];
    for p in parts {
        decls.push(p.trim().to_string());
    }
    Some((ty.trim().to_string(), decls))
}

#[must_use]
pub fn extent(ext: &str) -> Option<u32> {
    let num = |t: &str| -> Option<u32> {
        let t = t.trim();
        t.strip_prefix("0x").map_or_else(
            || t.parse::<u32>().ok(),
            |h| u32::from_str_radix(h, 16).ok(),
        )
    };
    if let Some((a, b)) = ext.split_once('-') {
        return num(a)?.checked_sub(num(b)?);
    }
    if let Some((a, b)) = ext.split_once('+') {
        return num(a)?.checked_add(num(b)?);
    }
    num(ext)
}

fn declarator(ty: &str, d: &str) -> Option<(u32, u32, String)> {
    let ptr = ty.ends_with('*') || d.starts_with('*');
    let ty = ty.trim_end_matches('*').trim();
    let d = d.trim_start_matches('*').trim();
    let (name, count) = match d.split_once('[') {
        None => (d.to_string(), 1u32),
        Some((nm, rest)) => (nm.trim().to_string(), extent(rest.trim_end_matches(']'))?),
    };
    if name.is_empty() || !name.chars().all(|c| c.is_alphanumeric() || c == '_') {
        return None;
    }
    let elem = if ptr { 4 } else { scalar_size(ty)? };
    Some((elem, count, name))
}

fn at_class_level(line: &str) -> String {
    let mut out = String::new();
    let mut depth = 0u32;
    for c in line.chars() {
        match c {
            '{' => depth += 1,
            '}' => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    out.push(';');
                }
            }
            _ if depth == 0 => out.push(c),
            _ => {}
        }
    }
    out
}

#[must_use]
pub fn encodes_own_offset(name: &str, off: u32) -> bool {
    let lower = name.to_ascii_lowercase();
    if lower.starts_with("pad") || matches!(lower.as_str(), "unknown" | "reserved" | "unused") {
        return true;
    }
    let tail = name.trim_start_matches(|c: char| c.is_alphabetic() || c == '_');
    if tail.is_empty() || !tail.chars().all(|c| c.is_ascii_hexdigit()) {
        return false;
    }
    u32::from_str_radix(tail, 16).is_ok_and(|v| v == off)
}

const DECLINES: [(&str, &str); 7] = [
    ("virtual", "a `virtual' puts a vptr at +0"),
    ("union", "a union overlays members"),
    ("struct", "a nested aggregate has unknown size"),
    ("class", "a nested aggregate has unknown size"),
    ("enum", "a nested enum"),
    ("typedef", "a typedef"),
    ("static", "a static member is not storage"),
];

fn derive(lines: &[&str], start: usize) -> (Layout, usize) {
    let mut fields: Vec<Field> = Vec::new();
    let mut unknown: Option<String> = None;
    let mut off = 0u32;
    let mut depth = 0usize;
    let mut i = start;
    while let Some(&line) = lines.get(i) {
        let opened = line.matches('{').count();
        let closed = line.matches('}').count();
        let level = if i == start { 1 } else { depth };
        if level == 1 {
            let after = if i == start {
                line.split_once('{').map_or(line, |(_, r)| r)
            } else {
                line
            };
            for raw in at_class_level(after).split(';') {
                let stmt = raw.trim();
                if stmt.is_empty() || matches!(stmt, "public:" | "private:" | "protected:") {
                    continue;
                }
                let s = stmt
                    .trim_start_matches("public:")
                    .trim_start_matches("private:")
                    .trim_start_matches("protected:")
                    .trim();
                if s.is_empty() {
                    continue;
                }
                if let Some((_, why)) = DECLINES
                    .iter()
                    .find(|(k, _)| s.split(|c: char| !c.is_alphanumeric()).any(|w| w == *k))
                {
                    unknown = Some((*why).to_string());
                    continue;
                }
                if s.contains(':') {
                    unknown = Some("a bitfield or a label".to_string());
                    continue;
                }
                if s.contains('(') {
                    continue;
                }
                match split_decl(s).and_then(|(ty, ds)| {
                    ds.iter()
                        .map(|d| declarator(&ty, d))
                        .collect::<Option<Vec<_>>>()
                }) {
                    None => unknown = Some(format!("cannot size `{s}'")),
                    Some(ds) => {
                        for (elem, count, name) in ds {
                            off = align_to(off, elem);
                            let size = elem.saturating_mul(count);
                            let pad =
                                elem == 1 && (size >= PAD_MIN || encodes_own_offset(&name, off));
                            fields.push(Field {
                                off,
                                size,
                                name,
                                pad,
                            });
                            off = off.saturating_add(size);
                        }
                    }
                }
            }
        }
        depth = depth.saturating_add(opened).saturating_sub(closed);
        i += 1;
        if depth == 0 && i > start {
            break;
        }
    }
    (Layout { fields, unknown }, i)
}

#[must_use]
pub fn declarations(text: &str) -> Vec<(String, usize, Layout)> {
    let stripped = decl::uncomment(text.as_bytes());
    let lines: Vec<&str> = stripped.lines().collect();
    let mut out = Vec::new();
    let mut i = 0;
    while i < lines.len() {
        if let Some(name) = lines.get(i).and_then(|l| opens_plain(l)) {
            let name = name.to_string();
            let (lay, next) = derive(&lines, i);
            out.push((name, i + 1, lay));
            i = next.max(i + 1);
        } else {
            i += 1;
        }
    }
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Verdict {
    Agree,
    Unnamed,
    Opaque,
    Split,
    NameClash,
    Layout,
}

fn tiles(lay: &Layout, start: u32, span: u32) -> bool {
    let mut at = start;
    let end = start.saturating_add(span);
    while at < end {
        match lay.fields.iter().find(|f| f.off == at) {
            Some(f) if f.size > 0 => at = at.saturating_add(f.size),
            _ => return false,
        }
    }
    at == end
}

#[must_use]
pub fn compare(a: &Layout, b: &Layout) -> (Verdict, Option<(u32, String, String)>) {
    let mut worst = (Verdict::Agree, None);
    for fa in &a.fields {
        for fb in &b.fields {
            if fa.pad || fb.pad {
                let overlap = fa.off < fb.off.saturating_add(fb.size)
                    && fb.off < fa.off.saturating_add(fa.size);
                if overlap && fa.pad != fb.pad && worst.0 < Verdict::Opaque {
                    let (p, n) = if fa.pad { (fa, fb) } else { (fb, fa) };
                    worst = (
                        Verdict::Opaque,
                        Some((
                            n.off,
                            format!("{}[{}] opaque", p.name, p.size),
                            n.name.clone(),
                        )),
                    );
                }
                continue;
            }
            if fa.off != fb.off {
                continue;
            }
            let at = || {
                Some((
                    fa.off,
                    format!("{}:{}", fa.name, fa.size),
                    format!("{}:{}", fb.name, fb.size),
                ))
            };
            if fa.size != fb.size {
                let (big, small) = if fa.size > fb.size {
                    (fa.size, b)
                } else {
                    (fb.size, a)
                };
                if tiles(small, fa.off, big) {
                    if Verdict::Split > worst.0 {
                        worst = (Verdict::Split, at());
                    }
                    continue;
                }
                return (Verdict::Layout, at());
            }
            if fa.name == fb.name {
                continue;
            }
            let v = if encodes_own_offset(&fa.name, fa.off) || encodes_own_offset(&fb.name, fb.off)
            {
                Verdict::Unnamed
            } else {
                Verdict::NameClash
            };
            if v > worst.0 {
                worst = (v, at());
            }
        }
    }
    worst
}

#[derive(Debug, Clone, Serialize)]
pub struct Row {
    pub declared: bool,
    pub class: String,
    pub verdict: Verdict,
    pub copies: usize,
    pub declined: usize,
    pub evidence: String,
}

#[derive(Debug, Clone, Default, Serialize)]
pub struct Report {
    pub files: usize,
    pub classes: usize,
    pub single: usize,
    pub placeholders: usize,
    pub all_declined: usize,
    pub rows: Vec<Row>,
    pub declines: Vec<(String, usize)>,
}

#[must_use]
pub fn screen(
    copies: &[(String, String)],
    declared: &BTreeSet<String>,
    placeholders: &[String],
) -> Report {
    let mut seen: BTreeMap<String, Vec<(String, Layout)>> = BTreeMap::new();
    for (file, text) in copies {
        let base = file.rsplit('/').next().unwrap_or(file);
        for (cls, line, lay) in declarations(text) {
            seen.entry(cls)
                .or_default()
                .push((format!("{base}:{line}"), lay));
        }
    }
    let mut r = Report {
        files: copies.len(),
        classes: seen.len(),
        ..Report::default()
    };
    for (cls, list) in &seen {
        if list.len() < 2 {
            r.single += 1;
            continue;
        }
        if is_placeholder(cls, placeholders) {
            r.placeholders += 1;
            continue;
        }
        let declined = list.iter().filter(|(_, l)| l.unknown.is_some()).count();
        let good: Vec<&(String, Layout)> =
            list.iter().filter(|(_, l)| l.unknown.is_none()).collect();
        if good.len() < 2 {
            r.all_declined += 1;
            continue;
        }
        let mut worst = Verdict::Agree;
        let mut ev = String::new();
        for (i, a) in good.iter().enumerate() {
            for b in good.iter().skip(i + 1) {
                let (v, at) = compare(&a.1, &b.1);
                if v > worst {
                    worst = v;
                    if let Some((off, x, y)) = at {
                        ev = format!("+0x{off:x}  {} says {x}   {} says {y}", a.0, b.0);
                    }
                }
            }
        }
        r.rows.push(Row {
            declared: declared.contains(cls),
            class: cls.clone(),
            verdict: worst,
            copies: list.len(),
            declined,
            evidence: ev,
        });
    }
    let mut why: BTreeMap<String, usize> = BTreeMap::new();
    for list in seen.values() {
        for (_, lay) in list {
            if let Some(w) = &lay.unknown {
                *why.entry(w.clone()).or_default() += 1;
            }
        }
    }
    let mut causes: Vec<(String, usize)> = why.into_iter().collect();
    causes.sort_by_key(|(w, n)| (std::cmp::Reverse(*n), w.clone()));
    r.declines = causes;
    r
}

#[must_use]
pub fn report(r: &Report, label: &str, population: &str) -> Vec<String> {
    let mut out = vec![
        format!(
            "{label}: DO THE COPIES OF ONE CLASS AGREE? Offsets DERIVED from declaration order and type size, never read from a comment."
        ),
        format!(
            "  POPULATION: {} source(s) in {population}, {} distinct class name(s), of which {} are declared in TWO OR MORE files and comparable. NOT comparable: {} declared once, {} placeholder name(s) ([image.classes] `placeholders'), {} where fewer than two copies could be derived at all.",
            r.files,
            r.classes,
            r.rows.len(),
            r.single,
            r.placeholders,
            r.all_declined
        ),
    ];
    let mut by: BTreeMap<Verdict, Vec<&Row>> = BTreeMap::new();
    for row in &r.rows {
        by.entry(row.verdict).or_default().push(row);
    }
    let count = |v: Verdict| by.get(&v).map_or(0, Vec::len);
    out.push(format!(
        "  {} LAYOUT, {} NAME CLASH, {} SPLIT, {} OPAQUE, {} UNNAMED, {} AGREE.",
        count(Verdict::Layout),
        count(Verdict::NameClash),
        count(Verdict::Split),
        count(Verdict::Opaque),
        count(Verdict::Unnamed),
        count(Verdict::Agree)
    ));
    for (v, label) in [
        (
            Verdict::Layout,
            "LAYOUT DISAGREEMENT -- different SIZE at the same derived offset, so at most one side is right about the bytes",
        ),
        (
            Verdict::Split,
            "SPLIT -- one side's contiguous fields TILE the other's one field exactly; both are right about the bytes",
        ),
        (
            Verdict::NameClash,
            "NAME CLASH -- same offset, same size, two different real names; the shared header must pick one",
        ),
        (
            Verdict::Opaque,
            "OPAQUE -- one copy declares a pad run over bytes another copy NAMES",
        ),
    ] {
        let Some(list) = by.get(&v) else { continue };
        out.push(format!("  {label}: {} class(es).", list.len()));
        let mut list = list.clone();
        list.sort_by_key(|row| std::cmp::Reverse(row.copies));
        for row in list {
            out.push(format!(
                "      {:<20} {} copies ({} declined) {}  {}",
                row.class,
                row.copies,
                row.declined,
                if row.declared {
                    "[project declares it]"
                } else {
                    "[NOT in the project]  "
                },
                row.evidence
            ));
        }
    }
    if let Some(list) = by.get(&Verdict::Unnamed) {
        out.push(format!(
            "  UNNAMED -- same offset and size, one side's member name merely ENCODES THE OFFSET, so the real name wins: {} class(es).",
            list.len()
        ));
    }
    if let Some(list) = by.get(&Verdict::Agree) {
        let mut list = list.clone();
        list.sort_by_key(|row| std::cmp::Reverse(row.copies));
        out.push(format!(
            "  AGREE -- no disagreement at any shared offset (one may be a PREFIX of another): {} class(es), largest first:",
            list.len()
        ));
        for row in list.iter().take(LISTED_AGREE) {
            out.push(format!(
                "      {:<20} {} copies ({} declined)",
                row.class, row.copies, row.declined
            ));
        }
    }
    let total: usize = r.declines.iter().map(|(_, n)| n).sum();
    out.push(format!(
        "  A DECLINED layout is never compared, because a guessed origin would manufacture a contradiction at every later offset. {total} declined, BY CAUSE:"
    ));
    for (reason, n) in r.declines.iter().take(LISTED_CAUSES) {
        out.push(format!("      {n:>5}  {reason}"));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn lay(src: &str) -> Layout {
        let d = declarations(src);
        assert_eq!(d.len(), 1, "{src}");
        d.into_iter().next().map(|(_, _, l)| l).unwrap()
    }

    fn offs(l: &Layout) -> Vec<u32> {
        l.fields.iter().map(|f| f.off).collect()
    }

    #[test]
    fn offsets_follow_declaration_order_size_and_the_zp8_alignment() {
        assert_eq!(
            offs(&lay("struct Rect { int left, top, right, bottom; };\n")),
            vec![0, 4, 8, 12]
        );
        assert_eq!(
            offs(&lay("struct A { char c; int n; double d; };\n")),
            vec![0, 4, 8]
        );
        assert_eq!(offs(&lay("struct A { Unknown *p; int n; };\n")), vec![0, 4]);
        assert_eq!(
            offs(&lay("struct A { char pad224[0x1358 - 0x224]; int n; };\n")),
            vec![0, 0x1134]
        );
        assert_eq!(extent("sizeof(Foo)"), None);
        assert_eq!(
            offs(&lay(
                "struct A { int m_count; void Set(int v) { m_count = v; } int m_next; };\n"
            )),
            vec![0, 4]
        );
        assert!(
            lay("struct V { virtual void f(); int n; };\n")
                .unknown
                .is_some()
        );
        assert!(
            lay("struct A { Something whole; int n; };\n")
                .unknown
                .is_some()
        );
        assert!(opens_plain("class D : public B {").is_none());
    }

    #[test]
    fn a_verdict_ranks_a_size_clash_over_names_and_splits() {
        let v = |a: &str, b: &str| compare(&lay(a), &lay(b)).0;
        assert_eq!(
            v("struct R { int a, b; };\n", "struct R { int x, y; };\n"),
            Verdict::NameClash
        );
        assert_eq!(
            v(
                "struct W { unsigned short t; unsigned short c; unsigned int n; };\n",
                "struct W { unsigned short t; unsigned short c; unsigned short lo; unsigned short hi; };\n"
            ),
            Verdict::Split
        );
        assert_eq!(
            v(
                "struct A { unsigned int n; int t; };\n",
                "struct A { unsigned short lo; int t; };\n"
            ),
            Verdict::Layout
        );
        assert_eq!(
            v(
                "struct C { int a; int n; };\n",
                "struct C { char pad_0000[8]; };\n"
            ),
            Verdict::Opaque
        );
        assert_eq!(
            v("struct A { int m_font; };\n", "struct A { int f00; };\n"),
            Verdict::Unnamed
        );
        assert_eq!(
            v("struct P { int a; int b; };\n", "struct P { int a; };\n"),
            Verdict::Agree
        );
        assert!(encodes_own_offset("f5C", 0x5c) && !encodes_own_offset("crc32", 0x10));
    }

    #[test]
    fn the_screen_counts_its_population_and_names_the_project_classes() {
        let copies = vec![
            (
                "a.cpp".to_string(),
                "struct Rect { int left, top; };\nstruct S { int a; };\n".to_string(),
            ),
            (
                "b.cpp".to_string(),
                "struct Rect { int x, y; };\nstruct S { int b; };\nstruct Once { int z; };\n"
                    .to_string(),
            ),
        ];
        let declared: BTreeSet<String> = ["Rect".to_string()].into_iter().collect();
        let r = screen(&copies, &declared, &["?".to_string()]);
        assert_eq!((r.files, r.classes, r.single, r.placeholders), (2, 3, 1, 1));
        let none = screen(&copies, &declared, &[]);
        assert_eq!(none.placeholders, 0);
        assert_eq!(r.rows.len(), 1);
        assert!(r.rows[0].declared && r.rows[0].verdict == Verdict::NameClash);
        let lines = report(&r, "IMG", "copies/");
        assert!(lines.iter().any(|l| l.contains("Rect                 2 copies (0 declined) [project declares it]  +0x0  a.cpp:1 says left:4   b.cpp:1 says x:4")), "{lines:#?}");
    }
}
