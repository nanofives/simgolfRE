use crate::buf::{Buf, n16, n32, us};
use crate::codeview::{st, sym, type_record};

pub const MSF20_MAGIC: &[u8] = b"Microsoft C/C++ program database 2.00\r\n\x1aJG";
pub const PAGE: usize = 1024;
pub const FPM_SPAN: usize = 8;

pub const S_COMPILE: u16 = 0x0001;
pub const S_END: u16 = 0x0006;
pub const S_OBJNAME_ST: u16 = 0x0009;
pub const S_THUNK32_ST: u16 = 0x0206;
pub const S_BLOCK32_ST: u16 = 0x0207;
pub const S_LABEL32_ST: u16 = 0x0209;
pub const S_PROCREF_ST: u16 = 0x0400;
pub const S_DATAREF_ST: u16 = 0x0401;
pub const S_LPROCREF_ST: u16 = 0x0403;
pub const S_REGISTER_ST: u16 = 0x1001;
pub const S_CONSTANT_ST: u16 = 0x1002;
pub const S_UDT_ST: u16 = 0x1003;
pub const S_BPREL32_ST: u16 = 0x1006;
pub const S_LDATA32_ST: u16 = 0x1007;
pub const S_GDATA32_ST: u16 = 0x1008;
pub const S_PUB32_ST: u16 = 0x1009;
pub const S_LPROC32_ST: u16 = 0x100a;
pub const S_GPROC32_ST: u16 = 0x100b;
pub const S_COMPILE2_ST: u16 = 0x1013;

pub const SIGNATURE: u32 = crate::sample::PDB_SIGNATURE;
pub const AGE: u32 = crate::sample::PDB_AGE;

#[must_use]
pub fn lhash(bytes: &[u8], modulo: u32) -> u32 {
    let mut h = 0u32;
    let whole = bytes.len() & !3;
    for w in bytes.get(..whole).unwrap_or_default().as_chunks::<4>().0 {
        h ^= u32::from_le_bytes(*w);
    }
    let rest = bytes.get(whole..).unwrap_or_default();
    let mut it = rest.iter();
    if rest.len() & 2 != 0 {
        let lo = it.next().copied().unwrap_or(0);
        let hi = it.next().copied().unwrap_or(0);
        h ^= u32::from(u16::from_le_bytes([lo, hi]));
    }
    if rest.len() & 1 != 0 {
        h ^= u32::from(it.next().copied().unwrap_or(0));
    }
    h |= 0x2020_2020;
    h ^= h >> 11;
    h ^= h >> 16;
    if modulo == 0 { h } else { h % modulo }
}

#[must_use]
pub fn small_msf(streams: &[Vec<u8>]) -> Vec<u8> {
    let data: usize = streams.iter().map(|s| s.len().div_ceil(PAGE)).sum();
    let dir_len = 4 + 8 * streams.len() + 2 * data;
    let dir_pages = dir_len.div_ceil(PAGE);
    let total = 1 + 2 * FPM_SPAN + data + dir_pages;
    let mut out = vec![0u8; total * PAGE];
    let mut next = 1 + 2 * FPM_SPAN;
    let mut dir = Buf::new();
    dir.u32(n32(streams.len()));
    for s in streams {
        dir.u32(n32(s.len())).u32(0x00c0_ffee);
    }
    let put = |out: &mut Vec<u8>, page: usize, bytes: &[u8]| {
        if let Some(d) = out.get_mut(page * PAGE..page * PAGE + bytes.len()) {
            d.copy_from_slice(bytes);
        }
    };
    for s in streams {
        for chunk in s.chunks(PAGE) {
            put(&mut out, next, chunk);
            dir.u16(n16(next));
            next += 1;
        }
    }
    let dir = dir.into_vec();
    let mut dir_list = Vec::new();
    for chunk in dir.chunks(PAGE) {
        put(&mut out, next, chunk);
        dir_list.push(n16(next));
        next += 1;
    }
    for p in total..FPM_SPAN * PAGE * 8 {
        if let Some(b) = out.get_mut(PAGE + p / 8) {
            *b |= 1 << (p % 8);
        }
    }
    let mut head = Buf::new();
    head.bytes(MSF20_MAGIC)
        .zeros(2)
        .u32(n32(PAGE))
        .u16(1)
        .u16(n16(total))
        .u32(n32(dir.len()))
        .u32(0);
    for p in dir_list {
        head.u16(p);
    }
    put(&mut out, 0, head.as_slice());
    out
}

