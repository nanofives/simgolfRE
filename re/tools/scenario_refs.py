r"""Reproducible scenario references for re/frida/scenario_ab.py (batch b2, 2026-10-17).

A scenario reference (det_b) is the OFF arm (every hook off) of one canonical or cov_* scenario, replayed from its recorded
schedule under the virtual clock (re/frida/js/scenario_det.js). It is only comparable with a fresh run made under the
same CONDITIONS: the same install files, the same golf_clean.exe, the same audio situation (see re/analysis/data/
scenario_baseline.md: with an audio output device the boot makes one more main-thread clock call in initSound
0x00483e90 than without, so seed_time 0x0051b380 and every RNG-derived word differ from the first checkpoint on), the
same schedule. This tool regenerates references under one procedure and pins them in a manifest:

    # one run (OFF, or ON with a hook set), optionally stopped after N checkpoints, with the opt-in clock log
    ~/bin/py -3.12 re/tools/scenario_refs.py run --scenario sandbox_basic --label probe1 --stop-after 1 --clock-log 40000
    ~/bin/py -3.12 re/tools/scenario_refs.py run --scenario cov_b_inject2 --label on1 --on 0x40f5c0
    # regenerate references: record det_a (schedule), replay det_b and det_c, require det_a == det_b == det_c
    ~/bin/py -3.12 re/tools/scenario_refs.py regen sandbox_basic course_season --jobs 2
    # repeat a scenario N times and report OFF-OFF / ON-OFF identity against the reference
    ~/bin/py -3.12 re/tools/scenario_refs.py variance --scenario cov_b_career --n 10 --arm off --jobs 2
    # compare one finished run with the reference (conditions first: a stale reference is reported as such)
    ~/bin/py -3.12 re/tools/scenario_refs.py check --scenario sandbox_basic --run probe1
    ~/bin/py -3.12 re/tools/scenario_refs.py manifest          # rewrite re/analysis/data/scenario_refs.json

Output root log/c3/alt/refs/<scenario>/ (schedule.json, det_a, det_b, det_c, other runs). The manifest
re/analysis/data/scenario_refs.json records per scenario the schedule's and the reference's sha256, the first
checkpoint's boot words, the build (winmm.dll, golf_clean.exe, c3fy.cpp, batch list) and the conditions.

Scenario families (registered the way the earlier batches' runners did; those runners live in log/c3/, which is not
committed, so this tool needs them on disk): canonical s36a scenarios and cov_k* (log/c3/s36a_run.py: restore of the
saves / themes / score table / log from log/c3/alt/s36a/pristine before every boot), cov_a_* (log/c3/c3fya_run.py: also
the cov_a_flags fixtures and the expected-exit handling of cov_a_quit*), cov_b_* (log/c3/c3fyb_run.py: per-scenario
fixture files), ui_tour / ui_tour_v (log/c3/s36b_run.py: restore from instances\s36b_snap), sandbox_basic and
course_season (scenario_ab.py itself, with s36a's restore).

Every boot holds a boot lock (log/c3/alt/refs/boot.lock, and waits while a sibling batch's lock exists) from before the
spawn to the first checkpoint: two boots at once moved the boot's clock-call count (c3fyc, c3fyk3). Game.kill waits for
the process handle (c3fya: a game still exiting wrote &AutoSave2.sve after the next boot's restore).
"""
from __future__ import annotations

import argparse
import ctypes
import datetime
import hashlib
import json
import os
import pathlib
import platform
import shutil
import socket
import subprocess
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
REFS = ROOT / "log" / "c3" / "alt" / "refs"
MANIFEST = ROOT / "re" / "analysis" / "data" / "scenario_refs.json"
LOCK = REFS / "boot.lock"
SIBLING_LOCKS = [ROOT / "log" / "c3" / "alt" / n / "boot.lock" for n in ("c3fyk1", "c3fyk2", "c3fyk3", "c3fyc")]
DEFAULT_INSTANCES = ["b2", "b2b"]
MANIFEST_VERSION = 1
PY = [sys.executable]                    # the interpreter this tool runs under (~/bin/py is a shell script)

CANONICAL = ["sandbox_basic", "course_season", "championship", "golfer_events", "golfer_stories", "stories_fixture",
             "stories9_fixture", "mood5_fixture", "ui_tour", "ui_tour_v"]


