use anyhow::{Context as _, Result, bail};
use msvc6_project::config::Project;
use msvc6_project::spec::{CompileSpec, Specs};
use msvc6_toolchain::cache::{Cache, Outcome, Outputs};
use msvc6_toolchain::compile::Job;
use msvc6_toolchain::layout::{Toolchain, to_win};
use msvc6_toolchain::runner::{Runner, Session};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Unit {
    pub source: PathBuf,
    pub stem: String,
    pub file: String,
    pub cflags: Vec<String>,
    pub includes: Vec<PathBuf>,
    pub include_env: Option<String>,
    pub toolchain: String,
}

#[derive(Debug, Clone)]
pub struct Out {
    pub dir: PathBuf,
    pub obj: PathBuf,
    pub cod: Option<PathBuf>,
    pub args: Vec<String>,
    pub text: String,
    pub outcome: Outcome,
}

impl Unit {
    pub fn of_spec(spec: &CompileSpec, source: &Path, default_toolchain: &str) -> Result<Self> {
        let file = source
            .file_name()
            .and_then(|f| f.to_str())
            .with_context(|| format!("{}: no file name", source.display()))?
            .to_string();
        let stem = source
            .file_stem()
            .and_then(|f| f.to_str())
            .unwrap_or(&spec.stem)
            .to_string();
        Ok(Self {
            source: source.to_path_buf(),
            stem,
            file,
            cflags: spec.cflags.clone(),
            includes: spec.include_paths.iter().map(PathBuf::from).collect(),
            include_env: None,
            toolchain: spec
                .toolchain
                .clone()
                .unwrap_or_else(|| default_toolchain.to_string()),
        })
    }

    #[must_use]
    pub fn args(&self, listing: bool) -> Vec<String> {
        let mut args = vec!["/nologo".to_string(), "/c".to_string()];
        args.extend(self.cflags.iter().cloned());
        for d in &self.includes {
            let abs = d.canonicalize().unwrap_or_else(|_| d.clone());
            args.push(format!("/I{}", to_win(&abs)));
        }
        args.push(format!("/Fo{}.obj", self.stem));
        if listing {
            args.push("/FAcs".to_string());
            args.push(format!("/Fa{}.cod", self.stem));
        }
        args.push(self.file.clone());
        args
    }
}

pub fn spec_for<'a>(specs: &'a Specs, image: &str, source: &Path) -> Option<&'a CompileSpec> {
    let canon = source.canonicalize().ok();
    let named = |s: &&CompileSpec| {
        canon
            .as_ref()
            .is_some_and(|c| Path::new(&s.path).canonicalize().ok().as_ref() == Some(c))
    };
    let mine = || specs.specs.iter().filter(|s| s.image == image);
    mine().find(named).or_else(|| {
        let stem = source.file_stem()?.to_str()?;
        let mut hits = mine().filter(|s| stem_names(stem, &s.stem));
        let first = hits.next()?;
        hits.next().is_none().then_some(first)
    })
}

#[must_use]
pub fn stem_names(copy: &str, tu: &str) -> bool {
    if copy.eq_ignore_ascii_case(tu) {
        return true;
    }
    let lower = copy.to_ascii_lowercase();
    let tu = tu.to_ascii_lowercase();
    let rest = lower.strip_prefix("zz_").unwrap_or(&lower);
    rest == tu
        || rest
            .strip_prefix(tu.as_str())
            .is_some_and(|r| r.starts_with('_') || r.starts_with('-') || r.starts_with('.'))
}

fn squash(win: &str) -> String {
    let mut out = String::with_capacity(win.len());
    for c in win.chars() {
        if c == '\\' && out.ends_with('\\') {
            continue;
        }
        out.push(c);
    }
    out.trim_end_matches('\\').to_string()
}

pub fn build_include(
    cfg: &msvc6_toolchain::config::Config,
    build: &msvc6_toolchain::config::BuildSpec,
) -> Result<(String, Vec<String>)> {
    let mut mounts: Vec<(String, PathBuf)> = Vec::new();
    for m in &build.mount {
        mounts.push((squash(&m.at).to_ascii_lowercase(), cfg.path(&m.from)?));
    }
    mounts.sort_by_key(|(at, _)| std::cmp::Reverse(at.len()));
    let mut dirs = Vec::new();
    let mut unmapped = Vec::new();
    for inc in &build.include {
        let w = squash(inc);
        let lower = w.to_ascii_lowercase();
        let hit = mounts.iter().find_map(|(at, host)| {
            let rest = lower.strip_prefix(at.as_str())?;
            if !(rest.is_empty() || rest.starts_with('\\')) {
                return None;
            }
            let tail = w.get(at.len()..)?.trim_start_matches('\\');
            Some(
                tail.split('\\')
                    .filter(|c| !c.is_empty())
                    .fold(host.clone(), |p, c| p.join(c)),
            )
        });
        match hit {
            Some(h) => dirs.push(to_win(
                &msvc6_toolchain::layout::resolve_ci(&h).unwrap_or(h),
            )),
            None => unmapped.push(inc.clone()),
        }
    }
    Ok((dirs.join(";"), unmapped))
}

