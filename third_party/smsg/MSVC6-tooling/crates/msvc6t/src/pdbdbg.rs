use crate::Outcome;
use crate::pdbreloc::{Reference, file_name, read_image, reference, report_key};
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::Args;
use msvc6_base::srcio;
use msvc6_image::pe::Image;
use msvc6_pdb::dbghelp::{self, Row};
use msvc6_pdb::report::Report;
use msvc6_pdb::{dbgprobe, symbolize};
use msvc6_toolchain::confine::Root;
use msvc6_toolchain::layout::{Toolchain, win_join};
use std::path::{Path, PathBuf};
use std::time::Duration;

pub const PROBE_TIMEOUT: Duration = Duration::from_secs(600);
pub const MAX_EXPECT: u64 = 256 << 20;
pub const DEFAULT_SAMPLE: usize = 30;

#[derive(Debug, Args)]
pub struct Rows {
    #[arg(
        long,
        value_name = "FILE",
        conflicts_with = "expect",
        help = "The pairing report of the build the PDB was relocated from: the rows to query."
    )]
    pub report: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "An authored PDB's expect table: its functions, queried at their entry points."
    )]
    pub expect: Option<PathBuf>,
    #[arg(
        long,
        value_name = "KEY",
        help = "The report rows' extension object. Default: [image.pdb.relocate] report_key, else `msvc6t`."
    )]
    pub report_key: Option<String>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = DEFAULT_SAMPLE,
        help = "Query N functions spread over the image (own functions first), entry point and mid-body each; 0: every row, entry points only."
    )]
    pub sample: usize,
}

#[derive(Debug, Args)]
pub struct DbghelpArgs {
    #[arg(value_name = "IMAGE", help = "The [[image]] id of the shipped image.")]
    pub image: String,
    #[arg(
        value_name = "PDB",
        help = "The PDB to load: the only file on the symbol search path."
    )]
    pub pdb: PathBuf,
    #[command(flatten)]
    pub rows: Rows,
    #[arg(
        long = "copy",
        value_name = "FILE",
        help = "Load this image instead of the reference: our own link (a control), or a copy carrying a debug record."
    )]
    pub copy: Option<PathBuf>,
    #[arg(
        long,
        value_name = "ID",
        help = "The toolchain that compiles the probe. Default: the image's `compiler`, else the first [toolchain.*]."
    )]
    pub toolchain: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        help = "Where the probe is compiled. Default: a directory of its own under the system temp directory, removed after."
    )]
    pub work: Option<PathBuf>,
    #[arg(long, help = "A missing toolchain or runner is an error, not a SKIP.")]
    pub require: bool,
}

#[derive(Debug, Args)]
pub struct SymbolizeArgs {
    #[arg(value_name = "IMAGE", help = "The [[image]] id of the shipped image.")]
    pub image: String,
    #[arg(value_name = "PDB", help = "The PDB 7.0 the copy's RSDS record names.")]
    pub pdb: PathBuf,
    #[arg(
        long = "copy",
        value_name = "FILE",
        help = "A copy of the shipped image carrying an RSDS record (`pdb attach --twin --out`)."
    )]
    pub copy: PathBuf,
    #[command(flatten)]
    pub rows: Rows,
    #[arg(
        long,
        value_name = "DIR",
        help = "LLVM's bin directory, holding llvm-symbolizer. Default: $MSVC6T_LLVM, else PATH."
    )]
    pub tool: Option<PathBuf>,
    #[arg(long, help = "A missing llvm-symbolizer is an error, not a SKIP.")]
    pub require: bool,
}

fn rows_of(c: &Config, r: &Reference, a: &Rows) -> Result<Vec<Row>> {
    match (&a.report, &a.expect) {
        (Some(p), _) => {
            let key = report_key(c, r.n, a.report_key.as_deref());
            dbghelp::rows_of_report(&Report::read(p, key)?, &p.display().to_string())
        }
        (None, Some(e)) => dbghelp::rows_of_expect(
            &srcio::own_read_bounded(e, MAX_EXPECT)
                .with_context(|| format!("reading {}", e.display()))?,
            &e.display().to_string(),
        ),
        (None, None) => bail!("give --report (a relocated PDB) or --expect (an authored one)"),
    }
}

