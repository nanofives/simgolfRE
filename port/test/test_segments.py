"""Segment corpus: load path and the C++ sgh64 against every accepted segment, plus the negative controls (phase 2.6)."""
from __future__ import annotations

import pathlib
import random
import sys

import numpy as np
import pytest

from port.test import lib as L
from port.test import segments as S

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not S.SEG_DIR.exists(), reason="segment corpus not present (port/corpus/segments, local)")
ACCEPTED = [r for r in S.manifest_rows() if r["status"] == "accepted"]


def test_hash_matches_state_image(portlib):
    """C++ sgh64 == re/tools/state_image.py sgh64 (lengths 0..37 and a 4 KB run)."""
    import state_image
    rng = random.Random(26)
    for n in list(range(38)) + [4096, 4099]:
        data = bytes(rng.randrange(256) for _ in range(n))
        assert portlib.hash_bytes(data) == state_image.sgh64(data), n


def test_class_hashes_match_state_image(portlib):
    """Per-object hashes of a random image, every class, pointer words zeroed, equal to state_image.StateImage."""
    import state_image
    rng = np.random.default_rng(26)
    img = rng.integers(0, 256, portlib.state_size, dtype=np.uint8).tobytes()
    si = state_image.StateImage(img, known=np.ones(portlib.state_size, bool))
    with portlib.state() as st:
        st.load_image(img)
        for cls in ("sim", "pointer", "ui", "const", "hidden", "render", "audio", "engine"):
            got = dict(zip([n for n, _, _ in portlib.objects(cls)], st.hash_class(cls)))
            exp = {k: int(v["hash"], 16) for k, v in si.hashes([cls]).items()}
            assert got == exp, cls


@need_corpus
@pytest.mark.parametrize("row", ACCEPTED, ids=[f"{r['scenario']}/{int(r['segment']):02d}" for r in ACCEPTED])
def test_segment_loads(srunner, row):
    d = S.segment_dir(row)
    if not d.exists():
        pytest.skip(f"{d} not present")
    seg = S.load_segment(d)
    assert len(seg.records) == int(row["frames"]) + 1, (len(seg.records), row["frames"])   # record 0 = the start
    # phase 3.7a: sg_run_frames exists, so a full run is judged elsewhere (port/test/test_tick.py: the TIME-FORWARD
    # segments; the COMMAND ones need phase 3.7b / 3.8's input); here the load path alone, for every accepted segment
    with srunner.lib.state() as st:
        st.set_data_base(S.DATA_VA)
        why = srunner.load_check(seg, st)
    assert why is None, f"{seg.name}: {why}"


@need_corpus
def test_corrupted_segment_fails_load(portlib, srunner):
    d = S.segment_dir(ACCEPTED[0])
    seg = S.load_segment(d)
    good = seg.start
    rng = random.Random(26)
    for _ in range(3):
        name, off, size = rng.choice(portlib.objects("sim"))
        k = rng.randrange(size)
        bad = bytearray(good)
        bad[off + k] ^= 0x01
        seg.start = bytes(bad)
        r = srunner.run(seg)
        assert r.status == "LOAD_FAILED" and name in r.detail, (name, r.detail)
        div = S.first_byte_divergence(portlib, good, bytes(bad))
        assert div["symbol"] == name and div["object_offset"] == k and div["objects_differing"] == [name]
    seg.start = good
    assert srunner.run(seg).status != "LOAD_FAILED"


@need_corpus
def test_byte_divergence_ignores_pointer_words_and_other_classes(portlib):
    seg = S.load_segment(S.segment_dir(ACCEPTED[0]))
    b = bytearray(seg.start)
    root = next(o for o in portlib.objects("ui") + portlib.objects("pointer") + portlib.objects(None)
                if o[0] == "g_rootWindow")
    b[root[1] + 0x11C] ^= 0xFF                      # an SG_POINTER_WORD dword
    assert S.first_byte_divergence(portlib, seg.start, bytes(b), classes=None) is None
    name, off, size = portlib.objects("render")[0]
    b[off] ^= 0x01
    assert S.first_byte_divergence(portlib, seg.start, bytes(b)) is None              # sim only
    assert S.first_byte_divergence(portlib, seg.start, bytes(b), classes=None)["symbol"] == name


@need_corpus
def test_run_frames_contract(portlib):
    """sg_run_frames returns SG_OK or a documented stop code, with frames_done records of T and sim hashes (phase 3.7a:
    the frames run; whether they equal the original is port/test/test_tick.py's question)."""
    seg = S.load_segment(S.segment_dir(ACCEPTED[0]))
    with portlib.state() as st:
        st.set_data_base(S.DATA_VA)   # some sim objects hold state addresses (unk_005a9cc8), valid at this base only
        st.load_image(seg.start)
        code, n, ts, hs = st.run_frames(seg.trace, seg.queued, len(seg.records) - 1)
    assert code in (L.SG_OK, L.SG_E_FAULT, L.SG_E_DIVIDE, L.SG_E_NO_RETURN, L.SG_E_NO_RESOURCE), code
    assert 0 <= n <= len(seg.records) - 1 and len(ts) == n and len(hs) == n
    assert all(len(h) == len(portlib.objects("sim")) for h in hs)
