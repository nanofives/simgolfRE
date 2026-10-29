use crate::Outcome;
use crate::pdbreloc::{file_name, reference};
use crate::project::Config;
use anyhow::{Context as _, Result, anyhow};
use clap::Args;
use msvc6_base::digest::sha256_hex;
use msvc6_base::srcio;
use msvc6_pdb::author::{self, Knowledge, Tsv};
use msvc6_pdb::config::AuthorSpec;
use msvc6_pdb::types::Types;
use std::path::{Path, PathBuf};

pub const MAX_TABLE: u64 = 256 << 20;

#[derive(Debug, Args)]
pub struct AuthorArgs {
    #[arg(
        value_name = "IMAGE",
        help = "An [[image]] id with an [image.pdb.author] table: the binary the PDB is written for."
    )]
    pub image: String,
    #[arg(long, value_name = "FILE", help = "Where the PDB 2.0 goes.")]
    pub out: PathBuf,
    #[arg(
        long,
        value_name = "FILE",
        help = "Where the expect table goes (the names a load must put at each address). Default: OUT with the extension .expect.tsv."
    )]
    pub expect: Option<PathBuf>,
    #[arg(
        long,
        value_name = "REV",
        help = "Read every table at this commit of author.root (`git show`). Default: author.rev, else the working tree."
    )]
    pub rev: Option<String>,
}

struct Source<'a> {
    root: &'a Path,
    rev: Option<&'a str>,
}

impl Source<'_> {
    fn read(&self, rel: &str) -> Result<String> {
        let bytes = match self.rev {
            Some(rev) => msvc6_source::git::file_at_bounded(self.root, rev, rel, MAX_TABLE)?
                .ok_or_else(|| anyhow!("{rev}:{rel} is not in {}", self.root.display()))?,
            None => {
                let p = self.root.join(rel);
                srcio::read_bounded(&p, MAX_TABLE)
                    .with_context(|| format!("reading {}", p.display()))?
            }
        };
        if u64::try_from(bytes.len()).unwrap_or(u64::MAX) > MAX_TABLE {
            anyhow::bail!("{rel}: more than the {MAX_TABLE} bytes a knowledge table is read to");
        }
        Ok(srcio::decode(&bytes))
    }

    fn optional(&self, key: &str, rel: Option<&str>) -> Result<Option<String>> {
        rel.map(|r| {
            self.read(r)
                .with_context(|| format!("[image.pdb.author] {key} names `{r}`"))
        })
        .transpose()
    }
}

pub fn spec_of(c: &Config, n: usize) -> Result<&AuthorSpec> {
    c.images
        .get(n)
        .and_then(|d| d.pdb.as_ref())
        .and_then(|p| p.author.as_ref())
        .with_context(|| {
            format!(
                "image `{}` has no [image.pdb.author] table",
                c.images.get(n).map_or("?", |d| d.id.as_str())
            )
        })
}

pub fn author(config: Option<&Path>, a: &AuthorArgs) -> Result<Outcome> {
    let c = Config::require(config)?;
    let r = reference(&c, &a.image)?;
    let spec = spec_of(&c, r.n)?;
    let key = format!("{}.pdb.author", c.image_key(r.n));
    let root = c.path_at(&format!("{key}.root"), &spec.root)?;
    let rev = a.rev.as_deref().or(spec.rev.as_deref());
    let src = Source { root: &root, rev };
    let commit = match rev {
        Some(r) => msvc6_source::git::rev(&root, r)?,
        None => "the working tree".to_string(),
    };
    println!(
        "authoring a PDB for {} from {} at {commit}",
        a.image,
        root.display()
    );
    let k = Knowledge {
        functions: Tsv::parse(&src.read(&spec.functions)?),
        data: Tsv::parse(&src.read(spec.data.file())?),
        data_va: spec.data.va().to_string(),
        globals: src
            .optional("globals", spec.globals.as_deref())?
            .map(|t| Tsv::parse(&t)),
        library: src
            .optional("library", spec.library.as_deref())?
            .map(|t| Tsv::parse(&t)),
        index: Tsv::parse(&src.read(&spec.index)?),
        locals: src
            .optional("locals", spec.locals.as_deref())?
            .map_or(0, |t| Tsv::parse(&t).rows.len()),
        modules: spec.modules.clone().unwrap_or_default(),
    };
    let dir = spec.types.clone().unwrap_or_default();
    let in_dir = |f: &str| {
        if dir.is_empty() {
            f.to_string()
        } else {
            format!("{dir}/{f}")
        }
    };
    let mut headers: Vec<&str> = spec.headers.iter().map(String::as_str).collect();
    for h in &spec.optional_headers {
        if src.read(&in_dir(h)).is_ok() {
            headers.push(h);
        }
    }
    let mut types = Types::default();
    types.load_headers(&headers, &|f: &str| src.read(&in_dir(f)))?;
    types.emit_headers()?;
    let done = author::author(&r.image, &k, types)?;
    srcio::own_write(&a.out, &done.written.bytes)
        .with_context(|| format!("writing {}", a.out.display()))?;
    let exp = a
        .expect
        .clone()
        .unwrap_or_else(|| a.out.with_extension("expect.tsv"));
    srcio::own_write(&exp, author::expect_tsv(&done.expect))
        .with_context(|| format!("writing {}", exp.display()))?;
    let w = &done.written;
    println!(
        "\n{} ({}): {} bytes, {} pages x {}, {} streams ({} bytes of symbol records) -> {}",
        a.image,
        file_name(&r.path),
        w.bytes.len(),
        w.pages,
        w.page_size,
        w.streams.len(),
        w.symrec_len,
        a.out.display()
    );
    let ident = match r.image.codeview()? {
        Some(cv) => format!(
            "signature {:#010x} age {} -- the shipped {} record ({})",
            cv.signature, cv.age, cv.format, cv.path
        ),
        None => format!(
            "no debug record in the image: signature {:#010x} (its link timestamp) age 1, loaded explicitly",
            r.image.timestamp
        ),
    };
    println!("  identity: {ident}");
    for l in author::lines(&done) {
        println!("{l}");
    }
    println!(
        "  expected names: {} -> {}",
        done.expect.len(),
        exp.display()
    );
    println!("  sha256 {}", sha256_hex(&w.bytes));
    Ok(Outcome::Ok)
}
