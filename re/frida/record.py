"""Record real calls of a registered function during a canonical scenario, for offline replay.

    py -3.12 re/frida/record.py TerrainDll_tileAt [--max 2000] [--every 7] [--seconds 20]

Boots the game with THAT hook OFF (the original runs live), drives the sandbox scenario, and records
up to --max calls (every --every-th call, to spread samples over the session). Writes a bundle:

  re/replay/<name>/meta.json       module bases/sizes, target, detour, live install witness
  re/replay/<name>/img/<module>.bin  module images (target module + shim) as mapped at record time
  re/replay/<name>/pages/<sha1>    deduplicated 4 KiB pages around pointer inputs
  re/replay/<name>/calls.jsonl     ecx, edx, stack args, live return value, page list

The install witness is taken in a second short boot with the hook ON (byte at the address == 0xE9).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from game import Game, JS_DIR  # noqa: E402
import imgcmp  # noqa: E402
from hooks_registry import HOOKS  # noqa: E402

GOLDEN = ROOT / "tests" / "golden"
SHIM = "winmm.dll"


def scenario_sandbox(g: Game, seconds: float):
    imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=60)
    g.click(130, 445)
    imgcmp.wait_for(g, imgcmp.load(GOLDEN / "location.png"), timeout=15)
    g.click(270, 32)  # Monterey
    time.sleep(seconds)


def witness(spec) -> dict:
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
        g.wait_window()
        sc = g.session.create_script("""
          rpc.exports.w = (mod, addr) => {
            const m = Process.getModuleByName(mod);
            const t = mod.toLowerCase() === 'golf_clean.exe' ? ptr(addr) : m.base.add(addr);
            const s = Process.getModuleByName('winmm.dll');
            const find = new NativeFunction(s.getExportByName('SimGolfShim_FindHook'), 'int', ['uint32','pointer','pointer','pointer'], 'stdcall');
            const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
            const found = find(addr, pd, po, pi);
            return { found, installed: pi.readS32(), byte: t.readU8() };
          };""")
        sc.load()
        return sc.exports_sync.w(spec["module"], spec["addr"])


def record(name: str, max_calls: int, every: int, seconds: float) -> pathlib.Path:
    spec = HOOKS[name]
    out = ROOT / "re" / "replay" / name
    if out.exists():
        shutil.rmtree(out)
    (out / "img").mkdir(parents=True)
    (out / "pages").mkdir()
    calls = []

    def on_msg(m, data):
        if m.get("type") == "send" and m["payload"].get("type") == "call":
            rec = m["payload"]["rec"]
            hashes = []
            for i, a in enumerate(rec.pop("pageAddrs")):
                chunk = data[i * 0x1000:(i + 1) * 0x1000]
                h = hashlib.sha1(chunk).hexdigest()
                p = out / "pages" / h
                if not p.exists():
                    p.write_bytes(chunk)
                hashes.append([a, h])
            rec["pages"] = hashes
            calls.append(rec)

    env = {"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_HOOKS_OFF": f"{spec['addr']:08x}"}
    with Game(env=env) as g:
        g.wait_window()
        sc = g.session.create_script((JS_DIR / "record.js").read_text())
        sc.on("message", on_msg)
        sc.load()
        # Dump module images BEFORE attaching: afterwards the target's first bytes are Frida's own
        # inline hook, and the "original" replayed offline would jump into Frida (2026-10-02 RED run).
        images = {}
        for mod in dict.fromkeys([spec["module"], SHIM]):
            mi = sc.exports_sync.dump_module(mod)
            (out / "img" / f"{mod}.bin").write_bytes(sc.exports_sync.read_range(mi["base"], mi["size"]))
            images[mod] = mi
        info = sc.exports_sync.start({"module": spec["module"], "addr": spec["addr"], "nargs": spec["nargs"],
                                      "max": max_calls, "every": every})
        assert info["firstByte"] != 0xE9, "hook is ON during recording; the original must run"
        # detour address from the shim (registered even while toggled OFF)
        det = g.session.create_script("""
          rpc.exports.d = (addr) => {
            const s = Process.getModuleByName('winmm.dll');
            const find = new NativeFunction(s.getExportByName('SimGolfShim_FindHook'), 'int', ['uint32','pointer','pointer','pointer'], 'stdcall');
            const pd = Memory.alloc(4);
            if (!find(addr, pd, NULL, NULL)) throw new Error('no SG_HOOK');
            return pd.readPointer().toUInt32();
          };""")
        det.load()
        detour = det.exports_sync.d(spec["addr"])
        scenario_sandbox(g, seconds)
        stats = sc.exports_sync.stats()
        time.sleep(0.5)
    w = witness(spec)
    meta = {"name": name, "module": spec["module"], "addr": spec["addr"], "target": info["target"],
            "detour": detour, "abi": spec["abi"], "nargs": spec["nargs"], "images": images,
            "calls_seen": stats["calls"], "calls_recorded": len(calls), "every": every,
            "scenario": f"sandbox_monterey_{seconds:g}s",
            "install_witness": {"byte": w["byte"], "installed": w["installed"], "ok": w["byte"] == 0xE9 and w["installed"] == 1},
            "recorded_at": time.strftime("%Y-%m-%dT%H:%M:%S")}
    (out / "meta.json").write_text(json.dumps(meta, indent=2))
    with (out / "calls.jsonl").open("w") as f:
        for c in calls:
            f.write(json.dumps(c) + "\n")
    npages = len(list((out / "pages").iterdir()))
    print(f"{name}: {stats['calls']} live calls seen, {len(calls)} recorded, {npages} unique pages, "
          f"witness 0x{w['byte']:02x} installed={w['installed']} -> {out.relative_to(ROOT)}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--max", type=int, default=2000)
    ap.add_argument("--every", type=int, default=7)
    ap.add_argument("--seconds", type=float, default=20)
    a = ap.parse_args()
    record(a.name, a.max, a.every, a.seconds)


if __name__ == "__main__":
    main()
