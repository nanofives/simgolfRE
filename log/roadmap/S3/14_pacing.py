"""S3.14: test_app_ingame's session + pacing tests, N sessions per variant, variants interleaved so they share the ambient load.
Per session: the two tests' outcome, the frame periods test_pacing_under_ingame_load judges (frames that waited and worked < 87 ms:
n, median, max, within 87 + 10 + 2), the frames that worked >= 87 ms without a terrain pass (the test's failing class), the work of
frames without a terrain pass (median / p99 / max), the machine's CPU busy % over the session (GetSystemTimes) and free physical
memory. The session's waits.tsv / frames.tsv / session.json are copied to log/roadmap/S3/14_pacing/<variant>_<k>/ and the session
directory the run kept is removed. Output: log/roadmap/S3/14_pacing.tsv (+ a summary on stdout).

    ~/bin/py -3.12 log/roadmap/S3/14_pacing.py [--n 10] [--variants c,d]
variants: c = port/build/S3_14 (app Normal, the default of 14_edits.py / G10), a = S3_14 with SG_APP_PRIORITY=inherit,
          d = port/build/S3_14_head (d66aa755: the app inherits pytest's BelowNormal)
"""
import ctypes as C
import ctypes.wintypes as W
import os
import pathlib
import re
import shutil
import statistics
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = ROOT / "log/roadmap/S3/14_pacing"
PYEXE = r"C:\Users\maria\AppData\Local\Programs\Python\Launcher\py.exe"
VAR = {"c": ("S3_14", {}), "a": ("S3_14", {"SG_APP_PRIORITY": "inherit"}), "d": ("S3_14_head", {})}


def sys_times():
    i, k, u = W.FILETIME(), W.FILETIME(), W.FILETIME()
    C.windll.kernel32.GetSystemTimes(C.byref(i), C.byref(k), C.byref(u))
    v = lambda f: (f.dwHighDateTime << 32) | f.dwLowDateTime  # noqa: E731
    return v(i), v(k) + v(u)


def free_phys_gb():
    class MS(C.Structure):
        _fields_ = [("dwLength", W.DWORD), ("dwMemoryLoad", W.DWORD), ("ullTotalPhys", C.c_ulonglong),
                    ("ullAvailPhys", C.c_ulonglong), ("ullTotalPageFile", C.c_ulonglong), ("ullAvailPageFile", C.c_ulonglong),
                    ("ullTotalVirtual", C.c_ulonglong), ("ullAvailVirtual", C.c_ulonglong), ("ullAvailExtendedVirtual", C.c_ulonglong)]
    m = MS()
    m.dwLength = C.sizeof(MS)
    C.windll.kernel32.GlobalMemoryStatusEx(C.byref(m))
    return round(m.ullAvailPhys / 2 ** 30, 1)


def stats(rec):
    heads = [tuple(int(x) for x in l.split("\t")) for l in (rec / "frames.tsv").read_text().splitlines() if l.strip()]
    waits = [[int(x) for x in l.split("\t")] for l in (rec / "waits.tsv").read_text().splitlines() if l.strip()]
    lo = heads[0][0] + 5 if heads else 0
    by = {f: t for f, t, _ in heads}
    wd = {w[0]: w for w in waits}
    prev = {waits[i][0]: waits[i - 1][6] for i in range(1, len(waits))}
    fits, over_np, np_work = [], [], []
    for k in sorted(by):
        if k < lo or k + 1 not in by or k not in wd:
            continue
        w = wd[k]
        per, passes = by[k + 1] - by[k], w[6] - prev.get(k, w[6])
        if passes == 0:
            np_work.append(w[1])
        if w[1] < 87:
            fits.append(per)
        elif passes == 0:
            over_np.append((k, w[1]))
    q = sorted(np_work)
    return {"fits": len(fits), "fit_med": statistics.median(fits) if fits else None, "fit_max": max(fits) if fits else None,
            "within": round(sum(1 for p in fits if p <= 99) / len(fits), 4) if fits else None,
            "over_nopass": over_np, "np_med": statistics.median(q) if q else None,
            "np_p99": q[int(len(q) * 0.99)] if q else None, "np_max": q[-1] if q else None}


