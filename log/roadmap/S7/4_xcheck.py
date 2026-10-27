"""Roadmap S7.4 cross-check: one app recording's input replayed into the ORIGINAL (notes log/roadmap/S7/4_notes.md section 4).

The app's recording (port/corpus/app/<name>) is shaped for the port's replay (sg_run_boot): its clock records are the port's
reads, which are fewer than the original's main-thread timeGetTime calls, so re/tools/trace_tool.py replay cannot take it as
a plan (its plan is one value per original clock call). The original is driven instead by the app's RAW INPUT at the pump
calls that took it in the app:

  - posted.json: every message the driver posted (the app's own TranslateMessage characters are not in it: the original's
    pumps run TranslateMessage too and make their own); input_dispatch.tsv: each message's seq, arrival frame and dispatch;
    dispatches.tsv: each dispatch's live frame and body pump call (body_call 0 = P6's pumpInput483bd0, the call at
    0x004199e0, return 0x004199e5; 1 = H1's pumpMessages, the call at 0x0041c0cd, return 0x0041c0d2).
  - in-game messages (live frame >= 1) are posted by the agent on the game's main thread at the entry of that pump call in
    that frame (frame = scenario_det.js's frame-head count, FH.ctl + 16, the same count as the app's live frame); a message
    with no command (a mouse move, the wheel) at P6 of the frame after it arrived. Mouse buttons and keys set the shim's
    virtual key state (SimGolfShim_SetVirtualKey: down 2 / up 1) when posted, as the harness's clicks and keys do and as
    the app's --keys message table does when the window receives them. The wheel's lParam is mapped to the original
    window's screen point (ClientToScreen of the client point the driver posted at).
  - the cursor the game reads in frame f (jgld's ScreenToClient slot, RVA 0x12c618, the slot trace_rec.js wraps for its
    CURSOR records) is the app's frame-f CURSOR value (the k-th read gets the app's k-th read of that frame, the last one
    after that); in the boot (frame 0: title, location screen) the harness's virtual cursor follows the posted moves.
  - frame 0 (title menu, location screen) is driven by the lockstep: each group of messages ending at a button release is
    posted when its screen is up (the golden main_menu / location screens), with the harness's click timing.

Run (instance s7_4, C3FT_AUDIO=none, C3FT_FRAMEHASH=sim; scenario cov_r_s74x of the s36a family; run dir
log/c3/alt/refs/cov_r_s74x/<label>):
    ~/bin/py -3.12 log/roadmap/S7/4_xcheck.py run --app hole --label x1
    ~/bin/py -3.12 log/roadmap/S7/4_xcheck.py compare --app hole --label x1     # per-record sim hashes, first divergence
    ~/bin/py -3.12 log/roadmap/S7/4_xcheck.py plan --app hole                   # the plan only (no game)
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
APPS = ROOT / "port" / "corpus" / "app"
SCEN = "cov_r_s74x"
P6_RA, H1_RA = 0x004199E5, 0x0041C0D2
PUMP_P6, PUMP_H1 = 0x00483BD0, 0x0045C030      # pumpInput483bd0, pumpMessages (re/names)
BUTTON_VK = {0x201: (1, 2), 0x202: (1, 1), 0x203: (1, 2), 0x204: (2, 2), 0x205: (2, 1), 0x206: (2, 2)}
KEY_DOWN, KEY_UP = (0x100, 0x104), (0x101, 0x105)


def _tsv(p: pathlib.Path) -> list[dict]:
    lines = p.read_text().splitlines()
    h = lines[0].split("\t")
    return [dict(zip(h, l.split("\t"))) for l in lines[1:] if l.strip()]


def build_plan(app: str) -> dict:
    import trace_format as TF
    d = APPS / app
    posted = json.loads((d / "posted.json").read_text())
    ev = {int(r["seq"]): r for r in _tsv(d / "input_dispatch.tsv")}
    disp = {int(r["index"]): r for r in _tsv(d / "dispatches.tsv")}
    slots: dict[str, list] = {}
    boot: list[list] = [[]]
    problems = []
    for p in posted:
        e = ev.get(p["seq"])
        if e is None:
            problems.append(f"posted seq {p['seq']} {p['tag']} not in input_dispatch.tsv")
            continue
        m = [p["msg"], p["wp"], p["lp"], p["tag"]]
        if p["msg"] == 0x20A:                      # the wheel: the client point the driver posted at (tag "wheel x y d")
            _w, x, y, _dl = p["tag"].split()[:4]
            m.append([int(x), int(y)])
        di = int(e["dispatch"])
        if di >= 0:
            dr = disp[di]
            f, body, bc = int(dr["frame"]), int(dr["body"]), int(dr["body_call"])
            if f == 0:
                boot[-1].append(m)
                if p["msg"] in (0x202, 0x205):
                    boot.append([])
                continue
            if not body:
                problems.append(f"seq {p['seq']} {p['tag']}: a modal dispatch in live frame {f} (not driven)")
                continue
            slots.setdefault(f"{f}:{bc}", []).append(m)
        else:
            f = int(e["arrival_frame"])
            if f == 0:
                boot[-1].append(m)
            else:
                slots.setdefault(f"{f + 1}:0", []).append(m)
    boot = [g for g in boot if g]
    # the app's cursor answers per live frame (frame n = after the n-th FRAME record)
    tr = TF.read(d / "trace.trace")
    cur: dict[int, list] = {}
    n = 0
    for r in tr.records:
        if r.name == "FRAME":
            n += 1
        elif r.name == "CURSOR" and n >= 1:
            cur.setdefault(n, []).append([r.f["x"], r.f["y"]])
    records = len((d / "framehash.bin").read_bytes()) // (8 + 8 * 203)
    return {"app": app, "boot": boot, "slots": slots, "cursor": {str(k): v for k, v in cur.items()}, "records": records,
            "problems": problems}


AGENT_JS = r"""
// ---- roadmap S7.4 cross-check (log/roadmap/S7/4_xcheck.py): the app's input at the pump calls that took it in the app
const S74 = { slots: {}, cursor: {}, posted: 0, missed: [], reads: {}, rf: -1, rk: 0, log: [], armed: false };
const S74_U32 = new NativeFunction(Process.getModuleByName('user32.dll').getExportByName('ClientToScreen'), 'int',
                                   ['pointer', 'pointer'], 'stdcall');
