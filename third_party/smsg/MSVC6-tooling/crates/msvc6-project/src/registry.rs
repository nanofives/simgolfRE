use crate::config::{ImageDecl, Project};
use crate::spans::join;
use anyhow::{Context as _, Result, bail};
use msvc6_base::digest::sha256_hex;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct Resolved {
    pub label: String,
    pub path: PathBuf,
    pub bytes: Vec<u8>,
    pub decl: Option<ImageDecl>,
    pub key: Option<String>,
}

pub fn read(path: &Path) -> Result<Vec<u8>> {
    std::fs::read(path).with_context(|| format!("reading {}", path.display()))
}

impl Project {
    pub fn reference_path(&self, n: usize) -> Result<PathBuf> {
        let d = self
            .images
            .get(n)
            .with_context(|| format!("no image[{n}]"))?;
        self.path_at(&join(&self.image_key(n), "reference"), &d.reference)
    }

    pub fn reference(&self, n: usize) -> Result<Resolved> {
        let d = self
            .images
            .get(n)
            .with_context(|| format!("no image[{n}]"))?;
        let key = self.image_key(n);
        let path = self.reference_path(n)?;
        let bytes = std::fs::read(&path).map_err(|e| {
            self.error(
                &join(&key, "reference"),
                format!("image `{}`: reading {}: {e}", d.id, path.display()),
            )
        })?;
        if let Some(want) = &d.sha256 {
            let have = sha256_hex(&bytes);
            if !have.eq_ignore_ascii_case(want.as_str()) {
                return Err(self.error(
                    &join(&key, "sha256"),
                    format!(
                        "image `{}`: {} has sha256 {have}, not the {} declared",
                        d.id,
                        path.display(),
                        want.as_str()
                    ),
                ));
            }
        }
        Ok(Resolved {
            label: d.id.clone(),
            path,
            bytes,
            decl: Some(d.clone()),
            key: Some(key),
        })
    }
}

pub fn resolve(config: Option<&Project>, arg: &str) -> Result<Resolved> {
    if let Some(c) = config
        && let Some((n, _)) = c.image(arg)
    {
        return c.reference(n);
    }
    let path = PathBuf::from(arg);
    if path.is_file() {
        let bytes = read(&path)?;
        let label = path
            .file_name()
            .map_or_else(|| arg.to_string(), |n| n.to_string_lossy().into_owned());
        return Ok(Resolved {
            label,
            path,
            bytes,
            decl: None,
            key: None,
        });
    }
    match config {
        Some(c) => bail!(
            "no image `{arg}`: not an [[image]] id in {} ({}) and no such file",
            c.file.display(),
            c.ids().join(", ")
        ),
        None => bail!(
            "no image `{arg}`: no such file, and no {} names it",
            crate::config::FILE
        ),
    }
}

pub fn resolve_id<'a>(config: &'a Project, arg: &str) -> Result<(usize, &'a ImageDecl)> {
    config.image(arg).with_context(|| {
        format!(
            "no [[image]] `{arg}` in {}; its ids are {}",
            config.file.display(),
            if config.images.is_empty() {
                "none".to_string()
            } else {
                config.ids().join(", ")
            }
        )
    })
}
