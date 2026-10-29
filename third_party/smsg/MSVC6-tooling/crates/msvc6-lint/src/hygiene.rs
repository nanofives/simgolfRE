use crate::Result;
use std::io::Read as _;
use std::path::Path;

pub const SCRIPT_EXTENSIONS: [&str; 29] = [
    "sh", "bash", "zsh", "ksh", "csh", "tcsh", "fish", "py", "pyw", "pl", "pm", "awk", "sed",
    "ps1", "psm1", "psd1", "bat", "cmd", "vbs", "vbe", "wsf", "js", "mjs", "cjs", "ts", "rb",
    "lua", "tcl", "php",
];

pub const GHIDRA_DIR: &str = "ghidra/";

pub const GHIDRA_EXTENSIONS: [&str; 3] = ["js", "mjs", "cjs"];

pub const DENYLIST: &str = "denylist.txt";

pub const EXAMPLES: &str = "examples/";

pub const NAME_EXEMPT_DIRS: [&str; 2] = ["examples/", "docs/"];

pub const NAME_EXEMPT_EXTENSIONS: [&str; 1] = ["md"];

const HEAD: u64 = 512;

const TAR_MAGIC_AT: usize = 257;

const COFF_MACHINES: [u16; 12] = [
    0x014c, 0x8664, 0x01c0, 0x01c2, 0x01c4, 0xaa64, 0x0162, 0x0166, 0x0184, 0x01f0, 0x0200, 0x0268,
];

#[must_use]
pub fn interpreter_line(head: &[u8]) -> Option<String> {
    let rest = head.strip_prefix(b"#!")?;
    if !rest.trim_ascii_start().starts_with(b"/") {
        return None;
    }
    let line = head.split(|b| *b == b'\n').next().unwrap_or(head);
    Some(crate::srcio::tool_text(line).trim_end().to_string())
}

#[must_use]
pub fn script_by_name(rel: &str) -> Option<String> {
    let ext = Path::new(rel)
        .extension()
        .and_then(|e| e.to_str())
        .map(str::to_ascii_lowercase)?;
    let ghidra = rel.starts_with(GHIDRA_DIR) && GHIDRA_EXTENSIONS.contains(&ext.as_str());
    (SCRIPT_EXTENSIONS.contains(&ext.as_str()) && !ghidra)
        .then(|| format!("{rel}: a .{ext} script; the tooling is Rust"))
}

fn le16(head: &[u8], at: usize) -> Option<u16> {
    let b = head.get(at..at.checked_add(2)?)?;
    Some(u16::from_le_bytes([*b.first()?, *b.get(1)?]))
}

#[must_use]
pub fn binary_kind(head: &[u8]) -> Option<&'static str> {
    const MAGICS: [(&[u8], &str); 25] = [
        (b"MZ", "an MZ executable (DOS, NE, LE or PE)"),
        (b"!<arch>\n", "an ar archive (a COFF library)"),
        (b"Microsoft C/C++ MSF", "an MSF file (a PDB 7.0)"),
        (b"Microsoft C/C++ program database", "a PDB 2.0"),
        (
            b"Microsoft Linker Database",
            "an incremental link database (.ilk)",
        ),
        (
            &[0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1],
            "an OLE compound file (.opt, .ncb, .msi)",
        ),
        (b"Joy!peff", "a PEF container"),
        (&[0xfe, 0xed, 0xfa, 0xce], "a Mach-O image"),
        (&[0xce, 0xfa, 0xed, 0xfe], "a Mach-O image"),
        (&[0xfe, 0xed, 0xfa, 0xcf], "a Mach-O image"),
        (&[0xcf, 0xfa, 0xed, 0xfe], "a Mach-O image"),
        (&[0xca, 0xfe, 0xba, 0xbe], "a Mach-O universal image"),
        (&[0xbe, 0xba, 0xfe, 0xca], "a Mach-O universal image"),
        (b"\x7fELF", "an ELF image"),
        (b"PK\x03\x04", "a zip archive"),
        (b"PK\x05\x06", "a zip archive (empty)"),
        (b"PK\x07\x08", "a zip archive (spanned)"),
        (&[0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c], "a 7z archive"),
        (&[0x1f, 0x8b], "a gzip stream"),
        (b"MSCF\0\0\0\0", "a cabinet archive"),
        (b"Rar!\x1a\x07", "a rar archive"),
        (&[0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00], "an xz stream"),
        (b"BZh", "a bzip2 stream"),
        (&[0x28, 0xb5, 0x2f, 0xfd], "a zstd stream"),
        (b"SZDD\x88\xf0\x27\x33", "an MS-DOS COMPRESS.EXE file"),
    ];
    if let Some((_, what)) = MAGICS.iter().find(|(m, _)| head.starts_with(m)) {
        return Some(what);
    }
    if head
        .get(TAR_MAGIC_AT..TAR_MAGIC_AT + 5)
        .is_some_and(|m| m == b"ustar")
    {
        return Some("a tar archive");
    }
    if head.len() >= 20 {
        let machine = le16(head, 0)?;
        let optional = le16(head, 16)?;
        if COFF_MACHINES.contains(&machine) && optional == 0 {
            return Some("a COFF object");
        }
        if machine == 0 && le16(head, 2) == Some(0xffff) {
            return Some("a COFF import or anonymous object");
        }
    }
    None
}

