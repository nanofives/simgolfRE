use std::collections::BTreeMap;
use std::path::Path;

pub const MAX_LISTING: u64 = 256 << 20;
pub const SPAN_LIST: usize = 12;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Site {
    pub offset: u32,
    pub line: u32,
    pub text: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Marker {
    pub line: u32,
    pub at: Option<u32>,
    pub text: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Proc {
    pub sites: Vec<Site>,
    pub markers: Vec<Marker>,
    pub labels: Vec<u64>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Listing {
    pub procs: BTreeMap<String, Proc>,
}

#[must_use]
pub fn read(path: &Path) -> Listing {
    let fits = std::fs::metadata(path).is_ok_and(|m| m.is_file() && m.len() <= MAX_LISTING);
    if !fits {
        return Listing::default();
    }
    std::fs::read(path).map_or_else(
        |_| Listing::default(),
        |b| parse(&msvc6_base::srcio::decode(&b)),
    )
}

fn marker(l: &str) -> Option<(u32, String)> {
    let rest = l.strip_prefix("; ")?;
    let (num, text) = rest.split_once(':')?;
    let num = num.trim();
    if num.is_empty() || !num.bytes().all(|b| b.is_ascii_digit()) {
        return None;
    }
    Some((num.parse().ok()?, text.trim().to_string()))
}

fn code_offset(l: &str) -> Option<u32> {
    if !l.starts_with(' ') && !l.starts_with('\t') {
        return None;
    }
    let field = l.split('\t').next()?.trim();
    if field.is_empty() || !field.bytes().all(|b| b.is_ascii_hexdigit()) {
        return None;
    }
    u32::from_str_radix(field, 16).ok()
}

fn labels_in(l: &str, out: &mut Vec<u64>) {
    let mut rest = l;
    while let Some(p) = rest.find("$L") {
        let tail = rest.get(p + 2..).unwrap_or_default();
        let digits: String = tail.chars().take_while(char::is_ascii_digit).collect();
        if let Ok(v) = digits.parse::<u64>() {
            out.push(v);
        }
        rest = tail;
    }
}

#[must_use]
pub fn parse(text: &str) -> Listing {
    let mut out = Listing::default();
    let mut current: Option<String> = None;
    let mut line = 0u32;
    let mut src = String::new();
    for raw in text.lines() {
        let l = raw.trim_end_matches('\r');
        if let Some(p) = l.find(" PROC NEAR") {
            let name = l.get(..p).unwrap_or_default().trim();
            if !name.is_empty() {
                current = Some(name.to_string());
                out.procs.entry(name.to_string()).or_default();
                line = 0;
                src.clear();
            }
            continue;
        }
        let Some(proc) = current.as_ref().and_then(|n| out.procs.get_mut(n)) else {
            continue;
        };
        if l.contains(" ENDP") {
            current = None;
            continue;
        }
        if let Some((n, t)) = marker(l) {
            line = n;
            src.clone_from(&t);
            proc.markers.push(Marker {
                line: n,
                at: None,
                text: t,
            });
            continue;
        }
        if l.starts_with(';') {
            continue;
        }
        labels_in(l, &mut proc.labels);
        let Some(offset) = code_offset(l) else {
            continue;
        };
        proc.sites.push(Site {
            offset,
            line,
            text: src.clone(),
        });
        for m in proc.markers.iter_mut().rev() {
            if m.at.is_some() {
                break;
            }
            m.at = Some(offset);
        }
    }
    for p in out.procs.values_mut() {
        p.sites.sort_by_key(|s| s.offset);
    }
    out
}

impl Listing {
    #[must_use]
    pub fn proc(&self, symbol: &str) -> Option<&Proc> {
        self.procs.get(symbol).or_else(|| {
            let mut all = self.procs.values();
            let one = all.next()?;
            all.next().is_none().then_some(one)
        })
    }

    pub fn retext(&mut self, texts: &BTreeMap<usize, String>) {
        for p in self.procs.values_mut() {
            for s in &mut p.sites {
                if let Some(t) = texts.get(&(s.line as usize)) {
                    s.text.clone_from(t);
                }
            }
            for m in &mut p.markers {
                if let Some(t) = texts.get(&(m.line as usize)) {
                    m.text.clone_from(t);
                }
            }
        }
    }

    #[must_use]
    pub fn first_label(&self, symbol: &str) -> Option<u64> {
        self.procs.get(symbol)?.labels.iter().min().copied()
    }
}

#[must_use]
pub fn site_at(sites: &[Site], offset: u32) -> Option<&Site> {
    sites
        .iter()
        .rev()
        .find(|s| s.offset <= offset)
        .filter(|s| s.line != 0)
}

#[must_use]
pub fn lines_in(p: &Proc, lo: usize, hi: usize) -> BTreeMap<u32, String> {
    let inside = |o: u32| (o as usize) >= lo && (o as usize) < hi;
    let mut out = BTreeMap::new();
    if let Some(s) = site_at(&p.sites, u32::try_from(lo).unwrap_or(u32::MAX)) {
        out.insert(s.line, s.text.clone());
    }
    for s in p.sites.iter().filter(|s| inside(s.offset) && s.line != 0) {
        out.entry(s.line).or_insert_with(|| s.text.clone());
    }
    for m in &p.markers {
        if m.at.is_some_and(inside) && m.line != 0 {
            out.entry(m.line)
                .or_insert_with(|| format!("{}  (group)", m.text));
        }
    }
    out
}

#[must_use]
pub fn line_span(lines: &BTreeMap<u32, String>) -> String {
    let keys: Vec<u32> = lines.keys().copied().collect();
    match (keys.first(), keys.last()) {
        (Some(a), Some(b)) if a == b => format!("source line {a}"),
        (Some(a), Some(b)) if keys.len() > SPAN_LIST => {
            format!("source lines {a}..{b} ({} lines)", keys.len())
        }
        (Some(a), Some(b)) => format!(
            "source lines {a}..{b} ({})",
            keys.iter()
                .map(u32::to_string)
                .collect::<Vec<_>>()
                .join(" ")
        ),
        _ => String::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = "\tTITLE\tZ:\\x.cpp\n_TEXT\tSEGMENT PARA USE32 PUBLIC 'CODE'\n;\tCOMDAT ?Go@T@@QAEXXZ\n\
?Go@T@@QAEXXZ PROC NEAR\t; T::Go, COMDAT\n\n; 494  : {\n\n  00000\t55\t\t push\t ebp\n  00001\t8b ec\t\t mov\t ebp, esp\n\n\
; 500  :     case 0:  this->F(g); break;\n\n  00035\t68 00 00 00 00\t push\t OFFSET FLAT:_g\n  0003a\t8b 4d fc\t mov\t ecx, \
DWORD PTR _this$[ebp]\n  0003d\te8 00 00 00 00\t call\t ?F@T@@QAEXPBD@Z ; T::F\n$L1207:\n  00042\teb fe\t\t jmp\t SHORT $L1205\n\
?Go@T@@QAEXXZ ENDP\t; T::Go\n";

    fn sites(l: &Listing) -> Vec<(u32, u32)> {
        l.proc("?Go@T@@QAEXXZ")
            .map(|p| p.sites.iter().map(|s| (s.offset, s.line)).collect())
            .unwrap_or_default()
    }

    #[test]
    fn each_code_line_is_bound_to_the_marker_above_it() {
        let l = parse(SAMPLE);
        assert_eq!(
            sites(&l),
            vec![
                (0, 494),
                (1, 494),
                (0x35, 500),
                (0x3a, 500),
                (0x3d, 500),
                (0x42, 500)
            ]
        );
        let p = l.proc("?Go@T@@QAEXXZ").unwrap();
        assert!(p.sites[2].text.contains("case 0"));
        assert_eq!(l.first_label("?Go@T@@QAEXXZ"), Some(1205));
        assert_eq!(l.first_label("?Other@@YAXXZ"), None);
    }

    #[test]
    fn a_byte_inside_an_instruction_belongs_to_the_instruction_that_starts_it() {
        let l = parse(SAMPLE);
        let p = l.proc("?Go@T@@QAEXXZ").unwrap();
        assert_eq!(
            site_at(&p.sites, 0x3b).map(|s| (s.offset, s.line)),
            Some((0x3a, 500))
        );
        let tail: Vec<Site> = p.sites.iter().skip(1).cloned().collect();
        assert!(site_at(&tail, 0).is_none());
        let lines = lines_in(p, 0x30, 0x40);
        assert_eq!(lines.keys().copied().collect::<Vec<_>>(), vec![494, 500]);
        assert_eq!(line_span(&lines), "source lines 494..500 (494 500)");
    }

    #[test]
    fn markers_with_no_code_of_their_own_head_the_next_instruction() {
        let body = "?Go@T@@QAEXXZ PROC NEAR\t; T::Go, COMDAT\n\n; 10   :   a();\n\n  00000\t90\t\t nop\n\n; 11   :   }\n\
; 12   :   i = i + 1;\n; 13   : } while (n);\n\n  00001\t42\t\t inc\t edx\n  00002\t75 fc\t\t jne\t SHORT $L1\n?Go@T@@QAEXXZ ENDP\n";
        let l = parse(body);
        let p = l.proc("?Go@T@@QAEXXZ").unwrap();
        let at: Vec<(u32, Option<u32>)> = p.markers.iter().map(|m| (m.line, m.at)).collect();
        assert_eq!(
            at,
            vec![(10, Some(0)), (11, Some(1)), (12, Some(1)), (13, Some(1))]
        );
        assert_eq!(sites(&l), vec![(0, 10), (1, 13), (2, 13)]);
        let lines = lines_in(p, 1, 4);
        assert_eq!(
            lines.get(&12).map(String::as_str),
            Some("i = i + 1;  (group)")
        );
    }

    #[test]
    fn a_missing_or_odd_listing_yields_no_annotation() {
        assert!(
            read(Path::new("/nonexistent/msvc6t/none.cod"))
                .procs
                .is_empty()
        );
        let l = parse("hello\n; not a marker\n  zzzz\tnope\n");
        assert!(l.procs.is_empty());
        let l = parse(
            "?Go@T@@QAEXXZ PROC NEAR\t; T::Go, COMDAT\n;\tCOMDAT ?Go@T@@QAEXXZ\n; T::Go\n  00000\t90\t\t nop\n",
        );
        assert_eq!(sites(&l), vec![(0, 0)]);
        let p = l.proc("anything").unwrap();
        assert!(site_at(&p.sites, 0).is_none());
        assert!(line_span(&lines_in(p, 0, 1)).is_empty());
    }

    #[test]
    fn a_listing_of_several_procs_is_read_by_exact_symbol() {
        let two = format!(
            "{SAMPLE}?Up@@YAXXZ PROC NEAR\n; 7    : x\n  00000\tc3\t\t ret\t 0\n?Up@@YAXXZ ENDP\n"
        );
        let l = parse(&two);
        assert_eq!(l.procs.len(), 2);
        assert!(l.proc("?Nope@@YAXXZ").is_none());
        assert_eq!(
            l.proc("?Up@@YAXXZ")
                .map(|p| p.sites.iter().map(|s| s.line).collect::<Vec<_>>()),
            Some(vec![7])
        );
    }

    #[test]
    fn a_long_span_is_summarised() {
        let many: BTreeMap<u32, String> = (100..113).map(|l| (l, String::new())).collect();
        assert_eq!(line_span(&many), "source lines 100..112 (13 lines)");
        let few: BTreeMap<u32, String> = [(7, String::new())].into_iter().collect();
        assert_eq!(line_span(&few), "source line 7");
    }

    #[test]
    fn a_listing_of_a_line_directed_file_takes_its_text_from_the_source() {
        let mut l = parse(SAMPLE);
        let texts: BTreeMap<usize, String> = [
            (494, "{".to_string()),
            (500, "case 0: this->F(g); break;".to_string()),
        ]
        .into_iter()
        .collect();
        l.retext(&texts);
        let p = l.proc("?Go@T@@QAEXXZ").unwrap();
        assert!(
            p.sites
                .iter()
                .all(|s| texts.get(&(s.line as usize)) == Some(&s.text))
        );
        assert_eq!(p.markers.first().map(|m| m.text.as_str()), Some("{"));
    }
}
