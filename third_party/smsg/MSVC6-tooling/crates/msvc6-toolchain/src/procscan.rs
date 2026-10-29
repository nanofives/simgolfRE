use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Proc {
    pub pid: u32,
    pub comm: String,
    pub start: Option<u64>,
}

pub fn pids() -> Vec<u32> {
    std::fs::read_dir("/proc")
        .map(|rd| {
            rd.flatten()
                .filter_map(|e| e.file_name().to_str()?.parse::<u32>().ok())
                .collect()
        })
        .unwrap_or_default()
}

fn stat_text(pid: u32) -> Option<String> {
    std::fs::read(format!("/proc/{pid}/stat"))
        .ok()
        .map(|b| msvc6_base::srcio::decode(&b))
}

pub fn stat_field(text: &str, n: usize) -> Option<&str> {
    let close = text.rfind(')')?;
    text.get(close + 1..)?
        .split_whitespace()
        .nth(n.checked_sub(3)?)
}

pub fn comm(pid: u32) -> String {
    std::fs::read(format!("/proc/{pid}/comm"))
        .map(|b| msvc6_base::srcio::decode(&b).trim().to_string())
        .unwrap_or_default()
}

pub fn ppid(pid: u32) -> Option<u32> {
    stat_field(&stat_text(pid)?, 4)?.parse().ok()
}

pub fn start_time(pid: u32) -> Option<u64> {
    stat_field(&stat_text(pid)?, 22)?.parse().ok()
}

pub fn alive(pid: u32) -> bool {
    Path::new(&format!("/proc/{pid}")).exists()
}

pub fn ns_pids(pid: u32) -> Vec<u32> {
    std::fs::read(format!("/proc/{pid}/status"))
        .map(|b| {
            msvc6_base::srcio::decode(&b)
                .lines()
                .find_map(|l| l.strip_prefix("NSpid:"))
                .map(|r| {
                    r.split_whitespace()
                        .filter_map(|x| x.parse().ok())
                        .collect()
                })
                .unwrap_or_default()
        })
        .unwrap_or_default()
}

pub fn innermost(n: u32) -> Vec<u32> {
    pids()
        .into_iter()
        .filter(|&p| ns_pids(p).last() == Some(&n))
        .collect()
}

pub fn environ_get(block: &[u8], key: &str) -> Option<String> {
    block.split(|&b| b == 0).find_map(|kv| {
        let kv = msvc6_base::srcio::decode(kv);
        let (k, v) = kv.split_once('=')?;
        (k == key).then(|| v.to_string())
    })
}

pub fn environ(pid: u32) -> Option<Vec<u8>> {
    std::fs::read(format!("/proc/{pid}/environ")).ok()
}

pub fn cmdline(pid: u32) -> Vec<String> {
    std::fs::read(format!("/proc/{pid}/cmdline"))
        .map(|b| {
            b.split(|&c| c == 0)
                .filter(|a| !a.is_empty())
                .map(msvc6_base::srcio::decode)
                .collect()
        })
        .unwrap_or_default()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Slotted {
    pub pid: u32,
    pub comm: String,
    pub start: u64,
}

pub fn in_slot(slot: &Path) -> Vec<Slotted> {
    let want = slot.as_os_str().as_encoded_bytes();
    pids()
        .into_iter()
        .filter(|&p| p != std::process::id())
        .filter_map(|pid| {
            let env = environ(pid)?;
            let wp = env
                .split(|&b| b == 0)
                .find_map(|kv| kv.strip_prefix(b"WINEPREFIX="))?;
            if wp != want {
                return None;
            }
            Some(Slotted {
                pid,
                comm: comm(pid),
                start: start_time(pid)?,
            })
        })
        .collect()
}

pub fn ancestry(pid: u32, limit: usize) -> Vec<Proc> {
    let mut out = Vec::new();
    let mut at = ppid(pid);
    while let Some(p) = at {
        if p == 0 || out.len() >= limit {
            break;
        }
        out.push(Proc {
            pid: p,
            comm: comm(p),
            start: start_time(p),
        });
        if p == 1 {
            break;
        }
        at = ppid(p);
    }
    out
}

pub fn with_cmdline(pred: impl Fn(&[String]) -> bool) -> Vec<u32> {
    pids().into_iter().filter(|&p| pred(&cmdline(p))).collect()
}

pub fn self_cgroup_memory_dir() -> Option<PathBuf> {
    let text = std::fs::read("/proc/self/cgroup").ok()?;
    let text = msvc6_base::srcio::decode(&text);
    for l in text.lines() {
        let mut parts = l.splitn(3, ':');
        let (_, ctl, path) = (parts.next()?, parts.next()?, parts.next()?);
        if ctl.split(',').any(|c| c == "memory") {
            let d = PathBuf::from("/sys/fs/cgroup/memory").join(path.trim_start_matches('/'));
            if d.is_dir() {
                return Some(d);
            }
        }
        if ctl.is_empty() {
            let d = PathBuf::from("/sys/fs/cgroup").join(path.trim_start_matches('/'));
            if d.join("memory.max").is_file() {
                return Some(d);
            }
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_stat_line_is_split_after_the_command_name() {
        let s = "42 (a b) c) S 7 1 1 0 -1 4194560 1 0 0 0 0 0 0 0 20 0 1 0 999 0";
        assert_eq!(stat_field(s, 3), Some("S"));
        assert_eq!(stat_field(s, 4), Some("7"));
        assert_eq!(stat_field(s, 22), Some("999"));
        assert_eq!(
            ppid(std::process::id()),
            Some(std::os::unix::process::parent_id())
        );
        assert!(alive(std::process::id()));
        assert_eq!(
            ns_pids(std::process::id()).last(),
            Some(&std::process::id())
        );
        assert!(innermost(std::process::id()).contains(&std::process::id()));
    }

    #[test]
    fn the_environment_block_is_read_by_key() {
        let b = b"A=1\0WINEPREFIX=/tmp/wp3\0B=x=y\0";
        assert_eq!(environ_get(b, "WINEPREFIX").as_deref(), Some("/tmp/wp3"));
        assert_eq!(environ_get(b, "B").as_deref(), Some("x=y"));
        assert_eq!(environ_get(b, "C"), None);
    }
}
