"""Census of the 980 sound.dll path-1 keys replayed without the game (step S4.1, re/analysis/data/sound_replay.md).

For every key of re/frida/registry.d with module="sound.dll": the fixture is rebuilt by the fixture VM, the ORIGINAL function
runs in Unicorn (port/test/sound_emu.py) in three layouts (A, B, C: other module base, other heap bases, other gaps between
fixture blocks), and each vector is compared with the live path-1 CSV (log/diff/<rva>_sound_<key>.path1.csv) on the fields
that do not depend on run addresses:

  return     the formatted return value, compared when layouts A, B and C print the same text (else it holds a run address)
  state      the FNV-1a of the state regions' hex, compared when A, B and C give the same bytes (else the regions hold a run
             address: a fixture block, a heap block, a list link or a pointer into the relocated module)
  changed    state_changed, always compared
  distinct   distinct_results of the key, compared

Output: re/roadmap/S4_census.tsv (one row per key) and log/roadmap/S4/1_census/<key>.json (per-vector detail).

    ~/bin/py -3.12 re/tools/sound_census.py [--jobs 4] [--keys k1,k2] [--batch c3an] [--out re/roadmap/S4_census.tsv]
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "port" / "test"))
DETAIL = ROOT / "log" / "roadmap" / "S4" / "1_census"
OUT = ROOT / "re" / "roadmap" / "S4_census.tsv"

COLS = ["key", "batch", "rva", "fixture", "styles", "vectors", "csv_vectors", "csv_verdict", "fixture_status", "missing_api",
        "emu_errors", "ret_cmp", "ret_eq", "ret_runaddr", "state_cmp", "state_eq", "state_runaddr", "changed_cmp",
        "changed_eq", "distinct_csv", "distinct_emu", "tier", "first_diff", "note"]

# Keys whose state region holds an fnsave image: c3fo's x87 probe ($blk +0x40: eax, edx, then the fnsave image, so the
# status word's low byte is byte 12 of region 0). Unicorn 2.1.4 never sets the x87 exception flags (PE, IE, SF, ES: measured
# by re/tools/fixvm/x87_probe_unicorn.py), so these rows are also compared with that one byte searched over its 256 values.
X87_FSW_LOW = {"c3fo": (0, 12)}


def _detail_name(key: str) -> str:
    return key.replace("::", "_").replace("<", "(").replace(">", ")").replace("*", "P").replace(" ", "_")


# ---- one key (worker side) --------------------------------------------------------------------------------------------
def census_key(vm, key: str, init: bool = True, budget_s: float = 60.0) -> dict:
    import sound_emu as E
    hooks, batch = E.registry()
    spec = hooks[key]
    res = {}
    for lay in ("A", "B", "C"):
        try:
            r = E.KeyRun(vm, key, lay, init=init, budget_s=budget_s).run()
            res[lay] = r
        except E.EmuError as e:
            res[lay] = e
    return compare(key, spec, batch.get(key), res, E.read_csv(spec, key))


def compare(key, spec, batch, res, csv) -> dict:
    import sound_emu as E
    a, b, cc = res["A"], res["B"], res.get("C")
    out = {c: "" for c in COLS}
    out.update(key=key, batch=batch or "", rva=f"0x{spec['addr']:x}", fixture=spec.get("fixture") or "-",
               vectors=len(spec["vectors"]))
    detail = {"key": key, "rows": []}
    if csv is None:
        out.update(csv_vectors=0, csv_verdict="NOCSV")
    else:
        out.update(csv_vectors=len(csv.rows), csv_verdict=csv.verdict or "?", distinct_csv=csv.distinct if csv.distinct is not None else "")
    if isinstance(a, Exception):
        out.update(fixture_status="SETUP_ERR", missing_api=getattr(a, "api", "") or "", tier="SETUP_ERR",
                   first_diff=str(a)[:200])
        detail["error"] = str(a)
        return {"row": out, "detail": detail}
    out["styles"] = ",".join(sorted(a.used)) or "pure"
    if a.fixture_error:
        api = ""
        if "[" in a.fixture_error and a.fixture_error.endswith("]"):
            api = a.fixture_error.rsplit("[", 1)[1][:-1]
        out.update(fixture_status="FIXTURE_ERR", missing_api=api, tier="FIXTURE_ERR", first_diff=a.fixture_error[:200])
        detail["error"] = a.fixture_error
        return {"row": out, "detail": detail}
    out["fixture_status"] = "OK"
    n = dict(err=0, ret_cmp=0, ret_eq=0, ret_run=0, st_cmp=0, st_eq=0, st_run=0, ch_cmp=0, ch_eq=0, st_x87=0)
    x87flags = set()
    apis, first = [], ""
    stateful = bool(spec.get("state"))
    csvrows = csv.rows if csv else []
    vec_mismatch = False
    for i, ra in enumerate(a.rows):
        rb = b.rows[i] if not isinstance(b, Exception) and i < len(b.rows) else None
        rc = cc.rows[i] if cc is not None and not isinstance(cc, Exception) and i < len(cc.rows) else None
        if cc is not None and rc is None:
            rb = None
        elif rc is not None and rb is not None and (rc.error or rc.orig != rb.orig or rc.state_hex != rb.state_hex):
            # C disagrees with B on a field: fold that into B so the field reads as a run address below
            rb = Row2(rb.orig if rc.orig == rb.orig else None, rb.state_hex if rc.state_hex == rb.state_hex else None,
                      rb.error or rc.error)
        c = csvrows[i] if i < len(csvrows) else None
        d = {"v": E.vec_text(ra.vector), "a": ra.orig, "b": rb.orig if rb else None, "csv": c and c["original"],
             "sa": ra.state_orig, "sb": rb.state_orig if rb else None, "scsv": c and c["state_original"],
             "ca": ra.state_changed, "ccsv": c and c["state_changed"], "err": ra.error}
        detail["rows"].append(d)
        if c is not None and c["vector"] != E.vec_text(ra.vector):
            vec_mismatch = True
        if ra.error:
            n["err"] += 1
            if ra.api and ra.api not in apis:
                apis.append(ra.api)
            if not first:
                first = f"v{i} error: {ra.error[:150]}"
            continue
        if c is None:
            continue
        if rb is not None and rb.error is None and ra.orig == rb.orig:
            n["ret_cmp"] += 1
            if ra.orig == c["original"]:
                n["ret_eq"] += 1
            elif not first:
                first = f"v{i} ret emu {ra.orig} csv {c['original']}"
        else:
            n["ret_run"] += 1
        if stateful:
            if rb is not None and rb.error is None and ra.state_hex == rb.state_hex:
                n["st_cmp"] += 1
                if ra.state_orig == c["state_original"]:
                    n["st_eq"] += 1
                else:
                    fl = x87_flag_match(batch, ra.state_hex, c["state_original"])
                    if fl is not None:
                        n["st_x87"] += 1
                        x87flags.add(fl)
                    elif not first:
                        first = f"v{i} state emu {ra.state_orig} csv {c['state_original']}"
            else:
                n["st_run"] += 1
            if c["state_changed"] is not None:
                n["ch_cmp"] += 1
                if ra.state_changed == c["state_changed"]:
                    n["ch_eq"] += 1
                elif not first:
                    first = f"v{i} changed emu {ra.state_changed} csv {c['state_changed']}"
    distinct = len({(r.orig, r.state_orig) for r in a.rows if not r.error})
    out.update(missing_api=",".join(apis), emu_errors=n["err"], ret_cmp=n["ret_cmp"], ret_eq=n["ret_eq"], ret_runaddr=n["ret_run"],
               state_cmp=n["st_cmp"] if stateful else "-", state_eq=n["st_eq"] if stateful else "-",
               state_runaddr=n["st_run"] if stateful else "-", changed_cmp=n["ch_cmp"] if stateful else "-",
               changed_eq=n["ch_eq"] if stateful else "-", distinct_emu=distinct)
    nv = len(a.rows)
    diffs = (n["ret_cmp"] - n["ret_eq"]) + (n["st_cmp"] - n["st_eq"] - n["st_x87"]) + (n["ch_cmp"] - n["ch_eq"])
    if n["st_x87"]:
        out["note"] = (f"{n['st_x87']} state rows equal only with the x87 status flag byte set to "
                       f"{','.join(f'0x{x:02x}' for x in sorted(x87flags))} (Unicorn sets no x87 exception flag)")
    dist_ok = csv is not None and csv.distinct is not None and csv.distinct == distinct
    if csv is None or not csvrows:
        tier = "NOCSV"
    elif vec_mismatch or len(csvrows) != nv:
        tier = "CSV_STALE"
    elif n["err"]:
        tier = "EMU_ERR" if n["err"] == nv else "PARTIAL_ERR"
    elif diffs:
        tier = "DIFF"
    elif n["st_x87"]:
        tier = "EQUAL_MOD_X87FLAGS"
    elif n["ret_run"] == 0 and n["st_run"] == 0:
        tier = "EXACT" if dist_ok else "EXACT_DISTINCT_DIFF"
    else:
        tier = "EQUAL_PI" if dist_ok else "EQUAL_PI_DISTINCT_DIFF"
    out["tier"] = tier
    out["first_diff"] = first.replace("\t", " ")[:200]
    return {"row": out, "detail": detail}


class Row2:
    """B's row with every field C disagrees on blanked."""
    def __init__(self, orig, state_hex, error):
        self.orig, self.state_hex, self.error = orig, state_hex, error
        self.state_orig = None


