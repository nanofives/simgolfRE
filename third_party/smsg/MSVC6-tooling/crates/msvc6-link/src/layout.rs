use crate::growth::{round16, slot};
use crate::linkmap::Map;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const EXHAUSTIVE: usize = 8;

pub const SEARCH_BUDGET: usize = 1 << 16;

pub const RANKED_ROWS: usize = 8;

pub const MISMATCH_ROWS: usize = 6;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Obj {
    pub name: String,
    pub comdats: Vec<(String, Option<u32>, u64)>,
    pub statics: BTreeSet<String>,
}

impl Obj {
    fn emits<'a>(&self, name: &'a str, seen: &mut BTreeSet<&'a str>) -> bool {
        self.statics.contains(name) || seen.insert(name)
    }
}

pub type Placed = (String, u32, Option<u32>);

#[must_use]
pub fn place(objs: &[&Obj], base: u32) -> Vec<Placed> {
    let mut cur = u64::from(base);
    let mut out = Vec::new();
    let mut seen: BTreeSet<&str> = BTreeSet::new();
    for o in objs {
        cur = round16(cur);
        for (name, rva, size) in &o.comdats {
            if !o.emits(name.as_str(), &mut seen) {
                continue;
            }
            out.push((name.clone(), u32::try_from(cur).unwrap_or(u32::MAX), *rva));
            cur = cur.saturating_add(slot(*size));
        }
    }
    out
}

#[must_use]
pub fn score(placed: &[Placed]) -> (usize, u64) {
    let mut exact = 0usize;
    let mut disp = 0u64;
    for (_, got, want) in placed {
        if let Some(w) = want {
            if got == w {
                exact += 1;
            }
            disp = disp.saturating_add(u64::from(got.abs_diff(*w)));
        }
    }
    (exact, disp)
}

