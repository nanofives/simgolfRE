use crate::comdat;
use crate::env::Env;
use crate::par;
use crate::score::{self, Fits, Plan, Seat};
use crate::seat;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub enum Verdict {
    Exact,
    Differs,
    Superseded,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Row {
    pub tu: String,
    pub name: String,
    pub rva: u32,
    pub seat: Seat,
    pub differing: usize,
    pub comparable: usize,
    pub kept_by: Option<String>,
}

impl Row {
    #[must_use]
    pub fn verdict(&self) -> Verdict {
        match self.seat {
            Seat::Superseded | Seat::Duplicate | Seat::Dropped => Verdict::Superseded,
            Seat::Unseated => Verdict::Differs,
            _ if self.differing == 0 => Verdict::Exact,
            _ => Verdict::Differs,
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct Table {
    pub rows: Vec<Row>,
    pub warnings: Vec<String>,
}

pub type Copies = BTreeMap<String, Vec<u8>>;

#[must_use]
pub fn select_any_copies(d: &[u8]) -> Copies {
    let any = score::select_any_sections(d);
    comdat::text_of(d)
        .unwrap_or_default()
        .into_iter()
        .filter(|c| any.contains(&c.index))
        .filter_map(|c| Some((c.symbol?, c.bytes)))
        .collect()
}

#[must_use]
pub fn kept_copy(earlier: &[(String, Copies)], sym: &str, fits: &Fits<'_>) -> Option<String> {
    earlier
        .iter()
        .find_map(|(name, copies)| copies.get(sym).map(|b| (name, b)))
        .and_then(|(name, b)| fits(b).then(|| name.clone()))
}

fn object_name(o: &Path) -> String {
    o.file_name()
        .and_then(|f| f.to_str())
        .unwrap_or_default()
        .to_string()
}

fn object_rows(env: &Env, o: &Path, earlier: &[(String, Copies)]) -> Table {
    let tu = seat::scope_stem(o);
    let mut out = Table::default();
    let d = match comdat::read(o) {
        Ok(d) => d,
        Err(e) => {
            out.warnings.push(format!("{tu}: {e:#}"));
            return out;
        }
    };
    let comdats = match comdat::text_of(&d) {
        Ok(c) => c,
        Err(e) => {
            out.warnings.push(format!("{tu}: {e:#}"));
            return out;
        }
    };
    let ms = match seat::members_of(env, o, &d, &comdats) {
        Ok(m) => m,
        Err(e) => {
            out.warnings.push(format!("{tu}: {e:#}"));
            return out;
        }
    };
    let mut kept = Vec::new();
    let mut at = Vec::new();
    for (m, c) in ms.iter().zip(comdats) {
        match m.rva {
            Some(r) => {
                kept.push(c);
                at.push(r);
            }
            None => out.warnings.push(format!(
                "{tu}: {} resolves to no address -- left out",
                m.name
            )),
        }
    }
    let Some((&root, members)) = at.split_first() else {
        return out;
    };
    let plan = Plan::Asserted(members.to_vec());
    let resolver = |sym: &str, fits: &Fits<'_>| kept_copy(earlier, sym, fits);
    match score::seat_and_score_with(env, &d, &kept, root, &plan, &resolver) {
        Ok(s) => {
            for m in s.members.into_iter().filter(|m| m.at >= env.text_rva) {
                out.rows.push(Row {
                    tu: tu.clone(),
                    name: m.symbol.clone().unwrap_or_else(|| m.name.clone()),
                    rva: m.at,
                    seat: m.seat,
                    differing: m.bad.len(),
                    comparable: m.comparable,
                    kept_by: m.kept_by,
                });
            }
        }
        Err(e) => out.warnings.push(format!("{tu}: {e:#}")),
    }
    out
}

#[must_use]
pub fn score_objects(env: &Env, objs: &[PathBuf], jobs: usize) -> Table {
    let list: Vec<PathBuf> = objs
        .iter()
        .filter(|p| p.extension().is_some_and(|x| x.eq_ignore_ascii_case("obj")))
        .cloned()
        .collect();
    let copies: Vec<(String, Copies)> = par::map(&list, jobs, |o| {
        (
            object_name(o),
            comdat::read(o)
                .map(|d| select_any_copies(&d))
                .unwrap_or_default(),
        )
    });
    let at: Vec<usize> = (0..list.len()).collect();
    let tables = par::map(&at, jobs, |&k| match list.get(k) {
        Some(o) => object_rows(env, o, copies.get(..k).unwrap_or_default()),
        None => Table::default(),
    });
    let mut order: Vec<(&PathBuf, Table)> = list.iter().zip(tables).collect();
    order.sort_by(|a, b| a.0.file_name().cmp(&b.0.file_name()));
    let mut out = Table::default();
    for (_, t) in order {
        out.rows.extend(t.rows);
        out.warnings.extend(t.warnings);
    }
    out
}

#[must_use]
pub fn differing(t: &Table) -> Vec<u32> {
    let mut v: Vec<u32> = t
        .rows
        .iter()
        .filter(|r| r.verdict() == Verdict::Differs && r.seat != Seat::Unseated)
        .map(|r| r.rva)
        .collect();
    v.sort_unstable();
    v.dedup();
    v
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(rva: u32, seat: Seat, differing: usize) -> Row {
        Row {
            tu: "T".into(),
            name: format!("f{rva:x}"),
            rva,
            seat,
            differing,
            comparable: 10,
            kept_by: None,
        }
    }

    #[test]
    fn the_differing_set_holds_seated_members_that_differ_once_each() {
        let t = Table {
            rows: vec![
                row(0x30, Seat::Image, 2),
                row(0x10, Seat::Given, 1),
                row(0x30, Seat::Misasserted, 4),
                row(0x40, Seat::Unseated, 9),
                row(0x50, Seat::Superseded, 0),
                row(0x58, Seat::Duplicate, 0),
                row(0x60, Seat::Image, 0),
            ],
            warnings: Vec::new(),
        };
        assert_eq!(differing(&t), [0x10, 0x30]);
        assert_eq!(row(0x40, Seat::Unseated, 0).verdict(), Verdict::Differs);
        assert_eq!(
            row(0x50, Seat::Superseded, 3).verdict(),
            Verdict::Superseded
        );
        assert_eq!(row(0x58, Seat::Duplicate, 3).verdict(), Verdict::Superseded);
        assert_eq!(row(0x60, Seat::Image, 0).verdict(), Verdict::Exact);
    }

    #[test]
    fn the_first_earlier_definer_decides_the_kept_copy() {
        let a: Copies = [("?f@@YAXXZ".to_string(), vec![1, 2, 3])]
            .into_iter()
            .collect();
        let b: Copies = [("?f@@YAXXZ".to_string(), vec![1, 2])]
            .into_iter()
            .collect();
        let earlier = [("a.obj".to_string(), a), ("b.obj".to_string(), b)];
        let three = |x: &[u8]| x.len() == 3;
        let two = |x: &[u8]| x.len() == 2;
        assert_eq!(
            kept_copy(&earlier, "?f@@YAXXZ", &three).as_deref(),
            Some("a.obj")
        );
        assert_eq!(kept_copy(&earlier, "?f@@YAXXZ", &two), None);
        assert_eq!(kept_copy(&earlier, "?g@@YAXXZ", &three), None);
        assert_eq!(kept_copy(&[], "?f@@YAXXZ", &three), None);
    }
}
