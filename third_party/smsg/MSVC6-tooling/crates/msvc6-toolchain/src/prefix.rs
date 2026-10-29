use anyhow::{Context as _, Result, bail};
use std::fs::File;
use std::path::{Path, PathBuf};

pub const MAX_SLOTS: usize = 256;
pub const REG_FILES: [&str; 3] = ["system.reg", "user.reg", "userdef.reg"];
pub const REG_HEADER: &[u8] = b"WINE REGISTRY Version 2\n";

#[derive(Debug, Clone)]
pub struct PrefixPool {
    pub root: PathBuf,
    pub template: PathBuf,
}

#[derive(Debug)]
pub struct Lease {
    pub path: PathBuf,
    pub slot: usize,
    pub pool: PathBuf,
    lock: std::sync::Mutex<File>,
    mark: std::sync::Mutex<crate::reap::Mark>,
}

impl Lease {
    pub fn drive_c(&self) -> PathBuf {
        self.path.join("drive_c")
    }

    pub fn record_job(&self, child: u32) -> Option<u64> {
        let tick = crate::procscan::start_time(child)?;
        let (Ok(mut m), Ok(mut f)) = (self.mark.lock(), self.lock.lock()) else {
            return Some(tick);
        };
        m.jobs.push(tick);
        let _ = crate::reap::write_mark(&mut f, &m);
        Some(tick)
    }

    pub fn reap_job(&self, since: u64) -> Vec<crate::procscan::Slotted> {
        let m = crate::reap::Mark {
            jobs: vec![since],
            ..crate::reap::Mark::default()
        };
        crate::reap::kill_all(&crate::reap::ours_in(&self.path, &m, false))
    }

    pub fn mark(&self) -> crate::reap::Mark {
        self.mark.lock().map(|m| m.clone()).unwrap_or_default()
    }
}

impl Drop for Lease {
    fn drop(&mut self) {
        if let (Ok(m), Ok(f)) = (self.mark.get_mut(), self.lock.get_mut()) {
            m.released = true;
            let _ = crate::reap::write_mark(f, m);
            let _ = f.unlock();
        }
    }
}

pub fn reg_keys(text: &[u8]) -> usize {
    text.windows(2).filter(|w| *w == b"\n[").count()
}

pub fn bad_registry(prefix: &Path, min: &dyn Fn(&str) -> usize) -> Option<String> {
    for f in REG_FILES {
        let Ok(b) = std::fs::read(prefix.join(f)) else {
            return Some(format!("{f} unreadable"));
        };
        if !b.starts_with(REG_HEADER) {
            return Some(format!(
                "{f} has no wine registry header ({} bytes)",
                b.len()
            ));
        }
        let (k, m) = (reg_keys(&b), min(f));
        if k < m {
            return Some(format!(
                "{f} holds {k} keys, under half the template's ({m}): truncated"
            ));
        }
    }
    None
}

fn copy_attrs(src: &Path, dst: &Path) -> Result<()> {
    let m = std::fs::metadata(src)?;
    std::fs::set_permissions(dst, m.permissions())?;
    if let Ok(t) = m.modified() {
        let f = File::options().write(true).open(dst)?;
        f.set_modified(t)?;
    }
    Ok(())
}

pub fn copy_tree(src: &Path, dst: &Path, hardlink: bool) -> Result<u64> {
    let mut n = 0u64;
    let mut todo = vec![(src.to_path_buf(), dst.to_path_buf())];
    while let Some((s, d)) = todo.pop() {
        let meta =
            std::fs::symlink_metadata(&s).with_context(|| format!("reading {}", s.display()))?;
        let ft = meta.file_type();
        if ft.is_symlink() {
            let target = std::fs::read_link(&s)?;
            std::os::unix::fs::symlink(&target, &d)
                .with_context(|| format!("linking {}", d.display()))?;
        } else if ft.is_dir() {
            std::fs::create_dir_all(&d).with_context(|| format!("creating {}", d.display()))?;
            for e in std::fs::read_dir(&s)?.flatten() {
                todo.push((e.path(), d.join(e.file_name())));
            }
        } else if ft.is_file() {
            if !hardlink || std::fs::hard_link(&s, &d).is_err() {
                std::fs::copy(&s, &d)
                    .with_context(|| format!("copying {} to {}", s.display(), d.display()))?;
                copy_attrs(&s, &d)?;
            }
            n += 1;
        }
    }
    Ok(n)
}

