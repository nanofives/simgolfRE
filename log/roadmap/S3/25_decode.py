"""S3.25: read the recordings of 25_rec.py (port/corpus/w8rec/<scenario>/<preset>/) into tables.

    ~/bin/py -3.12 log/roadmap/S3/25_decode.py rows  <scenario> <preset> <checkpoint> [--hook name,...] [--post] [--ra lo-hi]
    ~/bin/py -3.12 log/roadmap/S3/25_decode.py boxes <scenario> <checkpoint>          # inject: box events with their calls
    ~/bin/py -3.12 log/roadmap/S3/25_decode.py queue <scenario> <checkpoint> [--box x0,y0,x1,y1] [--out tsv]

queue walks g_spriteQueueHead -> g_spriteQueueNext (the order renderSpriteQueue draws) in the dump taken at renderSpriteQueue's
entry and prints each entry: index, sort key, anchor x / y, x / y, flags, shade (and its float bits), aux, the object pointer and,
for a sheet cell (0x005aaa30 + 0x2c * cell, cellAt 0x0043d6f0..0x0043d731), the sheet id / column / row from g_sheetFirstCell /
g_sheetCols and the sheet's .flc path (port/src/sim/menu_flics.inc: id -> a string of the exe's pool f_strpool_4c95f4).
"""
from __future__ import annotations

import argparse
import gzip
import json
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
REC = ROOT / "port" / "corpus" / "w8rec"
EXE = ROOT / "original" / "golf_clean.exe"

QARR = {  # name -> (va, count) inside the first qrange (0x0082415c..)
    "sortKey": 0x0082415C, "flags": 0x0082815C, "shade": 0x0082915C, "obj": 0x0082A15C, "anchorX": 0x0082B160,
    "anchorY": 0x0082C164, "x": 0x00830164, "y": 0x00831164, "aux": 0x00834174, "next": 0x008361F4}
HEAD, COUNT, Q0 = 0x0082C160, 0x00838200, 0x0082415C
CELL0, CELLSZ = 0x005AAA30, 0x2C


def load_rows(scen, preset, cp):
    with gzip.open(REC / scen / preset / f"{cp}.rows.json.gz", "rt") as f:
        return json.load(f)


def index(scen, preset):
    return json.loads((REC / scen / preset / "index.json").read_text())


def fmt_row(r, names):
    seq, hid, ra, ecx = r[:4]
    name = {-1: "<renderSpriteQueue entry>", -2: "<renderSpriteQueue return>"}.get(hid) or names[hid]
    if r is None:
        return ""
    args, extra = [], {}
    for x in r[4:]:
        if isinstance(x, dict):
            extra.update(x)
        else:
            args.append(x)
    s = f"{seq:6d} {name:28s} ra=0x{ra:08x} ecx=0x{ecx:08x} args=[{', '.join(hex(a) for a in args)}]"
    if "s" in extra and extra["s"]:
        s += " str=" + json.dumps(extra["s"])
    if "out" in extra and extra["out"]:
        s += " out=" + json.dumps({k: hex(v) for k, v in extra["out"].items()})
    if "ret" in extra:
        s += f" ret=0x{extra['ret']:08x}"
    return s


def cmd_rows(a):
    ix = index(a.scenario, a.preset)
    names = ix["hook_ids"]
    d = load_rows(a.scenario, a.preset, a.checkpoint)
    want = set(a.hook.split(",")) if a.hook else None
    lo, hi = (int(x, 16) for x in a.ra.split("-")) if a.ra else (0, 1 << 32)
    post = False
    print(json.dumps(d["meta"]))
    for r in d["rows"]:
        if r[1] == -2:
            post = True
        if a.post and not post:
            continue
        if want and (r[1] < 0 or names[r[1]] not in want):
            continue
        if r[1] >= 0 and not lo <= r[2] < hi:
            continue
        print(fmt_row(r, names))


def cmd_boxes(a):
    ix = index(a.scenario, a.preset)
    names = ix["hook_ids"]
    d = load_rows(a.scenario, a.preset, a.checkpoint)
    byseq = {r[0]: r for r in d["rows"]}
    boxes = ix["cfg"]["boxes"]
    print(json.dumps(d["meta"]))
    prev = None
    for seq, hid, px in d["boxEv"]:
        r = byseq.get(seq)
        what = fmt_row(r, names) if r else f"seq {seq} hook {hid} (row not logged)"
        print(what)
        if a.quiet:
            prev = px
            continue
        for bi, b in enumerate(boxes):
            if prev is None or px[bi] != prev[bi]:
                w = b[2] - b[0] + 1
                rowsx = [" ".join(f"{v:04x}" for v in px[bi][k:k + w]) for k in range(0, len(px[bi]), w)]
                print(f"    box{bi} {b}: " + " | ".join(rowsx))
        prev = px