def family(scen: str) -> str:
    if scen.startswith("ui_tour"):
        return "s36b"
    if scen.startswith("cov_a_"):
        return "c3fya"
    if scen.startswith("cov_b_"):
        return "c3fyb"
    return "s36a"            # canonical scenarios, cov_k1/k2/k3 (lockstep entries of scenario.py)


def all_scenarios() -> list[str]:
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import scenario as SCN
    out = list(CANONICAL)
    for n, e in SCN.SCENARIOS.items():
        if n.startswith("cov_") and "lockstep" in e:
            out.append(n)
    return out


# ---------------------------------------------------------------- conditions
def sha256(p: pathlib.Path) -> str | None:
    return hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None


def wave_out_devs() -> int:
    try:
        return int(ctypes.windll.winmm.waveOutGetNumDevs())
    except Exception:  # noqa: BLE001
        return -1


def other_games() -> int:
    """golf_clean.exe processes running when the run starts (tasklist; psutil is not installed here)."""
    try:
        out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq golf_clean.exe", "/NH", "/FO", "CSV"], capture_output=True,
                             text=True, timeout=30).stdout
        return sum(1 for line in out.splitlines() if line.lower().startswith('"golf_clean.exe"'))
    except Exception:  # noqa: BLE001
        return -1


def conditions(instance: pathlib.Path) -> dict:
    """What a run's timeline depends on outside the schedule (written next to every run and into the manifest)."""
    keep = os.environ.get("SIMGOLF_KEEP_SOUND")
    wo = wave_out_devs()
    audio = os.environ.get("C3FT_AUDIO", "none")
    return {"host": socket.gethostname(), "os": platform.platform(), "waveOutGetNumDevs": wo, "audio": audio,
            # scenario_ab pins sound.dll on every PC under the audio stand-in; otherwise game.py's detection decides
            "keep_sound": True if audio == "none" else ((keep == "1") if keep in ("0", "1") else (wo == 0)),
            "winmm_sha256": sha256(instance / "winmm.dll"), "golf_clean_sha256": sha256(instance / "golf_clean.exe"),
            "c3fy_cpp_sha256": sha256(ROOT / "shim" / "src" / "re" / "c3fy.cpp"),
            "scenario_det_js_sha256": sha256(ROOT / "re" / "frida" / "js" / "scenario_det.js"),
            "games_running_at_start": other_games(), "instance": instance.name}


# ---------------------------------------------------------------- boot lock
def lock_acquire(timeout=3600):
    if os.environ.get("SCENARIO_REFS_NO_LOCK") == "1":     # experiments only (b2's concurrent-boot measurement)
        return
    t0 = time.time()
    REFS.mkdir(parents=True, exist_ok=True)
    while True:
        busy = [s for s in SIBLING_LOCKS if s.exists() and time.time() - s.stat().st_mtime < 600]
        if busy and time.time() - t0 < 1800:
            time.sleep(1)
            continue
        try:
            LOCK.mkdir()
            (LOCK / "owner.txt").write_text(f"{os.getpid()} {time.time()}")
            return
        except FileExistsError:
            try:
                if time.time() - LOCK.stat().st_mtime > 600:        # a run killed while holding it
                    shutil.rmtree(LOCK, ignore_errors=True)
                    continue
            except OSError:
                pass
            if time.time() - t0 > timeout:
                raise TimeoutError("boot lock held for an hour")
            time.sleep(0.5)


def instance_lock(instance: pathlib.Path, timeout=7200):
    """One run per instance: log/c3/alt/refs/instance_<name>.lock (a directory, owner pid inside) for the whole run.
    Two runs on one install copy race on its ini [hooks] section and on the restored saves (b2 did that once by hand)."""
    REFS.mkdir(parents=True, exist_ok=True)
    lk = REFS / f"instance_{instance.name}.lock"
    t0 = time.time()
    while True:
        try:
            lk.mkdir()
            (lk / "owner.txt").write_text(f"{os.getpid()} {time.time()}")
            return lk
        except FileExistsError:
            try:
                pid = int((lk / "owner.txt").read_text().split()[0])
                alive = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, text=True,
                                       timeout=30).stdout.find(str(pid)) >= 0
                if not alive:
                    shutil.rmtree(lk, ignore_errors=True)
                    continue
            except (OSError, ValueError, IndexError):
                pass
            if time.time() - t0 > timeout:
                raise TimeoutError(f"{lk} held for {timeout} s")
            time.sleep(2)


