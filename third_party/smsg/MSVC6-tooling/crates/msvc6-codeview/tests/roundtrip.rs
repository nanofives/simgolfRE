#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_codeview::codec::{self, Gen};
use msvc6_codeview::decl::{self, Model, RKind};
use msvc6_codeview::nb::{self, Nb};
use msvc6_codeview::record::{Field, Record};
use msvc6_fixture::codeview::{DEV_HEADER, DEV_SOURCE, sample};
use msvc6_image::pe::Image;

fn record() -> Record {
    let s = sample();
    Record::from_image(&Image::parse(s.image).unwrap()).unwrap()
}

#[test]
fn the_fixture_record_writes_back_byte_for_byte() {
    let s = sample();
    let p = Nb::parse(&s.record).unwrap();
    assert_eq!(p.signature_text(), "NB11");
    assert_eq!(p.entries.len(), 3 * 3 + 3);
    assert_eq!(p.write().unwrap(), s.record);
    let again = nb::pack(
        b"NB11",
        &p.entries
            .iter()
            .map(|e| (e.sst, e.imod, p.data(e).to_vec()))
            .collect::<Vec<_>>(),
    )
    .unwrap();
    assert_eq!(again, s.record);
}

#[test]
fn every_type_and_symbol_record_round_trips_through_the_codec() {
    let s = sample();
    let p = Nb::parse(&s.record).unwrap();
    let types = p
        .entries
        .iter()
        .find(|e| e.sst == nb::SST_GLOBAL_TYPES)
        .map(|e| p.data(e))
        .unwrap();
    let n = codec::us(codec::u32_at(types, 4).unwrap());
    assert_eq!(n, s.nb.types.len());
    let mut at = 8 + 4 * n;
    for (leaf, data) in &s.nb.types {
        let (k, payload, end) = codec::type_record_at(types, at).unwrap();
        assert_eq!(k, *leaf);
        assert!(payload.starts_with(data));
        let rec = types.get(at..end).unwrap();
        assert_eq!(codec::respell_type(rec, Gen::St, Gen::St).unwrap(), rec);
        let sz = codec::respell_type(rec, Gen::St, Gen::Sz).unwrap();
        assert_eq!(codec::respell_type(&sz, Gen::Sz, Gen::St).unwrap(), rec);
        at = end;
    }
    for e in p.entries.iter().filter(|e| e.sst == nb::SST_ALIGN_SYM) {
        let region = p.data(e).get(4..).unwrap();
        let recs = codec::walk(
            region,
            codec::Walk {
                aligned: true,
                short_refs: false,
            },
        )
        .unwrap();
        assert!(!recs.is_empty());
        for r in recs {
            let (k, body) = codec::respell_sym(r.kind, r.payload, Gen::St, Gen::Sz).unwrap();
            let (k6, back) = codec::respell_sym(k, &body, Gen::Sz, Gen::St).unwrap();
            assert_eq!(
                codec::sym_record(k6, &back).unwrap(),
                region.get(r.at..r.end.next_multiple_of(4)).unwrap(),
                "{}",
                codec::sym_name(r.kind)
            );
        }
    }
}

