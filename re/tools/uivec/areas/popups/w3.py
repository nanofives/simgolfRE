"""P41 popups round 3 (2026-11-24): repoint common.py's outputs to log/roadmap/P41/work3/popups/ so the first round's files
in work/popups stay as they are. Import this module before any builder (it patches common.OUT / SCR / RESULTS in place)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

OUT3 = os.path.join(c.ROOT, "log", "roadmap", "P41", "work3", "popups")


def apply():
    """Point common's outputs at work3. Call again after importing any round-2 module (they import w2, which repoints to work2)."""
    c.OUT = OUT3
    c.SCR = os.path.join(OUT3, "_read")
    c.RESULTS = os.path.join(OUT3, "results.tsv")
    os.makedirs(c.SCR, exist_ok=True)


apply()
COLS = ("id", "status", "how", "changed", "unsure")


def record(eid, status, how, changed, unsure=""):
    """One results.tsv row of the fix round: id, status, how, changed (what changed against his note), unsure."""
    rows = {}
    if os.path.exists(c.RESULTS):
        with open(c.RESULTS, encoding="utf-8") as f:
            for ln in f.read().splitlines()[1:]:
                p = ln.split("\t")
                rows[p[0]] = p
    rows[eid] = [eid, status, how.replace("\t", " "), changed.replace("\t", " "), unsure.replace("\t", " ")]
    with open(c.RESULTS, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")
