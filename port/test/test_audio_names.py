"""The names the port registers for sound.dll functions (S4.2b, log/roadmap/merges/S4-3.md naming rule).

Every sound.dll function is registered as `snd.<hooks.csv name>` (re/roadmap/S4_functions.tsv `name`); the core's harness entries
keep `audio_core.<name>`. The rule exists because sound.dll and golf_clean.exe share function names (Random::next, CRT helpers ...)
and the vector harness looks a function up by the stem of its .vec file (port/corpus/vectors, golf_clean.exe functions only):
an unprefixed sound.dll name equal to an exe stem would run the wrong module's body.

These tests fail when
  * two registered functions have the same name (the registry's first match would hide the second),
  * a registered name normalised (`::` -> `_`) equals the .vec stem of a function of another module,
  * a function registered by a sound.dll module lacks the `snd.` prefix, or a `snd.` name is not a sound.dll name (a
    hooks.csv / re/names / S4_functions.tsv name, or FUN_<va> for a function the master lists unnamed with no such name).

The static half reads the FuncDef tables of port/src/sim; the library half reads the registry of the built DLL.

    ~/bin/py -3.12 -m pytest port/test/test_audio_names.py
"""
from __future__ import annotations

import collections
import csv
import ctypes as C
import pathlib
import re
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import lib as L  # noqa: E402
from port.test import sound_replay as R  # noqa: E402

pytestmark = pytest.mark.port
SIM = ROOT / "port" / "src" / "sim"
SND, HARNESS = "snd.", "audio_core."
# the sound.dll port's modules (S4.2 .. S4.9b, S4.11): every function they register is a sound.dll function or a harness entry
SOUND_MODULES = ("audio_core", "audio_mixer", "audio_chan", "audio_ds", "audio_dev", "audio_codec", "audio_seq", "audio_sound",
                 "audio_wave", "audio_soundfile", "audio_crt")
# audio_exe (S4.10) is the exe's sound layer: its names are exe functions, plus sound.dll functions it ports with the snd. prefix
# duplicates present at HEAD 2c05f02a, outside the sound.dll port (reported to the S4 lead, U-1419's note in 2b_notes.md):
# saveGame is registered by tick (tick.cpp) and by saveload (saveload.cpp); the registry's first match (modules.inc order) wins
KNOWN_DUPLICATES = {"saveGame": {"tick", "saveload"}}
ROW = re.compile(r'\{\s*"([^"]+)",\s*"(?:default|cdecl|stdcall|thiscall|fastcall|mscdecl)"')
MACRO = re.compile(r'#define\s+(\w+)\(name[^)]*\)\s*\{\s*"([^"]*)"\s*#name')


def norm(name: str) -> str:
    return name.replace("::", "_")


_ROWS = None


def _rva(text: str) -> int:
    v = int(text, 16)
    return v - 0x10000000 if v >= 0x10000000 else v


def sound_rows() -> set[str]:
    """Every name sound.dll has: re/roadmap/S4_functions.tsv, hooks.csv's sound.dll rows and the sound.dll rows of re/names
    (a name without a hooks.csv row yet, e.g. a later step's re/names/sound_<n>.tsv), plus FUN_<va> for every function of the
    master's sound.dll listing (re/functions_ghidra_sound.dll.tsv) that still has Ghidra's default name there AND no name in those
    three: the rva-based form for an unnamed function (rule in log/roadmap/S4/2b_notes.md). Once a function gets a name, its FUN_
    form is refused."""
    global _ROWS
    if _ROWS is None:
        out, named = set(), set()
        for r in R.step_rows_all():
            out.add(r["name"])
            named.add(_rva(r["rva"]))
        with (ROOT / "hooks.csv").open(newline="", encoding="utf-8") as f:
            for r in csv.DictReader(f):
                if (r.get("module") or "").lower() == "sound.dll":
                    out.add(r["name"])
                    named.add(_rva(r["addr"]))
        for p in (ROOT / "re" / "names").glob("*.tsv"):
            for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
                c = line.split("	")
                if len(c) >= 3 and c[0].lower() == "sound.dll":
                    out.add(c[2])
                    try:
                        named.add(_rva(c[1]))
                    except ValueError:
                        pass
        with (ROOT / "re" / "functions_ghidra_sound.dll.tsv").open(newline="", encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="	"):
                rva = int(r["entry"], 16)
                if r["name"] == f"FUN_{0x10000000 + rva:08x}" and rva not in named:
                    out.add(r["name"])
        _ROWS = out
    return _ROWS


