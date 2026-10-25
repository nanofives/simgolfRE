"""Roadmap S2.5 (log/roadmap/S2/5_notes.md, U-1115): cov_a_quit and cov_a_quit_title have traces. The port's boot run of
cov_a_quit equals every frame record of the capture; cov_a_quit_title never enters the frame loop (title menu, then the
credits), so the boot gate reports NO_FRAMES for it by construction and only its trace is judged here."""
from __future__ import annotations

import pytest

from port.test import boot as B

pytestmark = pytest.mark.port

QUIT, QUIT_TITLE = "cov_a_quit", "cov_a_quit_title"


def _need(scen):
    if not (B.TRACES / f"{scen}.trace").exists():
        pytest.skip(f"no trace for {scen} (re/tools/segment_corpus.py trace --scenario {scen})")


@pytest.mark.parametrize("scen", (QUIT, QUIT_TITLE))
def test_trace_present(scen):
    _need(scen)
    assert (B.TRACES / f"{scen}.trace").stat().st_size > 100_000


def test_quit_boot_has_frames():
    _need(QUIT)
    assert B.expected_records(QUIT)[1] is not None
