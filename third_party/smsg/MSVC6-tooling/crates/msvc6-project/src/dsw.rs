use anyhow::{Context as _, Result, bail};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DswProject {
    pub name: String,
    pub dsp: String,
    pub line: usize,
    pub deps: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dsw {
    pub path: PathBuf,
    pub projects: Vec<DswProject>,
}

pub const HEADER: &str = "Microsoft Developer Studio Workspace File, Format Version 6.00";

impl Dsw {
    pub fn read(path: &Path) -> Result<Self> {
        let text =
            msvc6_base::srcio::read(path).with_context(|| format!("reading {}", path.display()))?;
        Self::parse(path, &text)
    }

    pub fn parse(path: &Path, text: &str) -> Result<Self> {
        let mut projects: Vec<DswProject> = Vec::new();
        let mut header = false;
        let mut in_global = false;
        for (i, raw) in text.lines().enumerate() {
            let l = raw.trim_end_matches('\r');
            let n = i.saturating_add(1);
            if l.starts_with("Microsoft Developer Studio Workspace File") {
                if l != HEADER {
                    bail!(
                        "{}:{n}: `{l}` is not a version 6.00 workspace",
                        path.display()
                    );
                }
                header = true;
                continue;
            }
            if let Some(r) = l.strip_prefix("Project: \"") {
                let (name, rest) = r.split_once("\"=").with_context(|| {
                    format!(
                        "{}:{n}: a Project: line with no `\"=`: `{l}`",
                        path.display()
                    )
                })?;
                let dsp = rest
                    .split(" - Package Owner")
                    .next()
                    .unwrap_or_default()
                    .trim()
                    .trim_matches('"')
                    .to_string();
                if dsp.is_empty() {
                    bail!("{}:{n}: project `{name}` names no .dsp", path.display());
                }
                if projects.iter().any(|p| p.name.eq_ignore_ascii_case(name)) {
                    bail!("{}:{n}: a second project `{name}`", path.display());
                }
                projects.push(DswProject {
                    name: name.to_string(),
                    dsp,
                    line: n,
                    deps: Vec::new(),
                });
                in_global = false;
                continue;
            }
            if l.starts_with("Global:") {
                in_global = true;
                continue;
            }
            if let Some(d) = l.trim().strip_prefix("Project_Dep_Name ") {
                if in_global {
                    bail!("{}:{n}: a dependency outside any project", path.display());
                }
                let Some(p) = projects.last_mut() else {
                    bail!(
                        "{}:{n}: a dependency before the first project",
                        path.display()
                    );
                };
                p.deps.push(d.trim().to_string());
            }
        }
        if !header {
            bail!(
                "{}: no `{HEADER}` line: not a workspace file",
                path.display()
            );
        }
        for p in &projects {
            for d in &p.deps {
                if !projects.iter().any(|q| q.name.eq_ignore_ascii_case(d)) {
                    bail!(
                        "{}:{}: project `{}` depends on `{d}`, which the workspace does not list",
                        path.display(),
                        p.line,
                        p.name
                    );
                }
            }
        }
        Ok(Self {
            path: path.to_path_buf(),
            projects,
        })
    }

    #[must_use]
    pub fn dir(&self) -> PathBuf {
        self.path
            .parent()
            .map_or_else(|| PathBuf::from("."), Path::to_path_buf)
    }

    #[must_use]
    pub fn project(&self, name: &str) -> Option<&DswProject> {
        self.projects
            .iter()
            .find(|p| p.name.eq_ignore_ascii_case(name))
    }

    #[must_use]
    pub fn dsp_path(&self, p: &DswProject) -> PathBuf {
        crate::dsp::join_windows(&self.dir(), &p.dsp)
    }

    pub fn build_order(&self, top: &str) -> Result<Vec<&DswProject>> {
        fn visit<'a>(
            w: &'a Dsw,
            name: &str,
            stack: &mut Vec<String>,
            out: &mut Vec<&'a DswProject>,
        ) -> Result<()> {
            let p = w
                .project(name)
                .with_context(|| format!("{}: no project `{name}`", w.path.display()))?;
            if out.iter().any(|q| q.name == p.name) {
                return Ok(());
            }
            if stack.iter().any(|s| s == &p.name) {
                bail!(
                    "{}: a dependency cycle: {} -> {}",
                    w.path.display(),
                    stack.join(" -> "),
                    p.name
                );
            }
            stack.push(p.name.clone());
            for d in &p.deps {
                visit(w, d, stack, out)?;
            }
            stack.pop();
            out.push(p);
            Ok(())
        }
        let mut out = Vec::new();
        visit(self, top, &mut Vec::new(), &mut out)?;
        Ok(out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const DSW: &str = "Microsoft Developer Studio Workspace File, Format Version 6.00\r\n\r\n\
Project: \"app\"=.\\app\\app.dsp - Package Owner=<4>\r\n\r\nPackage=<5>\r\n{{{\r\n}}}\r\n\r\nPackage=<4>\r\n{{{\r\n\
    Begin Project Dependency\r\n    Project_Dep_Name util\r\n    End Project Dependency\r\n\
    Begin Project Dependency\r\n    Project_Dep_Name zip\r\n    End Project Dependency\r\n}}}\r\n\r\n\
Project: \"util\"=.\\util\\util.dsp - Package Owner=<4>\r\n\r\nPackage=<4>\r\n{{{\r\n\
    Begin Project Dependency\r\n    Project_Dep_Name zip\r\n    End Project Dependency\r\n}}}\r\n\r\n\
Project: \"zip\"=\"..\\vendor\\zip.dsp\" - Package Owner=<4>\r\n\r\n\
Project: \"tool\"=.\\tool\\tool.dsp - Package Owner=<4>\r\n\r\nGlobal:\r\n\r\nPackage=<5>\r\n{{{\r\n}}}\r\n";

    #[test]
    fn a_workspace_builds_a_project_after_its_dependencies() {
        let w = Dsw::parse(Path::new("/w/ws.dsw"), DSW).unwrap();
        assert_eq!(w.projects.len(), 4);
        assert_eq!(w.projects[0].deps, ["util", "zip"]);
        assert_eq!(w.projects[2].dsp, "..\\vendor\\zip.dsp");
        let order: Vec<&str> = w
            .build_order("app")
            .unwrap()
            .iter()
            .map(|p| p.name.as_str())
            .collect();
        assert_eq!(order, ["zip", "util", "app"]);
        assert_eq!(w.dsp_path(&w.projects[2]), PathBuf::from("/vendor/zip.dsp"));
        assert!(w.build_order("nope").is_err());
        let cyc = DSW.replace(
            "Project_Dep_Name zip\r\n    End Project Dependency\r\n}}}\r\n\r\nProject: \"zip\"",
            "Project_Dep_Name app\r\n    End Project Dependency\r\n}}}\r\n\r\nProject: \"zip\"",
        );
        let w = Dsw::parse(Path::new("/w/ws.dsw"), &cyc).unwrap();
        assert!(format!("{:#}", w.build_order("app").unwrap_err()).contains("cycle"));
        let e =
            Dsw::parse(Path::new("/w/ws.dsw"), &DSW.replace("Name zip", "Name zap")).unwrap_err();
        assert!(e.to_string().contains("zap"), "{e}");
    }
}
