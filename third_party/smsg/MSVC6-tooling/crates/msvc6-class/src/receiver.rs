use crate::callconv::{self, This};
use crate::decl::{self, VPTR};
use crate::layout;
use crate::sigs::bases_of;
use iced_x86::{Decoder, DecoderOptions, Instruction, Mnemonic, OpKind, Register};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const MAX_AGGREGATE_SPAN: u32 = 0x80;
pub const LISTED: usize = 8;
pub const MAX_BASE_DEPTH: usize = 16;

pub type Flat = BTreeMap<u32, (u32, String)>;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Declared {
    pub data: Vec<String>,
    pub bases: Vec<String>,
    pub virtual_bases: Vec<String>,
}

#[derive(Debug, Clone, Default)]
pub struct Population {
    pub fields: BTreeMap<String, Flat>,
    pub bases: BTreeMap<String, Vec<String>>,
    pub annotated: BTreeSet<String>,
    pub declared: BTreeMap<String, Declared>,
    pub notes: Vec<String>,
}

impl Population {
    #[must_use]
    pub fn classes(&self) -> BTreeSet<&String> {
        self.fields.keys().chain(self.declared.keys()).collect()
    }
}

fn comment_offset(line: &str) -> Option<(u32, Option<u32>)> {
    let c = line.split("/*").nth(1)?;
    if ["EBP", "ebp", "ESP", "esp"].iter().any(|r| c.contains(r)) {
        return None;
    }
    let at = c.find("0x")?;
    let hex: String = c
        .get(at + 2..)?
        .chars()
        .take_while(char::is_ascii_hexdigit)
        .collect();
    let first = u32::from_str_radix(&hex, 16).ok()?;
    let after = c.get(at + 2 + hex.len()..).unwrap_or_default();
    let end = after.find("0x").and_then(|k| {
        after.get(..k).filter(|g| g.contains(".."))?;
        let h: String = after
            .get(k + 2..)?
            .chars()
            .take_while(char::is_ascii_hexdigit)
            .collect();
        u32::from_str_radix(&h, 16).ok()
    });
    Some((first, end.filter(|e| *e > first)))
}

fn data_member(line: &str) -> Option<&str> {
    let code = line.split("/*").next()?.trim();
    let code = code.strip_suffix(';')?;
    if code.contains('(') || code.contains(')') {
        return None;
    }
    if [
        "public:",
        "private:",
        "protected:",
        "union",
        "struct",
        "class",
    ]
    .iter()
    .any(|k| code.starts_with(k))
    {
        return None;
    }
    let base = code.split('[').next()?.trim();
    let last = base
        .split(|c: char| c.is_whitespace() || c == '*' || c == '&')
        .next_back()?;
    (!last.is_empty() && last.chars().all(|c| c.is_alphanumeric() || c == '_')).then_some(last)
}

fn annotations(text: &str, pop: &mut Population) {
    let mut cur: Option<String> = None;
    let mut depth = 0usize;
    for line in text.lines() {
        let code = line.split("/*").next().unwrap_or(line);
        if cur.is_none()
            && let Some((n, _)) = bases_of(line)
        {
            cur = Some(n.to_string());
            depth = 0;
        }
        if cur.is_some() {
            let (opens, closes) = (code.matches('{').count(), code.matches('}').count());
            let was = depth;
            depth = depth.saturating_add(opens).saturating_sub(closes);
            if depth == 0 && (was > 0 || opens > 0) {
                cur = None;
                continue;
            }
        }
        let Some(class) = cur.as_deref() else {
            continue;
        };
        let (Some(member), Some((off, end))) = (data_member(line), comment_offset(line)) else {
            continue;
        };
        pop.annotated.insert(class.to_string());
        pop.fields
            .entry(class.to_string())
            .or_default()
            .entry(off)
            .or_insert((end.unwrap_or(off), member.to_string()));
    }
}

