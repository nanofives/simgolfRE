"""Phase 3.8 stream 38i: semantic input actions (portsim/sim/semantic.h, module `semantic`) and gate (d), a championship
driven by semantic commands only (port/test/semantic.py `xlate` / `gate`). Notes: log/p38/38i_notes.md."""
from __future__ import annotations

import re

import pytest

from port.test import integrator as I
from port.test import lib as L
from port.test import segments as S
from port.test import semantic as SEM
from port.test import trace_commands as TC

ROOT = SEM.ROOT
need_segments = pytest.mark.skipif(not S.SEG_DIR.exists(), reason="segment corpus not present (port/corpus/segments, local)")
need_install = pytest.mark.skipif(not S.INSTALL.is_dir(), reason="the user's install (original/) is not present")
CHAMP = ["championship/00", "championship/01", "championship/02"]
KEY, DOWN, CLICK, TILE = SEM.KIND_KEY, SEM.KIND_DOWN, SEM.KIND_CLICK, SEM.KIND_TILE


def _h1_cases():
    """The keys mainLoop H1 switches on, read from the port's own frame step (tick.cpp: sw_h1_key's table is key - 9 ->
    case, sw_h1_41de23's ranges are code - 0x21)."""
    src = (ROOT / "port" / "src" / "sim" / "tick.cpp").read_text(encoding="utf-8")
    body = src[src.index("static int sw_h1_key(int i)"):]
    body = body[:body.index("return 56;")]
    chars = {int(a) + 9 for a, _ in re.findall(r"\{(\d+), (\d+)\}", body)}
    codes = {0x21} | set(range(0x24, 0x29)) | set(range(0x70, 0x7A))   # sw_h1_41de23: i 0, 3..7, 79..88
    assert "if (i >= 3 && i <= 7) return i - 2;" in src and "if (i >= 79 && i <= 88) return i - 73;" in src
    return chars, codes


# ------------------------------------------------------------------------------------------- static
@pytest.mark.static
def test_key_table_names_every_h1_case():
    keys = SEM.key_table()
    assert len(keys) == 75 and len({(v, c) for _, v, c in keys.values()}) == 75
    assert len({n for n, _, _ in keys.values()}) == 75
    chars, codes = _h1_cases()
    assert len(chars) == 56
    have_ch = {c for _, _, c in keys.values() if c}
    have_code = {v for _, v, c in keys.values() if c == 0}
    assert chars <= have_ch, sorted(chars - have_ch)
    assert codes <= have_code, sorted(codes - have_code)


def _b(context, *cmds):
    return TC.Batch(1, {"body": 0x0040FC2F, "modal": 0x0045C0CC}[context], [TC.RawCmd(*c) for c in cmds])


@pytest.mark.static
def test_translate_batch_shapes():
    keys = SEM.key_table()
    by = {n: i for i, (n, _, _) in keys.items()}
    t = lambda b: SEM.translate_batch(b, keys)[:3]  # noqa: E731
    # Esc: WM_KEYDOWN + WM_CHAR; 'n' with two WM_CHARs ('N' then 'n'): the last character counts
    assert t(_b("body", (8, 27, 0), (7, 27, 27)))[0] == [{"kind": KEY, "arg": [by["Escape"]] + [0] * 7}]
    assert t(_b("body", (8, 78, 0), (7, 78, 78), (7, 110, 78)))[0][0]["arg"][0] == by["AbandonPlay"]
    # a non-character key, a release, a character with no key-down (exact form), its named form
    assert t(_b("body", (7, 0, 0x70)))[0][0]["arg"][0] == by["ReportCourse"]
    assert t(_b("body", (9, 0, 0))) == ([], ["release (no command)"], [])
    assert t(_b("body", (7, 108, 27)))[0] == [{"kind": KEY, "arg": [0, 108, 0, 27, 0, 0, 0, 0]}]
    assert SEM.translate_batch(_b("body", (7, 108, 27)), keys, True)[0][0]["arg"][0] == by["ShotStyle4"]
    # text typed into a modal screen: the character form (same lowering as the named key)
    assert t(_b("modal", (8, 66, 0), (7, 98, 66), (7, 98, 66)))[0] == [{"kind": KEY, "arg": [0, 98, 66, 0, 0, 0, 0, 0]}]
    assert t(_b("modal", (8, 27, 0), (7, 27, 27)))[0][0]["arg"][0] == by["Escape"]
    # pointer: down, the click with Shift held, a right click, a press without its unpoint (gap), an unknown handler (gap)
    assert t(_b("body", (2, 415, 508)))[0] == [{"kind": DOWN, "arg": [415, 508, 1, 0, 0, 0, 0, 0]}]
    cmds = t(_b("body", (1, 654, 197, 0, 0x8000), (3, 0, 0)))[0]
    assert cmds[0]["kind"] == CLICK and cmds[0]["arg"][:4] == [654, 197, 1, 2]
    assert t(_b("modal", (4, 10, 20), (6, 0, 0)))[0][0]["arg"][:3] == [10, 20, 2]
    assert t(_b("body", (1, 5, 5)))[2] and t(_b("body", (10, 5, 5)))[2]


