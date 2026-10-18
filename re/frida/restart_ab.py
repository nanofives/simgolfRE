r"""Phase 2.1 premise check (re/PHASE23_PLAN.md 2.1): does the ORIGINAL game reproduce checkpoint k+1 when it is restarted
from a state image of checkpoint k? Built on re/frida/scenario_ab.py's deterministic runs (virtual clock, replayed
schedule, checkpoint dumps) and the opt-in IMAGE block of re/frida/js/scenario_det.js (cfg.image).

The restart instant is a FRAME BOUNDARY: the first main-thread timeGetTime call after a checkpoint poll whose return
address is 0x0040fc2f, i.e. mainLoop's frame head `call [0x004ba21c]` at 0x0040fc29 (re/analysis/sim/mainLoop_structure.md).

    # 1. a continuous run that also CAPTURES a state image at the frame head after the listed checkpoints (golf_clean.exe's
    #    .data section, the main-thread stack around the site, the agent's clock / event / timer / input state); every
    #    checkpoint also dumps the whole .data section (<name>.full.bin) and the DLLs' writable ranges (<name>.dll.bin)
    ~/bin/py -3.12 re/frida/restart_ab.py capture --scenario sandbox_basic --label c_a --at sandbox_00 sandbox_20
    ~/bin/py -3.12 re/frida/restart_ab.py capture --scenario sandbox_basic --label c_b --at sandbox_00 sandbox_20
    # 2. a RESTART run: a fresh process replays the scenario up to checkpoint `--from`, and at the next frame head writes the
    #    image of checkpoint `--image` taken by run `--source` (bytes that differ between --source and --noise, i.e. heap
    #    pointers and handles of that process, keep the fresh process's own values), hands over the clock, events, timers
    #    and input, and continues the source's schedule from there
    ~/bin/py -3.12 re/frida/restart_ab.py restart --scenario sandbox_basic --label r_data --source c_a --noise c_b \
        --from sandbox_00 --image sandbox_20 --variant data
    #    variants: none (clock / events / input only), sve (the .sve globals), model (the non-pointer objects of
    #    re/model/addresses.tsv), data (all of .data minus noise bytes), full (all of .data, raw); --stack also writes
    #    mainLoop's frame (minus noise words); --load <run label>:<checkpoint> calls loadGame 0x0040b9b0 on the save that
    #    `capture --save-at` wrote at that frame head; --corrupt <symbol>[:delta] adds delta (default 1) to the first
    #    dword (or byte) of a model object in the written image (negative control)
    # 3. compare a restart run with the continuous run, noise taken from the second continuous run
    ~/bin/py -3.12 re/frida/restart_ab.py compare --scenario sandbox_basic --ref c_a --noise c_b --run r_data

Outputs: log/p2/p21/<scenario>/<label>/ (the scenario_ab files plus <k>.fh.bin / <k>.fh.json captures, <k>.pre.bin, the
restore metadata in restart.json, compare_<ref>.json). Instance instances\p21 (SIMGOLF_ALLOW_MULTI=1), every hook OFF,
C3FT_AUDIO=none, the pinned schedules of log/c3/alt/refs/<scenario>/schedule.json copied (read only) into
log/p2/p21/<scenario>/schedule.json. Without cfg.image scenario_det.js and scenario_ab.py behave as before.
Write-up: re/analysis/data/premise_check.md.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import pathlib
import queue
import re
import shutil
import struct
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "log" / "p2" / "p21"
PINNED = ROOT / "log" / "c3" / "alt" / "refs"
ADDRESSES = ROOT / "re" / "model" / "addresses.tsv"
SECTIONS = ROOT / "re" / "model" / "include" / "sections"
DATA_VA, DATA_N = 0x004C1000, 0x381174          # golf_clean.exe .data (globals and .bss), scenario_det.js IMG_DATA
FRAME_HEAD_RA = 0x0040FC2F                      # return address of mainLoop's frame-head timeGetTime call (0x0040fc29)
STACK = (-0x40, 0x1400)                         # captured stack window, relative to the game's esp at the site
MAIN_FRAME = (0, 0x1254)                        # gameEsp = F - 4 .. mainLoop's entry esp F + 0x1250
DEFAULT_INSTANCE = "p21"
FLAG_GAPS = ROOT / "re" / "analysis" / "data" / "premise_check_flag_gaps.tsv"   # written by `gaps`


# ---------------------------------------------------------------- the model (pure helpers, tested statically)
def pointer_structs(sections: pathlib.Path = SECTIONS) -> set[str]:
    """Struct types of the model headers that hold a pointer: a member commented `+0x.. ptr`, a `vptr` member, or a member
    whose type is such a struct (propagated)."""
    defs = {}
    for h in sorted(sections.glob("*.h")):
        txt = h.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"typedef struct (\w+) \{(.*?)\} \1;", txt, re.S):
            defs[m.group(1)] = m.group(2)
    ptrs = {n for n, body in defs.items()
            if re.search(r"/\*\s*\+0x[0-9a-fA-F]+\s+ptr\b", body) or re.search(r"\bvptr\b", body)}
    changed = True
    while changed:
        changed = False
        for n, body in defs.items():
            if n in ptrs:
                continue
            members = re.findall(r"^\s*(?:struct\s+)?(\w+)\s+\w+(?:\s*\[[^\]]*\])*\s*;", body, re.M)
            if any(t in ptrs for t in members):
                ptrs.add(n)
                changed = True
    return ptrs


def model_objects(path: pathlib.Path = ADDRESSES) -> list[dict]:
    """Top-level golf_clean.exe objects of the merged address map, sorted by address."""
    out = []
    with path.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["module"] != "golf_clean.exe" or r["relation"] != "top":
                continue
            out.append({"symbol": r["symbol"], "va": int(r["va"], 16), "size": int(r["size"], 16),
                        "element_type": r["element_type"], "c_type": r["c_type"], "stream": r["stream"],
                        "sve_field": r["sve_field"]})
    return sorted(out, key=lambda o: o["va"])


def is_pointer_object(o: dict, ptrs: set[str]) -> bool:
    et, ct = re.sub(r"\(.*?\)", "", o["element_type"]), o["c_type"]     # "(TF_*)" is a comment, not a pointer
    if "*" in et or "*" in ct or et.strip() in ("ptr", "vtable", "vbtable"):
        return True
    base = re.split(r"[\[\( ]", et.strip(), 1)[0]
    return base in ptrs or re.split(r"[\[\( ]", ct.strip(), 1)[0] in ptrs


class SymbolMap:
    """Byte offset in .data -> model object index (-1: no model object)."""

    def __init__(self, objs=None, ptrs=None, use_gaps=True):
        import numpy as np
        self.objs = [o for o in (objs if objs is not None else model_objects())
                     if DATA_VA <= o["va"] < DATA_VA + DATA_N]
        self.ptrs = pointer_structs() if ptrs is None else ptrs
        self.idx = np.full(DATA_N, -1, np.int32)
        for i, o in enumerate(self.objs):
            lo = o["va"] - DATA_VA
            hi = min(lo + o["size"], DATA_N)
            self.idx[lo:hi] = i
        self.pointer = [is_pointer_object(o, self.ptrs) for o in self.objs]
        # bytes where a value-in-range pointer word may be excused: pointer-flagged objects and bytes no model object
        # covers. Plain model objects are never excused: their bytes look like addresses often enough (string pools,
        # grids, RNG words fall in a mapped range) that the test would hide real differences there.
        self.loose = self.idx < 0
        for o, p in zip(self.objs, self.pointer):
            if p:
                lo = o["va"] - DATA_VA
                self.loose[lo:lo + o["size"]] = True
        # measured model flag gaps (`gaps` command): dwords of objects the model types as plain data that hold a
        # process-specific value (a heap pointer) in the continuous runs; treated like pointer-flagged bytes
        self.gaps = np.zeros(DATA_N, bool)
        if use_gaps and FLAG_GAPS.exists():
            for line in FLAG_GAPS.read_text(encoding="utf-8").splitlines()[1:]:
                f = line.split("\t")
                va = int(f[1], 16) - DATA_VA
                self.gaps[va:va + 4] = True
        self.loose |= self.gaps

    def symbol_of(self, va: int) -> str:
        i = int(self.idx[va - DATA_VA]) if DATA_VA <= va < DATA_VA + DATA_N else -1
        return self.objs[i]["symbol"] if i >= 0 else f"unmodeled_{va & ~0xff:08x}"

    def by_symbol(self, mask) -> dict:
        """{symbol: differing byte count, first va} for the True bytes of a .data-sized mask; unmodeled bytes are grouped
        per 256-byte block."""
        import numpy as np
        out = {}
        for off in np.nonzero(mask)[0]:
            i = int(self.idx[off])
            key = self.objs[i]["symbol"] if i >= 0 else f"unmodeled_{(DATA_VA + int(off)) & ~0xff:08x}"
            e = out.setdefault(key, {"bytes": 0, "first": f"0x{DATA_VA + int(off):08x}",
                                     "pointer": bool(self.pointer[i]) if i >= 0 else None,
                                     "stream": self.objs[i]["stream"] if i >= 0 else None,
                                     "sve": (self.objs[i]["sve_field"] not in ("", "-")) if i >= 0 else None})
            e["bytes"] += 1
        return out


def dwords(mask):
    """A byte mask widened to whole aligned dwords: a heap pointer of another process often shares its low bytes (the
    measured g_rootWindow words 0x037029d0 / 0x037129d0 / 0x037d29d0), so a byte-level difference marks only part of it,
    and writing the rest would splice two processes' pointers."""
    import numpy as np
    m = np.asarray(mask, bool)
    n = len(m) - len(m) % 4
    out = m.copy()
    out[:n] = np.repeat(m[:n].reshape(-1, 4).any(axis=1), 4)
    return out