def sheet_paths():
    """sheet id -> .flc path from port/src/sim/menu_flics.inc (GSO(f_strpool_4c95f4, off) = the exe's string at 0x4c95f4 + off)."""
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    img = pe.get_memory_mapped_image()
    out = {}
    for m in re.finditer(r"\{0x([0-9a-f]+), GSO\((f_strpool_([0-9a-f]+)), 0x([0-9a-f]+)\)",
                         (ROOT / "port" / "src" / "sim" / "menu_flics.inc").read_text()):
        va = int(m.group(3), 16) + int(m.group(4), 16)
        o = va - 0x400000
        s = img[o:img.index(b"\0", o)].decode("latin-1")
        out.setdefault(int(m.group(1), 16), set()).add(s)
    return {k: " / ".join(sorted(v)) for k, v in out.items()}


def cmd_queue(a):
    ix = index(a.scenario, "queue")
    qr = ix["cfg"]["qranges"]
    raw = (REC / a.scenario / "queue" / f"{a.checkpoint}.q.bin").read_bytes()
    segs, o = {}, 0
    for va, n in qr:
        segs[va] = raw[o:o + n]
        o += n
    q = segs[Q0]
    first = struct.unpack("<590i", segs[0x005A9370])
    cols = struct.unpack("<590i", segs[0x0053F3E8])

    def u32(va):
        return struct.unpack_from("<I", q, va - Q0)[0]

    def arr(name, i):
        return struct.unpack_from("<I", q, QARR[name] - Q0 + 4 * i)[0]

    def s32(v):
        return v - (1 << 32) if v & 0x80000000 else v

    paths = sheet_paths()
    box = [int(x) for x in a.box.split(",")] if a.box else None
    head, count = s32(u32(HEAD)), s32(u32(COUNT))
    print(f"count {count} head {head}")
    lines, i, n = [], head, 0
    hdr = "order\tidx\tsortKey\tanchorX\tanchorY\tx\ty\tflags\tshade\tshade_f32\taux\tobj\tcell\tsheet\tcol\trow\tpath"
    while i != -1 and n < 2048:
        obj = arr("obj", i)
        cell = sheet = col = row = ""
        path = ""
        if CELL0 <= obj < CELL0 + CELLSZ * 0x20000 and (obj - CELL0) % CELLSZ == 0:
            cell = (obj - CELL0) // CELLSZ
            cands = [(cell - f, sid) for sid, f in enumerate(first) if f != -1 and 0 <= cell - f]
            if cands:
                off, sid = min(cands)
                c = cols[sid] if cols[sid] > 0 else 1
                sheet, col, row = sid, off % c, off // c
                path = paths.get(sid, "")
        sh = arr("shade", i)
        x, y = s32(arr("x", i)), s32(arr("y", i))
        ln = [n, i, s32(arr("sortKey", i)), s32(arr("anchorX", i)), s32(arr("anchorY", i)), x, y, f"0x{arr('flags', i):08x}",
              s32(sh), f"{struct.unpack('<f', struct.pack('<I', sh))[0]:.6g}", s32(arr("aux", i)), f"0x{obj:08x}",
              cell, f"0x{sheet:x}" if sheet != "" else "", col, row, path]
        if box is None or (box[0] - 160 <= x <= box[2] + 40 and box[1] - 160 <= y <= box[3] + 40):
            lines.append("\t".join(str(v) for v in ln))
        i = s32(arr("next", i))
        n += 1
    out = hdr + "\n" + "\n".join(lines) + "\n"
    if a.out:
        pathlib.Path(a.out).write_text(out)
    print(out, end="")


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("rows")
    p.add_argument("scenario"); p.add_argument("preset"); p.add_argument("checkpoint")
    p.add_argument("--hook"); p.add_argument("--post", action="store_true"); p.add_argument("--ra")
    p.set_defaults(fn=cmd_rows)
    p = sp.add_parser("boxes")
    p.add_argument("scenario"); p.add_argument("checkpoint"); p.add_argument("--preset", default="inject")
    p.add_argument("--quiet", action="store_true", help="calls only, no pixels")
    p.set_defaults(fn=cmd_boxes)
    p = sp.add_parser("queue")
    p.add_argument("scenario"); p.add_argument("checkpoint"); p.add_argument("--box"); p.add_argument("--out")
    p.set_defaults(fn=cmd_queue)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
