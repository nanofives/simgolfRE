"""The sound.dll port judged against the emulated original, key by key (step S4.2; decision G1; one front end for every S4
step since S4.2b: log/roadmap/S4/2b_notes.md).

Two modes:

  ORACLE (SG_EMU_ORACLE=1, or `oracle_key`): S4.1's fixture VM rebuilds the key's fixture, the ORIGINAL function runs in
    Unicorn over the user's sound.dll (port/test/sound_emu.py), and the port runs as KeyRun's second arm on the same memory:
    every page the VM / emulator holds (the top 32 KB of the emulator's stack included) is mapped into the port context
    (sg_map), the port is called by name (sg_call), and its writes go back before the regions are compared. Per vector the
    port enters with
      * the ENTRY ESP the original's arm entered with (its by-address locals go to the original's addresses, audio.h "the
        stack"), and
      * the x87 unit loaded with the FPU the original arm FOUND (audio.h "the x87 unit"); the unit the port leaves is compared
        with the FPU the original LEFT (control word, TOP, C0..C3, registers in use and their 80 bits; for a float / double
        return after the caller's fstp).
    The port's calls outside itself reach a live host (the C ABI's sg_snd_set_host): the emulator runs them on the port's
    memory and on the port's x87 unit (the box page kSndFpuBox carries the unit's image in and out of every host call): a
    NativeCallback, a fake DLL export, an Interceptor redirect the fixture installed, or, in development, a sound.dll function
    no port function is registered for: a FALLBACK, counted (GREEN_PENDING when every fallback reaches another step's row).
    Each layout A / B / C runs; per vector the port must equal the emulated original on the return text, the bytes of every
    state region and the x87 exit state; the emulated original is tied to the path-1 CSV on every address-independent field
    by S4.1's census comparison (re/tools/sound_census.py compare()). The run records a BUNDLE per key (layout A): the port's
    starting memory per vector, its arguments, entry esp and x87 image, the claims, and the host calls it made with the
    replies and the memory the host wrote, plus the emulated original's results as golden digests.

  DEFAULT (no emulator, no Node): the bundle is replayed into a fresh context with the recorded host calls served by the
    library's own script host ("$audio/script", audio_core.useScript); the port's return text and regions must hash to the
    golden digest pinned from the emulator (port/test/golden_audio_<module>.json) and its x87 exit state to the pinned exit
    digest. This is what the Win32 backend runs (the runner needs no new op: the script travels as a context file).

    ~/bin/py -3.12 -m port.test.sound_replay oracle  --step S4.k [--family f] [--keys k ...] [--update] [--layouts ABC]
                                                     [--coverage <tsv>] [--out <tsv>]
    ~/bin/py -3.12 -m port.test.sound_replay replay  --step S4.k [--family f] [--keys ...] [--w32 [--soft]] [--negctl fn:mode]
    ~/bin/py -3.12 -m port.test.sound_replay negctl  --step S4.k [--mode 1] [--functions f ...] [--out <tsv>]
    ~/bin/py -3.12 -m port.test.sound_replay sinklog --step S4.8 [--family ds|dev]
    ~/bin/py -3.12 -m port.test.sound_replay keys    --step S4.k [--family f]

Per-step tables (STEPS, DERIVED, CUSTOM, EXCLUDED, ZERO_STACK, PATCHES, QUIRK_FUNCTIONS, NEGCTL_MODES, NEGCTL_EXTRA) hold what
6_run.py (S4.6), 8_replay.py (S4.8) and test_audio_mixer.py's harness (S4.4) added on top of S4.2's version; a new step adds
its rows there. Fixtures of keys no C3 batch defines are port/test/sound_fixtures/*.js (evaluated into every key's VM context
after the scripts load).
Bundles: port/corpus/audio/<key>.json.z (local, gitignored; $SG_SOUND_BUNDLES overrides). Goldens: port/test/golden_*.json
(committed; $SG_SOUND_GOLDENS overrides the directory).
"""
from __future__ import annotations

import argparse
import base64
import collections
import csv
import ctypes as C
import dataclasses
import hashlib
import json
import os
import pathlib
import struct
import sys
import zlib


def _root() -> pathlib.Path:
    for p in pathlib.Path(__file__).resolve().parents:
        if (p / "re" / "tools").is_dir() and (p / "port" / "test" / "sound_emu.py").exists():
            return p
    raise RuntimeError("SimGolf root not found")


ROOT = _root()
HERE = pathlib.Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "test"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import sound_emu as E  # noqa: E402
from port.test import lib as L  # noqa: E402

PAGE = 0x1000
SETUP_CLAIMS, SETUP_SCRIPT, SETUP_CALLBACK, SETUP_RESET = 1, 2, 4, 8     # audio_core.setup flags
BUNDLE_DIR = pathlib.Path(os.environ["SG_SOUND_BUNDLES"]) if os.environ.get("SG_SOUND_BUNDLES") else ROOT / "port" / "corpus" / "audio"
GOLDEN_DIR = pathlib.Path(os.environ["SG_SOUND_GOLDENS"]) if os.environ.get("SG_SOUND_GOLDENS") else ROOT / "port" / "test"
FIXTURE_DIR = ROOT / "port" / "test" / "sound_fixtures"
FUNCTIONS = ROOT / "re" / "roadmap" / "S4_functions.tsv"
MIRROR = pathlib.Path(os.environ["SG_S4_MIRROR"]) if os.environ.get("SG_S4_MIRROR") else ROOT / "port" / "build" / "S4_2b_mirror"
ABI = {"default": 0, "cdecl": 0, "stdcall": 1, "thiscall": 2, "fastcall": 3, "mscdecl": 0}
ABI_NAME = {0: "default", 1: "stdcall", 2: "thiscall", 3: "fastcall"}
KIND = {0: "external", 1: "claimed", 2: "fallback"}
TIERS = ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY", "A_ONLY")
FPU_BOX = 0xFEAF0000            # portsim/sim/audio.h kSndFpuBox: the host's view of the x87 unit
HOST_GAP = 0x2000               # host calls of the port run with esp this far below the entry esp (native_call adds 0x400)
FRAME_LIMIT = 0x1C00            # port frames stay above entry esp - FRAME_LIMIT (the codec chain needs 0x7a4: U-1420)
DEFAULT_CW = 0x027F

# ---- steps ------------------------------------------------------------------------------------------------------------
# golden file per step (and per family where a step has two modules)
STEPS = {
    "S4.2": {"golden": "golden_audio_core.json"},
    "S4.3": {"golden": "golden_audio_codec.json"},
    "S4.4": {"golden": "golden_audio_mixer.json"},
    "S4.5": {"golden": "golden_audio_seq.json"},
    "S4.6": {"golden": "golden_audio_chan.json"},
    "S4.7": {"golden": "golden_audio_sound.json"},
    "S4.8": {"golden": {"ds": "golden_audio_ds.json", "dev": "golden_audio_dev.json"}},
    "S4.9": {"golden": "golden_audio_wave.json"},
}

# Emu-only rows (re/roadmap/S4_functions.tsv oracle `emu-only`): no registry key, so a key of an instruction-identical twin
# (or of the same argument slots) lends its fixture and vectors; the function's own address replaces the twin's. No CSV.
DERIVED: dict[str, tuple[str, int]] = {
    # S4.2
    "PtrList_pushFront@": ("NodeList_pushFront", 0x1d1d0),      # 25 instructions identical to NodeList_pushFront 0x24f90
    "PtrList_insert@": ("NodeList_insert", 0x1d250),            # 112 identical to NodeList_insert 0x24e60
    "PtrList_insert@oom": ("NodeList_insert_oom", 0x1d250),
    "stubReturnBB@": ("setField6cReturn0", 0xe570),              # (this, uint32): the same slots
    # S4.4: the six MMX renderers take the rated mixers' arguments (ppOut, ppIn, count, voice, mode) and read the same voice
    # fields, so each borrows the fixture of the rated mixer of its sample format and add / store kind (4_notes.md)
    "mixRenderRatedC@": ("mixRenderOuterA2", 0x25190),          # stereo 16, add  <- mixRatedStereo16Add
    "Voice_renderStream32@": ("mixResampleG", 0x26800),         # stereo 16, store <- mixRatedStereo16Set
    "Voice_renderStream8c@": ("mixResampleE", 0x25c80),         # mono 8, add      <- mixRatedMono8Add
    "Voice_renderStream8d@": ("mixResampleI", 0x26d30),         # mono 8, store    <- mixRatedMono8Set
    "Voice_renderStream16b@": ("mixRenderRatedB", 0x256c0),     # mono 16, add, fade  <- mixRatedMono16AddFade
    "Voice_renderStream16c@": ("mixRenderOuterC", 0x26260),     # mono 16, store, fade <- mixRatedMono16SetFade
    "mixRenderRatedC@g": ("mixResampleG", 0x25190),             # the other fixture of each stereo pair
    "Voice_renderStream32@a": ("mixRenderOuterA2", 0x26800),
    # S4.4 CHAINED keys: a renderer's fixture with its mixer stubs reverted (PATCHES)
    "renderChannelSingle@mix": ("renderChannelSingle_post", 0x36280),
    "renderChannelVoice@mix": ("renderChannelVoice_eng", 0x371d0),
    # S4.6
    "stubReturnBC@": ("setField6cReturn0", 0x11160),            # (this, uint32): `mov eax, 0xb; ret 4`
    "Channel_stubReturnB@": ("Channel_set3DPosition", 0x35f60),  # (this, x, y, z): `mov eax, 0xb; ret 0xc`
    "VoiceSlot_dtorEmpty@": ("ChanRecord_freeBuffers", 0x37df0),  # (this), a bare `ret`
    # S4.8
    "return0Pop4@": ("setField6cReturn0", 0x12040),             # `xor eax, eax; ret 4`
    "stubReturn2fB@": ("setField6cReturn0", 0xa100),            # `mov eax, 0x2f; ret 4`
    "stubReturn2fA@": ("release_sound_null", 0xa0c0),           # `mov eax, 0x2f; ret` (no argument)
    "get_sound_version@": ("release_sound_null", 0x9370),       # `mov eax, 0x5000c01; ret`
}
# the MMX renderers' original arm runs on a zeroed emulator stack: with a count of 1 the pass reads two frame slots no
# instruction wrote (U-1433)
ZERO_STACK = {"mixRenderRatedC@", "Voice_renderStream32@", "Voice_renderStream8c@", "Voice_renderStream8d@",
              "Voice_renderStream16b@", "Voice_renderStream16c@", "mixRenderRatedC@g", "Voice_renderStream32@a"}

# Keys whose fixture is defined in port/test/sound_fixtures/*.js: registry-shaped specs.
CUSTOM: dict[str, dict] = {}
# S4.6: Voice::update (no C3 key): 34 voice rows, 9 registry-list variants (sound_fixtures/s46_voice_update.js)
VU_ROWS = 34
_VU_SYNTH = {"Voice_update_a": 2 * VU_ROWS + 1, "Voice_update_b": VU_ROWS + 1, "Voice_update_c": 3, "Voice_update_d": 2,
             "Voice_update_e": VU_ROWS + 1, "Voice_update_f": VU_ROWS + 1, "Voice_update_g": 2, "Voice_update_h": 2,
             "Voice_update_z": 1}                                  # key -> registry list size + 1
_VU_ARGS = {5: 1, 6: 3, 0: 100, 1: 7, 3: 1, 4: 1, 13: 2, 14: 2, 15: 2, 16: 2, 17: 2, 18: 9, 19: 9, 21: 5, 22: 5, 23: 5,
            25: 0xFFFFFFFF, 26: 5, 27: 2, 28: 3, 32: 1, 33: 1}
for _k, _tn in _VU_SYNTH.items():
    CUSTOM[_k] = dict(module="sound.dll", addr=0x1F8C0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                      fixture="s46_vu_" + _k[-1],
                      state=[(f"$o{i}", 0, 0x800) for i in range(VU_ROWS)]
                      + [("$nd", 0, 0x60 * 8 * VU_ROWS), ("$ix", 0, 0x20 * VU_ROWS), ("$tn", 0, 0x10 * _tn), ("$gth", 0, 4),
                         ("$gtt", 0, 4), ("$gtc", 0, 4), ("$gtn", 0, 4), ("$arena", 0, 0x110), ("$freelog", 0, 0x404),
                         ("$log", 0, 0x820), ("$g49a8", 0, 4)],
                      vectors=[(f"$o{i}", _VU_ARGS.get(i, 0)) for i in range(VU_ROWS)])