def x87_flag_match(batch, state_hex, want):
    """The status-word flag byte value for which the emulated state hashes to the CSV's, or None."""
    import sound_emu as E
    loc = X87_FSW_LOW.get(batch or "")
    if not loc:
        return None
    regs = state_hex.split("|")
    r, off = loc
    b = bytearray.fromhex(regs[r])
    for v in range(256):
        b[off] = v
        regs[r] = b.hex()
        if E.fnv("|".join(regs)) == want:
            return v
    return None


def worker():
    """Line protocol: a JSON {"key", "init"} per line on stdin, a JSON result per line on stdout."""
    import sound_emu as E
    vm = E.FixVM()
    for line in sys.stdin:
        job = json.loads(line)
        t = time.time()
        try:
            r = census_key(vm, job["key"], init=job.get("init", True))
        except Exception as e:                       # the VM died or the link broke: report, restart the VM
            import traceback
            r = {"row": {c: "" for c in COLS} | {"key": job["key"], "tier": "TOOL_ERR",
                                                  "first_diff": f"{type(e).__name__}: {e}"[:200]},
                 "detail": {"key": job["key"], "error": traceback.format_exc()}}
            try:
                vm.close()
            except Exception:
                pass
            vm = E.FixVM()
        r["secs"] = round(time.time() - t, 2)
        sys.stdout.write(json.dumps(r) + "\n")
        sys.stdout.flush()