def lock_release():
    try:
        if (LOCK / "owner.txt").read_text().split()[0] == str(os.getpid()):
            shutil.rmtree(LOCK, ignore_errors=True)
    except OSError:
        pass


# ---------------------------------------------------------------- family setup (one scenario per process)
def setup(scen: str, instance: pathlib.Path):
    """Import the family runner the scenario was built with and point its outputs at REFS. Returns scenario_ab."""
    if "--instance" in sys.argv:              # scenario_ab and the runners read the full path from argv at import
        sys.argv[sys.argv.index("--instance") + 1] = str(instance)
    else:
        sys.argv[1:1] = ["--instance", str(instance)]
    if "--scenario" not in sys.argv:
        sys.argv += ["--scenario", scen]
    for p in (ROOT / "log" / "c3", ROOT / "re" / "frida", ROOT / "re" / "tools"):
        sys.path.insert(0, str(p))
    fam = family(scen)
    inst_tag = instance.name
    if fam == "s36b":
        import s36b_run  # noqa: F401  (registers the lockstep scenarios, restore from instances\s36b_snap)
        import scenario_ab as S
        import scenario as SCN

        def sc_ui_tour_v(c):   # = log/c3/c3fy2_run.py's ui_tour_v (Sandbox Mode instead of Start New Game -> Easy)
            orig_click, orig_cp = c.click, c.checkpoint
            state = {"after_menu": False, "step": 0}

            def checkpoint(name):
                if name == "menu_done":
                    state["after_menu"] = True
                yield from orig_cp(name)

            def click(x, y, **kw):
                if state["after_menu"] and state["step"] == 0 and (x, y) == (560, 110):
                    state["step"] = 1
                    yield from orig_click(130, 445, **kw)
                    return
                if state["step"] == 1 and (x, y) == (390, 125):
                    state["step"] = 2
                    return
                yield from orig_click(x, y, **kw)

            c.click, c.checkpoint = click, checkpoint
            yield from SCN.ui_tour_lockstep(c)

        S.SCENARIOS["ui_tour_v"] = sc_ui_tour_v
    else:
        if fam == "c3fya":
            R = __import__("c3fya_run")
        elif fam == "c3fyb":
            R = __import__("c3fyb_run")
        else:
            R = None
        import s36a_run as A
        S = A.S
        if fam == "s36a":
            import scenario as SCN
            files = {}
            for n, e in SCN.SCENARIOS.items():
                if n.startswith(("cov_k", "cov_l")) and "lockstep" in e:   # c3fyl: cov_l_* too
                    S.SCENARIOS[n] = e["lockstep"]
                    files[n] = e.get("files", {})
            if files.get(scen):
                base_restore = A.restore_instance

                def restore_files(label):
                    base_restore(label)
                    for rel, srcf in files[scen].items():
                        dst = A.INSTANCE / rel
                        dst.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(ROOT / srcf, dst)

                A.restore_instance = restore_files
        A.MAIN_LOG = REFS / f"main_shim_{inst_tag}.log"
        A.MANIFEST = REFS / f"instance_manifest_{inst_tag}.json"
        A.CHANGES_LOG = REFS / "instance_changes.log"
        REFS.mkdir(parents=True, exist_ok=True)
        if not A.MANIFEST.exists():
            A.MANIFEST.write_text(json.dumps(A._manifest(), indent=0))
        del R
    S.OUT_ROOT = REFS
    # studies only: SCENARIO_REFS_EXTRA=<file.py> defining register(S) adds scenarios (e.g. an older version of one)
    extra = os.environ.get("SCENARIO_REFS_EXTRA")
    if extra:
        import importlib.util
        spec = importlib.util.spec_from_file_location("scenario_refs_extra", extra)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        mod.register(S)
    # Game.kill waits for the process to be gone (c3fya's fix, applied to every family here)
    from game import Game
    if getattr(Game.kill, "__name__", "") != "_kill_and_wait":
        orig_kill = Game.kill

        def _kill_and_wait(self):
            h = ctypes.windll.kernel32.OpenProcess(0x00100000, False, int(self.pid)) if self.pid else 0
            try:
                orig_kill(self)
                if h:
                    ctypes.windll.kernel32.WaitForSingleObject(h, 30000)
            finally:
                if h:
                    ctypes.windll.kernel32.CloseHandle(h)

        Game.kill = _kill_and_wait
    # boot lock: held from before the spawn until the first checkpoint's dump
    base_go, base_dump = S.Run.go, S.Run.take_dump

    def go(self, *a, **kw):
        lock_acquire()
        try:
            return base_go(self, *a, **kw)
        finally:
            lock_release()

    def take_dump(self, sc, name):
        lock_release()
        return base_dump(self, sc, name)

    S.Run.go, S.Run.take_dump = go, take_dump
    return S


