"""S4.10: list a sound.dll class table's slots with the function each holds (thunks followed), its hooks.csv name and
whether the port registers it (snd.<name>), from the mirror DLL's registry.

    ~/bin/py -3.12 log/roadmap/S4/10_vtable_cover.py 0x5b304 [nslots]
"""
import csv
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "test"))
import sound_emu as E  # noqa: E402
from port.test import audio_stream as AS  # noqa: E402

DATA, RELOCS, SECS, EXPS, ENTRY, IBASE = E._raw(str(E.SOUND_DLL))
names = {}
with open(ROOT / "hooks.csv", newline="") as f:
    for r in csv.DictReader(f):
        if r["module"].lower() == "sound.dll":
            names[int(r["addr"], 16)] = r["name"]
gh = {}
with open(ROOT / "re" / "functions_ghidra_sound.dll.tsv", newline="") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        gh[int(r["entry"], 16)] = (r["name"], int(r["size"]))
lib = AS.load_lib()
reg = set(lib.functions())
tab = int(sys.argv[1], 16)
n = int(sys.argv[2]) if len(sys.argv) > 2 else 64
for k in range(n):
    v = struct.unpack_from("<I", DATA, tab + 4 * k)[0] - IBASE
    if not (0x1000 <= v < 0x5b000):
        break
    t = v
    for _ in range(8):
        if DATA[t] == 0xE9:
            t = (t + 5 + struct.unpack_from("<i", DATA, t + 1)[0]) & 0xFFFFFFFF
        else:
            break
    nm = names.get(t) or gh.get(t, ("?", 0))[0]
    port = "snd." + nm in reg or ("snd." + gh.get(t, ("?",))[0]) in reg
    print(f"slot {k:2d} +0x{4*k:02x} 0x{t:05x} {nm:45s} size {gh.get(t, ('', 0))[1]:5d} {'PORT' if port else '----'}")
