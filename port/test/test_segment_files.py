"""Phase 3.8 stream 38b: the extra install files of cov_a_flags (port/test/segment_files.py)."""
from __future__ import annotations

import pathlib
import struct

import pytest

from port.test import segment_files as SF

ROOT = pathlib.Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "log" / "c3" / "c3fya" / "fixtures"
need_install = pytest.mark.skipif(not (SF.INSTALL / "Saved Games" / "mood5.sve").exists(), reason="the install's saves are absent")


@need_install
def test_flagged_copies_differ_from_mood5_in_one_flag_bit_only():
    raw = (SF.INSTALL / "Saved Games" / "mood5.sve").read_bytes()
    files = SF.files_for("cov_a_flags")
    assert sorted(files) == ["Saved Games/zdemo.sve", "Saved Games/zedit.sve", "Saved Games/ztut.sve"]
    (v0,) = struct.unpack_from("<I", raw, SF.FLAGS_AT)
    for name, bit in SF.COV_A_FLAGS_FIXTURES.items():
        data = files["Saved Games/" + name]
        assert len(data) == len(raw)
        diff = [i for i in range(len(raw)) if raw[i] != data[i]]
        assert diff and all(SF.FLAGS_AT <= i < SF.FLAGS_AT + 4 for i in diff)
        assert struct.unpack_from("<I", data, SF.FLAGS_AT)[0] == v0 | bit


@need_install
@pytest.mark.skipif(not FIXTURES.exists(), reason="the recording's fixture saves are not present (local, game-derived)")
def test_derivation_equals_the_recordings_fixtures():
    files = SF.files_for("cov_a_flags")
    for name in SF.COV_A_FLAGS_FIXTURES:
        assert files["Saved Games/" + name] == (FIXTURES / name).read_bytes(), name


def test_other_scenarios_need_no_extra_files():
    assert SF.files_for("sandbox_basic") == {} and SF.files_for("cov_a_keys") == {}
