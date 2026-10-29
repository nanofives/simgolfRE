use crate::config::Project;
use crate::spans::{index, join};
use msvc6_image::pe::Image;
use msvc6_toolchain::layout::Toolchain;
use msvc6_toolchain::version::{self, ImageReader};
use serde::Serialize;
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Status {
    Ok,
    Fail,
    Skip,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Row {
    pub status: Status,
    pub subject: String,
    pub check: String,
    pub detail: String,
}

#[derive(Debug, Default)]
pub struct Report {
    pub rows: Vec<Row>,
}

impl Report {
    fn push(&mut self, ok: bool, subject: &str, check: &str, detail: impl Into<String>) {
        self.rows.push(Row {
            status: if ok { Status::Ok } else { Status::Fail },
            subject: subject.to_string(),
            check: check.to_string(),
            detail: detail.into(),
        });
    }

    fn result<T>(
        &mut self,
        subject: &str,
        check: &str,
        r: anyhow::Result<T>,
        ok: impl FnOnce(&T) -> String,
    ) -> Option<T> {
        match r {
            Ok(v) => {
                let d = ok(&v);
                self.push(true, subject, check, d);
                Some(v)
            }
            Err(e) => {
                self.push(false, subject, check, format!("{e:#}"));
                None
            }
        }
    }

    #[must_use]
    pub fn failures(&self) -> usize {
        self.rows
            .iter()
            .filter(|r| r.status == Status::Fail)
            .count()
    }

    #[must_use]
    pub fn skipped(&self) -> usize {
        self.rows
            .iter()
            .filter(|r| r.status == Status::Skip)
            .count()
    }

    fn absent(&mut self, p: &Project, subject: &str, check: &str, raw: &str) -> bool {
        let Some(v) = p.absent(raw) else {
            return false;
        };
        self.rows.push(Row {
            status: Status::Skip,
            subject: subject.to_string(),
            check: check.to_string(),
            detail: crate::config::absent_note(&v),
        });
        true
    }
}

fn exists(p: &Project, key: &str, raw: &str, dir: bool) -> anyhow::Result<String> {
    let path = p.path_at(key, raw)?;
    let ok = if dir { path.is_dir() } else { path.is_file() };
    if ok {
        Ok(path.display().to_string())
    } else {
        Err(p.error(
            key,
            format!(
                "{} is not a {}",
                path.display(),
                if dir { "directory" } else { "file" }
            ),
        ))
    }
}

fn toolchains(p: &Project, r: &mut Report) {
    let cfg = p.toolchain_config();
    for id in p.toolchains.keys() {
        let subject = format!("toolchain.{id}");
        let Some(tc) = r.result(
            &subject,
            "tree",
            Toolchain::named(&cfg, id)
                .map_err(|e| p.error(&join(&subject, "root"), format!("{e:#}"))),
            |t| t.root.display().to_string(),
        ) else {
            continue;
        };
        let found = version::detect(&tc, &ImageReader);
        for t in found.iter().filter(|t| t.expected.is_some()) {
            let want = t.expected.clone().unwrap_or_default();
            let have = t.version.map_or_else(
                || {
                    if t.path.is_some() {
                        "no version".to_string()
                    } else {
                        "absent".to_string()
                    }
                },
                |v| v.to_string(),
            );
            let ok = t.verdict() == Some(true);
            r.push(
                ok,
                &subject,
                &format!("expect {}", t.role),
                if ok {
                    format!("{} {have}", t.file)
                } else {
                    format!(
                        "{}: {} is {have}, not the {want} expected",
                        p.at(&join(&join(&subject, "expect"), t.role)),
                        t.file
                    )
                },
            );
        }
    }
    r.result(
        "runner",
        "settings",
        msvc6_toolchain::runner::Runner::from_config(&cfg)
            .map_err(|e| p.error("runner", format!("{e:#}"))),
        |run| {
            let kind = format!("{:?}", run.kind).to_ascii_lowercase();
            format!("{kind}, {} job(s)", run.jobs)
        },
    );
}

fn image(p: &Project, n: usize, r: &mut Report) {
    let Some(d) = p.images.get(n) else {
        return;
    };
    let key = p.image_key(n);
    let subject = format!("{key} {}", d.id);
    let Some(res) = r.result(&subject, "reference", p.reference(n), |res| {
        format!(
            "{} ({} bytes{})",
            res.path.display(),
            res.bytes.len(),
            if d.sha256.is_some() {
                ", sha256 matches"
            } else {
                ", no sha256 declared"
            }
        )
    }) else {
        return;
    };
    let Some(img) = r.result(&subject, "pe", Image::parse(res.bytes.clone()), |i| {
        format!(
            "{} section(s), base {:#x}",
            i.sections.len(),
            i.opt.image_base.get()
        )
    }) else {
        return;
    };
    if !d.clock.is_empty() {
        r.result(
            &subject,
            "clock",
            msvc6_image::clock::resolve(&img, &d.clock)
                .map_err(|e| p.error(&join(&key, "clock"), format!("{e:#}"))),
            |s| format!("{} of {} field(s) located", s.len(), d.clock.len()),
        );
    }
    if !d.residue.is_empty() {
        r.result(
            &subject,
            "residue",
            msvc6_image::postlink::resolve(&d.residue, &img, &img)
                .map_err(|e| p.error(&join(&key, "residue"), format!("{e:#}"))),
            |s| format!("{} row(s) located on the reference", s.len()),
        );
    }
    if let Some(o) = &d.objdir
        && !r.absent(p, &subject, "objdir", o)
    {
        r.result(
            &subject,
            "objdir",
            exists(p, &join(&key, "objdir"), o, true),
            Clone::clone,
        );
    }
    if let Some(m) = &d.map
        && !r.absent(p, &subject, "map", m)
    {
        let k = join(&key, "map");
        r.result(
            &subject,
            "map",
            p.path_at(&k, m).and_then(|path| {
                crate::mapfile::Map::read(&path).map_err(|e| p.error(&k, format!("{e:#}")))
            }),
            |m| format!("{} symbol(s), base {:#x}", m.symbols.len(), m.base),
        );
    }
    for (i, l) in d.libraries.iter().enumerate() {
        if r.absent(p, &subject, "library", l) {
            continue;
        }
        let k = index(&join(&key, "libraries"), i);
        let got = p.path_at(&k, l).and_then(|path| {
            let b = std::fs::read(&path)
                .map_err(|e| p.error(&k, format!("{}: {e}", path.display())))?;
            if msvc6_image::archive::is_archive(&b) {
                Ok(path.display().to_string())
            } else {
                Err(p.error(&k, format!("{} is not an archive", path.display())))
            }
        });
        r.result(&subject, "library", got, Clone::clone);
    }
    if d.project.is_some() || !d.tu.is_empty() || !d.vendored.is_empty() {
        match crate::spec::resolve(p, n) {
            Ok(s) => {
                let missing: Vec<String> = s
                    .specs
                    .iter()
                    .filter(|c| !Path::new(&c.path).is_file())
                    .map(|c| c.path.clone())
                    .collect();
                let dirs: Vec<String> = s
                    .specs
                    .iter()
                    .flat_map(|c| c.include_paths.iter())
                    .filter(|i| !Path::new(i).is_dir())
                    .cloned()
                    .collect::<std::collections::BTreeSet<_>>()
                    .into_iter()
                    .collect();
                r.push(
                    s.refused.is_empty()
                        && missing.is_empty()
                        && dirs.is_empty()
                        && !s.specs.is_empty(),
                    &subject,
                    "project",
                    if s.refused.is_empty()
                        && missing.is_empty()
                        && dirs.is_empty()
                        && !s.specs.is_empty()
                    {
                        let projects: std::collections::BTreeSet<&str> =
                            s.specs.iter().map(|c| c.project.as_str()).collect();
                        format!(
                            "{} TU(s) in {} project(s), every source and /I directory present",
                            s.specs.len(),
                            projects.len()
                        )
                    } else if s.specs.is_empty() && s.refused.is_empty() {
                        format!("{}: no translation unit", p.at(&join(&key, "project")))
                    } else {
                        let mut why: Vec<String> = s
                            .refused
                            .iter()
                            .map(|f| {
                                format!(
                                    "{} {}: {}",
                                    f.project,
                                    f.source.as_deref().unwrap_or(""),
                                    f.why
                                )
                            })
                            .collect();
                        why.extend(missing.iter().map(|m| format!("no source {m}")));
                        why.extend(dirs.iter().map(|m| format!("no include directory {m}")));
                        why.join("; ")
                    },
                );
            }
            Err(e) => r.push(false, &subject, "project", format!("{e:#}")),
        }
    }
    for (i, s) in d.functions.iter().enumerate() {
        if s.input()
            .is_some_and(|raw| r.absent(p, &subject, "functions", raw))
        {
            continue;
        }
        r.result(
            &subject,
            "functions",
            crate::functions::read_one(p, n, i, s, &img),
            |(kind, src, fs, _)| format!("{kind} {src}: {} function(s)", fs.len()),
        );
    }
    for (i, s) in d.names.iter().enumerate() {
        if s.input()
            .is_some_and(|raw| r.absent(p, &subject, "names", raw))
        {
            continue;
        }
        r.result(
            &subject,
            "names",
            crate::names::read_one(p, n, i, s, &img),
            |(kind, src, ns, _)| format!("{kind} {src}: {} name(s)", ns.len()),
        );
    }
    for (i, s) in d.dropped.iter().enumerate() {
        if s.input()
            .is_some_and(|raw| r.absent(p, &subject, "dropped", raw))
        {
            continue;
        }
        r.result(
            &subject,
            "dropped",
            crate::dropped::read_one(p, n, i, s),
            |(kind, src, rows)| format!("{kind} {src}: {} dropped name(s)", rows.len()),
        );
    }
    codeview(p, n, &subject, r);
    for (ok, what, detail) in crate::classes::check(p, n) {
        r.push(ok, &subject, &what, detail);
    }
    pdb(p, n, &subject, &res.bytes, r);
    if let Some(b) = &d.build {
        let bk = join(&key, "build");
        for (i, m) in b.mount.iter().enumerate() {
            if r.absent(p, &subject, "build mount", &m.from) {
                continue;
            }
            let k = join(&index(&join(&bk, "mount"), i), "from");
            r.result(&subject, "build mount", exists(p, &k, &m.from, true), |s| {
                format!("{} at {s}", m.at)
            });
        }
        for (i, s) in b.stage.iter().enumerate() {
            if r.absent(p, &subject, "build stage", &s.from) {
                continue;
            }
            let k = join(&index(&join(&bk, "stage"), i), "from");
            let path = p.path_at(&k, &s.from);
            let got = path.and_then(|path| {
                if path.exists() {
                    Ok(path.display().to_string())
                } else {
                    Err(p.error(&k, format!("{} does not exist", path.display())))
                }
            });
            r.result(&subject, "build stage", got, |x| {
                format!("{} from {x}", s.at)
            });
        }
        for (i, s) in b.implib.iter().enumerate() {
            if r.absent(p, &subject, "build implib", &s.def) {
                continue;
            }
            let k = join(&index(&join(&bk, "implib"), i), "def");
            r.result(
                &subject,
                "build implib",
                exists(p, &k, &s.def, false),
                Clone::clone,
            );
        }
        for (i, s) in b.post.iter().enumerate() {
            if let Some(c) = &s.cwd
                && !r.absent(p, &subject, "build post", c)
            {
                let k = join(&index(&join(&bk, "post"), i), "cwd");
                r.result(&subject, "build post", exists(p, &k, c, true), Clone::clone);
            }
        }
    }
}

fn pdb(p: &Project, n: usize, subject: &str, reference: &[u8], r: &mut Report) {
    let Some(spec) = p.images.get(n).and_then(|d| d.pdb.as_ref()) else {
        return;
    };
    let key = join(&p.image_key(n), "pdb");
    let read = |field: &str, raw: &str| {
        let k = join(&key, field);
        p.path_at(&k, raw).and_then(|path| {
            std::fs::read(&path)
                .map(|b| (path.clone(), b))
                .map_err(|e| p.error(&k, format!("{}: {e}", path.display())))
        })
    };
    let mut file = None;
    if let Some(f) = &spec.file
        && !r.absent(p, subject, "pdb file", f)
    {
        let got = read("file", f).and_then(|(path, b)| {
            if !msvc6_pdb::msf::is_pdb20(&b) {
                return Err(p.error(
                    &join(&key, "file"),
                    format!("{} is not a PDB 2.0", path.display()),
                ));
            }
            let s = msvc6_pdb::structure::check(&b);
            if s.failures() > 0 {
                return Err(p.error(
                    &join(&key, "file"),
                    format!(
                        "{}: {} structural failure(s); `msvc6t pdb check` lists them",
                        path.display(),
                        s.failures()
                    ),
                ));
            }
            Ok((path, b, s.summary()))
        });
        file = r
            .result(subject, "pdb file", got, |(path, _, s)| {
                format!("{}: PDB 2.0, {s}", path.display())
            })
            .map(|(_, b, _)| b);
    }
    if let Some(t) = &spec.twin
        && !r.absent(p, subject, "pdb twin", t)
    {
        let got = read("twin", t).and_then(|(path, b)| {
            if !msvc6_pdb::msf7::is_pdb70(&b) {
                return Err(p.error(
                    &join(&key, "twin"),
                    format!("{} is not a PDB 7.0", path.display()),
                ));
            }
            match &file {
                Some(f) => {
                    let v = msvc6_pdb::eq::equivalence(f, &b)
                        .map_err(|e| p.error(&join(&key, "twin"), format!("{e:#}")))?;
                    if v.equivalent() {
                        Ok(format!(
                            "{}: PDB 7.0, equivalent to the file",
                            path.display()
                        ))
                    } else {
                        Err(p.error(
                            &join(&key, "twin"),
                            format!(
                                "{} is not equivalent to the file: {}",
                                path.display(),
                                v.differences.join("; ")
                            ),
                        ))
                    }
                }
                None => Ok(format!("{}: PDB 7.0", path.display())),
            }
        });
        r.result(subject, "pdb twin", got, Clone::clone);
    }
    let md5 = msvc6_base::digest::md5_hex(reference);
    let listed = spec
        .shipped
        .iter()
        .any(|s| s.md5.eq_ignore_ascii_case(&md5));
    for (i, s) in spec.shipped.iter().enumerate() {
        let Some(f) = &s.file else { continue };
        if r.absent(p, subject, "pdb shipped", f) {
            continue;
        }
        let k = index(&join(&key, "shipped"), i);
        let got = read(&index("shipped", i), f).and_then(|(path, b)| {
            let have = msvc6_base::digest::md5_hex(&b);
            if have.eq_ignore_ascii_case(&s.md5) {
                Ok(format!("{}: md5 {have}", path.display()))
            } else {
                Err(p.error(
                    &join(&k, "file"),
                    format!(
                        "{} has md5 {have}, not the {} listed",
                        path.display(),
                        s.md5
                    ),
                ))
            }
        });
        r.result(subject, "pdb shipped", got, Clone::clone);
    }
    if !spec.shipped.is_empty() {
        r.push(
            listed,
            subject,
            "pdb shipped",
            if listed {
                format!("the reference's md5 {md5} is listed")
            } else {
                format!(
                    "{}: the reference's md5 {md5} is not one of the {} listed",
                    p.at(&join(&key, "shipped")),
                    spec.shipped.len()
                )
            },
        );
    }
}

fn codeview(p: &Project, n: usize, subject: &str, r: &mut Report) {
    let Some(spec) = p.images.get(n).and_then(|d| d.codeview.as_ref()) else {
        return;
    };
    let key = join(&p.image_key(n), "codeview");
    let inputs = [
        Some(&spec.record),
        spec.root.as_ref(),
        spec.explained.as_ref(),
    ];
    if inputs
        .into_iter()
        .flatten()
        .any(|raw| r.absent(p, subject, "codeview", raw))
    {
        return;
    }
    let Some(setup) = r.result(
        subject,
        "codeview",
        spec.resolve(&p.dir, &|raw| p.expand(raw), true)
            .map_err(|e| p.error(&key, format!("{e:#}"))),
        |s| format!("{} view(s)", s.tree.as_ref().map_or(0, |t| t.views.len())),
    ) else {
        return;
    };
    let record = std::fs::read(&setup.record)
        .map_err(|e| anyhow::anyhow!("{}: {e}", setup.record.display()))
        .and_then(|b| {
            if let Some(want) = &setup.sha256 {
                let have = msvc6_base::digest::sha256_hex(&b);
                if !have.eq_ignore_ascii_case(want) {
                    anyhow::bail!(
                        "{} has sha256 {have}, not the {want} declared",
                        setup.record.display()
                    );
                }
            }
            let img = Image::parse(b)?;
            let l = msvc6_codeview::nb::locate(&img)?.ok_or_else(|| {
                anyhow::anyhow!("{} carries no NB09 or NB11 record", setup.record.display())
            })?;
            Ok(format!(
                "{}: {} record of {} bytes",
                setup.record.display(),
                l.signature,
                l.len
            ))
        })
        .map_err(|e| p.error(&join(&key, "record"), format!("{e:#}")));
    r.result(subject, "codeview record", record, Clone::clone);
    if let Some(tree) = &setup.tree {
        for (i, v) in tree.views.iter().enumerate() {
            let got = tree
                .view(&v.name)
                .map_err(|e| p.error(&index(&join(&key, "view"), i), format!("{e:#}")));
            r.result(subject, "codeview view", got, |view| {
                format!(
                    "{}: {} source(s), {} search director(ies)",
                    view.name,
                    view.sources.len(),
                    view.search.len()
                )
            });
        }
    }
    if let Some(e) = &setup.explained {
        let got = msvc6_base::srcio::own_read(e)
            .map_err(|x| anyhow::anyhow!("{}: {x}", e.display()))
            .and_then(|t| {
                msvc6_codeview::diff::read_explained(&t, &e.display().to_string(), &setup.reasons)
            })
            .map_err(|x| p.error(&join(&key, "explained"), format!("{x:#}")));
        r.result(subject, "codeview explained", got, |rows| {
            format!("{}: {} row(s)", e.display(), rows.len())
        });
    }
}
#[must_use]
pub fn run(p: &Project, only: &[usize]) -> Report {
    let mut r = Report::default();
    r.push(
        true,
        "config",
        "schema",
        format!(
            "{}: schema {}, {} image(s), {} toolchain(s)",
            p.file.display(),
            p.schema,
            p.images.len(),
            p.toolchains.len()
        ),
    );
    toolchains(p, &mut r);
    for n in 0..p.images.len() {
        if only.is_empty() || only.contains(&n) {
            image(p, n, &mut r);
        }
    }
    if let Some(root) = &p.source.root {
        r.result(
            "source",
            "root",
            exists(p, "source.root", root, true),
            Clone::clone,
        );
    }
    if let Some(rc) = &p.reccmp {
        r.result(
            "reccmp",
            "project",
            exists(p, "reccmp.project", &rc.project, false),
            Clone::clone,
        );
    }
    r
}
