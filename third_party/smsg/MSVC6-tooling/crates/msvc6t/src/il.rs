use crate::Outcome;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_il::grammar::{self, Grammar};
use msvc6_il::unit::{Files, Unit};
use std::path::{Path, PathBuf};

#[derive(Debug, Args)]
pub struct IlArgs {
    #[command(subcommand)]
    pub cmd: IlCmd,
}

#[derive(Debug, Subcommand)]
pub enum IlCmd {
    #[command(
        about = "Every record of a kept IL set, in the order C2 reads it",
        long_about = "Print every record of an IL set the front end kept (`CL /Bk<BASE>` writes <BASE>gl, \
                      <BASE>sy, <BASE>ex and <BASE>in): the gl header and the global symbol records, the in \
                      stream (initialised data in definition order), each function's symbol list from sy, and \
                      the ex stream walked end to end, each body's postfix records folded into statement trees \
                      (`; discard(asg.i32(x, add(load.i32(a), 1)))`), `[1st]` on a symbol's first reference in \
                      its body and `first refs:` after it (the order C2 makes their storage). The operand \
                      grammar of the ex records (each op's format, attribute word, and the 0x4f sub-op codes) \
                      is read out of the C2.DLL of --c2 or of the toolchain at run time; the symbol, in and \
                      header layouts are the reader's own. A record C2 would refuse is an error naming its \
                      offset."
    )]
    Dump(DumpArgs),
    #[command(
        about = "Every front-end id from the first the IL carries to its next-id, with the record that names it",
        long_about = "C1XX numbers every entity it creates from one counter, and only the entities that reach \
                      the IL carry their number in a file: a symbol record (gl, or a function's sy list), a \
                      front-end temporary (`9b ctemp`), a label. Every other numbered entity (a prototype's \
                      formals, a class's members, a typedef) leaves a gap. Lists every id up to the gl header's \
                      next-id with its record's kind, name and level (`-` for a gap), the function whose list \
                      holds it or whose body first names it, its first reference, and a `-- function` line at \
                      each definition's start id."
    )]
    Ids(SetArgs),
    #[command(
        about = "One row per function definition: its start id, span, size estimate and records",
        long_about = "One row per function definition, in ex order: its start (the smallest id its own records \
                      carry: the first formal, else the function, else `this` or the id before it for an older \
                      declaration), its span to the next definition's start, the span's ids by kind and its \
                      gaps, and the fdef's size estimate (s6d) beside the count of the body's ex records that \
                      are not 0x4f pragmas: the two are equal, the measure the front end gives C2's inliner. \
                      The breakdowns read at most 4 ids a held id (at least 2^20), which spans that do not \
                      overlap never reach; a breakdown past it is not read, and a NOTE says how many."
    )]
    Funcs(SetArgs),
    #[command(
        about = "The IL grammar tables read out of C2.DLL (and the front end's writer table)",
        long_about = "Locate and print the operand grammar C2's IL reader decodes the ex stream with, read out \
                      of the user's own C2.DLL: one format byte and one attribute word per op, and the operand \
                      codes of each 0x4f sub-op. The tables are found by their structure (the sub-op table \
                      whose entries 1 and 2 are the codes \"l\" and \"s\", the format table just below its code \
                      strings with its ops 0x26, 0x29, 0x33, 0x3e, 0x4f and 0x54 in the formats those ops read, \
                      the attribute table just below that whose branch, return and discard ops end a statement), \
                      never shipped. With the front end (--c1xx, or the toolchain's C1XX.DLL), --check also \
                      locates its op writer table by C2's attribute words and holds every op C2's reader \
                      handles to the operand sequence the writer emits."
    )]
    Tables(TablesArgs),
    #[command(
        about = "C1XX's identifier hash and the block-scope bucket of each name",
        long_about = "C1XX's identifier hash, `h = c + (h >> 4) + h * 4` over the name's bytes, the 16-bit hash \
                      `(h >> 16) ^ (h & 0xffff)` a scope table keys on, and its bucket of 16: a block lists its \
                      locals bucket by bucket upwards, newest declaration first in a bucket, and at /Od that \
                      list is their frame order."
    )]
    Hash(HashArgs),
    #[command(
        about = "Compile one TU of an image with /Bk and keep its IL beside its object and listing",
        long_about = "Compile SOURCE (a TU of the image's project, as `compile` names one) with the spec `dsp \
                      show` gives it, plus `/Bk<stem>.` and CL's /FAcs listing, in --out, on the configured \
                      runner, bypassing the compile cache (the cache holds no IL). Prints the IL set's base, \
                      for `il dump`, `il ids`, `il funcs` and `inline plan`, and the pch the compile wrote, if \
                      any (the ex stream of a pch-create compile's header bodies). Every compiler-running verb \
                      first checks this binary's provenance (`msvc6t lint --provenance`)."
    )]
    Keep(KeepArgs),
}

