use crate::Outcome;
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::Args;
use msvc6_base::digest::sha256_hex;
use msvc6_base::srcio;
use msvc6_image::pe::Image;
use msvc6_pdb::config::RelocateSpec;
use msvc6_pdb::linedir::{self, ByObject, TuSet};
use msvc6_pdb::msf::Msf2;
use msvc6_pdb::paths::Policy;
use msvc6_pdb::reloc::{self, MapRow};
use msvc6_pdb::report::Report;
use msvc6_project::names::Kind;
use std::collections::BTreeMap;
use std::io::Write as _;
use std::path::{Path, PathBuf};

pub const MAX_IMAGE: u64 = 512 << 20;
pub const MAX_PDB: u64 = 1 << 30;
pub const MAX_MAP: u64 = msvc6_project::mapfile::MAX_MAP;
pub const MAX_DSP: u64 = 16 << 20;

#[derive(Debug, Args)]
pub struct RelocateArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The [[image]] id of the shipped image the PDB is made for (its reference, sha256-checked)."
    )]
    pub image: String,
    #[arg(
        value_name = "PDB",
        help = "Our link's own PDB 2.0, written by VC6's LINK for --ours."
    )]
    pub pdb: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "The image our link wrote beside PDB."
    )]
    pub ours: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "The pairing report of the same build: reccmp's JSON (data[] rows of address, recomp and name), each row's symbol, obj, tu, size and edit under [image.pdb.relocate] report_key."
    )]
    pub report: PathBuf,
    #[arg(long, value_name = "FILE", help = "Where the relocated PDB goes.")]
    pub out: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link's /MAP. Default: --ours with the extension .map, when it is there."
    )]
    pub map: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Write the derived pairings here as TSV, and the retired records to <FILE stem>.retired.tsv."
    )]
    pub pairs: Option<PathBuf>,
    #[arg(long, help = "Relocate on the report's rows alone: the control.")]
    pub no_derive: bool,
    #[arg(
        long,
        value_name = "KEY",
        help = "The report rows' extension object. Default: [image.pdb.relocate] report_key, else `msvc6t`."
    )]
    pub report_key: Option<String>,
    #[arg(
        long,
        value_name = "DIR",
        help = "The source tree the report's TUs are relative to. Default: [image.pdb.paths] product."
    )]
    pub product: Option<PathBuf>,
    #[arg(
        long,
        value_name = "N",
        default_value_t = 0,
        help = "Prove N derived data pairings against our objects' own bytes (`--objs`)."
    )]
    pub sample: usize,
    #[arg(
        long = "objs",
        value_name = "DIR",
        help = "A directory of our objects, for --sample (repeatable). Default: the directory holding --ours."
    )]
    pub objs: Vec<PathBuf>,
}

pub struct Reference {
    pub n: usize,
    pub path: PathBuf,
    pub image: Image,
    pub bytes: Vec<u8>,
}

impl std::fmt::Debug for Reference {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Reference")
            .field("n", &self.n)
            .field("path", &self.path)
            .finish_non_exhaustive()
    }
}

pub fn read_image(path: &Path) -> Result<Image> {
    let bytes = srcio::read_bounded(path, MAX_IMAGE)
        .with_context(|| format!("reading {}", path.display()))?;
    Image::parse(bytes).with_context(|| path.display().to_string())
}

pub fn reference(c: &Config, id: &str) -> Result<Reference> {
    let (n, d) = msvc6_project::registry::resolve_id(c, id)?;
    let path = c.reference_path(n)?;
    let bytes = srcio::read_bounded(&path, MAX_IMAGE)
        .with_context(|| format!("image `{}`: reading {}", d.id, path.display()))?;
    if let Some(want) = &d.sha256 {
        let have = sha256_hex(&bytes);
        if !have.eq_ignore_ascii_case(want.as_str()) {
            return Err(c.error(
                &format!("{}.sha256", c.image_key(n)),
                format!(
                    "image `{}`: {} has sha256 {have}, not the {} declared",
                    d.id,
                    path.display(),
                    want.as_str()
                ),
            ));
        }
    }
    let image = Image::parse(bytes.clone()).with_context(|| path.display().to_string())?;
    Ok(Reference {
        n,
        path,
        image,
        bytes,
    })
}

pub fn file_name(p: &Path) -> String {
    p.file_name().map_or_else(
        || p.display().to_string(),
        |n| n.to_string_lossy().into_owned(),
    )
}

#[derive(Debug, Default)]
pub struct RefNames {
    pub flat: BTreeMap<String, u32>,
    pub scoped: BTreeMap<String, BTreeMap<String, u32>>,
}