#[derive(Debug)]
pub struct Compiler {
    pub runner: Runner,
    pub cache: Option<Cache>,
    pub cfg: msvc6_toolchain::config::Config,
}

impl Compiler {
    pub fn new(p: &Project) -> Result<Self> {
        let cfg = p.toolchain_config();
        let runner = Runner::from_config(&cfg)?;
        let cache = Cache::from_config(&cfg).ok();
        Ok(Self { runner, cache, cfg })
    }

    pub fn toolchain(&self, id: &str) -> Result<Toolchain> {
        Toolchain::named(&self.cfg, id)
    }

    pub fn compile(&self, sess: &Session<'_>, u: &Unit, dir: &Path, listing: bool) -> Result<Out> {
        let tc = self.toolchain(&u.toolchain)?;
        compile_in(sess, self.cache.as_ref(), &tc, u, dir, listing)
    }
}

pub fn compile_in(
    sess: &Session<'_>,
    cache: Option<&Cache>,
    tc: &Toolchain,
    u: &Unit,
    dir: &Path,
    listing: bool,
) -> Result<Out> {
    std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))?;
    let staged = dir.join(&u.file);
    if staged.canonicalize().ok() != u.source.canonicalize().ok() {
        std::fs::copy(&u.source, &staged)
            .with_context(|| format!("copying {} to {}", u.source.display(), staged.display()))?;
    }
    let obj = dir.join(format!("{}.obj", u.stem));
    let cod = listing.then(|| dir.join(format!("{}.cod", u.stem)));
    for stale in [Some(&obj), cod.as_ref()].into_iter().flatten() {
        if stale.exists() {
            std::fs::remove_file(stale).with_context(|| format!("removing {}", stale.display()))?;
        }
    }
    let mut env = Vec::new();
    if let Some(i) = &u.include_env {
        env.push(("INCLUDE".to_string(), i.clone()));
    }
    let args = u.args(listing);
    let job = Job {
        tc,
        dir: dir.to_path_buf(),
        args: args.clone(),
        outs: Outputs {
            obj: obj.clone(),
            cod: cod.clone(),
        },
        env,
        receipt: true,
    };
    let done = msvc6_toolchain::compile::compile(sess, cache, &job)?;
    if !obj.is_file() {
        bail!(
            "CL wrote no {} for {}:\n{}",
            obj.display(),
            u.source.display(),
            done.text.trim_end()
        );
    }
    Ok(Out {
        dir: dir.to_path_buf(),
        obj,
        cod: cod.filter(|c| c.is_file()),
        args,
        text: done.text,
        outcome: done.outcome,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_copy_names_its_tu_by_stem() {
        assert!(stem_names("Plot", "Plot"));
        assert!(stem_names("Plot_v2", "Plot"));
        assert!(stem_names("zz_Plot", "Plot"));
        assert!(!stem_names("Plots", "Plot"));
        assert!(!stem_names("Other", "Plot"));
    }

    #[test]
    fn the_arguments_compile_one_object_named_for_the_stem() {
        let u = Unit {
            source: PathBuf::from("/w/src/Plot.cpp"),
            stem: "Plot".into(),
            file: "Plot.cpp".into(),
            cflags: vec!["/O2".into(), "/Gy".into()],
            includes: vec![PathBuf::from("/nonexistent/include")],
            include_env: None,
            toolchain: "vc6".into(),
        };
        let a = u.args(true);
        assert_eq!(a.first().map(String::as_str), Some("/nologo"));
        assert!(a.contains(&"/FoPlot.obj".to_string()));
        assert!(a.contains(&"/FaPlot.cod".to_string()));
        assert!(a.iter().any(|x| x.starts_with("/IZ:")));
        assert_eq!(a.last().map(String::as_str), Some("Plot.cpp"));
    }
}
