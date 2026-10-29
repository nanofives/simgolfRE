use anyhow::{Context as _, Result, bail};
use std::path::Path;

pub const MAX_MAP: u64 = 256 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Symbol {
    pub section: u16,
    pub offset: u32,
    pub name: String,
    pub va: u32,
    pub rva: u32,
    pub function: bool,
    pub inline: bool,
    pub is_static: bool,
    pub lib: Option<String>,
    pub object: String,
    pub line: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Map {
    pub module: String,
    pub base: u32,
    pub entry: Option<(u16, u32)>,
    pub symbols: Vec<Symbol>,
}

fn hex(s: &str) -> Option<u32> {
    u32::from_str_radix(s, 16).ok()
}

fn row(l: &str, base: u32, is_static: bool, line: usize) -> Option<Symbol> {
    let f: Vec<&str> = l.split_whitespace().collect();
    let (addr, name, va, last) = (f.first()?, f.get(1)?, f.get(2)?, f.last()?);
    let (sect, off) = addr.split_once(':')?;
    let section = u16::from_str_radix(sect, 16).ok()?;
    let offset = hex(off)?;
    let va = hex(va)?;
    if f.len() < 4 || section == 0 {
        return None;
    }
    let flags = f.get(3..f.len().saturating_sub(1)).unwrap_or_default();
    if flags.iter().any(|t| !matches!(*t, "f" | "i")) {
        return None;
    }
    let (lib, object) = match last.rsplit_once(':') {
        Some((l, o)) => (Some(l.to_string()), o.to_string()),
        None => (None, (*last).to_string()),
    };
    Some(Symbol {
        section,
        offset,
        name: (*name).to_string(),
        va,
        rva: va.checked_sub(base)?,
        function: flags.contains(&"f"),
        inline: flags.contains(&"i"),
        is_static,
        lib,
        object,
        line,
    })
}

impl Map {
    pub fn read(path: &Path) -> Result<Self> {
        let text = msvc6_base::srcio::read_text_bounded(path, MAX_MAP)
            .with_context(|| format!("reading {}", path.display()))?;
        Self::parse(path, &text)
    }

    pub fn parse(path: &Path, text: &str) -> Result<Self> {
        let mut module = String::new();
        let mut base: Option<u32> = None;
        let mut entry = None;
        let mut symbols = Vec::new();
        let mut section: Option<bool> = None;
        for (i, raw) in text.lines().enumerate() {
            let n = i.saturating_add(1);
            let l = raw.trim_end_matches('\r');
            let t = l.trim();
            if module.is_empty() && !t.is_empty() {
                t.clone_into(&mut module);
                continue;
            }
            if let Some(r) = t.strip_prefix("Preferred load address is ") {
                base = Some(hex(r.trim()).with_context(|| {
                    format!(
                        "{}:{n}: a load address that is not hexadecimal: `{r}`",
                        path.display()
                    )
                })?);
                continue;
            }
            if t.contains("Publics by Value") {
                section = Some(false);
                continue;
            }
            if t == "Static symbols" {
                section = Some(true);
                continue;
            }
            if let Some(r) = t.strip_prefix("entry point at") {
                if let Some((s, o)) = r.trim().split_once(':') {
                    entry = u16::from_str_radix(s, 16).ok().zip(hex(o));
                }
                continue;
            }
            if t.starts_with("FIXUPS:") || t.starts_with("Exports") {
                section = None;
                continue;
            }
            let Some(is_static) = section else {
                continue;
            };
            let Some(b) = base else {
                bail!(
                    "{}:{n}: a symbol row before the `Preferred load address` line",
                    path.display()
                );
            };
            if let Some(s) = row(l, b, is_static, n) {
                symbols.push(s);
            }
        }
        let base = base.with_context(|| {
            format!(
                "{}: no `Preferred load address is` line: not a LINK /MAP",
                path.display()
            )
        })?;
        Ok(Self {
            module,
            base,
            entry,
            symbols,
        })
    }
}

impl Symbol {
    #[must_use]
    pub fn owner_token(&self) -> String {
        match &self.lib {
            Some(l) => format!("{l}:{}", self.object),
            None => self.object.clone(),
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct Owners {
    own: std::collections::BTreeSet<String>,
}

impl Owners {
    #[must_use]
    pub fn new(own: &[String]) -> Self {
        Self {
            own: own.iter().map(|s| s.to_ascii_lowercase()).collect(),
        }
    }

    #[must_use]
    pub fn is_own(&self, lib: &str) -> bool {
        self.own.contains(&lib.to_ascii_lowercase())
    }

    #[must_use]
    pub fn map_owner<'a>(&self, tok: &'a str) -> &'a str {
        match tok.split_once(':') {
            Some((lib, member)) if self.is_own(lib) => member,
            _ => tok,
        }
    }

    #[must_use]
    pub fn log_owner<'a>(&self, tok: &'a str) -> &'a str {
        let Some((lib, rest)) = tok.split_once('(') else {
            return tok;
        };
        let stem = lib.rsplit(['\\', '/']).next().unwrap_or(lib);
        let stem = stem
            .get(..stem.len().saturating_sub(4))
            .filter(|_| stem.to_ascii_lowercase().ends_with(".lib"));
        match (stem, rest.strip_suffix(')')) {
            (Some(s), Some(member)) if self.is_own(s) => member,
            _ => tok,
        }
    }

    #[must_use]
    pub fn object_key<'a>(&self, tok: &'a str) -> &'a str {
        let m = self.map_owner(tok);
        if m.len() != tok.len() {
            return m;
        }
        self.log_owner(tok)
    }
}

#[must_use]
pub fn is_member(owner: &str) -> bool {
    owner.contains(':')
}

#[cfg(test)]
mod tests {
    use super::*;

    pub const MAP: &str = " app\r\n\r\n Timestamp is 3a1b2c3d (Mon Nov 20 10:00:00 2000)\r\n\r\n Preferred load address is 00400000\r\n\r\n\
 Start         Length     Name                   Class\r\n 0001:00000000 00001000H .text                   CODE\r\n\r\n\
  Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n\
 0001:00000000       _main                      00401000 f   main.obj\r\n\
 0001:00000040       ?Run@App@@QAEXXZ           00401040 f i main.obj\r\n\
 0002:00000010       _g_count                   00402010     main.obj\r\n\
 0001:00000100       _printf                    00401100 f   LIBCMT:printf.obj\r\n\
 0000:00000000       ___safe_se_handler_count   00000000     <absolute>\r\n\r\n\
 entry point at        0001:00000080\r\n\r\n Static symbols\r\n\r\n\
 0001:00000020       _helper                    00401020 f   main.obj\r\n";

    #[test]
    fn publics_and_statics_are_read_with_their_objects() {
        let m = Map::parse(Path::new("app.map"), MAP).unwrap();
        assert_eq!(m.module, "app");
        assert_eq!(m.base, 0x40_0000);
        assert_eq!(m.entry, Some((1, 0x80)));
        assert_eq!(m.symbols.len(), 5);
        let run = &m.symbols[1];
        assert_eq!(run.rva, 0x1040);
        assert!(run.function && run.inline && !run.is_static);
        assert!(!m.symbols[2].function);
        assert_eq!(m.symbols[3].lib.as_deref(), Some("LIBCMT"));
        assert_eq!(m.symbols[3].object, "printf.obj");
        let h = &m.symbols[4];
        assert!(h.is_static && h.function);
        assert_eq!(h.name, "_helper");
        assert!(Map::parse(Path::new("x.map"), "x\n").is_err());
    }
}
