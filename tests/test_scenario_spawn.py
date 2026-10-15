"""re/frida/scenario_ab.py --spawn-reach (round 36, s36c): the opt-in mode must leave the default mode unchanged.

Checked without booting the game, in a subprocess (importing scenario_ab sets SIMGOLF_GAME_DIR / SIMGOLF_ALLOW_MULTI for
its process, which must not leak into this test session):
  - the default agent config has no `spawnReach` key, the spawn one has `spawnReach: true` and is otherwise identical;
  - the agent source differs from the default only in that config line;
  - outputs are filed under <scenario> by default and <scenario>_spawn with the flag (folder and CSV names);
  - scenario_det.js keeps every spawn-only code path behind CFG.spawnReach.
"""
import json
import re
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

PROBE = r"""
import json, pathlib, sys, tempfile, types
sys.argv = [sys.argv[0]]
sys.path.insert(0, r"%s")
import scenario_ab as s
out = {}
for spawn in (False, True):
    d = pathlib.Path(tempfile.mkdtemp())
    r = s.Run("x", "sandbox_basic", "replay", {}, [], [], out_dir=d, spawn_reach=spawn)
    src = r.agent_source().read_text()
    out[str(spawn)] = src
a0 = types.SimpleNamespace(scenario="sandbox_basic")
a1 = types.SimpleNamespace(scenario="sandbox_basic", spawn_reach=True)
a2 = types.SimpleNamespace(scenario="sandbox_basic", spawn_reach=False)
out["keys"] = [s.scen_key(a0), s.scen_key(a1), s.scen_key(a2)]
out["csv"] = [s.scenario_csv_path("golf_clean.exe", 0x473a60, "dynInit_initGlobalImage", pathlib.Path("."), k).name
              for k in out["keys"]]
print(json.dumps(out))
""" % (ROOT / "re" / "frida")


@pytest.fixture(scope="module")
def probe():
    r = subprocess.run([sys.executable, "-c", PROBE], capture_output=True, text=True, cwd=str(ROOT), timeout=120)
    assert r.returncode == 0, r.stderr[-2000:]
    return json.loads(r.stdout.strip().splitlines()[-1])


def cfg_of(src):
    line = src.split("\n", 1)[0]
    m = re.fullmatch(r"const SCENARIO_CFG = (.*);", line)
    assert m, line[:200]
    return json.loads(m.group(1))


def test_default_config_has_no_spawn_key(probe):
    c0, c1 = cfg_of(probe["False"]), cfg_of(probe["True"])
    assert "spawnReach" not in c0
    assert c1.pop("spawnReach") is True
    assert c0 == c1


def test_agent_differs_only_in_the_config_line(probe):
    assert probe["False"].split("\n", 1)[1] == probe["True"].split("\n", 1)[1]


def test_output_names(probe):
    assert probe["keys"] == ["sandbox_basic", "sandbox_basic_spawn", "sandbox_basic"]
    assert probe["csv"][0] == "00473a60_dynInit_initGlobalImage_sandbox_basic.scenario.csv"
    assert probe["csv"][1] == "00473a60_dynInit_initGlobalImage_sandbox_basic_spawn.scenario.csv"


def test_spawn_paths_are_gated_in_the_agent():
    js = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text()
    for name in ("bootHits", "armedAt"):
        for m in re.finditer(name, js):
            # every line that sets or reports a spawn-only field sits under an `if (CFG.spawnReach ...)` test
            start = js.rfind("\n", 0, m.start())
            block = js[max(0, start - 600):m.start()]
            assert "CFG.spawnReach" in block, js[start:start + 120]
