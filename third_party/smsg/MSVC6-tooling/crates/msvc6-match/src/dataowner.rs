use msvc6_image::pe::Image;
use msvc6_project::mapfile::{Map, is_member};
use std::collections::BTreeMap;

pub const PAIR_BUDGET: usize = 1 << 24;

pub type Spans = BTreeMap<(String, String), (u64, u64, usize)>;

fn section_key(n: u16) -> String {
    format!("{n:04x}:")
}

fn data_rows(map: &Map) -> impl Iterator<Item = (&msvc6_project::mapfile::Symbol, String)> {
    map.symbols
        .iter()
        .filter(|s| s.section > 1)
        .map(|s| (s, s.owner_token()))
        .filter(|(_, o)| o.ends_with(".obj"))
}

#[must_use]
pub fn spans(map: &Map) -> Spans {
    let mut out: Spans = BTreeMap::new();
    for (s, obj) in data_rows(map) {
        let v = u64::from(s.va);
        let e = out
            .entry((section_key(s.section), obj))
            .or_insert((v, v, 0));
        e.0 = e.0.min(v);
        e.1 = e.1.max(v);
        e.2 += 1;
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Interruption {
    pub section: String,
    pub object: String,
    pub other: String,
    pub va: u64,
}

#[derive(Debug, Default)]
pub struct Interruptions {
    pub rows: Vec<Interruption>,
    pub exhausted: bool,
}

#[must_use]
pub fn interruptions(sp: &Spans, map: &Map, budget: usize) -> Interruptions {
    let mut by_sec: BTreeMap<String, Vec<(u64, String)>> = BTreeMap::new();
    for (s, obj) in data_rows(map) {
        by_sec
            .entry(section_key(s.section))
            .or_default()
            .push((u64::from(s.va), obj));
    }
    for v in by_sec.values_mut() {
        v.sort();
    }
    let mut out = Interruptions::default();
    let mut budget = budget;
    for ((sec, obj), (lo, hi, _)) in sp {
        let Some(rows) = by_sec.get(sec) else {
            continue;
        };
        let a = rows.partition_point(|(v, _)| *v <= *lo);
        let b = rows.partition_point(|(v, _)| *v < *hi);
        for (v, other) in rows.get(a..b.max(a)).unwrap_or_default() {
            if budget == 0 {
                out.exhausted = true;
                return out;
            }
            budget -= 1;
            if other != obj {
                out.rows.push(Interruption {
                    section: sec.clone(),
                    object: obj.clone(),
                    other: other.clone(),
                    va: *v,
                });
            }
        }
    }
    out
}

#[must_use]
pub fn data_symbols(map: &Map) -> Vec<(u64, String, String)> {
    let mut out: Vec<(u64, String, String)> = data_rows(map)
        .map(|(s, obj)| (u64::from(s.rva), s.name.clone(), obj))
        .collect();
    out.sort_by_key(|r| r.0);
    out.dedup_by_key(|r| r.0);
    out
}

#[must_use]
pub fn owner_of(local: &[(u64, String, String)], rva: u64) -> Option<(usize, bool)> {
    let idx = local.partition_point(|s| s.0 <= rva);
    if idx == 0 {
        return None;
    }
    Some((idx - 1, idx == local.len()))
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Share {
    pub bytes: usize,
    pub past_last: usize,
}

pub type Shares = BTreeMap<(String, String, String), Share>;

#[derive(Debug, Default)]
pub struct Attribution {
    pub shares: Shares,
    pub per_section: BTreeMap<String, (usize, usize)>,
    pub unattributed: usize,
    pub total: usize,
}

#[must_use]
pub fn attribute(
    reference: &Image,
    candidate: &Image,
    syms: &[(u64, String, String)],
) -> Attribution {
    let mut out = Attribution::default();
    for rs in &reference.sections {
        if rs.name == ".text" {
            continue;
        }
        let Some(cs) = candidate.sections.iter().find(|c| c.name == rs.name) else {
            continue;
        };
        let rb = reference.section_bytes(rs);
        let cb = candidate.section_bytes(cs);
        let base = u64::from(rs.rva.get());
        let n = rb.len().min(cb.len());
        let end = base.saturating_add(n as u64);
        let first = syms.partition_point(|s| s.0 < base);
        let last = syms.partition_point(|s| s.0 < end);
        let local = syms.get(first..last).unwrap_or_default();
        for (i, (x, y)) in rb.iter().zip(cb).enumerate() {
            if x == y {
                continue;
            }
            out.total += 1;
            let ps = out.per_section.entry(rs.name.clone()).or_insert((0, 0));
            ps.0 += 1;
            let rva = base.saturating_add(i as u64);
            let Some((idx, past_last)) = owner_of(local, rva) else {
                out.unattributed += 1;
                ps.1 += 1;
                continue;
            };
            let Some((_, sym, obj)) = local.get(idx) else {
                out.unattributed += 1;
                ps.1 += 1;
                continue;
            };
            let e = out
                .shares
                .entry((rs.name.clone(), obj.clone(), sym.clone()))
                .or_default();
            e.bytes += 1;
            if past_last {
                e.past_last += 1;
            }
        }
    }
    out
}

fn clip(s: &str, n: usize) -> &str {
    let end = s
        .char_indices()
        .map(|(i, c)| i + c.len_utf8())
        .take_while(|e| *e <= n)
        .last()
        .unwrap_or(0);
    s.get(..end).unwrap_or(s)
}

#[must_use]
pub fn span_lines(sp: &Spans) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    for ((sec, obj), (lo, hi, n)) in sp {
        if *sec != cur {
            out.push(format!("  section {sec}"));
            cur.clone_from(sec);
        }
        out.push(format!(
            "    {obj:20} {lo:#012x}..{hi:#012x}  {:6} byte(s), {n} symbol(s)",
            hi.saturating_sub(*lo)
        ));
    }
    out
}

#[must_use]
pub fn interruption_lines(ints: &Interruptions) -> Vec<String> {
    let (mem, exp): (Vec<&Interruption>, Vec<&Interruption>) =
        ints.rows.iter().partition(|i| is_member(&i.object));
    let mut out = vec![format!(
        "  INTERRUPTIONS among EXPLICIT objects: {}   among ARCHIVE MEMBERS: {}",
        exp.len(),
        mem.len()
    )];
    for i in exp.iter().take(6) {
        out.push(format!(
            "    section {}  {}'s HULL contains {}'s symbol at {:#x}",
            i.section, i.object, i.other, i.va
        ));
    }
    for i in mem.iter().take(4) {
        out.push(format!(
            "    section {}  {}'s span contains {}'s symbol at {:#x}   (archive member, expected)",
            i.section, i.object, i.other, i.va
        ));
    }
    if ints.exhausted {
        out.push(String::from(
            "    the interruption scan ran out of its row checks: the counts are a floor",
        ));
    }
    out
}

#[must_use]
pub fn divergence_lines(syms: usize, a: &Attribution) -> Vec<String> {
    let attributed: usize = a.shares.values().map(|s| s.bytes).sum();
    let past: usize = a.shares.values().map(|s| s.past_last).sum();
    let ours: usize = a
        .shares
        .iter()
        .filter(|((_, o, _), _)| !is_member(o))
        .map(|(_, s)| s.bytes)
        .sum();
    let lib = attributed.saturating_sub(ours);
    let mut out = vec![
        format!(
            "  DIFFERING DATA BYTES BY OWNER -- POPULATION: {syms} DATA symbol row(s); .text excluded"
        ),
        format!("    differing DATA bytes      {:>8}", a.total),
        format!("      attributed to a symbol  {attributed:>8}"),
        format!(
            "      UNATTRIBUTED (below the first symbol of their section)  {:>8}",
            a.unattributed
        ),
        format!("    OURS (explicit objects)   {ours:>8}"),
        format!("    LIBRARY (archive members) {lib:>8}"),
        "    per section:".to_string(),
    ];
    for (name, (tot, un)) in &a.per_section {
        let so: usize = a
            .shares
            .iter()
            .filter(|((s, o, _), _)| s == name && !is_member(o))
            .map(|(_, v)| v.bytes)
            .sum();
        let sl: usize = a
            .shares
            .iter()
            .filter(|((s, o, _), _)| s == name && is_member(o))
            .map(|(_, v)| v.bytes)
            .sum();
        out.push(format!(
            "      {name:8} {tot:>7} differing   OURS {so:>6}   library {sl:>6}   unattributed {un:>6}"
        ));
    }
    if past > 0 {
        out.push(format!(
            "    of the attributed, {past} byte(s) sit PAST THE LAST SYMBOL of their section, whose extent the map does not give"
        ));
    }
    let mut rows: Vec<_> = a.shares.iter().collect();
    rows.sort_by_key(|(_, s)| std::cmp::Reverse(s.bytes));
    out.push("    largest shares:".to_string());
    for ((sec, obj, sym), s) in rows.iter().take(12) {
        let weak = if s.past_last > 0 { "  PAST-LAST" } else { "" };
        let side = if is_member(obj) { "library" } else { "OURS" };
        out.push(format!(
            "      {:>7} B  {sec:7} {side:7}  {obj:22} {}{weak}",
            s.bytes,
            clip(sym, 38)
        ));
    }
    if ours == 0 {
        out.push("    NO DIFFERING DATA BYTE IS OWNED BY ONE OF OUR OBJECTS".to_string());
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    const MAP: &str = " t\r\n\r\n Preferred load address is 10000000\r\n\r\n\
        \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
        \x200001:00000690  ?LoadImage@@YAXXZ    100016a0 f   image.obj\r\n\
        \x200002:00000024  _g_szMode            1005f024     image.obj\r\n\
        \x200002:00000030  _g_fScale            1005f030     image.obj\r\n\
        \x200002:00000368  _g_szRow             1005f368     grid.obj\r\n\
        \x200002:00000594  _g_fZoom             1005f594     grid.obj\r\n\
        \x200002:00000600  __real@one           1005f600     libcmtd:xtoa.obj\r\n";

    fn map(text: &str) -> Map {
        Map::parse(std::path::Path::new("t.map"), text).unwrap_or_else(|e| panic!("{e:#}"))
    }

    #[test]
    fn spans_are_per_section_and_object_and_exclude_text() {
        let s = spans(&map(MAP));
        assert_eq!(
            s.get(&("0002:".to_string(), "image.obj".to_string()))
                .copied(),
            Some((0x1005_f024, 0x1005_f030, 2))
        );
        assert!(!s.keys().any(|(sec, _)| sec == "0001:"));
        assert!(interruptions(&s, &map(MAP), PAIR_BUDGET).rows.is_empty());
    }

    #[test]
    fn an_interleaved_map_is_detected_and_owners_are_bounded_at_both_ends() {
        let bad = " t\r\n\r\n Preferred load address is 10000000\r\n\r\n\
            \x20 Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
            \x200002:00000024  _a  1005f024     image.obj\r\n\
            \x200002:00000100  _b  1005f100     grid.obj\r\n\
            \x200002:00000200  _c  1005f200     image.obj\r\n";
        let m = map(bad);
        let got = interruptions(&spans(&m), &m, PAIR_BUDGET);
        assert_eq!(got.rows.len(), 1);
        assert_eq!(got.rows.first().map(|g| g.other.as_str()), Some("grid.obj"));
        let syms = data_symbols(&map(MAP));
        assert_eq!(syms.first().map(|r| r.0), Some(0x5_f024));
        assert_eq!(owner_of(&syms, 0x5_f000), None);
        assert_eq!(owner_of(&syms, 0xf_ffff), Some((syms.len() - 1, true)));
        assert_eq!(owner_of(&syms, 0x5_f025), Some((0, false)));
    }
}
