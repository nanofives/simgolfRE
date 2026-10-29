#![allow(dead_code)]

#[path = "src/buildcheck.rs"]
mod buildcheck;
#[path = "src/config.rs"]
mod config;
#[path = "src/error.rs"]
mod error;
#[path = "src/fmtcheck.rs"]
mod fmtcheck;
#[path = "src/lex.rs"]
mod lex;
#[path = "src/rules.rs"]
mod rules;
#[path = "src/sha256.rs"]
mod sha256;
#[path = "src/srcio.rs"]
mod srcio;
#[path = "src/tomlread.rs"]
mod tomlread;
#[path = "src/tree.rs"]
mod tree;

use error::{Error, Result};

fn main() {
    buildcheck::crate_main();
}