def one(variant, k):
    tree, extra = VAR[variant]
    m = ROOT / "port/build" / tree
    env = dict(os.environ, S7_5_KEEP="1", SG_PORTSIM_DLL=str(m / "port/build/x64/Release/portsim_c.dll"),
               SG_APP_EXE=str(m / "port/build/x64/app/Release/simgolf_port.exe"), **extra)
    env.pop("SG_APP_PRIORITY", None) if "SG_APP_PRIORITY" not in extra else None
    i0, b0 = sys_times()
    t0 = time.time()
    r = subprocess.run([PYEXE, "-3.12", "-m", "pytest", "port/test/test_app_ingame.py", "-k", "session_ran or pacing_under", "-q",
                        "-p", "no:cacheprovider"], cwd=str(m), env=env, capture_output=True, text=True)
    i1, b1 = sys_times()
    busy = round(100.0 * (1 - (i1 - i0) / max(1, b1 - b0)), 1)
    txt = r.stdout + r.stderr
    kept = re.search(r"^kept (.+)$", txt, re.M)
    res = {"variant": variant, "k": k, "rc": r.returncode, "secs": round(time.time() - t0, 1), "cpu_busy": busy,
           "free_phys_gb": free_phys_gb(), "summary": (txt.strip().splitlines() or [""])[-1]}
    res["session_ran"] = "FAILED port/test/test_app_ingame.py::test_ingame_session_ran" not in txt
    res["pacing"] = "FAILED port/test/test_app_ingame.py::test_pacing_under_ingame_load" not in txt
    if kept:
        d = pathlib.Path(kept.group(1).strip())
        rec = d / "rec"
        dst = OUT / f"{variant}_{k:02d}"
        dst.mkdir(parents=True, exist_ok=True)
        for f in ("waits.tsv", "frames.tsv", "session.json"):
            if (rec / f).exists():
                shutil.copyfile(rec / f, dst / f)
        try:
            res.update(stats(rec))
        except Exception as e:  # noqa: BLE001
            res["stats_error"] = repr(e)
        shutil.rmtree(d, ignore_errors=True)   # the session directory this run kept (S7_5_KEEP), nothing else
    return res


def main():
    n = int(sys.argv[sys.argv.index("--n") + 1]) if "--n" in sys.argv else 10
    vs = sys.argv[sys.argv.index("--variants") + 1].split(",") if "--variants" in sys.argv else ["c", "d"]
    OUT.mkdir(parents=True, exist_ok=True)
    cols = ["variant", "k", "session_ran", "pacing", "fits", "fit_med", "fit_max", "within", "over_nopass", "np_med", "np_p99",
            "np_max", "cpu_busy", "free_phys_gb", "secs", "summary"]
    tsv = ROOT / "log/roadmap/S3/14_pacing.tsv"
    new = not tsv.exists()
    with open(tsv, "a", newline="\n") as f:
        if new:
            f.write("\t".join(cols) + "\n")
        for k in range(n):
            for v in vs:
                r = one(v, k)
                f.write("\t".join(str(r.get(c, "")) for c in cols) + "\n")
                f.flush()
                print(f"[{k * len(vs) + vs.index(v) + 1}/{n * len(vs)}] {v}{k:02d} session {'PASS' if r['session_ran'] else 'FAIL'} "
                      f"pacing {'PASS' if r['pacing'] else 'FAIL'} fit_med {r.get('fit_med')} fit_max {r.get('fit_max')} "
                      f"over_nopass {r.get('over_nopass')} np_med {r.get('np_med')} np_max {r.get('np_max')} cpu {r['cpu_busy']}% "
                      f"free {r['free_phys_gb']} GB", flush=True)


if __name__ == "__main__":
    main()
