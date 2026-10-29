use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_base::{rowjson, srcio};
use msvc6_source::config::{self, Loaded};
use msvc6_source::strip::{self, Layout};
use msvc6_source::syntax::Syntax;
use msvc6_source::{lex, tree};
use std::path::{Path, PathBuf};

pub const DIFFERS: u8 = 3;

#[derive(Debug, Args)]
pub struct Common {
    #[arg(
        long,
        value_name = "DIR",
        global = true,
        help = "The source tree. Default: [source] root, else the current directory."
    )]
    pub root: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        global = true,
        help = "Also write the rows as JSON ({format: 1, command, image, summary?, rows})."
    )]
    pub json: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct SrcArgs {
    #[command(flatten)]
    pub common: Common,
    #[command(subcommand)]
    pub verb: SrcVerb,
}

#[derive(Debug, Subcommand)]
pub enum SrcVerb {
    #[command(
        about = "Lex one C or C++ source as Visual C++ 6.0 reads it",
        long_about = "Lex one C or C++ source on its bytes as CL 12.00 reads it: a CR LF is one line break and a \
                      lone CR none; trigraphs are replaced first (`??/` is a backslash, inside a literal too); \
                      every backslash-newline is deleted, inside a literal or a `//` comment included; then the \
                      tokens, each with its raw byte span and the physical line of its first and last character, \
                      its directive, its conditional state (live, unknown, or dead in a known-false group such as \
                      `#if 0`, whose text is read only for comments, quotes and directives) and whether it lies \
                      in `__asm`, where `;` opens a comment. Code-page bytes 0x80-0xFF are kept as they are. Prints \
                      the tokens, comments, directives and any place the compiler would refuse."
    )]
    Lex(LexArgs),
    #[command(
        about = "Find, list or remove the comments of a source tree, each file in its own syntax",
        long_about = "Every comment of every file of the source tree, in the file's own syntax: C and C++ \
                      (`.rc` too; the lexer's own rules, so a literal never holds a comment and a trigraph or a \
                      splice is read as CL reads it), module definitions (`;`), batch files (`rem`, `::`), MSDEV \
                      projects (a `#` line that is no project directive and not the two signature lines MSDEV \
                      refuses a project without) and workspaces, shell, YAML, TOML and INI (`#`, `;`; a first-line \
                      `#!` is the interpreter's), markdown (`<!-- -->`). The files are git's tracked and \
                      untracked-but-not-ignored ones (every file, outside a repository), outside submodules and \
                      [source] exclude; [source] syntax maps globs to a syntax. A comment stays only when a \
                      [[source.keep]] row names its file, the line it opens on and the sha256 of its bytes. \
                      LAYOUTS: default (a line left holding only a removed comment goes; a blank run a removal joins \
                      keeps its longer side), anchored (the lines above a line position stay, back to the `#line` \
                      that numbers them), line-keep (every line stays). Each output is checked before it is \
                      written: the code tokens are the input's (with their lines, line-keep), the kept comments \
                      are there and nothing else, every line position keeps its compiler line and its function's \
                      `{` line. With [source.format] a formatted file is the formatter's output of the strip, which \
                      must reach a fixed point. The compiler decides the rest: `src prove --before ROOT --after DIR` \
                      on a preview."
    )]
    Strip(StripArgs),
    #[command(
        about = "Prove a source change byte-neutral: every unit that reads it compiles to the same object",
        long_about = "THE BYTE-NEUTRAL PROOF of a source change: every translation unit that reads a changed file \
                      compiles, with its own flags, to the same object before and after. The change is a git \
                      range (--repo DIR --rev A..B, or --rev C for C^..C, each revision materialized from git's \
                      objects) or two directories (--before, --after). Both states compile at ONE path, the work \
                      tree (the after files swapped in for the second pass), so `__FILE__` and every embedded path \
                      agree. The units are the files [[source.prove.tu]] matches (source or sources globs, first \
                      match wins), each with its cwd, cflags and include; a unit READS a file its #include closure \
                      reaches by file name (over-counting; a computed #include reads every file). The compiler is \
                      [source.prove] compile, an argv template ({cflags} expands to the unit's flags; {source}, \
                      {source_win}, {out}, {out_win}, {scratch}, {scratch_win}, {root}, {root_win}, {cwd}, {slot}, \
                      {win:PATH}; ${VAR} from the environment, $<id> a toolchain's root) run in the unit's cwd with \
                      [source.prove] env, CL, _CL_, LINK and _LINK_ removed; a template that runs wine (a word \
                      named wine* or a .exe) needs a WINEPREFIX there, and a prefix whose HKCU\\Environment sets \
                      one of those four is refused. \
                      Units run one at a time unless the template names {slot} (one isolated toolchain state per \
                      slot), never below 2 GiB MemAvailable. Two objects are the same when their CODE KEYS are \
                      equal (every non-debug section's name, characteristics, size, bytes and relocations, each \
                      relocation by the place it targets; no clock, no symbol name, no section order) and so are \
                      the names they reference and define; two outputs that are no COFF objects (a preprocessor \
                      run as the compiler) are compared byte for byte. A changed build input \
                      (C, C++, .rc, .dsp, .def) no unit reads is UNPROVEN unless [source.prove] unbuilt names it. \
                      Exit 0 when the proof holds, 3 when an object moves or a file is unproven."
    )]
    Prove(ProveArgs),
    #[command(
        about = "Split the locals of one function into their def-use webs",
        long_about = "Split each tracked local of one function into its disjoint DEF-USE WEBS: the writes and \
                      reads joined by reaching definitions, so each web can take its own name without changing \
                      what the function computes. A decompiler merges unrelated values that share a register or \
                      a stack home; CL /Od gives each C variable its own frame slot. The body is tokenised by the \
                      VC6 lexer and cut into a statement-level control-flow graph (if/else, while, do, for, \
                      switch/case/default, break, continue, goto and labels, return, try/catch, __try/__except, \
                      __asm); within a statement reads and writes are taken in evaluation order (`v = e` writes \
                      at the end of `e`). Conditions are lowered per `&&` / `||` operand, so a write inside one is \
                      exact on the paths that evaluate it; a write after a `?` is a may-write; `&v` is a read and a \
                      may-write (an --out-param callee writes it without reading it); an `__asm` mention is a read \
                      and a may-write. Tracked: decompiler-generic locals (`iVar3`, `puVar12`) by default, every \
                      local and parameter with --locals, or the --var names."
    )]
    Webs(WebsArgs),
    #[command(
        about = "Prove a rewrite renamed only: every read sees the value it saw before",
        long_about = "THE LOGIC PROOF OF A RENAME: the rewrite of a function computes what the original computes \
                      when it only renamed locals and parameters, split or merged them, moved, added or removed \
                      their declarations, and spelled numbers as constants. Both bodies (signature included) are \
                      tokenised and aligned (Myers, forward and backward) with every token keyed by what it means: \
                      a local or parameter is `L` whatever its name (`L=` before a `=`), a number, a `#define` of a \
                      number and an enumerator are their value, the function's own name is free, a --rename \
                      OLD=NEW member or global keys as OLD. A token the alignment leaves unpaired must be \
                      declaration-only (a local declarator's type, `*`s, name, array bound, commas, `;`). Then \
                      reaching definitions on both: every read and write pairs with one of its kind, every paired \
                      read is reached by exactly the paired writes (the entry as itself), each paired local keeps \
                      its declared type, and no value of the rewrite crosses the end of the block that declares \
                      it. Outside the function bodies the files may differ only in the parameter names of \
                      declarations. FILES: OLD NEW and the functions (NAME, OLD_NAME=NEW_NAME, NAME#N for the Nth \
                      overload), or every changed definition when none is named (definitions paired by \
                      position). A GIT CHANGE: --repo DIR --rev A..B|C checks every changed C or C++ file of it. \
                      Exit 0 when every function is proven, 3 otherwise."
    )]
    RenameCheck(RenameArgs),
    #[command(
        about = "List the line positions of a source, or prove a re-layout keeps them",
        long_about = "The bytes a re-layout can move without touching a token: every LINE POSITION (`__LINE__`, \
                      and every macro whose expansion uses one: the CRT's assert, _ASSERT*, _RPT*, MFC's ASSERT, \
                      VERIFY, DEBUG_NEW, and every macro the tree's sources define in terms of one, harvested to a \
                      fixed point, with [source.macros] beside them), every STRINGIZED argument (`assert(a&&b)` \
                      holds its spelling), and under /ZI each such function's `{` line. A function-like use is \
                      numbered by the closing parenthesis of the outermost macro invocation around it, as VC6 \
                      reads a macro call whole; `#line` renumbers; a `#line` in a group of unknown truth makes a \
                      number unknown; a `#if 0` group numbers nothing. With --against OLD, the model proof that \
                      FILE (the re-laid OLD) keeps every use's compiler lines, function `{` line and stringized \
                      spelling: exit 3 when one moves. The model explains; `src prove` decides."
    )]
    Lines(LinesArgs),
    #[command(
        about = "Reorder a unit's function definitions, proven a permutation of its lines",
        long_about = "Reorder the file-scope FUNCTION DEFINITIONS of one source: the named definitions take the \
                      slots the named ones held, in the order given; every other line stays where it is. A \
                      definition's block is its head through its `}` and the comment lines directly above it \
                      (with --carry-macros, the #define lines directly above too: a macro redefined before each \
                      body travels with it). A definition that shares a line with anything else is refused. The \
                      output must be a permutation of the input's lines with as many definitions, or nothing is \
                      written; every line position whose compiler line the move changes is listed (moving a body \
                      past an assert changes its __LINE__). --list prints the definitions in order."
    )]
    Reorder(ReorderArgs),
    #[command(
        about = "Screen one function for mirror locals: values a decompiler kept that the original never wrote",
        long_about = "The MIRROR locals of one function: values C2 strength-reduced into an unnamed temp stepped \
                      beside an induction variable (`remain--` beside every `taken++`), which a decompiler names and \
                      keeps by hand. LOCKSTEP: a local all of whose writes are inits and steps (`x++`, `x -= k`, \
                      `x = x + k`), each step within --window statements of a step of ONE other local in the same \
                      block (a for's step clause and a do's condition are the tail of its body); strict when every \
                      step of the partner has one of the mirror beside it; the ratio when the steps' amounts agree. \
                      DERIVED: a local all of whose writes are `x = f(y)` right after a write of y in the same block, \
                      or at the head of a loop body whose tail steps y. An address-taken local is never a \
                      candidate. A screen: each candidate is a source edit to measure."
    )]
    Mirrors(MirrorsArgs),
    #[command(
        about = "Spell one function's magic numbers as named constants from a table",
        long_about = "Spell one function's numbers as named constants, token by token, from a table of rows \
                      `name <TAB> value <TAB> lines [<TAB> meaning <TAB> group <TAB> define]` (lines in the file's \
                      numbering: `12 14-20`; define `no` when the name is declared elsewhere, an enumerator or a \
                      header's macro; a name `'A'` spells a character constant): a number on a listed line whose \
                      value is one constant's becomes its name; a number two constants claim stays; a name the file \
                      already spells is refused. The `#define`s used are written, grouped and comment-free, after \
                      the first line holding --after TEXT (else above the function). The rewrite is then held to \
                      `src rename-check`, which reads a constant and its number as one value."
    )]
    SpellNumbers(SpellArgs),
    #[command(
        about = "Count the placeholder-shaped names a source tree still declares",
        long_about = "THE PLACEHOLDER CENSUS: every declared name that encodes where a thing was found, or that it \
                      has no purpose, rather than what it does, read off the name's word parts (split at `_`, case \
                      changes and digit runs, so `TrunkLine1` holds no `unk`): DECOMPILER (`FUN_00401000`, `iVar3`, \
                      `local_1c`, `param_2`, IDA's `dword_4a5b6c`), PAD ([source.placeholders] pad_prefixes \
                      and pad_suffixes), RVA-SUFFIX (5 to 8 hex digits of an address on a name), UNKNOWN, SPARE, \
                      UNUSED, PROBE, SLOT (`slot12`, `v0c`), OFFSET-FIELD (`m_field08`, `pad13c`, a bit position), \
                      INDEXED (a generic stem and an index, a stub, an enumerator named by its value), POSITIONAL \
                      (`a0`, `arg4`). Declarations: macros, tags, enumerators, typedefs, functions, globals, \
                      members, parameters, locals and labels, each keyed by its owner (the function for a \
                      parameter, local or label; the class for a member). [source.placeholders] allow names or \
                      globs a project has decided are real; files restricts the census. --classify prints the \
                      shape of each NAME and reads no file. Exit 3 when any is counted."
    )]
    Placeholders(PlaceholderArgs),
    #[command(
        about = "Refuse hand-declarations and duplicate definitions in a project's sources",
        long_about = "Build-breaking guards on a project's own sources, per [[source.guard.image]] (name, dirs, \
                      exe): (1) a translation unit (.c, .cpp, .cxx, .cc) may not hand-declare what it could \
                      include: a file-scope `extern` declaration or a prototype of a function the unit does not \
                      itself define (a forward declaration of its own function is fine; `extern \"C\" { }` blocks \
                      are read through; a definition with `extern \"C\"` is a definition); (2) a class, struct, \
                      union, enum or typedef'd record, or a function (by qualified name and parameter types), \
                      defined twice in one image's sources; (3) a type defined in two images, refused when one of \
                      them is an executable (it should include the other's header), else reported. Exit 3 on a \
                      refusal."
    )]
    ProjectGuard(GuardArgs),
}

