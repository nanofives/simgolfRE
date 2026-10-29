use std::collections::BTreeSet;

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Keys {
    own: BTreeSet<String>,
}

fn leaf(p: &str) -> &str {
    p.rsplit(['\\', '/']).next().unwrap_or(p)
}

impl Keys {
    pub fn new<I, S>(own: I) -> Self
    where
        I: IntoIterator<Item = S>,
        S: AsRef<str>,
    {
        Self {
            own: own
                .into_iter()
                .map(|s| {
                    let l = leaf(s.as_ref()).to_ascii_lowercase();
                    l.strip_suffix(".lib").unwrap_or(&l).to_string()
                })
                .filter(|s| !s.is_empty())
                .collect(),
        }
    }

    #[must_use]
    pub fn own(&self) -> &BTreeSet<String> {
        &self.own
    }

    #[must_use]
    pub fn map_owner<'a>(&self, tok: &'a str) -> &'a str {
        match tok.split_once(':') {
            Some((lib, member)) if self.own.contains(&lib.to_ascii_lowercase()) => member,
            _ => tok,
        }
    }

    #[must_use]
    pub fn log_owner<'a>(&self, tok: &'a str) -> &'a str {
        let Some((lib, rest)) = tok.split_once('(') else {
            return tok;
        };
        let stem = leaf(lib).to_ascii_lowercase();
        match (stem.strip_suffix(".lib"), rest.strip_suffix(')')) {
            (Some(s), Some(member)) if self.own.contains(s) => member,
            _ => tok,
        }
    }

    #[must_use]
    pub fn object_key<'a>(&self, tok: &'a str) -> &'a str {
        let m = self.map_owner(tok);
        if m.len() != tok.len() {
            return m;
        }
        self.log_owner(tok)
    }

    #[must_use]
    pub fn is_own_or_object(&self, tok: &str) -> bool {
        !tok.contains('(') || self.log_owner(tok).len() != tok.len()
    }
}

#[must_use]
pub fn obj_key(s: &str) -> String {
    let s = s.trim().to_ascii_lowercase();
    if let Some((lib, rest)) = s.split_once('(')
        && let Some(member) = rest.strip_suffix(')')
    {
        let lib = leaf(lib);
        let stem = lib.strip_suffix(".lib").unwrap_or(lib);
        return format!("{stem}:{}", leaf(member));
    }
    if let Some((lib, member)) = s.split_once(':') {
        return format!("{lib}:{}", leaf(member));
    }
    leaf(&s).to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_own_librarys_member_reads_as_the_member_in_either_spelling() {
        let k = Keys::new(["engine.lib"]);
        assert_eq!(k.object_key("engine:Arena.obj"), "Arena.obj");
        assert_eq!(k.object_key("C:\\lk\\engine.lib(Arena.obj)"), "Arena.obj");
        assert_eq!(
            k.object_key("libcmt.lib(output.obj)"),
            "libcmt.lib(output.obj)"
        );
        assert_eq!(k.object_key("LIBCMT:output.obj"), "LIBCMT:output.obj");
        assert_eq!(k.object_key("Plot.obj"), "Plot.obj");
        assert!(k.is_own_or_object("engine.lib(Arena.obj)"));
        assert!(!k.is_own_or_object("libcmt.lib(output.obj)"));
    }

    #[test]
    fn both_object_spellings_read_as_one_key() {
        assert_eq!(obj_key("libcmt:new.obj"), "libcmt:new.obj");
        assert_eq!(obj_key("libcmt.lib(new.obj)"), "libcmt:new.obj");
        assert_eq!(
            obj_key("C:\\lk\\Release\\engine.lib(Arena.obj)"),
            "engine:arena.obj"
        );
        assert_eq!(obj_key("engine:Arena.obj"), "engine:arena.obj");
        assert_eq!(obj_key("vendor.lib(TERM.OBJ)"), obj_key("vendor:TERM.OBJ"));
        assert_eq!(obj_key("Plot.obj"), "plot.obj");
    }
}
