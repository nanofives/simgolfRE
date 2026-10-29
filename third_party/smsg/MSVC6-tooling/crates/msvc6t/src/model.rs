use crate::Outcome;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_project::config::Project;
use std::path::{Path, PathBuf};

#[derive(Debug, Args)]
pub struct C1Args {
    #[command(subcommand)]
    pub verb: C1Verb,
}

#[derive(Debug, Subcommand)]
pub enum C1Verb {
    #[command(
        about = "Price a header or a TU in C1's entity and label counter, from its text, with no compile",
        long_about = "C1 numbers declared entities and `$L` labels from one counter, so an edit ahead of a \
                      function shifts every later function's labels by the edit's cost, and a C2 tie that keys \
                      on a label number mod 64 re-rolls. This prices FILE from its text by the measured \
                      per-construct model (msvc6-model's c1count): a declarator 1, each parameter 1 (`(void)` 1, \
                      `()` 0), an enum 1 and each enumerator 1, a class definition 8 with its implicit members \
                      (a user constructor removes the implicit one, a destructor 4, a new virtual 3 once, each \
                      polymorphic base 2, a virtual base 3), a function definition its declarator plus its exit \
                      label (and `this`), its locals, `if` 1, `else` 1, a loop 3, `switch` 4 and each run of \
                      case labels 1, a goto label 2, each distinct string 1, a cast or `sizeof(type)` 1, `new` \
                      2, `delete` 1; /GX exception handling, templates and vector new/delete as measured. \
                      Directives are evaluated with no macro expansion; each file C1 first opens adds 1. The \
                      constructs the model does not price are listed UNMODELLED. With --with the edited file \
                      is priced too, and the shift (mod 64) printed. With --tu FILE is a translation unit: its \
                      quoted-include closure is walked in C1's first-open order (each file +1 on first open, a \
                      guarded re-include 0) with the counter at every open, then the shift of dropping each \
                      direct include (and each pair landing on --want), of moving one to the first include \
                      line, and of adding each header of the include directories the closure lacks, each with \
                      the TU's names only those files spell. The include directories are --include's, else the \
                      /I directories of FILE's compile spec in IMAGE's project. A text past the model's step or \
                      nesting bound is refused, never priced partly."
    )]
    Price(C1PriceArgs),
    #[command(
        about = "Hold the C1 counter model to CL's own labels, span by span, over whole TUs",
        long_about = "A shift is the sum of the costs of the text it moves past, so the model is exact for a TU \
                      exactly when it predicts the counter's advance over every stretch of the TU's text. CL \
                      preprocesses each TU (/P, its compile spec's switches and /I directories, INCLUDE as the \
                      image's build gives it); a probe function `void c1probe_N(int x) { if (x > 3) c1hook(1); \
                      c1hook(2); }` goes after every line that closes a file-scope declaration or function body \
                      of PROJECT text, and ahead of every `#line` that switches between project and system text; \
                      the probed text compiles once with /FAs, and each probe's `if` label is `$L<counter>`. The \
                      model walks the same text under the TU's /GX setting. A span holding system text is SYS \
                      (not claimed); every other span is ok or a MISS, its text written beside the compile as \
                      miss_<k>.txt. Project text is a `#line` file under a --project directory, else under the \
                      config's [source] root, else under the image's workspace. Exit 3 when a span misses."
    )]
    Check(C1CheckArgs),
}

#[derive(Debug, Args)]
pub struct C1CheckArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(long, value_name = "TEXT", help = "Only TUs whose stem contains TEXT.")]
    pub only: Option<String>,
    #[arg(long, help = "Print every span, not only the misses.")]
    pub all: bool,
    #[arg(
        long,
        value_name = "DIR",
        help = "A directory whose files are project text (repeatable)."
    )]
    pub project: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Compile under DIR. Default: <temp>/msvc6t-work/<image>/c1-check."
    )]
    pub work: Option<PathBuf>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Check N TUs at once. Default: min(8, cores / 2)."
    )]
    pub jobs: Option<usize>,
}

