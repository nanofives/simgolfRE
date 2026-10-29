use crate::config::{Config, StageSpec};
use crate::confine::{Root, dos_rel, rel_path};
use anyhow::{Context as _, Result, bail};
use std::collections::BTreeSet;
use std::os::unix::fs::MetadataExt as _;
use std::path::{Path, PathBuf};

pub const MANIFEST_MAGIC: &str = "msvc6t-staged 1";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Entry {
    Link { rel: PathBuf, target: PathBuf },
    Dir { rel: PathBuf, dev: u64, ino: u64 },
    File { rel: PathBuf, dev: u64, ino: u64 },
}

impl Entry {
    fn render(&self) -> String {
        match self {
            Self::Link { rel, target } => {
                format!("link\t{}\t{}\n", rel.display(), target.display())
            }
            Self::Dir { rel, dev, ino } => format!("dir\t{}\t{dev}\t{ino}\n", rel.display()),
            Self::File { rel, dev, ino } => format!("file\t{}\t{dev}\t{ino}\n", rel.display()),
        }
    }

    fn parse(line: &str) -> Option<Self> {
        let f: Vec<&str> = line.split('\t').collect();
        let rel = |s: &str| rel_path(s).ok();
        match f.as_slice() {
            ["link", r, t] => Some(Self::Link {
                rel: rel(r)?,
                target: PathBuf::from(t),
            }),
            ["dir", r, d, i] => Some(Self::Dir {
                rel: rel(r)?,
                dev: d.parse().ok()?,
                ino: i.parse().ok()?,
            }),
            ["file", r, d, i] => Some(Self::File {
                rel: rel(r)?,
                dev: d.parse().ok()?,
                ino: i.parse().ok()?,
            }),
            _ => None,
        }
    }
}

fn plain_file(p: &Path) -> bool {
    std::fs::symlink_metadata(p).is_ok_and(|m| m.is_file())
}

pub fn read_manifest(p: &Path) -> Result<Vec<Entry>> {
    match std::fs::symlink_metadata(p) {
        Err(_) => return Ok(Vec::new()),
        Ok(m) if !m.is_file() => bail!(
            "{} is not a plain file: refusing it as a staging manifest",
            p.display()
        ),
        Ok(_) => {}
    }
    let text =
        msvc6_base::srcio::own_read(p).with_context(|| format!("reading {}", p.display()))?;
    let mut lines = text.lines();
    if lines.next() != Some(MANIFEST_MAGIC) {
        bail!("{} is not a staging manifest this tool wrote", p.display());
    }
    lines
        .filter(|l| !l.is_empty())
        .map(|l| Entry::parse(l).with_context(|| format!("{}: unreadable row `{l}`", p.display())))
        .collect()
}