#[derive(Debug, Args)]
pub struct LinesArgs {
    #[arg(
        value_name = "FILE",
        help = "The source file (the re-laid one with --against)."
    )]
    pub file: PathBuf,
    #[arg(long, value_name = "OLD", help = "The source before the re-layout.")]
    pub against: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct ReorderArgs {
    #[arg(value_name = "FILE", help = "The source file.")]
    pub file: PathBuf,
    #[arg(
        long,
        conflicts_with = "order",
        help = "List the file-scope definitions in order."
    )]
    pub list: bool,
    #[arg(
        long,
        value_name = "NAME",
        num_args = 1..,
        required_unless_present = "list",
        help = "The definitions, in their new order (name, Class::name, or name#N)."
    )]
    pub order: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the result here (default: in place)."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        help = "Carry the #define lines directly above a definition with it."
    )]
    pub carry_macros: bool,
}

#[derive(Debug, Args)]
pub struct MirrorsArgs {
    #[arg(value_name = "FILE", help = "The source file.")]
    pub file: PathBuf,
    #[arg(
        value_name = "FUNCTION",
        help = "The function: name, Class::name or name#N."
    )]
    pub function: String,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 2,
        help = "How many statements apart two steps may be and still be in lockstep."
    )]
    pub window: usize,
}

