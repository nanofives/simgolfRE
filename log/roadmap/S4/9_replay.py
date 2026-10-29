"""S4.9's replay front end (a copy of 9_replay.py: golden file, mirror, rows and the fallback-owner gate are S4.9's) over S4.2's port/test/sound_replay.py (which this step may not edit; the changes it needs are in
log/roadmap/S4/8_merge.md). Same commands as sound_replay (oracle / replay / negctl), per family:

    ~/bin/py -3.12 log/roadmap/S4/9_replay.py oracle --family ds [--keys k ...] [--update] [--out tsv]
    ~/bin/py -3.12 log/roadmap/S4/9_replay.py replay --family ds [--w32]
    ~/bin/py -3.12 log/roadmap/S4/9_replay.py negctl --family ds [--out tsv]

What it adds to sound_replay:
  * GOLDEN per module: port/test/golden_audio_ds.json (family ds) / golden_audio_dev.json (family dev).
  * The S4.9 mirror (port/build/S4_9_mirror) as the default DLL and Win32 runner.
  * FRAME: the port keeps the locals the original passes by address in [kSndFrameBase, +kSndFrameSize) (audio_dev.h). The
    oracle maps that range into the emulator before each vector, so the host's callbacks can write the out-parameters and
    the bundle carries it; the default replay maps it from the bundle.
  * FPCW: "audio_dev.fpcw <word>" before each port call: the emulator's control word at that moment (oracle; recorded in
    the golden entry as "fpcw"), the recorded word in the default replay.
  * DERIVED / CUSTOM keys of the step's emu-only rows (no registry key): a twin key's fixture and vectors (DERIVED), or a
    fixture this step defines in log/roadmap/S4/8_fixtures.js (evaluated into each key's VM context before its fixture runs).
"""
from __future__ import annotations

import argparse
import base64
import os
import pathlib
import struct
import sys

HERE0 = pathlib.Path(__file__).resolve().parent
ROOT = HERE0.parents[2] if HERE0.name == "S4" else HERE0.parents[1]      # log/roadmap/S4/ or port/test/
HERE = pathlib.Path(__file__).resolve().parent
MIR = ROOT / "port" / "build" / "S4_9_mirror"
if not os.environ.get("SG_PORTSIM_DLL") and (MIR / "x64" / "Release" / "portsim_c.dll").exists():
    os.environ["SG_PORTSIM_DLL"] = str(MIR / "x64" / "Release" / "portsim_c.dll")
sys.path.insert(0, str(ROOT))
from port.test import sound_replay as R  # noqa: E402

E = R.E
R.MIRROR = MIR
FRAME_BASE, FRAME_SIZE = 0xFEB00000, 0x10000      # portsim/sim/audio_dev.h kSndFrameBase / kSndFrameSize
GOLDENS = {"wave": ROOT / "port" / "test" / "golden_audio_wave.json"}
# the step's fixtures next to this file: 8_fixtures.js here, sound_replay_s48_fixtures.js once moved to port/test (8_merge.md)
FIXTURES_JS = next((HERE / n for n in ("9_fixtures.js",) if (HERE / n).exists()),
                   HERE / "9_fixtures.js")

DERIVED8 = {}
CUSTOM8 = {}
EXCLUDED8 = {}
try:
    sys.path.insert(0, str(HERE))
    from emuonly9 import DERIVED8, CUSTOM8, EXCLUDED8  # noqa: E402,F811
except ImportError:
    pass


def family_keys(family: str) -> list[str]:
    rows = [r for r in R.step_rows("S4.9") if r["family"].rstrip("~") == "wave"]
    rvas = {int(r["rva"], 16) for r in rows}
    names = {r["name"] for r in rows}
    hooks, _ = E.registry()
    out = sorted(k for k, v in hooks.items() if (v.get("module") or "").lower() == "sound.dll" and v["addr"] in rvas
                 and k not in EXCLUDED8)
    for d, (twin, rva) in DERIVED8.items():
        if rva in rvas and d.split("@")[0] in names:
            out.append(d)
    for d, spec in CUSTOM8.items():
        if spec["addr"] in rvas and d not in out:
            out.append(d)
    return out


# ---- sound_replay hooks ------------------------------------------------------------------------------------------------
_spec_of = R.spec_of


def spec_of(key):
    if key in CUSTOM8:
        return dict(CUSTOM8[key])
    return _spec_of(key)


