"""S6.8 (wave 2): the `fixed`-profile switch generateRandomCourse.placementBound (D52b3, model_defects#52).

The two feature-site searches of generateRandomCourse that retry random cells while evalPlacementArea rejects them:
location 4 (2 x 2 site, kind 4) and locations 5, 6, 8, 12, 13, 15 (5 x 5 site, kind 0xF). Switch 1 (default) = the
original (NoReturn once no reachable cell is accepted); 0 = the search gives up and the feature is skipped.

Facts about the ORIGINAL (Unicorn, log/roadmap/S6/8_probes/, notes log/roadmap/S6/8_notes.md):
- d52b3_placement.log: on the sandbox_basic image both loops exhaust 300 M instructions when every flag word carries
  0x8080 at the loop entry (about 1.2 M / 1.1 M iterations), and complete as generated or with an 8 x 8 block left clear.
- d52b3_port_images*.log: on the very images below (BLOCKED) the original exhausts 300 M instructions inside the loop
  (921,011 / 933,493 iterations), and on the PLAIN images it completes with eax and the whole data image equal to the
  port's at both settings; the digests pinned in PLAIN_DIGESTS were taken from those port results.
Wave 1's file (test_defects_gen.py) is not edited. Run as one file per process.
"""
from __future__ import annotations

import hashlib
import struct

import pytest

from port.test.test_terrain_gen import _fixed_image, _i32, _put, need_exe, objs, port_call

pytestmark = pytest.mark.port

PLACE = "generateRandomCourse.placementBound"
SEARCH = "generateRandomCourse.searchBound"
FN = "generateRandomCourse"
KW = dict(setting=0, ctype=0, difficulty=1, relief=1)
LOC_A = (4,)
LOC_B = (5, 6, 8, 12, 13, 15)
SIDE = 4  # the clubhouse site (build item 15) is 4 x 4: port_explore.log, a 4 x 4 clear block lets it be placed

# sha256 of eax (u32 LE) + the final data image, port at both settings; equal to the original under Unicorn
PLAIN_DIGESTS = {
    3: "6f28aad76d0aa482dcc0b50041ae43156b1b83b362de0d246a03fc8b88f39e78",
    4: "afb37459351385bb0915601493f0eadbc78ac2eddd50dc3d5d4aace8877691da",
    5: "7e32dd5f5a0d56e50746d5d572f7dedf3387e30f4dd231ce2dbc0dda9be066dd",
    6: "9cf2696486877fd5dc4afac4e8f4dfe3bd4cf13177bd4b0d4ddd3732e0e3df1c",
    8: "f04bb2538b1910577ba18d5aae20e960bcc5fe2f84eff3096d56f33f80d8c360",
    12: "630a9fa1a2a77113010e63381c76cbd78fbabd687963c70b44970755ced35505",
    13: "03208bf11512ee4cc6394a7d3db55f900091efcfc6afbca4acbabe32742ec1ae",
    15: "7881e0fa5374ea4ebc4da6e3903c67ab3b85cd3f5f158c58a563a82b694ebdd0",
}
# the fixed arm on BLOCKED images (port choice, U-1598 / U-1599): pinned so that a change of the fixed arm is seen
FIXED_DIGESTS = {
    4: "7d076a7359002ab57d579c7c787aef78a3416f2999be2f0b49d3cbc09d0afa1c",
    5: "6421413df8d5cef42702ed37e3263267ed22d09a029707f88cfff88e58c3d299",
    6: "ca1e847f6e263e36394f92689b57eafd44874aaa0ebbf81dd51f9b94d6291cb9",
    8: "15a344e564dfa04bc0d08e38ee5d6e7c28e4228b3609746c19efdde801c62181",
    12: "8d4fbdd6f9d1f7f543360e049761c83ff9c34163b156173e4c632fdc6317c810",
    13: "ae1f5c25b57dab9db0565bbc17815ed327a8084b2b26eda73226a07285bc74b3",
    15: "ea7ee3adebeae8f99c0884574cbc898830e6fdcd0a515b379b665cf5add43740",
}


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl(FN, 0)
    yield
    portlib.defect_reset()
    portlib.negctl(FN, 0)


def _digest(res) -> str:
    eax, out = res
    assert out is not None and not isinstance(eax, tuple), eax
    return hashlib.sha256(struct.pack("<I", eax & 0xFFFFFFFF) + bytes(out)).hexdigest()


def _clubhouse(portlib):
    """the clubhouse corner the plain location-4 image gives (the same for every location: the switch comes after it)"""
    hole19 = objs(portlib)["g_holes"][0] + 19 * 0x208
    _, out = port_call(portlib, _fixed_image(portlib, n=3, loc=4, **KW), 3)
    return _i32(out, hole19 + 8), _i32(out, hole19 + 0xC)


