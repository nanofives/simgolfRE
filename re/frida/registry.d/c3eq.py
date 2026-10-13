# C3 batch c3eq (round 31, 2026-10-13) of golf_clean.exe: the round-30 leftovers and the Bink video player.
# Reimplementations in shim/src/re/c3eq.cpp; fixtures in re/frida/js/fixtures.d/c3eq.js; Purposes and per-jcc branch
# tables in log/c3/c3eq_purpose.md; notes in log/c3/c3eq_notes.txt.
#
# Every key runs under __uiTree (re/frida/js/uitree.js): frozen first thread, heap stand-in arena, import stand-in,
# recording display at 0x0083ad50, neutral window-tree globals.

EXE = "golf_clean.exe"

# re/frida/js/uitree.js: GLOBALS (4 bytes each), the z-list array, the recorder size
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000


def UI_STATE(arena=0x8000):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- BinkPlayer::play
# (this, path, offX, offY, soundParam). The rows mirror PLAY in the fixture; the fourth argument is 0 on every third row
# so both sides of the _BinkSetSoundSystem test at 0x004870a9 run.
_PLAY_OFF = [(0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (12, 34), (-1, -2), (7, 0), (0, 0), (0, 0), (1, 1),
             (0x7FFFFFFF, -0x80000000), (5, 6), (0, 0)]
HOOKS["BinkPlayer::play"] = dict(
    module=EXE, addr=0x00487090, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "int"], fixture="c3eq_play",
    state=UI_STATE() + [(0x0083AB9C, 0, 4)],
    vectors=[(f"$p{i}", f"$path{i}", _PLAY_OFF[i][0], _PLAY_OFF[i][1], 0 if i % 3 == 0 else 0x1234 + i)
             for i in range(14)])

# ---------------------------------------------------------------------------------------------------- playAudioFile
# (path, immediate). The live BinkPlayer 0x005aa6d0 and the deadline 0x005aa774 are restored between the arms.
_AUDIO_STATE = UI_STATE() + [(0x0083AB9C, 0, 4), (0x005AA6D0, 0, 0x18), (0x005AA774, 0, 4)]
HOOKS["playAudioFile"] = dict(
    module=EXE, addr=0x0043CCE0, abi="default", ret="int", args=["pointer", "int"], fixture="c3eq_audio",
    state=_AUDIO_STATE,
    vectors=[("$path0", 0), ("$path1", 1), ("$path2", 0), ("$path3", 1), ("$path4", 0), ("$path5", 1),
             ("$path6", 0), ("$path7", 1), ("$missing", 0), ("$missing", 1), ("$path0", 1), ("$path7", 0)])

# ---------------------------------------------------------------------------------------------------- TextView::render
# (this). Ten real TextViews per key; one key per setting of the two globals the function reads and cannot be varied by
# an argument: the display width its vtable slot +0xa8 answers (0x0048ffab) and the combo 0x0083fe08 with the width
# listCurrentIndex / Text_put report for it (0x0049026e-0x004902c3).
_RENDER_STATE = UI_STATE(0x60000) + [("$lg", 0, 0x200), (0x0083FE08, 0, 4), (0x0083B8D8, 0, 8)]
for _k, _fx in (("", "c3eq_render"), ("_big", "c3eq_render_big"), ("_nodisplay", "c3eq_render_nodisp"),
                ("_combo_short", "c3eq_render_cs"), ("_combo_long", "c3eq_render_cl"),
                ("_combo_none", "c3eq_render_c0")):
    HOOKS[f"TextView::render{_k}"] = dict(
        module=EXE, addr=0x0048FE60, abi="thiscall", ret="int", args=["pointer"], fixture=_fx,
        state=_RENDER_STATE, vectors=[(f"$tv{i}",) for i in range(12)])
