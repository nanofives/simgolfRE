use crate::cache::{
    Cache, LAYOUT, Manifest, Outcome, Outputs, bypasses, dir_listing, key_args, key_of,
    non_text_reason, normalize_preprocessed, sha_file,
};
use crate::layout::{Toolchain, resolve_ci};
use crate::runner::Session;
use anyhow::{Context as _, Result, bail};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct Job<'a> {
    pub tc: &'a Toolchain,
    pub dir: PathBuf,
    pub args: Vec<String>,
    pub outs: Outputs,
    pub env: Vec<(String, String)>,
    pub receipt: bool,
}

#[derive(Debug, Clone)]
pub struct Compiled {
    pub text: String,
    pub outcome: Outcome,
    pub manifest: Option<Manifest>,
}

pub fn source_arg(args: &[String]) -> Option<&String> {
    args.iter()
        .rev()
        .find(|a| !(a.starts_with('/') || a.starts_with('-')))
}

pub fn host_path(p: &str, dir: &Path, drive_c: Option<&Path>) -> Option<PathBuf> {
    let unix = p.replace('\\', "/");
    let lower = unix.to_ascii_lowercase();
    if let Some(rest) = lower
        .strip_prefix("z:")
        .map(|_| unix.get(2..).unwrap_or_default())
    {
        return Some(PathBuf::from(rest));
    }
    if lower.starts_with("c:") {
        let rest = unix.get(2..).unwrap_or_default().trim_start_matches('/');
        return drive_c.map(|c| c.join(rest));
    }
    if unix.as_bytes().get(1) == Some(&b':') {
        return None;
    }
    Some(dir.join(unix))
}

fn env_value<'e>(env: &'e [(String, String)], k: &str) -> Option<&'e str> {
    env.iter()
        .rev()
        .find(|(x, _)| x == k)
        .map(|(_, v)| v.as_str())
}

fn search_dirs(job: &Job<'_>, env: &[(String, String)], drive_c: Option<&Path>) -> Vec<PathBuf> {
    let mut v: Vec<PathBuf> = job
        .args
        .iter()
        .filter_map(|a| a.strip_prefix("/I").or_else(|| a.strip_prefix("-I")))
        .filter_map(|d| host_path(d.trim_matches('"'), &job.dir, drive_c))
        .collect();
    if let Some(inc) = env_value(env, "INCLUDE") {
        v.extend(
            inc.split(';')
                .filter(|s| !s.is_empty())
                .filter_map(|s| host_path(s, &job.dir, drive_c)),
        );
    }
    v.push(job.dir.clone());
    v.into_iter().filter_map(|d| resolve_ci(&d)).collect()
}

fn preprocess_args(args: &[String]) -> Vec<String> {
    let mut v: Vec<String> = args
        .iter()
        .filter(|a| {
            let l = a.to_ascii_lowercase();
            !(l == "/c" || l.starts_with("/fo") || l.starts_with("/fa") || l.starts_with("/fd"))
        })
        .cloned()
        .collect();
    let at = v.len().saturating_sub(1);
    v.insert(at, "/P".to_string());
    v
}

fn manifest_of(
    job: &Job<'_>,
    env: &[(String, String)],
    drive_c: Option<&Path>,
    paths: &[String],
) -> Option<Manifest> {
    let mut files = Vec::new();
    for p in paths {
        let h = host_path(p, &job.dir, drive_c).and_then(|h| resolve_ci(&h))?;
        let sum = sha_file(&h)?;
        if !files.iter().any(|(q, _)| q == &h) {
            files.push((h, sum));
        }
    }
    let dirs = search_dirs(job, env, drive_c)
        .into_iter()
        .map(|d| {
            let l = dir_listing(&d);
            (d, l)
        })
        .collect();
    Some(Manifest {
        key: String::new(),
        files,
        dirs,
    })
}

fn full_env(sess: &Session<'_>, job: &Job<'_>) -> Vec<(String, String)> {
    let mut env = crate::runner::tool_env(job.tc, &sess.runner.winepath);
    for (k, v) in &job.env {
        env.retain(|(x, _)| x != k);
        env.push((k.clone(), v.clone()));
    }
    env
}

#[must_use]
pub fn creates_pch(args: &[String]) -> bool {
    args.iter().any(|a| {
        let l = a.to_ascii_lowercase();
        l.starts_with("/yx") || l.starts_with("-yx")
    })
}

pub fn clear_pch(dir: &Path) -> usize {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return 0;
    };
    rd.flatten()
        .map(|e| e.path())
        .filter(|p| {
            p.extension().is_some_and(|x| x.eq_ignore_ascii_case("pch"))
                && std::fs::symlink_metadata(p).is_ok_and(|m| m.is_file())
        })
        .filter(|p| std::fs::remove_file(p).is_ok())
        .count()
}

