#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_codeview::decl::{
    self, BYTES_PER_MEMBER, MEMBERS_FLOOR, Model, NODES_FLOOR, NODES_PER_BYTE,
};
use msvc6_codeview::record::Record;
use msvc6_codeview::source::{
    EXPR_NEST, NEST, REWORK, REWORK_FLOOR, Src, eval_if_stats, parse_bytes,
};
use msvc6_codeview::{check, names, nb};
use msvc6_fixture::codeview::{
    self as cv, LF_CLASS, LF_FIELDLIST, Module, Nb11, index, member, method, onemethod, sample,
};
use msvc6_image::pe::Image;
use std::collections::BTreeMap;
use std::path::Path;

const SEGS: [u32; 3] = [0x1000, 0x2000, 0x3000];

fn every_reader(what: &str, record: &[u8]) -> anyhow::Result<usize> {
    let rec = Record::parse(record, &SEGS)?;
    assert!(
        rec.items() <= record.len(),
        "{what}: {} items decoded from {} bytes",
        rec.items(),
        record.len()
    );
    let named = names::attested(&rec, None)?;
    let name_cap = (record.len() / BYTES_PER_MEMBER).max(MEMBERS_FLOOR);
    assert!(named.len() <= name_cap, "{what}: {} names", named.len());
    let m = Model::build(&rec)?;
    assert_eq!(
        m.node_cap,
        record.len().saturating_mul(NODES_PER_BYTE).max(NODES_FLOOR),
        "{what}"
    );
    assert_eq!(m.member_cap, name_cap, "{what}");
    assert!(
        m.nodes <= m.node_cap && m.members <= m.member_cap,
        "{what}: {} nodes of {}, {} members of {}",
        m.nodes,
        m.node_cap,
        m.members,
        m.member_cap
    );
    let text = decl::render(&m, true);
    let rows = decl::rows(&m, true);
    assert!(rows.len() <= m.members, "{what}: {} rows", rows.len());
    let c = check::check(record)?;
    assert!(
        c.types.saturating_add(c.symbols) <= record.len(),
        "{what}: {} records checked",
        c.types + c.symbols
    );
    Ok(text.len() + rows.len() + named.len() + c.types)
}

fn backstop<T>(what: &str, f: impl FnOnce() -> T) -> T {
    msvc6_fixture::backstop::run(what, f).0
}

fn bounded(what: &str, record: &[u8]) -> anyhow::Result<usize> {
    backstop(what, || every_reader(what, record))
}

fn refused(what: &str, record: &[u8], want: &str) {
    let e = bounded(what, record).expect_err(what);
    assert!(format!("{e:#}").contains(want), "{what}: {e:#}");
}

#[test]
fn every_word_of_the_fixture_record_set_to_an_edge_value_is_read_or_refused_within_its_bounds() {
    let base = sample().record;
    assert!(bounded("the fixture", &base).is_ok());
    let mut errors = 0usize;
    for at in (0..base.len()).step_by(2) {
        for v in [0u16, 0xffff, 0x7fff, 0x8000, 0x1000] {
            let mut b = base.clone();
            if let Some(w) = b.get_mut(at..at + 2) {
                w.copy_from_slice(&v.to_le_bytes());
            }
            errors += usize::from(bounded(&format!("u16 {v:#x} at {at:#x}"), &b).is_err());
        }
    }
    for at in (0..base.len()).step_by(4) {
        for v in [
            u32::MAX,
            0x7fff_ffff,
            0x1000,
            u32::try_from(base.len()).unwrap(),
        ] {
            let mut b = base.clone();
            if let Some(w) = b.get_mut(at..at + 4) {
                w.copy_from_slice(&v.to_le_bytes());
            }
            errors += usize::from(bounded(&format!("u32 {v:#x} at {at:#x}"), &b).is_err());
        }
    }
    assert!(errors > 0);
}

#[test]
fn every_truncation_of_the_fixture_record_is_read_or_refused_within_its_bounds() {
    let base = sample().record;
    for n in 0..base.len() {
        let r = bounded(&format!("{n} bytes"), base.get(..n).unwrap());
        if n < 8 {
            assert!(r.is_err());
        }
    }
    let p = nb::Nb::parse(&base).unwrap();
    let first = p.dir_at + usize::from(p.dir_header);
    for (i, e) in p.entries.iter().enumerate() {
        let cb_at = first + i * usize::from(p.entry_size) + 8;
        for cut in 1..=e.cb {
            let mut b = base.clone();
            b[cb_at..cb_at + 4].copy_from_slice(&(e.cb - cut).to_le_bytes());
            let _ = bounded(&format!("subsection {i} cut by {cut}"), &b);
        }
    }
}

fn one_module(b: &mut Nb11, symbols: Vec<u8>) {
    b.modules.push(Module {
        name: ".\\m.obj".into(),
        ilib: 0,
        segments: vec![(1, 0, 0x100)],
        symbols,
        files: vec![("C:\\w\\m.cpp".into(), 1, vec![(0, 1)])],
    });
}

