"""I22 mirror: port/build/I22 = `git archive d075dc56` (I21's commit: wave 8 + both fix layers) + the junctions, then S3 wave 9's
rows 4 and 5 (log/roadmap/S3/MERGE_w9.md section 1: 28_assemble.py --only-28, 29_assemble.py --only-29), then the documents of
MERGE_w9.md: w9_uncertain.md's rows into UNCERTAINTIES.md (an id already there is replaced) and the three CLAUDE.md paragraphs of
section 4 after the last wave 8 paragraph. Orchestrator, 2026-11-01.

    ~/bin/py -3.12 log/roadmap/merges/I22/make_mirror.py        # refuses an existing mirror
"""
from __future__ import annotations

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
BASE = "d075dc56"
M = ROOT / "port" / "build" / "I22"
S3 = ROOT / "log" / "roadmap" / "S3"
PY = [sys.executable]


def run(*args: str) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run([*PY, *args], cwd=ROOT, check=True)


def section(text: str, start: str, stop: str) -> str:
    i = text.index(start) + len(start)
    j = text.find(stop, i)
    return text[i:j if j >= 0 else len(text)].strip("\r\n")


def main() -> None:
    if "--claude-only" in sys.argv:
        return claude()
    assert not M.exists(), M
    run(str(S3 / "w7_assemble.py"), "--dst", "I22", "--commit", BASE, "--plain")
    run(str(S3 / "28_assemble.py"), "--root", str(M), "--only-28")
    run(str(S3 / "29_assemble.py"), "--root", str(M), "--only-29")
    (M / "HEAD_COMMIT").write_text(BASE + " + S3 wave 9 rows 4-5 (I22)\n")

    # UNCERTAINTIES.md: replace rows whose id is present, append the others in w9_uncertain.md's order
    rows = [l for l in (S3 / "w9_uncertain.md").read_text(encoding="utf-8").splitlines() if re.match(r"\| U-\d{4} \|", l)]
    up = M / "UNCERTAINTIES.md"
    t = up.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    lines = t.split(nl)
    index = {re.match(r"\| (U-\d{4}) \|", l).group(1): k for k, l in enumerate(lines) if re.match(r"\| U-\d{4} \|", l)}
    added = replaced = 0
    for r in rows:
        uid = re.match(r"\| (U-\d{4}) \|", r).group(1)
        if uid in index:
            lines[index[uid]] = r
            replaced += 1
        else:
            last = max(k for k, l in enumerate(lines) if re.match(r"\| U-\d{4} \|", l))
            lines.insert(last + 1, r)
            index = {re.match(r"\| (U-\d{4}) \|", l).group(1): k for k, l in enumerate(lines) if re.match(r"\| U-\d{4} \|", l)}
            added += 1
    up.write_bytes(nl.join(lines).encode("utf-8"))
    print(f"UNCERTAINTIES.md: {replaced} replaced, {added} added")

    claude()


def claude() -> None:
    # CLAUDE.md: S3.28's paragraph, S3.29's, the wave 9 note, after the last wave 8 paragraph
    p28 = section((S3 / "28_merge.md").read_text(encoding="utf-8"), "### 4. CLAUDE.md paragraph (after the last wave-8 paragraph)", "\n#")
    m29 = (S3 / "29_merge.md").read_text(encoding="utf-8")
    p29 = section(m29, "## CLAUDE.md (suggested paragraph, after S3.21's / wave 8's)", "\n## ")
    w9 = (S3 / "MERGE_w9.md").read_text(encoding="utf-8")
    note = section(w9, "3. The wave 9 note:\n```", "```")
    paras = [p.strip() for p in (p28, p29, note)]
    cp = M / "CLAUDE.md"
    c = cp.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in c else "\n"
    anchor = "11 are group 5 (explained), group 2 (U-1952, 8 rows), U-1826, U-1829." + nl
    assert c.count(anchor) == 1, "wave 8 anchor"
    assert "Roadmap S3.28, lowerEdgeCorner" not in c
    block = "".join(p.replace("\r\n", "\n").replace("\n", nl) + nl for p in paras)
    cp.write_bytes(c.replace(anchor, anchor + block, 1).encode("utf-8"))
    print("CLAUDE.md: 3 paragraphs inserted")


if __name__ == "__main__":
    main()
