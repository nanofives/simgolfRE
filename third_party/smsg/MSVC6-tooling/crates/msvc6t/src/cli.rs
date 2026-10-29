use clap::{Args, Parser, Subcommand};
use std::path::PathBuf;

#[derive(Debug, Parser)]
#[command(
    name = "msvc6t",
    version,
    disable_help_subcommand = true,
    about = "Byte-exact decompilation tools for programs built with Visual C++ 6.0",
    long_about = "Byte-exact decompilation tools for programs built with Visual C++ 6.0 (CL, C1, C1XX and C2 \
                  12.00, LINK 6.00): identify the toolchain, set up a project, compile and compare one function at \
                  a time, check that every call and reference names the right target, explain the remaining bytes \
                  by the compiler's own rules, and publish PDBs a debugger loads. This build loads a project (init, check, dsp, \
                  functions, names: one strict msvc6t.toml, the .dsp/.dsw model, function indexes and name \
                  oracles), reads images, \
                  objects and archives (image, obj, lib) and CodeView records and debug information (cv, \
                  debuginfo), holds the source tools (src) and the source rules the \
                  toolkit is built under (lint), and drives the user's Visual C++ 6.0 trees (tc, build, crtguard, \
                  cache): their builds, a wine, container or native runner, pooled prefixes, job tokens, the \
                  reaper, the compile cache, the msvcrt guard and the MSDEV build. It checks each probe's rule \
                  on each of those trees by compiling, linking and matching the probes (probe), and cuts a \
                  repository's source release (publish). It reads the IL the front ends hand C2 and plans which \
                  call sites C2 inlines (il, inline). Exit codes: 0 ok or IDENTICAL, 1 error, 2 usage, 3 DIFFERS."
)]
pub struct Cli {
    #[arg(
        long,
        global = true,
        value_name = "FILE",
        help = "The project config. Default: ./msvc6t.toml, else $MSVC6T_CONFIG. An IMAGE argument that names \
                an [[image]] id is that image's reference, read only after its sha256 matches."
    )]
    pub config: Option<PathBuf>,
    #[command(subcommand)]
    pub cmd: Cmd,
}

