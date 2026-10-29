use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use std::io::Write as _;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

pub fn run(repo: &Path, args: &[&str]) -> Result<Vec<u8>> {
    let out = Command::new("git")
        .arg("-C")
        .arg(repo)
        .args(args)
        .env_remove("GIT_DIR")
        .env_remove("GIT_WORK_TREE")
        .output()
        .with_context(|| format!("running git {} in {}", args.join(" "), repo.display()))?;
    if !out.status.success() {
        bail!(
            "git {} in {} failed: {}",
            args.join(" "),
            repo.display(),
            srcio::tool_text(&out.stderr).trim()
        );
    }
    Ok(out.stdout)
}

pub fn rev(repo: &Path, spec: &str) -> Result<String> {
    let out = run(
        repo,
        &["rev-parse", "--verify", &format!("{spec}^{{commit}}")],
    )?;
    Ok(srcio::tool_text(&out).trim().to_string())
}

pub fn range(repo: &Path, spec: &str) -> Result<(String, String)> {
    match spec.split_once("..") {
        Some((a, b)) => Ok((rev(repo, a)?, rev(repo, b)?)),
        None => Ok((rev(repo, &format!("{spec}^"))?, rev(repo, spec)?)),
    }
}

pub fn subject(repo: &Path, rev: &str) -> Result<String> {
    let out = run(repo, &["log", "-1", "--format=%s", rev])?;
    Ok(srcio::tool_text(&out).trim().to_string())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Change {
    pub status: char,
    pub path: String,
}

pub fn changes(repo: &Path, a: &str, b: &str) -> Result<Vec<Change>> {
    let out = run(repo, &["diff", "--name-status", "-z", "--no-renames", a, b])?;
    let mut it = out.split(|&c| c == 0).filter(|s| !s.is_empty());
    let mut v = Vec::new();
    while let (Some(st), Some(p)) = (it.next(), it.next()) {
        v.push(Change {
            status: st.first().map_or('?', |&c| char::from(c)),
            path: srcio::decode(p),
        });
    }
    Ok(v)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    pub mode: String,
    pub kind: String,
    pub id: String,
    pub path: String,
}

pub fn ls_tree(repo: &Path, rev: &str) -> Result<Vec<Entry>> {
    let out = run(repo, &["ls-tree", "-r", "-z", "--full-tree", rev])?;
    let mut v = Vec::new();
    for rec in out.split(|&c| c == 0).filter(|s| !s.is_empty()) {
        let Some(tab) = rec.iter().position(|&c| c == b'\t') else {
            continue;
        };
        let head = srcio::decode(rec.get(..tab).unwrap_or_default());
        let path = srcio::decode(rec.get(tab + 1..).unwrap_or_default());
        let f: Vec<&str> = head.split(' ').collect();
        if let [mode, kind, id] = f.as_slice() {
            v.push(Entry {
                mode: (*mode).to_string(),
                kind: (*kind).to_string(),
                id: (*id).to_string(),
                path,
            });
        }
    }
    Ok(v)
}

pub fn blobs(repo: &Path, ids: &[String]) -> Result<Vec<Vec<u8>>> {
    let mut child = Command::new("git")
        .arg("-C")
        .arg(repo)
        .args(["cat-file", "--batch"])
        .env_remove("GIT_DIR")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .context("running git cat-file --batch")?;
    let mut stdin = child.stdin.take().context("git cat-file has no stdin")?;
    let input: Vec<u8> = ids
        .iter()
        .flat_map(|i| format!("{i}\n").into_bytes())
        .collect();
    let writer = std::thread::spawn(move || stdin.write_all(&input));
    let out = child.wait_with_output().context("reading git cat-file")?;
    writer
        .join()
        .map_err(|_| anyhow::anyhow!("the git cat-file writer panicked"))?
        .context("writing to git cat-file")?;
    if !out.status.success() {
        bail!(
            "git cat-file failed: {}",
            srcio::tool_text(&out.stderr).trim()
        );
    }
    let d = out.stdout;
    let mut v = Vec::with_capacity(ids.len());
    let mut at = 0usize;
    for id in ids {
        let nl = d
            .get(at..)
            .and_then(|r| r.iter().position(|&c| c == b'\n'))
            .context("git cat-file output ends early")?;
        let head = srcio::decode(d.get(at..at + nl).unwrap_or_default());
        let size: usize = head
            .rsplit(' ')
            .next()
            .and_then(|s| s.parse().ok())
            .with_context(|| format!("git cat-file: `{head}` for {id}"))?;
        let body = at + nl + 1;
        v.push(d.get(body..body + size).context("short blob")?.to_vec());
        at = body + size + 1;
    }
    Ok(v)
}

pub fn file_at(repo: &Path, rev: &str, path: &str) -> Result<Option<Vec<u8>>> {
    let out = Command::new("git")
        .arg("-C")
        .arg(repo)
        .args(["cat-file", "blob", &format!("{rev}:{path}")])
        .env_remove("GIT_DIR")
        .output()
        .context("running git cat-file blob")?;
    Ok(out.status.success().then_some(out.stdout))
}

pub fn file_at_bounded(repo: &Path, rev: &str, path: &str, max: u64) -> Result<Option<Vec<u8>>> {
    let Ok(id) = run(repo, &["rev-parse", "--verify", &format!("{rev}:{path}")]) else {
        return Ok(None);
    };
    let id = srcio::tool_text(&id).trim().to_string();
    if srcio::tool_text(&run(repo, &["cat-file", "-t", &id])?).trim() != "blob" {
        return Ok(None);
    }
    let size: u64 = srcio::tool_text(&run(repo, &["cat-file", "-s", &id])?)
        .trim()
        .parse()
        .with_context(|| format!("reading the size of {rev}:{path}"))?;
    if size > max {
        bail!("{rev}:{path} is {size} bytes, more than the {max} it is read to");
    }
    run(repo, &["cat-file", "blob", &id]).map(Some)
}

pub fn materialize(repo: &Path, rev: &str, dest: &Path) -> Result<Vec<PathBuf>> {
    let entries: Vec<Entry> = ls_tree(repo, rev)?
        .into_iter()
        .filter(|e| e.kind == "blob")
        .collect();
    let ids: Vec<String> = entries.iter().map(|e| e.id.clone()).collect();
    let data = blobs(repo, &ids)?;
    let mut written = Vec::with_capacity(entries.len());
    for (e, bytes) in entries.iter().zip(data) {
        if !crate::tree::safe_rel(&e.path) {
            bail!("{rev} holds `{}`, a path that leaves the tree", e.path);
        }
        let to = dest.join(&e.path);
        if let Some(p) = to.parent() {
            std::fs::create_dir_all(p).with_context(|| format!("creating {}", p.display()))?;
        }
        if to.symlink_metadata().is_ok() {
            std::fs::remove_file(&to)?;
        }
        srcio::write_bytes(&to, &bytes)?;
        {
            #[cfg(unix)]
            if e.mode == "100755" {
                use std::os::unix::fs::PermissionsExt as _;
                std::fs::set_permissions(&to, std::fs::Permissions::from_mode(0o755))?;
            }
        }
        written.push(PathBuf::from(&e.path));
    }
    Ok(written)
}

pub fn is_repo(dir: &Path) -> bool {
    dir.join(".git").exists()
}

pub fn listed(root: &Path) -> Result<Vec<PathBuf>> {
    let out = run(
        root,
        &[
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
        ],
    )?;
    let mut v: Vec<PathBuf> = out
        .split(|&c| c == 0)
        .filter(|s| !s.is_empty())
        .map(|p| PathBuf::from(srcio::decode(p)))
        .filter(|p| root.join(p).is_file())
        .collect();
    v.sort();
    v.dedup();
    Ok(v)
}

pub fn submodules(root: &Path) -> Vec<String> {
    let Ok(text) = srcio::read(root.join(".gitmodules")) else {
        return Vec::new();
    };
    text.lines()
        .filter_map(|l| {
            let (k, v) = l.trim().split_once('=')?;
            (k.trim() == "path").then(|| v.trim().to_string())
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6-source-git-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn git(dir: &Path, args: &[&str]) {
        let st = Command::new("git")
            .arg("-C")
            .arg(dir)
            .args([
                "-c",
                "user.name=t",
                "-c",
                "user.email=t@t",
                "-c",
                "commit.gpgsign=false",
            ])
            .args(args)
            .output()
            .unwrap();
        assert!(st.status.success(), "{}", srcio::tool_text(&st.stderr));
    }

    #[test]
    fn a_file_at_a_revision_is_read_only_within_its_bound() {
        let d = scratch("bounded");
        let repo = d.join("repo");
        std::fs::create_dir_all(repo.join("t")).unwrap();
        git(&repo, &["init", "-q"]);
        srcio::write_bytes(repo.join("t/big.tsv"), &[b'x'; 100]).unwrap();
        git(&repo, &["add", "."]);
        git(&repo, &["commit", "-q", "-m", "one"]);
        let e = file_at_bounded(&repo, "HEAD", "t/big.tsv", 99).unwrap_err();
        assert!(
            e.to_string()
                .contains("HEAD:t/big.tsv is 100 bytes, more than the 99"),
            "{e:#}"
        );
        assert_eq!(
            file_at_bounded(&repo, "HEAD", "t/big.tsv", 100)
                .unwrap()
                .map(|b| b.len()),
            Some(100)
        );
        assert_eq!(
            file_at_bounded(&repo, "HEAD", "t/none.tsv", 100).unwrap(),
            None
        );
        assert_eq!(file_at_bounded(&repo, "HEAD", "t", 100).unwrap(), None);
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[test]
    fn a_revision_materializes_byte_for_byte_and_changes_are_listed() {
        let d = scratch("mat");
        let repo = d.join("repo");
        std::fs::create_dir_all(repo.join("src")).unwrap();
        git(&repo, &["init", "-q"]);
        srcio::write_bytes(repo.join("src/a.cpp"), b"int a; /* \xa7 */\r\n").unwrap();
        srcio::write_bytes(repo.join("b.h"), b"int b;\n").unwrap();
        git(&repo, &["add", "."]);
        git(&repo, &["commit", "-q", "-m", "one"]);
        srcio::write_bytes(repo.join("src/a.cpp"), b"int a;\r\n").unwrap();
        std::fs::remove_file(repo.join("b.h")).unwrap();
        git(&repo, &["add", "-A"]);
        git(&repo, &["commit", "-q", "-m", "two"]);
        let (a, b) = range(&repo, "HEAD").unwrap();
        assert_eq!(subject(&repo, &b).unwrap(), "two");
        let ch = changes(&repo, &a, &b).unwrap();
        assert_eq!(
            ch,
            vec![
                Change {
                    status: 'D',
                    path: "b.h".into()
                },
                Change {
                    status: 'M',
                    path: "src/a.cpp".into()
                }
            ]
        );
        let out = d.join("tree");
        let files = materialize(&repo, &a, &out).unwrap();
        assert_eq!(files.len(), 2);
        assert_eq!(
            std::fs::read(out.join("src/a.cpp")).unwrap(),
            b"int a; /* \xa7 */\r\n"
        );
        assert_eq!(
            file_at(&repo, &b, "src/a.cpp").unwrap().unwrap(),
            b"int a;\r\n"
        );
        assert_eq!(file_at(&repo, &b, "b.h").unwrap(), None);
        assert_eq!(listed(&repo).unwrap(), vec![PathBuf::from("src/a.cpp")]);
        std::fs::remove_dir_all(&d).unwrap();
    }
}