fn head_of(path: &Path) -> Option<Vec<u8>> {
    let mut buf = Vec::new();
    std::fs::File::open(path)
        .and_then(|f| f.take(HEAD).read_to_end(&mut buf))
        .ok()?;
    Some(buf)
}

#[must_use]
pub fn script(root: &Path, rel: &str) -> Option<String> {
    if let Some(why) = script_by_name(rel) {
        return Some(why);
    }
    let head = head_of(&root.join(rel))?;
    interpreter_line(&head)
        .map(|l| format!("{rel}: starts `{l}`, a script under another name; the tooling is Rust"))
}

#[must_use]
pub fn binary(root: &Path, rel: &str) -> Option<String> {
    let head = head_of(&root.join(rel))?;
    binary_kind(&head).map(|k| format!("{rel}: {k}; no binary is kept in this repository"))
}

#[must_use]
pub fn denylist_files(files: &[String]) -> Vec<&String> {
    files
        .iter()
        .filter(|f| {
            f.strip_prefix(EXAMPLES)
                .and_then(|r| r.split_once('/'))
                .is_some_and(|(_, name)| name == DENYLIST)
        })
        .collect()
}

pub fn denylist(root: &Path, files: &[String]) -> Result<Vec<String>> {
    let mut names = Vec::new();
    for f in denylist_files(files) {
        let text = crate::srcio::own_read(root.join(f))?;
        names.extend(
            text.lines()
                .map(str::trim)
                .filter(|l| !l.is_empty())
                .map(str::to_string),
        );
    }
    names.sort();
    names.dedup();
    Ok(names)
}

#[must_use]
pub fn in_name_scope(rel: &str) -> bool {
    let exempt_ext = Path::new(rel)
        .extension()
        .and_then(|e| e.to_str())
        .is_some_and(|e| NAME_EXEMPT_EXTENSIONS.contains(&e.to_ascii_lowercase().as_str()));
    !exempt_ext && !NAME_EXEMPT_DIRS.iter().any(|d| rel.starts_with(d))
}

#[must_use]
pub fn names_in(text: &[u8], names: &[String]) -> Vec<(usize, String)> {
    let lower = text.to_ascii_lowercase();
    let mut out = Vec::new();
    for name in names {
        let needle = name.as_bytes().to_ascii_lowercase();
        if needle.is_empty() {
            continue;
        }
        let mut line = 1usize;
        let mut last = 0usize;
        for (i, w) in lower.windows(needle.len()).enumerate() {
            if w == needle.as_slice() {
                line += lower
                    .get(last..i)
                    .unwrap_or_default()
                    .iter()
                    .filter(|&&b| b == b'\n')
                    .count();
                last = i;
                out.push((line, name.clone()));
            }
        }
    }
    out.sort();
    out
}

