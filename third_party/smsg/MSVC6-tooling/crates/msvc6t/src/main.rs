mod class;
mod cli;
mod cv;
mod diff;
mod eh;
mod gate;
mod growth;
mod hdr;
mod il;
mod image;
mod isoloop;
mod link;
mod locate;
mod mac;
mod macpair;
mod markdown;
mod model;
mod objlib;
mod pdb;
mod pdbaudit;
mod pdbauthor;
mod pdbdbg;
mod pdbrelease;
mod pdbreloc;
mod pdbsample;
mod place;
mod probe;
mod project;
mod publish;
mod sibling;
mod source;
mod tc;
mod tryverb;
mod vendored;

use anyhow::{Context as _, Result};
use clap::{CommandFactory as _, Parser as _};
use msvc6_lint::guard;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const TREE_KEY: &str = env!("MSVC6T_TREE_KEY");

const FMT_CHECKED: &str = env!("MSVC6T_FMT_CHECKED");

const BUILT_FROM: &str = env!("CARGO_MANIFEST_DIR");

const MARKER_CRATE: &str = "crates/msvc6-lint/Cargo.toml";

fn fmt_checked() -> usize {
    FMT_CHECKED.parse().unwrap_or(0)
}

fn is_toolkit(root: &Path) -> bool {
    root.join(MARKER_CRATE).is_file()
}

fn toolkit_root(given: Option<&Path>) -> Result<PathBuf> {
    if let Some(r) = given {
        let r = std::fs::canonicalize(r).with_context(|| format!("--root {}", r.display()))?;
        if !is_toolkit(&r) {
            anyhow::bail!("--root {}: no {MARKER_CRATE} there", r.display());
        }
        return Ok(r);
    }
    let here = std::env::current_dir()
        .ok()
        .and_then(|d| msvc6_lint::tree::workspace_root(&d))
        .filter(|r| is_toolkit(r));
    let built = msvc6_lint::tree::workspace_root(Path::new(BUILT_FROM)).filter(|r| is_toolkit(r));
    here.or(built).context(
        "no MSVC6Tooling tree holds the current directory or this binary's sources; pass --root",
    )
}

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

fn lint(a: &cli::LintArgs) -> Result<()> {
    let root = toolkit_root(a.root.as_deref())?;
    say(&guard::tree_checks(&root)?.lines);
    if a.provenance {
        let m = guard::provenance(&root, TREE_KEY, fmt_checked(), |tc| {
            guard::full_lint(&root, tc, false).map(|l| say(&l))
        })?;
        println!("provenance: {m}");
        return Ok(());
    }
    let tc = guard::pinned(&root)?;
    if a.fix {
        say(&guard::full_lint(&root, &tc, true)?);
        return Ok(());
    }
    let dir = guard::marker_dir(&root);
    let lint = || guard::full_lint(&root, &tc, false).map(|l| say(&l));
    let gated = if a.gate {
        guard::gate(&dir, &root, &tc.identity, lint)?
    } else {
        guard::lint_and_mark(&dir, &root, &tc.identity, lint)?
    };
    match gated {
        guard::Gated::Known(p) => println!(
            "gate: this tree is already marked clean by {}: {}",
            tc.channel,
            p.display()
        ),
        guard::Gated::Marked(p) => println!("gate: marked clean: {}", p.display()),
    }
    Ok(())
}

fn help(h: &cli::HelpArgs) -> Result<Outcome> {
    if h.markdown {
        let text = markdown::render(cli::Cli::command());
        match &h.out {
            Some(p) => {
                msvc6_base::srcio::own_write(p, &text)
                    .with_context(|| format!("writing {}", p.display()))?;
                eprintln!("wrote {}", p.display());
            }
            None => print!("{text}"),
        }
        return Ok(Outcome::Ok);
    }
    let mut cmd = cli::Cli::command();
    cmd.build();
    let mut cur = &mut cmd;
    for v in &h.verb {
        let Some(next) = cur.find_subcommand_mut(v) else {
            eprintln!("msvc6t: error: no verb `{v}`; `msvc6t help` lists them");
            return Ok(Outcome::Exit(ExitCode::from(USAGE_EXIT)));
        };
        cur = next;
    }
    cur.print_long_help()?;
    Ok(Outcome::Ok)
}

fn compiler_guard() -> Result<()> {
    let root = toolkit_root(None)?;
    let m = guard::provenance(&root, TREE_KEY, fmt_checked(), |tc| {
        guard::full_lint(&root, tc, false).map(|l| say(&l))
    })?;
    eprintln!("provenance: {m}");
    Ok(())
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Outcome {
    Ok,
    Differs,
    Exit(ExitCode),
}

pub const DIFFERS_EXIT: u8 = 3;

pub const USAGE_EXIT: u8 = 2;

pub const BROKEN_PIPE_EXIT: i32 = 141;

fn quiet_broken_pipe() {
    let default = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        let text = info
            .payload()
            .downcast_ref::<String>()
            .map(String::as_str)
            .or_else(|| info.payload().downcast_ref::<&str>().copied())
            .unwrap_or_default();
        if text.contains("failed printing to stdout") && text.contains("Broken pipe") {
            std::process::exit(BROKEN_PIPE_EXIT);
        }
        default(info);
    }));
}

