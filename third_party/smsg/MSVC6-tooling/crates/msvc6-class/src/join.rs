use crate::decl::{self, ClassDecl, DTOR};
use crate::slotmap;
use crate::slots::{self, Rule};
use crate::vft::{Run, Text};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;

#[must_use]
pub fn class_method(sym: &str) -> Option<(String, String)> {
    let rest = sym.strip_prefix('?')?;
    for pre in ["?_G", "?_E"] {
        if let Some(r) = rest.strip_prefix(pre) {
            let class = r.split('@').next()?;
            return (!class.is_empty()).then(|| (class.to_string(), DTOR.to_string()));
        }
    }
    if rest.starts_with('?') {
        return None;
    }
    let mut it = rest.split('@');
    let method = it.next()?;
    let class = it.next()?;
    if method.is_empty() || class.is_empty() {
        return None;
    }
    Some((class.to_string(), method.to_string()))
}

#[must_use]
pub fn table_name(sym: &str) -> Option<(String, Option<String>)> {
    let rest = sym.strip_prefix("??_7")?;
    let (class, tail) = rest.split_once("@@6B")?;
    if class.is_empty() || class.contains(['?', '@', '$']) {
        return None;
    }
    if tail == "@" {
        return Some((class.to_string(), None));
    }
    let sub = tail.strip_suffix("@@@")?;
    (!sub.is_empty() && !sub.contains(['?', '@', '$']))
        .then(|| (class.to_string(), Some(sub.to_string())))
}

fn pinned_table(
    class: &str,
    chain: &[String],
    tables: &BTreeMap<String, Vec<(Option<String>, u32)>>,
) -> Option<u32> {
    let t = tables.get(class)?;
    if let Some((_, r)) = t.iter().find(|(s, _)| s.is_none()) {
        return Some(*r);
    }
    let on: Vec<u32> = t
        .iter()
        .filter(|(s, _)| s.as_ref().is_some_and(|s| chain.contains(s)))
        .map(|(_, r)| *r)
        .collect();
    match on.as_slice() {
        [r] => Some(*r),
        _ => None,
    }
}

#[must_use]
pub fn map_names(text: &str, image_base: u32) -> Vec<(String, u32)> {
    let mut out: BTreeMap<String, u32> = BTreeMap::new();
    for line in text.lines() {
        let c: Vec<&str> = line.split_whitespace().collect();
        if c.len() < 4 || !c.first().is_some_and(|f| f.starts_with("0001:")) {
            continue;
        }
        let (Some(sym), Some(va)) = (c.get(1), c.get(2)) else {
            continue;
        };
        if let Ok(va) = u32::from_str_radix(va, 16)
            && let Some(rva) = va.checked_sub(image_base)
        {
            out.insert((*sym).to_string(), rva);
        }
    }
    out.into_iter().collect()
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Slot {
    Agrees,
    Unpinned,
    Pure,
    Disagrees(String),
}

#[derive(Debug, Clone, Serialize)]
pub struct Assigned {
    pub class: String,
    pub file: String,
    pub table: u32,
    pub names: Vec<String>,
    pub slots: Vec<Slot>,
    pub sole: Vec<bool>,
    pub own_hits: usize,
    pub hits: usize,
}

pub type Tie = (u32, usize, usize);

#[derive(Debug, Default, Serialize)]
pub struct Join {
    pub assigned: Vec<Assigned>,
    pub unpinned: Vec<String>,
    pub ambiguous: Vec<(String, Vec<Tie>)>,
    pub unresolved: Vec<String>,
    pub shared: Vec<(u32, Vec<String>)>,
    pub population: usize,
    pub names_known: usize,
    pub comment_pins: usize,
    pub table_pinned: usize,
    pub corrected: Vec<(String, u32, u32)>,
    pub pin_outside: Vec<(String, u32)>,
    pub arities: Vec<Arity>,
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Copy)]
pub struct Inputs<'a> {
    pub runs: &'a [Run],
    pub names: &'a [(String, u32)],
    pub map: &'a [(String, u32)],
    pub placed: &'a [(String, u32)],
    pub files: &'a [(String, String)],
    pub headers: &'a [(String, String)],
    pub rule: Rule,
    pub text: Option<Text<'a>>,
}

