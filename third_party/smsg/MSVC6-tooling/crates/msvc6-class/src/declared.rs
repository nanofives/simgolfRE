use crate::decl::{self, ClassDecl, VPTR};
use crate::join;
use crate::slots::{self, CountVerdict, Implied, Rule};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const LISTED_SIGNATURES: usize = 40;
pub const LISTED_LAYOUTS: usize = 24;
pub const LAYOUT_WIDTH: usize = 74;
pub const DEFINITION_SCAN_FACTOR: usize = 4;

pub const DEFINITION_SCAN_FLOOR: usize = 1 << 16;

type SigRow = (String, String, Vec<(String, String)>);

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CountControl {
    pub class: String,
    pub slots: usize,
    pub what: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ControlResult {
    pub class: String,
    pub want: usize,
    pub got: Vec<usize>,
    pub reproduced: bool,
}

#[derive(Debug, Clone, Default, Serialize)]
pub struct Report {
    pub classes: usize,
    pub files: usize,
    pub polymorphic: usize,
    pub duplicated: usize,
    pub disagreeing: usize,
    pub signatures: usize,
    pub arities: usize,
    pub overloads: usize,
    pub layouts: usize,
    pub suspects: usize,
    pub admissible: usize,
    pub below: usize,
    pub unstated: usize,
    pub macros: usize,
    pub definitions: usize,
    pub controls: Vec<ControlResult>,
    #[serde(skip)]
    pub lines: Vec<String>,
}

fn arity(sig: &str) -> usize {
    decl::arity(sig)
}

fn duplicated(poly: &[&ClassDecl], legacy: bool, out: &mut Vec<String>, r: &mut Report) {
    let mut by_name: BTreeMap<&str, Vec<&ClassDecl>> = BTreeMap::new();
    for c in poly {
        by_name.entry(c.name.as_str()).or_default().push(c);
    }
    let dupes: Vec<_> = by_name.iter().filter(|(_, v)| v.len() > 1).collect();
    let counts = |v: &[&ClassDecl]| -> BTreeSet<usize> {
        v.iter().map(|c| c.virtuals(legacy).len()).collect()
    };
    r.duplicated = dupes.len();
    r.disagreeing = dupes.iter().filter(|(_, v)| counts(v).len() > 1).count();
    out.push(String::new());
    out.push(format!(
        "  == DECLARED IN MORE THAN ONE FILE: {} class(es), {} of them DISAGREEING on count ==",
        r.duplicated, r.disagreeing
    ));
    for (name, v) in &dupes {
        out.push(format!(
            "    {name:<24} {} copies  {}",
            v.len(),
            if counts(v).len() > 1 {
                "DISAGREE"
            } else {
                "agree"
            }
        ));
        for c in v.iter() {
            out.push(format!(
                "        {:>4} virtual(s), {:>3} pure   {}",
                c.virtuals(legacy).len(),
                c.pure(legacy),
                c.file
            ));
        }
    }
}

fn signatures(pop: &[ClassDecl], legacy: bool, out: &mut Vec<String>, r: &mut Report) {
    let mut by_name: BTreeMap<&str, Vec<&ClassDecl>> = BTreeMap::new();
    for c in pop {
        by_name.entry(c.name.as_str()).or_default().push(c);
    }
    let mut sig_rows: Vec<SigRow> = Vec::new();
    let mut arity_rows: Vec<SigRow> = Vec::new();
    for (name, copies) in &by_name {
        if copies.len() < 2 {
            continue;
        }
        let mut per: BTreeMap<String, BTreeMap<String, (String, BTreeSet<String>)>> =
            BTreeMap::new();
        for c in copies {
            for (m, sig) in c.members(legacy) {
                per.entry(m)
                    .or_default()
                    .entry(decl::signature_key(&sig))
                    .or_insert_with(|| (sig.clone(), BTreeSet::new()))
                    .1
                    .insert(c.file.clone());
            }
        }
        for (m, sigs) in per {
            if sigs.len() < 2 {
                continue;
            }
            let mut by_arity: BTreeMap<usize, Vec<(String, &BTreeSet<String>)>> = BTreeMap::new();
            for (disp, files) in sigs.values() {
                by_arity
                    .entry(arity(disp))
                    .or_default()
                    .push((disp.clone(), files));
            }
            let shares = |group: &[(String, &BTreeSet<String>)]| -> bool {
                group.iter().enumerate().any(|(i, (_, fa))| {
                    group
                        .iter()
                        .skip(i + 1)
                        .any(|(_, fb)| fa.intersection(fb).next().is_some())
                })
            };
            let all: Vec<(String, &BTreeSet<String>)> =
                sigs.values().map(|(d, f)| (d.clone(), f)).collect();
            let render = |group: &[(String, &BTreeSet<String>)]| -> Vec<(String, String)> {
                group
                    .iter()
                    .map(|(sig, files)| {
                        (
                            sig.clone(),
                            files.iter().cloned().collect::<Vec<_>>().join(", "),
                        )
                    })
                    .collect()
            };
            if by_arity.len() > 1 && !shares(&all) {
                arity_rows.push(((*name).to_string(), m.clone(), render(&all)));
            } else if by_arity.len() > 1 {
                r.overloads += 1;
            }
            for group in by_arity.values() {
                if group.len() > 1 {
                    if shares(group) {
                        r.overloads += 1;
                    } else {
                        sig_rows.push(((*name).to_string(), m.clone(), render(group)));
                    }
                }
            }
        }
    }
    r.signatures = sig_rows.len();
    r.arities = arity_rows.len();
    out.push(String::new());
    out.push(format!(
        "  == SAME CLASS, SAME MEMBER, DIFFERENT SIGNATURE: {} case(s) == (parameter names stripped; {} overload(s), a different count or two signatures in one file, not listed)",
        sig_rows.len(),
        r.overloads
    ));
    let rows = |rows: &[SigRow], out: &mut Vec<String>| {
        for (cls, m, sigs) in rows.iter().take(LISTED_SIGNATURES) {
            out.push(format!("    {cls}::{m}"));
            for (sig, f) in sigs {
                out.push(format!("        {sig:<58} [{f}]"));
            }
        }
    };
    rows(&sig_rows, out);
    out.push(String::new());
    out.push(format!(
        "  == SAME CLASS, SAME MEMBER, DIFFERENT PARAMETER COUNT, DIFFERENT FILES: {} case(s) ==",
        arity_rows.len()
    ));
    rows(&arity_rows, out);
}

fn layouts(pop: &[ClassDecl], rule: Rule, out: &mut Vec<String>, r: &mut Report) {
    let model = slots::Model::new(pop);
    let mut by_name: BTreeMap<&str, Vec<&ClassDecl>> = BTreeMap::new();
    for c in pop {
        by_name.entry(c.name.as_str()).or_default().push(c);
    }
    let mut rows: Vec<SigRow> = Vec::new();
    let (mut prefix, mut unstated, mut device) = (0usize, 0usize, 0usize);
    for (name, copies) in &by_name {
        if copies.len() < 2 {
            continue;
        }
        let mut per: BTreeMap<String, (String, BTreeSet<String>)> = BTreeMap::new();
        let mut unstated_one = false;
        for c in copies {
            let Some(full) = model.flat_layout(c, rule) else {
                unstated_one = true;
                continue;
            };
            let mut key: Vec<String> = Vec::new();
            for (i, d) in full.iter().enumerate() {
                if d == VPTR {
                    key.push(VPTR.to_string());
                } else if i == 0 && decl::is_explicit_vptr(d) {
                    device += 1;
                    key.push(VPTR.to_string());
                } else {
                    key.push(decl::data_types(d));
                }
            }
            per.entry(key.join("; "))
                .or_insert_with(|| (full.join("; "), BTreeSet::new()))
                .1
                .insert(c.file.clone());
        }
        if unstated_one {
            unstated += 1;
        }
        per.retain(|k, _| !k.is_empty());
        let keys: Vec<String> = per.keys().cloned().collect();
        let longest = keys.iter().map(String::len).max().unwrap_or(0);
        let all_prefix = keys.iter().all(|a| {
            a.len() == longest
                || keys
                    .iter()
                    .any(|b| b.starts_with(a.as_str()) && b.len() > a.len())
        });
        if per.len() > 1 && all_prefix {
            prefix += 1;
            continue;
        }
        if per.len() > 1 {
            rows.push((
                (*name).to_string(),
                String::new(),
                per.into_values()
                    .map(|(disp, files)| (disp, files.into_iter().collect::<Vec<_>>().join(", ")))
                    .collect(),
            ));
        }
    }
    r.layouts = rows.len();
    out.push(String::new());
    out.push(format!(
        "  == SAME CLASS, DIFFERENT LAYOUT: {} class(es) == (compared on type and order, bases first; not listed: {prefix} whose copies are prefixes of one another, {unstated} with a base this population cannot resolve; {device} cop(ies) model the vptr by hand)",
        rows.len()
    ));
    for (cls, _, lays) in rows.iter().take(LISTED_LAYOUTS) {
        out.push(format!("    {cls}"));
        for (lay, files) in lays {
            let shown = if lay.len() > LAYOUT_WIDTH {
                format!("{}...", lay.get(..LAYOUT_WIDTH - 3).unwrap_or(lay))
            } else {
                lay.clone()
            };
            out.push(format!("        {shown:<74} [{files}]"));
        }
    }
    out.extend(model.note());
}

fn counts(
    poly: &[&ClassDecl],
    model: &slots::Model<'_>,
    rule: Rule,
    lengths: &[usize],
    min_slots: usize,
    out: &mut Vec<String>,
    r: &mut Report,
) {
    out.push(String::new());
    out.push(format!(
        "  == IMPLIED SLOT COUNT vs {} CANDIDATE TABLE(S) OF >= {min_slots} SLOT(S) ==",
        lengths.len()
    ));
    let longest = lengths.iter().copied().max();
    for c in poly {
        if let Some(mac) = &c.macro_used {
            r.macros += 1;
            out.push(format!(
                "    {:<26} >= {:>2} slots            NOT STATED -- class scope invokes the macro `{mac}', whose slots this parser cannot see  [{}]",
                c.name,
                c.virtuals(rule.legacy()).len(),
                c.file
            ));
            continue;
        }
        match model.implied(c, rule) {
            Implied::Count { slots, from_base } => {
                let note = match slots::classify_count(slots, min_slots, lengths) {
                    CountVerdict::BelowThreshold => {
                        r.below += 1;
                        format!("below the {min_slots}-slot threshold, CANNOT MATCH")
                    }
                    CountVerdict::Exact(n) => format!("{n} candidate(s) of that length"),
                    CountVerdict::Admissible { admitting } => {
                        r.admissible += 1;
                        format!("no RUN of that length; ADMISSIBLE inside {admitting} (see note)")
                    }
                    CountVerdict::Exceeds(max) => {
                        r.suspects += 1;
                        format!("EXCEEDS THE LONGEST CANDIDATE ({max})")
                    }
                };
                let via = if from_base > 0 {
                    format!("{from_base} from base")
                } else {
                    String::new()
                };
                out.push(format!(
                    "    {:<26} {:>4} slots {:>3} pure  {:<14}  {:<26}  [{}]",
                    c.name,
                    slots,
                    c.pure(rule.legacy()),
                    via,
                    note,
                    c.file
                ));
            }
            Implied::UnknownBase(b) => {
                r.unstated += 1;
                out.push(format!(
                    "    {:<26}    ? slots            NOT STATED -- base `{b}' is not declared in this population  [{}]",
                    c.name, c.file
                ));
            }
            Implied::AmbiguousBase(b, totals) => {
                r.unstated += 1;
                out.push(format!(
                    "    {:<26}    ? slots            NOT STATED -- base `{b}' has copies implying {totals:?} slots  [{}]",
                    c.name, c.file
                ));
            }
        }
    }
    out.push(String::new());
    out.push(format!(
        "  {} row(s) EXCEED the longest candidate table ({} slots): the only form this screen can refuse. {} match no run length but fit inside a longer run (a table whose predecessor abuts it is no run of its own), {} invoke a class-scope macro, {} imply fewer slots than the threshold, {} could not be stated.",
        r.suspects,
        longest.map_or_else(|| "no".to_string(), |m| m.to_string()),
        r.admissible,
        r.macros,
        r.below,
        r.unstated
    ));
}

fn controls(
    poly: &[&ClassDecl],
    model: &slots::Model<'_>,
    rule: Rule,
    controls: &[CountControl],
    out: &mut Vec<String>,
    r: &mut Report,
) {
    out.push(String::new());
    if controls.is_empty() {
        out.push(
            "  NO CONTROL: the image's [image.classes] records no slot count, so the rows above are unvalidated."
                .to_string(),
        );
        return;
    }
    out.push(
        "  CONTROL -- the slot counts [image.classes] records from resolved tables; some declaration must compute each:"
            .to_string(),
    );
    for c in controls {
        let got: Vec<usize> = poly
            .iter()
            .filter(|d| d.name == c.class)
            .filter_map(|d| match model.implied(d, rule) {
                Implied::Count { slots, .. } => Some(slots),
                _ => None,
            })
            .collect();
        let hits = got.iter().filter(|g| **g == c.slots).count();
        let verdict = if got.is_empty() {
            "NOT DECLARED here, or its base is unresolvable".to_string()
        } else if hits == 0 {
            format!(
                "PARSER DISAGREES: no declaration computes {}; computes {got:?}",
                c.slots
            )
        } else if hits == got.len() {
            format!("reproduced by all {hits} declaration(s)")
        } else {
            format!(
                "reproduced by {hits} of {} declaration(s); the other(s) compute {:?} and are TRUNCATED DECLARATIONS, not parser errors",
                got.len(),
                got.iter().filter(|g| **g != c.slots).collect::<Vec<_>>()
            )
        };
        out.push(format!(
            "    {:<24} expect {:>3}  ({})  -- {verdict}",
            c.class, c.slots, c.what
        ));
        r.controls.push(ControlResult {
            class: c.class.clone(),
            want: c.slots,
            reproduced: hits > 0,
            got,
        });
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum HeadEnd {
    Definition(usize),
    Declaration,
    Open,
    Spent,
}

fn head_end(lines: &[&str], i: usize, budget: &mut usize) -> HeadEnd {
    let mut depth = 0i32;
    for (k, l) in lines.iter().enumerate().skip(i) {
        for c in l.chars() {
            if *budget == 0 {
                return HeadEnd::Spent;
            }
            *budget -= 1;
            match c {
                '(' => depth += 1,
                ')' => depth -= 1,
                ';' if depth <= 0 => return HeadEnd::Declaration,
                '{' if depth <= 0 => return HeadEnd::Definition(k),
                _ => {}
            }
        }
    }
    HeadEnd::Open
}

#[cfg(test)]
fn terminates_as_definition(lines: &[&str], i: usize) -> Option<bool> {
    let mut budget = usize::MAX;
    match head_end(lines, i, &mut budget) {
        HeadEnd::Definition(_) => Some(true),
        HeadEnd::Declaration => Some(false),
        HeadEnd::Open | HeadEnd::Spent => None,
    }
}

fn internal(line: &str) -> bool {
    line.split('(')
        .next()
        .is_some_and(|head| head.split_whitespace().any(|w| w == "static"))
}

fn defined_name(line: &str) -> Option<String> {
    let head = line.split('(').next()?;
    if !line.contains('(') || head.contains('=') || head.trim_start().starts_with('#') {
        return None;
    }
    let last = head.split_whitespace().last()?;
    let name = last.trim_start_matches(['*', '&']);
    if name.is_empty() || matches!(name, "if" | "for" | "while" | "switch" | "return" | "else") {
        return None;
    }
    name.chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == ':' || c == '~')
        .then(|| name.to_string())
}

fn definitions(files: &[(String, String)], out: &mut Vec<String>, r: &mut Report) {
    let mut where_defined: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut notes = Vec::new();
    for (path, src) in files {
        if !path.ends_with(".cpp") {
            continue;
        }
        let stripped = decl::uncomment(src.as_bytes());
        let lines: Vec<&str> = stripped.lines().collect();
        let mut budget = stripped
            .len()
            .saturating_mul(DEFINITION_SCAN_FACTOR)
            .max(DEFINITION_SCAN_FLOOR);
        let mut unread = 0usize;
        for (i, line) in lines.iter().enumerate() {
            if line.is_empty() || line.starts_with(char::is_whitespace) || !line.contains('(') {
                continue;
            }
            let Some(n) = defined_name(line) else {
                continue;
            };
            if internal(line) {
                continue;
            }
            match head_end(&lines, i, &mut budget) {
                HeadEnd::Definition(end) => {
                    let joined: String = lines.get(i..=end).unwrap_or_default().join(" ");
                    where_defined
                        .entry(format!("{n}/{}", arity(&joined)))
                        .or_default()
                        .insert(path.rsplit('/').next().unwrap_or(path).to_string());
                }
                HeadEnd::Spent => unread = unread.saturating_add(1),
                HeadEnd::Declaration | HeadEnd::Open => {}
            }
        }
        if unread > 0 {
            notes.push(format!(
                "NOTE: {path}: {unread} function head(s) not read: the definitions screen reads at most \
                 {DEFINITION_SCAN_FACTOR} bytes a source byte (at least {DEFINITION_SCAN_FLOOR})"
            ));
        }
    }
    let dups: Vec<(&String, &BTreeSet<String>)> =
        where_defined.iter().filter(|(_, f)| f.len() > 1).collect();
    r.definitions = dups.len();
    out.push(String::new());
    out.push(format!(
        "  == SAME FUNCTION DEFINED IN MORE THAN ONE FILE: {} case(s) == (keyed on name and arity: check the signatures)",
        dups.len()
    ));
    if dups.is_empty() {
        out.push("    none".to_string());
    }
    for (name, in_files) in &dups {
        out.push(format!(
            "    {name:<44} {}",
            in_files.iter().cloned().collect::<Vec<_>>().join("  ")
        ));
    }
    out.extend(notes);
}

#[derive(Debug, Clone, Copy)]
pub struct Inputs<'a> {
    pub label: &'a str,
    pub files: &'a [(String, String)],
    pub headers: &'a [(String, String)],
    pub lengths: &'a [usize],
    pub min_slots: usize,
    pub controls: &'a [CountControl],
    pub rule: Rule,
}

#[must_use]
pub fn screen(i: &Inputs<'_>) -> Report {
    let legacy = i.rule.legacy();
    let found = join::population_noted(i.files, !legacy);
    let mut notes = found.notes;
    let mut pop = found.decls;
    let own = pop.len();
    if !legacy {
        let h = join::population_noted(i.headers, true);
        pop.extend(h.decls);
        notes.extend(h.notes);
    }
    let owned: BTreeSet<&str> = i.files.iter().map(|(n, _)| n.as_str()).collect();
    let model = slots::Model::new(&pop);
    let poly: Vec<&ClassDecl> = pop
        .iter()
        .filter(|c| owned.contains(c.file.as_str()) && join::takes_part(c, &model, i.rule))
        .collect();
    let mut r = Report {
        classes: own,
        files: i.files.len(),
        polymorphic: poly.len(),
        ..Report::default()
    };
    let mut out = vec![
        format!("{}: DECLARED CLASSES vs CANDIDATE VFTABLE LENGTHS", i.label),
        String::new(),
        format!(
            "  POPULATION: {} class definition(s) across {} file(s) of [image.classes] sources; {} declare or override a virtual member.",
            r.classes, r.files, r.polymorphic
        ),
    ];
    let own_pop: Vec<ClassDecl> = pop
        .iter()
        .filter(|c| owned.contains(c.file.as_str()))
        .cloned()
        .collect();
    duplicated(&poly, legacy, &mut out, &mut r);
    signatures(&own_pop, legacy, &mut out, &mut r);
    layouts(&own_pop, i.rule, &mut out, &mut r);
    counts(
        &poly,
        &model,
        i.rule,
        i.lengths,
        i.min_slots,
        &mut out,
        &mut r,
    );
    controls(&poly, &model, i.rule, i.controls, &mut out, &mut r);
    definitions(i.files, &mut out, &mut r);
    out.push(String::new());
    out.push("  NO VERDICT. This screen reports counts and mismatches; promoting a row to a defect needs the table.".to_string());
    out.extend(notes);
    out.extend(model.note());
    r.lines = out;
    r
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(files: &[(&str, &str)], lengths: &[usize], rule: Rule) -> Report {
        let files: Vec<(String, String)> = files
            .iter()
            .map(|(a, b)| ((*a).to_string(), (*b).to_string()))
            .collect();
        screen(&Inputs {
            label: "IMG",
            files: &files,
            headers: &[],
            lengths,
            min_slots: 2,
            controls: &[CountControl {
                class: "D".into(),
                slots: 5,
                what: "its table".into(),
            }],
            rule,
        })
    }

    #[test]
    fn an_overload_group_counts_every_member_and_the_count_meets_the_table() {
        let src = "class B {\n    virtual ~B();\n    virtual void f(int);\n    virtual void f();\n};\nclass D : public B {\n    void f(int);\n    virtual void f(char);\n    virtual void g();\n};\n";
        let r = run(&[("include/a.h", src)], &[3, 5], Rule::Vc6);
        assert!(r.lines.iter().any(|l| l.starts_with("    D                             5 slots   0 pure  3 from base     1 candidate(s) of that length")), "{:#?}", r.lines);
        assert_eq!(r.controls[0].got, vec![5]);
        assert!(r.controls[0].reproduced);
        let l = run(&[("include/a.h", src)], &[3, 5], Rule::Legacy);
        assert_eq!(l.controls[0].got, vec![4]);
        assert!(!l.controls[0].reproduced);
    }

    #[test]
    fn copies_that_disagree_on_count_signature_or_layout_are_each_reported() {
        let a = "class C {\npublic:\n    virtual int a(int x);\n    int m_x;\n    int m_y;\n};\n";
        let b = "class C {\npublic:\n    virtual int a(char x);\n    virtual int b();\n    char *m_x;\n};\n";
        let r = run(&[("include/a.h", a), ("src/b.cpp", b)], &[2], Rule::Vc6);
        assert_eq!((r.duplicated, r.disagreeing), (1, 1));
        assert_eq!(r.signatures, 1);
        assert_eq!(r.layouts, 1);
        assert_eq!(r.definitions, 0);
    }

    #[test]
    fn a_function_defined_in_two_files_is_named_and_a_declaration_is_not_a_definition() {
        let a = "int Shared(int a, int b)\n{\n    return a;\n}\nint Wrapped(int a,\n            int b);\n";
        let b = "int Shared(int a, int b)\n{\n    return b;\n}\nint Wrapped(int a, int b)\n{\n    return 0;\n}\n";
        let r = run(&[("src/a.cpp", a), ("src/b.cpp", b)], &[], Rule::Vc6);
        assert_eq!(r.definitions, 1);
        assert!(
            r.lines.iter().any(|l| l.starts_with("    Shared/")),
            "{:#?}",
            r.lines
        );
        assert_eq!(
            terminates_as_definition(&["void f(int a /* ; */, int b)", "{"], 0),
            Some(true)
        );
    }

    #[test]
    fn a_long_parameter_list_is_read_to_its_end_and_an_internal_definition_is_no_duplicate() {
        let params: Vec<String> = (0..10).map(|k| format!("    int p{k}")).collect();
        let long = format!(
            "int Long(\n{}\n)\n{{\n    return 0;\n}}\n",
            params.join(",\n")
        );
        let inl = "static __inline int Inflate(int *r, int dx, int dy)\n{\n    return dx;\n}\n";
        let r = run(
            &[
                ("src/a.cpp", format!("{long}{inl}").as_str()),
                ("src/b.cpp", format!("{long}{inl}").as_str()),
            ],
            &[],
            Rule::Vc6,
        );
        assert_eq!(r.definitions, 1, "{:#?}", r.lines);
        assert!(
            r.lines.iter().any(|l| l.starts_with("    Long/10 ")),
            "{:#?}",
            r.lines
        );
        assert!(
            !r.lines.iter().any(|l| l.contains("Inflate")),
            "{:#?}",
            r.lines
        );
        let open = "int f(\n".repeat(1 << 12);
        let r = run(&[("src/open.cpp", open.as_str())], &[], Rule::Vc6);
        assert!(
            r.lines.iter().any(|l| l.starts_with("NOTE: src/open.cpp: ")
                && l.contains("function head(s) not read")),
            "{:#?}",
            r.lines.iter().rev().take(3).collect::<Vec<_>>()
        );
    }
}