#[derive(Debug, Args)]
pub struct SpellArgs {
    #[arg(value_name = "FILE", help = "The source file.")]
    pub file: PathBuf,
    #[arg(
        value_name = "FUNCTION",
        help = "The function: name, Class::name or name#N."
    )]
    pub function: String,
    #[arg(long, value_name = "TSV", help = "The table of constants.")]
    pub table: PathBuf,
    #[arg(
        long,
        value_name = "TEXT",
        help = "Write the #define block after the first line holding TEXT."
    )]
    pub after: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Where to write the rewritten source."
    )]
    pub out: PathBuf,
}

#[derive(Debug, Args)]
pub struct PlaceholderArgs {
    #[arg(
        value_name = "PATH",
        help = "Files or directories to census (default: the tree's C and C++ files)."
    )]
    pub paths: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "NAME",
        num_args = 1..,
        conflicts_with = "paths",
        help = "Print the shape of each name and read no file."
    )]
    pub classify: Vec<String>,
    #[arg(
        long,
        help = "Print every counted declaration, not only the totals per shape."
    )]
    pub rows: bool,
}

#[derive(Debug, Args)]
pub struct GuardArgs {}

#[derive(Debug, Args)]
pub struct WebsArgs {
    #[arg(value_name = "FILE", help = "The source file.")]
    pub file: PathBuf,
    #[arg(
        value_name = "FUNCTION",
        required_unless_present = "all",
        help = "The function: its name, Class::name, or name#N for the Nth definition of that name."
    )]
    pub function: Option<String>,
    #[arg(
        long,
        conflicts_with = "function",
        help = "Build the graph of every function definition of the file and print one line each (the webs \
                of its generic locals, or why its body does not parse)."
    )]
    pub all: bool,
    #[arg(
        long = "var",
        value_name = "NAME",
        help = "Track this local (repeatable)."
    )]
    pub vars: Vec<String>,
    #[arg(
        long,
        conflicts_with = "vars",
        help = "Track every local and parameter."
    )]
    pub locals: bool,
    #[arg(
        long = "out-param",
        value_name = "CALLEE",
        help = "A callee taken to write each `&local` argument without reading it (repeatable): an assumption, \
                printed with the result."
    )]
    pub out_param: Vec<String>,
    #[arg(long, help = "Print every def and use line of each web.")]
    pub full: bool,
}

#[derive(Debug, Args)]
pub struct RenameArgs {
    #[arg(value_name = "OLD", help = "The original source file.")]
    pub old: Option<PathBuf>,
    #[arg(value_name = "NEW", help = "The rewritten source file.")]
    pub new: Option<PathBuf>,
    #[arg(
        value_name = "FUNCTION",
        help = "The functions to check: NAME, OLD_NAME=NEW_NAME or NAME#N. Default: every changed definition."
    )]
    pub functions: Vec<String>,
    #[arg(
        long,
        value_name = "DIR",
        requires = "rev",
        conflicts_with_all = ["old", "new"],
        help = "The git repository holding the rewrite."
    )]
    pub repo: Option<PathBuf>,
    #[arg(
        long,
        value_name = "A..B|C",
        requires = "repo",
        help = "The rewrite: the range A..B, or the commit C against its first parent."
    )]
    pub rev: Option<String>,
    #[arg(
        long = "files",
        value_name = "REL",
        num_args = 1..,
        requires = "repo",
        help = "Only these changed files of the git change."
    )]
    pub files: Vec<String>,
    #[arg(
        long = "macros",
        value_name = "FILE",
        help = "A header whose `#define NAME <number>` lines and enumerators the rewrite may spell numbers with \
                (repeatable; both files' own are always read)."
    )]
    pub macros: Vec<PathBuf>,
    #[arg(
        long = "rename",
        value_name = "OLD=NEW",
        help = "A member or global the rewrite renamed at its declaration and at every use (repeatable): NEW keys \
                as OLD. An assumption, printed with the verdict."
    )]
    pub renames: Vec<String>,
    #[arg(
        long = "out-param",
        value_name = "CALLEE",
        help = "A callee taken to write each `&local` argument without reading it (repeatable)."
    )]
    pub out_param: Vec<String>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 20,
        help = "Findings printed per function."
    )]
    pub show: usize,
    #[arg(
        long,
        help = "Do not take the renames the definitions themselves make (a definition paired by position whose \
                name changed, consistently across the change: every call of OLD now spelt NEW) as --rename rows."
    )]
    pub no_implied: bool,
}

#[derive(Debug, Args)]
pub struct ProveArgs {
    #[arg(
        long,
        value_name = "DIR",
        requires = "rev",
        help = "The git repository holding the change."
    )]
    pub repo: Option<PathBuf>,
    #[arg(
        long,
        value_name = "A..B|C",
        requires = "repo",
        conflicts_with_all = ["before", "after"],
        help = "The change: the range A..B, or the commit C against its first parent."
    )]
    pub rev: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        requires = "after",
        help = "The tree before the change."
    )]
    pub before: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        requires = "before",
        help = "The tree after the change."
    )]
    pub after: Option<PathBuf>,
    #[arg(
        long,
        short = 'j',
        value_name = "N",
        help = "Units compiled at once when the compile template names {slot}. Default: [source.prove] jobs, else \
                min(8, cores / 2)."
    )]
    pub jobs: Option<usize>,
    #[arg(
        long,
        value_name = "DIR",
        help = "The work directory. The proof clears only a directory it marked as its own (its marker names the \
                directory's device and inode and has one link) and refuses any other \
                that holds files. Default: [source.prove] work, else a directory under the system temp directory \
                named for the repository or the before tree and this process."
    )]
    pub work: Option<PathBuf>,
    #[arg(long, help = "Leave the work directory in place.")]
    pub keep_work: bool,
    #[arg(
        long,
        value_name = "TSV",
        help = "Rows `<unit path> <TAB> <code key>`: each unit's object AFTER the change must have that key -- \
                another build's record of the same objects, which makes that build the oracle for the compile \
                template too."
    )]
    pub expect: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct LexArgs {
    #[arg(value_name = "FILE", help = "The source file.")]
    pub file: PathBuf,
}

#[derive(Debug, Args)]
pub struct StripArgs {
    #[arg(
        long,
        conflicts_with_all = ["list", "preview", "write"],
        help = "Change nothing: name every comment no [[source.keep]] row keeps, file and line, and exit 3 when \
                there is one."
    )]
    pub check: bool,
    #[arg(
        long,
        conflicts_with_all = ["preview", "write"],
        help = "Print each file's syntax, comment count and kept count."
    )]
    pub list: bool,
    #[arg(
        long,
        value_name = "DIR",
        conflicts_with = "write",
        help = "Write each stripped file under DIR at its path in the tree (a copy of the tree with the comments \
                gone, for `src prove --before ROOT --after DIR`); the tree is not touched."
    )]
    pub preview: Option<PathBuf>,
    #[arg(
        long,
        help = "Write each stripped file in place, each only when its checks pass."
    )]
    pub write: bool,
    #[arg(
        long,
        value_name = "LAYOUT",
        value_parser = ["default", "anchored", "line-keep"],
        help = "The layout for every file. Default: [source] layout's for the file, else anchored for a C \
                source holding a line position, else default; line-keep when the first refuses."
    )]
    pub layout: Option<String>,
    #[arg(
        long = "files",
        value_name = "REL",
        num_args = 1..,
        help = "Only these files of the tree (paths relative to the root)."
    )]
    pub files: Vec<String>,
}

fn formatter_for(cfg: &Loaded, rel: &str) -> Result<Option<Vec<String>>> {
    let Some(f) = &cfg.source.format else {
        return Ok(None);
    };
    let wanted = f.files.iter().any(|g| msvc6_source::glob::matches(g, rel))
        && !f.ignore.iter().any(|g| msvc6_source::glob::matches(g, rel));
    if !wanted {
        return Ok(None);
    }
    let cmd = f
        .command
        .iter()
        .map(|a| config::expand(a))
        .collect::<Result<Vec<_>>>()?;
    Ok(Some(cmd))
}