#[derive(Debug, Subcommand)]
pub enum Cmd {
    #[command(
        about = "Check the toolkit's own tree: comments, hygiene, rustfmt and clippy",
        long_about = "Check the toolkit's own tree with the pinned Rust toolchain (rust-toolchain.toml, resolved \
                      through rustup by path; RUSTFLAGS, rustc wrappers and target-directory overrides are removed \
                      from its environment). Every mode first runs the build's tree rules and the hygiene checks, \
                      and refuses: a comment, a shebang or a doc attribute (`doc = ...` at any depth; \
                      `#[doc(hidden)]` is allowed) in any .rs file; a `#[path]` or `include!` that is not a plain \
                      literal naming a .rs file inside its crate, an `include_bytes!` that is not a plain literal \
                      or names a .rs file, an `include_str!` (of a .rs file, which dep-info lists as compiled though \
                      no crate compiles it; of any other, text no check reads), a link to a directory inside a \
                      crate, a member's target path or `build` script outside its crate, a `use` that imports \
                      `include!`, `include_str!` or `include_bytes!` from std, core or alloc, a `macro_rules!` or \
                      a `use ... as` that names one of the three (and shadows the builtin), and an attribute a \
                      macro argument names (`#[$name = ...]`); \
                      clippy::disallowed_methods silenced outside a srcio.rs (by its old name, a group holding it, \
                      or renamed_and_removed_lints); a commented .toml or cargo config, a rustfmt or clippy config \
                      but the root rustfmt.toml and clippy.toml or one that switches their rules off, a lint level \
                      a manifest sets for those lints or a member sets at all, and, in the tree's, an ancestor's or \
                      $CARGO_HOME's cargo config, a lint flag in rustflags, a `build.rustc` or rustc wrapper, or an \
                      `[env]` entry for a variable the lint removes; a CACHEDIR.TAG the walk reaches; a tool \
                      script (one of 29 extensions, .sh, .py, .pl, .awk, .bat, .ps1 and .js outside ghidra/ among \
                      them, or a file opening on `#!` and a path); a binary (MZ, ar, MSF, PDB 2.0, .ilk, OLE, COFF \
                      object or import header, PEF, Mach-O, ELF, and the zip, 7z, gzip, cab, rar, xz, bzip2, zstd, \
                      tar and SZDD archives); a project name from examples/*/denylist.txt in the path or content \
                      of any file outside examples/ and docs/ that is not markdown, in a .rs file also in its \
                      decoded string literals and across a `concat!`; and an empty denylist. The files checked \
                      are git's tracked and untracked-but-not-ignored ones. The plain lint then runs `cargo fmt \
                      --all --check` and `cargo clippy --workspace --all-targets --locked -- -D warnings` in a \
                      target directory private to the tree, holds that every .rs file under crates/ is a module \
                      some target compiles, and marks the tree's content key clean for that toolchain. A file is \
                      one when it is in the module tree of a target root `cargo metadata` lists (`mod`, `#[path]` \
                      and `include!` followed as rustc follows them, `cfg(test)` judged for each unit clippy \
                      compiles) and a dep-info file that clippy run wrote names it. A file a compile only reads \
                      (an include_bytes!, a debugger visualizer) is in no module tree and is refused; a .d file \
                      older than the run, or one a build script writes in its OUT_DIR, names nothing; a `mod` or \
                      an `include!` in a macro or a block, an `include!` that is not at an item's head, and a `cfg` \
                      the walk cannot judge for any one unit on a module file or an include (or on an inline module \
                      that holds one), are refused as modules it cannot see."
    )]
    Lint(LintArgs),
    #[command(
        about = "Read a PE image: headers, Rich header, directories, resources; compare and post-link it",
        long_about = "Read a PE32 image. IMAGE is an [[image]] id of the config (its reference, sha256-checked) \
                      or a file path. Every row verb takes --json FILE and writes the envelope {format, command, \
                      image, summary?, rows} from the rows it prints."
    )]
    Image(ImageArgs),
    #[command(
        about = "Compare COFF objects",
        long_about = "Compare COFF objects, one pair or two directories of them."
    )]
    Obj(ObjArgs),
    #[command(
        about = "Read and compare static libraries",
        long_about = "Read and compare COFF archives: static libraries and import libraries."
    )]
    Lib(LibArgs),
    #[command(
        about = "The configured Visual C++ 6.0 toolchains and the runner they run under",
        long_about = "The Visual C++ 6.0 trees the config names ([toolchain.<id>], the user's own copies: no \
                      Microsoft file is part of the toolkit) and the runner ([runner]) every tool runs under."
    )]
    Tc(TcArgs),
    #[command(
        about = "Run an image's MSDEV build in a leased prefix and apply its post-link steps",
        long_about = "Run each image's [image.build]: check every toolchain it names against its `expect` table, \
                      lease a prefix, link each `mount` into its drive_c, copy each `stage` there (sources read \
                      through links, nothing written or removed through one, nothing outside drive_c, no file of \
                      the prefix overwritten), build each `implib`, start the display, and run each `msdev` step \
                      as its own batch file with `cmd /c` under the runner, with INCLUDE, LIB and WINEPATH as the \
                      build gives them and the loads traced. Every native module a step or implib loads must lie \
                      in the toolchain it declares (the step's, else the build's), at the version `expect` names; \
                      a DLL no configured toolchain ships may also come from a `path` or winepath directory, and \
                      the prefix's msvcrt is the crt guard's. The build fails on a toolchain of another build, \
                      an MSDEV error line, a missing log or output, a module outside those, or (with a native \
                      msvcrt configured) a tool process on wine's builtin msvcrt. What was staged is listed outside the prefix and removed \
                      only while it is still what was made. The output and its object directory are \
                      copied to --out, and beside them the libraries the link read that the build made (each \
                      step's `requires`, each `implib`), each `post` command runs in order with {image} and \
                      {reference} substituted, and \
                      then the image's `clock` fields (msvc6-image's vocabulary) are copied from its reference. \
                      The md5 printed is the final image's. Every compiler-running verb first checks this binary's \
                      provenance (`msvc6t lint --provenance`)."
    )]
    Build(BuildArgs),
    #[command(
        about = "Check that every tool in a wine run loaded the native msvcrt",
        long_about = "Read a `WINEDEBUG=+loaddll` log and pass only if every tool process (any executable \
                      outside C:\\windows) that mapped msvcrt.dll mapped it native, and each --require tool \
                      (default LINK.EXE) is seen doing so. LINK 6.00 sorts tied keys with msvcrt's qsort (the IAT \
                      order inside a DLL, ICF survivors), so a link on wine's builtin msvcrt is another link. A \
                      log in which the required tool never ran proves nothing and fails."
    )]
    Crtguard(CrtguardArgs),
    #[command(
        about = "Check each probe's rule on each configured toolchain: compile, link and match the probes",
        long_about = "Run each probe on each toolchain and check what CL and LINK emit against the probe's \
                      .expect. A probe is <name>.c (C1.DLL) or <name>.cpp (C1XX.DLL) beside <name>.expect, both \
                      comment-free: the source is read by the VC6 lexer and refused when it holds a comment. \
                      Each runs in a leased prefix through the compile cache: `CL /nologo /c <cl> /FAs \
                      /Fa<name>.asm /Fo<name>.obj <name>.<ext>`, then with a `link` line `LINK /nologo <link> \
                      /out:<name>.exe|dll /map:<name>.map <name>.obj`, traced with +loaddll so that a LINK that \
                      did not load the native msvcrt fails (its qsort orders imports and picks ICF survivors). \
                      The .expect holds one directive per line; an indented line is a pattern of the block \
                      above it; a `#` line is refused. Directives: `rule STAGE: TEXT` (one or more: the rule shown, \
                      STAGE the pass it is about, cl, c1, c1xx, c2 or link); `cl SWITCHES` (default /O2); \
                      `link SWITCHES`; `link-crt native|builtin` (default native; \
                      builtin runs LINK on wine's msvcrt, for a contrast); `include FILE` (a header beside the \
                      probe, by its plain name, comment-free, copied in with the source); `xfail REASON` (the rule as written is \
                      refuted: a failing block is XFAIL, a passing probe XPASS). Blocks, each followed by its \
                      patterns: `asm FN` (FN's instruction and label lines in the listing, by its PROC name), \
                      `frame FN` (the frame-symbol lines above FN's PROC), `data` (the listing outside every \
                      function), `clout` (what CL printed), `obj` (the object: `comp.id`, `section N NAME KIND \
                      rwx size= align= [comdat=SEL SYMBOL]`, `drectve DIRECTIVE`, `symbol NAME s<N>+OFFSET|undef|common \
                      N|abs CLASS [function]`), and after a link `map` (the map from `Publics by Value` on), \
                      `linkout` (what LINK printed), `rich` (each Rich stamp: product, build, count), `imports` \
                      (`DLL NAME` in IAT order); `listing` (every code line of the listing, PROC and ENDP lines \
                      included), and `il` (the IL the compile keeps: CL also gets /Bk<name>. and bypasses the \
                      compile cache, and the view is `il dump`, then `== funcs` and `il funcs`, then `== ids` and \
                      `il ids`, decoded with the toolchain's own C2.DLL). A block written `asm=` (any block with `=`) matches every line \
                      exactly, one pattern each. Lines compare with whitespace runs collapsed and listing `;` \
                      comments dropped; in a pattern `*` matches any run, `{name}` captures a token (letters, \
                      digits, `_ $ @ ? .`) at its first match and must equal it at every later use in its block, \
                      and a leading `!` makes a pattern that must not match between the positive matches around \
                      it. `{build:cl}`, `{build:c1}`, `{build:c1xx}`, `{build:c2}` and `{build:link}` stand for \
                      the build number of that tool in the toolchain being run, so a rule about builds holds on \
                      every tree. Positive patterns match in order, each on the first fitting line after the \
                      last. The shim directives (`mode`, `hooks`, `set`, `filter`, `rec`, `diagnostic`, `cut`, \
                      `cutobj`, `cutasm`) need the C2 tracing shim and are refused. A link probe is SKIPped when \
                      [runner] names no native_msvcrt. Each run's files stay under --out/<toolchain>/<probe>/, \
                      the views a block reads beside them as view-<view>.txt. Every row goes to the results \
                      file: probe, source, toolchain, the CL, front-end, C2 and LINK builds, status (PASS FAIL \
                      XFAIL XPASS ERROR SKIP) and whether the toolchain gates. Exits 3 when a gated run FAILs or \
                      XPASSes, 1 on an error. Every compiler-running verb first checks this binary's provenance \
                      (`msvc6t lint --provenance`)."
    )]
    Probe(ProbeArgs),
    #[command(
        about = "The content-addressed compile cache: stats, gc, verify",
        long_about = "The compile cache ([cache] dir, default ~/.cache/msvc6t/cl) keys each CL result by the \
                      toolchain's content hashes, the switches that are not paths, the preprocessed text with \
                      #line paths reduced to file names, and the content of every file the preprocessor opened; a \
                      direct-mode manifest skips the CL /P on a repeat while every input and searched directory is \
                      unchanged. Entries appear by one rename, each part's sha256 is recorded and checked on \
                      restore, and a compile whose inputs changed while it ran stores nothing. MSVC6T_CACHE=0 \
                      turns it off."
    )]
    Cache(CacheArgs),
    #[command(
        about = "Write a starting msvc6t.toml for the images given",
        long_about = "Write a msvc6t.toml (schema 1) for the images given: a [toolchain.vc6] whose root is \
                      ${MSVC6T_VC6} (or --vc6), a wine [runner], and one [[image]] per --image with its id (the \
                      file stem upper-cased, or --id), its reference relative to the config, its sha256, the \
                      discover function index, the exports and imports name oracles, and every clock field the \
                      image holds. --dsw or --dsp adds the image's `project`, in --project-config or the project \
                      file's default configuration (its CFG= line). The text is loaded back through the strict \
                      loader before it is written; an existing file is refused unless --force."
    )]
    Init(InitArgs),
    #[command(
        about = "Check the project config and every input it names",
        long_about = "Load msvc6t.toml strictly (one schema, every table under deny_unknown_fields: an unknown or \
                      misspelt key is an error naming the file, line, column and key) and check what it names: \
                      each toolchain's tree and the builds its `expect` table names; the runner's settings; each \
                      image's reference (read only after its sha256 matches), its clock fields and residue rows \
                      located on it, its objdir, map and libraries; its project, every translation unit resolved \
                      to a compile spec with its source and /I directories present; each function index, \
                      name oracle and dropped-name list read; its build's mount, stage, implib and post inputs \
                      present; the [source] root and the reccmp project. A top-level `optional` lists the \
                      variables a reader may leave unset: an input that names one while it is unset is absent, so \
                      its row is `skip`, an index leaves its source out and a verb that needs it says so. One row \
                      per check; exit 1 when any fails. JSON rows: status (ok, fail or skip), subject, check, \
                      detail."
    )]
    Check(CheckArgs),
    #[command(
        about = "The .dsp/.dsw model: each translation unit's compile spec",
        long_about = "Read Developer Studio 6 project files as MSDEV builds them: a .dsw's projects and their \
                      dependencies (a project builds after the projects it depends on), a .dsp's configurations \
                      (`!IF \"$(CFG)\" == ...` arms, nested), the project-wide `# ADD CPP` / `# SUBTRACT CPP` of \
                      the configuration, and each source block's own (a file's subtraction removes a project-wide \
                      switch too; `# PROP Exclude_From_Build` drops the file). What the reader does not model (a \
                      custom build step or Special Build Tool, per file or project-level, another condition, a second \
                      active # ADD CPP) is refused, by line."
    )]
    Dsp(DspArgs),
    #[command(
        about = "An image's function index: the union of its providers",
        long_about = "Read the image's `functions` providers in their order and print the union, keyed by entry \
                      rva: the first provider to name an address gives its row, and a later one fills an extent, \
                      name or object the earlier lacked. Providers: ghidra-json:<file> (functions.json format 1, \
                      or a bare array of {name, addr, thunk, ins} records: an entry is the record's own address, \
                      never the lowest of its instructions, and Ghidra's generated names are dropped), \
                      map[:<file>] (a LINK /MAP's `f` rows, publics and statics), reccmp-csv:<file> (rows whose \
                      type is a function's) and discover (the entry point, exports, call rel32 targets, /Od \
                      prologues and 16-byte lines after a return and pad, code pointers in data; a jmp thunk is \
                      a function marked thunk, and its target is one too; copy-protection sections are skipped). \
                      JSON rows: rva, end, name, thunk, provider, object."
    )]
    Functions(FunctionsArgs),
    #[command(
        about = "An image's name oracle: every name its providers give",
        long_about = "Read the image's `names` providers in their order; at an rva the first provider's name \
                      wins. Providers: tsv:<file> (rows `rva name kind [size] [scope]`, tab-separated, rva \
                      0x-hexadecimal, kind function, data, import, thunk, string, vftable or label, scope the \
                      object stem of a static or `_$E` name; an optional `rva` header row), map[:<file>] (every \
                      public and static of a LINK /MAP, a static scoped to its object), exports, imports (each \
                      IAT slot as <dll>!<name>), reccmp-markers:<dir> (reccmp's `// TYPE: MODULE 0xaddr` markers \
                      for this image's id in the sources under <dir>, each named by the definition or comment \
                      below it) and reccmp-csv:<file> (an address,name,type[,size] table). JSON rows: rva, name, \
                      kind, size, scope, provider."
    )]
    Names(NamesArgs),
    #[command(
        about = "Compile one translation unit with its project's compile spec",
        long_about = "Compile SOURCE (a TU of the image's project, or a copy whose file stem names exactly one TU: \
                      `Plot_v2.cpp` and `zz_Plot.cpp` name `Plot.cpp`; --args-from names it outright) with the \
                      spec `dsp show` gives that TU: its code switches (--cflags replaces them), its /I \
                      directories, then each --include, and INCLUDE as the image's [image.build] gives it, \
                      each directory mapped through the build's mounts to its host path. The source is copied \
                      into the work directory under its own file name, so the object records the name the \
                      TU's own compile does, and compiled there on the configured runner in a leased prefix, \
                      through the compile cache, by the TU's toolchain (else the image's `compiler`); \
                      --listing adds CL's /FAcs listing. Prints CL's output and the object. Every \
                      compiler-running verb first checks this binary's provenance (`msvc6t lint \
                      --provenance`). JSON rows: the source, object, listing, toolchain, cache outcome and CL \
                      arguments."
    )]
    Compile(CompileArgs),
    #[command(
        about = "Compile one translation unit and score each of its functions against the reference, byte for byte",
        long_about = "Compile SOURCE as `compile` does (the spec `dsp show` gives its TU, --cflags replacing the \
                      code switches, each --include after the spec's directories, --args-from naming the TU \
                      outright) in <temp>/msvc6t-work/<image>/<stem>, or in a private <stem>_<TAG> directory \
                      --isolate names, holding that directory's lock for the run, and score every .text COMDAT \
                      of the object against the reference image. COMDAT[0] is compared at RVA (a 0x-hexadecimal \
                      rva or a name the image's names resolve). Each later COMDAT is SEATED: at its --members \
                      address (COMDATs 1..n in object order); with --auto-members at the address its own symbol \
                      resolves to (exports, then each name provider in order, a row scoped to the TU's stem \
                      before a flat one, an ILT slot followed to its body; a non-EH vector iterator with no name \
                      sits on the object's ??_E), where RVA is a hint (`auto`, COMDAT[0]'s address or any \
                      COMDAT's) and the run moves to COMDAT[0]'s; otherwise predicted from the previous member \
                      by LINK's placement laws, an incremental link's reserve align16(start + len + len/4) \
                      first, then packing at the COMDAT's own alignment. A seat holds only where the image \
                      confirms it: a recorded function of the COMDAT's length starts there; or the recorded \
                      function is shorter by exactly one declared text-pad range, by CL's own 0x90 tail \
                      (the same bytes in the image), or by the switch tables the body names; or, for an \
                      asserted address no recorded function starts at, the image's instructions from it end \
                      exactly at the COMDAT's length on a jmp or ret and link fill (0xcc or 0x90) runs to \
                      the next 16-byte boundary. Otherwise the row is MISASSERTED (an address given) or \
                      UNSEATED (a prediction), and its figures compare whatever lies there. A weak-default \
                      ??_G, a non-EH vector iterator or a non-EH ??_E seated on the image's EH vector deleting \
                      destructor is SUPERSEDED: the image holds another unit's body there, and it is not \
                      scored. A name on the image's `dropped` list (a COMDAT the original link's /OPT:REF \
                      removed) is DROPPED: under --auto-members it holds the place of the member before it (at \
                      the head of the object, of the first one the image holds), and it is not scored. Every \
                      other byte is compared except the fields the object's relocations name \
                      and the bytes of a declared text-pad residue; a differing COMDAT prints its \
                      instruction diff, and --block-map / --align add its basic-block map and its shape \
                      alignment's drift. The last block is the verdict, `VERDICT: IDENTICAL  -- 0 of N \
                      comparable bytes differ` or DIFFERS, with each later member's share. Names the object \
                      needs are not resolved here (the identity gates check calls and references). Like \
                      every compiler-running verb, it first checks this binary's provenance. Exit 0 \
                      IDENTICAL, 3 DIFFERS. JSON rows: one per COMDAT (rva, seat, symbol, name, len, \
                      differing, comparable, masked, relocs); the summary holds the verdict, the anchor \
                      under --auto-members and the totals."
    )]
    Try(TryArgs),
    #[command(
        about = "One function cut into drift-immune segments, each scored on its own",
        long_about = "Seat FUNC (a 0x-hexadecimal rva, or a name the image's name oracle resolves) in the objects \
                      (--objs DIR, each with the static libraries its workspace builds beside it unless --no-libs, \
                      or --obj FILE): the first object in link order whose .text COMDAT's symbol resolves to the \
                      rva, or the COMDAT --symbol names. The image side is the reference's bytes over the \
                      function's recorded extent (the function index's, a `discover` extent not counted; else up \
                      to the next recorded start), cut at its first in-function table and after its last \
                      reachable instruction; ours is the COMDAT, cut likewise and at CL's 0x90 tail or a \
                      declared text-pad residue. Both are cut at ANCHORS whose identity does not depend on \
                      layout (a call to a resolved target, an ILT slot followed; a named, literal or constant \
                      data operand; an import; ret; a switch dispatch), matched by an LCS and kept only where \
                      the function-wide instruction alignment pairs them; a run under 50 image bytes merges \
                      forward and one over --max is cut again at aligned block leaders, then aligned calls. Each \
                      segment is EXACT (equal but for relocated fields and outbound branch displacements), \
                      REG-ONLY (the same instructions with registers and esp displacements masked) or SHAPE. \
                      `diff` is positional; `edit` is the segment's slice of the function-wide aligned edit (an \
                      inserted instruction costs its own length, every branch displacement masked), so the \
                      segments' edits sum to the function's. `regmap` names each register (and esp/ebp frame \
                      slot) our paired instructions hold where the image holds another: `ecx->esi@VA 5/5`, ours \
                      ecx read as the image's esi, first at VA, in 5 of ours ecx's 5 paired uses. --asm N, N-M \
                      or all prints those segments side by side, image | ours, one row per aligned row (` ` same \
                      bytes, `~` one form with other bytes, `*` two forms paired by position, `<` image only, \
                      `>` ours only), each address operand named: ours by its relocation's own symbol (an \
                      import as __imp_X, a literal by its text, a nonzero addend signed after it), at the rva \
                      the name oracle resolves it to in the object's scope and its definer's, else where the \
                      function-wide pairing places it; the image's by the paired relocation that addresses it, \
                      else by the IAT, the name oracle's first name at the rva, the function index's name, a \
                      code address or a literal's text; then the rows that address alike but read two ways, and \
                      the names shown for a relocation that are not its own (--out FILE: the listing there). \
                      Exit 0 when every segment is EXACT, else 3. JSON rows: one per segment."
    )]
    Segments(SegmentsArgs),
    #[command(
        about = "Every seated function of a build ranked by its segments' figures and its raw bytes",
        long_about = "Seat every .text COMDAT of the objects (--objs DIR, repeatable, each with the static \
                      libraries its workspace builds beside it, in LINK's order: the build log's response file, \
                      then each library's members in the order the image's /MAP pulled them) at the rva its \
                      symbol resolves to (exports first, then each name provider in order, a scope naming the \
                      object's stem before a flat row; an ILT slot is followed to its body), the first object in \
                      that order winning an rva and a vector deleting destructor superseding a scalar one. Score \
                      each (--rvas FILE: the first column's rvas; --all-differing: every member `try` \
                      reads as differing and not UNSEATED; default every seated one) as `segments` does, \
                      and its RAW bytes: every byte that differs at the seat, branch displacements included, a \
                      relocation field free only when our target links to the address the image's field holds \
                      (a seat, an absolute name, an import, a recorded name read in the object's and its \
                      definer's scope, a section base the reference's fields vote for, a library name's vote; \
                      an ILT slot against its body is free), CL's trailing fill against the image's pad exempt. \
                      A field no source resolves is `unres`. Rows `rva name differing segs exact/reg/shape edit \
                      image pct addend raw unres rawflat`, largest `differing` first (`rawflat`: raw with no \
                      scope read); --prev FILE prints the rows that moved; the total and raw lines close the \
                      output. --decisions FILE charges every aligned edit byte of every scored function to the \
                      compiler decision its residue's shape names (frame-slot sort, register priority tie, \
                      rotation pick, region split, block layout, branch test form, scheduler pure or \
                      RA-coupled, operand rank/hash, CSE/forward-substitution, RA reload/copy-fold, inliner \
                      budget, C1 counter, other), one row per segment and class (`rva name seg image_va verdict \
                      seg_edit class bytes rows first upstream`; an RA-coupled row's `upstream` names the \
                      register our web holds against the image's), and prints the per-class totals; \
                      --upstream FILE leads each RA-coupled row's upstream with the traced decision its \
                      `#regs` rows give for the segment. A ranking of the classes, not a proof. An object \
                      that cannot be read or named (not a COFF object, a table past its end) is REFUSED: each \
                      is named on stderr with its error, a REFUSED line after the raw line names them, the \
                      summary's `refused` lists them, and the exit is 1. Otherwise exit 0 when every edit and \
                      raw byte is 0, else 3. JSON rows: one per function, the totals and the refused objects \
                      as the summary."
    )]
    Segrank(SegrankArgs),
    #[command(
        about = "Does every relocation of each byte-identical function name what the image's operand names?",
        long_about = "RELOCATION IDENTITY. `try` masks every relocated field, so a body that calls the wrong \
                      function, reads the wrong global or pushes the wrong literal is IDENTICAL by its verdict. \
                      Seat every .text COMDAT of the objects (--objs, as segrank seats them), keep the ones \
                      identical under try's mask (no differing byte outside a relocation, no length gap), and \
                      resolve each relocation with segrank's linker model: a seated COMDAT, an import's IAT slot, \
                      a recorded name, an absolute, or a VOTE (a section or library name whose address no name \
                      records, elected from the image's own sites). A vote is fitted to the image, so it is no \
                      evidence: each fitted target is checked against an oracle that did not fit it, a library \
                      member's own bytes (any copy of the image's `libraries`, relocations masked, held at the \
                      elected address: LIBRARY, else LIBRARY-CONFLICT), the names recorded at the address (a name \
                      some object defines: NAME-CONFLICT; one none defines: STALE-NAME), the image's base \
                      relocations (every relocated dword of the body must be a DIR32 of ours and back: \
                      NOT-RELOCATED, EXTRA-RELOC) or, in an image with no base relocations, its absolute operands \
                      (one our field does not relocate that lands on a name an object defines: LITERAL; on another \
                      name: LITERAL-WEAK), and the function index's names (GHIDRA, GHIDRA-DIFF). Findings, which \
                      fail: MISMATCH, WRONG-DATUM, LIBRARY-CONFLICT, NAME-CONFLICT, NOT-RELOCATED, EXTRA-RELOC, \
                      LITERAL. Leads: LAYOUT, STALE-NAME, GHIDRA-DIFF, LITERAL-WEAK, UNRESOLVED. Proofs: NAMED, \
                      WEAK, ILT, CONTENT, LIBRARY, GHIDRA; agreeing by construction: VOTED, FITTED. A name source \
                      given with --notes-only (its path, or a trailing part of it) is bookkeeping: its names are \
                      notes, never a conflict. Prints the findings and leads as TSV (every field with --all or \
                      --fn), each finding as a line, and the tally. Exit 0 when clean, 3 on a finding, 1 when an \
                      object or archive could not be read."
    )]
    Relocid(RelocidArgs),
    #[command(
        about = "Does each of our call sites name the function the reference calls there?",
        long_about = "CALL TARGETS. For a COMDAT whose name resolves to the image's address R and an external \
                      relocation at offset k naming S: read what the image targets at R+k (one ILT hop \
                      followed) and compare it with S's own address, the address S's COMDAT seats at. Our \
                      objects give the name and the site, the image the truth; no image of ours is read. A \
                      COMDAT the link drops for the image's vector deleting destructor (try's SUPERSEDED) and a \
                      SELECT_ANY copy a later object in link order holds with other bytes (try's DUPLICATE) are \
                      not checked. A callee no object seats is UNCOVERED and counted by kind: IMPORT (the image \
                      imports it), OWN DATA (an object defines it as data, or --map's data rows name it) or \
                      LIBRARY. --causes FILE writes every disagreed row with the cause its calling body's callee \
                      census gives (a-misaligned, a-position, b-wrong-name, c-we-call, c-image-calls, \
                      d-unseated-deficit) and FILE.bodies.tsv the censuses. Exit 0 when every checked site \
                      agrees, 3 when one does not, 1 on an error."
    )]
    Calltargets(CalltargetsArgs),
    #[command(
        about = "Does every relocated address operand of our linked image name the symbol the image names?",
        long_about = "THE ADDRESS-SYMBOL SCREEN. Each function's own instruction alignment (segments') pairs \
                      every image instruction with ours. For each pair of one form, the absolute operands (a \
                      disp32 or imm32 inside the image, a call rel32's target) are read unmasked: the image's from \
                      the reference, ours from our LINKED image (--exe) at the address its LINK /MAP (--map) \
                      places the function, checked against the object's bytes so a stale image is refused. Ours \
                      is named by its relocation's symbol through the map (in the referring object first); that \
                      name's reference address comes from its definer's scope, else the flat names, else one \
                      other scope (OTHER-SCOPE), else the IAT. AGREE when it plus the offset is the image's \
                      operand; MISMATCH when it lands elsewhere (a wrong global, field, literal or callee), SWAP \
                      when the image names our address within 8 aligned rows; with no recorded address, CONTENT / \
                      CONTENT-DIFF (the data bytes at both addresses agree or not), UNNAMED, CODE, VFTABLE, LABEL; \
                      COUNT when the two instructions carry a different number of operands; SELF and SELF-SHIFT \
                      for an operand inside the function. Prints the non-AGREE rows as TSV (--all: every row), \
                      the per-function tally with --fn, the TOTAL and SPLIT lines (one symbol of ours whose \
                      sites imply two image bases). Exit 0 with no MISMATCH, 3 with one, 1 on an error."
    )]
    Addrsym(AddrsymArgs),
    #[command(
        about = "Give every byte our linked image differs from the reference by an owner and a cause",
        long_about = "THE ACCOUNT. Every byte where OURS differs from the reference gets a class, and a byte in \
                      none fails. Within each section both images hold, section-relative: OURS (under a /MAP \
                      symbol of an explicit object, or of a member of an --own library), LIBRARY (under an \
                      archive member's symbol, `lib:member.obj`), GENERATED (a section that publishes no symbol, \
                      because LINK emits it; an ILT thunk below the first symbol in both images, or a slot only \
                      the longer of two lattices has; the debug, import and IAT directories, rounded up to 16) \
                      or UNCLASSIFIED (below the first symbol of a section that has symbols: the failure). Past \
                      the section walk: HEADERS (before the first section's raw data), DEBUG (the CodeView \
                      record the debug directory names, in either image), SHIFTED (the two images give the file \
                      offset to different sections, or one section at different raw starts) and PADDING \
                      (FileAlignment filler past one image's content); a byte in none of these is UNSEEN and \
                      fails. With sections at equal raw offsets the classified total must reach the whole \
                      file's differing count. A data section with OURS > 0 is tested for CONTENT: each distinct \
                      aligned 4-byte value inside our own symbols' spans (a base-relocated dword skipped; with no \
                      base relocations, a dword both images hold a code address in) must occur somewhere in the \
                      reference's copy of the section, else it is a CONTENT DIFFERENCE and fails. --objs gives \
                      the .text cross-check: our objects' COMDAT bytes at their seats, against the hull figure. \
                      Exit 0 when the account is complete, 3 when a byte is UNCLASSIFIED or UNSEEN or a content \
                      value is absent, 1 on an error."
    )]
    Account(AccountArgs),
    #[command(
        about = "Does every string literal a function names sit where the reference's instruction points?",
        long_about = "LITERALS. try masks relocated operands, so two same-shaped sites naming each other's \
                      strings compile byte-identical under the mask. For each .text COMDAT of the objects whose \
                      address a name gives (its object's scope, the flat names, --map, a --notes-only source), \
                      every DIR32 relocation to a `??_C@` literal the object defines implies an address: the \
                      reference's dword in the field of the instruction the alignment pairs with ours (keys \
                      with relocated and image-address fields, esp displacements and branch targets masked, \
                      aligned by an LCS), less our addend. The literal's bytes, through its terminator, must be \
                      the reference's there; an address with no bytes on disk is a MISMATCH. A paired \
                      instruction naming another string is a MISMATCH on one unbroken diagonal, and NOT CHECKED \
                      when the reference (or ours) names the other's string across a gap (drift among identical \
                      idioms). NOT CHECKED and counted: a site with no paired instruction, an unplaced COMDAT, a \
                      `??_G`/`??_E` body, a site in a data section, a literal another object defines. It reads \
                      our objects and the reference, never an image of ours. Exit 0 when every checked site \
                      names the reference's string, 3 on a mismatch, 1 on an error."
    )]
    Literals(LiteralsArgs),
    #[command(
        about = "Does every near branch land on the instruction the image's branch lands on?",
        long_about = "THE BRANCH-TARGET GATE. segrank's edit masks every near branch's displacement, so a \
                      function whose bytes are exact but whose `je` lands on the wrong instruction scores 0. \
                      Through the function-wide instruction alignment segments scores, every near branch (jcc \
                      or jmp, rel8 or rel32; no call, no indirect jump) of the image is paired with the \
                      instruction aligned to it in ours and the two TARGETS compared through the same \
                      alignment: paired when both are one aligned pair; MISMATCH when they land before \
                      different aligned pairs; TARGET-UNALIGNED when they land before the same pair but a \
                      target has no counterpart (an instruction ours inserted or deleted); UNPAIRED when the \
                      counterpart is no branch. A branch that leaves the function on both sides is compared by \
                      DESTINATION: ours through its relocation as LINK would resolve it (segrank's linker \
                      model; one into its own section read off the object), the image's off its bytes, both \
                      through one ILT hop. With FUNC: one function, its findings and the landing check. \
                      Without: every seated function, and every duplicate external COMDAT copy the link does \
                      not take compared with the one it does (bytes and relocations, a per-TU `$L` label as \
                      any other). --excuses FILE names TARGET-UNALIGNED rows (`[[named]]`: image, rva, ours, \
                      why) and differing duplicate copies (`[[duplicate]]`: image, name, object, md5 of its \
                      bytes and relocations, why) a reviewer accepted; a MISMATCH is never excused. Exit 0 when \
                      it passes, 3 on a mismatch, an unexcused row, a differing copy or an unchecked function, \
                      1 on an error."
    )]
    Jumps(JumpsArgs),
    #[command(
        about = "Paired instructions whose operands name another parameter, local, global, field, constant or callee",
        long_about = "SEMANTIC OPERAND MISMATCHES. Over the instruction pairs segments' alignment gives as \
                      Same or Form, operand by operand, registers never compared. A frame operand is rebased \
                      to the entry esp by a push-depth walk (push/pop, sub/add esp, a callee's own `ret N` \
                      read off the image, an import's arguments, the /GZ esp check, the /GX registration); \
                      entry+4 and above is a PARAMETER, below 0 a LOCAL judged only against the function's \
                      majority slot map. Classes and base certainty: CALL 0.9, PARAM 0.9, RETN 0.9, GLOBAL \
                      0.8, STRING 0.8, GLOBAL+ 0.6 (0.4 indexed), IMM 0.7 pushed else 0.5, PARAM/LOCAL 0.5, \
                      FIELD 0.5, LOCAL 0.3. A pair whose image operand ours reads within 8 instructions is a \
                      reorder and dropped (0.6x when the image reads ours nearby too); a FIELD delta seen 3 \
                      times is an IV rebase (0.3x); the certainty is scaled by the Same rows around the pair. \
                      One TSV row per hit, most certain first (--min-cert drops below). --depths prints, to \
                      stderr, each paired row where ours' esp depth minus the image's changes. A screen: exit 0 \
                      unless an error."
    )]
    Operands(OperandsArgs),
    #[command(
        about = "Pair every global the reference's code touches with the one our linked image's code touches",
        long_about = "GLOBALS PAIRED BY (FUNCTION, OFFSET). Each base relocation of the reference's .text sits \
                      at the same offset in our function when the body is exact, so the two dwords there name \
                      the same global. Functions pair by name (OURS's LINK /MAP code rows, resolved in the \
                      image's names); a site pairs only when the 8 bytes before it agree in both images and \
                      both dwords land in a section other than .text. Prints each distinct (reference, ours) \
                      pair whose delta is not 0 (--all: every pair), the largest delta first. Needs a reference \
                      with base relocations. A delta of 0 says our global lands where the reference's does, \
                      nothing of its name, type or extent; a global reached only through a register computed \
                      elsewhere is not seen. A screen: exit 0 unless an error."
    )]
    Globals(GlobalsArgs),
    #[command(
        about = "Caller by callee direct-call counts, the image against our objects",
        long_about = "THE CALL TABLE. The image side is a linear sweep of .text: every `e8 rel32` call, its \
                      target followed through up to four `e9` thunks, credited to the nearest preceding \
                      function start (the function index's entries, our members' seats and every resolved \
                      call target). Our side is every REL32 relocation after an `e8` in every .text and \
                      .text$x COMDAT; a funclet's calls go to a pseudo-caller named for its parent. Names are \
                      joined as try's members table joins them: the COMDAT's seat in its own object, then any \
                      object's, then the image's names, the function index's, and a read-out of the image at \
                      our seated call sites (the majority target). A caller another COMDAT already holds is an \
                      inline duplicate or an identical-function-folding alias, listed under `aliases`. --diff \
                      prints only the pairs whose counts differ over callers both sides define; --by-callee \
                      per-callee totals. A screen: exit 0 unless an error."
    )]
    Calls(CallsArgs),
    #[command(
        about = "Where each of our objects' data sits in the reference, and who owns a map's data spans",
        long_about = "Data placement: `data seat` seats each object's .data, .rdata and .bss in the reference by \
                      the votes of its relocations, `data runs` derives where the reference put each object's \
                      data and which objects it interleaves, `data owner` reads a LINK /MAP's data spans per \
                      object and gives every differing data byte of a linked image its owner."
    )]
    Data(DataArgs),
    #[command(
        about = "Where our link put each own function against the reference, and which run of objects it reads",
        long_about = "Code placement: `place map` reads our link's /MAP against the reference's addresses (and \
                      its ILT lattice, the ILINK reservation law, the body bytes), `place seated` counts the own \
                      COMDATs byte-identical at their reference address in a linked image, `place partition` \
                      reads the reference's own-code layout as runs of our objects, `place locate` finds the \
                      reference's body at an rva in an image of ours."
    )]
    Place(PlaceArgs),
    #[command(
        about = "Layout models an incremental link leaves behind: growth reserve, member and pull order, holes, orphan data",
        long_about = "What the layout of a link says once the objects exist, measured as a census rather than read \
                      off one address: the growth reserve after every function (`growth', `slots'), the archive \
                      member order our references imply (`member-order', `pull-order'), our own code's predicted \
                      layout (`layout', `owner'), the 0xcc holes an incremental history left (`holes'), the data no \
                      kept section reaches (`dead-data'), the COMDAT count a link carries (`comdats') and the sizes \
                      an incremental link froze in its own `.ilk' (`ilk'). Exit codes: 0 ok, 1 error, 2 usage, 3 the \
                      model's own control failed."
    )]
    Placement(Box<crate::growth::PlacementArgs>),
    #[command(
        about = "Write a function's one-function translation unit from its TU and prove it compiles the same",
        long_about = "Write the ONE-FUNCTION translation unit of FUNC (a 0x-hexadecimal rva or a name the image's \
                      names resolve) from its project TU (--src, else the one TU of the image whose definitions \
                      name the function: --name, else the image's name at the rva), and PROVE it. The TU is split \
                      into top-level items by the Visual C++ 6.0 lexer (comments, strings, characters, \
                      preprocessor lines and `#if 0` text known); every non-definition is kept verbatim; of the \
                      function definitions, every overload of the target's name is kept, and so, to a fixed \
                      point, is any static, inline or template definition a kept item names and any definition \
                      ahead of the target a kept body or macro names (C2 compiles a call to a callee it has seen \
                      differently); a definition whose conditional group crosses its bounds stays whole; every \
                      other definition goes with the blank and comment lines above it. A `#line` after each \
                      dropped run keeps the kept text at its TU line numbers. Both files then compile side by \
                      side under the TU's file name with the TU's compile spec (--args-from names another TU's) \
                      and CL's /FAcs listing, each in its own directory under --out (default \
                      <temp>/msvc6t-work/<image>/iso-<rva>). The function's COMDAT is found in the TU's object by \
                      its seat and in the iso's by that COMDAT's symbol; when the listing's first `$L` label of \
                      the function shows C1's label counter moved mod 64, a counter pad (`extern int \
                      iso_counter_pad0, ...;`, one declarator per count) is written before the target and the iso \
                      compiled again, at most three times. FAITHFUL = equal length, equal bytes (relocated fields \
                      included) and equal relocations (offset, type, target), a target differing only by a \
                      compiler number counted as renumbered: the whole name $SG<n>, $L<n> or _$E<n>, or a static \
                      guard ?$S<n>@...; every other target, a template's value argument ($00, $01) and an EH \
                      funclet's state ($0) included, must be equal. DIFFERS names the suspects (the label \
                      counter, an _$E number, a precompiled header, an inline closure). Both objects are also \
                      scored by aligned segments. The proof is recorded as iso.json beside the iso file, which \
                      `loop` reads. Every compiler-running verb first checks this binary's provenance. Exit 0 \
                      when FAITHFUL, 3 when DIFFERS. JSON row: the proof."
    )]
    Iso(IsoArgs),
    #[command(
        about = "One function's edit loop: compile a file and its variants, score each, show what differs",
        long_about = "Compile FILE and every VARIANT at once (-j, default 4, each in its own directory under \
                      --work, default <temp>/msvc6t-work/<image>/loop/<rva>), each reduced to the function's \
                      one-function TU as `iso` writes it (the reduction is idempotent, so FILE may be the TU, a \
                      copy of it or the iso file) unless --whole, and staged under the TU's own file name with \
                      the TU's compile spec (--args-from names another TU's). The function's proof is read from \
                      `iso`'s iso.json (--iso DIR): an iso whose proof DIFFERS is refused without --whole, and \
                      with no proof the TU FILE names is compiled once to find the function's symbol and the \
                      reduction is used UNPROVEN. Each object's COMDAT of that symbol is scored as `segments` \
                      scores it (the aligned edit). The report is at most about 40 lines: the base file's edit and \
                      its delta against this function's previous loop run (a history TSV, \
                      <temp>/msvc6t-work/<image>/loop/<rva>.tsv, or <image>-<rva>.tsv under --history DIR), the \
                      variants ranked by edit with the segments each moved, then the best file's differing \
                      segments (image | ours instructions, differing rows with a row of context, at most \
                      --asm-lines) with the source lines that emitted them, read from CL's /FAcs listing \
                      (fail-soft: no listing, no annotation). --frontier prints the best file's first differing \
                      segment and its source lines instead. The objects, listings and full rows stay under \
                      <work>/last. Exit 0 when the best file's edit is 0, else 3. JSON rows: one per file, the \
                      run as the summary."
    )]
    Loop(LoopArgs),
    #[command(
        about = "C1's entity and label counter: price a text by the model, check the model against CL",
        long_about = "C1's one entity and label counter, which a label-keyed C2 tie reads mod 64: `price` \
                      predicts a header's or a TU's cost from its text, `check` holds that prediction to CL's \
                      own labels span by span."
    )]
    C1(Box<crate::model::C1Args>),
    #[command(
        about = "The order CL gives a TU's .bss, and where the reference disagrees",
        long_about = "C2 keeps every module-scope symbol in one chained table of 1024 buckets and lays the \
                      uninitialised ones down bucket by bucket at the end of the module, each chain \
                      latest-declared first. A bucket is the front end's identifier hash `h = h*4 + (h >> 4) + \
                      c` folded `(h >> 16) ^ (h & 0xffff)` and masked to 10 bits, over the identifier of a \
                      global, file static or function-local static, the namespace's identifier for a namespace \
                      variable, and the whole decorated name of a class static member. A variable defined with \
                      a zero or brace initialiser is appended after the walk in definition order, read from \
                      the TU's source (its compile spec's file in the image's project); a TU with no source is \
                      REFUSED, never judged as if every variable were walked, and the run exits 1. Each \
                      object's data sections are seated in the reference from its seated code's DIR32 sites \
                      (and the data names every other object's sites give), so each variable gets the rva the \
                      reference puts it at. Per TU: is our own order bucket order (the model), is the \
                      reference's (under our names), and for each variable off the longest ordered run the \
                      bucket interval its original name must hash into; same-bucket pairs in the other order \
                      (a declaration order to swap) and each `$S` guard's fitting numbers (a measure of what \
                      the original TU declared ahead of it). --plan and --assign choose renames for one TU, \
                      --merge-screen judges adjacent TUs as one, --names and --suggest need no image."
    )]
    BssOrder(Box<crate::model::BssOrderArgs>),
    #[command(
        about = "The /Od and SEH frame-order oracle: which locals a frame's slots say were misnamed",
        long_about = "In a body that homes every local (/Od, or /O2 with __try) compiled without /ZI, CL lays \
                      the named locals out by name: bucket `ident_key(name) & 0xf` walked 0..15 from the frame \
                      top, later-declared first in a bucket, 4-byte slots, nested blocks after their parent, \
                      `this` last; under /ZI (Edit and Continue) in declaration order, `this` first. Reads each \
                      function's S_BPREL32 and S_BLOCK32 records out of the objects' .debug$S (CV4 and C11 \
                      numbers), /ZI off the S_COMPILE command line, checks CL's own layout against the model its \
                      flags select (or --model's), and lists every HASH-domain local whose slot its name \
                      contradicts, with the bucket interval its name must hash into."
    )]
    FrameOrder(Box<crate::model::FrameOrderArgs>),
    #[command(
        about = "Each object's .text COMDAT order against the reference's addresses, with no link; predicted from C1XX's IL",
        long_about = "Seat every object of a build directory as `try --auto-members` seats it (a SELECT_ANY copy \
                      an earlier TU of the project's .dsp defines, its copy fitting the address, is a DUPLICATE \
                      and leaves the object, as LINK drops it) and read its own members in object order, each \
                      USER (a NODUPLICATES COMDAT: a .cpp definition), INL (a SELECT_ANY header body), GEN \
                      (`??_G`, `??_E`, `??_D`) or THUNK (`W`, `$4`). Members off one longest increasing run of \
                      reference rvas are ORDER bodies, inside the object's span (the run's, grown over members \
                      within 0x100); outside it AWAY. A later SELECT_ANY copy of an earlier TU in link order \
                      (the image's .dsp, then the workspace's other projects) leaves; a name at an rva two or \
                      more members hold is one ICF-folded body, counted once in the object whose span holds it \
                      at the copy that lies in order there. Every object is also scored under the end-drain \
                      model (every deferred body after the last USER one). On an incremental reference (an ILT) \
                      an ORDER body is an ILINK FOSSIL, not owed, when a foreign body inside the span sits beside \
                      it, when it is `??_GX` at the end of its own `??1X`'s ILINK slot, when the archive pull \
                      order (--library) pins it ahead of a run body its address follows, or when it is a \
                      compiler-generated or toolchain-header body that drains after such a pinned body. \
                      --predict reads each TU's IL (see its help)."
    )]
    TuOrder(Box<crate::model::TuOrderArgs>),
    #[command(
        about = "Price a header edit over every includer, sweep C1's label counter, and search edit combinations",
        long_about = "C1 numbers a translation unit's entities and labels with one shared counter, and some C2 ties \
                      key on that counter at a function's start, mod 64 exactly. So a declaration respelled in a \
                      shared header moves bytes in includers that never call it. `hdr price' compiles every includer \
                      as it stands and with the edit and reports what moved; `hdr sweep' measures each function's \
                      whole curve under a pure renumbering; `hdr safe' keeps those curves as safe residue sets and \
                      prices any sum of candidate edits by arithmetic; `hdr combo' searches combinations of named \
                      patch hunks for one that moves bytes with no riser. Exit codes: 0 ok, 1 a compile failed."
    )]
    Hdr(Box<crate::hdr::HdrArgs>),
    #[command(
        about = "Every `.rdata' float an image's code reads, classified by the shape of its uses",
        long_about = "A named float is a memory variable to C2 and a literal is a constant, so their uses differ in \
                      shape: at /O2 a literal is the SECOND operand of a commutative op straight after a variable \
                      load, while a named const is loaded FIRST, divided BY (a literal divisor becomes a multiply by \
                      its reciprocal), stored through the FPU (a literal store is an immediate move), moved into a \
                      register, pushed from memory, or negated by `fchs'. A datum with both kinds of evidence is a \
                      CONFLICT, since a literal and a named const never share an address. At /Od C2 keeps source \
                      operand order and folds no division, so only the move, store and negate rows discriminate \
                      there, and a /GZ body inside an /O2 image is read as /Od code. Literals merge image-wide, so \
                      of several datums holding one value at most one is a literal and the rest are named consts, \
                      whatever their shapes say: the COPIES section lists them. With --objs each use is joined to \
                      how our own object spells it, aligned by mnemonic, and a disagreement is named. Exit 0."
    )]
    Floats(Box<crate::hdr::FloatsArgs>),
    #[command(
        about = "Does a candidate translation unit span a per-TU switch transition? Refuted before a compile",
        long_about = "A translation unit is compiled with ONE flag set, so a candidate grouping whose members do not \
                      agree about a per-TU switch cannot be one object, whatever its byte score says -- and that is a \
                      lookup rather than a build. Exception-handling data in the image is attested /GX; its ABSENCE \
                      proves nothing, because /GX changes a function's bytes only where unwinding is needed, so only \
                      a row whose EFFECTIVE flags lack the switch counts against a grouping, and a row with no carve \
                      at all counts neither way. Parse noise therefore cannot manufacture a refutation: an rva that \
                      resolves to no row is unknown, and unknown never refutes. Exit 0."
    )]
    TuFlags(Box<crate::hdr::TuFlagsArgs>),
    #[command(
        about = "How many translation units of our own the original project had, as a band per language bucket",
        long_about = "The Rich header counts every object of a link, library members included, so it is not a count \
                      of files anyone had to write. own = reference - library, and the library term is bounded from \
                      two directions that do not meet: from below by the members a library table places in the image \
                      (it cannot see a folded member, one too short to match uniquely, or one with no `.text'), and \
                      from above by how many members of that bucket the archive holds at all, which needs no \
                      assumption about which the link chose. So `own' is a BAND, and a screen printing one number \
                      here would be concealing which bound it used. Our side is two figures against two different \
                      targets: the source files against the band, and our link's own Rich census against the \
                      reference's. --control prints a second image whose own counts are known independently, and \
                      reports where that figure falls without passing or failing. Exit 0."
    )]
    TuBudget(Box<crate::hdr::TuBudgetArgs>),
    #[command(
        about = "The front end's IL files: dump them, list C1XX's ids and definitions, read C2's IL grammar",
        long_about = "Read the intermediate files a front end (C1XX.DLL for C++, C1.DLL for C) hands C2: gl (the \
                      header and the file-scope symbol records), sy (each function's symbol list), ex (the \
                      functions' postfix expression records), in (initialised data) and, for a pch-create \
                      compile, the pch that holds its header bodies. CL keeps them with /Bk<base> (`il keep` \
                      compiles a TU so). The ex records are decoded with the operand grammar read out of the \
                      user's own C2.DLL at run time (`il tables`); no Microsoft table is part of the toolkit. \
                      Every read is bounded by the bytes that hold it, and a record C2 would refuse is an error \
                      naming its stream and offset."
    )]
    Il(Box<crate::il::IlArgs>),
    #[command(
        about = "The static inline plan: which call sites C2's inliner expands under its budget",
        long_about = "Replay C2's inline site walk from the IL alone: each compiled function's budget from its \
                      size estimate, each inline candidate's call site in the order its calls complete, taken first-fit against the \
                      budget, the running total and the exception gate, and the expansions' own sites with \
                      their share of it (`inline plan`), and hold the plan to the calls CL's listing keeps \
                      (`inline check`, which compiles the TUs)."
    )]
    Inline(Box<crate::il::InlineArgs>),
    #[command(
        about = "Source tools on the bytes of a project's C and C++: lex, strip, prove, webs, rename-check",
        long_about = "Source tools that read a project's sources as Visual C++ 6.0 does, on their bytes \
                      (code-page bytes 0x80-0xFF pass through unchanged, trigraphs and line splices are read as \
                      CL reads them, `#if 0` text is dead): the lexer, the comment strip, line positions, the \
                      byte-neutral compile proof, def-use webs, the rename proof, definition reorder, mirror \
                      locals, number spelling, the placeholder census and the project guard. Each reads the \
                      [source] table of the project configuration. Exit codes: 0 ok, 1 error, 2 usage, 3 the \
                      check measured a difference (a comment left, an object that moves, a rewrite not proven)."
    )]
    Src(Box<crate::source::SrcArgs>),
    #[command(
        about = "A CodeView record (NB09 / NB11) as C++ declarations, and against a project's own headers",
        long_about = "Read the CodeView record an image carries inside it (NB09 or NB11, as LINK writes a /debug \
                      image linked /pdb:none: found through the debug directory, else the file's trailing NBxx \
                      signature). IMAGE is an [[image]] id whose [image.codeview] names the record image (its \
                      `record`, sha256-checked when `sha256` is given) and the views of the project's sources, or \
                      a path. Every walk over the record is bounded by the bytes that hold it; a malformed record \
                      is an error, never a guess. Every row verb takes --json FILE."
    )]
    Cv(CvArgs),
    #[command(
        about = "PDB 2.0 and 7.0: identity, structure check, 2.0 to 7.0, equivalence, LLVM, attach, procedures, source paths, release",
        long_about = "Read, check and convert the program databases a VC6 link writes (PDB 2.0, the small MSF) and \
                      their 7.0 twins (the big MSF modern readers take). Every walk over a container, stream or \
                      record is bounded by the bytes that hold it: a malformed file is an error naming where, never a \
                      panic or an allocation the file cannot back. An IMAGE argument is an [[image]] id; its \
                      [image.pdb] table names the image's PDB (`file`), its 7.0 twin (`twin`), the file name a debug \
                      record gives (`name`), the md5s of the shipped files attach accepts (`shipped`) and the \
                      source-name policy (`paths`)."
    )]
    Pdb(Box<crate::pdb::PdbArgs>),
    #[command(
        about = "Models of what LINK 6.00 decides with its host CRT's qsort: ICF survivors, IAT order; the ILT and its reserves",
        long_about = "What LINK 6.00 decides once the objects exist, read from its own output and replayed from the \
                      objects: identical-COMDAT folding and which copy survives (icf, icf-predict, icf-diff), the \
                      import address table's order (iat, include-order), the incremental link's thunk table \
                      (ilt-order, ilt-diff) and the growth reserve after each import block (reserve). LINK sorts \
                      tied keys with its host CRT's qsort, so each model replays VC6's QSORT.C (the native msvcrt \
                      LINK must run on) and can replay wine's builtin one for a contrast. The inputs are LINK's \
                      /VERBOSE narration (`msvc6t build --link-env /VERBOSE` writes one), its /MAP, the objects and \
                      the images. Exit codes: 0 the model reproduces the link or the proof holds, 1 error, 2 usage, \
                      3 it does not."
    )]
    Link(Box<crate::link::LinkArgs>),
    #[command(
        about = "Class screens: candidate vftables, declared classes joined to them slot by slot, layouts, receivers",
        long_about = "What an image's vftables and a project's class declarations say about each other. The \
                      reference side is read off the image: candidate vftables in `.rdata' (a slot followed \
                      through its ILT thunk to a body), the starts decoded vptr installs write, a slot's body \
                      decoded (arity, forward target, constant), the `this' offsets a body touches. The source \
                      side is read off the declarations under the image's [image.classes] `sources': each \
                      class's vftable slot list under MSVC6's rules (an overload group at its name's first \
                      declaration, its members in reverse), the slot names joined to the table whose bodies the \
                      image's names place, and the copies of one class compared for their signatures and their \
                      layouts. No screen rules on a disagreement: each is a source question. Exit codes: 0 ok, 1 \
                      error, 2 usage, 3 a control the configuration records is not reproduced."
    )]
    Class(Box<crate::class::ClassArgs>),
    #[command(
        about = "Exception-handling screens: our /GX unwind funclets, the reference's funclet groups, a body's destructor ladder",
        long_about = "What /GX leaves in the bytes that no body comparison reads: the `.text$x' unwind funclets \
                      our objects emit (census), the funclet groups of the reference and of one of our links \
                      aligned function by function (funclets), and the destructor calls at each scope exit of one \
                      function, ours against the image (ladder). Exit codes: 0 ok, 1 error or an object that \
                      cannot be read, 2 usage."
    )]
    Eh(Box<crate::eh::EhArgs>),
    #[command(
        about = "Pair our functions with a sibling build's (another program built from code we share) and read its names, files and objects off the pairs",
        long_about = "A sibling build links code we share (an engine library, an earlier or later release, a debug \
                      build) and carries what ours lost: strings that name functions, `__FILE__` paths, packed \
                      object runs. Its PE is only read, never run. `pair` pairs our functions with its functions \
                      by anchors, call order and the rounds after them, each gated by the two calling \
                      conventions; `functions` lists a PE's functions found without symbols; `diff` compares \
                      two runs' tables."
    )]
    Sibling(Box<crate::sibling::SiblingArgs>),
    #[command(
        about = "Mac builds of shared code: PEF and Mach-O readers, PowerPC traceback names, CodeWarrior and Itanium signatures, the name pairing",
        long_about = "Mac ports of a program often keep the names a PC build lost: CodeWarrior and MrC leave a \
                      traceback table with the mangled name after each PowerPC function of a PEF container, and an \
                      unstripped Mach-O keeps its symbol table. These verbs read those containers (read only, never \
                      run; every count bounded by the bytes that hold it), scan the traceback tables, read the \
                      names as signatures, and pair the named functions with ours across the change of compiler \
                      and instruction set."
    )]
    Mac(Box<crate::mac::MacArgs>),
    #[command(
        about = "Vendored libraries: build them from their own source, locate their objects in the image",
        long_about = "A library that ships with its source (an image's [[image.vendored]]: a directory, its \
                      sources, the flags it was built with) is compiled, not reconstructed, and its objects' code \
                      located in the image by its bytes."
    )]
    Vendored(Box<crate::vendored::VendoredArgs>),
    #[command(
        about = "Rank the image's functions by the undefined callees they call: which bodies can link now",
        long_about = "A body links only when every function it calls is defined: LINK resolves every reference \
                      before /OPT:REF discards anything. For every function of IMAGE's function index with an \
                      extent that the project does not supply (--objs, --defined), decode its call rel32 sites in \
                      the image, follow each target through one ILT jmp, and count the callees that are functions \
                      of the index the project does not supply (blockers), the targets outside the index (foreign: \
                      the CRT, an import, a library) and the calls through a register and displacement (indirect, a \
                      virtual call this cannot follow). Rows sort by blockers, then size; a histogram of blocker \
                      counts closes the report. A zero says no DIRECT call blocks the body: a class it instantiates \
                      still needs its virtuals. JSON rows: one per function."
    )]
    Closure(Box<crate::vendored::ClosureArgs>),
    #[command(
        about = "List the debug information every image, PDB and DBG file under some paths carries",
        long_about = "Walk each PATH (a file, or a directory recursively, entries in sorted order, symlinked \
                      directories skipped and listed) and report every PE image's debug directory and CodeView \
                      record: an embedded NB09 / NB11 record (its size, subsections and modules) or a reference \
                      to a PDB (NB10 signature and age, RSDS guid and age, the path). PDB 2.00, MSF 7.00 and DBG \
                      files are listed by their magic. Each file and directory is read once: one reached again \
                      (named twice, under another spelling, or through a hard link: the same device and inode) \
                      is listed as a duplicate of the first spelling. A debug build of the code under study is a \
                      naming, type and layout oracle, so this is the first inventory to take. With --all, images \
                      that carry no CodeView record are listed too. JSON rows: one per file listed."
    )]
    Debuginfo(DebuginfoArgs),
    #[command(
        about = "Print the help of msvc6t or of a verb, or every verb as markdown",
        long_about = "Print the long help of msvc6t, or of the verb named. With --markdown, print the help of \
                      every verb as one markdown document, to stdout or with --out to FILE."
    )]
    Help(HelpArgs),
    #[command(
        about = "Cut a source release: the files a spec names, the one root commit of a branch of their own",
        long_about = "Read the release spec (default release/publish.toml) from REV's tree, never the work tree, \
                      and write the tree it names into the repository: every file under an `include` path (a \
                      file or a directory) that no `exclude` pattern matches (a pattern holding a `/` matches the \
                      whole path, one without matches the file name; `*` stays within one component, `**` \
                      crosses them, letters match in any case), and each `[overlay]` target with the file it \
                      names. Every file keeps its blob and mode, so one commit and one spec always give the same \
                      tree. An include that names nothing, an exclude that removes nothing, an overlay that is \
                      missing, unsafe or already included, a submodule, a symlink or any mode but 100644 and \
                      100755, a path that is not ASCII, or a path that is both a file and a directory is an \
                      error. A `[screen]` table then reads every file before the tree is written: each `refuse` \
                      text is refused in every published path and overlay name, and in every file outside the \
                      files `[screen.allow]` lists for it. A text with no `/`, `\\`, `:` or `~` is a word and is \
                      refused wherever it stands, in any case; a path form is refused where a name character \
                      follows it (a home path, a notes or docs reference; a run of backslashes is one separator \
                      and a drive path matches in any case). With `prose = \"overlays\"`, prose that is not an \
                      overlay (a .md, .markdown, .rst or .txt file, or a file name with no extension) is refused \
                      outside the `prose_allow` patterns; with `comments`, a C or C++ file holding a comment (the \
                      VC6 lexer's reading) is refused outside those patterns. With `[screen.readers]`, each \
                      pattern names what reads the files it matches (a compiler, a build, a test, or the licence \
                      a text is kept for), and every published path, an overlay's too, that no pattern matches is \
                      refused. Every `[screen]` pattern (`allow`, `prose_allow`, `comments`, `readers`) matches \
                      the whole path, case-sensitively, so one without a `/` names a file at the root. An allowance or a reader \
                      rule that matches nothing it excuses is an error too. The \
                      commit's message is the spec's `message`, then, with `credits`, a blank line and the \
                      `Credits` section of that file at REV verbatim, published or not; with \
                      `author` (`Name <email>`), its author and committer are that identity, dated at the cut in \
                      +0000. With --branch, the tree is committed as the branch's one root commit: an old tip is \
                      replaced, compared and swapped, and kept when it already is that commit; a branch that \
                      shares history with REV, or is checked out in a worktree, is refused. --check writes \
                      nothing to the branch and refuses a tip with a parent, another identity or time zone, \
                      another message, or another tree. Nothing is pushed. Exit codes: 0 ok, 1 error, 2 usage."
    )]
    Publish(crate::publish::PublishArgs),
}

