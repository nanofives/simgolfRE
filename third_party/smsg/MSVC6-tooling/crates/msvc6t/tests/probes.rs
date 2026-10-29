#[cfg(test)]
mod tests {
    use std::collections::{BTreeMap, BTreeSet};
    use std::path::{Path, PathBuf};
    use std::process::Command;

    const BIN: &str = env!("CARGO_BIN_EXE_msvc6t");

    const HEADER: &str = "probe\tsource\ttoolchain\tcl\tfront\tc2\tlink\tstatus\tgate";

    const GOLD: &str = "8168";

    const SERVICE_PACKS: [&str; 3] = ["8447", "8799", "8966"];

    fn root() -> PathBuf {
        msvc6_lint::tree::workspace_root(Path::new(env!("CARGO_MANIFEST_DIR"))).unwrap()
    }

    fn probe_names() -> BTreeSet<String> {
        std::fs::read_dir(root().join("probes"))
            .unwrap()
            .flatten()
            .map(|e| e.path())
            .filter(|p| p.extension().is_some_and(|x| x == "expect"))
            .map(|p| p.file_stem().unwrap().to_string_lossy().into_owned())
            .collect()
    }

    fn il_sources() -> usize {
        std::fs::read_dir(root().join("probes/il"))
            .unwrap()
            .flatten()
            .map(|e| e.path())
            .filter(|p| {
                p.extension()
                    .is_some_and(|x| x == "expect" || x == "c" || x == "cpp" || x == "h")
            })
            .count()
    }

    #[test]
    fn every_probe_reads_and_its_source_is_comment_free() {
        let out = Command::new(BIN)
            .args(["probe", "--check"])
            .current_dir(root())
            .output()
            .unwrap();
        let text = msvc6_base::srcio::tool_text(&out.stdout);
        assert!(
            out.status.success(),
            "{text}{}",
            msvc6_base::srcio::tool_text(&out.stderr)
        );
        assert!(
            text.contains(&format!("{} probe(s), 0 broken", probe_names().len())),
            "{text}"
        );
        for f in std::fs::read_dir(root().join("probes")).unwrap().flatten() {
            let p = f.path();
            if p.extension().is_some_and(|x| x == "c" || x == "cpp") {
                let lx = msvc6_source::lex::lex(&std::fs::read(&p).unwrap());
                assert!(lx.comments.is_empty(), "{}: a comment", p.display());
                assert!(lx.problems.is_empty(), "{}: {:?}", p.display(), lx.problems);
            }
        }
    }

    #[test]
    fn src_strip_check_finds_no_comment_in_the_probes() {
        let d = std::env::temp_dir().join(format!("msvc6t-probe-strip-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        let cfg = d.join("msvc6t.toml");
        msvc6_base::srcio::own_write(
            &cfg,
            format!(
                "schema = 1\n[source]\nroot = '{}'\nsyntax = {{ \"*.expect\" = \"hash-line\" }}\n",
                root().join("probes").display()
            ),
        )
        .unwrap();
        let out = Command::new(BIN)
            .args(["src", "strip", "--check", "--config"])
            .arg(&cfg)
            .output()
            .unwrap();
        let text = msvc6_base::srcio::tool_text(&out.stdout);
        assert!(
            out.status.success(),
            "{text}{}",
            msvc6_base::srcio::tool_text(&out.stderr)
        );
        let files = probe_names().len() * 2 + 1 + il_sources();
        assert!(
            text.contains(&format!("strip ok: {files} file(s) with a comment syntax")),
            "{text}"
        );
        assert!(text.contains(" 0 comment(s)"), "{text}");
        std::fs::remove_dir_all(&d).unwrap();
    }

    #[derive(Debug)]
    struct Row {
        probe: String,
        front: String,
        cl: String,
        c2: String,
        link: String,
        status: String,
    }

    fn results() -> Vec<Row> {
        let text = msvc6_base::srcio::own_read(root().join("probes/results.tsv")).unwrap();
        let mut lines = text.lines();
        assert_eq!(
            lines.next(),
            Some(HEADER),
            "probes/results.tsv has another header"
        );
        lines
            .map(|l| {
                let f: Vec<&str> = l.split('\t').collect();
                assert_eq!(f.len(), 9, "a short row: {l}");
                Row {
                    probe: f[0].to_string(),
                    cl: f[3].to_string(),
                    front: f[4].to_string(),
                    c2: f[5].to_string(),
                    link: f[6].to_string(),
                    status: f[7].to_string(),
                }
            })
            .collect()
    }

    #[test]
    fn the_probe_config_loads_strictly_and_names_every_toolchain_the_results_list() {
        let cfg = root().join("probes/msvc6t.toml");
        let p = msvc6_project::config::Project::load(&cfg).unwrap();
        let declared: BTreeSet<String> = p.toolchains.keys().cloned().collect();
        let text = msvc6_base::srcio::own_read(root().join("probes/results.tsv")).unwrap();
        let used: BTreeSet<String> = text
            .lines()
            .skip(1)
            .filter_map(|l| l.split('\t').nth(2))
            .map(str::to_string)
            .collect();
        assert_eq!(
            used, declared,
            "probes/results.tsv was run on other toolchains than probes/msvc6t.toml declares"
        );
    }

    #[test]
    fn the_results_record_every_probe_passing_on_gold_and_the_service_packs_run() {
        let rows = results();
        let names = probe_names();
        let listed: BTreeSet<String> = rows.iter().map(|r| r.probe.clone()).collect();
        assert_eq!(
            listed, names,
            "probes/results.tsv lists other probes than probes/ holds"
        );
        let mut gold: BTreeMap<&str, &str> = BTreeMap::new();
        let mut packs: BTreeMap<&str, BTreeSet<&str>> = BTreeMap::new();
        for r in &rows {
            assert!(
                ["PASS", "FAIL", "XFAIL", "XPASS", "ERROR", "SKIP"].contains(&r.status.as_str()),
                "{r:?}"
            );
            if [&r.cl, &r.front, &r.c2, &r.link]
                .iter()
                .all(|b| b.as_str() == GOLD)
            {
                gold.insert(&r.probe, &r.status);
            }
            if SERVICE_PACKS.contains(&r.c2.as_str()) && r.status != "ERROR" && r.status != "SKIP" {
                packs.entry(&r.probe).or_default().insert(&r.c2);
            }
        }
        for n in &names {
            let s = gold.get(n.as_str()).copied();
            assert!(
                matches!(s, Some("PASS" | "XFAIL")),
                "probe {n}: its gold 8168 row is {s:?}, not PASS"
            );
            let have = packs.get(n.as_str()).cloned().unwrap_or_default();
            for sp in SERVICE_PACKS {
                assert!(have.contains(sp), "probe {n}: no result on C2 {sp}");
            }
        }
    }
}
