"""Roadmap S5.1: module fttext, the FreeType TextMetrics provider (portsim/sim/fttext.h; notes log/roadmap/S5/1_notes.md).

Evidence:
  * FACES: each of the 15 FontRefs resolves to the font file the host GDI selects for Font::create's LOGFONTA (measured by
    log/roadmap/S5/1_gdi_probe.py: GetFontData's bytes hashed against the font directory and the game's files).
  * FONT METRICS: the five words Font::create stores (+0xc .. +0x1c) equal the GDI provider's and the original's (a
    segment start image), all 15 FontRefs, on this backend and on the Win32 build.
  * RECORDED WIDTHS: port/test/widths_check.py --provider freetype over port/corpus/widths/ (every Font::textWidth,
    Text_put and drawMarkupRun call the original made in ten scenarios): every row with printable text equal; the rows of a
    lone control character are reported (U-0960: the original and GDI give varying results for them).
  * NEGATIVE CONTROLS (g_negctl_fttext, one per primitive): 1 advance + 1, 2 wrong ppem, 3 wrong face for one FontRef,
    4 hinting off; each RED on the recordings and on its own primitive.
  * NO FREETYPE: a build configured with -DPORTSIM_FREETYPE=OFF ($SG_PORTSIM_DLL_NOFT) reports fttext.available 0 and
    throws MissingResource on attach (run in a subprocess: two portsim_c.dll in one process are not wanted).

The Win32 checks use the build directory $SG_PORTSIM_BUILD32 (default port/build/Win32) through widths_check.load_win32.
"""
from __future__ import annotations

import json
import os
import pathlib
import struct
import subprocess
import sys

import pytest

from port.test import lib as L
from port.test import widths_check as WC

ROOT = pathlib.Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.port

DATA_VA = 0x004C1000
TEXT_VA = 0x30000000
OUT_VA = 0x30100000
FONTS = ["g_font83ad20", "g_font51b320", "g_fontRef_519fd8", "g_font51b360", "g_font519928", "g_font51a028",
         "g_fontRef_519a40", "g_font519948", "g_font51b340", "g_font519968", "g_fontRef_821020", "g_fontRef_821f08",
         "g_fontRef_821ec8", "g_fontRef_821f28", "g_fontRef_821ee8"]
# the file the host GDI selects for each FontRef (log/roadmap/S5/1_gdi_probe.log, LIVING-PC, Windows 11)
GDI_FILE = {"g_font83ad20": "host:times.ttf", "g_font51b320": "host:arial.ttf", "g_fontRef_519fd8": "host:arial.ttf",
            "g_font51b360": "game:manu3_.ttf", "g_font519928": "game:manu3_.ttf", "g_font51a028": "game:klepto__.ttf",
            "g_fontRef_519a40": "game:klepto__.ttf", "g_font519948": "game:manu3_.ttf", "g_font51b340": "host:ariali.ttf",
            "g_font519968": "host:arial.ttf", "g_fontRef_821020": "game:klepto__.ttf",
            "g_fontRef_821f08": "game:manu3_.ttf", "g_fontRef_821ec8": "game:manu3_.ttf",
            "g_fontRef_821f28": "game:manu3_.ttf", "g_fontRef_821ee8": "game:manu3_.ttf"}
HOST_FONTS = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"
need_host = pytest.mark.skipif(not all((HOST_FONTS / f).exists() for f in ("times.ttf", "arial.ttf", "ariali.ttf")),
                               reason="the host font directory lacks the system faces the game asks GDI for")
need_widths = pytest.mark.skipif(not WC.scenarios(), reason="no recorded widths (re/tools/widths_rec.py record)")
SCEN = "cov_a_play" if "cov_a_play" in WC.scenarios() else (WC.scenarios() or ["cov_a_play"])[0]


def _offs(lib):
    return {n: o for n, o, _ in lib.objects()}


def _need(lib):
    if not lib.has("fttext.attach"):
        pytest.skip("module fttext not in this build")
    with lib.state() as st:
        if st.call("fttext.available", []).eax != 1:
            pytest.skip("this build has no FreeType")


def _image():
    img = WC.start_image(SCEN)
    if img is None or not WC.INSTALL.is_dir():
        pytest.skip("needs a segment start image and the user's install")
    return img


def _state(lib, img, clear=None):
    st = lib.state()
    st.set_data_base(DATA_VA)
    st.load_image(img)
    st.files_root(WC.INSTALL)
    if clear is not None:   # the provider's own font: clear the stored words so the safety check does not stop the call
        st.write(clear + 0xC, bytes(20))
    return st


def _metrics(st, mod, va):
    return [WC.i32(st.call(f"{mod}.fontMetrics", [va, k]).eax) for k in range(5)]


