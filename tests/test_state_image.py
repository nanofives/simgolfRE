"""re/tools/state_image.py (phase 2.2): GameState byte images, object hashes, diffs.

Static: synthetic images built on the generated layout (port/include/gamestate_meta.inc): the hash's properties, the
diff naming object / first byte / class, pointer words masked, gaps only on request, the dump and .sve parsers on
synthetic blobs, the .state round trip. Runtime-marked (needs local run folders, skipped when absent): a pinned
checkpoint dump of sandbox_basic (det_b) parses and equals the phase 2.1 continuous run's dump of the same checkpoint,
and that run's full .data dump agrees with its own checkpoint dump and its saveGame file.
"""
import pathlib
import struct
import sys

import numpy as np
import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import state_image as si  # noqa: E402

L = si.layout()


def obj(sym):
    return L.by[sym]


def img_with(**vals):
    img = si.StateImage(known=np.ones(L.n, bool))
    for sym, v in vals.items():
        o = obj(sym)
        img.data[o["offset"]:o["offset"] + 4] = struct.pack("<I", v)
    return img


@pytest.mark.static
def test_layout_reads_the_generated_tables():
    assert L.base == 0x004C1000 and L.n == 0x381174
    assert len(L.objs) == 989 and sum(o["size"] for o in L.objs) + sum(n for _o, n in L.gaps) == L.n
    assert obj("g_rng")["cls"] == "sim" and obj("g_sounds")["cls"] == "audio"
    assert L.words["g_rootWindow"] == [0x11c, 0x120, 0x124, 0x128, 0x14c, 0x278, 0x378, 0x518]


@pytest.mark.static
def test_hash_properties():
    a = bytes(range(64))
    assert si.sgh64(a) == si.sgh64(bytes(a))
    for i in range(0, 64, 4):                          # one changed dword always changes the hash
        b = bytearray(a)
        b[i] ^= 1
        assert si.sgh64(bytes(b)) != si.sgh64(a)
    assert si.sgh64(a[:4] + a[8:12] + a[4:8] + a[12:]) != si.sgh64(a)   # position matters
    assert si.sgh64(b"\0" * 4) != si.sgh64(b"\0" * 8)                  # length matters
    assert si.sgh64(b"\1\2\3") != si.sgh64(b"\1\2\3\0")                # zero padding is told apart by the length
    assert si.sgh64(b"") == si._mix(0)
    # a hand-computed reference value (the agent's CModule implements the same arithmetic)
    w = 0x04030201
    s = ((w ^ si.K_XOR) * (1 * si.K_MUL)) & si.MASK64
    assert si.sgh64(bytes([1, 2, 3, 4])) == si._mix(s ^ 4)


@pytest.mark.static
def test_diff_names_object_first_byte_and_class():
    a = img_with(g_rng=1, g_date=0x100)
    b = img_with(g_rng=1, g_date=0x100)
    assert si.diff(a, b) == []
    o = obj("g_golfers")
    b.data[o["offset"] + 0x123] ^= 0xff
    b.data[o["offset"] + 0x130] ^= 0xff
    rows = si.diff(a, b)
    assert [(r["symbol"], r["first"], r["bytes"], r["cls"]) for r in rows] == [("g_golfers", 0x123, 2, "sim")]
    assert si.diff(a, b, classes={"ui"}) == []
    h1, h2 = a.hashes({"sim"}), b.hashes({"sim"})
    assert [k for k in h1 if h1[k]["hash"] != h2[k]["hash"]] == ["g_golfers"]


@pytest.mark.static
def test_pointer_words_are_masked_and_gaps_on_request():
    a, b = img_with(), img_with()
    r = obj("g_rootWindow")
    b.data[r["offset"] + 0x278:r["offset"] + 0x27c] = b"\x70\xac\xd3\x03"     # a U-0604 word: masked
    off, n = L.gaps[0]
    b.data[off] ^= 1
    assert si.diff(a, b) == []
    assert a.hashes({"ui"})["g_rootWindow"] == b.hashes({"ui"})["g_rootWindow"]
    assert [x["cls"] for x in si.diff(a, b, include_gaps=True)] == ["unmodeled"]
    b.data[r["offset"] + 0x27c] ^= 1                                            # the next dword is not masked
    assert [x["symbol"] for x in si.diff(a, b)] == ["g_rootWindow"]


