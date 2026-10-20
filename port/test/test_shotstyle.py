"""Phase 3.8 stream 38b: shotStyleButtonCode (golf_clean.exe 0x0040d760, port/src/sim/shotstyle.cpp).

No corpus vector exists for it (re/port/vectors_manifest.tsv), so the evidence is the EMULATOR ORACLE: the original runs
under Unicorn (port/test/cmdemu.py) for every argument below and the port must return the same eax. The oracle is opt-in
(SG_EMU_ORACLE=1); the default suite checks the pinned table (port/test/golden_38b.json, regenerated from the emulated
original with SG_EMU_ORACLE=update) and the negative controls against it.
"""
from __future__ import annotations

import json
import os
import pathlib

import pytest

from port.test import emu38b as E
from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_38b.json"
ENTRY = 0x0040D760

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

# every number around the table (3..7), the unsigned wrap below 3, and the extremes
ARGS = list(range(-8, 24)) + [0x7FFFFFFF, -0x80000000, 0x100, 0x10000003, -0x7FFFFFFD]


def port_results(lib, args=ARGS) -> list[int]:
    with lib.state() as st:
        return [st.call("shotStyleButtonCode", [a]).eax for a in args]


def emu_results(args=ARGS, cover=None) -> list[int]:
    from port.test import cmdemu as CE
    import vector_lib as VL  # re/tools on sys.path through cmdemu
    emu = CE.Emu()
    out = []
    for a in args:
        r = emu.run(ENTRY, [a], bytes(VL.DATA_SIZE), cover=cover)
        assert not r.err, (a, r.err)
        out.append(r.eax)
    return out


@emu_oracle
def test_oracle_equals_original(portlib):
    cover = {}
    want = emu_results(cover=cover)
    got = port_results(portlib)
    assert got == want, [(a, hex(g), hex(w)) for a, g, w in zip(ARGS, got, want) if g != w]
    # the `ja` out-of-range branch and the table jump are both reached
    from port.test import cmdemu as CE
    sites, both, missing = CE.coverage_summary(cover)
    assert sites == 1 and both == 1, missing
    if os.environ.get("SG_EMU_ORACLE") == "update":
        E.golden_update("shotStyleButtonCode", {"args": ARGS, "eax": want})


def test_golden_table(portlib):
    g = E.golden_load()["shotStyleButtonCode"]
    assert g["args"] == ARGS
    got = port_results(portlib)
    assert got == g["eax"], [(a, hex(x), hex(y)) for a, x, y in zip(ARGS, got, g["eax"]) if x != y]


def test_table_matches_the_instructions(portlib):
    """the five table slots of the jump table 3..7 and the default, read from the disassembly"""
    want = {3: 0x800043F0, 4: 0, 5: 0x8000421F, 6: 0x80007FF0, 7: 0x80007FFF, 2: 0, 8: 0, 0: 0, -1: 0}
    with portlib.state() as st:
        for a, w in want.items():
            assert st.call("shotStyleButtonCode", [a]).eax == w, a


@pytest.mark.parametrize("mode,what", [(1, "buttons 4 and 5 swapped"), (2, "range test idx > 3: button 7 maps to 0")])
def test_negctl_reads_red(portlib, mode, what):
    g = E.golden_load()["shotStyleButtonCode"]
    assert portlib.negctl("shotStyleButtonCode", mode) == 0
    try:
        got = port_results(portlib)
    finally:
        portlib.negctl_reset()
    assert got != g["eax"], f"mode {mode} ({what}) left every pinned result unchanged"
    # exactly the arguments the wrong body touches differ
    bad = {a for a, x, y in zip(ARGS, got, g["eax"]) if x != y}
    assert bad <= {4, 5, 7}, bad
