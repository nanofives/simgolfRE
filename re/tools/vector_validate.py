"""Phase 2.5 validation of the function-vector corpus (re/analysis/data/vector_corpus.md section 5).

    py -3.12 re/tools/vector_validate.py emu  [--function NAME ...] [--sample N]   # offline, from the .vec alone
    py -3.12 re/tools/vector_validate.py live [--function NAME ...] [--sample N] [--instance p25] [--arm reimpl|original]

emu   Each `ok` vector is replayed in Unicorn (re/tools/vector_emu.py, the ORIGINAL code from the file image) from a
      data section that is ZERO except the key's seed and the vector's `in` regions, with every fixture block placed at a
      new base and the relocation table applied. Its return value, stack pop and written bytes must equal the stored
      ret / stack_pop / out exactly (written set and values). This proves the vector is self-contained: nothing the
      function reads is missing from the file.
live  The game boots with every hook ON; each `ok` vector's seed / in / blocks are written into the live process and
      the REIMPLEMENTATION (the SG_HOOK detour) is called; its return value and the bytes of every stored out region
      must equal the stored ones. Data-section dwords it changed outside the out regions are reported (extra writes;
      the game's own concurrent writes show there too, so they are counted, not judged). `--arm original` calls the
      hook's trampoline instead (a control for the harness itself).
Reports: log/p2/p25/validate_<mode>.tsv (one row per key) and a summary on stdout.
"""
from __future__ import annotations

import argparse
import collections
import csv
import os
import pathlib
import random
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "re" / "frida"))
import vector_lib as VL  # noqa: E402
import vector_emu as EMU  # noqa: E402

OUT_DIR = pathlib.Path(os.environ.get("SIMGOLF_VEC_REPORT_DIR") or ROOT / "log" / "p2" / "p25")
VAL_BASE = 0x40000000


def vec_files(functions, sample, seed=25) -> list[pathlib.Path]:
    files = sorted(VL.VEC_DIR.glob("*.vec"))
    if functions:
        want = {f.replace("::", "_") for f in functions}
        files = [p for p in files if p.stem in want]
    if sample and sample < len(files):
        files = sorted(random.Random(seed).sample(files, sample))
    return files


def block_bases(key) -> list[int]:
    # a page-offset that differs from the capture's, so a missed relocation cannot pass by accident
    import vector_capture
    return vector_capture.packed_bases(VAL_BASE, [b["size"] for b in key["blocks"]],
                                       [(0x10 * (i + 1)) & 0xFFF for i in range(len(key["blocks"]))])


def resolve(gs: VL.GsMap, img, bases, loc: dict) -> int:
    if "null" in loc:
        return 0
    if "sym" in loc:
        return gs.va(loc["sym"], loc["off"])
    if "block" in loc:
        return bases[loc["block"]] + loc["off"]
    if "exe" in loc:
        sec = next(s for s in img.sections if s[0] == loc["exe"])
        return sec[1] + loc["off"]
    return loc["raw"]


def loc_back(gs, img, key, bases, va: int) -> dict:
    if va == 0:
        return {"null": True}
    if VL.DATA_VA <= va < VL.DATA_VA + VL.DATA_SIZE:
        s, o = gs.ref(va)
        return {"sym": s, "off": o}
    for i, b in enumerate(key["blocks"]):
        if bases[i] <= va < bases[i] + b["size"]:
            return {"block": i, "off": va - bases[i]}
    sec = img.section_of(va)
    return {"exe": sec[0], "off": sec[1]} if sec else {"raw": va}


def expected_bytes(gs, img, bases, regions) -> dict[int, int]:
    out = {}
    for r in regions:
        a = resolve(gs, img, bases, r)
        for k, b in enumerate(bytes.fromhex(r["hex"])):
            out[a + k] = b
    return out


