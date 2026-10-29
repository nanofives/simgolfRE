use crate::cursor::{Cursor, Ids, Result, error};

pub const MAGIC: u8 = 0x11;
pub const TAGS: [u16; 2] = [0x0386, 0x2386];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Layout {
    Fixed { compact: bool, wide: bool },
    Flagged,
}

pub const STAMPS: [(u32, Layout); 10] = [
    (
        0x0130_937d,
        Layout::Fixed {
            compact: false,
            wide: false,
        },
    ),
    (
        0x0130_2051,
        Layout::Fixed {
            compact: false,
            wide: false,
        },
    ),
    (
        0x0130_9260,
        Layout::Fixed {
            compact: false,
            wide: true,
        },
    ),
    (
        0x0130_937e,
        Layout::Fixed {
            compact: false,
            wide: true,
        },
    ),
    (
        0x0130_45cb,
        Layout::Fixed {
            compact: true,
            wide: false,
        },
    ),
    (
        0x0130_937f,
        Layout::Fixed {
            compact: true,
            wide: false,
        },
    ),
    (
        0x0130_9261,
        Layout::Fixed {
            compact: true,
            wide: true,
        },
    ),
    (
        0x0130_9380,
        Layout::Fixed {
            compact: true,
            wide: true,
        },
    ),
    (0x0130_ba96, Layout::Flagged),
    (0x0131_09f2, Layout::Flagged),
];

pub const FLAG_COMPACT_IDS: u32 = 1;
pub const FLAG_WIDE: u32 = 2;
pub const FLAG_DLP: u32 = 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Header {
    pub magic: u8,
    pub tag: u16,
    pub stamp: u32,
    pub next_id: u32,
    pub lang: u8,
    pub flags: Option<u32>,
    pub version: Option<(i16, i16, i16)>,
    pub ids: Ids,
    pub wide: bool,
    pub end: usize,
}

impl Header {
    #[must_use]
    pub fn dlp(&self) -> bool {
        self.flags.is_some_and(|f| f & FLAG_DLP != 0)
    }

    #[must_use]
    pub fn build(&self) -> Option<u16> {
        self.version.and_then(|(_, _, b)| u16::try_from(b).ok())
    }

    #[must_use]
    pub fn front_end(&self) -> Option<String> {
        self.version
            .map(|(a, b, c)| format!("{a}.{b:02}.{}", u16::from_le_bytes(c.to_le_bytes())))
    }
}

#[must_use]
pub fn layout_of(stamp: u32) -> Option<Layout> {
    STAMPS.iter().find(|(s, _)| *s == stamp).map(|(_, l)| *l)
}

pub fn read(c: &mut Cursor<'_>) -> Result<Header> {
    let at = c.pos();
    let magic = c.byte()?;
    if magic != MAGIC {
        return Err(error(
            at,
            format!("first byte {magic:#04x}: no front end's gl stream (C2 wants {MAGIC:#04x})"),
        ));
    }
    let tag = c.u16()?;
    if !TAGS.contains(&tag) {
        return Err(error(
            at.saturating_add(1),
            format!("tag {tag:#06x}: C2 reads 0x0386 or 0x2386"),
        ));
    }
    let stamp_at = c.pos();
    let stamp = c.u32()?;
    let Some(layout) = layout_of(stamp) else {
        return Err(error(
            stamp_at,
            format!(
                "IL stamp {stamp:#010x} ({stamp}) is no version this reader knows (known: {})",
                STAMPS
                    .iter()
                    .map(|(s, _)| s.to_string())
                    .collect::<Vec<_>>()
                    .join(", ")
            ),
        ));
    };
    let next_id = c.u32()?;
    let lang = c.byte()?;
    let (compact, wide, flags, version) = match layout {
        Layout::Fixed { compact, wide } => (compact, wide, None, None),
        Layout::Flagged => {
            let f = c.u32()?;
            let v = (c.short()?, c.short()?, c.short()?);
            (
                f & FLAG_COMPACT_IDS != 0,
                f & FLAG_WIDE != 0,
                Some(f),
                Some(v),
            )
        }
    };
    Ok(Header {
        magic,
        tag,
        stamp,
        next_id,
        lang,
        flags,
        version,
        ids: if compact { Ids::Compact } else { Ids::Plain },
        wide,
        end: c.pos(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn gold() -> Vec<u8> {
        let mut b = vec![MAGIC, 0x86, 0x03];
        b.extend(0x0130_ba96u32.to_le_bytes());
        b.extend(0xdcu32.to_le_bytes());
        b.push(1);
        b.extend(2u32.to_le_bytes());
        b.extend([0x0c, 0x00, 0x80, 0xe8, 0x1f]);
        b
    }

    #[test]
    fn a_flagged_header_carries_its_flags_and_front_end_version() {
        let b = gold();
        let h = read(&mut Cursor::new(&b, 0)).unwrap();
        assert_eq!(h.next_id, 0xdc);
        assert_eq!(h.lang, 1);
        assert!(h.wide);
        assert_eq!(h.ids, Ids::Plain);
        assert_eq!(h.front_end().as_deref(), Some("12.00.8168"));
        assert_eq!(h.build(), Some(8168));
        assert_eq!(h.end, b.len());
    }

    #[test]
    fn an_older_stamp_fixes_the_modes_and_an_unknown_one_is_refused() {
        let mut b = vec![MAGIC, 0x86, 0x23];
        b.extend(0x0130_9261u32.to_le_bytes());
        b.extend(0x1cu32.to_le_bytes());
        b.push(0);
        let h = read(&mut Cursor::new(&b, 0)).unwrap();
        assert_eq!(h.ids, Ids::Compact);
        assert!(h.wide);
        assert!(h.version.is_none());
        let mut u = gold();
        u[3] = 0x97;
        let e = read(&mut Cursor::new(&u, 0)).unwrap_err();
        assert_eq!(e.at, 3);
        assert!(read(&mut Cursor::new(&[0x12], 0)).is_err());
        assert!(read(&mut Cursor::new(&[MAGIC, 0x86, 0x04], 0)).is_err());
    }
}
