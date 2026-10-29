use crate::Outcome;
use anyhow::{Context as _, Result, anyhow, bail};
use clap::Args;
use msvc6_base::srcio;
use msvc6_source::{git, glob, tree};
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet};
use std::io::Write as _;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

pub const DEFAULT_SPEC: &str = "release/publish.toml";

#[derive(Debug, Args)]
pub struct PublishArgs {
    #[arg(
        long,
        value_name = "DIR",
        default_value = ".",
        help = "The git repository the release is cut from."
    )]
    pub repo: PathBuf,
    #[arg(
        long,
        value_name = "REV",
        default_value = "HEAD",
        help = "The commit whose tree is published."
    )]
    pub rev: String,
    #[arg(
        long,
        value_name = "PATH",
        default_value = DEFAULT_SPEC,
        help = "The release spec, a path in REV's tree: it is read from the commit, never the work tree."
    )]
    pub spec: String,
    #[arg(
        long,
        value_name = "NAME",
        help = "Commit the tree to this branch as its one root commit, by the spec's `author` with the spec's \
                message; the old tip, a release of its own, is replaced, and kept when it already is that commit. \
                Without it, only the tree is written and named."
    )]
    pub branch: Option<String>,
    #[arg(
        long,
        requires = "branch",
        help = "Write nothing to the branch: check that its tip is the commit the spec gives at REV, rebuilt at the \
                tip's own time: one root commit by the spec's `author`, with the spec's message and tree and no other \
                header (a signature or an encoding is refused)."
    )]
    pub check: bool,
    #[arg(
        long,
        help = "Print every path published, an overlaid one with the path it comes from, and after a tab the \
                readers `screen.readers` names for it."
    )]
    pub list: bool,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Spec {
    pub message: String,
    pub author: Option<String>,
    pub credits: Option<String>,
    pub include: Vec<String>,
    #[serde(default)]
    pub exclude: Vec<String>,
    #[serde(default)]
    pub overlay: BTreeMap<String, String>,
    #[serde(default)]
    pub screen: Option<Screen>,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Prose {
    #[default]
    Any,
    Overlays,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Screen {
    #[serde(default)]
    pub refuse: Vec<String>,
    #[serde(default)]
    pub allow: BTreeMap<String, Vec<String>>,
    #[serde(default)]
    pub prose: Prose,
    #[serde(default)]
    pub prose_allow: Vec<String>,
    #[serde(default)]
    pub comments: Option<Vec<String>>,
    #[serde(default)]
    pub readers: Option<BTreeMap<String, String>>,
}

pub const REGULAR_MODES: [&str; 2] = ["100644", "100755"];

pub const C_EXTENSIONS: [&str; 9] = ["c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "inl"];

pub const PROSE_EXTENSIONS: [&str; 4] = ["md", "markdown", "rst", "txt"];

pub const SHOWN: usize = 20;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Screened {
    pub files: usize,
    pub allowed_references: usize,
    pub allowed_prose: usize,
    pub allowed_comments: usize,
    pub readers: Option<usize>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Item {
    pub mode: String,
    pub id: String,
    pub path: String,
    pub from: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Committed {
    Created(String),
    Replaced { old: String, new: String },
    Unchanged(String),
}

pub const CREDITS: &str = "Credits";

pub const UTC: &str = "+0000";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Identity {
    pub name: String,
    pub email: String,
}

impl Identity {
    pub fn parse(s: &str) -> Result<Self> {
        let (name, rest) = s
            .split_once('<')
            .with_context(|| format!("`{s}` is not `Name <email>`"))?;
        let email = rest
            .strip_suffix('>')
            .with_context(|| format!("`{s}` is not `Name <email>`"))?;
        let name = name.trim();
        let bad = |t: &str| t.is_empty() || t.contains(['<', '>', '\n', '\r']);
        if bad(name) || bad(email) || email.contains(char::is_whitespace) {
            bail!("`{s}` is not `Name <email>`");
        }
        Ok(Self {
            name: name.to_string(),
            email: email.to_string(),
        })
    }

    #[must_use]
    pub fn shown(&self) -> String {
        format!("{} <{}>", self.name, self.email)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Cut {
    pub message: String,
    pub author: Option<Identity>,
    pub time: u64,
}

fn heading(line: &str) -> Option<(usize, &str)> {
    let level = line.bytes().take_while(|&b| b == b'#').count();
    let title = line.get(level..)?.strip_prefix(' ')?;
    (1..=6).contains(&level).then(|| (level, title.trim()))
}

pub fn credits_section(text: &str) -> Result<String> {
    let lines: Vec<&str> = text.lines().collect();
    let mut fenced = false;
    let heads: Vec<Option<(usize, &str)>> = lines
        .iter()
        .map(|l| {
            if l.trim_start().starts_with("```") {
                fenced = !fenced;
                return None;
            }
            if fenced { None } else { heading(l) }
        })
        .collect();
    let (start, level) = heads
        .iter()
        .enumerate()
        .find_map(|(i, h)| h.filter(|(_, t)| *t == CREDITS).map(|(n, _)| (i, n)))
        .with_context(|| format!("no `{CREDITS}` heading"))?;
    let end = heads
        .iter()
        .enumerate()
        .skip(start + 1)
        .find(|(_, h)| h.is_some_and(|(n, _)| n <= level))
        .map_or(lines.len(), |(i, _)| i);
    let body = lines.get(start + 1..end).unwrap_or_default();
    let first = body.iter().position(|l| !l.trim().is_empty());
    let last = body.iter().rposition(|l| !l.trim().is_empty());
    let (Some(first), Some(last)) = (first, last) else {
        bail!("the `{CREDITS}` section is empty");
    };
    let mut out = body.get(first..=last).unwrap_or_default().join("\n");
    out.push('\n');
    Ok(out)
}

#[must_use]
pub fn message_with(title: &str, credits: Option<&str>) -> String {
    match credits {
        Some(c) => format!("{}\n\n{c}", title.trim()),
        None => format!("{}\n", title.trim()),
    }
}

pub fn parse(text: &str) -> Result<Spec> {
    let spec: Spec = toml::from_str(text).map_err(|e| anyhow!("{e}"))?;
    if spec.message.trim().is_empty() {
        bail!("`message` is empty");
    }
    if spec.include.is_empty() {
        bail!("`include` names nothing");
    }
    if let Some(a) = &spec.author {
        Identity::parse(a).context("`author`")?;
    }
    if let Some(c) = &spec.credits
        && (!tree::safe_rel(c) || c.contains('\\') || c.ends_with('/'))
    {
        bail!("`credits` = `{c}`: not a relative path inside the release");
    }
    if let Some(s) = &spec.screen {
        if s.refuse.iter().any(String::is_empty) {
            bail!("`screen.refuse` holds an empty text");
        }
        for t in s.allow.keys() {
            if !s.refuse.contains(t) {
                bail!("`screen.allow` names `{t}`, which `screen.refuse` does not hold");
            }
        }
        if s.prose == Prose::Any && !s.prose_allow.is_empty() {
            bail!(
                "`screen.prose_allow` excuses prose, and only `prose = \"overlays\"` refuses any"
            );
        }
        for (p, r) in s.readers.iter().flatten() {
            if p.trim().is_empty() || r.trim().is_empty() {
                bail!(
                    "`screen.readers` holds `{p}` = `{r}`: a rule is a pattern and the reader it names"
                );
            }
        }
    }
    Ok(spec)
}

fn name_byte(b: u8) -> bool {
    b.is_ascii_alphanumeric() || matches!(b, b'_' | b'.' | b'-')
}

fn drive_form(t: &[u8]) -> bool {
    matches!(t, [d, b':', ..] if d.is_ascii_alphabetic())
}

fn reference_end(text: &[u8], at: usize, t: &[u8], fold: bool) -> Option<usize> {
    let (mut i, mut k) = (at, 0usize);
    while let Some(&c) = t.get(k) {
        if c == b'\\' {
            while t.get(k) == Some(&b'\\') {
                k += 1;
            }
            let run = i;
            while text.get(i) == Some(&b'\\') {
                i += 1;
            }
            if i == run {
                return None;
            }
            continue;
        }
        let have = *text.get(i)?;
        if have != c && !(fold && have.eq_ignore_ascii_case(&c)) {
            return None;
        }
        i += 1;
        k += 1;
    }
    Some(i)
}

pub const PATH_BYTES: [u8; 4] = *b"/\\:~";

#[must_use]
pub fn is_word(t: &str) -> bool {
    !t.is_empty()
        && !t.starts_with(' ')
        && !t.ends_with(' ')
        && !t.contains("  ")
        && t.bytes()
            .all(|b| (b.is_ascii_graphic() || b == b' ') && !PATH_BYTES.contains(&b))
}

#[must_use]
pub fn references(text: &[u8], t: &str) -> Vec<usize> {
    let word = is_word(t);
    let t = t.as_bytes();
    let Some(&first) = t.first() else {
        return Vec::new();
    };
    let fold = word || drive_form(t);
    let bounded_start = name_byte(first);
    (0..text.len())
        .filter(|&at| {
            let Some(end) = reference_end(text, at, t, fold) else {
                return false;
            };
            if word {
                return true;
            }
            let prev = at.checked_sub(1).and_then(|p| text.get(p)).copied();
            let next = text.get(end).copied();
            (!bounded_start || !prev.is_some_and(name_byte)) && next.is_some_and(name_byte)
        })
        .collect()
}

fn line_at(text: &[u8], at: usize) -> usize {
    text.get(..at)
        .map_or(0, |b| b.iter().filter(|&&c| c == b'\n').count())
        + 1
}

fn excerpt(text: &[u8], at: usize) -> String {
    let rest = text.get(at..).unwrap_or_default();
    let n = rest
        .iter()
        .position(|&b| !(name_byte(b) || matches!(b, b'/' | b'\\' | b':' | b'~')))
        .unwrap_or(rest.len())
        .min(80);
    srcio::tool_text(rest.get(..n).unwrap_or_default())
}

fn is_c(path: &str) -> bool {
    Path::new(path)
        .extension()
        .and_then(|e| e.to_str())
        .is_some_and(|e| C_EXTENSIONS.contains(&e.to_ascii_lowercase().as_str()))
}

#[must_use]
pub fn prose_kind(path: &str) -> Option<String> {
    let name = path.rsplit('/').next().unwrap_or(path);
    if !name.contains('.') {
        return Some("a file with no extension".to_string());
    }
    let ext = Path::new(name).extension()?.to_str()?.to_ascii_lowercase();
    PROSE_EXTENSIONS
        .contains(&ext.as_str())
        .then(|| format!("a .{ext} file"))
}

#[must_use]
pub fn readers_of<'a>(spec: &'a Spec, path: &str) -> Vec<&'a str> {
    spec.screen
        .as_ref()
        .and_then(|s| s.readers.as_ref())
        .map(|rs| {
            rs.iter()
                .filter(|(p, _)| glob::whole(p, path))
                .map(|(_, r)| r.as_str())
                .collect()
        })
        .unwrap_or_default()
}

pub fn screen(spec: &Spec, items: &[Item], blobs: &[Vec<u8>]) -> Result<Screened> {
    let Some(s) = &spec.screen else {
        return Ok(Screened {
            files: items.len(),
            ..Screened::default()
        });
    };
    let mut bad: Vec<String> = Vec::new();
    let mut done = Screened {
        files: items.len(),
        ..Screened::default()
    };
    let mut allow_used: BTreeSet<(String, String)> = BTreeSet::new();
    let comment_allow = s.comments.clone().unwrap_or_default();
    let mut comment_used = vec![false; comment_allow.len()];
    let mut prose_used = vec![false; s.prose_allow.len()];
    let mut reader_used = vec![false; s.readers.as_ref().map_or(0, BTreeMap::len)];
    for (i, text) in items.iter().zip(blobs) {
        if let Some(rs) = &s.readers {
            let fits: Vec<usize> = rs
                .keys()
                .enumerate()
                .filter(|(_, p)| glob::whole(p, &i.path))
                .map(|(k, _)| k)
                .collect();
            if fits.is_empty() {
                bad.push(format!(
                    "{}: no `screen.readers` rule names what reads it: a release holds only what a compiler, \
                     a build or a test reads, and the texts its licences require",
                    i.path
                ));
            }
            for k in fits {
                mark(&mut reader_used, k);
            }
        }
        for t in &s.refuse {
            if let Some(&at) = references(i.path.as_bytes(), t).first() {
                bad.push(format!(
                    "{}: its path names {t} (`{}`), which a release does not hold",
                    i.path,
                    excerpt(i.path.as_bytes(), at)
                ));
            }
            let at = references(text, t);
            let Some(&first) = at.first() else {
                continue;
            };
            let allowed: Vec<&String> = s
                .allow
                .get(t)
                .map(|ps| ps.iter().filter(|p| glob::whole(p, &i.path)).collect())
                .unwrap_or_default();
            if allowed.is_empty() {
                bad.push(format!(
                    "{}:{}: `{}` names {t}, which a release does not hold",
                    i.path,
                    line_at(text, first),
                    excerpt(text, first)
                ));
            } else {
                done.allowed_references += at.len();
                for p in allowed {
                    allow_used.insert((t.clone(), p.clone()));
                }
            }
        }
        if s.prose == Prose::Overlays
            && !spec.overlay.contains_key(&i.path)
            && let Some(kind) = prose_kind(&i.path)
        {
            let fits: Vec<usize> = s
                .prose_allow
                .iter()
                .enumerate()
                .filter(|(_, p)| glob::whole(p, &i.path))
                .map(|(k, _)| k)
                .collect();
            if fits.is_empty() {
                bad.push(format!(
                    "{}: prose ({kind}) that is not an overlay: a release's prose is its overlays, \
                     and `screen.prose_allow` does not list it",
                    i.path
                ));
            } else {
                done.allowed_prose += 1;
                for k in fits {
                    mark(&mut prose_used, k);
                }
            }
        }
        if s.comments.is_some() && is_c(&i.path) {
            let lexed = msvc6_source::lex::lex(text);
            if let Some(c) = lexed.comments.first() {
                let fits: Vec<usize> = comment_allow
                    .iter()
                    .enumerate()
                    .filter(|(_, p)| glob::whole(p, &i.path))
                    .map(|(k, _)| k)
                    .collect();
                if fits.is_empty() {
                    bad.push(format!(
                        "{}:{}: a comment ({} in the file), outside the spec's comment allowlist",
                        i.path,
                        c.first,
                        lexed.comments.len()
                    ));
                } else {
                    done.allowed_comments += 1;
                    for k in fits {
                        mark(&mut comment_used, k);
                    }
                }
            }
        }
    }
    for (t, ps) in &s.allow {
        for p in ps {
            if !allow_used.contains(&(t.clone(), p.clone())) {
                bad.push(format!(
                    "`screen.allow` lets `{p}` name {t}, and no published file it matches does"
                ));
            }
        }
    }
    for p in unused(&s.prose_allow, &prose_used) {
        bad.push(format!(
            "`screen.prose_allow` lets `{p}` through, and no published prose file it matches needs it"
        ));
    }
    for p in unused(&comment_allow, &comment_used) {
        bad.push(format!(
            "`screen.comments` allows `{p}`, and no published C or C++ file it matches holds a comment"
        ));
    }
    if let Some(rs) = &s.readers {
        for ((p, r), used) in rs.iter().zip(&reader_used) {
            if !used {
                bad.push(format!(
                    "`screen.readers` names `{p}` ({r}), and no published file matches it"
                ));
            }
        }
        done.readers = Some(rs.len());
    }
    if !bad.is_empty() {
        let shown: Vec<&String> = bad.iter().take(SHOWN).collect();
        bail!(
            "the screen refuses {} thing(s){}:\n  {}",
            bad.len(),
            if bad.len() > SHOWN {
                format!(", the first {SHOWN}")
            } else {
                String::new()
            },
            shown
                .iter()
                .map(|b| b.as_str())
                .collect::<Vec<_>>()
                .join("\n  ")
        );
    }
    Ok(done)
}

fn mark(used: &mut [bool], k: usize) {
    if let Some(u) = used.get_mut(k) {
        *u = true;
    }
}

fn unused<'a>(names: &'a [String], used: &[bool]) -> Vec<&'a str> {
    names
        .iter()
        .zip(used)
        .filter(|(_, u)| !**u)
        .map(|(n, _)| n.as_str())
        .collect()
}

pub fn select(spec: &Spec, entries: &[git::Entry]) -> Result<Vec<Item>> {
    let mut inc_used = vec![false; spec.include.len()];
    let mut exc_used = vec![false; spec.exclude.len()];
    let mut out: BTreeMap<String, Item> = BTreeMap::new();
    for e in entries {
        let inc: Vec<usize> = spec
            .include
            .iter()
            .enumerate()
            .filter(|(_, i)| glob::under(i, &e.path))
            .map(|(k, _)| k)
            .collect();
        if inc.is_empty() {
            continue;
        }
        for k in inc {
            mark(&mut inc_used, k);
        }
        let exc: Vec<usize> = spec
            .exclude
            .iter()
            .enumerate()
            .filter(|(_, x)| glob::matches(x, &e.path))
            .map(|(k, _)| k)
            .collect();
        if !exc.is_empty() {
            for k in exc {
                mark(&mut exc_used, k);
            }
            continue;
        }
        if e.kind != "blob" {
            bail!(
                "{}: a {} (mode {}), not a file: a release holds files only",
                e.path,
                e.kind,
                e.mode
            );
        }
        if !REGULAR_MODES.contains(&e.mode.as_str()) {
            bail!(
                "{}: mode {}, not a regular file (100644 or 100755): a symlink would publish its target's path",
                e.path,
                e.mode
            );
        }
        out.insert(
            e.path.clone(),
            Item {
                mode: e.mode.clone(),
                id: e.id.clone(),
                path: e.path.clone(),
                from: e.path.clone(),
            },
        );
    }
    let stale = unused(&spec.include, &inc_used);
    if !stale.is_empty() {
        bail!(
            "`include` entries that name nothing at the commit: {}",
            stale.join(", ")
        );
    }
    let stale = unused(&spec.exclude, &exc_used);
    if !stale.is_empty() {
        bail!(
            "`exclude` patterns that remove nothing: {}",
            stale.join(", ")
        );
    }
    for (to, from) in &spec.overlay {
        if !tree::safe_rel(to) || to.contains('\\') || to.ends_with('/') {
            bail!("overlay `{to}`: not a relative path inside the tree");
        }
        let Some(e) = entries.iter().find(|e| e.path == *from && e.kind == "blob") else {
            bail!("overlay `{to}` = `{from}`: no such file at the commit");
        };
        if !REGULAR_MODES.contains(&e.mode.as_str()) {
            bail!(
                "overlay `{to}` = `{from}`: mode {}, not a regular file (100644 or 100755)",
                e.mode
            );
        }
        if out.contains_key(to) {
            bail!("overlay `{to}`: the commit's own `{to}` is included too");
        }
        out.insert(
            to.clone(),
            Item {
                mode: e.mode.clone(),
                id: e.id.clone(),
                path: to.clone(),
                from: from.clone(),
            },
        );
    }
    let paths: BTreeSet<&str> = out.keys().map(String::as_str).collect();
    for p in &paths {
        if !p.is_ascii() {
            bail!("{p}: a release path must be ASCII");
        }
        let mut at = 0usize;
        while let Some(k) = p.get(at..).and_then(|r| r.find('/')) {
            let dir = p.get(..at + k).unwrap_or_default();
            if paths.contains(dir) {
                bail!("{dir} is published as a file and as the directory holding {p}");
            }
            at += k + 1;
        }
    }
    Ok(out.into_values().collect())
}

#[must_use]
pub fn counts(items: &[Item]) -> Vec<(String, usize)> {
    let mut m: BTreeMap<String, usize> = BTreeMap::new();
    for i in items {
        let top = i.path.split('/').next().unwrap_or(&i.path).to_string();
        *m.entry(top).or_default() += 1;
    }
    m.into_iter().collect()
}

fn git_in(repo: &Path, args: &[&str], env: &[(&str, String)], input: &[u8]) -> Result<Vec<u8>> {
    let mut cmd = Command::new("git");
    cmd.arg("-C")
        .arg(repo)
        .args(args)
        .env_remove("GIT_DIR")
        .env_remove("GIT_WORK_TREE")
        .env_remove("GIT_INDEX_FILE")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    for (k, v) in env {
        cmd.env(k, v);
    }
    let mut child = cmd
        .spawn()
        .with_context(|| format!("running git {}", args.join(" ")))?;
    let mut stdin = child.stdin.take().context("git has no stdin")?;
    let data = input.to_vec();
    let writer = std::thread::spawn(move || stdin.write_all(&data));
    let out = child
        .wait_with_output()
        .with_context(|| format!("reading git {}", args.join(" ")))?;
    writer
        .join()
        .map_err(|_| anyhow!("the git {} writer panicked", args.join(" ")))?
        .with_context(|| format!("writing to git {}", args.join(" ")))?;
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

fn line(bytes: &[u8]) -> String {
    srcio::tool_text(bytes).trim().to_string()
}

fn ok(repo: &Path, args: &[&str]) -> Result<Option<String>> {
    let out = Command::new("git")
        .arg("-C")
        .arg(repo)
        .args(args)
        .env_remove("GIT_DIR")
        .env_remove("GIT_WORK_TREE")
        .output()
        .with_context(|| format!("running git {}", args.join(" ")))?;
    Ok(out.status.success().then(|| line(&out.stdout)))
}

pub fn write_tree(repo: &Path, items: &[Item]) -> Result<String> {
    let index = std::env::temp_dir().join(format!(
        "msvc6t-publish-{}-{}.index",
        std::process::id(),
        items.len()
    ));
    let mut input = Vec::new();
    for i in items {
        input.extend_from_slice(format!("{} {}\t{}", i.mode, i.id, i.path).as_bytes());
        input.push(0);
    }
    let env = [("GIT_INDEX_FILE", index.display().to_string())];
    let made = (|| -> Result<String> {
        git_in(repo, &["read-tree", "--empty"], &env, b"")?;
        git_in(repo, &["update-index", "-z", "--index-info"], &env, &input)?;
        Ok(line(&git_in(repo, &["write-tree"], &env, b"")?))
    })();
    std::fs::remove_file(&index).ok();
    made
}

fn checked_out(repo: &Path, branch: &str) -> Result<bool> {
    let list = git::run(repo, &["worktree", "list", "--porcelain"])?;
    let want = format!("branch refs/heads/{branch}");
    Ok(srcio::tool_text(&list).lines().any(|l| l == want))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Signature {
    pub who: Identity,
    pub time: String,
    pub zone: String,
}

fn signature(line: &str) -> Option<Signature> {
    let (who, when) = line.rsplit_once('>')?;
    let who = Identity::parse(&format!("{who}>")).ok()?;
    let mut when = when.split_whitespace();
    let time = when.next()?.to_string();
    let zone = when.next()?.to_string();
    Some(Signature { who, time, zone })
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tip {
    pub head: String,
    pub tree: String,
    pub parents: usize,
    pub author: Option<Signature>,
    pub committer: Option<Signature>,
    pub message: String,
}

fn header<'a>(head: &'a str, key: &str) -> Option<&'a str> {
    head.lines()
        .find_map(|l| l.strip_prefix(key).and_then(|r| r.strip_prefix(' ')))
}

#[must_use]
pub fn tip_of(raw: &str) -> Tip {
    let (head, message) = raw.split_once("\n\n").unwrap_or((raw, ""));
    Tip {
        head: head.to_string(),
        tree: header(head, "tree").unwrap_or_default().to_string(),
        parents: head.lines().filter(|l| l.starts_with("parent ")).count(),
        author: header(head, "author").and_then(signature),
        committer: header(head, "committer").and_then(signature),
        message: message.to_string(),
    }
}

#[must_use]
pub fn expected_commit(tip: &Tip, tree_id: &str, cut: &Cut) -> String {
    let (author, committer) = match &cut.author {
        Some(a) => {
            let time = tip.author.as_ref().map_or("", |s| s.time.as_str());
            let line = format!("{} {time} {UTC}", a.shown());
            (line.clone(), line)
        }
        None => (
            header(&tip.head, "author").unwrap_or_default().to_string(),
            header(&tip.head, "committer")
                .unwrap_or_default()
                .to_string(),
        ),
    };
    format!(
        "tree {tree_id}\nauthor {author}\ncommitter {committer}\n\n{}",
        cut.message
    )
}

fn first_difference(have: &str, want: &str) -> String {
    let Some(at) = have
        .split('\n')
        .zip(want.split('\n'))
        .position(|(a, b)| a != b)
        .or_else(|| {
            let (h, w) = (have.split('\n').count(), want.split('\n').count());
            (h != w).then_some(h.min(w))
        })
    else {
        return "the two are equal".to_string();
    };
    let show = |t: &str| {
        t.split('\n')
            .nth(at)
            .map_or_else(|| "(its end)".to_string(), |l| format!("`{l}`"))
    };
    format!(
        "line {}: {} where the spec gives {}",
        at + 1,
        show(have),
        show(want)
    )
}

#[must_use]
pub fn tip_problems(tip: &Tip, tree_id: &str, cut: &Cut) -> Vec<String> {
    let mut bad = Vec::new();
    if tip.parents > 0 {
        bad.push(format!(
            "it has {} parent(s): a release branch holds one root commit",
            tip.parents
        ));
    }
    if tip.tree != tree_id {
        bad.push(format!(
            "its tree {} is not the spec's tree {tree_id}",
            tip.tree
        ));
    }
    if let Some(want) = &cut.author {
        for (role, s) in [("author", &tip.author), ("committer", &tip.committer)] {
            match s {
                Some(s) if s.who == *want && s.zone == UTC => {}
                Some(s) => bad.push(format!(
                    "its {role} is `{} {}`, not the spec's `{} {UTC}`",
                    s.who.shown(),
                    s.zone,
                    want.shown()
                )),
                None => bad.push(format!("it has no readable {role}")),
            }
        }
    }
    if tip.message != cut.message {
        bad.push(format!(
            "its message is not the spec's: {}",
            first_difference(&tip.message, &cut.message)
        ));
    }
    bad
}

fn commit_id(repo: &Path, object: &str) -> Result<String> {
    Ok(line(&git_in(
        repo,
        &["hash-object", "-t", "commit", "--literally", "--stdin"],
        &[],
        object.as_bytes(),
    )?))
}

pub fn tip_check(repo: &Path, id: &str, tree_id: &str, cut: &Cut) -> Result<Vec<String>> {
    let raw = srcio::tool_text(&git::run(repo, &["cat-file", "commit", id])?);
    let tip = tip_of(&raw);
    let mut bad = tip_problems(&tip, tree_id, cut);
    if bad.is_empty() {
        let want = expected_commit(&tip, tree_id, cut);
        let want_id = commit_id(repo, &want)?;
        if want_id != id {
            bad.push(format!(
                "it is not the commit the spec gives at its own time, {}: {}",
                short(&want_id),
                first_difference(&raw, &want)
            ));
        }
    }
    Ok(bad)
}

pub fn commit(repo: &Path, branch: &str, tree_id: &str, rev: &str, cut: &Cut) -> Result<Committed> {
    let refname = format!("refs/heads/{branch}");
    git::run(repo, &["check-ref-format", &refname])
        .with_context(|| format!("`{branch}` is not a branch name"))?;
    if checked_out(repo, branch)? {
        bail!(
            "{branch} is checked out in a worktree of {}",
            repo.display()
        );
    }
    let old = ok(repo, &["rev-parse", "--verify", "-q", &refname])?;
    if let Some(old) = &old {
        if let Some(base) = ok(repo, &["merge-base", old, rev])? {
            bail!(
                "{branch} shares history with {rev} (merge base {base}): a release branch holds releases only"
            );
        }
        if tip_check(repo, old, tree_id, cut)?.is_empty() {
            return Ok(Committed::Unchanged(old.clone()));
        }
    }
    let mut env: Vec<(&str, String)> = Vec::new();
    if let Some(a) = &cut.author {
        let date = format!("{} {UTC}", cut.time);
        for (k, v) in [
            ("GIT_AUTHOR_NAME", a.name.clone()),
            ("GIT_AUTHOR_EMAIL", a.email.clone()),
            ("GIT_AUTHOR_DATE", date.clone()),
            ("GIT_COMMITTER_NAME", a.name.clone()),
            ("GIT_COMMITTER_EMAIL", a.email.clone()),
            ("GIT_COMMITTER_DATE", date),
        ] {
            env.push((k, v));
        }
    }
    let new = line(&git_in(
        repo,
        &[
            "-c",
            "i18n.commitEncoding=UTF-8",
            "commit-tree",
            "--no-gpg-sign",
            tree_id,
            "-F",
            "-",
        ],
        &env,
        cut.message.as_bytes(),
    )?);
    let made = tip_check(repo, &new, tree_id, cut)?;
    if !made.is_empty() {
        bail!(
            "the commit {new} git wrote for {branch}: {}",
            made.join("; ")
        );
    }
    git::run(
        repo,
        &["update-ref", &refname, &new, old.as_deref().unwrap_or("")],
    )?;
    Ok(match old {
        None => Committed::Created(new),
        Some(old) => Committed::Replaced { old, new },
    })
}

fn short(id: &str) -> &str {
    id.get(..12).unwrap_or(id)
}

fn now() -> Result<u64> {
    Ok(std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .context("the clock is before 1970")?
        .as_secs())
}

pub fn cut_of(repo: &Path, spec: &Spec, entries: &[git::Entry]) -> Result<Cut> {
    let credits = match &spec.credits {
        Some(c) => {
            let entry = entries
                .iter()
                .find(|e| e.path == *c && e.kind == "blob")
                .with_context(|| format!("`credits` = `{c}`: the commit holds no such file"))?;
            let blob = git::blobs(repo, std::slice::from_ref(&entry.id))?;
            let text = srcio::tool_text(blob.first().map(Vec::as_slice).unwrap_or_default());
            Some(credits_section(&text).with_context(|| format!("`credits` = `{c}`"))?)
        }
        None => None,
    };
    Ok(Cut {
        message: message_with(&spec.message, credits.as_deref()),
        author: spec.author.as_deref().map(Identity::parse).transpose()?,
        time: now()?,
    })
}

pub fn run(a: &PublishArgs) -> Result<Outcome> {
    let repo = &a.repo;
    let rev = git::rev(repo, &a.rev)?;
    let text = git::file_at(repo, &rev, &a.spec)?
        .with_context(|| format!("{} has no {} at {}", repo.display(), a.spec, short(&rev)))?;
    let spec = parse(&srcio::tool_text(&text))
        .with_context(|| format!("{} at {}", a.spec, short(&rev)))?;
    let entries = git::ls_tree(repo, &rev)?;
    let items =
        select(&spec, &entries).with_context(|| format!("{} at {}", a.spec, short(&rev)))?;
    let screened = if spec.screen.is_some() {
        let ids: Vec<String> = items.iter().map(|i| i.id.clone()).collect();
        let blobs = git::blobs(repo, &ids)?;
        Some(
            screen(&spec, &items, &blobs)
                .with_context(|| format!("{} at {}", a.spec, short(&rev)))?,
        )
    } else {
        None
    };
    let cut =
        cut_of(repo, &spec, &entries).with_context(|| format!("{} at {}", a.spec, short(&rev)))?;
    let tree_id = write_tree(repo, &items)?;
    if a.list {
        for i in &items {
            let from = if i.from == i.path {
                String::new()
            } else {
                format!(" <- {}", i.from)
            };
            let read = readers_of(&spec, &i.path);
            let read = if read.is_empty() {
                String::new()
            } else {
                format!("\t{}", read.join("; "))
            };
            println!("{}{from}{read}", i.path);
        }
    }
    println!(
        "publish: {} at {}: {} file(s), tree {tree_id}",
        a.spec,
        short(&rev),
        items.len()
    );
    let tops: Vec<String> = counts(&items)
        .into_iter()
        .map(|(t, n)| format!("{t} {n}"))
        .collect();
    println!("publish:   {}", tops.join(", "));
    match &screened {
        Some(s) => println!(
            "publish: screen: {} file(s) clean{}; {} allowed reference(s), {} allowed prose file(s), {} allowed \
             commented C or C++ file(s)",
            s.files,
            s.readers
                .map(|n| format!(", each with a reader {n} `screen.readers` rule(s) name"))
                .unwrap_or_default(),
            s.allowed_references,
            s.allowed_prose,
            s.allowed_comments
        ),
        None => println!("publish: screen: none (the spec has no [screen] table)"),
    }
    println!(
        "publish: commit: {}, message `{}`{}",
        cut.author.as_ref().map_or_else(
            || "git's own identity".to_string(),
            |i| format!("{} {UTC}", i.shown())
        ),
        spec.message.trim(),
        spec.credits
            .as_ref()
            .map(|c| format!(" and {c}'s {CREDITS} section"))
            .unwrap_or_default()
    );
    let Some(b) = &a.branch else {
        return Ok(Outcome::Ok);
    };
    if a.check {
        let tip = ok(
            repo,
            &["rev-parse", "--verify", "-q", &format!("refs/heads/{b}")],
        )?
        .with_context(|| format!("{} has no branch {b}", repo.display()))?;
        let bad = tip_check(repo, &tip, &tree_id, &cut)?;
        if !bad.is_empty() {
            bail!(
                "{b} at {}: {} problem(s):\n  {}",
                short(&tip),
                bad.len(),
                bad.join("\n  ")
            );
        }
        println!(
            "publish: {b} at {tip}: one root commit, the spec's identity, message and tree, and no other header"
        );
        return Ok(Outcome::Ok);
    }
    match commit(repo, b, &tree_id, &rev, &cut)? {
        Committed::Created(new) => println!("publish: {b} created at {new}"),
        Committed::Replaced { old, new } => {
            println!(
                "publish: {b} replaced: {} -> {new}, one root commit",
                short(&old)
            );
        }
        Committed::Unchanged(old) => {
            println!("publish: {b} unchanged at {old}: its tip is this release");
        }
    }
    Ok(Outcome::Ok)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn entry(path: &str, kind: &str) -> git::Entry {
        git::Entry {
            mode: if kind == "blob" { "100644" } else { "160000" }.to_string(),
            kind: kind.to_string(),
            id: format!("{:0>40}", path.len()),
            path: path.to_string(),
        }
    }

    fn tree_of(paths: &[&str]) -> Vec<git::Entry> {
        paths.iter().map(|p| entry(p, "blob")).collect()
    }

    fn spec(text: &str) -> Spec {
        parse(text).unwrap()
    }

    const BASE: &str = "message = \"r\"\ninclude = [\"Cargo.toml\", \"crates\", \"examples/\"]\n\
                        exclude = [\"examples/*/README.md\"]\n\
                        [overlay]\n\"README.md\" = \"release/README.md\"\n";

    #[test]
    fn the_spec_picks_its_files_and_overlays_its_own() {
        let t = tree_of(&[
            "Cargo.toml",
            "README.md",
            "crates/a/Cargo.toml",
            "crates/a/src/lib.rs",
            "docs/x.md",
            "examples/p/README.md",
            "examples/p/cfg.toml",
            "release/README.md",
            "cratesx/a.rs",
        ]);
        let items = select(&spec(BASE), &t).unwrap();
        let got: Vec<(&str, &str)> = items
            .iter()
            .map(|i| (i.path.as_str(), i.from.as_str()))
            .collect();
        assert_eq!(
            got,
            [
                ("Cargo.toml", "Cargo.toml"),
                ("README.md", "release/README.md"),
                ("crates/a/Cargo.toml", "crates/a/Cargo.toml"),
                ("crates/a/src/lib.rs", "crates/a/src/lib.rs"),
                ("examples/p/cfg.toml", "examples/p/cfg.toml"),
            ]
        );
        let readme = items.iter().find(|i| i.path == "README.md").unwrap();
        assert_eq!(readme.id, entry("release/README.md", "blob").id);
        assert_eq!(
            counts(&items),
            [
                ("Cargo.toml".to_string(), 1),
                ("README.md".to_string(), 1),
                ("crates".to_string(), 2),
                ("examples".to_string(), 1)
            ]
        );
    }

    #[test]
    fn a_stale_or_unsafe_spec_is_refused() {
        let t = tree_of(&[
            "Cargo.toml",
            "crates/a/src/lib.rs",
            "examples/p/README.md",
            "examples/p/cfg.toml",
            "release/README.md",
        ]);
        let refused = |text: &str| select(&spec(text), &t).unwrap_err().to_string();
        let e = refused("message = \"r\"\ninclude = [\"crates\", \"gone\"]\n");
        assert!(e.contains("name nothing at the commit: gone"), "{e}");
        let e = refused("message = \"r\"\ninclude = [\"crates\"]\nexclude = [\"*.md\"]\n");
        assert!(e.contains("remove nothing: *.md"), "{e}");
        let e = refused(
            "message = \"r\"\ninclude = [\"crates\"]\n[overlay]\n\"README.md\" = \"release/NONE.md\"\n",
        );
        assert!(e.contains("no such file at the commit"), "{e}");
        let e = refused(
            "message = \"r\"\ninclude = [\"Cargo.toml\"]\n[overlay]\n\"Cargo.toml\" = \"release/README.md\"\n",
        );
        assert!(e.contains("is included too"), "{e}");
        let e = refused(
            "message = \"r\"\ninclude = [\"Cargo.toml\"]\n[overlay]\n\"../x\" = \"release/README.md\"\n",
        );
        assert!(e.contains("not a relative path"), "{e}");
        let e = refused(
            "message = \"r\"\ninclude = [\"crates\"]\n[overlay]\n\"crates/a\" = \"release/README.md\"\n",
        );
        assert!(e.contains("as a file and as the directory"), "{e}");
        let mut sub = t.clone();
        sub.push(entry("crates/vendor", "commit"));
        let e = select(&spec("message = \"r\"\ninclude = [\"crates\"]\n"), &sub)
            .unwrap_err()
            .to_string();
        assert!(e.contains("crates/vendor: a commit"), "{e}");
        let mut link = t.clone();
        let mut l = entry("crates/link.rs", "blob");
        l.mode = "120000".to_string();
        link.push(l);
        let e = select(&spec("message = \"r\"\ninclude = [\"crates\"]\n"), &link)
            .unwrap_err()
            .to_string();
        assert!(
            e.contains("crates/link.rs: mode 120000, not a regular file"),
            "{e}"
        );
        let e = select(
            &spec("message = \"r\"\ninclude = [\"Cargo.toml\"]\n[overlay]\n\"L\" = \"crates/link.rs\"\n"),
            &link,
        )
        .unwrap_err()
        .to_string();
        assert!(e.contains("mode 120000, not a regular file"), "{e}");
        let mut exec = t.clone();
        let mut x = entry("crates/run.rs", "blob");
        x.mode = "100755".to_string();
        exec.push(x);
        assert!(select(&spec("message = \"r\"\ninclude = [\"crates\"]\n"), &exec).is_ok());
    }

    fn screened(spec_text: &str, files: &[(&str, &str)]) -> Result<Screened> {
        let s = spec(spec_text);
        let items: Vec<Item> = files
            .iter()
            .map(|(p, _)| Item {
                mode: "100644".to_string(),
                id: String::new(),
                path: (*p).to_string(),
                from: (*p).to_string(),
            })
            .collect();
        let blobs: Vec<Vec<u8>> = files.iter().map(|(_, t)| t.as_bytes().to_vec()).collect();
        screen(&s, &items, &blobs)
    }

    const SCREEN: &str = "message = \"r\"\ninclude = [\"a\"]\n\
                          [overlay]\n\"README.md\" = \"release/README.md\"\n\
                          [screen]\nrefuse = [\"/home/\", \"C:\\\\Users\\\\\", \"~/\", \"notes/\", \"docs/\"]\n\
                          prose = \"overlays\"\nprose_allow = [\"examples/*/denylist.txt\"]\n\
                          comments = [\"fixtures/lex/*.c\"]\n\
                          [screen.allow]\n\"docs/\" = [\"crates/lint/src/rules.rs\"]\n";

    fn refusal(files: &[(&str, &str)]) -> String {
        screened(SCREEN, files).unwrap_err().to_string()
    }

    const CLEAN: [(&str, &str); 5] = [
        (
            "crates/lint/src/rules.rs",
            "const D: &str = \"docs/x.md\";\n",
        ),
        (
            "fixtures/lex/brace.c",
            "/* } */ int f(void) { return 0; }\n",
        ),
        ("README.md", "# Release\n"),
        ("examples/p/denylist.txt", "Golfer\n"),
        (".gitignore", "/target\n"),
    ];

    #[test]
    fn a_clean_tree_passes_the_screen_with_its_allowances_counted() {
        let mut files = CLEAN.to_vec();
        files.push((
            "crates/a/src/lib.rs",
            "pub const R: &str = \"docs/\"; // footnotes/x\n",
        ));
        files.push(("fixtures/c/plain.c", "int g(void) { return 1; }\n"));
        files.push((
            "crates/a/src/win.rs",
            "const W: &str = \"C:\\\\Users\\\\ \"; const T: &str = \"~/\";\n",
        ));
        let s = screened(SCREEN, &files).unwrap();
        assert_eq!(
            s,
            Screened {
                files: 8,
                allowed_references: 1,
                allowed_prose: 1,
                allowed_comments: 1,
                readers: None
            }
        );
        assert!(
            screened(
                "message = \"r\"\ninclude = [\"a\"]\n",
                &[("x.md", "/home/me\n")]
            )
            .is_ok()
        );
    }

    #[test]
    fn a_home_path_or_a_notes_reference_is_refused() {
        let mut files = CLEAN.to_vec();
        files.push((
            "crates/a/src/lib.rs",
            "pub fn f() {}\nconst P: &str = \"/home/me/x\";\n",
        ));
        let e = refusal(&files);
        assert!(
            e.contains("crates/a/src/lib.rs:2: `/home/me/x` names /home/"),
            "{e}"
        );
        let mut files = CLEAN.to_vec();
        files.push(("examples/p/t.tsv", "a\tcensus: notes/golf/x.md\n"));
        let e = refusal(&files);
        assert!(
            e.contains("examples/p/t.tsv:1: `notes/golf/x.md` names notes/"),
            "{e}"
        );
    }

    #[test]
    fn a_docs_reference_is_refused_outside_its_allowlist_and_a_stale_allowance_too() {
        let mut files = CLEAN.to_vec();
        files.push(("crates/t/src/cli.rs", "help = \"see docs/y.md\"\n"));
        let e = refusal(&files);
        assert!(
            e.contains("crates/t/src/cli.rs:1: `docs/y.md` names docs/"),
            "{e}"
        );
        let files: Vec<(&str, &str)> = CLEAN
            .iter()
            .copied()
            .filter(|(p, _)| !p.ends_with("rules.rs"))
            .collect();
        let e = refusal(&files);
        assert!(
            e.contains(
                "lets `crates/lint/src/rules.rs` name docs/, and no published file it matches does"
            ),
            "{e}"
        );
    }

    #[test]
    fn a_windows_home_path_is_refused_however_its_backslashes_are_escaped_or_cased() {
        for (text, shown) in [
            (
                "p = \"C:\\\\Users\\\\x\\\\a.dll\"\n",
                "C:\\\\Users\\\\x\\\\a.dll",
            ),
            ("const P: &str = r\"C:\\Users\\x\";\n", "C:\\Users\\x"),
            ("p = 'c:\\users\\me'\n", "c:\\users\\me"),
            (
                "p = \"C:\\\\\\\\USERS\\\\\\\\me\"\n",
                "C:\\\\\\\\USERS\\\\\\\\me",
            ),
        ] {
            let mut files = CLEAN.to_vec();
            files.push(("crates/a/src/lib.rs", text));
            let e = refusal(&files);
            assert!(
                e.contains(&format!(
                    "crates/a/src/lib.rs:1: `{shown}` names C:\\Users\\"
                )),
                "{text}: {e}"
            );
        }
    }

    #[test]
    fn a_tilde_home_path_is_refused() {
        let mut files = CLEAN.to_vec();
        files.push(("examples/p/cfg.toml", "a = 1\ndir = \"~/src/x\"\n"));
        let e = refusal(&files);
        assert!(
            e.contains("examples/p/cfg.toml:2: `~/src/x` names ~/"),
            "{e}"
        );
    }

    #[test]
    fn prose_that_is_not_an_overlay_is_refused_outside_its_allowlist() {
        for (path, kind) in [
            ("examples/p/README.md", "a .md file"),
            ("docs/x.markdown", "a .markdown file"),
            ("docs/x.RST", "a .rst file"),
            ("examples/p/notes.txt", "a .txt file"),
            ("examples/p/NOTES", "a file with no extension"),
        ] {
            let mut files = CLEAN.to_vec();
            files.push((path, "x\n"));
            let e = refusal(&files);
            assert!(
                e.contains(&format!("{path}: prose ({kind}) that is not an overlay")),
                "{path}: {e}"
            );
        }
        let files: Vec<(&str, &str)> = CLEAN
            .iter()
            .copied()
            .filter(|(p, _)| !p.ends_with(".txt"))
            .collect();
        let e = refusal(&files);
        assert!(
            e.contains(
                "`screen.prose_allow` lets `examples/*/denylist.txt` through, and no published prose file it matches needs it"
            ),
            "{e}"
        );
        assert_eq!(prose_kind(".gitignore"), None);
        assert_eq!(prose_kind("crates/a/Cargo.toml"), None);
        assert_eq!(
            prose_kind("LICENSE").as_deref(),
            Some("a file with no extension")
        );
    }

    #[test]
    fn a_comment_outside_the_allowlist_is_refused_and_a_stale_allowance_too() {
        let mut files = CLEAN.to_vec();
        files.push(("fixtures/c/iso.cpp", "int a;\n// why\nint b;\n"));
        let e = refusal(&files);
        assert!(
            e.contains("fixtures/c/iso.cpp:2: a comment (1 in the file)"),
            "{e}"
        );
        let files: Vec<(&str, &str)> = CLEAN
            .iter()
            .copied()
            .filter(|(p, _)| !p.ends_with(".c"))
            .collect();
        let e = refusal(&files);
        assert!(
            e.contains("allows `fixtures/lex/*.c`, and no published C or C++ file it matches holds a comment"),
            "{e}"
        );
    }

    const READERS: &str = "message = \"r\"\ninclude = [\"a\"]\n\
                           [overlay]\n\"LICENSE\" = \"release/LICENSE\"\n\
                           [screen]\nprose = \"overlays\"\n\
                           [screen.readers]\n\"Cargo.toml\" = \"cargo\"\n\"Cargo.lock\" = \"cargo\"\n\
                           \"rust-toolchain.toml\" = \"rustup\"\n\"crates/*/Cargo.toml\" = \"cargo: a member\"\n\
                           \"crates/**/*.rs\" = \"rustc\"\n\
                           \"LICENSE\" = \"the licence\"\n\"fixtures/c/*.c\" = \"crates/a/tests/vc6.rs\"\n";

    const READ: [(&str, &str); 7] = [
        ("Cargo.toml", "[workspace]\n"),
        ("Cargo.lock", "version = 4\n"),
        ("rust-toolchain.toml", "[toolchain]\n"),
        ("LICENSE", "Licence text\n"),
        ("crates/a/Cargo.toml", "[package]\n"),
        ("crates/a/src/lib.rs", "pub fn a() {}\n"),
        ("fixtures/c/a.c", "int a;\n"),
    ];

    #[test]
    fn every_published_file_needs_a_rule_naming_its_reader() {
        let s = screened(READERS, &READ).unwrap();
        assert_eq!(s.files, 7);
        assert_eq!(s.readers, Some(7));
        let spec = spec(READERS);
        assert_eq!(readers_of(&spec, "crates/a/src/x/y.rs"), ["rustc"]);
        assert_eq!(readers_of(&spec, "Cargo.toml"), ["cargo"]);
        assert_eq!(
            readers_of(&spec, "crates/a/Cargo.toml"),
            ["cargo: a member"]
        );
        assert!(readers_of(&spec, "README.md").is_empty());
        for (path, text) in [
            ("README.md", "# Tool\n\nHow to build it.\n"),
            ("docs/DESIGN.md", "# Design\n"),
            ("crates/a/README.md", "# a\n"),
            ("examples/p/notes.toml", "why = \"x\"\n"),
            ("crates/a/Cargo.lock", "version = 4\n"),
            ("fixtures/rust-toolchain.toml", "[toolchain]\n"),
            ("crates/a/b/Cargo.toml", "[package]\n"),
            ("fixtures/c/sub/LICENSE", "Licence text\n"),
            ("crates/a/CARGO.TOML", "[package]\n"),
            ("fixtures/c/A.C", "int a;\n"),
        ] {
            let mut files = READ.to_vec();
            files.push((path, text));
            let e = screened(READERS, &files).unwrap_err().to_string();
            assert!(
                e.contains(&format!(
                    "{path}: no `screen.readers` rule names what reads it: a release holds only what a \
                     compiler, a build or a test reads"
                )),
                "{path}: {e}"
            );
        }
        let overlaid = READERS.replace(
            "[overlay]\n",
            "[overlay]\n\"README.md\" = \"release/README.md\"\n",
        );
        let mut files = READ.to_vec();
        files.push(("README.md", "# Release\n"));
        let e = screened(&overlaid, &files).unwrap_err().to_string();
        assert!(
            e.contains("README.md: no `screen.readers` rule names what reads it"),
            "{e}"
        );
        assert!(!e.contains("prose"), "{e}");
        let files: Vec<(&str, &str)> = READ
            .iter()
            .copied()
            .filter(|(p, _)| !p.ends_with(".c"))
            .collect();
        let e = screened(READERS, &files).unwrap_err().to_string();
        assert!(
            e.contains(
                "`screen.readers` names `fixtures/c/*.c` (crates/a/tests/vc6.rs), and no published file matches it"
            ),
            "{e}"
        );
        let e = parse(&READERS.replace("\"rustc\"", "\" \""))
            .unwrap_err()
            .to_string();
        assert!(
            e.contains("a rule is a pattern and the reader it names"),
            "{e}"
        );
        let empty = "message = \"r\"\ninclude = [\"a\"]\n[screen]\n[screen.readers]\n";
        let e = screened(empty, &READ).unwrap_err().to_string();
        assert!(e.contains("the screen refuses 7 thing(s)"), "{e}");
        assert!(
            e.contains("LICENSE: no `screen.readers` rule names what reads it"),
            "{e}"
        );
        assert_eq!(
            screened("message = \"r\"\ninclude = [\"a\"]\n[screen]\n", &READ)
                .unwrap()
                .readers,
            None
        );
    }

    #[test]
    fn a_screen_pattern_without_a_slash_names_a_root_file_only() {
        let spec = "message = \"r\"\ninclude = [\"a\"]\n[screen]\nrefuse = [\"docs/\"]\n\
                    prose = \"overlays\"\nprose_allow = [\"denylist.txt\"]\ncomments = [\"*.c\"]\n\
                    [screen.allow]\n\"docs/\" = [\"rules.rs\"]\n";
        let at_root = [
            ("rules.rs", "const D: &str = \"docs/x.md\";\n"),
            ("denylist.txt", "Golfer\n"),
            ("brace.c", "/* x */ int f;\n"),
        ];
        let s = screened(spec, &at_root).unwrap();
        assert_eq!(
            (s.allowed_references, s.allowed_prose, s.allowed_comments),
            (1, 1, 1)
        );
        let deeper = [
            (
                "crates/lint/src/rules.rs",
                "const D: &str = \"docs/x.md\";\n",
            ),
            ("examples/p/denylist.txt", "Golfer\n"),
            ("fixtures/lex/brace.c", "/* x */ int f;\n"),
        ];
        let e = screened(spec, &deeper).unwrap_err().to_string();
        for want in [
            "crates/lint/src/rules.rs:1: `docs/x.md` names docs/",
            "examples/p/denylist.txt: prose (a .txt file) that is not an overlay",
            "fixtures/lex/brace.c:1: a comment (1 in the file)",
            "lets `rules.rs` name docs/, and no published file it matches does",
        ] {
            assert!(e.contains(want), "{want}: {e}");
        }
    }

    #[test]
    fn an_allowance_for_a_text_the_screen_does_not_refuse_is_refused() {
        let e = parse("message = \"r\"\ninclude = [\"a\"]\n[screen]\nrefuse = [\"x/\"]\n[screen.allow]\n\"y/\" = [\"a\"]\n")
            .unwrap_err()
            .to_string();
        assert!(e.contains("`screen.allow` names `y/`"), "{e}");
        assert!(parse("message = \"r\"\ninclude = [\"a\"]\n[screen]\nprose = \"some\"\n").is_err());
        assert!(
            parse("message = \"r\"\ninclude = [\"a\"]\n[screen]\nmarkdown = \"overlays\"\n")
                .is_err()
        );
        let e = parse("message = \"r\"\ninclude = [\"a\"]\n[screen]\nprose_allow = [\"a.txt\"]\n")
            .unwrap_err()
            .to_string();
        assert!(e.contains("only `prose = \"overlays\"` refuses any"), "{e}");
    }

    #[test]
    fn a_reference_is_a_text_followed_by_a_name() {
        assert_eq!(references(b"docs/ a docs/x mydocs/y docs/", "docs/"), [8]);
        assert_eq!(references(b"/srv/home/me /home/", "/home/"), [4]);
        assert!(references(b"anything", "").is_empty());
        let win = "C:\\Users\\";
        assert_eq!(references(b"C:\\Users\\x", win), [0]);
        assert_eq!(references(b"\"C:\\\\Users\\\\x\"", win), [1]);
        assert_eq!(references(b"c:\\users\\x C:\\USERS\\\\\\y", win), [0, 11]);
        assert!(references(b"C:\\Users\\ XC:\\Users\\x C:Users\\x C:\\Usersx", win).is_empty());
        assert_eq!(references(b"/Data/x", "/data/"), Vec::<usize>::new());
        assert_eq!(references(b"~/a ~/ \"~/\" ~/.cache", "~/"), [0, 12]);
    }

    #[test]
    fn a_spec_must_say_what_it_publishes() {
        assert!(parse("message = \"r\"\ninclude = []\n").is_err());
        assert!(parse("message = \" \"\ninclude = [\"a\"]\n").is_err());
        assert!(parse("include = [\"a\"]\n").is_err());
        let e = parse("message = \"r\"\ninclude = [\"a\"]\nincludes = [\"b\"]\n")
            .unwrap_err()
            .to_string();
        assert!(e.contains("includes"), "{e}");
        let e = parse("message = \"r\"\nauthor = \"nobody\"\ninclude = [\"a\"]\n")
            .unwrap_err()
            .to_string();
        assert!(e.contains("`author`"), "{e}");
        let e = parse("message = \"r\"\ncredits = \"../R.md\"\ninclude = [\"a\"]\n")
            .unwrap_err()
            .to_string();
        assert!(e.contains("`credits` = `../R.md`"), "{e}");
    }

    #[test]
    fn an_identity_is_a_name_and_an_email() {
        let i = Identity::parse("Anonymous <anonymous@example.invalid>").unwrap();
        assert_eq!(i.name, "Anonymous");
        assert_eq!(i.email, "anonymous@example.invalid");
        assert_eq!(i.shown(), "Anonymous <anonymous@example.invalid>");
        for bad in [
            "Anonymous",
            "<a@b>",
            "A <>",
            "A <a b@c>",
            "A <a@b",
            "A <a@b> x",
        ] {
            assert!(Identity::parse(bad).is_err(), "{bad}");
        }
    }

    #[test]
    fn the_message_carries_the_credits_section_verbatim() {
        let readme = "# T\n\nIntro.\n\n## Credits\n\n- [a](https://a.example): one.\n- b: two.\n\n### Thanks\n\n- c\n\n## Licence\n\nGPL\n";
        let c = credits_section(readme).unwrap();
        assert_eq!(
            c,
            "- [a](https://a.example): one.\n- b: two.\n\n### Thanks\n\n- c\n"
        );
        assert_eq!(
            message_with("Initial commit", Some(&c)),
            format!("Initial commit\n\n{c}")
        );
        assert_eq!(message_with(" Release ", None), "Release\n");
        assert_eq!(credits_section("# T\n### Credits\n- x\n").unwrap(), "- x\n");
        assert_eq!(
            credits_section("## Credits\n\n```\n# not a heading\n```\n- x\n\n# Next\n").unwrap(),
            "```\n# not a heading\n```\n- x\n"
        );
        assert!(
            credits_section("# T\n## Credit\n- x\n")
                .unwrap_err()
                .to_string()
                .contains("no `Credits` heading")
        );
        assert!(
            credits_section("## Credits\n\n## Next\n")
                .unwrap_err()
                .to_string()
                .contains("section is empty")
        );
    }

    fn cut(message: &str) -> Cut {
        Cut {
            message: message.to_string(),
            author: Some(Identity::parse("Anonymous <anonymous@example.invalid>").unwrap()),
            time: 0,
        }
    }

    #[test]
    fn a_tip_is_one_root_commit_by_the_spec_s_identity_with_its_message() {
        let good = "tree t1\nauthor Anonymous <anonymous@example.invalid> 1791472443 +0000\n\
                    committer Anonymous <anonymous@example.invalid> 1791472443 +0000\n\nInitial commit\n\n- a\n";
        let tip = tip_of(good);
        assert_eq!(tip.tree, "t1");
        assert_eq!(tip.parents, 0);
        assert_eq!(tip.message, "Initial commit\n\n- a\n");
        let want = cut("Initial commit\n\n- a\n");
        assert!(tip_problems(&tip, "t1", &want).is_empty());
        let bad = "tree t2\nparent p1\nauthor J Doe <jdoe@example.com> 1791472443 +0200\n\
                   committer Anonymous <anonymous@example.invalid> 1791472443 +0100\n\nInitial commit\n\n- b\n";
        let got = tip_problems(&tip_of(bad), "t1", &want);
        let has = |s: &str| got.iter().any(|b| b.contains(s));
        assert!(has("it has 1 parent(s)"), "{got:?}");
        assert!(has("its tree t2 is not the spec's tree t1"), "{got:?}");
        assert!(
            has(
                "its author is `J Doe <jdoe@example.com> +0200`, not the spec's `Anonymous <anonymous@example.invalid> +0000`"
            ),
            "{got:?}"
        );
        assert!(
            has("its committer is `Anonymous <anonymous@example.invalid> +0100`"),
            "{got:?}"
        );
        assert!(
            has("its message is not the spec's: line 3: `- b` where the spec gives `- a`"),
            "{got:?}"
        );
        let short = tip_problems(&tip_of(good), "t1", &cut("Initial commit\n\n- a\n\n"));
        assert!(
            short
                .iter()
                .any(|b| b.contains("line 5: (its end) where the spec gives ``")),
            "{short:?}"
        );
        let anyone = Cut {
            author: None,
            ..cut("Initial commit\n\n- b\n")
        };
        assert_eq!(tip_problems(&tip_of(bad), "t2", &anyone).len(), 1);
    }

    #[test]
    fn a_tip_is_the_commit_the_spec_gives_at_its_own_time_and_no_other_header() {
        let good = "tree t1\nauthor Anonymous <anonymous@example.invalid> 1791472443 +0000\n\
                    committer Anonymous <anonymous@example.invalid> 1791472443 +0000\n\nInitial commit\n\n- a\n";
        let want = cut("Initial commit\n\n- a\n");
        assert_eq!(expected_commit(&tip_of(good), "t1", &want), good);
        for extra in [
            "gpgsig -----BEGIN PGP SIGNATURE-----\n \n iQEzBAABCAAdFiEE\n -----END PGP SIGNATURE-----\n",
            "encoding ISO-8859-1\n",
            "mergetag object 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n type commit\n tag v1\n \n v1\n",
        ] {
            let planted = good.replacen("\n\n", &format!("\n{extra}\n"), 1);
            let tip = tip_of(&planted);
            assert!(tip_problems(&tip, "t1", &want).is_empty(), "{extra}");
            assert_eq!(tip.message, "Initial commit\n\n- a\n", "{extra}");
            let rebuilt = expected_commit(&tip, "t1", &want);
            assert_eq!(rebuilt, good, "{extra}");
            let first = extra.lines().next().unwrap_or_default();
            assert_eq!(
                first_difference(&planted, &rebuilt),
                format!("line 4: `{first}` where the spec gives ``")
            );
        }
        let later = good.replacen("1791472443 +0000\n\n", "1791472444 +0000\n\n", 1);
        assert_ne!(expected_commit(&tip_of(&later), "t1", &want), later);
        let anyone = Cut {
            author: None,
            ..want
        };
        let theirs = good.replace(
            "Anonymous <anonymous@example.invalid>",
            "J Doe <jdoe@example.com>",
        );
        assert_eq!(expected_commit(&tip_of(&theirs), "t1", &anyone), theirs);
    }

    #[test]
    fn a_word_is_refused_anywhere_and_in_any_case() {
        assert_eq!(
            references(
                b"jdoe JDoe /home/jdoe/x jdoes xjdoe jdoe_x jdoe.example",
                "jdoe"
            ),
            [0, 5, 16, 23, 30, 35, 42]
        );
        assert_eq!(
            references(
                b"C:\\Documents and Settings\\me c:/documents and settings/x",
                "Documents and Settings"
            ),
            [3, 32]
        );
        assert!(is_word("Jdoe") && is_word("Documents and Settings"));
        assert!(
            is_word("box-zz")
                && is_word(".acme")
                && is_word("zz-east-9")
                && is_word("acme.example")
        );
        assert!(!is_word("docs/") && !is_word("") && !is_word("a  b") && !is_word(" a"));
        assert!(!is_word("C:/Users/") && !is_word("C:\\Users\\") && !is_word("~/"));
        let mut files = CLEAN.to_vec();
        files.push(("crates/a/Cargo.toml", "authors = [\"J. Q. Jdoe\"]\n"));
        let e = screened(
            &SCREEN.replace("\"docs/\"]\n", "\"docs/\", \"jdoe\"]\n"),
            &files,
        )
        .unwrap_err()
        .to_string();
        assert!(
            e.contains("crates/a/Cargo.toml:1: `Jdoe` names jdoe"),
            "{e}"
        );
    }

    const HOMES: &str = "\"docs/\", \"C:/Users/\", \"Documents and Settings\", \"/root/\", \"jdoe\", \"acme\", \
                         \"box-zz\", \".zzcloud\", \"zz-east-9\"]\n";

    #[test]
    fn every_home_form_and_an_identity_word_joined_to_another_are_refused() {
        assert_eq!(
            references(
                b"c:/users/bob C:/USERS/x xc:/users/y C:/Users/",
                "C:/Users/"
            ),
            [0, 13]
        );
        for (text, shown, t) in [
            (
                "p = \"c:/users/bob/a.dll\"\n",
                "c:/users/bob/a.dll",
                "C:/Users/",
            ),
            (
                "p = 'C:\\Documents and Settings\\me'\n",
                "Documents",
                "Documents and Settings",
            ),
            ("p = \"/root/.cargo\"\n", "/root/.cargo", "/root/"),
            ("host = \"jdoe_dev\"\n", "jdoe_dev", "jdoe"),
            ("host = \"box.acme_corp\"\n", "acme_corp", "acme"),
            ("host = \"AcmeBox\"\n", "AcmeBox", "acme"),
            ("host = \"dev-box-zz-01\"\n", "box-zz-01", "box-zz"),
            ("host = \"dev-BOX-ZZ\"\n", "BOX-ZZ", "box-zz"),
            (
                "p = \"/srv/.zzcloud/config\"\n",
                ".zzcloud/config",
                ".zzcloud",
            ),
            ("region = \"zz-east-9\"\n", "zz-east-9", "zz-east-9"),
        ] {
            let mut files = CLEAN.to_vec();
            files.push(("crates/a/src/lib.rs", text));
            let e = screened(&SCREEN.replace("\"docs/\"]\n", HOMES), &files)
                .unwrap_err()
                .to_string();
            assert!(
                e.contains(&format!("crates/a/src/lib.rs:1: `{shown}` names {t}")),
                "{text}: {e}"
            );
        }
    }

    #[test]
    fn a_path_or_an_overlay_name_that_names_a_refused_text_is_refused() {
        let mut files = CLEAN.to_vec();
        files.push(("crates/base/src/notes/jdoe.rs", "pub fn f() {}\n"));
        let e = screened(&SCREEN.replace("\"docs/\"]\n", HOMES), &files)
            .unwrap_err()
            .to_string();
        assert!(
            e.contains(
                "crates/base/src/notes/jdoe.rs: its path names notes/ (`notes/jdoe.rs`), which a release does not hold"
            ),
            "{e}"
        );
        assert!(
            e.contains("crates/base/src/notes/jdoe.rs: its path names jdoe (`jdoe.rs`)"),
            "{e}"
        );
        let s = spec(&SCREEN.replace(
            "\"README.md\" = \"release/README.md\"",
            "\"README.md\" = \"release/README.md\"\n\"notes/README.md\" = \"release/README.md\"",
        ));
        let mut items: Vec<Item> = CLEAN
            .iter()
            .map(|(p, _)| Item {
                mode: "100644".to_string(),
                id: String::new(),
                path: (*p).to_string(),
                from: (*p).to_string(),
            })
            .collect();
        items.push(Item {
            mode: "100644".to_string(),
            id: String::new(),
            path: "notes/README.md".to_string(),
            from: "release/README.md".to_string(),
        });
        let mut blobs: Vec<Vec<u8>> = CLEAN.iter().map(|(_, t)| t.as_bytes().to_vec()).collect();
        blobs.push(b"# Release\n".to_vec());
        let e = screen(&s, &items, &blobs).unwrap_err().to_string();
        assert!(
            e.contains("notes/README.md: its path names notes/ (`notes/README.md`)"),
            "{e}"
        );
        assert!(!e.contains("prose"), "{e}");
    }
}
