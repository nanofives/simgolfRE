"""S3.3 (U-1120): a boot that runs on past the last FRAME record (to_end, which framegate sets whenever a snapshot lies past the last
frame) ends in the last frame's Blocking pump with no recorded input left. That frame threw NeedInput (a MissingResource, code -11)
out of sg_run_boot after every record, with or without a pending gfx.snapshotAt. The boot now ends there as the end of the
recording: code 0, the snapshots not yet taken are taken from the back buffer as it stands (gfx_end_of_input)."""
import pytest

from port.test import boot as B
from port.test import lib as L

pytestmark = [pytest.mark.port]

SCENARIO = "cov_k3_career"  # 60 records, its trace ends in pumpMessages


def _run(snap, to_end=True):
    lib = L.load()
    got = {}

    def before(st):
        st.call("gfx.attach", [])
        if snap is not None:
            st.call("gfx.snapshotAt", [snap])

    def after(st, _res):
        got["detail"] = _res.run_detail
        got["snap"] = st.files_read("$gfx/snapshot_0.555") if snap is not None else None

    r = B.run(SCENARIO, lib, before=before, after=after, to_end=to_end)
    return r, got


def _need_corpus():
    if B.expected_records(SCENARIO)[1] is None or not (B.TRACES / f"{SCENARIO}.trace").exists():
        pytest.skip("the corpus trace / capture is not present")


@pytest.mark.parametrize("snap", [None, 100, 4_000_000_000])
def test_boot_to_end_returns_zero(snap):
    _need_corpus()
    r, got = _run(snap)
    assert r.status == "PASS", (r.status, r.detail)
    assert r.records == r.expected
    if got["detail"]:  # the Win32 runner does not carry sg_run_frames_detail
        assert "no input left" in got["detail"]


def test_pending_snapshot_past_the_input_is_taken_at_the_end():
    # a request beyond every trace item is only satisfiable by gfx_end_of_input
    _need_corpus()
    _r, got = _run(4_000_000_000)
    assert got["snap"] is not None and len(got["snap"]) == 800 * 600 * 2


def test_without_to_end_unchanged():
    _need_corpus()
    r, _got = _run(100, to_end=False)
    assert r.status == "PASS", (r.status, r.detail)