fn run(cli: &cli::Cli) -> Result<Outcome> {
    let ok = |r: Result<()>| r.map(|()| Outcome::Ok);
    let config = || project::load(cli.config.as_deref());
    match &cli.cmd {
        cli::Cmd::Lint(a) => ok(lint(a)),
        cli::Cmd::Help(h) => help(h),
        cli::Cmd::Publish(a) => publish::run(a),
        cli::Cmd::Image(i) => {
            let c = config()?;
            let c = c.as_ref();
            match &i.verb {
                cli::ImageVerb::Info(a) => image::info(c, a),
                cli::ImageVerb::Rich(a) => image::rich_verb(c, a),
                cli::ImageVerb::Dump(a) => image::dump_verb(c, a),
                cli::ImageVerb::Imports(a) => image::imports(c, a),
                cli::ImageVerb::Exports(a) => image::exports(c, a),
                cli::ImageVerb::Cmp(a) => image::cmp_verb(c, a),
                cli::ImageVerb::Postlink(a) => image::postlink_verb(c, a),
                cli::ImageVerb::Res(a) => image::res_verb(c, a),
            }
        }
        cli::Cmd::Cv(v) => {
            let c = config()?;
            let c = c.as_ref();
            match &v.verb {
                cli::CvVerb::Decl(a) => cv::decl(c, a),
                cli::CvVerb::Diff(a) => cv::diff(c, a),
                cli::CvVerb::Apply(a) => cv::apply(c, a),
                cli::CvVerb::Names(a) => cv::names(c, a),
                cli::CvVerb::Check(a) => cv::check(c, a),
            }
        }
        cli::Cmd::Debuginfo(a) => cv::debuginfo(a),
        cli::Cmd::Pdb(p) => pdb::run(cli.config.as_deref(), p, &compiler_guard),
        cli::Cmd::Link(l) => link::run(cli.config.as_deref(), l),
        cli::Cmd::Class(c) => class::run(cli.config.as_deref(), c),
        cli::Cmd::Eh(e) => eh::run(cli.config.as_deref(), e),
        cli::Cmd::Sibling(s) => match &s.verb {
            sibling::SiblingVerb::Pair(a) => sibling::pair(cli.config.as_deref(), a),
            sibling::SiblingVerb::Functions(a) => sibling::functions(a),
            sibling::SiblingVerb::Diff(a) => sibling::diff_verb(a),
        },
        cli::Cmd::Mac(m) => match &m.verb {
            mac::MacVerb::Info(a) => mac::info(a),
            mac::MacVerb::Functions(a) => mac::functions(a),
            mac::MacVerb::Tracebacks(a) => mac::tracebacks(a),
            mac::MacVerb::Demangle(a) => mac::demangle(a),
            mac::MacVerb::Pair(a) => macpair::run(cli.config.as_deref(), a),
            mac::MacVerb::Types(a) => macpair::types(cli.config.as_deref(), a),
        },
        cli::Cmd::Vendored(v) => match &v.verb {
            vendored::VendoredVerb::Build(a) => {
                vendored::build(cli.config.as_deref(), a, &compiler_guard)
            }
            vendored::VendoredVerb::Locate(a) => vendored::locate(cli.config.as_deref(), a),
        },
        cli::Cmd::Closure(a) => vendored::closure_verb(cli.config.as_deref(), a),
        cli::Cmd::Obj(o) => match &o.verb {
            cli::ObjVerb::Diff(a) => objlib::obj_diff(a),
        },
        cli::Cmd::Lib(l) => match &l.verb {
            cli::LibVerb::Diff(a) => objlib::lib_diff(a),
            cli::LibVerb::Census(a) => objlib::lib_census(a),
            cli::LibVerb::Locate(a) => locate::lib_locate(cli.config.as_deref(), a),
        },
        cli::Cmd::Src(s) => {
            let code = source::run(s, cli.config.as_deref(), &compiler_guard)?;
            Ok(if code == 0 {
                Outcome::Ok
            } else {
                Outcome::Differs
            })
        }
        cli::Cmd::Tc(t) => {
            let c = cli.config.as_deref();
            match &t.cmd {
                cli::TcCmd::Info(a) => tc::info(c, a),
                cli::TcCmd::Run(a) => tc::run(c, a, &compiler_guard),
                cli::TcCmd::Reap(a) => tc::reap(c, a),
                cli::TcCmd::Implib(a) => tc::implib(c, a, &compiler_guard),
                cli::TcCmd::Clmap(a) => tc::clmap(a),
            }
            .map(Outcome::Exit)
        }
        cli::Cmd::Build(a) => {
            tc::build(cli.config.as_deref(), a, &compiler_guard).map(Outcome::Exit)
        }
        cli::Cmd::Crtguard(a) => tc::crtguard(a).map(Outcome::Exit),
        cli::Cmd::Probe(a) => probe::run(cli.config.as_deref(), a, &compiler_guard, &|| {
            toolkit_root(None)
        }),
        cli::Cmd::Cache(a) => tc::cache(cli.config.as_deref(), a).map(Outcome::Exit),
        cli::Cmd::Init(a) => project::init(a),
        cli::Cmd::Check(a) => project::check(cli.config.as_deref(), a),
        cli::Cmd::Dsp(d) => match &d.verb {
            cli::DspVerb::Show(a) => project::dsp_show(cli.config.as_deref(), a),
        },
        cli::Cmd::Functions(a) => project::functions(cli.config.as_deref(), a),
        cli::Cmd::Names(a) => project::names(cli.config.as_deref(), a),
        cli::Cmd::Compile(a) => diff::compile(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::Try(a) => tryverb::run(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::Segments(a) => diff::segments(cli.config.as_deref(), a),
        cli::Cmd::Segrank(a) => diff::segrank(cli.config.as_deref(), a),
        cli::Cmd::Relocid(a) => gate::relocid(cli.config.as_deref(), a),
        cli::Cmd::Calltargets(a) => gate::calltargets(cli.config.as_deref(), a),
        cli::Cmd::Addrsym(a) => gate::addrsym(cli.config.as_deref(), a),
        cli::Cmd::Account(a) => gate::account(cli.config.as_deref(), a),
        cli::Cmd::Literals(a) => gate::literals(cli.config.as_deref(), a),
        cli::Cmd::Jumps(a) => gate::jumps(cli.config.as_deref(), a),
        cli::Cmd::Operands(a) => gate::operands(cli.config.as_deref(), a),
        cli::Cmd::Globals(a) => gate::globals(cli.config.as_deref(), a),
        cli::Cmd::Calls(a) => gate::calls(cli.config.as_deref(), a),
        cli::Cmd::Data(d) => place::data(cli.config.as_deref(), d),
        cli::Cmd::Place(p) => place::place(cli.config.as_deref(), p),
        cli::Cmd::Placement(p) => growth::run(cli.config.as_deref(), p),
        cli::Cmd::Iso(a) => isoloop::iso(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::Loop(a) => isoloop::run_loop(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::C1(a) => model::c1(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::BssOrder(a) => model::bss_order(cli.config.as_deref(), a),
        cli::Cmd::FrameOrder(a) => model::frame_order(a),
        cli::Cmd::TuOrder(a) => model::tu_order(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::Hdr(a) => hdr::run(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::Floats(a) => hdr::floats(cli.config.as_deref(), a),
        cli::Cmd::TuFlags(a) => hdr::tu_flags(cli.config.as_deref(), a),
        cli::Cmd::TuBudget(a) => hdr::tu_budget(cli.config.as_deref(), a),
        cli::Cmd::Il(a) => il::run(cli.config.as_deref(), a, &compiler_guard),
        cli::Cmd::Inline(a) => il::inline(cli.config.as_deref(), a, &compiler_guard),
    }
}

fn main() -> ExitCode {
    quiet_broken_pipe();
    let cli = cli::Cli::parse();
    match run(&cli) {
        Ok(Outcome::Ok) => ExitCode::SUCCESS,
        Ok(Outcome::Differs) => ExitCode::from(DIFFERS_EXIT),
        Ok(Outcome::Exit(code)) => code,
        Err(e) => {
            eprintln!("msvc6t: error: {e:#}");
            ExitCode::from(1)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_build_script_embedded_a_key_and_checked_every_crate_root() {
        let root = msvc6_lint::tree::workspace_root(Path::new(BUILT_FROM)).unwrap();
        assert_eq!(TREE_KEY.len(), 64);
        assert!(TREE_KEY.bytes().all(|b| b.is_ascii_hexdigit()));
        assert_eq!(
            fmt_checked(),
            msvc6_lint::fmtcheck::workspace_roots(&root).len()
        );
    }

    #[test]
    fn the_command_line_definition_is_consistent() {
        cli::Cli::command().debug_assert();
    }

    #[test]
    fn one_loader_reads_every_table_of_a_project_config() {
        assert_eq!(
            msvc6_project::config::TABLES,
            [
                "schema",
                "toolchain",
                "runner",
                "cache",
                "image",
                "reccmp",
                "source"
            ]
        );
    }
}
