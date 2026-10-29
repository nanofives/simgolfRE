#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_fixture::mac::{BLR, Function, Library, Pef};
use msvc6_image::pef;
use msvc6_match::annotations::{self, Row};
use msvc6_match::mac::feat::{
    Callee, Feat, api_key, float_key, int_key, is_noise, normalize_calls,
};
use msvc6_match::mac::names::{self, Context, MapRow};
use msvc6_match::mac::pair::{self, Fun, Rules, Side, code_check, pair_with};
use msvc6_match::mac::side::{self, Ours, Source, SourceKind};
use msvc6_match::mac::types;
use msvc6_match::mac::{Decisions, Inputs, decide, pair_all, ppc};
use msvc6_match::mangle::{self, SigLevel};
use std::collections::{BTreeMap, BTreeSet, HashMap};

#[test]
fn keys_name_constants_floats_and_apis_across_sides() {
    assert_eq!(int_key(0x123).as_deref(), Some("i:291"));
    assert_eq!(int_key(0x100), None);
    assert_eq!(int_key(0xffff), None);
    assert_eq!(int_key(-640).as_deref(), Some("i:-640"));
    assert_eq!(float_key(f64::from(0.1f32)), float_key(0.1));
    assert_eq!(float_key(1.0), None);
    assert_eq!(api_key("user32.dll!SetRect"), "setrect");
    assert_eq!(api_key("SetRect_Win32"), "setrect");
    assert_eq!(api_key("_strcpy"), "strcpy");
    assert_eq!(api_key(".PtInRect"), "ptinrect");
    assert_eq!(api_key("_Znwm"), "op_new");
    assert!(is_noise("memcpy") && is_noise("cxa_throw") && !is_noise("setrect"));
    let mut f = Feat {
        calls: vec![
            Callee::Api("memcpy".into()),
            Callee::Api("setrect".into()),
            Callee::Api("getport".into()),
        ],
        ..Feat::default()
    };
    normalize_calls(&mut f, &["setrect".to_string()].into_iter().collect());
    assert_eq!(f.calls, [Callee::Api("setrect".into()), Callee::Indirect]);
}

fn toc_pef() -> pef::Pef {
    let mut data = vec![0u8; 0x40];
    data[0x24..0x24 + 13].copy_from_slice(b"Pool notice!!");
    data[0x10..0x14].copy_from_slice(&0x20u32.to_be_bytes());
    data[8..12].copy_from_slice(&4u32.to_be_bytes());
    data[12..16].copy_from_slice(&0x10u32.to_be_bytes());
    let bytes = Pef {
        functions: vec![
            Function {
                words: vec![
                    0x8062_0000,
                    0x3883_0004,
                    0x807f_0024,
                    0x4800_002d,
                    0x3860_1234,
                    BLR,
                ],
                name: ".get__4PoolFi".into(),
            },
            Function {
                words: vec![BLR],
                name: ".squeeze__4PoolFv".into(),
            },
        ],
        data,
        libraries: vec![Library {
            name: "InterfaceLib".into(),
            imports: vec!["NewPtr".into()],
        }],
        relocations: vec![0x0083],
        main: 8,
        ..Pef::default()
    }
    .build();
    pef::load(&bytes).unwrap()
}

#[test]
fn powerpc_features_follow_the_toc_to_strings_calls_fields_and_constants() {
    let p = toc_pef();
    assert_eq!(p.toc, 0x10);
    let f0 = &p.tables[0];
    let entry: HashMap<u32, usize> = p
        .tables
        .iter()
        .enumerate()
        .map(|(i, t)| (t.start, i))
        .collect();
    let ft = ppc::features(&p, f0.start, f0.at, 0, &entry, &|_| None);
    assert_eq!(ft.strs, ["Pool notice"]);
    assert!(ft.disps.contains(&0x24));
    assert_eq!(ft.calls, [Callee::Fn(1)]);
    assert!(ft.consts.contains("i:4660"));
    assert_eq!(ft.size, 6);
    assert_eq!(ppc::glue_import(&p, 0), None);
}

