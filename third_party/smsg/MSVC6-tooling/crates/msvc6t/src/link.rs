use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::{self, Config};
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_diff::env::Env;
use msvc6_diff::resolve::Names;
use msvc6_image::archive::Archive;
use msvc6_image::pe::Image;
use msvc6_link::iat::{self, Control, Pull};
use msvc6_link::icf::{self, Definer, Dump, Entry, Inputs, LoadAt, Model, Move};
use msvc6_link::icfdiff::{self, Link, Renames, Residue};
use msvc6_link::objkey::Keys;
use msvc6_link::owners::{RefOwners, objects_of};
use msvc6_link::qsort::Crt;
use msvc6_link::verbose::{self, read_capped};
use msvc6_link::{folds, ilt, iltdiff, include, joint, linkmap, predict, reserve, runs};
use msvc6_project::dsp::Dsp;
use msvc6_project::dsw::Dsw;
use msvc6_project::names::{Kind as NameKind, Name, NameOracle};
use serde::Serialize;
use std::collections::{BTreeMap, HashMap};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

pub const MAX_OBJECT: u64 = 256 << 20;

pub const MAX_ARCHIVE: u64 = 1 << 30;

#[derive(Debug, Args)]
pub struct LinkArgs {
    #[command(subcommand)]
    pub verb: LinkVerb,
}

#[derive(Debug, Subcommand)]
pub enum LinkVerb {
    #[command(
        about = "Every identical-COMDAT fold a link made, against the folds the reference made",
        long_about = "Read a LINK /VERBOSE log's fold narration (each `Selected symbol:` block, its survivor and its \
                      `Replaced symbol(s):`, each with the object LINK took it from) and the link's /MAP, and judge \
                      every fold group against IMAGE, the reference: each member's reference address comes from the \
                      image's name oracle (exports first, then each provider, an ILT slot followed to its body). A \
                      group whose members the reference holds at two or more addresses is one the reference kept \
                      apart (SPLIT); a name another object of ours defines at a group's reference address is one \
                      the reference folded there (REF-FOLDS); a group none of whose members the names place is \
                      UNPLACED; otherwise the reference's object at the address (read off the objects of the nearest \
                      resolved names on either side) is the survivor's (SAME) or not (SURVIVOR-DIFFERS). Each \
                      member's own contribution is read from its object or archive member (the log names it) for \
                      the properties LINK's fold compare keys on: characteristics, COMDAT selection, size, \
                      relocation count. For a group without relocations every recorded function of the image whose \
                      bytes, trailing fill trimmed, equal the survivor's is listed as a reference copy of the body. \
                      The census counts the misfolded names: members ours folds that the reference keeps apart, and \
                      names the reference folds that ours keeps apart. An own library's member (--own) reads as the \
                      member itself in either spelling LINK prints. JSON rows: one per group."
    )]
    Icf(IcfArgs),
    #[command(
        about = "LINK 6.00's /OPT:ICF survivors replayed from the objects, with what-ifs",
        long_about = "Replay LINK 6.00's identical-COMDAT folding from the objects instead of reading it off the log. \
                      The candidate array is every COMDAT LINK accepts, in its append order: the command-line \
                      objects in their order (--obj, --order FILE, else the image project's translation units in \
                      .dsp order), then every library member the /VERBOSE log narrates `Loaded lib(member)`, each \
                      object's sections in section-index order; an ANY name an earlier object defined is not \
                      accepted, a STATIC COMDAT always is (its selection compares as 0), an ASSOCIATIVE one goes with \
                      its parent; a contribution is an element when it has bytes, MEM_EXECUTE, a name not starting \
                      `_$E`, and is not writable unless it is CNT_CODE. A long-format import member's thunk COMDAT \
                      is an element where it loads (found through the archive's symbol table); a short import \
                      member's thunk is LINK's own and is not. /OPT:REF's dead COMDATs (the log's `Discarded` lines) \
                      stay in the sort and never fold. The array is sorted with VC6's QSORT.C (the native \
                      msvcrt's; --model wine sorts with wine's builtin) on (size, relocation count), and each run of \
                      equal keys is walked in sorted order: an unfolded member absorbs every later one with equal \
                      characteristics, selection, aux checksum, bytes and relocation targets, to a fixed point. So a \
                      group's survivor is its earliest member in sorted order. Each group the log narrates is \
                      printed with ORDER (the log's members, the predicted survivor the one sorted first) and \
                      REPLAY (the replay's own fold, with what its class holds beyond the log's group), then PAIRS \
                      (every consecutive pair the printer walks in sorted order that the replay sorts the same way). \
                      SCORE judges each reference fold target's predicted survivor against the object the reference \
                      holds there (ok, EDGE at an object boundary, MISS, ?). What-ifs re-run the replay: --move a \
                      COMDAT before another of its object, --load a member at another point of the link order, \
                      --drop a definer's COMDATs, --define-in another object; with one, SCORE also shows the base \
                      link. --want NAME lists the single moves that would make NAME survive; --search-insert \
                      KEYS tries one dead contribution of each key at every position; --candidates FILE and \
                      --candidates-from DIR run the joint search over attested dead COMDATs (rows of OBJECT, BEFORE, KEYS, NAME and EVIDENCE). \
                      Controls a correct model must lose to: --model externals-only, dead-out, dead-null, dup-in, \
                      sym-order, thunks, stable, reverse. Exit 0 when every log group's survivor is the predicted \
                      one, 3 when one is not or a member is outside the array, 1 when an object cannot be read. JSON \
                      rows: one per group, the totals as the summary."
    )]
    IcfPredict(Box<IcfPredictArgs>),
    #[command(
        about = "Prove two full links differ only by ICF survivors and the layout they move",
        long_about = "Compare BASE and NEW, two full links (non-incremental) of one image before and after a change \
                      that should move nothing but LINK's /OPT:ICF fold survivors and the layout that follows, byte \
                      by byte with both /MAPs and /VERBOSE logs. Bodies are read off each map (every code symbol \
                      starts one, which runs to the next or its section's end; a contribution with no symbol at its \
                      start is one body) and paired: by name (a name a fold group narrates by name alone, any other \
                      by name and object; a name C1 numbers, `_$E12`, never by name), by the --rename map (OLD=NEW \
                      names or objects, applied to BASE), a static of a `.text` fold group across the group's \
                      objects, a run of bodies no name pairs by position between paired neighbours object by object, \
                      and last a body each side holds alone whose content, every address field zeroed, is the same \
                      (--no-content turns that off). Every differing byte gets an owner: MOVED-BODY (the body's own \
                      byte), ADDRESS-FIELD (a decoded rel32 / rel8 / disp32 / imm32 or jump-table entry whose BASE \
                      target translated through the pairing is its NEW target; a data dword or export rva naming \
                      moved code likewise), PAD (fill or a declared text-pad residue row of --image past a body's \
                      content), SECTION-TAIL, HEADER, or UNCLASSIFIED. Beyond the differing bytes: every field of \
                      every pair must translate (MISROUTED), every body equal itself beyond its fields (CHANGED), \
                      every pair but the moved survivors keep its order (ORDER), every data dword naming a moved body \
                      follow it (STALE), every data symbol keep its address. Exit 0 when it is placement only, 3 on \
                      findings. JSON summary: the report."
    )]
    IcfDiff(IcfDiffArgs),
    #[command(
        about = "The IAT order LINK 6.00 writes, predicted from its /VERBOSE log, and its inversion",
        long_about = "LINK 6.00 sorts the `.idata$5` (IAT) and `.idata$4` contributions with its host CRT's qsort, \
                      keyed by the archive member name of each import (`KERNEL32.dll`), over every import member and \
                      null thunk loaded in pass 1, in load order, across all DLLs at once: imports /OPT:REF later \
                      discards hold a place in the sort and none in the image. So each DLL's block is the qsort's \
                      permutation of the pull order, and the blocks come in strcmp order of the member names. LOG is \
                      LINK's /VERBOSE output (`Found`, `Referenced in`, `Loaded lib(member)`; the nested `.exp` pass \
                      is skipped). The prediction is compared slot by slot with IMAGE's IAT (and --ours'), with \
                      `.idata$6`, which is not sorted and so is the load order; then the reference's own `.idata$6` \
                      population (its live imports and DLL names, an orphan DLL name a null thunk at its place) is \
                      sorted with no log at all. --invert prints the pull order that sorts into the reference's \
                      IAT (the key sequence held) and the reference's load order against ours. --edit / --ref-edit \
                      apply a pull-edit script (`move|add D!N after|before D!N`, `drop D!N`) to the log's or the \
                      reference's population; --phantom-search N adds discarded imports greedily; --run-search SPEC \
                      searches the discarded counts per library sweep (RUN=A..B, +P[..Q]:DLL=A..B[/C..D]). \
                      --crt wine sorts with wine's builtin qsort; --control reverse and no-null are controls the \
                      model must beat. Exit 0 when the prediction reproduces the reference's IAT slot for slot, 3 \
                      when it does not. JSON rows: one per DLL block against the reference."
    )]
    Iat(Box<IatArgs>),
    #[command(
        about = "Compare an incremental link's thunk table order with the reference's",
        long_about = "LINK 6 lays an /INCREMENTAL image's incremental-link thunk table (ILT) at the start of .text, \
                      one `e9 rel32` per function after any int3 pad; its order is LINK's own function of the \
                      decorated names (no hash fits it), so this only compares two links. A reference slot is named \
                      by the export table (an export rva is its thunk) and, unless --exports-only, by the image's \
                      name rows (every provider but exports and imports, unscoped rows): a row at a thunk names that \
                      slot, a row at a thunk's body names it when it is the body's only row. A candidate slot is \
                      named by its exports and its /MAP's code publics at the body (--map, else CANDIDATE.map or the \
                      candidate with a .map extension). Over the names both sides name, LIS is the longest run that \
                      keeps the reference's order; a name off it is MISPLACED; inversions count the pairs the orders \
                      disagree on. Exit 0 when nothing is misplaced, 3 otherwise. JSON rows: the misplaced names."
    )]
    IltOrder(IltOrderArgs),
    #[command(
        about = "Prove two incremental links differ only by ILT thunk seats",
        long_about = "Compare BASE and NEW, two /INCREMENTAL links of one image before and after a change that should \
                      move nothing but the ILT's thunk seats (a rename, a parameter type that changes a decorated \
                      name). Every differing byte belongs to the thunk table or to one pointer field into it: a \
                      rel32 at a decoded instruction boundary (each body decoded from its ILT target to the next \
                      body), an absolute dword the image's own .reloc names, an export address; anything else is \
                      UNCLASSIFIED. Every pointer field of either image is followed through its own ILT and must \
                      reach the same body in both. --shipped REF also follows every relocated .rdata dword and export \
                      of NEW into its ILT and compares the body with the shipped image's at the same rva, counting \
                      only a disagreement BASE did not have. Exit 0 when it is placement only, 3 on findings."
    )]
    IltDiff(IltDiffArgs),
    #[command(
        about = "The incremental link's growth reserve after each import block, and its fossils",
        long_about = "An incremental LINK 6.00 leaves a zero-filled growth reserve after each DLL's ILT (.idata$4) \
                      and IAT (.idata$5) block, of 40 + 4 * floor(imports / 5) bytes (measured on every block of \
                      three /INCREMENTAL DLLs). A block whose reserve disagrees with its own import count is a FOSSIL \
                      of an earlier generation, which froze the block when the DLL had the import count the \
                      reserve implies (printed). A non-incremental image has no reserve after any block, and the \
                      rule says nothing about it. --candidate reads our linked image instead. Exit 3 when a block is \
                      a FOSSIL. JSON rows: one per block."
    )]
    Reserve(ReserveArgs),
    #[command(
        about = "The /INCLUDE: switches whose order makes LINK write the reference's IAT",
        long_about = "A `/INCLUDE:__imp__Name@N` switch is a first reference, consumed in command-line order, so in a \
                      link whose only references to imports are such switches each library's pulls follow the \
                      switches and then its null thunk, the libraries in command-line order (the image's descriptor \
                      order, else --libs). LINK's qsort of that key sequence permutes each DLL's block, and the \
                      permutation depends only on the key sequence, so it inverts: this prints the switches, per \
                      DLL in the order whose sort is the reference's IAT, each name's decorated symbol read from its \
                      import library (--libdir, else the image's linker toolchain's lib directory; an ordinal \
                      import by its ordinal). The order of `.idata$6` is the pull order too, so these switches \
                      reproduce the IAT and ILT, not the hint/name layout. --out writes the libraries and the \
                      switches as a LINK response fragment; --check IMG compares a link made with them slot by slot \
                      with the reference (exit 3 when a slot differs). JSON rows: the switches."
    )]
    IncludeOrder(IncludeOrderArgs),
}

