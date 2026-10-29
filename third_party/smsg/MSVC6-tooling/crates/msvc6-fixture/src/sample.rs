use crate::buf::n32;
use crate::pe::{
    DIR_BASERELOC, DIR_DEBUG, DIR_EXPORT, DIR_IAT, DIR_IMPORT, DIR_RESOURCE, Pe, SCN_CODE,
    SCN_DATA, SCN_RDATA, SCN_RELOC, SCN_RSRC, comp_id,
};
use crate::tables::{
    DebugEntry, Dll, Export, Id, Import, Resource, base_relocs, codeview_nb10, debug_directory,
    edata, idata, rsrc,
};

pub const TIMESTAMP: u32 = 0x3b9a_ca00;
pub const EXPORT_TIMESTAMP: u32 = 0x3b9a_c7a8;
pub const PDB_SIGNATURE: u32 = 0x3b9a_c5b0;
pub const PDB_AGE: u32 = 7;
pub const PDB_PATH: &str = "C:\\work\\Release\\sample.pdb";

#[derive(Debug, Clone)]
pub struct Sample {
    pub pe: Pe,
    pub text: u32,
    pub rdata: u32,
    pub nb10: u32,
    pub edata: u32,
    pub data: u32,
    pub idata: u32,
    pub ilt: Vec<u32>,
    pub iat: Vec<u32>,
    pub names: Vec<u32>,
    pub rsrc: u32,
    pub reloc: u32,
    pub reloc_sites: Vec<u32>,
}

pub fn reserve(n: usize) -> u32 {
    40u32.saturating_add(4u32.saturating_mul(n32(n / 5)))
}

pub fn dlls() -> Vec<Dll> {
    vec![
        Dll::named("KERNEL32.dll", &["ExitProcess", "GetVersion", "HeapAlloc"]),
        Dll {
            name: "WS2_32.dll".into(),
            imports: vec![Import::Ordinal(23), Import::Name(5, "send".into())],
        },
    ]
}

pub fn code() -> Vec<u8> {
    let mut c = vec![0x55, 0x8b, 0xec, 0xa1, 0, 0, 0, 0, 0x5d, 0xc3];
    c.resize(0x30, 0x90);
    c.extend_from_slice(&[0x33, 0xc0, 0xc3]);
    c.resize(0x40, 0xcc);
    c
}

pub fn sample() -> Sample {
    let mut pe = Pe::dll();
    pe.timestamp = TIMESTAMP;
    pe.rich = vec![
        (comp_id(0x0b, 8168), 12),
        (comp_id(0x0a, 8168), 40),
        (comp_id(0x01, 0), 5),
        (comp_id(0x04, 8447), 1),
    ];
    let text = pe.push(".text", SCN_CODE, code());
    pe.entry = text;
    let rdata = pe.next_rva();
    let nb10 = codeview_nb10(PDB_SIGNATURE, PDB_AGE, PDB_PATH);
    let nb10_rva = rdata + 28;
    let dbg = debug_directory(&[DebugEntry {
        kind: 2,
        timestamp: EXPORT_TIMESTAMP,
        size: n32(nb10.len()),
        rva: nb10_rva,
    }]);
    let edata_rva = rdata + 0x60;
    let ed = edata(
        edata_rva,
        "sample.dll",
        1,
        EXPORT_TIMESTAMP,
        &[
            Export {
                name: Some("beta".into()),
                target: text + 0x30,
            },
            Export {
                name: None,
                target: text + 0x33,
            },
            Export {
                name: Some("alpha".into()),
                target: text,
            },
        ],
    );
    let mut rd = dbg.clone();
    rd.extend_from_slice(&nb10);
    rd.resize(0x60, 0);
    rd.extend_from_slice(&ed);
    pe.push(".rdata", SCN_RDATA, rd);
    pe.dir(DIR_DEBUG, rdata, 28);
    pe.dir(DIR_EXPORT, edata_rva, n32(ed.len()));
    let data = pe.push_virtual(".data", SCN_DATA, vec![7u8; 0x80], 0x3000);
    let idata_rva = pe.next_rva();
    let t = idata(idata_rva, &dlls(), &reserve);
    pe.push(".idata", SCN_DATA, t.bytes.clone());
    pe.dir(DIR_IMPORT, t.import_dir.0, t.import_dir.1);
    pe.dir(DIR_IAT, t.iat_dir.0, t.iat_dir.1);
    let rsrc_rva = pe.next_rva();
    let rs = rsrc(
        rsrc_rva,
        &[
            Resource {
                type_id: Id::Ordinal(3),
                name: Id::Ordinal(1),
                lang: 1033,
                codepage: 0,
                data: vec![0x28; 40],
            },
            Resource {
                type_id: Id::Ordinal(14),
                name: Id::Ordinal(102),
                lang: 1033,
                codepage: 0,
                data: vec![0, 0, 1, 0, 1, 0],
            },
            Resource {
                type_id: Id::Name("WAVE".into()),
                name: Id::Name("CLICK".into()),
                lang: 0,
                codepage: 1252,
                data: b"RIFF....".to_vec(),
            },
        ],
    );
    pe.push(".rsrc", SCN_RSRC, rs.clone());
    pe.dir(DIR_RESOURCE, rsrc_rva, n32(rs.len()));
    let reloc_rva = pe.next_rva();
    let sites = vec![text + 4, text + 0x1004];
    let rl = base_relocs(&sites, 3);
    pe.push(".reloc", SCN_RELOC, rl.clone());
    pe.dir(DIR_BASERELOC, reloc_rva, n32(rl.len()));
    Sample {
        pe,
        text,
        rdata,
        nb10: nb10_rva,
        edata: edata_rva,
        data,
        idata: idata_rva,
        ilt: t.ilt,
        iat: t.iat,
        names: t.names,
        rsrc: rsrc_rva,
        reloc: reloc_rva,
        reloc_sites: sites,
    }
}

