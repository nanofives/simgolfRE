"""Negative controls of the sound.dll replay (step S4.1 gate part c; re/analysis/data/sound_replay.md).

Each control changes ONE thing in the replay of a key whose normal replay equals its path-1 CSV, and must read RED: some
field that matched the CSV in the normal replay (return text, state hash, state_changed) no longer matches, or a vector
stops with an error.

  flip     one input byte of the fixture flipped after the fixture ran (a byte the function reads, outside every state region)
  reloc    sound.dll relocated for base + 0x10000 but mapped at base (a wrong relocation base)
  stub     every non-void NativeCallback of the fixture (stand-in stubs, recording vtables) returns its value + 1

    ~/bin/py -3.12 re/tools/fixvm/controls.py [--out log/roadmap/S4/1_controls.tsv]
"""
from __future__ import annotations

import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))

import sound_emu as E  # noqa: E402

# (control, key, parameter): flip names the fixture value and the byte offset flipped
CONTROLS = [
    ("flip", "bitUnpack", ("ucnt0", 0)),
    ("flip", "Channel_setFormat_oom", ("f0", 0)),
    ("flip", "randRange", ("r0", 0)),                 # inside a state region: the trivial case, RED by the hash itself
    ("reloc", "getDsCoopLevel", 0x10000),
    ("reloc", "Seq_setTempo_w27f", 0x10000),
    ("reloc", "Midi_resetFileState_p1", 0x10000),
    ("stub", "Channel_setFormat_oom", 1),
    ("stub", "Seq_buildPlayOrder", 1),
    ("stub", "Midi_Device::openOutput_null", 1),
    ("stub", "crtValidatePointer_stand", 1),
]
# Stub controls that are equivalent mutants (recorded, not counted as RED): the fixture's callbacks there are recorders whose
# return value the function discards, or are never reached on the vectors (measured 2026-10-24: 28 of the first 40 EXACT
# import / replace keys, e.g. Midi_resetFileState_p1, ChanRecord_freeBuffers; sound_replay.md).
BLIND = [("stub", "Midi_resetFileState_p1", 1), ("stub", "ChanRecord_freeBuffers", 1)]


def matched_fields(res: E.KeyResult, csv: E.CsvKey) -> list:
    """(vector index, field, value) of every field of a replay that equals the CSV."""
    out = []
    for i, (r, c) in enumerate(zip(res.rows, csv.rows)):
        if r.error:
            continue
        if r.orig == c["original"]:
            out.append((i, "ret", r.orig))
        if r.state_orig is not None and r.state_orig == c["state_original"]:
            out.append((i, "state", r.state_orig))
        if r.state_changed is not None and r.state_changed == c["state_changed"]:
            out.append((i, "changed", r.state_changed))
    return out


def run_control(vm: E.FixVM, kind: str, key: str, param) -> dict:
    hooks, _ = E.registry()
    spec = hooks[key]
    csv = E.read_csv(spec, key)
    # the relocation control runs without DllMain (the CRT start-up would stop at the first relocated address it reads), and
    # so does its baseline
    kw = {"init": kind != "reloc"}
    base = E.KeyRun(vm, key, "A", **kw).run()
    good = matched_fields(base, csv)
    if kind == "flip":
        name, off = param

        def patch(run, fx):
            if name not in fx or "p" not in fx[name]:
                raise E.EmuError("fixture", f"control: fixture value {name} is not a pointer")
            a = fx[name]["p"] + off
            run.m.write(a, bytes([run.m.read(a, 1)[0] ^ 0x01]))
        kw["fixture_patch"] = patch
    elif kind == "reloc":
        kw["reloc_base"] = E.LAYOUTS["A"]["base"] + param
    elif kind == "stub":
        def patch(run, fx):
            r = run.vm.request({"op": "cbpatch", "delta": param})
            if not r.get("patched"):
                raise E.EmuError("fixture", "control: the fixture made no non-void NativeCallback")
        kw["fixture_patch"] = patch
    try:
        ctl = E.KeyRun(vm, key, "A", **kw).run()
    except E.EmuError as e:
        return {"control": kind, "key": key, "verdict": "RED", "lost": len(good), "of": len(good), "detail": f"setup: {e}"[:200]}
    if ctl.fixture_error:
        return {"control": kind, "key": key, "verdict": "RED", "lost": len(good), "of": len(good),
                "detail": f"fixture: {ctl.fixture_error}"[:200]}
    now = {(i, f): v for i, f, v in matched_fields(ctl, csv)}
    lost = [(i, f) for i, f, v in good if now.get((i, f)) != v]
    errs = sum(1 for r in ctl.rows if r.error)
    red = bool(lost) or errs > 0
    first = ""
    if lost:
        i, f = lost[0]
        first = f"v{i} {f}"
    elif errs:
        first = next(r.error for r in ctl.rows if r.error)
    return {"control": kind, "key": key, "verdict": "RED" if red else "GREEN (blind)", "lost": len(lost), "of": len(good),
            "errors": errs, "detail": first[:200]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S4" / "1_controls.tsv"))
    a = ap.parse_args()
    rows = []
    with E.FixVM() as vm:
        for n, (kind, key, param) in enumerate(CONTROLS + BLIND, 1):
            r = run_control(vm, kind, key, param)
            r["expect"] = "BLIND" if (kind, key, param) in BLIND else "RED"
            rows.append(r)
            print(f"[{n}/{len(CONTROLS) + len(BLIND)}] {kind} {key} {r['verdict']} (matched fields lost {r['lost']}/{r['of']}, "
                  f"errors {r.get('errors', '-')}) {r['detail']}", flush=True)
    cols = ["control", "key", "expect", "verdict", "lost", "of", "errors", "detail"]
    pathlib.Path(a.out).write_text("\t".join(cols) + "\n" + "".join("\t".join(str(r.get(c, "")) for c in cols) + "\n" for r in rows))
    return 0 if all(r["verdict"] == "RED" for r in rows if r["expect"] == "RED") else 1


if __name__ == "__main__":
    sys.exit(main())
