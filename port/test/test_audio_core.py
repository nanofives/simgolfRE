"""The audio core (step S4.2, log/roadmap/S4/2_notes.md): sound.dll's image and SoundState in the port's address space, the
call dispatcher (claims, thunks, port functions, fallbacks, the host), the script host, the AudioSink route, and the first
family of sound.dll functions judged against the emulated original (port/test/sound_replay.py).

Default runs replay the recorded bundles (port/corpus/audio, local) and check the golden digests pinned from the emulator
(port/test/golden_audio_core.json); SG_EMU_ORACLE=1 runs the live oracle (Node fixture VM + Unicorn over the user's
sound.dll) for every key in three layouts and the negative controls against it. The C ABI addition (sg_snd_set_host) is
in the S4 mirror build until merged: sound_replay.mirror_dll() picks it. Since S4.2b also: the core's stack (SndFrame, the
dispatcher's callee entry esp), the x87 unit's host box, sound.dll's __ftol and the import-stub route (U-1472).

    ~/bin/py -3.12 -m pytest port/test/test_audio_core.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_core.py -k oracle
"""
from __future__ import annotations

import os
import pathlib
import struct
import subprocess
import sys

import pytest

ROOT = next(p for p in pathlib.Path(__file__).resolve().parents if (p / "re" / "tools").is_dir())
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import lib as L  # noqa: E402
from port.test import sound_replay as R  # noqa: E402

pytestmark = pytest.mark.port
SOUND_DLL = ROOT / "original" / "sound.dll"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for("S4.2")


def _has_snd(lib) -> bool:
    try:
        return "audio_core.setup" in lib.functions()
    except Exception:  # noqa: BLE001
        return False


@pytest.fixture(scope="module")
def sndlib():
    p = R.mirror_dll()
    if not p.exists():
        pytest.skip(f"{p} not built (log/roadmap/S4/2_mirror.py)")
    lib = R.load_lib()
    if not _has_snd(lib):
        pytest.skip("the DLL has no audio_core module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def sndlib32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except L32.RunnerUnavailable as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if not _has_snd(lib):
        pytest.skip("the Win32 DLL has no audio_core module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden("S4.2")
    if not g:
        pytest.skip("no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --update)")
    return g


def _need_dll_file():
    if not SOUND_DLL.exists():
        pytest.skip("original/sound.dll (the user's file) is absent")


# ================================================================================================ static
@pytest.mark.static
def test_soundstate_header_current():
    _need_dll_file()
    r = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "soundstate_gen.py"), "--check"], capture_output=True,
                       text=True)
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.static
def test_every_step_row_has_a_key_or_a_derived_key():
    rows = R.step_rows("S4.2")
    names = {R.port_name(k) for k in KEYS}
    assert len(rows) == 60
    assert {R.SND + r["name"] for r in rows} == names


@pytest.mark.static
def test_goldens_cover_every_key():
    g = R.load_golden("S4.2")
    if not g:
        pytest.skip("no golden file")
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]), k
        assert g[k]["tier"] in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"), k
        assert len(g[k]["x87_exit"]) == g[k]["vectors"], k


# ================================================================================================ the hook-source converter
def _conv(args):
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import snd_hook2port as H
    import contextlib
    import io
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = H.main(args)
    return rc, buf.getvalue()


@pytest.mark.static
def test_converter_rewrites_the_idioms(tmp_path):
    """findAndUnlinkNode (c3dj): accessors -> ld / st, operator delete -> the dispatcher, VAs in comments -> RVAs; a thunk
    call (cmdNode::scalarDeletingDtor -> pairClear) keeps its rva; a vtable slot call (ListValues_callSlot18) becomes
    snd_vcall / snd_call_va; flushSoundList's IAT call becomes snd_import with the file's constant."""
    out = tmp_path / "c.cpp"
    rc, _ = _conv(["--rva", "0x136c0", "--rva", "0xcc40", "--rva", "0x1bc00", "--rva", "0x8470", "--out", str(out)])
    t = out.read_text()
    assert rc == 0 and "TODO(convert)" not in t.split("// TODO list")[0].replace("TODO(convert): give the body", "")
    assert "st<uint32_t>(ctx, hdr + 8, 0)" in t or "st<uint32_t>(ctx, hdr + 8, 0);" in t
    assert "snd_delete(ctx, node)" in t
    assert "snd_call(ctx, 0x00001816, SndAbi::Thiscall, {self})" in t
    assert "snd_import(ctx, kIatTimeGetTime, SndAbi::Stdcall, {})" in t and "constexpr uint32_t kIatTimeGetTime = 0xb8654;" in t
    assert "snd_call_va(ctx, ld<u32>(ctx, vt + 0x18), SndAbi::Thiscall, {obj})" in t
    import re
    assert not [m for m in re.findall(r"0x([0-9a-fA-F]{8})", t) if 0x10000000 <= int(m, 16) < 0x10200000]


