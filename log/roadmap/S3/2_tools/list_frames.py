"""S3.2: list the frames of a --pass-frames recording (checkpoint, frame ordinal, updateTerrainRender args, passes)."""
import json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[4]
for lab in sys.argv[1:]:
    d = ROOT / "log/p5/terrain" / lab / "frames"
    rows = []
    for c in d.iterdir():
        m = json.loads((c / "meta.json").read_text())
        if m.get("none"):
            continue
        rows.append((m["frame"], c.name, m["args"], [(p["name"], [hex(x) for x in p["args"]]) for p in m["passes"]]))
    for r in sorted(rows):
        print(lab, *r)
