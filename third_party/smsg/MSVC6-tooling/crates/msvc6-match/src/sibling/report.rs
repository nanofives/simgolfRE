use super::code::Code;
use super::conv::clash;
use super::feat::{Callee, Fun, Side};
use super::pair::{Pairs, cut, fits, shape};
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::fmt::Write as _;
use std::path::Path;

pub const ATOMS_HEAD: &str = "atom\tsib_lo\tsib_hi\tatom_fns\tpaired\tfiles\tmembers";

pub const ORDER_HEAD: &str = "image\tfile\tpaired\tinversions\tpairs\treversed_runs\tsequence";

pub const PAIRS_HEAD: &str = "image\tva\tfile\tname\tsib_va\tsib_atom\tatom_len\tround\tevidence\tshape\tours_n\tsib_n\tsib_file\tsib_names\tsig\tsig_clash";

pub const NAMES_HEAD: &str = "image\tva\tfile\tname\tsib_va\tround\ttoken\tstring";

pub const REVERSED_RUN: usize = 3;

#[must_use]
pub fn name_tokens(s: &str) -> Vec<String> {
    let mut out = Vec::new();
    let chars: Vec<char> = s.chars().collect();
    let at = |i: usize| chars.get(i).copied();
    let mut i = 0;
    while i < chars.len() {
        let c = at(i).unwrap_or(' ');
        if !(c.is_ascii_alphabetic() || c == '_' || c == '~') {
            i += 1;
            continue;
        }
        let st = i;
        while at(i).is_some_and(|c| c.is_ascii_alphanumeric() || c == '_' || c == '~')
            || (at(i) == Some(':') && at(i.saturating_add(1)) == Some(':'))
        {
            i = i.saturating_add(if at(i) == Some(':') { 2 } else { 1 });
        }
        let tok: String = chars
            .get(st..i)
            .map_or_else(String::new, |v| v.iter().collect());
        let call = at(i) == Some('(') && at(i.saturating_add(1)) == Some(')');
        let caps = tok.chars().filter(char::is_ascii_uppercase).count();
        let lower = tok.chars().any(|c| c.is_ascii_lowercase());
        let under = tok
            .find('_')
            .is_some_and(|p| p > 0 && p.saturating_add(1) < tok.len());
        if tok.contains("::")
            || call
            || (caps >= 2 && lower && tok.len() >= 6 && !tok.starts_with(char::is_lowercase))
            || (under && lower)
        {
            out.push(if call { format!("{tok}()") } else { tok });
        }
    }
    out
}

#[must_use]
pub fn is_source_path(s: &str) -> bool {
    let l = s.to_ascii_lowercase();
    (l.contains('\\') || l.contains('/'))
        && [".cpp", ".c", ".h", ".hpp", ".inl"]
            .iter()
            .any(|x| l.ends_with(x))
}

#[must_use]
pub fn reversed_runs(v: &[u32], min: usize) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    let mut st = 0;
    for i in 1..=v.len() {
        let desc = i < v.len() && v.get(i) < v.get(i - 1);
        if !desc {
            if i - st >= min {
                out.push((st, i - st));
            }
            st = i;
        }
    }
    out
}

#[must_use]
pub fn file_runs<T: PartialEq>(seq: &[T]) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    let mut st = 0;
    for i in 1..=seq.len() {
        if i == seq.len() || seq.get(i) != seq.get(st) {
            if i > st {
                out.push((st, i - st));
            }
            st = i;
        }
    }
    out
}

#[must_use]
pub fn intruders<T>(seq: &[T], runs: &[(usize, usize)]) -> Vec<(usize, usize)> {
    runs.windows(2)
        .filter_map(|w| {
            let (a, b) = (w.first()?, w.get(1)?);
            let from = a.0.saturating_add(a.1);
            (b.0 > from && b.0 <= seq.len()).then(|| (from, b.0 - from))
        })
        .collect()
}