fn skip(what: &str, why: &str, require: bool) -> Result<Outcome> {
    if require {
        bail!("{what}: {why}");
    }
    println!("SKIP {what}: {why}");
    Ok(Outcome::Ok)
}

fn pdb_name(img: &Image, pdb: &Path) -> Result<String> {
    Ok(match img.codeview()? {
        Some(cv) if !cv.path.is_empty() => cv
            .path
            .rsplit(['\\', '/'])
            .next()
            .unwrap_or(&cv.path)
            .to_string(),
        _ => file_name(pdb),
    })
}

struct Staged {
    dir: PathBuf,
    win: String,
}

fn stage(drive_c: &Path, files: &[(&str, &Path)], pdb: (&str, &Path)) -> Result<Staged> {
    let root = Root::new(drive_c)?;
    let rel = PathBuf::from(format!("msvc6t-dbghelp-{}", std::process::id()));
    let (dir, _) = root.create_dirs(&rel.join("sym"))?;
    let dir = dir.parent().map(Path::to_path_buf).unwrap_or(dir);
    for (name, src) in files {
        let at = root.check(&rel.join(name), false)?;
        std::fs::copy(src, &at)
            .with_context(|| format!("copying {} into the prefix", src.display()))?;
    }
    let at = root.check(&rel.join("sym").join(pdb.0), false)?;
    std::fs::copy(pdb.1, &at)
        .with_context(|| format!("copying {} into the prefix", pdb.1.display()))?;
    Ok(Staged {
        dir,
        win: format!("C:\\{}", rel.display()),
    })
}

