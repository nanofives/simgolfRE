use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::{Config, Resolved};
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_class::insns::{self, Function};
use msvc6_class::read;
use msvc6_class::slots::Rule;
use msvc6_class::vft::{self, Run, Sections, Text};
use msvc6_class::{declared, join, slotmap};
use msvc6_image::pe::Image;
use msvc6_project::config::FunctionSource;
use msvc6_project::names::Kind;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

fn parse_rva(s: &str) -> Result<u32, String> {
    let t = s.trim();
    t.strip_prefix("0x")
        .or_else(|| t.strip_prefix("0X"))
        .and_then(|h| u32::from_str_radix(h, 16).ok())
        .ok_or_else(|| format!("`{s}` is not a 0x-hexadecimal rva"))
}

#[derive(Debug, Args)]
pub struct ClassArgs {
    #[command(subcommand)]
    pub verb: ClassVerb,
}

#[derive(Debug, Subcommand)]
pub enum ClassVerb {
    #[command(
        about = "Every candidate vftable in an image's `.rdata', each slot followed through its ILT thunk to a body",
        long_about = "List every maximal run of 4-byte-aligned `.rdata' dwords that each point into `.text' once the \
                      image base is taken off: a CANDIDATE vftable (a jump table or an array of function pointers \
                      reads the same; corroborate a run before using it). Each slot is followed through one `e9 \
                      rel32' jump, so an incremental link's ILT thunk reads as the body it reaches; a slot that is \
                      already a body reads `(direct)'. The bodies a table's slots reach are one class, and often one \
                      translation unit. A maximal run is not a table: two tables with no separator read as one run. \
                      --stores splits each run at the slot addresses a decoded instruction writes (`c7 /0 imm32', a \
                      vptr install in a constructor or destructor, or `b8+r imm32'), and lists who writes each; an \
                      instruction in code the function index never decoded is invisible, and a missed start \
                      lengthens the table before it. The instructions are the image's ghidra-json function records \
                      when its function index has one, else every function of the index decoded from its entry. \
                      --table dumps one table (a run start or an attested start) slot by slot with the names the \
                      image's name oracle and function index give each body. --twin IMAGE:RVA pairs that table slot \
                      by slot with another image's and compares each body with every address masked. --candidate \
                      compares a linked image's runs with the reference's by length multiset and by the slot total \
                      over all runs, which merging two adjacent tables does not change. --owed ranks every \
                      store-split table by the distinct bodies (purecalls excluded) no object of --objs defines \
                      under a name the image's names place at that body, a body inside a library member's span of \
                      --library counting as defined. CONTROL rows are [image.classes] `tables`: tables resolved by \
                      hand; a detector that misses one is not to be read. JSON rows: one per table printed."
    )]
    Vftables(VftablesArgs),
    #[command(
        about = "Which reference vftable each declared class owns, joined by the names its slots carry, and every slot that disagrees",
        long_about = "Read every class defined under the image's [image.classes] `sources' (`class X ... {` or \
                      `struct X ... {` at the start of a line; a forward declaration is no definition) and lay out \
                      its vftable's slot names by MSVC6's rules: the primary base's slots first (the first \
                      non-virtual base that has a table, resolved in the declaring file first), a member matching a \
                      slot by name and parameter types overriding it in place whether or not it says `virtual', a \
                      member overriding a slot of another base or of a virtual base taking none here, and each new \
                      virtual appended by name group: every member function of one name forms a group placed at \
                      the name's first declaration in the class, its new virtuals in reverse declaration order; a \
                      destructor overrides the base's whatever it is spelt and is `~'. A one-parameter macro whose \
                      body declares virtuals is expanded; a class that invokes another macro at class scope is not \
                      statable. Each class is joined to the candidate table offset (every slot of every run of \
                      `class vftables', bodies followed through ILT thunks) whose bodies the image's names place \
                      for the class or its bases at the most slots, assigned only when a slot names the class \
                      itself and no other offset ties; a `??_7' name the names place at one address fixes the table \
                      outright. A slot DISAGREES when its declared name is placed at another body, or its body is \
                      named as another method of the class or a base (a body several slots share is a folded stub \
                      and names nothing). The names are every name source of the image but the imports, an \
                      object-scoped row excluded; --names keeps only the sources whose path ends with FILE. A \
                      `/* ... 0xNNNN */' comment on a `virtual' declaration naming one address of four or more \
                      digits pins that slot. --map adds a LINK /MAP whose bodies sit at the reference's addresses. \
                      --legacy-slots lays the slots out by declaration order instead: each `virtual' declaration \
                      ending in `;' takes the next slot unless its name already holds one (overloads, bodies \
                      defined in the class and overrides not spelt `virtual' are not seen), for comparing with \
                      figures measured that way. --tsv writes `<table va> <class> <slot>...' rows, a slot `!name' \
                      when it agrees and its body is in no other slot, `name' when unpinned, `-' when it \
                      disagrees. JSON rows: one per assigned class."
    )]
    Join(JoinArgs),
    #[command(
        about = "Each declared class's implied vftable slot count against the image's candidate table lengths, and the copies of one class that disagree",
        long_about = "No byte score reports a slot count: a class declared with the wrong number of virtual members \
                      compiles and reads IDENTICAL at every address, because a vftable is `.rdata'. Read the \
                      classes under [image.classes] `sources' as `class join' does and report: each class \
                      defined in more than one file with each copy's virtual count (copies that disagree model \
                      different tables); the same member declared with different parameter types in two files \
                      (names stripped; a different count, or two signatures in one file, is an overload); the \
                      same class laid out differently (data members compared on type and order, bases first, the \
                      vptr first in a polymorphic copy, a copy that is a prefix of another not listed); each \
                      class's implied slot count under MSVC6's rules (see `class join') against every candidate \
                      run length of `class vftables'; the [image.classes] `counts' controls; and every function \
                      defined in two `.cpp' files (keyed on name and the arity of its whole parameter list, a \
                      `static' one, internal to its file, left out). A count that \
                      EXCEEDS every run is the only refusal: a table whose predecessor abuts it is no run of its \
                      own, so a shorter count is ADMISSIBLE inside a longer run, and a count below --min-slots \
                      cannot match. A class that invokes a macro at class scope is not counted. --legacy-slots \
                      reads the slots as `class join --legacy-slots' does, with no macro expanded and no \
                      [image.classes] `headers'. Exit 3 when a control is not reproduced. JSON summary: the \
                      counts and the controls."
    )]
    Slots(SlotsArgs),
    #[command(
        about = "Decode one candidate vftable slot by slot: arity, forward target, constant answer",
        long_about = "Read each slot of the candidate run at TABLE (a run start of `class vftables'), followed \
                      through its ILT thunk, and decode the body from its own bytes with a walker that refuses \
                      an opcode it does not know rather than guess: the arity from every `ret' (exits that \
                      disagree are SUSPECT), a forward (a vptr load, then `call' or `jmp' through it) and the slot \
                      it targets (the displacement over four), the no-child answer after it (`33 c0' int zero, \
                      `32 c0' a BYTE zero, `83 c8 ff' -1, `b8 imm32' a constant), and whether the forwarded result \
                      is discarded (one `ret' reached on every path). A body that also calls or stores, or \
                      dispatches more than once, is a REAL BODY. A body that reads its object from its first \
                      stack argument rather than ECX is a __stdcall virtual, marked `this on the stack': its \
                      `ret' counts `this'. A body is read to its first run of four fill bytes, at most 96 bytes. \
                      --base RVA marks each slot `inh' (the base's own body), `ovr' or `new'. JSON rows: one per \
                      slot."
    )]
    SlotMap(SlotMapArgs),
    #[command(
        about = "Does each class member's declaration agree with its definition?",
        long_about = "Read every `.cpp' and `.h' under [image.classes] `sources' into logical lines (a wrapped \
                      parameter list joined, several declarations on one line split at `;') and collect each \
                      member function's signature where a class body declares it and where an out-of-line \
                      `Class::member(...)' defines it: qualifiers a definition cannot repeat (`virtual', \
                      `static', `inline', `explicit', `friend', `extern'), default arguments, parameter names, \
                      `(void)', pointer spacing and array parameters (`int p[]' is `int *p') are no difference, \
                      and neither is a linkage macro (an object-like macro the sources define as \
                      `__declspec(...)' or as nothing) or `__declspec(...)' itself. A pair DISAGREES when a \
                      definition matches no declaration while a declaration of the same parameter count goes \
                      undefined: one function written two ways, not an overload. DECL vs DEFN is the strong case \
                      (the definition was compiled and scored; suspect the declaration), DECL vs DECL names two \
                      places to look. JSON rows: one per disagreeing pair."
    )]
    Sigs(SigsArgs),
    #[command(
        about = "Do the copies of one class in a set of sources agree, field for field? Offsets derived, never documented",
        long_about = "Read every class or struct defined at the start of a line with no base clause in the C and \
                      C++ files of DIR (each directory's files in path order; default the image's [image.classes] \
                      `sources') and derive each member's offset from declaration order and type size under \
                      /Zp8 (the scalar types, their Win32 names, CRITICAL_SECTION, any pointer, arrays with a \
                      literal or one `a - b' / `a + b' extent). A `virtual', a union, a nested aggregate, an enum, \
                      a typedef, a static member, a bitfield or one type name of unknown size DECLINES the whole \
                      layout, which is then never compared: a guessed origin would put every later field at a \
                      wrong offset. Two derived copies are compared at each shared offset, weakest first: AGREE \
                      (one may be a prefix of the other), UNNAMED (one name merely encodes the offset: `f5C', \
                      `pad1a'), OPAQUE (a pad run over named fields), SPLIT (contiguous fields that tile the other's \
                      one field), NAME CLASH (same offset and size, two real names), LAYOUT (another size at one \
                      offset: at most one copy is right about the bytes). `[project declares it]' marks a class \
                      [image.classes] `sources' define. A class whose name matches an [image.classes] \
                      `placeholders' pattern (`?' a single letter) is a placeholder, a receiver's stand-in, and is \
                      not compared. JSON rows: one per comparable class."
    )]
    Layout(LayoutArgs),
    #[command(
        about = "Which declared class owns every offset one reference body dereferences off `this'",
        long_about = "Decode LEN bytes at RVA and collect every displacement read or written through `this': the \
                      register holding it starts as ECX (__thiscall) or, for a body that never reads ECX and \
                      dereferences its first stack argument, as the register that argument is loaded into \
                      (__stdcall); --this ecx|stack decides it. A `mov' between registers carries `this', a call \
                      clobbers EAX, ECX and EDX, a `mov' or `lea' into a holder retargets it; ESP and EBP bases are \
                      never fields. A body whose `this' arrives biased (a read at a NEGATIVE displacement, then the \
                      words at +4k of what it loaded) is an override entered through a virtual base: the first \
                      negative read is the vbptr at object +0, the bias less four is the class's own data, \
                      vbtable[k] is virtual base k's offset and a read [vbtable[k] + this + d] is that base's member \
                      at d plus the bias; the offsets are then the object's. The classes are those under \
                      [image.classes] `sources' and `headers': a field a `/* +0xNN */' (or `/* +0xNN .. +0xMM */') \
                      comment documents, else every field derived from declaration order and type size (the vptr \
                      first in a polymorphic class with no base, the vbptr after it in a class whose bases are all \
                      virtual, non-virtual bases before the own fields, virtual bases after them, each base at a \
                      4-aligned end); a derivation stops at the first field it cannot size. Each class is ranked by \
                      the DISTINCT fields the offsets land on (an offset inside a documented aggregate of at most \
                      0x80 bytes counts for it), then by the offsets it cannot name. A tie is the answer, not a \
                      rounding problem. JSON summary: the report."
    )]
    Receiver(ReceiverArgs),
}

