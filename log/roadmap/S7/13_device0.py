"""S7.13: audio_stream's device run at sound.golfSfxAlias 0 and 1 on the S7_w4_root build, x64 (SG_BACKEND unset) or Win32
(SG_BACKEND=win32): lines, sha256, mmioOpen lines and alias notes. Run from port/build/S7_w4_root.
    ~/bin/py -3.12 <this> [out.json]"""
import json
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import audio_stream as AS  # noqa: E402
from port.test import lib as L  # noqa: E402

lib = L.load()
res = {}
for on in (True, False):
    prev = lib.defect("sound.golfSfxAlias", on)
    try:
        out, _rep, lines = AS.device_run(lib)
    finally:
        lib.defect("sound.golfSfxAlias", bool(prev))
    al = [x for x in lines if " (alias " in x]
    res[f"switch{int(on)}"] = {"steps": out, "lines": len(lines), "stream_sha256": AS.device_sha(lines),
                               "mmio": sum("mmioOpen" in x for x in lines), "aliased": len(al),
                               "music_aliased": sum(" (alias sounds/music/" in x for x in al),
                               "absent": sum("(absent)" in x for x in lines)}
print(json.dumps(res, indent=1))
if len(sys.argv) > 1:
    pathlib.Path(sys.argv[1]).write_text(json.dumps(res, indent=1))
