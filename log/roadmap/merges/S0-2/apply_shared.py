"""I2 (S0 merge): apply the shared-file edits of log/roadmap/S0/MERGE.md section 1 to a tree root (the mirror first, then
the shared tree). The texts are read from MERGE.md itself (1.1: the `| U-11..` rows; 1.2 / 1.3: the first two fenced blocks).
Every edit is checked: the anchor must occur exactly once and the edit must not be present yet.
Usage: ~/bin/py -3.12 log/roadmap/merges/S0-2/apply_shared.py <root> [--empty-addr]
--empty-addr: write the U-rows' address cell empty instead of MERGE.md's `-` (scripts/re_classify.py norm_addr raises ValueError
on `-`, which fails tests/test_classify.py::test_real_tracker_state; see log/roadmap/merges/S0-2.md)."""
import pathlib
import re
import sys

MERGE = pathlib.Path(__file__).resolve().parents[2] / "S0" / "MERGE.md"   # log/roadmap/S0/MERGE.md (log is shared)
root = pathlib.Path(sys.argv[1]).resolve()
empty_addr = "--empty-addr" in sys.argv[2:]
CRLF = "\r\n"

merge = MERGE.read_bytes().decode("utf-8").replace("\r\n", "\n")
sec11 = merge.split("### 1.1", 1)[1].split("### 1.2", 1)[0]
rows = [l for l in sec11.split("\n") if l.startswith("| U-")]
assert [r.split("|")[1].strip() for r in rows] == ["U-1100", "U-1101", "U-1102", "U-1110", "U-1111", "U-1112", "U-1113",
                                                   "U-1114", "U-1115", "U-1120", "U-1121"], rows
if empty_addr:
    rows = [re.sub(r"^(\| U-\d{4} \|) - \|", r"\1  |", r) for r in rows]
blocks = re.findall(r"```\n(.*?)\n```", merge.split("### 1.2", 1)[1], re.S)
readme_par, claude_par = blocks[0], blocks[1]
assert readme_par.startswith("port/test/framegate.py (roadmap S0.2)") and claude_par.startswith("Roadmap S0, baseline"), blocks[:2]


def rd(p):
    return (root / p).read_bytes().decode("utf-8")


def wr(p, s):
    (root / p).write_bytes(s.encode("utf-8"))
    print("edited", p)


def crlf(s):
    return s.replace("\n", CRLF)


# 1.1 UNCERTAINTIES.md: the 11 rows after the last table row (U-1590 in HEAD 32c11a36)
t = rd("UNCERTAINTIES.md")
if "| U-1100 |" in t:
    sys.exit("UNCERTAINTIES.md: U-1100 already present")
assert CRLF in t and t.endswith(CRLF)
lines = t.split(CRLF)
last = max(i for i, l in enumerate(lines) if l.startswith("| U-"))
assert lines[last].startswith("| U-1590 |"), lines[last][:40]
lines[last + 1:last + 1] = rows
wr("UNCERTAINTIES.md", CRLF.join(lines))

# 1.2 port/test/README.md: the file has no phase 5 part at HEAD (it ends with "Phase 3.0 additions"), so the paragraph goes at
#     the end of the file after a blank line
t = rd("port/test/README.md")
if "port/test/framegate.py" in t:
    sys.exit("README.md: paragraph already present")
assert t.endswith(CRLF) and "Phase 5" not in t and "phase 5" not in t
wr("port/test/README.md", t + CRLF + crlf(readme_par) + CRLF)

# 1.3 CLAUDE.md: after the "Phase 5 step 2, the location screen" paragraph (the next one is "Phase 7 defect decisions")
t = rd("CLAUDE.md")
if "Roadmap S0, baseline and frame census" in t:
    sys.exit("CLAUDE.md: paragraph already present")
nxt = CRLF + "Phase 7 defect decisions, section S6 of the roadmap orchestrator"
if t.count(nxt) != 1 or t.count(CRLF + "Phase 5 step 2, the location screen") != 1:
    sys.exit("CLAUDE.md: anchors not unique")
assert t.index(CRLF + "Phase 5 step 2, the location screen") < t.index(nxt)
wr("CLAUDE.md", t.replace(nxt, CRLF + crlf(claude_par) + nxt, 1))
