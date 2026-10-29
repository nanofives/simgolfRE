use crate::c1count::{self, Count};
use anyhow::{Context as _, Result};
use msvc6_base::srcio;
use std::cell::{Cell, RefCell};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};
use std::rc::Rc;

pub const MAX_FILES: usize = 4096;

pub const MAX_FILE_BYTES: u64 = 64 << 20;

fn read_text(p: &Path) -> Result<String> {
    let len = std::fs::metadata(p)
        .with_context(|| format!("{}", p.display()))?
        .len();
    if len > MAX_FILE_BYTES {
        anyhow::bail!(
            "{}: {len} bytes, more than the {MAX_FILE_BYTES} a source the model prices may hold",
            p.display()
        );
    }
    srcio::read(p).with_context(|| format!("{}", p.display()))
}

fn shown(p: &Path, c: &Count, out: &mut Vec<String>) {
    let kinds: Vec<String> = c.by_kind.iter().map(|(k, v)| format!("{k} {v}")).collect();
    out.push(format!(
        "{}: {} (+1 when C1 first opens the file)",
        p.display(),
        c.total
    ));
    out.push(format!("  {}", kinds.join(", ")));
    if !c.includes.is_empty() {
        out.push(format!(
            "  includes (+1 each on first open, plus their own cost): {}",
            c.includes.join(" ")
        ));
    }
    if !c.unmodelled.is_empty() {
        out.push(format!(
            "  UNMODELLED, not priced: {}",
            c.unmodelled.join(" ")
        ));
    }
}

pub fn predict(header: &Path, with: Option<&Path>, gx: bool) -> Result<Vec<String>> {
    let mut out = Vec::new();
    let a = c1count::count_with(&read_text(header)?, gx)
        .with_context(|| header.display().to_string())?;
    shown(header, &a, &mut out);
    if let Some(w) = with {
        let b = c1count::count_with(&read_text(w)?, gx).with_context(|| w.display().to_string())?;
        shown(w, &b, &mut out);
        let d = b.total - a.total;
        out.push(format!(
            "shift {d:+} (mod 64 = {}): every function after the header in every includer",
            d.rem_euclid(64)
        ));
    }
    Ok(out)
}

#[derive(Debug, Clone)]
struct Opened {
    path: PathBuf,
    depth: usize,
    own: i64,
    before_key: bool,
    key_pre: Option<i64>,
    words: BTreeSet<String>,
    unmodelled: Vec<String>,
}

#[must_use]
pub fn include_lines(text: &str) -> Vec<(usize, String, bool)> {
    text.lines()
        .enumerate()
        .filter_map(|(i, l)| {
            let l = l.trim_start().strip_prefix('#')?.trim_start();
            let l = l.strip_prefix("include")?.trim_start();
            let quoted = l.starts_with('"');
            let name = l
                .trim_start_matches(['"', '<'])
                .split(['"', '>'])
                .next()?
                .to_string();
            Some((i + 1, name, quoted))
        })
        .collect()
}

fn prefix_cost(text: &str, line: usize) -> Result<i64> {
    let pre: Vec<&str> = text.lines().take(line.saturating_sub(1)).collect();
    Ok(c1count::count(&pre.join("\n"))?.total)
}

