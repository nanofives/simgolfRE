use std::fmt;

macro_rules! newtype_u32 {
    ($name:ident) => {
        #[derive(
            Clone,
            Copy,
            PartialEq,
            Eq,
            PartialOrd,
            Ord,
            Hash,
            Default,
            serde::Serialize,
            serde::Deserialize,
        )]
        #[serde(transparent)]
        pub struct $name(pub u32);

        impl $name {
            #[must_use]
            pub const fn get(self) -> u32 {
                self.0
            }
        }

        impl fmt::Debug for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(f, "{}({:#x})", stringify!($name), self.0)
            }
        }

        impl fmt::Display for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(f, "{:#x}", self.0)
            }
        }

        impl fmt::LowerHex for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                fmt::LowerHex::fmt(&self.0, f)
            }
        }

        impl fmt::UpperHex for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                fmt::UpperHex::fmt(&self.0, f)
            }
        }
    };
}

newtype_u32!(Rva);
newtype_u32!(Va);
newtype_u32!(VirtSize);
newtype_u32!(RawSize);

#[derive(
    Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default, serde::Serialize, serde::Deserialize,
)]
#[serde(transparent)]
pub struct FileOffset(pub usize);

impl FileOffset {
    #[must_use]
    pub const fn get(self) -> usize {
        self.0
    }

    #[must_use]
    pub fn checked_add(self, n: usize) -> Option<Self> {
        self.0.checked_add(n).map(Self)
    }
}

impl fmt::Debug for FileOffset {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "FileOffset({:#x})", self.0)
    }
}

impl fmt::Display for FileOffset {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:#x}", self.0)
    }
}

impl Rva {
    #[must_use]
    pub fn checked_add(self, n: u32) -> Option<Self> {
        self.0.checked_add(n).map(Self)
    }

    #[must_use]
    pub fn delta_from(self, base: Rva) -> Option<u32> {
        self.0.checked_sub(base.0)
    }

    #[must_use]
    pub fn to_va(self, image_base: Va) -> Option<Va> {
        image_base.0.checked_add(self.0).map(Va)
    }
}

impl Va {
    #[must_use]
    pub fn to_rva(self, image_base: Va) -> Option<Rva> {
        self.0.checked_sub(image_base.0).map(Rva)
    }
}

#[must_use]
pub fn on_disk_len(virt: VirtSize, raw: RawSize) -> usize {
    usize::try_from(virt.0.min(raw.0)).unwrap_or(usize::MAX)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn on_disk_len_is_the_minimum_of_virtual_and_raw() {
        assert_eq!(on_disk_len(VirtSize(0x5000), RawSize(0x2000)), 0x2000);
        assert_eq!(on_disk_len(VirtSize(0x1234), RawSize(0x1400)), 0x1234);
    }

    #[test]
    fn a_va_below_the_image_base_is_no_rva() {
        let base = Va(0x0040_0000);
        assert_eq!(Va(0x0040_1000).to_rva(base), Some(Rva(0x1000)));
        assert_eq!(Va(0x0000_0010).to_rva(base), None);
        assert_eq!(Rva(0x1000).to_va(base), Some(Va(0x0040_1000)));
        assert_eq!(Rva(0xffff_f000).to_va(base), None);
    }

    #[test]
    fn deltas_and_offsets_refuse_to_wrap() {
        assert_eq!(Rva(0x2000).delta_from(Rva(0x1000)), Some(0x1000));
        assert_eq!(Rva(0x1000).delta_from(Rva(0x2000)), None);
        assert_eq!(Rva(u32::MAX).checked_add(1), None);
        assert_eq!(FileOffset(8).checked_add(8), Some(FileOffset(16)));
        assert_eq!(FileOffset(usize::MAX).checked_add(1), None);
    }

    #[test]
    fn the_spaces_print_as_themselves() {
        assert_eq!(format!("{:?}", Rva(0x1234)), "Rva(0x1234)");
        assert_eq!(format!("{}", Va(0x0040_0000)), "0x400000");
        assert_eq!(format!("{:08x}", Rva(0xab)), "000000ab");
        assert_eq!(format!("{:?}", FileOffset(0x400)), "FileOffset(0x400)");
    }

    #[test]
    fn a_unit_serialises_as_its_number() {
        assert_eq!(serde_json::to_string(&Rva(16)).unwrap(), "16");
        assert_eq!(
            serde_json::from_str::<Va>("4194304").unwrap(),
            Va(0x0040_0000)
        );
    }
}
