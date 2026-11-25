"""P41 popups round 4 (2026-11-25): repoint common.py's outputs to log/roadmap/P41/work4/popups/ (rounds 1-3 stay as they are).
Call apply() after every import of an older round's module (they repoint to work2 / work3) and right before writing."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

OUT4 = os.path.join(c.ROOT, "log", "roadmap", "P41", "work4", "popups")


def apply():
    c.OUT = OUT4
    c.SCR = os.path.join(OUT4, "_read")
    c.RESULTS = os.path.join(OUT4, "results.tsv")
    os.makedirs(c.SCR, exist_ok=True)


def check():
    """Refuse to write anywhere but work4."""
    assert os.path.normcase(c.OUT) == os.path.normcase(OUT4), c.OUT
    assert os.path.normcase(c.RESULTS) == os.path.normcase(os.path.join(OUT4, "results.tsv")), c.RESULTS


apply()
COLS = ("id", "status", "changed", "unsure")


def record(eid, status, changed, unsure=""):
    """One results.tsv row of round 4: id, status, changed, unsure."""
    check()
    rows = {}
    if os.path.exists(c.RESULTS):
        with open(c.RESULTS, encoding="utf-8") as f:
            for ln in f.read().splitlines()[1:]:
                p = ln.split("\t")
                rows[p[0]] = p
    rows[eid] = [eid, status, changed.replace("\t", " "), unsure.replace("\t", " ")]
    with open(c.RESULTS, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")
