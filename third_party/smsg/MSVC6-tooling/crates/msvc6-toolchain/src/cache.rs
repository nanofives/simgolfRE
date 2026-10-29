use crate::confine::Root;
use crate::prefix::remove_any;
use anyhow::{Context as _, Result, bail};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

pub const LAYOUT: &str = "msvc6t-cl-cache/1";
pub const OK_FILE: &str = "OK";
pub const USED_FILE: &str = "USED";
pub const DEFAULT_MAX_ENTRY: u64 = 512 << 20;

pub fn key_of(fields: &[&[u8]]) -> String {
    let mut buf = Vec::new();
    buf.extend_from_slice(
        &u64::try_from(fields.len())
            .unwrap_or(u64::MAX)
            .to_le_bytes(),
    );
    for f in fields {
        buf.extend_from_slice(&u64::try_from(f.len()).unwrap_or(u64::MAX).to_le_bytes());
        buf.extend_from_slice(f);
    }
    msvc6_base::digest::sha256_hex(&buf)
}

pub fn sha_file(p: &Path) -> Option<String> {
    std::fs::read(p)
        .ok()
        .map(|b| msvc6_base::digest::sha256_hex(&b))
}

pub fn bypasses(arg: &str) -> bool {
    let l = arg.to_ascii_lowercase();
    [
        "/zi", "/z7", "/fd", "/yc", "/yu", "/yx", "/fp", "/fr", "/bk", "-zi", "-z7", "-yx", "-bk",
    ]
    .iter()
    .any(|p| l.starts_with(p))
}

pub fn key_args(args: &[String]) -> Vec<String> {
    args.iter()
        .filter(|a| {
            let l = a.to_ascii_lowercase();
            (a.starts_with('/') || a.starts_with('-'))
                && !l.starts_with("/i")
                && !l.starts_with("-i")
                && !l.starts_with("/fo")
                && !l.starts_with("/fa")
        })
        .cloned()
        .collect()
}

pub fn normalize_preprocessed(text: &[u8]) -> (Vec<u8>, Vec<String>) {
    let mut out = Vec::with_capacity(text.len());
    let mut paths: Vec<String> = Vec::new();
    for line in text.split_inclusive(|&b| b == b'\n') {
        let lead = line
            .iter()
            .take_while(|&&b| b == b' ' || b == b'\t')
            .count();
        let body = line.get(lead..).unwrap_or_default();
        let directive = body.starts_with(b"#line ") || body.starts_with(b"# ");
        let q0 = line.iter().position(|&b| b == b'"');
        let q1 = line.iter().rposition(|&b| b == b'"');
        match (directive, q0, q1) {
            (true, Some(a), Some(b)) if b > a => {
                let p = msvc6_base::srcio::decode(line.get(a + 1..b).unwrap_or_default())
                    .replace("\\\\", "\\");
                let name = p
                    .rsplit(['\\', '/'])
                    .next()
                    .unwrap_or("")
                    .to_ascii_lowercase();
                out.extend_from_slice(line.get(..=a).unwrap_or_default());
                out.extend_from_slice(name.as_bytes());
                out.extend_from_slice(line.get(b..).unwrap_or_default());
                if !paths.contains(&p) {
                    paths.push(p);
                }
            }
            _ => out.extend_from_slice(line),
        }
    }
    (out, paths)
}