def runs_of(mask) -> list[tuple[int, int]]:
    """[(offset, length)] of the maximal runs of True in a 1-D bool array."""
    import numpy as np
    m = np.concatenate(([False], np.asarray(mask, bool), [False]))
    d = np.diff(m.astype(np.int8))
    starts, ends = np.nonzero(d == 1)[0], np.nonzero(d == -1)[0]
    return [(int(s), int(e - s)) for s, e in zip(starts, ends)]


def sve_mask(data: bytes):
    """The .sve globals (re/tools/sve.py FIELDS and the tail blocks the flags word in this image selects) as a mask."""
    import numpy as np
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import sve
    m = np.zeros(DATA_N, bool)
    flags = struct.unpack_from("<I", data, sve.FLAGS_VA - DATA_VA)[0]
    for va, n in sve.layout(flags):
        if DATA_VA <= va < DATA_VA + DATA_N:
            m[va - DATA_VA:va - DATA_VA + n] = True
    return m


def build_image(variant: str, src: bytes, noise, smap: SymbolMap):
    """The write mask of a variant over the source .data image (True = write the source byte)."""
    import numpy as np
    keep = ~np.asarray(noise, bool)
    if variant == "none":
        return np.zeros(DATA_N, bool)
    if variant == "full":
        return np.ones(DATA_N, bool)
    if variant == "data":
        return keep
    if variant == "sve":
        return sve_mask(src) & keep
    if variant == "model":
        m = np.zeros(DATA_N, bool)
        for o, p in zip(smap.objs, smap.pointer):
            if not p:
                lo = o["va"] - DATA_VA
                m[lo:lo + o["size"]] = True
        return m & keep & ~smap.gaps
    raise ValueError(variant)


def corrupt(blob: bytearray, smap: SymbolMap, spec: str) -> dict:
    """--corrupt <symbol>[:delta]: add delta to the object's first dword (first byte when it is smaller than 4 bytes)."""
    name, _, d = spec.partition(":")
    delta = int(d, 0) if d else 1
    o = next((x for x in smap.objs if x["symbol"] == name), None)
    if o is None:
        raise SystemExit(f"--corrupt: no model object {name} in .data")
    off = o["va"] - DATA_VA
    if o["size"] >= 4:
        old = struct.unpack_from("<I", blob, off)[0]
        struct.pack_into("<I", blob, off, (old + delta) & 0xFFFFFFFF)
        new = (old + delta) & 0xFFFFFFFF
    else:
        old = blob[off]
        blob[off] = (old + delta) & 0xFF
        new = blob[off]
    return {"symbol": name, "va": f"0x{o['va']:08x}", "old": old, "new": new}


