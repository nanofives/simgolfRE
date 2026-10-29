use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand, ValueEnum};
use msvc6_image::macho::{self, CPU_I386, CPU_PPC};
use msvc6_image::pef::{self, Kind};
use msvc6_image::traceback::{self, Table};
use msvc6_match::input;
use msvc6_match::mangle::{self, Sig};
use serde::Serialize;
use std::path::{Path, PathBuf};

#[derive(Debug, Args)]
pub struct MacArgs {
    #[command(subcommand)]
    pub verb: MacVerb,
}

#[derive(Debug, Subcommand)]
pub enum MacVerb {
    #[command(
        about = "A PEF container's or a Mach-O image's sections, libraries, imports, exports and symbols",
        long_about = "Read a Classic Mac OS / CarbonLib PEF container (`Joy!peff`: its sections, pattern-initialised \
                      data unpacked, the loader section's imported libraries, imported symbols, exports, the \
                      relocations of the data section and the TOC its main transition vector names) or a 32-bit \
                      Mach-O image (thin, or each slice of a fat one: its sections, symbols and the import stubs and \
                      pointers the indirect symbol table names). Read only, never run; every count is bounded by the \
                      bytes that hold it. JSON rows: one per section."
    )]
    Info(InfoArgs),
    #[command(
        about = "The named functions of a PEF code section (its traceback tables) or a Mach-O slice (its symbols)",
        long_about = "Every function a Mac build names: in a PEF, each traceback table with a name after the code \
                      section's functions (CodeWarrior and MrC leave the AIX table after each function: a zero word, \
                      flags, the offset back to the first instruction, the mangled name), the function running from \
                      its first instruction to its table; in a Mach-O slice (--cpu, default i386), each symbol of an \
                      instruction section, ending at the next symbol or its section's end (a second name at one \
                      address is dropped). One row per function: start, end, name, and the name read as a \
                      signature (CodeWarrior's Cfront scheme for a PEF, Itanium's for a Mach-O). JSON rows: one per \
                      function."
    )]
    Functions(FunctionsArgs),
    #[command(
        about = "Every PowerPC traceback table in a PEF's executable sections or a Mach-O ppc slice's code",
        long_about = "Scan big-endian PowerPC code for AIX / CFM traceback tables: a zero word on a 4-byte boundary, \
                      version 0, the name-present flag, then the optional parameter word, tb_offset, handler mask \
                      and controlled-storage words (at most 64) before the name (1 to 1023 printable bytes). A PEF's \
                      code and executable-data sections are scanned, a Mach-O ppc slice's instruction sections. One \
                      line per table: the section, the function's first instruction (the table less its tb_offset; a \
                      table with none prints 0 and a size of -1, as the evidence's own scan does), the table, the \
                      function's size, the name as written, its \
                      language code and the instruction word before the table, offsets in the section. JSON rows: \
                      one per table."
    )]
    Tracebacks(TracebacksArgs),
    #[command(
        about = "Read mangled names and definition lines as signatures: scope, name, parameters",
        long_about = "Read each NAME as a signature: a CodeWarrior / MrC name in the Cfront scheme (`draw__6WidgetFii`, \
                      `__ct__6WidgetFv`, a traceback's leading `.` dropped), an Itanium name (`_ZN6Widget4drawEii`, \
                      through cpp_demangle), or a C++ definition line (`int Widget::draw(int x, int y)`). --scheme \
                      auto takes `_Z` as Itanium, a line with a parenthesis as text, anything else as CodeWarrior. \
                      Prints the signature key (`Class::name(param,...)`, parameter types normalised: const, \
                      volatile, class and struct dropped, `unsigned int` as `unsigned`, a function pointer as \
                      `fnptr`), whether it takes a `this`, and its parameters' kinds. JSON rows: one per name."
    )]
    Demangle(DemangleArgs),
    #[command(
        about = "Name our functions from Mac builds of shared code: each pairing, its evidence, and a name map",
        long_about = "Pair the named functions of each Mac SOURCE (a PEF's traceback names, read in CodeWarrior's \
                      scheme, or an unstripped Mach-O i386 slice's symbols, read in Itanium's) with IMAGE's, across \
                      the change of compiler and instruction set: ANCHORS (a string, an API by its cross-side name, \
                      a floating literal or a distinctive integer few functions use on both sides; a masked body \
                      hash between two MSVC sides) vote, and a pair lands as the mutual best of both its functions, \
                      untied, at one full vote, of comparable size, then CALL ORDER and SOLE CALLERS to a fixed \
                      point; a pair whose names' signatures contradict (the parameter count, a `this`, MSVC's `ret \
                      n` against the name's parameters, `ecx` read as a `this`) never lands. Two callees of one \
                      caller with the same signature are told apart by no gate, so their call-order pair needs a \
                      second caller proposing it (--single-link-twins lands it on position alone). Each source is \
                      also paired with its PC twin and the twin with IMAGE by anchors alone: the CHAIN, an \
                      independent second link, whose strong hops seed the direct pairing. Our functions come from \
                      IMAGE's function index, their signatures from the annotation table's definitions. Each of \
                      our functions in --scope then takes the best candidate name (a strong link, a string, a \
                      floating literal or a body hash, or two independent links with the signature agreeing by \
                      kinds; the lowest rank wins; two names of one family conflicting decide nothing), each class \
                      the source class most of its picked methods name (3 of them, or 1 with its constructor or \
                      destructor, three in four agreeing), and between two picked functions of one file a CodeWarrior \
                      build's functions in the same order pair by position when signature, class and field \
                      offsets or constants agree. Writes the map (kind state reason image va old new source mac \
                      mangled twin sig evidence detail: every function in scope, every class, every Mac name of \
                      --classes, APPLIED or RECORDED with one reason) and per source <label>.direct.tsv (its direct \
                      pairs) and, for a PEF, <label>.runs.tsv (the runs its `__sinit_` static initialisers close). \
                      Prints the count of each kind, state and reason."
    )]
    Pair(Box<crate::macpair::PairArgs>),
    #[command(
        about = "A name map's parameter types against the Mac's, each difference decided or refused",
        long_about = "For each APPLIED function or method row of a name map whose Mac signature (the `mac` column, a \
                      demangled `Class::name(params) const`) is known, compare our parameter types with it in one \
                      canonical form (class / struct / union / enum and calling conventions dropped, a leading const \
                      moved after its type, spaces dropped around * and &). Ours come from the image's decorated \
                      name at the row's rva whose qualified name is the row's `new`, else from the row's own \
                      `mangled` with every APPLIED class rename applied. Equal types read `same` (or APPLIED when the \
                      row's own mangled name differed); a difference takes the row's `types` decision (`RECORDED: \
                      why`), else the verdict of each KIND of difference (`FROM=>TO`, `ARITY`, `CONST`) from the \
                      verdicts table: all RECORDED reads RECORDED with their reasons; a kind with no verdict is \
                      UNDECIDED and one whose verdict is APPLIED is CONTRADICTED. --fill first writes the `mac` \
                      column from a Mac symbol dump: for each row naming a Mac function, an exact parameter match, \
                      then the same arity with the most equal parameters, then the one candidate left. Prints \
                      the count of each kind, state and reason and of each type state; exit 3 when a row is \
                      UNDECIDED or CONTRADICTED."
    )]
    Types(Box<crate::macpair::TypesArgs>),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum Cpu {
    I386,
    Ppc,
}

