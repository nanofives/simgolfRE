"""38f: re/tools/segment_corpus.py's pipeline for course_season_6y, the same steps in the same order, spread over two
instances (p38f, p38f2: copies of original\\ with the pinned references' winmm.dll 381ed2b2), with no per-step timeout
(the pipeline's 2,400 s capture timeout is below a 50,000-frame capture). Logs: log/p38/38f/cs6y_<step>.log.

    ~/bin/py -3.12 log/p38/cs6y_pipeline.py [--from capture|trace|plan|restart|verify|extract]
"""
import argparse
import concurrent.futures as CF
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
SC = "course_season_6y"
TOOL = ROOT / "re" / "tools" / "segment_corpus.py"
LOGS = ROOT / "log" / "p38" / "38f"
RUNS = ROOT / "log" / "p2" / "p24" / SC
INSTANCES = ["p38f", "p38f2"]
STEPS = ["capture", "trace", "plan", "restart", "verify", "extract"]


def run(argv, log_name, instance=None):
    LOGS.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, str(TOOL)] + (["--instance", instance] if instance else []) + argv
    t0 = time.time()
    with (LOGS / f"cs6y_{log_name}.log").open("w", encoding="utf-8") as f:
        f.write("$ " + " ".join(cmd) + "\n")
        f.flush()
        rc = subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=str(ROOT))
    print(f"{log_name}: rc={rc} {time.time() - t0:.0f}s", flush=True)
    return rc


def ok(label):
    p = RUNS / label / "summary.json"
    if not p.exists():
        return False
    s = json.loads(p.read_text())
    return not s.get("failure") and bool(s.get("checkpoints"))


def pool(jobs):
    """jobs: [(argv, log_name)], run two at a time, one per instance."""
    free = list(INSTANCES)
    results = {}
    with CF.ThreadPoolExecutor(len(INSTANCES)) as ex:
        pending = list(jobs)
        futs = {}
        while pending or futs:
            while pending and free:
                argv, name = pending.pop(0)
                inst = free.pop(0)
                futs[ex.submit(run, argv, name, inst)] = (name, inst)
                time.sleep(5)
            done, _ = CF.wait(list(futs), return_when=CF.FIRST_COMPLETED)
            for f in done:
                name, inst = futs.pop(f)
                results[name] = f.result()
                free.append(inst)
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="start", default="capture", choices=STEPS)
    a = ap.parse_args()
    k = STEPS.index(a.start)
    if k <= 0:
        jobs = [(["capture", "--scenario", SC, "--label", l], f"capture_{l}") for l in ("c_a", "c_b", "c_c") if not ok(l)]
        jobs.append((["trace", "--scenario", SC], "trace"))
        pool(jobs)
        bad = [l for l in ("c_a", "c_b", "c_c") if not ok(l)]
        if bad:      # one retry, as the pipeline does
            pool([(["capture", "--scenario", SC, "--label", l], f"capture_{l}_retry") for l in bad])
    elif k <= 1:
        pool([(["trace", "--scenario", SC], "trace")])
    if k <= 2:
        if run(["plan", "--scenario", SC], "plan") != 0:
            raise SystemExit("plan failed")
    if k <= 3:
        plan = json.loads((RUNS / "plan.json").read_text())
        jobs = []
        for seg in plan["segments"]:
            if not seg["continuous_same"]:
                continue
            labs = ["r1", "r2"] + (["cf"] if (seg.get("inputs") or {}).get("inputs") else [])
            for lab in labs:
                label = f"s{seg['id']:02d}_{lab}"
                if ok(label):
                    continue
                jobs.append((["restart", "--scenario", SC, "--segment", str(seg["id"]), "--label", label]
                             + (["--drop-input"] if lab == "cf" else []), f"restart_{label}"))
        pool(jobs)
        retry = [j for j in jobs if not ok(j[1].replace("restart_", ""))]
        if retry:
            pool([(argv, name + "_retry") for argv, name in retry])
    if k <= 4:
        run(["verify", "--scenario", SC], "verify")
    if k <= 5:
        run(["extract", "--scenario", SC], "extract")


if __name__ == "__main__":
    main()
