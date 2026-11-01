"""S3.25: log/roadmap/S3/25_recordings.tsv from the recordings on disk (one row per scenario, checkpoint and kind).

    ~/bin/py -3.12 log/roadmap/S3/25_table.py

The SAME evidence is re-computed for every run with re/tools/scenario_refs.py's comparison (run vs det_b, every checkpoint), not
copied from a log.
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
REC = ROOT / "port" / "corpus" / "w8rec"
TERR = ROOT / "log" / "p5" / "terrain"
GROUP = {("cov_b_inject", "inject"): "7", ("cov_b_inject2", "inject"): "7", ("cov_k3_camera", "camera"): "5",
         ("golfer_stories", "queue"): "4", ("golfer_stories", "queue_box"): "4", ("cov_b_holes18", "queue"): "3",
         ("cov_k3_camera2", "queue"): "3", ("cov_r_s2_screens", "terrain"): "2"}
FOCUS = {"cov_b_inject": {"zoom_box", "forced_events", "end"}, "cov_b_inject2": {"zoom_box", "forced_events", "end"},
         "cov_k3_camera": {"y_inj3"}, "golfer_stories": {"season_end"}, "cov_b_holes18": {"holes"}, "cov_k3_camera2": {"y_inj4"},
         "cov_r_s2_screens": {"course", "radial_hover", "rep_comments", "rep_histograph", "rep_financial", "rep_report1",
                              "rep_shortcuts", "rep_roster"}}


def check(scenario: str, run: str) -> str:
    p = subprocess.run([sys.executable, str(ROOT / "re/tools/scenario_refs.py"), "check", "--scenario", scenario, "--run", run],
                       capture_output=True, text=True, cwd=ROOT)
    out = [l for l in p.stdout.splitlines() if " vs det_b: " in l]
    return out[-1].split(" vs det_b: ", 1)[1] if out else ("check failed: " + (p.stdout + p.stderr).strip()[-200:])


rows = []
for ix in sorted(REC.glob("*/*/index.json")):
    d = json.loads(ix.read_text())
    sc, pre = d["scenario"], ix.parent.name
    run = pathlib.Path(d["run"]).name
    ev = f"{run} vs det_b: {check(sc, run)}"
    kind = {"inject": "call log + box watch after the queue", "camera": "call log (lines, worldToScreen outs) + root at queue return",
            "queue": "queue dump + call log + root at queue return", "queue_box": "queue dump + call log + root at queue return"}[pre]
    if pre.startswith("queue") and d["cfg"].get("boxes"):
        kind += f" + box watch from frame start {d['cfg']['boxes']}"
    for cp in d["checkpoints"]:
        if cp in FOCUS.get(sc, ()):
            rows.append([sc, cp, GROUP.get((sc, pre), ""), kind, str(ix.parent.relative_to(ROOT)).replace("\\", "/"), ev])
for t in sorted(TERR.glob("s325_*/index.json")):
    d = json.loads(t.read_text())
    sc, run = d["scenario"], pathlib.Path(d["run"]).name
    ev = f"{run} vs det_b: {check(sc, run)}"
    for cp in d["snaps"]:
        if cp in FOCUS.get(sc, ()):
            rows.append([sc, cp, "2", "terrain (updateTerrainRender entry / exit roots, state copy, Terrain calls)",
                         str(t.parent.relative_to(ROOT)).replace("\\", "/"), ev])
hdr = ["scenario", "checkpoint", "group", "kind", "path", "same_as_det_b"]
out = "\t".join(hdr) + "\n" + "".join("\t".join(r) + "\n" for r in rows)
(ROOT / "log/roadmap/S3/25_recordings.tsv").write_text(out)
print(out, end="")
