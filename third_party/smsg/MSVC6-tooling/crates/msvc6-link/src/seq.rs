use std::collections::BTreeSet;

fn lis_by<T: Ord>(seq: &[T], strict: bool) -> Vec<usize> {
    let mut tails: Vec<usize> = Vec::new();
    let mut prev: Vec<Option<usize>> = vec![None; seq.len()];
    for (i, v) in seq.iter().enumerate() {
        let k = tails.partition_point(|t| {
            seq.get(*t)
                .is_some_and(|x| if strict { x < v } else { x <= v })
        });
        if k > 0
            && let Some(p) = prev.get_mut(i)
        {
            *p = tails.get(k - 1).copied();
        }
        if k == tails.len() {
            tails.push(i);
        } else if let Some(t) = tails.get_mut(k) {
            *t = i;
        }
    }
    let mut out = Vec::new();
    let mut cur = tails.last().copied();
    while let Some(i) = cur {
        out.push(i);
        cur = prev.get(i).copied().flatten();
    }
    out.reverse();
    out
}

#[must_use]
pub fn lis_strict<T: Ord>(seq: &[T]) -> BTreeSet<usize> {
    lis_by(seq, true).into_iter().collect()
}

#[must_use]
pub fn lis_nondecreasing<T: Ord>(seq: &[T]) -> Vec<usize> {
    lis_by(seq, false)
}

#[must_use]
pub fn inversions(seq: &[usize]) -> u64 {
    let mut v = seq.to_vec();
    let mut buf = vec![0usize; v.len()];
    let mut n = 0u64;
    let mut width = 1usize;
    while width < v.len() {
        let mut lo = 0usize;
        while lo < v.len() {
            let mid = (lo + width).min(v.len());
            let hi = (lo + 2 * width).min(v.len());
            let (mut a, mut b, mut k) = (lo, mid, lo);
            while a < mid || b < hi {
                let take_b = match (v.get(a), v.get(b)) {
                    (Some(x), Some(y)) if a < mid && b < hi => y < x,
                    _ => a >= mid,
                };
                if take_b {
                    if let (Some(slot), Some(&y)) = (buf.get_mut(k), v.get(b)) {
                        *slot = y;
                    }
                    n += u64::try_from(mid - a).unwrap_or(u64::MAX);
                    b += 1;
                } else {
                    if let (Some(slot), Some(&x)) = (buf.get_mut(k), v.get(a)) {
                        *slot = x;
                    }
                    a += 1;
                }
                k += 1;
            }
            lo = hi;
        }
        std::mem::swap(&mut v, &mut buf);
        width *= 2;
    }
    n
}

pub const DP_CELLS: usize = 1 << 24;

fn distinct<T: Ord>(s: &[T]) -> bool {
    let mut seen = BTreeSet::new();
    s.iter().all(|x| seen.insert(x))
}

fn positions<T: Ord>(a: &[T], b: &[T]) -> Vec<usize> {
    let at: std::collections::BTreeMap<&T, usize> =
        b.iter().enumerate().map(|(i, x)| (x, i)).collect();
    a.iter().filter_map(|x| at.get(x).copied()).collect()
}

#[must_use]
pub fn lcs_len<T: Ord>(a: &[T], b: &[T]) -> usize {
    if distinct(a) && distinct(b) {
        return lis_by(&positions(a, b), true).len();
    }
    let mut prev = vec![0usize; b.len() + 1];
    let mut cur = vec![0usize; b.len() + 1];
    for x in a {
        for (j, y) in b.iter().enumerate() {
            let take = if x == y {
                prev.get(j).copied().unwrap_or(0) + 1
            } else {
                0
            };
            let skip = prev
                .get(j + 1)
                .copied()
                .unwrap_or(0)
                .max(cur.get(j).copied().unwrap_or(0));
            if let Some(c) = cur.get_mut(j + 1) {
                *c = take.max(skip);
            }
        }
        std::mem::swap(&mut prev, &mut cur);
        cur.iter_mut().for_each(|v| *v = 0);
    }
    prev.last().copied().unwrap_or(0)
}

