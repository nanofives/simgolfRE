"""Phase 4 .sve round trip: the saved games the ORIGINAL wrote in a scenario, as reference files for the port.

re/tools/scenario.py's cov_r_* scenarios copy every .sve of the instance's saved-games directory into
<run>/saves/<checkpoint>/ (with index.json = {name: sha256}) before the checkpoint. This tool collects them from every run
of the scenario (the pinned references log/c3/alt/refs/<scenario>/det_b, det_c and the segment corpus runs
log/p2/p24/<scenario>/c_*, r*), requires all of them to agree file for file, and copies one set into the corpus:

    ~/bin/py -3.12 -m port.test.sve_refs collect cov_r_sve      # -> port/corpus/saves/cov_r_sve/<checkpoint>/

The corpus is local (port/corpus/ is gitignored: the files are game-derived). port/test/test_sve_roundtrip.py reads it.
"""
from __future__ import annotations

import json
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CORPUS = ROOT / "port" / "corpus" / "saves"
RUN_ROOTS = [ROOT / "log" / "c3" / "alt" / "refs", ROOT / "log" / "p2" / "p24"]


def run_dirs(scenario: str) -> list[pathlib.Path]:
    out = []
    for root in RUN_ROOTS:
        d = root / scenario
        if d.is_dir():
            # not the counterfactual restarts (<k>_cf: the segment's input dropped, so no save is made)
            out += sorted(p for p in d.iterdir() if (p / "saves").is_dir() and not p.name.endswith("_cf"))
    return out


def collect(scenario: str) -> dict:
    """{checkpoint: index} after checking every run agrees; copies the files of the first run into the corpus."""
    runs = run_dirs(scenario)
    if not runs:
        raise SystemExit(f"{scenario}: no run with a saves/ folder under {[str(r) for r in RUN_ROOTS]}")
    ref, ref_run = {}, {}
    for r in runs:
        for cp in sorted(p for p in (r / "saves").iterdir() if p.is_dir()):
            idx = json.loads((cp / "index.json").read_text())
            if cp.name not in ref:
                ref[cp.name], ref_run[cp.name] = idx, r
            elif idx != ref[cp.name]:
                bad = sorted(k for k in set(idx) | set(ref[cp.name]) if idx.get(k) != ref[cp.name].get(k))
                raise SystemExit(f"{scenario}@{cp.name}: {r} differs from {ref_run[cp.name]} in {bad}")
    for cp, idx in ref.items():
        dst = CORPUS / scenario / cp
        if dst.exists():
            shutil.rmtree(dst)
        dst.mkdir(parents=True)
        src = ref_run[cp] / "saves" / cp
        for name in idx:
            shutil.copy2(src / disk_name(name), dst / disk_name(name))
        (dst / "index.json").write_text(json.dumps({"files": idx, "runs": [str(r.relative_to(ROOT)) for r in runs]},
                                                   indent=1))
        print(f"{scenario}@{cp}: {len(idx)} files, {len(runs)} runs agree -> {dst.relative_to(ROOT)}")
    return ref


def disk_name(key: str) -> str:
    """Index keys are saved-game file names, or install-relative paths such as Themes/Championship/x.cse (re/tools/scenario.py
    _cov_r_collect's `extra`), stored with '/' as '__'."""
    return key.replace("/", "__")


def logical(key: str) -> str:
    """The GameFiles name of an index key."""
    return key if "/" in key else "Saved Games/" + key


def load(scenario: str, checkpoint: str) -> dict[str, bytes] | None:
    """{index key: bytes} of the files the original's install held at `checkpoint` (the saved games, plus any collected
    install folder), None when not collected."""
    d = CORPUS / scenario / checkpoint
    if not (d / "index.json").exists():
        return None
    idx = json.loads((d / "index.json").read_text())["files"]
    return {n: (d / disk_name(n)).read_bytes() for n in idx}


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] != "collect":
        raise SystemExit(__doc__)
    collect(sys.argv[2])