#[must_use]
pub fn key_line(text: &str, key: &str) -> Option<usize> {
    let s = c1count::strip_comments(text);
    s.lines().enumerate().find_map(|(i, l)| {
        if l.trim_start().starts_with('#') {
            return None;
        }
        let hit = l
            .split(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
            .any(|w| w == key);
        hit.then_some(i + 1)
    })
}

#[must_use]
pub fn find_in(dir: &Path, name: &str) -> Option<PathBuf> {
    let p = dir.join(name);
    if p.is_file() {
        return Some(p);
    }
    let (sub, leaf) = match name.rsplit_once(['/', '\\']) {
        Some((s, l)) => (dir.join(s), l),
        None => (dir.to_path_buf(), name),
    };
    std::fs::read_dir(sub).ok()?.find_map(|e| {
        let p = e.ok()?.path();
        (p.file_name()?.to_string_lossy().eq_ignore_ascii_case(leaf) && p.is_file()).then_some(p)
    })
}

pub const MAX_OPENS: usize = 1 << 20;

#[derive(Debug)]
struct Parsed {
    text: String,
    total: i64,
    includes: Vec<String>,
    unmodelled: Vec<String>,
    words: BTreeSet<String>,
}

fn parsed(text: String, at: &Path) -> Result<Parsed> {
    let c = c1count::count(&text).with_context(|| at.display().to_string())?;
    Ok(Parsed {
        words: c1count::live_words(&text),
        total: c.total,
        includes: c.includes,
        unmodelled: c.unmodelled,
        text,
    })
}

#[derive(Debug)]
pub struct Shared {
    files: RefCell<BTreeMap<PathBuf, Rc<Parsed>>>,
    opens: Cell<usize>,
    max_opens: usize,
}

impl Shared {
    #[must_use]
    pub fn new(max_opens: usize) -> Self {
        Self {
            files: RefCell::new(BTreeMap::new()),
            opens: Cell::new(0),
            max_opens,
        }
    }

    #[must_use]
    pub fn opens(&self) -> usize {
        self.opens.get()
    }

    fn file(&self, canon: &Path, f: &Path) -> Result<Rc<Parsed>> {
        if let Some(p) = self.files.borrow().get(canon) {
            return Ok(Rc::clone(p));
        }
        let text = match std::fs::metadata(f) {
            Ok(m) if m.len() > MAX_FILE_BYTES => read_text(f)?,
            Ok(_) => read_text(f).unwrap_or_default(),
            Err(_) => String::new(),
        };
        let p = Rc::new(parsed(text, f)?);
        self.files
            .borrow_mut()
            .insert(canon.to_path_buf(), Rc::clone(&p));
        Ok(p)
    }
}

struct Walk<'s> {
    shared: &'s Shared,
    dirs: Vec<PathBuf>,
    key: Option<String>,
    drop: BTreeSet<(PathBuf, String)>,
    root_text: Option<String>,
    seen: BTreeSet<PathBuf>,
    opens: Vec<Opened>,
    key_hit: bool,
    key_file: Option<PathBuf>,
}

impl Walk<'_> {
    fn open(&mut self, f: &Path, depth: usize) -> Result<()> {
        let canon = f.canonicalize().unwrap_or_else(|_| f.to_path_buf());
        if !self.seen.insert(canon.clone()) {
            return Ok(());
        }
        let n = self.shared.opens.get().saturating_add(1);
        self.shared.opens.set(n);
        let bound = if self.opens.len() >= MAX_FILES {
            Some(format!("{MAX_FILES} files in one closure"))
        } else if depth > crate::budget::NESTING as usize {
            Some(format!("{} include levels", crate::budget::NESTING))
        } else if n > self.shared.max_opens {
            Some(format!("{} opens in all", self.shared.max_opens))
        } else {
            None
        };
        if let Some(b) = bound {
            anyhow::bail!("the include walks passed {b}, at {}", f.display());
        }
        let file = match (self.opens.is_empty(), self.root_text.take()) {
            (true, Some(t)) => Rc::new(parsed(t, f)?),
            _ => self.shared.file(&canon, f)?,
        };
        let text = file.text.as_str();
        let idx = self.opens.len();
        let kl = match &self.key {
            Some(k) if !self.key_hit => key_line(text, k),
            _ => None,
        };
        self.opens.push(Opened {
            path: canon.clone(),
            depth,
            own: file.total,
            before_key: !self.key_hit,
            key_pre: None,
            words: file.words.clone(),
            unmodelled: file.unmodelled.clone(),
        });
        let here = f.parent().map(Path::to_path_buf).unwrap_or_default();
        for (line, name, quoted) in include_lines(text) {
            if !file.includes.contains(&name) || self.drop.contains(&(canon.clone(), name.clone()))
            {
                continue;
            }
            if let Some(k) = kl
                && line > k
                && !self.key_hit
            {
                self.key_hit = true;
                self.key_file = Some(canon.clone());
                let pre = prefix_cost(text, k)?;
                if let Some(o) = self.opens.get_mut(idx) {
                    o.key_pre = Some(pre);
                }
            }
            let hit_before = self.key_hit;
            let found = quoted
                .then(|| find_in(&here, &name))
                .flatten()
                .or_else(|| self.dirs.iter().find_map(|d| find_in(d, &name)));
            match found {
                Some(p) => {
                    self.open(&p, depth + 1)?;
                    if !hit_before && self.key_hit {
                        let pre = prefix_cost(text, line)?;
                        if let Some(o) = self.opens.get_mut(idx) {
                            o.key_pre = Some(pre);
                        }
                    }
                }
                None => {
                    let p = PathBuf::from(format!("<{name}>"));
                    if self.seen.insert(p.clone()) {
                        self.opens.push(Opened {
                            path: p,
                            depth: depth + 1,
                            own: 0,
                            before_key: !self.key_hit,
                            key_pre: None,
                            words: BTreeSet::new(),
                            unmodelled: Vec::new(),
                        });
                    }
                }
            }
        }
        if let (Some(k), false) = (kl, self.key_hit) {
            self.key_hit = true;
            self.key_file = Some(canon);
            let pre = prefix_cost(text, k)?;
            if let Some(o) = self.opens.get_mut(idx) {
                o.key_pre = Some(pre);
            }
        }
        Ok(())
    }

    fn totals(&self) -> (i64, i64) {
        let all = self.opens.iter().map(|o| o.own + 1).sum();
        let pre = self
            .opens
            .iter()
            .filter(|o| o.before_key)
            .map(|o| match o.key_pre {
                Some(p) => p + 1,
                None => o.own + 1,
            })
            .sum();
        (all, pre)
    }
}