#[derive(Debug, Args)]
pub struct C1PriceArgs {
    #[arg(
        value_name = "FILE",
        help = "The header (or with --tu, the TU) to price."
    )]
    pub file: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        conflicts_with = "tu",
        help = "An edited copy of FILE: price it too and print the shift."
    )]
    pub with: Option<PathBuf>,
    #[arg(
        long,
        help = "Price without /GX: no exception-handling charges, the non-/GX vector iterators."
    )]
    pub no_gx: bool,
    #[arg(long, help = "FILE is a TU: walk its include closure.")]
    pub tu: bool,
    #[arg(
        long,
        value_name = "IMAGE",
        requires = "tu",
        help = "With --tu: the [[image]] whose project gives FILE's /I directories."
    )]
    pub image: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        requires = "tu",
        help = "With --tu: an include directory (repeatable), in place of the compile spec's."
    )]
    pub include: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "NAMES",
        requires = "tu",
        value_delimiter = ',',
        help = "With --tu: symbols the tie keys on, comma-separated: every row prints the shift ahead of each \
                and at the TU's end."
    )]
    pub key: Vec<String>,
    #[arg(
        long,
        value_name = "SHIFTS",
        requires = "tu",
        value_delimiter = ',',
        allow_hyphen_values = true,
        help = "With --tu: the shifts (mod 64) that close the tie; a row whose last key lands on one is WANT."
    )]
    pub want: Vec<i64>,
}

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

fn tu_includes(config: Option<&Path>, image: &str, file: &Path) -> Result<Vec<PathBuf>> {
    let p = Project::require(config)?;
    let (n, _) = msvc6_project::registry::resolve_id(&p, image)?;
    let specs = msvc6_project::spec::resolve(&p, n)?;
    let spec = msvc6_diff::compile::spec_for(&specs, image, file)
        .with_context(|| format!("{}: no TU of {image}'s project", file.display()))?;
    Ok(spec.include_paths.iter().map(PathBuf::from).collect())
}

fn c1_check(
    config: Option<&Path>,
    a: &C1CheckArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    let p = Project::require(config)?;
    let (n, d) = msvc6_project::registry::resolve_id(&p, &a.image)?;
    let specs = msvc6_project::spec::resolve(&p, n)?;
    let default_tc = d.compiler.clone().unwrap_or_else(|| "vc6".to_string());
    let include_env = d
        .build
        .as_ref()
        .map(|b| msvc6_diff::compile::build_include(&p.toolchain_config(), b))
        .transpose()?
        .map(|(inc, _)| inc);
    let mut units: Vec<msvc6_diff::compile::Unit> = Vec::new();
    for s in &specs.specs {
        if s.language != msvc6_project::spec::Language::Cpp
            || a.only.as_deref().is_some_and(|o| !s.stem.contains(o))
            || units.iter().any(|u| u.stem.eq_ignore_ascii_case(&s.stem))
        {
            continue;
        }
        let mut u = msvc6_diff::compile::Unit::of_spec(s, Path::new(&s.path), &default_tc)?;
        u.include_env.clone_from(&include_env);
        units.push(u);
    }
    units.sort_by(|x, y| x.source.file_name().cmp(&y.source.file_name()));
    let mut roots = a.project.clone();
    let from_source = roots.is_empty() && p.source.root.is_some();
    if roots.is_empty()
        && let Some(r) = &p.source.root
    {
        roots.push(p.path(r)?);
    }
    if roots.is_empty()
        && let Some(r) = &d.project
        && let Some(w) = r.dsw.as_ref().or(r.dsp.as_ref())
    {
        let key = format!("{}.project", p.image_key(n));
        if let Some(dir) = p.path_at(&key, w)?.parent() {
            roots.push(dir.to_path_buf());
        }
    }
    let excluded: Vec<PathBuf> = if from_source {
        p.source
            .exclude
            .iter()
            .filter_map(|e| roots.first().map(|r| r.join(e)))
            .collect()
    } else {
        Vec::new()
    };
    let inside = msvc6_model::c1check::project_test(&roots);
    let outside = msvc6_model::c1check::project_test(&excluded);
    let is_project = |f: &str| inside(f) && !outside(f);
    guard()?;
    let compiler = msvc6_diff::compile::Compiler::new(&p)?;
    let work = a.work.clone().unwrap_or_else(|| {
        std::env::temp_dir()
            .join("msvc6t-work")
            .join(&d.id)
            .join("c1-check")
    });
    let res = msvc6_diff::par::map(&units, crate::diff::jobs(a.jobs), |u| {
        msvc6_model::c1check::check_tu(&compiler, u, &work, &is_project, a.all)
    });
    let (mut spans, mut sys, mut misses, mut bad) = (0usize, 0usize, 0usize, 0usize);
    let mut failed = 0usize;
    for (u, r) in units.iter().zip(res) {
        match r {
            Ok(c) => {
                if c.misses > 0 || a.all {
                    say(&c.report);
                } else if let Some(first) = c.report.first() {
                    println!("{first}");
                }
                spans += c.spans;
                sys += c.sys;
                misses += c.misses;
                bad += usize::from(c.misses > 0);
            }
            Err(e) => {
                failed += 1;
                println!("{}: FAILED {e:#}", u.source.display());
            }
        }
    }
    println!(
        "{} TUs: {spans} project spans checked, {misses} MISS in {bad} TUs; {sys} system spans not claimed",
        units.len()
    );
    Ok(if failed > 0 {
        Outcome::Exit(std::process::ExitCode::from(1))
    } else if misses > 0 {
        Outcome::Differs
    } else {
        Outcome::Ok
    })
}