#[must_use]
pub fn inversions(v: &[u32]) -> (usize, usize) {
    let mut inv = 0usize;
    for (i, a) in v.iter().enumerate() {
        inv = inv.saturating_add(
            v.iter()
                .skip(i.saturating_add(1))
                .filter(|b| *b < a)
                .count(),
        );
    }
    let n = v.len();
    (inv, n.saturating_mul(n.saturating_sub(1)) / 2)
}

#[must_use]
pub fn defined_name(line: &str) -> Option<String> {
    let head = line.split('(').next()?.trim_end();
    if head.is_empty() || !line.contains('(') {
        return None;
    }
    if let Some(p) = head.rfind("operator") {
        let start = head
            .get(..p)?
            .rfind([' ', '*', '&', '\t'])
            .map_or(0, |q| q.saturating_add(1));
        return Some(head.get(start..)?.split_whitespace().collect::<String>());
    }
    let tok = head.rsplit([' ', '*', '&', '\t']).next()?;
    (!tok.is_empty()).then(|| tok.to_string())
}

fn base_name(file: &str) -> String {
    Path::new(file)
        .file_name()
        .map_or_else(String::new, |n| n.to_string_lossy().to_string())
}

#[derive(Debug, Clone)]
pub struct Image {
    pub label: String,
    pub base: u32,
}

#[derive(Debug)]
pub struct Report {
    pub files: Vec<(String, String)>,
    pub summary: String,
    pub atom_rows: String,
    pub inverted: String,
}

#[derive(Debug)]
pub struct Run<'a> {
    pub ours: &'a Side,
    pub images: &'a [Image],
    pub sib: &'a Side,
    pub sib_code: &'a Code<'a>,
    pub pairs: &'a Pairs,
    pub label: &'a str,
    pub only: &'a [String],
}

impl Run<'_> {
    fn our_va(&self, f: &Fun) -> u64 {
        let b = self
            .images
            .iter()
            .find(|i| i.label == f.image)
            .map_or(0, |i| i.base);
        u64::from(b).saturating_add(u64::from(f.rva))
    }

    fn sib_va(&self, rva: u32) -> u64 {
        self.sib_code.va(rva)
    }

    fn shown(&self, f: &Fun) -> bool {
        self.only.is_empty() || self.only.iter().any(|x| f.file.contains(x.as_str()))
    }

    fn atom_len(&self) -> HashMap<usize, usize> {
        let mut m: HashMap<usize, usize> = HashMap::new();
        for a in &self.sib.atom {
            let e = m.entry(*a).or_insert(0);
            *e = e.saturating_add(1);
        }
        m
    }
}

struct Tables {
    pairs: String,
    names: String,
    by_file: BTreeMap<(String, String), Vec<(u32, usize)>>,
    per_atom: BTreeMap<usize, Vec<(usize, usize)>>,
    shown_n: usize,
    shown_p: usize,
    kinds: BTreeMap<String, usize>,
}

