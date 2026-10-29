use crate::exec::{self, Cmd};
use anyhow::{Context as _, Result, bail};
use rustix::event::{PollFd, PollFlags, Timespec};
use rustix::fs::inotify;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

pub const WAIT: Duration = Duration::from_secs(30);

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Display {
    pub name: String,
    pub start: Vec<String>,
    pub sockets: PathBuf,
    pub locks: PathBuf,
    pub state: PathBuf,
    pub wait: Duration,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Owner {
    pub pid: u32,
    pub start: u64,
    pub lock: Option<u32>,
}

impl Display {
    pub fn number(&self) -> &str {
        self.name.trim_start_matches(':')
    }

    pub fn socket(&self) -> PathBuf {
        self.sockets.join(format!("X{}", self.number()))
    }

    pub fn lockfile(&self) -> PathBuf {
        self.locks.join(format!(".X{}-lock", self.number()))
    }

    pub fn owner_file(&self) -> PathBuf {
        self.state.join(format!("display-{}.owner", self.number()))
    }

    pub fn up(&self) -> bool {
        std::os::unix::net::UnixStream::connect(self.socket()).is_ok()
    }

    pub fn recorded_owner(&self) -> Option<Owner> {
        let text = msvc6_base::srcio::own_read(self.owner_file()).ok()?;
        let mut f = text.split_whitespace();
        Some(Owner {
            pid: f.next()?.parse().ok()?,
            start: f.next()?.parse().ok()?,
            lock: f.next().and_then(|l| l.parse().ok()),
        })
    }

    pub fn lock_pid(&self) -> Option<u32> {
        std::fs::read(self.lockfile())
            .ok()
            .and_then(|b| msvc6_base::srcio::decode(&b).trim().parse().ok())
    }

    fn lock_is_ours(&self, owner: &Owner, names: &str) -> Result<()> {
        if !exists(&self.lockfile()) {
            return Ok(());
        }
        let Some(l) = self.lock_pid() else {
            bail!(
                "display {} does not answer and {} names no pid: {names} are left in place",
                self.name,
                self.lockfile().display()
            );
        };
        if owner.lock != Some(l) {
            bail!(
                "display {} does not answer; {} names pid {l}, and the server this tool started wrote {}: \
                 {names} are left in place",
                self.name,
                self.lockfile().display(),
                owner.lock.map_or_else(
                    || "no lock this tool recorded".to_string(),
                    |p| format!("pid {p}")
                )
            );
        }
        let live = crate::procscan::innermost(l);
        if let Some(p) = live.first() {
            bail!(
                "display {} does not answer; {} names pid {l}, which a live process (pid {p} here) holds, \
                 not the server this tool started: {names} are left in place",
                self.name,
                self.lockfile().display()
            );
        }
        Ok(())
    }

    fn stale_files(&self) -> Vec<PathBuf> {
        [self.socket(), self.lockfile()]
            .into_iter()
            .filter(|p| std::fs::symlink_metadata(p).is_ok())
            .collect()
    }

    fn clear_stale(&self) -> Result<()> {
        let stale = self.stale_files();
        if stale.is_empty() {
            return Ok(());
        }
        let names = stale
            .iter()
            .map(|p| p.display().to_string())
            .collect::<Vec<_>>()
            .join(" and ");
        match self.recorded_owner() {
            Some(o) if crate::procscan::start_time(o.pid) != Some(o.start) => {
                self.lock_is_ours(&o, &names)?;
                let pid = o.pid;
                for p in &stale {
                    std::fs::remove_file(p).with_context(|| {
                        format!(
                            "removing {} left by the display server this tool started (pid {pid}), now gone",
                            p.display()
                        )
                    })?;
                }
                let _ = std::fs::remove_file(self.owner_file());
                Ok(())
            }
            Some(o) => bail!(
                "display {} does not answer, but the server this tool started (pid {}) still runs; \
                 {names} are its",
                self.name,
                o.pid
            ),
            None => bail!(
                "display {} does not answer and {names} exist, from a server this tool did not start: \
                 remove them as their owner, or name another display",
                self.name
            ),
        }
    }

    fn wait_for_socket(&self) -> Result<()> {
        let fd = inotify::init(inotify::CreateFlags::CLOEXEC | inotify::CreateFlags::NONBLOCK)
            .context("inotify_init")?;
        std::fs::create_dir_all(&self.sockets).ok();
        inotify::add_watch(
            &fd,
            &self.sockets,
            inotify::WatchFlags::CREATE | inotify::WatchFlags::MOVED_TO,
        )
        .with_context(|| format!("watching {}", self.sockets.display()))?;
        let deadline = Instant::now() + self.wait;
        let mut buf = [std::mem::MaybeUninit::<u8>::uninit(); 4096];
        loop {
            if std::fs::symlink_metadata(self.socket()).is_ok() {
                return Ok(());
            }
            let left = deadline.saturating_duration_since(Instant::now());
            if left.is_zero() {
                bail!(
                    "display {}: no {} within {} s of `{}`",
                    self.name,
                    self.socket().display(),
                    self.wait.as_secs(),
                    self.start.join(" ")
                );
            }
            let ts = Timespec {
                tv_sec: i64::try_from(left.as_secs()).unwrap_or(i64::MAX),
                tv_nsec: i64::from(left.subsec_nanos()),
            };
            let mut fds = [PollFd::new(&fd, PollFlags::IN)];
            match rustix::event::poll(&mut fds, Some(&ts)) {
                Ok(_) | Err(rustix::io::Errno::INTR) => {}
                Err(e) => return Err(std::io::Error::from(e)).context("waiting on inotify"),
            }
            let mut r = inotify::Reader::new(&fd, &mut buf);
            while r.next().is_ok() {}
        }
    }

    fn record_server(&self, since: u64) {
        let found = crate::procscan::pids().into_iter().find_map(|pid| {
            let argv = crate::procscan::cmdline(pid);
            let server = argv
                .first()
                .is_some_and(|a| a.ends_with("Xvfb") || a.ends_with("Xorg"));
            let start = crate::procscan::start_time(pid)?;
            (server && argv.iter().any(|a| a == &self.name) && start >= since)
                .then_some((pid, start))
        });
        if let Some((pid, start)) = found {
            let lock = self
                .lock_pid()
                .filter(|l| crate::procscan::ns_pids(pid).last() == Some(l))
                .map(|l| format!(" {l}"))
                .unwrap_or_default();
            let _ = std::fs::create_dir_all(&self.state);
            let _ =
                msvc6_base::srcio::own_write(self.owner_file(), format!("{pid} {start}{lock}\n"));
        }
    }

    pub fn ensure(&self) -> Result<()> {
        if self.up() {
            return Ok(());
        }
        self.clear_stale()?;
        let Some((prog, rest)) = self.start.split_first() else {
            bail!(
                "display {} is not up and [runner.display] names no `start` command",
                self.name
            );
        };
        let mut since = None;
        let out = exec::run_observed(
            &Cmd::new(prog)
                .args(rest)
                .timeout(Some(Duration::from_secs(60))),
            &mut |pid| since = crate::procscan::start_time(pid),
        )?;
        if !out.ok() {
            bail!(
                "starting display {}: `{}` failed\n{}",
                self.name,
                self.start.join(" "),
                out.text().trim()
            );
        }
        self.wait_for_socket()?;
        if let Some(s) = since {
            self.record_server(s);
        }
        Ok(())
    }
}

pub fn exists(p: &Path) -> bool {
    std::fs::symlink_metadata(p).is_ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Scratch(PathBuf);

    impl Drop for Scratch {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn display(d: &Path, n: &str, start: &[&str]) -> Display {
        Display {
            name: format!(":{n}"),
            start: start.iter().map(|s| (*s).to_string()).collect(),
            sockets: d.join("x11"),
            locks: d.to_path_buf(),
            state: d.join("state"),
            wait: Duration::from_secs(10),
        }
    }

    fn scratch(tag: &str) -> Scratch {
        let d = std::env::temp_dir().join(format!("msvc6t-display-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(d.join("x11")).unwrap();
        Scratch(d)
    }

    #[test]
    fn a_stale_socket_and_lock_this_tool_did_not_create_are_left_in_place() {
        let s = scratch("stale");
        let mut dp = display(&s.0, "77", &["true"]);
        dp.wait = Duration::from_secs(1);
        msvc6_base::srcio::write_bytes(dp.socket(), b"").unwrap();
        msvc6_base::srcio::write_bytes(dp.lockfile(), b"      123\n").unwrap();
        let e = dp.ensure().unwrap_err().to_string();
        assert!(e.contains("did not start"), "{e}");
        assert!(exists(&dp.socket()) && exists(&dp.lockfile()));
        std::fs::create_dir_all(&dp.state).unwrap();
        msvc6_base::srcio::own_write(dp.owner_file(), "4000000 1\n").unwrap();
        let e = dp.ensure().unwrap_err().to_string();
        assert!(e.contains("no lock this tool recorded"), "{e}");
        assert!(exists(&dp.socket()) && exists(&dp.lockfile()));
        let mut live = std::process::Command::new("cat")
            .stdin(std::process::Stdio::piped())
            .spawn()
            .unwrap();
        let pid = live.id();
        msvc6_base::srcio::write_bytes(dp.lockfile(), format!("{pid:>10}\n").as_bytes()).unwrap();
        msvc6_base::srcio::own_write(dp.owner_file(), format!("4000000 1 {pid}\n")).unwrap();
        let e = dp.ensure().unwrap_err().to_string();
        drop(live.stdin.take());
        live.wait().unwrap();
        assert!(e.contains("a live process"), "{e}");
        assert!(exists(&dp.socket()) && exists(&dp.lockfile()));
        let gone: u32 =
            msvc6_base::srcio::decode(&std::fs::read("/proc/sys/kernel/pid_max").unwrap())
                .trim()
                .parse()
                .unwrap();
        msvc6_base::srcio::write_bytes(dp.lockfile(), format!("{gone:>10}\n").as_bytes()).unwrap();
        msvc6_base::srcio::own_write(dp.owner_file(), format!("4000000 1 {gone}\n")).unwrap();
        let e = dp.ensure().unwrap_err().to_string();
        assert!(!exists(&dp.socket()) && !exists(&dp.lockfile()), "{e}");
    }

    #[test]
    fn the_wait_returns_when_the_socket_appears() {
        let s = scratch("wait");
        let dp = display(&s.0, "78", &["true"]);
        let sock = dp.socket();
        let t = Instant::now();
        let maker = std::thread::spawn(move || {
            let (_tx, rx) = std::sync::mpsc::channel::<()>();
            let _ = rx.recv_timeout(Duration::from_millis(300));
            std::os::unix::net::UnixListener::bind(&sock).unwrap()
        });
        dp.ensure().unwrap();
        let listener = maker.join().unwrap();
        assert!(dp.up());
        assert!(t.elapsed() < Duration::from_secs(10));
        drop(listener);
    }
}
