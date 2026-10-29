"""S4.2b: port/test/test_audio_ds.py and test_audio_dev.py onto port/test/sound_replay.py (no 8_replay.py). One-time patch.

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_tests48.py [--check]
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
OLD_IMPORT = '''import importlib.util
import os
import pathlib
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

# the S4.8 replay front end: port/test/sound_replay_s48.py once merged (8_merge.md), log/roadmap/S4/8_replay.py until then
_W = ROOT / "port" / "test" / "sound_replay_s48.py"
_spec = importlib.util.spec_from_file_location(
    "s48_replay", _W if _W.exists() else ROOT / "log" / "roadmap" / "S4" / "8_replay.py")
W = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(W)
R = W.R
E = W.E
from port.test import lib as L  # noqa: E402
'''
NEW_IMPORT = '''import os
import pathlib
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import sound_replay as R  # noqa: E402
from port.test import lib as L  # noqa: E402,F401

E = R.E
'''
OLD_DOC = '''The replay front end is log/roadmap/S4/8_replay.py (S4.2's port/test/sound_replay.py with this step's goldens, frame and
control-word handling and custom keys; the changes it needs in sound_replay.py are 8_merge.md). Default runs replay the'''
NEW_DOC = '''The replay front end is port/test/sound_replay.py (`--step S4.8 --family ds|dev`; S4.8's log/roadmap/S4/8_replay.py until
S4.2b folded its goldens per module, custom keys, entry esp and import-log check into it). Default runs replay the'''


def patch(name, family):
    p = ROOT / "port" / "test" / name
    t = p.read_text(encoding="utf-8")

    def rep(old, new, n=None):
        nonlocal t
        c = t.count(old)
        assert c and (n is None or c == n), (name, old[:70], c)
        t = t.replace(old, new)
    rep(OLD_IMPORT, NEW_IMPORT, 1)
    rep(OLD_DOC, NEW_DOC, 1)
    rep(f"KEYS = W.family_keys(FAMILY)", "KEYS = R.keys_for(\"S4.8\", FAMILY)", 1)
    rep("    R.GOLDEN = W.GOLDENS[FAMILY]\n    g = R.load_golden()\n", "    g = R.load_golden(\"S4.8\", FAMILY)\n")
    rep("W.replay_key(", "R.replay_key(")
    rep("    R.GOLDEN = W.GOLDENS[FAMILY]\n    v, _, _ = R.oracle_key(vm, sndlib, key)\n",
        "    v, _, _ = R.oracle_key(vm, sndlib, key)\n", 1)
    rep("    g = R.load_golden().get(key)\n", "    g = R.load_golden(\"S4.8\", FAMILY).get(key)\n", 1)
    rep("    assert W.sinklog(keys, FAMILY) == 0\n", "    assert R.sinklog_main(keys) == 0\n", 1)
    rep("logs record for record (8_replay.sinklog).", "logs record for record (sound_replay.sinklog_key).", 1)
    rep("(SG_EMU_ORACLE=1 log/roadmap/S4/8_replay.py oracle --family " + family + " --update)",
        "(SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.8 --family " + family + " --update)")
    rep("        pytest.skip(f\"{p} not built (log/roadmap/S4/8_mirror.py)\")\n",
        "        pytest.skip(f\"{p} not built (the S4 mirror, log/roadmap/S4/2b_mirror.py)\")\n", 1)
    assert "W." not in t.replace("NEW", ""), [ln for ln in t.splitlines() if "W." in ln][:5]
    return p, t


out = [patch("test_audio_ds.py", "ds"), patch("test_audio_dev.py", "dev")]
if "--check" in sys.argv:
    print("ok (not written)")
else:
    for p, t in out:
        p.write_text(t, encoding="utf-8")
        print(f"wrote {p}")
