use anyhow::{Context as _, Result, bail};
use std::fs::File;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

pub const MAX_JOBS: usize = 8;

pub fn default_jobs() -> usize {
    let cores = std::thread::available_parallelism().map_or(2, std::num::NonZero::get);
    (cores / 2).clamp(1, MAX_JOBS)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tokens {
    pub dir: PathBuf,
    pub total: usize,
}

#[derive(Debug)]
pub struct Token {
    pub slot: usize,
    _file: File,
}

fn open(p: &Path) -> std::io::Result<File> {
    File::options()
        .create(true)
        .truncate(false)
        .write(true)
        .open(p)
}

impl Tokens {
    pub fn new(dir: PathBuf, total: usize) -> Self {
        Self {
            dir,
            total: total.max(1),
        }
    }

    fn path(&self, k: usize) -> PathBuf {
        self.dir.join(format!("t{k}.lock"))
    }

    pub fn acquire(&self) -> Result<Token> {
        std::fs::create_dir_all(&self.dir)
            .with_context(|| format!("creating {}", self.dir.display()))?;
        let queue = open(&self.dir.join("queue.lock"))?;
        queue.lock().context("waiting in the job queue")?;
        let start = usize::try_from(std::process::id()).unwrap_or(0) % self.total;
        let mut oldest: Option<(SystemTime, usize)> = None;
        for i in 0..self.total {
            let k = (start + i) % self.total;
            let f = open(&self.path(k))?;
            match f.try_lock() {
                Ok(()) => {
                    let _ = f.set_modified(SystemTime::now());
                    return Ok(Token { slot: k, _file: f });
                }
                Err(std::fs::TryLockError::WouldBlock) => {
                    let t = f
                        .metadata()
                        .and_then(|m| m.modified())
                        .unwrap_or(SystemTime::UNIX_EPOCH);
                    if oldest.is_none_or(|(o, _)| t < o) {
                        oldest = Some((t, k));
                    }
                }
                Err(std::fs::TryLockError::Error(e)) => return Err(e.into()),
            }
        }
        let k = oldest.map_or(start, |(_, k)| k);
        let f = open(&self.path(k))?;
        f.lock()
            .with_context(|| format!("waiting for job token {k}"))?;
        let _ = f.set_modified(SystemTime::now());
        Ok(Token { slot: k, _file: f })
    }

    pub fn held(&self) -> usize {
        (0..self.total)
            .filter(|&k| {
                open(&self.path(k))
                    .is_ok_and(|f| matches!(f.try_lock(), Err(std::fs::TryLockError::WouldBlock)))
            })
            .count()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Memory {
    pub available: u64,
    pub cgroup_headroom: Option<u64>,
}

impl Memory {
    pub fn usable(&self) -> u64 {
        self.cgroup_headroom
            .map_or(self.available, |h| h.min(self.available))
    }
}

pub fn meminfo_available(text: &str) -> Option<u64> {
    text.lines().find_map(|l| {
        let rest = l.strip_prefix("MemAvailable:")?;
        let kb: u64 = rest.trim().trim_end_matches("kB").trim().parse().ok()?;
        kb.checked_mul(1024)
    })
}

fn read_num(p: &Path) -> Option<u64> {
    let b = std::fs::read(p).ok()?;
    msvc6_base::srcio::decode(&b).trim().parse().ok()
}

pub fn stat_value(text: &str, key: &str) -> Option<u64> {
    text.lines().find_map(|l| {
        let (k, v) = l.split_once(' ')?;
        (k == key).then(|| v.trim().parse().ok()).flatten()
    })
}

pub fn memory() -> Memory {
    let available = std::fs::read("/proc/meminfo")
        .ok()
        .and_then(|b| meminfo_available(&msvc6_base::srcio::decode(&b)))
        .unwrap_or(u64::MAX);
    let cgroup_headroom = crate::procscan::self_cgroup_memory_dir().and_then(|d| {
        if let Some(limit) = read_num(&d.join("memory.limit_in_bytes")) {
            let stat = std::fs::read(d.join("memory.stat")).ok()?;
            let rss = stat_value(&msvc6_base::srcio::decode(&stat), "total_rss")?;
            return Some(limit.saturating_sub(rss));
        }
        let max = read_num(&d.join("memory.max"))?;
        let cur = read_num(&d.join("memory.current"))?;
        Some(max.saturating_sub(cur))
    });
    Memory {
        available,
        cgroup_headroom,
    }
}

pub fn plan(want: usize, jobs: usize, per_job: u64, floor: u64, mem: Memory) -> Result<usize> {
    let usable = mem.usable();
    let room = usable.saturating_sub(floor);
    if room < per_job {
        bail!(
            "memory: {} usable ({} MemAvailable{}), under the floor {} plus one job's {}; nothing started",
            human(usable),
            human(mem.available),
            mem.cgroup_headroom
                .map(|h| format!(", cgroup headroom {}", human(h)))
                .unwrap_or_default(),
            human(floor),
            human(per_job)
        );
    }
    let fit = usize::try_from(room / per_job.max(1)).unwrap_or(usize::MAX);
    Ok(want.min(jobs).min(fit).max(1))
}

pub fn human(b: u64) -> String {
    let gib = 1u64 << 30;
    if b == u64::MAX {
        "unlimited".to_string()
    } else if b >= gib {
        format!("{}.{} GiB", b / gib, (b % gib) * 10 / gib)
    } else {
        format!("{} MiB", b >> 20)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_default_never_passes_eight() {
        assert!((1..=MAX_JOBS).contains(&default_jobs()));
    }

    #[test]
    fn tokens_exclude_each_other_and_a_waiter_gets_the_one_released() {
        let d = std::env::temp_dir().join(format!("msvc6t-tokens-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let t = Tokens::new(d.clone(), 2);
        let a = t.acquire().unwrap();
        let b = t.acquire().unwrap();
        assert_ne!(a.slot, b.slot);
        assert_eq!(t.held(), 2);
        let t2 = t.clone();
        let h = std::thread::spawn(move || t2.acquire().unwrap().slot);
        drop(a);
        drop(b);
        assert!(h.join().unwrap() < 2);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn the_memory_plan_shrinks_the_batch_and_refuses_below_the_floor() {
        let g = 1u64 << 30;
        let m = Memory {
            available: 40 * g,
            cgroup_headroom: Some(10 * g),
        };
        assert_eq!(m.usable(), 10 * g);
        assert_eq!(plan(16, 8, 2 * g, 4 * g, m).unwrap(), 3);
        assert_eq!(plan(2, 8, g, 0, m).unwrap(), 2);
        let e = plan(4, 8, 2 * g, 9 * g, m).unwrap_err().to_string();
        assert!(e.contains("under the floor"), "{e}");
        assert_eq!(
            meminfo_available("MemTotal: 9 kB\nMemAvailable:   1024 kB\n"),
            Some(1 << 20)
        );
        assert_eq!(stat_value("cache 1\ntotal_rss 77\n", "total_rss"), Some(77));
    }
}