fn pair_tables(r: &Run<'_>, atom_len: &HashMap<usize, usize>) -> Tables {
    let mut t = Tables {
        pairs: format!("{PAIRS_HEAD}\n"),
        names: format!("{NAMES_HEAD}\n"),
        by_file: BTreeMap::new(),
        per_atom: BTreeMap::new(),
        shown_n: 0,
        shown_p: 0,
        kinds: BTreeMap::new(),
    };
    for (i, f) in r.ours.funs.iter().enumerate() {
        if !r.shown(f) {
            continue;
        }
        t.shown_n = t.shown_n.saturating_add(1);
        let Some(j) = r.pairs.o2s.get(&i).copied() else {
            continue;
        };
        t.shown_p = t.shown_p.saturating_add(1);
        let (Some(g), Some(fo), Some(fs)) =
            (r.sib.funs.get(j), r.ours.feats.get(i), r.sib.feats.get(j))
        else {
            continue;
        };
        let (round, why) = r.pairs.why.get(&i).cloned().unwrap_or_default();
        let k = t
            .kinds
            .entry(why.split(':').next().unwrap_or("").to_string())
            .or_insert(0);
        *k = k.saturating_add(1);
        let atom = r.sib.atom.get(j).copied().unwrap_or(0);
        let own: HashSet<&String> = fo.strs.iter().collect();
        let sib_file = fs.raw.iter().find(|s| is_source_path(s));
        let mut toks: Vec<(String, String)> = Vec::new();
        for (key, raw) in fs.strs.iter().zip(fs.raw.iter()) {
            if own.contains(key) || is_source_path(raw) {
                continue;
            }
            for tok in name_tokens(raw) {
                toks.push((tok, raw.clone()));
            }
        }
        let va = r.our_va(f);
        let sva = r.sib_va(g.rva);
        let _ = writeln!(
            t.pairs,
            "{}\t{va:#x}\t{}\t{}\t{sva:#x}\t{atom}\t{}\t{round}\t{why}\t{:.2}\t{}\t{}\t{}\t{}\t{}|{}\t{}",
            f.image,
            f.file,
            f.name,
            atom_len.get(&atom).copied().unwrap_or(1),
            shape(&fo.mnem, &fs.mnem),
            fo.mnem.len(),
            fs.mnem.len(),
            sib_file.map_or("", |s| s.as_str()),
            toks.iter()
                .map(|x| x.0.as_str())
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect::<Vec<_>>()
                .join(","),
            fo.conv,
            fs.conv,
            clash(&fo.conv, &fs.conv).unwrap_or_default()
        );
        for (tok, raw) in &toks {
            let _ = writeln!(
                t.names,
                "{}\t{va:#x}\t{}\t{}\t{sva:#x}\t{round}\t{tok}\t{}",
                f.image,
                f.file,
                f.name,
                cut(raw, 80)
            );
        }
        t.by_file
            .entry((f.image.clone(), f.file.clone()))
            .or_default()
            .push((f.rva, j));
        t.per_atom.entry(atom).or_default().push((i, j));
    }
    t
}

fn atoms_table(
    r: &Run<'_>,
    per_atom: &BTreeMap<usize, Vec<(usize, usize)>>,
    atom_len: &HashMap<usize, usize>,
) -> (String, usize) {
    let mut at = format!("{ATOMS_HEAD}\n");
    let mut crossings = 0usize;
    for (a, mem) in per_atom {
        let mut files: BTreeMap<&str, usize> = BTreeMap::new();
        for (i, _) in mem {
            let f = r.ours.funs.get(*i).map_or("", |f| f.file.as_str());
            let e = files.entry(f).or_insert(0);
            *e = e.saturating_add(1);
        }
        if files.len() < 2 {
            continue;
        }
        crossings = crossings.saturating_add(1);
        let members = r
            .sib
            .funs
            .iter()
            .zip(r.sib.atom.iter())
            .filter(|(_, x)| *x == a);
        let lo = members.clone().map(|(f, _)| f.rva).min().unwrap_or(0);
        let hi = members.map(|(f, _)| f.end).max().unwrap_or(0);
        let mut ms: Vec<&(usize, usize)> = mem.iter().collect();
        ms.sort_by_key(|(_, j)| r.sib.funs.get(*j).map_or(0, |f| f.rva));
        let _ = writeln!(
            at,
            "{a}\t{:#x}\t{:#x}\t{}\t{}\t{}\t{}",
            r.sib_va(lo),
            r.sib_va(hi),
            atom_len.get(a).copied().unwrap_or(0),
            mem.len(),
            files
                .iter()
                .map(|(f, n)| format!("{f}:{n}"))
                .collect::<Vec<_>>()
                .join(" "),
            ms.iter()
                .filter_map(|(i, j)| {
                    let (f, g) = (r.ours.funs.get(*i)?, r.sib.funs.get(*j)?);
                    Some(format!(
                        "{:#x}={}@{}<-{:#x}",
                        r.sib_va(g.rva),
                        f.name,
                        base_name(&f.file),
                        r.our_va(f)
                    ))
                })
                .collect::<Vec<_>>()
                .join(" ")
        );
    }
    (at, crossings)
}

