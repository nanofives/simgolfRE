"""S3 wave 8 heavy-run lock (W8_HEAVY_RULE.md), enforced in code.

    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py take <step> [--what "<run>"] [--need-gb 16] [--max-wait <s>]
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py release <step>
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run <step> [--what "<run>"] -- <command ...>
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py status

--priority (lead / orchestrator only, e.g. step I20-rerun): the wait file gets epoch 0, so it ranks before every
  waiter; the running hold still finishes first.
take: writes log/roadmap/S3/w8_wait_<step> (epoch, time, pid of this take process, what), then blocks until
  (1) that wait file is the OLDEST live wait file, (2) the lock dir w8_heavy.lock is free, (3) at least
  --need-gb GB of commit charge is free. Then it creates the lock (mkdir, atomic), writes owner.txt and
  deletes its own wait file. Exit 0 = you hold the lock; exit 2 = --max-wait ran out (wait file removed).
release: removes the lock if owner.txt names <step> (else refuses, exit 1) and any wait file of <step>.
run: take, run the command, release in all cases (also on Ctrl-C / error); exit code = the command's.

The age of a wait file is the epoch it records; a file without one (written by hand) counts from its
mtime. A wait file whose take pid is no longer alive is stale and is removed. A lock is never broken
automatically: `status` shows who holds it and for how long.
"""
import ctypes, os, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
LOCK = HERE / "w8_heavy.lock"
POLL = 10.0


class _MS(ctypes.Structure):
    _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong),
                ("ullTotalPhys", ctypes.c_ulonglong), ("ullAvailPhys", ctypes.c_ulonglong),
                ("ullTotalPageFile", ctypes.c_ulonglong), ("ullAvailPageFile", ctypes.c_ulonglong),
                ("ullTotalVirtual", ctypes.c_ulonglong), ("ullAvailVirtual", ctypes.c_ulonglong),
                ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]


def free_commit_gb():
    m = _MS(); m.dwLength = ctypes.sizeof(_MS)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
    return m.ullAvailPageFile / 2**30


def pid_alive(pid):
    k = ctypes.windll.kernel32
    h = k.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return False
    code = ctypes.c_ulong()
    ok = k.GetExitCodeProcess(h, ctypes.byref(code))
    k.CloseHandle(h)
    return bool(ok) and code.value == 259  # STILL_ACTIVE


def wait_path(step):
    return HERE / f"w8_wait_{step}"


def read_kv(p):
    kv = {}
    try:
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                kv[k.strip()] = v.strip()
    except FileNotFoundError:
        return None
    return kv


def waiters():
    """Live wait files as (epoch, step, path), oldest first; stale ones (dead take pid) removed."""
    out = []
    for p in HERE.glob("w8_wait_*"):
        kv = read_kv(p)
        if kv is None:
            continue
        step = p.name[len("w8_wait_"):]
        pid = kv.get("pid")
        if pid and pid.isdigit() and not pid_alive(int(pid)):
            print(f"[w8_lock] removing stale wait file of {step} (take pid {pid} is gone)", flush=True)
            p.unlink(missing_ok=True)
            continue
        try:
            epoch = float(kv["epoch"])
        except (KeyError, ValueError):
            try:
                epoch = p.stat().st_mtime
            except FileNotFoundError:
                continue
        out.append((epoch, step, p))
    out.sort()
    return out


def owner():
    try:
        return (LOCK / "owner.txt").read_text(encoding="utf-8", errors="replace").strip()
    except FileNotFoundError:
        return "(no owner.txt)" if LOCK.exists() else None


def owner_step():
    kv = read_kv(LOCK / "owner.txt")
    if kv and "step" in kv:
        return kv["step"]
    o = owner()  # hand-written owner.txt: "<step> <what> <time>"
    return o.split()[0] if o else None


_seen_breach = set()


def note_breach(owner_line, waiter):
    """A lock taken by hand (owner.txt without step=) while a w8_lock.py take waits: log it once."""
    if owner_line in _seen_breach:
        return
    _seen_breach.add(owner_line)
    with open(HERE / "w8_breaches.log", "a", encoding="utf-8") as f:
        f.write(f"{stamp()}\tlock taken by hand: {owner_line!r}\twhile {waiter} waited via w8_lock.py\n")
    print(f"[w8_lock] BREACH logged: hand-taken lock {owner_line!r}", flush=True)


def stamp():
    return time.strftime("%Y-%m-%d %H:%M:%S")


