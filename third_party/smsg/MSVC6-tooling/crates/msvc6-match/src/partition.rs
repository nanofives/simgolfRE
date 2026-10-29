use msvc6_diff::env::Env;
use msvc6_project::mapfile::Map;
use std::collections::{BTreeMap, BTreeSet};

#[must_use]
pub fn owned_by_our_sources(mangled: &str, cpp: &str) -> bool {
    let looks_defined = |needle: &str| -> bool {
        cpp.lines().any(|l| {
            if l.starts_with(char::is_whitespace) || l.is_empty() || l.contains(';') {
                return false;
            }
            let Some(pos) = l.find(needle) else {
                return false;
            };
            let after_ok = l
                .get(pos.saturating_add(needle.len())..)
                .map(str::trim_start)
                .is_some_and(|r| r.starts_with('('));
            let before_ok = l
                .get(..pos)
                .and_then(|s| s.chars().last())
                .is_none_or(|c| !c.is_alphanumeric() && c != '_' && c != ':');
            after_ok && before_ok
        })
    };
    let body = mangled.strip_prefix('?').unwrap_or(mangled);
    if body.starts_with('?') {
        return false;
    }
    let mut parts = body.split('@');
    let Some(name) = parts.next() else {
        return false;
    };
    match parts.next() {
        Some(cls) if !cls.is_empty() => looks_defined(&format!("{cls}::{name}")),
        _ => looks_defined(name),
    }
}

#[must_use]
pub fn map_objects(map: &Map) -> BTreeMap<String, String> {
    map.symbols
        .iter()
        .filter(|s| s.section == 1 && s.lib.is_none() && s.object.ends_with(".obj"))
        .map(|s| (s.name.clone(), s.object.clone()))
        .collect()
}

#[derive(Debug, Default)]
pub struct Partition {
    pub symbols: usize,
    pub excluded: usize,
    pub runs: Vec<(String, Vec<(u32, String)>)>,
}

impl Partition {
    #[must_use]
    pub fn distinct(&self) -> usize {
        self.runs
            .iter()
            .map(|(o, _)| o.as_str())
            .collect::<BTreeSet<_>>()
            .len()
    }

    #[must_use]
    pub fn attributable(&self) -> usize {
        self.runs.iter().map(|(_, v)| v.len()).sum()
    }
}

#[must_use]
pub fn measure(env: &Env, map: &Map, cpp: &str) -> Partition {
    let objs = map_objects(map);
    let text = env.text();
    let mut rows: Vec<(u32, String, String)> = Vec::new();
    let mut excluded = 0usize;
    for (name, obj) in &objs {
        let Some(rva) = env.names.rva(name, text, env.text_rva) else {
            continue;
        };
        if owned_by_our_sources(name, cpp) {
            rows.push((rva, obj.clone(), name.clone()));
        } else {
            excluded += 1;
        }
    }
    rows.sort();
    let mut runs: Vec<(String, Vec<(u32, String)>)> = Vec::new();
    for (rva, obj, name) in &rows {
        match runs.last_mut() {
            Some((o, v)) if o == obj => v.push((*rva, name.clone())),
            _ => runs.push((obj.clone(), vec![(*rva, name.clone())])),
        }
    }
    Partition {
        symbols: objs.len(),
        excluded,
        runs,
    }
}

fn stem(o: &str) -> &str {
    o.strip_suffix(".obj").unwrap_or(o)
}

#[must_use]
pub fn lines(label: &str, p: &Partition, moves: bool, list: bool) -> Vec<String> {
    let mut out = vec![
        format!("{label}: TU PARTITION against the reference's own-code layout"),
        format!(
            "  {} own symbol(s) in the map; {} attributable, {} excluded as foldable or not ours",
            p.symbols,
            p.attributable(),
            p.excluded
        ),
        format!(
            "  RUNS: {} over {} object(s)  -- one run per object would mean our partition matches",
            p.runs.len(),
            p.distinct()
        ),
        format!(
            "  {}",
            p.runs
                .iter()
                .map(|(o, v)| format!("{} x{}", stem(o), v.len()))
                .collect::<Vec<_>>()
                .join(", ")
        ),
    ];
    if list {
        out.push("  RUN MEMBERS, in reference address order:".to_string());
        for (obj, v) in &p.runs {
            out.push(format!("    {} x{}", stem(obj), v.len()));
            for (rva, name) in v {
                out.push(format!("      {rva:#08x}  {name}"));
            }
        }
    }
    if moves {
        out.push(
            "  CANDIDATE MOVES -- a run whose neighbours are the SAME other object:".to_string(),
        );
        let mut any = false;
        for w in p.runs.windows(3) {
            let [prev, cur, next] = w else { continue };
            if prev.0 == next.0 && cur.1.len() <= 4 {
                any = true;
                for (rva, name) in &cur.1 {
                    out.push(format!("    {rva:#08x}  {name}"));
                    out.push(format!("        {} -> {}", stem(&cur.0), stem(&prev.0)));
                }
            }
        }
        if !any {
            out.push(
                "    NONE -- no run of four or fewer has matching neighbours on both sides"
                    .to_string(),
            );
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    const SRC: &str = "int Field::getKind(Cell *p)\n{\n    return p->getKind();\n}\n\
                       unsigned char *LoadImage(const char *f, int **p)\n{\n    return 0;\n}\n\
                       void InitGrid(int a, int b)\n{\n}\n\
                       void Field::loadNewKind(int t)\n{\n}\n\
                       int myLoadImage(int x)\n{\n    return x;\n}\n";

    #[test]
    fn ownership_matches_eleven_known_answers() {
        let cases: &[(&str, bool)] = &[
            ("?LoadImage@@YAPAEPBDPAPAUtagBITMAPINFOHEADER@@@Z", true),
            ("?InitGrid@@YAXHH@Z", true),
            ("?loadNewKind@Field@@QAEXH@Z", true),
            ("?getKind@Field@@QAEHPAVCell@@@Z", true),
            ("?getKind@Cell@@QAEDXZ", false),
            ("?setEdge@Cell@@QAEXHH_N@Z", false),
            ("?maxHeight@Cell@@QAEHXZ", false),
            ("?getline@istream@@QAEAAV1@PADHD@Z", false),
            ("?fail@ios@@QBEHXZ", false),
            ("??0CellIterBase@@QAE@XZ", false),
            ("??2@YAPAXIPAX@Z", false),
        ];
        for (sym, want) in cases {
            assert_eq!(
                owned_by_our_sources(sym, SRC),
                *want,
                "{sym} should be {want}"
            );
        }
        assert!(!owned_by_our_sources("?oadImage@@YAXXZ", SRC));
    }
}
