use anyhow::{Result, bail};
use iced_x86::{Decoder, DecoderOptions, FlowControl, Instruction, Mnemonic, OpKind, Register};
use msvc6_base::units::Rva;
use msvc6_image::pe::Image;
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

pub const NEAR: usize = 4;

pub const GZ_FILL: [u8; 5] = [0xb8, 0xcc, 0xcc, 0xcc, 0xcc];

pub const WIDE_OPCODES: [u8; 2] = [0xdc, 0xdd];

pub const SHOWN_FUNCS: usize = 4;

pub const MAX_BODY: usize = 1 << 20;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Shape {
    LitSecond,
    NamedFirst,
    NamedDivisor,
    NamedStore,
    NamedMove,
    NamedNegate,
    Neutral,
}

impl Shape {
    #[must_use]
    pub fn says(self) -> Option<bool> {
        match self {
            Self::LitSecond => Some(true),
            Self::Neutral => None,
            _ => Some(false),
        }
    }

    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::LitSecond => "lit-second",
            Self::NamedFirst => "named-first",
            Self::NamedDivisor => "named-fdiv",
            Self::NamedStore => "named-store",
            Self::NamedMove => "named-mov",
            Self::NamedNegate => "named-fchs",
            Self::Neutral => "neutral",
        }
    }
}

#[must_use]
pub fn abs_operand(x: &Instruction) -> Option<u32> {
    (0..x.op_count()).find(|&k| x.op_kind(k) == OpKind::Memory)?;
    (x.memory_base() == Register::None && x.memory_index() == Register::None)
        .then(|| x.memory_displacement32())
}

fn has_memory(x: &Instruction) -> bool {
    (0..x.op_count()).any(|k| x.op_kind(k) == OpKind::Memory)
}

#[must_use]
pub fn is_x87(m: Mnemonic) -> bool {
    format!("{m:?}").starts_with('F')
}

fn x87_near(ins: &[Instruction], i: usize, step: isize) -> Option<&Instruction> {
    let mut j = i;
    for _ in 0..NEAR {
        j = j.checked_add_signed(step)?;
        let y = ins.get(j)?;
        if is_x87(y.mnemonic()) {
            return Some(y);
        }
        if y.flow_control() != FlowControl::Next {
            return None;
        }
    }
    None
}

#[must_use]
pub fn shape_at(ins: &[Instruction], i: usize, is_const: &dyn Fn(u32) -> bool) -> Shape {
    let Some(x) = ins.get(i) else {
        return Shape::Neutral;
    };
    let next = x87_near(ins, i, 1);
    let prev = x87_near(ins, i, -1);
    let var_mem = |y: &Instruction| has_memory(y) && abs_operand(y).is_none_or(|a| !is_const(a));
    match x.mnemonic() {
        Mnemonic::Mov | Mnemonic::Push => Shape::NamedMove,
        Mnemonic::Fdiv => Shape::NamedDivisor,
        Mnemonic::Fld => match next {
            Some(n) if matches!(n.mnemonic(), Mnemonic::Fstp | Mnemonic::Fst) && has_memory(n) => {
                Shape::NamedStore
            }
            Some(n) if matches!(n.mnemonic(), Mnemonic::Fmul | Mnemonic::Fadd) && var_mem(n) => {
                Shape::NamedFirst
            }
            Some(n) if n.mnemonic() == Mnemonic::Fchs => Shape::NamedNegate,
            _ => Shape::Neutral,
        },
        Mnemonic::Fmul | Mnemonic::Fadd => match prev {
            Some(p) if p.mnemonic() == Mnemonic::Fld && var_mem(p) => Shape::LitSecond,
            _ => Shape::Neutral,
        },
        _ => Shape::Neutral,
    }
}

