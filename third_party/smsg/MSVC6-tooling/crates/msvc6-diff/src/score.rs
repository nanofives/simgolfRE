use crate::comdat::{Comdat, FIELD};
use crate::env::Env;
use crate::insndiff::{self, Report};
use crate::segments::{self, Tail};
use anyhow::{Context as _, Result, anyhow, bail};
use iced_x86::{Decoder, DecoderOptions, FlowControl, Mnemonic, OpKind};
use std::collections::BTreeMap;

pub const FICTIONAL_FLOOR: usize = 16;
pub const LINK_FILL: [u8; 2] = [0xcc, 0x90];
pub const RESERVE_ALIGN: u32 = 16;
pub const NOP: u8 = 0x90;

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub enum Seat {
    Given,
    Image,
    Unseated,
    Misasserted,
    Superseded,
    Duplicate,
    Dropped,
    Padded { pad: u32, len: u32 },
    NopTail { rec: u32 },
    JumpTable { rec: u32, entries: u32 },
    Unrecorded,
}

impl Seat {
    #[must_use]
    pub fn excluded(self) -> bool {
        matches!(self, Self::Superseded | Self::Duplicate | Self::Dropped)
    }

    #[must_use]
    pub fn void(self) -> bool {
        matches!(self, Self::Unseated | Self::Misasserted)
    }

