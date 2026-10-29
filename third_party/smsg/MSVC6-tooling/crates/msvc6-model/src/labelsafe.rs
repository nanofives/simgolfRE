use crate::labelsweep::{Curve, Fig, PERIOD};
use anyhow::{Result, bail};
use msvc6_base::bounded;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;
use std::path::Path;

pub const MAX_DELTAS: usize = 20;

pub const MAX_TABLE: u64 = 64 << 20;

pub const LABELS_HEADER: &str = "tu\tsymbol\tlabel";

#[must_use]
pub fn after_includes(text: &str) -> usize {
    text.lines()
        .enumerate()
        .filter(|(_, l)| {
            l.trim_start()
                .strip_prefix('#')
                .is_some_and(|r| r.trim_start().starts_with("include"))
        })
        .map(|(i, _)| i + 2)
        .last()
        .unwrap_or(1)
}

#[must_use]
pub fn ranges(v: &[usize]) -> String {
    let mut runs: Vec<(usize, usize)> = Vec::new();
    for &r in v {
        match runs.last_mut() {
            Some((_, hi)) if *hi + 1 == r => *hi = r,
            _ => runs.push((r, r)),
        }
    }
    if runs.is_empty() {
        return "-".to_string();
    }
    runs.iter()
        .map(|&(a, b)| {
            if a == b {
                a.to_string()
            } else {
                format!("{a}-{b}")
            }
        })
        .collect::<Vec<_>>()
        .join(",")
}