function s74Frame() { return FH ? FH.ctl.add(16).readU32() : -1; }
function s74PostSlot(key) {
  const g = S74.slots[key];
  if (!g) return;
  delete S74.slots[key];
  const s = shim();
  for (const m of g) {
    const msg = m[0] >>> 0;
    let lp = m[2] >>> 0;
    if (msg === 0x20a) {
      const pt = Memory.alloc(8);
      pt.writeS32(m[4][0]); pt.add(4).writeS32(m[4][1]);
      S74_U32(HWND, pt);
      lp = (((pt.add(4).readS32() & 0xffff) << 16) | (pt.readS32() & 0xffff)) >>> 0;
    }
    const bv = %BUTTON_VK%[msg];
    if (bv) s.vkey(bv[0], bv[1]);
    if (msg === 0x100 || msg === 0x104) s.vkey(m[1] & 0xff, 2);
    if (msg === 0x101 || msg === 0x105) s.vkey(m[1] & 0xff, 1);
    doAction({ k: 'post', msg: msg, wp: m[1] >>> 0, lp: lp, tag: 's74 ' + m[3] });
    S74.posted++;
    S74.log.push([key, s74Frame(), msg, m[3]]);
  }
}
function s74Arm() {
  if (S74.armed) return;
  S74.armed = true;
  Interceptor.attach(ptr(%PUMP_P6%), { onEnter() {
    if (Process.getCurrentThreadId() !== MAIN || this.returnAddress.toUInt32() !== %P6_RA%) return;
    s74PostSlot(s74Frame() + ':0');
  } });
  Interceptor.attach(ptr(%PUMP_H1%), { onEnter() {
    if (Process.getCurrentThreadId() !== MAIN || this.returnAddress.toUInt32() !== %H1_RA%) return;
    s74PostSlot(s74Frame() + ':1');
  } });
}
function s74HookJgld(m) {
  const slot = m.base.add(0x12c618);
  const orig = new NativeFunction(slot.readPointer(), 'int', ['pointer', 'pointer'], 'stdcall');
  const cb = K(new NativeCallback(function (h, pt) {
    const r = orig(h, pt);
    if (Process.getCurrentThreadId() === MAIN && !pt.isNull() && S74.armed) {
      const f = s74Frame();
      if (f >= 1) {
        if (f !== S74.rf) { S74.rf = f; S74.rk = 0; }
        const v = S74.cursor[f];
        S74.reads[f] = (S74.reads[f] || 0) + 1;
        if (v && v.length) {
          const c = v[Math.min(S74.rk, v.length - 1)];
          pt.writeS32(c[0]); pt.add(4).writeS32(c[1]);
        }
        S74.rk++;
      }
    }
    return r;
  }, 'int', ['pointer', 'pointer'], 'stdcall'));
  Memory.protect(slot, 4, 'rw-');
  slot.writePointer(cb);
}
{
  const j = Process.findModuleByName('jgld.dll');
  if (j) s74HookJgld(j);
  else Process.attachModuleObserver({ onAdded(m) { if (m.name.toLowerCase() === 'jgld.dll') s74HookJgld(m); } });
}
rpc.exports.s74Arm = function (slots, cursor) { S74.slots = slots; S74.cursor = cursor; s74Arm(); return Object.keys(slots).length; };
rpc.exports.s74Stat = function () {
  return { frames: s74Frame(), posted: S74.posted, left: Object.keys(S74.slots).length, log: S74.log.slice(-400),
           reads: S74.reads };
};
"""


def agent_js() -> str:
    return (AGENT_JS.replace("%BUTTON_VK%", json.dumps({str(k): list(v) for k, v in BUTTON_VK.items()}))
            .replace("%PUMP_P6%", str(PUMP_P6)).replace("%PUMP_H1%", str(PUMP_H1))
            .replace("%P6_RA%", str(P6_RA)).replace("%H1_RA%", str(H1_RA)))


def register(plan: dict):
    """the lockstep scenario: frame 0's groups through the harness, the rest through the agent part"""
    import imgcmp
    import scenario as SC
    golden = ROOT / "tests" / "golden"

    def post_group(c, g):
        for msg, wp, lp, tag, *_ in g:
            x, y = lp & 0xFFFF, (lp >> 16) & 0xFFFF
            if msg in (0x200, 0x201, 0x202, 0x204, 0x205):
                c.act(0, k="cursor", x=x, y=y, tag=f"s74 cursor {x},{y}")
            bv = BUTTON_VK.get(msg)
            if bv:
                c.act(0, k="vkey", vk=bv[0], st=bv[1])
            c.act(0, k="post", msg=msg, wp=wp, lp=lp, tag="s74 " + tag)
            yield from c.sleep(500 if msg == 0x200 else 150)

    def lockstep(c):
        a = SC._cova_agent(c)
        yield from SC._cov_b_menu(c)
        screens = ["main_menu", "location"]
        for i, g in enumerate(plan["boot"]):
            if i > 0 and i < len(screens):
                yield from c.wait_screen(imgcmp.load(golden / f"{screens[i]}.png"), name=screens[i])
                yield from c.sleep(1000)
            if i == len(plan["boot"]) - 1:
                n = a.s74_arm(plan["slots"], plan["cursor"])
                SC._cova_obs(c, "armed", lambda: n)
            yield from post_group(c, g)
        goal = plan["records"] + 2
        while True:
            st = a.s74_stat()
            if st["frames"] >= goal:
                break
            yield from c.sleep(1000)
        SC._cova_obs(c, "stat", lambda: {k: v for k, v in a.s74_stat().items() if k != "log"})
        yield from c.checkpoint("end")

    SC.SCENARIOS[SCEN] = {"env": {}, "phases": [], "lockstep": lockstep}


