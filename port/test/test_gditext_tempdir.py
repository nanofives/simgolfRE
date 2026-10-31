"""P2 (2026-10-31): the GDI text provider survives a killed process (port/src/sim/gditext.cpp).

The defect: the provider wrote the game's two font files into %TEMP%\\sg_gditext_<pid>_<n> and registered them with
AddFontResourceA (session-wide). A process killed before its destructor left both .ttf LOCKED by the session, and a later
process that Windows gave the same pid picked the same directory (an existing one was accepted), failed to fopen the locked
file and stopped at record 0 with MissingResource{"text font file"}.

  (a) the directories the provider would pick next already exist and hold a LOCKED manu3_.ttf / klepto__.ttf: the provider
      must still work (fresh directory every time);
  (b) a child process builds a provider and is killed with TerminateProcess: it leaves no locked font file behind
      (FR_PRIVATE registration ends with the process).
Negative control: gditext negctl mode 8 (the old behaviour) reads RED on both. The child is `python -m
port.test.test_gditext_tempdir child <mode> <stage>`; the x64 backend only (under SG_BACKEND=win32 the provider's pid is the
runner's, not the child's).
"""
from __future__ import annotations

import ctypes
import os
import pathlib
import subprocess
import sys
import tempfile
import time

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

pytestmark = pytest.mark.port

DATA_VA = 0x004C1000
FILES = ("manu3_.ttf", "klepto__.ttf")


def _find(rel):
    for p in (ROOT / "original").rglob(rel):
        return p
    return None


def _skip_unless_ready():
    if os.name != "nt":
        pytest.skip("host GDI")
    if os.environ.get("SG_BACKEND", "x64") != "x64":
        pytest.skip("child-process test: x64 backend only")
    from port.test import test_gditext as T
    if T._segment_image() is None or not all(_find(f) for f in FILES):
        pytest.skip("needs a segment start image and the user's install (both font files)")


def _temp_base():
    return pathlib.Path(tempfile.gettempdir())


def _locked(path) -> bool:
    """True when the file cannot be opened for writing (a session-held font file)."""
    try:
        with open(path, "r+b"):
            return False
    except OSError:
        return True


def _child_main(mode: int):
    from port.test import lib as L, test_gditext as T, widths_check as WC
    lib = L.load()
    offs = {n: o for n, o, _ in lib.objects()}
    img = T._segment_image()
    if mode:
        lib.negctl("gditext.fontWidth", mode)
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        st.files_root(WC.INSTALL)
        print("READY", os.getpid(), flush=True)
        sys.stdin.readline()                     # the parent creates the trap directories now
        st.call("gditext.attach", [])
        try:
            for name in T.FONTS:                 # the two font-file-backed fonts register their files
                for k in range(5):
                    st.call("gditext.fontMetrics", [DATA_VA + offs[name], k])
            print("DONE", flush=True)
            sys.stdin.readline()                 # wait to be killed
        except Exception as e:                   # noqa: BLE001
            print("FAIL", type(e).__name__, e, flush=True)
            sys.stdin.readline()


def _start_child(mode):
    p = subprocess.Popen([sys.executable, "-m", "port.test.test_gditext_tempdir", "child", str(mode)], cwd=str(ROOT),
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    line = p.stdout.readline().split()
    assert line and line[0] == "READY", line
    return p, int(line[1])


class _Lock:
    """An exclusive handle (share mode 0) so that fopen(path, "wb") fails exactly as on a session-registered font."""

    def __init__(self, path):
        k = ctypes.windll.kernel32
        k.CreateFileW.restype = ctypes.c_void_p
        h = k.CreateFileW(str(path), 0x80000000 | 0x40000000, 0, None, 3, 0x80, None)
        assert h not in (None, ctypes.c_void_p(-1).value), path
        self.h = h

    def close(self):
        ctypes.windll.kernel32.CloseHandle(ctypes.c_void_p(self.h))


def _run_trap_case(mode):
    srcs = {f: _find(f).read_bytes() for f in FILES}
    p, pid = _start_child(mode)
    dirs, locks = [], []
    try:
        for n in (1, 2, 3):
            d = _temp_base() / f"sg_gditext_{pid}_{n}"
            d.mkdir()
            dirs.append(d)
            for f in FILES:
                (d / f).write_bytes(srcs[f])
                locks.append(_Lock(d / f))
        assert all(_locked(d / f) for d in dirs for f in FILES)
        p.stdin.write("go\n")
        p.stdin.flush()
        res = p.stdout.readline().split()
        return res[0] if res else "EOF"
    finally:
        p.terminate()
        p.wait(timeout=30)
        for l in locks:
            l.close()
        for d in dirs:
            for f in d.glob("*"):
                try:
                    f.unlink()
                except OSError:
                    pass
            try:
                d.rmdir()
            except OSError:
                pass


def test_existing_locked_directory_does_not_stop_the_provider():
    _skip_unless_ready()
    assert _run_trap_case(0) == "DONE"


def test_negctl_old_behaviour_fails_on_the_locked_directory():
    _skip_unless_ready()
    assert _run_trap_case(8) != "DONE"


def _run_kill_case(mode):
    p, pid = _start_child(mode)
    mine = []
    try:
        p.stdin.write("go\n")
        p.stdin.flush()
        res = p.stdout.readline().split()
        assert res and res[0] == "DONE", res
        mine = sorted(_temp_base().glob(f"sg_gditext_{pid}_*"))
        files = [d / f for d in mine for f in FILES if (d / f).exists()]
        assert files, "the provider wrote no font file"
    finally:
        p.kill()                                   # TerminateProcess: no destructor runs
        p.wait(timeout=30)
    time.sleep(0.5)
    locked = [str(f) for f in files if _locked(f)]
    for f in files:                                 # only this run's own files
        try:
            f.unlink()
        except OSError:
            pass
    for d in mine:
        try:
            for x in d.glob("*"):
                x.unlink()
            d.rmdir()
        except OSError:
            pass
    return locked


def test_killed_process_leaves_no_locked_font_file():
    _skip_unless_ready()
    assert _run_kill_case(0) == []


def test_negctl_session_registration_leaves_locked_files():
    _skip_unless_ready()
    assert _run_kill_case(8) != []


if __name__ == "__main__" and len(sys.argv) >= 3 and sys.argv[1] == "child":
    _child_main(int(sys.argv[2]))