def _image(portlib, loc, blocked):
    img = _fixed_image(portlib, n=3, loc=loc, **KW)
    if blocked:
        cx, cy = _clubhouse(portlib)
        flags = bytearray(struct.pack("<H", 0x8080) * 2500)  # evalPlacementArea rejects any interior cell with these bits
        for i in range(SIDE):
            for j in range(SIDE):
                struct.pack_into("<H", flags, 2 * ((cx + i) * 50 + cy + j), 0)
        _put(portlib, img, "g_tileFlags", bytes(flags))
    return bytes(img)


def _run(portlib, img, bound=1, negctl=0):
    portlib.defect_reset()
    if bound == 0:
        portlib.defect(PLACE, False)
    portlib.negctl(FN, negctl)
    try:
        return port_call(portlib, img, 3)
    finally:
        portlib.negctl(FN, 0)
        portlib.defect_reset()


def test_switch_registered_default_on_and_wave1_switch_kept(portlib):
    d = portlib.defects()
    assert d[PLACE]["ref"] == "model_defects#52" and d[PLACE]["on"] == 1
    assert d[SEARCH]["ref"] == "model_defects#52" and d[SEARCH]["on"] == 1


@need_exe
@pytest.mark.parametrize("loc", LOC_A + LOC_B)
def test_blocked_site_search_hangs_at_1_and_returns_at_0(portlib, loc):
    img = _image(portlib, loc, True)
    eax, out = _run(portlib, img, 1)
    assert out is None and eax == ("error", -10)  # NoReturn: the original never leaves the search (8_probes logs)
    eax, out = _run(portlib, img, 0)
    assert out is not None and not isinstance(eax, tuple), "the fixed search gives up and the function finishes"
    # the same blocked image with location 3 (no eval-gated feature search) completes at 1: the -10 above is D52b3
    eax, out = _run(portlib, _image(portlib, 3, True), 1)
    assert out is not None and not isinstance(eax, tuple)


@need_exe
def test_blocked_at_0_does_not_touch_searchbound(portlib):
    """placementBound 0 alone fixes the blocked images: searchBound stays 1 (on) in the test above, and turning searchBound
    off alone does not end the placement searches"""
    img = _image(portlib, 4, True)
    portlib.defect(SEARCH, False)
    try:
        eax, out = port_call(portlib, img, 3)
    finally:
        portlib.defect_reset()
    assert out is None and eax == ("error", -10)


@need_exe
def test_location_4_fixed_arm_stamps_no_object(portlib):
    img = _image(portlib, 4, True)
    _, out = _run(portlib, img, 0)
    po = objs(portlib)["g_placed_objects"][0]
    types = [struct.unpack_from("<h", out, po + 16 * i)[0] for i in range(256)]
    plain = _run(portlib, _image(portlib, 4, False), 0)[1]
    ptypes = [struct.unpack_from("<h", plain, po + 16 * i)[0] for i in range(256)]
    assert types.count(4) == ptypes.count(4) - 1  # exactly the site object is missing


@need_exe
@pytest.mark.parametrize("loc", (3,) + LOC_A + LOC_B)
def test_completing_controls_equal_at_both_settings_and_to_the_original(portlib, loc):
    img = _image(portlib, loc, False)
    on, off = _run(portlib, img, 1), _run(portlib, img, 0)
    assert on == off
    assert _digest(on) == PLAIN_DIGESTS[loc]


@need_exe
@pytest.mark.parametrize("loc", LOC_A + LOC_B)
def test_fixed_arm_pinned(portlib, loc):
    assert _digest(_run(portlib, _image(portlib, loc, True), 0)) == FIXED_DIGESTS[loc]


@need_exe
@pytest.mark.parametrize("loc,mode,what", [(4, 4, "the object is stamped at the last draw anyway"),
                                           (5, 5, "the object pass runs after the search gave up"),
                                           (15, 5, "the object pass runs after the search gave up")])
def test_wrong_fixed_arm_reads_red(portlib, loc, mode, what):
    img = _image(portlib, loc, True)
    res = _run(portlib, img, 0, negctl=mode)
    assert res[1] is None or _digest(res) != FIXED_DIGESTS[loc], what


@need_exe
@pytest.mark.parametrize("loc", (4, 5))
def test_wrong_body_reads_red_on_the_completing_control(portlib, loc):
    """the wave-1 control (camera y = column - 3) changes the oracle-equal plain result at placementBound 0"""
    assert _digest(_run(portlib, _image(portlib, loc, False), 0, negctl=1)) != PLAIN_DIGESTS[loc]


def pin():  # development helper: print the digests (only after d52b3_port_images.py shows the plain runs equal)
    from port.test import lib as L
    lib = L.load()
    assert lib.exe_load() == ""
    print("PLAIN_DIGESTS = {")
    for loc in (3,) + LOC_A + LOC_B:
        print(f"    {loc}: \"{_digest(_run(lib, _image(lib, loc, False), 1))}\",")
    print("}\nFIXED_DIGESTS = {")
    for loc in LOC_A + LOC_B:
        print(f"    {loc}: \"{_digest(_run(lib, _image(lib, loc, True), 0))}\",")
    print("}")


if __name__ == "__main__":
    pin()