#[derive(Debug, Args)]
pub struct TcArgs {
    #[command(subcommand)]
    pub cmd: TcCmd,
}

#[derive(Debug, Subcommand)]
pub enum TcCmd {
    #[command(
        about = "Name each toolchain's tools, their builds and the runner",
        long_about = "For each [toolchain.<id>]: its directories, and each tool's file version read from its \
                      version resource, the build that number names (8168 no service pack, 8447 SP3, 8799 SP4, \
                      8966 SP5, 9044 the SP5 processor pack, 9782 SP6) and its md5, checked against the \
                      toolchain's `expect` table (roles cl, c1, c1xx, c2, link, lib, mspdb, msdev). Then the \
                      runner: its kind, prefix pool and template, the native msvcrt and its md5, the job cap and \
                      the memory there is to run under. Exits 1 when a tool an `expect` names is missing or is \
                      another build."
    )]
    Info(TcInfoArgs),
    #[command(
        about = "Run one tool of a toolchain in a leased prefix",
        long_about = "Run TOOL (CL.EXE, LINK.EXE, LIB.EXE, ... from the toolchain's VC98/BIN or \
                      COMMON/MSDEV98/BIN, found under either casing) with ARGS in --cwd, in a private prefix \
                      leased from the pool and holding one job token, with WINEPATH, LIB and INCLUDE set from the \
                      toolchain and the native msvcrt installed. Prints the tool's output and exits with its \
                      status."
    )]
    Run(TcRunArgs),
    #[command(
        about = "Kill the jobs left in pool prefixes whose holder died",
        long_about = "Sweep the prefix pool: in every slot no process holds, a tool still running under that \
                      WINEPREFIX lost its owner (killing the runner's client does not reach into a container) and \
                      is killed by PID, and a dead holder's mark is cleared. wine's own services are never jobs. \
                      Each process that leases a slot starts one detached watcher of this verb, which sweeps the \
                      slots its owner marked once the owner exits however it exits."
    )]
    Reap(TcReapArgs),
    #[command(
        about = "Build an import library from a .def or a DLL's exports",
        long_about = "Run the toolchain's LIB.EXE /DEF on DEF (a .def taken verbatim, or a DLL whose export \
                      table becomes one), then fix in place what LIB cannot write: with --verbatim-symbols each \
                      long-format member's two externals take the .def entry verbatim (the member's string table \
                      and both linker members rewritten, no size moving, read back through the archive symbol \
                      table); with --import-name each short record is NAME TYPE 1; with --hints-from each record's \
                      hint is the one IMAGE binds for that name. Every member is read back and the symbols listed."
    )]
    Implib(TcImplibArgs),
    #[command(
        about = "Map CL diagnostics through a source's #line directives",
        long_about = "Read compiler output on standard input and print it unchanged, adding below each \
                      diagnostic whose file name matches a SOURCE the line of that file it reports: `#line N` \
                      makes the next line report as N. A number no region covers, or two regions claim, says so. \
                      The counts of diagnostics seen, annotated and matching no SOURCE go to standard error."
    )]
    Clmap(TcClmapArgs),
}

