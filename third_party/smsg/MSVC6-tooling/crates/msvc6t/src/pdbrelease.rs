use crate::Outcome;
use crate::pdb::{label, policy_of, read};
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::Args;
use msvc6_base::digest::{md5_hex, sha256_hex};
use msvc6_base::srcio;
use msvc6_pdb::release::{self, Rev};
use msvc6_pdb::{attach, eq, llvm, paths, structure, to70};
use std::path::{Component, Path, PathBuf};

const TMP: &str = ".msvc6t-release-tmp";

#[derive(Debug, Args)]
pub struct ReleaseArgs {
    #[arg(
        value_name = "OUT",
        help = "Where the PDBs, their 7.0 twins (OUT/7.0), ATTACH.md and MANIFEST.tsv go."
    )]
    pub out: PathBuf,
    #[arg(
        long = "image",
        value_name = "ID",
        help = "An [[image]] id with an [image.pdb] file, in the order the files are listed. Default: every one, in config order."
    )]
    pub images: Vec<String>,
    #[arg(
        long = "rev",
        value_name = "NAME=COMMIT",
        help = "A revision column of MANIFEST.tsv, in order (`product=<sha>`)."
    )]
    pub revs: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "ATTACH.md's opening text, from FILE. Default: the tool's own."
    )]
    pub intro: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "LLVM's bin directory. Default: $MSVC6T_LLVM, else PATH."
    )]
    pub tool: Option<PathBuf>,
    #[arg(long, help = "A missing llvm-pdbutil is an error, not a SKIP.")]
    pub require_llvm: bool,
    #[arg(
        long,
        help = "Do not judge the source names by [image.pdb.paths] (said once per image)."
    )]
    pub no_paths: bool,
}

struct Ready {
    name: String,
    raw: Vec<u8>,
    pdb70: Vec<u8>,
}

fn same_file(a: &Path, b: &Path) -> bool {
    std::fs::canonicalize(a)
        .ok()
        .zip(std::fs::canonicalize(b).ok())
        .is_some_and(|(x, y)| x == y)
}

fn chosen(c: &Config, ids: &[String]) -> Result<Vec<usize>> {
    if ids.is_empty() {
        return Ok(c
            .images
            .iter()
            .enumerate()
            .filter(|(_, d)| d.pdb.as_ref().is_some_and(|p| p.file.is_some()))
            .map(|(n, _)| n)
            .collect());
    }
    let mut out: Vec<usize> = Vec::with_capacity(ids.len());
    for id in ids {
        let (n, d) = msvc6_project::registry::resolve_id(c, id)?;
        if d.pdb.as_ref().and_then(|p| p.file.as_ref()).is_none() {
            bail!("image `{}` has no [image.pdb] file", d.id);
        }
        if out.contains(&n) {
            bail!("--image {id} is given twice");
        }
        out.push(n);
    }
    Ok(out)
}

struct Run<'a> {
    c: &'a Config,
    a: &'a ReleaseArgs,
    tmp: PathBuf,
    tool: Option<PathBuf>,
    failures: usize,
    recipes: Vec<String>,
}

