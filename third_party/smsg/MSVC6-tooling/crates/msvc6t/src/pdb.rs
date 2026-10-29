use crate::Outcome;
use crate::project::{self, Config};
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_base::digest::{md5_hex, sha256_hex};
use msvc6_base::{rowjson, srcio};
use msvc6_pdb::{attach, eq, info, llvm, msf, msf7, procs, structure, to70};
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

#[derive(Debug, Args)]
pub struct PdbArgs {
    #[command(subcommand)]
    pub verb: PdbVerb,
}

#[derive(Debug, Args)]
pub struct Json {
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write the rows as JSON to FILE: {format, command, image, summary?, rows}. The text is unchanged."
    )]
    pub json: Option<PathBuf>,
}

#[derive(Debug, Subcommand)]
pub enum PdbVerb {
    #[command(
        about = "What a PDB is: its container, identity, DBI and the configured image it belongs to",
        long_about = "For each PDB (2.0 or 7.0): the container (page size, pages, streams), the identity (interface \
                      version, signature, age, the 7.0 GUID), the DBI (modules, symbol records by kind, an empty DBI \
                      is a library's or a compiler's PDB: types only) and the TPI's type count, then which \
                      configured image's CodeView record names it (signature and age for NB10, GUID and age for \
                      RSDS). JSON rows: one per file."
    )]
    Info(InfoArgs),
    #[command(
        about = "Check a PDB's structure with a reader of its own",
        long_about = "PDB 2.0: the structure check, a reader of its own that shares no parser with the writers. \
                      The small MSF (the file is its pages, the header names one of the two free page map regions, \
                      every page has one owner, owned pages are marked used and unowned ones free); the DBI's \
                      substreams; each module stream's length (cbSyms + cbLines + cbC13 and the global \
                      references) and its symbol region tiling 4..cbSyms; every scope's pParent, pEnd and pNext \
                      against the nesting, a scope kind VC6 does not write refused; every S_PROCREF, S_LPROCREF and \
                      S_DATAREF ibSym a record start of its module of the kind and name it says, each procedure and \
                      thunk referenced once; the EC names reachable by hash and each niSrcFile its S_COMPILE2 src; \
                      /LinkInfo's ichOutfile. PDB 7.0: the big MSF (LLVM's reader rules), every stream read back \
                      into the canonical form, every hash record in its name's bucket and the reference's bucket \
                      order, every TPI hash value 7.0's hash of its record, every TPI index offset at its record, no \
                      block in use marked free. Every walk is bounded by the bytes that hold it: a malformed file is \
                      a failure, never a panic. Exit 3 on any failure. JSON rows: class, passed, failures."
    )]
    Check(CheckArgs),
    #[command(
        about = "Convert a PDB 2.0 to PDB 7.0, proved by reading it back",
        long_about = "Re-encode a VC6 PDB 2.0 as PDB 7.0 for the readers that refuse 2.0 (LLVM, modern dbghelp): \
                      the big MSF container (stream 4 left empty for the IPI, every 2.0 stream from 4 up one higher); \
                      the identity with a GUID (the 2.0 file's md5 with the RFC 4122 version-3 bits, so one 2.0 \
                      file always gives one 7.0 file); every symbol and type record re-spelled from VC6's \
                      length-prefixed names to 7.0's NUL-terminated ones, every record pointer re-pointed; the GSI \
                      hashes in the 7.0 layout; the TPI hash stream rebuilt; C11 lines as C13 subsections, one per \
                      procedure, the file names in /names (extended, never rewritten); a module with no stream \
                      given an empty one; an empty EC table where the 2.0 DBI has none. Anything the converter does \
                      not know is an error naming it. The 7.0 file is written beside OUT under a temporary name, read \
                      back and held to the 2.0 file (`pdb eq`), and renamed to OUT only when EQUIVALENT. --llvm also \
                      reads it with llvm-pdbutil (`pdb llvm`). Exit 3 when not equivalent (nothing is written)."
    )]
    To70(To70Args),
    #[command(
        about = "Hold a PDB 7.0 to the PDB 2.0 it was converted from",
        long_about = "Read both files, each in its own generation, into one canonical form (7.0 spellings, every \
                      record-to-record offset replaced by the ordinal of the record it points at) and compare: the \
                      identity and named streams, every module record, symbol record and line row, the symbol \
                      records, the globals and publics hash memberships and cRefs, the address map, the thunk and \
                      section maps, every type, the DBI's contributions, segment map, type-server map, EC names, \
                      file info and debug-header streams. On the 7.0 side alone: the hash buckets and their order, \
                      the TPI hash values and index offsets, the free block map. Exit 3 on any difference."
    )]
    Eq(EqArgs),
    #[command(
        about = "Read a PDB 7.0 with LLVM's llvm-pdbutil and hold LLVM's counts to this tool's",
        long_about = "Run `llvm-pdbutil dump -all` on the PDB 7.0 (the tool from --tool, else $MSVC6T_LLVM, else \
                      PATH) and fail on a nonzero exit, any stderr, any line in LLVM's complaint forms (`Error \
                      loading module stream`, `Stream Error`, `(unknown file name offset`, `our hash = `, `UNKNOWN \
                      RECORD`, `is corrupt`; a name holding the word is no complaint), or a count (procedures, \
                      publics, data, types, line rows, UDT names, labels, blocks, thunks) that differs from this \
                      tool's reading of the 7.0 file or of the 2.0 file it came from. With no llvm-pdbutil it says \
                      SKIP and exits 0 (--require: 1). Exit 3 on a difference."
    )]
    Llvm(LlvmArgs),
    #[command(
        about = "Give your own copy of a shipped image the debug record that names one of the image's PDBs",
        long_about = "Plan, and with --out write, the edit that makes a copy of a shipped binary name a PDB by \
                      identity: an NB10 record for a PDB 2.0, an RSDS record (GUID, age) for a PDB 7.0. The copy \
                      must be one the image's [image.pdb] `shipped` list names by md5 (anything else is refused, the \
                      accepted md5s listed); the PDB's section-headers stream must be the copy's section table; an \
                      NB10 record the copy already has must carry the PDB's signature and age (then a PDB 2.0 needs \
                      no change). The record and a debug directory go in the zero slack past the read-only data \
                      section's VirtualSize (VirtualSize raised inside SizeOfRawData: no section moves, the file \
                      does not grow); an existing directory keeps its other entries; a nonzero CheckSum is rewritten \
                      only when this tool's PE checksum reproduces the stored one. Prints every changed range; \
                      --recipe writes them as a hex-editor recipe (`-`: to stdout). The copy itself is never \
                      written. A refusal prints `REFUSED:` and exits 1."
    )]
    Attach(AttachArgs),
    #[command(
        about = "Every module procedure and thunk of a PDB with its address, type and lines",
        long_about = "List each S_GPROC32 / S_LPROC32 and S_THUNK32 of every module (2.0 or 7.0): its rva through the \
                      PDB's own section-headers stream (`-` for a record at segment 0), length, scope, type index \
                      and the TPI leaf it names, the line rows inside it, its module and name; then the record kinds \
                      of the module regions, the labels and blocks against their procedure, and the C11 blocks' \
                      consistency. --tsv writes the rows as `rva len scope typind leaf lines module name`. JSON \
                      rows: one per procedure or thunk."
    )]
    Procs(ProcsArgs),
    #[command(
        about = "Publish a PDB 2.0's source names and build-record paths by the image's policy, or judge them",
        long_about = "THE SOURCE NAMES A PDB CARRIES, by the image's [image.pdb.paths] policy: each name of the \
                      image's own projects (the C11 file records, the DBI's file info) is resolved to the PRODUCT \
                      FILE it is (a lane directory, `lanes`, to the product directory it stages, a relative name \
                      against its module's project, every component matched in any case) and published as the \
                      file's own attested spelling (`files`, per kind of record), else its directory's attested \
                      spelling (`attested`) and the product's file name, else its product-relative path. The \
                      build records (S_OBJNAME, S_COMPILE2's strings and command lines, the DBI's module names and \
                      libraries, EC names and type-server map, /LinkInfo) are published by the same rule, what the \
                      build wrote (`configs`, `Release` / `Debug`) under its project directory. A name of no lane, \
                      or one a TU's own `#line` names (read from the image's project sources), stays; a lane name \
                      that is no product file is an error. --check judges every name the PDB carries on its own \
                      (exactly the rule's spelling, never a lane's directory, an attested spelling in its own case) \
                      and scans every stream for a lane directory; exit 3 on any failure. --out writes the renamed \
                      PDB: a longer name re-lays its line region, every record pointer and reference re-pointed, \
                      each stream at its new length through the MSF."
    )]
    Paths(PathsArgs),
    #[command(
        about = "Publish the configured images' PDBs: each checked, its 7.0 twin, the attach recipes, a manifest",
        long_about = "For each --image (default: every [[image]] with an [image.pdb] file, in config order): the PDB \
                      2.0 held to the structure check and, with an [image.pdb.paths] table, its source names to the \
                      image's policy (--no-paths: not judged, said); converted to 7.0 and proved EQUIVALENT \
                      (`pdb to70`), compared with the configured twin (IDENTICAL or DIFFERS, said), and read by \
                      llvm-pdbutil (`pdb llvm`; SKIP when absent, --require-llvm: an error); then, for each \
                      `shipped` file of the image (it must be at hand and have its listed md5), the hex-editor \
                      recipe for the 2.0 and the 7.0 PDB (`pdb attach --recipe`), planned in memory: no binary is \
                      written. Nothing reaches OUT until every image passes: then OUT holds the 2.0 files, OUT/7.0 \
                      their twins, ATTACH.md (--intro's text, else the tool's, then every recipe in order) and \
                      MANIFEST.tsv (file, bytes, sha256 and one column per --rev, for the 2.0 files, the 7.0 files \
                      and ATTACH.md). The files are staged in OUT/.msvc6t-release-tmp, a hard link inside it is \
                      probed first (a filesystem that refuses one is named before anything moves), and each file \
                      goes in last by a no-replace hard link, never \
                      over a file that appears meanwhile; on an error each file they replaced is put back, and one \
                      that cannot be is kept in the temporary directory and named. OUT may not be where an image's \
                      PDB or twin is. Exit 3 on any failure."
    )]
    Release(crate::pdbrelease::ReleaseArgs),
    #[command(
        about = "Rewrite our link's PDB onto the shipped image's addresses",
        long_about = "Our link's own PDB 2.0 (VC6's LINK wrote it beside --ours) holds the publics, the module \
                      symbols with their types, the TPI and the C11 lines of the code the shipped image holds, at \
                      OUR link's addresses. The relocation translates every address field in place (a record never \
                      moves, a stream never changes length, except where a published source name grows it) through \
                      the pairing: the report's rows (reccmp's JSON report of the same build: data[] rows of address, recomp and name, and an extension object under report_key), \
                      then every datum and function they do not reach, derived by walking each paired function \
                      against the shipped copy (each operand that differs is our address beside the shipped one), \
                      the paired data's pointers, the import directories, the section contributions, unique \
                      content, a bracket of two paired neighbours and the image's own names, each candidate \
                      proved by its code or its bytes; a report row with no address of ours is grafted from the \
                      PDB's own module records by object and name and proved byte-identical. What does not \
                      translate is RETIRED (segment and offset zero) and listed. First the source names are \
                      published by [image.pdb.paths] (`pdb paths`); a TU's own `#line` gives the file record it \
                      names the TU's physical lines. Then: the symbol records and every module's records (a label, \
                      block or `with` moves with its procedure), the C11 lines (each pair alone, kept pairs \
                      re-sorted, each range recomputed), the publics address map re-sorted and its thunk map set \
                      to the shipped ILT, the segment map, the section contributions (clamped to the run that \
                      translates), the FPO stream, the section headers stream, the identity (the shipped NB10 \
                      record's signature and age, else the image's TimeDateStamp and age 1, the DBI age and the \
                      type-server map with it), and every byte no stream holds zeroed. `pdb audit` reads the result \
                      back. --sample N proves N derived data pairings against our objects' own bytes (exit 3 on \
                      a difference)."
    )]
    Relocate(crate::pdbreloc::RelocateArgs),
    #[command(
        about = "Hold every address field of a PDB to the shipped thing it names, or to its retirement",
        long_about = "Read the finished PDB 2.0 back, with a table of record kinds of its own (a kind it cannot \
                      classify fails), and match every named record against an identity oracle of the shipped \
                      image: the pairing report's rows (--report, a relocated PDB) or an expect table (--expect, an \
                      authored one), --pairs' byte-proven pairings, and, for a fold, an alias or a symbol inside \
                      another, our link (--ours and its /MAP), which must put that name at the same offset of a \
                      byte-identical body (a fold fails on an incremental link, which folds nothing). A retired \
                      field has segment and offset zero, and with --original it must be one the retired list \
                      names. A procedure must not reach the next shipped function; a label, block or `with` lies \
                      in its procedure; every C11 line lies in a procedure of its own module (with --original: at \
                      that procedure's new start plus the same offset, with its own line number, the TU's physical \
                      line under its `#line`); a contribution is its module's own code or lies in one known datum; \
                      an FPO entry carries its procedure's length; the publics address map ascends and is a \
                      permutation of the publics; the thunk map names the shipped ILT; the section map, segment \
                      map and section headers describe the shipped sections; the identity answers to the shipped \
                      NB10 record (else the image's TimeDateStamp, age 1), the DBI age with it. With --original \
                      every other byte is held to it. Always: the structure check (`pdb check`), no host path \
                      (wine's z:\\, the usual home and temporary roots, and the first directory of this host's \
                      HOME, temporary and working directories), no user profile but wine's, no login or host name of this machine, every byte no stream holds \
                      zero, and with an [image.pdb.paths] policy every source name a published one. Exit 3 on any \
                      failure."
    )]
    Audit(crate::pdbaudit::AuditArgs),
    #[command(
        about = "Write a PDB 2.0 for a binary there is no build of, from a table of knowledge about it",
        long_about = "For an image with an [image.pdb.author] table (a binary of which nothing is rebuilt): every \
                      function of the functions table (va, name, prototype) an S_GPROC32 in its module (the index \
                      table's va, size, module; a size clipped at the next code start), typed by its prototype \
                      where the types subset states it (`__noreturn` sets the flag; custom storage cannot be \
                      stated and is counted); a public for each, the library table's decorated linker name where \
                      it has one (va, kind, name, symbol); every datum of the data table (its va column, name, \
                      type, or the globals table's type at that va) a public and an S_GDATA32 of its type, T_UCHAR \
                      outside code when it has none, a public alone inside code; the rest of the library table \
                      publics; the TPI from the headers (author.types, author.headers, then each of \
                      author.optional_headers that exists). The identity is the image's NB10 record, else its \
                      TimeDateStamp and age 1. --rev (else author.rev) reads every table at a commit of \
                      author.root. Writes the PDB and its expect table (`va kind names`), which `pdb audit \
                      --expect` and `pdb dbghelp --expect` read."
    )]
    Author(crate::pdbauthor::AuthorArgs),
    #[command(
        about = "Ask a real debugger API what a PDB says about the shipped image, and score it",
        long_about = "Compile a small probe with the configured Visual C++ 6.0 (its source is part of this tool) and \
                      run it in a leased prefix against the shipped image (or --copy), with a directory holding \
                      only the PDB, under the name the image's debug record gives, as the symbol search path: \
                      SymInitialize, SymLoadModuleEx, SymGetModuleInfo64, SymFromAddr, SymGetLineFromAddr64, \
                      SymGetTypeInfo, SymSetContext. Under wine the implementation is wine's builtin dbghelp. \
                      Nothing is forced: the file is found through the image's own record, and an image with no \
                      record reports SymExport. Each queried row (--report's paired rows, or --expect's \
                      functions; --sample N spread over the image, own functions first, entry point and \
                      mid-body) is scored: named at the reference address with displacement 0, the name the \
                      row's, a function symbol, its size (one that runs past the next function is another \
                      function's record), a source line in the row's own TU near its line, the signature the \
                      PDB's types give against the mangled name's, and the frame locals. With no toolchain it \
                      says SKIP and exits 0 (--require: 1)."
    )]
    Dbghelp(crate::pdbdbg::DbghelpArgs),
    #[command(
        about = "Resolve the same rows with LLVM's llvm-symbolizer against a copy carrying an RSDS record",
        long_about = "LLVM finds a PDB only through an RSDS record inside a section, so the copy is one `pdb \
                      attach --twin --out` wrote, and the PDB the 7.0 twin it names. The copy and the PDB go alone \
                      into a scratch directory under the record's name; `llvm-symbolizer --functions=short \
                      --no-demangle --output-style=JSON` resolves each queried address (as `pdb dbghelp` picks \
                      them), and the names, files and lines are scored against the rows. With no llvm-symbolizer \
                      (--tool, else $MSVC6T_LLVM, else PATH) it says SKIP and exits 0 (--require: 1)."
    )]
    Symbolize(crate::pdbdbg::SymbolizeArgs),
}

