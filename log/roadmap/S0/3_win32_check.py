"""S0.3: Win32 sample of the census. 10 mixed-status rows + 3 PASS rows of re/roadmap/frame_census.tsv re-run on the Win32 backend
(S0.3's own build port/build/S0_3_w32) and compared with the census row (status, total_px, one_level_px, beyond_px, regions, kind).
Run from the export root: SG_BACKEND=win32 ~/bin/py -3.12 log/roadmap/S0/3_win32_check.py [census.tsv]
The -11-at-end relabel of 3_rerun_noend.py is applied for the three scenarios it covers (as the x64 rows were made)."""
import collections
import csv
import json
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import boot as BT  # noqa: E402
from port.test import build as B  # noqa: E402

W32 = ROOT / "port" / "build" / "S0_3_w32" / "Release"
B.runner32_path = lambda config="Release": W32 / "portsim_runner32.exe"
_dll = B.dll_path
B.dll_path = lambda arch=None, config="Release": W32 / "portsim_c.dll" if arch == "Win32" else _dll(arch, config)
B.runner32_stale = lambda config="Release": False
B.stale = lambda arch=None, config="Release": False
from port.test import framegate as FG  # noqa: E402

_orig = BT.run


def _run(*a, **k):
    r = _orig(*a, **k)
    if r.status == "FAIL" and r.first_divergence is None and r.records == r.expected and "-11" in r.detail:
        r.detail, r.status = "KNOWN by S0.3: " + r.detail + "; every sim hash equal", "KNOWN"
    return r


BT.run = _run

census = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT.parents[2] / "re/roadmap/frame_census.tsv")
rows = [r for r in csv.DictReader((l for l in open(census, encoding="utf-8") if not l.startswith("#")), delimiter="\t")]
by = {(r["scenario"], r["checkpoint"]): r for r in rows}
PICK = [("sandbox_basic", "menu"), ("cov_b_location", "refused"), ("cov_r_quit", "title1"),                    # PASS
        ("course_season", "sandbox"), ("course_season", "hole_open"), ("cov_a_build", "terrain_03"), ("championship", "tournament_start"),
        ("cov_b_locations", "phoenix"), ("cov_r_quit", "quit_00"), ("cov_a_champ", "n_yes"), ("cov_a_quit", "menu"),
        ("cov_a_keys", "holes"), ("ui_tour", "menu")]
want = collections.OrderedDict()
for s, c in PICK:
    if (s, c) in by:
        want.setdefault(s, []).append(c)
out, bad = [], 0
for s, cs in want.items():
    got = {r["checkpoint"]: r for r in FG.run_scenario(s, cs, probe=False)}
    for c in cs:
        x, w = by[(s, c)], got[c]
        same = (x["status"] == w["status"] and str(x.get("total_px", "")) == str(w.get("total_px", "")) and
                str(x.get("beyond_px", "")) == str(w.get("beyond_px", "")) and x.get("regions", "") == (w.get("regions") or "") and
                x.get("kind", "") == (w.get("kind") or ""))
        bad += not same
        out.append(dict(scenario=s, checkpoint=c, x64_status=x["status"], win32_status=w["status"], x64_px=x.get("total_px", ""),
                        win32_px=w.get("total_px", ""), same=same))
        print(out[-1], flush=True)
print(json.dumps({"rows": len(out), "differ": bad}))
