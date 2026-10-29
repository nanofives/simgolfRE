use crate::listing::Function;
use iced_x86::{Code, FlowControl, OpKind, Register};
use msvc6_diff::decode::{Dec, decode_keyed};
use serde::Serialize;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Event {
    State(i64),
    Call(String),
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize)]
pub struct Exit {
    pub state: Option<i64>,
    pub dtors: Vec<String>,
    pub other: usize,
}

#[must_use]
pub fn is_dtor(sym: &str) -> bool {
    sym.starts_with("??1") || sym.starts_with("??_D") || sym.starts_with("??_G")
}

#[must_use]
pub fn short(sym: &str) -> String {
    let class = |rest: &str| rest.split("@@").next().unwrap_or(rest).to_string();
    if let Some(r) = sym.strip_prefix("??1") {
        return format!("~{}", class(r));
    }
    if let Some(r) = sym.strip_prefix("??_D") {
        return format!("D~{}", class(r));
    }
    if let Some(r) = sym.strip_prefix("??_G") {
        return format!("G~{}", class(r));
    }
    if let Some(r) = sym.strip_prefix('?')
        && !r.starts_with('?')
    {
        let mut parts = r.split("@@").next().unwrap_or(r).split('@');
        let f = parts.next().unwrap_or("");
        let scopes: Vec<&str> = parts.collect();
        if !scopes.is_empty() {
            let mut s: Vec<&str> = scopes.into_iter().rev().collect();
            s.push(f);
            return s.join("::");
        }
    }
    sym.to_string()
}

#[must_use]
pub fn cod_event(text: &str) -> Option<Event> {
    if text.starts_with("mov ") && text.contains("__$EHRec$[esp+") {
        let v = text.rsplit(',').next()?.trim();
        return v.parse::<i64>().ok().map(Event::State);
    }
    let callee = text.strip_prefix("call ")?.trim();
    (!callee.contains(' ') && !callee.contains('[')).then(|| Event::Call(callee.to_string()))
}

#[must_use]
pub fn exits(events: &[Event], openers: &[String]) -> Vec<Exit> {
    let mut out: Vec<Exit> = Vec::new();
    let mut state = None;
    for e in events {
        match e {
            Event::State(s) => state = Some(*s),
            Event::Call(c) => {
                if openers.iter().any(|p| c.contains(p.as_str())) {
                    out.push(Exit {
                        state,
                        dtors: vec![short(c)],
                        other: 0,
                    });
                } else if let Some(x) = out.last_mut() {
                    if is_dtor(c) || c.starts_with('[') {
                        x.dtors.push(short(c));
                    } else {
                        x.other += 1;
                    }
                }
            }
        }
    }
    out
}

#[must_use]
pub fn census(events: &[Event], openers: &[String]) -> BTreeMap<String, (usize, usize)> {
    let mut out: BTreeMap<String, (usize, usize)> = BTreeMap::new();
    let mut opened = false;
    for e in events {
        if let Event::Call(c) = e {
            opened |= openers.iter().any(|p| c.contains(p.as_str()));
            let n = out.entry(short(c)).or_default();
            if opened {
                n.1 += 1;
            } else {
                n.0 += 1;
            }
        }
    }
    out
}

fn keyed(xs: &[Exit]) -> Vec<((Option<i64>, usize), &Exit)> {
    let mut seen: BTreeMap<Option<i64>, usize> = BTreeMap::new();
    xs.iter()
        .map(|x| {
            let n = seen.entry(x.state).or_default();
            *n += 1;
            ((x.state, *n), x)
        })
        .collect()
}

fn cell(x: Option<&Exit>) -> String {
    x.map_or_else(
        || "-".to_string(),
        |x| {
            let mut s = x.dtors.join(" ");
            if x.other > 0 {
                s.push_str(&format!("  (+{} other)", x.other));
            }
            s
        },
    )
}

#[must_use]
pub fn image_store(insn: &iced_x86::Instruction) -> Option<(u32, i64)> {
    let imm = match insn.code() {
        Code::Mov_rm32_imm32 => i64::from(insn.immediate32().cast_signed()),
        Code::Mov_rm8_imm8 => i64::from(insn.immediate8().cast_signed()),
        _ => return None,
    };
    (insn.op0_kind() == OpKind::Memory
        && insn.memory_base() == Register::ESP
        && insn.memory_index() == Register::None)
        .then(|| (insn.memory_displacement32(), imm))
}

