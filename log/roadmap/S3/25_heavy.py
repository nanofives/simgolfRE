"""S3.25: run ONE command under the wave 8 heavy lock (log/roadmap/S3/W8_HEAVY_RULE.md).

    ~/bin/py -3.12 log/roadmap/S3/25_heavy.py "<what>" -- <command...>

Creates log/roadmap/S3/w8_wait_S3.25 while waiting, takes log/roadmap/S3/w8_heavy.lock (mkdir), checks free commit >= 16 GB
(w8_commit_free.py; below it releases, waits 5 min, retries), writes owner.txt, deletes the wait file, runs the command with its
output on this process's stdout, releases the lock. After a release of ours it waits 2 min and never re-takes while another
step's w8_wait_* file exists (fair turns).
"""
from __future__ import annotations

import datetime
import pathlib
import shutil
import subprocess
import sys
import time

S3 = pathlib.Path(__file__).resolve().parent
LOCK = S3 / "w8_heavy.lock"
WAIT = S3 / "w8_wait_S3.25"
LAST = S3 / "25_last_release.txt"
PY = sys.executable          # ~/bin/py is a shell script, which CreateProcess cannot start


def now() -> str:
    return datetime.datetime.now().strftime("%H:%M:%S")


def others_waiting() -> list[str]:
    """other steps' wait files older than ours (the lead's order, ORCH_LOG 23:03: the oldest wait file goes next)"""
    mine = WAIT.stat().st_mtime if WAIT.exists() else time.time()
    out = []
    for p in S3.glob("w8_wait_*"):
        try:
            if p.name != WAIT.name and p.stat().st_mtime < mine:
                out.append(p.name)
        except FileNotFoundError:
            pass
    return out


def commit_ok() -> bool:
    return subprocess.run([PY, str(S3 / "w8_commit_free.py")]).returncode == 0


def acquire(what: str) -> None:
    if not WAIT.exists():           # a restarted wrapper keeps its place in the queue (the file's mtime)
        WAIT.write_text(f"S3.25 {what} waiting since {now()}\n")
    said = ""
    while True:
        if others_waiting() or (LAST.exists() and time.time() - LAST.stat().st_mtime < 120):
            since = time.time() - LAST.stat().st_mtime if LAST.exists() else 0
            if True:
                msg = f"[heavy] {now()} waiting behind older waits {others_waiting()} ({int(since)} s since our release)"
                if msg[:40] != said:
                    print(msg, flush=True)
                    said = msg[:40]
                time.sleep(20)
                continue
        if not LOCK.exists() and not commit_ok():
            # the commit check before the lock is taken (the rule's check stays after it too): no take-and-release churn
            if "commit" not in said:
                print(f"[heavy] {now()} free commit below 16 GB: waiting (lock not taken)", flush=True)
                said = "commit"
            time.sleep(60)
            continue
        try:
            LOCK.mkdir()
        except FileExistsError:
            own = (LOCK / "owner.txt").read_text().strip() if (LOCK / "owner.txt").exists() else "?"
            msg = f"[heavy] {now()} lock held by: {own}"
            if msg[18:] != said:
                print(msg, flush=True)
                said = msg[18:]
            time.sleep(20)
            continue
        (LOCK / "owner.txt").write_text(f"S3.25 {what} {now()}\n")
        if not commit_ok():
            shutil.rmtree(LOCK, ignore_errors=True)
            print(f"[heavy] {now()} free commit below 16 GB: released, retry in 5 min", flush=True)
            time.sleep(300)
            continue
        WAIT.unlink(missing_ok=True)
        print(f"[heavy] {now()} lock taken: {what}", flush=True)
        return


def release() -> None:
    own = (LOCK / "owner.txt").read_text() if (LOCK / "owner.txt").exists() else ""
    if own.startswith("S3.25"):
        shutil.rmtree(LOCK, ignore_errors=True)
        LAST.write_text(now() + "\n")
        print(f"[heavy] {now()} lock released", flush=True)
    else:
        print(f"[heavy] {now()} lock not ours ({own.strip()!r}): left alone", flush=True)


def main() -> int:
    if "--" not in sys.argv:
        raise SystemExit(__doc__)
    i = sys.argv.index("--")
    what, cmd = " ".join(sys.argv[1:i]), sys.argv[i + 1:]
    if cmd and cmd[0] in ("py", "~/bin/py") or cmd[0].replace("\\", "/").endswith("/bin/py"):
        cmd = [PY] + (cmd[2:] if len(cmd) > 1 and cmd[1] == "-3.12" else cmd[1:])
    try:
        acquire(what)
    except BaseException:
        own = (LOCK / "owner.txt").read_text() if (LOCK / "owner.txt").exists() else ""
        if own.startswith("S3.25"):
            shutil.rmtree(LOCK, ignore_errors=True)
        WAIT.unlink(missing_ok=True)
        raise
    try:
        return subprocess.run(cmd).returncode
    finally:
        release()
        WAIT.unlink(missing_ok=True)


if __name__ == "__main__":
    sys.exit(main())
