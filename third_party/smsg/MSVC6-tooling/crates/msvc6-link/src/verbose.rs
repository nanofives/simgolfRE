use anyhow::{Context as _, Result, bail};
use serde::Serialize;
use std::collections::HashSet;
use std::io::Read as _;
use std::path::Path;

pub const MAX_TEXT: u64 = 256 << 20;

pub fn read_capped(path: &Path, cap: u64) -> Result<Vec<u8>> {
    let f = std::fs::File::open(path).with_context(|| format!("reading {}", path.display()))?;
    let mut out = Vec::new();
    f.take(cap.saturating_add(1))
        .read_to_end(&mut out)
        .with_context(|| format!("reading {}", path.display()))?;
    if u64::try_from(out.len()).unwrap_or(u64::MAX) > cap {
        bail!(
            "{}: larger than the {cap} bytes a LINK log, map or object is read to",
            path.display()
        );
    }
    Ok(out)
}

pub fn read_text(path: &Path) -> Result<String> {
    Ok(msvc6_base::srcio::tool_text(&read_capped(path, MAX_TEXT)?))
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize)]
pub struct Sym {
    pub name: String,
    pub obj: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Group {
    pub survivor: Sym,
    pub replaced: Vec<Sym>,
}

impl Group {
    pub fn members(&self) -> impl Iterator<Item = &Sym> {
        std::iter::once(&self.survivor).chain(self.replaced.iter())
    }
}

#[must_use]
pub fn sym_line(line: &str) -> Option<Sym> {
    let l = line.trim();
    let (left, obj) = l.rsplit_once(" from ")?;
    let name = if left.ends_with(')') {
        let (_, m) = left.rsplit_once(" (")?;
        m.strip_suffix(')')?.to_string()
    } else {
        left.trim_matches('"').to_string()
    };
    (!name.is_empty()).then(|| Sym {
        name,
        obj: obj.trim().to_string(),
    })
}

#[must_use]
pub fn groups(log: &str) -> Vec<Group> {
    let mut out: Vec<Group> = Vec::new();
    let mut state = 0u8;
    for raw in log.lines() {
        let t = raw.trim();
        if t == "Selected symbol:" {
            state = 1;
            continue;
        }
        if t == "Replaced symbol(s):" {
            state = 2;
            continue;
        }
        match state {
            1 => {
                if let Some(s) = sym_line(t) {
                    out.push(Group {
                        survivor: s,
                        replaced: Vec::new(),
                    });
                }
                state = 0;
            }
            2 => match (raw.starts_with("        "), sym_line(t), out.last_mut()) {
                (true, Some(s), Some(g)) => g.replaced.push(s),
                _ => state = 0,
            },
            _ => {}
        }
    }
    out
}

#[must_use]
pub fn found_symbol(rest: &str) -> String {
    let r = rest.trim();
    if r.starts_with('"')
        && r.ends_with(')')
        && let Some(open) = r.rfind(" (")
        && let Some(inner) = r.get(open + 2..r.len() - 1)
    {
        return inner.to_string();
    }
    r.to_string()
}

#[must_use]
pub fn is_import_member(member: &str) -> bool {
    let m = member.to_ascii_lowercase();
    m.ends_with(".dll") || m.ends_with(".exe") || m.ends_with(".drv")
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Kind {
    Import,
    Descriptor,
    NullDescriptor,
    NullThunk,
}

#[must_use]
pub fn classify(symbol: &str) -> Kind {
    if symbol.starts_with("__IMPORT_DESCRIPTOR_") {
        Kind::Descriptor
    } else if symbol == "__NULL_IMPORT_DESCRIPTOR" {
        Kind::NullDescriptor
    } else if symbol.ends_with("_NULL_THUNK_DATA") {
        Kind::NullThunk
    } else {
        Kind::Import
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Load {
    pub lib: String,
    pub member: String,
    pub symbol: String,
    pub first_ref: Option<String>,
}

impl Load {
    #[must_use]
    pub fn spelled(&self) -> String {
        format!("{}({})", self.lib, self.member)
    }

    #[must_use]
    pub fn is_import(&self) -> bool {
        is_import_member(&self.member)
    }

    #[must_use]
    pub fn kind(&self) -> Kind {
        classify(&self.symbol)
    }
}

fn split_loaded(rest: &str) -> Option<(&str, &str)> {
    let m = rest.trim();
    let inner = m.strip_suffix(')')?;
    let open = inner.rfind('(')?;
    Some((inner.get(..open)?, inner.get(open + 1..)?))
}

#[must_use]
pub fn loads(log: &str) -> Vec<Load> {
    let mut out = Vec::new();
    let mut in_exp = false;
    let mut cur = String::new();
    let mut first_ref: Option<String> = None;
    for raw in log.lines() {
        let line = raw.trim_end_matches('\r').trim();
        if line == "Start Generating .exp file" {
            in_exp = true;
            continue;
        }
        if line == "End Generating .exp file" {
            in_exp = false;
            continue;
        }
        if in_exp {
            continue;
        }
        if let Some(rest) = line.strip_prefix("Found ") {
            cur = found_symbol(rest);
            first_ref = None;
        } else if let Some(rest) = line.strip_prefix("Referenced in ") {
            if first_ref.is_none() {
                first_ref = Some(rest.trim().to_string());
            }
        } else if let Some(rest) = line.strip_prefix("Loaded ")
            && let Some((lib, member)) = split_loaded(rest)
        {
            out.push(Load {
                lib: lib.to_string(),
                member: member.to_string(),
                symbol: cur.clone(),
                first_ref: first_ref.clone(),
            });
        }
    }
    out
}

#[must_use]
pub fn discarded(log: &str) -> HashSet<(String, String)> {
    log.lines()
        .filter_map(|l| l.trim().strip_prefix("Discarded "))
        .filter_map(|r| sym_line(r).map(|s| (s.name, s.obj.to_ascii_lowercase())))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_group_reads_survivor_and_replaced_with_their_objects() {
        let log = "    Discarded x\n    Selected symbol:\n        \"void f(void)\" (?f@@YAXXZ) from A.obj\n    \
                   Replaced symbol(s):\n        \"void g(void)\" (?g@@YAXXZ) from B.obj\n        _vendor_term from \
                   vendor.lib(TERM.OBJ)\n    Discarded y\n";
        let g = groups(log);
        assert_eq!(g.len(), 1);
        assert_eq!(
            g[0].survivor,
            Sym {
                name: "?f@@YAXXZ".into(),
                obj: "A.obj".into()
            }
        );
        assert_eq!(g[0].replaced.len(), 2);
        assert_eq!(g[0].replaced[1].obj, "vendor.lib(TERM.OBJ)");
        assert_eq!(g[0].replaced[1].name, "_vendor_term");
    }

    #[test]
    fn a_demangled_name_with_parentheses_keeps_the_mangled_one() {
        let s = sym_line(
            "\"void __stdcall _CallMemberFunction0(void *,void *)\" (?_C@@YGXPAX0@Z) from libcmt.lib(trnsctrl.obj)",
        );
        assert_eq!(
            s,
            Some(Sym {
                name: "?_C@@YGXPAX0@Z".into(),
                obj: "libcmt.lib(trnsctrl.obj)".into()
            })
        );
    }

    #[test]
    fn loads_read_pulls_and_skip_the_exp_pass() {
        let log = "Start Pass1\r\nSearching Libraries\r\n    Searching C:\\L\\kernel32.lib:\r\n      Found __imp__Sleep@4\r\n        \
                   Referenced in A.obj\r\n        Loaded kernel32.lib(KERNEL32.dll)\r\n      Found \"public: void __thiscall \
                   T::f(void)\" (__imp_?f@T@@QAEXXZ)\r\n        Referenced in B.obj\r\n        Loaded t.lib(T.dll)\r\n      Found \
                   _strlen\r\n        Loaded libcmt.lib(strlen.obj)\r\nStart Generating .exp file\r\n      Found __imp__Beep@8\r\n        \
                   Loaded kernel32.lib(KERNEL32.dll)\r\nEnd Generating .exp file\r\n      Found \x7fKERNEL32_NULL_THUNK_DATA\r\n        \
                   Loaded kernel32.lib(KERNEL32.dll)\r\n";
        let p = loads(log);
        assert_eq!(p.len(), 4);
        assert_eq!(p[0].symbol, "__imp__Sleep@4");
        assert_eq!(p[0].first_ref.as_deref(), Some("A.obj"));
        assert_eq!(p[1].symbol, "__imp_?f@T@@QAEXXZ");
        assert_eq!(p[1].member, "T.dll");
        assert_eq!(p[2].spelled(), "libcmt.lib(strlen.obj)");
        assert!(!p[2].is_import());
        assert_eq!(p[3].kind(), Kind::NullThunk);
    }

    #[test]
    fn a_library_path_with_parentheses_keeps_its_member() {
        let p = loads("      Found _x\r\n        Loaded C:\\Program Files (x86)\\v.lib(x.obj)\r\n");
        assert_eq!(p[0].member, "x.obj");
        assert_eq!(p[0].lib, "C:\\Program Files (x86)\\v.lib");
    }

    #[test]
    fn discarded_names_carry_their_object_in_lower_case() {
        let d = discarded(
            "    Discarded _dead from A.obj\n    Discarded \"f\" (?f@@YAXXZ) from Lib.lib(B.OBJ)\n",
        );
        assert!(d.contains(&("_dead".to_string(), "a.obj".to_string())));
        assert!(d.contains(&("?f@@YAXXZ".to_string(), "lib.lib(b.obj)".to_string())));
    }
}