#[derive(Debug, Args)]
pub struct SigsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LayoutArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "DIR",
        help = "Directories of the copies to compare (default: [image.classes] sources)."
    )]
    pub dirs: Vec<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, clap::ValueEnum)]
pub enum ThisArg {
    Ecx,
    Stack,
}

#[derive(Debug, Args)]
pub struct ReceiverArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "RVA", value_parser = parse_rva, help = "The body's rva.")]
    pub rva: u32,
    #[arg(value_name = "LEN", value_parser = parse_rva, help = "The body's length, 0x-hexadecimal.")]
    pub len: u32,
    #[arg(
        long,
        value_enum,
        help = "Where `this' arrives (default: read off the body)."
    )]
    pub this: Option<ThisArg>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct SlotsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 2,
        help = "Ignore candidate runs shorter than N slots."
    )]
    pub min_slots: usize,
    #[arg(
        long,
        help = "Read the slots in declaration order, as `class join --legacy-slots` does, not by the MSVC6 rule."
    )]
    pub legacy_slots: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct SlotMapArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "TABLE", value_parser = parse_rva, help = "The run's rva, as `class vftables` prints it.")]
    pub table: u32,
    #[arg(long, value_name = "RVA", value_parser = parse_rva, help = "A base class's table.")]
    pub base: Option<u32>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct JoinArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        long,
        value_name = "FILE",
        help = "Read only the name sources whose path ends with FILE (repeatable)."
    )]
    pub names: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A LINK /MAP whose bodies sit at the reference's addresses."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        help = "Lay the slots out in declaration order, not by the MSVC6 rule."
    )]
    pub legacy_slots: bool,
    #[arg(long, value_name = "FILE", help = "Write the table rows here.")]
    pub tsv: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct VftablesArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(long, value_name = "RVA", value_parser = parse_rva, help = "Dump this table slot by slot.")]
    pub table: Option<u32>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 2,
        help = "Ignore runs shorter than N slots."
    )]
    pub min_slots: usize,
    #[arg(
        long,
        value_name = "FILE",
        help = "A linked image of ours: compare its runs with the reference's by length."
    )]
    pub candidate: Option<PathBuf>,
    #[arg(
        long,
        help = "Split the runs at the starts decoded instructions write."
    )]
    pub stores: bool,
    #[arg(
        long,
        requires = "stores",
        help = "With --stores: each row's slot bodies on a second line."
    )]
    pub bodies: bool,
    #[arg(
        long,
        requires = "objs",
        help = "Rank the store-split tables by the bodies no object of --objs defines."
    )]
    pub owed: bool,
    #[arg(
        long,
        value_name = "DIR",
        help = "With --owed: the objects of a build."
    )]
    pub objs: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "With --owed: a library table (the TOML `lib locate --record` writes) whose member spans supply bodies."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "IMAGE:RVA",
        requires = "table",
        help = "With --table: pair it with this table of another [[image]]."
    )]
    pub twin: Option<String>,
    #[arg(
        long,
        value_name = "NAME",
        default_value = vft::DATA,
        help = "The section the tables are read from."
    )]
    pub data_section: String,
    #[command(flatten)]
    pub json: JsonArg,
}

