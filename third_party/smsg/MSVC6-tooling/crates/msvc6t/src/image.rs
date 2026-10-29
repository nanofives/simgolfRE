use anyhow::{Context as _, Result, bail};
use msvc6_base::rowjson;
use msvc6_base::srcio;
use msvc6_base::units::{Rva, Va};
use msvc6_image::anatomy;
use msvc6_image::archive::Archive;
use msvc6_image::clock::{self, Field};
use msvc6_image::cmp;
use msvc6_image::coff::Object;
use msvc6_image::dump::{self, Where};
use msvc6_image::pe::dirs::debug_kind;
use msvc6_image::pe::{CodeView, DIR_NAMES, DebugEntry, Dir, Image, OptionalHeader};
use msvc6_image::postlink::{self, Row, State};
use msvc6_image::res;
use msvc6_image::rich::{self, Rich};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

use crate::Outcome;
use crate::cli::{
    ImageCmpArgs, ImageDumpArgs, ImageExportsArgs, ImageImportsArgs, ImageInfoArgs,
    ImagePostlinkArgs, ImageResArgs, ImageRichArgs, JsonArg,
};
use crate::project::{self, Config, Resolved};

pub fn json<S: Serialize, T: Serialize>(
    j: &JsonArg,
    command: &str,
    image: &str,
    summary: Option<&S>,
    rows: &[T],
) -> Result<()> {
    if let Some(p) = &j.json {
        rowjson::write_with(p, command, image, summary, rows)?;
    }
    Ok(())
}

fn open(config: Option<&Config>, arg: &str) -> Result<(Resolved, Image)> {
    let mut r = project::resolve(config, arg)?;
    let img =
        Image::parse(std::mem::take(&mut r.bytes)).with_context(|| r.path.display().to_string())?;
    Ok((r, img))
}

fn machine_name(m: u16) -> &'static str {
    match m {
        0x14c => "i386",
        0x8664 => "x64",
        0x1c0 => "ARM",
        0x1c2 => "Thumb",
        0x200 => "IA64",
        0x166 => "R4000",
        0x184 => "Alpha",
        0x1f0 => "PowerPC",
        _ => "?",
    }
}

fn section_name(img: &Image, rva: Rva) -> String {
    img.section_of(rva)
        .map_or_else(|| "-".to_string(), |s| s.name.clone())
}

#[derive(Debug, Serialize)]
struct DirRow {
    index: usize,
    name: &'static str,
    rva: u32,
    size: u32,
    section: String,
}

#[derive(Debug, Serialize)]
struct RichSummary {
    at: usize,
    key: u32,
    checksum: u32,
    key_is_checksum: bool,
    stamps: usize,
}

#[derive(Debug, Serialize)]
struct InfoSummary<'a> {
    path: String,
    size: usize,
    e_lfanew: usize,
    machine: u16,
    timestamp: u32,
    characteristics: u16,
    dll: bool,
    optional: &'a OptionalHeader,
    dirs: Vec<DirRow>,
    rich: Option<RichSummary>,
    debug: Vec<DebugEntry>,
    codeview: Option<CodeView>,
    header_structs_end: usize,
    overlay: usize,
}

