use crate::Outcome;
use anyhow::Result;
use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use msvc6_image::pe::image::IMAGE_SCN_MEM_EXECUTE;
use msvc6_pdb::pair::Derived;
use msvc6_pdb::sample;
use std::collections::BTreeMap;
use std::io::Write;
use std::path::PathBuf;

pub fn run(
    reference: &Image,
    ours: &Image,
    d: &Derived,
    dirs: &[PathBuf],
    want: usize,
    w: &mut dyn Write,
) -> Result<Outcome> {
    let mut pairs: BTreeMap<u32, (u32, Vec<String>)> = BTreeMap::new();
    for (a, p) in &d.pairs {
        if ours
            .section_of(Rva(*a))
            .is_some_and(|s| s.flags & IMAGE_SCN_MEM_EXECUTE == 0)
        {
            pairs.insert(*a, (p.refr, d.names.get(a).cloned().unwrap_or_default()));
        }
    }
    let defs = sample::collect(dirs)?;
    let s = sample::run(reference, &defs, &pairs, want);
    if s.pool > 0 {
        writeln!(
            w,
            "  sample pool: {} data pairing(s) our objects define a symbol for, {} of them with \
             unrelocated bytes to compare; 1 in {} taken",
            s.pool, s.with_bytes, s.step
        )?;
    }
    let (mut id, mut df, mut rel) = (0usize, 0usize, 0usize);
    writeln!(
        w,
        "\n  SAMPLE: {} data pairing(s) proved against OUR OBJECTS under {} -- the shipped bytes \
         at the paired address against the bytes the object supplies (its own DIR32 fields aside):",
        s.rows.len(),
        dirs.iter()
            .map(|p| p.display().to_string())
            .collect::<Vec<_>>()
            .join(" ")
    )?;
    for r in &s.rows {
        match r.verdict {
            "IDENTICAL" => id += 1,
            "DIFFERS" => df += 1,
            _ => rel += 1,
        }
        writeln!(
            w,
            "    {:9} ours {:#08x} -> {:#08x}  {:6} {:4} B compared, {} differ, {} relocated  {} {}",
            r.verdict, r.ours, r.refr, r.section, r.compared, r.differ, r.skipped, r.object, r.name
        )?;
    }
    writeln!(
        w,
        "  SAMPLE VERDICT: {id} IDENTICAL, {df} DIFFERS, {rel} with only relocated fields to compare"
    )?;
    if df > 0 {
        writeln!(
            w,
            "  FAIL: {df} sampled data pairing(s) DIFFER from our objects' bytes"
        )?;
        return Ok(Outcome::Differs);
    }
    Ok(Outcome::Ok)
}