def vec_stems() -> dict[str, str]:
    """stem -> module of every vector file: the committed manifest (re/port/vectors_manifest.tsv) and the local corpus."""
    out = {}
    with (ROOT / "re" / "port" / "vectors_manifest.tsv").open(newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r.get("vec_file") and r["vec_file"] != "-":
                out[pathlib.Path(r["vec_file"]).stem] = "golf_clean.exe" if int(r["addr"], 16) < 0x10000000 else "dll"
    for p in (ROOT / "port" / "corpus" / "vectors").glob("*.vec"):
        out.setdefault(p.stem, "golf_clean.exe")
    return out


def source_names() -> list[tuple[str, str]]:
    """(module file stem, registered name) of every FuncDef row in port/src/sim, the macro-built rows expanded."""
    out = []
    for p in sorted(SIM.glob("*.cpp")) + sorted(SIM.glob("*.inc")):
        t = p.read_text(encoding="utf-8", errors="replace")
        out += [(p.stem, n) for n in ROW.findall(t)]
        for macro, prefix in MACRO.findall(t):
            out += [(p.stem, prefix + n) for n in re.findall(rf"^\s*{macro}\((\w+),", t, re.M)]
    return out


def module_of_name(module: str, name: str, sound_modules=SOUND_MODULES) -> str:
    """golf_clean.exe or sound.dll: what the registered function is a port of."""
    return "sound.dll" if module in sound_modules and not name.startswith(HARNESS) else "golf_clean.exe"


def check(pairs: list[tuple[str, str]]) -> list[str]:
    bad = []
    by_name = collections.defaultdict(set)
    for m, n in pairs:
        by_name[n].add(m)
    counts = collections.Counter(n for _, n in pairs)
    for n, k in counts.items():
        if k > 1 and KNOWN_DUPLICATES.get(n) != by_name[n]:
            bad.append(f"registered {k} times: {n} ({', '.join(sorted(by_name[n]))})")
    rows, stems = sound_rows(), vec_stems()
    for m, n in pairs:
        # per name: every name of the sound.dll port's modules (harness entries aside) is sound.dll's; elsewhere a snd. name is
        # allowed only in an audio_* module (a later step's file in the tree, S4.10 / S4.11), never in an exe module (tick ...)
        own = "sound.dll" if n.startswith(SND) else module_of_name(m, n)
        if own == "sound.dll" and not n.startswith(SND):
            bad.append(f"{m}: sound.dll function without the prefix: {n}")
        elif n.startswith(SND) and m not in SOUND_MODULES and not m.startswith("audio_"):
            bad.append(f"{m}: {n} has the sound.dll prefix in a module that is not the sound.dll port's")
        elif n.startswith(SND) and n[len(SND):] not in rows:
            bad.append(f"{m}: {n} is not a sound.dll name (S4_functions.tsv, hooks.csv, re/names)")
        s = stems.get(norm(n))
        if s is not None and s != own:
            bad.append(f"{m}: {n} normalises to the .vec stem of a {s} function")
    return bad


# ================================================================================================ static
@pytest.mark.static
def test_source_tables_follow_the_naming_rule():
    pairs = source_names()
    mods = {m for m, _ in pairs}
    assert set(SOUND_MODULES) <= mods, sorted(set(SOUND_MODULES) - mods)
    assert check(pairs) == []


@pytest.mark.static
def test_every_sound_row_of_a_ported_step_is_registered_once():
    pairs = [(m, n) for m, n in source_names() if m in SOUND_MODULES and n.startswith(SND)]
    have = collections.Counter(n[len(SND):] for _, n in pairs)
    rows = [r for r in R.step_rows_all() if R.STEP_OF_FUNCTION.get(r["name"], r["step"]) in R.PORTED_STEPS]
    missing = sorted(r["name"] for r in rows if have[r["name"]] == 0)
    assert not missing, missing
    assert all(v == 1 for v in have.values())


@pytest.mark.static
@pytest.mark.parametrize("pairs, why", [
    ([("audio_seq", "snd.Seq_setTempo"), ("audio_sound", "snd.Seq_setTempo")], "registered 2 times"),
    ([("audio_seq", "Seq_setTempo")], "without the prefix"),
    ([("audio_seq", "snd.NotARow")], "is not a sound.dll name"),
    ([("tick", "snd.Seq_setTempo")], "has the sound.dll prefix"),
    ([("audio_exe", "snd.FUN_1002e3c0")], "is not a sound.dll name"),     # GlblHeader::init has a name (re/names/sound_19.tsv)
    ([("audio_exe", "snd.FUN_1002e3c1")], "is not a sound.dll name"),     # not a function entry of the master's listing
    ([("audio_exe", "snd.FUN_1000e810")], "is not a sound.dll name"),     # Wave_Device::set_rate (re/names/smsg_sound_1.tsv)
])
def test_the_check_catches_each_rule(pairs, why):
    """Negative controls of the check itself: each broken table is reported."""
    assert any(why in b for b in check(pairs)), check(pairs)


@pytest.mark.static
def test_an_unnamed_function_is_accepted_by_its_listing_name():
    assert "FUN_10029e30" in sound_rows() and check([("audio_exe", "snd.FUN_10029e30")]) == []


@pytest.mark.static
def test_the_check_catches_a_vec_stem_collision():
    stems = vec_stems()
    exe = sorted(s for s, m in stems.items() if m == "golf_clean.exe")
    assert exe
    # an exe stem registered under the sound.dll port's own name space would collide (the rule the prefix enforces)
    row = next(iter(sorted(sound_rows())))
    bad = check([("audio_seq", exe[0])])
    assert any("without the prefix" in b for b in bad) and any(".vec stem" in b for b in bad), bad
    assert check([("audio_seq", SND + row)]) == []


# ================================================================================================ the built registry
def test_registry_follows_the_naming_rule():
    lib = R.load_lib()
    pairs = []
    for i in range(lib.dll.sg_func_count()):
        fi = L.SgFuncInfo()
        assert lib.dll.sg_func_info_get(i, C.byref(fi)) == L.SG_OK
        pairs.append((fi.module.decode(), fi.name.decode()))
    mods = {m for m, _ in pairs}
    if not set(SOUND_MODULES) <= mods:
        pytest.skip(f"the DLL lacks {sorted(set(SOUND_MODULES) - mods)}")
    assert check(pairs) == []