def _face(lib, st, va):
    buf = getattr(st, "_ft_out", None)
    if buf is None:          # one output block per state, kept alive as long as the state
        buf = st._ft_out = (L.C.c_uint8 * 256)()
        st.map(OUT_VA, buf)
    bold = st.call("fttext.faceOf", [va, OUT_VA, 256]).eax
    return bytes(buf).split(b"\0")[0].decode("latin-1"), bold


# ------------------------------------------------------------------------------------------------ faces and metrics
@need_host
def test_faces_resolve_to_gdis_files(portlib):
    _need(portlib)
    img, offs = _image(), _offs(portlib)
    bad = []
    with _state(portlib, img) as st:
        st.call("fttext.attach", [])
        try:
            for name in FONTS:
                face, bold = _face(portlib, st, DATA_VA + offs[name])
                if face.rsplit("#", 1)[0].lower() != GDI_FILE[name] or bold:
                    bad.append((name, face, bold))
        finally:
            st.call("fttext.detach", [])
    assert not bad, bad


def _metrics_vs_gdi(lib):
    img, offs = _image(), _offs(lib)
    bad = []
    for name in FONTS:
        with _state(lib, img) as st:
            stored = list(struct.unpack("<5i", st.read(offs[name] + 0xC, 20)))
            st.write(offs[name] + 0xC, bytes(20))
            va = DATA_VA + offs[name]
            st.call("gditext.attach", [])
            gdi = _metrics(st, "gditext", va)
            st.call("gditext.detach", [])
            st.call("fttext.attach", [])
            ft = _metrics(st, "fttext", va)
            st.call("fttext.detach", [])
        if not (ft == gdi == stored):
            bad.append((name, ft, gdi, stored))
    return bad


@need_host
@pytest.mark.skipif(os.name != "nt", reason="the GDI provider is the reference")
def test_font_metrics_equal_gdi_and_the_originals(portlib):
    _need(portlib)
    bad = _metrics_vs_gdi(portlib)
    assert not bad, bad


def _win32():
    d = pathlib.Path(os.environ.get("SG_PORTSIM_BUILD32", ROOT / "port" / "build" / "Win32"))
    if not (d / "Release" / "portsim_runner32.exe").exists():
        pytest.skip(f"no Win32 build in {d}")
    lib = WC.load_win32(d)
    if not lib.has("fttext.attach"):
        pytest.skip(f"the Win32 build in {d} has no module fttext")
    return lib


@need_host
@pytest.mark.skipif(os.name != "nt", reason="the GDI provider is the reference")
def test_font_metrics_equal_on_win32():
    lib = _win32()
    _need(lib)
    bad = _metrics_vs_gdi(lib)
    assert not bad, bad


# ------------------------------------------------------------------------------------------------ recorded widths
@need_widths
@need_host
@pytest.mark.parametrize("scenario", WC.scenarios())
def test_widths_equal_the_recorded_ones(portlib, scenario):
    _need(portlib)
    o = WC.check(portlib, scenario, "freetype")
    w, t, m = o["widths"], o["textput"], o["markup"]
    print(json.dumps({k: {x: v for x, v in o[k].items() if x not in ("examples", "control_differ", "per_font")}
                      for k in ("widths", "textput", "markup")}))
    assert o["ok"], json.dumps([w["examples"], t["examples"], m["examples"]])[:2000]
    assert all(d["control_only"] for d in o["diffs"]), [d for d in o["diffs"] if not d["control_only"]][:5]


@need_widths
@need_host
@pytest.mark.skipif(os.name != "nt", reason="Win32 runner")
def test_widths_equal_the_recorded_ones_on_win32():
    lib = _win32()
    _need(lib)
    o = WC.check(lib, SCEN, "freetype")
    assert o["ok"], json.dumps([o[k]["examples"] for k in ("widths", "textput", "markup")])[:2000]


# ------------------------------------------------------------------------------------------------ game files and hosts
def test_game_faces_come_through_gamefiles(portlib):
    """manu3_.ttf / klepto__.ttf are read through the context's GameFiles: the same metrics from in-memory files, and a
    MissingResource when the files are absent (the host directory is not consulted for them)."""
    _need(portlib)
    img, offs = _image(), _offs(portlib)
    va = DATA_VA + offs["g_font51a028"]   # Klepto ITC TT 18, a game face
    with _state(portlib, img) as st:
        st.files_root(None)      # in-memory game files only
        st.files_clear()
        for f in ("manu3_.ttf", "klepto__.ttf"):
            st.files_put(f, (WC.INSTALL / f).read_bytes())
        st.call("fttext.attach", [])
        try:
            assert _metrics(st, "fttext", va) == list(struct.unpack("<5i", st.read(offs["g_font51a028"] + 0xC, 20)))
        finally:
            st.call("fttext.detach", [])
    with _state(portlib, img) as st:
        st.files_root(None)
        st.files_clear()
        st.call("fttext.attach", [])
        try:
            with pytest.raises(L.SgError) as e:
                st.call("fttext.fontMetrics", [va, 0])
            assert e.value.code == L.SG_E_NO_RESOURCE
        finally:
            st.call("fttext.detach", [])


