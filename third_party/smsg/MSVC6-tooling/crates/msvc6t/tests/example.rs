#[allow(clippy::indexing_slicing, dead_code)]
mod product {
    include!("example/product.rs");
}

#[cfg(test)]
mod t {
    use super::product::{
        Names, Product, Row, Sources, View, image_view, named_at, note_names, readable, rows_of,
    };
    use msvc6_base::srcio;
    use std::collections::BTreeMap;
    use std::path::{Path, PathBuf};
    use std::rc::Rc;

    const TEXT_KEYS: [&str; 3] = ["what", "evidence", "ceiling_evidence"];

    fn root() -> PathBuf {
        msvc6_lint::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR"))).unwrap()
    }

    fn configs() -> Vec<PathBuf> {
        let mut v: Vec<PathBuf> = std::fs::read_dir(root().join("examples"))
            .unwrap()
            .flatten()
            .map(|e| e.path().join("msvc6t.toml"))
            .filter(|p| p.is_file())
            .collect();
        v.sort();
        v
    }

    fn root_var(doc: &toml::Table) -> Option<String> {
        let r = doc.get("source")?.get("root")?.as_str()?;
        let v = r.strip_prefix("${")?.strip_suffix('}')?;
        (!v.contains(['$', '{', '}', ':'])).then(|| v.to_string())
    }

    fn rel_of(s: &str, var: &str) -> Option<String> {
        let rest = s.strip_prefix(&format!("${{{var}}}"))?;
        if rest.is_empty() {
            return Some(String::new());
        }
        let rest = rest.strip_prefix('/')?;
        (!rest.contains("${")).then(|| rest.to_string())
    }

    fn strs(v: Option<&toml::Value>) -> Vec<String> {
        match v {
            Some(toml::Value::String(s)) => vec![s.clone()],
            Some(toml::Value::Array(a)) => a
                .iter()
                .filter_map(|x| x.as_str().map(str::to_string))
                .collect(),
            _ => Vec::new(),
        }
    }

    fn tables(v: Option<&toml::Value>) -> Vec<toml::Table> {
        match v {
            Some(toml::Value::Array(a)) => a.iter().filter_map(|x| x.as_table().cloned()).collect(),
            Some(toml::Value::Table(t)) => vec![t.clone()],
            _ => Vec::new(),
        }
    }

    fn all_strings(v: &toml::Value, at: &str, out: &mut Vec<(String, String)>) {
        match v {
            toml::Value::String(s) => out.push((at.to_string(), s.clone())),
            toml::Value::Array(a) => {
                for (k, x) in a.iter().enumerate() {
                    all_strings(x, &format!("{at}[{k}]"), out);
                }
            }
            toml::Value::Table(t) => {
                for (k, x) in t {
                    all_strings(x, &format!("{at}.{k}"), out);
                }
            }
            _ => {}
        }
    }

    fn tsv_files(image: &toml::Table, key: &str) -> Vec<String> {
        strs(image.get(key))
            .into_iter()
            .filter_map(|f| f.strip_prefix("tsv:").map(str::to_string))
            .filter(|f| !f.contains('$'))
            .collect()
    }

    #[derive(Default)]
    struct Report {
        paths: usize,
        rows: usize,
        notes: usize,
        texts: usize,
        bad: Vec<String>,
    }

    impl Report {
        fn path(&mut self, p: &Product, at: &str, rel: &str) {
            self.paths += 1;
            let ok = if rel.contains(['*', '?']) {
                p.matches(rel) || p.matches(&format!("{rel}/**"))
            } else {
                p.has_path(rel)
            };
            if !ok {
                self.bad.push(format!("{at}: the path {rel:?}"));
            }
        }
    }

