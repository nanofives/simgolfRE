"""re/frida/scenario_ab.py --trace / --watch (batch c3fy2): the opt-in trace and write watch leave the default mode unchanged.

Checked without booting the game, in a subprocess (importing scenario_ab sets SIMGOLF_GAME_DIR / SIMGOLF_ALLOW_MULTI for
its process, which must not leak into this test session):
  - the default agent config has no `trace` / `watch` key; with C3FT_TRACE / C3FT_WATCH set it has them (parsed from
    `va:n` lists) and is otherwise identical;
  - the agent source differs from the default only in that config line;
  - the spec parser rejects more than four watches and sizes other than 1, 2, 4;
  - scenario_det.js keeps every trace / watch code path behind CFG.trace / CFG.watch (or the `watches` list built from it).
"""
import json
import re
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

PROBE = r"""
import json, os, pathlib, sys, tempfile
sys.argv = [sys.argv[0]]
sys.path.insert(0, r"%s")
import scenario_ab as s
out = {}
for opt in (False, True):
    os.environ.pop("C3FT_TRACE", None); os.environ.pop("C3FT_WATCH", None)
    if opt:
        os.environ["C3FT_TRACE"] = "575eb8:4,4df79b:1"
        os.environ["C3FT_WATCH"] = "575eb8:4"
    d = pathlib.Path(tempfile.mkdtemp())
    r = s.Run("x", "sandbox_basic", "replay", {}, [], [], out_dir=d)
    out[str(opt)] = r.agent_source().read_text()
errs = []
for spec in ("1:4,2:4,3:4,4:4,5:4", "575eb8:3"):
    os.environ["C3FT_WATCH"] = spec
    try:
        s.watch_spec()
        errs.append(None)
    except SystemExit as e:
        errs.append(str(e))
out["errs"] = errs
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


def test_default_config_has_no_trace_keys(probe):
    c0, c1 = cfg_of(probe["False"]), cfg_of(probe["True"])
    assert "trace" not in c0 and "watch" not in c0
    assert c1.pop("trace") == [[0x575EB8, 4], [0x4DF79B, 1]]
    assert c1.pop("watch") == [{"va": 0x575EB8, "size": 4}]
    assert c0 == c1


def test_agent_differs_only_in_the_config_line(probe):
    assert probe["False"].split("\n", 1)[1] == probe["True"].split("\n", 1)[1]


def test_watch_spec_limits(probe):
    assert all(e and "--watch" in e for e in probe["errs"]), probe["errs"]


def test_trace_paths_are_gated_in_the_agent():
    js = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text()
    # every use of the trace / watch helpers sits under a test of the config (or of the watches list built from it)
    for name in ("traceHex()", "armWatches()", "onWatchHit(d)", "msg.tr =", "msg.wl =", "where: 'exception'"):
        for m in re.finditer(re.escape(name), js):
            start = js.rfind("\n", 0, m.start())
            line = js[start:js.find("\n", m.start())]
            if line.lstrip().startswith("function"):
                continue
            block = js[max(0, start - 1000):m.start()]
            assert re.search(r"CFG\.(trace|watch)|traceRegs|watches\.length", block), line.strip()[:120]
