"""Phase 3.8 stream 38a: module gditext, the host GDI TextMetrics provider (portsim/sim/gditext.h; notes
log/p38/38a_notes.md).

Evidence:
  * RECORDED WIDTHS (port/test/widths_check.py over port/corpus/widths/, recorded from the original by
    re/tools/widths_rec.py): every Font::textWidth, Text_put and drawMarkupRun call of the ten scenarios whose segments
    reach the widgets must give the original's result; rows of a lone control character are counted apart (U-0960).
  * FONT METRICS: the provider's font for each of the 15 FontRefs the game creates must produce the five words
    Font::create stored into the FontRef in the original's run (a segment start image).
  * EMULATOR ORACLE for drawMarkupRun's token logic (opt-in SG_EMU_ORACLE=1): the ORIGINAL drawMarkupRun runs under
    Unicorn (port/test/cmdemu.py) with real measureTextWidth code and the jgld font slot stubbed by the rule of the port's
    scripted test provider (gditext.attachScripted), over random draw-context states and marked-up texts; eax and the
    whole data image must equal the port's. The default suite checks golden digests pinned from the port after the
    oracle was GREEN on them (port/test/golden_gditext.json; SG_EMU_ORACLE=update re-pins), and every negative control
    against them or against the recordings.
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L
from port.test import widths_check as WC

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_gditext.json"
pytestmark = pytest.mark.port

DATA_VA = 0x004C1000
DRAW_MARKUP_RUN = 0x00477280      # thiscall (text, len), ret 8
CTX = 0x00519CD4                  # the root window's Image (f_stateptr_4c1570[0])
TEXT_VA = 0x30000000
FAKE = 0x32000000                 # emulator only: fake jgld Font objects, their vtable and the textWidth stub
STUB = FAKE + 0x900
FONTS = ["g_font83ad20", "g_font51b320", "g_fontRef_519fd8", "g_font51b360", "g_font519928", "g_font51a028",
         "g_fontRef_519a40", "g_font519948", "g_font51b340", "g_font519968", "g_fontRef_821020", "g_fontRef_821f08",
         "g_fontRef_821ec8", "g_fontRef_821f28", "g_fontRef_821ee8"]
SCRIPT = (3, 0x1F)                # the scripted rule: base 3, mask 0x1f

emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")


def _offs(lib):
    return {n: o for n, o, _ in lib.objects()}


def _has(lib):
    if not lib.has("gditext.attach"):
        pytest.skip("module gditext not in this build")


# ------------------------------------------------------------------------------------------------ interface basics
def test_text_put_without_provider_is_a_missing_resource(portlib):
    _has(portlib)
    with portlib.state() as st:
        st.set_data_base(DATA_VA)
        with pytest.raises(L.SgError) as e:
            st.call("Text_put", [CTX, TEXT_VA])
        assert e.value.code == L.SG_E_NO_RESOURCE


def test_null_text_is_zero(portlib):
    _has(portlib)
    with portlib.state() as st:
        st.set_data_base(DATA_VA)
        st.call("gditext.attachScripted", list(SCRIPT))
        try:
            assert st.call("Text_put", [CTX, 0]).eax == 0
            assert st.call("gditext.textPut", [CTX, DATA_VA + _offs(portlib)["g_font519928"], 0]).eax == 0
        finally:
            st.call("gditext.detach", [])


# ------------------------------------------------------------------------------------------------ fonts
def _segment_image():
    for s in WC.scenarios() or ["cov_a_play"]:
        img = WC.start_image(s)
        if img:
            return img
    p = ROOT / "port" / "corpus" / "segments"
    for d in sorted(p.glob("*/[0-9][0-9]")) if p.is_dir() else []:
        if (d / "start.data.bin").exists():
            return (d / "start.data.bin").read_bytes()
    return None


@pytest.mark.skipif(os.name != "nt", reason="host GDI")
def test_font_metrics_equal_the_originals(portlib):
    """Font::create stores tmHeight + tmExternalLeading, tmAscent - tmInternalLeading, tmInternalLeading, tmAscent and
    tmDescent into the FontRef (+0xc .. +0x1c); the provider's fonts must give the same five words as the original's run."""
    _has(portlib)
    img = _segment_image()
    if img is None or not WC.INSTALL.is_dir():
        pytest.skip("needs a segment start image and the user's install")
    offs = _offs(portlib)
    with portlib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        st.files_root(WC.INSTALL)
        st.call("gditext.attach", [])
        try:
            bad = []
            for name in FONTS:
                want = list(struct.unpack("<5i", st.read(offs[name] + 0xC, 20)))
                got = [WC.i32(st.call("gditext.fontMetrics", [DATA_VA + offs[name], k]).eax) for k in range(5)]
                if want != got:
                    bad.append((name, want, got))
            assert not bad, bad
        finally:
            st.call("gditext.detach", [])


# ------------------------------------------------------------------------------------------------ recorded widths
need_widths = pytest.mark.skipif(not WC.scenarios(), reason="no recorded widths (re/tools/widths_rec.py record)")


@need_widths
@pytest.mark.skipif(os.name != "nt", reason="host GDI")
@pytest.mark.parametrize("scenario", WC.scenarios())
def test_widths_equal_the_recorded_ones(portlib, scenario):
    _has(portlib)
    o = WC.check(portlib, scenario)
    w, t, m = o["widths"], o["textput"], o["markup"]
    print(json.dumps({k: {x: v for x, v in o[k].items() if x not in ("examples", "control_differ")}
                      for k in ("widths", "textput", "markup")}))
    assert o["ok"], json.dumps([w["examples"], t["examples"], m["examples"]])[:2000]


# modes 1, 2 and 7 change printable widths everywhere; 3..6 change only texts with a markup token (the recordings hold
# few: they are judged on the emulator digests below as well)
@need_widths
@pytest.mark.skipif(os.name != "nt", reason="host GDI")
@pytest.mark.parametrize("mode", [1, 2, 7])
def test_negctl_red_on_recordings(portlib, mode):
    _has(portlib)
    scen = "cov_a_play" if "cov_a_play" in WC.scenarios() else WC.scenarios()[0]
    portlib.negctl("gditext.fontWidth", mode)
    try:
        assert not WC.check(portlib, scen)["ok"], f"mode {mode} reads GREEN on {scen}"
    finally:
        portlib.negctl("gditext.fontWidth", 0)


# ------------------------------------------------------------------------------------------------ drawMarkupRun oracle
def _tokens(img: bytes, offs) -> list[bytes]:
    """the three `$` tokens drawMarkupRun compares (f_strpool_4e4218 + 0x24 / + 0x30 / + 0x3c)"""
    base = offs["f_strpool_4e4218"]
    return [img[base + 0x24:base + 0x24 + 9], img[base + 0x30:base + 0x30 + 9], img[base + 0x3c:base + 0x3c + 6]]


ALPHA = list(b"abcdefgh ABCxyz,.'!") + [ord(c) for c in "{}[]$=>^"]


def _case(rng: random.Random, offs, toks) -> dict:
    parts = []
    for _ in range(rng.randrange(0, 9)):
        r = rng.random()
        if r < 0.45:
            parts.append(bytes(rng.choice(ALPHA[:19]) for _ in range(rng.randrange(1, 6))))
        elif r < 0.75:
            parts.append(bytes([rng.choice(b"{}[]$=>^")]))
        elif r < 0.85:
            parts.append(bytes([rng.choice(b"{}[]")]) * 2)
        else:
            parts.append(rng.choice(toks))
    text = b"".join(parts)[:60]
    fonts = [DATA_VA + offs[n] for n in FONTS]
    pick = lambda p0: 0 if rng.random() < p0 else rng.choice(fonts)   # noqa: E731
    n = len(text)
    ln = n if rng.random() < 0.7 else rng.randrange(0, n + 3)
    return {"text": text.hex(), "len": ln, "style": rng.choice([0, 0, 1, 2, 3]), "link": rng.choice([0, 0, 1, 2]),
            "markup": 0 if rng.random() < 0.15 else 1, "drop": rng.choice([0, 0, 1, 2]), "arrow": rng.choice([0, 1]),
            "f": [pick(0.2), pick(0.6), pick(0.6), pick(0.6)], "primary": rng.choice(fonts),
            "nohandle": sorted(rng.sample(range(len(FONTS)), rng.choice([0, 0, 0, 1, 2]))),
            "null": rng.random() < 0.03}   # a NULL text (callers of drawMarkupRun other than Text_put pass it through)


def _image(lib, offs, case) -> bytes:
    """the data image of a case: the exe's initialised data, every FontRef's handle word pointing at a fake Font (zero
    for the case's `nohandle` fonts), the draw context's words and g_primarySurface"""
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.fill_from_exe(whole_image=True)
        img = bytearray(st.image())
    for k, name in enumerate(FONTS):
        struct.pack_into("<I", img, offs[name] + 4, 0 if k in case["nohandle"] else FAKE + 0x40 * k)
    c = CTX - DATA_VA
    for off, v in ((0x38, case["style"]), (0x3c, case["link"]), (0x40, case["markup"]), (0x4c, case["drop"]),
                   (0x54, case["arrow"]), (0x5c, case["f"][0]), (0x60, case["f"][1]), (0x64, case["f"][2]),
                   (0x68, case["f"][3])):
        struct.pack_into("<I", img, c + off, v)
    struct.pack_into("<I", img, offs["g_primarySurface"], case["primary"])
    return bytes(img)


def _port(lib, offs, case) -> tuple:
    img = _image(lib, offs, case)
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        buf = (L.C.c_uint8 * 0x1000)()
        t = bytes.fromhex(case["text"])
        L.C.memmove(buf, t, len(t))
        st.map(TEXT_VA, buf)
        st.call("gditext.attachScripted", list(SCRIPT))
        try:
            try:
                eax = WC.i32(st.call("drawMarkupRun", [CTX, 0 if case.get("null") else TEXT_VA, case["len"]]).eax)
            except L.SgError as e:
                eax = f"error {e.code}"
            out = st.image()
        finally:
            st.call("gditext.detach", [])
    return eax, out


def _emu(offs, case, img: bytes, cover=None) -> tuple:
    from port.test import cmdemu as CE
    e = CE.Emu()

    def text_width(uc, a, ecx):   # jgld Font::textWidth(s, n) on a fake Font: the scripted rule of the port
        owner = struct.unpack("<I", uc.mem_read(ecx + 4, 4))[0]
        fontref = owner - 4
        s, n = a[0], WC.i32(a[1])
        if s == 0 or n <= 0:
            return 0
        b = bytes(uc.mem_read(s, n))
        return sum(SCRIPT[0] + ((fontref >> 2) & 7) + (c & SCRIPT[1]) for c in b) & 0xFFFFFFFF
    e.stub_py(STUB, 2, text_width, pops=True, name="Font::textWidth")
    fake = bytearray(0x1000)
    for k, name in enumerate(FONTS):
        struct.pack_into("<II", fake, 0x40 * k, FAKE + 0x800, DATA_VA + offs[name] + 4)
    struct.pack_into("<I", fake, 0x800 + 0x10, STUB)
    fake[0x900] = 0xC3
    t = bytes.fromhex(case["text"])
    r = e.run(DRAW_MARKUP_RUN, [0 if case.get("null") else TEXT_VA, case["len"] & 0xFFFFFFFF], img, ecx=CTX,
              blocks=[(TEXT_VA, t + b"\0" * (0x1000 - len(t))), (FAKE, bytes(fake))], budget=2_000_000, cover=cover)
    return (WC.i32(r.eax) if r.eax is not None else f"error {r.err[:60]}"), r.image


def _cases(lib, n, seed):
    offs = _offs(lib)
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.fill_from_exe(whole_image=True)
        toks = _tokens(st.image(), offs)
    rng = random.Random(seed)
    return [_case(rng, offs, toks) for _ in range(n)]


def _digest(eax, image: bytes) -> str:
    c = CTX - DATA_VA
    words = image[c + 0x38:c + 0x6c]
    return hashlib.sha256(json.dumps(eax).encode() + words).hexdigest()[:16]


@emu_oracle
def test_draw_markup_run_equals_the_original(portlib):
    """the original drawMarkupRun (Unicorn) and the port, same image, same scripted font rule: eax and the whole image"""
    _has(portlib)
    try:
        lib = portlib
        lib.exe_loaded() or lib.exe_load()
    except Exception as ex:  # noqa: BLE001
        pytest.skip(f"exe constants: {ex}")
    from port.test import cmdemu as CE
    offs = _offs(portlib)
    bad, errs = [], 0
    cover = {"__functions__": [DRAW_MARKUP_RUN, 0x00483930, 0x00476D40, 0x00476D80, 0x00476DD0]}
    cases = _cases(portlib, 600, "oracle")
    for k, case in enumerate(cases):
        img = _image(portlib, offs, case)
        pe, pimg = _port(portlib, offs, case)
        oe, oimg = _emu(offs, case, img, cover)
        if isinstance(oe, str) and isinstance(pe, str):
            errs += 1
            continue
        if pe != oe or pimg != oimg:
            bad.append((k, case, pe, oe))
    sites, both, missing = CE.coverage_summary(cover)
    print(f"{len(cases)} cases, {errs} fault in both; jcc sites {sites}, both sides {both}, missing {missing}")
    assert not bad, json.dumps(bad[:3])[:1500]


def _golden_now(lib):
    offs = _offs(lib)
    return [_digest(*_port(lib, offs, c)) for c in _cases(lib, GOLDEN_N, "golden")]


GOLDEN_N = 300


def test_draw_markup_run_golden(portlib):
    _has(portlib)
    try:
        portlib.exe_loaded() or portlib.exe_load()
    except Exception as ex:  # noqa: BLE001
        pytest.skip(f"exe constants: {ex}")
    if os.environ.get("SG_EMU_ORACLE") == "update":
        offs = _offs(portlib)
        cases = _cases(portlib, GOLDEN_N, "golden")
        bad = []
        for k, c in enumerate(cases):
            img = _image(portlib, offs, c)
            pe, pimg = _port(portlib, offs, c)
            oe, oimg = _emu(offs, c, img)
            if not (isinstance(pe, str) and isinstance(oe, str)) and (pe != oe or pimg != oimg):
                bad.append(k)
        assert not bad, f"oracle RED on golden cases {bad[:5]}: not pinned"
        GOLDEN.write_text(json.dumps({"drawMarkupRun": _golden_now(portlib)}, indent=0))
    if not GOLDEN.exists():
        pytest.skip("no golden digests (SG_EMU_ORACLE=update)")
    want = json.loads(GOLDEN.read_text())["drawMarkupRun"]
    assert _golden_now(portlib) == want


@pytest.mark.parametrize("mode", [3, 4, 5, 6])
def test_negctl_red_on_golden(portlib, mode):
    _has(portlib)
    if not GOLDEN.exists():
        pytest.skip("no golden digests")
    try:
        portlib.exe_loaded() or portlib.exe_load()
    except Exception as ex:  # noqa: BLE001
        pytest.skip(f"exe constants: {ex}")
    want = json.loads(GOLDEN.read_text())["drawMarkupRun"]
    portlib.negctl("gditext.fontWidth", mode)
    try:
        assert _golden_now(portlib) != want, f"mode {mode} is blind on the golden cases"
    finally:
        portlib.negctl("gditext.fontWidth", 0)


# ------------------------------------------------------------------------------------------------ the interface call
def test_text_put_interface_takes_the_callers_font(portlib):
    """setDrawTarget(font, 0, 0, 0) semantics: the context's +0x5c names another font, the interface measures in `font`;
    a FontRef whose handle word is 0 leaves the context's font (setDrawTarget stores only a created font)"""
    _has(portlib)
    offs = _offs(portlib)
    try:
        portlib.exe_loaded() or portlib.exe_load()
    except Exception as ex:  # noqa: BLE001
        pytest.skip(f"exe constants: {ex}")
    case = {"text": b"Hello world".hex(), "len": 11, "style": 0, "link": 0, "markup": 1, "drop": 0, "arrow": 0,
            "f": [DATA_VA + offs["g_font51b320"], 0, 0, 0], "primary": DATA_VA + offs["g_font519928"], "nohandle": [1]}
    img = _image(portlib, offs, case)
    a, b = DATA_VA + offs["g_font519928"], DATA_VA + offs["g_font51b320"]
    rule = lambda f: sum(SCRIPT[0] + ((f >> 2) & 7) + (c & SCRIPT[1]) for c in b"Hello world")   # noqa: E731
    with portlib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        buf = (L.C.c_uint8 * 64)()
        L.C.memmove(buf, b"Hello world", 11)
        st.map(TEXT_VA, buf)
        st.call("gditext.attachScripted", list(SCRIPT))
        try:
            assert WC.i32(st.call("gditext.textPut", [CTX, a, TEXT_VA]).eax) == rule(a)
            # FONTS[1] (g_font51b320) has no handle in this image: the context's +0x5c stays, which is that same font
            # with no handle, so measureTextWidth returns 0
            assert WC.i32(st.call("gditext.textPut", [CTX, b, TEXT_VA]).eax) == 0
            portlib.negctl("gditext.fontWidth", 7)
            assert WC.i32(st.call("gditext.textPut", [CTX, a, TEXT_VA]).eax) != rule(a)
        finally:
            portlib.negctl("gditext.fontWidth", 0)
            st.call("gditext.detach", [])
