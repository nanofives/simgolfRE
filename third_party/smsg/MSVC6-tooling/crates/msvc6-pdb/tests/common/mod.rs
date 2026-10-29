#![allow(dead_code, clippy::unwrap_used)]

use msvc6_pdb::write::{Global, Module, Proc, Public, Spec, Udt, build};

pub fn linked() -> Vec<u8> {
    msvc6_fixture::pdb::sample().bytes
}

pub fn authored() -> Vec<u8> {
    let s = msvc6_fixture::pdb::sample();
    let sections: Vec<[u8; 40]> = s.sections.as_chunks::<40>().0.to_vec();
    let proc_ = |name: &str, off: u32| Proc {
        name: name.to_string(),
        isect: 1,
        off,
        len: 0x10,
        ti: 0x1001,
        flags: 0,
    };
    let spec = Spec {
        signature: 0x1234_5678,
        age: 2,
        sections,
        modules: vec![
            Module {
                name: "a".into(),
                obj: "a.obj".into(),
                procs: vec![proc_("alpha", 0), proc_("beta", 0x10)],
            },
            Module {
                name: "b".into(),
                obj: "b.obj".into(),
                procs: vec![proc_("gamma", 0x20)],
            },
        ],
        publics: vec![Public {
            name: "_alpha".into(),
            isect: 1,
            off: 0,
        }],
        globals: vec![Global {
            name: "g_count".into(),
            isect: 3,
            off: 0x30,
            ti: 0x74,
        }],
        udts: vec![Udt {
            name: "size_t".into(),
            ti: 0x75,
        }],
        types: vec![
            msvc6_codeview::codec::type_record(0x1201, &[0, 0, 0, 0]).unwrap(),
            msvc6_codeview::codec::type_record(0x1008, &[3, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0])
                .unwrap(),
        ],
    };
    build(&spec).unwrap().bytes
}

pub const HEADER: &str = "typedef unsigned char u8;
typedef struct node { u8 tag; int count; struct node *next; char name[6]; } node;
#pragma pack(push, 2)
struct packed { u8 a; unsigned long b; };
#pragma pack(pop)
union cell { int i; float f; node *n; };
typedef int (__stdcall *visit_fn)(node *, void *);
struct table { struct packed head; union cell cells[4]; visit_fn visit; FILE *log; };
";

pub fn typed() -> (Vec<u8>, msvc6_pdb::types::Types) {
    use msvc6_pdb::types::{Types, parse_proto, parse_type};
    let s = msvc6_fixture::pdb::sample();
    let sections: Vec<[u8; 40]> = s.sections.as_chunks::<40>().0.to_vec();
    let mut t = Types::default();
    t.load_headers(&["types.h"], &|_| Ok(HEADER.to_string()))
        .unwrap();
    t.emit_headers().unwrap();
    let mut procs = Vec::new();
    for (i, p) in [
        "node * __cdecl node_find(node *head, const char *name)",
        "int __thiscall table_walk(struct table *t, visit_fn f, void *ctx)",
        "void __stdcall log_line(FILE *f, const char *fmt, ...)",
    ]
    .iter()
    .enumerate()
    {
        let proto = parse_proto(p).unwrap();
        let ti = t.procedure(&proto.func).unwrap().unwrap();
        procs.push(Proc {
            name: proto.name,
            isect: 1,
            off: u32::try_from(i * 0x10).unwrap(),
            len: 0x10,
            ti,
            flags: 0,
        });
    }
    let g = t.ti(&parse_type("struct table").unwrap()).unwrap().unwrap();
    let udts = t
        .udts
        .iter()
        .map(|(name, ti)| Udt {
            name: name.clone(),
            ti: *ti,
        })
        .collect();
    let spec = Spec {
        signature: 0x0bad_cafe,
        age: 1,
        sections,
        modules: vec![Module {
            name: "typed".into(),
            obj: "typed.obj".into(),
            procs,
        }],
        publics: Vec::new(),
        globals: vec![Global {
            name: "g_table".into(),
            isect: 3,
            off: 0,
            ti: g,
        }],
        udts,
        types: t.records.clone(),
    };
    (build(&spec).unwrap().bytes, t)
}
