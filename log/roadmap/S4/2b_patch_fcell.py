"""S4.2b: S4.5's and S4.7's `fcell(ctx, n)` (16-byte cells in the image's tail padding / .reloc tail, U-1440 / U-1461) replaced by
the core's stack: every function that used cells reserves ONE SndFrame holding them (cell size 0x10 in audio_seq, 0x100 in
audio_sound, where a cell is a 0x100-byte buffer) and `fcell(ctx, n)` becomes `fr_cells.at(...)`. The cells' order inside the
frame is the order of first use in the function; they are not at the original's offsets (as the image cells were not; no key
compares their addresses: U-1418). Nested port callees now enter below the frame instead of every function owning a fixed cell.

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_fcell.py [--check]
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SIM = ROOT / "port" / "src" / "sim"
FUNC_START = re.compile(r"^[A-Za-z_][\w:<>,\s\*&]*\b(\w+)\(([^;]*)\)\s*\{\s*$")


def patch(path: pathlib.Path, cell_size: int, helper_re: str) -> str:
    lines = path.read_text(encoding="utf-8").split("\n")
    # drop the helper definition
    out = [ln for ln in lines if not re.match(helper_re, ln)]
    assert len(out) == len(lines) - 1, "helper definition not found once"
    lines = out
    starts = [i for i, ln in enumerate(lines) if FUNC_START.match(ln)]
    uses: dict[int, list[int]] = {}
    for i, ln in enumerate(lines):
        for m in re.finditer(r"fcell\(ctx, (\d+)\)", ln):
            st = max(s for s in starts if s < i)
            cells = uses.setdefault(st, [])
            if int(m.group(1)) not in cells:
                cells.append(int(m.group(1)))
    for st in sorted(uses, reverse=True):
        cells = uses[st]
        nxt = min([s for s in starts if s > st] + [len(lines)])
        for i in range(st, nxt):
            lines[i] = re.sub(r"fcell\(ctx, (\d+)\)",
                              lambda m: f"fr_cells.at(0x{cell_size * (cells.index(int(m.group(1))) + 1):x})", lines[i])
        name = FUNC_START.match(lines[st]).group(1)
        lines.insert(st + 1, f"  SndFrame fr_cells(ctx, 0x{cell_size * len(cells):x});  // S4.2b: the cells {cells} of the "
                             f"original's locals passed by address (U-1418)")
        print(f"{path.name}: {name}: cells {cells}")
    t = "\n".join(lines)
    assert "fcell(" not in t, [ln for ln in t.split("\n") if "fcell(" in ln][:3]
    return t


res = {
    SIM / "audio_seq.cpp": patch(SIM / "audio_seq.cpp", 0x10, r"^u32 fcell\(Ctx &c, int cell\)"),
    SIM / "audio_sound.cpp": patch(SIM / "audio_sound.cpp", 0x100, r"^u32 fcell\(Ctx &c, int cell\)"),
}
if "--check" not in sys.argv:
    for p, t in res.items():
        p.write_text(t, encoding="utf-8")
    print("written")