fn walk_tu<'s>(
    shared: &'s Shared,
    tu: &Path,
    dirs: &[PathBuf],
    key: Option<&str>,
    drop: &BTreeSet<(PathBuf, String)>,
    pre_seen: &BTreeSet<PathBuf>,
    root_text: Option<String>,
) -> Result<Walk<'s>> {
    let mut w = Walk {
        shared,
        dirs: dirs.to_vec(),
        key: key.map(str::to_string),
        drop: drop.clone(),
        root_text,
        seen: pre_seen.clone(),
        opens: Vec::new(),
        key_hit: false,
        key_file: None,
    };
    w.open(tu, 0)?;
    Ok(w)
}

pub fn missing_includes(tu: &Path, dirs: &[PathBuf]) -> Result<Vec<String>> {
    let shared = Shared::new(MAX_OPENS);
    let none = BTreeSet::new();
    let w = walk_tu(&shared, tu, dirs, None, &none, &BTreeSet::new(), None)?;
    Ok(w.opens
        .iter()
        .filter_map(|o| {
            let p = o.path.to_str()?;
            Some(p.strip_prefix('<')?.strip_suffix('>')?.to_string())
        })
        .collect())
}

#[must_use]
pub fn move_line(text: &str, from: usize, to: usize) -> String {
    let lines: Vec<&str> = text.lines().collect();
    let Some(moved) = from.checked_sub(1).and_then(|f| lines.get(f)) else {
        return text.to_string();
    };
    let mut out: Vec<&str> = Vec::with_capacity(lines.len());
    for (i, l) in lines.iter().enumerate() {
        if i + 1 == to {
            out.push(moved);
        }
        if i + 1 != from {
            out.push(l);
        }
    }
    out.join("\n") + "\n"
}

const KEYWORDS: &[&str] = &[
    "int",
    "char",
    "short",
    "long",
    "unsigned",
    "signed",
    "void",
    "float",
    "double",
    "struct",
    "class",
    "union",
    "enum",
    "extern",
    "static",
    "const",
    "return",
    "if",
    "else",
    "for",
    "while",
    "do",
    "switch",
    "case",
    "default",
    "break",
    "continue",
    "sizeof",
    "typedef",
    "inline",
    "public",
    "private",
    "protected",
    "virtual",
    "this",
    "new",
    "delete",
    "goto",
    "operator",
    "friend",
    "volatile",
    "include",
    "define",
    "ifndef",
    "endif",
    "BOOL",
    "TRUE",
    "FALSE",
    "NULL",
    "DWORD",
    "WORD",
    "BYTE",
    "__stdcall",
    "__cdecl",
    "WINAPI",
    "HWND",
    "RECT",
    "POINT",
];