#[derive(Debug, Args)]
pub struct LogInputs {
    #[arg(
        long,
        value_name = "FILE",
        help = "The link's /MAP. Default: the image's `map` in the config."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The LINK /VERBOSE log. Default: build.log one directory above the map."
    )]
    pub log: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where the command-line objects are. Default: the image's `objdir`, else the map's directory."
    )]
    pub objdir: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "A directory to find the log's libraries in by file name, before the map's parent and directory, \
                the output directories of the static libraries the workspace builds beside it, the image's \
                configured `libraries` and its linker toolchain's lib directory. Repeatable."
    )]
    pub libdir: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "LIB",
        help = "An own library (its file stem): a member reads as the member itself in either spelling LINK prints. \
                Default: every static library the image's workspace builds that is not [[image.vendored]]. \
                Repeatable; `--own -` names none."
    )]
    pub own: Vec<String>,
}

#[derive(Debug, Args)]
pub struct IcfArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The reference: an [[image]] id, or a file path."
    )]
    pub image: String,
    #[command(flatten)]
    pub inputs: LogInputs,
    #[arg(long, help = "List every member and every REF-FOLDS name.")]
    pub verbose: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IcfPredictArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The reference: an [[image]] id, or a file path."
    )]
    pub image: String,
    #[command(flatten)]
    pub inputs: LogInputs,
    #[arg(
        long = "obj",
        value_name = "NAME",
        help = "A command-line object, in link order (its file name in --objdir). Repeatable; replaces the project's."
    )]
    pub objs: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The command-line objects, one file name per line, in link order."
    )]
    pub order: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The command-line objects are this .dsp's translation units in file order (MSDEV's link order), \
                in --project-config or its default configuration."
    )]
    pub dsp: Option<PathBuf>,
    #[arg(
        long = "project-config",
        value_name = "NAME",
        help = "The --dsp configuration."
    )]
    pub project_config: Option<String>,
    #[arg(
        long,
        value_name = "FLAGS",
        default_value = "",
        help = "Comma-separated: wine (sort with wine's builtin qsort) and the controls externals-only, dead-out, \
                dead-null, dup-in, sym-order, thunks, stable, reverse."
    )]
    pub model: String,
    #[arg(
        long,
        value_name = "NAME",
        help = "Search the single moves that make NAME survive its group. Repeatable."
    )]
    pub want: Vec<String>,
    #[arg(
        long = "move",
        value_name = "NAME,BEFORE",
        help = "Move COMDAT NAME before BEFORE in its object (NAME,: last in it; a decorated name holds no comma). Repeatable, in order."
    )]
    pub moves: Vec<String>,
    #[arg(
        long = "load",
        value_name = "OBJ,BEFORE",
        help = "Load link-order element OBJ (its log spelling or object key) before BEFORE (OBJ,: last). Repeatable."
    )]
    pub loads: Vec<String>,
    #[arg(
        long,
        value_name = "OBJ:GLOB",
        help = "OBJ (a glob) no longer emits the external COMDATs GLOB matches, nor their funclets. Repeatable."
    )]
    pub drop: Vec<String>,
    #[arg(
        long = "define-in",
        value_name = "OBJ:GLOB[,BEFORE]",
        help = "OBJ also emits each external COMDAT GLOB matches, its first definer's copy, last or before BEFORE. Repeatable."
    )]
    pub define_in: Vec<String>,
    #[arg(
        long = "dump-key",
        value_name = "SIZE:NRELOC|OBJ",
        help = "Print that key's candidates, or one object's, in append order with their sorted positions. Repeatable."
    )]
    pub dump_key: Vec<String>,
    #[arg(
        long = "search-insert",
        value_name = "KEYS",
        help = "Try one dead contribution of each SIZE:NRELOC key (comma-separated) at every position of the array, \
                each dead candidate left out, and each dead own candidate re-keyed."
    )]
    pub search_insert: Option<String>,
    #[arg(
        long,
        value_name = "RVA",
        help = "The pool --search-insert ranks first (its reference address). Default: the pool with the most names."
    )]
    pub focus: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Attested dead COMDATs for the joint search: OBJECT, BEFORE, KEYS, NAME, EVIDENCE, tab-separated."
    )]
    pub candidates: Option<PathBuf>,
    #[arg(
        long = "candidates-from",
        value_name = "DIR",
        help = "Another build's objects: every function its objects define that this link's same objects lack is an attested row."
    )]
    pub candidates_from: Option<PathBuf>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Threads for the joint search. Default: min(8, nproc / 2)."
    )]
    pub jobs: Option<usize>,
    #[arg(long, help = "List every group's members and every pool.")]
    pub verbose: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IcfDiffArgs {
    #[arg(value_name = "BASE", help = "The link before the change.")]
    pub base: PathBuf,
    #[arg(value_name = "NEW", help = "The link after it.")]
    pub new: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "BASE's /MAP. Default: BASE with a .map extension."
    )]
    pub base_map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "NEW's /MAP. Default: NEW with a .map extension."
    )]
    pub new_map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "BASE's /VERBOSE log. Default: build.log beside BASE, else one directory up."
    )]
    pub base_log: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "NEW's /VERBOSE log. Default: as for BASE."
    )]
    pub new_log: Option<PathBuf>,
    #[arg(
        long,
        value_name = "ID",
        help = "An [[image]] whose text-pad residue rows (and their reference bytes) may sit in a body's pad."
    )]
    pub image: Option<String>,
    #[arg(
        long,
        value_name = "OLD=NEW",
        help = "A body or object renamed between BASE and NEW. Repeatable."
    )]
    pub rename: Vec<String>,
    #[arg(long, value_name = "FILE", help = "Renames, OLD <tab> NEW per line.")]
    pub renames: Option<PathBuf>,
    #[arg(
        long = "no-content",
        help = "Do not pair bodies by their masked content."
    )]
    pub no_content: bool,
    #[arg(long, help = "Every line of every list.")]
    pub all: bool,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 12,
        help = "Lines per list without --all."
    )]
    pub limit: usize,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IatArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The reference: an [[image]] id, or a file path."
    )]
    pub image: String,
    #[arg(value_name = "LOG", help = "LINK's /VERBOSE output.")]
    pub log: PathBuf,
    #[arg(long, value_name = "IMG", help = "Our linked image, compared too.")]
    pub ours: Option<PathBuf>,
    #[arg(
        long,
        value_name = "vc6|wine",
        default_value = "vc6",
        help = "Whose qsort LINK ran."
    )]
    pub crt: String,
    #[arg(
        long,
        value_name = "model|reverse|no-null",
        default_value = "model",
        help = "The population sorted: the model, or a control."
    )]
    pub control: String,
    #[arg(
        long,
        help = "Print the pull order that gives the reference's IAT, and the reference's load order."
    )]
    pub invert: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "A pull-edit script applied to the log's pulls."
    )]
    pub edit: Option<PathBuf>,
    #[arg(
        long = "ref-edit",
        value_name = "FILE",
        help = "A pull-edit script applied to the reference's population."
    )]
    pub ref_edit: Option<PathBuf>,
    #[arg(
        long = "phantom-search",
        value_name = "N",
        default_value_t = 0,
        help = "Add up to N phantom imports to the reference's population, greedily."
    )]
    pub phantom_search: usize,
    #[arg(
        long = "seed-dead",
        help = "Seed the reference's population with this log's dead imports."
    )]
    pub seed_dead: bool,
    #[arg(
        long = "run-search",
        value_name = "SPEC",
        help = "The run-length search over the reference's population."
    )]
    pub run_search: Option<String>,
    #[arg(
        long = "dead-total",
        value_name = "N",
        help = "The run-length search's total of discarded imports."
    )]
    pub dead_total: Option<usize>,
    #[arg(
        long = "run-names",
        help = "List each run's live imports and each fit's slack."
    )]
    pub run_names: bool,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Threads for the run-length search."
    )]
    pub jobs: Option<usize>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IltOrderArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The reference: an [[image]] id, or a file path."
    )]
    pub image: String,
    #[arg(long, value_name = "IMG", help = "Our /INCREMENTAL link.")]
    pub candidate: PathBuf,
    #[arg(long, value_name = "FILE", help = "The candidate's /MAP.")]
    pub map: Option<PathBuf>,
    #[arg(long = "exports-only", help = "Name slots by the export tables only.")]
    pub exports_only: bool,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 20,
        help = "Misplaced names to show."
    )]
    pub show: usize,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IltDiffArgs {
    #[arg(value_name = "BASE", help = "The link before the change.")]
    pub base: PathBuf,
    #[arg(value_name = "NEW", help = "The link after it.")]
    pub new: PathBuf,
    #[arg(
        long,
        value_name = "REF",
        help = "The shipped image, for the slot-to-body map."
    )]
    pub shipped: Option<PathBuf>,
    #[arg(long, help = "Every line of every list.")]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ReserveArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id, or a file path.")]
    pub image: String,
    #[arg(long, value_name = "IMG", help = "Read this linked image instead.")]
    pub candidate: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IncludeOrderArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The reference: an [[image]] id, or a file path."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where the import libraries are. Repeatable."
    )]
    pub libdir: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "DLL,...",
        help = "The libraries' command-line order. Default: the descriptor order."
    )]
    pub libs: Option<String>,
    #[arg(
        long,
        value_name = "vc6|wine",
        default_value = "vc6",
        help = "Whose qsort LINK runs."
    )]
    pub crt: String,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the libraries and switches as a LINK response fragment."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        value_name = "IMG",
        help = "Compare this link's IAT with the reference's."
    )]
    pub check: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

