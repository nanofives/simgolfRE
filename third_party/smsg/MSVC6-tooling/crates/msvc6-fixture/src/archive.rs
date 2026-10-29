use crate::buf::{Buf, n32};

pub const MAGIC: &[u8; 8] = b"!<arch>\n";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Member {
    pub name: String,
    pub data: Vec<u8>,
    pub symbols: Vec<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Library {
    pub members: Vec<Member>,
    pub date: u32,
}

#[must_use]
pub fn short_import(dll: &str, symbol: &str, hint: u16, kind: u16, name_type: u16) -> Vec<u8> {
    let mut tail = Buf::new();
    tail.cstr(symbol).cstr(dll);
    let mut b = Buf::new();
    b.u16(0)
        .u16(0xffff)
        .u16(0)
        .u16(0x14c)
        .u32(0x3c00_0000)
        .u32(n32(tail.len()))
        .u16(hint)
        .u16((kind & 3) | ((name_type & 7) << 2));
    b.bytes(tail.as_slice());
    b.into_vec()
}

fn header(b: &mut Buf, name: &str, date: u32, size: usize) {
    let field = |s: &str, w: usize| format!("{s:<w$}");
    b.bytes(field(name, 16).as_bytes())
        .bytes(field(&date.to_string(), 12).as_bytes())
        .bytes(field("", 6).as_bytes())
        .bytes(field("", 6).as_bytes())
        .bytes(field("0", 8).as_bytes())
        .bytes(field(&size.to_string(), 10).as_bytes())
        .bytes(b"`\n");
}

fn padded(n: usize) -> usize {
    n.saturating_add(n % 2)
}

impl Library {
    pub fn member(&mut self, name: &str, data: Vec<u8>, symbols: &[&str]) {
        self.members.push(Member {
            name: name.to_string(),
            data,
            symbols: symbols.iter().map(ToString::to_string).collect(),
        });
    }

    #[must_use]
    pub fn build(&self) -> Vec<u8> {
        let symbols: Vec<(&str, usize)> = self
            .members
            .iter()
            .enumerate()
            .flat_map(|(i, m)| m.symbols.iter().map(move |s| (s.as_str(), i)))
            .collect();
        let names_len: usize = symbols.iter().map(|(s, _)| s.len().saturating_add(1)).sum();
        let first_len = 4usize
            .saturating_add(symbols.len().saturating_mul(4))
            .saturating_add(names_len);
        let second_len = 4usize
            .saturating_add(self.members.len().saturating_mul(4))
            .saturating_add(4)
            .saturating_add(symbols.len().saturating_mul(2))
            .saturating_add(names_len);
        let mut longnames = Buf::new();
        let mut header_names = Vec::new();
        for m in &self.members {
            if m.name.len() > 15 {
                header_names.push(format!("/{}", longnames.len()));
                longnames.cstr(&m.name);
            } else {
                header_names.push(format!("{}/", m.name));
            }
        }
        let mut at = MAGIC
            .len()
            .saturating_add(60)
            .saturating_add(padded(first_len))
            .saturating_add(60)
            .saturating_add(padded(second_len));
        if !longnames.is_empty() {
            at = at
                .saturating_add(60)
                .saturating_add(padded(longnames.len()));
        }
        let mut offsets = Vec::new();
        for m in &self.members {
            offsets.push(n32(at));
            at = at.saturating_add(60).saturating_add(padded(m.data.len()));
        }
        let offset = |i: usize| offsets.get(i).copied().unwrap_or(0);
        let mut b = Buf::new();
        b.bytes(MAGIC);
        header(&mut b, "/", self.date, first_len);
        b.u32be(n32(symbols.len()));
        for (_, i) in &symbols {
            b.u32be(offset(*i));
        }
        for (s, _) in &symbols {
            b.cstr(s);
        }
        b.align(2);
        let mut sorted = symbols.clone();
        sorted.sort_by(|a, c| a.0.as_bytes().cmp(c.0.as_bytes()));
        header(&mut b, "/", self.date, second_len);
        b.u32(n32(self.members.len()));
        for o in &offsets {
            b.u32(*o);
        }
        b.u32(n32(sorted.len()));
        for (_, i) in &sorted {
            b.u16(u16::try_from(i.saturating_add(1)).unwrap_or(u16::MAX));
        }
        for (s, _) in &sorted {
            b.cstr(s);
        }
        b.align(2);
        if !longnames.is_empty() {
            header(&mut b, "//", self.date, longnames.len());
            b.bytes(longnames.as_slice());
            b.align(2);
        }
        for (m, h) in self.members.iter().zip(&header_names) {
            header(&mut b, h, self.date, m.data.len());
            b.bytes(&m.data);
            if m.data.len() % 2 == 1 {
                b.u8(b'\n');
            }
        }
        b.into_vec()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn members_start_where_the_first_linker_member_says() {
        let mut l = Library::default();
        l.member("a.obj", vec![1, 2, 3], &["_a"]);
        l.member("a_member_name_over_fifteen.obj", vec![4, 5], &["_b", "_c"]);
        let b = l.build();
        assert_eq!(&b[..8], MAGIC);
        assert_eq!(&b[8..9], b"/");
        let n = u32::from_be_bytes(b[68..72].try_into().unwrap());
        assert_eq!(n, 3);
        let first = usize::try_from(u32::from_be_bytes(b[72..76].try_into().unwrap())).unwrap();
        assert_eq!(&b[first..first + 6], b"a.obj/");
        let second = usize::try_from(u32::from_be_bytes(b[76..80].try_into().unwrap())).unwrap();
        assert_eq!(&b[second..second + 2], b"/0");
        assert_eq!(&b[first + 60..first + 63], &[1, 2, 3]);
        assert_eq!(b[first + 63], b'\n');
    }

    #[test]
    fn a_short_import_carries_its_name_type_and_both_names() {
        let s = short_import("USER32.dll", "_MessageBoxA@16", 7, 0, 3);
        assert_eq!(&s[..4], &[0, 0, 0xff, 0xff]);
        assert_eq!(u16::from_le_bytes(s[18..20].try_into().unwrap()), 3 << 2);
        assert_eq!(&s[20..], b"_MessageBoxA@16\0USER32.dll\0");
    }
}