pub fn dbghelp_verb(
    config: Option<&Path>,
    a: &DbghelpArgs,
    guard: &dyn Fn() -> Result<()>,
) -> Result<Outcome> {
    let c = Config::require(config)?;
    let r = reference(&c, &a.image)?;
    let rows = rows_of(&c, &r, &a.rows)?;
    let image_path = a.copy.clone().unwrap_or_else(|| r.path.clone());
    let image = match &a.copy {
        Some(p) => read_image(p)?,
        None => Image::parse(r.bytes.clone())?,
    };
    let tcfg = c.toolchain_config();
    let id = a
        .toolchain
        .clone()
        .or_else(|| c.images.get(r.n).and_then(|d| d.compiler.clone()))
        .or_else(|| tcfg.toolchains.keys().next().cloned());
    let Some(id) = id else {
        return skip(
            "dbghelp",
            "no [toolchain.*] is configured to compile the probe",
            a.require,
        );
    };
    let tc = match Toolchain::named(&tcfg, &id) {
        Ok(t) => t,
        Err(e) => return skip("dbghelp", &format!("toolchain `{id}`: {e:#}"), a.require),
    };
    guard()?;
    let runner = crate::tc::runner(&tcfg)?;
    let sess = runner.session()?;
    let (work, made) = match &a.work {
        Some(w) => (w.clone(), false),
        None => (
            std::env::temp_dir().join(format!("msvc6t-dbghelp-work-{}", std::process::id())),
            true,
        ),
    };
    std::fs::create_dir_all(&work).with_context(|| format!("creating {}", work.display()))?;
    let interior = a.rows.sample > 0;
    let picked = dbghelp::picked_rows(&rows, a.rows.sample);
    let result = (|| -> Result<String> {
        srcio::own_write(work.join(dbgprobe::NAME), dbgprobe::SOURCE)?;
        let exe = work.join(dbgprobe::EXE);
        if exe.exists() {
            std::fs::remove_file(&exe)?;
        }
        let args: Vec<String> = [
            "/nologo",
            "/W3",
            "/O1",
            "/MT",
            dbgprobe::NAME,
            "/link",
            "/SUBSYSTEM:CONSOLE",
        ]
        .into_iter()
        .map(str::to_string)
        .collect();
        sess.run_tool(&tc, "CL.EXE", &args, Some(&work), &[])
            .context("compiling the dbghelp probe")?;
        if !exe.is_file() {
            bail!("the probe compiled but {} is not there", exe.display());
        }
        let queries = work.join("queries.txt");
        srcio::own_write(&queries, dbghelp::queries(&picked, interior))?;
        let img_name = file_name(&image_path);
        let pdb = pdb_name(&image, &a.pdb)?;
        let (dir, win, staged) = match sess.drive_c() {
            Some(dc) => {
                let s = stage(
                    &dc,
                    &[
                        (dbgprobe::EXE, &exe),
                        (&img_name, &image_path),
                        ("queries.txt", &queries),
                    ],
                    (&pdb, &a.pdb),
                )?;
                (s.dir.clone(), s.win.clone(), Some(s))
            }
            None => {
                let sym = work.join("sym");
                std::fs::create_dir_all(&sym)?;
                std::fs::copy(&a.pdb, sym.join(&pdb))?;
                std::fs::copy(&image_path, work.join(&img_name))?;
                (work.clone(), work.display().to_string(), None)
            }
        };
        let mut argv = vec![
            "-i".to_string(),
            win_join(&win, &img_name),
            "-p".to_string(),
            win_join(&win, "sym"),
            "-a".to_string(),
            win_join(&win, "queries.txt"),
        ];
        if interior {
            argv.push("-l".to_string());
        }
        let out = sess.run(
            &dir.join(dbgprobe::EXE),
            &argv,
            Some(&dir),
            &[],
            Some(PROBE_TIMEOUT),
        );
        if let Some(s) = &staged {
            std::fs::remove_dir_all(&s.dir).ok();
        }
        let out = out?;
        if out.timed_out {
            bail!("the dbghelp probe timed out");
        }
        Ok(srcio::tool_text(&out.stdout))
    })();
    if made {
        std::fs::remove_dir_all(&work).ok();
    }
    let text = result?;
    let field = |k: &str| dbghelp::module_field(&text, k);
    println!(
        "{}: {} against {}\n  dbghelp {} | SymType {} | PdbSig {} PdbSig70 {} age {} | PdbUnmatched {} | \
         LoadedPdbName {} | lines {} types {} publics {}",
        a.image,
        image_path.display(),
        a.pdb.display(),
        field("base"),
        field("SymType"),
        field("PdbSig"),
        field("PdbSig70"),
        field("PdbAge"),
        field("PdbUnmatched"),
        field("LoadedPdbName"),
        field("LineNumbers"),
        field("TypeInfo"),
        field("Publics"),
    );
    let answers = dbghelp::answers(&text);
    let expected = picked.len()
        + if interior {
            picked.iter().filter(|x| x.size > 1).count()
        } else {
            0
        };
    if answers.len() != expected {
        bail!(
            "the probe answered {} quer(ies) for {expected} -- its output is\n{}",
            answers.len(),
            text.trim()
        );
    }
    let mut notes = Vec::new();
    let t = dbghelp::score(&picked, &answers, interior, &mut notes);
    let seated = picked.iter().filter(|x| x.seat == dbghelp::SEATED).count();
    println!(
        "  {} row(s) queried ({seated} seated, {} library): named {} | at the reference address \
         with displacement 0 {} | name = the report's {} | not a function symbol {}\
         \n  size: = the report's {} | differs, still before the next function (the reference extent) \
         {} | RUNS PAST the next function {}",
        t.rows,
        t.rows - seated,
        t.named,
        t.at_start,
        t.name_matches,
        t.not_a_function,
        t.size_matches,
        t.size_within,
        t.size_overruns
    );
    if interior {
        println!(
            "  mid-body address inside the same function: {} of {}",
            t.interior_named, t.interior_rows
        );
    }
    println!(
        "  source lines: {} entry point(s) have one, {} in the report's own TU, {} within 0..3 of \
         the report's line (of {} comparable)\n  types: {} signature(s) agree with the mangled \
         name, {} differ, {} only the PDB has\n  frame locals: {} entry point(s) report at least one",
        t.with_line,
        t.file_matches,
        t.line_close,
        t.line_compared,
        t.sig_yes,
        t.sig_no,
        t.sig_only,
        t.with_locals
    );
    print_notes(&notes);
    Ok(Outcome::Ok)
}

