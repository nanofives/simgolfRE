"""P3 (log/roadmap/P3/notes.md): the device sink frees the sound buffers the engine releases.

With a sink that reports a buffer's end (Capture here; the real DirectSound device of `simgolf_port --audio on` the same way),
the engine reaps an idle sound and creates its buffer again on the next play. Before P3 the device sink (audio_exe.cpp
DeviceSink) kept every released buffer's storage in its 16 MB-chunk arena (at most 8 chunks) and its interface object in the
audio core's 64 KB object block, so a session's arena grew with wall time until MissingResource stopped the shadow: 40 plays
of slots 0x2c / 0x80 with 60 s idle gaps (simgolf_audio_check script, Capture) created 42 buffers and 7 chunks (115,602,288
arena bytes). Now the arena stays in its first chunk; the control (`--negctl audio_exe.deviceHeap=1`, nothing freed) is RED.
Model mode never reports an end, so it never reaps (4 buffers) and needs no test here.
"""
from __future__ import annotations

import json
import pathlib

import pytest

from port.test.test_app_audio import run_check

pytestmark = pytest.mark.port

PLAYS = 20   # pairs: playSound 0x2c, 2000 ticks (60 s), playSound 0x80, 2000 ticks


def _script(tmp: pathlib.Path) -> pathlib.Path:
    lines = ["# P3: static-buffer slots 0x2c / 0x80 with 60 s idle gaps (the engine's idle reaper), capture mode",
             "req loadSoundTable"]
    for _ in range(PLAYS):
        lines += ["req playSound 0x2c 100 0 0 0", "ticks 2000", "req playSound 0x80 100 0 0 0", "ticks 2000"]
    p = tmp / "p3_idle.script"
    p.write_text("\n".join(lines) + "\n")
    return p


def _run(tmp: pathlib.Path, arch: str, *extra: str) -> dict:
    st = tmp / f"stats_{arch}_{len(extra)}.json"
    r = run_check(arch, "script", "--script", str(_script(tmp)), "--mode", "capture", "--stats", str(st), *extra)
    assert r.returncode == 0, r.stdout + r.stderr
    return json.loads(st.read_text())


@pytest.mark.parametrize("arch", ["x64", "Win32"])
def test_released_buffers_are_freed(tmp_path, arch):
    s = _run(tmp_path, arch)
    assert s["failed"] == 0 and s["first_error"] == "", s
    assert s["sink"]["buffers"] >= 2 * PLAYS, s["sink"]          # the engine recreated a buffer for (nearly) every play
    assert s["heap"]["chunks"] == 1, s["heap"]                    # the arena stays in its first 16 MB chunk
    assert s["heap_peak_arena_bytes"] < 16 << 20, s


def test_control_keeps_every_buffer(tmp_path):
    """RED: with nothing freed the same script needs several chunks (7 before P3)"""
    s = _run(tmp_path, "x64", "--negctl", "audio_exe.deviceHeap=1")
    assert s["heap"]["chunks"] >= 4, s["heap"]


def _many(tmp: pathlib.Path) -> pathlib.Path:
    """every slot 0x00..0x80 played once with a 60 s idle gap after it: sounds of many sizes, and mmio's own buffers"""
    lines = ["# P3: slots 0x00..0x80 in turn with 60 s idle gaps, capture mode", "req loadSoundTable"]
    for s in range(0x81):
        lines += [f"req playSound {s:#x} 100 0 0 0", "ticks 2000"]
    p = tmp / "p3_many.script"
    p.write_text("\n".join(lines) + "\n")
    return p


def test_many_sizes_and_mmio_buffers_are_freed(tmp_path):
    """the arena keeps only what the engine holds: before P3 a freed block served only a request of its exact rounded size and
    mmioClose kept mmio's own buffer (31,075,728 live bytes at the end, the control); now 1,362,212"""
    st, ctl = tmp_path / "many.json", tmp_path / "many_ctl.json"
    for out, extra in ((st, ()), (ctl, ("--negctl", "audio_exe.deviceHeap=1"))):
        r = run_check("x64", "script", "--script", str(_many(tmp_path)), "--mode", "capture", "--stats", str(out), *extra)
        assert r.returncode == 0, r.stdout + r.stderr
    s, c = json.loads(st.read_text()), json.loads(ctl.read_text())
    assert s["failed"] == 0 and s["sink"]["buffers"] == c["sink"]["buffers"], (s, c)
    assert s["heap"]["live_bytes"] < 4 << 20, s["heap"]
    assert c["heap"]["live_bytes"] > 20 << 20, c["heap"]     # RED: nothing freed


def test_unknown_negctl_is_refused(tmp_path):
    r = run_check("x64", "script", "--script", str(_script(tmp_path)), "--mode", "capture", "--negctl", "no.such.entry=1")
    assert r.returncode == 2, r.stdout + r.stderr