pub fn reference_names(c: &Config, r: &Reference) -> Result<RefNames> {
    let resolved = msvc6_project::registry::Resolved {
        label: c.images.get(r.n).map(|d| d.id.clone()).unwrap_or_default(),
        path: r.path.clone(),
        bytes: r.bytes.clone(),
        decl: c.images.get(r.n).cloned(),
        key: Some(c.image_key(r.n)),
    };
    let o = msvc6_project::names::oracle_of(c, r.n, &resolved)?;
    let mut out = RefNames::default();
    let bodies = o.named_bodies();
    for n in o.iter() {
        if matches!(n.kind, Kind::Import | Kind::Absolute)
            || (n.kind == Kind::Thunk && bodies.contains(n.name.as_str()))
        {
            continue;
        }
        match &n.scope {
            Some(s) => {
                out.scoped
                    .entry(s.clone())
                    .or_default()
                    .insert(n.name.clone(), n.rva);
            }
            None => {
                if n.provider == "exports" {
                    out.flat.entry(format!("_{}", n.name)).or_insert(n.rva);
                }
                out.flat.entry(n.name.clone()).or_insert(n.rva);
            }
        }
    }
    Ok(out)
}

pub fn read_pdb(path: &Path) -> Result<Msf2> {
    let bytes = srcio::read_bounded(path, MAX_PDB)
        .with_context(|| format!("reading {}", path.display()))?;
    Msf2::parse(bytes).with_context(|| path.display().to_string())
}

pub fn map_rows(path: &Path) -> Result<Vec<MapRow>> {
    let text = srcio::read_text_bounded(path, MAX_MAP)
        .with_context(|| format!("reading {}", path.display()))?;
    let m = msvc6_project::mapfile::Map::parse(path, &text)?;
    Ok(msvc6_gate::addrsym::map_rows(&m)
        .into_iter()
        .map(|r| MapRow {
            rva: r.rva,
            name: r.name,
            stem: r.stem,
        })
        .collect())
}

pub fn product_of(c: &Config, n: usize, given: Option<&Path>) -> Result<Option<PathBuf>> {
    if let Some(p) = given {
        return Ok(Some(p.to_path_buf()));
    }
    let Some(spec) = c
        .images
        .get(n)
        .and_then(|d| d.pdb.as_ref())
        .and_then(|p| p.paths.as_ref())
    else {
        return Ok(None);
    };
    Ok(Some(c.path_at(
        &format!("{}.pdb.paths.product", c.image_key(n)),
        &spec.product,
    )?))
}

fn find_dsp(dir: &Path) -> Result<Option<PathBuf>> {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return Ok(None);
    };
    let mut found: Vec<PathBuf> = rd
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x.eq_ignore_ascii_case("dsp")) && p.is_file())
        .collect();
    found.sort();
    match found.as_slice() {
        [] => Ok(None),
        [one] => Ok(Some(one.clone())),
        many => bail!(
            "{} holds {} projects ({}): which one a TU there belongs to is not known",
            dir.display(),
            many.len(),
            many.iter()
                .map(|p| file_name(p))
                .collect::<Vec<_>>()
                .join(", ")
        ),
    }
}

fn dsp_includes(tu: &Path, product: &Path) -> Result<Vec<PathBuf>> {
    let mut dir = tu.parent();
    while let Some(d) = dir.filter(|d| d.starts_with(product)) {
        if let Some(dsp) = find_dsp(d)? {
            let text = srcio::read_text_bounded(&dsp, MAX_DSP)
                .with_context(|| format!("reading {}", dsp.display()))?;
            let p = msvc6_project::dsp::Dsp::parse(&dsp, &text)?;
            let Some(config) = p
                .default_config
                .clone()
                .or_else(|| p.configs.first().cloned())
            else {
                return Ok(Vec::new());
            };
            let s = p.settings(&config);
            return Ok(s
                .global_add
                .iter()
                .filter(|x| {
                    msvc6_project::switches::class(x) == msvc6_project::switches::Class::Include
                })
                .filter_map(|x| x.get(2..))
                .map(|i| msvc6_project::dsp::join_windows(&p.dir(), i))
                .collect());
        }
        dir = d.parent();
    }
    Ok(Vec::new())
}

pub fn line_directives(
    c: &Config,
    n: usize,
    report: &Report,
    product: Option<&Path>,
) -> Result<ByObject> {
    let tus = TuSet::of_report(report);
    if tus.by_object.is_empty() {
        return Ok(ByObject::new());
    }
    let Some(product) = product else {
        bail!(
            "the report ties {} object(s) to TUs, and no product root is known to read them from: give \
             --product or an [image.pdb.paths] product",
            tus.by_object.len()
        );
    };
    let mut by_tu: BTreeMap<PathBuf, Vec<PathBuf>> = BTreeMap::new();
    if let Ok(specs) = msvc6_project::spec::resolve(c, n) {
        for s in &specs.specs {
            if let Ok(p) = std::fs::canonicalize(&s.path) {
                by_tu.insert(p, s.include_paths.iter().map(PathBuf::from).collect());
            }
        }
    }
    let includes = |tu: &Path| -> Result<Vec<PathBuf>> {
        if let Some(v) = std::fs::canonicalize(tu).ok().and_then(|p| by_tu.get(&p)) {
            return Ok(v.clone());
        }
        dsp_includes(tu, product)
    };
    linedir::for_tus(&tus, product, &includes)
}

