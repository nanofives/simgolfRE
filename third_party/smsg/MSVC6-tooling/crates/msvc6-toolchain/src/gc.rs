use crate::cache::{Cache, OK_FILE, USED_FILE, is_hex};
use anyhow::{Result, bail};
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime};

pub const TOUCH_EVERY: Duration = Duration::from_secs(60);
pub const LOW_WATER_TENTHS: u64 = 9;
pub const GC_EVERY: u64 = 256;
pub const GRACE: Duration = Duration::from_secs(60);
pub const TMP_STALE: Duration = Duration::from_secs(6 * 3600);

fn lmeta(p: &Path) -> Option<std::fs::Metadata> {
    std::fs::symlink_metadata(p).ok()
}

fn real_dir(p: &Path) -> bool {
    lmeta(p).is_some_and(|m| m.is_dir() && !m.file_type().is_symlink())
}

fn real_file(p: &Path) -> bool {
    lmeta(p).is_some_and(|m| m.is_file())
}

fn mtime(p: &Path) -> Option<SystemTime> {
    lmeta(p)?.modified().ok()
}

fn age(t: SystemTime, now: SystemTime) -> Duration {
    now.duration_since(t).unwrap_or_default()
}

pub fn touch_used(e: &Path) {
    let p = e.join(USED_FILE);
    let now = SystemTime::now();
    if mtime(&p).is_some_and(|t| age(t, now) < TOUCH_EVERY) {
        return;
    }
    if lmeta(&p).is_some_and(|m| !m.is_file()) {
        return;
    }
    if let Ok(f) = std::fs::File::options()
        .create(true)
        .truncate(false)
        .write(true)
        .open(&p)
    {
        let _ = f.set_modified(now);
    }
}

pub fn note_store(root: &Path) {
    use std::io::Write as _;
    let p = root.join("grown");
    if lmeta(&p).is_some_and(|m| !m.is_file()) {
        return;
    }
    if let Ok(mut f) = std::fs::File::options().create(true).append(true).open(&p) {
        let _ = f.write_all(b".");
    }
}

pub fn tmp_key(name: &str) -> Option<&str> {
    let rest = name.strip_prefix(".tmp-")?;
    let (key, pid) = rest.rsplit_once('-')?;
    (is_hex(key, 64) && !pid.is_empty() && pid.bytes().all(|b| b.is_ascii_digit())).then_some(key)
}