#[test]
fn a_pef_side_names_its_functions_and_the_runs_its_static_initialisers_close() {
    let bytes = Pef {
        functions: vec![
            Function {
                words: vec![BLR],
                name: ".open__4FileFPc".into(),
            },
            Function {
                words: vec![BLR],
                name: ".__sinit_file_cpp".into(),
            },
            Function {
                words: vec![BLR],
                name: ".draw__6WidgetFv".into(),
            },
            Function {
                words: vec![BLR],
                name: ".__sinit_WIDGET_CPP".into(),
            },
        ],
        data: vec![0; 0x20],
        main: 8,
        ..Pef::default()
    }
    .build();
    let s = side::pef_side("mac", &bytes, &BTreeSet::new()).unwrap();
    let files: Vec<&str> = s.funs.iter().map(|f| f.file.as_str()).collect();
    assert_eq!(
        files,
        [
            "run:file.cpp",
            "run:file.cpp",
            "run:WIDGET.CPP",
            "run:WIDGET.CPP"
        ]
    );
    assert_eq!(s.funs[0].sig.as_ref().unwrap().key(), "File::open(char*)");
}

fn fun(raw: &str, sig: Option<mangle::Sig>) -> Fun {
    Fun {
        raw: raw.to_string(),
        sig,
        ..Fun::default()
    }
}

fn mk(label: &str, funs: Vec<Fun>, feats: Vec<Feat>, msvc: bool) -> Side {
    let mut s = Side {
        label: label.to_string(),
        funs,
        feats,
        msvc,
        ..Side::default()
    };
    s.finish();
    s
}

fn feat(strs: &[&str], calls: Vec<Callee>) -> Feat {
    Feat {
        strs: strs.iter().map(|s| (*s).to_string()).collect(),
        calls,
        size: 20,
        ..Feat::default()
    }
}

#[test]
fn a_string_anchor_then_call_order_lands_under_the_gate() {
    let m = mk(
        "mac",
        vec![
            fun("get__4PoolFi", mangle::cw("get__4PoolFi")),
            fun("squeeze__4PoolFi", mangle::cw("squeeze__4PoolFi")),
            fun("shutdown__4PoolFv", mangle::cw("shutdown__4PoolFv")),
        ],
        vec![
            feat(&["Pool notice"], vec![Callee::Fn(1), Callee::Fn(2)]),
            feat(&[], vec![]),
            feat(&[], vec![]),
        ],
        false,
    );
    let o = mk(
        "ours",
        vec![
            fun("", mangle::parse_text("void *Arena::Alloc(int n)", None)),
            fun("", mangle::parse_text("void Arena::Compact(int n)", None)),
            fun("", mangle::parse_text("void Arena::Free()", None)),
        ],
        vec![
            feat(&["Pool notice"], vec![Callee::Fn(1), Callee::Fn(2)]),
            feat(&[], vec![]),
            feat(&[], vec![]),
        ],
        true,
    );
    let p = pair::pair(&m, &o, &[], true);
    assert_eq!(p.a2b.get(&0), Some(&0));
    assert_eq!(p.a2b.get(&1), Some(&1));
    assert_eq!(p.a2b.get(&2), Some(&2));
    assert!(p.why[&0].kinds().contains("STR"));
    assert!(p.why[&1].kinds().contains("CALL"));
    let bad = mk(
        "ours",
        vec![fun(
            "",
            mangle::parse_text("void *Arena::Alloc(int n, int m)", None),
        )],
        vec![feat(&["Pool notice"], vec![])],
        true,
    );
    let lone = mk(
        "mac",
        vec![fun("get__4PoolFi", mangle::cw("get__4PoolFi"))],
        vec![feat(&["Pool notice"], vec![])],
        false,
    );
    assert!(pair::pair(&lone, &bad, &[], true).a2b.is_empty());
}