# S4.8 (sound_fixtures/s48_custom.js)
CUSTOM["DirectSoundDevice::setMasterVolume@"] = dict(
    module="sound.dll", addr=0x13560, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="s48_mastervol",
    state=[("$dev", 0, 0x200)] + [(f"$b{i}", 0, 0xd4) for i in range(4)] + [(f"$c{i}", 0, 0x40) for i in range(4)]
    + [("$gl", 0, 0xc)],
    vectors=[("$dev", v) for v in (0, 1, 0x40, 0x7f, 0x80, 0xffffffff, 0x3f, 0x7e, 2, 0x12345)])
# the empty list: eax is whatever the caller left (the replay's entry eax), so the key compares state only (ret void)
CUSTOM["DirectSoundDevice::setMasterVolume@empty"] = dict(CUSTOM["DirectSoundDevice::setMasterVolume@"], ret="void",
                                                          fixture="s48_mastervol_empty")
# postMidiNotification: the registry key's c3dh_postmidi waits for the live game's sound engine (S4.1 census FIXTURE_ERR);
# these keys use its objects, vectors and regions with g_soundEngine set by the fixture
for _v in ("eng", "eng0", "noeng", "noeng1"):
    CUSTOM[f"postMidiNotification@{_v}"] = dict(
        module="sound.dll", addr=0x20b60, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"s48_postmidi_{_v}",
        state=[(f"$m{i}", 0, 0x220) for i in range(8)] + [("$implog", 0, 0x400), ("$tstate", 0, 0x30)],
        vectors=[(f"$m{i}",) for i in range(8)])
# delete_sound (rva 0x97d0): no registry key and no twin; three global configurations over nine sound objects
_DS_STATE = ([("$log", 0, 0x800), ("$arena", 0, 0x210), ("$tmr", 0, 0x60), ("$eng", 0, 0x200), ("$midi", 0, 0x4060)]
             + [(f"$s{i}", 0, 0x960) for i in range(9)] + [("$sub4", 0, 0x200), ("$sub5", 0, 0x200), ("$olast", 0, 0x220)]
             + [(f"$o{k}", 0, 0x220) for k in range(0, 0x80, 18)])
for _v in ("ready", "notimer", "none"):
    CUSTOM[f"delete_sound@{_v}"] = dict(module="sound.dll", addr=0x97d0, abi="default", ret="uint32", args=["pointer"],
                                        fixture=f"s48_delsound_{_v}", state=_DS_STATE,
                                        vectors=[(0,)] + [(f"$s{i}",) for i in range(9)])

# registry keys the gate cannot run, with the keys that replace them
EXCLUDED = {
    "postMidiNotification": "S4.1 census FIXTURE_ERR: c3dh_postmidi waits for the live game's sound engine (flag bit 5); "
                            "replaced by postMidiNotification@eng / @eng0 / @noeng / @noeng1",
}

# The emulator's signaling-NaN quirk (U-1451): Unicorn's fld / fst of a signaling NaN keeps it signaling; the CPU (and the
# live C3 A/B: log/c3/c3fe_notes.txt, the control that copies the bits raw reads RED) sets the quiet bit. The port follows the
# CPU. For the functions whose original copies doubles or floats through the x87, a difference that is exactly the quiet bit
# of a NaN pattern (port set, emulator clear) is forgiven; every other difference still fails. The golden digest of a forgiven
# vector pins the port's regions.
QUIRK_FUNCTIONS = ("updateVoiceRate", "Channel_startVoice", "serviceChannelPlayback", "Channel_allocAndInitVoice")

# negative-control modes beyond 1, by function (each body documents its modes)
NEGCTL_MODES = {
    **{f: (2,) for f in ("Voice_mixAddMono16", "Voice_mixSetMono16", "Voice_mixSet8bit", "Voice_mixAdd8bit",
                         "Voice_mixSetStereo16", "Voice_mixAddStereo16", "streamRenderBlock", "streamRenderTail")},
    **{f: (2,) for f in ("Channel_setRate", "Channel_recomputeVoiceGains", "Channel_setReverseRate", "Channel_stepVolumeFade",
                         "Voice_resetDefaults")},
    "Voice::update": (2, 3, 4, 5, 6, 7, 8),
    "DsBuffer::setVolume": (2, 3),
    # S4.3: one `fstp dword` spill kept as a double (3 functions; the drivers' keys judge them through --callee, U-1425)
    **{f: (2,) for f in ("codecHighPassFilter", "lpcSynthesize", "codecRemoveMean")},
    "Seq_setTempo": (2,),                      # S4.5: plain doubles (RED at 0x0f7f)
    "Sound5b6a8_ctor": (2,),                   # S4.7: plain doubles (BLIND at acceptance, U-1461)
    # S4.9: import-level controls judged by sinklog (wrong mmioClose flags; waveInReset skipped)
    "closeMmioHandle": (2,), "WaveInDevice_reset": (2,),
    "loadSoundFile": tuple(range(2, 11)),      # S4.9b: modes 2..10 (audio_soundfile.cpp kNeg*)
}
# functions with no key of their own, judged through the keys of their callers: step -> {function: key prefixes}
NEGCTL_EXTRA = {"S4.2": {"audio_core.crt.ftol": ("renderChannelStreamed_fade", "renderChannelVoice")}}


# ---- S4.9: emu-only rows and keys the VM cannot run (log/roadmap/S4/emuonly9.py until S4.2b; fixtures sound_fixtures/s49_wave.js)
DERIVED.update({
    "stubReturnBA@": ("setField6cReturn0", 0xe350),           # `mov eax, 0xb; ret 4` (rva 0xe350): (this, uint32)
    "return0xb_32660@": ("release_sound_null", 0x32660),      # `mov eax, 0xb; ret` (rva 0x32660): no argument
})
_WAVECB_VEC = [(1, m, i, p, 9) for m in (0x3C0, 0x3BF, 0x3C1, 0x123C0, 0xFFFF03C0, 0, 0xFFFFFFFF, 0x13C0)
               for i, p in ((0x1234, 0x5678), (0, 0xFFFFFFFF))]
_OUTVOL_STATE = ([("$dev", 0, 0x200)] + [(f"$b{i}", 0, 0xd4) for i in range(4)] + [(f"$c{i}", 0, 0x40) for i in range(4)]
                 + [("$gl", 0, 0xc)])
_OUTVOL_VEC = [(v,) for v in (0, 1, 0x40, 0x7f, 0x80, 0xffffffff, 0x3f, 0x7e, 2, 0x12345)]
for _v in ("eng", "eng0", "noeng", "noeng1"):
    CUSTOM[f"onWaveInCallbackMessage@{_v}"] = dict(
        module="sound.dll", addr=0x38fd0, abi="stdcall", ret="void", args=["uint32"] * 5, fixture=f"s49_wavecb_{_v}",
        state=[("$implog", 0, 0x400), ("$tstate", 0, 0x30)], vectors=list(_WAVECB_VEC))
CUSTOM["setOutputMasterVolume@dev"] = dict(module="sound.dll", addr=0xe2a0, abi="stdcall", ret="uint32", args=["uint32"],
                                           fixture="s49_outvol_dev", state=_OUTVOL_STATE, vectors=list(_OUTVOL_VEC))
# the NULL device: eax is whatever the caller left (the replay's entry eax), so the key compares state only (ret void)
CUSTOM["setOutputMasterVolume@null"] = dict(module="sound.dll", addr=0xe2a0, abi="stdcall", ret="void", args=["uint32"],
                                            fixture="s49_outvol_null", state=_OUTVOL_STATE, vectors=list(_OUTVOL_VEC))
EXCLUDED["onWaveInCallbackMessage"] = ("S4.1 census FIXTURE_ERR: c3dh_wavecb waits for the live game's sound engine (flag bit 5); "
                                       "replaced by onWaveInCallbackMessage@eng / @eng0 / @noeng / @noeng1")


def _registry_fragment(name):
    ns = {}
    exec(compile((ROOT / "re" / "frida" / "registry.d" / f"{name}.py").read_text(encoding="utf-8"), name, "exec"), ns)
    return ns["HOOKS"]


_C3FL = _registry_fragment("c3fl")
# writeWaveFileB (this, buf, buf2): ten vectors over the six buffers of c3fl_wf
_WFB_VEC = [("$w0", 0, "$b1"), ("$w0", "$b1", 0), ("$w0", "$b0", "$b1"), ("$w1", "$b3", "$b2"), ("$w2", "$b1", "$b4"),
            ("$w0", "$b2", "$b3"), ("$w1", "$b4", "$b5"), ("$w2", "$b5", "$b1"), ("$w0", "$b1", "$b2"), ("$w1", "$b2", "$b5")]
for _k in ("noeng", "empty", "nullobj", "k6", "k4", "k1", "k1b", "mix"):
    CUSTOM[f"writeWaveFileB@{_k}"] = dict(_C3FL[f"writeWaveFile_{_k}"], addr=0x75e0, args=["pointer", "pointer", "pointer"],
                                          fixture=f"s49_wfb_{_k}", vectors=list(_WFB_VEC))
_MM = [("$mmblock", 0, 0x8000), ("$implog", 0, 0x400)]
_TAIL = [("$log", 0, 0x400)] + _MM + [("$arena", 0, 0x10 + 0x100), ("$freelog", 0, 0x404), ("$gmmio", 0, 4)]
# writeWaveChunk: c3fn_wwc's writers without the zero block align / zero first byte count rows (2^32-iteration loop: U-1481)
_WWC_ROWS = (0, 1, 2, 4, 5, 6, 7, 9)
CUSTOM["writeWaveChunk@wwc"] = dict(
    module="sound.dll", addr=0x7df0, abi="thiscall", ret="uint32", args=["pointer"] * 5, fixture="c3fn_wwc",
    state=[("$objs", 0, 10 * 0x400), ("$bufs", 0, 10 * 0x300), ("$regs", 0, 20 * 0x200), ("$srcs", 0, 10 * 0x200),
           ("$mixes", 0, 10 * 0x100)] + _TAIL,
    vectors=[(f"$o{i}", f"$a0{i}", f"$a1{i}", 0, 0) for i in _WWC_ROWS])
CUSTOM["writeWaveChunkB@wcb"] = dict(
    module="sound.dll", addr=0x8070, abi="thiscall", ret="uint32", args=["pointer"] * 7, fixture="s49_wcb",
    state=[("$objs", 0, 8 * 0x400), ("$bufsL", 0, 8 * 0x300), ("$bufsR", 0, 8 * 0x300), ("$regs", 0, 8 * 4 * 0x200),
           ("$srcs", 0, 8 * 0x400), ("$mixes", 0, 8 * 0x100)] + _TAIL,
    vectors=[(f"$o{i}", f"$l{i}", f"$r{i}", f"$s{i}", 0x11, 0x22, 0x33) for i in range(8)])
_WD_STATE = ([("$self", 0, 0x1c0), ("$dev", 0, 0x200), ("$stream", 0, 0x40), ("$prim", 0, 0x100), ("$s1", 0, 0x100),
              ("$s2", 0, 0x100), ("$timer", 0, 0x20), ("$gl", 0, 0x88), ("$log", 0, 0x400), ("$arena", 0, 0x10 + 0x400),
              ("$freelog", 0, 0x404)])
for _k in ("nodev_ok", "nodev_enumfail", "nodev_createfail", "nodev_notimer", "dev_cbfail", "dev_nofmt", "dev_fmt1fail",
           "dev_fmt2fail", "dev_fmt_nocom", "dev_prim", "dev_secbufs", "dev_nox60"):
    CUSTOM[f"Wave_Device::open@{_k}"] = dict(module="sound.dll", addr=0xb420, abi="thiscall", ret="uint32",
                                             args=["pointer", "uint32", "uint32"], fixture=f"s49_wdev_{_k}",
                                             state=list(_WD_STATE), vectors=[("$self", 0x77, f) for f in (0, 0x80, 0x11, 0x91)])
