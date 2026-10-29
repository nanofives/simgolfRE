use crate::growth::slot;
use serde::Serialize;
use std::collections::BTreeSet;

pub const EXCESS_ROWS: usize = 24;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub enum Fit {
    Exact,
    Spanning(i64, String),
    Over(i64),
    Under(i64),
}

impl Fit {
    #[must_use]
    pub fn label(&self) -> String {
        match self {
            Self::Exact => "EXACT".to_string(),
            Self::Spanning(d, u) => format!("SPANS +{d} (= slot of unplaced {u})"),
            Self::Over(d) => format!("OVER +{d}"),
            Self::Under(d) => format!("UNDER -{d}"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Member {
    pub name: String,
    pub size: u64,
    pub reference: Option<u32>,
    pub ours: Option<u32>,
}

fn signed(n: u64) -> i64 {
    i64::try_from(n).unwrap_or(i64::MAX)
}

#[must_use]
pub fn fits(members: &[Member]) -> Vec<(Fit, u64, u64, String)> {
    let mut placed: Vec<(u32, u64, &str)> = members
        .iter()
        .filter(|m| m.reference.is_some())
        .filter_map(|m| m.ours.map(|a| (a, m.size, m.name.as_str())))
        .collect();
    placed.sort_by_key(|(a, _, _)| *a);
    let unplaced: Vec<(u64, &str)> = members
        .iter()
        .filter(|m| m.reference.is_some() && m.ours.is_none())
        .map(|m| (m.size, m.name.as_str()))
        .collect();
    let mut out = Vec::new();
    for w in placed.windows(2) {
        let (Some(&(a0, s0, n0)), Some(&(a1, _, _))) = (w.first(), w.get(1)) else {
            continue;
        };
        let gap = u64::from(a1.saturating_sub(a0));
        let p = slot(s0);
        let f = match gap.cmp(&p) {
            std::cmp::Ordering::Equal => Fit::Exact,
            std::cmp::Ordering::Greater => {
                let excess = signed(gap) - signed(p);
                match unplaced.iter().find(|(us, _)| signed(slot(*us)) == excess) {
                    Some((_, un)) => Fit::Spanning(excess, (*un).to_string()),
                    None => Fit::Over(excess),
                }
            }
            std::cmp::Ordering::Less => Fit::Under(signed(p) - signed(gap)),
        };
        out.push((f, s0, gap, n0.to_string()));
    }
    out
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Excess {
    pub plus: i64,
    pub minus: i64,
    pub differing: usize,
    pub pairs: usize,
    pub skipped: usize,
    pub rows: Vec<(u32, i64, u64, u64, String)>,
}

#[must_use]
pub fn excess(members: &[Member], known: &BTreeSet<u32>) -> Excess {
    let mut byref: Vec<(u32, u64, &str)> = members
        .iter()
        .filter_map(|m| m.reference.map(|r| (r, m.size, m.name.as_str())))
        .collect();
    byref.sort_by_key(|(r, _, _)| *r);
    let mut e = Excess {
        pairs: byref.len().saturating_sub(1),
        ..Excess::default()
    };
    for w in byref.windows(2) {
        let (Some(&(r0, s0, n0)), Some(&(r1, _, _))) = (w.first(), w.get(1)) else {
            continue;
        };
        if known.range(r0.saturating_add(1)..r1).next().is_some() {
            e.skipped += 1;
            continue;
        }
        let refgap = u64::from(r1.saturating_sub(r0));
        let ourslot = slot(s0);
        let d = signed(ourslot) - signed(refgap);
        if d != 0 {
            e.differing += 1;
            e.rows.push((r0, d, refgap, ourslot, n0.to_string()));
        }
        if d > 0 {
            e.plus += d;
        } else {
            e.minus += d;
        }
    }
    e.rows.sort_by_key(|r| r.0);
    e
}

#[must_use]
pub fn report(label: &str, members: &[Member], known: &BTreeSet<u32>) -> Vec<String> {
    let f = fits(members);
    let unplaced = members
        .iter()
        .filter(|m| m.reference.is_some() && m.ours.is_none())
        .count();
    let mut out = vec![
        format!("{label}: COMDAT SLOTS against `round16(size x 5/4)', size read from the OBJECT"),
        "  the size is the object's `.text' COMDAT SECTION SIZE -- an exact field, not a decoded extent or a fill scan"
            .to_string(),
        String::new(),
        "  size   slot  predicted  verdict   function".to_string(),
    ];
    for (fit, s, gap, n) in &f {
        out.push(format!(
            "  {s:5}  {gap:5}  {:9}  {:9} {n}",
            slot(*s),
            fit.label()
        ));
    }
    let count = |p: &dyn Fn(&Fit) -> bool| f.iter().filter(|(x, _, _, _)| p(x)).count();
    let exact = count(&|x| *x == Fit::Exact);
    let spans = count(&|x| matches!(x, Fit::Spanning(_, _)));
    let over = count(&|x| matches!(x, Fit::Over(_)));
    let under = count(&|x| matches!(x, Fit::Under(_)));
    out.push(String::new());
    if f.is_empty() {
        out.push(format!(
            "  NO PAIR MEASURED. {} COMDAT(s) in the object, {unplaced} of them not uniquely locatable in our image.",
            members.len()
        ));
        return out;
    }
    out.push(format!(
        "  {exact} EXACT, {spans} SPANNING, {over} OVER, {under} UNDER of {} consecutive pair(s). The population is \
         this object's own COMDATs: {} in the object, {unplaced} not uniquely locatable under the relocation mask and \
         excluded. An UNDER refutes the rule; an OVER can be content not located.",
        f.len(),
        members.len()
    ));
    let e = excess(members, known);
    out.push(String::new());
    out.push(format!(
        "  WHAT OUR RESERVE COSTS AGAINST THE REFERENCE'S OWN GAPS -- {} of {} consecutive REFERENCE-ORDER pair(s) \
         differ: {:+} byte(s) of PADDING WE ADD, {:+} byte(s) where the reference gap is WIDER than the most ILINK \
         grants (content not recognised, never summed with the first). {} pair(s) SKIPPED as not adjacent in the \
         reference.",
        e.differing, e.pairs, e.plus, e.minus, e.skipped
    ));
    for (r, d, refgap, ourslot, n) in e.rows.iter().take(EXCESS_ROWS) {
        out.push(format!(
            "    {r:#08x}  {d:+6}   reference gap {refgap:5}  our slot {ourslot:5}  {n}"
        ));
    }
    if e.rows.len() > EXCESS_ROWS {
        out.push(format!(
            "    ... and {} more, in reference order",
            e.rows.len() - EXCESS_ROWS
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn m(name: &str, size: u64, reference: Option<u32>, ours: Option<u32>) -> Member {
        Member {
            name: name.to_string(),
            size,
            reference,
            ours,
        }
    }

    #[test]
    fn measured_slots_are_reproduced_and_a_body_extent_reads_sixteen_short() {
        for (size, s) in [(480, 608), (486, 608), (40, 64), (91, 128), (116, 160)] {
            assert_eq!(slot(size), s);
        }
        assert_eq!(slot(480) - slot(472), 16);
    }

    #[test]
    fn a_gap_over_the_rule_by_an_unplaced_slot_spans_it() {
        let ms = vec![
            m("a", 480, Some(0x1000), Some(0x5000)),
            m("b", 40, Some(0x1260), None),
            m("c", 40, Some(0x12a0), Some(0x5000 + 608 + 64)),
            m("d", 40, Some(0x1300), Some(0x5000 + 608 + 64 + 48)),
        ];
        let f = fits(&ms);
        assert_eq!(f[0].0, Fit::Spanning(64, "b".to_string()));
        assert_eq!(f[1].0, Fit::Under(16));
        let known: BTreeSet<u32> = [0x1270].into_iter().collect();
        let e = excess(&ms, &known);
        assert_eq!((e.pairs, e.skipped), (3, 1));
        assert_eq!(
            e.rows.iter().map(|r| (r.0, r.1)).collect::<Vec<_>>(),
            vec![(0x12a0, -32)]
        );
        assert_eq!((e.plus, e.minus), (0, -32));
    }
}
