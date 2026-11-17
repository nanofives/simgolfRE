"""P31A (decision G33 item 1, Mariano's answer "Limit by zoom"): the camera's range at the map edge.

mainLoop.cameraBoundByZoom: 1 = the original (every camera clamp is 0..0x31 on both axes, the view's centre on the grid); 0 = each
axis is clamped to the range that keeps the land's four corners (the tiles whose type is not 0x14) reachable with a black margin
beyond them for the zoom, rotation and view width (bridge.cpp camera_range), centred where the land is smaller than the view.
Checked through stepCourseEditQueue, a registered function that walks the camera one tile per call toward a goal (a hole's
midpoint) and goes on with its queue once the camera stands on it: at 0 the goal is taken inside the range, so the camera stops on
the range's corner. The expected range comes from this file's own transcription of the rule (worldToScreen's flat-ground offsets),
not from the port. Notes: log/roadmap/P31/P31A_notes.md."""
from __future__ import annotations

from pathlib import Path

import pytest

from port.test.test_golfers import VL

pytestmark = pytest.mark.port

SEGMENTS = Path(__file__).resolve().parents[1] / "corpus" / "segments"
EXTRA_PCT = 30  # P34A: each margin plus this share of the view on its axis (camera_range.h kCameraBoundExtraPct)
MARGINS0 = (16, 16, 16, 600 - 0x1DA + 16)  # P31A's: left, top, right, bottom (800-space units); they decide the zoom 1 rule
# P34A's, view width 800: each margin EXTRA_PCT of the way from P31A's to the view's middle; they give the range
MARGINS = tuple(m + (h - m) * EXTRA_PCT // 100 for m, h in zip(MARGINS0, (400, 300, 400, 300)))


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


def _off(lib, name):
    for n, o, _ in lib.objects():
        if n == name:
            return o
    raise KeyError(name)


def _offset2(rot, z, u, v, cx, cy):
    a2, b2 = 2 * (u - cx) - 1, 2 * (v - cy) - 1
    if rot == 0:
        return 8 * z * (a2 + b2) + 16 * z, 5 * z * (b2 - a2)
    if rot == 2:
        return 8 * z * (b2 - a2), -5 * z * (a2 + b2) - 10 * z
    if rot == 4:
        return -8 * z * (a2 + b2) - 16 * z, 5 * z * (a2 - b2)
    return 8 * z * (a2 - b2), 5 * z * (a2 + b2) + 10 * z


def expected_range(types: bytes, z: int, rot: int, vw: int = 800):
    """the rule, transcribed independently: the land box, then four margins grown one tile at a time (smallest first)"""
    land = [(x, y) for x in range(50) for y in range(50) if types[x * 50 + y] != 0x14]
    x0, x1 = min(p[0] for p in land), max(p[0] for p in land)
    y0, y1 = min(p[1] for p in land), max(p[1] for p in land)
    small = [False, False]
    for margins in (MARGINS0, MARGINS):
        m = _grow(x0, x1, y0, y1, z, rot, vw, margins, small if margins is MARGINS0 else [False, False])
    lo, hi = [x0 + m[0], y0 + m[2]], [x1 - m[1], y1 - m[3]]
    if small[0]:
        lo[0] = hi[0] = (x0 + x1 + 1) // 2
    if small[1]:
        lo[1] = hi[1] = (y0 + y1 + 1) // 2
    return lo, hi


def _grow(x0, x1, y0, y1, z, rot, vw, margins, small):
    ml, mt, mr, mb = margins

    def fits(t):
        lx, hx, ly, hy = x0 + t[0], x1 - t[1], y0 + t[2], y1 - t[3]
        if lx > hx or ly > hy:
            return False
        for u, v, cx, cy in ((x0, y0, lx, ly), (x1 + 1, y0, hx, ly), (x1 + 1, y1 + 1, hx, hy), (x0, y1 + 1, lx, hy)):
            X2, Y2 = _offset2(rot, z, u, v, cx, cy)
            if X2 < -(vw - 2 * ml) or X2 > vw - 2 * mr or Y2 < -(600 - 2 * mt) or Y2 > 600 - 2 * mb:
                return False
        return True
    m = [0, 0, 0, 0]
    grew = True
    while grew:
        grew = False
        for i in sorted(range(4), key=lambda k: (m[k], k)):
            t = list(m)
            t[i] += 1
            if fits(t):
                m, grew = t, True
                break
            if (x0 + t[0] > x1 - t[1]) if i < 2 else (y0 + t[2] > y1 - t[3]):
                small[i // 2] = True
    return m


def _walk(lib, original, z, rot, goal, start=(25, 25), border=5):
    """stepCourseEditQueue toward `goal` from `start` on a sandbox image whose outer `border` tiles are type 0x14 (a land of
    tiles border..49-border); returns the camera where the queue went on (its goal cleared), and the tile types"""
    p = SEGMENTS / "sandbox_basic" / "00" / "start.data.bin"
    if not p.exists():
        pytest.skip(f"no segment corpus: {p}")
    img = p.read_bytes()
    lib.defect("mainLoop.cameraBoundByZoom", original)
    o_t = _off(lib, "g_tileType")

    def w32(st, name, v):
        st.write(_off(lib, name), int(v).to_bytes(4, "little", signed=True))

    def r32(st, name):
        return int.from_bytes(st.read(_off(lib, name), 4), "little", signed=True)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        types = bytearray(st.read(o_t, 2500))
        for x in range(50):
            for y in range(50):
                if min(x, y) < border or max(x, y) > 49 - border:
                    types[x * 50 + y] = 0x14
                elif types[x * 50 + y] == 0x14:
                    types[x * 50 + y] = 4
        st.write(o_t, bytes(types))
        w32(st, "g_zoomScale", z)
        w32(st, "g_cameraRot", rot)
        w32(st, "gCameraTileX", start[0])
        w32(st, "gCameraTileY", start[1])
        w32(st, "g_cameraGoalX", goal[0])
        w32(st, "g_cameraTargetY", goal[1])
        w32(st, "g_ui_55e920", 0)        # queue index
        w32(st, "unk_005a9cd4", 0)       # queue length 0: the queue ends once the camera is on the goal
        for _ in range(200):
            st.call("stepCourseEditQueue", [0])
            if r32(st, "g_cameraGoalX") == -1:
                break
        else:
            pytest.fail("stepCourseEditQueue never reached its goal")
        return (r32(st, "gCameraTileX"), r32(st, "gCameraTileY")), bytes(types)


def test_original_walks_to_the_grid_corner(portlib):
    cam, _ = _walk(portlib, True, 4, 0, (0, 0))
    assert cam == (0, 0), "the original's camera reaches the goal, past the land's black border"


@pytest.mark.parametrize("z,rot,goal", [(4, 0, (0, 0)), (4, 0, (49, 49)), (8, 0, (49, 0)), (2, 2, (0, 49)),
                                         (8, 4, (0, 0)), (4, 6, (49, 49))])
def test_bound_stops_on_the_range_corner(portlib, z, rot, goal):
    cam, types = _walk(portlib, False, z, rot, goal)
    lo, hi = expected_range(types, z, rot)
    want = (lo[0] if goal[0] == 0 else hi[0], lo[1] if goal[1] == 0 else hi[1])
    assert cam == want, f"zoom {z} rotation {rot}: camera {cam}, range x {lo[0]}..{hi[0]} y {lo[1]}..{hi[1]}"
    assert 5 <= want[0] <= 44 and 5 <= want[1] <= 44, "the range lies inside the land's box"


def test_zoom1_centred(portlib):
    cam, types = _walk(portlib, False, 1, 0, (0, 0))
    assert expected_range(types, 1, 0) == ([25, 25], [25, 25])
    assert cam == (25, 25), "at zoom 1 the land (40 tiles) is smaller than the view: the camera stays on its centre"


def test_bound_inside_goal_unchanged(portlib):
    """a goal inside the range is reached exactly as the original reaches it"""
    a, _ = _walk(portlib, True, 4, 0, (20, 22))
    b, _ = _walk(portlib, False, 4, 0, (20, 22))
    assert a == b == (20, 22)