    fn check_paths(p: &Product, doc: &toml::Table, var: &str, r: &mut Report) {
        let mut every = Vec::new();
        for (k, v) in doc {
            all_strings(v, k, &mut every);
        }
        for (at, s) in &every {
            if let Some(rel) = rel_of(s, var) {
                r.path(p, at, &rel);
            }
        }
        if let Some(src) = doc.get("source").and_then(toml::Value::as_table) {
            for e in strs(src.get("exclude")) {
                r.path(p, "source.exclude", &e);
            }
            if let Some(layout) = src.get("layout").and_then(toml::Value::as_table) {
                for k in layout.keys() {
                    r.path(p, "source.layout", k);
                }
            }
            let prove = src.get("prove");
            for (k, tu) in tables(prove.and_then(|v| v.get("tu"))).iter().enumerate() {
                let at = format!("source.prove.tu[{k}]");
                for s in strs(tu.get("sources"))
                    .iter()
                    .chain(&strs(tu.get("source")))
                    .chain(&strs(tu.get("cwd")))
                {
                    r.path(p, &at, s);
                }
            }
            let guard = src.get("guard");
            for (k, g) in tables(guard.and_then(|v| v.get("image")))
                .iter()
                .enumerate()
            {
                for d in strs(g.get("dirs")) {
                    r.path(p, &format!("source.guard.image[{k}]"), &d);
                }
            }
        }
        let root_form = format!("${{{var}}}");
        for (k, image) in tables(doc.get("image")).iter().enumerate() {
            let at = format!("image[{k}]");
            if let Some(paths) = image
                .get("pdb")
                .and_then(|v| v.get("paths"))
                .and_then(toml::Value::as_table)
                .filter(|t| t.get("product").and_then(toml::Value::as_str) == Some(&root_form))
            {
                for lane in tables(paths.get("lanes")) {
                    for s in strs(lane.get("product")) {
                        r.path(p, &format!("{at}.pdb.paths.lanes"), &s);
                    }
                }
                for a in tables(paths.get("attested")) {
                    if a.get("output").and_then(toml::Value::as_bool) == Some(true) {
                        continue;
                    }
                    for s in strs(a.get("product")) {
                        r.path(p, &format!("{at}.pdb.paths.attested"), &s);
                    }
                }
                for f in tables(paths.get("files")) {
                    let d = strs(f.get("product")).concat();
                    r.path(p, &format!("{at}.pdb.paths.files"), &d);
                    for n in strs(f.get("names")) {
                        r.paths += 1;
                        if !p.has_path_folded(&format!("{d}/{n}")) {
                            r.bad.push(format!(
                                "{at}.pdb.paths.files: the file {d}/{n}, in any case"
                            ));
                        }
                    }
                }
            }
            if let Some(cv) = image
                .get("codeview")
                .and_then(toml::Value::as_table)
                .filter(|t| t.get("root").and_then(toml::Value::as_str) == Some(&root_form))
            {
                for s in strs(cv.get("headers"))
                    .iter()
                    .chain(&strs(cv.get("search")))
                {
                    r.path(p, &format!("{at}.codeview"), s);
                }
                for v in tables(cv.get("view")) {
                    for s in strs(v.get("projects")) {
                        r.path(p, &format!("{at}.codeview.view"), &s);
                    }
                }
            }
            if let Some(build) = image.get("build").and_then(toml::Value::as_table) {
                for st in tables(build.get("stage")) {
                    let Some(from) = st
                        .get("from")
                        .and_then(toml::Value::as_str)
                        .and_then(|f| rel_of(f, var))
                    else {
                        continue;
                    };
                    for pat in strs(st.get("files")) {
                        r.path(p, &format!("{at}.build.stage"), &format!("{from}/{pat}"));
                    }
                }
            }
        }
    }

    fn verdicts(r: &mut Report, table: &str, v: &super::product::Verdict) {
        for (row, why) in &v.left_out {
            r.bad
                .push(format!("{table}: {:#x} {}: {why}", row.rva, row.name));
        }
        for (row, n) in &v.renamed {
            r.bad.push(format!(
                "{table}: {:#x} {}: an ILT thunk whose target is named {n}",
                row.rva, row.name
            ));
        }
    }

