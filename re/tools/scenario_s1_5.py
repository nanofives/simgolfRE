"""Roadmap S1.5 scenarios (log/roadmap/S1/5_notes.md, U-1032). Registered into scenario.SCENARIOS by the hook at the end
of re/tools/scenario.py; names start with cov_r so scenario_refs' s36a family runs them.

cov_r_bio: showCharacterEditor 0x004385d0's bio entry (control 20, the spot (580, 98) of the control table
f_tab_point16_4c7b38 entry 20, taken only for a golfer whose kind bits (record + 0x18) & 0xe0 are not 0x20). mood5.sve is
loaded from the title menu, a golfer's info panel is opened by a click under its name label (the record run decides from
the s36a agent's call counter, _cova_open_golfer), and '?' (mainLoop H1 key 0x3f, ML+0xe387) opens the editor on that
golfer. Bio entry 1: printable characters posted as WM_CHAR alone (EditBox::insertChar through Window::onKeyChar's focus
walk), the edit keys Back / Left / Right / Home / End / Delete as key-down / key-up pairs (EditBox::onKey), Enter as
WM_CHAR 0x0d alone (rootWin_onKey stores 0x0d into gDirtyFlagB 0x00822b9c, the loop's exit test 0x00439f0a). Bio entry 2:
characters, then a click outside the EditBox ends the loop through the button word 0x00822d68 (0x00439f02). The editor is
left with its Exit control (753, 213)."""
import scenario as SC

SCENARIOS = SC.SCENARIOS
WM_KEYDOWN, WM_KEYUP, WM_CHAR = 0x100, 0x101, 0x102
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP = 0x200, 0x201, 0x202
ROOT_WINDOW = 0x00519A60          # the root window (g_rootWindow)
EDITBOX = 0x005A9F78              # g_characterEditorEditBox
CAMERA_X, CAMERA_Y = 0x004C2BA0, 0x004C2BA4
BIO_SPOT = (580, 98)
EXIT_SPOT = (753, 213)


# IN-LOOP INPUT. The bio loop 0x00439ed8..0x00439f0a (showScreen, redrawAll, leaveModal483d80, pumpMessages, refreshFull)
# makes no timeGetTime call, so the virtual clock of scenario_det.js never reaches a poll there and the harness's own
# input actions can never be delivered inside it (probe2: the record run stopped with the main thread in that loop and T
# frozen). For cov_r_bio runs the SCENARIO AGENT itself (scenario_det.js + trace_rec.js) carries INLOOP_JS (appended by
# the agent_source wrapper below): at a poll before the bio press the lockstep hands it the message GROUPS of one bio
# entry (rpc s15Arm, which also attaches the hook, so nothing is hooked before the editor), and each time leaveModal483d80
# is entered from the loop (return address 0x00439eec: the call at 0x00439ee7, right before the loop's pumpMessages
# call at 0x00439eec) the next group goes through the agent's own doAction({k: 'post'}) on the game's main thread: the
# same path as the harness's scheduled posts, so a trace recording (trace_rec.js wraps doAction) logs a DELIVER post
# before each message and trace_format.validate's undelivered_input rule (S1.1) holds. One group per pass: posted all at
# once (probe4), the WM_CHAR messages were dispatched ahead of the key-down / key-up messages posted before them, and the
# WM_CHAR 8 that the pump's TranslateMessage makes for a Back key-down came after the Enter, so gDirtyFlagB ended at 8 and
# the loop never ended. Armed identically in every arm, so every arm gets the same messages at the same loop passes.
LEAVE_MODAL = 0x00483D80
BIO_LOOP_RA = 0x00439EEC          # the return address of `call leaveModal483d80` at 0x00439ee7
INLOOP_JS = r"""
// ---- roadmap S1.5 (re/tools/scenario_s1_5.py, IN-LOOP INPUT): cov_r_bio's bio keys, posted inside the bio loop
const S15 = { queue: [], posted: 0, passes: 0, rescue: 0, mainTid: -1, hooked: false, dlog: [] };
const S15_DIAG = %DIAG%;
function s15Hook() {
  if (S15.hooked) return;
  S15.hooked = true;
  Interceptor.attach(ptr(%LEAVE%), {
    onEnter() {
      if (this.returnAddress.toUInt32() !== %RA%) return;
      S15.mainTid = Process.getCurrentThreadId();
      S15.passes++;
      if (S15_DIAG && S15.passes === 3000) {     // diagnostics only: a loop that never ended gets a click
        S15.rescue++;
        doAction({ k: 'post', msg: 0x201, wp: 1, lp: (500 << 16) | 300, tag: 's1_5 rescue' });
        doAction({ k: 'post', msg: 0x202, wp: 0, lp: (500 << 16) | 300, tag: 's1_5 rescue' });
      }
      if (S15.queue.length === 0) return;
      const g = S15.queue.shift();
      for (const [m, w, l] of g) doAction({ k: 'post', msg: m, wp: w, lp: l, tag: 's1_5 in-loop' });
      S15.posted += g.length;
    }
  });
  if (!S15_DIAG) return;
  for (const [name, va, n] of %DIAGFNS%) {
    Interceptor.attach(ptr(va), {
      onEnter(a) {
        this.on = Process.getCurrentThreadId() === S15.mainTid && S15.dlog.length < 4000;
        if (!this.on) return;
        const args = [];
        for (let i = 0; i < n; i++) args.push(a[i].toUInt32());
        this.row = [name, this.context.ecx.toUInt32(), args, this.returnAddress.toUInt32()];
      },
      onLeave(r) { if (this.on) { this.row.push(r.toUInt32()); S15.dlog.push(this.row); } }
    });
  }
}
rpc.exports.s15Arm = function (groups) { s15Hook(); S15.queue = groups; S15.passes = 0; return groups.length; };
rpc.exports.s15Posted = function () { return [S15.posted, S15.passes, S15.rescue]; };
rpc.exports.s15Log = function () { const r = S15.dlog.slice(); S15.dlog.length = 0; return r; };
"""
# S15_DIAG=1 (probe runs only, never in a pinned recording): the key-routing functions are logged and a loop still running
# 3000 passes after its messages gets a click
DIAG_FNS = [["uiKeyEvent", 0x00495910, 2], ["uiKeyEvent148", 0x00495990, 1], ["currentFocusOwner", 0x0047F2F0, 0],
            ["Window::key", 0x0047C5D0, 2], ["Window::dispatchAccel", 0x0047C6C0, 1], ["EditBox::insertChar", 0x004866F0, 2],
            ["EditBox::onKey", 0x004867B0, 1], ["rootWin_onKey", 0x00465750, 2], ["rootWin_onChar", 0x004657B0, 1],
            ["rootWin_onPress1", 0x00465580, 2], ["Window::setFocus", 0x0047F1B0, 2], ["EditBox::create", 0x00486110, 6],
            ["Window::show", 0x0047B670, 1]]


