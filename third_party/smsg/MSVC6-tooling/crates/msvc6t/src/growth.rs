use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::{self, Config};
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_base::bounded;
use msvc6_diff::env::Env;
use msvc6_image::pe::Image;
use msvc6_link::linkmap::{self, Map};
use msvc6_link::objkey::Keys;
use msvc6_link::{
    census, deaddata, growth, holes, ilk, layout, memberorder, owner, pullorder, slots,
};
use msvc6_project::functions::FunctionIndex;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const MAX_MAP: u64 = 256 << 20;

pub const MAX_LOG: u64 = 256 << 20;

pub const MAX_RSP: u64 = 16 << 20;

pub const MAX_IMAGE: u64 = 512 << 20;

pub const MAX_OBJECT: u64 = 256 << 20;

pub const MAX_ARCHIVE: u64 = 512 << 20;

#[derive(Debug, Args)]
pub struct ImageArg {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id whose reference the model reads."
    )]
    pub image: String,
}

#[derive(Debug, Args)]
pub struct GrowthArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(
        long,
        value_name = "SPEC",
        help = "Read the body extents from these function-index providers instead of the image's, in this order \
                (`ghidra-json:<file>', `pdb[:<file>]', `map[:<file>]', `reccmp-csv:<file>', `discover'). Every class \
                turns on the extent to within 16 bytes, so which source the census reads is a choice, not a detail. \
                Repeatable."
    )]
    pub functions: Vec<String>,
    #[arg(
        long,
        value_name = "MARK",
        help = "A text that, inside a function index name, marks a funclet rather than a function (the names a \
                function index gives split-off parts are its own, so they are given here): it is absorbed into the \
                function before it, never counted as a boundary. Repeatable; with none, every entry is a function."
    )]
    pub funclet: Vec<String>,
    #[arg(
        long,
        value_name = "VERDICT",
        help = "List every pair of this verdict: reserved, tight, overlap, ambiguous, over, under."
    )]
    pub list: Option<String>,
    #[arg(
        long,
        value_name = "VA",
        help = "Also split the census at this hexadecimal virtual address."
    )]
    pub band: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link's /MAP: split the census by who defines each symbol (an explicit object, an archive \
                member, or neither) and name the earliest own boundary that disagrees with the rule."
    )]
    pub ours: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A library table (lib locate --write): group the verdicts by the archive member each entry address \
                belongs to, against the null model of independent draws."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "IMG",
        help = "Our linked image: for every TIGHT pair, how much of the window our rule would pad is fill in each \
                image."
    )]
    pub occupancy: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link's /MAP: the inter-object `.rdata' gaps it implies, against each object's own `.rdata' \
                size (needs --objs)."
    )]
    pub rdata: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link's /MAP: compare the gap between consecutive same-object functions with the reference's own, \
                which is the one comparison an /INCREMENTAL image's shifted addresses leave readable. The reference \
                addresses are the image's name oracle (its decorated names and their rvas in `.text')."
    )]
    pub spacing: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        requires = "spacing",
        help = "With --spacing: read only the name sources whose path ends with FILE (repeatable). Default: every \
                source of the image's name oracle."
    )]
    pub names: Vec<PathBuf>,
    #[arg(long, value_name = "DIR", help = "Where the objects are. Repeatable.")]
    pub objs: Vec<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct MemberOrderArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(long, value_name = "FILE", help = "Our link's /MAP.")]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The library table the reference's members come from (lib locate --write)."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "An archive whose members decide which recorded rows are contested, because several of them define \
                the row's COMDAT. Repeatable; the default is the image's configured `libraries'."
    )]
    pub archive: Vec<PathBuf>,
    #[arg(long, help = "Name the members off one longest common subsequence.")]
    pub misplaced: bool,
    #[arg(long, help = "Per-member `.text' bytes, the reference's against ours.")]
    pub sizes: bool,
    #[arg(
        long,
        help = "Each common member's seat shift, ours minus the reference's."
    )]
    pub seats: bool,
    #[arg(
        long,
        value_name = "LO..HI",
        help = "Print this 1-based inclusive window of the two common-member sequences side by side."
    )]
    pub window: Option<String>,
    #[arg(
        long,
        value_name = "LIB",
        help = "An own library (its file stem): its members read as the image's own objects, not archive members. \
                Repeatable."
    )]
    pub own: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct PullOrderArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(long, value_name = "DIR", help = "Where the objects are. Repeatable.")]
    pub objs: Vec<PathBuf>,
    #[arg(long, value_name = "FILE", help = "Our link's /MAP.")]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The library table the reference's members come from (lib locate --write)."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "An archive the link searches, in search order. Repeatable; the default is the image's configured \
                `libraries'."
    )]
    pub archive: Vec<PathBuf>,
    #[arg(
        long,
        help = "Model LINK's whole library search (one unresolved-name list, each library walked in turn, a loaded \
                member's own references appended) instead of the directly-pulled subset."
    )]
    pub closure: bool,
    #[arg(
        long,
        value_name = "MEMBER",
        help = "With --closure: this member's references are spliced straight after the name that loaded it, the \
                hypothesis against the plain model. Repeatable."
    )]
    pub eager: Vec<String>,
    #[arg(
        long,
        value_name = "LO..HI",
        help = "With --closure: print this window of the reference and the model side by side."
    )]
    pub window: Option<String>,
    #[arg(long, help = "Name the member and object that pull each member.")]
    pub trace: bool,
    #[arg(
        long,
        value_name = "NAME",
        help = "Print one object's archive-defined references, or one own-library member's referencing sites. \
                Repeatable."
    )]
    pub object: Vec<String>,
    #[arg(
        long,
        value_name = "LIB",
        help = "An own library (its file stem), searched before the archives. Repeatable."
    )]
    pub own: Vec<String>,
    #[arg(
        long,
        value_name = "NAME",
        help = "The entry point LINK adds to the unresolved list after the command-line objects. Default: the \
                image's own (a DLL's __DllMainCRTStartup@12)."
    )]
    pub entry: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LayoutArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(long, value_name = "FILE", help = "Our link's /MAP.")]
    pub map: Option<PathBuf>,
    #[arg(long, value_name = "DIR", help = "Where the objects are. Repeatable.")]
    pub objs: Vec<PathBuf>,
    #[arg(long, help = "Rank object orders. Nothing is linked either way.")]
    pub search: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct SlotsArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(
        value_name = "OBJ",
        help = "The object whose COMDAT slots are measured."
    )]
    pub obj: PathBuf,
    #[arg(long, value_name = "IMG", help = "Our linked image.")]
    pub ours: PathBuf,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct HolesArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(value_name = "IMG", help = "Our linked image.")]
    pub candidate: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link's /MAP: where OUR own code ends (the lowest address an archive member holds)."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A library table (lib locate --write): where the REFERENCE's own code ends, at the lowest address it \
                places an archive member. Without it the whole `.text' is read, archive band included."
    )]
    pub library: Option<PathBuf>,
    #[arg(long, value_name = "DIR", help = "Where the objects are. Repeatable.")]
    pub objs: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 8,
        help = "The shortest run of 0xcc counted. `int3' and operand bytes are 0xcc too, so a threshold is \
                required and every count is approximate in both directions."
    )]
    pub min: usize,
    #[arg(long, help = "List the differing holes.")]
    pub list: bool,
    #[arg(long, help = "With --list, every row rather than the first twelve.")]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DeadDataArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(
        long,
        value_name = "FILE",
        help = "The LINK /VERBOSE log whose `Loaded' and `Discarded' lines say what the link kept."
    )]
    pub log: PathBuf,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where the objects are: the FIRST directory is the link's command line and every one after it a \
                library whose objects are read only where the log says the link loaded them (the library's name is \
                the nearest `.lib' directory above). Repeatable, in that order."
    )]
    pub objs: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link's /MAP: the virtual address of each datum, for the reference's own dword check."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "SWITCHES",
        help = "The link switches, when no `.dsp' beside the log states them (`/OPT:REF' or `/OPT:NOREF' decides; \
                else `/DEBUG' or `/INCREMENTAL:YES' mean NOREF)."
    )]
    pub link: Option<String>,
    #[arg(long, help = "Every datum, reached ones included.")]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ComdatsArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(long, value_name = "FILE", help = "The LINK response file.")]
    pub rsp: PathBuf,
    #[arg(
        long = "class",
        value_name = "CLASS=PATTERN[,PATTERN...]",
        help = "A class and the object-name globs that fall in it, in order (`*' and `?'; the `.obj' suffix is \
                optional). Repeatable; an object no class matches is `other'."
    )]
    pub classes: Vec<String>,
    #[arg(
        long,
        value_name = "NAME",
        default_value = "other",
        help = "The class of an object no --class pattern matches."
    )]
    pub fallback: String,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct OwnerArgs {
    #[command(flatten)]
    pub image: ImageArg,
    #[arg(long, value_name = "DIR", help = "Where the objects are. Repeatable.")]
    pub objs: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A library table (lib locate --write): the names an archive defines, which no source arrangement of \
                ours re-homes."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "CLASS",
        help = "A class spelling (as a decorated name holds it, `ios@@') whose bodies a library's headers define. \
                Repeatable."
    )]
    pub vendor_class: Vec<String>,
    #[arg(
        long,
        value_name = "RVA",
        help = "Print one maximal chain's members and which objects define each (0x-hexadecimal)."
    )]
    pub chain: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IlkArgs {
    #[arg(value_name = "FILE", help = "The `.ilk' an incremental link wrote.")]
    pub file: PathBuf,
    #[arg(
        long,
        value_name = "IMG",
        help = "Check every record against this image's own section table: the control, without which a row is a \
                shape match and nothing more."
    )]
    pub image: Option<PathBuf>,
    #[arg(
        long,
        value_name = "HEX",
        help = "Print the 4-aligned offsets holding this dword and the gaps between them. Repeatable."
    )]
    pub find: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A /MAP whose addresses are the ground truth for the per-placement record walk."
    )]
    pub map: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Subcommand)]
