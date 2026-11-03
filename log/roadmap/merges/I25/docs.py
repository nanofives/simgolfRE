"""I25 documents (log/roadmap/S11/MERGE_w2.md A4, B5, E1, E2) applied to the mirror port/build/I25. Orchestrator, 2026-11-03.

    ~/bin/py -3.12 log/roadmap/merges/I25/docs.py      # refuses a second apply
"""
from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[4]
M = ROOT / "port" / "build" / "I25"
S11 = ROOT / "log" / "roadmap" / "S11"

OLD_HOLD = "held at C3 pending decision G26 (U-2000, S11.1 2026-11-01)"
NEW_HOLD = ("held at C3: G26 (2026-11-02): an all-asm body inside a compiled frame is not C4 match evidence; "
            "needs scenario evidence (U-2000, S11.1 2026-11-01)")
U2000_ADD = (" Resolved by G26 (Mariano, 2026-11-02): an all-asm body inside a compiled frame is not C4 match evidence; "
             "fillSpanDown / fillSpanUp stay C3 held and need scenario evidence.")
U2020_ADD = (' Resolved S11.5: endstate.py dead check "not the PE entry point or a TLS callback" '
             "(tests/test_endstate_entry.py).")


def rd(p: pathlib.Path) -> str:
    return p.read_bytes().decode("utf-8")


def wr(p: pathlib.Path, t: str) -> None:
    p.write_bytes(t.encode("utf-8"))


def resolve(lines: list[str], uid: str, add: str) -> None:
    k = next(i for i, l in enumerate(lines) if l.startswith(f"| {uid} |"))
    cells = lines[k].rstrip().split(" | ")
    assert cells[-1].rstrip(" |") == "open", (uid, cells[-1])
    cells[-2] = cells[-2] + add
    cells[-1] = "resolved |"
    lines[k] = " | ".join(cells)


def main() -> None:
    hold = M / "re" / "c3_hold.tsv"
    t = rd(hold)
    if t.count(NEW_HOLD) == 2 and OLD_HOLD not in t:
        print("c3_hold already applied")
    else:
        assert t.count(OLD_HOLD) == 2, "c3_hold: expected the two fillSpan rows once each"
        wr(hold, t.replace(OLD_HOLD, NEW_HOLD))

    up = M / "UNCERTAINTIES.md"
    t = rd(up)
    nl = "\r\n" if "\r\n" in t else "\n"
    lines = t.split(nl)
    resolve(lines, "U-2000", U2000_ADD)
    resolve(lines, "U-2020", U2020_ADD)
    have = {m.group(1) for l in lines for m in [re.match(r"\| (U-\d{4}) \|", l)] if m}
    rows = []
    for name in ("4_uncertain.tsv", "5_uncertain.tsv"):
        for ln in (S11 / name).read_text(encoding="utf-8").splitlines()[1:]:
            if not ln.strip():
                continue
            cells = [c.replace("|", "\\|") for c in ln.split("\t")]
            if len(cells) == 5:  # 5_uncertain.tsv: id addr module status text -> id addr kind description needed status
                uid, addr, _module, status, text = cells
                cells = [uid, addr, "tooling", text, "", status]
            assert len(cells) == 6 and cells[0] not in have, cells[0]
            rows.append("| " + " | ".join(cells) + " |")
    last = max(i for i, l in enumerate(lines) if re.match(r"\| U-\d{4} \|", l))
    lines[last + 1:last + 1] = rows
    wr(up, nl.join(lines))
    print("c3_hold 2 rows; U-2000, U-2020 resolved; added", [r.split(" | ")[0][2:] for r in rows])


if __name__ == "__main__":
    main()