def arg_values(gs, img, bases, key, v):
    vals = []
    for t, a in zip(key["args"], v["args"]):
        if "ptr" in a:
            vals.append(resolve(gs, img, bases, a["ptr"]))
        elif "float" in a:
            vals.append(a["float"])
        elif "int64" in a:
            vals.append(a["int64"])
        else:
            vals.append(a["int"])
    return vals


def stored_ret(v):
    r = v["ret"]
    if isinstance(r, dict) and "float_repr" in r:
        return float(r["float_repr"])
    return r


def emu_validate(files) -> list[dict]:
    gs = VL.GsMap()
    img = EMU.load_image()
    rows = []
    for p in files:
        doc = VL.read_vec(p)
        entry = int(doc["tooling_va"], 16)
        for key in doc["keys"]:
            bases = block_bases(key)
            st = collections.Counter()
            first_bad = ""
            for v in key["vectors"]:
                if v["status"] != "ok":
                    continue
                data = bytearray(VL.DATA_SIZE)
                blocks = [bytearray.fromhex(b["hex"]) for b in key["blocks"]]
                for src in (key["seed"], v.get("in", [])):
                    for a, b in expected_bytes(gs, img, bases, src).items():
                        data[a - VL.DATA_VA] = b
                for r in key.get("relocs", []):
                    nv = resolve(gs, img, bases, r["to"]).to_bytes(4, "little")
                    if "block" in r["at"]:
                        blocks[r["at"]["block"]][r["at"]["off"]:r["at"]["off"] + 4] = nv
                    else:
                        a = resolve(gs, img, bases, r["at"]) - VL.DATA_VA
                        data[a:a + 4] = nv
                res = EMU.run_call(img, bytes(data), [(bases[i], bytes(b)) for i, b in enumerate(blocks)], key["abi"],
                                   key["args"], arg_values(gs, img, bases, key, v), key["ret"], key["fpcw"], entry)
                why = None
                if res.err:
                    why = f"emu error {res.err}"
                else:
                    got = VL.norm_ret(key["ret"], res.eax, res.edx, res.st0)
                    exp = stored_ret(v)
                    if key["ret"] == "pointer":
                        got = {"ptr": loc_back(gs, img, key, bases, got)}
                    elif "ret_loc" in v:
                        got, exp = loc_back(gs, img, key, bases, got & 0xFFFFFFFF), v["ret_loc"]
                    if not VL.float_eq(got, exp) and got != exp:
                        why = f"ret {got!r} != stored {exp!r}"
                    elif res.pop != v.get("stack_pop", res.pop):
                        why = f"stack pop {res.pop} != {v['stack_pop']}"
                    else:
                        exp_out = expected_bytes(gs, img, bases, v["out"])
                        for op in v.get("out_ptrs", []):      # written pointers into blocks, at this layout's bases
                            a = resolve(gs, img, bases, op["at"])
                            for k, byte in enumerate(resolve(gs, img, bases, op["to"]).to_bytes(4, "little")):
                                exp_out[a + k] = byte
                        if set(res.mem) != set(exp_out):
                            why = f"written set differs ({len(set(res.mem) ^ set(exp_out))} bytes)"
                        elif any(res.mem[a] != b for a, b in exp_out.items()):
                            why = "written values differ"
                if why:
                    st["fail"] += 1
                    first_bad = first_bad or f"vector {v['i']}: {why}"
                else:
                    st["pass"] += 1
            rows.append({"function": doc["function"], "key": key["key"], "vectors": st["pass"] + st["fail"],
                         "pass": st["pass"], "fail": st["fail"], "first_failure": first_bad or "-"})
            print(f"emu  {doc['function']:36s} {key['key']:40s} {st['pass']}/{st['pass'] + st['fail']} {first_bad[:90]}")
    return rows


def expected_hex(region: dict, v: dict, bases: list) -> str:
    """An out region's expected bytes with every written block pointer (out_ptrs) at this process's block bases."""
    b = bytearray.fromhex(region["hex"])
    key = "sym" if "sym" in region else "block"
    for op in v.get("out_ptrs", []):
        at = op["at"]
        if key not in at or at[key] != region[key]:
            continue
        rel = at["off"] - region["off"]
        if 0 <= rel and rel + 4 <= len(b):
            b[rel:rel + 4] = (bases[op["to"]["block"]] + op["to"]["off"]).to_bytes(4, "little")
    return b.hex()