pub enum PlacementVerb {
    #[command(
        about = "Does the reference reserve growth room after every function? A census over every pair",
        long_about = "Our link gives each `.text' COMDAT a slot of round16(body * 5/4), so a gap of exactly that is \
                      RESERVED and a gap of round16(body) is TIGHT (16-byte alignment only). Both rules predict the \
                      same gap for a body of 1..13, 17..26, 33..39 or 49..51 bytes, and such a pair is AMBIGUOUS \
                      rather than evidence either way. Body extents come from the image's function index, never from \
                      a 0xcc fill scan, which runs straight through two adjacent bodies; every extent is \
                      cross-checked against a fill scan and a pair whose two readings fall in different 16-byte \
                      buckets is reported UNTRUSTED. A funclet is absorbed into the function before it (--funclet). \
                      The table mixes own code and archive members, which answer oppositely: read --ours, which \
                      splits it by who defines each symbol, before quoting a percentage. Exit 0; the views are \
                      --list, --band, --ours, --library, --occupancy, --rdata and --spacing. JSON rows: one per pair."
    )]
    Growth(Box<GrowthArgs>),
    #[command(
        about = "Archive member selection and order against the reference's, as a number that can be driven",
        long_about = "LINK pulls an archive member the first time it needs one of its symbols and lays members out in \
                      that order, so member order is a consequence of the order our objects reference things. Two \
                      numbers: SELECTION (how many of the reference's members our link also pulls) and ORDER (the \
                      longest common subsequence of the two sequences over the members both hold). The reference's \
                      sequence comes from a library table, each member keyed on its first UNCONTESTED row -- a row \
                      shorter than 0x20 bytes, or one whose COMDAT two or more members of its archive define, sits \
                      wherever the FIRST of those members was pulled and so positions nothing. A data-only member has \
                      no `.text' row and is absent from the reference side rather than counted as excess. Views: \
                      --misplaced, --sizes, --seats and --window. Exit 0. JSON summary: the four figures."
    )]
    MemberOrder(Box<MemberOrderArgs>),
    #[command(
        about = "Predict the archive pull order from our objects' symbol references, with no link",
        long_about = "Each object's COFF symbol table gives the names it references in emission order and each archive \
                      member's gives the names it defines, so concatenating in link order and deduplicating by member \
                      predicts the pull order. STEERING POWER is printed first because it bounds everything: most of \
                      the reference's members are pulled by CRT-internal references that no permutation of our objects \
                      reorders. The CONTROL scores the predictor against the real link it models, and below 75% no \
                      search result is printed. With --closure the whole library search is modelled instead (one \
                      unresolved-name list, each library walked from the start, a loaded member's references appended, \
                      rounds until one loads nothing), its control against our own /MAP, and below 95% nothing it says \
                      about the reference is scored. Exit 0. JSON summary: the figures."
    )]
    PullOrder(Box<PullOrderArgs>),
    #[command(
        about = "Predict our own-code `.text' layout from the objects alone, then rank object orders",
        long_about = "Three measured facts make our own layout arithmetic: CL emits `.text' COMDATs in source order, \
                      our link gives each one round16(size * 5/4), and an object's contribution starts 16-aligned. \
                      Composing them predicts every own COMDAT's address from the objects' section sizes. The POSITIVE \
                      CONTROL is enforced, not printed: the layout is first predicted for the object order the /MAP \
                      used and compared against the /MAP's own addresses, and a single mismatch REFUSES the search, \
                      because a model that cannot reproduce the link in front of it would rank fictions. The UPPER \
                      BOUND is stronger than any search: give every object its reference start for free and walk its \
                      COMDATs in reference order; the first gap that is not our slot offsets every COMDAT after it. \
                      --search ranks object orders (exhaustive to eight objects, else single-object moves). Exit 0, \
                      3 when the control fails."
    )]
    Layout(Box<LayoutArgs>),
    #[command(
        about = "Does our link's COMDAT slot match round16(size x 5/4), the size read from the object?",
        long_about = "The size the linker lays out is the object's `.text' COMDAT SECTION SIZE, an exact field, not a \
                      decoded body extent (which runs short) nor a fill scan (which runs long through adjacent \
                      bodies). Each COMDAT is located in our image by a masked content search, and a body with \
                      near-identical siblings that does not locate uniquely is excluded rather than resolved by \
                      picking one. A slot is the gap to the next COMDAT WE LOCATED, so a gap that exceeds the rule by \
                      exactly an unplaced COMDAT's own slot is SPANNING -- this command's blind spot, not an anomaly. \
                      An UNDER is what would refute the rule. The second table is what our reserve costs against the \
                      reference's own gaps, in reference order, the padding we add and the gaps wider than the most \
                      ILINK grants kept apart. Exit 0. JSON rows: one per pair."
    )]
    Slots(Box<SlotsArgs>),
    #[command(
        about = "The 0xcc fill runs in `.text': how many holes the reference has, how many we reproduce",
        long_about = "An incremental link over many generations leaves a hole wherever a body shrank, and parks later \
                      evictions in those holes, so the layout is a function of the sources plus the sequence of holes. \
                      This counts the runs of 0xcc in the own-code region on each side and reports the holes we \
                      reproduce at the same rva AND length, the ones at the same rva with another length, and the ones \
                      we lack; then the converse, the fill we emit where the reference has none, which is as wrong and \
                      which no byte total separates. A missing hole is given the SLOT it was (the run less the \
                      previous body's alignment padding) and the generation-1 body sizes that reserve exactly that \
                      slot. `int3' and operand bytes are 0xcc too, so --min is an argument and the headline's sign \
                      changes with it. Exit 0. JSON summary: the counts."
    )]
    Holes(Box<HolesArgs>),
    #[command(
        about = "Every non-COMDAT datum of a link that no kept section reaches: the orphan-data census",
        long_about = "/OPT:REF removes an unreferenced COMDAT but never a plain `.data', `.rdata' or `.bss' section, \
                      nor a COMMON symbol, so a global only a dropped function used survives in the image with \
                      nothing pointing at it, and a byte-exact source must still define it. The link's own words \
                      decide what was kept: `Discarded <symbol> from <object>' for every COMDAT /OPT:REF removed and \
                      `Loaded <lib>(<member>)' for every member it pulled. A datum is a named symbol's HULL in a \
                      non-COMDAT data section, or a COMMON symbol; a relocation whose addend carries the address out \
                      of its own hull is a NEIGHBOUR of the datum it lands in, never a reach. REACHED is a fixed point \
                      over the direct targets of every kept non-data section and then of every reached datum's own \
                      relocations. The rest is classed CL-GUARD, HDR-COPY (an internal-linkage datum several objects \
                      define alike), NEIGHBOUR, INIT-ONLY, DROPPED, DATA-ONLY or NONE; the first two are compiler \
                      behaviour and print first so they are not read as evidence. `ref' counts the dwords of the \
                      reference file that encode an address inside the datum: a 0 is conclusive, a non-zero count may \
                      be a coincidental window. Refused unless the link ran /OPT:REF. Exit 0. JSON rows: one per datum."
    )]
    DeadData(Box<DeadDataArgs>),
    #[command(
        about = "How many COMDATs a link carries, by object class",
        long_about = "/INCREMENTAL:YES gives every COMDAT its own thunk slot, so the switch's cost scales with the \
                      COMDAT count and not with the object count. A COFF section either is a COMDAT or is not, and \
                      that is the whole discrimination: a compiled /Gy object emits one COMDAT per function, while a \
                      transcribed or hand-assembled part emits plain sections that never get a slot. --class groups \
                      the objects by name glob so the two are reported apart. The population is the objects the LINK \
                      was given, so a COMDAT /OPT:REF would discard is counted -- right for the incremental question, \
                      wrong for any question about the shipped image. An operand that does not resolve to a file fails \
                      the run rather than shrinking the census. Exit 0. JSON rows: one per class."
    )]
    Comdats(Box<ComdatsArgs>),
    #[command(
        about = "Which of our objects the reference's layout says each body belongs to, by subtraction",
        long_about = "LINK lays each object's `.text' contribution down contiguously and once, and our link's slot \
                      rule is round16(body * 5/4) with the LAST member of a contribution TIGHT. So where every member \
                      of an object sits exactly one slot after its predecessor the run is CONTIGUOUS and admits no \
                      further body: any other address inside it is FOREIGN, a body the incremental linker relocated \
                      there, and it belongs to whichever TU its callers live in. The UNION CHAINS go further: a \
                      maximal chain of consecutive bodies came from ONE object, so a chain whose owners INTERLEAVE (A, \
                      then B, then A again) is one our attribution gets wrong. Each block of such a chain is ranked by \
                      what the evidence can force -- FOLD (shared bodies, the fold picked the owner), RELOC, EDGE, \
                      INLINE, WHOLE (the object's entire contribution, which one group relocation places anywhere) and \
                      FORCED, the only verdict that licenses a source move. Exit 0. JSON rows: one per object span."
    )]
    Owner(Box<OwnerArgs>),
    #[command(
        about = "Read the frozen section sizes out of an incremental link's own `.ilk'",
        long_about = "An incremental link freezes quantities at its first generation -- `.reloc''s VirtualSize most \
                      sharply -- and no edit to the final sources moves them. The `.ilk' holds them: a section record \
                      is a name pointer, an rva, a VirtualSize at +0x10 and the characteristics at +0x1c, and the \
                      mapping base is derived from the header's own self pointer at +0x1d4. Records are found by \
                      SCANNING for that shape, so the count is a floor, and every one is checked against --image's \
                      own section table by name: if none agrees the walk prints NO VERDICT rather than a list, because \
                      a misparsed file yields plausible-looking rows. --map walks the per-placement array instead, \
                      validating each 60-byte slot's rva against the map's own addresses, where a run of five or more \
                      is evidence no chance match produces. Reading our own `.ilk' is toolchain diagnosis: no byte of \
                      it reaches a reconstructed image. Exit 0, 3 when the control finds no agreeing record."
    )]
    Ilk(Box<IlkArgs>),
}