@pytest.mark.static
@need_segments
def test_championship_translates_every_dispatch():
    """Gate d, translation half: all 145 recorded dispatches of championship/00..02 have a semantic translation (no gap):
    50 Escape taps, 3 ZoomOut, 3 ZoomIn, 57 releases (no command), 16 pointer-downs, 16 clicks."""
    rows = [r for s in CHAMP for r in SEM.xlate(s)]
    assert len(rows) == 145 and not [r for r in rows if r.gap]
    cmds = [c for r in rows for c in r.commands]
    assert all(c["kind"] in (KEY, DOWN, CLICK) for c in cmds)
    keys = SEM.key_table()
    names = [keys[c["arg"][0]][0] for c in cmds if c["kind"] == KEY]
    assert names.count("Escape") == 50 and names.count("ZoomOut") == 3 and names.count("ZoomIn") == 3
    assert sum(1 for c in cmds if c["kind"] == CLICK) == 16 and sum(1 for c in cmds if c["kind"] == DOWN) == 16


# ------------------------------------------------------------------------------------------- port
def _need_module(portlib):
    if not portlib.has("semantic.apply"):
        pytest.skip("the semantic module is not in this build (log/p38/38i_merge.md)")


def _image_after(portlib, start, calls):
    with portlib.state() as st:
        st.set_data_base(S.DATA_VA)
        st.load_image(start)
        for name, args in calls:
            st.call(name, args)
        return st.image()


def _raw(h, a0=0, a1=0, ctrl=0, shift=0):
    return ("commands.rootHandler", [h, a0, a1, ctrl, shift, 0])


def _sem(kind, *arg):
    return ("semantic.apply", [kind] + list(arg) + [0] * (7 - len(arg)))


@pytest.mark.port
@need_segments
def test_every_key_action_equals_its_raw_handler_calls(portlib):
    """Each key action leaves exactly the state its raw handler pair leaves (onChar(vk) + onKey(ch, vk), or onKey(0, vk)
    for a non-character key), from championship/00's and course_season/00's start images."""
    _need_module(portlib)
    for sg in ("championship/00", "course_season/00"):
        start = SEM.seg_of(sg).start
        for kid, (name, vk, ch) in SEM.key_table().items():
            raw = [_raw(8, vk), _raw(7, ch, vk)] if ch else [_raw(7, 0, vk)]
            assert _image_after(portlib, start, raw) == _image_after(portlib, start, [_sem(KEY, kid)]), (sg, name)
        # the character forms: Char(ch, vk) and a character with no key-down (onKey(ch, code) alone)
        assert _image_after(portlib, start, [_raw(8, 0x41), _raw(7, 0x61, 0x41)]) == \
            _image_after(portlib, start, [_sem(KEY, 0, 0x61, 0x41)])
        assert _image_after(portlib, start, [_raw(7, 0x6C, 27)]) == _image_after(portlib, start, [_sem(KEY, 0, 0x6C, 0, 27)])


@pytest.mark.port
@need_segments
def test_pointer_actions_equal_their_raw_handler_calls(portlib):
    _need_module(portlib)
    start = SEM.seg_of("championship/00").start
    left = [_raw(2, 415, 508), _raw(1, 415, 508), _raw(3)]
    assert _image_after(portlib, start, left) == _image_after(portlib, start, [_sem(DOWN, 415, 508, 1), _sem(CLICK, 415, 508, 1)])
    right = [_raw(5, 300, 200), _raw(4, 300, 200, 0x8000, 0), _raw(6)]
    assert _image_after(portlib, start, right) == \
        _image_after(portlib, start, [_sem(DOWN, 300, 200, 2), _sem(CLICK, 300, 200, 2, 1)])
    shift = [_raw(1, 654, 197, 0, 0x8001), _raw(3)]
    assert _image_after(portlib, start, shift) == _image_after(portlib, start, [_sem(CLICK, 654, 197, 1, 2)])