def live_validate(files, arm: str, instance: str) -> list[dict]:
    os.environ.setdefault("SIMGOLF_ALLOW_MULTI", "1")
    os.environ.setdefault("SIMGOLF_GAME_DIR", str(ROOT / "instances" / instance))
    from game import Game, JS_DIR
    gs = VL.GsMap()
    img = EMU.load_image()
    nores = [[off, size] for off, size, sym, cls, ptr in gs.items if cls == "pointer"]
    src = "\n".join((JS_DIR / n).read_text() for n in ("keepalive.js", "freeze.js", "vector_replay.js"))
    work = []
    for p in files:
        doc = VL.read_vec(p)
        for key in doc["keys"]:
            vs = [v for v in key["vectors"] if v["status"] == "ok"]
            if vs:
                work.append((doc, key, vs))
    rows = []
    queue = [(w, 0) for w in work]
    while queue:
        with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
            g.wait_window()
            time.sleep(4.0)                    # past sound.dll loading: early keys lost the game now and then
            sc = g.session.create_script(src)
            sc.load()
            while queue:
                (doc, key, vs), tries = queue.pop(0)
                bases = [0] * len(key["blocks"])            # live addresses are the script's; data VAs only here
                spec = {"addr": int(doc["tooling_va"], 16), "abi": key["abi"], "ret": key["ret"], "args": key["args"],
                        "arm": arm, "freeze": True, "noRestore": nores,
                        "blocks": [b["hex"] for b in key["blocks"]],
                        "relocs": [{"at": ({"block": r["at"]["block"], "off": r["at"]["off"]} if "block" in r["at"]
                                           else {"va": gs.va(r["at"]["sym"], r["at"]["off"])}), "to": r["to"]}
                                   for r in key.get("relocs", [])],
                        "seed": [[resolve(gs, img, bases, r), r["hex"]] for r in key["seed"]], "vectors": []}
                for v in vs:
                    args = []
                    for t, a in zip(key["args"], v["args"]):
                        if "ptr" in a:
                            l = a["ptr"]
                            args.append({"block": l["block"], "off": l["off"]} if "block" in l else {"va": resolve(gs, img, bases, l)})
                        elif "int64" in a:
                            args.append({"int64": str(a["int64"])})
                        else:
                            args.append({"n": a.get("int", a.get("float"))})
                    out = []
                    for r in v["out"]:
                        n = len(r["hex"]) // 2
                        out.append({"block": r["block"], "off": r["off"], "n": n} if "block" in r
                                   else {"va": resolve(gs, img, bases, r), "n": n})
                    spec["vectors"].append({"args": args, "in": [[resolve(gs, img, bases, r), r["hex"]] for r in v.get("in", [])],
                                            "out": out})
                try:
                    res = sc.exports_sync.replay(spec)
                except Exception as e:      # noqa: BLE001
                    time.sleep(0.5)
                    if not g.alive and tries == 0:
                        # the game died under this key: run it once more in a fresh boot before recording an error
                        print(f"live {doc['function']:36s} {key['key']:40s} game ended, retrying in a fresh boot")
                        queue.insert(0, ((doc, key, vs), 1))
                        break
                    rows.append({"function": doc["function"], "key": key["key"], "vectors": len(vs), "pass": 0,
                                 "fail": len(vs), "extra_write_vectors": 0, "witness": "-",
                                 "first_failure": f"ERROR {type(e).__name__}: {str(e)[:150]}"})
                    print(f"live {doc['function']:36s} {key['key']:40s} ERROR {str(e)[:120]}")
                    if not g.alive:
                        break
                    continue
                st = collections.Counter()
                first_bad = ""
                extra = 0
                for v, r in zip(vs, res["rows"]):
                    why = None
                    exp = stored_ret(v)
                    got = r["ret"]
                    if r["err"]:
                        why = f"error {r['err'][:80]}"
                    else:
                        if key["ret"] == "pointer":
                            got = {"ptr": {"block": got["block"], "off": got["off"]} if "block" in got
                                   else loc_back(gs, img, key, [-1] * len(key["blocks"]), got["va"])}
                        elif key["ret"] in ("double", "float"):
                            got = float(got) if got is not None else None
                        elif key["ret"] in ("int64", "uint64"):
                            got = int(got)
                        elif key["ret"] != "void":
                            got = VL.norm_ret(key["ret"], int(got) & 0xFFFFFFFF) if key["ret"] != "pointer" else got
                            if "ret_loc" in v:
                                u = int(got) & 0xFFFFFFFF
                                bi = next((i for i, b in enumerate(res["bases"]) if b <= u < b + key["blocks"][i]["size"]), None)
                                got = {"block": bi, "off": u - res["bases"][bi]} if bi is not None else {"raw": u}
                                exp = v["ret_loc"]
                        if not VL.float_eq(got, exp) and got != exp:
                            why = f"ret {got!r} != stored {exp!r}"
                        else:
                            bad = [o for o, h in zip(v["out"], r["out"]) if expected_hex(o, v, res["bases"]) != h]
                            if bad:
                                why = f"out region {bad[0].get('sym', '#%s' % bad[0].get('block'))}+{bad[0]['off']} differs"
                        exp_dw = set()
                        for o in v["out"]:
                            if "sym" in o:
                                a = gs.va(o["sym"], o["off"])
                                exp_dw |= {(a + k) & ~3 for k in range(len(o["hex"]) // 2)}
                        if any(c not in exp_dw for c in r["changed"]):
                            extra += 1
                    if why:
                        st["fail"] += 1
                        first_bad = first_bad or f"vector {v['i']}: {why}"
                    else:
                        st["pass"] += 1
                rows.append({"function": doc["function"], "key": key["key"], "vectors": len(vs), "pass": st["pass"],
                             "fail": st["fail"], "extra_write_vectors": extra, "witness": f"0x{res['witness']:02x}",
                             "first_failure": first_bad or "-"})
                print(f"live {doc['function']:36s} {key['key']:40s} {st['pass']}/{len(vs)} extra-writes {extra} "
                      f"witness 0x{res['witness']:02x} {first_bad[:80]}")
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["emu", "live"])
    ap.add_argument("--function", nargs="*")
    ap.add_argument("--sample", type=int)
    ap.add_argument("--instance", default="p25")
    ap.add_argument("--arm", default="reimpl", choices=["reimpl", "original"])
    ap.add_argument("--tag", default="")
    ap.add_argument("--jobs", type=int, default=1, help="emu: worker processes")
    a = ap.parse_args()
    files = vec_files(a.function, a.sample)
    if a.mode == "emu" and a.jobs > 1:
        import concurrent.futures as cf
        rows = []
        with cf.ProcessPoolExecutor(a.jobs) as ex:
            for rr in ex.map(emu_validate, [[f] for f in files]):
                rows += rr
    else:
        rows = emu_validate(files) if a.mode == "emu" else live_validate(files, a.arm, a.instance)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    name = f"validate_{a.mode}{'_' + a.arm if a.mode == 'live' and a.arm != 'reimpl' else ''}{a.tag}.tsv"
    if rows:
        with (OUT_DIR / name).open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter="\t", lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
    fns = collections.defaultdict(lambda: [0, 0])
    for r in rows:
        fns[r["function"]][0] += r["pass"]
        fns[r["function"]][1] += r["fail"]
    clean = sum(1 for p, f in fns.values() if f == 0 and p > 0)
    print(f"{a.mode}: {len(fns)} functions, {clean} with every vector reproduced, "
          f"{sum(p for p, _ in fns.values())} vectors pass, {sum(f for _, f in fns.values())} fail -> {OUT_DIR / name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
