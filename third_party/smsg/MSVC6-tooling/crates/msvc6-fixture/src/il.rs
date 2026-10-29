pub const STAMP: u32 = 0x0130_ba96;
pub const FLAG_WIDE: u32 = 2;

pub const OP_ADD: u8 = 0x02;
pub const OP_SYM: u8 = 0x26;
pub const OP_LABEL: u8 = 0x29;
pub const OP_PARAM: u8 = 0x2d;
pub const OP_LOAD: u8 = 0x30;
pub const OP_ASG: u8 = 0x32;
pub const OP_CONST: u8 = 0x33;
pub const OP_GOTO: u8 = 0x3a;
pub const OP_CALL: u8 = 0x3e;
pub const OP_RET: u8 = 0x41;
pub const OP_ENTRY: u8 = 0x46;
pub const OP_EXIT: u8 = 0x47;
pub const OP_DISCARD: u8 = 0x4b;
pub const OP_ENDLIST: u8 = 0x4c;
pub const OP_EOF: u8 = 0x4d;
pub const OP_PRAGMA: u8 = 0x4f;
pub const OP_SCOPE: u8 = 0x53;
pub const OP_ENDSCOPE: u8 = 0x54;
pub const OP_ARG: u8 = 0x55;
pub const OP_EXHEAD: u8 = 0x5b;
pub const OP_EH_LIVE: u8 = 0x5c;
pub const OP_EH_UNWIND: u8 = 0x5d;
pub const OP_EH_LEAVE: u8 = 0x5e;
pub const OP_THISARG: u8 = 0x99;

pub const TY_I32: u16 = 0x41;
pub const TY_VOID: u16 = 0x07;
pub const TY_P32: u16 = 0x43;

pub const BODY_AT: u32 = 0x3f4;

pub const FMT_OPS: [(u8, u8); 23] = [
    (OP_ADD, 0),
    (OP_SYM, 2),
    (OP_LABEL, 2),
    (OP_PARAM, 2),
    (OP_LOAD, 1),
    (OP_ASG, 1),
    (OP_CONST, 6),
    (OP_GOTO, 2),
    (OP_CALL, 5),
    (OP_RET, 1),
    (OP_ENTRY, 0),
    (OP_EXIT, 0),
    (OP_DISCARD, 0),
    (OP_ENDLIST, 0),
    (OP_EOF, 0),
    (OP_PRAGMA, 0x0c),
    (OP_SCOPE, 0),
    (OP_ENDSCOPE, 0x0d),
    (OP_ARG, 1),
    (OP_EXHEAD, 0x0d),
    (OP_EH_LIVE, 0x13),
    (OP_EH_UNWIND, 0x14),
    (OP_THISARG, 1),
];

pub const ENDING_OPS: [u8; 5] = [0x38, 0x39, 0x3a, 0x41, 0x4b];

#[must_use]
pub fn fmt_table(len: usize) -> Vec<u8> {
    let mut f = vec![0u8; len];
    for (op, fm) in FMT_OPS {
        if let Some(x) = f.get_mut(usize::from(op)) {
            *x = fm;
        }
    }
    if let Some(x) = f.get_mut(usize::from(OP_EH_LEAVE)) {
        *x = 0x14;
    }
    if let Some(x) = f.get_mut(0x38) {
        *x = 2;
    }
    if let Some(x) = f.get_mut(0x39) {
        *x = 2;
    }
    f
}

#[must_use]
pub fn attr_table(len: usize) -> Vec<u16> {
    let mut a = vec![0u16; len];
    for op in ENDING_OPS {
        if let Some(x) = a.get_mut(usize::from(op)) {
            *x = 0x1000;
        }
    }
    a
}

pub const PRAGMA_LINE: i16 = 1;
pub const PRAGMA_FILE: i16 = 2;
pub const PRAGMA_OPTFLAGS: i16 = 0x1f;

