use crate::c1count::{self, PROBE};
use anyhow::{Context as _, Result, bail};
use msvc6_base::srcio;
use msvc6_diff::compile::{Compiler, Unit};
use msvc6_toolchain::layout::to_win;
use std::fmt::Write as _;
use std::path::{Path, PathBuf};

pub const MAX_TEXT: u64 = 256 << 20;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Probe {
    pub file: String,
    pub line: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Probed {
    pub text: String,
    pub probes: Vec<Probe>,
    pub sys: Vec<bool>,
    pub spans: Vec<String>,
}

#[must_use]
pub fn line_directive(l: &str) -> Option<(u32, String)> {
    let r = l.trim_start().strip_prefix('#')?.trim_start();
    let r = r.strip_prefix("line")?.trim_start();
    let (n, rest) = r.split_once(char::is_whitespace).unwrap_or((r, ""));
    let n: u32 = n.parse().ok()?;
    let f = rest.trim().trim_matches('"').to_string();
    Some((n, f))
}

#[must_use]
pub fn probe_text(k: usize) -> String {
    format!("void {PROBE}{k}(int x) {{ if (x > 3) c1hook(1); c1hook(2); }}\n")
}

#[derive(Debug, Default)]
struct Scan {
    depth: i64,
    paren: i64,
    kinds: Vec<bool>,
    last: char,
    last_word: String,
    closed_fn: bool,
}

impl Scan {
    fn line(&mut self, l: &str) {
        let b: Vec<char> = l.chars().collect();
        let ch = |k: usize| b.get(k).copied();
        let mut i = 0;
        while let Some(c) = ch(i) {
            if c == '"' || c == '\'' {
                i += 1;
                while ch(i).is_some_and(|x| x != c) {
                    if ch(i) == Some('\\') {
                        i += 1;
                    }
                    i += 1;
                }
                self.last = c;
                i += 1;
                continue;
            }
            if c == '/' && ch(i + 1) == Some('/') {
                break;
            }
            if c.is_ascii_alphanumeric() || c == '_' {
                let st = i;
                while ch(i).is_some_and(|x| x.is_ascii_alphanumeric() || x == '_') {
                    i += 1;
                }
                self.last_word = b.get(st..i).unwrap_or_default().iter().collect();
                self.last = 'a';
                continue;
            }
            match c {
                '(' => self.paren += 1,
                ')' => self.paren -= 1,
                '{' => {
                    let f = self.last == ')' || (self.last == 'a' && self.last_word == "const");
                    self.kinds.push(f);
                    self.depth += 1;
                }
                '}' => {
                    self.depth -= 1;
                    let f = self.kinds.pop().unwrap_or(false);
                    if self.depth == 0 {
                        self.closed_fn = f;
                    }
                }
                _ => {}
            }
            if !c.is_whitespace() {
                self.last = c;
            }
            i += 1;
        }
    }

    fn boundary(&self) -> bool {
        self.depth == 0
            && self.paren == 0
            && (self.last == ';' || self.last == '\0' || (self.last == '}' && self.closed_fn))
    }
}

#[must_use]
pub fn probe(pre: &str, is_project: &dyn Fn(&str) -> bool) -> Probed {
    let mut out = String::with_capacity(pre.len() + pre.len() / 8);
    out.push_str("void c1hook(int);\n");
    let mut probes: Vec<Probe> = Vec::new();
    let mut sys = Vec::new();
    let mut spans = Vec::new();
    let mut span = String::new();
    let mut span_sys = false;
    let mut sc = Scan {
        last: '\0',
        ..Scan::default()
    };
    let mut file = String::new();
    let mut line: u32 = 0;
    let mut project: Option<bool> = None;
    let mut put = |out: &mut String,
                   span: &mut String,
                   span_sys: &mut bool,
                   file: &str,
                   line: u32,
                   probes: &mut Vec<Probe>| {
        out.push_str(&probe_text(probes.len()));
        if !probes.is_empty() {
            sys.push(*span_sys);
            spans.push(std::mem::take(span));
        }
        span.clear();
        *span_sys = false;
        probes.push(Probe {
            file: file.to_string(),
            line,
        });
    };
    put(&mut out, &mut span, &mut span_sys, "<top>", 0, &mut probes);
    for l in pre.lines() {
        if let Some((n, f)) = line_directive(l) {
            let p = is_project(&f);
            if project.is_some_and(|q| q != p) && sc.boundary() {
                put(&mut out, &mut span, &mut span_sys, &file, line, &mut probes);
            }
            project = Some(p);
            file = f;
            line = n.saturating_sub(1);
            out.push_str(l);
            out.push('\n');
            continue;
        }
        line = line.saturating_add(1);
        out.push_str(l);
        out.push('\n');
        let code = l.trim_start();
        if code.is_empty() || code.starts_with('#') {
            continue;
        }
        span.push_str(l);
        span.push('\n');
        let project = project.unwrap_or(false);
        if !project {
            span_sys = true;
        }
        sc.line(l);
        if project && sc.boundary() {
            put(&mut out, &mut span, &mut span_sys, &file, line, &mut probes);
        }
    }
    Probed {
        text: out,
        probes,
        sys,
        spans,
    }
}

#[must_use]
pub fn probe_labels(cod: &str, n: usize) -> Vec<Option<i64>> {
    let mut v = vec![None; n];
    let mut cur: Option<usize> = None;
    let head = format!("?{PROBE}");
    for l in cod.lines() {
        if let Some(r) = l.strip_prefix(&head) {
            if l.contains(" PROC") {
                cur = r.split('@').next().and_then(|k| k.parse().ok());
            } else if l.contains(" ENDP") {
                cur = None;
            }
            continue;
        }
        let Some(k) = cur else { continue };
        if v.get(k).is_some_and(Option::is_some) {
            continue;
        }
        if let Some(i) = l.find("$L") {
            let d: String = l
                .get(i + 2..)
                .unwrap_or_default()
                .chars()
                .take_while(char::is_ascii_digit)
                .collect();
            if let (Ok(x), Some(slot)) = (d.parse(), v.get_mut(k)) {
                *slot = Some(x);
            }
        }
    }
    v
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct TuCheck {
    pub report: Vec<String>,
    pub spans: usize,
    pub sys: usize,
    pub misses: usize,
    pub miss_sum: i64,
    pub probes: usize,
}

fn short(f: &str) -> String {
    let f = f.replace("\\\\", "\\");
    f.rsplit(['\\', '/']).next().unwrap_or(&f).to_string()
}

fn read_bounded(p: &Path) -> Result<String> {
    srcio::read_text_bounded(p, MAX_TEXT).with_context(|| format!("reading {}", p.display()))
}

pub fn compare(
    stem: &str,
    p: &Probed,
    labels: &[Option<i64>],
    gx: bool,
    all: bool,
    dump: Option<&Path>,
) -> Result<TuCheck> {
    let c = c1count::count_with(&p.text, gx)?;
    if c.marks.len() != p.probes.len() {
        bail!(
            "{stem}: the counter model saw {} of {} probes (the walker lost file scope)",
            c.marks.len(),
            p.probes.len()
        );
    }
    let mut out = TuCheck {
        probes: p.probes.len(),
        ..TuCheck::default()
    };
    let base = match (labels.first().copied().flatten(), c.marks.first()) {
        (Some(a), Some(m)) => a - m,
        _ => bail!("{stem}: probe 0 has no label"),
    };
    out.report.push(format!(
        "{stem}: {} probes, /GX {}, offset {base}",
        p.probes.len(),
        if gx { "on" } else { "off" }
    ));
    for k in 1..p.probes.len() {
        let (Some(a0), Some(a1)) = (
            labels.get(k - 1).copied().flatten(),
            labels.get(k).copied().flatten(),
        ) else {
            out.report
                .push(format!("  probe {k}: no label in the listing"));
            continue;
        };
        let (Some(m0), Some(m1)) = (c.marks.get(k - 1), c.marks.get(k)) else {
            continue;
        };
        let (act, pred) = (a1 - a0, m1 - m0);
        let (Some(pa), Some(pb)) = (p.probes.get(k - 1), p.probes.get(k)) else {
            continue;
        };
        let at = format!(
            "{}:{}..{}:{}",
            short(&pa.file),
            pa.line,
            short(&pb.file),
            pb.line
        );
        if p.sys.get(k - 1).copied().unwrap_or(false) {
            out.sys += 1;
            if all {
                out.report
                    .push(format!("  SYS  {at} actual {act} predicted {pred}"));
            }
            continue;
        }
        out.spans += 1;
        if act != pred {
            out.misses += 1;
            out.miss_sum += act - pred;
            let mut line = format!(
                "  MISS {at} actual {act} predicted {pred} ({:+})",
                act - pred
            );
            if let Some(dir) = dump {
                let f = dir.join(format!("miss_{k}.txt"));
                srcio::write(&f, p.spans.get(k - 1).map_or("", String::as_str))
                    .with_context(|| format!("writing {}", f.display()))?;
                let _ = write!(line, "  {}", f.display());
            }
            out.report.push(line);
        } else if all {
            out.report.push(format!("  ok   {at} {act}"));
        }
    }
    if !c.unmodelled.is_empty() {
        out.report
            .push(format!("  UNMODELLED {}", c.unmodelled.join("; ")));
    }
    Ok(out)
}

fn includes_of(u: &Unit) -> Vec<String> {
    let dir = u
        .source
        .canonicalize()
        .ok()
        .and_then(|p| p.parent().map(Path::to_path_buf));
    u.includes
        .iter()
        .cloned()
        .chain(dir)
        .map(|d| {
            let abs = d.canonicalize().unwrap_or(d);
            format!("/I{}", to_win(&abs))
        })
        .collect()
}

pub fn check_tu(
    c: &Compiler,
    u: &Unit,
    root: &Path,
    is_project: &dyn Fn(&str) -> bool,
    all: bool,
) -> Result<TuCheck> {
    let gx = u.cflags.iter().any(|f| f.eq_ignore_ascii_case("/GX"));
    let dir = root.join(&u.stem);
    std::fs::create_dir_all(&dir).with_context(|| format!("creating {}", dir.display()))?;
    let cpp = format!("{}.cpp", u.stem);
    std::fs::copy(&u.source, dir.join(&cpp))
        .with_context(|| format!("copying {}", u.source.display()))?;
    let tc = c.toolchain(&u.toolchain)?;
    let mut env = Vec::new();
    if let Some(i) = &u.include_env {
        env.push(("INCLUDE".to_string(), i.clone()));
    }
    let mut args = vec!["/nologo".to_string(), "/P".to_string()];
    args.extend(u.cflags.iter().cloned());
    args.extend(includes_of(u));
    args.push(cpp);
    {
        let sess = c.runner.session()?;
        sess.run_tool(&tc, "CL.EXE", &args, Some(&dir), &env)
            .with_context(|| format!("preprocessing {}", u.source.display()))?;
    }
    let pre = read_bounded(&dir.join(format!("{}.i", u.stem)))?;
    let own = format!("{}.cpp", u.stem);
    let claimed = |f: &str| f.eq_ignore_ascii_case(&own) || is_project(f);
    let p = probe(&pre, &claimed);
    let pname = format!("{}_p", u.stem);
    srcio::write(dir.join(format!("{pname}.cpp")), &p.text)
        .with_context(|| format!("writing {pname}.cpp"))?;
    let mut args = vec!["/nologo".to_string(), "/c".to_string()];
    args.extend(u.cflags.iter().cloned());
    args.extend([
        "/FAs".to_string(),
        format!("/Fa{pname}.cod"),
        format!("/Fo{pname}.obj"),
        format!("{pname}.cpp"),
    ]);
    {
        let sess = c.runner.session()?;
        sess.run_tool(&tc, "CL.EXE", &args, Some(&dir), &env)
            .with_context(|| format!("compiling the probed {}", u.source.display()))?;
    }
    let cod = read_bounded(&dir.join(format!("{pname}.cod")))?;
    let got = probe_labels(&cod, p.probes.len());
    compare(&u.stem, &p, &got, gx, all, Some(&dir))
}

pub fn project_test(roots: &[PathBuf]) -> impl Fn(&str) -> bool + use<> {
    let norm = |s: &str| {
        s.replace("\\\\", "/")
            .replace('\\', "/")
            .to_ascii_lowercase()
    };
    let roots: Vec<String> = roots
        .iter()
        .map(|r| {
            let r = r.canonicalize().unwrap_or_else(|_| r.clone());
            let mut w = norm(&to_win(&r));
            if !w.ends_with('/') {
                w.push('/');
            }
            w
        })
        .collect();
    move |file: &str| {
        let f = norm(file);
        roots.iter().any(|r| f.contains(r.as_str()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn under(dir: &str) -> impl Fn(&str) -> bool + use<> {
        let dir = dir.to_string();
        move |f: &str| f.replace("\\\\", "/").to_ascii_lowercase().contains(&dir)
    }

    #[test]
    fn a_preprocessed_text_or_listing_that_is_no_regular_file_is_refused() -> Result<()> {
        let d = std::env::temp_dir().join(format!("msvc6-model-c1check-{}", std::process::id()));
        std::fs::create_dir_all(&d)?;
        let fifo = d.join("t.i");
        let made = std::process::Command::new("mkfifo")
            .arg(&fifo)
            .status()
            .is_ok_and(|s| s.success());
        let zero = d.join("t.cod");
        std::os::unix::fs::symlink("/dev/zero", &zero)?;
        let mut tried = vec![zero];
        if made {
            tried.push(fifo);
        }
        for p in &tried {
            let e = read_bounded(p).map(|t| t.len());
            assert!(
                e.as_ref()
                    .err()
                    .is_some_and(|e| format!("{e:#}").contains("not a regular file")),
                "{}: {e:?}",
                p.display()
            );
        }
        let ok = d.join("u.i");
        srcio::own_write(&ok, "int a;\n")?;
        assert_eq!(read_bounded(&ok)?, "int a;\n");
        std::fs::remove_dir_all(&d)?;
        Ok(())
    }

    #[test]
    fn probes_go_after_project_boundaries_only() -> Result<()> {
        let pre = "#line 1 \"c:\\\\x\\\\proj\\\\src\\\\a.cpp\"\n\
                   int a;\n\
                   struct S {\n int m;\n};\n\
                   void f() {\n}\n\
                   #line 1 \"c:\\\\vc98\\\\include\\\\w.h\"\n\
                   int b;\n\
                   #line 9 \"c:\\\\x\\\\proj\\\\src\\\\a.cpp\"\n\
                   int c;\n";
        let p = probe(pre, &under("/proj/"));
        let lines: Vec<u32> = p.probes.iter().map(|x| x.line).collect();
        assert_eq!(lines, vec![0, 1, 4, 6, 6, 1, 9]);
        assert_eq!(p.sys, vec![false, false, false, false, true, false]);
        let c = c1count::count_with(&p.text, true)?;
        assert_eq!(c.marks.len(), p.probes.len());
        Ok(())
    }

    #[test]
    fn labels_read_per_probe() {
        let cod = "?c1probe_0@@YAXH@Z PROC NEAR\n jle SHORT $L219\n$L219:\n?c1probe_0@@YAXH@Z ENDP\n\
                   ?c1probe_1@@YAXH@Z PROC NEAR\n jle SHORT $L230\n?c1probe_1@@YAXH@Z ENDP\n";
        assert_eq!(probe_labels(cod, 2), vec![Some(219), Some(230)]);
    }

    #[test]
    fn a_span_whose_labels_move_as_the_model_says_is_ok_and_one_that_does_not_is_a_miss()
    -> Result<()> {
        let pre = "#line 1 \"c:\\\\proj\\\\a.cpp\"\nint a;\nint b;\n";
        let p = probe(pre, &under("/proj/"));
        let c = c1count::count_with(&p.text, true)?;
        let labels: Vec<Option<i64>> = c.marks.iter().map(|m| Some(m + 200)).collect();
        let ok = compare("a", &p, &labels, true, false, None)?;
        assert_eq!((ok.spans, ok.misses), (2, 0));
        let mut off = labels.clone();
        if let Some(Some(x)) = off.last_mut() {
            *x += 3;
        }
        let bad = compare("a", &p, &off, true, false, None)?;
        assert_eq!((bad.misses, bad.miss_sum), (1, 3));
        Ok(())
    }
}
