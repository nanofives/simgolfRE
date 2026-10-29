use anyhow::{Context as _, Result};
use std::ffi::OsString;
use std::io::Write as _;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::Duration;

#[derive(Debug, Clone, Default)]
pub struct Cmd {
    pub program: PathBuf,
    pub args: Vec<OsString>,
    pub cwd: Option<PathBuf>,
    pub env: Vec<(String, String)>,
    pub env_remove: Vec<String>,
    pub timeout: Option<Duration>,
    pub stdin: Option<Vec<u8>>,
}

impl Cmd {
    pub fn new(program: impl Into<PathBuf>) -> Self {
        Self {
            program: program.into(),
            ..Self::default()
        }
    }

    #[must_use]
    pub fn arg(mut self, a: impl Into<OsString>) -> Self {
        self.args.push(a.into());
        self
    }

    #[must_use]
    pub fn args<I, S>(mut self, it: I) -> Self
    where
        I: IntoIterator<Item = S>,
        S: Into<OsString>,
    {
        self.args.extend(it.into_iter().map(Into::into));
        self
    }

    #[must_use]
    pub fn cwd(mut self, d: impl Into<PathBuf>) -> Self {
        self.cwd = Some(d.into());
        self
    }

    #[must_use]
    pub fn env(mut self, k: &str, v: impl Into<String>) -> Self {
        self.env.push((k.to_string(), v.into()));
        self
    }

    #[must_use]
    pub fn timeout(mut self, t: Option<Duration>) -> Self {
        self.timeout = t;
        self
    }

    pub fn display(&self) -> String {
        std::iter::once(self.program.as_os_str())
            .chain(self.args.iter().map(OsString::as_os_str))
            .map(|a| a.to_string_lossy().into_owned())
            .collect::<Vec<_>>()
            .join(" ")
    }
}

#[derive(Debug, Clone, Default)]
pub struct Output {
    pub status: Option<i32>,
    pub stdout: Vec<u8>,
    pub stderr: Vec<u8>,
    pub timed_out: bool,
    pub pid: u32,
}

impl Output {
    pub fn ok(&self) -> bool {
        self.status == Some(0) && !self.timed_out
    }

    pub fn text(&self) -> String {
        let mut t = msvc6_base::srcio::tool_text(&self.stdout);
        t.push_str(&msvc6_base::srcio::tool_text(&self.stderr));
        t
    }
}

pub const DRAIN_GRACE: Duration = Duration::from_secs(2);

#[derive(Debug)]
struct Drain {
    buf: std::sync::Arc<std::sync::Mutex<Vec<u8>>>,
    done: std::sync::mpsc::Receiver<()>,
}

fn drain(r: Option<impl std::io::Read + Send + 'static>) -> Drain {
    let buf = std::sync::Arc::new(std::sync::Mutex::new(Vec::new()));
    let (tx, done) = std::sync::mpsc::channel();
    let fill = std::sync::Arc::clone(&buf);
    std::thread::spawn(move || {
        if let Some(mut r) = r {
            let mut chunk = vec![0u8; 1 << 16];
            loop {
                match r.read(&mut chunk) {
                    Ok(0) => break,
                    Ok(n) => {
                        if let (Ok(mut b), Some(got)) = (fill.lock(), chunk.get(..n)) {
                            b.extend_from_slice(got);
                        }
                    }
                    Err(e) if e.kind() == std::io::ErrorKind::Interrupted => {}
                    Err(_) => break,
                }
            }
        }
        let _ = tx.send(());
    });
    Drain { buf, done }
}

fn collected(d: &Drain, until: std::time::Instant) -> Vec<u8> {
    let _ = d
        .done
        .recv_timeout(until.saturating_duration_since(std::time::Instant::now()));
    d.buf
        .lock()
        .map(|mut b| std::mem::take(&mut *b))
        .unwrap_or_default()
}

pub fn run(c: &Cmd) -> Result<Output> {
    run_observed(c, &mut |_| {})
}

pub fn run_observed(c: &Cmd, on_spawn: &mut dyn FnMut(u32)) -> Result<Output> {
    let mut cmd = Command::new(&c.program);
    cmd.args(&c.args);
    if let Some(d) = &c.cwd {
        cmd.current_dir(d);
    }
    for k in &c.env_remove {
        cmd.env_remove(k);
    }
    for (k, v) in &c.env {
        cmd.env(k, v);
    }
    cmd.stdin(if c.stdin.is_some() {
        Stdio::piped()
    } else {
        Stdio::null()
    })
    .stdout(Stdio::piped())
    .stderr(Stdio::piped());
    let mut child = cmd
        .spawn()
        .with_context(|| format!("spawning {}", c.program.display()))?;
    let pid = child.id();
    on_spawn(pid);
    if let (Some(bytes), Some(mut w)) = (c.stdin.clone(), child.stdin.take()) {
        std::thread::spawn(move || {
            let _ = w.write_all(&bytes);
        });
    }
    let out = drain(child.stdout.take());
    let err = drain(child.stderr.take());
    let timed_out = match c.timeout {
        None => false,
        Some(limit) => {
            let Some(fd) = crate::ident::pidfd(pid) else {
                let _ = child.kill();
                let _ = child.wait();
                anyhow::bail!(
                    "no pidfd for {} (pid {pid}): a bounded run needs Linux 5.3 or later",
                    c.program.display()
                );
            };
            let exited = crate::ident::wait_exited(&fd, limit)?;
            if !exited {
                let _ = child.kill();
            }
            !exited
        }
    };
    let status = child
        .wait()
        .with_context(|| format!("waiting for {}", c.program.display()))?;
    let until = std::time::Instant::now() + DRAIN_GRACE;
    Ok(Output {
        status: status.code(),
        stdout: collected(&out, until),
        stderr: collected(&err, until),
        timed_out,
        pid,
    })
}