fn proc_body(name: &str, seg: u16, off: u32, len: u32, ty: u32) -> Vec<u8> {
    let mut d = vec![0u8; 12];
    d.extend_from_slice(&len.to_le_bytes());
    d.extend_from_slice(&0u32.to_le_bytes());
    d.extend_from_slice(&len.to_le_bytes());
    d.extend_from_slice(&ty.to_le_bytes());
    d.extend_from_slice(&off.to_le_bytes());
    d.extend_from_slice(&seg.to_le_bytes());
    d.push(0);
    d.extend_from_slice(&st(name));
    d
}

fn data_body(name: &str, seg: u16, off: u32, ty: u32) -> Vec<u8> {
    let mut d = ty.to_le_bytes().to_vec();
    d.extend_from_slice(&off.to_le_bytes());
    d.extend_from_slice(&seg.to_le_bytes());
    d.extend_from_slice(&st(name));
    d
}

fn refsym(kind: u16, ib: u32, imod: u16, name: &str) -> Vec<u8> {
    let mut r = Buf::new();
    r.u16(14)
        .u16(kind)
        .u32(0)
        .u32(ib)
        .u16(imod)
        .u16(0)
        .bytes(&st(name));
    r.align(4);
    r.into_vec()
}

#[derive(Debug, Default)]
struct Region {
    bytes: Vec<u8>,
}

impl Region {
    fn push(&mut self, rec: Vec<u8>) -> u32 {
        let at = n32(self.bytes.len() + 4);
        self.bytes.extend_from_slice(&rec);
        at
    }

    fn set(&mut self, rec_at: u32, field: usize, v: u32) {
        let at = us(rec_at) - 4 + 4 + field;
        if let Some(s) = self.bytes.get_mut(at..at + 4) {
            s.copy_from_slice(&v.to_le_bytes());
        }
    }
}

type FileLines = (String, Vec<(u32, u16)>);

fn c11(seg: u16, files: &[FileLines]) -> Vec<u8> {
    let cf = files.len();
    let head = (4 + 4 * cf + 8 + 2).next_multiple_of(4);
    let mut bodies = Vec::new();
    let mut offs = Vec::new();
    let mut lo = u32::MAX;
    let mut hi = 0u32;
    for (name, pairs) in files {
        let fo = head + bodies.len();
        offs.push(n32(fo));
        let name_len = name.len() + 1;
        let blk = (fo + 4 + 12 + name_len).next_multiple_of(4);
        let start = pairs.iter().map(|p| p.0).min().unwrap_or(0);
        let end = pairs.iter().map(|p| p.0).max().unwrap_or(0) + 3;
        lo = lo.min(start);
        hi = hi.max(end);
        let mut fe = Buf::new();
        fe.u16(1)
            .u16(0)
            .u32(n32(blk))
            .u32(start)
            .u32(end)
            .bytes(&st(name));
        while !(fo + fe.len()).is_multiple_of(4) {
            fe.u8(0);
        }
        fe.u16(seg).u16(n16(pairs.len()));
        for (o, _) in pairs {
            fe.u32(*o);
        }
        for (_, l) in pairs {
            fe.u16(*l);
        }
        fe.align(4);
        bodies.extend_from_slice(fe.as_slice());
    }
    let mut b = Buf::new();
    b.u16(n16(cf)).u16(1);
    for o in offs {
        b.u32(o);
    }
    b.u32(lo).u32(hi).u16(seg).pad_to(head).bytes(&bodies);
    b.into_vec()
}

