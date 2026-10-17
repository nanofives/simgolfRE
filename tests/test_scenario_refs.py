"""Format of the scenario reference manifest re/analysis/data/scenario_refs.json (re/tools/scenario_refs.py, batch b2).
Static: the manifest is read as a file; the references it points at live in log/ (not committed) and are not opened.
Runtime reproducibility is measured by `scenario_refs.py regen / variance`, never in the static suite."""
import json
import re
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static
MANIFEST = ROOT / "re" / "analysis" / "data" / "scenario_refs.json"
SHA = re.compile(r"^[0-9a-f]{64}$")
SHA16 = re.compile(r"^[0-9a-f]{16}$")
WORD = re.compile(r"^0x[0-9a-f]{8}$")


@pytest.fixture(scope="module")
def doc():
    if not MANIFEST.exists():
        pytest.skip("no manifest yet")
    return json.loads(MANIFEST.read_text())


def test_header(doc):
    assert doc["version"] == 1
    assert doc["root"] == "log/c3/alt/refs"
    assert "scenario_refs.py regen" in doc["procedure"]
    assert isinstance(doc["scenarios"], dict) and doc["scenarios"]


def test_every_entry(doc):
    for name, e in doc["scenarios"].items():
        assert e["family"] in ("s36a", "s36b", "c3fya", "c3fyb"), name
        assert e["reference"] == f"log/c3/alt/refs/{name}/det_b", name
        assert SHA.match(e["schedule_sha256"]) and SHA.match(e["reference_sha256"]), name
        assert e["checkpoints"] and all(isinstance(c, str) for c in e["checkpoints"]), name
        fc = e["first_checkpoint"]
        assert fc["checkpoint"] == e["checkpoints"][0], name
        for k in ("seed_time", "ticker", "rng_seed"):
            assert WORD.match(fc[k]), (name, k)
        assert SHA16.match(fc["state_sha16"]) and SHA16.match(fc["frame_sha16"]), name
        # a pinned reference: two OFF replays identical (det_b == det_c)
        assert e["verified"]["det_b_vs_det_c"].startswith("SAME"), name
        for k in ("winmm_sha256", "golf_clean_sha256", "scenario_det_js_sha256"):
            assert e["build"][k] is None or SHA.match(e["build"][k]), (name, k)
        assert isinstance(e["conditions"]["waveOutGetNumDevs"], int), name
        assert isinstance(e["sound_live"], bool), name


def test_audio_condition_is_uniform(doc):
    # every pinned reference must come from one audio condition: with the audio stand-in (C3FT_AUDIO=none, the default
    # since b2) the sound thread never runs, so sound_live is false everywhere and seed_time is T0 = 0x00100000
    live = {n for n, e in doc["scenarios"].items() if e["sound_live"]}
    assert not live, f"references made with the sound engine live: {sorted(live)}"
    seeds = {e["first_checkpoint"]["seed_time"] for e in doc["scenarios"].values()}
    assert seeds == {"0x00100000"}, seeds


PROBE = r"""
import json, os, sys
sys.argv = [sys.argv[0]]
sys.path.insert(0, r"%s")
import scenario_ab as s
out = {}
for mode in ("none", "live"):
    os.environ["C3FT_AUDIO"] = mode
    out[mode] = sorted(x["kind"] for x in s.slot_config() if x["kind"].startswith("audio:"))
cp = lambda fwd: {"checkpoints": {"menu": {"status": {"forwarded": fwd}}}}
out["same"] = s.arm_conditions_differ({**cp(0), "audio": "none"}, {**cp(0), "audio": "none"})
out["old_deviceless_ref"] = s.arm_conditions_differ(cp(0), {**cp(0), "audio": "none"})
out["old_live_ref"] = s.arm_conditions_differ(cp(234), {**cp(0), "audio": "none"})
out["live_vs_dead"] = s.arm_conditions_differ({**cp(0), "audio": "live"}, {**cp(12), "audio": "live"})
print(json.dumps(out))
"""


def test_audio_stand_in_slots_and_incomparable_arms():
    """C3FT_AUDIO=none (default) patches sound.dll's DirectSound enumeration / creation and binkw32's waveOutOpen;
    =live patches none. Two arms under different audio conditions are reported, never scored."""
    r = subprocess.run([sys.executable, "-c", PROBE % (ROOT / "re" / "frida")], capture_output=True, text=True,
                       cwd=str(ROOT), timeout=120)
    assert r.returncode == 0, r.stderr[-2000:]
    out = json.loads(r.stdout.strip().splitlines()[-1])
    assert out["none"] == ["audio:cocreate", "audio:dsenum", "audio:waveOutOpen"]
    assert out["live"] == []
    assert out["same"] == ""
    assert out["old_deviceless_ref"] == ""          # a pre-b2 reference made with no device ran the same path
    assert out["old_live_ref"] == "audio path sound engine live / device-less"
    assert out["live_vs_dead"] == "audio path device-less / sound engine live"
