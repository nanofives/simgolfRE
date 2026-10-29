"""S4.7 runner (a copy of S4.6's 7_run.py): port/test/sound_replay.py (S4.2's, not edited) for the rows of step S4.7, with this step's own
mirror build (port/build/S4_7_mirror), golden file (port/test/golden_audio_sound.json) and the x87 control word of every vector recorded
in the bundle and applied by the replay (port/CORE.md section 7).

    ~/bin/py -3.12 log/roadmap/S4/7_run.py oracle [--keys k ...] [--update] [--out tsv]      # live oracle, SG_EMU_ORACLE-style
    ~/bin/py -3.12 log/roadmap/S4/7_run.py negctl [--keys k ...] [--mode 1] [--out tsv]       # mode 1 of every function: RED
    ~/bin/py -3.12 log/roadmap/S4/7_run.py replay [--keys k ...] [--w32]                      # from the bundles, x64 or Win32
    ~/bin/py -3.12 log/roadmap/S4/7_run.py x87ctl [--out tsv]                                 # mode 2 (wrong x87 port) on Win32

Vectors whose fixture ran the original under an x87 control word other than 0x027f (the c3fh caller-word keys at 0x0f7f) are
judged on Win32 only: the x64 library computes with SSE2 doubles (SG_E_UNSUPPORTED for another word), so the oracle records
their bundle from the emulated original and reports the x64 mismatches (the vectors whose result depends on the word) as
X87_WIN32; the Win32 replay runs the port under the recorded word and must equal the golden digest.
"""
from __future__ import annotations

import argparse
import base64
import collections
import csv
import json
import os
import pathlib
import sys
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
for p in (ROOT, ROOT / "port" / "test", ROOT / "re" / "tools"):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))
from port.test import sound_replay as SR  # noqa: E402

STEP = "S4.7"
MIR = ROOT / "port" / "build" / "S4_7_mirror"
GOLDEN = ROOT / "port" / "test" / "golden_audio_sound.json"
DEFAULT_CW = 0x027F
DLL64 = MIR / "x64" / "Release" / "portsim_c.dll"
# S4.7 has no emu-only row: every one of the 80 rows has a registry key
TIERS = ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY")


SYNTH = {}


class Live6(SR.LivePort):
    """records the emulator's x87 control word at each vector's call"""

    def __call__(self, run, args):
        from unicorn.x86_const import UC_X86_REG_FPCW
        word = run.m.uc.reg_read(UC_X86_REG_FPCW)
        n = len(self.records)
        try:
            return super().__call__(run, args)
        finally:
            if len(self.records) > n:
                self.records[-1].fpcw = norm_cw(word)


SR.LivePort = Live6
_orig_save_bundle = SR.save_bundle


def save_bundle(key, lay, records, style, base):
    _orig_save_bundle(key, lay, records, style, base)
    p = SR.BUNDLE_DIR / f"{SR._safe(key)}.json.z"
    b = json.loads(zlib.decompress(p.read_bytes()))
    for v, r in zip(b["vectors"], records):
        v["fpcw"] = getattr(r, "fpcw", DEFAULT_CW)
    p.write_bytes(zlib.compress(json.dumps(b).encode(), 6))


SR.save_bundle = save_bundle


def norm_cw(word):
    """Unicorn reads the default word as 0x023f; the x64 library accepts the 0x027f-equivalent words (53-bit, nearest)"""
    return DEFAULT_CW if (word & 0x0F3F) == 0x023F else word


def load_lib6():
    """the S4.7 mirror's x64 library (SR.load_lib reads SR.MIRROR, which stays S4.2's)"""
    lib = SR.L.load(DLL64, rebuild=False)
    if not hasattr(lib, "_snd"):
        f = lib.dll.sg_snd_set_host
        f.restype = SR.C.c_int
        f.argtypes = [SR.C.c_void_p, SR.HOSTFN, SR.C.c_void_p]
        lib._snd = True
    return lib


