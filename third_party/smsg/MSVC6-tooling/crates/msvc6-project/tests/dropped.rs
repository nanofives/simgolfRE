#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use msvc6_project::config::Project;
use msvc6_project::dropped::{self, Dropped, HEADER};
use std::path::{Path, PathBuf};

fn tsv(text: &str) -> anyhow::Result<Vec<Dropped>> {
    dropped::parse_tsv(Path::new("gone.tsv"), text)
}

#[test]
fn a_row_is_a_decorated_name_and_the_reason_the_image_holds_no_body() {
    let rows = tsv(&format!(
        "{HEADER}\n?f@@YAXXZ\tnothing calls it\r\n\n??0A@@QAE@XZ\tthe template emits it; its import is discarded\n"
    ))
    .unwrap();
    assert_eq!(
        rows,
        [
            Dropped {
                name: "?f@@YAXXZ".into(),
                why: "nothing calls it".into()
            },
            Dropped {
                name: "??0A@@QAE@XZ".into(),
                why: "the template emits it; its import is discarded".into()
            },
        ]
    );
    assert!(tsv("").unwrap().is_empty());
    assert_eq!(tsv("_g\tno caller\n").unwrap().len(), 1);
}

#[test]
fn a_malformed_row_is_refused_by_its_line() {
    for (text, want) in [
        ("_f\n", "gone.tsv:1: 1 column(s)"),
        ("name\twhy\n_f\ta\tb\n", "gone.tsv:2: 3 column(s)"),
        ("\tno name\n", "gone.tsv:1: name `` is empty"),
        (
            "a b\twhy\n",
            "gone.tsv:1: name `a b` is empty or holds white space",
        ),
        ("_f\t \n", "gone.tsv:1: `_f` gives no reason"),
        (
            "_f\tone\n_f\ttwo\n",
            "gone.tsv:2: `_f` is already listed at line 1",
        ),
    ] {
        let e = format!("{:#}", tsv(text).unwrap_err());
        assert!(e.contains(want), "{text:?}: {e}");
    }
}

struct Scratch(PathBuf);

impl Drop for Scratch {
    fn drop(&mut self) {
        std::fs::remove_dir_all(&self.0).ok();
    }
}

#[test]
fn an_image_s_lists_are_read_in_order_and_the_first_reason_is_kept() {
    let dir = std::env::temp_dir().join(format!("msvc6-project-dropped-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let s = Scratch(dir.clone());
    msvc6_base::srcio::write(s.0.join("a.tsv"), "name\twhy\n_f\tfirst\n_g\tg's\n").unwrap();
    msvc6_base::srcio::write(s.0.join("b.tsv"), "_f\tsecond\n_h\th's\n").unwrap();
    let cfg = "schema = 1\n\n[[image]]\nid = \"A\"\nreference = \"a.exe\"\ndropped = [\"tsv:a.tsv\", \"tsv:b.tsv\"]\n\n[[image]]\nid = \"B\"\nreference = \"b.exe\"\ndropped = [\"tsv:missing.tsv\"]\n";
    let p = Project::parse(cfg, &s.0.join("msvc6t.toml")).unwrap();
    let all = dropped::dropped_of(&p, 0).unwrap();
    assert_eq!(
        all.iter()
            .map(|(k, v)| (k.as_str(), v.as_str()))
            .collect::<Vec<_>>(),
        [("_f", "first"), ("_g", "g's"), ("_h", "h's")]
    );
    let (kind, src, rows) = dropped::read_one(&p, 0, 1, &p.images[0].dropped[1]).unwrap();
    assert_eq!((kind.as_str(), rows.len()), ("tsv", 2));
    assert!(src.ends_with("b.tsv"), "{src}");
    let e = format!("{:#}", dropped::dropped_of(&p, 1).unwrap_err());
    assert!(e.contains("image[1].dropped[0]"), "{e}");
    assert!(e.contains("missing.tsv"), "{e}");
}