@pytest.mark.static
def test_unknown_bytes_are_not_compared():
    a = img_with(g_rng=5)
    b = si.StateImage()
    assert si.diff(a, b) == []
    assert b.hashes()["g_rng"]["coverage"] == "none"


@pytest.mark.static
def test_dump_parser_places_regions(tmp_path):
    regs = si.dump_regions()
    total = sum(n for _va, n in regs)
    blob = bytearray(total)
    pos = 0
    for va, n in regs:                                   # each region's first byte = its index + 1
        blob[pos] = (regs.index((va, n)) + 1) & 0xff
        pos += n
    p = tmp_path / "x.bin"
    p.write_bytes(bytes(blob))
    img = si.load(p)
    for k, (va, _n) in enumerate(regs[:-2]):
        assert img.data[va - L.base] == (k + 1) & 0xff
    assert img.known.sum() == len({(va + i) for va, n in regs for i in range(n)})
    with pytest.raises(ValueError):
        p.write_bytes(bytes(blob[:-1]))
        si.load(p)


@pytest.mark.static
def test_sve_parser_on_a_synthetic_save(tmp_path):
    import sve
    flags = 0x40000000                                     # tail A present, tail B absent
    parts = [b"H" * 100]
    for va, n in sve.layout(flags):
        parts.append(struct.pack("<I", flags) + b"\0" * (n - 4) if va == sve.FLAGS_VA else bytes([va & 0xff]) * n)
    p = tmp_path / "s.sve"
    p.write_bytes(b"".join(parts))
    img = si.load(p)
    assert img.data[L.offset(0x0051A068):L.offset(0x0051A068) + 100] == b"H" * 100
    for va, n in sve.layout(flags):
        if va != sve.FLAGS_VA:
            assert img.data[va - L.base] == va & 0xff
    tail_b = sve.TAIL_B[1][0][0]
    assert not img.known[tail_b - L.base]


@pytest.mark.static
def test_state_round_trip(tmp_path):
    a = img_with(g_rng=7)
    a.known[:16] = False
    a.save(tmp_path / "x")
    b = si.load(tmp_path / "x.state.bin")
    assert bytes(b.data) == bytes(a.data) and (b.known == a.known).all()


# ---------------------------------------------------------------- real dumps (local run folders, not committed)
PINNED = ROOT / "log" / "c3" / "alt" / "refs" / "sandbox_basic" / "det_b" / "sandbox_20.bin"
P21 = ROOT / "log" / "p2" / "p21" / "sandbox_basic"


@pytest.mark.runtime
@pytest.mark.skipif(not PINNED.exists() or not (P21 / "c_a" / "sandbox_20.bin").exists(),
                    reason="pinned reference or phase 2.1 run folders not present locally")
def test_real_pinned_checkpoint_and_phase21_dumps():
    pinned = si.load(PINNED)
    run = si.load(P21 / "c_a" / "sandbox_20.bin")
    assert si.diff(pinned, run) == []                       # the pinned det_b and 2.1's continuous run agree
    full = si.load(P21 / "c_a" / "sandbox_20.full.bin")
    assert si.diff(run, full, include_gaps=True) == []      # the dump is a subset of the same instant's .data
    hp, hf = pinned.hashes({"sim"}), full.hashes({"sim"})
    for k, v in hp.items():
        if v["coverage"] == "full":
            assert v["hash"] == hf[k]["hash"], k
    save = P21 / "c_save" / "p21_sandbox_20.sve"
    if save.exists():
        rows = si.diff(si.load(save), si.load(P21 / "c_save" / "sandbox_20.fh.bin"))
        assert [r["symbol"] for r in rows] == ["g_textBuffer"]   # saveGame rewrites the text buffer it saves
