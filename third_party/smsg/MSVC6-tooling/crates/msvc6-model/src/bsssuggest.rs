use crate::bss::{bucket_of, classify};
use anyhow::{Context as _, Result, bail};
use std::collections::BTreeSet;
use std::path::Path;

pub const MAX_VOCABULARY_BYTES: u64 = 1 << 20;

pub const MAX_SPELLINGS: usize = 1 << 22;

pub const MAX_SPELLING_BYTES: usize = 1 << 28;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Vocabulary {
    pub groups: Vec<Vec<String>>,
    pub qualifiers: Vec<String>,
}

impl Vocabulary {
    pub fn parse(text: &str, file: &Path) -> Result<Self> {
        let mut v = Self::default();
        for (n, line) in text.lines().enumerate() {
            let mut w = line.split_whitespace();
            match w.next() {
                None => {}
                Some("group") => {
                    let g: Vec<String> = w.map(str::to_lowercase).collect();
                    if g.len() < 2 {
                        bail!(
                            "{}:{}: a group names at least two words",
                            file.display(),
                            n + 1
                        );
                    }
                    v.groups.push(g);
                }
                Some("qualifiers") => v.qualifiers.extend(w.map(str::to_lowercase)),
                Some(other) => bail!(
                    "{}:{}: `{other}`: a line is `group WORD WORD...` or `qualifiers WORD...`",
                    file.display(),
                    n + 1
                ),
            }
        }
        Ok(v)
    }

    pub fn read(file: &Path) -> Result<Self> {
        let len = std::fs::metadata(file)
            .with_context(|| file.display().to_string())?
            .len();
        if len > MAX_VOCABULARY_BYTES {
            bail!(
                "{}: {len} bytes, more than the {MAX_VOCABULARY_BYTES} a vocabulary may hold",
                file.display()
            );
        }
        let text = msvc6_base::srcio::read(file).with_context(|| file.display().to_string())?;
        Self::parse(&text, file)
    }
}

fn split(name: &str) -> (String, Vec<String>) {
    let cut = name.rfind('_').map_or(0, |i| i + 1);
    let (pre, body) = name.split_at(cut);
    let mut words: Vec<String> = Vec::new();
    let mut cur = String::new();
    let mut prev_upper = false;
    for ch in body.chars() {
        let upper = ch.is_ascii_uppercase() || ch.is_ascii_digit();
        if upper && !prev_upper && !cur.is_empty() {
            words.push(std::mem::take(&mut cur));
        }
        cur.push(ch.to_ascii_lowercase());
        prev_upper = upper;
    }
    if !cur.is_empty() {
        words.push(cur);
    }
    (pre.to_string(), words)
}

fn join(pre: &str, words: &[&str]) -> String {
    let mut s = pre.to_string();
    for (i, w) in words.iter().enumerate() {
        if i == 0 {
            s.push_str(w);
        } else {
            let mut c = w.chars();
            if let Some(f) = c.next() {
                s.push(f.to_ascii_uppercase());
                s.push_str(c.as_str());
            }
        }
    }
    s
}

fn alternatives(w: &str, voc: &Vocabulary) -> Vec<String> {
    let mut out: Vec<String> = vec![w.to_string()];
    let mut add = |s: String| {
        if !out.contains(&s) {
            out.push(s);
        }
    };
    if let Some(stem) = w
        .strip_suffix('s')
        .filter(|s| s.len() > 1 && !s.ends_with('s'))
    {
        add(stem.to_string());
    } else if w.len() > 1 {
        add(format!("{w}s"));
    }
    for g in &voc.groups {
        if g.iter().any(|m| m == w) {
            for m in g {
                add(m.clone());
            }
        }
    }
    out
}

pub fn advance(at: &mut [usize], lens: &[usize]) -> bool {
    for (i, &n) in at.iter_mut().zip(lens) {
        *i += 1;
        if *i < n {
            return true;
        }
        *i = 0;
    }
    false
}

fn decompose(name: &str) -> (&str, &str, &str) {
    let Some(body) = name.strip_prefix('?') else {
        return ("", name, "");
    };
    let at = body.find('@').unwrap_or(body.len());
    (
        "?",
        body.get(..at).unwrap_or_default(),
        body.get(at..).unwrap_or_default(),
    )
}

#[must_use]
pub fn bucket(sym: &str) -> u32 {
    bucket_of(&classify(sym).1)
}

