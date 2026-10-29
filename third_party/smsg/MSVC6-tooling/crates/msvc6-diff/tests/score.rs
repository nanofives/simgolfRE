#![allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::expect_used)]

use msvc6_diff::env::Env;
use msvc6_diff::oracle::{AUTO, ByteOracle as _, Members, Try};
use msvc6_diff::resolve::Names;
use msvc6_diff::score::{self, Plan, Seat};
use msvc6_fixture::coff::{
    CLASS_WEAK_EXTERNAL, Object, REL_DIR32, Reloc, SCN_COMDAT, SCN_TEXT, SELECT_ANY,
    SELECT_NODUPLICATES, Section, Symbol, aux_with,
};
use msvc6_fixture::pe::Pe;
use msvc6_image::pe::Image;
use msvc6_project::names::{Kind, Name, NameOracle};

const BASE: u32 = 0x0040_0000;

const F0: [u8; 48] = {
    let mut b = [0x90u8; 48];
    b[0] = 0x55;
    b[1] = 0x8b;
    b[2] = 0xec;
    b[3] = 0x33;
    b[4] = 0xc0;
    b[5] = 0x5d;
    b[6] = 0xc3;
    b
};
const F1: [u8; 6] = [0xb8, 0x2a, 0, 0, 0, 0xc3];
const F2: [u8; 9] = [0xa1, 0, 0, 0, 0, 0x83, 0xc0, 0x01, 0xc3];

struct Img {
    text: Vec<u8>,
    seats: Vec<(u32, u32)>,
    names: Vec<(String, u32)>,
    pads: Vec<(u32, u32)>,
}

impl Img {
    fn new() -> Self {
        Self {
            text: vec![0xcc; 0x400],
            seats: Vec::new(),
            names: Vec::new(),
            pads: Vec::new(),
        }
    }

    fn put(&mut self, rva: u32, bytes: &[u8]) {
        let at = (rva - 0x1000) as usize;
        self.text[at..at + bytes.len()].copy_from_slice(bytes);
    }

    fn func(&mut self, name: &str, rva: u32, bytes: &[u8], recorded: Option<u32>) {
        self.put(rva, bytes);
        if let Some(len) = recorded {
            self.seats.push((rva, len));
        }
        self.names.push((name.to_string(), rva));
    }

    fn env(&self) -> Env {
        let mut pe = Pe::exe();
        let rva = pe.push(".text", 0x6000_0020, self.text.clone());
        assert_eq!(rva, 0x1000);
        let img = Image::parse(pe.build()).unwrap();
        let mut o = NameOracle::default();
        o.add(
            "tsv",
            "names.tsv",
            self.names
                .iter()
                .map(|(n, r)| Name {
                    rva: *r,
                    name: n.clone(),
                    kind: Kind::Label,
                    size: None,
                    scope: None,
                    provider: "tsv".into(),
                })
                .collect(),
            Vec::new(),
        );
        Env::new(
            "T",
            img,
            Names::from_oracle(&o),
            self.seats.clone(),
            self.pads.clone(),
        )
        .unwrap()
    }
}

struct Obj {
    o: Object,
    data: u32,
}

impl Obj {
    fn new() -> Self {
        let mut o = Object::new();
        let data = o.undefined("_g_data");
        Self { o, data }
    }

    fn func(&mut self, name: &str, bytes: &[u8], relocs: &[u32], selection: u8) -> u32 {
        let n = self.o.section(Section {
            name: ".text".into(),
            characteristics: SCN_TEXT | SCN_COMDAT,
            data: bytes.to_vec(),
            relocs: relocs
                .iter()
                .map(|&va| Reloc {
                    va,
                    symbol: self.data,
                    typ: REL_DIR32,
                })
                .collect(),
            ..Section::default()
        });
        self.o.section_symbol(n, selection, 0);
        self.o.external(name, 0, n)
    }

    fn build(&self) -> Vec<u8> {
        self.o.build()
    }
}

