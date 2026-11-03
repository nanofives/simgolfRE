"""P7: screenshots of --wide's side bands, from the pinned original's reference frames (log/c3/alt/refs/<scenario>/det_b/<cp>.png,
RGB = (v << 3) | (v >> 2) of the RGB555 back buffer, so v = c >> 3 restores it) composed by the app's own presenter
(simgolf_port.exe --compose). Run from a tree root: ~/bin/py -3.12 log/roadmap/P7/P7_compose.py [exe]
"""
import pathlib
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = pathlib.Path.cwd()
EXE = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "port/build/x64/app/Release/simgolf_port.exe")
REFS = ROOT / "log/c3/alt/refs"
OUT = ROOT / "log/roadmap/P7/shots"
SCREENS = [  # (scenario, checkpoint, band)
    ("sandbox_basic", "menu", "extend"),
    ("cov_b_location", "buttons", "extend"),
    ("cov_r_s2_screens", "loading_t200", "extend"),
    ("cov_r_s2_screens", "difficulty", "extend"),
    ("cov_r_s2_screens", "theme_packs", "extend"),
    ("cov_r_s2_screens", "champ_courses", "extend"),
    ("cov_r_s2_screens", "pick_a_pro", "extend"),
    ("cov_r_s2_screens", "editor", "extend"),
    ("sandbox_basic", "sandbox_20", "black"),  # in game: the bands stay black in stage 1
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    tmp = OUT / "_tmp"
    tmp.mkdir(exist_ok=True)
    rows = []
    for sc, cp, band in SCREENS:
        src = REFS / sc / "det_b" / f"{cp}.png"
        if not src.exists():
            print("missing", src)
            continue
        a = np.asarray(Image.open(src).convert("RGB")).astype(np.uint16) >> 3
        px = (a[..., 0] << 10) | (a[..., 1] << 5) | a[..., 2]
        f555 = tmp / f"{sc}_{cp}.555"
        f555.write_bytes(px.astype("<u2").tobytes())
        for fill, (cw, ch), scale in (("soft", (1920, 1080), "auto"), ("clamp", (1920, 1080), "auto"), ("soft", (1067, 600), "nearest")):
            bmp = tmp / f"{sc}_{cp}_{fill}_{cw}x{ch}.bmp"
            r = subprocess.run([EXE, "--compose", str(f555), str(cw), str(ch), str(bmp), "--wide", "--wide-fill", fill,
                                "--scale", scale, "--compose-band", band], capture_output=True, text=True)
            if r.returncode:
                print("FAIL", sc, cp, fill, r.stderr)
                return 1
            png = OUT / f"{sc}_{cp}_{fill}_{cw}x{ch}.png"
            Image.open(bmp).save(png)
            bmp.unlink()
            rows.append((sc, cp, band, fill, cw, ch, png.name))
        f555.unlink()
    tmp.rmdir()
    (OUT / "index.tsv").write_text("scenario\tcheckpoint\tband\tfill\tclient_w\tclient_h\tfile\n" +
                                   "".join("\t".join(map(str, r)) + "\n" for r in rows))
    print(len(rows), "images in", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
