use anyhow::Result;
use msvc6_image::archive::Archive;
use msvc6_image::coff::{Object, SELECT_ASSOCIATIVE};
use serde::Serialize;
use std::collections::BTreeMap;

pub const FUNCLETS: &str = ".text$x";

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Funclet {
    pub object: String,
    pub owner: String,
    pub len: usize,
}

#[must_use]
pub fn owners(o: &Object<'_>) -> Vec<(String, usize)> {
    let mut sizes: BTreeMap<u16, usize> = BTreeMap::new();
    for (k, s) in o.sections.iter().enumerate() {
        if s.name == FUNCLETS
            && let Ok(n) = u16::try_from(k + 1)
        {
            sizes.insert(n, usize::try_from(s.size).unwrap_or(usize::MAX));
        }
    }
    let mut named: BTreeMap<u16, Vec<(u32, &str)>> = BTreeMap::new();
    let mut defs: Vec<(u32, u16, u8, u16)> = Vec::new();
    for s in &o.symbols {
        let Some(sec) = s.section_number() else {
            continue;
        };
        match s.section_definition() {
            Some(d) => defs.push((s.index, sec, d.selection, d.number)),
            None => named.entry(sec).or_default().push((s.index, &s.name)),
        }
    }
    let mut leaders: Vec<(u16, String)> = Vec::new();
    let mut parent: BTreeMap<u16, u16> = BTreeMap::new();
    let mut seen = std::collections::BTreeSet::new();
    for (index, sec, selection, number) in defs {
        if selection == 0 || !seen.insert(sec) {
            continue;
        }
        if selection == SELECT_ASSOCIATIVE {
            parent.insert(sec, number);
            continue;
        }
        let name = named
            .get(&sec)
            .and_then(|v| v.iter().find(|(i, _)| *i > index))
            .map_or_else(String::new, |(_, n)| (*n).to_string());
        leaders.push((sec, name));
    }
    leaders.sort_by_key(|(s, _)| *s);
    let mut children: BTreeMap<u16, Vec<u16>> = BTreeMap::new();
    for (c, p) in &parent {
        children.entry(*p).or_default().push(*c);
    }
    let mut out = Vec::new();
    for (sec, name) in &leaders {
        let members = children.get(sec).map_or(&[][..], Vec::as_slice);
        for m in std::iter::once(*sec).chain(members.iter().copied()) {
            if let Some(len) = sizes.remove(&m) {
                out.push((name.clone(), len));
            }
        }
    }
    for (_, len) in sizes {
        out.push((String::new(), len));
    }
    out
}

