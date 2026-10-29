#![allow(dead_code, clippy::unwrap_used)]

pub use msvc6_fixture::sample::*;

pub fn image(pe: &msvc6_fixture::pe::Pe) -> msvc6_image::pe::Image {
    msvc6_image::pe::Image::parse(pe.build()).unwrap()
}