pub fn run(config: Option<&Path>, a: &LinkArgs) -> Result<Outcome> {
    match &a.verb {
        LinkVerb::Icf(x) => icf_verb(config, x),
        LinkVerb::IcfPredict(x) => icf_predict(config, x),
        LinkVerb::IcfDiff(x) => icf_diff(config, x),
        LinkVerb::Iat(x) => iat_verb(config, x),
        LinkVerb::IltOrder(x) => ilt_order(config, x),
        LinkVerb::IltDiff(x) => ilt_diff(x),
        LinkVerb::Reserve(x) => reserve_verb(config, x),
        LinkVerb::IncludeOrder(x) => include_order(config, x),
    }
}

fn find_ci(dir: &Path, name: &str) -> Option<PathBuf> {
    let want = name.to_ascii_lowercase();
    let mut hits: Vec<PathBuf> = std::fs::read_dir(dir)
        .ok()?
        .flatten()
        .filter(|e| e.file_name().to_string_lossy().to_ascii_lowercase() == want)
        .map(|e| e.path())
        .collect();
    hits.sort();
    hits.into_iter().next()
}

fn leaf(p: &str) -> &str {
    p.rsplit(['\\', '/']).next().unwrap_or(p)
}

struct Lib {
    data: Vec<u8>,
    by_name: HashMap<String, (usize, usize)>,
    by_symbol: HashMap<String, (usize, usize)>,
}

