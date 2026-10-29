use super::glob::{Pat, collapse};
use anyhow::{Result, bail};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum View {
    Asm,
    Frame,
    Data,
    ClOut,
    Obj,
    Map,
    LinkOut,
    Rich,
    Imports,
    Il,
    Listing,
}

impl View {
    #[must_use]
    pub fn word(self) -> &'static str {
        match self {
            Self::Asm => "asm",
            Self::Frame => "frame",
            Self::Data => "data",
            Self::ClOut => "clout",
            Self::Obj => "obj",
            Self::Map => "map",
            Self::LinkOut => "linkout",
            Self::Rich => "rich",
            Self::Imports => "imports",
            Self::Il => "il",
            Self::Listing => "listing",
        }
    }

    fn of(word: &str) -> Option<Self> {
        [
            Self::Asm,
            Self::Frame,
            Self::Data,
            Self::ClOut,
            Self::Obj,
            Self::Map,
            Self::LinkOut,
            Self::Rich,
            Self::Imports,
            Self::Il,
            Self::Listing,
        ]
        .into_iter()
        .find(|v| v.word() == word)
    }

    #[must_use]
    pub fn needs_function(self) -> bool {
        matches!(self, Self::Asm | Self::Frame)
    }

    #[must_use]
    pub fn needs_link(self) -> bool {
        matches!(self, Self::Map | Self::LinkOut | Self::Rich | Self::Imports)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Block {
    pub view: View,
    pub exact: bool,
    pub func: Option<String>,
    pub pats: Vec<Pat>,
    pub line: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Crt {
    #[default]
    Native,
    Builtin,
}

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Expect {
    pub rules: Vec<String>,
    pub cl: Vec<String>,
    pub link: Option<Vec<String>>,
    pub crt: Crt,
    pub xfail: Option<String>,
    pub blocks: Vec<Block>,
    pub includes: Vec<String>,
}

pub const DEFAULT_CL: [&str; 1] = ["/O2"];

pub const SHIM_DIRECTIVES: [&str; 10] = [
    "mode",
    "hooks",
    "set",
    "filter",
    "rec",
    "diagnostic",
    "cut",
    "cutobj",
    "cutasm",
    "cutasm=",
];

pub const DRIVER_SWITCHES: [&str; 7] = ["/c", "/e", "/ep", "/fa", "/fo", "/nologo", "/p"];

fn driver_switch(s: &str) -> bool {
    let l = s.to_ascii_lowercase().replace('-', "/");
    DRIVER_SWITCHES
        .iter()
        .any(|d| l == *d || (d.len() > 2 && l.starts_with(d)))
}

pub const BUILD_ROLES: [&str; 5] = ["cl", "c1", "c1xx", "c2", "link"];

fn build_roles(text: &str) -> Vec<&str> {
    text.split("{build:")
        .skip(1)
        .map(|rest| rest.split('}').next().unwrap_or_default())
        .collect()
}

fn unknown_build_role(text: &str) -> Option<String> {
    build_roles(text)
        .into_iter()
        .find(|r| !BUILD_ROLES.contains(r))
        .map(str::to_string)
}

#[must_use]
pub fn with_builds(text: &str, build: &dyn Fn(&str) -> Option<u32>) -> String {
    let mut out = text.to_string();
    for r in BUILD_ROLES {
        let v = build(r).map_or_else(|| "?".to_string(), |b| b.to_string());
        out = out.replace(&format!("{{build:{r}}}"), &v);
    }
    out
}

fn link_switch_reserved(s: &str) -> bool {
    let l = s.to_ascii_lowercase().replace('-', "/");
    l == "/nologo" || l.starts_with("/out:") || l.starts_with("/map")
}

pub fn parse(text: &str) -> Result<Expect> {
    let mut e = Expect::default();
    let mut cl: Option<Vec<String>> = None;
    let mut crt_line = None;
    for (i, raw) in text.lines().enumerate() {
        let n = i.saturating_add(1);
        let indented = raw.starts_with([' ', '\t']);
        let t = raw.trim();
        if t.is_empty() {
            continue;
        }
        if t.starts_with('#') {
            bail!(
                "line {n}: a comment line; a probe is comment-free and says what it shows in its `rule` lines"
            );
        }
        if indented {
            let Some(b) = e.blocks.last_mut() else {
                bail!("line {n}: a pattern line outside any block");
            };
            let (neg, body) = match t.strip_prefix('!') {
                Some(r) => (true, r.trim()),
                None => (false, t),
            };
            if neg && b.exact {
                bail!("line {n}: an exact block takes no negative pattern");
            }
            if let Some(bad) = unknown_build_role(body) {
                bail!(
                    "line {n}: `{{build:{bad}}}` names no tool; the roles are {}",
                    BUILD_ROLES.join(", ")
                );
            }
            b.pats.push(Pat {
                neg,
                text: collapse(body),
                line: n,
            });
            continue;
        }
        let (kw, rest) = t.split_once(char::is_whitespace).unwrap_or((t, ""));
        let rest = rest.trim();
        if SHIM_DIRECTIVES.contains(&kw) {
            bail!(
                "line {n}: `{kw}` is a shim directive: a traced compile needs the C2 tracing shim \
                 (msvc6-trace), which this build does not have"
            );
        }
        let words = || {
            rest.split_whitespace()
                .map(str::to_string)
                .collect::<Vec<_>>()
        };
        match kw {
            "rule" => {
                let stated = rest
                    .split_once(':')
                    .map(|(s, t)| (s.trim(), t.trim()))
                    .filter(|(s, t)| BUILD_ROLES.contains(s) && !t.is_empty());
                if stated.is_none() {
                    bail!(
                        "line {n}: `rule STAGE: TEXT`, the pass the rule is about ({}) and what it says",
                        BUILD_ROLES.join(", ")
                    );
                }
                e.rules.push(rest.to_string());
            }
            "cl" => {
                if cl.is_some() {
                    bail!("line {n}: a second `cl` line");
                }
                let w = words();
                if let Some(d) = w.iter().find(|s| driver_switch(s)) {
                    bail!(
                        "line {n}: `{d}`: the harness passes /nologo /c /FAs /Fa /Fo itself, and a probe that \
                         preprocesses (/P, /E) makes no object"
                    );
                }
                cl = Some(w);
            }
            "link" => {
                if e.link.is_some() {
                    bail!("line {n}: a second `link` line");
                }
                let w = words();
                if let Some(d) = w.iter().find(|s| link_switch_reserved(s)) {
                    bail!("line {n}: `{d}`: the harness passes /nologo /out: and /map: itself");
                }
                e.link = Some(w);
            }
            "link-crt" => {
                e.crt = match rest {
                    "native" => Crt::Native,
                    "builtin" => Crt::Builtin,
                    o => bail!("line {n}: `link-crt native|builtin`, not `{o}`"),
                };
                crt_line = Some(n);
            }
            "include" => {
                let plain = !rest.is_empty()
                    && !rest.starts_with('.')
                    && !rest.contains(['/', '\\', ':'])
                    && !rest.contains(char::is_whitespace);
                if !plain {
                    bail!(
                        "line {n}: `include FILE` names a header beside the probe by its plain file name, not `{rest}`"
                    );
                }
                e.includes.push(rest.to_string());
            }
            "xfail" => {
                if rest.is_empty() {
                    bail!("line {n}: `xfail` names the reason the rule as written is refuted");
                }
                e.xfail = Some(rest.to_string());
            }
            _ => {
                let exact = kw.ends_with('=');
                let Some(view) = View::of(kw.trim_end_matches('=')) else {
                    bail!("line {n}: unknown directive `{kw}`");
                };
                if view.needs_function() && rest.is_empty() {
                    bail!("line {n}: `{kw}` names a function (the listing's PROC name)");
                }
                if !view.needs_function() && !rest.is_empty() {
                    bail!("line {n}: `{kw}` takes no argument");
                }
                e.blocks.push(Block {
                    view,
                    exact,
                    func: (!rest.is_empty()).then(|| rest.to_string()),
                    pats: Vec::new(),
                    line: n,
                });
            }
        }
    }
    e.cl = cl.unwrap_or_else(|| DEFAULT_CL.iter().map(|s| (*s).to_string()).collect());
    if e.rules.is_empty() {
        bail!("no `rule` line: a probe names the rule it shows");
    }
    if e.blocks.is_empty() {
        bail!("no block: a probe checks something");
    }
    for b in &e.blocks {
        if b.pats.is_empty() {
            bail!("line {}: `{}` has no pattern line", b.line, b.view.word());
        }
        if b.view.needs_link() && e.link.is_none() {
            bail!(
                "line {}: a `{}` block reads the link, and there is no `link` line",
                b.line,
                b.view.word()
            );
        }
    }
    if let (Some(n), None) = (crt_line, &e.link) {
        bail!("line {n}: `link-crt` with no `link` line");
    }
    Ok(e)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_probe_reads_its_rules_switches_and_blocks() {
        let e = parse(
            "rule c2: y\ncl /Od /GR-\nframe= _f\n  _a$ = -4\nasm _f\n  push ebp\n  !sub esp, *\n",
        )
        .unwrap();
        assert_eq!(e.rules, vec!["c2: y".to_string()]);
        assert_eq!(e.cl, vec!["/Od".to_string(), "/GR-".to_string()]);
        assert_eq!(e.blocks.len(), 2);
        assert!(e.blocks[0].exact);
        assert_eq!(e.blocks[1].func.as_deref(), Some("_f"));
        assert!(e.blocks[1].pats[1].neg);
        assert!(e.link.is_none());
    }

    #[test]
    fn the_default_switches_are_o2() {
        let e = parse("rule c2: r\ndata\n  x\n").unwrap();
        assert_eq!(e.cl, vec!["/O2".to_string()]);
    }

    #[test]
    fn shim_directives_are_refused_by_name() {
        for d in SHIM_DIRECTIVES {
            let err = parse(&format!("rule c2: r\n{d} x\nasm _f\n  ret 0\n"))
                .unwrap_err()
                .to_string();
            assert!(err.contains("shim directive"), "{d}: {err}");
        }
    }

    #[test]
    fn a_comment_line_and_the_harness_switches_are_refused() {
        assert!(parse("# why\nrule c2: r\ndata\n  x\n").is_err());
        assert!(parse("rule c2: r\ncl /O2 /FAs\ndata\n  x\n").is_err());
        assert!(parse("rule c2: r\ncl /Fofoo.obj\ndata\n  x\n").is_err());
        assert!(parse("rule c2: r\nlink /out:x.exe\nmap\n  x\n").is_err());
        assert!(parse("rule c2: r\ncl /GF /Gf\ndata\n  x\n").is_ok());
    }

    #[test]
    fn a_build_placeholder_names_a_tool_and_takes_the_runs_build() {
        assert!(parse("rule c2: r\nobj\n  comp.id Utc12_CPP {build:c3}\n").is_err());
        let e = parse("rule c2: r\nobj\n  comp.id Utc12_CPP {build:c2}\n").unwrap();
        let b = |r: &str| (r == "c2").then_some(8447);
        assert_eq!(
            with_builds(&e.blocks[0].pats[0].text, &b),
            "comp.id Utc12_CPP 8447"
        );
        assert_eq!(with_builds("{build:link} {x}", &b), "? {x}");
    }

    #[test]
    fn a_link_view_needs_a_link_line() {
        assert!(parse("rule c2: r\nmap\n  x\n").is_err());
        assert!(parse("rule c2: r\nlink /dll\nmap\n  x\n").is_ok());
        assert!(parse("rule c2: r\nlink-crt builtin\nasm _f\n  x\n").is_err());
        let e = parse("rule c2: r\nlink /dll\nlink-crt builtin\nimports\n  x\n").unwrap();
        assert_eq!(e.crt, Crt::Builtin);
    }

    #[test]
    fn a_probe_names_a_rule_and_checks_something() {
        assert!(parse("cl /O2\nasm _f\n  ret 0\n").is_err());
        assert!(parse("rule c2: r\n").is_err());
        assert!(parse("rule c2: r\nasm _f\n").is_err());
        assert!(parse("rule c2: r\nasm\n  ret 0\n").is_err());
        assert!(parse("rule c2: r\ndata x\n  ret 0\n").is_err());
        assert!(parse("rule c2: r\nasm= _f\n  !ret 0\n").is_err());
        assert!(parse("  ret 0\nrule c2: r\n").is_err());
        assert!(parse("rule c2: r\nbogus\nasm _f\n  ret 0\n").is_err());
    }

    #[test]
    fn a_rule_names_the_pass_it_is_about() {
        for bad in [
            "rule r",
            "rule c2",
            "rule c2:",
            "rule c3: r",
            "rule c2 \"Frame\": r",
        ] {
            let e = parse(&format!("{bad}\ndata\n  x\n"))
                .unwrap_err()
                .to_string();
            assert!(e.contains("`rule STAGE: TEXT`"), "{bad}: {e}");
        }
        for stage in BUILD_ROLES {
            let e = parse(&format!("rule {stage}: r\ndata\n  x\n")).unwrap();
            assert_eq!(e.rules, vec![format!("{stage}: r")]);
        }
    }
}