def load_w32_6():
    """Lib32 over the S4.7 mirror's Win32 runner and DLL"""
    from port.test import lib32 as L32
    exe = MIR / "w32" / "Release" / "portsim_runner32.exe"
    dll = MIR / "w32" / "Release" / "portsim_c.dll"
    obj = L32.Lib32.__new__(L32.Lib32)
    obj.path = exe
    obj.runner = L32.Runner(exe, dll)
    _, r = obj.runner.tx(L32.OP_HELLO)
    obj.state_size = r.u32()
    obj._info = r.s()
    obj.pointer_size = r.u32()
    obj._objects, obj._funcs, obj.fpcw, obj.dll = {}, None, L32.DEFAULT_CW, None
    return obj


def load_golden6():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


def keys_for_step(only=None):
    keys = SR.keys_for(STEP)
    return [k for k in keys if (not only or k in only)]


# ---- oracle ----------------------------------------------------------------------------------------------------------
# ---- the emulator's signaling-NaN quirk (U-1451) -----------------------------------------------------------------------------
# Unicorn's fld / fst of a signaling NaN keeps it signaling; the CPU (and the live C3 A/B: log/c3/c3fe_notes.txt, the control
# that copies the bits raw reads RED) sets the quiet bit. The port follows the CPU. For the keys whose original copies doubles
# or floats through the x87 (QUIRK_KEYS), a difference that is exactly the quiet bit of a NaN pattern (port set, emulator clear)
# is forgiven; every other difference still fails. The golden digest of a forgiven vector pins the port's regions.
QUIRK_KEYS = ()


def _quirk_bytes_ok(bo: bytes, bp: bytes):
    """True when every differing byte is the quiet bit of a NaN (double: bit 51 of a qword, float: bit 22 of a dword) that the
    port set and the emulator left clear; returns (ok, number of forgiven NaNs)"""
    if len(bo) != len(bp):
        return False, 0
    diffs = [k for k in range(len(bo)) if bo[k] != bp[k]]
    done, n = set(), 0
    for k in diffs:
        if k in done:
            continue
        ok = False
        # double: the byte at qword offset 6 (bits 48..55) holds the quiet bit 0x08 under the exponent's low nibble
        if k + 1 < len(bo) and k >= 6:
            o, q = bo[k - 6:k + 2], bp[k - 6:k + 2]
            if (o[7] & 0x7F) == 0x7F and (o[6] & 0xF0) == 0xF0 and not (o[6] & 0x08) and q[6] == (o[6] | 0x08)                     and o[:6] == q[:6] and o[7] == q[7] and (o[6] & 0x07 or any(o[:6])):
                ok = True
                done.add(k)
        if not ok and k + 1 < len(bo) and k >= 2:
            o, q = bo[k - 2:k + 2], bp[k - 2:k + 2]
            if (o[3] & 0x7F) == 0x7F and (o[2] & 0x80) and not (o[2] & 0x40) and q[2] == (o[2] | 0x40)                     and o[:2] == q[:2] and o[3] == q[3] and (o[2] & 0x3F or any(o[:2])):
                ok = True
                done.add(k)
        if not ok:
            return False, 0
        n += 1
    return True, n


def quirk_equal(orig_hex, port_hex):
    if orig_hex is None or port_hex is None:
        return False, 0
    ro, rp = orig_hex.split("|"), port_hex.split("|")
    if len(ro) != len(rp):
        return False, 0
    total = 0
    for a, b in zip(ro, rp):
        if a == b:
            continue
        ok, n = _quirk_bytes_ok(bytes.fromhex(a), bytes.fromhex(b))
        if not ok:
            return False, 0
        total += n
    return True, total