fn atom_names(r: &Run<'_>, per_atom: &BTreeMap<usize, Vec<(usize, usize)>>) -> String {
    let mut an = String::from("atom\tsib_va\ttoken\tstring\tpaired_in_atom\n");
    for (a, mem) in per_atom {
        let held: Vec<String> = mem
            .iter()
            .filter_map(|(i, _)| r.ours.funs.get(*i).map(|f| f.name.clone()))
            .collect();
        for (j, g) in r.sib.funs.iter().enumerate() {
            if r.sib.atom.get(j) != Some(a) || r.pairs.s2o.contains_key(&j) {
                continue;
            }
            let Some(fs) = r.sib.feats.get(j) else {
                continue;
            };
            for raw in fs.raw.iter().filter(|s| !is_source_path(s)) {
                for t in name_tokens(raw) {
                    let _ = writeln!(
                        an,
                        "{a}\t{:#x}\t{t}\t{}\t{}",
                        r.sib_va(g.rva),
                        cut(raw, 80),
                        held.join(",")
                    );
                }
            }
        }
    }
    an
}

fn order_table(r: &Run<'_>, by_file: &BTreeMap<(String, String), Vec<(u32, usize)>>) -> String {
    let mut ord = format!("{ORDER_HEAD}\n");
    for ((img, file), v) in by_file {
        let mut v = v.clone();
        v.sort_unstable();
        let s: Vec<u32> = v
            .iter()
            .filter_map(|(_, j)| r.sib.funs.get(*j).map(|f| f.rva))
            .collect();
        let (inv, tot) = inversions(&s);
        let runs = reversed_runs(&s, REVERSED_RUN);
        let _ = writeln!(
            ord,
            "{img}\t{file}\t{}\t{inv}\t{tot}\t{}\t{}",
            v.len(),
            runs.iter()
                .map(|(st, n)| format!("{st}+{n}"))
                .collect::<Vec<_>>()
                .join(","),
            v.iter()
                .filter_map(|(rv, j)| {
                    let f = r
                        .ours
                        .funs
                        .iter()
                        .find(|f| f.rva == *rv && &f.image == img)?;
                    let g = r.sib.funs.get(*j)?;
                    Some(format!(
                        "{}={:#x}/a{}",
                        f.name,
                        r.sib_va(g.rva),
                        r.sib.atom.get(*j).copied().unwrap_or(0)
                    ))
                })
                .collect::<Vec<_>>()
                .join(" ")
        );
    }
    ord
}

fn runs_line(
    seq: &[&str],
    label: &dyn Fn(usize, usize) -> Option<String>,
) -> BTreeMap<String, (usize, usize, String)> {
    let mut by: BTreeMap<&str, Vec<(usize, usize)>> = BTreeMap::new();
    for (st, n) in file_runs(seq) {
        if let Some(f) = seq.get(st) {
            by.entry(f).or_default().push((st, n));
        }
    }
    by.iter()
        .map(|(f, rs)| {
            let intr = intruders(seq, rs)
                .iter()
                .filter_map(|(k, n)| label(*k, *n))
                .collect::<Vec<_>>()
                .join(" ");
            (
                (*f).to_string(),
                (rs.len(), rs.iter().map(|r| r.1).sum::<usize>(), intr),
            )
        })
        .collect()
}