def test_converter_output_compiles():
    """the whole S4.2 family converted as one translation unit compiles with MSVC /W4 against the port headers"""
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import snd_hook2port as H
    if not H.VCVARS.exists():
        pytest.skip("MSVC Build Tools absent")
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        out = pathlib.Path(d) / "tu.cpp"
        rc, txt = _conv(["--step", "S4.2", "--tu", "--compile", "--out", str(out)])
        assert rc == 0, txt
        assert "0 errors, 0 warnings" in txt


# ================================================================================================ default: bundles + goldens
def _replay(lib, key, golden):
    if R.load_bundle(key) is None:
        pytest.skip(f"no bundle for {key} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --update)")
    return R.replay_key(lib, key, golden)


@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(sndlib, golden, key):
    r = _replay(sndlib, key, golden)
    assert R.replay_ok(r) and r.fallbacks == 0, r


@pytest.mark.x87
def test_every_key_equals_golden_win32(sndlib32, golden):
    bad = []
    for key in KEYS:
        if R.load_bundle(key) is None:
            pytest.skip("bundles missing")
        r = R.replay_key(sndlib32, key, golden)
        if not (R.replay_ok(r) and r.fallbacks == 0):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


def _functions():
    out = {}
    for k in KEYS:
        out.setdefault(R.port_name(k), []).append(k)
    return out


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_negctl_red_against_golden(sndlib, golden, fn):
    """Mode 1 of every function: at least one vector of its keys no longer hashes to the emulator's digest."""
    keys = _functions()[fn]
    for k in keys:
        if R.load_bundle(k) is None:
            pytest.skip("bundles missing")
    sndlib.negctl(fn, 1)
    try:
        bad = sum(r.vectors - r.passed for r in (R.replay_key(sndlib, k, golden) for k in keys))
    finally:
        sndlib.negctl(fn, 0)
    assert bad > 0, f"{fn}: mode 1 reads GREEN on every vector"


def test_crt_strncpy_negctl_red(sndlib, golden):
    """The CRT strncpy the dispatcher serves by rva (getNodeByIndex's callee): no NUL padding reads RED."""
    if R.load_bundle("getNodeByIndex") is None:
        pytest.skip("bundles missing")
    sndlib.negctl("audio_core.crt.strncpy", 1)
    try:
        r = R.replay_key(sndlib, "getNodeByIndex", golden)
    finally:
        sndlib.negctl("audio_core.crt.strncpy", 0)
    assert r.passed < r.vectors


# ================================================================================================ the core on constructed states
BASE = 0x04C00000


@pytest.fixture()
def img_state(sndlib):
    """A context with the user's sound.dll mapped by the core itself (map_image) and one scratch block."""
    _need_dll_file()
    st = sndlib.state()
    st.files_put("sound.dll", SOUND_DLL.read_bytes())
    st.call("audio_core.attach", [BASE, 1])
    blk = st.map(0x30000000, bytearray(0x1000))
    yield st, blk
    st.call("audio_core.detach", [])
    st.close()


def _file_dword(rva: int) -> int:
    import pefile
    pe = pefile.PE(str(SOUND_DLL), fast_load=True)
    v = struct.unpack("<I", pe.get_memory_mapped_image()[rva:rva + 4])[0]
    pe.close()
    return v


def _rd(st, va):
    return st.call("snd.getField204", [va - 0x204]).eax


def test_map_image_relocates_and_keeps_data(img_state):
    st, _ = img_state
    from port.test.sound_replay import E
    vt = 0x5b19c                                           # snd_rva::vt_CtrlObject (soundstate.h)
    ibase = E._raw(str(SOUND_DLL))[5]
    assert _rd(st, BASE + vt) == (_file_dword(vt) - ibase + BASE) & 0xFFFFFFFF
    d = R.data_rva() + 0xd90                               # SoundState::g_fccMThd
    assert _rd(st, BASE + d) == _file_dword(d)


