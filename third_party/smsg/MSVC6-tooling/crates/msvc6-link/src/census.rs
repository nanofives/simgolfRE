use anyhow::{Result, bail};
use msvc6_image::coff::{Object, SCN_LNK_COMDAT};
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub const COUNTED: [&str; 3] = [".text", ".rdata", ".data"];

#[must_use]
pub fn rsp_objects(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    for line in text.lines() {
        let mut tok = String::new();
        let mut quoted = false;
        let mut toks = Vec::new();
        for ch in line.chars() {
            match ch {
                '"' => quoted = !quoted,
                c if c.is_whitespace() && !quoted => {
                    if !tok.is_empty() {
                        toks.push(std::mem::take(&mut tok));
                    }
                }
                c => tok.push(c),
            }
        }
        if !tok.is_empty() {
            toks.push(tok);
        }
        for t in toks {
            let option = t.starts_with('-')
                || (t.starts_with('/') && t.get(1..).is_some_and(|r| !r.contains('/')));
            if !option && t.to_ascii_lowercase().ends_with(".obj") {
                out.push(t);
            }
        }
    }
    out
}

#[must_use]
pub fn resolve_obj(dir: &Path, name: &str) -> Option<PathBuf> {
    let unix = name.replace('\\', "/");
    let mut chars = unix.chars();
    let drive = match (chars.next(), chars.next()) {
        (Some(d), Some(':')) if d.is_ascii_alphabetic() => Some(d.to_ascii_lowercase()),
        _ => None,
    };
    let Some(d) = drive else {
        let p = Path::new(&unix);
        return Some(if p.is_absolute() {
            p.to_path_buf()
        } else {
            dir.join(p)
        });
    };
    let rest = unix.get(2..).unwrap_or("").trim_start_matches('/');
    if d == 'z' {
        return Some(Path::new("/").join(rest));
    }
    let abs = std::fs::canonicalize(dir).unwrap_or_else(|_| dir.to_path_buf());
    for anc in abs.ancestors() {
        let dev = anc.join("dosdevices").join(format!("{d}:"));
        if dev.exists() {
            return Some(dev.join(rest));
        }
        if anc.file_name().and_then(|n| n.to_str()) == Some(&format!("drive_{d}")) {
            return Some(anc.join(rest));
        }
    }
    None
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Tally {
    pub class: String,
    pub objects: usize,
    pub comdats: usize,
    pub plain: usize,
    pub comdat_bytes: u64,
    pub plain_bytes: u64,
    pub unreadable: usize,
    pub uncounted: usize,
    pub uncounted_bytes: u64,
}

pub const OTHER_CODE_AND_DATA: [&str; 4] = [".text", ".rdata", ".data", ".bss"];

#[must_use]
pub fn uncounted_class(name: &str) -> bool {
    !COUNTED.contains(&name)
        && OTHER_CODE_AND_DATA.contains(&name.split('$').next().unwrap_or(name))
}

pub fn count_object(t: &mut Tally, d: &[u8]) {
    t.objects += 1;
    let Ok(o) = Object::parse(d) else {
        t.unreadable += 1;
        return;
    };
    for s in &o.sections {
        if uncounted_class(&s.name) {
            t.uncounted += 1;
            t.uncounted_bytes = t.uncounted_bytes.saturating_add(u64::from(s.size));
            continue;
        }
        if !COUNTED.contains(&s.name.as_str()) {
            continue;
        }
        let n = u64::try_from(o.section_data(s).len()).unwrap_or(u64::MAX);
        if s.characteristics & SCN_LNK_COMDAT != 0 {
            t.comdats += 1;
            t.comdat_bytes = t.comdat_bytes.saturating_add(n);
        } else {
            t.plain += 1;
            t.plain_bytes = t.plain_bytes.saturating_add(n);
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rule {
    pub class: String,
    pub patterns: Vec<String>,
}

pub fn parse_rule(spec: &str) -> Result<Rule> {
    let Some((class, pats)) = spec.split_once('=') else {
        bail!("--class wants CLASS=PATTERN[,PATTERN...], got `{spec}`");
    };
    let patterns: Vec<String> = pats
        .split(',')
        .map(|p| p.trim().to_ascii_lowercase())
        .filter(|p| !p.is_empty())
        .collect();
    if class.trim().is_empty() || patterns.is_empty() {
        bail!("--class `{spec}` names no class or no pattern");
    }
    Ok(Rule {
        class: class.trim().to_string(),
        patterns,
    })
}

#[must_use]
pub fn glob(pattern: &str, name: &str) -> bool {
    let (p, n): (Vec<char>, Vec<char>) = (pattern.chars().collect(), name.chars().collect());
    let (mut pi, mut ni) = (0usize, 0usize);
    let (mut star, mut mark) = (None::<usize>, 0usize);
    while ni < n.len() {
        match p.get(pi) {
            Some('*') => {
                star = Some(pi);
                mark = ni;
                pi += 1;
            }
            Some(c) if *c == '?' || Some(c) == n.get(ni) => {
                pi += 1;
                ni += 1;
            }
            _ => match star {
                Some(s) => {
                    pi = s + 1;
                    mark += 1;
                    ni = mark;
                }
                None => return false,
            },
        }
    }
    p.get(pi..).unwrap_or_default().iter().all(|c| *c == '*')
}

#[derive(Debug, Clone, Default)]
pub struct Classifier {
    pub rules: Vec<Rule>,
    pub fallback: String,
}

impl Classifier {
    #[must_use]
    pub fn class_of(&self, file: &str) -> String {
        let lower = file.to_ascii_lowercase();
        let stem = lower.strip_suffix(".obj").unwrap_or(&lower);
        self.rules
            .iter()
            .find(|r| r.patterns.iter().any(|p| glob(p, stem) || glob(p, &lower)))
            .map_or_else(|| self.fallback.clone(), |r| r.class.clone())
    }

    #[must_use]
    pub fn order(&self) -> Vec<String> {
        let mut v: Vec<String> = self.rules.iter().map(|r| r.class.clone()).collect();
        v.dedup();
        if !v.contains(&self.fallback) {
            v.push(self.fallback.clone());
        }
        v
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Census {
    pub listed: usize,
    pub rows: Vec<Tally>,
}

impl Census {
    #[must_use]
    pub fn total(&self) -> Tally {
        let mut t = Tally {
            class: "TOTAL".to_string(),
            objects: self.listed,
            ..Tally::default()
        };
        for r in &self.rows {
            t.comdats += r.comdats;
            t.plain += r.plain;
            t.comdat_bytes = t.comdat_bytes.saturating_add(r.comdat_bytes);
            t.plain_bytes = t.plain_bytes.saturating_add(r.plain_bytes);
            t.unreadable += r.unreadable;
            t.uncounted += r.uncounted;
            t.uncounted_bytes = t.uncounted_bytes.saturating_add(r.uncounted_bytes);
        }
        t
    }

    #[must_use]
    pub fn lines(&self, label: &str, rsp: &str) -> Vec<String> {
        let t = self.total();
        let row = |r: &Tally| {
            format!(
                "  {:<18} {:>7} {:>9} {:>13} {:>8} {:>13}",
                r.class, r.objects, r.comdats, r.comdat_bytes, r.plain, r.plain_bytes
            )
        };
        let mut out = vec![
            format!("{label}: COMDAT CENSUS over the objects `{rsp}' gives the LINK."),
            format!(
                "  POPULATION: {} `.obj' line(s), {} COMDAT section(s) and {} PLAIN section(s) named .text, .rdata \
                 or .data. {} object(s) unreadable.",
                self.listed, t.comdats, t.plain, t.unreadable
            ),
            format!(
                "  NOT COUNTED: {} code or data section(s) of another name (a .text$x funclet, a .bss, a grouped \
                 .data$x), {} byte(s).",
                t.uncounted, t.uncounted_bytes
            ),
            "  /INCREMENTAL:YES gives every COMDAT its own slot, so its cost scales with the COMDAT column; a plain \
             section never gets one. This is the link's input, not the image: a COMDAT /OPT:REF would discard is \
             counted."
                .to_string(),
            String::new(),
            format!(
                "  {:<18} {:>7} {:>9} {:>13} {:>8} {:>13}",
                "class", "objects", "COMDATs", "COMDAT bytes", "plain", "plain bytes"
            ),
        ];
        out.extend(self.rows.iter().map(row));
        out.push(row(&t));
        out
    }
}

pub fn census(objects: &[(String, Vec<u8>)], classifier: &Classifier) -> Census {
    let order = classifier.order();
    let mut by: BTreeMap<String, Tally> = BTreeMap::new();
    for (name, d) in objects {
        let base = name.rsplit(['/', '\\']).next().unwrap_or(name);
        let class = classifier.class_of(base);
        let t = by.entry(class.clone()).or_insert_with(|| Tally {
            class,
            ..Tally::default()
        });
        count_object(t, d);
    }
    let rank = |c: &str| order.iter().position(|o| o == c).unwrap_or(usize::MAX);
    let mut rows: Vec<Tally> = by.into_values().collect();
    rows.sort_by(|a, b| {
        rank(&a.class)
            .cmp(&rank(&b.class))
            .then(a.class.cmp(&b.class))
    });
    Census {
        listed: objects.len(),
        rows,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rsp_operands_are_read_and_options_skipped() {
        let rsp = "/nologo /dll /def:\"C:\\lk\\a b.def\" /out:x.dll\r\n\
                   C:\\lk\\ide\\Debug\\one.obj\r\n\
                   /tmp/lane/two.obj \"sp ace.obj\" rel.obj\r\n\
                   libcmtd.lib -opt:ref\r\n";
        assert_eq!(
            rsp_objects(rsp),
            vec![
                "C:\\lk\\ide\\Debug\\one.obj",
                "/tmp/lane/two.obj",
                "sp ace.obj",
                "rel.obj"
            ]
        );
    }

    #[test]
    fn operands_resolve_through_the_wine_prefix() {
        let root = std::env::temp_dir().join(format!("msvc6-link-census-{}", std::process::id()));
        let dir = root.join("wp/drive_c/lk/w");
        std::fs::create_dir_all(&dir).unwrap();
        let canon = std::fs::canonicalize(&root).unwrap();
        assert_eq!(
            resolve_obj(&dir, "C:\\lk\\ide\\a.obj"),
            Some(canon.join("wp/drive_c/lk/ide/a.obj"))
        );
        assert_eq!(
            resolve_obj(&dir, "Z:\\tmp\\b.obj"),
            Some(PathBuf::from("/tmp/b.obj"))
        );
        assert_eq!(resolve_obj(&dir, "c.obj"), Some(dir.join("c.obj")));
        assert_eq!(resolve_obj(&dir, "Q:\\x.obj"), None);
        std::fs::remove_dir_all(&root).ok();
    }

    #[test]
    fn the_first_rule_whose_pattern_matches_classes_a_name() {
        let c = Classifier {
            rules: vec![
                parse_rule("own carve=fn_*").unwrap(),
                parse_rule("blob=text_*,pool_*,idata").unwrap(),
            ],
            fallback: "other".to_string(),
        };
        assert_eq!(c.class_of("fn_00014c70.obj"), "own carve");
        assert_eq!(c.class_of("POOL_1.obj"), "blob");
        assert_eq!(c.class_of("idata.obj"), "blob");
        assert_eq!(c.class_of("mystery.obj"), "other");
        assert_eq!(c.order(), vec!["own carve", "blob", "other"]);
        assert!(parse_rule("x").is_err());
        assert!(parse_rule("x=").is_err());
        assert!(glob("a*b?d", "aXXbcd"));
        assert!(!glob("a*b?d", "aXXbd"));
        assert!(glob("*", ""));
    }

    #[test]
    fn a_funclet_or_bss_section_is_named_as_not_counted() {
        use msvc6_fixture::coff as fx;
        let mut o = fx::Object::new();
        for (name, characteristics, data, uninit) in [
            (".text", fx::SCN_TEXT | fx::SCN_COMDAT, vec![0xc3; 16], 0),
            (".text$x", fx::SCN_TEXT | fx::SCN_COMDAT, vec![0xc3; 8], 0),
            (".data", fx::SCN_DATA, vec![1; 4], 0),
            (".bss", fx::SCN_BSS, Vec::new(), 12),
            (".debug$S", fx::SCN_DEBUG, vec![0; 6], 0),
        ] {
            o.section(fx::Section {
                name: name.to_string(),
                characteristics,
                data,
                uninit,
                ..fx::Section::default()
            });
        }
        let c = census(&[("a.obj".to_string(), o.build())], &Classifier::default());
        let t = c.total();
        assert_eq!(
            (t.comdats, t.comdat_bytes, t.plain, t.plain_bytes),
            (1, 16, 1, 4)
        );
        assert_eq!((t.uncounted, t.uncounted_bytes), (2, 20));
        let lines = c.lines("IMG", "x.rsp");
        assert!(
            lines
                .iter()
                .any(|l| l.contains("NOT COUNTED: 2 code or data section(s)")
                    && l.contains("20 byte(s)")),
            "{lines:?}"
        );
        assert!(
            uncounted_class(".text$x") && uncounted_class(".bss") && uncounted_class(".data$r")
        );
        assert!(
            !uncounted_class(".text")
                && !uncounted_class(".debug$S")
                && !uncounted_class(".drectve")
        );
    }
}
