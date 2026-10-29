use anyhow::{Context as _, Result};
use rustix::event::{PollFd, PollFlags, Timespec};
use rustix::fd::OwnedFd;
use rustix::process::{Pid, PidfdFlags, Signal};
use std::path::Path;
use std::time::{Duration, Instant};

pub fn pidfd(pid: u32) -> Option<OwnedFd> {
    let raw = i32::try_from(pid).ok()?;
    let p = Pid::from_raw(raw)?;
    rustix::process::pidfd_open(p, PidfdFlags::empty()).ok()
}

pub fn wait_exited(fd: &OwnedFd, limit: Duration) -> Result<bool> {
    let deadline = Instant::now() + limit;
    loop {
        let left = deadline.saturating_duration_since(Instant::now());
        let ts = Timespec {
            tv_sec: i64::try_from(left.as_secs()).unwrap_or(i64::MAX),
            tv_nsec: i64::from(left.subsec_nanos()),
        };
        let mut fds = [PollFd::new(fd, PollFlags::IN)];
        match rustix::event::poll(&mut fds, Some(&ts)) {
            Ok(0) => return Ok(false),
            Ok(_) => return Ok(true),
            Err(rustix::io::Errno::INTR) => {}
            Err(e) => return Err(std::io::Error::from(e)).context("waiting on a pidfd"),
        }
    }
}

pub fn refusal(pid: u32) -> Option<String> {
    let me = std::process::id();
    if pid <= 1 {
        return Some("0 is this process group and 1 is init".into());
    }
    if pid == me || Path::new(&format!("/proc/self/task/{pid}")).exists() {
        return Some("it is this process".into());
    }
    if let Some(a) = crate::procscan::ancestry(me, 64)
        .into_iter()
        .find(|a| a.pid == pid)
    {
        return Some(format!("it is this process's ancestor ({})", a.comm));
    }
    None
}

pub fn kill_verified(pid: u32, start: u64) -> bool {
    if let Some(why) = refusal(pid) {
        eprintln!("msvc6t: NOT killing pid {pid}: {why}");
        return false;
    }
    let Some(fd) = pidfd(pid) else {
        return false;
    };
    if crate::procscan::start_time(pid) != Some(start) {
        return false;
    }
    rustix::process::pidfd_send_signal(&fd, Signal::KILL).is_ok()
}

pub fn descends_from(pid: u32, ancestor: u32) -> bool {
    crate::procscan::ancestry(pid, 256)
        .iter()
        .any(|a| a.pid == ancestor)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_kill_needs_the_recorded_start_and_never_reaches_this_process() {
        let mut child = std::process::Command::new("tail")
            .args(["-f", "/dev/null"])
            .spawn()
            .unwrap();
        let pid = child.id();
        let start = crate::procscan::start_time(pid).unwrap();
        assert!(
            !kill_verified(pid, start + 1),
            "a wrong start time is another process"
        );
        assert!(crate::procscan::alive(pid));
        assert!(descends_from(pid, std::process::id()));
        assert!(kill_verified(pid, start));
        assert!(!child.wait().unwrap().success());
        assert!(refusal(std::process::id()).is_some());
        assert!(refusal(std::os::unix::process::parent_id()).is_some());
        let me = crate::procscan::start_time(std::process::id()).unwrap();
        assert!(!kill_verified(std::process::id(), me));
    }

    #[test]
    fn a_pidfd_wait_returns_on_exit_or_at_the_limit() {
        let mut quick = std::process::Command::new("true").spawn().unwrap();
        let fd = pidfd(quick.id()).unwrap();
        assert!(wait_exited(&fd, Duration::from_secs(20)).unwrap());
        quick.wait().unwrap();
        let mut slow = std::process::Command::new("tail")
            .args(["-f", "/dev/null"])
            .spawn()
            .unwrap();
        let fd = pidfd(slow.id()).unwrap();
        let t = Instant::now();
        assert!(!wait_exited(&fd, Duration::from_millis(200)).unwrap());
        assert!(t.elapsed() >= Duration::from_millis(200));
        slow.kill().unwrap();
        slow.wait().unwrap();
    }
}
