"""re/tools/cv_debug.py on a synthetic PE carrying an NB11 CodeView block (S10.11): one module, one S_GPROC32, one
S_PUB32, one S_UDT, a class with a base, two members and an introducing virtual (LF_ONEMETHOD with its vtable
offset), plus the format refusals (no debug entry, NB10)."""
import struct
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import cv_debug  # noqa: E402

pytestmark = pytest.mark.static


def pstr(s):
    return bytes([len(s)]) + s.encode()


def rec(kind, body):
    """A symbol record: u16 length (from the kind on), u16 kind, body, padded to 4."""
    b = struct.pack("<H", kind) + body
    b += b"\0" * (-(len(b) + 2) % 4)
    return struct.pack("<H", len(b)) + b


def leaf(kind, body):
    b = struct.pack("<H", kind) + body
    return struct.pack("<H", len(b)) + b


def types_block():
    # 0x1000 fieldlist of Base: member +0 int `m_a`
    fl_base = struct.pack("<H", 0x1203) + struct.pack("<HHI", 0x1405, 3, 0x74) + struct.pack("<H", 0) + pstr("m_a")
    # 0x1001 class Base size 4
    cls_base = struct.pack("<HHHIII", 0x1004, 1, 0, 0x1000, 0, 0) + struct.pack("<H", 4) + pstr("Base")
    # 0x1002 fieldlist of Derived: base Base +0, member +0x8 uint `m_b`, intro virtual `go` at vtable +0x4, vfptr
    fl = struct.pack("<H", 0x1203)
    fl += struct.pack("<HHI", 0x1400, 3, 0x1001) + struct.pack("<H", 0)
    fl += struct.pack("<HHI", 0x1409, 0, 0x1004)
    fl += struct.pack("<HHI", 0x1405, 3, 0x75) + struct.pack("<H", 8) + pstr("m_b") + b"\xf1"
    fl += struct.pack("<HHII", 0x140b, 3 | (4 << 2), 0x1005, 4) + pstr("go")
    # 0x1003 class Derived size 0x8000 (an LF_USHORT numeric leaf), 2 fields
    cls = struct.pack("<HHHIII", 0x1004, 4, 0, 0x1002, 0, 0) + struct.pack("<HH", 0x8002, 0x8000) + pstr("Derived")
    recs = [fl_base, cls_base, fl, cls, struct.pack("<HII", 0x1002, 0x1001, 0), struct.pack("<H", 0x1009)]
    out = [leaf(struct.unpack_from("<H", r)[0], r[2:]) for r in recs]
    offs, p = [], 0
    for r in out:
        offs.append(p); p += len(r)
    return struct.pack("<II", 1, len(out)) + struct.pack("<%dI" % len(out), *offs) + b"".join(out)


def nb11():
    mod = struct.pack("<HHH2s", 0, 0, 1, b"CV") + struct.pack("<HHII", 1, 0, 0, 0x100) + pstr("demo.obj")
    proc = rec(0x100B, struct.pack("<6I", 0, 0, 0, 0x20, 3, 0x1D) + struct.pack("<IIHB", 0x1005, 0x10, 1, 0)
               + pstr("Derived::go"))
    align = struct.pack("<I", 1) + proc + rec(0x0006, b"")
    pub = rec(0x1009, struct.pack("<IIH", 0, 0x10, 1) + pstr("?go@Derived@@UAEXXZ"))
    udt = rec(0x1003, struct.pack("<I", 0x1003) + pstr("Derived"))
    gpub = struct.pack("<HHIII", 0, 0, len(pub), 0, 0) + pub
    gsym = struct.pack("<HHIII", 0, 0, len(udt), 0, 0) + udt
    segmap = struct.pack("<HH", 1, 1) + struct.pack("<6HII", 0, 0, 0, 1, 0, 0, 0, 0x200)
    subs = [(0x120, 1, mod), (0x125, 1, align), (0x12A, 0xFFFF, gpub), (0x129, 0xFFFF, gsym),
            (0x12B, 0xFFFF, types_block()), (0x12D, 0xFFFF, segmap)]
    body, ents, p = b"", [], 8
    for sst, imod, data in subs:
        data += b"\0" * (-len(data) % 4)
        ents.append(struct.pack("<HHII", sst, imod, p, len(data)))
        body += data; p += len(data)
    d = struct.pack("<HHIII", 16, 12, len(ents), 0, 0) + b"".join(ents)
    return b"NB11" + struct.pack("<I", p) + body + d