fn layout_tables(r: &Run<'_>) -> (String, String) {
    let mut laid: Vec<(u32, String, String)> = r
        .pairs
        .s2o
        .iter()
        .filter_map(|(j, i)| {
            let (g, f) = (r.sib.funs.get(*j)?, r.ours.funs.get(*i)?);
            r.shown(f).then(|| (g.rva, f.file.clone(), f.name.clone()))
        })
        .collect();
    laid.sort_unstable();
    let seq: Vec<&str> = laid.iter().map(|(_, f, _)| f.as_str()).collect();
    let mut rn = String::from("file\truns\tpaired\tintruded_by\n");
    let label = |k: usize, n: usize| {
        let (va, file, name) = laid.get(k)?;
        Some(format!(
            "{}x{n}@{:#x}:{name}",
            base_name(file),
            r.sib_va(*va)
        ))
    };
    for (f, (runs, paired, intr)) in runs_line(&seq, &label) {
        let _ = writeln!(rn, "{f}\t{runs}\t{paired}\t{intr}");
    }
    let mut own = String::from("image\tfile\truns\tfunctions\tintruded_by\n");
    for img in r.images {
        let mine: Vec<&Fun> = r
            .ours
            .funs
            .iter()
            .filter(|f| f.image == img.label && r.shown(f) && !f.file.is_empty())
            .collect();
        let seq: Vec<&str> = mine.iter().map(|f| f.file.as_str()).collect();
        let label = |k: usize, n: usize| {
            let g = mine.get(k)?;
            Some(format!(
                "{}x{n}@{:#x}:{}",
                base_name(&g.file),
                r.our_va(g),
                g.name
            ))
        };
        for (f, (runs, n, intr)) in runs_line(&seq, &label) {
            let _ = writeln!(own, "{}\t{f}\t{runs}\t{n}\t{intr}", img.label);
        }
    }
    (rn, own)
}

fn own_names(r: &Run<'_>) -> String {
    let mut out = String::from("image\tva\tfile\tname\ttoken\tstring\n");
    for (i, f) in r.ours.funs.iter().enumerate() {
        let Some(ft) = r.ours.feats.get(i).filter(|_| r.shown(f)) else {
            continue;
        };
        for raw in ft.raw.iter().filter(|s| !is_source_path(s)) {
            for t in name_tokens(raw) {
                let _ = writeln!(
                    out,
                    "{}\t{:#x}\t{}\t{}\t{t}\t{}",
                    f.image,
                    r.our_va(f),
                    f.file,
                    f.name,
                    cut(raw, 80)
                );
            }
        }
    }
    out
}

fn sib_table(r: &Run<'_>) -> String {
    let mut sx = String::from(
        "sib_va\tatom\tn\tours_image\tours_va\tours_file\tours_name\tsrc\tstrings\tcalls\n",
    );
    for (j, g) in r.sib.funs.iter().enumerate() {
        let Some(fs) = r.sib.feats.get(j) else {
            continue;
        };
        let ours_of = r.pairs.s2o.get(&j).and_then(|i| r.ours.funs.get(*i));
        let _ = writeln!(
            sx,
            "{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
            r.sib_va(g.rva),
            r.sib.atom.get(j).copied().unwrap_or(0),
            fs.mnem.len(),
            ours_of.map_or("", |f| f.image.as_str()),
            ours_of.map_or_else(String::new, |f| format!("{:#x}", r.our_va(f))),
            ours_of.map_or("", |f| f.file.as_str()),
            ours_of.map_or("", |f| f.name.as_str()),
            fs.raw
                .iter()
                .find(|s| is_source_path(s))
                .map_or("", |s| s.as_str()),
            fs.raw
                .iter()
                .filter(|s| !is_source_path(s))
                .take(6)
                .map(|s| cut(s, 60))
                .collect::<Vec<_>>()
                .join(" | "),
            fs.calls
                .iter()
                .take(48)
                .map(|c| match c {
                    Callee::Fn(x) => r
                        .sib
                        .funs
                        .get(*x)
                        .map_or_else(String::new, |t| format!("{:x}", r.sib_va(t.rva))),
                    Callee::Import(n) => n.rsplit('!').next().unwrap_or(n).to_string(),
                    Callee::Indirect => "*".to_string(),
                })
                .collect::<Vec<_>>()
                .join(" ")
        );
    }
    sx
}

