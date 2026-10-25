"""S2.12: the cursor tile at record 771 of cov_r_s2_screens: the port's value (boot run stopped after record 771) and the
capture's, found by brute force over sgh64 of the one-dword objects (U-1062's recipe). Run: ~/bin/py -3.12 log/roadmap/S2/12_cursor.py"""
import pathlib, struct, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import boot as B          # noqa: E402
import state_image as SI                 # noqa: E402

SC = "cov_r_s2_screens"
syms, exp = B.expected_records(SC)
ix, iy = syms.index("g_cursorTileX"), syms.index("g_cursorTileY")
DATA_VA = 0x4c1000
got = {}
def after(st, res):
    got["x"], got["y"] = struct.unpack("<ii", st.read(0x543d08 - DATA_VA, 8))
for k in (770, 771, 772):
    got.clear()
    r = B.run(SC, max_records=k + 1, after=after)
    print("port after record", k, "cursor tile", got, r.status)
def find(h):
    return [v for v in range(-300, 300) if SI.sgh64(struct.pack("<i", v)) == h]
for k in (770, 771, 772):
    print("capture record", k, "X", find(exp[k][1][ix]), "Y", find(exp[k][1][iy]))
