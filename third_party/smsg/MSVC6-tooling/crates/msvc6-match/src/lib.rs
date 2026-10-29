pub mod annotations;
pub mod bodies;
pub mod census;
pub mod closure;
pub mod code;
pub mod copies;
pub mod dataowner;
pub mod dataruns;
pub mod dataseat;
pub mod imports;
pub mod input;
pub mod locate;
pub mod mac;
pub mod mangle;
pub mod partition;
pub mod place;
pub mod placement;
pub mod seated;
pub mod sibling;
pub mod table;
pub mod vendored;
pub mod vftables;

use anyhow::{Context as _, Result, bail};
use msvc6_image::archive::Archive;
use msvc6_image::coff::Object;
use msvc6_image::pe::Image;
use serde::Serialize;

use crate::copies::{Group, Offered};
use crate::locate::{Lib, Recorded, Run, objects};
use crate::place::{Placement, Text, place};

#[derive(Debug)]
pub struct Report {
    pub text_rva: u32,
    pub text_len: usize,
    pub image_base: u32,
    pub groups: Vec<Group>,
    pub libraries: Vec<String>,
    pub run: Run,
    pub imports: Option<imports::Join>,
    pub census: Vec<census::Bucket>,
}

pub fn text_of(img: &Image) -> Result<Text<'_>> {
    let sec = img
        .section(code::SECTION)
        .with_context(|| format!("the image has no {} section", code::SECTION))?;
    Ok(Text {
        bytes: img.section_bytes(sec),
        rva: sec.rva.get(),
        image_base: img.opt.image_base.get(),
    })
}

pub fn parse_all(offered: &[Offered]) -> Result<Vec<Archive<'_>>> {
    offered
        .iter()
        .map(|o| Archive::parse(&o.bytes).with_context(|| o.path.display().to_string()))
        .collect()
}

pub fn run(img: &Image, offered: &[Offered], record: &[Recorded], jobs: usize) -> Result<Report> {
    if offered.is_empty() {
        bail!("no archive to place");
    }
    let t = text_of(img)?;
    let archives = parse_all(offered)?;
    let choice = copies::choose(&t, offered, &archives, record, jobs)?;
    let mut names: Vec<String> = Vec::new();
    let mut libs: Vec<Lib<'_>> = Vec::new();
    let mut all: Vec<(String, usize)> = Vec::new();
    for (g, &i) in choice.groups.iter().zip(&choice.picks) {
        names.push(g.library.clone());
        all.push((g.library.clone(), i));
    }
    for (name, i) in &all {
        let Some(a) = archives.get(*i) else { continue };
        if objects(a).next().is_some() {
            libs.push(Lib { name, archive: a });
        }
    }
    let run = locate::locate(&t, &libs, record, jobs)?;
    let pairs: Vec<(&str, &Archive<'_>)> = all
        .iter()
        .filter_map(|(n, i)| archives.get(*i).map(|a| (n.as_str(), a)))
        .collect();
    let imports = if pairs.iter().any(|(_, a)| imports::has_imports(a)) {
        Some(imports::join(&img.imports()?, &pairs))
    } else {
        None
    };
    let rich = msvc6_image::rich::decode(img.bytes());
    let none = std::collections::BTreeSet::new();
    let linked = imports.as_ref().map_or(&none, |j| &j.linked);
    let census = census::join(rich.as_ref(), &pairs, &run, linked)?;
    Ok(Report {
        text_rva: t.rva,
        text_len: t.bytes.len(),
        image_base: t.image_base,
        groups: choice.groups,
        libraries: names,
        run,
        imports,
        census,
    })
}

#[derive(Debug, Clone, Serialize)]
pub struct Verdict {
    pub archive: String,
    pub member: String,
    pub index: u16,
    pub symbol: Option<String>,
    pub len: usize,
    pub literal: usize,
    pub anchor: usize,
    pub verdict: String,
    pub hex: String,
}

pub fn explain(img: &Image, offered: &[Offered], needle: &str) -> Result<Vec<Verdict>> {
    let t = text_of(img)?;
    let archives = parse_all(offered)?;
    let needle = needle.to_ascii_lowercase();
    let mut out = Vec::new();
    for (o, a) in offered.iter().zip(&archives) {
        for (_, name, d) in objects(a) {
            if !name.to_ascii_lowercase().contains(&needle) {
                continue;
            }
            let obj = Object::parse(d).with_context(|| format!("{}: {name}", o.label))?;
            for c in code::sections(&obj).with_context(|| format!("{}: {name}", o.label))? {
                let anchor = c.anchor().map_or(0, |(_, a)| a.len());
                let verdict = match place(&c, t.bytes) {
                    Placement::Unique(off) => format!("UNIQUE at {:#x}", t.rva_of(off)),
                    Placement::Ambiguous(n) => format!("AMBIGUOUS x{n}"),
                    Placement::Absent => "ABSENT".to_string(),
                    Placement::Unidentifiable => {
                        format!("UNIDENTIFIABLE (anchor {anchor} < {})", place::MIN_ANCHOR)
                    }
                    Placement::Exhausted => "EXHAUSTED: the search budget ran out".to_string(),
                };
                out.push(Verdict {
                    archive: o.label.clone(),
                    member: name.to_string(),
                    index: c.index,
                    symbol: c.symbol.clone(),
                    len: c.bytes.len(),
                    literal: c.literal_len(),
                    anchor,
                    verdict,
                    hex: c.hex(),
                });
            }
        }
    }
    Ok(out)
}
