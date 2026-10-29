use std::sync::Mutex;
use std::sync::atomic::{AtomicUsize, Ordering};

pub const MAX_JOBS: usize = 8;

#[must_use]
pub fn default_jobs() -> usize {
    let n = std::thread::available_parallelism().map_or(1, std::num::NonZero::get);
    (n / 2).clamp(1, MAX_JOBS)
}

pub fn map<T, R, F>(items: &[T], jobs: usize, f: F) -> Vec<R>
where
    T: Sync,
    R: Send,
    F: Fn(&T) -> R + Sync,
{
    let jobs = jobs.clamp(1, items.len().max(1));
    if jobs == 1 {
        return items.iter().map(&f).collect();
    }
    let next = AtomicUsize::new(0);
    let done: Mutex<Vec<(usize, R)>> = Mutex::new(Vec::with_capacity(items.len()));
    std::thread::scope(|s| {
        for _ in 0..jobs {
            s.spawn(|| {
                loop {
                    let i = next.fetch_add(1, Ordering::Relaxed);
                    let Some(item) = items.get(i) else {
                        break;
                    };
                    let r = f(item);
                    if let Ok(mut d) = done.lock() {
                        d.push((i, r));
                    }
                }
            });
        }
    });
    let mut done = done.into_inner().unwrap_or_default();
    done.sort_by_key(|(i, _)| *i);
    done.into_iter().map(|(_, r)| r).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_results_keep_the_input_order() {
        let v: Vec<usize> = (0..100).collect();
        assert_eq!(
            map(&v, 4, |x| x * 2),
            v.iter().map(|x| x * 2).collect::<Vec<_>>()
        );
        assert_eq!(map(&v, 1, |x| x + 1)[99], 100);
        assert!(map(&Vec::<usize>::new(), 4, |x| *x).is_empty());
    }
}
