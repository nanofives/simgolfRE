use super::glob::collapse;
use anyhow::Result;
use msvc6_image::coff::{self, Object, SectionHeader, Symbol};
use msvc6_image::pe::image::Image;
use msvc6_image::rich;

const SCN_CNT_INITIALIZED_DATA: u32 = 0x40;
const SCN_LNK_INFO: u32 = 0x200;
const SCN_MEM_DISCARDABLE: u32 = 0x0200_0000;
const SCN_MEM_EXECUTE: u32 = 0x2000_0000;
const SCN_MEM_READ: u32 = 0x4000_0000;
const SCN_MEM_WRITE: u32 = 0x8000_0000;

fn kind(s: &SectionHeader) -> &'static str {
    let c = s.characteristics;
    if c & coff::SCN_CNT_CODE != 0 {
        "code"
    } else if c & coff::SCN_CNT_UNINITIALIZED_DATA != 0 {
        "bss"
    } else if c & SCN_LNK_INFO != 0 {
        "info"
    } else if c & SCN_CNT_INITIALIZED_DATA != 0 {
        "data"
    } else {
        "other"
    }
}

fn access(c: u32) -> String {
    let bit = |b: u32, ch: char| if c & b != 0 { ch } else { '-' };
    let mut s: String = [
        bit(SCN_MEM_READ, 'r'),
        bit(SCN_MEM_WRITE, 'w'),
        bit(SCN_MEM_EXECUTE, 'x'),
    ]
    .iter()
    .collect();
    if c & SCN_MEM_DISCARDABLE != 0 {
        s.push_str(" discard");
    }
    s
}

fn class_word(s: &Symbol) -> String {
    match s.class {
        coff::CLASS_EXTERNAL => "external".to_string(),
        coff::CLASS_STATIC => "static".to_string(),
        coff::CLASS_LABEL => "label".to_string(),
        coff::CLASS_FUNCTION => "function-record".to_string(),
        coff::CLASS_FILE => "file".to_string(),
        coff::CLASS_SECTION => "section".to_string(),
        coff::CLASS_WEAK_EXTERNAL => "weak".to_string(),
        n => format!("class{n}"),
    }
}

fn shown(s: &Symbol) -> bool {
    !(s.name.is_empty()
        || s.name == coff::COMP_ID
        || s.class == coff::CLASS_FILE
        || s.class == coff::CLASS_FUNCTION
        || s.class == coff::CLASS_LABEL
        || s.section_definition().is_some())
}

fn place(s: &Symbol) -> String {
    match s.section {
        0 if s.value > 0 => format!("common {}", s.value),
        0 => "undef".to_string(),
        -1 => "abs".to_string(),
        -2 => "debug".to_string(),
        n => format!("s{n}+{:#x}", s.value),
    }
}

pub fn obj(bytes: &[u8]) -> Result<Vec<String>> {
    let o = Object::parse(bytes)?;
    let mut out = Vec::new();
    if let Some(id) = o.comp_id() {
        out.push(format!(
            "comp.id {} {}",
            rich::prodid_name(id >> 16),
            id & 0xffff
        ));
    }
    let comdats = o.comdats();
    for s in &o.sections {
        let mut line = format!(
            "section {} {} {} {} size={:#x} align={}",
            s.number,
            s.name,
            kind(s),
            access(s.characteristics),
            s.size,
            s.align()
        );
        if let Some(c) = comdats.iter().find(|c| c.section == s.number) {
            line.push_str(&format!(" comdat={}", coff::selection_name(c.selection)));
            if let Some(a) = c.associated {
                line.push_str(&format!(" {a}"));
            }
            if let Some(sym) = &c.symbol {
                line.push_str(&format!(" {sym}"));
            }
        }
        out.push(line);
        if s.name == ".drectve" {
            let text = msvc6_base::srcio::decode(o.section_data(s));
            for d in text.split_whitespace() {
                out.push(format!("drectve {}", d.trim_end_matches('\0')));
            }
        }
    }
    for s in o.symbols.iter().filter(|s| shown(s)) {
        let mut line = format!("symbol {} {} {}", s.name, place(s), class_word(s));
        if s.is_function() {
            line.push_str(" function");
        }
        out.push(line);
    }
    Ok(out)
}

#[must_use]
pub fn map(text: &str) -> Vec<String> {
    let mut on = false;
    let mut out = Vec::new();
    for l in text.lines() {
        if !on {
            on = l.contains("Publics by Value");
            continue;
        }
        let t = collapse(l);
        if !t.is_empty() {
            out.push(t);
        }
    }
    out
}

#[must_use]
pub fn rich_lines(image: &[u8]) -> Vec<String> {
    rich::decode(image).map_or_else(
        || vec!["no Rich header".to_string()],
        |r| {
            r.stamps
                .iter()
                .map(|s| format!("{} {} {}", rich::prodid_name(s.prodid), s.build, s.objects))
                .collect()
        },
    )
}

pub fn imports(image: &[u8]) -> Result<Vec<String>> {
    let img = Image::parse(image.to_vec())?;
    let mut out = Vec::new();
    for d in img.imports()? {
        for e in &d.entries {
            out.push(match e.ordinal {
                Some(n) => format!("{} #{n}", d.dll),
                None => format!("{} {}", d.dll, e.name),
            });
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_map_view_starts_at_the_publics() {
        let m = " p\n\n  Address         Publics by Value              Rva+Base     Lib:Object\n\n 0001:00000000       _main                      00401000 f   p.obj\n\n entry point at        0001:00000000\n\n Static symbols\n\n";
        assert_eq!(
            map(m),
            vec![
                "0001:00000000 _main 00401000 f p.obj",
                "entry point at 0001:00000000",
                "Static symbols"
            ]
        );
    }

    #[test]
    fn an_object_lists_its_compiler_sections_and_symbols() {
        let mut o = msvc6_fixture::coff::Object::new();
        o.comp_id(0x0b, 8168);
        let t = o.section(msvc6_fixture::coff::Section {
            name: ".text".into(),
            characteristics: 0x6050_1020,
            data: vec![0xc3],
            ..Default::default()
        });
        o.section_symbol(t, 1, 0);
        o.external("?f@@YAXXZ", 0, t);
        o.undefined("_g");
        assert_eq!(
            obj(&o.build()).unwrap(),
            vec![
                "comp.id Utc12_CPP 8168",
                "section 1 .text code r-x size=0x1 align=16 comdat=nodup ?f@@YAXXZ",
                "symbol ?f@@YAXXZ s1+0x0 external function",
                "symbol _g undef external function",
            ]
        );
    }

    #[test]
    fn an_image_without_a_rich_header_says_so() {
        assert_eq!(rich_lines(b"MZ"), vec!["no Rich header".to_string()]);
    }
}