R.spec_of = spec_of
R.DERIVED.update(DERIVED8)
R.DERIVED.update({k: (None, s['addr']) for k, s in CUSTOM8.items()})   # no CSV: tier EMU_ONLY


class KeyRun8(E.KeyRun):
    """KeyRun with the step's custom specs and fixtures: the 8_fixtures.js source is evaluated into the key's VM context
    after the scripts load (setup), so its DIFF_FIXTURES entries exist when the fixture runs."""

    def __init__(self, vm, key, *a, **kw):
        if key in CUSTOM8:
            hooks, _ = E.registry()
            hooks[key] = dict(CUSTOM8[key])
        super().__init__(vm, key, *a, **kw)

    last: dict = {}     # (key, layout) -> the last KeyResult (the oracle reads the original's state regions from it)

    def run(self):
        res = super().run()
        KeyRun8.last[(self.key, self.layout)] = res
        return res

    def setup(self):
        m = super().setup()
        if FIXTURES_JS.exists():
            r = self.vm.request({"op": "eval", "src": (HERE / "8_fixtures.js").read_text() + "\n" + FIXTURES_JS.read_text()})
            if "error" in r:
                raise E.EmuError("fixture", "9_fixtures.js: " + r["error"])
        return m


R.E.KeyRun = KeyRun8


def ensure_frame(m):
    """Map the frame block into the emulator (zero pages, sent to the VM as dirty pages)."""
    for p in range(FRAME_BASE >> 12, (FRAME_BASE + FRAME_SIZE) >> 12):
        if p not in m.mapped:
            m._map(p << 12, R.PAGE)
            m.write(p << 12, bytes(R.PAGE))


# The top of the emulator's stack is part of the port's address space (the original's locals live there, below the arm's
# entry esp): only the pages below STACK_INIT - 0x8000 stay out (S4.2 kept the whole 1 MB out, U-1413).
R.STACK_PAGES = range(E.STACK_BASE >> 12, (E.STACK_INIT - 0x8000) >> 12)
PORT_ESP_GAP = 0x400       # host calls the port makes build their frames this far below the arm's entry esp


def entry_esp(spec, slots) -> int:
    """The esp the original's arm enters with (Machine.native_call at depth 0: the return address at ((STACK_INIT - 0x400)
    & ~0xF) - stack bytes - 4); register slots: thiscall ecx, fastcall ecx + edx."""
    nreg = {"thiscall": 1, "fastcall": 2}.get(spec.get("abi"), 0)
    return ((E.STACK_INIT - 0x400) & ~0xF) - 4 * max(0, len(slots) - nreg) - 4


class StateProxy:
    """The port state with "audio_dev.fpcw <cw>" and "audio_dev.esp <esp>" sent before the first port call that follows an
    audio_core.setup (the call's slots give the original's entry esp)."""

    def __init__(self, st, cw_fn, spec):
        self._st, self._cw, self._spec = st, cw_fn, spec
        self._armed = False

    def __getattr__(self, n):
        return getattr(self._st, n)

    def call(self, name, args):
        if self._armed and not name.startswith(("audio_core.", "audio_dev.")):
            self._armed = False
            self._st.call("audio_dev.fpcw", [self._cw()])
            self._st.call("audio_dev.esp", [entry_esp(self._spec, args)])
            try:
                return self._st.call(name, args)
            finally:
                # a function without locals leaves the entry esp pending: clear it so no later call (or a context
                # allocated at this one's address: audio_dev's table is keyed by the Ctx, U-1470) takes it
                self._st.call("audio_dev.esp", [0])
        r = self._st.call(name, args)
        if name == "audio_core.setup":
            self._armed = True
        return r


class LivePort8(R.LivePort):
    def __init__(self, lib, key, record):
        super().__init__(lib, key, record)
        self.st = StateProxy(self.st, self._fpcw, self.spec)
        self.space.st = self.st
        self.seen_cw = []
        self.esp = 0

    def _fpcw(self):
        cw = (self.run.m.uc.reg_read(E.UC_X86_REG_FPCW) & 0xFFFF) | 0x40   # bit 6 reads 1 on hardware; Unicorn reports it 0
        self.seen_cw.append(cw)
        return cw

    def __call__(self, run, args):
        m = run.m
        ensure_frame(m)
        self.run = run
        self.esp = entry_esp(self.spec, R.slots_of(args))
        # host calls of the port go below the port's frame, as the original's callees run below its locals
        saved_depth, saved_esp = m.depth, m.uc.reg_read(E.UC_X86_REG_ESP)
        m.depth += 1
        m.uc.reg_write(E.UC_X86_REG_ESP, self.esp - PORT_ESP_GAP)
        try:
            return super().__call__(run, args)
        finally:
            m.depth = saved_depth
            m.uc.reg_write(E.UC_X86_REG_ESP, saved_esp)


