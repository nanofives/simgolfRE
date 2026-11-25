"""P41 popups round 5 (2026-11-25): repoint common.py's outputs to log/roadmap/P41/work5/popups/ (rounds 1-4 stay as they are).
Call apply() after every import of an older round's module (they repoint to work2 / work3 / work4) and right before writing."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

OUT5 = os.path.join(c.ROOT, "log", "roadmap", "P41", "work5", "popups")


def apply():
    c.OUT = OUT5
    c.SCR = os.path.join(OUT5, "_read")
    c.RESULTS = os.path.join(OUT5, "results.tsv")
    os.makedirs(c.SCR, exist_ok=True)


def check():
    """Refuse to write anywhere but work5."""
    assert os.path.normcase(c.OUT) == os.path.normcase(OUT5), c.OUT
    assert os.path.normcase(c.RESULTS) == os.path.normcase(os.path.join(OUT5, "results.tsv")), c.RESULTS


apply()
COLS = ("id", "status", "changed", "unsure")


def record(eid, status, changed, unsure=""):
    """One results.tsv row of round 5: id, status, changed, unsure."""
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