def _inloop_js():
    import json
    import os
    diag = os.environ.get("S15_DIAG") == "1"
    return (INLOOP_JS.replace("%LEAVE%", str(LEAVE_MODAL)).replace("%RA%", str(BIO_LOOP_RA))
            .replace("%DIAG%", "true" if diag else "false").replace("%DIAGFNS%", json.dumps(DIAG_FNS)))


def _install_agent_part():
    """Wrap scenario_ab.Run.agent_source (the base every family's Run subclass calls through super()) so a cov_r_bio run's
    agent carries INLOOP_JS after scenario_det.js (and before the family's and trace_rec.js's own parts, which reach its
    doAction through the global binding at call time). Other scenarios' agents are unchanged. A no-op when scenario_ab is
    not loaded (the static tests import scenario.py alone)."""
    import sys
    S = sys.modules.get("scenario_ab")
    if S is None or getattr(S.Run, "_s15_wrapped", False):
        return
    base = next(k for k in S.Run.__mro__ if k.__module__ == "scenario_ab" and "agent_source" in k.__dict__)
    orig = base.agent_source

    def agent_source(self):
        p = orig(self)
        if getattr(self, "scenario_name", None) == "cov_r_bio":
            p.write_text(p.read_text() + "\n" + _inloop_js())
        return p
    base.agent_source = agent_source
    base._s15_wrapped = True


class _AfterScenarioAb:
    """scenario_refs imports scenario.py (and so this file) before the family runner imports scenario_ab: patch it once its
    module has run (a meta-path finder that wraps scenario_ab's loader, then removes itself)."""

    def find_spec(self, name, path, target=None):
        import importlib.machinery
        import sys
        if name != "scenario_ab":
            return None
        spec = importlib.machinery.PathFinder.find_spec(name, path)
        if spec is None or spec.loader is None:
            return spec
        inner = spec.loader

        class Loader:
            def create_module(self, s):
                return inner.create_module(s)

            def exec_module(self, m):
                inner.exec_module(m)
                _install_agent_part()
        spec.loader = Loader()
        if self in sys.meta_path:
            sys.meta_path.remove(self)
        return spec


def _install_hook():
    import sys
    if "scenario_ab" in sys.modules:
        _install_agent_part()
    elif not any(isinstance(f, _AfterScenarioAb) for f in sys.meta_path):
        sys.meta_path.insert(0, _AfterScenarioAb())


_install_hook()


def _arm_inloop(c, groups, name):
    """Queue groups of (msg, wParam, lParam) triples for the next bio loop: group k is posted at its k-th pass."""
    n = SC._cova_agent(c).s15_arm([[[m, w, l] for m, w, l in g] for g in groups])
    SC._cova_obs(c, name, lambda: n)


def _diag(c, name):
    """S15_DIAG=1 probe runs: the in-loop part's counters and call log (record run only, kept in the schedule)."""
    import os
    if os.environ.get("S15_DIAG") == "1":
        a = SC._cova_agent(c)
        SC._cova_obs(c, name, lambda: {"posted": a.s15_posted(), "log": a.s15_log()})