def oracle_key6(vm, lib, key, layouts=("A", "B", "C"), record=True):
    """SR.oracle_key with the quirk filter: (KeyVerdict, layout A records, regions, forgiven {vector: NaNs})"""
    hooks, batch = SR.E.registry()
    spec = SR.spec_of(key)
    derived = key in SR.DERIVED
    if derived:
        hooks[key] = spec
    quirk = SR.port_name(key) in QUIRK_KEYS
    res, ports = {}, {}
    for lay in layouts:
        lp = SR.LivePort(lib, key, record and lay == "A")
        try:
            res[lay] = SR.E.KeyRun(vm, key, lay, second_arm=lp).run()
        except SR.E.EmuError as e:
            res[lay] = e
        finally:
            lp.close()
        ports[lay] = lp
    a = res["A"]
    first, match, match_bc, errs = "", 0, 0, 0
    golden, pdig, forgiven = [], [], {}

    def row_ok(row):
        if row.match:
            return True, 0
        if quirk and not row.error and row.orig == row.re:
            ok, n = quirk_equal(row.state_hex, row.state_re_hex)
            if ok:
                return True, n
        return False, 0

    if isinstance(a, Exception) or a.fixture_error:
        return SR.KeyVerdict(key, SR.port_name(key), len(spec["vectors"]), 0, 0, len(spec["vectors"]), 0, "SETUP_ERR",
                             str(a if isinstance(a, Exception) else a.fixture_error)[:200], [], []), None, [], {}
    for i, row in enumerate(a.rows):
        ok, n = row_ok(row)
        if n:
            forgiven[i] = n
        golden.append(SR.digest(row.orig, row.state_re_hex if n else row.state_hex))
        pdig.append(SR.digest(row.re, row.state_re_hex))
        if row.error:
            errs += 1
            if not first:
                first = f"v{i} A: {row.error[:160]}"
        elif ok:
            match += 1
        elif not first:
            first = f"v{i} A: ret emu {row.orig} port {row.re}" + ("" if row.state_hex == row.state_re_hex else " (regions differ)")
    for lay in layouts[1:]:
        rb = res[lay]
        if isinstance(rb, Exception):
            first = first or f"{lay}: {rb}"
            continue
        for i, row in enumerate(rb.rows):
            if row_ok(row)[0]:
                match_bc += 1
            elif not first:
                first = f"v{i} {lay}: " + (row.error[:160] if row.error else f"ret emu {row.orig} port {row.re}")
    fb = sum(int(r.report.get("fallback", 0) or 0) for lp in ports.values() for r in lp.records)
    if derived or key in SYNTH:
        tier = "EMU_ONLY"
    elif not {"B", "C"} <= set(res):
        tier = "A_ONLY"
    else:
        import sound_census as SC
        tier = SC.compare(key, spec, batch.get(key), res, SR.E.read_csv(spec, key))["row"]["tier"]
    v = SR.KeyVerdict(key, SR.port_name(key), len(a.rows), match, match_bc, errs, fb, tier, first, golden, pdig)
    return v, (ports["A"].records if record else None), [list(x) for x in a.regions], forgiven


OWN_STEPS = ("S4.2", "S4.4", "S4.5", "S4.6", "S4.7", "S4.8")
FB_TOTAL = collections.Counter()
FB_RVAS = collections.Counter()
QUIRK_TOTAL = collections.Counter()
_OWNERS = None


def fallback_owners(recs):
    """layout A's fallback calls of a key: {owner step: calls} from each vector's report (rvas, decimal, comma separated);
    an rva no row of re/roadmap/S4_functions.tsv names is owner `unlisted`"""
    global _OWNERS
    if _OWNERS is None:
        _OWNERS = {int(r["rva"], 16): r["step"] for r in SR.step_rows_all()}
    c = collections.Counter()
    for r in recs or []:
        for x in (r.report.get("fallback_rvas") or "").split(","):
            if x:
                owner = _OWNERS.get(int(x), "unlisted")
                c[owner] += 1
                FB_RVAS[(owner, int(x))] += 1
    return dict(c)