fn name_table(names: &[&str]) -> (Vec<u8>, Vec<u32>) {
    let mut buf = vec![0u8];
    let mut ids = Vec::new();
    for n in names {
        ids.push(n32(buf.len()));
        buf.extend_from_slice(n.as_bytes());
        buf.push(0);
    }
    let nb = names.len() * 2 + 1;
    let mut buckets = vec![0u32; nb];
    for (n, id) in names.iter().zip(&ids) {
        let mut j = us(lhash(n.as_bytes(), 0)) % nb;
        while buckets.get(j).copied().unwrap_or(1) != 0 {
            j = (j + 1) % nb;
        }
        if let Some(b) = buckets.get_mut(j) {
            *b = *id;
        }
    }
    let mut t = Buf::new();
    t.u32(0xeffe_effe)
        .u32(1)
        .u32(n32(buf.len()))
        .bytes(&buf)
        .u32(n32(nb));
    for b in buckets {
        t.u32(b);
    }
    t.u32(n32(names.len()));
    (t.into_vec(), ids)
}

fn gsi(entries: &[(u32, &str)]) -> Vec<u8> {
    let mut keyed: Vec<(u32, usize, u32)> = entries
        .iter()
        .enumerate()
        .map(|(i, (off, name))| (lhash(name.as_bytes(), 4096), i, *off))
        .collect();
    keyed.sort_unstable();
    let mut heads = vec![-1i32; 4097];
    let mut b = Buf::new();
    for (i, (bucket, _, off)) in keyed.iter().enumerate() {
        if let Some(h) = heads.get_mut(us(*bucket))
            && *h < 0
        {
            *h = i32::try_from(i * 12).unwrap_or(-1);
        }
        b.u32(off + 1).u32(1);
    }
    for h in heads {
        b.bytes(&h.to_le_bytes());
    }
    b.into_vec()
}

#[derive(Debug, Clone)]
pub struct Sample {
    pub bytes: Vec<u8>,
    pub sections: Vec<u8>,
    pub image: Vec<u8>,
    pub procs: Vec<(String, u32, u32)>,
    pub publics: Vec<(String, u32)>,
    pub modules: usize,
    pub line_rows: usize,
}

fn types() -> Vec<Vec<u8>> {
    let mut t = Vec::new();
    t.push(type_record(0x1201, &0u32.to_le_bytes()));
    let mut p = 0x74u32.to_le_bytes().to_vec();
    p.extend_from_slice(&[0, 0, 0, 0]);
    p.extend_from_slice(&0x1000u32.to_le_bytes());
    t.push(type_record(0x1008, &p));
    let mut f = Vec::new();
    f.extend_from_slice(&crate::codeview::member(3, 0x74, 0, "m_x"));
    pad(&mut f);
    f.extend_from_slice(&crate::codeview::onemethod(
        3 | crate::codeview::INTRO,
        0x1006,
        Some(0),
        "get",
    ));
    pad(&mut f);
    f.extend_from_slice(&crate::codeview::vfptr());
    pad(&mut f);
    f.extend_from_slice(&crate::codeview::stmember(3, 0x74, "s_count"));
    pad(&mut f);
    f.extend_from_slice(&crate::codeview::nested(0x1008, "Color"));
    pad(&mut f);
    t.push(type_record(0x1203, &f));
    let class = |count: u16, prop: u16, field: u32, size: u16, name: &str| {
        let mut d = count.to_le_bytes().to_vec();
        d.extend_from_slice(&prop.to_le_bytes());
        d.extend_from_slice(&field.to_le_bytes());
        d.extend_from_slice(&[0; 8]);
        d.extend_from_slice(&size.to_le_bytes());
        d.extend_from_slice(&st(name));
        d
    };
    t.push(type_record(0x1005, &class(0, 0x80, 0, 0, "S")));
    t.push(type_record(0x1005, &class(4, 0, 0x1002, 8, "S")));
    let mut ptr = 0x1003u32.to_le_bytes().to_vec();
    ptr.extend_from_slice(&0x0000_000au32.to_le_bytes());
    t.push(type_record(0x1002, &ptr));
    let mut mf = Vec::new();
    for x in [0x74u32, 0x1003, 0x1005] {
        mf.extend_from_slice(&x.to_le_bytes());
    }
    mf.extend_from_slice(&[0x0b, 0, 0, 0]);
    mf.extend_from_slice(&0x1000u32.to_le_bytes());
    mf.extend_from_slice(&0u32.to_le_bytes());
    t.push(type_record(0x1009, &mf));
    let mut e = Vec::new();
    e.extend_from_slice(&crate::codeview::enumerate(0, "RED"));
    pad(&mut e);
    let mut blue = 0x0403u16.to_le_bytes().to_vec();
    blue.extend_from_slice(&3u16.to_le_bytes());
    blue.extend_from_slice(&0x8001u16.to_le_bytes());
    blue.extend_from_slice(&(-3i16).to_le_bytes());
    blue.extend_from_slice(&st("BLUE"));
    e.extend_from_slice(&blue);
    pad(&mut e);
    t.push(type_record(0x1203, &e));
    let mut en = 2u16.to_le_bytes().to_vec();
    en.extend_from_slice(&0u16.to_le_bytes());
    en.extend_from_slice(&0x74u32.to_le_bytes());
    en.extend_from_slice(&0x1007u32.to_le_bytes());
    en.extend_from_slice(&st("Color"));
    t.push(type_record(0x1007, &en));
    let mut arr = 0x70u32.to_le_bytes().to_vec();
    arr.extend_from_slice(&0x22u32.to_le_bytes());
    arr.extend_from_slice(&16u16.to_le_bytes());
    arr.extend_from_slice(&st(""));
    t.push(type_record(0x1003, &arr));
    let mut un = 1u16.to_le_bytes().to_vec();
    un.extend_from_slice(&0u16.to_le_bytes());
    un.extend_from_slice(&0x1002u32.to_le_bytes());
    un.extend_from_slice(&4u16.to_le_bytes());
    un.extend_from_slice(&st("__unnamed"));
    t.push(type_record(0x1006, &un));
    let mut m = 0x74u32.to_le_bytes().to_vec();
    m.extend_from_slice(&1u16.to_le_bytes());
    t.push(type_record(0x1001, &m));
    t.push(type_record(0x000a, &[1, 0, 0x01]));
    t
}

