use crate::config::{Config, RunnerKind, RunnerSpec, parse_duration, parse_size};
use crate::exec::{self, Cmd, Output};
use crate::layout::{Toolchain, to_win};
use crate::prefix::{Lease, PrefixPool};
use crate::slots::{Token, Tokens};
use anyhow::{Context as _, Result, bail};
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

pub const DEFAULT_JOB_MEMORY: u64 = 1 << 30;
pub const DEFAULT_FLOOR: u64 = 4 << 30;

#[derive(Debug, Clone)]
pub struct Runner {
    pub kind: RunnerKind,
    pub command: Vec<String>,
    pub docker: String,
    pub container: Option<String>,
    pub winepath: Vec<String>,
    pub display: Option<crate::display::Display>,
    pub pool: Option<PrefixPool>,
    pub msvcrt: Option<PathBuf>,
    pub msvcrt_md5: Option<String>,
    pub tokens: Tokens,
    pub jobs: usize,
    pub job_memory: u64,
    pub floor: u64,
    pub timeout: Option<Duration>,
    pub watchdog: Option<Vec<OsString>>,
}

fn home() -> PathBuf {
    PathBuf::from(std::env::var_os("HOME").unwrap_or_default())
}

impl Runner {
    pub fn from_config(cfg: &Config) -> Result<Self> {
        let r: &RunnerSpec = &cfg.runner;
        let path = |s: &Option<String>, dflt: PathBuf| -> Result<PathBuf> {
            s.as_deref().map_or(Ok(dflt), |v| cfg.path(v))
        };
        let pool = match r.kind {
            RunnerKind::Native => None,
            _ => Some(PrefixPool::new(
                path(&r.prefixes, home().join(".cache/msvc6t/prefixes"))?,
                path(&r.template, home().join(".wine"))?,
            )),
        };
        let jobs = std::env::var("MSVC6T_JOBS")
            .ok()
            .and_then(|v| v.trim().parse::<usize>().ok())
            .filter(|n| *n > 0)
            .or(r.jobs)
            .unwrap_or_else(crate::slots::default_jobs)
            .max(1);
        let container = match (r.kind, &r.container) {
            (RunnerKind::Container, None) => {
                bail!("[runner] kind = \"container\" needs `container`, the container's name")
            }
            (_, c) => c.clone(),
        };
        Ok(Self {
            kind: r.kind,
            command: if r.command.is_empty() {
                vec!["wine".to_string()]
            } else {
                r.command
                    .iter()
                    .map(|c| cfg.expand(c))
                    .collect::<Result<_>>()?
            },
            docker: r
                .docker
                .as_deref()
                .map_or_else(|| Ok("docker".to_string()), |d| cfg.expand(d))?,
            container,
            winepath: r
                .winepath
                .iter()
                .map(|w| cfg.expand(w))
                .collect::<Result<_>>()?,
            display: match &r.display {
                None => None,
                Some(d) => {
                    let d = d.spec();
                    Some(crate::display::Display {
                        name: d.name.clone(),
                        start: d
                            .start
                            .iter()
                            .map(|s| cfg.expand(s))
                            .collect::<Result<_>>()?,
                        sockets: d
                            .sockets
                            .as_deref()
                            .map_or_else(|| Ok(PathBuf::from("/tmp/.X11-unix")), |s| cfg.path(s))?,
                        locks: d
                            .locks
                            .as_deref()
                            .map_or_else(|| Ok(PathBuf::from("/tmp")), |s| cfg.path(s))?,
                        state: path(&r.tokens, std::env::temp_dir().join("msvc6t-jobs"))?,
                        wait: crate::display::WAIT,
                    })
                }
            },
            pool,
            msvcrt: r
                .native_msvcrt
                .as_deref()
                .map(|m| cfg.path(m))
                .transpose()?,
            msvcrt_md5: r.msvcrt_md5.clone(),
            tokens: Tokens::new(
                path(&r.tokens, std::env::temp_dir().join("msvc6t-jobs"))?,
                jobs,
            ),
            jobs,
            job_memory: r
                .job_memory
                .as_deref()
                .map(parse_size)
                .transpose()?
                .unwrap_or(DEFAULT_JOB_MEMORY),
            floor: DEFAULT_FLOOR,
            timeout: r.timeout.as_deref().map(parse_duration).transpose()?,
            watchdog: None,
        })
    }

    #[must_use]
    pub fn with_watchdog(mut self, argv: Vec<OsString>) -> Self {
        self.watchdog = Some(argv);
        self
    }

