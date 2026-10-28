"""Contact sheets of the reference bottom panels (0xc0..0x320 x 0x1e0..0x258) of every row of the given hudgate.tsv files,
to find the checkpoints whose frame holds the golfer list or the employee detail panel (log/roadmap/S3/6b_notes.md).

    python log/roadmap/S3/6b_sheet_refs.py <out prefix> <hudgate.tsv>...
"""
import csv
import pathlib
import sys

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[3]
REFS = ROOT / "log" / "c3" / "alt" / "refs"
W, H = 0x260, 0x78
rows = []
for t in sys.argv[2:]:
    rows += list(csv.DictReader(open(t, encoding="utf-8"), delimiter="\t"))
for part in range(0, len(rows), 24):
    sub = rows[part:part + 24]
    sheet = Image.new("RGB", (W * 2 + 4, H * ((len(sub) + 1) // 2)))
    for i, r in enumerate(sub):
        im = Image.open(REFS / r["scenario"] / "det_b" / f"{r['checkpoint']}.png").convert("RGB").crop((0xc0, 0x1e0, 0x320, 0x258))
        ImageDraw.Draw(im).text((2, 2), f"{r['scenario']} {r['checkpoint']}", fill=(255, 0, 0))
        sheet.paste(im, ((i % 2) * (W + 4), H * (i // 2)))
    sheet.save(f"{sys.argv[1]}{part // 24}.png")
print(len(rows))