#[test]
fn the_record_reads_back_what_the_fixture_wrote() {
    let s = sample();
    let rec = record();
    assert_eq!(rec.signature, "NB11");
    assert_eq!(rec.type_count(), s.nb.types.len());
    assert_eq!(rec.modules.len(), 3);
    assert_eq!(rec.modules[0].name, ".\\obj\\dev.obj");
    assert_eq!(rec.modules[0].objname, "C:\\work\\obj\\dev.obj");
    assert_eq!(rec.modules[2].ilib, 1);
    let set = rec.procs().find(|p| p.name == "Dev::set_volume").unwrap();
    assert_eq!(set.rva, s.text + 0x10);
    let names: Vec<&str> = set.locals.iter().map(|l| l.name.as_str()).collect();
    assert_eq!(names, ["this", "volume", "status"]);
    assert_eq!(
        set.params()
            .iter()
            .map(|l| l.name.as_str())
            .collect::<Vec<_>>(),
        ["volume"]
    );
    assert_eq!(rec.line_of(set.module, set.rva), Some((DEV_HEADER, 42)));
    assert_eq!(rec.line_of(set.module, set.rva + 9), Some((DEV_HEADER, 43)));
    let ctor = rec.procs().find(|p| p.name == "Dev::Dev").unwrap();
    assert_eq!(rec.line_of(ctor.module, ctor.rva), Some((DEV_SOURCE, 7)));
    assert_eq!(rec.publics.len(), 8);
    assert_eq!(rec.udts.len(), 2);
    assert_eq!(rec.constants, [("LIMIT".to_string(), 0x74, 3)]);
    assert_eq!(rec.modules[0].data[0].name, "Dev::m_head");
    assert_eq!(rec.vftables["??_7Dev@@6B@"], [s.text + 0x10]);
    assert_eq!(
        rec.vftables["??_7Snd@@6B@"],
        [s.text + 0x40, s.text + 0xb0, s.text + 0x90, s.text + 0x20]
    );
    let r = rec.ty(s.snd).unwrap();
    let fields = rec.fields(codec::u32_at(r.data, 4).unwrap());
    assert!(matches!(fields[0], Field::Base { off: 0, .. }));
    assert!(
        matches!(&fields[2], Field::Method { name, overloads } if name == "load" && overloads.len() == 2)
    );
    assert!(matches!(&fields[3], Field::Method { name, .. } if name == "play"));
}

const DECL: &str = "// The record's declarations: 1 class(es) in 1 header(s), 1 with no header recorded, 1 enum(s).

// ===== dev h.h

// T1009, size 0x8; inline bodies: dev h.h x2; out-of-line bodies: dev.cpp x1
class Dev {
    // (vfptr)
public:
    Dev();  // body dev.cpp:7
private:
    unsigned long m_flags;  // +0x4
protected:
    static Dev *m_head;  // defined at 0x3000
public:
    virtual void set_volume(unsigned long volume);  // vtbl +0x0; inline dev h.h:42
    bool is_on();  // inline dev h.h:50
    // implicit: void __local_vftable_ctor_closure();
};

// ===== no header recorded (no inline body)

// T101a, size 0xc; inline bodies: none; out-of-line bodies: snd.cpp x2
class Snd : public Dev {
    // (vfptr)
public:
    virtual void load(char *);  // vtbl +0x4; no body in the record
    virtual void load(unsigned long);  // vtbl +0x0; no body in the record
    virtual void play();  // vtbl +0x8; body snd.cpp:5
    virtual void play(unsigned long n);  // vtbl +0x4; body snd.cpp:9
};

// ===== enums

enum ERRCODE {
    ERR_OK = 0,
    ERR_FAIL = 1,
};
";

#[test]
fn cv_decl_on_the_fixture_is_stable() {
    let m = Model::build(&record()).unwrap();
    let t = decl::render(&m, false);
    assert_eq!(t, DECL, "\n{t}");
    assert_eq!(decl::render(&Model::build(&record()).unwrap(), false), t);
    assert_eq!(m.project_dir, "c:\\work\\src\\");
    assert!(m.scope.contains("Dev") && m.scope.contains("Snd") && m.scope.contains("ERRCODE"));
    assert_eq!(
        m.typedefs.get("DWORD").map(|t| t.spell()).as_deref(),
        Some("unsigned long")
    );
    let snd = m.class("Snd").unwrap();
    let measured: Vec<(Option<u32>, bool)> = snd
        .members
        .iter()
        .filter_map(|mm| match &mm.kind {
            RKind::Method(x) => Some((x.slot, x.slot_measured)),
            _ => None,
        })
        .collect();
    assert_eq!(
        measured,
        [
            (Some(4), false),
            (Some(0), false),
            (Some(8), true),
            (Some(4), true)
        ]
    );
    let rows = decl::rows(&m, false);
    assert_eq!(rows.len(), 14);
    assert_eq!(decl::rows(&m, true).len(), rows.len());
}