pub fn info(config: Option<&Config>, a: &ImageInfoArgs) -> Result<Outcome> {
    let (r, img) = open(config, &a.image)?;
    let o = &img.opt;
    let dirs: Vec<DirRow> = img
        .dirs
        .iter()
        .enumerate()
        .filter(|(_, d)| d.is_present())
        .map(|(i, d)| DirRow {
            index: i,
            name: DIR_NAMES.get(i).copied().unwrap_or("?"),
            rva: d.rva.get(),
            size: d.size,
            section: section_name(&img, d.rva),
        })
        .collect();
    let rich = rich::decode(img.bytes());
    let cv = img.codeview()?;
    let rows = dump::sections(&img);
    let summary = InfoSummary {
        path: r.path.display().to_string(),
        size: img.len(),
        e_lfanew: img.e_lfanew,
        machine: img.machine,
        timestamp: img.timestamp,
        characteristics: img.characteristics,
        dll: img.is_dll(),
        optional: o,
        dirs,
        rich: rich.as_ref().map(|h| RichSummary {
            at: h.dans,
            key: h.key,
            checksum: h.checksum,
            key_is_checksum: h.key_is_checksum(),
            stamps: h.stamps.len(),
        }),
        debug: img.debug_entries(),
        codeview: cv,
        header_structs_end: img.header_structs_end(),
        overlay: img.overlay(),
    };
    json(&a.json, "image info", &r.label, Some(&summary), &rows)?;
    println!("{}: {} ({} bytes)", r.label, r.path.display(), img.len());
    println!(
        "  PE32 {} {}, {} section(s), COFF timestamp {:#010x}, characteristics {:#06x}, LINK {}.{:02}",
        machine_name(img.machine),
        if img.is_dll() { "DLL" } else { "EXE" },
        img.sections.len(),
        img.timestamp,
        img.characteristics,
        o.linker_major,
        o.linker_minor
    );
    println!(
        "  image base {:#x}, entry {:#x}, SizeOfImage {:#x}, SizeOfHeaders {:#x}, alignment {:#x} / {:#x}",
        o.image_base.get(),
        o.entry_point.get(),
        o.size_of_image,
        o.size_of_headers,
        o.section_alignment,
        o.file_alignment
    );
    println!(
        "  subsystem {} {}.{:02}, OS {}.{:02}, image version {}.{}, checksum {:#x}, DLL characteristics {:#06x}",
        o.subsystem,
        o.subsystem_version.0,
        o.subsystem_version.1,
        o.os_version.0,
        o.os_version.1,
        o.image_version.0,
        o.image_version.1,
        o.checksum,
        o.dll_characteristics
    );
    println!(
        "  SizeOfCode {:#x}, initialized data {:#x}, uninitialized {:#x}, stack {:#x}/{:#x}, heap {:#x}/{:#x}",
        o.size_of_code,
        o.size_of_init_data,
        o.size_of_uninit_data,
        o.stack_reserve,
        o.stack_commit,
        o.heap_reserve,
        o.heap_commit
    );
    println!(
        "  e_lfanew {:#x}; header structures end at {:#x}, {:#x} byte(s) of header slack before the first section",
        img.e_lfanew,
        img.header_structs_end(),
        img.first_raw().saturating_sub(img.header_structs_end())
    );
    match &summary.rich {
        None => println!("  Rich header: none"),
        Some(h) => println!(
            "  Rich header at {:#x}: {} stamp(s), key {:#010x} {}",
            h.at,
            h.stamps,
            h.key,
            if h.key_is_checksum {
                "= the checksum".to_string()
            } else {
                format!(
                    "!= the checksum {:#010x}: edited after the link",
                    h.checksum
                )
            }
        ),
    }
    println!("  data directories:");
    for d in &summary.dirs {
        println!(
            "    {:>2} {:<12} {:#010x} {:>8}  {}",
            d.index, d.name, d.rva, d.size, d.section
        );
    }
    println!("  sections:");
    print_sections(&rows);
    for e in &summary.debug {
        println!(
            "  debug entry {} ({}) timestamp {:#010x}, {} byte(s) at rva {:#x}, file {:#x}",
            e.kind,
            debug_kind(e.kind),
            e.timestamp,
            e.size,
            e.data_rva.get(),
            e.data_ptr
        );
    }
    if let Some(c) = &summary.codeview {
        match &c.guid {
            Some(g) => println!("  CodeView {} guid {g} age {} {}", c.format, c.age, c.path),
            None if c.format.starts_with("NB") && c.format != "NB10" => println!(
                "  CodeView {}: a record of {} byte(s) in the file at {:#x} (msvc6t cv check reads it)",
                c.format, c.len, c.file_offset
            ),
            None => println!(
                "  CodeView {} signature {:#010x} age {} {}",
                c.format, c.signature, c.age, c.path
            ),
        }
    }
    if summary.overlay > 0 {
        println!(
            "  overlay: {} byte(s) past the last section",
            summary.overlay
        );
    }
    Ok(Outcome::Ok)
}

fn print_sections(rows: &[dump::SectionRow]) {
    println!(
        "    {:<10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}  tail with no file bytes",
        "name", "rva", "mapped end", "virt size", "raw size", "raw off", "flags"
    );
    for s in rows {
        println!(
            "    {:<10} {:>#10x} {:>#10x} {:>#10x} {:>#10x} {:>#10x} {:>#10x}  {}",
            s.name,
            s.rva,
            s.mapped_end,
            s.virt_size,
            s.raw_size,
            s.raw_off,
            s.flags,
            s.tail_from.map_or_else(
                || "none".to_string(),
                |f| format!("{:#x} byte(s) from {f:#x}", s.tail)
            )
        );
    }
}

fn stamp_line(s: &rich::Stamp) -> String {
    format!(
        "    prodid {:#04x} {:<14} build {:<6} objects {}",
        s.prodid,
        rich::prodid_name(s.prodid),
        s.build,
        s.objects
    )
}

fn families(h: &Rich) -> (u32, u32, u32) {
    h.families()
}

fn verdict(have: u32, want: u32) -> String {
    match have.cmp(&want) {
        std::cmp::Ordering::Equal => "MATCH".to_string(),
        std::cmp::Ordering::Less => format!("{} SHORT", want.saturating_sub(have)),
        std::cmp::Ordering::Greater => format!("{} EXCESS", have.saturating_sub(want)),
    }
}

fn class_list(v: &[u32]) -> String {
    v.iter()
        .map(|p| format!("{p:#04x} {}", rich::prodid_name(*p)))
        .collect::<Vec<_>>()
        .join(", ")
}

fn print_candidate(reference: &Rich, path: &Path, cand: Option<&Rich>) {
    println!();
    let Some(c) = cand else {
        println!(
            "  candidate {}: NO Rich header (or it failed the phase check), so its census is UNKNOWN, not zero",
            path.display()
        );
        return;
    };
    println!(
        "  candidate {}: key {:#010x}, {} stamp(s)",
        path.display(),
        c.key,
        c.stamps.len()
    );
    for s in c.sorted() {
        println!("{}", stamp_line(&s));
    }
    let (rc, rp, rm) = families(reference);
    let (cc, cp, cm) = families(c);
    println!(
        "    census   C {cc} vs {rc} ({});  C++ {cp} vs {rp} ({});  MASM {cm} vs {rm} ({})",
        verdict(cc, rc),
        verdict(cp, rp),
        verdict(cm, rm)
    );
    let (rset, cset) = (reference.classes(), c.classes());
    let missing: Vec<u32> = rset.difference(&cset).copied().collect();
    let extra: Vec<u32> = cset.difference(&rset).copied().collect();
    if missing.is_empty() && extra.is_empty() {
        println!("    stamp classes: the same {} in both", rset.len());
    } else {
        println!(
            "    STAMP CLASSES DIFFER: {} in the reference, {} here. Each stamp is 8 header bytes, so the \
             candidate cannot be md5-equal while they differ.",
            rset.len(),
            cset.len()
        );
        if !missing.is_empty() {
            println!("      absent here: {}", class_list(&missing));
        }
        if !extra.is_empty() {
            println!("      absent in the reference: {}", class_list(&extra));
        }
    }
}