pub fn remove_any(p: &Path) -> Result<()> {
    match std::fs::symlink_metadata(p) {
        Err(_) => Ok(()),
        Ok(m) if m.is_dir() => {
            std::fs::remove_dir_all(p).with_context(|| format!("removing {}", p.display()))
        }
        Ok(_) => std::fs::remove_file(p).with_context(|| format!("removing {}", p.display())),
    }
}

pub fn create(template: &Path, dest: &Path) -> Result<()> {
    if !template.join("drive_c").is_dir() {
        bail!(
            "no wine prefix template at {} (a prefix with drive_c/; set [runner] template)",
            template.display()
        );
    }
    remove_any(dest)?;
    std::fs::create_dir_all(dest.join("drive_c"))?;
    copy_tree(
        &template.join("drive_c/windows"),
        &dest.join("drive_c/windows"),
        true,
    )?;
    for e in std::fs::read_dir(template.join("drive_c"))?.flatten() {
        let p = e.path();
        if std::fs::symlink_metadata(&p).is_ok_and(|m| m.file_type().is_symlink()) {
            let target = std::fs::canonicalize(&p).unwrap_or_else(|_| p.clone());
            std::os::unix::fs::symlink(target, dest.join("drive_c").join(e.file_name()))?;
        }
    }
    for f in REG_FILES.iter().copied().chain([".update-timestamp"]) {
        let s = template.join(f);
        if s.exists() {
            std::fs::copy(&s, dest.join(f))?;
            copy_attrs(&s, &dest.join(f))?;
        }
    }
    let dd = template.join("dosdevices");
    if dd.is_dir() {
        copy_tree(&dd, &dest.join("dosdevices"), false)?;
    }
    let users = template.join("drive_c/users");
    if users.is_dir() {
        copy_tree(&users, &dest.join("drive_c/users"), false)?;
    }
    std::fs::create_dir_all(dest.join("drive_c/users/Public"))?;
    let names: Vec<PathBuf> = std::fs::read_dir(dest.join("drive_c/users"))?
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.is_dir())
        .chain([dest.join("drive_c/users/wine")])
        .collect();
    for u in names {
        std::fs::create_dir_all(u.join("Temp"))?;
    }
    Ok(())
}

impl PrefixPool {
    pub fn new(root: PathBuf, template: PathBuf) -> Self {
        Self { root, template }
    }

    fn min_keys(&self, file: &str) -> usize {
        std::fs::read(self.template.join(file)).map_or(0, |b| reg_keys(&b) / 2)
    }

    pub fn ensure(&self, path: &Path, prev: &crate::reap::Mark) -> Result<()> {
        if let Some(why) = crate::reap::slot_problem(&self.root, path) {
            bail!("refusing slot {}: {why}", path.display());
        }
        if path.join("drive_c").is_dir() {
            match bad_registry(path, &|f| self.min_keys(f)) {
                None => return Ok(()),
                Some(why) => {
                    eprintln!(
                        "note: wine prefix {} is broken ({why}); re-creating it from {}",
                        path.display(),
                        self.template.display()
                    );
                    crate::reap::kill_all(&crate::reap::ours_in(path, prev, true));
                    let left = crate::procscan::in_slot(path);
                    if !left.is_empty() {
                        bail!(
                            "wine prefix {} is broken and {} process(es) this pool cannot prove it started still \
                             run in it ({}): not re-creating it under them",
                            path.display(),
                            left.len(),
                            left.iter()
                                .map(|p| format!("{} {}", p.pid, p.comm))
                                .collect::<Vec<_>>()
                                .join(", ")
                        );
                    }
                }
            }
        }
        create(&self.template, path)?;
        if let Some(why) = bad_registry(path, &|f| self.min_keys(f)) {
            bail!(
                "the prefix created at {} is not whole ({why}): is the template {} damaged, or the disk full?",
                path.display(),
                self.template.display()
            );
        }
        Ok(())
    }

    fn open_lock(&self, slot: usize) -> Result<File> {
        std::fs::create_dir_all(&self.root)
            .with_context(|| format!("creating {}", self.root.display()))?;
        let p = self.root.join(format!("wp{slot}.lock"));
        if std::fs::symlink_metadata(&p).is_ok_and(|m| !m.is_file()) {
            bail!("{} is not a plain file", p.display());
        }
        File::options()
            .create(true)
            .truncate(false)
            .read(true)
            .write(true)
            .open(&p)
            .with_context(|| format!("opening {}", p.display()))
    }

