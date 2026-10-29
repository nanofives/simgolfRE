#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_pdb::procs::{NameKind, read, tsv};
use msvc6_pdb::to70;

#[test]
fn the_procedures_resolve_through_the_pdbs_own_section_table_in_both_generations() {
    let s = msvc6_fixture::pdb::sample();
    let twin = to70::convert(&s.bytes).unwrap().bytes;
    for (g, file) in [("2.0", &s.bytes), ("7.0", &twin)] {
        let l = read(file).unwrap();
        let got: Vec<(String, u32, u32)> = l
            .procs
            .iter()
            .map(|p| (p.name.clone(), p.rva.unwrap(), p.len))
            .collect();
        assert_eq!(got, s.procs, "{g}");
        assert!(l.procs[0].typed() && !l.procs[2].typed(), "{g}");
        assert_eq!(l.procs[0].leaf, Some(0x1008), "{g}");
        assert_eq!(l.procs[0].lines, 4, "{g}: _f holds four rows, one from a.h");
        assert_eq!(l.procs[1].scope, "local");
        assert_eq!(l.thunks.len(), 2, "{g}");
        assert_eq!(l.children.live, 2, "{g}: the block and its label");
        assert_eq!(
            l.children.outside, 1,
            "{g}: b.obj's $L99 after its last procedure"
        );
        let publics: Vec<(String, u32)> = l
            .names
            .iter()
            .filter(|n| n.record == "public")
            .map(|n| (n.name.clone(), n.rva))
            .collect();
        assert_eq!(publics, s.publics, "{g}");
        let g_count = l
            .names
            .iter()
            .find(|n| n.name == "_g_count" && n.record == "public")
            .unwrap();
        assert_eq!(g_count.kind, NameKind::Data);
        let f = l
            .names
            .iter()
            .find(|n| n.name == "_f" && n.record == "public")
            .unwrap();
        assert_eq!(f.kind, NameKind::Function);
        let st = l.names.iter().find(|n| n.name == "_s_static").unwrap();
        assert_eq!(st.scope.as_deref(), Some("a"));
        let t = tsv(&l);
        assert!(t.starts_with("rva\tlen\tscope"));
        assert_eq!(t.lines().count(), 1 + 3 + 2);
    }
}
