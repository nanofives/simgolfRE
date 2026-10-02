"""Ghidra layer: the master project exists, analysis landed, the pool works, and the MCP server speaks.

Runs against a pool slot (never the master) so it can run while someone has the master open.
"""
import json
import os
import pathlib
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.ghidra

GHIDRA = pathlib.Path(r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC")
HEADLESS = GHIDRA / "support" / "analyzeHeadless.bat"
MCP = pathlib.Path(r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\headless-mcp\ghidra_headless_mcp.py")
GIT_BASH = r"C:\Program Files\Git\bin\bash.exe"
POOL = ROOT / "scripts" / "ghidra_pool.sh"

# Addresses established from the live process (see re/analysis/PORTABLE_RUNTIME.md).
KNOWN = {
    "0x004a682f": "entry",       # PE entry point (CRT WinMainCRTStartup)
    "0x0045baf0": "WinMain",     # called from 0x004a690a with (hInst, 0, cmdline, nShow)
}


def _pool(*args):
    r = subprocess.run([GIT_BASH, POOL.as_posix(), *args], capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr
    return r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""


@pytest.fixture(scope="module")
def slot():
    s = _pool("acquire")
    assert s.startswith("SimGolf_pool"), s
    yield s
    _pool("release", s.replace("SimGolf_pool", ""))


def test_master_project_exists():
    assert (ROOT / "SimGolf.gpr").exists()
    assert (ROOT / "SimGolf.rep").is_dir()


@pytest.mark.parametrize("program,min_functions", [("golf_clean.exe", 1500), ("jgld.dll", 500)])
def test_headless_facts(slot, program, min_functions):
    cmd = [str(HEADLESS), str(ROOT / "simgolf_pool"), slot, "-process", program, "-readOnly", "-noanalysis",
           "-scriptPath", str(ROOT / "ghidra" / "scripts"), "-postScript", "ExportFacts.java", *KNOWN]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    line = next((l for l in r.stdout.splitlines() if "FACTS:" in l), None)
    assert line, r.stdout[-3000:] + r.stderr[-2000:]
    facts = json.loads(line.split("FACTS:", 1)[1].split(" (GhidraScript)")[0].strip())
    assert facts["program"] == program
    assert facts["function_count"] >= min_functions, facts
    if program == "golf_clean.exe":
        assert facts["image_base"] == "00400000"
        for addr in KNOWN:
            assert facts["entry"][addr] is not None, f"no function at {addr}"


def test_mcp_server_lists_tools():
    """Speak MCP over stdio: initialize + tools/list. Proves the server Claude uses is wired."""
    p = subprocess.Popen([sys.executable, str(MCP), "--ghidra-install-dir", str(GHIDRA)],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    try:
        def call(i, method, params=None):
            p.stdin.write(json.dumps({"jsonrpc": "2.0", "id": i, "method": method, "params": params or {}}) + "\n")
            p.stdin.flush()
            while True:
                msg = json.loads(p.stdout.readline())
                if msg.get("id") == i:
                    return msg
        init = call(1, "initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                                       "clientInfo": {"name": "simgolf-tests", "version": "0"}})
        assert "result" in init, init
        p.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n")
        p.stdin.flush()
        # Wire names are dotted (project.program.open_existing); Claude shows them with underscores.
        tools = {t["name"].replace(".", "_") for t in call(2, "tools/list")["result"]["tools"]}
        assert {"project_program_open_existing", "decomp_function", "function_at"} <= tools
    finally:
        p.kill()