# S4.1's fake WINMM has no timeGetTime (`'winmm.dll': {}` in re/tools/fixvm/syslibs.js), so the vectors of getPlaybackTime that reach the
# clock stop in the emulator (S4.1 tier PARTIAL_ERR, U-1460 here). The shared file is not edited: the keys in CLOCK_KEYS run on a copy of
# the VM whose WINMM answers timeGetTime with a constant (the C3 fixture picks every vector's anchor and limit so that the result does
# not depend on the clock: re/frida/registry.d/c3cl.py header). Both arms see the same constant.
CLOCK_KEYS = ("getPlaybackTime",)
CLOCK_VALUE = 0x1234


def clock_fixvm():
    import shutil
    src = ROOT / "re" / "tools" / "fixvm"
    dst = MIR / "fixvm_clock"
    dst.mkdir(parents=True, exist_ok=True)
    for f in ("fixvm.js", "frida.js", "mem.js", "syslibs.js"):
        shutil.copy2(src / f, dst / f)
    s = (dst / "syslibs.js").read_text(encoding="utf-8")
    old_ = "'winmm.dll': {},"
    assert s.count(old_) == 1
    (dst / "syslibs.js").write_text(s.replace(old_, "'winmm.dll': { timeGetTime: [0, () => %d] }," % CLOCK_VALUE), encoding="utf-8")
    keep = SR.E.FIXVM_JS
    SR.E.FIXVM_JS = dst / "fixvm.js"
    try:
        return SR.E.FixVM()
    finally:
        SR.E.FIXVM_JS = keep


_CLOCK_VM = []


def close_clock_vm():
    for v_ in _CLOCK_VM:
        v_.close()
    _CLOCK_VM.clear()


def vm_for_key(vm, key):
    """the plain VM, or the clock VM for the keys that read timeGetTime"""
    if SR.port_name(key) not in CLOCK_KEYS:
        return vm
    if not _CLOCK_VM:
        _CLOCK_VM.append(clock_fixvm())
    return _CLOCK_VM[0]


def oracle(keys, update, layouts="ABC", out=None):
    lib = load_lib6()
    golden = load_golden6()
    rows = []
    with SR.E.FixVM() as vm:
        for n, key in enumerate(keys, 1):
            v, recs, regions, forgiven = oracle_key6(vm_for_key(vm, key), lib, key, tuple(layouts), record=True)
            words = sorted({getattr(r, "fpcw", DEFAULT_CW) for r in (recs or [])})
            x87 = any(w != DEFAULT_CW for w in words)
            fb = fallback_owners(recs)          # layout A: fallback calls by the step that owns the callee
            own_fb = sum(n for s, n in fb.items() if s in OWN_STEPS)
            ok = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and own_fb == 0
                  and v.tier in TIERS)
            judged = ok or (x87 and v.errors == 0 and own_fb == 0 and v.tier in TIERS and v.vectors > 0)
            status = "GREEN" if ok else ("X87_WIN32" if judged else "RED")
            print(f"[{n}/{len(keys)}] {key} {status} match {v.match}/{v.vectors} bc {v.match_bc} err {v.errors} "
                  f"fallback(A) {fb} tier {v.tier} words {[hex(w) for w in words]} "
                  f"{('quirk-forgiven ' + str(forgiven)) if forgiven else ''} {v.first}", flush=True)
            rows.append((v, status, words, fb))
            FB_TOTAL.update(fb)
            QUIRK_TOTAL.update({key: len(forgiven)} if forgiven else {})
            if update and recs is not None and judged:
                spec = SR.spec_of(key)
                style = SR.E.csv_double_style(spec, key) if (key not in SR.DERIVED and key not in SYNTH) else "shortest"
                SR.save_bundle(key, "A", recs, style, SR.E.LAYOUTS["A"]["base"])
                golden[key] = {"name": v.name, "vectors": v.vectors, "digests": v.golden, "tier": v.tier,
                               "regions": regions, "fpcw": words, "quirk": {str(i): n for i, n in forgiven.items()}}
    close_clock_vm()
    if update:
        GOLDEN.write_text(json.dumps(golden, indent=1, sort_keys=True) + "\n")
    if out:
        with open(out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["key", "name", "vectors", "match", "match_bc", "errors", "fallback_calls_A_by_owner", "tier", "status",
                        "words", "first"])
            for v, status, words, fb in rows:
                w.writerow([v.key, v.name, v.vectors, v.match, v.match_bc, v.errors, json.dumps(fb, sort_keys=True), v.tier,
                            status, " ".join(hex(x) for x in words), v.first])
    bad = [r[0].key for r in rows if r[1] == "RED"]
    print(f"keys {len(rows)}  green {sum(1 for r in rows if r[1] == 'GREEN')}  x87_win32 "
          f"{sum(1 for r in rows if r[1] == 'X87_WIN32')}  red {len(bad)} {' '.join(bad)}")
    print(f"vectors forgiven for the sNaN quirk (U-1451): {dict(QUIRK_TOTAL)}")
    print("fallback callees: " + ", ".join(f"{o}:0x{r:x}x{n}" for (o, r), n in sorted(FB_RVAS.items())))
    print(f"fallback calls (layout A, development fallback of callees other steps own), by owner: {dict(FB_TOTAL)}")
    return 1 if bad else 0


