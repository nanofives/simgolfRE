use crate::buf::{Buf, n16, n32};
use crate::pe::{DIR_DEBUG, Pe, SCN_CODE, SCN_DATA, SCN_RDATA};
use crate::tables::{DebugEntry, debug_directory};

pub const SST_MODULE: u16 = 0x120;
pub const SST_ALIGN_SYM: u16 = 0x125;
pub const SST_SRC_MODULE: u16 = 0x127;
pub const SST_GLOBAL_SYM: u16 = 0x129;
pub const SST_GLOBAL_PUB: u16 = 0x12a;
pub const SST_GLOBAL_TYPES: u16 = 0x12b;

pub const LF_POINTER: u16 = 0x1002;
pub const LF_CLASS: u16 = 0x1004;
pub const LF_STRUCTURE: u16 = 0x1005;
pub const LF_ENUM: u16 = 0x1007;
pub const LF_MFUNCTION: u16 = 0x1009;
pub const LF_ARGLIST: u16 = 0x1201;
pub const LF_FIELDLIST: u16 = 0x1203;
pub const LF_METHODLIST: u16 = 0x1206;

pub const ATTR_PSEUDO: u16 = 0x20;
pub const INTRO: u16 = 4 << 2;

#[must_use]
pub fn st(s: &str) -> Vec<u8> {
    let mut v = vec![u8::try_from(s.len()).unwrap_or(u8::MAX)];
    v.extend_from_slice(s.as_bytes());
    v
}

fn lf_pad(v: &mut Vec<u8>) {
    while !v.len().is_multiple_of(4) {
        let k = 4 - v.len() % 4;
        v.push(0xf0 | u8::try_from(k).unwrap_or(0));
    }
}

#[must_use]
pub fn type_record(leaf: u16, data: &[u8]) -> Vec<u8> {
    let mut r = vec![0u8, 0];
    r.extend_from_slice(&leaf.to_le_bytes());
    r.extend_from_slice(data);
    lf_pad(&mut r);
    let len = n16(r.len().saturating_sub(2));
    if let Some(h) = r.get_mut(..2) {
        h.copy_from_slice(&len.to_le_bytes());
    }
    r
}

#[must_use]
pub fn sym(kind: u16, body: &[u8]) -> Vec<u8> {
    let total = body.len().saturating_add(4).next_multiple_of(4);
    let mut v = n16(total.saturating_sub(2)).to_le_bytes().to_vec();
    v.extend_from_slice(&kind.to_le_bytes());
    v.extend_from_slice(body);
    v.resize(total, 0);
    v
}

#[must_use]
pub fn member(attr: u16, ty: u32, off: u16, name: &str) -> Vec<u8> {
    let mut d = 0x1405u16.to_le_bytes().to_vec();
    d.extend_from_slice(&attr.to_le_bytes());
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&off.to_le_bytes());
    d.extend_from_slice(&st(name));
    d
}

#[must_use]
pub fn base_class(attr: u16, ty: u32, off: u16) -> Vec<u8> {
    let mut d = 0x1400u16.to_le_bytes().to_vec();
    d.extend_from_slice(&attr.to_le_bytes());
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&off.to_le_bytes());
    d
}

#[must_use]
pub fn onemethod(attr: u16, ty: u32, vbaseoff: Option<u32>, name: &str) -> Vec<u8> {
    let mut d = 0x140bu16.to_le_bytes().to_vec();
    d.extend_from_slice(&attr.to_le_bytes());
    d.extend_from_slice(&ty.to_le_bytes());
    if let Some(v) = vbaseoff {
        d.extend_from_slice(&v.to_le_bytes());
    }
    d.extend_from_slice(&st(name));
    d
}

#[must_use]
pub fn method(count: u16, ml: u32, name: &str) -> Vec<u8> {
    let mut d = 0x1407u16.to_le_bytes().to_vec();
    d.extend_from_slice(&count.to_le_bytes());
    d.extend_from_slice(&ml.to_le_bytes());
    d.extend_from_slice(&st(name));
    d
}