    fn check_image(
        p: &Product,
        view: &View,
        dir: &Path,
        image: &toml::Table,
        at: &str,
        r: &mut Report,
    ) {
        let read = |f: &str| srcio::own_read(dir.join(f)).unwrap();
        let names = tsv_files(image, "names");
        let mut addresses: Vec<Row> = Vec::new();
        let mut earlier: Vec<Row> = Vec::new();
        if let Some((last, rest)) = names.split_last() {
            addresses = rows_of(&read(last));
            for f in rest {
                earlier.extend(rows_of(&read(f)));
            }
            r.rows += addresses.len() + earlier.len();
            let (va, vs) = view.reconcile(addresses.clone(), earlier.clone());
            verdicts(r, last, &va);
            verdicts(r, &rest.join(", "), &vs);
            for row in &addresses {
                let scope = row.rest.split('\t').nth(2).unwrap_or_default();
                if !scope.is_empty() && !view.names.own.has_stem(scope) {
                    r.bad.push(format!(
                        "{last}: {:#x} {}: the scope {scope}, no source file's stem",
                        row.rva, row.name
                    ));
                }
            }
            for row in addresses.iter().chain(&earlier) {
                let kind = row.rest.split('\t').next().unwrap_or_default();
                if view.ilt_target(row.rva).is_some() != (kind == "thunk") {
                    r.bad.push(format!(
                        "{:#x} {}: kind {kind}, where the reference holds {} ILT thunk",
                        row.rva,
                        row.name,
                        if kind == "thunk" { "no" } else { "an" }
                    ));
                }
            }
        }
        for (k, res) in tables(image.get("residue")).iter().enumerate() {
            let Some(note) = res.get("note").and_then(toml::Value::as_str) else {
                continue;
            };
            r.notes += 1;
            for f in p.absent_files(note) {
                r.bad.push(format!("{at}.residue[{k}]: the note names {f}"));
            }
            for (named, va) in note_names(note) {
                let want = va
                    .checked_sub(view.base)
                    .and_then(|rva| named_at(&addresses, &earlier, rva))
                    .map(|n| readable(&n))
                    .unwrap_or_default();
                if named != want {
                    r.bad.push(format!(
                        "{at}.residue[{k}]: the note names {named:?} at {va:#x}, the tables {want:?}"
                    ));
                }
            }
        }
        for f in tsv_files(image, "dropped") {
            for (line, l) in read(&f).lines().enumerate().skip(1) {
                let (name, why) = l.split_once('\t').unwrap_or((l, ""));
                r.rows += 1;
                let problems = view.names.name_problems(name);
                if !problems.is_empty() {
                    r.bad
                        .push(format!("{f}:{}: {name}: {}", line + 1, problems.join("; ")));
                }
                for s in p.absent_files(why) {
                    r.bad
                        .push(format!("{f}:{}: {name}: its reason names {s}", line + 1));
                }
            }
        }
        if let Some(classes) = image.get("classes").and_then(toml::Value::as_table) {
            for c in tables(classes.get("counts")) {
                for n in strs(c.get("class")) {
                    r.rows += 1;
                    if !view.names.own.class_defined(&n) {
                        r.bad.push(format!(
                            "{at}.classes.counts: {n}, a class the image's sources do not define"
                        ));
                    }
                }
            }
        }
    }

    fn check_texts(p: &Product, doc: &toml::Table, views: &BTreeMap<usize, View>, r: &mut Report) {
        let mut every = Vec::new();
        for (k, v) in doc {
            all_strings(v, k, &mut every);
        }
        for (at, s) in &every {
            let key = at.rsplit('.').next().unwrap_or(at);
            let key = key.split('[').next().unwrap_or(key);
            if !TEXT_KEYS.contains(&key) {
                continue;
            }
            r.texts += 1;
            for f in p.absent_files(s) {
                r.bad.push(format!("{at}: {key} names {f}"));
            }
            let image = at
                .strip_prefix("image[")
                .and_then(|x| x.split(']').next())
                .and_then(|n| n.parse::<usize>().ok());
            let quoted: Vec<String> = match image.and_then(|k| views.get(&k)) {
                Some(v) => v.names.absent_quoted(s),
                None => Vec::new(),
            };
            for q in quoted {
                r.bad.push(format!(
                    "{at}: {key} quotes {q}, in none of the image's own sources"
                ));
            }
        }
    }

