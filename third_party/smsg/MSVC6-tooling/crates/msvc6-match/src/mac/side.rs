use super::feat::{Feat, api_key, normalize_calls};
use super::pair::{Fun, Side};
use super::{ppc, xmem};
use crate::annotations::{self, Row};
use crate::mangle::{self, Sig};
use crate::sibling::code::Code;
use crate::sibling::discover::functions as discovered;
use anyhow::{Context as _, Result, bail};
use msvc6_image::macho::{self, CPU_I386};
use msvc6_image::pe::Image;
use msvc6_image::pef;
use std::collections::{BTreeMap, BTreeSet, HashMap};

pub const MAX_SPAN: u64 = 0x4000;

pub const WALK_BUDGET: u64 = 4;

pub const WALK_ALLOWANCE: u64 = 1 << 20;

pub const MIN_WORD_RUN: usize = 4;

pub const MIN_WORD: usize = 3;

pub const OPERATORS: [&str; 4] = ["op_new", "op_delete", "op_new[]", "op_delete[]"];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SourceKind {
    Pef,
    MachO,
}

#[derive(Debug, Clone)]
pub struct Source {
    pub label: String,
    pub family: String,
    pub rank: u32,
    pub kind: SourceKind,
    pub path: std::path::PathBuf,
    pub twin: Option<String>,
}

pub fn parse_sources(text: &str) -> Result<Vec<Source>> {
    let mut out = Vec::new();
    for (n, l) in text.lines().enumerate() {
        if l.trim().is_empty() || l.starts_with('#') {
            continue;
        }
        let line = n.saturating_add(1);
        let c: Vec<&str> = l.split('\t').map(str::trim).collect();
        let (Some(label), Some(family), Some(rank), Some(kind), Some(path)) =
            (c.first(), c.get(1), c.get(2), c.get(3), c.get(4))
        else {
            bail!("line {line}: want label family rank kind path [twin]");
        };
        let kind = match *kind {
            "pef" => SourceKind::Pef,
            "macho" => SourceKind::MachO,
            k => bail!("line {line}: kind `{k}` is not pef or macho"),
        };
        out.push(Source {
            label: (*label).to_string(),
            family: (*family).to_string(),
            rank: rank
                .parse()
                .with_context(|| format!("line {line}: rank `{rank}`"))?,
            kind,
            path: crate::sibling::home_path(path),
            twin: c
                .get(5)
                .filter(|t| !t.is_empty() && **t != "-")
                .map(|t| (*t).to_string()),
        });
    }
    Ok(out)
}

#[derive(Debug, Default)]
pub struct Ours {
    pub label: String,
    pub kind: Vec<String>,
    pub in_scope: Vec<bool>,
    pub universe: BTreeSet<String>,
    pub words: BTreeSet<String>,
}

#[must_use]
pub fn choose_rows(rows: Vec<Row>, image: &str, prefer: &[String]) -> BTreeMap<u64, Row> {
    let mut by_va: BTreeMap<u64, Row> = BTreeMap::new();
    let preferred = |p: &str| prefer.iter().any(|x| p.starts_with(x.as_str()));
    for r in rows.into_iter().filter(|r| r.image == image) {
        match by_va.get(&r.va) {
            Some(e) if preferred(&e.path) || !preferred(&r.path) => {}
            _ => {
                by_va.insert(r.va, r);
            }
        }
    }
    by_va
}

fn our_sig(r: &Row, text: &str, code: &annotations::Code) -> (String, Option<Sig>) {
    let line = annotations::resolve(r, code).ok();
    let head = line.map_or_else(
        || annotations::head_of(&r.anchor),
        |l| annotations::definition_at(text, l),
    );
    let hint = if r.kind == "TEMPLATE" && r.name.contains("::") {
        r.name
            .rsplit_once("::")
            .map(|(c, _)| c.replace("struct ", "").replace("class ", ""))
    } else if !head.contains("::") && (r.path.ends_with(".h") || r.path.ends_with(".inl")) {
        line.and_then(|l| annotations::enclosing_class(text, l.saturating_sub(1)))
    } else {
        None
    };
    let sig = mangle::parse_text(&head, hint.as_deref());
    (head, sig)
}