def mutate(plan: dict, what: str) -> dict:
    """negative controls of the cross-check: `tee` moves the tee click's x 300 -> 364 (the app's own control,
    test_app_replay.py test_control_input_byte_changed); `drop_green` leaves the green click out"""
    for key in list(plan["slots"]):
        g = plan["slots"][key]
        for m in g:
            if what == "tee" and m[3].startswith("tee 300 200"):
                m[2] = (m[2] & 0xFFFF0000) | 364
        if what == "drop_green":
            plan["slots"][key] = [m for m in g if not m[3].startswith("green 520 330")]
    plan["mutation"] = what
    return plan


def run(app: str, label: str, instance: str, trace: bool = False, mutation: str | None = None) -> int:
    plan = build_plan(app)
    if mutation:
        plan = mutate(plan, mutation)
    if plan["problems"]:
        print("plan problems:", plan["problems"])
    os.environ.setdefault("C3FT_FRAMEHASH", "sim")
    os.environ.setdefault("C3FT_AUDIO", "none")
    sys.path.insert(0, str(ROOT / "re" / "frida"))
    register(plan)
    import scenario_refs as R
    orig_setup = R.setup
    js = agent_js()
    box = {}
    tstate: dict = {}

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        Base = S.Run

        class XRun(Base):
            def agent_source(self):
                p = super().agent_source()
                p.write_text(p.read_text() + "\n" + js)
                return p

            def _on_message(self, m, data):
                if m.get("type") == "error":
                    print("agent error:", m, flush=True)
                return super()._on_message(m, data)
        S.Run = XRun
        if trace:  # trace_rec.js after this part: its cursor wrap calls ours, so it logs what the game was given
            import trace_tool as TT
            TT.install(S, SCEN, "record", tstate)
        box["S"] = S
        return S

    R.setup = setup
    a = argparse.Namespace(instance=instance, scenario=SCEN, label=label, schedule=None, record=True, on=None, env=None,
                           stop_after=0, clock_log=0, msg_log=0, trace=None, watch=None)
    rc = R.cmd_run(a)
    out = R.REFS / SCEN / label
    (out / "s74_plan.json").write_text(json.dumps(plan, indent=0))
    if trace:
        import trace_tool as TT
        S = box["S"]
        tstate["meta"] = TT.trace_meta(S, SCEN)
        tstate["clock"] = {"step": S.STEP_MS, "t0": S.T0, "qstep": max(S.qpc_freq() // 1000, 1) * S.STEP_MS, "q0": 0x10000000}
        data = TT.write_trace(tstate, out / "orig.trace")
        print("original trace:", None if data is None else len(data), "bytes")
    try:
        import game
        game.purge_frida_temp()
    except Exception as e:  # noqa: BLE001
        print("purge_frida_temp:", e)
    return rc


# sim objects that hold a timeGetTime VALUE: the app answers real ms from --clock-origin, the original's virtual clock 1 ms per
# call, so they differ by construction (g_frameMs: H1's timeGetTime read at mainLoop + 0xf07f stored, never read back:
# re/analysis/sim/time_model.md)
CLOCK_VALUED = ("g_frameMs",)


def compare(app: str, label: str, mask=CLOCK_VALUED) -> dict:
    import scenario_refs as R
    d = APPS / app
    names = json.loads((d / "framehash.json").read_text())["symbols"]
    n = len(names)
    out = R.REFS / SCEN / label
    oj = json.loads((out / "framehash.json").read_text()) if (out / "framehash.json").exists() else {}
    onames = oj.get("symbols", names)
    if onames != names:
        print("object lists differ:", len(onames), len(names))

    def recs(p):
        raw = p.read_bytes()
        step = 8 + 8 * n
        return [(struct.unpack_from("<II", raw, o), struct.unpack_from(f"<{n}Q", raw, o + 8)) for o in range(0, len(raw), step)]
    A, O = recs(d / "framehash.bin"), recs(out / "framehash.bin")
    first = None
    same = 0
    for k in range(min(len(A), len(O))):
        bad = [names[i] for i in range(n) if A[k][1][i] != O[k][1][i] and names[i] not in mask]
        if bad:
            first = {"record": k, "objects": bad, "app_T": A[k][0][0], "orig_T": O[k][0][0]}
            break
        same += 1
    diff_counts: dict[str, int] = {}
    for k in range(min(len(A), len(O))):
        for i in range(n):
            if A[k][1][i] != O[k][1][i]:
                diff_counts[names[i]] = diff_counts.get(names[i], 0) + 1
    res = {"app": app, "label": label, "masked": list(mask), "records_differing_per_object": diff_counts, "app_records": len(A), "orig_records": len(O), "same_records": same, "first": first}
    print(json.dumps(res, indent=1))
    return res


def handler_seq(path: pathlib.Path) -> list[tuple]:
    """(live frame, handler, a0, a1) of every HANDLER record, and the MSG record each follows, in order"""
    import trace_format as TF
    tr = TF.read(path)
    out, n, last_msg = [], 0, None
    for r in tr.records:
        if r.name == "FRAME":
            n += 1
        elif r.name == "MSG":
            last_msg = r.f["msg"]
        elif r.name == "HANDLER":
            out.append((n, r.f["handler"], r.f["a0"], r.f["a1"], last_msg))
    return out


def handlers(app: str, label: str) -> dict:
    """the root-handler calls the app made against the ones the original made (trace_rec.js), per live frame; the app's
    trace has no MSG records (the live recorder writes the handler calls), so the message column is the original's"""
    import scenario_refs as R
    A = handler_seq(APPS / app / "trace.trace")
    O = handler_seq(R.REFS / SCEN / label / "orig.trace")
    first = None
    for k in range(min(len(A), len(O))):
        if A[k][:4] != O[k][:4]:
            first = {"index": k, "app": A[k][:4], "orig": O[k], "app_next": A[k:k + 4], "orig_next": O[k:k + 4]}
            break
    by_msg: dict[str, int] = {}
    for k in range(min(len(A), len(O))):
        if A[k][:4] == O[k][:4]:
            by_msg[hex(O[k][4] or 0)] = by_msg.get(hex(O[k][4] or 0), 0) + 1
    res = {"app_handlers": len(A), "orig_handlers": len(O), "first": first, "equal_by_original_msg": by_msg}
    print(json.dumps(res, indent=1, default=str))
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    for nm in ("run", "compare", "plan", "handlers"):
        p = sp.add_parser(nm)
        p.add_argument("--app", default="hole")
        if nm != "plan":
            p.add_argument("--label", default="x1")
        if nm == "run":
            p.add_argument("--instance", default="s7_4")
            p.add_argument("--trace", action="store_true", help="also record the original's trace (trace_rec.js)")
            p.add_argument("--mutate", choices=["tee", "drop_green"])
    a = ap.parse_args()
    if a.cmd == "plan":
        pl = build_plan(a.app)
        print(json.dumps({k: (v if k != "cursor" else f"{len(v)} frames") for k, v in pl.items()}, indent=1)[:6000])
        return 0
    if a.cmd == "run":
        return run(a.app, a.label, a.instance, a.trace, a.mutate)
    if a.cmd == "handlers":
        handlers(a.app, a.label)
        return 0
    compare(a.app, a.label)
    return 0


if __name__ == "__main__":
    sys.exit(main())