def test_map_image_refuses_another_build(sndlib):
    _need_dll_file()
    b = bytearray(SOUND_DLL.read_bytes())
    import pefile
    pe = pefile.PE(data=bytes(b), fast_load=True)
    sec = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".rdata")
    b[sec.PointerToRawData + 0x40] ^= 1
    st = sndlib.state()
    try:
        st.files_put("sound.dll", bytes(b))
        with pytest.raises(L.SgError) as e:
            st.call("audio_core.attach", [BASE, 1])
        assert e.value.code == L.SG_E_NO_RESOURCE
    finally:
        st.close()


def _args(st, abi, slots):
    st.files_put("$audio/args", struct.pack("<II", abi, len(slots)) + b"".join(struct.pack("<I", s) for s in slots))


def _report(st):
    st.call("audio_core.report", [])
    return R.parse_report(st.files_read("$audio/out"))


def test_thunk_dispatches_to_the_port(img_state):
    """rva 0x1816 is the incremental-link thunk of pairClear (cmdNode::scalarDeletingDtor calls it): the dispatcher follows
    the jmp and runs the registered port function; no host call."""
    st, blk = img_state
    blk[0:8] = b"\x11" * 8
    st.call("audio_core.reset", [])
    _args(st, 2, [0x30000000])
    st.call("audio_core.dispatch", [BASE + 0x1816])
    assert bytes(blk[0:8]) == b"\0" * 8
    rep = _report(st)
    assert rep["dispatched"] == "1" and rep["external"] == rep["claimed"] == rep["fallback"] == "0"


def _script(entries):
    return R.script_blob(entries)


def _entry(kind, va, abi, args, eax=0, writes=(), maps=()):
    return {"kind": kind, "va": va, "abi": abi, "nargs": len(args), "want": 0, "args": list(args), "eax": eax, "edx": 0,
            "has_st0": 0, "st0": 0, "writes": [(a, b.hex()) for a, b in writes], "maps": [(a, b.hex()) for a, b in maps]}


def _unported(sndlib) -> int:
    """The rva of a sound.dll function body no module of this build registers (the first such row of
    re/roadmap/S4_functions.tsv that is not dead and does not start with a thunk or an import stub; since every step row is
    ported, one of the rows no step owns)."""
    import pefile
    pe = pefile.PE(str(SOUND_DLL), fast_load=True)
    img = pe.get_memory_mapped_image()
    pe.close()
    have = set(sndlib.functions())
    for r in R.step_rows_all():
        rva = int(r["rva"], 16)
        if R.SND + r["name"] not in have and r["oracle"] != "dead" and img[rva] not in (0xE9, 0xFF):
            return rva
    raise AssertionError("every row is ported")


def test_fallback_and_claim_go_to_the_host(img_state, sndlib):
    """An unported sound.dll function is a FALLBACK host call (counted, its rva logged); a claimed address is served by the
    host even when a port function exists (a fixture's stand-in); the script host applies the recorded writes."""
    st, blk = img_state
    unported = _unported(sndlib)
    st.files_put("$audio/script", _script([
        _entry(2, BASE + unported, 2, [0x30000000], eax=7, writes=[(0x30000010, b"\x05\x06")]),
        _entry(1, BASE + 0x8840, 2, [0x30000000], eax=9)]))
    st.files_put("$audio/claims", struct.pack("<I", BASE + 0x8840))
    st.call("audio_core.setup", [BASE, R.SETUP_CLAIMS | R.SETUP_SCRIPT | R.SETUP_RESET])
    _args(st, 2, [0x30000000])
    assert st.call("audio_core.dispatch", [BASE + unported]).eax == 7
    assert bytes(blk[0x10:0x12]) == b"\x05\x06"
    blk[0:8] = b"\x11" * 8
    assert st.call("audio_core.dispatch", [BASE + 0x8840]).eax == 9    # pairClear claimed: the port body did not run
    assert bytes(blk[0:8]) == b"\x11" * 8
    rep = _report(st)
    assert rep["fallback"] == "1" and rep["claimed"] == "1" and rep["fallback_rvas"] == str(unported)
    assert rep["script_pos"] == rep["script_len"] == "2"


def test_script_mismatch_stops_the_call(img_state, sndlib):
    st, _ = img_state
    unported = _unported(sndlib)
    st.files_put("$audio/script", _script([_entry(2, BASE + unported, 2, [0x30000004])]))
    st.call("audio_core.setup", [BASE, R.SETUP_SCRIPT | R.SETUP_RESET])
    _args(st, 2, [0x30000000])
    with pytest.raises(L.SgError) as e:
        st.call("audio_core.dispatch", [BASE + unported])
    assert e.value.code == L.SG_E_EXCEPTION
    assert "script entry 0" in _report(st)["detail"]