impl Lib {
    fn read(path: &Path) -> Option<Self> {
        let data = read_capped(path, MAX_ARCHIVE).ok()?;
        let (by_name, by_symbol) = {
            let ar = Archive::parse(&data).ok()?;
            let mut by_name: HashMap<String, (usize, usize)> = HashMap::new();
            let mut at: HashMap<usize, (usize, usize)> = HashMap::new();
            for m in ar.members.iter().filter(|m| m.is_content()) {
                let span = (m.data_off, m.size);
                at.insert(m.header_off, span);
                let n = m.name.to_ascii_lowercase();
                by_name.entry(leaf(&n).to_string()).or_insert(span);
                by_name.entry(n).or_insert(span);
            }
            let mut by_symbol = HashMap::new();
            for (s, off) in ar.symbol_index() {
                if let Some(span) = at.get(&off) {
                    by_symbol.entry(s).or_insert(*span);
                }
            }
            (by_name, by_symbol)
        };
        Some(Self {
            data,
            by_name,
            by_symbol,
        })
    }

    fn bytes(&self, span: (usize, usize)) -> Option<Vec<u8>> {
        self.data
            .get(span.0..span.0.checked_add(span.1)?)
            .map(<[u8]>::to_vec)
    }
}

struct Finder {
    objdir: PathBuf,
    libdirs: Vec<PathBuf>,
    libraries: Vec<PathBuf>,
    libs: HashMap<String, Option<Lib>>,
}

impl Finder {
    fn new(objdir: &Path, libdirs: Vec<PathBuf>, libraries: Vec<PathBuf>) -> Self {
        Self {
            objdir: objdir.to_path_buf(),
            libdirs,
            libraries,
            libs: HashMap::new(),
        }
    }

    fn lib(&mut self, lib: &str) -> Option<&Lib> {
        let key = lib.to_ascii_lowercase();
        if !self.libs.contains_key(&key) {
            let name = leaf(lib);
            let path = self
                .libdirs
                .iter()
                .find_map(|d| find_ci(d, name))
                .or_else(|| {
                    self.libraries
                        .iter()
                        .find(|p| {
                            p.file_name()
                                .is_some_and(|f| f.to_string_lossy().eq_ignore_ascii_case(name))
                        })
                        .cloned()
                });
            let loaded = path.and_then(|p| Lib::read(&p));
            self.libs.insert(key.clone(), loaded);
        }
        self.libs.get(&key)?.as_ref()
    }
}

impl Inputs for Finder {
    fn object(&mut self, name: &str) -> Option<Vec<u8>> {
        let p = find_ci(&self.objdir, leaf(name))?;
        read_capped(&p, MAX_OBJECT).ok()
    }

    fn member(&mut self, lib: &str, member: &str) -> Option<Vec<u8>> {
        let want = member.to_ascii_lowercase();
        let l = self.lib(lib)?;
        let span = l
            .by_name
            .get(&want)
            .or_else(|| l.by_name.get(leaf(&want)))
            .copied()?;
        l.bytes(span)
    }

    fn member_defining(&mut self, lib: &str, names: &[String]) -> Option<Vec<u8>> {
        let l = self.lib(lib)?;
        let span = names.iter().find_map(|n| l.by_symbol.get(n).copied())?;
        l.bytes(span)
    }
}

struct Ctx {
    label: String,
    project: Option<(Config, usize)>,
    img: Image,
    names: Names,
    seats: Vec<(u32, u32)>,
    text_pad: Vec<(u32, u32)>,
}

fn exports_oracle(img: &Image) -> Result<NameOracle> {
    let mut o = NameOracle::default();
    let ns: Vec<Name> = img
        .exports()?
        .map(|e| {
            e.entries
                .iter()
                .filter_map(|x| {
                    Some(Name {
                        rva: x.target.get(),
                        name: x.name.clone()?,
                        kind: NameKind::Function,
                        size: None,
                        scope: None,
                        provider: "exports".into(),
                    })
                })
                .collect()
        })
        .unwrap_or_default();
    o.add("exports", "the export table", ns, Vec::new());
    Ok(o)
}

impl Ctx {
    fn load(config: Option<&Path>, arg: &str) -> Result<Self> {
        let p = project::load(config)?;
        if let Some(c) = &p
            && let Some((n, _)) = c.image(arg)
        {
            let env = Env::load(c, n)?;
            let seats = env.seats.clone();
            return Ok(Self {
                label: env.label.clone(),
                names: env.names.clone(),
                img: env.img.clone(),
                seats,
                text_pad: env.text_pad.clone(),
                project: p.map(|c| (c, n)),
            });
        }
        let r = project::resolve(p.as_ref(), arg)?;
        let img = Image::parse(r.bytes).with_context(|| r.path.display().to_string())?;
        let names = Names::from_oracle(&exports_oracle(&img)?);
        Ok(Self {
            label: r.label,
            project: None,
            img,
            names,
            seats: Vec::new(),
            text_pad: Vec::new(),
        })
    }

    fn decl(&self) -> Option<&msvc6_project::config::ImageDecl> {
        self.project.as_ref().and_then(|(c, n)| c.images.get(*n))
    }

    fn config_path(&self, field: &str, raw: Option<&String>) -> Result<Option<PathBuf>> {
        let (Some((c, n)), Some(raw)) = (self.project.as_ref(), raw) else {
            return Ok(None);
        };
        let key = format!("{}.{field}", c.image_key(*n));
        Ok(Some(c.path_at(&key, raw)?))
    }

    fn text(&self) -> (&[u8], u32) {
        self.img
            .section(".text")
            .map_or((&[][..], 0), |s| (self.img.section_bytes(s), s.rva.get()))
    }

    fn resolve(&self, name: &str) -> Option<u32> {
        let (text, rva) = self.text();
        self.names.rva(name, text, rva)
    }

    fn workspace(&self) -> Option<(Dsw, String)> {
        let (c, n) = self.project.as_ref()?;
        let d = c.images.get(*n)?;
        let r = d.project.as_ref()?;
        let key = format!("{}.project.dsw", c.image_key(*n));
        let path = c.path_at(&key, r.dsw.as_ref()?).ok()?;
        let dsw = Dsw::read(&path).ok()?;
        Some((dsw, msvc6_project::spec::top_of(&r.config).to_string()))
    }

    fn own_default(&self) -> Vec<String> {
        let Some((dsw, top)) = self.workspace() else {
            return Vec::new();
        };
        let vendored: Vec<String> = self
            .decl()
            .map(|d| {
                d.vendored
                    .iter()
                    .map(|v| v.name.to_ascii_lowercase())
                    .collect()
            })
            .unwrap_or_default();
        dsw.projects
            .iter()
            .filter(|p| !p.name.eq_ignore_ascii_case(&top))
            .filter(|p| !vendored.contains(&p.name.to_ascii_lowercase()))
            .filter_map(|p| {
                let path = dsw.dsp_path(p);
                let dsp = Dsp::read(&path).ok()?;
                dsp.is_static_library()
                    .then(|| path.file_stem().map(|s| s.to_string_lossy().into_owned()))
                    .flatten()
            })
            .collect()
    }

