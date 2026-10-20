"""Phase 3.8 parent merge: append the streams' U-rows (log/p38/38*_uncertain.tsv) to UNCERTAINTIES.md and apply status updates.

Usage: ~/bin/py -3.12 log/p38/merge_uncertain.py [--dry-run]
Refuses duplicate ids, rows with a column count other than 6, and status updates whose id is missing.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
UNC = ROOT / "UNCERTAINTIES.md"

# id -> (new status, text appended to the path-to-resolution column)
UPDATES = {
    "U-0944": ("resolved", "phase 3.8 38a: module gditext (host GDI TextMetrics provider) equals every recorded printable width (11,459 / 11,459)"),
    "U-0947": ("resolved", "phase 3.8 38a: SegmentRunner attaches the gditext provider by default (SG_TEXT_METRICS=none disables it)"),
    "U-0949": ("resolved", "phase 3.8 38e: InputSource::clock, the trace's next clock record at a call site; showHoleStatsScreen ported over it, "
                           "championship/02 and cov_a_keys2/02 PASS with all 457 / 385 clock records at its five sites served in order"),
    "U-0865": ("resolved", "phase 3.8 38d: skillPointDialog ported in module dialogs (log/p38/38d_notes.md section 2)"),
    "U-0931": ("resolved", "phase 3.8 38d: showCharacterIntro registered (module dialogs); ui_tour/02 and ui_tour_v/02 PASS"),
    "U-0950": ("resolved", "phase 3.8 38d: drawScrollText's layout ported over TextMetrics (module dialogs), oracle GREEN with scripted widths; no segment reaches it"),
    "U-0864": ("resolved", "phase 3.8 38b: GameFiles list / write overlay; listMatchingFiles and loadGolferRoster ported (module loaders)"),
    "U-0654": ("partly resolved", "phase 3.8 38i (log/p38/38i_notes.md section 4): character keys arrive as rootWin_onChar(vk) then "
                                  "rootWin_onKey(ch, vk) once or twice, non-character keys as rootWin_onKey(0, vk) alone "
                                  "(portsim/sim/semantic.h); a WM_CHAR with no WM_KEYDOWN reaches rootWin_onKey with the code of the most "
                                  "recent key-down (156 of 156); not mapped: other keys' pairs, modifier combinations, numpad 0x60..0x69"),
    "U-0996": ("resolved", "phase 3.8 38h: showCharacterEditor ported (module chareditor); the segment opens it with key 0x3f, not through clickGolferPanel"),
}
NOTES = {
    "U-0905": "phase 3.8 38d: showBulletinBoard is registered (module dialogs); with photo index 99 the board has no award dialog, only its modal wait",
    "U-0650": "resolved for course_season by U-1013 (phase 3.8 38f)",
}


def cell(s: str) -> str:
    return s.replace("|", "\\|").strip()


def main() -> int:
    dry = "--dry-run" in sys.argv
    text = UNC.read_text(encoding="utf-8")
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl)
    have = {m.group(1) for ln in lines if (m := re.match(r"\| (U-\d{4}) \|", ln))}
    rows = []
    for f in sorted((ROOT / "log" / "p38").glob("38*_uncertain.tsv")):
        for i, ln in enumerate(f.read_text(encoding="utf-8").splitlines()):
            if not ln.strip() or ln.lower().startswith("id\t"):
                continue
            c = ln.split("\t")
            if len(c) != 6:
                sys.exit(f"{f.name}:{i + 1}: {len(c)} columns")
            if c[0] in have or c[0] in {r[0] for r in rows}:
                sys.exit(f"{f.name}:{i + 1}: duplicate id {c[0]}")
            rows.append(c)
    rows.sort(key=lambda c: c[0])
    # status updates and notes on existing rows (and on rows appended now)
    out = [ln for ln in lines]
    while out and out[-1] == "":
        out.pop()
    out += ["| " + " | ".join(cell(x) for x in c) + " |" for c in rows]
    done = set()
    for k, ln in enumerate(out):
        m = re.match(r"\| (U-\d{4}) \|", ln)
        if not m or (m.group(1) not in UPDATES and m.group(1) not in NOTES):
            continue
        uid = m.group(1)
        parts = ln.split(" | ")
        # columns: '| ID', addr, type, description, path, 'status |'
        if len(parts) < 6:
            sys.exit(f"{uid}: unexpected row shape")
        path_i, status_i = len(parts) - 2, len(parts) - 1
        if uid in UPDATES:
            st, add = UPDATES[uid]
            parts[path_i] = parts[path_i] + "; " + cell(add)
            parts[status_i] = st + " |"
        else:
            parts[path_i] = parts[path_i] + "; " + cell(NOTES[uid])
        out[k] = " | ".join(parts)
        done.add(uid)
    missing = (set(UPDATES) | set(NOTES)) - done
    if missing:
        sys.exit(f"ids not found: {sorted(missing)}")
    print(f"appended {len(rows)} rows ({rows[0][0]}..{rows[-1][0]}), updated {len(done)}")
    if not dry:
        UNC.write_text(nl.join(out) + nl, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