#[must_use]
pub fn pragma_codes() -> Vec<Option<Vec<u8>>> {
    let mut v: Vec<Option<Vec<u8>>> = vec![None; 49];
    if let Some(x) = v.get_mut(1) {
        *x = Some(b"l".to_vec());
    }
    if let Some(x) = v.get_mut(2) {
        *x = Some(b"s".to_vec());
    }
    if let Some(x) = v.get_mut(0x1f) {
        *x = Some(vec![0x14]);
    }
    v
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Enc(pub Vec<u8>);

impl Enc {
    pub fn byte(&mut self, b: u8) -> &mut Self {
        self.0.push(b);
        self
    }

    pub fn raw(&mut self, b: &[u8]) -> &mut Self {
        self.0.extend_from_slice(b);
        self
    }

    pub fn id(&mut self, id: u32) -> &mut Self {
        if id < 0x8000 {
            let lo = u16::try_from(id).unwrap_or(0);
            self.0.extend_from_slice(&lo.to_le_bytes());
        } else {
            let lo = u16::try_from(id & 0x7fff).unwrap_or(0) | 0x8000;
            let hi = u16::try_from((id >> 15) & 0xffff).unwrap_or(0);
            self.0.extend_from_slice(&lo.to_le_bytes());
            self.0.extend_from_slice(&hi.to_le_bytes());
        }
        self
    }

    pub fn short(&mut self, v: i16) -> &mut Self {
        match i8::try_from(v) {
            Ok(b) if b != i8::MIN => self.0.extend_from_slice(&b.to_le_bytes()),
            _ => {
                self.0.push(0x80);
                self.0.extend_from_slice(&v.to_le_bytes());
            }
        }
        self
    }

    pub fn long(&mut self, v: i32) -> &mut Self {
        match i8::try_from(v) {
            Ok(b) if b != i8::MIN => self.0.extend_from_slice(&b.to_le_bytes()),
            _ => {
                self.0.push(0x80);
                self.0.extend_from_slice(&v.to_le_bytes());
            }
        }
        self
    }

    pub fn ty(&mut self, t: u16) -> &mut Self {
        if t < 0x80 {
            self.0.push(u8::try_from(t).unwrap_or(0));
        } else {
            self.0.extend_from_slice(&(t | 0x8000).to_be_bytes());
        }
        self
    }

    pub fn str(&mut self, s: &[u8]) -> &mut Self {
        self.0.extend_from_slice(s);
        self.0.push(0);
        self
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.0.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Def {
    pub id: u32,
    pub name: String,
    pub ret: u16,
    pub fclass: u8,
    pub flags: u32,
    pub s6d: i16,
    pub fflags: u32,
    pub formals: i16,
    pub ex: u32,
    pub sy: u32,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Set {
    pub next_id: u32,
    pub lang: u8,
    pub srcfile: Option<(u32, String)>,
    pub defs: Vec<Def>,
    pub sy: Enc,
    pub ex: Enc,
    pub init: Enc,
}

#[must_use]
pub fn ex_header() -> Enc {
    let mut e = Enc::default();
    e.byte(OP_EXHEAD).long(i32::try_from(BODY_AT).unwrap_or(0));
    let pad = usize::try_from(BODY_AT)
        .unwrap_or(0)
        .saturating_sub(e.len());
    e.0.extend(std::iter::repeat_n(0u8, pad));
    e
}

impl Set {
    #[must_use]
    pub fn new(next_id: u32) -> Self {
        Self {
            next_id,
            lang: 1,
            ex: ex_header(),
            ..Self::default()
        }
    }

    #[must_use]
    pub fn gl(&self) -> Vec<u8> {
        let mut g = Enc::default();
        g.byte(0x11).raw(&0x0386u16.to_le_bytes());
        g.raw(&STAMP.to_le_bytes())
            .raw(&self.next_id.to_le_bytes())
            .byte(self.lang)
            .raw(&FLAG_WIDE.to_le_bytes());
        g.short(12).short(0).short(8168);
        if let Some((id, name)) = &self.srcfile {
            g.byte(0x12).id(*id).str(name.as_bytes());
        }
        for d in &self.defs {
            g.byte(0x0e)
                .id(d.id)
                .byte(0)
                .str(d.name.as_bytes())
                .ty(d.ret)
                .byte(d.fclass)
                .long(if d.ret == TY_VOID { 0 } else { 4 })
                .id(d.flags)
                .long(0)
                .long(i32::try_from(d.ex).unwrap_or(0))
                .long(i32::try_from(d.sy).unwrap_or(0))
                .short(d.s6d)
                .id(d.fflags)
                .short(d.formals);
        }
        g.byte(0x0a).long(0).str(b"").long(0);
        g.0
    }

    pub fn list_label(&mut self, level: u8, id: u32, role: u8, refs: i16) {
        self.sy
            .byte(3)
            .byte(level)
            .id(id)
            .byte(role)
            .str(b"")
            .short(refs)
            .byte(1);
    }

    pub fn list_block(&mut self, level: u8) {
        self.sy.byte(0x0d).byte(level);
    }

    pub fn list_var(&mut self, level: u8, id: u32, name: &str, storage: u8) {
        self.sy
            .byte(1)
            .byte(level)
            .id(id)
            .byte(0)
            .str(name.as_bytes())
            .ty(1)
            .byte(0)
            .byte(storage)
            .byte(4)
            .long(4)
            .id(0x100)
            .long(0);
    }

    pub fn list_end(&mut self) {
        self.sy.byte(6);
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Body(pub Enc);

impl Body {
    pub fn open(&mut self, f: u32, formals: &[u32]) -> &mut Self {
        self.0
            .byte(OP_PRAGMA)
            .short(PRAGMA_OPTFLAGS)
            .long(0x0080_0000);
        self.0.byte(OP_SCOPE).byte(OP_SCOPE);
        self.0.byte(OP_SYM).id(f).byte(OP_ENTRY);
        for p in formals {
            self.0.byte(OP_PARAM).id(*p);
        }
        self.0.byte(OP_ENDLIST).byte(OP_SCOPE);
        self
    }

    pub fn call_void(&mut self, callee: u32) -> &mut Self {
        self.0
            .byte(OP_SYM)
            .id(callee)
            .byte(OP_CALL)
            .ty(TY_VOID)
            .byte(0)
            .byte(OP_ENDLIST)
            .byte(OP_DISCARD);
        self
    }

    pub fn call_with_call(&mut self, outer: u32, inner: u32) -> &mut Self {
        self.0
            .byte(OP_SYM)
            .id(outer)
            .byte(OP_CALL)
            .ty(TY_VOID)
            .byte(0)
            .byte(OP_SYM)
            .id(inner)
            .byte(OP_CALL)
            .ty(TY_I32)
            .byte(0)
            .byte(OP_ENDLIST)
            .byte(OP_ARG)
            .ty(TY_I32)
            .byte(OP_ENDLIST)
            .byte(OP_DISCARD);
        self
    }

    pub fn local_object(&mut self, ctor: u32, dtor: u32, obj: u32) -> &mut Self {
        self.0
            .byte(OP_SYM)
            .id(ctor)
            .byte(OP_SYM)
            .id(obj)
            .byte(OP_THISARG)
            .ty(TY_P32)
            .byte(OP_CALL)
            .ty(TY_P32)
            .byte(0x14)
            .byte(OP_ENDLIST)
            .byte(OP_SYM)
            .id(dtor)
            .byte(OP_SYM)
            .id(obj)
            .byte(OP_THISARG)
            .ty(TY_P32)
            .byte(OP_CALL)
            .ty(TY_VOID)
            .byte(0x14)
            .byte(OP_ENDLIST)
            .byte(OP_EH_LIVE)
            .ty(TY_P32)
            .long(1)
            .byte(OP_DISCARD);
        self
    }

    pub fn leave(&mut self, count: i32, flags: i32) -> &mut Self {
        self.0
            .byte(OP_EH_LEAVE)
            .long(count)
            .long(flags)
            .byte(OP_DISCARD);
        self
    }

    pub fn unwind(&mut self, count: i32, flags: i32) -> &mut Self {
        self.0
            .byte(OP_EH_UNWIND)
            .long(count)
            .long(flags)
            .byte(OP_DISCARD);
        self
    }

    pub fn ret_const(&mut self, v: i32) -> &mut Self {
        self.0
            .byte(OP_CONST)
            .ty(TY_I32)
            .long(v)
            .byte(OP_RET)
            .ty(TY_I32);
        self
    }

    pub fn close(&mut self, exit_label: u32) -> &mut Self {
        self.0.byte(OP_GOTO).id(exit_label);
        self.0.byte(OP_ENDSCOPE).long(2);
        self.0.byte(OP_LABEL).id(exit_label).byte(OP_EXIT);
        self.0.byte(OP_ENDSCOPE).long(1).byte(OP_ENDSCOPE).long(0);
        self
    }
}

impl Set {
    pub fn define(&mut self, mut d: Def, body: &Body) {
        d.ex = u32::try_from(self.ex.len()).unwrap_or(u32::MAX);
        d.sy = u32::try_from(self.sy.len()).unwrap_or(u32::MAX);
        self.list_label(1, d.id.saturating_add(0x1000), 0x1f, 1);
        self.list_block(1);
        self.list_block(2);
        self.list_end();
        self.ex.raw(&body.0.0);
        self.init.byte(9).id(d.id);
        self.defs.push(d);
    }

    #[must_use]
    pub fn ex_bytes(&self) -> Vec<u8> {
        let mut e = self.ex.clone();
        e.byte(OP_EOF);
        e.0
    }

    #[must_use]
    pub fn in_bytes(&self) -> Vec<u8> {
        let mut i = self.init.clone();
        i.byte(0x0f);
        i.0
    }
}

#[must_use]
pub fn def(id: u32, name: &str, s6d: i16, fflags: u32) -> Def {
    Def {
        id,
        name: name.to_string(),
        ret: TY_VOID,
        fclass: 5,
        flags: 0,
        s6d,
        fflags,
        formals: 0,
        ex: 0,
        sy: 0,
    }
}

#[must_use]
pub fn grammar_image(fmt: &[u8], attr: &[u16], pragma: &[Option<Vec<u8>>]) -> Vec<u8> {
    let mut pe = crate::pe::Pe::dll();
    pe.push(".text", 0x6000_0020, vec![0xc3; 16]);
    let rva = pe.next_rva();
    let base = pe.image_base.saturating_add(rva);
    let mut d: Vec<u8> = vec![0x55; 64];
    for a in attr {
        d.extend_from_slice(&a.to_le_bytes());
    }
    d.extend_from_slice(fmt);
    let mut ptrs = Vec::new();
    for p in pragma {
        match p {
            Some(codes) => {
                ptrs.push(base.saturating_add(u32::try_from(d.len()).unwrap_or(0)));
                d.extend_from_slice(codes);
                d.push(0);
            }
            None => ptrs.push(0),
        }
    }
    while !d.len().is_multiple_of(8) {
        d.push(0);
    }
    for p in ptrs {
        d.extend_from_slice(&p.to_le_bytes());
        d.extend_from_slice(&0u32.to_le_bytes());
    }
    d.extend_from_slice(&0x0002_0018u32.to_le_bytes());
    d.extend_from_slice(&0x0071_0017u32.to_le_bytes());
    d.extend(std::iter::repeat_n(0x55u8, 64));
    pe.push(".rdata", 0x4000_0040, d);
    pe.build()
}

#[must_use]
pub fn writer_image(ops: &[(u16, Option<Vec<u8>>)], syms: &[Vec<u8>]) -> Vec<u8> {
    let mut pe = crate::pe::Pe::dll();
    pe.push(".text", 0x6000_0020, vec![0xc3; 16]);
    let rva = pe.next_rva();
    let base = pe.image_base.saturating_add(rva);
    let mut strings: Vec<u8> = vec![0x55; 32];
    let at = |codes: &[u8], strings: &mut Vec<u8>| -> u32 {
        let p = base.saturating_add(u32::try_from(strings.len()).unwrap_or(0));
        strings.extend_from_slice(codes);
        strings.push(0);
        p
    };
    let op_ptrs: Vec<(u16, u32)> = ops
        .iter()
        .map(|(a, c)| (*a, c.as_ref().map_or(0, |c| at(c, &mut strings))))
        .collect();
    let sym_ptrs: Vec<u32> = syms.iter().map(|c| at(c, &mut strings)).collect();
    while !strings.len().is_multiple_of(8) {
        strings.push(0);
    }
    let mut d = strings;
    for p in sym_ptrs {
        d.extend_from_slice(&p.to_le_bytes());
        d.extend_from_slice(&2u32.to_le_bytes());
    }
    d.extend_from_slice(&0x0002_0018u32.to_le_bytes());
    d.extend_from_slice(&0x0071_0017u32.to_le_bytes());
    for (a, p) in op_ptrs {
        d.extend_from_slice(&a.to_le_bytes());
        d.extend_from_slice(&0u16.to_le_bytes());
        d.extend_from_slice(&p.to_le_bytes());
    }
    d.extend_from_slice(&0x0002_0018u32.to_le_bytes());
    d.extend_from_slice(&0x0071_0017u32.to_le_bytes());
    pe.push(".rdata", 0x4000_0040, d);
    pe.build()
}
