use std::collections::{BTreeMap, BTreeSet};

use crate::objkey::Keys;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Row {
    pub sect: u16,
    pub name: String,
    pub va: u32,
    pub obj: String,
    pub is_static: bool,
    pub function: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Contrib {
    pub sect: u16,
    pub off: u32,
    pub len: u32,
    pub name: String,
    pub class: String,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Map {
    pub load: u32,
    pub contribs: Vec<Contrib>,
    pub rows: Vec<Row>,
}

fn hex_u32(s: &str) -> Option<u32> {
    u32::from_str_radix(s, 16).ok()
}

fn sect_off(tok: &str) -> Option<(u16, u32)> {
    let (s, o) = tok.split_once(':')?;
    Some((u16::from_str_radix(s, 16).ok()?, hex_u32(o)?))
}

#[must_use]
pub fn parse(text: &str) -> Map {
    let mut m = Map::default();
    let mut part = 0u8;
    for line in text.lines() {
        let line = line.trim_end();
        if line.contains("Publics by Value") {
            part = 1;
            continue;
        }
        if line.trim() == "Static symbols" {
            part = 2;
            continue;
        }
        if let Some(rest) = line.trim().strip_prefix("Preferred load address is ") {
            if let Some(v) = hex_u32(rest.trim()) {
                m.load = v;
            }
            continue;
        }
        let t: Vec<&str> = line.split_whitespace().collect();
        let Some((sect, off)) = t.first().and_then(|f| sect_off(f)) else {
            continue;
        };
        if part == 0 {
            if let (Some(len), Some(name), Some(class)) = (
                t.get(1).and_then(|l| l.strip_suffix('H')).and_then(hex_u32),
                t.get(2),
                t.get(3),
            ) {
                m.contribs.push(Contrib {
                    sect,
                    off,
                    len,
                    name: (*name).to_string(),
                    class: (*class).to_string(),
                });
            }
            continue;
        }
        let (Some(name), Some(va), Some(obj)) =
            (t.get(1), t.get(2).and_then(|v| hex_u32(v)), t.last())
        else {
            continue;
        };
        if t.len() < 4 {
            continue;
        }
        m.rows.push(Row {
            sect,
            name: (*name).to_string(),
            va,
            obj: (*obj).to_string(),
            is_static: part == 2,
            function: t
                .get(3..t.len().saturating_sub(1))
                .is_some_and(|flags| flags.contains(&"f")),
        });
    }
    m
}

impl Map {
    #[must_use]
    pub fn code_sections(&self) -> BTreeSet<u16> {
        let s: BTreeSet<u16> = self
            .contribs
            .iter()
            .filter(|c| c.class == "CODE")
            .map(|c| c.sect)
            .collect();
        if s.is_empty() { [1].into() } else { s }
    }

    #[must_use]
    pub fn symbols(&self, base: u32) -> BTreeMap<String, u32> {
        let code = self.code_sections();
        let mut out = BTreeMap::new();
        for r in self.rows.iter().filter(|r| code.contains(&r.sect)) {
            if let Some(rva) = r.va.checked_sub(base) {
                out.insert(r.name.clone(), rva);
            }
        }
        out
    }

    #[must_use]
    pub fn objects(&self, keys: &Keys) -> BTreeMap<String, String> {
        let code = self.code_sections();
        let mut out = BTreeMap::new();
        for r in self.rows.iter().filter(|r| code.contains(&r.sect)) {
            if r.obj.to_ascii_lowercase().ends_with(".obj") {
                out.insert(r.name.clone(), keys.map_owner(&r.obj).to_string());
            }
        }
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_map_reads_contributions_publics_and_statics() {
        let text = " app\r\n\r\n Preferred load address is 00400000\r\n\r\n Start         Length     Name \
                    Class\r\n 0001:00000000 000b62ddH .text                   CODE\r\n 0001:000b62e0 00002143H \
                    .text$x                 CODE\r\n 0002:00000268 00003064H .rdata                  DATA\r\n\r\n  \
                    Address         Publics by Value              Rva+Base     Lib:Object\r\n\r\n 0000:00000000       \
                    ___safe_se_handler_count   00000000     <absolute>\r\n 0001:00000000       ?Clear@@YAXH@Z         \
                    00401000 f   alpha.obj\r\n 0001:000038a0       ?Draw@Shape@@UAEHPAU1@PADH@Z \
                    004048a0 f i beta.obj\r\n 0003:0038116c       ___onexitend               0084216c     \
                    <common>\r\n\r\n entry point at        0001:000a585f\r\n\r\n Static symbols\r\n\r\n \
                    0001:00072a60       _$E16                      00473a60 f   gamma.obj\r\n 0001:000ad560       \
                    _flush_pending       004ae560 f   vendor:DEFLATE.OBJ\r\n\r\n 2639 bytes saved through ICF\r\n";
        let m = parse(text);
        assert_eq!(m.load, 0x40_0000);
        assert_eq!(m.contribs.len(), 3);
        assert_eq!(
            m.contribs[1],
            Contrib {
                sect: 1,
                off: 0xb62e0,
                len: 0x2143,
                name: ".text$x".into(),
                class: "CODE".into()
            }
        );
        assert_eq!(m.rows.len(), 6);
        assert_eq!(m.rows[0].sect, 0);
        assert_eq!(m.rows[3].obj, "<common>");
        assert!(m.rows[5].is_static);
        let syms = m.symbols(0x40_0000);
        assert_eq!(syms.get("?Clear@@YAXH@Z"), Some(&0x1000));
        assert!(!syms.contains_key("___onexitend"));
        let objs = m.objects(&Keys::new(["vendor"]));
        assert_eq!(
            objs.get("_flush_pending").map(String::as_str),
            Some("DEFLATE.OBJ")
        );
        assert_eq!(objs.get("_$E16").map(String::as_str), Some("gamma.obj"));
    }
}