pub fn policy(
    c: &Config,
    n: usize,
    product: Option<&Path>,
    dirs: &ByObject,
) -> Result<Option<Policy>> {
    let Some(spec) = c
        .images
        .get(n)
        .and_then(|d| d.pdb.as_ref())
        .and_then(|p| p.paths.as_ref())
    else {
        return Ok(None);
    };
    let key = format!("{}.pdb.paths", c.image_key(n));
    let product = match product {
        Some(p) => p.to_path_buf(),
        None => c.path_at(&format!("{key}.product"), &spec.product)?,
    };
    Policy::new(spec, &product, linedir::line_names(dirs))
        .map(Some)
        .map_err(|e| c.error(&key, format!("{e:#}")))
}

pub fn report_key<'a>(c: &'a Config, n: usize, given: Option<&'a str>) -> &'a str {
    given.unwrap_or_else(|| {
        RelocateSpec::key(
            c.images
                .get(n)
                .and_then(|d| d.pdb.as_ref())
                .and_then(|p| p.relocate.as_ref()),
        )
    })
}

fn same_file(a: &Path, b: &Path) -> bool {
    std::fs::canonicalize(a)
        .ok()
        .zip(std::fs::canonicalize(b).ok())
        .is_some_and(|(x, y)| x == y)
}

pub fn relocate(config: Option<&Path>, a: &RelocateArgs) -> Result<Outcome> {
    let c = Config::require(config)?;
    let r = reference(&c, &a.image)?;
    if same_file(&a.out, &a.pdb) {
        bail!("--out {} is the PDB being relocated", a.out.display());
    }
    let ours = read_image(&a.ours)?;
    let key = report_key(&c, r.n, a.report_key.as_deref());
    let report = Report::read(&a.report, key)?;
    let product = product_of(&c, r.n, a.product.as_deref())?;
    let dirs = line_directives(&c, r.n, &report, product.as_deref())?;
    let policy = policy(&c, r.n, product.as_deref(), &dirs)?;
    let names = reference_names(&c, &r)?;
    let dropped = msvc6_project::dropped::dropped_of(&c, r.n)?;
    let map_path = a
        .map
        .clone()
        .or_else(|| Some(a.ours.with_extension("map")).filter(|p| p.is_file()));
    let map = map_path.as_deref().map(map_rows).transpose()?;
    let map_label = map_path.as_ref().map(|p| p.display().to_string());
    let mut db = read_pdb(&a.pdb)?;
    let report_label = a.report.display().to_string();
    let name = file_name(&r.path);
    let inputs = reloc::Inputs {
        name: &name,
        reference: &r.image,
        ours: &ours,
        report: &report,
        report_label: &report_label,
        map: map.as_deref().zip(map_label.as_deref()),
        names: &names.flat,
        scoped: &names.scoped,
        dropped: &dropped,
        dirs: &dirs,
        policy: policy.as_ref(),
        pairs: a.pairs.as_deref(),
        no_derive: a.no_derive,
    };
    let stdout = std::io::stdout();
    let mut w = stdout.lock();
    let done = reloc::relocate(&inputs, &mut db, &mut w)?;
    srcio::own_write(&a.out, db.bytes()).with_context(|| format!("writing {}", a.out.display()))?;
    done.tail(&a.out.display().to_string(), db.bytes().len(), &mut w)?;
    if let Some(p) = &a.pairs {
        let rp = reloc::retired_path(p);
        srcio::own_write(&rp, done.retired_tsv())
            .with_context(|| format!("writing {}", rp.display()))?;
        writeln!(
            w,
            "  wrote {} ({} retired record(s))",
            rp.display(),
            done.score.retired_list.len()
        )?;
    }
    writeln!(w, "  sha256 {}", sha256_hex(db.bytes()))?;
    if a.sample > 0 {
        let d = done.derived.as_ref().context(
            "--sample proves the DERIVED pairings, so it cannot be used with --no-derive",
        )?;
        let dirs: Vec<PathBuf> = if a.objs.is_empty() {
            a.ours.parent().map(Path::to_path_buf).into_iter().collect()
        } else {
            a.objs.clone()
        };
        return crate::pdbsample::run(&r.image, &ours, d, &dirs, a.sample, &mut w);
    }
    Ok(Outcome::Ok)
}