#[test]
fn a_type_web_that_fans_out_is_cut_by_the_node_budget() {
    let mut b = Nb11::default();
    let args = u32::try_from(b.types.len()).unwrap() + 0x1000;
    let proc = args + 1;
    let mut al = 16000u32.to_le_bytes().to_vec();
    for _ in 0..16000 {
        al.extend_from_slice(&proc.to_le_bytes());
    }
    b.ty(0x1201, al);
    let mut pd = 0x0003u32.to_le_bytes().to_vec();
    pd.extend_from_slice(&[0, 0]);
    pd.extend_from_slice(&16000u16.to_le_bytes());
    pd.extend_from_slice(&args.to_le_bytes());
    b.ty(0x1008, pd);
    let ptr = b.pointer(proc);
    let members: Vec<Vec<u8>> = (0..600)
        .map(|i| member(3, ptr, i * 4, &format!("m{i}")))
        .collect();
    let fl = b.fieldlist(&members);
    b.class(LF_CLASS, 600, fl, 2400, "Web");
    let mut s = cv::gproc("Web::f", 1, 0, 1, proc);
    s.extend(cv::end());
    one_module(&mut b, s);
    refused("fan-out", &b.build(), "nodes, more than its");
}

#[test]
fn a_pointer_to_itself_and_a_field_list_that_continues_into_itself_end() {
    let mut b = Nb11::default();
    let selfp = 0x1000u32;
    let mut pd = selfp.to_le_bytes().to_vec();
    pd.extend_from_slice(&0x0000_000au32.to_le_bytes());
    b.ty(0x1002, pd);
    let fl = 0x1001u32;
    let mut d = member(3, selfp, 0, "m_self");
    while !d.len().is_multiple_of(4) {
        d.push(0xf0 | u8::try_from(4 - d.len() % 4).unwrap());
    }
    d.extend(index(fl));
    b.ty(LF_FIELDLIST, d);
    b.class(LF_CLASS, 1, fl, 4, "Loop");
    one_module(&mut b, Vec::new());
    assert!(bounded("cycles", &b.build()).is_ok());
}

#[test]
fn method_lists_that_fan_out_and_shared_field_lists_are_capped() {
    let mut b = Nb11::default();
    let fwd = b.class(LF_CLASS, 0, 0, 0, "Wide");
    let p = b.pointer(fwd);
    let mf = b.mfunc(3, fwd, p, &[]);
    let entries: Vec<(u16, u32, Option<u32>)> = (0..7000).map(|_| (3, mf, None)).collect();
    let ml = b.methodlist(&entries);
    let groups: Vec<Vec<u8>> = (0..4000)
        .map(|i| method(7000, ml, &format!("f{i}")))
        .collect();
    let fl = b.fieldlist(&groups);
    for i in 0..400 {
        b.class(LF_CLASS, 1, fl, 4, &format!("Wide{i}"));
    }
    one_module(&mut b, Vec::new());
    refused("method lists", &b.build(), "members, more than its");
}

#[test]
fn hostile_symbol_streams_are_refused() {
    let base = || {
        let mut b = Nb11::default();
        b.class(LF_CLASS, 0, 0, 0, "C");
        let fl = b.fieldlist(&[onemethod(3, 0x0003, None, "f")]);
        b.class(LF_CLASS, 1, fl, 4, "C");
        b
    };
    let mut b = base();
    let mut unclosed = Vec::new();
    for i in 0..5000 {
        unclosed.extend(cv::gproc(&format!("C::f{i}"), 1, i, 1, 0x0003));
        unclosed.extend(cv::block(i, 1));
    }
    one_module(&mut b, unclosed);
    assert!(bounded("unclosed scopes", &b.build()).is_ok());
    let mut b = base();
    let mut long = cv::sym(0x1006, &[0; 8]);
    long[0] = 0xff;
    long[1] = 0x7f;
    one_module(&mut b, long);
    assert!(bounded("a record past its stream", &b.build()).is_err());
    let mut b = base();
    let mut name = 0i32.to_le_bytes().to_vec();
    name.extend_from_slice(&0x74u32.to_le_bytes());
    name.push(200);
    name.extend_from_slice(b"short");
    one_module(&mut b, cv::sym(0x1006, &name));
    assert!(bounded("a name past its record", &b.build()).is_err());
}

