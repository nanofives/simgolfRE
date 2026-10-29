use crate::cursor::Cursor;
use crate::ex::{self, Arg};
use crate::grammar::Grammar;
use crate::ops;
use crate::sym::{self, Sym};
use crate::tree::{self, Trees};
use crate::unit::{Files, Stream, Unit, stream_of};
use anyhow::{Context as _, Result, bail};
use std::collections::{BTreeMap, HashSet};
use std::fmt::Write as _;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Opts {
    pub hex: bool,
    pub tree: bool,
}

impl Default for Opts {
    fn default() -> Self {
        Self {
            hex: false,
            tree: true,
        }
    }
}

fn hexbytes(d: &[u8]) -> String {
    let mut s = String::new();
    for b in d {
        let _ = write!(s, "{b:02x} ");
    }
    s
}

fn sym_lines(out: &mut String, data: &[u8], v: &[Sym], hex: bool) {
    for s in v {
        let _ = writeln!(out, "{:04x}  {}", s.off, sym::render(s));
        if hex {
            let _ = writeln!(
                out,
                "      {}",
                hexbytes(data.get(s.off..s.end).unwrap_or_default())
            );
        }
    }
}

pub fn dump(files: &Files, g: &Grammar, opts: &Opts) -> Result<String> {
    let u = Unit::read(files)?;
    dump_unit(files, &u, g, opts)
}

pub fn dump_unit(files: &Files, u: &Unit, g: &Grammar, opts: &Opts) -> Result<String> {
    let mut out = String::new();
    let base = &files.base;
    let _ = writeln!(
        out,
        "IL set {base}  (gl {} B, in {} B, sy {} B, ex {} B)",
        files.gl.len(),
        files.init.len(),
        files.sy.len(),
        files.ex.len()
    );
    if files.db {
        let _ = writeln!(
            out,
            "  (a db stream {base}db exists; C2 does not read it here)"
        );
    }
    let h = &u.header;
    let _ = writeln!(out, "\n== gl  header");
    let _ = writeln!(
        out,
        "0000  magic 0x{:02x} tag 0x{:04x} stamp 0x{:08x} next-id #{:x} lang {}{}{}",
        h.magic,
        h.tag,
        h.stamp,
        h.next_id,
        h.lang,
        h.flags
            .map(|f| format!(" flags 0x{f:x}"))
            .unwrap_or_default(),
        h.front_end()
            .map(|v| format!(" front-end {v}"))
            .unwrap_or_default()
    );
    let _ = writeln!(
        out,
        "      pdb-ids {} wide-sizes {}",
        h.ids == crate::cursor::Ids::Compact,
        h.wide
    );
    sym_lines(&mut out, &files.gl, &u.globals, opts.hex);
    if u.gl_tail < files.gl.len() {
        let _ = writeln!(
            out,
            "{:04x}  ({} bytes after the list C2 does not read)",
            u.gl_tail,
            files.gl.len().saturating_sub(u.gl_tail)
        );
    }
    let _ = writeln!(out, "\n== in");
    crate::init::dump(&files.init, h.ids, &|i| u.full(i), &mut out)
        .with_context(|| format!("{base}in"))?;
    let funcs = u.fdefs();
    let starts: BTreeMap<usize, &Sym> = funcs
        .iter()
        .filter(|f| stream_of(f) == Stream::Ex)
        .filter_map(|f| Some((usize::try_from(f.ex?).ok()?, *f)))
        .collect();
    let _ = writeln!(out, "\n== sy  ({} lists)", u.lists.len());
    for (start, v) in &u.lists {
        let owner = funcs.iter().find(|f| {
            stream_of(f) == Stream::Ex && f.sy.and_then(|s| usize::try_from(s).ok()) == Some(*start)
        });
        let _ = writeln!(
            out,
            "-- list at 0x{start:x}{}",
            owner.map_or_else(
                || " (no fdef names it)".to_string(),
                |f| format!(" (function {})", u.full(f.id.unwrap_or(0)))
            )
        );
        sym_lines(&mut out, &files.sy, v, opts.hex);
    }
    let _ = writeln!(out, "\n== ex");
    let ex_bytes = &files.ex;
    let mut c = Cursor::new(ex_bytes, 0).with_ids(h.ids);
    let dlp = u.dlp();
    let body = if ex_bytes.first() == Some(&ops::EXHEAD) {
        let head = ex::read(&mut c, g, dlp).with_context(|| format!("{base}ex"))?;
        let Some(body) = head.long().and_then(|n| usize::try_from(n).ok()) else {
            bail!("{base}ex: the 0x5b header record names no body offset");
        };
        let _ = writeln!(
            out,
            "0000  5b    exhead body at 0x{body:x} (the bytes up to it are reserved, zero)"
        );
        let nz = ex_bytes
            .get(head.end..body.min(ex_bytes.len()))
            .unwrap_or_default()
            .iter()
            .filter(|&&b| b != 0)
            .count();
        if nz != 0 {
            let _ = writeln!(out, "      ({nz} non-zero bytes in the reserved range)");
        }
        body
    } else {
        let _ = writeln!(
            out,
            "0000  (no 0x5b header record: a pch compile's .ex, bodies from offset 0; pch bodies are not in this file)"
        );
        0
    };
    c.seek(body);
    let mut trees = Trees::new();
    let mut firsts: Vec<String> = Vec::new();
    let mut seen: HashSet<u32> = HashSet::new();
    let mut in_fn = false;
    while !c.at_end() {
        if let Some(f) = starts.get(&c.pos()) {
            let _ = writeln!(
                out,
                "-- function {} at 0x{:x} (sy list 0x{:x})",
                u.full(f.id.unwrap_or(0)),
                c.pos(),
                f.sy.unwrap_or(0)
            );
            trees = Trees::new();
            firsts.clear();
            seen.clear();
            in_fn = true;
        }
        let r = ex::read(&mut c, g, dlp).with_context(|| format!("{base}ex"))?;
        let mut line = format!("{:04x}  {}", r.off, tree::rec_text(u, &r));
        if in_fn
            && matches!(r.op, ops::SYM | ops::PARAM | ops::CTEMP)
            && let Some(i) = r.args.iter().find_map(|a| match a {
                Arg::Id(i) => Some(*i),
                _ => None,
            })
            && seen.insert(i)
        {
            firsts.push(u.short(i));
            line.push_str(" [1st]");
        }
        if opts.tree
            && let Some(stmt) = trees.step(g, u, &r).stmt
        {
            let pad = 56usize.saturating_sub(line.len());
            line.push_str(&" ".repeat(pad));
            line.push_str("  ; ");
            line.push_str(&stmt);
        }
        let _ = writeln!(out, "{line}");
        if opts.hex {
            let _ = writeln!(
                out,
                "      {}",
                hexbytes(ex_bytes.get(r.off..r.end).unwrap_or_default())
            );
        }
        if in_fn && r.ends_body() {
            let _ = writeln!(out, "      first refs: {}", firsts.join(" "));
            in_fn = false;
        }
        if r.op == ops::EOF {
            if !c.at_end() {
                let _ = writeln!(
                    out,
                    "{:04x}  ({} bytes after the eof record)",
                    c.pos(),
                    ex_bytes.len().saturating_sub(c.pos())
                );
            }
            break;
        }
    }
    Ok(out)
}