#[derive(Debug, Args)]
pub struct PlacementArgs {
    #[command(subcommand)]
    pub verb: PlacementVerb,
}

pub fn run(config: Option<&Path>, a: &PlacementArgs) -> Result<Outcome> {
    match &a.verb {
        PlacementVerb::Growth(x) => growth_verb(config, x),
        PlacementVerb::MemberOrder(x) => member_order(config, x),
        PlacementVerb::PullOrder(x) => pull_order(config, x),
        PlacementVerb::Layout(x) => layout_verb(config, x),
        PlacementVerb::Slots(x) => slots_verb(config, x),
        PlacementVerb::Holes(x) => holes_verb(config, x),
        PlacementVerb::DeadData(x) => dead_data(config, x),
        PlacementVerb::Comdats(x) => comdats_verb(config, x),
        PlacementVerb::Owner(x) => owner_verb(config, x),
        PlacementVerb::Ilk(x) => ilk_verb(x),
    }
}

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

fn read_map(p: &Path) -> Result<Map> {
    Ok(linkmap::parse(&bounded::tool(p, MAX_MAP).with_context(
        || format!("reading the /MAP {}", p.display()),
    )?))
}

fn parse_rva(s: &str) -> Result<u32> {
    let t = s
        .strip_prefix("0x")
        .or_else(|| s.strip_prefix("0X"))
        .with_context(|| format!("`{s}' is not a 0x-hexadecimal address"))?;
    u32::from_str_radix(t, 16).with_context(|| format!("`{s}' is not a 0x-hexadecimal address"))
}

struct Ctx {
    p: Config,
    n: usize,
    env: Env,
    index: FunctionIndex,
}

impl Ctx {
    fn load(config: Option<&Path>, image: &str) -> Result<Self> {
        Self::load_with(config, image, &[])
    }

    fn load_with(config: Option<&Path>, image: &str, functions: &[String]) -> Result<Self> {
        let (p, n, env) = crate::diff::env_of(config, image)?;
        let r = p.reference(n)?;
        let index = if functions.is_empty() {
            msvc6_project::functions::index_of(&p, n, &r)?
        } else {
            let img = msvc6_image::pe::Image::parse(r.bytes.clone())?;
            let mut ix = FunctionIndex::default();
            for (i, spec) in functions.iter().enumerate() {
                let src = msvc6_project::config::FunctionSource::try_from(spec.clone())
                    .map_err(|e| anyhow::anyhow!("--functions {spec}: {e}"))?;
                let (kind, from, fs, notes) =
                    msvc6_project::functions::read_one(&p, n, i, &src, &img)?;
                ix.add(&kind, &from, fs, notes);
            }
            ix
        };
        Ok(Self { p, n, env, index })
    }

    fn base(&self) -> u32 {
        self.env.img.opt.image_base.get()
    }

