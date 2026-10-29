use crate::Outcome;
use crate::cli::JsonArg;
use crate::image::json;
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use msvc6_base::units::Rva;
use msvc6_class::{ehcensus, ehladder, funclets, insns, listing, read};
use msvc6_image::pe::Image;
use msvc6_project::config::FunctionSource;
use msvc6_project::names::Kind;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

fn parse_rva(s: &str) -> Result<u32, String> {
    let t = s.trim();
    t.strip_prefix("0x")
        .or_else(|| t.strip_prefix("0X"))
        .and_then(|h| u32::from_str_radix(h, 16).ok())
        .ok_or_else(|| format!("`{s}` is not a 0x-hexadecimal rva"))
}

#[derive(Debug, Args)]
pub struct EhArgs {
    #[command(subcommand)]
    pub verb: EhVerb,
}

#[derive(Debug, Subcommand)]
pub enum EhVerb {
    #[command(
        about = "Every `.text$x' unwind funclet our objects emit, sized and named by the function it belongs to",
        long_about = "/GX emits a function's unwind funclets in `.text$x' sections, each an associative COMDAT of \
                      the function's own: no gate that compares function bodies reads a funclet byte. Read every \
                      `.obj' of OBJDIR (in path order) and list each object's funclet bytes, and every funclet, \
                      largest first, under the COMDAT its group belongs to (a `.text$x' in no readable group is \
                      `<no readable COMDAT group>'). A row is a candidate, never a defect: our objects say what \
                      we emit and nothing about what the original had. With [image.eh] `textx_ceiling' (the \
                      reference's `.text$x' bytes at most, derived elsewhere, with its `ceiling_evidence'), the \
                      surplus is our total less it. --archive FILE lists the `.text$x' bytes each member of an \
                      archive carries (--member NAME, repeatable, keeps those named; default every member). An \
                      object that cannot be read is named on stderr and the run exits 1. JSON rows: one per \
                      funclet."
    )]
    Census(CensusArgs),
    #[command(
        about = "The `.text$x' funclet groups of the reference and of one of our links, aligned function by function",
        long_about = "LINK lays each /GX function's `.text$x' contribution down 16-aligned and 0xcc-padded, and CL \
                      ends every contribution with the function's handler stub, `mov eax, offset FuncInfo; jmp \
                      ___CxxFrameHandler'. So a `.text$x' region cuts into one GROUP per function with no symbol: \
                      decode from each group's first non-fill byte to a near `jmp' right after a `mov eax, \
                      imm32'; a `jmp' or `ret' elsewhere ends one funclet. A group's bytes, every rel32, 32-bit \
                      immediate and absolute displacement zeroed, are its funclets' frame offsets and shapes, \
                      identical across two links wherever the source agrees. A group's OWNER is the function \
                      whose prologue pushes its stub (`6a ff 68 <stub va>'): ours named from --map, the \
                      reference's from its ghidra-json function records (else its function index). Ours runs over \
                      the map's `.text$x' contribution; the reference's from --ref-at, else the lowest record \
                      named `Unwind@...', to the end of `.text'. The two group lists are aligned by LCS over the \
                      masked bytes (at most 2^26 cells) and only the disagreements print: REF ONLY, OURS ONLY, or \
                      MOVED (an identical group sits unpaired on the other side). A tail no stub ends is UNCUT. \
                      JSON rows: one per disagreement."
    )]
    Funclets(FuncletsArgs),
    #[command(
        about = "The per-exit destructor tier ladder of one /GX function, ours against the image",
        long_about = "A /GX body that destroys a large local at several exits gets one copy of the destructor per \
                      exit, and CL's /Ob1 budget decides at each copy how deep the nested destructor chain \
                      expands: an expanded `??_DX' shows as calls to what it destroys, one left out of line as \
                      `call ??_DX' itself. Cut our function (the CL listing COD, its object beside it or --obj, \
                      --symbol when the listing holds several) and the image's body at RVA into exits, each opened \
                      by a call matching an --exit pattern and labelled with the EH state stored last before it \
                      (`mov __$EHRec$[esp+N], k'), and list each exit's destructor calls (`??1', `??_D', `??_G') \
                      side by side, paired by (state, occurrence). The image has no names: both streams are \
                      decoded and aligned with esp displacements and addresses masked; an aligned call names the \
                      image's target (a candidate our object also defines is settled by its bytes, relocated \
                      fields masked), an image-only callee is `[name]' from the image's names, and an image state \
                      store is a `mov [esp+d], imm' at a displacement an aligned store of ours used. The image \
                      body runs to the next function the index starts, else our length plus an eighth cut at the \
                      first `ret' past our length. --all-inlines adds every callee whose count differs, split at \
                      the first exit. JSON rows: one per exit."
    )]
    Ladder(LadderArgs),
}