#[must_use]
pub fn misplaced<T: Ord + Clone>(r: &[T], o: &[T]) -> Vec<(T, usize, usize)> {
    let (n, m) = (r.len(), o.len());
    let first: std::collections::BTreeMap<&T, usize> =
        o.iter().enumerate().rev().map(|(i, x)| (x, i)).collect();
    let mut kept: BTreeSet<&T> = BTreeSet::new();
    if (n + 1).saturating_mul(m + 1) <= DP_CELLS {
        let w = m + 1;
        let mut dp = vec![0u32; (n + 1) * w];
        let cell = |dp: &[u32], i: usize, j: usize| dp.get(i * w + j).copied().unwrap_or(0);
        for i in 1..=n {
            for j in 1..=m {
                let v = if r.get(i - 1) == o.get(j - 1) {
                    cell(&dp, i - 1, j - 1) + 1
                } else {
                    cell(&dp, i - 1, j).max(cell(&dp, i, j - 1))
                };
                if let Some(s) = dp.get_mut(i * w + j) {
                    *s = v;
                }
            }
        }
        let (mut i, mut j) = (n, m);
        while i > 0 && j > 0 {
            if r.get(i - 1) == o.get(j - 1) {
                if let Some(x) = r.get(i - 1) {
                    kept.insert(x);
                }
                i -= 1;
                j -= 1;
            } else if cell(&dp, i - 1, j) >= cell(&dp, i, j - 1) {
                i -= 1;
            } else {
                j -= 1;
            }
        }
    } else {
        let pos: Vec<(usize, &T)> = r.iter().filter_map(|x| Some((*first.get(x)?, x))).collect();
        let keys: Vec<usize> = pos.iter().map(|(p, _)| *p).collect();
        for k in lis_by(&keys, true) {
            if let Some((_, x)) = pos.get(k) {
                kept.insert(x);
            }
        }
    }
    r.iter()
        .enumerate()
        .filter(|(_, x)| !kept.contains(x))
        .filter_map(|(ri, x)| Some((x.clone(), ri + 1, first.get(x)? + 1)))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lcs_is_order_sensitive_with_or_without_repeats() {
        let a = ["a", "b", "c", "d"];
        assert_eq!(lcs_len(&a, &a), 4);
        assert_eq!(lcs_len(&a, &["d", "c", "b", "a"]), 1);
        assert_eq!(lcs_len(&a, &["a", "c"]), 2);
        assert_eq!(lcs_len::<&str>(&a, &[]), 0);
        assert_eq!(lcs_len(&["a", "a", "b"], &["a", "b", "a"]), 2);
        assert_eq!(
            misplaced(&a, &["a", "c", "b", "d"]),
            vec![("c", 3, 2)],
            "the backtrack prefers the reference's earlier row on a tie"
        );
        assert!(misplaced(&a, &a).is_empty());
    }

    #[test]
    fn lis_and_inversions_on_known_sequences() {
        assert_eq!(lis_nondecreasing(&[0usize, 1, 2, 3]).len(), 4);
        assert_eq!(inversions(&[0, 1, 2, 3]), 0);
        assert_eq!(inversions(&[3, 2, 1, 0]), 6);
        let s = [0usize, 4, 1, 2, 3];
        assert_eq!(lis_nondecreasing(&s), vec![0, 2, 3, 4]);
        assert_eq!(inversions(&s), 3);
        assert_eq!(lis_nondecreasing(&[1usize, 1, 2]).len(), 3);
        assert_eq!(inversions(&[1, 1, 2]), 0);
        assert!(lis_nondecreasing::<usize>(&[]).is_empty());
        assert_eq!(
            lis_strict(&[0usize, 1, 5, 2, 3, 4]),
            [0, 1, 3, 4, 5].into_iter().collect()
        );
        assert_eq!(lis_strict(&[1u32, 1, 2]).len(), 2);
    }

    #[test]
    fn inversions_agree_with_the_pair_count() {
        let s: Vec<usize> = (0..97).map(|i| (i * 31 + 7) % 53).collect();
        let mut want = 0u64;
        for i in 0..s.len() {
            for j in i + 1..s.len() {
                if s[i] > s[j] {
                    want += 1;
                }
            }
        }
        assert_eq!(inversions(&s), want);
    }
}
