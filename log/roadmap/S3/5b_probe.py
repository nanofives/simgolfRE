"""S3.5b probe: the port's sprite-queue entries (SG_GFX_SCENE_LOG: `q` lines and their `bbox`, between `end <n-1>` and `end <n>`)
of renderSpriteQueue call `n` of a scenario, the port's root at that call as a PNG, and with --walkers the walker table and
selection words after the frame.

    cd port/build/S3_5b_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/5b_probe.py <scenario> <n> <out prefix> [--walkers]

--walkers stops the boot after n frame records (one renderSpriteQueue per frame) so the state read is the frame's.
"""
import ctypes as C
import importlib.util
import os
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
spec = importlib.util.spec_from_file_location("sg5", pathlib.Path(__file__).with_name("5_sprite_gate.py"))
sg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sg)

scenario, ordn, prefix = sys.argv[1], int(sys.argv[2]), sys.argv[3]
walkers = "--walkers" in sys.argv
log = prefix + ".log"
if os.path.exists(log):
    os.remove(log)
os.environ["SG_GFX_SCENE_LOG"] = log
from port.test import boot as B  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TLm  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

FG = sg.FG
lib = L.load()
va = FG.terrain_address(scenario)
if getattr(lib, "dll", None) is not None:
    FG._bind_live(lib.dll)
got = {}


def before(st):
    st.call("gfx.attach", [])
    nm = st.map(0x30000000, bytearray(0x1000))
    raw = b"gfx_scene.captureQueueAt\0"
    C.memmove(C.addressof(nm), raw, len(raw))
    st.call("gfx.call", [0x30000000, ordn] + [0] * 10)
    if TLm.live_attach(lib, st, va) != 0:
        raise RuntimeError("sg_ts_live_attach failed")


def after(st, _res):
    raw = st.files_read(f"$gfx/queue_{ordn}.555")
    got["root"] = None if raw is None else np.frombuffer(raw, "<u2").reshape(FG.H, FG.W).copy()
    if walkers:
        objs = {n: (off, sz) for n, off, sz in lib.objects()}
        for name in ("g_walkers", "f_w_4c2e00", "g_zoomScale", "g_cameraRot", "g_playerGolfer", "g_flags", "g_date",
                     "g_golfers", "g_recordBanks", "g_animalCell", "g_sheetCols", "g_sheetFirstCell"):
            off, sz = objs[name]
            got[name] = st.read(off, sz)
    if walkers:
        import struct as S
        pool = objs["g_cellPool"][0]
        sc = S.unpack("<590i", got["g_sheetCols"])
        sf = S.unpack("<590i", got["g_sheetFirstCell"])
        got["sheets"] = {}
        nm = st.map(0x30100000, bytearray(0x1000))
        raw = b"gfx_scene.spriteInfo" + bytes([0])
        C.memmove(C.addressof(nm), raw, len(raw))
        for sh in list(range(0x136, 0x13c + 12)):
            if sf[sh] == -1:
                continue
            n = sc[sh] * 4
            key = st.data_base + pool + 0x2c * sf[sh]
            st.call("gfx.call", [0x30100000, key, n] + [0] * 9)
            info = st.files_read("$gfx/scene_spriteinfo.bin")
            got["sheets"][sh] = (sf[sh], sc[sh], info)
    st.call("gfx.detach", [])
    TLm.live_detach(lib, st)


max_records = ordn + 1 if walkers else ordn + 3
res = B.run(scenario, lib, max_records=max_records, before=before, after=after, to_end=False)
pa = got["root"]
if pa is not None:
    rgb = np.stack([((pa >> s) & 0x1f).astype(np.uint8) << 3 for s in (10, 5, 0)], axis=2)
    Image.fromarray(rgb).save(prefix + ".port.png")
if walkers:
    import struct
    w = got["g_walkers"]
    print("zoom", struct.unpack("<i", got["g_zoomScale"][:4]), "rot", struct.unpack("<i", got["g_cameraRot"][:4]),
          "sel", struct.unpack("<i", got["f_w_4c2e00"][16:20]), "player", struct.unpack("<i", got["g_playerGolfer"][:4]),
          "flags %08x" % struct.unpack("<I", got["g_flags"][:4]))
    for k in range(len(w) // 0x4c):
        r = w[k * 0x4c:(k + 1) * 0x4c]
        if r[0x12] == 0:
            continue
        wx, wy, sx, sy = struct.unpack("<iiii", r[:16])
        print(f"walker {k}: world ({wx},{wy}) screen ({sx},{sy}) flags {r[0x12]:02x} kind {struct.unpack('b', r[0x13:0x14])[0]} "
              f"lab {r[0x15]} dir {r[0x16]} anim {struct.unpack('<h', r[0x1c:0x1e])[0]} state {struct.unpack('<h', r[0x1e:0x20])[0]}")
    import struct as S2
    gb = got["g_golfers"]
    for i in range(12):
        r = gb[i * 0x100:(i + 1) * 0x100]
        x, y, sx, sy, fl = S2.unpack_from("<iiiiI", r, 0)
        print(f"golfer {i}: world ({x >> 10},{y >> 10}) screen ({sx},{sy}) flags {fl:08x} kind {r[0x18]:02x} st {r[0x1d]} frame {r[0x1e]} {r[0x1f]}")
    print("g_animalCell", S2.unpack("<I", got["g_animalCell"])[0])
    rb = got["g_recordBanks"]
    for bank in range(6):
        b = rb[bank * 0x74:(bank + 1) * 0x74]
        print("bank", bank, "words", [hex(x) for x in S2.unpack("<16H", b[:32])], "A", list(b[0x20:0x30]), "B", list(b[0x30:0x40]), "C", list(b[0x40:0x50]))
    for sh, (first, cols, info) in got["sheets"].items():
        wh = S2.unpack("<%di" % (len(info) // 4), info) if info else ()
        cells = [(wh[2 * i], wh[2 * i + 1]) for i in range(len(wh) // 2)]
        print(f"sheet {sh:#x} first {first} cols {cols}: sprites (w,h) per cell, rows of {cols}:")
        for r in range(0, len(cells), max(cols, 1)):
            print("   ", cells[r:r + cols])
print("status", res.status, "log", log)
