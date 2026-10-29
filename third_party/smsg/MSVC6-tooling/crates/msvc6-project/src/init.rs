use crate::config::Project;
use crate::dsp::Dsp;
use crate::dsw::Dsw;
use anyhow::{Context as _, Result, bail};
use msvc6_image::clock::Field;
use msvc6_image::pe::Image;
use std::fmt::Write as _;
use std::path::{Component, Path, PathBuf};

#[derive(Debug, Clone, Default)]
pub struct Options {
    pub images: Vec<PathBuf>,
    pub ids: Vec<String>,
    pub dsw: Option<PathBuf>,
    pub dsp: Option<PathBuf>,
    pub config: Option<String>,
    pub vc6: String,
}

fn quote(s: &str) -> String {
    if s.contains('\'') || s.contains('\n') {
        format!("{s:?}")
    } else {
        format!("'{s}'")
    }
}

#[must_use]
pub fn id_of(path: &Path) -> String {
    let stem = path
        .file_stem()
        .map(|s| s.to_string_lossy().into_owned())
        .unwrap_or_default();
    let id: String = stem
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() {
                c.to_ascii_uppercase()
            } else {
                '_'
            }
        })
        .collect();
    if id.is_empty() {
        "IMAGE".to_string()
    } else {
        id
    }
}

fn relative(dir: &Path, target: &Path) -> String {
    let abs = |p: &Path| std::fs::canonicalize(p).unwrap_or_else(|_| p.to_path_buf());
    let (d, t) = (abs(dir), abs(target));
    let dc: Vec<Component<'_>> = d.components().collect();
    let tc: Vec<Component<'_>> = t.components().collect();
    let common = dc.iter().zip(tc.iter()).take_while(|(a, b)| a == b).count();
    if common <= 1 {
        return t.display().to_string();
    }
    let mut out = PathBuf::new();
    for _ in common..dc.len() {
        out.push("..");
    }
    for c in tc.iter().skip(common) {
        out.push(c.as_os_str());
    }
    out.display().to_string()
}

#[must_use]
pub fn clock_fields(img: &Image) -> Vec<Field> {
    msvc6_image::clock::ALL
        .into_iter()
        .filter(|f| msvc6_image::clock::resolve(img, &[*f]).is_ok_and(|s| !s.is_empty()))
        .collect()
}

fn project_line(dir: &Path, o: &Options) -> Result<Option<String>> {
    if let Some(w) = &o.dsw {
        let dsw = Dsw::read(w)?;
        let config = match &o.config {
            Some(c) => c.clone(),
            None => {
                let first = dsw
                    .projects
                    .first()
                    .with_context(|| format!("{}: lists no project", w.display()))?;
                let dsp = Dsp::read(&dsw.dsp_path(first))?;
                dsp.default_config
                    .clone()
                    .or_else(|| dsp.configs.first().cloned())
                    .with_context(|| format!("{}: no configuration", dsp.path.display()))?
            }
        };
        return Ok(Some(format!(
            "project = {{ dsw = {}, config = {} }}\n",
            quote(&relative(dir, w)),
            quote(&config)
        )));
    }
    if let Some(p) = &o.dsp {
        let dsp = Dsp::read(p)?;
        let config = match &o.config {
            Some(c) => c.clone(),
            None => dsp
                .default_config
                .clone()
                .or_else(|| dsp.configs.first().cloned())
                .with_context(|| format!("{}: no configuration", p.display()))?,
        };
        return Ok(Some(format!(
            "project = {{ dsp = {}, config = {} }}\n",
            quote(&relative(dir, p)),
            quote(&config)
        )));
    }
    Ok(None)
}

pub fn render(out: &Path, o: &Options) -> Result<String> {
    if o.images.is_empty() {
        bail!("init needs at least one --image");
    }
    if (o.dsw.is_some() || o.dsp.is_some()) && o.images.len() != 1 {
        bail!("--dsw and --dsp describe one image's project; give one --image with them");
    }
    if !o.ids.is_empty() && o.ids.len() != o.images.len() {
        bail!(
            "{} --id for {} --image; give one per image or none",
            o.ids.len(),
            o.images.len()
        );
    }
    let dir = out
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
        .map_or_else(|| PathBuf::from("."), Path::to_path_buf);
    let mut t = String::new();
    t.push_str("schema = 1\n\n[toolchain.vc6]\n");
    let _ = writeln!(t, "root = {}", quote(&o.vc6));
    t.push_str("layout = \"vs6\"\n\n[runner]\nkind = \"wine\"\n");
    let project = project_line(&dir, o)?;
    for (k, image) in o.images.iter().enumerate() {
        let bytes = std::fs::read(image).with_context(|| format!("reading {}", image.display()))?;
        let img = Image::parse(bytes.clone()).with_context(|| image.display().to_string())?;
        let id = o.ids.get(k).cloned().unwrap_or_else(|| id_of(image));
        t.push_str("\n[[image]]\n");
        let _ = writeln!(t, "id = {}", quote(&id));
        let _ = writeln!(t, "reference = {}", quote(&relative(&dir, image)));
        let _ = writeln!(t, "sha256 = \"{}\"", msvc6_base::digest::sha256_hex(&bytes));
        t.push_str("compiler = \"vc6\"\nlinker = \"vc6\"\n");
        if let Some(p) = &project {
            t.push_str(p);
        }
        let mut names = vec!["\"imports\""];
        if img.exports().ok().flatten().is_some() {
            names.insert(0, "\"exports\"");
        }
        t.push_str("functions = [\"discover\"]\n");
        let _ = writeln!(t, "names = [{}]", names.join(", "));
        let clock: Vec<String> = clock_fields(&img)
            .iter()
            .map(|f| format!("\"{}\"", f.key()))
            .collect();
        let _ = writeln!(t, "clock = [{}]", clock.join(", "));
    }
    Project::parse(&t, out).context("the config init writes does not load (a defect in init)")?;
    Ok(t)
}

pub fn write(out: &Path, o: &Options, force: bool) -> Result<String> {
    if out.exists() && !force {
        bail!("{} exists; pass --force to replace it", out.display());
    }
    let t = render(out, o)?;
    msvc6_base::srcio::own_write(out, &t).with_context(|| format!("writing {}", out.display()))?;
    Ok(t)
}