    #[test]
    fn every_path_and_name_an_example_gives_its_product_is_in_that_product() {
        let mut checked = 0usize;
        for cfg in configs() {
            let doc: toml::Table = toml::from_str(&srcio::own_read(&cfg).unwrap()).unwrap();
            let dir = cfg.parent().unwrap();
            let example = dir.display().to_string();
            let Some(var) = root_var(&doc) else {
                println!("SKIP {example}: its [source] names no `root = \"${{VAR}}\"`");
                continue;
            };
            let Some(product_dir) = std::env::var_os(&var).filter(|v| !v.is_empty()) else {
                println!("SKIP {example}: set {var} to a checkout of its product");
                continue;
            };
            let product_dir = PathBuf::from(product_dir);
            let p = Product::read(&product_dir).unwrap();
            let project = msvc6_project::config::Project::load(&cfg).unwrap();
            let mut r = Report::default();
            check_paths(&p, &doc, &var, &mut r);
            let mut libraries = BTreeMap::new();
            let mut views = BTreeMap::new();
            for (k, image) in tables(doc.get("image")).iter().enumerate() {
                if tsv_files(image, "names").is_empty() && tsv_files(image, "dropped").is_empty() {
                    continue;
                }
                let id = image.get("id").and_then(toml::Value::as_str).unwrap();
                let view = image_view(&project, id, &product_dir, &mut libraries)
                    .unwrap_or_else(|e| panic!("{example}: image {id}: {e}"));
                check_image(&p, &view, dir, image, &format!("image[{k}]"), &mut r);
                views.insert(k, view);
            }
            check_texts(&p, &doc, &views, &mut r);
            println!(
                "{example}: {} product files; {} paths, {} table rows, {} notes, {} texts: {} absent or inconsistent",
                p.files().len(),
                r.paths,
                r.rows,
                r.notes,
                r.texts,
                r.bad.len()
            );
            for b in &r.bad {
                println!("BAD {b}");
            }
            assert!(
                r.bad.is_empty(),
                "{example} gives {} path(s) or name(s) {var} does not hold, or rows its tables contradict",
                r.bad.len()
            );
            checked += 1;
        }
        println!("{checked} example(s) checked against their product");
    }

    fn scratch(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("msvc6t-example-{tag}-{}", std::process::id()));
        if d.exists() {
            std::fs::remove_dir_all(&d).unwrap();
        }
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn put(d: &Path, rel: &str, text: &str) {
        let p = d.join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        srcio::own_write(&p, text).unwrap();
    }

    const DSB: &str = "enum SNDERR { SND_OK };\n\
                       struct IBuffer { int Play(int, int, unsigned long); };\n\
                       class CDSB {\n\
                       public:\n    SNDERR play(unsigned long flags);\n    void stop();\n    ~CDSB();\n\
                       private:\n    IBuffer *m_lpdsb;\n};\n\
                       SNDERR CDSB::play(unsigned long flags) { m_lpdsb->Play(0, 0, flags); return SND_OK; }\n\
                       static const char *s_names[] = { \"Voice Rx\", \"Voice Tx\" };\n\
                       int Button;\n\
                       class BaseButton { public: virtual ~BaseButton(); };\n";

    fn names_of(d: &Path, dirs: &[&str]) -> Names {
        let dirs: Vec<String> = dirs.iter().map(|s| (*s).to_string()).collect();
        Names::new(Sources::read(d, &dirs, false).unwrap(), Rc::default(), &[])
    }