#[derive(Debug, Clone, Copy)]
pub struct Ours<'a> {
    pub symbol: &'a str,
    pub listing: &'a Function,
    pub bytes: &'a [u8],
    pub relocated: &'a [bool],
}

#[derive(Debug, Clone, Copy)]
pub struct Reference<'a> {
    pub rva: u32,
    pub bytes: &'a [u8],
    pub base: u64,
    pub size: u32,
    pub bounded: bool,
}

pub struct Inputs<'a> {
    pub ours: Ours<'a>,
    pub image: Reference<'a>,
    pub openers: &'a [String],
    pub comdats: &'a [(String, Vec<u8>, Vec<bool>)],
    pub at: &'a dyn Fn(u32, usize) -> Option<&'a [u8]>,
    pub name: &'a dyn Fn(u32) -> Option<String>,
}

impl std::fmt::Debug for Inputs<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Inputs")
            .field("ours", &self.ours)
            .field("image", &self.image)
            .field("openers", &self.openers)
            .finish_non_exhaustive()
    }
}

pub type Paired = (Option<i64>, usize, Option<Exit>, Option<Exit>);
pub type Counted = (String, (usize, usize), (usize, usize));

#[derive(Debug, Clone, Default, Serialize)]
pub struct Ladder {
    pub ours_insns: usize,
    pub image_insns: usize,
    pub aligned: usize,
    pub state_disps: usize,
    pub rows: Vec<Paired>,
    pub same: usize,
    pub ours_exits: usize,
    pub image_exits: usize,
    pub split: Vec<String>,
    pub fronts: (Option<usize>, Option<usize>),
    pub census: Vec<Counted>,
    pub census_agree: usize,
}