def limited(fn, n):
    """The scenario script, ended after its n-th checkpoint (0 = whole script)."""
    if not n:
        return fn

    def g(c):
        k = 0
        for _ in fn(c):
            hit = bool(c.dump_name)
            yield
            if hit:
                k += 1
                if k >= n:
                    return
    return g


# ---------------------------------------------------------------- one run
def cmd_run(a) -> int:
    instance = (ROOT / "instances" / a.instance).resolve()
    if a.clock_log:
        os.environ["C3FT_CLOCK_LOG"] = str(a.clock_log)
    if a.msg_log:
        os.environ["C3FT_MSG_LOG"] = str(a.msg_log)
    if a.trace:
        os.environ["C3FT_TRACE"] = a.trace
    if a.watch:
        os.environ["C3FT_WATCH"] = a.watch
    ilock = instance_lock(instance)
    try:
        return _cmd_run(a, instance)
    finally:
        shutil.rmtree(ilock, ignore_errors=True)


def _cmd_run(a, instance: pathlib.Path) -> int:
    S = setup(a.scenario, instance)
    sd = REFS / a.scenario
    sd.mkdir(parents=True, exist_ok=True)
    sched_path = pathlib.Path(a.schedule) if a.schedule else sd / "schedule.json"
    mode = "record" if a.record else "replay"
    sched = None
    if mode == "replay":
        if not sched_path.exists():
            raise SystemExit(f"no schedule {sched_path}: record first (regen, or run --record)")
        sched = json.loads(sched_path.read_text())
    out = sd / a.label
    if out.exists():
        shutil.rmtree(out)
    dll, backup = instance / "winmm.dll", instance / "winmm.dll.c3ft_main"
    if backup.exists() and sha256(dll) != sha256(backup):
        raise SystemExit(f"{dll} differs from {backup.name}: a killed control run left its DLL (restore it first)")
    main_log = REFS / f"main_shim_{instance.name}.log"
    if a.on:
        if not main_log.exists():
            raise SystemExit(f"no {main_log}: make one OFF run on {instance.name} first")
        reg = S.registered_hooks(main_log)
        on = S.resolve_on_set(a.on, reg)
        on_keys = {(m.lower(), r) for _n, m, r in on}
        off_list = [(m, r) for _n, m, r in reg if (m.lower(), r) not in on_keys]
        on_rvas = {r for _m, r in on_keys}
        S.write_ini_hooks([(m, r) for m, r in off_list if r not in on_rvas])
        env_off = [f"{m}:{r:x}" for m, r in off_list if r in on_rvas]
        env = {"SIMGOLF_HOOKS_OFF": ",".join(env_off)} if env_off else {}
        label = "on"
    else:
        S.write_ini_hooks([])
        env = S.arm_env_off()
        label = "off"
    for kv in a.env or []:
        k, v = kv.split("=", 1)
        env[k] = v
    cond = conditions(instance)
    run = S.Run(label, a.scenario, mode, env, [], [], schedule=sched, out_dir=out)
    rc = 0
    try:
        run.go(limited(S.SCENARIOS[a.scenario], a.stop_after))
    except Exception as e:  # noqa: BLE001
        print(f"RUN FAILED: {type(e).__name__}: {e}", flush=True)
        rc = 2
    finally:
        S.write_ini_hooks([])
        log = instance / "simgolf_shim.log"
        if log.exists():
            shutil.copy2(log, out / "shim.log")
            if not a.on and backup.exists() and sha256(dll) == sha256(backup):
                shutil.copy2(log, main_log)
        cond["on"] = a.on or []
        cond["mode"] = mode
        cond["stop_after"] = a.stop_after
        cond["finished"] = datetime.datetime.now().isoformat(timespec="seconds")
        (out / "conditions.json").write_text(json.dumps(cond, indent=1))
    if mode == "record" and rc == 0:
        sched_path.write_text(json.dumps(run.schedule, indent=1))
        print(f"schedule -> {sched_path}")
    print(f"run {a.scenario}/{a.label}: {'OK' if rc == 0 else 'FAILED'} polls={run.polls} checkpoints={len(run.checkpoints)}")
    return rc


