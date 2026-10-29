"""S4.10 gate (c) controls: each function's negative control (mode 1) on the device-present run, one subprocess per control
(120 s each; a control that makes the engine loop counts as a TIMEOUT). RED = the stream digest differs from the pinned
`$device` entry of port/test/golden_audio_exe.json, or the chain stops with an error.

    ~/bin/py -3.12 log/roadmap/S4/10_device_negctl.py [fn ...]
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
PY = sys.executable
FNS = ["playSound", "Snd::play", "Snd::open", "Snd::applyMode484e30", "loadSoundTable", "initSound", "shutdownSoundSystem",
       "snd.FUN_1000e830", "Snd::setVolume", "Snd::configure", "Snd::start", "soundVersionCheck", "Snd::setPan",
       "Snd::stop484f00", "Snd::ctorDerived", "createSound4840e0", "Snd::busy"]
CHILD = """
import sys, json
sys.path.insert(0, {root!r})
from port.test import audio_stream as AS
from port.test import lib as L
lib = AS.load_lib()
g = json.loads(AS.GOLDEN.read_text())["$device"]["stream_sha256"]
fn = {fn!r}
try:
    _, _, ln = AS.device_run(lib, neg=(fn, 1) if fn else None)
    print("RED" if AS.device_sha(ln) != g else "GREEN")
except L.SgError as e:
    print("RED_STOPS " + str(e)[:80])
"""


def main():
    fns = sys.argv[1:] or [""] + FNS
    for fn in fns:
        try:
            r = subprocess.run([PY, "-c", CHILD.format(root=str(ROOT), fn=fn)], capture_output=True, text=True, timeout=120)
            v = (r.stdout.strip().splitlines() or ["ERR " + r.stderr.strip()[-200:]])[-1]
        except subprocess.TimeoutExpired:
            v = "TIMEOUT"
        print(f"{fn or '(none)'}\t{v}", flush=True)


if __name__ == "__main__":
    main()
