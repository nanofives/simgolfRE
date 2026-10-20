"""Pure-Python checks of the harness and the port sources (no DLL, no corpus)."""
from __future__ import annotations

import pathlib
import re

import pytest

from port.test import segments as S
from port.test import vectors as V

pytestmark = pytest.mark.static
ROOT = pathlib.Path(__file__).resolve().parents[2]
PORT = ROOT / "port"


def _port_sources():
    return [p for p in list((PORT / "src").rglob("*")) + list((PORT / "include" / "portsim").rglob("*"))
            if p.suffix in (".cpp", ".h", ".inc")] + [PORT / "include" / "portsim_c.h"]


def test_no_original_addresses_in_port_code():
    """README rule: no original address in port code. Literals inside the exe image (0x00400000-0x008fffff) or a
    DLL image (0x1000xxxx-0x101xxxxx) are refused; gamestate_addresses.inc is tooling only."""
    bad = []
    for p in _port_sources():
        text = p.read_text(encoding="utf-8")
        if "gamestate_addresses.inc" in text:
            bad.append(f"{p.name}: includes gamestate_addresses.inc")
        for m in re.finditer(r"0x([0-9a-fA-F]{6,8})", text):
            v = int(m.group(1), 16)
            if 0x00400000 <= v < 0x00900000 or 0x10001000 <= v < 0x10200000:
                bad.append(f"{p.relative_to(ROOT)}: {m.group(0)}")
    assert not bad, bad


def test_every_module_file_is_listed():
    listed = set(re.findall(r"^SG_MODULE\((\w+)\)", (PORT / "src" / "sim" / "modules.inc").read_text(), re.M))
    files = {p.stem for p in (PORT / "src" / "sim").glob("*.cpp")}
    assert files == listed, f"files {sorted(files)} != modules.inc {sorted(listed)}"
    for m in listed:
        src = (PORT / "src" / "sim" / f"{m}.cpp").read_text(encoding="utf-8")
        assert f"kModule_{m}" in src


def test_block_bases_change_page_offsets_and_do_not_overlap():
    sizes = [4, 0x1000, 0x2345, 1, 0]
    bases = V.block_bases(sizes)
    for i, (b, n) in enumerate(zip(bases, sizes)):
        assert b & 0xFFF == (0x10 * (i + 1)) & 0xFFF
        if i:
            assert b >= bases[i - 1] + max(sizes[i - 1], 1) + 0x1000 - 0xFFF
    assert bases[0] >= V.BLOCK_BASE


def test_vec_path_naming():
    assert V.vec_path("Random::range").name == "Random_range.vec"


def test_first_divergent_record():
    syms = ["a", "b", "c"]
    e = [(1, 0, [1, 2, 3]), (2, 1, [1, 2, 3]), (3, 2, [1, 2, 4])]
    assert S.first_divergent_record(e, list(e), syms) is None
    g = [e[0], e[1], (3, 2, [1, 9, 4])]
    assert S.first_divergent_record(e, g, syms) == {"record": 2, "T_expected": 3, "T_got": 3, "objects": ["b"]}
    g = [e[0], (5, 1, [1, 2, 3])]
    d = S.first_divergent_record(e, g, syms)
    assert d["record"] == 1 and d["T_got"] == 5
    assert S.first_divergent_record(e, e[:2], syms)["record"] == 2


def test_manifests_readable():
    fns = V.manifest_functions()
    assert len(fns) >= 300 and "Random::range" in fns
    rows = S.manifest_rows()
    # 190 from phase 2.4, plus course_season_6y's 10 (phase 3.8, stream 38f: the multi-year recording)
    assert sum(r["status"] == "accepted" for r in rows) == 200