pub fn c1(config: Option<&Path>, a: &C1Args, guard: &dyn Fn() -> Result<()>) -> Result<Outcome> {
    match &a.verb {
        C1Verb::Check(c) => c1_check(config, c, guard),
        C1Verb::Price(p) => {
            if !p.tu {
                say(&msvc6_model::c1price::predict(
                    &p.file,
                    p.with.as_deref(),
                    !p.no_gx,
                )?);
                return Ok(Outcome::Ok);
            }
            let dirs = if p.include.is_empty() {
                let Some(image) = &p.image else {
                    bail!(
                        "--tu needs --image (its compile spec gives the /I directories) or --include"
                    );
                };
                tu_includes(config, image, &p.file)?
            } else {
                p.include.clone()
            };
            say(&msvc6_model::c1price::predict_tu(
                &p.file, &dirs, &p.key, &p.want,
            )?);
            let missing = msvc6_model::c1price::missing_includes(&p.file, &dirs)?;
            if !missing.is_empty() {
                eprintln!(
                    "NOTE: {} include(s) found in no include directory, each priced as an empty file (its \
                     open alone, +1); C1 stops at the first (C1083), so the price past it is of text CL never \
                     reads: {}",
                    missing.len(),
                    missing.join(", ")
                );
            }
            Ok(Outcome::Ok)
        }
    }
}