pub fn object(stem: &str, data: &[u8]) -> Result<Vec<Funclet>> {
    let o = Object::parse(data)?;
    Ok(owners(&o)
        .into_iter()
        .map(|(owner, len)| Funclet {
            object: stem.to_string(),
            owner,
            len,
        })
        .collect())
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Ceiling {
    pub bytes: u32,
    pub evidence: Option<String>,
}

#[must_use]
pub fn report(
    label: &str,
    dir: &str,
    objects: usize,
    per_object: &[(String, usize)],
    funclets: &[Funclet],
    ceiling: Option<&Ceiling>,
) -> Vec<String> {
    let mut out = vec![
        format!("{label}: `.text$x' UNWIND FUNCLETS IN OUR OBJECTS, sized and named"),
        format!(
            "  POPULATION: {objects} object file(s) in {dir} -- OURS ONLY. A row is a candidate, never a defect: this screen"
        ),
        "  cannot see which funclets the original had.".to_string(),
        String::new(),
    ];
    for (stem, total) in per_object.iter().filter(|(_, t)| *t > 0) {
        out.push(format!("  {stem:<14} {total:6} byte(s) of funclet"));
    }
    let mut rows = funclets.to_vec();
    rows.sort_by_key(|f| std::cmp::Reverse(f.len));
    out.push(String::new());
    out.push("  EVERY FUNCLET, LARGEST FIRST:".to_string());
    for f in &rows {
        out.push(format!(
            "    {:5} B  {:<14} {}",
            f.len,
            f.object,
            if f.owner.is_empty() {
                "<no readable COMDAT group>"
            } else {
                &f.owner
            }
        ));
    }
    let total: usize = funclets.iter().map(|f| f.len).sum();
    out.push(String::new());
    out.push(format!(
        "  OURS {total} byte(s) in {} funclet(s).",
        rows.len()
    ));
    let Some(c) = ceiling else {
        out.push(
            "  NO REFERENCE CEILING: [image.eh] records no `textx_ceiling', so no surplus is computed.".to_string(),
        );
        return out;
    };
    let cap = usize::try_from(c.bytes).unwrap_or(usize::MAX);
    out.push(format!(
        "  REFERENCE at most {} byte(s){}.",
        c.bytes,
        c.evidence
            .as_ref()
            .map_or_else(String::new, |e| format!(" ({e})"))
    ));
    out.push(format!(
        "  SURPLUS at least {} byte(s): the edit is never to a funclet but to what makes CL emit one -- a destructor given a body the original left trivial, or an object at function scope the original held in a narrower block.",
        total.saturating_sub(cap)
    ));
    if total <= cap {
        out.push(
            "  NO SURPLUS AT THIS CEILING, which refutes the ceiling's derivation rather than confirming the sources."
                .to_string(),
        );
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Carried {
    pub member: String,
    pub bytes: usize,
    pub pieces: usize,
}

pub fn archive(data: &[u8], want: &[String]) -> Result<(usize, Vec<Carried>)> {
    let a = Archive::parse(data)?;
    let mut walked = 0usize;
    let mut out = Vec::new();
    for (m, body) in a.content() {
        walked += 1;
        let base = m.stem().to_string();
        if !want.is_empty()
            && !want
                .iter()
                .any(|w| base == *w || base == format!("{w}.obj"))
        {
            continue;
        }
        let (bytes, pieces) = Object::parse(body).map_or((0, 0), |o| {
            o.sections
                .iter()
                .filter(|s| s.name == FUNCLETS)
                .fold((0usize, 0usize), |(b, n), s| {
                    (
                        b.saturating_add(usize::try_from(s.size).unwrap_or(0)),
                        n + 1,
                    )
                })
        });
        out.push(Carried {
            member: base,
            bytes,
            pieces,
        });
    }
    Ok((walked, out))
}

#[must_use]
pub fn archive_report(path: &str, walked: usize, want: &[String], rows: &[Carried]) -> Vec<String> {
    let mut out = vec![
        format!("  {path}: {walked} member(s) walked"),
        "  POPULATION: the members NAMED, matched on the basename of the archive path; a member not listed carried no `.text$x' or was not found."
            .to_string(),
    ];
    for r in rows {
        out.push(format!(
            "    {:5} B  {} piece(s)  {}",
            r.bytes, r.pieces, r.member
        ));
    }
    let total: usize = rows.iter().map(|r| r.bytes).sum();
    if want.is_empty() {
        out.push(format!(
            "    ---- {total} byte(s) of `.text$x' over ALL {} member(s) of this archive",
            rows.len()
        ));
    } else {
        out.push(format!(
            "    ---- {total} byte(s) of `.text$x' over {} of the {} member(s) named",
            rows.len(),
            want.len()
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_fixture::coff::{Object as Fixture, Section, Symbol};

    fn with_funclets() -> Vec<u8> {
        let mut o = Fixture::new();
        let f = o.section(Section {
            name: ".text".into(),
            characteristics: 0x6030_1020,
            data: vec![0xc3; 16],
            ..Section::default()
        });
        let x = o.section(Section {
            name: FUNCLETS.into(),
            characteristics: 0x6030_1020,
            data: vec![0xcc; 21],
            ..Section::default()
        });
        let y = o.section(Section {
            name: FUNCLETS.into(),
            characteristics: 0x6030_1020,
            data: vec![0xcc; 5],
            ..Section::default()
        });
        let def = |n: i16, len: u32, number: u16, sel: u8| Symbol {
            name: if n == f {
                ".text".into()
            } else {
                FUNCLETS.into()
            },
            value: 0,
            section: n,
            typ: 0,
            class: 3,
            aux: vec![msvc6_fixture::coff::section_aux(len, 0, 0, 0, number, sel)],
        };
        o.symbols.push(def(f, 16, 0, 1));
        o.symbols.push(Symbol {
            name: "?f@@YAXXZ".into(),
            value: 0,
            section: f,
            typ: 0x20,
            class: 2,
            aux: Vec::new(),
        });
        o.symbols
            .push(def(x, 21, u16::try_from(f).unwrap(), SELECT_ASSOCIATIVE));
        o.symbols.push(def(y, 5, 0, 0));
        o.build()
    }

    #[test]
    fn a_funclet_is_named_by_the_comdat_its_group_belongs_to() {
        let got = object("a", &with_funclets()).unwrap();
        let rows: Vec<(&str, usize)> = got.iter().map(|f| (f.owner.as_str(), f.len)).collect();
        assert_eq!(rows, vec![("?f@@YAXXZ", 21), ("", 5)]);
        let lines = report("IMG", "objs", 1, &[("a".into(), 26)], &got, None);
        assert!(
            lines
                .iter()
                .any(|l| l == "       21 B  a              ?f@@YAXXZ"),
            "{lines:#?}"
        );
        assert!(
            lines
                .iter()
                .any(|l| l == "        5 B  a              <no readable COMDAT group>")
        );
        assert!(
            lines
                .iter()
                .any(|l| l.starts_with("  NO REFERENCE CEILING"))
        );
        let c = Ceiling {
            bytes: 20,
            evidence: Some("measured".into()),
        };
        let lines = report("IMG", "objs", 1, &[], &got, Some(&c));
        assert!(
            lines
                .iter()
                .any(|l| l.starts_with("  SURPLUS at least 6 byte(s)")),
            "{lines:#?}"
        );
    }
}