#[derive(Debug, Args)]
pub struct CensusArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "OBJDIR", help = "The directory of a build's objects.")]
    pub objdir: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "Also list what this archive's members carry."
    )]
    pub archive: Option<PathBuf>,
    #[arg(
        long,
        value_name = "NAME",
        help = "With --archive: a member to list (repeatable)."
    )]
    pub member: Vec<String>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct FuncletsArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(long, value_name = "FILE", help = "Our linked image.")]
    pub candidate: PathBuf,
    #[arg(long, value_name = "FILE", help = "That link's /MAP.")]
    pub map: PathBuf,
    #[arg(long, value_name = "RVA", value_parser = parse_rva, help = "Where the reference's `.text$x' starts.")]
    pub ref_at: Option<u32>,
    #[command(flatten)]
    pub json: JsonArg,
}

#[derive(Debug, Args)]
pub struct LadderArgs {
    #[arg(value_name = "IMAGE", help = "An [[image]] id of the config.")]
    pub image: String,
    #[arg(value_name = "RVA", value_parser = parse_rva, help = "The image function's rva.")]
    pub rva: u32,
    #[arg(value_name = "COD", help = "The CL listing (/FAcs) of our compile.")]
    pub cod: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "The object of that compile (default: COD with .obj)."
    )]
    pub obj: Option<PathBuf>,
    #[arg(
        long,
        value_name = "NAME",
        help = "The decorated function in the listing."
    )]
    pub symbol: Option<String>,
    #[arg(
        long = "exit",
        value_name = "PATTERN",
        required = true,
        help = "A substring of the call that opens an exit (repeatable)."
    )]
    pub exits: Vec<String>,
    #[arg(long, help = "Also list every callee whose count differs.")]
    pub all_inlines: bool,
    #[command(flatten)]
    pub json: JsonArg,
}

pub fn run(config: Option<&Path>, a: &EhArgs) -> Result<Outcome> {
    match &a.verb {
        EhVerb::Census(v) => census(config, v),
        EhVerb::Funclets(v) => funclets_verb(config, v),
        EhVerb::Ladder(v) => ladder(config, v),
    }
}