# ---------------------------------------------------------------- runs (need the game)
def setup(scen: str, instance: pathlib.Path):
    """scenario_refs.setup with every output under log/p2/p21 (the boot lock stays the shared one)."""
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import scenario_refs as SR
    SR.REFS = OUT
    os.environ.setdefault("C3FT_AUDIO", "none")
    os.environ["C3FT_FULLDUMP"] = "1"
    # scenario_det.js reports a main-thread exception (pc, backtrace, registers) only with --trace / --watch: a one-dword
    # trace of the RNG word turns that on (read at every poll, no effect on the game)
    os.environ.setdefault("C3FT_TRACE", "822d9c:4")
    S = SR.setup(scen, instance)
    register_scenarios(S)
    return S, SR


def register_scenarios(S):
    """Scenarios of this check with denser checkpoints than the canonical ones (schedules recorded under log/p2/p21)."""
    A = sys.modules.get("s36a_run")
    import scenario as SCN

    def sc_p21_mood5(c):
        """mood5_fixture's start (Continue Saved Game -> mood5.sve, Esc x4, camera tile (28, 15)), then 240 virtual s of
        season (Esc every 2 s) with a checkpoint every 20 s and no story stop."""
        yield from A.load_saved_game(c, "mood5.sve")
        for _ in range(4):
            yield from c.key(0x1B); yield from c.sleep(500)
        c.write32(SCN.CAMERA_TILE[0], 28, tag="camera x")
        c.write32(SCN.CAMERA_TILE[1], 15, tag="camera y")
        yield from c.sleep(1000)
        yield from c.checkpoint("view")
        yield from A.season(c, 240, "season", every_s=20)

    S.SCENARIOS["p21_mood5"] = sc_p21_mood5


def sched_path(scen: str) -> pathlib.Path:
    p = OUT / scen / "schedule.json"
    if not p.exists() and (PINNED / scen / "schedule.json").exists():
        p.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PINNED / scen / "schedule.json", p)
    return p


def make_run_class(S):
    T0, POLL_MS = S.T0, S.POLL_MS

    class P21Run(S.Run):
        def __init__(self, *a, captures=(), save_at=None, restart=None, **kw):
            super().__init__(*a, **kw)
            self.captures = list(captures)
            self.save_at = save_at                # (checkpoint, save name)
            self.restart = restart                # dict, see cmd_restart
            self.img_q: queue.Queue = queue.Queue()
            self.img_log: list = []

        def agent_source(self):
            p = super().agent_source()
            first, rest = p.read_text().split("\n", 1)
            head = "const SCENARIO_CFG = "
            assert first.startswith(head) and first.endswith(";")
            cfg = json.loads(first[len(head):-1])
            cfg["image"] = {"ra": FRAME_HEAD_RA}
            p.write_text(head + json.dumps(cfg) + ";\n" + rest)
            return p

        def _on_message(self, m, data):
            if m.get("type") == "send" and isinstance(m["payload"], dict) and m["payload"].get("type") == "img":
                p = m["payload"]
                op = p.get("op")
                if op == "capture":
                    name = self.pending_capture.pop(0) if getattr(self, "pending_capture", None) else f"cap{len(self.img_log)}"
                    (self.out / f"{name}.fh.bin").write_bytes(data)
                    (self.out / f"{name}.fh.json").write_text(json.dumps(p["meta"], indent=1))
                    self.img_log.append({"op": op, "name": name, "T": p["meta"]["T"], "seen": p["meta"]["seen"],
                                         "esp": p["meta"]["esp"]})
                elif op == "pre":
                    (self.out / f"{self.restart['image']}.pre.bin").write_bytes(data)
                    (self.out / f"{self.restart['image']}.pre.json").write_text(json.dumps(p["meta"], indent=1))
                    self.img_q.put((p, data))
                else:
                    if op != "dlldata":
                        self.img_log.append({k: v for k, v in p.items() if k != "type"})
                    self.img_q.put((p, data))
                return
            super()._on_message(m, data)

        def img_wait(self, ops, timeout=300):
            t0 = time.time()
            while True:
                try:
                    p, data = self.img_q.get(timeout=5)
                except queue.Empty:
                    held = []
                    while not self.q.empty():          # an agent error while waiting (onDue catches and sends it)
                        e = self.q.get()
                        if e[0].get("type") == "error":
                            self.errors.append(e[0])
                            raise RuntimeError(f"{self.label}: agent error waiting for {ops}: {e[0].get('where')}: "
                                               f"{str(e[0].get('msg'))[:600]}")
                        held.append(e)
                    for e in held:
                        self.q.put(e)
                    if not self.g.alive:
                        raise RuntimeError(f"{self.label}: game died waiting for {ops}: {self.g.detach_reason}")
                    if time.time() - t0 > timeout:
                        raise TimeoutError(f"{self.label}: no {ops} image message in {timeout} s")
                    continue
                if p.get("op") == "timeout":
                    raise TimeoutError(f"{self.label}: frame-head site not met: {p}")
                if p.get("op") in ops:
                    return p, data

        def take_dump(self, sc, name):
            super().take_dump(sc, name)
            sc.post({"type": "go", "img": {"op": "dlldata"}, "hwnd": self.hwnd_val()})
            p, data = self.img_wait(("dlldata",))
            (self.out / f"{name}.dll.bin").write_bytes(data)
            (self.out / f"{name}.dll.json").write_text(json.dumps(p["meta"]))
            (self.out / f"{name}.ranges.json").write_text(json.dumps(p.get("ranges") or []))
            if name in self.captures and not (self.restart and self.restarted):
                req = {"op": "capture", "stack": list(STACK)}
                if self.save_at and self.save_at[0] == name:
                    req["saveAs"] = self.save_at[1]
                self.pending_capture = getattr(self, "pending_capture", []) + [name]
                sc.post({"type": "go", "img": req, "hwnd": self.hwnd_val()})

        def go(self, script_fn, extra_env=None):
            self.restarted = False
            return super().go(script_fn, extra_env)

        def _go(self, script_fn, env, js, t_start):
            Game = S.Game
            with Game(env=env, extra_js=[str(js)], on_message=self._on_message) as g:
                self.g = g
                sc = g.scripts[-1]
                p, _ = self.wait_msg(("ready",), timeout=60)
                self.slots = p["slots"]
                ctx = S.Ctx(self)
                gen = (self.restart["prefix_fn"] if self.restart else script_fn)(ctx)
                first = True
                while True:
                    p, _ = self.wait_msg(("poll",))
                    self.polls += 1
                    ctx.T = p["st"]["T"] - T0
                    self.agent_log += p.get("log", [])
                    self.trace.append([ctx.T, p["st"]["qpc"], p["st"]["forwarded"], round(time.time() - t_start, 3)])
                    ctx.actions = []
                    if first and os.environ.get("C3FT_NO_VKEYS") != "1":
                        for vk in range(1, 255):
                            ctx.act(0, k="vkey", vk=vk, st=1)
                        first = False
                    ctx.dump_name = None
                    try:
                        next(gen)
                        finished = False
                    except StopIteration:
                        finished = True
                    if ctx.dump_name:
                        self.take_dump(sc, ctx.dump_name)
                    if self.restart and not self.restarted and ctx.dump_name == self.restart["from"]:
                        ctx, gen = self.do_restart(sc, script_fn)
                        continue
                    if finished:
                        self.post(sc, actions=ctx.actions, hwnd=self.hwnd_val())
                        break
                    self.post(sc, actions=ctx.actions, next=POLL_MS, hwnd=self.hwnd_val())
                    if not g.alive:
                        raise RuntimeError(f"{self.label}: game died: {g.detach_reason}")
                self.final_status = p["st"]

        def do_restart(self, sc, script_fn):
            r = self.restart
            if r.get("copy_save"):
                dst = S.INSTANCE / "saved games" / pathlib.Path(r["copy_save"]).name
                shutil.copy2(r["copy_save"], dst)
            req = {"op": "restore", "report": True, "twoPhase": True, "stack": list(STACK),
                   "clock": r["clock"], "events": r["events"], "timers": r["timers"], "input": r["input"]}
            if r.get("load"):
                req["load"] = r["load"]
            sc.post({"type": "go", "img": req, "hwnd": self.hwnd_val()})
            sc.post({"type": "go", "actions": [], "hwnd": self.hwnd_val()})      # resume without scheduling a poll
            pm, pre = self.img_wait(("pre",))
            regions, stack_regions, blob, stats = r["build"](pm["meta"], pre)
            self.plan_stats = stats
            sc.post({"type": "img2", "regions": regions, "stackRegions": stack_regions}, data=blob)
            p, _ = self.img_wait(("restored",))
            self.restore_meta = p["meta"]
            self.restarted = True
            print(f"  [{self.label}] restored at T={p['meta']['T'] - T0} (site after {p['meta']['seen']} clock calls, "
                  f"{p['meta']['regions']} regions / {p['meta']['bytes']} bytes, esp 0x{p['meta']['esp']:08x})", flush=True)
            # the source's generator, advanced over the source's polls before the capture instant (their actions are in
            # the image or among the handed-over events)
            self.schedule = r["schedule"]
            ctx = S.Ctx(self)
            gen = script_fn(ctx)
            for t in r["src_polls"]:
                if t + T0 >= r["capture_T"]:
                    break
                ctx.T = t
                ctx.actions = []
                ctx.dump_name = None
                next(gen)
            return ctx, gen

    return P21Run