# ---------------------------------------------------------------- comparison helpers (no game)
def _S():
    sys.path.insert(0, str(ROOT / "re" / "frida"))
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    if "--instance" not in sys.argv:
        sys.argv[1:1] = ["--instance", str(ROOT / "instances" / DEFAULT_INSTANCES[0])]
    import scenario_ab as S
    return S


def run_ok(d: pathlib.Path) -> tuple[bool, str]:
    """A run is scorable only when it finished: summary.json present, no failure, at least one checkpoint."""
    p = d / "summary.json"
    if not p.exists():
        return False, "no summary"
    s = json.loads(p.read_text())
    if s.get("failure"):
        return False, "failure: " + s["failure"][:200]
    if not s.get("checkpoints"):
        return False, "no checkpoint"
    return True, ""


def compare(a_dir: pathlib.Path, b_dir: pathlib.Path, common_only=True) -> dict:
    """{'same': bool, 'first': checkpoint name or None, 'n': compared checkpoints, 'detail': {...}}; checkpoints present
    only in b (a short run) are skipped when common_only."""
    S = _S()
    cmp = S.compare_runs(a_dir, b_dir)
    first, n = None, 0
    for name, r in cmp.items():
        if r.get("missing"):
            if common_only:
                continue
            first = first or name
            continue
        n += 1
        if (r["regions_differ"] or r["frame_px_differ"]) and first is None:
            first = name
    return {"same": first is None and n > 0, "first": first, "n": n, "detail": cmp}


def describe_diff(r: dict) -> str:
    regs = ",".join(f"{d['region']}({d['bytes']})" for d in r["regions_differ"][:6])
    return f"state {'SAME' if not r['regions_differ'] else regs} frame {r['frame_px_differ']} px"


def summarize(c: dict) -> str:
    if c["same"]:
        return f"SAME ({c['n']} checkpoints)"
    if c["first"] is None:
        return "NOTHING COMPARED"
    r = c["detail"][c["first"]]
    if r.get("missing"):
        return f"DIFF at {c['first']}: missing"
    return f"DIFF at {c['first']}: {describe_diff(r)}"


def boot_words(d: pathlib.Path) -> dict:
    """seed_time 0x0051b380, title ticker 0x00822c78, RNG seed 0x00822d9c and the state hash at the first checkpoint."""
    import struct
    S = _S()
    s = json.loads((d / "summary.json").read_text())
    cps = list(s["checkpoints"])
    b = (d / f"{cps[0]}.bin").read_bytes()
    out, off = {}, 0
    want = {0x0051B380: "seed_time", 0x00822C78: "ticker", 0x00822D9C: "rng_seed"}
    for va, n, _ in S.regions():
        for w, name in want.items():
            if va <= w < va + n:
                out[name] = f"0x{struct.unpack_from('<I', b, off + w - va)[0]:08x}"
        off += n
    st = s["checkpoints"][cps[0]]["status"]
    out.update({"checkpoint": cps[0], "state_sha16": hashlib.sha256(b).hexdigest()[:16],
                "frame_sha16": s["checkpoints"][cps[0]]["frame_sha"][:16], "T": st["T"] - 0x00100000,
                "forwarded": st["forwarded"]})
    return out


def clock_digest(d: pathlib.Path) -> str:
    """(count, sha256 prefix) of a run's clock log sequence (kind, slot module, caller) when it was recorded."""
    p = d / "clock.json"
    if not p.exists():
        return "-"
    rows = json.loads(p.read_text())["rows"]
    h = hashlib.sha256(json.dumps([r[2:] for r in rows]).encode()).hexdigest()[:12]
    return f"{len(rows)}:{h}"


