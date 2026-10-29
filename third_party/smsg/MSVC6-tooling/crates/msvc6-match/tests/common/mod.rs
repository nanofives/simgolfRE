#![allow(dead_code, clippy::unwrap_used)]

pub use msvc6_fixture::placement::*;
use msvc6_match::copies::Offered;
use std::path::PathBuf;

pub fn parsed(text: Vec<u8>) -> msvc6_image::pe::Image {
    msvc6_image::pe::Image::parse(image(text)).unwrap()
}

pub fn offered(label: &str, bytes: Vec<u8>) -> Offered {
    Offered {
        label: label.to_string(),
        path: PathBuf::from(label),
        bytes,
    }
}

pub fn filler(n: usize) -> Vec<u8> {
    vec![0xcc; n]
}