# ---- replay from the bundles, x87 word applied -------------------------------------------------------------------------
def replay_key(lib, key, golden, prefix="", w32=False):
    b = SR.load_bundle(key)
    if b is None:
        return SR.ReplayResult(key, 0, 0, 0, "no bundle (run the oracle with --update)"), 0
    spec = SR.spec_of(key)
    name = prefix + SR.port_name(key)
    want = golden.get(key, {}).get("digests")
    if not want:
        return SR.ReplayResult(key, 0, 0, 0, "no golden digests"), 0
    regions_spec = golden[key]["regions"]
    pages = {}
    passed, fb, skipped, first = 0, 0, 0, ""
    st = lib.state()
    space = SR.PortSpace(st)
    space.ro = (b["base"], b["base"] + SR.data_rva())
    cw_now = DEFAULT_CW
    try:
        for i, v in enumerate(b["vectors"]):
            for p in v["gone"]:
                pages.pop(p, None)
            for p, b64 in v["delta"].items():
                pages[int(p)] = base64.b64decode(b64)
            word = v.get("fpcw", DEFAULT_CW)
            if word != cw_now:
                if lib.set_fpcw(word) != 0:
                    skipped += 1       # the x64 library cannot run another control word: judged on Win32
                    continue
                cw_now = word
            st.unmap_all()
            st.set_data_base(SR.pick_data_base(pages))
            space.load(pages)
            st.files_put("$audio/claims", b"".join(SR.struct.pack("<I", c) for c in v["claims"]))
            st.files_put("$audio/script", SR.script_blob(v["script"]))
            st.call("audio_core.setup", [b["base"], SR.SETUP_CLAIMS | SR.SETUP_SCRIPT | SR.SETUP_RESET])
            err = None
            try:
                r = st.call(name, v["args"])
                out = {"eax": r.eax, "edx": r.edx}
                if r.has_st0:
                    out["f"] = SR.E._json_float(r.st0)
                txt = SR.ret_text(spec, out, v["obj"], b["style"])
            except SR.L.SgError as e:
                err = str(e)
                txt = None
            st.call("audio_core.report", [])
            rep = SR.parse_report(st.files_read("$audio/out"))
            fb += int(rep.get("fallback", 0) or 0)
            regions = [tuple(x) for x in regions_spec]
            state_hex = "|".join(space.read(a, n).hex() for a, n in regions) if regions else None
            got = SR.digest(txt, state_hex) if err is None else None
            ok = got == want[i] and rep.get("script_pos") == rep.get("script_len")
            if v.get("error") and err is not None:
                ok = False
            if ok:
                passed += 1
            elif not first:
                first = (f"v{i} (cw {word:#x}): {err or 'digest ' + str(got) + ' != ' + want[i]}; script "
                         f"{rep.get('script_pos')}/{rep.get('script_len')}; {rep.get('detail', '')}")[:300]
            st.call("audio_core.detach", [])
    finally:
        if cw_now != DEFAULT_CW:
            lib.set_fpcw(DEFAULT_CW)
        st.close()
    return SR.ReplayResult(key, len(b["vectors"]) - skipped, passed, fb, first), skipped


