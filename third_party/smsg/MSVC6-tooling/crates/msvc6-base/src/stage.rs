use std::sync::OnceLock;
use std::time::Instant;

pub const TIMING_ENV: &str = "MSVC6T_TIMING";

fn on() -> bool {
    static ON: OnceLock<bool> = OnceLock::new();
    *ON.get_or_init(|| std::env::var(TIMING_ENV).is_ok_and(|v| v == "1"))
}

#[derive(Debug)]
pub struct Stage {
    name: &'static str,
    detail: String,
    start: Option<Instant>,
}

impl Stage {
    pub fn note(&mut self, d: impl Into<String>) {
        if self.start.is_some() {
            self.detail = d.into();
        }
    }

    #[must_use]
    pub fn line(&self) -> Option<String> {
        self.start.map(|s| {
            format!(
                "[t] {} {} ms {}",
                self.name,
                s.elapsed().as_millis(),
                self.detail
            )
        })
    }
}

fn started(name: &'static str, on: bool, detail: impl FnOnce() -> String) -> Stage {
    let start = on.then(Instant::now);
    Stage {
        name,
        detail: if start.is_some() {
            detail()
        } else {
            String::new()
        },
        start,
    }
}

#[must_use]
pub fn stage(name: &'static str, detail: impl FnOnce() -> String) -> Stage {
    started(name, on(), detail)
}

impl Drop for Stage {
    fn drop(&mut self) {
        if let Some(l) = self.line() {
            eprintln!("{l}");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_off_stage_costs_its_detail_nothing_and_prints_nothing() {
        let mut ran = false;
        let mut s = started("compile", false, || {
            ran = true;
            String::from("x")
        });
        s.note("y");
        assert!(!ran);
        assert_eq!(s.line(), None);
    }

    #[test]
    fn an_on_stage_names_itself_its_time_and_its_last_note() {
        let mut s = started("compile", true, || String::from("first"));
        s.note("second");
        let l = s.line().unwrap();
        assert!(
            l.starts_with("[t] compile ") && l.ends_with(" ms second"),
            "{l}"
        );
    }
}