#[must_use]
pub fn regime(s: Shape, od: bool) -> Shape {
    match s {
        Shape::LitSecond | Shape::NamedFirst | Shape::NamedDivisor if od => Shape::Neutral,
        _ => s,
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Use {
    pub func: String,
    pub func_rva: u32,
    pub at: u32,
    pub datum: u32,
    pub mnemonic: String,
    pub wide: bool,
    pub shape: Shape,
}

#[must_use]
pub fn decode(bytes: &[u8], ip: u64) -> Vec<Instruction> {
    let mut d = Decoder::with_ip(32, bytes, ip, DecoderOptions::NONE);
    let mut out = Vec::new();
    while d.can_decode() {
        out.push(d.decode());
    }
    out
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Verdict {
    Literal,
    Named,
    Conflict,
    Unknown,
}

#[must_use]
pub fn verdict(shapes: &[Shape]) -> Verdict {
    let lit = shapes.iter().any(|s| s.says() == Some(true));
    let named = shapes.iter().any(|s| s.says() == Some(false));
    match (lit, named) {
        (true, true) => Verdict::Conflict,
        (true, false) => Verdict::Literal,
        (false, true) => Verdict::Named,
        (false, false) => Verdict::Unknown,
    }
}

#[must_use]
pub fn plain_names(sym: &str) -> Vec<String> {
    if let Some(rest) = sym.strip_prefix('?') {
        let mut parts = rest.split("@@").next().unwrap_or("").split('@');
        let method = parts.next().unwrap_or("");
        let mut out = vec![method.to_string()];
        if let Some(class) = parts.next() {
            out.push(format!("{class}::{method}"));
        }
        return out;
    }
    let s = sym.trim_start_matches('_');
    vec![s.split('@').next().unwrap_or(s).to_string()]
}

pub struct Reference<'a> {
    pub label: &'a str,
    pub img: &'a Image,
    pub functions: &'a [(u32, String)],
    pub od: bool,
}

impl std::fmt::Debug for Reference<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Reference")
            .field("label", &self.label)
            .field("functions", &self.functions.len())
            .finish_non_exhaustive()
    }
}

pub fn reference_uses(r: &Reference<'_>) -> Result<Vec<Use>> {
    let base = r.img.opt.image_base.get();
    let Some(rdata) = r.img.section(".rdata").cloned() else {
        bail!("{}: no .rdata section", r.label);
    };
    let iat = r
        .img
        .dir(msvc6_image::pe::image::Dir::Iat)
        .unwrap_or_default();
    let is_const = |va: u32| -> bool {
        let Some(x) = va.checked_sub(base) else {
            return false;
        };
        rdata.contains(Rva(x)) && !(iat.rva.get() <= x && x < iat.rva.get() + iat.size)
    };
    let Some(text) = r.img.section(".text").cloned() else {
        bail!("{}: no .text section", r.label);
    };
    let text_end = text.rva.get().saturating_add(text.virt_size.get());
    let mut entries: Vec<(u32, String)> = r
        .functions
        .iter()
        .filter(|(rva, _)| *rva >= text.rva.get() && *rva < text_end)
        .cloned()
        .collect();
    entries.sort();
    entries.dedup_by_key(|e| e.0);
    let mut uses = Vec::new();
    for (k, (rva, name)) in entries.iter().enumerate() {
        let end = entries.get(k + 1).map_or(text_end, |e| e.0).min(text_end);
        let len = usize::try_from(end.saturating_sub(*rva))
            .unwrap_or(0)
            .min(MAX_BODY);
        let Some(bytes) = r.img.at_rva(Rva(*rva), len) else {
            continue;
        };
        let ins = decode(bytes, u64::from(base.saturating_add(*rva)));
        let od = r.od || bytes.windows(GZ_FILL.len()).any(|w| w == GZ_FILL);
        for (i, x) in ins.iter().enumerate() {
            let Some(a) = abs_operand(x) else { continue };
            if !is_const(a) {
                continue;
            }
            let m = x.mnemonic();
            if !is_x87(m) && !matches!(m, Mnemonic::Mov | Mnemonic::Push) {
                continue;
            }
            if m == Mnemonic::Mov && x.op0_kind() != OpKind::Register {
                continue;
            }
            let at = u32::try_from(x.ip()).unwrap_or(0).saturating_sub(base);
            uses.push(Use {
                func: name.clone(),
                func_rva: *rva,
                at,
                datum: a.saturating_sub(base),
                mnemonic: format!("{m:?}"),
                wide: r
                    .img
                    .at_rva(Rva(at), 1)
                    .is_some_and(|b| b.first().is_some_and(|o| WIDE_OPCODES.contains(o))),
                shape: regime(shape_at(&ins, i, &is_const), od),
            });
        }
    }
    let fpu: BTreeSet<u32> = uses
        .iter()
        .filter(|u| is_x87_name(&u.mnemonic))
        .map(|u| u.datum)
        .collect();
    uses.retain(|u| is_x87_name(&u.mnemonic) || fpu.contains(&u.datum));
    Ok(uses)
}

