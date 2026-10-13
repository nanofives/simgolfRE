import os

# C3 batch c3fc (round 33, 2026-10-13): golf_clean.exe size-only rows over 2 KB.
# Reimplementations: shim/src/re/c3fc.cpp. Fixtures: re/frida/js/fixtures.d/c3fc.js.
# Purposes and per-jcc tables: log/c3/c3fc_purpose.md. Notes and drops: log/c3/c3fc_notes.txt.

EXE = "golf_clean.exe"

# loadSoundTable 0x00448220: __cdecl, no arguments, so one vector is one whole call (219 Snd::applyMode484e30 calls,
# stood in and logged). The single "int" argument is an unused cdecl stack word: diff_hook resolves "$sN" before the
# state snapshot, so the getter of fx.sN writes scene N's seed into the log (re/frida/js/fixtures.d/c3fc.js).
_SND_STATE = [
    ("$clog", 0, 0x6000),      # the stand-in's call log: count, FNV-1a hash, seed, entries (object, path text, mode)
]

# loadGolferRoster 0x004659a0: __cdecl, no arguments; one vector is one whole call over scene N (the getter of fx.sN
# writes the two fixture files, the listMatchingFiles / fopen scripts, the flag bit, the theme name and the text buffer).
_GR_STATE = [
    ("$clog", 0, 0x8000),      # every stood-in call: theme loads, list patterns, fopen paths, fgets, fclose, error texts
    ("$fileA", 0, 0x3800),     # the celebrity file block (cursor, length, bytes)
    ("$fileB", 0, 0x3800),     # the pro-golfer file block
    (0x0051A068, 0, 0x800),    # the shared text buffer (paths and the strtok fields)
    (0x0053A454, 0, 0x1388),   # the 50 copied list slots (0x004659f4 clear, 0x00465acd copy)
    (0x0080B130, 0, 0x2710),   # listMatchingFiles' table (written by its stand-in)
    (0x00838220, 0, 0x880),    # the slot word, the eight 0x10c entries and [0x00838a9c]
    (0x0055D738, 0, 0xE74),    # the 100 celebrity records
    (0x0058DD50, 0, 0x15E0),   # the 100 pro-golfer records
    (0x0059E7B8, 0, 4),        # the flag word (bit 0x10000000)
    (0x004E06C8, 0, 0x230),    # theme record 0x4c (written by the loadThemeFile stand-in)
    (0x0057E1A8, 0, 0x10),     # its ten skill bytes
]

# drawEmployeeDetailPanel 0x00436e50(x, y): the getter of fx.xN applies scene N (the walker records, the panel's
# globals, hitEmployee's answer); y is a plain value. Every drawing callee is a recording stand-in.
_EP_STATE = [
    ("$clog", 0, 0x6000),      # every stood-in call (images, circle, sprites, texts, targets, hire, sound, tooltip text)
    (0x00585850, 0, 0x1300),   # the 64 walker records (the mode word +0x1e is rewritten at 0x00437205)
    (0x0051A068, 0, 0x200),    # the text buffer
    (0x005A9F6C, 0, 0x10),     # the panel's itoa buffer (0x004375a7)
    (0x0058A528, 0, 0x20),     # appendNumber / appendDate itoa scratch
    (0x005A6D3C, 0, 4),        # the year word appendDate stores
    (0x005AA554, 0, 8),        # the tooltip tick counter and the scroll position
    (0x005AA5B8, 0, 0x40),     # the slot table
    (0x005AA6C0, 0, 8),        # the slot count and the last cell pointer
    (0x004C7AEC, 0, 4),        # the selected slot
    (0x004C2844, 0, 4),        # the circle global set around drawCircle
    (0x005A9F54, 0, 4),        # the last hit
    (0x0059CA54, 0, 4),        # the once flag of the hire button
]

HOOKS = {
    "drawEmployeeDetailPanel": dict(module=EXE, addr=0x00436E50, abi="default", ret="int", args=["int", "int"],
                                    fixture="c3fc_emp", state=_EP_STATE,
                                    vectors=[("$x%d" % i, "$y%d" % i) for i in range(34)]),

    "loadGolferRoster": dict(module=EXE, addr=0x004659A0, abi="default", ret="int", args=["int"],
                             fixture="c3fc_roster", state=_GR_STATE,
                             vectors=[("$s%d" % i,) for i in range(12)]),

    "loadSoundTable": dict(module=EXE, addr=0x00448220, abi="default", ret="int", args=["int"],
                           fixture="c3fc_sound", state=_SND_STATE,
                           vectors=[("$s%d" % i,) for i in range(12)]),
}

# Negative-control keys for the negctl build (shim/src/re/c3fc_negctl.cpp through log/c3/c3fc_negctl_batches.txt):
# one wrong detail per vector, picked through word 2 of the exported C3fcNegCtl switch. Registered only when
# C3FC_CONTROLS=1 (log/c3/alt/c3fc_ctl.py sets it), so a normal run never sees them.
if os.environ.get("C3FC_CONTROLS") == "1":
    HOOKS["loadSoundTable_neg"] = dict(module=EXE, addr=0x00448220, abi="default", ret="int", args=["int"],
                                       fixture="c3fc_sound_neg", state=_SND_STATE,
                                       vectors=[("$n%d" % i,) for i in range(4)])
    HOOKS["drawEmployeeDetailPanel_neg"] = dict(module=EXE, addr=0x00436E50, abi="default", ret="int",
                                                args=["int", "int"], fixture="c3fc_emp_neg", state=_EP_STATE,
                                                vectors=[("$n%d" % i, "$m%d" % i) for i in range(8)])
    HOOKS["loadGolferRoster_neg"] = dict(module=EXE, addr=0x004659A0, abi="default", ret="int", args=["int"],
                                         fixture="c3fc_roster_neg", state=_GR_STATE,
                                         vectors=[("$n%d" % i,) for i in range(6)])