    fn text(&self) -> (&[u8], u32) {
        (self.env.text(), self.env.text_rva)
    }

    fn table(&self, given: Option<&Path>) -> Result<msvc6_match::table::Table> {
        let p = given
            .context("name the library table: --library FILE (lib locate --write writes one)")?;
        msvc6_match::table::Table::read(p)
    }

    fn libraries(&self) -> Vec<PathBuf> {
        let Some(d) = self.p.images.get(self.n) else {
            return Vec::new();
        };
        let key = format!("{}.libraries", self.p.image_key(self.n));
        d.libraries
            .iter()
            .filter_map(|l| self.p.path_at(&key, l).ok())
            .collect()
    }

    fn map_path(&self, given: Option<&Path>) -> Result<PathBuf> {
        if let Some(m) = given {
            return Ok(m.to_path_buf());
        }
        let d = self.p.images.get(self.n).context("no such image")?;
        let raw = d
            .map
            .as_ref()
            .context("name our link's /MAP: --map, or the image's `map' in the config")?;
        let key = format!("{}.map", self.p.image_key(self.n));
        self.p.path_at(&key, raw)
    }

    fn objects(&self, dirs: &[PathBuf]) -> Result<Vec<PathBuf>> {
        if dirs.is_empty() {
            bail!("name the objects: --objs DIR");
        }
        msvc6_diff::objs::build_list(dirs, true)
    }

    fn entries(&self) -> BTreeMap<u32, growth::Entry> {
        let (lo, hi) = (
            self.env.text_rva,
            self.env
                .text_rva
                .saturating_add(self.env.section().virt_size.get()),
        );
        self.index
            .iter()
            .filter(|f| f.rva >= lo && f.rva < hi)
            .filter_map(|f| {
                let end = f.end?;
                Some((
                    f.rva,
                    growth::Entry {
                        body: u64::from(end.checked_sub(f.rva)?),
                        name: f.name.clone().unwrap_or_else(|| format!("{:#x}", f.rva)),
                        thunk: f.thunk,
                    },
                ))
            })
            .collect()
    }
}

fn funclet_test(given: &[String]) -> impl Fn(&str) -> bool + use<> {
    let marks: Vec<String> = given.to_vec();
    move |name: &str| marks.iter().any(|m| name.contains(m.as_str()))
}

#[derive(Debug, Serialize)]
struct GrowthSummary {
    entries: usize,
    pairs: usize,
    absorbed: usize,
    thunks: usize,
    agree: usize,
    disagree: usize,
    nofill: usize,
}

fn growth_verb(config: Option<&Path>, a: &GrowthArgs) -> Result<Outcome> {
    let ctx = Ctx::load_with(config, &a.image.image, &a.functions)?;
    let funclet = funclet_test(&a.funclet);
    let (text, text_rva) = ctx.text();
    let entries = ctx.entries();
    if entries.len() < 2 {
        bail!(
            "{}: the function index gives {} usable `.text' extent(s); a census over that would report on nothing",
            ctx.env.label,
            entries.len()
        );
    }
    let c = growth::census(entries, text, text_rva, &funclet);
    let base = ctx.base();
    let mut lines = vec![format!(
        "{} `.text': DOES THE REFERENCE RESERVE GROWTH ROOM AFTER EVERY FUNCTION?",
        ctx.env.label
    )];
    lines.push(format!(
        "  {} recorded extent(s) read from the function index, never from 0xcc fill",
        c.entries.len()
    ));
    if c.absorbed > 0 {
        lines.push(format!(
            "  {} funclet(s) absorbed into the function before them rather than counted as boundaries",
            c.absorbed
        ));
    }
    lines.extend(c.count_lines());
    lines.push(
        "  MIXED POPULATION: own code and archive members together answer oppositely; read --ours before quoting a \
         percentage."
            .to_string(),
    );
    if let Some(want) = &a.list {
        lines.extend(c.list_lines(want, base).map_err(anyhow::Error::msg)?);
    }
    lines.extend(c.cross_lines(base));
    lines.extend(c.first_lines(base));
    if let Some(m) = &a.rdata {
        let map = read_map(m)?;
        let objs = ctx.objects(&a.objs)?;
        let by_name: BTreeMap<String, PathBuf> = objs
            .iter()
            .filter_map(|p| {
                Some((
                    p.file_name()?.to_string_lossy().to_ascii_lowercase(),
                    p.clone(),
                ))
            })
            .collect();
        let raw = |obj: &str| -> u64 {
            let Some(p) = by_name.get(&obj.to_ascii_lowercase()) else {
                return 0;
            };
            let Ok(d) = bounded::read(p, MAX_OBJECT) else {
                return 0;
            };
            let Ok(o) = msvc6_image::coff::Object::parse(&d) else {
                return 0;
            };
            let secs: Vec<(String, u64)> = o
                .sections
                .iter()
                .map(|s| (s.name.clone(), u64::from(s.size)))
                .collect();
            growth::rdata_total(&secs)
        };
        let starts = growth::rdata_starts(&map);
        if starts.is_empty() {
            bail!(
                "{}: no `.rdata' symbol of an explicit object, so there is nothing to measure",
                m.display()
            );
        }
        lines.extend(growth::rdata_lines(&starts, &raw));
    }
    if let Some(m) = &a.ours {
        let map = read_map(m)?;
        let (ours, theirs) = growth::ours_and_theirs(&map);
        if ours.is_empty() {
            bail!(
                "{}: no explicit-object `.text' symbol, so every pair would classify UNKNOWN",
                m.display()
            );
        }
        lines.extend(c.split_lines(&m.display().to_string(), &ours, &theirs, base));
    }
    if let Some(b) = &a.band {
        lines.extend(c.band_lines(parse_rva(b)?, base));
    }
    lines.extend(c.run_lines(base));
    if let Some(t) = &a.library {
        let table = ctx.table(Some(t))?;
        let placed = member_points(&table);
        if placed.is_empty() {
            bail!("{}: the table places no member", t.display());
        }
        lines.extend(growth::by_member(&c.pairs, &placed).lines());
    }
    if let Some(cand) = &a.occupancy {
        let img = Image::parse(bounded::read(cand, MAX_IMAGE)?)
            .with_context(|| cand.display().to_string())?;
        let sec = img
            .section(".text")
            .with_context(|| format!("{}: no .text section", cand.display()))?;
        let s = growth::Sections {
            reference: text,
            reference_rva: text_rva,
            ours: img.section_bytes(sec),
            ours_rva: sec.rva.get(),
        };
        lines.extend(growth::occupancy_lines(
            &c,
            &s,
            base,
            &cand.display().to_string(),
        ));
    }
    if let Some(m) = &a.spacing {
        let map = read_map(m)?;
        let (refs, used) = spacing_refs(&ctx, &a.names)?;
        if refs.is_empty() {
            bail!(
                "{}: the name oracle ({}) names nothing in `.text', so every pair would be excluded",
                ctx.env.label,
                used.join(", ")
            );
        }
        let ours = growth::publics_by_object(&map, map.load);
        let mut v = growth::spacing(&ours, &refs).lines(&ctx.env.label, &m.display().to_string());
        v.insert(
            3.min(v.len()),
            format!(
                "  REFERENCE: {} name(s) in `.text' from {}",
                refs.len(),
                used.join(", ")
            ),
        );
        lines.extend(v);
    }
    say(&lines);
    let s = GrowthSummary {
        entries: c.entries.len(),
        pairs: c.pairs.len(),
        absorbed: c.absorbed,
        thunks: c.thunks,
        agree: c.agree,
        disagree: c.disagree,
        nofill: c.nofill,
    };
    json(
        &a.json,
        "placement growth",
        &ctx.env.label,
        Some(&s),
        &c.pairs,
    )?;
    Ok(Outcome::Ok)
}

