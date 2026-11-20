"""P38E: TRACTS FOR SALE at N > 50 (log/roadmap/P38/P38FE_notes.md).

The screen (showBuyLandScreen, buyland.cpp) on a flat N = 152 course (generateFlatCourse): a scripted left click on a lot's text
region buys it, the price is the cash the purchase takes. The price is the original's per-tile sum (5, plus 1 << purchases for every
tile for sale whose Random::range(3) draw is 0; worth sum * 20 / 100, cost worth * 10), so a 152 lot of 2,500 tiles costs about
2,500 / 256 times a 50 lot. Prices are recorded for a fresh course and after 1, 2, 3 purchases; every for-sale tile of a bought lot
turns into its saved type; the map projection at 152 keeps the panel (world.h map_project)."""
from __future__ import annotations

import random
import struct

import pytest

from port.test import cmdemu as CE
from port.test import lib as L
from port.test import test_buyland as TB
from port.test import test_commands as TC

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]
N = 152
REC = 0x2E


def _lot_point(lot):
    lo, hi = TB.COLS[lot // 3]
    return (lo + hi) // 2, 0x36 + 68 * (lot % 3) + 30


def _course(lib, n):
    rng = random.Random(7)
    img = bytearray(TB._buy_image(lib, rng, (5, 5)))
    o = TC.objs(lib)
    # a sane course record 3 (acres 40, relief 1, setting 0, course type 0) and the real tile type records
    base = o["g_courseTable"][0] + REC * 3
    img[base + 0x1D], img[base + 0x1E], img[base + 0x1F], img[base + 0x20] = 40, 1, 0, 0
    a0 = o["g_tileTypeAttrs"][0]
    img[a0:a0 + 23 * 0x30] = img[o["f_tab_tileTypeAttrInit_4c1a40"][0]:][:23 * 0x30]
    img[o["g_placed_objects"][0]:o["g_placed_objects"][0] + 16 * 256] = struct.pack("<hhhBBii", -1, 0, 0, 0, 0, 0, 0) * 256
    struct.pack_into("<i", img, o["g_cash_div100"][0], 10 ** 6)
    struct.pack_into("<i", img, o["g_landPurchaseCount"][0], 0)
    struct.pack_into("<I", img, o["g_flags"][0], 0)
    return bytes(img)


_KEEP = []


def _run(st, inputs, cur, k):
    import ctypes as C
    words = [w for c in inputs for w in c.args()]
    buf = (C.c_uint32 * max(1, len(words)))(*words)
    _KEEP.append(buf)
    va = TC.INPUT_VA + 0x10000 * k   # a fresh input block per run (mappings are not dropped one by one)
    st.map(va, buf, readonly=True)
    st.call("commands.setInput", [va, len(inputs)])
    st.call("commands.setCursor", [cur[0], cur[1]])
    r = st.call(TB.NAME, []).eax
    st.call("commands.clearInput", [])
    return r


def _cash(st):
    o = TC.objs(st.lib)
    return struct.unpack_from("<i", st.image(), o["g_cash_div100"][0])[0]


@pytest.mark.parametrize("n", [N, 50])
def test_lot_prices(portlib, n, capsys):
    with portlib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(TC.DATA_VA)
        st.load_image(_course(portlib, n))
        if n != 50:
            st.world_resize(n)
        st.call("generateFlatCourse", [3])
        o = TC.objs(portlib)
        L_ = (n - 2) // 3
        prices = []
        for p, lot in enumerate((8, 6, 2, 0)):   # the four corner lots, one purchase after the other
            st.mem_write(TC.DATA_VA + o["g_landPurchaseCount"][0], struct.pack("<i", p))
            types = st.mem_read(st.grid_va("g_tileType"), n * n)
            r0, c0 = (lot % 3) * L_ + 1, (lot // 3) * L_ + 1
            for_sale = sum(1 for i in range(L_) for j in range(L_) if types[(r0 + i) * n + c0 + j] == 0x14)
            before = _cash(st)
            x, y = _lot_point(lot)
            got = _run(st, CE.click(x, y), (x, y), p)
            after = _cash(st)
            price = before - after
            types2 = st.mem_read(st.grid_va("g_tileType"), n * n)
            left = sum(1 for i in range(L_) for j in range(L_) if types2[(r0 + i) * n + c0 + j] == 0x14)
            assert got == 1 and price > 0 and left == 0, (n, p, lot, got, price, left)
            # the original's arithmetic: cost = (sum * 20 / 100) * 10 with 5 <= sum <= 5 + for_sale << p
            assert (5 * 20 // 100) * 10 <= price <= ((5 + (for_sale << p)) * 20 // 100) * 10
            prices.append((p, lot, for_sale, price))
        with capsys.disabled():
            print(f"\n[P38E] N {n}: " + "; ".join(f"after {p} purchases lot {lot} ({fs} tiles for sale) {pr} (x100 $)"
                                                  for p, lot, fs, pr in prices))


def test_map_projection_keeps_the_panel(portlib):
    """the small map's corners at 152 land where 50's land, within one 50-tile diamond (12 x 6): the far corner is 300 (2 N - 2) / N
    from the origin, 588 at 50 and 596 at 152"""
    with portlib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(_course(portlib, 50))
        corners50 = [st.call("p38fMapProject", [a, b]).eax for a, b in ((0, 0), (49, 0), (0, 49), (49, 49))]
        st.world_resize(N)
        corners152 = [st.call("p38fMapProject", [a, b]).eax for a, b in ((0, 0), (151, 0), (0, 151), (151, 151))]
    for a, b in zip(corners50, corners152):
        ax, ay = a & 0xFFFF, (a >> 16) & 0xFFFF
        bx, by = b & 0xFFFF, (b >> 16) & 0xFFFF
        assert abs(ax - bx) <= 12 and abs(ay - by) <= 6, (hex(a), hex(b))


# ------------------------------------------------------------------------------------------------ P36A's hole jump on the scaled map
def test_hole_jump_on_the_152_map(portlib):
    """a double click on a hole's number of the small map at N = 152 (the number drawn at the scaled label point: the middle of the
    projected tee and green, y - 4) leaves the map with the camera halfway between tee and green (test_p36a_hole_jump at 50)"""
    import ctypes as C
    from port.test import test_p36a_hole_jump as HJ
    tee, green = (140, 120), (148, 146)
    img = HJ._image(portlib, mode=1, holes={3: (4, tee, green)})
    with portlib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        st.world_resize(N)

        def proj(a, b):
            v = st.call("p38fMapProject", [a, b]).eax
            return v & 0xFFFF, (v >> 16) & 0xFFFF
        (tx, ty), (gx, gy) = proj(*tee), proj(*green)
        lx, ly = int((gx + tx) / 2), int((gy + ty) / 2) - 4
        assert st.call("bridge.viewDouble", [1]).eax == 1
        inputs = CE.click(lx, ly + 4)
        words = [w for c in inputs for w in c.args()]
        buf = (C.c_uint32 * len(words))(*words)
        st.map(TC.INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [TC.INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [0, 0])
        st.call(HJ.TR.NAME, [])
        st.call("commands.clearInput", [])
        out = st.image()
    o = TC.objs(portlib)
    cam = struct.unpack_from("<ii", out, o["gCameraTileX"][0])[0], struct.unpack_from("<i", out, o["gCameraTileY"][0])[0]
    assert cam == ((tee[0] + green[0]) // 2, (tee[1] + green[1]) // 2), cam
