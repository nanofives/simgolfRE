"""Phase 2.5 function-vector corpus: CAPTURE the original's behaviour on every registry vector, then BUILD .vec files.

    py -3.12 re/tools/vector_capture.py capture [--instance p25] [--batch c3q ...] [--key K ...] [--all-selected]
    py -3.12 re/tools/vector_capture.py build   [--function NAME ...]       # raw captures -> .vec + manifest
    py -3.12 re/tools/vector_capture.py status

CAPTURE boots the game with SIMGOLF_HOOKS_OFF=all (every address is the original code, callees included), loads the
same fixture files as re/frida/diff_hook.py and re/frida/js/vector_capture.js in place of diff_hook.js, and per registry
key records: the data-section image right after the fixture, the fixture's memory blocks (every __keepAlloc block, plus
the pages behind pointers that are neither data, module nor block), the stand-ins the fixture used, the FPU control word
of the calling thread, and per vector the resolved arguments, the live return value, the data-section drift since the
previous vector and the exact dword / byte changes the call made. Each vector's changes are undone before the next (the
pointer-class objects of the model excepted). One boot per batch (CLAUDE.md: many batches' fixtures in one boot flake);
a key that kills or hangs the game is recorded and the next key gets a fresh boot. Raw captures:
port/corpus/vectors/raw/<key file>.raw.gz (local only).

BUILD replays every captured vector in Unicorn from the file image of golf_clean.exe (re/tools/vector_emu.py) with the
captured memory, keeps the vectors whose emulated return value and writes agree with the live ones, derives from the
emulation the bytes each vector READ (inputs) and WROTE (outputs), proves the fixture blocks' relocations by a second
emulation with every block moved, and writes port/corpus/vectors/<function>.vec (schema re/model/vector_schema.md) and
the committed index re/port/vectors_manifest.tsv.
"""
from __future__ import annotations

import argparse
import collections
import csv
import gzip
import hashlib
import json
import os
import pathlib
import struct
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "re" / "frida"))
import vector_lib as VL  # noqa: E402

RAW_DIR = VL.VEC_DIR / "raw"
SEL = ROOT / "re" / "port" / "vector_selection.tsv"
MANIFEST = pathlib.Path(os.environ.get("SIMGOLF_VEC_MANIFEST") or ROOT / "re" / "port" / "vectors_manifest.tsv")
REPORT_DIR = ROOT / "log" / "p2" / "p25"
KEY_TIMEOUT = 180.0
# the files diff_hook.py loads before the fixtures, in its order (tests/test_vector_corpus.py checks it against diff_hook.py)
PRE_PARTS = ["keepalive.js", "heapstandin.js", "importstandin.js", "filestandin.js", "mmiostandin.js", "freeze.js",
             "recdisplay.js", "uitree.js", "modal.js", "framemask.js", "diff_fixtures.js"]


def key_file(key: str) -> str:
    return key.replace("::", "_").replace("<", "(").replace(">", ")").replace("*", "P").replace(" ", "_")