fn spacing_refs(ctx: &Ctx, only: &[PathBuf]) -> Result<(BTreeMap<String, u32>, Vec<String>)> {
    let r = ctx.p.reference(ctx.n)?;
    let o = msvc6_project::names::oracle_of(&ctx.p, ctx.n, &r)?;
    let names = msvc6_diff::resolve::Names::from_oracle(&o);
    let (text, text_rva) = ctx.text();
    let code = u64::from(text_rva)..u64::from(text_rva) + text.len() as u64;
    let body = |rva: u32| -> u32 {
        msvc6_diff::resolve::ilt_slot(text, text_rva, rva)
            .filter(|t| {
                t.checked_sub(text_rva)
                    .and_then(|o| text.get(usize::try_from(o).ok()?))
                    != Some(&0xe9)
            })
            .unwrap_or(rva)
    };
    let mut refs: BTreeMap<String, u32> = BTreeMap::new();
    let mut keep = |n: &String, rva: u32| {
        let at = body(rva);
        if code.contains(&u64::from(at)) {
            refs.entry(n.clone()).or_insert(at);
        }
    };
    let mut used = Vec::new();
    if only.is_empty() {
        let mut e: Vec<(&String, &u32)> = names.exports().iter().collect();
        e.sort();
        for (n, rva) in e {
            keep(n, *rva);
        }
        used.push("exports".to_string());
    }
    for t in names.tiers() {
        if !only.is_empty() && !only.iter().any(|f| Path::new(t.source).ends_with(f)) {
            continue;
        }
        let mut flat: Vec<(&String, &u32)> = t.flat.iter().collect();
        flat.sort();
        for (n, rva) in flat {
            keep(n, *rva);
        }
        used.push(t.source.to_string());
    }
    if used.is_empty() {
        bail!(
            "no name source of image `{}` ends with {}",
            ctx.env.label,
            only.iter()
                .map(|p| p.display().to_string())
                .collect::<Vec<_>>()
                .join(" or ")
        );
    }
    Ok((refs, used))
}

fn archives_of(ctx: &Ctx, given: &[PathBuf]) -> Vec<PathBuf> {
    if given.is_empty() {
        ctx.libraries()
    } else {
        given.to_vec()
    }
}

fn archive_texts(paths: &[PathBuf]) -> Result<Vec<memberorder::ArchiveText>> {
    paths
        .iter()
        .map(|p| {
            let stem = p
                .file_stem()
                .map(|s| s.to_string_lossy().into_owned())
                .unwrap_or_default();
            let d = bounded::read(p, MAX_ARCHIVE).with_context(|| p.display().to_string())?;
            memberorder::archive_text(&stem, &d).with_context(|| p.display().to_string())
        })
        .collect()
}

fn member_points(table: &msvc6_match::table::Table) -> Vec<(u32, String)> {
    let point = |p: &msvc6_match::table::PointRow| (p.rva, memberorder::member_stem(&p.member));
    table
        .import
        .iter()
        .map(point)
        .chain(
            table
                .member
                .iter()
                .map(|m| (m.rva, memberorder::member_stem(&m.member))),
        )
        .chain(table.byname.iter().map(point))
        .collect()
}

fn placed_rows(table: &msvc6_match::table::Table) -> Vec<memberorder::Placed> {
    table
        .member
        .iter()
        .map(|m| memberorder::Placed {
            rva: m.rva,
            len: m.len,
            member: m.member.clone(),
            archive: m.archive.clone(),
        })
        .collect()
}

fn member_order(config: Option<&Path>, a: &MemberOrderArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let table = ctx.table(a.library.as_deref())?;
    let rows = placed_rows(&table);
    if rows.is_empty() {
        bail!("the library table records no placed member, so there is no reference sequence");
    }
    let (text, text_rva) = ctx.text();
    let archives = archive_texts(&archives_of(&ctx, &a.archive))?;
    let shared = memberorder::shared_rows(&rows, text, text_rva, &archives);
    let refseq = memberorder::reference_sequence(&rows, &shared);
    let path = ctx.map_path(a.map.as_deref())?;
    let map = read_map(&path)?;
    let keys = Keys::new(&a.own);
    let ours = memberorder::map_order(&map, &keys);
    if ours.is_empty() {
        bail!(
            "{} names no archive member: a LINK 6.00 /MAP puts `libname:member.obj' in the last column of its \
             public-symbol lines, so reporting zero as a RESULT is refused",
            path.display()
        );
    }
    let window = a
        .window
        .as_deref()
        .map(memberorder::parse_window)
        .transpose()?;
    let (ag, lines) = memberorder::report(
        &memberorder::Inputs {
            label: &ctx.env.label,
            table: &a
                .library
                .as_ref()
                .map_or_else(String::new, |p| p.display().to_string()),
            map: &path.display().to_string(),
            refseq: &refseq,
            ours: &ours,
        },
        &map,
        &memberorder::Views {
            misplaced: a.misplaced,
            sizes: a.sizes,
            seats: a.seats,
            window,
            rows: &rows,
        },
    );
    say(&lines);
    say(&[
        "  A DATA-ONLY MEMBER CANNOT APPEAR ON THE REFERENCE SIDE: the table places members by their `.text' bytes, \
         so members our link pulls that hold no code are absent rather than excess."
            .to_string(),
    ]);
    json(
        &a.json,
        "placement member-order",
        &ctx.env.label,
        Some(&ag),
        &rows,
    )?;
    Ok(Outcome::Ok)
}

type Refs = Vec<(String, Vec<String>)>;

fn object_refs(objs: &[PathBuf]) -> Result<(Refs, BTreeSet<String>)> {
    let mut refs = Vec::new();
    let mut defs = BTreeSet::new();
    for p in objs {
        let Some(name) = p.file_name().and_then(|x| x.to_str()) else {
            continue;
        };
        let d = bounded::read(p, MAX_OBJECT).with_context(|| p.display().to_string())?;
        let Ok(o) = msvc6_image::coff::Object::parse(&d) else {
            continue;
        };
        refs.push((name.to_string(), msvc6_link::symbols::undefined_ordered(&o)));
        defs.extend(msvc6_link::symbols::defined_externals(&o));
    }
    Ok((refs, defs))
}

