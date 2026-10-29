use anyhow::{Context as _, Result, bail};
use msvc6_base::units::Rva;
use serde::{Deserialize, Serialize};

use crate::pe::Image;
use crate::pe::image::DIRS_AT;
use crate::us;

pub const PAD_WITNESS: usize = 8;
pub const MAX_PASSES: usize = 8;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Kind {
    HeaderSlack,
    TextPad,
    HeaderField,
    ImportStamps,
}

impl Kind {
    #[must_use]
    pub fn name(self) -> &'static str {
        match self {
            Self::HeaderSlack => "header-slack",
            Self::TextPad => "text-pad",
            Self::HeaderField => "header-field",
            Self::ImportStamps => "import-stamps",
        }
    }

    #[must_use]
    pub fn positional(self) -> bool {
        matches!(self, Self::HeaderSlack | Self::TextPad)
    }

    #[must_use]
    pub fn default_fill(self) -> u8 {
        if self == Self::TextPad { 0x90 } else { 0 }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Anchor {
    SizeOfCode,
    SizeOfImage,
    Checksum,
    IatDirectory,
    ImportStamps,
    SectionTail,
}

impl Anchor {
    #[must_use]
    pub fn fixed_len(self) -> Option<usize> {
        match self {
            Self::SizeOfCode | Self::SizeOfImage | Self::Checksum => Some(4),
            Self::IatDirectory | Self::ImportStamps => Some(8),
            Self::SectionTail => None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Row {
    pub kind: Kind,
    #[serde(default)]
    pub off: Option<usize>,
    #[serde(default)]
    pub rva: Option<u32>,
    pub len: usize,
    #[serde(default)]
    pub at: Option<Anchor>,
    #[serde(default)]
    pub fill: Option<u8>,
    #[serde(default)]
    pub note: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub enum State {
    Matches,
    Writable,
    Blocked(String),
}

impl State {
    #[must_use]
    pub fn word(&self) -> &'static str {
        match self {
            Self::Matches => "MATCHES",
            Self::Writable => "WRITE",
            Self::Blocked(_) => "BLOCKED",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct RowSite {
    pub kind: Kind,
    pub off: usize,
    pub len: usize,
    pub at: Option<Anchor>,
    pub note: String,
    pub site: Option<usize>,
    pub extra: Option<(usize, u32)>,
    pub state: State,
    pub differing: usize,
}

impl RowSite {
    #[must_use]
    pub fn in_place(&self) -> bool {
        self.site == Some(self.off)
    }

    #[must_use]
    pub fn blocked(&self) -> bool {
        matches!(self.state, State::Blocked(_))
    }
}

fn descriptors(img: &Image) -> Result<Vec<(usize, String)>> {
    Ok(img
        .imports()?
        .into_iter()
        .filter_map(|t| {
            img.rva_to_off(t.descriptor)
                .map(|o| (o.get(), t.dll.to_ascii_lowercase()))
        })
        .collect())
}

fn anchor_site(a: Anchor, img: &Image, dll: Option<&str>) -> Result<Option<usize>> {
    let o = img.opt_off;
    Ok(match a {
        Anchor::SizeOfCode => Some(o.saturating_add(4)),
        Anchor::SizeOfImage => Some(o.saturating_add(56)),
        Anchor::Checksum => Some(o.saturating_add(64)),
        Anchor::IatDirectory => Some(o.saturating_add(DIRS_AT).saturating_add(8 * 12)),
        Anchor::ImportStamps => descriptors(img)?
            .into_iter()
            .find(|(_, n)| Some(n.as_str()) == dll)
            .map(|(d, _)| d.saturating_add(4)),
        Anchor::SectionTail => None,
    })
}

fn row_offset(row: &Row, r: &Image) -> Result<usize> {
    match (row.off, row.rva) {
        (Some(o), None) => Ok(o),
        (None, Some(rva)) => r.rva_to_off(Rva(rva)).map(|o| o.get()).with_context(|| {
            format!(
                "{} row at rva {rva:#x} has no file bytes in the reference",
                row.kind.name()
            )
        }),
        _ => bail!(
            "{} row: give exactly one of `off` and `rva`",
            row.kind.name()
        ),
    }
}

fn header_slack(img: &Image) -> (usize, usize) {
    (img.header_structs_end(), img.first_raw())
}

fn in_code(img: &Image, off: usize, len: usize) -> Option<String> {
    img.sections
        .iter()
        .find(|s| {
            s.is_code()
                && off >= s.raw_off.get()
                && off.saturating_add(len) <= s.raw_off.get().saturating_add(s.disk_len())
        })
        .map(|s| s.name.clone())
}

fn positional(row: &Row, rs: &mut RowSite, r: &Image, o: &Image, want: &[u8]) -> Result<()> {
    let (off, len) = (rs.off, rs.len);
    let (rb, ob) = (r.bytes(), o.bytes());
    let fill = row.fill.unwrap_or_else(|| row.kind.default_fill());
    let (ok_ref, ok_ours, region) = match row.kind {
        Kind::HeaderSlack => {
            let inside = |img: &Image| {
                let (lo, hi) = header_slack(img);
                off >= lo && off.saturating_add(len) <= hi
            };
            (inside(r), inside(o), "header slack")
        }
        _ => {
            let sec = in_code(r, off, len);
            let ours = sec
                .as_deref()
                .is_some_and(|n| in_code(o, off, len).as_deref() == Some(n));
            (sec.is_some(), ours, "code section")
        }
    };
    if !ok_ref {
        bail!(
            "{} row {off:#x}+{len} is not inside the reference's {region}",
            row.kind.name()
        );
    }
    rs.site = Some(off);
    rs.differing = (0..len)
        .filter(|&k| ob.get(off.saturating_add(k)) != want.get(k))
        .count();
    rs.state = if !ok_ours {
        State::Blocked(format!("the image has no {region} there"))
    } else if rs.differing == 0 {
        State::Matches
    } else {
        let end = if row.kind == Kind::TextPad {
            off.saturating_add(len).div_ceil(16).saturating_mul(16)
        } else {
            off.saturating_add(len)
        };
        let foreign = (off..end).find(|&k| ob.get(k) != Some(&fill) && ob.get(k) != rb.get(k));
        let witness_bad = row.kind == Kind::TextPad
            && off >= PAD_WITNESS
            && ob.get(off.saturating_sub(PAD_WITNESS)..off)
                != rb.get(off.saturating_sub(PAD_WITNESS)..off);
        match foreign {
            Some(k) => State::Blocked(format!(
                "the image holds {:#04x} at {k:#x}, not the fill {fill:#04x}: its own code is there",
                ob.get(k).copied().unwrap_or(0)
            )),
            None if witness_bad => State::Blocked(format!(
                "the {PAD_WITNESS} bytes before the row are not the reference's: another body precedes this pad"
            )),
            None => State::Writable,
        }
    };
    Ok(())
}

fn section_tail(rs: &mut RowSite, r: &Image, o: &Image, want: &[u8]) -> Result<()> {
    let (off, len) = (rs.off, rs.len);
    let ob = o.bytes();
    let Some(s) = r.section_at_file(off) else {
        bail!("section-tail row {off:#x}+{len} is in no reference section");
    };
    let vend = s.raw_off.get().saturating_add(us(s.virt_size.get()));
    if vend != off.saturating_add(len) {
        bail!(
            "section-tail row {off:#x}+{len} does not end at {}'s VirtualSize end {vend:#x} in the reference",
            s.name
        );
    }
    let Some(os) = o.section(&s.name) else {
        rs.state = State::Blocked(format!("the image has no {}", s.name));
        return Ok(());
    };
    let (raw, vs) = (os.raw_off.get(), us(os.virt_size.get()));
    let done = vs >= len
        && ob.get(raw.saturating_add(vs).saturating_sub(len)..raw.saturating_add(vs)) == Some(want);
    if done {
        rs.site = Some(raw.saturating_add(vs).saturating_sub(len));
        rs.differing = 0;
        rs.state = State::Matches;
        return Ok(());
    }
    let site = raw.saturating_add(vs);
    rs.site = Some(site);
    rs.differing = (0..len)
        .filter(|&k| ob.get(site.saturating_add(k)) != want.get(k))
        .count();
    let grown = u32::try_from(vs.saturating_add(len)).context("section size overflows")?;
    rs.extra = Some((os.vsize_field(), grown));
    let next = o
        .sections
        .get(os.index.saturating_add(1))
        .map_or(u32::MAX, |n| n.rva.get());
    rs.state = if vs.saturating_add(len) > us(os.raw_size.get()) {
        State::Blocked(format!(
            "{}'s file padding is shorter than {len} bytes",
            s.name
        ))
    } else if os.rva.get().saturating_add(grown) > next {
        State::Blocked(format!(
            "growing {} by {len} bytes would overlap the next section",
            s.name
        ))
    } else if let Some(k) = (site..site.saturating_add(len)).find(|&k| ob.get(k) != Some(&0)) {
        State::Blocked(format!(
            "the image holds {:#04x} at {k:#x} past {}'s content",
            ob.get(k).copied().unwrap_or(0),
            s.name
        ))
    } else {
        State::Writable
    };
    Ok(())
}

fn anchored(a: Anchor, rs: &mut RowSite, r: &Image, o: &Image, want: &[u8]) -> Result<()> {
    let (off, len) = (rs.off, rs.len);
    if a.fixed_len() != Some(len) {
        bail!(
            "{} row {off:#x}+{len}: `{a:?}` is {:?} byte(s)",
            rs.kind.name(),
            a.fixed_len()
        );
    }
    let dll = if a == Anchor::ImportStamps {
        Some(
            descriptors(r)
                .context("the reference's import descriptors")?
                .into_iter()
                .find(|(d, _)| d.saturating_add(4) == off)
                .map(|(_, n)| n)
                .with_context(|| {
                    format!("import-stamps row {off:#x} is not an import descriptor's TimeDateStamp in the reference")
                })?,
        )
    } else {
        None
    };
    if anchor_site(a, r, dll.as_deref())? != Some(off) {
        bail!(
            "{} row {off:#x}+{len} is not the reference's `{a:?}` field",
            rs.kind.name()
        );
    }
    let ob = o.bytes();
    match anchor_site(a, o, dll.as_deref()).context("the image's import descriptors")? {
        None => rs.state = State::Blocked(format!("the image has no `{a:?}` for {dll:?}")),
        Some(s) => {
            rs.site = Some(s);
            rs.differing = (0..len)
                .filter(|&k| ob.get(s.saturating_add(k)) != want.get(k))
                .count();
            let link_stamps = a != Anchor::ImportStamps
                || ob
                    .get(s..s.saturating_add(len))
                    .is_some_and(|b| b.iter().all(|&x| x == 0));
            rs.state = if rs.differing == 0 {
                State::Matches
            } else if link_stamps {
                State::Writable
            } else {
                State::Blocked("the image's descriptor stamps are not LINK's 0/0".to_string())
            };
        }
    }
    Ok(())
}

pub fn resolve(rows: &[Row], r: &Image, o: &Image) -> Result<Vec<RowSite>> {
    let mut out = Vec::new();
    for row in rows {
        let off = row_offset(row, r)?;
        let len = row.len;
        let want = r
            .bytes()
            .get(off..off.saturating_add(len))
            .with_context(|| {
                format!(
                    "{} row {off:#x}+{len} lies past the reference",
                    row.kind.name()
                )
            })?;
        let mut rs = RowSite {
            kind: row.kind,
            off,
            len,
            at: row.at,
            note: row.note.clone(),
            site: None,
            extra: None,
            state: State::Blocked(String::new()),
            differing: len,
        };
        match (row.kind.positional(), row.at) {
            (true, None) => positional(row, &mut rs, r, o, want)?,
            (true, Some(a)) => bail!(
                "{} row {off:#x}+{len} names `{a:?}`, but its kind is positional",
                row.kind.name()
            ),
            (false, None) => bail!(
                "{} row {off:#x}+{len} names no anchor (`at`)",
                row.kind.name()
            ),
            (false, Some(Anchor::SectionTail)) => section_tail(&mut rs, r, o, want)?,
            (false, Some(a)) => anchored(a, &mut rs, r, o, want)?,
        }
        out.push(rs);
    }
    Ok(out)
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize)]
pub struct Totals {
    pub rows: usize,
    pub owed: usize,
    pub blocked_rows: usize,
    pub blocked_bytes: usize,
}

#[must_use]
pub fn totals(rows: &[RowSite]) -> Totals {
    let mut t = Totals {
        rows: rows.len(),
        ..Totals::default()
    };
    for r in rows {
        if r.blocked() {
            t.blocked_rows = t.blocked_rows.saturating_add(1);
            t.blocked_bytes = t.blocked_bytes.saturating_add(r.differing);
        } else {
            t.owed = t.owed.saturating_add(r.differing);
        }
    }
    t
}

pub fn write_rows(rb: &[u8], ob: &[u8], rows: &[RowSite]) -> Result<(Vec<u8>, usize)> {
    let mut next = ob.to_vec();
    let mut written = 0usize;
    let writable: Vec<&RowSite> = rows.iter().filter(|x| x.state == State::Writable).collect();
    for row in &writable {
        let Some(s) = row.site else {
            continue;
        };
        let want = rb
            .get(row.off..row.off.saturating_add(row.len))
            .context("the reference row runs past the reference")?;
        for (k, &b) in want.iter().enumerate() {
            let slot = next
                .get_mut(s.saturating_add(k))
                .context("the row's site runs past the image")?;
            if *slot != b {
                *slot = b;
                written = written.saturating_add(1);
            }
        }
        if let Some((f, v)) = row.extra {
            for (k, b) in v.to_le_bytes().into_iter().enumerate() {
                let slot = next
                    .get_mut(f.saturating_add(k))
                    .context("the VirtualSize field runs past the image")?;
                if *slot != b {
                    *slot = b;
                    written = written.saturating_add(1);
                }
            }
        }
    }
    let allowed = |k: usize| {
        writable.iter().any(|x| {
            x.site
                .is_some_and(|s| (s..s.saturating_add(x.len)).contains(&k))
                || x.extra
                    .is_some_and(|(f, _)| (f..f.saturating_add(4)).contains(&k))
        })
    };
    let stray = (0..next.len())
        .filter(|&k| ob.get(k) != next.get(k) && !allowed(k))
        .count();
    if stray != 0 {
        bail!(
            "post-link: {stray} changed byte(s) outside the declared rows' sites; nothing written"
        );
    }
    Ok((next, written))
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Stepped {
    pub written: usize,
    pub rows_written: usize,
    pub passes: usize,
    pub rows: Vec<RowSite>,
}

pub fn step(rows: &[Row], r: &Image, ours: Vec<u8>) -> Result<(Vec<u8>, Stepped)> {
    let mut cur_img = Image::parse(ours)?;
    let mut cur = resolve(rows, r, &cur_img)?;
    let mut s = Stepped {
        written: 0,
        rows_written: 0,
        passes: 0,
        rows: Vec::new(),
    };
    for _ in 0..MAX_PASSES {
        let n = cur.iter().filter(|x| x.state == State::Writable).count();
        if n == 0 {
            break;
        }
        let (next, w) = write_rows(r.bytes(), cur_img.bytes(), &cur)?;
        s.written = s.written.saturating_add(w);
        s.rows_written = s.rows_written.saturating_add(n);
        s.passes = s.passes.saturating_add(1);
        cur_img = Image::parse(next)?;
        cur = resolve(rows, r, &cur_img)?;
    }
    s.rows = cur;
    Ok((cur_img.into_bytes(), s))
}