#[must_use]
pub fn is_x87_name(m: &str) -> bool {
    m.starts_with('F')
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OurUse {
    pub mnemonic: String,
    pub symbol: String,
}

pub type Ours = BTreeMap<String, (String, Vec<OurUse>)>;

#[must_use]
pub fn our_uses(objs: &[(String, Vec<msvc6_diff::comdat::Comdat>)]) -> Ours {
    let mut out = Ours::new();
    for (stem, comdats) in objs {
        for c in comdats {
            let Some(sym) = c.symbol.clone() else {
                continue;
            };
            let mut uses = Vec::new();
            let mut dec = Decoder::with_ip(32, &c.bytes, 0, DecoderOptions::NONE);
            while dec.can_decode() {
                let x = dec.decode();
                if !is_x87(x.mnemonic()) || abs_operand(&x).is_none() {
                    continue;
                }
                let off = dec.get_constant_offsets(&x);
                if !off.has_displacement() {
                    continue;
                }
                let at = u32::try_from(x.ip()).unwrap_or(0)
                    + u32::try_from(off.displacement_offset()).unwrap_or(0);
                if let Some(r) = c.relocations.iter().find(|r| r.at == at) {
                    uses.push(OurUse {
                        mnemonic: format!("{:?}", x.mnemonic()),
                        symbol: r.symbol.clone(),
                    });
                }
            }
            for n in plain_names(&sym) {
                out.entry(n).or_insert_with(|| (stem.clone(), uses.clone()));
            }
        }
    }
    out
}

#[must_use]
pub fn lookup<'a>(ours: &'a Ours, f: &str) -> Option<&'a (String, Vec<OurUse>)> {
    ours.get(f)
        .or_else(|| ours.get(f.rsplit("::").next().unwrap_or(f)))
}

#[must_use]
pub fn value_of(img: &Image, rva: u32, wide: bool) -> String {
    if wide {
        img.at_rva(Rva(rva), 8)
            .and_then(|b| <[u8; 8]>::try_from(b).ok())
            .map_or_else(
                || "?".to_string(),
                |b| format!("{:?}", f64::from_le_bytes(b)),
            )
    } else {
        img.at_rva(Rva(rva), 4)
            .and_then(|b| <[u8; 4]>::try_from(b).ok())
            .map_or_else(
                || "?".to_string(),
                |b| format!("{:?}f", f32::from_le_bytes(b)),
            )
    }
}

#[derive(Debug, Clone, Default)]
pub struct Report {
    pub lines: Vec<String>,
    pub counts: BTreeMap<String, usize>,
    pub copies: usize,
    pub joined: usize,
    pub disagree: usize,
}