fn layout_of(s: &str) -> Option<Layout> {
    match s {
        "default" => Some(Layout::Default),
        "anchored" => Some(Layout::Anchored),
        "line-keep" => Some(Layout::LineKeep),
        _ => None,
    }
}

pub fn run(
    a: &SrcArgs,
    config_path: Option<&Path>,
    provenance: &dyn Fn() -> Result<()>,
) -> Result<u8> {
    let cfg = msvc6_project::config::source_config(config_path)?;
    match &a.verb {
        SrcVerb::Lex(l) => lex_cmd(&a.common, l),
        SrcVerb::Strip(s) => strip_cmd(&a.common, &cfg, s),
        SrcVerb::Prove(p) => {
            provenance()?;
            prove_cmd(&a.common, &cfg, p)
        }
        SrcVerb::Webs(w) => webs_cmd(&a.common, w),
        SrcVerb::RenameCheck(r) => rename_cmd(&a.common, &cfg, r),
        SrcVerb::Lines(l) => lines_cmd(&a.common, &cfg, l),
        SrcVerb::Reorder(r) => reorder_cmd(&a.common, r),
        SrcVerb::Mirrors(m) => mirrors_cmd(&a.common, m),
        SrcVerb::SpellNumbers(s) => spell_cmd(&a.common, s),
        SrcVerb::Placeholders(p) => placeholders_cmd(&a.common, &cfg, p),
        SrcVerb::ProjectGuard(g) => guard_cmd(&a.common, &cfg, g),
    }
}

fn macros_for(c: &Common, cfg: &Loaded) -> msvc6_source::linepos::MacroSet {
    let root = cfg.root(c.root.as_deref()).ok();
    match root
        .as_ref()
        .and_then(|r| tree::files(r, cfg).ok().map(|f| (r.clone(), f)))
    {
        Some((r, files)) => strip::macro_set(&r, cfg, &files),
        None => {
            let mut m = msvc6_source::linepos::MacroSet::builtin();
            let s = &cfg.source.macros;
            m.add(&s.function, &s.object, &s.stringize);
            m
        }
    }
}

fn lines_cmd(c: &Common, cfg: &Loaded, a: &LinesArgs) -> Result<u8> {
    use msvc6_source::linepos;
    let mut macros = macros_for(c, cfg);
    let src = read_bytes(&a.file)?;
    let lx = lex::lex(&src);
    macros.harvest([&lx], cfg.source.macros.crtdbg_map_alloc);
    if let Some(old) = &a.against {
        let ob = read_bytes(old)?;
        let olx = lex::lex(&ob);
        let moved = linepos::proof(&olx, &lx, &macros);
        for m in &moved {
            println!("MOVES  {m}");
        }
        println!(
            "lines {}: {} line position(s) and stringizing use(s) before, {} move",
            if moved.is_empty() { "HOLD" } else { "MOVE" },
            linepos::uses(&olx, &macros).len(),
            moved.len()
        );
        json::<_, ()>(c, "src lines", &a.file.display().to_string(), None, &moved)?;
        return Ok(if moved.is_empty() { 0 } else { DIFFERS });
    }
    let pins = linepos::pins(&lx);
    let uses = linepos::uses(&lx, &macros);
    for u in &uses {
        let n = linepos::compiler_line(&pins, u.own_last);
        println!(
            "{:>6} {:<10} {:<24} compiler line {}{}{}",
            u.first,
            if u.line_pos { "line" } else { "stringize" },
            u.name,
            n.map_or("unknown".to_string(), |n| n.to_string()),
            u.brace
                .map_or(String::new(), |b| format!(", function `{{` at line {b}")),
            match u.cond {
                lex::Cond::Unknown => " (conditional)",
                _ => "",
            }
        );
    }
    for p in &pins {
        println!("{:>6} #line {}", p.line, p.number);
    }
    println!(
        "lines {}: {} use(s), {} #line pin(s); {} function-like and {} object-like line macros known",
        a.file.display(),
        uses.len(),
        pins.len(),
        macros.function.len(),
        macros.object.len()
    );
    json::<_, ()>(c, "src lines", &a.file.display().to_string(), None, &uses)?;
    Ok(0)
}

fn reorder_cmd(c: &Common, a: &ReorderArgs) -> Result<u8> {
    use msvc6_source::{defs, reorder};
    let src = read_bytes(&a.file)?;
    if a.list {
        let lx = lex::lex(&src);
        let ds = defs::definitions(&lx);
        let bs =
            reorder::blocks(&src, &lx, &ds, a.carry_macros).map_err(|e| anyhow::anyhow!("{e}"))?;
        for b in &bs {
            println!("{:>6}-{:<6} {}", b.first, b.last, b.name);
        }
        println!(
            "reorder {}: {} file-scope definition(s)",
            a.file.display(),
            bs.len()
        );
        json::<_, ()>(c, "src reorder", &a.file.display().to_string(), None, &bs)?;
        return Ok(0);
    }
    let r = reorder::reorder(&src, &a.order, a.carry_macros).map_err(|e| anyhow::anyhow!("{e}"))?;
    let to = a.out.clone().unwrap_or_else(|| a.file.clone());
    srcio::replace_bytes(&to, &r.bytes).with_context(|| format!("writing {}", to.display()))?;
    for m in &r.line_positions_moved {
        println!("LINE POSITION MOVES  {m}");
    }
    println!(
        "reorder: {} definition(s) moved, the output a permutation of the input's lines, written to {}; {} line \
         position(s) move",
        r.moved,
        to.display(),
        r.line_positions_moved.len()
    );
    Ok(0)
}

fn mirrors_cmd(c: &Common, a: &MirrorsArgs) -> Result<u8> {
    use msvc6_source::{defs, mirrors};
    let src = read_bytes(&a.file)?;
    let lx = lex::lex(&src);
    let ds = defs::definitions(&lx);
    let d = defs::one(&ds, &a.function).map_err(|e| anyhow::anyhow!("{e}"))?;
    let ms =
        mirrors::screen(&lx, d, a.window).map_err(|e| anyhow::anyhow!("{}: {e}", d.qualified()))?;
    for m in &ms {
        println!(
            "{:<9} {:<20} beside {:<20} {}{} steps {}: lines {:?}",
            m.kind.to_ascii_uppercase(),
            m.var,
            m.partner,
            m.ratio.map_or(String::new(), |r| format!("ratio {r} ")),
            if m.strict { "strict " } else { "" },
            m.steps,
            m.lines
        );
    }
    println!(
        "mirrors {}: {} candidate(s), window {}",
        d.qualified(),
        ms.len(),
        a.window
    );
    json::<_, ()>(c, "src mirrors", &a.file.display().to_string(), None, &ms)?;
    Ok(0)
}

