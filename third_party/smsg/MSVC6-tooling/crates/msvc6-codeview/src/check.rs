use crate::codec::{self, Gen, NB_SYMBOLS};
use crate::nb::{Nb, SST_ALIGN_SYM, SST_GLOBAL_PUB, SST_GLOBAL_SYM, SST_GLOBAL_TYPES, sst_name};
use anyhow::{Context as _, Result};
use serde::Serialize;

pub const SAMPLES: usize = 8;

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Report {
    pub signature: String,
    pub bytes: usize,
    pub subsections: usize,
    pub container_identical: bool,
    pub types: usize,
    pub types_identical: usize,
    pub types_respelled: usize,
    pub symbols: usize,
    pub symbols_identical: usize,
    pub symbols_respelled: usize,
    pub problems: Vec<String>,
}

impl Report {
    #[must_use]
    pub fn identical(&self) -> bool {
        self.container_identical
            && self.types_identical == self.types
            && self.symbols_identical == self.symbols
    }

    fn note(&mut self, p: String) {
        if self.problems.len() < SAMPLES {
            self.problems.push(p);
        }
    }
}

fn symbol_region(sst: u16, d: &[u8]) -> Result<&[u8]> {
    match sst {
        SST_ALIGN_SYM => Ok(d.get(4..).unwrap_or_default()),
        _ => {
            let cbs = codec::us(codec::u32_at(d, 4)?);
            codec::bytes(d, 16, cbs).context("the symbols run past their subsection")
        }
    }
}

pub fn check(record: &[u8]) -> Result<Report> {
    let nb = Nb::parse(record)?;
    let mut r = Report {
        signature: nb.signature_text(),
        bytes: record.len(),
        subsections: nb.entries.len(),
        container_identical: nb.write()? == record,
        ..Report::default()
    };
    for e in &nb.entries {
        let d = nb.data(e);
        let what = format!("{} of module {}", sst_name(e.sst), e.imod);
        match e.sst {
            SST_GLOBAL_TYPES => {
                let n = codec::us(codec::u32_at(d, 4)?);
                if n > d.len().saturating_sub(8) / 4 {
                    anyhow::bail!("{what}: {n} types claimed, more than its offsets can hold");
                }
                let base = 8usize.saturating_add(n.saturating_mul(4));
                for i in 0..n {
                    let ti = 0x1000usize.saturating_add(i);
                    let off = codec::us(codec::u32_at(
                        d,
                        8usize.saturating_add(i.saturating_mul(4)),
                    )?);
                    let at = base.saturating_add(off);
                    let (_, _, end) =
                        codec::type_record_at(d, at).with_context(|| format!("type {ti:#x}"))?;
                    let rec = d.get(at..end).unwrap_or_default();
                    r.types = r.types.saturating_add(1);
                    match codec::respell_type(rec, Gen::St, Gen::St) {
                        Ok(w) if w == rec => {
                            r.types_identical = r.types_identical.saturating_add(1)
                        }
                        Ok(_) => r.note(format!("type {ti:#x} re-encodes to other bytes")),
                        Err(x) => r.note(format!("type {ti:#x}: {x:#}")),
                    }
                    match codec::respell_type(rec, Gen::St, Gen::Sz) {
                        Ok(_) => r.types_respelled = r.types_respelled.saturating_add(1),
                        Err(x) => r.note(format!("type {ti:#x} does not respell: {x:#}")),
                    }
                }
            }
            SST_ALIGN_SYM | SST_GLOBAL_SYM | SST_GLOBAL_PUB => {
                let region = symbol_region(e.sst, d).with_context(|| what.clone())?;
                for s in codec::walk(region, NB_SYMBOLS).with_context(|| what.clone())? {
                    r.symbols = r.symbols.saturating_add(1);
                    match codec::parse_sym(s.kind, s.payload, Gen::St)
                        .and_then(|p| codec::write_sym(&p, Gen::St, true))
                    {
                        Ok((k, w)) if k == s.kind && w == s.payload => {
                            r.symbols_identical = r.symbols_identical.saturating_add(1);
                        }
                        Ok(_) => r.note(format!(
                            "{what}: the {} at {:#x} re-encodes to other bytes",
                            codec::sym_name(s.kind),
                            s.at
                        )),
                        Err(x) => r.note(format!("{what}: the record at {:#x}: {x:#}", s.at)),
                    }
                    match codec::respell_sym(s.kind, s.payload, Gen::St, Gen::Sz) {
                        Ok(_) => r.symbols_respelled = r.symbols_respelled.saturating_add(1),
                        Err(x) => r.note(format!(
                            "{what}: the {} at {:#x} does not respell: {x:#}",
                            codec::sym_name(s.kind),
                            s.at
                        )),
                    }
                }
            }
            _ => {}
        }
    }
    Ok(r)
}

#[must_use]
pub fn lines(r: &Report) -> Vec<String> {
    let mut out = vec![
        format!(
            "{} record: {} bytes, {} subsection(s); the container writes back {}",
            r.signature,
            r.bytes,
            r.subsections,
            if r.container_identical {
                "byte for byte"
            } else {
                "to OTHER bytes"
            }
        ),
        format!(
            "  type records: {} of {} re-encode identically, {} respell to SZ",
            r.types_identical, r.types, r.types_respelled
        ),
        format!(
            "  symbol records: {} of {} re-encode identically, {} respell to SZ",
            r.symbols_identical, r.symbols, r.symbols_respelled
        ),
    ];
    out.extend(r.problems.iter().map(|p| format!("  PROBLEM {p}")));
    out.push(format!(
        "CV-CHECK: {}",
        if r.identical() {
            "IDENTICAL"
        } else {
            "DIFFERS"
        }
    ));
    out
}