def finish(run, out: pathlib.Path, extra: dict):
    s = json.loads((out / "summary.json").read_text())
    s["p21"] = {"img_log": run.img_log, **extra}
    if getattr(run, "restore_meta", None):
        s["p21"]["restore"] = run.restore_meta
    if getattr(run, "plan_stats", None):
        s["p21"]["plan"] = run.plan_stats
    (out / "summary.json").write_text(json.dumps(s, indent=1))


def run_common(a, mode, extra_kw, label_extra):
    instance = (ROOT / "instances" / a.instance).resolve()
    S, SR = setup(a.scenario, instance)
    P21Run = make_run_class(S)
    sp = sched_path(a.scenario)
    sched = json.loads(sp.read_text()) if mode == "replay" else None
    out = OUT / a.scenario / a.label
    if out.exists():
        shutil.rmtree(out)
    ilock = SR.instance_lock(instance)
    try:
        dll, backup = instance / "winmm.dll", instance / "winmm.dll.c3ft_main"
        if backup.exists() and SR.sha256(dll) != SR.sha256(backup):
            raise SystemExit(f"{dll} differs from {backup.name}: a killed control run left its DLL (restore it first)")
        S.write_ini_hooks([])
        cond = SR.conditions(instance)
        run = P21Run("p21", a.scenario, mode, S.arm_env_off(), [], [], schedule=sched, out_dir=out, **extra_kw)
        rc = 0
        try:
            run.go(S.SCENARIOS[a.scenario])
        except Exception as e:  # noqa: BLE001
            print(f"RUN FAILED: {type(e).__name__}: {e}", flush=True)
            rc = 2
        finally:
            log = instance / "simgolf_shim.log"
            if log.exists():
                shutil.copy2(log, out / "shim.log")
            (out / "conditions.json").write_text(json.dumps(cond, indent=1))
            if (out / "summary.json").exists():
                finish(run, out, label_extra)
        if mode == "record" and rc == 0:
            sp.write_text(json.dumps(run.schedule, indent=1))
            print(f"schedule -> {sp}")
        print(f"{a.scenario}/{a.label}: {'OK' if rc == 0 else 'FAILED'} polls={run.polls} checkpoints={len(run.checkpoints)}")
        return rc, run, out
    finally:
        shutil.rmtree(ilock, ignore_errors=True)


def cmd_capture(a):
    save_at = (a.save_at, a.save_name or f"p21_{a.save_at}") if a.save_at else None
    caps = list(a.at or []) + ([a.save_at] if a.save_at and a.save_at not in (a.at or []) else [])
    rc, run, out = run_common(a, "record" if a.record else "replay", {"captures": caps, "save_at": save_at},
                              {"captures": caps, "save_at": save_at})
    if save_at and rc == 0:
        sve_file = (ROOT / "instances" / a.instance / "saved games" / f"{save_at[1]}.sve")
        if sve_file.exists():
            shutil.copy2(sve_file, out / sve_file.name)
            print(f"save -> {out / sve_file.name}")
        else:
            print(f"WARNING: {sve_file} was not written")
    return rc