fn widest<I, J>(slots: I) -> usize
where
    I: Iterator<Item = J>,
    J: Iterator<Item = usize>,
{
    slots
        .map(|s| s.max().unwrap_or(0))
        .fold(0usize, usize::saturating_add)
}

fn product(lens: &[usize]) -> usize {
    lens.iter()
        .try_fold(1usize, |a, &n| a.checked_mul(n.max(1)))
        .unwrap_or(usize::MAX)
}

pub fn suggest(
    name: &str,
    lo: u32,
    hi: u32,
    voc: &Vocabulary,
) -> Result<Vec<(usize, u32, String)>> {
    let (lead, ident, tail) = decompose(name);
    let (pre, words) = split(ident);
    let alts: Vec<Vec<String>> = words.iter().map(|w| alternatives(w, voc)).collect();
    let lens: Vec<usize> = alts.iter().map(Vec::len).collect();
    let forms = 1 + 2 * voc.qualifiers.len();
    if product(&lens).saturating_mul(forms) > MAX_SPELLINGS {
        bail!(
            "{name}: its words and qualifiers offer more than {MAX_SPELLINGS} spellings; shorten the vocabulary"
        );
    }
    let width = name
        .len()
        .saturating_add(widest(alts.iter().map(|a| a.iter().map(String::len))))
        .saturating_add(voc.qualifiers.iter().map(String::len).max().unwrap_or(0));
    if product(&lens).saturating_mul(forms).saturating_mul(width) > MAX_SPELLING_BYTES {
        bail!(
            "{name}: its spellings would hold more than {MAX_SPELLING_BYTES} bytes; shorten the vocabulary"
        );
    }
    let mut seen: BTreeSet<String> = BTreeSet::new();
    let mut out: Vec<(usize, u32, String)> = Vec::new();
    let mut at = vec![0usize; alts.len()];
    loop {
        let base: Vec<&str> = at
            .iter()
            .zip(&alts)
            .filter_map(|(&i, a)| a.get(i).map(String::as_str))
            .collect();
        let changed = at.iter().filter(|&&i| i != 0).count();
        let mut variants: Vec<(usize, Vec<&str>)> = vec![(changed, base.clone())];
        for q in &voc.qualifiers {
            let mut front = vec![q.as_str()];
            front.extend(base.iter().copied());
            variants.push((changed + 1, front));
            let mut back = base.clone();
            back.push(q);
            variants.push((changed + 1, back));
        }
        for (cost, f) in variants {
            let s = format!("{lead}{}{tail}", join(&pre, &f));
            if s == name || !seen.insert(s.clone()) {
                continue;
            }
            let b = bucket(&s);
            if (lo..=hi).contains(&b) {
                out.push((cost, b, s));
            }
        }
        if !advance(&mut at, &lens) {
            out.sort();
            return Ok(out);
        }
    }
}

pub fn expand(pattern: &str, lo: u32, hi: u32) -> Result<Vec<(usize, u32, String)>> {
    let mut parts: Vec<Vec<&str>> = Vec::new();
    let mut rest = pattern;
    while let Some(open) = rest.find('[') {
        if open > 0 {
            parts.push(vec![rest.get(..open).unwrap_or_default()]);
        }
        let Some(close) = rest.get(open..).and_then(|r| r.find(']')) else {
            break;
        };
        parts.push(
            rest.get(open + 1..open + close)
                .unwrap_or_default()
                .split(',')
                .collect(),
        );
        rest = rest.get(open + close + 1..).unwrap_or_default();
    }
    if !rest.is_empty() {
        parts.push(vec![rest]);
    }
    let lens: Vec<usize> = parts.iter().map(Vec::len).collect();
    if product(&lens) > MAX_SPELLINGS {
        bail!("{pattern}: more than {MAX_SPELLINGS} spellings; share or drop some slots");
    }
    let width = widest(parts.iter().map(|p| p.iter().map(|s| s.len())));
    if product(&lens).saturating_mul(width) > MAX_SPELLING_BYTES {
        bail!(
            "{pattern}: its spellings would hold more than {MAX_SPELLING_BYTES} bytes; share or drop some slots"
        );
    }
    let mut out: Vec<(usize, u32, String)> = Vec::new();
    let mut at = vec![0usize; parts.len()];
    loop {
        let s: String = at
            .iter()
            .zip(&parts)
            .filter_map(|(&i, p)| p.get(i).copied())
            .collect();
        let b = bucket(&s);
        if (lo..=hi).contains(&b) {
            out.push((at.iter().filter(|&&i| i != 0).count(), b, s));
        }
        if !advance(&mut at, &lens) {
            out.sort();
            out.dedup_by(|a, b| a.2 == b.2);
            return Ok(out);
        }
    }
}