fn pad(v: &mut Vec<u8>) {
    while !v.len().is_multiple_of(4) {
        let k = 4 - v.len() % 4;
        v.push(0xf0 | u8::try_from(k).unwrap_or(0));
    }
}

fn tpi(records: &[Vec<u8>], hash_stream: u16) -> (Vec<u8>, Vec<u8>) {
    let mut hash = Buf::new();
    for r in records {
        hash.u16(n16(us(lhash(r, 4096))));
    }
    let vals = hash.len();
    hash.u32(0x1000).u32(0);
    let gprec: usize = records.iter().map(Vec::len).sum();
    let mut s = Buf::new();
    s.u32(19_961_031)
        .u32(56)
        .u32(0x1000)
        .u32(0x1000 + n32(records.len()))
        .u32(n32(gprec))
        .u16(hash_stream)
        .u16(0xffff)
        .u32(2)
        .u32(4096)
        .u32(0)
        .u32(n32(vals))
        .u32(n32(vals))
        .u32(8)
        .u32(n32(vals + 8))
        .u32(0);
    for r in records {
        s.bytes(r);
    }
    (s.into_vec(), hash.into_vec())
}

fn contribution(isect: u16, off: u32, size: u32, chars: u32, imod: u16) -> Vec<u8> {
    let mut b = Buf::new();
    b.u16(isect)
        .u16(0)
        .u32(off)
        .u32(size)
        .u32(chars)
        .u16(imod)
        .u16(0)
        .u32(0)
        .u32(0);
    b.into_vec()
}

fn module_record(
    c: &[u8],
    stream: u16,
    syms: usize,
    lines: usize,
    ni_src: u32,
    name: &str,
    obj: &str,
) -> Vec<u8> {
    let mut m = Buf::new();
    m.u32(0)
        .bytes(c)
        .u16(0)
        .u16(stream)
        .u32(n32(syms))
        .u32(n32(lines))
        .u32(0)
        .u16(0)
        .u16(0)
        .u32(0)
        .u32(ni_src)
        .u32(0)
        .cstr(name)
        .cstr(obj)
        .align(4);
    m.into_vec()
}

