use crate::Outcome;
use crate::pdbreloc::{
    file_name, line_directives, map_rows, policy, product_of, read_image, read_pdb, reference,
    report_key,
};
use crate::project::Config;
use anyhow::{Context as _, Result, bail};
use clap::Args;
use msvc6_base::srcio;
use msvc6_pdb::audit::{self, EXAMPLES};
use msvc6_pdb::linedir::ByObject;
use msvc6_pdb::report::Report;
use std::io::Write as _;
use std::path::{Path, PathBuf};

pub const MAX_TABLE: u64 = 256 << 20;

#[derive(Debug, Args)]
pub struct AuditArgs {
    #[arg(
        value_name = "IMAGE",
        help = "The [[image]] id of the shipped image the PDB answers to (its reference, sha256-checked)."
    )]
    pub image: String,
    #[arg(value_name = "PDB", help = "The PDB 2.0 to audit.")]
    pub pdb: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        conflicts_with = "expect",
        help = "The pairing report the relocation read: the identity oracle of a relocated PDB."
    )]
    pub report: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "An authored PDB's expect table (`va kind names`, names `|`-separated): its identity oracle."
    )]
    pub expect: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The derived pairings `pdb relocate --pairs` wrote (and its <stem>.retired.tsv beside it)."
    )]
    pub pairs: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "Our link (its /MAP beside it, .map): the proof of a fold, an alias or a symbol inside another."
    )]
    pub ours: Option<PathBuf>,
    #[arg(
        long,
        value_name = "FILE",
        help = "The PDB the relocation started from: the audit then holds every byte to it but what a relocation writes."
    )]
    pub original: Option<PathBuf>,
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
    #[arg(long, default_value_t = EXAMPLES, help = "Failures and proofs shown per class.")]
    pub shown: usize,
}

fn table(p: &Path) -> Result<String> {
    srcio::own_read_bounded(p, MAX_TABLE).with_context(|| format!("reading {}", p.display()))
}

pub fn audit(config: Option<&Path>, a: &AuditArgs) -> Result<Outcome> {
    let c = Config::require(config)?;
    let r = reference(&c, &a.image)?;
    if a.report.is_none() && a.expect.is_none() {
        bail!("give --report (a relocated PDB) or --expect (an authored one)");
    }
    let key = report_key(&c, r.n, a.report_key.as_deref());
    let report = a
        .report
        .as_deref()
        .map(|p| Report::read(p, key))
        .transpose()?;
    let product = product_of(&c, r.n, a.product.as_deref())?;
    let dirs = match &report {
        Some(rep) => line_directives(&c, r.n, rep, product.as_deref())?,
        None => ByObject::new(),
    };
    let policy = match &report {
        Some(_) => policy(&c, r.n, product.as_deref(), &dirs)?,
        None => None,
    };
    let expect = a.expect.as_deref().map(table).transpose()?;
    let pairs = a.pairs.as_deref().map(table).transpose()?;
    let retired = a
        .pairs
        .as_deref()
        .map(|p| table(&msvc6_pdb::reloc::retired_path(p)))
        .transpose()?;
    let ours = a.ours.as_deref().map(read_image).transpose()?;
    let rows = a
        .ours
        .as_deref()
        .map(|o| map_rows(&o.with_extension("map")))
        .transpose()?;
    let db = read_pdb(&a.pdb)?;
    let original = a.original.as_deref().map(read_pdb).transpose()?;
    let host = audit::host_names();
    let host_paths = audit::host_paths();
    let labels = (
        a.report.as_ref().map(|p| p.display().to_string()),
        a.expect.as_ref().map(|p| p.display().to_string()),
        a.pairs.as_ref().map(|p| p.display().to_string()),
        a.ours.as_ref().map(|p| p.display().to_string()),
    );
    let image_label = r.path.display().to_string();
    let inputs = audit::Inputs {
        image: &r.image,
        image_label: &image_label,
        report: report.as_ref().zip(labels.0.as_deref()),
        expect: expect.as_deref().zip(labels.1.as_deref()),
        pairs: pairs.as_deref().zip(labels.2.as_deref()),
        retired: retired.as_deref(),
        ours: match (&ours, &rows, labels.3.as_deref()) {
            (Some(i), Some(m), Some(l)) => Some((i, m.as_slice(), l)),
            _ => None,
        },
        line_dirs: &dirs,
        policy: policy.as_ref(),
        host: &host,
        host_paths: &host_paths,
        examples: a.shown,
    };
    let stdout = std::io::stdout();
    let mut w = stdout.lock();
    let tally = audit::run(&inputs, &a.pdb.display().to_string(), &db, original, &mut w)?;
    tally.print(&mut w)?;
    let n = tally.failures();
    if n > 0 {
        writeln!(
            w,
            "  {}: {n} failure(s) -- address field(s) FAIL the audit against {}",
            audit::RESULT,
            file_name(&r.path)
        )?;
        return Ok(Outcome::Differs);
    }
    writeln!(
        w,
        "  {}: 0 failures -- every live field is the shipped thing it names, or retired",
        audit::RESULT
    )?;
    Ok(Outcome::Ok)
}
