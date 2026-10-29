use serde::Serialize;

pub const SEPARATE_ARG: [&str; 4] = ["/FI", "/D", "/U", "/I"];

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Class {
    Include,
    Define,
    Undefine,
    ForcedInclude,
    Pch,
    Output,
    Build,
    Code,
}

fn blank(c: char) -> bool {
    c == ' ' || c == '\t'
}

#[must_use]
pub fn tokens(line: &str) -> Vec<String> {
    let cs: Vec<char> = line.chars().collect();
    let at = |i: usize| cs.get(i).copied();
    let mut out = Vec::new();
    let mut i = 0usize;
    loop {
        while at(i).is_some_and(blank) {
            i = i.saturating_add(1);
        }
        if at(i).is_none() {
            break;
        }
        let mut cur = String::new();
        let mut quoted = false;
        while let Some(c) = at(i) {
            if blank(c) && !quoted {
                break;
            }
            if c == '\\' {
                let mut k = i;
                while at(k) == Some('\\') {
                    k = k.saturating_add(1);
                }
                let run = k.saturating_sub(i);
                if at(k) == Some('"') {
                    cur.extend(std::iter::repeat_n('\\', run / 2));
                    if run % 2 == 1 {
                        cur.push('"');
                        i = k.saturating_add(1);
                        continue;
                    }
                    i = k;
                } else {
                    cur.extend(std::iter::repeat_n('\\', run));
                    i = k;
                    continue;
                }
            }
            if at(i) == Some('"') {
                if quoted && at(i.saturating_add(1)) == Some('"') {
                    cur.push('"');
                    i = i.saturating_add(2);
                    quoted = false;
                } else {
                    quoted = !quoted;
                    i = i.saturating_add(1);
                }
                continue;
            }
            cur.push(c);
            i = i.saturating_add(1);
        }
        out.push(cur);
    }
    out
}

fn slash(t: &str) -> String {
    t.strip_prefix('-')
        .map_or_else(|| t.to_string(), |r| format!("/{r}"))
}

#[must_use]
pub fn normalize(toks: &[String]) -> Vec<String> {
    let mut out = Vec::new();
    let mut it = toks.iter();
    while let Some(t) = it.next() {
        let s = slash(t);
        if SEPARATE_ARG.contains(&s.as_str()) {
            match it.next() {
                Some(a) => out.push(format!("{s}{a}")),
                None => out.push(s),
            }
            continue;
        }
        out.push(s);
    }
    out
}

#[must_use]
pub fn normalize_line(line: &str) -> Vec<String> {
    normalize(&tokens(line))
}

#[must_use]
pub fn class(s: &str) -> Class {
    let has = |p: &str| s.starts_with(p);
    if has("/FI") {
        Class::ForcedInclude
    } else if has("/Fp") || has("/YX") || has("/Yc") || has("/Yu") || s == "/Yd" {
        Class::Pch
    } else if has("/Fo")
        || has("/Fd")
        || has("/Fa")
        || has("/FA")
        || has("/Fr")
        || has("/FR")
        || has("/Fe")
        || has("/Fm")
        || has("/Fx")
    {
        Class::Output
    } else if has("/I") {
        Class::Include
    } else if has("/D") {
        Class::Define
    } else if has("/U") || s == "/u" {
        Class::Undefine
    } else if matches!(s, "/nologo" | "/c" | "/FD" | "/Gm" | "/Gm-" | "/WX" | "/w")
        || (has("/W") && s.len() == 3 && s.as_bytes().get(2).is_some_and(u8::is_ascii_digit))
    {
        Class::Build
    } else {
        Class::Code
    }
}

#[must_use]
pub fn define_name(s: &str) -> Option<&str> {
    let body = s.strip_prefix("/D")?;
    Some(body.split('=').next().unwrap_or(body))
}

#[must_use]
pub fn effective(
    global_add: &[String],
    global_sub: &[String],
    file_add: &[String],
    file_sub: &[String],
) -> Vec<String> {
    let gsub = normalize(global_sub);
    let fsub = normalize(file_sub);
    let mut out: Vec<String> = normalize(global_add)
        .into_iter()
        .filter(|s| !gsub.contains(s) && !fsub.contains(s))
        .collect();
    out.extend(
        normalize(file_add)
            .into_iter()
            .filter(|s| !fsub.contains(s)),
    );
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn switches_join_their_separate_argument_and_lose_their_quotes() {
        let n = normalize_line(
            "/nologo /W3 /MT /O2 /D \"WIN32\" /D NDEBUG -DX=1 /I \"..\\a b\\include\" /Iinc /FI\"pre.h\" /YX /Fp\"Release/golf.pch\" /FD /c",
        );
        assert_eq!(
            n,
            [
                "/nologo",
                "/W3",
                "/MT",
                "/O2",
                "/DWIN32",
                "/DNDEBUG",
                "/DX=1",
                "/I..\\a b\\include",
                "/Iinc",
                "/FIpre.h",
                "/YX",
                "/FpRelease/golf.pch",
                "/FD",
                "/c"
            ]
        );
        let classes: Vec<Class> = n.iter().map(|s| class(s)).collect();
        assert_eq!(
            classes,
            [
                Class::Build,
                Class::Build,
                Class::Code,
                Class::Code,
                Class::Define,
                Class::Define,
                Class::Define,
                Class::Include,
                Class::Include,
                Class::ForcedInclude,
                Class::Pch,
                Class::Pch,
                Class::Build,
                Class::Build
            ]
        );
        assert_eq!(define_name("/DX=1"), Some("X"));
        assert_eq!(class("/GZ"), Class::Code);
        assert_eq!(class("/Gm"), Class::Build);
        assert_eq!(class("/Zl"), Class::Code);
        assert_eq!(class("/W3"), Class::Build);
        assert_eq!(class("/Wp64"), Class::Code);
    }

    #[test]
    fn a_file_adds_after_the_project_and_its_subtraction_removes_from_both() {
        let t = |s: &str| tokens(s);
        let e = effective(
            &t("/nologo /MT /O2 /D \"A\" /GX"),
            &t("/GX"),
            &t("/Ob2 /D B"),
            &t("/O2"),
        );
        assert_eq!(e, ["/nologo", "/MT", "/DA", "/Ob2", "/DB"]);
    }

    #[test]
    fn an_escaped_quote_is_read_as_the_command_line_reads_it() {
        assert_eq!(
            normalize_line(r#"/D VERSION=\"1.0\" /D "NAME=\"a b\"" /D WIN32"#),
            [r#"/DVERSION="1.0""#, r#"/DNAME="a b""#, "/DWIN32"]
        );
        assert_eq!(define_name(r#"/DNAME="a b""#), Some("NAME"));
        assert_eq!(
            tokens(r#"/I "..\a\b" /Fo"Rel\\" /c"#),
            [r"/I", r"..\a\b", r"/FoRel\", "/c"]
        );
        assert_eq!(tokens(r#"a\\\"b c\d"#), [r#"a\"b"#, r"c\d"]);
        assert_eq!(tokens(r#""a""b""#), [r#"a"b"#]);
        assert_eq!(tokens(r#""a""b" "" x"#), [r#"a"b ""#, "x"]);
        assert_eq!(tokens(r#""" x"#), ["", "x"]);
        assert_eq!(tokens("  \t/O2\t/GX  "), ["/O2", "/GX"]);
        let e = effective(
            &tokens(r#"/D "NAME=\"a b\"" /D VERSION=\"1.0\""#),
            &[],
            &[],
            &tokens(r#"/D VERSION=\"1.0\""#),
        );
        assert_eq!(e, [r#"/DNAME="a b""#]);
    }
}