pub const SLACK_BYTES: [u8; 8] = [0x11, 0x22, 0x00, 0x00, 0x33, 0x00, 0x44, 0x55];
pub const PAD_STUB: [u8; 6] = [0xff, 0x15, 0x00, 0x30, 0x00, 0x10];
pub const TAIL_BYTES: &[u8; 8] = b"tailtxt\0";

#[derive(Debug, Clone)]
pub struct PostLinked {
    pub sample: Sample,
    pub reference: Vec<u8>,
    pub ours: Vec<u8>,
    pub slack: usize,
    pub pad: usize,
    pub size_of_code: usize,
    pub stamps: usize,
    pub tail: usize,
}

fn put(b: &mut [u8], at: usize, v: &[u8]) {
    if let Some(s) = b.get_mut(at..at.saturating_add(v.len())) {
        s.copy_from_slice(v);
    }
}

#[must_use]
pub fn postlinked() -> PostLinked {
    let s = sample();
    let ours = s.pe.build();
    let l = s.pe.layout();
    let us = |n: u32| usize::try_from(n).unwrap_or(usize::MAX);
    let opt = us(l.e_lfanew).saturating_add(24);
    let slack = us(l.headers_end).saturating_add(8);
    let pad = us(s.pe.rva_to_file(s.text + 0x10).unwrap_or(0));
    let size_of_code = opt.saturating_add(4);
    let stamps = us(s.pe.rva_to_file(s.idata).unwrap_or(0)).saturating_add(4);
    let rd = l.sections.get(1).copied().unwrap_or(crate::pe::Placed {
        rva: 0,
        virt_size: 0,
        raw_off: 0,
        raw_size: 0,
    });
    let tail = us(rd.raw_off).saturating_add(us(rd.virt_size));
    let vsize_field = opt
        .saturating_add(0xe0)
        .saturating_add(40)
        .saturating_add(8);
    let mut r = ours.clone();
    put(&mut r, slack, &SLACK_BYTES);
    put(&mut r, pad, &PAD_STUB);
    put(&mut r, size_of_code, &0x1000u32.to_le_bytes());
    put(&mut r, stamps, &[0xff; 8]);
    put(&mut r, tail, TAIL_BYTES);
    put(
        &mut r,
        vsize_field,
        &rd.virt_size.saturating_add(8).to_le_bytes(),
    );
    PostLinked {
        sample: s,
        reference: r,
        ours,
        slack,
        pad,
        size_of_code,
        stamps,
        tail,
    }
}

#[must_use]
pub fn versioned(major: u16, minor: u16, build: u16) -> Vec<u8> {
    let mut pe = Pe::dll();
    let rva = pe.next_rva();
    let rs = rsrc(
        rva,
        &[Resource {
            type_id: Id::Ordinal(16),
            name: Id::Ordinal(1),
            lang: 1033,
            codepage: 0,
            data: crate::tables::version_info(major, minor, build, 0),
        }],
    );
    pe.push(".rsrc", SCN_RSRC, rs.clone());
    pe.dir(DIR_RESOURCE, rva, n32(rs.len()));
    pe.build()
}
