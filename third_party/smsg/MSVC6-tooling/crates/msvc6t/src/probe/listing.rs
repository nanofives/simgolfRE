use super::glob::collapse;
use std::collections::BTreeMap;

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Func {
    pub frame: Vec<String>,
    pub code: Vec<String>,
}

fn code_part(raw: &str) -> String {
    collapse(raw.split(';').next().unwrap_or_default())
}

fn frame_symbol(raw: &str) -> bool {
    let w: Vec<&str> = raw.split_whitespace().collect();
    matches!(w.as_slice(), [name, "=", _] if name.contains('$'))
}

fn opens(raw: &str) -> Option<&str> {
    if raw.starts_with(char::is_whitespace) {
        return None;
    }
    match raw.split_whitespace().collect::<Vec<_>>().as_slice() {
        [name, "PROC", ..] => Some(name),
        _ => None,
    }
}

fn closes(raw: &str) -> bool {
    !raw.starts_with(char::is_whitespace)
        && matches!(
            raw.split_whitespace().collect::<Vec<_>>().as_slice(),
            [_, "ENDP", ..]
        )
}

fn segment(raw: &str) -> bool {
    !raw.starts_with(char::is_whitespace)
        && matches!(
            raw.split_whitespace().collect::<Vec<_>>().as_slice(),
            [_, "SEGMENT", ..]
        )
}

#[must_use]
pub fn functions(asm: &str) -> BTreeMap<String, Func> {
    let mut out: BTreeMap<String, Func> = BTreeMap::new();
    let mut cur: Option<String> = None;
    let mut syms: Vec<String> = Vec::new();
    for raw in asm.lines() {
        if let Some(name) = opens(raw) {
            out.entry(name.to_string()).or_default().frame = std::mem::take(&mut syms);
            cur = Some(name.to_string());
            continue;
        }
        if closes(raw) {
            cur = None;
            continue;
        }
        let Some(f) = &cur else {
            if !raw.starts_with(char::is_whitespace) && frame_symbol(raw) {
                syms.push(collapse(raw));
            } else if segment(raw) {
                syms.clear();
            }
            continue;
        };
        let t = code_part(raw);
        if !t.is_empty() {
            out.entry(f.clone()).or_default().code.push(t);
        }
    }
    out
}

#[must_use]
pub fn data(asm: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut in_fn = false;
    for raw in asm.lines() {
        if opens(raw).is_some() {
            in_fn = true;
            continue;
        }
        if closes(raw) {
            in_fn = false;
            continue;
        }
        let t = code_part(raw);
        let title = raw.split_whitespace().next() == Some("TITLE");
        if in_fn || t.is_empty() || title || frame_symbol(raw) {
            continue;
        }
        out.push(t);
    }
    out
}

#[must_use]
pub fn text_lines(text: &str) -> Vec<String> {
    text.lines()
        .map(collapse)
        .filter(|l| !l.is_empty())
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const ASM: &str = "\tTITLE\tp.cpp\n_TEXT\tSEGMENT\n_a$ = 8\n_b$ = -4\n?f@@YAHH@Z PROC NEAR\t\t\t; f, COMDAT\n\n; 3    : int f(int a) {\n\n\tpush\tecx ; keep\n\tmov\teax, DWORD PTR _a$[esp]\n$L12:\n\tret\t0\n?f@@YAHH@Z ENDP\t\t\t\t\t; f\n_TEXT\tENDS\n_DATA\tSEGMENT\n_g\tDD\t01H\n_DATA\tENDS\nEND\n";

    #[test]
    fn a_function_holds_its_frame_symbols_and_code_without_comments() {
        let f = functions(ASM);
        let g = &f["?f@@YAHH@Z"];
        assert_eq!(g.frame, vec!["_a$ = 8", "_b$ = -4"]);
        assert_eq!(
            g.code,
            vec!["push ecx", "mov eax, DWORD PTR _a$[esp]", "$L12:", "ret 0"]
        );
    }

    #[test]
    fn the_data_view_is_every_line_outside_a_function() {
        assert_eq!(
            data(ASM),
            vec![
                "_TEXT SEGMENT",
                "_TEXT ENDS",
                "_DATA SEGMENT",
                "_g DD 01H",
                "_DATA ENDS",
                "END"
            ]
        );
    }

    #[test]
    fn tool_text_is_its_non_empty_lines() {
        assert_eq!(text_lines("p.cpp\r\n\n  x   y \n"), vec!["p.cpp", "x y"]);
    }
}