#[must_use]
pub fn population(files: &[(String, String)], expand: bool) -> Vec<ClassDecl> {
    population_noted(files, expand).decls
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Population {
    pub decls: Vec<ClassDecl>,
    pub notes: Vec<String>,
}

#[must_use]
pub fn population_noted(files: &[(String, String)], expand: bool) -> Population {
    let stripped: Vec<(String, String)> = files
        .iter()
        .map(|(n, s)| (n.clone(), decl::uncomment(s.as_bytes())))
        .collect();
    let macros = decl::virtual_macros(&stripped);
    let mut out = Population::default();
    for (name, src) in &stripped {
        let text = if expand {
            let (t, spent) = decl::expand_within(src, &macros);
            if spent {
                out.notes.push(format!(
                    "NOTE: {name}: the virtual-member macro expansion stopped at its budget ({} \
                     expansions, {} bytes rewritten): later uses are read unexpanded",
                    decl::MAX_EXPANSIONS,
                    decl::MAX_EXPAND_WORK
                ));
            }
            t
        } else {
            src.clone()
        };
        let s = decl::class_scan(&text, name);
        if s.unread > 0 {
            out.notes.push(format!(
                "NOTE: {name}: {} class definition(s) not read: a head over {} bytes, more than {} bases, \
                 or past the scan's budget of {} bytes a source byte",
                s.unread,
                decl::MAX_HEAD,
                decl::MAX_BASES,
                decl::SCAN_FACTOR
            ));
        }
        out.decls.extend(s.decls);
    }
    out
}

#[must_use]
pub fn takes_part(d: &ClassDecl, model: &slots::Model<'_>, rule: Rule) -> bool {
    if rule.legacy() || d.polymorphic(false) {
        return d.polymorphic(rule.legacy());
    }
    let mut base: Vec<slots::Slot> = Vec::new();
    for b in &d.bases {
        for c in model.named(b) {
            base.extend(model.slots(c, rule).unwrap_or_default());
        }
    }
    d.methods.iter().any(|m| {
        base.iter()
            .any(|s| s.name == m.name && (m.name == DTOR || s.params == m.params))
    })
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Arity {
    pub class: String,
    pub table: u32,
    pub slot: usize,
    pub name: String,
    pub params: String,
    pub stdcall: bool,
    pub declared: usize,
    pub popped: usize,
}

fn popped(text: Text<'_>, body: u32) -> Option<usize> {
    let off = usize::try_from(body.checked_sub(text.rva)?).ok()?;
    let window = text.bytes.get(
        off..text
            .bytes
            .len()
            .min(off.saturating_add(slotmap::READ_WINDOW)),
    )?;
    let rets = slotmap::decode(0, body, window).rets;
    let first = *rets.first()?;
    rets.iter().all(|r| *r == first).then_some(first)
}

fn arities(
    text: Text<'_>,
    class: &str,
    at: u32,
    table: &[slots::Slot],
    bodies: &[Option<u32>],
) -> Vec<Arity> {
    let mut out = Vec::new();
    for (i, (s, b)) in table.iter().zip(bodies).enumerate() {
        let (Some(b), Some(n)) = (b, slots::param_count(&s.params)) else {
            continue;
        };
        if s.pure || bodies.iter().filter(|x| **x == Some(*b)).count() > 1 {
            continue;
        }
        let declared = n + usize::from(s.stdcall);
        if let Some(p) = popped(text, *b)
            && p != declared
        {
            out.push(Arity {
                class: class.to_string(),
                table: at,
                slot: i,
                name: s.name.clone(),
                params: s.params.clone(),
                stdcall: s.stdcall,
                declared,
                popped: p,
            });
        }
    }
    out
}

type ByRva = BTreeMap<u32, BTreeSet<(String, String)>>;
type ByName = BTreeMap<(String, String), BTreeSet<u32>>;

#[must_use]
pub fn join(i: &Inputs<'_>) -> Join {
    let mut by_rva: ByRva = BTreeMap::new();
    let mut by_name: ByName = BTreeMap::new();
    let mut vft: BTreeMap<String, Option<u32>> = BTreeMap::new();
    let add = |sym: &str, rva: u32, by_rva: &mut ByRva, by_name: &mut ByName| {
        if let Some(cm) = class_method(sym) {
            by_rva.entry(rva).or_default().insert(cm.clone());
            by_name.entry(cm).or_default().insert(rva);
        }
    };
    let place = |sym: &str, rva: u32, vft: &mut BTreeMap<String, Option<u32>>| {
        if table_name(sym).is_some() {
            let e = vft.entry(sym.to_string()).or_insert(Some(rva));
            if *e != Some(rva) {
                *e = None;
            }
        }
    };
    for (s, r) in i.names {
        add(s, *r, &mut by_rva, &mut by_name);
        place(s, *r, &mut vft);
    }
    for (s, r) in i.map {
        add(s, *r, &mut by_rva, &mut by_name);
    }
    for (s, r) in i.placed {
        place(s, *r, &mut vft);
    }
    let mut tables: BTreeMap<String, Vec<(Option<String>, u32)>> = BTreeMap::new();
    for (s, r) in &vft {
        if let (Some((c, sub)), Some(r)) = (table_name(s), r) {
            tables.entry(c).or_default().push((sub, *r));
        }
    }
    let own = population_noted(i.files, true);
    let mut pop = own.decls;
    let mut notes = own.notes;
    if !i.rule.legacy() {
        let h = population_noted(i.headers, true);
        pop.extend(h.decls);
        notes.extend(h.notes);
    }
    let owned: BTreeSet<&str> = i.files.iter().map(|(n, _)| n.as_str()).collect();
    let model = slots::Model::new(&pop);
    let mut by_file: BTreeMap<&str, Vec<&ClassDecl>> = BTreeMap::new();
    for c in &pop {
        by_file.entry(c.file.as_str()).or_default().push(c);
    }
    let legacy = i.rule.legacy();
    let mut comment_pins = 0usize;
    for (file, src) in i.files {
        for (m, rva) in decl::declaration_rvas(src) {
            let owners: Vec<&str> = by_file
                .get(file.as_str())
                .map_or(&[][..], Vec::as_slice)
                .iter()
                .filter(|c| &c.file == file && c.virtuals(legacy).contains(&m))
                .map(|c| c.name.as_str())
                .collect();
            if let [class] = owners.as_slice() {
                let cm = ((*class).to_string(), m.clone());
                if by_name.get(&cm).is_none_or(BTreeSet::is_empty) {
                    comment_pins += 1;
                    by_rva.entry(rva).or_default().insert(cm.clone());
                    by_name.entry(cm).or_default().insert(rva);
                }
            }
        }
    }
    let mut out = Join {
        names_known: by_rva.len(),
        comment_pins,
        ..Join::default()
    };
    let mut done: BTreeSet<(String, Vec<String>)> = BTreeSet::new();
    for d in pop
        .iter()
        .filter(|c| owned.contains(c.file.as_str()) && takes_part(c, &model, i.rule))
    {
        let names = if d.macro_used.is_some() {
            None
        } else {
            model.slots(d, i.rule)
        };
        let Some(table) = names else {
            out.unresolved.push(format!("{} ({})", d.name, d.file));
            continue;
        };
        let names: Vec<String> = table.iter().map(|s| s.name.clone()).collect();
        if !done.insert((d.name.clone(), names.clone())) {
            continue;
        }
        out.population += 1;
        let lineage = model.lineage(&d.name);
        let named = |rva: u32, m: &str| -> (bool, bool) {
            by_rva.get(&rva).map_or((false, false), |s| {
                let own = s.contains(&(d.name.clone(), m.to_string()));
                let any = own
                    || lineage
                        .iter()
                        .any(|c| s.contains(&(c.clone(), m.to_string())));
                (own, any)
            })
        };
        let score = |slots: &[u32], k: usize| -> (usize, usize) {
            let (mut own, mut hits) = (0usize, 0usize);
            for (j, m) in names.iter().enumerate() {
                let Some(b) = slots.get(k + j) else { break };
                let (o, a) = named(*b, m);
                own += usize::from(o);
                hits += usize::from(a);
            }
            (own, hits)
        };
        let mut scored: Vec<(u32, usize, usize, usize, usize)> = Vec::new();
        for (ti, r) in i.runs.iter().enumerate() {
            for k in 0..r.slots.len() {
                let (own, hits) = score(&r.slots, k);
                if hits > 0 {
                    let at = r.rva.wrapping_add(u32::try_from(4 * k).unwrap_or(u32::MAX));
                    scored.push((at, own, hits, ti, k));
                }
            }
        }
        let best = scored
            .iter()
            .map(|s| (s.1, s.2))
            .max()
            .filter(|(o, _)| *o > 0);
        let top: Vec<_> = best.map_or_else(Vec::new, |b| {
            scored.iter().filter(|s| (s.1, s.2) == b).collect()
        });
        let pin = pinned_table(&d.name, &model.primary_chain(&d.name), &tables);
        let at_pin = pin.and_then(|p| {
            i.runs.iter().enumerate().find_map(|(ti, r)| {
                let off = usize::try_from(p.checked_sub(r.rva)?).ok()?;
                (off % 4 == 0 && off / 4 < r.slots.len()).then_some((ti, off / 4))
            })
        });
        let (at, own, hits, ti, k) = match (pin, at_pin) {
            (Some(p), Some((ti, k))) => {
                if let [s] = top.as_slice()
                    && s.0 != p
                {
                    out.corrected.push((d.name.clone(), s.0, p));
                }
                out.table_pinned += 1;
                let (own, hits) = i.runs.get(ti).map_or((0, 0), |r| score(&r.slots, k));
                (p, own, hits, ti, k)
            }
            _ => {
                if let Some(p) = pin {
                    out.pin_outside.push((d.name.clone(), p));
                }
                match top.as_slice() {
                    [] => {
                        out.unpinned.push(d.name.clone());
                        continue;
                    }
                    [s] => **s,
                    _ => {
                        out.ambiguous.push((
                            d.name.clone(),
                            top.iter().map(|s| (s.0, s.1, s.2)).collect(),
                        ));
                        continue;
                    }
                }
            }
        };
        let run = i.runs.get(ti).map_or(&[][..], |r| r.slots.as_slice());
        let bodies: Vec<Option<u32>> = (0..names.len()).map(|j| run.get(k + j).copied()).collect();
        let count = |b: u32| bodies.iter().filter(|x| **x == Some(b)).count();
        let sole: Vec<bool> = bodies
            .iter()
            .map(|b| b.is_some_and(|b| count(b) == 1))
            .collect();
        let verdicts = table
            .iter()
            .zip(&bodies)
            .map(|(s, b)| {
                let shared = b.is_some_and(|b| count(b) > 1);
                let owner = [s.owner.clone()];
                let who = Who {
                    lineage: &lineage,
                    pins: if legacy || s.owner.is_empty() {
                        &lineage
                    } else {
                        &owner
                    },
                    label: if legacy || s.owner.is_empty() {
                        &d.name
                    } else {
                        &s.owner
                    },
                };
                let v = verdict(&s.name, *b, shared, &who, &by_rva, &by_name);
                match v {
                    Slot::Disagrees(_) if s.pure && shared => Slot::Pure,
                    other => other,
                }
            })
            .collect();
        if let Some(t) = i.text.filter(|_| !legacy) {
            out.arities.extend(arities(t, &d.name, at, &table, &bodies));
        }
        out.assigned.push(Assigned {
            class: d.name.clone(),
            file: d.file.clone(),
            table: at,
            names,
            slots: verdicts,
            sole,
            own_hits: own,
            hits,
        });
    }
    let mut claims: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    for a in &out.assigned {
        claims.entry(a.table).or_default().push(a.class.clone());
    }
    out.shared = claims.into_iter().filter(|(_, v)| v.len() > 1).collect();
    out.notes = notes;
    out.notes.extend(model.note());
    out
}

fn describe(rva: u32, by_rva: &ByRva) -> String {
    by_rva.get(&rva).map_or_else(
        || format!("0x{rva:x} (unnamed)"),
        |s| {
            let n: Vec<String> = s.iter().map(|(c, m)| format!("{c}::{m}")).collect();
            format!("0x{rva:x} {}", n.join(" / "))
        },
    )
}

fn fmt_set(s: &BTreeSet<u32>) -> String {
    s.iter()
        .map(|r| format!("0x{r:x}"))
        .collect::<Vec<_>>()
        .join(", ")
}

#[derive(Debug, Clone, Copy)]
struct Who<'a> {
    lineage: &'a [String],
    pins: &'a [String],
    label: &'a str,
}

fn verdict(
    m: &str,
    body: Option<u32>,
    shared: bool,
    who: &Who<'_>,
    by_rva: &ByRva,
    by_name: &ByName,
) -> Slot {
    let (class, lineage) = (who.label, who.lineage);
    let pinned: BTreeSet<u32> = who
        .pins
        .iter()
        .filter_map(|c| by_name.get(&(c.clone(), m.to_string())))
        .flatten()
        .copied()
        .collect();
    let Some(b) = body else {
        return Slot::Disagrees(format!(
            "declared past the end of the reference run{}",
            if pinned.is_empty() {
                String::new()
            } else {
                format!("; `{m}' is pinned at {}", fmt_set(&pinned))
            }
        ));
    };
    if pinned.contains(&b) {
        return Slot::Agrees;
    }
    let other: Vec<String> = by_rva
        .get(&b)
        .into_iter()
        .flatten()
        .filter(|(c, n)| !shared && n != m && lineage.contains(c))
        .map(|(c, n)| format!("{c}::{n}"))
        .collect();
    if pinned.is_empty() && other.is_empty() {
        return Slot::Unpinned;
    }
    let mut why = format!("reference body {}", describe(b, by_rva));
    if !other.is_empty() && !why.contains(&other.join(" / ")) {
        let _ = write!(why, " [{}]", other.join(" / "));
    }
    if !pinned.is_empty() {
        let _ = write!(
            why,
            "; declared `{class}::{m}' is pinned at {}",
            fmt_set(&pinned)
        );
    }
    Slot::Disagrees(why)
}

#[must_use]
pub fn tsv(j: &Join, image_base: u32) -> String {
    let shared: BTreeSet<u32> = j.shared.iter().map(|(t, _)| *t).collect();
    let mut s = String::from(
        "# table va, class, then each slot: !name agrees, name unpinned, - disagrees\n",
    );
    for a in j.assigned.iter().filter(|a| !shared.contains(&a.table)) {
        let _ = write!(s, "0x{:x}\t{}", a.table.wrapping_add(image_base), a.class);
        for ((n, v), sole) in a.names.iter().zip(&a.slots).zip(&a.sole) {
            let n = if n == DTOR {
                format!("~{}", a.class)
            } else {
                n.clone()
            };
            match v {
                Slot::Agrees if *sole => {
                    let _ = write!(s, "\t!{n}");
                }
                Slot::Agrees | Slot::Unpinned | Slot::Pure => {
                    let _ = write!(s, "\t{n}");
                }
                Slot::Disagrees(_) => s.push_str("\t-"),
            }
        }
        s.push('\n');
    }
    s
}

#[derive(Debug, Clone, Copy)]
pub struct Header<'a> {
    pub image: &'a str,
    pub population: &'a str,
    pub names: &'a str,
}

