"""Phase 2.2: the address map's pointer column and the generated GameState (re/tools/model_state_gen.py).

- the pointer column flags the 195 type-pointer objects phase 2.1 measured and every U-0604 dword;
- port/include/gamestate.h, gamestate_meta.inc and gamestate_addresses.inc are current, and two generations are identical;
- gamestate.h holds no original address;
- the layout covers the data section exactly, and every object's offset equals its addresses.tsv va minus the section base;
- gamestate.h compiles with MSVC as C++17 for x86 and x64 (/W4 /WX), with every static_assert, and a probe that asserts
  offsetof(GameState, s) == va - base for every SG_ADDRESS / SG_OBJECT pair; a wrong offset assert fails.
"""
import csv
import pathlib
import subprocess
import sys
import tempfile

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_merge as mm  # noqa: E402
import model_state_gen as gen  # noqa: E402

pytestmark = pytest.mark.static

VS = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools")
VCVARS = VS / "VC" / "Auxiliary" / "Build"
MODEL_INC = ROOT / "re" / "model" / "include"
FLAG_GAPS = ROOT / "re" / "analysis" / "data" / "premise_check_flag_gaps.tsv"


def _rows():
    with (ROOT / "re" / "model" / "addresses.tsv").open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def test_pointer_column_matches_phase21():
    rows = [r for r in _rows() if r["relation"] == "top" and gen.DATA_VA <= int(r["va"], 16) < gen.DATA_VA + gen.DATA_N]
    typed = [r for r in rows if r["pointer"] == "type"]
    assert len(typed) == 195 and sum(int(r["size"], 16) for r in typed) == 2_863_036   # premise_check.md section 5
    by = {r["symbol"]: r for r in rows}
    with FLAG_GAPS.open(newline="", encoding="utf-8") as f:
        gaps = list(csv.DictReader(f, delimiter="\t"))
    assert len(gaps) == 12
    for g in gaps:
        r = by[g["symbol"]]
        assert r["pointer"].startswith("words:"), g["symbol"]
        assert int(g["va"], 16) - int(r["va"], 16) in [int(w, 16) for w in r["pointer"][6:].split(",")]
    assert sum(len(r["pointer"][6:].split(",")) for r in rows if r["pointer"].startswith("words:")) == 12


def test_merge_writes_the_current_map():
    rows, _ = mm.merged_rows()
    cur = {r["symbol"]: r["pointer"] for r in _rows()}
    assert {r["symbol"]: r["pointer"] for r in rows} == cur


def test_generated_files_are_current_and_deterministic():
    a, _, _ = gen.build()
    b, _, _ = gen.build()
    assert a == b
    for p, t in a.items():
        assert p.read_text(encoding="utf-8") == t, f"{p} is stale: run re/tools/model_state_gen.py"


def test_header_has_no_original_address():
    assert not mm.ADDR_RE.findall(gen.OUT_H.read_text(encoding="utf-8"))
    assert not mm.ADDR_RE.findall(gen.OUT_META.read_text(encoding="utf-8"))


def test_layout_covers_the_section_and_matches_addresses():
    objs, unmatched = gen.objects()
    assert not unmatched
    pos = 0
    for o in objs:
        assert o["offset"] == pos
        pos += o["size"]
    assert pos == gen.DATA_N
    va = {r["symbol"]: int(r["va"], 16) for r in _rows() if r["relation"] == "top"}
    for o in objs:
        if o["cls"] != "unmodeled":
            assert o["offset"] == va[o["symbol"]] - gen.DATA_VA
    # the two hidden-state objects phase 2.1 found causal are compared state
    by = {o["symbol"]: o for o in objs}
    assert by["g_rng"]["cls"] == "sim" and by["g_lastTipDate"]["cls"] == "sim"
    assert by["g_rootWindow"]["words"] == [0x11c, 0x120, 0x124, 0x128, 0x14c, 0x278, 0x378, 0x518]


PROBE = r"""#include "gamestate.h"
#define SG_ADDRESS(s, va) static_assert(offsetof(portsim::GameState, s) == (va) - 0x{base:08x}u, #s);
#include "gamestate_addresses.inc"
#define SG_OBJECT(s, off, size, cls, ptr, hid, saved) static_assert(offsetof(portsim::GameState, s) == (off) && \
    sizeof(portsim::GameState::s) == (size), #s);
#define SG_GAP(off, size) static_assert((off) + (size) <= sizeof(portsim::GameState), "gap");
#include "gamestate_meta.inc"
int portsim_gamestate_probe() {{ return (int)sizeof(portsim::GameState); }}
"""
BAD = '#include "gamestate.h"\nstatic_assert(offsetof(portsim::GameState, g_rng) == 0x10, "p22_negative_control");\n'


def _cl(arch, src, tmp):
    bat = pathlib.Path(tmp) / "b.bat"
    vc = VCVARS / ("vcvars32.bat" if arch == "x86" else "vcvars64.bat")
    bat.write_text(f'@echo off\nset CL=\nset _CL_=\ncall "{vc}" >nul 2>nul\ncd /d "{tmp}"\n'
                   f'cl /nologo /W4 /WX /TP /std:c++17 /permissive- /I"{gen.PORT_INC}" /I"{MODEL_INC}" /c "{src}" '
                   f'/Fo"{tmp}\\o.obj"\n', encoding="ascii")
    return subprocess.run(["cmd", "/c", str(bat)], capture_output=True, text=True, errors="replace", timeout=600)


@pytest.mark.skipif(not (VCVARS / "vcvars32.bat").exists(), reason="Visual Studio 2022 Build Tools (cl) not installed")
@pytest.mark.parametrize("arch", ["x86", "x64"])
def test_gamestate_compiles_with_msvc(arch):
    with tempfile.TemporaryDirectory() as tmp:
        src = pathlib.Path(tmp) / "t.cpp"
        src.write_text(PROBE.format(base=gen.DATA_VA), encoding="ascii")
        r = _cl(arch, src, tmp)
        assert r.returncode == 0, r.stdout[-3000:] + r.stderr[-2000:]


@pytest.mark.skipif(not (VCVARS / "vcvars32.bat").exists(), reason="Visual Studio 2022 Build Tools (cl) not installed")
def test_wrong_offset_assert_fails_with_msvc():
    with tempfile.TemporaryDirectory() as tmp:
        src = pathlib.Path(tmp) / "bad.cpp"
        src.write_text(BAD, encoding="ascii")
        r = _cl("x86", src, tmp)
        assert r.returncode != 0 and "p22_negative_control" in r.stdout
