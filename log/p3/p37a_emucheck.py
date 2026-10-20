"""Phase 3.7a: emulator cross-check of one dumped callee call (SG_TICK_DUMP=<name>;<dir>, tick.cpp): the ORIGINAL function
runs in Unicorn (re/tools/vector_emu.py, golf_clean.exe's file image) on the GameState the port had before the call, and
its written bytes are compared with the port's state after the call, object by object (sim class flagged).
usage: py -3.12 log/p3/p37a_emucheck.py <dir>/<name>_<k> 0x<entry> [abi]"""
import pathlib, re, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_emu as E
import vector_lib as VL

base, entry = sys.argv[1], int(sys.argv[2], 16)
abi = sys.argv[3] if len(sys.argv) > 3 else "default"
pre = pathlib.Path(base + ".pre.bin").read_bytes()
post = pathlib.Path(base + ".post.bin").read_bytes()
args = [int(x) for x in pathlib.Path(base + ".args").read_text().split()]
img = E.load_image()
r = E.run_call(img, pre, [], abi, ["int"] * len(args), args, "int", 0x027F, entry)
print("eax", hex(r.eax), "err", r.err, "imports", r.imports[:8], "misses", [hex(m) for m in r.misses[:5]],
      "fetch_misses", [hex(m) for m in r.fetch_misses[:5]])
orig = bytearray(pre)
for a, v in r.mem.items():
    if VL.DATA_VA <= a < VL.DATA_VA + len(pre):
        orig[a - VL.DATA_VA] = v
objs = []
for line in (ROOT / "port" / "include" / "gamestate_meta.inc").read_text().splitlines():
    m = re.match(r"SG_OBJECT\((\w+), (0x[0-9a-f]+), (0x[0-9a-f]+), (\w+)", line)
    if m:
        objs.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
for n, off, size, cls in objs:
    a, b = orig[off:off + size], post[off:off + size]
    if a != b:
        k = next(i for i in range(size) if a[i] != b[i])
        diffs = sum(1 for i in range(size) if a[i] != b[i])
        print("%-28s %-7s first +0x%x orig %02x port %02x  (%d bytes differ)" % (n, cls, k, a[k], b[k], diffs))