fn write_manifest(p: &Path, entries: &[Entry]) -> Result<()> {
    if std::fs::symlink_metadata(p).is_ok_and(|m| !m.is_file()) {
        bail!("{} is not a plain file", p.display());
    }
    let mut text = format!("{MANIFEST_MAGIC}\n");
    for e in entries {
        text.push_str(&e.render());
    }
    let tmp = p.with_extension(format!("staged-tmp-{}", std::process::id()));
    msvc6_base::srcio::own_write(&tmp, text)?;
    std::fs::rename(&tmp, p).with_context(|| format!("writing {}", p.display()))?;
    Ok(())
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Cleaned {
    pub removed: usize,
    pub skipped: Vec<String>,
}

fn clean_one(root: &Root, e: &Entry) -> Result<bool> {
    match e {
        Entry::Link { rel, target } => {
            let p = root.check(rel, true)?;
            let ours = std::fs::symlink_metadata(&p).is_ok_and(|m| m.file_type().is_symlink())
                && std::fs::read_link(&p).is_ok_and(|t| &t == target);
            if !ours {
                return Ok(false);
            }
            std::fs::remove_file(&p)?;
            Ok(true)
        }
        Entry::Dir { rel, dev, ino } | Entry::File { rel, dev, ino } => {
            let p = root.check(rel, false)?;
            let Ok(m) = std::fs::symlink_metadata(&p) else {
                return Ok(false);
            };
            if m.dev() != *dev || m.ino() != *ino || m.file_type().is_symlink() {
                return Ok(false);
            }
            if m.is_dir() {
                std::fs::remove_dir_all(&p)?;
            } else {
                std::fs::remove_file(&p)?;
            }
            Ok(true)
        }
    }
}

pub fn clean(root: &Root, manifest: &Path) -> Result<Cleaned> {
    let entries = read_manifest(manifest)?;
    let mut out = Cleaned::default();
    for e in entries.iter().rev() {
        let rel = match e {
            Entry::Link { rel, .. } | Entry::Dir { rel, .. } | Entry::File { rel, .. } => rel,
        };
        let gone = std::fs::symlink_metadata(root.path(rel)).is_err();
        match clean_one(root, e) {
            Ok(true) => out.removed += 1,
            Ok(false) if gone => {}
            Ok(false) => out.skipped.push(format!(
                "{}: not the entry this tool made there, left in place",
                root.path(rel).display()
            )),
            Err(why) => out
                .skipped
                .push(format!("{}: {why:#}", root.path(rel).display())),
        }
    }
    if plain_file(manifest) {
        std::fs::remove_file(manifest)?;
    }
    Ok(out)
}

pub fn files_under(root: &Path) -> Vec<String> {
    let mut out = Vec::new();
    let mut seen = BTreeSet::new();
    let mut todo = vec![PathBuf::new()];
    while let Some(rel) = todo.pop() {
        let here = root.join(&rel);
        if let Ok(real) = std::fs::canonicalize(&here)
            && !seen.insert(real)
        {
            continue;
        }
        let Ok(rd) = std::fs::read_dir(&here) else {
            continue;
        };
        for e in rd.flatten() {
            let r = rel.join(e.file_name());
            let full = root.join(&r);
            if full.is_dir() {
                todo.push(r);
            } else if full.is_file() {
                out.push(r.to_string_lossy().into_owned());
            }
        }
    }
    out.sort();
    out
}

#[derive(Debug)]
pub struct Stager<'a> {
    pub root: Root,
    pub cfg: &'a Config,
    pub manifest: PathBuf,
    entries: Vec<Entry>,
    dirs: BTreeSet<PathBuf>,
    files: BTreeSet<PathBuf>,
}

impl<'a> Stager<'a> {
    pub fn new(root: Root, cfg: &'a Config, manifest: PathBuf) -> Self {
        Self {
            root,
            cfg,
            manifest,
            entries: Vec::new(),
            dirs: BTreeSet::new(),
            files: BTreeSet::new(),
        }
    }

    fn record(&mut self, e: Entry) -> Result<()> {
        self.entries.push(e);
        write_manifest(&self.manifest, &self.entries)
    }

    fn ours(&self, rel: &Path) -> bool {
        self.files.contains(rel) || self.dirs.iter().any(|d| rel.starts_with(d))
    }

    pub fn make_dir(&mut self, rel: &Path) -> Result<PathBuf> {
        let (at, first) = self.root.create_dirs(rel)?;
        if let Some(f) = first {
            let m = std::fs::symlink_metadata(self.root.path(&f))?;
            self.dirs.insert(f.clone());
            self.record(Entry::Dir {
                rel: f,
                dev: m.dev(),
                ino: m.ino(),
            })?;
        }
        Ok(at)
    }

    pub fn host(&self, dos: &str) -> Result<PathBuf> {
        let rel = dos_rel(dos)?;
        self.root.check(&rel, false)
    }

    pub fn mount(&mut self, at: &str, from: &str) -> Result<()> {
        let rel = dos_rel(at)?;
        let src = self.cfg.path(from)?;
        if !src.exists() {
            bail!("mount {at}: {} does not exist", src.display());
        }
        if let Some(parent) = rel.parent().filter(|p| !p.as_os_str().is_empty()) {
            self.make_dir(parent)?;
        }
        let dst = self.root.check(&rel, true)?;
        if std::fs::symlink_metadata(&dst).is_ok() {
            bail!(
                "mount {at}: {} exists and this build did not make it",
                dst.display()
            );
        }
        std::os::unix::fs::symlink(&src, &dst)
            .with_context(|| format!("linking {} to {}", dst.display(), src.display()))?;
        self.record(Entry::Link { rel, target: src })
    }