pub fn run_ok(c: &Cmd) -> Result<Output> {
    let o = run(c)?;
    if o.timed_out {
        anyhow::bail!(
            "{} timed out after {} s\n{}",
            c.display(),
            c.timeout.map_or(0, |t| t.as_secs()),
            o.text().trim()
        );
    }
    if o.status != Some(0) {
        anyhow::bail!(
            "{} exited {}\n{}",
            c.display(),
            o.status
                .map_or_else(|| "on a signal".to_string(), |s| s.to_string()),
            o.text().trim()
        );
    }
    Ok(o)
}

pub fn spawn_detached(program: &Path, args: &[OsString]) -> Result<std::process::ChildStdin> {
    use std::os::unix::process::CommandExt as _;
    let mut child = Command::new(program)
        .args(args)
        .stdin(Stdio::piped())
        .stdout(Stdio::null())
        .stderr(Stdio::inherit())
        .process_group(0)
        .spawn()
        .with_context(|| format!("starting {}", program.display()))?;
    child
        .stdin
        .take()
        .context("the detached child has no stdin pipe")
}

pub fn spawn_background(c: &Cmd) -> Result<u32> {
    use std::os::unix::process::CommandExt as _;
    let mut cmd = Command::new(&c.program);
    cmd.args(&c.args)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .process_group(0);
    for (k, v) in &c.env {
        cmd.env(k, v);
    }
    let child = cmd
        .spawn()
        .with_context(|| format!("starting {}", c.display()))?;
    Ok(child.id())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn output_and_status_come_back_and_a_timeout_kills() {
        let o = run(&Cmd::new("echo").arg("a")).unwrap();
        assert!(o.ok());
        assert_eq!(o.text(), "a\n");
        let o = run(&Cmd::new("ls").arg("/msvc6t-no-such-path")).unwrap();
        assert_eq!(o.status, Some(2));
        assert!(!o.stderr.is_empty());
        let t = std::time::Instant::now();
        let o = run(&Cmd::new("tail")
            .args(["-f", "/dev/null"])
            .timeout(Some(Duration::from_millis(300))))
        .unwrap();
        assert!(o.timed_out);
        assert!(t.elapsed() < Duration::from_secs(20));
        let o = run(&Cmd {
            stdin: Some(b"in".to_vec()),
            ..Cmd::new("cat")
        })
        .unwrap();
        assert_eq!(o.stdout, b"in");
        assert!(run_ok(&Cmd::new("false")).is_err());
    }

    #[test]
    fn a_child_that_closes_its_pipes_and_runs_on_is_still_killed_at_the_timeout() {
        let t = std::time::Instant::now();
        let o = run(&Cmd::new("sh")
            .args(["-c", "exec 1>&- 2>&-; exec tail -f /dev/null"])
            .timeout(Some(Duration::from_millis(300))))
        .unwrap();
        assert!(o.timed_out);
        assert!(o.stdout.is_empty() && o.stderr.is_empty());
        assert!(t.elapsed() < Duration::from_secs(20), "{:?}", t.elapsed());
    }

    #[test]
    fn a_grandchild_that_holds_the_pipes_does_not_hold_the_run() {
        let d = std::env::temp_dir().join(format!("msvc6t-exec-grandchild-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        let pidfile = d.join("pid");
        let script = format!(
            "tail -f /dev/null & echo $! > '{}'; echo a",
            pidfile.display()
        );
        let (tx, rx) = std::sync::mpsc::channel();
        std::thread::spawn(move || {
            let _ = tx.send(run(&Cmd::new("sh").args(["-c", script.as_str()])));
        });
        let got = rx.recv_timeout(Duration::from_secs(20));
        let grandchild = msvc6_base::srcio::own_read(&pidfile).unwrap();
        let killed = Command::new("kill")
            .arg(grandchild.trim())
            .status()
            .unwrap();
        std::fs::remove_dir_all(&d).unwrap();
        assert!(killed.success());
        let o = got
            .expect("the run waited on a grandchild that holds its pipes")
            .unwrap();
        assert_eq!(o.status, Some(0));
        assert_eq!(o.text(), "a\n");
    }
}
