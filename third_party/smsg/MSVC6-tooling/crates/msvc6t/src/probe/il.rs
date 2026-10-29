use super::glob::collapse;
use anyhow::{Context as _, Result};
use msvc6_il::grammar::Grammar;
use msvc6_il::unit::{Files, Unit};
use std::path::Path;

#[must_use]
pub fn bk_switch(name: &str) -> String {
    format!("/Bk{name}.")
}

pub fn view(dir: &Path, name: &str, c2: &Path) -> Result<Vec<String>> {
    let g = Grammar::load(c2)?;
    let files = Files::open(&dir.join(format!("{name}.")), None)
        .with_context(|| format!("CL kept no IL for {name}"))?;
    let u = Unit::read(&files)?;
    let mut text = msvc6_il::dump::dump_unit(&files, &u, &g, &msvc6_il::dump::Opts::default())?;
    let ids = msvc6_il::ids::load_unit(&files, &u, &g)?;
    text.push_str("\n== funcs\n");
    text.push_str(&msvc6_il::ids::funcs_text(&ids));
    text.push_str("\n== ids\n");
    text.push_str(&msvc6_il::ids::ids_text(&ids));
    Ok(lines(&text))
}

#[must_use]
pub fn lines(text: &str) -> Vec<String> {
    text.lines()
        .map(collapse)
        .filter(|l| !l.is_empty())
        .collect()
}

#[must_use]
pub fn listing(asm: &str) -> Vec<String> {
    asm.lines()
        .map(|l| collapse(l.split(';').next().unwrap_or_default()))
        .filter(|l| !l.is_empty())
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_listing_view_is_every_code_line_without_comments() {
        let asm = "\tTITLE\tp.cpp\n?f@@YAHXZ PROC NEAR\t\t; f\n; 3 : int f()\n\tmov\teax, 1 ; one\n?f@@YAHXZ ENDP\n";
        assert_eq!(
            listing(asm),
            vec![
                "TITLE p.cpp",
                "?f@@YAHXZ PROC NEAR",
                "mov eax, 1",
                "?f@@YAHXZ ENDP"
            ]
        );
        assert_eq!(lines("a   b\n\n c "), vec!["a b", "c"]);
        assert_eq!(bk_switch("p"), "/Bkp.");
    }
}
