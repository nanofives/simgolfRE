"""S2.8 merge item 3: copy the two re-pinned golden digests from the mirror into the working tree's files (only these keys;
every other key of the working-tree files is kept). The mirror pinned them with SG_EMU_ORACLE=update after the emulator oracle
was GREEN on every golden case (log/roadmap/S2/8_notes.md).

usage: ~/bin/py -3.12 log/roadmap/S2/8_tools/merge_goldens.py [--check]
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
MIR = ROOT / "port" / "build" / "S2_8_head"
KEYS = {"port/test/golden_reports.json": ["showEndOfYearScreen"],
        "port/test/golden_commands.json": ["showPlayerCommentsReport"]}


def main():
    check = "--check" in sys.argv
    for f, keys in KEYS.items():
        src = json.loads((MIR / f).read_text())
        dst_p = ROOT / f
        dst = json.loads(dst_p.read_text())
        for k in keys:
            print(f, k, "same" if dst.get(k) == src[k] else "differs")
            dst[k] = src[k]
        if not check:
            dst_p.write_text(json.dumps(dst, indent=0, sort_keys=True))


if __name__ == "__main__":
    main()