def load_capture(run_dir: pathlib.Path, name: str):
    meta = json.loads((run_dir / f"{name}.fh.json").read_text())
    blob = (run_dir / f"{name}.fh.bin").read_bytes()
    return meta, blob[:DATA_N], blob[DATA_N:]


def noise_masks(a_dir, b_dirs, name):
    """Bytes of the source's frame-head image that differ in any of the other continuous runs (.data and stack)."""
    import numpy as np
    if isinstance(b_dirs, pathlib.Path):
        b_dirs = [b_dirs]
    ma, da, sa = load_capture(a_dir, name)
    nd = np.zeros(len(da), bool)
    ns = np.zeros(len(sa), bool)
    for b_dir in b_dirs:
        mb, db, sb = load_capture(b_dir, name)
        nd |= np.frombuffer(da, np.uint8) != np.frombuffer(db, np.uint8)
        ns |= np.frombuffer(sa, np.uint8) != np.frombuffer(sb, np.uint8)
        if ma["esp"] != mb["esp"]:
            print(f"note: site esp differs between continuous runs (0x{ma['esp']:08x} / 0x{mb['esp']:08x})")
    return dwords(nd), dwords(ns), (ma, da, sa)


def pointer_words(img: bytes, ranges):
    """Bytes of the aligned dwords of an image whose value lies in one of `ranges` ([base, size] of the process's mapped
    ranges outside golf_clean.exe, scenario_det.js imgRanges): pointers into the heap, a stack or a DLL, valid only in that
    process. golf_clean.exe has no relocations, so a value inside its own image means the same in every process."""
    import numpy as np
    v = np.frombuffer(img[:len(img) - len(img) % 4], "<u4").astype(np.int64)
    rs = sorted((int(b), int(n)) for b, n in ranges)
    if not rs:
        return np.zeros(len(img), bool)
    starts = np.array([b for b, _ in rs], np.int64)
    ends = starts + np.array([n for _, n in rs], np.int64)
    i = np.searchsorted(starts, v, "right") - 1
    inr = (i >= 0) & (v < ends[np.clip(i, 0, None)]) & (v >= 0x10000)
    out = np.zeros(len(img), bool)
    out[:len(inr) * 4] = np.repeat(inr, 4)
    return out


VARIANTS = ["none", "sve", "model", "data", "data+", "full"]


def object_mask(smap: SymbolMap, names) -> "np.ndarray":
    """The bytes of the named model objects (exact symbols of re/model/addresses.tsv)."""
    import numpy as np
    m = np.zeros(DATA_N, bool)
    for n in names or []:
        o = next((x for x in smap.objs if x["symbol"] == n), None)
        if o is None:
            raise SystemExit(f"no model object {n} in .data")
        m[o["va"] - DATA_VA:o["va"] - DATA_VA + o["size"]] = True
    return m


def restore_plan(variant, stack_on, src_meta, src_data, src_stack, nd, ns, pre_meta, pre, smap, corrupt_spec=None,
                 add=None, drop=None):
    """The write set at the restart instant. `pre` = the fresh process's own .data + stack there, pre_meta its ranges.
    model / data: as decided by the model flags / noise only; data+: all of .data minus noise and minus, inside
    pointer-flagged objects and unmodeled bytes, every dword that is a pointer in either process (the source's value in
    the source's ranges, or this run's value in its own ranges)."""
    import numpy as np
    base = "data" if variant == "data+" else variant
    mask = build_image(base, src_data, nd, smap)
    pre_data, pre_stack = pre[:DATA_N], pre[DATA_N:]
    pw = pointer_words(src_data, src_meta.get("ranges", [])) | pointer_words(pre_data, pre_meta.get("ranges", []))
    pw &= smap.loose
    if variant == "data+":
        mask &= ~pw
    if add:                                       # --add: these objects too (minus noise)
        mask |= object_mask(smap, add) & ~nd
    if drop:                                      # --drop: never these objects (the fresh process keeps its own)
        mask &= ~object_mask(smap, drop)
    blob = bytearray(src_data)
    corr = corrupt(blob, smap, corrupt_spec) if corrupt_spec else None
    if corr and not mask[int(corr["va"], 16) - DATA_VA]:
        raise RuntimeError(f"--corrupt {corrupt_spec}: the variant {variant} does not write that byte")
    regions, parts = [], []
    for off, n in runs_of(mask):
        regions.append([DATA_VA + off, n])
        parts.append(bytes(blob[off:off + n]))
    stack_regions = []
    spw = pointer_words(src_stack, src_meta.get("ranges", [])) | pointer_words(pre_stack, pre_meta.get("ranges", []))
    if stack_on:
        smask = np.zeros(len(src_stack), bool)
        lo = MAIN_FRAME[0] - STACK[0]
        smask[lo:lo + MAIN_FRAME[1] - MAIN_FRAME[0]] = True
        smask &= ~ns & ~spw
        for off, n in runs_of(smask):
            stack_regions.append([off + STACK[0], n])
            parts.append(src_stack[off:off + n])
    stats = {"image_regions": len(regions), "image_bytes": int(mask.sum()), "noise_bytes": int(nd.sum()),
             "pointer_word_bytes": int(pw.sum()), "stack_regions": len(stack_regions),
             "stack_bytes": int(sum(n for _, n in stack_regions)),
             "stack_noise_words": int(ns.reshape(-1, 4).any(axis=1).sum()), "corrupt": corr}
    return regions, stack_regions, b"".join(parts), stats, pw


