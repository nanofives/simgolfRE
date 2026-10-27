"""Roadmap S7.2: the Windows host's input translation (port/app/input_map.h) judged on every MSG record of the corpus traces.

The translator maps one window message (msg, wParam, lParam) plus the host's key / cursor / MapVirtualKeyA answers to the
root-handler calls and reads the original took (re/model/trace_schema.md: MSG, HANDLER, KEYSTATE, MAPVK, CURSOR). The checker
exe port/app/tools/input_check.cpp feeds it each recorded message and compares what it returns with the records that follow
the MSG: 100 % of HANDLER records (id, a0, a1, the click's Ctrl / Shift), every owned read with its answer, the focus child's
keys (handlers 12 / 13) as tick.cpp parse_trace derives them, and no HANDLER outside a translated message's window.

    ~/bin/py -3.12 -m pytest port/test/test_app_input.py

Controls (checker --negctl N, all RED): 1 left / right buttons swapped, 2 WM_CHAR and WM_KEYDOWN handlers swapped, 3 y flipped,
4 y offset by the caption height, 5 Ctrl bit of the click dropped, 6 the focused child never takes a key, 7 a key-down always
onChar, 8 WM_CHAR's code is its own character, 9 Shift bit dropped. Control 10 (the host window's scale ignored) is an
equivalent mutant on the corpus' identity window and reads RED with --viewport (a scaled window): recorded as blind there.
"""
from __future__ import annotations

import json
import pathlib
import shutil
import subprocess

import pytest

from port.test import build as B

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
APP = ROOT / "port" / "app"
TRACES = ROOT / "port" / "corpus" / "traces"
BUILD = ROOT / "port" / "build" / "S7_2_app"
EXE = BUILD / "Release" / "simgolf_input_check.exe"

# the message ids the corpus holds: WM_KEYDOWN / KEYUP / CHAR, WM_MOUSEMOVE, WM_L/RBUTTONDOWN / UP
CORPUS_MSGS = {0x100, 0x101, 0x102, 0x200, 0x201, 0x202, 0x204, 0x205}


def _sources():
    return [p for p in APP.rglob("*") if p.suffix in (".cpp", ".h", ".txt")]


@pytest.fixture(scope="session")
def checker():
    if not B.toolchain_ok():
        pytest.skip("no CMake / Visual Studio 2022 Build Tools")
    if not EXE.exists() or any(s.stat().st_mtime > EXE.stat().st_mtime for s in _sources()):
        cm = B.cmake_exe()
        subprocess.run([cm, "-S", str(APP), "-B", str(BUILD), "-A", "x64"], check=True, capture_output=True)
        r = subprocess.run([cm, "--build", str(BUILD), "--config", "Release"], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout[-2000:]
    return EXE


def _traces():
    files = sorted(TRACES.glob("*.trace")) if TRACES.exists() else []
    if not files:
        pytest.skip("no corpus traces under port/corpus/traces")
    return [str(f) for f in files]


def _run(exe, tmp_path, *args):
    out = tmp_path / "result.json"
    r = subprocess.run([str(exe), *args, "--json", str(out), "--list", "5", *_traces()], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout[-2000:] + r.stderr[-2000:]
    return json.loads(out.read_text())


def test_selftest(checker):
    r = subprocess.run([str(checker), "--selftest"], capture_output=True, text=True)
    assert r.returncode == 0 and "0 failed" in r.stdout, r.stdout


def test_every_msg_record_of_the_corpus(checker, tmp_path):
    res = _run(checker, tmp_path)
    assert res["mismatches"] == 0, res["listed"]
    assert res["traces"] >= 64
    assert res["msgs"] >= 26000 and res["translated"] == res["msgs"]
    # every HANDLER record of every trace lies in a translated message's window and was produced by the translator
    assert res["handlers"] == res["handlers_in_traces"] >= 23000
    assert res["reads"] >= 42000 and res["cursor_reads"] >= 5600
    assert {int(k) for k in res["by_msg"]} <= CORPUS_MSGS
    assert set(CORPUS_MSGS) <= {int(k) for k in res["by_msg"]}
    assert res["focus_cmds"] >= 1


def test_mouse_move_cursor_read_follows_the_cursor_not_the_message(checker, tmp_path):
    """The window procedure's cursor read returns the host's cursor; in the corpus it equals the message position except where
    the harness moved its virtual cursor separately (4 scripted cases, listed in the notes)."""
    res = _run(checker, tmp_path)
    assert res["mousemove_cursor_eq_lp"] >= 5600 and res["mousemove_cursor_ne_lp"] <= 8


@pytest.mark.parametrize("viewport", ["1600,1200,8,31", "1000,750,0,0", "1920,1440,3,40"])
def test_scaled_host_window_gives_the_same_logical_input(checker, tmp_path, viewport):
    """A host window larger than 800x600 (or with a non-client origin): mouse messages and the cursor arrive in physical client
    pixels and the translator must hand the root handlers the same logical coordinates the trace records."""
    res = _run(checker, tmp_path, "--viewport", viewport)
    assert res["mismatches"] == 0, res["listed"]
    assert res["handlers"] == res["handlers_in_traces"]


# control, what it gets wrong, the msg ids that must be among the listed mismatches' messages
CONTROLS = [
    (1, "buttons swapped", {0x201, 0x202, 0x204, 0x205}),
    (2, "WM_CHAR / WM_KEYDOWN handler shapes swapped", {0x100, 0x102}),
    (3, "y flipped", {0x200, 0x201, 0x202}),
    (4, "y offset by the caption height", {0x200, 0x201, 0x202}),
    (5, "Ctrl bit dropped", {0x202}),
    (6, "focus child never takes a key", {0x102}),
    (7, "a key-down is always onChar", {0x100}),
    (8, "WM_CHAR code is its own character", {0x102}),
    (9, "Shift bit dropped", {0x202}),
]


@pytest.mark.parametrize("n,what,msgs", CONTROLS, ids=[c[1] for c in CONTROLS])
def test_negative_control_reads_red(checker, tmp_path, n, what, msgs):
    res = _run(checker, tmp_path, "--negctl", str(n))
    assert res["mismatches"] > 0, f"control {n} ({what}) read GREEN"
    seen = {m["msg"] for m in res["listed"]}
    assert seen & msgs, (n, what, res["listed"])


def test_viewport_control_is_blind_on_the_identity_window_and_red_on_a_scaled_one(checker, tmp_path):
    blind = _run(checker, tmp_path, "--negctl", "10")
    assert blind["mismatches"] == 0, "equivalent mutant on the identity viewport (recorded as blind)"
    red = _run(checker, tmp_path, "--negctl", "10", "--viewport", "1600,1200,8,31")
    assert red["mismatches"] > 0