R.LivePort = LivePort8


def original_implogs(key):
    """The emulated original's import stand-in log per vector (layout A, the `$implog` region of arm 1's final state),
    decoded: [[import index, [values]], ...] per vector; None when the key has no `$implog` region."""
    res = KeyRun8.last.get((key, "A"))
    names = [s[0] for s in R.spec_of(key).get("state", [])]
    if res is None or isinstance(res, Exception) or "$implog" not in names:
        return None
    k = names.index("$implog")
    out = []
    for row in res.rows:
        if row.error or not row.state_hex:
            out.append(None)
            continue
        recs, lost = decode_implog(bytes.fromhex(row.state_hex.split("|")[k]))
        out.append({"records": [[i, v] for i, v in recs], "lost": lost})
    return out


import quirk9 as Q  # noqa: E402
try:
    from emuonly9 import QUIRK_KEYS  # noqa: E402
except ImportError:
    QUIRK_KEYS = ()
OWN_STEPS = ("S4.2", "S4.4", "S4.5", "S4.6", "S4.8", "S4.9")     # ACCEPTED or this step: a fallback to one of their functions fails
_OWNERS = None


def fallback_owners(recs):
    """layout A's fallback calls of a key: {owner step: calls} from each vector's report (rvas, decimal, comma separated)"""
    global _OWNERS
    if _OWNERS is None:
        _OWNERS = {int(r["rva"], 16): r["step"] for r in R.step_rows_all()}
    c = {}
    for r in recs or []:
        for x in (r.report.get("fallback_rvas") or "").split(","):
            if x:
                owner = _OWNERS.get(int(x), "unlisted")
                c[owner] = c.get(owner, 0) + 1
    return c


FB_RVAS = {}


def oracle(keys, layouts, update, out, family):
    R.GOLDEN = GOLDENS[family]
    lib = R.load_lib()
    golden = R.load_golden()
    rows = []
    cws = {}
    orig_init = LivePort8.__init__

    def init(self, lib_, key, record):
        orig_init(self, lib_, key, record)
        cws.setdefault(key, []).append(self)
    LivePort8.__init__ = init
    try:
        with E.FixVM() as vm:
            for n, key in enumerate(keys, 1):
                v, recs, regions, forgiven = Q.oracle_key_q(R, vm, lib, key, tuple(layouts), True, QUIRK_KEYS)
                fb = fallback_owners(recs)
                own_fb = sum(n for s, n in fb.items() if s in OWN_STEPS)
                for r_ in recs or []:
                    for x in (r_.report.get('fallback_rvas') or '').split(','):
                        if x:
                            FB_RVAS[int(x)] = FB_RVAS.get(int(x), 0) + 1
                ok = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and v.fallbacks == 0
                      and v.tier in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"))
                okown = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and own_fb == 0
                         and v.tier in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"))
                okf = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1)
                       and v.tier in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"))
                seen = sorted({c for lp in cws.get(key, []) for c in lp.seen_cw})
                verdict = "GREEN" if ok else ("GREEN_FB" if okown else "RED")
                print(f"[{n}/{len(keys)}] {key} {verdict} match {v.match}/{v.vectors} bc {v.match_bc} err {v.errors} "
                      f"{("quirk-forgiven " + str(forgiven) + " ") if forgiven else ""}fallback {v.fallbacks} {fb} tier {v.tier} cw {','.join(hex(c) for c in seen)} {v.first}", flush=True)
                rows.append((v, seen))
                if update and recs is not None and okown:
                    spec = R.spec_of(key)
                    style = E.csv_double_style(spec, key) if key not in R.DERIVED and key not in CUSTOM8 else "shortest"
                    R.save_bundle(key, "A", recs, style, E.LAYOUTS["A"]["base"])
                    golden[key] = {"name": v.name, "vectors": v.vectors, "digests": v.golden, "tier": v.tier,
                                   "regions": regions, "fallbacks": v.fallbacks, "fallback_owners": fb,
                                   "quirk": {str(i): n for i, n in forgiven.items()}}
                    il = original_implogs(key)
                    if il is not None:
                        golden[key]["implog"] = il
                    if len(seen) == 1 and seen[0] != 0x027F:
                        golden[key]["fpcw"] = seen[0]
                    elif len(seen) > 1:
                        golden[key]["fpcw"] = seen[0]
                        print(f"   note: {key} saw control words {seen}", flush=True)
    finally:
        LivePort8.__init__ = orig_init
    if update:
        import json
        R.GOLDEN.write_text(json.dumps(golden, indent=1, sort_keys=True) + "\n")
    if out:
        import csv
        with open(out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["key", "name", "vectors", "match", "match_bc", "errors", "fallbacks", "tier", "cw", "first"])
            for v, seen in rows:
                w.writerow([v.key, v.name, v.vectors, v.match, v.match_bc, v.errors, v.fallbacks, v.tier,
                            ",".join(hex(c) for c in seen), v.first])
    bad = [v.key for v, _ in rows if not (v.match == v.vectors)]
    fb = sum(v.fallbacks for v, _ in rows)
    print(f"keys {len(rows)}  green {len(rows) - len(bad)}  red {len(bad)}  fallbacks {fb}  {' '.join(bad)}")
    print("fallback callees: " + ", ".join(f"0x{r:x}x{n}" for r, n in sorted(FB_RVAS.items())))
    return 1 if bad else 0


