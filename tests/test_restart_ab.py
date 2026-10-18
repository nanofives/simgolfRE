"""re/frida/restart_ab.py and the opt-in IMAGE block of scenario_det.js (phase 2.1 premise check, 2026-10-18).

Static (no game):
  - scenario_ab's default agent config carries no `image` key, so a default run never enters the IMAGE block;
  - every IMAGE code path in scenario_det.js sits behind CFG.image or the imgWatch / imgInput state that only the block
    sets: the stub's extra `mov [D+0x48], esp`, the forced onDue of recompute, the site check in onDue, the input note
    in doAction and the poll reply handler;
  - the pure helpers of restart_ab.py: runs_of, pointer_words, the variant masks of build_image, corrupt, and the
    model's pointer flags (a struct with a `ptr` member is a pointer type, a `(TF_*)` comment is not a pointer).
The runtime proof that the default mode is unchanged is a replay of a pinned reference (re/analysis/data/premise_check.md).
"""
import json
import re
import subprocess
import sys

import numpy as np
import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

sys.path.insert(0, str(ROOT / "re" / "frida"))
import restart_ab as R  # noqa: E402  (import defines only; setup() is what sets the run environment)

JS = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text(encoding="utf-8")

PROBE = r"""
import json, pathlib, sys, tempfile
sys.argv = [sys.argv[0]]
sys.path.insert(0, r"%s")
import scenario_ab as s
d = pathlib.Path(tempfile.mkdtemp())
r = s.Run("x", "sandbox_basic", "replay", {}, [], [], out_dir=d)
first = r.agent_source().read_text().split("\n", 1)[0]
print(json.dumps(json.loads(first[len("const SCENARIO_CFG = "):-1])))
"""


def test_default_agent_config_has_no_image_key():
    out = subprocess.run([sys.executable, "-c", PROBE % (ROOT / "re" / "frida")], capture_output=True, text=True,
                         timeout=120, cwd=str(ROOT))
    assert out.returncode == 0, out.stderr
    cfg = json.loads(out.stdout.strip().splitlines()[-1])
    assert "image" not in cfg


def test_image_paths_are_gated_in_the_agent():
    # the stub's extra store is emitted only with cfg.image
    assert re.search(r"\.\.\.\(CFG\.image \? \[0x89, 0x25, \.\.\.d\(0x48\)\] : \[\]\)", JS)
    # recompute forces due 0 only while a watch is armed; onDue's image block runs only then
    assert "if (imgWatch) { setDue(0); return; }" in JS
    assert re.search(r"try \{\s*if \(imgWatch\) \{", JS)
    # imgWatch / imgInput start null and are set only by the IMAGE block
    assert "let imgWatch = null;" in JS and "let imgInput = null;" in JS
    sets = [m.start() for m in re.finditer(r"imgWatch = \{", JS)]
    enclosing = [re.findall(r"\nfunction (\w+)", JS[:i])[-1] for i in sets]
    assert enclosing == ["imgRequest"], enclosing
    assert "if (CFG.image) imgInput = { cursor: null, vk: {} };" in JS
    assert "if (imgInput) imgNoteInput(a);" in JS
    # the poll reply handler
    assert "if (reply.img && CFG.image) {" in JS


def test_runs_of():
    m = np.array([0, 1, 1, 0, 1, 0, 0, 1, 1, 1], bool)
    assert R.runs_of(m) == [(1, 2), (4, 1), (7, 3)]
    assert R.runs_of(np.zeros(5, bool)) == []
    assert R.runs_of(np.ones(3, bool)) == [(0, 3)]


def test_pointer_words():
    words = [0x00001234, 0x02345678, 0x004C1000, 0x10010000, 0x0000FFFF, 0x02000000]
    img = b"".join(w.to_bytes(4, "little") for w in words)
    ranges = [[0x02000000, 0x01000000], [0x10000000, 0x00100000], [0x00000000, 0x00020000]]
    m = R.pointer_words(img, ranges)
    # a value in a heap / DLL range is a pointer; small values (< 0x10000) and golf_clean.exe addresses are not
    got = [bool(m[4 * i]) for i in range(len(words))]
    assert got == [False, True, False, True, False, True]
    assert all(m[4 * i:4 * i + 4].all() == got[i] for i in range(len(words)))


def test_variant_masks():
    smap = R.SymbolMap()
    data = bytes(R.DATA_N)
    noise = np.zeros(R.DATA_N, bool)
    noise[100:110] = True
    assert not R.build_image("none", data, noise, smap).any()
    assert R.build_image("full", data, noise, smap).all()
    d = R.build_image("data", data, noise, smap)
    assert not d[100:110].any() and d.sum() == R.DATA_N - 10
    s = R.build_image("sve", data, noise, smap)
    assert s[0x00834170 - R.DATA_VA:0x00834174 - R.DATA_VA].all()      # g_date is a save field
    assert not s[0x00822D9C - R.DATA_VA]                               # the RNG seed is not
    m = R.build_image("model", data, noise, smap)
    assert m[0x00834170 - R.DATA_VA] and not m[0x005AAA34 - R.DATA_VA]  # g_cellPool (SpriteRef pool) is pointer-flagged


def test_model_pointer_flags():
    ps = R.pointer_structs()
    assert "SpriteRef" in ps and "Golfer" not in ps
    o = {"element_type": "uint16_t [50][50] (TF_*)", "c_type": "uint16_t"}
    assert not R.is_pointer_object(o, ps)
    assert R.is_pointer_object({"element_type": "ptr", "c_type": "void *"}, ps)
    assert R.is_pointer_object({"element_type": "SpriteRef", "c_type": "SpriteRef"}, ps)


def test_corrupt():
    smap = R.SymbolMap()
    blob = bytearray(R.DATA_N)
    c = R.corrupt(blob, smap, "g_date:5")
    assert c["symbol"] == "g_date" and c["old"] == 0 and c["new"] == 5
    assert int.from_bytes(blob[0x00834170 - R.DATA_VA:0x00834174 - R.DATA_VA], "little") == 5