@pytest.mark.port
@need_segments
def test_click_route_and_refusal(portlib):
    """The route of a click on championship/00's start image: (415, 508) is the golfer panel (H2's panel band in panel
    mode 3 / 4), (654, 197) the course; a Click stating another target is refused before any write, and the route itself
    changes nothing."""
    _need_module(portlib)
    start = SEM.seg_of("championship/00").start
    with portlib.state() as st:
        st.set_data_base(S.DATA_VA)
        st.load_image(start)
        r = st.call("semantic.route", [415, 508, 1]).eax
        assert r & 0xFF == 2 and r >> 8 == 0x100
        assert st.call("semantic.route", [654, 197, 1]).eax & 0xFF in (1, 4)
        assert st.call("semantic.route", [100, 500, 1]).eax & 0xFF == 3
        assert st.image() == start
        with pytest.raises(L.SgError) as e:
            st.call("semantic.apply", [CLICK, 654, 197, 1, 0, 3, -1, -1])   # target Hud at a course point
        assert e.value.code == L.SG_E_NO_RESOURCE and st.image() == start
        tile = st.call("semantic.routeTile", [654, 197, 1]).eax
        tx, ty = tile & 0xFFFF, tile >> 16
        tgt = st.call("semantic.route", [654, 197, 1]).eax & 0xFF
        st.call("semantic.apply", [CLICK, 654, 197, 1, 0, tgt, tx, ty])     # the right tile: accepted
        with pytest.raises(L.SgError):
            st.load_image(start)
            st.call("semantic.apply", [CLICK, 654, 197, 1, 0, tgt, tx + 1, ty])   # another tile: refused


@pytest.mark.port
@need_segments
def test_tile_click_selects_its_tile(portlib):
    """TileClick(t) clicks t's screen centre: the logged route's tile is t and the state equals the raw handler calls
    at that point; negative control 4 (one row down) and an off-screen tile are refused or land elsewhere."""
    _need_module(portlib)
    start = SEM.seg_of("championship/00").start
    with portlib.state() as st:
        st.set_data_base(S.DATA_VA)
        st.load_image(start)
        tile = st.call("semantic.routeTile", [400, 250, 1]).eax
        tx, ty = tile & 0xFFFF, tile >> 16
        st.call("semantic.logClear", [])
        st.call("semantic.logOn", [1])
        st.call("semantic.apply", [TILE, tx, ty, 1, 0, 0, 0, 0])
        rec = [st.call("semantic.logGet", [0, f]).eax for f in range(8)]
        st.call("semantic.logOn", [0])
        img = st.image()
    x, y = SEM._s32(rec[0]), SEM._s32(rec[1])
    assert (rec[6], rec[7]) == (tx, ty)
    assert img == _image_after(portlib, start, [_raw(2, x, y), _raw(1, x, y), _raw(3)])
    with portlib.state() as st:
        st.set_data_base(S.DATA_VA)
        st.load_image(start)
        with pytest.raises(L.SgError):
            st.call("semantic.apply", [TILE, 200, 200, 1, 0, 0, 0, 0])   # no such tile on screen
    portlib.negctl("semantic.apply", 4)
    try:
        with portlib.state() as st:
            st.set_data_base(S.DATA_VA)
            st.load_image(start)
            try:
                st.call("semantic.apply", [TILE, tx, ty, 1, 0, 0, 0, 0])
                moved = st.image() != img
            except L.SgError:
                moved = True
        assert moved
    finally:
        portlib.negctl_reset()


def _spec(portlib) -> I.LibSpec:
    return I.LibSpec(backend="x64", dll=str(portlib.path))


@pytest.mark.port
@need_segments
@need_install
def test_gate_d_championship_by_semantic_commands_only(portlib):
    """Gate d: championship/00..02 PASS with every recorded dispatch replaced by its semantic translation (no RootHandler
    command from the trace), in the probe run (click targets unstated) and the final run (targets stated and checked)."""
    _need_module(portlib)
    if not portlib.has("integrator.setBatchOverrides"):
        pytest.skip("the integrator module is not in this build")
    for sg in CHAMP:
        d = SEM.gate_segment(_spec(portlib), sg)
        assert d["status"] == "PASS" and d["probe"] == "PASS", (sg, d["record"], d["objects"], d["run_detail"][:200])
        assert d["gaps"] == 0 and d["applied"] == d["batches"] and d["clicks_unrouted"] == 0


@pytest.mark.port
@need_segments
@need_install
@pytest.mark.parametrize("mode", [1, 2, 3])
def test_gate_d_negative_controls_read_red(portlib, mode):
    """The semantic module's wrong lowerings fail championship/00 driven by semantic commands: 1 the left click lowered as
    a right click, 2 a character key without its WM_CHAR, 3 a stated target that holds refused."""
    _need_module(portlib)
    d = SEM.gate_segment(_spec(portlib), "championship/00", negctl={"semantic.apply": mode})
    assert d["status"] == "FAIL", d