#[test]
fn msvc_returns_contradict_a_name() {
    let s = mangle::cw("fill__6BufferFi").unwrap();
    let pops8 = Feat {
        this_ecx: Some(true),
        ret_pop: Some(8),
        ..Feat::default()
    };
    assert_eq!(code_check(&s, &pops8), SigLevel::Mismatch);
    let pops4 = Feat {
        this_ecx: Some(true),
        ret_pop: Some(4),
        ..Feat::default()
    };
    assert_eq!(code_check(&s, &pops4), SigLevel::Count);
    let free = mangle::cw("mix__Fii").unwrap();
    let reads_ecx = Feat {
        this_ecx: Some(true),
        ..Feat::default()
    };
    assert_eq!(code_check(&free, &reads_ecx), SigLevel::Mismatch);
}

fn twin_sides(second_caller: bool) -> (Side, Side) {
    let name = |s: &str| fun(s, mangle::cw(s));
    let text = |s: &str| fun("", mangle::parse_text(s, None));
    let mut mf = vec![
        name("load__4FileFPc"),
        name("first__4FileFv"),
        name("second__4FileFv"),
    ];
    let mut of = vec![
        text("int File::Load(char *p)"),
        text("void File::A()"),
        text("void File::B()"),
    ];
    let mut mfe = vec![
        feat(&["file loader banner"], vec![Callee::Fn(1), Callee::Fn(2)]),
        feat(&[], vec![]),
        feat(&[], vec![]),
    ];
    let mut ofe = mfe.clone();
    if second_caller {
        mf.push(name("save__4FileFPc"));
        of.push(text("int File::Save(char *p)"));
        mfe.push(feat(
            &["file saver banner"],
            vec![Callee::Fn(1), Callee::Fn(2)],
        ));
        ofe.push(feat(
            &["file saver banner"],
            vec![Callee::Fn(1), Callee::Fn(2)],
        ));
    }
    (mk("mac", mf, mfe, false), mk("ours", of, ofe, true))
}

#[test]
fn two_same_signature_callees_of_one_caller_need_a_second_link() {
    let (m, o) = twin_sides(false);
    let held = pair_with(&m, &o, &[], true, Rules::default());
    assert_eq!(held.a2b.len(), 1);
    assert_eq!(held.held_twins.len(), 2);
    let single = pair_with(
        &m,
        &o,
        &[],
        true,
        Rules {
            twins_need_two_links: false,
        },
    );
    assert_eq!(single.a2b.len(), 3);
    let (m2, o2) = twin_sides(true);
    let two = pair_with(&m2, &o2, &[], true, Rules::default());
    assert_eq!(two.a2b.len(), 4);
    assert_eq!((two.a2b.get(&1), two.a2b.get(&2)), (Some(&1), Some(&2)));
    assert!(two.why[&1].items.len() == 2);
}

fn ours_info(n: usize) -> Ours {
    Ours {
        label: "ours.exe".into(),
        kind: vec!["FUNCTION".into(); n],
        in_scope: vec![true; n],
        ..Ours::default()
    }
}

