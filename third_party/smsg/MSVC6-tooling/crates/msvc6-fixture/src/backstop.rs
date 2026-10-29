use std::collections::BTreeMap;
use std::io::Write as _;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Condvar, Mutex, MutexGuard, Once, PoisonError};
use std::time::{Duration, Instant};

pub const LIMIT: Duration = Duration::from_secs(30);

type Running = BTreeMap<u64, (String, Instant)>;

static CASES: Mutex<Running> = Mutex::new(BTreeMap::new());
static CHANGED: Condvar = Condvar::new();
static PATROL: Once = Once::new();
static NEXT: AtomicU64 = AtomicU64::new(0);

fn cases() -> MutexGuard<'static, Running> {
    CASES.lock().unwrap_or_else(PoisonError::into_inner)
}

fn overrun(what: &str, limit: Duration) -> ! {
    let mut e = std::io::stderr().lock();
    let _ = writeln!(
        e,
        "BACKSTOP: `{what}` is still running after {limit:?}, so it loops; aborting the test binary"
    );
    let _ = e.flush();
    std::process::abort();
}

fn patrol(limit: Duration) {
    let mut running = cases();
    loop {
        let first = running
            .values()
            .min_by_key(|(_, at)| *at)
            .map(|(what, at)| (what.clone(), *at));
        running = match first {
            None => CHANGED
                .wait(running)
                .unwrap_or_else(PoisonError::into_inner),
            Some((what, at)) => {
                let left = limit.saturating_sub(at.elapsed());
                if left.is_zero() {
                    overrun(&what, limit);
                }
                CHANGED
                    .wait_timeout(running, left)
                    .unwrap_or_else(PoisonError::into_inner)
                    .0
            }
        };
    }
}

struct Case(u64);

impl Drop for Case {
    fn drop(&mut self) {
        cases().remove(&self.0);
        CHANGED.notify_all();
    }
}

pub fn run<T>(what: &str, f: impl FnOnce() -> T) -> (T, Duration) {
    PATROL.call_once(|| {
        std::thread::spawn(|| patrol(LIMIT));
    });
    let id = NEXT.fetch_add(1, Ordering::Relaxed);
    let at = Instant::now();
    cases().insert(id, (what.to_string(), at));
    CHANGED.notify_all();
    let case = Case(id);
    let r = f();
    drop(case);
    (r, at.elapsed())
}