pub fn run(config: Option<&Path>, a: &ClassArgs) -> Result<Outcome> {
    match &a.verb {
        ClassVerb::Vftables(v) => vftables(config, v),
        ClassVerb::Join(v) => join(config, v),
        ClassVerb::Slots(v) => slots(config, v),
        ClassVerb::SlotMap(v) => slot_map(config, v),
        ClassVerb::Sigs(v) => sigs(config, v),
        ClassVerb::Layout(v) => layout(config, v),
        ClassVerb::Receiver(v) => receiver(config, v),
    }
}

fn rule(legacy: bool) -> Rule {
    if legacy { Rule::Legacy } else { Rule::Vc6 }
}

fn sigs(config: Option<&Path>, a: &SigsArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let files = pairs(l.read_dirs("sources", &["cpp", "h"])?);
    let r = msvc6_class::sigs::screen(&files, false);
    say(&msvc6_class::sigs::report(&r, &l.id));
    json(&a.json, "class sigs", &l.id, Some(&r), &r.disagreeing)?;
    Ok(Outcome::Ok)
}

pub const COPY_EXTENSIONS: [&str; 8] = ["c", "cc", "cpp", "cxx", "h", "hpp", "hxx", "inl"];

fn layout(config: Option<&Path>, a: &LayoutArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let mut copies = Vec::new();
    let population = if a.dirs.is_empty() {
        copies = pairs(l.read_dirs("sources", &COPY_EXTENSIONS)?);
        "[image.classes] sources".to_string()
    } else {
        for d in &a.dirs {
            for p in read::files(d, &COPY_EXTENSIONS)? {
                copies.push((p.display().to_string(), read::text(&p, read::MAX_SOURCE)?));
            }
        }
        a.dirs
            .iter()
            .map(|d| format!("`{}'", d.display()))
            .collect::<Vec<_>>()
            .join(", ")
    };
    let declared: std::collections::BTreeSet<String> =
        pairs(l.read_dirs("sources", &COPY_EXTENSIONS)?)
            .iter()
            .flat_map(|(_, t)| {
                msvc6_class::layout::declarations(t)
                    .into_iter()
                    .map(|(n, _, _)| n)
            })
            .collect();
    let placeholders =
        l.p.images
            .get(l.n)
            .and_then(|d| d.classes.as_ref())
            .map(|c| c.placeholders.clone())
            .unwrap_or_default();
    let r = msvc6_class::layout::screen(&copies, &declared, &placeholders);
    say(&msvc6_class::layout::report(&r, &l.id, &population));
    json(&a.json, "class layout", &l.id, Some(&r.declines), &r.rows)?;
    Ok(Outcome::Ok)
}