    pub fn put_file(&mut self, rel: &Path, src: &Path) -> Result<()> {
        if let Some(parent) = rel.parent().filter(|p| !p.as_os_str().is_empty()) {
            self.make_dir(parent)?;
        }
        let dst = self.root.check(rel, false)?;
        if std::fs::symlink_metadata(&dst).is_ok() {
            if !self.files.contains(rel) || !plain_file(&dst) {
                bail!(
                    "{} exists and this build did not stage it: a stage never overwrites the prefix's own files",
                    dst.display()
                );
            }
            std::fs::remove_file(&dst)?;
        }
        let mut from =
            std::fs::File::open(src).with_context(|| format!("reading {}", src.display()))?;
        let mut to = std::fs::File::create_new(&dst)
            .with_context(|| format!("creating {}", dst.display()))?;
        std::io::copy(&mut from, &mut to)
            .with_context(|| format!("copying {} to {}", src.display(), dst.display()))?;
        if let Ok(p) = from.metadata().map(|m| m.permissions()) {
            let _ = to.set_permissions(p);
        }
        let fresh = !self.ours(rel);
        self.files.insert(rel.to_path_buf());
        if fresh {
            let m = to.metadata()?;
            self.record(Entry::File {
                rel: rel.to_path_buf(),
                dev: m.dev(),
                ino: m.ino(),
            })?;
        }
        Ok(())
    }

    fn our_file(&self, rel: &Path) -> Result<PathBuf> {
        if !self.files.contains(rel) {
            bail!(
                "{} is not a file this build staged",
                self.root.path(rel).display()
            );
        }
        let p = self.root.check(rel, false)?;
        if !plain_file(&p) {
            bail!("{} is not a plain file", p.display());
        }
        Ok(p)
    }

    pub fn stage(&mut self, s: &StageSpec) -> Result<u64> {
        let rel = dos_rel(&s.at)?;
        let src = self.cfg.path(&s.from)?;
        let mut n = 0;
        let is_dir = src.is_dir();
        if is_dir {
            self.make_dir(&rel)?;
            let all = files_under(&src);
            for f in &all {
                if s.files.is_empty() || s.files.iter().any(|g| crate::msdev::glob_match(g, f)) {
                    let name = match s.rename.get(f) {
                        Some(r) => rel_path(r)?,
                        None => rel_path(f)?,
                    };
                    self.put_file(&rel.join(name), &src.join(f))?;
                    n += 1;
                }
            }
            for g in &s.files {
                if !g.contains('*') && !all.iter().any(|f| crate::msdev::glob_match(g, f)) {
                    bail!("stage {}: {} has no {g}", s.at, src.display());
                }
            }
        } else if src.is_file() {
            self.put_file(&rel, &src)?;
            n += 1;
        } else {
            bail!("stage {}: {} does not exist", s.at, src.display());
        }
        let target = |name: &str| -> Result<PathBuf> {
            Ok(if is_dir {
                rel.join(rel_path(name)?)
            } else {
                rel.clone()
            })
        };
        for e in &s.edit {
            let f = self.our_file(&target(&e.file)?)?;
            let text =
                msvc6_base::srcio::read(&f).with_context(|| format!("reading {}", f.display()))?;
            let find = self.cfg.expand(&e.find)?;
            if !text.contains(&find) {
                bail!(
                    "stage {}: {} holds no `{find}` to replace",
                    s.at,
                    f.display()
                );
            }
            let rep = self.cfg.expand(&e.replace)?;
            msvc6_base::srcio::write(&f, &text.replace(&find, &rep))?;
        }
        for c in &s.crlf {
            let f = self.our_file(&target(c)?)?;
            let text = msvc6_base::srcio::read(&f)?;
            let fixed = text.replace("\r\n", "\n").replace('\n', "\r\n");
            msvc6_base::srcio::write(&f, &fixed)?;
        }
        Ok(n)
    }

