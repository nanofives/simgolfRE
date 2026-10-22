"""Phase 4, the boot path (re/PHASE4_BOOT_PLAN.md): whole scenarios run from process start over the user's own exe.

A BOOT RUN starts from the exe's initialised data (sg_exe_fill_state 1: no image of the original), attaches the install as
game files (plus the scenario's fixture files, as SegmentRunner does) and the host text provider, and runs sg_run_boot over the
scenario's whole trace (port/corpus/traces/<scenario>.trace, recorded from the game's spawn): the records before the first
FRAME run the boot (the CRT's static construction, gameMain's part, mainLoop's Setup, the title menu and Session), then every
frame. Record k of the run is compared with record k of the original's per-frame sim hashes from a capture run of the same
pinned schedule (log/p2/p24/<scenario>/c_a/framehash.bin: u32 T, u32 frame index, u64 sgh64 x 203, from the first frame head).

    ~/bin/py -3.12 -m port.test.boot <scenario>... | --all [--jobs 3] [--out log/p4/boot_gate.tsv]
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import json
import os
import pathlib
import struct
import subprocess
import sys
from dataclasses import dataclass, field

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import segment_files  # noqa: E402
from port.test import segments as SG  # noqa: E402

TRACES = ROOT / "port" / "corpus" / "traces"
CAPTURES = ROOT / "log" / "p2" / "p24"

# Records where the capture's original read uninitialised stack the port cannot reproduce: scenario -> [(first record, end
# record, objects not compared in that range, U row)]. The run passes as KNOWN when only these differ there.
# U-1062: screenToTile's first projection is off the grid, so its first distance uses two uninitialised locals; under the
# capture's frame-hash tracer they hold junk whose distance overflows to 0 and the climb keeps the off-grid tile
_CURSOR = ("g_cursorTileX", "g_cursorTileY")
KNOWN = {
    "cov_b_holes18": [(727, 734, _CURSOR, "U-1062")],     # (48, -1) where the port climbs to (49, 0)
    "cov_a_golfer": [(4334, 4401, _CURSOR, "U-1062")],    # (50, 39) where the port climbs to (49, 39)
}


@dataclass
class BootResult:
    scenario: str
    status: str                 # PASS / KNOWN (only the KNOWN table's objects differ) / FAIL / ERROR / MISSING / NO_FRAMES
    records: int = 0
    expected: int = 0
    first_divergence: dict | None = None
    detail: str = ""
    run_detail: str = ""
    extra: dict = field(default_factory=dict)


def expected_records(scenario: str):
    fj = CAPTURES / scenario / "c_a" / "framehash.json"
    fb = CAPTURES / scenario / "c_a" / "framehash.bin"
    if not fj.exists() or not fb.exists():
        return None, None
    meta = json.loads(fj.read_text())
    n = len(meta["symbols"])
    raw = fb.read_bytes()
    rec = 8 + 8 * n
    out = [(struct.unpack_from("<I", raw, i)[0], list(struct.unpack_from("<%dQ" % n, raw, i + 8))) for i in range(0, len(raw), rec)]
    return meta["symbols"], out


def scenarios() -> list[str]:
    return sorted(p.stem for p in TRACES.glob("*.trace") if (CAPTURES / p.stem / "c_a" / "framehash.bin").exists())


def run(scenario: str, lib: L.Lib | None = None, max_records: int | None = None, before=None, after=None) -> BootResult:
    lib = lib or L.load()
    symbols, exp = expected_records(scenario)
    tr = TRACES / f"{scenario}.trace"
    if exp is None or not tr.exists():
        return BootResult(scenario, "MISSING", detail="no trace or no capture frame hashes")
    if not exp:
        # the capture never reached mainLoop's frame head (cov_b_location stays on the location screen): nothing to compare
        return BootResult(scenario, "NO_FRAMES", detail="the capture has no frame record")
    names = [n for n, _, _ in lib.objects("sim")]
    if symbols != names:
        return BootResult(scenario, "ERROR", detail="the capture's symbols are not the library's sim objects")
    why = lib.exe_load()
    if why:
        return BootResult(scenario, "MISSING", detail=f"exe: {why}")
    n = len(exp) if max_records is None else min(max_records, len(exp))
    res = BootResult(scenario, "FAIL", expected=n)
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.fill_from_exe(True)
        if SG.INSTALL.is_dir():
            st.files_root(SG.INSTALL)
        for fname, data in segment_files.files_for(scenario).items():
            st.files_put(fname, data)
        gdi = lib.has("gditext.attach") and os.environ.get("SG_TEXT_METRICS", "gdi") == "gdi"
        if gdi:
            st.call("gditext.attach", [])
        try:
            if before:
                before(st)
            code, done, ts, hs = st.run_boot(tr.read_bytes(), n)
            res.run_detail = SG.SegmentRunner(lib)._detail(st)
            if after:
                after(st, res)
        finally:
            if gdi:
                st.call("gditext.detach", [])
    res.records = done
    known = KNOWN.get(scenario, [])
    seen = []   # (range, first and last record that differed in it)
    for k in range(done):
        bad = [names[j] for j in range(len(names)) if hs[k][j] != exp[k][1][j]]
        rng = next((r for r in known if r[0] <= k < r[1] and bad and set(bad) <= set(r[2]) and ts[k] == exp[k][0]), None)
        if rng:
            if seen and seen[-1][0] is rng:
                seen[-1][2] = k
            else:
                seen.append([rng, k, k])
            continue
        if bad or ts[k] != exp[k][0]:
            res.first_divergence = {"record": k, "T_port": ts[k], "T_orig": exp[k][0], "objects": bad[:20]}
            res.detail = json.dumps(res.first_divergence)[:300]
            return res
    if code != L.SG_OK:
        res.detail = f"sg_run_boot returned {code} ({L.ERRORS.get(code, '?')}) after {done} of {n} records"
        return res
    if done < n:
        res.detail = f"{done} of {n} records"
        return res
    res.status = "KNOWN" if seen else "PASS"
    if seen:
        res.detail = "; ".join(f"records {a}..{b}: {', '.join(r[2])} differ ({r[3]})" for r, a, b in seen)
    return res


def _one(scenario: str) -> dict:
    r = run(scenario)
    return {"scenario": r.scenario, "status": r.status, "records": r.records, "expected": r.expected, "detail": r.detail,
            "run_detail": r.run_detail}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenarios", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    names = scenarios() if a.all else a.scenarios
    rows = []
    if a.jobs > 1:
        # one process per scenario: a boot run of a long scenario holds its whole trace and hash arrays
        cmds = {s: [sys.executable, "-m", "port.test.boot", s, "--out", "-"] for s in names}
        with cf.ThreadPoolExecutor(a.jobs) as ex:
            futs = {ex.submit(subprocess.run, c, capture_output=True, text=True, cwd=str(ROOT)): s for s, c in cmds.items()}
            for i, f in enumerate(cf.as_completed(futs), 1):
                s = futs[f]
                out = f.result().stdout.strip().splitlines()
                row = json.loads(out[-1]) if out and out[-1].startswith("{") else {"scenario": s, "status": "ERROR", "records": 0,
                                                                                    "expected": 0, "detail": f.result().stderr[-300:],
                                                                                    "run_detail": ""}
                rows.append(row)
                print(f"[{i}/{len(names)}] {s} {row['status']} {row['records']}/{row['expected']} {row['detail'][:160]}", flush=True)
    else:
        for i, s in enumerate(names, 1):
            row = _one(s)
            rows.append(row)
            if a.out == "-":
                print(json.dumps(row))
            else:
                print(f"[{i}/{len(names)}] {s} {row['status']} {row['records']}/{row['expected']} {row['detail'][:160]}", flush=True)
    if a.out and a.out != "-":
        p = pathlib.Path(a.out)
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open("w", encoding="utf-8") as f:
            f.write("scenario\tstatus\trecords\texpected\tdetail\trun_detail\n")
            for r in sorted(rows, key=lambda r: r["scenario"]):
                f.write("\t".join(str(r[k]).replace("\t", " ") for k in ("scenario", "status", "records", "expected", "detail",
                                                                          "run_detail")) + "\n")
        counts = {}
        for r in rows:
            counts[r["status"]] = counts.get(r["status"], 0) + 1
        print(json.dumps({"counts": counts}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