fn derived(texts: &[(String, String)], pop: &mut Population) {
    let found = crate::join::population_noted(texts, true);
    pop.notes.extend(found.notes);
    for d in found.decls {
        if pop.annotated.contains(&d.name) || pop.declared.contains_key(&d.name) {
            continue;
        }
        pop.declared.insert(
            d.name.clone(),
            Declared {
                bases: d
                    .bases
                    .iter()
                    .filter(|b| !d.virtual_bases.contains(b))
                    .cloned()
                    .collect(),
                virtual_bases: d.virtual_bases.clone(),
                data: d.data,
            },
        );
    }
}
fn sized(entry: &str) -> Option<Vec<(u32, u32, String)>> {
    let mut parts = entry.split(',');
    let first = parts.next()?.trim();
    let upto = first.find('[').unwrap_or(first.len());
    let head = first.get(..upto)?;
    let cut = head
        .rfind(|c: char| c.is_whitespace() || c == '*' || c == '&')
        .map(|i| i + 1)?;
    let (ty, d0) = first.split_at(cut);
    let ptr = ty.trim_end().ends_with('*') || ty.trim_end().ends_with('&');
    let ty = decl::normalise_ws(ty.trim_end_matches(['*', '&', ' ']));
    let mut out = Vec::new();
    for d in std::iter::once(d0.to_string()).chain(parts.map(str::to_string)) {
        let d = d.trim();
        let star = d.starts_with('*') || d.starts_with('&');
        let d = d.trim_start_matches(['*', '&']).trim();
        let (name, count) = match d.split_once('[') {
            None => (d.to_string(), 1),
            Some((n, rest)) => (
                n.trim().to_string(),
                layout::extent(rest.trim_end_matches(']'))?,
            ),
        };
        if name.is_empty() || !name.chars().all(|c| c.is_alphanumeric() || c == '_') {
            return None;
        }
        let elem = if ptr || star {
            4
        } else {
            layout::scalar_size(&ty)?
        };
        out.push((elem, count, name));
    }
    Some(out)
}

#[must_use]
pub fn population(texts: &[(String, String)]) -> Population {
    let mut pop = Population::default();
    for (_, t) in texts {
        annotations(t, &mut pop);
        for line in t.lines() {
            if let Some((n, bs)) = bases_of(line)
                && !bs.is_empty()
            {
                pop.bases.entry(n.to_string()).or_insert(bs);
            }
        }
    }
    derived(texts, &mut pop);
    pop
}

fn own_end(fields: &Flat) -> u32 {
    fields
        .values()
        .map(|(e, _)| e.saturating_add(1))
        .chain(fields.keys().map(|o| o.saturating_add(4)))
        .max()
        .unwrap_or(0)
}

#[must_use]
fn place(out: &mut Flat, at: u32, sub: Flat) -> u32 {
    let end = own_end(&sub).div_ceil(4) * 4;
    for (o, (e, m)) in sub {
        out.insert(at.saturating_add(o), (at.saturating_add(e), m));
    }
    at.saturating_add(end)
}

fn flatten_declared(
    name: &str,
    d: &Declared,
    pop: &Population,
    seen: &mut BTreeSet<String>,
) -> Flat {
    let mut out = Flat::new();
    let mut at = 0u32;
    for b in &d.bases {
        let sub = flatten(b, pop, seen);
        at = place(&mut out, at, sub);
    }
    let mut data = d.data.iter().peekable();
    if data.peek().is_some_and(|x| *x == VPTR) {
        data.next();
        if !out.values().any(|(_, m)| m.ends_with(VPTR)) {
            out.insert(at, (at.saturating_add(3), format!("{name}::{VPTR}")));
            at = at.saturating_add(4);
        }
    }
    if !d.virtual_bases.is_empty() && d.bases.is_empty() {
        out.insert(at, (at.saturating_add(3), format!("{name}::<vbptr>")));
        at = at.saturating_add(4);
    }
    for entry in data {
        let Some(sized) = sized(entry) else { break };
        for (elem, count, field) in sized {
            at = layout::align_to(at, elem);
            let size = elem.saturating_mul(count);
            out.insert(
                at,
                (
                    at.saturating_add(size.saturating_sub(1)),
                    format!("{name}::{field}"),
                ),
            );
            at = at.saturating_add(size);
        }
    }
    let mut end = at.div_ceil(4) * 4;
    for b in &d.virtual_bases {
        let sub = flatten(b, pop, seen);
        end = place(&mut out, end, sub);
    }
    out
}