#[must_use]
pub fn stmember(attr: u16, ty: u32, name: &str) -> Vec<u8> {
    let mut d = 0x1406u16.to_le_bytes().to_vec();
    d.extend_from_slice(&attr.to_le_bytes());
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&st(name));
    d
}

#[must_use]
pub fn nested(ty: u32, name: &str) -> Vec<u8> {
    let mut d = 0x1408u16.to_le_bytes().to_vec();
    d.extend_from_slice(&0u16.to_le_bytes());
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&st(name));
    d
}

#[must_use]
pub fn enumerate(value: u16, name: &str) -> Vec<u8> {
    let mut d = 0x0403u16.to_le_bytes().to_vec();
    d.extend_from_slice(&3u16.to_le_bytes());
    d.extend_from_slice(&value.to_le_bytes());
    d.extend_from_slice(&st(name));
    d
}

#[must_use]
pub fn vfptr() -> Vec<u8> {
    let mut d = 0x1409u16.to_le_bytes().to_vec();
    d.extend_from_slice(&0u16.to_le_bytes());
    d.extend_from_slice(&0u32.to_le_bytes());
    d
}

#[must_use]
pub fn index(cont: u32) -> Vec<u8> {
    let mut d = 0x1404u16.to_le_bytes().to_vec();
    d.extend_from_slice(&0u16.to_le_bytes());
    d.extend_from_slice(&cont.to_le_bytes());
    d
}

#[must_use]
pub fn gproc(name: &str, seg: u16, off: u32, len: u32, ty: u32) -> Vec<u8> {
    let mut d = vec![0u8; 12];
    d.extend_from_slice(&len.to_le_bytes());
    d.extend_from_slice(&[0u8; 8]);
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&off.to_le_bytes());
    d.extend_from_slice(&seg.to_le_bytes());
    d.push(0);
    d.extend_from_slice(&st(name));
    sym(0x100b, &d)
}

#[must_use]
pub fn bprel(off: i32, ty: u32, name: &str) -> Vec<u8> {
    let mut d = off.to_le_bytes().to_vec();
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&st(name));
    sym(0x1006, &d)
}

#[must_use]
pub fn block(off: u32, len: u32) -> Vec<u8> {
    let mut d = vec![0u8; 8];
    d.extend_from_slice(&len.to_le_bytes());
    d.extend_from_slice(&off.to_le_bytes());
    d.extend_from_slice(&1u16.to_le_bytes());
    d.extend_from_slice(&st(""));
    sym(0x0207, &d)
}

#[must_use]
pub fn end() -> Vec<u8> {
    sym(0x0006, &[])
}

#[must_use]
pub fn objname(path: &str) -> Vec<u8> {
    let mut d = 0u32.to_le_bytes().to_vec();
    d.extend_from_slice(&st(path));
    sym(0x0009, &d)
}

#[must_use]
pub fn data32(kind: u16, name: &str, seg: u16, off: u32, ty: u32) -> Vec<u8> {
    let mut d = ty.to_le_bytes().to_vec();
    d.extend_from_slice(&off.to_le_bytes());
    d.extend_from_slice(&seg.to_le_bytes());
    d.extend_from_slice(&st(name));
    sym(kind, &d)
}

#[must_use]
pub fn pub32(name: &str, seg: u16, off: u32) -> Vec<u8> {
    data32(0x1009, name, seg, off, 0)
}

#[must_use]
pub fn gdata32(name: &str, seg: u16, off: u32, ty: u32) -> Vec<u8> {
    data32(0x1008, name, seg, off, ty)
}

#[must_use]
pub fn udt(ty: u32, name: &str) -> Vec<u8> {
    let mut d = ty.to_le_bytes().to_vec();
    d.extend_from_slice(&st(name));
    sym(0x1003, &d)
}

