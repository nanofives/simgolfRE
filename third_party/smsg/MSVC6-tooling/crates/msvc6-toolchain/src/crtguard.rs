use std::collections::BTreeMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Crt {
    Native,
    Builtin,
    None,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Proc {
    pub exe: String,
    pub crt: Crt,
}

fn parse_line(l: &str) -> Option<(&str, String, &str)> {
    let (tid, rest) = l.split_once(':')?;
    if !rest.starts_with("trace:loaddll:") {
        return None;
    }
    let q = rest.find("Loaded L\"")? + "Loaded L\"".len();
    let tail = rest.get(q..)?;
    let end = tail.find('"')?;
    let path = tail.get(..end)?.replace("\\\\", "\\");
    let how = tail.get(end..)?.rsplit(": ").next()?.trim();
    Some((tid, path, how))
}

fn base(path: &str) -> &str {
    path.rsplit(['\\', '/']).next().unwrap_or(path)
}

pub fn native_modules(log: &str) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    for l in log.lines() {
        if let Some((_, path, how)) = parse_line(l)
            && how == "native"
            && !out.contains(&path)
        {
            out.push(path);
        }
    }
    out
}

pub fn processes(log: &str) -> Vec<Proc> {
    let mut procs: Vec<Proc> = Vec::new();
    let mut by_tid: BTreeMap<String, usize> = BTreeMap::new();
    for l in log.lines() {
        let Some((tid, path, how)) = parse_line(l) else {
            continue;
        };
        let name = base(&path).to_ascii_lowercase();
        if name.ends_with(".exe") && (!by_tid.contains_key(tid) || l.contains(" at 00400000:")) {
            by_tid.insert(tid.to_string(), procs.len());
            procs.push(Proc {
                exe: path,
                crt: Crt::None,
            });
            continue;
        }
        if name == "msvcrt.dll"
            && let Some(p) = by_tid.get(tid).and_then(|&i| procs.get_mut(i))
            && p.crt == Crt::None
        {
            p.crt = if how == "native" {
                Crt::Native
            } else {
                Crt::Builtin
            };
        }
    }
    procs
}

fn is_wine_own(exe: &str) -> bool {
    exe.to_ascii_lowercase().starts_with("c:\\windows\\")
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Tally {
    pub native: usize,
    pub builtin: usize,
    pub none: usize,
}

pub fn tally(procs: &[Proc]) -> BTreeMap<String, Tally> {
    let mut per: BTreeMap<String, Tally> = BTreeMap::new();
    for p in procs.iter().filter(|p| !is_wine_own(&p.exe)) {
        let t = per.entry(base(&p.exe).to_ascii_uppercase()).or_default();
        match p.crt {
            Crt::Native => t.native += 1,
            Crt::Builtin => t.builtin += 1,
            Crt::None => t.none += 1,
        }
    }
    per
}

pub fn summary(per: &BTreeMap<String, Tally>) -> String {
    per.iter()
        .map(|(k, t)| {
            let mut s = format!("{k} {} native", t.native);
            if t.builtin > 0 {
                s.push_str(&format!(" {} BUILTIN", t.builtin));
            }
            if t.none > 0 {
                s.push_str(&format!(" {} none", t.none));
            }
            s
        })
        .collect::<Vec<_>>()
        .join(", ")
}

pub fn verdict(log: &str, require: &[String]) -> std::result::Result<String, String> {
    let procs = processes(log);
    let per = tally(&procs);
    let sum = summary(&per);
    let bad: Vec<&str> = procs
        .iter()
        .filter(|p| !is_wine_own(&p.exe) && p.crt == Crt::Builtin)
        .map(|p| p.exe.as_str())
        .collect();
    if !bad.is_empty() {
        return Err(format!(
            "{} process(es) ran on wine's BUILTIN msvcrt: {} [{sum}]",
            bad.len(),
            bad.join(", ")
        ));
    }
    let require: Vec<String> = if require.is_empty() {
        vec!["LINK.EXE".to_string()]
    } else {
        require.iter().map(|r| r.to_ascii_uppercase()).collect()
    };
    for r in &require {
        if per.get(r).is_none_or(|t| t.native == 0) {
            return Err(format!(
                "no {r} process mapped the native msvcrt: the log does not show {r} running, so it proves nothing [{}]",
                if sum.is_empty() {
                    "no tool processes"
                } else {
                    &sum
                }
            ));
        }
    }
    Ok(sum)
}

pub fn line(log: &str, require: &[String]) -> (String, bool) {
    match verdict(log, require) {
        Ok(s) => (
            format!("CRT GUARD: PASS -- every tool process mapped the native msvcrt.dll ({s})"),
            true,
        ),
        Err(e) => (format!("CRT GUARD: FAIL -- {e}"), false),
    }
}

pub fn non_trace(log: &str) -> String {
    log.lines()
        .filter(|l| !l.contains(":trace:loaddll:"))
        .map(|l| format!("{l}\n"))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const LOG: &str = r#"002c:trace:loaddll:build_module Loaded L"C:\\windows\\system32\\wineboot.exe" at 00400000: builtin
002c:trace:loaddll:build_module Loaded L"C:\\windows\\system32\\msvcrt.dll" at 78000000: native
010c:trace:loaddll:build_module Loaded L"Z:\\t\\VC98\\BIN\\LINK.EXE" at 00400000: native
010c:trace:loaddll:build_module Loaded L"C:\\windows\\system32\\kernel32.dll" at 7B600000: builtin
010c:trace:loaddll:build_module Loaded L"C:\\windows\\system32\\MSVCRT.dll" at 78000000: native
"#;

    #[test]
    fn a_native_link_passes_and_a_builtin_one_fails() {
        let (l, ok) = line(LOG, &[]);
        assert!(ok, "{l}");
        assert!(l.contains("LINK.EXE 1 native"), "{l}");
        let b = LOG.replace("78000000: native", "65680000: builtin");
        let (l, ok) = line(&b, &[]);
        assert!(!ok && l.contains("BUILTIN"), "{l}");
        let only_wine: String = LOG.lines().take(2).map(|l| format!("{l}\n")).collect();
        let (l, ok) = line(&only_wine, &["link.exe".into()]);
        assert!(!ok && l.contains("proves nothing"), "{l}");
    }

    #[test]
    fn a_reissued_thread_id_starts_a_new_process() {
        let two = format!(
            "{LOG}010c:trace:loaddll:build_module Loaded L\"Z:\\\\t\\\\CL.EXE\" at 00400000: native\n\
             010c:trace:loaddll:build_module Loaded L\"C:\\\\windows\\\\system32\\\\msvcrt.dll\" at 65680000: builtin\n\
             0100:fixme:x:y not a trace line\n"
        );
        let p = processes(&two);
        assert_eq!(p.len(), 3);
        assert_eq!(p[2].crt, Crt::Builtin);
        assert!(verdict(&two, &[]).unwrap_err().contains("CL.EXE"));
        assert_eq!(non_trace(&two), "0100:fixme:x:y not a trace line\n");
    }
}