pub fn compile(sess: &Session<'_>, cache: Option<&Cache>, job: &Job<'_>) -> Result<Compiled> {
    let src_name = source_arg(&job.args)
        .context("the CL arguments name no source file")?
        .clone();
    let src = job.dir.join(&src_name);
    let src_bytes = std::fs::read(&src).with_context(|| format!("reading {}", src.display()))?;
    if let Some(why) = non_text_reason(&src_bytes) {
        bail!("REFUSING TO COMPILE {}: {why}", src.display());
    }
    let env = full_env(sess, job);
    let drive_c = sess.drive_c();
    let run_cl = || -> Result<String> {
        if creates_pch(&job.args) {
            clear_pch(&job.dir);
        }
        let out = sess.run_tool(job.tc, "CL.EXE", &job.args, Some(&job.dir), &job.env)?;
        unchanged(&src, &src_bytes)?;
        Ok(out.text())
    };
    let cache = match cache {
        Some(c) if c.enabled && !job.args.iter().any(|a| bypasses(a)) => c,
        _ => {
            let text = run_cl()?;
            write_receipt(job, &src, &src_bytes, &env, None)?;
            return Ok(Compiled {
                text,
                outcome: Outcome::Bypass,
                manifest: None,
            });
        }
    };
    let ident = job.tc.identity();
    let env_text: String = env
        .iter()
        .filter(|(k, _)| k != "WINEPATH")
        .map(|(k, v)| format!("{k}={v}\n"))
        .collect();
    let roles = format!("cod={}", job.outs.cod.is_some());
    let mkey = key_of(&[
        b"direct",
        LAYOUT.as_bytes(),
        ident.as_bytes(),
        env_text.as_bytes(),
        job.args.join("\n").as_bytes(),
        &src_bytes,
        roles.as_bytes(),
        job.dir.as_os_str().as_encoded_bytes(),
    ]);
    if let Some(m) = cache.read_manifest(&mkey)
        && m.changed().is_none()
        && let Some(text) = cache.restore(&m.key, &job.outs)
    {
        write_receipt(job, &src, &src_bytes, &env, Some(&m))?;
        return Ok(Compiled {
            text,
            outcome: Outcome::HitDirect,
            manifest: Some(m),
        });
    }
    let since = std::time::SystemTime::now();
    let pre = sess.run_tool(
        job.tc,
        "CL.EXE",
        &preprocess_args(&job.args),
        Some(&job.dir),
        &job.env,
    );
    let stem = Path::new(&src_name)
        .file_stem()
        .map(|s| s.to_string_lossy().into_owned())
        .unwrap_or_default();
    let ifile = resolve_ci(&job.dir.join(format!("{stem}.i")));
    let pre_text = match (pre, ifile.as_ref().and_then(|p| std::fs::read(p).ok())) {
        (Ok(_), Some(t)) => t,
        _ => {
            let text = run_cl()?;
            write_receipt(job, &src, &src_bytes, &env, None)?;
            return Ok(Compiled {
                text,
                outcome: Outcome::Bypass,
                manifest: None,
            });
        }
    };
    if let Some(i) = &ifile {
        let _ = std::fs::remove_file(i);
    }
    let (norm, paths) = normalize_preprocessed(&pre_text);
    let Some(mut m) = manifest_of(job, &env, drive_c.as_deref(), &paths) else {
        let text = run_cl()?;
        write_receipt(job, &src, &src_bytes, &env, None)?;
        return Ok(Compiled {
            text,
            outcome: Outcome::Bypass,
            manifest: None,
        });
    };
    let raw: String = if job.outs.cod.is_some() {
        m.files.iter().map(|(_, h)| format!("{h}\n")).collect()
    } else {
        String::new()
    };
    let text_key = key_of(&[
        LAYOUT.as_bytes(),
        ident.as_bytes(),
        key_args(&job.args).join("\n").as_bytes(),
        &norm,
        roles.as_bytes(),
        raw.as_bytes(),
    ]);
    m.key = key_of(&[b"entry", text_key.as_bytes(), m.reads().as_bytes()]);
    if let Some(text) = cache.restore(&m.key, &job.outs) {
        cache.save_manifest(&mkey, &m);
        write_receipt(job, &src, &src_bytes, &env, Some(&m))?;
        return Ok(Compiled {
            text,
            outcome: Outcome::Hit,
            manifest: Some(m),
        });
    }
    let _lock = cache.lock(&m.key);
    if let Some(text) = cache.restore(&m.key, &job.outs) {
        cache.save_manifest(&mkey, &m);
        write_receipt(job, &src, &src_bytes, &env, Some(&m))?;
        return Ok(Compiled {
            text,
            outcome: Outcome::Hit,
            manifest: Some(m),
        });
    }
    let text = run_cl()?;
    if let Some(why) = m.changed().or_else(|| m.touched_since(since)) {
        eprintln!(
            "note: compile cache: nothing stored for {}: {why}",
            src.display()
        );
        write_receipt(job, &src, &src_bytes, &env, None)?;
        return Ok(Compiled {
            text,
            outcome: Outcome::Bypass,
            manifest: None,
        });
    }
    let stored = cache.store(&m.key, &job.outs, &text)?;
    if stored {
        cache.save_manifest(&mkey, &m);
        crate::gc::gc_if_due(cache);
    }
    write_receipt(job, &src, &src_bytes, &env, Some(&m))?;
    Ok(Compiled {
        text,
        outcome: if stored {
            Outcome::Stored
        } else {
            Outcome::Bypass
        },
        manifest: Some(m),
    })
}