fn receiver(config: Option<&Path>, a: &ReceiverArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let text = Text::of(&l.img, vft::CODE)?;
    let len = usize::try_from(a.len).unwrap_or(usize::MAX);
    let Some(body) = text.at(a.rva, len).or_else(|| {
        let off = usize::try_from(a.rva.checked_sub(text.rva)?).ok()?;
        text.bytes.get(off..)
    }) else {
        bail!("{}: {:#x} is not inside `.text'", l.id, a.rva);
    };
    let mut texts = pairs(l.sources("sources")?);
    texts.extend(pairs(l.sources("headers")?));
    let pop = msvc6_class::receiver::population(&texts);
    let seed = a.this.map(|t| match t {
        ThisArg::Ecx => msvc6_class::receiver::Seed::Ecx,
        ThisArg::Stack => msvc6_class::receiver::Seed::Stack,
    });
    let r = msvc6_class::receiver::rank(body, a.rva, seed, &pop);
    say(&msvc6_class::receiver::report(&r, &l.id, a.rva, a.len));
    json(&a.json, "class receiver", &l.id, Some(&r), &r.candidates)?;
    Ok(Outcome::Ok)
}

fn pairs(v: Vec<msvc6_class::sources::Source>) -> Vec<(String, String)> {
    v.into_iter().map(|s| (s.key, s.text)).collect()
}