struct Loaded {
    p: Config,
    n: usize,
    id: String,
    r: msvc6_project::registry::Resolved,
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

fn say(lines: &[String]) {
    for l in lines {
        println!("{l}");
    }
}

fn census(config: Option<&Path>, a: &CensusArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let objs = read::files(&a.objdir, &["obj"])?;
    let mut per = Vec::new();
    let mut rows = Vec::new();
    let mut refused = 0usize;
    for p in &objs {
        let stem = p
            .file_stem()
            .map_or_else(String::new, |s| s.to_string_lossy().into_owned());
        match read::bytes(p, read::MAX_OBJECT).and_then(|d| ehcensus::object(&stem, &d)) {
            Ok(f) => {
                per.push((stem, f.iter().map(|x| x.len).sum()));
                rows.extend(f);
            }
            Err(e) => {
                refused += 1;
                eprintln!("eh census: REFUSED {}: {e:#}", p.display());
            }
        }
    }
    let ceiling =
        l.p.images
            .get(l.n)
            .and_then(|d| d.eh.as_ref())
            .and_then(|e| {
                e.textx_ceiling.map(|bytes| ehcensus::Ceiling {
                    bytes,
                    evidence: e.ceiling_evidence.clone(),
                })
            });
    say(&ehcensus::report(
        &l.id,
        &a.objdir.display().to_string(),
        objs.len(),
        &per,
        &rows,
        ceiling.as_ref(),
    ));
    if let Some(ar) = &a.archive {
        let data = read::bytes(ar, read::MAX_ARCHIVE)?;
        let (walked, carried) =
            ehcensus::archive(&data, &a.member).with_context(|| ar.display().to_string())?;
        say(&ehcensus::archive_report(
            &ar.display().to_string(),
            walked,
            &a.member,
            &carried,
        ));
    }
    json(&a.json, "eh census", &l.id, ceiling.as_ref(), &rows)?;
    if refused > 0 {
        return Ok(Outcome::Exit(std::process::ExitCode::from(1)));
    }
    Ok(Outcome::Ok)
}

fn reference_names(l: &Loaded) -> Result<BTreeMap<u32, String>> {
    let d =
        l.p.images
            .get(l.n)
            .with_context(|| format!("no image[{}]", l.n))?;
    let key = msvc6_project::spans::join(&l.p.image_key(l.n), "functions");
    for (i, s) in d.functions.iter().enumerate() {
        if let FunctionSource::GhidraJson(f) = s {
            let path = l.p.path_at(&msvc6_project::spans::index(&key, i), f)?;
            let text = msvc6_base::srcio::tool_text(&read::bytes(&path, read::MAX_TEXT)?);
            let base = l.img.opt.image_base.get();
            return Ok(insns::from_ghidra(&text, base)
                .with_context(|| path.display().to_string())?
                .into_iter()
                .filter_map(|f| Some((f.entry, f.name?)))
                .collect());
        }
    }
    let ix = msvc6_project::functions::index_of(&l.p, l.n, &l.r)?;
    Ok(ix
        .iter()
        .filter_map(|f| Some((f.rva, f.name.clone()?)))
        .collect())
}

fn funclets_verb(config: Option<&Path>, a: &FuncletsArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let ours = Image::parse(read::bytes(&a.candidate, read::MAX_IMAGE)?)
        .with_context(|| a.candidate.display().to_string())?;
    let (onames, textx) = funclets::read_map(
        &read::text(&a.map, read::MAX_TEXT)?,
        ours.opt.image_base.get(),
    );
    let Some((o_off, o_len)) = textx else {
        bail!("{}: no `.text$x' line in the map", a.map.display());
    };
    let otext = ours
        .section(".text")
        .with_context(|| format!("{}: no .text", a.candidate.display()))?;
    let o_start = otext.rva.get().wrapping_add(o_off);
    let rnames = reference_names(&l)?;
    let first_unwind = rnames
        .iter()
        .filter(|(_, n)| n.starts_with("Unwind@"))
        .map(|(r, _)| *r)
        .min();
    let Some(r_start) = a.ref_at.or(first_unwind) else {
        bail!(
            "no `Unwind@' function in the reference's records to start its region at; pass --ref-at"
        );
    };
    let rtext = l
        .img
        .section(".text")
        .context("the reference has no .text")?;
    let r_end = rtext.rva.get().saturating_add(rtext.virt_size.get());
    let r_len = r_end.saturating_sub(r_start);
    let rbytes = l
        .img
        .at_rva(Rva(r_start), usize::try_from(r_len)?)
        .with_context(|| format!("reference: {r_start:#x}+{r_len:#x} is not on disk"))?;
    let obytes = ours
        .at_rva(Rva(o_start), usize::try_from(o_len)?)
        .with_context(|| format!("ours: {o_start:#x}+{o_len:#x} is not on disk"))?;
    let (rbase, obase) = (l.img.opt.image_base.get(), ours.opt.image_base.get());
    let rg = funclets::cut(rbytes, r_start, rbase);
    let og = funclets::cut(obytes, o_start, obase);
    let owners = |img: &Image, gs: &[funclets::Group], base: u32, names: &BTreeMap<u32, String>| {
        let Some(t) = img.section(".text") else {
            return Vec::new();
        };
        let stubs: Vec<u32> = gs.iter().map(|g| g.stub).collect();
        let push = funclets::pushers(img.section_bytes(t), t.rva.get(), base, &stubs);
        gs.iter()
            .map(|g| funclets::owner(names, push.get(&g.stub).copied()))
            .collect::<Vec<_>>()
    };
    let reference = funclets::Side {
        start: r_start,
        len: r_len,
        owners: owners(&l.img, &rg, rbase, &rnames),
        groups: rg,
    };
    let side = funclets::Side {
        start: o_start,
        len: o_len,
        owners: owners(&ours, &og, obase, &onames),
        groups: og,
    };
    let r = funclets::compare(reference, side)?;
    say(&funclets::report(
        &r,
        &l.id,
        &a.candidate.display().to_string(),
    ));
    json(&a.json, "eh funclets", &l.id, Some(&r.net), &r.rows)?;
    Ok(Outcome::Ok)
}

fn ladder(config: Option<&Path>, a: &LadderArgs) -> Result<Outcome> {
    let l = load(config, &a.image)?;
    let text = read::text(&a.cod, read::MAX_TEXT)?;
    let (sym, func) = listing::function(&text, a.symbol.as_deref())?;
    let obj = a.obj.clone().unwrap_or_else(|| a.cod.with_extension("obj"));
    let comdats = msvc6_diff::comdat::text_of(&read::bytes(&obj, read::MAX_OBJECT)?)
        .with_context(|| obj.display().to_string())?;
    let ours = comdats
        .iter()
        .find(|c| c.symbol.as_deref() == Some(sym.as_str()))
        .with_context(|| format!("{} has no .text COMDAT for `{sym}'", obj.display()))?;
    let ix = msvc6_project::functions::index_of(&l.p, l.n, &l.r)?;
    let next = ix.iter().map(|f| f.rva).filter(|r| *r > a.rva).min();
    let want = next.map_or(ours.bytes.len().saturating_add(ours.bytes.len() / 8), |e| {
        usize::try_from(e - a.rva).unwrap_or(usize::MAX)
    });
    let Some(body) = l
        .img
        .at_rva(Rva(a.rva), want)
        .or_else(|| l.img.at_rva(Rva(a.rva), ours.bytes.len()))
    else {
        bail!("{:#x}+{want:#x} is not in {}'s file bytes", a.rva, l.id);
    };
    let o = msvc6_project::names::oracle_of(&l.p, l.n, &l.r)?;
    let mut names: BTreeMap<u32, String> = BTreeMap::new();
    for x in o
        .iter()
        .filter(|x| x.kind != Kind::Import && x.scope.is_none())
    {
        names.entry(x.rva).or_insert_with(|| x.name.clone());
    }
    for f in ix.iter() {
        if let Some(n) = &f.name {
            names.entry(f.rva).or_insert_with(|| n.clone());
        }
    }
    let triples: Vec<(String, Vec<u8>, Vec<bool>)> = comdats
        .iter()
        .filter_map(|c| Some((c.symbol.clone()?, c.bytes.clone(), c.relocated.clone())))
        .collect();
    let img = &l.img;
    let at = |r: u32, n: usize| img.at_rva(Rva(r), n);
    let name = |r: u32| names.get(&r).cloned();
    let lad = ehladder::ladder(&ehladder::Inputs {
        ours: ehladder::Ours {
            symbol: &sym,
            listing: &func,
            bytes: &ours.bytes,
            relocated: &ours.relocated,
        },
        image: ehladder::Reference {
            rva: a.rva,
            bytes: body,
            base: u64::from(img.opt.image_base.get()),
            size: img.opt.size_of_image,
            bounded: next.is_some(),
        },
        openers: &a.exits,
        comdats: &triples,
        at: &at,
        name: &name,
    });
    say(&ehladder::report(
        &lad,
        &l.id,
        a.rva,
        &sym,
        &a.exits,
        a.all_inlines,
    ));
    json(&a.json, "eh ladder", &l.id, Some(&lad.census), &lad.rows)?;
    Ok(Outcome::Ok)
}