#[derive(Debug, Args)]
pub struct GrammarArgs {
    #[arg(
        long,
        value_name = "FILE",
        help = "The C2.DLL whose IL grammar decodes the ex stream. Default: the toolchain's."
    )]
    pub c2: Option<PathBuf>,
    #[arg(
        long,
        value_name = "ID",
        help = "The toolchain whose C2.DLL decodes the ex stream. Default: the first in the config."
    )]
    pub toolchain: Option<String>,
}

#[derive(Debug, Args)]
pub struct SetArgs {
    #[arg(
        value_name = "BASE",
        required_unless_present = "tu",
        help = "The IL set's base: <BASE>gl, <BASE>sy, <BASE>ex and <BASE>in (CL's /Bk<BASE>; `dir/two.` for \
                dir/two.gl)."
    )]
    pub base: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The pch the compile wrote: the stream of every definition whose fflags bit 0 is set."
    )]
    pub pch: Option<PathBuf>,
    #[arg(
        long,
        num_args = 2,
        value_names = ["IMAGE", "SOURCE"],
        conflicts_with_all = ["base", "pch"],
        help = "Compile this TU of the image as `il keep` does (in --out) and read the IL it keeps, with its pch \
                and its toolchain's C2.DLL. Prints only what the verb prints."
    )]
    pub tu: Vec<String>,
    #[arg(
        long,
        value_name = "DIR",
        requires = "tu",
        help = "With --tu: compile in DIR. Default: <temp>/msvc6t-work/<image>/il/<stem>."
    )]
    pub out: Option<PathBuf>,
    #[command(flatten)]
    pub grammar: GrammarArgs,
}

#[derive(Debug, Args)]
pub struct DumpArgs {
    #[command(flatten)]
    pub set: SetArgs,
    #[arg(long, help = "Also print each record's bytes.")]
    pub hex: bool,
    #[arg(long, help = "Do not fold the ex records into statement trees.")]
    pub no_tree: bool,
    #[arg(
        long = "fn",
        value_name = "NAME",
        help = "Print only this function's body (its decorated name; repeatable), from the ex stream or, with --pch, the pch."
    )]
    pub func: Vec<String>,
}

#[derive(Debug, Args)]
pub struct TablesArgs {
    #[command(flatten)]
    pub grammar: GrammarArgs,
    #[arg(
        long,
        value_name = "FILE",
        help = "The front end whose writer table --check reads. Default: the toolchain's C1XX.DLL."
    )]
    pub c1xx: Option<PathBuf>,
    #[arg(
        long,
        help = "Hold the front end's writer table to C2's reader: exits 3 when an op's operands differ."
    )]
    pub check: bool,
}

#[derive(Debug, Args)]
pub struct HashArgs {
    #[arg(value_name = "NAME", required = true, help = "The names to hash.")]
    pub names: Vec<String>,
}

#[derive(Debug, Args)]
pub struct KeepArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(
        value_name = "SOURCE",
        help = "A TU of the image's project, or a copy whose file stem names one."
    )]
    pub source: PathBuf,
    #[arg(
        long,
        value_name = "DIR",
        help = "Compile in DIR. Default: <temp>/msvc6t-work/<image>/il/<stem>."
    )]
    pub out: Option<PathBuf>,
    #[arg(long, value_name = "FLAGS", help = "Replace the spec's code switches.")]
    pub cflags: Option<String>,
}