    #[must_use]
    pub fn tag(self) -> &'static str {
        match self {
            Self::Given => "given",
            Self::Image => "seated",
            Self::Unseated => "unseated",
            Self::Misasserted => "misasserted",
            Self::Superseded => "superseded",
            Self::Duplicate => "duplicate",
            Self::Dropped => "dropped",
            Self::Padded { .. } => "padded",
            Self::NopTail { .. } => "nop-tail",
            Self::JumpTable { .. } => "jump-table",
            Self::Unrecorded => "unrecorded",
        }
    }

    #[must_use]
    pub fn label(self) -> String {
        match self {
            Self::Given => String::new(),
            Self::Image => " (SEATED)".into(),
            Self::Padded { pad, len } => format!(" (PADDED, SEATED -- text-pad {pad:#x}+{len})"),
            Self::NopTail { rec } => format!(
                " (SEATED -- recorded {rec:#x}, then CL's own 0x90 tail, same bytes in the image)"
            ),
            Self::JumpTable { rec, entries } => format!(
                " (SEATED -- recorded {rec:#x}, then {entries} switch table entries the body names, which the recorded extent leaves out)"
            ),
            Self::Unrecorded => " (SEATED -- no recorded extent; the image's instructions end at this length, then link fill)".into(),
            Self::Unseated => " (UNSEATED -- SCORE IS NOT EVIDENCE)".into(),
            Self::Misasserted => {
                " (MISASSERTED -- no recorded function of this length at the address given)".into()
            }
            Self::Superseded => " (SUPERSEDED, NOT SCORED -- a weak-default `??_G' or a non-EH vector iterator; the image's `??_E' holds this slot)".into(),
            Self::Duplicate => " (DUPLICATE, NOT SCORED -- a SELECT_ANY copy the link discards for an earlier object's)".into(),
            Self::Dropped => " (DROPPED, NOT SCORED -- the image's dropped list names it: the image holds no body for it)".into(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Scored {
    pub at: u32,
    pub seat: Seat,
    pub symbol: Option<String>,
    pub name: String,
    pub bad: Vec<usize>,
    pub comparable: usize,
    pub masked: usize,
    pub relocs: usize,
    pub want: Vec<u8>,
    pub comdat: Comdat,
    pub kept_by: Option<String>,
}

impl Scored {
    #[must_use]
    pub fn label(&self) -> String {
        match (&self.kept_by, self.seat) {
            (Some(k), Seat::Duplicate) => format!(
                " (DUPLICATE, NOT SCORED -- a SELECT_ANY copy the link discards: {k}, earlier in the link, holds the kept copy here)"
            ),
            _ => self.seat.label(),
        }
    }

    #[must_use]
    pub fn report(&self) -> Report {
        insndiff::compare(
            &self.want,
            &self.comdat.bytes,
            &self.comdat.relocated,
            u64::from(self.at),
        )
    }

    #[must_use]
    pub fn line(&self) -> String {
        format!(
            "  {:#08x}{} +{:#x}  {} of {} comparable bytes differ ({} masked by {} relocation(s))  {}",
            self.at,
            self.label(),
            self.comdat.bytes.len(),
            self.bad.len(),
            self.comparable,
            self.masked,
            self.relocs,
            self.name
        )
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Anchor {
    pub root: u32,
    pub root_name: String,
    pub own: Option<(String, u32)>,
    pub moved: bool,
}

#[derive(Debug, Clone, Default)]
pub struct Score {
    pub rva: u32,
    pub members: Vec<Scored>,
    pub compared: usize,
    pub differing: usize,
    pub pad_skipped: usize,
    pub anchor: Option<Anchor>,
}

impl Score {
    #[must_use]
    pub fn identical(&self) -> bool {
        self.differing == 0
    }

    #[must_use]
    pub fn right(&self) -> usize {
        self.compared.saturating_sub(self.differing)
    }

    #[must_use]
    pub fn void(&self) -> usize {
        self.members.iter().filter(|m| m.seat.void()).count()
    }

    #[must_use]
    pub fn verdict(&self) -> &'static str {
        if self.identical() {
            "IDENTICAL"
        } else {
            "DIFFERS"
        }
    }

    #[must_use]
    pub fn verdict_line(&self) -> String {
        format!(
            "  VERDICT: {}  -- {} of {} comparable bytes differ",
            self.verdict(),
            self.differing,
            self.compared
        )
    }

    #[must_use]
    pub fn breakdown(&self) -> Option<Vec<String>> {
        if self.members.len() < 2 {
            return None;
        }
        let first = self.members.first()?;
        let later: Vec<&Scored> = self
            .members
            .iter()
            .skip(1)
            .filter(|m| !m.seat.excluded())
            .collect();
        let bad = later.iter().map(|m| m.bad.len()).sum::<usize>();
        let cmp = later.iter().map(|m| m.comparable).sum::<usize>();
        let vacuous = later
            .iter()
            .filter(|m| m.comparable < FICTIONAL_FLOOR)
            .count();
        let fictional = later
            .iter()
            .filter(|m| fictional_placement(m.bad.len(), m.comparable).is_some())
            .count();
        let unseated = self
            .members
            .iter()
            .filter(|m| m.seat == Seat::Unseated)
            .count();
        let misasserted = self
            .members
            .iter()
            .filter(|m| m.seat == Seat::Misasserted)
            .count();
        let padded = self
            .members
            .iter()
            .filter(|m| matches!(m.seat, Seat::Padded { .. }))
            .count();
        let how = match (unseated, padded) {
            (0, 0) => "at their own image addresses, seated from the recorded function starts and length-checked".to_string(),
            (0, p) => format!(
                "at their own image addresses, seated from the recorded function starts and length-checked ({p} padded: a recorded length plus one declared text-pad range)"
            ),
            _ => "at addresses not all of which the image confirms".to_string(),
        };
        let mut out = vec![
            format!(
                "  of which: {} of {} in the COMDAT at {:#x} (the address asked for);",
                first.bad.len(),
                first.comparable,
                self.rva
            ),
            format!(
                "            {bad} of {cmp} in {} later COMDAT(s) {how}, {fictional} of them >85% differing",
                self.members.len().saturating_sub(1)
            ),
        ];
        if unseated > 0 {
            out.push(format!(
                "            and {unseated} UNSEATED -- no recorded function of the predicted address and length sits at or after the previous member's end"
            ));
        }
        if misasserted > 0 {
            out.push(format!(
                "            and {misasserted} MISASSERTED -- an address was given or resolved for these and no recorded function of that COMDAT's length starts there"
            ));
        }
        if unseated + misasserted > 0 {
            out.push(
                "            those rows compare this object's bytes against whatever lies at their addresses: exclude them before quoting any total".to_string(),
            );
        }
        if vacuous > 0 {
            out.push(format!(
                "            and {vacuous} too small to classify (<{FICTIONAL_FLOOR} comparable bytes)"
            ));
        }
        Some(out)
    }

    #[must_use]
    pub fn lines(&self, detail: bool) -> Vec<String> {
        let mut out = Vec::new();
        for m in &self.members {
            out.push(m.line());
            if detail && !m.bad.is_empty() {
                out.extend(m.report().lines("      "));
            }
        }
        out.push(String::new());
        out.push(self.verdict_line());
        if let Some(b) = self.breakdown() {
            out.extend(b);
        }
        let superseded: Vec<&str> = self
            .members
            .iter()
            .filter(|m| m.seat == Seat::Superseded)
            .map(|m| m.name.as_str())
            .collect();
        if !superseded.is_empty() {
            out.push(format!(
                "  EXCLUDED, NOT SCORED: {} weak-default `??_G' or non-EH vector iterator COMDAT(s) whose slot the image's `??_E' holds:",
                superseded.len()
            ));
            out.extend(superseded.iter().map(|n| format!("      {n}")));
        }
        let dropped: Vec<&str> = self
            .members
            .iter()
            .filter(|m| m.seat == Seat::Dropped)
            .map(|m| m.name.as_str())
            .collect();
        if !dropped.is_empty() {
            out.push(format!(
                "  EXCLUDED, NOT SCORED: {} COMDAT(s) the image's dropped list names: functions the original link's /OPT:REF removed, so the image holds no body to compare (their rows' figures are not a score):",
                dropped.len()
            ));
            out.extend(dropped.iter().map(|n| format!("      {n}")));
        }
        let duplicates: Vec<&Scored> = self
            .members
            .iter()
            .filter(|m| m.seat == Seat::Duplicate)
            .collect();
        if !duplicates.is_empty() {
            out.push(format!(
                "  EXCLUDED, NOT SCORED: {} SELECT_ANY COMDAT(s) the link discards: LINK keeps the first definition in its order, an earlier object's copy fits the address, and this one has no image bytes:",
                duplicates.len()
            ));
            out.extend(duplicates.iter().map(|m| {
                format!(
                    "      {:#08x} {} (kept: {})",
                    m.at,
                    m.name,
                    m.kept_by.as_deref().unwrap_or("?")
                )
            }));
        }
        if self.pad_skipped > 0 {
            out.push(format!(
                "  note: {} byte(s) fall in a declared text-pad residue range and are not scored",
                self.pad_skipped
            ));
        }
        out
    }
}

#[must_use]
pub fn fictional_placement(bad: usize, comparable: usize) -> Option<u32> {
    if comparable < FICTIONAL_FLOOR || bad == 0 {
        return None;
    }
    let pct = u32::try_from(bad.saturating_mul(100) / comparable).ok()?;
    (pct > 85).then_some(pct)
}

#[must_use]
pub fn demangle(symbol: Option<&str>) -> String {
    let mangled = symbol.unwrap_or("<static>");
    msvc_demangler::demangle(mangled, msvc_demangler::DemangleFlags::llvm())
        .unwrap_or_else(|_| mangled.to_string())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Plan {
    Predict,
    Asserted(Vec<u32>),
}

fn text_off(env: &Env, at: u32) -> Option<usize> {
    at.checked_sub(env.text_rva).map(|o| o as usize)
}

#[must_use]
pub fn jump_table_seat(env: &Env, base: u32, addr: u32, len: u32) -> Option<Seat> {
    let seats = &env.seats;
    let k = seats.binary_search_by_key(&addr, |x| x.0).ok()?;
    let rec = seats.get(k)?.1;
    let end = addr.checked_add(len)?;
    if rec == 0
        || rec >= len
        || seats
            .get(k.checked_add(1)?)
            .is_some_and(|next| next.0 < end)
    {
        return None;
    }
    let off = text_off(env, addr)?;
    let img = env.text().get(off..off.checked_add(len as usize)?)?;
    let code = img.get(..rec as usize)?;
    let va =
        |i: usize| -> Option<u32> { base.checked_add(addr)?.checked_add(u32::try_from(i).ok()?) };
    let named =
        |i: usize| -> bool { va(i).is_some_and(|v| code.windows(4).any(|w| w == v.to_le_bytes())) };
    let entry = |i: usize| -> bool {
        img.get(i..i.saturating_add(4))
            .and_then(|b| <[u8; 4]>::try_from(b).ok())
            .map(u32::from_le_bytes)
            .and_then(|v| v.checked_sub(base))
            .is_some_and(|r| r >= addr && r < addr.saturating_add(rec))
    };
    let aligned = |i: usize| (addr as usize).saturating_add(i).is_multiple_of(4);
    let n = len as usize;
    let mut i = rec as usize;
    let mut entries = 0u32;
    loop {
        let found = (i..i.saturating_add(4)).find(|&j| aligned(j) && entry(j) && named(j));
        let Some(t) = found else {
            let tail = img.get(i..n)?;
            let ok = entries > 0 && tail.len() < 16 && tail.iter().all(|&b| b == NOP);
            return ok.then_some(Seat::JumpTable { rec, entries });
        };
        i = t;
        let mut count = 0usize;
        while i.saturating_add(4) <= n && entry(i) {
            count = count.saturating_add(1);
            i = i.saturating_add(4);
        }
        entries = entries.checked_add(u32::try_from(count).ok()?)?;
        if i < n && named(i) {
            let idx = img
                .get(i..n)?
                .iter()
                .take_while(|&&b| usize::from(b) < count)
                .count();
            if idx == 0 {
                return None;
            }
            i = i.saturating_add(idx);
        }
    }
}

#[must_use]
pub fn unrecorded_seat(
    env: &Env,
    base: u32,
    addr: u32,
    len: u32,
    align: u32,
    fill: &[u8],
) -> Option<Seat> {
    if align < 16 || !addr.is_multiple_of(align) || len == 0 {
        return None;
    }
    let seats = &env.seats;
    let end = addr.checked_add(len)?;
    let fill_end = end.checked_add(15)? / 16 * 16;
    let Err(k) = seats.binary_search_by_key(&addr, |x| x.0) else {
        return None;
    };
    let prev_end = match k.checked_sub(1) {
        Some(p) => Some(seats.get(p).map(|p| p.0.saturating_add(p.1))?),
        None => None,
    };
    let next_ok = seats.get(k).is_none_or(|n| n.0 >= fill_end);
    if prev_end.is_some_and(|e| e > addr) || !next_ok {
        return None;
    }
    let text = env.text();
    let off = text_off(env, addr)?;
    let before_ok = prev_end == Some(addr)
        || off
            .checked_sub(1)
            .is_none_or(|p| text.get(p).is_some_and(|b| fill.contains(b)));
    if !before_ok {
        return None;
    }
    let body = text.get(off..off.checked_add(len as usize)?)?;
    let code = if fill.contains(&NOP) {
        let tail = body.iter().rev().take_while(|&&b| b == NOP).count();
        body.get(..body.len().checked_sub(tail.min(15))?)?
    } else {
        body
    };
    let gap =
        text.get(off.checked_add(code.len())?..off.checked_add((fill_end - addr) as usize)?)?;
    if !gap.iter().all(|b| fill.contains(b)) {
        return None;
    }
    let mut dec = Decoder::with_ip(
        32,
        code,
        u64::from(base.checked_add(addr)?),
        DecoderOptions::NONE,
    );
    let mut last = None;
    while dec.can_decode() {
        let i = dec.decode();
        if i.is_invalid() {
            return None;
        }
        last = Some(i.flow_control());
    }
    matches!(
        last,
        Some(FlowControl::UnconditionalBranch | FlowControl::IndirectBranch | FlowControl::Return)
    )
    .then_some(Seat::Unrecorded)
}

fn tail_seat(t: Tail) -> Seat {
    match t {
        Tail::NopTail { rec } => Seat::NopTail { rec },
        Tail::Padded { pad, len } => Seat::Padded { pad, len },
    }
}

fn exact(env: &Env, addr: u32, len: u32) -> bool {
    env.recorded(addr) == Some(len)
}

#[must_use]
pub fn asserted_seat(env: &Env, base: u32, addr: u32, c: &Comdat) -> Option<Seat> {
    let len = u32::try_from(c.bytes.len()).ok()?;
    if exact(env, addr, len) {
        return Some(Seat::Image);
    }
    segments::padded(env, addr, len)
        .or_else(|| segments::nop_tail(env, addr, &c.bytes))
        .map(tail_seat)
        .or_else(|| jump_table_seat(env, base, addr, len))
        .or_else(|| unrecorded_seat(env, base, addr, len, c.align, &LINK_FILL))
}

fn predicted_seat(
    env: &Env,
    base: u32,
    c: &Comdat,
    packed: u32,
    prev_start: u32,
    prev_len: u32,
) -> (u32, Seat) {
    let len = u32::try_from(c.bytes.len()).unwrap_or(u32::MAX);
    let floor = prev_start.saturating_add(prev_len);
    let reserved = prev_start
        .saturating_add(prev_len.saturating_add(prev_len / 4))
        .div_ceil(RESERVE_ALIGN)
        .saturating_mul(RESERVE_ALIGN);
    let at_floor = |addr: u32| addr >= floor;
    let candidates = [reserved, packed];
    for addr in candidates {
        if at_floor(addr) && exact(env, addr, len) {
            return (addr, Seat::Image);
        }
    }
    for addr in candidates {
        if let Some(t) = segments::padded(env, addr, len).filter(|_| at_floor(addr)) {
            return (addr, tail_seat(t));
        }
    }
    for addr in candidates {
        if let Some(t) = segments::nop_tail(env, addr, &c.bytes).filter(|_| at_floor(addr)) {
            return (addr, tail_seat(t));
        }
    }
    for addr in candidates {
        if let Some(s) = jump_table_seat(env, base, addr, len).filter(|_| at_floor(addr)) {
            return (addr, s);
        }
    }
    (packed, Seat::Unseated)
}

fn opens_as_vector_deleting_dtor(body: &[u8], ip: u32) -> bool {
    let mut dec = Decoder::with_ip(32, body, u64::from(ip), DecoderOptions::NONE);
    for _ in 0..8 {
        if !dec.can_decode() {
            return false;
        }
        let i = dec.decode();
        if i.is_invalid() || i.mnemonic() == Mnemonic::Ret || i.mnemonic() == Mnemonic::Call {
            return false;
        }
        if i.mnemonic() == Mnemonic::Test
            && i.op_count() == 2
            && matches!(
                i.op1_kind(),
                OpKind::Immediate8 | OpKind::Immediate16 | OpKind::Immediate32
            )
        {
            return i.immediate(1) == 2;
        }
    }
    false
}

fn vector_dtor_passes_element_dtor(body: &[u8], ip: u32) -> bool {
    let mut dec = Decoder::with_ip(32, body, u64::from(ip), DecoderOptions::NONE);
    for _ in 0..24 {
        if !dec.can_decode() {
            return false;
        }
        let i = dec.decode();
        if i.is_invalid() || i.mnemonic() == Mnemonic::Ret || i.mnemonic() == Mnemonic::Call {
            return false;
        }
        if i.mnemonic() == Mnemonic::Push && i.op0_kind() == OpKind::Immediate32 {
            return true;
        }
    }
    false
}

#[must_use]
pub fn is_vector_iterator(sym: &str) -> bool {
    sym.starts_with("??_H@") || sym.starts_with("??_I@")
}

#[must_use]
pub fn superseded_at(
    sym: Option<&str>,
    ours: &[u8],
    weak: &BTreeMap<String, String>,
    image: &[u8],
    at: u32,
) -> bool {
    let Some(sym) = sym else {
        return false;
    };
    let vector_at_seat = || opens_as_vector_deleting_dtor(image, at);
    let weak_g = sym.strip_prefix("??_G").is_some_and(|rest| {
        weak.get(&format!("??_E{rest}")).map(String::as_str) == Some(sym) && vector_at_seat()
    });
    let iterator = is_vector_iterator(sym) && vector_at_seat();
    let lost_e = sym.starts_with("??_E")
        && opens_as_vector_deleting_dtor(image, at)
        && vector_dtor_passes_element_dtor(image, at)
        && opens_as_vector_deleting_dtor(ours, at)
        && !vector_dtor_passes_element_dtor(ours, at);
    weak_g || iterator || lost_e
}

fn in_text_pad(env: &Env, rva: u32) -> bool {
    env.text_pad
        .iter()
        .any(|&(pad, len)| rva >= pad && rva < pad.saturating_add(len))
}

pub type Fits<'a> = dyn Fn(&[u8]) -> bool + 'a;

pub type Kept<'a> = dyn Fn(&str, &Fits<'_>) -> Option<String> + 'a;

pub fn seat_and_score(
    env: &Env,
    object: &[u8],
    comdats: &[Comdat],
    rva: u32,
    plan: &Plan,
) -> Result<Score> {
    seat_and_score_with(env, object, comdats, rva, plan, &|_, _| None)
}

#[must_use]
pub fn fits_at(env: &Env, base: u32, at: u32, bytes: &[u8]) -> bool {
    let Ok(len) = u32::try_from(bytes.len()) else {
        return false;
    };
    exact(env, at, len)
        || segments::padded(env, at, len).is_some()
        || segments::nop_tail(env, at, bytes).is_some()
        || jump_table_seat(env, base, at, len).is_some()
}

pub fn seat_and_score_with(
    env: &Env,
    object: &[u8],
    comdats: &[Comdat],
    rva: u32,
    plan: &Plan,
    kept: &Kept<'_>,
) -> Result<Score> {
    let text = env.text();
    let base = u32::try_from(env.base).context("an image base past 4 GiB")?;
    let weak = crate::comdat::weak_defaults_of(object).unwrap_or_default();
    let any = select_any_sections(object);
    let asserted = match plan {
        Plan::Asserted(v) if !v.is_empty() => Some(v.as_slice()),
        _ => None,
    };
    let mut score = Score {
        rva,
        ..Score::default()
    };
    let mut at = rva;
    let mut prev_start = rva;
    let mut prev_len = 0u32;
    for (idx, c) in comdats.iter().enumerate() {
        let len = u32::try_from(c.bytes.len()).context("a COMDAT past 4 GiB")?;
        let mut seat = Seat::Given;
        if idx > 0 {
            let a = c.align.max(1);
            let packed = at
                .checked_next_multiple_of(a)
                .ok_or_else(|| anyhow!("{at:#x} rounds past the address space"))?;
            match asserted {
                Some(ms) => match ms.get(idx - 1) {
                    Some(&want) => {
                        at = want;
                        seat = asserted_seat(env, base, want, c).unwrap_or(Seat::Misasserted);
                    }
                    None => {
                        at = packed;
                        seat = Seat::Misasserted;
                    }
                },
                None => {
                    (at, seat) = predicted_seat(env, base, c, packed, prev_start, prev_len);
                }
            }
        }
        let off = text_off(env, at).ok_or_else(|| anyhow!("{at:#x} is before .text"))?;
        let dropped = c
            .symbol
            .as_deref()
            .is_some_and(|s| env.dropped.contains_key(s));
        let want = match text.get(off..off.saturating_add(c.bytes.len())) {
            Some(w) => w,
            None if dropped => &[],
            None => bail!("{at:#x}+{len:#x} runs past .text"),
        };
        let superseded = text
            .get(off..)
            .is_some_and(|b| superseded_at(c.symbol.as_deref(), &c.bytes, &weak, b, at));
        if superseded {
            seat = Seat::Superseded;
        }
        if dropped {
            seat = Seat::Dropped;
        }
        let mut kept_by = None;
        if seat == Seat::Misasserted
            && any.contains(&c.index)
            && let Some(sym) = c.symbol.as_deref()
        {
            kept_by = kept(sym, &|b: &[u8]| fits_at(env, base, at, b));
            if kept_by.is_some() {
                seat = Seat::Duplicate;
            }
        }
        let mut bad = Vec::new();
        if !superseded && !dropped && kept_by.is_none() {
            for (i, (got, exp)) in c.bytes.iter().zip(want).enumerate() {
                if c.relocated.get(i).copied().unwrap_or(false) {
                    continue;
                }
                let here = at.saturating_add(u32::try_from(i).unwrap_or(u32::MAX));
                if in_text_pad(env, here) {
                    score.pad_skipped = score.pad_skipped.saturating_add(1);
                    continue;
                }
                score.compared = score.compared.saturating_add(1);
                if got != exp {
                    bad.push(i);
                    score.differing = score.differing.saturating_add(1);
                }
            }
        }
        let literal = c.literal_len();
        score.members.push(Scored {
            at,
            seat,
            symbol: c.symbol.clone(),
            name: demangle(c.symbol.as_deref()),
            bad,
            comparable: literal,
            masked: c.bytes.len().saturating_sub(literal),
            relocs: c.relocated.iter().filter(|r| **r).count() / FIELD,
            want: want.to_vec(),
            comdat: c.clone(),
            kept_by,
        });
        prev_start = at;
        prev_len = len;
        at = at.saturating_add(len);
    }
    Ok(score)
}

pub fn choose_anchor(hint: Option<u32>, members: &[(String, u32, bool)]) -> Result<Anchor> {
    let (root_name, root) = members
        .first()
        .map(|(n, r, _)| (n.clone(), *r))
        .ok_or_else(|| anyhow!("the object has no .text COMDAT to anchor"))?;
    let own = members
        .iter()
        .find(|(_, _, any)| !any)
        .map(|(n, r, _)| (n.clone(), *r));
    match hint {
        Some(h) if h == root => Ok(Anchor {
            root,
            root_name,
            own,
            moved: false,
        }),
        Some(h) if !members.iter().any(|(_, r, _)| *r == h) => bail!(
            "the anchor {h:#x} is no COMDAT of this object, so it names another unit's function; this object's COMDAT[0] ({root_name}) resolves to {root:#x}{}: pass that, the unit's own first function, or `auto`",
            own.as_ref().map_or(String::new(), |(n, r)| format!(
                " and its own first function {n} to {r:#x}"
            ))
        ),
        _ => Ok(Anchor {
            root,
            root_name,
            own,
            moved: true,
        }),
    }
}

pub fn park_dropped(ms: &mut [crate::seat::Member], is_dropped: impl Fn(&str) -> bool) {
    let first_held = ms.iter().find_map(|m| m.rva);
    let mut prev = None;
    for m in ms.iter_mut() {
        if m.rva.is_none() && is_dropped(&m.name) {
            m.rva = prev.or(first_held);
        }
        prev = m.rva.or(prev);
    }
}

pub fn derive(
    env: &Env,
    object: &[u8],
    comdats: &[Comdat],
    scopes: &[&str],
    hint: Option<u32>,
) -> Result<(Vec<u32>, Anchor)> {
    let mut ms = crate::seat::members_scoped(env, scopes, object, comdats)?;
    if let Some(seat) = ms
        .iter()
        .find(|m| m.name.starts_with("??_E") && m.rva.is_some())
        .and_then(|m| m.rva)
    {
        for m in ms
            .iter_mut()
            .filter(|m| m.rva.is_none() && is_vector_iterator(&m.name))
        {
            m.rva = Some(seat);
        }
    }
    if let Some((m, rva)) = ms
        .iter()
        .filter(|m| env.dropped.contains_key(&m.name))
        .find_map(|m| Some((m, m.rva?)))
    {
        bail!(
            "{} is on the image's dropped list but resolves to {rva:#x}: the image holds a body for it, so it is not dropped and is scored; take it off the list",
            m.name
        );
    }
    park_dropped(&mut ms, |name| env.dropped.contains_key(name));
    let unresolved: Vec<&str> = ms
        .iter()
        .filter(|m| m.rva.is_none())
        .map(|m| m.name.as_str())
        .collect();
    if !unresolved.is_empty() {
        bail!(
            "--auto-members cannot resolve {} of this object's {} COMDAT(s) to an image address, so the list would be short and every member after the first gap would score against the wrong function; give each a name in one of the image's name providers:\n    {}",
            unresolved.len(),
            ms.len(),
            unresolved.join("\n    ")
        );
    }
    let any = select_any_sections(object);
    let resolved: Vec<(String, u32, bool)> = ms
        .iter()
        .zip(comdats)
        .filter_map(|(m, c)| Some((m.name.clone(), m.rva?, any.contains(&c.index))))
        .collect();
    let anchor = choose_anchor(hint, &resolved)?;
    Ok((ms.iter().skip(1).filter_map(|m| m.rva).collect(), anchor))
}

#[must_use]
pub fn select_any_sections(object: &[u8]) -> Vec<u16> {
    crate::comdat::parse(object)
        .map(|o| {
            o.comdats()
                .into_iter()
                .filter(|c| c.selection == msvc6_image::coff::SELECT_ANY)
                .map(|c| c.section)
                .collect()
        })
        .unwrap_or_default()
}
