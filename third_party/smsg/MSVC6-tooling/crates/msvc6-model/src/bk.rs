use crate::drain::{self, Stream, TuIl};
use anyhow::{Context as _, Result, bail};
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_toolchain::layout::to_win;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub const MAX_IL: u64 = 1 << 30;

#[derive(Debug)]
pub struct Driver<'a> {
    pub compiler: &'a Compiler,
    pub units: BTreeMap<String, Unit>,
    pub order: Vec<String>,
    pub batches: BTreeMap<String, String>,
    pub work: PathBuf,
    pub jobs: usize,
}

fn read_il(p: &Path) -> Vec<u8> {
    msvc6_il::file::read(p, MAX_IL).unwrap_or_default()
}

fn remove(p: &Path) -> Result<()> {
    match std::fs::remove_file(p) {
        Ok(()) => Ok(()),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(e) => Err(e).with_context(|| format!("removing {}", p.display())),
    }
}

#[derive(Debug, Clone)]
pub struct Replayed {
    pub stem: String,
    pub il: TuIl,
    pub obj: PathBuf,
    pub creator: String,
}

impl Driver<'_> {
    #[must_use]
    pub fn unit(&self, stem: &str) -> Option<&Unit> {
        self.units.get(&stem.to_ascii_lowercase())
    }

    fn run(
        &self,
        u: &Unit,
        dir: &Path,
        extra: &[&str],
        base: &str,
        pch: Option<&Path>,
    ) -> Result<(PathBuf, PathBuf)> {
        std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))?;
        let staged = dir.join(&u.file);
        if staged.canonicalize().ok() != u.source.canonicalize().ok() {
            std::fs::copy(&u.source, &staged).with_context(|| {
                format!("copying {} to {}", u.source.display(), staged.display())
            })?;
        }
        for s in ["ex", "gl", "in", "sy", "db"] {
            remove(&dir.join(format!("{base}{s}")))?;
        }
        if pch.is_none() {
            msvc6_toolchain::compile::clear_pch(dir);
        }
        let obj = dir.join(format!("{}.obj", u.stem));
        remove(&obj)?;
        let mut args = vec!["/nologo".to_string(), "/c".to_string()];
        args.extend(u.cflags.iter().cloned());
        args.extend(extra.iter().map(|s| (*s).to_string()));
        for d in &u.includes {
            let abs = d.canonicalize().unwrap_or_else(|_| d.clone());
            args.push(format!("/I{}", to_win(&abs)));
        }
        if let Some(p) = pch {
            args.push(format!("/Fp{}", to_win(p)));
        }
        args.push(format!("/Bk{base}"));
        args.push(format!("/Fo{}.obj", u.stem));
        args.push(u.file.clone());
        let tc = self.compiler.toolchain(&u.toolchain)?;
        let sess = self.compiler.runner.session()?;
        let mut env = Vec::new();
        if let Some(i) = &u.include_env {
            env.push(("INCLUDE".to_string(), i.clone()));
        }
        sess.run_tool(&tc, "CL.EXE", &args, Some(dir), &env)
            .with_context(|| format!("compiling {} with /Bk", u.source.display()))?;
        let gl = dir.join(format!("{base}gl"));
        if !gl.is_file() || !obj.is_file() {
            bail!("CL kept no .gl / object for {}", u.source.display());
        }
        Ok((gl, obj))
    }

    pub fn bk(
        &self,
        stem: &str,
        sub: Option<&str>,
        extra: &[&str],
    ) -> Result<Option<(TuIl, PathBuf)>> {
        let Some(u) = self.unit(stem) else {
            return Ok(None);
        };
        let mut dir = self.work.join(&u.stem);
        if let Some(s) = sub {
            dir = dir.join(s);
        }
        let (gl, obj) = self.run(u, &dir, extra, "c1_", None)?;
        let il = drain::read_tu(
            &read_il(&gl),
            &read_il(&dir.join("c1_ex")),
            &read_il(&dir.join("vc60.pch")),
        );
        Ok(Some((il, obj)))
    }

    pub fn replay(&self, seq: &[String]) -> Result<Vec<Replayed>> {
        let root = self.work.join("pch-use");
        let (cu, pch_path) = (root.join("cu"), root.join("shared.pch"));
        std::fs::create_dir_all(&cu).with_context(|| format!("creating {}", cu.display()))?;
        for f in [cu.join("vc60.pdb"), cu.join("vc60.idb"), pch_path.clone()] {
            remove(&f)?;
        }
        let stamp = |p: &Path| std::fs::metadata(p).and_then(|m| m.modified()).ok();
        let (mut bytes, mut when): (Vec<u8>, Option<std::time::SystemTime>) = (Vec::new(), None);
        let mut creator = String::new();
        let mut resident: BTreeMap<String, u32> = BTreeMap::new();
        let mut out = Vec::new();
        for stem in seq {
            let Some(u) = self.unit(stem) else {
                continue;
            };
            let base = format!("{}_", u.stem);
            let (gl, obj) = self.run(u, &cu, &[], &base, Some(&pch_path))?;
            let pch = read_il(&pch_path);
            let now = stamp(&pch_path);
            let created = creator.is_empty() || now != when || pch != bytes;
            let g = read_il(&gl);
            let ex = read_il(&cu.join(format!("{}_ex", u.stem)));
            let il = if created {
                creator.clone_from(&u.stem);
                let il = drain::read_tu(&g, &ex, &pch);
                resident = il
                    .fns
                    .values()
                    .filter(|f| f.stream == Stream::Pch)
                    .map(|f| (f.name.clone(), f.off))
                    .collect();
                il
            } else {
                drain::read_tu_used(&g, &ex, &pch, &resident)
            };
            out.push(Replayed {
                il,
                stem: u.stem.clone(),
                obj,
                creator: creator.clone(),
            });
            (bytes, when) = (pch, now);
        }
        Ok(out)
    }

    #[must_use]
    pub fn msdev_order(&self) -> Vec<String> {
        drain::batch_order(&self.order, &self.batches)
    }
}