#[must_use]
pub fn literal_texts(src: &str) -> Vec<(usize, String)> {
    let Ok(toks) = crate::lex::tokens(src) else {
        return Vec::new();
    };
    let decoded = |t: &crate::lex::Token| match &t.tok {
        crate::lex::Tok::Lit(l) => crate::lex::literal_text(l),
        _ => None,
    };
    let mut out: Vec<(usize, String)> = toks
        .iter()
        .filter_map(|t| decoded(t).map(|s| (t.line, s)))
        .collect();
    for (k, t) in toks.iter().enumerate() {
        let invoked = t.is_ident("concat")
            && toks.get(k + 1).is_some_and(|n| n.is_punct('!'))
            && toks
                .get(k + 2)
                .is_some_and(|n| matches!(n.tok, crate::lex::Tok::Punct('(' | '[' | '{')));
        if invoked {
            let close = crate::lex::group_end(&toks, k + 2);
            let joined: String = toks
                .get(k + 3..close)
                .unwrap_or_default()
                .iter()
                .filter_map(decoded)
                .collect();
            out.push((t.line, joined));
        }
    }
    out
}

#[must_use]
pub fn literal_names(src: &str, names: &[String]) -> Vec<(usize, String)> {
    let mut out: Vec<(usize, String)> = literal_texts(src)
        .into_iter()
        .flat_map(|(line, s)| {
            names_in(s.as_bytes(), names)
                .into_iter()
                .map(move |(l, n)| (line + l.saturating_sub(1), n))
        })
        .collect();
    out.sort();
    out.dedup();
    out
}

