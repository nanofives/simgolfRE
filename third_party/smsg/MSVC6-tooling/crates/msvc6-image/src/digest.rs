use msvc6_base::digest::md5_hex;
use serde::Serialize;
use std::collections::BTreeMap;

use crate::{u32_of, us};

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Function {
    pub name: String,
    pub bytes: Vec<u8>,
    pub relocs: Vec<u32>,
}

pub fn feed(out: &mut Vec<u8>, name: &str, body: &[u8], relocs: &[u32]) {
    out.extend_from_slice(name.as_bytes());
    out.push(0);
    out.extend_from_slice(&u32_of(body.len()).to_le_bytes());
    let mut b = body.to_vec();
    for at in relocs {
        let o = us(*at);
        if let Some(s) = b.get_mut(o..o.saturating_add(4)) {
            s.fill(0);
        }
    }
    out.extend_from_slice(&b);
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CodeDigest {
    pub md5: String,
    pub functions: usize,
    pub bytes: usize,
}

#[must_use]
pub fn code_digest(functions: &[Function]) -> Option<CodeDigest> {
    if functions.is_empty() {
        return None;
    }
    let by_name: BTreeMap<&str, &Function> =
        functions.iter().map(|f| (f.name.as_str(), f)).collect();
    let mut stream = Vec::new();
    for (name, f) in &by_name {
        feed(&mut stream, name, &f.bytes, &f.relocs);
    }
    Some(CodeDigest {
        md5: md5_hex(&stream),
        functions: by_name.len(),
        bytes: stream.len(),
    })
}

#[must_use]
pub fn first_disagreement<'a>(a: &'a [Function], b: &[Function]) -> Option<&'a str> {
    let theirs: BTreeMap<&str, &Function> = b.iter().map(|f| (f.name.as_str(), f)).collect();
    let mut ours: Vec<&Function> = a.iter().collect();
    ours.sort_by(|x, y| x.name.cmp(&y.name));
    ours.into_iter()
        .find(|f| {
            theirs.get(f.name.as_str()).is_none_or(|g| {
                let (mut x, mut y) = (Vec::new(), Vec::new());
                feed(&mut x, &f.name, &f.bytes, &f.relocs);
                feed(&mut y, &g.name, &g.bytes, &f.relocs);
                x != y
            })
        })
        .map(|f| f.name.as_str())
}