impl Run<'_> {
    fn fail(&mut self, what: String) {
        self.failures += 1;
        println!("  FAIL {what}");
    }

    fn image(&mut self, n: usize, names: &mut Vec<String>) -> Result<Option<Ready>> {
        let c = self.c;
        let d = c.images.get(n).context("image index")?;
        let spec = d.pdb.as_ref().context("no [image.pdb]")?;
        let key = format!("{}.pdb", c.image_key(n));
        let file = c.path_at(
            &format!("{key}.file"),
            spec.file.as_ref().context("no [image.pdb] file")?,
        )?;
        let name = spec.name.clone().unwrap_or_else(|| label(&file));
        if names.contains(&name) {
            bail!("two images publish `{name}`");
        }
        names.push(name.clone());
        let at20 = self.a.out.join(&name);
        let at70 = self.a.out.join(release::DIR70).join(&name);
        if same_file(&at20, &file) {
            bail!(
                "OUT holds image {}'s PDB itself ({}); release into another directory",
                d.id,
                file.display()
            );
        }
        println!("{} {} -> {name}", d.id, file.display());
        let raw = read(&file)?;
        let s = structure::check(&raw);
        if s.failures() > 0 {
            self.fail(format!(
                "structure: {} structural failure(s); `msvc6t pdb check` lists them",
                s.failures()
            ));
            return Ok(None);
        }
        println!("  structure: {}", s.summary());
        match (&spec.paths, self.a.no_paths) {
            (None, _) => println!("  source names: no [image.pdb.paths], not judged"),
            (Some(_), true) => println!("  source names: NOT judged (--no-paths)"),
            (Some(_), false) => {
                let (_, policy) = policy_of(c, &d.id)?;
                let j = paths::check(&raw, &policy)?;
                if j.failures() > 0 {
                    self.fail(format!(
                        "source names: {} failure(s) by the image's policy; `msvc6t pdb paths {} --check` lists them",
                        j.failures(),
                        d.id
                    ));
                    return Ok(None);
                }
                let ok: usize = j.rows.values().map(|(n, _)| n).sum();
                println!("  source names: {ok} judged, 0 failure(s)");
            }
        }
        let cv = match to70::convert(&raw) {
            Ok(cv) => cv,
            Err(e) => {
                self.fail(format!("7.0: {e:#}"));
                return Ok(None);
            }
        };
        let v = eq::equivalence(&raw, &cv.bytes)?;
        if !v.equivalent() {
            self.fail(
                "7.0: not equivalent to the 2.0 file; `msvc6t pdb to70` shows the differences"
                    .into(),
            );
            return Ok(None);
        }
        println!(
            "  7.0: EQUIVALENT, {} bytes, sha256 {}",
            cv.bytes.len(),
            sha256_hex(&cv.bytes)
        );
        if let Some(t) = &spec.twin {
            let twin = c.path_at(&format!("{key}.twin"), t)?;
            if same_file(&at70, &twin) {
                bail!(
                    "OUT holds image {}'s 7.0 twin itself ({}); release into another directory",
                    d.id,
                    twin.display()
                );
            }
            match std::fs::read(&twin) {
                Ok(b) if b == cv.bytes => {
                    println!("  7.0: IDENTICAL to the twin {}", twin.display())
                }
                Ok(b) => println!(
                    "  7.0: DIFFERS from the twin {} (sha256 {})",
                    twin.display(),
                    sha256_hex(&b)
                ),
                Err(e) => println!("  7.0: no twin to compare ({}: {e})", twin.display()),
            }
        }
        let tmp70 = self.tmp.join(release::DIR70).join(&name);
        srcio::own_write(&tmp70, &cv.bytes)
            .with_context(|| format!("writing {}", tmp70.display()))?;
        if let Some(tool) = &self.tool {
            let r = llvm::read(tool, &tmp70)?;
            let (a, b) = (eq::counts_of(&raw)?, eq::counts_of(&cv.bytes)?);
            if !r.clean() || a != b || b != r.counts {
                self.fail(format!(
                    "llvm-pdbutil: {} error line(s), {} stderr line(s), counts {}; `msvc6t pdb llvm` shows them",
                    r.errors.len(),
                    r.stderr.len(),
                    if r.counts == b { "the same" } else { "DIFFERENT" }
                ));
                return Ok(None);
            }
            println!("  llvm-pdbutil: SAME, every count this tool reads");
        }
        for (i, s) in spec.shipped.iter().enumerate() {
            let Some(f) = &s.file else {
                self.fail(format!(
                    "recipe for {} (md5 {}): no `file` names a copy to plan it on",
                    s.what, s.md5
                ));
                continue;
            };
            let path = c.path_at(&format!("{key}.shipped[{i}].file"), f)?;
            let copy = match std::fs::read(&path) {
                Ok(b) => b,
                Err(e) => {
                    self.fail(format!("recipe for {}: {}: {e}", s.what, path.display()));
                    continue;
                }
            };
            let md5 = md5_hex(&copy);
            if md5 != s.md5 {
                self.fail(format!(
                    "recipe for {}: {} has md5 {md5}, the list says {}",
                    s.what,
                    path.display(),
                    s.md5
                ));
                continue;
            }
            for (fmt, pdb) in [("2.0", &raw), ("7.0", &cv.bytes)] {
                match attach::attach(&copy, &spec.shipped, pdb, &name) {
                    Ok(r) => {
                        println!(
                            "  recipe {}, PDB {fmt}: {} range(s), md5 after {}",
                            s.what,
                            r.changes.len(),
                            r.md5_out
                        );
                        self.recipes.push(r.recipe());
                    }
                    Err(e) => self.fail(format!("recipe for {}, PDB {fmt}: {e:#}", s.what)),
                }
            }
        }
        Ok(Some(Ready {
            name,
            raw,
            pdb70: cv.bytes,
        }))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Id {
    #[cfg(unix)]
    Inode(u64, u64),
    #[cfg(not(unix))]
    Path(PathBuf),
}

#[cfg(unix)]
fn id_of(p: &Path) -> Option<Id> {
    use std::os::unix::fs::MetadataExt as _;
    std::fs::symlink_metadata(p)
        .ok()
        .map(|m| Id::Inode(m.dev(), m.ino()))
}

#[cfg(not(unix))]
fn id_of(p: &Path) -> Option<Id> {
    std::fs::canonicalize(p).ok().map(Id::Path)
}

const ASIDE: &str = "aside";

const LINK_PROBE: &str = ".link-probe";

fn in_the_way(p: &Path, what: &str) -> anyhow::Error {
    anyhow::anyhow!("{} is in the way: {what}; move it aside", p.display())
}

fn fresh_dir(p: &Path) -> Result<Id> {
    match std::fs::create_dir(p) {
        Ok(()) => id_of(p).with_context(|| format!("reading back {}", p.display())),
        Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => Err(in_the_way(
            p,
            "a release makes this directory itself and takes over nothing it finds there",
        )),
        Err(e) => Err(e).with_context(|| format!("creating {}", p.display())),
    }
}

#[derive(Debug)]
struct Replaced {
    aside: PathBuf,
    to: PathBuf,
    old: Option<Id>,
    new: Option<Id>,
}

#[derive(Debug)]
struct Staging {
    tmp: PathBuf,
    tmp_id: Option<Id>,
    dirs: Vec<(PathBuf, Id)>,
    files: Vec<(PathBuf, Id)>,
    replaced: Vec<Replaced>,
    link: fn(&Path, &Path) -> std::io::Result<()>,
}

impl Staging {
    fn new(out: &Path) -> Self {
        Self {
            tmp: out.join(TMP),
            tmp_id: None,
            dirs: Vec::new(),
            files: Vec::new(),
            replaced: Vec::new(),
            link: |from, to| std::fs::hard_link(from, to),
        }
    }

    fn dirs_to(&mut self, path: &Path) -> Result<()> {
        let mut at = PathBuf::new();
        for c in path.components() {
            at.push(c);
            if !matches!(c, Component::Normal(_)) {
                continue;
            }
            match std::fs::create_dir(&at) {
                Ok(()) => {
                    if let Some(id) = id_of(&at) {
                        self.dirs.push((at.clone(), id));
                    }
                }
                Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists && at.is_dir() => {}
                Err(e) => return Err(e).with_context(|| format!("creating {}", at.display())),
            }
        }
        Ok(())
    }

    fn open(&mut self, out: &Path) -> Result<()> {
        self.dirs_to(out)?;
        self.tmp_id = Some(fresh_dir(&self.tmp)?);
        fresh_dir(&self.tmp.join(release::DIR70))?;
        fresh_dir(&self.tmp.join(ASIDE))?;
        self.probe_link(out)
    }

    fn probe_link(&self, out: &Path) -> Result<()> {
        let probe = self.tmp.join(LINK_PROBE);
        let linked = self.tmp.join(ASIDE).join(LINK_PROBE);
        srcio::own_write(&probe, "").with_context(|| format!("writing {}", probe.display()))?;
        let made = (self.link)(&probe, &linked);
        let gone = std::fs::remove_file(&linked);
        std::fs::remove_file(&probe).with_context(|| format!("removing {}", probe.display()))?;
        match made {
            Ok(()) => gone.with_context(|| format!("removing {}", linked.display())),
            Err(e) => Err(anyhow::anyhow!(
                "{}: its filesystem refuses a hard link ({e}); pdb release places every file by a \
                 no-replace hard link, and nothing was moved",
                out.display()
            )),
        }
    }

    fn preflight(dests: &[PathBuf]) -> Result<()> {
        for to in dests {
            if let Ok(m) = std::fs::symlink_metadata(to)
                && !m.is_file()
            {
                return Err(in_the_way(
                    to,
                    "a release replaces only a regular file it wrote before",
                ));
            }
        }
        Ok(())
    }

    fn place(&mut self, from: &Path, to: &Path, between: &mut dyn FnMut()) -> Result<()> {
        let held = match std::fs::symlink_metadata(to) {
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => None,
            Err(e) => return Err(e).with_context(|| format!("reading {}", to.display())),
            Ok(m) if m.is_file() => {
                let k = self.replaced.len();
                let aside = self.tmp.join(ASIDE).join(k.to_string());
                std::fs::rename(to, &aside).with_context(|| {
                    format!("moving {} aside to {}", to.display(), aside.display())
                })?;
                self.replaced.push(Replaced {
                    old: id_of(&aside),
                    aside,
                    to: to.to_path_buf(),
                    new: None,
                });
                Some(k)
            }
            Ok(_) => {
                return Err(in_the_way(
                    to,
                    "a release replaces only a regular file it wrote before",
                ));
            }
        };
        let new = if cfg!(unix) { id_of(from) } else { None };
        between();
        match (self.link)(from, to) {
            Ok(()) => {}
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
                return Err(in_the_way(
                    to,
                    "it appeared while the release moved its file in, and nothing was overwritten",
                ));
            }
            Err(e) => {
                return Err(anyhow::anyhow!(
                    "{}: its filesystem refuses a hard link from {} ({e}); pdb release places every file \
                     by a no-replace hard link, and what it moved is put back",
                    to.display(),
                    from.display()
                ));
            }
        }
        let new = new.or_else(|| id_of(to));
        match held.and_then(|k| self.replaced.get_mut(k)) {
            Some(r) => r.new = new,
            None => {
                if let Some(id) = new {
                    self.files.push((to.to_path_buf(), id));
                }
            }
        }
        std::fs::remove_file(from).with_context(|| format!("removing {}", from.display()))
    }

    fn tidy(&self) -> Vec<String> {
        let ours = |p: &Path, id: &Id| id_of(p).as_ref() == Some(id);
        let mut kept: Vec<String> = Vec::new();
        for r in self.replaced.iter().rev() {
            if let Some(n) = &r.new
                && ours(&r.to, n)
            {
                let _ = std::fs::remove_file(&r.to);
            }
            let back = r.old.as_ref().is_some_and(|o| ours(&r.aside, o))
                && std::fs::hard_link(&r.aside, &r.to).is_ok();
            if back {
                let _ = std::fs::remove_file(&r.aside);
            } else {
                kept.push(format!(
                    "{} is not put back: the file it held is kept at {}",
                    r.to.display(),
                    r.aside.display()
                ));
            }
        }
        if let Some(id) = &self.tmp_id
            && kept.is_empty()
            && ours(&self.tmp, id)
        {
            let _ = std::fs::remove_dir_all(&self.tmp);
        }
        for (p, id) in self.files.iter().rev() {
            if ours(p, id) {
                let _ = std::fs::remove_file(p);
            }
        }
        for (p, id) in self.dirs.iter().rev() {
            if ours(p, id) {
                let _ = std::fs::remove_dir(p);
            }
        }
        kept
    }
}

