use anyhow::{Result, bail};
use serde::Serialize;
use std::cell::Cell;
use std::cmp::Ordering;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Crt {
    Vc6,
    Wine,
}

impl Crt {
    pub fn parse(s: &str) -> Result<Self> {
        match s.to_ascii_lowercase().as_str() {
            "vc6" | "native" | "msvcrt" => Ok(Self::Vc6),
            "wine" | "builtin" => Ok(Self::Wine),
            _ => bail!(
                "`{s}`: the CRT whose qsort LINK ran is vc6 (native msvcrt) or wine (builtin)"
            ),
        }
    }

    #[must_use]
    pub fn name(self) -> &'static str {
        match self {
            Self::Vc6 => "vc6",
            Self::Wine => "wine",
        }
    }

    #[must_use]
    pub fn label(self) -> &'static str {
        match self {
            Self::Vc6 => "Vc6",
            Self::Wine => "Wine",
        }
    }
}

pub const CUTOFF: usize = 8;

pub const BUDGET_FLOOR: u64 = 1 << 16;

#[must_use]
pub fn budget(n: usize) -> u64 {
    let n = u64::try_from(n).unwrap_or(u64::MAX);
    let lg = u64::from(64 - n.leading_zeros());
    n.saturating_mul(lg.saturating_add(1))
        .saturating_mul(32)
        .saturating_add(BUDGET_FLOOR)
}

fn cmp_at<T>(a: &[T], i: usize, j: usize, comp: &dyn Fn(&T, &T) -> Ordering) -> Ordering {
    match (a.get(i), a.get(j)) {
        (Some(x), Some(y)) => comp(x, y),
        _ => Ordering::Equal,
    }
}

fn vc6_shortsort<T>(a: &mut [T], lo: usize, mut hi: usize, comp: &dyn Fn(&T, &T) -> Ordering) {
    while hi > lo {
        let mut max = lo;
        for p in lo + 1..=hi {
            if cmp_at(a, p, max, comp) == Ordering::Greater {
                max = p;
            }
        }
        a.swap(max, hi);
        hi -= 1;
    }
}

pub fn vc6_qsort<T>(a: &mut [T], comp: &dyn Fn(&T, &T) -> Ordering) {
    let n = a.len();
    if n < 2 {
        return;
    }
    let mut stack: Vec<(usize, usize)> = Vec::new();
    let (mut lo, mut hi) = (0usize, n - 1);
    loop {
        let size = hi - lo + 1;
        let mut recurse = false;
        if size <= CUTOFF {
            vc6_shortsort(a, lo, hi, comp);
        } else {
            let mid = lo + size / 2;
            a.swap(mid, lo);
            let mut loguy = lo;
            let mut higuy = hi + 1;
            loop {
                loop {
                    loguy += 1;
                    if !(loguy <= hi && cmp_at(a, loguy, lo, comp) != Ordering::Greater) {
                        break;
                    }
                }
                loop {
                    higuy -= 1;
                    if !(higuy > lo && cmp_at(a, higuy, lo, comp) != Ordering::Less) {
                        break;
                    }
                }
                if higuy < loguy {
                    break;
                }
                a.swap(loguy, higuy);
            }
            a.swap(lo, higuy);
            if higuy + loguy > hi + lo {
                if lo + 1 < higuy {
                    stack.push((lo, higuy - 1));
                }
                if loguy < hi {
                    lo = loguy;
                    recurse = true;
                }
            } else {
                if loguy < hi {
                    stack.push((loguy, hi));
                }
                if lo + 1 < higuy {
                    hi = higuy - 1;
                    recurse = true;
                }
            }
        }
        if recurse {
            continue;
        }
        match stack.pop() {
            Some((l, h)) => {
                lo = l;
                hi = h;
            }
            None => return,
        }
    }
}

fn wine_small_sort<T>(a: &mut [T], beg: usize, nm: usize, comp: &dyn Fn(&T, &T) -> Ordering) {
    let mut e = nm;
    while e > 1 {
        let mut max = beg;
        let p = beg + e - 1;
        for i in 1..e {
            if cmp_at(a, beg + i, max, comp) == Ordering::Greater {
                max = beg + i;
            }
        }
        if p != max {
            a.swap(p, max);
        }
        e -= 1;
    }
}

