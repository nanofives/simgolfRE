use crate::buf::{Buf, align_up, n16, n32, us};

pub const MACHINE_I386: u16 = 0x14c;
pub const SCN_CODE: u32 = 0x6000_0020;
pub const SCN_RDATA: u32 = 0x4000_0040;
pub const SCN_DATA: u32 = 0xc000_0040;
pub const SCN_BSS: u32 = 0xc000_0080;
pub const SCN_RELOC: u32 = 0x4200_0040;
pub const SCN_RSRC: u32 = 0x4000_0040;

pub const DIR_EXPORT: usize = 0;
pub const DIR_IMPORT: usize = 1;
pub const DIR_RESOURCE: usize = 2;
pub const DIR_BASERELOC: usize = 5;
pub const DIR_DEBUG: usize = 6;
pub const DIR_IAT: usize = 12;

pub const DOS_STUB: [u8; 64] = [
    0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd, 0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, b'T', b'h',
    b'i', b's', b' ', b'p', b'r', b'o', b'g', b'r', b'a', b'm', b' ', b'c', b'a', b'n', b'n', b'o',
    b't', b' ', b'b', b'e', b' ', b'r', b'u', b'n', b' ', b'i', b'n', b' ', b'D', b'O', b'S', b' ',
    b'm', b'o', b'd', b'e', b'.', b'\r', b'\r', b'\n', b'$', 0, 0, 0, 0, 0, 0, 0,
];

pub const RICH_AT: usize = 0x80;