fn publish(
    c: &Config,
    a: &ReleaseArgs,
    images: Vec<usize>,
    revs: &[Rev],
    intro: &str,
    tool: Option<PathBuf>,
    st: &mut Staging,
) -> Result<Outcome> {
    st.open(&a.out)?;
    let staged = st.tmp.clone();
    let tmp = staged.as_path();
    let tmp70 = tmp.join(release::DIR70);
    let mut r = Run {
        c,
        a,
        tmp: tmp.to_path_buf(),
        tool,
        failures: 0,
        recipes: Vec::new(),
    };
    let mut names = Vec::new();
    let mut ready = Vec::with_capacity(images.len());
    for n in images {
        if let Some(x) = r.image(n, &mut names)? {
            ready.push(x);
        }
    }
    if r.failures > 0 {
        println!(
            "release: {} failure(s); nothing written to {}",
            r.failures,
            a.out.display()
        );
        return Ok(Outcome::Differs);
    }
    let doc = release::attach_doc(intro, &r.recipes);
    let mut listed: Vec<(String, &[u8])> = Vec::with_capacity(2 * ready.len() + 1);
    listed.extend(ready.iter().map(|x| (x.name.clone(), x.raw.as_slice())));
    listed.extend(
        ready
            .iter()
            .map(|x| (format!("{}/{}", release::DIR70, x.name), x.pdb70.as_slice())),
    );
    listed.push((release::ATTACH.to_string(), doc.as_bytes()));
    let manifest = release::manifest(&listed, revs)?;
    for x in &ready {
        let at = tmp.join(&x.name);
        srcio::own_write(&at, &x.raw).with_context(|| format!("writing {}", at.display()))?;
    }
    for (file, text) in [(release::ATTACH, &doc), (release::MANIFEST, &manifest)] {
        let at = tmp.join(file);
        srcio::own_write(&at, text).with_context(|| format!("writing {}", at.display()))?;
    }
    let dir70 = a.out.join(release::DIR70);
    st.dirs_to(&dir70)?;
    let mut moves: Vec<(PathBuf, PathBuf)> = Vec::with_capacity(2 * ready.len() + 2);
    moves.extend(
        ready
            .iter()
            .map(|x| (tmp.join(&x.name), a.out.join(&x.name))),
    );
    moves.extend(
        ready
            .iter()
            .map(|x| (tmp70.join(&x.name), dir70.join(&x.name))),
    );
    moves.extend(
        [release::ATTACH, release::MANIFEST]
            .iter()
            .map(|f| (tmp.join(f), a.out.join(f))),
    );
    let dests: Vec<PathBuf> = moves.iter().map(|(_, to)| to.clone()).collect();
    Staging::preflight(&dests)?;
    for (from, to) in &moves {
        st.place(from, to, &mut || {})?;
    }
    std::fs::remove_dir_all(tmp).with_context(|| format!("removing {}", tmp.display()))?;
    print!("{manifest}");
    println!(
        "release: {} PDB(s) and their 7.0 twins in {}, {} recipe(s) in {}, {} sha256 {}",
        ready.len(),
        a.out.display(),
        r.recipes.len(),
        release::ATTACH,
        release::MANIFEST,
        sha256_hex(manifest.as_bytes())
    );
    Ok(Outcome::Ok)
}

