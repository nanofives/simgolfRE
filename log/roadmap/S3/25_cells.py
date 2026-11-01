"""S3.25: every addSprite / enqueueSprite call of a queue recording's checkpoint frame with its cell decoded (sheet, column, row,
.flc path) and the Sprite_draw that drew it (position, palette). Filter by a path substring or a screen box.

    ~/bin/py -3.12 log/roadmap/S3/25_cells.py <scenario> <checkpoint> [--path spray,Fountain] [--box x0,y0,x1,y1] [--preset queue]
"""
import argparse, gzip, importlib.util, json, pathlib, struct

ROOT = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("dec", ROOT / "log/roadmap/S3/25_decode.py")
dec = importlib.util.module_from_spec(spec); spec.loader.exec_module(dec)
ap = argparse.ArgumentParser()
ap.add_argument("scenario"); ap.add_argument("checkpoint"); ap.add_argument("--path"); ap.add_argument("--box"); ap.add_argument("--preset", default="queue")
a = ap.parse_args()
D = ROOT / "port/corpus/w8rec" / a.scenario / a.preset
ix = json.loads((D / "index.json").read_text()); names = ix["hook_ids"]
raw = (D / f"{a.checkpoint}.q.bin").read_bytes()
seg, o = {}, 0
for va, n in ix["cfg"]["qranges"]:
    seg[va] = raw[o:o + n]; o += n
first = struct.unpack("<590i", seg[0x5A9370]); cols = struct.unpack("<590i", seg[0x53F3E8])
paths = dec.sheet_paths()


def s(v):
    return v - (1 << 32) if v & 0x80000000 else v


def cell(obj):
    if obj < 0x5AAA30 or (obj - 0x5AAA30) % 0x2C:
        return None
    c = (obj - 0x5AAA30) // 0x2C
    cand = [(c - f, sid) for sid, f in enumerate(first) if f != -1 and c >= f]
    if not cand:
        return None
    off, sid = min(cand)
    k = cols[sid] if cols[sid] > 0 else 1
    return c, sid, off % k, off // k, paths.get(sid, "(not in menu_flics.inc)")


want = [p for p in a.path.split(",")] if a.path else None
box = [int(v) for v in a.box.split(",")] if a.box else None
d = json.loads(gzip.open(D / f"{a.checkpoint}.rows.json.gz", "rt").read())
rows = d["rows"]
for i, r in enumerate(rows):
    if r[1] < 0 or names[r[1]] not in ("addSprite", "enqueueSprite"):
        continue
    ar = r[4:12]
    ci = cell(ar[5])
    desc = (f"cell {ci[0]} sheet 0x{ci[1]:x} col {ci[2]} row {ci[3]} {ci[4]}" if ci else f"obj 0x{ar[5]:08x} (not a sheet cell)")
    if want and not (ci and any(w in ci[4] for w in want)):
        continue
    drawn = ""
    if names[r[1]] == "addSprite":
        for r2 in rows[i + 1:i + 6]:
            if r2[1] >= 0 and names[r2[1]] in ("Sprite_draw", "Sprite_drawScaled", "Surface_blit2", "Surface_drawSprite") and r2[3] == ar[5]:
                drawn = f" -> {names[r2[1]]}({s(r2[5])}, {s(r2[6])}) pal 0x{r2[8]:08x}"; break
    x, y = s(ar[0]), s(ar[1])
    if box and not (box[0] - 200 <= x <= box[2] + 60 and box[1] - 120 <= y <= box[3] + 120):
        continue
    print(f"{r[0]:5d} {names[r[1]]:13s} ra 0x{r[2]:08x} x {x} y {y} key {s(ar[2])} w {s(ar[3])} h {s(ar[4])} shade {s(ar[6])} flags 0x{ar[7]:x} | {desc}{drawn}")