def test_no_host_is_a_missing_resource(img_state, sndlib):
    st, _ = img_state
    st.call("audio_core.noHost", [])
    _args(st, 2, [0x30000000])
    with pytest.raises(L.SgError) as e:
        st.call("audio_core.dispatch", [BASE + _unported(sndlib)])
    assert e.value.code == L.SG_E_NO_RESOURCE


def test_callback_host_live(img_state, sndlib):
    """The C ABI's host callback (sg_snd_set_host) receives the request and its reply is what the port sees."""
    st, _ = img_state
    seen = []

    def host(user, pc):
        c = pc.contents
        seen.append((c.kind, c.va, c.abi, [c.args[i] for i in range(c.nargs)]))
        c.eax = 0x1234
        return 0
    cb = R.HOSTFN(host)
    assert sndlib.dll.sg_snd_set_host(st.ctx, cb, None) == 0
    st.call("audio_core.useCallback", [])
    _args(st, 0, [3, 4])
    assert st.call("audio_core.dispatch", [0x7A001230]).eax == 0x1234
    assert seen == [(0, 0x7A001230, 0, [3, 4])]
    sndlib.dll.sg_snd_set_host(st.ctx, R.HOSTFN(0), None)


def test_sink_receives_imports_and_com(img_state):
    """With no host, an IAT call reaches the AudioSink by the import's name (the image's import directory), and a method of
    a sink interface object reaches com()."""
    st, blk = img_state
    st.call("audio_core.useSink", [0])
    _args(st, 1, [])
    st.call("audio_core.import", [0xb8654])                # flushSoundList's timeGetTime slot (c3fl kIatTimeGetTime)
    obj = st.call("audio_core.sinkObject", [3, 8]).eax
    # a list {head +0xc, cursor +0x14} with one node {+8 = obj}: ListValues_callSlot18 calls slot +0x18 of obj
    struct.pack_into("<I", blk, 0x0c, 0x30000100)
    struct.pack_into("<III", blk, 0x100, 0, 0, obj)
    st.call("snd.ListValues_callSlot18", [0x30000000])
    st.call("audio_core.sinkLog", [])
    lines = st.files_read("$audio/sink").decode().splitlines()
    assert lines[0].lower() == "import winmm.dll!timegettime"
    assert lines[1] == f"com 3.6 {obj}"


# ================================================================================================ S4.2b: the x87 unit, the stack, stubs
def x87_image(cw: int, values=(), cc: int = 0) -> bytes:
    """An x87 unit image ("SX87", audio.h) with `values` pushed (values[0] = st(0)), each an (m, se) pair."""
    n = len(values)
    top = (8 - n) & 7
    regs = [(0, 0)] * 8
    tag = 0xFFFF
    for i, v in enumerate(values):
        p = (top + i) & 7
        regs[p] = v
        tag &= ~(3 << (2 * p))
    sw = (top << 11) | cc
    return b"SX87" + struct.pack("<HHHH", cw, sw, tag, 0) + b"".join(struct.pack("<QH", m, e) for m, e in regs)


def _x80_of(d: float):
    """A double as the 80-bit value fld qword gives (normal values only)."""
    b = struct.unpack("<Q", struct.pack("<d", d))[0]
    s, ex, f = b >> 63, (b >> 52) & 0x7FF, b & ((1 << 52) - 1)
    return ((1 << 63) | (f << 11), (s << 15) | (ex - 1023 + 16383))