#[derive(Debug, Args)]
pub struct PathsArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.pdb.paths] table."
    )]
    pub image: String,
    #[arg(value_name = "PDB", help = "A PDB 2.0. Default: [image.pdb] file.")]
    pub pdb: Option<PathBuf>,
    #[arg(long, help = "Judge every name the PDB carries.")]
    pub check: bool,
    #[arg(long, value_name = "FILE", help = "Write the renamed PDB to FILE.")]
    pub out: Option<PathBuf>,
    #[arg(long, default_value_t = 8, help = "Failures shown per class.")]
    pub shown: usize,
}

#[derive(Debug, Args)]
pub struct InfoArgs {
    #[arg(value_name = "PDB", required = true, help = "PDB files, 2.0 or 7.0.")]
    pub files: Vec<PathBuf>,
    #[command(flatten)]
    pub json: Json,
}

#[derive(Debug, Args)]
pub struct CheckArgs {
    #[arg(value_name = "PDB", required = true, help = "PDB files, 2.0 or 7.0.")]
    pub files: Vec<PathBuf>,
    #[arg(long, default_value_t = 8, help = "Failures shown per class.")]
    pub shown: usize,
    #[command(flatten)]
    pub json: Json,
}

#[derive(Debug, Args)]
pub struct To70Args {
    #[arg(value_name = "PDB20", help = "The PDB 2.0 to convert.")]
    pub input: PathBuf,
    #[arg(value_name = "OUT", help = "The PDB 7.0 to write.")]
    pub output: PathBuf,
    #[arg(
        long,
        help = "Also read the written file with llvm-pdbutil (`pdb llvm`)."
    )]
    pub llvm: bool,
    #[arg(long, value_name = "DIR", help = "LLVM's bin directory, for --llvm.")]
    pub tool: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct EqArgs {
    #[arg(value_name = "PDB20", help = "The PDB 2.0.")]
    pub a: PathBuf,
    #[arg(value_name = "PDB70", help = "Its PDB 7.0 twin.")]
    pub b: PathBuf,
    #[command(flatten)]
    pub json: Json,
}

