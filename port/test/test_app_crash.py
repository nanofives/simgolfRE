"""P33D fix 2 (Mariano's fifteenth play-test, item 5): the app's last-chance handler and the sim-stop line (port/app/crash.h).

Until P33D a session the sim stopped (`missing: boundIndex` in his recording 20261117-001527-18648) closed the window like a quit
with exit code 0 and nothing in simgolf_port.log, and a fault on any thread left no dump and no line either.

  1. --negctl crash-av: a thread writes through a null pointer 1.5 s after READY. The process ends with the access violation's
     code, stderr (the log when the app has no handles) carries `CRASH: exception 0xc0000005 (access violation, write to 0x0)
     at simgolf_port.exe+0x...`, and <data>/crashes holds one non-empty .dmp that the line names.
  2. --negctl crash-terminate: a thread's function throws (std::terminate -> abort): a `CRASH: abort` line and a dump.
  3. SG_TICK_CALL_LIMIT (the tick's development stop): the sim stops with `no return`; the window closes and stderr carries
     `SIM STOPPED: code -10 (no return: SG_TICK_CALL_LIMIT reached ...)` with the record directory.
  Control: SG_APP_CRASHDUMP=off with crash-av: no dump and no CRASH line (the process still ends with the fault).

    ~/bin/py -3.12 -m pytest port/test/test_app_crash.py -p no:cacheprovider
"""
from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import tempfile

import pytest

from port.test import build as B

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
EXE = B.build_dir("x64") / "app" / "Release" / "simgolf_port.exe"
AV = 0xC0000005


@pytest.fixture(scope="module", autouse=True)
def _no_fault_dialog():
    """the app inherits the error mode: with the handler off (the control) Windows' fault dialog would hold the run"""
    import ctypes
    k = ctypes.windll.kernel32
    old = k.SetErrorMode(0)
    k.SetErrorMode(old | 0x0002)  # SEM_NOGPFAULTERRORBOX
    yield
    k.SetErrorMode(old)


@pytest.fixture()
def tmp():
    if not EXE.exists():
        pytest.skip(f"no app build ({EXE}: port/test/build.py --arch x64)")
    if not (INSTALL / "golf_clean.exe").exists():
        pytest.skip("no install (original/golf_clean.exe)")
    d = pathlib.Path(tempfile.mkdtemp(prefix="p33d_"))
    yield d
    shutil.rmtree(d, ignore_errors=True)


def _run(tmp: pathlib.Path, negctl: str | None, env_extra: dict | None = None, timeout: float = 180):
    args = [str(EXE), "--install", str(INSTALL), "--data", str(tmp / "data"), "--record-dir", str(tmp / "rec"), "--no-activate"]
    if negctl:
        args += ["--negctl", negctl]
    env = dict(os.environ)
    env.update(env_extra or {})
    r = subprocess.run(args, capture_output=True, text=True, cwd=str(ROOT), timeout=timeout, env=env)
    dumps = sorted((tmp / "data" / "crashes").glob("*.dmp")) if (tmp / "data" / "crashes").is_dir() else []
    return r, dumps


def test_access_violation_writes_a_dump_and_a_line(tmp):
    r, dumps = _run(tmp, "crash-av")
    assert r.returncode & 0xFFFFFFFF == AV, (r.returncode, r.stderr[-2000:])
    m = re.search(r"CRASH: exception 0xc0000005 \(access violation, write to 0x0\) at simgolf_port\.exe\+0x[0-9a-f]+ "
                  r"\(thread \d+\); minidump (.+\.dmp); record ", r.stderr)
    assert m, r.stderr[-2000:]
    assert len(dumps) == 1 and dumps[0].stat().st_size > 4096, dumps
    assert pathlib.Path(m.group(1)).name == dumps[0].name
    assert dumps[0].read_bytes()[:4] == b"MDMP"


def test_terminate_writes_a_dump_and_a_line(tmp):
    r, dumps = _run(tmp, "crash-terminate")
    assert r.returncode == 3, (r.returncode, r.stderr[-2000:])
    assert re.search(r"CRASH: abort \(std::terminate: .*\) \(thread \d+\); minidump .+\.dmp; record ", r.stderr), r.stderr[-2000:]
    assert len(dumps) == 1 and dumps[0].read_bytes()[:4] == b"MDMP", dumps


def test_sim_stop_writes_a_line(tmp):
    r, dumps = _run(tmp, None, {"SG_TICK_CALL_LIMIT": "20000"})
    m = re.search(r"SIM STOPPED: code -10 \(no return: SG_TICK_CALL_LIMIT reached[^\n]*\) after \d+ records, stop error; "
                  r"the window was closed; record (.+)", r.stderr)
    assert m, (r.returncode, r.stderr[-2000:])
    assert pathlib.Path(m.group(1).strip()) == tmp / "rec"
    assert not dumps, "a sim stop is not a crash: no dump"


def test_control_handler_off_writes_nothing(tmp):
    r, dumps = _run(tmp, "crash-av", {"SG_APP_CRASHDUMP": "off"})
    assert r.returncode & 0xFFFFFFFF == AV, (r.returncode, r.stderr[-2000:])
    assert "CRASH:" not in r.stderr and not dumps