fn print_archives(h: &Rich, archives: &[PathBuf]) -> Result<()> {
    let mut per = Vec::new();
    for p in archives {
        let d = project::read(p)?;
        let a = Archive::parse(&d).with_context(|| p.display().to_string())?;
        per.push((p.display().to_string(), rich::archive_builds(&a)));
    }
    let w = rich::witnesses(h, &per);
    println!(
        "\n  which archive's members are in this image, by the builds only one archive stamps:"
    );
    for (name, _) in &per {
        let hits: Vec<String> = w
            .iter()
            .filter(|x| &x.archive == name)
            .map(|x| {
                format!(
                    "build {} x{} {}",
                    x.build,
                    x.members,
                    match (x.unique, x.in_image) {
                        (false, _) => "shared, decides nothing",
                        (true, true) => "IN IMAGE",
                        (true, false) => "absent",
                    }
                )
            })
            .collect();
        println!("    {name}: {}", hits.join("; "));
    }
    if per.len() < 2 {
        println!(
            "  VERDICT: none; one archive has nothing to be told apart from, so give two or more copies"
        );
        return Ok(());
    }
    match rich::deciding_archives(&w).as_slice() {
        [] => println!(
            "  VERDICT: no archive stamps a build that only it has and this image carries; the copies are not \
             told apart here"
        ),
        [one] => println!("  VERDICT: {one}, decided by a build only that copy stamps"),
        many => println!(
            "  VERDICT: AMBIGUOUS, {} copies stamp a deciding build present here. A build the image's own \
             compiler also stamps is carried by its own objects and by any library built with that compiler, \
             so it decides nothing; read the copy from the build no compiler of the image has.",
            many.len()
        ),
    }
    Ok(())
}

fn print_rsp(rsp: &Path, cand: Option<&Rich>, bucket: Option<&str>) -> Result<()> {
    let text = srcio::read(rsp).with_context(|| format!("reading {}", rsp.display()))?;
    let dir = rsp.parent().unwrap_or_else(|| Path::new("."));
    let mut by: BTreeMap<Option<u32>, Vec<String>> = BTreeMap::new();
    let mut unreadable = 0usize;
    for line in text.lines() {
        for tok in line.split_whitespace() {
            let t = tok.trim_matches('"');
            if t.starts_with('/') || !t.to_ascii_lowercase().ends_with(".obj") {
                continue;
            }
            match std::fs::read(dir.join(t)) {
                Ok(b) => by
                    .entry(
                        Object::parse(&b)
                            .ok()
                            .and_then(|o| o.comp_id())
                            .map(|c| c >> 16),
                    )
                    .or_default()
                    .push(t.to_string()),
                Err(_) => unreadable = unreadable.saturating_add(1),
            }
        }
    }
    let total: usize = by.values().map(Vec::len).sum();
    println!(
        "\n  the candidate's census by object: {total} object(s) read from {}, {unreadable} unreadable",
        rsp.display()
    );
    let want: BTreeMap<u32, u32> = cand
        .map(|h| h.classes().into_iter().map(|p| (p, h.objects(p))).collect())
        .unwrap_or_default();
    for (id, objs) in &by {
        let label = id.map_or_else(
            || "no @comp.id".to_string(),
            |p| rich::prodid_name(p).to_string(),
        );
        let ctrl = match id.and_then(|p| want.get(&p)) {
            Some(w) if usize::try_from(*w).ok() == Some(objs.len()) => {
                "  == the header".to_string()
            }
            Some(w) => format!("  != the header, which says {w}"),
            None => "  (no such stamp in the header)".to_string(),
        };
        println!("    {:5}  {label:<14}{ctrl}", objs.len());
        let show = if bucket.is_some_and(|b| b.eq_ignore_ascii_case(&label)) {
            objs.len()
        } else {
            4
        };
        for o in objs.iter().take(show) {
            println!("             {o}");
        }
    }
    Ok(())
}