fn attests(
    tu: &BTreeSet<String>,
    gone: &[&Opened],
    kept: &[&Opened],
    skip: &BTreeSet<String>,
) -> Vec<String> {
    let kept: BTreeSet<&String> = kept.iter().flat_map(|o| o.words.iter()).collect();
    let gone: BTreeSet<&String> = gone.iter().flat_map(|o| o.words.iter()).collect();
    tu.iter()
        .filter(|w| gone.contains(w) && !kept.contains(w) && !skip.contains(*w))
        .filter(|w| w.len() > 2 && !KEYWORDS.contains(&w.as_str()))
        .cloned()
        .collect()
}

fn short(p: &Path) -> String {
    p.file_name().map_or_else(
        || p.display().to_string(),
        |f| f.to_string_lossy().into_owned(),
    )
}

fn pre_keys(
    shared: &Shared,
    tu: &Path,
    dirs: &[PathBuf],
    keys: &[String],
    drop: &BTreeSet<(PathBuf, String)>,
    text: Option<&str>,
) -> Result<Vec<i64>> {
    let one = |k: Option<&str>| -> Result<i64> {
        let w = walk_tu(
            shared,
            tu,
            dirs,
            k,
            drop,
            &BTreeSet::new(),
            text.map(str::to_string),
        )?;
        let (all, pre) = w.totals();
        Ok(if k.is_some() && w.key_file.is_some() {
            pre
        } else {
            all
        })
    };
    let mut v: Vec<i64> = keys.iter().map(|k| one(Some(k))).collect::<Result<_>>()?;
    v.push(one(None)?);
    Ok(v)
}

fn shifts(now: &[i64], base: &[i64]) -> Vec<i64> {
    now.iter().zip(base).map(|(a, b)| a - b).collect()
}

#[must_use]
pub fn show_shifts(sh: &[i64], want: &[i64]) -> String {
    let (keys, all) = sh.split_at(sh.len().saturating_sub(1));
    let f = |v: &[i64], m: bool| -> String {
        v.iter()
            .map(|x| {
                if m {
                    x.rem_euclid(64).to_string()
                } else {
                    format!("{x:+}")
                }
            })
            .collect::<Vec<_>>()
            .join(" ")
    };
    let hit = !want.is_empty()
        && keys
            .last()
            .is_some_and(|x| want.iter().any(|w| w.rem_euclid(64) == x.rem_euclid(64)));
    format!(
        "shift [{} | {}] mod64 [{} | {}]{}",
        f(keys, false),
        f(all, false),
        f(keys, true),
        f(all, true),
        if hit { "  WANT" } else { "" }
    )
}

type Dropped = (Vec<i64>, Vec<String>, Vec<String>);

pub fn predict_tu(
    tu: &Path,
    dirs: &[PathBuf],
    keys: &[String],
    want: &[i64],
) -> Result<Vec<String>> {
    predict_tu_with(&Shared::new(MAX_OPENS), tu, dirs, keys, want)
}