    pub fn batch(&self, want: usize) -> Result<usize> {
        crate::slots::plan(
            want,
            self.jobs,
            self.job_memory,
            self.floor,
            crate::slots::memory(),
        )
    }

    fn start_watchdog(&self) {
        static HELD: OnceLock<Mutex<Option<std::process::ChildStdin>>> = OnceLock::new();
        let Some(argv) = &self.watchdog else {
            return;
        };
        HELD.get_or_init(|| {
            let started = argv.split_first().and_then(|(p, rest)| {
                exec::spawn_detached(Path::new(p), rest)
                    .map_err(|e| eprintln!("note: the owner watchdog did not start: {e:#}"))
                    .ok()
            });
            Mutex::new(started)
        });
    }

    pub fn session(&self) -> Result<Session<'_>> {
        let token = self.tokens.acquire().ok();
        let lease = match &self.pool {
            None => None,
            Some(p) => {
                self.start_watchdog();
                let l = p.lease()?;
                if let Some(m) = &self.msvcrt {
                    crate::prefix::install_pinned(
                        &l.path,
                        "msvcrt.dll",
                        m,
                        self.msvcrt_md5.as_deref(),
                    )?;
                }
                Some(l)
            }
        };
        Ok(Session {
            runner: self,
            lease,
            _token: token,
        })
    }

    pub fn display_up(&self) -> bool {
        self.display
            .as_ref()
            .is_none_or(crate::display::Display::up)
    }

    pub fn ensure_display(&self) -> Result<()> {
        match &self.display {
            None => Ok(()),
            Some(d) => d.ensure(),
        }
    }
}

pub fn tool_env(tc: &Toolchain, extra_winepath: &[String]) -> Vec<(String, String)> {
    let mut wp = vec![to_win(&tc.bin)];
    if let Some(c) = &tc.common {
        wp.push(to_win(c));
    }
    wp.extend(extra_winepath.iter().cloned());
    let mut env = vec![("WINEPATH".to_string(), wp.join(";"))];
    if let Some(l) = &tc.lib {
        env.push(("LIB".to_string(), to_win(l)));
    }
    if let Some(i) = &tc.include {
        env.push(("INCLUDE".to_string(), to_win(i)));
    }
    env
}

#[derive(Debug)]
pub struct Session<'r> {
    pub runner: &'r Runner,
    pub lease: Option<Lease>,
    _token: Option<Token>,
}