    fn take(&self, slot: usize, mut lock: File) -> Result<Lease> {
        let path = self.root.join(format!("wp{slot}"));
        let prev = crate::reap::read_mark(&mut lock);
        if prev.foreign.is_none() && !prev.released && !prev.owner_alive() {
            let killed = crate::reap::kill_all(&crate::reap::ours_in(&path, &prev, false));
            if !killed.is_empty() {
                eprintln!(
                    "note: wine prefix {} was left leased ({}); killed {} of its job process(es): {}",
                    path.display(),
                    prev.summary(),
                    killed.len(),
                    killed
                        .iter()
                        .map(|p| format!("{} {}", p.pid, p.comm))
                        .collect::<Vec<_>>()
                        .join(", ")
                );
            }
        }
        self.ensure(&path, &prev)?;
        let mark = crate::reap::Mark::ours();
        crate::reap::write_mark(&mut lock, &mark)?;
        Ok(Lease {
            path,
            slot,
            pool: self.root.clone(),
            lock: std::sync::Mutex::new(lock),
            mark: std::sync::Mutex::new(mark),
        })
    }

    pub fn lease(&self) -> Result<Lease> {
        let mut refused = Vec::new();
        for slot in 0..MAX_SLOTS {
            if let Some(why) =
                crate::reap::slot_problem(&self.root, &self.root.join(format!("wp{slot}")))
            {
                refused.push(why);
                continue;
            }
            let lock = self.open_lock(slot)?;
            match lock.try_lock() {
                Ok(()) => return self.take(slot, lock),
                Err(std::fs::TryLockError::WouldBlock) => {}
                Err(std::fs::TryLockError::Error(e)) => {
                    return Err(e).with_context(|| format!("locking slot {slot}"));
                }
            }
        }
        let slot = usize::try_from(std::process::id()).unwrap_or(0) % MAX_SLOTS;
        if let Some(why) =
            crate::reap::slot_problem(&self.root, &self.root.join(format!("wp{slot}")))
        {
            bail!(
                "every slot is busy or refused ({} refused; slot {slot}: {why})",
                refused.len()
            );
        }
        let lock = self.open_lock(slot)?;
        lock.lock()
            .with_context(|| format!("waiting for wine prefix slot {slot}"))?;
        self.take(slot, lock)
    }
}