#[allow(clippy::too_many_lines)]
fn pull_order(config: Option<&Path>, a: &PullOrderArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let table = ctx.table(a.library.as_deref())?;
    let rows = placed_rows(&table);
    let (text, text_rva) = ctx.text();
    let archive_paths = archives_of(&ctx, &a.archive);
    let archives = archive_texts(&archive_paths)?;
    let shared = memberorder::shared_rows(&rows, text, text_rva, &archives);
    let refseq = memberorder::reference_sequence(&rows, &shared);
    let objs = ctx.objects(&a.objs)?;
    let (mut refs, mut defs) = object_refs(&objs)?;
    if refs.is_empty() {
        bail!(
            "no readable `.obj' under {:?}: a prediction over zero objects would print a perfect score",
            a.objs
        );
    }
    let path = ctx.map_path(a.map.as_deref())?;
    let map = read_map(&path)?;
    let keys = Keys::new(&a.own);
    let seen = pullorder::map_object_order(&map, &keys);
    refs.sort_by_key(|(n, _)| {
        seen.iter()
            .position(|s| s.eq_ignore_ascii_case(n))
            .unwrap_or(usize::MAX)
    });
    let actual = memberorder::map_order(&map, &keys);
    if !a.closure {
        let bytes: Vec<Vec<u8>> = archive_paths
            .iter()
            .map(|p| bounded::read(p, MAX_ARCHIVE).with_context(|| p.display().to_string()))
            .collect::<Result<_>>()?;
        let slices: Vec<&[u8]> = bytes.iter().map(Vec::as_slice).collect();
        let arch = pullorder::definers(&slices)?;
        let (d, lines) = pullorder::direct(&pullorder::DirectInputs {
            label: &ctx.env.label,
            objdir: &format!("{:?}", a.objs),
            refs: &refs,
            arch: &arch,
            refseq: &refseq,
            actual: &actual,
            trace: a.trace,
            objects: &a.object,
        });
        say(&lines);
        json(
            &a.json,
            "placement pull-order",
            &ctx.env.label,
            Some(&d),
            &refs,
        )?;
        return Ok(Outcome::Ok);
    }
    let own: Vec<PathBuf> = objs
        .iter()
        .filter(|p| {
            p.file_name()
                .and_then(|x| x.to_str())
                .is_some_and(|n| !refs.iter().any(|(r, _)| r.eq_ignore_ascii_case(n)))
        })
        .cloned()
        .collect();
    let cmdline: BTreeSet<String> = objs
        .iter()
        .filter(|p| !own.contains(p))
        .filter_map(|p| Some(p.file_name()?.to_str()?.to_ascii_lowercase()))
        .collect();
    let mut members: Vec<pullorder::LibMember> = Vec::new();
    let own_libs: Vec<PathBuf> = a
        .own
        .iter()
        .filter_map(|lib| {
            objs.iter()
                .find(|p| {
                    p.parent().is_some_and(|d| {
                        d.ancestors()
                            .any(|x| x.file_name().and_then(|n| n.to_str()) == Some(lib.as_str()))
                    })
                })
                .cloned()
        })
        .collect();
    let _ = own_libs;
    for (k, p) in archive_paths.iter().enumerate() {
        let d = bounded::read(p, MAX_ARCHIVE).with_context(|| p.display().to_string())?;
        members.extend(pullorder::archive_members(k, &d).with_context(|| p.display().to_string())?);
    }
    if members.is_empty() {
        bail!("no archive member could be read, so the closure cannot be modelled");
    }
    let entry = match &a.entry {
        Some(e) => e.clone(),
        None => {
            pullorder::default_entry(ctx.env.img.is_dll(), ctx.env.img.opt.subsystem).to_string()
        }
    };
    refs = pullorder::with_entry(refs, &cmdline, &entry);
    let eager: BTreeSet<String> = a.eager.iter().map(|e| e.to_ascii_lowercase()).collect();
    let window = a
        .window
        .as_deref()
        .map(memberorder::parse_window)
        .transpose()?;
    defs.extend(std::iter::once(entry));
    let (r, lines) = pullorder::closure_report(&pullorder::ClosureInputs {
        label: &ctx.env.label,
        map: &path.display().to_string(),
        refs: &refs,
        obj_defs: &defs,
        members: &members,
        eager: &eager,
        refseq: &refseq,
        actual: &actual,
        window,
    });
    say(&lines);
    json(
        &a.json,
        "placement pull-order",
        &ctx.env.label,
        Some(&r),
        &refs,
    )?;
    Ok(Outcome::Ok)
}

fn layout_objs(ctx: &Ctx, order: &[String], objs: &[PathBuf]) -> Result<Vec<layout::Obj>> {
    let by_name: BTreeMap<String, PathBuf> = objs
        .iter()
        .filter_map(|p| {
            Some((
                p.file_name()?.to_string_lossy().to_ascii_lowercase(),
                p.clone(),
            ))
        })
        .collect();
    let mut out = Vec::new();
    for name in order {
        let Some(p) = by_name.get(&name.to_ascii_lowercase()) else {
            continue;
        };
        let (comdats, members) = msvc6_diff::seat::derive(&ctx.env, p)?;
        out.push(layout::Obj {
            name: name.clone(),
            comdats: members
                .iter()
                .zip(&comdats)
                .map(|(m, c)| {
                    (
                        m.name.clone(),
                        m.rva,
                        u64::try_from(c.bytes.len()).unwrap_or(0),
                    )
                })
                .collect(),
            statics: members
                .iter()
                .filter(|m| !m.external)
                .map(|m| m.name.clone())
                .collect(),
        });
    }
    Ok(out)
}

fn layout_verb(config: Option<&Path>, a: &LayoutArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let path = ctx.map_path(a.map.as_deref())?;
    let map = read_map(&path)?;
    let (order, mapaddr) = layout::from_map(&map);
    if order.is_empty() {
        bail!(
            "{} names no own object in a code section, so every control below would pass vacuously",
            path.display()
        );
    }
    let objs = layout_objs(&ctx, &order, &ctx.objects(&a.objs)?)?;
    let (v, lines) = layout::report(&layout::Report {
        label: &ctx.env.label,
        order: &order,
        objs: &objs,
        mapaddr: &mapaddr,
        image_base: ctx.base(),
        search: a.search,
    });
    say(&lines);
    json(
        &a.json,
        "placement layout",
        &ctx.env.label,
        None::<&()>,
        &order,
    )?;
    Ok(match v {
        layout::Verdict::Modelled => Outcome::Ok,
        _ => Outcome::Differs,
    })
}

fn slots_verb(config: Option<&Path>, a: &SlotsArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let ours = Image::parse(bounded::read(&a.ours, MAX_IMAGE)?)
        .with_context(|| a.ours.display().to_string())?;
    let (comdats, members) = msvc6_diff::seat::derive(&ctx.env, &a.obj)?;
    let rows: Vec<slots::Member> = members
        .iter()
        .zip(&comdats)
        .map(|(m, c)| {
            let size = u64::try_from(c.bytes.len()).unwrap_or(0);
            slots::Member {
                name: m.name.clone(),
                size,
                reference: m.rva,
                ours: m.rva.and_then(|r| {
                    msvc6_match::bodies::find_unique(&ctx.env.img, &ours, r, c.bytes.len())
                }),
            }
        })
        .collect();
    let known: BTreeSet<u32> = ctx.index.iter().map(|f| f.rva).collect();
    let lines = slots::report(&a.obj.display().to_string(), &rows, &known);
    say(&lines);
    let fits = slots::fits(&rows);
    json(
        &a.json,
        "placement slots",
        &ctx.env.label,
        Some(&slots::excess(&rows, &known)),
        &fits
            .iter()
            .map(|(f, _, _, n)| (n.clone(), f.label()))
            .collect::<Vec<_>>(),
    )?;
    Ok(Outcome::Ok)
}