fn spell_cmd(c: &Common, a: &SpellArgs) -> Result<u8> {
    use msvc6_source::spellnums;
    let src = read_bytes(&a.file)?;
    let text =
        srcio::own_read(&a.table).with_context(|| format!("reading {}", a.table.display()))?;
    let consts =
        spellnums::table(&text).map_err(|e| anyhow::anyhow!("{}: {e}", a.table.display()))?;
    let s = spellnums::spell(&src, &a.function, &consts, a.after.as_deref())
        .map_err(|e| anyhow::anyhow!("{e}"))?;
    srcio::write_bytes(&a.out, &s.bytes).with_context(|| format!("writing {}", a.out.display()))?;
    let proven = s.verdict.as_ref().is_some_and(|v| v.proven);
    println!(
        "spell-numbers {}: {} number(s) spelt by {} constant(s) ({} defined here); {} left (two constants claim \
         them); {} name(s) refused (the file already spells them){}; rename-check {} -> {}",
        a.function,
        s.edits,
        s.used.len(),
        s.defined.len(),
        s.ambiguous,
        s.clashes.len(),
        if s.clashes.is_empty() {
            String::new()
        } else {
            format!(": {}", s.clashes.join(", "))
        },
        if proven { "PROVEN" } else { "REFUTED" },
        a.out.display()
    );
    if let Some(v) = &s.verdict {
        for f in v.findings.iter().take(10) {
            println!("      {f}");
        }
    }
    json::<_, ()>(
        c,
        "src spell-numbers",
        &a.file.display().to_string(),
        None,
        &[&s],
    )?;
    Ok(if proven { 0 } else { DIFFERS })
}

fn placeholders_cmd(c: &Common, cfg: &Loaded, a: &PlaceholderArgs) -> Result<u8> {
    use msvc6_source::placeholders::{self, Classifier, Shape};
    let cl = Classifier::new(&cfg.source.placeholders);
    if !a.classify.is_empty() {
        let mut any = false;
        for n in &a.classify {
            let s = cl.classify(n);
            any |= s.is_some();
            println!("{n}: {}", s.map_or("a real name", Shape::label));
        }
        return Ok(if any { DIFFERS } else { 0 });
    }
    let root = cfg.root(c.root.as_deref())?;
    let files: Vec<PathBuf> = if a.paths.is_empty() {
        tree::files(&root, cfg)?
            .into_iter()
            .filter(|p| cfg.syntax_of(&tree::rel_str(p)) == Syntax::C)
            .filter(|p| {
                let r = tree::rel_str(p);
                cfg.source.placeholders.files.is_empty()
                    || cfg
                        .source
                        .placeholders
                        .files
                        .iter()
                        .any(|g| msvc6_source::glob::matches(g, &r))
            })
            .map(|p| root.join(p))
            .collect()
    } else {
        let mut v = Vec::new();
        for p in &a.paths {
            if p.is_dir() {
                for f in tree::files(p, cfg)? {
                    if cfg.syntax_of(&tree::rel_str(&f)) == Syntax::C {
                        v.push(p.join(f));
                    }
                }
            } else {
                v.push(p.clone());
            }
        }
        v
    };
    let mut rows = Vec::new();
    for f in &files {
        let bytes = read_bytes(f)?;
        let rel = f
            .strip_prefix(&root)
            .map_or_else(|_| f.display().to_string(), tree::rel_str);
        rows.extend(placeholders::census(
            &rel,
            &lex::lex(&bytes),
            &cl,
            &cfg.source.placeholders.allow,
        ));
    }
    if a.rows {
        for r in &rows {
            println!(
                "{}:{}: {:<12} {:?} {} {}",
                r.path,
                r.line,
                r.shape.label(),
                r.kind,
                r.owner,
                r.name
            );
        }
    }
    let mut by: std::collections::BTreeMap<Shape, usize> = std::collections::BTreeMap::new();
    for r in &rows {
        *by.entry(r.shape).or_default() += 1;
    }
    for s in Shape::ALL {
        if let Some(n) = by.get(&s) {
            println!("  {:<12} {n}", s.label());
        }
    }
    println!("PLACEHOLDERS: {} in {} file(s)", rows.len(), files.len());
    json::<_, ()>(
        c,
        "src placeholders",
        &root.display().to_string(),
        None,
        &rows,
    )?;
    Ok(if rows.is_empty() { 0 } else { DIFFERS })
}

fn guard_cmd(c: &Common, cfg: &Loaded, _a: &GuardArgs) -> Result<u8> {
    use msvc6_source::projectguard::{self, Image};
    let root = cfg.root(c.root.as_deref())?;
    if cfg.source.guard.image.is_empty() {
        bail!("[[source.guard.image]] names no image: give each image's name, dirs and exe");
    }
    let files = tree::files(&root, cfg)?;
    let mut images = Vec::new();
    for g in &cfg.source.guard.image {
        let mut fs = Vec::new();
        for p in &files {
            let r = tree::rel_str(p);
            if cfg.syntax_of(&r) == Syntax::C
                && g.dirs.iter().any(|d| msvc6_source::glob::under(d, &r))
            {
                fs.push((r, read_bytes(&root.join(p))?));
            }
        }
        images.push(Image {
            name: g.name.clone(),
            exe: g.exe,
            files: fs,
        });
    }
    let v = projectguard::guard(&images, cfg.source.guard.prototypes);
    for x in &v {
        println!(
            "{} {}:{}: [{}] {}",
            if x.refuses { "REFUSED" } else { "REPORT " },
            x.file,
            x.line,
            x.rule,
            x.what
        );
    }
    let refused = v.iter().filter(|x| x.refuses).count();
    println!(
        "project-guard: {} image(s), {} file(s); {refused} refusal(s), {} report(s)",
        images.len(),
        images.iter().map(|i| i.files.len()).sum::<usize>(),
        v.len() - refused
    );
    json::<_, ()>(
        c,
        "src project-guard",
        &root.display().to_string(),
        None,
        &v,
    )?;
    Ok(if refused == 0 { 0 } else { DIFFERS })
}

fn read_bytes(p: &Path) -> Result<Vec<u8>> {
    std::fs::read(p).with_context(|| format!("reading {}", p.display()))
}

fn lines_of(v: &[(usize, usize)], full: bool) -> String {
    let mut ls: Vec<usize> = v.iter().map(|x| x.0).collect();
    ls.sort_unstable();
    ls.dedup();
    if full || ls.len() <= 6 {
        return ls
            .iter()
            .map(ToString::to_string)
            .collect::<Vec<_>>()
            .join(" ");
    }
    format!(
        "{}..{} ({} lines)",
        ls.first().copied().unwrap_or(0),
        ls.last().copied().unwrap_or(0),
        ls.len()
    )
}

#[derive(Debug, serde::Serialize)]
struct WebRow {
    var: String,
    web: usize,
    defs: Vec<usize>,
    uses: Vec<usize>,
    from_entry: bool,
}

