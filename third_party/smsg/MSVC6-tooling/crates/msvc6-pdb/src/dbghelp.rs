use crate::report::Report;
use anyhow::{Result, bail};
use std::collections::BTreeMap;
use std::fmt::Write as _;

pub const TAG_FUNCTION: u32 = 5;
pub const SEATED: &str = "seated";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Row {
    pub address: u32,
    pub name: String,
    pub symbol: String,
    pub size: u32,
    pub seat: String,
    pub tu: Option<String>,
    pub line: Option<u32>,
    next: Option<u32>,
    next_continues: bool,
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Answer {
    pub sym: String,
    pub disp: Option<u32>,
    pub size: Option<u32>,
    pub tag: Option<u32>,
    pub file: Option<String>,
    pub line: Option<u32>,
    pub sig: String,
    pub und: String,
    pub sigmatch: String,
    pub locals: Option<usize>,
}

impl Answer {
    #[must_use]
    pub fn named(&self) -> bool {
        !self.sym.is_empty() && self.sym != "-"
    }

    #[must_use]
    pub fn is_function(&self) -> bool {
        self.tag == Some(TAG_FUNCTION)
    }
}

fn fields(line: &str) -> BTreeMap<&str, &str> {
    line.trim_end_matches(['\r', '\n'])
        .split('\t')
        .filter_map(|f| f.split_once('='))
        .collect()
}

fn opt_u32(m: &BTreeMap<&str, &str>, k: &str) -> Option<u32> {
    m.get(k).and_then(|v| v.parse().ok())
}

#[must_use]
pub fn answer(line: &str) -> (u32, Answer) {
    let m = fields(line);
    let at = line
        .split('\t')
        .nth(1)
        .and_then(|a| u32::from_str_radix(a, 16).ok())
        .unwrap_or(0);
    let text = |k: &str| {
        m.get(k)
            .filter(|v| **v != "-")
            .map_or_else(String::new, |v| (*v).to_string())
    };
    let (file, line_no) = match m.get("line").filter(|v| **v != "-") {
        Some(v) => match v.rsplit_once(':') {
            Some((f, n)) => (Some(f.to_string()), n.parse().ok()),
            None => (Some((*v).to_string()), None),
        },
        None => (None, None),
    };
    (
        at,
        Answer {
            sym: text("sym"),
            disp: opt_u32(&m, "disp"),
            size: opt_u32(&m, "size"),
            tag: opt_u32(&m, "tag"),
            file,
            line: line_no,
            sig: text("sig"),
            und: text("und"),
            sigmatch: text("sigmatch"),
            locals: m.get("locals").and_then(|v| v.parse().ok()),
        },
    )
}

#[must_use]
pub fn answers(text: &str) -> Vec<(u32, Answer)> {
    text.lines()
        .filter(|l| l.starts_with("Q\t"))
        .map(answer)
        .collect()
}

fn link(out: &mut [Row], continuation: bool) {
    for i in 0..out.len() {
        let next = out.get(i + 1).map(|r| r.address);
        let cont = continuation
            && match (out.get(i), out.get(i + 1)) {
                (Some(a), Some(b)) => b.name.starts_with(&format!("{}+0x", a.name)),
                _ => false,
            };
        if let Some(r) = out.get_mut(i) {
            r.next = next;
            r.next_continues = cont;
        }
    }
}

pub fn rows_of_report(r: &Report, what: &str) -> Result<Vec<Row>> {
    let mut by_addr: BTreeMap<u32, Row> = BTreeMap::new();
    for x in &r.rows {
        let Some(a) = x.address.and_then(|v| u32::try_from(v).ok()) else {
            continue;
        };
        if x.name.is_empty() || !x.has_recomp {
            continue;
        }
        by_addr.insert(
            a,
            Row {
                address: a,
                name: x.name.clone(),
                symbol: x.symbol_or_name().to_string(),
                size: x.size.and_then(|n| u32::try_from(n).ok()).unwrap_or(0),
                seat: x.seat.clone().unwrap_or_else(|| "?".to_string()),
                tu: (!x.tu.is_empty()).then(|| x.tu.clone()),
                line: x.line.and_then(|n| u32::try_from(n).ok()),
                next: None,
                next_continues: false,
            },
        );
    }
    let mut out: Vec<Row> = by_addr.into_values().collect();
    link(&mut out, true);
    if out.is_empty() {
        bail!("{what} paired no address at all");
    }
    Ok(out)
}

pub fn rows_of_expect(text: &str, what: &str) -> Result<Vec<Row>> {
    let mut out: Vec<Row> = Vec::new();
    for line in text.lines().skip(1) {
        let mut f = line.split('\t');
        let (Some(va), Some(kind), Some(names)) = (f.next(), f.next(), f.next()) else {
            continue;
        };
        if kind != "function" {
            continue;
        }
        let Ok(address) = u32::from_str_radix(va.trim_start_matches("0x"), 16) else {
            bail!("{what}: `{va}` is not a hex address");
        };
        let name = names.split('|').next().unwrap_or_default().to_string();
        out.push(Row {
            address,
            symbol: name.clone(),
            name,
            size: 0,
            seat: SEATED.to_string(),
            tu: None,
            line: None,
            next: None,
            next_continues: false,
        });
    }
    out.sort_by_key(|r| r.address);
    out.dedup_by_key(|r| r.address);
    link(&mut out, false);
    if out.is_empty() {
        bail!("{what} lists no function");
    }
    Ok(out)
}

#[must_use]
pub fn sample(rows: &[Row], n: usize) -> Vec<&Row> {
    if rows.len() <= n {
        return rows.iter().collect();
    }
    (0..n)
        .map(|i| i.saturating_mul(rows.len()) / n)
        .filter_map(|i| rows.get(i))
        .collect()
}

#[must_use]
pub fn picked_rows(all: &[Row], n: usize) -> Vec<&Row> {
    let own: Vec<&Row> = all.iter().filter(|r| r.seat == SEATED).collect();
    if n == 0 {
        all.iter().collect()
    } else if own.len() >= n {
        (0..n)
            .map(|i| i.saturating_mul(own.len()) / n)
            .filter_map(|i| own.get(i).copied())
            .collect()
    } else {
        sample(all, n)
    }
}

#[must_use]
pub fn queries(rows: &[&Row], interior: bool) -> String {
    let mut out = String::new();
    for r in rows {
        let _ = writeln!(out, "{:x} {}", r.address, r.symbol);
        if interior && r.size > 1 {
            let _ = writeln!(out, "{:x} {}", r.address + r.size / 2, r.symbol);
        }
    }
    out
}

#[must_use]
pub fn addresses(rows: &[&Row], interior: bool) -> Vec<u32> {
    let mut out = Vec::new();
    for r in rows {
        out.push(r.address);
        if interior && r.size > 1 {
            out.push(r.address + r.size / 2);
        }
    }
    out
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Tally {
    pub rows: usize,
    pub named: usize,
    pub at_start: usize,
    pub not_a_function: usize,
    pub name_matches: usize,
    pub size_matches: usize,
    pub size_within: usize,
    pub size_overruns: usize,
    pub with_line: usize,
    pub file_matches: usize,
    pub line_close: usize,
    pub line_compared: usize,
    pub sig_yes: usize,
    pub sig_no: usize,
    pub sig_only: usize,
    pub with_locals: usize,
    pub interior_named: usize,
    pub interior_rows: usize,
}

#[must_use]
pub fn same_name(reported: &str, symbol: &str, got: &str) -> bool {
    let norm = |s: &str| {
        s.replace("class ", "")
            .replace("struct ", "")
            .replace("union ", "")
            .replace("enum ", "")
            .replace(' ', "")
    };
    got == reported || got == symbol || norm(got) == norm(reported) || norm(got) == norm(symbol)
}

#[must_use]
pub fn base(p: &str) -> String {
    p.rsplit(['\\', '/'])
        .next()
        .unwrap_or(p)
        .to_ascii_lowercase()
}

#[must_use]
pub fn line_close(reported: u32, got: u32) -> bool {
    got >= reported && got - reported <= 3
}

pub fn score(
    rows: &[&Row],
    answers: &[(u32, Answer)],
    interior: bool,
    out: &mut Vec<String>,
) -> Tally {
    let mut t = Tally::default();
    let mut it = answers.iter();
    for r in rows {
        let Some((_, a)) = it.next() else { break };
        t.rows += 1;
        if a.named() {
            t.named += 1;
        }
        let at_start = a.named() && a.disp == Some(0);
        if at_start {
            t.at_start += 1;
            if !a.is_function() {
                t.not_a_function += 1;
                out.push(format!(
                    "  TAG   {:#010x} {}: dbghelp has {} here as a tag-{} symbol, not a function",
                    r.address,
                    r.name,
                    a.sym,
                    a.tag.unwrap_or(0)
                ));
            }
            if same_name(&r.name, &r.symbol, &a.sym) {
                t.name_matches += 1;
            } else {
                out.push(format!(
                    "  NAME  {:#010x}: the report says {} and dbghelp says {}",
                    r.address, r.name, a.sym
                ));
            }
            let over = !r.next_continues
                && a.size
                    .zip(r.next)
                    .is_some_and(|(n, next)| r.address.saturating_add(n) > next);
            if r.size == 0 || a.size == Some(r.size) {
                t.size_matches += 1;
            } else if over {
                t.size_overruns += 1;
                out.push(format!(
                    "  SIZE  {:#010x} {}: the report has {} byte(s) and dbghelp {}, which runs past \
                     the next function at {:#010x} -- a record for ANOTHER function at this address",
                    r.address,
                    r.name,
                    r.size,
                    a.size.unwrap_or(0),
                    r.next.unwrap_or(0)
                ));
            } else {
                t.size_within += 1;
            }
            match a.sigmatch.as_str() {
                "yes" => t.sig_yes += 1,
                "no" => {
                    t.sig_no += 1;
                    out.push(format!(
                        "  TYPE  {:#010x} {}: the PDB's types say {} and the mangled name {}",
                        r.address, r.name, a.sig, a.und
                    ));
                }
                _ if !a.sig.is_empty() => t.sig_only += 1,
                _ => {}
            }
            if a.locals.is_some_and(|n| n > 0) {
                t.with_locals += 1;
            }
            if let (Some(f), Some(l)) = (&a.file, a.line) {
                t.with_line += 1;
                if r.tu.as_deref().is_some_and(|tu| base(tu) == base(f)) {
                    t.file_matches += 1;
                }
                if let Some(rl) = r.line {
                    t.line_compared += 1;
                    if line_close(rl, l) {
                        t.line_close += 1;
                    }
                }
            }
        }
        if interior && r.size > 1 {
            let Some((_, m)) = it.next() else { break };
            t.interior_rows += 1;
            if m.named() && same_name(&r.name, &r.symbol, &m.sym) {
                t.interior_named += 1;
            }
        }
    }
    t
}

#[must_use]
pub fn module_field(text: &str, key: &str) -> String {
    text.lines()
        .find(|l| l.starts_with("MODULE\t"))
        .and_then(|l| fields(l).get(key).map(|v| (*v).to_string()))
        .unwrap_or_else(|| "?".to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(address: u32, name: &str, symbol: &str, size: u32, line: Option<u32>) -> Row {
        Row {
            address,
            name: name.to_string(),
            symbol: symbol.to_string(),
            size,
            seat: SEATED.to_string(),
            tu: Some("app/src/Bmp.cpp".to_string()),
            line,
            next: Some(address + size.max(1) + 16),
            next_continues: false,
        }
    }

    #[test]
    fn a_probe_row_parses_into_an_answer() {
        let (at, a) = answer(
            "Q\t100016a0\tsym=LoadBmp\tdisp=0\tsize=480\ttag=5\t\
             line=c:\\work\\app\\src\\bmp.cpp:32\tldisp=0\tsig=unsignedchar*()\t\
             class=-\tund=unsignedchar*()\tsigmatch=yes\tlocals=7\tpszFile,ppbi\r",
        );
        assert_eq!(at, 0x1000_16a0);
        assert_eq!(a.sym, "LoadBmp");
        assert_eq!((a.disp, a.size, a.tag), (Some(0), Some(480), Some(5)));
        assert_eq!(a.file.as_deref(), Some("c:\\work\\app\\src\\bmp.cpp"));
        assert_eq!(a.line, Some(32));
        assert_eq!(a.sigmatch, "yes");
        assert_eq!(a.locals, Some(7));
        assert!(a.named() && a.is_function());
    }

    #[test]
    fn an_unanswered_query_is_not_a_name() {
        let (at, a) = answer("Q\t4058e0\tsym=-\tdisp=-\tsize=-\ttag=-\tline=-\tldisp=-");
        assert_eq!(at, 0x0040_58e0);
        assert!(!a.named());
        assert_eq!(a.disp, None);
        assert_eq!(
            answers("BIND\tx\nQ\t10\tsym=a\tdisp=0\nMODULE\tx\n").len(),
            1
        );
    }

    #[test]
    fn a_name_at_the_wrong_address_or_of_the_wrong_size_is_not_a_match() {
        let rows = [
            row(0x1000_16a0, "LoadBmp", "?LoadBmp@@YAPAE@Z", 480, Some(31)),
            row(0x1000_1b60, "Swap", "?Swap@@YAX@Z", 116, Some(176)),
            row(0x0043_d630, "$E35", "_$E35", 10, None),
        ];
        let picked: Vec<&Row> = rows.iter().collect();
        let answers = vec![
            answer(
                "Q\t100016a0\tsym=LoadBmp\tdisp=0\tsize=480\ttag=5\tline=src\\Bmp.cpp:32\t\
                 sig=void()\tund=void()\tsigmatch=yes\tlocals=7\tx",
            ),
            answer("Q\t10001b60\tsym=Swap\tdisp=58\tsize=116\ttag=5\tline=-"),
            answer("Q\t43d630\tsym=$E35\tdisp=0\tsize=145\ttag=5\tline=-\tsigmatch=-"),
        ];
        let mut notes = Vec::new();
        let t = score(&picked, &answers, false, &mut notes);
        assert_eq!((t.rows, t.named, t.at_start, t.name_matches), (3, 3, 2, 2));
        assert_eq!(t.size_matches, 1);
        assert_eq!((t.size_overruns, t.size_within), (1, 0));
        assert_eq!((t.with_line, t.file_matches), (1, 1));
        assert_eq!((t.line_compared, t.line_close), (1, 1));
        assert_eq!((t.sig_yes, t.with_locals), (1, 1));
        assert!(
            notes
                .iter()
                .any(|n| n.contains("SIZE") && n.contains("$E35")),
            "{notes:?}"
        );
    }

    #[test]
    fn a_demangled_spelling_is_the_same_name() {
        assert!(same_name(
            "std::list<class Cell *,class std::allocator<class Cell *> >::clear",
            "?clear@?$list@PAVCell@@V?$allocator@PAVCell@@@std@@@std@@QAEXXZ",
            "std::list<Cell *,std::allocator<Cell *> >::clear"
        ));
        assert!(same_name("LoadBmp", "?LoadBmp@@YAPAE@Z", "LoadBmp"));
        assert!(!same_name("LoadBmp", "?LoadBmp@@YAPAE@Z", "Swap"));
    }

    #[test]
    fn the_sample_spreads_over_the_whole_range_and_queries_both_ends() {
        let rows: Vec<Row> = (0..100)
            .map(|i| row(0x40_1000 + i * 0x20, "f", "?f@@YAXXZ", 16, None))
            .collect();
        let picked = sample(&rows, 10);
        assert_eq!(picked.len(), 10);
        assert_eq!(picked[0].address, 0x40_1000);
        assert_eq!(picked[9].address, 0x40_1000 + 90 * 0x20);
        let q = queries(&picked, true);
        assert_eq!(q.lines().count(), 20);
        assert!(q.starts_with("401000 ?f@@YAXXZ\n401008 ?f@@YAXXZ\n"));
        assert_eq!(addresses(&picked, true).len(), 20);
        let few = sample(&rows[..3], 10);
        assert_eq!(few.len(), 3);
        assert_eq!(queries(&few, false).lines().count(), 3);
        assert_eq!(picked_rows(&rows, 0).len(), 100);
        assert_eq!(picked_rows(&rows, 5).len(), 5);
    }

    #[test]
    fn a_continuation_row_is_not_the_next_function() {
        let r = Report::parse(
            br#"{"data":[
            {"address":"0x10015960","recomp":"0x10015cc0","name":"_nh_malloc","x":{"size":93,"seat":"archive"}},
            {"address":"0x100159bd","recomp":"0x10015d1d","name":"_nh_malloc+0x5d","x":{"size":11,"seat":"archive"}},
            {"address":"0x10015a90","recomp":"0x10015df0","name":"_malloc_x","x":{"size":40,"seat":"archive"}},
            {"address":"0x10015b00","name":"unpaired","x":{"size":4}}]}"#,
            "x",
            "r",
        )
        .unwrap();
        let rows = rows_of_report(&r, "r").unwrap();
        assert_eq!(rows.len(), 3);
        assert!(rows[0].next_continues);
        assert!(!rows[1].next_continues);
        let picked: Vec<&Row> = vec![&rows[0], &rows[1]];
        let answers = vec![
            answer("Q\t10015960\tsym=_nh_malloc\tdisp=0\tsize=160\ttag=5\tline=-"),
            answer("Q\t100159bd\tsym=_nh_malloc\tdisp=93\tsize=160\ttag=5\tline=-"),
        ];
        let mut notes = Vec::new();
        let t = score(&picked, &answers, false, &mut notes);
        assert_eq!((t.size_overruns, t.size_within), (0, 1), "{notes:?}");
        assert!(notes.is_empty(), "{notes:?}");
    }

    #[test]
    fn an_expect_table_gives_its_functions_and_the_module_row_is_read_by_key() {
        let rows = rows_of_expect(
            "va\tkind\tnames\n00401010\tfunction\tg|_g\n00401000\tfunction\tf\n00402000\tdata\td\n",
            "e",
        )
        .unwrap();
        assert_eq!(
            rows.iter().map(|r| r.address).collect::<Vec<_>>(),
            [0x40_1000, 0x40_1010]
        );
        assert_eq!(rows[1].name, "g");
        assert!(rows_of_expect("va\tkind\tnames\nzz\tfunction\tf\n", "e").is_err());
        assert!(rows_of_expect("va\tkind\tnames\n", "e").is_err());
        let text = "BIND\tdbghelp.dll\tC:\\windows\\system32\\dbghelp.dll\n\
                    MODULE\tstruct=1680\tbase=10000000\tSymType=3(SymPdb)\tPdbSig=3c18cb6a\t\
                    PdbAge=23\tPdbUnmatched=0\n";
        assert_eq!(module_field(text, "SymType"), "3(SymPdb)");
        assert_eq!(module_field(text, "PdbAge"), "23");
        assert_eq!(module_field(text, "nosuch"), "?");
    }
}