impl Session<'_> {
    pub fn prefix(&self) -> Option<&Path> {
        self.lease.as_ref().map(|l| l.path.as_path())
    }

    pub fn drive_c(&self) -> Option<PathBuf> {
        self.lease.as_ref().map(Lease::drive_c)
    }

    pub fn command(
        &self,
        tool: &Path,
        args: &[String],
        cwd: Option<&Path>,
        env: &[(String, String)],
    ) -> Result<Cmd> {
        let r = self.runner;
        let mut vars: Vec<(String, String)> = Vec::new();
        if let Some(p) = self.prefix() {
            vars.push(("WINEPREFIX".into(), p.display().to_string()));
            vars.push((
                "WINEDEBUG".into(),
                std::env::var("WINEDEBUG").unwrap_or_else(|_| "-all".into()),
            ));
            if r.msvcrt.is_some() {
                vars.push(("WINEDLLOVERRIDES".into(), "msvcrt=n".into()));
            }
            if let Some(d) = &r.display {
                vars.push(("DISPLAY".into(), d.name.clone()));
            }
        }
        for (k, v) in env {
            vars.retain(|(x, _)| x != k);
            vars.push((k.clone(), v.clone()));
        }
        let cwd_path = cwd
            .map(Path::to_path_buf)
            .or_else(|| std::env::current_dir().ok())
            .unwrap_or_else(|| PathBuf::from("/"));
        Ok(match r.kind {
            RunnerKind::Native => {
                let mut c = Cmd::new(tool).args(args).cwd(cwd_path);
                c.env = vars;
                c
            }
            RunnerKind::Wine => {
                let (prog, rest) = r
                    .command
                    .split_first()
                    .context("[runner] command is empty")?;
                let mut c = Cmd::new(prog)
                    .args(rest)
                    .arg(tool.as_os_str())
                    .args(args)
                    .cwd(cwd_path);
                c.env = vars;
                c
            }
            RunnerKind::Container => {
                use std::os::unix::fs::MetadataExt as _;
                let me = std::fs::metadata("/proc/self").context("reading /proc/self")?;
                let mut c = Cmd::new(&r.docker).args(["exec", "-i", "-u"]).arg(format!(
                    "{}:{}",
                    me.uid(),
                    me.gid()
                ));
                vars.push(("HOME".into(), home().display().to_string()));
                for (k, v) in &vars {
                    c = c.arg("-e").arg(format!("{k}={v}"));
                }
                c.arg("-w")
                    .arg(cwd_path.as_os_str())
                    .arg(r.container.clone().unwrap_or_default())
                    .arg("wine")
                    .arg(tool.as_os_str())
                    .args(args)
            }
        })
    }

    pub fn run(
        &self,
        tool: &Path,
        args: &[String],
        cwd: Option<&Path>,
        env: &[(String, String)],
        timeout: Option<Duration>,
    ) -> Result<Output> {
        let c = self
            .command(tool, args, cwd, env)?
            .timeout(timeout.or(self.runner.timeout));
        let mut tick = None;
        let out = exec::run_observed(&c, &mut |pid| {
            tick = self.lease.as_ref().and_then(|l| l.record_job(pid));
        })?;
        if (out.timed_out || out.status.is_none())
            && let (Some(l), Some(since)) = (&self.lease, tick)
        {
            let killed = l.reap_job(since);
            if !killed.is_empty() {
                eprintln!(
                    "note: killed {} process(es) the interrupted run left in {}",
                    killed.len(),
                    l.path.display()
                );
            }
        }
        Ok(out)
    }

    pub fn run_tool(
        &self,
        tc: &Toolchain,
        exe: &str,
        args: &[String],
        cwd: Option<&Path>,
        extra: &[(String, String)],
    ) -> Result<Output> {
        let tool = tc.tool(exe)?;
        let mut env = tool_env(tc, &self.runner.winepath);
        for (k, v) in extra {
            env.retain(|(x, _)| x != k);
            env.push((k.clone(), v.clone()));
        }
        let out = self.run(&tool, args, cwd, &env, None)?;
        if out.timed_out {
            bail!("{exe} timed out\n{}", out.text().trim());
        }
        if out.status != Some(0) {
            bail!(
                "{exe} exited {} in {}\n{}",
                out.status
                    .map_or_else(|| "on a signal".to_string(), |s| s.to_string()),
                cwd.map_or_else(|| ".".to_string(), |c| c.display().to_string()),
                out.text().trim()
            );
        }
        Ok(out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cfg(kind: &str) -> Config {
        crate::config::test_config(
            &format!(
                "[runner]\nkind = \"{kind}\"\ncontainer = \"wc\"\nprefixes = \"/tmp/p\"\ndisplay = {{ name = \":7\" }}\n\
                 native_msvcrt = \"/x/msvcrt.dll\"\njobs = 3\n"
            ),
            None,
        )
    }

    #[test]
    fn a_container_run_passes_its_environment_by_name() {
        let r = Runner::from_config(&cfg("container")).unwrap();
        assert_eq!(r.jobs, 3);
        let s = Session {
            runner: &r,
            lease: None,
            _token: None,
        };
        let c = s
            .command(
                Path::new("/t/CL.EXE"),
                &["/c".into()],
                Some(Path::new("/w")),
                &[("INCLUDE".into(), "C:\\\\x".into())],
            )
            .unwrap();
        let line = c.display();
        assert!(line.starts_with("docker exec -i -u "), "{line}");
        assert!(line.contains("-e INCLUDE=C:\\\\x"), "{line}");
        assert!(line.ends_with("-w /w wc wine /t/CL.EXE /c"), "{line}");
    }

    #[test]
    fn a_wine_run_puts_the_prefix_and_the_runtime_override_in_the_environment() {
        let r = Runner::from_config(&cfg("wine")).unwrap();
        assert_eq!(r.command, vec!["wine".to_string()]);
        assert_eq!(r.pool.as_ref().unwrap().root, PathBuf::from("/tmp/p"));
        let s = Session {
            runner: &r,
            lease: None,
            _token: None,
        };
        let c = s.command(Path::new("/t/LINK.EXE"), &[], None, &[]).unwrap();
        assert_eq!(c.display(), "wine /t/LINK.EXE");
        assert!(
            Runner::from_config(&crate::config::test_config(
                "[runner]\nkind = \"container\"\n",
                None
            ))
            .is_err()
        );
    }
}
