use crate::procscan::{self, Slotted};
use std::io::{Read as _, Seek as _, Write as _};
use std::path::{Path, PathBuf};

pub const INFRA: [&str; 7] = [
    "services.exe",
    "winedevice.exe",
    "plugplay.exe",
    "explorer.exe",
    "rpcss.exe",
    "svchost.exe",
    "conhost.exe",
];

pub const MARK_MAGIC: &str = "msvc6t-slot 1";

pub fn is_infra(comm: &str) -> bool {
    comm.starts_with("wineserver") || INFRA.iter().any(|i| comm.eq_ignore_ascii_case(i))
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Mark {
    pub owner: Option<(u32, u64)>,
    pub jobs: Vec<u64>,
    pub released: bool,
    pub foreign: Option<String>,
}

impl Mark {
    pub fn ours() -> Self {
        let me = std::process::id();
        Self {
            owner: procscan::start_time(me).map(|s| (me, s)),
            ..Self::default()
        }
    }

    pub fn parse(text: &str) -> Self {
        let t = text.trim();
        if t.is_empty() {
            return Self::default();
        }
        let mut lines = t.lines();
        if lines.next() != Some(MARK_MAGIC) {
            return Self {
                foreign: Some(t.to_string()),
                ..Self::default()
            };
        }
        let mut m = Self::default();
        for l in lines {
            let f: Vec<&str> = l.split_whitespace().collect();
            match f.as_slice() {
                ["owner", pid, start] => {
                    m.owner = pid.parse().ok().zip(start.parse().ok());
                }
                ["job", tick] => {
                    if let Ok(t) = tick.parse() {
                        m.jobs.push(t);
                    }
                }
                ["released", ..] => m.released = true,
                _ => {
                    return Self {
                        foreign: Some(t.to_string()),
                        ..Self::default()
                    };
                }
            }
        }
        m
    }

    pub fn render(&self) -> String {
        let mut s = format!("{MARK_MAGIC}\n");
        if let Some((p, st)) = self.owner {
            s.push_str(&format!("owner {p} {st}\n"));
        }
        for j in &self.jobs {
            s.push_str(&format!("job {j}\n"));
        }
        if self.released {
            s.push_str("released\n");
        }
        s
    }

    pub fn first_job(&self) -> Option<u64> {
        self.jobs.iter().copied().min()
    }

    pub fn owner_alive(&self) -> bool {
        self.owner
            .is_some_and(|(p, s)| procscan::start_time(p) == Some(s))
    }

    pub fn summary(&self) -> String {
        match (&self.foreign, self.owner) {
            (Some(f), _) => format!("unrecognised mark `{}`", f.lines().next().unwrap_or("")),
            (None, Some((p, s))) => format!(
                "owner {p} (started at tick {s}), {} job(s){}",
                self.jobs.len(),
                if self.released { ", released" } else { "" }
            ),
            (None, None) => "empty mark".to_string(),
        }
    }
}

pub fn read_mark(f: &mut std::fs::File) -> Mark {
    let mut b = Vec::new();
    if f.rewind().is_err() || f.read_to_end(&mut b).is_err() {
        return Mark::default();
    }
    Mark::parse(&msvc6_base::srcio::decode(&b))
}

pub fn write_mark(f: &mut std::fs::File, m: &Mark) -> std::io::Result<()> {
    f.set_len(0)?;
    f.rewind()?;
    f.write_all(m.render().as_bytes())?;
    f.flush()
}

pub fn slot_problem(pool: &Path, slot: &Path) -> Option<String> {
    let name = slot.file_name()?.to_owned();
    if slot.parent() != Some(pool) {
        return Some(format!(
            "{} is not a slot of {}",
            slot.display(),
            pool.display()
        ));
    }
    match std::fs::symlink_metadata(slot) {
        Ok(m) if m.file_type().is_symlink() => {
            return Some(format!(
                "{} is a symbolic link: a slot is never another directory's alias",
                slot.display()
            ));
        }
        Ok(m) if !m.is_dir() => return Some(format!("{} is not a directory", slot.display())),
        Ok(_) => {}
        Err(_) => return None,
    }
    let real_pool = std::fs::canonicalize(pool).ok()?;
    let real_slot = std::fs::canonicalize(slot).ok()?;
    (real_slot != real_pool.join(&name)).then(|| {
        format!(
            "{} resolves to {}, outside the pool {}",
            slot.display(),
            real_slot.display(),
            real_pool.display()
        )
    })
}

pub fn ours_in(slot: &Path, mark: &Mark, with_infra: bool) -> Vec<Slotted> {
    let Some(since) = mark.first_job() else {
        return Vec::new();
    };
    procscan::in_slot(slot)
        .into_iter()
        .filter(|p| with_infra || !is_infra(&p.comm))
        .filter(|p| p.start >= since)
        .collect()
}

pub fn kill_all(victims: &[Slotted]) -> Vec<Slotted> {
    victims
        .iter()
        .filter(|v| crate::ident::kill_verified(v.pid, v.start))
        .cloned()
        .collect()
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Sweep {
    pub slots: usize,
    pub held: usize,
    pub refused: Vec<(PathBuf, String)>,
    pub released: Vec<(PathBuf, String)>,
    pub orphans: Vec<(PathBuf, Slotted)>,
}

pub fn slot_locks(pool: &Path) -> Vec<(PathBuf, PathBuf)> {
    let mut v: Vec<(PathBuf, PathBuf)> = std::fs::read_dir(pool)
        .map(|rd| {
            rd.flatten()
                .filter_map(|e| {
                    let n = e.file_name().to_string_lossy().into_owned();
                    let stem = n.strip_suffix(".lock")?;
                    let num = stem.strip_prefix("wp")?;
                    (!num.is_empty() && num.bytes().all(|b| b.is_ascii_digit()))
                        .then(|| (e.path(), pool.join(stem)))
                })
                .collect()
        })
        .unwrap_or_default();
    v.sort();
    v
}

pub fn sweep(pool: &Path, dry: bool, owner: Option<(u32, u64)>) -> Sweep {
    let mut out = Sweep::default();
    for (lock, slot) in slot_locks(pool) {
        out.slots += 1;
        if let Some(why) = slot_problem(pool, &slot) {
            out.refused.push((slot, why));
            continue;
        }
        if std::fs::symlink_metadata(&lock).is_ok_and(|m| !m.is_file()) {
            out.refused
                .push((slot, "its lock is not a plain file".into()));
            continue;
        }
        let Ok(mut f) = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(&lock)
        else {
            continue;
        };
        if f.try_lock().is_err() {
            out.held += 1;
            continue;
        }
        let mut mark = read_mark(&mut f);
        if mark.foreign.is_some() || mark.released || mark.jobs.is_empty() {
            continue;
        }
        if owner.is_some_and(|o| mark.owner != Some(o)) || mark.owner_alive() {
            continue;
        }
        let victims = ours_in(&slot, &mark, false);
        let gone = if dry { victims } else { kill_all(&victims) };
        out.orphans
            .extend(gone.into_iter().map(|p| (slot.clone(), p)));
        out.released.push((slot.clone(), mark.summary()));
        if !dry {
            mark.released = true;
            let _ = write_mark(&mut f, &mark);
        }
    }
    out
}

pub fn retire(pool: &Path, slot: &Path) -> anyhow::Result<Vec<Slotted>> {
    if let Some(why) = slot_problem(pool, slot) {
        anyhow::bail!("refusing {}: {why}", slot.display());
    }
    let name = slot
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_default();
    let mut f = std::fs::OpenOptions::new()
        .read(true)
        .write(true)
        .open(pool.join(format!("{name}.lock")))?;
    f.try_lock()
        .map_err(|_| anyhow::anyhow!("{} is leased", slot.display()))?;
    let mark = read_mark(&mut f);
    Ok(kill_all(&ours_in(slot, &mark, true)))
}

pub fn watch(owner: u32, owner_start: u64, pool: &Path) -> Sweep {
    let mut buf = [0u8; 64];
    let mut stdin = std::io::stdin();
    while matches!(stdin.read(&mut buf), Ok(n) if n > 0) {}
    sweep(pool, false, Some((owner, owner_start)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::process::{Child, Command};

    struct Scratch(PathBuf);

    impl Drop for Scratch {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn scratch(tag: &str) -> Scratch {
        let d = std::env::temp_dir().join(format!("msvc6t-reap-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        Scratch(d)
    }

    fn in_wineprefix(prefix: &str) -> Child {
        Command::new("tail")
            .args(["-f", "/dev/null"])
            .env("WINEPREFIX", prefix)
            .spawn()
            .unwrap()
    }

    fn dead_owner_mark(jobs: &[u64]) -> Mark {
        Mark {
            owner: Some((4_000_000, 1)),
            jobs: jobs.to_vec(),
            released: false,
            foreign: None,
        }
    }

    fn mark_slot(pool: &Path, n: usize, m: &Mark) {
        std::fs::create_dir_all(pool.join(format!("wp{n}"))).unwrap();
        msvc6_base::srcio::own_write(pool.join(format!("wp{n}.lock")), m.render()).unwrap();
    }

    fn reaped(c: &mut Child) -> bool {
        let _ = c.kill();
        !c.wait().unwrap().success()
    }

    fn settled(pool: &Path, dry: bool, owner: Option<(u32, u64)>) -> Sweep {
        let until = std::time::Instant::now() + std::time::Duration::from_secs(10);
        loop {
            let r = sweep(pool, dry, owner);
            if r.held == 0 || std::time::Instant::now() >= until {
                return r;
            }
            std::thread::yield_now();
        }
    }

    #[test]
    fn a_mark_round_trips_and_an_old_one_is_foreign() {
        let m = dead_owner_mark(&[7, 9]);
        assert_eq!(Mark::parse(&m.render()), m);
        assert_eq!(m.first_job(), Some(7));
        assert!(Mark::parse("4242 1").foreign.is_some());
        assert_eq!(Mark::parse(""), Mark::default());
        assert!(!dead_owner_mark(&[]).owner_alive());
        assert!(Mark::ours().owner_alive());
    }

    #[test]
    fn a_same_uid_process_started_before_the_slots_first_job_is_not_ours() {
        let s = scratch("before");
        let pool = s.0.join("pool");
        let slot = pool.join("wp0");
        let mut early = in_wineprefix(slot.to_str().unwrap());
        let tick = procscan::start_time(early.id()).unwrap() + 1_000_000;
        mark_slot(&pool, 0, &dead_owner_mark(&[tick]));
        let r = settled(&pool, false, None);
        assert!(r.orphans.is_empty() && r.held == 0, "{r:?}");
        assert!(procscan::alive(early.id()));
        assert!(reaped(&mut early));
    }

    #[test]
    fn a_dead_holders_job_is_killed_and_the_mark_released() {
        let s = scratch("orphan");
        let pool = s.0.join("pool");
        let slot = pool.join("wp0");
        mark_slot(&pool, 0, &dead_owner_mark(&[0]));
        let mut job = in_wineprefix(slot.to_str().unwrap());
        let dry = settled(&pool, true, None);
        assert_eq!(dry.orphans.len(), 1, "{dry:?}");
        assert!(procscan::alive(job.id()));
        let other = settled(&pool, false, Some((77, 1)));
        assert!(other.orphans.is_empty() && other.held == 0, "{other:?}");
        let r = settled(&pool, false, Some((4_000_000, 1)));
        assert_eq!(r.orphans.len(), 1, "{r:?}");
        assert!(!job.wait().unwrap().success());
        let after = msvc6_base::srcio::own_read(pool.join("wp0.lock")).unwrap();
        assert!(Mark::parse(&after).released);
    }

    #[test]
    fn only_the_literal_slot_path_identifies_a_job_and_a_symlinked_slot_is_refused() {
        let s = scratch("alias");
        let pool_a = s.0.join("a");
        let pool_b = s.0.join("b");
        std::fs::create_dir_all(pool_b.join("wp0")).unwrap();
        std::fs::create_dir_all(&pool_a).unwrap();
        std::os::unix::fs::symlink(pool_b.join("wp0"), pool_a.join("wp3")).unwrap();
        msvc6_base::srcio::own_write(pool_a.join("wp3.lock"), dead_owner_mark(&[0]).render())
            .unwrap();
        mark_slot(&pool_a, 0, &dead_owner_mark(&[0]));
        let mut in_b = in_wineprefix(pool_b.join("wp0").to_str().unwrap());
        let mut slashed = in_wineprefix(&format!("{}/", pool_a.join("wp0").display()));
        let r = settled(&pool_a, false, None);
        assert!(r.orphans.is_empty() && r.held == 0, "{r:?}");
        assert!(
            r.refused
                .iter()
                .any(|(p, why)| p.ends_with("wp3") && why.contains("symbolic link"))
        );
        assert!(procscan::alive(in_b.id()) && procscan::alive(slashed.id()));
        assert!(reaped(&mut in_b) && reaped(&mut slashed));
    }

    #[test]
    fn a_held_slot_and_infrastructure_are_left_alone() {
        let s = scratch("held");
        let pool = s.0.join("pool");
        mark_slot(&pool, 0, &dead_owner_mark(&[0]));
        let held = std::fs::OpenOptions::new()
            .write(true)
            .open(pool.join("wp0.lock"))
            .unwrap();
        held.lock().unwrap();
        let r = sweep(&pool, false, None);
        assert_eq!((r.slots, r.held), (1, 1));
        drop(held);
        assert!(is_infra("wineserver32") && is_infra("services.exe") && !is_infra("CL.EXE"));
    }
}
