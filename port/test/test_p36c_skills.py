"""P36C (play-test 18 item 3, log/roadmap/P36/P36C_notes.md): the pro's skills past 100 %.

Three play switches, 1 = the original:
  narrateShot.skillTop100         on 10 or more holes a shot raises the pro's skills up to level 20 (200 %); no "expand your
                                  course to a Championship course" line at 100 %, a "most this skill can improve" line at 200 %
  narrateShot.missLowersTopSkill  a missed shot leaves a pro's skill at the cap that applies to the course alone
  skillPointDialog.top100         the dialog's + goes past 100 % (to 200 %) once all ten rows are at 100 % or more

narrateShot runs on a constructed shot of the followed golfer 5 (slot 0, box corner x 0: off screen, so no SimFoto, the
reward still runs for the followed golfer) with club 5: the reward draw's case 8 (club 3..10) picks Accurate Irons (skill 3)
without a mask test, and every other shot skill is put at the cap (masked out of a good shot's draw) or at 0 (out of a missed
shot's draw, which takes the skills above 3), so skill 3 is the only skill the call can change.
"""
from __future__ import annotations

import random
import struct

import pytest

from port.test import lib as L
from port.test.test_commands import DATA_VA, base_image, put
from port.test.test_dialogs import TITLE_VA, _dialog_image, port_run

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]

SKILL = 3   # Accurate Irons, picked by the draw's case 8


def _obj(portlib, name):
    return next((o, s) for n, o, s in portlib.objects() if n == name)


def _need(portlib):
    if any(f not in portlib.functions() for f in ("buildGolferName", "appendHoleName", "golferShotReach")):
        pytest.skip("narrateShot's callees are not registered yet")


def _narrate(portlib, holes, level, others, flag, top100=1, miss=1):
    """(the pro's skill 3 after the call, the text buffer)"""
    g = 5
    try:
        img = base_image(portlib)   # the exe's initialised data: the skill and class name tables point into it
        with portlib.state() as st:
            st.set_data_base(DATA_VA)
            st.load_image(img)
            st.write(_obj(portlib, "g_storyPairs")[0], struct.pack("<hh", g, 0))
            st.write(_obj(portlib, "g_playerGolfer")[0], struct.pack("<i", g))
            st.write(_obj(portlib, "g_holesPlusOne")[0], struct.pack("<i", holes + 1))
            sk = bytearray([others] * 10 + [0] * 22)
            sk[SKILL] = level
            ps, _ = _obj(portlib, "g_playerSkills")
            st.write(ps, bytes(sk))
            go, _ = _obj(portlib, "g_golfers")
            R = go + 0x100 * g
            st.write(R + 0x08, struct.pack("<ii", 0, 0x100))   # off screen: the followed golfer keeps the reward, no frame
            st.write(R + 0x18, b"\x01")                         # a classed golfer (the reward needs kind != 0)
            st.write(R + 0x1C, b"\x05")                         # club 5: the draw's case 8
            st.write(R + 0xA2, struct.pack("<h", 6))
            portlib.defect("narrateShot.skillTop100", top100)
            portlib.defect("narrateShot.missLowersTopSkill", miss)
            st.call("narrateShot", [0, 3, 140, flag])
            after = st.read(ps + SKILL, 1)[0]
            tb, _ = _obj(portlib, "g_textBuffer")
            raw = st.read(tb, 1024)
    finally:
        portlib.defect("narrateShot.skillTop100", 1)
        portlib.defect("narrateShot.missLowersTopSkill", 1)
    return after, raw.split(b"\0", 1)[0].decode("latin-1")


FURTHER = "To further improve this skill"
MOST = "This is the most this skill can improve."


# (holes, switch, level before, the other skills, level after, text that must appear, text that must not)
GOOD = [
    (14, 1, 9, 10, 10, ["skill improves to 100%", FURTHER, "Championship course"], [MOST]),   # the original's prompt
    (14, 1, 10, 10, 10, [], ["improves to"]),                                                 # 100 % is the top
    (18, 1, 10, 10, 10, [], ["improves to"]),                                                 # also on 18 holes
    (14, 0, 9, 20, 10, ["skill improves to 100%"], [FURTHER, MOST]),                          # no line at 100 %
    (14, 0, 10, 20, 11, ["skill improves to 110%"], [FURTHER, MOST]),
    (14, 0, 19, 20, 20, ["skill improves to 200%", MOST], [FURTHER]),
    (14, 0, 20, 20, 20, [], ["improves to"]),                                                 # 200 % is the top
    (18, 0, 19, 20, 20, ["skill improves to 200%", MOST], [FURTHER]),
    (9, 0, 7, 8, 8, ["skill improves to 80%", FURTHER, "Country Club course"], [MOST]),       # Daily Fee keeps 80 %
    (9, 0, 8, 8, 8, [], ["improves to"]),
    (5, 0, 5, 6, 6, ["skill improves to 60%", FURTHER, "Daily Fee course"], [MOST]),          # Municipal keeps 60 %
]