#[derive(Debug, Args)]
pub struct BssOrderArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id of the config (not needed with --names or --suggest)."
    )]
    pub image: Option<String>,
    #[arg(
        value_name = "OBJ",
        help = "Objects of the image, judged in this order."
    )]
    pub objs: Vec<PathBuf>,
    #[arg(
        long = "objs",
        value_name = "DIR",
        help = "A build's object directory (repeatable), its workspace's static libraries read beside it."
    )]
    pub dirs: Vec<PathBuf>,
    #[arg(long, help = "Read no workspace library beside an --objs directory.")]
    pub no_libs: bool,
    #[arg(
        long,
        value_name = "STEM",
        help = "Only this TU (object stem, .cpp optional): print its variable table."
    )]
    pub src: Option<String>,
    #[arg(
        long,
        value_name = "NAMES",
        help = "Just order these names (comma or space separated, plain or decorated); no image needed."
    )]
    pub names: Option<String>,
    #[arg(
        long,
        help = "Also print the variable table of every TU whose reference order differs."
    )]
    pub detail: bool,
    #[arg(
        long,
        help = "Every pair of TUs adjacent in the reference's .bss, judged as one original TU: walked buckets \
                that cross, or A appending before B's walked ones, refute the pair; a pair that passes prints \
                the odds of passing by chance."
    )]
    pub merge_screen: bool,
    #[arg(
        long,
        value_name = "NAME",
        requires = "window",
        help = "Respellings of NAME whose bucket is in --window: each word's plural or singular, its \
                --vocabulary group's words, one qualifier before or after; or with `[a,b,]` slots, the \
                spellings of that pattern. Fewest changed words first. Proposals only."
    )]
    pub suggest: Option<String>,
    #[arg(
        long,
        value_name = "LO..HI",
        help = "The bucket window for --suggest, inclusive."
    )]
    pub window: Option<String>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 60,
        help = "At most N --suggest candidates."
    )]
    pub limit: usize,
    #[arg(
        long,
        value_name = "FILE",
        help = "The words --suggest, --plan-suggest and --synonyms respell with: lines `group WORD WORD...` \
                (words that stand for one another) and `qualifiers WORD...` (a word a name may gain before \
                its first word or after its last). Without it a word only gains or loses a plural `s`."
    )]
    pub vocabulary: Option<PathBuf>,
    #[arg(
        long,
        value_name = "K",
        requires = "src",
        help = "A rename plan for the --src TU: the fewest renames that put the reference's order in bucket \
                order with every renamed variable its own window at least K buckets wide (disjoint, in \
                image order). Prints the cost against k first."
    )]
    pub plan: Option<u32>,
    #[arg(
        long,
        value_name = "FILE",
        requires = "src",
        help = "A whole-TU name assignment for the --src TU from a candidates file: one line per variable, \
                `IDENT TOKEN...` (`#` starts a comment; a TOKEN is a name or a `[a,b,]` slot pattern, \
                optionally tiered `N:PATTERN`, or `-NAME` to exclude a spelling, `-IDENT` refusing the \
                current name); a line `- NAME...` lists names the program already has; a family line \
                `=LABEL TIER IDENT=PATTERN...` names variables chosen together, the n-th `{a,b}` slot of \
                every member taking the same choice. Per variable: its name, a candidate, or UNPLACED, so \
                the reference order is bucket order: fewest unplaced, then fewest renames, then the cheapest."
    )]
    pub assign: Option<PathBuf>,
    #[arg(
        long,
        value_name = "N",
        requires = "assign",
        default_value_t = 0,
        help = "With --assign: also offer --suggest's respellings up to cost N."
    )]
    pub synonyms: usize,
    #[arg(
        long,
        value_name = "N",
        requires = "assign",
        default_value_t = msvc6_model::bss::RENAME_COST,
        help = "With --assign: the price of one rename against the tiers."
    )]
    pub rename_cost: u64,
    #[arg(
        long,
        value_name = "NAMES",
        help = "With --plan or --assign: keep these variables whatever it costs (comma or space separated)."
    )]
    pub pin: Option<String>,
    #[arg(
        long,
        value_name = "N",
        requires = "plan",
        default_value_t = 0,
        help = "With --plan: per rename, N --suggest respellings that land in its window."
    )]
    pub plan_suggest: usize,
}

fn vocabulary(p: Option<&Path>) -> Result<msvc6_model::bsssuggest::Vocabulary> {
    p.map_or_else(
        || Ok(msvc6_model::bsssuggest::Vocabulary::default()),
        msvc6_model::bsssuggest::Vocabulary::read,
    )
}

pub fn bss_order(config: Option<&Path>, a: &BssOrderArgs) -> Result<Outcome> {
    if let Some(n) = &a.names {
        say(&msvc6_model::bss::names_table(n));
        return Ok(Outcome::Ok);
    }
    let voc = vocabulary(a.vocabulary.as_deref())?;
    if let (Some(s), Some(w)) = (&a.suggest, &a.window) {
        say(&msvc6_model::bsssuggest::print(s, w, a.limit, &voc)?);
        return Ok(Outcome::Ok);
    }
    let image = a
        .image
        .as_deref()
        .context("bss-order needs IMAGE (or --names, or --suggest with --window)")?;
    let (p, n, env) = crate::diff::env_of(config, image)?;
    let specs = msvc6_project::spec::resolve_with(
        &p,
        n,
        &msvc6_diff::objs::library_projects(&a.dirs, !a.no_libs),
    )?;
    let mut objs = if a.dirs.is_empty() {
        Vec::new()
    } else {
        msvc6_diff::objs::build_list(&a.dirs, !a.no_libs)?
    };
    objs.extend(a.objs.iter().cloned());
    if objs.is_empty() {
        bail!("no objects: name them, or pass --objs DIR");
    }
    let plan = if a.plan.is_some() || a.assign.is_some() {
        let assign = a
            .assign
            .as_ref()
            .map(|f| msvc6_base::srcio::read(f).with_context(|| format!("reading {}", f.display())))
            .transpose()?;
        Some(msvc6_model::bssplan::PlanOpts {
            k: a.plan.unwrap_or(1),
            assign,
            synonyms: a.synonyms,
            rename_cost: a.rename_cost,
            pins: a
                .pin
                .as_deref()
                .unwrap_or("")
                .split([',', ' '])
                .filter(|s| !s.is_empty())
                .map(str::to_string)
                .collect(),
            suggest: a.plan_suggest,
            vocabulary: voc,
        })
    } else {
        None
    };
    let source_of = |stem: &str| -> Option<PathBuf> {
        specs
            .specs
            .iter()
            .find(|s| s.stem.eq_ignore_ascii_case(stem))
            .map(|s| PathBuf::from(&s.path))
    };
    let run = msvc6_model::bssrun::run(
        &env.label,
        &env,
        &objs,
        &msvc6_model::bssrun::Opts {
            src: a.src.clone(),
            detail: a.detail,
            plan,
            merge_screen: a.merge_screen,
        },
        &source_of,
    )?;
    for w in &run.warnings {
        eprintln!("bss-order: {w}");
    }
    say(&run.lines);
    if run.refused.is_empty() {
        return Ok(Outcome::Ok);
    }
    for (stem, why) in &run.refused {
        println!("REFUSED {stem}: {why}");
    }
    println!(
        "bss-order: {} TU(s) REFUSED, not judged and not in the totals",
        run.refused.len()
    );
    Ok(Outcome::Exit(std::process::ExitCode::from(1)))
}