fn webs_cmd(c: &Common, a: &WebsArgs) -> Result<u8> {
    use msvc6_source::{defs, flow};
    let src = read_bytes(&a.file)?;
    let lx = lex::lex(&src);
    let all = defs::definitions(&lx);
    if a.all {
        let opts = flow::Opts {
            track: if a.locals {
                flow::Track::Locals
            } else {
                flow::Track::Generic
            },
            out_param: a.out_param.clone(),
            short_circuit: true,
            ..flow::Opts::default()
        };
        let mut failed = 0usize;
        for d in &all {
            match flow::analyse(&lx, d, &opts) {
                Ok(fl) => println!(
                    "ok      {} (lines {}-{}): {} web(s), {} node(s)",
                    d.qualified(),
                    d.first_line,
                    d.last_line,
                    fl.webs.len(),
                    fl.nodes
                ),
                Err(p) => {
                    failed += 1;
                    println!(
                        "FAILED  {} (lines {}-{}): {}",
                        d.qualified(),
                        d.first_line,
                        d.last_line,
                        p.0
                    );
                }
            }
        }
        println!(
            "webs {}: {} definition(s), {failed} whose body does not parse",
            a.file.display(),
            all.len()
        );
        return Ok(if failed == 0 { 0 } else { DIFFERS });
    }
    let name = a.function.as_deref().unwrap_or_default();
    let d = defs::one(&all, name).map_err(|e| anyhow::anyhow!("{}: {e}", a.file.display()))?;
    let track = if a.locals {
        flow::Track::Locals
    } else if a.vars.is_empty() {
        flow::Track::Generic
    } else {
        flow::Track::Names(a.vars.iter().cloned().collect())
    };
    let opts = flow::Opts {
        track,
        out_param: a.out_param.clone(),
        short_circuit: true,
        ..flow::Opts::default()
    };
    let fl =
        flow::analyse(&lx, d, &opts).map_err(|p| anyhow::anyhow!("{}: {}", d.qualified(), p.0))?;
    let mut rows = Vec::new();
    let mut by_var: std::collections::BTreeMap<&str, Vec<&flow::Web>> =
        std::collections::BTreeMap::new();
    for w in &fl.webs {
        by_var.entry(w.var.as_str()).or_default().push(w);
    }
    for (var, ws) in &by_var {
        println!(
            "{var}: {} web(s){}",
            ws.len(),
            if fl.escaped.contains(*var) {
                " -- its address is taken, so the webs around each `&` are merged"
            } else {
                ""
            }
        );
        for (n, w) in ws.iter().enumerate() {
            println!(
                "  web {}: writes {}; reads {}{}",
                n + 1,
                if w.defs.is_empty() {
                    "-".to_string()
                } else {
                    lines_of(&w.defs, a.full)
                },
                if w.uses.is_empty() {
                    "-".to_string()
                } else {
                    lines_of(&w.uses, a.full)
                },
                if w.from_entry {
                    " (a read the function entry reaches: a parameter, or a value read before any write)"
                } else {
                    ""
                }
            );
            rows.push(WebRow {
                var: (*var).to_string(),
                web: n + 1,
                defs: w.defs.iter().map(|x| x.0).collect(),
                uses: w.uses.iter().map(|x| x.0).collect(),
                from_entry: w.from_entry,
            });
        }
    }
    println!(
        "webs {} ({}, lines {}-{}): {} variable(s), {} web(s), {} graph node(s){}",
        d.qualified(),
        a.file.display(),
        d.first_line,
        d.last_line,
        by_var.len(),
        fl.webs.len(),
        fl.nodes,
        if a.out_param.is_empty() {
            String::new()
        } else {
            format!(
                "; assumed out-parameter callees: {}",
                a.out_param.join(", ")
            )
        }
    );
    json::<_, ()>(c, "src webs", &a.file.display().to_string(), None, &rows)?;
    Ok(0)
}

fn rename_options(cfg: &Loaded, a: &RenameArgs) -> Result<msvc6_source::renamecheck::Options> {
    use msvc6_source::renamecheck;
    let _ = cfg;
    let mut o = renamecheck::Options {
        out_param: a.out_param.clone(),
        ..renamecheck::Options::default()
    };
    for m in &a.macros {
        let lx = lex::lex(&read_bytes(m)?);
        o.macros.extend(renamecheck::numeric_macros(&lx));
    }
    for r in &a.renames {
        let (old, new) = r
            .split_once('=')
            .with_context(|| format!("--rename {r}: not OLD=NEW"))?;
        o.renames
            .insert(new.trim().to_string(), old.trim().to_string());
    }
    Ok(o)
}

fn say_verdict(v: &msvc6_source::renamecheck::Verdict, show: usize) {
    let name = if v.function == v.new_function {
        v.function.clone()
    } else {
        format!("{} -> {}", v.function, v.new_function)
    };
    if v.proven {
        let renamed: Vec<String> = v
            .pairs
            .iter()
            .filter(|p| p.old != p.new)
            .map(|p| format!("{} -> {}", p.old, p.new))
            .collect();
        println!(
            "  PROVEN    {name}: {} read(s), {} write(s){}",
            v.reads,
            v.writes,
            if renamed.is_empty() {
                String::new()
            } else {
                format!("; {}", renamed.join(", "))
            }
        );
    } else {
        println!("  REFUTED   {name}: {} finding(s)", v.findings.len());
        for f in v.findings.iter().take(show) {
            println!("      {f}");
        }
    }
}

fn rename_cmd(c: &Common, cfg: &Loaded, a: &RenameArgs) -> Result<u8> {
    use msvc6_source::renamecheck;
    let mut o = rename_options(cfg, a)?;
    let mut files: Vec<renamecheck::FileVerdict> = Vec::new();
    let mut named: Vec<renamecheck::Verdict> = Vec::new();
    let mut skipped: Vec<String> = Vec::new();
    let mut implied: std::collections::BTreeMap<String, String> = std::collections::BTreeMap::new();
    let mut imply = |o: &mut renamecheck::Options, pairs: &[(String, String)]| {
        if a.no_implied {
            return;
        }
        for (new, old) in renamecheck::consistent(pairs) {
            if !o.renames.contains_key(&new) {
                o.renames.insert(new.clone(), old.clone());
                implied.insert(new, old);
            }
        }
    };
    match (&a.repo, &a.rev, &a.old, &a.new) {
        (Some(repo), Some(rev), _, _) => {
            let repo = std::fs::canonicalize(repo)
                .with_context(|| format!("--repo {}", repo.display()))?;
            let (before, after) = msvc6_source::git::range(&repo, rev)?;
            println!(
                "rename-check: {} {}..{} ({})",
                repo.display(),
                before.get(..10).unwrap_or(&before),
                after.get(..10).unwrap_or(&after),
                msvc6_source::git::subject(&repo, &after).unwrap_or_default()
            );
            let mut sources: Vec<(String, Vec<u8>, Vec<u8>)> = Vec::new();
            for ch in msvc6_source::git::changes(&repo, &before, &after)? {
                if !a.files.is_empty() && !a.files.contains(&ch.path) {
                    continue;
                }
                if cfg.syntax_of(&ch.path) != Syntax::C {
                    skipped.push(format!("{} (not a C or C++ source)", ch.path));
                    continue;
                }
                if ch.status != 'M' {
                    skipped.push(format!(
                        "{} ({}: added or deleted, no rewrite to check)",
                        ch.path, ch.status
                    ));
                    continue;
                }
                let old = msvc6_source::git::file_at(&repo, &before, &ch.path)?.unwrap_or_default();
                let new = msvc6_source::git::file_at(&repo, &after, &ch.path)?.unwrap_or_default();
                sources.push((ch.path, old, new));
            }
            let pairs: Vec<(String, String)> = sources
                .iter()
                .flat_map(|(_, x, y)| renamecheck::implied(x, y))
                .collect();
            imply(&mut o, &pairs);
            for (path, old, new) in &sources {
                files.push(renamecheck::check_file(path, old, new, &o));
            }
        }
        (None, None, Some(old), Some(new)) => {
            let (ob, nb) = (read_bytes(old)?, read_bytes(new)?);
            imply(&mut o, &renamecheck::implied(&ob, &nb));
            if a.functions.is_empty() {
                files.push(renamecheck::check_file(
                    &new.display().to_string(),
                    &ob,
                    &nb,
                    &o,
                ));
            } else {
                let names: Vec<(String, String)> = a
                    .functions
                    .iter()
                    .map(|f| match f.split_once('=') {
                        Some((x, y)) => (x.to_string(), y.to_string()),
                        None => (f.clone(), f.clone()),
                    })
                    .collect();
                for r in renamecheck::check_named(&ob, &nb, &names, &o) {
                    named.push(r.map_err(|e| anyhow::anyhow!("{e}"))?);
                }
            }
        }
        _ => bail!("give OLD NEW [FUNCTION...], or --repo DIR --rev SPEC"),
    }
    let mut proven = 0usize;
    let mut refuted = 0usize;
    for f in &files {
        println!(
            "{} {}: {} changed function(s), {} unchanged",
            if f.proven() { "PROVEN " } else { "REFUTED" },
            f.path,
            f.functions.len(),
            f.unchanged
        );
        for p in &f.problems {
            println!("      {p}");
        }
        for p in &f.outside {
            println!("      {p}");
        }
        for v in &f.functions {
            say_verdict(v, a.show);
        }
        proven += f.functions.iter().filter(|v| v.proven).count();
        refuted += f.functions.iter().filter(|v| !v.proven).count();
    }
    for v in &named {
        say_verdict(v, a.show);
        if v.proven {
            proven += 1;
        } else {
            refuted += 1;
        }
    }
    for s in &skipped {
        println!("  skipped {s}");
    }
    let files_ok = files.iter().all(renamecheck::FileVerdict::proven);
    let ok = files_ok && refuted == 0 && (proven > 0 || !files.is_empty());
    println!(
        "rename-check {}: {proven} function(s) proven, {refuted} refuted, in {} file(s){}{}",
        if ok { "PROVEN" } else { "REFUTED" },
        files.len().max(usize::from(!named.is_empty())),
        {
            let say = |m: &mut dyn Iterator<Item = (&String, &String)>| {
                m.map(|(n, o)| format!("{o}={n}"))
                    .collect::<Vec<_>>()
                    .join(", ")
            };
            let explicit: Vec<(&String, &String)> = o
                .renames
                .iter()
                .filter(|(n, _)| !implied.contains_key(*n))
                .collect();
            let mut s = String::new();
            if !explicit.is_empty() {
                s.push_str(&format!(
                    "; assumed renames: {}",
                    say(&mut explicit.into_iter())
                ));
            }
            if !implied.is_empty() {
                s.push_str(&format!(
                    "; renamed by their own definitions: {}",
                    say(&mut implied.iter())
                ));
            }
            s
        },
        if o.out_param.is_empty() {
            String::new()
        } else {
            format!(
                "; assumed out-parameter callees: {}",
                o.out_param.join(", ")
            )
        }
    );
    let rows: Vec<&renamecheck::Verdict> = files
        .iter()
        .flat_map(|f| &f.functions)
        .chain(&named)
        .collect();
    json(c, "src rename-check", "", Some(&files), &rows)?;
    Ok(if ok { 0 } else { DIFFERS })
}