    #[test]
    fn a_name_needs_its_class_to_declare_it_in_the_image_s_own_sources() {
        let d = scratch("names");
        put(&d, "img/dsb.cpp", DSB);
        put(
            &d,
            "img/sprite.h",
            "class JGLSprite { public: virtual ~JGLSprite(); };\n",
        );
        put(
            &d,
            "other/sprite.h",
            "class Sprite { public: virtual ~Sprite(); };\n",
        );
        let n = names_of(&d, &["img"]);
        assert!(n.name_problems("?play@CDSB@@QAE?AW4SNDERR@@K@Z").is_empty());
        assert!(n.name_problems("??1CDSB@@QAE@XZ").is_empty());
        assert_eq!(
            n.name_problems("?Play@CDSB@@QAE?AW4SNDERR@@H@Z"),
            ["CDSB::Play, a member CDSB does not declare"]
        );
        assert_eq!(
            n.name_problems("??1Voice@@QAE@XZ"),
            ["Voice, in none of the image's own sources"]
        );
        assert_eq!(
            n.name_problems("??1Sprite@@UAE@XZ"),
            ["Sprite, in none of the image's own sources"]
        );
        assert_eq!(
            n.name_problems("??1Button@@UAE@XZ"),
            ["Button, a class the image's sources do not define"]
        );
        assert!(n.name_problems("??1BaseButton@@UAE@XZ").is_empty());
        assert!(n.name_problems("??_7BaseButton@@6B@").is_empty());
        assert!(n.name_problems("?stop@CDSB@@$4PPPPPPPM@A@AEXXZ").is_empty());
        assert_eq!(
            n.name_problems("?halt@CDSB@@$4PPPPPPPM@A@AEXXZ"),
            ["halt, in none of the image's own sources"]
        );
        let wider = names_of(&d, &["img", "other"]);
        assert!(wider.name_problems("??1Sprite@@UAE@XZ").is_empty());
        assert_eq!(readable("?play@CDSB@@QAE?AW4SNDERR@@K@Z"), "CDSB::play");
        assert_eq!(readable("_SetRect@16"), "SetRect");
        std::fs::remove_dir_all(&d).unwrap();
    }

    const IMPORTS: &str = "struct Rect { int l, t, r, b; };\n\
                           extern \"C\" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);\n\
                           extern \"C\" {\n    int __stdcall IntersectRect(Rect *, const Rect *, const Rect *);\n}\n\
                           namespace jgl {\n    int __stdcall Helper(int);\n}\n\
                           int __stdcall Spare(int);\n\
                           void SetRect(Rect *r) { FreeLibrary(0); GetAsyncKeyState(1); }\n";

    #[test]
    fn a_decorated_import_needs_its_function_declared_with_cpp_linkage() {
        let d = scratch("imports");
        put(&d, "img/screen.h", IMPORTS);
        put(
            &d,
            "img/legacy.c",
            "int __cdecl c_only(int x) { return x; }\n",
        );
        let n = names_of(&d, &["img"]);
        let p = n
            .import_spelling("__imp_?IntersectRect@@YGHPAURect@fn_00075b20@@PBU12@1@Z")
            .unwrap();
        assert_eq!(p, "fn_00075b20, in none of the image's own sources");
        assert_eq!(
            n.import_spelling("__imp_?GetAsyncKeyState@@YGFH@Z")
                .unwrap(),
            "GetAsyncKeyState, a function the image's sources give C linkage"
        );
        assert_eq!(
            n.import_spelling("__imp_?IntersectRect@@YGHPAURect@@PBU1@1@Z")
                .unwrap(),
            "IntersectRect, a function the image's sources give C linkage"
        );
        assert_eq!(
            n.import_spelling("__imp_?FreeLibrary@@YGHPAX@Z").unwrap(),
            "a C++ spelling of FreeLibrary, which the image's sources do not declare with C++ linkage"
        );
        assert!(n.import_spelling("__imp_?Spare@@YGHH@Z").is_none());
        assert!(n.import_spelling("__imp__GetAsyncKeyState@4").is_none());
        assert_eq!(
            n.name_problems("_j_SetRect"),
            ["_j_SetRect, in none of the image's own sources"]
        );
        assert!(n.name_problems("_SetRect").is_empty());
        assert!(n.import_spelling("__imp_?Helper@jgl@@YGHH@Z").is_none());
        assert_eq!(
            n.import_spelling("__imp_?Helper@@YGHH@Z").unwrap(),
            "a C++ spelling of Helper, which the image's sources do not declare with C++ linkage"
        );
        assert_eq!(
            n.import_spelling("__imp_?c_only@@YAHH@Z").unwrap(),
            "c_only, a function the image's sources give C linkage"
        );
        assert!(n.name_problems("_c_only").is_empty());
        std::fs::remove_dir_all(&d).unwrap();
    }

