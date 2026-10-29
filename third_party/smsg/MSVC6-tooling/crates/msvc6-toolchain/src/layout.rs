use crate::config::{Config, ToolchainSpec};
use anyhow::{Context as _, Result, bail};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Toolchain {
    pub id: String,
    pub root: PathBuf,
    pub bin: PathBuf,
    pub common: Option<PathBuf>,
    pub lib: Option<PathBuf>,
    pub include: Option<PathBuf>,
    pub expect: BTreeMap<String, String>,
}

pub const COMPILER_FILES: [&str; 4] = ["CL.EXE", "C1.DLL", "C1XX.DLL", "C2.DLL"];

pub const ROLES: [(&str, &str, bool); 8] = [
    ("cl", "CL.EXE", false),
    ("c1", "C1.DLL", false),
    ("c1xx", "C1XX.DLL", false),
    ("c2", "C2.DLL", false),
    ("link", "LINK.EXE", false),
    ("lib", "LIB.EXE", false),
    ("mspdb", "MSPDB60.DLL", true),
    ("msdev", "MSDEV.EXE", true),
];

pub fn resolve_ci(p: &Path) -> Option<PathBuf> {
    if p.exists() {
        return Some(p.to_path_buf());
    }
    let mut at = PathBuf::new();
    for c in p.components() {
        let next = at.join(c);
        if next.exists() {
            at = next;
            continue;
        }
        let want = c.as_os_str().to_string_lossy().to_ascii_lowercase();
        let hit = std::fs::read_dir(if at.as_os_str().is_empty() {
            Path::new(".")
        } else {
            &at
        })
        .ok()?
        .flatten()
        .find(|e| e.file_name().to_string_lossy().to_ascii_lowercase() == want)?;
        at = hit.path();
    }
    Some(at)
}

impl Toolchain {
    pub fn from_spec(id: &str, spec: &ToolchainSpec, cfg: &Config) -> Result<Self> {
        if spec.layout != "vs6" {
            bail!(
                "toolchain {id}: layout `{}` is unknown (only \"vs6\": VC98/BIN, COMMON/MSDEV98/BIN, \
                 VC98/LIB, VC98/INCLUDE)",
                spec.layout
            );
        }
        let root = cfg
            .path(&spec.root)
            .with_context(|| format!("toolchain {id}: its root"))?;
        if !root.is_dir() {
            bail!("toolchain {id}: root {} is not a directory", root.display());
        }
        let sub = |given: &Option<String>, dflt: &str| -> Option<PathBuf> {
            let rel = given.clone().unwrap_or_else(|| dflt.to_string());
            resolve_ci(&root.join(rel))
        };
        let bin = sub(&spec.bin, "VC98/BIN")
            .with_context(|| format!("toolchain {id}: no VC98/BIN under {}", root.display()))?;
        Ok(Self {
            id: id.to_string(),
            bin,
            common: sub(&spec.common, "COMMON/MSDEV98/BIN"),
            lib: sub(&spec.lib, "VC98/LIB"),
            include: sub(&spec.include, "VC98/INCLUDE"),
            root,
            expect: spec.expect.clone(),
        })
    }

    pub fn all(cfg: &Config) -> Result<Vec<Self>> {
        cfg.toolchains
            .iter()
            .map(|(id, s)| Self::from_spec(id, s, cfg))
            .collect()
    }

    pub fn named(cfg: &Config, id: &str) -> Result<Self> {
        let spec = cfg.toolchains.get(id).with_context(|| {
            format!(
                "no [toolchain.{id}] in the config; it has {}",
                cfg.toolchains
                    .keys()
                    .map(String::as_str)
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        })?;
        Self::from_spec(id, spec, cfg)
    }

    pub fn tool(&self, exe: &str) -> Result<PathBuf> {
        for dir in std::iter::once(&self.bin).chain(self.common.iter()) {
            if let Some(p) = resolve_ci(&dir.join(exe)) {
                return Ok(p);
            }
        }
        bail!(
            "toolchain {}: {exe} is not in {}{}",
            self.id,
            self.bin.display(),
            self.common
                .as_ref()
                .map(|c| format!(" or {}", c.display()))
                .unwrap_or_default()
        )
    }

    pub fn subdir(&self, rel: &str) -> Option<PathBuf> {
        resolve_ci(&self.root.join(rel))
    }

    pub fn identity(&self) -> String {
        let mut fields: Vec<Vec<u8>> = vec![b"msvc6t-toolchain/1".to_vec()];
        for exe in COMPILER_FILES {
            let h = self
                .tool(exe)
                .ok()
                .and_then(|p| std::fs::read(p).ok())
                .map(|b| msvc6_base::digest::sha256_hex(&b))
                .unwrap_or_default();
            fields.push(format!("{exe}={h}").into_bytes());
        }
        crate::cache::key_of(&fields.iter().map(Vec::as_slice).collect::<Vec<_>>())
    }
}

pub fn to_win(p: &Path) -> String {
    format!("Z:{}", p.display()).replace('/', "\\")
}

pub fn win_join(dir: &str, name: &str) -> String {
    if dir.ends_with('\\') {
        format!("{dir}{name}")
    } else {
        format!("{dir}\\{name}")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_tree_is_read_under_its_own_casing() {
        let d = std::env::temp_dir().join(format!("msvc6t-layout-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(d.join("VC98/Bin")).unwrap();
        std::fs::create_dir_all(d.join("Common/MSDev98/Bin")).unwrap();
        msvc6_base::srcio::write_bytes(d.join("VC98/Bin/cl.exe"), b"x").unwrap();
        msvc6_base::srcio::write_bytes(d.join("Common/MSDev98/Bin/MSDEV.EXE"), b"y").unwrap();
        let text = format!("[toolchain.t]\nroot = \"{}\"\n", d.display());
        let cfg = crate::config::test_config(&text, None);
        let t = Toolchain::named(&cfg, "t").unwrap();
        assert_eq!(t.bin, d.join("VC98/Bin"));
        assert_eq!(t.tool("CL.EXE").unwrap(), d.join("VC98/Bin/cl.exe"));
        assert_eq!(
            t.tool("msdev.exe").unwrap(),
            d.join("Common/MSDev98/Bin/MSDEV.EXE")
        );
        assert!(t.tool("LINK.EXE").is_err());
        assert!(t.lib.is_none());
        assert!(Toolchain::named(&cfg, "u").is_err());
        assert_eq!(to_win(Path::new("/a/b")), "Z:\\a\\b");
        assert_eq!(win_join("C:\\x", "y"), "C:\\x\\y");
        std::fs::remove_dir_all(&d).unwrap();
    }
}