class Worker:
    def __init__(self):
        self.p = subprocess.Popen([sys.executable, __file__, "--worker"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True, cwd=str(ROOT), bufsize=1)
        self.key, self.t0 = None, 0.0

    def give(self, key, init):
        self.key, self.t0 = key, time.time()
        self.p.stdin.write(json.dumps({"key": key, "init": init}) + "\n")
        self.p.stdin.flush()

    def kill(self):
        try:
            self.p.kill()
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worker", action="store_true")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--keys", default="")
    ap.add_argument("--batch", default="")
    ap.add_argument("--out", default=str(OUT))
    ap.add_argument("--no-init", action="store_true", help="skip DllMain (the CRT start-up) before each key")
    ap.add_argument("--timeout", type=float, default=600.0, help="seconds per key before its worker is killed")
    a = ap.parse_args()
    if a.worker:
        return worker()
    import sound_emu as E
    keys = E.sound_keys()
    _, batch = E.registry()
    if a.keys:
        want = a.keys.split(",")
        keys = [k for k in keys if k in want]
    if a.batch:
        keys = [k for k in keys if batch.get(k) in a.batch.split(",")]
    DETAIL.mkdir(parents=True, exist_ok=True)
    import threading
    import queue
    results: dict = {}
    todo = list(keys)
    done_q: queue.Queue = queue.Queue()
    workers = [Worker() for _ in range(min(a.jobs, len(todo)) or 1)]

    def reader(w):
        for line in w.p.stdout:
            done_q.put((w, json.loads(line)))
        done_q.put((w, None))

    for w in workers:
        threading.Thread(target=reader, args=(w,), daemon=True).start()
    idle = list(workers)
    total = len(todo)
    t_start = time.time()
    busy = set()
    while todo or busy:
        while idle and todo:
            w = idle.pop()
            w.give(todo.pop(0), not a.no_init)
            busy.add(w)
        try:
            w, r = done_q.get(timeout=5)
        except queue.Empty:
            for w in list(busy):
                if time.time() - w.t0 > a.timeout:
                    k = w.key
                    w.kill()
                    busy.discard(w)
                    results[k] = {"row": {c: "" for c in COLS} | {"key": k, "tier": "TIMEOUT",
                                                                  "first_diff": f"worker killed after {a.timeout} s"}}
                    print(f"[{len(results)}/{total}] {k} TIMEOUT", flush=True)
                    nw = Worker()
                    threading.Thread(target=reader, args=(nw,), daemon=True).start()
                    idle.append(nw)
            continue
        if w not in busy:
            continue                                # a killed worker's last words
        busy.discard(w)
        if r is None:                               # the worker process ended
            k = w.key
            results[k] = {"row": {c: "" for c in COLS} | {"key": k, "tier": "TOOL_ERR", "first_diff": "worker exited"}}
            print(f"[{len(results)}/{total}] {k} TOOL_ERR worker exited", flush=True)
            nw = Worker()
            threading.Thread(target=reader, args=(nw,), daemon=True).start()
            idle.append(nw)
            continue
        k = r["row"]["key"]
        results[k] = r
        (DETAIL / (_detail_name(k) + ".json")).write_text(json.dumps(r.get("detail", {}), indent=0))
        row = r["row"]
        print(f"[{len(results)}/{total}] {k} {row['tier']} ({r.get('secs')} s) {row.get('first_diff', '')[:100]}", flush=True)
        idle.append(w)
    for w in workers + idle:
        w.kill()
    out = pathlib.Path(a.out)
    old = {}
    if out.exists() and (a.keys or a.batch):          # a partial run updates its rows in the existing table
        for line in out.read_text().splitlines()[1:]:
            f = line.split("\t")
            old[f[0]] = f
    rows = {k: [str(v["row"].get(c, "")) for c in COLS] for k, v in results.items()}
    merged = {**old, **rows}
    with out.open("w", newline="\n") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(merged):
            f.write("\t".join(merged[k]) + "\n")
    from collections import Counter
    print("tiers:", dict(Counter(r[COLS.index("tier")] for r in merged.values())), f"({time.time() - t_start:.0f} s)")


if __name__ == "__main__":
    sys.exit(main())