for _k in ("noflag", "nodev", "nostream", "primexists", "createfail", "cbfail", "ok", "nofmt", "fmt1fail", "fmt2fail", "nocom",
           "nox60", "x60zero", "prim_ok", "plain"):
    CUSTOM[f"Wave_Device::start@{_k}"] = dict(module="sound.dll", addr=0xbaa0, abi="thiscall", ret="uint32", args=["pointer"],
                                              fixture=f"s49_wdevs_{_k}", state=list(_WD_STATE), vectors=[("$self",)])
for _k in ("post", "thread"):
    CUSTOM[f"Wave_Device::dispatchCommand@gen_{_k}"] = dict(
        module="sound.dll", addr=0xcd80, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"s49_wdgen_{_k}",
        state=[("$arena", 0, 0x500000), ("$heaparena", 0, 0x2010), ("$log", 0, 0x1000), ("$freelog", 0, 0x404),
               ("$implog", 0, 0x400), ("$geng", 0, 4)],
        vectors=[(f"$dev{i}",) for i in range(27)])
for _k, _n in (("x1", 6), ("x2", 8)):
    CUSTOM[f"fillDecodeRing@{_k}"] = dict(
        module="sound.dll", addr=0x33300, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"s49_fdr_{_k}",
        state=[("$objs", 0, _n * 0x1400), ("$rings", 0, _n * 0x100), ("$rbufs", 0, _n * 0x200), ("$gmmio", 0, 4),
               ("$mmblock", 0, 0x8000), ("$implog", 0, 0x400)],
        vectors=[(f"$o{i}",) for i in range(_n)])

# ---- S4.9b: loadSoundFile (rva 0x41d0; its row is S4.9's, its module audio_soundfile and its golden its own) -------------------
# keys over sound_fixtures/s49b_soundfile.js (log/roadmap/S4/9b_keys.py until S4.2b); the `user` key loads the user's own files,
# which the harness hands to the fixture as `globalThis.__s49bUser` (USER_FILES, read from original\)
SOUND_SIZE_9B, ARENA_9B, MMSIZE_9B = 0xB00, 0x40000, 0x20000


def _state_9b(n, nlist):
    s = [("$objs", 0, n * SOUND_SIZE_9B), ("$arena", 0, 0x10 + ARENA_9B), ("$freelog", 0, 0x404), ("$mmblock", 0, MMSIZE_9B),
         ("$implog", 0, 0x4000), ("$log", 0, 0x400), ("$glist", 0, 0x10), ("$gmmio", 0, 4), ("$lnodes", 0, 0x400),
         ("$gstr", 0, 0x40), ("$rnodes", 0, 0x40 * n * 8), ("$recs", 0, 0x20 * n * 8)]
    return s + [(f"$lent{i}", 0, 0x220) for i in range(nlist)]


for _k, _n, _l in (("basic", 11, 1), ("busy7", 2, 0), ("busy0", 2, 0), ("seekfail", 4, 0), ("seeknomove", 3, 0), ("nopre", 2, 0),
                   ("list", 2, 3), ("pre_g0", 5, 0), ("pre_g1", 5, 0), ("pre_g2", 5, 0), ("pre_g3", 5, 0), ("pre_g4", 5, 0),
                   ("pre_g5", 5, 0), ("oom0", 2, 0), ("oom1", 1, 0), ("oom2", 1, 0), ("nullA", 4, 0), ("nullB", 1, 0),
                   ("nullC", 1, 0), ("nullU", 1, 0), ("sfnull", 1, 0), ("sfnullU", 1, 0), ("snmnull", 1, 0), ("snmnullU", 1, 0),
                   ("user", 3, 0)):
    CUSTOM[f"loadSoundFile@{_k}"] = dict(module="sound.dll", addr=0x41d0, abi="thiscall", ret="uint32",
                                         args=["pointer", "pointer"], fixture=f"s49b_{_k}", state=_state_9b(_n, _l),
                                         vectors=[(f"$s{i}", f"$n{i}") for i in range(_n)])
USER_FILES_9B = ("Sounds/Interface/Text Box.wav", "Sounds/vssver.scc", "Sounds/Effects/Sheep 2.wav")
# a function whose step is not its row's (the row of loadSoundFile is S4.9's; S4.9b ported it as its own module)
STEP_OF_FUNCTION = {"loadSoundFile": "S4.9b"}
STEPS["S4.9b"] = {"golden": "golden_audio_soundfile.json"}
QUIRK_FUNCTIONS = QUIRK_FUNCTIONS + ("closeMmioSound",)
# the steps whose functions are ported: a fallback into one of their rows means a missing registration (RED)
PORTED_STEPS = ("S4.2", "S4.3", "S4.4", "S4.5", "S4.6", "S4.7", "S4.8", "S4.9", "S4.9b")

# ---- S4.7: getPlaybackTime's vectors reach timeGetTime, which S4.1's fake WINMM lacks (U-1462): those keys run on a copy of the
# VM whose WINMM answers timeGetTime with a constant (the C3 fixture picks anchor and limit so the result does not depend on it;
# re/frida/registry.d/c3cl.py header). Both arms see the same constant.
CLOCK_FUNCTIONS = ("getPlaybackTime",)
CLOCK_VALUE = 0x1234
_CLOCK_VM: list = []


def _clock_fixvm():
    import shutil
    src = ROOT / "re" / "tools" / "fixvm"
    dst = MIRROR / "fixvm_clock"
    dst.mkdir(parents=True, exist_ok=True)
    for f in ("fixvm.js", "frida.js", "mem.js", "syslibs.js"):
        shutil.copy2(src / f, dst / f)
    s = (dst / "syslibs.js").read_text(encoding="utf-8")
    old_ = "'winmm.dll': {},"
    assert s.count(old_) == 1
    (dst / "syslibs.js").write_text(s.replace(old_, "'winmm.dll': { timeGetTime: [0, () => %d] }," % CLOCK_VALUE),
                                    encoding="utf-8")
    keep = E.FIXVM_JS
    E.FIXVM_JS = dst / "fixvm.js"
    try:
        return E.FixVM()
    finally:
        E.FIXVM_JS = keep


def vm_for(vm, key):
    """the shared VM, or the clock VM for the keys of CLOCK_FUNCTIONS"""
    if row_name(key) not in CLOCK_FUNCTIONS:
        return vm
    if not _CLOCK_VM:
        _CLOCK_VM.append(_clock_fixvm())
    return _CLOCK_VM[0]


def close_clock_vm():
    for v_ in _CLOCK_VM:
        v_.close()
    _CLOCK_VM.clear()


import atexit  # noqa: E402

atexit.register(close_clock_vm)


_USER_JS = None


def user_files_js() -> str:
    """`globalThis.__s49bUser = {basename: hex}` of the user's own files (USER_FILES_9B under original/)."""
    global _USER_JS
    if _USER_JS is None:
        d = {}
        for rel in USER_FILES_9B:
            p = ROOT / "original" / rel
            if p.exists():
                d[p.name] = p.read_bytes().hex()
        _USER_JS = "globalThis.__s49bUser = " + json.dumps(d) + ";"
    return _USER_JS


# ---- the S4.4 chained keys -------------------------------------------------------------------------------------------
# The renderers' fixtures stand in their mixers (recording stubs), so the oracle never sees a renderer compute on top of the
# registers a real mixer leaves (renderChannelSingle's fade after Voice_mix*, renderChannelSingle+0x169). These keys revert
# the mixer stubs in the fixture VM (`Interceptor.revert`) and give every channel / voice a real sample record over SAMPLES,
# a fixture-owned region of the emulator (mapped and filled here, outside every compared region).
SAMPLES_VA, SAMPLES_SIZE = 0x6D000000, 0x10000


def _samples(m):
    if (SAMPLES_VA >> 12) not in m.mapped:
        m._map(SAMPLES_VA, SAMPLES_SIZE)
    words = bytearray()
    x = 0x1234567
    for i in range(SAMPLES_SIZE // 2):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        w = {7: 0x7FFF, 8: 0x8000, 9: 0xFFFF, 10: 0}.get(i % 61, (x >> 8) & 0xFFFF)
        words += struct.pack("<H", w)
    m.write(SAMPLES_VA, bytes(words))


def _d(v: float) -> bytes:
    return struct.pack("<d", v)


def _revert(run, rvas):
    src = ";".join(f"Interceptor.revert(ptr({run.img.base + r}))" for r in rvas)
    run.vm.request({"op": "eval", "src": src + ";0"})


def _patch_single_mix(run, fx):
    """renderChannelSingle@mix: the six Voice_mix* thunks run for real; channel i reads samples from SAMPLES + 0x400 * i."""
    m = run.m
    _samples(m)
    _revert(run, [0x10dc, 0x2220, 0x1f64, 0x12ee, 0x267b, 0x2617])
    for i in range(20):
        c, x = fx[f"c{i}"]["p"], fx[f"x{i}"]["p"]
        base = SAMPLES_VA + 0x400 * i
        kind = i % 4
        start, end = base, base + (0x300 if kind != 1 else 0x20)      # kind 1: a short buffer (the loop / end arms)
        cur = base + (0x10 if kind != 1 else 0x16)
        m.write(x + 0x18, struct.pack("<I", start))
        m.write(x + 0x1c, struct.pack("<I", cur))
        m.write(x + 0x20, struct.pack("<I", end))
        m.write(x + 0x24, struct.pack("<I", end + (0x40 if kind == 2 else 0)))
        m.write(c + 0x1f0, _d([0.75, 1.0 / 3, -0.5, 0.123456789][kind]))
        m.write(c + 0x200, _d([0.5, 0.7071067811865476, 2.0, -1e-3][kind]))
        m.write(c + 0x1380, _d([0.0, 0.25, 0.9999999, 1.0 / 7][kind]))
        m.write(c + 0x1388, _d([0.37, 1.0, 1.5, 0.3333333333333333][kind]))
        m.write(c + 0x1378, struct.pack("<I", base + 0x80))


def _patch_voice_mix(run, fx):
    """renderChannelVoice@mix: the two mono Voice_mix* thunks (voice records, mode 0) run for real; voice i reads samples
    from SAMPLES + 0x400 * i; the stereo / 8-bit channels keep their stubs (their mode-1 fields lie past the 0x400 block)."""
    m = run.m
    _samples(m)
    _revert(run, [0x1f64, 0x2617])
    for i in range(20):
        v = fx[f"v{i}"]["p"]
        base = SAMPLES_VA + 0x400 * i
        kind = i % 4
        m.write(v + 0x18, struct.pack("<I", base))
        m.write(v + 0x1c, struct.pack("<I", base + 0x10))
        m.write(v + 0x20, struct.pack("<I", base + (0x300 if kind != 1 else 0x1e)))
        m.write(v + 0xd8, struct.pack("<I", base + 4))
        m.write(v + 0xd4, struct.pack("<I", base + (0x200 if kind != 1 else 0x1c)))
        m.write(v + 0x98, bytes([1 if kind in (1, 3) else 0]))
        m.write(v + 0x80, _d([0.75, 1.0 / 3, -0.5, 0.123456789][kind]))
        m.write(v + 0x70, _d([0.5, 0.7071067811865476, 2.0, -1e-3][kind]))
        m.write(v + 0x100, _d([0.0, 0.25, 0.9999999, 1.0 / 7][kind]))
        m.write(v + 0x108, _d([0.37, 1.0, 1.5, 0.3333333333333333][kind]))


PATCHES = {"renderChannelSingle@mix": _patch_single_mix, "renderChannelVoice@mix": _patch_voice_mix}


class SgSndCall(C.Structure):
    _fields_ = [("va", C.c_uint32), ("rva", C.c_uint32), ("kind", C.c_int32), ("abi", C.c_int32), ("nargs", C.c_int32),
                ("want_st0", C.c_int32), ("args", C.c_uint32 * 16), ("eax", C.c_uint32), ("edx", C.c_uint32),
                ("st0", C.c_double), ("has_st0", C.c_int32), ("status", C.c_int32)]


HOSTFN = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(SgSndCall))


def mirror_dll() -> pathlib.Path:
    """$SG_PORTSIM_DLL, else the S4 mirror build while the modules are unmerged ($SG_S4_MIRROR, default
    port/build/S4_2b_mirror), else the integrator's build."""
    if os.environ.get("SG_PORTSIM_DLL"):
        return pathlib.Path(os.environ["SG_PORTSIM_DLL"])
    m = MIRROR / "x64" / "Release" / "portsim_c.dll"
    return m if m.exists() else L.B.dll_path("x64")