#[derive(Debug, Args)]
pub struct InlineArgs {
    #[command(subcommand)]
    pub cmd: InlineCmd,
}

#[derive(Debug, Subcommand)]
pub enum InlineCmd {
    #[command(
        about = "Walk each compiled function's inline sites under C2's static budget",
        long_about = "The static inline plan of a kept IL set. A site is a call in a body's ex records whose \
                      callee is a definition of this unit marked an inline candidate (fflags 0x40), in the order \
                      the calls complete (a call after the calls in its arguments), with none in dead code \
                      (after an unconditional goto, until a label something jumps to). An eh.live cleanup \
                      (the unwind funclet's call) is no site; an object's destructor is a site at each \
                      eh.leave and at each eh.unwind that pops (0x20) its objects, and none at a \
                      constructor's closing eh.unwind. The exception gate calls a callee with a try block \
                      (fflags 0x200), and one with destructible objects (0x100) under a root that has \
                      neither; a callee already on the walk is recursive and called. A compiled function \
                      (fflags 0x20) is a root: its budget is twice its size estimate (the signed s6d) held to \
                      1000..35000, and the running total starts at that size. A site expands when its \
                      callee's size is at most 40 (free) or at most the budget left, and the running total is \
                      at most 35000; an expansion adds its size to the running total, a non-free one takes its \
                      size from the budget, and its own sites are walked with the budget left divided by the \
                      sites left in this body (this one counted); what they spend is taken from this body's \
                      budget when it returns. Prints each root's walk, one line per site: callee, size, \
                      budget left, sites left, running total, verdict. With --listing, holds each listing \
                      PROC's calls of every candidate (its `call` and tail `jmp` instructions naming it, \
                      outside the text$x funclets) to the plan's for that root, skipping adjustor and \
                      vtordisp thunks and not judging a function's calls of itself (C2 turns a tail self call \
                      into a jump), and exits 3 when one differs."
    )]
    Plan(PlanArgs),
    #[command(
        about = "Compile TUs with /Bk and a listing, and hold the inline plan to each listing's calls",
        long_about = "For each SOURCE (a TU of the image's project; with --all every TU), compile it as `il \
                      keep` does, plan its inline sites as `inline plan` does, and hold every function in the \
                      listing to the plan as `inline plan --listing` does: for each candidate the function's \
                      walk calls rather than expands, the planned number of calls against the listing's, and \
                      for each it expands, none. Prints a line per TU and per differing pair, and the totals. \
                      Exits 3 when a pair differs or a TU cannot be compiled or read. Every compiler-running \
                      verb first checks this binary's provenance (`msvc6t lint --provenance`)."
    )]
    Check(CheckArgs),
}

#[derive(Debug, Args)]
pub struct PlanArgs {
    #[command(flatten)]
    pub set: SetArgs,
    #[arg(
        long,
        value_name = "FILE",
        help = "CL's listing of the same compile (/FAs or /FAcs)."
    )]
    pub listing: Option<PathBuf>,
    #[arg(
        long = "fn",
        value_name = "NAME",
        help = "Plan only this function (its decorated name; repeatable)."
    )]
    pub func: Vec<String>,
    #[arg(long, help = "Print only the listing check, not each root's walk.")]
    pub quiet: bool,
}

#[derive(Debug, Args)]
pub struct CheckArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "SOURCE", help = "TUs of the image's project.")]
    pub sources: Vec<PathBuf>,
    #[arg(long, help = "Every TU of the image's own project.")]
    pub all: bool,
    #[arg(
        long,
        value_name = "DIR",
        help = "Compile under DIR, one directory per TU. Default: <temp>/msvc6t-work/<image>/inline."
    )]
    pub out: Option<PathBuf>,
    #[arg(long, help = "Also print each differing pair's walk.")]
    pub walks: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write one row per TU (its totals and differing pairs) and the totals as JSON to FILE."
    )]
    pub json: Option<PathBuf>,
}

