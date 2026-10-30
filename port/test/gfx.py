"""Phase 5, step 1 (re/PHASE5_GFX_PLAN.md): the graphics backend's frames against the original's checkpoint frames.

A checkpoint frame of the original is jgld's back buffer at the clock call where scenario_ab.py took the checkpoint (the
pinned references log/c3/alt/refs/<scenario>/det_b/<checkpoint>.png; summary.json gives the checkpoint's main-thread
timeGetTime count `status.tgt`). The port runs the scenario from process start (port/test/boot.py) with the backend
attached (gfx.attach) and a snapshot requested at that clock call's ordinal in the trace (gfx.snapshotAt); the snapshot is
read back from the files overlay and compared pixel by pixel (re/tools/imgcmp.py exact_diff), in the whole frame and per
region.

    ~/bin/py -3.12 -m port.test.gfx <scenario> <checkpoint>... [--out <dir>]   # writes <checkpoint>.port.png / .diff.png
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import sys
from dataclasses import dataclass, field

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import imgcmp  # noqa: E402
import trace_format as TF  # noqa: E402
from port.test import boot  # noqa: E402
from port.test import lib as L  # noqa: E402

REFS = ROOT / "log" / "c3" / "alt" / "refs"
W, H = 0x320, 0x258

# The title menu's regions (tick_menu.inc, showTitleScreen's button table in port/src/sim/gfx.cpp): each button's
# destination rectangle (the crop rectangle, drawn at the same point) and a box around each label's centre point.
MENU_BUTTONS = {
    "button_continue": (0x2a, 0x15, 0x144, 0x79), "button_new": (0x1a0, 0x1f, 0x16b, 0x8d),
    "button_theme": (0x1fe, 0x167, 0x10f, 0x6f), "button_champ": (0x11f, 0x1da, 0x120, 0x73),
    "button_sandbox": (0xd, 0x17e, 0x11f, 0x73), "button_exit": (0x2d1, 0x210, 0x3a, 0x39),
    "logo": (0xa8, 0xc6, 0x1d8, 0xb0),
}
MENU_LABELS = {   # (centre x, top y) of drawCenteredText's calls, a 240 x 26 box (two lines for the new-game label)
    "label_continue": (0xb6, 0x42), "label_new": (0x23d, 0x58), "label_theme": (0x277, 0x199),
    "label_champ": (0x19f, 0x20d), "label_sandbox": (0x8d, 0x1af),
}


def menu_regions() -> dict:
    regs = {}
    for k, (cx, ty) in MENU_LABELS.items():
        regs[k] = (cx - 120, ty, 240, 26 + (0x18 if k == "label_new" else 0))
    regs.update(MENU_BUTTONS)
    return regs


def checkpoint_ordinal(scenario: str, checkpoint: str) -> int:
    """The trace's clock ordinal of the checkpoint's clock call: the status.tgt-th main-thread timeGetTime call (0-based
    count of the calls before it), whose value must be the checkpoint's T."""
    summ = json.loads((REFS / scenario / "det_b" / "summary.json").read_text())
    st = summ["checkpoints"][checkpoint]["status"]
    tr = TF.read(boot.TRACES / f"{scenario}.trace")
    # roadmap S3.16: the trace's POLL record of the checkpoint gives the clock call the poll rode on (pos = the calls before
    # it). When that call is not a tgt call (jgld Palette::update's QueryPerformanceCounter reads at the start of a frame body
    # that reads no timeGetTime before H1's wait), status.tgt counts the tgt calls before it and the tgt-th call lies later,
    # in H1's wait: after that frame's present, while the reference frame is the back buffer before it (cov_k1_paths cart,
    # cov_k1_river r20: 7 census rows, U-1820)
    poll = next((r.f["pos"] for r in tr.records if r.type == TF.POLL and r.f.get("checkpoint") == checkpoint), None)
    k = 0
    for ordinal, _ra, _slot, kind, value in TF.clock_calls(tr):
        if kind != 1:
            continue
        if k == st["tgt"]:
            if value != st["T"]:
                raise ValueError(f"{scenario}/{checkpoint}: tgt call {k} has value {value}, the checkpoint's T is {st['T']}")
            if poll is not None and poll < ordinal and os.environ.get("SG_GFX_CHECKPOINT_AT", "poll") == "poll":
                return poll
            return ordinal
        k += 1
    raise ValueError(f"{scenario}/{checkpoint}: the trace has only {k} tgt calls")


def reference(scenario: str, checkpoint: str) -> np.ndarray:
    from PIL import Image
    return np.asarray(Image.open(REFS / scenario / "det_b" / f"{checkpoint}.png").convert("RGB"))