def load_lib():
    lib = L.load(mirror_dll(), rebuild=False)
    if not hasattr(lib, "_snd"):
        f = lib.dll.sg_snd_set_host
        f.restype = C.c_int
        f.argtypes = [C.c_void_p, HOSTFN, C.c_void_p]
        lib._snd = True
    return lib


def load_w32():
    """lib32.Lib32 over the S4 mirror's Win32 runner and DLL (MIRROR/w32), else the integrator's Win32 build."""
    from port.test import lib32 as L32
    exe = MIRROR / "w32" / "Release" / "portsim_runner32.exe"
    dll = MIRROR / "w32" / "Release" / "portsim_c.dll"
    if not exe.exists() or not dll.exists():
        return L32.load32()
    obj = L32.Lib32.__new__(L32.Lib32)
    obj.path = exe
    obj.runner = L32.Runner(exe, dll)
    _, r = obj.runner.tx(L32.OP_HELLO)
    obj.state_size = r.u32()
    obj._info = r.s()
    obj.pointer_size = r.u32()
    obj._objects, obj._funcs, obj.fpcw, obj.dll = {}, None, L32.DEFAULT_CW, None
    return obj


# ---- keys -------------------------------------------------------------------------------------------------------------
_ALL = None
_BATCH = None


def step_rows_all():
    global _ALL
    if _ALL is None:
        with FUNCTIONS.open(newline="") as f:
            _ALL = list(csv.DictReader(f, delimiter="\t"))
    return _ALL


def step_rows(step: str = "S4.2", family: str | None = None) -> list[dict]:
    return [r for r in step_rows_all() if r["step"] == step and (family is None or r["family"].rstrip("~") == family)]


_HOOKS = None


def hooks():
    """The registry dict (re/frida/hooks_registry.py's HOOKS, the one KeyRun reads) with the CUSTOM specs added."""
    global _HOOKS, _BATCH
    if _HOOKS is None:
        _HOOKS, _BATCH = E.registry()
    for k, s in CUSTOM.items():
        _HOOKS.setdefault(k, dict(s))
    return _HOOKS


def batch_of(key):
    hooks()
    return _BATCH.get(key)


def keys_for(step: str = "S4.2", family: str | None = None) -> list[str]:
    """Every registry key of the step's functions (EXCLUDED ones left out), plus the derived and custom keys of its rows."""
    h = hooks()
    moved = {n for n, s in STEP_OF_FUNCTION.items() if s == step}
    if moved:      # a step that owns functions of another step's rows (S4.9b: loadSoundFile)
        rows = [r for r in step_rows_all() if r["name"] in moved]
    else:
        rows = [r for r in step_rows(step, family) if r["name"] not in STEP_OF_FUNCTION]
    rvas = {int(r["rva"], 16) for r in rows}
    names = {r["name"] for r in rows}
    out = sorted(k for k, v in h.items() if (v.get("module") or "").lower() == "sound.dll" and v["addr"] in rvas
                 and k not in EXCLUDED and k not in CUSTOM)
    for d, (twin, rva) in DERIVED.items():
        if rva in rvas and (d.split("@")[0] in names or d.split("@")[0] in out):
            out.append(d)
    for d, spec in CUSTOM.items():
        if spec["addr"] in rvas and d not in out:
            out.append(d)
    return out


def spec_of(key: str) -> dict:
    h = hooks()
    if key in DERIVED:
        twin, rva = DERIVED[key]
        s = dict(h[twin])
        s["addr"] = rva
        return s
    return h[key]


def row_of(key: str) -> dict | None:
    spec = spec_of(key)
    rows = [r for r in step_rows_all() if int(r["rva"], 16) == spec["addr"]]
    return rows[0] if rows else None


def row_name(key: str) -> str:
    """The hooks.csv name of the key's function (re/roadmap/S4_functions.tsv)."""
    r = row_of(key)
    return r["name"] if r else key


SND = "snd."      # every sound.dll function is registered as snd.<hooks.csv name> (log/roadmap/merges/S4-3.md)


def port_name(key: str) -> str:
    """The name the port registers for the key's function: snd.<hooks.csv name>."""
    r = row_of(key)
    return SND + r["name"] if r else key


def bare(name: str) -> str:
    return name[len(SND):] if name.startswith(SND) else name


def step_of(key: str) -> str:
    r = row_of(key)
    return (STEP_OF_FUNCTION.get(r["name"], r["step"])) if r else "-"


def family_of(key: str) -> str:
    r = row_of(key)
    return r["family"].rstrip("~") if r else ""


def golden_path(step: str, family: str | None = None) -> pathlib.Path:
    g = STEPS.get(step, {}).get("golden", f"golden_{step.replace('.', '_')}.json")
    if isinstance(g, dict):
        g = g[family] if family else next(iter(g.values()))
    return GOLDEN_DIR / g


def golden_path_of(key: str) -> pathlib.Path:
    step = step_of(key)
    g = STEPS.get(step, {}).get("golden")
    return golden_path(step, family_of(key) if isinstance(g, dict) else None)


def load_golden(step: str | None = None, family: str | None = None, keys=None) -> dict:
    """The golden entries of a step (and family), or of every file the keys' steps use."""
    paths = {golden_path(step, family)} if step else {golden_path_of(k) for k in keys or []}
    out = {}
    for p in sorted(paths):
        if p.exists():
            out.update(json.loads(p.read_text()))
    return out


def derived(key: str) -> bool:
    return key in DERIVED or key in CUSTOM


def slots_of(args: list[dict]) -> list[int]:
    out = []
    for a in args:
        t, v = a["t"], a["v"]
        if t == "double":
            lo, hi = struct.unpack("<II", struct.pack("<d", float(v)))
            out += [lo, hi]
        elif t == "float":
            out.append(struct.unpack("<I", E._pack_f32(float(v)))[0])
        elif t in E._WIDE:
            q = int(v) & (2 ** 64 - 1)
            out += [q & 0xFFFFFFFF, q >> 32]
        else:
            out.append(int(v) & 0xFFFFFFFF)
    return out


def entry_esp(spec, slots) -> int:
    """The esp the original's arm enters with (Machine.native_call at depth 0: the return address at ((STACK_INIT - 0x400)
    & ~0xF) - stack bytes - 4); register slots: thiscall ecx, fastcall ecx + edx."""
    nreg = {"thiscall": 1, "fastcall": 2}.get(spec.get("abi"), 0)
    return ((E.STACK_INIT - 0x400) & ~0xF) - 4 * max(0, len(slots) - nreg) - 4


# ---- the port's address space -----------------------------------------------------------------------------------------
def runs_of(pages) -> list[tuple[int, int]]:
    """Contiguous page runs as (base, size)."""
    out = []
    for p in sorted(pages):
        if out and out[-1][0] + out[-1][1] == p << 12:
            out[-1] = (out[-1][0], out[-1][1] + PAGE)
        else:
            out.append((p << 12, PAGE))
    return out