def cmd_restart(a):
    import numpy as np
    src_dir, noise_dirs = OUT / a.scenario / a.source, [OUT / a.scenario / n for n in a.noise]
    nd, ns, (meta, data, stack) = noise_masks(src_dir, noise_dirs, a.image)
    smap = SymbolMap()

    def build(pre_meta, pre):
        regions, stack_regions, blob, stats, pw = restore_plan(a.variant, a.stack, meta, data, stack, nd, ns, pre_meta,
                                                               pre, smap, a.corrupt, a.add, a.drop)
        # pointer words outside the model's pointer-flagged objects (model flag gaps), for the write-up
        flagged = np.zeros(DATA_N, bool)
        for o, p in zip(smap.objs, smap.pointer):
            if p:
                flagged[o["va"] - DATA_VA:o["va"] - DATA_VA + o["size"]] = True
        stats["pointer_words_outside_flagged"] = smap.by_symbol(pw & ~flagged)
        print(f"  restore plan: variant {a.variant}{' +stack' if a.stack else ''}: {stats['image_regions']} regions, "
              f"{stats['image_bytes']} bytes, {stats['noise_bytes']} noise bytes, {stats['pointer_word_bytes']} "
              f"pointer-word bytes, stack {stats['stack_bytes']} bytes"
              + (f", corrupt {stats['corrupt']}" if stats["corrupt"] else ""), flush=True)
        return regions, stack_regions, blob, stats

    src_sum = json.loads((src_dir / "summary.json").read_text())
    src_polls = [row[0] for row in json.loads((src_dir / "poll_log.json").read_text())["trace"]]
    restart = {"from": a.from_cp, "image": a.image, "build": build,
               "clock": {"ret": meta["ret"], "T": meta["T"], "Qlo": meta["Qlo"], "Qhi": meta["Qhi"]},
               "events": meta["events"], "timers": meta["timers"], "input": meta["input"],
               "schedule": src_sum["schedule"], "src_polls": src_polls, "capture_T": meta["T"]}
    if a.load:
        lab, _, cp = a.load.partition(":")
        sm = json.loads((OUT / a.scenario / lab / "summary.json").read_text())["p21"]["save_at"]
        f = OUT / a.scenario / lab / f"{sm[1]}.sve"
        if not f.exists():
            raise SystemExit(f"--load: {f} missing")
        restart["copy_save"] = str(f)
        # loadGame takes the file name with its extension (it compares it with "While_Browsing.sve", 0x004c5314)
        restart["load"] = {"name": sm[1] + ".sve", "a2": 1, "a3": 0, "a4": 0}
    prefix = a.prefix_scenario or a.scenario
    extra = {"variant": a.variant, "stack": a.stack, "source": a.source, "noise": a.noise, "from": a.from_cp,
             "image": a.image, "prefix_scenario": prefix, "load": a.load, "add": a.add, "drop": a.drop}
    # the prefix: the prefix scenario's script and schedule up to --from
    S, SR = setup(a.scenario, (ROOT / "instances" / a.instance).resolve())
    if prefix != a.scenario:
        restart["prefix_fn"] = S.SCENARIOS[prefix]
        pre_sched = json.loads(sched_path(prefix).read_text())
    else:
        restart["prefix_fn"] = S.SCENARIOS[a.scenario]
        pre_sched = json.loads(sched_path(a.scenario).read_text())
    a2 = argparse.Namespace(**vars(a))
    rc, run, out = run_common_restart(a2, S, SR, restart, pre_sched, extra)
    if rc == 0 and not a.no_compare:
        cmp = compare(src_dir, noise_dirs, out, after=a.image)
        (out / f"compare_{a.source}.json").write_text(json.dumps(cmp, indent=1))
        print_compare(cmp)
    return rc


def run_common_restart(a, S, SR, restart, pre_sched, extra):
    instance = (ROOT / "instances" / a.instance).resolve()
    P21Run = make_run_class(S)
    out = OUT / a.scenario / a.label
    if out.exists():
        shutil.rmtree(out)
    ilock = SR.instance_lock(instance)
    try:
        S.write_ini_hooks([])
        cond = SR.conditions(instance)
        run = P21Run("p21r", a.scenario, "replay", S.arm_env_off(), [], [], schedule=pre_sched, out_dir=out,
                     restart=restart)
        rc = 0
        try:
            run.go(S.SCENARIOS[a.scenario])
        except Exception as e:  # noqa: BLE001
            print(f"RUN FAILED: {type(e).__name__}: {e}", flush=True)
            rc = 2
        finally:
            log = instance / "simgolf_shim.log"
            if log.exists():
                shutil.copy2(log, out / "shim.log")
            (out / "conditions.json").write_text(json.dumps(cond, indent=1))
            if (out / "summary.json").exists():
                finish(run, out, extra)
        print(f"{a.scenario}/{a.label}: {'OK' if rc == 0 else 'FAILED'} polls={run.polls} checkpoints={len(run.checkpoints)}")
        return rc, run, out
    finally:
        shutil.rmtree(ilock, ignore_errors=True)


# ---------------------------------------------------------------- comparison (no game)
def _frame_px(a_png, b_png):
    import numpy as np
    from PIL import Image
    ia = np.asarray(Image.open(a_png).convert("RGB"), np.int16)
    ib = np.asarray(Image.open(b_png).convert("RGB"), np.int16)
    return int((np.abs(ia - ib).sum(axis=2) > 0).sum())


def dll_pages(d: pathlib.Path, name: str) -> dict | None:
    """{(module, page rva): 4096 bytes} of a <name>.dll.bin dump (the writable ranges' split changes over a run, so dumps
    are compared page by page)."""
    f = d / f"{name}.dll.bin"
    if not f.exists():
        return None
    meta, blob = json.loads((d / f"{name}.dll.json").read_text()), f.read_bytes()
    out, off = {}, 0
    for mod, rva, n in meta:
        for p in range(0, n, 0x1000):
            out[(mod, rva + p)] = blob[off + p:off + p + 0x1000]
        off += n
    return out


def x_bytes(pages, key):
    return pages[key]


def _dll_diff(ref, noises, run, name):
    import numpy as np
    A, R = dll_pages(ref, name), dll_pages(run, name)
    Bs = [dll_pages(n, name) for n in noises]
    if A is None or R is None or any(b is None for b in Bs):
        return None

    def rng(d):
        rf = d / f"{name}.ranges.json"
        return json.loads(rf.read_text()) if rf.exists() else []
    ra, rr, rbs = rng(ref), rng(run), [rng(n) for n in noises]
    keys = set(A) & set(R)
    for b in Bs:
        keys &= set(b)
    out = {}
    for key in sorted(keys):
        a, r = np.frombuffer(A[key], np.uint8), np.frombuffer(R[key], np.uint8)
        nm = pointer_words(A[key], ra) | pointer_words(R[key], rr)
        for b, rb in zip(Bs, rbs):
            nm |= (a != np.frombuffer(b[key], np.uint8)) | pointer_words(b[key], rb)
        nm = dwords(nm)
        diff = (a != r) & ~nm
        k = int(diff.sum())
        if k:
            mod, rva = key
            first = int(np.nonzero(diff)[0][0])
            out.setdefault(mod, []).append({"page": f"0x{rva:x}", "bytes": k, "first_rva": f"0x{rva + first:x}",
                                            "noise": int(nm.sum())})
    return out