#[must_use]
pub fn sample() -> Sample {
    let dll = crate::sample::sample();
    let image = dll.pe.build();
    let layout = dll.pe.layout();
    let table_at = us(layout.e_lfanew) + 24 + 0xe0;
    let sections = image
        .get(table_at..table_at + 40 * dll.pe.sections.len())
        .unwrap_or_default()
        .to_vec();
    let chars = |i: usize| {
        sections
            .get(40 * i + 36..40 * i + 40)
            .and_then(|b| <[u8; 4]>::try_from(b).ok())
            .map_or(0, u32::from_le_bytes)
    };
    let mut a = Region::default();
    let mut objname = 0u32.to_le_bytes().to_vec();
    objname.extend_from_slice(&st("C:\\work\\Release\\a.obj"));
    a.push(sym(S_OBJNAME_ST, &objname));
    let mut c2 = vec![0u8; 18];
    c2.extend_from_slice(&st("Microsoft (R) 32-bit C/C++ Optimizing Compiler"));
    for s in [
        "cwd",
        "C:\\work",
        "cl",
        "C:\\vc\\bin\\cl.exe",
        "cmd",
        "-c -Zi",
        "src",
        "C:\\work\\a.cpp",
        "pdb",
        "C:\\work\\Release\\vc60.pdb",
    ] {
        c2.extend_from_slice(s.as_bytes());
        c2.push(0);
    }
    c2.push(0);
    c2.extend_from_slice(&[0x77, 0x10]);
    a.push(sym(S_COMPILE2_ST, &c2));
    let f = a.push(sym(S_GPROC32_ST, &proc_body("_f", 1, 0, 0x20, 0x1001)));
    let mut bp = (-4i32).to_le_bytes().to_vec();
    bp.extend_from_slice(&0x74u32.to_le_bytes());
    bp.extend_from_slice(&st("x"));
    a.push(sym(S_BPREL32_ST, &bp));
    let mut blk = vec![0u8; 8];
    blk.extend_from_slice(&8u32.to_le_bytes());
    blk.extend_from_slice(&8u32.to_le_bytes());
    blk.extend_from_slice(&1u16.to_le_bytes());
    blk.extend_from_slice(&st(""));
    let b = a.push(sym(S_BLOCK32_ST, &blk));
    let mut lab = 0xcu32.to_le_bytes().to_vec();
    lab.extend_from_slice(&1u16.to_le_bytes());
    lab.push(0);
    lab.extend_from_slice(&st("$L1"));
    a.push(sym(S_LABEL32_ST, &lab));
    let b_end = a.push(sym(S_END, &[]));
    let mut reg = 0x74u32.to_le_bytes().to_vec();
    reg.extend_from_slice(&17u16.to_le_bytes());
    reg.extend_from_slice(&st("r"));
    a.push(sym(S_REGISTER_ST, &reg));
    let f_end = a.push(sym(S_END, &[]));
    let g = a.push(sym(S_LPROC32_ST, &proc_body("_g", 1, 0x20, 0x10, 0x1001)));
    let g_end = a.push(sym(S_END, &[]));
    let mut adj = vec![0u8; 12];
    adj.extend_from_slice(&0x30u32.to_le_bytes());
    adj.extend_from_slice(&1u16.to_le_bytes());
    adj.extend_from_slice(&5u16.to_le_bytes());
    adj.push(1);
    adj.extend_from_slice(&st("[thunk]:_f`adjustor{4}'"));
    adj.extend_from_slice(&(-4i16).to_le_bytes());
    adj.extend_from_slice(&st("_f"));
    let t1 = a.push(sym(S_THUNK32_ST, &adj));
    let t1_end = a.push(sym(S_END, &[]));
    let mut vc = vec![0u8; 12];
    vc.extend_from_slice(&0x38u32.to_le_bytes());
    vc.extend_from_slice(&1u16.to_le_bytes());
    vc.extend_from_slice(&6u16.to_le_bytes());
    vc.push(2);
    vc.extend_from_slice(&st("_vcall"));
    vc.extend_from_slice(&8u16.to_le_bytes());
    let t2 = a.push(sym(S_THUNK32_ST, &vc));
    let t2_end = a.push(sym(S_END, &[]));
    let mut udt = 0x1004u32.to_le_bytes().to_vec();
    udt.extend_from_slice(&st("S"));
    a.push(sym(S_UDT_ST, &udt));
    let mut cn = 0x74u32.to_le_bytes().to_vec();
    cn.extend_from_slice(&7u16.to_le_bytes());
    cn.extend_from_slice(&st("SEVEN"));
    a.push(sym(S_CONSTANT_ST, &cn));
    let s_static = a.push(sym(S_LDATA32_ST, &data_body("_s_static", 3, 0x10, 0x74)));
    a.set(f, 4, f_end);
    a.set(f, 8, g);
    a.set(b, 0, f);
    a.set(b, 4, b_end);
    a.set(g, 4, g_end);
    a.set(t1, 4, t1_end);
    a.set(t2, 4, t2_end);
    let mut bm = Region::default();
    let mut objb = 0u32.to_le_bytes().to_vec();
    objb.extend_from_slice(&st("C:\\work\\Release\\b.obj"));
    bm.push(sym(S_OBJNAME_ST, &objb));
    bm.push(sym(S_COMPILE, &[3, 0, 0, 0, 1, b'x']));
    let main = bm.push(sym(S_GPROC32_ST, &proc_body("_main", 1, 0x40, 0x10, 0)));
    let main_end = bm.push(sym(S_END, &[]));
    let mut lab2 = 0x50u32.to_le_bytes().to_vec();
    lab2.extend_from_slice(&1u16.to_le_bytes());
    lab2.push(0);
    lab2.extend_from_slice(&st("$L99"));
    bm.push(sym(S_LABEL32_ST, &lab2));
    bm.set(main, 4, main_end);
    let mut rec = Vec::new();
    let mut glob: Vec<(u32, &str)> = Vec::new();
    let mut pubs: Vec<(u32, &str, u16, u32)> = Vec::new();
    for (name, seg, off) in [("_f", 1u16, 0u32), ("_main", 1, 0x40), ("_g_count", 3, 0)] {
        let at = n32(rec.len());
        rec.extend_from_slice(&sym(S_PUB32_ST, &data_body(name, seg, off, 0)));
        pubs.push((at, name, seg, off));
    }
    let mut a_refs = Vec::new();
    let at = n32(rec.len());
    rec.extend_from_slice(&sym(S_GDATA32_ST, &data_body("_g_count", 3, 0, 0x74)));
    glob.push((at, "_g_count"));
    a_refs.push(at);
    let at = n32(rec.len());
    let mut gu = 0x1008u32.to_le_bytes().to_vec();
    gu.extend_from_slice(&st("Color"));
    rec.extend_from_slice(&sym(S_UDT_ST, &gu));
    glob.push((at, "Color"));
    a_refs.push(at);
    let at = n32(rec.len());
    let mut gc = 0x75u32.to_le_bytes().to_vec();
    gc.extend_from_slice(&0x8004u16.to_le_bytes());
    gc.extend_from_slice(&0x0001_0000u32.to_le_bytes());
    gc.extend_from_slice(&st("LIMIT"));
    rec.extend_from_slice(&sym(S_CONSTANT_ST, &gc));
    glob.push((at, "LIMIT"));
    for (kind, ib, imod, name) in [
        (S_PROCREF_ST, f, 1u16, "_f"),
        (S_LPROCREF_ST, g, 1, "_g"),
        (S_PROCREF_ST, t1, 1, "[thunk]:_f`adjustor{4}'"),
        (S_PROCREF_ST, t2, 1, "_vcall"),
        (S_DATAREF_ST, s_static, 1, "_s_static"),
        (S_PROCREF_ST, main, 2, "_main"),
    ] {
        let at = n32(rec.len());
        rec.extend_from_slice(&refsym(kind, ib, imod, name));
        glob.push((at, name));
        if imod == 1 {
            a_refs.push(at);
        }
    }
    let globals = gsi(&glob);
    let pub_names: Vec<(u32, &str)> = pubs.iter().map(|p| (p.0, p.1)).collect();
    let pub_hash = gsi(&pub_names);
    let mut by_addr = pubs.clone();
    by_addr.sort_by_key(|p| (p.2, p.3, p.1.to_string()));
    let mut psi = Buf::new();
    psi.u32(n32(pub_hash.len()))
        .u32(n32(4 * by_addr.len()))
        .zeros(20)
        .bytes(&pub_hash);
    for p in &by_addr {
        psi.u32(p.0);
    }
    let a_lines = c11(
        1,
        &[
            (
                "C:\\work\\a.cpp".into(),
                vec![
                    (0, 10),
                    (4, 11),
                    (8, 12),
                    (0x20, 20),
                    (0x24, 21),
                    (0x30, 30),
                ],
            ),
            ("C:\\work\\a.h".into(), vec![(0xc, 5)]),
        ],
    );
    let b_lines = c11(1, &[("C:\\work\\b.cpp".into(), vec![(0x40, 3), (0x48, 4)])]);
    let mut module_a = 2u32.to_le_bytes().to_vec();
    module_a.extend_from_slice(&a.bytes);
    let a_syms = module_a.len();
    module_a.extend_from_slice(&a_lines);
    module_a.extend_from_slice(&n32(4 * a_refs.len()).to_le_bytes());
    for r in &a_refs {
        module_a.extend_from_slice(&r.to_le_bytes());
    }
    let mut module_b = 2u32.to_le_bytes().to_vec();
    module_b.extend_from_slice(&bm.bytes);
    let b_syms = module_b.len();
    module_b.extend_from_slice(&b_lines);
    let (tpi_bytes, tpi_hash) = tpi(&types(), 4);
    let (ec, ec_ids) = name_table(&["C:\\work\\a.cpp", "C:\\work\\Release\\vc60.pdb"]);
    let (names, _) = name_table(&[]);
    let ca = contribution(1, 0, 0x40, chars(0), 0);
    let cb = contribution(1, 0x40, 0x10, chars(0), 1);
    let cc = contribution(3, 0, 0x10, chars(2), 2);
    let mut modi = Vec::new();
    modi.extend_from_slice(&module_record(
        &ca,
        11,
        a_syms,
        a_lines.len(),
        ec_ids.first().copied().unwrap_or(0),
        "C:\\work\\Release\\a.obj",
        "C:\\work\\Release\\a.obj",
    ));
    modi.extend_from_slice(&module_record(
        &cb,
        12,
        b_syms,
        b_lines.len(),
        0,
        "C:\\work\\Release\\b.obj",
        "C:\\work\\Release\\b.obj",
    ));
    modi.extend_from_slice(&module_record(
        &cc,
        0xffff,
        0,
        0,
        0,
        "crt0.obj",
        "C:\\vc\\lib\\LIBCMT.lib",
    ));
    let mut sc = (0xeffe_0000u32 + 19_970_605).to_le_bytes().to_vec();
    for c in [&ca, &cb, &cc] {
        sc.extend_from_slice(c);
    }
    let nseg = n16(dll.pe.sections.len() + 1);
    let mut segmap = Buf::new();
    segmap.u16(nseg).u16(nseg);
    for (i, s) in dll.pe.sections.iter().enumerate() {
        segmap
            .u16(0x0109)
            .u16(0)
            .u16(0)
            .u16(n16(i + 1))
            .u16(0xffff)
            .u16(0xffff)
            .u32(0)
            .u32(n32(s.data.len()));
    }
    segmap
        .u16(0x0208)
        .u16(0)
        .u16(0)
        .u16(0)
        .u16(0xffff)
        .u16(0xffff)
        .u32(0)
        .u32(u32::MAX);
    let files = {
        let mods: [&[&str]; 3] = [
            &["C:\\work\\a.cpp", "C:\\work\\a.h"],
            &["C:\\work\\b.cpp"],
            &[],
        ];
        let mut b = Buf::new();
        b.u16(3).u16(3);
        let mut first = 0u16;
        for m in mods {
            b.u16(first);
            first += n16(m.len());
        }
        for m in mods {
            b.u16(n16(m.len()));
        }
        let mut names = Vec::new();
        let mut offs = Vec::new();
        for m in mods {
            for n in m {
                offs.push(n32(names.len()));
                names.extend_from_slice(&st(n));
            }
        }
        for o in offs {
            b.u32(o);
        }
        b.bytes(&names).align(4);
        b.into_vec()
    };
    let dbg: [u16; 6] = [8, 0xffff, 0xffff, 0xffff, 0xffff, 13];
    let mut dbi = Buf::new();
    dbi.u32(u32::MAX)
        .u32(19_970_606)
        .u32(AGE)
        .u16(5)
        .u16(0)
        .u16(6)
        .u16(0)
        .u16(7)
        .u16(0);
    for n in [modi.len(), sc.len(), segmap.len(), files.len(), 0] {
        dbi.u32(n32(n));
    }
    dbi.u32(0)
        .u32(12)
        .u32(n32(ec.len()))
        .u16(0)
        .u16(0x14c)
        .u32(0);
    dbi.bytes(&modi)
        .bytes(&sc)
        .bytes(segmap.as_slice())
        .bytes(&files)
        .bytes(&ec);
    for d in dbg {
        dbi.u16(d);
    }
    let cmd = "\"C:\\vc\\bin\\LINK.EXE\" -debug -out:\"C:\\work\\Release\\sample.dll\"";
    let mut link = Buf::new();
    link.zeros(20).cstr("C:\\work");
    let at_cmd = link.len();
    link.cstr(cmd);
    let n = link.len();
    link.put_u32(0, n32(n));
    link.put_u32(4, 1);
    link.put_u32(8, 20);
    link.put_u32(12, n32(at_cmd));
    link.put_u32(16, n32(cmd.find("-out:").unwrap_or(0) + 5));
    let mut info = Buf::new();
    info.u32(19_970_604).u32(SIGNATURE).u32(AGE);
    let key_names = b"/LinkInfo\0/names\0";
    info.u32(n32(key_names.len())).bytes(key_names);
    let h = |s: &[u8]| us(lhash(s, 0) & 0xffff) % 4;
    let (hl, hn) = (h(b"/LinkInfo"), h(b"/names"));
    let mut slots = [None; 4];
    if let Some(s) = slots.get_mut(hl) {
        *s = Some((0u32, 9u32));
    }
    let mut j = hn;
    while slots.get(j).is_some_and(Option::is_some) {
        j = (j + 1) % 4;
    }
    if let Some(s) = slots.get_mut(j) {
        *s = Some((10, 10));
    }
    let present = slots
        .iter()
        .enumerate()
        .filter(|(_, s)| s.is_some())
        .fold(0u32, |w, (i, _)| w | (1 << i));
    info.u32(2).u32(4).u32(1).u32(present).u32(0);
    for (k, v) in slots.iter().flatten() {
        info.u32(*k).u32(*v);
    }
    info.u32(11);
    let mut fpo = Buf::new();
    fpo.u32(0).u32(0x20).u32(1).u16(0).u16(0x0100);
    let streams = vec![
        Vec::new(),
        info.into_vec(),
        tpi_bytes,
        dbi.into_vec(),
        tpi_hash,
        globals,
        psi.into_vec(),
        rec,
        fpo.into_vec(),
        link.into_vec(),
        names,
        module_a,
        module_b,
        sections.clone(),
    ];
    Sample {
        bytes: small_msf(&streams),
        sections,
        image,
        procs: vec![
            ("_f".into(), dll.text, 0x20),
            ("_g".into(), dll.text + 0x20, 0x10),
            ("_main".into(), dll.text + 0x40, 0x10),
        ],
        publics: vec![
            ("_f".into(), dll.text),
            ("_main".into(), dll.text + 0x40),
            ("_g_count".into(), dll.data),
        ],
        modules: 3,
        line_rows: 9,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_sample_is_a_whole_small_msf() {
        let s = sample();
        assert!(s.bytes.starts_with(MSF20_MAGIC));
        assert_eq!(s.bytes.len() % PAGE, 0);
        assert_eq!(lhash(b"__lc_codepage", 4096), 3);
        assert_eq!(s.sections.len() % 40, 0);
    }
}
