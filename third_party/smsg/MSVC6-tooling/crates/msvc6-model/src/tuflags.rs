use serde::Serialize;
use std::collections::BTreeMap;

pub const RVA_DIGITS: std::ops::RangeInclusive<usize> = 1..=8;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Flag {
    Certain,
    Absent,
    Unknown,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub rva: u32,
    pub flag: Flag,
}

#[must_use]
pub fn member_rvas(src: &str) -> Vec<u32> {
    let mut out = Vec::new();
    for line in src.lines() {
        let t = line.trim_start().trim_start_matches('*').trim_start();
        let Some(rest) = t.strip_prefix("0x") else {
            continue;
        };
        let hex: String = rest.chars().take_while(char::is_ascii_hexdigit).collect();
        if !RVA_DIGITS.contains(&hex.len()) {
            continue;
        }
        let after = rest.get(hex.len()..).unwrap_or_default().trim_start();
        let tail = after.strip_prefix('+').unwrap_or(after);
        if !tail.starts_with("0x") {
            continue;
        }
        if let Ok(r) = u32::from_str_radix(&hex, 16) {
            out.push(r);
        }
    }
    out.dedup();
    out
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Carve {
    pub eh_data: bool,
    pub cflags: Option<Vec<String>>,
}

#[must_use]
pub fn effective<'a>(c: &'a Carve, default: &'a [String]) -> &'a [String] {
    c.cflags.as_deref().unwrap_or(default)
}

#[must_use]
pub fn classify(c: Option<&Carve>, default: &[String], switch: &str) -> Flag {
    match c {
        None => Flag::Unknown,
        Some(c) => {
            if c.eh_data
                || effective(c, default)
                    .iter()
                    .any(|x| x.eq_ignore_ascii_case(switch))
            {
                Flag::Certain
            } else {
                Flag::Absent
            }
        }
    }
}

#[must_use]
pub fn refuted(kinds: &[Flag]) -> bool {
    kinds.contains(&Flag::Certain) && kinds.contains(&Flag::Absent)
}

#[must_use]
pub fn resolve_rate(rvas: &[u32], is_carve: &dyn Fn(u32) -> bool) -> (usize, usize) {
    (rvas.iter().filter(|r| is_carve(**r)).count(), rvas.len())
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Verdicts {
    pub refuted: usize,
    pub agree: usize,
    pub withheld: usize,
}

#[derive(Debug)]
pub struct Candidate<'a> {
    pub name: &'a str,
    pub rvas: Vec<u32>,
}

#[must_use]
pub fn report(
    label: &str,
    switch: &str,
    candidates: &[Candidate<'_>],
    carves: &BTreeMap<u32, Carve>,
    default: &[String],
) -> (Verdicts, Vec<String>) {
    let mut v = Verdicts::default();
    let mut out = vec![
        format!(
            "{label}: DOES A CANDIDATE TU SPAN A `{switch}' TRANSITION? If it does, it is refuted BEFORE a compile."
        ),
        format!(
            "  POPULATION: {} candidate(s) against {} carve row(s). A TU is compiled with ONE flag set, so members \
             that disagree about `{switch}' cannot be one object.",
            candidates.len(),
            carves.len()
        ),
        format!(
            "  EH data in the image is attested `{switch}'. Its ABSENCE proves nothing, so only a row whose \
             effective flags lack `{switch}' counts against, and a row with no carve counts neither way."
        ),
    ];
    for c in candidates {
        if c.rvas.is_empty() {
            v.withheld += 1;
            out.push(format!(
                "  NO ROWS   {:<44} no per-COMDAT row parsed, so no member list to classify",
                c.name
            ));
            continue;
        }
        let (hit, total) = resolve_rate(&c.rvas, &|r| carves.contains_key(&r));
        let kinds: Vec<Flag> = c
            .rvas
            .iter()
            .map(|r| classify(carves.get(r), default, switch))
            .collect();
        let count = |f: Flag| kinds.iter().filter(|k| **k == f).count();
        let (certain, absent, unknown) = (
            count(Flag::Certain),
            count(Flag::Absent),
            count(Flag::Unknown),
        );
        if refuted(&kinds) {
            v.refuted += 1;
            out.push(format!(
                "  REFUTED BY FLAGS  {:<36} {total} parsed row(s), {hit} resolve to a carve: {certain} certain, \
                 {absent} default-or-recorded-without, {unknown} unknown -- cannot be ONE object",
                c.name
            ));
        } else {
            v.agree += 1;
            out.push(format!(
                "  flags agree       {:<36} {total} parsed row(s), {hit} resolve to a carve: {certain} certain, \
                 {absent} default-or-recorded-without, {unknown} unknown",
                c.name
            ));
        }
    }
    out.push(format!(
        "  -> {} candidate(s) REFUTED BY FLAGS, {} whose flags are consistent with one object, {} with no \
         per-COMDAT row.{}",
        v.refuted,
        v.agree,
        v.withheld,
        if v.refuted == 0 {
            " No candidate is refuted this way, which is not the same as any being proven: flag agreement is a \
             NECESSARY condition for one object and nothing more."
        } else {
            " A refuted candidate needs splitting at its flag boundary, not recompiling."
        }
    ));
    (v, out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn both_per_comdat_row_conventions_parse_and_prose_does_not() {
        assert_eq!(member_rvas(" *   0x067560  +0x2d\n"), vec![0x67560]);
        assert_eq!(
            member_rvas(" *   0x0669e0  0x3f   0 of 51\n"),
            vec![0x669e0]
        );
        assert!(
            member_rvas(" * the display-mode API (`EnumDisplaySettings` 0x65a90,\n").is_empty()
        );
        assert!(member_rvas(" * 0x1000 is the first rva\n").is_empty());
    }

    #[test]
    fn a_member_row_is_read_at_any_rva_a_32_bit_image_holds() {
        assert_eq!(member_rvas(" *   0x1000  +0x10\n"), vec![0x1000]);
        assert_eq!(member_rvas(" *   0x1067560  +0x2d\n"), vec![0x0106_7560]);
        assert_eq!(member_rvas(" *   0x7ffff000  0x20\n"), vec![0x7fff_f000]);
        assert!(member_rvas(" *   0x100000000  +0x2d\n").is_empty());
    }

    #[test]
    fn an_unknown_member_never_refutes_and_a_certain_against_an_absent_does() {
        assert!(!refuted(&[Flag::Certain, Flag::Unknown, Flag::Unknown]));
        assert!(!refuted(&[Flag::Unknown, Flag::Unknown]));
        assert!(!refuted(&[Flag::Certain, Flag::Certain]));
        assert!(!refuted(&[Flag::Absent, Flag::Absent]));
        assert!(refuted(&[Flag::Absent, Flag::Certain, Flag::Absent]));
        assert_eq!(
            resolve_rate(&[0x1000, 0x2000, 0x3000], &|r| r == 0x1000),
            (1, 3)
        );
    }

    #[test]
    fn a_row_with_no_flags_of_its_own_takes_the_targets_default() {
        let gx = vec!["/GX".to_string()];
        let plain: Vec<String> = vec!["/Od".to_string()];
        let none = Carve::default();
        assert_eq!(classify(Some(&none), &plain, "/GX"), Flag::Absent);
        assert_eq!(classify(Some(&none), &gx, "/GX"), Flag::Certain);
        let data = Carve {
            eh_data: true,
            cflags: Some(plain.clone()),
        };
        assert_eq!(classify(Some(&data), &plain, "/GX"), Flag::Certain);
        let own = Carve {
            eh_data: false,
            cflags: Some(plain.clone()),
        };
        assert_eq!(classify(Some(&own), &gx, "/GX"), Flag::Absent);
        assert_eq!(classify(None, &gx, "/GX"), Flag::Unknown);
        assert_eq!(effective(&none, &gx), gx.as_slice());
    }

    #[test]
    fn the_report_counts_each_verdict() {
        let carves: BTreeMap<u32, Carve> = [
            (
                0x1000,
                Carve {
                    eh_data: true,
                    cflags: None,
                },
            ),
            (0x2000, Carve::default()),
        ]
        .into();
        let cands = vec![
            Candidate {
                name: "a.cpp",
                rvas: vec![0x1000, 0x2000],
            },
            Candidate {
                name: "b.cpp",
                rvas: vec![0x1000, 0x9999],
            },
            Candidate {
                name: "c.cpp",
                rvas: vec![],
            },
        ];
        let (v, lines) = report("IMG", "/GX", &cands, &carves, &["/Od".to_string()]);
        assert_eq!(
            v,
            Verdicts {
                refuted: 1,
                agree: 1,
                withheld: 1
            }
        );
        assert!(
            lines.iter().any(|l| l.contains("REFUTED BY FLAGS  a.cpp")),
            "{lines:?}"
        );
        assert!(
            lines.iter().any(|l| l.contains("NO ROWS   c.cpp")),
            "{lines:?}"
        );
    }
}