#[must_use]
pub fn constant(ty: u32, value: u16, name: &str) -> Vec<u8> {
    let mut d = ty.to_le_bytes().to_vec();
    d.extend_from_slice(&value.to_le_bytes());
    d.extend_from_slice(&st(name));
    sym(0x1002, &d)
}

pub type SrcFile = (String, u16, Vec<(u32, u16)>);

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Module {
    pub name: String,
    pub ilib: u16,
    pub segments: Vec<(u16, u32, u32)>,
    pub symbols: Vec<u8>,
    pub files: Vec<SrcFile>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Nb11 {
    pub types: Vec<(u16, Vec<u8>)>,
    pub modules: Vec<Module>,
    pub publics: Vec<u8>,
    pub globals: Vec<u8>,
}

impl Nb11 {
    pub fn ty(&mut self, leaf: u16, data: Vec<u8>) -> u32 {
        self.types.push((leaf, data));
        0x1000u32.saturating_add(n32(self.types.len().saturating_sub(1)))
    }

    pub fn fieldlist(&mut self, members: &[Vec<u8>]) -> u32 {
        let mut d = Vec::new();
        for m in members {
            d.extend_from_slice(m);
            lf_pad(&mut d);
        }
        self.ty(LF_FIELDLIST, d)
    }

    pub fn class(&mut self, leaf: u16, count: u16, field: u32, size: u16, name: &str) -> u32 {
        let prop: u16 = if field == 0 { 0x80 } else { 0 };
        let mut d = Vec::new();
        d.extend_from_slice(&count.to_le_bytes());
        d.extend_from_slice(&prop.to_le_bytes());
        d.extend_from_slice(&field.to_le_bytes());
        d.extend_from_slice(&0u32.to_le_bytes());
        d.extend_from_slice(&0u32.to_le_bytes());
        d.extend_from_slice(&size.to_le_bytes());
        d.extend_from_slice(&st(name));
        self.ty(leaf, d)
    }

    pub fn enumeration(&mut self, count: u16, utype: u32, field: u32, name: &str) -> u32 {
        let mut d = count.to_le_bytes().to_vec();
        d.extend_from_slice(&0u16.to_le_bytes());
        d.extend_from_slice(&utype.to_le_bytes());
        d.extend_from_slice(&field.to_le_bytes());
        d.extend_from_slice(&st(name));
        self.ty(LF_ENUM, d)
    }

    pub fn arglist(&mut self, args: &[u32]) -> u32 {
        let mut d = n32(args.len()).to_le_bytes().to_vec();
        for a in args {
            d.extend_from_slice(&a.to_le_bytes());
        }
        self.ty(LF_ARGLIST, d)
    }

    pub fn pointer(&mut self, to: u32) -> u32 {
        let mut d = to.to_le_bytes().to_vec();
        d.extend_from_slice(&0x0000_000au32.to_le_bytes());
        self.ty(LF_POINTER, d)
    }

    pub fn mfunc(&mut self, ret: u32, class: u32, this: u32, args: &[u32]) -> u32 {
        let al = self.arglist(args);
        let mut d = Vec::new();
        for x in [ret, class, this] {
            d.extend_from_slice(&x.to_le_bytes());
        }
        d.push(if this == 0 { 0 } else { 11 });
        d.push(0);
        d.extend_from_slice(&n16(args.len()).to_le_bytes());
        d.extend_from_slice(&al.to_le_bytes());
        d.extend_from_slice(&0u32.to_le_bytes());
        self.ty(LF_MFUNCTION, d)
    }

    pub fn methodlist(&mut self, entries: &[(u16, u32, Option<u32>)]) -> u32 {
        let mut d = Vec::new();
        for (attr, ty, vb) in entries {
            d.extend_from_slice(&attr.to_le_bytes());
            d.extend_from_slice(&0u16.to_le_bytes());
            d.extend_from_slice(&ty.to_le_bytes());
            if let Some(v) = vb {
                d.extend_from_slice(&v.to_le_bytes());
            }
        }
        self.ty(LF_METHODLIST, d)
    }

    #[must_use]
    pub fn global_types(&self) -> Vec<u8> {
        let mut t = Buf::new();
        t.u32(0x0000_0001).u32(n32(self.types.len()));
        let mut recs = Vec::new();
        let mut offs = Vec::new();
        for (leaf, data) in &self.types {
            offs.push(n32(recs.len()));
            recs.extend_from_slice(&type_record(*leaf, data));
        }
        for o in offs {
            t.u32(o);
        }
        t.bytes(&recs);
        t.into_vec()
    }

    #[must_use]
    pub fn subsections(&self) -> Vec<(u16, u16, Vec<u8>)> {
        let mut subs = Vec::new();
        for (i, m) in self.modules.iter().enumerate() {
            let imod = n16(i.saturating_add(1));
            let mut b = Buf::new();
            b.u16(0).u16(m.ilib).u16(n16(m.segments.len())).bytes(b"CV");
            for (seg, off, cb) in &m.segments {
                b.u16(*seg).u16(0).u32(*off).u32(*cb);
            }
            b.bytes(&st(&m.name));
            subs.push((SST_MODULE, imod, b.into_vec()));
            let mut s = 1u32.to_le_bytes().to_vec();
            s.extend_from_slice(&m.symbols);
            subs.push((SST_ALIGN_SYM, imod, s));
            subs.push((SST_SRC_MODULE, imod, src_module(&m.files)));
        }
        for (sst, body) in [
            (SST_GLOBAL_SYM, &self.globals),
            (SST_GLOBAL_PUB, &self.publics),
        ] {
            let mut g = Buf::new();
            g.u16(0)
                .u16(0)
                .u32(n32(body.len()))
                .u32(0)
                .u32(0)
                .bytes(body);
            subs.push((sst, 0xffff, g.into_vec()));
        }
        subs.push((SST_GLOBAL_TYPES, 0xffff, self.global_types()));
        subs
    }

    #[must_use]
    pub fn build(&self) -> Vec<u8> {
        pack(b"NB11", &self.subsections())
    }
}

fn src_module(files: &[SrcFile]) -> Vec<u8> {
    let cf = files.len();
    let head = (4usize
        .saturating_add(cf.saturating_mul(4))
        .saturating_add(8)
        .saturating_add(2))
    .next_multiple_of(4);
    let mut bodies = Vec::new();
    let mut offs = Vec::new();
    for (fname, seg, lines) in files {
        let fo = head.saturating_add(bodies.len());
        offs.push(n32(fo));
        let name_len = fname.len().saturating_add(1);
        let lb_at = fo
            .saturating_add(4)
            .saturating_add(12)
            .saturating_add(name_len)
            .next_multiple_of(4);
        let mut fe = Buf::new();
        fe.u16(1).u16(0).u32(n32(lb_at)).zeros(8).bytes(&st(fname));
        while !fo.saturating_add(fe.len()).is_multiple_of(4) {
            fe.u8(0);
        }
        fe.u16(*seg).u16(n16(lines.len()));
        for (o, _) in lines {
            fe.u32(*o);
        }
        for (_, l) in lines {
            fe.u16(*l);
        }
        fe.align(4);
        bodies.extend_from_slice(fe.as_slice());
    }
    let mut sm = Buf::new();
    sm.u16(n16(cf)).u16(1);
    for o in offs {
        sm.u32(o);
    }
    sm.zeros(8).u16(1).pad_to(head).bytes(&bodies);
    sm.into_vec()
}

#[must_use]
pub fn pack(signature: &[u8; 4], subs: &[(u16, u16, Vec<u8>)]) -> Vec<u8> {
    let mut nb = Buf::new();
    nb.bytes(signature).u32(0);
    let mut placed = Vec::new();
    for (sst, imod, body) in subs {
        let at = nb.len();
        nb.bytes(body).align(4);
        placed.push((*sst, *imod, at, body.len()));
    }
    let dir = nb.pos();
    nb.put_u32(4, dir);
    nb.u16(16).u16(12).u32(n32(placed.len())).u32(0).u32(0);
    for (sst, imod, at, cb) in placed {
        nb.u16(sst).u16(imod).u32(n32(at)).u32(n32(cb));
    }
    nb.into_vec()
}

#[derive(Debug, Clone)]
pub struct Sample {
    pub nb: Nb11,
    pub record: Vec<u8>,
    pub image: Vec<u8>,
    pub text: u32,
    pub rdata: u32,
    pub dev: u32,
    pub snd: u32,
}

pub const DEV_HEADER: &str = "c:\\work\\src\\dev h.h";
pub const DEV_SOURCE: &str = "C:\\work\\src\\dev.cpp";
pub const SND_SOURCE: &str = "C:\\work\\src\\snd.cpp";

#[must_use]
pub fn sample() -> Sample {
    let mut b = Nb11::default();
    let fwd = b.class(LF_CLASS, 0, 0, 0, "Dev");
    let pdev = b.pointer(fwd);
    let set = b.mfunc(0x0003, fwd, pdev, &[0x0022]);
    let is_on = b.mfunc(0x0020, fwd, pdev, &[]);
    let ctor = b.mfunc(0x0003, fwd, pdev, &[]);
    let fl = b.fieldlist(&[
        vfptr(),
        onemethod(3, ctor, None, "Dev"),
        member(1, 0x0022, 4, "m_flags"),
        stmember(2, pdev, "m_head"),
        onemethod(3 | INTRO, set, Some(0), "set_volume"),
        onemethod(3, is_on, None, "is_on"),
        onemethod(3 | ATTR_PSEUDO, ctor, None, "__local_vftable_ctor_closure"),
    ]);
    let dev = b.class(LF_CLASS, 6, fl, 8, "Dev");
    let efl = b.fieldlist(&[enumerate(0, "ERR_OK"), enumerate(1, "ERR_FAIL")]);
    let err = b.enumeration(2, 0x74, efl, "ERRCODE");
    let sfwd = b.class(LF_CLASS, 0, 0, 0, "Snd");
    let ps = b.pointer(sfwd);
    let load_s = b.mfunc(0x0003, sfwd, ps, &[0x0470]);
    let load_n = b.mfunc(0x0003, sfwd, ps, &[0x0022]);
    let play0 = b.mfunc(0x0003, sfwd, ps, &[]);
    let play1 = b.mfunc(0x0003, sfwd, ps, &[0x0022]);
    let intro = 3 | INTRO;
    let loads = b.methodlist(&[(intro, load_n, Some(4)), (intro, load_s, Some(4))]);
    let plays = b.methodlist(&[(intro, play1, Some(0xc)), (intro, play0, Some(0xc))]);
    let sfl2 = b.fieldlist(&[method(2, plays, "play")]);
    let sfl = b.fieldlist(&[
        base_class(3, dev, 0),
        vfptr(),
        method(2, loads, "load"),
        index(sfl2),
    ]);
    let snd = b.class(LF_CLASS, 3, sfl, 12, "Snd");
    let mut syms = objname("C:\\work\\obj\\dev.obj");
    syms.extend(gproc("Dev::set_volume", 1, 0x10, 0x20, set));
    syms.extend(bprel(-4, pdev, "this"));
    syms.extend(bprel(8, 0x0022, "volume"));
    syms.extend(block(0x18, 4));
    syms.extend(bprel(-8, err, "status"));
    syms.extend(end());
    syms.extend(end());
    syms.extend(gproc("Dev::is_on", 1, 0x40, 0x10, is_on));
    syms.extend(end());
    syms.extend(gproc("Dev::Dev", 1, 0x60, 0x10, ctor));
    syms.extend(end());
    syms.extend(gdata32("Dev::m_head", 3, 0, pdev));
    b.modules.push(Module {
        name: ".\\obj\\dev.obj".into(),
        ilib: 0,
        segments: vec![(1, 0, 0x80)],
        symbols: syms,
        files: vec![
            (
                DEV_HEADER.into(),
                1,
                vec![(0x10, 42), (0x18, 43), (0x40, 50)],
            ),
            (DEV_SOURCE.into(), 1, vec![(0x60, 7)]),
        ],
    });
    let mut s2 = objname("C:\\work\\obj\\snd.obj");
    s2.extend(gproc("Snd::play", 1, 0x90, 0x10, play0));
    s2.extend(end());
    s2.extend(gproc("Snd::play", 1, 0xb0, 0x10, play1));
    s2.extend(bprel(8, 0x0022, "n"));
    s2.extend(end());
    b.modules.push(Module {
        name: ".\\obj\\snd.obj".into(),
        ilib: 0,
        segments: vec![(1, 0x90, 0x40)],
        symbols: s2,
        files: vec![(SND_SOURCE.into(), 1, vec![(0x90, 5), (0xb0, 9)])],
    });
    b.modules.push(Module {
        name: "LIBC.lib".into(),
        ilib: 1,
        segments: vec![(1, 0xd0, 0x10)],
        symbols: gproc("_memset", 1, 0xd0, 0x10, 0),
        files: Vec::new(),
    });
    let mut p = pub32("?set_volume@Dev@@UAEXK@Z", 1, 0x10);
    p.extend(pub32("?is_on@Dev@@QAE_NXZ", 1, 0x40));
    p.extend(pub32("??0Dev@@QAE@XZ", 1, 0x60));
    p.extend(pub32("?play@Snd@@UAEXXZ", 1, 0x90));
    p.extend(pub32("?play@Snd@@UAEXK@Z", 1, 0xb0));
    p.extend(pub32("??_7Dev@@6B@", 2, 0));
    p.extend(pub32("??_7Snd@@6B@", 2, 0x10));
    p.extend(pub32("_end_of_tables", 2, 0x30));
    b.publics = p;
    let mut g = udt(0x0022, "DWORD");
    g.extend(udt(dev, "Dev"));
    g.extend(constant(0x0074, 3, "LIMIT"));
    b.globals = g;
    let record = b.build();
    let mut pe = Pe::dll();
    let mut code = vec![0xcc; 0x100];
    for at in [0x10usize, 0x40, 0x60, 0x90, 0xb0, 0xd0] {
        if let Some(c) = code.get_mut(at) {
            *c = 0xc3;
        }
    }
    let text = pe.push(".text", SCN_CODE, code);
    let rdata = pe.next_rva();
    let base = pe.image_base;
    let mut rd = Buf::new();
    rd.u32(base.saturating_add(text).saturating_add(0x10))
        .zeros(12);
    for off in [0x40u32, 0xb0, 0x90, 0x20] {
        rd.u32(base.saturating_add(text).saturating_add(off));
    }
    rd.zeros(16);
    let dbg_at = rd.len();
    rd.bytes(&debug_directory(&[DebugEntry {
        kind: 2,
        timestamp: 0x3b00_0000,
        size: n32(record.len()),
        rva: 0,
    }]));
    pe.push(".rdata", SCN_RDATA, rd.into_vec());
    pe.push_virtual(".data", SCN_DATA, vec![0; 0x10], 0x100);
    pe.dir(DIR_DEBUG, rdata.saturating_add(n32(dbg_at)), 28);
    let layout = pe.layout();
    let overlay = layout
        .sections
        .last()
        .map_or(0, |s| s.raw_off.saturating_add(s.raw_size));
    if let Some(s) = pe.sections.get_mut(1) {
        let ptr = dbg_at.saturating_add(24);
        if let Some(f) = s.data.get_mut(ptr..ptr.saturating_add(4)) {
            f.copy_from_slice(&overlay.to_le_bytes());
        }
    }
    pe.overlay.clone_from(&record);
    Sample {
        image: pe.build(),
        nb: b,
        record,
        text,
        rdata,
        dev,
        snd,
    }
}