fn holes_verb(config: Option<&Path>, a: &HolesArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let cand = Image::parse(bounded::read(&a.candidate, MAX_IMAGE)?)
        .with_context(|| a.candidate.display().to_string())?;
    let csec = cand
        .section(".text")
        .with_context(|| format!("{}: no .text section", a.candidate.display()))?;
    let map = a.map.as_deref().map(read_map).transpose()?;
    let (text, text_rva) = ctx.text();
    let placed = a
        .library
        .as_deref()
        .map(|p| ctx.table(Some(p)))
        .transpose()?
        .and_then(|t| t.member.iter().map(|m| m.rva).min());
    let ref_end = placed
        .or_else(|| {
            ctx.index
                .iter()
                .filter(|f| f.object.as_ref().is_some_and(|o| o.contains(':')))
                .map(|f| f.rva)
                .min()
        })
        .unwrap_or_else(|| text_rva.saturating_add(ctx.env.section().virt_size.get()));
    let our_end = map.as_ref().and_then(holes::own_end).map_or(ref_end, |e| {
        csec.rva.get().saturating_add(e.saturating_sub(text_rva))
    });
    let mut members: Vec<(String, u32, u64)> = Vec::new();
    if !a.objs.is_empty() {
        for p in ctx.objects(&a.objs)? {
            let (comdats, ms) = msvc6_diff::seat::derive(&ctx.env, &p)?;
            for (m, c) in ms.iter().zip(&comdats) {
                if let Some(r) = m.rva {
                    members.push((m.name.clone(), r, u64::try_from(c.bytes.len()).unwrap_or(0)));
                }
            }
        }
    }
    let own = holes::Own::of(&members);
    let layout = map.as_ref().map(holes::our_layout).unwrap_or_default();
    let (c, lines) = holes::report(&holes::Input {
        label: &ctx.env.label,
        candidate: &a.candidate.display().to_string(),
        reference: holes::Region {
            bytes: text,
            base: text_rva,
            end: ref_end,
        },
        ours: holes::Region {
            bytes: cand.section_bytes(csec),
            base: csec.rva.get(),
            end: our_end,
        },
        end_from_map: map.is_some(),
        min: a.min,
        own: &own,
        layout: &layout,
        show: holes::Listing {
            list: a.list,
            all: a.all,
        },
    });
    say(&lines);
    json(
        &a.json,
        "placement holes",
        &ctx.env.label,
        Some(&c),
        &[0u8; 0],
    )?;
    Ok(Outcome::Ok)
}

fn library_name(dir: &Path) -> Option<String> {
    dir.ancestors()
        .filter_map(|d| d.file_name().and_then(|n| n.to_str()))
        .find(|n| n.to_ascii_lowercase().ends_with(".lib"))
        .map(str::to_string)
}

fn dsp_beside(log: &Path) -> Option<PathBuf> {
    let dir = log.parent()?;
    let mut hits: Vec<PathBuf> = std::fs::read_dir(dir)
        .ok()?
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x.eq_ignore_ascii_case("dsp")))
        .collect();
    hits.sort();
    hits.into_iter().next()
}

fn link_switches(a: &DeadDataArgs, log: &str) -> Result<(String, String)> {
    if let Some(s) = &a.link {
        return Ok(("--link".to_string(), s.clone()));
    }
    let dsp = dsp_beside(&a.log).with_context(|| {
        format!(
            "{}: no `.dsp' beside the log; name the switches with --link",
            a.log.display()
        )
    })?;
    let cfg = deaddata::last_configuration(log)
        .with_context(|| format!("{}: no `Configuration:' line", a.log.display()))?;
    let text = bounded::source(&dsp, MAX_RSP).with_context(|| dsp.display().to_string())?;
    let line = deaddata::link_line(&text, &cfg)
        .with_context(|| format!("{}: no `# ADD LINK32' line for {cfg}", dsp.display()))?;
    Ok((cfg, line))
}

fn dead_data(config: Option<&Path>, a: &DeadDataArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let text = bounded::tool(&a.log, MAX_LOG).with_context(|| a.log.display().to_string())?;
    let (cfg, switches) = link_switches(a, &text)?;
    if !deaddata::opt_ref(&switches) {
        bail!(
            "{cfg}: LINK does not run /OPT:REF under `{switches}' (/DEBUG or /INCREMENTAL:YES without /OPT:REF): \
             every COMDAT is kept, so no datum's reader was discarded and the census does not apply"
        );
    }
    if !deaddata::is_verbose_log(&text) {
        bail!(
            "{}: not a LINK /VERBOSE log (`msvc6t build --link-env /VERBOSE' writes one)",
            a.log.display()
        );
    }
    let log = deaddata::parse_log(&text);
    if a.objs.is_empty() {
        bail!("name the objects: --objs DIR, the command line's first and each library's after it");
    }
    let mut objs = Vec::new();
    for (k, dir) in a.objs.iter().enumerate() {
        let lib = library_name(dir);
        for p in msvc6_diff::objs::objs_in(dir)? {
            let Some(name) = p.file_name().and_then(|x| x.to_str()) else {
                continue;
            };
            let spelled = match (k, &lib) {
                (0, _) => name.to_string(),
                (_, Some(l)) if deaddata::loaded_members(&log, l, name) => format!("{l}({name})"),
                (_, Some(_)) => continue,
                (_, None) => name.to_string(),
            };
            let d = bounded::read(&p, MAX_OBJECT).with_context(|| p.display().to_string())?;
            objs.push(deaddata::read_object(&spelled, &d, &log)?);
        }
    }
    if objs.is_empty() {
        bail!(
            "no object of the link could be read: --objs names the command line's and the loaded members'"
        );
    }
    let census = deaddata::census(&objs, a.all);
    let map = a.map.as_deref().map(read_map).transpose()?;
    let vas = map.as_ref().map(deaddata::map_vas).unwrap_or_default();
    let bases = deaddata::section_bases(&objs, &vas);
    let spans: Vec<Option<(u32, u32)>> = census
        .rows
        .iter()
        .map(|r| deaddata::row_va(r, &vas, &bases).map(|va| (va, r.size)))
        .collect();
    let hits = deaddata::reference_hits(&spans, ctx.env.img.bytes());
    let map_label = a.map.as_ref().map(|p| p.display().to_string());
    let report = deaddata::Report {
        label: &ctx.env.label,
        config: &cfg,
        log: &a.log.display().to_string(),
        map: map_label.as_deref(),
        objs: &objs,
        census: &census,
        hits: &hits,
    };
    say(&report.lines());
    json(
        &a.json,
        "placement dead-data",
        &ctx.env.label,
        None::<&()>,
        &census.rows,
    )?;
    Ok(Outcome::Ok)
}

fn comdats_verb(config: Option<&Path>, a: &ComdatsArgs) -> Result<Outcome> {
    let p = project::load(config)?;
    let label = match p.as_ref().and_then(|c| c.image(&a.image.image)) {
        Some((_, d)) => d.id.clone(),
        None => a.image.image.clone(),
    };
    let text = bounded::source(&a.rsp, MAX_RSP).with_context(|| a.rsp.display().to_string())?;
    let names = census::rsp_objects(&text);
    if names.is_empty() {
        bail!(
            "{}: no `.obj' operand found, so there is no population to census",
            a.rsp.display()
        );
    }
    let dir = a.rsp.parent().unwrap_or(Path::new("."));
    let mut missing: Vec<String> = Vec::new();
    let mut objects: Vec<(String, Vec<u8>)> = Vec::new();
    for name in &names {
        match census::resolve_obj(dir, name) {
            Some(p) if p.is_file() => {
                objects.push((name.clone(), bounded::read(&p, MAX_OBJECT)?));
            }
            Some(p) => missing.push(format!("{name} -> {}", p.display())),
            None => missing.push(format!(
                "{name} -> no wine drive mapping above {}",
                dir.display()
            )),
        }
    }
    if !missing.is_empty() {
        bail!(
            "{}: {} of {} `.obj' operand(s) do not resolve to a file:\n  {}",
            a.rsp.display(),
            missing.len(),
            names.len(),
            missing
                .iter()
                .take(8)
                .cloned()
                .collect::<Vec<_>>()
                .join("\n  ")
        );
    }
    let classifier = census::Classifier {
        rules: a
            .classes
            .iter()
            .map(|s| census::parse_rule(s))
            .collect::<Result<_>>()?,
        fallback: a.fallback.clone(),
    };
    let c = census::census(&objects, &classifier);
    say(&c.lines(&label, &a.rsp.display().to_string()));
    json(
        &a.json,
        "placement comdats",
        &label,
        Some(&c.total()),
        &c.rows,
    )?;
    Ok(Outcome::Ok)
}