#[derive(Debug, Args)]
pub struct FrameOrderArgs {
    #[arg(
        required = true,
        value_name = "PATH",
        help = "Objects, archives, or directories of them (searched recursively; a link below a directory is not followed)."
    )]
    pub paths: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "MODEL",
        value_parser = ["hash", "decl"],
        help = "Hold every function to this model instead of reading /ZI off each object."
    )]
    pub model: Option<String>,
    #[arg(long, help = "Print every function, not only the misfits.")]
    pub verbose: bool,
}

pub fn frame_order(a: &FrameOrderArgs) -> Result<Outcome> {
    let inputs = msvc6_model::frameorder::collect(&a.paths)?;
    for s in &inputs.skipped {
        eprintln!("frame-order: {s}");
    }
    let model = match a.model.as_deref() {
        Some("hash") => Some(msvc6_model::frameorder::Model::Hash),
        Some("decl") => Some(msvc6_model::frameorder::Model::Decl),
        _ => None,
    };
    let (lines, t) = msvc6_model::frameorder::report(&inputs.objects, model, a.verbose);
    say(&lines);
    if t.unreadable > 0 {
        eprintln!(
            "frame-order: {} input(s) are no COFF object this reader parses",
            t.unreadable
        );
    }
    Ok(Outcome::Ok)
}

