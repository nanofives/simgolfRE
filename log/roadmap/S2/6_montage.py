"""S2.6: 2x2 contact sheets (half size, labelled) of a run's checkpoint PNGs, for viewing several references at once.
usage: 6_montage.py <run dir> <out dir> [names...]   (default: every checkpoint in summary.json order)"""
import json
import pathlib
import sys

from PIL import Image, ImageDraw

run, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
names = sys.argv[3:]
if not names:
    summ = json.loads((run / "summary.json").read_text())
    cps = summ.get("checkpoints", [])
    names = [c["name"] if isinstance(c, dict) else c for c in cps] or sorted(p.stem for p in run.glob("*.png"))
for k in range(0, len(names), 4):
    sheet = Image.new("RGB", (800, 600), "white")
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(names[k:k + 4]):
        p = run / f"{n}.png"
        if not p.exists():
            continue
        im = Image.open(p).convert("RGB").resize((400, 300))
        x, y = (i % 2) * 400, (i // 2) * 300
        sheet.paste(im, (x, y))
        d.rectangle([x, y, x + 160, y + 14], fill="white")
        d.text((x + 2, y + 1), n, fill="red")
    sheet.save(out / f"sheet_{k // 4:02d}.png")
    print(out / f"sheet_{k // 4:02d}.png", names[k:k + 4])