@pytest.mark.parametrize("holes,sw,before,others,after,has,hasnt", GOOD)
def test_good_shot_raise(portlib, holes, sw, before, others, after, has, hasnt):
    _need(portlib)
    level, text = _narrate(portlib, holes, before, others, 0, top100=sw)
    assert level == after, text
    for s in has:
        assert s in text, text
    for s in hasnt:
        assert s not in text, text


# (holes, skillTop100, missLowersTopSkill, level before, level after)
MISSED = [
    (14, 1, 1, 10, 9),    # the original: a 100 % skill loses 10 %
    (14, 1, 0, 10, 10),   # held at the Country Club cap 100 %
    (14, 1, 0, 9, 8),
    (18, 1, 0, 10, 10),   # Championship under the original top: held at 100 %
    (14, 0, 0, 10, 9),    # top 200 %: a 100 % skill still loses 10 %
    (14, 0, 0, 15, 14),
    (14, 0, 0, 20, 20),   # held at the top
    (14, 0, 1, 20, 19),
    (5, 1, 0, 6, 6),      # held at the Municipal cap 60 %
    (5, 1, 0, 5, 4),
    (9, 0, 0, 8, 8),      # held at the Daily Fee cap 80 %
    (9, 0, 0, 10, 10),    # above the cap (skill points): held too
]


@pytest.mark.parametrize("holes,top,miss,before,after", MISSED)
def test_missed_shot_drop(portlib, holes, top, miss, before, after):
    _need(portlib)
    level, text = _narrate(portlib, holes, before, 0, 1, top100=top, miss=miss)
    assert level == after, text
    assert (" loses 10% of" in text) == (after < before), text


# ------------------------------------------------------------------------------------------------ skillPointDialog
XBASE = 0xC8


def _plus(row):
    return (XBASE + 0x32 + 5, 0x5A + 0x18 * row + 5)


def _dialog(portlib, skills, row, sw):
    """the ten rows' levels after one + click on `row` with 1 point, then OK (and the points-left popup's Down Enter)"""
    from port.test import cmdemu as CE
    img = _dialog_image(portlib, random.Random(36), -1)
    put(portlib, img, "g_playerSkills", bytes(skills) + bytes(22))
    pt = _plus(row)
    inputs = CE.click(*pt)
    cursors = [pt]
    ok = (XBASE + 0x15E, 0x14E)
    for _ in range(2):
        inputs += CE.click(*ok) + [x for x in CE.key(0x28)[::2]] + CE.key(0x0D, 0x0D)
        cursors.append((0, 0))
    cursors += [(0, 0)] * 4
    try:
        portlib.defect("skillPointDialog.top100", sw)
        _, image, _, _, _, _ = port_run(portlib, "skillPointDialog", img, [TITLE_VA, 0, 1, -1, XBASE], inputs, cursors,
                                        (5, 0), salt=1)
    finally:
        portlib.defect("skillPointDialog.top100", 1)
    off = next(o for n, o, s in portlib.objects() if n == "g_playerSkills")
    return list(image[off:off + 10])


@pytest.mark.parametrize("sw,skills,row,want", [
    (1, [10] * 10, 0, 10),                    # the original: + stops at 100 %
    (0, [10] * 10, 0, 11),                    # all ten at 100 %: past it
    (0, [12, 10, 15, 10, 20, 10, 10, 11, 10, 10], 0, 13),
    (0, [20] + [10] * 9, 0, 20),              # 200 % is the top
    (0, [10] * 5 + [9] + [10] * 4, 0, 10),    # one row below 100 %: no row goes past
    (0, [10] * 5 + [9] + [10] * 4, 5, 10),    # that row still rises to 100 %
    (1, [9] + [10] * 9, 0, 10),
])
def test_skill_points_past_100(portlib, sw, skills, row, want):
    if "skillPointDialog" not in portlib.functions():
        pytest.skip("skillPointDialog not registered")
    after = _dialog(portlib, skills, row, sw)
    assert after[row] == want, after
    assert all(after[i] == skills[i] for i in range(10) if i != row), after