fn three() -> (Img, Obj) {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    img.func("_f1", 0x1040, &F1, Some(6));
    let mut f2 = F2;
    f2[1..5].copy_from_slice(&(BASE + 0x3000).to_le_bytes());
    img.func("_f2", 0x1050, &f2, Some(9));
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    obj.func("_f1", &F1, &[], SELECT_NODUPLICATES);
    obj.func("_f2", &F2, &[1], SELECT_NODUPLICATES);
    (img, obj)
}

fn comdats(d: &[u8]) -> Vec<msvc6_diff::comdat::Comdat> {
    msvc6_diff::comdat::text_of(d).unwrap()
}

#[test]
fn a_predicted_member_seats_at_the_reserve_law_and_relocated_bytes_are_masked() {
    let (img, obj) = three();
    let env = img.env();
    let d = obj.build();
    let s = score::seat_and_score(&env, &d, &comdats(&d), 0x1000, &Plan::Predict).unwrap();
    let seats: Vec<(u32, Seat)> = s.members.iter().map(|m| (m.at, m.seat)).collect();
    assert_eq!(
        seats,
        [
            (0x1000, Seat::Given),
            (0x1040, Seat::Image),
            (0x1050, Seat::Image)
        ]
    );
    assert_eq!(s.compared, 48 + 6 + 5);
    assert_eq!(s.differing, 0);
    assert!(s.identical());
    assert_eq!(s.members[2].masked, 4);
    assert_eq!(s.members[2].relocs, 1);
    assert_eq!(
        s.verdict_line(),
        "  VERDICT: IDENTICAL  -- 0 of 59 comparable bytes differ"
    );
    assert_eq!(
        s.members[1].line(),
        "  0x001040 (SEATED) +0x6  0 of 6 comparable bytes differ (0 masked by 0 relocation(s))  _f1"
    );
}

#[test]
fn a_prediction_no_recorded_function_confirms_is_unseated() {
    let (mut img, obj) = three();
    img.seats.retain(|s| s.0 != 0x1040);
    img.seats.push((0x1040, 7));
    let env = img.env();
    let d = obj.build();
    let s = score::seat_and_score(&env, &d, &comdats(&d), 0x1000, &Plan::Predict).unwrap();
    assert_eq!(s.members[1].seat, Seat::Unseated);
    assert_eq!(s.members[1].at, 0x1030);
    assert_eq!(s.members[2].seat, Seat::Unseated);
    assert_eq!(s.void(), 2);
    assert!(
        s.breakdown()
            .unwrap()
            .iter()
            .any(|l| l.contains("2 UNSEATED"))
    );
}

#[test]
fn an_asserted_address_is_checked_and_a_wrong_one_is_misasserted() {
    let (img, obj) = three();
    let env = img.env();
    let d = obj.build();
    let c = comdats(&d);
    let good =
        score::seat_and_score(&env, &d, &c, 0x1000, &Plan::Asserted(vec![0x1040, 0x1050])).unwrap();
    assert!(good.members.iter().skip(1).all(|m| m.seat == Seat::Image));
    let bad = score::seat_and_score(&env, &d, &c, 0x1000, &Plan::Asserted(vec![0x1050])).unwrap();
    assert_eq!(bad.members[1].seat, Seat::Misasserted);
    assert_eq!(bad.members[2].seat, Seat::Misasserted);
    assert!(!bad.identical());
}

