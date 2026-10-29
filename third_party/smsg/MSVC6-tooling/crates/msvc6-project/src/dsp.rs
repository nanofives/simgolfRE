use crate::switches;
use anyhow::{Context as _, Result};
use std::path::{Path, PathBuf};

pub const CL_EXTENSIONS: [&str; 4] = ["c", "cpp", "cxx", "cc"];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dsp {
    pub path: PathBuf,
    pub name: String,
    pub target_type: Option<String>,
    pub default_config: Option<String>,
    pub configs: Vec<String>,
    lines: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct FileBlock {
    pub source: String,
    pub line: usize,
    pub group: Option<String>,
    pub add: Vec<String>,
    pub sub: Vec<String>,
    pub excluded: bool,
    pub intermediate_dir: Option<String>,
    pub unmodelled: Vec<String>,
}

impl FileBlock {
    #[must_use]
    pub fn extension(&self) -> String {
        let name = self.source.rsplit(['\\', '/']).next().unwrap_or_default();
        name.rsplit_once('.')
            .map(|(_, e)| e.to_ascii_lowercase())
            .unwrap_or_default()
    }

    #[must_use]
    pub fn is_tu(&self) -> bool {
        CL_EXTENSIONS.contains(&self.extension().as_str()) && !self.excluded
    }

    #[must_use]
    pub fn stem(&self) -> String {
        let name = self.source.rsplit(['\\', '/']).next().unwrap_or_default();
        name.rsplit_once('.')
            .map_or_else(|| name.to_string(), |(s, _)| s.to_string())
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Settings {
    pub config: String,
    pub global_add: Vec<String>,
    pub global_sub: Vec<String>,
    pub output_dir: Option<String>,
    pub intermediate_dir: Option<String>,
    pub files: Vec<FileBlock>,
    pub unmodelled: Vec<String>,
}

impl Settings {
    #[must_use]
    pub fn switches(&self, f: &FileBlock) -> Vec<String> {
        switches::effective(&self.global_add, &self.global_sub, &f.add, &f.sub)
    }
}

fn cfg_condition(rest: &str) -> Option<String> {
    let r = rest.trim().strip_prefix("\"$(CFG)\"")?.trim_start();
    let r = r.strip_prefix("==")?.trim();
    let c = r.strip_prefix('"')?.strip_suffix('"')?;
    (!c.contains('"')).then(|| c.to_string())
}

fn quoted_value(s: &str) -> String {
    s.trim().trim_matches('"').to_string()
}

#[must_use]
pub fn project_name(first: &str) -> Option<String> {
    let rest = first.strip_prefix("# Microsoft Developer Studio Project File - Name=\"")?;
    let (name, _) = rest.split_once('"')?;
    (!name.is_empty()).then(|| name.to_string())
}

impl Dsp {
    pub fn read(path: &Path) -> Result<Self> {
        let text =
            msvc6_base::srcio::read(path).with_context(|| format!("reading {}", path.display()))?;
        Self::parse(path, &text)
    }

    pub fn parse(path: &Path, text: &str) -> Result<Self> {
        let lines: Vec<String> = text
            .lines()
            .map(|l| l.trim_end_matches('\r').to_string())
            .collect();
        let first = lines.first().map(String::as_str).unwrap_or_default();
        let name = project_name(first).with_context(|| {
            format!(
                "{}:1: not a Developer Studio project file (the first line is `{first}`, not `# Microsoft \
                 Developer Studio Project File - Name=\"<name>\" ...`)",
                path.display()
            )
        })?;
        let target_type = lines.iter().find_map(|l| {
            let r = l.strip_prefix("# TARGTYPE ")?;
            let r = r.trim().strip_prefix('"')?;
            r.split_once('"').map(|(t, _)| t.to_string())
        });
        let default_config = lines
            .iter()
            .find_map(|l| l.strip_prefix("CFG=").map(|c| c.trim().to_string()));
        let mut configs = Vec::new();
        let mut in_target = false;
        for l in &lines {
            if l == "# Begin Target" {
                in_target = true;
            } else if l == "# End Target" {
                in_target = false;
            } else if in_target && let Some(n) = l.strip_prefix("# Name ") {
                configs.push(quoted_value(n));
            }
        }
        Ok(Self {
            path: path.to_path_buf(),
            name,
            target_type,
            default_config,
            configs,
            lines,
        })
    }

    #[must_use]
    pub fn dir(&self) -> PathBuf {
        self.path
            .parent()
            .map_or_else(|| PathBuf::from("."), Path::to_path_buf)
    }

    #[must_use]
    pub fn is_static_library(&self) -> bool {
        self.target_type
            .as_deref()
            .is_some_and(|t| t.contains("Static Library"))
    }

    #[must_use]
    pub fn settings(&self, config: &str) -> Settings {
        let mut s = Settings {
            config: config.to_string(),
            ..Settings::default()
        };
        if !self.configs.iter().any(|c| c == config) {
            s.unmodelled.push(format!(
                "{}: no configuration `{config}`; it has {}",
                self.path.display(),
                if self.configs.is_empty() {
                    "none".to_string()
                } else {
                    self.configs.join(", ")
                }
            ));
        }
        let mut conds: Vec<(bool, bool)> = Vec::new();
        let mut in_target = false;
        let mut block: Option<(FileBlock, usize, bool, bool)> = None;
        let mut custom = false;
        let mut group: Vec<String> = Vec::new();
        let mut gadd = false;
        let mut gsub = false;
        for (i, l) in self.lines.iter().enumerate() {
            let n = i.saturating_add(1);
            let at = |why: &str| format!("{}:{n}: `{l}`: {why}", self.path.display());
            let on = conds.iter().all(|(a, _)| *a);
            if let Some(rest) = l.strip_prefix('!') {
                let (word, arg) = rest.split_once(char::is_whitespace).unwrap_or((rest, ""));
                match word {
                    "IF" | "ELSEIF" => match cfg_condition(arg) {
                        Some(c) => {
                            let hit = c == config;
                            if word == "IF" {
                                conds.push((hit, hit));
                            } else if let Some(top) = conds.last_mut() {
                                *top = (hit && !top.1, top.1 || hit);
                            } else {
                                s.unmodelled.push(at("an !ELSEIF with no !IF"));
                            }
                        }
                        None => {
                            s.unmodelled.push(at(
                                "only a `\"$(CFG)\" == \"<configuration>\"` condition is read",
                            ));
                            conds.push((false, true));
                        }
                    },
                    "ELSE" => match conds.last_mut() {
                        Some(top) => *top = (!top.1, true),
                        None => s.unmodelled.push(at("an !ELSE with no !IF")),
                    },
                    "ENDIF" => {
                        if conds.pop().is_none() {
                            s.unmodelled.push(at("an !ENDIF with no !IF"));
                        }
                    }
                    "MESSAGE" => {}
                    _ => s
                        .unmodelled
                        .push(at("a directive this reader does not model")),
                }
                continue;
            }
            match l.as_str() {
                "# Begin Target" => {
                    in_target = true;
                    continue;
                }
                "# End Target" => {
                    in_target = false;
                    continue;
                }
                "# Begin Source File" => {
                    if block.is_some() {
                        s.unmodelled
                            .push(at("a source block inside a source block"));
                    }
                    let b = FileBlock {
                        line: n,
                        group: group.last().cloned(),
                        ..FileBlock::default()
                    };
                    block = Some((b, conds.len(), false, false));
                    continue;
                }
                "# End Source File" => {
                    match block.take() {
                        Some((mut b, depth, _, _)) => {
                            if conds.len() != depth {
                                b.unmodelled.push(format!(
                                    "{}:{}: its conditionals do not close inside the block",
                                    self.path.display(),
                                    b.line
                                ));
                            }
                            if b.source.is_empty() {
                                s.unmodelled.push(format!(
                                    "{}:{}: a source block with no SOURCE=",
                                    self.path.display(),
                                    b.line
                                ));
                            }
                            s.files.push(b);
                        }
                        None => s.unmodelled.push(at("an # End Source File with no block")),
                    }
                    continue;
                }
                b if b.starts_with("# Begin Custom Build")
                    || b.starts_with("# Begin Special Build Tool") =>
                {
                    custom = true;
                    let what = if b.starts_with("# Begin Custom Build") {
                        "custom build step"
                    } else {
                        "pre-link or post-build step (a Special Build Tool)"
                    };
                    match block.as_mut() {
                        Some((f, ..)) if on => f
                            .unmodelled
                            .push(at(&format!("a per-file {what} is not modelled"))),
                        None if on => s
                            .unmodelled
                            .push(at(&format!("a project-level {what} is not modelled"))),
                        _ => {}
                    }
                    continue;
                }
                e if e.starts_with("# End Custom Build")
                    || e.starts_with("# End Special Build Tool") =>
                {
                    custom = false;
                    continue;
                }
                _ => {}
            }
            if custom {
                continue;
            }
            if let Some(g) = l.strip_prefix("# Begin Group ") {
                group.push(quoted_value(g));
                continue;
            }
            if l == "# End Group" {
                group.pop();
                continue;
            }
            if let Some((b, _, add_seen, sub_seen)) = block.as_mut() {
                if l.is_empty() {
                    continue;
                }
                if let Some(v) = l.strip_prefix("SOURCE=") {
                    if !b.source.is_empty() {
                        b.unmodelled.push(at("a second SOURCE= in one block"));
                    }
                    b.source = quoted_value(v);
                } else if let Some(f) = l.strip_prefix("# ADD CPP") {
                    if on {
                        if *add_seen {
                            b.unmodelled.push(at("a second active per-file # ADD CPP"));
                        }
                        *add_seen = true;
                        b.add = switches::tokens(f);
                    }
                } else if let Some(f) = l.strip_prefix("# SUBTRACT CPP") {
                    if on {
                        if *sub_seen {
                            b.unmodelled
                                .push(at("a second active per-file # SUBTRACT CPP"));
                        }
                        *sub_seen = true;
                        b.sub = switches::tokens(f);
                    }
                } else if let Some(v) = l.strip_prefix("# PROP Exclude_From_Build ") {
                    if on {
                        b.excluded = v.trim() != "0";
                    }
                } else if let Some(v) = l.strip_prefix("# PROP Intermediate_Dir ") {
                    if on {
                        b.intermediate_dir = Some(quoted_value(v));
                    }
                } else if l.starts_with("# ADD BASE ")
                    || l.starts_with("# SUBTRACT BASE ")
                    || l.starts_with("# PROP BASE ")
                    || l.starts_with("DEP_CPP_")
                    || l.starts_with("NODEP_CPP_")
                    || l.starts_with("# ADD RSC")
                    || l.starts_with("# SUBTRACT RSC")
                    || l.starts_with("# ADD MTL")
                    || l.starts_with("# PROP Ignore_Default_Tool 0")
                    || l.starts_with('\t')
                    || l.starts_with(' ')
                {
                } else if on {
                    b.unmodelled
                        .push(at("a line in a source block this reader does not model"));
                }
                continue;
            }
            if l.starts_with("SOURCE=") {
                s.unmodelled
                    .push(at("a SOURCE= outside a # Begin Source File block"));
                continue;
            }
            if in_target || !on {
                continue;
            }
            if let Some(f) = l.strip_prefix("# ADD CPP") {
                if gadd {
                    s.unmodelled
                        .push(at("a second active project-wide # ADD CPP"));
                }
                gadd = true;
                s.global_add = switches::tokens(f);
            } else if let Some(f) = l.strip_prefix("# SUBTRACT CPP") {
                if gsub {
                    s.unmodelled
                        .push(at("a second active project-wide # SUBTRACT CPP"));
                }
                gsub = true;
                s.global_sub = switches::tokens(f);
            } else if let Some(v) = l.strip_prefix("# PROP Output_Dir ") {
                s.output_dir = Some(quoted_value(v));
            } else if let Some(v) = l.strip_prefix("# PROP Intermediate_Dir ") {
                s.intermediate_dir = Some(quoted_value(v));
            }
        }
        if !conds.is_empty() {
            s.unmodelled.push(format!(
                "{}: {} !IF left open at the end",
                self.path.display(),
                conds.len()
            ));
        }
        if block.is_some() {
            s.unmodelled.push(format!(
                "{}: a source block left open at the end",
                self.path.display()
            ));
        }
        if !gadd && s.unmodelled.is_empty() {
            s.unmodelled.push(format!(
                "{}: configuration `{config}` has no project-wide # ADD CPP",
                self.path.display()
            ));
        }
        s
    }
}

#[must_use]
pub fn join_windows(dir: &Path, rel: &str) -> PathBuf {
    let mut p = dir.to_path_buf();
    for c in rel
        .split(['\\', '/'])
        .filter(|c| !c.is_empty() && *c != ".")
    {
        if c == ".." && p.file_name().is_some() {
            p.pop();
        } else {
            p.push(c);
        }
    }
    msvc6_toolchain::layout::resolve_ci(&p).unwrap_or(p)
}

#[cfg(test)]
mod tests {
    use super::*;

    const DSP: &str = "# Microsoft Developer Studio Project File - Name=\"app\" - Package Owner=<4>\r\n\
# TARGTYPE \"Win32 (x86) Application\" 0x0101\r\n\
CFG=app - Win32 Debug\r\n\
!MESSAGE \"app - Win32 Release\" (based on \"Win32 (x86) Application\")\r\n\
!IF  \"$(CFG)\" == \"app - Win32 Release\"\r\n\
# PROP Intermediate_Dir \"Release\"\r\n\
# ADD BASE CPP /nologo /W3 /c\r\n\
# ADD CPP /nologo /W3 /MT /O2 /D \"WIN32\" /D \"NDEBUG\" /YX /FD /c\r\n\
!ELSEIF  \"$(CFG)\" == \"app - Win32 Debug\"\r\n\
# PROP Intermediate_Dir \"Debug\"\r\n\
# ADD CPP /nologo /W3 /Gm /MTd /Od /ZI /D \"WIN32\" /D \"_DEBUG\" /YX /FD /GZ /c\r\n\
# SUBTRACT CPP /YX\r\n\
!ENDIF\r\n\
# Begin Target\r\n\
# Name \"app - Win32 Release\"\r\n\
# Name \"app - Win32 Debug\"\r\n\
# Begin Group \"Source Files\"\r\n\
# Begin Source File\r\n\
SOURCE=.\\src\\a.cpp\r\n\
!IF  \"$(CFG)\" == \"app - Win32 Release\"\r\n\
# ADD CPP /GX /Ob2\r\n\
# SUBTRACT CPP /O2\r\n\
!ELSEIF  \"$(CFG)\" == \"app - Win32 Debug\"\r\n\
# PROP Exclude_From_Build 1\r\n\
!ENDIF\r\n\
# End Source File\r\n\
# Begin Source File\r\n\
SOURCE=.\\src\\b.c\r\n\
# End Source File\r\n\
# Begin Source File\r\n\
SOURCE=.\\app.rc\r\n\
# End Source File\r\n\
# End Group\r\n\
# End Target\r\n\
# End Project\r\n";

    #[test]
    fn each_configuration_reads_its_own_arm_and_its_files_own_switches() {
        let d = Dsp::parse(Path::new("/p/app/app.dsp"), DSP).unwrap();
        assert_eq!(d.name, "app");
        assert_eq!(d.configs, ["app - Win32 Release", "app - Win32 Debug"]);
        assert_eq!(d.default_config.as_deref(), Some("app - Win32 Debug"));
        let r = d.settings("app - Win32 Release");
        assert!(r.unmodelled.is_empty(), "{:?}", r.unmodelled);
        assert_eq!(r.intermediate_dir.as_deref(), Some("Release"));
        let tus: Vec<&FileBlock> = r.files.iter().filter(|f| f.is_tu()).collect();
        assert_eq!(tus.len(), 2);
        assert_eq!(tus[0].group.as_deref(), Some("Source Files"));
        assert_eq!(
            r.switches(tus[0]),
            [
                "/nologo", "/W3", "/MT", "/DWIN32", "/DNDEBUG", "/YX", "/FD", "/c", "/GX", "/Ob2"
            ]
        );
        assert_eq!(tus[1].stem(), "b");
        let g = d.settings("app - Win32 Debug");
        assert!(g.unmodelled.is_empty(), "{:?}", g.unmodelled);
        let tus: Vec<&FileBlock> = g.files.iter().filter(|f| f.is_tu()).collect();
        assert_eq!(tus.len(), 1);
        assert_eq!(
            g.switches(tus[0]),
            [
                "/nologo", "/W3", "/Gm", "/MTd", "/Od", "/ZI", "/DWIN32", "/D_DEBUG", "/FD", "/GZ",
                "/c"
            ]
        );
        let none = d.settings("app - Win32 Profile");
        assert!(
            none.unmodelled[0].contains("no configuration"),
            "{:?}",
            none.unmodelled
        );
    }

    #[test]
    fn what_the_reader_does_not_model_is_named_by_line() {
        let bad = DSP.replace(
            "# PROP Exclude_From_Build 1",
            "# Begin Custom Build\r\nx\r\n# End Custom Build",
        );
        let d = Dsp::parse(Path::new("app.dsp"), &bad).unwrap();
        let g = d.settings("app - Win32 Debug");
        let a = g.files.iter().find(|f| f.stem() == "a").unwrap();
        assert!(
            a.unmodelled[0].contains("custom build"),
            "{:?}",
            a.unmodelled
        );
        let odd = DSP.replace(
            "!ENDIF\r\n# Begin Target",
            "!IF \"$(X)\" == \"y\"\r\n!ENDIF\r\n!ENDIF\r\n# Begin Target",
        );
        let g = Dsp::parse(Path::new("app.dsp"), &odd)
            .unwrap()
            .settings("app - Win32 Debug");
        assert!(
            g.unmodelled.iter().any(|u| u.contains(":13:")),
            "{:?}",
            g.unmodelled
        );
        assert!(Dsp::parse(Path::new("x.dsp"), "hello\n").is_err());
        let post = "# Begin Custom Build - Performing registration\r\nOutDir=.\\Out\r\nSOURCE=\"$(InputPath)\"\r\n\r\n\"$(OutDir)\\reg.trg\" : $(SOURCE)\r\n\tregsvr32 /s \"$(TargetPath)\"\r\n# End Custom Build\r\n";
        let reg = DSP.replace(
            "# SUBTRACT CPP /YX\r\n!ENDIF",
            &format!("# SUBTRACT CPP /YX\r\n{post}!ENDIF"),
        );
        let d = Dsp::parse(Path::new("app.dsp"), &reg).unwrap();
        let g = d.settings("app - Win32 Debug");
        assert_eq!(g.unmodelled.len(), 1, "{:?}", g.unmodelled);
        assert!(
            g.unmodelled[0]
                .starts_with("app.dsp:13: `# Begin Custom Build - Performing registration`")
                && g.unmodelled[0].contains("a project-level custom build step is not modelled"),
            "{:?}",
            g.unmodelled
        );
        let r = d.settings("app - Win32 Release");
        assert!(r.unmodelled.is_empty(), "{:?}", r.unmodelled);
    }

    #[test]
    fn an_escaped_quote_in_a_define_stays_one_switch() {
        let q = DSP.replace(
            "# ADD CPP /GX /Ob2",
            "# ADD CPP /D VERSION=\\\"1.0\\\" /D \"NAME=\\\"a b\\\"\"",
        );
        let d = Dsp::parse(Path::new("app.dsp"), &q).unwrap();
        let r = d.settings("app - Win32 Release");
        let a = r.files.iter().find(|f| f.stem() == "a").unwrap();
        let sw = r.switches(a);
        assert_eq!(
            sw.get(sw.len() - 2..).unwrap(),
            ["/DVERSION=\"1.0\"", "/DNAME=\"a b\""]
        );
    }

    #[test]
    fn a_windows_path_joins_lexically() {
        assert_eq!(
            join_windows(
                Path::new("/nowhere/game/game.exe"),
                "..\\engine.lib\\engine.dsp"
            ),
            PathBuf::from("/nowhere/game/engine.lib/engine.dsp")
        );
        assert_eq!(
            join_windows(Path::new("/nowhere/g"), ".\\a\\b.dsp"),
            PathBuf::from("/nowhere/g/a/b.dsp")
        );
    }
}
