use serde::Serialize;
use std::io;
use std::path::Path;

pub const FORMAT: u32 = 1;

#[derive(Debug, Serialize)]
struct Envelope<'a, S: Serialize, T: Serialize> {
    format: u32,
    command: &'a str,
    image: &'a str,
    #[serde(skip_serializing_if = "Option::is_none")]
    summary: Option<&'a S>,
    rows: &'a [T],
}

pub fn render_with<S: Serialize, T: Serialize>(
    command: &str,
    image: &str,
    summary: Option<&S>,
    rows: &[T],
) -> Result<String, serde_json::Error> {
    let mut s = serde_json::to_string_pretty(&Envelope {
        format: FORMAT,
        command,
        image,
        summary,
        rows,
    })?;
    s.push('\n');
    Ok(s)
}

pub fn render<T: Serialize>(
    command: &str,
    image: &str,
    rows: &[T],
) -> Result<String, serde_json::Error> {
    render_with::<(), T>(command, image, None, rows)
}

pub fn write_with<S: Serialize, T: Serialize>(
    path: &Path,
    command: &str,
    image: &str,
    summary: Option<&S>,
    rows: &[T],
) -> io::Result<()> {
    let text = render_with(command, image, summary, rows)?;
    crate::srcio::own_write(path, text)
        .map_err(|e| io::Error::new(e.kind(), format!("writing {}: {e}", path.display())))?;
    eprintln!(
        "wrote {} {command} row(s) as JSON to {}",
        rows.len(),
        path.display()
    );
    Ok(())
}

pub fn write<T: Serialize>(path: &Path, command: &str, image: &str, rows: &[T]) -> io::Result<()> {
    write_with::<(), T>(path, command, image, None, rows)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug, Serialize)]
    struct R {
        rva: u32,
        name: &'static str,
    }

    #[test]
    fn the_envelope_names_its_command_and_keeps_every_row() {
        let rows = [
            R {
                rva: 0x1000,
                name: "WinMain",
            },
            R {
                rva: 0x10,
                name: "b",
            },
        ];
        let s = render("segrank", "game.exe", &rows).unwrap();
        assert!(s.ends_with('\n'));
        let v: serde_json::Value = serde_json::from_str(&s).unwrap();
        assert_eq!(v["format"], 1);
        assert_eq!(v["command"], "segrank");
        assert_eq!(v["image"], "game.exe");
        assert_eq!(v["rows"].as_array().unwrap().len(), 2);
        assert_eq!(v["rows"][0]["rva"], 0x1000);
        assert_eq!(v["rows"][1]["name"], "b");
        let empty: [R; 0] = [];
        let v: serde_json::Value =
            serde_json::from_str(&render("rich", "engine.dll", &empty).unwrap()).unwrap();
        assert!(v["rows"].as_array().unwrap().is_empty());
    }

    #[test]
    fn a_summary_is_written_only_when_given() {
        #[derive(Debug, Serialize)]
        struct S {
            md5_identical: bool,
        }
        let rows = [R { rva: 1, name: "a" }];
        let with: serde_json::Value = serde_json::from_str(
            &render_with(
                "score",
                "engine.dll",
                Some(&S {
                    md5_identical: true,
                }),
                &rows,
            )
            .unwrap(),
        )
        .unwrap();
        assert_eq!(with["summary"]["md5_identical"], true);
        let without: serde_json::Value =
            serde_json::from_str(&render("segrank", "engine.dll", &rows).unwrap()).unwrap();
        assert!(without.get("summary").is_none());
    }

    #[test]
    fn the_file_holds_the_rendered_envelope() {
        let p =
            std::env::temp_dir().join(format!("msvc6-base-rowjson-{}.json", std::process::id()));
        let rows = [R { rva: 2, name: "c" }];
        write(&p, "calltargets", "game.exe", &rows).unwrap();
        let text = crate::srcio::own_read(&p).unwrap();
        assert_eq!(text, render("calltargets", "game.exe", &rows).unwrap());
        std::fs::remove_file(&p).unwrap();
    }
}
