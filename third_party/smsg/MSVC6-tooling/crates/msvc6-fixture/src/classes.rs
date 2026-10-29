use crate::pe::{Pe, SCN_CODE, SCN_RDATA};

pub const BASE: u32 = 0x1000_0000;
pub const TEXT: u32 = 0x1000;
pub const RDATA: u32 = 0x2000;
pub const BASE_VT: u32 = RDATA;
pub const DERIVED_VT: u32 = RDATA + 0x10;

pub const HEADER: &str = "class Base {\npublic:\n    Base();\n    virtual ~Base();\n    virtual void f(int n);\n    virtual void g();\n    virtual void f();\n};\n\nclass Derived : public Base {\npublic:\n    Derived();\n    void f(int n);\n    virtual void h();\n};\n";

pub const BODIES: [(u32, &str); 9] = [
    (0x1000, "??0Base@@QAE@XZ"),
    (0x1010, "??0Derived@@QAE@XZ"),
    (0x1020, "??_GBase@@UAEPAXI@Z"),
    (0x1030, "?f@Base@@UAEXXZ"),
    (0x1040, "?f@Base@@UAEXH@Z"),
    (0x1050, "?g@Base@@UAEXXZ"),
    (0x1060, "??_GDerived@@UAEPAXI@Z"),
    (0x1070, "?f@Derived@@UAEXH@Z"),
    (0x1080, "?h@Derived@@UAEXXZ"),
];

fn le(v: u32) -> [u8; 4] {
    v.to_le_bytes()
}

#[must_use]
pub fn text() -> Vec<u8> {
    let mut t = vec![0xccu8; 0x90];
    let mut put =
        |at: usize, b: &[u8]| t.splice(at..at + b.len(), b.iter().copied()).for_each(drop);
    let mut base_ctor = vec![0x8b, 0xc1, 0xc7, 0x00];
    base_ctor.extend(le(BASE + BASE_VT));
    base_ctor.push(0xc3);
    put(0x00, &base_ctor);
    let mut derived_ctor = vec![0xc7, 0x01];
    derived_ctor.extend(le(BASE + DERIVED_VT));
    derived_ctor.extend([0x8b, 0xc1, 0xc3]);
    put(0x10, &derived_ctor);
    for (at, ret4) in [
        (0x20, true),
        (0x30, false),
        (0x40, true),
        (0x50, false),
        (0x60, true),
        (0x70, true),
        (0x80, false),
    ] {
        if ret4 {
            put(at, &[0xc2, 0x04, 0x00]);
        } else {
            put(at, &[0xc3]);
        }
    }
    t
}

#[must_use]
pub fn rdata() -> Vec<u8> {
    let mut r = Vec::new();
    for b in [
        0x1020, 0x1030, 0x1040, 0x1050, 0x1060, 0x1030, 0x1070, 0x1050, 0x1080,
    ] {
        r.extend(le(BASE + b));
    }
    r.extend([0u8; 12]);
    r
}

#[must_use]
pub fn image() -> Vec<u8> {
    let mut pe = Pe::dll();
    pe.image_base = BASE;
    pe.push(".text", SCN_CODE, text());
    pe.push(".rdata", SCN_RDATA, rdata());
    pe.build()
}

#[must_use]
pub fn names() -> Vec<(String, u32)> {
    BODIES.iter().map(|(r, n)| ((*n).to_string(), *r)).collect()
}

#[must_use]
pub fn extents() -> Vec<(u32, Option<u32>)> {
    BODIES.iter().map(|(r, _)| (*r, Some(r + 0x10))).collect()
}

#[must_use]
pub fn names_tsv() -> String {
    let mut s = String::from("rva\tname\tkind\n");
    for (r, n) in BODIES {
        s.push_str(&format!("{r:#x}\t{n}\tfunction\n"));
    }
    s
}

fn hex(b: &[u8]) -> String {
    b.iter()
        .map(|x| format!("{x:02x}"))
        .collect::<Vec<_>>()
        .join(" ")
}

#[must_use]
pub fn ghidra_json() -> String {
    let t = text();
    let ctor = |at: u32, cuts: &[(u32, usize)], name: &str| {
        let ins: Vec<String> = cuts
            .iter()
            .map(|(off, len)| {
                let o = usize::try_from(*off).unwrap_or(0);
                format!(
                    "[\"{:x}\", \"\", \"{}\", 0]",
                    BASE + at + off,
                    hex(t
                        .get(o + usize::try_from(at - TEXT).unwrap_or(0)..)
                        .unwrap_or_default()
                        .get(..*len)
                        .unwrap_or_default())
                )
            })
            .collect();
        format!(
            "{{\"addr\": \"{:x}\", \"name\": \"{name}\", \"thunk\": 0, \"ins\": [{}]}}",
            BASE + at,
            ins.join(", ")
        )
    };
    format!(
        "[{}, {}]",
        ctor(0x1000, &[(0, 2), (2, 6), (8, 1)], "??0Base@@QAE@XZ"),
        ctor(0x1010, &[(0, 6), (6, 2), (8, 1)], "??0Derived@@QAE@XZ")
    )
}