#[test]
fn a_differing_byte_is_counted_once_and_reported_by_the_instruction_diff() {
    let (img, _) = three();
    let env = img.env();
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    let mut f1 = F1;
    f1[1] = 0x2b;
    obj.func("_f1", &f1, &[], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = Try::new(&env, 0x1000, Members::Predict).score(&d).unwrap();
    assert_eq!(s.differing, 1);
    assert_eq!(s.members[1].bad, [1]);
    assert_eq!(s.verdict(), "DIFFERS");
    let lines = s.lines(true);
    assert!(
        lines.iter().any(|l| l.contains("want mov eax,2Ah")),
        "{lines:#?}"
    );
    assert!(lines.iter().any(|l| l.contains("1 wrong instruction(s)")));
    assert!(lines.contains(&"  VERDICT: DIFFERS  -- 1 of 54 comparable bytes differ".to_string()));
}

#[test]
fn a_declared_text_pad_is_not_scored_and_seats_a_member_padded() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    img.func("_f1", 0x1030, &F1, Some(6));
    img.put(0x1036, &[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    img.pads.push((0x1036, 10));
    let env = img.env();
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    let mut f1 = [0x90u8; 16];
    f1[..6].copy_from_slice(&F1);
    obj.func("_f1", &f1, &[], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = score::seat_and_score(
        &env,
        &d,
        &comdats(&d),
        0x1000,
        &Plan::Asserted(vec![0x1030]),
    )
    .unwrap();
    assert_eq!(
        s.members[1].seat,
        Seat::Padded {
            pad: 0x1036,
            len: 10
        }
    );
    assert_eq!(s.pad_skipped, 10);
    assert!(s.identical());
}

#[test]
fn cl_s_own_nop_tail_seats_a_member_recorded_short() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    let mut f1 = [0x90u8; 16];
    f1[..6].copy_from_slice(&F1);
    img.func("_f1", 0x1030, &f1, Some(6));
    let env = img.env();
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    obj.func("_f1", &f1, &[], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = score::seat_and_score(&env, &d, &comdats(&d), 0x1000, &Plan::Predict).unwrap();
    assert_eq!(s.members[1].seat, Seat::NopTail { rec: 6 });
    assert!(s.identical());
}

fn switch_body(at: u32) -> Vec<u8> {
    let table = BASE + at + 20;
    let mut b = vec![0xff, 0x24, 0x85];
    b.extend_from_slice(&table.to_le_bytes());
    b.extend_from_slice(&[0xb8, 1, 0, 0, 0, 0xc3, 0xb8, 2, 0, 0, 0, 0xc3]);
    b.push(0x90);
    b.extend_from_slice(&(BASE + at + 7).to_le_bytes());
    b.extend_from_slice(&(BASE + at + 13).to_le_bytes());
    b.extend_from_slice(&[0x90; 4]);
    b
}

#[test]
fn a_switch_table_past_the_recorded_length_seats_the_member() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    let body = switch_body(0x1030);
    assert_eq!(body.len(), 32);
    img.func("_sw", 0x1030, &body, Some(19));
    let env = img.env();
    assert_eq!(
        score::jump_table_seat(&env, BASE, 0x1030, 32),
        Some(Seat::JumpTable {
            rec: 19,
            entries: 2
        })
    );
    assert_eq!(score::jump_table_seat(&env, BASE, 0x1030, 36), None);
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    obj.func("_sw", &body, &[3, 20, 24], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = score::seat_and_score(&env, &d, &comdats(&d), 0x1000, &Plan::Predict).unwrap();
    assert_eq!(
        s.members[1].seat,
        Seat::JumpTable {
            rec: 19,
            entries: 2
        }
    );
    assert!(s.identical());
}

#[test]
fn the_first_byte_of_text_has_nothing_before_it_to_disqualify_an_unrecorded_seat() {
    let mut img = Img::new();
    img.func("_f1", 0x1000, &F1, None);
    let env = img.env();
    assert_eq!(
        score::unrecorded_seat(&env, BASE, 0x1000, 6, 16, &score::LINK_FILL),
        Some(Seat::Unrecorded)
    );
}

#[test]
fn an_unrecorded_address_seats_where_the_image_s_code_ends_at_the_length() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    img.func("_f1", 0x1040, &F1, None);
    img.func("_f2", 0x1080, &F2, Some(9));
    let env = img.env();
    assert_eq!(
        score::unrecorded_seat(&env, BASE, 0x1040, 6, 16, &score::LINK_FILL),
        Some(Seat::Unrecorded)
    );
    assert_eq!(
        score::unrecorded_seat(&env, BASE, 0x1040, 5, 16, &score::LINK_FILL),
        None
    );
    assert_eq!(
        score::unrecorded_seat(&env, BASE, 0x1040, 6, 4, &score::LINK_FILL),
        None
    );
    assert_eq!(
        score::unrecorded_seat(&env, BASE, 0x1080, 9, 16, &score::LINK_FILL),
        None
    );
}

fn vector_dtor() -> Vec<u8> {
    vec![
        0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0xf6, 0xc3, 0x02, 0x74, 0x01, 0xc3,
    ]
}

#[test]
fn a_weak_default_scalar_dtor_on_the_image_s_vector_dtor_is_superseded() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    img.func("??_EFoo@@UAEPAXI@Z", 0x1030, &vector_dtor(), Some(14));
    let env = img.env();
    let scalar = [
        0x56, 0x8b, 0xf1, 0xf6, 0x44, 0x24, 0x08, 0x01, 0x5e, 0xc2, 0x04, 0x00,
    ];
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    let g = obj.func("??_GFoo@@UAEPAXI@Z", &scalar, &[], SELECT_ANY);
    obj.o.symbol(Symbol {
        name: "??_EFoo@@UAEPAXI@Z".into(),
        value: 0,
        section: 0,
        typ: 0x20,
        class: CLASS_WEAK_EXTERNAL,
        aux: vec![aux_with(0, &g.to_le_bytes())],
    });
    let d = obj.build();
    let weak = msvc6_diff::comdat::weak_defaults_of(&d).unwrap();
    assert_eq!(
        weak.get("??_EFoo@@UAEPAXI@Z").map(String::as_str),
        Some("??_GFoo@@UAEPAXI@Z")
    );
    let s = score::seat_and_score(
        &env,
        &d,
        &comdats(&d),
        0x1000,
        &Plan::Asserted(vec![0x1030]),
    )
    .unwrap();
    assert_eq!(s.members[1].seat, Seat::Superseded);
    assert!(s.members[1].bad.is_empty());
    assert_eq!(s.compared, 48);
    assert!(s.lines(false).iter().any(|l| l.starts_with("  EXCLUDED")));
}

#[test]
fn auto_members_seat_each_comdat_by_its_name_and_the_hint_moves_to_comdat_zero() {
    let (img, obj) = three();
    let env = img.env();
    let d = obj.build();
    let auto = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap();
    let an = auto.anchor.clone().unwrap();
    assert_eq!((an.root, an.moved), (0x1000, true));
    assert_eq!(an.own, Some(("_f0".to_string(), 0x1000)));
    assert!(auto.members.iter().skip(1).all(|m| m.seat == Seat::Image));
    let hinted = Try::new(&env, 0x1050, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap();
    assert_eq!(hinted.rva, 0x1000);
    assert!(hinted.anchor.unwrap().moved);
    let exact = Try::new(&env, 0x1000, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap();
    assert!(!exact.anchor.unwrap().moved);
    let stranger = Try::new(&env, 0x1100, Members::Auto(Vec::new())).score(&d);
    assert!(
        stranger
            .unwrap_err()
            .to_string()
            .contains("no COMDAT of this object")
    );
}

#[test]
fn an_unnamed_comdat_refuses_auto_members_and_comdat_zero_in_the_list_is_refused() {
    let (img, mut obj) = three();
    obj.func("_nowhere", &F1, &[], SELECT_NODUPLICATES);
    let env = img.env();
    let d = obj.build();
    let e = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap_err()
        .to_string();
    assert!(e.contains("_nowhere"), "{e}");
    let e = Try::new(&env, 0x1000, Members::Asserted(vec![0x1000, 0x1040]))
        .score(&d)
        .unwrap_err()
        .to_string();
    assert!(e.contains("COMDAT[0]"), "{e}");
    let e = Try::new(&env, AUTO, Members::Predict)
        .score(&d)
        .unwrap_err()
        .to_string();
    assert!(e.contains("--auto-members"), "{e}");
}

#[test]
fn a_select_any_first_member_leaves_the_own_function_to_the_next() {
    let members = [
        ("??1Inline@@QAE@XZ".to_string(), 0x2000, true),
        ("_own".to_string(), 0x1000, false),
    ];
    let a = score::choose_anchor(None, &members).unwrap();
    assert_eq!(a.root, 0x2000);
    assert_eq!(a.own, Some(("_own".to_string(), 0x1000)));
    assert!(score::choose_anchor(Some(0x3000), &members).is_err());
    assert!(!score::choose_anchor(Some(0x2000), &members).unwrap().moved);
}

#[test]
fn a_hostile_object_is_an_error_not_a_panic() {
    let (img, obj) = three();
    let env = img.env();
    let d = obj.build();
    let oracle = Try::new(&env, 0x1000, Members::Predict);
    for cut in [0, 1, 19, 20, 60, d.len() / 2, d.len() - 1] {
        let _ = oracle.score(&d[..cut]);
    }
    let mut bad = d.clone();
    for b in bad.iter_mut().skip(20).step_by(7) {
        *b = 0xff;
    }
    let _ = oracle.score(&bad);
    let far = Try::new(&env, 0x0fff_0000, Members::Predict).score(&d);
    assert!(far.is_err());
}

#[test]
fn the_member_table_scores_each_object_at_its_names_and_lists_the_differing() {
    let (img, _) = three();
    let env = img.env();
    let dir = std::env::temp_dir().join(format!("msvc6t-memtable-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let mut good = Obj::new();
    good.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    good.func("_f1", &F1, &[], SELECT_NODUPLICATES);
    let mut bad = Obj::new();
    let mut f2 = F2;
    f2[7] = 0x02;
    bad.func("_f2", &f2, &[1], SELECT_NODUPLICATES);
    bad.func("_nowhere", &F1, &[], SELECT_NODUPLICATES);
    msvc6_base::srcio::own_write(dir.join("b.obj"), bad.build()).unwrap();
    msvc6_base::srcio::own_write(dir.join("a.obj"), good.build()).unwrap();
    let objs = vec![dir.join("b.obj"), dir.join("a.obj")];
    let t = msvc6_diff::memtable::score_objects(&env, &objs, 2);
    std::fs::remove_dir_all(&dir).unwrap();
    let rows: Vec<(&str, u32, usize)> = t
        .rows
        .iter()
        .map(|r| (r.tu.as_str(), r.rva, r.differing))
        .collect();
    assert_eq!(rows, [("a", 0x1000, 0), ("a", 0x1040, 0), ("b", 0x1050, 1)]);
    assert_eq!(msvc6_diff::memtable::differing(&t), [0x1050]);
    assert!(t.warnings.iter().any(|w| w.contains("_nowhere")));
}

#[test]
fn a_select_any_copy_an_earlier_object_keeps_is_a_duplicate_and_not_scored() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    img.func("?i@@YAHXZ", 0x1040, &F1, Some(6));
    let mut f2 = F2;
    f2[1..5].copy_from_slice(&(BASE + 0x3000).to_le_bytes());
    img.func("_f2", 0x1050, &f2, Some(9));
    let env = img.env();
    let dir = std::env::temp_dir().join(format!("msvc6t-duplicate-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let mut a = Obj::new();
    a.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    a.func("?i@@YAHXZ", &F1, &[], SELECT_ANY);
    let mut b = Obj::new();
    b.func("_f2", &F2, &[1], SELECT_NODUPLICATES);
    b.func(
        "?i@@YAHXZ",
        &[0x33, 0xc0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xc3],
        &[],
        SELECT_ANY,
    );
    msvc6_base::srcio::own_write(dir.join("z.obj"), a.build()).unwrap();
    msvc6_base::srcio::own_write(dir.join("b.obj"), b.build()).unwrap();
    let objs = vec![dir.join("z.obj"), dir.join("b.obj")];
    let t = msvc6_diff::memtable::score_objects(&env, &objs, 2);
    let late = msvc6_diff::memtable::score_objects(&env, &[dir.join("b.obj")], 1);
    std::fs::remove_dir_all(&dir).unwrap();
    let dup = t
        .rows
        .iter()
        .find(|r| r.tu == "b" && r.name == "?i@@YAHXZ")
        .unwrap();
    assert_eq!(dup.seat, Seat::Duplicate);
    assert_eq!(dup.kept_by.as_deref(), Some("z.obj"));
    assert_eq!(dup.differing, 0);
    assert!(msvc6_diff::memtable::differing(&t).is_empty());
    let alone = late.rows.iter().find(|r| r.name == "?i@@YAHXZ").unwrap();
    assert_eq!(alone.seat, Seat::Misasserted);
    assert_eq!(msvc6_diff::memtable::differing(&late), [0x1040]);
}

#[test]
fn a_differing_member_has_a_block_map_and_an_alignment_and_an_exact_one_has_neither() {
    let (img, _) = three();
    let env = img.env();
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    let mut f1 = F1;
    f1[1] = 0x2b;
    obj.func("_f1", &f1, &[], SELECT_NODUPLICATES);
    let s = Try::new(&env, 0x1000, Members::Predict)
        .score(&obj.build())
        .unwrap();
    let views = msvc6_diff::blockmap::Views {
        block_map: true,
        window: None,
        align: true,
        step: 0x400,
        keep_regs: false,
    };
    let differing = msvc6_diff::blockmap::render_views(&env, &s.members[1], &views);
    assert!(
        differing.contains("BLOCK MAP of image 0x1040"),
        "{differing}"
    );
    assert!(differing.contains("ALIGN image 0x1040"), "{differing}");
    let exact = msvc6_diff::blockmap::render_views(&env, &s.members[0], &views);
    assert!(
        exact.contains("nothing to map") || exact.is_empty(),
        "{exact}"
    );
    let none = msvc6_diff::blockmap::render_views(
        &env,
        &s.members[1],
        &msvc6_diff::blockmap::Views::default(),
    );
    assert!(none.is_empty());
}

const GONE: [u8; 6] = [0xb8, 7, 0, 0, 0, 0xc3];

fn dropped_env(img: &Img, names: &[&str]) -> Env {
    let mut env = img.env();
    for n in names {
        env.dropped
            .insert((*n).to_string(), "nothing calls it".into());
    }
    env
}

#[test]
fn a_dropped_name_holds_the_place_of_the_member_before_it_and_is_not_scored() {
    let (img, _) = three();
    let env = dropped_env(&img, &["_gone"]);
    let mut obj = Obj::new();
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    obj.func("_gone", &GONE, &[], SELECT_NODUPLICATES);
    obj.func("_f1", &F1, &[], SELECT_NODUPLICATES);
    obj.func("_f2", &F2, &[1], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap();
    let seats: Vec<(u32, Seat)> = s.members.iter().map(|m| (m.at, m.seat)).collect();
    assert_eq!(
        seats,
        [
            (0x1000, Seat::Given),
            (0x1000, Seat::Dropped),
            (0x1040, Seat::Image),
            (0x1050, Seat::Image)
        ]
    );
    assert_eq!((s.compared, s.differing), (48 + 6 + 5, 0));
    assert!(s.members[1].bad.is_empty());
    assert_eq!(
        s.members[1].line(),
        "  0x001000 (DROPPED, NOT SCORED -- the image's dropped list names it: the image holds no body for it) +0x6  0 of 6 comparable bytes differ (0 masked by 0 relocation(s))  _gone"
    );
    let lines = s.lines(false);
    let at = lines
        .iter()
        .position(|l| {
            l.starts_with("  EXCLUDED, NOT SCORED: 1 COMDAT(s) the image's dropped list names")
        })
        .unwrap();
    assert_eq!(lines[at + 1], "      _gone");
    assert!(
        lines
            .iter()
            .any(|l| l.contains("0 of 11 in 3 later COMDAT(s)")),
        "{lines:#?}"
    );
    let gone = Seat::Dropped;
    assert!(gone.excluded() && !gone.void());
    assert_eq!(gone.tag(), "dropped");
}

#[test]
fn dropped_names_at_the_head_hold_the_first_held_member_s_place() {
    let (img, _) = three();
    let env = dropped_env(&img, &["_lead", "_gone"]);
    let mut obj = Obj::new();
    obj.func("_lead", &F1, &[], SELECT_NODUPLICATES);
    obj.func("_gone", &GONE, &[], SELECT_NODUPLICATES);
    obj.func("_f0", &F0, &[], SELECT_NODUPLICATES);
    obj.func("_f1", &F1, &[], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap();
    let an = s.anchor.clone().unwrap();
    assert_eq!((an.root, an.root_name.as_str()), (0x1000, "_lead"));
    let seats: Vec<(u32, Seat)> = s.members.iter().map(|m| (m.at, m.seat)).collect();
    assert_eq!(
        seats,
        [
            (0x1000, Seat::Dropped),
            (0x1000, Seat::Dropped),
            (0x1000, Seat::Image),
            (0x1040, Seat::Image)
        ]
    );
    assert_eq!((s.compared, s.differing), (48 + 6, 0));
}

#[test]
fn a_dropped_name_the_image_holds_is_refused_and_one_alone_stays_unresolved() {
    let (img, obj) = three();
    let env = dropped_env(&img, &["_f1"]);
    let d = obj.build();
    let e = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap_err()
        .to_string();
    assert!(
        e.contains("_f1 is on the image's dropped list but resolves to 0x1040"),
        "{e}"
    );
    let env = dropped_env(&img, &["_gone"]);
    let mut alone = Obj::new();
    alone.func("_gone", &GONE, &[], SELECT_NODUPLICATES);
    let d = alone.build();
    let e = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap_err()
        .to_string();
    assert!(e.contains("cannot resolve 1 of this object's 1"), "{e}");
}

#[test]
fn a_dropped_member_parked_at_the_end_of_text_reads_no_image_bytes() {
    let mut img = Img::new();
    img.func("_f0", 0x1000, &F0, Some(48));
    img.func("_last", 0x13f0, &F1, Some(6));
    let env = dropped_env(&img, &["_long"]);
    let mut obj = Obj::new();
    obj.func("_last", &F1, &[], SELECT_NODUPLICATES);
    obj.func("_long", &[0x90; 0x40], &[], SELECT_NODUPLICATES);
    let d = obj.build();
    let s = Try::new(&env, AUTO, Members::Auto(Vec::new()))
        .score(&d)
        .unwrap();
    assert_eq!(s.members[1].seat, Seat::Dropped);
    assert_eq!(s.members[1].at, 0x13f0);
    assert!(s.members[1].want.is_empty());
    assert!(s.identical());
}

#[test]
fn park_dropped_moves_only_unresolved_dropped_names() {
    let m = |name: &str, rva: Option<u32>| msvc6_diff::seat::Member {
        name: name.into(),
        size: 1,
        rva,
        via_thunk: None,
        attested: false,
        external: true,
    };
    let mut ms = vec![
        m("lead1", None),
        m("lead2", None),
        m("a", Some(0x10)),
        m("mid", None),
        m("b", Some(0x20)),
        m("tail", None),
        m("other", None),
    ];
    score::park_dropped(&mut ms, |n| n != "other");
    let got: Vec<Option<u32>> = ms.iter().map(|m| m.rva).collect();
    assert_eq!(
        got,
        [
            Some(0x10),
            Some(0x10),
            Some(0x10),
            Some(0x10),
            Some(0x20),
            Some(0x20),
            None
        ]
    );
    let mut none = vec![m("x", None), m("y", None)];
    score::park_dropped(&mut none, |_| true);
    assert!(none.iter().all(|m| m.rva.is_none()));
}