#[derive(Debug, Args)]
pub struct LlvmArgs {
    #[arg(
        value_name = "PDB20",
        help = "The PDB 2.0 the 7.0 file was converted from."
    )]
    pub a: PathBuf,
    #[arg(value_name = "PDB70", help = "The PDB 7.0 LLVM reads.")]
    pub b: PathBuf,
    #[arg(
        long,
        value_name = "DIR",
        help = "LLVM's bin directory. Default: $MSVC6T_LLVM, else PATH."
    )]
    pub tool: Option<PathBuf>,
    #[arg(long, help = "A missing llvm-pdbutil is an error, not a SKIP.")]
    pub require: bool,
}

#[derive(Debug, Args)]
pub struct AttachArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.pdb] table."
    )]
    pub image: String,
    #[arg(
        value_name = "COPY",
        help = "Your own copy of the shipped binary. It is only read."
    )]
    pub copy: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        conflicts_with = "twin",
        help = "The PDB to name. Default: [image.pdb] file."
    )]
    pub pdb: Option<PathBuf>,
    #[arg(long, help = "Name [image.pdb] twin, the PDB 7.0.")]
    pub twin: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the attached copy to FILE (never over COPY)."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the hex-editor recipe to FILE (`-`: stdout)."
    )]
    pub recipe: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct ProcsArgs {
    #[arg(value_name = "PDB", help = "A PDB, 2.0 or 7.0.")]
    pub file: PathBuf,
    #[arg(long, value_name = "FILE", help = "Write the rows as TSV.")]
    pub tsv: Option<PathBuf>,
    #[command(flatten)]
    pub json: Json,
}