def ref_digest(d: pathlib.Path) -> str:
    """sha256 over the reference's checkpoint dumps and frame pixel hashes, in checkpoint order."""
    s = json.loads((d / "summary.json").read_text())
    h = hashlib.sha256()
    for name, cp in s["checkpoints"].items():
        h.update(name.encode() + b"\0" + (d / f"{name}.bin").read_bytes() + cp["frame_sha"].encode())
    return h.hexdigest()


# ---------------------------------------------------------------- subprocess pool over the instances
class Pool:
    def __init__(self, instances: list[str], jobs: int):
        self.free = list(instances[:max(1, jobs)])
        self.cv = threading.Condition()
        self.threads: list[threading.Thread] = []
        self.results: dict = {}

    def submit(self, key, argv: list[str], log: pathlib.Path):
        def work():
            with self.cv:
                while not self.free:
                    self.cv.wait()
                inst = self.free.pop(0)
            try:
                log.parent.mkdir(parents=True, exist_ok=True)
                with log.open("w") as f:
                    rc = subprocess.call(PY + [str(pathlib.Path(__file__)), "run", "--instance", inst] + argv,
                                         stdout=f, stderr=subprocess.STDOUT, cwd=str(ROOT))
                self.results[key] = (rc, inst)
            finally:
                with self.cv:
                    self.free.append(inst)
                    self.cv.notify_all()
        t = threading.Thread(target=work, daemon=True)
        t.start()
        self.threads.append(t)
        time.sleep(1)                       # keep the submission order on the lock

    def wait(self):
        for t in self.threads:
            t.join()
        self.threads = []


# ---------------------------------------------------------------- regen
def cmd_regen(a) -> int:
    scens = all_scenarios() if a.scenarios == ["all"] else a.scenarios
    REFS.mkdir(parents=True, exist_ok=True)
    pool = Pool(a.instances, a.jobs)
    status = {}
    for scen in scens:
        sd = REFS / scen
        sd.mkdir(parents=True, exist_ok=True)
        if a.rerecord or not (sd / "schedule.json").exists():
            if (sd / "schedule.json").exists():
                shutil.move(str(sd / "schedule.json"), str(sd / f"schedule.prev_{int(time.time())}.json"))
            pool.submit((scen, "det_a"), ["--scenario", scen, "--label", "det_a", "--record"], sd / "det_a.log")
    pool.wait()
    for scen in scens:
        if not (REFS / scen / "schedule.json").exists():
            status[scen] = "RECORD FAILED"
            continue
        for lab in ("det_b", "det_c"):
            pool.submit((scen, lab), ["--scenario", scen, "--label", lab], REFS / scen / f"{lab}.log")
    pool.wait()
    for scen in scens:
        if scen in status:
            continue
        sd = REFS / scen
        oks = {lab: run_ok(sd / lab) for lab in ("det_a", "det_b", "det_c") if (sd / lab).exists()}
        bad = [f"{k}: {v[1]}" for k, v in oks.items() if not v[0]]
        if bad or "det_b" not in oks or "det_c" not in oks:
            status[scen] = "RUN FAILED " + "; ".join(bad)
            continue
        bc = compare(sd / "det_b", sd / "det_c", common_only=False)
        ab = compare(sd / "det_a", sd / "det_b", common_only=False) if "det_a" in oks else None
        verdict = {"det_b_vs_det_c": summarize(bc), "det_a_vs_det_b": summarize(ab) if ab else "not recorded here"}
        (sd / "regen.json").write_text(json.dumps(verdict, indent=1))
        status[scen] = "PINNED" if bc["same"] and (ab is None or ab["same"]) else "NOT DETERMINISTIC"
        status[scen] += f"  b/c {verdict['det_b_vs_det_c']}  a/b {verdict['det_a_vs_det_b']}"
    for scen, s in status.items():
        print(f"{scen:24s} {s}")
    write_manifest()
    return 0 if all(s.startswith("PINNED") for s in status.values()) else 1