pub fn run(config: Option<&Path>, a: &ReleaseArgs) -> Result<Outcome> {
    let c = Config::require(config)?;
    let images = chosen(&c, &a.images)?;
    if images.is_empty() {
        bail!("no [[image]] of this config has an [image.pdb] file");
    }
    let revs: Vec<Rev> = a
        .revs
        .iter()
        .map(|r| release::parse_rev(r))
        .collect::<Result<_>>()?;
    release::check_revs(&revs)?;
    let intro = match &a.intro {
        Some(p) => srcio::own_read(p).with_context(|| format!("reading {}", p.display()))?,
        None => release::INTRO.to_string(),
    };
    let tool = llvm::find(a.tool.as_deref());
    if tool.is_none() {
        println!(
            "SKIP llvm: no {} in {} or on PATH: LLVM's reading of the 7.0 files is NOT checked",
            llvm::TOOL,
            a.tool
                .as_ref()
                .map_or_else(|| format!("${}", llvm::ENV), |d| d.display().to_string())
        );
        if a.require_llvm {
            bail!("--require-llvm and no {}", llvm::TOOL);
        }
    }
    let mut st = Staging::new(&a.out);
    let done = publish(&c, a, images, &revs, &intro, tool, &mut st);
    if matches!(done, Ok(Outcome::Ok)) {
        return done;
    }
    let kept = st.tidy();
    match done {
        Err(e) if !kept.is_empty() => Err(e.context(kept.join("; "))),
        other => {
            for k in &kept {
                println!("release: {k}");
            }
            other
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Dir(PathBuf);

    impl Dir {
        fn new(tag: &str) -> Self {
            let d = std::env::temp_dir()
                .join(format!("msvc6t-pdbrelease-{tag}-{}", std::process::id()));
            if d.exists() {
                std::fs::remove_dir_all(&d).unwrap();
            }
            std::fs::create_dir_all(&d).unwrap();
            Self(d)
        }
    }

    impl Drop for Dir {
        fn drop(&mut self) {
            std::fs::remove_dir_all(&self.0).ok();
        }
    }

    fn staged(d: &Dir, old: Option<&str>) -> (Staging, PathBuf, PathBuf) {
        let out = d.0.join("out");
        let mut st = Staging::new(&out);
        st.open(&out).unwrap();
        let from = st.tmp.join("a.pdb");
        srcio::own_write(&from, "new").unwrap();
        let to = out.join("a.pdb");
        if let Some(t) = old {
            srcio::own_write(&to, t).unwrap();
        }
        (st, from, to)
    }

    fn text(p: &Path) -> String {
        srcio::own_read(p).unwrap()
    }

    #[cfg(unix)]
    #[test]
    fn a_filesystem_that_refuses_a_hard_link_is_named_before_any_move_and_the_probe_leaves_nothing()
    {
        use std::os::unix::fs::PermissionsExt as _;
        let d = Dir::new("probe");
        let out = d.0.join("out");
        let mut st = Staging::new(&out);
        st.open(&out).unwrap();
        let names = |st: &Staging| [st.tmp.join(LINK_PROBE), st.tmp.join(ASIDE).join(LINK_PROBE)];
        assert!(names(&st).iter().all(|p| !p.exists()));
        let aside = st.tmp.join(ASIDE);
        let mode = |m: u32| {
            std::fs::set_permissions(&aside, std::fs::Permissions::from_mode(m)).unwrap();
        };
        mode(0o555);
        if srcio::own_write(aside.join("w"), "x").is_ok() {
            mode(0o755);
            eprintln!("SKIP a read-only directory is writable here");
            return;
        }
        let e = st.probe_link(&out).unwrap_err().to_string();
        mode(0o755);
        assert!(
            e.contains(&format!(
                "{}: its filesystem refuses a hard link (",
                out.display()
            )) && e.contains(
                "pdb release places every file by a no-replace hard link, and nothing was moved"
            ),
            "{e}"
        );
        assert!(names(&st).iter().all(|p| !p.exists()));
        assert!(st.tidy().is_empty());
        assert!(!st.tmp.exists());
    }

    #[test]
    fn open_runs_the_link_probe_and_a_refused_link_fails_it_before_anything_moves() {
        let d = Dir::new("open");
        let out = d.0.join("out");
        let mut st = Staging::new(&out);
        st.link = |_, _| Err(std::io::Error::from(std::io::ErrorKind::PermissionDenied));
        let e = st.open(&out).unwrap_err().to_string();
        assert!(
            e.contains(&format!(
                "{}: its filesystem refuses a hard link (",
                out.display()
            )) && e.contains(
                "pdb release places every file by a no-replace hard link, and nothing was moved"
            ),
            "{e}"
        );
        let mut held: Vec<String> = std::fs::read_dir(&st.tmp)
            .unwrap()
            .map(|e| e.unwrap().file_name().to_string_lossy().into_owned())
            .collect();
        held.sort();
        assert_eq!(held, [release::DIR70, ASIDE]);
        for sub in [release::DIR70, ASIDE] {
            assert_eq!(
                std::fs::read_dir(st.tmp.join(sub)).unwrap().count(),
                0,
                "{sub}"
            );
        }
        assert!(st.tidy().is_empty());
        assert!(!st.tmp.exists());
        assert!(!out.exists());
        let mut fine = Staging::new(&out);
        fine.open(&out).unwrap();
        assert!(fine.tidy().is_empty());
        assert!(!out.exists());
    }

    #[test]
    fn a_file_that_appears_at_a_destination_while_its_file_moves_in_is_never_overwritten() {
        let d = Dir::new("race");
        let (mut st, from, to) = staged(&d, None);
        let e = st
            .place(&from, &to, &mut || srcio::own_write(&to, "theirs").unwrap())
            .unwrap_err();
        assert!(e.to_string().contains("is in the way"), "{e:#}");
        assert_eq!(text(&to), "theirs");
        assert!(st.tidy().is_empty());
        assert_eq!(text(&to), "theirs");
        assert!(!st.tmp.exists());
    }

    #[test]
    fn a_replaced_file_that_cannot_be_put_back_is_kept_and_named() {
        let d = Dir::new("kept");
        let (mut st, from, to) = staged(&d, Some("old"));
        let gone = from.clone();
        assert!(
            st.place(&from, &to, &mut || {
                std::fs::remove_file(&gone).unwrap();
                srcio::own_write(&to, "theirs").unwrap();
            })
            .is_err()
        );
        let kept = st.tidy();
        assert_eq!(text(&to), "theirs");
        let aside = st.tmp.join(ASIDE).join("0");
        assert_eq!(kept.len(), 1, "{kept:?}");
        assert!(
            kept[0].contains(&format!("kept at {}", aside.display())),
            "{kept:?}"
        );
        assert_eq!(text(&aside), "old");
    }

    #[test]
    fn a_replaced_file_is_put_back_after_a_failed_or_a_finished_move() {
        let d = Dir::new("back");
        let (mut st, from, to) = staged(&d, Some("old"));
        let gone = from.clone();
        assert!(
            st.place(&from, &to, &mut || std::fs::remove_file(&gone).unwrap())
                .is_err()
        );
        assert!(st.tidy().is_empty());
        assert_eq!(text(&to), "old");
        assert!(!st.tmp.exists());
        let d = Dir::new("back2");
        let (mut st, from, to) = staged(&d, Some("old"));
        st.place(&from, &to, &mut || {}).unwrap();
        assert_eq!(text(&to), "new");
        assert!(!from.exists());
        assert!(st.tidy().is_empty());
        assert_eq!(text(&to), "old");
        assert!(!st.tmp.exists());
    }
}