fn grammar_of(config: Option<&Path>, g: &GrammarArgs, image_tc: Option<&str>) -> Result<Grammar> {
    if let Some(p) = &g.c2 {
        return Grammar::load(p);
    }
    let tc = toolchain(config, g.toolchain.as_deref().or(image_tc))?;
    Grammar::load(&tc.tool("C2.DLL")?)
}

fn toolchain(
    config: Option<&Path>,
    id: Option<&str>,
) -> Result<msvc6_toolchain::layout::Toolchain> {
    let cfg = msvc6_project::config::toolchain_config(config).context(
        "the IL grammar comes from a C2.DLL: pass --c2 FILE, or a config whose toolchain has one",
    )?;
    let id = match id {
        Some(i) => i.to_string(),
        None => cfg
            .toolchains
            .keys()
            .next()
            .cloned()
            .context("the config names no [toolchain.*]")?,
    };
    msvc6_toolchain::layout::Toolchain::named(&cfg, &id)
}

fn set_of(
    config: Option<&Path>,
    s: &SetArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<(Files, Grammar)> {
    if let [image, source] = s.tu.as_slice() {
        guard()?;
        let k = keep(config, image, Path::new(source), s.out.as_deref(), None)?;
        let g = grammar_of(config, &s.grammar, Some(&k.toolchain))?;
        return Ok((Files::open(&k.base, k.pch.as_deref())?, g));
    }
    let Some(base) = &s.base else {
        bail!("name the IL set's BASE, or --tu IMAGE SOURCE");
    };
    let g = grammar_of(config, &s.grammar, None)?;
    Ok((Files::open(base, s.pch.as_deref())?, g))
}

pub fn run(config: Option<&Path>, a: &IlArgs, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    match &a.cmd {
        IlCmd::Dump(d) => {
            let (f, g) = set_of(config, &d.set, guard)?;
            let opts = msvc6_il::dump::Opts {
                hex: d.hex,
                tree: !d.no_tree,
            };
            if d.func.is_empty() {
                print!("{}", msvc6_il::dump::dump(&f, &g, &opts)?);
                return Ok(Outcome::Ok);
            }
            let u = Unit::read(&f)?;
            let defs = u.fdefs();
            for name in &d.func {
                let Some(def) = defs.iter().find(|s| s.name_text() == *name) else {
                    bail!("{}: no definition named {name}", f.base);
                };
                print!("{}", msvc6_il::dump::dump_function(&f, &u, &g, def, &opts)?);
            }
            Ok(Outcome::Ok)
        }
        IlCmd::Ids(s) | IlCmd::Funcs(s) => {
            let (f, g) = set_of(config, s, guard)?;
            let u = msvc6_il::ids::load(&f, &g)?;
            if matches!(a.cmd, IlCmd::Ids(_)) {
                print!("{}", msvc6_il::ids::ids_text(&u));
            } else {
                print!("{}", msvc6_il::ids::funcs_text(&u));
            }
            Ok(Outcome::Ok)
        }
        IlCmd::Tables(t) => tables(config, t),
        IlCmd::Hash(h) => {
            for n in &h.names {
                let (x, s) = msvc6_il::sym::name_hash(n.as_bytes());
                println!(
                    "{n:24} h 0x{x:08x}  hash16 0x{s:04x}  bucket {}",
                    s % msvc6_il::sym::SCOPE_BUCKETS
                );
            }
            Ok(Outcome::Ok)
        }
        IlCmd::Keep(k) => {
            guard()?;
            let kept = keep(
                config,
                &k.image,
                &k.source,
                k.out.as_deref(),
                k.cflags.as_deref(),
            )?;
            println!("il keep: {}", kept.base.display());
            if let Some(p) = &kept.pch {
                println!("il keep: pch {}", p.display());
            }
            println!("il keep: listing {}", kept.listing.display());
            Ok(Outcome::Ok)
        }
    }
}

fn tables(config: Option<&Path>, t: &TablesArgs) -> Result<Outcome> {
    let g = grammar_of(config, &t.grammar, None)?;
    let found = g.found.unwrap_or_default();
    println!("C2 grammar of {}", g.source);
    println!(
        "  attribute table at {:#x} ({} ops), format table at {:#x} ({} ops), 0x4f sub-op table at {:#x} ({} sub-ops)",
        found.attr,
        g.attr.len(),
        found.fmt,
        g.fmt.len(),
        found.pragma,
        g.pragma.len()
    );
    let c1xx = match (&t.c1xx, &t.grammar.c2) {
        (Some(p), _) => Some(p.clone()),
        (None, None) => toolchain(config, t.grammar.toolchain.as_deref())
            .ok()
            .and_then(|tc| tc.tool("C1XX.DLL").ok()),
        (None, Some(_)) => None,
    };
    let writer = match &c1xx {
        Some(p) => {
            let img = grammar::read_image(p)?;
            Some(
                grammar::writer(&img, &g, &msvc6_il::sym::WRITER_ANCHORS)
                    .with_context(|| p.display().to_string())?,
            )
        }
        None => None,
    };
    println!(
        "op  fmt  attr    C2 reader operands{:28}front-end writer codes  name",
        ""
    );
    let mut bad = 0usize;
    let mut handled = 0usize;
    let mut attr_diff = 0usize;
    for (i, &f) in g.fmt.iter().enumerate() {
        let Ok(op) = u8::try_from(i) else { break };
        let a = g.attr(op);
        let info = msvc6_il::ops::info(op);
        let w = writer.as_ref().and_then(|w| w.ops.get(i));
        let codes = match w {
            Some((_, Some(c))) => c
                .iter()
                .map(|b| format!("{b:02x}"))
                .collect::<Vec<_>>()
                .join(" "),
            Some((_, None)) => "-".into(),
            None => "?".into(),
        };
        if let Some((wa, c)) = w {
            if *wa != a & 0x7fff {
                attr_diff = attr_diff.saturating_add(1);
                println!("  attr differs, op 0x{op:02x}: C2 0x{a:04x}, front end 0x{wa:04x}");
            }
            if info.name != "?" && op != msvc6_il::ops::PRAGMA {
                handled = handled.saturating_add(1);
                let wt = grammar::writer_tokens(c.as_deref().unwrap_or_default());
                let rt = grammar::fmt_tokens(f);
                if wt != rt {
                    bad = bad.saturating_add(1);
                    println!(
                        "  MISMATCH op 0x{op:02x}: the front end writes [{wt}], C2 reads [{rt}]"
                    );
                }
            }
        }
        if f == 0 && codes == "-" && info.name == "?" && !t.check {
            continue;
        }
        println!(
            "{op:02x}  {f:02x}   0x{a:04x}  {:<46}{codes:<24}{}",
            grammar::fmt_desc(f),
            info.name
        );
    }
    println!("\n0x4f sub-ops: sub  flag  codes");
    for (i, p) in g.pragma.iter().enumerate() {
        if p.codes.is_none() && p.flag == 0 {
            continue;
        }
        let c = p.codes.as_ref().map_or_else(
            || "-".to_string(),
            |c| {
                c.iter()
                    .map(|&b| format!("{b:02x} ({})", grammar::pragma_code_desc(b)))
                    .collect::<Vec<_>>()
                    .join(", ")
            },
        );
        let name = i16::try_from(i).map_or("?", msvc6_il::ops::pragma_name);
        println!("  {i:02x}  {}  {c}  {name}", p.flag);
    }
    if let Some(w) = &writer {
        println!(
            "\nsymbol classes: reader fields | front-end writer codes (table at {})",
            w.syms_at
                .map_or_else(|| "not found".to_string(), |a| format!("{a:#x}"))
        );
        for (k, codes) in w.syms.iter().enumerate() {
            let Ok(class) = u8::try_from(k) else { break };
            let fields = msvc6_il::sym::READER_LAYOUT
                .iter()
                .find(|(c, _)| *c == class)
                .map_or("(C2 ICEs)", |(_, f)| *f);
            let codes = codes
                .iter()
                .map(|b| format!("{b:02x}"))
                .collect::<Vec<_>>()
                .join(" ");
            println!(
                "  {class:02x} {:<10} {fields:<92} | {codes}",
                msvc6_il::sym::class_name(class)
            );
        }
    }
    if t.check {
        let Some(w) = &writer else {
            bail!("--check reads the front end's writer table: pass --c1xx FILE or a toolchain");
        };
        println!(
            "\ncheck: operand sequences: the front end writes what C2 reads for {} of the {handled} ops C2 handles \
             (0x4f by its sub-op table); attribute words: equal but for C2-only bit 0x8000 and {attr_diff} ops; \
             writer table at {:#x}",
            if bad == 0 { "all" } else { "NOT all" },
            w.ops_at
        );
        if bad > 0 {
            return Ok(Outcome::Differs);
        }
    }
    Ok(Outcome::Ok)
}

#[derive(Debug, Clone)]
pub struct Kept {
    pub base: PathBuf,
    pub pch: Option<PathBuf>,
    pub listing: PathBuf,
    pub toolchain: String,
}

fn pch_of(dir: &Path, args: &[String]) -> Option<PathBuf> {
    let named = args.iter().find_map(|a| {
        let l = a.to_ascii_lowercase();
        (l.starts_with("/fp") || l.starts_with("-fp"))
            .then(|| a.get(3..).unwrap_or_default().to_string())
    });
    let p = match named {
        Some(n) if !n.is_empty() => {
            let name = n.rsplit(['\\', '/']).next().unwrap_or(&n).to_string();
            dir.join(name)
        }
        _ => dir.join("vc60.pch"),
    };
    p.is_file().then_some(p)
}

pub fn keep(
    config: Option<&Path>,
    image: &str,
    source: &Path,
    out: Option<&Path>,
    cflags: Option<&str>,
) -> Result<Kept> {
    let pr = crate::diff::prepare(config, image, source, None, cflags, &[])?;
    let id = pr
        .project
        .images
        .get(pr.n)
        .map(|d| d.id.clone())
        .unwrap_or_default();
    let dir = out.map_or_else(
        || {
            std::env::temp_dir()
                .join("msvc6t-work")
                .join(&id)
                .join("il")
                .join(&pr.unit.stem)
        },
        Path::to_path_buf,
    );
    std::fs::create_dir_all(&dir).with_context(|| format!("creating {}", dir.display()))?;
    let stem = pr.unit.stem.clone();
    for ext in ["gl", "sy", "ex", "in", "db"] {
        let f = dir.join(format!("{stem}.{ext}"));
        if f.exists() {
            std::fs::remove_file(&f).with_context(|| format!("removing {}", f.display()))?;
        }
    }
    let mut unit = pr.unit.clone();
    unit.cflags.push(format!("/Bk{stem}."));
    let c = msvc6_diff::compile::Compiler::new(&pr.project)?;
    let sess = c.runner.session()?;
    let done = c.compile(&sess, &unit, &dir, true)?;
    let base = dir.join(format!("{stem}."));
    if !msvc6_il::unit::member(&base, "gl").is_file() {
        bail!(
            "CL kept no IL for {} (no {}gl):\n{}",
            source.display(),
            base.display(),
            done.text.trim_end()
        );
    }
    let listing = done
        .cod
        .clone()
        .with_context(|| format!("CL wrote no listing for {}", source.display()))?;
    Ok(Kept {
        pch: pch_of(&dir, &done.args),
        base,
        listing,
        toolchain: unit.toolchain,
    })
}

pub fn inline(
    config: Option<&Path>,
    a: &InlineArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    match &a.cmd {
        InlineCmd::Plan(p) => {
            let (f, g) = set_of(config, &p.set, guard)?;
            let u = Unit::read(&f)?;
            if !p.quiet {
                let mut planner = msvc6_il::inline::Planner::new(&u, &f, &g);
                for root in planner.roots() {
                    let name = u.short(root);
                    if !p.func.is_empty() && !p.func.contains(&name) {
                        continue;
                    }
                    if let Some(w) = planner.walk(root) {
                        print!("{}", msvc6_il::inline::text(&w));
                    }
                }
                for (f, e) in &planner.errors {
                    println!("NOTE {f}: {e}");
                }
            }
            let Some(l) = &p.listing else {
                return Ok(Outcome::Ok);
            };
            let text =
                msvc6_base::srcio::tool_text(&msvc6_il::file::read(l, msvc6_il::unit::MAX_STREAM)?);
            let t = msvc6_il::inline::check(&u, &f, &g, &text, &p.func, false);
            say_check(&t);
            println!("INLINE CHECK: {}", t.line());
            Ok(if t.agree == t.pairs {
                Outcome::Ok
            } else {
                Outcome::Differs
            })
        }
        InlineCmd::Check(c) => {
            guard()?;
            check(config, c)
        }
    }
}

fn check(config: Option<&Path>, c: &CheckArgs) -> Result<Outcome> {
    let sources: Vec<PathBuf> = if c.all {
        let p = msvc6_project::config::Project::require(config)?;
        let (n, d) = msvc6_project::registry::resolve_id(&p, &c.image)?;
        let specs = msvc6_project::spec::resolve(&p, n)?;
        specs
            .specs
            .iter()
            .filter(|s| s.image == d.id)
            .map(|s| PathBuf::from(&s.path))
            .collect()
    } else {
        c.sources.clone()
    };
    if sources.is_empty() {
        bail!("no TU: name SOURCE files or pass --all");
    }
    let root = c.out.clone().unwrap_or_else(|| {
        std::env::temp_dir()
            .join("msvc6t-work")
            .join(&c.image)
            .join("inline")
    });
    let mut total = msvc6_il::inline::Check::default();
    let mut rows: Vec<TuRow> = Vec::new();
    let mut tus = 0usize;
    for s in &sources {
        let stem = s
            .file_stem()
            .map(|x| x.to_string_lossy().into_owned())
            .unwrap_or_default();
        let kept = match keep(config, &c.image, s, Some(&root.join(&stem)), None) {
            Ok(k) => k,
            Err(e) => {
                println!("CHECK {stem}: NOTE not compiled: {e:#}");
                continue;
            }
        };
        let g = grammar_of(
            config,
            &GrammarArgs {
                c2: None,
                toolchain: None,
            },
            Some(&kept.toolchain),
        )?;
        let f = Files::open(&kept.base, kept.pch.as_deref())?;
        let u = match Unit::read(&f) {
            Ok(u) => u,
            Err(e) => {
                println!("CHECK {stem}: NOTE IL unread: {e:#}");
                continue;
            }
        };
        let text = msvc6_base::srcio::tool_text(&msvc6_il::file::read(
            &kept.listing,
            msvc6_il::unit::MAX_STREAM,
        )?);
        let t = msvc6_il::inline::check(&u, &f, &g, &text, &[], c.walks);
        tus = tus.saturating_add(1);
        println!(
            "CHECK {stem}: {}{}",
            t.line(),
            if t.agree == t.pairs { "" } else { "  DIFFERS" }
        );
        say_check(&t);
        total.add(&t);
        rows.push(TuRow {
            tu: stem.clone(),
            check: t,
        });
    }
    println!("INLINE TOTAL: {tus} TU(s), {}", total.line());
    if let Some(j) = &c.json {
        msvc6_base::rowjson::write_with(j, "inline check", &c.image, Some(&total), &rows)?;
    }
    Ok(if total.agree == total.pairs && tus == sources.len() {
        Outcome::Ok
    } else {
        Outcome::Differs
    })
}

fn say_check(c: &msvc6_il::inline::Check) {
    for d in &c.diffs {
        println!(
            "  DIFF {} -> {}: plan {} call(s) ({} expanded), listing {}",
            d.root, d.callee, d.planned, d.expanded, d.listed
        );
    }
    for w in &c.walks {
        for l in msvc6_il::inline::text(w).lines() {
            println!("    {l}");
        }
    }
    for n in &c.notes {
        println!("  NOTE {n}");
    }
}

#[derive(Debug, serde::Serialize)]
struct TuRow {
    tu: String,
    #[serde(flatten)]
    check: msvc6_il::inline::Check,
}