#[must_use]
pub fn ladder(i: &Inputs<'_>) -> Ladder {
    let relocated = i.ours.relocated;
    let ours_mask = |ip: u32, len: u32, _v: u64| {
        let lo = usize::try_from(ip).unwrap_or(usize::MAX);
        let hi = lo.saturating_add(usize::try_from(len).unwrap_or(0));
        relocated.get(lo..hi).is_some_and(|m| m.iter().any(|b| *b))
    };
    let (base, top) = (i.image.base, i.image.base + u64::from(i.image.size));
    let image_mask = |_ip: u32, _len: u32, v: u64| v >= base && v < top;
    let od = decode_keyed(i.ours.bytes, &ours_mask, false);
    let mut rd = decode_keyed(i.image.bytes, &image_mask, false);
    if !i.image.bounded {
        let end = rd
            .iter()
            .position(|d| {
                usize::try_from(d.ip).unwrap_or(usize::MAX) >= i.ours.bytes.len()
                    && d.insn.flow_control() == FlowControl::Return
            })
            .map_or(rd.len(), |k| k + 1);
        rd.truncate(end);
    }
    let ok: Vec<&str> = od.iter().map(|d| d.key.as_str()).collect();
    let rk: Vec<&str> = rd.iter().map(|d| d.key.as_str()).collect();
    let aligned: BTreeMap<usize, usize> = msvc6_diff::insndiff::align(&ok, &rk)
        .into_iter()
        .filter_map(|(o, r)| Some((o?, r?)))
        .filter(|(o, r)| ok.get(*o) == rk.get(*r))
        .collect();
    let by_ip: BTreeMap<u32, usize> = od.iter().enumerate().map(|(k, d)| (d.ip, k)).collect();
    let target = |d: &Dec| {
        i.image
            .rva
            .wrapping_add(u32::try_from(d.insn.near_branch_target()).unwrap_or(u32::MAX))
    };
    let is_call = |d: &Dec| {
        d.insn.flow_control() == FlowControl::Call && d.insn.op0_kind() == OpKind::NearBranch32
    };
    let mut ours_events = Vec::new();
    let mut votes: BTreeMap<u32, BTreeMap<String, usize>> = BTreeMap::new();
    let mut state_disps: BTreeSet<u32> = BTreeSet::new();
    for ci in &i.ours.listing.insns {
        let Some(e) = cod_event(&ci.text) else {
            continue;
        };
        if let Some(r) = by_ip
            .get(&ci.off)
            .and_then(|oi| aligned.get(oi))
            .and_then(|ri| rd.get(*ri))
        {
            match &e {
                Event::State(_) => {
                    if let Some((d, _)) = image_store(&r.insn) {
                        state_disps.insert(d);
                    }
                }
                Event::Call(c) if is_call(r) => {
                    *votes
                        .entry(target(r))
                        .or_default()
                        .entry(c.clone())
                        .or_default() += 1;
                }
                Event::Call(_) => {}
            }
        }
        ours_events.push(e);
    }
    let bytes_say = |t: u32, n: &str| -> Option<bool> {
        let (_, bytes, mask) = i.comdats.iter().find(|(s, _, _)| s == n)?;
        let at = (i.at)(t, bytes.len())?;
        Some(
            bytes
                .iter()
                .zip(at)
                .zip(mask.iter().chain(std::iter::repeat(&false)))
                .all(|((a, b), m)| *m || a == b),
        )
    };
    let mut evidence: BTreeMap<u32, &str> = BTreeMap::new();
    let names: BTreeMap<u32, String> = votes
        .iter()
        .filter_map(|(t, v)| {
            if let Some(n) = v.keys().find(|n| bytes_say(*t, n) == Some(true)) {
                evidence.insert(*t, "bytes");
                return Some((*t, n.clone()));
            }
            let best = v
                .iter()
                .filter(|(n, _)| bytes_say(*t, n) != Some(false))
                .max_by_key(|(n, c)| (**c, std::cmp::Reverse((*n).clone())))?;
            evidence.insert(*t, if v.len() > 1 { "vote" } else { "aligned" });
            Some((*t, best.0.clone()))
        })
        .collect();
    let name_of = |t: u32| -> String {
        names
            .get(&t)
            .cloned()
            .unwrap_or_else(|| format!("[{}]", (i.name)(t).unwrap_or_else(|| format!("sub_{t:x}"))))
    };
    let mut image_events = Vec::new();
    let mut image_front: Option<usize> = None;
    for (k, d) in rd.iter().enumerate() {
        if let Some((disp, v)) = image_store(&d.insn) {
            if state_disps.contains(&disp) {
                image_events.push(Event::State(v));
            }
        } else if is_call(d) {
            let n = name_of(target(d));
            if image_front.is_none() && i.openers.iter().any(|p| n.contains(p.as_str())) {
                image_front = Some(k);
            }
            image_events.push(Event::Call(n));
        }
    }
    let ox = exits(&ours_events, i.openers);
    let ix = exits(&image_events, i.openers);
    let ok_ = keyed(&ox);
    let ik_ = keyed(&ix);
    let ours_by: BTreeMap<_, _> = ok_.iter().map(|(k, x)| (*k, *x)).collect();
    let image_by: BTreeMap<_, _> = ik_.iter().map(|(k, x)| (*k, *x)).collect();
    let mut order: Vec<(Option<i64>, usize)> = ok_.iter().map(|(k, _)| *k).collect();
    order.extend(
        ik_.iter()
            .map(|(k, _)| *k)
            .filter(|k| !ours_by.contains_key(k)),
    );
    let mut out = Ladder {
        ours_insns: od.len(),
        image_insns: rd.len(),
        aligned: aligned.len(),
        state_disps: state_disps.len(),
        ours_exits: ox.len(),
        image_exits: ix.len(),
        ..Ladder::default()
    };
    for k in &order {
        let (o, im) = (ours_by.get(k).copied(), image_by.get(k).copied());
        if matches!((o, im), (Some(a), Some(b)) if a.dtors == b.dtors) {
            out.same += 1;
        }
        out.rows.push((k.0, k.1, o.cloned(), im.cloned()));
    }
    out.split = votes
        .iter()
        .filter(|(_, v)| v.len() > 1)
        .map(|(t, v)| {
            let each: Vec<String> = v.iter().map(|(n, c)| format!("{}x{c}", short(n))).collect();
            let chose = names
                .get(t)
                .map_or_else(|| "none".to_string(), |n| short(n));
            let why = evidence.get(t).copied().unwrap_or("struck");
            format!("{t:#x} {} -> {chose} by {why}", each.join("/"))
        })
        .collect();
    let ours_front = i
        .ours
        .listing
        .insns
        .iter()
        .find(|ci| {
            matches!(cod_event(&ci.text), Some(Event::Call(c)) if i.openers.iter().any(|p| c.contains(p.as_str())))
        })
        .and_then(|ci| by_ip.get(&ci.off).copied());
    out.fronts = (ours_front, image_front);
    let oc = census(&ours_events, i.openers);
    let ic = census(&image_events, i.openers);
    let keys: BTreeSet<&String> = oc.keys().chain(ic.keys()).collect();
    for k in keys {
        let (o, im) = (
            oc.get(k).copied().unwrap_or_default(),
            ic.get(k).copied().unwrap_or_default(),
        );
        if o == im {
            out.census_agree += 1;
        } else {
            out.census.push((k.clone(), o, im));
        }
    }
    out
}

