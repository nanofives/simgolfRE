"""Roadmap S3.32: the boot's static Surface constructions (tick_boot.inc BootStaticSurfaces, table tick_boot_surfaces.inc).

The CRT's C++ initializers run Surface_ctor (golf_clean.exe 0x00473ab0) on 147 static objects / sub-object arrays, 60,773
elements; Surface_ctor stores the vtable word 0x004ba2d8 at +0 (0x00473ab7) and zeros at +4..+0x24. The port's boot now makes
those stores, with the word read from the user's exe (descriptor f_imm_473ab9 = the imm32 at 0x00473ab9). They matter where a
simulation function indexes a per-tile grid past its end: worldToScreen's level read for tile (49, 54), g_tileLevel[2504], is
byte 0 of g_surfaces5439e0[0] (U-1834; cov_k3_camera y_inj3, 847 px).

  * the table equals the original: every element's +0 holds the descriptor's word in every segment start image (the original's
    .data at a frame head), and no other word of those images holds it;
  * a boot run writes them: after record 0 every element's +0 holds the word (the only dwords that differ from the boot without the
    stores), and g_tileLevel[2504] is its low byte (0xd8, -40 as the int8 worldToScreen reads); control tick.frame 14 (stores skipped) leaves
    all of them 0.
"""
from __future__ import annotations

import pathlib
import re
import struct

import numpy as np
import pytest

from port.test import boot as B
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

PORT = pathlib.Path(__file__).resolve().parents[1]
TABLE = PORT / "src" / "sim" / "tick_boot_surfaces.inc"
SEGMENTS = PORT / "corpus" / "segments"


def _elements(lib) -> list[int]:
    objs = {name: off for name, off, size in lib.objects()}
    out = []
    for m in re.finditer(r"^SG_BOOT_SURFACE\((\w+), (\d+), (\d+), (\d+)\)", TABLE.read_text(), re.M):
        base = objs[m.group(1)] + int(m.group(2))
        out += [base + i * int(m.group(4)) for i in range(int(m.group(3)))]
    return out


@pytest.fixture(scope="module")
def lib():
    lb = L.load()
    why = lb.exe_load()
    if why:
        pytest.skip(f"exe: {why}")
    return lb


@pytest.fixture(scope="module")
def vt(lib) -> int:
    return struct.unpack("<I", lib.exe_const("f_imm_473ab9"))[0]


def test_table_counts(lib):
    el = _elements(lib)
    assert len(el) == 60773 and len(set(el)) == len(el)
    assert TABLE.read_text().count("\nSG_BOOT_SURFACE(") == 147


def test_table_equals_every_segment_start_image(lib, vt):
    imgs = sorted(SEGMENTS.glob("*/*/start.data.bin"))
    if len(imgs) < 200:
        pytest.skip("segment corpus not present (port/corpus/segments, local)")
    el = np.array(_elements(lib), dtype=np.int64) // 4
    mask = np.zeros(lib.state_size // 4, dtype=bool)
    mask[el] = True
    for p in imgs:
        w = np.frombuffer(p.read_bytes(), dtype="<u4")
        assert np.all(w[el] == vt), p
        assert not np.any((w == vt) & ~mask), p      # no other word of the image holds Surface_ctor's vtable word


def _boot_image(lib, mode: int) -> bytes:
    got = {}
    lib.negctl("tick.frame", mode)
    try:
        r = B.run("cov_k3_camera", lib, max_records=1, after=lambda st, res: got.setdefault("img", st.image()))
    finally:
        lib.negctl_reset()
    assert r.status == "PASS", r.detail
    return got["img"]


def test_boot_writes_the_vtable_words(lib, vt):
    img = _boot_image(lib, 0)
    w = np.frombuffer(img, dtype="<u4")
    el = np.array(_elements(lib), dtype=np.int64)
    assert np.all(w[el // 4] == vt)
    lvl = next(off for name, off, size in lib.objects() if name == "g_tileLevel")
    assert img[lvl + 2504] == vt & 0xFF and struct.unpack("b", img[lvl + 2504:lvl + 2505])[0] == -40


def test_negctl_14_skips_the_stores(lib, vt):
    img = _boot_image(lib, 14)
    w = np.frombuffer(img, dtype="<u4")
    el = np.array(_elements(lib), dtype=np.int64)
    assert not np.any(w[el // 4] == vt)
    # the stores change exactly the elements' +0 words: the rest of the state at record 1 equals the boot without them (the
    # zeros at +4..+0x24 equal the bss; later boot code, e.g. the loaders filling g_sprites570cb8's +0x10..+0x1c, runs the same)
    on = np.frombuffer(_boot_image(lib, 0), dtype="<u4")
    assert sorted(np.nonzero(on != w)[0].tolist()) == sorted((el // 4).tolist())
    lvl = next(off for name, off, size in lib.objects() if name == "g_tileLevel")
    assert img[lvl + 2504] == 0