def compare(ref: pathlib.Path, noises, run: pathlib.Path, after: str | None = None) -> dict:
    """Per checkpoint of the restart run: the model objects whose bytes differ from the continuous run, with the bytes that
    differ between the two continuous runs (noise) left out; the frame; the DLL writable ranges; T."""
    import numpy as np
    smap = SymbolMap()
    sr = json.loads((ref / "summary.json").read_text())
    su = json.loads((run / "summary.json").read_text())
    names = list(sr["checkpoints"])
    k = names.index(after) if after in names else -1
    if isinstance(noises, pathlib.Path):
        noises = [noises]
    out = {"ref": ref.name, "noise": [n.name for n in noises], "run": run.name, "after": after, "checkpoints": {}}
    if (run / f"{after}.pre.bin").exists() and (ref / f"{after}.fh.bin").exists():
        # the restart instant: this run's own state (before the write) against the source's image there
        nd, ns, (_m, da, sa) = noise_masks(ref, noises, after)
        pre = (run / f"{after}.pre.bin").read_bytes()
        pm = json.loads((run / f"{after}.pre.json").read_text())
        nd |= (pointer_words(da, _m.get("ranges", [])) | pointer_words(pre[:DATA_N], pm.get("ranges", []))) & smap.loose
        diff = (np.frombuffer(da, np.uint8) != np.frombuffer(pre[:DATA_N], np.uint8)) & ~nd
        ns |= pointer_words(sa, _m.get("ranges", [])) | pointer_words(pre[DATA_N:], pm.get("ranges", []))
        sd = (np.frombuffer(sa, np.uint8) != np.frombuffer(pre[DATA_N:], np.uint8)) & ~ns
        lo = MAIN_FRAME[0] - STACK[0]
        fr = sd[lo:lo + MAIN_FRAME[1]]
        words = sorted({(i // 4) * 4 for i in np.nonzero(fr)[0]})
        out["restart_instant"] = {"data_bytes_differ": int(diff.sum()), "objects_differ": smap.by_symbol(diff),
                                  "mainloop_frame_words_differ": [f"F{w - 4:+#x}" for w in words],
                                  "noise_data_bytes": int(nd.sum()), "noise_frame_words": int(ns.reshape(-1, 4).any(axis=1).sum())}
    for i, name in enumerate(names):
        if name not in su["checkpoints"]:
            out["checkpoints"][name] = {"missing": True}
            continue
        r = {"phase": "before" if i < k else ("restart" if i == k else "after"),
             "T_ref": sr["checkpoints"][name]["T"], "T_run": su["checkpoints"][name]["T"]}
        fa, fu = ref / f"{name}.full.bin", run / f"{name}.full.bin"
        fbs = [n / f"{name}.full.bin" for n in noises]
        if fa.exists() and fu.exists() and all(f.exists() for f in fbs):
            A = np.frombuffer(fa.read_bytes(), np.uint8)
            U = np.frombuffer(fu.read_bytes(), np.uint8)
            Bs = [np.frombuffer(f.read_bytes(), np.uint8) for f in fbs]
            nm = np.zeros(DATA_N, bool)
            for B in Bs:
                nm |= A != B
            nm = dwords(nm)
            # pointer words: a dword whose value is a pointer into a non-exe range of the process it was read in (each
            # run's ranges at this checkpoint, <name>.ranges.json); heap layouts repeat between runs only in part, so
            # byte noise from two runs does not cover them
            pwm = np.zeros(DATA_N, bool)
            for d, X in [(ref, A), (run, U)] + list(zip(noises, Bs)):
                rf = d / f"{name}.ranges.json"
                if rf.exists():
                    pwm |= pointer_words(X.tobytes(), json.loads(rf.read_text()))
            pwm &= smap.loose
            raw = (A != U) & ~nm
            diff = raw & ~pwm
            r["noise_bytes"] = int(nm.sum())
            r["pointer_word_diff_bytes"] = int((raw & pwm).sum())
            r["data_bytes_differ"] = int(diff.sum())
            r["objects_differ"] = smap.by_symbol(diff)
            # simulation-state objects: streams B (simulation) and C (course / terrain / world) of the model, and every
            # .sve field (the other streams are rendering / audio, UI, engine classes and constants)
            r["sim_objects_differ"] = sorted(k for k, v in r["objects_differ"].items()
                                             if v["stream"] in ("b", "c") or v["sve"])
        r["frame_px_differ"] = _frame_px(ref / f"{name}.png", run / f"{name}.png")
        r["frame_px_noise"] = max(_frame_px(ref / f"{name}.png", n / f"{name}.png") for n in noises)
        r["dll_differ"] = _dll_diff(ref, noises, run, name)
        r["same"] = (r["T_ref"] == r["T_run"] and not r.get("data_bytes_differ") and r["frame_px_differ"] == 0
                     and not r["dll_differ"])
        out["checkpoints"][name] = r
    after_rows = [r for r in out["checkpoints"].values() if r.get("phase") == "after"]
    out["verdict"] = ("SAME" if after_rows and all(r["same"] for r in after_rows)
                      else "DIFF" if after_rows else "NO CHECKPOINT AFTER THE RESTART")
    out["sim_verdict"] = ("SAME" if after_rows and all(r["T_ref"] == r["T_run"] and not r.get("sim_objects_differ")
                                                       for r in after_rows) else "DIFF" if after_rows else "-")
    return out


def print_compare(c: dict):
    if "restart_instant" in c:
        ri = c["restart_instant"]
        print(f"  restart instant: this run's own state differs from the image in {ri['data_bytes_differ']} bytes / "
              f"{len(ri['objects_differ'])} objects; mainLoop frame words differ: {len(ri['mainloop_frame_words_differ'])}")
    for name, r in c["checkpoints"].items():
        if r.get("missing"):
            print(f"  {name}: missing")
            continue
        objs = sorted(r.get("objects_differ", {}).items(), key=lambda kv: -kv[1]["bytes"])
        s = ", ".join(f"{k}({v['bytes']})" for k, v in objs[:8]) + (f" +{len(objs) - 8}" if len(objs) > 8 else "")
        dl = r.get("dll_differ")
        dls = "" if not dl else " dll " + ", ".join(f"{m}:{sum(x['bytes'] for x in v)}" for m, v in dl.items())
        sim = r.get("sim_objects_differ")
        if sim is not None and r["phase"] == "after":
            s += f"  | sim objects differ: {len(sim)}" + (f" ({', '.join(sim[:6])}{' ...' if len(sim) > 6 else ''})" if sim else "")
        print(f"  [{r['phase']:7}] {name}: T {'=' if r['T_ref'] == r['T_run'] else str(r['T_ref'] - S_T0()) + '/' + str(r['T_run'] - S_T0())} "
              f"data {r.get('data_bytes_differ', '?')} B ({len(objs)} objs) frame {r['frame_px_differ']} px{dls}  {s}")
    print(f"  VERDICT (checkpoints after the restart): {c['verdict']}   simulation objects only: {c['sim_verdict']}")


def S_T0():
    return 0x00100000


def cmd_gaps(a):
    """Model flag gaps: every aligned dword of a model object NOT flagged as holding pointers whose value differs between
    two continuous runs of one scenario (frame-head captures and checkpoint dumps of the runs labelled c_* / rec under
    log/p2/p21/<scenario>/), with the values seen. Written to re/analysis/data/premise_check_flag_gaps.tsv."""
    import numpy as np
    smap = SymbolMap(use_gaps=False)
    found = {}
    for sd in sorted(p for p in OUT.iterdir() if p.is_dir()):
        runs = [r for r in sorted(sd.iterdir()) if r.is_dir() and (r.name.startswith("c_") and r.name != "c_save"
                                                                   or r.name == "rec") and (r / "summary.json").exists()]
        if len(runs) < 2:
            continue
        names = sorted({f.name for r in runs for f in r.glob("*.full.bin")} | {f.name for r in runs for f in r.glob("*.fh.bin")})
        for fn in names:
            blobs = [(r.name, (r / fn).read_bytes()[:DATA_N]) for r in runs if (r / fn).exists()]
            if len(blobs) < 2:
                continue
            arr = [np.frombuffer(b, np.uint8) for _n, b in blobs]
            m = np.zeros(DATA_N, bool)
            for x in arr[1:]:
                m |= arr[0] != x
            m = dwords(m)
            for off in np.nonzero(m[::4])[0] * 4:
                i = int(smap.idx[off])
                if i < 0 or smap.pointer[i]:
                    continue
                o = smap.objs[i]
                e = found.setdefault(off, {"symbol": o["symbol"], "offset": DATA_VA + off - o["va"], "type": o["element_type"],
                                           "stream": o["stream"], "scenarios": set(), "values": set()})
                e["scenarios"].add(sd.name)
                for _n, b in blobs:
                    if len(e["values"]) < 4:
                        e["values"].add(f"0x{struct.unpack_from('<I', b, off)[0]:08x}")
    lines = ["symbol\tva\toffset\telement_type\tstream\tscenarios\tvalues_seen"]
    for off in sorted(found):
        e = found[off]
        lines.append(f"{e['symbol']}\t0x{DATA_VA + off:08x}\t0x{e['offset']:x}\t{e['type']}\t{e['stream']}\t"
                     f"{','.join(sorted(e['scenarios']))}\t{','.join(sorted(e['values']))}")
    FLAG_GAPS.write_text("\n".join(lines) + "\n", encoding="utf-8")
    syms = sorted({e["symbol"] for e in found.values()})
    print(f"{len(found)} dwords in {len(syms)} objects -> {FLAG_GAPS}")
    for sname in syms:
        print("  " + sname + ": " + ", ".join(f"+0x{e['offset']:x}" for e in found.values() if e["symbol"] == sname))
    return 0


def cmd_compare(a):
    c = compare(OUT / a.scenario / a.ref, [OUT / a.scenario / n for n in a.noise], OUT / a.scenario / a.run,
                after=a.after)
    p = OUT / a.scenario / a.run / f"compare_{a.ref}.json"
    p.write_text(json.dumps(c, indent=1))
    print_compare(c)
    return 0 if c["verdict"] == "SAME" else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--instance", default=DEFAULT_INSTANCE)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("capture")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--at", nargs="*", help="checkpoints after which the next frame head is captured")
    p.add_argument("--save-at", help="also call saveGame at the frame head after this checkpoint")
    p.add_argument("--save-name")
    p.add_argument("--record", action="store_true", help="record the schedule (scenarios of this tool only)")
    p.set_defaults(fn=cmd_capture)
    p = sp.add_parser("restart")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--source", required=True)
    p.add_argument("--noise", required=True, nargs="+", help="other continuous runs (noise)")
    p.add_argument("--image", required=True, help="the source checkpoint whose frame-head image is written")
    p.add_argument("--from", dest="from_cp", required=True, help="the checkpoint of this run after which it is written")
    p.add_argument("--prefix-scenario", help="replay another scenario up to --from (default: --scenario)")
    p.add_argument("--variant", choices=VARIANTS, default="model")
    p.add_argument("--stack", action="store_true", help="also write mainLoop's frame (minus noise words)")
    p.add_argument("--load", help="<capture label>:<checkpoint>: call loadGame on the save that capture wrote")
    p.add_argument("--corrupt", help="<model symbol>[:delta] (negative control)")
    p.add_argument("--add", nargs="*", help="model objects written in addition to the variant (minus noise)")
    p.add_argument("--drop", nargs="*", help="model objects never written (the fresh process keeps its own)")
    p.add_argument("--no-compare", action="store_true")
    p.set_defaults(fn=cmd_restart)
    p = sp.add_parser("gaps")
    p.set_defaults(fn=cmd_gaps)
    p = sp.add_parser("compare")
    p.add_argument("--scenario", required=True)
    p.add_argument("--ref", required=True)
    p.add_argument("--noise", required=True, nargs="+", help="other continuous runs (noise)")
    p.add_argument("--run", required=True)
    p.add_argument("--after")
    p.set_defaults(fn=cmd_compare)
    a = ap.parse_args()
    return a.fn(a) or 0


if __name__ == "__main__":
    sys.exit(main())