fn words_of(mem: &xmem::XMem) -> BTreeSet<String> {
    let mut words = BTreeSet::new();
    for s in mem.secs.iter().filter(|s| !s.exec) {
        let mut cur: Vec<u8> = Vec::new();
        for b in &s.bytes {
            if b.is_ascii_graphic() || *b == b' ' {
                cur.push(*b);
                continue;
            }
            if cur.len() >= MIN_WORD_RUN {
                for w in msvc6_base::srcio::decode(&cur)
                    .split(|c: char| !(c.is_ascii_alphanumeric() || c == '_' || c == ':'))
                {
                    if w.len() >= MIN_WORD {
                        words.insert(w.to_string());
                        for part in w.split("::").filter(|p| p.len() >= MIN_WORD) {
                            words.insert(part.to_string());
                        }
                    }
                }
            }
            cur.clear();
        }
    }
    words
}

#[derive(Debug)]
pub struct OurInput<'a> {
    pub label: &'a str,
    pub img: &'a Image,
    pub spans: Vec<(u64, u64)>,
    pub rows: &'a BTreeMap<u64, Row>,
    pub texts: &'a BTreeMap<String, String>,
    pub scope: &'a [String],
}

#[must_use]
pub fn our_side(input: &OurInput<'_>) -> (Side, Ours) {
    let mem = xmem::XMem::from_pe(input.img);
    let mut spans = input.spans.clone();
    for va in input.rows.keys() {
        if !spans.iter().any(|(s, _)| s == va) {
            spans.push((*va, *va));
        }
    }
    spans.sort_unstable();
    spans.dedup_by_key(|s| s.0);
    let mut side = Side {
        label: input.label.to_string(),
        msvc: true,
        ..Side::default()
    };
    let mut ours = Ours {
        label: input.label.to_string(),
        ..Ours::default()
    };
    let mut code: HashMap<String, annotations::Code> = HashMap::new();
    let empty = String::new();
    for (k, (va, end)) in spans.iter().enumerate() {
        let next = spans.get(k.saturating_add(1)).map_or(u64::MAX, |s| s.0);
        let end = if *end <= *va || *end > next {
            next.min(va.saturating_add(MAX_SPAN))
        } else {
            *end
        };
        let (raw, sig, file, kind) = match input.rows.get(va) {
            Some(r) if r.kind == "LIBRARY" => (
                r.name.clone(),
                Some(Sig::named(&r.name)),
                String::new(),
                r.kind.clone(),
            ),
            Some(r) => {
                let text = input.texts.get(&r.path).unwrap_or(&empty);
                let lines = code
                    .entry(r.path.clone())
                    .or_insert_with(|| annotations::code_lines(text));
                let (head, sig) = our_sig(r, text, lines);
                (head, sig, r.path.clone(), r.kind.clone())
            }
            None => (String::new(), None, String::new(), String::new()),
        };
        ours.in_scope
            .push(input.scope.iter().any(|s| file.contains(s.as_str())));
        ours.kind.push(kind);
        side.funs.push(Fun {
            va: *va,
            end,
            raw,
            sig,
            file,
        });
    }
    let entry: HashMap<u64, usize> = side
        .funs
        .iter()
        .enumerate()
        .map(|(i, f)| (f.va, i))
        .collect();
    for name in mem.slots.values() {
        ours.universe.insert(api_key(name));
    }
    let lib: HashMap<u64, String> = side
        .funs
        .iter()
        .zip(ours.kind.iter())
        .filter(|(_, k)| k.as_str() == "LIBRARY")
        .map(|(f, _)| (f.va, api_key(&f.raw)))
        .collect();
    ours.universe.extend(lib.values().cloned());
    ours.universe
        .extend(OPERATORS.iter().map(|s| (*s).to_string()));
    side.feats = side
        .funs
        .iter()
        .enumerate()
        .map(|(i, f)| {
            xmem::features(
                &mem,
                f.va,
                f.end,
                i,
                &entry,
                &|va| lib.get(&va).cloned(),
                true,
            )
        })
        .collect();
    for x in &mut side.feats {
        normalize_calls(x, &ours.universe);
    }
    side.finish();
    ours.words = words_of(&mem);
    (side, ours)
}

fn api_of(universe: &BTreeSet<String>, name: &str, s: Option<&Sig>) -> Option<String> {
    let c_name = s.is_none_or(|s| s.scope.is_empty() && !s.params_known);
    let k = api_key(name);
    (c_name && universe.contains(&k)).then_some(k)
}

#[must_use]
pub fn run_file(rest: &str) -> String {
    if let Some(s) = rest.strip_suffix("_cpp") {
        return format!("{s}.cpp");
    }
    if let Some(s) = rest.strip_suffix("_CPP") {
        return format!("{s}.CPP");
    }
    rest.to_string()
}

pub const SINIT: &str = "__sinit_";

