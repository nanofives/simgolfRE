use anyhow::{Context as _, Result, bail};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub const MAX_SOURCE: u64 = 64 << 20;
pub const MAX_HEADERS: usize = 1 << 16;

#[derive(Debug, Clone, PartialEq, Eq)]
struct Region {
    from: u32,
    to: u32,
    file: String,
    presumed: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct LineDirectives {
    regions: Vec<Region>,
    index: BTreeMap<String, Vec<Span>>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Span {
    start: u32,
    count: u32,
    offset: i64,
}

fn spans_of(regions: &[&Region]) -> Vec<Span> {
    let mut events: Vec<(u32, i64, i64)> = Vec::with_capacity(2 * regions.len());
    for r in regions {
        let span = r.to.saturating_sub(r.from);
        if span == 0 {
            continue;
        }
        let shift = i64::from(r.from) - i64::from(r.presumed);
        events.push((r.presumed, 1, shift));
        events.push((r.presumed.saturating_add(span), -1, -shift));
    }
    events.sort_unstable_by_key(|e| e.0);
    let mut out: Vec<Span> = Vec::with_capacity(events.len());
    let (mut count, mut offset) = (0i64, 0i64);
    let mut k = 0usize;
    while let Some(&(at, _, _)) = events.get(k) {
        while let Some(&(p, dc, doff)) = events.get(k)
            && p == at
        {
            count += dc;
            offset += doff;
            k += 1;
        }
        out.push(Span {
            start: at,
            count: u32::try_from(count).unwrap_or(u32::MAX),
            offset,
        });
    }
    out
}

fn index_of(regions: &[Region]) -> BTreeMap<String, Vec<Span>> {
    let mut by_file: BTreeMap<String, Vec<&Region>> = BTreeMap::new();
    for r in regions {
        by_file
            .entry(r.file.to_ascii_lowercase())
            .or_default()
            .push(r);
    }
    by_file
        .into_iter()
        .map(|(f, rs)| (f, spans_of(&rs)))
        .collect()
}

fn literal(s: &str) -> Option<String> {
    let mut out = String::new();
    let mut it = s.strip_prefix('"')?.chars();
    while let Some(c) = it.next() {
        match c {
            '"' => return Some(out),
            '\\' => out.push(it.next()?),
            c => out.push(c),
        }
    }
    None
}

#[must_use]
pub fn directive(l: &str) -> Option<(u32, Option<String>)> {
    let rest = l
        .trim_start()
        .strip_prefix('#')?
        .trim_start()
        .strip_prefix("line")?;
    if !rest.starts_with(|c: char| c.is_ascii_whitespace()) {
        return None;
    }
    let rest = rest.trim_start();
    let digits: String = rest.chars().take_while(char::is_ascii_digit).collect();
    let n = digits.parse().ok()?;
    let tail = rest.get(digits.len()..)?.trim();
    if tail.is_empty() {
        return Some((n, None));
    }
    Some((n, Some(literal(tail)?)))
}

impl LineDirectives {
    pub fn parse(src: &str, tu: &str) -> Result<Option<Self>> {
        let total = u32::try_from(src.lines().count())?;
        let mut regions: Vec<Region> = Vec::new();
        let mut open: Option<(u32, String, u32)> = None;
        for (i, l) in src.lines().enumerate() {
            let Some((n, file)) = directive(l) else {
                continue;
            };
            let p = u32::try_from(i)?.saturating_add(1);
            if let Some((from, f, presumed)) = open.take() {
                regions.push(Region {
                    from,
                    to: p,
                    file: f,
                    presumed,
                });
            }
            let file = match (file, regions.last()) {
                (Some(f), _) => f,
                (None, Some(r)) if r.to == p => r.file.clone(),
                (None, _) => bail!(
                    "{tu}:{p}: `#line {n}` without a file name renumbers the TU's own line records, which the \
                     PDB line translation does not handle"
                ),
            };
            open = Some((p.saturating_add(1), file, n));
        }
        if let Some((from, file, presumed)) = open {
            regions.push(Region {
                from,
                to: total.saturating_add(1),
                file,
                presumed,
            });
        }
        if regions.is_empty() {
            return Ok(None);
        }
        let index = index_of(&regions);
        Ok(Some(Self { regions, index }))
    }

    #[must_use]
    pub fn names(&self, name: &str) -> bool {
        self.index.contains_key(&name.to_ascii_lowercase())
    }

    #[must_use]
    pub fn files(&self) -> Vec<String> {
        let mut seen: BTreeSet<String> = BTreeSet::new();
        self.regions
            .iter()
            .filter(|r| seen.insert(r.file.to_ascii_lowercase()))
            .map(|r| r.file.clone())
            .collect()
    }

    #[must_use]
    pub fn physical(&self, name: &str, line: u16) -> Option<u16> {
        let l = u32::from(line);
        let spans = self.index.get(&name.to_ascii_lowercase())?;
        let k = spans.partition_point(|s| s.start <= l);
        let s = spans.get(k.checked_sub(1)?)?;
        if s.count != 1 {
            return None;
        }
        u16::try_from(i64::from(l) + s.offset).ok()
    }
}

pub type ByObject = BTreeMap<String, LineDirectives>;

#[must_use]
pub fn line_names(d: &ByObject) -> BTreeMap<String, Vec<String>> {
    d.iter().map(|(o, l)| (o.clone(), l.files())).collect()
}

#[derive(Debug, Clone, Default)]
pub struct TuSet {
    pub by_object: BTreeMap<String, BTreeSet<String>>,
}

impl TuSet {
    #[must_use]
    pub fn of_report(r: &crate::report::Report) -> Self {
        let mut by_object: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
        for row in &r.rows {
            if row.obj.is_empty() || row.tu.is_empty() {
                continue;
            }
            by_object
                .entry(row.obj.to_ascii_lowercase())
                .or_default()
                .insert(row.tu.clone());
        }
        Self { by_object }
    }
}

pub type Includes<'a> = &'a dyn Fn(&Path) -> Result<Vec<PathBuf>>;

pub fn for_tus(tus: &TuSet, product: &Path, includes: Includes<'_>) -> Result<ByObject> {
    let mut out = BTreeMap::new();
    let mut seen: BTreeSet<PathBuf> = BTreeSet::new();
    for (obj, set) in &tus.by_object {
        let mut with: Vec<(&String, LineDirectives)> = Vec::new();
        for tu in set {
            let path = product.join(tu);
            let src = msvc6_base::srcio::read_text_bounded(&path, MAX_SOURCE).with_context(|| {
                format!(
                    "{obj}'s TU {tu}: cannot read {} -- the PDB line translation reads every TU the report \
                     names (is the product root the tree the build was made from?)",
                    path.display()
                )
            })?;
            let search = includes(&path)?;
            if let Some((h, line)) = headers(&path, &src, &search, &mut seen)?.first() {
                bail!(
                    "{}:{line}: a `#line` in a header {tu} includes -- the PDB line translation maps a TU's own \
                     directives only, so the lines it renumbers would stay presumed",
                    h.display()
                );
            }
            if let Some(d) = LineDirectives::parse(&src, tu)? {
                with.push((tu, d));
            }
        }
        match (set.len(), with.pop()) {
            (_, None) => {}
            (1, Some((_, d))) => {
                out.insert(obj.clone(), d);
            }
            (_, Some((tu, _))) => bail!(
                "{obj}: the report's rows name {} TUs ({}) and {tu} holds a `#line` -- which TU a module's line \
                 records came from is not known, so its directives cannot be applied",
                set.len(),
                set.iter().cloned().collect::<Vec<_>>().join(", ")
            ),
        }
    }
    Ok(out)
}

#[must_use]
pub fn quoted_includes(src: &str) -> Vec<String> {
    src.lines()
        .filter_map(|l| {
            let r = l
                .trim_start()
                .strip_prefix('#')?
                .trim_start()
                .strip_prefix("include")?
                .trim_start()
                .strip_prefix('"')?;
            Some(r.get(..r.find('"')?)?.to_string())
        })
        .collect()
}

#[must_use]
pub fn find_ci(dir: &Path, rel: &str) -> Option<PathBuf> {
    let mut at = dir.to_path_buf();
    for c in rel
        .split(['\\', '/'])
        .filter(|c| !c.is_empty() && *c != ".")
    {
        if c == ".." {
            at.pop();
            continue;
        }
        let hit = std::fs::read_dir(&at).ok()?.find_map(|e| {
            let e = e.ok()?;
            e.file_name()
                .to_string_lossy()
                .eq_ignore_ascii_case(c)
                .then(|| e.path())
        })?;
        at = hit;
    }
    at.is_file().then_some(at)
}

fn headers(
    tu: &Path,
    src: &str,
    search_dirs: &[PathBuf],
    seen: &mut BTreeSet<PathBuf>,
) -> Result<Vec<(PathBuf, usize)>> {
    let tu_dir = tu.parent().map(Path::to_path_buf).unwrap_or_default();
    let mut search = vec![tu_dir.clone()];
    search.extend(search_dirs.iter().cloned());
    let mut found = Vec::new();
    let mut stack: Vec<(PathBuf, Vec<String>)> = vec![(tu_dir, quoted_includes(src))];
    while let Some((from, names)) = stack.pop() {
        for n in names {
            let Some(h) = std::iter::once(&from)
                .chain(&search)
                .find_map(|d| find_ci(d, &n))
            else {
                continue;
            };
            if !seen.insert(h.clone()) {
                continue;
            }
            if seen.len() > MAX_HEADERS {
                bail!(
                    "{}: more than {MAX_HEADERS} headers reached through quoted includes",
                    tu.display()
                );
            }
            let text = msvc6_base::srcio::read_text_bounded(&h, MAX_SOURCE)
                .with_context(|| format!("reading {}", h.display()))?;
            for (i, l) in text.lines().enumerate() {
                if directive(l).is_some() {
                    found.push((h.clone(), i + 1));
                }
            }
            let dir = h.parent().map(Path::to_path_buf).unwrap_or_default();
            stack.push((dir, quoted_includes(&text)));
        }
    }
    Ok(found)
}

#[must_use]
pub fn object_key(module: &str) -> String {
    module
        .rsplit(['\\', '/'])
        .next()
        .unwrap_or(module)
        .to_ascii_lowercase()
}

#[cfg(test)]
mod tests {
    use super::*;

    const DIRECTIVE: &str = "#line 1458 \"C:\\\\Orig\\\\Lib\\\\Big.cpp\"";

    #[test]
    fn a_directive_maps_its_presumed_lines_back() {
        let mut src: Vec<String> = (1..=931).map(|i| format!("// {i}")).collect();
        src.push(DIRECTIVE.to_string());
        src.extend((933..=1683).map(|i| format!("x{i};")));
        let d = LineDirectives::parse(&src.join("\n"), "Big.cpp")
            .unwrap()
            .unwrap();
        let name = "C:\\Orig\\Lib\\Big.cpp";
        assert!(d.names(name));
        assert!(d.names("c:\\orig\\lib\\big.cpp"), "any case");
        assert!(!d.names("c:\\orig\\lib\\src\\big.cpp"));
        assert_eq!(d.files(), [name]);
        assert_eq!(d.physical(name, 1458), Some(933));
        assert_eq!(d.physical(name, 1459), Some(934));
        assert_eq!(d.physical(name, 1458 + 750), Some(1683), "the last line");
        assert_eq!(d.physical(name, 1458 + 751), None, "past the end of the TU");
        assert_eq!(d.physical(name, 1457), None, "before the directive");
        assert_eq!(d.physical("other.cpp", 1459), None);
    }

    #[test]
    fn a_tu_without_a_directive_has_none() {
        assert_eq!(
            LineDirectives::parse("int a;\n#include \"x.h\"\n", "a.cpp").unwrap(),
            None
        );
        assert_eq!(directive("#lines 3"), None);
        assert_eq!(
            directive("  #  line 7 \"a\\\\b.c\""),
            Some((7, Some("a\\b.c".into())))
        );
        assert_eq!(directive("#line 9"), Some((9, None)));
        assert_eq!(directive("#line x \"a\""), None);
        assert_eq!(directive("#line 9 \"open"), None);
    }

    #[test]
    fn two_regions_and_a_bare_renumbering_of_the_same_file() {
        let src = "a\n#line 100 \"f.c\"\nb\nc\n#line 200\nd\n#line 10 \"g.c\"\ne\n";
        let d = LineDirectives::parse(src, "t.c").unwrap().unwrap();
        assert_eq!(d.physical("f.c", 100), Some(3));
        assert_eq!(d.physical("f.c", 101), Some(4));
        assert_eq!(d.physical("f.c", 102), None, "line 5 is the next directive");
        assert_eq!(d.physical("f.c", 200), Some(6));
        assert_eq!(d.physical("g.c", 10), Some(8));
        assert_eq!(d.files(), ["f.c", "g.c"]);
    }

    #[test]
    fn an_ambiguous_presumed_line_is_not_translated() {
        let src = "#line 5 \"f.c\"\na\nb\n#line 5 \"f.c\"\nc\n";
        let d = LineDirectives::parse(src, "t.c").unwrap().unwrap();
        assert_eq!(d.physical("f.c", 5), None);
        assert_eq!(d.physical("f.c", 6), Some(3));
    }

    #[test]
    fn a_bare_renumbering_of_the_tu_itself_is_refused() {
        let e = LineDirectives::parse("a\n#line 40\nb\n", "t.c").unwrap_err();
        assert!(e.to_string().contains("t.c:2"), "{e}");
    }

    #[test]
    fn a_modules_object_is_its_lower_cased_basename() {
        assert_eq!(object_key(".\\Debug\\Big.obj"), "big.obj");
        assert_eq!(object_key("C:\\w\\app\\Release/Plot.obj"), "plot.obj");
        assert_eq!(object_key("x.obj"), "x.obj");
    }

    struct Product(PathBuf);
    impl Product {
        fn new(tag: &str, files: &[(&str, &str)]) -> Self {
            let p = std::env::temp_dir()
                .join(format!("msvc6-pdb-linedir-{tag}-{}", std::process::id()));
            let _ = std::fs::remove_dir_all(&p);
            for (f, text) in files {
                let at = p.join(f);
                std::fs::create_dir_all(at.parent().unwrap()).unwrap();
                msvc6_base::srcio::write(&at, text).unwrap();
            }
            std::fs::create_dir_all(&p).unwrap();
            Self(p)
        }
        fn tus(rows: &[(&str, &str)]) -> TuSet {
            let mut t = TuSet::default();
            for (o, tu) in rows {
                t.by_object
                    .entry(o.to_ascii_lowercase())
                    .or_default()
                    .insert((*tu).to_string());
            }
            t
        }
    }
    impl Drop for Product {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    fn none(_: &Path) -> Result<Vec<PathBuf>> {
        Ok(Vec::new())
    }

    #[test]
    fn an_object_with_two_tus_and_a_directive_in_one_is_refused() {
        let a = "a\n#line 100 \"C:\\\\orig\\\\A.cpp\"\nb\n";
        let p = Product::new(
            "two",
            &[
                ("c/p/src/A.cpp", a),
                ("c/p/src/b1.cpp", "int b1;\n"),
                ("c/p/src/b2.cpp", "int b2;\n"),
            ],
        );
        let r = Product::tus(&[
            ("A.obj", "c/p/src/A.cpp"),
            ("b.obj", "c/p/src/b1.cpp"),
            ("b.obj", "c/p/src/b2.cpp"),
        ]);
        let d = for_tus(&r, &p.0, &none).unwrap();
        assert_eq!(d.keys().collect::<Vec<_>>(), ["a.obj"]);
        assert_eq!(d["a.obj"].physical("C:\\orig\\A.cpp", 100), Some(3));
        assert_eq!(line_names(&d)["a.obj"], ["C:\\orig\\A.cpp"]);
        msvc6_base::srcio::write(
            p.0.join("c/p/src/b2.cpp"),
            "#line 7 \"C:\\\\orig\\\\b.cpp\"\nx\n",
        )
        .unwrap();
        let e = format!("{:#}", for_tus(&r, &p.0, &none).unwrap_err());
        assert!(e.contains("b.obj") && e.contains("2 TUs"), "{e}");
        assert!(
            e.contains("c/p/src/b1.cpp") && e.contains("c/p/src/b2.cpp"),
            "{e}"
        );
    }

    #[test]
    fn an_unreadable_tu_is_refused_naming_its_path() {
        let p = Product::new("unreadable", &[("c/p/keep.txt", "")]);
        let r = Product::tus(&[("gone.obj", "c/p/src/Gone.cpp")]);
        let e = format!("{:#}", for_tus(&r, &p.0, &none).unwrap_err());
        assert!(e.contains("gone.obj") && e.contains("cannot read"), "{e}");
        assert!(
            e.contains(&p.0.join("c/p/src/Gone.cpp").display().to_string()),
            "{e}"
        );
    }

    #[test]
    fn a_directive_in_an_included_header_is_refused() {
        let p = Product::new(
            "header",
            &[
                (
                    "c/p/src/T.cpp",
                    "#include \"Outer.h\"\n#include \"windows.h\"\nint t;\n",
                ),
                ("c/p/include/outer.h", "#include \"inner.h\"\n"),
                ("c/p/include/inner.h", "int i;\n"),
            ],
        );
        let inc = p.0.join("c/p/include");
        let search = move |_: &Path| -> Result<Vec<PathBuf>> { Ok(vec![inc.clone()]) };
        let r = Product::tus(&[("T.obj", "c/p/src/T.cpp")]);
        assert!(for_tus(&r, &p.0, &search).unwrap().is_empty());
        msvc6_base::srcio::write(
            p.0.join("c/p/include/inner.h"),
            "int i;\n#line 50 \"C:\\\\orig\\\\inner.h\"\nint j;\n",
        )
        .unwrap();
        let e = format!("{:#}", for_tus(&r, &p.0, &search).unwrap_err());
        assert!(
            e.contains("inner.h:2") && e.contains("c/p/src/T.cpp"),
            "{e}"
        );
    }
}