    fn keys(&self, own: &[String]) -> Keys {
        if own.iter().any(|o| o == "-") {
            return Keys::default();
        }
        if own.is_empty() {
            Keys::new(self.own_default())
        } else {
            Keys::new(own)
        }
    }

    fn tus(&self) -> Result<Vec<String>> {
        let Some((c, n)) = self.project.as_ref() else {
            return Ok(Vec::new());
        };
        let Some(r) = c.images.get(*n).and_then(|d| d.project.as_ref()) else {
            return Ok(Vec::new());
        };
        let top = msvc6_project::spec::top_of(&r.config).to_string();
        let specs = msvc6_project::spec::resolve(c, *n)?;
        Ok(specs
            .specs
            .iter()
            .filter(|s| s.project.eq_ignore_ascii_case(&top))
            .map(|s| format!("{}.obj", s.stem))
            .collect())
    }

    fn linker_lib(&self) -> Option<PathBuf> {
        let (c, n) = self.project.as_ref()?;
        let d = c.images.get(*n)?;
        let id = d.linker.as_deref().or(d.compiler.as_deref())?;
        let cfg = c.toolchain_config();
        msvc6_toolchain::layout::Toolchain::named(&cfg, id)
            .ok()?
            .lib
    }

    fn libraries(&self) -> Vec<PathBuf> {
        let Some((c, n)) = self.project.as_ref() else {
            return Vec::new();
        };
        let Some(d) = c.images.get(*n) else {
            return Vec::new();
        };
        let key = format!("{}.libraries", c.image_key(*n));
        d.libraries
            .iter()
            .filter_map(|l| c.path_at(&key, l).ok())
            .collect()
    }
}

fn workspace_lib_dirs(dir: &Path) -> Vec<PathBuf> {
    msvc6_diff::objs::workspace_libs(dir)
        .into_iter()
        .map(|(d, _)| d)
        .collect()
}

fn search_dirs(ctx: &Ctx, given: &[PathBuf], map_dir: &Path) -> Vec<PathBuf> {
    let mut v = given.to_vec();
    v.push(map_dir.join(".."));
    v.push(map_dir.to_path_buf());
    v.extend(workspace_lib_dirs(map_dir));
    if let Some(l) = ctx.linker_lib() {
        v.push(l);
    }
    v
}

struct LogCtx {
    map: PathBuf,
    map_dir: PathBuf,
    log: PathBuf,
    log_text: String,
    objdir: PathBuf,
}

fn log_ctx(ctx: &Ctx, i: &LogInputs) -> Result<LogCtx> {
    let map = match &i.map {
        Some(m) => m.clone(),
        None => ctx
            .config_path("map", ctx.decl().and_then(|d| d.map.as_ref()))?
            .context("name the link's /MAP: --map, or the image's `map` in the config")?,
    };
    let map_dir = map
        .parent()
        .map_or_else(|| PathBuf::from("."), Path::to_path_buf);
    let log = i
        .log
        .clone()
        .unwrap_or_else(|| map_dir.join("..").join("build.log"));
    let log_text = verbose::read_text(&log).with_context(|| {
        format!(
            "{}: a LINK /VERBOSE log (`msvc6t build --link-env /VERBOSE` writes one)",
            log.display()
        )
    })?;
    let objdir = match &i.objdir {
        Some(o) => o.clone(),
        None => ctx
            .config_path("objdir", ctx.decl().and_then(|d| d.objdir.as_ref()))?
            .unwrap_or_else(|| map_dir.clone()),
    };
    Ok(LogCtx {
        map,
        map_dir,
        log,
        log_text,
        objdir,
    })
}

fn icf_verb(config: Option<&Path>, a: &IcfArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image)?;
    let lc = log_ctx(&ctx, &a.inputs)?;
    let keys = ctx.keys(&a.inputs.own);
    let groups = verbose::groups(&lc.log_text);
    let map = linkmap::parse(&verbose::read_text(&lc.map)?);
    let ours = map.symbols(ctx.img.opt.image_base.get());
    let objof = objects_of(&map, &groups, &keys);
    let resolve = |n: &str| ctx.resolve(n);
    let owners = RefOwners::new(&ours, &objof, &resolve, &keys);
    let (text, text_rva) = ctx.text();
    let functions: Vec<(u32, Vec<u8>)> = ctx
        .seats
        .iter()
        .filter_map(|&(rva, len)| {
            let end = rva.checked_add(len)?;
            let cut = ctx
                .text_pad
                .iter()
                .map(|(p, _)| *p)
                .filter(|p| *p > rva && *p < end)
                .min()
                .unwrap_or(end);
            let off = usize::try_from(rva.checked_sub(text_rva)?).ok()?;
            let b = text.get(off..off.checked_add(usize::try_from(cut - rva).ok()?)?)?;
            Some((rva, b.to_vec()))
        })
        .collect();
    let mut finder = Finder::new(
        &lc.objdir,
        search_dirs(&ctx, &a.inputs.libdir, &lc.map_dir),
        ctx.libraries(),
    );
    let mut objs = folds::Objects::new(&mut finder);
    let log_s = lc.log.display().to_string();
    let map_s = lc.map.display().to_string();
    let rep = folds::run(
        &folds::Input {
            label: &ctx.label,
            log: &log_s,
            map: &map_s,
            groups: &groups,
            ours: &ours,
            objof: &objof,
            owners: &owners,
            resolve: &resolve,
            functions: &functions,
            keys: &keys,
            verbose: a.verbose,
        },
        &mut objs,
    );
    for l in &rep.lines {
        println!("{l}");
    }
    json(
        &a.json,
        "link icf",
        &ctx.label,
        Some(&rep.summary),
        &rep.rows,
    )?;
    Ok(Outcome::Ok)
}

fn command_line_objects(ctx: &Ctx, a: &IcfPredictArgs) -> Result<Vec<String>> {
    if !a.objs.is_empty() {
        return Ok(a.objs.clone());
    }
    if let Some(f) = &a.order {
        let text = msvc6_base::srcio::tool_text(&read_capped(f, verbose::MAX_TEXT)?);
        return Ok(text
            .lines()
            .map(str::trim)
            .filter(|l| !l.is_empty())
            .map(|l| leaf(l.trim_matches('"')).to_string())
            .collect());
    }
    if let Some(d) = &a.dsp {
        let dsp = Dsp::read(d)?;
        let cfg = match &a.project_config {
            Some(c) => c.clone(),
            None => dsp
                .default_config
                .clone()
                .with_context(|| format!("{}: no CFG= line; pass --project-config", d.display()))?,
        };
        return Ok(dsp
            .settings(&cfg)
            .files
            .iter()
            .filter(|f| f.is_tu())
            .map(|f| format!("{}.obj", f.stem()))
            .collect());
    }
    let tus = ctx.tus()?;
    if tus.is_empty() {
        bail!(
            "name the command-line objects: --obj NAME (repeatable), --order FILE, --dsp FILE, or an [[image]] \
             with a project"
        );
    }
    Ok(tus)
}

fn parse_rva(s: &str) -> Result<u32> {
    let t = s
        .strip_prefix("0x")
        .or_else(|| s.strip_prefix("0X"))
        .with_context(|| format!("`{s}` is not a 0x-hexadecimal rva"))?;
    u32::from_str_radix(t, 16).with_context(|| format!("`{s}` is not a 0x-hexadecimal rva"))
}