fn write_json<S: Serialize, T: Serialize>(
    j: &Json,
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

pub(crate) fn read(path: &Path) -> Result<Vec<u8>> {
    std::fs::read(path).with_context(|| format!("reading {}", path.display()))
}

pub(crate) fn label(path: &Path) -> String {
    path.file_name().map_or_else(
        || path.display().to_string(),
        |n| n.to_string_lossy().into_owned(),
    )
}

pub fn run(config: Option<&Path>, a: &PdbArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    match &a.verb {
        PdbVerb::Info(x) => info_verb(config, x),
        PdbVerb::Check(x) => check_verb(x),
        PdbVerb::To70(x) => to70_verb(x),
        PdbVerb::Eq(x) => eq_verb(x),
        PdbVerb::Llvm(x) => llvm_verb(x),
        PdbVerb::Attach(x) => attach_verb(config, x),
        PdbVerb::Procs(x) => procs_verb(x),
        PdbVerb::Paths(x) => paths_verb(config, x),
        PdbVerb::Release(x) => crate::pdbrelease::run(config, x),
        PdbVerb::Relocate(x) => crate::pdbreloc::relocate(config, x),
        PdbVerb::Audit(x) => crate::pdbaudit::audit(config, x),
        PdbVerb::Author(x) => crate::pdbauthor::author(config, x),
        PdbVerb::Dbghelp(x) => crate::pdbdbg::dbghelp_verb(config, x, guard),
        PdbVerb::Symbolize(x) => crate::pdbdbg::symbolize_verb(config, x),
    }
}

fn line_names(c: &Config, n: usize) -> Result<std::collections::BTreeMap<String, Vec<String>>> {
    let mut out: std::collections::BTreeMap<String, Vec<String>> =
        std::collections::BTreeMap::new();
    let Some(d) = c.images.get(n) else {
        return Ok(out);
    };
    if d.project.is_none() && d.tu.is_empty() {
        return Ok(out);
    }
    let specs = msvc6_project::spec::resolve(c, n)?;
    for s in &specs.specs {
        let Ok(text) = srcio::read(&s.path) else {
            continue;
        };
        let names: Vec<String> = text
            .lines()
            .filter_map(msvc6_pdb::paths::line_directive)
            .collect();
        if !names.is_empty() {
            out.entry(format!("{}.obj", s.stem).to_ascii_lowercase())
                .or_default()
                .extend(names);
        }
    }
    Ok(out)
}

pub fn policy_of(c: &Config, image: &str) -> Result<(usize, msvc6_pdb::paths::Policy)> {
    let (n, d) = msvc6_project::registry::resolve_id(c, image)?;
    let key = format!("{}.pdb.paths", c.image_key(n));
    let spec = d
        .pdb
        .as_ref()
        .and_then(|p| p.paths.as_ref())
        .with_context(|| format!("image `{}` has no [image.pdb.paths] table", d.id))?;
    let product = c.path_at(&format!("{key}.product"), &spec.product)?;
    let names = line_names(c, n)?;
    let policy = msvc6_pdb::paths::Policy::new(spec, &product, names)
        .map_err(|e| c.error(&key, format!("{e:#}")))?;
    Ok((n, policy))
}

fn paths_verb(config: Option<&Path>, a: &PathsArgs) -> Result<Outcome> {
    let c = Config::require(config)?;
    let (n, policy) = policy_of(&c, &a.image)?;
    let pdb = match &a.pdb {
        Some(p) => p.clone(),
        None => {
            let d = c.images.get(n).context("image")?;
            let f = d
                .pdb
                .as_ref()
                .and_then(|p| p.file.as_ref())
                .context("[image.pdb] has no `file`; give the PDB")?;
            c.path_at(&format!("{}.pdb.file", c.image_key(n)), f)?
        }
    };
    let bytes = read(&pdb)?;
    if !a.check && a.out.is_none() {
        bail!("give --check, --out FILE, or both");
    }
    let mut outcome = Outcome::Ok;
    if a.check {
        let j = msvc6_pdb::paths::check(&bytes, &policy)?;
        println!(
            "{}: source names by image {}'s policy",
            pdb.display(),
            a.image
        );
        println!("  {:<44} {:>7} {:>7}", "class", "ok", "FAILED");
        for (class, (ok, fails)) in &j.rows {
            println!("  {class:<44} {ok:>7} {:>7}", fails.len());
        }
        for (class, (_, fails)) in &j.rows {
            for e in fails.iter().take(a.shown) {
                println!("    FAIL {class}: {e}");
            }
            if fails.len() > a.shown {
                println!("    FAIL {class}: ... and {} more", fails.len() - a.shown);
            }
        }
        println!(
            "  {}: {} failure(s)",
            if j.failures() == 0 { "PASS" } else { "FAIL" },
            j.failures()
        );
        if j.failures() > 0 {
            outcome = Outcome::Differs;
        }
    }
    if let Some(out) = &a.out {
        let mut db = msf::Msf2::parse(bytes)?;
        let score = msvc6_pdb::paths::rename(&mut db, &policy)?;
        for l in score.lines(&policy) {
            println!("{l}");
        }
        srcio::own_write(out, db.bytes()).with_context(|| format!("writing {}", out.display()))?;
        println!(
            "  wrote {}: {} bytes, sha256 {}",
            out.display(),
            db.bytes().len(),
            sha256_hex(db.bytes())
        );
    }
    Ok(outcome)
}

#[derive(Debug, Serialize)]
struct InfoRow {
    file: String,
    generation: &'static str,
    bytes: usize,
    page_size: usize,
    pages: usize,
    streams: usize,
    version: u32,
    signature: u32,
    age: u32,
    guid: Option<String>,
    modules: usize,
    types: usize,
    image: Option<String>,
}

fn info_verb(config: Option<&Path>, a: &InfoArgs) -> Result<Outcome> {
    let cfg = project::load(config)?;
    let mut records = Vec::new();
    if let Some(c) = &cfg {
        for n in 0..c.images.len() {
            let Ok(r) = c.reference(n) else { continue };
            if let Ok(img) = msvc6_image::pe::Image::parse(r.bytes)
                && let Ok(Some(cv)) = img.codeview()
            {
                records.push((r.label, cv));
            }
        }
    }
    let mut rows = Vec::new();
    for f in &a.files {
        let b = read(f)?;
        let (generation, page_size, pages, streams, s1, s2, s3) = if msf7::is_pdb70(&b) {
            let m = msf7::Msf7::parse(b.clone()).with_context(|| f.display().to_string())?;
            (
                "7.0",
                m.block_size,
                m.num_blocks,
                m.streams.len(),
                m.stream(1)?,
                m.stream(2)?,
                m.stream(3)?,
            )
        } else {
            let m = msf::Msf2::parse(b.clone()).with_context(|| f.display().to_string())?;
            (
                "2.0",
                m.page_size,
                m.pages_used,
                m.streams.len(),
                m.stream(1)?,
                m.stream(2)?,
                m.stream(3)?,
            )
        };
        let id = info::read_info(&s1).with_context(|| format!("{}: stream 1", f.display()))?;
        let modules = if s3.is_empty() {
            0
        } else {
            msvc6_pdb::dbi::Dbi::parse(&s3)
                .with_context(|| format!("{}: the DBI", f.display()))?
                .mods
                .len()
        };
        let types = msvc6_pdb::tpi::read(&s2).map_or(0, |t| t.records.len());
        let guid_hex = id
            .guid
            .map(|g| g.iter().map(|x| format!("{x:02x}")).collect::<String>());
        let image = records
            .iter()
            .find(|(_, cv)| match (&id.guid, cv.format.as_str()) {
                (Some(_), "RSDS") => cv.guid == guid_hex && cv.age == id.age,
                (None, "NB10") => cv.signature == id.signature && cv.age == id.age,
                _ => false,
            })
            .map(|(l, _)| l.clone());
        println!("{}", f.display());
        println!(
            "  PDB {generation}: {} bytes = {pages} page(s) of {page_size}, {streams} stream(s)",
            b.len()
        );
        println!(
            "  interface version {}{}, signature {:#010x}, age {}{}",
            id.version,
            if id.version == info::VC60 {
                " (VC6)"
            } else if id.version == info::VC70 {
                " (7.0)"
            } else {
                ""
            },
            id.signature,
            id.age,
            id.guid
                .map_or_else(String::new, |g| format!(", GUID {}", info::guid_text(&g)))
        );
        if modules == 0 {
            println!(
                "  DBI: no module: a library's or a compiler's PDB, types only; {types} type(s)"
            );
        } else {
            println!("  DBI: {modules} module(s); {types} type(s)");
        }
        match &image {
            Some(l) => {
                println!("  MATCHES image {l}: its CodeView record names this PDB by identity")
            }
            None => println!("  no configured image's CodeView record names this PDB"),
        }
        rows.push(InfoRow {
            file: f.display().to_string(),
            generation,
            bytes: b.len(),
            page_size,
            pages,
            streams,
            version: id.version,
            signature: id.signature,
            age: id.age,
            guid: id.guid.map(|g| info::guid_text(&g)),
            modules,
            types,
            image,
        });
    }
    write_json::<(), _>(&a.json, "pdb info", "", None, &rows)?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct CheckRow {
    file: String,
    class: String,
    passed: usize,
    failures: Vec<String>,
}

fn check70(b: &[u8]) -> Vec<(String, usize, Vec<String>)> {
    let mut out = Vec::new();
    let m = match msf7::Msf7::parse(b.to_vec()) {
        Ok(m) => m,
        Err(e) => {
            out.push(("7.0: MSF container".into(), 0, vec![format!("{e:#}")]));
            return out;
        }
    };
    out.push(("7.0: MSF container".into(), 1, Vec::new()));
    match m.used_but_free() {
        Ok((n, 0)) => out.push(("7.0: free block map".into(), n, Vec::new())),
        Ok((n, bad)) => out.push((
            "7.0: free block map".into(),
            n - bad,
            vec![format!("{bad} block(s) in use are marked free")],
        )),
        Err(e) => out.push(("7.0: free block map".into(), 0, vec![format!("{e:#}")])),
    }
    match eq::equivalence(b, b) {
        Ok(v) => {
            let pair = |what: &str, (n, ok): (usize, usize)| {
                (
                    format!("7.0: {what}"),
                    ok,
                    if ok == n {
                        Vec::new()
                    } else {
                        vec![format!("{} of {n} do not", n - ok)]
                    },
                )
            };
            out.push(pair(
                "hash record in its name's bucket",
                (v.gsi_checked.0, v.gsi_checked.1),
            ));
            out.push(pair(
                "hash bucket in the reference's order",
                (v.gsi_checked.0, v.gsi_checked.2),
            ));
            out.push(pair("TPI hash value of its record", v.tpi_hashes));
            out.push(pair("TPI index offset at its record", v.tpi_offsets));
        }
        Err(e) => out.push(("7.0: streams read back".into(), 0, vec![format!("{e:#}")])),
    }
    out
}

fn check_verb(a: &CheckArgs) -> Result<Outcome> {
    let mut rows = Vec::new();
    let mut bad = 0usize;
    for f in &a.files {
        let b = read(f)?;
        let classes: Vec<(String, usize, Vec<String>)> = if msf7::is_pdb70(&b) {
            println!("{}\n  PDB 7.0", f.display());
            check70(&b)
        } else {
            let r = structure::check(&b);
            println!("{}\n  PDB 2.0: {}", f.display(), r.summary());
            r.rows()
                .map(|(c, n, fs)| (c.to_string(), n, fs.to_vec()))
                .collect()
        };
        println!("  {:<44} {:>7} {:>7}", "class", "ok", "FAILED");
        for (c, n, fs) in &classes {
            println!("  {c:<44} {n:>7} {:>7}", fs.len());
        }
        for (c, _, fs) in &classes {
            for e in fs.iter().take(a.shown) {
                println!("    FAIL {c}: {e}");
            }
            if fs.len() > a.shown {
                println!("    FAIL {c}: ... and {} more", fs.len() - a.shown);
            }
        }
        let n: usize = classes.iter().map(|c| c.2.len()).sum();
        println!(
            "  {}: {n} structural failure(s)",
            if n == 0 { "PASS" } else { "FAIL" }
        );
        bad += n;
        rows.extend(
            classes
                .into_iter()
                .map(|(class, passed, failures)| CheckRow {
                    file: f.display().to_string(),
                    class,
                    passed,
                    failures,
                }),
        );
    }
    write_json::<(), _>(&a.json, "pdb check", "", None, &rows)?;
    println!(
        "check: {} file(s), {bad} structural failure(s)",
        a.files.len()
    );
    Ok(if bad == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn kinds(m: &std::collections::BTreeMap<u16, usize>) -> String {
    m.iter()
        .map(|(k, n)| format!("{} {n}", msvc6_codeview::codec::sym_name(*k)))
        .collect::<Vec<_>>()
        .join(", ")
}

fn to70_verb(a: &To70Args) -> Result<Outcome> {
    let raw = read(&a.input)?;
    let cv = to70::convert(&raw).with_context(|| format!("converting {}", a.input.display()))?;
    let name = a
        .output
        .file_name()
        .context("the output path has no file name")?
        .to_string_lossy()
        .into_owned();
    if let Some(d) = a.output.parent().filter(|d| !d.as_os_str().is_empty()) {
        std::fs::create_dir_all(d).with_context(|| format!("creating {}", d.display()))?;
    }
    let tmp = a.output.with_file_name(format!(".{name}.to70-tmp"));
    srcio::own_write(&tmp, &cv.bytes).with_context(|| format!("writing {}", tmp.display()))?;
    println!(
        "{} -> {}\n  MSF 7.00, {} bytes, {} streams (from {}); signature {:#010x} age {} GUID {}",
        a.input.display(),
        a.output.display(),
        cv.bytes.len(),
        cv.streams_out,
        cv.streams_in,
        cv.signature,
        cv.age,
        info::guid_text(&cv.guid)
    );
    println!(
        "  modules {} ({} without a stream given an empty one); module symbols: {}",
        cv.modules,
        cv.empty_modules,
        kinds(&cv.module_syms.by_kind)
    );
    println!("  symbol records: {}", kinds(&cv.global_syms.by_kind));
    println!(
        "  thunk ordinals {:?}; S_COMPILE2 strings {} (non-zero pads {}); record pointers re-pointed {}, references {}; module global refs {}",
        cv.module_syms.thunk_ords,
        cv.module_syms.compile2_strings,
        cv.module_syms.dirty_pads,
        cv.repointed,
        cv.refs_repointed,
        cv.global_refs
    );
    println!(
        "  globals hash {} records, publics {}; TPI {} -> V80, {} records; lines: {} C11 rows -> C13, {} inside a procedure, {} outside every one, {} LINES subsections",
        cv.globals,
        cv.publics,
        cv.tpi_version_in,
        cv.types.by_leaf.values().sum::<usize>(),
        cv.lines.rows,
        cv.lines.in_procs,
        cv.lines.orphans,
        cv.lines.subsections
    );
    for (i, what, n) in &cv.copied {
        println!("  stream {i:3} copied: {what} ({n} bytes)");
    }
    if cv.empty_ec {
        println!("  no edit-and-continue name table in the 2.0 DBI: the empty one written");
    }
    let back = read(&tmp)?;
    let v = match eq::equivalence(&raw, &back) {
        Ok(v) => v,
        Err(e) => {
            let _ = std::fs::remove_file(&tmp);
            return Err(e.context(format!(
                "{} NOT written: reading it back failed",
                a.output.display()
            )));
        }
    };
    for l in v.lines(
        &a.input.display().to_string(),
        &a.output.display().to_string(),
    ) {
        println!("  {l}");
    }
    if !v.equivalent() {
        let _ = std::fs::remove_file(&tmp);
        println!(
            "  {} NOT written: the round-trip check failed",
            a.output.display()
        );
        return Ok(Outcome::Differs);
    }
    std::fs::rename(&tmp, &a.output)
        .with_context(|| format!("renaming {} to {}", tmp.display(), a.output.display()))?;
    println!(
        "  wrote {}: {} bytes, sha256 {}",
        a.output.display(),
        cv.bytes.len(),
        sha256_hex(&cv.bytes)
    );
    if a.llvm {
        return llvm_verb(&LlvmArgs {
            a: a.input.clone(),
            b: a.output.clone(),
            tool: a.tool.clone(),
            require: false,
        });
    }
    Ok(Outcome::Ok)
}

fn eq_verb(a: &EqArgs) -> Result<Outcome> {
    let v = eq::equivalence(&read(&a.a)?, &read(&a.b)?)?;
    for l in v.lines(&a.a.display().to_string(), &a.b.display().to_string()) {
        println!("{l}");
    }
    write_json(&a.json, "pdb eq", "", Some(&v), &v.differences)?;
    Ok(if v.equivalent() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn llvm_verb(a: &LlvmArgs) -> Result<Outcome> {
    let Some(tool) = llvm::find(a.tool.as_deref()) else {
        println!(
            "SKIP llvm: no {} in {} or on PATH: LLVM's reading of {} was NOT checked",
            llvm::TOOL,
            a.tool
                .as_ref()
                .map_or_else(|| format!("${}", llvm::ENV), |d| d.display().to_string()),
            a.b.display()
        );
        if a.require {
            bail!("--require and no {}", llvm::TOOL);
        }
        return Ok(Outcome::Ok);
    };
    let r = llvm::read(&tool, &a.b)?;
    let ours20 = eq::counts_of(&read(&a.a)?)?;
    let ours70 = eq::counts_of(&read(&a.b)?)?;
    println!(
        "{}: llvm-pdbutil dump -all exit {}, {} stderr line(s), {} error line(s), {} type reference(s) LLVM cannot resolve",
        a.b.display(),
        r.exit.map_or_else(|| "none".to_string(), |c| c.to_string()),
        r.stderr.len(),
        r.errors.len(),
        r.unresolved
    );
    let row = |n: &str, c: &eq::Counts| {
        println!(
            "  {n:<20} procs {:>5} | publics {:>5} | data {:>5} | types {:>5} | line rows {:>6} | UDT names {:>5} | labels {:>5} | blocks {:>4} | thunks {:>4}",
            c.procs, c.publics, c.data, c.types, c.lines, c.udts, c.labels, c.blocks, c.thunks
        );
    };
    row("2.0, this tool", &ours20);
    row("7.0, this tool", &ours70);
    row("7.0, llvm-pdbutil", &r.counts);
    for e in r.errors.iter().take(20) {
        println!("  ERROR LINE {e}");
    }
    for l in r.stderr.iter().take(10) {
        println!("  STDERR {l}");
    }
    if !r.clean() {
        println!(
            "  FAIL: llvm-pdbutil does not read {} cleanly",
            a.b.display()
        );
        return Ok(Outcome::Differs);
    }
    if ours20 != ours70 || ours70 != r.counts {
        println!("  FAIL: the counts differ");
        return Ok(Outcome::Differs);
    }
    println!("  SAME: LLVM reads every count this tool reads, in both files");
    Ok(Outcome::Ok)
}

fn attach_verb(config: Option<&Path>, a: &AttachArgs) -> Result<Outcome> {
    let c = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&c, &a.image)?;
    let key = format!("{}.pdb", c.image_key(n));
    let spec = d
        .pdb
        .as_ref()
        .with_context(|| format!("image `{}` has no [image.pdb] table", d.id))?;
    let pdb = match (&a.pdb, a.twin) {
        (Some(p), _) => p.clone(),
        (None, true) => c.path_at(
            &format!("{key}.twin"),
            spec.twin.as_ref().context("[image.pdb] has no `twin`")?,
        )?,
        (None, false) => c.path_at(
            &format!("{key}.file"),
            spec.file.as_ref().context("[image.pdb] has no `file`")?,
        )?,
    };
    let file = spec.name.clone().unwrap_or_else(|| label(&pdb));
    let copy = read(&a.copy)?;
    let p = read(&pdb)?;
    let r = match attach::attach(&copy, &spec.shipped, &p, &file) {
        Ok(r) => r,
        Err(e) => {
            println!(
                "REFUSED: {} ({}) for image {}: {e:#}",
                a.copy.display(),
                label(&a.copy),
                d.id
            );
            return Ok(Outcome::Exit(ExitCode::from(1)));
        }
    };
    let ident = r.id.ident();
    if r.changes.is_empty() {
        println!(
            "{} ({}, md5 {}) already names {file} ({ident}) by its NB10 record: nothing to change",
            a.copy.display(),
            r.shipped.what,
            r.md5_in
        );
    } else {
        let total: usize = r.changes.iter().map(|c| c.new.len()).sum();
        println!(
            "{} ({}, md5 {}) names {file} by {ident} once these {total} byte(s) over {} range(s) are written, {} of them changed:",
            a.copy.display(),
            r.shipped.what,
            r.md5_in,
            r.changes.len(),
            r.differing()
        );
        for ch in &r.changes {
            println!(
                "  file {:#08x}..{:#08x}  {:<40} {} -> {}",
                ch.at,
                ch.at + ch.new.len(),
                ch.what,
                attach::short_hex(&ch.old),
                attach::short_hex(&ch.new)
            );
        }
        println!("  md5 of the attached copy: {}", r.md5_out);
    }
    if let Some(out) = &a.out {
        let same = std::fs::canonicalize(out)
            .ok()
            .zip(std::fs::canonicalize(&a.copy).ok())
            .is_some_and(|(x, y)| x == y);
        if same {
            bail!(
                "--out {} is the copy itself; write the attached copy elsewhere",
                out.display()
            );
        }
        srcio::own_write(out, &r.bytes).with_context(|| format!("writing {}", out.display()))?;
        println!("  wrote {} (md5 {})", out.display(), md5_hex(&r.bytes));
    }
    match &a.recipe {
        Some(p) if p.as_os_str() == "-" => print!("{}", r.recipe()),
        Some(p) => {
            srcio::own_write(p, r.recipe()).with_context(|| format!("writing {}", p.display()))?;
            println!("  the recipe: {}", p.display());
        }
        None => {}
    }
    Ok(Outcome::Ok)
}

fn procs_verb(a: &ProcsArgs) -> Result<Outcome> {
    let l = procs::read(&read(&a.file)?).with_context(|| a.file.display().to_string())?;
    let live = l.procs.iter().filter(|p| p.rva.is_some()).count();
    println!(
        "{}: {} module procedure record(s), {live} at an address ({} retired); {} with a procedure type, {} with line rows; {} thunk record(s), {} at an address",
        a.file.display(),
        l.procs.len(),
        l.procs.len() - live,
        l.procs
            .iter()
            .filter(|p| p.rva.is_some() && p.typed())
            .count(),
        l.procs.iter().filter(|p| p.lines > 0).count(),
        l.thunks.len(),
        l.thunks.iter().filter(|t| t.rva.is_some()).count()
    );
    let c = &l.children;
    println!(
        "  labels and blocks: {} with an address inside a procedure that has one, {} retired with theirs; {} retired inside a procedure with an address, {} with an address inside a retired one; {} outside every procedure's scope, {} of those retired",
        c.live, c.retired, c.retired_in_live, c.live_in_retired, c.outside, c.outside_retired
    );
    println!("  record kinds in the module regions:");
    for (k, n) in &l.kinds {
        println!(
            "    {k:#06x}  {:<14} {n}",
            msvc6_codeview::codec::sym_name(*k)
        );
    }
    let s = &l.lines;
    println!(
        "  C11 line blocks: {} holding {} pair(s); {} with pairs out of offset order, {} pair(s) outside their block's range; range end = the end of its procedure minus 1: {}, exactly: {}, neither: {}",
        s.blocks, s.pairs, s.unsorted, s.outside, s.end_inclusive, s.end_exclusive, s.end_other
    );
    if let Some(t) = &a.tsv {
        srcio::own_write(t, procs::tsv(&l)).with_context(|| format!("writing {}", t.display()))?;
        println!("  wrote {}", t.display());
    }
    let rows: Vec<&procs::Proc> = l.procs.iter().chain(&l.thunks).collect();
    write_json::<(), _>(&a.json, "pdb procs", &label(&a.file), None, &rows)?;
    Ok(Outcome::Ok)
}