#[must_use]
pub fn flatten(name: &str, pop: &Population, seen: &mut BTreeSet<String>) -> Flat {
    let mut out = Flat::new();
    if seen.len() >= MAX_BASE_DEPTH || !seen.insert(name.to_string()) {
        return out;
    }
    if !pop.annotated.contains(name)
        && let Some(d) = pop.declared.get(name)
    {
        let flat = flatten_declared(name, d, pop, seen);
        seen.remove(name);
        return flat;
    }
    let mut at = 0u32;
    for b in pop.bases.get(name).map(Vec::as_slice).unwrap_or_default() {
        let sub = flatten(b, pop, seen);
        at = place(&mut out, at, sub);
    }
    for (o, (e, m)) in pop.fields.get(name).into_iter().flatten() {
        out.insert(*o, (*e, format!("{name}::{m}")));
    }
    seen.remove(name);
    out
}

fn track(holds: &mut BTreeSet<Register>, i: &Instruction) {
    if i.mnemonic() == Mnemonic::Mov
        && i.op0_kind() == OpKind::Register
        && i.op1_kind() == OpKind::Register
    {
        if holds.contains(&i.op1_register()) {
            holds.insert(i.op0_register());
        } else {
            holds.remove(&i.op0_register());
        }
        return;
    }
    if i.mnemonic() == Mnemonic::Call {
        for r in [Register::EAX, Register::ECX, Register::EDX] {
            holds.remove(&r);
        }
        return;
    }
    if matches!(i.mnemonic(), Mnemonic::Mov | Mnemonic::Lea) && i.op0_kind() == OpKind::Register {
        holds.remove(&i.op0_register());
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Seed {
    Ecx,
    Stack,
}

fn stack_load(i: &Instruction, depth: i64) -> bool {
    i.mnemonic() == Mnemonic::Mov
        && i.op0_kind() == OpKind::Register
        && i.op1_kind() == OpKind::Memory
        && i.memory_base() == Register::ESP
        && i.memory_index() == Register::None
        && i64::from(i.memory_displacement32()) == depth + 4
}

fn push_depth(i: &Instruction) -> i64 {
    match i.mnemonic() {
        Mnemonic::Push => 4,
        Mnemonic::Pop => -4,
        Mnemonic::Sub | Mnemonic::Add
            if i.op0_kind() == OpKind::Register && i.op0_register() == Register::ESP =>
        {
            let v = i64::from(i.immediate32().cast_signed());
            if i.mnemonic() == Mnemonic::Sub { v } else { -v }
        }
        _ => 0,
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Vbase {
    pub bias: u32,
    pub entries: BTreeSet<u32>,
    pub reads: Vec<(u32, i32)>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Touched {
    pub offsets: BTreeSet<i32>,
    pub vbase: Option<Vbase>,
}

#[must_use]
pub fn touched(body: &[u8], ip: u64, seed: Seed) -> Touched {
    let mut d = Decoder::with_ip(32, body, ip, DecoderOptions::NONE);
    let mut holds: BTreeSet<Register> = BTreeSet::new();
    if seed == Seed::Ecx {
        holds.insert(Register::ECX);
    }
    let mut out = Touched::default();
    let mut vbptr: BTreeMap<Register, u32> = BTreeMap::new();
    let mut entry: BTreeMap<Register, u32> = BTreeMap::new();
    let mut depth = 0i64;
    while d.can_decode() {
        let i = d.decode();
        if i.is_invalid() {
            break;
        }
        let (base, index) = (i.memory_base(), i.memory_index());
        let disp = i.memory_displacement32().cast_signed();
        let memory = (0..i.op_count()).any(|k| i.op_kind(k) == OpKind::Memory);
        if memory {
            if holds.contains(&base) && index == Register::None {
                out.offsets.insert(disp);
            }
            let through =
                |a: Register, b: Register| entry.get(&a).copied().filter(|_| holds.contains(&b));
            if let Some(k) = through(base, index).or_else(|| through(index, base))
                && let Some(v) = out.vbase.as_mut()
            {
                v.reads.push((k, disp));
            }
        }
        let dest = (i.op0_kind() == OpKind::Register).then(|| i.op0_register());
        let mut learned: Option<(Register, bool, u32)> = None;
        if i.mnemonic() == Mnemonic::Mov
            && let Some(r) = dest
            && i.op1_kind() == OpKind::Memory
            && index == Register::None
        {
            if holds.contains(&base) && disp < 0 {
                let bias = disp.unsigned_abs();
                out.vbase.get_or_insert_with(|| Vbase {
                    bias,
                    ..Vbase::default()
                });
                learned = Some((r, true, bias));
            } else if vbptr.contains_key(&base) && disp > 0 && disp % 4 == 0 {
                let k = disp.unsigned_abs() / 4;
                if let Some(v) = out.vbase.as_mut() {
                    v.entries.insert(k);
                }
                learned = Some((r, false, k));
            }
        }
        if seed == Seed::Stack && stack_load(&i, depth) {
            if let Some(r) = dest {
                track(&mut holds, &i);
                holds.insert(r);
            }
        } else {
            track(&mut holds, &i);
        }
        if let Some(r) = dest {
            vbptr.remove(&r);
            entry.remove(&r);
        }
        match learned {
            Some((r, true, bias)) => {
                vbptr.insert(r, bias);
            }
            Some((r, false, k)) => {
                entry.insert(r, k);
            }
            None => {}
        }
        depth += push_depth(&i);
    }
    out
}

#[derive(Debug, Clone, Serialize)]
pub struct Candidate {
    pub class: String,
    pub distinct: usize,
    pub unnamed: usize,
    pub inside: usize,
    pub lines: Vec<String>,
    pub fields: usize,
    pub span: u32,
}

#[derive(Debug, Clone, Serialize)]
pub struct Report {
    pub seed: Seed,
    pub this: This,
    pub offsets: Vec<u32>,
    pub vbase: Option<Vbase>,
    pub classes: usize,
    pub annotated: usize,
    pub based: usize,
    pub candidates: Vec<Candidate>,
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub notes: Vec<String>,
}

#[must_use]
pub fn rank(body: &[u8], rva: u32, seed: Option<Seed>, pop: &Population) -> Report {
    let this = callconv::this_of(body);
    let seed = seed.unwrap_or(if this == This::FirstArgument {
        Seed::Stack
    } else {
        Seed::Ecx
    });
    let t = touched(body, u64::from(rva), seed);
    let bias = t.vbase.as_ref().map_or(0, |v| v.bias);
    let offsets: Vec<u32> = t
        .offsets
        .iter()
        .filter_map(|d| u32::try_from(i64::from(*d) + i64::from(bias)).ok())
        .collect::<BTreeSet<u32>>()
        .into_iter()
        .collect();
    let mut candidates = Vec::new();
    for name in pop.classes() {
        let flat = flatten(name, pop, &mut BTreeSet::new());
        let mut lines = Vec::new();
        let mut unnamed = 0usize;
        let mut inside = 0usize;
        let mut hit: BTreeSet<u32> = BTreeSet::new();
        for &h in &offsets {
            if let Some((_, m)) = flat.get(&h) {
                hit.insert(h);
                lines.push(format!("+0x{h:x}  {m}"));
                continue;
            }
            match flat.iter().find(|(o, (e, _))| {
                **o < h && h <= *e && e.saturating_sub(**o) <= MAX_AGGREGATE_SPAN
            }) {
                Some((o, (_, m))) => {
                    hit.insert(*o);
                    inside += 1;
                    lines.push(format!("+0x{h:x}  {m} +0x{:x}", h - o));
                }
                None => {
                    unnamed += 1;
                    lines.push(format!("+0x{h:x}  -- no field"));
                }
            }
        }
        candidates.push(Candidate {
            class: name.clone(),
            distinct: hit.len(),
            unnamed,
            inside,
            lines,
            fields: flat.len(),
            span: own_end(&flat),
        });
    }
    candidates.sort_by(|a, b| {
        b.distinct
            .cmp(&a.distinct)
            .then(a.unnamed.cmp(&b.unnamed))
            .then(a.class.cmp(&b.class))
    });
    Report {
        seed,
        this,
        offsets,
        vbase: t.vbase,
        classes: pop.classes().len(),
        annotated: pop.annotated.len(),
        based: pop.bases.len(),
        candidates,
        notes: pop.notes.clone(),
    }
}

fn density(c: &Candidate) -> String {
    let slots = (c.span / 4).max(1);
    let n = u32::try_from(c.fields).unwrap_or(u32::MAX);
    let hundredths = n.saturating_mul(100) / slots;
    format!("{}.{:02}", hundredths / 100, hundredths % 100)
}

#[must_use]
pub fn report(r: &Report, label: &str, rva: u32, len: u32) -> Vec<String> {
    let mut out = vec![format!(
        "{label} 0x{rva:x}, {len} byte(s): WHICH DECLARED CLASS OWNS THE OFFSETS IT TOUCHES."
    )];
    out.push(format!(
        "  `this' is tracked from {}: {}; `mov' copies carry it, a call clobbers the volatiles, ESP and EBP bases are never fields.",
        match r.seed {
            Seed::Ecx => "ECX (__thiscall)",
            Seed::Stack => "the first stack argument (__stdcall)",
        },
        match r.this {
            This::Ecx => "the body reads ECX before it writes it",
            This::FirstArgument => "the body never reads ECX and dereferences its first stack argument",
            This::Unknown => "the body commits to neither",
        }
    ));
    if let Some(v) = &r.vbase {
        out.push(format!(
            "  VIRTUAL-BASE ENTRY: `this' arrives 0x{:x} past the object, so the vbptr is at object +0x00 and the class's own data spans 0x{:x} byte(s); offsets below are the object's.",
            v.bias,
            v.bias.saturating_sub(4)
        ));
        for k in &v.entries {
            out.push(format!(
                "    reads vbtable[{k}], the offset of virtual base {k}"
            ));
        }
        for (k, d) in &v.reads {
            out.push(format!(
                "    a member of virtual base {k} at its +0x{:x} (the bias folded into [vbtable[{k}] + this + {d:#x}])",
                i64::from(*d) + i64::from(v.bias)
            ));
        }
    }
    if r.offsets.is_empty() {
        out.push(String::new());
        out.push("  NO `this'-RELATIVE DEREFERENCE AT ALL: the body is static or free, or only forwards its arguments.".to_string());
        return out;
    }
    out.push(String::new());
    out.push(format!(
        "  OFFSETS TOUCHED ({}): {}",
        r.offsets.len(),
        r.offsets
            .iter()
            .map(|h| format!("+0x{h:x}"))
            .collect::<Vec<_>>()
            .join(" ")
    ));
    out.push(format!(
        "  POPULATION: {} class(es) with a known field ({} by `/* +0xNN */' comment, the rest derived from declaration order and type size), {} with a base list.",
        r.classes, r.annotated, r.based
    ));
    out.push(String::new());
    out.push("  CANDIDATES, MOST DISTINCT FIELDS FIRST (an offset inside an aggregate of at most 0x80 bytes counts for it):".to_string());
    out.push(
        "        unnamed  of  distinct  class                     fields  span   per dword"
            .to_string(),
    );
    for c in r.candidates.iter().take(LISTED) {
        out.push(format!(
            "      {:>9}  {:>2}  {:>8}  {:<24}  {:>6}  0x{:<4x}  {}",
            c.unnamed,
            r.offsets.len(),
            c.distinct,
            c.class,
            c.fields,
            c.span,
            density(c)
        ));
    }
    let best = r.candidates.first().map(|c| (c.distinct, c.unnamed));
    let tied: Vec<&Candidate> = r
        .candidates
        .iter()
        .filter(|c| Some((c.distinct, c.unnamed)) == best)
        .collect();
    if tied.len() > 1 {
        out.push(String::new());
        out.push(format!(
            "  NO VERDICT: {} class(es) tie at the top; prefer the sparsest and settle it from a `sizeof' at an allocation, a vftable slot or the call site's receiver.",
            tied.len()
        ));
        for c in tied {
            out.push(format!(
                "      tied: {:<24} {} per dword over 0x{:x}",
                c.class,
                density(c),
                c.span
            ));
        }
        return out;
    }
    if let Some(c) = r.candidates.first() {
        out.push(String::new());
        out.push(format!("  BEST CANDIDATE `{}', alone at the top:", c.class));
        for l in &c.lines {
            out.push(format!("      {l}"));
        }
        out.push(String::new());
        out.push(if c.unnamed == 0 && c.inside > 0 {
            format!(
                "  {} OFFSET(S) FALL INSIDE AN AGGREGATE MEMBER, NOT ON ONE OF ITS FIELDS: the class owns them only if that member's layout does, which this screen cannot see; the runners-up say whether another class names them.",
                c.inside
            )
        } else if c.unnamed == 0 {
            "  EVERY OFFSET NAMED, AND NO TIE.".to_string()
        } else {
            format!(
                "  {} OFFSET(S) UNNAMED: a wrong receiver, or a naming gap in the right class; the runners-up say which.",
                c.unnamed
            )
        });
    }
    out.extend(r.notes.iter().cloned());
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn holds_after(bytes: &[u8]) -> BTreeSet<i32> {
        touched(bytes, 0x1000, Seed::Ecx).offsets
    }

    #[test]
    fn this_follows_mov_copies_and_survives_reads_and_epilogue_pops() {
        assert_eq!(
            holds_after(&[0x8b, 0xf1, 0x56, 0x3b, 0xc6, 0x8b, 0x46, 0x08]),
            [8].into()
        );
        assert_eq!(
            holds_after(&[0x8b, 0xf1, 0x5f, 0x5e, 0x5d, 0x8b, 0x46, 0x0c]),
            [0xc].into()
        );
        assert!(holds_after(&[0x8b, 0xf1, 0x8b, 0xc8, 0x8b, 0x41, 0x04]).is_empty());
        assert_eq!(
            holds_after(&[
                0x8b, 0xf1, 0xe8, 0, 0, 0, 0, 0x8b, 0x41, 0x04, 0x8b, 0x46, 0x10
            ]),
            [0x10].into()
        );
        assert!(holds_after(&[0x8b, 0x49, 0x08, 0x8b, 0x41, 0x04]).len() == 1);
    }

    #[test]
    fn a_stdcall_body_is_read_from_its_first_stack_argument() {
        let body = [
            0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0x46, 0x60, 0x8b, 0x4e, 0x10, 0x5e, 0xc2, 0x04,
            0x00,
        ];
        assert_eq!(touched(&body, 0, Seed::Stack).offsets, [0x10, 0x60].into());
        assert!(touched(&body, 0, Seed::Ecx).offsets.is_empty());
    }

    #[test]
    fn a_biased_this_reads_the_vbptr_and_its_bases_through_the_vbtable() {
        let body = [
            0x8b, 0xf1, 0x8b, 0x46, 0xdc, 0x8b, 0x48, 0x04, 0x8b, 0x94, 0x31, 0x0c, 0x01, 0x00,
            0x00, 0x8b, 0x48, 0x08, 0x8b, 0x46, 0xf0, 0xc3,
        ];
        let t = touched(&body, 0x9e450, Seed::Ecx);
        let v = t.vbase.clone().unwrap();
        assert_eq!(v.bias, 0x24);
        assert_eq!(v.entries, [1, 2].into());
        assert_eq!(v.reads, vec![(1, 0x10c)]);
        assert_eq!(t.offsets, [-0x24, -0x10].into());
        let pop = population(&[(
            "a.h".to_string(),
            "class List : public virtual Widget {\n    int m_a;\n    int m_b;\n    int m_c;\n    int m_d;\n    int m_e;\n    int m_items;\n};\n".to_string(),
        )]);
        let r = rank(&body, 0x9e450, None, &pop);
        assert_eq!(r.offsets, vec![0, 0x14]);
        let lines = report(&r, "IMG", 0x9e450, 22);
        assert!(
            lines
                .iter()
                .any(|l| l.contains("arrives 0x24 past the object")),
            "{lines:#?}"
        );
        assert!(
            lines
                .iter()
                .any(|l| l.contains("a member of virtual base 1 at its +0x130")),
            "{lines:#?}"
        );
    }

    #[test]
    fn annotations_win_and_bases_are_folded_in_at_their_aligned_end() {
        let pop = population(&[(
            "a.h".to_string(),
            "struct Obj {\n    int m_vt;  /* +0x00 */\n    char m_flag; /* +0x70 */\n};\nstruct Voice {\n    int m_handle; /* +0x00 */\n    int m_rate;   /* +0x04 */\n};\nstruct Wave : public Obj, public Voice {\n    int m_own; /* +0x7c */\n};\nstruct Plain {\n    int a;\n    short b;\n    int c;\n};\n".to_string(),
        )]);
        let flat = flatten("Wave", &pop, &mut BTreeSet::new());
        assert_eq!(
            flat.get(&0x74).map(|(_, m)| m.as_str()),
            Some("Voice::m_handle")
        );
        assert_eq!(
            flat.get(&0x7c).map(|(_, m)| m.as_str()),
            Some("Wave::m_own")
        );
        let plain = flatten("Plain", &pop, &mut BTreeSet::new());
        assert_eq!(plain.keys().copied().collect::<Vec<_>>(), vec![0, 4, 8]);
        assert!(pop.annotated.contains("Obj") && !pop.annotated.contains("Plain"));
    }

    #[test]
    fn an_offset_inside_an_aggregate_member_is_not_called_named() {
        let pop = population(&[(
            "a.h".to_string(),
            "struct Map {\n    int m_count;\n    int m_slots[16];\n    int m_tail;\n};\nstruct Pair {\n    int m_a;\n    int m_b;\n};\n"
                .to_string(),
        )]);
        let inner = [0x8b, 0x41, 0x0c, 0x8b, 0x41, 0x00, 0xc3];
        let r = rank(&inner, 0x1000, None, &pop);
        let best = r.candidates.first().unwrap();
        assert_eq!(
            (best.class.as_str(), best.unnamed, best.inside),
            ("Map", 0, 1)
        );
        let lines = report(&r, "IMG", 0x1000, 7);
        assert!(
            lines
                .iter()
                .any(|l| l.starts_with("  1 OFFSET(S) FALL INSIDE AN AGGREGATE MEMBER")),
            "{lines:#?}"
        );
        assert!(
            !lines.iter().any(|l| l.contains("EVERY OFFSET NAMED")),
            "{lines:#?}"
        );
        let exact = [0x8b, 0x41, 0x44, 0x8b, 0x41, 0x00, 0xc3];
        let lines = report(&rank(&exact, 0x1000, None, &pop), "IMG", 0x1000, 7);
        assert!(
            lines.iter().any(|l| l.contains("EVERY OFFSET NAMED")),
            "{lines:#?}"
        );
    }
}