#[test]
fn line_blocks_that_overlap_are_refused() {
    let segs = 0x4000usize;
    let pairs = 16usize;
    let fo = 20usize;
    let lb = (fo + 4 + 12 * segs + 1 + 5).next_multiple_of(4);
    let mut body = Vec::new();
    body.extend_from_slice(&1u16.to_le_bytes());
    body.extend_from_slice(&1u16.to_le_bytes());
    body.extend_from_slice(&u32::try_from(fo).unwrap().to_le_bytes());
    body.resize(fo, 0);
    body.extend_from_slice(&u16::try_from(segs).unwrap().to_le_bytes());
    body.extend_from_slice(&[0, 0]);
    for _ in 0..segs {
        body.extend_from_slice(&u32::try_from(lb).unwrap().to_le_bytes());
    }
    body.resize(body.len() + 8 * segs, 0);
    body.extend_from_slice(&cv::st("x.cpp"));
    body.resize(lb, 0);
    body.extend_from_slice(&1u16.to_le_bytes());
    body.extend_from_slice(&u16::try_from(pairs).unwrap().to_le_bytes());
    for k in 0..pairs {
        body.extend_from_slice(&u32::try_from(k).unwrap().to_le_bytes());
    }
    for k in 0..pairs {
        body.extend_from_slice(&u16::try_from(k + 1).unwrap().to_le_bytes());
    }
    let mut b = Nb11::default();
    one_module(&mut b, Vec::new());
    let mut subs = b.subsections();
    subs.iter_mut()
        .find(|(sst, _, _)| *sst == nb::SST_SRC_MODULE)
        .unwrap()
        .2 = body.clone();
    refused(
        "overlapping line blocks",
        &cv::pack(b"NB11", &subs),
        "they overlap",
    );
    let mut once = body;
    once[fo..fo + 2].copy_from_slice(&1u16.to_le_bytes());
    let honest = cv::pack(b"NB11", &{
        let mut s = b.subsections();
        s.iter_mut()
            .find(|(sst, _, _)| *sst == nb::SST_SRC_MODULE)
            .unwrap()
            .2 = once;
        s
    });
    let rec = Record::parse(&honest, &SEGS).unwrap();
    assert_eq!(rec.modules[0].files[0].lines.len(), pairs);
}

#[test]
fn an_image_whose_codeview_entry_lies_is_refused() {
    let s = sample();
    let img = Image::parse(s.image.clone()).unwrap();
    assert!(nb::locate(&img).unwrap().is_some());
    let dbg = img.dir(msvc6_image::pe::Dir::Debug).unwrap();
    let at = img.rva_to_off(dbg.rva).unwrap().get();
    for (field, v) in [(16usize, u32::MAX), (24, u32::MAX), (16, 4)] {
        let mut b = s.image.clone();
        b[at + field..at + field + 4].copy_from_slice(&v.to_le_bytes());
        let r = backstop(&format!("field {field}"), || {
            Image::parse(b).and_then(|i| Record::from_image(&i).map(|_| ()))
        });
        assert!(r.is_err(), "field {field} = {v:#x}");
    }
}

#[test]
fn decorated_names_the_demangler_cannot_read_are_skipped() {
    let base = "?set_volume@Dev@@UAEXK@Z";
    for i in 0..base.len() {
        for c in ['?', '@', '$', 'A', '0', '\u{ff}'] {
            let mut s: Vec<char> = base.chars().collect();
            s[i] = c;
            let s: String = s.into_iter().collect();
            let _ = decl::decorated_types(&s);
            let _ = decl::decorated_class(&s);
        }
    }
    assert!(decl::decorated_types("").is_none());
}

fn parse_src(text: &str) -> Src {
    let mut defs = BTreeMap::new();
    let mut src = Src::default();
    parse_bytes(text.as_bytes(), Path::new("x.h"), &mut defs, &mut src);
    src
}

#[test]
fn deeply_nested_source_is_read_within_its_depth_and_rework_bounds() {
    let cases = [
        (
            true,
            format!("{}int x;{}", "struct A {".repeat(20000), "};".repeat(20000)),
        ),
        (true, format!("int f({});", "int (*".repeat(20000))),
        (true, format!("int f({}", "int g(".repeat(20000))),
        (true, format!("class C {{ {} }};", "union { ".repeat(20000))),
        (
            false,
            format!(
                "#if {}1{}\nint a;\n#endif\n",
                "(".repeat(50000),
                ")".repeat(50000)
            ),
        ),
        (
            false,
            format!("#define X \\\n{}\n", "1 \\\n".repeat(1_000_000)),
        ),
        (false, "/* never closed".repeat(1000)),
        (false, "\"never closed\n".repeat(1000)),
    ];
    for (i, (deep, c)) in cases.iter().enumerate() {
        let s = backstop(&format!("case {i}"), || parse_src(c));
        let st = s.stats;
        assert!(
            st.max_nest <= NEST + 1,
            "case {i}: nested {} levels",
            st.max_nest
        );
        if *deep {
            assert_eq!(
                st.max_nest,
                NEST + 1,
                "case {i}: the nesting cap was not reached"
            );
        }
        assert!(
            st.rework <= st.tokens.saturating_mul(REWORK + NEST + 2) + REWORK_FLOOR,
            "case {i}: {} tokens re-parsed for {} tokens",
            st.rework,
            st.tokens
        );
    }
    let deep_if = format!("{}1{}", "(".repeat(50000), ")".repeat(50000));
    let (_, nest) = backstop("the #if expression", || {
        eval_if_stats(&deep_if, &BTreeMap::new())
    });
    assert_eq!(nest, EXPR_NEST + 1);
    let unions = format!("class C {{ {} }};", "union { ".repeat(20000));
    let s = parse_src(&unions);
    assert!(
        s.stats.rework > 0,
        "the re-parse of an unclosed anonymous union was not exercised"
    );
}