fn default_jobs() -> usize {
    std::thread::available_parallelism()
        .map_or(1, std::num::NonZero::get)
        .div_euclid(2)
        .clamp(1, 8)
}

fn prove_cmd(c: &Common, cfg: &Loaded, a: &ProveArgs) -> Result<u8> {
    use msvc6_source::prove;
    let input = match (&a.repo, &a.rev, &a.before, &a.after) {
        (Some(repo), Some(rev), _, _) => {
            let repo = std::fs::canonicalize(repo)
                .with_context(|| format!("--repo {}", repo.display()))?;
            let (before, after) = msvc6_source::git::range(&repo, rev)?;
            println!(
                "prove: {} {}..{} ({})",
                repo.display(),
                before.get(..10).unwrap_or(&before),
                after.get(..10).unwrap_or(&after),
                msvc6_source::git::subject(&repo, &after).unwrap_or_default()
            );
            prove::Input::Revs {
                repo,
                before,
                after,
            }
        }
        (None, None, Some(b), Some(af)) => prove::Input::Dirs {
            before: std::fs::canonicalize(b)
                .with_context(|| format!("--before {}", b.display()))?,
            after: std::fs::canonicalize(af)
                .with_context(|| format!("--after {}", af.display()))?,
        },
        _ => bail!("give --repo DIR --rev SPEC, or --before DIR --after DIR"),
    };
    let tag = match &input {
        prove::Input::Revs { repo, .. } => repo.display().to_string(),
        prove::Input::Dirs { before, .. } => before.display().to_string(),
    };
    let work = match (&a.work, &cfg.source.prove.work) {
        (Some(w), _) => w.clone(),
        (None, Some(w)) => cfg.path_of(w)?,
        (None, None) => std::env::temp_dir().join(format!(
            "msvc6t-prove-{}-{}",
            msvc6_base::digest::sha256_hex(tag.as_bytes())
                .get(..12)
                .unwrap_or_default(),
            std::process::id()
        )),
    };
    let compiler = prove::CommandCompile::from_config(cfg)?;
    let jobs = a
        .jobs
        .or(cfg.source.prove.jobs)
        .unwrap_or_else(default_jobs);
    let report = prove::prove(&input, cfg, &compiler, &work, jobs)?;
    let expect: std::collections::BTreeMap<String, String> = match &a.expect {
        Some(p) => srcio::own_read(p)
            .with_context(|| format!("reading {}", p.display()))?
            .lines()
            .filter_map(|l| l.split_once('\t'))
            .map(|(k, v)| (k.trim().to_string(), v.trim().to_string()))
            .collect(),
        None => std::collections::BTreeMap::new(),
    };
    for ch in &report.changed {
        println!(
            "  {} {:<60} {}{}",
            ch.status,
            ch.path,
            ch.class,
            if ch.readers.is_empty() {
                String::new()
            } else {
                format!(" by {} unit(s)", ch.readers.len())
            }
        );
    }
    let mut expect_bad = 0usize;
    for r in &report.tus {
        let exp = match expect.get(&r.tu) {
            Some(k) if *k == r.after => " (the expected key)".to_string(),
            Some(k) => {
                expect_bad += 1;
                format!(" (EXPECTED {})", k.get(..12).unwrap_or(k))
            }
            None => String::new(),
        };
        println!(
            "{:<10} {:<60} {} -> {}{}{}",
            r.verdict.to_ascii_uppercase(),
            r.tu,
            r.before.get(..12).unwrap_or(&r.before),
            r.after.get(..12).unwrap_or(&r.after),
            exp,
            if r.at.is_empty() {
                String::new()
            } else {
                format!(": {}", r.at)
            }
        );
    }
    for u in &report.unproven {
        println!("UNPROVEN   {u}");
    }
    let identical = report
        .tus
        .iter()
        .filter(|r| r.verdict == "identical")
        .count();
    let holds = report.holds && expect_bad == 0;
    println!(
        "prove {}: {identical} of {} unit(s) compile to the same object before and after{}; {} changed file(s), {} unproven; compiler: {}",
        if holds { "HOLDS" } else { "FAILS" },
        report.tus.len(),
        if expect.is_empty() {
            String::new()
        } else {
            format!(
                ", {} of {} with the expected key",
                report
                    .tus
                    .iter()
                    .filter(|r| expect.get(&r.tu) == Some(&r.after))
                    .count(),
                report
                    .tus
                    .iter()
                    .filter(|r| expect.contains_key(&r.tu))
                    .count()
            )
        },
        report.changed.len(),
        report.unproven.len(),
        report.compiler
    );
    json(c, "src prove", &tag, Some(&report), &report.tus)?;
    if !a.keep_work {
        prove::release_work(&work)?;
    }
    Ok(if holds { 0 } else { DIFFERS })
}

fn json<T: serde::Serialize, S: serde::Serialize>(
    c: &Common,
    command: &str,
    image: &str,
    summary: Option<&S>,
    rows: &[T],
) -> Result<()> {
    if let Some(p) = &c.json {
        rowjson::write_with(p, command, image, summary, rows)?;
    }
    Ok(())
}

#[derive(Debug, serde::Serialize)]
struct TokRow<'a> {
    kind: lex::Kind,
    text: &'a str,
    start: usize,
    end: usize,
    line: usize,
    end_line: usize,
    directive: Option<usize>,
    cond: lex::Cond,
    asm: bool,
}