#[must_use]
#[allow(clippy::too_many_lines)]
pub fn report(label: &str, img: &Image, uses: &[Use], ours: Option<&Ours>, all: bool) -> Report {
    let mut by_datum: BTreeMap<u32, Vec<&Use>> = BTreeMap::new();
    for u in uses {
        by_datum.entry(u.datum).or_default().push(u);
    }
    let mut r = Report::default();
    let owner_of = |f: &str| -> Option<String> { lookup(ours?, f).map(|o| o.0.clone()) };
    r.lines
        .push("datum      value           verdict   uses  shapes / functions".to_string());
    for (datum, us) in &by_datum {
        let shapes: Vec<Shape> = us.iter().map(|u| u.shape).collect();
        let v = verdict(&shapes);
        let tag = match v {
            Verdict::Literal => "LITERAL",
            Verdict::Named => "NAMED",
            Verdict::Conflict => "CONFLICT",
            Verdict::Unknown => "unknown",
        };
        *r.counts.entry(tag.to_string()).or_default() += 1;
        let funcs: BTreeSet<&str> = us.iter().map(|u| u.func.as_str()).collect();
        let owners: BTreeSet<String> = funcs.iter().filter_map(|f| owner_of(f)).collect();
        let wide = us.iter().any(|u| u.wide);
        let mut note = String::new();
        if v == Verdict::Named && owners.len() > 1 {
            note = format!(
                "  NAMED BUT READ BY {} OF OUR TUs ({}): one TU's extern const, or not named",
                owners.len(),
                owners.iter().cloned().collect::<Vec<_>>().join(",")
            );
            *r.counts.entry("named-cross-TU".to_string()).or_default() += 1;
        }
        if all || v == Verdict::Conflict || !note.is_empty() {
            let mut sh: BTreeMap<&str, usize> = BTreeMap::new();
            for s in &shapes {
                *sh.entry(s.tag()).or_default() += 1;
            }
            r.lines.push(format!(
                "{datum:#08x}  {:<14}  {tag:<8}  {:>4}  {} | {}{note}  [{}]",
                value_of(img, *datum, wide),
                us.len(),
                sh.iter()
                    .map(|(k, n)| format!("{k} {n}"))
                    .collect::<Vec<_>>()
                    .join(", "),
                funcs
                    .iter()
                    .take(SHOWN_FUNCS)
                    .copied()
                    .collect::<Vec<_>>()
                    .join(" "),
                us.first().map_or("", |u| u.mnemonic.as_str())
            ));
        }
    }
    let mut spelled: BTreeMap<u32, BTreeSet<(String, String)>> = BTreeMap::new();
    if let Some(ours) = ours {
        let mut per_fn: BTreeMap<(u32, &str), Vec<&Use>> = BTreeMap::new();
        for u in uses.iter().filter(|u| is_x87_name(&u.mnemonic)) {
            per_fn
                .entry((u.func_rva, u.func.as_str()))
                .or_default()
                .push(u);
        }
        r.lines.push(String::new());
        r.lines.push(
            "DISAGREEMENTS (reference shape against our spelling, per function, aligned by mnemonic):".to_string(),
        );
        for ((_, f), refs) in &per_fn {
            let Some((stem, mine)) = lookup(ours, f) else {
                continue;
            };
            let a: Vec<String> = refs.iter().map(|u| u.mnemonic.clone()).collect();
            let b: Vec<String> = mine.iter().map(|u| u.mnemonic.clone()).collect();
            for op in similar::capture_diff_slices(similar::Algorithm::Myers, &a, &b) {
                let similar::DiffOp::Equal {
                    old_index,
                    new_index,
                    len,
                } = op
                else {
                    continue;
                };
                for k in 0..len {
                    let (Some(rf), Some(o)) = (refs.get(old_index + k), mine.get(new_index + k))
                    else {
                        continue;
                    };
                    let ours_lit = o.symbol.starts_with("__real@");
                    r.joined += 1;
                    spelled
                        .entry(rf.datum)
                        .or_default()
                        .insert((stem.clone(), o.symbol.clone()));
                    let Some(says_lit) = rf.shape.says() else {
                        continue;
                    };
                    if says_lit != ours_lit {
                        r.disagree += 1;
                        r.lines.push(format!(
                            "  {f} ({stem}) {:#x} {} [{:#x}] = {}: image {} -> {}, ours `{}`",
                            rf.at,
                            rf.mnemonic,
                            rf.datum,
                            value_of(img, rf.datum, rf.wide),
                            rf.shape.tag(),
                            if says_lit {
                                "a LITERAL"
                            } else {
                                "a NAMED const"
                            },
                            o.symbol
                        ));
                    }
                }
            }
        }
    }
    let mut copies: BTreeMap<(bool, Vec<u8>), Vec<u32>> = BTreeMap::new();
    for (datum, us) in &by_datum {
        let wide = us.iter().any(|u| u.wide);
        if let Some(b) = img.at_rva(Rva(*datum), if wide { 8 } else { 4 }) {
            copies.entry((wide, b.to_vec())).or_default().push(*datum);
        }
    }
    r.lines.push(String::new());
    r.lines.push(
        "COPIES (one value at several datums: at most one is a literal, the others named consts):"
            .to_string(),
    );
    for ((wide, _), ds) in copies.iter().filter(|(_, ds)| ds.len() > 1) {
        r.copies += 1;
        let first = ds.first().copied().unwrap_or(0);
        let at = ds
            .iter()
            .map(|d| {
                let fs: BTreeSet<&str> = by_datum
                    .get(d)
                    .map(|us| us.iter().map(|u| u.func.as_str()).collect())
                    .unwrap_or_default();
                let mine: Vec<String> = spelled
                    .get(d)
                    .map(|s| s.iter().map(|(t, y)| format!("{t}:{y}")).collect())
                    .unwrap_or_default();
                format!(
                    "{d:#x} [{}]{}",
                    fs.into_iter().take(3).collect::<Vec<_>>().join(" "),
                    if mine.is_empty() {
                        String::new()
                    } else {
                        format!(" ours {}", mine.join(","))
                    }
                )
            })
            .collect::<Vec<_>>()
            .join("; ");
        r.lines
            .push(format!("  {} at {at}", value_of(img, first, *wide)));
    }
    r.lines.push(format!(
        "TOTAL {label}: {} .rdata float datum(s) read by code ({} use(s)), {} value(s) held more than once: {}{}",
        by_datum.len(),
        uses.len(),
        r.copies,
        r.counts
            .iter()
            .map(|(k, n)| format!("{k} {n}"))
            .collect::<Vec<_>>()
            .join(", "),
        if ours.is_some() {
            format!("; {} use(s) joined to our objects, {} disagree", r.joined, r.disagree)
        } else {
            String::new()
        }
    ));
    r
}