def replay_key(lib, key, golden, **kw):
    cw = golden.get(key, {}).get("fpcw", 0x027F)
    orig_state = lib.state

    def state():
        return StateProxy(orig_state(), lambda: cw, R.spec_of(key))
    lib.state = state
    try:
        return R.replay_key(lib, key, golden, **kw)
    finally:
        lib.state = orig_state


def replay(keys, w32, family, prefix=""):
    R.GOLDEN = GOLDENS[family]
    lib = R.load_w32() if w32 else R.load_lib()
    golden = R.load_golden()
    bad, fbs = 0, 0
    for n, key in enumerate(keys, 1):
        r = replay_key(lib, key, golden, prefix=prefix)
        ok = r.vectors and r.passed == r.vectors
        fbs += r.fallbacks
        bad += 0 if ok else 1
        print(f"[{n}/{len(keys)}] {key} {'PASS' if ok else 'FAIL'} {r.passed}/{r.vectors} fallback {r.fallbacks} {r.first}",
              flush=True)
    print(f"keys {len(keys)}  fail {bad}  fallbacks {fbs}")
    return 1 if bad else 0


# ---- the AudioSink command log against the fixtures' import logs (S4.8 gate, second part) ------------------------------
def sound_imports() -> list[tuple[int, str]]:
    """(IAT slot rva, 'DLL!name') of every import of the user's sound.dll."""
    import pefile
    pe = pefile.PE(str(E.SOUND_DLL))
    ib = pe.OPTIONAL_HEADER.ImageBase
    out = [(i.address - ib, f"{d.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}")
           for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports]
    pe.close()
    return out


def replay_vectors(lib, key, golden):
    """The default replay of one key, vector by vector: yields (index, script entries, final port pages, ok)."""
    b = R.load_bundle(key)
    if b is None:
        return
    cw = golden.get(key, {}).get("fpcw", 0x027F)
    spec = R.spec_of(key)
    st = StateProxy(lib.state(), lambda: cw, spec)
    space = R.PortSpace(st)
    space.ro = (b["base"], b["base"] + R.data_rva())
    pages = {}
    try:
        for i, v in enumerate(b["vectors"]):
            for p in v["gone"]:
                pages.pop(p, None)
            for p, b64 in v["delta"].items():
                pages[int(p)] = base64.b64decode(b64)
            st.unmap_all()
            st.set_data_base(R.pick_data_base(pages))
            space.load(pages)
            st.files_put("$audio/claims", b"".join(struct.pack("<I", c) for c in v["claims"]))
            st.files_put("$audio/script", R.script_blob(v["script"]))
            st.call("audio_core.setup", [b["base"], R.SETUP_CLAIMS | R.SETUP_SCRIPT | R.SETUP_RESET])
            ok = True
            try:
                st.call(R.port_name(key), v["args"])
            except R.L.SgError:
                ok = False
            yield i, v["script"], space.pages(), b["base"], ok, pages
            st.call("audio_core.detach", [])
    finally:
        st.close()