pub fn dump_function(files: &Files, u: &Unit, g: &Grammar, f: &Sym, opts: &Opts) -> Result<String> {
    let recs = u
        .body(files, g, f)
        .with_context(|| format!("{}: {}", files.base, f.name_text()))?;
    let data: &[u8] = match stream_of(f) {
        Stream::Ex => &files.ex,
        Stream::Pch => files.pch.as_deref().unwrap_or_default(),
    };
    let mut out = String::new();
    let _ = writeln!(
        out,
        "-- function {} at 0x{:x} in {} (sy list 0x{:x})",
        u.full(f.id.unwrap_or(0)),
        f.ex.unwrap_or(0),
        if stream_of(f) == Stream::Pch {
            "pch"
        } else {
            "ex"
        },
        f.sy.unwrap_or(0)
    );
    let mut trees = Trees::new();
    for r in &recs {
        let mut line = format!("{:04x}  {}", r.off, tree::rec_text(u, r));
        if opts.tree
            && let Some(stmt) = trees.step(g, u, r).stmt
        {
            let pad = 56usize.saturating_sub(line.len());
            line.push_str(&" ".repeat(pad));
            line.push_str("  ; ");
            line.push_str(&stmt);
        }
        let _ = writeln!(out, "{line}");
        if opts.hex {
            let _ = writeln!(
                out,
                "      {}",
                hexbytes(data.get(r.off..r.end).unwrap_or_default())
            );
        }
    }
    Ok(out)
}