def test_no_host_fonts_is_a_missing_resource_for_the_system_faces(portlib, tmp_path):
    _need(portlib)
    img, offs = _image(), _offs(portlib)
    with _state(portlib, img) as st:
        st.call("fttext.attach", [])
        try:
            path = (L.C.c_uint8 * 512)()
            b = str(tmp_path).encode() + b"\0"
            L.C.memmove(path, b, len(b))
            st.map(OUT_VA + 0x1000, path)
            st.call("fttext.fontDir", [OUT_VA + 0x1000])
            with pytest.raises(L.SgError) as e:
                st.call("fttext.fontMetrics", [DATA_VA + offs["g_fontRef_519fd8"], 0])   # Arial Bold: a host face
            assert e.value.code == L.SG_E_NO_RESOURCE
            # a game face still resolves
            assert WC.i32(st.call("fttext.fontMetrics", [DATA_VA + offs["g_font519928"], 3]).eax) > 0
        finally:
            st.call("fttext.detach", [])


def test_build_without_freetype(tmp_path):
    """$SG_PORTSIM_DLL_NOFT: the -DPORTSIM_FREETYPE=OFF build (log/roadmap/S5/1_mirror.py --build noft)."""
    dll = os.environ.get("SG_PORTSIM_DLL_NOFT")
    if not dll:
        pytest.skip("set SG_PORTSIM_DLL_NOFT to a build configured with -DPORTSIM_FREETYPE=OFF")
    code = ("import sys; sys.path.insert(0, %r)\n"
            "from port.test import lib as L\n"
            "lib = L.Lib(__import__('pathlib').Path(%r))\n"
            "st = lib.state()\n"
            "print('available', st.call('fttext.available', []).eax)\n"
            "try:\n"
            "    st.call('fttext.attach', [])\n"
            "    print('attach ok')\n"
            "except L.SgError as e:\n"
            "    print('attach code', e.code == L.SG_E_NO_RESOURCE, str(e))\n") % (str(ROOT), dll)
    out = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=120)
    assert "available 0" in out.stdout, out.stdout + out.stderr
    # (sg_call reports the code only; the resource name, "FreeType", is not exposed through the C ABI)
    assert "attach code True" in out.stdout, out.stdout + out.stderr


# ------------------------------------------------------------------------------------------------ negative controls
@need_widths
@need_host
@pytest.mark.parametrize("mode", [1, 2, 3, 4])
def test_negctl_red_on_recordings(portlib, mode):
    _need(portlib)
    portlib.negctl("fttext.fontWidth", mode)
    try:
        assert not WC.check(portlib, SCEN, "freetype")["ok"], f"mode {mode} reads GREEN on {SCEN}"
    finally:
        portlib.negctl("fttext.fontWidth", 0)


@need_host
def test_negctl_red_on_their_primitives(portlib):
    """Each control changes what it claims to: 1 an advance, 2 the metric words, 3 the face of g_fontRef_519fd8 only,
    4 an advance of a hinted face (Arial 10, which has no hdmx record at 10 ppem)."""
    _need(portlib)
    img, offs = _image(), _offs(portlib)
    arial = offs["g_fontRef_519fd8"]
    va = DATA_VA + arial
    text = bytes(range(0x20, 0x7F))

    def run(mode):
        portlib.negctl("fttext.fontWidth", mode)
        try:
            with _state(portlib, img, clear=arial) as st:
                st.write(offs["g_font51b320"] + 0xC, bytes(20))
                st.call("fttext.attach", [])
                try:
                    buf = (L.C.c_uint8 * 0x100)()
                    st.map(TEXT_VA, buf)
                    widths = []
                    for b in text:
                        buf[0] = b
                        widths.append(WC.i32(st.call("fttext.fontWidth", [va, TEXT_VA, 1]).eax))
                    return {"widths": widths, "metrics": _metrics(st, "fttext", va), "face": _face(portlib, st, va)[0],
                            "other": _face(portlib, st, DATA_VA + offs["g_font51b320"])[0]}
                finally:
                    st.call("fttext.detach", [])
        finally:
            portlib.negctl("fttext.fontWidth", 0)

    base = run(0)
    m1, m2, m3, m4 = run(1), run(2), run(3), run(4)
    assert all(b + 1 == x for b, x in zip(base["widths"], m1["widths"])), "mode 1: every advance + 1"
    assert m2["metrics"] != base["metrics"], "mode 2: the ppem changes the metric words"
    assert "times" in m3["face"].lower() and m3["other"] == base["other"], "mode 3: one FontRef's face only"
    assert m4["widths"] != base["widths"], "mode 4: hinting off changes an advance of Arial 10"