#[must_use]
pub fn report(r: &Run<'_>, out_dir: &str) -> Report {
    let atom_len = r.atom_len();
    let t = pair_tables(r, &atom_len);
    let (atoms, crossings) = atoms_table(r, &t.per_atom, &atom_len);
    let order = order_table(r, &t.by_file);
    let (runs, own_runs) = layout_tables(r);
    let multi = atom_len.values().filter(|n| **n > 1).count();
    let anchored: Vec<(usize, usize)> = r
        .pairs
        .why
        .iter()
        .filter(|(_, (_, w))| w.starts_with("anchor:"))
        .filter_map(|(a, _)| Some((*a, *r.pairs.o2s.get(a)?)))
        .collect();
    let clashing = anchored
        .iter()
        .filter(|(a, b)| !fits(r.ours, r.sib, *a, *b))
        .count();
    let label = r.label;
    let mut summary = String::new();
    let _ = writeln!(
        summary,
        "{label}: sibling {} functions, {multi} atoms of 2+ (largest {}); ours {} functions; {} pairs in all; \
         shown {} of {} paired ({}); {crossings} atoms cross our files; signatures refused {} call / {} caller / {} shape-tie \
         / {} vftable / {} layout proposal(s), {clashing} of {} anchor pairs clash",
        r.sib.funs.len(),
        atom_len.values().max().copied().unwrap_or(0),
        r.ours.funs.len(),
        r.pairs.o2s.len(),
        t.shown_p,
        t.shown_n,
        t.kinds
            .iter()
            .map(|(k, n)| format!("{k} {n}"))
            .collect::<Vec<_>>()
            .join(", "),
        r.pairs.refused_call.len(),
        r.pairs.refused_caller.len(),
        r.pairs.refused_tie.len(),
        r.pairs.refused_slot.len(),
        r.pairs.refused_layout.len(),
        anchored.len()
    );
    let _ = writeln!(
        summary,
        "wrote {out_dir}/{label}.{{pairs,names,atoms,order,runs,sib}}.tsv"
    );
    let atom_rows: String = atoms.lines().skip(1).map(|l| format!("{l}\n")).collect();
    let inverted: String = order
        .lines()
        .skip(1)
        .filter(|l| {
            l.split('\t')
                .nth(3)
                .and_then(|x| x.parse::<usize>().ok())
                .is_some_and(|n| n > 0)
        })
        .map(|l| format!("{l}\n"))
        .collect();
    let files = vec![
        (format!("{label}.pairs.tsv"), t.pairs),
        (format!("{label}.names.tsv"), t.names),
        (format!("{label}.atoms.tsv"), atoms),
        (format!("{label}.atomnames.tsv"), atom_names(r, &t.per_atom)),
        (format!("{label}.order.tsv"), order),
        (format!("{label}.runs.tsv"), runs),
        ("ours.runs.tsv".to_string(), own_runs),
        ("ours.names.tsv".to_string(), own_names(r)),
        (format!("{label}.sib.tsv"), sib_table(r)),
    ];
    Report {
        files,
        summary,
        atom_rows,
        inverted,
    }
}

#[must_use]
pub fn union_tables(
    list: &str,
    rows: &[(String, String, String, String)],
) -> (String, String, usize, usize) {
    let (mut atoms, mut order) = (String::new(), String::new());
    for (label, branch, at, inv) in rows {
        for l in at.lines() {
            let _ = writeln!(atoms, "{label}\t{branch}\t{l}");
        }
        for l in inv.lines() {
            let _ = writeln!(order, "{label}\t{branch}\t{l}");
        }
    }
    let head = format!("# msvc6t sibling pair --oracles {list}");
    let (na, no) = (atoms.lines().count(), order.lines().count());
    (
        format!(
            "{head}: every sibling ATOM (packed run, one object) whose pairs fall in two or more of our files.\n\
             oracle\tbranch\t{ATOMS_HEAD}\n{atoms}"
        ),
        format!(
            "{head}: each of our files with an INVERTED pair: its pairs in OUR order against the sibling's \
             addresses, the inverted pairs and the reversed runs (first index+length).\n\
             oracle\tbranch\t{ORDER_HEAD}\n{order}"
        ),
        na,
        no,
    )
}