#[derive(Debug, Args)]
pub struct TcInfoArgs {
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write the rows as JSON to FILE."
    )]
    pub json: Option<PathBuf>,
    #[arg(
        value_name = "TOOLCHAIN",
        help = "The toolchains to show. Default: every one."
    )]
    pub toolchains: Vec<String>,
}

#[derive(Debug, Args)]
pub struct TcRunArgs {
    #[arg(
        long,
        value_name = "ID",
        help = "The toolchain whose tool runs. Default: the first in the config."
    )]
    pub toolchain: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        help = "The working directory. Default: the current one."
    )]
    pub cwd: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DURATION",
        help = "Kill the tool after DURATION (\"90\", \"30 min\")."
    )]
    pub timeout: Option<String>,
    #[arg(value_name = "TOOL", help = "The tool, e.g. CL.EXE.")]
    pub tool: String,
    #[arg(
        value_name = "ARGS",
        trailing_var_arg = true,
        allow_hyphen_values = true,
        help = "The tool's arguments, verbatim."
    )]
    pub args: Vec<String>,
}

#[derive(Debug, Args)]
pub struct TcReapArgs {
    #[arg(
        long,
        value_name = "DIR",
        help = "The prefix pool. Default: [runner] prefixes."
    )]
    pub pool: Option<PathBuf>,
    #[arg(long, help = "Report what would be killed and cleared; touch nothing.")]
    pub dry_run: bool,
    #[arg(long, value_name = "PID", hide = true, requires = "owner_start")]
    pub watch_owner: Option<u32>,
    #[arg(long, value_name = "TICK", hide = true)]
    pub owner_start: Option<u64>,
}

