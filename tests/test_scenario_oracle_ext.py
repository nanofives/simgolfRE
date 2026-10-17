"""re/frida/scenario_ab.py --call-log / --extra-regions (batch c3fyl): the opt-in extended oracle leaves the default mode
unchanged.

Checked without booting the game, in a subprocess (importing scenario_ab sets SIMGOLF_GAME_DIR / SIMGOLF_ALLOW_MULTI for
its process, which must not leak into this test session):
  - the default agent config has no `callLog` / `extraRegions` key; with C3FT_CALL_LOG / C3FT_EXTRA_REGIONS set it has
    them (parsed from the spec) and is otherwise identical, and the agent source differs only in that config line;
  - the spec parser rejects an address outside golf_clean.exe .text and a string argument index >= n;
  - compare_ext returns None for two runs made without the options (so write_results / determinism print and write
    nothing new), finds a differing call argument and a differing extra region byte, and reports runs made with
    different specs as incomparable;
  - scenario_det.js keeps every call-log / extra-region code path behind CFG.callLog / CFG.extraRegions.
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
    os.environ.pop("C3FT_CALL_LOG", None); os.environ.pop("C3FT_EXTRA_REGIONS", None)
    if opt:
        os.environ["C3FT_CALL_LOG"] = "4481b0:5,404bc0:4:40c9f5:s0"
        os.environ["C3FT_EXTRA_REGIONS"] = "80e248:0xd8"
    d = pathlib.Path(tempfile.mkdtemp())
    r = s.Run("x", "sandbox_basic", "replay", {}, [], [], out_dir=d)
    out[str(opt)] = r.agent_source().read_text()
errs = []
for spec in ("10001000:2", "4481b0:2:s2"):
    os.environ["C3FT_CALL_LOG"] = spec
    try:
        s.call_log_spec()
        errs.append(None)
    except SystemExit as e:
        errs.append(str(e))
os.environ.pop("C3FT_CALL_LOG", None)
out["errs"] = errs

# synthetic run folders for compare_ext
def mk(calls=None, extra=None, xspec=None, cspec=None):
    d = pathlib.Path(tempfile.mkdtemp())
    sm = {"checkpoints": {"cp1": {"T": 1}, "cp2": {"T": 2}}}
    if xspec:
        sm["extra_regions"] = xspec
    (d / "summary.json").write_text(json.dumps(sm))
    if calls is not None:
        (d / "calls.json").write_text(json.dumps({"spec": cspec, "dropped": 0, "rows": calls}))
    if extra is not None:
        for n, b in extra.items():
            (d / f"{n}.extra.bin").write_bytes(b)
    return d
spec = [{"va": 0x4481B0, "n": 2}]
base = [["cp1", 10, 0x4481B0, 0x410373, 0x26, 0x64], ["cp2", 20, 0x4481B0, 0x41e26e, 0x95, 0x64]]
mut = [["cp1", 10, 0x4481B0, 0x410373, 0x27, 0x64], ["cp2", 20, 0x4481B0, 0x41e26e, 0x95, 0x64]]
xs = [[0x80E248, 4]]
res = {}
res["default"] = s.compare_ext(mk(), mk())
res["same"] = s.compare_ext(mk(base, cspec=spec), mk(base, cspec=spec))
res["call_diff"] = s.compare_ext(mk(base, cspec=spec), mk(mut, cspec=spec))
res["extra_diff"] = s.compare_ext(mk(extra={"cp1": b"\0\0\0\0", "cp2": b"\1\0\0\0"}, xspec=xs),
                                  mk(extra={"cp1": b"\0\0\0\0", "cp2": b"\1\2\0\0"}, xspec=xs))
res["incomparable"] = s.compare_ext(mk(base, cspec=spec), mk(base, cspec=[{"va": 0x4481B0, "n": 3}]))
# a return address inside the shim (an ON arm's reimplementation) compares equal to any other shim address
shim1 = [["cp1", 10, 0x4481B0, 0x61EC0001, 0x26, 0x64], ["cp2", 20, 0x4481B0, 0x41e26e, 0x95, 0x64]]
shim2 = [["cp1", 10, 0x4481B0, 0x5F0C2002, 0x26, 0x64], ["cp2", 20, 0x4481B0, 0x41e26e, 0x95, 0x64]]
res["shim_ra"] = s.compare_ext(mk(shim1, cspec=spec), mk(shim2, cspec=spec))
# `xK`: a masked argument (heap pointer) differs without a difference
xspec_c = [{"va": 0x4A5B58, "n": 3, "x": [1]}]
w1 = [["cp1", 5, 0x4A5B58, 0x40FA87, 4, 0x3AB9A80, 0x618]]
w2 = [["cp1", 5, 0x4A5B58, 0x40FA87, 4, 0x3A99A80, 0x618]]
w3 = [["cp1", 5, 0x4A5B58, 0x40FA87, 5, 0x3A99A80, 0x618]]
res["masked_same"] = s.compare_ext(mk(w1, cspec=xspec_c), mk(w2, cspec=xspec_c))
res["masked_diff"] = s.compare_ext(mk(w1, cspec=xspec_c), mk(w3, cspec=xspec_c))
os.environ["C3FT_CALL_LOG"] = "4a5b58:3:x1:r"
res["xspec"] = s.call_log_spec()
os.environ.pop("C3FT_CALL_LOG", None)
out["ext"] = res
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


def test_default_config_has_no_oracle_keys(probe):
    c0, c1 = cfg_of(probe["False"]), cfg_of(probe["True"])
    assert "callLog" not in c0 and "extraRegions" not in c0
    assert c1.pop("callLog") == [{"va": 0x4481B0, "n": 5}, {"va": 0x404BC0, "n": 4, "ra": [0x40C9F5], "s": 0}]
    assert c1.pop("extraRegions") == [[0x80E248, 0xD8]]
    assert c0 == c1


def test_agent_differs_only_in_the_config_line(probe):
    assert probe["False"].split("\n", 1)[1] == probe["True"].split("\n", 1)[1]


def test_call_log_spec_limits(probe):
    assert all(e and "--call-log" in e for e in probe["errs"]), probe["errs"]


def test_compare_ext(probe):
    e = probe["ext"]
    assert e["default"] is None
    assert e["same"]["same"] is True
    cd = e["call_diff"]
    assert cd["same"] is False and cd["checkpoints"]["cp2"]["same"] is True
    assert cd["checkpoints"]["cp1"]["calls_first_diff"]["index"] == 0
    assert cd["checkpoints"]["cp1"]["calls_first_diff"]["b"][3] == 0x27
    xd = e["extra_diff"]
    assert xd["same"] is False and xd["checkpoints"]["cp1"]["same"] is True
    assert xd["checkpoints"]["cp2"]["extra_differ"] == [{"va": "0x0080e248", "bytes": 1, "first": "0x0080e249"}]
    assert "incomparable" in e["incomparable"]
    assert e["shim_ra"]["same"] is True
    assert e["masked_same"]["same"] is True and e["masked_diff"]["same"] is False
    assert e["xspec"] == [{"va": 0x4A5B58, "n": 3, "x": [1], "r": 1}]


def test_oracle_paths_are_gated_in_the_agent():
    js = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text()
    # the Interceptor that logs calls and the extra-region send sit under a test of their config key
    for name in ("Interceptor.attach(ptr(c.va)", "send({ type: 'extra' }"):
        i = js.find(name)
        assert i > 0, name
        block = js[max(0, i - 1500):i]
        assert re.search(r"CFG\.(callLog|extraRegions)", block), name
    # callLog() returns undefined without the key, so the dump message (cl: callLog()) is the default one
    m = re.search(r"function callLog\(\) \{\s*if \(!CFG\.callLog\) return undefined;", js)
    assert m
