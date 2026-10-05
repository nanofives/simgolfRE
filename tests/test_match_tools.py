"""Matching-plan tooling (re/match/PLAN.md, phase 0): inventory, candidate queue, annotation addresses, permuter
rewrites and the C compile mode. Static layer; the compile check is skipped without the VC6 toolchain."""
import json
import re
import subprocess
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402
import match_permute  # noqa: E402
import match_queue  # noqa: E402

pytestmark = pytest.mark.static
VC6 = (ROOT / "tools" / "vc6" / "vc98" / "bin" / "cl.exe").exists()


def test_inventory_covers_the_four_modules():
    r = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "match_inventory.py"), "--json"],
                       capture_output=True, text=True, check=True)
    inv = json.loads(r.stdout)
    assert set(inv) == {"golf_clean.exe", "Terrain.dll", "jgld.dll", "sound.dll"}
    for mod, d in inv.items():
        game = sum(b["game"] for b in d["buckets"].values())
        assert 0 < game < d["functions"], mod


def test_every_match_annotation_is_a_function_entry():
    """A typo in a // MATCH: address would compare the wrong bytes; every address must be a function entry in
    its module's list (re/functions_ghidra*.tsv)."""
    entries = {m: {int(l.split("\t")[0], 16) for l in (ROOT / "re" / t).read_text().splitlines()[1:]}
               for m, t in match.TSV.items()}
    for m in entries:
        entries[m] |= set(match.boundary_overrides(m))     # entries Ghidra merged into a neighbour
    bad = []
    for f in sorted((ROOT / "re" / "match").rglob("*.cpp")):
        for mod, a in re.findall(r"// MATCH: (\S+) 0x([0-9a-fA-F]+)", f.read_text()):
            if match.tracker_key(mod, int(a, 16)) not in entries[mod]:
                bad.append(f"{f.name}: {mod} 0x{a}")
    assert not bad, bad


def test_queue_skips_matched_and_non_game_functions():
    q = match_queue.queue("golf_clean.exe")
    keys = {c[2] for c in q}
    done = match_queue.annotated("golf_clean.exe")
    assert keys and not keys & done
    assert 0x0042dc00 not in keys                          # appendNumber (re/match/golf_small32.cpp)
    names = {c[3] for c in q}
    assert not any(n.startswith(match_queue.FUNCLETS) for n in names)


def test_permuter_rewrites():
    assert "x <= 4" in list(match_permute.rw_cmp("if (x < 5) y();"))[0]
    assert "x >= 6" in list(match_permute.rw_cmp("if (x > 5) y();"))[0]
    swapped = list(match_permute.rw_ifelse("{\n    if (a) {\n        f();\n    } else {\n        g();\n    }\n}"))
    assert swapped and swapped[0].index("g();") < swapped[0].index("f();") and "!(a)" in swapped[0]
    body = "{\n    a = 1;\n    b = 2;\n}"
    assert "b = 2;\n    a = 1;" in list(match_permute.rw_swap(body))[0]


@pytest.mark.skipif(not VC6, reason="VC6 toolchain not extracted to tools/vc6")
def test_lang_c_compiles_as_c(tmp_path):
    """`// LANG c` gives undecorated C symbols; appendCents matches as C as well as C++ (source language not
    determined by that function)."""
    src = (ROOT / "re" / "match" / "golf_small33.cpp").read_text()
    body = src[src.index("extern char g_text[];"):src.index("void appendNumber(int n);")]
    c = ("// LANG c\n// FLAGS golf_clean.exe: /O2\n#include <stdlib.h>\n#include <string.h>\n"
         + body.replace("?appendCents@@YAXH@Z", "_appendCents").replace("    int len = strlen(g_text);\n", "")
         .replace("    if (n < 0) {\n", "    if (n < 0) {\n        int len = strlen(g_text);\n"))
    f = tmp_path / "c_mode.cpp"
    f.write_text(c)
    res = match.compare(f, "", False, quiet=True)
    assert len(res) == 1 and res[0][4] == 1.0, res[0][4] if res else None
