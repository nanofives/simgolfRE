"""Record a SimGolf scenario with WinDbg Time Travel Debugging (TTD).

TTD recording needs administrator rights; the game and the harness do not. So the game is started
unelevated as usual (Game, Frida, shim), and ttd.exe is launched elevated once (one UAC prompt) to
-attach to that PID. Killing the game ends the recording.

Usage: py -3.12 re/tools/ttd_record.py [--scenario stories_load] [--seconds 20] [--load-wait 90]
Trace: log/ttd/<scenario>/golf_clean*.run (+ .idx after first open). Open with
  WinDbgX -z <file.run>   or query in WinDbg: dx @$cursession.TTD.Memory(0x543d10, 0x55d738, "r")

Scenarios:
  stories_load  main menu (attach here) -> Continue Saved Game stories5.sve -> camera over the course ->
                N seconds of play at x32. Covers the .sve load (FUN_0040b9b0 -> FUN_0040afa0(1)) and the
                first readers of every restored global (UNCERTAINTIES U-0004).
"""
import argparse, ctypes, pathlib, subprocess, sys, time

import imgcmp
import scenario
from game import Game

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "log" / "ttd"


def ttd_elevated(args):
    """ShellExecute 'runas' ttd.exe; returns once the UAC prompt is answered (>32 = launched)."""
    r = ctypes.windll.shell32.ShellExecuteW(None, "runas", "ttd.exe", subprocess.list2cmdline(args), None, 0)
    if r <= 32:
        raise RuntimeError(f"elevated ttd.exe not launched (ShellExecute {r}; UAC declined?)")


def ttd_running():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq TTD.exe", "/NH"], capture_output=True, text=True).stdout
    return "ttd.exe" in out.lower()


def wait_trace_started(out_dir, timeout=60):
    t0 = time.time()
    while time.time() - t0 < timeout:
        runs = list(out_dir.glob("*.run"))
        if runs and runs[0].stat().st_size > 0:
            return runs[0]
        time.sleep(0.5)
    raise TimeoutError(f"no .run file in {out_dir} after {timeout} s")


def stories_load(g, seconds, load_wait):
    # Under TTD the game runs far slower: on 2026-10-03 the frame after load_saved_game's 8 s + 20 s of
    # play still showed "Loading..." on the Load Previous Game screen (both FUN_0040afa0(1) calls were in the trace).
    scenario.load_saved_game(g, scenario.STORIES_FIXTURE)
    time.sleep(load_wait)
    scenario.view_stories_course(g)
    t0 = time.time()
    while time.time() - t0 < seconds:  # Esc clears event popups that hold game time (see run_season)
        time.sleep(2); g.key(0x1B)


SCENARIOS = {"stories_load": {"env": {"SIMGOLF_TIMEWARP": "32"}, "run": stories_load}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", default="stories_load", choices=SCENARIOS)
    ap.add_argument("--seconds", type=float, default=20)
    ap.add_argument("--load-wait", type=float, default=90, help="extra seconds after clicking Load (TTD slowdown)")
    a = ap.parse_args()
    spec = SCENARIOS[a.scenario]
    out_dir = OUT / a.scenario
    out_dir.mkdir(parents=True, exist_ok=True)
    for old in out_dir.glob("*"):
        old.unlink()
    with Game(env={"SIMGOLF_SKIP_INTRO": "1", **spec["env"]}) as g:
        g.wait_window()
        imgcmp.wait_for(g, imgcmp.load(scenario.GOLDEN / "main_menu.png"), timeout=60)
        print(f"game pid {g.pid}; requesting elevation for ttd.exe -attach")
        ttd_elevated(["-acceptEula", "-noUI", "-out", str(out_dir), "-attach", str(g.pid)])
        run = wait_trace_started(out_dir)
        print(f"recording -> {run.name}")
        time.sleep(2)
        spec["run"](g, a.seconds, a.load_wait)
        g.shot(out_dir / "last_frame.png")
    t0 = time.time()
    while ttd_running() and time.time() - t0 < 300:  # trace is finalized when the target is gone
        time.sleep(1)
    for p in sorted(out_dir.glob("*")):
        print(f"  {p.name:40s} {p.stat().st_size / 1e6:9.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
