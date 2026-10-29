use crate::defs::{self, Def};
use crate::lex::{self, Lexed};
use crate::linepos::{self, MacroSet};
use serde::Serialize;
use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Block {
    pub name: String,
    pub first: usize,
    pub last: usize,
}

fn line_text(src: &[u8], lines: &[(usize, usize, usize)], n: usize) -> Vec<u8> {
    lines
        .get(n.saturating_sub(1))
        .and_then(|l| src.get(l.0..l.1))
        .unwrap_or_default()
        .to_vec()
}

fn only_ws(b: &[u8]) -> bool {
    b.iter()
        .all(|c| matches!(c, b' ' | b'\t' | b'\r' | 0x0b | 0x0c))
}

pub fn blocks(
    src: &[u8],
    lx: &Lexed,
    ds: &[Def],
    carry_macros: bool,
) -> Result<Vec<Block>, String> {
    let lines = lex::physical_lines(src);
    let comment_lines: BTreeMap<usize, bool> = {
        let mut m = BTreeMap::new();
        for c in &lx.comments {
            for l in c.first..=c.last {
                m.insert(l, true);
            }
        }
        m
    };
    let code_on = |n: usize| -> bool {
        lx.tokens
            .iter()
            .any(|t| t.line <= n && n <= t.end_line && t.directive.is_none())
    };
    let directive_on = |n: usize| -> Option<&str> {
        lx.directives
            .iter()
            .find(|d| d.first <= n && n <= d.last)
            .map(|d| d.name.as_str())
    };
    let mut out = Vec::with_capacity(ds.len());
    for d in ds.iter().filter(|d| !d.in_class) {
        let head_line = line_text(src, &lines, d.first_line);
        let head_col = lx.tokens.get(d.head).map_or(0, |t| t.start)
            - lines.get(d.first_line.saturating_sub(1)).map_or(0, |l| l.0);
        if !only_ws(head_line.get(..head_col).unwrap_or_default()) {
            return Err(format!(
                "{} starts mid-line {}: something else ends on its first line",
                d.qualified(),
                d.first_line
            ));
        }
        let close = lx.tokens.get(d.close).map_or(0, |t| t.end);
        let tail = lines
            .get(d.last_line.saturating_sub(1))
            .and_then(|l| src.get(close..l.1))
            .unwrap_or_default();
        let tail_ok = only_ws(tail)
            || lx.comments.iter().any(|c| {
                c.start >= close
                    && c.first == d.last_line
                    && only_ws(src.get(close..c.start).unwrap_or_default())
            });
        if !tail_ok {
            return Err(format!(
                "{} ends mid-line {}: something else starts after its `}}`",
                d.qualified(),
                d.last_line
            ));
        }
        let mut first = d.first_line;
        while first > 1 {
            let above = first - 1;
            let text = line_text(src, &lines, above);
            if only_ws(&text) {
                break;
            }
            let comment_only = comment_lines.contains_key(&above)
                && !code_on(above)
                && directive_on(above).is_none();
            let macro_line =
                carry_macros && matches!(directive_on(above), Some("define" | "undef"));
            if comment_only || macro_line {
                first = above;
                continue;
            }
            break;
        }
        out.push(Block {
            name: d.qualified(),
            first,
            last: d.last_line,
        });
    }
    for w in out.windows(2) {
        if let [a, b] = w
            && b.first <= a.last
        {
            return Err(format!(
                "{} (lines {}-{}) and {} (lines {}-{}) share a line",
                a.name, a.first, a.last, b.name, b.first, b.last
            ));
        }
    }
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Reordered {
    pub bytes: Vec<u8>,
    pub moved: usize,
    pub line_positions_moved: Vec<String>,
}

pub fn reorder(src: &[u8], order: &[String], carry_macros: bool) -> Result<Reordered, String> {
    let lx = lex::lex(src);
    let ds = defs::definitions(&lx);
    let bs = blocks(src, &lx, &ds, carry_macros)?;
    let mut chosen: Vec<usize> = Vec::with_capacity(order.len());
    for name in order {
        let picks: Vec<usize> = {
            let (base, nth) = match name.rsplit_once('#') {
                Some((b, n)) => (b, n.parse::<usize>().ok()),
                None => (name.as_str(), None),
            };
            let hits: Vec<usize> = bs
                .iter()
                .enumerate()
                .filter(|(_, b)| {
                    b.name == base
                        || b.name.rsplit("::").next() == Some(base) && !base.contains("::")
                })
                .map(|(i, _)| i)
                .collect();
            match nth {
                Some(n) => hits
                    .into_iter()
                    .nth(n.saturating_sub(1))
                    .into_iter()
                    .collect(),
                None => hits,
            }
        };
        match picks.as_slice() {
            [i] => {
                if chosen.contains(i) {
                    return Err(format!("`{name}` is named twice"));
                }
                chosen.push(*i);
            }
            [] => return Err(format!("no file-scope definition named `{name}`")),
            many => {
                return Err(format!(
                    "`{name}` names {} definitions: pick one with `{name}#N`",
                    many.len()
                ));
            }
        }
    }
    let mut slots: Vec<usize> = chosen.clone();
    slots.sort_unstable();
    let lines = lex::physical_lines(src);
    let piece = |b: &Block| -> Vec<u8> {
        let s = lines.get(b.first.saturating_sub(1)).map_or(0, |l| l.0);
        let e = lines
            .get(b.last.saturating_sub(1))
            .map_or(src.len(), |l| l.2);
        src.get(s..e).unwrap_or_default().to_vec()
    };
    let mut out = Vec::with_capacity(src.len());
    let mut at = 0usize;
    let mut moved = 0usize;
    for (slot, pick) in slots.iter().zip(&chosen) {
        let (Some(sb), Some(pb)) = (bs.get(*slot), bs.get(*pick)) else {
            continue;
        };
        let s = lines.get(sb.first.saturating_sub(1)).map_or(0, |l| l.0);
        let e = lines
            .get(sb.last.saturating_sub(1))
            .map_or(src.len(), |l| l.2);
        out.extend_from_slice(src.get(at..s).unwrap_or_default());
        let mut p = piece(pb);
        if !p.ends_with(b"\n") && e < src.len() {
            p.push(b'\n');
        }
        out.extend(p);
        if slot != pick {
            moved += 1;
        }
        at = e;
    }
    out.extend_from_slice(src.get(at..).unwrap_or_default());
    let mut a: Vec<&[u8]> = src
        .split(|&b| b == b'\n')
        .filter(|l| !l.is_empty())
        .collect();
    let mut b: Vec<&[u8]> = out
        .split(|&b| b == b'\n')
        .filter(|l| !l.is_empty())
        .collect();
    a.sort_unstable();
    b.sort_unstable();
    if a != b {
        return Err(
            "the output is not a permutation of the input's lines: nothing was written".to_string(),
        );
    }
    let after = defs::definitions(&lex::lex(&out));
    if after.len() != ds.len() {
        return Err(format!(
            "{} definitions in and {} out: nothing was written",
            ds.len(),
            after.len()
        ));
    }
    let m = MacroSet::builtin();
    let (lb, la) = (lex::lex(src), lex::lex(&out));
    let mut moved_pos = Vec::new();
    let (pb, pa) = (linepos::pins(&lb), linepos::pins(&la));
    let mut ua: BTreeMap<String, Vec<usize>> = BTreeMap::new();
    for u in linepos::uses(&la, &m).iter().filter(|u| u.line_pos) {
        ua.entry(u.spelling.clone())
            .or_default()
            .push(linepos::compiler_line(&pa, u.own_last).unwrap_or(0));
    }
    let mut ub: BTreeMap<String, Vec<usize>> = BTreeMap::new();
    for u in linepos::uses(&lb, &m).iter().filter(|u| u.line_pos) {
        ub.entry(u.spelling.clone())
            .or_default()
            .push(linepos::compiler_line(&pb, u.own_last).unwrap_or(0));
    }
    for (k, v) in &ub {
        let mut x = v.clone();
        let mut y = ua.get(k).cloned().unwrap_or_default();
        x.sort_unstable();
        y.sort_unstable();
        if x != y {
            moved_pos.push(format!("`{k}` numbered {x:?} before, {y:?} after"));
        }
    }
    Ok(Reordered {
        bytes: out,
        moved,
        line_positions_moved: moved_pos,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    const SRC: &[u8] = b"#include \"a.h\"\nint g_x;\n\nint a()\n{\n    return 1;\n}\n// about b\nint b() { return 2; }\n\nstatic int c(int x) {\n    return x;\n}\n";

    #[test]
    fn named_bodies_swap_slots_and_the_rest_stays() {
        let r = reorder(SRC, &["c".into(), "a".into()], false).unwrap();
        let text = msvc6_base::srcio::decode(&r.bytes);
        assert_eq!(
            text,
            "#include \"a.h\"\nint g_x;\n\nstatic int c(int x) {\n    return x;\n}\n// about b\nint b() { return 2; }\n\nint a()\n{\n    return 1;\n}\n"
        );
        assert_eq!(r.moved, 2);
        let r = reorder(SRC, &["b".into(), "a".into()], false).unwrap();
        let text = msvc6_base::srcio::decode(&r.bytes);
        assert!(
            text.starts_with("#include \"a.h\"\nint g_x;\n\n// about b\nint b() { return 2; }\n"),
            "{text}"
        );
    }

    #[test]
    fn a_shared_line_or_an_unknown_name_is_refused() {
        assert!(
            reorder(SRC, &["nothing".into()], false)
                .unwrap_err()
                .contains("no file-scope")
        );
        let squashed = b"int a() { return 1; } int b() { return 2; }\n";
        assert!(reorder(squashed, &["b".into(), "a".into()], false).is_err());
    }

    #[test]
    fn a_moved_line_position_is_reported() {
        let src = b"int a()\n{\n    return 1;\n}\nint b()\n{\n    return __LINE__;\n}\n";
        let r = reorder(src, &["b".into(), "a".into()], false).unwrap();
        assert_eq!(r.line_positions_moved.len(), 1);
    }
}
