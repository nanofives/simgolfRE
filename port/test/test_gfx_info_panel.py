"""Roadmap S3.16: the golfer info panel's DRAWING (drawGolferInfoPanel, dialogs.cpp draw_golfer_info_panel; notes
log/roadmap/S3/16_notes.md).

EMULATOR CALL ORACLE (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py, like test_gfx_hud.py's for the employee panels): the
ORIGINAL drawGolferInfoPanel runs under Unicorn on test_dialogs.py's panel cases (the same image, arguments, cursor, Text_put
rule and scripted peers) with every drawing callee replaced by a recorder (its own `ret N` convention): Surface_blit2,
Surface_stretchBlit, Image_draw, Surface_drawScaled, Win_fillRect, setDrawTarget, drawCenteredText / 2, drawText4049d0,
drawTextSurface, fillWindowRect, Widget_fillRect, drawPanelBackdrop; drawThoughtBubble runs for real (its drawing callees are
recorded). The port runs with a graphics backend attached and logs every call SkDraw hands over (dialogs.recordDraw). The
two sequences must be equal: callee, its object and arguments, and the text of a text call. skillPointDialog's drawing pass
(record byte +0x18) is S2.10's and is switched off in these cases. The default suite pins the digest of the port's logs on
fixed cases (taken after the oracle was GREEN) and checks the negative controls against it.
"""
from __future__ import annotations

import hashlib
import os
import struct

import pytest

from port.test import lib as L
from port.test import test_dialogs as TD

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")
LOG_VA, LOG_CAP = 0x33000000, 0x40000

# (address, name, stack arguments, thiscall): the panel's drawing callees and drawThoughtBubble's
RECORDED = [
    (0x004740F0, "Surface_blit2", 5, True),
    (0x00473F60, "Surface_stretchBlit", 5, True),
    (0x00473E60, "Image_draw", 4, True),
    (0x00473CB0, "Surface_drawScaled", 7, True),
    (0x00478AF0, "Win_fillRect", 5, True),
    (0x004762D0, "setDrawTarget", 4, True),
    (0x00480B00, "fillWindowRect", 5, True),
    (0x00478B80, "Widget_fillRect", 5, True),
    (0x00404B70, "drawCenteredText", 4, False),
    (0x00404BC0, "drawCenteredText2", 4, False),
    (0x004049D0, "drawText4049d0", 4, False),
    (0x00404AD0, "drawTextSurface", 4, False),
    (0x00432620, "drawPanelBackdrop", 2, False),
]
TEXT_CALLS = {"drawCenteredText", "drawCenteredText2", "drawText4049d0", "drawTextSurface"}


@pytest.fixture(scope="module")
def plib():
    lib = L.load()
    if not lib.has("dialogs.recordDraw"):
        pytest.skip("the library has no dialogs.recordDraw (S3.16)")
    return lib


def panel_cases(lib, n, seed):
    """test_dialogs.py's drawGolferInfoPanel cases with the skill dialog's drawing pass switched off (record byte +0x18)"""
    goff, _ = TD.objs(lib)["g_golfers"]
    out = []
    for img, args, inputs, cursors, rule, salt in TD.cases(lib, "drawGolferInfoPanel", n, seed):
        b = bytearray(img)
        b[goff + 0x100 * args[0] + 0x18] = 0
        out.append((bytes(b), args, inputs, cursors, rule, salt))
    return out


def emu_calls(img, args, cursors, rule, salt):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in TD.DROPPED:
        emu.drop(a)
    for a in TD.NORET:
        emu.stub(a, 0, 0, False)
    emu.stub(TD.TEXT_PUT, 1, ("width",) + tuple(rule), True)
    seq = TD.CursorSeq(cursors)
    emu.stub(TD.VIEW_TO_LOCAL, 2, ("cursor", seq), True)
    for a, n in TD.EH_VEC.items():
        emu.stub(a, n, 0, True)
    for nm, (a, n) in TD.SCRIPTED.items():
        emu.stub(a, n, (lambda nm: lambda av, ecx: TD.script_value(nm, av, salt))(nm), False, nm)
    calls = []

    def cstr(uc, p):
        s = bytearray()
        while len(s) < 0x400:
            ch = bytes(uc.mem_read(p + len(s), 1))
            if ch == b"\0":
                break
            s += ch
        return s.decode("latin-1")

    def rec(name, thiscall):
        def fn(uc, a, ecx):
            vals = ([ecx] if thiscall else []) + list(a)
            line = name + "".join(" " + str(v & 0xFFFFFFFF) for v in vals)
            if name in TEXT_CALLS:
                line += "|" + cstr(uc, a[0])
            calls.append(line)
            return 0
        return fn

    for addr, name, nargs, thiscall in RECORDED:
        emu.stub_py(addr, nargs, rec(name, thiscall), emu.ret_pop(addr) > 0, name)
    r = emu.run(TD.ENTRY["drawGolferInfoPanel"], args, img)
    return calls, r