@dataclass
class GfxRun:
    scenario: str
    frames: dict = field(default_factory=dict)      # checkpoint -> h x w x 3 uint8 (None: no snapshot)
    log: str = ""
    unsupported: int = -1
    unsupported_before: dict = field(default_factory=dict)   # checkpoint -> distinct unsupported cases before its snapshot
    boot: object = None                              # boot.BootResult of the same run (sim hashes compared as usual)


def record_budget(scenario: str, last_ordinal: int, max_records: int | None = None) -> tuple[int, bool]:
    """(max_records, to_end) of a boot run that snapshots up to the clock ordinal `last_ordinal`.

    max_records = the FRAME records at or before it plus 2 (S0 recommendation 4, U-1114; framegate.py does the same): a
    checkpoint in a frame's H1 wait is snapshotted at the next cursor read or dispatch past it, which lies in the following
    frame's body, so the old `plus 1` stopped one frame short. to_end: the run goes on past the last FRAME record (the title
    menu after the session, the quit) to the end of the trace when the budget exceeds the trace's FRAME records, or when the
    capture has no frame record at all (cov_b_location stays on the location screen). An explicit max_records is kept."""
    no_frames = not (boot.expected_records(scenario)[1] or [])
    tr = TF.read(boot.TRACES / f"{scenario}.trace")
    frames_pos = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    del tr
    if max_records is None:
        max_records = sum(1 for p in frames_pos if p <= last_ordinal) + 2
    return max_records, no_frames or max_records > len(frames_pos)


def render(scenario: str, checkpoints: list[str], lib: L.Lib | None = None, negctl: int = 0,
           max_records: int | None = None, negctl_name: str = "gfx.attach") -> GfxRun:
    lib = lib or L.load()
    if not lib.has("gfx.attach"):
        raise RuntimeError("the library has no gfx module")
    ords = sorted((checkpoint_ordinal(scenario, c), c) for c in checkpoints)
    out = GfxRun(scenario)
    max_records, to_end = record_budget(scenario, ords[-1][0], max_records)

    def before(st):
        st.call("gfx.attach", [])
        for o, _c in ords:
            st.call("gfx.snapshotAt", [o])

    def after(st, _res):
        for k, (_o, c) in enumerate(ords):
            raw = st.files_read(f"$gfx/snapshot_{k}.555")
            if raw is None or len(raw) != W * H * 2:
                out.frames[c] = None
                continue
            out.frames[c] = imgcmp.rgb555_to_rgb(np.frombuffer(raw, "<u2").reshape(H, W))
        out.unsupported = st.call("gfx.log", []).eax
        txt = st.files_read("$gfx/log.txt")
        out.log = txt.decode() if txt else ""
        for line in out.log.splitlines():   # "snapshot <k> unsupported before it <n>"
            f = line.split("\t")
            if len(f) == 4 and f[0] == "snapshot":
                out.unsupported_before[ords[int(f[1])][1]] = int(f[3])
        st.call("gfx.detach", [])

    prev = lib.negctl(negctl_name, negctl) if negctl else None
    try:
        out.boot = boot.run(scenario, lib, max_records=max_records, before=before, after=after, to_end=to_end)
    finally:
        if negctl:
            lib.negctl(negctl_name, prev or 0)
    return out


def compare(run: GfxRun, checkpoint: str, regions: dict | None = None) -> dict:
    img = run.frames.get(checkpoint)
    if img is None:
        return {"total": -1, "regions": {}, "bbox": None, "why": "no snapshot"}
    return imgcmp.exact_diff(img, reference(run.scenario, checkpoint), regions)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenario")
    ap.add_argument("checkpoints", nargs="+")
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--out", type=pathlib.Path)
    a = ap.parse_args(argv)
    run = render(a.scenario, a.checkpoints, negctl=a.negctl)
    print(f"boot: {run.boot.status} {run.boot.detail}")
    print(run.log.strip())
    rc = 0
    for c in a.checkpoints:
        r = compare(run, c, menu_regions() if c == "menu" else None)
        print(f"{a.scenario}/{c}: {r['total']} px differ, bbox {r['bbox']}, regions {r['regions']}")
        rc |= r["total"] != 0
        if a.out and run.frames.get(c) is not None:
            from PIL import Image
            a.out.mkdir(parents=True, exist_ok=True)
            Image.fromarray(run.frames[c]).save(a.out / f"{c}.port.png")
            d = (run.frames[c] != reference(a.scenario, c)).any(axis=2)
            Image.fromarray((d * 255).astype(np.uint8)).save(a.out / f"{c}.diff.png")
    return rc


if __name__ == "__main__":
    sys.exit(main())