#[test]
fn decisions_apply_a_strong_pairing_and_name_the_class() {
    let name = |s: &str| fun(s, mangle::cw(s));
    let text = |s: &str| Fun {
        va: 0x1000,
        file: "src/arena.cpp".into(),
        ..fun("", mangle::parse_text(s, None))
    };
    let mac = mk(
        "demo",
        vec![
            name("__ct__4PoolFv"),
            name("get__4PoolFi"),
            name("put__4PoolFPv"),
            name("drop__4PoolFv"),
        ],
        vec![
            feat(&["pool ctor banner"], vec![]),
            feat(&["pool get banner"], vec![]),
            feat(&["pool put banner"], vec![]),
            feat(&["pool drop banner"], vec![]),
        ],
        false,
    );
    let ours = mk(
        "ours",
        vec![
            text("Arena::Arena()"),
            text("void *Arena::Alloc(int n)"),
            text("void Arena::Release(void *p)"),
            text("void Arena::Reset()"),
        ],
        vec![
            feat(&["pool ctor banner"], vec![]),
            feat(&["pool get banner"], vec![]),
            feat(&["pool put banner"], vec![]),
            feat(&["pool drop banner"], vec![]),
        ],
        true,
    );
    let info = ours_info(4);
    let sources = vec![Source {
        label: "demo".into(),
        family: "game".into(),
        rank: 1,
        kind: SourceKind::Pef,
        path: "/nonexistent".into(),
        twin: None,
    }];
    let out = pair_all(Inputs {
        sources: &sources,
        sides: vec![mac],
        twins: BTreeMap::new(),
        ours: &ours,
        info: &info,
        rules: Rules::default(),
    });
    let classes: BTreeSet<String> = ["Pool".to_string()].into_iter().collect();
    let attested = BTreeSet::new();
    let (rows, stale) = decide(
        &out,
        &sources,
        &ours,
        &info,
        &Decisions {
            context: Context {
                image: "ours.exe",
                attested: &attested,
                classes: &classes,
                class_families: &[],
            },
            prior: &[],
            measured: Some(
                "method\t0x1000\tbyte-cost\ta build moved bytes\nmethod\t0x9\tbyte-cost\tstale\n",
            ),
        },
    )
    .unwrap();
    assert_eq!(stale, ["method 0x9"]);
    let class = rows.iter().find(|r| r.kind == "class").unwrap();
    assert_eq!(
        (class.old.as_str(), class.new.as_str(), class.state.as_str()),
        ("Arena", "Pool", "APPLIED")
    );
    let alloc = rows.iter().find(|r| r.old == "Arena::Alloc(int)").unwrap();
    assert_eq!(alloc.new, "Pool::get(int)");
    assert_eq!(alloc.state, "RECORDED");
    assert_eq!(alloc.reason, "byte-cost");
    let macs: Vec<&MapRow> = rows.iter().filter(|r| r.kind == "mac").collect();
    assert_eq!(macs.len(), 4);
    let map = names::render_map(&rows, "# test\n");
    assert_eq!(names::parse_map(&map).unwrap().len(), rows.len());
    let direct = names::direct_table(&out.sides[0], &ours, &out.direct[0]);
    assert_eq!(direct.lines().count(), 5);
    assert!(
        names::summary(&rows)
            .iter()
            .any(|l| l.starts_with("class APPLIED evidence"))
    );
}

#[test]
fn sources_tables_parse_and_refuse_an_unknown_kind() {
    let s =
        side::parse_sources("# c\nb\tfam\t3\tmacho\t/x/b.macho\tpc\na\tfam\t4\tpef\t/x/a.pef\t-\n")
            .unwrap();
    assert_eq!(s.len(), 2);
    assert_eq!(s[0].twin.as_deref(), Some("pc"));
    assert!(s[1].twin.is_none());
    assert!(side::parse_sources("x\tf\t1\telf\t/p\n").is_err());
    assert!(side::parse_sources("x\tf\tone\tpef\t/p\n").is_err());
}