def port_calls(lib, img, args, cursors, rule, salt, negctl=0):
    import ctypes as C
    prev = lib.negctl("drawGolferInfoPanel", negctl) if negctl else None
    try:
        with lib.state() as st:
            if st.lib.has("audio_exe.stubRequests"):
                st.call("audio_exe.stubRequests", [1])
            st.set_data_base(TD.DATA_VA)
            st.load_image(img)
            cw = [v & 0xFFFFFFFF for p in cursors for v in p] or [0]
            cbuf = (C.c_uint32 * len(cw))(*cw)
            st.map(TD.CURSOR_VA, cbuf, readonly=True)
            ibuf = (C.c_uint32 * 1)(0)
            st.map(TD.INPUT_VA, ibuf, readonly=True)
            st.call("dialogs.setScript", [TD.INPUT_VA, 0, TD.CURSOR_VA, len(cursors)])
            st.call("commands.setTextWidth", [rule[0], rule[1]])
            plog = (C.c_uint32 * (1 + 8 * TD.PEER_LOG_CAP))()
            st.map(TD.PEER_LOG_VA, plog)
            for nm in TD.SCRIPTED:
                st.call("dialogs.scriptPeer", [TD.fnv1a(nm), salt, TD.PEER_LOG_VA, TD.PEER_LOG_CAP])
            dlog = (C.c_uint8 * LOG_CAP)()
            st.map(LOG_VA, dlog)
            st.call("gfx.attach", [])
            st.call("dialogs.recordDraw", [1])
            try:
                st.call("drawGolferInfoPanel", [a & 0xFFFFFFFF for a in args])
                code = 0
            except L.SgError as e:
                code = e.code
            st.call("dialogs.recordDraw", [0])
            n = st.call("dialogs.drawLog", [LOG_VA, LOG_CAP]).eax
            st.call("gfx.detach", [])
            st.call("dialogs.clearScript", [])
            st.call("dialogs.clearPeers", [])
            st.call("commands.clearTextWidth", [])
            text = bytes(dlog[:min(n, LOG_CAP)]).decode("latin-1")
            return [ln for ln in text.split("\n") if ln], code
    finally:
        if negctl:
            lib.negctl("drawGolferInfoPanel", prev or 0)


@need_exe
@emu_oracle
def test_info_panel_calls_equal_the_original(plib):
    compared, faults, bad = 0, 0, []
    for i, (img, args, _inputs, cursors, rule, salt) in enumerate(panel_cases(plib, 300, 0x3516)):
        want, r = emu_calls(img, args, cursors, rule, salt)
        got, code = port_calls(plib, img, args, cursors, rule, salt)
        if r.err:
            faults += 1      # the original faults or waits (an unmapped read through a varied record): not compared
            continue
        compared += 1
        if got != want:
            k = next((j for j in range(min(len(got), len(want))) if got[j] != want[j]), min(len(got), len(want)))
            bad.append((i, k, want[k:k + 2], got[k:k + 2]))
    assert compared >= 200, f"only {compared} cases ran in the original ({faults} faulted)"
    assert not bad, f"{len(bad)} of {compared} cases differ; first {bad[:3]}"


GOLDEN_N = 120
GOLDEN = "299d474290a0d26a"   # sha256 of the port's logs over panel_cases(GOLDEN_N, 0x3516b), pinned after the oracle was GREEN


def _digest(lib, negctl=0) -> str:
    h = hashlib.sha256()
    for img, args, _inputs, cursors, rule, salt in panel_cases(lib, GOLDEN_N, 0x3516B):
        got, code = port_calls(lib, img, args, cursors, rule, salt, negctl)
        h.update(("\n".join(got) + f"#{code}\n").encode("latin-1"))
    return h.hexdigest()[:16]


@need_exe
def test_info_panel_calls_golden(plib):
    d = _digest(plib)
    if os.environ.get("SG_EMU_ORACLE") == "update" or GOLDEN == "pending":
        print("GOLDEN digest", d)
        if GOLDEN == "pending":
            pytest.skip(f"golden not pinned yet: {d}")
    assert d == GOLDEN


@need_exe
@pytest.mark.parametrize("mode,what", [(6, "the name line one pixel lower"), (7, "the meters one pixel lower")])
def test_info_panel_calls_negctl_red(plib, mode, what):
    assert _digest(plib, mode) != _digest(plib), what