# ---------------------------------------------------------------- manifest
def write_manifest() -> dict:
    old = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {}
    entries = dict(old.get("scenarios", {}))
    for sd in sorted(p for p in REFS.iterdir() if p.is_dir()) if REFS.exists() else []:
        ref, rg = sd / "det_b", sd / "regen.json"
        if not rg.exists() or not run_ok(ref)[0]:
            continue
        verdict = json.loads(rg.read_text())
        if not verdict["det_b_vs_det_c"].startswith("SAME"):
            continue
        cond = json.loads((ref / "conditions.json").read_text()) if (ref / "conditions.json").exists() else {}
        s = json.loads((ref / "summary.json").read_text())
        entries[sd.name] = {
            "family": family(sd.name),
            "reference": (ref.relative_to(ROOT)).as_posix(),
            "schedule_sha256": sha256(sd / "schedule.json"),
            "reference_sha256": ref_digest(ref),
            "checkpoints": list(s["checkpoints"]),
            "first_checkpoint": boot_words(ref),
            "verified": verdict,
            "build": {k: cond.get(k) for k in ("winmm_sha256", "golf_clean_sha256", "c3fy_cpp_sha256",
                                                "scenario_det_js_sha256")},
            "conditions": {k: cond.get(k) for k in ("host", "waveOutGetNumDevs", "keep_sound", "instance")},
            "sound_live": boot_words(ref)["forwarded"] > 0,
            "recorded": cond.get("finished"),
        }
    doc = {"version": MANIFEST_VERSION, "root": REFS.relative_to(ROOT).as_posix(),
           "procedure": "re/tools/scenario_refs.py regen (det_a record, det_b / det_c replay, det_b == det_c "
                        "required); checklist re/analysis/data/scenario_baseline.md",
           "updated": datetime.datetime.now().isoformat(timespec="seconds"), "scenarios": entries}
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(doc, indent=1, sort_keys=False) + "\n")
    print(f"manifest: {len(entries)} scenarios -> {MANIFEST.relative_to(ROOT).as_posix()}")
    return doc


def cmd_manifest(a) -> int:
    write_manifest()
    return 0


# ---------------------------------------------------------------- check
def stale_reasons(ref: pathlib.Path, run: pathlib.Path) -> list[str]:
    """Conditions that make two runs incomparable (re/analysis/data/scenario_baseline.md): another golf_clean.exe, or
    another audio path (scenario_ab.arm_conditions_differ). Another host is reported as a note only."""
    S = _S()
    out = []
    sr = json.loads((ref / "summary.json").read_text())
    sn = json.loads((run / "summary.json").read_text())
    d = S.arm_conditions_differ(sr, sn)
    if d:
        out.append(d)
    try:
        cr = json.loads((ref / "conditions.json").read_text())
        cn = json.loads((run / "conditions.json").read_text())
    except OSError:
        return out + ["conditions.json missing (a run not made by this tool): compare the boot words by hand"]
    if cr.get("golf_clean_sha256") != cn.get("golf_clean_sha256"):
        out.append(f"golf_clean.exe {cr.get('golf_clean_sha256')} / {cn.get('golf_clean_sha256')}")
    if cr.get("host") != cn.get("host"):
        print(f"note: reference made on {cr.get('host')}, run on {cn.get('host')}")
    return out


def cmd_check(a) -> int:
    sd = REFS / a.scenario
    ref, run = sd / a.ref, sd / a.run
    ok, why = run_ok(run)
    if not ok:
        print(f"ERROR: {a.run} is not scorable ({why})")
        return 2
    st = stale_reasons(ref, run)
    for s in st:
        print(f"STALE CONDITION: {s}")
    c = compare(run, ref)
    print(f"{a.scenario} {a.run} vs {a.ref}: {summarize(c)}")
    wr, wn = boot_words(ref), boot_words(run)
    if wr != wn:
        print(f"  first checkpoint: ref {wr}\n                    run {wn}")
    return 0 if c["same"] else 1


