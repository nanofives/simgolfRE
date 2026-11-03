"""I25: CLAUDE.md edits of log/roadmap/S11/MERGE_w2.md C1, C2 and E5 on the repo's CLAUDE.md. Orchestrator, 2026-11-03."""
from __future__ import annotations

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
P = ROOT / "CLAUDE.md"

E5_OLD = ("whether that counts under the round-36 inline-asm rule is Mariano's (U-2000; CSVs parked in\n"
          "log/roadmap/S11/1_ev/, not log/diff)")
E5_NEW = ("which G26 (Mariano, 2026-11-02) ruled is not C4 match evidence: they need scenario evidence\n"
          "(U-2000 resolved; CSVs parked in log/roadmap/S11/1_ev/, not log/diff)")

OLD_GATES = ("Gates on a copy of I22: builds x64 /\n"
             "Win32 rc 0, port test_static 6, test_model_header 14, segments 205 / 205 x64 and Win32 with 0 differing cells (digests included),\n"
             "boot equal to I22's 70 rows on both backends.")
NEW_GATES = ("Merged by I25 on cdeb2609 (log/roadmap/merges/I25.md, fast gates by\n"
             "G30): builds x64 / Win32 rc 0, static 820 passed (tests/test_port_scope.py and port/test/audio_stream.py read re/port/scope.tsv's\n"
             "old names through port/test/renames.py), segments 205 / 205 x64 and Win32 with 0 digests changed, boot 67 / 2 / 2 on both, app\n"
             "replay 3 / 3, gfx.check 0 duplicates, the 8 golden files, test_tick 250, test_golfers 60 and the audio files.")

C2 = """S11.5 (2026-11-01, log/roadmap/S11/5_notes.md, U-2020 resolved, U-2040..U-2042): re/tools/endstate.py's dead checks refuse a function
containing the module's AddressOfEntryPoint or a TLS callback (`not the PE entry point or a TLS callback`; none of the four modules has a
TLS directory) and count an export inside a function's range, not only at its entry; before, the four CRT startups passed as dead (S11.3
left them out of its marks by hand). tests/test_endstate_entry.py: the entry points refused, every hooks.csv dead / trivial row still
passing, stubbed TLS / export cases, controls RED."""


def c1() -> str:
    t = (ROOT / "log/roadmap/S11/4_merge.md").read_text(encoding="utf-8")
    sec = t[t.index("## 5. CLAUDE.md paragraph"):]
    body = sec.split("```", 2)[1].strip("\n")
    assert OLD_GATES in body, "4_merge.md section 5 gate sentence changed"
    return body.replace(OLD_GATES, NEW_GATES)


def main() -> None:
    raw = P.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in raw else "\n"
    t = raw.replace(nl, "\n")
    assert "Roadmap S11.4, the S10 renames" not in t, "already applied"
    assert t.count(E5_OLD) == 1, "E5 anchor"
    t = t.replace(E5_OLD, E5_NEW)
    i = t.index("Section S11, RE upkeep wave 1")
    j = t.index("\nRoadmap S9.1, the network survey", i)
    t = t[:j] + "\n" + c1() + "\n" + C2 + t[j:]
    P.write_bytes(t.replace("\n", nl).encode("utf-8"))
    print("CLAUDE.md: E5 replaced, S11.4 + S11.5 paragraphs added")


if __name__ == "__main__":
    main()