def pe(cv=None, debug=True):
    """MZ / PE32 with one .text section (VA 0x1000, file 0x200) holding the debug directory; cv appended at 0x400."""
    f = bytearray(0x400)
    f[0:2] = b"MZ"; struct.pack_into("<I", f, 0x3C, 0x40)
    f[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", f, 0x44, 0x14C, 1, 0, 0, 0, 0xE0, 0x2102)
    struct.pack_into("<H", f, 0x58, 0x10B)
    if debug:
        struct.pack_into("<II", f, 0x58 + 96 + 6 * 8, 0x1000, 28)
    struct.pack_into("<8sIIII", f, 0x58 + 0xE0, b".text", 0x200, 0x1000, 0x200, 0x200)
    cv = nb11() if cv is None else cv
    struct.pack_into("<IIHHIIII", f, 0x200, 0, 0, 0, 0, 2, len(cv), 0, 0x400)
    return bytes(f) + cv


def test_nb11_symbols():
    cv = cv_debug.CvImage(pe())
    assert cv.sig == "NB11" and cv.wide
    assert cv.modules == {1: "demo.obj"}
    (p,) = cv.procs
    assert (p["rva"], p["size"], p["kind"], p["module"], p["name"]) == (0x1010, 0x20, "G", "demo.obj", "Derived::go")
    assert (p["debug_start"], p["debug_end"], p["type"]) == (3, 0x1D, 0x1005)
    assert [(x["rva"], x["name"]) for x in cv.publics] == [(0x1010, "?go@Derived@@UAEXXZ")]
    assert cv.udts == [{"type": 0x1003, "name": "Derived"}]


def test_nb11_class_layout():
    cv = cv_debug.CvImage(pe())
    by = {a["name"]: a for a in cv.classes()}
    assert set(by) == {"Base", "Derived"}
    d = by["Derived"]
    assert d["size"] == 0x8000 and d["vfptr"] and d["count"] == 4
    assert d["bases"] == [{"name": "Base", "offset": 0}]
    assert d["members"] == [{"offset": 8, "name": "m_b", "type": "uint"}]
    assert [(m["name"], m["mprop"], m.get("vtable_offset")) for m in d["methods"]] == [("go", "intro", 4)]
    assert by["Base"]["members"] == [{"offset": 0, "name": "m_a", "type": "int"}]
    assert cv.type_name(0x1004) == "Base*"


def test_refusals():
    with pytest.raises(ValueError, match="no CodeView"):
        cv_debug.CvImage(pe(debug=False))
    with pytest.raises(ValueError, match="NB10: external PDB"):
        cv_debug.CvImage(pe(cv=b"NB10" + struct.pack("<IIII", 0, 0, 1, 1) + b"x.pdb\0"))
    with pytest.raises(ValueError, match="unknown CodeView"):
        cv_debug.CvImage(pe(cv=b"NB05" + b"\0" * 12))


def test_numeric_and_pascal():
    assert cv_debug.numeric(struct.pack("<H", 0x7FFF), 0) == (0x7FFF, 2)
    assert cv_debug.numeric(struct.pack("<HI", 0x8004, 0x12345678), 0) == (0x12345678, 6)
    assert cv_debug.numeric(struct.pack("<Hb", 0x8000, -2), 0) == (-2, 3)
    assert cv_debug.pstr(pstr("abc") + b"zz", 0) == ("abc", 4)


DEMO = ROOT / "log" / "roadmap" / "S10" / "11_dl" / "demo" / "36_sound.dll"


@pytest.mark.skipif(not DEMO.exists(), reason="demo sound.dll not extracted (log/roadmap/S10/11_notes.md)")
def test_demo_sound_dll_counts():
    """The SimGolf demo's sound.dll (S10.11): NB11 at file offset 0x751a0 with the counts the notes report."""
    cv = cv_debug.CvImage(DEMO.read_bytes())
    assert (cv.sig, cv.base, cv.size) == ("NB11", 0x751A0, 1925272)
    assert (len(cv.procs), len(cv.publics), len(cv.modules), len(cv.types)) == (2146, 3395, 389, 11040)
    by = {p["rva"]: p["name"] for p in cv.procs}
    assert by[0x2340] == "Sound::get_next"
    lay = {a["name"]: a for a in cv.classes()}
    assert {m["offset"]: m["name"] for m in lay["Sound"]["members"]}[0x4C] == "m_pnext"
