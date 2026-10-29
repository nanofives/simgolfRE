use crate::comdat;
use crate::env::Env;
use crate::score::{self, Plan, Score};
use anyhow::{Context as _, Result, bail};
use std::path::Path;

pub const AUTO: u32 = 0;

pub trait ByteOracle {
    fn score(&self, object: &[u8]) -> Result<Score>;

    fn score_file(&self, path: &Path) -> Result<Score> {
        let d = comdat::read(path)?;
        self.score(&d)
            .with_context(|| format!("scoring {}", path.display()))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Members {
    Predict,
    Asserted(Vec<u32>),
    Auto(Vec<String>),
}

#[derive(Debug)]
pub struct Try<'a> {
    pub env: &'a Env,
    pub rva: u32,
    pub members: Members,
}

impl<'a> Try<'a> {
    #[must_use]
    pub fn new(env: &'a Env, rva: u32, members: Members) -> Self {
        Self { env, rva, members }
    }
}

impl ByteOracle for Try<'_> {
    fn score(&self, object: &[u8]) -> Result<Score> {
        let comdats = comdat::text_of(object)?;
        if comdats.is_empty() {
            if comdat::has_code(object)? {
                bail!(
                    "the object has code and no `.text` section: without /Gy each function is not its own section"
                );
            }
            bail!("the object has no .text COMDAT: nothing to compare");
        }
        let (rva, plan, anchor) = match &self.members {
            Members::Auto(scopes) => {
                let scopes: Vec<&str> = scopes.iter().map(String::as_str).collect();
                let hint = (self.rva != AUTO).then_some(self.rva);
                let (ms, a) = score::derive(self.env, object, &comdats, &scopes, hint)?;
                (a.root, Plan::Asserted(ms), Some(a))
            }
            Members::Asserted(ms) if !ms.is_empty() => {
                if ms.first() == Some(&self.rva) {
                    bail!(
                        "--members lists COMDATs 1..n and {:#x} is COMDAT[0], already named by the RVA; drop it, or every later member seats one slot too low",
                        self.rva
                    );
                }
                (self.rva, Plan::Asserted(ms.clone()), None)
            }
            Members::Asserted(_) | Members::Predict => (self.rva, Plan::Predict, None),
        };
        if rva == AUTO {
            bail!("the RVA `auto` derives the anchor from the object and needs --auto-members");
        }
        let mut s = score::seat_and_score(self.env, object, &comdats, rva, &plan)?;
        s.anchor = anchor;
        Ok(s)
    }
}
