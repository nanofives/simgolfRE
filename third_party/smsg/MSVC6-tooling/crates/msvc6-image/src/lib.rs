pub mod anatomy;
pub mod archive;
pub mod be;
pub mod census;
pub mod clock;
pub mod cmp;
pub mod coff;
pub mod digest;
pub mod dump;
pub mod libdiff;
pub mod macho;
pub mod objcmp;
pub mod pe;
pub mod pef;
pub mod postlink;
pub mod res;
pub mod rich;
pub mod traceback;

#[must_use]
pub fn us(n: u32) -> usize {
    usize::try_from(n).unwrap_or(usize::MAX)
}

#[must_use]
pub fn u32_of(n: usize) -> u32 {
    u32::try_from(n).unwrap_or(u32::MAX)
}
