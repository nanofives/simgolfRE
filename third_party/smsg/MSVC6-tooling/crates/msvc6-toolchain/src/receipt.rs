use crate::cache::{Manifest, sha_file};
use anyhow::{Context as _, Result, bail};
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

pub const FORMAT: &str = "msvc6t-receipt/1";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Receipt {
    pub source: PathBuf,
    pub source_sha: String,
    pub args: Vec<String>,
    pub env: Vec<(String, String)>,
    pub toolchain: String,
    pub reads: Option<Manifest>,
    pub obj_sha: String,
    pub cod_sha: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
struct Wire {
    format: String,
    source: String,
    source_sha: String,
    args: Vec<String>,
    env: Vec<(String, String)>,
    toolchain: String,
    files: Option<Vec<(String, String)>>,
    dirs: Option<Vec<(String, String)>>,
    obj_sha: String,
    cod_sha: Option<String>,
}

pub fn path_for(obj: &Path) -> PathBuf {
    let mut s = obj.as_os_str().to_os_string();
    s.push(".receipt");
    PathBuf::from(s)
}

fn strs(v: &[(PathBuf, String)]) -> Vec<(String, String)> {
    v.iter()
        .map(|(p, h)| (p.display().to_string(), h.clone()))
        .collect()
}

fn paths(v: Vec<(String, String)>) -> Vec<(PathBuf, String)> {
    v.into_iter().map(|(p, h)| (PathBuf::from(p), h)).collect()
}

impl Receipt {
    pub fn write(&self, obj: &Path) -> Result<()> {
        let w = Wire {
            format: FORMAT.to_string(),
            source: self.source.display().to_string(),
            source_sha: self.source_sha.clone(),
            args: self.args.clone(),
            env: self.env.clone(),
            toolchain: self.toolchain.clone(),
            files: self.reads.as_ref().map(|m| strs(&m.files)),
            dirs: self.reads.as_ref().map(|m| strs(&m.dirs)),
            obj_sha: self.obj_sha.clone(),
            cod_sha: self.cod_sha.clone(),
        };
        let text = serde_json::to_string_pretty(&w)?;
        msvc6_base::srcio::own_write(path_for(obj), text)
            .with_context(|| format!("writing the receipt of {}", obj.display()))
    }

    pub fn read(obj: &Path) -> Result<Self> {
        let p = path_for(obj);
        let text = msvc6_base::srcio::own_read(&p)
            .with_context(|| format!("{} has no receipt ({})", obj.display(), p.display()))?;
        let w: Wire =
            serde_json::from_str(&text).with_context(|| format!("parsing {}", p.display()))?;
        if w.format != FORMAT {
            bail!("{}: format {} is not {FORMAT}", p.display(), w.format);
        }
        let reads = match (w.files, w.dirs) {
            (Some(f), Some(d)) => Some(Manifest {
                key: String::new(),
                files: paths(f),
                dirs: paths(d),
            }),
            _ => None,
        };
        Ok(Self {
            source: PathBuf::from(w.source),
            source_sha: w.source_sha,
            args: w.args,
            env: w.env,
            toolchain: w.toolchain,
            reads,
            obj_sha: w.obj_sha,
            cod_sha: w.cod_sha,
        })
    }

    pub fn current(&self, obj: &Path, cod: Option<&Path>, toolchain: Option<&str>) -> Result<()> {
        if sha_file(obj).as_deref() != Some(self.obj_sha.as_str()) {
            bail!(
                "{} is not the object its receipt names: another writer replaced it",
                obj.display()
            );
        }
        if let (Some(c), Some(want)) = (cod, &self.cod_sha)
            && sha_file(c).as_deref() != Some(want.as_str())
        {
            bail!("{} is not the listing its receipt names", c.display());
        }
        if sha_file(&self.source).as_deref() != Some(self.source_sha.as_str()) {
            bail!("{} changed since it was compiled", self.source.display());
        }
        if let Some(t) = toolchain
            && t != self.toolchain
        {
            bail!("the toolchain changed since {} was compiled", obj.display());
        }
        let Some(m) = &self.reads else {
            bail!(
                "{}'s receipt records no read set (an uncached compile), so it cannot be certified current",
                obj.display()
            );
        };
        if let Some(why) = m.changed() {
            bail!("{why} since {} was compiled", obj.display());
        }
        Ok(())
    }
}

pub fn current(obj: &Path, toolchain: Option<&str>) -> Result<()> {
    Receipt::read(obj)?.current(obj, None, toolchain)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_receipt_holds_until_any_input_changes() {
        let d = std::env::temp_dir().join(format!("msvc6t-receipt-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        let (src, hdr, obj) = (d.join("a.c"), d.join("a.h"), d.join("a.obj"));
        msvc6_base::srcio::write_bytes(&src, b"#include \"a.h\"\n").unwrap();
        msvc6_base::srcio::write_bytes(&hdr, b"int a;\n").unwrap();
        msvc6_base::srcio::write_bytes(&obj, b"obj").unwrap();
        let r = Receipt {
            source: src.clone(),
            source_sha: sha_file(&src).unwrap(),
            args: vec!["/c".into(), "a.c".into()],
            env: vec![("INCLUDE".into(), "Z:\\i".into())],
            toolchain: "t1".into(),
            reads: Some(Manifest {
                key: String::new(),
                files: vec![(hdr.clone(), sha_file(&hdr).unwrap())],
                dirs: vec![],
            }),
            obj_sha: sha_file(&obj).unwrap(),
            cod_sha: None,
        };
        r.write(&obj).unwrap();
        assert_eq!(Receipt::read(&obj).unwrap(), r);
        current(&obj, Some("t1")).unwrap();
        assert!(current(&obj, Some("t2")).is_err());
        msvc6_base::srcio::write_bytes(&hdr, b"int b;\n").unwrap();
        let e = current(&obj, None).unwrap_err().to_string();
        assert!(e.contains("a.h changed"), "{e}");
        std::fs::remove_dir_all(&d).unwrap();
    }
}