pub fn rich_verb(config: Option<&Config>, a: &ImageRichArgs) -> Result<Outcome> {
    let (r, img) = open(config, &a.image)?;
    let reference = rich::decode(img.bytes());
    let cand_img = a
        .candidate
        .as_deref()
        .map(|p| Image::parse(project::read(p)?).with_context(|| p.display().to_string()))
        .transpose()?;
    let cand = cand_img.as_ref().map(|c| rich::decode(c.bytes()));
    let rows = rich::census_rows(reference.as_ref(), cand.as_ref().map(Option::as_ref));
    json::<(), _>(&a.json, "image rich", &r.label, None, &rows)?;
    let Some(h) = reference else {
        println!("{}: no Rich header (or it failed the phase check)", r.label);
        return Ok(Outcome::Ok);
    };
    println!(
        "{}: Rich header at {:#x}..{:#x}, xor key {:#010x} ({}), {} stamp(s)",
        r.label,
        h.dans,
        h.end,
        h.key,
        if h.key_is_checksum() {
            "the checksum".to_string()
        } else {
            format!(
                "NOT the checksum {:#010x}: edited after the link",
                h.checksum
            )
        },
        h.stamps.len()
    );
    for s in h.sorted() {
        println!("{}", stamp_line(&s));
    }
    let (c, cpp, masm) = families(&h);
    println!(
        "\n  compiled objects in the link: {c} C, {cpp} C++, {masm} MASM = {}",
        c.saturating_add(cpp).saturating_add(masm)
    );
    if let (Some(p), Some(cand)) = (&a.candidate, &cand) {
        print_candidate(&h, p, cand.as_ref());
    }
    if !a.archive.is_empty() {
        print_archives(&h, &a.archive)?;
    }
    if let Some(rsp) = &a.rsp {
        print_rsp(
            rsp,
            cand.as_ref().and_then(Option::as_ref),
            a.bucket.as_deref(),
        )?;
    }
    Ok(Outcome::Ok)
}

fn number(s: &str) -> Result<u32> {
    let t = s.trim();
    match t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) {
        Some(h) => u32::from_str_radix(h, 16),
        None => t.parse(),
    }
    .with_context(|| format!("`{s}` is not a number"))
}

#[derive(Debug, Serialize)]
struct DumpSummary {
    rva: u32,
    location: Where,
    bytes: String,
}

pub fn dump_verb(config: Option<&Config>, a: &ImageDumpArgs) -> Result<Outcome> {
    let (r, img) = open(config, &a.image)?;
    let base = img.opt.image_base;
    if a.textbss {
        return textbss(&r, &img, a);
    }
    let Some(at) = &a.at else {
        let rows = dump::sections(&img);
        json::<(), _>(&a.json, "image dump", &r.label, None, &rows)?;
        println!(
            "{}: image base {:#x}, {} section(s). `mapped` is max(virtual, raw), what an address lookup \
             uses; only min(virtual, raw) is in the file, and an address past it reads as zero at run time.",
            r.label,
            base.get(),
            img.sections.len()
        );
        print_sections(&rows);
        return Ok(Outcome::Ok);
    };
    let n = number(at)?;
    let rva = if a.va {
        Va(n)
            .to_rva(base)
            .with_context(|| format!("VA {n:#x} is below the image base {:#x}", base.get()))?
    } else {
        Rva(n)
    };
    let va = u64::from(rva.get()).saturating_add(u64::from(base.get()));
    let loc = dump::locate(&img, rva);
    let bytes = img.at_rva(rva, a.len);
    let summary = DumpSummary {
        rva: rva.get(),
        location: loc.clone(),
        bytes: bytes
            .map(|b| b.iter().map(|x| format!("{x:02x}")).collect())
            .unwrap_or_default(),
    };
    json::<_, ()>(&a.json, "image dump", &r.label, Some(&summary), &[])?;
    match &loc {
        Where::Outside => {
            println!(
                "{}: {:#x} (VA {va:#x}) is in no section and past the headers: not an address in this image. \
                 Is it a VA? Subtract the image base {:#x}.",
                r.label,
                rva.get(),
                base.get()
            );
            return Ok(Outcome::Ok);
        }
        Where::Headers { file_off } => println!(
            "{}: {:#x} (VA {va:#x}) is in the headers, file offset {file_off:#x}",
            r.label,
            rva.get()
        ),
        Where::Section {
            name,
            offset,
            file_off,
            disk_end,
        } => match file_off {
            Some(f) => println!(
                "{}: {:#x} (VA {va:#x}) is in `{name}` at +{offset:#x}; file bytes at offset {f:#x}",
                r.label,
                rva.get()
            ),
            None => println!(
                "{}: {:#x} (VA {va:#x}) is in `{name}` at +{offset:#x}, in the virtual-only tail: the \
                 section's file bytes end at rva {disk_end:#x}, and these read as zero at run time",
                r.label,
                rva.get()
            ),
        },
    }
    let Some(b) = bytes else {
        println!(
            "  no {} byte(s) to dump at {:#x}: they run past the file bytes there",
            a.len,
            rva.get()
        );
        return Ok(Outcome::Ok);
    };
    if let Some((d, sec)) = dump::dword_target(&img, b) {
        println!(
            "  dword at {:#x} = {d:#010x}{}",
            rva.get(),
            sec.map_or_else(String::new, |(rv, s)| format!(
                "; as a VA, rva {rv:#x} in `{s}`"
            ))
        );
    }
    for l in dump::hex_lines(rva.get(), b) {
        println!("{l}");
    }
    Ok(Outcome::Ok)
}

