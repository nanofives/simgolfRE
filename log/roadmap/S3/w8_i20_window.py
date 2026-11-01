"""Quiet window for I20's re-runs (orchestrator, 2026-10-31): when `[queue done]` appears in
log/roadmap/merges/I20/g_gates.log, take the S3 heavy lock as step I20-rerun with --priority (the running S3 hold
finishes first, no new one starts), hold it until `[rerun done]` appears, then release. Deadline 3 h from start:
release anyway and log it. Log: log/roadmap/S3/w8_i20_window.log."""
import subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GATES = ROOT / "log/roadmap/merges/I20/g_gates.log"
LOCK = [sys.executable, str(ROOT / "log/roadmap/S3/w8_lock.py")]
LOG = ROOT / "log/roadmap/S3/w8_i20_window.log"
DEADLINE = time.time() + 3 * 3600


def log(msg):
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(time.strftime("%Y-%m-%d %H:%M:%S") + "\t" + msg + "\n")


def has(marker):
    try:
        return marker in GATES.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return False


def wait_for(marker):
    while not has(marker):
        if time.time() > DEADLINE:
            return False
        time.sleep(30)
    return True


log("started; waiting for [queue done]")
if not wait_for("[queue done]"):
    log("deadline before [queue done]; nothing taken")
    sys.exit(1)
log("[queue done] seen; taking the lock as I20-rerun (priority)")
rc = subprocess.call(LOCK + ["take", "I20-rerun", "--priority", "--what", "quiet window for I20 re-runs", "--need-gb", "0"])
if rc != 0:
    log(f"take failed rc={rc}")
    sys.exit(rc)
log("lock held; waiting for [rerun done]")
ok = wait_for("[rerun done]")
log("[rerun done] seen" if ok else "DEADLINE (3 h) before [rerun done]; releasing anyway")
subprocess.call(LOCK + ["release", "I20-rerun"])
log("released; window closed")