#[derive(Debug, Args)]
pub struct TcImplibArgs {
    #[arg(
        long,
        value_name = "ID",
        help = "The toolchain whose LIB.EXE runs. Default: the first."
    )]
    pub toolchain: Option<String>,
    #[arg(long, value_name = "FILE", help = "The archive to write.")]
    pub out: PathBuf,
    #[arg(
        long,
        help = "Run LIB with /LINK50COMPAT: one long-format COFF member per export."
    )]
    pub link50compat: bool,
    #[arg(
        long,
        requires = "link50compat",
        help = "Rename each long member's `_E` and `__imp__E` to the .def entry verbatim."
    )]
    pub verbatim_symbols: bool,
    #[arg(
        long,
        help = "Make every short import record NAME TYPE 1 (the symbol verbatim)."
    )]
    pub import_name: bool,
    #[arg(
        long,
        value_name = "IMAGE",
        help = "Set each record's hint to the one this PE image's import table binds for its name."
    )]
    pub hints_from: Option<PathBuf>,
    #[arg(
        value_name = "DEF",
        help = "A .def file, or a DLL whose exports make one."
    )]
    pub def: PathBuf,
}

#[derive(Debug, Args)]
pub struct TcClmapArgs {
    #[arg(
        value_name = "SOURCE",
        required = true,
        help = "The sources whose #line directives map the diagnostics."
    )]
    pub sources: Vec<PathBuf>,
}

#[derive(Debug, Args)]
pub struct BuildArgs {
    #[arg(
        long,
        help = "Build every image that has an [image.build].",
        conflicts_with = "images"
    )]
    pub all: bool,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where each image's output, logs and object directory go: DIR/<id>. Default: ./msvc6t-out."
    )]
    pub out: Option<PathBuf>,
    #[arg(long, help = "Leave the staged files in the prefix for a post-mortem.")]
    pub keep_staging: bool,
    #[arg(
        long = "link-env",
        value_name = "SWITCHES",
        help = "Set the LINK environment variable for the MSDEV steps: LINK prepends it to every command line \
                and MSDEV's /USEENV hands it down, so `--link-env /VERBOSE` writes LINK's narration into each \
                step's log (msvc6t link reads it) and `/MAP` a map beside the output, the project unchanged."
    )]
    pub link_env: Option<String>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Build up to N images at once, each in its own prefix, fewer when memory is short. Default: \
                as many as the images, capped by [runner] jobs."
    )]
    pub jobs: Option<usize>,
    #[arg(
        long,
        value_name = "DURATION",
        help = "Kill the build after DURATION. Default: 1 hour."
    )]
    pub timeout: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write the rows as JSON to FILE."
    )]
    pub json: Option<PathBuf>,
    #[arg(value_name = "IMAGE", help = "The [[image]] ids to build.")]
    pub images: Vec<String>,
}

#[derive(Debug, Args)]
pub struct CrtguardArgs {
    #[arg(
        long,
        value_name = "TOOL",
        help = "A tool that must be seen loading the native msvcrt. Default: LINK.EXE."
    )]
    pub require: Vec<String>,
    #[arg(value_name = "LOG", required = true, help = "The +loaddll logs.")]
    pub logs: Vec<PathBuf>,
}

#[derive(Debug, Args)]
pub struct ProbeArgs {
    #[arg(
        value_name = "PROBE",
        help = "Probe directories, or probe files (a source or its .expect). Default: the toolkit's probes/."
    )]
    pub probes: Vec<PathBuf>,
    #[arg(
        long = "toolchain",
        value_name = "ID",
        help = "A [toolchain.<id>] to run on (repeat for more). Default: every toolchain the config names."
    )]
    pub toolchains: Vec<String>,
    #[arg(
        long,
        value_name = "ID",
        help = "A toolchain whose FAIL, XPASS or ERROR fails the run (repeat for more); the others are \
                recorded. Default: every toolchain run."
    )]
    pub gate: Vec<String>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where the runs go, one directory per toolchain and probe. Default: msvc6t-probe in the \
                temporary directory. A directory that holds files must carry the MSVC6T-PROBE-RUN marker."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the results TSV to FILE. Default: results.tsv in --out."
    )]
    pub results: Option<PathBuf>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Probe runs at once, each in its own prefix. Default: [runner] jobs, lowered to what memory \
                allows."
    )]
    pub jobs: Option<usize>,
    #[arg(
        long,
        help = "Only read every probe: parse each .expect and refuse a source with a comment, a stem with \
                two sources, a source or .expect alone. Runs no compiler and needs no config."
    )]
    pub check: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write the rows as JSON to FILE."
    )]
    pub json: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct CacheArgs {
    #[command(subcommand)]
    pub cmd: CacheCmd,
}

#[derive(Debug, Subcommand)]
pub enum CacheCmd {
    #[command(
        about = "Entries, bytes and ages in the compile cache",
        long_about = "Count the cache's entries and bytes, with the age of their last use in buckets, and the \
                      direct-mode manifests."
    )]
    Stats(CacheStatsArgs),
    #[command(
        about = "Evict least-recently-used entries down to the cap",
        long_about = "Evict the least recently used entries until the cache is at nine tenths of the cap \
                      (--max, else [cache] max), and with --older-than every entry unused that long. An entry \
                      leaves by one rename into the trash, deleted after a grace minute, so a restore in flight \
                      completes or misses cleanly. Manifests naming an absent entry are pruned. A store runs this \
                      by itself every 256 stores when [cache] max is set."
    )]
    Gc(CacheGcArgs),
    #[command(
        about = "Re-hash every entry against its record",
        long_about = "Re-hash every part of every entry against the sha256 its OK record holds, and list the \
                      entries that do not match. Exits 1 when one does."
    )]
    Verify(CacheVerifyArgs),
}

#[derive(Debug, Args)]
pub struct CacheStatsArgs {}

#[derive(Debug, Args)]
pub struct CacheGcArgs {
    #[arg(
        long,
        value_name = "SIZE",
        help = "The cap, e.g. \"20 GiB\". Default: [cache] max."
    )]
    pub max: Option<String>,
    #[arg(
        long,
        value_name = "DURATION",
        help = "Also evict every entry unused this long, e.g. \"48 h\"."
    )]
    pub older_than: Option<String>,
    #[arg(long, help = "Report what would be evicted; touch nothing.")]
    pub dry_run: bool,
}

#[derive(Debug, Args)]
pub struct CacheVerifyArgs {}

#[derive(Debug, Args)]
pub struct LintArgs {
    #[arg(
        long,
        conflicts_with_all = ["fix", "provenance"],
        help = "Answer from the marker when this tree's content key is already marked clean by this toolchain; \
                else run the full lint and mark it. Markers are kept in target/msvc6t-lint/markers \
                ($MSVC6T_LINT_MARKERS); the lint builds in target/msvc6t-lint/build/<path hash> \
                ($MSVC6T_LINT_TARGET). A failing lint, or a tree edited while it ran, marks nothing."
    )]
    pub gate: bool,
    #[arg(
        long,
        conflicts_with = "provenance",
        help = "Apply rustfmt and every machine-applicable clippy suggestion. Marks nothing."
    )]
    pub fix: bool,
    #[arg(
        long,
        help = "Check that this binary was built from a tree marked clean, the check every verb that runs a \
                compiler makes first. When this checkout is that tree and is not yet marked, lint it now."
    )]
    pub provenance: bool,
    #[arg(
        long,
        value_name = "DIR",
        help = "The toolkit tree to check. Default: the one holding the current directory, else the one this \
                binary was built from."
    )]
    pub root: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct HelpArgs {
    #[arg(long, help = "Print the help of every verb as one markdown document.")]
    pub markdown: bool,
    #[arg(
        long,
        value_name = "FILE",
        requires = "markdown",
        help = "Write the markdown to FILE instead of standard output."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        value_name = "VERB",
        conflicts_with = "markdown",
        help = "The verb, then any sub-verb, whose help to print."
    )]
    pub verb: Vec<String>,
}

#[derive(Debug, Args)]
pub struct JsonArg {
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write the rows as JSON to FILE: {format, command, image, summary?, rows}. The text is \
                unchanged."
    )]
    pub json: Option<PathBuf>,
}

#[derive(Debug, Args)]
pub struct ImageArgs {
    #[command(subcommand)]
    pub verb: ImageVerb,
}