const OPT_SIZE: u16 = 0xe0;
const DANS: u32 = 0x536e_6144;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Section {
    pub name: String,
    pub characteristics: u32,
    pub data: Vec<u8>,
    pub virt_size: Option<u32>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Pe {
    pub machine: u16,
    pub timestamp: u32,
    pub characteristics: u16,
    pub linker: (u8, u8),
    pub image_base: u32,
    pub entry: u32,
    pub section_alignment: u32,
    pub file_alignment: u32,
    pub os_version: (u16, u16),
    pub image_version: (u16, u16),
    pub subsystem_version: (u16, u16),
    pub subsystem: u16,
    pub dll_characteristics: u16,
    pub stack: (u32, u32),
    pub heap: (u32, u32),
    pub checksum: u32,
    pub rich: Vec<(u32, u32)>,
    pub rich_key: Option<u32>,
    pub sections: Vec<Section>,
    pub dirs: [(u32, u32); 16],
    pub header_slack: Vec<u8>,
    pub overlay: Vec<u8>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Placed {
    pub rva: u32,
    pub virt_size: u32,
    pub raw_off: u32,
    pub raw_size: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Layout {
    pub e_lfanew: u32,
    pub headers_end: u32,
    pub size_of_headers: u32,
    pub sections: Vec<Placed>,
    pub size_of_image: u32,
}

#[must_use]
pub fn comp_id(prodid: u16, build: u16) -> u32 {
    (u32::from(prodid) << 16) | u32::from(build)
}

#[must_use]
pub fn rich_checksum(head: &[u8], stamps: &[(u32, u32)]) -> u32 {
    let mut c = n32(head.len());
    for (i, b) in head.iter().enumerate() {
        if (0x3c..0x40).contains(&i) {
            continue;
        }
        c = c.wrapping_add(u32::from(*b).rotate_left(n32(i)));
    }
    for (id, count) in stamps {
        c = c.wrapping_add(id.rotate_left(*count));
    }
    c
}

#[must_use]
pub fn rich_block(key: u32, stamps: &[(u32, u32)]) -> Vec<u8> {
    let mut b = Buf::new();
    b.u32(DANS ^ key).u32(key).u32(key).u32(key);
    for (id, count) in stamps {
        b.u32(id ^ key).u32(count ^ key);
    }
    b.bytes(b"Rich").u32(key);
    b.align(8);
    b.into_vec()
}

fn dos_header(e_lfanew: u32) -> Buf {
    let mut b = Buf::new();
    b.bytes(b"MZ")
        .u16(0x90)
        .u16(3)
        .u16(0)
        .u16(4)
        .u16(0)
        .u16(0xffff)
        .u16(0)
        .u16(0xb8)
        .u16(0)
        .u16(0)
        .u16(0)
        .u16(0x40)
        .u16(0);
    b.pad_to(0x3c);
    b.u32(e_lfanew);
    b.bytes(&DOS_STUB);
    b
}

impl Pe {
    #[must_use]
    pub fn exe() -> Self {
        Self {
            machine: MACHINE_I386,
            timestamp: 0x3c00_0000,
            characteristics: 0x010f,
            linker: (6, 0),
            image_base: 0x0040_0000,
            entry: 0,
            section_alignment: 0x1000,
            file_alignment: 0x200,
            os_version: (4, 0),
            image_version: (0, 0),
            subsystem_version: (4, 0),
            subsystem: 2,
            dll_characteristics: 0,
            stack: (0x0010_0000, 0x1000),
            heap: (0x0010_0000, 0x1000),
            checksum: 0,
            rich: Vec::new(),
            rich_key: None,
            sections: Vec::new(),
            dirs: [(0, 0); 16],
            header_slack: Vec::new(),
            overlay: Vec::new(),
        }
    }

    #[must_use]
    pub fn dll() -> Self {
        Self {
            characteristics: 0x210e,
            image_base: 0x1000_0000,
            ..Self::exe()
        }
    }

    fn rich_len(&self) -> usize {
        if self.rich.is_empty() {
            0
        } else {
            rich_block(0, &self.rich).len()
        }
    }

    fn e_lfanew(&self) -> u32 {
        n32(RICH_AT.saturating_add(self.rich_len()))
    }

    fn headers_end(&self, nsec: usize) -> u32 {
        self.e_lfanew()
            .saturating_add(24)
            .saturating_add(u32::from(OPT_SIZE))
            .saturating_add(n32(nsec.saturating_mul(40)))
            .saturating_add(n32(self.header_slack.len()))
    }

    fn first_rva(&self, nsec: usize) -> u32 {
        align_up(
            align_up(self.headers_end(nsec), self.file_alignment).max(1),
            self.section_alignment,
        )
    }

    fn span(s: &Section) -> u32 {
        s.virt_size.unwrap_or(0).max(n32(s.data.len())).max(1)
    }

    #[must_use]
    pub fn next_rva(&self) -> u32 {
        let n = self.sections.len().saturating_add(1);
        let mut rva = self.first_rva(n);
        for s in &self.sections {
            rva = align_up(rva.saturating_add(Self::span(s)), self.section_alignment);
        }
        rva
    }

    pub fn push(&mut self, name: &str, characteristics: u32, data: Vec<u8>) -> u32 {
        let rva = self.next_rva();
        self.sections.push(Section {
            name: name.to_string(),
            characteristics,
            data,
            virt_size: None,
        });
        rva
    }

    pub fn push_virtual(
        &mut self,
        name: &str,
        characteristics: u32,
        data: Vec<u8>,
        virt_size: u32,
    ) -> u32 {
        let rva = self.next_rva();
        self.sections.push(Section {
            name: name.to_string(),
            characteristics,
            data,
            virt_size: Some(virt_size),
        });
        rva
    }

    pub fn dir(&mut self, which: usize, rva: u32, size: u32) {
        if let Some(d) = self.dirs.get_mut(which) {
            *d = (rva, size);
        }
    }

    #[must_use]
    pub fn layout(&self) -> Layout {
        let n = self.sections.len();
        let headers_end = self.headers_end(n);
        let size_of_headers = align_up(headers_end, self.file_alignment);
        let mut rva = self.first_rva(n);
        let mut raw = size_of_headers;
        let mut placed = Vec::with_capacity(n);
        for s in &self.sections {
            let raw_size = align_up(n32(s.data.len()), self.file_alignment);
            let raw_off = if raw_size == 0 { 0 } else { raw };
            placed.push(Placed {
                rva,
                virt_size: s.virt_size.unwrap_or(n32(s.data.len())),
                raw_off,
                raw_size,
            });
            raw = raw.saturating_add(raw_size);
            rva = align_up(rva.saturating_add(Self::span(s)), self.section_alignment);
        }
        let size_of_image = if n == 0 {
            align_up(size_of_headers, self.section_alignment)
        } else {
            rva
        };
        Layout {
            e_lfanew: self.e_lfanew(),
            headers_end,
            size_of_headers,
            sections: placed,
            size_of_image,
        }
    }

    fn rich_bytes(&self) -> Vec<u8> {
        if self.rich.is_empty() {
            return Vec::new();
        }
        let head = dos_header(0).into_vec();
        let key = self
            .rich_key
            .unwrap_or_else(|| rich_checksum(&head, &self.rich));
        rich_block(key, &self.rich)
    }

    fn sum_raw(&self, l: &Layout, want: u32) -> u32 {
        self.sections
            .iter()
            .zip(&l.sections)
            .filter(|(s, _)| s.characteristics & want != 0)
            .map(|(_, p)| p.raw_size)
            .fold(0, u32::saturating_add)
    }

    fn first_rva_with(&self, l: &Layout, keep: impl Fn(u32) -> bool) -> u32 {
        self.sections
            .iter()
            .zip(&l.sections)
            .find(|(s, _)| keep(s.characteristics))
            .map_or(0, |(_, p)| p.rva)
    }

    #[must_use]
    pub fn rva_to_file(&self, rva: u32) -> Option<u32> {
        let l = self.layout();
        l.sections.iter().find_map(|p| {
            let d = rva.checked_sub(p.rva)?;
            (d < p.raw_size).then(|| p.raw_off.saturating_add(d))
        })
    }

    #[must_use]
    pub fn build(&self) -> Vec<u8> {
        let l = self.layout();
        let mut b = dos_header(l.e_lfanew);
        b.bytes(&self.rich_bytes());
        b.pad_to(us(l.e_lfanew));
        b.bytes(b"PE\0\0")
            .u16(self.machine)
            .u16(n16(self.sections.len()))
            .u32(self.timestamp)
            .u32(0)
            .u32(0)
            .u16(OPT_SIZE)
            .u16(self.characteristics);
        let uninit = self
            .sections
            .iter()
            .zip(&l.sections)
            .filter(|(s, _)| s.characteristics & 0x80 != 0)
            .map(|(_, p)| p.virt_size)
            .fold(0, u32::saturating_add);
        b.u16(0x10b)
            .u8(self.linker.0)
            .u8(self.linker.1)
            .u32(self.sum_raw(&l, 0x20))
            .u32(self.sum_raw(&l, 0x40))
            .u32(uninit)
            .u32(self.entry)
            .u32(self.first_rva_with(&l, |c| c & 0x20 != 0))
            .u32(self.first_rva_with(&l, |c| c & 0x20 == 0))
            .u32(self.image_base)
            .u32(self.section_alignment)
            .u32(self.file_alignment)
            .u16(self.os_version.0)
            .u16(self.os_version.1)
            .u16(self.image_version.0)
            .u16(self.image_version.1)
            .u16(self.subsystem_version.0)
            .u16(self.subsystem_version.1)
            .u32(0)
            .u32(l.size_of_image)
            .u32(l.size_of_headers)
            .u32(self.checksum)
            .u16(self.subsystem)
            .u16(self.dll_characteristics)
            .u32(self.stack.0)
            .u32(self.stack.1)
            .u32(self.heap.0)
            .u32(self.heap.1)
            .u32(0)
            .u32(16);
        for (rva, size) in self.dirs {
            b.u32(rva).u32(size);
        }
        for (s, p) in self.sections.iter().zip(&l.sections) {
            let mut name = [0u8; 8];
            for (slot, c) in name.iter_mut().zip(s.name.bytes()) {
                *slot = c;
            }
            b.bytes(&name)
                .u32(p.virt_size)
                .u32(p.rva)
                .u32(p.raw_size)
                .u32(p.raw_off)
                .u32(0)
                .u32(0)
                .u16(0)
                .u16(0)
                .u32(s.characteristics);
        }
        b.bytes(&self.header_slack);
        b.pad_to(us(l.size_of_headers));
        for (s, p) in self.sections.iter().zip(&l.sections) {
            if p.raw_size == 0 {
                continue;
            }
            b.pad_to(us(p.raw_off));
            b.bytes(&s.data);
            b.pad_to(us(p.raw_off.saturating_add(p.raw_size)));
        }
        b.bytes(&self.overlay);
        self.fix_debug(&mut b);
        b.into_vec()
    }

    fn fix_debug(&self, b: &mut Buf) {
        let (rva, size) = self.dirs.get(DIR_DEBUG).copied().unwrap_or((0, 0));
        let Some(at) = self.rva_to_file(rva) else {
            return;
        };
        for k in 0..us(size / 28) {
            let e = us(at).saturating_add(k.saturating_mul(28));
            let read = |o: usize| {
                b.as_slice()
                    .get(o..o.saturating_add(4))
                    .and_then(|s| <[u8; 4]>::try_from(s).ok())
                    .map_or(0, u32::from_le_bytes)
            };
            let (data_rva, ptr) = (read(e.saturating_add(20)), read(e.saturating_add(24)));
            if ptr == 0
                && data_rva != 0
                && let Some(off) = self.rva_to_file(data_rva)
            {
                b.put_u32(e.saturating_add(24), off);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sections_are_placed_on_their_alignments() {
        let mut pe = Pe::dll();
        let a = pe.push(".text", SCN_CODE, vec![0xc3; 0x1234]);
        let b = pe.push(".rdata", SCN_RDATA, vec![1; 10]);
        let c = pe.push_virtual(".data", SCN_DATA, vec![2; 0x300], 0x5000);
        assert_eq!((a, b, c), (0x1000, 0x3000, 0x4000));
        let l = pe.layout();
        assert_eq!(l.size_of_headers, 0x200);
        assert_eq!(l.sections[0].raw_off, 0x200);
        assert_eq!(l.sections[0].raw_size, 0x1400);
        assert_eq!(l.sections[1].raw_off, 0x1600);
        assert_eq!(l.sections[2].virt_size, 0x5000);
        assert_eq!(l.size_of_image, 0x9000);
        let img = pe.build();
        assert_eq!(&img[..2], b"MZ");
        assert_eq!(img.len(), 0x1c00);
        assert_eq!(&img[0x80..0x84], b"PE\0\0");
        assert_eq!(img[0x200], 0xc3);
    }

    #[test]
    fn a_rich_block_moves_the_pe_header_and_carries_its_checksum() {
        let mut pe = Pe::exe();
        pe.rich = vec![(comp_id(0x0b, 8168), 31), (comp_id(4, 8168), 1)];
        let img = pe.build();
        assert_eq!(
            &img[0x80..0x84],
            &(DANS ^ rich_checksum(&img[..0x80], &pe.rich)).to_le_bytes()
        );
        let lfanew = u32::from_le_bytes(img[0x3c..0x40].try_into().unwrap());
        assert_eq!(lfanew, 0x80 + 16 + 16 + 8);
        assert_eq!(&img[us(lfanew)..us(lfanew) + 4], b"PE\0\0");
    }
}