#[must_use]
pub fn report(
    l: &Ladder,
    label: &str,
    rva: u32,
    symbol: &str,
    openers: &[String],
    all: bool,
) -> Vec<String> {
    let mut out = vec![format!(
        "{label} {rva:#x}  `{symbol}'  ours {} insn(s) / image {} decoded, {} aligned; {} image state displacement(s) learned; exits open at {}",
        l.ours_insns,
        l.image_insns,
        l.aligned,
        l.state_disps,
        openers.join(" | ")
    )];
    for (state, n, o, im) in &l.rows {
        let flag = match (o, im) {
            (Some(a), Some(b)) if a.dtors == b.dtors => "",
            _ => "  DIFFERS",
        };
        let st = state.map_or_else(|| "?".to_string(), |s| s.to_string());
        out.push(format!("  st {st:>4}#{n}{flag}"));
        out.push(format!("    ours : {}", cell(o.as_ref())));
        out.push(format!("    image: {}", cell(im.as_ref())));
    }
    out.push(format!(
        "  {} of {} exit(s) carry the same destructor calls ({} ours, {} image).",
        l.same,
        l.rows.len(),
        l.ours_exits,
        l.image_exits
    ));
    if !l.split.is_empty() {
        out.push(format!(
            "  image targets several of our names aligned to (`bytes' = our COMDAT equals the image there): {}",
            l.split.join("; ")
        ));
    }
    if all {
        let front = |x: Option<usize>| x.map_or_else(|| "-".to_string(), |n| n.to_string());
        out.push(format!(
            "  front (before the first exit): ours {} insn(s), image {} insn(s)",
            front(l.fronts.0),
            front(l.fronts.1)
        ));
        out.push("  callees whose count differs (front / exits, ours vs image):".to_string());
        for (k, o, im) in &l.census {
            out.push(format!(
                "    {k:<60} front {:>2} / {:<2}  exits {:>2} / {:<2}",
                o.0, im.0, o.1, im.1
            ));
        }
        out.push(format!(
            "  {} callee(s) differ, {} agree.",
            l.census.len(),
            l.census_agree
        ));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::listing;

    const COD: &str = concat!(
        ";\tCOMDAT ?f@N@@QAEHH@Z\r\n",
        "?f@N@@QAEHH@Z PROC NEAR\t\t\t\t\t; N::f, COMDAT\r\n",
        "  00000\tc7 84 24 60 22\r\n",
        "\t00 00 09 00 00\r\n",
        "\t00\t\t mov\t DWORD PTR __$EHRec$[esp+8768], 9\r\n",
        "  0000b\te8 00 00 00 00\t call\t ?ResetGameState@GameDialog@@QAEXXZ ; GameDialog::ResetGameState\r\n",
        "  00010\te8 00 00 00 00\t call\t ??1MultiWidget@@UAE@XZ\t; MultiWidget::~MultiWidget\r\n",
        "  00015\te8 00 00 00 00\t call\t ?Send@N@@QAEXXZ\t; N::Send\r\n",
        "  0001a\tc6 84 24 60 22\r\n",
        "\t00 00 12\t mov\t BYTE PTR __$EHRec$[esp+8768], 18\r\n",
        "  00022\te8 00 00 00 00\t call\t ?ResetGameState@GameDialog@@QAEXXZ ; GameDialog::ResetGameState\r\n",
        "  00027\te8 00 00 00 00\t call\t ??_DTextItemList@@QAEXXZ ; TextItemList::`vbase destructor'\r\n",
        "?f@N@@QAEHH@Z ENDP\t\t\t\t\t; N::f\r\n",
    );

    #[test]
    fn a_listing_cuts_into_exits_by_opener_and_state() {
        let (_, f) = listing::function(COD, Some("?f@N@@QAEHH@Z")).unwrap();
        let ev: Vec<Event> = f.insns.iter().filter_map(|i| cod_event(&i.text)).collect();
        assert_eq!(ev.len(), 7);
        let xs = exits(&ev, &["ResetGameState".to_string()]);
        assert_eq!(
            xs,
            vec![
                Exit {
                    state: Some(9),
                    dtors: vec!["GameDialog::ResetGameState".into(), "~MultiWidget".into()],
                    other: 1
                },
                Exit {
                    state: Some(18),
                    dtors: vec!["GameDialog::ResetGameState".into(), "D~TextItemList".into()],
                    other: 0
                },
            ]
        );
        let mut more = vec![Event::Call("?Send@N@@QAEXXZ".into())];
        more.extend(ev);
        let c = census(&more, &["ResetGameState".to_string()]);
        assert_eq!(c.get("N::Send"), Some(&(1, 1)));
        assert_eq!(c.get("GameDialog::ResetGameState"), Some(&(0, 2)));
    }

    #[test]
    fn short_names_every_destructor_form_and_an_image_store_is_an_esp_move() {
        assert_eq!(short("??1GameDialog@@UAE@XZ"), "~GameDialog");
        assert_eq!(short("??_DMultiWidget@@QAEXXZ"), "D~MultiWidget");
        assert_eq!(short("??_GSprite@@UAEPAXI@Z"), "G~Sprite");
        assert_eq!(
            short("?RemoveById@PeerList@@QAEXH@Z"),
            "PeerList::RemoveById"
        );
        assert_eq!(short("_strcat"), "_strcat");
        let bytes = [
            0xc7, 0x84, 0x24, 0x60, 0x22, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0xc6, 0x84, 0x24,
            0x60, 0x22, 0x00, 0x00, 0x12, 0xc7, 0x44, 0x84, 0x08, 0x01, 0x00, 0x00, 0x00,
        ];
        let d = decode_keyed(&bytes, &|_, _, _| false, false);
        let s: Vec<Option<(u32, i64)>> = d.iter().map(|x| image_store(&x.insn)).collect();
        assert_eq!(s, vec![Some((0x2260, 9)), Some((0x2260, 18)), None]);
    }

    #[test]
    fn our_ladder_against_an_image_that_inlined_one_destructor_differs_at_that_exit() {
        let (sym, f) = listing::function(COD, Some("?f@N@@QAEHH@Z")).unwrap();
        let mut ours = vec![0xc7, 0x84, 0x24, 0x60, 0x22, 0, 0, 0x09, 0, 0, 0];
        for _ in 0..3 {
            ours.extend([0xe8, 0, 0, 0, 0]);
        }
        ours.extend([0xc6, 0x84, 0x24, 0x60, 0x22, 0, 0, 0x12]);
        ours.extend([0xe8, 0, 0, 0, 0, 0xe8, 0, 0, 0, 0]);
        let mask: Vec<bool> = ours
            .iter()
            .enumerate()
            .map(|(k, _)| matches!(k, 12..=15 | 17..=20 | 22..=25 | 35..=38 | 40..=43))
            .collect();
        let image = ours.clone();
        let i = Inputs {
            ours: Ours {
                symbol: &sym,
                listing: &f,
                bytes: &ours,
                relocated: &mask,
            },
            image: Reference {
                rva: 0x1000,
                bytes: &image,
                base: 0x40_0000,
                size: 0x1_0000,
                bounded: true,
            },
            openers: &["ResetGameState".to_string()],
            comdats: &[],
            at: &|_, _| None,
            name: &|_| None,
        };
        let l = ladder(&i);
        assert_eq!(l.ours_exits, 2);
        assert_eq!(l.image_exits, 2);
        assert_eq!(l.same, 2, "{:?}", l.rows);
        let lines = report(&l, "IMG", 0x1000, &sym, i.openers, true);
        assert!(
            lines
                .iter()
                .any(|x| x.starts_with("  2 of 2 exit(s) carry the same destructor calls")),
            "{lines:#?}"
        );
    }
}
