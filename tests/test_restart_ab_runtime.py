"""re/frida/restart_ab.py end to end on the original game (phase 2.1 premise check, 2026-10-18).

Boots instances\\p21 five times (67 s on LIVING-PC, marked `slow`): three continuous sandbox_basic runs with a frame-head
capture after sandbox_00 and sandbox_05, a restart from sandbox_00 with the `model` image of sandbox_05 (at every
checkpoint after it: T, every .data byte outside the noise of the other two runs and the frame equal; DLL pages are not
asserted: the DLLs' data is not part of the image, and Terrain.dll / jgld.dll differ in 1-2 bytes), and the same
restart with g_date + 1 in the image (must be DIFF, with g_date among the differing objects). The instance must exist (re/tools/instances.py create p21, winmm.dll of
the pinned references' build).
"""
import json
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = [pytest.mark.runtime, pytest.mark.slow]

TOOL = [sys.executable, str(ROOT / "re" / "frida" / "restart_ab.py")]
OUT = ROOT / "log" / "p2" / "p21" / "sandbox_basic"


def run(*args, timeout=1200):
    r = subprocess.run(TOOL + list(args), capture_output=True, text=True, timeout=timeout, cwd=str(ROOT))
    return r.returncode, r.stdout + r.stderr


@pytest.fixture(scope="module")
def captures():
    if not (ROOT / "instances" / "p21" / "golf_clean.exe").exists():
        pytest.skip("instances\\p21 missing (re/tools/instances.py create p21)")
    for label in ("t_ca", "t_cb", "t_cc"):
        rc, out = run("capture", "--scenario", "sandbox_basic", "--label", label, "--at", "sandbox_00", "sandbox_05")
        assert rc == 0, out[-2000:]
    return True


def test_model_restart_reproduces_the_next_checkpoints(captures):
    rc, out = run("restart", "--scenario", "sandbox_basic", "--label", "t_r_model", "--source", "t_ca", "--noise",
                  "t_cb", "t_cc", "--from", "sandbox_00", "--image", "sandbox_05", "--variant", "model")
    assert rc == 0, out[-2000:]
    c = json.loads((OUT / "t_r_model" / "compare_t_ca.json").read_text())
    after = {n: r for n, r in c["checkpoints"].items() if r.get("phase") == "after"}
    assert after, c
    assert all(r["T_ref"] == r["T_run"] and r["data_bytes_differ"] == 0 and r["frame_px_differ"] == 0
               for r in after.values()), {n: r.get("objects_differ") for n, r in after.items()}


def test_corrupted_image_reads_red(captures):
    rc, out = run("restart", "--scenario", "sandbox_basic", "--label", "t_r_neg", "--source", "t_ca", "--noise",
                  "t_cb", "t_cc", "--from", "sandbox_00", "--image", "sandbox_05", "--variant", "model",
                  "--corrupt", "g_date:1")
    assert rc == 0, out[-2000:]
    c = json.loads((OUT / "t_r_neg" / "compare_t_ca.json").read_text())
    assert c["verdict"] == "DIFF"
    after = [r for r in c["checkpoints"].values() if r.get("phase") == "after"]
    assert after and all("g_date" in r["objects_differ"] for r in after)