pub fn install_pinned(prefix: &Path, name: &str, src: &Path, md5: Option<&str>) -> Result<String> {
    let sys = prefix.join("drive_c/windows/system32");
    let dst = sys.join(name);
    let bytes = std::fs::read(src).with_context(|| format!("reading {}", src.display()))?;
    let have = msvc6_base::digest::md5_hex(&bytes);
    if let Some(pin) = md5
        && !pin.eq_ignore_ascii_case(&have)
    {
        bail!(
            "{} has md5 {have}, not the pinned {pin}: refusing to run the tools on another C runtime",
            src.display()
        );
    }
    if std::fs::read(&dst).is_ok_and(|b| msvc6_base::digest::md5_hex(&b) == have) {
        return Ok(have);
    }
    let tmp = sys.join(format!(".{name}.msvc6t-{}", std::process::id()));
    msvc6_base::srcio::write_bytes(&tmp, &bytes)
        .with_context(|| format!("writing {}", tmp.display()))?;
    std::fs::rename(&tmp, &dst)
        .with_context(|| format!("installing {} in {}", name, prefix.display()))?;
    Ok(have)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn reg(keys: usize) -> Vec<u8> {
        let mut s = String::from("WINE REGISTRY Version 2\n;; x\n\n#arch=win32\n");
        for i in 0..keys {
            s.push_str(&format!("\n[Software\\\\K{i}] 1\n\"v\"=\"1\"\n"));
        }
        s.into_bytes()
    }

    fn template(d: &Path) -> PathBuf {
        let t = d.join("template");
        std::fs::create_dir_all(t.join("drive_c/windows/system32")).unwrap();
        std::fs::create_dir_all(t.join("drive_c/users/someone")).unwrap();
        std::fs::create_dir_all(t.join("dosdevices")).unwrap();
        std::os::unix::fs::symlink("../drive_c", t.join("dosdevices/c:")).unwrap();
        msvc6_base::srcio::write_bytes(t.join("drive_c/windows/system32/msvcrt.dll"), b"builtin")
            .unwrap();
        for f in REG_FILES {
            msvc6_base::srcio::write_bytes(t.join(f), &reg(10)).unwrap();
        }
        t
    }

    #[test]
    fn a_prefix_shares_the_templates_files_and_owns_its_registry() {
        let d = std::env::temp_dir().join(format!("msvc6t-prefix-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let t = template(&d);
        let pool = PrefixPool::new(d.join("pool"), t.clone());
        let a = pool.lease().unwrap();
        let b = pool.lease().unwrap();
        assert_ne!(a.slot, b.slot);
        use std::os::unix::fs::MetadataExt as _;
        let ino = |p: &Path| std::fs::metadata(p).unwrap().ino();
        let dll = "drive_c/windows/system32/msvcrt.dll";
        assert_eq!(ino(&a.path.join(dll)), ino(&t.join(dll)));
        assert_ne!(ino(&a.path.join("system.reg")), ino(&t.join("system.reg")));
        assert!(a.path.join("drive_c/users/someone/Temp").is_dir());
        assert!(
            std::fs::symlink_metadata(a.path.join("dosdevices/c:"))
                .unwrap()
                .file_type()
                .is_symlink()
        );
        let mark =
            msvc6_base::srcio::own_read(pool.root.join(format!("wp{}.lock", a.slot))).unwrap();
        assert_eq!(
            crate::reap::Mark::parse(&mark).owner.map(|o| o.0),
            Some(std::process::id())
        );
        let mut kid = std::process::Command::new("true").spawn().unwrap();
        let tick = a.record_job(kid.id()).unwrap();
        kid.wait().unwrap();
        assert_eq!(a.mark().jobs, vec![tick]);
        msvc6_base::srcio::write_bytes(d.join("native.dll"), b"native").unwrap();
        let pin = msvc6_base::digest::md5_hex(b"native");
        install_pinned(&a.path, "msvcrt.dll", &d.join("native.dll"), Some(&pin)).unwrap();
        assert_eq!(std::fs::read(a.path.join(dll)).unwrap(), b"native");
        assert_eq!(std::fs::read(t.join(dll)).unwrap(), b"builtin");
        let e =
            install_pinned(&b.path, "msvcrt.dll", &d.join("native.dll"), Some("00")).unwrap_err();
        assert!(e.to_string().contains("not the pinned"), "{e}");
        let slot = a.slot;
        drop(a);
        let after = msvc6_base::srcio::own_read(pool.root.join(format!("wp{slot}.lock"))).unwrap();
        let after = crate::reap::Mark::parse(&after);
        assert!(after.released && after.jobs == vec![tick]);
        msvc6_base::srcio::write_bytes(b.path.join("system.reg"), &reg(1)).unwrap();
        let why = bad_registry(&b.path, &|f| pool.min_keys(f)).unwrap();
        assert!(why.contains("system.reg holds 1 keys"), "{why}");
        pool.ensure(&b.path, &b.mark()).unwrap();
        assert_eq!(bad_registry(&b.path, &|f| pool.min_keys(f)), None);
        drop(b);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_symlinked_slot_is_never_leased_or_recreated() {
        let d = std::env::temp_dir().join(format!("msvc6t-prefix-alias-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let t = template(&d);
        let elsewhere = d.join("elsewhere");
        std::fs::create_dir_all(&elsewhere).unwrap();
        msvc6_base::srcio::write_bytes(elsewhere.join("keep"), b"k").unwrap();
        let pool = PrefixPool::new(d.join("pool"), t);
        std::fs::create_dir_all(&pool.root).unwrap();
        std::os::unix::fs::symlink(&elsewhere, pool.root.join("wp0")).unwrap();
        let a = pool.lease().unwrap();
        assert_eq!(a.slot, 1);
        assert!(elsewhere.join("keep").is_file());
        assert!(!elsewhere.join("drive_c").exists());
        let e = pool
            .ensure(&pool.root.join("wp0"), &crate::reap::Mark::default())
            .unwrap_err();
        assert!(e.to_string().contains("symbolic link"), "{e}");
        drop(a);
        std::fs::remove_dir_all(&d).unwrap();
    }
}