#[test]
fn annotation_rows_resolve_to_their_definition_heads() {
    let rows = annotations::parse(
        "image\tva\tkind\tpath\tnth\tanchor\tname\n\
         ours.exe\t0x401000\tFUNCTION\tsrc/a.cpp\t0\tint Arena::Alloc(int n,\t-\n\
         ours.exe\t0x401020\tFUNCTION\tsrc/a.h\t2\tvoid Reset() {}\t-\n\
         ours.exe\t0x401040\tLIBRARY\t-\t0\tlibrary:\tstrcpy\n",
    )
    .unwrap();
    assert_eq!(rows.len(), 3);
    let text = "// x\nint Arena::Alloc(int n,\n    int m)\n{\n}\n";
    let code = annotations::code_lines(text);
    let l = annotations::resolve(&rows[0], &code).unwrap();
    assert_eq!(l, 2);
    assert_eq!(
        annotations::definition_at(text, l),
        "int Arena::Alloc(int n,     int m)"
    );
    let header =
        "struct Arena {\n    void Reset() {}\n};\nstruct Pool {\n    void Reset() {}\n};\n";
    let code = annotations::code_lines(header);
    let l = annotations::resolve(&rows[1], &code).unwrap();
    assert_eq!(
        annotations::enclosing_class(header, l - 1).as_deref(),
        Some("Pool")
    );
    assert!(annotations::resolve(&rows[2], &code).is_err());
    assert!(annotations::parse("a\tb\n").is_err());
    let picked = side::choose_rows(
        vec![
            Row {
                image: "ours.exe".into(),
                va: 1,
                kind: "FUNCTION".into(),
                path: "lib/x.cpp".into(),
                nth: 0,
                anchor: "x".into(),
                name: String::new(),
            },
            Row {
                image: "ours.exe".into(),
                va: 1,
                kind: "FUNCTION".into(),
                path: "app/x.cpp".into(),
                nth: 0,
                anchor: "y".into(),
                name: String::new(),
            },
        ],
        "ours.exe",
        &["app/".to_string()],
    );
    assert_eq!(picked[&1].path, "app/x.cpp");
}

#[test]
fn parameter_types_meet_in_one_canonical_form() {
    let (q, p, k) = types::text_sig("Buffer::copy(Surface*, int, int, char const*) const").unwrap();
    assert_eq!(q, "Buffer::copy");
    assert_eq!(p, ["Surface*", "int", "int", "char const*"]);
    assert!(k);
    assert_eq!(types::canon_type("const char *"), "char const*");
    assert_eq!(
        types::canon_type("const unsigned char *"),
        "unsigned char const*"
    );
    assert_eq!(types::canon_type("class Buffer *"), "Buffer*");
    assert_eq!(
        types::canon_type("int (__cdecl*)(unsigned int,long)"),
        "int(*)(unsigned int,long)"
    );
    let (mp, mk) = types::msvc_sig("?open_read@Filemap@@QAEPAXPBDH@Z").unwrap();
    assert_eq!(mp, ["char const*", "int"]);
    assert!(!mk);
    let (q2, p2, _) = types::text_sig("List<Item*, unsigned char>::add(Item*)").unwrap();
    assert_eq!(types::class_of(&q2), "List<Item*, unsigned char>");
    assert_eq!(p2, ["Item*"]);
    assert_eq!(
        types::msvc_qualified("?open_read@Filemap@@QAEPAXPBDH@Z").as_deref(),
        Some("Filemap::open_read")
    );
    assert_eq!(
        types::difference_kinds(&["int".into()], false, &["int".into(), "int".into()], false),
        ["ARITY"]
    );
}