#[must_use]
pub fn report(j: &Join, h: Header<'_>) -> Vec<String> {
    let mut out = vec![
        format!(
            "{}: DECLARED CLASSES JOINED TO REFERENCE VFTABLES BY SLOT NAME",
            h.image
        ),
        String::new(),
        format!(
            "  POPULATION: {} distinct polymorphic class declaration(s) under {}; {} reference rva(s) carry a class-method name",
            j.population, h.population, j.names_known
        ),
        format!(
            "  ({}; {} of them from a declaration's own comment).",
            h.names, j.comment_pins
        ),
        "  A class is ASSIGNED a table only when a slot names the class ITSELF and no other offset ties. NO VERDICT is drawn on"
            .to_string(),
        "  any disagreement: each is a source question for the reader.".to_string(),
    ];
    let (mut agree, mut unp, mut dis, mut pure) = (0usize, 0usize, 0usize, 0usize);
    for a in &j.assigned {
        for v in &a.slots {
            match v {
                Slot::Agrees => agree += 1,
                Slot::Unpinned => unp += 1,
                Slot::Pure => pure += 1,
                Slot::Disagrees(_) => dis += 1,
            }
        }
    }
    out.push(String::new());
    out.push(format!(
        "  ASSIGNED {} class(es): {agree} slot(s) agree, {unp} unpinned, {dis} DISAGREE.{}",
        j.assigned.len(),
        if pure > 0 {
            format!(" {pure} pure slot(s) hold a body several slots share, the `= 0' stub.")
        } else {
            String::new()
        }
    ));
    out.push(format!(
        "  {} of them PLACED by a `??_7' name rather than scored; {} placed name(s) lie in no detected run.",
        j.table_pinned,
        j.pin_outside.len()
    ));
    for (c, s, p) in &j.corrected {
        out.push(format!(
            "    CORRECTED {c}: the slot-name score chose 0x{s:x}, its `??_7' sits at 0x{p:x}"
        ));
    }
    for (c, p) in &j.pin_outside {
        out.push(format!("    OUTSIDE   {c}: `??_7' at 0x{p:x}, no run"));
    }
    for a in &j.assigned {
        out.push(format!(
            "    0x{:06x}  {:<28} {:>3} slots  {} own / {} total hit(s)  {}",
            a.table,
            a.class,
            a.names.len(),
            a.own_hits,
            a.hits,
            a.file
        ));
    }
    out.push(String::new());
    out.push("  DISAGREEMENTS, by class:".to_string());
    let mut any = false;
    for a in &j.assigned {
        let rows: Vec<(usize, &String, &String)> = a
            .names
            .iter()
            .zip(&a.slots)
            .enumerate()
            .filter_map(|(i, (n, v))| match v {
                Slot::Disagrees(w) => Some((i, n, w)),
                _ => None,
            })
            .collect();
        if rows.is_empty() {
            continue;
        }
        any = true;
        out.push(format!(
            "    {} (table 0x{:x}, {}):",
            a.class, a.table, a.file
        ));
        for (i, n, w) in rows {
            out.push(format!(
                "      slot {i:>2} +0x{:02x}  declared `{n}': {w}",
                i * 4
            ));
        }
    }
    if !any {
        out.push("    none".to_string());
    }
    if !j.arities.is_empty() {
        out.push(String::new());
        out.push(format!(
            "  ARITY -- {} slot(s) whose body's `ret' pops another argument count than the declaration:",
            j.arities.len()
        ));
        for a in &j.arities {
            out.push(format!(
                "    {} (table 0x{:x}) slot {:>2} +0x{:02x}  `{}{}'{} takes {}, the body pops {}{}",
                a.class,
                a.table,
                a.slot,
                a.slot * 4,
                a.name,
                a.params,
                if a.stdcall { " __stdcall" } else { "" },
                a.declared,
                a.popped,
                if !a.stdcall && a.popped == a.declared + 1 {
                    ": one more, the shape of a __stdcall virtual (its `this' on the stack)"
                } else {
                    ""
                }
            ));
        }
    }
    if !j.shared.is_empty() {
        out.push(String::new());
        out.push(
            "  ONE TABLE, SEVERAL CLASSES -- none of these is labelled (a shared address is not a class identity):"
                .to_string(),
        );
        for (t, cs) in &j.shared {
            out.push(format!("    0x{t:x}: {}", cs.join(", ")));
        }
    }
    if !j.ambiguous.is_empty() {
        out.push(String::new());
        out.push(
            "  AMBIGUOUS -- two or more offsets tie for best, so none is assigned:".to_string(),
        );
        for (c, t) in &j.ambiguous {
            let s: Vec<String> = t
                .iter()
                .map(|(a, o, h)| format!("0x{a:x} ({o}/{h})"))
                .collect();
            out.push(format!("    {c}: {}", s.join(", ")));
        }
    }
    out.push(String::new());
    out.push(format!(
        "  UNPINNED -- no slot of these names a body the tree has attached to the class itself ({}):",
        j.unpinned.len()
    ));
    out.push(format!("    {}", j.unpinned.join(", ")));
    if !j.unresolved.is_empty() {
        out.push(String::new());
        out.push("  SLOT LIST NOT STATABLE (base unresolved or its copies disagree):".to_string());
        out.push(format!("    {}", j.unresolved.join(", ")));
    }
    out.extend(j.notes.iter().cloned());
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_table_name_reads_its_class_and_subobject() {
        assert_eq!(table_name("??_7Arena@@6B@"), Some(("Arena".into(), None)));
        assert_eq!(
            table_name("??_7Bar@@6BBase@@@"),
            Some(("Bar".into(), Some("Base".into())))
        );
        assert_eq!(table_name("??_7?$List@UCell@@@@6B0@@"), None);
        assert_eq!(table_name("??_7CellList@@6B?$List@UCell@@@@@"), None);
        assert_eq!(table_name("?Draw@Arena@@UAEXXZ"), None);
    }

    #[test]
    fn a_pin_takes_the_subobject_on_the_primary_chain() {
        let mut tables: BTreeMap<String, Vec<(Option<String>, u32)>> = BTreeMap::new();
        tables.insert(
            "Bar".into(),
            vec![
                (Some("Surface".into()), 0xbbab4),
                (Some("Widget".into()), 0xbbac8),
            ],
        );
        let chain: Vec<String> = ["Bar", "Render", "Widget"]
            .iter()
            .map(|s| (*s).to_string())
            .collect();
        assert_eq!(pinned_table("Bar", &chain, &tables), Some(0xbbac8));
        assert_eq!(pinned_table("Bar", &["Bar".to_string()], &tables), None);
        tables.insert("Arena".into(), vec![(None, 0xba84c)]);
        assert_eq!(pinned_table("Arena", &[], &tables), Some(0xba84c));
    }

    #[test]
    fn a_class_method_reads_scope_and_deleting_destructors() {
        assert_eq!(
            class_method("?Hide@Widget@@UAEXXZ"),
            Some(("Widget".into(), "Hide".into()))
        );
        assert_eq!(
            class_method("?Layout@Widget@fn_00080f80@@QAEXHH@Z"),
            Some(("Widget".into(), "Layout".into()))
        );
        assert_eq!(
            class_method("??_GHook@@UAEPAXI@Z"),
            Some(("Hook".into(), "~".into()))
        );
        assert_eq!(class_method("??1Hook@@UAE@XZ"), None);
        assert_eq!(class_method("??0Hook@@QAE@XZ"), None);
        assert_eq!(class_method("?Stub@@YAXH@Z"), None);
        assert_eq!(class_method("_g_rect"), None);
    }

    #[test]
    fn a_map_names_section_one_symbols_at_their_rva() {
        let text = " 0001:00000010       ?f@C@@UAEXXZ               10001010 f   c.obj\n 0002:00000000       ??_7C@@6B@                 10002000     c.obj\n";
        assert_eq!(
            map_names(text, 0x1000_0000),
            vec![("?f@C@@UAEXXZ".to_string(), 0x1010)]
        );
    }

    #[test]
    fn a_verdict_flags_a_pinned_name_at_another_body() {
        let lineage = vec!["C".to_string(), "B".to_string()];
        let mut by_rva: ByRva = BTreeMap::new();
        let mut by_name: ByName = BTreeMap::new();
        for (r, c, n) in [(0x10, "C", "A"), (0x20, "C", "B"), (0x40, "B", "D")] {
            by_rva.entry(r).or_default().insert((c.into(), n.into()));
            by_name.entry((c.into(), n.into())).or_default().insert(r);
        }
        let who = Who {
            lineage: &lineage,
            pins: &lineage,
            label: "C",
        };
        let v = |m, b, shared| verdict(m, Some(b), shared, &who, &by_rva, &by_name);
        assert_eq!(v("A", 0x10, false), Slot::Agrees);
        assert!(matches!(v("A", 0x20, false), Slot::Disagrees(_)));
        assert_eq!(v("Z", 0x30, false), Slot::Unpinned);
        assert!(matches!(v("Z", 0x20, false), Slot::Disagrees(_)));
        assert_eq!(v("Z", 0x20, true), Slot::Unpinned);
        assert!(matches!(v("D", 0x50, false), Slot::Disagrees(_)));
        let own = ["C".to_string()];
        let overrider = Who { pins: &own, ..who };
        assert_eq!(
            verdict("D", Some(0x50), false, &overrider, &by_rva, &by_name),
            Slot::Unpinned
        );
    }
}