pub fn non_text_reason(bytes: &[u8]) -> Option<String> {
    let magic: [(&[u8], &str); 4] = [
        (b"MZ", "a PE image"),
        (b"!<arch>\n", "an archive"),
        (b"Microsoft C/C++ MSF", "a PDB"),
        (&[0x4c, 0x01], "a COFF object"),
    ];
    for (m, what) in magic {
        if bytes.starts_with(m) {
            return Some(format!("it is {what}"));
        }
    }
    bytes
        .iter()
        .position(|&b| b == 0)
        .map(|i| format!("it holds a NUL byte at offset {i}"))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Outputs {
    pub obj: PathBuf,
    pub cod: Option<PathBuf>,
}

impl Outputs {
    fn parts(&self) -> Vec<(&'static str, &Path)> {
        let mut v = vec![("obj", self.obj.as_path())];
        if let Some(c) = &self.cod {
            v.push(("cod", c.as_path()));
        }
        v
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Outcome {
    HitDirect,
    Hit,
    Stored,
    Bypass,
}

impl Outcome {
    pub fn is_hit(self) -> bool {
        matches!(self, Self::HitDirect | Self::Hit)
    }

    pub fn label(self) -> &'static str {
        match self {
            Self::HitDirect => "hit (direct, no wine)",
            Self::Hit => "hit (after CL /P)",
            Self::Stored => "stored",
            Self::Bypass => "uncached",
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Manifest {
    pub key: String,
    pub files: Vec<(PathBuf, String)>,
    pub dirs: Vec<(PathBuf, String)>,
}

pub fn dir_listing(d: &Path) -> String {
    let mut names: Vec<String> = std::fs::read_dir(d)
        .map(|rd| {
            rd.flatten()
                .map(|e| e.file_name().to_string_lossy().to_ascii_lowercase())
                .filter(|n| {
                    [".h", ".hpp", ".hxx", ".inl", ".inc"]
                        .iter()
                        .any(|x| n.ends_with(x))
                })
                .collect()
        })
        .unwrap_or_default();
    names.sort();
    key_of(&names.iter().map(String::as_bytes).collect::<Vec<_>>())
}

impl Manifest {
    pub fn render(&self) -> String {
        let mut s = format!("{LAYOUT}\nkey {}\n", self.key);
        for (p, h) in &self.files {
            s.push_str(&format!("file {h} {}\n", p.display()));
        }
        for (p, h) in &self.dirs {
            s.push_str(&format!("dir {h} {}\n", p.display()));
        }
        s
    }

    pub fn parse(text: &str) -> Option<Self> {
        let mut lines = text.lines();
        if lines.next()? != LAYOUT {
            return None;
        }
        let mut m = Self::default();
        for l in lines {
            let (kind, rest) = l.split_once(' ')?;
            match kind {
                "key" => m.key = rest.to_string(),
                "file" | "dir" => {
                    let (h, p) = rest.split_once(' ')?;
                    let row = (PathBuf::from(p), h.to_string());
                    if kind == "file" {
                        m.files.push(row);
                    } else {
                        m.dirs.push(row);
                    }
                }
                _ => return None,
            }
        }
        (!m.key.is_empty()).then_some(m)
    }

    pub fn changed(&self) -> Option<String> {
        for (p, h) in &self.files {
            if sha_file(p).as_deref() != Some(h.as_str()) {
                return Some(format!("{} changed", p.display()));
            }
        }
        for (d, h) in &self.dirs {
            if &dir_listing(d) != h {
                return Some(format!("the headers in {} changed", d.display()));
            }
        }
        None
    }

    pub fn touched_since(&self, t: SystemTime) -> Option<String> {
        self.files.iter().find_map(|(p, _)| {
            let m = std::fs::metadata(p).ok()?.modified().ok()?;
            (m >= t).then(|| format!("{} was modified while it compiled", p.display()))
        })
    }

    pub fn reads(&self) -> String {
        let mut v: Vec<&str> = self.files.iter().map(|(_, h)| h.as_str()).collect();
        v.sort_unstable();
        v.join("\n")
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Cache {
    pub root: PathBuf,
    pub enabled: bool,
    pub max_entry: u64,
    pub cap: Option<u64>,
}

pub const MARKER: &str = "MSVC6T-CL-CACHE";

pub fn is_hex(s: &str, n: usize) -> bool {
    s.len() == n
        && s.bytes()
            .all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase())
}

fn shard_rel(kind: &str, key: &str) -> PathBuf {
    Path::new(kind).join(key.get(..2).unwrap_or("00"))
}

fn lexical(p: &Path) -> Option<PathBuf> {
    let mut out = PathBuf::new();
    for c in p.components() {
        match c {
            std::path::Component::RootDir => out.push("/"),
            std::path::Component::Normal(n) => out.push(n),
            std::path::Component::CurDir => {}
            _ => return None,
        }
    }
    Some(out)
}

pub fn root_problem(root: &Path) -> Option<String> {
    if root.as_os_str().is_empty() {
        return Some("the cache root is empty".into());
    }
    if !root.is_absolute() {
        return Some(format!("{} is relative", root.display()));
    }
    let Some(lex) = lexical(root) else {
        return Some(format!("{} holds `..`", root.display()));
    };
    let mut views = vec![lex];
    if let Ok(real) = std::fs::canonicalize(root) {
        views.push(real);
    }
    let homes: Vec<PathBuf> = std::env::var_os("HOME")
        .map(PathBuf::from)
        .into_iter()
        .flat_map(|h| {
            let real = std::fs::canonicalize(&h).ok();
            std::iter::once(h).chain(real)
        })
        .collect();
    for v in &views {
        if v.parent().is_none() {
            return Some(format!("{} is the file system root", root.display()));
        }
        if let Some(h) = homes.iter().find(|h| h.starts_with(v)) {
            return Some(format!(
                "{} is $HOME ({}) or one of its ancestors",
                root.display(),
                h.display()
            ));
        }
    }
    match std::fs::symlink_metadata(root) {
        Err(_) => None,
        Ok(m) if m.file_type().is_symlink() => Some(format!(
            "{} is a symbolic link: name the cache directory itself",
            root.display()
        )),
        Ok(m) if !m.is_dir() => Some(format!("{} is not a directory", root.display())),
        Ok(_) => {
            let empty = std::fs::read_dir(root).is_ok_and(|mut rd| rd.next().is_none());
            let marked = std::fs::symlink_metadata(root.join(MARKER)).is_ok_and(|m| m.is_file());
            (!empty && !marked).then(|| {
                format!(
                    "{} holds files and no {MARKER} marker: it is not a msvc6t compile cache",
                    root.display()
                )
            })
        }
    }
}

impl Cache {
    pub fn new(root: PathBuf) -> Result<Self> {
        if let Some(why) = root_problem(&root) {
            bail!("refusing the compile cache root: {why}");
        }
        Ok(Self {
            root,
            enabled: std::env::var("MSVC6T_CACHE").map_or(true, |v| v.trim() != "0"),
            max_entry: DEFAULT_MAX_ENTRY,
            cap: None,
        })
    }

    pub fn from_config(cfg: &crate::config::Config) -> Result<Self> {
        let dir = match &cfg.cache.dir {
            Some(d) => cfg.path(d)?,
            None => {
                PathBuf::from(std::env::var_os("HOME").unwrap_or_default()).join(".cache/msvc6t/cl")
            }
        };
        let mut c = Self::new(dir)?;
        c.cap = cfg
            .cache
            .max
            .as_deref()
            .map(crate::config::parse_size)
            .transpose()?;
        if cfg.cache.enabled == Some(false) {
            c.enabled = false;
        }
        Ok(c)
    }

    pub fn checked(&self) -> Result<Root> {
        if let Some(why) = root_problem(&self.root) {
            bail!("refusing the compile cache root: {why}");
        }
        Root::new(&self.root)
    }

    pub fn open_root(&self) -> Result<Root> {
        if let Some(why) = root_problem(&self.root) {
            bail!("refusing the compile cache root: {why}");
        }
        std::fs::create_dir_all(&self.root)
            .with_context(|| format!("creating {}", self.root.display()))?;
        let marker = self.root.join(MARKER);
        if std::fs::symlink_metadata(&marker).is_err() {
            let mut f = std::fs::File::create_new(&marker)
                .with_context(|| format!("creating {}", marker.display()))?;
            std::io::Write::write_all(&mut f, format!("{LAYOUT}\n").as_bytes())?;
        }
        self.checked()
    }

    pub fn entry(&self, key: &str) -> PathBuf {
        self.root.join(shard_rel("o", key)).join(key)
    }

    pub fn manifest_path(&self, mkey: &str) -> PathBuf {
        self.root.join(shard_rel("m", mkey)).join(mkey)
    }

    fn checked_entry(&self, key: &str) -> Option<PathBuf> {
        if !is_hex(key, 64) {
            return None;
        }
        let r = self.checked().ok()?;
        r.check(&shard_rel("o", key).join(key), false).ok()
    }

    pub fn read_manifest(&self, mkey: &str) -> Option<Manifest> {
        if !is_hex(mkey, 64) {
            return None;
        }
        let r = self.checked().ok()?;
        let p = r.check(&shard_rel("m", mkey).join(mkey), false).ok()?;
        if !std::fs::symlink_metadata(&p).is_ok_and(|m| m.is_file()) {
            return None;
        }
        let b = std::fs::read(p).ok()?;
        Manifest::parse(&msvc6_base::srcio::decode(&b))
    }

    pub fn save_manifest(&self, mkey: &str, m: &Manifest) {
        let Ok(r) = self.open_root() else {
            return;
        };
        if !is_hex(mkey, 64) {
            return;
        }
        let Ok((dir, _)) = r.create_dirs(&shard_rel("m", mkey)) else {
            return;
        };
        let tmp = dir.join(format!("{mkey}.tmp-{}", std::process::id()));
        let _ = std::fs::remove_file(&tmp);
        let written = std::fs::File::create_new(&tmp)
            .and_then(|mut f| std::io::Write::write_all(&mut f, m.render().as_bytes()));
        if written.is_ok() {
            let _ = std::fs::rename(&tmp, dir.join(mkey));
        } else {
            let _ = std::fs::remove_file(&tmp);
        }
    }

    pub fn lock(&self, key: &str) -> Option<std::fs::File> {
        if !is_hex(key, 64) {
            return None;
        }
        let r = self.open_root().ok()?;
        let (dir, _) = r.create_dirs(Path::new("l")).ok()?;
        let p = r
            .check(&Path::new("l").join(format!("{key}.lock")), false)
            .ok()?;
        let _ = dir;
        let f = std::fs::File::options()
            .create(true)
            .truncate(false)
            .write(true)
            .open(p)
            .ok()?;
        f.lock().ok()?;
        Some(f)
    }

    pub fn part_sums(e: &Path) -> Option<BTreeMap<String, String>> {
        let ok = e.join(OK_FILE);
        if !std::fs::symlink_metadata(&ok).is_ok_and(|m| m.is_file()) {
            return None;
        }
        let b = std::fs::read(ok).ok()?;
        let text = msvc6_base::srcio::decode(&b);
        let mut lines = text.lines();
        if lines.next()? != LAYOUT {
            return None;
        }
        Some(
            lines
                .filter_map(|l| {
                    let (name, h) = l.split_once(' ')?;
                    Some((name.to_string(), h.to_string()))
                })
                .collect(),
        )
    }

    pub fn restore(&self, key: &str, outs: &Outputs) -> Option<String> {
        let e = self.checked_entry(key)?;
        if !std::fs::symlink_metadata(&e).is_ok_and(|m| m.is_dir()) {
            return None;
        }
        let sums = Self::part_sums(&e)?;
        let mut held: Vec<(&Path, Vec<u8>)> = Vec::new();
        for (name, dst) in outs.parts() {
            let part = e.join(name);
            if !std::fs::symlink_metadata(&part).is_ok_and(|m| m.is_file()) {
                return None;
            }
            let b = std::fs::read(part).ok()?;
            if sums.get(name).map(String::as_str)
                != Some(msvc6_base::digest::sha256_hex(&b).as_str())
            {
                eprintln!("note: compile cache entry {key} has a corrupt {name}; evicting it");
                let _ = std::fs::remove_dir_all(&e);
                return None;
            }
            held.push((dst, b));
        }
        let text = std::fs::read(e.join("text"))
            .map(|b| msvc6_base::srcio::tool_text(&b))
            .unwrap_or_default();
        for (dst, b) in held {
            msvc6_base::srcio::write_bytes(dst, &b).ok()?;
        }
        crate::gc::touch_used(&e);
        Some(text)
    }

    pub fn store(&self, key: &str, outs: &Outputs, text: &str) -> Result<bool> {
        if !is_hex(key, 64) {
            bail!("`{key}` is not a cache key");
        }
        let r = self.open_root()?;
        let e = r.check(&shard_rel("o", key).join(key), false)?;
        if Self::part_sums(&e).is_some() {
            return Ok(true);
        }
        let mut parts: Vec<(&str, Vec<u8>)> = Vec::new();
        let mut total = 0u64;
        for (name, src) in outs.parts() {
            let b = std::fs::read(src).with_context(|| format!("reading {}", src.display()))?;
            total += u64::try_from(b.len()).unwrap_or(u64::MAX);
            parts.push((name, b));
        }
        if total > self.max_entry {
            return Ok(false);
        }
        let (parent, _) = r.create_dirs(&shard_rel("o", key))?;
        let tmp = parent.join(format!(".tmp-{key}-{}", std::process::id()));
        remove_any(&tmp)?;
        std::fs::create_dir(&tmp)?;
        let mut ok = format!("{LAYOUT}\n");
        for (name, b) in &parts {
            msvc6_base::srcio::write_bytes(tmp.join(name), b)?;
            ok.push_str(&format!("{name} {}\n", msvc6_base::digest::sha256_hex(b)));
        }
        msvc6_base::srcio::own_write(tmp.join("text"), text)?;
        msvc6_base::srcio::own_write(tmp.join(OK_FILE), ok)?;
        match std::fs::rename(&tmp, &e) {
            Ok(()) => {
                crate::gc::note_store(&self.root);
                Ok(true)
            }
            Err(_) if Self::part_sums(&e).is_some() => {
                remove_any(&tmp)?;
                Ok(true)
            }
            Err(err) => {
                remove_any(&tmp)?;
                bail!("storing {}: {err}", e.display())
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn line_paths_reduce_to_names_and_are_collected() {
        let t = b"#line 1 \"Z:\\\\w\\\\a.c\"\nint a;\n  #line 7 \"C:\\\\INC\\\\Stdio.H\"\n#pragma x \"q\"\n";
        let (n, p) = normalize_preprocessed(t);
        assert_eq!(
            msvc6_base::srcio::decode(&n),
            "#line 1 \"a.c\"\nint a;\n  #line 7 \"stdio.h\"\n#pragma x \"q\"\n"
        );
        assert_eq!(
            p,
            vec!["Z:\\w\\a.c".to_string(), "C:\\INC\\Stdio.H".to_string()]
        );
    }

    #[test]
    fn switches_that_name_outputs_or_dirs_leave_the_key() {
        let a: Vec<String> = ["/c", "/O2", "/IZ:\\x", "/FoZ:\\o.obj", "/FAcs", "a.c"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        assert_eq!(key_args(&a), vec!["/c".to_string(), "/O2".to_string()]);
        assert!(bypasses("/Zi") && bypasses("/YXfoo.h") && !bypasses("/O2"));
        assert_ne!(key_of(&[b"ab", b"c"]), key_of(&[b"a", b"bc"]));
        assert!(non_text_reason(b"MZ\x90").unwrap().contains("PE"));
        assert!(non_text_reason(b"int a;\0").unwrap().contains("offset 6"));
        assert_eq!(non_text_reason(b"/* \xa7 */\r\nint a;"), None);
    }

    #[test]
    fn an_entry_restores_what_was_stored_and_refuses_a_corrupt_part() {
        let d = std::env::temp_dir().join(format!("msvc6t-cache-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        let c = Cache::new(d.join("c")).unwrap();
        let k1 = "1".repeat(64);
        let outs = Outputs {
            obj: d.join("a.obj"),
            cod: None,
        };
        msvc6_base::srcio::write_bytes(&outs.obj, b"obj-bytes").unwrap();
        assert!(c.store(&k1, &outs, "CL said").unwrap());
        std::fs::remove_file(&outs.obj).unwrap();
        assert_eq!(c.restore(&k1, &outs).as_deref(), Some("CL said"));
        assert_eq!(std::fs::read(&outs.obj).unwrap(), b"obj-bytes");
        msvc6_base::srcio::write_bytes(c.entry(&k1).join("obj"), b"tampered").unwrap();
        assert_eq!(c.restore(&k1, &outs), None);
        assert!(!c.entry(&k1).exists());
        let m = Manifest {
            key: "k".into(),
            files: vec![(outs.obj.clone(), sha_file(&outs.obj).unwrap())],
            dirs: vec![(d.clone(), dir_listing(&d))],
        };
        assert_eq!(Manifest::parse(&m.render()), Some(m.clone()));
        assert_eq!(m.changed(), None);
        msvc6_base::srcio::write_bytes(d.join("new.h"), b"").unwrap();
        assert!(m.changed().unwrap().contains("headers"));
        std::fs::remove_dir_all(&d).unwrap();
    }
}