#[test]
fn a_type_difference_is_decided_by_its_kind_or_refused() {
    let text = "rva\tkind\told\tnew\tstate\tmangled\tevidence\n\
                0x2870\tfunction\tMap::OpenRead\tFilemap::open_read\tAPPLIED\t?OpenRead@Map@@QAEPAXPBDH@Z\tx\tFilemap::open_read(char*, int)\t-\n\
                0x2bd0\tfunction\tMap::Create\tFilemap::create\tAPPLIED\t?Create@Map@@QAEPAXPBDKH@Z\tx\tFilemap::create(char*, unsigned long, int)\t-\n\
                0x2e10\tfunction\tMap::Close\tFilemap::close\tAPPLIED\t?Close@Map@@QAEXXZ\tx\tFilemap::close()\t-\n";
    let none: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    let no_verdicts: BTreeMap<String, String> = BTreeMap::new();
    let t = types::table(&types::parse(text), &none, &no_verdicts);
    assert_eq!(t.bad.len(), 2);
    assert!(types::check(&t).is_err());
    assert!(
        t.text
            .contains("Filemap::close\tAPPLIED\t()\tFilemap::close()\tsame")
    );
    let v = types::parse_verdicts("# kinds\nchar const*=>char*\tRECORDED: callers pass literals\n");
    let t = types::table(&types::parse(text), &none, &v);
    assert!(t.bad.is_empty());
    assert_eq!(
        t.text
            .matches("char const*=>char*: callers pass literals")
            .count(),
        2
    );
    let applied = types::parse_verdicts("char const*=>char*\tAPPLIED\n");
    let owed = types::table(&types::parse(text), &none, &applied);
    assert_eq!(owed.bad.len(), 2);
    assert!(owed.bad.iter().all(|b| b.contains("decided APPLIED")));
    let mut lines = types::parse(
        "rva\tkind\told\tnew\tstate\tmangled\tevidence\n\
         -\tclass\tMap\tFilemap\tAPPLIED\t-\tx\n\
         0x2e10\tfunction\tMap::Close\tFilemap::close\tAPPLIED\t?Close@Map@@QAEXXZ\tx\n",
    );
    let r = types::fill(
        &mut lines,
        "Filemap::close()\t_ZN7Filemap5closeEv\nFilemap::flush()\t_ZN7Filemap5flushEv\n",
        &none,
    );
    assert_eq!((r.set, r.added), (1, 1));
    let back = types::render(&lines);
    assert!(back.contains("Filemap::close()"));
    assert!(back.contains("mac-only"));
}

#[test]
fn a_machine_side_reads_its_stubs_as_apis() {
    use msvc6_fixture::mac::{
        ATTR_PURE_INSTRUCTIONS, ATTR_SOME_INSTRUCTIONS, CPU_I386, MachO, MachSection, MachSymbol,
        N_EXT, N_SECT, SYMBOL_STUBS,
    };
    let mut text = vec![0x68, 0x00, 0x30, 0x00, 0x00, 0xe8];
    text.extend_from_slice(&(0x2000u32.wrapping_sub(0x1000 + 10)).to_le_bytes());
    text.extend_from_slice(&[0x59, 0xc3]);
    text.resize(0x20, 0x90);
    let m = MachO {
        cpu: CPU_I386,
        big_endian: false,
        sections: vec![
            MachSection {
                segment: "__TEXT".into(),
                name: "__text".into(),
                addr: 0x1000,
                data: text,
                flags: ATTR_PURE_INSTRUCTIONS | ATTR_SOME_INSTRUCTIONS,
                ..MachSection::default()
            },
            MachSection {
                segment: "__IMPORT".into(),
                name: "__jump_table".into(),
                addr: 0x2000,
                data: vec![0xf4; 5],
                flags: SYMBOL_STUBS | ATTR_SOME_INSTRUCTIONS,
                reserved1: 0,
                reserved2: 5,
            },
            MachSection {
                segment: "__DATA".into(),
                name: "__cstring".into(),
                addr: 0x3000,
                data: b"a string for the slice\0".to_vec(),
                ..MachSection::default()
            },
        ],
        symbols: vec![
            MachSymbol {
                name: "__Z4drawv".into(),
                ty: N_SECT | N_EXT,
                sect: 1,
                value: 0x1000,
            },
            MachSymbol {
                name: "_SetRect".into(),
                ty: N_EXT,
                sect: 0,
                value: 0,
            },
        ],
        indirect: vec![1],
    }
    .build();
    let universe: BTreeSet<String> = ["setrect".to_string()].into_iter().collect();
    let s = side::macho_side("osx", &m, &universe).unwrap();
    assert_eq!(s.funs.len(), 1);
    assert_eq!(s.feats[0].calls, [Callee::Api("setrect".into())]);
    assert_eq!(s.feats[0].strs, ["a string for the slice"]);
    assert_eq!(s.funs[0].sig.as_ref().unwrap().key(), "draw()");
}
