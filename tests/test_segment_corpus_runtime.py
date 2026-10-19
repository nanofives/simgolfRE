"""re/tools/segment_corpus.py end to end on the original game (phase 2.4 segment corpus, 2026-10-18).

Boots the instance (default instances\\p24, P24_TEST_INSTANCE overrides; winmm.dll of the pinned references' build) six
times on sandbox_basic, about 2-3 minutes, marked `slow`; run folders go to log/p2/p24/test (P24_OUT), never to the corpus
runs: three continuous capture runs (each SAME as the pinned det_b), the plan (captures agree, trace and frame hashes
join), two restarts of segment 0 from its frame-head image (both reproduce every expected frame record and the end
checkpoint's sim hashes) and a negative control restart with g_rng + 1 in the written image (RED at the first frame after
the start, g_rng first). Then extract runs in-process into a temporary corpus: the trace slice validates, the start frame
record equals the image's sim hashes, the manifest lists the files with their sha256.
"""
import json
import os
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = [pytest.mark.runtime, pytest.mark.slow]

INSTANCE = os.environ.get("P24_TEST_INSTANCE", "p24")
OUT = ROOT / "log" / "p2" / "p24" / "test"
TOOL = [sys.executable, str(ROOT / "re" / "tools" / "segment_corpus.py"), "--instance", INSTANCE]
SCEN = "sandbox_basic"


def run(*args, timeout=1800):
    env = dict(os.environ, P24_OUT=str(OUT))
    r = subprocess.run(TOOL + list(args), capture_output=True, text=True, timeout=timeout, cwd=str(ROOT), env=env)
    return r.returncode, r.stdout + r.stderr


@pytest.fixture(scope="module")
def runs():
    if not (ROOT / "instances" / INSTANCE / "golf_clean.exe").exists():
        pytest.skip(f"instances\\{INSTANCE} missing (re/tools/instances.py create {INSTANCE} + the pinned winmm.dll)")
    if not (ROOT / "port" / "corpus" / "traces" / f"{SCEN}.trace").exists():
        pytest.skip("port/corpus/traces/sandbox_basic.trace missing (segment_corpus.py trace --scenario sandbox_basic)")
    out = {}
    for lab in ("c_a", "c_b", "c_c"):
        out[lab] = run("capture", "--scenario", SCEN, "--label", lab)
    out["plan"] = run("plan", "--scenario", SCEN)
    for lab in ("s00_r1", "s00_r2"):
        out[lab] = run("restart", "--scenario", SCEN, "--segment", "0", "--label", lab)
    out["neg"] = run("restart", "--scenario", SCEN, "--segment", "0", "--label", "s00_neg", "--corrupt", "g_rng")
    out["verify"] = run("verify", "--scenario", SCEN)
    return out


def test_captures_equal_pinned(runs):
    for lab in ("c_a", "c_b", "c_c"):
        rc, txt = runs[lab]
        assert rc == 0, txt[-2000:]
        s = json.loads((OUT / SCEN / lab / "summary.json").read_text())
        assert s["p24"]["vs_pinned"].startswith("SAME"), s["p24"]["vs_pinned"]


def test_plan(runs):
    assert runs["plan"][0] == 0, runs["plan"][1][-2000:]
    plan = json.loads((OUT / SCEN / "plan.json").read_text())
    assert plan["captures_agree"] and plan["prefix"] == "sandbox_00"
    assert plan["trace_join"]["same_T"] and plan["trace_join"]["same_index"]
    s0 = plan["segments"][0]
    assert s0["continuous_same"] and s0["trace"]["valid"] == "OK" and s0["inputs"]["inputs"] == 0


def test_restarts_reproduce_and_control_reads_red(runs):
    for lab in ("s00_r1", "s00_r2", "neg"):
        assert runs[lab][0] == 0, runs[lab][1][-2000:]
    v = json.loads((OUT / SCEN / "verify.json").read_text())["segments"][0]
    assert v["accepted"], v["reasons"]
    assert v["class"] == "TIME-FORWARD"
    for lab in ("r1", "r2"):
        r = v["restarts"][lab]
        assert r["frames"]["same"] and r["frames"]["expected"] == r["frames"]["got"] > 20 and r["end_sim"]["same"]
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    os.environ["P24_OUT"] = str(OUT)
    import segment_corpus as SC
    SC.OUT = OUT
    sd = OUT / SCEN
    seg = SC.load_plan(SCEN)["segments"][0]
    sym, T, _I, H = SC.read_frames(sd / "c_a")
    w = SC.frame_window(T, seg["start_ret"], seg["end_T"])
    post = w[T[w] > seg["start_ret"]]
    end_hash = json.loads((sd / "c_a" / f"{seg['end']}.objhash.json").read_text())["hashes"]
    neg = SC.restart_result(sd, "s00_neg", seg, (T[post], H[post]), sym, end_hash)
    d = neg["frames"]["first_divergent"]
    assert d is not None and d["record"] == 0 and d["objects"][0] == "g_rng"
    assert not neg["end_sim"]["same"]


def test_extract(runs, tmp_path, monkeypatch):
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import segment_corpus as SC
    import trace_format as TF
    monkeypatch.setattr(SC, "OUT", OUT)
    monkeypatch.setattr(SC, "CORPUS", tmp_path / "segments")
    monkeypatch.setattr(SC, "MANIFEST_JSON", tmp_path / "segments.json")
    monkeypatch.setattr(SC, "MANIFEST_TSV", tmp_path / "segments_manifest.tsv")
    monkeypatch.setattr(SC, "ROOT", tmp_path)
    import argparse
    assert SC.cmd_extract(argparse.Namespace(scenario=SCEN)) == 0
    d = tmp_path / "segments" / SCEN / "00"
    sj = json.loads((d / "start.json").read_text())
    assert sj["start_frame_hash_check"] and sj["noise"]["sim_bytes"] == 0
    st = TF.read(d / "trace.trace")
    assert TF.validate(st) == [] and st.t0 == sj["start"]["frame_value"]
    man = json.loads((tmp_path / "segments.json").read_text())["segments"]
    e = next(x for x in man if x["scenario"] == SCEN and x["segment"] == 0)
    assert e["status"] == "accepted" and set(e["files"]) == set(SC.CORPUS_FILES)