pub fn wine_qsort<T>(a: &mut [T], comp: &dyn Fn(&T, &T) -> Ordering) {
    let n = a.len();
    if n < 2 {
        return;
    }
    let mut st: Vec<(usize, usize)> = vec![(0, n - 1)];
    while let Some((beg, end)) = st.pop() {
        if end < beg + CUTOFF {
            wine_small_sort(a, beg, (end + 1).saturating_sub(beg), comp);
            continue;
        }
        let (mut lo, mut hi) = (beg, end);
        let mut med = lo + (hi - lo).div_ceil(2);
        if cmp_at(a, lo, med, comp) == Ordering::Greater {
            a.swap(lo, med);
        }
        if cmp_at(a, lo, hi, comp) == Ordering::Greater {
            a.swap(lo, hi);
        }
        if cmp_at(a, med, hi, comp) == Ordering::Greater {
            a.swap(med, hi);
        }
        lo += 1;
        hi -= 1;
        loop {
            while lo <= hi {
                if lo != med && cmp_at(a, lo, med, comp) == Ordering::Greater {
                    break;
                }
                lo += 1;
            }
            while med != hi {
                if cmp_at(a, hi, med, comp) != Ordering::Greater {
                    break;
                }
                hi -= 1;
            }
            if hi < lo {
                break;
            }
            a.swap(lo, hi);
            if hi == med {
                med = lo;
            }
            lo += 1;
            hi -= 1;
        }
        while hi > beg {
            if hi != med && cmp_at(a, hi, med, comp) != Ordering::Equal {
                break;
            }
            hi -= 1;
        }
        if hi - beg >= end.saturating_sub(lo) {
            st.push((beg, hi));
            st.push((lo, end));
        } else {
            st.push((lo, end));
            st.push((beg, hi));
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
pub struct Work {
    pub compares: u64,
    pub budget: u64,
}

pub fn sort<T>(crt: Crt, a: &mut [T], comp: &dyn Fn(&T, &T) -> Ordering) -> Result<Work> {
    sort_within(crt, a, comp, budget(a.len()))
}

pub fn sort_within<T>(
    crt: Crt,
    a: &mut [T],
    comp: &dyn Fn(&T, &T) -> Ordering,
    budget: u64,
) -> Result<Work> {
    let used = Cell::new(0u64);
    let ran_out = Cell::new(false);
    let counted = |x: &T, y: &T| -> Ordering {
        let n = used.get();
        if n >= budget {
            ran_out.set(true);
            return Ordering::Equal;
        }
        used.set(n + 1);
        comp(x, y)
    };
    match crt {
        Crt::Vc6 => vc6_qsort(a, &counted),
        Crt::Wine => wine_qsort(a, &counted),
    }
    let compares = used.get();
    if ran_out.get() {
        bail!(
            "{} qsort of {} element(s) ran out of its {budget} comparisons: an input built to make it quadratic",
            crt.label(),
            a.len()
        );
    }
    Ok(Work { compares, budget })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn both_ports_sort_distinct_keys() {
        let base: Vec<u32> = (0..200).map(|i| (i * 7919 + 13) % 1009).collect();
        let mut want = base.clone();
        want.sort_unstable();
        for crt in [Crt::Vc6, Crt::Wine] {
            let mut a = base.clone();
            sort(crt, &mut a, &|x, y| x.cmp(y)).unwrap();
            assert_eq!(a, want);
        }
    }

    #[test]
    fn vc6_shortsort_rotates_ties() {
        let mut a = vec![0u32, 1, 2, 3];
        vc6_qsort(&mut a, &|_, _| Ordering::Equal);
        assert_eq!(a, vec![1, 2, 3, 0]);
    }

    #[test]
    fn the_two_ports_differ_on_ties_above_the_cutoff() {
        let mut a: Vec<u32> = (0..20).collect();
        let mut b = a.clone();
        vc6_qsort(&mut a, &|_, _| Ordering::Equal);
        wine_qsort(&mut b, &|_, _| Ordering::Equal);
        assert_ne!(a, b);
    }

    #[test]
    fn above_the_cutoff_vc6_swaps_the_middle_to_the_front_and_nothing_else_moves() {
        let mut a: Vec<u32> = (0..9).collect();
        vc6_qsort(&mut a, &|_, _| Ordering::Equal);
        assert_eq!(a, vec![4, 1, 2, 3, 0, 5, 6, 7, 8]);
    }

    #[test]
    fn a_comparator_that_lies_spends_the_budget_and_is_refused() {
        let mut a: Vec<u32> = (0..5000).collect();
        let liar = |_: &u32, _: &u32| Ordering::Less;
        let e = sort(Crt::Vc6, &mut a, &liar).unwrap_err();
        assert!(format!("{e:#}").contains("ran out of its"), "{e:#}");
        let mut b: Vec<u32> = (0..5000).collect();
        let w = sort(Crt::Wine, &mut b, &|x, y| x.cmp(y)).unwrap();
        assert!(w.compares < w.budget / 4, "{w:?}");
    }

    #[test]
    fn a_sort_that_needs_exactly_its_budget_is_not_refused() {
        for crt in [Crt::Vc6, Crt::Wine] {
            let base: Vec<u32> = (0..300).map(|i| (i * 7919 + 13) % 1009).collect();
            let mut a = base.clone();
            let need = sort_within(crt, &mut a, &|x, y| x.cmp(y), u64::MAX)
                .unwrap()
                .compares;
            let mut b = base.clone();
            let w = sort_within(crt, &mut b, &|x, y| x.cmp(y), need).unwrap();
            assert_eq!((w.compares, b), (need, a));
            let mut c = base.clone();
            assert!(sort_within(crt, &mut c, &|x, y| x.cmp(y), need - 1).is_err());
        }
    }

    #[test]
    fn the_crt_names_parse() {
        assert_eq!(Crt::parse("VC6").unwrap(), Crt::Vc6);
        assert_eq!(Crt::parse("builtin").unwrap(), Crt::Wine);
        assert!(Crt::parse("glibc").is_err());
    }
}