def test_ftol_pops_the_unit_and_chops(img_state, sndlib):
    """sound.dll's __ftol (rva 0x42550, audio_core.crt.ftol) through the dispatcher: st(0) chopped to 64 bits whatever the
    rounding control (-2.75 at round-up 0x0b7f gives -2), popped off the unit; mode 1 (the caller's rounding kept) gives -2
    too at round-up and -3 at round-down 0x077f."""
    st, _ = img_state
    for cw, want, want_neg in ((0x0b7f, -2, -2), (0x077f, -2, -3), (0x027f, -2, -3)):
        st.files_put("$audio/x87", x87_image(cw, [_x80_of(-2.75)]))
        st.call("audio_core.x87load", [])
        _args(st, 0, [])
        r = st.call("audio_core.dispatch", [BASE + 0x42550])
        assert struct.unpack("<i", struct.pack("<I", r.eax))[0] == want and r.edx == 0xFFFFFFFF
        st.call("audio_core.x87save", [])
        img = st.files_read("$audio/x87out")
        assert R.x87_view(img)["regs"] == {} and R.x87_view(img)["top"] == 0
        st.files_put("$audio/x87", x87_image(cw, [_x80_of(-2.75)]))
        st.call("audio_core.x87load", [])

        sndlib.negctl("audio_core.crt.ftol", 1)
        try:
            r = st.call("audio_core.dispatch", [BASE + 0x42550])
        finally:
            sndlib.negctl("audio_core.crt.ftol", 0)
        assert struct.unpack("<i", struct.pack("<I", r.eax))[0] == want_neg, hex(cw)


def test_host_sees_the_unit_through_the_box(img_state, sndlib):
    """With the box page kSndFpuBox mapped, the unit's image is there when a host call starts, and what the host leaves
    there is the unit afterwards (a host-run x87 callee computes on the port's FPU)."""
    st, _ = img_state
    box = st.map(R.FPU_BOX, bytearray(0x1000))
    st.files_put("$audio/x87", x87_image(0x0f7f, [_x80_of(1.5)], cc=0x4000))
    st.call("audio_core.x87load", [])
    seen = []

    def host(user, pc):
        img = bytes(box[:R.X87_IMAGE])
        seen.append(R.x87_view(img))
        box[:R.X87_IMAGE] = x87_image(0x027f, [])            # the callee popped and reloaded the default word
        pc.contents.eax = 5
        return 0
    cb = R.HOSTFN(host)
    assert sndlib.dll.sg_snd_set_host(st.ctx, cb, None) == 0
    st.call("audio_core.useCallback", [])
    _args(st, 0, [])
    assert st.call("audio_core.dispatch", [0x7A001240]).eax == 5
    sndlib.dll.sg_snd_set_host(st.ctx, R.HOSTFN(0), None)
    assert seen and seen[0]["cw"] == 0x0f7f and seen[0]["cc"] == 0x4000 and list(seen[0]["regs"].values()) == [_x80_of(1.5)]
    st.call("audio_core.x87save", [])
    v = R.x87_view(st.files_read("$audio/x87out"))
    assert v["cw"] == 0x027f and v["regs"] == {}


def test_import_stub_reaches_the_sink(img_state):
    """U-1472: an unclaimed `jmp [iat]` stub inside the image (rva 0x3a978, DirectSoundEnumerateA's) is the import itself:
    with no host the AudioSink receives it by name, not a fallback."""
    st, _ = img_state
    import pefile
    pe = pefile.PE(str(SOUND_DLL))
    ib = pe.OPTIONAL_HEADER.ImageBase
    raw = pe.get_memory_mapped_image()
    assert raw[0x3a978:0x3a97a] == b"\xff\x25"
    slot = struct.unpack("<I", raw[0x3a97a:0x3a97e])[0] - ib
    name = next(f"{d.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}" for d in pe.DIRECTORY_ENTRY_IMPORT
                for i in d.imports if i.address - ib == slot)
    pe.close()
    st.call("audio_core.useSink", [0])
    st.call("audio_core.reset", [])
    _args(st, 1, [0x30000000, 7])
    st.call("audio_core.dispatch", [BASE + 0x3a978])
    st.call("audio_core.sinkLog", [])
    assert st.files_read("$audio/sink").decode().splitlines() == [f"import {name} {0x30000000} 7"]
    assert _report(st)["fallback"] == "0"


# ================================================================================================ the live oracle (opt-in)
@pytest.fixture(scope="module")
def vm():
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    from port.test.sound_replay import E
    with E.FixVM() as v:
        yield v


@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(vm, sndlib, key):
    v, _, _ = R.oracle_key(vm, sndlib, key)
    assert v.verdict == "GREEN" and v.errors == 0, v
    g = R.load_golden("S4.2").get(key)
    if g:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_oracle_negctl_red(vm, sndlib, fn):
    sndlib.negctl(fn, 1)
    try:
        bad = 0
        for k in _functions()[fn]:
            v, _, _ = R.oracle_key(vm, sndlib, k, ("A",), record=False)
            bad += v.vectors - v.match
    finally:
        sndlib.negctl(fn, 0)
    assert bad > 0