#[allow(clippy::too_many_lines)]
pub fn predict_tu_with(
    shared: &Shared,
    tu: &Path,
    dirs: &[PathBuf],
    keys: &[String],
    want: &[i64],
) -> Result<Vec<String>> {
    let mut out = Vec::new();
    let none = BTreeSet::new();
    let base = walk_tu(
        shared,
        tu,
        dirs,
        keys.last().map(String::as_str),
        &none,
        &BTreeSet::new(),
        None,
    )?;
    let (all, _) = base.totals();
    let b = pre_keys(shared, tu, dirs, keys, &none, None)?;
    out.push(format!(
        "{}: closure {} files, predicted counter {all}",
        tu.display(),
        base.opens.len()
    ));
    for (k, v) in keys.iter().zip(&b) {
        out.push(format!("  key `{k}`: {v} ahead of it"));
    }
    let mut cum = 0;
    for o in &base.opens {
        cum += o.own + 1;
        let tag = if base.key_file.as_ref() == Some(&o.path) {
            "  <- KEY"
        } else if !o.before_key {
            "  (after key)"
        } else {
            ""
        };
        let um = if o.unmodelled.is_empty() {
            String::new()
        } else {
            format!("  UNMODELLED {}", o.unmodelled.join(" "))
        };
        out.push(format!(
            "  {:>5} {}{} +{}{tag}{um}",
            cum,
            "  ".repeat(o.depth),
            short(&o.path),
            o.own + 1
        ));
    }
    let empty = BTreeSet::new();
    let tu_words = base.opens.first().map_or(&empty, |o| &o.words);
    let skip: BTreeSet<String> = BTreeSet::new();
    let text = read_text(tu)?;
    let canon = tu
        .canonicalize()
        .with_context(|| tu.display().to_string())?;
    let incs: Vec<(usize, String, bool)> =
        include_lines(&text).into_iter().filter(|x| x.2).collect();
    let direct: Vec<String> = incs.iter().map(|x| x.1.clone()).collect();
    let list = |att: &[String]| -> String {
        if att.is_empty() {
            String::new()
        } else {
            format!(
                " ({})",
                att.iter().take(6).cloned().collect::<Vec<_>>().join(" ")
            )
        }
    };
    let drop_of = |names: &[&String]| -> Result<Dropped> {
        let drop: BTreeSet<(PathBuf, String)> = names
            .iter()
            .map(|n| (canon.clone(), (*n).clone()))
            .collect();
        let w = walk_tu(shared, tu, dirs, None, &drop, &BTreeSet::new(), None)?;
        let kept: BTreeSet<&PathBuf> = w.opens.iter().map(|o| &o.path).collect();
        let gone: Vec<&Opened> = base
            .opens
            .iter()
            .filter(|o| !kept.contains(&o.path))
            .collect();
        let keep: Vec<&Opened> = w.opens.iter().skip(1).collect();
        let files = gone.iter().map(|o| short(&o.path)).collect();
        let sh = shifts(&pre_keys(shared, tu, dirs, keys, &drop, None)?, &b);
        Ok((sh, files, attests(tu_words, &gone, &keep, &skip)))
    };
    out.push(
        "shift columns: [each key | TU end]; `uses` = TU names only the dropped/added files spell"
            .to_string(),
    );
    out.push("DROP one direct include:".to_string());
    for n in &direct {
        let (sh, files, att) = drop_of(&[n])?;
        out.push(format!(
            "  {n}: {}  files [{}]  uses {}{}",
            show_shifts(&sh, want),
            files.join(" "),
            att.len(),
            list(&att)
        ));
    }
    if !want.is_empty() {
        out.push("DROP a PAIR landing on WANT:".to_string());
        for (i, a) in direct.iter().enumerate() {
            for c in direct.iter().skip(i + 1) {
                let (sh, _, att) = drop_of(&[a, c])?;
                let line = show_shifts(&sh, want);
                if line.ends_with("WANT") {
                    out.push(format!("  {a} + {c}: {line}  uses {}", att.len()));
                }
            }
        }
    }
    out.push(
        "MOVE one direct include to the TU's first include line (include order is live):"
            .to_string(),
    );
    if let Some(first) = incs.first().map(|x| x.0) {
        for (line, n, _) in incs.iter().skip(1) {
            let t = move_line(&text, *line, first);
            let sh = shifts(&pre_keys(shared, tu, dirs, keys, &none, Some(&t))?, &b);
            if sh.iter().any(|x| *x != 0) {
                out.push(format!("  {n} (line {line}): {}", show_shifts(&sh, want)));
            }
        }
    }
    out.push("ADD a project header the closure lacks, at the TU's first include line:".to_string());
    let first = incs.first().map_or(1, |x| x.0);
    let mut hs: Vec<PathBuf> = dirs
        .iter()
        .filter_map(|d| std::fs::read_dir(d).ok())
        .flatten()
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.extension().is_some_and(|x| x.eq_ignore_ascii_case("h")))
        .collect();
    hs.sort();
    let mut shown_n = 0;
    for h in hs {
        let hc = h.canonicalize().unwrap_or_else(|_| h.clone());
        if base.seen.contains(&hc) {
            continue;
        }
        let name = short(&h);
        let mut t: Vec<String> = text.lines().map(str::to_string).collect();
        t.insert(
            first.saturating_sub(1).min(t.len()),
            format!("#include \"{name}\""),
        );
        let t = t.join("\n") + "\n";
        let sh = shifts(&pre_keys(shared, tu, dirs, keys, &none, Some(&t))?, &b);
        let line = show_shifts(&sh, want);
        if !want.is_empty() && !line.ends_with("WANT") {
            continue;
        }
        let w = walk_tu(shared, &h, dirs, None, &none, &base.seen, None)?;
        let gone: Vec<&Opened> = w.opens.iter().collect();
        let att = attests(tu_words, &gone, &[], &skip);
        out.push(format!(
            "  {name}: {line}  new files {}  uses {}{}",
            w.opens.len(),
            att.len(),
            list(&att)
        ));
        shown_n += 1;
    }
    out.push(format!("{shown_n} header(s) listed"));
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn include_lines_carry_line_and_kind() {
        let t = "// x\n#include \"A.h\"\n# include <b.h>\nint a;\n";
        assert_eq!(
            include_lines(t),
            vec![(2, "A.h".to_string(), true), (3, "b.h".to_string(), false)]
        );
    }

    #[test]
    fn key_line_skips_comments_and_directives() {
        let t =
            "/* g_holes here */\n#define g_holes 1\nextern int g_holesX;\nextern int g_holes;\n";
        assert_eq!(key_line(t, "g_holes"), Some(4));
    }

    #[test]
    fn move_line_lifts_an_include_above_another() {
        assert_eq!(move_line("a\nb\nc\n", 3, 1), "c\na\nb\n");
        assert_eq!(move_line("a\nb\n", 9, 1), "a\nb\n");
        assert_eq!(move_line("a\nb\n", 0, 1), "a\nb\n");
    }

    #[test]
    fn a_tu_walk_opens_each_file_once_and_splits_at_the_key() -> Result<()> {
        let d = std::env::temp_dir().join(format!("msvc6-model-walk-{}", std::process::id()));
        std::fs::create_dir_all(&d)?;
        srcio::own_write(
            d.join("A.h"),
            "#include \"B.h\"\nextern int a;\nextern int key;\nextern int z;\n",
        )?;
        srcio::own_write(d.join("B.h"), "extern int b;\n")?;
        srcio::own_write(d.join("t.cpp"), "#include \"A.h\"\n#include \"B.h\"\n")?;
        let none = BTreeSet::new();
        let shared = Shared::new(MAX_OPENS);
        let w = walk_tu(
            &shared,
            &d.join("t.cpp"),
            std::slice::from_ref(&d),
            Some("key"),
            &none,
            &BTreeSet::new(),
            None,
        )?;
        assert_eq!(w.opens.len(), 3);
        assert_eq!(w.totals(), (7, 5));
        let lines = predict_tu(&d.join("t.cpp"), std::slice::from_ref(&d), &[], &[])?;
        assert!(
            lines
                .first()
                .is_some_and(|l| l.ends_with("closure 3 files, predicted counter 7"))
        );
        std::fs::remove_dir_all(&d).ok();
        Ok(())
    }

    #[test]
    fn an_include_no_directory_holds_is_named_as_missing() -> Result<()> {
        let d = std::env::temp_dir().join(format!("msvc6-model-missing-{}", std::process::id()));
        std::fs::create_dir_all(&d)?;
        srcio::own_write(d.join("A.h"), "extern int a;\n")?;
        srcio::own_write(
            d.join("t.cpp"),
            "#include \"gone.h\"\n#include \"A.h\"\n#include <sys.h>\n",
        )?;
        assert_eq!(
            missing_includes(&d.join("t.cpp"), std::slice::from_ref(&d))?,
            ["gone.h", "sys.h"]
        );
        std::fs::remove_dir_all(&d).ok();
        Ok(())
    }

    #[test]
    fn a_shift_line_marks_a_want_on_the_last_key() {
        assert_eq!(
            show_shifts(&[3, 67], &[3]),
            "shift [+3 | +67] mod64 [3 | 3]  WANT"
        );
        assert_eq!(show_shifts(&[5], &[]), "shift [ | +5] mod64 [ | 5]");
    }
}