    pub fn in_staged_dir(&self, rel: &Path) -> bool {
        self.dirs.iter().any(|d| rel.starts_with(d))
    }

    pub fn claim_output(&mut self, rel: &Path) -> Result<PathBuf> {
        if !rel
            .parent()
            .is_some_and(|p| self.in_staged_dir(p) || self.dirs.contains(p))
        {
            bail!(
                "{} is not inside a directory this build staged",
                self.root.path(rel).display()
            );
        }
        let p = self.root.check(rel, false)?;
        if std::fs::symlink_metadata(&p).is_ok() && !self.files.contains(rel) {
            bail!("{} exists and this build did not make it", p.display());
        }
        Ok(p)
    }

    pub fn write_text(&mut self, rel: &Path, text: &str) -> Result<PathBuf> {
        let p = self.claim_output(rel)?;
        if std::fs::symlink_metadata(&p).is_ok() {
            std::fs::remove_file(&p)?;
        }
        let bytes = msvc6_base::srcio::encode(text)?;
        let mut f =
            std::fs::File::create_new(&p).with_context(|| format!("creating {}", p.display()))?;
        std::io::Write::write_all(&mut f, &bytes)?;
        self.files.insert(rel.to_path_buf());
        Ok(p)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::EditSpec;

    struct Scratch(PathBuf);

    impl Drop for Scratch {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn setup(tag: &str) -> (Scratch, PathBuf, PathBuf, Config) {
        let d = std::env::temp_dir().join(format!("msvc6t-stage-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let src = d.join("product/app");
        std::fs::create_dir_all(src.join("src")).unwrap();
        std::fs::create_dir_all(src.join("m")).unwrap();
        for (f, body) in [
            ("src/a.cpp", "int a;\r\n"),
            ("src/notes.txt", "x"),
            ("m/new.txt", "n"),
            ("app.dsw", "Project: \"app\"=.\\app\\app.dsp - x\r\n"),
        ] {
            msvc6_base::srcio::write_bytes(src.join(f), body.as_bytes()).unwrap();
        }
        let prefix = d.join("pool/wp0");
        std::fs::create_dir_all(prefix.join("drive_c/windows")).unwrap();
        msvc6_base::srcio::write_bytes(prefix.join("drive_c/windows/template.dll"), b"t").unwrap();
        let cfg = crate::config::test_config("", Some(&d.join("msvc6t.toml")));
        (Scratch(d), src, prefix, cfg)
    }

    fn spec(at: &str, from: &Path) -> StageSpec {
        StageSpec {
            at: at.into(),
            from: from.display().to_string(),
            files: Vec::new(),
            crlf: Vec::new(),
            rename: Default::default(),
            edit: Vec::new(),
        }
    }

    #[test]
    fn a_stage_never_writes_through_a_mount_or_out_of_drive_c() {
        let (s, src, prefix, cfg) = setup("mount");
        let root = Root::new(&prefix.join("drive_c")).unwrap();
        let mut st = Stager::new(root, &cfg, s.0.join("pool/wp0.staged"));
        st.mount("C:\\work\\m", src.join("m").to_str().unwrap())
            .unwrap();
        let e = st.stage(&spec("C:\\work", &src)).unwrap_err();
        assert!(e.to_string().contains("symbolic link"), "{e}");
        assert!(!src.join("m/m").exists() && !src.join("m/src").exists());
        let mut ren = spec("C:\\work2", &src);
        ren.files = vec!["src/a.cpp".into()];
        ren.rename
            .insert("src/a.cpp".into(), "../work/m/renamed.txt".into());
        assert!(st.stage(&ren).is_err());
        assert!(!src.join("m/renamed.txt").exists());
        let mut ed = spec("C:\\work3", &src);
        ed.files = vec!["src/a.cpp".into()];
        ed.edit = vec![EditSpec {
            file: "../work/m/new.txt".into(),
            find: "n".into(),
            replace: "CHANGED".into(),
        }];
        assert!(st.stage(&ed).is_err());
        assert_eq!(std::fs::read(src.join("m/new.txt")).unwrap(), b"n");
        assert!(st.stage(&spec("C:\\n/../../escaped", &src)).is_err());
        assert!(!prefix.join("escaped").exists() && !s.0.join("pool/escaped").exists());
        assert!(
            st.mount("C:\\q/../../mnt-escaped", src.to_str().unwrap())
                .is_err()
        );
        assert!(!s.0.join("pool/mnt-escaped").exists());
        let e = st
            .stage(&spec("C:\\windows\\template.dll", &src.join("app.dsw")))
            .unwrap_err();
        assert!(e.to_string().contains("did not stage"), "{e}");
        assert_eq!(
            std::fs::read(prefix.join("drive_c/windows/template.dll")).unwrap(),
            b"t"
        );
        assert!(st.claim_output(Path::new("windows/x.lib")).is_err());
        assert!(st.claim_output(Path::new("work/m/x.lib")).is_err());
    }

    #[test]
    fn a_stage_filters_edits_and_cleans_only_what_it_made() {
        let (s, src, prefix, cfg) = setup("clean");
        let drive_c = prefix.join("drive_c");
        let manifest = s.0.join("pool/wp0.staged");
        let mut st = Stager::new(Root::new(&drive_c).unwrap(), &cfg, manifest.clone());
        st.mount("C:\\Prod\\ucts\\app", src.to_str().unwrap())
            .unwrap();
        let mut a = spec("C:\\work\\app", &src);
        a.files = vec!["src/*.cpp".into()];
        assert_eq!(st.stage(&a).unwrap(), 1);
        let mut dsw = spec("C:\\work\\app\\app.dsw", &src.join("app.dsw"));
        dsw.edit = vec![EditSpec {
            file: String::new(),
            find: "=.\\app\\".into(),
            replace: "=.\\".into(),
        }];
        st.stage(&dsw).unwrap();
        let w = drive_c.join("work/app");
        assert!(w.join("src/a.cpp").is_file() && !w.join("src/notes.txt").exists());
        assert_eq!(
            msvc6_base::srcio::read(w.join("app.dsw")).unwrap(),
            "Project: \"app\"=.\\app.dsp - x\r\n"
        );
        let victim = s.0.join("victim");
        std::fs::create_dir_all(&victim).unwrap();
        msvc6_base::srcio::write_bytes(victim.join("keep"), b"k").unwrap();
        msvc6_base::srcio::write_bytes(
            prefix.join(".msvc6t-staged"),
            format!("{}\n", drive_c.join("../../victim").display()).as_bytes(),
        )
        .unwrap();
        let c = clean(&Root::new(&drive_c).unwrap(), &manifest).unwrap();
        assert!(c.skipped.is_empty(), "{c:?}");
        let left: Vec<_> = std::fs::read_dir(&drive_c)
            .unwrap()
            .flatten()
            .map(|e| e.file_name())
            .collect();
        assert_eq!(left, vec![std::ffi::OsString::from("windows")]);
        assert!(src.join("src/a.cpp").is_file() && victim.join("keep").is_file());
        msvc6_base::srcio::own_write(
            &manifest,
            format!("{MANIFEST_MAGIC}\ndir\twindows\t1\t1\nlink\t..\t/\n"),
        )
        .unwrap();
        assert!(clean(&Root::new(&drive_c).unwrap(), &manifest).is_err());
        msvc6_base::srcio::own_write(&manifest, format!("{MANIFEST_MAGIC}\ndir\twindows\t1\t1\n"))
            .unwrap();
        let c = clean(&Root::new(&drive_c).unwrap(), &manifest).unwrap();
        assert_eq!(c.skipped.len(), 1);
        assert!(drive_c.join("windows/template.dll").is_file());
    }
}
