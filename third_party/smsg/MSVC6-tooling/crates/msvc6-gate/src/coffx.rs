use anyhow::Result;
use msvc6_image::coff::{CLASS_EXTERNAL, CLASS_STATIC, SCN_LNK_COMDAT, SELECT_ANY};
use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Bss {
    pub index: u16,
    pub size: u32,
    pub align: u32,
    pub symbols: Vec<(u32, String)>,
}

pub fn bss(d: &[u8]) -> Result<Option<Bss>> {
    let o = msvc6_diff::comdat::parse(d)?;
    let mut out = None;
    for sec in o.sections.iter().filter(|s| s.name == ".bss") {
        let size = sec.size.max(sec.virt_size);
        if size == 0 {
            continue;
        }
        let mut symbols: Vec<(u32, String)> = o
            .symbols
            .iter()
            .filter(|s| s.section_number() == Some(sec.number))
            .filter(|s| s.class == CLASS_EXTERNAL || s.class == CLASS_STATIC)
            .filter(|s| !s.name.is_empty() && !s.name.starts_with('.'))
            .map(|s| (s.value, s.name.clone()))
            .collect();
        symbols.sort();
        out = Some(Bss {
            index: sec.number,
            size,
            align: sec.align(),
            symbols,
        });
    }
    Ok(out)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Header {
    pub index: u16,
    pub name: String,
    pub size: u32,
    pub characteristics: u32,
    pub comdat: bool,
}

pub fn headers(d: &[u8]) -> Result<Vec<Header>> {
    let o = msvc6_diff::comdat::parse(d)?;
    Ok(o.sections
        .iter()
        .map(|s| Header {
            index: s.number,
            name: s.name.clone(),
            size: if s.is_uninit() {
                s.size.max(s.virt_size)
            } else {
                s.size
            },
            characteristics: s.characteristics,
            comdat: s.characteristics & SCN_LNK_COMDAT != 0,
        })
        .collect())
}

pub fn selections(d: &[u8]) -> Result<BTreeMap<String, bool>> {
    let o = msvc6_diff::comdat::parse(d)?;
    let mut out = BTreeMap::new();
    for c in o.comdats() {
        if let Some(n) = c.symbol.filter(|n| !n.is_empty()) {
            out.insert(n, c.selection == SELECT_ANY);
        }
    }
    Ok(out)
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Sizes {
    pub text: usize,
    pub textx: usize,
    pub xdata: usize,
    pub data: usize,
    pub data_sections: Vec<(String, usize)>,
}

pub fn sizes(d: &[u8]) -> Result<Sizes> {
    let o = msvc6_diff::comdat::parse(d)?;
    let mut out = Sizes::default();
    for s in &o.sections {
        let raw = s.size as usize;
        match s.name.as_str() {
            ".text" => out.text = out.text.saturating_add(raw),
            ".text$x" => out.textx = out.textx.saturating_add(raw),
            ".xdata$x" => out.xdata = out.xdata.saturating_add(raw),
            n if n.starts_with(".debug") || n == ".drectve" => {}
            n => {
                out.data = out.data.saturating_add(raw);
                out.data_sections.push((n.to_string(), raw));
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use msvc6_fixture::coff::{Object, SCN_BSS, SCN_COMDAT, SCN_TEXT, SELECT_ANY, Section};

    #[test]
    fn bss_symbols_selections_and_sizes_are_read_from_one_object() {
        let mut o = Object::new();
        let text = o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: vec![0xc3],
            ..Section::default()
        });
        let b = o.section(Section {
            name: ".bss".into(),
            characteristics: SCN_BSS,
            uninit: 12,
            ..Section::default()
        });
        o.section_symbol(text, SELECT_ANY, 0);
        o.external("_f", 0, text);
        o.external("_g", 8, b);
        o.external("_h", 0, b);
        let d = o.build();
        let got = bss(&d).unwrap().unwrap();
        assert_eq!(got.size, 12);
        assert_eq!(got.symbols, [(0, "_h".to_string()), (8, "_g".to_string())]);
        assert_eq!(selections(&d).unwrap().get("_f"), Some(&true));
        let s = sizes(&d).unwrap();
        assert_eq!(s.text, 1);
        assert!(
            headers(&d)
                .unwrap()
                .iter()
                .any(|h| h.name == ".bss" && h.size == 12)
        );
    }
}