#[must_use]
pub fn from_map(map: &Map) -> (Vec<String>, BTreeMap<String, u32>) {
    let code = map.code_sections();
    let mut addr: BTreeMap<String, u32> = BTreeMap::new();
    let mut rows: Vec<(u32, &str)> = Vec::new();
    for r in map.rows.iter().filter(|r| code.contains(&r.sect)) {
        if !r.obj.ends_with(".obj") || r.obj.contains(':') {
            continue;
        }
        rows.push((r.va, r.obj.as_str()));
        addr.insert(r.name.clone(), r.va);
    }
    rows.sort_by_key(|(a, _)| *a);
    let mut order: Vec<String> = Vec::new();
    for (_, o) in rows {
        if !order.iter().any(|x| x == o) {
            order.push(o.to_string());
        }
    }
    (order, addr)
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Control {
    pub checked: usize,
    pub wrong: Vec<(String, u32, u32)>,
}

#[must_use]
pub fn control(predicted: &[Placed], mapaddr: &BTreeMap<String, u32>, base: u32) -> Control {
    let mut checked = 0usize;
    let mut wrong = Vec::new();
    for (n, got, _) in predicted {
        if let Some(&actual) = mapaddr.get(n) {
            checked += 1;
            let want = actual.saturating_sub(base);
            if *got != want {
                wrong.push((n.clone(), *got, want));
            }
        }
    }
    Control { checked, wrong }
}

#[must_use]
pub fn bound(objs: &[&Obj]) -> Vec<(String, usize, usize)> {
    let mut owned: BTreeSet<&str> = BTreeSet::new();
    let mut rows = Vec::new();
    for o in objs {
        let mut byref: Vec<(u32, u64)> = o
            .comdats
            .iter()
            .filter(|(n, _, _)| o.emits(n.as_str(), &mut owned))
            .filter_map(|(_, r, s)| r.map(|r| (r, *s)))
            .collect();
        byref.sort_by_key(|(r, _)| *r);
        let mut land = usize::from(!byref.is_empty());
        for w in byref.windows(2) {
            let (Some(&(r0, s0)), Some(&(r1, _))) = (w.first(), w.get(1)) else {
                break;
            };
            if u64::from(r1.saturating_sub(r0)) == slot(s0) {
                land += 1;
            } else {
                break;
            }
        }
        rows.push((o.name.clone(), land, byref.len()));
    }
    rows
}

fn permute(v: &mut Vec<usize>, k: usize, f: &mut impl FnMut(&[usize])) {
    if k + 1 >= v.len() {
        f(v);
        return;
    }
    for i in k..v.len() {
        v.swap(k, i);
        permute(v, k + 1, f);
        v.swap(k, i);
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Ranked {
    pub exhaustive: bool,
    pub evaluated: usize,
    pub exhausted: bool,
    pub rows: Vec<(usize, u64, Vec<usize>)>,
}

#[must_use]
pub fn search(objs: &[Obj], start: u32) -> Ranked {
    let n = objs.len();
    let grade = |p: &[usize]| {
        let sel: Vec<&Obj> = p.iter().filter_map(|i| objs.get(*i)).collect();
        score(&place(&sel, start))
    };
    let mut r = Ranked {
        exhaustive: n <= EXHAUSTIVE,
        ..Ranked::default()
    };
    if r.exhaustive {
        let mut idxs: Vec<usize> = (0..n).collect();
        permute(&mut idxs, 0, &mut |p| {
            let (e, d) = grade(p);
            r.rows.push((e, d, p.to_vec()));
        });
        r.evaluated = r.rows.len();
    } else {
        let mut cur: Vec<usize> = (0..n).collect();
        let (mut ce, mut cd) = grade(&cur);
        r.evaluated = 1;
        r.rows.push((ce, cd, cur.clone()));
        'steps: loop {
            let mut best: Option<(usize, u64, Vec<usize>)> = None;
            for i in 0..n {
                for j in 0..n {
                    if i == j {
                        continue;
                    }
                    if r.evaluated >= SEARCH_BUDGET {
                        r.exhausted = true;
                        break 'steps;
                    }
                    let mut p = cur.clone();
                    let o = p.remove(i);
                    p.insert(j, o);
                    let (e, d) = grade(&p);
                    r.evaluated += 1;
                    let better = |be: usize, bd: u64| e > be || (e == be && d < bd);
                    if better(ce, cd) && best.as_ref().is_none_or(|(be, bd, _)| better(*be, *bd)) {
                        best = Some((e, d, p));
                    }
                }
            }
            let Some((e, d, p)) = best else { break };
            (ce, cd) = (e, d);
            cur.clone_from(&p);
            r.rows.push((e, d, p));
        }
    }
    r.rows.sort_by(|a, b| b.0.cmp(&a.0).then(a.1.cmp(&b.1)));
    r
}

pub struct Report<'a> {
    pub label: &'a str,
    pub order: &'a [String],
    pub objs: &'a [Obj],
    pub mapaddr: &'a BTreeMap<String, u32>,
    pub image_base: u32,
    pub search: bool,
}

impl std::fmt::Debug for Report<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Report")
            .field("label", &self.label)
            .field("objs", &self.objs.len())
            .finish_non_exhaustive()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Verdict {
    NoStart,
    Refused,
    Modelled,
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn report(r: &Report<'_>) -> (Verdict, Vec<String>) {
    let refs: Vec<&Obj> = r.objs.iter().collect();
    let Some(first) = r.mapaddr.values().min().copied() else {
        return (
            Verdict::NoStart,
            vec![format!("{}: no own symbol address in the map", r.label)],
        );
    };
    let start = first.saturating_sub(r.image_base);
    let predicted = place(&refs, start);
    let c = control(&predicted, r.mapaddr, r.image_base);
    let mut out = vec![
        format!(
            "{}: PREDICTED own-code `.text` layout, from objects alone",
            r.label
        ),
        format!(
            "  model: COMDATs in object order, each given round16(size*5/4), each object's contribution 16-aligned, \
             the first own object at {start:#08x} (the map's lowest own address)"
        ),
        format!("  object order from the map: {}", r.order.join(" ")),
        String::new(),
        format!(
            "  POSITIVE CONTROL -- does the model reproduce THIS link? {} of {} own COMDAT(s) predicted EXACTLY.",
            c.checked - c.wrong.len(),
            c.checked
        ),
    ];
    for (n, got, want) in c.wrong.iter().take(MISMATCH_ROWS) {
        out.push(format!(
            "    MISMATCH  predicted {got:#08x}  map says {want:#08x}  {n}"
        ));
    }
    if !c.wrong.is_empty() {
        if c.wrong.len() > MISMATCH_ROWS {
            out.push(format!(
                "    ... and {} more",
                c.wrong.len() - MISMATCH_ROWS
            ));
        }
        out.push(
            "  SEARCH REFUSED: a model that cannot reproduce the link in front of it would rank fictions.".to_string(),
        );
        return (Verdict::Refused, out);
    }
    let rows = bound(&refs);
    let reach: usize = rows.iter().map(|(_, l, _)| *l).sum();
    let total: usize = rows.iter().map(|(_, _, t)| *t).sum();
    out.push(String::new());
    out.push(format!(
        "  UPPER BOUND on own COMDATs that can EVER sit at their reference address, the slot rule held fixed and \
         every object given its reference start: {reach} of {total}, over the fold-deduped population (a body several \
         objects define belongs to the first in link order):"
    ));
    for (n, land, tot) in &rows {
        out.push(format!("    {land:3} of {tot:3}   {n}"));
    }
    let (exact, disp) = score(&predicted);
    let named = predicted.iter().filter(|(_, _, w)| w.is_some()).count();
    out.push(format!(
        "  BASELINE against the reference: {exact} of {named} own COMDAT(s) at their reference address, total absolute \
         displacement {disp} byte(s)."
    ));
    if !r.search {
        out.push(
            "  pass --search to rank object orders. Nothing is linked either way.".to_string(),
        );
        return (Verdict::Modelled, out);
    }
    let ranked = search(r.objs, start);
    out.push(String::new());
    if !ranked.exhaustive {
        out.push(format!(
            "  {} objects is {}! arrangements -- LOCAL SEARCH by single-object moves from the map's order{}.",
            r.objs.len(),
            r.objs.len(),
            if ranked.exhausted {
                format!(", stopped at its {SEARCH_BUDGET} evaluations")
            } else {
                String::new()
            }
        ));
    }
    out.push(format!(
        "  {} object order(s) evaluated with NO LINK. Best by exact hits, then by displacement:",
        ranked.evaluated
    ));
    for (e, d, p) in ranked.rows.iter().take(RANKED_ROWS) {
        let names: Vec<&str> = p
            .iter()
            .filter_map(|i| r.objs.get(*i))
            .map(|o| o.name.trim_end_matches(".obj"))
            .collect();
        out.push(format!(
            "    exact {e:3}  displacement {d:9}   {}",
            names.join(" ")
        ));
    }
    out.push(
        "  A top row is a candidate for one link, not a result: the model places own objects only."
            .to_string(),
    );
    (Verdict::Modelled, out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn obj(name: &str, comdats: &[(&str, Option<u32>, u64)]) -> Obj {
        Obj {
            name: name.to_string(),
            comdats: comdats
                .iter()
                .map(|(n, r, s)| ((*n).to_string(), *r, *s))
                .collect(),
            statics: BTreeSet::new(),
        }
    }

    #[test]
    fn slots_sum_to_the_next_objects_start() {
        let sizes = [480u64, 486, 40, 91, 116, 258, 107, 70, 47, 60, 50, 56, 42];
        let total: u64 = sizes.iter().map(|s| slot(*s)).sum();
        assert_eq!(total, 2496);
        let a = obj(
            "A.obj",
            &sizes.iter().map(|s| ("f", None, *s)).collect::<Vec<_>>(),
        );
        let mut a = a;
        for (k, c) in a.comdats.iter_mut().enumerate() {
            c.0 = format!("f{k}");
        }
        let b = obj("B.obj", &[("g", Some(0x16a0 + 2496), 10)]);
        let p = place(&[&a, &b], 0x16a0);
        assert_eq!(p.last().map(|x| x.1), Some(0x2060));
        assert_eq!(score(&p), (1, 0));
    }

    #[test]
    fn the_model_does_not_agree_with_an_arbitrary_layout() {
        let o = obj(
            "X.obj",
            &[("a", Some(0x1000), 480), ("b", Some(0x2000), 40)],
        );
        let placed = place(&[&o], 0x1000);
        assert_eq!(placed[0].1, 0x1000);
        assert_eq!(placed[1].1, 0x1000 + 608);
        let (exact, disp) = score(&placed);
        assert_eq!(exact, 1);
        assert!(disp > 0);
    }

    #[test]
    fn a_shared_body_is_placed_once_by_its_first_definer_and_a_static_every_time() {
        let mut a = obj("A.obj", &[("inl", None, 16), ("_$E1", None, 16)]);
        a.statics.insert("_$E1".to_string());
        let mut b = obj(
            "B.obj",
            &[("inl", None, 16), ("_$E1", None, 16), ("g", None, 4)],
        );
        b.statics.insert("_$E1".to_string());
        let p = place(&[&a, &b], 0);
        let names: Vec<&str> = p.iter().map(|x| x.0.as_str()).collect();
        assert_eq!(names, vec!["inl", "_$E1", "_$E1", "g"]);
        assert_eq!(
            bound(&[&a, &b]),
            vec![("A.obj".into(), 0, 0), ("B.obj".into(), 0, 0)]
        );
    }

    #[test]
    fn a_small_search_is_exhaustive_and_a_large_one_local_and_bounded() {
        let objs: Vec<Obj> = (0..4u32)
            .map(|k| obj(&format!("o{k}.obj"), &[("f", Some(0x40 * (3 - k)), 48)]))
            .map(|mut o| {
                o.comdats[0].0 = o.name.clone();
                o
            })
            .collect();
        let r = search(&objs, 0);
        assert!(r.exhaustive);
        assert_eq!(r.evaluated, 24);
        assert_eq!(r.rows[0].0, 4);
        assert_eq!(r.rows[0].2, vec![3, 2, 1, 0]);
        let many: Vec<Obj> = (0..12u32)
            .map(|k| {
                let mut o = obj(&format!("o{k}.obj"), &[("f", Some(0x40 * (11 - k)), 48)]);
                o.comdats[0].0 = o.name.clone();
                o
            })
            .collect();
        let r = search(&many, 0);
        assert!(!r.exhaustive);
        assert!(r.evaluated <= SEARCH_BUDGET);
        assert!(r.rows[0].0 >= 2);
    }
}