def _m_chars(text):       # one group per character (WM_CHAR alone)
    return [[(WM_CHAR, ord(ch), 1)] for ch in text]


def _m_vk(vk):            # one group: key-down, key-up
    return [[(WM_KEYDOWN, vk, 1), (WM_KEYUP, vk, 0xC0000001)]]


def _m_click(c, x, y):    # one group: move, button down, button up
    lp = c.lp(x, y)
    return [[(WM_MOUSEMOVE, 0, lp), (WM_LBUTTONDOWN, 1, lp), (WM_LBUTTONUP, 0, lp)]]


def _probe(c, name):
    """Record-run observation of the window-tree words the key routing reads (kept in the schedule, not an input)."""
    a = SC._cova_agent(c)
    words = {}
    for base, offs in ((ROOT_WINDOW, (0x9c, 0xa0, 0x138, 0x13c, 0x140)), (EDITBOX, (0x9c, 0xa0, 0xb0, 0x574, 0x578, 0x5a4))):
        for o in offs:
            words[f"{base + o:#x}"] = int(a.s36a_read_s32(base + o)) & 0xFFFFFFFF
    for g in (0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AB38, 0x0083AB60, 0x0083AB88, 0x0083AB00, 0x0083AAFC):
        words[f"{g:#x}"] = int(a.s36a_read_s32(g)) & 0xFFFFFFFF
    SC._cova_obs(c, name, lambda: words)


def _bio_text(c, name):
    """Record-run observation: the selected golfer's character record bio (g_golferTypes 0x004d6088 + type * 0x230 +
    0x30, the type word at golfer record + 0xb6, the golfer = [g_msgState53df54 0x0053df54] - 1)."""
    a = SC._cova_agent(c)

    def rd():
        g = (int(a.s36a_read_s32(0x0053DF54)) - 1) & 0xFFFFFFFF
        t = int(a.s36a_read_s32(0x005794B8 + (g << 8) + 0xB6)) & 0xFFFF
        va = 0x004D6088 + t * 0x230 + 0x30
        b = b"".join((int(a.s36a_read_s32(va + 4 * k)) & 0xFFFFFFFF).to_bytes(4, "little") for k in range(0x80))
        return {"golfer": g, "type": t, "bio": b.split(bytes(1))[0].decode("latin-1")}
    return SC._cova_obs(c, name, rd)


def cov_r_bio(c):
    yield from SC._cov_b_menu(c)
    yield from SC._cova_load_row(c, 2, True)                    # mood5 (row 2 of the load list)
    for _ in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("loaded")
    # the camera tile pair 0x004c2ba0 / 0x004c2ba4 back on the tile mood5.sve was saved at (scenario.view_mood5_course)
    c.write32(CAMERA_X, 28, tag="camera x = 28"); c.write32(CAMERA_Y, 15, tag="camera y = 15")
    for _ in range(3):
        yield from c.sleep(1000)
    ok = yield from SC._cova_open_golfer(c, tries=8)
    SC._cova_obs(c, "panel_ok", lambda: ok)
    yield from c.checkpoint("panel")
    yield from SC._cova_char(c, "?", settle=3000)               # the editor on the selected golfer
    yield from c.checkpoint("editor")
    _bio_text(c, "bio0")
    _probe(c, "tree0")
    _diag(c, "diag0")
    # ---- bio entry 1: typed text, the edit keys, Enter (posted inside the loop, see IN-LOOP INPUT)
    msgs = (_m_chars("Hello Golf") + _m_vk(0x08)                 # Back
            + _m_vk(0x25) + _m_vk(0x25) + _m_chars("X")          # Left x2, insert in the middle
            + _m_vk(0x24) + _m_chars("A")                        # Home, insert at the start
            + _m_vk(0x2E) + _m_vk(0x27) + _m_vk(0x23)            # Delete, Right, End
            + _m_chars("!") + _m_chars(chr(0x0D)))               # Enter: WM_CHAR 0x0d
    _arm_inloop(c, msgs, "armed1")
    yield from c.click(*BIO_SPOT); yield from c.sleep(2500)
    _bio_text(c, "bio1")
    _probe(c, "tree1")
    _diag(c, "diag1")
    yield from c.checkpoint("bio1_done")
    # ---- bio entry 2: text appended, then a click outside the EditBox ends the loop (the button word)
    _arm_inloop(c, _m_vk(0x23) + _m_chars(" Two") + _m_click(c, 300, 500), "armed2")
    yield from c.click(*BIO_SPOT); yield from c.sleep(2500)
    _bio_text(c, "bio2")
    _diag(c, "diag2")
    yield from c.checkpoint("bio2_done")
    yield from c.click(*EXIT_SPOT); yield from c.sleep(3000)     # Exit
    yield from c.checkpoint("exit")
    yield from SC._cova_esc(c, 2)
    for _ in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


SCENARIOS["cov_r_bio"] = {"env": {}, "phases": [], "lockstep": cov_r_bio}