pub fn print(name: &str, window: &str, limit: usize, voc: &Vocabulary) -> Result<Vec<String>> {
    let (lo, hi) = window
        .split_once("..")
        .and_then(|(a, b)| {
            Some((
                a.parse::<u32>().ok()?,
                b.trim_start_matches('=').parse::<u32>().ok()?,
            ))
        })
        .with_context(|| format!("--window wants LO..HI (buckets), got '{window}'"))?;
    let mut out = Vec::new();
    let all = if name.contains('[') {
        out.push(format!("pattern {name}: window {lo}..={hi}"));
        expand(name, lo, hi)?
    } else {
        out.push(format!(
            "{name}: bucket {} now; window {lo}..={hi}",
            bucket(name)
        ));
        suggest(name, lo, hi, voc)?
    };
    for (cost, b, s) in all.iter().take(limit) {
        out.push(format!("  {b:>4}  cost {cost}  {s}"));
    }
    if all.len() > limit {
        out.push(format!("  ... {} more", all.len() - limit));
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn voc() -> Vocabulary {
        Vocabulary::parse(
            "group image images img pic\ngroup param parm arg\nqualifiers cur last\n",
            Path::new("t"),
        )
        .unwrap_or_default()
    }

    #[test]
    fn split_and_join_round_trip() {
        let (p, w) = split("g_popUpIconsMask");
        assert_eq!(p, "g_");
        assert_eq!(w, vec!["pop", "up", "icons", "mask"]);
        let ws: Vec<&str> = w.iter().map(String::as_str).collect();
        assert_eq!(join(&p, &ws), "g_popUpIconsMask");
    }

    #[test]
    fn every_candidate_is_in_its_window() -> Result<()> {
        for (_, b, s) in suggest("g_ruleParam", 27, 40, &voc())? {
            assert!((27..=40).contains(&b));
            assert_eq!(bucket_of(&s), b);
        }
        Ok(())
    }

    #[test]
    fn a_decorated_name_keeps_its_scope_and_type_code() -> Result<()> {
        let tail = "@ListCtl@@2UObj@@A";
        let name = format!("?s_hotImage{tail}");
        let all = suggest(&name, 0, 1023, &voc())?;
        assert!(!all.is_empty());
        for (_, b, s) in &all {
            assert!(s.starts_with("?s_") && s.ends_with(tail), "{s}");
            assert_eq!(bucket_of(s), *b);
        }
        assert!(all.iter().any(|(_, _, s)| s == &format!("?s_hotPic{tail}")));
        for (_, b, s) in suggest("?g_ruleParam@@3HA", 0, 1023, &voc())? {
            let ident = s.trim_start_matches('?').split('@').next().unwrap_or("");
            assert!(s.ends_with("@@3HA"), "{s}");
            assert_eq!(bucket_of(ident), b);
        }
        Ok(())
    }

    #[test]
    fn a_vocabulary_line_is_a_group_or_the_qualifiers_and_nothing_else() {
        let v = voc();
        assert_eq!(v.groups.len(), 2);
        assert_eq!(v.qualifiers, vec!["cur".to_string(), "last".to_string()]);
        assert!(Vocabulary::parse("words a b\n", Path::new("t")).is_err());
        assert!(Vocabulary::parse("group a\n", Path::new("t")).is_err());
        assert_eq!(
            Vocabulary::parse("", Path::new("t")).ok(),
            Some(Vocabulary::default())
        );
    }

    #[test]
    fn a_pattern_offers_each_slot_choice_and_costs_the_slots_it_changed() -> Result<()> {
        let all = expand("g_[a,b][X,]", 0, 1023)?;
        let names: Vec<&str> = all.iter().map(|x| x.2.as_str()).collect();
        assert_eq!(names.len(), 4);
        assert!(names.contains(&"g_aX") && names.contains(&"g_b"));
        assert!(all.iter().any(|x| x.2 == "g_aX" && x.0 == 0));
        Ok(())
    }
}