# ---------------------------------------------------------------- variance
def cmd_variance(a) -> int:
    sd = REFS / a.scenario
    pool = Pool(a.instances, a.jobs)
    labels = [f"{a.prefix}{a.arm}{i:02d}" for i in range(1, a.n + 1)]
    for lab in labels:
        argv = ["--scenario", a.scenario, "--label", lab]
        if a.stop_after:
            argv += ["--stop-after", str(a.stop_after)]
        if a.clock_log:
            argv += ["--clock-log", str(a.clock_log)]
        if a.msg_log:
            argv += ["--msg-log", str(a.msg_log)]
        if a.trace:
            argv += ["--trace", a.trace]
        if a.arm == "on":
            argv += ["--on", *a.on]
        pool.submit(lab, argv, sd / f"{lab}.log")
    pool.wait()
    rows, base = [], None
    for lab in labels:
        ok, why = run_ok(sd / lab)
        if not ok:
            rows.append((lab, "ERROR", why))
            continue
        vs_ref = compare(sd / lab, sd / a.ref) if (sd / a.ref).exists() else None
        if base is None:
            base = lab
        vs_first = compare(sd / lab, sd / base)
        bw = boot_words(sd / lab)
        rows.append((lab, summarize(vs_ref) if vs_ref else "-", summarize(vs_first), bw["seed_time"], bw["ticker"],
                     bw["state_sha16"], bw["forwarded"], clock_digest(sd / lab)))
    scored = [r for r in rows if r[1] != "ERROR"]
    same_ref = sum(1 for r in scored if r[1].startswith("SAME"))
    same_first = sum(1 for r in scored if r[2].startswith("SAME"))
    for r in rows:
        print("  " + " | ".join(str(x) for x in r))
    clocks = sorted({r[7] for r in scored})
    print(f"variance {a.scenario} arm={a.arm}: N={len(rows)} scored={len(scored)} identical_to_{a.ref}={same_ref} "
          f"identical_to_first={same_first} distinct_clock_sequences={len(clocks)} {clocks[:4]}")
    (sd / f"variance_{a.prefix}{a.arm}.json").write_text(json.dumps(
        {"scenario": a.scenario, "arm": a.arm, "on": a.on, "rows": rows, "N": len(rows), "scored": len(scored),
         "identical_to_ref": same_ref, "identical_to_first": same_first, "ref": a.ref,
         "clock_sequences": clocks}, indent=1))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run", help="one boot (OFF, or ON with --on)")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--instance", default=DEFAULT_INSTANCES[0])
    p.add_argument("--record", action="store_true", help="record mode: screen waits, writes the schedule")
    p.add_argument("--schedule", help="schedule file (default log/c3/alt/refs/<scenario>/schedule.json)")
    p.add_argument("--on", nargs="+", help="hook targets ON (module:0xaddr, exe bare VA, or `all`); others OFF")
    p.add_argument("--env", nargs="*", help="extra KEY=VALUE for the game")
    p.add_argument("--stop-after", type=int, default=0, help="end after this many checkpoints")
    p.add_argument("--clock-log", type=int, default=0, help="record the first N main-thread clock calls (clock.json)")
    p.add_argument("--msg-log", type=int, default=0, help="record the first N main-thread windowProc entries (wndmsg.json)")
    p.add_argument("--trace")
    p.add_argument("--watch")
    p.set_defaults(fn=cmd_run)
    p = sp.add_parser("regen", help="record + two replays per scenario; manifest")
    p.add_argument("scenarios", nargs="+", help="names, or `all`")
    p.add_argument("--rerecord", action="store_true", help="record a new schedule even if one exists")
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--instances", nargs="+", default=DEFAULT_INSTANCES)
    p.set_defaults(fn=cmd_regen)
    p = sp.add_parser("variance", help="N runs of one scenario, identity against the reference and the first run")
    p.add_argument("--scenario", required=True)
    p.add_argument("--n", type=int, default=10)
    p.add_argument("--arm", choices=["off", "on"], default="off")
    p.add_argument("--on", nargs="+", default=["0x40f5c0"])
    p.add_argument("--prefix", default="v_")
    p.add_argument("--ref", default="det_b")
    p.add_argument("--stop-after", type=int, default=0)
    p.add_argument("--clock-log", type=int, default=0)
    p.add_argument("--msg-log", type=int, default=0)
    p.add_argument("--trace")
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--instances", nargs="+", default=DEFAULT_INSTANCES)
    p.set_defaults(fn=cmd_variance)
    p = sp.add_parser("check", help="compare one finished run with the reference")
    p.add_argument("--scenario", required=True)
    p.add_argument("--run", required=True)
    p.add_argument("--ref", default="det_b")
    p.set_defaults(fn=cmd_check)
    p = sp.add_parser("manifest")
    p.set_defaults(fn=cmd_manifest)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
