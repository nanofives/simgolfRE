use std::fmt::Write as _;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Key {
    Class,
    Struct,
    Union,
    Enum,
}

impl Key {
    pub fn word(self) -> &'static str {
        match self {
            Self::Class => "class",
            Self::Struct => "struct",
            Self::Union => "union",
            Self::Enum => "enum",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Ty {
    Prim(String),
    Udt {
        key: Option<Key>,
        name: String,
    },
    Ptr {
        to: Box<Ty>,
        reference: bool,
        konst: bool,
        volat: bool,
    },
    Cv {
        to: Box<Ty>,
        konst: bool,
        volat: bool,
    },
    Array {
        elem: Box<Ty>,
        count: Option<u64>,
    },
    Func {
        ret: Box<Ty>,
        cc: Option<String>,
        params: Vec<Ty>,
        varargs: bool,
    },
    Bits {
        ty: Box<Ty>,
        width: u8,
    },
    Unknown(String),
}

impl Ty {
    pub fn prim(s: &str) -> Self {
        Self::Prim(s.to_string())
    }

    pub fn udt(key: Option<Key>, name: &str) -> Self {
        Self::Udt {
            key,
            name: name.to_string(),
        }
    }

    pub fn ptr(to: Ty) -> Self {
        Self::Ptr {
            to: Box::new(to),
            reference: false,
            konst: false,
            volat: false,
        }
    }

    pub fn keyless(&self) -> Self {
        self.map_udt(&|_, n| Ty::udt(None, n))
    }

    pub fn map_udt(&self, f: &dyn Fn(Option<Key>, &str) -> Ty) -> Self {
        match self {
            Self::Udt { key, name } => f(*key, name),
            Self::Ptr {
                to,
                reference,
                konst,
                volat,
            } => Self::Ptr {
                to: Box::new(to.map_udt(f)),
                reference: *reference,
                konst: *konst,
                volat: *volat,
            },
            Self::Cv { to, konst, volat } => Self::Cv {
                to: Box::new(to.map_udt(f)),
                konst: *konst,
                volat: *volat,
            },
            Self::Array { elem, count } => Self::Array {
                elem: Box::new(elem.map_udt(f)),
                count: *count,
            },
            Self::Func {
                ret,
                cc,
                params,
                varargs,
            } => Self::Func {
                ret: Box::new(ret.map_udt(f)),
                cc: cc.clone(),
                params: params.iter().map(|p| p.map_udt(f)).collect(),
                varargs: *varargs,
            },
            Self::Bits { ty, width } => Self::Bits {
                ty: Box::new(ty.map_udt(f)),
                width: *width,
            },
            other => other.clone(),
        }
    }

    pub fn names(&self, out: &mut Vec<(Option<Key>, String)>) {
        match self {
            Self::Udt { key, name } => out.push((*key, name.clone())),
            Self::Ptr { to, .. } | Self::Cv { to, .. } => to.names(out),
            Self::Array { elem, .. } => elem.names(out),
            Self::Func { ret, params, .. } => {
                ret.names(out);
                params.iter().for_each(|p| p.names(out));
            }
            Self::Bits { ty, .. } => ty.names(out),
            _ => {}
        }
    }

    pub fn param_decay(&self) -> Self {
        match self {
            Self::Cv {
                to, volat: false, ..
            } => to.param_decay(),
            Self::Array { elem, .. } => Ty::ptr((**elem).clone()),
            Self::Ptr {
                to,
                reference,
                volat,
                ..
            } => Self::Ptr {
                to: to.clone(),
                reference: *reference,
                konst: false,
                volat: *volat,
            },
            other => other.clone(),
        }
    }

    pub fn decl(&self, name: &str) -> String {
        let (base, d) = self.split(name.to_string(), false);
        if d.is_empty() {
            base
        } else {
            format!("{base} {d}")
        }
    }

    pub fn spell(&self) -> String {
        self.decl("")
    }

    fn split(&self, inner: String, after_ptr: bool) -> (String, String) {
        match self {
            Self::Prim(s) => (s.clone(), inner),
            Self::Udt { key, name } => (
                match key {
                    Some(k) => format!("{} {name}", k.word()),
                    None => name.clone(),
                },
                inner,
            ),
            Self::Unknown(s) => (s.clone(), inner),
            Self::Cv { to, konst, volat } => {
                let q = qual(*konst, *volat);
                match &**to {
                    Self::Prim(_) | Self::Udt { .. } | Self::Unknown(_) => {
                        let (b, d) = to.split(inner, after_ptr);
                        (format!("{q} {b}"), d)
                    }
                    _ => {
                        let d = if inner.is_empty() {
                            q.to_string()
                        } else {
                            format!("{q} {inner}")
                        };
                        to.split(d, after_ptr)
                    }
                }
            }
            Self::Ptr {
                to,
                reference,
                konst,
                volat,
            } => {
                let mut d = String::from(if *reference { "&" } else { "*" });
                let q = qual(*konst, *volat);
                if !q.is_empty() {
                    let _ = write!(d, "{q}");
                    if !inner.is_empty() {
                        d.push(' ');
                    }
                }
                d.push_str(&inner);
                to.split(d, true)
            }
            Self::Array { elem, count } => {
                let inner = if after_ptr {
                    format!("({inner})")
                } else {
                    inner
                };
                let n = count.map_or(String::new(), |c| c.to_string());
                elem.split(format!("{inner}[{n}]"), false)
            }
            Self::Func {
                ret,
                cc,
                params,
                varargs,
            } => {
                let mut inner = inner;
                if let Some(c) = cc {
                    inner = if inner.is_empty() {
                        c.clone()
                    } else {
                        format!("{c} {inner}")
                    };
                }
                if after_ptr || cc.is_some() {
                    inner = format!("({inner})");
                }
                let mut ps: Vec<String> = params.iter().map(Ty::spell).collect();
                if *varargs {
                    ps.push("...".into());
                }
                ret.split(format!("{inner}({})", ps.join(", ")), false)
            }
            Self::Bits { ty, width } => {
                let (b, d) = ty.split(inner, after_ptr);
                (b, format!("{d} : {width}"))
            }
        }
    }
}

fn qual(konst: bool, volat: bool) -> &'static str {
    match (konst, volat) {
        (true, true) => "const volatile",
        (true, false) => "const",
        (false, true) => "volatile",
        (false, false) => "",
    }
}

pub fn fundamental(words: &[&str]) -> Option<&'static str> {
    let has = |w: &str| words.contains(&w);
    let longs = words.iter().filter(|w| **w == "long").count();
    let unsigned = has("unsigned");
    let signed = has("signed");
    Some(if has("void") {
        "void"
    } else if has("bool") {
        "bool"
    } else if has("wchar_t") {
        "wchar_t"
    } else if has("float") {
        "float"
    } else if has("double") {
        if longs > 0 { "long double" } else { "double" }
    } else if has("char") {
        if unsigned {
            "unsigned char"
        } else if signed {
            "signed char"
        } else {
            "char"
        }
    } else if has("__int64") || longs >= 2 {
        if unsigned {
            "unsigned __int64"
        } else {
            "__int64"
        }
    } else if has("__int8") {
        if unsigned { "unsigned char" } else { "char" }
    } else if has("__int16") || has("short") {
        if unsigned { "unsigned short" } else { "short" }
    } else if longs == 1 {
        if unsigned { "unsigned long" } else { "long" }
    } else if has("int") || has("__int32") || unsigned || signed {
        if unsigned { "unsigned int" } else { "int" }
    } else {
        return None;
    })
}