pub fn pef_side(label: &str, bytes: &[u8], universe: &BTreeSet<String>) -> Result<Side> {
    let p = pef::load(bytes)?;
    let walked = p.tables.iter().fold(0u64, |s, t| {
        s.saturating_add(u64::from(t.at.saturating_sub(t.start)))
    });
    let cap = u64::try_from(p.code.len())
        .unwrap_or(u64::MAX)
        .saturating_mul(WALK_BUDGET)
        .saturating_add(WALK_ALLOWANCE);
    if walked > cap {
        bail!(
            "{label}: the traceback tables' functions span {walked} bytes of a {}-byte code section, more than \
             {cap}; their tb_offsets overlap",
            p.code.len()
        );
    }
    let mut run: Vec<String> = vec![String::new(); p.tables.len()];
    let mut open = 0usize;
    for (i, t) in p.tables.iter().enumerate() {
        if let Some(rest) = t.function().strip_prefix(SINIT) {
            let name = format!("run:{}", run_file(rest));
            for x in run.iter_mut().take(i.saturating_add(1)).skip(open) {
                x.clone_from(&name);
            }
            open = i.saturating_add(1);
        }
    }
    let mut side = Side {
        label: label.to_string(),
        ..Side::default()
    };
    for (t, file) in p.tables.iter().zip(run) {
        side.funs.push(Fun {
            va: u64::from(t.start),
            end: u64::from(t.at),
            raw: t.function().to_string(),
            sig: mangle::cw(t.function()),
            file,
        });
    }
    let entry: HashMap<u32, usize> = p
        .tables
        .iter()
        .enumerate()
        .map(|(i, t)| (t.start, i))
        .collect();
    let apis: Vec<Option<String>> = side
        .funs
        .iter()
        .map(|f| api_of(universe, &f.raw, f.sig.as_ref()))
        .collect();
    side.feats = p
        .tables
        .iter()
        .enumerate()
        .map(|(i, t)| {
            ppc::features(&p, t.start, t.at, i, &entry, &|x| {
                apis.get(x).cloned().flatten()
            })
        })
        .collect();
    finish(&mut side, universe);
    Ok(side)
}

pub fn macho_side(label: &str, bytes: &[u8], universe: &BTreeSet<String>) -> Result<Side> {
    let m = macho::load(bytes, CPU_I386)?;
    let mem = xmem::XMem::from_macho(&m);
    let mut side = Side {
        label: label.to_string(),
        ..Side::default()
    };
    for (va, end, name) in m.functions() {
        side.funs.push(Fun {
            va,
            end,
            sig: mangle::itanium(&name),
            raw: name,
            file: String::new(),
        });
    }
    let entry: HashMap<u64, usize> = side
        .funs
        .iter()
        .enumerate()
        .map(|(i, f)| (f.va, i))
        .collect();
    let apis: HashMap<u64, String> = side
        .funs
        .iter()
        .filter_map(|f| api_of(universe, &f.raw, f.sig.as_ref()).map(|k| (f.va, k)))
        .collect();
    side.feats = side
        .funs
        .iter()
        .enumerate()
        .map(|(i, f)| {
            xmem::features(
                &mem,
                f.va,
                f.end,
                i,
                &entry,
                &|va| apis.get(&va).cloned(),
                false,
            )
        })
        .collect();
    finish(&mut side, universe);
    Ok(side)
}

pub fn source_side(src: &Source, bytes: &[u8], universe: &BTreeSet<String>) -> Result<Side> {
    match src.kind {
        SourceKind::Pef => pef_side(&src.label, bytes, universe),
        SourceKind::MachO => macho_side(&src.label, bytes, universe),
    }
}

fn finish(side: &mut Side, universe: &BTreeSet<String>) {
    for x in &mut side.feats {
        normalize_calls(x, universe);
    }
    side.finish();
}

#[must_use]
pub fn pc_side(label: &str, img: &Image, universe: &BTreeSet<String>) -> Side {
    let base = u64::from(img.opt.image_base.get());
    let mem = xmem::XMem::from_pe(img);
    let mut side = Side {
        label: label.to_string(),
        msvc: true,
        ..Side::default()
    };
    for (rva, end) in discovered(&Code::new(img)) {
        side.funs.push(Fun {
            va: base.saturating_add(u64::from(rva)),
            end: base.saturating_add(u64::from(end)),
            ..Fun::default()
        });
    }
    let entry: HashMap<u64, usize> = side
        .funs
        .iter()
        .enumerate()
        .map(|(i, f)| (f.va, i))
        .collect();
    side.feats = side
        .funs
        .iter()
        .enumerate()
        .map(|(i, f)| xmem::features(&mem, f.va, f.end, i, &entry, &|_| None, true))
        .collect::<Vec<Feat>>();
    finish(&mut side, universe);
    side
}