    fn import_image() -> (msvc6_image::pe::Image, Vec<u32>) {
        use msvc6_fixture::pe::{DIR_IAT, DIR_IMPORT, Pe, SCN_CODE, SCN_RDATA};
        use msvc6_fixture::tables::{Dll, idata};
        let dlls = [
            Dll::named("USER32.dll", &["IntersectRect", "SetRect"]),
            Dll::named("Landscape.dll", &["?getInstance@Landscape@@SAPAV1@XZ"]),
        ];
        let id = idata(0x2000, &dlls, &|_| 0);
        let slots = vec![id.iat[0], id.iat[0] + 4, id.iat[1]];
        let mut text = Vec::new();
        for s in &slots {
            text.extend_from_slice(&[0xff, 0x25]);
            text.extend_from_slice(&(0x1000_0000 + s).to_le_bytes());
        }
        text.resize(0x40, 0xc3);
        let mut pe = Pe::dll();
        pe.push(".text", SCN_CODE, text);
        let at = pe.push(".rdata", SCN_RDATA, id.bytes.clone());
        assert_eq!(at, 0x2000);
        pe.dir(DIR_IMPORT, id.import_dir.0, id.import_dir.1);
        pe.dir(DIR_IAT, id.iat_dir.0, id.iat_dir.1);
        (msvc6_image::pe::Image::parse(pe.build()).unwrap(), slots)
    }

    #[test]
    fn an_iat_row_and_a_stub_row_spell_their_import_as_its_library_does() {
        let d = scratch("iat");
        put(
            &d,
            "img/land.h",
            "struct Rect { int l, t, r, b; };\nclass Landscape { public: static Landscape *getInstance(void); };\n\
             void Draw(Rect *r, Rect *s) { IntersectRect(r, s, s); }\n",
        );
        let (image, slots) = import_image();
        let mut lib = msvc6_fixture::archive::Library::default();
        lib.member(
            "USER32.dll",
            Vec::new(),
            &[
                "__imp__IntersectRect@12",
                "_IntersectRect@12",
                "__imp__SetRect@20",
                "_SetRect@20",
            ],
        );
        let view = View::new(image, names_of(&d, &["img"]), None)
            .unwrap()
            .with_import_libs(&[lib.build()]);
        assert_eq!(
            view.import_stub(0x1000).map(String::as_str),
            Some("IntersectRect")
        );
        assert!(
            view.import_problem(slots[0], "__imp__IntersectRect@12")
                .is_none()
        );
        assert_eq!(
            view.import_problem(slots[0], "__imp__IntersectRect@16")
                .unwrap(),
            "__imp__IntersectRect@16, a symbol of no import library the image's build links"
        );
        assert!(
            view.import_problem(slots[2], "__imp_?getInstance@Landscape@@SAPAV1@XZ")
                .is_none()
        );
        assert_eq!(
            view.import_problem(slots[2], "__imp_?getInstance@Landscape@@SAPAV1@H@Z")
                .unwrap(),
            "the reference's IAT slot here imports ?getInstance@Landscape@@SAPAV1@XZ, which it does not spell"
        );
        assert!(
            view.stub_problem(0x100c, "?getInstance@Landscape@@SAPAV1@XZ")
                .is_none()
        );
        assert!(
            view.stub_problem(0x100c, "?getInstance@Landscape@@SAPAV1@H@Z")
                .is_some()
        );
        assert!(view.stub_problem(0x1000, "_IntersectRect@12").is_none());
        assert!(
            view.stub_problem(0x1000, "?IntersectRect@@YGHPAURect@@PBU1@1@Z")
                .is_some()
        );
        assert!(view.stub_problem(0x1006, "_SetRect@16").is_some());
        let (_, s) = view.reconcile(
            Vec::new(),
            vec![
                row(0x1000, "?IntersectRect@@YGHPAURect@@PBU1@1@Z"),
                row(0x100c, "?getInstance@Landscape@@SAPAV1@H@Z"),
                row(0x1006, "_SetRect@20"),
            ],
        );
        let kept: Vec<&str> = s.kept.iter().map(|r| r.name.as_str()).collect();
        assert_eq!(kept, ["_SetRect@20"]);
        std::fs::remove_dir_all(&d).unwrap();
    }