def take(step, what="", need_gb=16.0, max_wait=None, priority=False):
    wp = wait_path(step)
    if wp.exists():
        kv = read_kv(wp) or {}
        if kv.get("pid", "").isdigit() and pid_alive(int(kv["pid"])) and int(kv["pid"]) != os.getpid():
            print(f"[w8_lock] {step} already has a live take (pid {kv['pid']})", flush=True)
            return 1
    if owner_step() == step:
        print(f"[w8_lock] {step} already holds the lock; release it first", flush=True)
        return 1
    t0 = time.time()
    epoch = t0
    if priority:
        epoch = 0.0
        # Alternation: a priority step that also made the LAST hold yields one turn: it queues right behind the oldest other
        # waiter instead of ahead of everyone, so ordinary steps get one hold between consecutive priority holds.
        try:
            last = (HERE / "w8_last_release.txt").read_text(encoding="utf-8").split()[0]
        except (FileNotFoundError, IndexError):
            last = None
        others = [e for e, s, _ in waiters() if s != step]
        if last == step and others:
            epoch = min(others) + 0.001
            print(f"[w8_lock] {step} priority yields one turn (it made the last hold)", flush=True)
    wp.write_text(f"step={step}\nepoch={epoch:.3f}\ntime={stamp()}\npid={os.getpid()}\nwhat={what}\n", encoding="utf-8")
    print(f"[w8_lock] {step} queued at {stamp()}", flush=True)
    last_msg = ""
    try:
        while True:
            ws = waiters()
            first = ws[0][1] if ws else None
            held = owner()
            gb = free_commit_gb()
            if first == step and held is None and gb >= need_gb:
                try:
                    LOCK.mkdir()
                except FileExistsError:
                    time.sleep(1)
                    continue
                (LOCK / "owner.txt").write_text(
                    f"step={step}\nwhat={what}\ntime={stamp()}\nepoch={time.time():.3f}\n", encoding="utf-8")
                wp.unlink(missing_ok=True)
                print(f"[w8_lock] {step} TOOK the lock at {stamp()} (waited {time.time() - t0:.0f} s, "
                      f"free commit {gb:.1f} GB)", flush=True)
                return 0
            if held is not None and "step=" not in held:
                note_breach(held.splitlines()[0] if held.strip() else "(empty owner.txt)", step)
            order = [s for _, s, _ in ws]
            ahead = order[: order.index(step)] if step in order else order
            msg = (f"[w8_lock] {step} waiting: lock={'free' if held is None else held.splitlines()[0]!r} "
                   f"ahead={ahead} free_commit={gb:.1f} GB")
            if msg != last_msg or int(time.time() - t0) % 300 < POLL:
                print(msg, flush=True)
                last_msg = msg
            if max_wait is not None and time.time() - t0 > max_wait:
                print(f"[w8_lock] {step} gave up after {max_wait} s", flush=True)
                wp.unlink(missing_ok=True)
                return 2
            time.sleep(POLL)
    except BaseException:
        wp.unlink(missing_ok=True)
        raise


def release(step):
    os_ = owner_step()
    wait_path(step).unlink(missing_ok=True)
    if os_ is None:
        print(f"[w8_lock] lock is not held; nothing to release", flush=True)
        return 0
    if os_ != step:
        print(f"[w8_lock] lock is held by {os_!r}, not {step}; refused", flush=True)
        return 1
    (LOCK / "owner.txt").unlink(missing_ok=True)
    for p in LOCK.iterdir():
        p.unlink()
    LOCK.rmdir()
    try:
        (HERE / "w8_last_release.txt").write_text(f"{step} {time.time():.3f}\n", encoding="utf-8")
    except OSError:
        pass
    print(f"[w8_lock] {step} released at {stamp()}", flush=True)
    return 0


def status():
    o = owner()
    print("lock:", "free" if o is None else o.replace("\n", "; "))
    if o is not None:
        kv = read_kv(LOCK / "owner.txt") or {}
        t = float(kv["epoch"]) if "epoch" in kv else (LOCK / "owner.txt").stat().st_mtime if (LOCK / "owner.txt").exists() else LOCK.stat().st_mtime
        print(f"held for {(time.time() - t) / 60:.0f} min")
    for i, (e, s, p) in enumerate(waiters()):
        print(f"queue {i + 1}: {s} since {time.strftime('%H:%M:%S', time.localtime(e))}")
    print(f"free commit {free_commit_gb():.1f} GB")
    return 0


def main(argv):
    if not argv or argv[0] not in ("take", "release", "run", "status"):
        print(__doc__)
        return 1
    if argv[0] == "status":
        return status()
    if len(argv) < 2:
        print(__doc__)
        return 1
    cmd, step, rest = argv[0], argv[1], argv[2:]
    what, need, max_wait, command, priority = "", 16.0, None, [], False
    if "--" in rest:
        i = rest.index("--")
        rest, command = rest[:i], rest[i + 1:]
    it = iter(rest)
    for a in it:
        if a == "--what":
            what = next(it)
        elif a == "--need-gb":
            need = float(next(it))
        elif a == "--priority":
            priority = True
        elif a == "--max-wait":
            max_wait = float(next(it))
        else:
            print(f"unknown argument {a}")
            return 1
    if cmd == "take":
        return take(step, what, need, max_wait, priority)
    if cmd == "release":
        return release(step)
    if not command:
        print("run needs `-- <command ...>`")
        return 1
    rc = take(step, what or " ".join(command)[:200], need, max_wait)
    if rc:
        return rc
    try:
        return subprocess.call(command)
    finally:
        release(step)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