def decode_implog(raw: bytes):
    """importstandin.js LOG: +0 records, +4 byte cursor, +8 calls not recorded, +0x10 per-import counters, +0x90 records
    {u32 import index, u32 value count, u32 values...}."""
    n, cur, lost = struct.unpack_from("<III", raw, 0)
    recs, off = [], 0x90
    for _ in range(n):
        idx, cnt = struct.unpack_from("<II", raw, off)
        recs.append((idx, list(struct.unpack_from(f"<{cnt}I", raw, off + 8))))
        off += 8 + 4 * cnt
    return recs, lost


def _subsequence(vals, args) -> bool:
    it = iter(args)
    return all(any(v == x for x in it) for v in vals)


def _records_match(args, vals, nm="") -> bool:
    """A stand-in records all slots, the N leading ones (`record: N`), an index list (pointer slots dropped: the values are a subsequence
    of the slots) or, for the mmio imports, the transforms of mmiostandin.js (and c3dh's memory-open record)."""
    n = len(vals)
    if args[:n] == vals or (n > 0 and args[len(args) - n:] == vals) or _subsequence(vals, args):
        return True
    if nm.endswith("mmioOpenA") and len(args) == 3:
        if n == 3 and vals[2] == args[2] and vals[1] == (1 if args[1] else 0):
            return True            # mmiostandin.js: crc32(name) or 0, info != NULL, flags
        if n == 20 and (vals[0], vals[1]) == (args[0], args[2]):
            return True            # c3dh's memory-open stand-in: name, flags, then the 18 MMIOINFO dwords
    if nm.endswith("mmioDescend") and len(args) == 4:
        return n == 3 and vals[0] == args[0] and vals[1] == (1 if args[2] else 0) and vals[2] == args[3]
    if nm.endswith("mmioAdvance") and len(args) == 3:
        return n == 3 and vals[0] == args[0] and vals[1] == (1 if args[1] else 0) and vals[2] == args[2]
    if nm.endswith("mmioStringToFOURCCA") and len(args) == 2:
        return n == 2 and vals[1] == args[1]     # the stand-in records the four-character code of the string (a pointer here), then the flags
    return False


