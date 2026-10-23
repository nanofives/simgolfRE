"""Snapshots of the ORIGINAL Terrain.dll's state side (phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 13), recorded by
re/tools/terrain_calls.py: the call log of every exported Terrain method from process start (calls.txt) and snapshots of the Terrain
object, the DLL's data image and the collar table, taken on the game thread at a Terrain::render entry (named after the scenario step)
or around single calls (<method>_<seq>.before / .after).

    ~/bin/py -3.12 -m port.test.terraincorpus export log/p5/terrain/t1 t1     # copy into port/corpus/terrain/t1 (local only)
"""
from __future__ import annotations

import json
import pathlib
import shutil
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import glcorpus as G  # noqa: E402

CORPUS = ROOT / "port" / "corpus" / "terrain"
COURSE_TYPE_RVA = 0x70A0C       # g_courseType in Terrain.dll's data (tooling only, like glcorpus.RVA)


class TSnap(G.Snap):
    """A state snapshot in the shape glcorpus.load takes (Terrain object, data image), plus the collar table and the log position."""

    def __init__(self, root: pathlib.Path, name: str):
        d = root / "snaps" / name
        self.step = name
        self.dir = d
        self.info = json.loads((d / "meta.json").read_text())
        self.meta = {}
        self.tobj = (d / "tobj.bin").read_bytes()
        self.tdata = (d / "tdata.bin").read_bytes()
        self.collar = (d / "collar.bin").read_bytes() if (d / "collar.bin").exists() else b""
        self.terrain = self.info["tobjAddr"]
        self.seq = self.info["seq"]
        self.kind = 0

    @property
    def course_type(self) -> int:
        return struct.unpack_from("<i", self.tdata, COURSE_TYPE_RVA - G.DATA_RVA0)[0]

    def collar_ints(self) -> list[int]:
        return list(struct.unpack(f"<{len(self.collar) // 4}i", self.collar))


def snaps(root: pathlib.Path) -> list[str]:
    if not (root / "index.json").exists():
        return []
    return [n for n in json.loads((root / "index.json").read_text())["snaps"] if (root / "snaps" / n / "tobj.bin").exists()]


def views(root: pathlib.Path) -> list[str]:
    """The snapshots taken at a Terrain::render entry (scenario steps), not the before / after pairs."""
    return [n for n in snaps(root) if not n.endswith((".before", ".after"))]


def export(src: pathlib.Path, label: str) -> None:
    dst = CORPUS / label
    (dst / "snaps").mkdir(parents=True, exist_ok=True)
    for f in ("calls.txt", "index.json"):
        shutil.copyfile(src / f, dst / f)
    n = 0
    for d in sorted((src / "snaps").iterdir()):
        (dst / "snaps" / d.name).mkdir(exist_ok=True)
        for f in d.iterdir():
            shutil.copyfile(f, dst / "snaps" / d.name / f.name)
        n += 1
    print(f"exported {n} snapshots to {dst}")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "export":
        export(pathlib.Path(sys.argv[2]), sys.argv[3])
    else:
        print(__doc__)