fn unchanged(src: &Path, before: &[u8]) -> Result<()> {
    let now = std::fs::read(src).with_context(|| format!("re-reading {}", src.display()))?;
    if now != before {
        bail!(
            "{} changed while it compiled: its object is not this text's, and nothing was stored",
            src.display()
        );
    }
    Ok(())
}

fn write_receipt(
    job: &Job<'_>,
    src: &Path,
    src_bytes: &[u8],
    env: &[(String, String)],
    m: Option<&Manifest>,
) -> Result<()> {
    if !job.receipt {
        return Ok(());
    }
    let r = crate::receipt::Receipt {
        source: src.to_path_buf(),
        source_sha: msvc6_base::digest::sha256_hex(src_bytes),
        args: job.args.clone(),
        env: env
            .iter()
            .filter(|(k, _)| k != "WINEPATH")
            .cloned()
            .collect(),
        toolchain: job.tc.identity(),
        reads: m.cloned(),
        obj_sha: sha_file(&job.outs.obj).unwrap_or_default(),
        cod_sha: job.outs.cod.as_deref().and_then(sha_file),
    };
    r.write(&job.outs.obj)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn paths_cl_prints_map_back_to_the_host() {
        let d = Path::new("/w");
        let c = Path::new("/p/drive_c");
        assert_eq!(
            host_path("Z:\\a\\b.h", d, None),
            Some(PathBuf::from("/a/b.h"))
        );
        assert_eq!(
            host_path("C:\\x\\y.h", d, Some(c)),
            Some(PathBuf::from("/p/drive_c/x/y.h"))
        );
        assert_eq!(host_path("C:\\x\\y.h", d, None), None);
        assert_eq!(host_path("D:\\x", d, Some(c)), None);
        assert_eq!(
            host_path("inc\\q.h", d, None),
            Some(PathBuf::from("/w/inc/q.h"))
        );
        let a: Vec<String> = ["/c", "/O2", "/Fohello.obj", "hello.c"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        assert_eq!(preprocess_args(&a), vec!["/O2", "/P", "hello.c"]);
        assert_eq!(source_arg(&a).map(String::as_str), Some("hello.c"));
    }

    #[test]
    fn a_yx_compile_creates_its_pch_so_every_pch_in_its_directory_goes_first() {
        let args = |v: &[&str]| -> Vec<String> { v.iter().map(|s| (*s).to_string()).collect() };
        assert!(creates_pch(&args(&["/c", "/YX", "a.cpp"])));
        assert!(creates_pch(&args(&["/c", "-yxstdafx.h", "a.cpp"])));
        assert!(!creates_pch(&args(&[
            "/c", "/Yupch.h", "/Fpa.pch", "a.cpp"
        ])));
        let d = std::env::temp_dir().join(format!("msvc6t-clear-pch-{}", std::process::id()));
        std::fs::create_dir_all(d.join("sub.pch")).unwrap();
        for f in ["vc60.pch", "OTHER.PCH", "vc60.idb", "a.obj"] {
            msvc6_base::srcio::write_bytes(d.join(f), b"x").unwrap();
        }
        assert_eq!(clear_pch(&d), 2);
        assert!(!d.join("vc60.pch").exists() && !d.join("OTHER.PCH").exists());
        assert!(d.join("vc60.idb").exists() && d.join("a.obj").exists());
        assert!(d.join("sub.pch").is_dir());
        assert_eq!(clear_pch(&d.join("absent")), 0);
        std::fs::remove_dir_all(&d).ok();
    }
}