#[derive(Debug, Subcommand)]
pub enum ImageVerb {
    #[command(
        about = "The headers, data directories, sections, Rich summary and CodeView record",
        long_about = "Print the COFF and optional headers, every data directory with the section it lands in, \
                      the section table, the Rich header's stamp count and whether its key is the checksum over \
                      the DOS header and stamps, the CodeView record, and any overlay past the last section. \
                      JSON: the header fields as the summary, one row per section."
    )]
    Info(ImageInfoArgs),
    #[command(
        about = "The Rich header's object census, optionally against a candidate and archives",
        long_about = "Decode the Rich header: one stamp per (product, build) with the count of objects the \
                      link consumed, and whether the key is the checksum (a key that is not was edited after \
                      the link). --candidate censuses a second image beside it; a missing or undecodable Rich \
                      header there is reported, never read as zero objects. Each --archive is a static library \
                      whose members' @comp.id builds are counted: a build only one archive stamps decides which \
                      copy the image's library members came from, and a build several stamp decides nothing. \
                      --rsp attributes the candidate's census to the objects a LINK response file names, by \
                      each object's @comp.id. JSON rows: prodid, product, build, reference and, with \
                      --candidate, candidate."
    )]
    Rich(ImageRichArgs),
    #[command(
        about = "The section table, or the bytes at one address",
        long_about = "Without --at, print the section table: mapped extent max(virtual, raw), on-disk extent \
                      min(virtual, raw), and the tail that has no file bytes. With --at, say which section the \
                      address is in and whether its bytes exist in the file, then the dword there (followed as a \
                      VA when it lands in a section) and a hex dump that never elides a repeated line. With \
                      --textbss, where the code section's final zero run starts: the upper bound of an \
                      incremental link's .textbss contribution, checked against a /MAP's \
                      __enc$textbss$begin/$end when --map names one."
    )]
    Dump(ImageDumpArgs),
    #[command(
        about = "The import directory: tables, orders and the anatomy of the sections holding them",
        long_about = "Print the import directory: one row per DLL (symbols, ILT, IAT, name), the four orders \
                      (descriptors and DLL names per library; ILT and IAT per function), and the anatomy of each \
                      section the tables live in: every ILT and IAT block, DLL name, hint/name run, the \
                      descriptors and the export directory, with each gap classed as linker padding (zero, under \
                      16 bytes), a zero-filled reserve, or content LINK did not generate. Each --libdir is \
                      searched for <dll>.lib, and its member order is compared with the image's per-function \
                      order. JSON rows: one per DLL, or with --entries one per imported symbol."
    )]
    Imports(ImageImportsArgs),
    #[command(
        about = "The export directory",
        long_about = "Print the export directory: name, ordinal base, every exported function with its \
                      ordinal, target and forwarder, and its TimeDateStamp against the PE header's (LINK writes \
                      the export stamp on the link that builds the table and leaves it on an incremental \
                      relink, so an older stamp shows a relink). JSON rows: one per export."
    )]
    Exports(ImageExportsArgs),
    #[command(
        about = "A candidate against the reference: md5, differing bytes, per-section cost, clock fields",
        long_about = "Compare a candidate image with the reference: size and md5 of each, the whole-file count \
                      of differing bytes (a length difference counts), and the cost of each reference section \
                      at the same section-relative offset (a displaced section reads as mostly differing; this is \
                      a cost report). The declared clock fields are read on both sides. --against reads the \
                      result as a delta from a baseline candidate; a delta within 12 bytes is inside the band \
                      three wall-clock fields move on every relink. Exit 0 when the md5s are equal, 3 when they \
                      differ. JSON: the file figures as the summary, one row per section."
    )]
    Cmp(ImageCmpArgs),
    #[command(
        about = "Copy the declared post-link bytes and clock fields from the reference into an image",
        long_about = "Resolve the image's declared clock fields and residue rows ([[image.residue]] of the \
                      config, or --rows) against the reference and the image, and with --apply write them. A \
                      clock field is named, never an offset, and is resolved by walking each file's own headers. \
                      A residue row's kind is header-slack, text-pad (both positional), header-field or \
                      import-stamps (anchored: `at` names size-of-code, size-of-image, checksum, iat-directory, \
                      import-stamps or section-tail). A positional row is written only where the image holds the \
                      fill (0x00 in header slack, 0x90 in a code pad, or `fill`) or already the reference's \
                      bytes, and a code pad also needs the 8 bytes before it to be the reference's; anything \
                      else is BLOCKED and left alone. An anchored row must be exactly its field in the reference. \
                      The values are read from the reference at run time. With --apply the step repeats to a \
                      fixed point and fails unless every non-blocked row then matches. JSON rows: one per \
                      residue row."
    )]
    Postlink(ImagePostlinkArgs),
    #[command(
        about = "The resources, and a .res rebuilt from them",
        long_about = "List every resource leaf: type, name, language, codepage, size and rva. --out writes a \
                      32-bit .res file of them (sorted by type, name and language) for CVTRES to link back in; \
                      a .res header's memory flags and versions are not stored in a PE and are written as \
                      defaults. JSON rows: one per resource."
    )]
    Res(ImageResArgs),
}

#[derive(Debug, Args)]
pub struct ImageInfoArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a path."
    )]
    pub image: String,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImageRichArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a path."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "FILE",
        help = "A second image to census beside IMAGE, typically one built from the reconstruction."
    )]
    pub candidate: Option<PathBuf>,
    #[arg(
        long,
        value_name = "LIB",
        help = "A static library whose members' builds are counted against IMAGE's stamps (repeatable)."
    )]
    pub archive: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        requires = "candidate",
        help = "A LINK response file: each .obj it names is read and counted under its @comp.id, beside the \
                candidate's own census."
    )]
    pub rsp: Option<PathBuf>,
    #[arg(
        long,
        value_name = "PRODUCT",
        requires = "rsp",
        help = "List every object of this --rsp bucket (a product name such as Utc12_CPP); others show four."
    )]
    pub bucket: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImageDumpArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a path."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "RVA",
        help = "The address to read: hex with 0x, else decimal. An rva unless --va."
    )]
    pub at: Option<String>,
    #[arg(long, requires = "at", help = "Read --at as a virtual address.")]
    pub va: bool,
    #[arg(long, default_value_t = 64, value_name = "N", help = "Bytes to dump.")]
    pub len: usize,
    #[arg(
        long,
        conflicts_with = "at",
        help = "Where the code section's final zero run starts."
    )]
    pub textbss: bool,
    #[arg(
        long,
        value_name = "MAP",
        requires = "textbss",
        help = "A LINK /MAP of this image, whose __enc$textbss$begin/$end are the exact contribution."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        default_value = ".text",
        value_name = "NAME",
        help = "The code section --textbss reads."
    )]
    pub section: String,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImageImportsArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a path."
    )]
    pub image: String,
    #[arg(
        long,
        help = "List every imported symbol, and write one JSON row per symbol."
    )]
    pub entries: bool,
    #[arg(
        long,
        value_name = "DIR",
        help = "A directory of import libraries, searched case-blind for <dll>.lib (repeatable)."
    )]
    pub libdir: Vec<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImageExportsArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a path."
    )]
    pub image: String,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImageCmpArgs {
    #[arg(
        value_name = "REFERENCE",
        help = "The reference: an [[image]] id of the config, or a path."
    )]
    pub reference: String,
    #[arg(value_name = "CANDIDATE", help = "The image to compare with it.")]
    pub candidate: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "A baseline candidate: report this one as a delta from it."
    )]
    pub against: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FIELD",
        value_delimiter = ',',
        help = "The clock fields to read (coff.timestamp, debug.timestamp, pdb.signature, pdb.age, \
                export.timestamp). Default: the [[image]]'s `clock`."
    )]
    pub clock: Option<Vec<String>>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImagePostlinkArgs {
    #[arg(
        value_name = "REFERENCE",
        help = "The reference: an [[image]] id of the config, or a path."
    )]
    pub reference: String,
    #[arg(value_name = "IMAGE", help = "The linked image to write.")]
    pub image: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "A TOML file of [[residue]] rows to use instead of the [[image]]'s."
    )]
    pub rows: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FIELD",
        value_delimiter = ',',
        help = "The clock fields to copy. Default: the [[image]]'s `clock`."
    )]
    pub clock: Option<Vec<String>>,
    #[arg(long, help = "Write the image. Without it nothing is written.")]
    pub apply: bool,
    #[arg(
        long,
        help = "Print every row, not only the blocked and anchored ones."
    )]
    pub verbose: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ImageResArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a path."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the resources as a .res file."
    )]
    pub out: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct ObjArgs {
    #[command(subcommand)]
    pub verb: ObjVerb,
}

#[derive(Debug, Subcommand)]
pub enum ObjVerb {
    #[command(
        about = "Are two objects, or two directories of them, the same bytes but for the COFF timestamp?",
        long_about = "Compare two COFF objects, or every .obj under directory A (recursively; a symlink is not \
                      followed) with the same relative path under B. The plain comparison is the whole file \
                      with bytes 4..8, the COFF TimeDateStamp, masked; --mask-pdb-signature also masks each \
                      LF_TYPESERVER signature in .debug$T, the second clock a /Zi or /ZI object carries. The \
                      other flags compare canonically: the file pointers, the string table, the .file name and \
                      a non-COMDAT section's checksum aside, everything else line by line, with \
                      --normalize-labels renaming every $L/$T and author label by first appearance, \
                      --mask-line-numbers masking every source line number, and --ignore-debug leaving out \
                      the .debug$ sections. Exit 3 when any pair differs or is missing. JSON rows: one per \
                      pair."
    )]
    Diff(ObjDiffArgs),
}

#[derive(Debug, Args)]
pub struct ObjDiffArgs {
    #[arg(value_name = "A", help = "An object, or a directory of objects.")]
    pub a: PathBuf,
    #[arg(value_name = "B", help = "The object, or directory, to compare with.")]
    pub b: PathBuf,
    #[arg(long, help = "Rename $L/$T and author labels by first appearance.")]
    pub normalize_labels: bool,
    #[arg(long, help = "Mask every COFF source line number.")]
    pub mask_line_numbers: bool,
    #[arg(long, help = "Mask each LF_TYPESERVER signature.")]
    pub mask_pdb_signature: bool,
    #[arg(long, help = "Leave the .debug$ sections out of the comparison.")]
    pub ignore_debug: bool,
    #[arg(
        long,
        default_value_t = 12,
        value_name = "N",
        help = "Differing canonical lines to print per pair."
    )]
    pub show: usize,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LibArgs {
    #[command(subcommand)]
    pub verb: LibVerb,
}

#[derive(Debug, Subcommand)]
pub enum LibVerb {
    #[command(
        about = "Two static libraries compared member by member",
        long_about = "Compare two archives member by member, paired by name (case-blind; a repeated name pairs \
                      by occurrence): the member order, then each member's sections (name, characteristics, \
                      size, md5 of the bytes), relocations (offset, type, target name), COMDATs (section, \
                      selection, symbol) and symbols (name, value, section, type, class; @comp.id included). The \
                      member headers' dates, the COFF timestamps and the .file records are not compared. Exit 3 \
                      when they differ. JSON rows: one per member."
    )]
    Diff(LibDiffArgs),
    #[command(
        about = "Every member of a library, bucketed by the product its @comp.id stamps",
        long_about = "Classify every member the way a Rich header counts it: by the prodid of its @comp.id \
                      (prodid << 16 | build). A member with no @comp.id is unstamped, its own bucket, not \
                      MASM; a short import member is counted under Import. The members with no .text* section \
                      are listed per bucket: a placement by code bytes can never see them. --members lists \
                      every member in archive order with its stamp, sections and defined externals; --only \
                      keeps members whose name or a defined symbol contains the text. JSON rows: one per \
                      bucket, or with --members one per member."
    )]
    Census(LibCensusArgs),
    #[command(
        about = "Where a static library's members sit in an image, and which copy of the library it linked",
        long_about = "Place every member of the archives an image links by its code bytes. Each .text \
                      section of a member is searched for in the image's .text by its longest unrelocated run \
                      (at least 4 bytes) and kept where its whole body matches with the relocated fields masked: \
                      a unique match is placed; several matches are placed by section order into the holes the \
                      member's unique matches leave, then into the gap between its placed neighbours; a body \
                      with no 4-byte run is placed where its relocations land on addresses the other members \
                      attest. One address has one owner: a range the image's relocation witnesses contradict \
                      goes to a member they support; then members keep ranges in order of covered bytes, an \
                      exact tie broken by those witnesses, then by the row the --record table (or the table --write replaces) holds for the span, then \
                      by archive and member name (reported UNDECIDED), and a range that overlaps one already \
                      kept is refused, naming its holders. Archives are the image's `libraries` (each entry's \
                      text is its label), or the --lib files instead. Archives with one file name (case-blind) \
                      are copies of one library, as service packs ship them: each distinct copy is placed on \
                      its own, the copy covering the most bytes is chosen (a tie goes to the first listed and \
                      says so), and the code sections only the chosen copy places are its witnesses. Import \
                      members are joined to the image's import directory: which member supplies each named \
                      import. The census joins each member's @comp.id with the placement and the image's Rich \
                      header. --record compares with a library table (its [[member]] rows) and exits 3 when a \
                      row is not re-derived or a kept range is not recorded; --write writes the table, carrying \
                      forward the rows of an existing one this placement does not re-derive (and, with no --record, reading \
                      its rows to decide an exact tie). JSON rows: one per \
                      member that keeps a range."
    )]
    Locate(LibLocateArgs),
}

#[derive(Debug, Args)]
pub struct LibLocateArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config, or a PE file."
    )]
    pub image: String,
    #[arg(
        long = "lib",
        value_name = "FILE",
        help = "An archive to read instead of the image's `libraries` (repeatable)."
    )]
    pub libs: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A library table to compare with; its [[member]] rows also decide an exact tie the image \
                cannot."
    )]
    pub record: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the library table to FILE (TOML: format, image, image_sha256 and the [[archive]], \
                [[import]], [[member]] and [[byname]] rows), carrying forward the rows of an existing \
                FILE this placement does not re-derive. Without --record, an existing FILE also decides an exact \
                tie, so a rewrite keeps its hand-decided rows."
    )]
    pub write: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A LINK /MAP of our own link: the members that matched nothing are split into those it pulls \
                (candidate misses) and those it does not."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "TEXT",
        help = "List every .text section of the members whose name contains TEXT, with its anchor and \
                verdict, and place nothing."
    )]
    pub explain: Option<String>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Placement threads. Default: min(8, nproc / 2)."
    )]
    pub jobs: Option<usize>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LibDiffArgs {
    #[arg(
        value_name = "OURS",
        help = "The library built from the reconstruction."
    )]
    pub ours: PathBuf,
    #[arg(value_name = "REFERENCE", help = "The library to compare with.")]
    pub reference: PathBuf,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LibCensusArgs {
    #[arg(value_name = "LIB", help = "The archive.")]
    pub archive: PathBuf,
    #[arg(long, help = "Every member in archive order.")]
    pub members: bool,
    #[arg(
        long,
        value_name = "TEXT",
        help = "Keep members whose name or a defined symbol contains TEXT."
    )]
    pub only: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct InitArgs {
    #[arg(
        long = "image",
        value_name = "FILE",
        required = true,
        help = "A shipped image to declare (repeatable): the user's own copy."
    )]
    pub images: Vec<PathBuf>,
    #[arg(
        long = "id",
        value_name = "ID",
        help = "The image's id, one per --image in order. Default: the file stem upper-cased."
    )]
    pub ids: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        conflicts_with = "dsp",
        help = "The image's workspace (.dsw)."
    )]
    pub dsw: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The image's project file (.dsp), when it has no workspace."
    )]
    pub dsp: Option<PathBuf>,
    #[arg(
        long = "project-config",
        value_name = "NAME",
        help = "The configuration the image is built in (`game - Win32 Release`). Default: the project file's CFG= line."
    )]
    pub project_config: Option<String>,
    #[arg(
        long,
        value_name = "VALUE",
        default_value = "${MSVC6T_VC6}",
        help = "The [toolchain.vc6] root, as written into the config."
    )]
    pub vc6: String,
    #[arg(
        long,
        value_name = "FILE",
        default_value = "msvc6t.toml",
        help = "Where to write the config."
    )]
    pub out: PathBuf,
    #[arg(long, help = "Replace an existing file.")]
    pub force: bool,
}

#[derive(Debug, Args)]
pub struct CheckArgs {
    #[arg(
        value_name = "IMAGE",
        help = "Check only these [[image]] ids. Default: every image."
    )]
    pub images: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DspArgs {
    #[command(subcommand)]
    pub verb: DspVerb,
}

#[derive(Debug, Subcommand)]
pub enum DspVerb {
    #[command(
        about = "Print each translation unit's compile spec",
        long_about = "Print each translation unit an image's project builds, in build order, with the switches \
                      MSDEV compiles it with (normalised: split into arguments as the command line is, so \
                      `\\\"` is a quote and `/D \"X\"` is /DX; the \
                      project-wide switches, then the file's own) and their reading: the cflags (every switch but \
                      the /I directories, the output names /Fo /Fd /Fa /Fp /Fr, and the build switches, which steer \
                      the run and not the code: /nologo, /c, /W<n>, /WX, /w, /FD, /Gm), the includes as written and resolved, the \
                      defines, the pch mode and the object path. IMAGE is an [[image]] id whose `project` names a \
                      .dsw or .dsp (its [[image.tu]] and [[image.vendored]] TUs follow); or give --dsw or --dsp \
                      with --project-config and no config. A refusal is printed with its reason and exits 1. JSON \
                      rows: one compile spec per TU."
    )]
    Show(DspShowArgs),
}

