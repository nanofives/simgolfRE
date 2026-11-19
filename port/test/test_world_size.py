"""P38A: the world size goes through portsim/sim/world.h (re/roadmap/P38_LARGE_MAPS.md phase A).

Three rules over the simulation sources (port/src, port/app; comments stripped):
  1. a grid's base comes from world.h (grid_va / grid_arr / grid_ref / grid_host / tile_grid / vertex_grid, GV() in the tick,
     command and event hooks), never GS(<grid>), state_va / state_arr / state_obj / state_grid of the member, offsetof or
     direct member indexing;
  2. a code line naming a grid carries no world-size literal (50 / 0x32, 51 / 0x33, 49 / 0x31, 2500 / 0x9c4, 2601 / 0xa29,
     0x271, 0x4e2, 2550, 20000): strides and bounds are world_n / world_v / world_last / world_tiles / grid_dwords ... / WN / WV;
  3. no grid is addressed through ANOTHER symbol: GS(sym) + k landing in the N + 1 rows before a grid (Ghidra names a lea target
     after the object it falls in, so g_tileType - 50 reads g_courseTable+0x2de; the port wrote it that way until P38A).
A grid is every GameState array member of 2500, 2601 or 20000 entries (gamestate.h). The reviewed exceptions are rows of
port/test/world_size_allow.tsv (rule, file, a piece of the code line, the reason); a row that matches nothing fails too, so the
list stays exact."""
from __future__ import annotations

import csv
import pathlib
import re

import pytest

pytestmark = pytest.mark.static
ROOT = pathlib.Path(__file__).resolve().parents[2]
PORT = ROOT / "port"
ALLOW = PORT / "test" / "world_size_allow.tsv"

LITERAL = re.compile(r"(?<![\w.])(50|0x32|51|0x33|49|0x31|2500|0x9c4|2601|0xa29|0x271|0x4e2|2550|20000)[uUlL]*(?![\w.])", re.I)


def _grids() -> dict[str, int]:
    """grid member -> element size in bytes (gamestate.h)"""
    sizes = {"int8_t": 1, "uint8_t": 1, "int16_t": 2, "uint16_t": 2}
    out = {}
    for m in re.finditer(r"^\s*(u?int(?:8|16)_t)\s+(\w+)\[(2500|2601|20000)\];", (PORT / "include" / "gamestate.h").read_text(), re.M):
        out[m.group(2)] = sizes[m.group(1)]
    return out


def _objects() -> dict[str, tuple[int, int]]:
    text = (PORT / "include" / "gamestate_meta.inc").read_text()
    return {m.group(1): (int(m.group(2), 16), int(m.group(3), 16))
            for m in re.finditer(r"SG_OBJECT\((\w+), (0x[0-9a-f]+), (0x[0-9a-f]+),", text)}


def _sources():
    files = [p for d in ("src", "app") for p in (PORT / d).rglob("*") if p.suffix in (".cpp", ".h", ".inc")]
    return sorted(p for p in files if "build" not in p.relative_to(PORT).parts)


def _code(line: str) -> str:
    line = re.sub(r'"(?:\\.|[^"\\])*"', '""', line)  # string literals (defect descriptions) are not code
    return line.split("//", 1)[0]


def _allow():
    with open(ALLOW, encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f, delimiter="\t") if r["rule"] and not r["rule"].startswith("#")]
    return rows


def _findings():
    grids, objs = _grids(), _objects()
    assert len(grids) == 20 and "g_tileType" in grids and "g_cornerLevel" in grids, grids
    g_alt = "|".join(sorted(grids, key=len, reverse=True))
    base_bad = re.compile(
        rf"\bGS\(({g_alt})\)|\bstate_(?:va|arr|obj|grid)\(\s*\w+\s*,\s*&(?:portsim::)?GameState::({g_alt})\b"
        rf"|offsetof\(\s*(?:portsim::)?GameState\s*,\s*({g_alt})\b|GSO\(({g_alt})\b|(?:\.|->)({g_alt})\s*[\[+;),]")
    names = re.compile(rf"\b({g_alt})\b")
    disguised = re.compile(r"\b(?:GS|GV)\((\w+)\)\s*\)?\s*([+-])\s*(0x[0-9a-fA-F]+|\d+)\b"
                           r"|\bstate_(?:va|arr)\(\s*\w+\s*,\s*&(?:portsim::)?GameState::(\w+)\)\s*\)?\s*([+-])\s*(0x[0-9a-fA-F]+|\d+)\b")
    lows = [(objs[g][0], objs[g][0] + objs[g][1], g, e) for g, e in grids.items()]
    out = []
    for p in _sources():
        rel = p.relative_to(PORT).as_posix()
        for i, raw in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            code = _code(raw)
            if not code.strip():
                continue
            if base_bad.search(code):
                out.append(("base", rel, i, raw.strip()))
            if names.search(code) and LITERAL.search(code):
                out.append(("literal", rel, i, raw.strip()))
            for m in disguised.finditer(code):
                sym, sign, k = (m.group(1), m.group(2), m.group(3)) if m.group(1) else (m.group(4), m.group(5), m.group(6))
                if sym in grids or sym not in objs:
                    continue
                at = objs[sym][0] + (int(k, 0) if sign == "+" else -int(k, 0))
                for lo, hi, g, e in lows:
                    if lo - 2 * 51 * e <= at < lo or hi <= at < hi + e:   # the 2 rows before a grid, or one entry past its end
                        out.append(("disguised", rel, i, raw.strip()))
    return out


def test_world_size_goes_through_world_h():
    allow = _allow()
    used = [0] * len(allow)
    bad = []
    for rule, rel, i, line in _findings():
        hit = [k for k, r in enumerate(allow) if r["rule"] == rule and r["file"] == rel and r["match"] in line]
        for k in hit:
            used[k] += 1
        if not hit:
            bad.append(f"{rule} {rel}:{i}: {line[:160]}")
    stale = [f"{r['rule']} {r['file']} {r['match']!r}" for r, n in zip(allow, used) if n == 0]
    assert not bad, "world size outside world.h (P38A; allow only reviewed non-world uses in port/test/world_size_allow.tsv):\n" + \
        "\n".join(bad)
    assert not stale, "allowlist rows that match nothing (remove them): " + "; ".join(stale)


def test_world_size_detector_sees_the_old_forms():
    """Negative control of the checker itself: the forms P38A removed are found."""
    import types
    samples = {
        "base": "if (G8(GS(g_tileType) + idx) == 0x11) return;",
        "literal": "const int32_t cell = add32(mul32(x, 50), y); (grid_arr(ctx, &GameState::g_tileType) + cell).set(0);",
        "disguised": "if (G8((GS(g_courseTable) + 734) + edi) == 0x11) return;",
    }
    grids, objs = _grids(), _objects()
    g_alt = "|".join(sorted(grids, key=len, reverse=True))
    assert re.search(rf"\bGS\(({g_alt})\)", samples["base"])
    assert re.search(rf"\b({g_alt})\b", samples["literal"]) and LITERAL.search(samples["literal"])
    at = objs["g_courseTable"][0] + 734
    assert objs["g_tileType"][0] - 2 * 51 <= at < objs["g_tileType"][0]
    del types