#[must_use]
pub fn named(root: &Path, rel: &str, names: &[String]) -> Vec<String> {
    let mut out: Vec<String> = names_in(rel.as_bytes(), names)
        .into_iter()
        .map(|(_, n)| format!("{rel}: the path holds the project name `{n}`"))
        .collect();
    if let Ok(bytes) = std::fs::read(root.join(rel)) {
        let raw = names_in(&bytes, names);
        out.extend(raw.iter().map(|(l, n)| {
            format!("{rel}:{l}: the project name `{n}`, outside examples/ and docs/")
        }));
        if rel.ends_with(".rs") {
            out.extend(
                literal_names(&crate::srcio::tool_text(&bytes), names)
                    .into_iter()
                    .filter(|f| !raw.contains(f))
                    .map(|(l, n)| {
                        format!(
                            "{rel}:{l}: the project name `{n}`, spelled across or inside string literals, \
                             outside examples/ and docs/"
                        )
                    }),
            );
        }
    }
    out
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Findings {
    pub files: usize,
    pub names: usize,
    pub scripts: Vec<String>,
    pub binaries: Vec<String>,
    pub denylisted: Vec<String>,
}

impl Findings {
    #[must_use]
    pub fn refusals(&self) -> Vec<String> {
        self.scripts
            .iter()
            .chain(&self.binaries)
            .chain(&self.denylisted)
            .cloned()
            .collect()
    }

    #[must_use]
    pub fn summary(&self) -> String {
        format!(
            "hygiene: {} file(s) listed; {} script(s), {} binar{}, {} of {} denylisted name(s) in code",
            self.files,
            self.scripts.len(),
            self.binaries.len(),
            if self.binaries.len() == 1 { "y" } else { "ies" },
            self.denylisted.len(),
            self.names
        )
    }
}

pub fn scan_files(root: &Path, files: &[String]) -> Result<Findings> {
    let names = denylist(root, files)?;
    let mut f = Findings {
        files: files.len(),
        names: names.len(),
        ..Findings::default()
    };
    for rel in files {
        f.scripts.extend(script(root, rel));
        f.binaries.extend(binary(root, rel));
        if in_name_scope(rel) {
            f.denylisted.extend(named(root, rel, &names));
        }
    }
    Ok(f)
}

pub fn scan(root: &Path) -> Result<Findings> {
    scan_files(root, &crate::tree::listed_files(root))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::srcio::own_write;

    #[test]
    fn a_script_is_refused_by_name_and_by_interpreter_line() {
        for p in ["tools/run.sh", "a.PY", "x/y.pl", "z.awk"] {
            assert!(script_by_name(p).is_some(), "{p}");
        }
        for p in ["src/main.rs", "docs/sh.md", "Cargo.toml", "shim/a.c"] {
            assert!(script_by_name(p).is_none(), "{p}");
        }
        assert!(interpreter_line(b"#!/bin/sh\necho\n").is_some());
        assert!(interpreter_line(b"#! /usr/bin/env python3\n").is_some());
        assert!(interpreter_line(b"#![allow(dead_code)]\n").is_none());
        assert!(interpreter_line(b"fn main() {}\n").is_none());
    }

    #[test]
    fn every_script_language_is_refused_and_ghidra_may_hold_javascript() {
        for p in [
            "crates/msvc6t/run.bash",
            "run.ps1",
            "x/y.zsh",
            "a.BAT",
            "b.cmd",
            "c.vbs",
            "tools/d.js",
            "e.rb",
            "f.lua",
            "g.tcl",
            "h.ksh",
            "i.fish",
            "j.psm1",
            "k.mjs",
        ] {
            assert!(script_by_name(p).is_some(), "{p}");
        }
        for p in [
            "ghidra/Export.js",
            "ghidra/Export.java",
            "res/app.rc",
            "a.toml",
        ] {
            assert!(script_by_name(p).is_none(), "{p}");
        }
    }

    #[test]
    fn every_archive_magic_is_recognised() {
        let mut tar = vec![b'a'; 512];
        tar.splice(257..262, *b"ustar");
        for (bytes, kind) in [
            (b"PK\x03\x04\x14\x00".to_vec(), "zip"),
            (b"PK\x05\x06".to_vec(), "zip"),
            (vec![0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c, 0, 4], "7z"),
            (vec![0x1f, 0x8b, 0x08, 0x00], "gzip"),
            (b"MSCF\0\0\0\0\x12".to_vec(), "cabinet"),
            (b"Rar!\x1a\x07\x00".to_vec(), "rar"),
            (vec![0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00], "xz"),
            (b"BZh91AY".to_vec(), "bzip2"),
            (vec![0x28, 0xb5, 0x2f, 0xfd], "zstd"),
            (tar, "tar"),
            (vec![0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1], "OLE"),
            (b"Microsoft Linker Database\n\n\x07\x00".to_vec(), ".ilk"),
        ] {
            let k = binary_kind(&bytes).unwrap_or_else(|| panic!("{kind} not recognised"));
            assert!(k.contains(kind), "{kind}: {k}");
        }
    }

    #[test]
    fn a_name_split_across_concat_pieces_or_escapes_is_found() {
        let names = vec!["AcmeRocket".to_string()];
        let src = "pub const A: &str = concat!(\"Acme\", \"Rocket\");\npub const B: &str = \"Acme\\x52ocket\";\n\
                   pub const C: &str = concat!(\"A\", concat!(\"cme\", \"R\"), \"ocket\");\npub const D: &str = \"Acme\";\n\
                   pub const E: &str = \"Rocket\";\n";
        assert_eq!(
            literal_names(src, &names),
            [
                (1, "AcmeRocket".to_string()),
                (2, "AcmeRocket".to_string()),
                (3, "AcmeRocket".to_string())
            ]
        );
    }

    #[test]
    fn every_binary_magic_is_recognised() {
        let mut coff = vec![0x4c, 0x01, 0x02, 0x00];
        coff.resize(20, 0);
        let mut import = vec![0x00, 0x00, 0xff, 0xff];
        import.resize(20, 0);
        let mut pe_optional = coff.clone();
        pe_optional[16] = 0xe0;
        for (bytes, kind) in [
            (b"MZ\x90\x00".to_vec(), "MZ"),
            (b"!<arch>\n/ ".to_vec(), "ar archive"),
            (b"Microsoft C/C++ MSF 7.00\r\n\x1aDS".to_vec(), "PDB 7.0"),
            (
                b"Microsoft C/C++ program database 2.00\r\n".to_vec(),
                "PDB 2.0",
            ),
            (b"Joy!peffpwpc".to_vec(), "PEF"),
            (vec![0xfe, 0xed, 0xfa, 0xce, 0, 0, 0, 18], "Mach-O"),
            (vec![0xcf, 0xfa, 0xed, 0xfe], "Mach-O"),
            (b"\x7fELF\x02".to_vec(), "ELF"),
            (coff, "COFF object"),
            (import, "import"),
        ] {
            let k = binary_kind(&bytes).unwrap_or_else(|| panic!("{kind} not recognised"));
            assert!(k.contains(kind), "{kind}: {k}");
        }
        assert_eq!(binary_kind(&pe_optional), None);
        assert_eq!(binary_kind(b"[package]\nname = \"x\"\n"), None);
        assert_eq!(binary_kind(b""), None);
    }

    #[test]
    fn names_are_found_case_blind_with_their_lines() {
        let names = vec!["Acme".to_string(), "rocket.exe".to_string()];
        let found = names_in(b"one\nan ACME tool\nROCKET.EXE and acme\n", &names);
        assert_eq!(
            found,
            [
                (2, "Acme".to_string()),
                (3, "Acme".to_string()),
                (3, "rocket.exe".to_string())
            ]
        );
    }

    #[test]
    fn the_name_scope_spares_examples_docs_and_markdown() {
        assert!(in_name_scope("crates/x/src/lib.rs"));
        assert!(in_name_scope("Cargo.toml"));
        assert!(!in_name_scope("examples/acme/msvc6t.toml"));
        assert!(!in_name_scope("docs/x.md"));
        assert!(!in_name_scope("README.md"));
    }

    #[test]
    fn a_planted_tree_is_refused_on_every_rule() {
        let root = std::env::temp_dir().join(format!("msvc6-lint-hygiene-{}", std::process::id()));
        if root.exists() {
            std::fs::remove_dir_all(&root).unwrap();
        }
        for d in ["examples/acme", "crates/x/src", "docs", "tools"] {
            std::fs::create_dir_all(root.join(d)).unwrap();
        }
        own_write(root.join("examples/acme/denylist.txt"), "Acme\n\nRocket\n").unwrap();
        own_write(root.join("examples/acme/msvc6t.toml"), "id = \"ACME\"\n").unwrap();
        own_write(root.join("docs/acme.txt"), "Acme\n").unwrap();
        own_write(
            root.join("crates/x/src/lib.rs"),
            "pub const N: &str = \"rocket\";\n",
        )
        .unwrap();
        own_write(root.join("tools/go"), "#!/bin/bash\nexit 0\n").unwrap();
        own_write(root.join("tools/x.sh"), "exit 0\n").unwrap();
        own_write(root.join("tools/run.bash"), "exit 0\n").unwrap();
        own_write(root.join("tools/run.ps1"), "exit 0\n").unwrap();
        own_write(root.join("tools/a.exe"), b"MZ\x90\x00\x03").unwrap();
        own_write(root.join("tools/a.zip"), b"PK\x03\x04\x14\x00MZ").unwrap();
        own_write(
            root.join("crates/x/src/split.rs"),
            "pub const S: &str = concat!(\"Roc\", \"ket\");\n",
        )
        .unwrap();
        let files: Vec<String> = crate::tree::files_under(&root, &root)
            .iter()
            .map(|p| crate::tree::relative(&root, p))
            .collect();
        let f = scan_files(&root, &files).unwrap();
        assert_eq!(f.names, 2);
        assert_eq!(f.scripts.len(), 4, "{:?}", f.scripts);
        assert_eq!(f.binaries.len(), 2, "{:?}", f.binaries);
        assert_eq!(f.denylisted.len(), 2, "{:?}", f.denylisted);
        assert!(f.denylisted[0].starts_with("crates/x/src/lib.rs:1:"));
        assert!(f.denylisted[1].starts_with("crates/x/src/split.rs:1:"));
        assert_eq!(f.refusals().len(), 8);
        std::fs::remove_dir_all(&root).unwrap();
    }
}