fn i64_of(n: usize) -> i64 {
    i64::try_from(n).unwrap_or(i64::MAX)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tied {
    pub tu: String,
    pub rva: u32,
    pub sym: String,
    pub base: Fig,
    pub at: Vec<Option<Fig>>,
}

impl Tied {
    #[must_use]
    pub fn fig(&self, r: usize) -> Option<Fig> {
        self.at.get(r).copied().flatten()
    }

    #[must_use]
    pub fn safe(&self, r: usize) -> Option<bool> {
        self.fig(r)
            .map(|f| f.0 <= self.base.0 && f.1 <= self.base.1 && f.2 <= self.base.2)
    }

    #[must_use]
    pub fn falls(&self, r: usize) -> bool {
        self.safe(r) == Some(true)
            && self.fig(r).is_some_and(|f| {
                f != self.base && (f.0 < self.base.0 || f.1 < self.base.1 || f.2 < self.base.2)
            })
    }

    #[must_use]
    pub fn of(tu: &str, rva: u32, sym: &str, base: Fig, curve: &Curve, swept: &[usize]) -> Self {
        let measured: BTreeMap<usize, Fig> = curve
            .iter()
            .map(|(n, d, e, a)| (*n, (*d, *e, *a)))
            .collect();
        let at = (0..PERIOD)
            .map(|r| {
                if r == 0 {
                    Some(base)
                } else if !swept.contains(&r) {
                    None
                } else {
                    Some(measured.get(&r).copied().unwrap_or(base))
                }
            })
            .collect();
        Self {
            tu: tu.to_string(),
            rva,
            sym: sym.to_string(),
            base,
            at,
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Delta {
    pub name: String,
    pub per_tu: BTreeMap<String, i64>,
    pub per_fn: BTreeMap<(String, String), i64>,
}

impl Delta {
    #[must_use]
    pub fn shift(&self, tu: &str, sym: &str) -> i64 {
        self.per_fn
            .get(&(tu.to_string(), sym.to_string()))
            .or_else(|| self.per_tu.get(tu))
            .copied()
            .unwrap_or(0)
    }

    #[must_use]
    pub fn touched(&self) -> BTreeSet<String> {
        self.per_tu
            .iter()
            .filter(|(_, d)| d.rem_euclid(64) != 0)
            .map(|(t, _)| t.clone())
            .chain(
                self.per_fn
                    .iter()
                    .filter(|(_, d)| d.rem_euclid(64) != 0)
                    .map(|((t, _), _)| t.clone()),
            )
            .collect()
    }
}

#[must_use]
pub fn labels_tsv(labels: &BTreeMap<(String, String), u32>) -> String {
    let mut out = format!("{LABELS_HEADER}\n");
    for ((tu, sym), l) in labels {
        out.push_str(&format!("{tu}\t{sym}\t{l}\n"));
    }
    out
}

pub fn parse_delta(spec: &str, base_labels: &BTreeMap<(String, String), u32>) -> Result<Delta> {
    let Some((name, file)) = spec.split_once(0x3d as char) else {
        bail!("--delta wants NAME=FILE, got `{spec}'");
    };
    if name.trim().is_empty() || file.trim().is_empty() {
        bail!("--delta wants NAME=FILE, and this one leaves a half empty: `{spec}'");
    }
    let text = bounded::tool(Path::new(file), MAX_TABLE)?;
    let mut d = Delta {
        name: name.to_string(),
        ..Delta::default()
    };
    for l in text.lines().filter(|l| *l != LABELS_HEADER) {
        let cols: Vec<&str> = l.split('\t').collect();
        if let [tu, sym, lab] = cols.as_slice() {
            let Ok(lab) = lab.parse::<u32>() else {
                continue;
            };
            if let Some(&b) = base_labels.get(&((*tu).to_string(), (*sym).to_string())) {
                d.per_fn.insert(
                    ((*tu).to_string(), (*sym).to_string()),
                    i64::from(lab) - i64::from(b),
                );
            }
            continue;
        }
        if let Some((stem, rest)) = l.split_once(": C1 counter ")
            && let Some(s) = rest.split(", shift ").nth(1)
        {
            let s: String = s
                .chars()
                .take_while(|c| *c == '+' || *c == '-' || c.is_ascii_digit())
                .collect();
            if let Ok(v) = s.trim_start_matches('+').parse::<i64>() {
                d.per_tu.insert(stem.trim().to_string(), v);
            }
        }
    }
    if d.per_tu.is_empty() && d.per_fn.is_empty() {
        bail!("{file}: no `--c1' shift rows and no label rows this run can pair");
    }
    Ok(d)
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Table {
    pub tsv: String,
    pub lines: Vec<String>,
}

#[must_use]
pub fn table(tied: &[Tied], flat: &[String], at: &BTreeMap<String, usize>) -> Table {
    let mut t = Table {
        tsv: String::from("tu\trva\tsymbol\tdiffering\tedit\taddend\tsafe\tfalls\tat\n"),
        ..Table::default()
    };
    let mut by_tu: BTreeMap<&str, Vec<&Tied>> = BTreeMap::new();
    for x in tied {
        by_tu.entry(x.tu.as_str()).or_default().push(x);
    }
    for (tu, fns) in &by_tu {
        let line = at.get(*tu).copied().unwrap_or(0);
        for x in fns {
            let safe: Vec<usize> = (0..PERIOD).filter(|r| x.safe(*r) == Some(true)).collect();
            let falls: Vec<String> = (0..PERIOD)
                .filter(|r| x.falls(*r))
                .map(|r| {
                    let f = x.fig(r).unwrap_or(x.base);
                    format!(
                        "{r}:e{:+}/d{:+}",
                        i64_of(f.1) - i64_of(x.base.1),
                        i64_of(f.0) - i64_of(x.base.0)
                    )
                })
                .collect();
            let _ = writeln!(
                t.tsv,
                "{tu}\t{:#x}\t{}\t{}\t{}\t{}\t{}\t{}\t{line}",
                x.rva,
                x.sym,
                x.base.0,
                x.base.1,
                x.base.2,
                ranges(&safe),
                if falls.is_empty() {
                    "-".to_string()
                } else {
                    falls.join(",")
                }
            );
        }
        let safe: Vec<usize> = (0..PERIOD)
            .filter(|r| fns.iter().all(|x| x.safe(*r) == Some(true)))
            .collect();
        let gains: Vec<(i64, usize)> = safe
            .iter()
            .map(|&r| {
                let g: i64 = fns
                    .iter()
                    .map(|x| i64_of(x.fig(r).unwrap_or(x.base).1) - i64_of(x.base.1))
                    .sum();
                (g, r)
            })
            .filter(|(g, _)| *g < 0)
            .collect();
        let falls: Vec<String> = gains.iter().map(|(g, r)| format!("{r}:e{g:+}")).collect();
        let (d0, e0, a0) = fns.iter().fold((0, 0, 0), |(d, e, a), x| {
            (d + x.base.0, e + x.base.1, a + x.base.2)
        });
        let _ = writeln!(
            t.tsv,
            "{tu}\t*\tTU\t{d0}\t{e0}\t{a0}\t{}\t{}\t{line}",
            ranges(&safe),
            if falls.is_empty() {
                "-".to_string()
            } else {
                falls.join(",")
            }
        );
        t.lines.push(format!(
            "{tu}: {} tied function(s), safe at {} of {PERIOD} residues: {}{}",
            fns.len(),
            safe.len(),
            ranges(&safe),
            if falls.is_empty() {
                String::new()
            } else {
                format!("; falls at {}", falls.join(" "))
            }
        ));
    }
    for f in flat {
        let _ = writeln!(t.tsv, "{f}\t*\tFLAT\t0\t0\t0\t0-{}\t-\t0", PERIOD - 1);
    }
    t
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Priced {
    pub names: Vec<String>,
    pub risers: Vec<String>,
    pub falls: Vec<String>,
    pub unknown: Vec<String>,
    pub unswept: BTreeSet<String>,
    pub edit: i64,
    pub differing: i64,
}

#[must_use]
pub fn price(set: &[&Delta], tied: &[Tied], swept: &BTreeSet<String>) -> Priced {
    let mut p = Priced {
        names: set.iter().map(|d| d.name.clone()).collect(),
        ..Priced::default()
    };
    for t in tied {
        let s: i64 = set.iter().map(|d| d.shift(&t.tu, &t.sym)).sum();
        let r = usize::try_from(s.rem_euclid(64)).unwrap_or(0);
        let Some(f) = t.fig(r) else {
            p.unknown.push(format!("{:#x}@{r}", t.rva));
            continue;
        };
        let (de, dd) = (
            i64_of(f.1) - i64_of(t.base.1),
            i64_of(f.0) - i64_of(t.base.0),
        );
        p.edit += de;
        p.differing += dd;
        let tag = format!("{:#x}({}) {s:+}:e{de:+}/d{dd:+}", t.rva, t.tu);
        if t.safe(r) == Some(false) {
            p.risers.push(tag);
        } else if t.falls(r) {
            p.falls.push(tag);
        }
    }
    for d in set {
        for tu in d.touched() {
            if !swept.contains(&tu) {
                p.unswept.insert(tu);
            }
        }
    }
    p
}

impl Priced {
    #[must_use]
    pub fn line(&self) -> String {
        let list = |tag: &str, v: &[String]| {
            if v.is_empty() {
                String::new()
            } else {
                format!("{tag}[{}]", v.join(", "))
            }
        };
        format!(
            "{}: edit {:+} differing {:+}; {} riser(s){}{}{}{}",
            self.names.join("+"),
            self.edit,
            self.differing,
            self.risers.len(),
            list(" ", &self.risers),
            list("; falls ", &self.falls),
            if self.unknown.is_empty() {
                String::new()
            } else {
                format!("; UNKNOWN {}", self.unknown.join(","))
            },
            if self.unswept.is_empty() {
                String::new()
            } else {
                format!(
                    "; UNSWEPT {}",
                    self.unswept.iter().cloned().collect::<Vec<_>>().join(",")
                )
            }
        )
    }
}

pub fn search(
    deltas: &[Delta],
    tied: &[Tied],
    swept: &BTreeSet<String>,
    max: Option<usize>,
    top: usize,
) -> Result<Vec<String>> {
    let n = deltas.len();
    if n > MAX_DELTAS {
        bail!("{n} deltas: at most {MAX_DELTAS} (2^{MAX_DELTAS} combinations)");
    }
    let max = max.unwrap_or(n);
    let mut all: Vec<Priced> = Vec::new();
    for mask in 1u32..(1u32 << n) {
        if usize::try_from(mask.count_ones()).unwrap_or(usize::MAX) > max {
            continue;
        }
        let set: Vec<&Delta> = deltas
            .iter()
            .enumerate()
            .filter(|(i, _)| mask & (1 << i) != 0)
            .map(|(_, d)| d)
            .collect();
        all.push(price(&set, tied, swept));
    }
    let mut out: Vec<String> = deltas
        .iter()
        .map(|d| {
            let mut v: BTreeMap<i64, usize> = BTreeMap::new();
            for s in d.per_tu.values().chain(d.per_fn.values()) {
                *v.entry(*s).or_default() += 1;
            }
            format!(
                "DELTA {}: {} TU row(s), {} function row(s); shifts {}",
                d.name,
                d.per_tu.len(),
                d.per_fn.len(),
                v.iter()
                    .map(|(s, k)| format!("{s:+} x{k}"))
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        })
        .collect();
    let mut clean: Vec<&Priced> = all
        .iter()
        .filter(|p| p.risers.is_empty() && p.unknown.is_empty())
        .collect();
    clean.sort_by_key(|p| (p.edit, p.differing, p.names.len()));
    out.push(format!(
        "{} combination(s) priced; {} with no riser:",
        all.len(),
        clean.len()
    ));
    out.extend(
        clean
            .iter()
            .take(top)
            .map(|p| format!("  CLEAN {}", p.line())),
    );
    let mut near: Vec<&Priced> = all.iter().filter(|p| !p.risers.is_empty()).collect();
    near.sort_by_key(|p| (p.risers.len(), p.edit, p.names.len()));
    out.extend(
        near.iter()
            .take(top)
            .map(|p| format!("  NEAR  {}", p.line())),
    );
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ranges_compact_runs_and_the_pad_line_is_past_the_last_include() {
        assert_eq!(ranges(&[0, 1, 2, 5, 7, 8]), "0-2,5,7-8");
        assert_eq!(ranges(&[]), "-");
        assert_eq!(
            after_includes("x\n#include \"a.h\"\n#  include <b.h>\nint x;\n"),
            4
        );
        assert_eq!(after_includes("int x;\n"), 1);
    }

    #[test]
    fn a_delta_reads_a_c1_shift_row_and_a_label_table() {
        let d = std::env::temp_dir().join(format!("labelsafe-delta-{}.txt", std::process::id()));
        msvc6_base::srcio::own_write(
            &d,
            "Toolbar: C1 counter 100 -> 83, shift -17 (mod 64 47) [C1XX 8168]\nnoise\n",
        )
        .unwrap();
        let got = parse_delta(&format!("A={}", d.display()), &BTreeMap::new()).unwrap();
        assert_eq!(got.per_tu.get("Toolbar"), Some(&-17));
        assert_eq!(got.shift("Toolbar", "?f@@YAXXZ"), -17);
        assert_eq!(got.touched(), ["Toolbar".to_string()].into());
        msvc6_base::srcio::own_write(
            &d,
            "tu\tsymbol\tlabel\nT\t?f@@YAXXZ\t120\nT\t?g@@YAXXZ\t9\n",
        )
        .unwrap();
        let base: BTreeMap<(String, String), u32> =
            [(("T".to_string(), "?f@@YAXXZ".to_string()), 100)].into();
        let got = parse_delta(&format!("B={}", d.display()), &base).unwrap();
        assert_eq!(got.shift("T", "?f@@YAXXZ"), 20);
        assert_eq!(got.shift("T", "?g@@YAXXZ"), 0);
        std::fs::remove_file(&d).ok();
    }

    fn pad0(stem: &str, cod: &str) -> BTreeMap<(String, String), u32> {
        crate::hdrprice::first_labels(cod)
            .into_iter()
            .map(|(sym, l)| ((stem.to_string(), sym), l))
            .collect()
    }

    #[test]
    fn a_labels_table_written_by_one_tree_is_a_per_function_delta_against_another() {
        let here = pad0(
            "Tile",
            "?f@@YAXXZ PROC NEAR\n$L100:\n jmp SHORT $L98\n?f@@YAXXZ ENDP\n_g PROC NEAR\n $L7:\n_g ENDP\n",
        );
        let edited = pad0(
            "Tile",
            "?f@@YAXXZ PROC NEAR\n$L104:\n jmp SHORT $L101\n?f@@YAXXZ ENDP\n_g PROC NEAR\n $L7:\n_g ENDP\n",
        );
        let text = labels_tsv(&edited);
        assert_eq!(
            text,
            format!("{LABELS_HEADER}\nTile\t?f@@YAXXZ\t101\nTile\t_g\t7\n")
        );
        let d = std::env::temp_dir().join(format!("labelsafe-trip-{}.tsv", std::process::id()));
        msvc6_base::srcio::own_write(&d, &text).unwrap();
        let got = parse_delta(&format!("E={}", d.display()), &here).unwrap();
        let unpaired = parse_delta(&format!("E={}", d.display()), &BTreeMap::new());
        std::fs::remove_file(&d).ok();
        assert_eq!(got.shift("Tile", "?f@@YAXXZ"), 3);
        assert_eq!(got.shift("Tile", "_g"), 0);
        assert_eq!(got.per_fn.len(), 2);
        assert_eq!(got.touched(), ["Tile".to_string()].into());
        assert!(
            unpaired
                .unwrap_err()
                .to_string()
                .contains("no label rows this run can pair")
        );
    }

    fn tied(tu: &str, curve: &Curve, swept: &[usize]) -> Tied {
        Tied::of(tu, 0x1000, "?f@@YAXXZ", (4, 8, 1), curve, swept)
    }

    #[test]
    fn a_residue_the_sweep_skipped_is_unknown_and_a_riser_is_not_clean() {
        let t = tied("T", &vec![(16, 0, 0, 0), (32, 9, 20, 2)], &[16, 32]);
        assert_eq!(t.safe(0), Some(true));
        assert_eq!(t.safe(16), Some(true));
        assert!(t.falls(16));
        assert_eq!(t.safe(32), Some(false));
        assert_eq!(t.fig(5), None);
        let mk = |name: &str, shift: i64| Delta {
            name: name.to_string(),
            per_tu: [("T".to_string(), shift)].into(),
            per_fn: BTreeMap::new(),
        };
        let swept: BTreeSet<String> = ["T".to_string()].into();
        let good = price(&[&mk("A", 16)], std::slice::from_ref(&t), &swept);
        assert!(good.risers.is_empty() && good.falls.len() == 1 && good.edit == -8);
        let bad = price(&[&mk("B", 32)], std::slice::from_ref(&t), &swept);
        assert_eq!(bad.risers.len(), 1);
        let unknown = price(&[&mk("C", 5)], std::slice::from_ref(&t), &swept);
        assert_eq!(unknown.unknown.len(), 1);
        let elsewhere = price(&[&mk("D", 16)], &[t], &BTreeSet::new());
        assert_eq!(elsewhere.unswept, ["T".to_string()].into());
        let lines = search(
            &[mk("A", 16), mk("B", 32)],
            &[tied("T", &vec![(16, 0, 0, 0)], &[16, 32])],
            &swept,
            None,
            4,
        )
        .unwrap();
        assert!(
            lines.iter().any(|l| l.starts_with("  CLEAN A:")),
            "{lines:?}"
        );
    }

    #[test]
    fn the_table_rows_name_each_tus_safe_residues() {
        let t = tied("T", &vec![(16, 0, 0, 0)], &[16]);
        let at: BTreeMap<String, usize> = [("T".to_string(), 12)].into();
        let table = table(&[t], &["FLATTU".to_string()], &at);
        assert!(
            table
                .tsv
                .contains("T\t0x1000\t?f@@YAXXZ\t4\t8\t1\t0,16\t16:e-8/d-4\t12\n"),
            "{}",
            table.tsv
        );
        assert!(
            table.tsv.contains("T\t*\tTU\t4\t8\t1\t0,16\t16:e-8\t12\n"),
            "{}",
            table.tsv
        );
        assert!(table.tsv.contains("FLATTU\t*\tFLAT\t0\t0\t0\t0-63\t-\t0\n"));
        assert_eq!(
            table.lines,
            ["T: 1 tied function(s), safe at 2 of 64 residues: 0,16; falls at 16:e-8"]
        );
    }
}