impl Cpu {
    fn code(self) -> u32 {
        match self {
            Self::I386 => CPU_I386,
            Self::Ppc => CPU_PPC,
        }
    }
}

#[derive(Debug, Args)]
pub struct InfoArgs {
    pub file: PathBuf,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct FunctionsArgs {
    pub file: PathBuf,
    #[arg(
        long,
        value_enum,
        default_value = "i386",
        help = "The Mach-O slice to read"
    )]
    pub cpu: Cpu,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct TracebacksArgs {
    pub file: PathBuf,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum Scheme {
    Auto,
    Cw,
    Itanium,
    Text,
}

#[derive(Debug, Args)]
pub struct DemangleArgs {
    #[arg(required = true)]
    pub names: Vec<String>,
    #[arg(long, value_enum, default_value = "auto")]
    pub scheme: Scheme,
    #[arg(long, help = "The class a bare in-class definition line belongs to")]
    pub class: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

pub enum Container {
    Pef(Box<pef::Pef>),
    MachO(Vec<u8>),
}

pub fn open(path: &Path) -> Result<Container> {
    let bytes = input::read(path, input::MAX_IMAGE)?;
    if bytes.starts_with(pef::MAGIC) {
        return Ok(Container::Pef(Box::new(
            pef::load(&bytes).with_context(|| path.display().to_string())?,
        )));
    }
    macho::slices(&bytes).with_context(|| {
        format!(
            "{}: neither a PEF container nor a Mach-O image",
            path.display()
        )
    })?;
    Ok(Container::MachO(bytes))
}

#[derive(Debug, Serialize)]
struct SectionRow {
    index: usize,
    kind: String,
    address: String,
    size: u64,
    offset: u64,
    detail: String,
}

fn pef_info(path: &Path, p: &pef::Pef) -> Vec<SectionRow> {
    let h = &p.header;
    println!(
        "{}: PEF {} format {}, {} section(s) ({} instantiated), timestamp {:#x}",
        path.display(),
        h.architecture,
        h.format_version,
        h.sections.len(),
        h.instantiated,
        h.timestamp
    );
    let rows: Vec<SectionRow> = h
        .sections
        .iter()
        .map(|s| SectionRow {
            index: s.index,
            kind: s.kind.label(),
            address: format!("{:#x}", s.default_address),
            size: u64::from(s.total),
            offset: u64::from(s.offset),
            detail: format!(
                "unpacked {} packed {} share {} align {}",
                s.unpacked, s.packed, s.share, s.alignment
            ),
        })
        .collect();
    for r in &rows {
        println!(
            "  section {} {:<9} address {} total {:#x} at {:#x}: {}",
            r.index, r.kind, r.address, r.size, r.offset, r.detail
        );
    }
    let l = &p.loader;
    println!(
        "  loader: {} librar(ies), {} import(s), {} export(s), {} relocated data word(s); code section {}, data section {}, TOC {:#x}",
        l.libraries.len(),
        l.imports.len(),
        l.exports.len(),
        p.relocs.len(),
        p.code_section,
        p.data_section,
        p.toc
    );
    for lib in &l.libraries {
        println!(
            "    library {} imports {} (first {})",
            lib.name, lib.count, lib.first
        );
    }
    println!(
        "  traceback tables: {} named function(s) in the code section",
        p.tables.len()
    );
    rows
}

fn macho_info(path: &Path, bytes: &[u8]) -> Result<Vec<SectionRow>> {
    let slices = macho::slices(bytes)?;
    println!("{}: Mach-O, {} slice(s)", path.display(), slices.len());
    let mut rows = Vec::new();
    for s in &slices {
        println!(
            "  slice {} at {:#x}, {} byte(s)",
            macho::cpu_name(s.cpu),
            s.offset,
            s.size
        );
        let body = bytes
            .get(s.offset..s.offset.saturating_add(s.size))
            .context("slice past the file")?;
        let m = match macho::parse(body) {
            Ok(m) => m,
            Err(e) => {
                println!("    unread: {e:#}");
                continue;
            }
        };
        println!(
            "    {} section(s), {} symbol(s), {} function(s), {} stub(s), {} pointer slot(s)",
            m.sections.len(),
            m.symbols.len(),
            m.functions().len(),
            m.stubs.len(),
            m.slots.len()
        );
        for x in &m.sections {
            let row = SectionRow {
                index: x.index,
                kind: x.label(),
                address: format!("{:#x}", x.addr),
                size: x.size,
                offset: u64::from(x.offset),
                detail: format!(
                    "{} type {:#x}{}",
                    macho::cpu_name(s.cpu),
                    x.kind(),
                    if x.is_code() { " code" } else { "" }
                ),
            };
            println!(
                "    section {} {:<24} address {} size {:#x}: {}",
                row.index, row.kind, row.address, row.size, row.detail
            );
            rows.push(row);
        }
    }
    Ok(rows)
}

pub fn info(a: &InfoArgs) -> Result<Outcome> {
    let rows = match open(&a.file)? {
        Container::Pef(p) => pef_info(&a.file, &p),
        Container::MachO(b) => macho_info(&a.file, &b)?,
    };
    json(
        &a.json,
        "mac info",
        &a.file.display().to_string(),
        None::<&()>,
        &rows,
    )?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct FunctionRow {
    start: String,
    end: String,
    name: String,
    signature: Option<String>,
}

pub type Named = (u64, u64, String, Option<Sig>);

pub fn named_functions(path: &Path, cpu: u32) -> Result<Vec<Named>> {
    Ok(match open(path)? {
        Container::Pef(p) => p
            .tables
            .iter()
            .map(|t| {
                (
                    u64::from(t.start),
                    u64::from(t.at),
                    t.function().to_string(),
                    mangle::cw(t.function()),
                )
            })
            .collect(),
        Container::MachO(b) => macho::load(&b, cpu)?
            .functions()
            .into_iter()
            .map(|(va, end, name)| {
                let s = mangle::itanium(&name);
                (va, end, name, s)
            })
            .collect(),
    })
}

pub fn functions(a: &FunctionsArgs) -> Result<Outcome> {
    let mut rows = Vec::new();
    for (start, end, name, sig) in named_functions(&a.file, a.cpu.code())? {
        let row = FunctionRow {
            start: format!("{start:#x}"),
            end: format!("{end:#x}"),
            signature: sig.map(|s| s.key()),
            name,
        };
        println!(
            "{}\t{}\t{}\t{}",
            row.start,
            row.end,
            row.name,
            row.signature.as_deref().unwrap_or("-")
        );
        rows.push(row);
    }
    eprintln!("{}: {} named function(s)", a.file.display(), rows.len());
    json(
        &a.json,
        "mac functions",
        &a.file.display().to_string(),
        None::<&()>,
        &rows,
    )?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct TableRow {
    section: usize,
    start: Option<u32>,
    at: u32,
    size: Option<u32>,
    name: String,
    lang: u8,
    prev: u32,
}

fn prev_word(code: &[u8], at: u32) -> u32 {
    usize::try_from(at)
        .ok()
        .and_then(|a| a.checked_sub(4))
        .and_then(|p| msvc6_image::be::u32_at(code, p))
        .unwrap_or(0)
}

fn scan_rows(section: usize, code: &[u8], out: &mut Vec<TableRow>) {
    for t in traceback::scan(code) {
        let Table {
            at,
            start,
            name,
            lang,
            tb_offset,
            ..
        } = t;
        let start = tb_offset.map(|_| start);
        out.push(TableRow {
            section,
            start,
            at,
            size: start.map(|s| at.saturating_sub(s)),
            name,
            lang,
            prev: prev_word(code, at),
        });
    }
}

pub fn tracebacks(a: &TracebacksArgs) -> Result<Outcome> {
    let bytes = input::read(&a.file, input::MAX_IMAGE)?;
    let mut rows = Vec::new();
    if bytes.starts_with(pef::MAGIC) {
        let h = pef::header(&bytes).with_context(|| a.file.display().to_string())?;
        for s in h
            .sections
            .iter()
            .filter(|s| matches!(s.kind, Kind::Code | Kind::ExecutableData))
        {
            scan_rows(s.index, pef::raw(&bytes, s)?, &mut rows);
        }
    } else {
        let m = macho::load(&bytes, CPU_PPC).with_context(|| a.file.display().to_string())?;
        for s in m.sections.iter().filter(|s| s.is_code()) {
            if let Some(code) = m.section_bytes(s) {
                scan_rows(s.index, code, &mut rows);
            }
        }
    }
    for r in &rows {
        println!(
            "{}\t0x{:06x}\t0x{:06x}\t{}\t{}\tlang={}\tprev=0x{:08x}",
            r.section,
            r.start.unwrap_or(0),
            r.at,
            r.size.map_or_else(|| "-1".to_string(), |s| s.to_string()),
            r.name,
            r.lang,
            r.prev
        );
    }
    eprintln!(
        "{}: {} named traceback table(s)",
        a.file.display(),
        rows.len()
    );
    json(
        &a.json,
        "mac tracebacks",
        &a.file.display().to_string(),
        None::<&()>,
        &rows,
    )?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct SigRow {
    name: String,
    scheme: String,
    key: Option<String>,
    this: Option<bool>,
    kinds: Vec<String>,
}

pub fn demangle(a: &DemangleArgs) -> Result<Outcome> {
    let mut rows = Vec::new();
    let mut unread = 0usize;
    for n in &a.names {
        let scheme = match a.scheme {
            Scheme::Auto if n.starts_with("_Z") => Scheme::Itanium,
            Scheme::Auto if n.contains('(') => Scheme::Text,
            Scheme::Auto => Scheme::Cw,
            s => s,
        };
        let sig = match scheme {
            Scheme::Itanium => mangle::itanium(n),
            Scheme::Text => mangle::parse_text(n, a.class.as_deref()),
            _ => mangle::cw(n),
        };
        let label = match scheme {
            Scheme::Itanium => "itanium",
            Scheme::Text => "text",
            _ => "cw",
        };
        let row = SigRow {
            name: n.clone(),
            scheme: label.to_string(),
            key: sig.as_ref().map(Sig::key),
            this: sig.as_ref().and_then(Sig::has_this),
            kinds: sig
                .as_ref()
                .map(|s| s.kinds().iter().map(|k| format!("{k:?}")).collect())
                .unwrap_or_default(),
        };
        match &row.key {
            Some(k) => println!(
                "{n}\t{label}\t{k}\tthis={}\t{}",
                row.this.map_or("?", |t| if t { "yes" } else { "no" }),
                row.kinds.join(",")
            ),
            None => {
                unread = unread.saturating_add(1);
                println!("{n}\t{label}\tUNREAD");
            }
        }
        rows.push(row);
    }
    json(&a.json, "mac demangle", "-", None::<&()>, &rows)?;
    if unread > 0 && unread == rows.len() {
        bail!("no name read");
    }
    Ok(Outcome::Ok)
}