#[derive(Debug, Args)]
#[allow(clippy::struct_excessive_bools)]
pub struct TuOrderArgs {
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
        value_name = "STEM",
        help = "List this object's members, each deferred one with its drain point (repeatable)."
    )]
    pub show: Vec<String>,
    #[arg(
        long,
        help = "A summary line for every object, not only those with ORDER."
    )]
    pub all: bool,
    #[arg(
        long,
        help = "In --show listings, also each COMDAT the object emits that is not its own member: a SELECT_ANY \
                copy an earlier TU's keeps (DUP<tu), another name at an ICF-folded rva (ICF)."
    )]
    pub dups: bool,
    #[arg(
        long,
        help = "List every ORDER body's ILINK verdict: on an incremental reference a FOSSIL (a relink placed \
                it), else OWED."
    )]
    pub ilink: bool,
    #[arg(
        long,
        value_name = "FILE",
        help = "A library table (`lib locate --write`'s, or one in its format): the reference's archive member \
                sequence the pull-order and drain fossil rules read."
    )]
    pub library: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "An archive the image links (repeatable): its members' definitions for those rules. Default: \
                the --library table's [[archive]] paths."
    )]
    pub archive: Vec<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Also write one row per object and a TOTAL row to this TSV."
    )]
    pub out: Option<PathBuf>,
    #[arg(
        long,
        help = "Predict each object's COMDAT order from C1XX's IL offsets: its TU compiles pch-create with /Bk, \
                each .gl function record gives its IL offset (into the pch for a header body, .ex for a .cpp \
                one), and the merge by offset, and the merge with demand (C2 compiles the first NEEDED and not \
                done function of the offset-sorted list), are scored against the object; each ORDER header \
                body gets the pch shift that best restores the reference's order. Objects: --show's, else \
                every object with ORDER; with --all every object."
    )]
    pub predict: bool,
    #[arg(
        long,
        requires = "predict",
        help = "With --predict: list each object's IL table."
    )]
    pub il: bool,
    #[arg(
        long,
        requires = "predict",
        help = "With --predict: compile as MSDEV does, every TU of the image's project in batch order (one \
                batch per per-file option set, in .dsp order of first appearance) in one directory sharing \
                one pch, so /YX uses an earlier TU's pch wherever it would."
    )]
    pub pch_use: bool,
    #[arg(
        long,
        requires = "predict",
        help = "With --predict: also compile each TU with /Ob0 and score the object as a subsequence of that \
                order, and the hybrid merge (the real IL offsets, the /Ob0 object's references)."
    )]
    pub ob0: bool,
    #[arg(
        long,
        value_name = "DIR",
        requires = "predict",
        help = "With --predict: compile under DIR. Default: <temp>/msvc6t-work/<image>/tu-order."
    )]
    pub work: Option<PathBuf>,
    #[arg(
        short = 'j',
        long,
        value_name = "N",
        help = "Compile and sweep N at once. Default: min(8, cores / 2)."
    )]
    pub jobs: Option<usize>,
}

struct ImageProject {
    specs: msvc6_project::spec::Specs,
    top: String,
    projects: Vec<String>,
}

fn image_project(p: &Project, n: usize) -> Result<ImageProject> {
    let specs = msvc6_project::spec::resolve(p, n)?;
    let d = p.images.get(n).context("no such image")?;
    let mut projects: Vec<String> = Vec::new();
    let mut top = d.id.clone();
    if let Some(r) = &d.project {
        top = msvc6_project::spec::top_of(&r.config).to_string();
        if let Some(w) = &r.dsw {
            let key = format!("{}.project.dsw", p.image_key(n));
            let path = p.path_at(&key, w)?;
            let dsw = msvc6_project::dsw::Dsw::read(&path)?;
            projects = dsw.projects.iter().map(|x| x.name.clone()).collect();
        }
    }
    Ok(ImageProject {
        specs,
        top,
        projects,
    })
}

fn is_cpp(s: &msvc6_project::spec::CompileSpec) -> bool {
    s.source
        .rsplit_once('.')
        .is_some_and(|(_, e)| e.eq_ignore_ascii_case("cpp"))
}

impl ImageProject {
    fn own_order(&self) -> Vec<String> {
        self.specs
            .specs
            .iter()
            .filter(|s| s.project.eq_ignore_ascii_case(&self.top) && is_cpp(s))
            .map(|s| s.stem.clone())
            .collect()
    }

    fn link_order(&self) -> Vec<String> {
        let mut order = self.own_order();
        for proj in &self.projects {
            if proj.eq_ignore_ascii_case(&self.top) {
                continue;
            }
            for s in self
                .specs
                .specs
                .iter()
                .filter(|s| s.project.eq_ignore_ascii_case(proj) && is_cpp(s))
            {
                if !order.iter().any(|o| o.eq_ignore_ascii_case(&s.stem)) {
                    order.push(s.stem.clone());
                }
            }
        }
        order
    }
}

fn archives_of(
    a: &TuOrderArgs,
    table: &msvc6_match::table::Table,
) -> Vec<msvc6_model::tuorder::Archived> {
    let named = |p: &Path| msvc6_model::tuorder::Archived {
        name: p
            .file_name()
            .map_or_else(String::new, |n| n.to_string_lossy().to_ascii_lowercase()),
        path: p.to_path_buf(),
    };
    if a.archive.is_empty() {
        table
            .archive
            .iter()
            .map(|r| named(Path::new(&r.path)))
            .collect()
    } else {
        a.archive.iter().map(|p| named(p)).collect()
    }
}