class PortSpace:
    """The port context's mappings: one ctypes buffer per contiguous run of pages."""

    def __init__(self, st):
        self.st = st
        self.runs: list[tuple[int, int, object]] = []

    ro: tuple[int, int] | None = None      # [lo, hi) mapped read-only: sound.dll's headers, .text and .rdata

    def map_pages(self, pages: dict[int, bytes]):
        groups = {}
        for p in pages:
            groups.setdefault(bool(self.ro and self.ro[0] <= (p << 12) < self.ro[1]), []).append(p)
        for ro, ps in groups.items():
            for base, size in runs_of(ps):
                buf = (C.c_uint8 * size).from_buffer_copy(b"".join(pages[p] for p in range(base >> 12, (base + size) >> 12)))
                self.st.map(base, buf, readonly=ro)
                self.runs.append((base, size, buf))

    def load(self, pages: dict[int, bytes]):
        self.st.unmap_all()
        self.runs = []
        self.map_pages(pages)

    def pages(self) -> dict[int, bytes]:
        out = {}
        for base, size, buf in self.runs:
            raw = C.string_at(C.addressof(buf), size)
            for k in range(size // PAGE):
                out[(base >> 12) + k] = raw[k * PAGE:(k + 1) * PAGE]
        return out

    def read(self, va: int, n: int) -> bytes:
        for base, size, buf in self.runs:
            if base <= va and va + n <= base + size:
                return C.string_at(C.addressof(buf) + va - base, n)
        raise KeyError(f"0x{va:08x}+{n} not mapped in the port")

    def mapped(self) -> set[int]:
        return {p for base, size, _ in self.runs for p in range(base >> 12, (base + size) >> 12)}


# The top 32 KB of the emulator's stack is part of the port's address space (the original's locals live there, below the
# arm's entry esp); only the pages below STACK_INIT - 0x8000 stay out (S4.2 kept the whole 1 MB out, U-1413; S4.8 mapped the
# top; since S4.2b every key does).
STACK_PAGES = range(E.STACK_BASE >> 12, (E.STACK_INIT - 0x8000) >> 12)


def machine_pages(m) -> dict[int, bytes]:
    """Every page the emulator holds except the bottom of its stack (host calls write their frames there)."""
    return {p: m.read(p << 12, PAGE) for p in sorted(m.mapped) if p not in STACK_PAGES}


def diff_writes(old: bytes, new: bytes, base: int) -> list[tuple[int, bytes]]:
    out, i, n = [], 0, len(old)
    while i < n:
        if old[i] == new[i]:
            i += 1
            continue
        j = i
        while j < n and old[j] != new[j]:
            j += 1
        out.append((base + i, new[i:j]))
        i = j
    return out


_DATA_RVA = None


def data_rva() -> int:
    """.data's rva in the user's sound.dll: below it are the headers, .text and .rdata (read-only for the port)."""
    global _DATA_RVA
    if _DATA_RVA is None:
        _DATA_RVA = next(r for r, v, prot, name in E._raw(str(E.SOUND_DLL))[2] if name == ".data")
    return _DATA_RVA


def pick_data_base(pages) -> int:
    """A GameState base no mapped page overlaps (the GameState is 0x381174 bytes)."""
    occupied = set(pages)
    for cand in (0x20000000, 0x24000000, 0x28000000, 0x2C000000):
        if not any((cand >> 12) <= p < ((cand + 0x400000) >> 12) for p in occupied):
            return cand
    raise RuntimeError("no free GameState base")


# ---- digests ----------------------------------------------------------------------------------------------------------
def digest(ret_text: str | None, state_hex: str | None) -> str:
    return hashlib.sha256(f"{ret_text}|{state_hex}".encode()).hexdigest()[:20]


def ret_text(spec: dict, r: dict, obj, style: str) -> str:
    if spec["ret"] in ("double", "float"):
        return E.format_double(E._from_json_float(r["f"]), style)
    return E.format_ret(spec["ret"], r, {"obj": {"p": obj}} if obj is not None else {})


# ---- the x87 state ----------------------------------------------------------------------------------------------------
# The image format of portsim::x87::Unit (audio.h): "SX87", u16 cw, u16 sw, u16 full tag word, u16 0, physical registers
# 0..7 as u64 significand, u16 sign|exponent.
CC_MASK = 0x4700           # C3 C2 C1 C0
TOP_MASK = 0x3800
X87_IMAGE = 4 + 8 + 80


def fpu_image(uc) -> bytes:
    from unicorn.x86_const import UC_X86_REG_FP0, UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG
    out = b"SX87" + struct.pack("<HHHH", uc.reg_read(UC_X86_REG_FPCW) & 0xFFFF, uc.reg_read(UC_X86_REG_FPSW) & 0xFFFF,
                                uc.reg_read(UC_X86_REG_FPTAG) & 0xFFFF, 0)
    for i in range(8):
        m, e = uc.reg_read(UC_X86_REG_FP0 + i)
        out += struct.pack("<QH", m & (2 ** 64 - 1), e & 0xFFFF)
    return out


def set_fpu(uc, img: bytes):
    from unicorn.x86_const import UC_X86_REG_FP0, UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG
    cw, sw, tag, _ = struct.unpack_from("<HHHH", img, 4)
    uc.reg_write(UC_X86_REG_FPCW, cw)
    uc.reg_write(UC_X86_REG_FPSW, sw)
    uc.reg_write(UC_X86_REG_FPTAG, tag)
    for i in range(8):
        m, e = struct.unpack_from("<QH", img, 12 + 10 * i)
        uc.reg_write(UC_X86_REG_FP0 + i, (m, e))


def cc_unknown(img: bytes | None) -> bool:
    """the port's unit marked its condition codes unknown (a body whose compares are not modelled ran: audio.h cc_unknown)"""
    return bool(img) and (struct.unpack_from("<H", img, 10)[0] & 1) == 1


def x87_view(img: bytes) -> dict:
    cw, sw, tag, _ = struct.unpack_from("<HHHH", img, 4)
    regs = {}
    for i in range(8):
        if (tag >> (2 * i)) & 3 != 3:
            regs[i] = struct.unpack_from("<QH", img, 12 + 10 * i)
    return {"cw": cw, "top": (sw & TOP_MASK) >> 11, "cc": sw & CC_MASK, "regs": regs}


def x87_digest(img: bytes, mask_cc: bool = False) -> str:
    """The golden form of an exit image: a digest of exactly what x87_diff compares (C0..C3 left out when the port's unit had
    them unknown: mask_cc)."""
    v = x87_view(img)
    return hashlib.sha256(json.dumps([v["cw"], v["top"], None if mask_cc else v["cc"],
                                      sorted(v["regs"].items())]).encode()).hexdigest()[:20]


def x87_diff(port: bytes | None, emu: bytes | None) -> str:
    """'' when the unit the port left equals the FPU the original left (cw, TOP, C0..C3, registers in use, their bits)."""
    if port is None or emu is None:
        return "" if port == emu else "missing x87 image"
    a, b = x87_view(port), x87_view(emu)
    out = []
    for k in (("cw", "top") if cc_unknown(port) else ("cw", "top", "cc")):
        if a[k] != b[k]:
            out.append(f"{k} port 0x{a[k]:x} emu 0x{b[k]:x}")
    if set(a["regs"]) != set(b["regs"]):
        out.append(f"in use port {sorted(a['regs'])} emu {sorted(b['regs'])}")
    for i in sorted(set(a["regs"]) & set(b["regs"])):
        if a["regs"][i] != b["regs"][i]:
            (m1, e1), (m2, e2) = a["regs"][i], b["regs"][i]
            out.append(f"R{i} port {e1:04x}:{m1:016x} emu {e2:04x}:{m2:016x}")
    return "; ".join(out)


def pop_image(img: bytes) -> bytes:
    """The image after the caller's `fstp` of a double / float return (what the emulator's return stub does)."""
    cw, sw, tag, z = struct.unpack_from("<HHHH", img, 4)
    top = (sw >> 11) & 7
    tag |= 3 << (2 * top)
    sw = (sw & ~TOP_MASK) | (((top + 1) & 7) << 11)
    return img[:4] + struct.pack("<HHHH", cw, sw, tag, z) + img[12:]


def norm_cw(word: int) -> int:
    """Unicorn reads a control word with its reserved bit 6 clear; the hardware reads it set."""
    return (word & 0xFFFF) | 0x40


# ---- KeyRun extensions (installed once) -------------------------------------------------------------------------------
# The emulator's FPU at key start: Unicorn never ran an `fninit`, so its eight tags read `valid` (zero registers), while a
# Windows thread starts with an empty stack. The unit is compared register by register, so the tags are set empty (TOP 0)
# once the key's setup (DllMain) is done; the control word stays the emulator's. Unicorn's arithmetic does not read the tags
# (no stack fault, U-1407): SG_S44_RAW_TAGS=1 keeps the raw state (log/roadmap/S4/4_tags_check.tsv: digests equal both ways).
RAW_TAGS = os.environ.get("SG_S44_RAW_TAGS") == "1"
COVERAGE: dict[int, dict] = {}       # rva -> {"jcc": {va: (target, fall)}, "seen": {va: sides}, "ranges", "insns", "hit"}
COVER_ON = False


def _fixture_sources() -> list[str]:
    return [p.read_text(encoding="utf-8") for p in sorted(FIXTURE_DIR.glob("*.js"))] if FIXTURE_DIR.is_dir() else []


if not getattr(E.KeyRun, "_sr_patched", False):
    _orig_init, _orig_setup, _orig_run, _orig_arm = E.KeyRun.__init__, E.KeyRun.setup, E.KeyRun.run, E.KeyRun._arm

    def _init(self, vm, key, *a, **kw):
        if key in CUSTOM or key in DERIVED:
            hooks()[key] = spec_of(key)          # KeyRun reads the registry dict
        _orig_init(self, vm, key, *a, **kw)

    def _setup(self):
        m = _orig_setup(self)
        if str(self.spec.get("fixture", "")).startswith("s49b_"):
            r = self.vm.request({"op": "eval", "src": user_files_js()})
            if "error" in r:
                raise E.EmuError("fixture", "s49b user files: " + r["error"])
        for src in _fixture_sources():
            r = self.vm.request({"op": "eval", "src": src})
            if "error" in r:
                raise E.EmuError("fixture", "sound_fixtures: " + r["error"])
        if self.second_arm is not None and not RAW_TAGS:
            from unicorn.x86_const import UC_X86_REG_FPSW, UC_X86_REG_FPTAG
            m.uc.reg_write(UC_X86_REG_FPTAG, 0xFFFF)
            m.uc.reg_write(UC_X86_REG_FPSW, m.uc.reg_read(UC_X86_REG_FPSW) & ~TOP_MASK & 0xFFFF)
        return m

    def _run(self):
        if self.key in PATCHES and self.fixture_patch is None:
            self.fixture_patch = PATCHES[self.key]
        return _orig_run(self)

    def _arm(self):
        """The original arm with the FPU it found and left recorded (only when a second arm, the port, follows)."""
        if self.second_arm is None:
            return _orig_arm(self)
        if self.key in ZERO_STACK:
            self.m.write(E.STACK_INIT - 0x2000, bytes(0x2000))
        hs = _cover_hooks(self) if COVER_ON else []
        self._x87_in = fpu_image(self.m.uc)
        try:
            return _orig_arm(self)
        finally:
            self._x87_out = fpu_image(self.m.uc)
            for h in hs:
                self.m.uc.hook_del(h)

    E.KeyRun.__init__, E.KeyRun.setup, E.KeyRun.run, E.KeyRun._arm = _init, _setup, _run, _arm
    E.KeyRun._sr_patched = True


# ---- coverage of the emulated original (oracle --coverage) ------------------------------------------------------------
def _ranges_of(rva):
    if not hasattr(_ranges_of, "rows"):
        with open(ROOT / "re" / "functions_ghidra_sound.dll.tsv", newline="") as f:
            _ranges_of.rows = {int(r["entry"], 16): r for r in csv.DictReader(f, delimiter="\t")}
    row = _ranges_of.rows.get(rva)
    return [tuple(int(x, 16) for x in rg.split("-")) for rg in row["ranges"].split(";")] if row else []


def _cover_hooks(run):
    """Per function (its ranges in re/functions_ghidra_sound.dll.tsv) every instruction the ORIGINAL arm executes and, for
    each conditional jump of its own body, which sides it takes (the instruction executed right after the jcc)."""
    import capstone
    from unicorn import UC_HOOK_CODE
    rva = run.spec["addr"]
    if rva not in COVERAGE:
        img = E.load_image(0x10000000)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        jcc, insns, ranges = {}, set(), _ranges_of(rva)
        for lo, hi in ranges:
            for i in md.disasm(bytes(img.data[lo:hi]), lo):
                insns.add(i.address)
                if i.mnemonic.startswith("j") and i.mnemonic != "jmp" and i.op_str.startswith("0x"):
                    jcc[i.address] = (int(i.op_str, 16), i.address + i.size)
        COVERAGE[rva] = {"jcc": jcc, "seen": {a: set() for a in jcc}, "ranges": ranges, "insns": insns, "hit": set()}
    cov = COVERAGE[rva]
    base = run.img.base
    state = {"prev": None}

    def on_code(uc, addr, size, _):
        r = addr - base
        cov["hit"].add(r)
        p = state["prev"]
        if p is not None:
            t, f = cov["jcc"][p]
            cov["seen"][p].add("T" if r == t else ("F" if r == f else "?"))
        state["prev"] = r if r in cov["jcc"] else None

    return [run.m.uc.hook_add(UC_HOOK_CODE, on_code, None, base + lo, base + hi - 1) for lo, hi in cov["ranges"]]


def coverage_rows():
    names = {int(r["rva"], 16): r["name"] for r in step_rows_all()}
    out = []
    for rva, cov in sorted(COVERAGE.items()):
        n = len(cov["jcc"])
        both = sum(1 for s in cov["seen"].values() if {"T", "F"} <= s)
        one = sum(1 for s in cov["seen"].values() if len(s & {"T", "F"}) == 1)
        none = [f"0x{a:x}" for a, s in sorted(cov["seen"].items()) if not s & {"T", "F"}]
        single = [f"0x{a:x}{'T' if 'T' in s else 'F'}" for a, s in sorted(cov["seen"].items()) if len(s & {"T", "F"}) == 1]
        out.append((names.get(rva, f"0x{rva:x}"), f"0x{rva:x}", len(cov["insns"]), len(cov["hit"] & cov["insns"]), n, both,
                    one, n - both - one, " ".join(single), " ".join(none)))
    return out


# ---- the emulator's signaling-NaN quirk (U-1451) ----------------------------------------------------------------------
def _quirk_bytes_ok(bo: bytes, bp: bytes):
    """True when every differing byte is the quiet bit of a NaN (double: bit 51 of a qword, float: bit 22 of a dword) that the
    port set and the emulator left clear; returns (ok, number of forgiven NaNs)"""
    if len(bo) != len(bp):
        return False, 0
    diffs = [k for k in range(len(bo)) if bo[k] != bp[k]]
    done, n = set(), 0
    for k in diffs:
        if k in done:
            continue
        ok = False
        if k + 1 < len(bo) and k >= 6:
            o, q = bo[k - 6:k + 2], bp[k - 6:k + 2]
            if ((o[7] & 0x7F) == 0x7F and (o[6] & 0xF0) == 0xF0 and not (o[6] & 0x08) and q[6] == (o[6] | 0x08)
                    and o[:6] == q[:6] and o[7] == q[7] and (o[6] & 0x07 or any(o[:6]))):
                ok = True
                done.add(k)
        if not ok and k + 1 < len(bo) and k >= 2:
            o, q = bo[k - 2:k + 2], bp[k - 2:k + 2]
            if ((o[3] & 0x7F) == 0x7F and (o[2] & 0x80) and not (o[2] & 0x40) and q[2] == (o[2] | 0x40)
                    and o[:2] == q[:2] and o[3] == q[3] and (o[2] & 0x3F or any(o[:2]))):
                ok = True
                done.add(k)
        if not ok:
            return False, 0
        n += 1
    return True, n


def quirk_equal(orig_hex, port_hex):
    if orig_hex is None or port_hex is None:
        return False, 0
    ro, rp = orig_hex.split("|"), port_hex.split("|")
    if len(ro) != len(rp):
        return False, 0
    total = 0
    for a, b in zip(ro, rp):
        if a == b:
            continue
        ok, n = _quirk_bytes_ok(bytes.fromhex(a), bytes.fromhex(b))
        if not ok:
            return False, 0
        total += n
    return True, total


# ---- the live second arm ----------------------------------------------------------------------------------------------
@dataclasses.dataclass
class VecRecord:
    args: list
    obj: int | None
    claims: list
    pages: dict           # page -> bytes: the port's starting memory
    script: list          # host calls: dicts
    report: dict
    esp: int = 0
    x87_in: bytes | None = None
    x87_exp: bytes | None = None
    x87_port: bytes | None = None
    x87_diff: str = ""
    error: str | None = None


class LivePort:
    """KeyRun's second arm: the port over the emulator's memory, with the emulator as the live host."""

    def __init__(self, lib, key: str, record: bool):
        self.lib, self.key, self.record = lib, key, record
        self.spec = spec_of(key)
        self.name = port_name(key)
        self.st = lib.state()
        self.space = PortSpace(self.st)
        self.records: list[VecRecord] = []
        self.err = None
        self._cb = HOSTFN(self._host)
        self.run = None
        self.cur = None

    def close(self):
        try:
            self.st.call("audio_core.detach", [])
        except L.SgError:
            pass
        self.st.close()

    # host: the emulator runs the call on the port's memory and on the port's x87 unit (the box page)
    def _host(self, user, pc):
        c = pc.contents
        try:
            m = self.run.m
            before = self.space.pages()
            for p, b in before.items():
                if m.read(p << 12, PAGE) != b:
                    m.write(p << 12, b)
            box = m.read(FPU_BOX, X87_IMAGE)
            if box[:4] == b"SX87":
                set_fpu(m.uc, box)
            args = [{"t": "uint32", "v": int(c.args[i])} for i in range(c.nargs)]
            abi = ABI_NAME[c.abi]
            r = m.native_call(int(c.va), abi, args, "double" if c.want_st0 else "uint32")
            m.write(FPU_BOX, fpu_image(m.uc))
            # the emulator's memory back into the port; pages the host mapped are mapped here too
            new_pages = {}
            writes = []
            for base, size, buf in self.space.runs:
                cur = m.read(base, size)
                old = C.string_at(C.addressof(buf), size)
                if cur != old:
                    if self.record:
                        writes += diff_writes(old, cur, base)
                    C.memmove(C.addressof(buf), cur, size)
            have = self.space.mapped()
            for p in sorted(m.mapped):
                if p not in have and p not in STACK_PAGES:
                    new_pages[p] = m.read(p << 12, PAGE)
            if new_pages:
                self.space.map_pages(new_pages)
            c.eax = r["eax"] & 0xFFFFFFFF
            c.edx = r["edx"] & 0xFFFFFFFF
            fb = None
            if c.want_st0 and "fbits" in r:
                fbits = bytes.fromhex(r["fbits"])
                c.st0 = struct.unpack("<d", fbits)[0] if len(fbits) == 8 else struct.unpack("<f", fbits)[0]
                c.has_st0 = 1
                fb = struct.unpack("<Q", struct.pack("<d", c.st0))[0]
            if self.record:
                self.cur.script.append({"kind": c.kind, "va": c.va, "abi": c.abi, "nargs": c.nargs, "want": c.want_st0,
                                        "args": [int(c.args[i]) for i in range(c.nargs)], "eax": c.eax, "edx": c.edx,
                                        "has_st0": c.has_st0, "st0": fb or 0,
                                        "writes": [(va, b.hex()) for va, b in writes],
                                        "maps": [(p << 12, b.hex()) for p, b in new_pages.items()]})
            return 0
        except Exception as e:  # noqa: BLE001
            self.err = f"host: {type(e).__name__}: {e}"
            return 1

    def __call__(self, run, args):
        self.run = run
        m = run.m
        if (FPU_BOX >> 12) not in m.mapped:                # the box page, in the emulator so the page syncs carry it
            m._map(FPU_BOX, PAGE)
            m.write(FPU_BOX, bytes(PAGE))
        slots = slots_of(args)
        esp = entry_esp(self.spec, slots)
        img_in, img_exp = getattr(run, "_x87_in", None), getattr(run, "_x87_out", None)
        pages = machine_pages(m)
        st = self.st
        st.unmap_all()
        st.set_data_base(pick_data_base(pages))
        self.space.ro = (run.img.base, run.img.base + data_rva())
        self.space.load(pages)
        r = self.lib.dll.sg_snd_set_host(st.ctx, self._cb, None)
        assert r == 0
        claims = sorted(int(t) for t in m.redirects)
        st.files_put("$audio/claims", b"".join(struct.pack("<I", v) for v in claims))
        st.call("audio_core.setup", [run.img.base, SETUP_CLAIMS | SETUP_CALLBACK | SETUP_RESET])
        st.files_put("$audio/x87", img_in or b"")
        st.call("audio_core.vector", [esp, esp - FRAME_LIMIT])
        ca = run._req({"op": "curargs"})
        self.cur = VecRecord(slots, ca.get("obj"), claims, pages if self.record else {}, [], {}, esp, img_in, img_exp)
        self.err = None
        # host calls of the port go below the port's frames, as the original's callees run below its locals
        saved_depth, saved_esp = m.depth, m.uc.reg_read(E.UC_X86_REG_ESP)
        m.depth += 1
        m.uc.reg_write(E.UC_X86_REG_ESP, esp - HOST_GAP)
        try:
            res = st.call(self.name, slots)
            out = {"eax": res.eax, "edx": res.edx}
            if res.has_st0:
                out["f"] = E._json_float(res.st0)
            elif self.spec["ret"] in ("double", "float"):
                out["f"] = "NaN"
        except L.SgError as e:
            self.cur.error = f"{e}{'; ' + self.err if self.err else ''}"
            out = None
        finally:
            m.depth = saved_depth
            m.uc.reg_write(E.UC_X86_REG_ESP, saved_esp)
        st.call("audio_core.finish", [])
        self.cur.report = parse_report(st.files_read("$audio/out"))
        xp = st.files_read("$audio/x87out")
        if self.spec["ret"] in ("double", "float") and out is not None and xp:
            xp = pop_image(xp)
        self.cur.x87_port = xp
        self.cur.x87_diff = x87_diff(xp, img_exp) if img_exp is not None else ""
        if self.cur.report.get("x87_detail"):
            self.cur.error = (self.cur.error or "") + "; x87: " + self.cur.report["x87_detail"]
        if self.cur.error is None and self.err:
            self.cur.error = self.err
        # the port's memory back into the emulator (KeyRun compares the regions there)
        for p, b in self.space.pages().items():
            if p in m.mapped and m.read(p << 12, PAGE) != b:
                m.write(p << 12, b)
        if img_exp is not None:
            set_fpu(m.uc, img_exp)          # the original arm's exit state goes on to the next vector
        self.records.append(self.cur)
        if out is None:
            raise E.EmuError("fault", "port: " + self.cur.error)
        return out


def parse_report(b: bytes | None) -> dict:
    out = {}
    for line in (b or b"").decode(errors="replace").splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            out[k] = v
    return out


# ---- the importstandin.js log ($implog) -------------------------------------------------------------------------------
def decode_implog(raw: bytes):
    """importstandin.js LOG: +0 records, +4 byte cursor, +8 calls not recorded, +0x10 per-import counters, +0x90 records
    {u32 import index, u32 value count, u32 values...}."""
    n, cur, lost = struct.unpack_from("<III", raw, 0)
    recs, off = [], 0x90
    for _ in range(n):
        idx, cnt = struct.unpack_from("<II", raw, off)
        recs.append((idx, list(struct.unpack_from(f"<{cnt}I", raw, off + 8))))
        off += 8 + 4 * cnt
    return recs, lost


def original_implogs(key, res):
    """The emulated original's import stand-in log per vector (layout A, the `$implog` region of arm 1's final state),
    decoded; None when the key has no `$implog` region."""
    names = [s[0] for s in spec_of(key).get("state", [])]
    if res is None or isinstance(res, Exception) or "$implog" not in names:
        return None
    k = names.index("$implog")
    out = []
    for row in res.rows:
        if row.error or not row.state_hex:
            out.append(None)
            continue
        recs, lost = decode_implog(bytes.fromhex(row.state_hex.split("|")[k]))
        out.append({"records": [[i, v] for i, v in recs], "lost": lost})
    return out


# ---- oracle -----------------------------------------------------------------------------------------------------------
@dataclasses.dataclass
class KeyVerdict:
    key: str
    name: str
    vectors: int
    match: int          # vectors where port == emulated original (layout A): return, regions and x87 exit state
    match_bc: int       # the same in layouts B and C (vectors x 2)
    errors: int         # vectors where the original or the port stopped
    fallbacks: int      # fallback calls of the port, all layouts
    tier: str           # the census tier of the emulated original against the CSV (derived / custom keys: EMU_ONLY)
    first: str
    golden: list        # emulated original's digests per vector (layout A)
    port_digests: list
    x87diff: int = 0    # vectors (all layouts) whose x87 exit state differs
    forgiven: dict = dataclasses.field(default_factory=dict)   # vector -> NaNs forgiven (U-1451), layout A
    pending: str | None = ""   # '' none; 'S4.k:name ...' every fallback reaches another step's row; None: one is this step's
    x87_exit: list = dataclasses.field(default_factory=list)
    words: list = dataclasses.field(default_factory=list)
    implog: list | None = None
    verdict: str = "RED"
    cc_unknown: int = 0   # vectors (layout A) whose C0..C3 were not compared (the port's unit had them unknown)


def pending_of(key, recs) -> str | None:
    """'' when the port made no fallback call; the `<step>:<name>` list when every fallback reached code no ported step owns (a
    row of a step not in PORTED_STEPS, the static CRT no step owns: `-:<name>`, an unnamed callee: `-:0x<rva>`, S4.11's work);
    None when one reached a row of a ported step (its function is not registered: RED)."""
    rows = {int(r["rva"], 16): r for r in step_rows_all()}
    rvas = set()
    for r in recs or []:
        for x in (r.report.get("fallback_rvas") or "").split(","):
            if x:
                rvas.add(int(x))
    out = []
    for rva in sorted(rvas):
        row = rows.get(rva)
        if row is not None and STEP_OF_FUNCTION.get(row["name"], row["step"]) in PORTED_STEPS:
            return None
        out.append(f"{row['step']}:{row['name']}" if row else f"-:0x{rva:x}")
    return " ".join(out)


def oracle_key(vm, lib, key: str, layouts=("A", "B", "C"), record=True) -> tuple[KeyVerdict, list | None, list]:
    """(KeyVerdict, layout A's VecRecords or None, layout A's state regions)"""
    vm = vm_for(vm, key)
    spec = spec_of(key)
    quirk = row_name(key) in QUIRK_FUNCTIONS
    res, ports = {}, {}
    for lay in layouts:
        lp = LivePort(lib, key, record and lay == "A")
        try:
            res[lay] = E.KeyRun(vm, key, lay, second_arm=lp).run()
        except E.EmuError as e:
            res[lay] = e
        finally:
            lp.close()
        ports[lay] = lp
    a = res["A"]
    if isinstance(a, Exception) or a.fixture_error:
        return KeyVerdict(key, port_name(key), len(spec["vectors"]), 0, 0, len(spec["vectors"]), 0, "SETUP_ERR",
                          str(a if isinstance(a, Exception) else a.fixture_error)[:200], [], [], verdict="RED"), None, []
    first, match, match_bc, errs, x87d = "", 0, 0, 0, 0
    golden, pdig, forgiven = [], [], {}

    def row_ok(row, rec):
        """(ok, NaNs forgiven, x87 differs)"""
        xd = bool(rec is not None and rec.x87_diff)
        if row.match:
            return not xd, 0, xd
        if quirk and not row.error and row.orig == row.re:
            ok, n = quirk_equal(row.state_hex, row.state_re_hex)
            if ok:
                return not xd, n, xd
        return False, 0, xd

    recs_a = ports["A"].records
    for i, row in enumerate(a.rows):
        rec = recs_a[i] if i < len(recs_a) else None
        ok, n, xd = row_ok(row, rec)
        x87d += xd
        if n:
            forgiven[i] = n
        golden.append(digest(row.orig, row.state_re_hex if n else row.state_hex))
        pdig.append(digest(row.re, row.state_re_hex))
        if row.error:
            errs += 1
            first = first or f"v{i} A: {row.error[:160]}"
        elif ok:
            match += 1
        elif not first:
            first = (f"v{i} A: x87 {rec.x87_diff}" if xd and row.orig == row.re else
                     f"v{i} A: ret emu {row.orig} port {row.re}" + ("" if row.state_hex == row.state_re_hex else " (regions differ)"))
    for lay in layouts[1:]:
        rb = res[lay]
        if isinstance(rb, Exception):
            first = first or f"{lay}: {rb}"
            continue
        recs = ports[lay].records
        for i, row in enumerate(rb.rows):
            ok, _, xd = row_ok(row, recs[i] if i < len(recs) else None)
            x87d += xd
            if ok:
                match_bc += 1
            elif not first:
                first = f"v{i} {lay}: " + (row.error[:160] if row.error else f"ret emu {row.orig} port {row.re}")
    fb = sum(int(r.report.get("fallback", 0) or 0) for lp in ports.values() for r in lp.records)
    if derived(key):
        tier = "EMU_ONLY"
    elif not {"B", "C"} <= set(res):
        tier = "A_ONLY"
    else:
        import sound_census as SC
        tier = SC.compare(key, spec, batch_of(key), res, E.read_csv(spec, key))["row"]["tier"]
    pend = pending_of(key, [r for lp in ports.values() for r in lp.records])
    v = KeyVerdict(key, port_name(key), len(a.rows), match, match_bc, errs, fb, tier, first, golden, pdig, x87d, forgiven,
                   pend)
    v.x87_exit = [x87_digest(r.x87_exp, cc_unknown(r.x87_port)) if r.x87_exp else None for r in recs_a]
    v.cc_unknown = sum(1 for r in recs_a if cc_unknown(r.x87_port))
    v.words = sorted({norm_cw(struct.unpack_from("<H", r.x87_in, 4)[0]) for r in recs_a if r.x87_in})
    v.implog = original_implogs(key, a)
    allm = v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and v.vectors > 0
    if allm and tier in TIERS and pend is not None:
        v.verdict = "GREEN_PENDING" if pend else "GREEN"
    if pend:
        v.first = (v.first + " " if v.first else "") + "pending " + pend
    return v, (recs_a if record else None), [list(x) for x in a.regions]


def _safe(key: str) -> str:
    return key.replace("::", "_").replace("@", "__").replace("<", "(").replace(">", ")").replace("*", "P")


def save_bundle(key: str, lay: str, records: list[VecRecord], style: str, base: int):
    BUNDLE_DIR.mkdir(parents=True, exist_ok=True)
    prev: dict[int, bytes] = {}
    vecs = []
    for r in records:
        delta = {p: b for p, b in r.pages.items() if prev.get(p) != b}
        gone = [p for p in prev if p not in r.pages]
        prev = dict(r.pages)
        vecs.append({"args": r.args, "obj": r.obj, "claims": r.claims, "error": r.error, "report": r.report,
                     "delta": {str(p): base64.b64encode(b).decode() for p, b in delta.items()}, "gone": gone,
                     "script": r.script, "esp": r.esp, "x87": (r.x87_in or b"").hex()})
    blob = json.dumps({"key": key, "layout": lay, "base": base, "style": style, "format": 2, "vectors": vecs}).encode()
    (BUNDLE_DIR / f"{_safe(key)}.json.z").write_bytes(zlib.compress(blob, 6))


def load_bundle(key: str) -> dict | None:
    p = BUNDLE_DIR / f"{_safe(key)}.json.z"
    if not p.exists():
        return None
    return json.loads(zlib.decompress(p.read_bytes()))


def script_blob(entries: list[dict]) -> bytes:
    out = bytearray(b"SGAS" + struct.pack("<II", 1, len(entries)))
    for e in entries:
        out += struct.pack("<5I", e["kind"], e["va"], e["abi"], e["nargs"], e["want"])
        out += b"".join(struct.pack("<I", a) for a in e["args"])
        out += struct.pack("<3I", e["eax"], e["edx"], e["has_st0"]) + struct.pack("<Q", e["st0"])
        out += struct.pack("<I", len(e["writes"]))
        for va, hx in e["writes"]:
            b = bytes.fromhex(hx)
            out += struct.pack("<II", va, len(b)) + b
        out += struct.pack("<I", len(e["maps"]))
        for va, hx in e["maps"]:
            b = bytes.fromhex(hx)
            out += struct.pack("<II", va, len(b)) + b
    return bytes(out)


def golden_entry(v: KeyVerdict, regions) -> dict:
    g = {"name": v.name, "vectors": v.vectors, "digests": v.golden, "tier": v.tier, "regions": regions,
         "x87_exit": v.x87_exit, "fpcw": v.words, "fallbacks": v.fallbacks}
    if v.pending:
        g["pending"] = v.pending
    if v.forgiven:
        g["quirk"] = {str(i): n for i, n in v.forgiven.items()}
    if v.implog is not None:
        g["implog"] = v.implog
    if v.cc_unknown:
        g["cc_unknown"] = v.cc_unknown
    return g


def oracle_main(keys, layouts="ABC", update=False, out=None, coverage=None) -> int:
    global COVER_ON
    COVER_ON = bool(coverage)
    lib = load_lib()
    goldens: dict[pathlib.Path, dict] = {}
    rows = []
    fb_by = collections.Counter()
    with E.FixVM() as vm:
        for n, key in enumerate(keys, 1):
            v, recs, regions = oracle_key(vm, lib, key, tuple(layouts), record=update)
            print(f"[{n}/{len(keys)}] {key} {v.verdict} match {v.match}/{v.vectors} bc {v.match_bc} err {v.errors} "
                  f"fallback {v.fallbacks} x87diff {v.x87diff}{' ccU ' + str(v.cc_unknown) if v.cc_unknown else ''} tier {v.tier} "
                  f"cw {','.join(hex(w) for w in v.words)}"
                  f"{' quirk ' + str(v.forgiven) if v.forgiven else ''} {v.first}", flush=True)
            rows.append(v)
            for x in (v.pending or "").split():
                fb_by[x] += 1
            if update and recs is not None and v.verdict != "RED":
                spec = spec_of(key)
                style = E.csv_double_style(spec, key) if not derived(key) else "shortest"
                save_bundle(key, "A", recs, style, E.LAYOUTS["A"]["base"])
                gp = golden_path_of(key)
                if gp not in goldens:
                    goldens[gp] = json.loads(gp.read_text()) if gp.exists() else {}
                goldens[gp][key] = golden_entry(v, regions)
    for gp, g in goldens.items():
        gp.parent.mkdir(parents=True, exist_ok=True)
        gp.write_text(json.dumps(g, indent=1, sort_keys=True) + "\n")
    if out:
        with open(out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["key", "name", "verdict", "vectors", "match", "match_bc", "errors", "fallbacks", "x87diff", "tier",
                        "quirk", "pending", "first"])
            for v in rows:
                w.writerow([v.key, v.name, v.verdict, v.vectors, v.match, v.match_bc, v.errors, v.fallbacks, v.x87diff,
                            v.tier, json.dumps(v.forgiven), v.pending, v.first])
    if coverage:
        with open(coverage, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["function", "rva", "instructions", "executed", "jcc", "both_sides", "one_side", "none",
                        "one_side_taken", "never"])
            w.writerows(coverage_rows())
        print(f"coverage of {len(COVERAGE)} functions -> {coverage}")
    c = collections.Counter(v.verdict for v in rows)
    vecs = sum(v.vectors for v in rows)
    print(f"keys {len(rows)}  vectors {vecs}  green {c['GREEN']}  green_pending {c['GREEN_PENDING']}  red {c['RED']}  "
          f"fallbacks {sum(v.fallbacks for v in rows)}  quirk-forgiven vectors {sum(len(v.forgiven) for v in rows)}  "
          f"{' '.join(v.key for v in rows if v.verdict == 'RED')}")
    if fb_by:
        print("pending callees (keys): " + ", ".join(f"{k} {n}" for k, n in sorted(fb_by.items())))
    return 1 if c["RED"] else 0


# ---- default replay ---------------------------------------------------------------------------------------------------
@dataclasses.dataclass
class ReplayResult:
    key: str
    vectors: int
    passed: int
    fallbacks: int
    first: str
    pending: str = ""


def replay_vectors(lib, key: str, golden: dict | None = None, prefix: str = "", soft: bool = False):
    """The default replay of one key, vector by vector: yields (index, vector record, result dict). result: ret text (or
    None), err, report, x87 exit image, the port's pages after the call, the starting pages."""
    b = load_bundle(key)
    if b is None:
        return
    spec = spec_of(key)
    name = prefix + port_name(key)
    pages: dict[int, bytes] = {}
    st = lib.state()
    space = PortSpace(st)
    space.ro = (b["base"], b["base"] + data_rva())
    try:
        if soft:
            st.call("audio_core.x80soft", [1])
        for i, v in enumerate(b["vectors"]):
            for p in v["gone"]:
                pages.pop(p, None)
            for p, b64 in v["delta"].items():
                pages[int(p)] = base64.b64decode(b64)
            st.unmap_all()
            st.set_data_base(pick_data_base(pages))
            space.load(pages)
            st.files_put("$audio/claims", b"".join(struct.pack("<I", c) for c in v["claims"]))
            st.files_put("$audio/script", script_blob(v["script"]))
            st.call("audio_core.setup", [b["base"], SETUP_CLAIMS | SETUP_SCRIPT | SETUP_RESET])
            esp = v.get("esp", 0)
            st.files_put("$audio/x87", bytes.fromhex(v.get("x87", "")))
            st.call("audio_core.vector", [esp, esp - FRAME_LIMIT if esp else 0])
            res = {"txt": None, "err": None}
            try:
                r = st.call(name, v["args"])
                out = {"eax": r.eax, "edx": r.edx}
                if r.has_st0:
                    out["f"] = E._json_float(r.st0)
                res["txt"] = ret_text(spec, out, v["obj"], b["style"])
            except L.SgError as e:
                res["err"] = str(e)
            st.call("audio_core.finish", [])
            res["report"] = parse_report(st.files_read("$audio/out"))
            xp = st.files_read("$audio/x87out")
            if spec["ret"] in ("double", "float") and res["err"] is None and xp:
                xp = pop_image(xp)
            res["x87"] = xp
            res["space"] = space
            res["start"] = dict(pages)
            res["base"] = b["base"]
            yield i, v, res
            st.call("audio_core.detach", [])
    finally:
        if soft:
            try:
                st.call("audio_core.x80soft", [0])
            except L.SgError:
                pass
        st.close()


def replay_key(lib, key: str, golden: dict, vector_filter=None, prefix: str = "", soft: bool = False) -> ReplayResult:
    if load_bundle(key) is None:
        return ReplayResult(key, 0, 0, 0, "no bundle (run the oracle: SG_EMU_ORACLE=1 ... oracle --update)")
    g = golden.get(key)
    if not g or not g.get("digests"):
        return ReplayResult(key, 0, 0, 0, "no golden digests")
    want, want_x87 = g["digests"], g.get("x87_exit") or []
    regions = [tuple(x) for x in g["regions"]]
    passed = fb = n = 0
    first = ""
    for i, v, res in replay_vectors(lib, key, golden, prefix, soft):
        if vector_filter is not None and i not in vector_filter:
            continue
        n += 1
        rep = res["report"]
        fb += int(rep.get("fallback", 0) or 0)
        state_hex = "|".join(res["space"].read(a, k).hex() for a, k in regions) if regions else None
        got = digest(res["txt"], state_hex) if res["err"] is None else None
        xok = i >= len(want_x87) or want_x87[i] is None or (
            res["x87"] and x87_digest(res["x87"], cc_unknown(res["x87"])) == want_x87[i])
        ok = got == want[i] and rep.get("script_pos") == rep.get("script_len") and xok
        if v.get("error") and res["err"] is not None:
            ok = False      # the oracle run errored on this vector: never counted as a pass
        if ok:
            passed += 1
        elif not first:
            why = res["err"] or ("" if got == want[i] else f"digest {got} != {want[i]}")
            if not xok:
                why += f"; x87 exit {x87_view(res['x87']) if res['x87'] else None}"
            first = (f"v{i}: {why}; script {rep.get('script_pos')}/{rep.get('script_len')}; {rep.get('detail', '')}"
                     f"{'; x87: ' + rep['x87_detail'] if rep.get('x87_detail') else ''}")[:300]
    return ReplayResult(key, n, passed, fb, first, g.get("pending", ""))


def replay_ok(r: ReplayResult) -> bool:
    return bool(r.vectors) and r.passed == r.vectors and (r.fallbacks == 0 or bool(r.pending))


def replay_main(keys, w32=False, soft=False, negctl=None, prefix="") -> int:
    lib = load_w32() if w32 else load_lib()
    golden = load_golden(keys=keys)
    bad = 0
    if negctl:
        fn, mode = negctl.rsplit(":", 1)
        lib.negctl(fn, int(mode))
    try:
        for n, key in enumerate(keys, 1):
            r = replay_key(lib, key, golden, prefix=prefix, soft=soft)
            ok = replay_ok(r)
            bad += 0 if ok else 1
            tag = ("PASS_PENDING" if r.fallbacks else "PASS") if ok else "FAIL"
            print(f"[{n}/{len(keys)}] {key} {tag} {r.passed}/{r.vectors} fallback {r.fallbacks} {r.first}", flush=True)
    finally:
        if negctl:
            lib.negctl(fn, 0)
    print(f"keys {len(keys)}  fail {bad}")
    return 1 if bad else 0


# ---- the AudioSink command log against the fixtures' import logs (S4.8) ------------------------------------------------
def sound_imports() -> list[tuple[int, str]]:
    """(IAT slot rva, 'DLL!name') of every import of the user's sound.dll."""
    import pefile
    pe = pefile.PE(str(E.SOUND_DLL))
    ib = pe.OPTIONAL_HEADER.ImageBase
    out = [(i.address - ib, f"{d.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}")
           for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports]
    pe.close()
    return out


def _subsequence(vals, args) -> bool:
    it = iter(args)
    return all(any(v == x for x in it) for v in vals)


def _records_match(args, vals, nm="") -> bool:
    """A stand-in records all slots, the N leading ones (`record: N`), an index list (pointer slots dropped: the values are a
    subsequence of the slots; S4.8's PeekMessageA skips its MSG buffer) or, for the mmio imports, the transforms of
    mmiostandin.js (and c3dh's memory-open record): S4.9's rules."""
    n = len(vals)
    if args[:n] == vals or (n > 0 and args[len(args) - n:] == vals) or _subsequence(vals, args):
        return True
    if nm.endswith("mmioOpenA") and len(args) == 3:
        if n == 3 and vals[2] == args[2] and vals[1] == (1 if args[1] else 0):
            return True            # mmiostandin.js: crc32(name) or 0, info != NULL, flags
        if n == 20 and (vals[0], vals[1]) == (args[0], args[2]):
            return True            # c3dh's memory-open stand-in: name, flags, then the 18 MMIOINFO dwords
    if nm.endswith("mmioDescend") and len(args) == 4:
        return n == 3 and vals[0] == args[0] and vals[1] == (1 if args[2] else 0) and vals[2] == args[3]
    if nm.endswith("mmioAdvance") and len(args) == 3:
        return n == 3 and vals[0] == args[0] and vals[1] == (1 if args[1] else 0) and vals[2] == args[2]
    if nm.endswith("mmioStringToFOURCCA") and len(args) == 2:
        return n == 2 and vals[1] == args[1]     # the four-character code of the string (a pointer here), then the flags
    return False


def sinklog_key(lib, key, golden, imps, cmd_names=None):
    """For every vector of a key whose fixture stands imports in (a `$implog` state region): the log the port's replay leaves
    equals the EMULATED ORIGINAL's (decoded by the oracle into the golden file, "implog"), record for record; every port call
    whose target is the value of one of sound.dll's IAT slots appends exactly ONE log record, for the same import as every
    other record of that index, whose values are the command's recorded slots. A record is attributed to the host call whose
    memory writes appended it, so records written by original code the host ran are counted apart. Returns (verdict, vectors
    equal, vectors, import commands, records from host-run code, first)."""
    spec = spec_of(key)
    names = [s[0] for s in spec.get("state", [])]
    regions = golden.get(key, {}).get("regions", [])
    logs = [tuple(regions[i]) for i, nm in enumerate(names) if nm == "$implog" and i < len(regions)]
    vec_ok = vec_n = cmds_all = hrec = 0
    first = ""
    for i, v, res in replay_vectors(lib, key, golden):
        script, start, base = v["script"], res["start"], res["base"]
        final = res["space"].pages()
        slot_val = {}
        for srva, nm in imps:
            p = (base + srva) >> 12
            if p in start:
                slot_val.setdefault(struct.unpack_from("<I", start[p], (base + srva) & 0xFFF)[0], []).append(nm)
        cmds = [(slot_val[e["va"]][0], e["args"], e["va"]) for e in script if e["kind"] == 0 and e["va"] in slot_val]
        if cmd_names is not None:
            for c in cmds:
                cmd_names[c[0]] += 1
        cmds_all += len(cmds)
        if not logs:
            continue
        vec_n += 1
        a, sz = logs[0]

        def pg(pages):
            return b"".join(pages.get(p, bytes(PAGE)) for p in range(a >> 12, ((a + sz - 1) >> 12) + 1))
        raw = pg(final)[a & 0xFFF:(a & 0xFFF) + sz]
        recs_port, lost = decode_implog(raw)
        orig = (golden.get(key, {}).get("implog") or [None] * (i + 1))[i]
        good = (res["err"] is None and orig is not None and orig["lost"] == lost
                and [(r[0], r[1]) for r in orig["records"]] == recs_port)
        cur = bytearray(pg(start)[a & 0xFFF:(a & 0xFFF) + sz])
        idx_name, why = {}, ""
        for e in script:
            dec0 = decode_implog(bytes(cur))
            before, lost_b = len(dec0[0]), dec0[1]
            for va, hx in e["writes"]:
                b = bytes.fromhex(hx)
                lo, hi = max(va, a), min(va + len(b), a + sz)
                if lo < hi:
                    cur[lo - a:hi - a] = b[lo - va:hi - va]
            dec1 = decode_implog(bytes(cur))
            new, lost_a = dec1[0][before:], dec1[1]
            is_cmd = e["kind"] == 0 and e["va"] in slot_val and not (E.TRAP_LO <= e["va"] < E.TRAP_HI)
            if not is_cmd:
                hrec += len(new)
                continue
            nm = slot_val[e["va"]][0]
            if len(new) == 0 and lost_a > lost_b:
                continue            # the log is full: the call is counted in the header, not recorded
            if len(new) != 1:
                why = why or f"{nm}: {len(new)} records for one command"
                continue
            idx, vals = new[0]
            if idx_name.setdefault(idx, nm) != nm or not _records_match(e["args"], vals, nm):
                why = why or f"{nm} {e['args']} vs record {new[0]}"
        good = good and not why
        if good:
            vec_ok += 1
        elif not first:
            first = (f"v{i}: {why or 'log differs from the original'}; port records {recs_port[:3]} original "
                     f"{(orig or {}).get('records', [])[:3]}")[:300]
    verdict = "NOLOG" if not logs else ("EQUAL" if vec_ok == vec_n else "DIFF")
    return verdict, vec_ok, vec_n, cmds_all, hrec, first


def sinklog_main(keys, negctl=None) -> int:
    lib = load_lib()
    if negctl:      # a control: the function's mode on during the check (S4.9's SG_SINK_NEGCTL)
        fn, md = negctl.rsplit(":", 1)
        lib.negctl(fn, int(md))
    golden = load_golden(keys=keys)
    imps = sound_imports()
    rows, bad, host_recs = [], 0, 0
    cmd_names = collections.Counter()
    for n, key in enumerate(keys, 1):
        verdict, ok, tot, cmds, hrec, first = sinklog_key(lib, key, golden, imps, cmd_names)
        host_recs += hrec
        bad += verdict == "DIFF"
        rows.append((key, verdict))
        print(f"[{n}/{len(keys)}] {key} {verdict} {ok}/{tot} vectors, {cmds} import commands, {hrec} records from "
              f"host-run code {first}", flush=True)
    print("import commands by name: " + ", ".join(f"{k} {v}" for k, v in sorted(cmd_names.items())))
    print(f"keys {len(rows)}  with an import log {sum(1 for r in rows if r[1] != 'NOLOG')}  equal "
          f"{sum(1 for r in rows if r[1] == 'EQUAL')}  diff {bad}  import commands {sum(cmd_names.values())}  records from "
          f"host-run code {host_recs}")
    return 1 if bad else 0


# ---- negative controls (live) -----------------------------------------------------------------------------------------
def negctl_plan(keys, mode, step=None, functions=None, callee=False) -> dict[str, list[str]]:
    """{registered function: the keys that judge its mode}. callee=True (U-1425): each of `functions` is judged through ALL
    the given keys (a callee's control under its callers' keys) instead of its own."""
    if callee and functions:
        return {(f if f.startswith(("audio_", SND)) else SND + f): list(keys) for f in functions}
    by_fn: dict[str, list[str]] = {}
    for k in keys:
        by_fn.setdefault(port_name(k), []).append(k)
    if mode == 1 and step in NEGCTL_EXTRA:
        every = [k for s in STEPS for k in keys_for(s)] if NEGCTL_EXTRA[step] else []
        for fn, prefixes in NEGCTL_EXTRA[step].items():
            ks = [k for k in every if k.startswith(prefixes)]
            if ks:
                by_fn[fn] = ks
    if mode != 1:
        by_fn = {f: ks for f, ks in by_fn.items() if mode in NEGCTL_MODES.get(bare(f), ())}
    if functions:
        by_fn = {f: ks for f, ks in by_fn.items() if f in functions or bare(f) in functions}
    return by_fn


def negctl_main(keys, mode, out=None, step=None, functions=None, callee=False) -> int:
    """Per function: mode `mode` on, every key of the function through the live oracle (layout A). RED = at least one
    vector where the port differs from the emulated original (return, regions or x87 exit state) or stops."""
    lib = load_lib()
    by_fn = negctl_plan(keys, mode, step, functions, callee)
    rows = []
    with E.FixVM() as vm:
        for n, (fn, ks) in enumerate(sorted(by_fn.items()), 1):
            lib.negctl(fn, mode)
            try:
                bad = tot = xbad = 0
                for k in ks:
                    v, _, _ = oracle_key(vm, lib, k, ("A",), record=False)
                    tot += v.vectors
                    bad += v.vectors - v.match
                    xbad += v.x87diff
            finally:
                lib.negctl(fn, 0)
            verdict = "RED" if bad else "BLIND"
            rows.append((fn, mode, verdict, bad, xbad, tot, " ".join(ks)))
            print(f"[{n}/{len(by_fn)}] {fn} mode {mode} {verdict} {bad}/{tot} vectors differ ({xbad} in the x87 exit state)",
                  flush=True)
    if out:
        with open(out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["function", "mode", "verdict", "vectors_differ", "x87_differ", "vectors", "keys"])
            w.writerows(rows)
    blind = [r[0] for r in rows if r[2] != "RED"]
    print(f"functions {len(rows)}  red {len(rows) - len(blind)}  blind {len(blind)} {' '.join(blind)}")
    return 1 if blind else 0


# ---- CLI --------------------------------------------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["oracle", "replay", "negctl", "sinklog", "keys"])
    ap.add_argument("--step", default="S4.2", help="re/roadmap/S4_steps.tsv step (S4.2 .. S4.9)")
    ap.add_argument("--family", default=None, help="one family of the step's rows (S4.8: ds or dev)")
    ap.add_argument("--keys", nargs="*")
    ap.add_argument("--update", action="store_true", help="oracle: write bundles and golden entries of the keys that pass")
    ap.add_argument("--layouts", default="ABC")
    ap.add_argument("--coverage", help="oracle: the emulated original's instruction and jcc coverage per function (TSV)")
    ap.add_argument("--out", default=None, help="TSV of verdicts")
    ap.add_argument("--w32", action="store_true", help="replay: the Win32 backend")
    ap.add_argument("--soft", action="store_true", help="replay on Win32: the software x87 path instead of the instructions")
    ap.add_argument("--negctl", default=None, help="replay: fn:mode on during the replay")
    ap.add_argument("--prefix", default="", help="replay: registered name = prefix + hooks.csv name (conv: for the converter)")
    ap.add_argument("--mode", type=int, default=1, help="negctl: the mode")
    ap.add_argument("--functions", nargs="*", help="negctl: only these functions (bare or snd. names)")
    ap.add_argument("--callee", action="store_true", help="negctl: judge each of --functions through all --keys (U-1425)")
    a = ap.parse_args(argv)
    keys = a.keys or keys_for(a.step, a.family)
    if a.cmd == "keys":
        print("\n".join(keys))
        return 0
    if a.cmd == "oracle":
        return oracle_main(keys, a.layouts, a.update, a.out, a.coverage)
    if a.cmd == "negctl":
        return negctl_main(keys, a.mode, a.out, a.step, a.functions, a.callee)
    if a.cmd == "sinklog":
        return sinklog_main(keys, a.negctl)
    return replay_main(keys, a.w32, a.soft, a.negctl, a.prefix)


from port.test import sound_keys_crt as _s411  # noqa: E402  (S4.11: its rows, golden, custom keys, excluded keys, PORTED_STEPS)
_s411.install(sys.modules[__name__])

if __name__ == "__main__":
    sys.exit(main())
