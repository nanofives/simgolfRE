"""I18: apply S9's shared-file edits (log/roadmap/S9/MERGE.md section 2, minus item 4 = the P rename, and item 6 =
sections.tsv, which the orchestrator owns) to a tree. Usage: ~/bin/py -3.12 apply_shared.py <tree root>. Each edit refuses
to run twice or when its anchor is not found exactly once."""
import os, pathlib, re, sys

T = pathlib.Path(sys.argv[1])
S9 = pathlib.Path(__file__).resolve().parents[2] / "S9"
RESOLVED = {"U-1760", "U-1761"}


def edit(rel, old, new, count=1):
    p = T / rel
    s = p.read_text(encoding="utf-8")
    assert s.count(old) == count, (rel, old[:60], s.count(old))
    p.write_bytes(s.replace(old, new).encode("utf-8"))
    print("edit", rel)


def append(rel, text, marker):
    p = T / rel
    s = p.read_text(encoding="utf-8")
    assert marker not in s, (rel, "already applied")
    p.write_bytes((s.rstrip("\n") + "\n" + text).encode("utf-8"))
    print("append", rel)


# 1. UNCERTAINTIES.md rows U-1750..U-1754, U-1760..U-1763, U-1771..U-1774
rows = []
for f in ("1_uncertain.tsv", "2_uncertain.tsv", "3_uncertain.tsv"):
    for line in (S9 / f).read_text(encoding="utf-8").splitlines()[1:]:
        c = line.split("\t")
        assert len(c) == 6 and "|" not in line, (f, line[:40])
        addr = "" if c[1] == "-" else c[1]
        assert addr == "" or re.fullmatch(r"0x[0-9a-f]+", addr), addr
        st = "resolved: NET_MODEL.md section 0" if c[0] in RESOLVED else c[5]
        rows.append(f"| {c[0]} | {addr} | {c[2]} | {c[3]} | {c[4]} | {st} |\n")
append("UNCERTAINTIES.md", "".join(rows), "| U-1750 |")
# 2. re/c3_hold.tsv reasons
edit("re/c3_hold.tsv", "the COM object [0x008400b0], wsock32,",
     "the IDirectPlay2A object [0x008400b0] (Send / EnumGroupPlayers slots; the exe imports no socket DLL),")
edit("re/c3_hold.tsv", "same frame, dialog, COM and wsock32 layer", "same frame, dialog and IDirectPlay2A Send layer")
# 3. re/model/DECISIONS.md
append("re/model/DECISIONS.md", "\n## 2026-10-29, correction reported by S9.1, to apply at the start of the next model phase\n"
       "- `g_ui_83ff9c` 0x0083ff9c (re/model/addresses.tsv, int32_t[4], class ui) is GetPlayerName's DPNAME out-buffer: netPoll "
       "passes it with a size word of 0x100 (0x0049b8ad / 0x0049b8bb) and reads +0xc = lpszLongNameA (0x0049b8d6). Proposed: "
       "`g_netPlayerName` uint8_t[0x100], ending at 0x0084009c (below g_net8400ac). Evidence: log/roadmap/S9/1_merge.md section 3, "
       "re/analysis/net/NET_MODEL.md.\n", "g_netPlayerName")
# 5. re/roadmap/S9_PLAN.md outcome
append("re/roadmap/S9_PLAN.md", "\n## Outcome (G14, 2026-10-29)\n\nThe original has no network play (re/analysis/net/NET_MODEL.md "
       "section 0). The survey is merged, the dormant layer is ported as specification (S9.3, module net), and the recording, "
       "replay and interop gates are dropped.\n", "## Outcome (G14")
# 7. modules.inc + touch registry.cpp
append("port/src/sim/modules.inc", "SG_MODULE(net)\n", "SG_MODULE(net)")
os.utime(T / "port/src/registry.cpp")
# 8. CLAUDE.md paragraph after the S10 wave 2 paragraph (MERGE.md section 3)
para = (S9 / "MERGE.md").read_text(encoding="utf-8").split("## 3. CLAUDE.md paragraph\n", 1)[1].split("\n## 4.", 1)[0].strip("\n")
c = (T / "CLAUDE.md").read_text(encoding="utf-8")
assert "Roadmap S9.1, the network survey" not in c
i = c.index("Wave 2: the SimGolf demo's sound.dll")
j = c.index("\n### ", i)
(T / "CLAUDE.md").write_bytes((c[:j] + para + "\n" + c[j:]).encode("utf-8"))
print("insert CLAUDE.md")