#[allow(clippy::too_many_lines)]
pub fn tu_order(
    config: Option<&Path>,
    a: &TuOrderArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    let (p, n, env) = crate::diff::env_of(config, &a.image)?;
    let ip = image_project(&p, n)?;
    let objs = msvc6_diff::objs::build_list(&a.objs, !a.no_libs)?;
    if objs.is_empty() {
        bail!("no objects under {:?}", a.objs);
    }
    let own_order = ip.own_order();
    let link_order = ip.link_order();
    let seated = msvc6_model::tuorder::seat_dir(&env, &objs, &own_order);
    let incremental = msvc6_model::tuorder::reference_is_incremental(&env);
    let table = a
        .library
        .as_deref()
        .map(msvc6_match::table::Table::read)
        .transpose()?;
    let evidence = |objects: &[(String, Vec<msvc6_model::tuorder::Member>)],
                    paths: &[PathBuf]|
     -> Result<msvc6_model::tuorder::LinkEvidence> {
        let Some(t) = &table else {
            bail!("no --library table names the reference's archive members");
        };
        msvc6_model::tuorder::link_evidence(&env, objects, paths, &t.recorded(), &archives_of(a, t))
    };
    let opts = msvc6_model::tuorder::Opts {
        all: a.all,
        dups: a.dups,
        ilink: a.ilink,
        show: a.show.clone(),
        tsv: a.out.is_some(),
    };
    let mut ordered =
        msvc6_model::tuorder::order_objects(&seated, &link_order, incremental, &evidence, &opts)?;
    say(&ordered.lines);
    if let Some(path) = &a.out {
        let at = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_or(0, |d| d.as_secs());
        ordered.tsv.push_str(&format!(
            "# written by msvc6t tu-order {} objs={} at={at}\n",
            env.label,
            a.objs
                .iter()
                .map(|d| d.display().to_string())
                .collect::<Vec<_>>()
                .join(",")
        ));
        msvc6_base::srcio::own_write(path, &ordered.tsv)
            .with_context(|| format!("writing {}", path.display()))?;
        println!(
            "  wrote {} object row(s) and TOTAL to {}",
            ordered.objects.len(),
            path.display()
        );
    }
    if !a.predict {
        return Ok(Outcome::Ok);
    }
    guard()?;
    let d = p.images.get(n).context("no such image")?;
    let default_tc = d.compiler.clone().unwrap_or_else(|| "vc6".to_string());
    let include_env = d
        .build
        .as_ref()
        .map(|b| msvc6_diff::compile::build_include(&p.toolchain_config(), b))
        .transpose()?
        .map(|(inc, _)| inc);
    let mut units = std::collections::BTreeMap::new();
    for s in &ip.specs.specs {
        let mut u = msvc6_diff::compile::Unit::of_spec(s, Path::new(&s.path), &default_tc)?;
        u.include_env.clone_from(&include_env);
        units.entry(s.stem.to_ascii_lowercase()).or_insert(u);
    }
    let mut batches = std::collections::BTreeMap::new();
    if let Some(dsp) = ip
        .specs
        .specs
        .iter()
        .find(|s| s.project.eq_ignore_ascii_case(&ip.top))
        .and_then(|s| s.dsp.clone())
    {
        let parsed = msvc6_project::dsp::Dsp::read(Path::new(&dsp))?;
        let cfg = ip
            .specs
            .specs
            .iter()
            .find(|s| s.project.eq_ignore_ascii_case(&ip.top))
            .and_then(|s| s.config.clone())
            .unwrap_or_default();
        for f in parsed.settings(&cfg).files {
            batches.insert(f.stem(), f.add.join(" "));
        }
    }
    let compiler = msvc6_diff::compile::Compiler::new(&p)?;
    let work = a.work.clone().unwrap_or_else(|| {
        std::env::temp_dir()
            .join("msvc6t-work")
            .join(&env.label)
            .join("tu-order")
    });
    let driver = msvc6_model::bk::Driver {
        compiler: &compiler,
        units,
        order: own_order,
        batches,
        work,
        jobs: crate::diff::jobs(a.jobs),
    };
    let (lines, _) = msvc6_model::tupredict::predict(
        &driver,
        &ordered,
        &msvc6_model::tupredict::Opts {
            il_table: a.il,
            all: a.all,
            pch_use: a.pch_use,
            ob0: a.ob0,
            show: a.show.clone(),
        },
    )?;
    say(&lines);
    Ok(Outcome::Ok)
}