#[derive(Debug, Args)]
pub struct DspShowArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: Option<String>,
    #[arg(long, value_name = "FILE", conflicts_with_all = ["image", "dsp"], help = "A workspace to read without a config.")]
    pub dsw: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        conflicts_with = "image",
        help = "A project file to read without a config."
    )]
    pub dsp: Option<PathBuf>,
    #[arg(
        long = "project-config",
        value_name = "NAME",
        help = "With --dsw or --dsp: the configuration to build. Default: the top project's CFG= line."
    )]
    pub project_config: Option<String>,
    #[arg(
        long = "tu",
        value_name = "STEM",
        help = "Show only these translation units, by file stem (repeatable)."
    )]
    pub tus: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct FunctionsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        long,
        value_name = "RVA",
        help = "Show only the function holding this rva (0x-hexadecimal)."
    )]
    pub at: Option<String>,
    #[arg(long, help = "Print the providers' counts, not the rows.")]
    pub summary: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct NamesArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        long,
        value_name = "RVA",
        help = "Show only the names at this rva (0x-hexadecimal)."
    )]
    pub at: Option<String>,
    #[arg(long, value_name = "NAME", help = "Show only the rows with this name.")]
    pub name: Option<String>,
    #[arg(long, help = "Print the providers' counts, not the rows.")]
    pub summary: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CvArgs {
    #[command(subcommand)]
    pub verb: CvVerb,
}

#[derive(Debug, Subcommand)]
pub enum CvVerb {
    #[command(
        about = "The record's classes, structs, unions and enums as C++ declarations",
        long_about = "Print every class, struct, union and enum the record holds for the project's own objects \
                      (modules the link read as objects, not from a library): bases, members in declaration \
                      order with offsets, types and access, methods with their exact signatures (a decorated \
                      public decides `bool` against `unsigned char`, access, virtual, static and const), each \
                      virtual's vtable slot (measured in the image's vftable where it holds the body), \
                      statics, nested types, the parameter names of every body the record compiled and each \
                      body's header:line (inline) or file.cpp:line (out of line), grouped by the header the \
                      record places each class in (the one holding most of its inline bodies). An overload \
                      group prints in declaration order: VC6 writes its method list reversed. Compiler-made \
                      members print as comments. JSON rows: one per member."
    )]
    Decl(CvDeclArgs),
    #[command(
        about = "The record against our declarations of the same classes, one line per mismatch",
        long_about = "Read VIEW's sources ([[image.codeview.view]]: the projects' .dsp files give the defines, \
                      the include directories and the sources; each header is read once where it is first \
                      included, its conditionals evaluated against the defines in force there) and set every \
                      class declared under `headers` beside the record class it is (by member names, then \
                      through field types, bases and nested types, then by name). One line per mismatch: \
                      class-name, class-key, class-missing (an owner view only), class-extra, base, \
                      member-missing, member-extra, type, order, access, static-virtual, param-name, \
                      header-placement, inline-placement, enumerator. A row the explained table names prints \
                      EXPLAINED with its reason; the last line counts the unexplained. Exit 3 when any row is \
                      unexplained. JSON rows: kind, subject, ours, record, our_says, at, explained."
    )]
    Diff(CvDiffArgs),
    #[command(
        about = "Apply one kind of the comparison's mechanical fixes to the view's sources",
        long_about = "Apply the fixes of KIND to VIEW's sources, never in a file another view reads (without \
                      --shared): `param-name`, each record parameter name onto our declaration and definition, \
                      refused where the new name already appears; `access`, each paired member's record access, \
                      the class's labels written again; `order`, each paired method that is virtual in neither \
                      the record nor ours (or an override alone in its name) moved to its record position; \
                      `field-name`, a field the record names otherwise in a class whose fields agree position by \
                      position, renamed at every use (refused unless the old name is declared once and the new \
                      one appears in no file the old one does). Prints each change and each refusal."
    )]
    Apply(CvApplyArgs),
    #[command(
        about = "The names the record attests: members, enumerators, and each method's parameters and locals",
        long_about = "Print `member <class> <name>` for every data member, static member and method of every \
                      class the record defines, `enumerator <enum> <name>` for every enumerator, and `local \
                      <class> <function> <name>` for every parameter and local of a method in the project's \
                      modules (`this` omitted): by default the modules the link read as objects, else those \
                      whose object path contains --modules or the config's `modules`."
    )]
    Names(CvNamesArgs),
    #[command(
        about = "Check the record's structure: the container and every type and symbol record re-encode",
        long_about = "Parse the record's directory and every subsection it lists, write the container back and \
                      compare it with the record byte for byte, and re-encode every type record \
                      (sstGlobalTypes) and every symbol record (sstAlignSym, sstGlobalSym, sstGlobalPub) \
                      through the record codec, counting those that come back identical and those that respell \
                      from VC6's length-prefixed names to 7.0's NUL-terminated ones. Exit 3 when anything comes \
                      back otherwise. JSON: the counts as the summary, the problems as rows."
    )]
    Check(CvCheckArgs),
}

#[derive(Debug, Args)]
pub struct CvDeclArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.codeview] table, or the path of an image carrying a record."
    )]
    pub image: String,
    #[arg(
        long,
        help = "Every class of the record, not only those with a body in, or used by, the project's objects."
    )]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CvDiffArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.codeview] table."
    )]
    pub image: String,
    #[arg(
        value_name = "VIEW",
        help = "A view of [[image.codeview.view]], by name."
    )]
    pub view: String,
    #[arg(
        long,
        value_name = "FILE",
        help = "The explained-mismatch table: `view <TAB> kind <TAB> subject <TAB> reason` (`*` any view, a \
                trailing `*` a subject prefix). Default: the config's `explained`."
    )]
    pub explained: Option<PathBuf>,
    #[arg(long, help = "Print the explained mismatches too.")]
    pub all: bool,
    #[arg(long, help = "Only the per-class and per-kind counts and the totals.")]
    pub counts: bool,
    #[arg(
        long,
        value_name = "CLASS",
        help = "Only this class (the record's name or ours)."
    )]
    pub class: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CvApplyArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.codeview] table."
    )]
    pub image: String,
    #[arg(
        value_name = "VIEW",
        help = "A view of [[image.codeview.view]], by name."
    )]
    pub view: String,
    #[arg(
        value_name = "KIND",
        value_parser = ["param-name", "access", "order", "field-name"],
        help = "The fixes to apply."
    )]
    pub kind: String,
    #[arg(
        long,
        value_name = "CLASS",
        help = "Only this class (the record's name or ours)."
    )]
    pub class: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Subjects never changed: `Class::member` as the record names them, one a line (`#` lines \
                skipped), or an MSVC build log whose C2248 errors name members our code reads across."
    )]
    pub except: Option<PathBuf>,
    #[arg(
        long,
        help = "Rewrite files the other views read too: the caller proves those images."
    )]
    pub shared: bool,
}

#[derive(Debug, Args)]
pub struct CvNamesArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.codeview] table, or the path of an image carrying a record."
    )]
    pub image: String,
    #[arg(
        long,
        value_name = "TEXT",
        help = "Only modules whose object path contains TEXT give `local` rows."
    )]
    pub modules: Option<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DebuginfoArgs {
    #[arg(
        value_name = "PATH",
        required = true,
        help = "Files or directories to inventory."
    )]
    pub paths: Vec<PathBuf>,
    #[arg(long, help = "List images with no CodeView record too.")]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CvCheckArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.codeview] table, or the path of an image carrying a record."
    )]
    pub image: String,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct SegmentsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "FUNC",
        help = "The function: a 0x-hexadecimal rva or a name the image's names resolve."
    )]
    pub func: String,
    #[arg(
        long,
        value_name = "DIR",
        help = "A build's object directory (repeatable), its workspace's static libraries read beside it."
    )]
    pub objs: Vec<PathBuf>,
    #[arg(long, value_name = "FILE", help = "One object (repeatable).")]
    pub obj: Vec<PathBuf>,
    #[arg(long, help = "Read no workspace library beside an --objs directory.")]
    pub no_libs: bool,
    #[arg(
        long,
        value_name = "SYMBOL",
        help = "Pick our COMDAT by its decorated name instead of by seat."
    )]
    pub symbol: Option<String>,
    #[arg(long, help = "List only the SHAPE segments.")]
    pub shape: bool,
    #[arg(
        long,
        value_name = "BYTES",
        default_value_t = msvc6_diff::segments::MAX_SEGMENT,
        help = "The cap on a segment's image bytes."
    )]
    pub max: usize,
    #[arg(
        long,
        help = "Also group adjacent SHAPE segments into dispatch units of at most --max image bytes."
    )]
    pub batch: bool,
    #[arg(
        long,
        value_name = "N|N-M|all",
        help = "Print these segments side by side, image | ours, every address operand named."
    )]
    pub asm: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        requires = "asm",
        help = "Write the --asm listing here instead of stdout."
    )]
    pub out: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct SegrankArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        long,
        value_name = "DIR",
        required = true,
        help = "A build's object directory (repeatable), its workspace's static libraries read beside it."
    )]
    pub objs: Vec<PathBuf>,
    #[arg(long, help = "Read no workspace library beside an --objs directory.")]
    pub no_libs: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Score the rvas in this file's first column (an earlier output). Default: every seated one."
    )]
    pub rvas: Option<PathBuf>,
    #[arg(
        long,
        conflicts_with = "rvas",
        help = "Score every member `try` would read as differing: each object's COMDATs seated at the addresses \
                their names resolve to and scored as `try --auto-members` scores them (a SELECT_ANY copy that an \
                earlier object in link order keeps, its copy fitting the address, is a DUPLICATE and not \
                scored), the members that differ and are not UNSEATED."
    )]
    pub all_differing: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "An earlier output: print the rvas whose figures moved and the total delta."
    )]
    pub prev: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the rows here instead of stdout."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Score N functions at once. Default: min(8, cores / 2)."
    )]
    pub jobs: Option<usize>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write every scored function's edit bytes by compiler decision here and print the per-class totals."
    )]
    pub decisions: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        requires = "decisions",
        help = "A file of `#regs` rows (seg, image va, region, verdict, ours, image, left, steps, decision): \
                each RA-coupled row's upstream leads with the traced decision."
    )]
    pub upstream: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct BuildObjs {
    #[arg(
        long,
        value_name = "DIR",
        help = "A build's object directory (repeatable), its workspace's static libraries read beside it, in \
                LINK's order."
    )]
    pub objs: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "One object (repeatable), after the directories'."
    )]
    pub obj: Vec<PathBuf>,
    #[arg(long, help = "Read no workspace library beside an --objs directory.")]
    pub no_libs: bool,
    #[arg(
        long,
        value_name = "NAME",
        help = "Read not this workspace library project beside an --objs directory (repeatable): a vendored \
                library, whose objects are not the image's own."
    )]
    pub skip_lib: Vec<String>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Work on N functions at once. Default: min(8, cores / 2)."
    )]
    pub jobs: Option<usize>,
}

#[derive(Debug, Args)]
pub struct RelocidArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long = "fn",
        value_name = "FUNC",
        help = "One function (a 0x-hexadecimal rva or a name): every field listed, agreeing ones too."
    )]
    pub func: Option<String>,
    #[arg(
        long,
        help = "List every field of every checked function, not only findings and leads."
    )]
    pub all: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the TSV here instead of stdout."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write every seated rva, ascending, as `<rva> TAB checked|not`."
    )]
    pub seats_out: Option<PathBuf>,
    #[arg(
        long,
        value_name = "PATH",
        help = "A name source of the image whose names are bookkeeping, notes and never a conflict: its path, \
                or a trailing part of it (repeatable)."
    )]
    pub notes_only: Vec<PathBuf>,
    #[arg(
        long = "lib",
        value_name = "FILE",
        help = "An archive whose members are the library oracle (repeatable). Default: the image's `libraries`."
    )]
    pub libs: Vec<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CalltargetsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        help = "List every uncovered callee with its kind, instead of the first six."
    )]
    pub list_uncovered: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "A LINK /MAP of our build: its data rows owned by an explicit object classify an uncovered \
                callee as OUR OWN DATA. It never enters the comparison."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write every disagreed row with its cause here, and FILE.bodies.tsv the callee censuses."
    )]
    pub causes: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LiteralsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        value_name = "FILE",
        help = "A LINK /MAP whose rows place a function no name of the image's does."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "PATH",
        help = "A name source consulted last, after --map (repeatable; its path or a trailing part)."
    )]
    pub notes_only: Vec<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct JumpsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "FUNC",
        help = "One function (a 0x-hexadecimal rva or a name). Default: every seated one, and the duplicates."
    )]
    pub func: Option<String>,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        value_name = "SYMBOL",
        requires = "func",
        help = "Pick our COMDAT for FUNC by its decorated name instead of by seat."
    )]
    pub symbol: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "A TOML of `[[named]]` TARGET-UNALIGNED rows and `[[duplicate]]` copies a reviewer accepted."
    )]
    pub excuses: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FUNC",
        help = "Also print this function's summary line in the all-seated run (repeatable)."
    )]
    pub show: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write every seated rva, ascending, one per line."
    )]
    pub seats_out: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct OperandsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long = "fn",
        value_name = "FUNC",
        help = "Only this function (a 0x-hexadecimal rva or a name)."
    )]
    pub func: Option<String>,
    #[arg(
        long,
        value_name = "SYMBOL",
        requires = "func",
        help = "Seat the COMDAT of this decorated name at --fn."
    )]
    pub symbol: Option<String>,
    #[arg(
        long,
        value_name = "CERT",
        default_value_t = 0.0,
        help = "Drop the hits below this certainty."
    )]
    pub min_cert: f64,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the TSV here instead of stdout."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        help = "Print, to stderr, each paired row where ours' esp depth minus the image's changes."
    )]
    pub depths: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct GlobalsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "OURS", help = "The image our build linked.")]
    pub ours: PathBuf,
    #[arg(long, value_name = "FILE", help = "The LINK /MAP of that same link.")]
    pub map: PathBuf,
    #[arg(long, help = "Print the pairs already at the reference's address too.")]
    pub all: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DataArgs {
    #[command(subcommand)]
    pub verb: DataVerb,
}

#[derive(Debug, Subcommand)]
pub enum DataVerb {
    #[command(
        about = "Seat each object's .data, .rdata and .bss in the reference by its relocations' votes",
        long_about = "Every DIR32 relocation site of a seated section (the .text COMDATs seed it, by name) holds, \
                      in the reference, its target's final address: less the addend and the target symbol's \
                      offset, a vote for the base of the target section. A section is seated at the base with the \
                      most votes (a tie to the lower), a site read only when the two bytes before it agree with \
                      the image and initialised data landing in raw bytes; a seated data section votes in turn, \
                      to a fixed point (at most 16 rounds). A data symbol an image name records votes too, and a \
                      data address another object's relocation names (pooled, dropped when two disagree). A \
                      section no vote seats is placed by CONTENT: each dword in a seated referencing section's \
                      image extent (a quarter more than ours) is a candidate, kept when the section's \
                      unrelocated bytes (4 or more) all match there, and seated when exactly one base survives; a \
                      one-vote seat whose bytes disagree goes to the content search. .data and .rdata are \
                      compared at their base, relocation fields masked; a .bss symbol whose own votes give \
                      another address is a BSS-ORDER row. One object prints its table; several print a ranking \
                      by differing bytes (--detail STEM: that one's table too). --layout NAME lists every seated \
                      section of that name over the objects in reference address order with the gap to the \
                      previous one; with --map our link's base for each and the shift (--steps: only where it \
                      steps). A screen: exit 0 unless an error."
    )]
    Seat(DataSeatArgs),
    #[command(
        about = "Where the reference put each object's data, and which objects' data it interleaves",
        long_about = "Points of reference address: every DIR32 relocation of a placed .text COMDAT to a data \
                      symbol, its instruction paired with the reference's by the literal screen's alignment and \
                      the reference's dword read in the paired field; a data section holding pointers to placed \
                      functions located by a unique match of its bytes with each pointer widened to the body and \
                      every ILT thunk reaching it (4-aligned, in every non-executable section); and the \
                      propagation of a section placed at one base through its own relocations (6 rounds). Per \
                      object section: the distinct bases (rva less offset) the points imply. Then the non-COMDAT \
                      points in reference order per section class, as runs of one object, and the pairs of \
                      objects whose runs interleave (A, B, A: the reference held them in one contribution); the \
                      placed functions as .text runs and A-B-A sandwiches. A screen: exit 0 unless an error."
    )]
    Runs(DataRunsArgs),
    #[command(
        about = "A LINK /MAP's data spans per object, and every differing data byte's owner",
        long_about = "From our link's /MAP, each object's hull per data section (lowest to highest symbol) and \
                      the hulls another object's symbol interrupts (explicit objects and archive members \
                      counted apart: archive members interleave, explicit objects lay one contribution each). \
                      A hull is not a size: an object publishing one symbol in a section spans 0. --objs adds \
                      each object's true data section sizes from its COFF headers, in the list's order. \
                      --candidate OURS \
                      attributes every byte where OURS differs from the reference in a section other than .text \
                      to the greatest map symbol at or below it: OURS or LIBRARY, a byte past a section's last \
                      symbol flagged PAST-LAST, one below its first UNATTRIBUTED. A screen: exit 0 unless an \
                      error."
    )]
    Owner(DataOwnerArgs),
}