pub fn prim_size(s: &str) -> Option<u64> {
    Some(match s {
        "char" | "signed char" | "unsigned char" | "bool" => 1,
        "short" | "unsigned short" | "wchar_t" => 2,
        "int" | "unsigned int" | "long" | "unsigned long" | "float" | "HRESULT" => 4,
        "double" | "long double" | "__int64" | "unsigned __int64" => 8,
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn declarators_render_inside_out() {
        let u = Ty::udt(Some(Key::Class), "Device");
        assert_eq!(Ty::ptr(u.clone()).decl("m_pnext"), "class Device *m_pnext");
        assert_eq!(
            Ty::Array {
                elem: Box::new(Ty::prim("char")),
                count: Some(260)
            }
            .decl("m_fname"),
            "char m_fname[260]"
        );
        let f = Ty::Func {
            ret: Box::new(Ty::prim("void")),
            cc: None,
            params: vec![Ty::prim("unsigned int"), Ty::prim("unsigned long")],
            varargs: false,
        };
        assert_eq!(
            Ty::ptr(f).decl("m_pcallback"),
            "void (*m_pcallback)(unsigned int, unsigned long)"
        );
        let pc = Ty::ptr(Ty::Cv {
            to: Box::new(Ty::prim("char")),
            konst: true,
            volat: false,
        });
        assert_eq!(pc.decl("fname"), "const char *fname");
        assert_eq!(fundamental(&["unsigned"]), Some("unsigned int"));
        assert_eq!(fundamental(&["long", "int"]), Some("long"));
        assert_eq!(fundamental(&["unsigned", "long"]), Some("unsigned long"));
        assert_eq!(fundamental(&["Sound"]), None);
    }
}