type Bodies = BTreeMap<String, Vec<owner::Body>>;

type Definers = BTreeMap<String, BTreeSet<String>>;

#[derive(Debug, Default)]
struct Own {
    per: Bodies,
    defined_by: Definers,
    chain: Vec<(u32, u64)>,
}

fn own_bodies(ctx: &Ctx, objs: &[PathBuf]) -> Result<Own> {
    let mut per: BTreeMap<String, Vec<owner::Body>> = BTreeMap::new();
    let mut defined_by: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut seen: BTreeSet<String> = BTreeSet::new();
    let mut chain: Vec<(u32, u64)> = Vec::new();
    for p in objs {
        let stem = msvc6_diff::seat::scope_stem(p);
        let (comdats, members) = msvc6_diff::seat::derive(&ctx.env, p)?;
        for (m, c) in members.iter().zip(&comdats) {
            let Some(rva) = m.rva else { continue };
            defined_by
                .entry(stem.clone())
                .or_default()
                .insert(m.name.clone());
            if !seen.insert(m.name.clone()) {
                continue;
            }
            let size = u64::try_from(c.bytes.len()).unwrap_or(0);
            chain.push((rva, size));
            per.entry(stem.clone()).or_default().push(owner::Body {
                name: m.name.clone(),
                rva,
                size,
            });
        }
    }
    for v in per.values_mut() {
        v.sort_by_key(|b| b.rva);
    }
    Ok(Own {
        per,
        defined_by,
        chain,
    })
}

fn defined_in_cpp(ctx: &Ctx) -> Result<BTreeSet<String>> {
    let specs = msvc6_project::spec::resolve(&ctx.p, ctx.n)?;
    let mut out = BTreeSet::new();
    for s in &specs.specs {
        let Ok(src) = bounded::read(Path::new(&s.path), MAX_OBJECT) else {
            continue;
        };
        let lexed = msvc6_source::lex::lex(&src);
        out.extend(
            msvc6_source::defs::definitions(&lexed)
                .into_iter()
                .map(|d| d.base),
        );
    }
    Ok(out)
}

fn owner_verb(config: Option<&Path>, a: &OwnerArgs) -> Result<Outcome> {
    let ctx = Ctx::load(config, &a.image.image)?;
    let objs = ctx.objects(&a.objs)?;
    let own = own_bodies(&ctx, &objs)?;
    let (per, defined_by, chain) = (own.per, own.defined_by, own.chain);
    if per.is_empty() {
        bail!("no object of {:?} seats a body in the reference", a.objs);
    }
    let archived: BTreeSet<String> = match a.library.as_deref() {
        Some(p) => {
            let t = msvc6_match::table::Table::read(p)?;
            t.import
                .iter()
                .chain(&t.byname)
                .map(|r| r.name.clone())
                .collect()
        }
        None => BTreeSet::new(),
    };
    let classes: Vec<String> = if a.vendor_class.is_empty() {
        owner::VENDOR_CLASSES
            .iter()
            .map(|s| (*s).to_string())
            .collect()
    } else {
        a.vendor_class.clone()
    };
    let objects: Vec<String> = per.keys().cloned().collect();
    let lines = owner::report(&owner::Input {
        label: &ctx.env.label,
        per: &per,
        defined_by: &defined_by,
        objects: &objects,
        relocated_by_fill: &owner::fill_bodies(&chain),
        archived: &archived,
        vendor_classes: &classes,
        defined_in_cpp: &defined_in_cpp(&ctx)?,
        chain_at: a.chain.as_deref().map(parse_rva).transpose()?,
    });
    say(&lines);
    let spans: Vec<owner::Span> = per
        .iter()
        .filter_map(|(o, b)| owner::span_of(o, b))
        .collect();
    json(
        &a.json,
        "placement owner",
        &ctx.env.label,
        None::<&()>,
        &spans,
    )?;
    Ok(Outcome::Ok)
}

fn ilk_verb(a: &IlkArgs) -> Result<Outcome> {
    let b = bounded::read(&a.file, ilk::MAX_ILK).with_context(|| a.file.display().to_string())?;
    let h = ilk::header(&b).with_context(|| a.file.display().to_string())?;
    let mut lines = vec![
        a.file.display().to_string(),
        format!(
            "  signature OK, version {}; mapping base {:#x} derived from +0x1d4 = {:#x}",
            h.version, h.base, h.self_ptr
        ),
        format!("  {} bytes", b.len()),
        String::new(),
    ];
    if !a.find.is_empty() {
        let values: Vec<u32> = a
            .find
            .iter()
            .map(|s| {
                u32::from_str_radix(s.trim_start_matches("0x"), 16)
                    .with_context(|| format!("--find {s}: not a hex dword"))
            })
            .collect::<Result<_>>()?;
        lines.extend(ilk::find_lines(&b, &values));
        lines.push(
            "  A CONSTANT gap across several known rvas would evidence an array of that stride; one hit, or ragged \
             gaps, evidences nothing."
                .to_string(),
        );
    }
    if let Some(m) = &a.map {
        let map = read_map(m)?;
        let known: std::collections::BTreeSet<u32> = map
            .rows
            .iter()
            .filter_map(|r| r.va.checked_sub(map.load))
            .collect();
        let (longest, walk) = ilk::walk_lines(&b, &known);
        lines.extend(walk);
        say(&lines);
        json(
            &a.json,
            "placement ilk",
            &a.file.display().to_string(),
            None::<&()>,
            &[longest],
        )?;
        return Ok(Outcome::Ok);
    }
    let rs = ilk::section_records(&b, h.base);
    let truth: Vec<(String, u32, u32)> = match &a.image {
        Some(p) => Image::parse(bounded::read(p, MAX_IMAGE)?)
            .with_context(|| p.display().to_string())?
            .sections
            .iter()
            .map(|s| (s.name.clone(), s.rva.get(), s.virt_size.get()))
            .collect(),
        None => Vec::new(),
    };
    let (control, rows) = ilk::record_lines(&rs, &truth);
    lines.extend(rows);
    say(&lines);
    json(
        &a.json,
        "placement ilk",
        &a.file.display().to_string(),
        Some(&control),
        &rs.kept,
    )?;
    Ok(match control {
        ilk::Control::NoVerdict(_) => Outcome::Differs,
        _ => Outcome::Ok,
    })
}

#[cfg(test)]
mod tests {
    use super::{funclet_test, member_points};

    #[test]
    fn a_by_member_table_seats_its_point_rows_in_the_order_lib_locate_writes_them() {
        let t = msvc6_match::table::Table::parse(
            "[[member]]\nrva = 0x1000\nlen = 0x40\nmember = 'lib\\\\A.obj'\n\
             [[byname]]\nrva = 0x2000\nname = 'f'\nmember = 'b.obj'\n\
             [[import]]\nrva = 0x3000\nname = 'g'\nmember = 'c.obj'\n",
            std::path::Path::new("t.toml"),
        )
        .unwrap();
        let got = member_points(&t);
        assert_eq!(
            got,
            [
                (0x3000, "c.obj".to_string()),
                (0x1000, "a.obj".to_string()),
                (0x2000, "b.obj".to_string())
            ]
        );
    }

    #[test]
    fn no_name_is_a_funclet_unless_a_mark_is_given() {
        assert!(!funclet_test(&[])("nh_malloc_dbg_finally_UnlockHeap"));
        let marks = ["_finally_".to_string()];
        assert!(funclet_test(&marks)("nh_malloc_dbg_finally_UnlockHeap"));
        assert!(!funclet_test(&marks)("free_dbg"));
    }
}
