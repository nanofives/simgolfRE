use msvc6_image::rich::Rich;
use serde::Serialize;

pub const BUCKETS: [(u32, &str); 3] = [(0x0a, "C"), (0x0b, "C++"), (0x0e, "MASM")];

#[must_use]
pub fn bucket_of_source(source: &str) -> Option<u32> {
    match source.rsplit('.').next()?.to_ascii_lowercase().as_str() {
        "c" => Some(0x0a),
        "cpp" | "cxx" | "cc" => Some(0x0b),
        "asm" => Some(0x0e),
        _ => None,
    }
}

#[must_use]
pub fn signed(ours: u32, reference: u32) -> String {
    match ours.cmp(&reference) {
        std::cmp::Ordering::Equal => "0".to_string(),
        std::cmp::Ordering::Greater => format!("+{}", ours - reference),
        std::cmp::Ordering::Less => format!("-{}", reference - ours),
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Row {
    pub label: &'static str,
    pub prodid: u32,
    pub reference: u32,
    pub located: u32,
    pub ceiling: u32,
    pub vendored: u32,
    pub by_signal: Vec<(String, u32)>,
}

impl Row {
    #[must_use]
    pub fn own_max(&self) -> u32 {
        self.reference
            .saturating_sub(self.located)
            .saturating_sub(self.vendored)
    }

    #[must_use]
    pub fn own_min(&self) -> u32 {
        self.reference
            .saturating_sub(self.ceiling)
            .saturating_sub(self.vendored)
    }

    #[must_use]
    pub fn exact(&self) -> bool {
        self.own_min() == self.own_max()
    }

    #[must_use]
    pub fn band(&self) -> String {
        if self.exact() {
            format!("{} EXACT", self.own_min())
        } else {
            format!("{} .. {}", self.own_min(), self.own_max())
        }
    }

    #[must_use]
    pub fn holds(&self, known: u32) -> bool {
        known >= self.own_min() && known <= self.own_max()
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Measured {
    pub rows: Vec<Row>,
}

pub struct Counts<'a> {
    pub rich: &'a Rich,
    pub located: &'a dyn Fn(u32) -> (u32, Vec<(String, u32)>),
    pub ceiling: &'a dyn Fn(u32) -> u32,
    pub vendored: &'a dyn Fn(u32) -> u32,
}

impl std::fmt::Debug for Counts<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Counts").finish_non_exhaustive()
    }
}

#[must_use]
pub fn measure(c: &Counts<'_>) -> Measured {
    Measured {
        rows: BUCKETS
            .into_iter()
            .map(|(prodid, label)| {
                let (located, by_signal) = (c.located)(prodid);
                Row {
                    label,
                    prodid,
                    reference: c.rich.objects(prodid),
                    located,
                    ceiling: (c.ceiling)(prodid),
                    vendored: (c.vendored)(prodid),
                    by_signal,
                }
            })
            .collect(),
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Ours {
    pub sources: Vec<u32>,
    pub linked: Option<(String, Vec<u32>)>,
}

#[must_use]
pub fn source_counts(files: &[String]) -> Vec<u32> {
    let mut out = vec![0u32; BUCKETS.len()];
    for f in files {
        if let Some(p) = bucket_of_source(f)
            && let Some(slot) = BUCKETS
                .iter()
                .position(|(b, _)| *b == p)
                .and_then(|i| out.get_mut(i))
        {
            *slot += 1;
        }
    }
    out
}

#[must_use]
pub fn linked_counts(rich: &Rich) -> Vec<u32> {
    BUCKETS.into_iter().map(|(p, _)| rich.objects(p)).collect()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Control {
    pub label: String,
    pub known: Vec<u32>,
    pub rows: Vec<Row>,
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn report(label: &str, m: &Measured, ours: &Ours, control: Option<&Control>) -> Vec<String> {
    let mut out = vec![
        format!(
            "{label}: HOW MANY TRANSLATION UNITS OF OUR OWN the project had, per Rich language bucket."
        ),
        String::new(),
        "  own = reference - library. THE LIBRARY TERM IS BOUNDED FROM BOTH SIDES AND THEY DO NOT MEET, so `own' is \
         a BAND. Every column is a measurement; there is no fitted parameter."
            .to_string(),
        "    reference   objects the shipped image's Rich header counts for this bucket".to_string(),
        "    located     archive members the library table places in it -- a FLOOR on the library".to_string(),
        "    ceiling     members the archive HOLDS at all -- a CEILING on the library".to_string(),
        "    vendored    objects of a library held as SOURCE, so never located".to_string(),
        String::new(),
        "  THE MEASURED TERMS the band is built from:".to_string(),
    ];
    for r in &m.rows {
        out.push(format!(
            "    {:<4} {:#04x}   reference {:>4}   located {:>4}   ceiling {:>4}   vendored {:>3}",
            r.label, r.prodid, r.reference, r.located, r.ceiling, r.vendored
        ));
    }
    out.push(String::new());
    out.push(
        "  TWO DIFFERENT QUESTIONS, SIDE BY SIDE. Left compares our SOURCE FILES against the own band; right \
         compares our LINK's whole census against the reference's. A Rich count includes library members, so it can \
         only be read against another Rich count."
            .to_string(),
    );
    match &ours.linked {
        Some((p, _)) => out.push(format!("  our link: {p}")),
        None => out.push(
            "  NO LINK IS GIVEN, so the right-hand pair is blank; the left-hand pair needs only the source tree."
                .to_string(),
        ),
    }
    out.push(String::new());
    out.push(
        "  bucket        OWN TU BAND        our SOURCES   over  |  reference   our LINK   over"
            .to_string(),
    );
    for (i, r) in m.rows.iter().enumerate() {
        let src = ours.sources.get(i).copied().unwrap_or(0);
        let over_band = src.saturating_sub(r.own_max());
        let (lk, over_ref) = match ours.linked.as_ref().and_then(|(_, c)| c.get(i).copied()) {
            Some(n) => (n.to_string(), signed(n, r.reference)),
            None => ("-".to_string(), "-".to_string()),
        };
        out.push(format!(
            "  {:<4} {:#04x}   {:<17} {:>11}  {:>5}  |  {:>9}  {:>9}  {:>5}",
            r.label,
            r.prodid,
            r.band(),
            src,
            if over_band == 0 {
                "-".to_string()
            } else {
                format!("+{over_band}")
            },
            r.reference,
            lk,
            over_ref
        ));
    }
    out.push(String::new());
    out.push(
        "  WHICH SIGNAL WITNESSED EACH `located' TERM -- different strengths of evidence:"
            .to_string(),
    );
    for r in &m.rows {
        out.push(format!(
            "    {:<4} {}   union {:>3}",
            r.label,
            r.by_signal
                .iter()
                .map(|(k, n)| format!("{k} {n:>3}"))
                .collect::<Vec<_>>()
                .join("   "),
            r.located
        ));
    }
    if let Some(c) = control {
        out.push(String::new());
        out.push(format!(
            "  CONTROL -- {}, whose own object counts are known without this screen:",
            c.label
        ));
        for (r, known) in c.rows.iter().zip(&c.known) {
            out.push(format!(
                "    {:<4} band {:>3} .. {:<3}   known {known:>3}   {}",
                r.label,
                r.own_min(),
                r.own_max(),
                if r.holds(*known) {
                    "INSIDE"
                } else {
                    "OUTSIDE -- distrust the band, do NOT widen the formula"
                }
            ));
        }
        out.push(
            "    NO VERDICT IS DRAWN FROM THIS: it reports where the known figure falls."
                .to_string(),
        );
    }
    out.push(String::new());
    out.push(
        "  NEITHER `over' COLUMN IS A COUNT OF FILES TO DELETE. The left one is source consolidation; the right one \
         is the link's census, whose surplus can be code that should arrive from an archive rather than code that \
         does not belong. A band's upper end is not reachable while a third-party archive we do not hold puts its \
         members in `own'."
            .to_string(),
    );
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(reference: u32, located: u32, ceiling: u32, vendored: u32) -> Row {
        Row {
            label: "C++",
            prodid: 0x0b,
            reference,
            located,
            ceiling,
            vendored,
            by_signal: vec![("member".to_string(), located)],
        }
    }

    #[test]
    fn the_band_is_ordered_and_saturates() {
        let r = row(31, 8, 22, 0);
        assert_eq!((r.own_min(), r.own_max()), (9, 23));
        assert!(!r.exact());
        assert_eq!(r.band(), "9 .. 23");
        assert!(r.holds(9) && r.holds(23) && !r.holds(24));
        let full = row(34, 34, 73, 0);
        assert_eq!((full.own_min(), full.own_max()), (0, 0));
        assert!(full.exact());
        assert_eq!(full.band(), "0 EXACT");
    }

    #[test]
    fn a_shortfall_keeps_its_sign_and_an_extension_picks_the_front_end() {
        assert_eq!(signed(64, 141), "-77");
        assert_eq!(signed(794, 34), "+760");
        assert_eq!(signed(27, 27), "0");
        assert_eq!(bucket_of_source("pngread.c"), Some(0x0a));
        assert_eq!(bucket_of_source("Surface.cpp"), Some(0x0b));
        assert_eq!(bucket_of_source("text_1000.asm"), Some(0x0e));
        assert_eq!(bucket_of_source("README.md"), None);
        assert_eq!(bucket_of_source("noextension"), None);
        assert_eq!(
            source_counts(&["a.c".into(), "b.cpp".into(), "c.cpp".into(), "d.txt".into()]),
            vec![1, 2, 0]
        );
    }

    #[test]
    fn the_report_prints_both_comparisons_and_the_control() {
        let m = Measured {
            rows: vec![row(31, 8, 22, 0)],
        };
        let ours = Ours {
            sources: vec![30],
            linked: Some(("out/x.dll".to_string(), vec![29])),
        };
        let control = Control {
            label: "other.dll".to_string(),
            known: vec![6],
            rows: vec![row(27, 21, 21, 0)],
        };
        let lines = report("IMG", &m, &ours, Some(&control)).join("\n");
        assert!(
            lines.contains("9 .. 23                    30     +7  |         31         29     -2"),
            "{lines}"
        );
        assert!(lines.contains("known   6   INSIDE"), "{lines}");
        let no_link = report(
            "IMG",
            &m,
            &Ours {
                sources: vec![4],
                linked: None,
            },
            None,
        )
        .join("\n");
        assert!(
            no_link
                .contains("9 .. 23                     4      -  |         31          -      -"),
            "{no_link}"
        );
        assert!(!no_link.contains("CONTROL"), "{no_link}");
    }
}