fn textbss(r: &Resolved, img: &Image, a: &ImageDumpArgs) -> Result<Outcome> {
    let t = dump::textbss(img, &a.section)
        .with_context(|| format!("{}: no `{}` section", r.label, a.section))?;
    let map = match &a.map {
        Some(p) => {
            let text = srcio::read(p).with_context(|| format!("reading {}", p.display()))?;
            let number = img
                .section(&a.section)
                .map_or(1, |s| s.index.saturating_add(1));
            dump::map_textbss(&text, &format!("{number:04}"))
        }
        None => None,
    };
    json::<_, ()>(&a.json, "image dump", &r.label, Some(&(&t, &map)), &[])?;
    println!(
        "{}: {} rva {:#x}..{:#x}; its final zero run starts at rva {:#x}: {} ({:#x}) byte(s). {}",
        r.label,
        a.section,
        t.text_rva,
        t.text_end,
        t.run_from,
        t.run,
        t.run,
        if t.run == 0 {
            "No zero tail, so no incremental .textbss contribution ends the section."
        } else {
            "An upper bound on .textbss: a body ending in 00, or zero padding, lengthens it."
        }
    );
    if let Some(p) = &a.map {
        match map {
            None => println!(
                "  {}: no __enc$textbss$begin/$end, so that link has no .textbss contribution",
                p.display()
            ),
            Some(m) => {
                let (b, e) = (
                    t.text_rva.saturating_add(m.begin),
                    t.text_rva.saturating_add(m.end),
                );
                let len = e.saturating_sub(b);
                println!(
                    "  map: __enc$textbss$begin..$end = rva {b:#x}..{e:#x}, {len} ({len:#x}) byte(s); segment row {}. {}",
                    m.row_len
                        .map_or_else(|| "absent".to_string(), |x| format!("{x:#x}")),
                    if t.run_from <= b && e == t.text_end {
                        "The run covers the contribution and the contribution ends the section."
                    } else {
                        "The run does NOT cover the map's .textbss."
                    }
                );
            }
        }
    }
    Ok(Outcome::Ok)
}

fn find_lib(dirs: &[PathBuf], dll: &str) -> Option<PathBuf> {
    let stem = dll
        .rsplit_once('.')
        .map_or(dll, |(s, _)| s)
        .to_ascii_lowercase();
    let want = format!("{stem}.lib");
    dirs.iter().find_map(|d| {
        std::fs::read_dir(d)
            .ok()?
            .flatten()
            .map(|e| e.path())
            .find(|p| {
                p.file_name()
                    .and_then(|n| n.to_str())
                    .is_some_and(|n| n.eq_ignore_ascii_case(&want))
            })
    })
}

#[derive(Debug, Serialize)]
struct DllRow {
    index: usize,
    dll: String,
    symbols: usize,
    ordinals: usize,
    descriptor: u32,
    name_rva: u32,
    ilt_rva: u32,
    iat_rva: u32,
    timestamp: u32,
    forwarder_chain: u32,
}

#[derive(Debug, Serialize)]
struct EntryRow {
    dll: String,
    slot: usize,
    iat_rva: u32,
    hint_rva: Option<u32>,
    hint: Option<u16>,
    name: String,
    ordinal: Option<u16>,
}

#[derive(Debug, Serialize)]
struct ImportSummary {
    rva: u32,
    size: u32,
    section: String,
    dlls: usize,
    symbols: usize,
    iat_directory: Option<(u32, u32)>,
    orders: anatomy::Orders,
    anatomy: Vec<anatomy::SectionAnatomy>,
}