fn slots(config: Option<&Path>, a: &SlotsArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let runs = vft::detect(&l.img, Sections::default())?;
    let lengths: Vec<usize> = runs
        .iter()
        .map(|r| r.slots.len())
        .filter(|n| *n >= a.min_slots)
        .collect();
    let files = pairs(l.sources("sources")?);
    let headers = pairs(l.sources("headers")?);
    let controls = l.count_controls();
    let r = declared::screen(&declared::Inputs {
        label: &l.id,
        files: &files,
        headers: &headers,
        lengths: &lengths,
        min_slots: a.min_slots,
        controls: &controls,
        rule: rule(a.legacy_slots),
    });
    say(&r.lines);
    json(&a.json, "class slots", &l.id, Some(&r), &r.controls)?;
    Ok(if r.controls.iter().any(|c| !c.reproduced) {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

fn slot_map(config: Option<&Path>, a: &SlotMapArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let text = Text::of(&l.img, vft::CODE)?;
    let runs = vft::detect(&l.img, Sections::default())?;
    let find = |want: u32| runs.iter().find(|r| r.rva == want).map(|r| r.slots.clone());
    let Some(slots) = find(a.table) else {
        bail!(
            "{}: no candidate run at {:#x}; `class vftables {}` lists them, and a run that over-reaches by one dword at the front reads as a table of N+1 (try {:#x})",
            l.id,
            a.table,
            l.id,
            a.table.wrapping_add(4)
        );
    };
    let base = match a.base {
        Some(b) => Some(
            find(b).with_context(|| format!("{}: no candidate run at {b:#x} for --base", l.id))?,
        ),
        None => None,
    };
    let m = slotmap::map(text, &slots, base.as_deref());
    say(&slotmap::report(
        &m,
        &l.id,
        a.table,
        slots.len(),
        a.base.zip(base.as_ref().map(Vec::len)),
    ));
    json(&a.json, "class slot-map", &l.id, Some(&m.suspects), &m.rows)?;
    Ok(Outcome::Ok)
}

#[derive(Debug, Serialize)]
struct JoinSummary {
    population: usize,
    names_known: usize,
    assigned: usize,
    agree: usize,
    unpinned: usize,
    disagree: usize,
    table_pinned: usize,
    ambiguous: usize,
    unpinned_classes: usize,
    unresolved: usize,
    shared: usize,
}

fn join(config: Option<&Path>, a: &JoinArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let text = Text::of(&l.img, vft::CODE)?;
    let runs: Vec<Run> = vft::detect(&l.img, Sections::default())?
        .into_iter()
        .map(|r| Run {
            rva: r.rva,
            slots: r.slots.iter().map(|s| text.follow(*s)).collect(),
        })
        .collect();
    let (names, sources) = l.flat_names(&a.names)?;
    let map = match &a.map {
        Some(m) => join::map_names(&read::text(m, read::MAX_TEXT)?, l.base()),
        None => Vec::new(),
    };
    let files = pairs(l.sources("sources")?);
    let headers = pairs(l.sources("headers")?);
    let j = join::join(&join::Inputs {
        runs: &runs,
        names: &names,
        map: &map,
        placed: &[],
        files: &files,
        headers: &headers,
        rule: rule(a.legacy_slots),
        text: Some(text),
    });
    let mut said = format!("names: {}", sources.join(", "));
    match &a.map {
        Some(m) => said.push_str(&format!(" and {}", m.display())),
        None => said.push_str("; no map"),
    }
    let population = format!("{} file(s) of [image.classes] sources", files.len());
    say(&join::report(
        &j,
        join::Header {
            image: &l.id,
            population: &population,
            names: &said,
        },
    ));
    if let Some(p) = &a.tsv {
        msvc6_base::srcio::own_write(p, join::tsv(&j, l.base()))
            .with_context(|| format!("writing {}", p.display()))?;
        println!();
        println!("  wrote {}", p.display());
    }
    let count = |want: fn(&join::Slot) -> bool| -> usize {
        j.assigned
            .iter()
            .flat_map(|x| x.slots.iter())
            .filter(|s| want(s))
            .count()
    };
    let summary = JoinSummary {
        population: j.population,
        names_known: j.names_known,
        assigned: j.assigned.len(),
        agree: count(|s| *s == join::Slot::Agrees),
        unpinned: count(|s| *s == join::Slot::Unpinned),
        disagree: count(|s| matches!(s, join::Slot::Disagrees(_))),
        table_pinned: j.table_pinned,
        ambiguous: j.ambiguous.len(),
        unpinned_classes: j.unpinned.len(),
        unresolved: j.unresolved.len(),
        shared: j.shared.len(),
    };
    json(&a.json, "class join", &l.id, Some(&summary), &j.assigned)?;
    Ok(Outcome::Ok)
}

type FlatNames = (Vec<(String, u32)>, Vec<String>);

struct Loaded {
    p: Config,
    n: usize,
    id: String,
    r: Resolved,
    img: Image,
}

fn load(config: Option<&Path>, image: &str) -> Result<Loaded> {
    let p = Config::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, image)?;
    let id = d.id.clone();
    let r = p.reference(n)?;
    let img = Image::parse(r.bytes.clone()).with_context(|| r.path.display().to_string())?;
    Ok(Loaded { p, n, id, r, img })
}

impl Loaded {
    fn sibling(&self, image: &str) -> Result<Self> {
        let (n, d) = msvc6_project::registry::resolve_id(&self.p, image)?;
        let id = d.id.clone();
        let r = self.p.reference(n)?;
        let img = Image::parse(r.bytes.clone()).with_context(|| r.path.display().to_string())?;
        Ok(Self {
            p: self.p.clone(),
            n,
            id,
            r,
            img,
        })
    }

    fn base(&self) -> u32 {
        self.img.opt.image_base.get()
    }

    fn instructions(&self) -> Result<Vec<Function>> {
        let d = self
            .p
            .images
            .get(self.n)
            .with_context(|| format!("no image[{}]", self.n))?;
        let key = msvc6_project::spans::join(&self.p.image_key(self.n), "functions");
        for (i, s) in d.functions.iter().enumerate() {
            if let FunctionSource::GhidraJson(f) = s {
                let path = self.p.path_at(&msvc6_project::spans::index(&key, i), f)?;
                let text = msvc6_base::srcio::tool_text(&read::bytes(&path, read::MAX_TEXT)?);
                return insns::from_ghidra(&text, self.base())
                    .with_context(|| path.display().to_string());
            }
        }
        let ix = msvc6_project::functions::index_of(&self.p, self.n, &self.r)?;
        let extents: Vec<(u32, Option<u32>)> = ix.iter().map(|f| (f.rva, f.end)).collect();
        let text = Text::of(&self.img, vft::CODE)?;
        let (funcs, starved) = insns::decode_within(text.bytes, text.rva, &extents);
        if starved {
            println!(
                "NOTE: the decode of the function index stopped at its budget (at most {} instructions, twice \
                 the .text bytes): the bodies past it are read short",
                insns::MAX_INSNS
            );
        }
        Ok(funcs)
    }

    fn names(&self) -> Result<BTreeMap<u32, Vec<String>>> {
        let o = msvc6_project::names::oracle_of(&self.p, self.n, &self.r)?;
        let mut out: BTreeMap<u32, Vec<String>> = BTreeMap::new();
        for x in o
            .iter()
            .filter(|x| x.kind != Kind::Import && x.scope.is_none())
        {
            let v = out.entry(x.rva).or_default();
            if !v.contains(&x.name) {
                v.push(x.name.clone());
            }
        }
        Ok(out)
    }

    fn flat_names(&self, only: &[PathBuf]) -> Result<FlatNames> {
        let o = msvc6_project::names::oracle_of(&self.p, self.n, &self.r)?;
        let names = msvc6_diff::resolve::Names::from_oracle(&o);
        let mut out: Vec<(String, u32)> = Vec::new();
        let mut used = Vec::new();
        if only.is_empty() {
            let mut e: Vec<(&String, &u32)> = names.exports().iter().collect();
            e.sort();
            out.extend(e.into_iter().map(|(n, r)| (n.clone(), *r)));
            used.push("exports".to_string());
        }
        for t in names.tiers() {
            if !only.is_empty() && !only.iter().any(|f| Path::new(t.source).ends_with(f)) {
                continue;
            }
            let mut flat: Vec<(&String, &u32)> = t.flat.iter().collect();
            flat.sort();
            out.extend(flat.into_iter().map(|(n, r)| (n.clone(), *r)));
            used.push(t.source.to_string());
        }
        if used.is_empty() {
            bail!(
                "no name source of image `{}` ends with {}",
                self.id,
                only.iter()
                    .map(|p| p.display().to_string())
                    .collect::<Vec<_>>()
                    .join(" or ")
            );
        }
        Ok((out, used))
    }

    fn sources(&self, list: &str) -> Result<Vec<msvc6_class::sources::Source>> {
        self.read_dirs(list, &msvc6_class::sources::EXTENSIONS)
    }

    fn read_dirs(&self, list: &str, exts: &[&str]) -> Result<Vec<msvc6_class::sources::Source>> {
        let d = self
            .p
            .images
            .get(self.n)
            .with_context(|| format!("no image[{}]", self.n))?;
        let Some(c) = &d.classes else {
            bail!(
                "image `{}` has no [image.classes] `sources`: the directories its classes are declared in",
                self.id
            );
        };
        let given = if list == "headers" {
            &c.headers
        } else {
            &c.sources
        };
        let key = msvc6_project::spans::join(&self.p.image_key(self.n), "classes");
        let mut dirs = Vec::new();
        for (i, s) in given.iter().enumerate() {
            dirs.push(self.p.path_at(
                &msvc6_project::spans::index(&msvc6_project::spans::join(&key, list), i),
                s,
            )?);
        }
        if dirs.is_empty() && list == "sources" {
            bail!("image `{}`: [image.classes] `sources` is empty", self.id);
        }
        msvc6_class::sources::read_all(&dirs, exts)
    }

    fn controls(&self) -> Vec<vft::Control> {
        self.p
            .images
            .get(self.n)
            .and_then(|d| d.classes.as_ref())
            .map(|c| {
                c.tables
                    .iter()
                    .map(|t| vft::Control {
                        rva: t.rva,
                        slots: t.slots,
                        what: t.what.clone(),
                    })
                    .collect()
            })
            .unwrap_or_default()
    }

    fn count_controls(&self) -> Vec<declared::CountControl> {
        self.p
            .images
            .get(self.n)
            .and_then(|d| d.classes.as_ref())
            .map(|c| {
                c.counts
                    .iter()
                    .map(|t| declared::CountControl {
                        class: t.class.clone(),
                        slots: t.slots,
                        what: t.what.clone(),
                    })
                    .collect()
            })
            .unwrap_or_default()
    }
}

#[derive(Debug, Serialize)]
struct TableRow {
    rva: u32,
    slots: usize,
    attested: Option<bool>,
    bodies: Vec<u32>,
    written_by: Vec<vft::Store>,
}

#[derive(Debug, Serialize)]
struct VftSummary {
    runs: usize,
    kept: usize,
    min_slots: usize,
    instructions: Option<usize>,
    controls: Vec<(vft::Control, vft::Found)>,
    census: Option<vft::Census>,
}

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

fn vftables(config: Option<&Path>, a: &VftablesArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let sections = Sections {
        code: vft::CODE,
        data: &a.data_section,
    };
    let text = Text::of(&l.img, sections.code)?;
    let runs = vft::detect(&l.img, sections)?;
    let labels = vft::Labels {
        image: &l.id,
        sections,
    };
    say(&vft::header(labels, &runs, a.min_slots));
    let mut summary = VftSummary {
        runs: runs.len(),
        kept: runs.iter().filter(|r| r.slots.len() >= a.min_slots).count(),
        min_slots: a.min_slots,
        instructions: None,
        controls: Vec::new(),
        census: None,
    };
    if let Some(c) = &a.candidate {
        let ours = Image::parse(read::bytes(c, read::MAX_IMAGE)?)
            .with_context(|| c.display().to_string())?;
        let mine = vft::detect(&ours, sections).with_context(|| c.display().to_string())?;
        let census = vft::census(&runs, &mine, a.min_slots);
        say(&vft::census_report(
            &census,
            &c.display().to_string(),
            a.min_slots,
        ));
        summary.census = Some(census);
    }
    let follow = |s: &[u32]| -> Vec<u32> { s.iter().map(|x| text.follow(*x)).collect() };
    if a.stores || a.owed || a.table.is_some() {
        let funcs = l.instructions()?;
        summary.instructions = Some(funcs.iter().map(|f| f.insns.len()).sum());
        let stores = vft::stores(&funcs, l.base(), &runs);
        let tables = vft::split(&runs, &stores);
        let row = |t: &vft::Table| TableRow {
            rva: t.rva,
            slots: t.slots.len(),
            attested: Some(t.attested),
            bodies: follow(&t.slots),
            written_by: stores
                .iter()
                .filter(|s| s.table == t.rva)
                .copied()
                .collect(),
        };
        if let Some(want) = a.table {
            let names = l.names()?;
            let ix = msvc6_project::functions::index_of(&l.p, l.n, &l.r)?;
            let name_of = |b: u32| -> Option<String> {
                names
                    .get(&b)
                    .and_then(|v| v.first().cloned())
                    .or_else(|| ix.get(b).and_then(|f| f.name.clone()))
            };
            say(&vft::table_report(
                text, &runs, &tables, &stores, want, &name_of,
            )?);
            let t = tables
                .iter()
                .find(|t| t.rva == want)
                .context("the table printed above")?;
            if let Some(tw) = &a.twin {
                twin(&l, &funcs, t, tw)?;
            }
            json(&a.json, "class vftables", &l.id, Some(&summary), &[row(t)])?;
            return Ok(Outcome::Ok);
        }
        if a.owed {
            let rows = owed(&l, a, text, &tables)?;
            json(
                &a.json,
                "class vftables --owed",
                &l.id,
                Some(&summary),
                &rows,
            )?;
            return Ok(Outcome::Ok);
        }
        say(&vft::stores_report(
            text,
            &tables,
            &stores,
            a.min_slots,
            a.bodies,
        ));
        let rows: Vec<TableRow> = tables
            .iter()
            .filter(|t| t.slots.len() >= a.min_slots || t.attested)
            .map(row)
            .collect();
        json(&a.json, "class vftables", &l.id, Some(&summary), &rows)?;
        return Ok(Outcome::Ok);
    }
    let controls = l.controls();
    say(&vft::summary(text, &runs, a.min_slots, &controls));
    let kept: Vec<Run> = runs
        .iter()
        .filter(|r| r.slots.len() >= a.min_slots)
        .cloned()
        .collect();
    summary.controls = vft::check_controls(&kept, &controls);
    let rows: Vec<TableRow> = kept
        .iter()
        .map(|r| TableRow {
            rva: r.rva,
            slots: r.slots.len(),
            attested: None,
            bodies: follow(&r.slots),
            written_by: Vec::new(),
        })
        .collect();
    json(&a.json, "class vftables", &l.id, Some(&summary), &rows)?;
    let missed = summary
        .controls
        .iter()
        .any(|(_, f)| *f != vft::Found::Agrees);
    Ok(if missed {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

fn twin(l: &Loaded, funcs: &[Function], ours: &vft::Table, spec: &str) -> Result<()> {
    let Some((other, at)) = spec.split_once(':') else {
        bail!("--twin wants IMAGE:RVA, e.g. OTHER:0x5bae0");
    };
    let at = parse_rva(at).map_err(anyhow::Error::msg)?;
    let t = l.sibling(other)?;
    let ttext = Text::of(&t.img, vft::CODE)?;
    let runs = vft::detect(&t.img, Sections::default())?;
    let tfuncs = t.instructions()?;
    let tables = vft::split(&runs, &vft::stores(&tfuncs, t.base(), &runs));
    let Some(theirs) = tables.iter().find(|x| x.rva == at) else {
        bail!("{}: no candidate table at {at:#x}", t.id);
    };
    let text = Text::of(&l.img, vft::CODE)?;
    let ob: Vec<u32> = ours.slots.iter().map(|s| text.follow(*s)).collect();
    let tb: Vec<u32> = theirs.slots.iter().map(|s| ttext.follow(*s)).collect();
    let tnames = t.names()?;
    let side = |x: &Loaded, f| vft::Side {
        label: "",
        funcs: f,
        base: x.base(),
        size: x.img.opt.size_of_image,
    };
    let w = vft::Twin {
        ours: vft::Side {
            label: &l.id,
            ..side(l, funcs)
        },
        theirs: vft::Side {
            label: &t.id,
            ..side(&t, &tfuncs)
        },
        table: at,
        attested: theirs.attested,
    };
    let (lines, _) = vft::twin_report(&w, &ob, &tb, &|r| {
        tnames.get(&r).cloned().unwrap_or_default()
    });
    say(&lines);
    Ok(())
}

fn owed(
    l: &Loaded,
    a: &VftablesArgs,
    text: Text<'_>,
    tables: &[vft::Table],
) -> Result<Vec<vft::OwedRow>> {
    let dir = a
        .objs
        .as_ref()
        .context("--owed reads the objects of --objs")?;
    let oracle = msvc6_project::names::oracle_of(&l.p, l.n, &l.r)?;
    let names = msvc6_diff::resolve::Names::from_oracle(&oracle);
    let objects = read::files(dir, &["obj"])?;
    let mut defined: BTreeSet<u32> = BTreeSet::new();
    let mut counts = vft::OwedCounts {
        objects: objects.len(),
        ..vft::OwedCounts::default()
    };
    let mut stems: BTreeSet<String> = BTreeSet::new();
    for p in &objects {
        let stem = p
            .file_stem()
            .map_or_else(String::new, |s| s.to_string_lossy().into_owned());
        stems.insert(stem.to_ascii_lowercase());
        let d = match read::bytes(p, read::MAX_OBJECT) {
            Ok(d) => d,
            Err(e) => {
                eprintln!("class vftables --owed: REFUSED {}: {e:#}", p.display());
                continue;
            }
        };
        let Ok(defs) = msvc6_diff::comdat::defined_externals(&d) else {
            eprintln!(
                "class vftables --owed: REFUSED {}: not a COFF object",
                p.display()
            );
            continue;
        };
        for (_, _, name) in defs {
            counts.names += 1;
            if let Some((r, _)) = names.recorded(&name, &[stem.as_str()]) {
                counts.placed += 1;
                defined.insert(text.follow(r));
            }
        }
    }
    let followed =
        |t: &vft::Table| -> Vec<u32> { t.slots.iter().map(|s| text.follow(*s)).collect() };
    let every: Vec<u32> = tables.iter().flat_map(followed).collect();
    let spans: Vec<vft::Span> = match &a.library {
        Some(f) => msvc6_match::table::Table::parse(&read::text(f, read::MAX_TEXT)?, f)?
            .member
            .iter()
            .map(|m| vft::Span {
                rva: m.rva,
                len: m.len,
                member: m.member.clone(),
                archive: m.archive.clone(),
            })
            .collect(),
        None => Vec::new(),
    };
    let archive = vft::archive_bodies(&spans, every.iter().copied());
    counts.archive = archive.len();
    defined.extend(archive);
    let mut skip: BTreeSet<u32> = oracle
        .iter()
        .filter(|x| x.name.trim_start_matches('_') == vft::PURECALL)
        .map(|x| text.follow(x.rva))
        .collect();
    let d =
        l.p.images
            .get(l.n)
            .with_context(|| format!("no image[{}]", l.n))?;
    let key = msvc6_project::spans::join(&l.p.image_key(l.n), "libraries");
    for (i, raw) in d.libraries.iter().enumerate() {
        let path = l.p.path_at(&msvc6_project::spans::index(&key, i), raw)?;
        let Ok(bytes) = read::bytes(&path, read::MAX_ARCHIVE) else {
            continue;
        };
        let Ok(ar) = msvc6_image::archive::Archive::parse(&bytes) else {
            continue;
        };
        for (_, body) in ar.content() {
            for c in msvc6_diff::comdat::text_of(body).unwrap_or_default() {
                if c.symbol.as_deref() == Some(vft::CRT_PURECALL) {
                    counts.member_read = true;
                    skip.extend(vft::bodies_matching(
                        text,
                        every.iter().copied(),
                        &c.bytes,
                        &c.relocated,
                    ));
                }
            }
        }
    }
    counts.purecalls = skip.len();
    let tabs: Vec<(u32, Vec<u32>)> = tables
        .iter()
        .filter(|t| t.slots.len() >= a.min_slots || t.attested)
        .map(|t| (t.rva, followed(t)))
        .collect();
    let rows = vft::owed_rows(&tabs, &defined, &skip);
    if let Some(r) = &d.project {
        let top = msvc6_project::spec::top_of(&r.config).to_string();
        counts.missing = msvc6_project::spec::resolve(&l.p, l.n)?
            .specs
            .iter()
            .filter(|s| s.project.eq_ignore_ascii_case(&top))
            .filter(|s| !stems.contains(&s.stem.to_ascii_lowercase()))
            .map(|s| s.stem.clone())
            .collect();
    }
    say(&vft::owed_report(
        &rows,
        &counts,
        &dir.display().to_string(),
    ));
    Ok(rows)
}
