r"""Roadmap S8.3: the intro-ON boot scenario for the port's boot gate (log/roadmap/S8/3_notes.md, re/analysis/video/skip_path.md).

S8.2's intro-ON recordings (cov_r_s8_intro, cov_r_s8_full) stay on the title menu, so they hold no mainLoop frame head and the boot
gate (port/test/boot.py) has no per-frame record to compare. This tool adds one scenario that plays the intro and then reaches the
frame loop:

  cov_r_s8_play   process start with SIMGOLF_SKIP_INTRO=0 (SMSG_introfinal.bik played by binkw32 from showTitleScreen through
                  playAudioFile), checkpoint `intro_early` 2 virtual s after the start, then cov_r_quit's first session: Sandbox,
                  one hole, Exit the Game -> "So long for now." (the quit save), the title, the title menu's exit icon (mainLoop
                  QUIT: top10.sve, promptAndSaveFile, the closing movie SMSG_closingfinal.bik, the credits), Esc, process exit.
                  The install's files are collected at the start (run_a) and before the credits (end), as cov_r_quit does.

It is a cov_r_* scenario, so scenario_refs' s36a family restores the pristine saves / themes / score table before every boot,
and it has `expect_exit`, so the exit support is loaded. Runs go through re/tools/s8_movie_runs.py (S8.2's driver: the scenario
module is imported first, SIMGOLF_SKIP_INTRO goes into the game environment) on instances\s8_3 (never original\).

    export SIMGOLF_ALLOW_MULTI=1 SIMGOLF_GAME_DIR=instances/s8_3
    ~/bin/py -3.12 re/tools/s8_boot_capture.py run --label rec --record           # records the schedule
    ~/bin/py -3.12 re/tools/s8_boot_capture.py run --label c1 --framehash         # replays with per-frame sim hashes (x2: c1, c2)
    ~/bin/py -3.12 re/tools/s8_boot_capture.py trace --label t1                   # the clock / input trace (trace_tool's recorder)
    ~/bin/py -3.12 re/tools/s8_boot_capture.py check c1 c2 --trace t1             # determinism + trace FRAME count = capture records
    ~/bin/py -3.12 re/tools/s8_boot_capture.py install c1 --trace t1             # -> log/p2/p24/<scen>/c_a/, port/corpus/traces/<scen>.trace

(`install` writes only the gitignored capture / trace locations boot.py reads; nothing tracked.)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
for _p in (ROOT / "log" / "c3", ROOT / "re" / "frida", ROOT / "re" / "tools"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

SCENARIO = "cov_r_s8_play"
INSTANCE = "s8_3"
REFS = ROOT / "log" / "c3" / "alt" / "refs"
CAPTURES = ROOT / "log" / "p2" / "p24"
TRACES = ROOT / "port" / "corpus" / "traces"
TRACE_RUNS = ROOT / "log" / "roadmap" / "S8" / "runs" / "s8_3"


# ================================================================ the scenario
def register():
    """Adds cov_r_s8_play to scenario.SCENARIOS (scenario_refs' s36a family picks up cov_r_* lockstep entries from it)."""
    import scenario as SC

    def cov_r_s8_play(c):
        yield from c.sleep(2000)
        yield from c.checkpoint("intro_early")                      # with the intro ON: already past the movie (it adds no T)
        yield from c.wait_screen(SC.imgcmp.load(SC.GOLDEN / "main_menu.png"), name="main_menu")
        SC._cov_r_collect(c, "run_a", extra=SC.R_QUIT_DIRS, root_files=("top10.sve",))
        yield from SC._cova_to_sandbox(c)                           # menu checkpoint, Sandbox, Monterey, the HUD, Esc
        yield from c.click(45, 470); yield from c.sleep(1500)       # Build Course palette
        yield from SC._cova_hole(c, (300, 200), (520, 330))
        yield from c.click(45, 470); yield from c.sleep(1500)
        yield from c.checkpoint("game")
        yield from SC._cova_exit_menu(c)
        yield from c.checkpoint("exit_prompt")
        yield from c.click(260, 211); yield from c.sleep(4000)      # "So long for now." -> quit save -> title
        yield from c.checkpoint("exit_leave")
        for _ in range(4):
            yield from c.sleep(1000)
        yield from c.checkpoint("title")
        c.move(750, 553); yield from c.sleep(1500)
        c.run.exit_expected = True
        yield from c.click(750, 553); yield from c.sleep(3000)      # the title menu's exit icon: QUIT, the closing movie
        for i in range(4):
            yield from c.checkpoint(f"quit_{i:02d}")
            yield from c.sleep(2000)
        SC._cov_r_collect(c, "end", extra=SC.R_QUIT_DIRS, root_files=("top10.sve",))
        yield from c.checkpoint("credits")
        yield from c.key(0x1B); yield from c.sleep(1500)             # the credits end on a key
        for _ in range(300):
            yield from c.sleep(1000)

    cov_r_s8_play.expect_exit = True
    SC.SCENARIOS[SCENARIO] = {"env": {}, "phases": [], "lockstep": cov_r_s8_play}


# ================================================================ helpers (pure; port/test/test_video.py imports them)
def read_framehash(run: pathlib.Path):
    """(symbols, [(T, frame index, [sgh64 ...])]) of a run folder's framehash.bin / framehash.json"""
    meta = json.loads((run / "framehash.json").read_text())
    n = len(meta["symbols"])
    raw = (run / "framehash.bin").read_bytes()
    rec = 8 + 8 * n
    out = []
    for i in range(0, len(raw) - rec + 1, rec):
        t, k = struct.unpack_from("<II", raw, i)
        out.append((t, k, list(struct.unpack_from("<%dQ" % n, raw, i + 8))))
    return meta["symbols"], out


def trace_frames(path: pathlib.Path) -> list[tuple[int, int]]:
    """[(frame index, T)] of the trace's FRAME records"""
    import trace_format as TF
    tr = TF.read(path)
    out = []
    for r in tr.records:
        if TF.fmt_record(r).split()[1:2] == ["FRAME"]:
            d = TF.decode(r)
            out.append((d.get("frame"), d.get("t")))
    return out


def sha(p: pathlib.Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


# ================================================================ commands
def _delegate(tool: str, rest: list[str]) -> int:
    """S8.2's driver with this scenario registered and the intro ON"""
    register()
    import s8_movie_runs as M
    sys.argv = [str(ROOT / "re" / "tools" / "s8_movie_runs.py"), "--skip-intro", "0", tool] + rest
    return M.main()


def cmd_run(a) -> int:
    if a.framehash:
        os.environ["C3FT_FRAMEHASH"] = "sim"
        os.environ["C3FT_OBJHASH"] = "1"
    os.environ.setdefault("C3FT_AUDIO", "none")
    rest = ["run", "--instance", a.instance, "--scenario", SCENARIO, "--label", a.label]
    if a.record:
        rest.append("--record")
    return _delegate("ref", rest)


def cmd_trace(a) -> int:
    os.environ.setdefault("C3FT_AUDIO", "none")
    for k in ("C3FT_FRAMEHASH", "C3FT_OBJHASH"):
        os.environ.pop(k, None)
    TRACE_RUNS.mkdir(parents=True, exist_ok=True)
    out = TRACE_RUNS / f"{a.label}.trace"
    return _delegate("trace", ["record", "--instance", a.instance, "--scenario", SCENARIO, "--label", a.label, "--out", str(out)])


def cmd_check(a) -> int:
    runs = [REFS / SCENARIO / lab for lab in a.labels]
    first = read_framehash(runs[0])
    ok = True
    print(f"{a.labels[0]}: {len(first[1])} frame records, T {first[1][0][0] if first[1] else None}..{first[1][-1][0] if first[1] else None}")
    for lab, run in zip(a.labels[1:], runs[1:]):
        other = read_framehash(run)
        same = other == first
        ok &= same
        print(f"{lab}: {len(other[1])} frame records, identical to {a.labels[0]}: {same}")
    for lab, run in zip(a.labels, runs):
        s = json.loads((run / "summary.json").read_text())
        print(f"{lab}: checkpoints {len(s['checkpoints'])} exit {s.get('exit_code', s.get('exit'))}")
    if a.trace:
        tf = trace_frames(TRACE_RUNS / f"{a.trace}.trace")
        cap = [(k, t) for t, k, _h in first[1]]
        same = tf == cap
        ok &= same
        print(f"trace {a.trace}: {len(tf)} FRAME records; (index, T) equal to the capture's records: {same}")
        if not same:
            for i, (x, y) in enumerate(zip(tf, cap)):
                if x != y:
                    print(f"  first difference at record {i}: trace {x} capture {y}")
                    break
    return 0 if ok else 1


def cmd_install(a) -> int:
    run = REFS / SCENARIO / a.label
    dst = CAPTURES / SCENARIO / "c_a"
    dst.mkdir(parents=True, exist_ok=True)
    for f in ("framehash.bin", "framehash.json", "summary.json"):
        shutil.copy2(run / f, dst / f)
    src = TRACE_RUNS / f"{a.trace}.trace"
    TRACES.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, TRACES / f"{SCENARIO}.trace")
    side = src.parent / (src.name + ".json")
    if side.exists():
        shutil.copy2(side, TRACES / f"{SCENARIO}.trace.json")
    (dst / "s8_3_install.json").write_text(json.dumps({
        "run": str(run.relative_to(ROOT)), "trace": str(src.relative_to(ROOT)),
        "framehash_sha256": sha(dst / "framehash.bin"), "trace_sha256": sha(TRACES / f"{SCENARIO}.trace")}, indent=1))
    print(f"installed {dst} and {TRACES / (SCENARIO + '.trace')}")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run")
    p.add_argument("--label", required=True)
    p.add_argument("--instance", default=INSTANCE)
    p.add_argument("--record", action="store_true")
    p.add_argument("--framehash", action="store_true")
    p = sp.add_parser("trace")
    p.add_argument("--label", required=True)
    p.add_argument("--instance", default=INSTANCE)
    p = sp.add_parser("check")
    p.add_argument("labels", nargs="+")
    p.add_argument("--trace")
    p = sp.add_parser("install")
    p.add_argument("label")
    p.add_argument("--trace", required=True)
    a = ap.parse_args(argv)
    return {"run": cmd_run, "trace": cmd_trace, "check": cmd_check, "install": cmd_install}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