#[derive(Debug, Args)]
pub struct DataSeatArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        value_name = "STEM",
        help = "With several objects, also print this object's table."
    )]
    pub detail: Option<String>,
    #[arg(
        long,
        value_name = "NAME",
        help = "Print every seated section of this name in reference address order, with the gaps."
    )]
    pub layout: Option<String>,
    #[arg(
        long,
        value_name = "FILE",
        requires = "layout",
        help = "With --layout, our link's /MAP: each row's base in our image and the shift."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        requires = "map",
        help = "With --layout --map, only the rows where the shift steps."
    )]
    pub steps: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DataRunsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        value_name = "FILE",
        help = "A LINK /MAP whose rows place a function no name of the image's does."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "PATH",
        help = "A name source consulted last, after --map (repeatable; its path or a trailing part)."
    )]
    pub notes_only: Vec<PathBuf>,
    #[arg(long, help = "Also list every non-COMDAT point with its symbol.")]
    pub list: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct DataOwnerArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "MAP", help = "A LINK /MAP of our build.")]
    pub map: PathBuf,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        value_name = "OURS",
        help = "The image of that same link: attribute its differing data bytes."
    )]
    pub candidate: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct PlaceArgs {
    #[command(subcommand)]
    pub verb: PlaceVerb,
}

#[derive(Debug, Subcommand)]
pub enum PlaceVerb {
    #[command(
        about = "Where our link put each own function, against where the reference has it",
        long_about = "Each `0001:` symbol of our link's /MAP at our rva, beside the rva the image's names give \
                      it: AT THE REFERENCE'S ADDRESS N of M, the misplaced ones listed (--all: every one). The \
                      provenance first: the newest of the image's TU sources against the map's mtime (a \
                      warning, never a refusal) and a digest of those sources' names and bytes, so two figures \
                      are compared only across one change. The ILT lattice (e9 thunks at .text's start, the \
                      first within 16 bytes) of both images, from the image beside the map (OURS with --ours, \
                      else the map's stem with the reference's extension). The ILINK reservation law, slot = \
                      round_up_16(floor(body * 1.25)), fitted over both sides' consecutive slots with the \
                      recorded extents as bodies. --content classes each differing byte of our own bodies at \
                      each image's own address: a base-relocated byte, an e8/e9 rel32 byte, or unexplained. \
                      --by-object prints the reference's .text order labelled with our map's object (an \
                      --own library's member read as its object): every symbol, --collapse one line per run, \
                      --interleave only the objects in more than one run, --self-control our own order. A \
                      screen: exit 0 unless an error."
    )]
    Map(PlaceMapArgs),
    #[command(
        about = "How many own COMDATs sit byte-identical at their reference address in an image we built",
        long_about = "The population: every .text COMDAT of the objects whose name resolves to an image \
                      address, deduplicated by name, the first object in link order owning it. SEATED: its \
                      bytes in OURS at the reference's file offset equal the reference's, every byte (only \
                      when the two images are one size; one of at most 32 bytes can agree by luck and is \
                      counted). AT ADDRESS: our /MAP puts it at the reference's rva (--map; else UNKNOWN). \
                      --list-placement prints every body's reference rva beside ours. A screen: exit 0 unless \
                      an error."
    )]
    Seated(PlaceSeatedArgs),
    #[command(
        about = "Which of our TUs the reference's own-code layout says each function belongs to",
        long_about = "The reference's own functions in rva order, each labelled with the object our /MAP \
                      defines it in (an archive member skipped), as runs of one object: one run per object \
                      would mean our TU partition matches the original's. A symbol counts only when a TU \
                      source of the image defines it: a line at column 0, with no `;`, naming `Class::name(` \
                      for a member or `name(` for a free function (constructors, destructors and operators \
                      `??x` never count: they are in-class or library bodies, and fold). --list prints each \
                      run's members, --moves the runs of four or fewer whose neighbours are one other object. \
                      The run count's floor is the object count, and it says nothing of the order inside a run \
                      or of any address. On an incremental reference each body ILINK evicted into an earlier \
                      COMDAT's reserve tail breaks a run whichever TU emitted it, so the count over-counts the \
                      disagreement by the evictions. A screen: exit 0 unless an error."
    )]
    Partition(PlacePartitionArgs),
    #[command(
        about = "Where is the reference's body at RVA inside an image of ours?",
        long_about = "The reference's body at RVA (the function index's recorded extent) searched in the \
                      candidate's .text with every byte the reference's base relocations touch and every e8/e9 \
                      rel32 operand masked, every match reported with its shift (one match is the strongest \
                      answer; several mean the body is not distinguishing). The search stops after 2^30 byte \
                      comparisons. A screen: exit 0 unless an error."
    )]
    Locate(PlaceLocateArgs),
}

#[derive(Debug, Args)]
pub struct PlaceMapArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "MAP", help = "A LINK /MAP of our build.")]
    pub map: PathBuf,
    #[arg(
        long,
        value_name = "OURS",
        help = "The image of that same link. Default: beside the map, its stem with the reference's extension."
    )]
    pub ours: Option<PathBuf>,
    #[arg(
        long,
        help = "Print every resolved symbol, not just the misplaced ones."
    )]
    pub all: bool,
    #[arg(
        long,
        help = "Also class every differing byte of our own bodies as reloc, rel32 or unexplained."
    )]
    pub content: bool,
    #[arg(
        long,
        help = "The reference's .text order labelled with our map's object."
    )]
    pub by_object: bool,
    #[arg(
        long,
        requires = "by_object",
        help = "With --by-object: one line per run."
    )]
    pub collapse: bool,
    #[arg(
        long,
        requires = "by_object",
        help = "With --by-object: only the objects in more than one run."
    )]
    pub interleave: bool,
    #[arg(
        long,
        requires = "by_object",
        help = "With --by-object: our own map's order, which must read one run per object."
    )]
    pub self_control: bool,
    #[arg(
        long,
        value_name = "LIB",
        help = "A library whose archive members are the image's own objects (repeatable)."
    )]
    pub own: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct PlaceSeatedArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "OURS", help = "The image our build linked.")]
    pub ours: PathBuf,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(long, value_name = "FILE", help = "The LINK /MAP of that same link.")]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        requires = "map",
        help = "List every body's reference rva beside ours."
    )]
    pub list_placement: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct PlacePartitionArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "MAP", help = "A LINK /MAP of our build.")]
    pub map: PathBuf,
    #[arg(long, help = "Also list the candidate single-function moves.")]
    pub moves: bool,
    #[arg(long, help = "List each run's members with their reference addresses.")]
    pub list: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct PlaceLocateArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "RVA",
        help = "The reference's rva of the body to find (0x-hexadecimal), not our linked address."
    )]
    pub rva: String,
    #[arg(value_name = "CANDIDATE", help = "Our linked image to search.")]
    pub candidate: PathBuf,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CallsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long,
        value_name = "FILTER",
        help = "Only callers matching: 0x<rva or va>, or a substring of a name."
    )]
    pub caller: Option<String>,
    #[arg(
        long,
        value_name = "FILTER",
        help = "Only callees matching: 0x<rva or va>, or a substring of a name."
    )]
    pub callee: Option<String>,
    #[arg(
        long,
        help = "Only the pairs whose counts differ, over callers both sides define."
    )]
    pub diff: bool,
    #[arg(
        long,
        help = "Per-callee totals over the callers both sides define, instead of pairs."
    )]
    pub by_callee: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct AccountArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "OURS", help = "The image our build linked.")]
    pub ours: PathBuf,
    #[arg(long, value_name = "FILE", help = "The LINK /MAP of that same link.")]
    pub map: PathBuf,
    #[arg(
        long,
        value_name = "LIB",
        help = "A library whose archive members are the image's own objects (repeatable): its /MAP prefix, \
                `lib:member.obj`."
    )]
    pub own: Vec<String>,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct AddrsymArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[command(flatten)]
    pub objs: BuildObjs,
    #[arg(
        long = "fn",
        value_name = "FUNC",
        help = "A function (repeatable; a 0x-hexadecimal rva or a name). Default: every seated one."
    )]
    pub funcs: Vec<String>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our linked image. Default: the reference's file name in the first --objs directory."
    )]
    pub exe: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Its LINK /MAP. Default: beside the image, with the extension .map."
    )]
    pub map: Option<PathBuf>,
    #[arg(long, help = "Print the agreeing rows too.")]
    pub all: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the TSV here instead of stdout."
    )]
    pub out: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct CompileArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "SOURCE",
        help = "The source to compile: a TU of the image's project, or a copy whose file stem names one."
    )]
    pub source: PathBuf,
    #[arg(
        long,
        value_name = "TU",
        help = "Compile with this TU's compile spec instead of the one SOURCE names."
    )]
    pub args_from: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FLAGS",
        allow_hyphen_values = true,
        help = "Replace the spec's code switches (every one you omit is dropped; use --include for a directory)."
    )]
    pub cflags: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        help = "An include directory after the spec's own (repeatable)."
    )]
    pub include: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Compile in DIR. Default: <temp>/msvc6t-work/<image>/<stem>."
    )]
    pub out: Option<PathBuf>,
    #[arg(long, help = "Also write CL's /FAcs listing beside the object.")]
    pub listing: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct IsoArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "FUNC",
        help = "The function: a 0x-hexadecimal rva or a name the image's names resolve."
    )]
    pub func: String,
    #[arg(
        long,
        value_name = "TU",
        help = "The project TU that defines the function. Default: the one TU whose definitions name it."
    )]
    pub src: Option<PathBuf>,
    #[arg(
        long,
        value_name = "NAME",
        help = "The function's name (`Update`, `Actor::Update` or decorated) when the image's names give none."
    )]
    pub name: Option<String>,
    #[arg(
        long,
        value_name = "TU",
        help = "Compile with this TU's compile spec instead of the one the TU names."
    )]
    pub args_from: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Write the iso file, both compiles and iso.json here. Default: <temp>/msvc6t-work/<image>/iso-<rva>."
    )]
    pub out: Option<PathBuf>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LoopArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "FUNC",
        help = "The function: a 0x-hexadecimal rva or a name the image's names resolve."
    )]
    pub func: String,
    #[arg(
        value_name = "FILE",
        help = "The base file: the project TU, a copy of it, or the iso file."
    )]
    pub file: PathBuf,
    #[arg(
        value_name = "VARIANT",
        help = "Variant files, each a TU or iso copy with the body edited; compiled beside FILE."
    )]
    pub variants: Vec<PathBuf>,
    #[arg(
        long,
        help = "Compile the files as given (whole TUs), not their one-function reduction. Required when the iso DIFFERS."
    )]
    pub whole: bool,
    #[arg(
        long,
        help = "Print the best file's first differing segment and its source lines instead of every differing segment."
    )]
    pub frontier: bool,
    #[arg(
        long,
        value_name = "TU",
        help = "Compile with this TU's compile spec. Default: the proof's, else the TU FILE names."
    )]
    pub args_from: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where `iso` recorded the proof. Default: <temp>/msvc6t-work/<image>/iso-<rva>."
    )]
    pub iso: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Compile here. Default: <temp>/msvc6t-work/<image>/loop/<rva>."
    )]
    pub work: Option<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Keep the history TSV here as <image>-<rva>.tsv. Default: <temp>/msvc6t-work/<image>/loop/<rva>.tsv. A history over 16 MiB, or one that cannot be read, is refused before any compile and left as it is."
    )]
    pub history: Option<PathBuf>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Compile N files at once. Default: 4."
    )]
    pub jobs: Option<usize>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = msvc6_diff::fastloop::ASM_LINES,
        help = "Lines of differing rows to print (every row is in <work>/last/asm.txt)."
    )]
    pub asm_lines: usize,
    #[command(flatten)]
    pub json: JsonArg,
}

fn parse_rva(s: &str) -> Result<u32, String> {
    let t = s.trim();
    t.strip_prefix("0x")
        .or_else(|| t.strip_prefix("0X"))
        .and_then(|h| u32::from_str_radix(h, 16).ok())
        .ok_or_else(|| format!("`{s}` is not a 0x-hexadecimal rva"))
}

#[derive(Debug, Args)]
pub struct TryArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "RVA",
        help = "Where COMDAT[0] is compared: a 0x-hexadecimal rva or a name the image's names resolve. Under \
                --auto-members a hint: `auto`, COMDAT[0]'s address or any COMDAT's of the object."
    )]
    pub rva: String,
    #[arg(
        value_name = "SOURCE",
        help = "The source to compile: a TU of the image's project, or a copy whose file stem names one."
    )]
    pub source: PathBuf,
    #[arg(
        long,
        conflicts_with = "members",
        help = "Seat each COMDAT at the address its own symbol resolves to through the image's names. A name on \
                the image's `dropped` list (a COMDAT the original link's /OPT:REF removed) is DROPPED: it holds the \
                place of the member before it and is not scored; one that resolves is refused."
    )]
    pub auto_members: bool,
    #[arg(
        long,
        value_name = "RVA,...",
        value_delimiter = ',',
        value_parser = parse_rva,
        help = "Seat COMDATs 1..n, in object order, at these 0x-hexadecimal rvas (COMDAT[0] is RVA)."
    )]
    pub members: Vec<u32>,
    #[arg(
        long,
        value_name = "TU",
        help = "Compile with this TU's compile spec instead of the one SOURCE names."
    )]
    pub args_from: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FLAGS",
        allow_hyphen_values = true,
        help = "Replace the spec's code switches (every one you omit is dropped; use --include for a directory)."
    )]
    pub cflags: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        help = "An include directory after the spec's own (repeatable)."
    )]
    pub include: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "TAG",
        help = "Compile in a private directory <stem>_<TAG> (each character outside [A-Za-z0-9] read as `_`), \
                so two runs on one TU cannot overwrite each other's object."
    )]
    pub isolate: Option<String>,
    #[arg(
        long,
        help = "Per seated COMDAT that differs, split the IMAGE function (its recorded extent, pad stripped) into \
                basic blocks (the entry, every branch and jump-table target, the instruction after every \
                jcc/jmp/ret) and place the instruction diff's rows in them by image address: blocks, exact \
                blocks and bytes, wrong / reg / ord / ours-only. After a length drift rows land in the wrong \
                block; --align locates the drift."
    )]
    pub block_map: bool,
    #[arg(
        long,
        value_name = "BYTES",
        value_parser = parse_rva,
        requires = "block_map",
        help = "With --block-map, also one row per this many image bytes (0x-hexadecimal)."
    )]
    pub window: Option<u32>,
    #[arg(
        long,
        help = "Per seated COMDAT that differs, align the image's and our instruction SHAPES (immediates and \
                displacements read N, registers R) and print our offset less the image's as the walk passes \
                every --align-step image bytes, so interior drifts that cancel by the end are located."
    )]
    pub align: bool,
    #[arg(
        long,
        value_name = "BYTES",
        value_parser = parse_rva,
        default_value = "0x400",
        requires = "align",
        help = "With --align, image bytes per delta row (0x-hexadecimal)."
    )]
    pub align_step: u32,
    #[arg(
        long,
        requires = "align",
        help = "With --align, match registers exactly instead of reading every general register as R."
    )]
    pub keep_regs: bool,
    #[command(flatten)]
    pub json: JsonArg,
}