pub fn imports(config: Option<&Config>, a: &ImageImportsArgs) -> Result<Outcome> {
    let (r, img) = open(config, &a.image)?;
    let tables = img.imports()?;
    let Some(d) = img.present(Dir::Import) else {
        json::<(), DllRow>(&a.json, "image imports", &r.label, None, &[])?;
        println!("{}: no import directory", r.label);
        return Ok(Outcome::Ok);
    };
    let symbols: usize = tables.iter().map(|t| t.entries.len()).sum();
    let summary = ImportSummary {
        rva: d.rva.get(),
        size: d.size,
        section: section_name(&img, d.rva),
        dlls: tables.len(),
        symbols,
        iat_directory: img.present(Dir::Iat).map(|x| (x.rva.get(), x.size)),
        orders: anatomy::orders(&tables),
        anatomy: anatomy::anatomy(&img, &tables),
    };
    if a.entries {
        let rows: Vec<EntryRow> = tables
            .iter()
            .flat_map(|t| {
                t.entries.iter().enumerate().map(|(i, e)| EntryRow {
                    dll: t.dll.clone(),
                    slot: i,
                    iat_rva: e.iat_rva.get(),
                    hint_rva: e.hint_rva.map(Rva::get),
                    hint: e.hint,
                    name: e.name.clone(),
                    ordinal: e.ordinal,
                })
            })
            .collect();
        json(&a.json, "image imports", &r.label, Some(&summary), &rows)?;
    } else {
        let rows: Vec<DllRow> = tables
            .iter()
            .map(|t| DllRow {
                index: t.index,
                dll: t.dll.clone(),
                symbols: t.entries.len(),
                ordinals: t.entries.iter().filter(|e| e.by_ordinal()).count(),
                descriptor: t.descriptor.get(),
                name_rva: t.name_rva.get(),
                ilt_rva: t.ilt_rva.get(),
                iat_rva: t.iat_rva.get(),
                timestamp: t.timestamp,
                forwarder_chain: t.forwarder_chain,
            })
            .collect();
        json(&a.json, "image imports", &r.label, Some(&summary), &rows)?;
    }
    println!("{}:", r.label);
    println!(
        "  import directory {:#x} + {} in {}: {} DLLs, {} symbols",
        summary.rva, summary.size, summary.section, summary.dlls, summary.symbols
    );
    for t in &tables {
        println!(
            "    {:<16} {:>4} symbol(s)  ILT {:#08x}  IAT {:#08x}  name {:#08x}{}",
            t.dll,
            t.entries.len(),
            t.ilt_rva.get(),
            t.iat_rva.get(),
            t.name_rva.get(),
            if t.timestamp != 0 || t.forwarder_chain != 0 {
                format!("  stamps {:#010x}/{:#010x}", t.timestamp, t.forwarder_chain)
            } else {
                String::new()
            }
        );
    }
    let o = &summary.orders;
    println!("\n  the orders:");
    println!(
        "    descriptors  .idata$2  per LIBRARY   {}",
        o.descriptors.join(", ")
    );
    println!(
        "    dll names    .idata$6  per LIBRARY   {}",
        o.names.join(", ")
    );
    println!(
        "    ILT          .idata$4  per FUNCTION  {}",
        o.ilt.join(", ")
    );
    println!(
        "    IAT          .idata$5  per FUNCTION  {}",
        o.iat.join(", ")
    );
    if !a.libdir.is_empty() {
        println!("\n  is the per-function order the import library's member order?");
        for t in &tables {
            let Some(lib) = find_lib(&a.libdir, &t.dll) else {
                println!(
                    "    {:16} no import library in the --libdir directories",
                    t.dll
                );
                continue;
            };
            let d = project::read(&lib)?;
            let ar = Archive::parse(&d).with_context(|| lib.display().to_string())?;
            let m = anatomy::member_order(t, &ar.import_order());
            println!(
                "    {:16} {:>3}/{:<3} in lib   ILT slot order: {:5}   name layout order: {:5}",
                m.dll, m.in_library, m.named, m.slot_order, m.name_order
            );
        }
    }
    for s in &summary.anatomy {
        println!();
        for l in s.lines() {
            println!("{l}");
        }
    }
    if a.entries {
        for t in &tables {
            println!("\n  {}:", t.dll);
            for e in &t.entries {
                println!(
                    "    IAT {:#08x}  {}  {}",
                    e.iat_rva.get(),
                    match (e.hint_rva, e.hint) {
                        (Some(h), Some(n)) => format!("hint {n:>4} at {:#08x}", h.get()),
                        _ => format!("{:>22}", "by ordinal"),
                    },
                    e.name
                );
            }
        }
    }
    Ok(Outcome::Ok)
}

pub fn exports(config: Option<&Config>, a: &ImageExportsArgs) -> Result<Outcome> {
    let (r, img) = open(config, &a.image)?;
    let Some(e) = img.exports()? else {
        json::<(), ()>(&a.json, "image exports", &r.label, None, &[])?;
        println!("{}: no export directory", r.label);
        return Ok(Outcome::Ok);
    };
    json(&a.json, "image exports", &r.label, Some(&e), &e.entries)?;
    let d = img.dir(Dir::Export).unwrap_or_default();
    println!(
        "{}: export directory {:#x} + {} in {}: '{}', ordinal base {}, {} function(s), {} name(s)",
        r.label,
        d.rva.get(),
        d.size,
        section_name(&img, d.rva),
        e.dll_name,
        e.ordinal_base,
        e.functions,
        e.names
    );
    let delta = img.timestamp.saturating_sub(e.timestamp);
    if e.timestamp == img.timestamp {
        println!(
            "  timestamp {:#010x} == the PE header's: laid down by the final link",
            e.timestamp
        );
    } else if delta > 0 {
        println!(
            "  timestamp {:#010x} is {delta}s OLDER than the PE header's {:#010x}: an earlier link's, so \
             this image was relinked",
            e.timestamp, img.timestamp
        );
    } else {
        println!(
            "  timestamp {:#010x} is LATER than the PE header's {:#010x}",
            e.timestamp, img.timestamp
        );
    }
    for x in &e.entries {
        println!(
            "    @{:<4} {:#08x}  {}{}",
            x.ordinal,
            x.target.get(),
            x.name.as_deref().unwrap_or("(by ordinal)"),
            x.forwarder
                .as_deref()
                .map_or_else(String::new, |f| format!("  -> {f}"))
        );
    }
    Ok(Outcome::Ok)
}

fn fields_of(over: Option<&Vec<String>>, r: &Resolved) -> Result<Vec<Field>> {
    match over {
        Some(v) => v.iter().map(|s| Field::parse(s.trim())).collect(),
        None => Ok(r.decl.as_ref().map(|d| d.clock.clone()).unwrap_or_default()),
    }
}