fn lex_cmd(c: &Common, a: &LexArgs) -> Result<u8> {
    let src = std::fs::read(&a.file).with_context(|| format!("reading {}", a.file.display()))?;
    let lx = lex::lex(&src);
    for t in &lx.tokens {
        println!(
            "{:>5}{} {:<6} {:?}{}{}",
            t.line,
            if t.end_line == t.line {
                String::new()
            } else {
                format!("-{}", t.end_line)
            },
            format!("{:?}", t.kind).to_ascii_lowercase(),
            t.text,
            match t.cond {
                lex::Cond::Live => "",
                lex::Cond::Unknown => " (conditional)",
                lex::Cond::Dead => " (dead)",
            },
            if t.asm { " (asm)" } else { "" }
        );
    }
    for cm in &lx.comments {
        println!(
            "comment {:?} lines {}-{} bytes {}..{}: {}",
            cm.kind,
            cm.first,
            cm.last,
            cm.start,
            cm.end,
            strip::excerpt(&src, cm)
        );
    }
    for d in &lx.directives {
        println!(
            "directive #{} lines {}-{} ({:?})",
            d.name, d.first, d.last, d.cond
        );
    }
    for p in &lx.problems {
        println!("PROBLEM line {}: {}", p.line, p.what);
    }
    println!(
        "lex: {} token(s), {} comment(s), {} directive(s), {} trigraph(s), {} splice(s), {} line(s), {} problem(s)",
        lx.tokens.len(),
        lx.comments.len(),
        lx.directives.len(),
        lx.trigraphs.len(),
        lx.splices.len(),
        lx.lines,
        lx.problems.len()
    );
    let rows: Vec<TokRow<'_>> = lx
        .tokens
        .iter()
        .map(|t| TokRow {
            kind: t.kind,
            text: &t.text,
            start: t.start,
            end: t.end,
            line: t.line,
            end_line: t.end_line,
            directive: t.directive,
            cond: t.cond,
            asm: t.asm,
        })
        .collect();
    json::<_, ()>(c, "src lex", &a.file.display().to_string(), None, &rows)?;
    Ok(if lx.problems.is_empty() { 0 } else { DIFFERS })
}

fn selected(root: &Path, cfg: &Loaded, only: &[String]) -> Result<Vec<PathBuf>> {
    let all = tree::files(root, cfg)?;
    if only.is_empty() {
        return Ok(all);
    }
    let mut out = Vec::new();
    for f in only {
        let p = PathBuf::from(f);
        if !root.join(&p).is_file() {
            bail!("--files {f}: no such file under {}", root.display());
        }
        out.push(p);
    }
    Ok(out)
}

#[derive(Debug, serde::Serialize)]
struct StripSummary {
    files: usize,
    comments: usize,
    kept: usize,
    left: usize,
    without_syntax: usize,
}

fn strip_cmd(c: &Common, cfg: &Loaded, a: &StripArgs) -> Result<u8> {
    let root = cfg.root(c.root.as_deref())?;
    let files = selected(&root, cfg, &a.files)?;
    let macros = strip::macro_set(&root, cfg, &tree::files(&root, cfg)?);
    let scan = strip::scan(&root, cfg, &files, &macros)?;
    let summary = StripSummary {
        files: scan.files.len(),
        comments: scan.files.iter().map(|f| f.comments).sum(),
        kept: scan.files.iter().map(|f| f.kept).sum(),
        left: scan.left.len(),
        without_syntax: scan.without_syntax.len(),
    };
    if a.list {
        for f in &scan.files {
            println!(
                "{:<11} {:>5} comment(s) {:>3} kept  {}",
                f.syntax.label(),
                f.comments,
                f.kept,
                f.path
            );
        }
        for f in &scan.without_syntax {
            println!("{:<11} {:>5}             {f}", Syntax::None.label(), "-");
        }
    }
    if a.check || a.list {
        for l in &scan.left {
            if a.check {
                println!("{}:{}: {}", l.path, l.line, l.text);
            }
        }
        for f in scan.files.iter().filter(|f| !f.problems.is_empty()) {
            for p in &f.problems {
                println!("{}: PROBLEM {p}", f.path);
            }
        }
        println!(
            "strip {}: {} file(s) with a comment syntax ({} without), {} comment(s), {} kept by [[source.keep]], {} left",
            if scan.left.is_empty() { "ok" } else { "check" },
            summary.files,
            summary.without_syntax,
            summary.comments,
            summary.kept,
            summary.left
        );
        json(
            c,
            "src strip",
            &root.display().to_string(),
            Some(&summary),
            &scan.left,
        )?;
        return Ok(if a.check && !scan.left.is_empty() {
            DIFFERS
        } else {
            0
        });
    }
    let forced = a.layout.as_deref().and_then(layout_of);
    let mut refused = 0usize;
    let mut changed = 0usize;
    let mut rows = Vec::new();
    for f in scan.files.iter().filter(|f| f.comments > f.kept) {
        let path = root.join(&f.path);
        let src = std::fs::read(&path).with_context(|| format!("reading {}", path.display()))?;
        let plan = strip::Plan::new(f.syntax, &src, &cfg.keep_for(&f.path), &macros);
        let want = forced.or_else(|| cfg.source.layout.get(&f.path).copied());
        let fmt_cmd = formatter_for(cfg, &f.path)?;
        let run_fmt = |b: &[u8]| -> Result<Vec<u8>, String> {
            strip::run_formatter(fmt_cmd.as_deref().unwrap_or_default(), &path, b)
        };
        let fmt: Option<&strip::Formatter<'_>> = fmt_cmd.as_ref().map(|_| &run_fmt as _);
        let out = plan.best(&src, &macros, want, fmt);
        match out.layout {
            Some(l) => {
                changed += 1;
                println!("{:<9} {}", l.label(), f.path);
                let to = match &a.preview {
                    Some(d) => Some(d.join(&f.path)),
                    None => a.write.then(|| path.clone()),
                };
                if let Some(to) = to {
                    if let Some(p) = to.parent() {
                        std::fs::create_dir_all(p)?;
                    }
                    srcio::replace_bytes(&to, &out.bytes)
                        .with_context(|| format!("writing {}", to.display()))?;
                }
            }
            None => {
                refused += 1;
                println!("REFUSED   {}", f.path);
            }
        }
        for (l, why) in &out.refusals {
            println!("  {}: {why}", l.label());
        }
        rows.push(serde_json::json!({
            "path": f.path,
            "layout": out.layout.map(Layout::label),
            "refusals": out.refusals.iter().map(|(l, w)| format!("{}: {w}", l.label())).collect::<Vec<_>>(),
        }));
    }
    if let Some(d) = &a.preview {
        let mut copied = 0usize;
        for p in &files {
            let to = d.join(p);
            if !to.exists() {
                if let Some(par) = to.parent() {
                    std::fs::create_dir_all(par)?;
                }
                std::fs::copy(root.join(p), &to)
                    .with_context(|| format!("copying {}", p.display()))?;
                copied += 1;
            }
        }
        println!(
            "strip preview: {changed} file(s) stripped, {copied} copied unchanged, under {}",
            d.display()
        );
    }
    println!(
        "strip: {changed} file(s) {}, {refused} refused, {} comment(s) in the tree",
        if a.write {
            "written"
        } else if a.preview.is_some() {
            "previewed"
        } else {
            "would change (pass --write or --preview DIR)"
        },
        summary.comments - summary.kept
    );
    json(
        c,
        "src strip",
        &root.display().to_string(),
        Some(&summary),
        &rows,
    )?;
    Ok(if refused > 0 { DIFFERS } else { 0 })
}
