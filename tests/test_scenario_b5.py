"""Batch b5 (2026-10-17): the scenario determinism fixes and the championship-rounds loop of cov_b_shots / cov_b_shots2.

- scenario_ab.back_buffer_image: the checkpoint frame from jgld's back buffer (RGB555 / 565 -> the window's RGB values).
- scenario_ab.stack_seed and scenario_det.js STACK SEED: courseActionMenu's uninitialised frame words zeroed at the thunk
  0x00470a00 by default, `probe` records them, `0` leaves the thunk alone.
- scenario.py _cov_b_champ_rounds: replayed against a synthetic schedule, the results panel that comes up in the middle of
  Gary's turn stops the turn, gets its checkpoint and its check mark, and the next round starts from the title menu.
Static: no game is started (re/analysis/data/scenario_baseline.md section 11 has the runtime evidence)."""
import os
import struct
import sys

import numpy as np
import pefile
import pytest

from conftest import ROOT

pytestmark = pytest.mark.static
sys.path.insert(0, str(ROOT / "re" / "frida"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import scenario_ab as S  # noqa: E402
import scenario as SCN  # noqa: E402

META = {"ok": True, "bpp": 16, "w": 800, "h": 600, "wb": 1600, "comp": 0, "masks": [0, 0, 0], "biHeight": 600}


def test_back_buffer_rgb555_widens_like_the_window():
    v = np.zeros((600, 800), np.uint16)
    v[0, 0] = (31 << 10) | (0 << 5) | 1          # r 31 -> 255, g 0 -> 0, b 1 -> 8
    v[0, 1] = (12 << 10) | (12 << 5) | 19        # 12 -> 99, 19 -> 156: the location screen's lavender (99, 99, 156)
    v[599, 799] = 0x7FFF
    img = S.back_buffer_image(dict(META), v.tobytes())
    a = np.asarray(img)
    assert img.size == (800, 600) and img.mode == "RGB"
    assert tuple(a[0, 0]) == (255, 0, 8)
    assert tuple(a[0, 1]) == (99, 99, 156)
    assert tuple(a[599, 799]) == (255, 255, 255)      # top-down rows although biHeight is +600


def test_back_buffer_rgb565_and_refusals():
    v = np.zeros((600, 800), np.uint16)
    v[5, 5] = (31 << 11) | (63 << 5) | 0
    m = dict(META, comp=3, masks=[0xF800, 0x7E0, 0x1F])
    assert tuple(np.asarray(S.back_buffer_image(m, v.tobytes()))[5, 5]) == (255, 255, 0)
    assert S.back_buffer_image(dict(META, ok=False), v.tobytes()) is None
    assert S.back_buffer_image(dict(META, bpp=8), v.tobytes()) is None
    assert S.back_buffer_image(dict(META, w=640), v.tobytes()) is None
    assert S.back_buffer_image(dict(META, comp=3, masks=[1, 2, 3]), v.tobytes()) is None


def test_frame_and_stack_seed_switches(monkeypatch):
    monkeypatch.delenv("C3FT_FRAME", raising=False)
    monkeypatch.delenv("C3FT_STACK_SEED", raising=False)
    assert S.frame_mode() == "backbuffer" and S.stack_seed() == "zero"
    monkeypatch.setenv("C3FT_FRAME", "window")
    monkeypatch.setenv("C3FT_STACK_SEED", "probe")
    assert S.frame_mode() == "window" and S.stack_seed() == "probe"
    monkeypatch.setenv("C3FT_STACK_SEED", "0")
    assert S.stack_seed() == ""
    monkeypatch.setenv("C3FT_STACK_SEED", "yes")
    with pytest.raises(SystemExit):
        S.stack_seed()


def test_stack_seed_targets_the_only_entry_of_course_action_menu():
    """The thunk the stub replaces is `jmp 0x0046f550`, and the three slots it records are the frame words the
    function reads before writing (0x00470027 / 0x00470299 / 0x0047028f with F = entry esp - 0x4f0)."""
    pe = pefile.PE(str(ROOT / "original" / "golf_clean.exe"), fast_load=True)
    img = pe.get_memory_mapped_image()
    at = lambda va, n: img[va - 0x400000:va - 0x400000 + n]
    th = at(0x00470A00, 5)
    assert th[0] == 0xE9 and 0x00470A05 + struct.unpack("<i", th[1:])[0] == 0x0046F550
    # prologue: push -1; push handler; push fs:[0] (3 dwords), sub esp, 0x4d4, four register pushes
    assert at(0x0046F565, 6) == bytes.fromhex("81ecd4040000")
    assert at(0x0046F56B, 4) == bytes.fromhex("53555657")
    assert at(0x00470027, 5) == bytes.fromhex("f644242408")      # test byte [esp+0x24], 8
    assert at(0x00470299, 4) == bytes.fromhex("8b442428")        # mov eax, [esp+0x28]
    assert at(0x0047028F, 4) == bytes.fromhex("395c2430")        # cmp [esp+0x30], ebx
    assert 0xC + 0x4D4 + 0x10 - 0x24 == 0x4CC
    js = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text(encoding="utf-8")
    assert "if (CFG.stackSeed) {" in js and "ptr(0x00470a00)" in js and "TARGET = 0x0046f550" in js
    for disp in ("-0x4c0", "-0x4bc", "-0x4b4"):
        assert f"u32({disp})" in js


# ---------------------------------------------------------------- the championship-rounds loop, replayed
class FakeRun:
    def __init__(self, obs):
        self.mode = "replay"
        self.schedule = {"obs": obs, "waits": {"0:main_menu": 0}}


class FakeCtx:
    """Ctx's surface for lockstep scripts: input is recorded, sleep advances T by one 100 ms poll per yield."""

    def __init__(self, obs):
        self.run = FakeRun(obs)
        self.T = 0
        self.wait_index = 0
        self.inputs = []
        self.checkpoints = []

    def sleep(self, ms):
        end = self.T + ms
        while self.T < end:
            self.T += 100
            yield

    def wait_screen(self, ref, crop=None, threshold=0, timeout=0, name=""):
        self.wait_index += 1
        yield

    def click(self, x, y, right=False, hold=150, settle=350):
        self.inputs.append(("click", x, y))
        yield from self.sleep(settle + hold + 100)

    def key(self, vk, hold=150, char=None):
        self.inputs.append(("key", vk))
        yield from self.sleep(hold + 50)

    def move(self, x, y, dt=0):
        self.inputs.append(("move", x, y))

    def checkpoint(self, name):
        self.checkpoints.append(name)
        yield


def obs_schedule(states):
    """obs keys in the order _cov_b_champ_rounds asks them: each entry is (kind, value)."""
    out = {}
    for i, (k, v) in enumerate(states):
        out[f"cb{i}:{k}"] = v
    return out


def test_results_mid_turn_ends_the_turn_and_starts_the_next_round(monkeypatch):
    # turn 1 fires; turn 2 sees the results after its first key; the check mark leads to the title menu; round 2's
    # first turn fires; then the cap of 2 shots ends the run
    seq = [("tstate", "other"), ("over", False),                      # Esc path: no results, Esc pressed
           ("tstate", "gary"), ("green", [500, 200]), ("target", [510, 190])]
    seq += [("over", False)] * 6                                       # five key groups + the aim group of shot 1
    seq += [("tstate", "gary"), ("green", None), ("target", [400, 260]), ("over", True),   # results after key 1
            ("tstate", "over"), ("after_results", "menu"),
            ("tstate", "gary"), ("green", [300, 300]), ("target", [300, 300])]
    seq += [("over", False)] * 6
    seq += [("shots_end", {"shots": 2, "rounds": 2, "virtual_s": 0})]
    c = FakeCtx(obs_schedule(seq))
    for _ in SCN._cov_b_champ_rounds(c, (390, 238), SCN._cov_b_shots_turn, cap_s=10_000, max_shots=2):
        pass
    assert c.checkpoints == ["tournament_start", "results", "end"]
    keys = [i for i in c.inputs if i[0] == "key"]
    assert keys[0] == ("key", 0x1B)                                    # the Esc of the first `other`
    clicks = [i for i in c.inputs if i[0] == "click"]
    # two fired shots (SHOT_STRAIGHT then the target each), the results check mark, two championship starts (6 each)
    assert clicks.count(("click",) + SCN.SHOT_STRAIGHT) == 2
    assert ("click",) + SCN.RESULTS_OK in clicks
    assert clicks.count(("click", 400, 540)) == 2                      # Play a Championship, rounds 1 and 2
    assert len(c.run.schedule["obs"]) == len(seq)                      # every recorded decision was asked for


def test_turn_and_post_hole_panel_templates():
    """The post-hole panel is within COV_B_PANEL_MAX of the shot panel (19.9 measured) and matches its own template;
    the shot panel itself is far from the post-hole template (every turn measured 19.9 or more)."""
    import imgcmp
    if not (SCN.GOLDEN / "gary_skill_panel.png").exists() or not (SCN.GOLDEN / "shot_panel.png").exists():
        pytest.skip("goldens are not committed (tests/golden/README.md)")
    shot = imgcmp.load(SCN.GOLDEN / "shot_panel.png")
    holed = imgcmp.load(SCN.GOLDEN / "gary_skill_panel.png")
    assert holed.size == shot.size == (SCN.SHOT_PANEL[2] - SCN.SHOT_PANEL[0], SCN.SHOT_PANEL[3] - SCN.SHOT_PANEL[1])
    assert imgcmp.distance(holed, shot) <= SCN.COV_B_PANEL_MAX
    assert imgcmp.distance(shot, holed) > SCN.COV_B_HOLED_MAX


def test_holed_panel_gets_the_turn_input_once_per_round():
    # the panel after Gary holes out: the turn's input once (no shot counted), the second time the Esc path; then a
    # real turn fires the only shot
    seq = [("tstate", "holed"), ("holed_target", [450, 250])] + [("over", False)] * 6
    seq += [("tstate", "holed"), ("over", False)]                       # already probed this round: Esc
    seq += [("tstate", "gary"), ("green", None), ("target", [400, 260])] + [("over", False)] * 6
    seq += [("shots_end", {"shots": 1, "rounds": 1, "virtual_s": 0})]
    c = FakeCtx(obs_schedule(seq))
    for _ in SCN._cov_b_champ_rounds(c, (390, 356), SCN._cov_b_shots2_turn, cap_s=10_000, max_shots=1):
        pass
    assert c.checkpoints == ["tournament_start", "end"]
    clicks = [i for i in c.inputs if i[0] == "click"]
    assert clicks.count(("click",) + SCN.SHOT_STRAIGHT) == 2          # the probe and the real shot
    assert [i for i in c.inputs if i[0] == "key"].count(("key", 0x1B)) == 1
    assert len(c.run.schedule["obs"]) == len(seq)