def sinklog(keys, family):
    """For every vector of every key whose fixture stands imports in (a `$implog` state region):
      * the log the port's replay leaves equals the EMULATED ORIGINAL's (decoded by the oracle into the golden file,
        "implog"), record for record;
      * every port call whose target is the value of one of sound.dll's IAT slots is an AudioSink import command (with no
        host attached audio_core's snd_import issues DLL!name + the slots); each such call to a stood-in import appends
        exactly ONE log record, for the same import as every other record of that index, whose values are the command's
        recorded slots. A record is attributed to the host call whose memory writes appended it (the script entry), so
        records written by original code the host ran (a fallback or claimed callee: another step's function) are counted
        apart, not as port commands. Imports the VM serves with its own bodies (trap range) are counted, not compared."""
    import collections
    R.GOLDEN = GOLDENS[family]
    lib = R.load_lib()
    if os.environ.get('SG_SINK_NEGCTL'):
        _fn, _md = os.environ['SG_SINK_NEGCTL'].rsplit(':', 1)
        lib.negctl(_fn, int(_md))
    golden = R.load_golden()
    imps = sound_imports()
    rows, bad = [], 0
    cmd_names = collections.Counter()
    host_recs = 0
    for n, key in enumerate(keys, 1):
        spec = R.spec_of(key)
        names = [s[0] for s in spec.get("state", [])]
        regions = golden.get(key, {}).get("regions", [])
        logs = [tuple(regions[i]) for i, nm in enumerate(names) if nm in ("$implog",) and i < len(regions)]
        vec_ok = vec_n = cmds_all = hrec = 0
        first = ""
        for i, script, final, base, ok, start in replay_vectors(lib, key, golden):
            slot_val = {}
            for srva, nm in imps:
                p = (base + srva) >> 12
                if p in start:
                    off = (base + srva) & 0xFFF
                    slot_val.setdefault(struct.unpack_from("<I", start[p], off)[0], []).append(nm)
            cmds = [(slot_val[e["va"]][0], e["args"], e["va"]) for e in script if e["kind"] == 0 and e["va"] in slot_val]
            for c in cmds:
                cmd_names[c[0]] += 1
            cmds_all += len(cmds)
            if not logs:
                continue
            vec_n += 1
            a, sz = logs[0]
            pg = lambda pages: b"".join(pages.get(p, bytes(R.PAGE)) for p in range(a >> 12, ((a + sz - 1) >> 12) + 1))
            raw = pg(final)[a & 0xFFF:(a & 0xFFF) + sz]
            recs_port, lost = decode_implog(raw)
            orig = (golden.get(key, {}).get("implog") or [None] * (i + 1))[i]
            good = ok and orig is not None and orig["lost"] == lost and                 [(r[0], r[1]) for r in orig["records"]] == recs_port
            # the log region entry by entry: start bytes, then each script entry's writes into it
            cur = bytearray(pg(start)[a & 0xFFF:(a & 0xFFF) + sz])
            idx_name, why = {}, ""
            for e in script:
                dec0 = decode_implog(bytes(cur))
                before, lost_b = len(dec0[0]), dec0[1]
                for va, hx in e["writes"]:
                    b = bytes.fromhex(hx)
                    lo, hi = max(va, a), min(va + len(b), a + sz)
                    if lo < hi:
                        cur[lo - a:hi - a] = b[lo - va:hi - va]
                dec1 = decode_implog(bytes(cur))
                new, lost_a = dec1[0][before:], dec1[1]
                is_cmd = e["kind"] == 0 and e["va"] in slot_val and not (E.TRAP_LO <= e["va"] < E.TRAP_HI)
                if not is_cmd:
                    hrec += len(new)
                    continue
                nm = slot_val[e["va"]][0]
                if len(new) == 0 and lost_a > lost_b:
                    continue            # the log is full: the call is counted in the header, not recorded
                if len(new) != 1:
                    why = why or f"{nm}: {len(new)} records for one command"
                    continue
                idx, vals = new[0]
                if idx_name.setdefault(idx, nm) != nm or not _records_match(e["args"], vals, nm):
                    why = why or f"{nm} {e['args']} vs record {new[0]}"
            good = good and not why
            if good:
                vec_ok += 1
            elif not first:
                first = (f"v{i}: {why or 'log differs from the original'}; port records {recs_port[:3]} original "
                         f"{(orig or {}).get('records', [])[:3]}")[:300]
        host_recs += hrec
        verdict = "NOLOG" if not logs else ("EQUAL" if vec_ok == vec_n else "DIFF")
        bad += verdict == "DIFF"
        rows.append((key, verdict, vec_ok, vec_n, cmds_all, first))
        print(f"[{n}/{len(keys)}] {key} {verdict} {vec_ok}/{vec_n} vectors, {cmds_all} import commands, {hrec} records "
              f"from host-run code {first}", flush=True)
    print("import commands by name: " + ", ".join(f"{k} {v}" for k, v in sorted(cmd_names.items())))
    eq = sum(1 for r in rows if r[1] == "EQUAL")
    print(f"keys {len(rows)}  with an import log {sum(1 for r in rows if r[1] != 'NOLOG')}  equal {eq}  diff {bad}  "
          f"import commands {sum(cmd_names.values())}  records from host-run code {host_recs}")
    return 1 if bad else 0


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["oracle", "replay", "negctl", "keys", "sinklog"])
    ap.add_argument("--family", choices=["wave"], required=True)
    ap.add_argument("--keys", nargs="*")
    ap.add_argument("--layouts", default="ABC")
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--out")
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--mode", type=int, default=1)
    a = ap.parse_args(argv)
    keys = a.keys or family_keys(a.family)
    if a.cmd == "keys":
        print("\n".join(keys))
        return 0
    if a.cmd == "oracle":
        return oracle(keys, a.layouts, a.update, a.out, a.family)
    if a.cmd == "replay":
        return replay(keys, a.w32, a.family)
    if a.cmd == "sinklog":
        return sinklog(keys, a.family)
    R.GOLDEN = GOLDENS[a.family]
    return R.negctl_main(keys, a.mode, a.out)


if __name__ == "__main__":
    sys.exit(main())