fn icf_predict(config: Option<&Path>, a: &IcfPredictArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image)?;
    let lc = log_ctx(&ctx, &a.inputs)?;
    let keys = ctx.keys(&a.inputs.own);
    let model = Model::parse(&a.model)?;
    let moves: Vec<Move> = a
        .moves
        .iter()
        .map(|m| Move::parse(m))
        .collect::<Result<_>>()?;
    let loads: Vec<LoadAt> = a
        .loads
        .iter()
        .map(|l| LoadAt::parse(l))
        .collect::<Result<_>>()?;
    let mut defs: Vec<Definer> = a
        .drop
        .iter()
        .map(|d| Definer::parse(d, false))
        .collect::<Result<_>>()?;
    for d in &a.define_in {
        defs.push(Definer::parse(d, true)?);
    }
    let dump: Vec<Dump> = a
        .dump_key
        .iter()
        .map(|d| Dump::parse(d))
        .collect::<Result<_>>()?;
    let focus = a.focus.as_deref().map(parse_rva).transpose()?;
    let mut order0: Vec<Entry> = command_line_objects(&ctx, a)?
        .into_iter()
        .map(Entry::Object)
        .collect();
    order0.extend(icf::loaded_entries(&verbose::loads(&lc.log_text)));
    let order = icf::apply_loads(&order0, &loads, &keys)?;
    let dead = verbose::discarded(&lc.log_text);
    let libdirs = search_dirs(&ctx, &a.inputs.libdir, &lc.map_dir);
    let mut finder = Finder::new(&lc.objdir, libdirs.clone(), ctx.libraries());
    let parsed = icf::parse(&order, &mut finder);
    let jobs = crate::diff::jobs(a.jobs);
    let mut joint_rows: Vec<joint::Attested> = Vec::new();
    let mut sources: Vec<String> = Vec::new();
    let mut other: Option<icf::Array> = None;
    if let Some(p) = &a.candidates {
        let text = msvc6_base::srcio::tool_text(&read_capped(p, verbose::MAX_TEXT)?);
        joint_rows.extend(joint::parse_candidates(&text).with_context(|| p.display().to_string())?);
        sources.push(format!("--candidates {}", p.display()));
    }
    if let Some(d) = &a.candidates_from {
        let mut dirs = a.inputs.libdir.clone();
        dirs.push(d.join(".."));
        dirs.push(d.clone());
        dirs.extend(workspace_lib_dirs(d));
        if let Some(l) = ctx.linker_lib() {
            dirs.push(l);
        }
        let mut f2 = Finder::new(d, dirs, ctx.libraries());
        let theirs = icf::parse(&order, &mut f2);
        let from = d.display().to_string();
        let (rows, note) = joint::derive_candidates(&parsed, &theirs, &from);
        sources.push(format!(
            "--candidates-from {from}: {} row(s); {note}",
            rows.len()
        ));
        other = Some(predict::joint_other(&theirs, &rows, &dead, model, &defs)?);
        joint_rows.extend(rows);
    }
    let groups = verbose::groups(&lc.log_text);
    let map = linkmap::parse(&verbose::read_text(&lc.map)?);
    let ours = map.symbols(ctx.img.opt.image_base.get());
    let objof = objects_of(&map, &groups, &keys);
    let resolve = |n: &str| ctx.resolve(n);
    let owners = RefOwners::new(&ours, &objof, &resolve, &keys);
    let what_if =
        !defs.is_empty() || !moves.is_empty() || !loads.is_empty() || lc.objdir != lc.map_dir;
    let base_parsed = if what_if && !(lc.objdir == lc.map_dir && loads.is_empty()) {
        let mut f0 = Finder::new(&lc.map_dir, libdirs, ctx.libraries());
        Some(icf::parse(&order0, &mut f0))
    } else {
        None
    };
    let base = if what_if {
        Some(base_parsed.as_ref().unwrap_or(&parsed))
    } else {
        None
    };
    let joint =
        (a.candidates.is_some() || a.candidates_from.is_some()).then(|| predict::JointInput {
            rows: joint_rows,
            sources: sources.join("; "),
            other,
        });
    let rep = predict::run(&predict::Input {
        label: &ctx.label,
        parsed: &parsed,
        base,
        dead: &dead,
        groups: &groups,
        model,
        want: &a.want,
        moves: &moves,
        defs: &defs,
        dump: &dump,
        owners: &owners,
        keys: &keys,
        search_insert: a.search_insert.as_deref(),
        focus,
        joint,
        verbose: a.verbose,
        jobs,
    })?;
    for l in &rep.lines {
        println!("{l}");
    }
    json(
        &a.json,
        "link icf-predict",
        &ctx.label,
        Some(&rep.summary),
        &rep.rows,
    )?;
    let s = &rep.summary;
    Ok(if s.refused > 0 {
        Outcome::Exit(ExitCode::from(1))
    } else if s.order_ok < s.groups {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

fn default_map(img: &Path) -> PathBuf {
    img.with_extension("map")
}

fn default_log(img: &Path) -> PathBuf {
    let dir = img.parent().unwrap_or(Path::new("."));
    let here = dir.join("build.log");
    if here.exists() {
        return here;
    }
    dir.join("..").join("build.log")
}

fn load_link(
    img: &Path,
    map: Option<&Path>,
    log: Option<&Path>,
) -> Result<(Link, PathBuf, PathBuf)> {
    let mp = map.map_or_else(|| default_map(img), Path::to_path_buf);
    let lp = log.map_or_else(|| default_log(img), Path::to_path_buf);
    let m = linkmap::parse(&verbose::read_text(&mp)?);
    if m.rows.is_empty() {
        bail!("{}: no symbol rows -- not a LINK /MAP?", mp.display());
    }
    let groups = verbose::groups(
        &verbose::read_text(&lp)
            .with_context(|| format!("{}: the LINK /VERBOSE log", lp.display()))?,
    );
    let bytes = read_capped(img, MAX_ARCHIVE)?;
    Ok((
        Link {
            img: Image::parse(bytes).with_context(|| img.display().to_string())?,
            map: m,
            groups,
        },
        mp,
        lp,
    ))
}

fn residue_of(config: Option<&Path>, id: Option<&str>) -> Result<Residue> {
    let Some(id) = id else {
        return Ok(Residue::default());
    };
    let c = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&c, id)?;
    let r = c.reference(n)?;
    let img = Image::parse(r.bytes.clone())?;
    let rows = d
        .residue
        .iter()
        .filter(|x| x.kind == msvc6_image::postlink::Kind::TextPad && x.len > 0)
        .filter_map(|x| {
            let off = x.off.or_else(|| {
                x.rva
                    .and_then(|v| img.rva_to_off(msvc6_base::units::Rva(v)).map(|o| o.get()))
            })?;
            Some((off, x.len))
        })
        .collect();
    Ok(Residue {
        rows,
        reference: r.bytes,
    })
}

fn icf_diff(config: Option<&Path>, a: &IcfDiffArgs) -> Result<Outcome> {
    let (bl, bmp, blp) = load_link(&a.base, a.base_map.as_deref(), a.base_log.as_deref())?;
    let (nl, nmp, nlp) = load_link(&a.new, a.new_map.as_deref(), a.new_log.as_deref())?;
    let res = residue_of(config, a.image.as_deref())?;
    let mut renames = Renames::default();
    for r in &a.rename {
        renames.parse_arg(r)?;
    }
    if let Some(f) = &a.renames {
        renames.parse_file(&msvc6_base::srcio::tool_text(&read_capped(
            f,
            verbose::MAX_TEXT,
        )?))?;
    }
    let rep = icfdiff::compare(
        &bl,
        &nl,
        &res,
        &icfdiff::Options {
            renames: (!renames.is_empty()).then_some(&renames),
            content: !a.no_content,
        },
    )?;
    let (bs, ns) = (a.base.display().to_string(), a.new.display().to_string());
    let (bm, nm) = (bmp.display().to_string(), nmp.display().to_string());
    let (bg, ng) = (blp.display().to_string(), nlp.display().to_string());
    for l in icfdiff::lines(
        &rep,
        &bl,
        &nl,
        &res,
        &icfdiff::Names {
            base: &bs,
            new: &ns,
            maps: (&bm, &nm),
            logs: (&bg, &ng),
        },
        a.all,
        a.limit,
    ) {
        println!("{l}");
    }
    json::<icfdiff::Report, ()>(&a.json, "link icf-diff", &ns, Some(&rep), &[])?;
    Ok(if rep.findings() == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

#[derive(Debug, Serialize)]
struct IatSummary {
    same: usize,
    slots: usize,
    crt: &'static str,
    control: &'static str,
}

fn read_edits(p: &Path) -> Result<Vec<iat::Edit>> {
    let text = msvc6_base::srcio::tool_text(&read_capped(p, verbose::MAX_TEXT)?);
    iat::parse_edits(&text).with_context(|| p.display().to_string())
}

fn iat_verb(config: Option<&Path>, a: &IatArgs) -> Result<Outcome> {
    let p = project::load(config)?;
    let r = project::resolve(p.as_ref(), &a.image)?;
    let refimg = Image::parse(r.bytes).with_context(|| r.path.display().to_string())?;
    let crt = Crt::parse(&a.crt)?;
    let control = Control::parse(&a.control)?;
    let text = verbose::read_text(&a.log)?;
    let mut pulls: Vec<Pull> = iat::pulls(&verbose::loads(&text))?;
    if pulls.is_empty() {
        bail!(
            "{}: no import-library member is `Loaded' in it; this needs LINK's /VERBOSE narration",
            a.log.display()
        );
    }
    let mut out: Vec<String> = Vec::new();
    if let Some(e) = &a.edit {
        let edits = read_edits(e)?;
        iat::apply_edits(&mut pulls, &edits)?;
        out.push(format!(
            "{}: {} pull-edit(s) from {} applied to the log's pulls -- a PRICE, not a link; `ours' below is the unedited image",
            r.label,
            edits.len(),
            e.display()
        ));
    }
    let sorted = iat::sorted_population(&pulls, crt, control)?;
    let n_imp = pulls
        .iter()
        .filter(|p| p.kind == verbose::Kind::Import)
        .count();
    let n_null = pulls
        .iter()
        .filter(|p| p.kind == verbose::Kind::NullThunk)
        .count();
    out.push(format!(
        "{}: IAT PREDICTED FROM {} ({} qsort, population {})",
        r.label,
        a.log.display(),
        crt.label(),
        control.label()
    ));
    out.push(format!(
        "  sorted {} of {n_imp} import(s) + {n_null} null thunk(s) loaded; sort key = archive member name",
        sorted.len()
    ));
    if control != Control::Model {
        out.push("  ** A CONTROL: this population is NOT the mechanism **".to_string());
    }
    let order = iat::block_order(&pulls, &sorted);
    out.push(format!("  predicted block order: {}", order.join(", ")));
    let mut images: Vec<(&str, Image)> = Vec::new();
    if let Some(o) = &a.ours {
        images.push((
            "ours",
            Image::parse(read_capped(o, MAX_ARCHIVE)?).with_context(|| o.display().to_string())?,
        ));
    }
    images.push(("reference", refimg));
    let mut verdict = iat::Comparison::default();
    for (label, img) in &images {
        let blocks = iat::blocks(img)?;
        out.push(String::new());
        out.push(format!("  PREDICTED vs {label}:"));
        let c = iat::compare(&pulls, &sorted, &blocks, &order);
        out.extend(c.lines());
        out.push(iat::six_agreement(&pulls, img, &blocks)?);
        verdict = c;
    }
    let refimg = &images.last().context("no reference image")?.1;
    let refblocks = iat::blocks(refimg)?;
    out.extend(reference_population(refimg, &refblocks, &pulls, a, crt)?);
    if a.invert {
        let inv = iat::invert(&pulls, &sorted, &refblocks);
        out.extend(iat::inversion_lines(&inv));
        out.extend(iat::load_order_lines(&pulls, refimg)?);
    }
    for l in &out {
        println!("{l}");
    }
    json(
        &a.json,
        "link iat",
        &r.label,
        Some(&IatSummary {
            same: verdict.same,
            slots: verdict.slots,
            crt: crt.name(),
            control: control.label(),
        }),
        &verdict.rows,
    )?;
    Ok(if verdict.same == verdict.slots {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn reference_population(
    img: &Image,
    blocks: &iat::Blocks,
    log_pulls: &[Pull],
    a: &IatArgs,
    crt: Crt,
) -> Result<Vec<String>> {
    let (mut pulls, orphans) = iat::reference_population(img)?;
    let mut out = vec![
        String::new(),
        format!(
            "  THE REFERENCE'S OWN .idata$6 ORDER, SORTED ({}): its live imports and DLL names in load order, no",
            crt.label()
        ),
        "  log involved -- a block that agrees confirms the model on the original link; one that does not lacks the"
            .to_string(),
        "  imports /OPT:REF discarded (they held sort positions and left no hint/name):".to_string(),
    ];
    if !orphans.is_empty() {
        let names: Vec<&str> = orphans.iter().map(|(_, n)| n.as_str()).collect();
        out.push(format!(
            "    orphan DLL name(s), each a null thunk at its load position: {}",
            names.join(", ")
        ));
    }
    if let Some(spec) = &a.run_search {
        out.extend(runs::run_search(
            &pulls,
            crt,
            blocks,
            spec,
            a.dead_total,
            a.run_names,
            crate::diff::jobs(a.jobs),
        )?);
    }
    if a.seed_dead {
        let lines = iat::seed_dead(&mut pulls, log_pulls, blocks);
        let n = lines.len();
        out.extend(lines);
        out.push(format!(
            "    seeded with this log's {n} pull(s) the reference does not bind (our dead imports), each after the last \
             pull before it that both populations hold"
        ));
    }
    if let Some(e) = &a.ref_edit {
        let edits = read_edits(e)?;
        iat::apply_edits(&mut pulls, &edits)?;
        out.push(format!(
            "    {} pull-edit(s) from {} applied to this population",
            edits.len(),
            e.display()
        ));
    }
    let sorted = iat::sorted_population(&pulls, crt, Control::Model)?;
    let order = iat::block_order(&pulls, &sorted);
    out.extend(iat::compare(&pulls, &sorted, blocks, &order).lines());
    if a.phantom_search > 0 {
        out.extend(iat::phantom_search(
            &mut pulls,
            crt,
            blocks,
            a.phantom_search,
        )?);
        let sorted = iat::sorted_population(&pulls, crt, Control::Model)?;
        out.extend(iat::compare(&pulls, &sorted, blocks, &order).lines());
    }
    Ok(out)
}

fn reference_rows(ctx: &Ctx) -> Result<BTreeMap<u32, Vec<String>>> {
    let mut at: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    let Some((c, n)) = ctx.project.as_ref() else {
        return Ok(at);
    };
    let r = c.reference(*n)?;
    let o = msvc6_project::names::oracle_of(c, *n, &r)?;
    let mut all = o.iter();
    for s in &o.stats {
        let rows: Vec<&Name> = all.by_ref().take(s.names).collect();
        if matches!(s.provider.as_str(), "exports" | "imports") {
            continue;
        }
        for x in rows.into_iter().filter(|x| x.scope.is_none()) {
            let v = at.entry(x.rva).or_default();
            if !v.contains(&x.name) {
                v.push(x.name.clone());
            }
        }
    }
    Ok(at)
}

fn ilt_order(config: Option<&Path>, a: &IltOrderArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image)?;
    let cimg = Image::parse(read_capped(&a.candidate, MAX_ARCHIVE)?)
        .with_context(|| a.candidate.display().to_string())?;
    let rilt = ilt::ilt_of(&ctx.img, "reference")?;
    let cilt = ilt::ilt_of(&cimg, &a.candidate.display().to_string())?;
    let ref_at = if a.exports_only {
        BTreeMap::new()
    } else {
        reference_rows(&ctx)?
    };
    let map_path = a.map.clone().unwrap_or_else(|| {
        let with = PathBuf::from(format!("{}.map", a.candidate.display()));
        if with.is_file() {
            with
        } else {
            a.candidate.with_extension("map")
        }
    });
    let map_text = if a.exports_only {
        None
    } else {
        verbose::read_text(&map_path).ok()
    };
    let mut cand_at: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    if let Some(t) = &map_text {
        for (n, r) in linkmap::parse(t).symbols(cimg.opt.image_base.get()) {
            cand_at.entry(r).or_default().push(n);
        }
    }
    let rn = ilt::name_slots(&rilt, &ilt::exports_of(&ctx.img), &ref_at, true);
    let cn = ilt::name_slots(&cilt, &ilt::exports_of(&cimg), &cand_at, false);
    let source = match (&map_text, a.exports_only) {
        (_, true) => " -- --exports-only".to_string(),
        (None, false) => format!(" -- no map at {}", map_path.display()),
        (Some(_), false) => format!(" -- {}", map_path.display()),
    };
    let rep = ilt::compare(
        &ctx.label,
        &ilt::Side {
            ilt: &rilt,
            named: &rn,
            source: String::new(),
        },
        &ilt::Side {
            ilt: &cilt,
            named: &cn,
            source,
        },
        a.show,
    );
    for l in &rep.lines {
        println!("{l}");
    }
    json(
        &a.json,
        "link ilt-order",
        &ctx.label,
        Some(&rep.summary),
        &rep.rows,
    )?;
    Ok(if rep.summary.misplaced == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn ilt_diff(a: &IltDiffArgs) -> Result<Outcome> {
    let load = |p: &Path| -> Result<Image> {
        Image::parse(read_capped(p, MAX_ARCHIVE)?).with_context(|| p.display().to_string())
    };
    let (bi, ni) = (load(&a.base)?, load(&a.new)?);
    let si = a.shipped.as_deref().map(load).transpose()?;
    let (bs, ns) = (a.base.display().to_string(), a.new.display().to_string());
    let ss = a.shipped.as_ref().map(|p| p.display().to_string());
    let (lines, sum) = iltdiff::report(
        &bi,
        &ni,
        si.as_ref(),
        &iltdiff::Labels {
            base: &bs,
            new: &ns,
            shipped: ss.as_deref(),
        },
        a.all,
    )?;
    for l in &lines {
        println!("{l}");
    }
    json::<iltdiff::Summary, ()>(&a.json, "link ilt-diff", &ns, Some(&sum), &[])?;
    Ok(if sum.findings == 0 {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn reserve_verb(config: Option<&Path>, a: &ReserveArgs) -> Result<Outcome> {
    let p = project::load(config)?;
    let (label, img) = match &a.candidate {
        Some(c) => (
            c.display().to_string(),
            Image::parse(read_capped(c, MAX_ARCHIVE)?).with_context(|| c.display().to_string())?,
        ),
        None => {
            let r = project::resolve(p.as_ref(), &a.image)?;
            (
                r.label.clone(),
                Image::parse(r.bytes).with_context(|| r.path.display().to_string())?,
            )
        }
    };
    let (blocks, sum) = reserve::blocks(&img)?;
    println!(
        "{label}: the ILINK reserve after each ILT / IAT block, due 40 + 4 * floor(imports / 5) bytes"
    );
    for b in &blocks {
        println!("{}", b.line());
    }
    println!(
        "  {} block(s): {} with the due reserve, {} FOSSIL, {} with none",
        sum.blocks, sum.due, sum.fossils, sum.none
    );
    if sum.blocks > 0 && sum.none == sum.blocks {
        println!(
            "  no block has a reserve: not an incremental link's layout, which the rule does not describe"
        );
    }
    json(&a.json, "link reserve", &label, Some(&sum), &blocks)?;
    Ok(if sum.fossils > 0 {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

fn find_lib(dirs: &[PathBuf], dll: &str) -> Option<PathBuf> {
    let stem = dll.rsplit_once('.').map_or(dll, |(s, _)| s);
    dirs.iter().find_map(|d| find_ci(d, &format!("{stem}.lib")))
}

#[derive(Debug, Serialize)]
struct IncludeSummary {
    libraries: Vec<String>,
    same: usize,
    slots: usize,
    checked: Option<(usize, usize)>,
}

fn include_order(config: Option<&Path>, a: &IncludeOrderArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image)?;
    let crt = Crt::parse(&a.crt)?;
    let blocks = iat::blocks(&ctx.img)?;
    let dlls: Vec<String> = match &a.libs {
        Some(l) => l
            .split(',')
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .collect(),
        None => ctx.img.imports()?.iter().map(|d| d.dll.clone()).collect(),
    };
    let mut dirs = a.libdir.clone();
    if let Some(l) = ctx.linker_lib() {
        dirs.push(l);
    }
    let mut libs: Vec<include::ImportLib> = Vec::new();
    let mut files: Vec<String> = Vec::new();
    for d in &dlls {
        let path = find_lib(&dirs, d).with_context(|| {
            format!(
                "{d}: no import library {} in the --libdir directories",
                d.rsplit_once('.').map_or(d.as_str(), |(s, _)| s)
            )
        })?;
        let data = read_capped(&path, MAX_ARCHIVE)?;
        libs.extend(include::read_import_lib(&data).with_context(|| path.display().to_string())?);
        files.push(
            path.file_name()
                .map(|f| f.to_string_lossy().into_owned())
                .unwrap_or_default(),
        );
    }
    let plan = include::plan(&blocks, &dlls, &libs, crt)?;
    println!(
        "{}: the /INCLUDE: order that reproduces its IAT under {} qsort, for a link whose only references to \
         imports are these switches, the libraries in this order:",
        ctx.label,
        crt.label()
    );
    for (d, f) in dlls.iter().zip(&files) {
        println!("  {d} ({f})");
    }
    for i in &plan.includes {
        println!("/INCLUDE:{}", i.symbol);
    }
    println!(
        "  {} switch(es); the sort of this order reproduces {} of {} slot(s)",
        plan.includes.len(),
        plan.same,
        plan.slots
    );
    if let Some(o) = &a.out {
        let mut text = String::new();
        for f in &files {
            text.push_str(f);
            text.push('\n');
        }
        for i in &plan.includes {
            text.push_str(&format!("/INCLUDE:{}\n", i.symbol));
        }
        msvc6_base::srcio::own_write(o, &text)
            .with_context(|| format!("writing {}", o.display()))?;
        eprintln!("wrote {}", o.display());
    }
    let mut checked = None;
    if let Some(c) = &a.check {
        let img =
            Image::parse(read_capped(c, MAX_ARCHIVE)?).with_context(|| c.display().to_string())?;
        let rows = include::check(&blocks, &iat::blocks(&img)?);
        let (mut same, mut slots) = (0, 0);
        for r in &rows {
            same += r.same;
            slots += r.slots;
            match &r.first {
                None => println!("  check {}: {}/{} in sequence", r.dll, r.same, r.slots),
                Some((i, w, g)) => println!(
                    "  check {}: {}/{} in sequence; first divergence at slot {i}: wanted {w}, linked {g}",
                    r.dll, r.same, r.slots
                ),
            }
        }
        println!("  CHECK {same}/{slots} slot(s) as the reference's");
        checked = Some((same, slots));
    }
    json(
        &a.json,
        "link include-order",
        &ctx.label,
        Some(&IncludeSummary {
            libraries: dlls,
            same: plan.same,
            slots: plan.slots,
            checked,
        }),
        &plan.includes,
    )?;
    Ok(match checked {
        Some((s, n)) if s != n => Outcome::Differs,
        _ => Outcome::Ok,
    })
}