pub fn trash_parts(name: &str) -> Option<(&str, u128)> {
    let (key, nanos) = name.rsplit_once('-')?;
    if !is_hex(key, 64) || nanos.is_empty() || !nanos.bytes().all(|b| b.is_ascii_digit()) {
        return None;
    }
    Some((key, nanos.parse().ok()?))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    pub key: String,
    pub dir: PathBuf,
    pub bytes: u64,
    pub used: SystemTime,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Scan {
    pub entries: Vec<Entry>,
    pub bytes: u64,
    pub broken: Vec<PathBuf>,
    pub foreign: Vec<PathBuf>,
}

fn dir_bytes(d: &Path) -> u64 {
    std::fs::read_dir(d)
        .map(|rd| {
            rd.flatten()
                .filter_map(|e| lmeta(&e.path()))
                .filter(std::fs::Metadata::is_file)
                .map(|m| m.len())
                .sum()
        })
        .unwrap_or(0)
}

fn names(d: &Path) -> Vec<(String, PathBuf)> {
    if !real_dir(d) {
        return Vec::new();
    }
    std::fs::read_dir(d)
        .map(|rd| {
            rd.flatten()
                .map(|e| (e.file_name().to_string_lossy().into_owned(), e.path()))
                .collect()
        })
        .unwrap_or_default()
}

fn shards(d: &Path, foreign: &mut Vec<PathBuf>) -> Vec<PathBuf> {
    names(d)
        .into_iter()
        .filter_map(|(n, p)| {
            if is_hex(&n, 2) && real_dir(&p) {
                Some(p)
            } else {
                foreign.push(p);
                None
            }
        })
        .collect()
}

pub fn scan(root: &Path) -> Scan {
    let mut s = Scan::default();
    let mut foreign = Vec::new();
    for sh in shards(&root.join("o"), &mut foreign) {
        for (name, e) in names(&sh) {
            if !real_dir(&e) {
                foreign.push(e);
                continue;
            }
            if tmp_key(&name).is_some() {
                s.broken.push(e);
                continue;
            }
            if !is_hex(&name, 64) {
                foreign.push(e);
                continue;
            }
            let ok = e.join(OK_FILE);
            let Some(okt) = real_file(&ok).then(|| mtime(&ok)).flatten() else {
                s.broken.push(e);
                continue;
            };
            let used = real_file(&e.join(USED_FILE))
                .then(|| mtime(&e.join(USED_FILE)))
                .flatten()
                .map_or(okt, |u| u.max(okt));
            let bytes = dir_bytes(&e);
            s.bytes += bytes;
            s.entries.push(Entry {
                key: name,
                dir: e,
                bytes,
                used,
            });
        }
    }
    s.foreign = foreign;
    s
}

pub fn plan(
    entries: &[Entry],
    cap: Option<u64>,
    older: Option<Duration>,
    now: SystemTime,
) -> Vec<Entry> {
    let mut v: Vec<&Entry> = entries.iter().collect();
    v.sort_by_key(|e| e.used);
    let total: u64 = entries.iter().map(|e| e.bytes).sum();
    let mut keep = total;
    let low = cap.map(|c| c / 10 * LOW_WATER_TENTHS);
    let mut out = Vec::new();
    for e in v {
        let old = older.is_some_and(|o| age(e.used, now) >= o);
        let over = cap.is_some_and(|c| total > c) && low.is_some_and(|l| keep > l);
        if old || over {
            keep = keep.saturating_sub(e.bytes);
            out.push(e.clone());
        }
    }
    out
}

pub fn evict(root: &Path, e: &Entry, now: SystemTime) -> bool {
    let nanos = now
        .duration_since(SystemTime::UNIX_EPOCH)
        .map_or(0, |d| d.as_nanos());
    let t = root.join("t");
    if lmeta(&t).is_none() && std::fs::create_dir(&t).is_err() {
        return false;
    }
    real_dir(&t)
        && real_dir(&e.dir)
        && is_hex(&e.key, 64)
        && std::fs::rename(&e.dir, t.join(format!("{}-{nanos}", e.key))).is_ok()
}

pub fn purge_trash(root: &Path, grace: Duration, now: SystemTime, dry: bool) -> (usize, u64) {
    let mut n = (0, 0);
    for (name, d) in names(&root.join("t")) {
        let Some((_, nanos)) = trash_parts(&name) else {
            continue;
        };
        if !real_dir(&d) {
            continue;
        }
        let at = u64::try_from(nanos)
            .ok()
            .and_then(|ns| SystemTime::UNIX_EPOCH.checked_add(Duration::from_nanos(ns)));
        if at.is_some_and(|t| age(t, now) >= grace) {
            let b = dir_bytes(&d);
            if dry || std::fs::remove_dir_all(&d).is_ok() {
                n.0 += 1;
                n.1 += b;
            }
        }
    }
    n
}

pub fn prune_manifests(cache: &Cache, dry: bool) -> usize {
    let mut n = 0;
    let mut foreign = Vec::new();
    for sh in shards(&cache.root.join("m"), &mut foreign) {
        for (name, m) in names(&sh) {
            if !real_file(&m) {
                continue;
            }
            let stale_tmp = name.split_once(".tmp-").is_some_and(|(k, pid)| {
                is_hex(k, 64) && !pid.is_empty() && pid.bytes().all(|b| b.is_ascii_digit())
            }) && mtime(&m).is_some_and(|t| age(t, SystemTime::now()) > TMP_STALE);
            let absent = is_hex(&name, 64)
                && cache
                    .read_manifest(&name)
                    .is_some_and(|mf| Cache::part_sums(&cache.entry(&mf.key)).is_none());
            if stale_tmp || absent {
                n += 1;
                if !dry {
                    let _ = std::fs::remove_file(&m);
                }
            }
        }
    }
    n
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Report {
    pub before: u64,
    pub entries: usize,
    pub evicted: usize,
    pub evicted_bytes: u64,
    pub purged: usize,
    pub manifests: usize,
    pub broken: usize,
    pub foreign: usize,
}

pub fn gc(
    cache: &Cache,
    cap: Option<u64>,
    older: Option<Duration>,
    dry: bool,
) -> Result<Option<Report>> {
    cache.checked()?;
    let lock_path = cache.root.join("gc.lock");
    if lmeta(&lock_path).is_some_and(|m| !m.is_file()) {
        bail!("{} is not a plain file", lock_path.display());
    }
    let lock = std::fs::File::options()
        .create(true)
        .truncate(false)
        .write(true)
        .open(&lock_path)
        .map_err(|e| anyhow::anyhow!("opening {}: {e}", lock_path.display()))?;
    if lock.try_lock().is_err() {
        return Ok(None);
    }
    let now = SystemTime::now();
    let s = scan(&cache.root);
    let victims = plan(&s.entries, cap, older, now);
    let mut r = Report {
        before: s.bytes,
        entries: s.entries.len(),
        broken: s.broken.len(),
        foreign: s.foreign.len(),
        ..Report::default()
    };
    for v in &victims {
        if dry || evict(&cache.root, v, now) {
            r.evicted += 1;
            r.evicted_bytes += v.bytes;
        }
    }
    for b in &s.broken {
        let name = b
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default();
        let ours = tmp_key(&name).is_some() || is_hex(&name, 64);
        if ours && !dry && real_dir(b) && mtime(b).is_some_and(|t| age(t, now) > TMP_STALE) {
            let _ = std::fs::remove_dir_all(b);
        }
    }
    r.purged = purge_trash(&cache.root, GRACE, now, dry).0;
    if !dry && real_file(&cache.root.join("grown")) {
        let _ = std::fs::remove_file(cache.root.join("grown"));
    }
    r.manifests = prune_manifests(cache, dry);
    Ok(Some(r))
}

pub fn gc_if_due(cache: &Cache) {
    let grown = lmeta(&cache.root.join("grown")).map_or(0, |m| m.len());
    if grown >= GC_EVERY && cache.cap.is_some() {
        let _ = gc(cache, cache.cap, None, false);
    }
}

pub fn verify(cache: &Cache) -> Result<Vec<(String, String)>> {
    cache.checked()?;
    let mut bad = Vec::new();
    for e in scan(&cache.root).entries {
        let Some(sums) = Cache::part_sums(&e.dir) else {
            bad.push((e.key, "its OK record is unreadable".to_string()));
            continue;
        };
        for (name, want) in sums {
            let part = e.dir.join(&name);
            let got = real_file(&part)
                .then(|| crate::cache::sha_file(&part))
                .flatten();
            if got.as_deref() != Some(want.as_str()) {
                bad.push((e.key.clone(), format!("{name} does not hash to its record")));
            }
        }
    }
    Ok(bad)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cache::Outputs;

    struct Scratch(PathBuf);

    impl Drop for Scratch {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn scratch(tag: &str) -> Scratch {
        let d = std::env::temp_dir().join(format!("msvc6t-gc-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        Scratch(d)
    }

    fn key(c: char) -> String {
        std::iter::repeat_n(c, 64).collect()
    }

    #[test]
    fn the_least_recently_used_go_first_down_to_the_low_water_mark() {
        let t0 = SystemTime::UNIX_EPOCH + Duration::from_secs(1_000_000);
        let e = |k: &str, b: u64, s: u64| Entry {
            key: k.into(),
            dir: PathBuf::from(k),
            bytes: b,
            used: t0 + Duration::from_secs(s),
        };
        let v = vec![e("new", 40, 30), e("old", 40, 10), e("mid", 40, 20)];
        let p = plan(&v, Some(50), None, t0 + Duration::from_secs(40));
        assert_eq!(
            p.iter().map(|e| e.key.as_str()).collect::<Vec<_>>(),
            ["old", "mid"]
        );
        assert!(plan(&v, Some(1000), None, t0).is_empty());
        assert_eq!(plan(&v, Some(100), None, t0).len(), 1);
        let p = plan(
            &v,
            None,
            Some(Duration::from_secs(25)),
            t0 + Duration::from_secs(40),
        );
        assert_eq!(p.len(), 1);
    }

    #[test]
    fn gc_evicts_by_rename_and_verify_finds_a_bad_part() {
        let s = scratch("evict");
        let c = Cache::new(s.0.join("c")).unwrap();
        let outs = Outputs {
            obj: s.0.join("x.obj"),
            cod: None,
        };
        msvc6_base::srcio::write_bytes(&outs.obj, &[7u8; 1000]).unwrap();
        c.store(&key('a'), &outs, "").unwrap();
        c.store(&key('b'), &outs, "").unwrap();
        assert_eq!(scan(&c.root).entries.len(), 2);
        assert!(verify(&c).unwrap().is_empty());
        msvc6_base::srcio::write_bytes(c.entry(&key('b')).join("obj"), b"x").unwrap();
        assert_eq!(verify(&c).unwrap().len(), 1);
        let r = gc(&c, Some(1000), None, false).unwrap().unwrap();
        assert_eq!((r.entries, r.evicted), (2, 1));
        assert_eq!(scan(&c.root).entries.len(), 1);
        assert_eq!(
            purge_trash(&c.root, Duration::ZERO, SystemTime::now(), true).0,
            1
        );
        assert_eq!(
            purge_trash(&c.root, Duration::ZERO, SystemTime::now(), false).0,
            1
        );
    }

    #[test]
    fn gc_never_follows_a_link_out_of_the_cache_or_deletes_a_name_it_did_not_write() {
        let s = scratch("links");
        let c = Cache::new(s.0.join("c")).unwrap();
        let outs = Outputs {
            obj: s.0.join("x.obj"),
            cod: None,
        };
        msvc6_base::srcio::write_bytes(&outs.obj, b"o").unwrap();
        c.store(&key('c'), &outs, "").unwrap();
        let victim1 = s.0.join("victim1");
        std::fs::create_dir_all(victim1.join("photos-2024")).unwrap();
        msvc6_base::srcio::write_bytes(victim1.join("keepme"), b"k").unwrap();
        std::os::unix::fs::symlink(&victim1, c.root.join("t")).unwrap();
        let victim2 = s.0.join("victim2");
        std::fs::create_dir_all(victim2.join(key('d'))).unwrap();
        std::os::unix::fs::symlink(&victim2, c.root.join("o/dd")).unwrap();
        std::fs::create_dir_all(c.root.join("m")).unwrap();
        std::os::unix::fs::symlink(&victim2, c.root.join("m/ee")).unwrap();
        let r = gc(&c, Some(1), Some(Duration::ZERO), false)
            .unwrap()
            .unwrap();
        assert!(victim1.join("keepme").is_file() && victim1.join("photos-2024").is_dir());
        assert!(victim2.join(key('d')).is_dir());
        assert_eq!(r.evicted, 0, "t/ is a link: {r:?}");
        std::fs::remove_file(c.root.join("t")).unwrap();
        std::fs::create_dir_all(c.root.join("t/notes")).unwrap();
        std::fs::create_dir_all(c.root.join(format!("t/{}-xyz", key('e')))).unwrap();
        gc(&c, None, Some(Duration::ZERO), false).unwrap();
        assert!(c.root.join("t/notes").is_dir());
        assert!(c.root.join(format!("t/{}-xyz", key('e'))).is_dir());
    }

    #[test]
    fn a_cache_root_that_is_not_a_cache_is_refused() {
        let s = scratch("roots");
        let home = std::env::var("HOME").unwrap();
        for bad in ["", "/", "relative/dir", home.as_str()] {
            assert!(Cache::new(PathBuf::from(bad)).is_err(), "{bad:?}");
        }
        assert!(Cache::new(Path::new(&home).parent().unwrap().to_path_buf()).is_err());
        let plain = s.0.join("plain");
        std::fs::create_dir_all(&plain).unwrap();
        msvc6_base::srcio::write_bytes(plain.join("thesis.tex"), b"t").unwrap();
        let e = Cache::new(plain.clone()).unwrap_err().to_string();
        assert!(e.contains("not a msvc6t compile cache"), "{e}");
        let fresh = Cache::new(s.0.join("fresh")).unwrap();
        let outs = Outputs {
            obj: s.0.join("y.obj"),
            cod: None,
        };
        msvc6_base::srcio::write_bytes(&outs.obj, b"o").unwrap();
        fresh.store(&key('f'), &outs, "").unwrap();
        assert!(fresh.root.join(crate::cache::MARKER).is_file());
        assert!(Cache::new(fresh.root.clone()).is_ok());
        assert!(fresh.store("not-a-key", &outs, "").is_err());
    }
}