def replay(keys, w32=False, prefix=""):
    lib = load_w32_6() if w32 else load_lib6()
    golden = load_golden6()
    bad = 0
    tot_skipped = 0
    tot_fb = 0
    n_skipkeys = 0
    for n, key in enumerate(keys, 1):
        r, skipped = replay_key(lib, key, golden, prefix, w32)
        tot_skipped += skipped
        ok = r.vectors and r.passed == r.vectors     # fallback calls (other steps' callees) are reported, not failed
        only_other_word = not r.vectors and skipped > 0
        bad += 0 if (ok or only_other_word) else 1
        n_skipkeys += 1 if only_other_word else 0
        tot_fb += r.fallbacks
        print(f"[{n}/{len(keys)}] {key} {'PASS' if ok else ('SKIP' if only_other_word else 'FAIL')} {r.passed}/{r.vectors} "
              f"skipped {skipped} fallback {r.fallbacks} {r.first}", flush=True)
    print(f"keys {len(keys)}  fail {bad}  vectors skipped (another x87 word, x64) {tot_skipped} in {n_skipkeys} keys (judged on Win32)  fallback calls {tot_fb}")
    return 1 if bad else 0


# ---- x87 controls on Win32 ---------------------------------------------------------------------------------------------
X87_FUNCTIONS = ["Sound5b6a8_ctor"]


def x87ctl(out=None):
    """mode 2 (plain double quotients, the classic wrong x87 port) of the c3fh rate functions: the Win32 replay of their
    keys must go RED at the 0x0f7f vectors the word changes, and stay GREEN at 0x027f"""
    lib = load_w32_6()
    golden = load_golden6()
    rows = []
    for fn in X87_FUNCTIONS:
        ks = [k for k in keys_for_step() if SR.port_name(k) == fn]
        for mode in (0, 2):
            lib.negctl(fn, mode)
            try:
                for k in ks:
                    r, _ = replay_key(lib, k, golden, "", True)
                    rows.append((fn, k, mode, r.vectors, r.passed))
                    print(f"{fn} {k} mode {mode}: {r.passed}/{r.vectors} pass {r.first}", flush=True)
            finally:
                lib.negctl(fn, 0)
    if out:
        with open(out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["function", "key", "mode", "vectors", "pass"])
            w.writerows(rows)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    o = sub.add_parser("oracle")
    o.add_argument("--keys", nargs="*")
    o.add_argument("--update", action="store_true")
    o.add_argument("--layouts", default="ABC")
    o.add_argument("--out")
    n = sub.add_parser("negctl")
    n.add_argument("--keys", nargs="*")
    n.add_argument("--mode", type=int, default=1)
    n.add_argument("--out")
    r = sub.add_parser("replay")
    r.add_argument("--keys", nargs="*")
    r.add_argument("--w32", action="store_true")
    x = sub.add_parser("x87ctl")
    x.add_argument("--out")
    a = ap.parse_args(argv)
    os.environ["SG_PORTSIM_DLL"] = str(DLL64)     # SR.negctl_main loads its library through SR.mirror_dll
    if a.cmd == "oracle":
        return oracle(keys_for_step(a.keys), a.update, a.layouts, a.out)
    if a.cmd == "negctl":
        return SR.negctl_main(keys_for_step(a.keys), a.mode, a.out)
    if a.cmd == "replay":
        return replay(keys_for_step(a.keys), a.w32)
    return x87ctl(a.out)


if __name__ == "__main__":
    sys.exit(main())