#[cfg(test)]
mod tests {
    use super::*;

    fn shapes_in(bytes: &[u8], is_const: &dyn Fn(u32) -> bool) -> Vec<Shape> {
        let ins = decode(bytes, 0);
        ins.iter()
            .enumerate()
            .filter(|(_, x)| abs_operand(x).is_some_and(is_const))
            .map(|(i, _)| shape_at(&ins, i, is_const))
            .collect()
    }

    fn c(a: u32) -> bool {
        a == 0x1000
    }

    #[test]
    fn the_measured_shapes_classify() {
        let lit_mul = [
            0xd9, 0x44, 0x24, 0x04, 0xd8, 0x0d, 0x00, 0x10, 0x00, 0x00, 0xc3,
        ];
        assert_eq!(shapes_in(&lit_mul, &c), vec![Shape::LitSecond]);
        let named_mul = [
            0xd9, 0x05, 0x00, 0x10, 0x00, 0x00, 0xd8, 0x4c, 0x24, 0x04, 0xc3,
        ];
        assert_eq!(shapes_in(&named_mul, &c), vec![Shape::NamedFirst]);
        let named_div = [
            0xd9, 0x44, 0x24, 0x04, 0xd8, 0x35, 0x00, 0x10, 0x00, 0x00, 0xc3,
        ];
        assert_eq!(shapes_in(&named_div, &c), vec![Shape::NamedDivisor]);
        let named_store = [
            0xd9, 0x05, 0x00, 0x10, 0x00, 0x00, 0x8b, 0x44, 0x24, 0x04, 0xd9, 0x58, 0x08, 0xc3,
        ];
        assert_eq!(shapes_in(&named_store, &c), vec![Shape::NamedStore]);
        let named_push = [0xa1, 0x00, 0x10, 0x00, 0x00, 0x50, 0xc3];
        assert_eq!(shapes_in(&named_push, &c), vec![Shape::NamedMove]);
        let sub = [
            0xd9, 0x05, 0x00, 0x10, 0x00, 0x00, 0xd8, 0x64, 0x24, 0x04, 0xc3,
        ];
        assert_eq!(shapes_in(&sub, &c), vec![Shape::Neutral]);
        assert_eq!(
            verdict(&[Shape::LitSecond, Shape::Neutral]),
            Verdict::Literal
        );
        assert_eq!(
            verdict(&[Shape::LitSecond, Shape::NamedDivisor]),
            Verdict::Conflict
        );
        assert_eq!(verdict(&[]), Verdict::Unknown);
        assert_eq!(regime(Shape::LitSecond, true), Shape::Neutral);
        assert_eq!(regime(Shape::NamedMove, true), Shape::NamedMove);
    }

    #[test]
    fn a_decorated_method_answers_to_its_plain_names() {
        assert_eq!(
            plain_names("?Draw@Widget@@QAEXXZ"),
            vec!["Draw".to_string(), "Widget::Draw".to_string()]
        );
        assert_eq!(plain_names("?Run@@YAXXZ"), vec!["Run".to_string()]);
        assert_eq!(plain_names("_fill_buffer"), vec!["fill_buffer".to_string()]);
        let ours: Ours = [("Draw".to_string(), ("A".to_string(), vec![]))].into();
        assert!(lookup(&ours, "Widget::Draw").is_some());
        assert!(lookup(&ours, "Paint").is_none());
    }
}