pub fn cmp_verb(config: Option<&Config>, a: &ImageCmpArgs) -> Result<Outcome> {
    let (r, ri) = open(config, &a.reference)?;
    let ob = project::read(&a.candidate)?;
    let oi = Image::parse(ob).with_context(|| a.candidate.display().to_string())?;
    let costs = cmp::section_costs(&ri, &oi);
    let s = cmp::summary(ri.bytes(), oi.bytes(), &costs);
    json(&a.json, "image cmp", &r.label, Some(&s), &costs)?;
    println!(
        "{}: {} against the reference {}\n",
        r.label,
        a.candidate.display(),
        r.path.display()
    );
    println!(
        "  reference  size {:>9}  md5 {}",
        s.reference_size, s.reference_md5
    );
    println!(
        "  candidate  size {:>9}  md5 {}",
        s.candidate_size, s.candidate_md5
    );
    println!(
        "  {}",
        if s.md5_identical {
            "IDENTICAL: the md5s are equal".to_string()
        } else {
            format!(
                "DIFFERS: the md5s differ{}",
                if s.reference_size == s.candidate_size {
                    ", though the size is equal"
                } else {
                    ""
                }
            )
        }
    );
    println!(
        "  differing bytes {} of {}",
        s.differing,
        s.reference_size.max(s.candidate_size)
    );
    println!(
        "\n  {:<10} {:>10} {:>10}  {:>7}",
        "section", "ref bytes", "differing", "of"
    );
    for c in &costs {
        let (Some(ours), Some(d)) = (c.ours, c.differing) else {
            println!("  {:<10} {:>10} {:>10}", c.name, c.reference, "ABSENT");
            continue;
        };
        let flag = if ours == c.reference {
            String::new()
        } else {
            let delta = cmp::signed(ours).saturating_sub(cmp::signed(c.reference));
            format!("   candidate {ours}: a length difference of {delta:+} byte(s)")
        };
        println!(
            "  {:<10} {:>10} {:>10}  {:>6.1}%{flag}",
            c.name,
            c.reference,
            d,
            cmp::percent(d, c.reference)
        );
    }
    println!("  {:<10} {:>10} {:>10}", "sum", "", s.sections_sum);
    let (fd, sd) = (cmp::signed(s.differing), cmp::signed(s.sections_sum));
    match sd.cmp(&fd) {
        std::cmp::Ordering::Equal => {
            println!("  the sections account for the whole-file figure; the headers do not differ");
        }
        std::cmp::Ordering::Less => println!(
            "  {} byte(s) of the whole-file figure lie outside every section: the headers and the Rich block",
            fd.saturating_sub(sd)
        ),
        std::cmp::Ordering::Greater => println!(
            "  the section sum exceeds the whole-file figure by {}: a section whose length differs is counted \
             here and again at shifted offsets in the whole file",
            sd.saturating_sub(fd)
        ),
    }
    let fields = fields_of(a.clock.as_ref(), &r)?;
    if fields.is_empty() {
        println!("\n  clock fields: none declared");
    } else {
        match clock::compare(&oi, &ri, &fields) {
            Ok(readings) => {
                println!("\n  clock fields (copied by the post-link step; not compiled):");
                for l in clock::render(&readings).0 {
                    println!("{l}");
                }
            }
            Err(e) => println!("\n  clock fields: UNRESOLVED: {e:#}"),
        }
    }
    if let Some(base) = &a.against {
        let bb = project::read(base)?;
        let bd = msvc6_base::digest::differ(ri.bytes(), &bb);
        let delta = fd.saturating_sub(cmp::signed(bd));
        println!(
            "\n  against {}: that baseline differs by {bd}, this candidate by {}; delta {delta:+}\n    {}",
            base.display(),
            s.differing,
            match cmp::baseline_verdict(delta) {
                cmp::Delta::ClockBand => format!(
                    "inside the {}-byte band three wall-clock fields move on every relink: not a result",
                    clock::RELINK_NOISE_BYTES
                ),
                cmp::Delta::Better => "fewer differing bytes than the baseline".to_string(),
                cmp::Delta::Worse => "MORE differing bytes than the baseline".to_string(),
            }
        );
    }
    Ok(if s.md5_identical {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[derive(Debug, serde::Deserialize)]
#[serde(deny_unknown_fields)]
struct RowsFile {
    #[serde(default)]
    residue: Vec<Row>,
}

#[derive(Debug, Serialize)]
struct PostlinkSummary {
    clock: Vec<clock::Reading>,
    totals: postlink::Totals,
    written: usize,
    clock_bytes: usize,
}

fn print_rows(rows: &[postlink::RowSite], verbose: bool) {
    let mut by: BTreeMap<(&str, &str), (usize, usize)> = BTreeMap::new();
    for r in rows {
        let k = r.state.word();
        let e = by.entry((r.kind.name(), k)).or_default();
        e.0 = e.0.saturating_add(1);
        e.1 = e.1.saturating_add(r.differing);
        if verbose || r.blocked() || r.at.is_some() {
            let site = r
                .site
                .map_or_else(|| "-".to_string(), |s| format!("{s:#x}"));
            let why = match &r.state {
                State::Blocked(w) => format!("  -- {w}"),
                _ => String::new(),
            };
            let extra = r.extra.map_or_else(String::new, |(f, v)| {
                format!(" (+VirtualSize := {v:#x} at {f:#x})")
            });
            println!(
                "    {k:<8} {:<13} ref {:#08x}+{:<3} image {site:<9} {:>3} B differ{extra}  {}{why}{}",
                r.kind.name(),
                r.off,
                r.len,
                r.differing,
                r.at.map_or_else(String::new, |a| format!("[{a:?}] ")),
                if verbose && !r.note.is_empty() {
                    format!("  ({})", r.note)
                } else {
                    String::new()
                },
            );
        }
    }
    for ((g, k), (n, b)) in &by {
        println!("  {g:<13} {k:<8} {n:>4} row(s) {b:>6} B differing");
    }
}

pub fn postlink_verb(config: Option<&Config>, a: &ImagePostlinkArgs) -> Result<Outcome> {
    let (r, ri) = open(config, &a.reference)?;
    let rows: Vec<Row> = match &a.rows {
        Some(p) => {
            let text = srcio::own_read(p).with_context(|| format!("reading {}", p.display()))?;
            toml::from_str::<RowsFile>(&text)
                .with_context(|| p.display().to_string())?
                .residue
        }
        None => r
            .decl
            .as_ref()
            .map(|d| d.residue.clone())
            .unwrap_or_default(),
    };
    let fields = fields_of(a.clock.as_ref(), &r)?;
    let mut ours = project::read(&a.image)?;
    let mut oi = Image::parse(ours.clone()).with_context(|| a.image.display().to_string())?;
    let before = clock::compare(&oi, &ri, &fields)?;
    let sites = postlink::resolve(&rows, &ri, &oi)?;
    println!(
        "{}: post-link step on {} ({} residue row(s), {} clock field(s))",
        r.label,
        a.image.display(),
        rows.len(),
        fields.len()
    );
    if !fields.is_empty() {
        println!("  clock fields:");
        for l in clock::render(&before).0 {
            println!("{l}");
        }
    }
    print_rows(&sites, a.verbose);
    let t = postlink::totals(&sites);
    println!(
        "  {} B owed by the step in {} row(s); {} BLOCKED row(s) hold {} B of the image's own bytes, never \
         overwritten",
        t.owed,
        t.rows.saturating_sub(t.blocked_rows),
        t.blocked_rows,
        t.blocked_bytes
    );
    let mut summary = PostlinkSummary {
        clock: before,
        totals: t,
        written: 0,
        clock_bytes: 0,
    };
    if !a.apply {
        json(&a.json, "image postlink", &r.label, Some(&summary), &sites)?;
        return Ok(Outcome::Ok);
    }
    if !fields.is_empty() {
        let (next, applied) = clock::apply(&oi, &ri, &fields)?;
        summary.clock_bytes = applied.bytes;
        ours = next;
    }
    let (next, stepped) = postlink::step(&rows, &ri, ours)?;
    summary.written = stepped.written;
    oi = Image::parse(next).with_context(|| a.image.display().to_string())?;
    srcio::replace_bytes(&a.image, oi.bytes())
        .with_context(|| format!("writing {}", a.image.display()))?;
    summary.clock = clock::compare(&oi, &ri, &fields)?;
    summary.totals = postlink::totals(&stepped.rows);
    json(
        &a.json,
        "image postlink",
        &r.label,
        Some(&summary),
        &stepped.rows,
    )?;
    println!(
        "  WROTE {} byte(s) into {}: {} clock byte(s), {} residue byte(s) in {} row(s) over {} pass(es); \
         the values were read from the reference",
        summary.clock_bytes.saturating_add(stepped.written),
        a.image.display(),
        summary.clock_bytes,
        stepped.written,
        stepped.rows_written,
        stepped.passes
    );
    let clock_ok = summary.clock.iter().all(clock::Reading::identical);
    if summary.totals.owed != 0 || !clock_ok {
        bail!(
            "post-link: {} B still differ in non-blocked rows{} after the write",
            summary.totals.owed,
            if clock_ok { "" } else { ", and a clock field" }
        );
    }
    println!(
        "  every non-blocked row and clock field now matches the reference; {} blocked row(s) left",
        summary.totals.blocked_rows
    );
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct ResRow {
    type_id: String,
    type_name: String,
    name: String,
    lang: String,
    codepage: u32,
    rva: u32,
    size: usize,
}

pub fn res_verb(config: Option<&Config>, a: &ImageResArgs) -> Result<Outcome> {
    let (r, img) = open(config, &a.image)?;
    let items = res::res_items(&img)?;
    if items.is_empty() {
        json::<(), ResRow>(&a.json, "image res", &r.label, None, &[])?;
        println!("{}: no resources", r.label);
        return Ok(Outcome::Ok);
    }
    let rows: Vec<ResRow> = items
        .iter()
        .map(|x| ResRow {
            type_id: x.type_id.to_string(),
            type_name: res::type_name(&x.type_id),
            name: x.name_id.to_string(),
            lang: x.lang_id.to_string(),
            codepage: x.codepage,
            rva: x.data_rva.get(),
            size: x.data_len,
        })
        .collect();
    json::<(), _>(&a.json, "image res", &r.label, None, &rows)?;
    let types: BTreeSet<&str> = rows.iter().map(|x| x.type_name.as_str()).collect();
    println!(
        "{}: {} resource(s) of {} type(s), {} byte(s) of data",
        r.label,
        rows.len(),
        types.len(),
        rows.iter().map(|x| x.size).sum::<usize>()
    );
    for x in &rows {
        println!(
            "    {:<14} {:<20} lang {:<6} codepage {:<5} {:>8} byte(s) at rva {:#x}",
            x.type_name, x.name, x.lang, x.codepage, x.size, x.rva
        );
    }
    if let Some(out) = &a.out {
        let blob = res::res_blob(&img, &items)?;
        srcio::write_bytes(out, &blob).with_context(|| format!("writing {}", out.display()))?;
        println!("  wrote {} ({} bytes)", out.display(), blob.len());
    }
    Ok(Outcome::Ok)
}