# --------------------------------------------------------------------------------------------- raw container
def write_raw(path: pathlib.Path, meta: dict, blob: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    j = json.dumps(meta, separators=(",", ":")).encode()
    tmp = path.with_suffix(".tmp")
    with gzip.GzipFile(tmp, "wb", compresslevel=4, mtime=0) as f:
        f.write(struct.pack("<I", len(j)))
        f.write(j)
        f.write(blob)
    tmp.replace(path)


def read_raw(path: pathlib.Path) -> tuple[dict, bytes]:
    with gzip.open(path, "rb") as f:
        n = struct.unpack("<I", f.read(4))[0]
        meta = json.loads(f.read(n))
        blob = f.read()
    return meta, blob


# --------------------------------------------------------------------------------------------- selection
def selected_keys() -> list[tuple[str, str]]:
    """[(key, batch)] for every selected function, in selection order."""
    import vector_select
    kb = vector_select.key_batches()
    out = []
    for r in csv.DictReader(SEL.open(), delimiter="\t"):
        if r["selected"] == "1":
            for k in r["key_list"].split(" "):
                out.append((k, kb[k]))
    return out


def no_restore_ranges(gs: VL.GsMap) -> list[list[int]]:
    return [[off, size] for off, size, sym, cls, ptr in gs.items if cls == "pointer"]


# --------------------------------------------------------------------------------------------- capture
def script_source(js_dir: pathlib.Path) -> str:
    from hooks_registry import enabled_batches
    frags = [p for p in sorted((js_dir / "fixtures.d").glob("*.js")) if p.stem in enabled_batches()]
    parts = [js_dir / n for n in PRE_PARTS] + frags + [js_dir / "vector_capture.js"]
    # as diff_hook.py: fixtures allocate through __keepAlloc so linked blocks are never garbage-collected mid-run
    return "\n".join(p.read_text() if p.name in ("keepalive.js", "vector_capture.js")
                     else p.read_text().replace("Memory.alloc(", "__keepAlloc(") for p in parts)


def entry_bytes() -> list:
    """[va, first 8 bytes hex] of every golf_clean.exe function start, from the file image (the capture's patch scan)."""
    import vector_emu as EMU
    img = EMU.load_image()
    return [[va, img.data[va - VL.IMAGE_BASE:va - VL.IMAGE_BASE + 8].hex()] for va in sorted(img.func_starts)]


PROBE_VECTORS = 24       # vectors emulated per round to find live pages the capture missed
PROBE_ROUNDS = 4


def fetch_missing_pages(sc, spec, r, pay, blob) -> tuple[list, bytes]:
    """Emulate a spread of the key's vectors; every unmapped read (not an import, above 64 KiB) names a live page the
    function reaches through a pointer the capture did not follow (a game object on the heap). Fetch those pages from the
    still running process and repeat, up to PROBE_ROUNDS times. The build then proves them: a vector whose emulation
    over the fetched pages does not reproduce the live call is not kept. Returns (page records, their bytes)."""
    import vector_emu as EMU
    img = EMU.load_image()
    rows = r["rows"]
    step = max(1, len(rows) // PROBE_VECTORS)
    probe = [row for row in rows[::step] if not row["err"]][:PROBE_VECTORS]
    pages, extra = [], b""
    for _ in range(PROBE_ROUNDS):
        meta = {"blocks": pay["blocks"], "pages": pay["pages"] + pages}
        kc = KeyCapture(meta, blob + extra)
        data = bytearray(kc.image)
        want = set()
        for row in probe:
            res = EMU.run_call(img, bytes(data), [(b, x) for b, x, _ in kc.blocks], spec["abi"], spec["args"],
                               _arg_values(kc, row, spec["args"], None), spec["ret"], r["cw"], spec["addr"], budget=2_000_000, light=True)
            for m in res.misses:
                if m in res.fetch_misses:
                    continue                       # code outside the exe (a DLL call): never emulated from live pages
                if m >= 0x10000 and m not in img.imports and not (EMU.TRAP_BASE <= m < EMU.TRAP_BASE + 0x10000):
                    want.add(m & ~0xFFF)
        have = {p["base"] for p in pay["pages"] + pages} | {b["base"] & ~0xFFF for b in pay["blocks"]}
        want -= have
        if not want:
            break
        got = sc.exports_sync.pages(sorted(want))
        if not got:
            break
        for g in got:
            pages.append({"base": g["base"], "size": len(g["hex"]) // 2, "fetched": True, "module": g.get("module")})
            extra += bytes.fromhex(g["hex"])
    if pages:
        print(f"  {spec['key']}: fetched {len(pages)} live pages the emulation reached")
    return pages, extra


UNSTUB = "+unstub"       # suffix of a key captured with its sim-scope stand-ins reverted


def unstub_targets() -> set[int]:
    """Functions whose fixture stand-ins the --unstub variant reverts: the selected (sim-scope) functions."""
    return {int(r["addr"], 16) for r in csv.DictReader(SEL.open(), delimiter="\t") if r["selected"] == "1"}


def unstub_keys(keys: list[tuple[str, str]]) -> list[tuple[str, str]]:
    """Keys whose plain capture shows stand-ins on selected functions."""
    tg = unstub_targets()
    out = []
    for k, b in keys:
        p = RAW_DIR / f"{key_file(k)}.raw.gz"
        if p.exists() and set(read_raw(p)[0].get("patched", [])) & tg:
            out.append((k, b))
    return out


def capture(keys: list[tuple[str, str]], force: bool = False, unstub: bool = False) -> dict:
    from game import Game, JS_DIR
    from hooks_registry import HOOKS
    gs = VL.GsMap()
    nores = no_restore_ranges(gs)
    sfx = UNSTUB if unstub else ""
    tg = sorted(unstub_targets()) if unstub else []
    todo = [(k, b) for k, b in keys if force or not (RAW_DIR / f"{key_file(k)}{sfx}.raw.gz").exists()]
    by_batch = collections.OrderedDict()
    for k, b in todo:
        by_batch.setdefault(b, []).append(k)
    src = script_source(JS_DIR)
    stats = collections.Counter()
    for batch, bkeys in by_batch.items():
        queue = list(bkeys)
        retried = set()
        while queue:
            pending = {}

            def on_msg(m, data):
                if m.get("type") == "send" and m["payload"].get("type") == "key":
                    pending[m["payload"]["key"]] = (m["payload"], data)

            t0 = time.time()
            with Game(env={"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_HOOKS_OFF": "all"}) as g:
                g.wait_window()
                time.sleep(4.0)                # past sound.dll loading: early keys lost the game now and then
                sc = g.session.create_script(src)
                sc.on("message", on_msg)
                sc.load()
                sc.exports_sync.init(entry_bytes())
                print(f"[{batch}] boot pid {g.pid} ({time.time() - t0:.1f} s), {len(queue)} keys")
                while queue:
                    key = queue.pop(0)
                    spec = HOOKS[key]
                    call = {"key": key, "addr": spec["addr"], "abi": spec["abi"], "ret": spec["ret"], "args": spec["args"],
                            "fixture": spec.get("fixture"), "vectors": [list(v) for v in spec["vectors"]],
                            "noRestore": nores, "unstub": tg}
                    res, exc = {}, {}

                    def work():
                        try:
                            res["r"] = sc.exports_sync.capture(call)
                        except Exception as e:     # noqa: BLE001
                            exc["e"] = e

                    th = threading.Thread(target=work, daemon=True)
                    t1 = time.time()
                    th.start()
                    th.join(KEY_TIMEOUT)
                    path = RAW_DIR / f"{key_file(key)}{sfx}.raw.gz"
                    base = {"key": key + sfx, "batch": batch, "addr": spec["addr"], "abi": spec["abi"], "ret": spec["ret"],
                            "args": spec["args"], "fixture": spec.get("fixture"), "state": [list(s) for s in spec.get("state", [])],
                            "vectors_registry": [list(v) for v in spec["vectors"]],
                            "captured": time.strftime("%Y-%m-%dT%H:%M:%S"),
                            "winmm_sha256": hashlib.sha256((pathlib.Path(os.environ.get("SIMGOLF_GAME_DIR", ROOT / "original"))
                                                            / "winmm.dll").read_bytes()).hexdigest()[:16]}
                    if "e" in exc and not th.is_alive():
                        time.sleep(0.5)            # the detach notification can trail the rpc error
                    if (th.is_alive() or "e" in exc or not g.alive) and key not in retried:
                        # first failure: the same key once more in a fresh boot (CLAUDE.md: re-run a RED / ERROR once)
                        retried.add(key)
                        queue.insert(0, key)
                        print(f"  {key}: {'TIMEOUT' if th.is_alive() else 'ERROR'} {str(exc.get('e'))[:100]}; retrying in a fresh boot")
                        break
                    if th.is_alive() or "e" in exc or not g.alive:
                        why = "TIMEOUT" if th.is_alive() else f"ERROR {type(exc.get('e')).__name__}: {exc.get('e')}"
                        if not g.alive:
                            why += f" (game ended: {g.detach_reason})"
                        write_raw(path, {**base, "status": "capture_error", "error": why[:500]}, b"")
                        print(f"  {key}: {why[:160]}")
                        stats["capture_error"] += 1
                        break                      # fresh boot for the rest of the batch
                    r = res["r"]
                    pay, blob = pending.pop(key, (None, None))
                    if pay is None:
                        write_raw(path, {**base, "status": "capture_error", "error": "no key message"}, b"")
                        stats["capture_error"] += 1
                        continue
                    fetched = b""
                    if not r["fixtureError"] and r["rows"]:
                        extra, fetched = fetch_missing_pages(sc, call, r, pay, blob)
                        pay["pages"] = pay["pages"] + extra
                    meta = {**base, "status": "ok" if not r["fixtureError"] else "fixture_error", "first_byte": r["firstByte"],
                            "modules": r.get("modules", []),
                            "patched": r.get("patched", []), "unstubbed": r.get("unstubbed", []), "frozen": r.get("frozen"), "freeze_error": r.get("freezeError"),
                            "fpcw": r["cw"], "uses": r["uses"], "fx": r["fxNames"], "fixture_error": r["fixtureError"],
                            "blocks": pay["blocks"], "pages": pay["pages"], "rows": r["rows"],
                            "seconds": round(time.time() - t1, 2)}
                    write_raw(path, meta, blob + fetched)
                    nerr = sum(1 for row in r["rows"] if row["err"])
                    stats["ok"] += 1
                    if r.get("patched"):
                        print(f"  {key}: stood-in functions " + " ".join(f"0x{a:08x}" for a in r["patched"][:12]))
                    print(f"  {key}: {len(r['rows'])} vectors ({nerr} live errors), {len(pay['blocks'])} blocks, "
                          f"{len(pay['pages'])} pages, uses={','.join(r['uses']) or '-'} cw=0x{r['cw']:04x} "
                          f"first=0x{r['firstByte']:02x} frozen={r.get('frozen')} {time.time() - t1:.1f} s")
    return dict(stats)


# --------------------------------------------------------------------------------------------- build
import vector_emu as EMU  # noqa: E402

# functions whose input arrives in an x87 register: a vector of memory and stack arguments cannot carry it (the C3 A/B
# of c3fo ran them through an exported probe that loads st(0); a direct call leaves st(0) whatever the thread held, and
# the live replay of the reimplementation read another value for every __ftol vector, 2026-10-19)
X87_REGISTER_INPUT = {0x004A6030: "x87 register input: __ftol converts st(0); not representable as a memory vector"}
HEAVY_ACCESSES = 2_000_000   # a key whose first hooked vector makes more memory accesses keeps HEAVY_SAMPLE vectors
HEAVY_SAMPLE = 12
HEAVY_INSNS = 50_000_000     # a vector whose plain emulation runs more instructions is not run with the hooks
EARLY_GIVE_UP = 12          # emulated vectors of a key that must all fail before the rest are skipped
RELOC_BASE = 0x30000000      # second emulation: blocks packed upwards from here, 64 KiB apart, live page offset kept


def packed_bases(base0: int, sizes: list[int], offsets: list[int]) -> list[int]:
    """Block bases for a moved layout: each block on its own 64 KiB-aligned run (List402_clear has 80+ blocks; a fixed
    16 MiB stride ran into the stack / trap region, UC_ERR_MAP)."""
    out, cur = [], base0
    for n, o in zip(sizes, offsets):
        out.append(cur + o)
        cur = (cur + o + n + 0x1FFFF) & ~0xFFFF
    return out


class KeyCapture:
    def __init__(self, meta: dict, blob: bytes, drop: frozenset = frozenset()):
        self.meta = meta
        self.image = blob[:VL.DATA_SIZE]
        o = VL.DATA_SIZE
        self.blocks = []          # (live base, bytes, role)
        for b in meta.get("blocks", []):
            self.blocks.append((b["base"], blob[o:o + b["size"]], "fixture"))
            o += b["size"]
        for p in meta.get("pages", []):
            self.blocks.append((p["base"], blob[o:o + p["size"]], "page"))
            o += p["size"]
        # overlapping pages / blocks: drop a page that overlaps a fixture block or an earlier page
        keep = []
        for base, b, role in self.blocks:
            if role == "page" and any(base < kb + len(kbb) and kb < base + len(b) for kb, kbb, _ in keep):
                continue
            keep.append((base, b, role))
        # pages no vector touched (an integer that happened to be a mapped address) are dropped by the builder's 2nd pass
        self.blocks = [x for x in keep if not (x[2] == "page" and x[0] in drop)]

    def block_of(self, va: int):
        if not hasattr(self, "_iv"):
            import bisect
            self._bisect = bisect
            self._iv = sorted((base, base + len(b), i) for i, (base, b, _) in enumerate(self.blocks))
            self._starts = [x[0] for x in self._iv]
        k = self._bisect.bisect_right(self._starts, va) - 1
        if k >= 0:
            lo, hi, i = self._iv[k]
            if va < hi:
                return i, va - lo
        return None


def _apply_dw(buf: bytearray, flat: list[int]):
    for k in range(0, len(flat), 2):
        struct.pack_into("<I", buf, 4 * flat[k], flat[k + 1])


def _arg_values(kc: KeyCapture, row: dict, types: list[str], reloc: dict | None):
    vals = []
    for t, a in zip(types, row["args"]):
        if "p" in a:
            v = a["p"]
        elif "n" in a:
            v = a["n"]
        else:
            v = int(a["s"]) if t in ("int64", "uint64") else float(a["s"])
        if t == "pointer" and reloc is not None:
            v = reloc_ptr(kc, v, reloc)
        vals.append(v)
    return vals


def reloc_ptr(kc: KeyCapture, v: int, reloc: dict) -> int:
    hit = kc.block_of(v)
    return reloc[hit[0]] + hit[1] if hit else v


def live_ret(meta: dict, row: dict):
    t = meta["ret"]
    r = row["ret"]
    if t == "void":
        return None
    if t in ("double", "float"):
        return float(r) if r is not None else None
    if t in ("int64", "uint64"):
        return int(r)
    return VL.norm_ret(t, int(r) & 0xFFFFFFFF) if t != "pointer" else int(r) & 0xFFFFFFFF


def emu_ret(meta: dict, res: EMU.CallResult):
    return VL.norm_ret(meta["ret"], res.eax, res.edx, res.st0)


def loc_of(kc: KeyCapture, gs: VL.GsMap, img: EMU.ExeImage, va: int) -> dict:
    """An address as a corpus location."""
    if va == 0:
        return {"null": True}
    if VL.DATA_VA <= va < VL.DATA_VA + VL.DATA_SIZE:
        s, o = gs.ref(va)
        return {"sym": s, "off": o}
    hit = kc.block_of(va)
    if hit:
        return {"block": hit[0], "off": hit[1]}
    sec = img.section_of(va)
    if sec:
        return {"exe": sec[0], "off": sec[1]}
    return {"raw": va}


def build_key(path: pathlib.Path, gs: VL.GsMap, img: EMU.ExeImage, drop: frozenset = frozenset()) -> tuple[dict | None, dict]:
    """One raw capture -> (the key's .vec section or None, its manifest facts)."""
    meta, blob = read_raw(path)
    facts = {"key": meta["key"], "addr": meta["addr"], "batch": meta["batch"], "status": meta["status"],
             "vectors": 0, "ok": 0, "distinct": 0, "x87": 0, "heap": 0, "imports": "", "uses": "",
             "reason": meta.get("error") or meta.get("fixture_error") or "", "insns": 0}
    if meta["status"] != "ok":
        return None, facts
    kc = KeyCapture(meta, blob, drop)
    facts["uses"] = ",".join(meta["uses"])
    if meta["addr"] in X87_REGISTER_INPUT:
        facts["status"], facts["reason"] = "excluded", X87_REGISTER_INPUT[meta["addr"]]
        return None, facts
    if meta["first_byte"] == 0xE9:
        facts["status"], facts["reason"] = "excluded", "first byte 0xE9 at capture (a hook was on)"
        return None, facts
    data = bytearray(kc.image)
    blocks_live = [(base, b) for base, b, _ in kc.blocks]
    reloc = dict(enumerate(packed_bases(RELOC_BASE, [len(b) for _, b, _ in kc.blocks], [base & 0xFFF for base, _, _ in kc.blocks])))
    vec_out, reads_all, imports, callees, mism = [], set(), collections.Counter(), [], collections.Counter()
    dll_calls = collections.Counter()
    data_reads_all = set()
    const_reads = set()
    x87 = False
    entry = meta["addr"]
    per_vec = []
    keep_every = 1                 # set when the key turns out heavy: only every keep_every-th row is emulated
    for row in meta["rows"]:
        _apply_dw(data, row["drift"])
        st = {"i": row["i"], "status": "ok"}
        if keep_every > 1 and row["i"] % keep_every:
            st["status"] = "not_emulated"
            st["detail"] = f"heavy key: every {keep_every}th vector emulated ({HEAVY_SAMPLE} of {len(meta['rows'])})"
            mism["not_emulated"] += 1
            per_vec.append((row, st, None, None, bytes(data)))
            continue
        tried = [x for x in per_vec if x[2] is not None]
        if len(tried) >= EARLY_GIVE_UP and not any(x[1]["status"] == "ok" for x in tried):
            # the first EARLY_GIVE_UP emulated vectors all failed (stood-in callees, imports): the rest are not emulated
            st["status"] = "not_emulated"
            st["detail"] = f"the first {EARLY_GIVE_UP} emulated vectors of the key all failed"
            mism["not_emulated"] += 1
            per_vec.append((row, st, None, None, bytes(data)))
            continue
        if row["err"]:
            st["status"] = "live_error"
            st["detail"] = row["err"][:200]
            per_vec.append((row, st, None, None, bytes(data)))
            mism["live_error"] += 1
            continue
        # light pre-check (no Python hooks, native speed): a vector whose plain emulation already fails or returns another
        # value is not run with the hooks (editTerrainHeight: 2 s light against minutes hooked for a diverging vector)
        pre = EMU.run_call(img, bytes(data), blocks_live, meta["abi"], meta["args"], _arg_values(kc, row, meta["args"], None),
                           meta["ret"], meta["fpcw"], entry, light=True, budget=HEAVY_INSNS // 20)
        if pre.err and pre.err.startswith("stopped at"):
            # more than HEAVY_INSNS instructions: the hooked emulation would take minutes per vector (demolishObject's
            # vectors ran ~100 M); left to the segment corpus (deterministic: an instruction count, not a time)
            st["status"] = "not_emulated"
            st["detail"] = f"over the emulation budget ({HEAVY_INSNS:,} instructions)"
            mism["not_emulated"] += 1
            per_vec.append((row, st, None, None, bytes(data)))
            continue
        pre_bad = pre.err is not None or (meta["ret"] != "void" and not VL.float_eq(live_ret(meta, row), emu_ret(meta, pre)))
        if pre_bad:
            res = pre
            if pre.err is None:
                st["status"] = "ret_mismatch"
                st["detail"] = f"live {live_ret(meta, row)!r} emulated {emu_ret(meta, pre)!r} (light emulation)"
                mism["ret_mismatch"] += 1
                per_vec.append((row, st, None, None, bytes(data)))
                continue
        else:
            res = EMU.run_call(img, bytes(data), blocks_live, meta["abi"], meta["args"],
                               _arg_values(kc, row, meta["args"], None), meta["ret"], meta["fpcw"], entry)
            if keep_every == 1 and res.accesses > HEAVY_ACCESSES and len(meta["rows"]) > HEAVY_SAMPLE:
                # deterministic (an access count, not a time): updateGolfers' vectors cost ~10 s each with the hooks
                keep_every = -(-len(meta["rows"]) // HEAVY_SAMPLE)
        facts["insns"] += res.insns
        x87 |= res.x87
        for c in res.callees:
            if c not in callees:
                callees.append(c)
        if res.imports:
            imports[res.imports[0]] += 1
        if res.err:
            st["status"] = "emu_error"
            st["detail"] = res.err[:200]
            if res.fetch_misses:
                mod = next((m["name"] for m in meta.get("modules", []) if m["base"] <= res.fetch_misses[0] < m["base"] + m["size"]), None)
                st["detail"] = f"calls code outside golf_clean.exe ({mod or 'no module: a fixture stub'})"
                if mod:
                    dll_calls[mod] += 1
            mism["emu_error"] += 1
            per_vec.append((row, st, res, None, bytes(data)))
            continue
        lr, er = live_ret(meta, row), emu_ret(meta, res)
        if not VL.float_eq(lr, er):
            st["status"] = "ret_mismatch"
            st["detail"] = f"live {lr!r} emulated {er!r}"
            mism["ret_mismatch"] += 1
        # writes: every emulated data byte must hold its live post value, and every live change must be an emulated write
        post = bytearray(data)
        _apply_dw(post, row["dataDiff"])
        live_changed = set()
        for k in range(0, len(row["dataDiff"]), 2):
            i = row["dataDiff"][k]
            for b in range(4):
                a = 4 * i + b
                if post[a] != data[a]:
                    live_changed.add(VL.DATA_VA + a)
        bad_w = [a for a, v in res.mem.items() if VL.DATA_VA <= a < VL.DATA_VA + VL.DATA_SIZE and post[a - VL.DATA_VA] != v]
        unexplained = sorted(a for a in live_changed if a not in res.writes)
        # blocks
        for bi, (base, b, _) in enumerate(kc.blocks):
            bpost = bytearray(b)
            if bi < len(row["blockDiff"]):
                d = row["blockDiff"][bi]
                for k in range(0, len(d), 2):
                    bpost[d[k]] = d[k + 1]
            for a, v in res.mem.items():
                if base <= a < base + len(b) and bpost[a - base] != v:
                    bad_w.append(a)
            for off in range(len(b)):
                if bpost[off] != b[off] and base + off not in res.writes:
                    unexplained.append(base + off)
        st["unexplained_live_bytes"] = len(unexplained)
        if bad_w:
            st["status"] = "write_mismatch"
            st["detail"] = f"{len(bad_w)} emulated bytes differ from live, first 0x{min(bad_w):08x}"
            mism["write_mismatch"] += 1
        elif unexplained and st["status"] == "ok":
            # a live change the emulated original never wrote: a concurrent game write (the menu runs meanwhile)
            # unless it touches a byte the function read; then the vector is not trustworthy
            rd = {a + k for a, n in res.reads for k in range(n)}
            if any(a in rd for a in unexplained):
                st["status"] = "concurrent_input"
                st["detail"] = f"{len(unexplained)} live bytes changed by another thread, some of them read"
                mism["concurrent_input"] += 1
            else:
                st["noise_objects"] = sorted({gs.ref(a)[0] for a in unexplained if VL.DATA_VA <= a < VL.DATA_VA + VL.DATA_SIZE})[:8]
        per_vec.append((row, st, res, None, bytes(data)))
    # relocation candidates: dwords (in blocks and in data bytes read) whose value points into a block
    for row, st, res, _, dsnap in per_vec:
        if res is None:
            continue
        for a, n in res.reads:
            for k in range(n):
                x = a + k
                if VL.DATA_VA <= x < VL.DATA_VA + VL.DATA_SIZE:
                    data_reads_all.add(x)
                elif kc.block_of(x) is None and img.section_of(x):
                    const_reads.add(x)
    used = set()
    for row, st, res, _, dsnap in per_vec:
        if res is None:
            continue
        for a, n in res.reads:
            h = kc.block_of(a)
            if h:
                used.add(h[0])
        for a in res.writes:
            h = kc.block_of(a)
            if h:
                used.add(h[0])
    unused_pages = frozenset(base for i, (base, _, role) in enumerate(kc.blocks) if role == "page" and i not in used)
    if unused_pages:
        return build_key(path, gs, img, drop | unused_pages)
    relocs = []          # (location kind, index/offset, target block, target off): dwords that point into a block
    # every byte offset, not only aligned dwords: List402_clear's 64-byte records hold block pointers at +17 and +22;
    # overlapping candidates keep the first (left to right); the moved-block emulation below proves the set
    for bi, (base, b, _) in enumerate(kc.blocks):
        nxt = 0
        for off in range(0, len(b) - 3):
            if off < nxt:
                continue
            v = struct.unpack_from("<I", b, off)[0]
            if v < 0x10000:
                continue
            hit = kc.block_of(v)
            if hit and (hit[0] in used or kc.blocks[hit[0]][2] == "fixture"):
                relocs.append((("block", bi, off), hit))
                nxt = off + 4
    nxt = 0
    for dva in sorted(data_reads_all):
        if dva < nxt or not all(dva + k in data_reads_all for k in (1, 2, 3)):
            continue
        v = struct.unpack_from("<I", kc.image, dva - VL.DATA_VA)[0]
        hit = kc.block_of(v) if v >= 0x10000 else None
        if hit:
            relocs.append((("data", dva), hit))
            nxt = dva + 4
    # second emulation with every block moved: proves the relocations are complete for the vectors that passed
    reloc_bad = 0
    moved_blocks = [(reloc[i], b) for i, (_, b, _) in enumerate(kc.blocks)]
    for row, st, res, _, dsnap in per_vec:
        if res is None or st["status"] != "ok" or not kc.blocks:
            continue
        d2 = bytearray(dsnap)
        mb = [bytearray(b) for _, b in moved_blocks]
        for (where, hit) in relocs:
            nv = reloc[hit[0]] + hit[1]
            if where[0] == "block":
                struct.pack_into("<I", mb[where[1]], where[2], nv)
            else:
                struct.pack_into("<I", d2, where[1] - VL.DATA_VA, nv)
        r2 = EMU.run_call(img, bytes(d2), [(reloc[i], bytes(mb[i])) for i in range(len(mb))], meta["abi"], meta["args"],
                          _arg_values(kc, row, meta["args"], reloc), meta["ret"], meta["fpcw"], entry)
        ok2 = r2.err is None
        if ok2:
            e1, e2 = emu_ret(meta, res), emu_ret(meta, r2)
            if meta["ret"] == "pointer" or (isinstance(e1, int) and kc.block_of(e1 & 0xFFFFFFFF)):
                h1 = kc.block_of(e1 & 0xFFFFFFFF)
                e1 = (h1[0], h1[1]) if h1 else e1
                e2u = e2 & 0xFFFFFFFF
                h2 = next(((k, e2u - v) for k, v in reloc.items() if v <= e2u < v + len(kc.blocks[k][1])), None)
                e2 = h2 if h2 else e2
            ok2 = VL.float_eq(e1, e2)
        if ok2:
            # writes, mapped back to live addresses
            back = {}
            for a, v in r2.mem.items():
                k = next((k for k, base in reloc.items() if base <= a < base + len(kc.blocks[k][1])), None)
                back[kc.blocks[k][0] + a - reloc[k] if k is not None else a] = v
            exp = dict(res.mem)
            # a pointer into a block the function wrote differs by design: expect it at its moved value
            for a, (k, o) in out_pointers(kc, res.mem):
                for i, byte in enumerate((reloc[k] + o).to_bytes(4, "little")):
                    exp[a + i] = byte
            ok2 = set(back) == set(exp) and all(back[a] == exp[a] for a in exp)
        # the vector itself stands (live == emulated); only the relocation table is unproven for it
        st["reloc_check"] = bool(ok2)
        if not ok2:
            reloc_bad += 1
    # ------------------------------------------------------------------ the .vec section of this key
    seed_bytes = {}
    vectors = []
    for row, st, res, _, dsnap in per_vec:
        v = {"i": row["i"], "status": st["status"]}
        if "detail" in st:
            v["detail"] = st["detail"]
        v["args"] = []
        for t, a in zip(meta["args"], row["args"]):
            if t == "pointer":
                v["args"].append({"ptr": loc_of(kc, gs, img, a.get("p", a.get("n", 0)) & 0xFFFFFFFF)})
            elif t in ("double", "float"):
                v["args"].append({"float": float(a.get("n", a.get("s", 0)))})
            elif t in ("int64", "uint64"):
                v["args"].append({"int64": int(a.get("s", a.get("n", 0)))})
            else:
                v["args"].append({"int": int(a["n"]) if "n" in a else int(a["p"])})
        if row["err"]:
            v["ret"] = None
            v["out"] = []
            vectors.append(v)
            continue
        lr = live_ret(meta, row)
        if meta["ret"] == "pointer":
            v["ret"] = {"ptr": loc_of(kc, gs, img, lr)}
        elif isinstance(lr, float) and (lr != lr or lr in (float("inf"), float("-inf"))):
            v["ret"] = {"float_repr": repr(lr)}
        else:
            v["ret"] = lr
            if isinstance(lr, int) and kc.block_of(lr & 0xFFFFFFFF):
                # an integer return that is a block address (logWrite): compare it as a location
                v["ret_loc"] = loc_of(kc, gs, img, lr & 0xFFFFFFFF)
        if res is None:
            v["out"] = []
            vectors.append(v)
            continue
        # inputs: data bytes this vector read; the value at the key image goes to the seed, a drifted value to `in`
        ins = []
        rd_all = {a + k for a, n in res.reads for k in range(n)}
        rd = sorted(a for a in rd_all if VL.DATA_VA <= a < VL.DATA_VA + VL.DATA_SIZE)
        rd_blk = sorted(a for a in rd_all if kc.block_of(a))
        for a in rd:
            seed_bytes[a] = kc.image[a - VL.DATA_VA]
            if dsnap[a - VL.DATA_VA] != kc.image[a - VL.DATA_VA]:
                ins.append(a)
        v["in"] = _regions(gs, kc, img, ins, lambda a: dsnap[a - VL.DATA_VA])
        v["reads"] = [[r["sym"], r["off"], len(r["hex"]) // 2] for r in _regions(gs, kc, img, rd, lambda a: dsnap[a - VL.DATA_VA])]
        v["reads"] += [["#%d" % r["block"], r["off"], len(r["hex"]) // 2] for r in _regions(gs, kc, img, rd_blk, lambda a: 0)]
        if "reloc_check" in st:
            v["reloc_check"] = st["reloc_check"]
        v["out"] = _regions(gs, kc, img, sorted(res.mem), lambda a: res.mem[a])
        ops = out_pointers(kc, res.mem)
        if ops:
            # written dwords that point into a block: a harness compares them as locations, not as bytes
            v["out_ptrs"] = [{"at": loc_of(kc, gs, img, a), "to": {"block": k, "off": o}} for a, (k, o) in ops]
        v["stack_pop"] = res.pop
        v["blocks_executed"] = res.insns
        vectors.append(v)
    ok_vecs = [v for v in vectors if v["status"] == "ok"]
    # branch coverage of the function's own conditional jumps over the vectors that reproduced (emulation edges)
    edges_all = set()
    for row, st, res, _, dsnap in per_vec:
        if res is not None and st["status"] == "ok":
            edges_all |= res.edges
    cov = jcc_coverage(img, entry, edges_all)
    distinct = len({json.dumps([v["ret"], v["out"]], sort_keys=True) for v in ok_vecs})
    heap_imports = {"KERNEL32.dll!HeapAlloc", "KERNEL32.dll!HeapFree", "KERNEL32.dll!HeapReAlloc", "KERNEL32.dll!VirtualAlloc",
                    "KERNEL32.dll!HeapSize", "KERNEL32.dll!GlobalAlloc", "KERNEL32.dll!LocalAlloc"}
    facts["edges"] = edges_all
    facts.update(vectors=len(vectors), ok=len(ok_vecs), distinct=distinct, x87=int(x87),
                 heap=int("heap_standin" in meta["uses"] or any(i in heap_imports for i in imports)),
                 imports=",".join(sorted(imports) + [f"{m}(call)" for m in sorted(dll_calls)]), mismatch=dict(mism), reloc_bad=reloc_bad)
    # A stand-in or a frozen thread in the fixture does not by itself exclude a key: the emulation runs the plain
    # original (real callees, no stand-ins, no other thread), so a vector it reproduces stands on its own memory. The
    # stand-ins' effect shows as emulation errors (an import, an unmapped stub) or mismatches, which exclude vectors.
    if not ok_vecs:
        facts["status"] = "excluded"
        facts["reason"] = "no vector reproduced: " + ", ".join(f"{k} {n}" for k, n in mism.items()) + \
            (f" (imports {facts['imports']})" if imports else "")
    elif len(ok_vecs) < len(vectors):
        facts["status"] = "partial"
        facts["reason"] = ", ".join(f"{k} {n}" for k, n in mism.items())
    first = next((v for v in vectors if v["status"] != "ok"), None)
    if first is not None:
        facts["reason"] += f"; first: vector {first['i']} {first.get('detail', first['status'])[:120]}"
    patched = set(meta.get("patched", []))
    hit = [c for c in callees if c in patched]
    if hit and first is not None:
        facts["reason"] += "; stood-in callees reached: " + " ".join(img.func_starts.get(c, f"FUN_{c:08x}") for c in hit[:8])
    stand = [u for u in meta["uses"] if u in ("heap_standin", "import_standin", "file_standin", "mmio_standin", "uitree",
                                              "modal", "framemask", "interceptor", "recdisplay")]
    if stand and first is not None:
        facts["reason"] += f"; fixture stand-ins {','.join(stand)} (the live original ran with them, the emulation without)"
    blocks_json = [{"id": i, "role": role, "size": len(b), "hex": b.hex(), "tooling_live_base": base}
                   for i, (base, b, role) in enumerate(kc.blocks)]
    relocs_json = []
    for where, hit in relocs:
        at = {"block": where[1], "off": where[2]} if where[0] == "block" else dict(zip(("sym", "off"), gs.ref(where[1])))
        relocs_json.append({"at": at, "to": {"block": hit[0], "off": hit[1]}})
    seed = _regions(gs, kc, img, sorted(seed_bytes), lambda a: seed_bytes[a])
    # inputs from objects the model says hold process pointers (class `pointer`, or a `words` object): their values are
    # original addresses, which a port GameState cannot hold as they are
    cls = gs.classes()
    ptr_objs = sorted({r["sym"] for r in seed if cls.get(r["sym"]) == "pointer" or gs.by_sym[r["sym"]][4].startswith("words")})
    const = []
    for a, n in VL.runs(const_reads):
        sec = img.section_of(a)
        const.append({"exe": sec[0], "off": sec[1], "hex": img.data[a - VL.IMAGE_BASE:a - VL.IMAGE_BASE + n].hex()})
    section = {"key": meta["key"], "batch": meta["batch"], "abi": meta["abi"], "ret": meta["ret"], "args": meta["args"],
               "fixture": meta["fixture"], "fpcw": meta["fpcw"], "x87": x87, "uses": meta["uses"],
               "imports": sorted(imports), "callees": [img.func_starts.get(c, f"FUN_{c:08x}") for c in callees],
               "blocks": blocks_json, "relocs": relocs_json, "seed": seed, "const": const,
               "pointer_objects_read": ptr_objs,
               "stood_in": [img.func_starts.get(c, f"FUN_{c:08x}") for c in sorted(meta.get("patched", []))],
               "stood_in_reached": [img.func_starts.get(c, f"FUN_{c:08x}") for c in hit],
               "registry_state": meta["state"], "capture": {"captured": meta["captured"], "winmm": meta["winmm_sha256"],
                                                            "seconds": meta.get("seconds")},
               "relocs_proven": reloc_bad == 0, "coverage": cov, "vectors": vectors, "distinct_results": distinct}
    return section, facts


def jcc_coverage(img, entry: int, edges: set) -> dict:
    """Sides of each conditional jump in the function's own ranges that the edges took. Offsets are function-relative."""
    sites = EMU.jcc_sites(img, entry)
    both = one = none = 0
    open_sides = []
    for j, (t, f) in sorted(sites.items()):
        taken, fall = (j, t) in edges, (j, f) in edges
        if t == f:                      # a jcc to the next instruction: one side
            taken = fall = taken or fall
        if taken and fall:
            both += 1
        elif taken or fall:
            one += 1
            open_sides.append(f"+0x{j - entry:x}:{'fall' if taken else 'taken'}")
        else:
            none += 1
            open_sides.append(f"+0x{j - entry:x}:both")
    return {"jcc": len(sites), "both": both, "one_side": one, "none": none, "open": open_sides}


def out_pointers(kc, mem: dict) -> list:
    """Written byte runs scanned at every offset for 4-byte values that point into a block (greedy, left to right)."""
    out = []
    for start, n in VL.runs(mem):
        off = 0
        while off + 4 <= n:
            v = int.from_bytes(bytes(mem[start + off + k] for k in range(4)), "little")
            hit = kc.block_of(v) if v >= 0x10000 else None
            if hit:
                out.append((start + off, hit))
                off += 4
            else:
                off += 1
    return out


def _is_reloc_byte(a, back, exp, kc, reloc):
    """Bytes of a dword whose relocated and live values are the same block offset."""
    dw = a & ~3
    try:
        v1 = sum(exp[dw + k] << 8 * k for k in range(4))
        v2 = sum(back[dw + k] << 8 * k for k in range(4))
    except KeyError:
        return False
    h1 = kc.block_of(v1)
    if not h1:
        return False
    return v2 == reloc[h1[0]] + h1[1]


def _regions(gs, kc, img, addrs, val) -> list[dict]:
    out = []
    for a, n in VL.runs(addrs):
        b = bytes(val(a + k) for k in range(n))
        if VL.DATA_VA <= a < VL.DATA_VA + VL.DATA_SIZE:
            out.extend(gs.split(a, b))
            continue
        hit = kc.block_of(a)
        if hit:
            out.append({"block": hit[0], "off": hit[1], "hex": b.hex()})
            continue
        sec = img.section_of(a)
        out.append({"exe": sec[0], "off": sec[1], "hex": b.hex()} if sec else {"raw": a, "hex": b.hex()})
    return out


_GS = _IMG = None


def build_function(r: dict, notes: dict) -> tuple[list, list]:
    """One selected function: build every key, write its .vec, return (manifest rows, log lines). Runs in a worker."""
    global _GS, _IMG
    if _GS is None:
        _GS, _IMG = VL.GsMap(), EMU.load_image()
    gs, img = _GS, _IMG
    rows, log = [], []
    keys = r["key_list"].split(" ")
    keys += [k + UNSTUB for k in keys if (RAW_DIR / f"{key_file(k)}{UNSTUB}.raw.gz").exists()]
    sections, facts = [], []
    fn_edges = set()
    for k in keys:
        p = RAW_DIR / f"{key_file(k)}.raw.gz"
        if not p.exists():
            facts.append({"key": k, "status": "not_captured", "vectors": 0, "ok": 0, "distinct": 0, "x87": 0, "heap": 0,
                          "imports": "", "uses": "", "reason": "", "batch": "", "insns": 0})
            continue
        t0 = time.time()
        try:
            sec, f = build_key(p, gs, img)
        except Exception as e:     # noqa: BLE001 - a build defect of one key must not hide the others
            sec, f = None, {"key": k, "status": "build_error", "vectors": 0, "ok": 0, "distinct": 0, "x87": 0,
                            "heap": 0, "imports": "", "uses": "", "reason": f"{type(e).__name__}: {e}"[:300],
                            "batch": "", "insns": 0}
        f["build_s"] = round(time.time() - t0, 1)
        fn_edges |= f.pop("edges", set())
        facts.append(f)
        if sec is not None and f["status"] in ("ok", "partial"):
            sections.append(sec)
    vec_path = VL.VEC_DIR / f"{key_file(r['name'])}.vec"
    sha, size = "-", 0
    if sections:
        doc = {"schema": VL.SCHEMA, "function": r["name"], "module": "golf_clean.exe", "tooling_va": r["addr"],
               "subsystem": r["subsystem"], "tier": r["tier"], "level": r["level"],
               "exe_sha256": "E74EA9F95350796432CB1CCDA3BFFE00E48789F3E410CD5FFB61F70ACEB26789",
               "gamestate_meta_sha256": VL.sha256_file(VL.META_INC)[:16], "keys": sections}
        VL.write_vec(vec_path, doc)
        sha, size = VL.sha256_file(vec_path), vec_path.stat().st_size
    elif vec_path.exists():
        vec_path.unlink()
    fcov = jcc_coverage(img, int(r["addr"], 16), fn_edges)
    for f in facts:
        rows.append({"function": r["name"], "addr": r["addr"], "tier": r["tier"], "key": f["key"], "status": f["status"],
                     "vectors": f["vectors"], "ok_vectors": f["ok"], "distinct_results": f["distinct"],
                     "x87": f["x87"], "depends_on_heap": f["heap"], "imports": f["imports"] or "-",
                     "fn_jcc": fcov["jcc"], "fn_jcc_both": fcov["both"], "fn_jcc_one": fcov["one_side"],
                     "fn_jcc_none": fcov["none"], "fn_jcc_open": " ".join(fcov["open"]) or "-",
                     "fixture_uses": f["uses"] or "-", "branch_notes": notes.get(int(r["addr"], 16), "-"),
                     "vec_file": vec_path.name if sha != "-" else "-",   # in port/corpus/vectors/
                     "sha256": sha, "bytes": size, "reason": (f.get("reason") or "-").replace("\t", " ").replace("\n", " ")})
        log.append(f"{r['name']:40s} {f['key']:40s} {f['status']:12s} {f['ok']}/{f['vectors']} distinct {f['distinct']} "
                   f"{f.get('build_s', '')}s {f.get('reason', '')[:80]}")
    return rows, log


def build(functions: list[str] | None = None, jobs: int = 1) -> None:
    sel = [r for r in csv.DictReader(SEL.open(), delimiter="\t") if r["selected"] == "1"]
    todo = [r for r in sel if not functions or r["name"] in functions]
    notes = branch_notes()
    rows = []
    if jobs > 1:
        import concurrent.futures as cf
        with cf.ProcessPoolExecutor(jobs) as ex:
            for rr, lg in ex.map(build_function, todo, [notes] * len(todo)):
                rows += rr
                print("\n".join(lg), flush=True)
    else:
        for r in todo:
            rr, lg = build_function(r, notes)
            rows += rr
            print("\n".join(lg), flush=True)
    if functions:
        old = [x for x in csv.DictReader(MANIFEST.open(), delimiter="\t")] if MANIFEST.exists() else []
        names = {x["function"] for x in rows}
        rows = [x for x in old if x["function"] not in names] + rows
        order = {r["name"]: i for i, r in enumerate(sel)}
        rows.sort(key=lambda x: order.get(x["function"], 1 << 30))
    with MANIFEST.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    print(f"-> {MANIFEST} ({len(rows)} keys)")


def branch_notes() -> dict[int, str]:
    """Function address -> the C3 purpose / notes file that states its branch coverage, where one exists."""
    import re
    out = {}
    for p in sorted((ROOT / "log" / "c3").glob("*_purpose.md")):
        txt = p.read_text(errors="replace")
        for m in re.finditer(r"0x(00[0-9a-fA-F]{6})", txt):
            out.setdefault(int(m[1], 16), p.relative_to(ROOT).as_posix())
    return out


def status():
    st = collections.Counter()
    for p in RAW_DIR.glob("*.raw.gz"):
        with gzip.open(p, "rb") as f:
            n = struct.unpack("<I", f.read(4))[0]
            st[json.loads(f.read(n))["status"]] += 1
    print(dict(st))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["capture", "build", "status"])
    ap.add_argument("--instance", default="p25")
    ap.add_argument("--batch", nargs="*")
    ap.add_argument("--key", nargs="*")
    ap.add_argument("--function", nargs="*")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--jobs", type=int, default=1, help="build: worker processes")
    ap.add_argument("--unstub", action="store_true",
                    help="capture the keys whose fixtures stand in selected functions again, with those stand-ins reverted")
    ap.add_argument("--stale", action="store_true", help="capture keys with no raw file or one from before the frozen capture")
    ap.add_argument("--shard", help="k/n: only every n-th batch starting at k (parallel instances)")
    ap.add_argument("--retry-errors", action="store_true", help="re-capture the keys whose raw capture is an error")
    a = ap.parse_args()
    if a.cmd == "capture":
        os.environ.setdefault("SIMGOLF_ALLOW_MULTI", "1")
        os.environ.setdefault("SIMGOLF_GAME_DIR", str(ROOT / "instances" / a.instance))
        keys = selected_keys()
        if a.batch:
            keys = [(k, b) for k, b in keys if b in a.batch]
        if a.key:
            from vector_select import key_batches
            kb = key_batches()
            keys = [(k, kb[k]) for k in a.key]
        if a.stale:
            def fresh(k):
                p = RAW_DIR / f"{key_file(k)}.raw.gz"
                return p.exists() and "modules" in read_raw(p)[0]
            keys = [(k, b) for k, b in keys if not fresh(k)]
        if a.shard:
            i, n = (int(x) for x in a.shard.split("/"))
            order = list(dict.fromkeys(b for _, b in keys))
            mine = set(order[i::n])
            keys = [(k, b) for k, b in keys if b in mine]
        if a.retry_errors:
            def raw_status(k):
                p = RAW_DIR / f"{key_file(k)}.raw.gz"
                return read_raw(p)[0]["status"] if p.exists() else None
            keys = [(k, b) for k, b in keys if raw_status(k) == "capture_error"]
        if a.unstub:
            keys = unstub_keys(keys)
        print(f"{len(keys)} keys")
        print(capture(keys, a.force or a.retry_errors or a.stale, a.unstub))
    elif a.cmd == "build":
        build(a.function, a.jobs)
    else:
        status()


if __name__ == "__main__":
    main()