    fn thunk_image() -> msvc6_image::pe::Image {
        let mut text = Vec::new();
        for (at, to) in [(0u32, 0x20u32), (5, 0x30), (10, 0x40)] {
            text.push(0xe9);
            let rel = i32::try_from(to).unwrap() - i32::try_from(at + 5).unwrap();
            text.extend_from_slice(&rel.to_le_bytes());
        }
        text.resize(0x50, 0xc3);
        let mut pe = msvc6_fixture::pe::Pe::dll();
        pe.push(".text", msvc6_fixture::pe::SCN_CODE, text);
        msvc6_image::pe::Image::parse(pe.build()).unwrap()
    }

    fn row(rva: u32, name: &str) -> Row {
        Row {
            rva,
            name: name.to_string(),
            rest: "label".to_string(),
        }
    }

    #[test]
    fn a_table_row_agrees_with_its_thunk_s_target_the_addresses_and_the_globals() {
        let d = scratch("reconcile");
        put(&d, "img/dsb.cpp", &format!("{DSB}int g_x;\nint g_y;\n"));
        let view = View::new(
            thunk_image(),
            names_of(&d, &["img"]),
            Some("address,name,type\n0x10002000,?g_x@@3HA,global\n"),
        )
        .unwrap();
        assert_eq!(view.ilt_target(0x1000), Some(0x1020));
        assert_eq!(view.ilt_target(0x1020), None);
        let addresses = vec![
            row(0x1020, "?play@CDSB@@QAE?AW4SNDERR@@K@Z"),
            row(0x1030, "?stop@CDSB@@QAEXXZ"),
        ];
        let symbols = vec![
            row(0x1000, "?Play@CDSB@@QAE?AW4SNDERR@@H@Z"),
            row(0x1005, "?stop@CDSB@@QAEXXZ"),
            row(0x100a, "??1CDSB@@QAE@XZ"),
            row(0x1030, "??1CDSB@@QAE@XZ"),
            row(0x2000, "?g_y@@3HA"),
            row(0x2000, "?g_x@@3HA"),
        ];
        let (a, s) = view.reconcile(addresses, symbols);
        assert!(a.left_out.is_empty() && a.renamed.is_empty(), "{a:?}");
        assert_eq!(
            s.renamed
                .iter()
                .map(|(r, n)| (r.rva, n.as_str()))
                .collect::<Vec<_>>(),
            [(0x1000, "?play@CDSB@@QAE?AW4SNDERR@@K@Z")]
        );
        let left: Vec<(u32, &str)> = s
            .left_out
            .iter()
            .map(|(r, _)| (r.rva, r.name.as_str()))
            .collect();
        assert_eq!(
            left,
            [
                (0x1030, "??1CDSB@@QAE@XZ"),
                (0x2000, "?g_y@@3HA"),
                (0x100a, "??1CDSB@@QAE@XZ")
            ]
        );
        let kept: Vec<(u32, &str)> = s.kept.iter().map(|r| (r.rva, r.name.as_str())).collect();
        assert_eq!(
            kept,
            [
                (0x2000, "?g_x@@3HA"),
                (0x1000, "?play@CDSB@@QAE?AW4SNDERR@@K@Z"),
                (0x1005, "?stop@CDSB@@QAEXXZ")
            ]
        );
        let notes = super::product::note_names(
            "`nop' in CDSB::play@0x10001020's align pad (after @0x10001030)",
        );
        assert_eq!(
            notes,
            [
                ("CDSB::play".to_string(), 0x1000_1020),
                (String::new(), 0x1000_1030)
            ]
        );
        assert!(p_files_absent(&d));
        std::fs::remove_dir_all(&d).unwrap();
    }

    fn p_files_absent(d: &Path) -> bool {
        let p = Product::read(d).unwrap();
        p.absent_files("dsb.cpp: and gone.cpp, DSB.CPP") == ["gone.cpp"]
            && p.has_path("img")
            && !p.has_path("im")
            && p.matches("img/*.cpp")
    }
}