fn print_notes(notes: &[String]) {
    for n in notes.iter().take(20) {
        println!("{n}");
    }
    if notes.len() > 20 {
        println!("  ... and {} more", notes.len() - 20);
    }
}

pub fn symbolize_verb(config: Option<&Path>, a: &SymbolizeArgs) -> Result<Outcome> {
    let Some(tool) = msvc6_pdb::llvm::find_named(a.tool.as_deref(), symbolize::TOOL) else {
        return skip(
            "llvm-symbolizer",
            "not found in --tool, $MSVC6T_LLVM or PATH: LLVM's reading of the PDB 7.0 is not checked",
            a.require,
        );
    };
    let c = Config::require(config)?;
    let r = reference(&c, &a.image)?;
    let rows = rows_of(&c, &r, &a.rows)?;
    let interior = a.rows.sample > 0;
    let picked = dbghelp::picked_rows(&rows, a.rows.sample);
    let copy = read_image(&a.copy)?;
    let Some(cv) = copy.codeview()? else {
        bail!(
            "{} carries no CodeView record: give the copy `pdb attach --twin --out` writes",
            a.copy.display()
        );
    };
    if cv.format != "RSDS" {
        bail!(
            "{} names its PDB by a {} record, which LLVM does not read: attach the 7.0 PDB to a copy first",
            a.copy.display(),
            cv.format
        );
    }
    let pdb_name = cv
        .path
        .rsplit(['\\', '/'])
        .next()
        .unwrap_or(&cv.path)
        .to_string();
    let dir = std::env::temp_dir().join(format!("msvc6t-symbolize-{}", std::process::id()));
    if dir.exists() {
        std::fs::remove_dir_all(&dir)?;
    }
    std::fs::create_dir_all(&dir)?;
    let img_name = file_name(&r.path);
    let run = (|| -> Result<Vec<symbolize::Answer>> {
        std::fs::copy(&a.copy, dir.join(&img_name))?;
        std::fs::copy(&a.pdb, dir.join(&pdb_name))
            .with_context(|| format!("copying {}", a.pdb.display()))?;
        symbolize::run(
            &tool,
            &dir,
            &img_name,
            &dbghelp::addresses(&picked, interior),
        )
    })();
    std::fs::remove_dir_all(&dir).ok();
    let ans = run?;
    let mut notes = Vec::new();
    let t = symbolize::score(&picked, &ans, interior, &mut notes);
    println!(
        "{}: llvm-symbolizer on {} (RSDS -> {pdb_name}) with {}\n  {} row(s) queried: named {} \
         | name = the expected one {} | mid-body inside the same function {} of \
         {}\n  source lines: {} entry point(s) have one, {} in the report's own TU, {} within 0..3 of the \
         report's line (of {} comparable)",
        a.image,
        a.copy.display(),
        a.pdb.display(),
        t.rows,
        t.named,
        t.same,
        t.inside,
        t.inside_rows,
        t.with_line,
        t.file_ok,
        t.line_close,
        t.compared
    );
    print_notes(&notes);
    Ok(Outcome::Ok)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_pdb_is_staged_under_the_name_the_image_s_record_gives() {
        let named = Image::parse(msvc6_fixture::sample::sample().pe.build()).unwrap();
        assert_eq!(
            pdb_name(&named, Path::new("out/relocated.pdb")).unwrap(),
            "sample.pdb"
        );
        let mut bare = msvc6_fixture::sample::sample();
        bare.pe.dirs[msvc6_fixture::pe::DIR_DEBUG] = (0, 0);
        let bare = Image::parse(bare.pe.build()).unwrap();
        assert!(bare.codeview().unwrap().is_none());
        assert_eq!(
            pdb_name(&bare, Path::new("out/relocated.pdb")).unwrap(),
            "relocated.pdb"
        );
    }
}
